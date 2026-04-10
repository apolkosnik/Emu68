#include <stdint.h>
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

#ifndef __aarch64__
extern void pistorm_handle_overlay_byte_store(uint32_t address, uint32_t value);
extern void pistorm_handle_special_write(uint32_t address, uint32_t value, uint32_t size);
extern uint32_t pistorm_handle_special_read(uint32_t address, uint32_t size);
extern uint32_t pistorm_handle_special_read_trampoline(uint32_t address, uint32_t size);
extern void pistorm_special_read_callsite_probe(uint32_t address, uint32_t size);
extern uint32_t overlay;

static inline __attribute__((always_inline)) uint32_t *emit_blx_literal(uint32_t *ptr, uintptr_t target)
{
    uint32_t *skip_literal;

    *ptr++ = ldr_offset(15, 12, 4);
    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
    skip_literal = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    *ptr++ = BE32((uint32_t)target);
    *skip_literal = b_cc(ARM_CC_AL, ptr - skip_literal - 2);
    return ptr;
}

static inline __attribute__((always_inline)) uint32_t *emit_overlay_byte_store_hook(uint32_t *ptr, uint8_t addr_reg, uint8_t value_reg)
{
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t flags_reg = RA_AllocARMRegister(&ptr);
    uint16_t scratch_save_mask = (uint16_t)((1 << tmp) | (1 << flags_reg));
    uint16_t call_save_mask = (uint16_t)(0x0f | (1 << REG_SR) | (1 << REG_CTX) | (1 << 12) | (1 << 14) | (1 << flags_reg));
    uint32_t *skip_hook;
    uint32_t *skip_literal;
    uint32_t *restore_flags;

    *ptr++ = push(scratch_save_mask);
    *ptr++ = mrs(flags_reg);
    *ptr++ = movw_immed_u16(tmp, 0xe001);
    *ptr++ = movt_immed_u16(tmp, 0x00bf);
    *ptr++ = cmp_reg(addr_reg, tmp);
    skip_hook = ptr;
    *ptr++ = b_cc(ARM_CC_NE, 0);

    *ptr++ = push(call_save_mask);
    if (value_reg == 0 && addr_reg != 0)
        *ptr++ = mov_reg(tmp, value_reg);
    if (addr_reg != 0)
        *ptr++ = mov_reg(0, addr_reg);
    if (value_reg != 1)
    {
        if (value_reg == 0 && addr_reg != 0)
            *ptr++ = mov_reg(1, tmp);
        else
            *ptr++ = mov_reg(1, value_reg);
    }
    *ptr++ = ldr_offset(15, 12, 8);
    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
    *ptr++ = pop(call_save_mask);
    skip_literal = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    *ptr++ = BE32((uint32_t)(uintptr_t)&pistorm_handle_overlay_byte_store);
    restore_flags = ptr;
    *ptr++ = msr(flags_reg, 8);
    *ptr++ = pop(scratch_save_mask);

    *skip_hook = b_cc(ARM_CC_NE, restore_flags - skip_hook - 2);
    *skip_literal = b_cc(ARM_CC_AL, restore_flags - skip_literal - 2);

    RA_FreeARMRegister(&ptr, flags_reg);
    RA_FreeARMRegister(&ptr, tmp);
    return ptr;
}

static inline __attribute__((always_inline)) uint32_t *emit_special_store_hook(uint32_t *ptr, uint8_t size, uint8_t addr_reg, uint8_t value_reg)
{
    uint8_t tmp = RA_AllocARMRegister(&ptr);
    uint8_t flags_reg = RA_AllocARMRegister(&ptr);
    uint16_t scratch_save_mask = (uint16_t)((1 << tmp) | (1 << flags_reg));
    uint32_t *match[16];
    uint32_t *cia_skip = NULL;
    uint32_t *cia_match = NULL;
    uint32_t *cia_after = NULL;
    uint32_t *custom_skip = NULL;
    uint32_t *custom_match = NULL;
    uint32_t *custom_after = NULL;
    uint32_t *custom_mirror_skip_lo = NULL;
    uint32_t *custom_mirror_skip_hi = NULL;
    uint32_t *custom_mirror_match = NULL;
    uint32_t *custom_mirror_after = NULL;
    uint32_t *zorro_skip = NULL;
    uint32_t *zorro_match = NULL;
    uint32_t *zorro_after = NULL;
    uint32_t *slow_skip = NULL;
    uint32_t *slow_match = NULL;
    uint32_t *slow_after = NULL;
    uint32_t *c0_skip = NULL;
    uint32_t *c0_match = NULL;
    uint32_t *c0_after = NULL;
    uint32_t *skip_call;
    uint32_t *skip_literal;
    uint32_t *call_start;
    uint32_t *restore_flags;
    int match_count = 0;
    const uint16_t save_mask = 0x0f | (1 << REG_SR) | (1 << REG_CTX) | (1 << 12) | (1 << 14);
    const uint16_t call_save_mask = (uint16_t)(save_mask | (1 << flags_reg));

    *ptr++ = push(scratch_save_mask);
    *ptr++ = mrs(flags_reg);
    switch (size)
    {
        case 4:
            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            match[match_count++] = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        case 1:
            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            match[match_count++] = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0xa000);
            *ptr++ = movt_immed_u16(tmp, 0x00bf);
            *ptr++ = cmp_reg(addr_reg, tmp);
            cia_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0xf000);
            *ptr++ = movt_immed_u16(tmp, 0x00bf);
            *ptr++ = cmp_reg(addr_reg, tmp);
            cia_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            cia_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0xf000);
            *ptr++ = movt_immed_u16(tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0xf200);
            *ptr++ = movt_immed_u16(tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            custom_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_mirror_skip_lo = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_mirror_skip_hi = ptr;
            *ptr++ = b_cc(ARM_CC_CS, 0);

            *ptr++ = lsr_immed(tmp, addr_reg, 12);
            *ptr++ = and_immed(tmp, tmp, 0x0f);
            *ptr++ = cmp_immed(tmp, 0x0f);
            custom_mirror_match = ptr;
            *ptr++ = b_cc(ARM_CC_EQ, 0);
            custom_mirror_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e8);
            *ptr++ = cmp_reg(addr_reg, tmp);
            zorro_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e9);
            *ptr++ = cmp_reg(addr_reg, tmp);
            zorro_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            zorro_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        case 2:
            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            match[match_count++] = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0xf000);
            *ptr++ = movt_immed_u16(tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0xf200);
            *ptr++ = movt_immed_u16(tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            custom_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_mirror_skip_lo = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            custom_mirror_skip_hi = ptr;
            *ptr++ = b_cc(ARM_CC_CS, 0);

            *ptr++ = lsr_immed(tmp, addr_reg, 12);
            *ptr++ = and_immed(tmp, tmp, 0x0f);
            *ptr++ = cmp_immed(tmp, 0x0f);
            custom_mirror_match = ptr;
            *ptr++ = b_cc(ARM_CC_EQ, 0);
            custom_mirror_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(tmp, 0x0000);
            *ptr++ = movt_immed_u16(tmp, 0x00c8);
            *ptr++ = cmp_reg(addr_reg, tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        default:
            RA_FreeARMRegister(&ptr, tmp);
            return ptr;
    }

    skip_call = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    call_start = ptr;

    *ptr++ = push(call_save_mask);
    if (value_reg == 0 && addr_reg != 0)
        *ptr++ = mov_reg(tmp, value_reg);
    if (addr_reg != 0)
        *ptr++ = mov_reg(0, addr_reg);
    if (value_reg != 1)
    {
        if (value_reg == 0 && addr_reg != 0)
            *ptr++ = mov_reg(1, tmp);
        else
            *ptr++ = mov_reg(1, value_reg);
    }
    *ptr++ = mov_immed_u8(2, size);
    *ptr++ = ldr_offset(15, 12, 8);
    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
    *ptr++ = pop(call_save_mask);
    skip_literal = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    *ptr++ = BE32((uint32_t)(uintptr_t)&pistorm_handle_special_write);
    restore_flags = ptr;
    *ptr++ = msr(flags_reg, 8);
    *ptr++ = pop(scratch_save_mask);

    for (int i = 0; i < match_count; ++i)
        *match[i] = b_cc(ARM_CC_CC, call_start - match[i] - 2);
    if (cia_skip != NULL)
        *cia_skip = b_cc(ARM_CC_CC, cia_after - cia_skip - 2);
    if (cia_match != NULL)
        *cia_match = b_cc(ARM_CC_CC, call_start - cia_match - 2);
    if (custom_skip != NULL)
        *custom_skip = b_cc(ARM_CC_CC, custom_after - custom_skip - 2);
    if (custom_match != NULL)
        *custom_match = b_cc(ARM_CC_CC, call_start - custom_match - 2);
    if (custom_mirror_skip_lo != NULL)
        *custom_mirror_skip_lo = b_cc(ARM_CC_CC, custom_mirror_after - custom_mirror_skip_lo - 2);
    if (custom_mirror_skip_hi != NULL)
        *custom_mirror_skip_hi = b_cc(ARM_CC_CS, custom_mirror_after - custom_mirror_skip_hi - 2);
    if (custom_mirror_match != NULL)
        *custom_mirror_match = b_cc(ARM_CC_EQ, call_start - custom_mirror_match - 2);
    if (zorro_skip != NULL)
        *zorro_skip = b_cc(ARM_CC_CC, zorro_after - zorro_skip - 2);
    if (zorro_match != NULL)
        *zorro_match = b_cc(ARM_CC_CC, call_start - zorro_match - 2);
    if (slow_skip != NULL)
        *slow_skip = b_cc(ARM_CC_CC, slow_after - slow_skip - 2);
    if (slow_match != NULL)
        *slow_match = b_cc(ARM_CC_CC, call_start - slow_match - 2);
    if (c0_skip != NULL)
        *c0_skip = b_cc(ARM_CC_CC, c0_after - c0_skip - 2);
    if (c0_match != NULL)
        *c0_match = b_cc(ARM_CC_CC, call_start - c0_match - 2);
    *skip_call = b_cc(ARM_CC_AL, restore_flags - skip_call - 2);
    *skip_literal = b_cc(ARM_CC_AL, restore_flags - skip_literal - 2);

    RA_FreeARMRegister(&ptr, flags_reg);
    RA_FreeARMRegister(&ptr, tmp);
    return ptr;
}

