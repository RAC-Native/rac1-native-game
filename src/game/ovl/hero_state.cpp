// Ported overlay code: the hero state machine's state entry (hero_set_state 0x2223f8) and the
// small functions in its call cycle.
#include <game/enhance/advanced.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;

// the default animation for idle-like states: the "stand" set of the current item
// (sub_00205000), blended over 18 frames when that set is 0x54, else with the set's default
// orig: 0x002223f8 (shared tail)
void idle_anim() {
    if (sub_00205000(0) == 0x54) {
        const uint32_t set = sub_00205000(0);
        hero_anim_set(set, 0, static_cast<float>(frame_scale_i(0x12)));
        return;
    }
    hero_anim_set(sub_00205000(0), 0, -1.0f);
}

void copy_vel_150() { V(P + 0x150) = V(P + 0x110); }
void copy_vel_150_from_100() { V(P + 0x150) = V(P + 0x100); }

// the stop animation (set 6, or 5 for the mid-length jump of item kind 4 with +0xaa8 in (3, 16)),
// kept-pose blend over 9 frames
// orig: 0x002223f8 (states 3 / 0x66)
void stop_anim(uint32_t kind) {
    const float t = lwc1(P + 0xaa8);
    uint32_t set = 6;
    if (sub_002050b8(kind) == 4 && 3.0f < t && t < 16.0f) set = 5;
    hero_anim_set(set, 4, static_cast<float>(frame_scale_i(9)));
}

// play `set` (mode 3) and start the hero moby's scripted rate from table 0x17bdf0 (row +0xaa0,
// step +0xaa4, advanced)
// orig: 0x002223f8 (shared tail 0x227188)
void scripted_anim(uint32_t set, float blend) {
    hero_anim_set(set, 3, blend);
    const uint32_t row = lwu(P + 0xaa0) * 100u, k = lwu(P + 0xaa4);
    swc1(lwu(P + 0x2080) + 0x54, lwc1(OV(0x0017bdf0) + (k << 2) + row));
    sw(P + 0xaa4, k + 1);
}

// re-enter a different state instead (the entry of this one is abandoned: returns 0)
// orig: 0x002223f8 (the `jal 0x2223f8; li v0, 0` exits)
int32_t redirect(uint32_t state) {
    hero_set_state(state, 1);
    return 0;
}
}  // namespace

