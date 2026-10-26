// Ported overlay code: the hero's per-frame input -> state transitions (0x229b70).
#include <game/enhance/advanced.h>
#include <initializer_list>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
constexpr uint32_t PAD = 0x0013c940;

uint32_t buttons() { return lwu(PAD + 0x1a0); }
uint32_t pressed() { return lwu(PAD + 0x1a4); }


constexpr OvConst<0x0017a680u> FX{};
float fs6c() { return lwc1(0x0015ed6c); }

// the hydro-pack's / helipack's two flaps (+0x1180, +0x1184) closed to animation set 1 over
// `frames`, when they are open (+0x70 bit 1 for `need_open`)
// orig: 0x00229b70 (state 0, blocks 0x22a090 / 0x22a16c)
void pack_close(bool need_open, int32_t frames) {
    for (uint32_t o : {0x1180u, 0x1184u}) {
        const uint32_t m = lwu(P + o);
        if (m == 0) continue;
        if (need_open && (lbu(m + 0x70) & 2) == 0) continue;
        if (lbu(m + 0x53) == 1) continue;
        moby_anim_set(m, 1, 0, frame_scale_i(frames));
    }
}

// orig: (helper) enter a state, forced (mode 1)
void set(uint32_t state) { hero_set_state(state, 1); }

// Standing (state 0): the crouch-ready 1 on L1 / L2 (0x5), crouching 0x81, the idle animations
// (the item's stand set, idle events from 0x179890, the held item's pose), a slide off a slope
// 0x79, falling 6, the jump, the hover-board 0x71, the crouch 4 on R1 / L1, grinding 0x28, the
// board 0x2f and starting to run 2.
// orig: 0x00229b70 (state 0, block 0x229fdc)
void stand_input(uint32_t hm0) {
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    sub_00232628();
    if (lbu(P + 0x22ca) != 0 && (lh(P + 0x308) == 0 || lbu(0x0013d4dc) == 0)) {
        set(0x81);
        return;
    }
    bool idle_anim = true;
    if (sub_0020d498(0) == 0x1b) {
        if (lbu(lwu(P + 0x2080) + 0x53) != sub_00205000(0)) hero_anim_set(sub_00205000(0), 0, -2.0f);
        idle_anim = false;
    }
    if (lbu(P + 0x12e2) != 0) idle_anim = false;
    if (lw(P + 0x11a4) == 2) pack_close(true, 7);
    if (u2f(0x3f1c61aau) < lwc1(FX + 0x258) || u2f(0x3edf66f3u) < lwc1(FX + 0x254)) pack_close(false, 0xc);
    if (sub_002050b8(lbu(hm0 + 0x53)) == sub_00205000(0) && idle_anim) {
        int32_t ev = 0;
        if (hero_idle_event_pick(&ev) != 0) {  // an idle event: its state or animation
            const uint32_t e = OV(0x00179890) + static_cast<uint32_t>(ev) * 0x70;
            sw(P + 0x2270, static_cast<uint32_t>(ev));
            sh(P + 0x1e8, static_cast<uint32_t>(frame_scale_i(0x78)));
            sw(e + 0x60, static_cast<uint32_t>(float_to_int(frame_scale_f(lwc1(e + 0x5c)) * 60.0f)));
            if (lw(e + 0x44) != 0) {
                set(0x40);
            } else {
                hero_anim_set(lwu(e + 0x48), 0, static_cast<float>(frame_scale_i(0xc)));
            }
        }
    } else if ((lw(P + 0xa98) & 2) != 0) {  // the landing pose is over: back to the stand set
        bool keep = false;
        if (lbu(P + 0x20a8) != 0 && lbu(P + 0x20aa) != 0) {
            keep = lbu(lwu(P + 0x2080) + 0x53) == lwu(P + 0x2294) || lw(P + 0xd08) != 0;
        }
        if (!keep) {
            sb(P + 0x20a8, 0);
            if (sub_00205000(0) == 0x54) {
                const uint32_t s = sub_00205000(0);
                hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x12)));
            } else {
                hero_anim_set(sub_00205000(0), 0, -2.0f);
            }
        }
    }
    if (frame_scale_i(0xa) < lw(P + 0x198)) {  // the pack's flaps fidget now and then
        const uint32_t m = lwu(P + 0x1184);
        if (m != 0 && lbu(m + 0x53) == 1 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x22c4))) != 0 &&
            lwc1(FX + 0x258) < u2f(0x3f0efa35u) && lwc1(FX + 0x254) < u2f(0x3eb2b8c2u) && lw(P + 0x11a4) == 2) {
            const int32_t lo = frame_scale_i(0x6e);
            sw(P + 0x22c4, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x10e))));
            hero_hands_anim_start();
        }
    }
    if (lbu(P + 0x20a8) != 0 && lw(P + 0xa9c) == 0 && lbu(P + 0x20aa) != 0 && lh(P + 0x22c8) == 0) {  // the held item's pose
        const int32_t k = sub_0020d498(0);
        if (k != lw(P + 0x2298)) {
            hero_hold_release();
        } else {
            const uint32_t s = lwu(OV(0x00179ac0) + static_cast<uint32_t>(k) * 0x4c + 0x24);
            if (lbu(lwu(P + 0x2080) + 0x53) != s) {
                if (lw(P + 0xd08) != 0) sw(P + 0xd14, 1);
                sw(P + 0x2294, s);
                hero_anim_set(lwu(P + 0x2294), 0, static_cast<float>(frame_scale_i(0xb)));
            }
        }
    }
    hero_stick_move(0, 1.0f);
    if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) {
        set(0x79);
        return;
    }
    if (frame_scale_i(4) < lh(P + 0x30e) && u2f(0x3ecccccdu) < lwc1(P + 0x2dc)) {  // off the ground
        if (lw(P + 0x2090) == 8 && lw(P + 0x198) < frame_scale_i(0xf)) return;
        set(6);
        return;
    }
    if (lw(P + 0x1c4) != 0) return;
    bool jump = true;
    if ((lw(P + 0x2090) == 1 || lw(P + 0x2090) == 0x1e) && lw(P + 0x198) < frame_scale_i(5)) jump = false;
    if (jump && lh(P + 0x308) != 1) {
        const int32_t back = lw(P + 0x1d4) != 0 ? 1 : frame_scale_i(7);
        if (hero_try_jump(back, 0) != 0) return;
    }
    if (lh(P + 0x308) == 1 && pad_button_history(0x40, frame_scale_i(7), nullptr) != 0) {
        set(0x71);
        return;
    }
    if ((buttons() & 0xa) != 0 && lw(P + 0x300) != 0 &&
        (lbu(lwu(P + 0x2080) + 0x53) != 0x1a || !(lwc1(P + 0xaa8) < 108.0f))) {
        set(4);
        return;
    }
    if (lw(P + 0x56c) != 0) {
        set(0x28);
        return;
    }
    if (lbu(P + 0x12e2) != 0 && u2f(0x3db2b8c2u) < lwc1(P + 0x2e0)) {
        set(0x2f);
        return;
    }
    bool run = true;
    if ((lw(P + 0x2090) == 1 || lw(P + 0x2090) == 0x1e) && lw(P + 0x198) < frame_scale_i(0x18) && lh(P + 0x308) == 0) {
        hero_stick_move(0, 1.0f);
        if (u2f(0x3f9c61aau) < angle_dist(lwc1(P + 0x180), lwc1(OV(0x00166dd8)))) run = false;
    }
    if (!run || !(u2f(0x3e6147aeu) < lwc1(P + 0x229c))) return;
    if (lw(0x0015ed84) == 0xc) {
        const uint32_t foot = lbu(P + 0x12ed);
        hero_footstep(static_cast<int32_t>(foot), 0, 0);
        hero_footstep(static_cast<int32_t>(foot), 1, 0);
    }
    set(2);
}


// L1 / L2 held (state 1): the crouch-ready or the item's stand animation; on release back to
// standing (or idle out of a mode), with R1 / L1 the crouch 4. Not while holding a grabbed moby.
// orig: 0x00229b70 (state 1, block 0x22af4c)
void ready_input() {
    if (lbu(P + 0x20a4) == 0 && (lbu(P + 0x20aa) == 0 || lbu(P + 0x20a5) != 0)) {
        if ((buttons() & 0xa) != 0) {
            if (lbu(lwu(P + 0x2080) + 0x53) != 0xd) hero_anim_set(0xd, 0, static_cast<float>(frame_scale_i(8)));
        } else {
            const uint32_t s = sub_00205000(1);
            if (lbu(lwu(P + 0x2080) + 0x53) != s) hero_anim_set(s, 0, static_cast<float>(frame_scale_i(8)));
        }
    }
    bool free = true;
    const uint32_t held = lwu(P + 0x1090);
    if (held != 0 && moby_is_class_0x47(held) != 0 && lbu(held + 0x20) != 0) free = false;
    if ((buttons() & 5) != 0 || !free) return;
    if (lbu(P + 0x20a4) != 0) {
        hero_enter_idle();
        return;
    }
    if ((buttons() & 0xa) != 0) {
        set(4);
        return;
    }
    hero_set_state(0, 0);
    const uint32_t s = sub_00205000(1);
    if (lbu(lwu(P + 0x2080) + 0x53) != s) hero_anim_set(s, 0, static_cast<float>(frame_scale_i(8)));
}


// falling off the ground: after 8 frames airborne +0x30e above 0.3 (state 3), the fall 6 (not
// in the first 15 frames after the slam 8)
// orig: (helper) the shared tail of states 3 / 4
bool fall_check(float above) {
    if (!(frame_scale_i(8) < lh(P + 0x30e))) return false;
    if (above != 0.0f) return above < lwc1(P + 0x2dc);
    return true;
}

// The landing (state 3): L1 / L2 → 1, a jump press → 7 and R1 / L1 → crouch 4 (unless +0x1c4),
// falling 6, the landing animation over → standing, a rail → 0x28; with the stick the run 2 (the
// item stand 0x54 hands over to standing after 10 frames; no speed: standing at once).
// orig: 0x00229b70 (state 3, block 0x22c2e4)
void landed_input() {
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    if (lw(P + 0x1c4) == 0) {
        if ((pressed() & 0x40) != 0) {
            set(7);
            return;
        }
        if ((buttons() & 0xa) != 0) {
            set(4);
            return;
        }
    }
    if (fall_check(u2f(0x3e99999au))) {
        set(6);
        return;
    }
    if ((lw(P + 0xa98) & 2) != 0) {
        set(0);
        return;
    }
    if (lw(P + 0x56c) != 0) {
        set(0x28);
        return;
    }
    if (!(0.2f < lwc1(P + 0x229c))) return;
    if (sub_00205000(0) == 0x54) {
        if (10.0f < lwc1(P + 0xaa8) && hero_set_state(0, 0) != 0) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x11)));
        }
    } else if (lwc1(P + 0x194) == 0.0f) {
        hero_set_state(0, 0);
        return;
    }
    if (0.5f < lwc1(P + 0x229c)) set(2);
}

// Crouching (state 4): L1 / L2 → 1; R1 / L1 let go after 11 frames → standing; a slope slide
// 0x79; the jump (0x71 on the hover-board); the item button → the wrench spin 0x15; falling 6.
// orig: 0x00229b70 (state 4, block 0x22c45c)
void crouch_input() {
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    if (frame_scale_i(0xb) < lw(P + 0x198) && (buttons() & 0xa) == 0) {
        hero_set_state(0, 0);
        hero_anim_set(sub_00205000(0), 0, -2.0f);
        return;
    }
    if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) {
        set(0x79);
        return;
    }
    if (lh(P + 0x308) == 1) {
        if (pad_button_history(0x40, frame_scale_i(7), nullptr) != 0) {
            set(0x71);
            return;
        }
    } else if (hero_jump_input() != 0) {
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(0xf), nullptr) != 0 && lw(P + 0x10b8) == 8 && !advanced_rac3_wrench_out()) set(0x15);  // enhancement: R&C 3 throws from the crouch only with the wrench back
    if (!fall_check(0.0f)) return;
    if (lw(P + 0x2090) == 8 && lw(P + 0x198) < frame_scale_i(0xf)) return;
    set(6);
}


// the jump out of a run: on a run gait faster than the band's start (0x17bdb8 +0xc) and facing
// within 60 degrees of the stick, the long jump 9 when faster than 0.85 x the band's +0x18 (the
// plain jump 7 otherwise)
// orig: 0x00229b70 (block 0x22f2d4)
void run_jump() {
    if (advanced_rac3_movement()) {  // enhancement: R&C 3's running jump: the stick past 0.85, not while aiming
        set(advanced_rac3_running_jump() ? 9u : 7u);
        return;
    }
    if (advanced_strafing()) {  // enhancement: no long running jump while strafing
        set(7);
        return;
    }
    constexpr OvConst<0x0017bdb8u> BAND{};
    if (!(lwc1(BAND + 0xc) < lwc1(P + 0x190))) {
        set(7);
        return;
    }
    if (!(angle_dist(lwc1(P + 0x180), lwc1(P + 0x98)) < u2f(0x3f860a92u))) {
        set(7);
        return;
    }
    set(lwc1(BAND + 0x18) * fs6c() * u2f(0x3f59999au) < lwc1(P + 0x164) ? 9u : 7u);
}

