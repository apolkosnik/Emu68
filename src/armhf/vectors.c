/*
    Copyright © 2019 Michal Schulz <michal.schulz@gmx.de>
    https://github.com/michalsc

    This Source Code Form is subject to the terms of the
    Mozilla Public License, v. 2.0. If a copy of the MPL was not distributed
    with this file, You can obtain one at http://mozilla.org/MPL/2.0/.

    ARMv7 exception vector table and handlers.

    This mirrors the organizational structure of aarch64/vectors.c but
    uses the ARM32 exception model (7 vectors at fixed offsets) instead
    of the AArch64 vector table layout.
*/

#include <stdint.h>
#include "config.h"
#include "support.h"
#include "M68k.h"

extern struct M68KState *__m68k_state;

/*
 * SYSHandler - unified exception handler called from the vector stubs.
 *
 * For the PiStorm path, the interrupt delivery is handled in the
 * main execution loop (start_rpi.c / ExecutionLoop.c) rather than
 * in the exception handler, since the ARM32 JIT works through the
 * M68KState pointer and doesn't use fixed register bindings.
 */
void __attribute__((used)) SYSHandler(uint32_t type, void *context)
{
    (void)context;

    switch (type) {
        case 0:     /* Undefined instruction */
            kprintf("[EXC] Undefined instruction\n");
            break;
        case 1:     /* SWI */
            kprintf("[EXC] Software interrupt\n");
            break;
        case 2:     /* Prefetch abort */
        {
            uint32_t far, fsr;
            asm volatile("mrc p15, 0, %0, c6, c0, 2" : "=r"(far));
            asm volatile("mrc p15, 0, %0, c5, c0, 1" : "=r"(fsr));
            kprintf("[EXC] Prefetch abort at %08x, IFSR=%08x\n", far, fsr);
            break;
        }
        case 3:     /* Data abort */
        {
            uint32_t far, fsr;
            asm volatile("mrc p15, 0, %0, c6, c0, 0" : "=r"(far));
            asm volatile("mrc p15, 0, %0, c5, c0, 0" : "=r"(fsr));
            kprintf("[EXC] Data abort at %08x, DFSR=%08x\n", far, fsr);
            break;
        }
        case 4:     /* IRQ */
        {
#ifdef PISTORM
            /* Mark interrupt pending so the main loop processes it */
            if (__m68k_state)
                __m68k_state->INT.ARM = 6;
#endif
            break;
        }
        case 5:     /* FIQ */
        {
#ifdef PISTORM
            if (__m68k_state)
                __m68k_state->INT.ARM = 6;
#endif
            break;
        }
        default:
            kprintf("[EXC] Unknown exception type %d\n", type);
            break;
    }
}

/*
 * ARMv7 exception vector table.
 *
 * The vector table uses simple branch-to-handler stubs. Each exception
 * mode has its own banked LR and SP, so we save minimal context on the
 * stack and call into C.
 *
 * This follows the same pattern as aarch64/vectors.c where all exception
 * entries save context, call SYSHandler with a type code, and restore.
 */
void __attribute__((used)) __stub_armv7_vectors()
{
    asm volatile(
    "       .section .vectors           \n"
    "       .balign 32                  \n"
    "       .globl _vectors_start       \n"
    "_vectors_start:                    \n"
    "       b   _vec_reset              \n"     /* 0x00 Reset */
    "       b   _vec_undef              \n"     /* 0x04 Undefined Instruction */
    "       b   _vec_swi                \n"     /* 0x08 Software Interrupt */
    "       b   _vec_prefetch_abort     \n"     /* 0x0C Prefetch Abort */
    "       b   _vec_data_abort         \n"     /* 0x10 Data Abort */
    "       b   _vec_reserved           \n"     /* 0x14 Reserved */
    "       b   _vec_irq                \n"     /* 0x18 IRQ */
    "       b   _vec_fiq                \n"     /* 0x1C FIQ */
    "                                   \n"
    "_vec_reset:                        \n"
    "       b   _vec_reset              \n"
    "                                   \n"
    "_vec_undef:                        \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #0                 \n"     /* type = 0: undefined */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "_vec_swi:                          \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #1                 \n"     /* type = 1: SWI */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "_vec_prefetch_abort:               \n"
    "       sub  lr, lr, #4             \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #2                 \n"     /* type = 2: prefetch abort */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "_vec_data_abort:                   \n"
    "       sub  lr, lr, #8             \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #3                 \n"     /* type = 3: data abort */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "_vec_reserved:                     \n"
    "       b   _vec_reserved           \n"
    "                                   \n"
    "_vec_irq:                          \n"
    "       sub  lr, lr, #4             \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #4                 \n"     /* type = 4: IRQ */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "_vec_fiq:                          \n"
    "       sub  lr, lr, #4             \n"
    "       push {r0-r3, r12, lr}       \n"
    "       mov  r0, #5                 \n"     /* type = 5: FIQ */
    "       mov  r1, sp                 \n"
    "       bl   SYSHandler             \n"
    "       pop  {r0-r3, r12, lr}       \n"
    "       movs pc, lr                 \n"
    "                                   \n"
    "       .section .text              \n"
    );
}
