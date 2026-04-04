/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.
*/

#include "support.h"
#include "M68k.h"
#include "RegisterAllocator.h"
#include "EmuFeatures.h"

uint32_t *EMIT_MULU(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_MUL_DIV")));
uint32_t *EMIT_MULS(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr) __attribute__((alias("EMIT_MUL_DIV")));

/* AArch64 has both multiply and divide. No need to have them in C form */
#ifndef __aarch64__
static uint32_t *EMIT_DivideByZeroException(uint32_t *ptr, uint8_t ext_words, uint16_t **m68k_ptr)
{
    ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

    RA_StoreDirtyFPURegs(&ptr);
    RA_StoreDirtyM68kRegs(&ptr);

    RA_StoreCC(&ptr);
    RA_StoreFPCR(&ptr);
    RA_StoreFPSR(&ptr);

#if EMU68_INSN_COUNTER
    extern uint32_t insn_count;
    uint8_t ctx = RA_GetCTX(&ptr);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
    if (insn_count != 0)
    {
        uint8_t adj = RA_AllocARMRegister(&ptr);
        *ptr++ = movw_immed_u16(adj, insn_count & 0xffff);
        if (insn_count >> 16)
            *ptr++ = movt_immed_u16(adj, insn_count >> 16);
        *ptr++ = add_reg(tmp, tmp, adj, 0);
        RA_FreeARMRegister(&ptr, adj);
    }
    *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

    RA_FreeARMRegister(&ptr, tmp);
#endif

    return ptr;
}

struct Result32 uidiv(uint32_t n, uint32_t d)
{
    struct Result32 res = { 0, 0 };

    if (n == 0)
        return res;

    for (int i = 31 - __builtin_clzl(n); i >= 0; --i)
    {
        res.r <<= 1;
        if (n & (1 << i)) res.r |= 1;
        if (res.r >= d) {
            res.r -= d;
            res.q |= 1 << i;
        }
    }

    return res;
}

struct Result32 sidiv(int32_t n, int32_t d)
{
    struct Result32 res = { 0, 0 };

    if (d < 0) {
        res = sidiv(n, -d);
        res.q = -res.q;
        return res;
    }

    if (n < 0) {
        res = sidiv(-n, d);
        if (res.r == 0) {
            res.q = -res.q;
        }
        else {
            res.q = -res.q - 1;
            res.r = d - res.r;
        }

        return res;
    }

    res = uidiv(n, d);

    return res;
}

struct Result64 uldiv(uint64_t n, uint64_t d)
{
    struct Result64 res = { 0, 0 };

    if (n == 0)
        return res;

    for (int i = 63 - __builtin_clzll(n); i >= 0; --i)
    {
        res.r <<= 1;
        if (n & (1 << i)) res.r |= 1;
        if (res.r >= d) {
            res.r -= d;
            res.q |= 1 << i;
        }
    }

    return res;
}

struct Result64 sldiv(int64_t n, int64_t d)
{
    struct Result64 res = { 0, 0 };

    if (d < 0) {
        res = sldiv(n, -d);
        res.q = -res.q;
        return res;
    }

    if (n < 0) {
        res = sldiv(-n, d);
        if (res.r == 0) {
            res.q = -res.q;
        }
        else {
            res.q = -res.q - 1;
            res.r = d - res.r;
        }

        return res;
    }

    res = uldiv(n, d);

    return res;
}

struct Result64Parts {
    uint32_t q_lo;
    uint32_t q_hi;
    uint32_t r_lo;
    uint32_t r_hi;
};

static struct Result64Parts uldiv_64_32(uint32_t n_lo, uint32_t n_hi, uint32_t d)
{
    struct Result64Parts res = { 0, 0, 0, 0 };
    uint64_t r = 0;

    for (int i = 63; i >= 0; --i)
    {
        r <<= 1;
        if (i >= 32)
            r |= (n_hi >> (i - 32)) & 1;
        else
            r |= (n_lo >> i) & 1;

        if (r >= d)
        {
            r -= d;
            if (i >= 32)
                res.q_hi |= 1u << (i - 32);
            else
                res.q_lo |= 1u << i;
        }
    }

    res.r_lo = (uint32_t)r;

    return res;
}

static struct Result64Parts sldiv_64_32(uint32_t n_lo, uint32_t n_hi, int32_t d)
{
    int neg_n = ((int32_t)n_hi) < 0;
    int neg_d = d < 0;
    uint32_t d_abs = neg_d ? (uint32_t)(-(uint32_t)d) : (uint32_t)d;
    uint32_t abs_lo = n_lo;
    uint32_t abs_hi = n_hi;

    if (neg_n)
    {
        abs_lo = ~abs_lo + 1;
        abs_hi = ~abs_hi + (abs_lo == 0);
    }

    struct Result64Parts res = uldiv_64_32(abs_lo, abs_hi, d_abs);

    if (neg_n ^ neg_d)
    {
        res.q_lo = ~res.q_lo + 1;
        res.q_hi = ~res.q_hi + (res.q_lo == 0);
    }

    if (neg_n)
    {
        res.r_lo = ~res.r_lo + 1;
        res.r_hi = ~res.r_hi + (res.r_lo == 0);
    }

    return res;
}
#endif

uint32_t *EMIT_MULS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
#ifdef __aarch64__
    *ptr++ = sxth(reg, reg);
    *ptr++ = sxth(src, src);

    *ptr++ = mul(reg, reg, src);
#else
    *ptr++ = sxth(reg, reg, 0);
    *ptr++ = sxth(src, src, 0);

    *ptr++ = muls(reg, reg, src);
#endif
    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
        *ptr++ = cmn_reg(31, reg, LSL, 0);
#endif
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
    }

    return ptr;
}

