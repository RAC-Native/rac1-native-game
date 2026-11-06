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
}  // namespace detail

#if defined(__WIIU__) || defined(RAC1_VU0_SCALAR)  // (RAC1_VU0_SCALAR: the PC with the Wii U's scalar lanes, to check them)
// (the Wii U, 2026-11-01: no SSE - vu0_regs.h's ops run on the SSE2 shim, every operand and result clamped with integer masks
// through memory, ~150 instructions an op; here a lane is clamped by vuc, an op a few instructions a lane: the same values)
namespace detail {
// orig: (helper) vuc (ovl_common.h's: its bits, the console's fastest - RAC1_VUC_FPU its FPU-decided form)
[[gnu::always_inline]] inline float fvuc(float x) { return vuc(x); }
enum Op { kAdd, kSub, kMul, kMadd, kMsub };
// orig: (helper) the op on the masked lanes of `d`: `bv` the second operand's lanes or `bs` a broadcast (not yet clamped), `c` ACC
// (kMadd / kMsub: the product not clamped before the sum); the lanes computed from the operands first, then written
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
        else if constexpr (O == kMadd) r[i] = fvuc(fvuc(c.v[i]) + x * y);
        else r[i] = fvuc(fvuc(c.v[i]) - x * y);
    }
    for (int i = 0; i < 4; ++i)
        if ((m >> i & 1) != 0) d.v[i] = r[i];
}
}  // namespace detail

// orig: (helper) VADD / VSUB / VMUL and their broadcast forms, VMADD / VMSUB (broadcast, vector), the ACC forms: vu0_regs.h's
// without the flags (to vf0, or switched off: vu0_regs.h's own)
#define RAC1_NF_V(fn, O)                                                                     \
    inline void fn(Vf& d, Vf& a, Vf& b, int m) {                                             \
        if (detail::flagged(d)) return vu0m::fn(d, a, b, m);                                 \
        detail::op<detail::O, false>(d, a, &b, 0.0f, m);                                     \
    }
#define RAC1_NF_BC(fn, O)                                                                    \
    inline void fn(Vf& d, Vf& a, float b, int m) {                                           \
        if (detail::flagged(d)) return vu0m::fn(d, a, b, m);                                 \
        detail::op<detail::O, true>(d, a, nullptr, b, m);                                    \
    }
#define RAC1_NF_ABC(fn, O)                                                                   \
    inline void fn(Vf& a, float b, int m) {                                                  \
        if (!detail::g_on) return vu0m::fn(a, b, m);                                         \
        detail::op<detail::O, true>(vu0m::acc(), a, nullptr, b, m);                          \
    }
#define RAC1_NF_AV(fn, O)                                                                    \
    inline void fn(Vf& a, Vf& b, int m) {                                                    \
        if (!detail::g_on) return vu0m::fn(a, b, m);                                         \
        detail::op<detail::O, false>(vu0m::acc(), a, &b, 0.0f, m);                           \
    }
RAC1_NF_V(add, kAdd)
RAC1_NF_V(sub, kSub)
RAC1_NF_V(mul, kMul)
RAC1_NF_BC(addbc, kAdd)
RAC1_NF_BC(subbc, kSub)
RAC1_NF_BC(mulbc, kMul)
RAC1_NF_BC(maddbc, kMadd)
RAC1_NF_BC(msubbc, kMsub)
RAC1_NF_V(maddv, kMadd)
RAC1_NF_V(msubv, kMsub)
RAC1_NF_ABC(addabc, kAdd)
RAC1_NF_ABC(subabc, kSub)
RAC1_NF_ABC(mulabc, kMul)
RAC1_NF_ABC(maddabc, kMadd)
RAC1_NF_ABC(msubabc, kMsub)
RAC1_NF_AV(addav, kAdd)
RAC1_NF_AV(subav, kSub)
RAC1_NF_AV(mulav, kMul)
RAC1_NF_AV(maddav, kMadd)
RAC1_NF_AV(msubav, kMsub)
#undef RAC1_NF_V
#undef RAC1_NF_BC
#undef RAC1_NF_ABC
#undef RAC1_NF_AV

#else
namespace detail {
// orig: (helper) a raw result clamped (cop4: vu0_regs.h's operand clamp, the same rule as the FMAC's result clamp) into the
// masked lanes of `d`
[[gnu::always_inline]] inline void put(Vf& d, __m128i raw, int m) { vu0m::st4_m(d, vu0m::cop4(raw), m); }
[[gnu::always_inline]] inline __m128i op(const Vf& a) { return vu0m::cop4(vu0m::ld4(a)); }
}  // namespace detail

