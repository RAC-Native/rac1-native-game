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
#include <bit>
#include <cmath>
#include <cstdint>
#include <cstring>

namespace rac1 {
RAC1_HOT_INLINE uint8_t lbu(uint32_t a) { return ee_read<uint8_t>(a); }
RAC1_HOT_INLINE int16_t lh(uint32_t a) { return ee_read<int16_t>(a); }
RAC1_HOT_INLINE uint16_t lhu(uint32_t a) { return ee_read<uint16_t>(a); }
RAC1_HOT_INLINE int32_t lw(uint32_t a) { return ee_read<int32_t>(a); }
RAC1_HOT_INLINE uint32_t lwu(uint32_t a) { return ee_read<uint32_t>(a); }
RAC1_HOT_INLINE uint64_t ld(uint32_t a) { return ee_read<uint64_t>(a); }
RAC1_HOT_INLINE float lwc1(uint32_t a) { return ee_read<float>(a); }
RAC1_HOT_INLINE void sb(uint32_t a, uint32_t v) { ee_write<uint8_t>(a, static_cast<uint8_t>(v)); }
RAC1_HOT_INLINE void sh(uint32_t a, uint32_t v) { ee_write<uint16_t>(a, static_cast<uint16_t>(v)); }
RAC1_HOT_INLINE void sw(uint32_t a, uint32_t v) { ee_write<uint32_t>(a, v); }
RAC1_HOT_INLINE void sd(uint32_t a, uint64_t v) { ee_write<uint64_t>(a, v); }
RAC1_HOT_INLINE void swc1(uint32_t a, float v) { ee_write<float>(a, v); }
RAC1_HOT_INLINE float u2f(uint32_t u) { return std::bit_cast<float>(u); }
RAC1_HOT_INLINE uint32_t f2u(float f) { return std::bit_cast<uint32_t>(f); }
// Floating point: the ports run with the host FPU rounding toward zero, the EE's mode
// (runtime/ee/ps2_fpu.h: set once by the host and the oracle), so plain float arithmetic and
// fpu_add / fpu_sub / fpu_mul round as the PS2 does; outside the normal range the PS2 rules apply
// (denormal results flush to 0, overflow clamps to +-FLT_MAX, no Inf/NaN). Divisions use fpu_div
// (rounded to nearest, PCSX2's FPUDiv setting), VU code vuc(). Game data stays in range.
// orig: (helper) an EE FPU operand: exponent 255 is an ordinary number (up to 2^129, the PS2
// has no Inf / NaN, game data uses 0x7f800000 as "very large"), denormals read as signed zero.
#if defined(RAC1_VUC_FPU)
[[gnu::noinline, gnu::cold]] inline double fpu_in_rare(float f);
inline double fpu_in(float f) {
    // (RAC1_VUC_FPU, testing - off: measured slower on the console, see vuc below: a normal number or a zero decided on the FPU,
    // returned as it is, as below; the rest out of line, the same rules)
    const float a = __builtin_fabsf(f);
    if (__builtin_expect(a <= 3.40282347e+38f && (a >= 1.17549435e-38f || a == 0.0f), 1)) return static_cast<double>(f);
    return fpu_in_rare(f);
}
inline double fpu_in_rare(float f) {
#else
RAC1_HOT_INLINE double fpu_in(float f) {
#endif
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
RAC1_HOT_INLINE float fpu_out(double r) {
    if (__builtin_expect(std::fabs(r) > static_cast<double>(FLT_MAX), 0)) return std::signbit(r) ? -FLT_MAX : FLT_MAX;
    const float f = static_cast<float>(r);
#if defined(RAC1_VUC_FPU)
    {  // (RAC1_VUC_FPU, as fpu_in: a normal result or a zero decided on the FPU, kept)
        const float a = __builtin_fabsf(f);
        if (__builtin_expect(a <= 3.40282347e+38f && (a >= 1.17549435e-38f || a == 0.0f), 1)) return f;
    }
#endif
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
RAC1_HOT_INLINE float fpu_mul(float s, float t) { return fpu_out(fpu_in(s) * fpu_in(t)); }
// orig: (helper) EE FPU ADD.S / SUB.S with the PS2 operand and result rules.
RAC1_HOT_INLINE float fpu_add(float s, float t) { return fpu_out(fpu_in(s) + fpu_in(t)); }
RAC1_HOT_INLINE float fpu_sub(float s, float t) { return fpu_out(fpu_in(s) - fpu_in(t)); }
// orig: (helper) R5900 DIV quotient (LO): x/0 gives -1 (x >= 0) or 1, INT_MIN/-1 gives INT_MIN; no trap.
inline int32_t ee_div(int32_t a, int32_t b) { return b == 0 ? (a >= 0 ? -1 : 1) : (b == -1 ? static_cast<int32_t>(0u - static_cast<uint32_t>(a)) : a / b); }
// orig: (helper) R5900 DIV remainder (HI): x/0 leaves HI = x, INT_MIN/-1 gives 0.
inline int32_t ee_rem(int32_t a, int32_t b) { return b == 0 ? a : (b == -1 ? 0 : a % b); }
// orig: (helper) VU0 float operand/result rule — denormals are signed zero, Inf/NaN are +-max.
RAC1_HOT_INLINE float vuc_bits(float f) {
    const uint32_t u = std::bit_cast<uint32_t>(f);
    const uint32_t m = u & 0x7fffffffu;
    if (__builtin_expect(m >= 0x7f800000u || (m - 1u < 0x007fffffu), 0)) return std::bit_cast<float>((u & 0x80000000u) | (m >= 0x7f800000u ? 0x7f7fffffu : 0u));
    return f;
}
#if defined(RAC1_VUC_FPU)
// (RAC1_VUC_FPU, testing: the common case decided on the FPU - a normal finite number is returned as it is, as vuc_bits does; the
// rest (zeros, denormals, infinities, NaNs: any comparison of a NaN is false) by vuc_bits, out of line. The same value always. Off:
// on the console (2026-11-01, Kerwan, 1500 steps not in real time) the game's code 22.9 ms a step with it, 20.6 without - the 750's
// trip through memory is cheap, the zeros' (common) call out of line is not)
[[gnu::noinline, gnu::cold]] inline float vuc_rare(float f) { return vuc_bits(f); }
inline float vuc(float f) {
    const float a = __builtin_fabsf(f);
    if (__builtin_expect(a >= 1.17549435e-38f && a <= 3.40282347e+38f, 1)) return f;
    return vuc_rare(f);
}
#else
RAC1_HOT_INLINE float vuc(float f) { return vuc_bits(f); }
#endif
// Views of EE RAM as the math library's vector / matrix types.
RAC1_HOT_INLINE Vec4& V(uint32_t a) { return *static_cast<Vec4*>(ee_ram_ptr(a)); }
RAC1_HOT_INLINE Mat4& M(uint32_t a) { return *static_cast<Mat4*>(ee_ram_ptr(a)); }
RAC1_HOT_INLINE void lq_sq(uint32_t dst, uint32_t src) {  // 128-bit aligned copy
    ee_write<EeQuad>(dst & ~15u, ee_read<EeQuad>(src & ~15u));
}
// orig: (helper) scratchpad DMA, normal mode, done synchronously: toSPR (ch9, 0x1000d400) copies
// qwc quadwords from RAM madr to the scratchpad at sadr (wrapping in 16 KB); fromSPR (ch8) back.
inline void spr_dma_to(uint32_t sadr, uint32_t madr, uint32_t qwc) {
    // (host) one copy when the quadwords neither wrap in the scratchpad nor leave one host block of the source: the same bytes as
    // the quadword loop (the skinning's poses, ~1.3% of Kerwan's game thread on the Wii U quadword by quadword)
    const uint32_t n = (qwc & 0xffffu) * 16, s0 = sadr & 0x3ff0u;
    if (n != 0 && s0 + n <= 0x4000u && (madr & 15u) == 0) {
        const uint8_t* src = static_cast<const uint8_t*>(ee_ram_ptr_read(madr));
        if (src != nullptr && static_cast<const uint8_t*>(ee_ram_ptr_read(madr + n - 1)) == src + n - 1) {
            if (void* dst = ee_ram_ptr_write(0x70000000u + s0, n); dst != nullptr) {
                std::memmove(dst, src, n);
                return;
            }
        }
    }
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