// count one use in a game statistics record: +0 uses (saturating), +2 the highest play time
// (frame-scaled 0x15eea4 / 600) seen, +4 the level bit | 0x80000000
// orig: 0x002223f8 (the inlined statistics updates; also in 0x217970)
void hero_stat_use(uint32_t a) {
    const uint32_t c = lhu(a);
    if (!(0xfffeu < c)) sh(a, c + 1);
    if (static_cast<int32_t>(lhu(a + 2)) < frame_scale_i(lw(0x0015eea4)) / 600) {
        sh(a + 2, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 600));
    }
    sw(a + 4, lwu(a + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
}

// orig: 0x0020b930 — leave the vehicle / special mode 3 (PLAYER+0x20a4).
void hero_mode3_exit() {
    if (lbu(P + 0x20a4) == 3) hero_mode_exit();
}

// orig: 0x00229a98 — with an item in hand (PLAYER+0x20a8, +0x20aa, no +0x22c8): when it is still
// the held one (+0x2298) re-enter state 0 quietly with the item's idle animation (table
// 0x179ac0 +0x24; +0x2294 = it, +0xd14 = 1 with a layer), else drop the hold. 1 when entered.
int32_t hero_item_idle() {
    if (lbu(P + 0x20a8) == 0 || lbu(P + 0x20aa) == 0 || lh(P + 0x22c8) != 0) return 0;
    const int32_t item = sub_0020d498(0);
    if (item != lw(P + 0x2298)) {
        hero_hold_release();
        return 0;
    }
    const uint32_t set = lwu(OV(0x00179ac0) + static_cast<uint32_t>(item) * 0x4c + 0x24);
    if (hero_set_state(0, 0) == 0) return 0;
    hero_anim_set(set, 0, static_cast<float>(frame_scale_i(0xb)));
    if (lwu(P + 0xd08) != 0) sw(P + 0xd14, 1);
    sw(P + 0x2294, set);
    return 1;
}

// orig: 0x00227638 — enter the idle state of the movement mode PLAYER+0x20a4 (0: 0, 1: 0x43,
// 2: 0x5a, 3: 0x53).
void hero_enter_idle() {
    if (on_l1_family() || on_rilgar()) return hero_enter_idle_l1();
    const uint32_t m = lbu(P + 0x20a4);
    if (m == 0) {
        hero_set_state(0, 1);
    } else if (hero_has_clank() && m == 1) {
        hero_set_state(0x43, 1);
    } else if (hero_has_giant_clank() && m == 2) {
        hero_set_state(0x5a, 1);
    } else if (m == 3) {
        hero_set_state(0x53, 1);
    }
}

// Enter the special movement mode `mode` (PLAYER+0x20a4; 1 the ship, 2 the 0x5a..0x61 set, 3 the
// 0x53 set...) riding `moby`, in state `state`: the held moby released, the slots updated, the
// hero moby's children retired and its effects reset, `moby` becomes the hero moby (+0x2080) and
// the ridden one (+0xa84, flags 6), the gait / shadow / light refreshed, +0x22cc the entry state;
// mode 1 swaps the counter +0x22a8 with +0x22ac (saving it to +0x22b0), mode 2 sets +0x1630 to 200.
// orig: 0x00210a08
void hero_mode_enter(uint32_t mode, uint32_t state, uint32_t moby) {
    if (on_l1_family() || on_rilgar()) return hero_mode_enter_l1(mode, state, moby);
    hero_hold_release();
    sb(P + 0x20a6, 1);
    hero_slots_update(P);
    sw(P + 0x2080, lwu(P + 0xa88));
    retire_children(P + 0x1670);
    sw(lwu(P + 0x2080) + 0x98, 0xffffffffu);
    sub_00232fe8();
    sub_00206c08();
    sub_00205538();
    sw(P + 0x2080, moby);
    sb(P + 0x20a4, mode);
    sw(P + 0xa84, moby);
    hero_gait_update();
    swc1(P + 0x224, lwc1(P + 0x22c));
    swc1(P + 0x220, lwc1(P + 0x228));
    const uint32_t hm = lwu(P + 0x2080);
    sh(hm + 0x34, lhu(hm + 0x34) | 6u);
    swc1(P + 0xa94, 1.0f);
    swc1(P + 0xa90, 1.0f);
    sub_00232628();
    moby_update_light(lwu(P + 0x2080));
    sub_00207a58();
    sh(P + 0x22cc, state);
    if (mode == 1) {
        sh(P + 0x22b0, lhu(P + 0x22a8));
        sw(P + 0x22a8, lwu(P + 0x22ac));
    } else if (hero_has_giant_clank() && mode == 2) {
        sw(P + 0x1630, 200);
    }
    hero_set_state(state, 1);
}

// Leave the special movement mode PLAYER+0x20a4: mode 1 restores the counter +0x22a8 from
// +0x22b0 (saving it to +0x22ac) and retires the helper mobys +0x1620 / +0x1624, mode 2 drops its
// camera request +0x1634; effects are reset, the mode is cleared, the ridden moby +0xa84 loses
// flags 1/2, the extra moby +0xa8c is retired, the hero moby +0xa88 is restored at the hero's
// position (+0xa94 = 1) and the hero re-enters state 0 (not on levels 2 / 6 in state 0x64).
// orig: 0x00210b30
void hero_mode_exit() {
    if (on_l1_family() || on_rilgar()) return hero_mode_exit_l1();
    if (hero_has_clank() && lbu(P + 0x20a4) == 1) {
        sw(P + 0x22ac, lwu(P + 0x22a8));
        sw(P + 0x22a8, static_cast<uint32_t>(lh(P + 0x22b0)));
        if (lwu(P + 0x1620) != 0) {
            moby_retire(lwu(P + 0x1620));
            sw(P + 0x1620, 0);
        }
        if (lwu(P + 0x1624) != 0) {
            moby_retire(lwu(P + 0x1624));
            sw(P + 0x1624, 0);
        }
    }
    if (hero_has_giant_clank() && lbu(P + 0x20a4) == 2 && lh(P + 0x1634) != -1) {
        sub_002359f8(static_cast<uint32_t>(lh(P + 0x1634)), 0);
        sh(P + 0x1634, 0xffff);
    }
    sub_00205538();
    sb(P + 0x20a4, 0);
    const uint32_t ride = lwu(P + 0xa84);
    if (ride != 0) sh(ride + 0x34, lhu(ride + 0x34) & 0xfff9u);
    sw(P + 0xa84, 0);
    if (lwu(P + 0xa8c) != 0) {
        moby_retire(lwu(P + 0xa8c));
        sw(P + 0xa8c, 0);
    }
    sub_00206c08();
    const uint32_t hm = lwu(P + 0xa88);
    sw(P + 0x2080, hm);
    V(hm + 0x10) = V(P + 0x80);
    sw(hm + 0x98, 0);
    swc1(P + 0xa94, 1.0f);
    sub_00232628();
    if (lw(P + 0x2084) == 0x64) {
        const int32_t lvl = lw(OV(0x0015f5c4));
        if (lvl == 2 || lvl == 6) return;
    }
    hero_set_state(0, 1);
}

namespace {
// the arc parameters shared by the jump family: rise height +0x430 (= +0x488), peak +0x48c,
// rise time +0x420, the +0x434 / +0x440 / +0x444 / +0x448 shape terms and the +0x498 timer
// orig: 0x002223f8 (block 0x224590)
void jump_arc(float h, float peak, int32_t t420, float a434, float a440, float a444, float a448, int32_t t498) {
    swc1(P + 0x430, h);
    sw(P + 0x420, static_cast<uint32_t>(frame_scale_i(t420)));
    swc1(P + 0x434, a434);
    swc1(P + 0x440, a440);
    swc1(P + 0x444, a444);
    swc1(P + 0x448, a448);
    swc1(P + 0x48c, peak);
    swc1(P + 0x488, h);
    sh(P + 0x498, static_cast<uint32_t>(frame_scale_i(t498)));
}

// hero_anim_set with a frame-scaled blend
// orig: 0x002223f8 (the `jal 0x1fef20; cvt.s.w f12` / `jal 0x2323b8` tails)
void anim_fs(uint32_t set, uint32_t how, int32_t frames) {
    hero_anim_set(set, how, static_cast<float>(frame_scale_i(frames)));
}

// The jump family (states 7, 9..0x12, 0x1c, 0x29, 0x2a, 0x3c, 0x49, 0x4c, 0x5e, 0x69): the
// take-off parameters at +0x3d0..+0x4af, then the take-off animation. Same return convention as
// enter_state.
// orig: 0x002223f8 (block 0x224590)
int32_t enter_jump(uint32_t state, int32_t mode, uint32_t hm_old, uint32_t old_2088) {
    if (lh(P + 0x1f2) != 0) {
        sw(P + 0x2084, lwu(P + 0x2090));
        sw(P + 0x198, lwu(P + 0x2098));
        sw(P + 0x2088, old_2088);
        return 0;
    }
    sw(P + 0x208c, 4);
    sw(P + 0x2284, state - 0x29u < 2u ? 3u : 0x50u);
    V(P + 0xe0) = V(P + 0x110);
    if (lw(P + 0x2084) != 0x12 && lbu(P + 0x12e4) != 0 && lwc1(P + 0x88) < lwc1(P + 0x2f0) + 0.5f) {
        hero_land_fx(3, 0x10, 0);
    }
    if (hero_has_lava() && lw(P + 0x2084) == 0x69) hero_sound_start(10, 0);
    {
        const float fs70 = lwc1(0x0015ed70);
        swc1(P + 0x410, lwc1(P + 0x88));
        swc1(P + 0x4a0, fs70 * u2f(0x41ed999au));
        sw(P + 0x3e8, 0xffffffffu);
        swc1(P + 0x480, fs70 * 20.0f);
        swc1(P + 0x484, fs70 * 11.0f);
        sh(P + 0x4a8, lbu(P + 0x12e2));
        sh(P + 0x41e, 0);
        sw(P + 0x418, 0);
        sw(P + 0x428, 0);
        sw(P + 0x42c, 0);
        sh(P + 0x41c, 0);
        swc1(P + 0x490, 2.5f);
        sh(P + 0x4a4, 0);
        sh(P + 0x4ac, 0);
        sb(P + 0x4ae, 0);
        sb(P + 0x4af, 0);
        sh(P + 0x49a, static_cast<uint32_t>(frame_scale_i(0x50)));
        sh(P + 0x4aa, static_cast<uint32_t>(frame_scale_i(0x46)));
        const float fs6c = lwc1(0x0015ed6c);
        swc1(P + 0x494, 0.5f);
        swc1(P + 0x424, fs6c * u2f(0x40b66666u));
        swc1(P + 0x414, fs6c * u2f(0x41702845u));
        sh(P + 0x4a6, 0);
        sw(P + 0x3f8, 0);
        sw(P + 0x22b4, 0);
    }
    const float fs70 = lwc1(0x0015ed70);
    const float fs6c = lwc1(0x0015ed6c);
    const float k06 = u2f(0x3f19999au), k08 = u2f(0x3f4ccccdu), k085 = u2f(0x3f59999au);
    const uint32_t st = lwu(P + 0x2084);
    switch (st) {
    case 0x07:
        jump_arc(u2f(0x3fbc28f6u), u2f(0x4027ae14u), 5, k06, 18.0f, 30.0f, 12.0f, 15);
        sb(P + 0x4af, 1);
        sh(P + 0x4ac, 1);
        sb(P + 0x4ae, 1);
        swc1(P + 0x3f8, lwc1(P + 0x4a0) * u2f(0x3f95c28fu));
        break;
    case 0x12:
        if (!hero_has_water()) break;
        jump_arc(u2f(0x3dcccccdu), u2f(0x3e4ccccdu), 2, k06, 18.0f, 28.0f, 14.0f, 10);
        if (lwc1(P + 0x22a4) < 1.25f) {
            sw(P + 0x420, static_cast<uint32_t>(frame_scale_i(15)));
            swc1(P + 0x430, u2f(0x3ff9999au));
            swc1(P + 0x48c, u2f(0x3ffd70a4u));
            swc1(P + 0x488, u2f(0x3ff9999au));
        } else {
            sub_00233b20(P + 0xe0, P + 0xe0, 0.0f);
            sh(P + 0x41c, 1);
            sw(P + 0x3d0, static_cast<uint32_t>(frame_scale_i(2)));
            sw(P + 0x3d4, static_cast<uint32_t>(frame_scale_i(0x23)) + lwu(P + 0x3d0));
            swc1(P + 0x3d8, 0.0f);
            swc1(P + 0x3dc, 18.0f);
            sw(P + 0x3e0, static_cast<uint32_t>(frame_scale_i(0x28)));
            sw(P + 0x3e4, OV(0x0017bf60u));
        }
        sb(P + 0x20a7, 1);
        break;
    case 0x3c: {
        if (!hero_has_state_7c()) break;
        jump_arc(5.5f, u2f(0x40b1999au), 5, k06, 18.0f, 26.0f, 13.0f, 2);
        sh(P + 0x49a, static_cast<uint32_t>(frame_scale_i(0x78)));
        hero_counter_use(1);
        const int32_t lvl = lw(0x0015ed84);
        if (lvl == 10) return redirect(0x7c);
        if (lvl != 6) {
            bool count = true;
            if (lw(P + 0x2090) != 0x3c && lw(P + 0x209c) != 0x3c && lw(P + 0x22a8) != 0) {
                sw(P + 0x1660, 0);
                count = false;
            }
            if (count) {
                const int32_t n = lw(P + 0x1660) + 1;
                sw(P + 0x1660, static_cast<uint32_t>(n));
                if (n >= 2 || lw(P + 0x22a8) == 0) return redirect(0x7c);
            }
        }
        sh(P + 0x4aa, static_cast<uint32_t>(frame_scale_i(0x96)));
        swc1(P + 0x4a0, fs70 * 15.0f);
        sh(P + 0x49c, static_cast<uint32_t>(frame_scale_i(5)));
        sh(P + 0x49e, static_cast<uint32_t>(frame_scale_i(4)));
        break;
    }
    case 0x69: {
        if (!hero_has_lava()) break;
        sw(P + 0x208c, 0x19);
        const float y = lwc1(P + 0x88), top = lwc1(P + 0x2f4);
        const float d = y < top ? top - y : lwc1(P + 0x3f8);
        const float h = d + u2f(0x3fd9999au);
        jump_arc(h, d + 1.75f, 5, k06, 18.0f, 30.0f, 12.0f, 0x13);
        break;
    }
    case 0x29:
        if (!hero_has_grind()) break;
        jump_arc(u2f(0x4019999au), u2f(0x401ccccdu), 5, k08, 18.0f, 28.0f, 14.0f, 0x11);
        sw(P + 0x208c, 0xf);
        sw(P + 0x5c4, 0);
        swc1(P + 0x4a0, fs70 * 27.0f);
        if (lbu(P + 0x12e8) != 0) {
            const float k = u2f(0x40966666u);
            swc1(P + 0x488, lwc1(P + 0x488) + k);
            swc1(P + 0x430, lwc1(P + 0x430) + k);
            swc1(P + 0x48c, lwc1(P + 0x48c) + k);
            swc1(P + 0x4a0, fs70 * 14.0f);
            const Vec4 spot{331.0f, 104.0f, 145.0f, 0.0f};
            if (vec3_distance(V(P + 0x80), spot) < 10.0f) {
                const float k2 = u2f(0x3f333333u);
                swc1(P + 0x430, lwc1(P + 0x430) + k2);
                swc1(P + 0x488, lwc1(P + 0x488) + k2);
                swc1(P + 0x48c, lwc1(P + 0x48c) + k2);
            }
        }
        break;
    case 0x2a:
        if (!hero_has_grind()) break;
        jump_arc(2.5f, u2f(0x40233333u), 2, k08, 18.0f, 31.0f, 14.0f, 10);
        sw(P + 0x208c, 0xf);
        sw(P + 0x5c4, 0);
        swc1(P + 0x4a0, fs70 * 24.0f);
        break;
    case 0x09:
        jump_arc(1.0f, u2f(0x402ccccdu), 5, k06, 18.0f, 29.0f, 11.0f, 15);
        sb(P + 0x4af, 1);
        sh(P + 0x4ac, 1);
        sb(P + 0x4ae, 1);
        swc1(P + 0x424, fs6c * u2f(0x40b66666u));
        break;
    case 0x5e:
        if (hero_has_giant_clank()) {
            jump_arc(u2f(0x40866666u), u2f(0x4089999au), 0x13, k06, 17.0f, 22.0f, 40.0f, 15);
            swc1(P + 0x4a0, fs70 * 30.0f);
            swc1(P + 0x414, fs6c * u2f(0x408ba058u));
            swc1(P + 0x424, fs6c * 4.0f);
            swc1(P + 0x3f8, fs70 * 50.0f);
            swc1(P + 0x494, 2.0f);
        }
        break;
    case 0x49:
        if (!hero_has_clank()) break;
        jump_arc(u2f(0x3fa66666u), u2f(0x3faccccdu), 5, k06, 18.0f, 27.0f, 27.0f, 0xe);
        swc1(P + 0x4a0, fs70 * 25.0f);
        swc1(P + 0x424, fs6c + fs6c);
        swc1(P + 0x484, fs70 * 50.0f);
        break;
    case 0x0a:
        jump_arc(u2f(0x3ff33333u), u2f(0x3ff9999au), 5, k06, 18.0f, 29.0f, 10.0f, 0xe);
        sb(P + 0x4ae, 1);
        swc1(P + 0x4a0, fs70 * 11.0f);
        sh(P + 0x49a, static_cast<uint32_t>(frame_scale_i(0x78)));
        sh(P + 0x4aa, static_cast<uint32_t>(frame_scale_i(0x5a)));
        sh(P + 0x4a6, static_cast<uint32_t>(frame_scale_i(0x46)));
        break;
    case 0x10: {
        jump_arc(1.5f, u2f(0x3fc66666u), 5, k08, 18.0f, 30.0f, 11.0f, 1);
        swc1(P + 0x488, u2f(0x3ecccccdu));
        swc1(P + 0x48c, u2f(0x3f666666u));
        swc1(P + 0x430, u2f(0x3ecccccdu));
        const uint32_t list = P + 0x1670;
        sub_00262500(lwu(P + 0x2080), list);
        sub_00262528(list, 0x28, 2);
        sub_00262528(list, 0x14, 4);
        sub_00262528(list, 0xa, 6);
        sb(P + 0x4ae, 1);
        swc1(P + 0x3f4, fs6c * 11.5f);
        if (u2f(0x3fb33333u) < lwc1(P + 0x2dc)) swc1(P + 0x3f4, fs6c * u2f(0x40b66666u));
        swc1(P + 0x4a0, fs70 * 8.5f);
        swc1(P + 0x480, fs70 * 44.0f);
        swc1(P + 0x484, fs70 * 50.0f);
        break;
    }
    case 0x1c:
        jump_arc(2.0f, u2f(0x40033333u), 10, k085, 20.0f, 29.0f, 19.0f, 1);
        sb(P + 0x4ae, 1);
        swc1(P + 0x494, 1.0f);
        swc1(P + 0x4a0, fs70 * 25.0f);
        sw(P + 0x45c, 0);
        sb(P + 0x20a7, 1);
        break;
    case 0x4c:
        if (!hero_has_clank()) break;
        jump_arc(u2f(0x3fc66666u), u2f(0x3fcb851fu), 0xd, k085, 10.0f, 18.0f, 18.0f, 1);
        sw(P + 0x45c, 0);
        swc1(P + 0x4a0, fs70 * 28.0f);
        break;
    case 0x11:
        jump_arc(u2f(0x40533333u), u2f(0x40566666u), 9, k06, 12.0f, 20.0f, 10.0f, 0xc);
        sh(P + 0x4aa, static_cast<uint32_t>(frame_scale_i(0x50)));
        swc1(P + 0x4a0, fs70 * 29.0f);
        V(P + 0x470) = V(P + 0x460);
        sh(P + 0x4ac, 1);
        sw(P + 0x22b4, 1);
        sb(P + 0x20a7, 1);
        break;
    case 0x0d: {
        jump_arc(u2f(0x3dcccccdu), u2f(0x3e4ccccdu), 9, k06, 26.0f, 38.0f, 18.0f, 0xe);
        swc1(P + 0x490, 4.5f);
        sh(P + 0x49a, static_cast<uint32_t>(frame_scale_i(0x78)));
        swc1(P + 0x424, fs6c * u2f(0x3ff33333u));
        const uint32_t list = P + 0x1670;
        sub_00262500(lwu(P + 0x2080), list);
        sub_00262528(list, 0x28, 3);
        sub_00262528(list, 0x14, 5);
        sh(P + 0x41c, 1);
        sw(P + 0x3d0, static_cast<uint32_t>(frame_scale_i(9)));
        sw(P + 0x3d4, static_cast<uint32_t>(frame_scale_i(0x23)) + lwu(P + 0x3d0));
        swc1(P + 0x3dc, 26.0f);
        sw(P + 0x3d8, 0);
        sw(P + 0x3e0, static_cast<uint32_t>(frame_scale_i(0x2d)));
        sb(P + 0x4ae, 1);
        sw(P + 0x3e4, OV(0x0017bf20u));
        break;
    }
    case 0x0f:
        jump_arc(u2f(0x3ff33333u), u2f(0x3ff9999au), 9, k06, 50.0f, 64.0f, 28.0f, 5);
        swc1(P + 0x490, 4.5f);
        sh(P + 0x49a, static_cast<uint32_t>(frame_scale_i(0x82)));
        swc1(P + 0x424, fs6c * u2f(0x40533333u));
        sh(P + 0x4a6, static_cast<uint32_t>(frame_scale_i(0x55)));
        sh(P + 0x1b0, static_cast<uint32_t>(frame_scale_i(0x46)));
        sh(P + 0x41c, 1);
        sw(P + 0x3d0, static_cast<uint32_t>(frame_scale_i(lw(0x0015ed80) != 0 ? 0x22 : 0x23)));
        sw(P + 0x3d4, static_cast<uint32_t>(frame_scale_i(0x24)) + lwu(P + 0x3d0));
        swc1(P + 0x3d8, 29.0f);
        swc1(P + 0x3dc, 50.0f);
        sw(P + 0x3e0, static_cast<uint32_t>(frame_scale_i(0x56)));
        sb(P + 0x4ae, 1);
        sw(P + 0x3e4, OV(0x00166c00u - 0x7518));
        break;
    case 0x0e: {
        const float s = lwc1(P + 0x2dc);
        float h = u2f(0x3f333333u);
        if (s < 2.0f) h = (2.5f - s) * 0.5f + h;
        jump_arc(h, h + u2f(0x3d4ccccdu), 0, k06, 32.0f, 43.0f, 19.0f, 0xe);
        sw(P + 0x420, 0xffffffffu);
        swc1(P + 0x494, 1.0f);
        swc1(P + 0x424, fs6c * 3.5f);
        sh(P + 0x4a6, static_cast<uint32_t>(frame_scale_i(0x1e)));
        break;
    }
    case 0x0b:
        jump_arc(u2f(0x404ccccdu), 3.25f, 5, k06, 18.0f, 31.0f, 24.0f, 0xe);
        swc1(P + 0x490, 4.5f);
        sh(P + 0x4aa, static_cast<uint32_t>(frame_scale_i(0x32)));
        swc1(P + 0x494, 1.0f);
        swc1(P + 0x43c, sub_0020d658(frame_scale_i(10)));
        sw(P + 0x450, static_cast<uint32_t>(hero_stick_quadrant()));
        swc1(P + 0x438, lwc1(P + 0x180));
        if (fs6c * 1.5f < lwc1(P + 0x164)) {
            swc1(P + 0x454, lwc1(P + 0x164));
        } else {
            sw(P + 0x454, 0);
        }
        sb(P + 0x4ae, 1);
        sw(P + 0x458, 0);
        sh(P + 0x4ac, 1);
        break;
    default:
        break;
    }
    advanced_rac3_jump();  // enhancement: R&C 3's take-off, heights and air speed
    if (lwc1(P + 0x490) == 0.0f) swc1(P + 0x490, lwc1(P + 0x430) * 1.5f);
    if (mode == 0) return 1;
    switch (lw(P + 0x2084)) {
    case 0x07: anim_fs(7, 0, 5); break;
    case 0x12: if (hero_has_water()) hero_anim_set(0x72, 1, -1.0f); break;
    case 0x3c: if (hero_has_state_7c()) anim_fs(0x43, 0, 4); break;
    case 0x69: if (hero_has_lava()) anim_fs(7, 0, 5); break;
    case 0x29: if (hero_has_grind()) anim_fs(0x50, 4, 5); break;
    case 0x2a: {
        if (!hero_has_grind()) break;
        const float a = atan2_f(lwc1(P + 0x500) - lwc1(P + 0x540), lwc1(P + 0x504) - lwc1(P + 0x544));
        const uint32_t set = u2f(0x3fc90fdbu) < angle_dist(lwc1(P + 0x98), a) ? 0x1f : 0x1e;
        anim_fs(set, 0, 5);
        break;
    }
    case 0x09: {
        const int32_t i = float_to_int(lwc1(P + 0xaa8));
        uint32_t set = 8;
        if (sub_002050b8(lbu(hm_old + 0x52)) == 4) set = lbu(OV(0x0017bdd8) + static_cast<uint32_t>(i));
        anim_fs(set, 0, 5);
        break;
    }
    case 0x5e: if (hero_has_giant_clank()) anim_fs(4, 0, 8); break;
    case 0x49: if (hero_has_clank()) anim_fs(7, 5, 5); break;
    case 0x0a: anim_fs(0x12, 0, 5); break;
    case 0x10:
        if (lw(P + 0x2094) == 4 || (lw(P + 0x2090) == 4 && lw(P + 0x198) < frame_scale_i(7))) {
            anim_fs(0x26, 0, 8);
            sw(P + 0x420, lwu(P + 0x420) + static_cast<uint32_t>(frame_scale_i(3)));
        } else {
            anim_fs(0x26, 0, 5);
        }
        break;
    case 0x1c: anim_fs(0x22, 0, 4); break;
    case 0x4c: if (hero_has_clank()) anim_fs(9, 0, 4); break;
    case 0x11: anim_fs(0x23, 2, 7); break;
    case 0x0d: anim_fs(0x11, 0, 5); break;
    case 0x0e:
        anim_fs(0x16, 3, 7);
        swc1(P + 0xa90, lwc1(0x0015ed60) * u2f(0x3f4ccccdu));
        break;
    case 0x0f: anim_fs(0x15, 0, 5); break;
    case 0x0b: {
        if (lw(P + 0x450) == 2) sw(P + 0x450, 0);
        uint32_t set = lwu(P + 0x450) + 0x1c;
        if (lbu(0x0015edb5) != 0) {
            if (set == 0x1c) set = 0x1d;
            else if (set == 0x1d) set = 0x1c;
        }
        if (lw(P + 0x2094) == 4) {
            anim_fs(set, 1, 4);
            sw(P + 0x420, static_cast<uint32_t>(frame_scale_i(1)));
        } else {
            anim_fs(set, 0, 5);
        }
        break;
    }
    default:
        break;
    }
    return 1;
}


// The wrench attack family (states 0x13, 0x14, 0x15, 0x20, 0x23, 0x51). Same return convention
// as enter_state. The combo table 0x17bc28 (0x2c-byte steps: +0 end flag, +4 next step,
// +8 last frame of the window) picks the next swing of a chained 0x13 / 0x51.
// orig: 0x002223f8 (block 0x225bdc)
int32_t enter_attack(int32_t mode) {
    swc1(P + 0xa6c, 1.0f);
    sw(P + 0x208c, 6);
    sw(P + 0x2284, 0);
    sw(P + 0xa58, 0);
    sw(P + 0xa54, 0);
    sw(P + 0xa64, 0);
    constexpr OvConst<0x0017bc28u> COMBO{};
    switch (lw(P + 0x2084)) {
    case 0x14: {
        hero_stat_use(0x00141848);
        sw(P + 0x2284, 0x50);
        sw(P + 0xa60, 4);
        sw(P + 0x20f0, 0);
        hero_lock_heading(8.0f, u2f(0x3f860a92u), -1.0f);
        sw(P + 0xa70, 0);
        sh(P + 0x10a8, 10);
        if (mode == 0) return 1;
        int32_t n = frame_scale_i(0xb);
        if (lw(P + 0x2090) == 0xb && lw(P + 0x2098) < frame_scale_i(0x2a)) n = frame_scale_i(0x11);
        hero_anim_set(0x2b, 4, static_cast<float>(n));
        const uint32_t w = sub_0020d460(0);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, 10, 4, n + 2);
        return 1;
    }
    case 0x15: {
        hero_stat_use(0x00141848 + 8);
        hero_stat_use(0x00141680 + 0x128);
        sw(P + 0xa60, 3);
        sw(P + 0x20f0, 0);
        hero_lock_heading(14.0f, u2f(0x3f060a92u), -1.0f);
        sw(P + 0xa7c, lw(P + 0xa7c) == 0 ? 1u : 0u);
        if (mode == 0) return 1;
        int32_t n = frame_scale_i(10);
        if (lbu(lwu(P + 0x2080) + 0x53) == 0x1a) n = frame_scale_i(3);
        if (lw(P + 0x2090) == 1) n = frame_scale_i(0x11);  // (s6, reused as the bit 1 above)
        hero_anim_set(0x1a, 0, static_cast<float>(n));
        sub_002325e0(6, 0x15);
        return 1;
    }
    case 0x20: {
        if (lwu(P + 0x1090) == 0) return 1;
        if (mode != 0) {
            hero_anim_set(0x67, 4, -1.0f);
            moby_anim_set(lwu(P + 0x1090), 3, 4, -1);
        }
        hero_lock_heading(11.0f, u2f(0x3f5f66f3u), -1.0f);
        return 1;
    }
    case 0x13: {
        hero_stat_use(0x00141680 + 0x40);
        const uint32_t e = lwu(P + 0xa60) * 0x2c + COMBO;
        int32_t step = 0;
        sw(P + 0x20f0, 0);
        if (lw(P + 0x2094) == 6 && lw(e) == 0 && lwc1(P + 0xaa8) < static_cast<float>(lw(e + 8))) step = (lw(e + 4) + 1) % 3;
        sw(P + 0xa60, static_cast<uint32_t>(step));
        hero_lock_heading(5.0f, u2f(0x3f490fdbu), u2f(0x3f490fdbu));
        if (mode == 0) return 1;
        const int32_t how = step == 0 ? 0 : step == 1 ? 2 : 4;
        int32_t n = frame_scale_i(step == 0 ? 5 : 7);
        if (lw(P + 0xa58) != 0 && step < 2) n += float_to_int(angle_dist(lwc1(P + 0xa5c), lwc1(P + 0x98)) * 1.5f);
        hero_anim_set(static_cast<uint32_t>(step + 0x17), static_cast<uint32_t>(how / 2), static_cast<float>(n));
        const uint32_t w = sub_0020d460(0);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, static_cast<uint32_t>(step + 3), how / 2, n);
        return 1;
    }
    case 0x51: {
        if (!hero_has_clank()) return 1;
        const uint32_t e = lwu(P + 0xa60) * 0x2c + COMBO;
        int32_t step = 0;
        const bool chain = lw(P + 0x2094) == 6 || (lw(P + 0x20a0) == 6 && lbu(lwu(P + 0x2080) + 0x53) - 0xfu < 2u);
        if (chain && lw(e) == 0 && lwc1(P + 0xaa8) < static_cast<float>(lw(e + 8))) step = (lw(e + 4) + 1) % 2;
        sw(P + 0xa60, static_cast<uint32_t>(step + 7));
        hero_lock_heading(5.0f, u2f(0x3f490fdbu), u2f(0x3f490fdbu));
        if (mode == 0) return 1;
        hero_anim_set(static_cast<uint32_t>(step + 0xf), 1, static_cast<float>(frame_scale_i(5)));
        return 1;
    }
    case 0x23: {
        if (mode != 0) hero_anim_set(0x2c, 7, static_cast<float>(frame_scale_i(9)));
        if (lw(P + 0x10b8) != 10) return 1;
        const uint32_t t = lwu(P + 0xa50);
        if (t == 0) return 1;
        sw(P + 0xa58, 1);
        swc1(P + 0xa5c, atan2_f(lwc1(t + 0x10) - lwc1(P + 0x80), lwc1(t + 0x14) - lwc1(P + 0x84)));
        return 1;
    }
    default:
        return 1;
    }
}

