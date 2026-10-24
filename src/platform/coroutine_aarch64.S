// The game frame's coroutine on AArch64 (Linux arm64, Android, macOS): a hand-written context switch
// (platform_posix.cpp, ctx_switch.h). Saves the callee-saved registers (x19-x30, d8-d15) and FPCR on the
// current stack, stores sp, then loads the other context: each context keeps its own float environment, as
// swapcontext does (the EE's rounding toward zero and flush-to-zero of docs/decisions.md, 11, stay with the
// coroutine). Host code: no game decision here.
//
// Adapted from rac2-native-game's src/platform/coroutine_aarch64.S (commit 2af0c6b; MIT,
// Copyright (c) 2026 rac2-native contributors: THIRD_PARTY_NOTICES.md), itself derived from this project.
#if (defined(__aarch64__) || defined(__arm64__)) && !defined(_WIN32) && !defined(RAC1_CTX_FORCE_UCONTEXT)
#if defined(__APPLE__)
#define SYM(x) _##x
#else
#define SYM(x) x
#endif
    .text
    .p2align 2
    .globl SYM(rac1_ctx_switch)
#if defined(__ELF__)
    .type SYM(rac1_ctx_switch), %function
    .hidden SYM(rac1_ctx_switch)
#endif
// void rac1_ctx_switch(void** save_sp, void* load_sp)
// The frame, from sp: x19-x30 (96 bytes), d8-d15 (64), FPCR (8), padding (8).
SYM(rac1_ctx_switch):
    sub  sp, sp, #176
    stp  x19, x20, [sp, #0]
    stp  x21, x22, [sp, #16]
    stp  x23, x24, [sp, #32]
    stp  x25, x26, [sp, #48]
    stp  x27, x28, [sp, #64]
    stp  x29, x30, [sp, #80]
    stp  d8,  d9,  [sp, #96]
    stp  d10, d11, [sp, #112]
    stp  d12, d13, [sp, #128]
    stp  d14, d15, [sp, #144]
    mrs  x9, fpcr
    str  x9, [sp, #160]
    mov  x9, sp
    str  x9, [x0]
    mov  sp, x1
    ldp  x19, x20, [sp, #0]
    ldp  x21, x22, [sp, #16]
    ldp  x23, x24, [sp, #32]
    ldp  x25, x26, [sp, #48]
    ldp  x27, x28, [sp, #64]
    ldp  x29, x30, [sp, #80]
    ldp  d8,  d9,  [sp, #96]
    ldp  d10, d11, [sp, #112]
    ldp  d12, d13, [sp, #128]
    ldp  d14, d15, [sp, #144]
    ldr  x9, [sp, #160]
    msr  fpcr, x9
    add  sp, sp, #176
    ret
#if defined(__ELF__)
    .size SYM(rac1_ctx_switch), .-SYM(rac1_ctx_switch)
#endif

    .p2align 2
    .globl SYM(rac1_ctx_trampoline)
#if defined(__ELF__)
    .type SYM(rac1_ctx_trampoline), %function
    .hidden SYM(rac1_ctx_trampoline)
#endif
// A new context's first "return" lands here: x19 holds the C++ entry, which never returns.
SYM(rac1_ctx_trampoline):
    mov  x29, #0
    blr  x19
    brk  #0
#if defined(__ELF__)
    .size SYM(rac1_ctx_trampoline), .-SYM(rac1_ctx_trampoline)
#endif
#endif
#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack, "", %progbits
#endif
