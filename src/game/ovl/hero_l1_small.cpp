// Novalis's (level 1) variants of the small hero-controller functions: the same
// source as Veldin's, built with other features, so the code differs. Addresses are Novalis's.
// Veldin-equivalent globals go through OV (the level's own address); the player block is in the
// base ELF (the same on every overlay).
#include <game/ovl/hero_l1.h>
#include <game/ovl/hero_move_mode.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;

// the hero's feet within 1 of the height h (0.25 above them) and below it, coming down
// orig: (helper) the hazard tests of hero_surface_update (L1 blocks 0x22cfd8 / 0x22d00c)
bool sinking_into(float h) {
    if (!(abs_f(h - (lwc1(P + 0x88) + 0.25f)) < 1.0f)) return false;
    return 0.0f < h - lwc1(P + 0x88);
}
}  // namespace

// Set bit 0 of the hero moby's flags (+0x34). Veldin's 0x233210 also does the second hero moby
// (+0x1620); Novalis's has no such block.
// orig: L1:0x002488e8 L2:0x00236748
void hero_mobys_set_flag1_l1() {
    const uint32_t m = lwu(g::PLAYER + 0x2080);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
}

// orig: L1:0x00248900 L2:0x00236760
void hero_mobys_clear_flag1_l1() {
    const uint32_t m = lwu(g::PLAYER + 0x2080);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
}


// Kind of the current interaction (0 = none). Novalis's has no +0x12e2 check (Veldin's returns 0x6e
// there) and only mode 3 ends it early (Veldin's: modes 1, 2 and 3).
// orig: L1:0x00226f10 L2:0x00216068
uint32_t sub_00205000_l1(int32_t flag) {
    const uint32_t mode = lbu(g::PLAYER + 0x20a4);
    if (mode == 3) return 0;
    if (lw(g::PLAYER + 0x22a8) == 1) return 0x54;
    if (flag == 0) return 0;
    if (lbu(g::PLAYER + 0x20a8) == 0) return 0;
    if (lbu(g::PLAYER + 0x20aa) == 0) return 0;
    if (lh(g::PLAYER + 0x22c8) != 0) return 0;
    const int32_t k = sub_0020d498(0);
    return lwu(OV(0x00179ac0) + static_cast<uint32_t>(k * 0x4c) + 0x24);
}

// Novalis's footstep is the sound alone: level_sound_start(a, foot, c) when `sound`, c = the hero
// holds a moby of class 0xad (PLAYER+0x10e0); no footprint particle (Veldin's: on level 0xc).
// orig: L1:0x00227e48 L2:0x00216fa0
void hero_footstep_l1(int32_t a, uint32_t foot, int32_t sound) {
    if (sound == 0) return;
    const uint32_t held = lwu(g::PLAYER + 0x10e0);
    const int32_t c = held != 0 && lh(held + 0xa6) == 0xad ? 1 : 0;
    level_sound_start(a, static_cast<int32_t>(foot), c, 0, lwu(g::PLAYER + 0x2080));
}

// Hero colour effects (Veldin's 0x2076a8) without the tint fade of PLAYER+0x22f8: blink blue in the
// damage states (0x80 / 0x82 for 30 frames, 0x76 for 20 frames on levels 15 / 17) and refresh the
// held item's colour.
// orig: L1:0x00228e38 L2:0x00217f90
void sub_002076a8_l1() {
    int32_t blink = 0;
    const int32_t st = lw(g::HERO_STATE);
    if (st == 0x80 || st == 0x82) blink = lw(g::PLAYER + 0x198) < frame_scale_i(0x1e) ? 1 : 0;
    const int32_t level = lw(0x0015ed84);
    if ((level == 0xf || level == 0x11) && lw(g::HERO_STATE) == 0x76) {
        if (lw(g::PLAYER + 0x198) < frame_scale_i(0x14)) blink = 1;
    }
    if (blink != 0) {
        uint32_t c[3];
        moby_get_color_hi_v(lwu(g::PLAYER + 0x2080), &c[0], &c[1], &c[2]);
        const int32_t t = lw(g::PLAYER + 0x198);
        if (t - ((t < 0 ? t + 3 : t) >> 2 << 2) < 3) {
            c[0] = c[1] = c[2] = 0;
        } else {
            c[1] = 0x90;
            c[2] = 0xf0;
            c[0] = 0x90;
        }
        moby_set_color_hi(lwu(g::PLAYER + 0x2080), c[0], c[1], c[2]);
        sub_002072c8();
    }
    const int32_t s = lw(g::PLAYER + 0x10b8);
    if (s < 0) return;
    if (lbu(0x0013e520 + static_cast<uint32_t>(s)) == 0) return;
    const uint32_t m = lwu(g::PLAYER + 0x1090);
    if (m == 0) return;
    sub_00207330(m, lwu(g::PLAYER + 0x2080));
}

