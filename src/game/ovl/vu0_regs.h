// The VU0 macro ops on the shared register file (vu0m::regs), as the hand-written routines
// issue them: lane-masked, broadcast, ACC forms, with the VU float rules.
#ifndef RAC1_GAME_OVL_VU0_REGS_H
#define RAC1_GAME_OVL_VU0_REGS_H

#include <game/ovl/vu0_macro.h>
// The macro ops below run on four lanes at once with SSE2 integer / float ops
// on x86; on ARM NEON the same call sites map to NEON twins, keeping the x86
// path byte-identical (runtime/vu/vu_simd_shim.h).
#if defined(__ARM_NEON) || defined(__aarch64__)
#include <runtime/vu/vu_simd_shim.h>
#else
#include <emmintrin.h>
#endif

namespace rac1::vu0m {

enum : int { X = 1, Y = 2, Z = 4, W = 8, XY = 3, XZ = 5, YZ = 6, XYZ = 7, XYZW = 15 };

inline Vf* vf() { return regs().vf.data(); }
inline Vf& acc() { return regs().acc; }
inline float& q() { return regs().q; }

inline __m128i ld4(const Vf& v) { return _mm_load_si128(reinterpret_cast<const __m128i*>(&v)); }
inline void st4_m(Vf& d, __m128i x, int m) {
    __m128i* dp = reinterpret_cast<__m128i*>(&d);
    if (m == 15) {
        _mm_store_si128(dp, x);
    } else if (m != 0) {
        const __m128i k = _mm_set_epi32((m & 8) != 0 ? -1 : 0, (m & 4) != 0 ? -1 : 0, (m & 2) != 0 ? -1 : 0, (m & 1) != 0 ? -1 : 0);
        _mm_store_si128(dp, _mm_or_si128(_mm_and_si128(k, x), _mm_andnot_si128(k, _mm_load_si128(dp))));
    }
}
inline __m128i cop4(__m128i x) {
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(x, expm);
    const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const __m128i isi = _mm_cmpeq_epi32(e, expm);
    __m128i r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), x);
    const __m128i big = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32(static_cast<int>(0x80000000u))), _mm_set1_epi32(0x7f7fffff));
    return _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, big));
}
inline __m128i bc4(float b) { return _mm_set1_epi32(static_cast<int>(f2u(vuc(b)))); }
inline __m128i vadd4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
inline __m128i vsub4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_sub_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
inline __m128i vmul4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_mul_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
inline __m128i fin_m(Regs& R, __m128i v, int m) {
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(v, expm);
    const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const __m128i isi = _mm_cmpeq_epi32(e, expm);
    const __m128i sgn = _mm_and_si128(v, _mm_set1_epi32(static_cast<int>(0x80000000u)));
    __m128i r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), v);
    r = _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, _mm_or_si128(sgn, _mm_set1_epi32(0x7f7fffff))));
    const __m128i nz = _mm_cmpeq_epi32(_mm_and_si128(v, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128());
    const uint32_t mu = static_cast<uint32_t>(m);
    const uint32_t zm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isz))) & mu;
    const uint32_t sm = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(v))) & mu;
    const uint32_t um = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(_mm_andnot_si128(nz, isz)))) & mu;
    const uint32_t om = static_cast<uint32_t>(_mm_movemask_ps(_mm_castsi128_ps(isi))) & mu;
    const uint32_t x = zm | (sm << 4) | (um << 8) | (om << 12);
    R.macflag = ((x & 0x1111u) << 3) | ((x & 0x2222u) << 1) | ((x & 0x4444u) >> 1) | ((x & 0x8888u) >> 3);
    const uint32_t t = (x | (x >> 1) | (x >> 2) | (x >> 3)) & 0x1111u;
    const uint32_t st = ((t * 0x1248u) >> 12) & 15u;
    R.statusflag = (R.statusflag & 0xfc0) | st | (st << 6);
    return r;
}
inline void fmac4(Vf& d, int m, __m128i raw) {
    Regs& R = regs();
    const __m128i r = fin_m(R, raw, m);
    if (__builtin_expect(&d == R.vf.data(), 0)) {
        _mm_store_ps(d.v, _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));
    } else {
        st4_m(d, r, m);
    }
}
inline void fmac4_acc(int m, __m128i raw) {
    Regs& R = regs();
    const __m128i r = fin_m(R, raw, m);
    st4_m(R.acc, r, m);
}