// Running (states 2, 0x2f on the board, 0x73 with smoke +0x20a9, 0x7e): the slope slide 0x79,
// running off an edge 6, the jump (long jump 9 at speed), L1 / L2 → 1 and R1 / L1 → crouch 4,
// the board's animations, the switches between 2 / 0x2f / 0x73 / 0x3f, a rail 0x28, the item
// button flag +0x3be; stopping: the skid 3 at speed, else standing (after 25 frames idle);
// still running: the gait.
// orig: 0x00229b70 (states 2, 0x2f, 0x73, 0x7e, block 0x22b434)
void run_input(uint32_t hm0) {
    if (lh(P + 0x3bc) != 0 && (u2f(0x3f666666u) < lwc1(hm0 + 0x54) || lbu(hm0 + 0x52) != 0xff)) sh(P + 0x3bc, 0);
    if (lbu(P + 0x12ea) != 0 && lh(P + 0x30c) == 0) {
        set(0x79);
        return;
    }
    if (!(lh(P + 0x30e) < frame_scale_i(5)) && !(lw(P + 0x2094) == 0x12 && lbu(P + 0x20a9) != 0) &&
        u2f(0x3faccccdu) < lwc1(P + 0x2dc) && hero_floor_ahead() == 0) {
        set(6);
        return;
    }
    const int32_t back = lw(P + 0x1d4) != 0 ? 1 : frame_scale_i(9);
    if (pad_button_history(0x40, back, nullptr) != 0 && !(frame_scale_i(4) < lh(P + 0x30e) && lh(P + 0x1e2) == 0)) {
        hero_stick_move(0, 1.0f);
        if (advanced_strafe_flip()) {  // enhancement: aiming, the stick pushed sideways or back: the flip
            set(0xb);
            advanced_strafe_flip_entered();
            return;
        }
        if (hero_try_jump(back, 1) != 0) return;
        if (lw(P + 0x2088) <= 0) {
            set(7);
            return;
        }
        run_jump();
        return;
    }
    if (lh(P + 0x30e) != 0) return;
    bool board_fast = false;
    if (lbu(P + 0x12e2) != 0) board_fast = !(lwc1(P + 0x164) < fs6c() + fs6c()) || !(lwc1(P + 0x2e0) < u2f(0x3db2b8c2u));
    if (!board_fast && (buttons() & 5) != 0) {
        set(1);
        return;
    }
    if ((buttons() & 0xa) != 0) {
        set(4);
        return;
    }
    if (lbu(P + 0x12e2) != 0 && lw(P + 0x2084) == 0x2f) {  // the board's stand / push animations
        const float speed = vec2_length(V(P + 0x100));
        const uint32_t hm = lwu(P + 0x2080);
        bool done = false;
        if (lwc1(P + 0x229c) < u2f(0x3e2e147bu)) {
            const uint32_t a = lbu(hm + 0x53);
            const bool push = a == 0x37 || (a == sub_00205000(0) && u2f(0x3db2b8c2u) < lwc1(P + 0x2e0));
            if (push && frame_scale_i(8) < lw(P + 0x1ac) && lw(P + 0xa9c) == 0) {
                hero_anim_set(0x6d, 0, static_cast<float>(frame_scale_i(0xe)));
                done = true;
            }
        }
        if (!done && u2f(0x3e99999au) < lwc1(P + 0x229c) && lbu(lwu(P + 0x2080) + 0x53) != 0x37 && lw(P + 0xa9c) == 0) {
            hero_anim_set(0x37, 0, static_cast<float>(frame_scale_i(0xe)));
            done = true;
        }
        const bool coasting = lbu(lwu(P + 0x2080) + 0x53) == 0x6d && lw(P + 0xa9c) == 0 && lwc1(P + 0x229c) < u2f(0x3e2e147bu);
        if (!done && coasting && lwc1(P + 0x2e0) < u2f(0x3db2b8c2u) && speed < fs6c() * u2f(0x3fd9999au)) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x14)));
        }
    }
    {
        const uint32_t st = lwu(P + 0x2084);
        if (st == 2 || st == 0x7e) {
            if (lbu(P + 0x12e2) != 0) set(0x2f);
        } else if (st == 0x2f && lbu(P + 0x12e2) == 0) {
            set(2);
        }
    }
    {
        const uint32_t st = lwu(P + 0x2084);
        if (st == 2) {
            if (lbu(P + 0x20a9) != 0) set(0x73);
        } else if (st == 0x73 && lbu(P + 0x20a9) == 0) {
            set(2);
        }
    }
    if (lw(P + 0x2084) == 2 && lh(P + 0x308) != 0) set(0x3f);
    if (lw(P + 0x56c) != 0) {
        set(0x28);
        return;
    }
    if ((frame_scale_i(0xf) < lw(P + 0x198) || !(lwu(P + 0x2094) < 2u)) && (pressed() & 0x10000) != 0 && lbu(P + 0x20aa) == 0) {
        sh(P + 0x3be, 1);
    }
    const uint32_t st = lwu(P + 0x2084);
    if (st == 2) {
        if (!(lwc1(P + 0x229c) < u2f(0x3e2e147bu))) {
            hero_run_gait_update();
            return;
        }
        if (advanced_rac3_movement()) {  // enhancement: R&C 3 stops at once, no skid
            advanced_rac3_stop();
            if (hero_item_idle() != 0) return;
            if (hero_set_state(0, 0) == 0) return;
            if (sub_00205000(0) == 0x54) {
                const uint32_t s = sub_00205000(0);
                hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x12)));
            } else {
                hero_anim_set(sub_00205000(0), 0, static_cast<float>(frame_scale_i(6)));
            }
            return;
        }
        if (fs6c() * u2f(0x402ccccdu) < lwc1(P + 0x160) && lbu(P + 0x20aa) == 0) {  // stopping at speed: the skid
            if (!(lwc1(P + 0x160) < fs6c() * u2f(0x408ccccdu)) && !(pad_stick_history(4) < u2f(0x3f4ccccdu))) {
                set(3);
                return;
            }
            if (hero_set_state(3, 0) == 0) return;
            if (sub_00205000(0) == 0x54) {
                const uint32_t s = sub_00205000(0);
                hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x12)));
            } else {
                hero_anim_set(0x14, 7, static_cast<float>(frame_scale_i(9)));
            }
            return;
        }
        if (!(frame_scale_i(0x19) < lw(P + 0x198)) || hero_item_idle() != 0) return;
        if (hero_set_state(0, 0) == 0) return;
        if (sub_00205000(0) == 0x54) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x12)));
        } else {
            hero_anim_set(sub_00205000(0), 0, -1.0f);
        }
        return;
    }
    if (st == 0x2f) {  // the board: stop when slow and off the stick; the push animations' speed
        if (lw(PAD + 0x1d4) != 0 && vec2_length(V(P + 0xe0)) < fs6c() * u2f(0x3d4ccccdu) &&
            lwc1(P + 0x2e0) < u2f(0x3db2b8c2u) && hero_set_state(0, 0) != 0) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x11)));
        }
        const uint32_t a = lbu(hm0 + 0x53);
        if (a == 0x37) {
            float r = (1.0f - lwc1(P + 0x3b0)) * 2.0f;
            swc1(P + 0xa90, r);
            if (u2f(0x3fd9999au) < r) swc1(P + 0xa90, u2f(0x3fd9999au));
            if (lwc1(P + 0xa90) < 0.75f) swc1(P + 0xa90, 0.75f);
        } else if (a == 0x6d) {
            const float r = lwc1(P + 0x160) * 10.0f;
            swc1(P + 0xa90, r);
            if (u2f(0x3f8ccccdu) < r) swc1(P + 0xa90, u2f(0x3f8ccccdu));
            if (lwc1(P + 0xa90) < u2f(0x3e99999au)) swc1(P + 0xa90, u2f(0x3e99999au));
        }
        return;
    }
    if (st == 0x73) {  // smoking: stop when slow; the animation speed from the run speed
        if (lw(PAD + 0x1d4) != 0 && vec2_length(V(P + 0xe0)) < fs6c() * u2f(0x404ccccdu) && hero_set_state(0, 0) != 0) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x11)));
        }
        const float r = lwc1(P + 0x160) * 35.0f;
        swc1(P + 0xa90, r);
        if (r < u2f(0x3f19999au)) swc1(P + 0xa90, u2f(0x3f19999au));
        if (u2f(0x3f866666u) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x3f866666u));
    }
}


// orig: (helper) st is one of l
bool st_in(uint32_t st, std::initializer_list<uint32_t> l) {
    for (uint32_t v : l) {
        if (st == v) return true;
    }
    return false;
}

// The jump family (states 7, 9..0x12, 0x1c, 0x3c, 0x69) in the air: the long jump 0xb from a
// flick with R1 / L1 (+0x4ae), the wall jump 0xa / flip 0x10 off slot 3's wall kinds, the
// comet strike 0x22 near the ground, a rail 0x28 or 0x74, falling 6, the side flip 0x7a off a
// wall; the jump arc's animation pacing (+0x420 the take-off frame, +0x3d0..+0x3e0 the arc's
// frames, +0x440..+0x44c the landing animation, +0xa90 its speed); landing (+0x418) through
// hero_land or straight into the next jump; the double jump 0x11 (+0x1b4), the glide 0xe, the
// slam 8 (with jump held high enough above the ground), the ledge 0x18 (+0x4e8).
// orig: 0x00229b70 (states 7, 9..0x12, 0x1c, 0x3c, 0x69, block 0x22e7f4)
void jump_input(uint32_t hm0) {
    if (lbu(P + 0x4ae) != 0) {  // the long jump from a flick
        const int32_t t = hero_stick_turn_frames();
        if (0 < t && (lw(P + 0x198) < t || lw(P + 0x418) != 0) && lw(P + 0x300) != 0) {
            bool ok = lw(P + 0x2084) != 0xb || pad_button_history(0x40, frame_scale_i(7), nullptr) != 0;
            if (ok) {
                bool flick = true;
                if (lw(P + 0x2084) == 0x1c || lw(P + 0x418) != 0) flick = pad_button_history(0x40, frame_scale_i(7), nullptr) != 0;
                if (lw(P + 0x2084) == 0xb && lw(P + 0x418) == 0) flick = false;
                if (flick && pad_button_history(0x1f000, frame_scale_i(5), nullptr) != 0 && (buttons() & 0xa) != 0) {
                    set(0xb);
                    return;
                }
            }
        }
    }
    if (sub_0020d498(3) == 3 && lh(P + 0x22d8) == 0) {  // the flip off a wall
        const uint32_t st = lwu(P + 0x2084);
        if (!st_in(st, {0x10, 0x11, 0x12, 0x3c, 0xf, 0xa, 0x69}) && (st == 0xe || frame_scale_i(9) < lw(P + 0x198)) &&
            (buttons() & 0xa) != 0 && pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
            set(0x10);
            return;
        }
    }
    const bool early = lw(P + 0x198) < frame_scale_i(7) && pad_button_history(0xa, 2, nullptr) != 0;
    if (early && !st_in(lwu(P + 0x2084), {0x10, 0xa, 0x12, 0x3c, 0x11, 0xe, 0x69})) {  // R1 / L1 just after the take-off
        const float k = fs6c() * u2f(0x3f666666u);
        if (sub_0020d498(3) == 2 && lbu(0x0013d4c2) != 0 && lh(P + 0x22d8) == 0 && k < lwc1(P + 0x168)) {
            set(0xa);
        } else if (sub_0020d498(3) == 3 && lh(P + 0x22d8) == 0 && fs6c() * u2f(0x3f666666u) < lwc1(P + 0x168) &&
                   lw(P + 0x1b8) == 0) {
            set(0x10);
        }
    }
    bool comet = lw(P + 0x420) + frame_scale_i(8) < lw(P + 0x198) && !(lh(P + 0x41e) != 0 && !(frame_scale_i(0xa) < lw(P + 0x44c)));
    comet = comet && lwc1(P + 0x229c) < u2f(0x3e99999au) && st_in(lwu(P + 0x2084), {7, 9, 0xd, 0xf, 0xe});
    if (comet && pad_button_history(0xa, frame_scale_i(9), nullptr) != 0) {  // the comet strike
        if (sub_0020d498(3) != 3 || lh(P + 0x22d8) != 0) return;
        set(0x22);
        return;
    }
    if (lw(P + 0x56c) != 0) {
        set(0x28);
        return;
    }
    if (lw(P + 0x5fc) != 0) {
        set(0x74);
        return;
    }
    if (lh(P + 0x49a) < lw(P + 0x198)) {
        if (lw(P + 0x418) == 0) {
            set(6);
            return;
        }
    } else if (lw(P + 0x418) == 0 && (lw(P + 0x2084) == 0xa || lw(P + 0x2084) == 0x10)) {  // the side flip off a wall
        const bool wall = lwc1(P + 0x248) < u2f(0x3f666666u) && lbu(P + 0x257) != 0;
        if (wall && fs6c() * 7.0f < vec3_length(V(P + 0xe0)) && vec3_length(V(P + 0x100)) < vec3_length(V(P + 0xe0)) * 0.5f &&
            lw(P + 0x4e8) == 0 && lwc1(P + 0x2dc) < 2.0f) {
            set(0x7a);
            return;
        }
    }
    const bool glide_early = lw(P + 0x2084) == 0xe && lwc1(P + 0xaa8) < 35.0f;
    if (!(lw(P + 0x420) < lw(P + 0x198))) return;
    if (lw(P + 0x198) == lw(P + 0x420) + 1) {  // the frame after take-off: start the arc's animation
        if (lh(P + 0x41c) != 0) {
            const float n = static_cast<float>(lw(P + 0x3d0) - lw(P + 0x198));
            if (0.0f < n) sub_00208f20(lwc1(P + 0x3d8), n, lwc1(P + 0x494), -1.0f);
            return;
        }
        if (lw(P + 0x2084) == 0xe) return;
        int32_t frames = 0;
        sub_00233c30(P + 0x80, P + 0xe0, &frames, lwc1(P + 0x4a0));
        sub_00208f20(lwc1(P + 0x440), static_cast<float>(frames), lwc1(P + 0x494), -1.0f);
        if (1.5f < lwc1(P + 0xa90)) swc1(P + 0xa90, 1.5f);
        return;
    }
    if (lh(P + 0x41c) != 0 && !(lw(P + 0x198) < lw(P + 0x3d0)) && lw(P + 0x198) < lw(P + 0x3d4)) {  // the apex part
        const float n = static_cast<float>(lw(P + 0x3e0) - lw(P + 0x3d0));
        if (0.0f < n) sub_00208f20(lwc1(P + 0x440), n, lwc1(P + 0x494), -1.0f);
        return;
    }
    if (lw(P + 0x2f8) == 1) {
        hero_sound_start(0x11, 0);
        hero_land_fx(3, 0x18, 1);
    }
    if (lw(P + 0x418) != 0) {
        sw(P + 0x418, lwu(P + 0x418) + 1);
    } else if (lh(P + 0x30e) == 0 && lh(P + 0x41e) != 0 && !glide_early) {  // touching down: the landing animation
        sw(P + 0x418, static_cast<uint32_t>(frame_scale_i(1)));
        sub_00233288(5, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
        const int32_t f = float_to_int(lwc1(P + 0x448)) + 2;
        if (lwc1(P + 0xaa8) < lwc1(P + 0x444) - 1.5f) {
            const uint32_t s = sub_002050b8(lbu(hm0 + 0x53));
            hero_anim_set(s, static_cast<uint32_t>(f), static_cast<float>(frame_scale_i(7)));
        }
    }
    if (lw(P + 0x418) != 0) {
        swc1(P + 0xa90, 1.0f);
    } else if (lh(P + 0x41e) != 0) {  // falling towards the ground: pace the animation to the landing
        if (static_cast<int32_t>(lbu(hm0 + 0x51)) < float_to_int(lwc1(P + 0x448))) {
            if (lw(P + 0x2084) == 0xe && lwc1(P + 0xaa8) < 37.0f) {
                approach_f(P + 0xa90, 1.0f, lwc1(0x0015ed64) * u2f(0x3e19999au));
            } else {
                const float t = hero_fall_time(60.0f, lwc1(P + 0x4a0), -1.0f);
                const int32_t n = float_to_int(t);
                sw(P + 0x44c, static_cast<uint32_t>(n));
                sub_00208f20(lwc1(P + 0x444), static_cast<float>(n), lwc1(P + 0x494), lwc1(0x0015ed64) * 0.2f);
            }
            if (!glide_early) {
                const int32_t f = float_to_int(lwc1(P + 0x448)) + 2;
                if (u2f(0x3fe66666u) < lwc1(P + 0xa90)) {
                    const uint32_t s = sub_002050b8(lbu(hm0 + 0x53));
                    hero_anim_set(s, static_cast<uint32_t>(f), static_cast<float>(lw(P + 0x44c) + 3));
                }
            }
            if (lwc1(P + 0xa90) < 0.2f) swc1(P + 0xa90, 0.2f);
            if (u2f(0x402ccccdu) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x402ccccdu));
        }
        if (lw(P + 0x2084) == 9 && 6.0f < lwc1(P + 0x444) - lwc1(P + 0xaa8)) {  // the long jump turns into a jump when slow
            const uint32_t a = sub_002050b8(lbu(hm0 + 0x52));
            const bool jump_anim = (a == 8 || sub_002050b8(lbu(hm0 + 0x52)) == 9) && lbu(hm0 + 0x52) == lbu(hm0 + 0x53);
            const bool slow = lwc1(P + 0x190) < lwc1(OV(0x0017bdd0)) * fs6c() * u2f(0x3dcccccdu) && lwc1(P + 0x164) < fs6c() * 2.5f;
            if (jump_anim && slow) {
                float blend = static_cast<float>(frame_scale_i(9)) / lwc1(P + 0xa90);
                if (static_cast<float>(frame_scale_i(0xa)) < blend) blend = static_cast<float>(frame_scale_i(0xa));
                hero_anim_set(7, static_cast<uint32_t>((float_to_int(lwc1(P + 0xaa8)) + 4) / 2), blend);
                sw(P + 0x2084, 7);
            }
        }
        if (float_to_int(lwc1(P + 0xaa8)) == float_to_int(lwc1(P + 0x444))) sw(P + 0xa90, 0);
    }
    if (lw(P + 0x418) != 0) {  // on the ground again
        if (lw(P + 0x2084) == 0xa) {
            if (hero_set_state(3, 0) == 0) return;
            hero_anim_set(5, 0, -2.0f);
            vec3_scale(V(P + 0x150), V(P + 0x150), u2f(0x3f4ccccdu));
            return;
        }
        if (lbu(P + 0x12ea) == 0 && lw(P + 0x1c4) == 0 && pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
            if (lh(P + 0x30e) < frame_scale_i(4)) {
                run_jump();
                return;
            }
        }
    }
    if (frame_scale_i(1) < lw(P + 0x418)) {
        if (lw(P + 0x2084) != 0xa) hero_land();
    } else if (lh(P + 0x41e) != 0 && !(lw(P + 0x198) < lh(P + 0x4aa)) && lwc1(P + 0x490) < lwc1(P + 0x2dc)) {
        set(6);
    }
    if (lw(P + 0x2084) == 0x10 && frame_scale_i(0x3c) < lw(P + 0x198) && 1.5f < lwc1(P + 0x2dc)) {
        set(6);
        return;
    }
    {  // the double jump
        const uint32_t st = lwu(P + 0x2084);
        const bool can = st == 7 || st == 9 || st == 0x11;
        if (lw(P + 0x1b4) != 0 && lw(P + 0x418) == 0 && can && (st != 0x11 || frame_scale_i(0x19) < lw(P + 0x198))) {
            int32_t ago = 0;
            if (pad_button_history(0x40, frame_scale_i(7), &ago) != 0 && ago < lw(P + 0x198)) {
                set(0x11);
                return;
            }
        }
    }
    if ((lw(P + 0x2084) == 0xa || lw(P + 0x2084) == 0x10) && frame_scale_i(0x19) < lw(P + 0x198) && lh(P + 0x30a) != 0) {
        sh(P + 0x4a6, static_cast<uint32_t>(frame_scale_i(0x14)));
    }
    int32_t hold = frame_scale_i(0x14);  // the slam needs the jump held this long
    if (lw(P + 0x2084) == 0x10) hold = frame_scale_i(8);
    if (lw(P + 0x2084) == 0xe && 4.0f < lwc1(P + 0x2dc)) {
        if (lh(P + 0x41e) == 0 && lwc1(P + 0xa90) < u2f(0x3fa66666u)) {
            approach_f(P + 0xa90, u2f(0x3fa66666u), lwc1(0x0015ed60) * u2f(0x3dcccccdu));
        }
        hold = frame_scale_i(0x11);
        sh(P + 0x4a6, static_cast<uint32_t>(hold));
    }
    int32_t back = lw(P + 0x198) - 4;
    if (!(1 < back)) back = 2;
    if (frame_scale_i(0x1e) < back) back = frame_scale_i(0x1e);
    if (lw(P + 0x418) == 0 && !st_in(lwu(P + 0x2084), {0x12, 0x1c, 0x11, 0xb, 0x3c, 0x69})) {
        const bool pressed_again = pad_button_history(0x40, back, nullptr) != 0;
        const uint32_t st = lwu(P + 0x2084);
        bool slam_check = true;
        if (pressed_again || st == 0xf || st == 0xd) {  // the glide 0xe
            const bool glide = st != 0xe && lbu(P + 0x20a8) == 0 && !(lbu(P + 0x12ea) != 0 && lwc1(P + 0x2dc) < u2f(0x3ecccccdu)) &&
                               lbu(P + 0x4af) != 0 && frame_scale_i(0xe) < lw(P + 0x198);
            if (glide) {
                if (lh(P + 0x41e) == 0) {
                    set(0xe);
                    slam_check = false;
                } else if (u2f(0x3f333333u) < lwc1(P + 0x2dc)) {  // high enough and not falling fast
                    if (abs_f(sub_00233a78_v(V(P + 0xe0))) < fs6c() * u2f(0x40b9999au)) {
                        set(0xe);
                        slam_check = false;
                    }
                }
            }
        } else if (st != 0xe) {
            slam_check = false;
        }
        const bool slam = lbu(0x0013d4c2) != 0 && sub_0020d498(3) != 4 && lh(P + 0x22d8) == 0 && (buttons() & 0x40) != 0;
        if (slam_check && slam && hold < lw(P + 0x198) && lh(P + 0x4a6) < lw(P + 0x198)) {  // the slam from high enough
            float high = u2f(0x3f19999au);
            if (lw(P + 0x2084) == 9 || lw(P + 0x2084) == 7) high = 1.0f;
            if (lw(P + 0x2084) == 0xe) high = 1.5f;
            if (high < lwc1(P + 0x2dc)) set(8);
        }
    }
    if (lw(P + 0x4e8) != 0 && lw(P + 0x2084) != 0xb) set(0x18);
}