// The per-state entry actions (the 131-way switch of 0x2223f8). Returns 0 when the entry was
// abandoned for another state, 2 to return 1 at once, 1 to run the common tail.
// orig: 0x002223f8 (jump table 0x1e8030)
int32_t enter_state(uint32_t state, int32_t mode, uint32_t hm_old, uint32_t old_2088) {
    if (!hero_has_giant_clank() && state >= 0x5au && state <= 0x62u && state != 0x5eu) return 1;
    if (!hero_has_clank() && ((state >= 0x43u && state <= 0x52u && state != 0x49u && state != 0x4cu && state != 0x51u) || state == 0x7du)) return 1;
    switch (state) {
    case 0x00: {
        sw(P + 0x208c, 0);
        sw(P + 0x2284, 0);
        const int32_t lo = frame_scale_i(0x32);
        sw(P + 0x1010, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x64))));
        sub_0020a500();
        if (lbu(P + 0x20a4) != 0) {
            hero_enter_idle();
            return 2;
        }
        copy_vel_150();
        if (hero_has_skate() && lbu(P + 0x12e2) != 0 && lwc1(0x0015ed6c) * 0.5f < vec2_length(V(P + 0x150))) return redirect(0x2f);
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x79);
        sw(P + 0xff8, 0x68);
        if (mode != 0) idle_anim();
        return 1;
    }
    case 0x67:
        sw(P + 0x208c, 0);
        sw(P + 0x2284, 0);
        vec_clear(V(P + 0x150));
        sw(P + 0xff8, 0x68);
        if (mode != 0) idle_anim();
        return 1;
    case 0x41: {
        const int32_t n = frame_scale_i(9);
        hero_anim_set(sub_00205000(0), 0, static_cast<float>(n));
        if (lwu(P + 0x1614) != 0) moby_anim_set(lwu(P + 0x1614), 0, 0, n);
        return 1;
    }
    case 0x3d:
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        copy_vel_150();
        if (mode != 0) hero_anim_set(0x45, 0, static_cast<float>(frame_scale_i(0xc)));
        return 1;
    case 0x80:
        if (!hero_has_sink_7f()) return 1;
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        copy_vel_150();
        vec3_clamp_length(P + 0x150, lwc1(0x0015ed6c) * 3.5f);
        if (mode != 0) hero_anim_set(0x7c, 0, static_cast<float>(frame_scale_i(0xc)));
        return 1;
    case 0x7f: {
        if (!hero_has_sink_7f()) return 1;
        const float vz = lwc1(P + 0x108) * u2f(0x3e4ccccdu);
        const float lim = lwc1(0x0015ed6c) * -7.0f;
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        swc1(P + 0x694, vz);
        swc1(P + 0x69c, lwc1(P + 0x2f0) - u2f(0x3ebd70a4u));
        if (vz < lim) swc1(P + 0x694, lim);
        sb(P + 0x20a7, 1);
        sw(P + 0x6a0, 0);
        swc1(P + 0x690, lwc1(P + 0x88) - lwc1(P + 0x69c));
        if (mode != 0) hero_anim_set(0x74, 0, static_cast<float>(frame_scale_i(0xb)));
        return 1;
    }
    case 0x33: {
        if (!hero_has_water()) return 1;
        sw(P + 0x208c, 0x11);
        sw(P + 0x2284, 0xb);
        sb(P + 0x20a7, 1);
        sw(P + 0x8fc, 0);
        if (lw(P + 0x2094) == 0x12) {
            hero_sound_start(3, 0);
            sw(P + 0x2088, 1);
        } else {
            sw(P + 0x8f4, static_cast<uint32_t>(frame_scale_i(0x2d)));
        }
        sw(P + 0xff8, 0x68);
        sw(P + 0x8e0, 0);
        const int32_t t = frame_scale_i(0xc);
        if (mode != 0) hero_anim_set(0x3b, 0, static_cast<float>(t));
        return 1;
    }
    case 0x35: {
        if (!hero_has_water()) return 1;
        sw(P + 0x2284, 0xb);
        sw(P + 0x208c, 0x11);
        sb(P + 0x20a7, 1);
        sw(P + 0x8f4, static_cast<uint32_t>(frame_scale_i(0x3c)));
        if (lw(P + 0x2094) == 0x12) {
            hero_sound_start(3, 0);
            sw(P + 0x2088, 1);
        }
        sw(P + 0x8e0, 0);
        sw(P + 0xff8, 0x68);
        int32_t t = frame_scale_i(0xb);
        if (lw(P + 0x2090) == 0x33) t = frame_scale_i(0xf);
        if (mode != 0) hero_anim_set(0x3d, 0, static_cast<float>(t));
        return 1;
    }
    case 0x34: {
        if (!hero_has_water()) return 1;
        sw(P + 0x208c, 0x11);
        sw(P + 0x2284, 0xb);
        sb(P + 0x20a7, 1);
        copy_vel_150();
        sw(P + 0xff8, 0x68);
        const int32_t t = frame_scale_i(0x23);
        const int32_t b = lw(P + 0x2090) != 0x35 ? t : -2;
        if (mode != 0) hero_anim_set(0x3c, 0, static_cast<float>(b));
        return 1;
    }
    case 0x37: {
        if (!hero_has_water()) return 1;
        const int32_t old = lw(P + 0x2094);
        sw(P + 0x208c, 0x12);
        sb(P + 0x20a7, 1);
        sw(P + 0x2284, 0);
        sh(P + 0x90a, 0);
        if (old == 0x11) {
            hero_land_fx(3, 10, 0);
            if (lwu(OV(0x001612d0)) != 0) region_height_splash(lwu(OV(0x001612d0)), lw(OV(0x001612d8)), 0, lwc1(P + 0x80), lwc1(P + 0x84), u2f(0x3ecccccdu), u2f(0x3e99999au));
        } else if (lwc1(P + 0x108) < lwc1(0x0015ed6c) * -0.5f) {
            const int32_t n = float_to_int(abs_f(lwc1(P + 0x108)) * 300.0f);
            hero_land_fx(3, n < 0x29 ? n : 0x28, 1);
            if (lwu(OV(0x001612d0)) != 0) region_height_splash(lwu(OV(0x001612d0)), lw(OV(0x001612d8)), 0, lwc1(P + 0x80), lwc1(P + 0x84), 0.5f, u2f(0xbecccccdu));
            sh(P + 0x90a, static_cast<uint32_t>(frame_scale_i(0x4b)));
        }
        sw(P + 0x698, 0);
        // the landing rumble by the charge PLAYER+0x22a0 (two copies with different lengths)
        for (int32_t pass = 0; pass < 2; ++pass) {
            if (lw(P + 0x2094) != 0x11) break;
            const float c = static_cast<float>(lw(P + 0x22a0));
            if (c < 2500.0f) {
                sub_00216ec0(8, static_cast<int16_t>(frame_scale_i(0x1e)), 0);
            } else if (c < 7500.0f) {
                sub_00216ec0(7, static_cast<int16_t>(frame_scale_i(pass == 0 ? 0x1b : 0x28)), 0);
            }
        }
        const float vz = lwc1(P + 0x108) * u2f(0x3ebd70a4u);
        const float lim = lwc1(0x0015ed6c) * -7.0f;
        swc1(P + 0x694, vz);
        if (vz < lim) swc1(P + 0x694, lim);
        const float g = lwc1(P + 0x2f0);
        swc1(P + 0x69c, g);
        sw(P + 0x6a0, 0);
        swc1(P + 0x690, (lwc1(P + 0x88) - g) - u2f(0xbdf5c28fu));
        copy_vel_150();
        sw(P + 0x158, 0);
        vec3_clamp_length(P + 0x150, lwc1(0x0015ed6c) * 5.5f);
        sw(P + 0xff8, 0x68);
        if (mode != 0) hero_anim_set(0x3a, 0, static_cast<float>(frame_scale_i(0x16)));
        return 1;
    }
    case 0x75:
    case 0x76: {
        if (!hero_has_water()) return 1;
        sw(P + 0x208c, 7);
        sw(P + 0x2284, 0);
        sub_00213de8(0.5f);
        sb(P + 0x20a7, 1);
        V(P + 0xe0) = V(P + 0x100);
        const uint32_t r = sub_0025a420(lwu(P + 0x2080), 1, 0);
        hero_counter_use(r != 0 ? float_to_int(lwc1(r + 0x2c)) : 1);
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x4d)));
        if (mode == 0) return 1;
        scripted_anim(lw(P + 0x2084) == 0x75 ? 0x70 : 0x6f, -3.0f);
        return 1;
    }
    case 0x36:
        if (!hero_has_water()) return 1;
        sb(P + 0x20a7, 1);
        sw(P + 0x208c, 0x12);
        sw(P + 0x2284, 0);
        sw(P + 0x8fc, 0);
        sw(P + 0x698, static_cast<uint32_t>(frame_scale_i(0x2d)));
        sw(P + 0xff8, 0x68);
        if (mode != 0) hero_anim_set(0x65, 0, static_cast<float>(frame_scale_i(0x11)));
        return 1;
    case 0x6a: {
        if (!hero_has_water()) return 1;
        constexpr uint32_t S = 0x00141848;  // the game statistics block
        const int32_t charge = lh(P + 0x90e);
        sw(P + 0x208c, 0x14);
        sb(P + 0x20a7, 1);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        auto bump = [](uint32_t a) {
            const uint32_t c = lhu(a);
            if (!(0xfffeu < c)) sh(a, c + 1);
        };
        auto at_least = [](uint32_t a) {
            const int32_t q = frame_scale_i(lw(0x0015eea4)) / 600;
            if (static_cast<int32_t>(lhu(a)) < q) sh(a, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 600));
        };
        if (frame_scale_i(0x32) < charge) bump(S + 0x60);
        at_least(S + 0x62);
        sw(S + 0x64, lwu(S + 0x64) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        if (!(lw(P + 0x910) < 3)) bump(S + 0xe8);
        at_least(S + 0xea);
        sw(S + 0xec, lwu(S + 0xec) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        copy_vel_150();
        if (mode != 0) hero_anim_set(0x5f, 0, static_cast<float>(frame_scale_i(0xa)));
        return 1;
    }
    case 0x82:
        if (!hero_has_water()) return 1;
        sb(P + 0x20a7, 1);
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        copy_vel_150();
        if (mode != 0) hero_anim_set(0x80, 0, static_cast<float>(frame_scale_i(0xa)));
        return 1;
    case 0x38:
    case 0x39:
    case 0x3a:
        if (!hero_has_hydropack()) return 1;
        sw(P + 0x208c, 0x13);
        sw(P + 0x2284, 0);
        if (mode != 0) hero_anim_set(0x46 + (state - 0x38), 0, static_cast<float>(frame_scale_i(8)));
        return 1;
    case 0x01: {
        sw(P + 0x2284, 4);
        sw(P + 0x208c, 0);
        sw(OV(0x0015f688), static_cast<uint32_t>(frame_scale_i(0x3c)));
        if (lbu(P + 0x20aa) != 0) return 1;
        const uint32_t set = sub_00205000(1);
        if (lbu(lwu(P + 0x2080) + 0x53) == set) return 1;
        if (mode == 0) return 1;
        hero_anim_set(set, 0, static_cast<float>(frame_scale_i(0x12)));
        return 1;
    }
    case 0x17:
    case 0x30: {
        sw(P + 0x208c, 8);
        sw(P + 0x2284, 0);
        copy_vel_150_from_100();
        const uint32_t set = sub_00205000(0);
        hero_anim_set(set, 0, static_cast<float>(frame_scale_i(state == 0x17 ? 0xa : 5)));
        sw(P + 0x1d0, static_cast<uint32_t>(frame_scale_i(7)));
        return 1;
    }
    case 0x2e:
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, vec2_length(V(P + 0x100)));
        sh(P + 0x3bc, 0);
        sw(P + 0x2088, 0);
        sh(P + 0x3b8, 0);
        hero_anim_set(0x2e, 0, static_cast<float>(frame_scale_i(5)));
        return 1;
    case 0x2f:
        if (!hero_has_skate()) return 1;
        [[fallthrough]];
    case 0x02:
    case 0x3f:
    case 0x73: {
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        if (lw(P + 0x2094) != 4 && lw(P + 0x20a0) != 4) swc1(P + 0x194, vec2_length(V(P + 0x110)));
        const float vmax = lwc1(0x0015ed6c) * 7.0f;
        if (vmax < lwc1(P + 0x194)) swc1(P + 0x194, vmax);
        sh(P + 0x3be, 0);
        hero_stick_move(0, 1.0f);
        if (u2f(0x3fc90fdbu) < angle_dist(lwc1(P + 0x98), lwc1(P + 0x180))) {
            if (lw(P + 0x2094) == 7 || lw(P + 0x2090) == 3 || lw(P + 0x209c) == 3) sw(P + 0x194, 0);
        }
        sw(P + 0xff8, 0x68);
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x79);
        if (hero_has_skate() && (lbu(P + 0x12e2) != 0 || state == 0x2f)) {  // sliding run
            const float k = u2f(0x40f66666u);
            const float lim = lwc1(0x0015ed6c) * k;
            if (lim < lwc1(P + 0x194)) swc1(P + 0x194, lim);
            sw(P + 0xe8, 0);
            vec3_clamp_length(P + 0xe0, lwc1(0x0015ed6c) * k);
            sw(P + 0x2084, 0x2f);
            sw(P + 0x3b4, 0);
            hero_anim_set(0x37, 0, static_cast<float>(frame_scale_i(8)));
            return 1;
        }
        if (hero_has_water() && (lbu(P + 0x20a9) != 0 || state == 0x73)) {
            sw(P + 0x2084, 0x73);
            hero_anim_set(0x60, 0, static_cast<float>(frame_scale_i(8)));
            return 1;
        }
        if (hero_has_beam() && (lh(P + 0x308) == 1 || state == 0x3f)) {
            sw(P + 0x2084, 0x3f);
            hero_anim_set(0x5b, 0, static_cast<float>(frame_scale_i(8)));
            return 1;
        }
        sw(P + 0x2088, 0);
        sh(P + 0x3b8, 0);
        sh(P + 0x3bc, 0);
        int32_t blend = frame_scale_i(8);
        if (lw(P + 0x22a8) == 1) blend = frame_scale_i(0xc);
        if (lw(P + 0x209c) == 0x23) blend = frame_scale_i(0xf);
        if (lw(P + 0x209c) == 6 || lw(P + 0x209c) == 0x2d) blend = frame_scale_i(0x12);
        if (lw(P + 0x20a0) == 4 && frame_scale_i(5) < lw(P + 0x418)) blend = frame_scale_i(0xd);
        if (mode == 0) return 1;
        const uint32_t g = lwu(P + 0x2088);
        hero_anim_set(g + 3, lwu(OV(0x0015f6d8) + (g << 2)), static_cast<float>(blend));
        return 1;
    }
    case 0x71:
        if (!hero_has_beam()) return 1;
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        if (mode == 0) return 1;
        if (lw(P + 0x2090) == 4) {
            hero_anim_set(0x5e, 2, static_cast<float>(frame_scale_i(5)));
        } else {
            hero_anim_set(0x5e, 1, static_cast<float>(frame_scale_i(8)));
        }
        return 1;
    case 0x65: {
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, vec2_length(V(P + 0x110)));
        sb(P + 0x20ac, 1);
        if (lbu(P + 0x20a4) == 0) hero_hold_release();
        if (mode == 0) return 1;
        const uint32_t m = lbu(P + 0x20a4);
        if (m == 3) {
            hero_anim_set(1, 0, static_cast<float>(frame_scale_i(8)));
        } else if (m == 1) {
            hero_anim_set(2, 0, static_cast<float>(frame_scale_i(8)));
        } else if (m == 0) {
            const uint32_t g = lwu(P + 0x2088);
            const int32_t b = frame_scale_i(8);
            hero_anim_set(g + 3, lwu(OV(0x0015f6d8) + (g << 2)), static_cast<float>(b));
        }
        return 1;
    }
    case 0x31:
        if (!hero_has_ice_slide()) return 1;
        sb(P + 0x20a7, 1);
        sw(P + 0x2284, 3);
        sw(P + 0x208c, 0x10);
        mem_fill32(P + 0x9d0, 0, 0x20);
        copy_vel_150_from_100();
        if (mode != 0) hero_anim_set(0x64, 0, static_cast<float>(frame_scale_i(0xf)));
        return 1;
    case 0x03: {
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        copy_vel_150();
        if (hero_has_skate() && lbu(P + 0x12e2) != 0) return redirect(0x2f);
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x79);
        if (hero_item_idle() != 0) return 0;
        if (sub_00205000(0) == 0x54) {
            if (hero_set_state(0, 0) == 0) return 0;
            const uint32_t set = sub_00205000(0);
            hero_anim_set(set, 0, static_cast<float>(frame_scale_i(0x12)));
            return 0;
        }
        if (mode == 0) return 1;
        stop_anim(lbu(hm_old + 0x52));
        return 1;
    }
    case 0x66:
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, sub_002339d0(P + 0x110));
        copy_vel_150();
        if (mode == 0) return 1;
        stop_anim(lbu(hm_old + 0x52));
        return 1;
    case 0x04: {
        sw(P + 0x208c, 0xc);
        sw(P + 0x2284, 0);
        const float v = vec2_length(V(P + 0x110));
        const float lim = lwc1(0x0015ed6c) * 7.0f;
        swc1(P + 0x194, v);
        if (lim < v) swc1(P + 0x194, lim);
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x79);
        sw(P + 0xff8, 0x68);
        copy_vel_150();
        vec3_clamp_length(P + 0x150, lwc1(0x0015ed6c) * 7.0f);
        if (lwc1(0x0015ed6c) * 3.5f < lwc1(P + 0x168)) hero_sound_start(0xc, 0);
        if (mode == 0) return 1;
        const uint32_t cur = lbu(lwu(P + 0x2080) + 0x52);
        hero_anim_set(0xd, 2, static_cast<float>(frame_scale_i(cur == sub_00205000(0) ? 9 : 0xe)));
        sub_002325e0(3, 0x12);
        return 1;
    }
    case 0x24: {
        if (!hero_has_swingshot()) return 1;
        constexpr uint32_t S = 0x00141680;
        sw(P + 0x208c, 0xd);
        sw(P + 0x2284, 7);
        const uint32_t c = lhu(S + 0x60);
        if (!(0xfffeu < c)) sh(S + 0x60, c + 1);
        if (static_cast<int32_t>(lhu(S + 0x62)) < frame_scale_i(lw(0x0015eea4)) / 600) sh(S + 0x62, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 600));
        sw(S + 0x64, lwu(S + 0x64) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        const uint32_t t = lwu(P + 0x964);
        swc1(P + 0x180, atan2_f(lwc1(t + 0x10) - lwc1(P + 0x80), lwc1(t + 0x14) - lwc1(P + 0x84)));
        if (mode == 0) return 1;
        hero_anim_set(0x2f, 2, static_cast<float>(frame_scale_i(5)));
        sub_002325e0(0xa, 0xe);
        return 1;
    }
    case 0x25:
    case 0x26:
        if (!hero_has_swingshot()) return 1;
        sb(P + 0x20ac, 1);
        sw(P + 0x208c, 0xd);
        sw(P + 0x2284, 7);
        sw(P + 0x960, 0);
        sw(P + 0x974, 0);
        if (state == 0x26) swc1(P + 0x97c, vec3_distance(V(P + 0x80), V(lwu(P + 0x964) + 0x10)));
        if (mode != 0) hero_anim_set(0x30, 1, static_cast<float>(frame_scale_i(state == 0x25 ? 0xa : 5)));
        return 1;
    case 0x2c: {  // swing from the target picked by hero_pick_target_323 (+0x990)
        if (!hero_has_swingshot()) return 1;
        constexpr uint32_t S = 0x00141680;
        sw(P + 0x208c, 0xe);
        sw(P + 0x2284, 7);
        const uint32_t c = lhu(S + 0x60);
        if (!(0xfffeu < c)) sh(S + 0x60, c + 1);
        if (static_cast<int32_t>(lhu(S + 0x62)) < frame_scale_i(lw(0x0015eea4)) / 600) sh(S + 0x62, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 600));
        sh(P + 0x9be, 1);
        sw(S + 0x64, lwu(S + 0x64) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        const uint32_t t = lwu(P + 0x990);
        sw(P + 0x9a8, 0);
        sb(P + 0x20ac, 1);
        sw(P + 0x994, t);
        sh(P + 0x99c, 0);
        sh(P + 0x9bc, 0);
        sw(P + 0x9b4, 0);
        sw(P + 0x9b8, 0);
        const uint32_t pv = lwu(t + 0x78);
        swc1(P + 0x9a0, lwc1(pv + 4));
        swc1(pv + 0x14, lwc1(pv + 0x10) == lwc1(P + 0x9a8) ? lwc1(pv + 0xc) : lwc1(pv + 0x10));
        const float fs = lwc1(0x0015ed64);
        swc1(P + 0x9c4, fs * u2f(0x3ccccccdu));
        swc1(P + 0x9c8, fs * u2f(0x3ea8f5c3u));
        swc1(P + 0x9cc, lwc1(0x0015ed6c) * 11.0f);
        vec_clear(V(P + 0xa0));
        sh(P + 0x1e6, static_cast<uint32_t>(frame_scale_i(0x28)));
        if (lwc1(pv + 0x24) != 0.0f) swc1(P + 0x9c4, lwc1(pv + 0x24));
        if (lwc1(pv + 0x28) != 0.0f) swc1(P + 0x9c8, lwc1(pv + 0x28));
        if (lwc1(pv + 0x2c) != 0.0f) swc1(P + 0x9cc, lwc1(pv + 0x2c));
        const uint32_t m = lwu(P + 0x994);
        swc1(P + 0x9ac, atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84)));
        swc1(P + 0x9a4, vec3_distance(V(P + 0x80), V(lwu(P + 0x994) + 0x10)));
        swc1(P + 0x9b0, lwc1(0x0015ed70) * 27.0f);
        if (lw(P + 0x2094) == 4) {
            swc1(P + 0x9b0, lwc1(P + 0x4a0));
        } else if (lw(P + 0x2094) == 2) {
            swc1(P + 0x9b0, lwc1(P + 0x940));
        }
        if (mode == 0) return 1;
        hero_anim_set(0x34, 0, static_cast<float>(frame_scale_i(0xb)));
        swc1(P + 0xa90, u2f(0x3f333333u));
        return 1;
    }
    case 0x2d:
        sw(P + 0x2284, 7);
        sw(P + 0x208c, 2);
        vec_clear(V(P + 0xa0));
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) hero_anim_set(0xb, 0, static_cast<float>(frame_scale_i(0xf)));
        return 1;
    case 0x22: {
        const float v = lwc1(0x0015ed70) * 100.0f;
        sw(P + 0x208c, 0xb);
        swc1(P + 0x4c0, 39.0f);
        sw(P + 0x2284, 0x50);
        swc1(P + 0x4b0, v);
        sw(P + 0x4b8, 0);
        sw(P + 0x4c4, 0);
        if (mode != 0) hero_anim_set(0x2a, 0, static_cast<float>(frame_scale_i(6)));
        return 1;
    }
    case 0x28:
    case 0x74: {  // on a rail (0x28: grind path +0x560..; 0x74: the path +0x5f0..)
        const bool grind = state == 0x28;
        if (grind ? !hero_has_grind() : !hero_has_state_74()) return 1;
        const uint32_t R = grind ? P + 0x560 : P + 0x5f0;
        const int32_t old = lw(P + 0x2094);
        sw(P + 0x2284, 3);
        if (grind) {
            sw(P + 0xff8, 0x68);
            sw(P + 0x578, 1);
            swc1(P + 0x588, u2f(0xbfc90fdbu));
            sw(P + 0x208c, 0xf);
            sw(P + 0x570, 0);
            sw(P + 0x58c, 0);
            if (old != 0xf) {
                sw(P + 0x5ac, 0);
                sw(P + 0x5a8, 0xffffffffu);
                sw(P + 0x5b0, 0);
            }
            sw(P + 0x5b4, 0);
            sw(P + 0x5b8, 0);
            sh(P + 0x5bc, 0);
            sw(P + 0x5c0, 0);
        } else {
            sw(P + 0x208c, 0x1a);
            sb(P + 0x20a7, 1);
            sw(P + 0xff8, 0x68);
            sw(P + 0x600, 0);
            sw(P + 0x618, 0);
        }
        int32_t idx = lw(R + 4);
        float frac = lwc1(R + 8);
        Vec4 pt;
        path_advance_v(lwu(R), pt, &idx, &frac, lw(R + (grind ? 0x24 : 0x1c)), 1.0f);
        Vec4 d;
        vec3_sub(d, pt, V(P + 0x80));
        if (u2f(0x3a83126fu) < vec3_length(d)) {
            vec3_set_length(d, d, 1.0f);
            if (!grind) {
                swc1(P + 0x604, vec3_dot(V(P + 0x100), d));
            } else if (lw(P + 0x2094) != 0xf) {
                swc1(P + 0x5b0, vec3_dot(V(P + 0x100), d));
            }
        }
        if (!grind) {
            const float v = lwc1(P + 0x604) * 0.5f;
            swc1(P + 0x604, v);
            if (v < 0.0f) sw(P + 0x604, 0);
            hero_anim_set(0x73, 0, static_cast<float>(frame_scale_i(8)));
            return 1;
        }
        const uint32_t r = lwu(P + 0x5d8);
        if (r != 0) {
            const float to = atan2_f(lwc1(r + 0x10) - lwc1(P + 0x80), lwc1(r + 0x14) - lwc1(P + 0x84));
            const float along = angle_add(lwc1(P + 0x588), atan2_f(d.x, d.y));
            if (u2f(0x3fc90fdbu) < angle_dist(to, along)) {
                swc1(P + 0x588, u2f(0x3fc90fdbu));
                sw(P + 0x58c, 1);
            }
        }
        if (mode != 0) hero_anim_set(lwu(P + 0x58c) + 0x31, 0, static_cast<float>(frame_scale_i(6)));
        return 1;
    }
    case 0x18: {
        const float v = lwc1(0x0015ed70) * 24.0f;
        const float z = lwc1(P + 0x88);
        sw(P + 0x208c, 3);
        sw(P + 0x2284, 0xd);
        swc1(P + 0x4ec, v);
        swc1(P + 0x4f0, z);
        if (lw(P + 0x2094) == 4) {
            swc1(P + 0x4ec, lwc1(P + 0x4a0));
            swc1(P + 0x4f0, lwc1(P + 0x410));
        }
        sb(P + 0x20a7, 1);
        if (mode == 0) return 1;
        if (lwc1(0x0015ed6c) * 3.5f < lwc1(P + 0x164)) {
            hero_anim_set(0x20, 0, static_cast<float>(frame_scale_i(9)));
        } else {
            hero_anim_set(0x20, 1, static_cast<float>(frame_scale_i(9) + 2));
        }
        return 1;
    }
    case 0x19:
        sw(P + 0x208c, 3);
        sw(P + 0xff8, 0x68);
        sw(P + 0x2284, 0xd);
        sb(P + 0x20a7, 1);
        if (mode != 0) hero_anim_set(0x21, 0, static_cast<float>(frame_scale_i(6)));
        return 1;
    case 0x1a:
    case 0x1b:
        sb(P + 0x20a7, 1);
        sw(P + 0x208c, 3);
        sw(P + 0x2284, 0xd);
        if (mode != 0) hero_anim_set(state == 0x1a ? 0x24 : 0x25, 0, static_cast<float>(frame_scale_i(6)));
        return 1;
    case 0x79:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) hero_anim_set(0xb, 0, static_cast<float>(frame_scale_i(0xc)));
        return 1;
    case 0x06:
    case 0x77: {
        const float f = lwc1(0x0015ed70);
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        swc1(P + 0x940, f * 24.0f);
        swc1(P + 0x944, f * 7.0f);
        if (lw(P + 0x2084) == 0x77) {
            if (lbu(P + 0x20a4) == 0) sh(P + 0x22b2, static_cast<uint32_t>(hero_sound_start(0x17, 0x20)));
            const float lo = u2f(0x40490fdbu), hi = u2f(0x408ba058u);
            const float a = lwc1(0x0015ed6c);
            swc1(P + 0x950, rand_range_signed_f(a * lo, a * hi));
            const float b = lwc1(0x0015ed6c);
            swc1(P + 0x954, rand_range_signed_f(b * lo, b * hi));
        }
        sw(P + 0xff8, 0x68);
        sw(P + 0x2088, 0);
        V(P + 0xe0) = V(P + 0x110);
        if (mode == 0) return 1;
        const uint32_t m = lbu(P + 0x20a4);
        if (hero_has_clank() && m == 1) {
            hero_anim_set(4, 0, static_cast<float>(frame_scale_i(8)));
            return 2;
        }
        if (m == 3) {
            hero_anim_set(6, 0, static_cast<float>(frame_scale_i(8)));
            return 2;
        }
        if (1.75f < lwc1(P + 0x2dc)) {
            hero_anim_set(0xb, 0, static_cast<float>(frame_scale_i(0xc)));
            sw(P + 0x2088, 1);
        } else {
            hero_anim_set(0xa, 0, static_cast<float>(frame_scale_i(0xa)));
        }
        return 1;
    }
    case 0x81:
    case 0x08: {
        const float lim = lwc1(0x0015ed6c) * 4.5f;
        const float sp = lwc1(P + 0x160);
        if (state == 0x81) {
            sb(P + 0x22ca, 1);
            sw(P + 0xff8, 0x68);
            sw(P + 0x208c, 1);
        } else {
            sw(P + 0x208c, 5);
            sw(P + 0xff8, 0x68);
        }
        sw(P + 0x2284, 0);
        if (lim < sp) sub_00213de8(fpu_div(lim, sp));
        V(P + 0xe0) = V(P + (state == 0x81 ? 0x110 : 0x100));
        if (mode != 0) hero_anim_set(0x13, 0, -2.0f);
        return 1;
    }
    case 0x07: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
    case 0x10: case 0x11: case 0x12: case 0x1c: case 0x29: case 0x2a: case 0x3c: case 0x49:
    case 0x4c: case 0x5e: case 0x69:
        return enter_jump(state, mode, hm_old, old_2088);
    case 0x13: case 0x14: case 0x15: case 0x20: case 0x23: case 0x51:
        return enter_attack(mode);
    case 0x70: {  // wrench throw / combo finisher: step 5, or 6 chained from a step-4 end
        if (!hero_has_beam()) return 1;
        constexpr OvConst<0x0017bc28u> COMBO{};
        sw(P + 0x208c, 6);
        sw(P + 0xa64, 0);
        if (lw(P + 0x2090) == 0x70) {
            sw(P + 0xa60, lw(COMBO + lwu(P + 0xa60) * 0x2c + 4) == 0 ? 6u : 5u);
        } else {
            sw(P + 0xa60, 5);
        }
        const int32_t k = lw(COMBO + lwu(P + 0xa60) * 0x2c + 4);
        if (mode == 0) return 1;
        const uint32_t how = k == 0 ? 1u : 0u;
        const int32_t n = frame_scale_i(5);
        hero_anim_set(static_cast<uint32_t>(k + 0x5c), how, static_cast<float>(n));
        const uint32_t w = sub_0020d460(0);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, static_cast<uint32_t>(k + 0xc), static_cast<int32_t>(how), n);
        return 1;
    }
    case 0x2b: {
        if (!hero_has_grind()) return 1;
        sw(P + 0x208c, 0xf);
        sw(P + 0xa64, 0);
        sw(P + 0xa60, lw(P + 0x2090) == 0x2b ? (lw(P + 0xa60) == 0 ? 1u : 0u) : 0u);
        if (mode == 0) return 1;
        const int32_t n = frame_scale_i(4);
        hero_anim_set(lwu(P + 0xa60) + 0x4e, 0, static_cast<float>(n));
        const uint32_t w = sub_0020d460(0);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, lwu(P + 0xa60) + 0xc, 0, n);
        return 1;
    }
    case 0x21: {
        sw(P + 0x208c, 0xa);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, lwc1(0x0015ed6c) * 9.0f);
        if (mode == 0) return 1;
        const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
        int32_t k = lw(e + 4);
        if (lw(e) == 1) k = 2;
        const int32_t n = frame_scale_i(5);
        hero_anim_set(static_cast<uint32_t>(k + 0x27), 5, static_cast<float>(n));
        const uint32_t w = sub_0020d460(0);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, static_cast<uint32_t>(k + 7), 5, n);
        return 1;
    }
    case 0x7a:
        sw(P + 0x208c, 0xa);
        sw(P + 0x2284, 0);
        swc1(P + 0xa68, angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        swc1(P + 0x194, lwc1(0x0015ed6c) * 9.0f);
        if (mode != 0) hero_anim_set(0x29, 5, static_cast<float>(frame_scale_i(8)));
        return 1;
    case 0x16: {
        sw(P + 0x208c, 7);
        sw(P + 0x2284, 0);
        sub_00213de8(0.5f);
        sh(P + 0x1ee, static_cast<uint32_t>(frame_scale_i(0x2d)));
        V(P + 0xe0) = V(P + 0x110);
        vec3_clamp_length_v(V(P + 0xe0), lwc1(0x0015ed6c) * 7.0f);
        const uint32_t r = sub_0025a420(lwu(P + 0x2080), 1, 0);
        hero_counter_use(r != 0 ? float_to_int(lwc1(r + 0x2c)) : 1);
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x4d)));
        if (mode == 0) return 1;
        scripted_anim(0x10, -3.0f);
        return 1;
    }
    case 0x68: {  // (+0x1660 counts consecutive bounces off 0x69)
        if (!hero_has_lava()) return 1;
        const int32_t prev = lw(P + 0x2090);
        if (prev == 0x69 || (lw(P + 0x209c) == 0x69 && lw(P + 0x2094) != 0 && prev != 7)) {
            sw(P + 0x1660, lwu(P + 0x1660) + 1);
        } else {
            sw(P + 0x1660, 0);
        }
        sw(P + 0x208c, 0x19);
        sw(P + 0x2284, 0);
        hero_sound_start(9, 0);
        sw(P + 0x194, 0);
        sw(P + 0xff8, 0x68);
        sw(P + 0x190, 0);
        if (mode != 0) anim_fs(0x71, 0, 8);
        return 1;
    }
    case 0x7b:
        if (!hero_has_lava()) return 1;
        sw(P + 0x208c, 0x19);
        sw(P + 0x2284, 0);
        if (lw(0x0015ed84) == 0xf) sub_00217450();
        sw(P + 0x1cc, 10000);
        hero_sound_start(9, 0);
        sw(P + 0xff8, 0x68);
        if (mode != 0) anim_fs(0x71, 0, 8);
        return 1;
    case 0x7c:
        if (!hero_has_state_7c()) return 1;
        if (lw(0x0015ed84) == 6) {
            hero_set_state(0x3d, 1);
            sw(P + 0x22f4, 1);
            uint32_t a = 0, b = 0, c = 0;
            moby_get_color_hi_v(lwu(P + 0x2080), &a, &b, &c);
            sw(P + 0x22f8, (c << 16) | 0x80000000u | (b << 8) | a);
            return 0;
        }
        sw(P + 0x208c, 0x14);
        swc1(P + 0x2f4, lwc1(P + 0x2d8));
        sw(P + 0x1cc, 10000);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        hero_sound_start(9, 0);
        if (mode != 0) anim_fs(0x71, 0, 8);
        return 1;
    case 0x7d: {
        sw(P + 0x208c, 0x14);
        sw(P + 0x1cc, 10000);
        swc1(P + 0x2f4, lwc1(P + 0x2d8));
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        const uint32_t veh = lwu(P + 0xa88);
        V(veh + 0x10) = V(P + 0x80);
        moby_sound_start(9, 0, veh);
        if (mode != 0) anim_fs(0x11, 0, 8);
        return 1;
    }
    case 0x43:
        sw(P + 0x208c, 0);
        sw(P + 0x2284, 0);
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x52);
        if (mode != 0) anim_fs(0, 0, 9);
        return 1;
    case 0x4f: {
        sw(P + 0x208c, 5);
        sw(P + 0x2284, 0);
        const float lim = lwc1(0x0015ed6c) * 4.5f, sp = lwc1(P + 0x160);
        if (lim < sp) sub_00213de8(fpu_div(lim, sp));
        V(P + 0xe0) = V(P + 0x100);
        if (mode != 0) anim_fs(0xe, 0, 0xc);
        return 1;
    }
    case 0x45:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        swc1(P + 0x944, lwc1(0x0015ed70) * 7.0f);
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) anim_fs(4, 0, 8);
        return 1;
    case 0x52:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) anim_fs(4, 0, 0xc);
        return 1;
    case 0x46:
        sw(P + 0x208c, 7);
        sw(P + 0x2284, 0);
        sub_00213de8(0.5f);
        V(P + 0xe0) = V(P + 0x110);
        vec3_clamp_length_v(V(P + 0xe0), lwc1(0x0015ed6c) * 5.0f);
        hero_counter_use(1);
        sh(P + 0x1ee, static_cast<uint32_t>(frame_scale_i(0x2d)));
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x32)));
        if (mode != 0) scripted_anim(6, -3.0f);
        return 1;
    case 0x44:
        if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) return redirect(0x52);
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, vec2_length(V(P + 0x110)));
        if (lw(P + 0x2090) == 3) sw(P + 0x194, 0);
        if (lw(P + 0x2094) == 0) swc1(P + 0x194, vec2_length(V(P + 0x150)));
        sh(P + 0x3b8, 0);
        if (mode != 0) anim_fs(2, 0, 8);
        return 1;
    case 0x47:
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        sw(P + 0x22a8, 0);
        V(P + 0x150) = V(P + 0x110);
        if (mode != 0) anim_fs(5, 0, 0xc);
        return 1;
    case 0x4a:
        sw(P + 0x208c, 3);
        sw(P + 0x2284, 0xd);
        swc1(P + 0x4ec, lwc1(0x0015ed70) * 12.0f);
        swc1(P + 0x4f0, lwc1(P + 0x88));
        if (lw(P + 0x2094) == 4) {
            swc1(P + 0x4ec, lwc1(P + 0x4a0));
            swc1(P + 0x4f0, lwc1(P + 0x410));
        }
        if (mode != 0) hero_anim_set(8, 0x12, static_cast<float>(frame_scale_i(7) + 2));
        return 1;
    case 0x4b:
    case 0x4d:
    case 0x4e:
        sw(P + 0x208c, 3);
        sw(P + 0x2284, 0xd);
        if (mode != 0) anim_fs(state == 0x4b ? 0xa : state == 0x4d ? 0xb : 0xc, 0, 6);
        return 1;
    case 0x5a:
    case 0x53:
        // (1.02 Gameplay, fix 9: the 1.02 build, 0x2276d8, takes 0x53 on deadly ground to 0x58)
        if (state == 0x53 && gameplay102_deadly_landing(0)) return 0;
        sw(P + 0x2284, 0);
        sw(P + 0x208c, 0);
        if (mode != 0) anim_fs(0, 0, state == 0x5a ? 0xf : 0xe);
        return 1;
    case 0x60: {
        sw(P + 0x208c, 6);
        sw(P + 0x2284, 0);
        if (lw(P + 0x2090) == 0x60 || (lw(P + 0x209c) == 0x60 && lw(P + 0x2098) < frame_scale_i(10))) {
            sw(P + 0xa60, static_cast<uint32_t>((lw(P + 0xa60) + 1) % 3));
        } else {
            sw(P + 0xa60, 0);
        }
        int32_t n = frame_scale_i(7);
        if (lw(P + 0x2090) == 0x60) n = frame_scale_i(0xd);
        if (mode != 0) hero_anim_set(lwu(P + 0xa60) + 9, 1, static_cast<float>(n));
        return 1;
    }
    case 0x61:
    case 0x5f:
        sw(P + 0x208c, 6);
        sw(P + 0x2284, 0);
        if (mode != 0) anim_fs(state == 0x61 ? 0xc : 8, 0, 8);
        return 1;
    case 0x5c:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        swc1(P + 0x940, lwc1(0x0015ed70) * 24.0f);
        swc1(P + 0x944, lwc1(0x0015ed70) * 7.0f);
        V(P + 0xe0) = V(P + 0x110);
        if (mode == 0) return 1;
        if (2.5f < lwc1(P + 0x2dc)) {
            anim_fs(5, 0, 8);
        } else {
            anim_fs(0, 0, 0xb);
        }
        return 1;
    case 0x5b:
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        swc1(P + 0x194, vec2_length(V(P + 0x110)));
        sh(P + 0x3b8, 0);
        if (mode != 0) anim_fs(3, 0x12, 8);
        return 1;
    case 0x62:
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        V(P + 0x150) = V(P + 0x110);
        if (mode != 0) anim_fs(7, 0, 0xc);
        return 1;
    case 0x5d:
        sw(P + 0x208c, 7);
        sw(P + 0x2284, 0);
        sub_00213de8(0.5f);
        V(P + 0xe0) = V(P + 0x110);
        vec3_clamp_length_v(V(P + 0xe0), lwc1(0x0015ed6c) * 7.0f);
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x4d)));
        if (mode != 0) scripted_anim(6, -3.0f);
        return 1;
    case 0x59:
        sw(P + 0x2284, 0);
        sw(P + 0x208c, 0);
        if (mode == 0) return 1;
        anim_fs(4, 0, 7);
        sub_002325e0(0xe, 0x22);
        return 1;
    case 0x58:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) anim_fs(6, 0, 0xc);
        return 1;
    case 0x55:
        sw(P + 0x208c, 2);
        sw(P + 0x2284, 0);
        swc1(P + 0x940, lwc1(0x0015ed70) * 24.0f);
        swc1(P + 0x944, lwc1(0x0015ed70) * 7.0f);
        V(P + 0xe0) = V(P + 0x110);
        if (mode != 0) anim_fs(6, 0, 7);
        return 1;
    case 0x54: {
        if (gameplay102_deadly_landing(1)) return 0;  // (1.02 Gameplay, fix 9: the 1.02 build, 0x227814)
        sw(P + 0x208c, 1);
        sw(P + 0x2284, 0);
        const float v = vec2_length(V(P + 0x110)), cap = lwc1(0x0015ed6c) * 1.5f;
        swc1(P + 0x194, v);
        if (cap < v) swc1(P + 0x194, cap);
        sh(P + 0x3b8, 0);
        if (mode != 0) anim_fs(1, 0, 8);
        return 1;
    }
    case 0x57:
        sw(P + 0x208c, 0x14);
        sw(P + 0x2284, 0);
        V(P + 0x150) = V(P + 0x110);
        if (mode != 0) anim_fs(3, 0, 0xc);
        return 1;
    case 0x56:
        sw(P + 0x208c, 7);
        sw(P + 0x2284, 0);
        sub_00213de8(0.5f);
        V(P + 0xe0) = V(P + 0x110);
        vec3_clamp_length_v(V(P + 0xe0), lwc1(0x0015ed6c) * 5.0f);
        hero_counter_use(1);
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x4d)));
        if (mode != 0) scripted_anim(2, -3.0f);
        return 1;
    case 0x42: {  // (rail: +0x5b0 speed capped at 2.5)
        sw(P + 0x208c, 0xf);
        sw(P + 0x2284, 0);
        if (lw(P + 0x570) == 0) hero_counter_use(1);
        sw(P + 0x1c0, static_cast<uint32_t>(frame_scale_i(0x4d)));
        const float cap = lwc1(0x0015ed6c) * 2.5f;
        swc1(P + 0x5cc, cap);
        if (cap < lwc1(P + 0x5b0)) swc1(P + 0x5b0, cap);
        sw(P + 0x5ac, 0);
        vec_clear(V(P + 0xe0));
        vec_clear(V(P + 0x100));
        swc1(P + 0xe8, lwc1(0x0015ed6c) * 10.0f);
        if (mode != 0) hero_anim_set(0x10, 2, -3.0f);
        return 1;
    }
    // (0x1d, 0x1e, 0x1f, 0x72, 0x27 and 0x3b play their animation in every mode)
    case 0x1d:
    case 0x1f:
    case 0x72:
    case 0x27: {
        if (state == 0x1d || state == 0x72) sb(P + 0x20ac, 1);
        sw(P + 0x208c, state == 0x27 ? 8u : 9u);
        sw(P + 0x2284, 0);
        if (state == 0x72) hero_hold_release();
        const uint32_t set = sub_00205000(0);
        anim_fs(set, 0, 10);
        return 1;
    }
    case 0x1e: {
        sw(P + 0x2284, 0x51);
        sw(P + 0x208c, 0);
        if (sub_002050b8(lbu(lwu(P + 0x2080) + 0x53)) == sub_00205000(1)) return 1;
        const uint32_t set = sub_00205000(1);
        anim_fs(set, 0, 5);
        return 1;
    }
    case 0x32:
        sb(P + 0x20ac, 1);
        sw(P + 0x208c, 9);
        sw(P + 0x2284, 0);
        sb(P + 0x20a7, 1);
        sb(P + 0x20ad, 1);
        if (mode != 0) hero_anim_set(sub_00205000(0), 0, -1.0f);
        return 1;
    case 0x78:
        sb(P + 0x20ac, 1);
        sw(P + 0x208c, 9);
        sw(P + 0x2284, 0);
        sb(P + 0x20a7, 1);
        vec_clear(V(P + 0xe0));
        if (mode != 0) hero_anim_set(0xb, 0, -1.0f);
        return 1;
    case 0x3b: {
        if (!hero_has_state_3b()) return 1;
        sb(P + 0x20ad, 1);
        sb(P + 0x20a7, 1);
        sw(P + 0x208c, 9);
        const int32_t n = frame_scale_i(8);
        hero_anim_set(0x44, 2, static_cast<float>(n));
        const uint32_t w = lwu(P + 0x1090);
        if (moby_is_class_0x47(w) != 0) moby_anim_set(w, 0xb, 2, n + frame_scale_i(5));
        return 1;
    }
    case 0x63:
    case 0x64:
        sw(P + 0x208c, 0x18);
        sb(P + 0x20ac, 1);
        hero_hold_release();
        if (mode != 0) hero_anim_set(sub_00205000(0), 0, -1.0f);
        return 1;
    case 0x3e:
        if (hero_has_state_3e()) hero_state_3e_set(mode);
        return 1;
    case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f:
        if (hero_has_hoverboard()) hero_hoverboard_set_state(state, mode);
        return 1;
    default:  // 5, 0x40, 0x48, 0x50, 0x7e: only the common tail
        return 1;
    }
}
}  // namespace

