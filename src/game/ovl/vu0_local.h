// The VU0 registers as a routine's local copy, for hand-written rewrites of macro-mode (COP2) routines that keep the original's
// arithmetic: the 32 VF registers, ACC, Q and the MAC / status / clip flags are read from the VU0 once (load), the routine's ops
// run on the copy, and the copy is written back at its end (store), so the VU0 is left as the routine leaves it. Each op is
// vu0_nf.h's, lane for lane (and takes its raw form as they do: vu0_regs.h's raw): the operands clamped by the VU rules (a zero exponent the signed zero, the largest the signed largest
// float), the op (a VMADD's product rounded on its own, then the sum), the result clamped the same way, the masked lanes only, ACC
// written by the ACC forms, no flags made - except an op into vf0, which is kept for its flags (vf0 itself does not change), as
// vu0_nf.h runs it. VuF, for a routine whose transliteration runs vu0_regs.h's ops (it reads the MAC flags after ordinary ops),
// makes every op's flags too, kept as bytes and made words where they are read. What a rewrite gains: no call, no shared register
// file in memory, no "vf0 or switched off" test per op. A routine that runs a VU0 micro program writes the copy back first and
// reads it again after.
// A routine using this must not run while a VU0 micro program runs (its macro ops would wait for the program; the original then),
// nor with RAC1_PERF_VU0_NF=0 (every op flagged: the original then). See usable().
// orig: (host)
#ifndef RAC1_GAME_OVL_VU0_LOCAL_H
#define RAC1_GAME_OVL_VU0_LOCAL_H

#include <game/ovl/vu0_nf.h>
#include <game/ovl/vu0_regs.h>
#include <runtime/vu/vu0.h>

#include <cstdint>

namespace rac1::vu0l {

using vu0m::Vf;
namespace raw = vu0m::raw;

// orig: (host) the lane masks (x the low bit, as vu0m's)
enum : int { X = 1, Y = 2, Z = 4, W = 8, XY = 3, XZ = 5, YZ = 6, XYZ = 7, XYZW = 15 };

// orig: (host) whether a rewrite may run on a local copy now: no micro program running, the flag-less ops on
[[gnu::always_inline]] inline bool usable() { return !vu0::running() && vu0nf::detail::g_on; }

// orig: (host) the copy. Flags = false: vu0_nf.h's ops (no flags but an op into vf0's). Flags = true: vu0_regs.h's ops, every
// FMAC op's MAC and status flags made as the VU0 makes them, for a routine whose transliteration runs those (a file that reads
// the MAC flags after an ordinary op: vu0_nf_pass.py leaves it on vu0_regs.h's ops). The flags are kept as vu0_regs.h's LocalFlags
// keeps them (the last op's flag bytes and every op's ORed) and made words when read (macflags()) and at store().
template <bool Flags>
struct VuT {
    Vf vf[32];
    Vf acc;
    float q;
    uint32_t mac, status, clip;
    __m128i last = _mm_setzero_si128(), sticky = _mm_setzero_si128();  // (Flags: the flag bytes since the last sync)
    bool pending = false;

    void load() {
        const VUState& s = vu0::global().st;
        for (int i = 0; i < 32; ++i) vf[i] = s.vf[static_cast<size_t>(i)];
        acc = s.acc;
        q = s.q;
        mac = s.macflag;
        status = s.statusflag;
        clip = s.clipflag;
        pending = false;
        sticky = _mm_setzero_si128();
    }
    // the MAC / status flags as words (Flags: from the ops' flag bytes; CFC2 16 / 17)
    [[gnu::always_inline]] void sync() {
        if constexpr (Flags) {
            if (!pending) return;
            mac = static_cast<uint32_t>(_mm_movemask_epi8(last));
            const uint32_t any = vu0m::status_bits(static_cast<uint32_t>(_mm_movemask_epi8(sticky)));
            status = (status & 0xfc0) | (any << 6) | vu0m::status_bits(mac);
            sticky = _mm_setzero_si128();
            pending = false;
        }
    }
    [[gnu::always_inline]] uint32_t macflags() {
        sync();
        return mac;
    }
    void store() {
        sync();
        VUState& s = vu0::global().st;
        for (int i = 0; i < 32; ++i) s.vf[static_cast<size_t>(i)] = vf[i];
        s.acc = acc;
        s.q = q;
        s.macflag = mac;
        s.statusflag = status;
        s.clipflag = clip;
    }

