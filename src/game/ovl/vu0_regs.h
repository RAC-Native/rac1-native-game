// The VU0 macro ops on the shared register file (vu0m::regs), as the hand-written routines
// issue them: lane-masked, broadcast, ACC forms, with the VU float rules.
#ifndef RAC1_GAME_OVL_VU0_REGS_H
#define RAC1_GAME_OVL_VU0_REGS_H

#include <game/ovl/vu0_macro.h>
#include <emmintrin.h>

#include <concepts>

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
// orig: (helper) the status flag's four current bits (Z S U O: any lane's) from the MAC flags
inline uint32_t status_bits(uint32_t mac) {
    const uint32_t t = (mac | (mac >> 1) | (mac >> 2) | (mac >> 3)) & 0x1111u;
    return ((t * 0x1248u) >> 12) & 15u;
}

// The FMAC flags of a routine's ops kept in locals (registers) and written back to the VU0 at the scope's end: the same
// values the ops would have left in the registers one by one, without the store and reload of each op's flags through the
// VU0's state. An op leaves its flag bytes (fin_m: zero, sign, underflow, overflow per lane, its written lanes only) as
// `last` and ORs them into `sticky`; the MAC flags are the last op's, the status flag's current bits the last op's and its
// sticky bits every op's (an op sets status = (status & 0xfc0) | st | st << 6: after ops 1..n, (S0 & 0xfc0) | (st1 | .. |
// stn) << 6 | stn). sync() makes them words again (opmula, which keeps a lane's MAC flags, needs them). Only for a stretch of
// macro ops that never reads the flags from the VU0 state, nor runs a VU0 micro program (vcallms, ctc2) or anything that
// does (moby_joints_pose: its ops' flags are read by nobody before it returns).
// orig: (host)
struct LocalFlags {
    Regs& R;
    uint32_t macflag, statusflag;
    __m128i last = _mm_setzero_si128(), sticky = _mm_setzero_si128();
    bool pending = false;  // ops since the last sync
    explicit LocalFlags(Regs& r) : R(r), macflag(r.macflag), statusflag(r.statusflag) {}
    void sync() {
        if (!pending) return;
        macflag = static_cast<uint32_t>(_mm_movemask_epi8(last));
        const uint32_t any = status_bits(static_cast<uint32_t>(_mm_movemask_epi8(sticky)));
        statusflag = (statusflag & 0xfc0) | (any << 6) | status_bits(macflag);
        sticky = _mm_setzero_si128();
        pending = false;
    }
    ~LocalFlags() {
        sync();
        R.macflag = macflag;
        R.statusflag = statusflag;
    }
    LocalFlags(const LocalFlags&) = delete;
    LocalFlags& operator=(const LocalFlags&) = delete;
};
template <class F>
concept FlagSink = std::same_as<F, Regs> || std::same_as<F, LocalFlags>;

