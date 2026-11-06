// (two shims for hosts without SSE2: ARM NEON's - main's - and GCC's generic vectors - the Wii U's PowerPC)
#if defined(__ARM_NEON) || defined(__aarch64__)
// The SSE2 subset used by the VU fast paths (vu_native_fast.h, vu0_regs.h, moby_joints.cpp),
// mapped to ARM NEON twins so the algorithm bodies stay identical across ISAs.
//
// x86 builds keep including <emmintrin.h> directly and are completely untouched:
// this header is only pulled in behind `#if defined(__ARM_NEON) || defined(__aarch64__)`.
//
// Design notes:
// - `__m128i` becomes the NEON 4xint32 vector; every intrinsic used by the VU
//   code is a function-like macro over a small NEON helper, so call sites in
//   the two headers do not change at all.
// - `_mm_movemask_ps` has no NEON equivalent; it is emulated by shifting each
//   lane's sign bit to bit 0 and assembling the 4-bit mask (the VU code only
//   ever masks the result with a lane mask, so bit order [lane0 = bit0] is what
//   matters, matching x86).
// - `_mm_shuffle_epi32` has no single NEON equivalent; the few immediates used
//   (the two opmula/opmsub swizzles and VUN_BC's lane broadcast) are handled by
//   a scalar lane-index helper. Lane order matches _MM_SHUFFLE semantics.
// - All float ops are single IEEE-754 lane ops (no FMA): results are
//   bit-identical to the SSE2 path when both run in the same host FP
//   environment. That environment is the one the game sets up in
//   enter_ee_rounding (runtime/ee/ps2_fpu.h): round toward zero, with
//   denormals flushed (FTZ/DAZ on x86, the FPCR FZ bit on AArch64).
#ifndef RAC1_RUNTIME_VU_SIMD_SHIM_H
#define RAC1_RUNTIME_VU_SIMD_SHIM_H

#if defined(__ARM_NEON) || defined(__aarch64__)

#include <arm_neon.h>

#include <cstdint>

typedef int32x4_t __m128i;

inline int32x4_t neon_set_epi32(int w, int z, int y, int x) {
    const int32x4_t v = {x, y, z, w};
    return v;
}
inline float32x4_t neon_set_ps_f(float w, float z, float y, float x) {
    const float32x4_t v = {x, y, z, w};
    return v;
}
// x86 _mm_movemask_ps: bit i = sign of float lane i.
inline uint32_t neon_movemask_ps(float32x4_t v) {
    const uint32x4_t s = vshrq_n_u32(vreinterpretq_u32_f32(v), 31);
    return vgetq_lane_u32(s, 0) | (vgetq_lane_u32(s, 1) << 1) | (vgetq_lane_u32(s, 2) << 2) |
           (vgetq_lane_u32(s, 3) << 3);
}
// x86 _mm_shuffle_epi32(v, imm): out lane i = in lane (imm >> 2*i) & 3.
inline int32x4_t neon_shuffle_epi32(int32x4_t v, int imm) {
    int32_t s[4];
    vst1q_s32(s, v);
    const int32x4_t r = {s[imm & 3], s[(imm >> 2) & 3], s[(imm >> 4) & 3], s[(imm >> 6) & 3]};
    return r;
}
// x86 _mm_movemask_epi8: bit i = sign of byte lane i.
inline int neon_movemask_epi8(int32x4_t v) {
    const uint8x16_t b = vshrq_n_u8(vreinterpretq_u8_s32(v), 7);
    static const uint8_t w[16] = {1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128};
    const uint8x16_t m = vmulq_u8(b, vld1q_u8(w));
    return static_cast<int>(vaddv_u8(vget_low_u8(m)) | (static_cast<uint32_t>(vaddv_u8(vget_high_u8(m))) << 8));
}
// x86 _mm_packs_epi32 / _mm_packs_epi16: signed saturation to the narrower lanes, a's lanes first.
inline int32x4_t neon_packs_epi32(int32x4_t a, int32x4_t b) {
    return vreinterpretq_s32_s16(vcombine_s16(vqmovn_s32(a), vqmovn_s32(b)));
}
inline int32x4_t neon_packs_epi16(int32x4_t a, int32x4_t b) {
    return vreinterpretq_s32_s8(vcombine_s8(vqmovn_s16(vreinterpretq_s16_s32(a)), vqmovn_s16(vreinterpretq_s16_s32(b))));
}
// x86 _mm_srai_epi32 / _mm_slli_epi32 (a count past 31: all sign bits / zero, as SSE2).
inline int32x4_t neon_srai_epi32(int32x4_t v, int n) { return vshlq_s32(v, vdupq_n_s32(-(n > 31 ? 31 : n))); }
inline int32x4_t neon_slli_epi32(int32x4_t v, int n) { return n > 31 ? vdupq_n_s32(0) : vshlq_s32(v, vdupq_n_s32(n)); }
// x86 _mm_cvtsi64_si128: the low 64 bits, the high ones zero.
inline int32x4_t neon_cvtsi64_si128(long long x) {
    return vreinterpretq_s32_s64(vcombine_s64(vcreate_s64(static_cast<uint64_t>(x)), vdup_n_s64(0)));
}
// x86 _mm_unpacklo_epi16: the low four halfwords of a and b interleaved (a first).
inline int32x4_t neon_unpacklo_epi16(int32x4_t a, int32x4_t b) {
    return vreinterpretq_s32_s16(vzip1q_s16(vreinterpretq_s16_s32(a), vreinterpretq_s16_s32(b)));
}

