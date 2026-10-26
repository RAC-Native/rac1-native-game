// The game frame's coroutine on 32-bit PowerPC (the Wii U's Espresso, devkitPPC's EABI): a hand-written context
// switch (platform_posix.cpp, ctx_switch.h). Saves the callee-saved registers (r14-r31, f14-f31), CR, LR and FPSCR
// on the current stack, stores r1, then loads the other context: each context keeps its own float environment, as
// swapcontext does (the EE's rounding toward zero and the non-IEEE mode of docs/decisions.md, 11, stay with the
// coroutine). Host code: no game decision here. Only the doubles of f14-f31 are kept: the compiler emits no
// paired-single code (no -mpaired), so their second halves carry nothing across a call.
#if (defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__) && !defined(RAC1_CTX_FORCE_UCONTEXT)
    .text
    .p2align 2
    .globl rac1_ctx_switch
    .type rac1_ctx_switch, @function
// void rac1_ctx_switch(void** save_sp /* r3 */, void* load_sp /* r4 */)
// The frame, from r1 (240 bytes, 16-byte aligned): the back chain (4), padding (4), r14-r31 (72, at 8),
// CR (at 80), LR (at 84), FPSCR as a double (at 88), f14-f31 (144, at 96).
rac1_ctx_switch:
    stwu  1, -240(1)
    stmw  14, 8(1)
    mfcr  0
    stw   0, 80(1)
    mflr  0
    stw   0, 84(1)
    mffs  0
    stfd  0, 88(1)
    stfd  14, 96(1)
    stfd  15, 104(1)
    stfd  16, 112(1)
    stfd  17, 120(1)
    stfd  18, 128(1)
    stfd  19, 136(1)
    stfd  20, 144(1)
    stfd  21, 152(1)
    stfd  22, 160(1)
    stfd  23, 168(1)
    stfd  24, 176(1)
    stfd  25, 184(1)
    stfd  26, 192(1)
    stfd  27, 200(1)
    stfd  28, 208(1)
    stfd  29, 216(1)
    stfd  30, 224(1)
    stfd  31, 232(1)
    stw   1, 0(3)
    mr    1, 4
    lfd   14, 96(1)
    lfd   15, 104(1)
    lfd   16, 112(1)
    lfd   17, 120(1)
    lfd   18, 128(1)
    lfd   19, 136(1)
    lfd   20, 144(1)
    lfd   21, 152(1)
    lfd   22, 160(1)
    lfd   23, 168(1)
    lfd   24, 176(1)
    lfd   25, 184(1)
    lfd   26, 192(1)
    lfd   27, 200(1)
    lfd   28, 208(1)
    lfd   29, 216(1)
    lfd   30, 224(1)
    lfd   31, 232(1)
    lfd   0, 88(1)
    mtfsf 0xff, 0
    lwz   0, 84(1)
    mtlr  0
    lwz   0, 80(1)
    mtcr  0
    lmw   14, 8(1)
    addi  1, 1, 240
    blr
    .size rac1_ctx_switch, .-rac1_ctx_switch

    .p2align 2
    .globl rac1_ctx_trampoline
    .type rac1_ctx_trampoline, @function
// A new context's first "return" lands here: r14 holds the C++ entry, which never returns. A null back chain
// ends the stack for debuggers.
rac1_ctx_trampoline:
    stwu  1, -16(1)
    li    0, 0
    stw   0, 0(1)
    mtctr 14
    bctrl
    trap
    .size rac1_ctx_trampoline, .-rac1_ctx_trampoline
#endif
#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack, "", %progbits
#endif