// orig: (helper) a lane-masked VU0 op: the lanes computed from the operands first, then written
template <class F>
inline void lanes(Vf& d, int m, F f) {
    float r[4];
    for (int i = 0; i < 4; ++i) r[i] = (m >> i & 1) != 0 ? f(i) : d.v[i];
    for (int i = 0; i < 4; ++i) d.v[i] = r[i];
}
// orig: (helper) a result lane's MAC flags (sign, zero, underflow, overflow; the lane clamped)
// and the status flag after the op, as the FMAC sets them; lanes not written are cleared
// (the flags built in a local word, the common normal result without branches; the same bits as before)
inline float mac_lane_in(uint32_t& mac, int i, float raw) {
    const uint32_t v = f2u(raw), sh = static_cast<uint32_t>(3 - i);
    const uint32_t e = (v >> 23) & 0xffu;
    uint32_t b = (v >> 31) << 4;  // S
    float out = raw;
    if (e - 1u >= 254u) {  // (e 0 or 255: rare)
        if (e == 0) {
            b |= 1u;  // Z (a zero; a denormal also U, the result a signed zero)
            if ((v & 0x7fffffu) != 0) {
                b |= 0x100u;
                out = u2f(v & 0x80000000u);
            }
        } else {
            b |= 0x1000u;  // O, the result clamped
            out = u2f((v & 0x80000000u) | 0x7f7fffffu);
        }
    }
    mac = (mac & ~(0x1111u << sh)) | (b << sh);
    return out;
}
inline float mac_lane(int i, float raw) { return mac_lane_in(regs().macflag, i, raw); }
inline uint32_t status_of(uint32_t mac, uint32_t status) {
    uint32_t st = 0;
    if ((mac & 0x000f) != 0) st |= 1;
    if ((mac & 0x00f0) != 0) st |= 2;
    if ((mac & 0x0f00) != 0) st |= 4;
    if ((mac & 0xf000) != 0) st |= 8;
    return (status & 0xfc0) | st | (st << 6);
}
inline void mac_status() { regs().statusflag = status_of(regs().macflag, regs().statusflag); }
// orig: (helper) an FMAC op: the raw lanes (operands clamped), flags and clamp on the written ones
template <class F>
inline void fmac(Vf& d, int m, F raw) {
    Regs& R = regs();
    uint32_t mac = R.macflag;
    float r[4];
    for (int i = 0; i < 4; ++i) {
        if ((m >> i & 1) != 0) r[i] = mac_lane_in(mac, i, raw(i));
        else {
            mac &= ~(0x1111u << (3 - i));
            r[i] = d.v[i];
        }
    }
    for (int i = 0; i < 4; ++i) d.v[i] = r[i];
    if (&d == R.vf.data()) d.v[0] = 0.0f, d.v[1] = 0.0f, d.v[2] = 0.0f, d.v[3] = 1.0f;  // (vf0 stays constant; the flags of the op are set: vsubx.w vf0 is a compare)
    R.macflag = mac;
    R.statusflag = status_of(mac, R.statusflag);
}
// orig: (helper) VADD / VSUB / VMUL (lane by lane) and their broadcast forms
inline void add(Vf& d, const Vf& a, const Vf& b, int m) { fmac4(d, m, vadd4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void sub(Vf& d, const Vf& a, const Vf& b, int m) { fmac4(d, m, vsub4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void mul(Vf& d, const Vf& a, const Vf& b, int m) { fmac4(d, m, vmul4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void addbc(Vf& d, const Vf& a, float b, int m) { fmac4(d, m, vadd4(cop4(ld4(a)), bc4(b))); }
inline void subbc(Vf& d, const Vf& a, float b, int m) { fmac4(d, m, vsub4(cop4(ld4(a)), bc4(b))); }
inline void mulbc(Vf& d, const Vf& a, float b, int m) { fmac4(d, m, vmul4(cop4(ld4(a)), bc4(b))); }
// orig: (helper) VADDA / VMULA / VMADDA (broadcast) into ACC, VMADD from ACC
inline void addabc(const Vf& a, float b, int m) { fmac4_acc(m, vadd4(cop4(ld4(a)), bc4(b))); }
inline void mulabc(const Vf& a, float b, int m) { fmac4_acc(m, vmul4(cop4(ld4(a)), bc4(b))); }
inline void maddabc(const Vf& a, float b, int m) {
    fmac4_acc(m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
// orig: (helper) VMADD (broadcast): ACC + a b
inline void maddbc(Vf& d, const Vf& a, float b, int m) {
    fmac4(d, m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
// orig: (helper) VMULAx / VMADDAy / VMADDz: d = r0 p.x + r1 p.y + r2 p.z on the masked lanes
inline void rotate(Vf& d, int r0, const Vf& p, int m) {
    const Vf s = p;
    mulabc(vf()[r0], s.v[0], m);
    maddabc(vf()[r0 + 1], s.v[1], m);
    maddbc(d, vf()[r0 + 2], s.v[2], m);
}
// orig: (helper) the same with VMADDAz / VMADDw: d = r0 p.x + r1 p.y + r2 p.z + r3 w
inline void transform(Vf& d, int r0, const Vf& p, float w) {
    const Vf s = p;
    mulabc(vf()[r0], s.v[0], XYZW);
    maddabc(vf()[r0 + 1], s.v[1], XYZW);
    maddabc(vf()[r0 + 2], s.v[2], XYZW);
    maddbc(d, vf()[r0 + 3], w, XYZW);
}
// orig: (helper) VMAX on float bits (both negative: the smaller word)
inline float vmax(float a, float b) {
    const int32_t x = static_cast<int32_t>(f2u(a)), y = static_cast<int32_t>(f2u(b));
    return u2f(static_cast<uint32_t>((x < 0 && y < 0) ? (x < y ? x : y) : (x > y ? x : y)));
}
// orig: (helper) VFTOI0 on the masked lanes
inline void ftoi0(Vf& d, int m) {
    lanes(d, m, [&](int i) { return bits(ftoi(d.v[i], 1.0f)); });
}
// orig: (helper) VITOFn of one lane
inline float itof(float bits, float scale) { return static_cast<float>(static_cast<int32_t>(f2u(bits))) * scale; }
// orig: (helper) VSQRT Q = sqrt(|t|)
inline float vsqrt(float t) { return vuc(std::sqrt(std::fabs(vuc(t)))); }
// orig: (helper) VRSQRT Q = s / sqrt(|t|) (by zero: the signed largest float, 0 / 0 a signed 0)
inline float vrsqrt(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) {
        const bool n = ((f2u(s) ^ f2u(t)) & 0x80000000u) != 0;
        if (a != 0.0f) return u2f(n ? 0xff7fffffu : 0x7f7fffffu);
        return u2f(n ? 0x80000000u : 0u);
    }
    return vuc(a / std::sqrt(std::fabs(b)));
}
// orig: (helper) QMFC2's low doubleword (x | y << 32)
inline uint64_t qlo(const Vf& v) { return f2u(v.v[0]) | static_cast<uint64_t>(f2u(v.v[1])) << 32; }
// orig: (helper) QMTC2's low doubleword into x / y (the high half: lanes nothing reads)
inline void qset_xy(Vf& d, uint64_t lo) {
    d.v[0] = u2f(static_cast<uint32_t>(lo));
    d.v[1] = u2f(static_cast<uint32_t>(lo >> 32));
}

// orig: (helper) the remaining macro forms: ACC-vector ops and their broadcast / Q / I kinds
inline void msubbc(Vf& d, const Vf& a, float b, int m) {
    fmac4(d, m, vsub4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
// orig: (helper) VMADD / VMSUB (vector): ACC +- a b
inline void maddv(Vf& d, const Vf& a, const Vf& b, int m) {
    fmac4(d, m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), cop4(ld4(b)))));
}
inline void msubv(Vf& d, const Vf& a, const Vf& b, int m) {
    fmac4(d, m, vsub4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), cop4(ld4(b)))));
}
// orig: (helper) VMULA / VADDA / VSUBA / VMADDA / VMSUBA (vector) and VSUBA / VMSUBA (broadcast)
inline void mulav(const Vf& a, const Vf& b, int m) { fmac4_acc(m, vmul4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void addav(const Vf& a, const Vf& b, int m) { fmac4_acc(m, vadd4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void subav(const Vf& a, const Vf& b, int m) { fmac4_acc(m, vsub4(cop4(ld4(a)), cop4(ld4(b)))); }
inline void subabc(const Vf& a, float b, int m) { fmac4_acc(m, vsub4(cop4(ld4(a)), bc4(b))); }
inline void maddav(const Vf& a, const Vf& b, int m) {
    fmac4_acc(m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), cop4(ld4(b)))));
}
inline void msubav(const Vf& a, const Vf& b, int m) {
    fmac4_acc(m, vsub4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), cop4(ld4(b)))));
}
inline void msubabc(const Vf& a, float b, int m) {
    fmac4_acc(m, vsub4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
// orig: (helper) VMINI on float bits (both negative: the larger word)
inline float vmini(float a, float b) {
    const int32_t x = static_cast<int32_t>(f2u(a)), y = static_cast<int32_t>(f2u(b));
    return u2f(static_cast<uint32_t>((x < 0 && y < 0) ? (x > y ? x : y) : (x < y ? x : y)));
}
// orig: (helper) VOPMULA / VOPMSUB (the cross product's two halves; w's flags cleared)
inline void opmula(const Vf& a, const Vf& b) {  // (w's flags kept)
    const Vf s = a, t = b;
    for (int i = 0; i < 3; ++i) acc().v[i] = mac_lane(i, vuc(s.v[(i + 1) % 3]) * vuc(t.v[(i + 2) % 3]));
    mac_status();
}
inline void opmsub(Vf& d, const Vf& a, const Vf& b) {
    const __m128i sa = cop4(ld4(a)), sb = cop4(ld4(b)), c = cop4(ld4(acc()));
    const __m128i a_yzx = _mm_shuffle_epi32(sa, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128i b_zxy = _mm_shuffle_epi32(sb, _MM_SHUFFLE(3, 1, 0, 2));
    fmac4(d, XYZ, vsub4(c, vmul4(a_yzx, b_zxy)));
}
// orig: (helper) VITOFn / VFTOIn on the masked lanes
inline void itofv(Vf& d, const Vf& s, int m, float scale) {
    const Vf t = s;
    lanes(d, m, [&](int i) { return itof(t.v[i], scale); });
}
inline void ftoiv(Vf& d, const Vf& s, int m, float scale) {
    const Vf t = s;
    lanes(d, m, [&](int i) { return bits(ftoi(t.v[i], scale)); });
}
// orig: (helper) VCLIPw.xyz into the clip flags
inline void vclip(const Vf& p, float w) { clipw(regs().clipflag, p, w); }

}  // namespace rac1::vu0m

#endif
