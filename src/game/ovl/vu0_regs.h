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

#include <runtime/ee/ps2_fpu.h>

#include <concepts>

namespace rac1::vu0m {

enum : int { X = 1, Y = 2, Z = 4, W = 8, XY = 3, XZ = 5, YZ = 6, XYZ = 7, XYZW = 15 };

RAC1_HOT_INLINE Vf* vf() { return regs().vf.data(); }
RAC1_HOT_INLINE Vf& acc() { return regs().acc; }
RAC1_HOT_INLINE float& q() { return regs().q; }

RAC1_HOT_INLINE __m128i ld4(const Vf& v) { return _mm_load_si128(reinterpret_cast<const __m128i*>(&v)); }
RAC1_HOT_INLINE void st4_m(Vf& d, __m128i x, int m) {
    __m128i* dp = reinterpret_cast<__m128i*>(&d);
    if (m == 15) {
        _mm_store_si128(dp, x);
    } else if (m != 0) {
        const __m128i k = _mm_set_epi32((m & 8) != 0 ? -1 : 0, (m & 4) != 0 ? -1 : 0, (m & 2) != 0 ? -1 : 0, (m & 1) != 0 ? -1 : 0);
        _mm_store_si128(dp, _mm_or_si128(_mm_and_si128(k, x), _mm_andnot_si128(k, _mm_load_si128(dp))));
    }
}
RAC1_HOT_INLINE __m128i cop4(__m128i x) {
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(x, expm);
    const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
    const __m128i isi = _mm_cmpeq_epi32(e, expm);
    __m128i r = _mm_andnot_si128(_mm_and_si128(isz, _mm_set1_epi32(0x7fffffff)), x);
    const __m128i big = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32(static_cast<int>(0x80000000u))), _mm_set1_epi32(0x7f7fffff));
    return _mm_or_si128(_mm_andnot_si128(isi, r), _mm_and_si128(isi, big));
}
RAC1_HOT_INLINE __m128i bc4(float b) { return _mm_set1_epi32(static_cast<int>(f2u(vuc(b)))); }
RAC1_HOT_INLINE __m128i vadd4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_add_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
RAC1_HOT_INLINE __m128i vsub4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_sub_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
RAC1_HOT_INLINE __m128i vmul4(__m128i a, __m128i b) { return _mm_castps_si128(_mm_mul_ps(_mm_castsi128_ps(a), _mm_castsi128_ps(b))); }
// ---- The raw forms (RAC1_PERF_VU0_RAW; runtime/ee/ps2_fpu.h) ----
// In the thread's mode (toward zero, no denormals) the host's own add, subtract and multiply of the lanes as they are give the
// result of the VU rules - the operands clamped, the op, the result clamped - unless an operand lane has exponent 255, and then
// the raw result's lane has it too (an infinity or a NaN). So an FMAC op computes the raw lanes, and only when a lane it writes
// has exponent 255 computes them again by the rules, out of line. A result the rules leave as it is has no denormal lane
// either, so its flags are the zero and the sign bits alone.
namespace raw {
enum : int { kAdd, kSub, kMul, kMadd, kMsub };
// orig: (host) the raw forms on (RAC1_PERF_VU0_RAW at 1, the thread's mode the EE's)
[[gnu::always_inline]] inline bool on() { return __builtin_expect(ps2fpu::g_vu0_raw == 1, 1); }
// orig: (helper) no ACC operand; a broadcast operand's lanes as they are
[[gnu::always_inline]] inline __m128i none() { return _mm_setzero_si128(); }
[[gnu::always_inline]] inline __m128i bcr(float b) { return _mm_set1_epi32(static_cast<int>(f2u(b))); }
// orig: (helper) the op on the lanes given: x op y, or c +- x y (the product rounded on its own, then the sum)
template <int K>
[[gnu::always_inline]] inline __m128i calc(__m128i x, __m128i y, __m128i c) {
    if constexpr (K == kAdd) {
        return vadd4(x, y);
    } else if constexpr (K == kSub) {
        return vsub4(x, y);
    } else if constexpr (K == kMul) {
        return vmul4(x, y);
    } else if constexpr (K == kMadd) {
        return vadd4(c, vmul4(x, y));
    } else {
        return vsub4(c, vmul4(x, y));
    }
}
inline __m128i calc_k(int k, __m128i x, __m128i y, __m128i c) {
    switch (k) {
    case kAdd: return calc<kAdd>(x, y, c);
    case kSub: return calc<kSub>(x, y, c);
    case kMul: return calc<kMul>(x, y, c);
    case kMadd: return calc<kMadd>(x, y, c);
    default: return calc<kMsub>(x, y, c);
    }
}
// orig: (helper) a raw result has exponent 255 in one of the lanes `m`
[[gnu::always_inline]] inline bool bad(__m128i r, int m) {
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    return (_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_and_si128(r, expm), expm))) & m) != 0;
}
// orig: (helper) the op by the VU rules, lane by lane: the operands clamped, the op, the result clamped (the form every op had:
// reached for an operand with exponent 255)
[[gnu::noinline, gnu::cold]] inline __m128i rules(int k, __m128i x, __m128i y, __m128i c) { return cop4(calc_k(k, cop4(x), cop4(y), cop4(c))); }
// orig: (host) RAC1_PERF_VU0_RAW=2: an op the rules computed, against its raw form: the lanes `m` the same words, and no
// underflow or overflow flag (ps2_fpu.h's account)
[[gnu::noinline, gnu::cold]] inline void check(int k, __m128i x, __m128i y, __m128i c, int m) {
    ps2fpu::RawCheck& a = ps2fpu::g_vu0_raw_check;
    if (!ps2fpu::thread_chops()) a.off_thread.fetch_add(1, std::memory_order_relaxed);
    const __m128i r = calc_k(k, x, y, c);
    if (bad(r, m)) {
        a.by_rules.fetch_add(1, std::memory_order_relaxed);
        return;
    }
    a.compared.fetch_add(1, std::memory_order_relaxed);
    const __m128i o = calc_k(k, cop4(x), cop4(y), cop4(c));  // (the rules' result before its clamp: what the flags are made of)
    const __m128i expm = _mm_set1_epi32(0x7f800000);
    const __m128i e = _mm_and_si128(o, expm);
    const __m128i den = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(o, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128()), _mm_cmpeq_epi32(e, _mm_setzero_si128()));
    const __m128i wrong = _mm_or_si128(_mm_or_si128(den, _mm_cmpeq_epi32(e, expm)), _mm_xor_si128(_mm_cmpeq_epi32(r, cop4(o)), _mm_set1_epi32(-1)));
    const int lanes = _mm_movemask_ps(_mm_castsi128_ps(wrong)) & m;
    if (lanes == 0) return;
    alignas(16) uint32_t xw[4], yw[4], cw[4], rw[4], ow[4];
    _mm_store_si128(reinterpret_cast<__m128i*>(xw), x);
    _mm_store_si128(reinterpret_cast<__m128i*>(yw), y);
    _mm_store_si128(reinterpret_cast<__m128i*>(cw), c);
    _mm_store_si128(reinterpret_cast<__m128i*>(rw), r);
    _mm_store_si128(reinterpret_cast<__m128i*>(ow), o);
    const int l = __builtin_ctz(static_cast<unsigned>(lanes));
    a.differ(static_cast<uint32_t>(k), static_cast<uint32_t>(lanes), xw[l], yw[l], cw[l], rw[l], ow[l]);
}
// orig: (helper) an op's lanes with no flags (vu0_nf.h's and vu0_local.h's ops): raw, by the rules where a lane of `m` asks; or
// as before, operand by operand (the raw forms off)
template <int K>
[[gnu::always_inline]] inline __m128i value(__m128i x, __m128i y, __m128i c, int m) {
    if (on()) {
        __m128i r = calc<K>(x, y, c);
        if (__builtin_expect(bad(r, m), 0)) r = rules(K, x, y, c);
        return r;
    }
    const __m128i r = cop4(calc<K>(cop4(x), cop4(y), cop4(c)));
    if (__builtin_expect(ps2fpu::g_vu0_raw == 2, 0)) check(K, x, y, c, m);
    return r;
}
}  // namespace raw

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
// orig: (helper) an op's flag bytes `b` (fin_m's: zero, sign, underflow, overflow per lane, w first) into the sink, the lanes `m`
template <FlagSink F>
[[gnu::always_inline]] inline void fin_flags(F& R, __m128i b, int m) {
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
}
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
    fin_flags(R, _mm_packs_epi16(_mm_packs_epi32(isz, sgn), _mm_packs_epi32(den, isi)), m);
    return _mm_shuffle_epi32(r, _MM_SHUFFLE(0, 1, 2, 3));
}
// orig: (helper) fin_m's flags of a result the rules leave as it is (the raw forms: no lane of `m` with exponent 255, and in the
// thread's mode no denormal): the zero and the sign bits, the underflow and overflow ones clear; nothing to clamp
template <FlagSink F>
[[gnu::always_inline]] inline void flags_raw(F& R, __m128i v, int m) {
    const __m128i vr = _mm_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3));  // (w z y x)
    const __m128i isz = _mm_cmpeq_epi32(_mm_and_si128(vr, _mm_set1_epi32(0x7f800000)), _mm_setzero_si128());
    fin_flags(R, _mm_packs_epi16(_mm_packs_epi32(isz, _mm_srai_epi32(vr, 31)), _mm_setzero_si128()), m);
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
// orig: (helper) one FMAC op on the lanes x, y (c: ACC's, for VMADD / VMSUB) as the registers hold them, into the lanes `m` of d
// (fmac_acc_k: of ACC), its flags into fl. The raw form (see raw above), or as before: the operands clamped, fin_m
template <int K, FlagSink F>
[[gnu::always_inline]] inline void fmac_k(F& fl, Vf& d, int m, __m128i x, __m128i y, __m128i c) {
    if (raw::on()) {
        __m128i r = raw::calc<K>(x, y, c);
        if (__builtin_expect(raw::bad(r, m), 0)) r = raw::rules(K, x, y, c);
        flags_raw(fl, r, m);
        if (__builtin_expect(&d == regs().vf.data(), 0)) {
            _mm_store_ps(d.v, _mm_set_ps(1.0f, 0.0f, 0.0f, 0.0f));
        } else {
            st4_m(d, r, m);
        }
        return;
    }
    fmac4(fl, d, m, raw::calc<K>(cop4(x), cop4(y), cop4(c)));
    if (__builtin_expect(ps2fpu::g_vu0_raw == 2, 0)) raw::check(K, x, y, c, m);
}
template <int K, FlagSink F>
[[gnu::always_inline]] inline void fmac_acc_k(F& fl, int m, __m128i x, __m128i y, __m128i c) {
    if (raw::on()) {
        __m128i r = raw::calc<K>(x, y, c);
        if (__builtin_expect(raw::bad(r, m), 0)) r = raw::rules(K, x, y, c);
        flags_raw(fl, r, m);
        st4_m(regs().acc, r, m);
        return;
    }
    fmac4_acc(fl, m, raw::calc<K>(cop4(x), cop4(y), cop4(c)));
    if (__builtin_expect(ps2fpu::g_vu0_raw == 2, 0)) raw::check(K, x, y, c, m);
}

