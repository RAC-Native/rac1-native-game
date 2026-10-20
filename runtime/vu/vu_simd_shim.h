// The SSE2 subset used by the VU fast paths (vu_native_fast.h, vu0_regs.h),
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
#define _MM_SHUFFLE(z, y, x, w) ((((z) << 6) | ((y) << 4) | ((x) << 2) | (w)))

#endif  // defined(__ARM_NEON) || defined(__aarch64__)

#endif  // RAC1_RUNTIME_VU_SIMD_SHIM_H
