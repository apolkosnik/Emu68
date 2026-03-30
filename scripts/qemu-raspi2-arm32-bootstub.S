    .syntax unified
    .arm
    .global _start

    .equ DTB_ADDR, 0x00400000
    .equ KERNEL_ADDR, 0x00008000

_start:
    mov r0, #0
    mov r1, #0
    ldr r2, =DTB_ADDR
    mov r3, #0
    ldr pc, =KERNEL_ADDR