static inline __attribute__((always_inline)) uint32_t *emit_special_load_hook(uint32_t *ptr, uint8_t size, uint8_t addr_reg, uint8_t value_reg)
{
    uint8_t cmp_tmp = RA_AllocARMRegister(&ptr);
    uint8_t flags_reg = RA_AllocARMRegister(&ptr);
    uint16_t scratch_save_mask = (uint16_t)((1 << cmp_tmp) | (1 << flags_reg));
    uint32_t *match[16];
    uint32_t *cia_skip = NULL;
    uint32_t *cia_match = NULL;
    uint32_t *cia_after = NULL;
    uint32_t *custom_skip = NULL;
    uint32_t *custom_match = NULL;
    uint32_t *custom_after = NULL;
    uint32_t *custom_mirror_skip_lo = NULL;
    uint32_t *custom_mirror_skip_hi = NULL;
    uint32_t *custom_mirror_match = NULL;
    uint32_t *custom_mirror_after = NULL;
    uint32_t *zorro_skip = NULL;
    uint32_t *zorro_match = NULL;
    uint32_t *zorro_after = NULL;
    uint32_t *slow_skip = NULL;
    uint32_t *slow_match = NULL;
    uint32_t *slow_after = NULL;
    uint32_t *c0_skip = NULL;
    uint32_t *c0_match = NULL;
    uint32_t *c0_after = NULL;
    uint32_t *skip_call;
    uint32_t *skip_literal;
    uint32_t *call_start;
    uint32_t *restore_flags;
    int match_count = 0;
    const uint16_t save_mask = 0x0f | (1 << REG_SR) | (1 << REG_CTX) | (1 << 12) | (1 << 14);
    const uint16_t call_save_mask = (uint16_t)((save_mask | (1 << flags_reg)) & ~(1 << value_reg));

    *ptr++ = push(scratch_save_mask);
    *ptr++ = mrs(flags_reg);
    switch (size)
    {
        case 4:
            /*
             * Low-memory longword reads are safe through the raw ARM32 mapping
             * now that page-0 overlay and fake low RAM are mapped directly.
             * Avoid the special-read helper for this range so wrapped-ROM
             * compare loops can stay on the normal fast path.
             */
            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        case 1:
            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            match[match_count++] = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0xa000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00bf);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            cia_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0xf000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00bf);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            cia_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            cia_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0xf000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0xf200);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            custom_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_mirror_skip_lo = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_mirror_skip_hi = ptr;
            *ptr++ = b_cc(ARM_CC_CS, 0);

            *ptr++ = lsr_immed(cmp_tmp, addr_reg, 12);
            *ptr++ = and_immed(cmp_tmp, cmp_tmp, 0x0f);
            *ptr++ = cmp_immed(cmp_tmp, 0x0f);
            custom_mirror_match = ptr;
            *ptr++ = b_cc(ARM_CC_EQ, 0);
            custom_mirror_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e8);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            zorro_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e9);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            zorro_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            zorro_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        case 2:
            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            match[match_count++] = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0xf000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0xf200);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00df);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            custom_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_mirror_skip_lo = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00e0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            custom_mirror_skip_hi = ptr;
            *ptr++ = b_cc(ARM_CC_CS, 0);

            *ptr++ = lsr_immed(cmp_tmp, addr_reg, 12);
            *ptr++ = and_immed(cmp_tmp, cmp_tmp, 0x0f);
            *ptr++ = cmp_immed(cmp_tmp, 0x0f);
            custom_mirror_match = ptr;
            *ptr++ = b_cc(ARM_CC_EQ, 0);
            custom_mirror_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x0010);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            slow_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            slow_after = ptr;

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_skip = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);

            *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
            *ptr++ = movt_immed_u16(cmp_tmp, 0x00c8);
            *ptr++ = cmp_reg(addr_reg, cmp_tmp);
            c0_match = ptr;
            *ptr++ = b_cc(ARM_CC_CC, 0);
            c0_after = ptr;
            break;
        default:
            RA_FreeARMRegister(&ptr, cmp_tmp);
            return ptr;
    }

    skip_call = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    call_start = ptr;

    *ptr++ = push(call_save_mask);
    if (addr_reg != 0)
        *ptr++ = mov_reg(0, addr_reg);
    *ptr++ = mov_immed_u8(1, size);
    *ptr++ = push(0x0003);
    ptr = emit_blx_literal(ptr, (uintptr_t)&pistorm_special_read_callsite_probe);
    *ptr++ = pop(0x0003);
    ptr = emit_blx_literal(ptr, (uintptr_t)&pistorm_handle_special_read_trampoline);
    if (value_reg != 0)
        *ptr++ = mov_reg(value_reg, 0);
    *ptr++ = pop(call_save_mask);
    skip_literal = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    restore_flags = ptr;
    *ptr++ = msr(flags_reg, 8);
    *ptr++ = pop(scratch_save_mask);

    for (int i = 0; i < match_count; ++i)
        *match[i] = b_cc(ARM_CC_CC, call_start - match[i] - 2);
    if (cia_skip != NULL)
        *cia_skip = b_cc(ARM_CC_CC, cia_after - cia_skip - 2);
    if (cia_match != NULL)
        *cia_match = b_cc(ARM_CC_CC, call_start - cia_match - 2);
    if (custom_skip != NULL)
        *custom_skip = b_cc(ARM_CC_CC, custom_after - custom_skip - 2);
    if (custom_match != NULL)
        *custom_match = b_cc(ARM_CC_CC, call_start - custom_match - 2);
    if (custom_mirror_skip_lo != NULL)
        *custom_mirror_skip_lo = b_cc(ARM_CC_CC, custom_mirror_after - custom_mirror_skip_lo - 2);
    if (custom_mirror_skip_hi != NULL)
        *custom_mirror_skip_hi = b_cc(ARM_CC_CS, custom_mirror_after - custom_mirror_skip_hi - 2);
    if (custom_mirror_match != NULL)
        *custom_mirror_match = b_cc(ARM_CC_EQ, call_start - custom_mirror_match - 2);
    if (zorro_skip != NULL)
        *zorro_skip = b_cc(ARM_CC_CC, zorro_after - zorro_skip - 2);
    if (zorro_match != NULL)
        *zorro_match = b_cc(ARM_CC_CC, call_start - zorro_match - 2);
    if (slow_skip != NULL)
        *slow_skip = b_cc(ARM_CC_CC, slow_after - slow_skip - 2);
    if (slow_match != NULL)
        *slow_match = b_cc(ARM_CC_CC, call_start - slow_match - 2);
    if (c0_skip != NULL)
        *c0_skip = b_cc(ARM_CC_CC, c0_after - c0_skip - 2);
    if (c0_match != NULL)
        *c0_match = b_cc(ARM_CC_CC, call_start - c0_match - 2);
    *skip_call = b_cc(ARM_CC_AL, restore_flags - skip_call - 2);
    *skip_literal = b_cc(ARM_CC_AL, restore_flags - skip_literal - 2);

    RA_FreeARMRegister(&ptr, flags_reg);
    RA_FreeARMRegister(&ptr, cmp_tmp);
    return ptr;
}

uint32_t *EMIT_HookSpecialLoad(uint32_t *ptr, uint8_t size, uint8_t addr_reg, uint8_t value_reg)
{
#ifndef __aarch64__
    return emit_special_load_hook(ptr, size, addr_reg, value_reg);
#else
    (void)size;
    (void)addr_reg;
    (void)value_reg;
    return ptr;
#endif
}

uint32_t *EMIT_HookSpecialStore(uint32_t *ptr, uint8_t size, uint8_t addr_reg, uint8_t value_reg)
{
#ifndef __aarch64__
    return emit_special_store_hook(ptr, size, addr_reg, value_reg);
#else
    (void)size;
    (void)addr_reg;
    (void)value_reg;
    return ptr;
#endif
}

static inline __attribute__((always_inline)) uint32_t *emit_special_load_helper_only(uint32_t *ptr, uint8_t size, uint8_t addr_reg, uint8_t value_reg)
{
    uint8_t flags_reg = RA_AllocARMRegister(&ptr);
    uint16_t scratch_save_mask = (uint16_t)(1 << flags_reg);
    uint32_t *skip_literal;
    uint32_t *restore_flags;
    const uint16_t save_mask = 0x0f | (1 << REG_SR) | (1 << REG_CTX) | (1 << 12) | (1 << 14);
    const uint16_t call_save_mask = (uint16_t)((save_mask | (1 << flags_reg)) & ~(1 << value_reg));

    *ptr++ = push(scratch_save_mask);
    *ptr++ = mrs(flags_reg);
    *ptr++ = push(call_save_mask);
    if (addr_reg != 0)
        *ptr++ = mov_reg(0, addr_reg);
    *ptr++ = mov_immed_u8(1, size);
    *ptr++ = push(0x0003);
    ptr = emit_blx_literal(ptr, (uintptr_t)&pistorm_special_read_callsite_probe);
    *ptr++ = pop(0x0003);
    ptr = emit_blx_literal(ptr, (uintptr_t)&pistorm_handle_special_read_trampoline);
    if (value_reg != 0)
        *ptr++ = mov_reg(value_reg, 0);
    *ptr++ = pop(call_save_mask);
    skip_literal = ptr;
    *ptr++ = b_cc(ARM_CC_AL, 0);
    restore_flags = ptr;
    *ptr++ = msr(flags_reg, 8);
    *ptr++ = pop(scratch_save_mask);

    *skip_literal = b_cc(ARM_CC_AL, restore_flags - skip_literal - 2);

    RA_FreeARMRegister(&ptr, flags_reg);
    return ptr;
}
#endif

