// The gadgets the hero picks up before a panel of four pulsing lights (shared_panel_lights.cpp): Aridia's class 1005, the
// Trespasser, and Blarg's class 1016, one update (levels 2 and 6, the same code relinked, compared word by word: only
// relocated words differ; it tests its own class number for the item, 0x1a or 0x16, and the scene), and class 1428 of
// Quartu and Drek's fleet (levels 15 and 17). Their glitter's pvars are the gold bolt's second build (glitter_pvars_init at
// 0). Level-own words: class 1428's size factor (per level below); the engine's (OV): the collision's floor height
// 0x173e68, the game mode 0x15f5c4, the frame count 0x15f5cc, the screen's fade 0x15f3fc, the message timer 0x15f640, the
// camera 0x166e00, the scripted camera 0x179410; the base ELF's (bare): the hero 0x13f350, the items owned 0x13d4e8, the
// story flags 0x13d38a / 0x13d400, the level number 0x15ed84, the frame scales 0x15ed6c / 0x15ed70. Header
// shared_gadget_pickups.h.
#include <game/ovl/shared_gadget_pickups.h>

#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hydro.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_gold_bolt.h>
#include <game/ovl/shared_panel_lights.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's place (+0x10 its turn)
constexpr int32_t TRESPASSER_CLASS = 0x3ed;

float frame_dt() { return lwc1(0x0015ed6c); }
}  // namespace

// orig: L2:0x002ea210 L6:0x002f4638
void gadget_pickup_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float spin = fpu_div(u2f(0x40490fdbu), static_cast<float>(frame_scale_i(0x3c)));  // (half a turn a second)
    const bool trespasser = lh(m + 0xa6) == TRESPASSER_CLASS;
    const int32_t item = trespasser ? 0x1a : 0x16;
    const uint32_t fade = OV(0x0015f3fc);
    switch (lbu(m + 0x20)) {  // (the level's jump table of six states: 0x1fad40 on Aridia)
    case 0:  // (L2:0x002ea294) set up on the floor, turned up; gone once owned
        glitter_pvars_init(m, 0);
        if (lbu(0x0013d4e8 + static_cast<uint32_t>(item)) != 0) {
            moby_retire(m);
            return;
        }
        floor_probe_v(0.5f, V(m + 0x10), 0);
        swc1(m + 0x44, u2f(0x40490fdbu));  // (pi)
        swc1(m + 0x40, u2f(0x3fc90fdbu));  // (pi / 2)
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(OV(0x00173e68)), u2f(0x3e4ccccdu)));  // (0.2 above the floor)
        return;
    case 1:  // (L2:0x002ea30c) spinning; every tenth frame: the hero by it, its scene
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), spin));
        panel_lights(m, 0);
        if (ee_rem(lw(OV(0x0015f5cc)), 10) != 0) return;
        if (!(vec2_distance(V(m + 0x10), V(HERO_POS)) < 1.0f)) return;
        if (!(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8))) < 2.0f)) return;
        sb(m + 0x20, 3);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        sb(m + 0x31, 0);
        sw(m + 0x94, 0);
        cutscene_start(trespasser ? 4 : 2);
        return;
    case 3:  // (L2:0x002ea3e4) given (and equipped but in the hero's state 1 / 0x16 has its message cleared)
        if (lw(OV(0x0015f5c4)) == 2) return;
        if (lbu(0x001413f4) == 0) {
            item_give(item, 1);
        } else if (item == 0x16) {
            item_give(0x16, 0);
            sw(OV(0x0015f640), 0);
        } else {
            item_give(item, 0);
        }
        if (!trespasser) {
            level_change_request(0, -1);
            moby_retire(m);
            return;
        }
        // the Trespasser: its message and hint, the screen faded in, the hero put back, the camera's move
        sub_00263d40(TXT(0x7df), -1);
        sub_002039a0();
        sub_00203908(0x7d9, 0x13);
        swc1(fade, 1.0f);
        sb(m + 0x20, 4);
        hero_place(HERO_POS, HERO_POS + 0x10, 0x72, 1);
        return;
    case 4: {  // (L2:0x002ea4bc) the camera moved along a fixed line toward the panel, eased; the fade with it
        const uint32_t sp = ee_stack_alloc(0xc0);
        const uint32_t from = sp, to = sp + 0x10, turn0 = sp + 0x20, turn1 = sp + 0x30, at = sp + 0x40, turn = sp + 0x50;
        for (uint32_t k = 0; k < 0x40; k += 4) sw(sp + k, 0);
        swc1(from, u2f(0x42b66666u));        // (91.2)
        swc1(from + 4, u2f(0x43871ae1u));    // (270.21)
        swc1(from + 8, u2f(0x4281b333u));    // (64.85)
        swc1(to, u2f(0x42d5f5c3u));          // (106.98)
        swc1(to + 4, u2f(0x4388bae1u));      // (273.46)
        swc1(to + 8, u2f(0x427a999au));      // (62.65)
        swc1(turn0, u2f(0x3d8f5c29u));       // (0.07)
        swc1(turn0 + 4, u2f(0x3ea8f5c3u));   // (0.33)
        swc1(turn0 + 8, u2f(0x3f0ccccdu));   // (0.55)
        swc1(turn1, u2f(0x3d8f5c29u));       // (0.07)
        swc1(turn1 + 4, u2f(0x3edc28f6u));   // (0.43)
        swc1(turn1 + 8, u2f(0x3f3851ecu));   // (0.72)
        const float k = u2f(0x3e19999au);    // (0.15)
        const float accel = fpu_mul(lwc1(0x0015ed70), k);
        seek_1d(pv + 0x40, pv + 0x44, 1.0f, accel, accel, fpu_mul(frame_dt(), k));
        vec3_lerp(V(at), V(from), V(to), lwc1(pv + 0x40));
        for (uint32_t c = 8;; c -= 4) {  // (z, y, x: each angle eased the short way)
            const float a = lwc1(turn0 + c);
            swc1(turn + c, angle_add(fpu_mul(angle_sub(lwc1(turn1 + c), a), lwc1(pv + 0x40)), a));
            if (c == 0) break;
        }
        const uint32_t cam = lwu(OV(0x00166e00));
        if (cam != 0 && lh(cam + 0x86) != 5) camera_scripted_set(at, turn, 1, 0, 0);
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(turn);
        // the screen faded out (to 1) past a fifth of the way while no scripted camera runs, else in
        float shade;
        if (lw(OV(0x00179410)) == 0 && lw(OV(0x00179434)) == -1 && u2f(0x3e4ccccdu) < lwc1(pv + 0x40)) {  // (0.2)
            approach_f(fade, 1.0f, fpu_mul(frame_dt(), 4.0f));
            shade = lwc1(fade);
        } else {
            approach_f(fade, 0.0f, fpu_mul(frame_dt(), 4.0f));
            shade = lwc1(fade);
        }
        if (shade == 1.0f) {
            hero_place(HERO_POS, HERO_POS + 0x10, 0, 0);
            hydro_2eac18(0);
            sb(m + 0x20, 5);
        }
        ee_stack_free(0xc0);
        return;
    }
    case 5:  // (L2:0x002ea758) the screen faded in; then the level changed
        approach_f(fade, 0.0f, fpu_mul(frame_dt(), 4.0f));
        if (lwc1(fade) == 0.0f) {
            level_change_request(0, -1);
            moby_retire(m);
        }
        return;
    default:  // (L2:0x002ea7ac: 2 and past the table)
        return;
    }
}