// The fall below the kill height (state 0x77): lost once below the level's floor (level 3 under
// 5, level 6 under 50, level 16 under 77 or on the ground rising after 21 frames) or after 120
// frames; returns true then (the caller skips its tail).
// orig: 0x00229b70 (state 0x77, block 0x22e274)
bool kill_fall_input() {
    const uint32_t lvl = lwu(0x0015ed84);
    bool lost = false;
    if (lvl == 3 && lwc1(P + 0x88) < 5.0f) lost = true;
    if (lvl == 6 && lwc1(P + 0x88) < 50.0f) lost = true;
    if (lvl == 0x10) {
        if (lwc1(P + 0x88) < 77.0f) lost = true;
        if (!(lw(P + 0x198) < 0x15) && lbu(P + 0x257) != 0 && 0.0f <= lwc1(P + 0x108)) lost = true;
    }
    if (!lost && !(frame_scale_i(0x78) < lw(P + 0x198))) return false;
    hero_lost();
    return true;
}

// the shared end of states 8 / 0x81 back to standing: from the air animation 7 frame 0x17,
// else the stand set; then the landing camera nudge
// orig: 0x00229b70 (blocks 0x22e754 / 0x22e788)
void land_to_stand() {
    if (lh(P + 0x30e) != 0) {
        if (hero_set_state(0, 0) == 0) return;
        hero_anim_set(7, 0x17, static_cast<float>(frame_scale_i(9)));
    } else {
        if (hero_set_state(0, 0) == 0) return;
        hero_anim_set(sub_00205000(0), 0, -2.0f);
    }
    sub_00233288(5, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
}

// Crouching under a low ceiling (state 0x81): the slope slide 0x79; R1 kept with R1 pressed
// again in 30 frames keeps it; after 20 frames a jump press or R1 let go stands up.
// orig: 0x00229b70 (state 0x81, block 0x22e35c)
void low_crouch_input() {
    if (lbu(P + 0x12ea) != 0 && lwc1(P + 0x2dc) < 0.5f) {
        set(0x79);
        return;
    }
    constexpr OvConst<0x0015f678u> TIMER{};
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(TIMER)));
    bool held = false;
    constexpr uint64_t BOTH = 0x0000000800000008ull;  // R1 held and pressed
    if (lw(TIMER) != 0) held = (ld(PAD + 0x1a0) & BOTH) == BOTH;
    if ((pressed() & 8) != 0) sw(TIMER, static_cast<uint32_t>(frame_scale_i(0x1e)));
    if (!(frame_scale_i(0x14) < lw(P + 0x198))) return;
    if ((pressed() & 0x40) == 0 && !held) return;
    sb(P + 0x22ca, 0);
    sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x19)));
    land_to_stand();
}

// The slam's landing (state 8): the double jump 0x11 straight after a jump; the jump let go
// after 30 frames → falling 6; the ledge 0x18; on the ground: the slam from the flip 0x10 ends
// in the hard landing 3, R1 / L1 → crouch 4, at speed facing the stick → run 2 (the landing roll),
// faster → run 2, else standing.
// orig: 0x00229b70 (state 8, block 0x22e460)
void slam_land_input() {
    if (lw(P + 0x1b4) != 0 && lw(P + 0x198) < frame_scale_i(9)) {
        if (st_in(lwu(P + 0x2090), {7, 9, 0xe, 0x11})) set(0x11);
        return;
    }
    if (frame_scale_i(0x1e) < lw(P + 0x198) && (buttons() & 0x40) == 0 && (lwu(PAD + 0x1c0) & 0x40) == 0) {
        set(6);
        return;
    }
    if (lw(P + 0x4e8) != 0) {
        set(0x18);
        return;
    }
    if (lh(P + 0x30e) != 0 && !(lwc1(P + 0x88) - lwc1(P + 0x2d8) < u2f(0x3ecccccdu))) return;
    if (lw(P + 0x198) < frame_scale_i(0xa)) sw(P + 0x1d4, static_cast<uint32_t>(frame_scale_i(0xc) - lw(P + 0x198)));
    hero_stick_move(0, 1.0f);
    if (lw(P + 0x2090) == 0x10 && lw(P + 0x198) < frame_scale_i(4)) {
        if (hero_set_state(3, 0) == 0) return;
        hero_anim_set(5, 0, -1.0f);
        sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x22)));
        return;
    }
    if ((buttons() & 0xa) != 0) {
        set(4);
        sub_00233288(5, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
        return;
    }
    constexpr OvConst<0x0017bdb8u> BAND{};
    const bool fast = lwc1(BAND + 0xc) < lwc1(P + 0x190);
    if (fast) {
        const bool facing = angle_dist(lwc1(P + 0x180), lwc1(P + 0x98)) < u2f(0x3fc90fdbu);
        if (lbu(P + 0x12e2) != 0 || lbu(P + 0x20a9) != 0) {
            set(2);
            return;
        }
        hero_set_state(2, 0);
        if (lw(P + 0x2084) == 2) {
            sw(P + 0x2088, 1);
            if (facing) {
                hero_anim_set(4, 2, static_cast<float>(frame_scale_i(8)));
                sh(P + 0x3bc, 1);
            } else {
                hero_anim_set(4, lwu(OV(0x0015f6dc)), static_cast<float>(frame_scale_i(0xc)));
            }
        }
        sub_00233288(5, 2, 1, 0, u2f(0x3c83126fu), u2f(0x3b03126fu));
        return;
    }
    land_to_stand();
}


// Falling (states 6, 0x2d): the fall animation once falling for 18 frames or high; landing: the
// footsteps, the vertical speed capped, the hard landing (after 90 frames: the stand-up, the
// stun +0x1c4 without +0x2fc), the landing roll on the fall animation, the run 2 with the stick,
// the skid 3 at speed, else standing; in the air: the slam 8, the jump (with +0x1e2), the flip
// 0x10 off a wall, the ledge 0x18, a rail 0x28 / 0x74.
// orig: 0x00229b70 (states 6, 0x2d, block 0x22ddac)
void fall_input() {
    if (lw(P + 0x2088) == 0 && (!(lw(P + 0x198) < frame_scale_i(0x12)) || 1.75f < lwc1(P + 0x2dc))) {
        hero_anim_set(0xb, 0, static_cast<float>(frame_scale_i(0x10)));
        sw(P + 0x2088, 1);
        sw(P + 0x2284, lw(P + 0x2084) == 0x2d ? 7u : 0u);
    }
    if (lh(P + 0x30e) == 0) {  // landed
        hero_footstep(static_cast<int32_t>(lbu(P + 0x12ed)), 0, 1);
        hero_footstep(static_cast<int32_t>(lbu(P + 0x12ed)), 1, 1);
        if (lwc1(P + 0x108) < fs6c() * -9.0f) swc1(P + 0x108, fs6c() * -9.0f);
        if (lwc1(P + 0xe8) < fs6c() * -9.0f) swc1(P + 0xe8, fs6c() * -9.0f);
        if (!(0 < lw(P + 0x22a8))) {
            set(0x3d);
            return;
        }
        const bool stun = lw(P + 0x2fc) == 0;
        if (!(lw(P + 0x198) < frame_scale_i(0x5a))) {  // a long fall: the hard landing
            if (hero_set_state(0, 0) != 0) hero_anim_set(0xc, 4, static_cast<float>(frame_scale_i(6)));
            if (stun) sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x16)));
            sub_00233288(0xa, 4, 0, 1, u2f(0x3cb43958u), u2f(0x3b83126fu));
            return;
        }
        if (lw(P + 0x2088) == 1) {  // the fall animation was on: the landing roll
            if (hero_set_state(0, 0) != 0) hero_anim_set(0xc, 9, -1.0f);
            sub_00233b20_v(V(P + 0x150), V(P + 0x150), 0.0f);
            vec3_clamp_length(P + 0x150, fs6c() * 7.0f);
            sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(7)));
            if (lw(P + 0x2090) == 0x2d) sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0xa)));
            if (stun && frame_scale_i(0x32) < lw(P + 0x198)) sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x12)));
            sub_00233288(0xa, 2, 0, 1, u2f(0x3c9ba5e3u), u2f(0x3b03126fu));
            return;
        }
        if (0.5f < lwc1(P + 0x229c) && lw(P + 0x1c4) == 0) {
            set(2);
            sub_00233288(7, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
            return;
        }
        if (fs6c() * 3.0f < lwc1(P + 0x164)) {
            if (hero_set_state(3, 0) == 0) return;
            hero_anim_set(6, 0, static_cast<float>(frame_scale_i(0xc)));
            vec3_clamp_length(P + 0x150, fs6c() * u2f(0x40f66666u));
            return;
        }
        set(0);
        sub_00233288(5, 2, 0, 1, u2f(0x3c83126fu), u2f(0x3b03126fu));
        return;
    }
    if (!(0 < lw(P + 0x22a8))) return;
    const bool slam = (buttons() & 0x40) != 0 && lbu(0x0013d4c2) != 0 && sub_0020d498(3) != 4 && lh(P + 0x22d8) == 0;
    if (slam && lw(P + 0x1c4) == 0 && u2f(0x3f19999au) < lwc1(P + 0x2dc)) {
        set(8);
        return;
    }
    if (frame_scale_i(0x14) < lh(P + 0x1e2)) {  // still in the jump window
        const int32_t back = lw(P + 0x1d4) != 0 ? 1 : frame_scale_i(6);
        if (hero_try_jump(back, 0) != 0) return;
    }
    if (sub_0020d498(3) == 3 && lh(P + 0x22d8) == 0) {
        const int32_t a = frame_scale_i(9);
        const bool both = pad_buttons_together(0x40, 0xa, a, frame_scale_i(8)) != 0;
        if (both && (buttons() & 0xa) != 0 && (pressed() & 0x40) != 0 && (lh(P + 0x41e) == 0 || 1.5f < lwc1(P + 0x2dc))) {
            set(0x10);
            return;
        }
    }
    if (lw(P + 0x4e8) != 0) {
        set(0x18);
        return;
    }
    if (lw(P + 0x56c) != 0) {
        set(0x28);
        return;
    }
    if (lw(P + 0x5fc) != 0) set(0x74);
}


// The wrench swings (0x13 swing, 0x14 slam, 0x15 spin): the slam's animation pace to the
// ground; once the combo step's jump window (+0x14) opened, a jump press (with R1 / L1 the
// jump input); after the step's end (+0x18) standing; the spin with a non-wrench item ends at
// once, held long (100 frames) it plays the dizzy idle (animation set +0xab4 = 0x1a).
// orig: 0x00229b70 (states 0x13..0x15, block 0x22b168)
void swing_input() {
    if (lw(P + 0x2084) == 0x14) {  // the slam: pace the animation to land on frame 26
        const float k = lwc1(0x0015ed70);
        const int32_t n = float_to_int(hero_fall_time(60.0f, k * 130.0f, -1.0f));
        if (lwc1(P + 0xa70) == 0.0f) {
            swc1(P + 0xa90, u2f(0x3ea8f5c3u));
        } else if (lh(P + 0x30e) != 0 && n != 0 && 1.0f < 26.0f - lwc1(P + 0xaa8)) {
            sub_00208f20(26.0f, static_cast<float>(n), 0.5f, -1.0f);
            if (lwc1(P + 0x2dc) < u2f(0x3f333333u) && lwc1(P + 0xa90) < 0.75f) swc1(P + 0xa90, 0.75f);
        } else if (lh(P + 0x30e) == 0 && lwc1(P + 0xaa8) < 26.0f) {
            swc1(P + 0xa90, 2.0f);
        } else {
            swc1(P + 0xa90, 1.0f);
        }
    }
    const uint32_t step = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
    // enhancement: R&C 3's throw looks back 8 frames (at 60 Hz) for the jump press
    const int32_t back = advanced_rac3_swing_jump_back((float_to_int(lwc1(P + 0xaa8)) - lw(step + 0xc)) * 2);
    bool to_end = true;
    if (static_cast<float>(lw(step + 0x14)) < lwc1(P + 0xaa8)) {  // the jump window
        if (lw(P + 0xa9c) != 0) to_end = false;
        if (to_end && !(lw(P + 0x2084) == 0x15 && lh(P + 0x308) != 0) && pad_button_history(0x40, back, nullptr) != 0) {
            if ((buttons() & 0xa) == 0) {
                if (lh(P + 0x1f2) == 0) {
                    set(7);
                    return;
                }
            } else if (hero_jump_input() != 0) {
                return;
            }
        }
    }
    if (to_end && advanced_rac3_swing_exit(step)) return;  // enhancement: R&C 3 runs out of a swing / the throw
    if (to_end && lw(P + 0xa9c) == 0 && static_cast<float>(lw(step + 0x18)) < lwc1(P + 0xaa8)) {
        hero_set_state(0, 0);
        return;
    }
    if (lw(P + 0x2084) != 0x15) return;
    if (lw(P + 0x10b8) != 8) {
        set(0);
        return;
    }
    if (!(frame_scale_i(0x64) < lw(P + 0x198)) || !(lw(P + 0xab4) < 0x1a)) return;
    sub_00232640(2);
    sw(P + 0xab4, 0x1a);
}


// into the water from the surface: the hydro-pack's 0x35 with it and R1 / L1, else the swim 0x33
// orig: (helper) the shared tail of states 0x34 / 0x36 / 0x37
void dive() { set(lbu(0x0013d4c4) != 0 && (buttons() & 0xa) != 0 ? 0x35u : 0x33u); }