static inline __attribute__((always_inline)) uint32_t * load_s16_ext32(uint32_t *ptr, uint8_t reg, int16_t s16)
{
#ifdef __aarch64__
    if (s16 & 0x8000)
        *ptr++ = movn_immed_u16(reg, ~s16, 0);
    else
        *ptr++ = movw_immed_u16(reg, s16);
#else
    if (s16 >= -256 && s16 < 0)
        *ptr++ = mvn_immed_u8(reg, ~s16);
    else {
        *ptr++ = movw_immed_u16(reg, s16);
        if (s16 & 0x8000)
            *ptr++ = movt_immed_u16(reg, 0xffff);
    }
#endif
    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * load_reg_from_addr_offset(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, int32_t offset, uint8_t offset_32bit)
{
    uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
    uint8_t hook_addr_reg = base;

    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

#ifndef __aarch64__
    if (size == 0 ||
        (size == 1 && (offset < -255 || offset > 255)) ||
        (size == 4 && (offset < -255 || offset > 255)) ||
        (size == 2 && (offset < -255 || offset > 255)) ||
        (offset < -4095 || offset > 4095))
    {
        if (offset_32bit) {
            *ptr++ = movw_immed_u16(reg_d16, offset);
            if ((offset >> 16) & 0xffff) {
                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
            }
        } else {
            *ptr++ = movw_immed_u16(reg_d16, offset);
            if (offset & 0x8000)
                *ptr++ = movt_immed_u16(reg_d16, 0xffff);
        }
    }
#endif

    switch (size)
    {
        case 4:
#ifdef __aarch64__
            if (offset == 0)
                *ptr++ = ldr_offset(base, reg, 0);
            else if (offset > -256 && offset < 256)
#ifdef __aarch64__
                *ptr++ = ldur_offset(base, reg, offset & 0x1ff);
#else
                *ptr++ = ldr_offset(base, reg, offset & 0x1ff);
#endif
            else if (offset > 0 && offset < 16384 && (offset & 3) == 0)
                *ptr++ = ldr_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if ((offset & 0xffff) != 0) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff)
                        {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        }
                        else
                        {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                }
                else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = ldr_offset(reg_d16, reg, 0);
            }
#else
                if (offset == 0)
                {
                    uint8_t low_reg = RA_AllocARMRegister(&ptr);
                    uint32_t *fast_load;
                    uint32_t *slow_done;
                    uint32_t *fast_target;
                    uint32_t *fast_done;
                    uint32_t *load_done;

                    /*
                     * Plain zero-offset longword reads back (An), (An)+ and
                     * -(An) on ARM32. Use the simpler direct load sequence
                     * here and let the shared special-read hook override
                     * low-memory/MMIO ranges afterwards. The heavier offset
                     * helper path was corrupting Dn in DiagROM's FixBitplane
                     * (move.l (a1)+,d0) loop.
                     */
                    *ptr++ = tst_immed(base, 2);
                    fast_load = ptr;
                    *ptr++ = b_cc(ARM_CC_EQ, 0);
                    *ptr++ = ldrh_offset(base, reg, 0);
                    *ptr++ = ldrh_offset(base, low_reg, 2);
                    *ptr++ = lsl_immed(reg, reg, 16);
                    *ptr++ = orr_reg(reg, reg, low_reg, 0);
                    slow_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    fast_target = ptr;
                    *ptr++ = ldr_offset(base, reg, 0);
                    fast_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    load_done = ptr;

                    *fast_load = b_cc(ARM_CC_EQ, fast_target - fast_load - 2);
                    *slow_done = b_cc(ARM_CC_AL, load_done - slow_done - 2);
                    *fast_done = b_cc(ARM_CC_AL, load_done - fast_done - 2);

                    RA_FreeARMRegister(&ptr, low_reg);
                }
                else
                {
                    uint8_t addr_reg = reg_d16;
                    uint8_t low_reg = RA_AllocARMRegister(&ptr);
                    uint8_t cmp_tmp = RA_AllocARMRegister(&ptr);
                    uint8_t flags_reg = RA_AllocARMRegister(&ptr);
                    uint16_t scratch_save_mask = (uint16_t)((1 << cmp_tmp) | (1 << flags_reg));
                    const uint16_t save_mask = 0x0f | (1 << REG_SR) | (1 << REG_CTX) | (1 << 12) | (1 << 14);
                    const uint16_t call_save_mask = (uint16_t)((save_mask | (1 << flags_reg)) & ~(1 << reg));
                    uint32_t *slow_skip = NULL;
                    uint32_t *slow_match = NULL;
                    uint32_t *slow_after = NULL;
                    uint32_t *c0_skip = NULL;
                    uint32_t *c0_match = NULL;
                    uint32_t *c0_after = NULL;
                    uint32_t *rom_direct = NULL;
                    uint32_t *low_match = NULL;
                    uint32_t *low_overlay_check;
                    uint32_t *low_overlay_direct;
                    uint32_t *low_overlay_call;
                    uint32_t *direct_restore;
                    uint32_t *direct_entry;
                    uint32_t *direct_fast;
                    uint32_t *direct_done;
                    uint32_t *direct_fast_target;
                    uint32_t *call_start;
                    uint32_t *skip_literal;
                    uint32_t *restore_start;
                    uint32_t *direct_restore_done;
                    uint32_t *direct_fast_done;
                    uint32_t *helper_restore_done;
                    uint32_t *load_complete;

                    if (offset == 0)
                        *ptr++ = mov_reg(addr_reg, base);
                    else if (offset > -256 && offset < 256)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(addr_reg, base, offset);
                        else
                            *ptr++ = sub_immed(addr_reg, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(addr_reg, base, reg_d16, 0);
                    }

                    *ptr++ = push(scratch_save_mask);
                    *ptr++ = mrs(flags_reg);

                    /*
                     * Kickstart space at 0x00f00000+ does not need any of the
                     * PiStorm helper range probes below. Jump straight to the
                     * direct longword load path so ROM checksum loops are not
                     * dominated by helper scaffolding. The scratch/flags save
                     * must happen before this branch, because the direct path
                     * restores it before returning to the unit epilogue.
                     */
                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x00f0);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    rom_direct = ptr;
                    *ptr++ = b_cc(ARM_CC_CS, 0);

                    /*
                     * On ARM32, low-memory and C0-window accesses are handled
                     * by the PiStorm helper. Probe those ranges before the raw
                     * halfword pair, otherwise real-ROM low-memory reads can
                     * stall before the helper gets a chance to override them.
                     */

                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    low_match = ptr;
                    *ptr++ = b_cc(ARM_CC_CC, 0);

                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x0008);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    slow_skip = ptr;
                    *ptr++ = b_cc(ARM_CC_CC, 0);

                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x0010);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    slow_match = ptr;
                    *ptr++ = b_cc(ARM_CC_CC, 0);
                    slow_after = ptr;

                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x00c0);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    c0_skip = ptr;
                    *ptr++ = b_cc(ARM_CC_CC, 0);

                    *ptr++ = movw_immed_u16(cmp_tmp, 0x0000);
                    *ptr++ = movt_immed_u16(cmp_tmp, 0x00c8);
                    *ptr++ = cmp_reg(addr_reg, cmp_tmp);
                    c0_match = ptr;
                    *ptr++ = b_cc(ARM_CC_CC, 0);
                    c0_after = ptr;

                    direct_restore = ptr;
                    *ptr++ = msr(flags_reg, 8);
                    *ptr++ = pop(scratch_save_mask);
                    direct_restore_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);

                    direct_entry = ptr;
                    *ptr++ = tst_immed(addr_reg, 2);
                    direct_fast = ptr;
                    *ptr++ = b_cc(ARM_CC_EQ, 0);
                    *ptr++ = ldrh_offset(addr_reg, reg, 0);
                    *ptr++ = ldrh_offset(addr_reg, low_reg, 2);
                    *ptr++ = lsl_immed(reg, reg, 16);
                    *ptr++ = orr_reg(reg, reg, low_reg, 0);
                    direct_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    direct_fast_target = ptr;
                    *ptr++ = ldr_offset(addr_reg, reg, 0);
                    direct_fast_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);

                    call_start = ptr;
                    *ptr++ = push(call_save_mask);
                    if (addr_reg != 0)
                        *ptr++ = mov_reg(0, addr_reg);
                    *ptr++ = mov_immed_u8(1, size);
                    *ptr++ = ldr_offset(15, 12, 8);
                    *ptr++ = blx_cc_reg(ARM_CC_AL, 12);
                    *ptr++ = pop(call_save_mask);
                    if (reg != 0)
                        *ptr++ = mov_reg(reg, 0);
                    skip_literal = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    *ptr++ = BE32((uint32_t)(uintptr_t)&pistorm_handle_special_read_trampoline);
                    restore_start = ptr;
                    *ptr++ = msr(flags_reg, 8);
                    *ptr++ = pop(scratch_save_mask);
                    helper_restore_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);

                    /*
                     * Low-memory longword reads must go through the PiStorm
                     * helper so wrapped/headerized ROM boots see the fake
                     * low-RAM backing rather than the raw physical page-zero
                     * contents.
                     */
                    low_overlay_check = ptr;
                    *ptr++ = movw_immed_u16(cmp_tmp, (uint32_t)(uintptr_t)&overlay & 0xffffu);
                    *ptr++ = movt_immed_u16(cmp_tmp, ((uint32_t)(uintptr_t)&overlay >> 16) & 0xffffu);
                    *ptr++ = ldr_offset(cmp_tmp, cmp_tmp, 0);
                    *ptr++ = cmp_immed(cmp_tmp, 0);
                    low_overlay_direct = ptr;
                    *ptr++ = b_cc(ARM_CC_EQ, 0);
                    low_overlay_call = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);

                    *low_match = b_cc(ARM_CC_CC, low_overlay_check - low_match - 2);
                    *low_overlay_direct = b_cc(ARM_CC_EQ, call_start - low_overlay_direct - 2);
                    *low_overlay_call = b_cc(ARM_CC_AL, call_start - low_overlay_call - 2);
                    *slow_skip = b_cc(ARM_CC_CC, slow_after - slow_skip - 2);
                    *slow_match = b_cc(ARM_CC_CC, call_start - slow_match - 2);
                    *c0_skip = b_cc(ARM_CC_CC, c0_after - c0_skip - 2);
                    *c0_match = b_cc(ARM_CC_CC, call_start - c0_match - 2);
                    *rom_direct = b_cc(ARM_CC_CS, direct_entry - rom_direct - 2);
                    load_complete = ptr;
                    *direct_fast = b_cc(ARM_CC_EQ, direct_fast_target - direct_fast - 2);
                    *direct_done = b_cc(ARM_CC_AL, direct_restore - direct_done - 2);
                    *direct_fast_done = b_cc(ARM_CC_AL, direct_restore - direct_fast_done - 2);
                    *direct_restore_done = b_cc(ARM_CC_AL, load_complete - direct_restore_done - 2);
                    *helper_restore_done = b_cc(ARM_CC_AL, load_complete - helper_restore_done - 2);
                    *skip_literal = b_cc(ARM_CC_AL, restore_start - skip_literal - 2);

                    RA_FreeARMRegister(&ptr, flags_reg);
                    RA_FreeARMRegister(&ptr, cmp_tmp);
                    RA_FreeARMRegister(&ptr, low_reg);
                }
#endif
                break;
            case 2:
#ifdef __aarch64__
                if (offset > -256 && offset < 256)
                    *ptr++ = ldurh_offset(base, reg, offset & 0x1ff);
                else if (offset >= 0 && offset < 8192 && (offset & 1) == 0)
                    *ptr++ = ldrh_offset(base, reg, offset);
                else {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                            }
                        }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                    *ptr++ = ldrh_offset(reg_d16, reg, 0);
                }
#else
                if (offset > -256 && offset < 256)
                    *ptr++ = ldrh_offset(base, reg, offset);
                else
                    *ptr++ = ldrh_regoffset(base, reg, reg_d16);
#endif
                break;
            case 1:
#ifdef __aarch64__
                if (offset > -256 && offset < 256)
                    *ptr++ = ldurb_offset(base, reg, offset);
                else if (offset >= 0 && offset < 4096)
                    *ptr++ = ldrb_offset(base, reg, offset);
                else {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                            }
                        }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                    *ptr++ = ldrb_offset(reg_d16, reg, 0);
                }
#else
                if (offset > -4096 && offset < 4096)
                    *ptr++ = ldrb_offset(base, reg, offset);
                else
                    *ptr++ = ldrb_regoffset(base, reg, reg_d16, 0);
#endif
                break;
            case 0:
#ifdef __aarch64__
                if (offset > -4096 && offset < 4096)
                {
                    if (offset < 0)
                        *ptr++ = sub_immed(reg, base, -offset);
                    else
                        *ptr++ = add_immed(reg, base, offset);
                }
                else
                {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                            }
                        }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg, base, reg_d16, LSL, 0);
                }
#else
                *ptr++ = add_reg(reg, base, reg_d16, 0);
#endif
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
            }
#ifndef __aarch64__
            if (size == 1 || size == 2 || size == 4)
            {
                hook_addr_reg = base;
                if (offset != 0)
                {
                    hook_addr_reg = reg_d16;
                    if (offset > -4096 && offset < 4096)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(reg_d16, base, offset);
                        else
                            *ptr++ = sub_immed(reg_d16, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(reg_d16, base, reg_d16, 0);
                    }
                }
                ptr = emit_special_load_hook(ptr, size, hook_addr_reg, reg);
            }
