// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: two props of the level's scenes, class 1091 (the update of
// classes 1091 .. 1098 and 1103: a piece floating up and down that keeps still while Ratchet is held in a scene, and once
// set drifting, drifts and spins away until the scene lets him go) and class 1551 (a smoke source in the cutscene 5).
// Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR, the
// base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
// Ratchet held in a scene (the hero's state 0x72)
bool hero_held() { return lw(0x001413d4) == 0x72; }
}  // namespace

// Class 1091 (and 1092 .. 1098, 1103), a floating piece. pvars: its drift +0 (x, y, z; +0xc its rest height, also the
// drift's w lane) and its spin +0x10 (added to its rotation each frame). States: 0 set up (+0x32 0x200, +0x30 0xff, its
// rest height its height) (1); 1 floats: its height the rest height plus the level's amplitude 0x162090 times the sine of
// the level's frame count 0x15f5cc over 600 frames (one swing every 10 s), kept at its rest height while Ratchet is held in
// a scene; 2 (set by others) drifts (the drift slowed by 0.985 a frame while its w lane is above 1) and spins, retired as
// soon as Ratchet is not held in a scene. Without pvars it is retired.
// orig: L6:0x00300df0
void blg_class1091_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) {
        moby_retire(m);
        return;
    }
    switch (lbu(m + 0x20)) {
    case 0:
        sh(m + 0x32, 0x200);
        sb(m + 0x30, 0xff);
        swc1(pv + 0xc, lwc1(m + 0x18));
        sb(m + 0x20, 1);
        break;
    case 1: {
        if (hero_held()) {
            swc1(m + 0x18, lwc1(pv + 0xc));
            break;
        }
        const float phase = fpu_mul(fpu_div(int_to_float(ee_rem(lw(L6_ADDR(0x0015f5cc)), 600)), 600.0f), 6.28318f);
        swc1(m + 0x18, fpu_add(lwc1(pv + 0xc), fpu_mul(lwc1(L6_ADDR(0x00162090)), sin_vu(phase))));
        break;
    }
    case 2:
        if (1.0f < lwc1(pv + 0xc)) vec3_scale(V(pv), V(pv), 0.985f);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
        vec3_add(V(m + 0x40), V(m + 0x40), V(pv + 0x10));
        if (!hero_held()) moby_retire(m);
        break;
    default:
        break;
    }
}

// Class 1551, a smoke source of the cutscene 5. States: 0 set up (+0x30 0xff) (1); 1 while the cutscene 5 runs (0x15f5c4
// 2, the scene block 0x16cfe0's number +0x30), up to its frame 127 (+0x34), its third moby (+0x180) smokes
// (scene_moby_smoke: a class-750 moby only).
// orig: L6:0x00309348
void blg_class1551_update_l6(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    case 1:
        if (lw(L6_ADDR(0x0015f5c4)) != 2 || lw(L6_ADDR(0x0016d010)) != 5) break;
        if (frame_scale_i(0x7f) < lw(L6_ADDR(0x0016d014))) break;
        scene_moby_smoke(lwu(L6_ADDR(0x0016d160)));
        break;
    default:
        break;
    }
}

}  // namespace rac1