// Swimming (0x33 stroke, 0x35 hydro-pack): the stroke's animation speed from the speed; with no
// input after the stroke (frames 9..19) the drift 0x34; R1 / L1 with the hydro-pack → 0x35 (after
// 20 frames), off them → 0x33 (stick out); nothing pressed and the stick low → drift 0x34.
// orig: 0x00229b70 (states 0x33, 0x35, block 0x22a8a8)
void swim_input() {
    if (lw(P + 0x2084) == 0x33) {
        float r = lwc1(P + 0x194) * 15.0f;
        swc1(P + 0xa90, r);
        if (r < 0.25f) swc1(P + 0xa90, 0.25f);
        if (0.75f < lwc1(P + 0xa90)) swc1(P + 0xa90, 0.75f);
        const float slow = fs6c() + fs6c();
        const float t = lwc1(P + 0xaa8);
        const bool idle = (buttons() & 0xc0) == 0 && 9.0f < t && t < 19.0f && lw(P + 0x2088) != 1;
        if (idle && lwc1(P + 0x229c) < u2f(0x3dcccccdu) && lwc1(P + 0x194) < slow) {
            set(0x34);
            return;
        }
    }
    if (lbu(0x0013d4c4) != 0 && (buttons() & 0xa) != 0) {
        if (lw(P + 0x2084) == 0x33) {
            if (frame_scale_i(0x14) < lw(P + 0x198)) set(0x35);
            return;
        }
    } else {
        if (lw(P + 0x2084) != 0x35) return;
        if (0.5f < lwc1(P + 0x229c)) {
            set(0x33);
            return;
        }
    }
    if (lw(P + 0x2084) != 0x35) return;
    const uint32_t b = buttons();
    if ((b & 0xc0) != 0 || lw(P + 0x2088) == 1 || !(lwc1(P + 0x229c) < u2f(0x3f333333u)) || (b & 0xa) != 0) return;
    set(0x34);
}

// On the water's surface (0x37 still, 0x36 paddling): the jump out 0x12 (not with +0x1d8), the
// dive on square (not with +0x1de) or R1 / L1 with the hydro-pack, standing up in shallows
// (+0x20a9: 0 / 0x73), paddling with the stick, still when stopped.
// orig: 0x00229b70 (states 0x37 / 0x36, blocks 0x22ab60 / 0x22ace4)
void surface_input() {
    const bool still = lw(P + 0x2084) == 0x37;
    const bool jump = lh(P + 0x1d8) == 0 && pad_button_history(0x40, frame_scale_i(8), nullptr) != 0;
    if (jump && (!still || frame_scale_i(0xa) < lw(P + 0x198))) {
        set(0x12);
        return;
    }
    if (still) sb(P + 0x20a7, 1);
    if (lh(P + 0x1de) == 0) {
        bool go = pad_button_history(0x80, frame_scale_i(7), nullptr) != 0;
        if (still && !go && lbu(0x0013d4c4) != 0) go = pad_button_history(0xa, frame_scale_i(7), nullptr) != 0;
        if (go) {
            dive();
            return;
        }
    }
    if (still) {
        if (lbu(P + 0x20a9) != 0 && lwc1(P + 0x22a4) < u2f(0x3ecccccdu) && hero_set_state(0, 0) != 0) {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x12)));
        }
        if (0.2f < lwc1(P + 0x229c) && frame_scale_i(0xa) < lw(P + 0x198)) set(0x36);
        return;
    }
    if (lbu(P + 0x20a9) != 0 && lwc1(P + 0x22a4) < u2f(0x3f333333u)) {
        hero_set_state(0x73, 0);
        hero_anim_set(0x60, 0, static_cast<float>(frame_scale_i(0x11)));
        sw(P + 0x108, 0);
        sw(P + 0xe8, 0);
    }
    if (lwc1(P + 0x229c) < u2f(0x3dcccccdu) && lwc1(P + 0x194) == 0.0f && lw(P + 0x698) == 0) set(0x37);
}

// fallen below the floor by `drop` (+0x2f4: the last floor height) or past `frames` in the state:
// lost (block 0x22e334); true when lost
// orig: (helper) the tail of states 0x7f / 0x68 / 0x7b / 0x7c / 0x7d
bool lost_timer(float drop, int32_t frames) {
    if (lwc1(P + 0x88) < lwc1(P + 0x2f4) - drop || frame_scale_i(frames) < lw(P + 0x198)) {
        hero_lost();
        return true;
    }
    return false;
}

// Walking to a point (0x65; +0x1640 the point, +0x164c the facing there): the run gait until within
// 0.2, then the stop 0x67 when slow and facing it, else 0x66 (with +0x20a4) while still fast.
// orig: 0x00229b70 (state 0x65, block 0x22bd40)
void walk_to_input() {
    if (!(vec2_distance(V(P + 0x80), V(P + 0x1640)) < 0.2f)) {
        hero_run_gait_update();
        return;
    }
    const float k = fs6c() * u2f(0x402ccccdu);
    if (lwc1(P + 0x160) < k && angle_dist(lwc1(P + 0x98), lwc1(P + 0x164c)) < u2f(0x3d0efa35u)) {
        set(0x67);
        return;
    }
    if (lbu(P + 0x20a4) != 0 && fs6c() * u2f(0x402ccccdu) <= lwc1(P + 0x160)) set(0x66);
}

// The hover-board's ready stance (0x3f): the board's animation speed from the speed, R1 / L1 off
// it (4), the jump 0x71, falling 6, the board timers (+0x1f8; +0x1b0 when tilted past pi/4),
// riding off 2, the ready animation 0x5b, and back to standing after 30 frames with +0x1d4.
// orig: 0x00229b70 (state 0x3f, block 0x22bfcc)
void board_ready_input() {
    const float r = lwc1(P + 0x194) * 27.0f;
    swc1(P + 0xa90, r);
    if (r < 0.5f) swc1(P + 0xa90, 0.5f);
    if ((buttons() & 0xa) != 0) {
        set(4);
        return;
    }
    if (pad_button_history(0x40, frame_scale_i(7), nullptr) != 0) {
        set(0x71);
        return;
    }
    if (!(lh(P + 0x30e) < frame_scale_i(5)) && 0.8f < lwc1(P + 0x2dc)) {
        set(6);
        return;
    }
    sh(P + 0x1f8, static_cast<uint32_t>(frame_scale_i(0x28)));
    const float q = u2f(0x3f490fdbu);
    if (q < abs_f(lwc1(P + 0x94)) || q < abs_f(lwc1(P + 0x90))) sh(P + 0x1b0, static_cast<uint32_t>(frame_scale_i(0x28)));
    if (lw(P + 0x300) != 0 && lbu(P + 0x12e7) == 0) {
        set(2);
        return;
    }
    if (frame_scale_i(7) < lh(P + 0x30e) && 1.0f < lwc1(P + 0x2dc)) {
        set(2);
        return;
    }
    if (lbu(lwu(P + 0x2080) + 0x53) != 0x5b) hero_anim_set(0x5b, 0, static_cast<float>(frame_scale_i(8)));
    if (lw(PAD + 0x1d4) == 0 || lw(P + 0x198) < frame_scale_i(0x1e)) return;
    if (hero_set_state(0, 0) == 0) return;
    vec_clear(V(P + 0x150));
    const uint32_t a = sub_00205000(0);
    hero_anim_set(a, 0, static_cast<float>(frame_scale_i(0xf)));
}

// The fall-off speeds (+0x944 / +0x940) and +0x2284 cleared, on falling out of 0x25.
// orig: (helper) the tail of state 0x25
void drop_speeds(float k944, float k940) {
    const float g = lwc1(0x0015ed70);
    const float a = g * k944;
    sw(P + 0x2284, 0);
    const float b = g * k940;
    swc1(P + 0x944, a);
    swc1(P + 0x940, b);
}

// the speed +0x194 from +0x960, capped at 7 fs6c
// orig: (helper) shared by states 0x25 / 0x26
void speed_from_960() {
    const float cap = fs6c() * 7.0f;
    const float v = lwc1(P + 0x960);
    swc1(P + 0x194, v);
    if (cap < v) swc1(P + 0x194, cap);
}

// State 0x25 (holding on with the item's button +0x10a0): the animation pace by +0x96c, the 0xa
// animation between frames 22 and 30; on letting go (or slowing under 4 fs6c with +0x2088 1):
// standing when low (+0x2dc < 0.3), else falling 6 with the fall-off speeds.
// orig: 0x00229b70 (state 0x25, block 0x22c5c8)
void hold_input() {
    sub_00208f20(10.0f, lwc1(P + 0x96c), 0.5f, -1.0f);
    if (lwc1(P + 0x96c) < static_cast<float>(frame_scale_i(0x16)) && lwc1(P + 0x96c) < static_cast<float>(frame_scale_i(0x1e))) {
        hero_anim_set(0xa, 0, static_cast<float>(frame_scale_i(0x10)));
    }
    bool off = false;
    if ((buttons() & lwu(P + 0x10a0)) == 0) {
        if (static_cast<float>(frame_scale_i(0x19)) < lwc1(P + 0x96c)) sw(P + 0x988, 1);
        off = true;
    }
    if (lw(P + 0x2088) == 1 && lwc1(P + 0x960) < fs6c() * 4.0f) off = true;
    if (!off) return;
    speed_from_960();
    if (lwc1(P + 0x2dc) < u2f(0x3e99999au)) {
        hero_set_state(0, 0);
        const uint32_t a = sub_00205000(0);
        hero_anim_set(a, 0, static_cast<float>(frame_scale_i(0x1e)));
        return;
    }
    if (lbu(lwu(P + 0x2080) + 0x53) == 0xa) {
        hero_set_state(6, 0);
    } else {
        hero_set_state(6, 0);
        hero_anim_set(0xa, 0, static_cast<float>(frame_scale_i(0x11)));
    }
    drop_speeds(42.0f, 29.0f);
}

// State 0x22: the jump's 0x10 on a fresh X press after 12 frames (not from the jumps 8 / 6 with
// +0x209c 0x10, nor from 0xf / 0xa / 0x10 / 0x12 / 0x3c), the animation pace by the predicted fall
// time (+0x4bc), standing when the animation ends.
// orig: 0x00229b70 (state 0x22, block 0x22c8e8)
void air_flip_input() {
    if (lw(P + 0x198) < frame_scale_i(0xc)) {
        const uint32_t prev = lwu(P + 0x2090);
        bool block = lw(P + 0x209c) == 0x10 && (prev == 8 || prev == 6);
        for (uint32_t s : {0xfu, 0xau, 0x10u, 0x12u, 0x3cu}) block = block || prev == s;
        if (!block && (pressed() & 0x40) != 0) set(0x10);
    }
    const int32_t t = float_to_int(hero_fall_time(60.0f, lwc1(P + 0x4b0), -1.0f));
    sw(P + 0x4bc, static_cast<uint32_t>(t));
    if (lwc1(P + 0x4b8) == 0.0f || lh(P + 0x30c) == 0 || t == 0) {
        swc1(P + 0xa90, 1.0f);
    } else {
        if (t == 2) static_cast<void>(frame_scale_i(0xa));  // (the original calls it and drops the result)
        sub_00208f20(lwc1(P + 0x4c0), static_cast<float>(lw(P + 0x4bc)), 0.5f, -1.0f);
    }
    if ((lw(P + 0xa98) & 2) != 0) set(0);
}

// the hero's position stepped 0.3 to the side `side` (+-pi/2 from the facing)
// orig: (helper) the shimmy probe of states 0x19 / 0x1a / 0x1b
Vec4 side_probe(float side) {
    Vec4 p = V(P + 0x80);
    p.x = p.x + cos_vu(angle_add(lwc1(P + 0x98), side)) * 0.3f;
    p.y = p.y + sin_vu(angle_add(lwc1(P + 0x98), side)) * 0.3f;
    return p;
}

// Hanging from a ledge (0x19; `wet`: 0x4b of the 0x43 set): the hang animation 0x21 after the
// grab's 0x20 (not wet); falling (6 / 0x45) when the ledge is gone or on R1 / L1 (pressed; held
// when wet) / jumping away (more than 3pi/4 off the facing); the climb (0x1c / 0x4c) on X; the
// shimmies (0x1a / 0x1b, 0x4d / 0x4e) with the stick within pi/4 of the sides and a ledge there.
// orig: 0x00229b70 (states 0x19 / 0x4b, blocks 0x22d8ac / 0x2306a4)
void hang_input(bool wet) {
    if (!wet && (lw(P + 0xa98) & 2) != 0 && lbu(lwu(P + 0x2080) + 0x53) == 0x20) {
        hero_anim_set(0x21, 0, static_cast<float>(frame_scale_i(6)));
    }
    const uint32_t fall = wet ? 0x45u : 6u;
    hero_stick_move(0, 1.0f);
    const Vec4 at = V(P + 0x80);
    const float yaw = lwc1(P + 0x98);
    if (hero_ledge_check_v(at, &yaw) == 0) {
        set(fall);
        sh(P + 0x1b0, static_cast<uint32_t>(frame_scale_i(0xa)));
        return;
    }
    bool away = false;
    if (0.5f < lwc1(P + 0x229c) && pad_button_history(0x40, frame_scale_i(9), nullptr) != 0) {
        away = u2f(0x4016cbe4u) < angle_dist(lwc1(P + 0x180), lwc1(P + 0x98));
    }
    if (away || ((wet ? buttons() : pressed()) & 0xa) != 0) {
        set(fall);
        sh(P + 0x1b0, static_cast<uint32_t>(frame_scale_i(0xa)));
        return;
    }
    if (pad_button_history(0x40, frame_scale_i(wet ? 9 : 0x11), nullptr) != 0) {
        set(wet ? 0x4cu : 0x1cu);
        return;
    }
    if (!(0.5f < lwc1(P + 0x229c))) return;
    const float q = u2f(0x3f490fdbu);
    if (angle_dist(lwc1(P + 0x180), angle_add(lwc1(P + 0x98), u2f(0x3fc90fdbu))) < q) {
        const Vec4 p = side_probe(u2f(0x3fc90fdbu));
        const float y = lwc1(P + 0x98);
        if (hero_ledge_check_v(p, &y) != 0) set(wet ? 0x4du : 0x1au);
        return;
    }
    if (!(0.5f < lwc1(P + 0x229c))) return;
    if (!(angle_dist(lwc1(P + 0x180), angle_add(lwc1(P + 0x98), u2f(0xbfc90fdbu))) < q)) return;
    const Vec4 p = side_probe(u2f(0xbfc90fdbu));
    const float y = lwc1(P + 0x98);
    if (hero_ledge_check_v(p, &y) != 0) set(wet ? 0x4eu : 0x1bu);
}

// Shimmying along a ledge (0x1a right, 0x1b left; `wet`: 0x4d / 0x4e): back to hanging (0x19 /
// 0x4b) at the animation's end unless the stick still points that way with a ledge there; jumping
// away (6 / 0x45); the climb (0x1c / 0x4c).
// orig: 0x00229b70 (states 0x1a / 0x1b / 0x4d / 0x4e, blocks 0x22dbb0 / 0x230980)
void shimmy_input(bool wet) {
    const float side = lw(P + 0x2084) == (wet ? 0x4e : 0x1b) ? u2f(0xbfc90fdbu) : u2f(0x3fc90fdbu);
    const Vec4 p = side_probe(side);
    const float yaw = lwc1(P + 0x98);
    hero_stick_move(0, 1.0f);
    bool more = false;
    if (!(lwc1(P + 0x229c) < 0.5f)) {
        const float d = angle_dist(lwc1(P + 0x180), angle_add(lwc1(P + 0x98), side));
        if (!(u2f(0x3f490fdbu) < d)) more = hero_ledge_check_v(p, &yaw) != 0;
    }
    if (!more && (lw(P + 0xa98) & 2) != 0) {
        set(wet ? 0x4bu : 0x19u);
        return;
    }
    bool away = false;
    if (0.5f < lwc1(P + 0x229c) && pad_button_history(0x40, frame_scale_i(9), nullptr) != 0) {
        away = u2f(0x4016cbe4u) < angle_dist(lwc1(P + 0x180), lwc1(P + 0x98));
    }
    if (away) {
        set(wet ? 0x45u : 6u);
        sh(P + 0x1b0, static_cast<uint32_t>(frame_scale_i(0x28)));
        return;
    }
    if (pad_button_history(0x40, frame_scale_i(9), nullptr) != 0) set(wet ? 0x4cu : 0x1cu);
}

// State 0x16: falling 6 after 12.5 frames with +0x30e and high (+0x2dc > 1); on level 0x10 dropping
// with no air left (+0x22a8) after state 0xf: the kill fall 0x77; after 27 frames: with air back
// to standing (or 0x81 from it), without: 0x7c (+0x12e5) or the death 0x3d.
// orig: 0x00229b70 (state 0x16, block 0x22f858)
void state16_input() {
    if (12.5f < lwc1(P + 0xaa8) && lh(P + 0x30e) != 0 && 1.0f < lwc1(P + 0x2dc)) {
        set(6);
        return;
    }
    if (lw(0x0015ed84) == 0x10 && lwc1(P + 0xe8) < 0.0f && lw(P + 0x22a8) == 0 && lw(P + 0x2094) == 0xf) {
        set(0x77);
        return;
    }
    if (!(frame_scale_i(0x1b) < lw(P + 0x198))) return;
    if (0 < lw(P + 0x22a8)) {
        if (lw(P + 0x2090) == 0x81) {
            set(0x81);
        } else {
            hero_set_state(0, 0);
        }
        return;
    }
    set(lbu(P + 0x12e5) != 0 ? 0x7cu : 0x3du);
}