#endif
            RA_FreeARMRegister(&ptr, reg_d16);

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * load_reg_from_addr(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, uint8_t index, uint8_t shift)
{
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    if (index == 0xff)
    {
        switch (size)
        {
            case 4:
#ifdef __aarch64__
                *ptr++ = ldr_offset(base, reg, 0);
#else
                {
                    uint8_t low_reg = RA_AllocARMRegister(&ptr);

                    /* A 68k longword only requires even alignment. Match the
                     * offset-based ARM32 path so direct loads preserve the
                     * same low-memory/MMIO behavior in real ROM code. */
                    *ptr++ = ldrh_offset(base, reg, 0);
                    *ptr++ = ldrh_offset(base, low_reg, 2);
                    *ptr++ = lsl_immed(reg, reg, 16);
                    *ptr++ = orr_reg(reg, reg, low_reg, 0);

                    RA_FreeARMRegister(&ptr, low_reg);
                }
#endif
                break;
            case 2:
                *ptr++ = ldrh_offset(base, reg, 0);
                break;
            case 1:
                *ptr++ = ldrb_offset(base, reg, 0);
                break;
            case 0:
                *ptr++ = mov_reg(reg, base);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
#ifndef __aarch64__
        if (size == 1 || size == 2 || size == 4)
            ptr = emit_special_load_hook(ptr, size, base, reg);
#endif
    }
    else
    {
#ifdef __aarch64__
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        switch (size)
        {
            case 4:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = ldr_offset(tmp, reg, 0);
                break;
            case 2:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = ldrh_offset(tmp, reg, 0);
                break;
            case 1:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = ldrb_offset(tmp, reg, 0);
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, LSL, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
        RA_FreeARMRegister(&ptr, tmp);
#else
        switch (size)
        {
            case 4:
                *ptr++ = ldr_regoffset(base, reg, index, shift);
                break;
            case 2:
                {
                    uint8_t offset_reg = index;
                    uint8_t tmp2 = 0xff;

                    if (shift)
                    {
                        tmp2 = RA_AllocARMRegister(&ptr);
                        *ptr++ = lsl_immed(tmp2, index, shift);
                        offset_reg = tmp2;
                    }

                    *ptr++ = ldrh_regoffset(base, reg, offset_reg);

                    if (tmp2 != 0xff)
                        RA_FreeARMRegister(&ptr, tmp2);
                }
                break;
            case 1:
                *ptr++ = ldrb_regoffset(base, reg, index, shift);
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
#endif
    }

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * store_reg_to_addr_offset(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, int32_t offset, uint8_t offset_32bit)
{
    uint8_t reg_d16 = RA_AllocARMRegister(&ptr);
    uint8_t hook_addr_reg = 0xff;
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

#ifndef __aarch64__
    if (size == 0 ||
        (size == 2 && (offset < -255 || offset > 255)) ||
        (offset < -4095 || offset > 4095))
    {
        if (offset_32bit) {
            *ptr++ = movw_immed_u16(reg_d16, offset);
            if ((offset >> 16) & 0xffff) {
                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
            }
        } else {
            *ptr++ = movw_immed_u16(reg_d16, offset);
            if (offset & 0x8000)
                *ptr++ = movt_immed_u16(reg_d16, 0xffff);
        }
    }
#endif

    switch (size)
    {
        case 4:
#ifdef __aarch64__
            if (offset == 0)
                *ptr++ = str_offset(base, reg, 0);
            else if (offset > -256 && offset < 256)
                *ptr++ = stur_offset(base, reg, offset & 0x1ff);
            else if (offset > 0 && offset < 16384 && (offset & 3) == 0)
                *ptr++ = str_offset(base, reg, offset);
            else {
                if (offset_32bit) {
                    if (offset & 0xffff) {
                        *ptr++ = movw_immed_u16(reg_d16, offset);
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                        }
                    } else {
                        if ((offset >> 16) & 0xffff) {
                            *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                        } else {
                            *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                }
                else {
                    if (offset > 0)
                        *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                    else
                        *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                }
                *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                *ptr++ = str_offset(reg_d16, reg, 0);
            }
#else
                {
                    uint8_t addr_reg = reg_d16;
                    uint8_t high_reg = RA_AllocARMRegister(&ptr);
                    uint32_t *fast_store = NULL;
                    uint32_t *store_done = NULL;
                    uint32_t *fast_store_target = NULL;
                    uint32_t *fast_store_done = NULL;
                    uint32_t *store_complete = NULL;

                    if (offset == 0)
                        *ptr++ = mov_reg(addr_reg, base);
                    else if (offset > -256 && offset < 256)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(addr_reg, base, offset);
                        else
                            *ptr++ = sub_immed(addr_reg, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(addr_reg, base, reg_d16, 0);
                    }

                    *ptr++ = tst_immed(addr_reg, 2);
                    fast_store = ptr;
                    *ptr++ = b_cc(ARM_CC_EQ, 0);
                    *ptr++ = lsr_immed(high_reg, reg, 16);
                    *ptr++ = strh_offset(addr_reg, high_reg, 0);
                    *ptr++ = strh_offset(addr_reg, reg, 2);
                    store_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    fast_store_target = ptr;
                    *ptr++ = str_offset(addr_reg, reg, 0);
                    fast_store_done = ptr;
                    *ptr++ = b_cc(ARM_CC_AL, 0);
                    store_complete = ptr;
                    *fast_store = b_cc(ARM_CC_EQ, fast_store_target - fast_store - 2);
                    *store_done = b_cc(ARM_CC_AL, store_complete - store_done - 2);
                    *fast_store_done = b_cc(ARM_CC_AL, store_complete - fast_store_done - 2);

                    RA_FreeARMRegister(&ptr, high_reg);
                }
                hook_addr_reg = (offset == 0) ? base : reg_d16;
                if (offset != 0)
                {
                    if (offset > -256 && offset < 256)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(reg_d16, base, offset);
                        else
                            *ptr++ = sub_immed(reg_d16, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(reg_d16, base, reg_d16, 0);
                    }
                }
                ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
#endif
                break;
            case 2:
#ifdef __aarch64__
                if (offset > -256 && offset < 256)
                    *ptr++ = sturh_offset(base, reg, offset & 0x1ff);
                else if (offset >= 0 && offset < 8192 && (offset & 1) == 0)
                    *ptr++ = strh_offset(base, reg, offset);
                else {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                            }
                        }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                    *ptr++ = strh_offset(reg_d16, reg, 0);
                }
#else
                if (offset > -256 && offset < 256)
                    *ptr++ = strh_offset(base, reg, offset);
                else
                    *ptr++ = strh_regoffset(base, reg, reg_d16);
                hook_addr_reg = (offset == 0) ? base : reg_d16;
                if (offset != 0)
                {
                    if (offset > -4096 && offset < 4096)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(reg_d16, base, offset);
                        else
                            *ptr++ = sub_immed(reg_d16, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(reg_d16, base, reg_d16, 0);
                    }
                }
                ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
#endif
                break;
            case 1:
#ifdef __aarch64__
                if (offset > -256 && offset < 256)
                    *ptr++ = sturb_offset(base, reg, offset);
                else if (offset >= 0 && offset < 4096)
                    *ptr++ = strb_offset(base, reg, offset);
                else {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                        }
                    }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg_d16, base, reg_d16, LSL, 0);
                    *ptr++ = strb_offset(reg_d16, reg, 0);
                }
#else
                if (offset > -4096 && offset < 4096)
                    *ptr++ = strb_offset(base, reg, offset);
                else
                    *ptr++ = strb_regoffset(base, reg, reg_d16, 0);

                hook_addr_reg = (offset == 0) ? base : reg_d16;
                if (offset != 0)
                {
                    if (offset > -4096 && offset < 4096)
                    {
                        if (offset > 0)
                            *ptr++ = add_immed(reg_d16, base, offset);
                        else
                            *ptr++ = sub_immed(reg_d16, base, -offset);
                    }
                    else
                    {
                        *ptr++ = add_reg(reg_d16, base, reg_d16, 0);
                    }
                }
                ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
#endif
                break;
            case 0:
#ifdef __aarch64__
                if (offset > -4096 && offset < 4096)
                {
                    if (offset < 0)
                        *ptr++ = sub_immed(reg, base, -offset);
                    else
                        *ptr++ = add_immed(reg, base, offset);
                }
                else
                {
                    if (offset_32bit) {
                        if (offset & 0xffff) {
                            *ptr++ = movw_immed_u16(reg_d16, offset);
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = movt_immed_u16(reg_d16, (offset >> 16) & 0xffff);
                            }
                        } else {
                            if ((offset >> 16) & 0xffff) {
                                *ptr++ = mov_immed_u16(reg_d16, (offset >> 16) & 0xffff, 1);
                            } else {
                                *ptr++ = mov_reg(reg_d16, 31);
                            }
                        }
                    } else {
                        if (offset > 0)
                            *ptr++ = mov_immed_u16(reg_d16, offset, 0);
                        else
                            *ptr++ = movn_immed_u16(reg_d16, -offset - 1, 0);
                    }
                    *ptr++ = add_reg(reg, base, reg_d16, LSL, 0);
                }
#else
                *ptr++ = add_reg(reg, base, reg_d16, 0);
#endif
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
        RA_FreeARMRegister(&ptr, reg_d16);

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

static inline __attribute__((always_inline)) uint32_t * store_reg_to_addr(uint32_t *ptr, uint8_t size, uint8_t base, uint8_t reg, uint8_t index, uint8_t shift)
{
    int free_base = 0;

    if (base == 0xff)
    {
        free_base = 1;
        base = RA_AllocARMRegister(&ptr);
        *ptr++ = mov_reg(base, 31);
    }

    if (index == 0xff)
    {
        switch (size)
        {
            case 4:
#ifdef __aarch64__
                *ptr++ = str_offset(base, reg, 0);
#else
                {
                    uint8_t high_reg = RA_AllocARMRegister(&ptr);

                    /* Match the paired-halfword offset path for direct 68k
                     * longword stores on ARM32. */
                    *ptr++ = lsr_immed(high_reg, reg, 16);
                    *ptr++ = strh_offset(base, high_reg, 0);
                    *ptr++ = strh_offset(base, reg, 2);

                    RA_FreeARMRegister(&ptr, high_reg);
                }
                ptr = emit_special_store_hook(ptr, size, base, reg);
#endif
                break;
            case 2:
                *ptr++ = strh_offset(base, reg, 0);
#ifndef __aarch64__
                ptr = emit_special_store_hook(ptr, size, base, reg);
#endif
                break;
            case 1:
                *ptr++ = strb_offset(base, reg, 0);
#ifndef __aarch64__
                ptr = emit_special_store_hook(ptr, size, base, reg);
#endif
                break;
            case 0:
                *ptr++ = mov_reg(reg, base);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
    }
    else
    {
#ifdef __aarch64__
        uint8_t tmp = RA_AllocARMRegister(&ptr);
        switch (size)
        {
            case 4:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = str_offset(tmp, reg, 0);
                break;
            case 2:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = strh_offset(tmp, reg, 0);
                break;
            case 1:
                *ptr++ = add_reg(tmp, base, index, LSL, shift);
                *ptr++ = strb_offset(tmp, reg, 0);
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, LSL, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
        RA_FreeARMRegister(&ptr, tmp);
#else
        switch (size)
        {
            case 4:
                *ptr++ = str_regoffset(base, reg, index, shift);
#ifndef __aarch64__
                {
                    uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_reg(hook_addr_reg, base, index, shift);
                    ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
                    RA_FreeARMRegister(&ptr, hook_addr_reg);
                }
#endif
                break;
            case 2:
                {
                    uint8_t offset_reg = index;
                    uint8_t tmp2 = 0xff;

                    if (shift)
                    {
                        tmp2 = RA_AllocARMRegister(&ptr);
                        *ptr++ = lsl_immed(tmp2, index, shift);
                        offset_reg = tmp2;
                    }

                    *ptr++ = strh_regoffset(base, reg, offset_reg);

                    if (tmp2 != 0xff)
                        RA_FreeARMRegister(&ptr, tmp2);

#ifndef __aarch64__
                    {
                        uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = add_reg(hook_addr_reg, base, index, shift);
                        ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
                        RA_FreeARMRegister(&ptr, hook_addr_reg);
                    }
#endif
                }
                break;
            case 1:
                *ptr++ = strb_regoffset(base, reg, index, shift);
#ifndef __aarch64__
                {
                    uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_reg(hook_addr_reg, base, index, shift);
                    ptr = emit_special_store_hook(ptr, size, hook_addr_reg, reg);
                    RA_FreeARMRegister(&ptr, hook_addr_reg);
                }
#endif
                break;
            case 0:
                *ptr++ = add_reg(reg, base, index, shift);
                break;
            default:
                kprintf("Unknown size opcode\n");
                break;
        }
#endif
    }

    if (free_base)
        RA_FreeARMRegister(&ptr, base);

    return ptr;
}

#define M68K_EA_DA 0x8000
#define M68K_EA_REG 0x7000
#define M68K_EA_WL 0x0800
#define M68K_EA_SCALE 0x0600
#define M68K_EA_FULL 0x0100
#define M68K_EA_OFF8 0x00FF

#define M68K_EA_BS 0x0080
#define M68K_EA_IS 0x0040
#define M68K_EA_BD_SIZE 0x0030
#define M68K_EA_IIS 0x0007

/*
    Emits ARM insns to load effective address and read value from ther to specified register.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for load operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not load a value from EA into
                register but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_LoadFromEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, uint8_t read_only, int32_t *imm_offset)
{
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;

    if (imm_offset)
        *imm_offset = 0;

    if (mode == 0) /* Mode 000: Dn */
    {
        switch (size)
        {
            case 4:
            case 2:
            case 1:
                if (read_only)
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg);
                else
                    *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg);
                break;
            case 0:
#ifdef __aarch64__
                kprintf("Load form EA: Dn with wrong operand size! Opcode %04x at %08x\n", BE16(m68k_ptr[-*ext_words]), m68k_ptr - *ext_words);
#else
                *arm_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, D[src_reg]));
#endif
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        switch (size)
        {
            case 4:
            case 2:
                if (read_only)
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                else
                    *arm_reg = RA_CopyFromM68kRegister(&ptr, src_reg + 8);
                break;
            case 0:
#ifdef __aarch64__
                kprintf("Load form EA: An with wrong operand size! Opcode %04x at %08x\n", BE16(m68k_ptr[-*ext_words]), m68k_ptr - *ext_words);
                {
                    uint16_t *ptr = &m68k_ptr[-*ext_words] - 8;
                    for (int i=0; i < 16; i++)
                        kprintf("%04x ", ptr[i]);
                    kprintf("\n");
                }
#else
                *arm_reg = RA_AllocARMRegister(&ptr);
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, A[src_reg]));
#endif
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else
    {
        if (*arm_reg == 0xff)
            *arm_reg = RA_AllocARMRegister(&ptr);

        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0) {
                if (read_only) {
                    RA_FreeARMRegister(&ptr, *arm_reg);
                    *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                } else {
                    uint8_t tmp = RA_MapM68kRegister(&ptr, src_reg + 8);
                    *ptr++ = mov_reg(*arm_reg, tmp);
                }
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                switch (size)
                {
                    case 4:
#ifdef __aarch64__
                        *ptr++ = ldr_offset(reg_An, *arm_reg, 0);
#else
                        if (reg_An == *arm_reg)
                        {
                            uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                            uint8_t load_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(addr_reg, reg_An);
                            ptr = load_reg_from_addr_offset(ptr, size, addr_reg, load_reg, 0, 0);
                            *ptr++ = mov_reg(*arm_reg, load_reg);
                            RA_FreeARMRegister(&ptr, load_reg);
                            RA_FreeARMRegister(&ptr, addr_reg);
                        }
                        else
                        {
                            ptr = load_reg_from_addr_offset(ptr, size, reg_An, *arm_reg, 0, 0);
                        }
#endif
                        break;
                    case 2:
                        *ptr++ = ldrh_offset(reg_An, *arm_reg, 0);
                        break;
                    case 1:
                        *ptr++ = ldrb_offset(reg_An, *arm_reg, 0);
                        break;
                    default:
                        kprintf("Unknown size opcode\n");
                        break;
                }
#ifndef __aarch64__
                if (size == 1 || size == 2)
                    ptr = emit_special_load_hook(ptr, size, reg_An, *arm_reg);
#endif
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                /* Rare case where source and dest are the same register and size == 4 */
                if (size == 4 && reg_An == *arm_reg) {
#ifdef __aarch64__
                    *ptr++ = ldr_offset(reg_An, *arm_reg, 0);
#else
                    uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                    uint8_t load_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = mov_reg(addr_reg, reg_An);
                    *ptr++ = add_immed(reg_An, reg_An, 4);
                    ptr = load_reg_from_addr_offset(ptr, size, addr_reg, load_reg, 0, 0);
                    *ptr++ = mov_reg(*arm_reg, load_reg);
                    RA_FreeARMRegister(&ptr, load_reg);
                    RA_FreeARMRegister(&ptr, addr_reg);
#endif
                }
                else
                {
                    switch (size)
                    {
                        case 4:
#ifdef __aarch64__
                            *ptr++ = ldr_offset_postindex(reg_An, *arm_reg, 4);
#else
                        {
                            uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(addr_reg, reg_An);
                            *ptr++ = add_immed(reg_An, reg_An, 4);
                            ptr = load_reg_from_addr_offset(ptr, size, addr_reg, *arm_reg, 0, 0);
                            RA_FreeARMRegister(&ptr, addr_reg);
                        }
#endif
                            break;
                        case 2:
                            *ptr++ = ldrh_offset_postindex(reg_An, *arm_reg, 2);
                            break;
                        case 1:
                            if (src_reg == 7)
                                *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 2);
                            else
                                *ptr++ = ldrb_offset_postindex(reg_An, *arm_reg, 1);
                            break;
                        default:
                            kprintf("Unknown size opcode\n");
                            break;
                    }
#ifndef __aarch64__
                    if (size == 1 || size == 2)
                    {
                        uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = sub_immed(hook_addr_reg, reg_An, size == 4 ? 4 : (size == 2 ? 2 : (src_reg == 7 ? 2 : 1)));
                        ptr = emit_special_load_hook(ptr, size, hook_addr_reg, *arm_reg);
                        RA_FreeARMRegister(&ptr, hook_addr_reg);
                    }
#endif
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                /* Rare case where source and dest are the same register and size == 4 */
                if (size == 4 && reg_An == *arm_reg) {
#ifdef __aarch64__
                    *ptr++ = ldur_offset(reg_An, *arm_reg, -4);
#else
                    uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                    uint8_t load_reg = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed(addr_reg, reg_An, 4);
                    *ptr++ = mov_reg(reg_An, addr_reg);
                    ptr = load_reg_from_addr_offset(ptr, size, addr_reg, load_reg, 0, 0);
                    *ptr++ = mov_reg(*arm_reg, load_reg);
                    RA_FreeARMRegister(&ptr, load_reg);
                    RA_FreeARMRegister(&ptr, addr_reg);
#endif
                }
                else
                {
                    switch (size)
                    {
                        case 4:
#ifdef __aarch64__
                            *ptr++ = ldr_offset_preindex(reg_An, *arm_reg, -4);
#else
                        {
                            uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = sub_immed(addr_reg, reg_An, 4);
                            *ptr++ = mov_reg(reg_An, addr_reg);
                            ptr = load_reg_from_addr_offset(ptr, size, addr_reg, *arm_reg, 0, 0);
                            RA_FreeARMRegister(&ptr, addr_reg);
                        }
#endif
                            break;
                        case 2:
                            *ptr++ = ldrh_offset_preindex(reg_An, *arm_reg, -2);
                            break;
                        case 1:
                            if (src_reg == 7)
                                *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -2);
                            else
                                *ptr++ = ldrb_offset_preindex(reg_An, *arm_reg, -1);
                            break;
                        default:
                            kprintf("Unknown size opcode\n");
                            break;
                    }
#ifndef __aarch64__
                    if (size == 1 || size == 2)
                        ptr = emit_special_load_hook(ptr, size, reg_An, *arm_reg);
#endif
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            if (imm_offset && size == 0 && read_only)
            {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
                *imm_offset = (int16_t)BE16(m68k_ptr[(*ext_words)++]);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                int16_t off16 = (int16_t)BE16(m68k_ptr[(*ext_words)++]);

                ptr = load_reg_from_addr_offset(ptr, size, reg_An, *arm_reg, off16, 0);
            }
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = BE16(m68k_ptr[(*ext_words)++]);
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = 0xff;
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else if (displ < 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }
                else
                {
                    //*ptr++ = mov_reg(tmp1, reg_An);
                }


                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);
                    
                    uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    *ptr++ = sxth(tmp3, tmp2);
