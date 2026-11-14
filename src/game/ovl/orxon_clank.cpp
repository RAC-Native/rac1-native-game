// Orxon's own moby classes (level 10): Clank's part. Class 22, the place where Ratchet hands over to Clank (the hero
// becomes Clank at its camera's box, and Ratchet again once the game's byte 0x13d4c6 says Clank's part is done); class
// 351, the pads that carry Clank to their partner pad in a spray of sparks; class 18, the O2 Mask on its stand (item
// 0x1c). Header orxon_mobys.h, registered in orxon_mobys.cpp; level 10's own addresses (`orig: L10:`).
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/level_change.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/arrival.h>
#include <game/ovl/orxon_common.h>
#include <game/ovl/orxon_mobys.h>
#include <game/ovl/shared_panel_lights.h>
#include <game/ovl/sound_emitter.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace orxon;

namespace {
constexpr uint32_t STATS = 0x00141848;    // the game's statistics block
constexpr uint32_t CLANK_DONE = 0x0013d4c6;  // the game's byte: Clank's part done

// the hero placed at level box `box`'s place and turn (the box's +0x30 / +0x70)
void hero_to_box(int32_t box) {
    const uint32_t rec = lvd::box(lvd::BOXES, box);
    hero_place(rec + 0x30, rec + 0x70, 0, 1);
}
// the moby hidden and its +0x94 cleared
void hide(uint32_t mo) {
    sh(mo + 0x34, lhu(mo + 0x34) | 3u);
    sw(mo + 0x94, 0);
}
}  // namespace