#define _mm_set_epi32(w, z, y, x) neon_set_epi32((w), (z), (y), (x))
#define _mm_set1_epi32(x) vdupq_n_s32((x))
#define _mm_setzero_si128() vdupq_n_s32(0)
#define _mm_set_ps(w, z, y, x) vreinterpretq_s32_f32(neon_set_ps_f((w), (z), (y), (x)))
#define _mm_load_si128(p) vld1q_s32(reinterpret_cast<const int32_t*>(p))
#define _mm_store_si128(p, x) vst1q_s32(reinterpret_cast<int32_t*>(p), (x))
#define _mm_store_ps(p, x) vst1q_f32(reinterpret_cast<float*>(p), vreinterpretq_f32_s32((x)))
#define _mm_and_si128(a, b) vandq_s32((a), (b))
#define _mm_or_si128(a, b) vorrq_s32((a), (b))
#define _mm_andnot_si128(a, b) vbicq_s32((b), (a))
#define _mm_cmpeq_epi32(a, b) vreinterpretq_s32_u32(vceqq_s32((a), (b)))
#define _mm_castsi128_ps(a) vreinterpretq_f32_s32((a))
#define _mm_castps_si128(a) vreinterpretq_s32_f32((a))
#define _mm_add_ps(a, b) vaddq_f32((a), (b))
#define _mm_sub_ps(a, b) vsubq_f32((a), (b))
#define _mm_mul_ps(a, b) vmulq_f32((a), (b))
#define _mm_movemask_ps(a) static_cast<int>(neon_movemask_ps((a)))
#define _mm_shuffle_epi32(v, imm) neon_shuffle_epi32((v), (imm))
#define _mm_add_epi32(a, b) vaddq_s32((a), (b))
#define _mm_srai_epi32(v, n) neon_srai_epi32((v), (n))
#define _mm_slli_epi32(v, n) neon_slli_epi32((v), (n))
#define _mm_packs_epi32(a, b) neon_packs_epi32((a), (b))
#define _mm_packs_epi16(a, b) neon_packs_epi16((a), (b))
#define _mm_movemask_epi8(a) neon_movemask_epi8((a))
// (a byte shift right of the whole register; the count a constant, as SSE2's immediate)
#define _mm_srli_si128(v, n) \
    vreinterpretq_s32_u8(vextq_u8(vreinterpretq_u8_s32((v)), vdupq_n_u8(0), (n)))
#define _mm_cvtsi64_si128(x) neon_cvtsi64_si128((x))
#define _mm_cvtsi128_si32(v) vgetq_lane_s32((v), 0)
#define _mm_unpacklo_epi16(a, b) neon_unpacklo_epi16((a), (b))
// (SCVTF rounds in FPCR's mode, as cvtdq2ps in MXCSR's)
#define _mm_cvtepi32_ps(v) vcvtq_f32_s32((v))
#define _mm_set1_ps(x) vdupq_n_f32((x))
#define _MM_SHUFFLE(z, y, x, w) ((((z) << 6) | ((y) << 4) | ((x) << 2) | (w)))

#endif  // defined(__ARM_NEON) || defined(__aarch64__)

