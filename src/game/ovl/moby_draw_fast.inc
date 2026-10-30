// The draw loop's VU0 macro ops without their flags (RAC1_WIIU_FAST_MOBY_LOOP: the Wii U's default, 2026-10-30), included by
// moby_draw.cpp inside its anonymous namespace. The same lanes as vu0_regs.h's ops - the operands clamped by the VU rules
// (vuc: a zero exponent the signed zero, the largest the signed largest float), the op, the result clamped the same way, the
// masked lanes only, vf0 kept constant, ACC written by the ACC forms - but no MAC / status flags. Nothing reads the flags the
// loop leaves: its own code never does (no CFC2, no branch on them), and after it the game never reads the status flag (no
// CFC2 of it; the VU0 programs test none) and each MAC read (CFC2 17: the drop shadows') follows an op of its own (see
// RAC1_WIIU_FAST_POSE, moby_joints.cpp). The VU0's flag registers keep the values they had before the loop.
// Why: on the Wii U there is no SSE; vu0_regs.h's ops run on the SSE2 shim (four scalar lanes, every operand and result
// clamped with integer masks through memory - the 750 has no move between its float and integer registers - and the flag
// words built lane by lane by fin_lanes): the loop was 142 KB of code (4.4 times the 750's instruction cache), ~150
// instructions an op. Here a lane's clamp is decided on the FPU (|x| a normal number: kept as it is; zeros, denormals,
// infinities and NaNs take vuc's integer path), an op is a few instructions a lane.
// orig: (host)

// orig: (host) the flag sink of the fast ops (no state: the flags are not kept)
struct NoFlags {};