    // --- the FMAC ops (vu0_nf.h's, on the copy)
    // d = a op b / d = a op b.bc, the masked lanes
    [[gnu::always_inline]] void add(int d, int a, int b, int m) { fin<raw::kAdd>(d, ld(a), ld(b), raw::none(), m); }
    [[gnu::always_inline]] void sub(int d, int a, int b, int m) { fin<raw::kSub>(d, ld(a), ld(b), raw::none(), m); }
    [[gnu::always_inline]] void mul(int d, int a, int b, int m) { fin<raw::kMul>(d, ld(a), ld(b), raw::none(), m); }
    [[gnu::always_inline]] void addbc(int d, int a, float b, int m) { fin<raw::kAdd>(d, ld(a), raw::bcr(b), raw::none(), m); }
    [[gnu::always_inline]] void subbc(int d, int a, float b, int m) { fin<raw::kSub>(d, ld(a), raw::bcr(b), raw::none(), m); }
    [[gnu::always_inline]] void mulbc(int d, int a, float b, int m) { fin<raw::kMul>(d, ld(a), raw::bcr(b), raw::none(), m); }
    // d = ACC + a b.bc
    [[gnu::always_inline]] void maddbc(int d, int a, float b, int m) { fin<raw::kMadd>(d, ld(a), raw::bcr(b), ldacc(), m); }
    // ACC = a op b.bc / ACC + a b.bc
    [[gnu::always_inline]] void addabc(int a, float b, int m) { finacc<raw::kAdd>(ld(a), raw::bcr(b), raw::none(), m); }
    [[gnu::always_inline]] void mulabc(int a, float b, int m) { finacc<raw::kMul>(ld(a), raw::bcr(b), raw::none(), m); }
    [[gnu::always_inline]] void maddabc(int a, float b, int m) { finacc<raw::kMadd>(ld(a), raw::bcr(b), ldacc(), m); }
    [[gnu::always_inline]] void addav(int a, int b, int m) { finacc<raw::kAdd>(ld(a), ld(b), raw::none(), m); }
    // a row-vector transform as the routines write it: ACC = r0 p.x, + r1 p.y, + r2 p.z, then d = ACC + r3 w (all four lanes)
    [[gnu::always_inline]] void transform(int d, int r0, int r1, int r2, int r3, int p, float w) {
        const Vf s = vf[p];
        mulabc(r0, s.v[0], XYZW);
        maddabc(r1, s.v[1], XYZW);
        maddabc(r2, s.v[2], XYZW);
        maddbc(d, r3, w, XYZW);
    }
    // the same on the xyz lanes (VMULAx.xyz / VMADDAy.xyz / VMADDAz.xyz / VMADDw.xyz)
    [[gnu::always_inline]] void transform3(int d, int r0, int r1, int r2, int r3, int p, float w) {
        const Vf s = vf[p];
        mulabc(r0, s.v[0], XYZ);
        maddabc(r1, s.v[1], XYZ);
        maddabc(r2, s.v[2], XYZ);
        maddbc(d, r3, w, XYZ);
    }
    // the three-row form (VMULAx / VMADDAy / VMADDz): d = r0 p.x + r1 p.y + r2 p.z, the lanes `m`
    [[gnu::always_inline]] void rotate(int d, int r0, int r1, int r2, int p, int m) {
        const Vf s = vf[p];
        mulabc(r0, s.v[0], m);
        maddabc(r1, s.v[1], m);
        maddbc(d, r2, s.v[2], m);
    }
    // VSUBbc into vf0: only its MAC and status flags (a compare); vf0 keeps its constant
    [[gnu::always_inline]] void subbc_flags(int a, float b, int m) { fin<raw::kSub>(0, ld(a), raw::bcr(b), raw::none(), m); }
    // ACC = a * b (VMULA), d = ACC + a * b (VMADD, vector)
    [[gnu::always_inline]] void mulav(int a, int b, int m) { finacc<raw::kMul>(ld(a), ld(b), raw::none(), m); }
    [[gnu::always_inline]] void maddv(int d, int a, int b, int m) { fin<raw::kMadd>(d, ld(a), ld(b), ldacc(), m); }
    // VOPMULA (ACC.xyz = a.yzx * b.zxy, lane by lane with the VU rules, w's flags kept) and VOPMSUB (d.xyz = ACC - a.yzx * b.zxy)
    void opmula(int a, int b) {
        sync();
        const Vf s = vf[a], t = vf[b];
        for (int i = 0; i < 3; ++i) acc.v[i] = vu0m::mac_lane_in(mac, i, vuc(s.v[(i + 1) % 3]) * vuc(t.v[(i + 2) % 3]));
        status = vu0m::status_of(mac, status);
    }
    [[gnu::always_inline]] void opmsub(int d, int a, int b) {
        const __m128i a_yzx = _mm_shuffle_epi32(ld(a), _MM_SHUFFLE(3, 0, 2, 1));
        const __m128i b_zxy = _mm_shuffle_epi32(ld(b), _MM_SHUFFLE(3, 1, 0, 2));
        fin<raw::kMsub>(d, a_yzx, b_zxy, ldacc(), XYZ);
    }
    // a lane-wise copy (VMOVE) on the masked lanes
    [[gnu::always_inline]] void move(int d, int a, int m) {
        if (d == 0) return;
        const Vf s = vf[a];
        for (int i = 0; i < 4; ++i)
            if ((m >> i & 1) != 0) vf[d].v[i] = s.v[i];
    }

