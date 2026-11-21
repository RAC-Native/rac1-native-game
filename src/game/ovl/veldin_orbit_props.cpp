// Drek's fleet's (level 17) classes 1562, 1873 and 1876, ported by hand from the level's own code: class 1562 keeps a
// cutscene's moby smoking, classes 1873 / 1876 are props that break when a hit does damage (bolts, debris, a broken copy
// of themselves). The original's order of calls kept. Level 17 only: `orig: L17:`, the level's own data L17_ADDR, base ELF
// bare. Registered in veldin_orbit_mobys.cpp.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
// a prop's state: 0 set up (-> 1), 1 whole (a hit (mask 0x10000) with damage above 0: -> 2), 2 breaks; true when it breaks
bool prop_breaks(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        return false;
    case 1:
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return false;
    case 2:
        return true;
    default:
        return false;
    }
}
}  // namespace

// Class 1562: in a cutscene (the game mode L17:0x15f5c4 2) whose scene state (the scene block L17:0x16d160 +0x30) is 0 or
// 2, the scene's moby (+0x180) smokes (scene_moby_smoke). States: 0 set up (+0x30 0xff), 1 on.
// orig: L17:0x002f2e78
void vo_class1562_update_l17(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    case 1: {
        if (lw(L17_ADDR(0x0015f5c4)) != 2) break;
        const int32_t scene = lw(L17_ADDR(0x0016d190));
        if (scene != 0 && scene != 2) break;
        scene_moby_smoke(lwu(L17_ADDR(0x0016d160) + 0x180));  // (the original's index 2 of the words at +0x178)
        break;
    }
    default:
        break;
    }
}

// Class 1873, a prop: broken, its sound, its bolts, two bursts of debris (0x753, 0x756), its broken copy (class 0x752).
// orig: L17:0x002f5260
void vo_class1873_update_l17(uint32_t m) {
    if (!prop_breaks(m)) return;
    moby_sound_start(0, 0, m);
    moby_drop_bolts(m, 0, -1);
    plant_bolts_drop_l1(m);
    debris_burst(m, 0x753, 1, 0x753, 1, 0xb, 2);
    debris_burst(m, 0x756, 1, 0x756, 1, 1, 2);
    moby_clone_l1(m, 0x752);
    moby_retire(m);
}

// Class 1876, a prop: broken, its sound, its bolts, one piece of debris (0x753) and a burst (0x756), its broken copy (class
// 0x755).
// orig: L17:0x002f5390
void vo_class1876_update_l17(uint32_t m) {
    if (!prop_breaks(m)) return;
    moby_sound_start(0, 0, m);
    moby_drop_bolts(m, 0, -1);
    plant_bolts_drop_l1(m);
    const uint32_t still = L17_ADDR(0x0015f580);  // (a zero vector: the debris' velocity, spin and size)
    debris_spawn(m, 0x753, m + 0x10, m + 0x40, 0, 0, still, still, still, 0.0f);
    debris_burst(m, 0x756, 1, 0x756, 1, 1, 2);
    moby_clone_l1(m, 0x755);
    moby_retire(m);
}

}  // namespace rac1
