// RAC1_SKY_FAST: the sky's routines 0x28c14c (a part's vertices to the screen), 0x28c3c0 / 0x28c298 (its triangles' packets, with
// a texture / with vertex colours), 0x28c680 (the spheres against the view) and 0x28bd58 (the sprites) as
// written by hand (sky_hand.cpp) or as transliterated (sky_ee.cpp's ovl_*_ref_tr, the reference; the generated callers call these
// switches by the routines' names).
//   0  the transliterations, as before;
//   1  the hand-written routines (the default); the transliteration still runs where they cannot (a VU0 micro program running,
//      RAC1_PERF_VU0_NF=0);
//   2  both, compared at every call (world_cull_hand.h's hand_check, and the returned $v0 / $f0); the game goes on with the
//      transliteration's results.
// orig: (host)
#include <game/ovl/sky_ee.h>
#include <game/ovl/sky_hand.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>

namespace rac1 {

namespace {

int sky_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_SKY_FAST");
        return e != nullptr ? std::atoi(e) : 1;
    }();
    return m;
}

world_hand::HandCheck g_checks[5] = {{"[sky]", "RAC1_SKY_FAST", "sky vertices 0x28c14c"},
                                     {"[sky]", "RAC1_SKY_FAST", "sky spheres 0x28c680"},
                                     {"[sky]", "RAC1_SKY_FAST", "sky triangles 0x28c3c0"},
                                     {"[sky]", "RAC1_SKY_FAST", "sky colour triangles 0x28c298"},
                                     {"[sky]", "RAC1_SKY_FAST", "sky sprites 0x28bd58"}};

// the switch for a routine whose hand-written form returns $v0 (`hand(ret)`), $f0 0
template <class H, class T>
TrRet run(int which, H hand, T tr) {
    const int m = sky_mode();
    if (m == 0) return tr();
    if (m == 1 || ee_journal_active()) {
        uint32_t v0 = 0;
        if (hand(v0)) return TrRet{v0, 0.0f};
        return tr();
    }
    if (!vu0l::usable()) return tr();
    uint32_t v0 = 0;
    bool ran = false;  // (RAC1_HAND_CHECK_EVERY: a call not checked runs the transliteration alone)
    TrRet ref{};
    const unsigned long long before = g_checks[which].differing;
    world_hand::hand_check(g_checks[which], [&] { ran = true; return hand(v0); }, [&] { ref = tr(); });
    if (ran && (ref.v0 != v0 || f2u(ref.f0) != 0)) {
        if (g_checks[which].differing == before) ++g_checks[which].differing;
        std::fprintf(stderr, "%s %s: returned %08x, the transliteration %08x / %08x\n", g_checks[which].tag, g_checks[which].name, v0,
                     ref.v0, f2u(ref.f0));
    }
    return ref;
}

}  // namespace

// orig: 0x0028c14c -- see sky_ee.cpp's ovl_28c14c_ref_tr
TrRet ovl_28c14c_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    return run(0, [=](uint32_t& v0) { return sky_hand::project(a0, a1, a2, v0); },
               [=] { return ovl_28c14c_ref_tr(a0, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15); });
}
// orig: 0x0028c3c0 -- see sky_ee.cpp's ovl_28c3c0_ref_tr
TrRet ovl_28c3c0_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    return run(2, [=](uint32_t& v0) { return sky_hand::triangles(a0, a1, a2, a3, v0); },
               [=] { return ovl_28c3c0_ref_tr(a0, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15); });
}
// orig: 0x0028bd58 -- see sky_ee.cpp's ovl_28bd58_ref_tr
TrRet ovl_28bd58_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    return run(4, [=](uint32_t& v0) { return sky_hand::sprites(v0); },
               [=] { return ovl_28bd58_ref_tr(a0, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15); });
}
// orig: 0x0028c298 -- see sky_ee.cpp's ovl_28c298_ref_tr
TrRet ovl_28c298_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    return run(3, [=](uint32_t& v0) { v0 = sky_hand::colour_triangles(a0, a1, a2, a3); return true; },
               [=] { return ovl_28c298_ref_tr(a0, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15); });
}
// orig: 0x0028c680 -- see sky_ee.cpp's ovl_28c680_ref_tr
TrRet ovl_28c680_tr(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13,
                    float f14, float f15) {
    return run(1, [=](uint32_t& v0) { v0 = 0; return sky_hand::spheres(a0, a1, a2); },
               [=] { return ovl_28c680_ref_tr(a0, a1, a2, a3, t0, t1, t2, t3, f12, f13, f14, f15); });
}

}  // namespace rac1
