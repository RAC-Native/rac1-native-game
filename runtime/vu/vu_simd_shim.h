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
