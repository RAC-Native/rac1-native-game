// Orxon's own moby classes (level 10): the classes of the level's scenes and messages: 1326 (the vendor of the two
// purchases that start a scene), 1344 (the help messages and the Gadgebot run's record) and 1555 (the scenes' smoke).
// Header orxon_classes.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/orxon_classes.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/veldin_mobys.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

// Class 1326: the vendor. 0: the hero in front of it (within 90 degrees of its heading) and within 4, its pause (pvars +4)
// over and no scene playing: it talks; the first purchase (0x13d38c) for 4,000 bolts (0x15ed98), else the second (0x13d38d)
// for 30,000: its offer (message 0x271e / 0x271f), or "not enough bolts" (0x2720 / 0x2721); the offer shown and the button
// 0x10 of the pad (0x13cae4) pressed: bought (the flag, the bolts paid, the hero's state 0x1415f8 5 / 8 and 0x15eda0 the
// same) and the scene 0 / 1 started (1). 1, once the scene is over: the level reloaded where it was (the level change
// request 0, -1), the message 0x271c / 0x271d, L10:0x15f640 180 frames, its pause 240 frames (0).
// orig: L10:0x002e8358
void orx_class1326_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    tick32(pv + 4);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        if (lw(OV(0x0015f5c4)) == 2) return;
        level_change_request(0, -1);
        sub_00263d40(lbu(0x0013d38d) == 0 ? 0x271c : 0x271d, -1);
        sw(OV(0x0015f640), 0xb4);
        sw(pv + 4, static_cast<uint32_t>(frame_scale_i(0xf0)));
        sb(m + 0x20, 0);
        return;
    }
    if (st != 0) return;
    const float to_hero = atan2_f(fpu_sub(lwc1(HERO_POS), lwc1(m + 0x10)), fpu_sub(lwc1(HERO_POS + 4), lwc1(m + 0x14)));
    if (!(angle_dist(lwc1(m + 0x48), to_hero) < u2f(0x3fc90fdbu))) return;
    if (!(vec2_distance(V(m + 0x10), V(HERO_POS)) < 4.0f)) return;
    if (lw(pv + 4) != 0) return;
    if (lw(OV(0x0015f5c4)) == 2) return;
    talker_word_set(m, 1);
    uint32_t bought, state;
    int32_t scene, price;
    if (lbu(0x0013d38c) == 0) {  // the first purchase
        if (lw(0x0015ed98) < 0xfa1) {
            hud_message_request(1, 0x2720);
            return;
        }
        const bool shown = hud_message_request(1, 0x271e) != 0;
        if ((lwu(0x0013cae4) & 0x10u) == 0 || !shown) return;
        bought = 0x0013d38c;
        state = 5;
        price = 0xfa0;
        scene = 0;
    } else {  // the second
        if (lbu(0x0013d38d) != 0) return;
        if (lw(pv + 4) != 0) return;
        if (lw(0x0015ed98) < 0x7531) {
            hud_message_request(1, 0x2721);
            return;
        }
        const bool shown = hud_message_request(1, 0x271f) != 0;
        if ((lwu(0x0013cae4) & 0x10u) == 0 || !shown) return;
        bought = 0x0013d38d;
        state = 8;
        price = 0x7530;
        scene = 1;
    }
    sb(bought, 1);
    sw(0x001415f8, state);
    sw(0x0015ed98, static_cast<uint32_t>(lw(0x0015ed98) - price));
    sw(0x0015eda0, state);
    cutscene_start(scene);
    sb(m + 0x20, 1);
}

namespace {
// the hero standing (his state 0x1413dc 0 or 1, or 9) and no message or scene of the help system running (0x179410 /
// 0x179434 at Veldin's addresses)
bool may_help() {
    const uint32_t s = lwu(0x001413dc);
    if (!(s < 2) && s != 9) return false;
    return lw(OV(0x00179410)) == 0 && lw(OV(0x00179434)) == -1;
}
}  // namespace