// orig: L15:0x002ebd78 L17:0x002f1790
void class1428_update(uint32_t m) {
    if (lw(0x0015ed84) == 0xf) return;  // (not on Quartu itself)
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(frame_dt(), u2f(0x3fc90fdbu))));  // (a quarter turn a second)
    const float size = on_level(QUARTU) ? lwc1(L15_ADDR(0x001621a0)) : lwc1(L17_ADDR(0x001623a0));
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), size));
    switch (lbu(m + 0x20)) {
    case 0:  // set up (gone once owned), lifted by 1
        glitter_pvars_init(m, 0);
        if (lbu(0x0013d38a) != 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        return;
    case 1:  // spinning; the hero within 3 (not in the state 0x32): its scene
        panel_lights(m, 0);
        if (!(vec3_distance(V(m + 0x10), V(HERO_POS)) < 3.0f)) return;
        if (lw(0x001413d4) == 0x32) return;
        sh(m + 0x34, lhu(m + 0x34) | 0x41);
        cutscene_start(1);
        sb(m + 0x20, 2);
        return;
    case 2:  // out of the scene: its message, owned, the level changed
        if (lw(OV(0x0015f5c4)) == 2) return;
        sub_00263d40(TXT(0x426a), -1);
        sw(OV(0x0015f640), static_cast<uint32_t>(frame_scale_i(0xb4)));
        sb(0x0013d400, 1);
        sb(0x0013d38a, 1);
        sb(m + 0x20, 3);
        level_change_request(0, -1);
        return;
    case 3:
        moby_retire(m);
        return;
    default:
        return;
    }
}

}  // namespace rac1