// (with RAC1_INLINE_HELPERS: the ops inline, their flagged form - the switch off, or an op into vf0 - a call out of line)
#if defined(RAC1_INLINE_HELPERS) && RAC1_INLINE_HELPERS
namespace detail {
RAC1_COLD_PATH inline void flagged_add(Vf& d, Vf& a, Vf& b, int m) { vu0m::add(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_sub(Vf& d, Vf& a, Vf& b, int m) { vu0m::sub(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_mul(Vf& d, Vf& a, Vf& b, int m) { vu0m::mul(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_addbc(Vf& d, Vf& a, float b, int m) { vu0m::addbc(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_subbc(Vf& d, Vf& a, float b, int m) { vu0m::subbc(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_mulbc(Vf& d, Vf& a, float b, int m) { vu0m::mulbc(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_maddbc(Vf& d, Vf& a, float b, int m) { vu0m::maddbc(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_msubbc(Vf& d, Vf& a, float b, int m) { vu0m::msubbc(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_maddv(Vf& d, Vf& a, Vf& b, int m) { vu0m::maddv(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_msubv(Vf& d, Vf& a, Vf& b, int m) { vu0m::msubv(d, a, b, m); }
RAC1_COLD_PATH inline void flagged_addabc(Vf& a, float b, int m) { vu0m::addabc(a, b, m); }
RAC1_COLD_PATH inline void flagged_subabc(Vf& a, float b, int m) { vu0m::subabc(a, b, m); }
RAC1_COLD_PATH inline void flagged_mulabc(Vf& a, float b, int m) { vu0m::mulabc(a, b, m); }
RAC1_COLD_PATH inline void flagged_maddabc(Vf& a, float b, int m) { vu0m::maddabc(a, b, m); }
RAC1_COLD_PATH inline void flagged_msubabc(Vf& a, float b, int m) { vu0m::msubabc(a, b, m); }
RAC1_COLD_PATH inline void flagged_addav(Vf& a, Vf& b, int m) { vu0m::addav(a, b, m); }
RAC1_COLD_PATH inline void flagged_subav(Vf& a, Vf& b, int m) { vu0m::subav(a, b, m); }
RAC1_COLD_PATH inline void flagged_mulav(Vf& a, Vf& b, int m) { vu0m::mulav(a, b, m); }
RAC1_COLD_PATH inline void flagged_maddav(Vf& a, Vf& b, int m) { vu0m::maddav(a, b, m); }
RAC1_COLD_PATH inline void flagged_msubav(Vf& a, Vf& b, int m) { vu0m::msubav(a, b, m); }
}  // namespace detail
#define RAC1_NF_FLAGGED(fn) detail::flagged_##fn
#else
#define RAC1_NF_FLAGGED(fn) vu0m::fn
#endif

// orig: (helper) VADD / VSUB / VMUL and their broadcast forms, VMADD / VMSUB (broadcast, vector), the ACC forms: vu0_regs.h's
// without the flags (to vf0, or switched off: vu0_regs.h's own)
RAC1_HOT_INLINE void add(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(add)(d, a, b, m);
    detail::put(d, vadd4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void sub(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(sub)(d, a, b, m);
    detail::put(d, vsub4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void mul(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(mul)(d, a, b, m);
    detail::put(d, vmul4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void addbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(addbc)(d, a, b, m);
    detail::put(d, vadd4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void subbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(subbc)(d, a, b, m);
    detail::put(d, vsub4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void mulbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(mulbc)(d, a, b, m);
    detail::put(d, vmul4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void maddbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(maddbc)(d, a, b, m);
    detail::put(d, vadd4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
RAC1_HOT_INLINE void msubbc(Vf& d, Vf& a, float b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(msubbc)(d, a, b, m);
    detail::put(d, vsub4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
RAC1_HOT_INLINE void maddv(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(maddv)(d, a, b, m);
    detail::put(d, vadd4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
RAC1_HOT_INLINE void msubv(Vf& d, Vf& a, Vf& b, int m) {
    if (detail::flagged(d)) return RAC1_NF_FLAGGED(msubv)(d, a, b, m);
    detail::put(d, vsub4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
RAC1_HOT_INLINE void addabc(Vf& a, float b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(addabc)(a, b, m);
    detail::put(acc(), vadd4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void subabc(Vf& a, float b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(subabc)(a, b, m);
    detail::put(acc(), vsub4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void mulabc(Vf& a, float b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(mulabc)(a, b, m);
    detail::put(acc(), vmul4(detail::op(a), bc4(b)), m);
}
RAC1_HOT_INLINE void maddabc(Vf& a, float b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(maddabc)(a, b, m);
    detail::put(acc(), vadd4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
RAC1_HOT_INLINE void msubabc(Vf& a, float b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(msubabc)(a, b, m);
    detail::put(acc(), vsub4(detail::op(acc()), vmul4(detail::op(a), bc4(b))), m);
}
RAC1_HOT_INLINE void addav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(addav)(a, b, m);
    detail::put(acc(), vadd4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void subav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(subav)(a, b, m);
    detail::put(acc(), vsub4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void mulav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(mulav)(a, b, m);
    detail::put(acc(), vmul4(detail::op(a), detail::op(b)), m);
}
RAC1_HOT_INLINE void maddav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(maddav)(a, b, m);
    detail::put(acc(), vadd4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}
RAC1_HOT_INLINE void msubav(Vf& a, Vf& b, int m) {
    if (!detail::g_on) return RAC1_NF_FLAGGED(msubav)(a, b, m);
    detail::put(acc(), vsub4(detail::op(acc()), vmul4(detail::op(a), detail::op(b))), m);
}

#endif  // __WIIU__

}  // namespace rac1::vu0nf

#endif