#else
                    *ptr++ = sxth(tmp3, tmp2, 0);
#endif
                    RA_FreeARMRegister(&ptr, tmp2);
                    tmp2 = tmp3;
                }

                ptr = load_reg_from_addr(ptr, size, displ ? tmp1 : reg_An, *arm_reg, tmp2, (brief >> 9) & 3);

                if (displ)
                    RA_FreeARMRegister(&ptr, tmp1);

                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                        *ptr++ = sxth(tmp3, index_reg);
#else
                        *ptr++ = sxth(tmp3, index_reg, 0);
#endif
                        RA_FreeARMRegister(&ptr, index_reg);
                        index_reg = tmp3;
                    }
                }

                uint16_t lo16, hi16;

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                    case 2: /* Word displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        lo16 = BE16(m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, bd_reg, lo16);
                        break;
                    case 3: /* Long displacement */
                        bd_reg = RA_AllocARMRegister(&ptr);
                        hi16 = BE16(m68k_ptr[(*ext_words)++]);
                        lo16 = BE16(m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(bd_reg, lo16);
                        if (hi16 != 0)
                            *ptr++ = movt_immed_u16(bd_reg, hi16);
                        break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                    case 2: /* Word outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        lo16 = BE16(m68k_ptr[(*ext_words)++]);
                        ptr = load_s16_ext32(ptr, outer_reg, lo16);
                        break;
                    case 3: /* Long outer displacement */
                        outer_reg = RA_AllocARMRegister(&ptr);
                        hi16 = BE16(m68k_ptr[(*ext_words)++]);
                        lo16 = BE16(m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(outer_reg, lo16);
                        if (hi16 != 0)
                            *ptr++ = movt_immed_u16(outer_reg, hi16);
                        break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
#ifdef __aarch64__
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */
                    ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
#ifdef __aarch64__
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);
#else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, 0);
#endif
                        if (outer_reg != 0xff)
#ifdef __aarch64__
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif

                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff) {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
                                ptr = load_reg_from_addr(ptr, 4, base_reg, bd_reg, bd_reg, 0);
                        }
                        else
                        {
#ifdef __aarch64__
                            if (bd_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                            if (bd_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                            ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                        }

                        if (outer_reg != 0xff)
#ifdef __aarch64__
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif
                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0);
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 2) /* (d16, PC) mode */
            {
                if (imm_offset && size == 0 && read_only)
                {
                    int8_t off8 = 2 + 2*(*ext_words);
                    ptr = EMIT_GetOffsetPC(ptr, &off8);
                    RA_FreeARMRegister(&ptr, *arm_reg);
                    *arm_reg = REG_PC;
                    *imm_offset = off8 + (int16_t)BE16(m68k_ptr[(*ext_words)++]);
                }
                else
                {
                    int8_t off8 = 2 + 2*(*ext_words);
                    ptr = EMIT_GetOffsetPC(ptr, &off8);
                    int32_t off = off8 + (int16_t)(BE16(m68k_ptr[(*ext_words)++]));

                    ptr = load_reg_from_addr_offset(ptr, size, REG_PC, *arm_reg, off, 1);
                }
            }
            else if (src_reg == 3)
            {
                uint16_t brief = BE16(m68k_ptr[(*ext_words)++]);
                uint8_t extra_reg = (brief >> 12) & 7;

                if ((brief & 0x0100) == 0)
                {
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = 0xff;
                    int8_t displ = brief & 0xff;
                    int8_t off = 2 + 2*(*ext_words - 1);
                    int16_t full_off = 0;
                    ptr = EMIT_GetOffsetPC(ptr, &off);

                    full_off = off + displ;

                    if (full_off >= 0)
                    {
                        *ptr++ = add_immed(tmp1, REG_PC, full_off);
                    }
                    else
                    {
                        *ptr++ = sub_immed(tmp1, REG_PC, -full_off);
                    }

                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                        *ptr++ = sxth(tmp3, tmp2);
#else
                        *ptr++ = sxth(tmp3, tmp2, 0);
#endif
                        RA_FreeARMRegister(&ptr, tmp2);
                        tmp2 = tmp3;
                    }

                    ptr = load_reg_from_addr(ptr, size, tmp1, *arm_reg, tmp2, (brief >> 9) & 3);

                    RA_FreeARMRegister(&ptr, tmp1);
                    RA_FreeARMRegister(&ptr, tmp2);
                }
                else
                {
                    uint8_t bd_reg = 0xff;
                    uint8_t outer_reg = 0xff;
                    uint8_t base_reg = 0xff;
                    uint8_t index_reg = 0xff;

                    /* Check if base register is suppressed */
                    if (!(brief & M68K_EA_BS))
                    {
                        /* Base register in use. Alloc it and load its contents */
                        base_reg = RA_AllocARMRegister(&ptr);
                        int8_t off = 2 + 2*(*ext_words - 1);
                        //ptr = EMIT_FlushPC(ptr);
                        ptr = EMIT_GetOffsetPC(ptr, &off);
                        if (off > 0)
                            *ptr++ = add_immed(base_reg, REG_PC, off);
                        else
                            *ptr++ = sub_immed(base_reg, REG_PC, -off);
                    }

                    /* Check if index register is in use */
                    if (!(brief & M68K_EA_IS))
                    {
                        /* Index register in use. Alloc it and load its contents */
                        if (brief & (1 << 11))
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                        }
                        else
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                            uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                            *ptr++ = sxth(tmp3, index_reg);
#else
                            *ptr++ = sxth(tmp3, index_reg, 0);
#endif
                            RA_FreeARMRegister(&ptr, index_reg);
                            index_reg = tmp3;
                        }
                    }

                    int8_t pc_off = 2 + (*ext_words) * 2;
                    ptr = EMIT_GetOffsetPC(ptr, &pc_off);
                    /* Check if base displacement needs to be fetched */
                    switch ((brief & M68K_EA_BD_SIZE) >> 4)
                    {
                        case 2: /* Word displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = ldrsh_offset(REG_PC, bd_reg, pc_off);
                            (*ext_words)++;
                            break;
                        case 3: /* Long displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            if (pc_off & 2) {
#ifdef __aarch64__
                                *ptr++ = ldur_offset(REG_PC, bd_reg, pc_off);
#else
                                *ptr++ = ldr_offset(REG_PC, bd_reg, pc_off);
#endif
                            } else {
                                *ptr++ = ldr_offset(REG_PC, bd_reg, pc_off);
                            }
                            (*ext_words) += 2;
                            break;
                    }

                    pc_off = 2 + (*ext_words) * 2;
                    ptr = EMIT_GetOffsetPC(ptr, &pc_off);

                    /* Check if outer displacement needs to be fetched */
                    switch ((brief & M68K_EA_IIS) & 3)
                    {
                        case 2: /* Word outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = ldrsh_offset(REG_PC, outer_reg, pc_off);
                            (*ext_words)++;
                            break;
                        case 3: /* Long outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            if (pc_off & 2) {
#ifdef __aarch64__
                                *ptr++ = ldur_offset(REG_PC, outer_reg, pc_off);
#else
                                *ptr++ = ldr_offset(REG_PC, outer_reg, pc_off);
#endif
                            }
                            else
                                *ptr++ = ldr_offset(REG_PC, outer_reg, pc_off);
                            (*ext_words) += 2;
                            break;
                    }

                    if ((brief & 0x0f) == 0)
                    {
                        /* Address register indirect with index mode */
                        if (base_reg != 0xff && bd_reg != 0xff)
                        {
#ifdef __aarch64__
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                        }
                        else if (bd_reg == 0xff && base_reg != 0xff)
                        {
                            bd_reg = base_reg;
                        }
                        /*
                            Now, either base register or base displacement were given, if
                            index register was specified, use it.
                        */
                        ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else
                    {
                        if (bd_reg == 0xff)
                        {
                            bd_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(bd_reg, base_reg);
                            RA_FreeARMRegister(&ptr, base_reg);
                            base_reg = 0xff;
                        }

                        /* Postindexed mode */
                        if (brief & 0x04)
                        {
                            /* Fetch data from base reg */
                            if (base_reg == 0xff)
                                *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                            else
#ifdef __aarch64__
                                *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);
                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                                *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, 0);
                            if (outer_reg != 0xff)
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif
                            ptr = load_reg_from_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                        }
                        else /* Preindexed mode */
                        {
                            /* Fetch data from base reg with eventually applied index */
                            if (brief & M68K_EA_IS)
                            {
                                if (bd_reg == 0xff) {
                                    bd_reg = RA_AllocARMRegister(&ptr);
                                    *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                                }
                                else
#ifdef __aarch64__
                                {
                                    if (base_reg == 0xff) {
                                        uint8_t t = RA_AllocARMRegister(&ptr);
                                        *ptr++ = mov_reg(t, 31);
                                        *ptr++ = ldr_regoffset(t, bd_reg, bd_reg, UXTW, 0);
                                        RA_FreeARMRegister(&ptr, t);
                                    }
                                    else {
                                        *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, UXTW, 0);
                                    }
                                }
                                    
#else
                                    *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, 0);
#endif
                            }
                            else
                            {
#ifdef __aarch64__
                                if (bd_reg != 0xff)
                                    *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                                if (bd_reg != 0xff)
                                    *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);

