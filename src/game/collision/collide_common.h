#ifndef RAC1_GAME_COLLISION_COLLIDE_COMMON_H
#define RAC1_GAME_COLLISION_COLLIDE_COMMON_H
// The collision routines' shared helpers (src/game/collision/collide_*.cpp: sphere_mesh_collide, mobys_in_sphere,
// sphere_collide, capsule_collide, segment_collide), moved here unchanged from ovl_1e_1f.cpp's anonymous namespaces: the
// VU0 macro-mode lane arithmetic, the level collision tree's cell lookup, and the contact registers vf25..vf29.
// The functions stay `static`, not `inline`: each routine's file compiles its own copy with the same inlining as when they were
// in one file (`[[maybe_unused]]`: a file that does not use one is not warned about it). The contact registers are one
// object, defined in collide_mesh.cpp.
// The routines' declarations stay in game/ovl/ovl.h (the generator's glue and the levels' skeletons include it).
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <algorithm>
#include <cmath>
#include <cstdint>
#include <cstdlib>
#include <cstring>

namespace rac1 {
namespace collide_detail {

// VU0 macro-mode lane arithmetic as the interpreter models it (operands and results clamped)
[[maybe_unused]] static float qadd(float a, float b) { return vuc(vuc(a) + vuc(b)); }
[[maybe_unused]] static float qsub(float a, float b) { return vuc(vuc(a) - vuc(b)); }
[[maybe_unused]] static float qmul(float a, float b) { return vuc(vuc(a) * vuc(b)); }
[[maybe_unused]] static float qmadd(float acc, float a, float b) { return vuc(vuc(acc) + vuc(a) * vuc(b)); }
[[maybe_unused]] static float qmsub(float acc, float a, float b) { return vuc(vuc(acc) - vuc(a) * vuc(b)); }
[[maybe_unused]] static uint32_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }
[[maybe_unused]] static float qdiv(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    if (b == 0.0f) return u2f(((fbits(s) ^ fbits(t)) & 0x80000000u) != 0 ? 0xff7fffffu : 0x7f7fffffu);
    return vuc(a / b);
}
[[maybe_unused]] static float qrsqrt(float s, float t) {
    const float a = vuc(s), b = vuc(t);
    const uint32_t neg = (fbits(s) ^ fbits(t)) & 0x80000000u;
    if (b == 0.0f) return a != 0.0f ? u2f(neg != 0 ? 0xff7fffffu : 0x7f7fffffu) : u2f(neg);
    return vuc(a / std::sqrt(std::fabs(b)));
}
[[maybe_unused]] static int32_t qftoi0(float f) {
    const uint32_t u = fbits(f);
    if ((u & 0x7f800000u) >= 0x4f000000u) return (u & 0x80000000u) != 0 ? INT32_MIN : INT32_MAX;
    return static_cast<int32_t>(f);
}
struct L3 {
    float x, y, z;
};
[[maybe_unused]] static L3 u16x3(uint32_t a) { return L3{static_cast<float>(lhu(a)), static_cast<float>(lhu(a + 2)), static_cast<float>(lhu(a + 4))}; }
[[maybe_unused]] static L3 l3(uint32_t a) { return L3{lwc1(a), lwc1(a + 4), lwc1(a + 8)}; }
[[maybe_unused]] static L3 add3(const L3& a, const L3& b) { return L3{qadd(a.x, b.x), qadd(a.y, b.y), qadd(a.z, b.z)}; }
[[maybe_unused]] static L3 sub3(const L3& a, const L3& b) { return L3{qsub(a.x, b.x), qsub(a.y, b.y), qsub(a.z, b.z)}; }
[[maybe_unused]] static L3 mul3(const L3& a, const L3& b) { return L3{qmul(a.x, b.x), qmul(a.y, b.y), qmul(a.z, b.z)}; }
[[maybe_unused]] static L3 muls(const L3& a, float s) { return L3{qmul(a.x, s), qmul(a.y, s), qmul(a.z, s)}; }
[[maybe_unused]] static L3 madds(const L3& acc, const L3& a, float s) { return L3{qmadd(acc.x, a.x, s), qmadd(acc.y, a.y, s), qmadd(acc.z, a.z, s)}; }
// vopmula ACC, a, b ; vopmsub d, b, a
[[maybe_unused]] static L3 opm(const L3& a, const L3& b) {
    return L3{qmsub(qmul(a.y, b.z), b.y, a.z), qmsub(qmul(a.z, b.x), b.z, a.x), qmsub(qmul(a.x, b.y), b.x, a.y)};
}
[[maybe_unused]] static float hsum(const L3& v) { return qmadd(qadd(v.x, v.y), 1.0f, v.z); }      // vadday.x + vmaddz.x
[[maybe_unused]] static float hsum_y(const L3& v) { return qmadd(qadd(v.y, v.x), 1.0f, v.z); }    // vaddax.y + vmaddz.y
[[maybe_unused]] static bool neg(float f) { return (fbits(f) & 0x80000000u) != 0; }
[[maybe_unused]] static void sq3(uint32_t a, const L3& v, uint32_t w) { swc1(a, v.x); swc1(a + 4, v.y); swc1(a + 8, v.z); sw(a + 0xc, w); }
// (host) RAC1_PERF_VU_CLAMP (default on): the lane arithmetic below for operands known to be clamped already (a VU result, a small
// integer's float, a value clamped as it is read): vuc(x) is x for those, so only the result is clamped - the same values as the q*
// forms above, which clamp every operand again (RAC1_PERF_VU_CLAMP=0: those, to compare).
static const bool g_clamp_once = std::getenv("RAC1_PERF_VU_CLAMP") == nullptr || std::atoi(std::getenv("RAC1_PERF_VU_CLAMP")) != 0;
[[maybe_unused]] static float cadd(float a, float b) { return g_clamp_once ? vuc(a + b) : qadd(a, b); }
[[maybe_unused]] static float csub(float a, float b) { return g_clamp_once ? vuc(a - b) : qsub(a, b); }
[[maybe_unused]] static float cmul(float a, float b) { return g_clamp_once ? vuc(a * b) : qmul(a, b); }
[[maybe_unused]] static float cmadd(float acc, float a, float b) { return g_clamp_once ? vuc(acc + a * b) : qmadd(acc, a, b); }
[[maybe_unused]] static float cmsub(float acc, float a, float b) { return g_clamp_once ? vuc(acc - a * b) : qmsub(acc, a, b); }
[[maybe_unused]] static L3 cl3(uint32_t a) { return g_clamp_once ? L3{vuc(lwc1(a)), vuc(lwc1(a + 4)), vuc(lwc1(a + 8))} : l3(a); }
[[maybe_unused]] static L3 cadd3(const L3& a, const L3& b) { return L3{cadd(a.x, b.x), cadd(a.y, b.y), cadd(a.z, b.z)}; }
[[maybe_unused]] static L3 csub3(const L3& a, const L3& b) { return L3{csub(a.x, b.x), csub(a.y, b.y), csub(a.z, b.z)}; }
[[maybe_unused]] static L3 cmul3(const L3& a, const L3& b) { return L3{cmul(a.x, b.x), cmul(a.y, b.y), cmul(a.z, b.z)}; }
[[maybe_unused]] static L3 cmuls(const L3& a, float s) { return L3{cmul(a.x, s), cmul(a.y, s), cmul(a.z, s)}; }
[[maybe_unused]] static L3 cmadds(const L3& acc, const L3& a, float s) { return L3{cmadd(acc.x, a.x, s), cmadd(acc.y, a.y, s), cmadd(acc.z, a.z, s)}; }
[[maybe_unused]] static L3 copm(const L3& a, const L3& b) {
    return L3{cmsub(cmul(a.y, b.z), b.y, a.z), cmsub(cmul(a.z, b.x), b.z, a.x), cmsub(cmul(a.x, b.y), b.x, a.y)};
}
[[maybe_unused]] static float chsum(const L3& v) { return cmadd(cadd(v.x, v.y), 1.0f, v.z); }
// vf25..vf29 of VU0: the collision routines keep their nearest contact there and store it from
// there, so a contact that sets only some of them (a primitive in sphere_collide) stores the
// others as the previous contact left them. Kept here across calls, as the registers are.
struct VuContact {
    L3 v25{}, v26{}, v27{}, v28{}, v29{};
};
extern VuContact g_vu;  // (collide_mesh.cpp)

[[maybe_unused]] static uint32_t fp_maxb(uint32_t a, uint32_t b) {  // VMAX on raw lanes
    return (static_cast<int32_t>(a) < 0 && static_cast<int32_t>(b) < 0) ? static_cast<uint32_t>(std::min(static_cast<int32_t>(a), static_cast<int32_t>(b)))
                                                                          : static_cast<uint32_t>(std::max(static_cast<int32_t>(a), static_cast<int32_t>(b)));
}
[[maybe_unused]] static uint32_t fp_minb(uint32_t a, uint32_t b) {  // VMINI on raw lanes
    return (static_cast<int32_t>(a) < 0 && static_cast<int32_t>(b) < 0) ? static_cast<uint32_t>(std::max(static_cast<int32_t>(a), static_cast<int32_t>(b)))
                                                                          : static_cast<uint32_t>(std::min(static_cast<int32_t>(a), static_cast<int32_t>(b)));
}
[[maybe_unused]] static float fmaxv(float a, float b) { return u2f(fp_maxb(fbits(a), fbits(b))); }
[[maybe_unused]] static float fminv(float a, float b) { return u2f(fp_minb(fbits(a), fbits(b))); }
[[maybe_unused]] static float clamp01(float t) { return fminv(fmaxv(t, 0.0f), 1.0f); }
[[maybe_unused]] static float qsqrt(float x) { return vuc(std::sqrt(std::fabs(vuc(x)))); }
[[maybe_unused]] static void sq4f(uint32_t a, float x, float y, float z, float w) { swc1(a, x); swc1(a + 4, y); swc1(a + 8, z); swc1(a + 0xc, w); }

// 0x1efbd0 (custom register ABI: tree t8, z v1, y v0, x at -> at): the level collision tree, three
// levels of (u16 first, u16 count, entries); 0 outside or for an empty cell.
[[maybe_unused]] static uint32_t coll_tree_cell(uint32_t tree, int32_t x, int32_t y, int32_t z) {
    int32_t i = z - static_cast<int32_t>(lhu(tree));
    if (i < 0 || static_cast<int32_t>(lhu(tree + 2)) - i <= 0) return 0;
    const uint32_t zl = static_cast<uint32_t>(lhu(tree + 4 + static_cast<uint32_t>(i) * 2)) << 2;
    if (zl == 0) return 0;
    const uint32_t yn = zl + tree;
    i = y - static_cast<int32_t>(lhu(yn));
    if (i < 0 || static_cast<int32_t>(lhu(yn + 2)) - i <= 0) return 0;
    const uint32_t yl = lwu(yn + 4 + static_cast<uint32_t>(i) * 4);
    if (yl == 0) return 0;
    const uint32_t xn = yl + tree;
    i = x - static_cast<int32_t>(lhu(xn));
    if (i < 0 || static_cast<int32_t>(lhu(xn + 2)) - i <= 0) return 0;
    return lwu(xn + 4 + static_cast<uint32_t>(i) * 4);
}
[[maybe_unused]] static int16_t h16(int32_t v) { return static_cast<int16_t>(static_cast<uint16_t>(v)); }

}  // namespace collide_detail
}  // namespace rac1

#endif  // RAC1_GAME_COLLISION_COLLIDE_COMMON_H