// Class 1344: Orxon's help messages and the Gadgebot run's record. L10:0x161f38 1 while the hero is in box pvars +0x1c.
// From state 1, each once (its flag 0x141bxx / 0x141cxx): in box +0 message 0x2710; in box +4 0x2712 (before the first
// purchase) or 0x2713 (after it, before the second); out of a scene with the item 0x13d504 0x2711; in box +8 (no 0x13d4cb)
// 0x2715. Then, while a Gadgebot (class 0x359) is not standing by (state 1): both mobys pvars +0x10 / +0x14 within 15 of
// the hero, 0x2717; in box +0x18 at the goal moby pvars +0x20 (class 0x3f7, not in state 4): the run's time
// (0x15eea4, in units of 600 frames) against the record 0x141cc2 (the first run: counted 0x141cc0, the level's bit in
// 0x141cc4 with 0x80000000; too slow, 18 seconds, or no record: 0x2718).
// orig: L10:0x002e85b8
void orx_class1344_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sw(L10_ADDR(0x00161f38), point_in_box(HERO_POS, lw(pv + 0x1c)) != 0 ? 1u : 0u);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (point_in_box(HERO_POS, lw(pv)) != 0 && may_help() && lhu(0x00141b10) == 0) sub_00203908(0x2710, 0x35);
    if (point_in_box(HERO_POS, lw(pv + 4)) != 0 && may_help()) {
        if (lbu(0x0013d38c) == 0) {
            if (lhu(0x00141b20) == 0) sub_00203908(0x2712, 0x37);
        } else if (lbu(0x0013d38d) == 0 && lhu(0x00141b28) == 0) {
            sub_00203908(0x2713, 0x38);
        }
    }
    if (lw(OV(0x0015f5c4)) == 0 && lbu(0x0013d504) != 0 && lw(OV(0x00179410)) == 0 && lw(OV(0x00179434)) == -1 && lhu(0x00141b18) == 0)
        sub_00203908(0x2711, 0x36);
    if (point_in_box(HERO_POS, lw(pv + 8)) != 0) {
        const uint32_t s = lwu(0x001413dc);
        if ((s < 2 || s == 9) && lbu(0x0013d4cb) == 0 && lw(OV(0x00179410)) == 0 && lw(OV(0x00179434)) == -1 && lhu(0x00141c48) == 0)
            sub_00203908(0x2715, 0x5c);
    }
    bool restless = false;  // a Gadgebot not standing by
    for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28))
        if (lh(o + 0xa6) == 0x359 && (lbu(o + 0x20) ^ 1u) != 0) restless = true;
    if (!restless) return;
    if (lw(pv + 0x10) != -1 && vec2_distance(V(HERO_POS), V(moby_at(lw(pv + 0x10)) + 0x10)) < 15.0f && lw(pv + 0x14) != -1 &&
        vec2_distance(V(HERO_POS), V(moby_at(lw(pv + 0x14)) + 0x10)) < 15.0f && lhu(0x00141cb8) == 0)
        sub_00203908(0x2717, 0x6a);
    if (point_in_box(HERO_POS, lw(pv + 0x18)) == 0) return;
    if (lw(pv + 0x20) == -1) return;
    const uint32_t goal = moby_at(lw(pv + 0x20));
    if (lh(goal + 0xa6) != 0x3f7 || lbu(goal + 0x20) == 4) return;
    constexpr uint32_t REC = 0x00141968;  // (+0x358 the runs, +0x35a the record, +0x35c the levels' bits)
    if (lhu(REC + 0x358) == 0) {  // the first run
        sh(REC + 0x358, 1);
        const int32_t t = ee_div(frame_scale_i(lw(0x0015eea4)), 600);
        if (static_cast<int32_t>(lhu(REC + 0x35a)) < t) sh(REC + 0x35a, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
        sw(REC + 0x35c, lwu(REC + 0x35c) | (1u << (level_number() & 31)) | 0x80000000u);
        return;
    }
    const int32_t left = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(REC + 0x35a)) * 600;
    const int32_t limit = float_to_int(fpu_mul(int_to_float(frame_scale_i(0x12)), 60.0f));
    if (limit < left || static_cast<int32_t>(lhu(REC + 0x35a)) * 600 == 0) {
        sub_00203908(0x2718, 0x6b);
        return;
    }
    const int32_t t = ee_div(frame_scale_i(lw(0x0015eea4)), 600);
    if (!(static_cast<int32_t>(lhu(REC + 0x35a)) < t)) return;
    sh(REC + 0x35a, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
}

// Class 1555: the scenes' smoke. Started (1, +0x30 0xff); during a scene (game mode 2) of the scene list 0x16c860 (Veldin's
// address) in scene 3 its moby 1 smokes, in scenes 5 / 6 its moby 2, in scene 7 its moby 3 (scene_moby_smoke).
// orig: L10:0x002eacd8
void orx_class1555_update_l10(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (lw(OV(0x0015f5c4)) != 2) return;
    constexpr OvConst<0x0016c860u> SC{};
    const int32_t scene = lw(SC + 0x30);
    if (scene != 3 && scene != 5 && scene != 6 && scene != 7) return;
    uint32_t slot;
    if (scene == 3) {
        slot = 1;
    } else if (static_cast<uint32_t>(scene - 5) < 2u) {
        slot = 2;
    } else {
        slot = 3;  // (scene 7; any other would take slot 0)
    }
    scene_moby_smoke(lwu(SC + 0x178 + slot * 4));
}

}  // namespace rac1