// orig: (helper) a lane-masked VU0 op: the lanes computed from the operands first, then written
template <class F>
RAC1_HOT_INLINE void lanes(Vf& d, int m, F f) {
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
template <FlagSink F> [[gnu::always_inline]] inline void add(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac_k<raw::kAdd>(fl, d, m, ld4(a), ld4(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void sub(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac_k<raw::kSub>(fl, d, m, ld4(a), ld4(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void mul(F& fl, Vf& d, const Vf& a, const Vf& b, int m) { fmac_k<raw::kMul>(fl, d, m, ld4(a), ld4(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void addbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac_k<raw::kAdd>(fl, d, m, ld4(a), raw::bcr(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void subbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac_k<raw::kSub>(fl, d, m, ld4(a), raw::bcr(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void mulbc(F& fl, Vf& d, const Vf& a, float b, int m) { fmac_k<raw::kMul>(fl, d, m, ld4(a), raw::bcr(b), raw::none()); }
inline void add(Vf& d, const Vf& a, const Vf& b, int m) { add(regs(), d, a, b, m); }
inline void sub(Vf& d, const Vf& a, const Vf& b, int m) { sub(regs(), d, a, b, m); }
inline void mul(Vf& d, const Vf& a, const Vf& b, int m) { mul(regs(), d, a, b, m); }
inline void addbc(Vf& d, const Vf& a, float b, int m) { addbc(regs(), d, a, b, m); }
inline void subbc(Vf& d, const Vf& a, float b, int m) { subbc(regs(), d, a, b, m); }
inline void mulbc(Vf& d, const Vf& a, float b, int m) { mulbc(regs(), d, a, b, m); }
// orig: (helper) VADDA / VMULA / VMADDA (broadcast) into ACC, VMADD from ACC
template <FlagSink F> [[gnu::always_inline]] inline void addabc(F& fl, const Vf& a, float b, int m) { fmac_acc_k<raw::kAdd>(fl, m, ld4(a), raw::bcr(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void mulabc(F& fl, const Vf& a, float b, int m) { fmac_acc_k<raw::kMul>(fl, m, ld4(a), raw::bcr(b), raw::none()); }
template <FlagSink F> [[gnu::always_inline]] inline void maddabc(F& fl, const Vf& a, float b, int m) {
    fmac_acc_k<raw::kMadd>(fl, m, ld4(a), raw::bcr(b), ld4(acc()));
}
inline void addabc(const Vf& a, float b, int m) { addabc(regs(), a, b, m); }
inline void mulabc(const Vf& a, float b, int m) { mulabc(regs(), a, b, m); }
inline void maddabc(const Vf& a, float b, int m) { maddabc(regs(), a, b, m); }
// orig: (helper) VMADD (broadcast): ACC + a b
template <FlagSink F> [[gnu::always_inline]] inline void maddbc(F& fl, Vf& d, const Vf& a, float b, int m) {
    fmac_k<raw::kMadd>(fl, d, m, ld4(a), raw::bcr(b), ld4(acc()));
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
RAC1_HOT_INLINE float vmax(float a, float b) {
    const int32_t x = static_cast<int32_t>(f2u(a)), y = static_cast<int32_t>(f2u(b));
    return u2f(static_cast<uint32_t>((x < 0 && y < 0) ? (x < y ? x : y) : (x > y ? x : y)));
}
// orig: (helper) VFTOI0 on the masked lanes
inline void ftoi0(Vf& d, int m) {
    lanes(d, m, [&](int i) { return bits(ftoi(d.v[i], 1.0f)); });
}
// orig: (helper) VITOFn of one lane
RAC1_HOT_INLINE float itof(float bits, float scale) { return static_cast<float>(static_cast<int32_t>(f2u(bits))) * scale; }
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
    fmac_k<raw::kMsub>(regs(), d, m, ld4(a), raw::bcr(b), ld4(acc()));
}
// orig: (helper) VMADD / VMSUB (vector): ACC +- a b
inline void maddv(Vf& d, const Vf& a, const Vf& b, int m) {
    fmac_k<raw::kMadd>(regs(), d, m, ld4(a), ld4(b), ld4(acc()));
}
inline void msubv(Vf& d, const Vf& a, const Vf& b, int m) {
    fmac_k<raw::kMsub>(regs(), d, m, ld4(a), ld4(b), ld4(acc()));
}
// orig: (helper) VMULA / VADDA / VSUBA / VMADDA / VMSUBA (vector) and VSUBA / VMSUBA (broadcast)
inline void mulav(const Vf& a, const Vf& b, int m) { fmac_acc_k<raw::kMul>(regs(), m, ld4(a), ld4(b), raw::none()); }
inline void addav(const Vf& a, const Vf& b, int m) { fmac_acc_k<raw::kAdd>(regs(), m, ld4(a), ld4(b), raw::none()); }
inline void subav(const Vf& a, const Vf& b, int m) { fmac_acc_k<raw::kSub>(regs(), m, ld4(a), ld4(b), raw::none()); }
inline void subabc(const Vf& a, float b, int m) { fmac_acc_k<raw::kSub>(regs(), m, ld4(a), raw::bcr(b), raw::none()); }
inline void maddav(const Vf& a, const Vf& b, int m) {
    fmac_acc_k<raw::kMadd>(regs(), m, ld4(a), ld4(b), ld4(acc()));
}
inline void msubav(const Vf& a, const Vf& b, int m) {
    fmac_acc_k<raw::kMsub>(regs(), m, ld4(a), ld4(b), ld4(acc()));
}
inline void msubabc(const Vf& a, float b, int m) {
    fmac_acc_k<raw::kMsub>(regs(), m, ld4(a), raw::bcr(b), ld4(acc()));
}
// orig: (helper) VMINI on float bits (both negative: the larger word)
RAC1_HOT_INLINE float vmini(float a, float b) {
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
    const __m128i a_yzx = _mm_shuffle_epi32(ld4(a), _MM_SHUFFLE(3, 0, 2, 1));
    const __m128i b_zxy = _mm_shuffle_epi32(ld4(b), _MM_SHUFFLE(3, 1, 0, 2));
    fmac_k<raw::kMsub>(fl, d, XYZ, a_yzx, b_zxy, ld4(acc()));
}
inline void opmula(const Vf& a, const Vf& b) { opmula(regs(), a, b); }
inline void opmsub(Vf& d, const Vf& a, const Vf& b) { opmsub(regs(), d, a, b); }
// orig: (helper) VITOFn / VFTOIn on the masked lanes
RAC1_HOT_INLINE void itofv(Vf& d, const Vf& s, int m, float scale) {
    const Vf t = s;
    lanes(d, m, [&](int i) { return itof(t.v[i], scale); });
}
RAC1_HOT_INLINE void ftoiv(Vf& d, const Vf& s, int m, float scale) {
    const Vf t = s;
    lanes(d, m, [&](int i) { return bits(ftoi(t.v[i], scale)); });
}
// orig: (helper) VCLIPw.xyz into the clip flags
RAC1_HOT_INLINE void vclip(const Vf& p, float w) { clipw(regs().clipflag, p, w); }

}  // namespace rac1::vu0m

#endif