#endif  // RAC1_RUNTIME_VU_SIMD_SHIM_H
#else
// The SSE2 subset used by the VU fast paths (vu_native_fast.h, vu0_regs.h), for hosts without SSE2: GCC's
// generic vector types, which the compiler lowers to whatever the target has (four scalar operations on the
// Wii U's PowerPC 750). x86 builds include <emmintrin.h> instead and are untouched.
//
// Every operation is defined by the lanes' values, never by their bytes in memory, so the results are the same
// on a big-endian host: a "byte k" of a vector (_mm_packs_epi16, _mm_movemask_epi8) is bits 8 * (k % 4) .. of
// lane k / 4, as x86 lays it out. The float operations are single IEEE operations per lane (no fused
// multiply-add: -ffp-contract=off), so they round as the SSE2 ones do in the same float environment
// (enter_ee_rounding, runtime/ee/ps2_fpu.h).
#ifndef RAC1_RUNTIME_VU_SIMD_SHIM_H
#define RAC1_RUNTIME_VU_SIMD_SHIM_H

#include <cstdint>
#include <cstring>

// (the vectors are passed by value between inline functions only: no ABI to keep)
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wpsabi"

typedef int32_t __m128i __attribute__((vector_size(16), aligned(16)));
typedef float __m128 __attribute__((vector_size(16), aligned(16)));

inline __m128i _mm_set_epi32(int w, int z, int y, int x) { return __m128i{x, y, z, w}; }
inline __m128i _mm_set1_epi32(int x) { return __m128i{x, x, x, x}; }
inline __m128i _mm_setzero_si128() { return __m128i{0, 0, 0, 0}; }
inline __m128 _mm_set_ps(float w, float z, float y, float x) { return __m128{x, y, z, w}; }
inline __m128i _mm_load_si128(const __m128i* p) { return *p; }
inline void _mm_store_si128(__m128i* p, __m128i x) { *p = x; }
inline void _mm_store_ps(float* p, __m128 x) { std::memcpy(p, &x, 16); }

inline __m128i _mm_and_si128(__m128i a, __m128i b) { return a & b; }
inline __m128i _mm_or_si128(__m128i a, __m128i b) { return a | b; }
inline __m128i _mm_andnot_si128(__m128i a, __m128i b) { return ~a & b; }
inline __m128i _mm_cmpeq_epi32(__m128i a, __m128i b) { return a == b; }
inline __m128i _mm_add_epi32(__m128i a, __m128i b) {  // (wrapping, as SSE2's)
    return reinterpret_cast<__m128i>(reinterpret_cast<uint32_t __attribute__((vector_size(16)))>(a) +
                                      reinterpret_cast<uint32_t __attribute__((vector_size(16)))>(b));
}
inline __m128i _mm_slli_epi32(__m128i a, int n) {
    return reinterpret_cast<__m128i>(reinterpret_cast<uint32_t __attribute__((vector_size(16)))>(a) << n);
}
inline __m128i _mm_srai_epi32(__m128i a, int n) { return a >> (n > 31 ? 31 : n); }

inline __m128 _mm_castsi128_ps(__m128i a) { return reinterpret_cast<__m128>(a); }
inline __m128i _mm_castps_si128(__m128 a) { return reinterpret_cast<__m128i>(a); }
inline __m128 _mm_add_ps(__m128 a, __m128 b) { return a + b; }
inline __m128 _mm_sub_ps(__m128 a, __m128 b) { return a - b; }
inline __m128 _mm_mul_ps(__m128 a, __m128 b) { return a * b; }

// bit i = the sign of lane i
inline int _mm_movemask_ps(__m128 a) {
    const __m128i u = reinterpret_cast<__m128i>(a);
    return static_cast<int>((static_cast<uint32_t>(u[0]) >> 31) | ((static_cast<uint32_t>(u[1]) >> 31) << 1) |
                            ((static_cast<uint32_t>(u[2]) >> 31) << 2) | ((static_cast<uint32_t>(u[3]) >> 31) << 3));
}
// bit k = the top bit of byte k (byte k: bits 8 * (k % 4) .. of lane k / 4)
inline int _mm_movemask_epi8(__m128i a) {
    uint32_t m = 0;
    for (int k = 0; k < 16; k++) m |= ((static_cast<uint32_t>(a[k >> 2]) >> (8 * (k & 3) + 7)) & 1u) << k;
    return static_cast<int>(m);
}