// orig: (helper) vuc's rare cases (zeros, denormals, infinities, NaNs), out of the loop's code
[[gnu::noinline, gnu::cold]] float fvuc_rare(float x) { return vuc(x); }
// orig: (helper) vuc, its common case decided on the FPU: a normal finite number is returned as it is (vuc does too); the rest
// (any comparison of a NaN is false) by vuc itself - so always vuc's value
[[gnu::always_inline]] inline float fvuc(float x) {
    const float a = __builtin_fabsf(x);
    if (__builtin_expect(a >= 1.17549435e-38f && a <= 3.40282347e+38f, 1)) return x;
    return fvuc_rare(x);
}
// orig: (helper) one lane of an op: kAdd / kSub / kMul a op b, kMadd c + a b (the product rounded on its own: -ffp-contract=off,
// as the ops' vectors), the operands and the result clamped
enum NfOp { kAdd, kSub, kMul, kMadd };
template <int O>
[[gnu::always_inline]] inline float nf_lane(float a, float b, float c) {
    const float x = fvuc(a);
    if constexpr (O == kAdd) return fvuc(x + b);
    else if constexpr (O == kSub) return fvuc(x - b);
    else if constexpr (O == kMul) return fvuc(x * b);
    else {
        const float p = x * b;
        return fvuc(fvuc(c) + p);
    }
}
// orig: (helper) the op on the lanes M of `d` (ACC: d is acc()): `b` the second operand's lanes (`bv`) or a broadcast (`bs`, already
// clamped), `c` ACC (kMadd); vf0 left as the hardware keeps it - (0, 0, 0, 1). One small function per (op, form, mask), called: the
// loop's code stays small (the 750's 32 KB instruction cache; inlined, the ops made it ~130 KB, read again for every moby drawn)
template <int O, bool Bc, int M>
[[gnu::noinline]] void nf_op(Vf& d, const Vf& a, const Vf* bv, float bs) {
    if (__builtin_expect(&d == vf(), 0)) {
        d.v[0] = 0.0f, d.v[1] = 0.0f, d.v[2] = 0.0f, d.v[3] = 1.0f;
        return;
    }
    const Vf& c = acc();
    float r[4];
#pragma GCC unroll 4
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) r[i] = nf_lane<O>(a.v[i], Bc ? bs : fvuc(bv->v[i]), c.v[i]);
#pragma GCC unroll 4
    for (int i = 0; i < 4; ++i)
        if ((M >> i & 1) != 0) d.v[i] = r[i];
}
// orig: (helper) nf_op for the call's mask (a constant where the ops are used: the switch folds to one call)
template <int O, bool Bc>
[[gnu::always_inline]] inline void nf_call(Vf& d, int m, const Vf& a, const Vf* bv, float bs) {
    switch (m) {
#define RAC1_NF_CASE(k) \
    case k: nf_op<O, Bc, k>(d, a, bv, bs); break;
        RAC1_NF_CASE(1) RAC1_NF_CASE(2) RAC1_NF_CASE(3) RAC1_NF_CASE(4) RAC1_NF_CASE(5) RAC1_NF_CASE(6) RAC1_NF_CASE(7)
        RAC1_NF_CASE(8) RAC1_NF_CASE(9) RAC1_NF_CASE(10) RAC1_NF_CASE(11) RAC1_NF_CASE(12) RAC1_NF_CASE(13) RAC1_NF_CASE(14)
        RAC1_NF_CASE(15)
#undef RAC1_NF_CASE
        default: break;
    }
}
// orig: (helper) VADD / VSUB / VMUL (lane by lane) and their broadcast forms, VADDA / VMULA / VMADDA (broadcast), VMADD
// (broadcast): vu0_regs.h's, no flags
[[gnu::always_inline]] inline void add(NoFlags&, Vf& d, const Vf& a, const Vf& b, int m) { nf_call<kAdd, false>(d, m, a, &b, 0.0f); }
[[gnu::always_inline]] inline void sub(NoFlags&, Vf& d, const Vf& a, const Vf& b, int m) { nf_call<kSub, false>(d, m, a, &b, 0.0f); }
[[gnu::always_inline]] inline void mul(NoFlags&, Vf& d, const Vf& a, const Vf& b, int m) { nf_call<kMul, false>(d, m, a, &b, 0.0f); }
[[gnu::always_inline]] inline void addbc(NoFlags&, Vf& d, const Vf& a, float b, int m) { nf_call<kAdd, true>(d, m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void subbc(NoFlags&, Vf& d, const Vf& a, float b, int m) { nf_call<kSub, true>(d, m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void mulbc(NoFlags&, Vf& d, const Vf& a, float b, int m) { nf_call<kMul, true>(d, m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void maddbc(NoFlags&, Vf& d, const Vf& a, float b, int m) { nf_call<kMadd, true>(d, m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void addabc(NoFlags&, const Vf& a, float b, int m) { nf_call<kAdd, true>(acc(), m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void mulabc(NoFlags&, const Vf& a, float b, int m) { nf_call<kMul, true>(acc(), m, a, nullptr, fvuc(b)); }
[[gnu::always_inline]] inline void maddabc(NoFlags&, const Vf& a, float b, int m) { nf_call<kMadd, true>(acc(), m, a, nullptr, fvuc(b)); }
// orig: (helper) VMULAx / VMADDAy / VMADDz: d = r0 p.x + r1 p.y + r2 p.z on the masked lanes (ACC left as the ops leave it)
template <int M>
[[gnu::noinline]] void nf_rotate(Vf& d, int r0, const Vf& p) {
    NoFlags fl;
    const Vf s = p;
    mulabc(fl, vf()[r0], s.v[0], M);
    maddabc(fl, vf()[r0 + 1], s.v[1], M);
    maddbc(fl, d, vf()[r0 + 2], s.v[2], M);
}
[[gnu::always_inline]] inline void rotate(NoFlags&, Vf& d, int r0, const Vf& p, int m) {
    if (m == XYZ) nf_rotate<XYZ>(d, r0, p);
    else if (m == XYZW) nf_rotate<XYZW>(d, r0, p);
    else if (m == XY) nf_rotate<XY>(d, r0, p);
    else {
        NoFlags fl;
        const Vf s = p;
        mulabc(fl, vf()[r0], s.v[0], m);
        maddabc(fl, vf()[r0 + 1], s.v[1], m);
        maddbc(fl, d, vf()[r0 + 2], s.v[2], m);
    }
}
// orig: (helper) the same with VMADDAz / VMADDw: d = r0 p.x + r1 p.y + r2 p.z + r3 w
[[gnu::noinline]] void nf_transform(Vf& d, int r0, const Vf& p, float w) {
    NoFlags fl;
    const Vf s = p;
    mulabc(fl, vf()[r0], s.v[0], XYZW);
    maddabc(fl, vf()[r0 + 1], s.v[1], XYZW);
    maddabc(fl, vf()[r0 + 2], s.v[2], XYZW);
    maddbc(fl, d, vf()[r0 + 3], w, XYZW);
}
[[gnu::always_inline]] inline void transform(NoFlags&, Vf& d, int r0, const Vf& p, float w) { nf_transform(d, r0, p, w); }
