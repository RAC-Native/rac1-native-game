#ifndef RAC1_GAME_OVL_COMMON_H
#define RAC1_GAME_OVL_COMMON_H
// Shared includes and tiny EE-memory accessors for the ovl_*.cpp ports.
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/math/vecmath.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/ee/ps2_fpu.h>

#include <cfloat>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace rac1 {
inline uint8_t lbu(uint32_t a) { return ee_read<uint8_t>(a); }
inline int16_t lh(uint32_t a) { return ee_read<int16_t>(a); }
inline uint16_t lhu(uint32_t a) { return ee_read<uint16_t>(a); }
inline int32_t lw(uint32_t a) { return ee_read<int32_t>(a); }
inline uint32_t lwu(uint32_t a) { return ee_read<uint32_t>(a); }
inline uint64_t ld(uint32_t a) { return ee_read<uint64_t>(a); }
inline float lwc1(uint32_t a) { return ee_read<float>(a); }
inline void sb(uint32_t a, uint32_t v) { ee_write<uint8_t>(a, static_cast<uint8_t>(v)); }
inline void sh(uint32_t a, uint32_t v) { ee_write<uint16_t>(a, static_cast<uint16_t>(v)); }
inline void sw(uint32_t a, uint32_t v) { ee_write<uint32_t>(a, v); }
inline void sd(uint32_t a, uint64_t v) { ee_write<uint64_t>(a, v); }
inline void swc1(uint32_t a, float v) { ee_write<float>(a, v); }
inline float u2f(uint32_t u) { float f; std::memcpy(&f, &u, 4); return f; }
inline uint32_t f2u(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
// Floating point: the ports run with the host FPU rounding toward zero, the EE's mode
// (runtime/ee/ps2_fpu.h: set once by the host and the oracle), so plain float arithmetic and
// fpu_add / fpu_sub / fpu_mul round as the PS2 does; outside the normal range the PS2 rules apply
// (denormal results flush to 0, overflow clamps to +-FLT_MAX, no Inf/NaN). Divisions use fpu_div
// (rounded to nearest, PCSX2's FPUDiv setting), VU code vuc(). Game data stays in range.
// orig: (helper) an EE FPU operand: exponent 255 is an ordinary number (up to 2^129, the PS2
// has no Inf / NaN, game data uses 0x7f800000 as "very large"), denormals read as signed zero.
inline double fpu_in(float f) {
    const uint32_t u = f2u(f);
    const uint32_t m = u & 0x7fffffffu;
    if (__builtin_expect(m >= 0x7f800000u || (m - 1u < 0x007fffffu), 0)) {
        if (m >= 0x7f800000u) {
            const double v = std::ldexp(1.0 + static_cast<double>(u & 0x7fffffu) / 8388608.0, 128);
            return (u >> 31) != 0 ? -v : v;
        }
        return (u >> 31) != 0 ? -0.0 : 0.0;
    }
    return static_cast<double>(f);
}
// orig: (helper) an EE FPU result: beyond the range clamps to +-FLT_MAX, denormals flush.
inline float fpu_out(double r) {
    if (__builtin_expect(std::fabs(r) > static_cast<double>(FLT_MAX), 0)) return std::signbit(r) ? -FLT_MAX : FLT_MAX;
    const float f = static_cast<float>(r);
    const uint32_t u = f2u(f), m = u & 0x7fffffffu;
    if (__builtin_expect(m >= 0x7f800000u || (m - 1u < 0x007fffffu), 0)) {
        return u2f((u & 0x80000000u) | (m >= 0x7f800000u ? 0x7f7fffffu : 0u));
    }
    return f;
}
// orig: (helper) EE FPU DIV.S (rounded to nearest) — x/0 gives +-FLT_MAX; results clamp to +-FLT_MAX, denormals flush.
inline float fpu_div(float s, float t) {
    const double a = fpu_in(s), b = fpu_in(t);
    if (b == 0.0) return (std::signbit(a) != std::signbit(b)) ? -FLT_MAX : FLT_MAX;
    return fpu_out(ps2fpu::div_nearest(a, b));
}
// orig: (helper) EE FPU MUL.S — an overflowing product clamps to +-FLT_MAX, denormals flush.
inline float fpu_mul(float s, float t) { return fpu_out(fpu_in(s) * fpu_in(t)); }
// orig: (helper) EE FPU ADD.S / SUB.S with the PS2 operand and result rules.
inline float fpu_add(float s, float t) { return fpu_out(fpu_in(s) + fpu_in(t)); }
inline float fpu_sub(float s, float t) { return fpu_out(fpu_in(s) - fpu_in(t)); }
// orig: (helper) R5900 DIV quotient (LO): x/0 gives -1 (x >= 0) or 1, INT_MIN/-1 gives INT_MIN; no trap.
inline int32_t ee_div(int32_t a, int32_t b) { return b == 0 ? (a >= 0 ? -1 : 1) : (b == -1 ? static_cast<int32_t>(0u - static_cast<uint32_t>(a)) : a / b); }
// orig: (helper) R5900 DIV remainder (HI): x/0 leaves HI = x, INT_MIN/-1 gives 0.
inline int32_t ee_rem(int32_t a, int32_t b) { return b == 0 ? a : (b == -1 ? 0 : a % b); }
// orig: (helper) VU0 float operand/result rule — denormals are signed zero, Inf/NaN are +-max.
inline float vuc(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    const uint32_t m = u & 0x7fffffffu;
    if (__builtin_expect(m >= 0x7f800000u || (m - 1u < 0x007fffffu), 0)) {
        u = (u & 0x80000000u) | (m >= 0x7f800000u ? 0x7f7fffffu : 0u);
        std::memcpy(&f, &u, 4);
    }
    return f;
}
// Views of EE RAM as the math library's vector / matrix types.
inline Vec4& V(uint32_t a) { return *static_cast<Vec4*>(ee_ram_ptr(a)); }
inline Mat4& M(uint32_t a) { return *static_cast<Mat4*>(ee_ram_ptr(a)); }
inline void lq_sq(uint32_t dst, uint32_t src) {  // 128-bit aligned copy
    ee_write<EeQuad>(dst & ~15u, ee_read<EeQuad>(src & ~15u));
}
// orig: (helper) scratchpad DMA, normal mode, done synchronously: toSPR (ch9, 0x1000d400) copies
// qwc quadwords from RAM madr to the scratchpad at sadr (wrapping in 16 KB); fromSPR (ch8) back.
inline void spr_dma_to(uint32_t sadr, uint32_t madr, uint32_t qwc) {
    for (uint32_t q = 0; q < (qwc & 0xffffu); ++q) lq_sq(0x70000000u + ((sadr + q * 16) & 0x3ff0u), madr + q * 16);
}
inline void spr_dma_from(uint32_t madr, uint32_t sadr, uint32_t qwc) {
    for (uint32_t q = 0; q < (qwc & 0xffffu); ++q) lq_sq(madr + q * 16, 0x70000000u + ((sadr + q * 16) & 0x3ff0u));
}
// orig: (helper) toSPR in source chain mode (CHCR 0x104 / with TIE 0x184): the tags at tadr
// (refe / cnt / next / ref / refs / end) copied on from the scratchpad's sadr
inline void spr_dma_to_chain(uint32_t sadr, uint32_t tadr, uint32_t chcr) {
    for (int guard = 0; guard < 65536; ++guard) {
        const uint64_t tag = ld(tadr);
        const uint32_t qwc = static_cast<uint32_t>(tag & 0xffff), id = static_cast<uint32_t>(tag >> 28) & 7;
        const uint32_t addr = static_cast<uint32_t>(tag >> 32) & 0x7ffffff0u;
        bool end = false;
        uint32_t from = 0;
        switch (id) {
        case 0: from = addr; end = true; break;  // refe
        case 1: from = tadr + 16; tadr += 16 + qwc * 16; break;  // cnt
        case 2: from = tadr + 16; tadr = addr; break;  // next
        case 3: case 4: from = addr; tadr += 16; break;  // ref / refs
        case 7: from = tadr + 16; end = true; break;  // end
        default: return;
        }
        spr_dma_to(sadr, from, qwc);
        sadr = (sadr + qwc * 16) & 0x3ff0u;
        if (end || (((tag >> 31) & 1) != 0 && (chcr & 0x80u) != 0)) return;
    }
}
// orig: (helper) a function's stack frame for the buffers it hands to others by EE address
// (RPC / DMA descriptors, libgraph packets): bytes taken from the top of EE RAM, below the
// original's stack top (32 MB less 64), freed in reverse order.
inline uint32_t& ee_stack_ptr() {
    static uint32_t sp = 0x02000000u - 0x1000u;
    return sp;
}
inline uint32_t ee_stack_alloc(uint32_t n) { return ee_stack_ptr() -= (n + 15u) & ~15u; }
inline void ee_stack_free(uint32_t n) {  // (the bytes cleared: no leftovers for an uninitialised read)
    const uint32_t m = (n + 15u) & ~15u;
    for (uint32_t i = 0; i < m;) {  // (a page's part at a time through one write pointer: the words sw cleared one by one)
        const uint32_t a = ee_stack_ptr() + i, k = std::min(m - i, 0x1000u - (a & 0xfffu));
        std::memset(ee_ram_ptr_write(a, k), 0, k);
        i += k;
    }
    ee_stack_ptr() += m;
}
}  // namespace rac1
#endif