// State 0x43 (the stand of the 0x43..0x52 set): the 0x12 animation while +0x22fc runs out, the
// idle animations (0, or 1 at random), 0x52 with +0x12ea, 1 on L1 / L2, falling 0x45, the jump
// 0x49, the attack 0x51 (square), moving 0x44; back to animation 0 after 0x12.
// orig: 0x00229b70 (state 0x43, block 0x22f93c)
void state43_input() {
    const int32_t t = lh(P + 0x22fc);
    const int32_t a = frame_scale_i(0x39);
    if (a - frame_scale_i(0xf) < t) {
        hero_anim_set(0x12, 0, static_cast<float>(frame_scale_i(0xa)));
        return;
    }
    if (lh(P + 0x22fc) == 0 && (lw(P + 0xa98) & 2) != 0) {
        if (lbu(lwu(P + 0x2080) + 0x53) != 0) {
            hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0xc)));
        } else if (rand_range_f(0.0f, 1.0f) < 0.5f) {
            hero_anim_set(1, 0, static_cast<float>(frame_scale_i(8)));
        }
    }
    if (lbu(P + 0x12ea) != 0) {
        set(0x52);
        return;
    }
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    if (lh(P + 0x30e) != 0) {
        set(0x45);
        return;
    }
    if (pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
        set(0x49);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(9), nullptr) != 0) {
        set(0x51);
        return;
    }
    hero_stick_move(0, 1.0f);
    if (u2f(0x3e4ccccdu) < lwc1(P + 0x229c) && 0.0f < lwc1(P + 0x190)) {
        set(0x44);
        return;
    }
    if (lh(P + 0x22fc) != 0 || lbu(lwu(P + 0x2080) + 0x53) != 0x12) return;
    hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0xc)));
}

// State 0x51 (the attack of the 0x43 set, paced by the combo table 0x17bc28 step +0xa60): 0x52 with
// +0x12ea; past the step's end (+0x18) back to 0x43; in its chain window (+0x10) square again
// within the frames since its start (+0xc, 1..15) → 0x51; past +0x14 the jump 0x49 on X within
// twice those frames.
// orig: 0x00229b70 (state 0x51, block 0x22fb10)
void state51_input() {
    if (lbu(P + 0x12ea) != 0) {
        set(0x52);
        return;
    }
    const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
    if (lw(P + 0xa9c) == 0) {
        if (static_cast<float>(lw(e + 0x18)) < lwc1(P + 0xaa8)) hero_set_state(0x43, 0);
        const int32_t s2 = lw(P + 0xa9c);
        if (s2 == 0 && static_cast<float>(lw(e + 0x10)) <= lwc1(P + 0xaa8)) {
            int32_t n = float_to_int(lwc1(P + 0xaa8)) - lw(e + 0xc);
            if (!(s2 < n)) n = 1;
            if (frame_scale_i(0xf) < n) n = frame_scale_i(0xf);
            if (pad_button_history(0x80, n, nullptr) != 0) set(0x51);
        }
    }
    const int32_t f = float_to_int(lwc1(P + 0xaa8));
    if (!(static_cast<float>(lw(e + 0x14)) < lwc1(P + 0xaa8))) return;
    if (lw(P + 0xa9c) != 0) return;
    if (pad_button_history(0x40, (f - lw(e + 0xc)) * 2, nullptr) != 0) set(0x49);
}

// The jump of the 0x43 set (0x49, 0x4c): the arc's animation pacing (+0x420 the take-off frame,
// +0x3d0..+0x3e0 the apex, +0x440 / +0x444 the arc's / landing's animation lengths), touching
// down (+0x418) with the landing animation, the landing pacing by the predicted fall time
// (+0x44c); on the ground the next jump 0x49 on X, moving 0x44 or standing 0x43; falling 0x45
// (high for 40 frames), the attack 0x51 low, the ledge 0x4a (+0x4e8), the 0x4f on X high.
// orig: 0x00229b70 (states 0x49 / 0x4c, block 0x22fc3c)
void jump43_input(uint32_t hm0) {
    if (!(lw(P + 0x420) < lw(P + 0x198))) return;
    if (lw(P + 0x198) == lw(P + 0x420) + frame_scale_i(1)) {  // the frame after take-off
        int32_t frames = 0;
        sub_00233c30(P + 0x80, P + 0xe0, &frames, lwc1(P + 0x4a0));
        sub_00208f20(lwc1(P + 0x440), static_cast<float>(frames), 0.5f, -1.0f);
        return;
    }
    if (lh(P + 0x41c) != 0 && !(lw(P + 0x198) < lw(P + 0x3d0)) && lw(P + 0x198) < lw(P + 0x3d4)) {  // the apex
        const float n = static_cast<float>(lw(P + 0x3e0) - lw(P + 0x3d0));
        if (0.0f < n) sub_00208f20(lwc1(P + 0x440), n, 0.5f, -1.0f);
        return;
    }
    if (lw(P + 0x418) != 0) {
        sw(P + 0x418, lwu(P + 0x418) + 1);
    } else if (lh(P + 0x30e) == 0 && lh(P + 0x41e) != 0) {  // touching down
        sw(P + 0x418, static_cast<uint32_t>(frame_scale_i(1)));
        const int32_t f = float_to_int(lwc1(P + 0x444)) + 3;
        if (lwc1(P + 0xaa8) < lwc1(P + 0x444)) {
            const uint32_t a = lbu(hm0 + 0x53);
            hero_anim_set(a, static_cast<uint32_t>(f), static_cast<float>(frame_scale_i(6)));
        }
    }
    if (lw(P + 0x418) != 0) {
        swc1(P + 0xa90, 1.0f);
    } else if (lh(P + 0x41e) != 0) {  // falling towards the ground: pace the animation to the landing
        if (static_cast<int32_t>(lbu(hm0 + 0x51)) < float_to_int(lwc1(P + 0x444))) {
            const int32_t n = float_to_int(hero_fall_time(60.0f, lwc1(P + 0x4a0), -1.0f));
            sw(P + 0x44c, static_cast<uint32_t>(n));
            sub_00208f20(lwc1(P + 0x444), static_cast<float>(n), 0.5f, -1.0f);
            const int32_t f = float_to_int(lwc1(P + 0x444)) + 1;
            if (u2f(0x3fe66666u) < lwc1(P + 0xa90)) hero_anim_set(lbu(hm0 + 0x53), static_cast<uint32_t>(f), static_cast<float>(lw(P + 0x44c) + 2));
            if (lwc1(P + 0xa90) < 0.2f) swc1(P + 0xa90, 0.2f);
            if (u2f(0x402ccccdu) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x402ccccdu));
        }
        if (float_to_int(lwc1(P + 0xaa8)) == float_to_int(lwc1(P + 0x444))) sw(P + 0xa90, 0);
    }
    const bool again = lw(P + 0x418) != 0 && lw(P + 0x1c4) == 0 && pad_button_history(0x40, frame_scale_i(4), nullptr) != 0;
    if (again && lh(P + 0x30e) < frame_scale_i(4)) {  // the next jump straight away
        hero_stick_move(0, 1.0f);
        set(0x49);
        return;
    }
    if (frame_scale_i(1) < lw(P + 0x418)) {  // landed: moving on or standing
        sw(P + 0x2284, 0);
        hero_stick_move(0, 1.0f);
        bool move = lwc1(OV(0x0017bdc4)) < lwc1(P + 0x190) && 0.5f < lwc1(P + 0x229c) && lbu(P + 0x12ea) == 0 && lw(P + 0x1c4) == 0;
        move = move && angle_dist(lwc1(P + 0x180), lwc1(P + 0x98)) < u2f(0x3fc90fdbu);
        if (move) {
            set(0x44);
        } else if (frame_scale_i(0xc) < lw(P + 0x418)) {
            set(0x43);
        }
    }
    if (2.0f < lwc1(P + 0x2dc) && lh(P + 0x41e) != 0 && frame_scale_i(0x28) < lw(P + 0x198)) {
        set(0x45);
        return;
    }
    if (lh(P + 0x41e) != 0 && lwc1(P + 0x2dc) < u2f(0x3e4ccccdu) && pad_button_history(0x80, frame_scale_i(5), nullptr) != 0) {
        set(0x51);
        return;
    }
    if (lw(P + 0x4e8) != 0) {
        set(0x4a);
        return;
    }
    if (lw(P + 0x2084) == 0x4c) return;
    if (pad_button_history(0x40, frame_scale_i(5), nullptr) != 0 && u2f(0x3f0ccccdu) < lwc1(P + 0x2dc)) set(0x4f);
}

// the frame in animation `to` matching the current frame of the other gait animation `from`
// (the frame counts at +0x10 of the hero moby's sets), plus `add`, wrapped to `to`'s length
// orig: (helper) the gait switch of state 0x44
uint32_t gait_phase(uint32_t hm, uint32_t to, uint32_t from, int32_t add) {
    const uint32_t sets = lwu(hm + 0x24);
    const int32_t n_to = static_cast<int32_t>(lbu(lwu(sets + 0x48 + 4 * to) + 0x10));
    const int32_t n_from = static_cast<int32_t>(lbu(lwu(sets + 0x48 + 4 * from) + 0x10));
    const int32_t v = static_cast<int32_t>(lbu(hm + 0x51)) * n_to / n_from + add;
    if (n_to == 0) return static_cast<uint32_t>(v);  // (the EE's divide by zero leaves HI = the dividend)
    return static_cast<uint32_t>(v % n_to);
}

// The walk / run of the 0x43 set (0x44; 0x50 its other gait): animation 2 below 1.5 fs6c, 3 above
// 1.8 fs6c, entered at the matching phase; the animation speed from the speed +0x160; the jump
// 0x49, the attack 0x51, falling 0x45 (off the ground and high or steep), standing 0x43 with the
// stick released.
// orig: 0x00229b70 (states 0x44 / 0x50, block 0x230348)
void walk43_input() {
    if (lbu(P + 0x12ea) != 0) {
        set(0x52);
        return;
    }
    if (lw(P + 0x2084) != 0x44) {
        swc1(P + 0xa90, lwc1(P + 0x160) * 196.0f);
    } else {
        if (lw(P + 0xa9c) == 0) {
            const float f = fs6c();
            const uint32_t hm = lwu(P + 0x2080);
            if (lwc1(P + 0x160) < f * 3.0f * 0.5f && lbu(hm + 0x53) != 2) {
                hero_anim_set(2, gait_phase(hm, 2, 3, 1), static_cast<float>(frame_scale_i(8)));
            } else if (lw(P + 0xa9c) == 0 && f * 3.0f * u2f(0x3f19999au) < lwc1(P + 0x160) && lbu(lwu(P + 0x2080) + 0x53) != 3) {
                const uint32_t h = lwu(P + 0x2080);
                hero_anim_set(3, gait_phase(h, 3, 2, 7), static_cast<float>(frame_scale_i(8)));
            }
        }
        const uint32_t a = lbu(lwu(P + 0x2080) + 0x53);
        if (a == 2) {
            swc1(P + 0xa90, lwc1(P + 0x160) * 90.0f);
        } else if (a == 3) {
            swc1(P + 0xa90, lwc1(P + 0x160) * 20.0f);
        }
    }
    if (pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
        set(0x49);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(9), nullptr) != 0) {
        set(0x51);
        return;
    }
    if (!(lh(P + 0x30e) < frame_scale_i(5)) && (0.8f < lwc1(P + 0x2dc) || u2f(0x3f5f66f3u) < lwc1(P + 0x2e0))) {
        set(0x45);
        return;
    }
    if (lh(P + 0x30e) != 0) return;
    if (lwc1(P + 0x229c) < u2f(0x3e2e147bu)) set(0x43);
}

// The fall of the 0x43 set (0x45): lost below 115 on level 6; landing: the speeds capped at -9
// fs6c, then moving 0x44 / standing 0x43 with air (+0x22a8), else the death 0x47; in the air with
// air: the ledge 0x4a, the 0x4f on X high.
// orig: 0x00229b70 (state 0x45, block 0x2301d8)
bool fall43_input() {
    if (lw(0x0015ed84) == 6 && lwc1(P + 0x88) < 115.0f) {
        hero_lost();
        return true;
    }
    if (lh(P + 0x30e) == 0) {
        const float k = fs6c() * -9.0f;
        if (lwc1(P + 0x108) < k) swc1(P + 0x108, k);
        const float k2 = fs6c() * -9.0f;
        if (lwc1(P + 0xe8) < k2) swc1(P + 0xe8, k2);
        if (!(0 < lw(P + 0x22a8))) {
            set(0x47);
        } else {
            set(0.5f < lwc1(P + 0x229c) && lw(P + 0x1c4) == 0 ? 0x44u : 0x43u);
        }
        return false;
    }
    if (!(0 < lw(P + 0x22a8))) return false;
    if (lw(P + 0x4e8) != 0) {
        set(0x4a);
        return false;
    }
    if (pad_button_history(0x40, frame_scale_i(5), nullptr) != 0 && u2f(0x3f19999au) < lwc1(P + 0x2dc)) set(0x4f);
    return false;
}

// State 0x4f: falling 0x45 after 30 frames with X released; the ledge 0x4a; on landing (or below
// the ground) +0x1d4 the frames left of the first 12, moving 0x44 with the stick, else standing.
// orig: 0x00229b70 (state 0x4f, block 0x230104)
void state4f_input() {
    if (frame_scale_i(0x1e) < lw(P + 0x198) && (buttons() & 0x40) == 0) {
        set(0x45);
        return;
    }
    if (lw(P + 0x4e8) != 0) {
        set(0x4a);
        return;
    }
    if (lh(P + 0x30e) != 0 && !(lwc1(P + 0x88) - lwc1(P + 0x2d8) < 0.0f)) return;
    if (lw(P + 0x198) < frame_scale_i(0xa)) sw(P + 0x1d4, static_cast<uint32_t>(frame_scale_i(0xc) - lw(P + 0x198)));
    if (u2f(0x3e99999au) < lwc1(P + 0x229c)) {
        set(0x44);
        return;
    }
    if (lh(P + 0x30e) == 0) set(0x43);
}

// State 0x5a (the stand of the 0x5a..0x61 set): its idle animations (0; after 50 frames 1 or 2 at
// random), 1 on L1 / L2, the jump 0x5e (X), 0x60 (square), 0x61 (triangle, without +0x1636), 0x5f
// (circle), falling 0x5c (high), moving 0x5b.
// orig: 0x00229b70 (state 0x5a, block 0x230b54)
void state5a_input() {
    if ((lw(P + 0xa98) & 2) != 0) {
        if (lbu(lwu(P + 0x2080) + 0x53) != 0) {
            hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0xc)));
        } else if (frame_scale_i(0x32) < lw(P + 0x198)) {
            const float r = rand_range_f(0.0f, 1.0f);
            if (r < u2f(0x3dcccccdu)) {
                hero_anim_set(1, 0, static_cast<float>(frame_scale_i(8)));
            } else if (r < 0.5f) {
                hero_anim_set(2, 0, static_cast<float>(frame_scale_i(8)));
            }
        }
    }
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    if (pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
        set(0x5e);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(0xf), nullptr) != 0) {
        set(0x60);
        return;
    }
    if (pad_button_history(0x10, frame_scale_i(8), nullptr) != 0 && lh(P + 0x1636) == 0) {
        set(0x61);
        return;
    }
    if (pad_button_history(0x20, frame_scale_i(8), nullptr) != 0) {
        set(0x5f);
        return;
    }
    if (lh(P + 0x30e) != 0 && 2.0f < lwc1(P + 0x2dc)) {
        set(0x5c);
        return;
    }
    hero_stick_move(0, 1.0f);
    if (u2f(0x3e6147aeu) < lwc1(P + 0x229c) && 0.0f < lwc1(P + 0x190)) set(0x5b);
}

