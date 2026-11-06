// RAC1_DROP_SHADOWS_FAST: the moby drop shadows' VU0 routines (shadow_points 0x285a68, shadow_points_clip 0x286848, shadow_point_draw
// 0x2860a8, shadow_segment_draw 0x2861f8, shadow_quads 0x285d30, shadow_packets 0x2868c0) as written by hand
// (moby_shadows_hand.cpp) or as transliterated (moby_shadows_asm.cpp's *_tr, the reference).
//   0  the transliterations, as before;
//   1  the hand-written routines (the default); the transliteration still runs where they cannot (a VU0 micro program running,
//      RAC1_PERF_VU0_NF=0);
//   2  both, compared at every call (world_cull_hand.h's hand_check); the game goes on with the transliteration's results.
// orig: (host)
#include <game/ovl/moby_shadows.h>
#include <game/ovl/moby_shadows_hand.h>
#include <game/ovl/world_cull_hand.h>

#include <runtime/kernel/ee_memory.h>

#include <cstdlib>

namespace rac1 {

namespace {

int shadows_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_DROP_SHADOWS_FAST");
        return e != nullptr ? std::atoi(e) : 1;
    }();
    return m;
}

world_hand::HandCheck g_checks[6] = {{"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_points"},
                                     {"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_points_clip"},
                                     {"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_point_draw"},
                                     {"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_segment_draw"},
                                     {"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_quads"},
                                     {"[drop-shadows]", "RAC1_DROP_SHADOWS_FAST", "shadow_packets"}};

template <class H, class T>
void run(int which, H hand, T tr) {
    const int m = shadows_mode();
    if (m == 0) return tr();
    if (m == 1 || ee_journal_active()) {
        if (!hand()) tr();
        return;
    }
    if (!vu0l::usable()) return tr();
    world_hand::hand_check(g_checks[which], hand, tr);
}

}  // namespace

// orig: 0x00285a68 -- see moby_shadows_asm.cpp's shadow_points_tr
void shadow_points() { run(0, [] { return shadow_hand::points(); }, [] { shadow_points_tr(); }); }
// orig: 0x00286848
void shadow_points_clip(uint32_t a0, uint32_t a1) {
    run(1, [=] { return shadow_hand::points_clip(a0, a1); }, [=] { shadow_points_clip_tr(a0, a1); });
}
// orig: 0x002860a8
void shadow_point_draw(uint32_t a0) { run(2, [=] { return shadow_hand::point_draw(a0); }, [=] { shadow_point_draw_tr(a0); }); }
// orig: 0x002861f8
void shadow_segment_draw(uint32_t a0) { run(3, [=] { return shadow_hand::segment_draw(a0); }, [=] { shadow_segment_draw_tr(a0); }); }
// orig: 0x00285d30
void shadow_quads(uint32_t a0, uint32_t a1, uint32_t a2) {
    run(4, [=] { return shadow_hand::quads(a0, a1, a2); }, [=] { shadow_quads_tr(a0, a1, a2); });
}
// orig: 0x002868c0
void shadow_packets(uint32_t a0, uint32_t a1) { run(5, [=] { return shadow_hand::packets(a0, a1); }, [=] { shadow_packets_tr(a0, a1); }); }

}  // namespace rac1