uint32_t *EMIT_MULU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg;
    uint8_t src = 0xff;
    uint8_t ext_words = 0;

    // Fetch 16-bit register: source and destination
    reg = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    // Fetch 16-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);

    // Sign-extend 16-bit multiplicants
#ifdef __aarch64__
    *ptr++ = uxth(reg, reg);
    *ptr++ = uxth(src, src);
    *ptr++ = mul(reg, reg, src);
#else
    *ptr++ = uxth(reg, reg, 0);
    *ptr++ = uxth(src, src, 0);

    *ptr++ = muls(reg, reg, src);
#endif
    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
#ifdef __aarch64__
        *ptr++ = cmn_reg(31, reg, LSL, 0);
#endif
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
    }

    return ptr;
}

uint32_t *EMIT_MULS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_dl;
    uint8_t reg_dh = 0xff;
    uint8_t src = 0xff;
    uint8_t ext_words = 1;
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);

    // Fetch 32-bit register: source and destination
    reg_dl = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);

    // Fetch 32-bit multiplicant
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &src, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);

    if (opcode2 & (1 << 10))
    {
        reg_dh = RA_MapM68kRegisterForWrite(&ptr, (opcode2 & 7));
    }
    else
    {
        reg_dh = RA_AllocARMRegister(&ptr);
    }

#ifdef __aarch64__
    if (opcode2 & (1 << 11))
        *ptr++ = smull(reg_dl, reg_dl, src);
    else
        *ptr++ = umull(reg_dl, reg_dl, src);
    if (opcode2 & (1 << 10) && (reg_dh != reg_dl))
    {
        *ptr++ = add64_reg(reg_dh, 31, reg_dl, LSR, 32);
    }
#else
    if (opcode2 & (1 << 11))
        *ptr++ = smulls(reg_dh, reg_dl, reg_dl, src);
    else
        *ptr++ = umulls(reg_dh, reg_dl, reg_dl, src);
#endif

    RA_FreeARMRegister(&ptr, src);

    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

#ifdef __aarch64__
        if (opcode2 & (1 << 10)) { 
            *ptr++ = cmn64_reg(31, reg_dl, LSL, 0);
        }
        else {
            *ptr++ = cmn_reg(31, reg_dl, LSL, 0);
        }
