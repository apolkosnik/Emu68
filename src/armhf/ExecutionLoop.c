/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

    ARM32 JIT execution loop, mirroring the structure of the AArch64
    ExecutionLoop.c but adapted for the ARMv7 dynamic register allocator.

    On ARM32 the JIT-translated code receives the M68KState pointer in
    r11 (REG_CTX) and uses r10 for SR, r12 for the m68k PC. Registers
    r0-r9 are dynamically allocated by the LRU register allocator.
*/

#include <M68k.h>
#include <support.h>
#include <config.h>
#ifdef PISTORM
#ifndef PISTORM32
#define PS_PROTOCOL_IMPL
#include "../pistorm/ps_protocol.h"
#endif
#endif

extern struct List ICache[EMU68_HASHSIZE];
extern struct M68KState *__m68k_state;

/*
 * Call into a JIT-translated ARM code block.
 *
 * This is a naked function that sets up the register environment expected
 * by JIT code (r11 = CTX, r12 = entry) and calls the block, preserving
 * callee-saved registers across the call boundary.
 */
static void __attribute__((naked, noinline)) CallARMCode(
    void (*arm_code)(struct M68KState *), struct M68KState *ctx)
{
    (void)arm_code;
    (void)ctx;

    asm volatile(
        /* Isolate JIT code from the C caller's register state */
        "sub sp, sp, #4\n"
        "push {r4-r11, lr}\n"
        "mov r11, r1\n"        /* r11 = CTX (M68KState*) */
        "mov r12, r0\n"        /* r12 = ARM code entry point */
        "mov r0, r1\n"         /* r0 = CTX as first arg for JIT */
        "blx r12\n"
        "pop {r4-r11, lr}\n"
        "add sp, sp, #4\n"
        "bx lr\n");
}

static inline struct M68KTranslationUnit *FindUnit(uint16_t *PC)
{
    /* Perform search */
    uint32_t hash = (uint32_t)(uintptr_t)PC;
    struct List *bucket = &ICache[(hash >> EMU68_HASHSHIFT) & EMU68_HASHMASK];
    struct M68KTranslationUnit *node;

    /* Go through the list of translated units */
    ForeachNode(bucket, node)
    {
        /* Check if unit is found */
        if (node->mt_M68kAddress == PC)
        {
            return node;
        }
    }

    return NULL;
}

#ifdef PISTORM
#ifndef PISTORM32

extern volatile unsigned char bus_lock;

static inline int GetIPLLevel()
{
    volatile uint32_t *gpio = (void *)0xf2200000;

    *(gpio + 7) = LE32(REG_STATUS << PIN_A0);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);
    *(gpio + 7) = LE32(1 << PIN_RD);

    unsigned int value = LE32(*(gpio + 13));

    *(gpio + 10) = LE32(0xffffec);

    return (value >> 21) & 7;
}
#endif
#else
static inline int GetIPLLevel() { return 0; }
#endif