    // --- the other ops (no clamp: they work on the bits)
    // VCLIPw.xyz: p against |w| into the clip flags
    [[gnu::always_inline]] void clipw(int p) { vu0m::clipw(clip, vf[p], vf[p].v[3]); }
    // VDIV Q = s / t, VSQRT Q = sqrt(|t|), VRSQRT Q = s / sqrt(|t|)
    [[gnu::always_inline]] void div(float s, float t) { q = vu0m::div(s, t); }
    [[gnu::always_inline]] void sqrt(float t) { q = vu0m::vsqrt(t); }
    [[gnu::always_inline]] void rsqrt(float s, float t) { q = vu0m::vrsqrt(s, t); }
    // VMAXbc / VMINIbc of one lane: d.lane = max / mini(a.lane, b)
    [[gnu::always_inline]] void max_lane(int d, int lane, float a, float b) { vf[d].v[lane] = vu0m::vmax(a, b); }
    [[gnu::always_inline]] void mini_lane(int d, int lane, float a, float b) { vf[d].v[lane] = vu0m::vmini(a, b); }
    // VABS on the masked lanes
    [[gnu::always_inline]] void abs(int d, int a, int m) {
        const Vf s = vf[a];
        for (int i = 0; i < 4; ++i)
            if ((m >> i & 1) != 0) vf[d].v[i] = u2f(f2u(s.v[i]) & 0x7fffffffu);
    }
    // VMR32 on the masked lanes: d.lane = a.(lane + 1)
    [[gnu::always_inline]] void mr32(int d, int a, int m) {
        const Vf s = vf[a];
        for (int i = 0; i < 4; ++i)
            if ((m >> i & 1) != 0) vf[d].v[i] = s.v[(i + 1) & 3];
    }
    // VITOFn / VFTOIn on the masked lanes
    [[gnu::always_inline]] void itof(int d, int a, int m, float scale) {
        const Vf s = vf[a];
        for (int i = 0; i < 4; ++i)
            if ((m >> i & 1) != 0) vf[d].v[i] = vu0m::itof(s.v[i], scale);
    }
    [[gnu::always_inline]] void ftoi(int d, int a, int m, float scale) {
        const Vf s = vf[a];
        for (int i = 0; i < 4; ++i)
            if ((m >> i & 1) != 0) vf[d].v[i] = vu0m::bits(vu0m::ftoi(s.v[i], scale));
    }