#endif
        uint8_t old_mask = update_mask & SR_V;
        ptr = EMIT_GetNZ00(ptr, cc, &update_mask);
        update_mask |= old_mask;

        if (update_mask & SR_Z) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
        if (update_mask & SR_N) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
        }
        if ((update_mask & SR_V) && 0 == (opcode2 & (1 << 10))) {
            ptr = EMIT_ClearFlags(ptr, cc, SR_V);
#ifdef __aarch64__
            uint8_t tmp = RA_AllocARMRegister(&ptr);
            /* If signed multiply check higher 32bit against 0 or -1. For unsigned multiply upper 32 bit must be zero */
            if (opcode2 & (1 << 11)) {
                *ptr++ = cmn_reg(reg_dl, 31, LSL, 0);
                *ptr++ = csetm(tmp, A64_CC_MI);
            } else {
                *ptr++ = mov_immed_u16(tmp, 0, 0);
            }
            *ptr++ = cmp64_reg(tmp, reg_dl, LSR, 32);
            RA_FreeARMRegister(&ptr, tmp);
#else
            if (opcode2 & (1 << 11))
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);
                /*
                    32-bit result was requested. Check if top 32 are sign-extension of lower 32bit variable.
                    If this is not the case, set V bit
                */
                *ptr++ = cmp_immed(reg_dl, 0);
                *ptr++ = mvn_cc_immed_u8(ARM_CC_MI, tmp, 0);
                *ptr++ = mov_cc_immed_u8(ARM_CC_PL, tmp, 0);
                *ptr++ = subs_reg(tmp, reg_dh, tmp, 0);
                RA_FreeARMRegister(&ptr, tmp);
            }
            else
            {
                *ptr++ = cmp_immed(reg_dh, 0);
            }
#endif
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }
    }

    RA_FreeARMRegister(&ptr, reg_dh);

    return ptr;
}

uint32_t *EMIT_DIVS_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = 0xff;
    uint8_t reg_rem = 0xff;
    uint8_t ext_words = 0;
#ifndef __aarch64__
    uint8_t reg_overflow = 0xff;
    RAStateSnapshot divide_by_zero_state;
    uint32_t *branch_nonzero = NULL;
    uint32_t *branch_exception_end = NULL;
    uint32_t *branch_unit_end = NULL;
#endif

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

#ifdef __aarch64__
    *ptr++ = ands_immed(31, reg_q, 16, 0);
    uint32_t *tmp_ptr = ptr;
    *ptr++ = b_cc(A64_CC_NE, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
#else
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;
    ptr = EMIT_FlushPC(ptr);

    *ptr++ = cmp_immed(reg_q, 0);
    branch_nonzero = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);

    RA_SaveState(&divide_by_zero_state);
    ptr = EMIT_DivideByZeroException(ptr, ext_words, m68k_ptr);
    RA_RestoreState(&divide_by_zero_state);
    branch_exception_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_nonzero = b_cc(ARM_CC_NE, ptr - branch_nonzero - 2);
#endif

    reg_quot = RA_AllocARMRegister(&ptr);
    reg_rem = RA_AllocARMRegister(&ptr);


#ifdef __aarch64__
    *ptr++ = sxth(reg_rem, reg_q);
    *ptr++ = sdiv(reg_quot, reg_a, reg_rem);
    *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_rem);
#else
    if (Features.ARM_SUPPORTS_DIV)
    {
        /* Sign extend divisor from 16-bit to 32-bit */
        *ptr++ = sxth(reg_rem, reg_q, 0);
        *ptr++ = sdiv(reg_quot, reg_a, reg_rem);
        *ptr++ = mls(reg_rem, reg_a, reg_quot, reg_rem);
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));

        if (reg_a != 1)
            *ptr++ = push(1 << reg_a);
        if (reg_q != 2) {
            *ptr++ = push(1 << reg_q);
            *ptr++ = pop(4);
        }
        if (reg_a != 1)
            *ptr++ = pop(2);

        /* Call (u)idivmod */
        *ptr++ = sub_immed(13, 13, 8);
        *ptr++ = mov_reg(0, 13);
        *ptr++ = ldr_offset(15, 12, 4);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&sidiv);

        /* Pop quotient and (eventually) reminder from the stack */
        *ptr++ = pop(1 << reg_quot);
        *ptr++ = pop(1 << reg_rem);

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12) | (1 << 14)) & ~((1 << reg_quot) | (1 << reg_rem)));
    }
#endif

#ifdef __aarch64__
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = sxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    RA_FreeARMRegister(&ptr, tmp);
#else
    reg_overflow = RA_AllocARMRegister(&ptr);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = sxth(tmp, reg_quot, 0);
    *ptr++ = cmp_reg(tmp, reg_quot);
    *ptr++ = mov_cc_immed_u8(ARM_CC_EQ, reg_overflow, 0);
    *ptr++ = mov_cc_immed_u8(ARM_CC_NE, reg_overflow, 1);
    RA_FreeARMRegister(&ptr, tmp);
#endif

#ifdef __aarch64__
    (*m68k_ptr) += ext_words;
