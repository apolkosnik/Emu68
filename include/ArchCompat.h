/*
    Architecture compatibility macros for AArch32/AArch64.
    These shims let the current translator code compile for the restored ARMv7
    backend without disturbing the AArch64 path.
*/

#ifndef _ARCH_COMPAT_H
#define _ARCH_COMPAT_H

#include <stdint.h>

#ifndef __aarch64__

#define A64_CC_EQ ARM_CC_EQ
#define A64_CC_AL ARM_CC_AL
#define A64_CC_NE ARM_CC_NE
#define A64_CC_CS ARM_CC_CS
#define A64_CC_CC ARM_CC_CC
#define A64_CC_MI ARM_CC_MI
#define A64_CC_PL ARM_CC_PL
#define A64_CC_VS ARM_CC_VS
#define A64_CC_VC ARM_CC_VC
#define A64_CC_HI ARM_CC_HI
#define A64_CC_LS ARM_CC_LS
#define A64_CC_GE ARM_CC_GE
#define A64_CC_LT ARM_CC_LT
#define A64_CC_GT ARM_CC_GT
#define A64_CC_LE ARM_CC_LE

#define LSL 0
#define LSR 1
#define ASR 2
#define ROR 3

#define REG_PROTECT 0

#define csel(dest, src1, src2, cond) mov_cc_reg(cond, dest, src1)

static inline uint32_t mov_immed_u16(uint8_t rd, uint16_t imm, uint8_t shift)
{
    if (shift == 0) {
        return movw_immed_u16(rd, imm);
    }

    return movt_immed_u16(rd, imm);
}

#define lsrv64(rd, rn, rm) lsr_reg(rd, rn, rm)
#define lslv64(rd, rn, rm) lsl_reg(rd, rn, rm)
#define asrv64(rd, rn, rm) asr_reg(rd, rn, rm)
#define rorv64(rd, rn, rm) ror_reg(rd, rn, rm)
#define rorv(rd, rn, rm) ror_reg(rd, rn, rm)
#define lsr64(rd, rn, shift) lsr_immed(rd, rn, shift)
#define lsl64(rd, rn, shift) lsl_immed(rd, rn, shift)
#define asr64(rd, rn, shift) asr_immed(rd, rn, shift)
#define ror(rd, rn, shift) ror_immed(rd, rn, shift)

#define ubfx64(rd, rn, lsb, width) ubfx(rd, rn, lsb, width)
#define sbfx64(rd, rn, lsb, width) sbfx(rd, rn, lsb, width)
#define bfxil64(rd, rn, lsb, width) bfxil(rd, rn, lsb, width)
#define bfxil(rd, rn, lsb, width) bfi(rd, rn, lsb, width)

static inline uint32_t bfi64(uint8_t rd, uint8_t rn, uint8_t lsb, uint8_t width)
{
    return bfi(rd, rn, lsb, width > 32 ? 32 : width);
}

#define ands64_reg(rd, rn, rm, shift, amount) ands_reg(rd, rn, rm, amount)
#define orr64_reg(rd, rn, rm, shift, amount) orr_reg(rd, rn, rm, amount)
#define eor64_reg(rd, rn, rm, shift, amount) eor_reg(rd, rn, rm, amount)
#define bic64_reg(rd, rn, rm, shift, amount) bic_reg(rd, rn, rm)
#define orn64_reg(rd, rn, rm, shift, amount) orn_reg(rd, rn, rm)

static inline uint32_t orn_reg(uint8_t rd, uint8_t rn, uint8_t rm)
{
    (void)rn;
    return mvn_reg(rd, rm, 0);
}

static inline uint16_t arm_compat_mask(uint8_t width)
{
    if (width >= 16) {
        return 0xffffu;
    }

    return (1u << width) - 1u;
}

#define sub64_immed(rd, rn, imm) sub_immed(rd, rn, imm)
#define add64_immed(rd, rn, imm) add_immed(rd, rn, imm)
#define bic64_immed(rd, rn, width, ror, n) bic_immed(rd, rn, (1u << (width)) - 1)

static inline uint32_t ands64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n)
{
    (void)ror;
    (void)n;
    return ands_immed(rd, rn, arm_compat_mask(width));
}

static inline uint32_t orr64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n)
{
    (void)ror;
    (void)n;
    return orr_immed(rd, rn, arm_compat_mask(width));
}

static inline uint32_t eor64_immed(uint8_t rd, uint8_t rn, uint8_t width, uint8_t ror, uint8_t n)
{
    (void)ror;
    (void)n;
    return eor_immed(rd, rn, arm_compat_mask(width));
}