#endif
                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                            }

                            if (outer_reg != 0xff)
#ifdef __aarch64__
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif
                            ptr = load_reg_from_addr_offset(ptr, size, bd_reg, *arm_reg, 0, 0);
                        }
                    }

                    if (bd_reg != 0xff)
                        RA_FreeARMRegister(&ptr, bd_reg);
                    if (outer_reg != 0xff)
                        RA_FreeARMRegister(&ptr, outer_reg);
                    if (base_reg != 0xff)
                        RA_FreeARMRegister(&ptr, base_reg);
                    if (index_reg != 0xff)
                        RA_FreeARMRegister(&ptr, index_reg);
                }
            }
            else if (src_reg == 0)
            {
                uint16_t lo16;
                lo16 = BE16(m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    ptr = load_s16_ext32(ptr, *arm_reg, lo16);
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    ptr = load_s16_ext32(ptr, tmp_reg, lo16);
                    ptr = load_reg_from_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                uint16_t hi16, lo16;
                hi16 = BE16(m68k_ptr[(*ext_words)++]);
                lo16 = BE16(m68k_ptr[(*ext_words)++]);

                if (size == 0) {
#ifdef __aarch64__
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(*arm_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(*arm_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(*arm_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                    }
#else
                    *ptr++ = movw_immed_u16(*arm_reg, lo16);
                    if (hi16 != 0 || lo16 & 0x8000)
                        *ptr++ = movt_immed_u16(*arm_reg, hi16);
#endif
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(tmp_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(tmp_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(tmp_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(tmp_reg, hi16, 1);
                    }
#else
                    *ptr++ = movw_immed_u16(tmp_reg, lo16);
                    if (hi16 != 0 || lo16 & 0x8000)
                        *ptr++ = movt_immed_u16(tmp_reg, hi16);
#endif
                    ptr = load_reg_from_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 4)
            {
                int8_t pc_off;
                uint16_t lo16, hi16, off;
                switch (size)
                {
                    case 4:
                        hi16 = BE16(m68k_ptr[(*ext_words)++]);
                        lo16 = BE16(m68k_ptr[(*ext_words)++]);
#ifdef __aarch64__
                        if (lo16 == 0 && hi16 == 0)
                        {
                            *ptr++ = mov_reg(*arm_reg, 31);
                        }
                        else if (lo16 != 0)
                        {
                            *ptr++ = movw_immed_u16(*arm_reg, lo16);
                            if (hi16 != 0 || lo16 & 0x8000)
                                *ptr++ = movt_immed_u16(*arm_reg, hi16);
                        }
                        else
                        {
                            *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                        }
#else
                        *ptr++ = movw_immed_u16(*arm_reg, lo16);
                        if (hi16 != 0)
                            *ptr++ = movt_immed_u16(*arm_reg, hi16);
#endif
                        break;
                    case 2:
                        off = BE16(m68k_ptr[(*ext_words)++]);
                        *ptr++ = movw_immed_u16(*arm_reg, off);
                        break;
                    case 1:
                        off = BE16(m68k_ptr[(*ext_words)++]);
                        *ptr++ = mov_immed_u8(*arm_reg, off);
                        break;
                    case 0:
                        pc_off = 2 + 2*(*ext_words);
                        ptr = EMIT_GetOffsetPC(ptr, &pc_off);
                        *ptr++ = add_immed(*arm_reg, REG_PC, pc_off);
                        break;
                }
            }
        }
    }

    return ptr;
}

/*
    Emits ARM insns to load effective address and store value from specified register to the EA.

    Inputs:
        ptr     pointer to ARM instruction stream
        size    size of data for store operation, can be 4 (long), 2 (short) or 1 (byte).
                If size of 0 is specified the function does not store a value from register into
                EA but rather loads the EA into that register.
                If postincrement or predecrement modes are used and size 0 is specified, then
                the instruction translator is reponsible for increasing/decreasing the address
                register, otherwise it is done in this function!
        arm_reg ARM register to store the EA or value from EA into
        ea      EA encoded field.
        m68k_ptr pointer to m68k instruction stream past the instruction opcode itself. It may
                be increased if further bytes from m68k side are read

    Output:
        ptr     pointer to ARM instruction stream after the newly generated code
*/
uint32_t *EMIT_StoreToEffectiveAddress(uint32_t *ptr, uint8_t size, uint8_t *arm_reg, uint8_t ea, uint16_t *m68k_ptr, uint8_t *ext_words, int sign_extend)
{
    (void)sign_extend;
    uint8_t mode = ea >> 3;
    uint8_t src_reg = ea & 7;
    (void)ext_words;
    (void)m68k_ptr;
    if (size == 0)
        *arm_reg = RA_AllocARMRegister(&ptr);

    if (mode == 0) /* Mode 000: Dn */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
#ifdef __aarch64__
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
#else
                if (RA_IsARMRegisterMapped(*arm_reg)) {
                    reg_dest = RA_MapM68kRegisterForWrite(&ptr, src_reg);
                    *ptr++ = mov_reg(reg_dest, *arm_reg);
                }
                else
                {
                    RA_AssignM68kRegister(&ptr, src_reg, *arm_reg);
                }
#endif
                break;
            case 2:
                reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg);
                *ptr++ = bfi(reg_dest, *arm_reg, 0, 16);
                break;
            case 1:
                reg_dest = RA_MapM68kRegister(&ptr, src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg);
                *ptr++ = bfi(reg_dest, *arm_reg, 0, 8);
                break;
            case 0:
#ifdef __aarch64__
                kprintf("Store to EA with wrong operand size 0\n");
#else
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, D[src_reg]));
#endif
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else if (mode == 1) /* Mode 001: An */
    {
        uint8_t reg_dest;
        switch (size)
        {
            case 4:
#ifdef __aarch64__
                reg_dest = RA_MapM68kRegisterForWrite(&ptr, 8 + src_reg);
                *ptr++ = mov_reg(reg_dest, *arm_reg);
#else
                if (RA_IsARMRegisterMapped(*arm_reg)) {
                    reg_dest = RA_MapM68kRegisterForWrite(&ptr, 8 + src_reg);
                    *ptr++ = mov_reg(reg_dest, *arm_reg);
                }
                else
                {
                    RA_AssignM68kRegister(&ptr, 8 + src_reg, *arm_reg);
                }
#endif
                break;
            case 2:
                reg_dest = RA_MapM68kRegister(&ptr, 8 + src_reg);
                RA_SetDirtyM68kRegister(&ptr, src_reg + 8);
                *ptr++ = bfi(reg_dest, *arm_reg, 0, 16);
                break;
                ;
            case 0:
#ifdef __aarch64__
                kprintf("Store to EA with wrong operand size 0\n");
#else
                *ptr++ = add_immed(*arm_reg, REG_CTX, __builtin_offsetof(struct M68KState, A[src_reg]));
#endif
                break;
            default:
                kprintf("Wrong size\n");
                break;
        }
    }
    else
    {
        if (mode == 2) /* Mode 002: (An) */
        {
            if (size == 0) {
                uint8_t tmp = RA_MapM68kRegister(&ptr, src_reg + 8);
                *ptr++ = mov_reg(*arm_reg, tmp);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                ptr = store_reg_to_addr_offset(ptr, size, reg_An, *arm_reg, 0, 0);
            }
        }
        else if (mode == 3) /* Mode 003: (An)+ */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
#ifndef __aarch64__
                    {
                        uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                        uint8_t data_reg = *arm_reg;

                        *ptr++ = mov_reg(addr_reg, reg_An);
                        if (reg_An == *arm_reg)
                        {
                            data_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(data_reg, *arm_reg);
                        }

                        *ptr++ = add_immed(reg_An, reg_An, 4);
                        ptr = store_reg_to_addr_offset(ptr, size, addr_reg, data_reg, 0, 0);

                        if (data_reg != *arm_reg)
                            RA_FreeARMRegister(&ptr, data_reg);
                        RA_FreeARMRegister(&ptr, addr_reg);
                    }
#else
                    *ptr++ = str_offset_postindex(reg_An, *arm_reg, 4);
#endif
                    break;
                case 2:
                    *ptr++ = strh_offset_postindex(reg_An, *arm_reg, 2);
#ifndef __aarch64__
                    {
                        uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = sub_immed(hook_addr_reg, reg_An, 2);
                        ptr = emit_special_store_hook(ptr, size, hook_addr_reg, *arm_reg);
                        RA_FreeARMRegister(&ptr, hook_addr_reg);
                    }
#endif
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 2);
                    else
                        *ptr++ = strb_offset_postindex(reg_An, *arm_reg, 1);
#ifndef __aarch64__
                    {
                        uint8_t hook_addr_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = sub_immed(hook_addr_reg, reg_An, src_reg == 7 ? 2 : 1);
                        ptr = emit_special_store_hook(ptr, size, hook_addr_reg, *arm_reg);
                        RA_FreeARMRegister(&ptr, hook_addr_reg);
                    }
#endif
                    break;
                default:
                    kprintf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 4) /* Mode 004: -(An) */
        {
            if (size == 0) {
                RA_FreeARMRegister(&ptr, *arm_reg);
                *arm_reg = RA_MapM68kRegister(&ptr, src_reg + 8);
            }
            else
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);

                RA_SetDirtyM68kRegister(&ptr, 8 + src_reg);

                switch (size)
                {
                case 4:
#ifndef __aarch64__
                    {
                        uint8_t addr_reg = RA_AllocARMRegister(&ptr);
                        uint8_t data_reg = *arm_reg;

                        *ptr++ = sub_immed(addr_reg, reg_An, 4);
                        if (reg_An == *arm_reg)
                        {
                            data_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(data_reg, *arm_reg);
                        }

                        *ptr++ = mov_reg(reg_An, addr_reg);
                        ptr = store_reg_to_addr_offset(ptr, size, addr_reg, data_reg, 0, 0);

                        if (data_reg != *arm_reg)
                            RA_FreeARMRegister(&ptr, data_reg);
                        RA_FreeARMRegister(&ptr, addr_reg);
                    }
#else
                    *ptr++ = str_offset_preindex(reg_An, *arm_reg, -4);
#endif
                    break;
                case 2:
                    *ptr++ = strh_offset_preindex(reg_An, *arm_reg, -2);
#ifndef __aarch64__
                    ptr = emit_special_store_hook(ptr, size, reg_An, *arm_reg);
#endif
                    break;
                case 1:
                    if (src_reg == 7)
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -2);
                    else
                        *ptr++ = strb_offset_preindex(reg_An, *arm_reg, -1);
