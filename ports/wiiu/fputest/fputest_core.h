// The PS2 float rules and the coroutine, checked the same way on the PC and on the Wii U: the same
// pseudo-random operands go through the EE's rounding (ps2fpu: toward zero, denormals to zero; divide and
// square root to nearest) and the results are hashed. Both machines must print the same hashes.
#pragma once
#include "runtime/ee/ps2_fpu.h"

#include <cstdint>
#include <cstring>

namespace fputest {

struct Rng {  // xorshift32
    uint32_t s = 0x12345678u;
    uint32_t next() { s ^= s << 13; s ^= s >> 17; s ^= s << 5; return s; }
    // floats of every kind, denormals and huge values included, NaN and infinity left out (the PS2 has none)
    float any() {
        uint32_t u = next();
        uint32_t e = (u >> 23) & 0xffu;
        if (e == 0xffu) u &= ~(1u << 23);
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
    // ordinary numbers in a range where sums and products stay finite
    float mid() {
        uint32_t u = (next() & 0x807fffffu) | ((96u + next() % 64u) << 23);
        float f;
        std::memcpy(&f, &u, 4);
        return f;
    }
};

struct Hash {  // FNV-1a over the results' bits
    uint32_t h = 2166136261u;
    void add(float f) {
        uint32_t u;
        std::memcpy(&u, &f, 4);
        for (int i = 0; i < 4; i++) { h ^= (u >> (8 * i)) & 0xffu; h *= 16777619u; }
    }
};

struct Results {
    uint32_t add, mul, div, sqrt, denorm, vendor;
};

// volatile operands: the compiler must not fold anything at compile time (it would round to nearest)
inline Results run(int n) {
    Results r{};
    Rng g;
    Hash ha, hm, hd, hs, hz;
    for (int i = 0; i < n; i++) {
        volatile float a = g.mid(), b = g.mid();
        ha.add(a + b); ha.add(a - b);
        hm.add(a * b);
        hd.add(rac1::ps2fpu::div_nearest(static_cast<float>(a), static_cast<float>(b)));
        hs.add(rac1::ps2fpu::sqrt_nearest(a < 0 ? -a : a));
        volatile float c = g.any(), d = g.any();
        hz.add(c + d); hz.add(c * d);
    }
    r.add = ha.h; r.mul = hm.h; r.div = hd.h; r.sqrt = hs.h; r.denorm = hz.h;
    // The Vendor's opening (docs/decisions.md 11): 0.1 added eleven times toward zero ends at 1.0999998
    volatile float v = 0.0f, step = 0.1f;
    for (int i = 0; i < 11; i++) v = v + step;
    float vf = v;
    std::memcpy(&r.vendor, &vf, 4);
    return r;
}

}  // namespace fputest

// The coroutine: a context made while the thread rounds toward zero keeps that mode while the host thread
// rounds to nearest between switches (each context has its own float environment).
#include "src/platform/ctx_switch.h"
#if defined(RAC1_CTX_ASM)
namespace fputest {
inline void* g_host_sp = nullptr;
inline void* g_game_sp = nullptr;
inline volatile float g_acc = 0.0f;
inline int g_rounds = 0;
inline void co_main() {
    for (;;) {
        volatile float k = 1.0001f, step = 0.1f;
        g_acc = g_acc * k + step;
        g_rounds++;
        rac1_ctx_switch(&g_game_sp, g_host_sp);
    }
}
alignas(16) inline unsigned char g_stack[64 * 1024];
// Returns the coroutine's final value's bits; `direct` gets the same computation done on this thread toward zero.
inline uint32_t coroutine_check(int rounds, uint32_t* direct) {
    g_game_sp = rac1::platform::ctx_new_frame(reinterpret_cast<std::uintptr_t>(g_stack + sizeof g_stack) & ~std::uintptr_t{15},
                                              &co_main);
    {
        rac1::ps2fpu::NearestScope nearest;  // the host side rounds to nearest meanwhile
        for (int i = 0; i < rounds; i++) rac1_ctx_switch(&g_host_sp, g_game_sp);
    }
    volatile float acc = 0.0f;
    for (int i = 0; i < rounds; i++) { volatile float k = 1.0001f, step = 0.1f; acc = acc * k + step; }
    float a = acc, b = g_acc;
    std::memcpy(direct, &a, 4);
    uint32_t u;
    std::memcpy(&u, &b, 4);
    return u;
}
}  // namespace fputest
#endif