    // --- the lanes as words (QMTC2 / QMFC2 / LQC2 / SQC2)
    [[gnu::always_inline]] uint32_t word(int r, int lane) const { return f2u(vf[r].v[lane]); }
    [[gnu::always_inline]] void set_words(int r, uint32_t x, uint32_t y, uint32_t z, uint32_t w) {
        if (r == 0) return;  // (vf0 is constant)
        vf[r].v[0] = u2f(x), vf[r].v[1] = u2f(y), vf[r].v[2] = u2f(z), vf[r].v[3] = u2f(w);
    }
    // a word's sign bit (BLTZ on a QMFC2'd lane, the word moved to the register's top)
    [[gnu::always_inline]] bool negative(int r, int lane) const { return (word(r, lane) & 0x80000000u) != 0; }
    [[gnu::always_inline]] float lane(int r, int i) const { return vf[r].v[i]; }

private:
    [[gnu::always_inline]] __m128i ld(int a) const { return vu0m::ld4(vf[a]); }
    [[gnu::always_inline]] __m128i ldacc() const { return vu0m::ld4(acc); }
    // one FMAC op on the lanes x, y (c: ACC's) as the copy holds them, into the lanes `m` of vf[d] (finacc: of ACC). The raw
    // form (vu0_regs.h's raw: the lanes as they are, by the rules only where a written lane has exponent 255; the result needs
    // no clamp and makes no underflow or overflow flag), or as before: the operands clamped, the op, put
    template <int K>
    [[gnu::always_inline]] void fin(int d, __m128i x, __m128i y, __m128i c, int m) {
        if (raw::on()) {
            __m128i r = raw::calc<K>(x, y, c);
            if (__builtin_expect(raw::bad(r, m), 0)) r = raw::rules(K, x, y, c);
            if (d == 0) return flags<true>(r, m);
            if constexpr (Flags) keep_flags<true>(r, m);
            vu0m::st4_m(vf[d], r, m);
            return;
        }
        put(d, raw::calc<K>(vu0m::cop4(x), vu0m::cop4(y), vu0m::cop4(c)), m);
        if (__builtin_expect(ps2fpu::g_vu0_raw == 2, 0)) raw::check(K, x, y, c, m);
    }
    template <int K>
    [[gnu::always_inline]] void finacc(__m128i x, __m128i y, __m128i c, int m) {
        if (raw::on()) {
            __m128i r = raw::calc<K>(x, y, c);
            if (__builtin_expect(raw::bad(r, m), 0)) r = raw::rules(K, x, y, c);
            if constexpr (Flags) keep_flags<true>(r, m);
            vu0m::st4_m(acc, r, m);
            return;
        }
        putacc(raw::calc<K>(vu0m::cop4(x), vu0m::cop4(y), vu0m::cop4(c)), m);
        if (__builtin_expect(ps2fpu::g_vu0_raw == 2, 0)) raw::check(K, x, y, c, m);
    }
    [[gnu::always_inline]] void put(int d, __m128i raw, int m) {
        if (d == 0) return flags<false>(raw, m);  // (an op into vf0: its flags; never in the routines rewritten so far but kept exact)
        if constexpr (Flags) keep_flags<false>(raw, m);
        vu0m::st4_m(vf[d], vu0m::cop4(raw), m);
    }
    [[gnu::always_inline]] void putacc(__m128i raw, int m) {
        if constexpr (Flags) keep_flags<false>(raw, m);
        vu0m::st4_m(acc, vu0m::cop4(raw), m);
    }
    // an op's flag bytes (vu0_regs.h's fin_m: zero, sign, underflow, overflow per lane, w first). Clean: of a result the rules
    // leave as it is (the raw form's: no denormal, no exponent 255), the zero and the sign alone
    template <bool Clean>
    [[gnu::always_inline]] static __m128i flag_bytes(__m128i v) {
        const __m128i vr = _mm_shuffle_epi32(v, _MM_SHUFFLE(0, 1, 2, 3));
        const __m128i expm = _mm_set1_epi32(0x7f800000);
        const __m128i e = _mm_and_si128(vr, expm);
        const __m128i isz = _mm_cmpeq_epi32(e, _mm_setzero_si128());
        const __m128i sgn = _mm_srai_epi32(vr, 31);
        if constexpr (Clean) {
            return _mm_packs_epi16(_mm_packs_epi32(isz, sgn), _mm_setzero_si128());
        } else {
            const __m128i isi = _mm_cmpeq_epi32(e, expm);
            const __m128i den = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(vr, _mm_set1_epi32(0x007fffff)), _mm_setzero_si128()), isz);
            return _mm_packs_epi16(_mm_packs_epi32(isz, sgn), _mm_packs_epi32(den, isi));
        }
    }
    // (Flags) an op's flag bytes kept (vu0_regs.h's fin_m for a LocalFlags: those of the written lanes)
    template <bool Clean>
    [[gnu::always_inline]] void keep_flags(__m128i v, int m) {
        const __m128i k = _mm_set1_epi32(static_cast<int>(((m & 8) != 0 ? 0xffu : 0u) | ((m & 4) != 0 ? 0xff00u : 0u) |
                                                          ((m & 2) != 0 ? 0xff0000u : 0u) | ((m & 1) != 0 ? 0xff000000u : 0u)));
        const __m128i x = _mm_and_si128(flag_bytes<Clean>(v), k);
        last = x;
        sticky = _mm_or_si128(sticky, x);
        pending = true;
    }
    // the MAC and status flags of an op whose result goes nowhere (vu0_regs.h's fin_m: zero, sign, underflow, overflow of the
    // written lanes; the status flag's current bits and its sticky ones)
    template <bool Clean>
    void flags(__m128i v, int m) {
        sync();
        const uint32_t x = static_cast<uint32_t>(_mm_movemask_epi8(flag_bytes<Clean>(v))) & (vu0m::mask_rev(m) * 0x1111u);
        mac = x;
        const uint32_t st = vu0m::status_bits(x);
        status = (status & 0xfc0) | st | (st << 6);
        vf[0].v[0] = 0.0f, vf[0].v[1] = 0.0f, vf[0].v[2] = 0.0f, vf[0].v[3] = 1.0f;  // (vu0_regs.h's fmac4 writes vf0's constant)
    }
};

using Vu = VuT<false>;
using VuF = VuT<true>;

}  // namespace rac1::vu0l

#endif
