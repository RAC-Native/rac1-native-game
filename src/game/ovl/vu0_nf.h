// The VU0 macro ops of vu0_regs.h without their flags, for the transliterated routines that read none of them (2026-11-01, the
// Wii U's CPU): the same lanes - the operands clamped by the VU rules (vuc: a zero exponent the signed zero, the largest the signed
// largest float), the op (a VMADD's product not clamped before the sum, as vu0_regs.h's), the result clamped the same way, the
// masked lanes only, ACC written by the ACC forms - but the MAC and status flags not set. An op whose destination is vf0 is kept for
// its flags alone (vf0 does not change): it runs as vu0_regs.h's, flags and all, so a CFC2 after it reads what it would.
// Why: on the Wii U (no SSE) vu0_regs.h's ops run on the SSE2 shim - every operand and result clamped with integer masks through
// memory, the flag words built lane by lane, ~150 instructions an op; moby_draw_fast.inc measured the draw loop so. Here a lane is
// clamped by vuc, an op a few instructions a lane.
// A file uses them with `using namespace vu0nf;` in place of `using namespace vu0m;` (tools/codegen/vu0_nf_pass.py puts it in the
// generated files that may: their CFC2 of the status / MAC flags only after an op into vf0).
// orig: (host)
#ifndef RAC1_GAME_OVL_VU0_NF_H
#define RAC1_GAME_OVL_VU0_NF_H

#include <game/ovl/vu0_regs.h>

namespace rac1::vu0nf {

// (every other name is vu0m's; the ops below take their vector operands by non-const reference, so for the generated code's
// operands - vf()[n], acc(), all non-const - they are chosen over vu0m's const-reference ones; a const operand falls to vu0m's)
using namespace vu0m;

namespace detail {
// orig: (helper) vuc (ovl_common.h's: its bits, the console's fastest - RAC1_VUC_FPU its FPU-decided form)
[[gnu::always_inline]] inline float fvuc(float x) { return vuc(x); }
enum Op { kAdd, kSub, kMul, kMadd };
// orig: (helper) the op on the masked lanes of `d`: `bv` the second operand's lanes or `bs` a broadcast (not yet clamped), `c` ACC
// (kMadd); the lanes computed from the operands first, then written
template <int O, bool Bc>
[[gnu::noinline]] inline void op(Vf& d, const Vf& a, const Vf* bv, float bs, int m) {
    const Vf& c = vu0m::acc();
    const float b0 = Bc ? fvuc(bs) : 0.0f;
    float r[4];
    for (int i = 0; i < 4; ++i) {
        if ((m >> i & 1) == 0) continue;
        const float x = fvuc(a.v[i]), y = Bc ? b0 : fvuc(bv->v[i]);
        if constexpr (O == kAdd) r[i] = fvuc(x + y);
        else if constexpr (O == kSub) r[i] = fvuc(x - y);
        else if constexpr (O == kMul) r[i] = fvuc(x * y);
        else {
            const float p = x * y;
            r[i] = fvuc(fvuc(c.v[i]) + p);
        }
    }
    for (int i = 0; i < 4; ++i)
        if ((m >> i & 1) != 0) d.v[i] = r[i];
}
inline bool is_vf0(const Vf& d) { return &d == vu0m::vf(); }
}  // namespace detail

// orig: (helper) VADD / VSUB / VMUL and their broadcast forms, VMADD (broadcast), VADDA / VMULA / VMADDA (broadcast): vu0_regs.h's,
// no flags (to vf0: vu0_regs.h's own, for its flags)
inline void add(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::is_vf0(d)) return vu0m::add(d, a, b, m);
    detail::op<detail::kAdd, false>(d, a, &b, 0.0f, m);
}
inline void sub(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::is_vf0(d)) return vu0m::sub(d, a, b, m);
    detail::op<detail::kSub, false>(d, a, &b, 0.0f, m);
}
inline void mul(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::is_vf0(d)) return vu0m::mul(d, a, b, m);
    detail::op<detail::kMul, false>(d, a, &b, 0.0f, m);
}
inline void addbc(Vf& d, Vf& a, float b, int m) {
    if (detail::is_vf0(d)) return vu0m::addbc(d, a, b, m);
    detail::op<detail::kAdd, true>(d, a, nullptr, b, m);
}
inline void subbc(Vf& d, Vf& a, float b, int m) {
    if (detail::is_vf0(d)) return vu0m::subbc(d, a, b, m);
    detail::op<detail::kSub, true>(d, a, nullptr, b, m);
}
inline void mulbc(Vf& d, Vf& a, float b, int m) {
    if (detail::is_vf0(d)) return vu0m::mulbc(d, a, b, m);
    detail::op<detail::kMul, true>(d, a, nullptr, b, m);
}
inline void maddbc(Vf& d, Vf& a, float b, int m) {
    if (detail::is_vf0(d)) return vu0m::maddbc(d, a, b, m);
    detail::op<detail::kMadd, true>(d, a, nullptr, b, m);
}
inline void addabc(Vf& a, float b, int m) { detail::op<detail::kAdd, true>(vu0m::acc(), a, nullptr, b, m); }
inline void mulabc(Vf& a, float b, int m) { detail::op<detail::kMul, true>(vu0m::acc(), a, nullptr, b, m); }
inline void maddabc(Vf& a, float b, int m) { detail::op<detail::kMadd, true>(vu0m::acc(), a, nullptr, b, m); }

}  // namespace rac1::vu0nf

#endif