// orig: (helper) a lane mask's lanes in the flags' order (x the high bit of each nibble)
constexpr uint32_t mask_rev(int m) {
    return static_cast<uint32_t>(((m & 1) << 3) | ((m & 2) << 1) | ((m & 4) >> 1) | ((m & 8) >> 3));
}
// orig: (helper) an FMAC result clamped (a zero exponent: the signed zero; the largest: the signed largest float) and its
// MAC / status flags for the written lanes `m` (zero, sign, underflow, overflow; the lanes not written clear); the
// status flag's sticky bits kept. The flags are built on the lanes reversed (w first: the flag's bit order) and packed
// into one mask with a single movemask.
template <FlagSink F>
[[gnu::always_inline]] inline __m128i fin_m(F& R, __m128i v, int m) {
    const __m128i vr = _mm_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3));  // (w z y x)
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(vr, expm);
    const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const __m128i isi = _mm_cmpeq_epi32(e, expm);
    const __m128i sgn = _mm_srai_epi32(vr, 31);
    // (a zero exponent: the signed zero; the largest: the mantissa all ones, the exponent one less, the sign kept)
    __m128i r = _mm_or_si128(vr, _mm_and_si128(isi, _mm_set1_epi32(0x007fffff)));
    r = _mm_add_epi32(r, _mm_slli_epi32(isi, 23));
    r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), r);
    const __m128i den = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(vr, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128()), isz);  // (underflow)
    // the four masks' lanes as bytes: zero (bits 0..3), sign (4..7), underflow (8..11), overflow (12..15), w..x in each
    const __m128i b = _mm_packs_epi16(_mm_packs_epi32(isz, sgn), _mm_packs_epi32(den, isi));
    if constexpr (std::same_as<F, LocalFlags>) {  // (the flag bytes of the written lanes kept: LocalFlags)
        const __m128i k = _mm_set1_epi32(static_cast<int>(((m & 8) != 0 ? 0xffu : 0u) | ((m & 4) != 0 ? 0xff00u : 0u) |
                                                          ((m & 2) != 0 ? 0xff0000u : 0u) | ((m & 1) != 0 ? 0xff000000u : 0u)));
        const __m128i x = _mm_and_si128(b, k);
        R.last = x;
        R.sticky = _mm_or_si128(R.sticky, x);
        R.pending = true;
    } else {
        const uint32_t x = static_cast<uint32_t>(_mm_movemask_epi8(b)) & (mask_rev(m) * 0x1111u);
        R.macflag = x;
        const uint32_t st = status_bits(x);
        R.statusflag = (R.statusflag & 0xfc0) | st | (st << 6);
    }
    return _mm_shuffle_epi32(r, _MM_SHUFFLE(0, 1, 2, 3));
}
template <FlagSink F>
[[gnu::always_inline]] inline void fmac4(F& fl, Vf& d, int m, __m128i raw) {
    const __m128i r = fin_m(fl, raw, m);
    if (__builtin_expect(&d == regs().vf.data(), 0)) {
        _mm_store_ps(d.v, _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));
    } else {
        st4_m(d, r, m);
    }
}
template <FlagSink F>
[[gnu::always_inline]] inline void fmac4_acc(F& fl, int m, __m128i raw) {
    const __m128i r = fin_m(fl, raw, m);
    st4_m(regs().acc, r, m);
}
inline void fmac4(Vf& d, int m, __m128i raw) { fmac4(regs(), d, m, raw); }
inline void fmac4_acc(int m, __m128i raw) { fmac4_acc(regs(), m, raw); }

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
// orig: (helper) VADD / VSUB / VMUL (lane by lane) and their broadcast forms. Each op in two forms: on the VU0's flags,
// and on a LocalFlags (`fl` first: the same op, its flags kept in the scope's locals)
template <FlagSink F> [[gnu::always_inline]] inline void add(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac4(fl, d, m, vadd4(cop4(ld4(a)), cop4(ld4(b)))); }
template <FlagSink F> [[gnu::always_inline]] inline void sub(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac4(fl, d, m, vsub4(cop4(ld4(a)), cop4(ld4(b)))); }
template <FlagSink F> [[gnu::always_inline]] inline void mul(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac4(fl, d, m, vmul4(cop4(ld4(a)), cop4(ld4(b)))); }
template <FlagSink F> [[gnu::always_inline]] inline void addbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac4(fl, d, m, vadd4(cop4(ld4(a)), bc4(b))); }
template <FlagSink F> [[gnu::always_inline]] inline void subbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac4(fl, d, m, vsub4(cop4(ld4(a)), bc4(b))); }
template <FlagSink F> [[gnu::always_inline]] inline void mulbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac4(fl, d, m, vmul4(cop4(ld4(a)), bc4(b))); }
inline void add(Vf& d, const Vf& a, const Vf& b, int m) { add(regs(), d, a, b, m); }
inline void sub(Vf& d, const Vf& a, const Vf& b, int m) { sub(regs(), d, a, b, m); }
inline void mul(Vf& d, const Vf& a, const Vf& b, int m) { mul(regs(), d, a, b, m); }
inline void addbc(Vf& d, const Vf& a, float b, int m) { addbc(regs(), d, a, b, m); }
inline void subbc(Vf& d, const Vf& a, float b, int m) { subbc(regs(), d, a, b, m); }
inline void mulbc(Vf& d, const Vf& a, float b, int m) { mulbc(regs(), d, a, b, m); }
// orig: (helper) VADDA / VMULA / VMADDA (broadcast) into ACC, VMADD from ACC
template <FlagSink F> [[gnu::always_inline]] inline void addabc(F& fl, const Vf& a, float b, int m) { fmac4_acc(fl, m, vadd4(cop4(ld4(a)), bc4(b))); }
template <FlagSink F> [[gnu::always_inline]] inline void mulabc(F& fl, const Vf& a, float b, int m) { fmac4_acc(fl, m, vmul4(cop4(ld4(a)), bc4(b))); }
template <FlagSink F> [[gnu::always_inline]] inline void maddabc(F& fl, const Vf& a, float b, int m) {
    fmac4_acc(fl, m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
inline void addabc(const Vf& a, float b, int m) { addabc(regs(), a, b, m); }
inline void mulabc(const Vf& a, float b, int m) { mulabc(regs(), a, b, m); }
inline void maddabc(const Vf& a, float b, int m) { maddabc(regs(), a, b, m); }
// orig: (helper) VMADD (broadcast): ACC + a b
template <FlagSink F> [[gnu::always_inline]] inline void maddbc(F& fl, Vf& d, const Vf& a, float b, int m) {
    fmac4(fl, d, m, vadd4(cop4(ld4(acc())), vmul4(cop4(ld4(a)), bc4(b))));
}
inline void maddbc(Vf& d, const Vf& a, float b, int m) { maddbc(regs(), d, a, b, m); }
// orig: (helper) VMULAx / VMADDAy / VMADDz: d = r0 p.x + r1 p.y + r2 p.z on the masked lanes
template <FlagSink F> [[gnu::always_inline]] inline void rotate(F& fl, Vf& d, int r0, const Vf& p, int m) {
    const Vf s = p;
    mulabc(fl, vf()[r0], s.v[0], m);
    maddabc(fl, vf()[r0 + 1], s.v[1], m);
    maddbc(fl, d, vf()[r0 + 2], s.v[2], m);
}
inline void rotate(Vf& d, int r0, const Vf& p, int m) { rotate(regs(), d, r0, p, m); }
// orig: (helper) the same with VMADDAz / VMADDw: d = r0 p.x + r1 p.y + r2 p.z + r3 w
template <FlagSink F> [[gnu::always_inline]] inline void transform(F& fl, Vf& d, int r0, const Vf& p, float w) {
    const Vf s = p;
    mulabc(fl, vf()[r0], s.v[0], XYZW);
    maddabc(fl, vf()[r0 + 1], s.v[1], XYZW);
    maddabc(fl, vf()[r0 + 2], s.v[2], XYZW);
    maddbc(fl, d, vf()[r0 + 3], w, XYZW);
}
inline void transform(Vf& d, int r0, const Vf& p, float w) { transform(regs(), d, r0, p, w); }
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
template <FlagSink F> [[gnu::always_inline]] inline void opmula(F& fl, const Vf& a, const Vf& b) {  // (w's flags kept)
    if constexpr (std::same_as<F, LocalFlags>) fl.sync();
    const Vf s = a, t = b;
    for (int i = 0; i < 3; ++i) acc().v[i] = mac_lane_in(fl.macflag, i, vuc(s.v[(i + 1) % 3]) * vuc(t.v[(i + 2) % 3]));
    fl.statusflag = status_of(fl.macflag, fl.statusflag);
}
template <FlagSink F> [[gnu::always_inline]] inline void opmsub(F& fl, Vf& d, const Vf& a, const Vf& b) {
    const __m128i sa = cop4(ld4(a)), sb = cop4(ld4(b)), c = cop4(ld4(acc()));
    const __m128i a_yzx = _mm_shuffle_epi32(sa, _MM_SHUFFLE(3, 0, 2, 1));
    const __m128i b_zxy = _mm_shuffle_epi32(sb, _MM_SHUFFLE(3, 1, 0, 2));
    fmac4(fl, d, XYZ, vsub4(c, vmul4(a_yzx, b_zxy)));
}
inline void opmula(const Vf& a, const Vf& b) { opmula(regs(), a, b); }
inline void opmsub(Vf& d, const Vf& a, const Vf& b) { opmsub(regs(), d, a, b); }
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