#endif

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_ClearFlags(ptr, cc, update_mask);

        if (update_mask & SR_V) {
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }
        if (update_mask & (SR_Z | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);
#else
            *ptr++ = cmp_immed(reg_quot, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

#ifdef __aarch64__
    if (update_mask & SR_V) {
        uint8_t cc = RA_GetCC(&ptr);
        *ptr++ = tbnz(cc, SRB_V, 3);
    }
    else {
        *ptr++ = b_cc(A64_CC_NE, 3);
    }
#else
    *ptr++ = cmp_immed(reg_overflow, 0);
    uint32_t *branch_overflow = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);
#endif

    /* Move signed 16-bit quotient to lower 16 bits of target register, signed 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

#ifndef __aarch64__
    *branch_overflow = b_cc(ARM_CC_NE, ptr - branch_overflow - 2);
    RA_FreeARMRegister(&ptr, reg_overflow);
#endif

    /* Advance PC */
#ifdef __aarch64__
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
#endif

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

#ifndef __aarch64__
    branch_unit_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_exception_end = b_cc(ARM_CC_AL, ptr - branch_exception_end - 2);
    *branch_unit_end = b_cc(ARM_CC_AL, ptr - branch_unit_end - 2);
    *ptr++ = (uint32_t)(uintptr_t)branch_unit_end;
    *ptr++ = (uint32_t)(uintptr_t)branch_exception_end;
    *ptr++ = 2;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);

    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

    return ptr;
}

uint32_t *EMIT_DIVU_W(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint8_t reg_a = RA_MapM68kRegister(&ptr, (opcode >> 9) & 7);
    uint8_t reg_q = 0xff;
    uint8_t reg_quot = 0xff;
    uint8_t reg_rem = 0xff;
    uint8_t ext_words = 0;
#ifndef __aarch64__
    uint8_t reg_overflow = 0xff;
    RAStateSnapshot divide_by_zero_state;
    uint32_t *branch_nonzero = NULL;
    uint32_t *branch_exception_end = NULL;
    uint32_t *branch_unit_end = NULL;
#endif

    ptr = EMIT_LoadFromEffectiveAddress(ptr, 2, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 0, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

#ifdef __aarch64__
    *ptr++ = ands_immed(31, reg_q, 16, 0);
    uint32_t *tmp_ptr = ptr;
    *ptr++ = b_cc(A64_CC_NE, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);
#else
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;
    ptr = EMIT_FlushPC(ptr);

    *ptr++ = cmp_immed(reg_q, 0);
    branch_nonzero = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);

    RA_SaveState(&divide_by_zero_state);
    ptr = EMIT_DivideByZeroException(ptr, ext_words, m68k_ptr);
    RA_RestoreState(&divide_by_zero_state);
    branch_exception_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_nonzero = b_cc(ARM_CC_NE, ptr - branch_nonzero - 2);
#endif

    reg_quot = RA_AllocARMRegister(&ptr);
    reg_rem = RA_AllocARMRegister(&ptr);

#ifdef __aarch64__
    *ptr++ = uxth(reg_rem, reg_q);
    *ptr++ = udiv(reg_quot, reg_a, reg_rem);
    *ptr++ = msub(reg_rem, reg_a, reg_quot, reg_rem);
#else
    if (Features.ARM_SUPPORTS_DIV)
    {
        /* Sign extend divisor from 16-bit to 32-bit */
        *ptr++ = uxth(reg_rem, reg_q, 0);
        *ptr++ = udiv(reg_quot, reg_a, reg_rem);
        *ptr++ = mls(reg_rem, reg_a, reg_quot, reg_rem);
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_quot and reg_rem in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));

        if (reg_a != 1)
            *ptr++ = push(1 << reg_a);
        if (reg_q != 2) {
            *ptr++ = push(1 << reg_q);
            *ptr++ = pop(4);
        }
        if (reg_a != 1)
            *ptr++ = pop(2);

        /* Call (u)idivmod */
        *ptr++ = sub_immed(13, 13, 8);
        *ptr++ = mov_reg(0, 13);
        *ptr++ = ldr_offset(15, 12, 4);
        *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
        *ptr++ = b_cc(ARM_CC_AL, 0);
        *ptr++ = BE32((uint32_t)&uidiv);

        /* Pop quotient and (eventually) reminder from the stack */
        *ptr++ = pop(1 << reg_quot);
        *ptr++ = pop(1 << reg_rem);

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_a) | (1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_quot) | (1 << reg_rem)));
    }
#endif

