// The VU0 macro ops of vu0_regs.h without their flags, for the generated routines that read none of them: the same lanes - the
// operands clamped by the VU rules (a zero exponent the signed zero, the largest the signed largest float), the op (a VMADD's
// product not clamped before the sum, as vu0_regs.h's), the result clamped the same way, the masked lanes only, ACC written by
// the ACC forms - but the MAC and status flags not made. Building the flags is most of a vu0_regs.h op's work. An op whose
// destination is vf0 is kept for its flags alone (vf0 does not change): it runs as vu0_regs.h's, flags and all, so a CFC2 after
// it reads what it would.
// A file uses them with `using namespace vu0nf;` in place of `using namespace vu0m;` (tools/codegen/vu0_nf_pass.py puts it in the
// generated files that may: their CFC2 of the status / MAC flags only after an op into vf0). RAC1_PERF_VU0_NF=0: every op as
// vu0_regs.h's, flags and all (A/B).
// orig: (host)
#ifndef RAC1_GAME_OVL_VU0_NF_H
#define RAC1_GAME_OVL_VU0_NF_H

#include <game/ovl/vu0_regs.h>

#include <cstdlib>

namespace rac1::vu0nf {

// (every other name is vu0m's; the ops below take their vector operands by non-const reference, so for the generated code's
// operands - vf()[n], acc(), all non-const - they are chosen over vu0m's const-reference ones; a const operand falls to vu0m's)
using namespace vu0m;

namespace detail {
// orig: (host) RAC1_PERF_VU0_NF (default on)
inline const bool g_on = [] {
    const char* e = std::getenv("RAC1_PERF_VU0_NF");
    return e == nullptr || std::atoi(e) != 0;
}();
// orig: (helper) the flagged form wanted: the switch off, or the destination vf0 (an op kept for its flags)
[[gnu::always_inline]] inline bool flagged(const Vf& d) { return !g_on || &d == vu0m::vf(); }
// orig: (helper) a raw result clamped (cop4: vu0_regs.h's operand clamp, the same rule as the FMAC's result clamp) into the
// masked lanes of `d`
[[gnu::always_inline]] inline void put(Vf& d, __m128i raw, int m) { vu0m::st4_m(d, vu0m::cop4(raw), m); }
[[gnu::always_inline]] inline __m128i op(const Vf& a) { return vu0m::cop4(vu0m::ld4(a)); }
}  // namespace detail

// orig: (helper) VADD / VSUB / VMUL and their broadcast forms, VMADD / VMSUB (broadcast, vector), the ACC forms: vu0_regs.h's
// without the flags (to vf0, or switched off: vu0_regs.h's own)
inline void add(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return vu0m::add(d, a, b, m);
    detail::put(d, vadd4(detail::op(a), detail::op(b)), m);
}
inline void sub(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return vu0m::sub(d, a, b, m);
    detail::put(d, vsub4(detail::op(a), detail::op(b)), m);
}
inline void mul(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return vu0m::mul(d, a, b, m);
    detail::put(d, vmul4(detail::op(a), detail::op(b)), m);
}
inline void addbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return vu0m::addbc(d, a, b, m);
    detail::put(d, vadd4(detail::op(a), bc4(b)), m);
}
inline void subbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return vu0m::subbc(d, a, b, m);
    detail::put(d, vsub4(detail::op(a), bc4(b)), m);
}
inline void mulbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return vu0m::mulbc(d, a, b, m);
    detail::put(d, vmul4(detail::op(a), bc4(b)), m);
}
inline void maddbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return vu0m::maddbc(d, a, b, m);
    detail::put(d, vadd4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
inline void msubbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return vu0m::msubbc(d, a, b, m);
    detail::put(d, vsub4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
inline void maddv(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return vu0m::maddv(d, a, b, m);
    detail::put(d, vadd4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
inline void msubv(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return vu0m::msubv(d, a, b, m);
    detail::put(d, vsub4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
inline void addabc(Vf& a, float b, int m) {
    if (!detail::g_on) return vu0m::addabc(a, b, m);
    detail::put(acc(), vadd4(detail::op(a), bc4(b)), m);
}
inline void subabc(Vf& a, float b, int m) {
    if (!detail::g_on) return vu0m::subabc(a, b, m);
    detail::put(acc(), vsub4(detail::op(a), bc4(b)), m);
}
inline void mulabc(Vf& a, float b, int m) {
    if (!detail::g_on) return vu0m::mulabc(a, b, m);
    detail::put(acc(), vmul4(detail::op(a), bc4(b)), m);
}
inline void maddabc(Vf& a, float b, int m) {
    if (!detail::g_on) return vu0m::maddabc(a, b, m);
    detail::put(acc(), vadd4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
inline void msubabc(Vf& a, float b, int m) {
    if (!detail::g_on) return vu0m::msubabc(a, b, m);
    detail::put(acc(), vsub4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
inline void addav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return vu0m::addav(a, b, m);
    detail::put(acc(), vadd4(detail::op(a), detail::op(b)), m);
}
inline void subav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return vu0m::subav(a, b, m);
    detail::put(acc(), vsub4(detail::op(a), detail::op(b)), m);
}
inline void mulav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return vu0m::mulav(a, b, m);
    detail::put(acc(), vmul4(detail::op(a), detail::op(b)), m);
}
inline void maddav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return vu0m::maddav(a, b, m);
    detail::put(acc(), vadd4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
inline void msubav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return vu0m::msubav(a, b, m);
    detail::put(acc(), vsub4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}

}  // namespace rac1::vu0nf

#endif