void MainLoop()
{
    struct M68KState *ctx = __m68k_state;
    uint32_t last_PC = 0xffffffff;

    /* The JIT loop runs forever */
    while(1)
    {
        uint16_t *PC = (uint16_t *)(uintptr_t)BE32(ctx->PC);

        /* If (unlikely) there was interrupt pending, check if it needs to be processed */
        if (unlikely(ctx->INT32 != 0))
        {
            uint16_t SR;
            uint16_t SRcopy;
            int level = 0;
            uint32_t vector;
            uint32_t vbr;

            /* Find out requested IPL level based on ARM state and real IPL line */
            if (ctx->INT.ARM_err)
            {
                level = 7;
                ctx->INT.ARM_err = 0;
            }
            else
            {
                if (ctx->INT.ARM)
                {
                    level = 6;
                    ctx->INT.ARM = 0;
                }
#ifdef PISTORM32
                if (ctx->INT.IPL > level)
                {
                    level = ctx->INT.IPL;
                }
#else
                if (ctx->INT.IPL)
                {
                    int ipl_level;

#if PISTORM_WRITE_BUFFER
                    while(__atomic_test_and_set(&bus_lock, __ATOMIC_ACQUIRE)) { asm volatile("yield"); }
#endif
                    ipl_level = GetIPLLevel();

#if PISTORM_WRITE_BUFFER
                    __atomic_clear(&bus_lock, __ATOMIC_RELEASE);
#endif
                    if (ipl_level > level)
                    {
                        level = ipl_level;
                    }
                }
#endif
            }

            /* Get SR and test the IPL mask value */
            SR = BE16(ctx->SR);
            int IPL_mask = (SR & SR_IPL) >> SRB_IPL;

            /* Any unmasked interrupts? Process them */
            if (level == 7 || level > IPL_mask)
            {
                uint32_t sp;

                if (likely((SR & SR_S) == 0))
                {
                    /* If not yet in supervisor mode, save USP */
                    ctx->USP.u32 = ctx->A[7].u32;

                    /* Load either ISP or MSP */
                    if (unlikely((SR & SR_M) != 0))
                    {
                        sp = BE32(ctx->MSP.u32);
                    }
                    else
                    {
                        sp = BE32(ctx->ISP.u32);
                    }
                }
                else
                {
                    sp = BE32(ctx->A[7].u32);
                }

                SRcopy = SR;
                /* Swap C and V flags in the copy */
                if ((SRcopy & 3) != 0 && (SRcopy & 3) != 3)
                    SRcopy ^= 3;
                vector = 0x60 + (level << 2);

                /* Set supervisor mode */
                SR |= SR_S;

                /* Clear Trace mode */
                SR &= ~(SR_T0 | SR_T1);

                /* Insert current level into SR */
                SR &= ~SR_IPL;
                SR |= ((level & 7) << SRB_IPL);

                /* Push exception frame */
                sp -= 8;
                *(uint16_t *)(uintptr_t)(sp) = BE16(SRcopy);
                *(uint32_t *)(uintptr_t)(sp + 2) = ctx->PC;
                *(uint16_t *)(uintptr_t)(sp + 6) = BE16(vector);

                /* Update SP */
                ctx->A[7].u32 = BE32(sp);

                /* Set SR */
                ctx->SR = BE16(SR);

                /* Get VBR */
                vbr = BE32(ctx->VBR);

                /* Load PC from vector table */
                ctx->PC = *(uint32_t *)(uintptr_t)(vbr + vector);

                /* Update PC for the search below */
                PC = (uint16_t *)(uintptr_t)BE32(ctx->PC);
            }

            /* All interrupts masked or new PC loaded and stack swapped */
        }

        /* Check if JIT cache is enabled */
        uint32_t cacr = BE32(ctx->CACR);

        if (likely(cacr & CACR_IE))
        {
            /* The last PC is the same as currently set PC? */
            if (last_PC == (uint32_t)(uintptr_t)PC)
            {
                /* PC hasn't changed, the unit is still valid from last iteration */
                /* Fall through to translation lookup */
            }

            /* Find unit in the hashtable based on the PC value */
            struct M68KTranslationUnit *node = FindUnit(PC);

            /* Unit exists? */
            if (node != NULL)
            {
                last_PC = (uint32_t)(uintptr_t)PC;

                /* Call the JIT-translated code */
                CallARMCode(node->mt_ARMEntryPoint, ctx);
                continue;
            }

            /* If we are that far there was no JIT unit found */
            /* Get the code. This never fails */
            node = M68K_GetTranslationUnit(PC);
            last_PC = (uint32_t)(uintptr_t)PC;

            /* Call the JIT-translated code */
            CallARMCode(node->mt_ARMEntryPoint, ctx);
        }
        else
        {
            struct M68KTranslationUnit *node = NULL;

            /* Uncached mode - reset last_PC */
            last_PC = 0xffffffff;

            /* Find the unit */
            node = FindUnit(PC);
            /* If node is found verify it */
            if (likely(node != NULL))
            {
                node = M68K_VerifyUnit(node);
            }
            /* If node was not found or invalidated, translate code */
            if (unlikely(node == NULL))
            {
                node = M68K_GetTranslationUnit(PC);
            }

            CallARMCode(node->mt_ARMEntryPoint, ctx);
        }
    }
}