#ifndef __aarch64__
                    ptr = emit_special_store_hook(ptr, size, reg_An, *arm_reg);
#endif
                    break;
                default:
                    kprintf("Unknown size opcode\n");
                    break;
                }
            }
        }
        else if (mode == 5) /* Mode 005: (d16, An) */
        {
            uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
            int16_t off16 = (int16_t)BE16(m68k_ptr[(*ext_words)++]);

            ptr = store_reg_to_addr_offset(ptr, size, reg_An, *arm_reg, off16, 0);
        }
        else if (mode == 6) /* Mode 006: (d8, An, Xn.SIZE*SCALE) */
        {
            uint16_t brief = BE16(m68k_ptr[(*ext_words)++]);
            uint8_t extra_reg = (brief >> 12) & 7;

            if ((brief & 0x0100) == 0)
            {
                uint8_t reg_An = RA_MapM68kRegister(&ptr, src_reg + 8);
                uint8_t tmp1 = 0xff;
                uint8_t tmp2 = 0xff;
                int8_t displ = brief & 0xff;

                if (displ > 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = add_immed(tmp1, reg_An, displ);
                }
                else if (displ < 0)
                {
                    tmp1 = RA_AllocARMRegister(&ptr);
                    *ptr++ = sub_immed(tmp1, reg_An, -displ);
                }

                if (brief & (1 << 11))
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                }
                else
                {
                    if (brief & 0x8000)
                        tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                    else
                        tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                    uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                    *ptr++ = sxth(tmp3, tmp2);
#else
                    *ptr++ = sxth(tmp3, tmp2, 0);
#endif
                    RA_FreeARMRegister(&ptr, tmp2);
                    tmp2 = tmp3;
                }

                ptr = store_reg_to_addr(ptr, size, displ ? tmp1 : reg_An, *arm_reg, tmp2, (brief >> 9) & 3);

                if (displ) RA_FreeARMRegister(&ptr, tmp1);
                RA_FreeARMRegister(&ptr, tmp2);
            }
            else
            {
                uint8_t bd_reg = 0xff;
                uint8_t outer_reg = 0xff;
                uint8_t base_reg = 0xff;
                uint8_t index_reg = 0xff;

                /* Check if base register is suppressed */
                if (!(brief & M68K_EA_BS))
                {
                    /* Base register in use. Alloc it and load its contents */
                    base_reg = RA_MapM68kRegister(&ptr, 8 + src_reg);
                }

                /* Check if index register is in use */
                if (!(brief & M68K_EA_IS))
                {
                    /* Index register in use. Alloc it and load its contents */
                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                        *ptr++ = sxth(tmp3, index_reg);
#else
                        *ptr++ = sxth(tmp3, index_reg, 0);
#endif
                        RA_FreeARMRegister(&ptr, index_reg);
                        index_reg = tmp3;
                    }
                }
                uint16_t lo16, hi16;

                /* Check if base displacement needs to be fetched */
                switch ((brief & M68K_EA_BD_SIZE) >> 4)
                {
                case 2: /* Word displacement */
                    bd_reg = RA_AllocARMRegister(&ptr);
                    lo16 = BE16(m68k_ptr[(*ext_words)++]);
                    ptr = load_s16_ext32(ptr, bd_reg, lo16);
                    break;
                case 3: /* Long displacement */
                    bd_reg = RA_AllocARMRegister(&ptr);
                    hi16 = BE16(m68k_ptr[(*ext_words)++]);
                    lo16 = BE16(m68k_ptr[(*ext_words)++]);
                    *ptr++ = movw_immed_u16(bd_reg, lo16);
                    if (hi16)
                        *ptr++ = movt_immed_u16(bd_reg, hi16);
                    break;
                }

                /* Check if outer displacement needs to be fetched */
                switch ((brief & M68K_EA_IIS) & 3)
                {
                case 2: /* Word outer displacement */
                    outer_reg = RA_AllocARMRegister(&ptr);
                    lo16 = BE16(m68k_ptr[(*ext_words)++]);
                    ptr = load_s16_ext32(ptr, outer_reg, lo16);
                    break;
                case 3: /* Long outer displacement */
                    outer_reg = RA_AllocARMRegister(&ptr);
                    hi16 = BE16(m68k_ptr[(*ext_words)++]);
                    lo16 = BE16(m68k_ptr[(*ext_words)++]);
                    *ptr++ = movw_immed_u16(outer_reg, lo16);
                    if (hi16)
                        *ptr++ = movt_immed_u16(outer_reg, hi16);
                    break;
                }

                if ((brief & 0x0f) == 0)
                {
                    /* Address register indirect with index mode */
                    if (base_reg != 0xff && bd_reg != 0xff)
                    {
#ifdef __aarch64__
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                        *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                    }
                    else if (bd_reg == 0xff && base_reg != 0xff)
                    {
                        bd_reg = base_reg;
                    }
                    /*
                        Now, either base register or base displacement were given, if
                        index register was specified, use it.
                    */

                    ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                }
                else
                {
                    if (bd_reg == 0xff)
                    {
                        bd_reg = RA_AllocARMRegister(&ptr);
                        *ptr++ = mov_reg(bd_reg, base_reg);
                        base_reg = 0xff;
                    }

                    /* Postindexed mode */
                    if (brief & 0x04)
                    {
                        /* Fetch data from base reg */
                        if (base_reg == 0xff)
                            *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                        else
#ifdef __aarch64__
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, UXTW, 0);
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                            *ptr++ = ldr_regoffset(bd_reg, bd_reg, base_reg, 0);
                        if (outer_reg != 0xff)
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif
                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else /* Preindexed mode */
                    {
                        /* Fetch data from base reg with eventually applied index */
                        if (brief & M68K_EA_IS)
                        {
                            if (bd_reg == 0xff)
                            {
                                bd_reg = RA_AllocARMRegister(&ptr);
                                *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                            }
                            else
#ifdef __aarch64__
                            {
                                if (base_reg == 0xff) {
                                    uint8_t t = RA_AllocARMRegister(&ptr);
                                    *ptr++ = mov_reg(t, 31);
                                    *ptr++ = ldr_regoffset(t, bd_reg, bd_reg, UXTW, 0);
                                    RA_FreeARMRegister(&ptr, t);
                                }
                                else
                                    *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, UXTW, 0);
                            }
#else
                                *ptr++ = ldr_regoffset(base_reg, bd_reg, bd_reg, 0);
#endif
                        }
                        else
                        {
                            if (bd_reg != 0xff)
#ifdef __aarch64__
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                                *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                            ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                        }

                        if (outer_reg != 0xff)
#ifdef __aarch64__
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                            *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif

                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0);
                    }
                }

                if (bd_reg != 0xff)
                    RA_FreeARMRegister(&ptr, bd_reg);
                if (outer_reg != 0xff)
                    RA_FreeARMRegister(&ptr, outer_reg);
                if (base_reg != 0xff)
                    RA_FreeARMRegister(&ptr, base_reg);
                if (index_reg != 0xff)
                    RA_FreeARMRegister(&ptr, index_reg);
            }
        }
        else if (mode == 7)
        {
            if (src_reg == 2) /* (d16, PC) mode */
            {
                int8_t off = 2;
                int32_t off32 = (int16_t)BE16(m68k_ptr[(*ext_words)++]);
                ptr = EMIT_GetOffsetPC(ptr, &off);
                off32 += off;

                ptr = store_reg_to_addr_offset(ptr, size, REG_PC, *arm_reg, off32, 1);
            }
            else if (src_reg == 3)
            {
                uint16_t brief = BE16(m68k_ptr[(*ext_words)++]);
                uint8_t extra_reg = (brief >> 12) & 7;

                if ((brief & 0x0100) == 0)
                {
                    uint8_t tmp1 = RA_AllocARMRegister(&ptr);
                    uint8_t tmp2 = 0xff;
                    int8_t displ = brief & 0xff;
                    int8_t off = 2;
                    int16_t full_off = 0;
                    ptr = EMIT_GetOffsetPC(ptr, &off);

                    full_off = off + displ;

                    if (full_off >= 0)
                    {
                        *ptr++ = add_immed(tmp1, REG_PC, full_off);
                    }
                    else
                    {
                        *ptr++ = sub_immed(tmp1, REG_PC, -full_off);
                    }

                    if (brief & (1 << 11))
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                    }
                    else
                    {
                        if (brief & 0x8000)
                            tmp2 = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                        else
                            tmp2 = RA_MapM68kRegister(&ptr, extra_reg);

                        uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                        *ptr++ = sxth(tmp3, tmp2);
#else
                        *ptr++ = sxth(tmp3, tmp2, 0);
#endif
                        RA_FreeARMRegister(&ptr, tmp2);
                        tmp2 = tmp3;
                    }

                    ptr = store_reg_to_addr(ptr, size, tmp1, *arm_reg, tmp2, (brief >> 9) & 3);

                    RA_FreeARMRegister(&ptr, tmp1);
                    RA_FreeARMRegister(&ptr, tmp2);
                }
                else
                {
                    uint8_t bd_reg = 0xff;
                    uint8_t outer_reg = 0xff;
                    uint8_t base_reg = 0xff;
                    uint8_t index_reg = 0xff;

                    /* Check if base register is suppressed */
                    if (!(brief & M68K_EA_BS))
                    {
                        /* Base register in use. Alloc it and load its contents */
                        base_reg = RA_AllocARMRegister(&ptr);
                        int8_t off = 2;
                        //ptr = EMIT_FlushPC(ptr);
                        ptr = EMIT_GetOffsetPC(ptr, &off);
                        if (off > 0)
                            *ptr++ = add_immed(base_reg, REG_PC, off);
                        else
                            *ptr++ = sub_immed(base_reg, REG_PC, -off);

                        //*ptr++ = add_immed(base_reg, REG_PC, 2);
                    }

                    /* Check if index register is in use */
                    if (!(brief & M68K_EA_IS))
                    {
                        /* Index register in use. Alloc it and load its contents */
                        if (brief & (1 << 11))
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg); // RA_CopyFromM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg); // RA_CopyFromM68kRegister(&ptr, extra_reg);
                        }
                        else
                        {
                            if (brief & 0x8000)
                                index_reg = RA_MapM68kRegister(&ptr, 8 + extra_reg);
                            else
                                index_reg = RA_MapM68kRegister(&ptr, extra_reg);

                            uint8_t tmp3 = RA_AllocARMRegister(&ptr);
#ifdef __aarch64__
                            *ptr++ = sxth(tmp3, index_reg);
#else
                            *ptr++ = sxth(tmp3, index_reg, 0);
#endif
                            RA_FreeARMRegister(&ptr, index_reg);
                            index_reg = tmp3;
                        }
                    }

                    uint16_t lo16, hi16;

                    /* Check if base displacement needs to be fetched */
                    switch ((brief & M68K_EA_BD_SIZE) >> 4)
                    {
                        case 2: /* Word displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            lo16 = BE16(m68k_ptr[(*ext_words)++]);
                            ptr = load_s16_ext32(ptr, bd_reg, lo16);
                            break;
                        case 3: /* Long displacement */
                            bd_reg = RA_AllocARMRegister(&ptr);
                            hi16 = BE16(m68k_ptr[(*ext_words)++]);
                            lo16 = BE16(m68k_ptr[(*ext_words)++]);
                            *ptr++ = movw_immed_u16(bd_reg, lo16);
                            if (hi16)
                                *ptr++ = movt_immed_u16(bd_reg, hi16);
                            break;
                    }

                    /* Check if outer displacement needs to be fetched */
                    switch ((brief & M68K_EA_IIS) & 3)
                    {
                        case 2: /* Word outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            lo16 = BE16(m68k_ptr[(*ext_words)++]);
                            ptr = load_s16_ext32(ptr, outer_reg, lo16);
                            break;
                        case 3: /* Long outer displacement */
                            outer_reg = RA_AllocARMRegister(&ptr);
                            hi16 = BE16(m68k_ptr[(*ext_words)++]);
                            lo16 = BE16(m68k_ptr[(*ext_words)++]);
                            *ptr++ = movw_immed_u16(outer_reg, lo16);
                            if (hi16)
                                *ptr++ = movt_immed_u16(outer_reg, hi16);
                            break;
                    }

                    if ((brief & 0x0f) == 0)
                    {
                        /* Address register indirect with index mode */
                        if (base_reg != 0xff && bd_reg != 0xff)
                        {
#ifdef __aarch64__
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                            *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                        }
                        else if (bd_reg == 0xff && base_reg != 0xff)
                        {
                            bd_reg = base_reg;
                        }
                        /*
                            Now, either base register or base displacement were given, if
                            index register was specified, use it.
                        */
                        ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                    }
                    else
                    {
                        if (bd_reg == 0xff)
                        {
                            bd_reg = RA_AllocARMRegister(&ptr);
                            *ptr++ = mov_reg(bd_reg, base_reg);
                            base_reg = 0xff;
                        }

                        /* Postindexed mode */
                        if (brief & 0x04)
                        {
                            /* Fetch data from base reg */
                            if (base_reg == 0xff)
                                *ptr++ = ldr_offset(bd_reg, bd_reg, 0);
                            else
                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, base_reg, 0);

                            if (outer_reg != 0xff)
#ifdef __aarch64__
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif

                            ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, index_reg, (brief >> 9) & 3);
                        }
                        else /* Preindexed mode */
                        {
                            /* Fetch data from base reg with eventually applied index */
                            if (brief & M68K_EA_IS)
                            {
                                if (bd_reg == 0xff) {
                                    bd_reg = RA_AllocARMRegister(&ptr);
                                    *ptr++ = ldr_offset(base_reg, bd_reg, 0);
                                }
                                else
                                    ptr = load_reg_from_addr(ptr, 4, base_reg, bd_reg, bd_reg, 0);
                            }
                            else
                            {
                                if (bd_reg != 0xff)
#ifdef __aarch64__
                                    *ptr++ = add_reg(bd_reg, base_reg, bd_reg, LSL, 0);
#else
                                    *ptr++ = add_reg(bd_reg, base_reg, bd_reg, 0);
#endif
                                ptr = load_reg_from_addr(ptr, 4, bd_reg, bd_reg, index_reg, (brief >> 9) & 3);
                            }

                            if (outer_reg != 0xff)
#ifdef __aarch64__
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, LSL, 0);
#else
                                *ptr++ = add_reg(bd_reg, bd_reg, outer_reg, 0);