#ifdef __aarch64__
    uint8_t tmp = RA_AllocARMRegister(&ptr);

    *ptr++ = uxth(tmp, reg_quot);
    *ptr++ = cmp_reg(tmp, reg_quot, LSL, 0);

    RA_FreeARMRegister(&ptr, tmp);
#else
    reg_overflow = RA_AllocARMRegister(&ptr);
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    *ptr++ = uxth(tmp, reg_quot, 0);
    *ptr++ = cmp_reg(tmp, reg_quot);
    *ptr++ = mov_cc_immed_u8(ARM_CC_EQ, reg_overflow, 0);
    *ptr++ = mov_cc_immed_u8(ARM_CC_NE, reg_overflow, 1);
    RA_FreeARMRegister(&ptr, tmp);
#endif

#ifdef __aarch64__
    (*m68k_ptr) += ext_words;
#endif

    RA_SetDirtyM68kRegister(&ptr, (opcode >> 9) & 7);

    /* if temporary register was 0 the division was successful, otherwise overflow occured! */
    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);

        ptr = EMIT_ClearFlags(ptr, cc, update_mask);

        if (update_mask & SR_V) {
            
            ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
        }

        if (update_mask & (SR_Z | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_quot, LSL, 16);
#else
            *ptr++ = cmp_immed(reg_quot, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_Z) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
            }
            if (update_mask & SR_N) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            }
        }
    }

#ifdef __aarch64__
    if (update_mask & SR_V) {
        uint8_t cc = RA_GetCC(&ptr);
        *ptr++ = tbnz(cc, SRB_V, 3);
    }
    else {
        *ptr++ = b_cc(A64_CC_NE, 3);
    }
#else
    *ptr++ = cmp_immed(reg_overflow, 0);
    uint32_t *branch_overflow = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);
#endif

    /* Move unsigned 16-bit quotient to lower 16 bits of target register, unsigned 16 bit reminder to upper 16 bits */
    *ptr++ = mov_reg(reg_a, reg_quot);
    *ptr++ = bfi(reg_a, reg_rem, 16, 16);

#ifndef __aarch64__
    *branch_overflow = b_cc(ARM_CC_NE, ptr - branch_overflow - 2);
    RA_FreeARMRegister(&ptr, reg_overflow);
#endif

    /* Advance PC */
#ifdef __aarch64__
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
#endif

    RA_FreeARMRegister(&ptr, reg_a);
    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_quot);
    RA_FreeARMRegister(&ptr, reg_rem);

#ifndef __aarch64__
    branch_unit_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_exception_end = b_cc(ARM_CC_AL, ptr - branch_exception_end - 2);
    *branch_unit_end = b_cc(ARM_CC_AL, ptr - branch_unit_end - 2);
    *ptr++ = (uint32_t)(uintptr_t)branch_unit_end;
    *ptr++ = (uint32_t)(uintptr_t)branch_exception_end;
    *ptr++ = 2;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);

    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

    return ptr;
}

uint32_t *EMIT_DIVUS_L(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    uint8_t update_mask = M68K_GetSRMask(*m68k_ptr - 1);
    uint16_t opcode2 = BE16((*m68k_ptr)[0]);
    uint8_t sig = (opcode2 & (1 << 11)) != 0;
    uint8_t div64 = (opcode2 & (1 << 10)) != 0;
    uint8_t reg_q = 0xff;
    uint8_t reg_dq = RA_MapM68kRegister(&ptr, (opcode2 >> 12) & 7);
    uint8_t reg_dr = RA_MapM68kRegister(&ptr, opcode2 & 7);
    uint8_t ext_words = 1;
#ifndef __aarch64__
    RAStateSnapshot divide_by_zero_state;
    uint32_t *branch_nonzero = NULL;
    uint32_t *branch_exception_end = NULL;
    uint32_t *branch_unit_end = NULL;
#endif

    // Load divisor
    ptr = EMIT_LoadFromEffectiveAddress(ptr, 4, &reg_q, opcode & 0x3f, *m68k_ptr, &ext_words, 1, NULL);
    ptr = EMIT_FlushPC(ptr);
    RA_GetCC(&ptr);

    // Check if division by 0
#ifdef __aarch64__
    uint32_t *tmp_ptr = ptr;
    *ptr++ = cbnz(reg_q, 2);

    if (1)
    {
        /*
            This is a point of no return. Issue division by zero exception here
        */
        *ptr++ = add_immed(REG_PC, REG_PC, 2 * (ext_words + 1));

        ptr = EMIT_Exception(ptr, VECTOR_DIVIDE_BY_ZERO, 2, (uint32_t)(intptr_t)(*m68k_ptr - 1));

        RA_StoreDirtyFPURegs(&ptr);
        RA_StoreDirtyM68kRegs(&ptr);

        RA_StoreCC(&ptr);
        RA_StoreFPCR(&ptr);
        RA_StoreFPSR(&ptr);
        
#if EMU68_INSN_COUNTER
        extern uint32_t insn_count;
        uint8_t ctx_free = 0;
        uint8_t ctx = RA_TryCTX(&ptr);
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        if (ctx == 0xff)
        {
            ctx = RA_AllocARMRegister(&ptr);
            *ptr++ = mrs(ctx, 3, 3, 13, 0, 3);
            ctx_free = 1;
        }
        *ptr++ = ldr64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));
        *ptr++ = add64_immed(tmp, tmp, insn_count & 0xfff);
        if (insn_count & 0xfff000)
            *ptr++ = adds64_immed_lsl12(tmp, tmp, insn_count >> 12);
        *ptr++ = str64_offset(ctx, tmp, __builtin_offsetof(struct M68KState, INSN_COUNT));

        RA_FreeARMRegister(&ptr, tmp);
        if (ctx_free)
            RA_FreeARMRegister(&ptr, ctx);
