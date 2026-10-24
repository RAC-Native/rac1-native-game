// The game frame's coroutine on x86-64 (Linux, macOS; the System V calling convention): a hand-written
// context switch (platform_posix.cpp, ctx_switch.h). Saves the callee-saved registers (rbx, rbp, r12-r15),
// MXCSR and the x87 control word on the current stack, stores rsp, then loads the other context: each
// context keeps its own float environment, as swapcontext does (the EE's rounding toward zero and FTZ / DAZ
// of docs/decisions.md, 11, stay with the coroutine). Host code: no game decision here.
//
// Adapted from rac2-native-game's src/platform/coroutine_x86_64.S (commit 2af0c6b; MIT,
// Copyright (c) 2026 rac2-native contributors: THIRD_PARTY_NOTICES.md), itself derived from this project.
//
// (No .note.gnu.property here: a binary with this object is not marked for CET's shadow stack, which a
// switch of stacks by hand would break.)
#if defined(__x86_64__) && !defined(_WIN32) && !defined(RAC1_CTX_FORCE_UCONTEXT)
#if defined(__APPLE__)
#define SYM(x) _##x
#else
#define SYM(x) x
#endif
    .text
    .p2align 4
    .globl SYM(rac1_ctx_switch)
#if defined(__ELF__)
    .type SYM(rac1_ctx_switch), @function
    .hidden SYM(rac1_ctx_switch)
#endif
// void rac1_ctx_switch(void** save_sp, void* load_sp)
// The frame, from rsp: MXCSR (4 bytes), the x87 control word (2), padding, r15, r14, r13, r12, rbx, rbp,
// the return address.
SYM(rac1_ctx_switch):
    pushq %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15
    subq  $8, %rsp
    stmxcsr (%rsp)
    fnstcw 4(%rsp)
    movq  %rsp, (%rdi)
    movq  %rsi, %rsp
    ldmxcsr (%rsp)
    fldcw 4(%rsp)
    addq  $8, %rsp
    popq  %r15
    popq  %r14
    popq  %r13
    popq  %r12
    popq  %rbx
    popq  %rbp
    ret
#if defined(__ELF__)
    .size SYM(rac1_ctx_switch), .-SYM(rac1_ctx_switch)
#endif

    .p2align 4
    .globl SYM(rac1_ctx_trampoline)
#if defined(__ELF__)
    .type SYM(rac1_ctx_trampoline), @function
    .hidden SYM(rac1_ctx_trampoline)
#endif
// A new context's first "return" lands here: rbx holds the C++ entry, which never returns.
SYM(rac1_ctx_trampoline):
    xorl  %ebp, %ebp
    callq *%rbx
    ud2
#if defined(__ELF__)
    .size SYM(rac1_ctx_trampoline), .-SYM(rac1_ctx_trampoline)
#endif
#endif
#if defined(__linux__) && defined(__ELF__)
    .section .note.GNU-stack, "", @progbits
#endif