#endif
                            ptr = store_reg_to_addr(ptr, size, bd_reg, *arm_reg, 0xff, 0);
                        }
                    }

                    if (bd_reg != 0xff)
                        RA_FreeARMRegister(&ptr, bd_reg);
                    if (outer_reg != 0xff)
                        RA_FreeARMRegister(&ptr, outer_reg);
                    if (base_reg != 0xff)
                        RA_FreeARMRegister(&ptr, base_reg);
                    if (index_reg != 0xff)
                        RA_FreeARMRegister(&ptr, index_reg);
                }
            }
            else if (src_reg == 0)
            {
                uint16_t lo16;
                lo16 = BE16(m68k_ptr[(*ext_words)++]);

                if (size == 0) {
                    ptr = load_s16_ext32(ptr, *arm_reg, lo16);
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    ptr = load_s16_ext32(ptr, tmp_reg, lo16);
                    ptr = store_reg_to_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);
                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
            else if (src_reg == 1)
            {
                uint16_t lo16, hi16;
                hi16 = BE16(m68k_ptr[(*ext_words)++]);
                lo16 = BE16(m68k_ptr[(*ext_words)++]);

                if (size == 0) {
#ifdef __aarch64__
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(*arm_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(*arm_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(*arm_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(*arm_reg, hi16, 1);
                    }
#else
                    *ptr++ = movw_immed_u16(*arm_reg, lo16);
                    if (hi16 != 0 || lo16 & 0x8000)
                        *ptr++ = movt_immed_u16(*arm_reg, hi16);
#endif
                //    *ptr++ = ldr_offset(REG_PC, *arm_reg, pc_off);
                }
                else
                {
                    uint8_t tmp_reg = RA_AllocARMRegister(&ptr);
                    //*ptr++ = ldr_offset(REG_PC, tmp_reg, pc_off);
#ifdef __aarch64__
                    if (lo16 == 0 && hi16 == 0)
                    {
                        *ptr++ = mov_reg(tmp_reg, 31);
                    }
                    else if (lo16 != 0)
                    {
                        *ptr++ = movw_immed_u16(tmp_reg, lo16);
                        if (hi16 != 0 || lo16 & 0x8000)
                            *ptr++ = movt_immed_u16(tmp_reg, hi16);
                    }
                    else
                    {
                        *ptr++ = mov_immed_u16(tmp_reg, hi16, 1);
                    }
#else
                    *ptr++ = movw_immed_u16(tmp_reg, lo16);
                    if (hi16 != 0 || lo16 & 0x8000)
                        *ptr++ = movt_immed_u16(tmp_reg, hi16);
#endif
                    ptr = store_reg_to_addr(ptr, size, tmp_reg, *arm_reg, 0xff, 0);

                    RA_FreeARMRegister(&ptr, tmp_reg);
                }
            }
        }
    }

    return ptr;
}