#endif
        /* Return here */
        *ptr++ = bx_lr();
    }
    /* Update branch to the continuation */
    *tmp_ptr = cbnz(reg_q, ptr - tmp_ptr);
#else
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
    (*m68k_ptr) += ext_words;
    ptr = EMIT_FlushPC(ptr);

    *ptr++ = cmp_immed(reg_q, 0);
    branch_nonzero = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);

    RA_SaveState(&divide_by_zero_state);
    ptr = EMIT_DivideByZeroException(ptr, ext_words, m68k_ptr);
    RA_RestoreState(&divide_by_zero_state);
    branch_exception_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_nonzero = b_cc(ARM_CC_NE, ptr - branch_nonzero - 2);
#endif


#ifdef __aarch64__
    if (div64)
    {
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        uint8_t result = RA_AllocARMRegister(&ptr);
        uint8_t tmp2 = RA_AllocARMRegister(&ptr);

        // Use temporary result - in case of overflow destination regs remain unchanged
        *ptr++ = mov_reg(tmp2, reg_dq);
        *ptr++ = bfi64(tmp2, reg_dr, 32, 32);

        if (sig)
        {
            uint8_t q_ext = RA_AllocARMRegister(&ptr);
            *ptr++ = sxtw64(q_ext, reg_q);
            *ptr++ = sdiv64(result, tmp2, q_ext);
            if (reg_dr != reg_dq)
                *ptr++ = msub64(tmp, tmp2, result, q_ext);
            RA_FreeARMRegister(&ptr, q_ext);
        }
        else
        {
            *ptr++ = udiv64(result, tmp2, reg_q);
            if (reg_dr != reg_dq)
                *ptr++ = msub64(tmp, tmp2, result, reg_q);
        }

        if (sig) {
            *ptr++ = sxtw64(tmp2, result);
        }
        else {
            *ptr++ = mov_reg(tmp2, result);
        }
        *ptr++ = cmp64_reg(tmp2, result, LSL, 0);

        tmp_ptr = ptr;
        *ptr++ = b_cc(A64_CC_NE, 0);

        *ptr++ = mov_reg(reg_dq, result);
        if (reg_dr != reg_dq) {
            *ptr++ = mov_reg(reg_dr, tmp);
        }

        *tmp_ptr = b_cc(A64_CC_NE, ptr - tmp_ptr);

        RA_FreeARMRegister(&ptr, tmp);
        RA_FreeARMRegister(&ptr, tmp2);
        RA_FreeARMRegister(&ptr, result);
    }
    else
    {
        if (reg_dr == reg_dq)
        {
            if (sig)
                *ptr++ = sdiv(reg_dq, reg_dq, reg_q);
            else
                *ptr++ = udiv(reg_dq, reg_dq, reg_q);
        }
        else
        {
            uint8_t tmp = RA_AllocARMRegister(&ptr);

            if (sig)
                *ptr++ = sdiv(tmp, reg_dq, reg_q);
            else
                *ptr++ = udiv(tmp, reg_dq, reg_q);

            *ptr++ = msub(reg_dr, reg_dq, tmp, reg_q);
            *ptr++ = mov_reg(reg_dq, tmp);

            RA_FreeARMRegister(&ptr, tmp);
        }
    }