// The jump of the 0x5a set (0x5e): pushing mobys below while falling (+0x41e); the arc's animation
// pacing; touching down (+0x418) with a camera shake (0x166de0 / 0x166de8) and the landing
// animation; the landing pacing by the predicted fall time; on the first ground frame a light
// shaft (moby 0x593) at the feet; the next jump on X, moving 0x5b or standing 0x5a.
// orig: 0x00229b70 (state 0x5e, block 0x230d70)
void jump5a_input(uint32_t hm0) {
    if (lh(P + 0x41e) != 0) {
        Vec4 v = V(P + 0x80);
        v.z = v.z - 1.0f;
        moby_sphere_contact_v(lwu(P + 0x2080), v, 0x30000, 0, 1, 0, 3.5f, 40.0f, 1.0f);
    }
    if (!(lw(P + 0x420) < lw(P + 0x198))) return;
    if (lw(P + 0x198) == lw(P + 0x420) + frame_scale_i(1)) {  // the frame after take-off
        int32_t frames = 0;
        sub_00233c30(P + 0x80, P + 0xe0, &frames, lwc1(P + 0x4a0));
        sub_00208f20(lwc1(P + 0x440), static_cast<float>(frames), lwc1(P + 0x494), -1.0f);
        return;
    }
    if (lw(P + 0x418) != 0) {
        sw(P + 0x418, lwu(P + 0x418) + 1);
    } else if (lh(P + 0x30e) == 0 && lh(P + 0x41e) != 0) {  // touching down
        sw(P + 0x418, static_cast<uint32_t>(frame_scale_i(1)));
        swc1(OV(0x00166de0), u2f(0x3e19999au));
        sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x1e)));
        const int32_t f = float_to_int(lwc1(P + 0x448)) + 3;
        if (lwc1(P + 0xaa8) < lwc1(P + 0x444)) {
            const uint32_t a = lbu(hm0 + 0x53);
            hero_anim_set(a, static_cast<uint32_t>(f), static_cast<float>(frame_scale_i(6)));
        }
    }
    if (lw(P + 0x418) != 0) {
        swc1(P + 0xa90, 1.0f);
    } else if (lh(P + 0x41e) != 0) {  // falling towards the ground: pace the animation to the landing
        if (static_cast<int32_t>(lbu(hm0 + 0x51)) < float_to_int(lwc1(P + 0x444))) {
            const int32_t n = float_to_int(hero_fall_time(60.0f, lwc1(P + 0x4a0), -1.0f));
            sw(P + 0x44c, static_cast<uint32_t>(n));
            sub_00208f20(lwc1(P + 0x444), static_cast<float>(n), lwc1(P + 0x494), -1.0f);
            const int32_t f = float_to_int(lwc1(P + 0x448)) + 1;
            if (u2f(0x3fe66666u) < lwc1(P + 0xa90)) hero_anim_set(lbu(hm0 + 0x53), static_cast<uint32_t>(f), static_cast<float>(lw(P + 0x44c) + 2));
            if (lwc1(P + 0xa90) < 0.2f) swc1(P + 0xa90, 0.2f);
            if (u2f(0x402ccccdu) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x402ccccdu));
        }
        if (float_to_int(lwc1(P + 0xaa8)) == float_to_int(lwc1(P + 0x444))) sw(P + 0xa90, 0);
    }
    if (lw(P + 0x418) == 1) {  // the first frame on the ground: the light shaft at the feet
        Vec4 v = V(P + 0x80);
        v.z = lwc1(P + 0x2d8) + 0.2f;
        const uint32_t n = static_cast<uint32_t>(frame_scale_i(0x19));
        spawn_moby_593_v(lwu(P + 0x2080), v, n, 2.0f, fs6c() * 15.0f, 40.0f);
    }
    const bool again = lw(P + 0x418) != 0 && lw(P + 0x1c4) == 0 && pad_button_history(0x40, frame_scale_i(4), nullptr) != 0;
    if (again && lh(P + 0x30e) < frame_scale_i(4)) {  // the next jump straight away
        hero_stick_move(0, 1.0f);
        set(0x5e);
        return;
    }
    if (!(frame_scale_i(1) < lw(P + 0x418))) return;
    sw(P + 0x2284, 0);
    hero_stick_move(0, 1.0f);
    bool move = lwc1(OV(0x0017bdc4)) < lwc1(P + 0x190) && 0.5f < lwc1(P + 0x229c) && lw(P + 0x1c4) == 0;
    move = move && angle_dist(lwc1(P + 0x180), lwc1(P + 0x98)) < u2f(0x3fc90fdbu);
    if (move) {
        set(0x5b);
        return;
    }
    if (frame_scale_i(0xc) < lw(P + 0x418)) set(0x5a);
}

// The walk of the 0x5a set (0x5b): the animation speed from the speed (at least 0.75); the jump
// 0x5e, 0x60, 0x61, 0x5f on their buttons; falling 0x5c (high); standing 0x5a 20 frames after the
// stick is released.
// orig: 0x00229b70 (state 0x5b, block 0x2313b4)
void walk5a_input() {
    const float r = lwc1(P + 0x160) * 8.0f;
    swc1(P + 0xa90, r);
    if (r < 0.75f) swc1(P + 0xa90, 0.75f);
    if (pad_button_history(0x40, frame_scale_i(8), nullptr) != 0) {
        set(0x5e);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(8), nullptr) != 0) {
        set(0x60);
        return;
    }
    if (pad_button_history(0x10, frame_scale_i(8), nullptr) != 0 && lh(P + 0x1636) == 0) {
        set(0x61);
        return;
    }
    if (pad_button_history(0x20, frame_scale_i(8), nullptr) != 0) {
        set(0x5f);
        return;
    }
    if (!(lh(P + 0x30e) < frame_scale_i(5)) && 1.5f < lwc1(P + 0x2dc)) {  // (the original's +0x2e0 test after this is unreachable)
        set(0x5c);
        return;
    }
    if (lh(P + 0x30e) != 0 || !(lwc1(P + 0x229c) < u2f(0x3e2e147bu))) return;
    if (frame_scale_i(0x14) < lw(P + 0x198)) set(0x5a);
}

// square in the 0x53 set: the swing (sound 0x19, +0x22dc / +0x22de) unless one is running, then the
// item slot update
// orig: (helper) shared by states 0x53 / 0x54 (block 0x23192c)
void swing53() {
    if (lh(P + 0x22de) == 0) {
        hero_sound_start(0x19, 0);
        sh(P + 0x22dc, 1);
        sh(P + 0x22de, 0x12);
    }
    hero_slot_update(0);
}

// State 0x53 (the stand of the 0x53..0x59 set): its idle animations (0, or 5 at random), 1 on L1 /
// L2, falling 0x55, 0x59 on circle, the swing on square, moving 0x54.
// orig: 0x00229b70 (state 0x53, block 0x2315b0)
void state53_input() {
    if ((lw(P + 0xa98) & 2) != 0) {
        if (lbu(lwu(P + 0x2080) + 0x53) != 0) {
            hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0xc)));
        } else if (rand_range_f(0.0f, 1.0f) < 0.5f) {
            hero_anim_set(5, 0, static_cast<float>(frame_scale_i(8)));
        }
    }
    if ((buttons() & 5) != 0) {
        set(1);
        return;
    }
    if (lh(P + 0x30e) != 0) {
        set(0x55);
        return;
    }
    if ((pressed() & 0x20) != 0) {
        set(0x59);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(7), nullptr) != 0) {
        swing53();
        return;
    }
    hero_stick_move(0, 1.0f);
    if (u2f(0x3e6147aeu) < lwc1(P + 0x229c) && 0.0f < lwc1(P + 0x190)) set(0x54);
}

// The walk of the 0x53 set (0x54): the animation speed from the speed; falling 0x55 (off the ground,
// high or steep); the swing on square; 0x59 on circle; standing 0x53 with the stick released.
// orig: 0x00229b70 (state 0x54, block 0x231894)
void walk53_input() {
    const int32_t air = lh(P + 0x30e);
    swc1(P + 0xa90, lwc1(P + 0x160) * 54.0f);
    if (!(air < frame_scale_i(5)) && (0.8f < lwc1(P + 0x2dc) || u2f(0x3f5f66f3u) < lwc1(P + 0x2e0))) {
        set(0x55);
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(7), nullptr) != 0) {
        swing53();
        return;
    }
    if ((pressed() & 0x20) != 0) {
        set(0x59);
        return;
    }
    if (lh(P + 0x30e) != 0) return;
    if (lwc1(P + 0x229c) < u2f(0x3e2e147bu)) set(0x53);
}

// The fall of the 0x53 set (0x55), on landing: the speeds capped at -9 fs6c; without air (+0x22a8)
// the death 0x57; moving on 0x54 within 40 frames; else standing 0x53 in the landing animation 7
// (speed 0.35) with +0x1c4 the 20-frame lock.
// orig: 0x00229b70 (state 0x55, block 0x2317a0)
void fall53_input() {
    if (lh(P + 0x30e) != 0) return;
    if (lwc1(P + 0x108) < fs6c() * -9.0f) swc1(P + 0x108, fs6c() * -9.0f);
    if (lwc1(P + 0xe8) < fs6c() * -9.0f) swc1(P + 0xe8, fs6c() * -9.0f);
    if (!(0 < lw(P + 0x22a8))) {
        set(0x57);
        return;
    }
    if (0.5f < lwc1(P + 0x229c) && lw(P + 0x198) < frame_scale_i(0x28) && lw(P + 0x1c4) == 0) {
        set(0x54);
        return;
    }
    hero_set_state(0x53, 0);
    hero_anim_set(7, 0, -1.0f);
    swc1(P + 0xa90, u2f(0x3eb33333u));
    sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x14)));
}

// State 0x1e (letting go of L1 / L2 from the ready stance 1): the strafe animation 0xd on R1 / L1,
// else the item's stand set (not on the hover-pack +0x20a4, nor with +0x20aa and without +0x20a5);
// once L1 / L2 are released, standing again in the item's stand set; 0x16c780 cleared.
// orig: 0x00229b70 (state 0x1e, block 0x231a38)
void unready_input() {
    if (lbu(P + 0x20a4) == 0 && (lbu(P + 0x20aa) == 0 || lbu(P + 0x20a5) != 0)) {
        if ((buttons() & 0xa) != 0) {
            if (lbu(lwu(P + 0x2080) + 0x53) != 0xd) hero_anim_set(0xd, 0, static_cast<float>(frame_scale_i(8)));
        } else {
            const uint32_t a = sub_00205000(1);
            if (lbu(lwu(P + 0x2080) + 0x53) != a) hero_anim_set(a, 0, static_cast<float>(frame_scale_i(8)));
        }
    }
    if ((buttons() & 5) != 0) return;
    if (lw(P + 0x2084) == 0x1e) {
        hero_set_state(0, 0);
        const uint32_t a = sub_00205000(1);
        if (lbu(lwu(P + 0x2080) + 0x53) != a) hero_anim_set(a, 0, static_cast<float>(frame_scale_i(8)));
    }
    sw(OV(0x0016c780), 0);
}

int32_t* pi32(uint32_t a) { return static_cast<int32_t*>(ee_ram_ptr(a)); }  // orig: (helper) EE word pointer
float* pf32(uint32_t a) { return static_cast<float*>(ee_ram_ptr(a)); }      // orig: (helper) EE float pointer

// Leaving the rail (states 0x28..0x2b; block 0x22ca24): on level 0x10 a 0x101 moby under the feet
// (+0x23c) is struck (+0xbc) and sends the hero up; when the hero is slow along the rail (or the
// rail ended, +0x5be), unless a wall is 5 ahead along the rail (+0x57c): with air (+0x22a8 >= 2)
// the hop 0x42 (with the struck moby's bounce by height); else thrown off sideways (0.7 of +0x574)
// into 0x16, the air topped up to 2 after 15 frames. True when the rail state was left.
// orig: 0x00229b70 (states 0x28..0x2b, block 0x22ca24)
bool rail_leave() {
    bool struck = false;
    if (lw(0x0015ed84) == 0x10 && lbu(P + 0x257) != 0) {
        const uint32_t m = lwu(P + 0x23c);
        if (m != 0 && lh(m + 0xa6) == 0x101) {
            struck = true;
            sb(m + 0xbc, 1);
        }
    }
    const float along = vec2_length(V(P + 0x100));
    const bool slow = along < vec2_length(V(P + 0xe0)) * 0.5f;
    if (!slow && !struck && lh(P + 0x5be) == 0) return false;
    bool clear = true;
    if (lw(0x0015ed84) != 0x10) {
        Vec4 a = V(P + 0x500);
        a.z = a.z + 2.0f;
        Vec4 d = a;
        d.x = cos_vu(lwc1(P + 0x57c)) * 5.0f;
        d.y = sin_vu(lwc1(P + 0x57c)) * 5.0f;
        d.z = 0.0f;
        d.w = 0.0f;
        vec3_add(d, d, a);
        if (segment_collide_v(a, d, 2, 0, nullptr) != 0) clear = false;
    }
    sh(P + 0x5be, 0);
    if (!(lw(P + 0x22a8) < 2) && clear) {
        set(0x42);
        if (!struck) return true;
        const float h = lwc1(P + 0x2dc);
        float k = 13.0f;
        if (u2f(0x3fd9999au) < h) {
            k = 8.0f;
        } else if (1.0f < h) {
            k = 10.0f;
        }
        swc1(P + 0xe8, lwc1(0x0015ed6c) * k);
        return true;
    }
    const float back = angle_add(atan2_f(lwc1(P + 0xe0), lwc1(P + 0xe4)), u2f(0x40490fdbu));
    const float yaw = angle_add(back, rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f1c61aau)));
    swc1(P + 0x110, cos_vu(yaw) * lwc1(P + 0x574) * u2f(0x3f333333u));
    const float sn = sin_vu(yaw);
    swc1(P + 0x118, fs6c() * 4.5f);
    swc1(P + 0x114, sn * lwc1(P + 0x574) * u2f(0x3f333333u));
    if (frame_scale_i(0xf) < lw(P + 0x570) && lw(P + 0x22a8) < 2) sw(P + 0x22a8, 2);
    set(0x16);
    sh(P + 0x1c8, static_cast<uint32_t>(frame_scale_i(0x46)));
    return true;
}

// Grinding (0x28): on level 8 the X jump is off near (342, 111.1, 42) (+0x1c4 100 frames); X: a
// queued jump with +0x5b8, else the jump 0x29; off the rail's end (+0x570) falling 6; square the
// attack 0x2b; the grind animation (0x31 + the stance +0x58c, or 0x4a + the lean +0x5a8); a queued
// jump (+0x5bc / +0x5c0) launched toward the next rail point (+0x550).
// orig: 0x00229b70 (state 0x28, block 0x22ccf4)
void grind_input(uint32_t hm0) {
    (void)hm0;
    bool can_jump = true;
    if (lw(0x0015ed84) == 8) {
        Vec4 spot = V(P + 0x80);
        spot.x = u2f(0x43ab0000u);
        spot.y = u2f(0x42de3333u);
        spot.z = 42.0f;
        spot.w = 0.0f;
        if (vec3_distance(V(P + 0x80), spot) < 7.0f) {
            can_jump = false;
            sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x64)));
        }
    }
    if (can_jump && pad_button_history(0x40, frame_scale_i(7), nullptr) != 0) {
        if (lw(P + 0x5b8) != 0) {
            sw(P + 0x5c0, 1);
        } else {
            set(0x29);
        }
        return;
    }
    if (frame_scale_i(5) < lw(P + 0x570)) {
        if (8.0f < lwc1(P + 0x2dc)) sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x46)));
        set(6);
        sh(P + 0x1c8, static_cast<uint32_t>(frame_scale_i(8)));
        return;
    }
    if (pad_button_history(0x80, frame_scale_i(7), nullptr) != 0) {
        set(0x2b);
        return;
    }
    const int32_t lean = lw(P + 0x5a8);
    const uint32_t a = lean == -1 ? lwu(P + 0x58c) + 0x31 : static_cast<uint32_t>(lean) + 0x4a;
    if (lbu(lwu(P + 0x2080) + 0x53) != a) hero_anim_set(a, 0, static_cast<float>(frame_scale_i(0xc)));
    if (lh(P + 0x5bc) != 0 && lw(P + 0x5c0) != 0) {  // the queued jump
        set(0x29);
        sw(P + 0x5c4, 1);
        const float vz = sub_0025abf0(P + 0x80, P + 0x550, nullptr, lwc1(P + 0x164), -lwc1(P + 0x4a0));
        Vec4 zero = V(P + 0x80);
        zero.x = zero.y = zero.z = zero.w = 0.0f;
        Vec4 vel = zero;
        vel.z = vz;
        const float top = sub_00259468_v(zero, vel, nullptr, lwc1(P + 0x4a0));
        swc1(P + 0x488, top);
        swc1(P + 0x4a0, lwc1(P + 0x5c8));
        swc1(P + 0x430, top);
        swc1(P + 0x48c, top + u2f(0x3d4ccccdu));
    }
    sh(P + 0x5bc, 0);
    sw(P + 0x5b8, 0);
}