inline int32_t rac1_sat16(int32_t v) { return v < -32768 ? -32768 : (v > 32767 ? 32767 : v); }
inline int32_t rac1_sat8(int32_t v) { return v < -128 ? -128 : (v > 127 ? 127 : v); }
// the eight lanes saturated to 16 bits: a's in halves 0..3, b's in 4..7 (half h: bits 16 * (h % 2) .. of lane h / 2)
inline __m128i _mm_packs_epi32(__m128i a, __m128i b) {
    const int32_t s[8] = {a[0], a[1], a[2], a[3], b[0], b[1], b[2], b[3]};
    __m128i r;
    for (int j = 0; j < 4; j++)
        r[j] = static_cast<int32_t>((static_cast<uint32_t>(rac1_sat16(s[2 * j])) & 0xffffu) |
                                    (static_cast<uint32_t>(rac1_sat16(s[2 * j + 1])) << 16));
    return r;
}
// the sixteen 16-bit halves saturated to 8 bits: a's in bytes 0..7, b's in 8..15
inline __m128i _mm_packs_epi16(__m128i a, __m128i b) {
    int32_t s[16];
    for (int h = 0; h < 8; h++) {
        s[h] = static_cast<int16_t>(static_cast<uint32_t>(a[h >> 1]) >> (16 * (h & 1)));
        s[8 + h] = static_cast<int16_t>(static_cast<uint32_t>(b[h >> 1]) >> (16 * (h & 1)));
    }
    __m128i r = {0, 0, 0, 0};
    for (int k = 0; k < 16; k++)
        r[k >> 2] = static_cast<int32_t>(static_cast<uint32_t>(r[k >> 2]) |
                                         ((static_cast<uint32_t>(rac1_sat8(s[k])) & 0xffu) << (8 * (k & 3))));
    return r;
}

// out lane i = in lane (imm >> 2i) & 3
// (moby_joints.cpp's poses) a 64-bit value into lanes 0 (its low word) and 1, the others zero; lane 0 out
inline __m128i _mm_cvtsi64_si128(long long x) {
    const uint64_t u = static_cast<uint64_t>(x);
    return __m128i{static_cast<int32_t>(u), static_cast<int32_t>(u >> 32), 0, 0};
}
inline int _mm_cvtsi128_si32(__m128i a) { return a[0]; }
// the low four 16-bit halves of a and b interleaved: lane j holds a's half j (low) and b's half j (high)
inline __m128i _mm_unpacklo_epi16(__m128i a, __m128i b) {
    __m128i r;
    for (int j = 0; j < 4; j++) {
        const uint32_t ha = (static_cast<uint32_t>(a[j >> 1]) >> (16 * (j & 1))) & 0xffffu;
        const uint32_t hb = (static_cast<uint32_t>(b[j >> 1]) >> (16 * (j & 1))) & 0xffffu;
        r[j] = static_cast<int32_t>(ha | (hb << 16));
    }
    return r;
}
// the lanes' integers as floats (rounded in the float environment, as cvtdq2ps by MXCSR)
inline __m128 _mm_cvtepi32_ps(__m128i a) {
    return __m128{static_cast<float>(a[0]), static_cast<float>(a[1]), static_cast<float>(a[2]), static_cast<float>(a[3])};
}
inline __m128 _mm_set1_ps(float x) { return __m128{x, x, x, x}; }
// the vector shifted right by n bytes (byte k of the result: byte k + n of a, zero past the 16th)
inline __m128i _mm_srli_si128(__m128i a, int n) {
    __m128i r = {0, 0, 0, 0};
    for (int k = 0; k + n < 16; k++) {
        const uint32_t b = (static_cast<uint32_t>(a[(k + n) >> 2]) >> (8 * ((k + n) & 3))) & 0xffu;
        r[k >> 2] = static_cast<int32_t>(static_cast<uint32_t>(r[k >> 2]) | (b << (8 * (k & 3))));
    }
    return r;
}

#define _mm_shuffle_epi32(v, imm) \
    (__m128i{(v)[(imm) & 3], (v)[((imm) >> 2) & 3], (v)[((imm) >> 4) & 3], (v)[((imm) >> 6) & 3]})
#define _MM_SHUFFLE(z, y, x, w) ((((z) << 6) | ((y) << 4) | ((x) << 2) | (w)))

#pragma GCC diagnostic pop

#endif
#endif