// Class 22: every frame it takes the hero over in Clank's part. Its pvars: +0 Clank's ship (a moby), +4 the box Ratchet
// comes back to, +8 / +0x14 the boxes Clank starts at, +0x18 / +0x1c two spawn points (class 805) of which the one
// for the part's state (the moby gone for good or not) is turned on (+8 bit 3). Unless the hero is Clank already, on
// the first frame of its flag or with no scene playing (then also the statistics' timer, a planet's mark, and a
// message), the hero becomes Clank (state 0x43, the ship's moby) at the start box, with music track 2; once the game's
// byte 0x13d4c6 is set the ship is hidden, Clank becomes Ratchet again at the box +4, track 0, and the moby goes.
// orig: L10:0x00298b68
void orx_class22_update_l10(uint32_t m) {
    sb(m + 0x30, 0xff);
    if (lw(L10_ADDR(0x00161388)) == 1) {
        sw(L10_ADDR(0x00161388), 0);
        sh(STATS + 0x100, 0);
    }
    const uint32_t pv = lwu(m + 0x78);
    const int32_t point_a = lw(pv + 0x18);
    const int32_t point_b = lw(pv + 0x1c);
    if (point_a != -1 && point_b != -1) {
        const uint32_t a = moby_at(point_a), b = moby_at(point_b);
        if (lh(a + 0xa6) == 0x325 && lh(b + 0xa6) == 0x325) {
            const uint32_t pva = lwu(a + 0x78), pvb = lwu(b + 0x78);
            if (gone_byte(m) == 0xff) {
                sw(pva + 8, lwu(pva + 8) | 8u);
                sw(pvb + 8, lwu(pvb + 8) & ~8u);
            } else {
                sw(pva + 8, lwu(pva + 8) & ~8u);
                sw(pvb + 8, lwu(pvb + 8) | 8u);
            }
        }
    }
    if (flag_set(m)) {
        if (lbu(CLANK_DONE) != 0) {
            hide(moby_at(lw(pv)));
        } else if (lw(OV(0x0015f5c4)) != 0) {  // (a scene playing)
            if (lbu(HERO_FORM) == 1) return;
        } else {
            if (lhu(STATS + 0x100) < frame_scale_i(0x12c)) {
                const uint32_t t = lhu(STATS + 0x100);
                if (!(0xfffeu < t)) sh(STATS + 0x100, t + 1);
                const int32_t minutes = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
                if (lhu(STATS + 0x102) < minutes) sh(STATS + 0x102, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
                sw(STATS + 0x104, lwu(STATS + 0x104) | (1u << (level_number() & 31)) | 0x80000000u);
                hud_message_request(1, TXT(0x53f1));
            }
            if (lbu(HERO_FORM) == 1) return;
        }
    }
    flag_mark(m);
    if (lw(pv + 8) == -1 || lw(pv) == -1) return;
    if (lbu(CLANK_DONE) == 0) {
        const uint32_t ship = moby_at(lw(pv));
        sw(ship + 0x94, lwu(lwu(ship + 0x24) + 0x10));
        // (the box: +0x14 when the moby is gone for good, else +8 unless +0x14 is -1, then +0x14: -1)
        int32_t box = lw(pv + 0x14);
        if (gone_byte(m) != 0xff && box != -1) box = lw(pv + 8);
        hero_to_box(box);
        hero_mode_enter(1, 0x43, moby_at(lw(pv)));
        music_track_request(2, 4);
        return;
    }
    if (lbu(HERO + 0x20a4) == 1) {
        hero_mode_exit();
        hero_to_box(lw(pv + 4));
        music_track_request(0, 5);
        hide(moby_at(lw(pv)));
    }
    sb(HERO + 0x22cb, 1);
    moby_retire(m);
}

namespace {
// Ten sparks (particle 0x35) about the point at `centre`: within half a unit of it, random colours, flying along the previous spark's
// speed set to a random length up to the frame time (`vel`, kept in the caller's frame) and up 2.5 to 5 frame times.
// `vel` is the original's stack vector (the first spark of the first spray starts from what the frame holds).
void sparks_about(uint32_t pos, uint32_t vel, uint32_t centre, bool reset_vel) {
    for (int32_t k = 0; k < 10; ++k) {
        if (reset_vel) {  // (the second spray: a random flat direction first)
            V(pos) = Vec4{};
            swc1(pos, rand_range_f(-1.0f, 1.0f));
            swc1(pos + 4, rand_range_f(-1.0f, 1.0f));
            lq_sq(vel, pos);
        }
        const float size = rand_range_f(u2f(0x3dcccccdu), 1.0f);
        const uint32_t red = static_cast<uint32_t>(rand_range(0x40, 0xff));
        const uint32_t green = static_cast<uint32_t>(rand_range(0x40, 0xff));
        const uint32_t blue = static_cast<uint32_t>(rand_range(0x40, 0xff));
        const float cx = cos_vu(rand_angle());  // (each call on its own line: the random sequence's order)
        swc1(pos, fpu_mul(cx, rand_range_f(0.0f, 0.5f)));
        const float sy = sin_vu(rand_angle());
        const float y = fpu_mul(sy, rand_range_f(0.0f, 0.5f));
        swc1(pos + 8, 0.0f);
        swc1(pos + 4, y);
        swc1(pos + 8, fpu_add(lwc1(pos + 8), rand_range_f(0.0f, 0.5f)));
        vec3_add_inplace(V(pos), V(centre));
        vec3_set_length(V(vel), V(vel), rand_range_f(0.0f, dt()));
        swc1(vel + 8, rand_range_f(fpu_mul(dt(), 2.5f), fpu_mul(dt(), 5.0f)));
        const int32_t life = frame_scale_i(rand_range(0xf, 0x1e));
        const uint32_t colour = 0x7f000000u | (blue << 16) | (green << 8) | red;
        const float small = fpu_mul(size, u2f(0x3d8f5c29u)), big = fpu_mul(size, u2f(0x3f333333u));
        const uint32_t turn = rand_mod(2) == 0 ? 0xffffffffu : 1u;
        spawn_particle_35(pos, static_cast<uint32_t>(life), colour, 0, turn, vel, small, big, fpu_mul(dt2(), 15.0f));
    }
}
}  // namespace

// Class 351: a pad that carries Clank (the hero in form 1, within half a unit) to its partner pad (pvar +0: the
// partner's index): sparks at both ends, its sound, the hero placed on the partner in state 0x43, the level's camera
// eased in (the timer L10:0x1618a8 from 0x1618a0, the camera's rates by how much of 0x1618a4 is left). Every second
// (frame scaled) a column of 16 particles 0x2d rises from a pad with a partner; the pad's colour pulses over 150 frames.
// The first frame of each pad scales the level's two times (0x1618a0 / 0x1618a4) by the frame scale.
// orig: L10:0x002be858
void orx_class351_update_l10(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        const int32_t total = lw(L10_ADDR(0x001618a4));
        sw(L10_ADDR(0x001618a8), 0);
        sw(L10_ADDR(0x001618a4), static_cast<uint32_t>(frame_scale_i(total)));
        sw(L10_ADDR(0x001618a0), static_cast<uint32_t>(frame_scale_i(lw(L10_ADDR(0x001618a0)))));
        sb(m + 0x20, 1);
    }
    const uint32_t frame = ee_stack_alloc(0x100);
    const uint32_t vel = frame, pos = frame + 0x10;
    if (lbu(HERO_FORM) == 1 && lw(pv) != -1 && vec2_distance(V(m + 0x10), V(HERO_POS)) < 0.5f) {
        sparks_about(pos, vel, HERO_POS, false);
        moby_sound_start(0, 0, m);
        const uint32_t partner = moby_at(lw(pv));
        hero_place(partner + 0x10, partner + 0x40, 0x43, 0);
        sw(L10_ADDR(0x001618a8), lwu(L10_ADDR(0x001618a0)));
        const uint32_t cam = lwu(OV(0x00166e00));
        if (lh(cam + 0x86) == 0) {
            sh(lwu(cam + 0x70) + 0x20, static_cast<uint32_t>(frame_scale_i(0x5a)));
            lq_sq(cam + 0x40, cam);
        }
        sparks_about(pos, vel, partner + 0x10, true);
    }
    const int32_t clock = lw(OV(0x0015f5cc));
    if (ee_rem(clock, frame_scale_i(0x3c)) == 0 && lw(pv) != -1) {
        const int32_t count = float_to_int(16.0f);
        for (int32_t k = 0; k < count; ++k) {
            lq_sq(vel, m + 0x10);
            swc1(vel + 8, fpu_add(lwc1(vel + 8), fpu_add(fpu_mul(fpu_mul(int_to_float(k), u2f(0x3dcccccdu)), 0.0625f), u2f(0x3fe66666u))));
            const uint32_t p = spawn_particle_2d(vel, 0, 0x1eff7f40u, 0.0f, fpu_mul(dt(), 315000.0f));
            if (p != 0) {
                sw(p + 0x20, 0);
                sw(p + 0x24, 0);
                swc1(p + 0x28, fpu_mul(dt(), -1.5f));
            }
        }
    }
    {  // the colour's pulse
        const float phase = fpu_div(static_cast<float>(ee_rem(clock, 0x96)), 150.0f);
        float angle = fpu_mul(fpu_add(phase, phase), u2f(0x40490fdbu));
        if (u2f(0x40490fdbu) < angle) angle = fpu_sub(angle, u2f(0x40c90fdbu));
        sw(m + 0x90, rgba_lerp(0x80464646u, 0x80828282u, fpu_mul(fpu_add(sin_vu(angle), 1.0f), 0.5f)));
    }
    tick32(L10_ADDR(0x001618a8));
    const int32_t left = lw(L10_ADDR(0x001618a8));
    if (left != 0) {
        const int32_t total = lw(L10_ADDR(0x001618a4));
        float rate = u2f(0x3b03126fu);
        if (!(total < left)) rate = fpu_add(fpu_mul(fpu_div(int_to_float(left), int_to_float(total)), u2f(0x3c03126eu)), u2f(0x3b03126fu));
        cam0_set_rates(rate, u2f(0x3e4ccccdu));
        cam0_set_flags3();
        cam0_set_flag116();
        cam0_set_mode(0);
    }
    ee_stack_free(0x100);
}