// The rail attack (0x2b, on the combo table 0x17bc28 step +0xa60): the jump 0x29 on X past the
// step's +0x14; square again past frame 23 (14 after the first step) chains 0x2b; past the step's
// end (+0x18) back to grinding; off the rail's end falling 6.
// orig: 0x00229b70 (state 0x2b, block 0x22cf5c)
void rail_attack_input() {
    if (pad_button_history(0x40, frame_scale_i(5), nullptr) != 0) {
        const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
        if (static_cast<float>(lw(e + 0x14)) <= lwc1(P + 0xaa8)) {
            set(0x29);
            return;
        }
    }
    const int32_t chain = lw(P + 0xa60) == 0 ? 0x17 : 0xe;
    if (pad_button_history(0x80, frame_scale_i(7), nullptr) != 0) {
        if (static_cast<float>(chain) < lwc1(P + 0xaa8)) set(0x2b);
        return;
    }
    const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
    if (static_cast<float>(lw(e + 0x18)) < lwc1(P + 0xaa8)) {
        set(0x28);
        return;
    }
    if (!(frame_scale_i(5) < lw(P + 0x570))) return;
    set(6);
    sh(P + 0x1c8, static_cast<uint32_t>(frame_scale_i(8)));
}

// Jumping off / between rails (0x29, 0x2a): landing back on a rail (+0x56c) within 0.35 of it
// grinds 0x28; in 0x29 after 10 frames, rising, with the stick within 70 degrees of the rail's
// side: a parallel rail searched 2..4.5 to that side (ahead by the speed when the rail goes on)
// switches to it (+0x560.. to +0x594.., the new rail's closest point) as 0x2a; the arc's pacing
// (the apex flag +0x41e once the vertical speed is gone; +0xa90 kept to 0.5..2 when switching);
// coming down: the landing pacing toward the rail height +0x508, touching it (+0x418) when low,
// falling 6 half a unit below it; landed: grinding 0x28 after 2 frames, or standing off a rail.
// orig: 0x00229b70 (states 0x29 / 0x2a, block 0x22d07c)
void rail_jump_input() {
    bool check = frame_scale_i(5) < lw(P + 0x570) || lw(P + 0x5c4) != 0;
    if (check && lw(P + 0x56c) != 0 && lwc1(P + 0x88) - lwc1(P + 0x508) < u2f(0x3eb33333u)) {
        set(0x28);
        return;
    }
    bool side = lw(P + 0x2084) == 0x29 && lw(P + 0x198) < frame_scale_i(0xa) && lh(P + 0x41e) == 0 && lbu(P + 0x12ee) == 0;
    side = side && 0.5f < lwc1(P + 0x229c) && lw(P + 0x590) == 0;
    if (side) {
        hero_stick_move(0, 1.0f);
        const float q = u2f(0x3fc90fdbu);
        if (angle_dist(angle_dist(lwc1(P + 0x180), lwc1(P + 0x57c)), q) < u2f(0x3f9c61aau)) {
            float dir = angle_add(lwc1(P + 0x57c), q);
            if (q < angle_dist(lwc1(P + 0x180), dir)) dir = angle_add(lwc1(P + 0x57c), u2f(0xbfc90fdbu));
            for (float r = 2.0f; r <= 4.5f; r = r + 0.75f) {
                Vec4 at = V(P + 0x80);
                int32_t idx = lw(P + 0x564);
                float frac = lwc1(P + 0x568);
                const float step = lwc1(P + 0x160) * static_cast<float>(frame_scale_i(0));
                if (path_advance_v(lwu(P + 0x560), at, &idx, &frac, lw(P + 0x584), step) != 0) {
                    Vec4 ahead;
                    vec3_scale(ahead, V(P + 0x100), static_cast<float>(frame_scale_i(0x30)));
                    vec3_add(at, V(P + 0x80), ahead);
                }
                at.x = at.x + cos_vu(dir) * r;
                at.y = at.y + sin_vu(dir) * r;
                uint32_t path = 0;
                Vec4 point = at;
                int32_t k = 0;
                float d = 0.0f;
                int32_t loop = 0;
                if (find_rail_v(at, &path, point, &k, &d, &loop, lwu(P + 0x560), 0) == 0) continue;
                // switch to the found rail, keeping the old one at +0x594..
                const uint32_t old = lwu(P + 0x560);
                sw(P + 0x594, old);
                sw(P + 0x598, lwu(P + 0x564));
                swc1(P + 0x59c, lwc1(P + 0x568));
                sw(P + 0x5a0, lwu(P + 0x584));
                V(P + 0x540) = V(P + 0x500);
                sw(P + 0x560, path);
                sw(P + 0x584, static_cast<uint32_t>(loop));
                path_closest_point_v(path, V(P + 0x80), V(P + 0x500), pi32(P + 0x564), pf32(P + 0x568), loop, u2f(0x4479c000u), 2.0f, 0.0f);
                sw(P + 0x5a4, 0);
                sw(P + 0x590, 1);
                set(0x2a);
                break;
            }
        }
    }
    bool landing = true;
    if (lw(P + 0x420) < lw(P + 0x198) && sub_00233a78(P + 0xe0) < u2f(0x3a83126fu)) sh(P + 0x41e, 1);
    const bool early = lw(P + 0x198) < lw(P + 0x420) + frame_scale_i(1);
    if (!early && lw(P + 0x198) < lw(P + 0x420) + frame_scale_i(9) && lh(P + 0x41e) == 0 && lw(P + 0xa9c) == 0) {  // the arc
        int32_t frames = 0;
        sub_00233c30(P + 0x80, P + 0xe0, &frames, lwc1(P + 0x4a0));
        sub_00208f20(lwc1(P + 0x440), static_cast<float>(frames), 0.5f, lwc1(0x0015ed64) * 0.25f);
        if (lw(P + 0x590) != 0) {
            if (2.0f < lwc1(P + 0xa90)) swc1(P + 0xa90, 2.0f);
            if (lwc1(P + 0xa90) < 0.5f) swc1(P + 0xa90, 0.5f);
        }
        landing = false;
    }
    if (landing && lh(P + 0x41e) != 0 && lw(P + 0x418) == 0) {  // coming down
        sw(P + 0x44c, static_cast<uint32_t>(frame_scale_i(0x1e)));
        if (lw(P + 0x570) == 0) sw(P + 0x44c, static_cast<uint32_t>(float_to_int(hero_fall_time(60.0f, lwc1(P + 0x4a0), lwc1(P + 0x508)))));
        const int32_t n = lw(P + 0x44c);
        if (n != 0) sub_00208f20(lwc1(P + 0x444), static_cast<float>(n), 0.5f, lwc1(0x0015ed64) * 0.25f);
        if (lw(P + 0x590) != 0) {
            if (2.0f < lwc1(P + 0xa90)) swc1(P + 0xa90, 2.0f);
            if (lwc1(P + 0xa90) < 0.5f) swc1(P + 0xa90, 0.5f);
        }
        if (lw(P + 0x570) == 0 && lwc1(P + 0x2dc) < 0.5f && lwc1(P + 0x88) + lwc1(P + 0xe8) < lwc1(P + 0x508)) {  // touching the rail
            swc1(P + 0xe8, lwc1(P + 0x508) - lwc1(P + 0x88));
            sw(P + 0x418, static_cast<uint32_t>(frame_scale_i(1)));
            swc1(P + 0xa90, 1.0f);
        }
        if (lwc1(P + 0x88) - lwc1(P + 0x508) < -0.5f) {
            set(6);
            sw(P + 0x1c4, static_cast<uint32_t>(frame_scale_i(0x46)));
            sh(P + 0x1c8, static_cast<uint32_t>(frame_scale_i(8)));
            return;
        }
    }
    if (lw(P + 0x418) != 0) {  // on the rail again: grinding after 2 frames
        sw(P + 0x418, lwu(P + 0x418) + 1);
        if (frame_scale_i(2) < lw(P + 0x418)) {
            sw(P + 0x2084, 0x28);
            const uint32_t a = lwu(P + 0x58c) + 0x31;
            hero_anim_set(a, 0, static_cast<float>(frame_scale_i(7)));
            return;
        }
    }
    if (lw(P + 0x570) == 0 || lh(P + 0x30e) != 0 || lh(P + 0x41e) == 0) return;
    if (hero_set_state(0, 0) == 0) return;
    hero_anim_set(0xc, 4, static_cast<float>(frame_scale_i(6)));
}

// The hop off the rail's end (0x42): the arc's pacing while rising; grinding 0x28 at the animation's
// end or when coming down within 0.3 of the rail height.
// orig: 0x00229b70 (state 0x42, block 0x22d718)
void rail_hop_input() {
    if (0.0f < lwc1(P + 0xe8)) {
        int32_t frames = 0;
        sub_00233c30(P + 0x80, P + 0xe0, &frames, lwc1(0x0015ed70) * 23.0f);
        sub_00208f20(9.0f, static_cast<float>(frames), 0.5f, -1.0f);
    }
    if ((lw(P + 0xa98) & 2) != 0) {
        set(0x28);
        return;
    }
    if (!(lwc1(P + 0xe8) < 0.0f)) return;
    if (abs_f(lwc1(P + 0x88) - lwc1(P + 0x508)) < u2f(0x3e99999au)) set(0x28);
}