#else
    if (Features.ARM_SUPPORTS_DIV)
    {
        if (div64)
        {
            uint8_t reg_res_q = RA_AllocARMRegister(&ptr);
            uint8_t reg_res_r = RA_AllocARMRegister(&ptr);
            uint8_t reg_spills[4] = { 0xff, 0xff, 0xff, 0xff };
            uint8_t reg_spill_count = 0;

            while ((reg_res_q < 4) || (reg_res_r < 4))
            {
                if (reg_res_q < 4)
                {
                    reg_spills[reg_spill_count++] = reg_res_q;
                    reg_res_q = RA_AllocARMRegister(&ptr);
                }
                if (reg_res_r < 4)
                {
                    reg_spills[reg_spill_count++] = reg_res_r;
                    reg_res_r = RA_AllocARMRegister(&ptr);
                }
            }

            *ptr++ = push(0x0f | (1 << 12) | (1 << 14));
            *ptr++ = sub_immed(13, 13, 16);
            *ptr++ = mov_reg(0, 13);
            if (reg_dq < 4)
            {
                *ptr++ = ldr_offset(13, 1, 16 + 4 * reg_dq);
            }
            else if (reg_dq != 1)
            {
                *ptr++ = mov_reg(1, reg_dq);
            }
            if (reg_dr < 4)
            {
                *ptr++ = ldr_offset(13, 2, 16 + 4 * reg_dr);
            }
            else if (reg_dr != 2)
            {
                *ptr++ = mov_reg(2, reg_dr);
            }
            if (reg_q < 4)
            {
                *ptr++ = ldr_offset(13, 3, 16 + 4 * reg_q);
            }
            else if (reg_q != 3)
            {
                *ptr++ = mov_reg(3, reg_q);
            }

            *ptr++ = ldr_offset(15, 12, 4);
            *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
            *ptr++ = b_cc(ARM_CC_AL, 0);
            if (sig)
                *ptr++ = BE32((uint32_t)&sldiv_64_32);
            else
                *ptr++ = BE32((uint32_t)&uldiv_64_32);

            *ptr++ = ldr_offset(13, reg_res_q, 0);
            *ptr++ = ldr_offset(13, reg_res_r, 8);
            *ptr++ = ldr_offset(13, 12, 4);

            if (sig)
            {
                *ptr++ = asr_immed(3, reg_res_q, 31);
                *ptr++ = cmp_reg(12, 3);
            }
            else
            {
                *ptr++ = cmp_immed(12, 0);
            }

            *ptr++ = add_immed(13, 13, 16);
            *ptr++ = pop(0x0f | (1 << 12) | (1 << 14));

            *ptr++ = mov_cc_reg(ARM_CC_EQ, reg_dq, reg_res_q);
            if (reg_dr != reg_dq)
                *ptr++ = mov_cc_reg(ARM_CC_EQ, reg_dr, reg_res_r);

            RA_FreeARMRegister(&ptr, reg_res_q);
            RA_FreeARMRegister(&ptr, reg_res_r);
            while (reg_spill_count > 0)
                RA_FreeARMRegister(&ptr, reg_spills[--reg_spill_count]);
        }
        else
        {
            if (reg_dr == 0xff)
            {
                if (sig)
                    *ptr++ = sdiv(reg_dq, reg_dq, reg_q);
                else
                    *ptr++ = udiv(reg_dq, reg_dq, reg_q);
            }
            else
            {
                uint8_t tmp = RA_AllocARMRegister(&ptr);

                if (sig)
                    *ptr++ = sdiv(tmp, reg_dq, reg_q);
                else
                    *ptr++ = udiv(tmp, reg_dq, reg_q);

                *ptr++ = mls(reg_dr, reg_dq, tmp, reg_q);
                *ptr++ = mov_reg(reg_dq, tmp);

                RA_FreeARMRegister(&ptr, tmp);
            }
        }
    }
    else
    {
        /* Keep r0-r3,lr and ip safe on the stack. Exclude reg_dr and reg_dq in case they were allocated in r0..r4 range */
        *ptr++ = push(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));

        /* In case of 64-bit division use (u)ldivmod, otherwise use (u)idivmod */
        if (div64)
        {
    kprintf("64 bit division not done yet!\n");
        }
        else
        {
            /* Use stack to put divisor and divident into registers */
            if (reg_dq != 1)
                *ptr++ = push(1 << reg_dq);
            if (reg_q != 2) {
                *ptr++ = push(1 << reg_q);
                *ptr++ = pop(4);
            }
            if (reg_dq != 1)
                *ptr++ = pop(2);

            /* Call (u)idivmod */
            *ptr++ = sub_immed(13, 13, 8);
            *ptr++ = mov_reg(0, 13);
            *ptr++ = ldr_offset(15, 12, 4);
            *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
            *ptr++ = b_cc(ARM_CC_AL, 0);
            if (sig)
                *ptr++ = BE32((uint32_t)&sidiv);
            else
                *ptr++ = BE32((uint32_t)&uidiv);

            /* Pop quotient and (eventually) reminder from the stack */
            *ptr++ = pop(1 << reg_dq);
            if (reg_dr != 0xff)
                *ptr++ = pop(1 << reg_dr);
            else
                *ptr++ = add_immed(13, 13, 4);
        }

        /* Restore registers from the stack */
        *ptr++ = pop(((1 << reg_q) | 0x0f | (1 << 12)) & ~((1 << reg_dr) | (1 << reg_dq)));
    }
