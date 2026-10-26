// The building blocks of the fast ported micro programs: the same VU
// float rules as vu_native.h's lane-by-lane helpers (operands clamped, results through the MAC
// flag update), done on the four lanes at once with SSE2 integer / float ops -- the results are
// bit-identical (IEEE single rounding, no fused multiply-add, no flush-to-zero mode); the flags
// come out of the lane masks of the result as 4-bit sets instead of per-lane branches.
#ifndef RAC1_RUNTIME_VU_VU_NATIVE_FAST_H
#define RAC1_RUNTIME_VU_VU_NATIVE_FAST_H

#include <runtime/vu/vu_native.h>

#if defined(__SSE2__) || defined(__x86_64__)
#include <emmintrin.h>
#else
#include <runtime/vu/vu_simd_shim.h>  // (GCC vectors: the Wii U's PowerPC)
#endif

namespace rac1::vun {

using I4 = __m128i;

VUN_INLINE I4 ld4(const VUVector& v) { return _mm_load_si128(reinterpret_cast<const __m128i*>(&v)); }
VUN_INLINE void st4(VUVector& v, I4 x) { _mm_store_si128(reinterpret_cast<__m128i*>(&v), x); }
// a register's lane stored in the lanes of mask M (bit l: lane l), the others kept
template <int M>
VUN_INLINE void st4m(VUVector& v, I4 x) {
    if constexpr (M == 15) {
        st4(v, x);
    } else if constexpr (M != 0) {
        const I4 m = _mm_set_epi32((M & 8) != 0 ? -1 : 0, (M & 4) != 0 ? -1 : 0, (M & 2) != 0 ? -1 : 0, (M & 1) != 0 ? -1 : 0);
        st4(v, _mm_or_si128(_mm_and_si128(m, x), _mm_andnot_si128(m, ld4(v))));
    }
}
VUN_INLINE I4 bits4(uint32_t u) { return _mm_set1_epi32(static_cast<int>(u)); }
// an operand: denormals signed zero, Inf / NaN the signed largest float (vu_native.h's cop on four lanes)
VUN_INLINE I4 cop4(I4 x) {
    const I4 expm = _mm_set1_epi32(0x7f800000);
    const I4 e = _mm_and_si128(x, expm);
    const I4 isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const I4 isi = _mm_cmpeq_epi32(e, expm);
    I4 r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), x);
    const I4 big = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32(static_cast<int>(0x80000000u))), _mm_set1_epi32(0x7f7fffff));
    r = _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, big));
    return r;
}
// a quadword of the data memory (16-byte aligned) loaded / stored in the lanes of mask M
VUN_INLINE I4 ldq4(const uint32_t* m) { return _mm_load_si128(reinterpret_cast<const __m128i*>(m)); }
template <int M>
VUN_INLINE void stq4m(uint32_t* m, I4 x) {
    __m128i* d = reinterpret_cast<__m128i*>(m);
    if constexpr (M == 15) {
        _mm_store_si128(d, x);
    } else if constexpr (M != 0) {
        const I4 k = _mm_set_epi32((M & 8) != 0 ? -1 : 0, (M & 4) != 0 ? -1 : 0, (M & 2) != 0 ? -1 : 0, (M & 1) != 0 ? -1 : 0);
        _mm_store_si128(d, _mm_or_si128(_mm_and_si128(k, x), _mm_andnot_si128(k, _mm_load_si128(d))));
    }
}
// one lane broadcast to the four
#define VUN_BC(x, l) _mm_shuffle_epi32((x), (l) * 0x55)
VUN_INLINE I4 vadd(I4 a, I4 b) { return _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
VUN_INLINE I4 vsub(I4 a, I4 b) { return _mm_castps_si128(_mm_sub_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
VUN_INLINE I4 vmul(I4 a, I4 b) { return _mm_castps_si128(_mm_mul_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
VUN_INLINE I4 vq(float f) { return _mm_set1_epi32(static_cast<int>(u_of(copf(f)))); }

// A result through the MAC flag update: the lanes of mask M clamped (the others come back as
// they are, to be discarded), the flags of those lanes set in f (the lanes outside M cleared:
// the whole 16-bit MAC flag is rebuilt, the status flag's sticky bits kept and added to).
template <int M>
VUN_INLINE I4 fin(FlagRegs& f, I4 v) {
    const I4 expm = _mm_set1_epi32(0x7f800000);
    const I4 e = _mm_and_si128(v, expm);
    const I4 isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const I4 isi = _mm_cmpeq_epi32(e, expm);
    const I4 sgn = _mm_and_si128(v, _mm_set1_epi32(static_cast<int>(0x80000000u)));
    I4 r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), v);
    r = _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, _mm_or_si128(sgn, _mm_set1_epi32(0x7f7fffff))));
    const I4 nz = _mm_cmpeq_epi32(_mm_and_si128(v, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128());
    const uint32_t zm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isz))) & M;
    const uint32_t sm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(v))) & M;
    const uint32_t um = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(_mm_andnot_si128(nz, isz)))) & M;
    const uint32_t om = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isi))) & M;
    // the four lane sets (zero, sign, underflow, overflow) as nibbles; the MAC flag has each reversed (lane l at bit 3 - l)
    const uint32_t x = zm | (sm << 4) | (um << 8) | (om << 12);
    f.macflag = ((x & 0x1111u) << 3) | ((x & 0x2222u) << 1) | ((x & 0x4444u) >> 1) | ((x & 0x8888u) >> 3);
    // the status flag's four bits: one per nibble that is not empty
    const uint32_t t = (x | (x >> 1) | (x >> 2) | (x >> 3)) & 0x1111u;
    const uint32_t st = ((t * 0x1248u) >> 12) & 15u;
    f.statusflag = (f.statusflag & 0xfc0) | st | (st << 6);
    return r;
}

}  // namespace rac1::vun

#endif