// The hero's per-frame update (Veldin's 0x2070d0), Novalis's build: the same sequence without the
// rail land / grab checks (0x20cf58 / 0x20d330 do not exist on Novalis); a movement mode
// (+0x20a4) runs the level's own update (L1:0x228000, hero_move_mode_l<N>_tr_1.cpp).
// orig: L1:0x00228870 L2:0x002179c8 L3:0x002002f8 (Kerwan: plus the rail grab check)
void hero_frame_update_l1() {
    const float x = lwc1(P + 0x80);
    const float y = lwc1(P + 0x84);
    if (x < 2.0f || y < 2.0f || 1022.0f < x || 1022.0f < y) {
        screen_fade(frame_scale_i(0x10));
        sb(P + 0x20b1, 1);
        return;
    }
    sh(P + 0x1eb2, 0);
    sh(P + 0x1eb0, 0);
    if (lbu(P + 0x20a4) != 0) {
        hero_move_mode_l1();
        return;
    }
    hero_anim_advance();
    hero_tick();
    hero_surface_update();
    hero_ledge_grab_check();
    if (on_kerwan()) hero_rail_grab_check();  // (Kerwan's build only, L3:0x00205a68 = Veldin 0x20d330)
    hero_input_update();
    if (lbu(P + 0x20a4) != 0) return;
    hero_delayed_sounds();
    hero_look_fx();
    hero_wall_lean();
    hero_head_motion();
    hero_look_at_update();
    sub_0020a9e8();
    hero_head_look();
    clank_glow_blink();
    hero_aim_bones();
    bone_nodes_update();
    hero_moby_update();
    hero_chain_world(0);
    hero_items_update();
    hero_layers_tick();
    sub_00207c48();
    sub_00207a58();
    sub_00206cf8();
    hero_camera_7_update();
    hero_footsteps_update();
    cheat_input();
    sub_001f6c20();  // (empty; the original hands it the hero moby)
    if (lw(P + 0x208c) != 0x16 && lw(P + 0x2084) != 0x32) map_reveal(P + 0x80);
    moby_update_light(lwu(P + 0x2080));
    sub_002076a8();
    hero_shadow_update();
}

// The surface under the hero (Veldin's 0x20b960), Novalis's build: the same flags (+0x12e0 consumed
// and reset to -1; +0x12ed kept) minus the lava (3: +0x12e6), the +0x12e2 (7) and +0x12e5 (1) and
// deep-water (2: only the first block sets +0x12e7) flags and their forced states (the deadly water,
// the lava, the quicksand's 0x7b). Kinds: 2 deep water (+0x12e7; the 0xad moby +0x10e0 sets +0x308),
// 0xe shallow water, 0 water (+0x12e4, the depth +0x22a4, +0x20a9 wading), 0xb (+0x12eb), 4 slippery
// (+0x12e3), 0xd (+0x12ec), 8 / 0xc (+0x12ea), 9 (+0x12ee); on level 0xd a type-0xb wall at the gait
// centre sinks the hero (0x7b). Then 0x31 sliding on slippery ground (not sub-states 0x10 / 0x14 /
// 7) and 0x7f for +0x12ec once sunk in and coming down.
// orig: L1:0x0022cd48
void hero_surface_update_l1() {
    const uint32_t keep = lbu(P + 0x12ed);
    const int32_t kind = lh(P + 0x12e0);
    mem_clear128(P + 0x12e0, 0x10);
    sb(P + 0x12ed, keep);
    sh(P + 0x12e0, 0xffff);
    sb(P + 0x20a9, 0);
    sh(P + 0x308, 0);
    if (kind == -1) return;
    if (kind == 2 && (lh(P + 0x30c) == 0 || lwc1(P + 0x2dc) < u2f(0x3e99999au))) {
        sb(P + 0x12e7, 1);
        if (lbu(P + 0x20a4) == 0) {
            const uint32_t m = lwu(P + 0x10e0);
            if (m != 0 && lh(m + 0xa6) == 0xad) sh(P + 0x308, 1);
        }
    }
    if (kind == 0xe) {
        sb(P + 0x12e4, 1);
        swc1(P + 0x22a4, u2f(0x3e4ccccdu));
        swc1(P + 0x2f0, lwc1(P + 0x2d8) + u2f(0x3e4ccccdu));
    }
    if (kind == 0) {
        const float d = lwc1(P + 0x2f0) - lwc1(P + 0x2d8);
        swc1(P + 0x22a4, d);
        if (d < u2f(0x3f59999au) && 0.25f < d) sb(P + 0x20a9, 1);
        sb(P + 0x12e4, 1);
    }
    if (lw(0x0015ed84) == 0xd && lw(P + 0x2084) != 0x7b) {
        const float r = lwc1(P + 0x234) + u2f(0x3cf5c28fu);
        if (sphere_collide_v(V(P + 0xd0), r, 2, 0) != 0 && sub_001f35d8() == 0xb) {
            hero_set_state(0x7b, 1);
            return;
        }
    }
    if (kind == 0xb) sb(P + 0x12eb, 1);
    if (kind == 4) sb(P + 0x12e3, 1);
    if (kind == 0xd) sb(P + 0x12ec, 1);
    if (kind == 8) sb(P + 0x12ea, 1);
    if (kind == 9) sb(P + 0x12ee, 1);
    if (kind == 0xc) sb(P + 0x12ea, 1);
    if (lbu(P + 0x12e3) != 0 && lw(P + 0x300) != 0) {  // sliding
        const uint32_t sub = lwu(P + 0x208c);
        if (sub != 0x10 && sub != 0x14 && sub != 7) {
            hero_set_state(0x31, 1);
            return;
        }
    }
    if (lbu(P + 0x12ec) != 0 && lw(P + 0x2084) != 0x7f && sinking_into(lwc1(P + 0x2f0)) && lwc1(P + 0x108) < 0.0f) {
        hero_mode3_exit();
        hero_set_state(0x7f, 1);
    }
}

}  // namespace rac1