// The per-state transitions. Each case is one target of the jump table 0x1e82b0.
// orig: 0x00229b70 (jump table 0x1e82b0)
bool transitions(uint32_t state, uint32_t hm0) {
    switch (state) {
    case 0x77:
        return kill_fall_input();
    case 0x40:  // holding a moby overhead: dropping it to move, standing when done
        if ((buttons() & 5) != 0) {
            set(1);
            return false;
        }
        if (lh(P + 0x30e) != 0 || (buttons() & 0x40) != 0 || (buttons() & 0xa) != 0) {
            set(0x41);
            return false;
        }
        hero_stick_speed();
        if (0.0f < lwc1(P + 0x190)) {
            set(0x41);
            return false;
        }
        if ((lw(P + 0xa98) & 2) == 0) return false;
        hero_release_attached();
        set(0);
        return false;
    case 0x41:  // dropping it: after 8 frames free to fall, jump, crouch, run or stand
        if (!(frame_scale_i(8) < lw(P + 0x198))) return false;
        hero_release_attached();
        if (lh(P + 0x30e) != 0) {
            set(6);
            return false;
        }
        if (hero_try_jump(frame_scale_i(7), 0) != 0) return false;
        if ((buttons() & 0xa) != 0) {
            set(4);
            return false;
        }
        hero_stick_speed();
        set(0.0f < lwc1(P + 0x190) ? 2u : 0u);
        return false;
    case 0x3d: case 0x47: case 0x57: case 0x62: case 0x6a: case 0x80: case 0x82:  // dying: lost when the animation ends
        if ((lw(P + 0xa98) & 2) == 0) return false;
        hero_lost();
        return true;
    case 0x7f:  // warping out: lost after 220 frames
        if (!(frame_scale_i(0xdc) < lw(P + 0x198))) return false;
        hero_lost();
        return true;
    case 0x33: case 0x35:
        swim_input();
        return false;
    case 0x34:  // drifting: the drift animation; any input swims on
        if ((lw(P + 0xa98) & 2) != 0 && lbu(lwu(P + 0x2080) + 0x53) != 0x3c) {
            hero_anim_set(0x3c, 0, static_cast<float>(frame_scale_i(0xb)));
        }
        if (0.2f < lwc1(P + 0x229c) || (buttons() & 0xc0) != 0 || (lbu(0x0013d4c4) != 0 && (buttons() & 0xa) != 0)) dive();
        return false;
    case 0x36: case 0x37:
        surface_input();
        return false;
    case 0x76:  // hurt under water: out of air → drowning (0x82 on levels 15 / 17); else swim on
        if (!(0 < lw(P + 0x22a8))) {
            const uint32_t lvl = lwu(0x0015ed84);
            if (lvl == 0xf || lvl == 0x11) {
                set(0x82);
                return false;
            }
            set(0x6a);
            hero_anim_set(0x5f, 0x28, static_cast<float>(frame_scale_i(0x12)));
            return false;
        }
        if ((lw(P + 0xa98) & 2) != 0) {
            set(0x34);
            return false;
        }
        if (frame_scale_i(0x32) < lw(P + 0x198)) hero_set_state(0x34, 0);
        return false;
    case 0x75:  // hurt on the surface: out of air → drowning; else back to the surface
        if (!(0 < lw(P + 0x22a8))) {
            set(0x6a);
            hero_anim_set(0x5f, 0x25, static_cast<float>(frame_scale_i(0x12)));
            return false;
        }
        if (frame_scale_i(0x32) < lw(P + 0x198)) hero_set_state(0x37, 0);
        return false;
    case 0x38: case 0x39:  // riding: off when the item is not the rideable 0x16
        if (lw(P + 0x10b8) != 0x16) set(0);
        return false;
    case 0x3a:
        if ((lw(P + 0xa98) & 2) != 0) set(0);
        return false;
    case 0x65:
        walk_to_input();
        return false;
    case 0x66:  // walking to a point, fast: back to 0x65 when the animation ends
        if ((lw(P + 0xa98) & 2) != 0) set(0x65);
        return false;
    case 0x67:  // stopping at the point: standing when done (or +0x1650); walking on if moved off
        if ((lw(P + 0xa98) & 2) != 0 || lw(P + 0x1650) != 0) set(0);
        if (0.2f < vec2_distance(V(P + 0x80), V(P + 0x1640))) set(0x65);
        return false;
    case 0x68: {  // +0x1660 < 2: the jump 0x69 after 15 + 25 * +0x1660 frames
        const int32_t n = lw(P + 0x1660);
        if (n < 2) {
            const int32_t a = frame_scale_i(0xf);
            const int32_t b = frame_scale_i(0x19);
            int32_t ago = 0;
            if (a + n * b < lw(P + 0x198) && pad_button_history(0x40, frame_scale_i(7), &ago) != 0) set(0x69);
        }
        return lost_timer(u2f(0x3f99999au), 0xaa);
    }
    case 0x7b:
        return lost_timer(u2f(0x3f99999au), 0x96);
    case 0x7c:
        return lost_timer(u2f(0x3f99999au), 0x64);
    case 0x7d:
        return lost_timer(u2f(0x3f4ccccdu), 0x96);
    case 0x3f:
        board_ready_input();
        return false;
    case 0x24:  // standing with the item 0xc: back to 0 when it is put away
        if (lw(P + 0x10b8) != 0xc) set(0);
        return false;
    case 0x25:
        hold_input();
        return false;
    case 0x26:  // let go: standing when low, else falling
        if ((buttons() & lwu(P + 0x10a0)) != 0) return false;
        speed_from_960();
        if (lwc1(P + 0x2dc) < u2f(0x3ecccccdu)) {
            set(0);
            return false;
        }
        set(6);
        sw(P + 0x2284, 0);
        return false;
    case 0x2c:  // let go after 10 frames: the fall 0x2d (+0x988 before 40 frames)
        if ((buttons() & lwu(P + 0x10a0)) != 0 || !(frame_scale_i(0xa) < lw(P + 0x198))) return false;
        if (lw(P + 0x198) < frame_scale_i(0x28)) sw(P + 0x988, 1);
        set(0x2d);
        swc1(P + 0x944, lwc1(0x0015ed70) * 7.0f);
        swc1(P + 0x940, lwc1(0x0015ed70) * 27.0f);
        return false;
    case 0x22:
        air_flip_input();
        return false;
    case 0x74:  // state 0x74: in its 0x73 animation the 11-frame cue (sound 0xd, sparks), then 0x66;
                // falling 6 once +0x600 passes 5 frames
        if (lbu(lwu(P + 0x2080) + 0x53) == 0x73) {
            if (sub_00211870(11.0f) != 0) {
                hero_sound_start(0xd, 0);
                spark_pairs(P + 0x5e0, 4);
            }
            if ((lw(P + 0xa98) & 2) != 0) hero_anim_set(0x66, 0, static_cast<float>(frame_scale_i(0x1e)));
        }
        if (frame_scale_i(5) < lw(P + 0x600)) {
            set(6);
            sh(P + 0x1e4, static_cast<uint32_t>(frame_scale_i(8)));
        }
        return false;
    case 0x18:  // grabbing a ledge: hanging 0x19 after 13.5 frames
        if (13.5f < lwc1(P + 0xaa8)) hero_set_state(0x19, 0);
        return false;
    case 0x19:
        hang_input(false);
        return false;
    case 0x1a: case 0x1b:
        shimmy_input(false);
        return false;
    case 0x4b:
        hang_input(true);
        return false;
    case 0x4d: case 0x4e:
        shimmy_input(true);
        return false;
    case 0x4a:  // grabbing a ledge (0x43 set): hanging 0x4b when the animation ends
        if ((lw(P + 0xa98) & 2) != 0) set(0x4b);
        return false;
    case 0x46:  // state 0x46: falling 0x45 (after 32 frames, high); after 35: 0x43, or 0x47 without air
        if (!(lw(P + 0x198) < frame_scale_i(0x20)) && lh(P + 0x30e) != 0 && 1.0f < lwc1(P + 0x2dc)) {
            set(0x45);
            return false;
        }
        if (!(frame_scale_i(0x23) < lw(P + 0x198))) return false;
        if (!(0 < lw(P + 0x22a8))) {
            set(0x47);
            return false;
        }
        hero_set_state(0x43, 0);
        return false;
    case 0x16:
        state16_input();
        return false;
    case 0x43:
        state43_input();
        return false;
    case 0x51:
        state51_input();
        return false;
    case 0x49: case 0x4c:
        jump43_input(hm0);
        return false;
    case 0x44: case 0x50:
        walk43_input();
        return false;
    case 0x45:
        return fall43_input();
    case 0x4f:
        state4f_input();
        return false;
    case 0x52:  // state 0x52: lost after 300 frames (and on); without +0x12ea back to 0x45 / 0x43
        if (frame_scale_i(0x12c) < lw(P + 0x198)) hero_lost();
        if (lbu(P + 0x12ea) != 0) return false;
        set(lh(P + 0x30e) != 0 ? 0x45u : 0x43u);
        return false;
    case 0x5a:
        state5a_input();
        return false;
    case 0x5e:
        jump5a_input(hm0);
        return false;
    case 0x5b:
        walk5a_input();
        return false;
    case 0x5c:  // the fall of the 0x5a set: pushing mobys below; landing: the speeds capped at -9
                // fs6c, then 0x5b / 0x5a with +0x1630, else 0x62
        if (lh(P + 0x41e) != 0) {
            Vec4 v = V(P + 0x80);
            v.z = v.z - 1.0f;
            moby_sphere_contact_v(lwu(P + 0x2080), v, 0x30000, 0, 1, 0, 3.5f, 1.0f, 1.0f);
        }
        if (lh(P + 0x30e) != 0) return false;
        if (lwc1(P + 0x108) < fs6c() * -9.0f) swc1(P + 0x108, fs6c() * -9.0f);
        if (lwc1(P + 0xe8) < fs6c() * -9.0f) swc1(P + 0xe8, fs6c() * -9.0f);
        if (!(0 < lw(P + 0x1630))) {
            set(0x62);
            return false;
        }
        set(0.5f < lwc1(P + 0x229c) && lw(P + 0x1c4) == 0 ? 0x5bu : 0x5au);
        return false;
    case 0x60: {  // the attack of the 0x5a set: chained on square after 20 frames past frame 21 (25.5
                  // in animation 0xb); standing 0x5a past frame 28.5 or at the animation's end
        const float k = lbu(lwu(P + 0x2080) + 0x53) == 0xb ? 25.5f : 21.0f;
        if (lw(P + 0xa9c) == 0 && k < lwc1(P + 0xaa8) && frame_scale_i(0x14) < lw(P + 0x198) &&
            pad_button_history(0x80, frame_scale_i(0x11), nullptr) != 0) {
            set(0x60);
        }
        if ((28.5f < lwc1(P + 0xaa8) && lw(P + 0xa9c) == 0) || (lw(P + 0xa98) & 2) != 0) hero_set_state(0x5a, 0);
        return false;
    }
    case 0x61:  // the 0x5a set's 0x61: standing at the animation's end, +0x1636 the 301-frame wait
        if ((lw(P + 0xa98) & 2) == 0) return false;
        sh(P + 0x1636, static_cast<uint32_t>(frame_scale_i(0x12d)));
        set(0x5a);
        return false;
    case 0x5f:  // the 0x5a set's 0x5f: while circle is held; standing at the animation's end
        if ((buttons() & 0x20) != 0) return false;
        if ((lw(P + 0xa98) & 2) != 0) set(0x5a);
        return false;
    case 0x5d:  // the 0x5a set's 0x5d: falling 0x5c (after 32 frames, high); after 40 frames or the
                // animation's end: 0x5a, or 0x62 without +0x1630
        if (!(lw(P + 0x198) < frame_scale_i(0x20)) && lh(P + 0x30e) != 0 && 1.5f < lwc1(P + 0x2dc)) {
            set(0x5c);
            return false;
        }
        if (!(frame_scale_i(0x28) < lw(P + 0x198)) && (lw(P + 0xa98) & 2) == 0) return false;
        if (!(0 < lw(P + 0x1630))) {
            set(0x62);
            return false;
        }
        hero_set_state(0x5a, 0);
        return false;
    case 0x53:
        state53_input();
        return false;
    case 0x54:
        walk53_input();
        return false;
    case 0x55:
        fall53_input();
        return false;
    case 0x56:  // the 0x53 set's 0x56: falling 0x55 (after 32 frames, high); 0x53 at the animation's
                // end; after 45 frames 0x53, or 0x57 without air
        if (!(lw(P + 0x198) < frame_scale_i(0x20)) && lh(P + 0x30e) != 0 && 1.0f < lwc1(P + 0x2dc)) {
            set(0x55);
            return false;
        }
        if ((lw(P + 0xa98) & 2) != 0) {
            set(0x53);
            return false;
        }
        if (!(frame_scale_i(0x2d) < lw(P + 0x198))) return false;
        if (!(0 < lw(P + 0x22a8))) {
            set(0x57);
            return false;
        }
        hero_set_state(0x53, 0);
        return false;
    case 0x58:  // the 0x53 set's 0x58: lost after 300 frames (and on); without +0x12ea back to 0x55 / 0x53
        if (frame_scale_i(0x12c) < lw(P + 0x198)) hero_lost();
        if (lbu(P + 0x12ea) != 0) return false;
        set(lh(P + 0x30e) != 0 ? 0x55u : 0x53u);
        return false;
    case 0x59:  // the 0x53 set's 0x59: after 45 frames with circle released, standing in animation 0
        if (!(frame_scale_i(0x2d) < lw(P + 0x198)) || (buttons() & 0x20) != 0) return false;
        hero_set_state(0x53, 0);
        hero_anim_set(0, 0, static_cast<float>(frame_scale_i(0x14)));
        return false;
    case 0x1e:
        unready_input();
        return false;
    case 0x42:
        sh(P + 0x5be, 0);
        rail_hop_input();
        return false;
    case 0x28: case 0x29: case 0x2a: case 0x2b:
        if (rail_leave()) return false;
        if (state == 0x28) {
            grind_input(hm0);
        } else if (state == 0x2b) {
            rail_attack_input();
        } else {
            rail_jump_input();
        }
        return false;
    case 0x13: case 0x14: case 0x15:
        swing_input();
        return false;
    case 0x21: case 0x7a:  // the side flips: standing when the animation ends
        if ((lw(P + 0xa98) & 2) != 0) set(0);
        return false;
    case 0x06: case 0x2d:
        fall_input();
        return false;
    case 0x79:  // sliding down a slope: lost after 300 frames; off it: falling or standing
        if (frame_scale_i(0x12c) < lw(P + 0x198)) hero_lost();
        if (lbu(P + 0x12ea) != 0) return false;
        set(lh(P + 0x30e) != 0 ? 6u : 0u);
        return false;
    case 0x81:
        low_crouch_input();
        return false;
    case 0x08:
        slam_land_input();
        return false;
    case 0x00:
        stand_input(hm0);
        return false;
    case 0x01:
        ready_input();
        return false;
    case 0x17:  // the item button released and no +0x1d0: standing
        if ((buttons() & lwu(P + 0x10a0)) != 0 || lw(P + 0x1d0) != 0) return false;
        set(0);
        return false;
    case 0x30:  // likewise, quietly from animation 0x36
        if ((buttons() & lwu(P + 0x10a0)) != 0 || lw(P + 0x1d0) != 0) return false;
        if (lbu(lwu(P + 0x2080) + 0x53) == 0x36) {
            hero_set_state(0, 0);
        } else {
            set(0);
        }
        return false;
    case 0x2e:  // after 28 frames of animation 0x2e: 0x17 with the item button held, else standing
        if (lbu(lwu(P + 0x2080) + 0x52) != 0x2e || !(28.0f < lwc1(P + 0xaa8))) return false;
        set((buttons() & lwu(P + 0x10a0)) != 0 ? 0x17u : 0u);
        return false;
    case 0x03:
        landed_input();
        return false;
    case 0x04:
        crouch_input();
        return false;
    case 0x70:  // the hover-board swing: standing once the combo step's end frame +0x18 passed
        if (static_cast<float>(lw(OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c + 0x18)) < lwc1(P + 0xaa8)) set(0);
        return false;
    case 0x71:  // the hover-board hop: standing after 25 frames
        if (25.0f < lwc1(P + 0xaa8)) set(0);
        return false;
    case 0x31:  // the board: off it (+0x12e3 clear, +0x1e0 run out) back to standing
        if (lbu(P + 0x12e3) != 0 || lh(P + 0x1e0) != 0) return false;
        {
            const int32_t s = lw(P + 0x2220);
            if (s != -1) {
                const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s * 0x70);
                if (lwu(e + 0x88) == lwu(P + 0x2080) && lbu(e + 0x74) != 0) sub_0028d918(s);
            }
            sw(P + 0x2220, 0xffffffffu);
        }
        if (hero_set_state(0, 0) == 0) return false;
        {
            const uint32_t s = sub_00205000(0);
            hero_anim_set(s, 0, static_cast<float>(frame_scale_i(0x11)));
        }
        V(P + 0x150) = V(P + 0x140);
        return false;
    case 0x02: case 0x2f: case 0x73: case 0x7e:
        run_input(hm0);
        return false;
    case 0x07: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f: case 0x10:
    case 0x11: case 0x12: case 0x1c: case 0x3c: case 0x69:
        jump_input(hm0);
        return false;
    case 0x20:  // the thruster's jet: back to standing once it has run 20 frames
        if (lw(P + 0x1090) == 0 || lw(P + 0xa9c) != 0) return false;
        if (20.0f < lwc1(P + 0xaa8)) hero_set_state(0, 0);
        return false;
    case 0x23:  // the throw stance: a jump after 19 frames, standing after 21
        if (19.0f < lwc1(P + 0xaa8) && pad_button_history(0x40, frame_scale_i(0xb), nullptr) != 0) {
            if ((buttons() & 0xa) != 0) {
                hero_jump_input();
            } else {
                hero_set_state(7, 1);
            }
            return false;
        }
        if (21.0f <= lwc1(P + 0xaa8)) hero_set_state(0, 0);
        return false;
    case 0x05: case 0x1d: case 0x1f: case 0x27: case 0x32: case 0x3b: case 0x3e: case 0x48: case 0x63:
    case 0x64: case 0x6b: case 0x6c: case 0x6d: case 0x6e: case 0x6f: case 0x72: case 0x78:
        return false;  // no transitions of their own
    default:
        return false;  // past the jump table (0x83 states): no transitions
    }
}
}  // namespace

// The select button's mode switch (the gadget menu: camera slot 3 and the 0x754e message), the
// item-change counters +0x22d4 / +0x22d6 (a quick double select swaps to the last item +0x20b8,
// counted in a stat) and the fall below the level's kill height 0x15f638 (state 0x77) run
// before the per-state transitions; hits, the item in hand, water, the air meter and the item
// stance can take over first. Afterwards a sub-state change runs sub_00206c08, a pending
// release of the attached moby (+0x2090 0x40 / 0x41) is done, and a change of +0x2088 or of the
// hero moby's animation resets the timers +0x1a0 / +0x1a4.
// orig: 0x00229b70
void hero_input_update() {
    if (on_blarg()) { blg_call(blg_hero_input_update_u); return; }  // (level 6's own: L6:0x0023b440)
    if (on_batalia()) { bat_call(bat_hero_input_update_u); return; }  // (level 8's own: L8:0x002370d8)
    if (on_pokitaru()) { pok_call(pok_hero_input_update_u); return; }  // (level 11's own: L11:0x00253a18)
    if (on_hoven()) { hov_call(hov_hero_input_update_u); return; }  // (level 12's own: L12:0x002461f0)
    if (on_oltanis()) { olt_call(olt_hero_input_update_u); return; }  // (level 14's own: L14:0x00235600)
    if (on_quartu()) { qua_call(qua_hero_input_update_u); return; }  // (level 15's own: L15:0x0021d040)
    if (on_kalebo()) { kal_call(kal_hero_input_update_u); return; }  // (level 16's own: L16:0x002270c8)
    if (on_veldin_finale()) { vf_call(vf_hero_input_update_u); return; }  // (level 18's own: L18:0x0022e8f8)
    if (on_veldin_orbit()) { vo_call(vo_hero_input_update_u); return; }  // (level 17's own: L17:0x002258a8)
    if (on_rilgar()) { ril_call(ril_hero_input_update_u); return; }  // (Rilgar's own: L5:0x00255960)
    if (on_l1_family()) return hero_input_update_l1();
    const uint32_t hm = lwu(P + 0x2080);
    const uint32_t mode0 = lwu(P + 0x208c);
    const uint32_t sub0 = lwu(P + 0x2088);
    const uint32_t anim0 = lbu(hm + 0x53);
    bool skip_tail = false;  // (the lost hero leaves without the tail)
    [&] {
        if (hero_hurt(1) != 0) return;
        if (!(0 < lw(P + 0x22a8)) && lw(P + 0x208c) != 0x14 && (lw(P + 0x2084) == 0 || lw(P + 0x2084) == 2)) {
            hero_set_state(0x3d, 1);
            return;
        }
        if (hero_item_use() != 0) return;
        if (hero_water_entry() != 0 || hero_water_air() != 0 || hero_try_item_stance() != 0) return;
        if (((pressed() & 0x10) != 0 || (lwu(PAD + 0x1c4) & 0x10) != 0) && lbu(P + 0x10ac) != 2 && lw(OV(0x0015f594)) == 0) {
            const bool free_a = lbu(P + 0x20ac) == 0 || lbu(P + 0x20a4) == 1 || lh(P + 0x1b2) != 0;
            const bool free_b = lbu(P + 0x20a7) == 0 || lbu(P + 0x20a4) == 1 || lh(P + 0x1b2) != 0;
            const uint32_t mode = lbu(P + 0x20a4);
            if (free_a && free_b) {
                if (mode == 1) {
                    camera_request(3, 0, OVF(0x00238b18), OVF(0x00238b80), OVF(0x00238f88), 0, 0);
                } else if (mode == 0 || mode == 3) {
                    sh(P + 0x22d2, static_cast<uint32_t>(frame_scale_i(lh(P + 0x22d6) != 0 ? 0x1b : 0xf)));
                    camera_request(3, 0, OVF(0x00237ae8), OVF(0x00237ba0), OVF(0x002382a0), 0, 0);
                    const int32_t k = camera_request_7535();
                    sub_00235e18(static_cast<uint32_t>(k), frame_scale_i(0xb4));
                    const int32_t m = camera_request(2, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), 0x0015ed98, 0x0098967f);
                    sub_00235e18(static_cast<uint32_t>(m), frame_scale_i(0xb4));
                }
            }
        }
        timer_tick_i16(ee_ref<int16_t>(P + 0x22d4));
        timer_tick_i16(ee_ref<int16_t>(P + 0x22d6));
        if ((pressed() & 0x10) != 0) sh(P + 0x22d4, static_cast<uint32_t>(frame_scale_i(0x14)));
        if ((ld(P + 0x22d0) >> 32) != 0 && (lwu(PAD + 0x1b8) & 0x10) != 0) {  // select released in the window
            if (lh(P + 0x22d6) == 0) {
                sh(P + 0x22d6, static_cast<uint32_t>(frame_scale_i(0x14)));
            } else {
                const uint32_t prev = lwu(0x0015ed8c);  // (the item compared against, even for the wrench)
                const uint32_t last = lw(P + 0x10b8) == 8 ? lwu(0x00141660) : prev;
                if (last != 0 && lwu(P + 0x10b8) != prev) {
                    hero_stat_use(0x00141848 + 0xf8);
                    sw(P + 0x20b8, last);
                    sh(P + 0x22d6, 0);
                    sh(P + 0x22d4, 0);
                }
            }
        }
        if (lwc1(P + 0x88) < lwc1(OV(0x0015f638)) && 2.0f < lwc1(P + 0x2dc) && lbu(P + 0x20a4) != 2 && lw(P + 0x2084) != 0x77) {
            hero_set_state(0x77, 1);
            return;
        }
        if (lwu(P + 0x2084) < 0x83 && transitions(lwu(P + 0x2084), hm)) skip_tail = true;
    }();
    if (skip_tail) return;
    if (mode0 != lwu(P + 0x208c)) sub_00206c08();
    const uint32_t att = lwu(P + 0x2090);
    if ((att == 0x40 && lw(P + 0x2084) != 0x41) || (att == 0x41 && lw(P + 0x1614) != 0)) hero_release_attached();
    if (sub0 != lwu(P + 0x2088)) sw(P + 0x1a0, 0);
    if (anim0 != lbu(hm + 0x53)) sw(P + 0x1a4, 0);
}

}  // namespace rac1