#define tbz(reg, bit, offset) tst_immed(reg, 1u << (bit))
#define tbnz(reg, bit, offset) tst_immed(reg, 1u << (bit))
#define cbnz(reg, offset) cmp_immed(reg, 0)
#define cbz(reg, offset) cmp_immed(reg, 0)
#define cbnz_64(reg, offset) cmp_immed(reg, 0)
#define cbz_64(reg, offset) cmp_immed(reg, 0)
#define b(offset) b_cc(ARM_CC_AL, offset)

#define clz64(rd, rn) clz(rd, rn)

static inline uint32_t movn_immed_u16(uint8_t rd, uint16_t imm, uint8_t shift)
{
    (void)shift;
    return movw_immed_u16(rd, (uint16_t)~imm);
}

#define neg_reg(...) _GET_NEG_MACRO(__VA_ARGS__, neg_reg_4, neg_reg_2)(__VA_ARGS__)
#define _GET_NEG_MACRO(_1, _2, _3, _4, NAME, ...) NAME
#define neg_reg_2(rd, rm) rsb_immed(rd, rm, 0)
#define neg_reg_4(rd, rm, shift, amount) rsb_immed(rd, rm, 0)
#define msub(rd, rn, rm, ra) mls(rd, rn, rm, ra)
#define cmn64_reg(rn, rm, shift, amount) cmn_reg(rn, rm)

static inline uint32_t ldr64_offset(uint8_t rn, uint8_t rt, int32_t offset)
{
    return ldr_offset(rn, rt, (int16_t)offset);
}

static inline uint32_t ldr64_offset_preindex(uint8_t rn, uint8_t rt, int32_t offset)
{
    return ldr_offset_preindex(rn, rt, (int16_t)offset);
}

static inline uint32_t ldr64_offset_postindex(uint8_t rn, uint8_t rt, int32_t offset)
{
    return ldr_offset_postindex(rn, rt, (int16_t)offset);
}

static inline uint32_t str64_offset(uint8_t rn, uint8_t rt, int32_t offset)
{
    return str_offset(rn, rt, (int16_t)offset);
}

static inline uint32_t str64_offset_preindex(uint8_t rn, uint8_t rt, int32_t offset)
{
    return str_offset_preindex(rn, rt, (int16_t)offset);
}

static inline uint32_t str64_offset_postindex(uint8_t rn, uint8_t rt, int32_t offset)
{
    return str_offset_postindex(rn, rt, (int16_t)offset);
}

static inline uint32_t ror64(uint8_t rd, uint8_t rn, uint8_t shift)
{
    return ror_immed(rd, rn, shift);
}

static inline uint32_t csinc(uint8_t rd, uint8_t rn, uint8_t rm, uint8_t cond)
{
    (void)rm;
    return mov_cc_reg(cond, rd, rn);
}

static inline uint32_t ldrsh_offset_preindex(uint8_t rn, uint8_t rt, int32_t offset)
{
    return ldrsh_offset(rt, rn, (int8_t)offset);
}

#define and_immed_compat(dest, src, width, ror) and_immed(dest, src, (1u << (width)) - 1)
#define ands_immed_compat(dest, src, width, ror) ands_immed(dest, src, (1u << (width)) - 1)
#define bic_immed_compat(dest, src, width, ror) bic_immed(dest, src, (1u << (width)) - 1)
#define orr_immed_compat(dest, src, width, ror) orr_immed(dest, src, (1u << (width)) - 1)
#define tst_immed_compat(src, width, ror) tst_immed(src, (1u << (width)) - 1)

#define add_reg_compat(dest, src, reg, shift, amount) add_reg(dest, src, reg, amount)
#define sub_reg_compat(dest, src, reg, shift, amount) sub_reg(dest, src, reg, amount)

#else

#define and_immed_4(dest, src, width, ror) and_immed(dest, src, width, ror)
#define ands_immed_4(dest, src, width, ror) ands_immed(dest, src, width, ror)
#define bic_immed_4(dest, src, width, ror) bic_immed(dest, src, width, ror)
#define orr_immed_4(dest, src, width, ror) orr_immed(dest, src, width, ror)
#define tst_immed_4(src, width, ror) tst_immed(src, width, ror)
#define add_reg_5(dest, src, reg, shift, amount) add_reg(dest, src, reg, shift, amount)
#define sub_reg_5(dest, src, reg, shift, amount) sub_reg(dest, src, reg, shift, amount)

#endif

#endif /* _ARCH_COMPAT_H */
