// The deadly zones of Blarg (level 6), Batalia (8), Gemlik Base (13) and Veldin's finale (18), class 1039: two lists of 32 of
// the level's boxes (pvars +0 and +0x80, -1 unused). Ratchet in a box of the first list falls (the hero state 0x77; in a
// vehicle he is lost at once), in one of the second he is lost (the screen faded, hero_lost). Not while he is in the state
// 0x32 riding (hero +0x15f0, alive) with +0x15f4 0x45. The four levels' copies are the same code, relinked; no level word, the
// hero 0x13f350 is the base ELF's. Header shared_zones.h.
#include <game/ovl/shared_zones.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/screen_fade.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
constexpr int32_t kBoxes = 32;
constexpr uint32_t kFall = 0x77;

void lost() {
    screen_fade(frame_scale_i(10));
    hero_lost();
}
}  // namespace

// orig: L6:0x002f7930 L8:0x00302a78 L13:0x003036f0 L18:0x002ec858
void deadly_zones_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        return;
    case 1:
        break;
    default:
        return;
    }
    if (lw(HERO + 0x2084) == 0x32) {
        const uint32_t ride = lwu(HERO + 0x15f0);
        if (ride != 0 && lbu(ride + 0x20) != 0xfe && lbu(ride + 0x20) != 0xfd && lh(HERO + 0x15f4) == 0x45) return;
    }
    const uint32_t at = HERO + 0x80;
    for (int32_t i = 0; i < kBoxes; ++i) {  // (falling)
        const int32_t box = lw(pv + static_cast<uint32_t>(i) * 4);
        if (box < 0 || point_in_box(at, box) == 0) continue;
        if (lbu(HERO + 0x20a4) != 0) lost();
        else if (lw(HERO + 0x2084) != static_cast<int32_t>(kFall)) hero_set_state(kFall, 1);
    }
    for (int32_t i = 0; i < kBoxes; ++i) {  // (lost)
        const int32_t box = lw(pv + 0x80 + static_cast<uint32_t>(i) * 4);
        if (box < 0 || point_in_box(at, box) == 0) continue;
        lost();
    }
}

}  // namespace rac1
