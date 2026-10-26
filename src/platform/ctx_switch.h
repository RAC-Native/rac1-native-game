// The coroutine's context switch on POSIX systems (platform_posix.cpp; Windows uses a fiber): a hand-written
// switch on x86-64 and AArch64 (coroutine_x86_64.S, coroutine_aarch64.S), ucontext elsewhere. Host code.
//
// Why by hand: ucontext (getcontext / makecontext / swapcontext) is deprecated on macOS and absent from
// Android's C library (bionic has the ucontext_t type, not the functions). Each context keeps its own float
// environment, as ucontext does: MXCSR and the x87 control word on x86-64, FPCR on AArch64 (the EE's rounding
// toward zero and flush-to-zero, docs/decisions.md 11, carried into the coroutine at its start).
//
// RAC1_CTX_ASM: the hand-written switch is compiled. RAC1_CTX_UCONTEXT: the ucontext switch is compiled (the
// fallback for other architectures; on Linux also the comparison path, RAC1_COROUTINE=ucontext). CMake's
// RAC1_UCONTEXT=ON (RAC1_CTX_FORCE_UCONTEXT) compiles only ucontext.
//
// Adapted from rac2-native-game's src/platform/ctx_switch.h (commit 2af0c6b; MIT, Copyright (c) 2026
// rac2-native contributors: THIRD_PARTY_NOTICES.md), itself derived from this project.
#ifndef RAC1_PLATFORM_CTX_SWITCH_H
#define RAC1_PLATFORM_CTX_SWITCH_H

#if (defined(__PPC__) || defined(__powerpc__)) && !defined(__powerpc64__)
#define RAC1_CTX_PPC32 1
#endif
#if (defined(__aarch64__) || defined(__arm64__) || defined(__x86_64__) || defined(RAC1_CTX_PPC32)) && !defined(_WIN32) && \
    !defined(RAC1_CTX_FORCE_UCONTEXT)
#define RAC1_CTX_ASM 1
#endif
// (the Wii U's C library, devkitPPC's newlib, has no ucontext)
#if !defined(RAC1_CTX_ASM) || (!defined(__ANDROID__) && !defined(__APPLE__) && !defined(__WIIU__))
#define RAC1_CTX_UCONTEXT 1
#endif

#if defined(RAC1_CTX_ASM)
#include <cstdint>
#include <cstring>
#if defined(__x86_64__)
#include <xmmintrin.h>
#endif

extern "C" void rac1_ctx_switch(void** save_sp, void* load_sp);
extern "C" void rac1_ctx_trampoline();

namespace rac1::platform {

// A new context's first frame below `top` (16-byte aligned), as rac1_ctx_switch leaves one: the first switch to
// it "returns" into the trampoline, which calls `fn` (never returning). Its float modes are the calling thread's
// (as getcontext copies them).
inline void* ctx_new_frame(std::uintptr_t top, void (*fn)()) {
#if defined(__x86_64__)
    auto* f = reinterpret_cast<std::uint64_t*>(top - 80);  // (after the return, rsp = top - 16: the call aligned as the ABI wants)
    std::memset(f, 0, 80);
    std::uint16_t fcw = 0;
    __asm__ volatile("fnstcw %0" : "=m"(fcw));
    f[0] = _mm_getcsr() | static_cast<std::uint64_t>(fcw) << 32;  // MXCSR, the x87 control word
    f[5] = reinterpret_cast<std::uint64_t>(fn);                    // rbx
    f[7] = reinterpret_cast<std::uint64_t>(&rac1_ctx_trampoline);  // the return address
    return f;
#elif defined(RAC1_CTX_PPC32)
    auto* f = reinterpret_cast<std::uint32_t*>(top - 16 - 240);  // (after the switch's addi, r1 = top - 16)
    std::memset(f, 0, 240);
    f[2] = reinterpret_cast<std::uint32_t>(fn);                    // r14
    f[21] = reinterpret_cast<std::uint32_t>(&rac1_ctx_trampoline);  // LR
    double fpscr = 0.0;
    __asm__ volatile("mffs %0" : "=f"(fpscr));
    std::memcpy(f + 22, &fpscr, 8);                                 // FPSCR (the calling thread's float mode)
    return f;
#else
    auto* f = reinterpret_cast<std::uint64_t*>(top - 176);
    std::memset(f, 0, 176);
    f[0] = reinterpret_cast<std::uint64_t>(fn);                     // x19
    f[11] = reinterpret_cast<std::uint64_t>(&rac1_ctx_trampoline);  // x30
    std::uint64_t fpcr = 0;
    __asm__ volatile("mrs %0, fpcr" : "=r"(fpcr));
    f[20] = fpcr;
    return f;
#endif
}

}  // namespace rac1::platform
#endif

#endif
