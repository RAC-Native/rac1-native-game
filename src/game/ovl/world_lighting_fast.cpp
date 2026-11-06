// RAC1_WORLD_LIGHTING_FAST: the tfrag entries' static lighting (0x295350) as written by hand (world_lighting_tfrag.cpp) or as
// transliterated (world_lighting.cpp's world_tfrag_lighting_tr, the reference).
//   0  the transliteration, as before;
//   1  the hand-written routine (the default); the transliteration still runs where it cannot (a VU0 micro program running,
//      RAC1_PERF_VU0_NF=0);
//   2  both, compared at every call (world_cull_hand.h's hand_check); the game goes on with the transliteration's results.
// orig: (host)
#include <game/ovl/world_cull_hand.h>
#include <game/ovl/world_lighting.h>

#include <runtime/kernel/ee_memory.h>

#include <cstdlib>

namespace rac1 {

namespace {

int lighting_mode() {
    static const int m = [] {
        const char* e = std::getenv("RAC1_WORLD_LIGHTING_FAST");
        return e != nullptr ? std::atoi(e) : 1;
    }();
    return m;
}

world_hand::HandCheck g_tfrag{"[world-lighting]", "RAC1_WORLD_LIGHTING_FAST", "tfrag lighting"};

}  // namespace

// orig: 0x00295350 -- see world_lighting.cpp's world_tfrag_lighting_tr
void world_tfrag_lighting(uint32_t list) {
    const int m = lighting_mode();
    if (m == 0) return world_tfrag_lighting_tr(list);
    if (m == 1 || ee_journal_active()) {
        if (!world_hand::tfrag_lighting(list)) world_tfrag_lighting_tr(list);
        return;
    }
    if (!vu0l::usable()) return world_tfrag_lighting_tr(list);
    world_hand::hand_check(g_tfrag, [list] { return world_hand::tfrag_lighting(list); }, [list] { world_tfrag_lighting_tr(list); });
}

}  // namespace rac1