#endif

#ifdef __aarch64__
    (*m68k_ptr) += ext_words;
#endif

    /* Set Dq dirty */
    RA_SetDirtyM68kRegister(&ptr, (opcode2 >> 12) & 7);
    /* Set Dr dirty if it was used/changed */
    if (reg_dr != 0xff)
        RA_SetDirtyM68kRegister(&ptr, opcode2 & 7);

    if (update_mask)
    {
        uint8_t cc = RA_ModifyCC(&ptr);
        if (update_mask & SR_VC) {
            ptr = EMIT_ClearFlags(ptr, cc, SR_V | SR_C);
            if (div64) {
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_V, ARM_CC_NE);
            }
        }
        if (update_mask & (SR_C | SR_N))
        {
#ifdef __aarch64__
            *ptr++ = cmn_reg(31, reg_dq, LSL, 0);
#else
            *ptr++ = cmp_immed(reg_dq, 0);
#endif
            ptr = EMIT_GetNZxx(ptr, cc, &update_mask);
            if (update_mask & SR_N)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_N, ARM_CC_MI);
            if (update_mask & SR_Z)
                ptr = EMIT_SetFlagsConditional(ptr, cc, SR_Z, ARM_CC_EQ);
        }
    }

    /* Advance PC */
#ifdef __aarch64__
    ptr = EMIT_AdvancePC(ptr, 2 * (ext_words + 1));
#endif

    RA_FreeARMRegister(&ptr, reg_q);
    RA_FreeARMRegister(&ptr, reg_dq);
    if (reg_dr != 0xff)
        RA_FreeARMRegister(&ptr, reg_dr);

#ifndef __aarch64__
    branch_unit_end = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);

    *branch_exception_end = b_cc(ARM_CC_AL, ptr - branch_exception_end - 2);
    *branch_unit_end = b_cc(ARM_CC_AL, ptr - branch_unit_end - 2);
    *ptr++ = (uint32_t)(uintptr_t)branch_unit_end;
    *ptr++ = (uint32_t)(uintptr_t)branch_exception_end;
    *ptr++ = 2;
    *ptr++ = 0;
    *ptr++ = INSN_TO_LE(0xfffffffe);
    *ptr++ = INSN_TO_LE(0xffffffff);

    if (!Features.ARM_SUPPORTS_DIV)
        *ptr++ = INSN_TO_LE(0xfffffff0);
#endif

    return ptr;
}

uint32_t *EMIT_MUL_DIV(uint32_t *ptr, uint16_t opcode, uint16_t **m68k_ptr)
{
    (void)m68k_ptr;

    if ((opcode & 0xf1c0) == 0xc1c0)
    {
        ptr = EMIT_MULS_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0xc0c0)
    {
        ptr = EMIT_MULU_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xffc0) == 0x4c00)
    {
        ptr = EMIT_MULS_L(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xffc0) == 0x4c40)
    {
        ptr = EMIT_DIVUS_L(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0x81c0)
    {
        ptr = EMIT_DIVS_W(ptr, opcode, m68k_ptr);
    }
    if ((opcode & 0xf1c0) == 0x80c0)
    {
        ptr = EMIT_DIVU_W(ptr, opcode, m68k_ptr);
    }

    return ptr;
}