// Class 18: the O2 Mask on its stand. State 0: its glow set up (helmet_init_fx); gone at once when the game's byte
// 0x13d504 says it was taken, else lifted half a unit and shrunk to two thirds; 1 spins (a quarter turn a frame time),
// its panel lights blink, and with the hero within 3 (in the plane) and 2 (in height), alive, it starts scene 2 (hidden
// meanwhile); 2 once the scene is over: the message, the item (0x1c), the checkpoint at its pvar +4's moby, a level
// change request; 3 goes.
// orig: L10:0x00298668
void orx_class18_update_l10(uint32_t m) {
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(dt(), u2f(0x3fc90fdbu))));
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        helmet_init_fx(m);
        if (lbu(0x0013d504) != 0) {
            moby_retire(m);
            break;
        }
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.5f));
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3f2ac083u)));
        break;
    case 1:
        panel_lights(m, 0x10);
        if (!(vec2_distance(V(m + 0x10), V(HERO_POS)) < 3.0f)) break;
        if (!(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(HERO_POS + 8))) < 2.0f)) break;
        if (lwu(0x001415f8) == 0) break;  // (the hero's health)
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        cutscene_start(2);
        sb(m + 0x20, 2);
        break;
    case 2: {
        if (lw(OV(0x0015f5c4)) == 2) break;  // (the scene still playing)
        sub_00263d40(TXT(0x271b), -1);
        sw(OV(0x0015f640), 0xb4);
        item_give(0x1c, 1);
        const int32_t point = lw(pv + 4);
        if (point != -1) checkpoint_save(moby_at(point) + 0x10, moby_at(point) + 0x40);
        level_change_request(0, -1);
        sb(m + 0x20, 3);
        break;
    }
    case 3:
        moby_retire(m);
        break;
    default:
        break;
    }
}

}  // namespace rac1