// Enter hero state `state` (PLAYER+0x2084; previous one in +0x2090, previous substate +0x2094,
// +0x209c / +0x20a0 the ones before, +0x2098 the old state time +0x198). Refused (0) while in
// substate 0x14 unless (100, 2), on levels 2 / 6 unless mode 2, and in substates 2 / 7 with
// +0x22a8 clear except for a few states. Leaving mode 0 clears the per-state flags and retires
// the hero's child mobys; mode 3 is left for the states outside 0..1, 0x53..0x59, 0x65..0x67.
// The entry actions of the state follow (with mode != 0 its animation); the common tail then
// flags the jump-ready edges +0x210c / +0x2110, handles the substate change and resets the
// state timers. Returns 1 when entered.
// orig: 0x002223f8
int32_t hero_set_state(uint32_t state, int32_t mode) {
    if (on_l1_family()) return hero_set_state_l1(state, mode);
    if (lw(P + 0x208c) == 0x14) {
        if (state != 100) return 0;
        if (mode != 2) return 0;
    }
    const int32_t lvl = lw(OV(0x0015f5c4));
    if ((lvl == 2 || lvl == 6) && mode != 2) return 0;
    if (lw(P + 0x22a8) == 0) {
        const int32_t sub = lw(P + 0x208c);
        if (sub == 2 || sub == 7) {
            static const uint32_t ok[] = {6, 0x3d, 0x47, 0x57, 0x62, 0x77, 0x7b, 0x7c, 0x7f, 0x6a, 0x80, 0x82};
            bool found = false;
            for (uint32_t s : ok) found = found || s == state;
            if (!found) return 0;
        }
    }
    const uint32_t old_20a7 = lbu(P + 0x20a7);
    const uint32_t hm = lwu(P + 0x2080);
    const uint32_t t198 = lwu(P + 0x198), s2090 = lwu(P + 0x2090), s2094 = lwu(P + 0x2094);
    const uint32_t s2084 = lwu(P + 0x2084), s208c = lwu(P + 0x208c);
    const uint32_t mode20a4 = lbu(P + 0x20a4);
    const uint32_t old_2088 = lwu(P + 0x2088);
    sw(P + 0x209c, s2090);
    sw(P + 0x20a0, s2094);
    sw(P + 0x2098, t198);
    sw(P + 0x2090, s2084);
    sw(P + 0x2094, s208c);
    sw(P + 0x2084, state);
    sw(P + 0x2088, 0);
    sb(P + 0x20a5, 0);
    sb(P + 0x20ac, 0);
    if (mode20a4 == 0) {
        sw(hm + 0x98, 0);
        sb(P + 0x20ad, 0);
        sb(P + 0x20a7, 0);
        sb(P + 0x20b2, 0);
        sb(P + 0x20af, 0);
        sh(P + 0x22e0, 0);
        retire_children(P + 0x1670);
    }
    if (lbu(P + 0x20a4) == 3) {
        const uint32_t st = lwu(P + 0x2084);
        if (!(st - 0x53u < 7u) && !(st - 0x65u < 3u) && st != 0 && st != 1) {
            hero_mode3_exit();
            gameplay102_mode3_left(state);  // (1.02 Gameplay, fix 13: the 1.02 build, 0x222d60)
        }
    }
    hero_layers_start(lw(P + 0x2090), lw(P + 0x2084));
    sw(P + 0xff8, 0);
    const uint32_t limit = hero_has_water() ? 0x83u : 0x82u;
    if (state < limit) {
        const int32_t r = enter_state(state, mode, hm, old_2088);
        if (r == 0) return 0;
        if (r == 2) return 1;
    }
    if (old_20a7 != 0 && lbu(P + 0x20a7) == 0) sw(P + 0x210c, 1);
    if (lh(P + 0x308) == 0 && lw(P + 0x1108) == 0x1c && lw(P + 0x20f4) != 0) sw(P + 0x2110, 1);
    if (lw(P + 0x208c) != 0xf && lw(P + 0x1108) == 0x1d && lw(P + 0x20f4) != 0) sw(P + 0x2110, 1);
    if (sub_0020d568() == 0 && lw(P + 0x1108) == 0 && lw(P + 0x20f4) != 0) sw(P + 0x2110, 1);
    if (lw(P + 0x208c) == 0x14) {
        hero_hold_release();
        sb(P + 0x20ac, 1);
    }
    if (lw(P + 0x208c) != lw(P + 0x2094)) sub_00206c08();
    sw(P + 0x198, 0);
    sw(P + 0x1a0, 0);
    if (lw(P + 0x208c) != lw(P + 0x2094)) sw(P + 0x19c, 0);
    sw(P + 0x1a4, 0);
    return 1;
}

}  // namespace rac1
