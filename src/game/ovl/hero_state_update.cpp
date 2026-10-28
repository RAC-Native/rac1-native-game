// Ported overlay code: the per-frame update of the hero's current state (0x217970).
#include <game/enhance/advanced.h>
#include <cstring>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;

float fs6c() { return lwc1(0x0015ed6c); }
float fs70() { return lwc1(0x0015ed70); }

// the velocity follow-up of the grounded states: on a moving platform (+0x30e) blend +0xe0 into
// the platform velocity +0x120 and settle (0x2121c0), else damp it towards rest
// orig: 0x00217970 (shared tails 0x2211a4 / 0x221264)
void ground_end() {
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x120, fs70() * 25.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// standing still: brake at edges, read the stick, no speed, settle the ground velocity
// (with `drift`, 0x216710 too)
// orig: 0x00217970 (blocks 0x21f7e8 / 0x220e9c / 0x2201ec)
void stand_update(bool drift) {
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    hero_stick_move(0, 1.0f);
    swc1(P + 0x190, 0.0f);
    sub_00212088(0.0f, fs70() * u2f(0x4149999au));
    vec_clear(V(P + 0xe0));
    if (drift) sub_00216710(fs70() * u2f(0x4149999au));
    hero_wall_slide(0);
    ground_end();
}

// airborne drag: shrink the horizontal velocity by a frame-scaled factor (less on a platform),
// no vertical speed, then ease towards the platform velocity +0x110 or towards rest
// orig: 0x00217970 (blocks 0x21ff08 / 0x2211d8)
void air_drift() {
    const float len = vec2_length(V(P + 0xe0));
    const float k = lwc1(0x0015ed60) * (lh(P + 0x30e) != 0 ? u2f(0xbc23d700u) : u2f(0xbda3d708u));
    vec2_clamp_length(V(P + 0xe0), V(P + 0xe0), k * len + len);
    sw(P + 0xe8, 0);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x110, fs70() * 24.0f);
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, u2f(0x3b83126fu));
    }
}

// swimming-style steer: stick at `stick`, horizontal velocity unless scripted (+0x1c4), ease
// towards `ease`, fall no faster than `fall`
// orig: 0x00217970 (blocks 0x21fcc4 / 0x220f9c)
void paddle_update(float stick, float ease, float fall) {
    hero_stick_move(0, fs6c() * stick);
    if (lw(P + 0x1c4) == 0) hero_horizontal_velocity();
    sub_002334d0(P + 0xe0, P + 0xe0, fs70() * ease);
    const float lim = fs6c() * fall;
    if (lwc1(P + 0xe8) < lim) swc1(P + 0xe8, lim);
}

// orig: 0x00217970 (blocks 0x21fc6c / 0x220f44)
void glide_update() {
    swc1(P + 0x180, lwc1(P + 0x2ec));
    swc1(P + 0x190, fs6c() * 5.5f);
    sub_00213e68(fs70() * 25.0f);
    sub_002334d0(P + 0xe0, P + 0x120, fs70() * 18.0f);
}

// The standing / idle family (states 0, 1, 3, 4, 0x17, 0x1d..0x1f, 0x3d, 0x70..0x72, 0x80):
// smoke, footing, the look-around of 1 / 0x1e, the turn-in-place animation of 4 / 0x17, the item
// idle of 3, then speed and the ground velocity follow-up.
// orig: 0x00217970 (block 0x217a00)
void idle_update() {
    if (lbu(P + 0x20a9) != 0) sub_00209540(0xf, 0x1e);
    if (lbu(P + 0x12e2) != 0) {
        sub_002132b8(u2f(0x3f333333u), fs6c() * 0.0f);
    } else {
        sub_002132b8(0.5f, fs6c() + fs6c());
    }
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    hero_stick_move(0, 1.0f);
    const uint32_t st = lwu(P + 0x2084);
    if ((st == 1 || st == 0x1e) && lbu(P + 0x20a5) != 0) {
        const Vec4 ax{1.0f, 0.0f, 0.0f, 0.0f};
        mat3_transform(V(P + 0x170), ax, M(OV(0x00166fd0)));
        swc1(P + 0x180, lwc1(OV(0x00166dd8)));
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k, k * 0.0f, fs6c() * u2f(0x432e886eu));
        const uint32_t T = OV(0x00166c00u - 0x7578);  // the look-around timer, +4 its count
        if (lw(T) > 0 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(T))) != 0) {
            hero_stat_use(0x00141848 + 0x90);
            sw(T, 0xffffffffu);
            sw(T + 4, lwu(T + 4) + 1);
        }
    }
    if (lw(P + 0x2084) == 3 && hero_item_idle() != 0) return;
    const uint32_t s2 = lwu(P + 0x2084);
    const bool turning = 0.2f < lwc1(P + 0x229c) || lbu(lwu(P + 0x2080) + 0x53) - 0xeu < 2u;
    if ((s2 == 4 || s2 == 0x17) && turning) {
        if (lw(P + 0x2084) == 4) {
            const float k = lwc1(0x0015ed64);
            sub_00211e30(0, k * u2f(0x3b03126fu), k * u2f(0x3d8f5c29u), fs6c() * u2f(0x40df66f3u));
        } else if (lw(P + 0x2084) == 0x17) {
            sub_00211e98();
        }
        if (fs6c() * u2f(0x3eb2b8c2u) < abs_f(lwc1(P + 0x184))) {
            const uint32_t set = (lwc1(P + 0x184) < 0.0f ? 1u : 0u) + 0xe;
            if (lbu(lwu(P + 0x2080) + 0x53) != set) hero_anim_set(set, 0, static_cast<float>(frame_scale_i(5)));
            const float lim = u2f(0x3f0ccccdu);
            const float r = fpu_div(abs_f(lwc1(P + 0x184)), fs6c() * u2f(0x40543b67u));
            swc1(P + 0xa90, r);
            if (r < lim) swc1(P + 0xa90, lim);
            if (2.5f < lwc1(P + 0xa90)) swc1(P + 0xa90, 2.5f);
        } else if (lwc1(P + 0x184) == 0.0f) {
            uint32_t set = 0xd;
            if (lw(P + 0x2084) == 0x17) set = sub_00205000(0);
            if (lbu(lwu(P + 0x2080) + 0x53) != set) hero_anim_set(set, 0, static_cast<float>(frame_scale_i(7)));
        }
    }
    swc1(P + 0x190, 0.0f);
    float speed;
    if (lw(P + 0x2084) == 3) {
        float f = lwc1(P + 0x16c) + 1.0f;
        if (1.0f < f) f = 1.0f;
        speed = fs70() * 12.0f * f;
        swc1(P + 0x2290, speed);
        const uint32_t hm = lwu(P + 0x2080);
        const uint32_t a = lbu(hm + 0x52);
        if (a == lbu(hm + 0x53) && a != 0x14) {
            const float r = fpu_div(lwc1(P + 0x164), speed);
            if (static_cast<float>(frame_scale_i(5)) < r && static_cast<float>(lbu(lwu(P + 0x2080) + 0x51)) < 5.5f) {
                sub_00208f20(13.0f, r, 0.5f, -1.0f);
                if (u2f(0x3f8ccccdu) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x3f8ccccdu));
            } else if (6.5f < static_cast<float>(lbu(lwu(P + 0x2080) + 0x51))) {
                swc1(P + 0xa90, 1.0f);
            }
        }
    } else if (lw(P + 0x2084) == 4) {
        speed = fs70() * (lbu(P + 0x12e2) != 0 ? u2f(0x3fd9999au) : u2f(0x40cf5c29u));
    } else {
        speed = fs70() * u2f(0x4149999au);
    }
    sub_00212088(0.0f, speed);
    vec_clear(V(P + 0xe0));
    sub_00216710(speed);
    if (lbu(P + 0x12e7) != 0 && lbu(0x0013d4dc) != 0) {
        if (0.2f < lwc1(P + 0x2dc)) {
            sub_002334d0(P + 0xe0, P + 0x110, fs70() * 24.0f);
        } else {
            sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
            if (lw(P + 0x2fc) != 0) {
                V(P + 0x80) = V(P + 0x2a0);
                swc1(P + 0x2dc, 0.0f);
                sh(P + 0x30e, 0);
            }
        }
    } else {
        hero_wall_slide(0);
        ground_end();
    }
    if (lw(P + 0x22f4) != 0) sub_00217450();
}

// Running (states 2 / 0x73): dust and smoke by speed, skid (+0x3b8) and sharp-turn (+0x3be)
// flags, speed from the stick, the turn spring (or the strafe turn 0x211f38 at full stick), lean,
// then speed limits and the ground velocity follow-up.
// orig: 0x00217970 (block 0x21bf50)
void run_update() {
    const bool riding = lbu(P + 0x12e4) != 0 && lwc1(P + 0x22a4) < 0.25f;
    if (lbu(P + 0x20a9) != 0 || riding) {
        if (fs6c() < lwc1(P + 0x164)) sub_00209638(0, 2);
        if (fs6c() * 0.5f < lwc1(P + 0x164)) sub_00209848(2, 4);
        hero_smoke_puffs(5, 2);
    }
    if (advanced_rac3_movement()) {  // enhancement: R&C 3's "move along the stick" rule
        advanced_rac3_along_stick();
    } else {
        if (lwc1(P + 0x164) < fs6c() * u2f(0x3e99999au) && u2f(0x3f59999au) < lwc1(P + 0x229c)) sh(P + 0x3b8, 1);
        if (abs_f(lwc1(P + 0x188)) < u2f(0x3d567750u) || lbu(P + 0x20a8) != 0 || lw(P + 0x2088) == 1) sh(P + 0x3b8, 0);
    }
    if (lh(P + 0x3be) != 0 && (abs_f(lwc1(P + 0x188)) < u2f(0x3d8efa35u) || lbu(P + 0x20a8) != 0)) sh(P + 0x3be, 0);
    hero_stick_speed();
    if (0.0f < lwc1(P + 0x930)) {
        const float k = u2f(0x3f333333u);
        sub_002132b8(k, 0.0f);
        const Vec4 push = V(P + 0x920);
        const float v = lwc1(P + 0x190) + sub_00213350_v(push);
        swc1(P + 0x190, v);
        if (v < fs6c() * k) swc1(P + 0x190, fs6c() * k);
        vec_clear(V(P + 0x920));
    }
    float slow = 1.0f;
    if (advanced_rac3_movement()) {  // enhancement: R&C 3's turn springs and speed rules
        slow = advanced_rac3_run_turn();
    } else if (lw(P + 0x2088) == 1 && lh(P + 0x3be) == 0) {
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k * u2f(0x3c03126fu), k * u2f(0x3e19999au), fs6c() * u2f(0x411f2c8du));
    } else {
        const float stick = lwc1(P + 0x229c), turn = lwc1(P + 0x188);
        if (lbu(P + 0x20a8) != 0) {
            slow = 1.0f - turn * u2f(0x3f0ccccdu);
            if (slow < 0.0f) slow = 0.0f;
        } else if (stick < u2f(0x3f4ccccdu) && u2f(0x3f060a92u) < turn) {
            slow = 0.0f;
        }
        if (lwc1(OV(0x0017bdc4)) < stick) {
            sub_00211f38();
        } else {
            const float k = lwc1(0x0015ed64), g = lwc1(P + 0x229c) + u2f(0x3eb33333u);
            sub_00211e30(0, k * u2f(0x3ba3d70au) * g, k * u2f(0x3dcccccdu), fs6c() * u2f(0x4096cbe4u) * g);
        }
    }
    hero_run_lean();
    sub_00215ef8();
    const float up = fs70() * (advanced_rac3_movement() ? 37.0f : 7.5f) * slow;  // (enhancement: R&C 3's 37 / 30)
    float down = fs70() * (advanced_rac3_movement() ? 30.0f : 8.5f);
    if (lh(P + 0x3be) != 0) {
        float f = u2f(0x40278d36u) - lwc1(P + 0x188);
        if (1.0f < f) f = 1.0f;
        if (f < u2f(0x3e4ccccdu)) f = u2f(0x3e4ccccdu);
        swc1(P + 0x190, lwc1(P + 0x190) * f);
        down = fs70() * 17.0f;
    }
    sub_00212088(up, down);
    if (lh(P + 0x3b8) != 0 && lh(P + 0x308) == 0) {
        sub_002120d8(lwc1(P + 0x180));
    } else {
        sub_002120d8(99999.0f);
    }
    hero_wall_slide(1);
    if (lh(P + 0x1f4) != 0) hero_edge_brake(5.0f, u2f(0x3e4ccccdu));
    ground_end();
}

// The jump family (states 7, 9..0x12, 0x1c, 0x3c, 0x49, 0x4c, 0x5e, 0x69): air steering, the
// take-off point +0x400, horizontal velocity while rising or the air turn otherwise, the
// per-jump boosts (9, 0xa) and ledge probe, the apex height +0x410 and apex flag +0x41e, lean,
// the trail of 0xd / 0x10 and the shove of 0x10, then gravity and the wall check.
// orig: 0x00217970 (block 0x21ead0)
void jump_update() {
    if (lw(P + 0x2084) == 0x1c && lh(P + 0x22d8) == 0 && lw(P + 0x11a8) == 3 && sub_00211870(10.0f) != 0) {
        hero_sound_start(4, 0);
    }
    if (lh(P + 0x4a8) != 0 && lbu(P + 0x12e2) != 0) {
        swc1(P + 0x190, lwc1(P + 0x194));
    } else {
        if (lw(P + 0x418) != 0) hero_stick_move(0, fs6c() * u2f(0x40b66666u));
        hero_stick_move(0, lwc1(P + 0x424));
    }
    if (lw(P + 0x2084) == 0x12) {
        if (lw(P + 0x198) == frame_scale_i(0x14)) {
            hero_land_fx(3, 0x10, 1);
            if (lwu(OV(0x001612d0)) != 0) {
                region_height_splash(lwu(OV(0x001612d0)), lw(OV(0x001612d8)), 0, lwc1(P + 0x80), lwc1(P + 0x84), u2f(0x3ecccccdu), u2f(0x3eb33333u));
            }
        }
        const float t = lwc1(P + 0xaa8);
        if (4.0f <= t && t <= 7.0f) hero_smoke_puffs(7, 0);
    }
    const int32_t time = lw(P + 0x198), rise = lw(P + 0x420);
    if (time == rise) V(P + 0x400) = V(P + 0x80);
    bool push = false;
    if (lw(P + 0x418) == 0) {
        if (!(time < rise)) {
            push = true;
        } else {
            const uint32_t st = lwu(P + 0x2084);
            push = st == 0xb || st == 0x10 || st == 0xc || st == 0x11 || st == 0x1c || st == 0x4c;
        }
    }
    if (push) {
        hero_horizontal_velocity();
        if (lw(P + 0x2084) == 0x12) {
            float len = vec2_length(V(P + 0xe0));
            const float lim = fs6c() * u2f(0x406ccccdu);
            if (lim < len) len = lim;
            vec2_set_length(V(P + 0xe0), V(P + 0xe0), len);
        }
    } else if (lh(P + 0x4a8) != 0) {
        hero_slide_move();
        if (lw(P + 0x2084) == 0xa && lw(P + 0x198) < frame_scale_i(10) && lwc1(P + 0x168) < fs6c() * 7.0f) {
            const float k = u2f(0x3f333333u);
            swc1(P + 0xe0, lwc1(P + 0xe0) + cos_vu(lwc1(P + 0x98)) * (fs6c() * k));
            swc1(P + 0xe4, lwc1(P + 0xe4) + sin_vu(lwc1(P + 0x98)) * (fs6c() * k));
            V(P + 0x130) = V(P + 0xe0);
        }
    } else {
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k * u2f(0x3c75c28fu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
        const float spd = lwc1(P + 0x194);
        if (!(lw(P + 0x418) < 2)) swc1(P + 0x194, spd * u2f(0x3ecccccdu));
        sub_002120d8(99999.0f);
        swc1(P + 0x194, spd);
        if (lw(P + 0x418) != 1) sub_00212088(lwc1(P + 0x480), lwc1(P + 0x484));
        if (lh(P + 0x308) != 0) swc1(P + 0x194, 0.0f);
        if (lw(P + 0x418) != 0) {
            if (lwu(0x0013cb14) != 0 && lh(P + 0x30a) != 0) swc1(P + 0x194, 0.0f);
            if (lw(P + 0x418) != 0) sub_00216710(lwc1(P + 0x484));
        }
        sub_002132b8(0.5f, fs6c() + fs6c());
        if (lw(P + 0x2084) == 9 && lw(P + 0x198) < lw(P + 0x420) + frame_scale_i(4)) {
            sub_00233708(P + 0xe0, P + 0xe0, fs70() * 16.0f * static_cast<float>(lw(P + 0x198)));
        }
        if (lw(P + 0x2084) == 0xa && lw(P + 0x198) < frame_scale_i(0x22)) {
            sub_00233708(P + 0xe0, P + 0xe0, fs70() * 53.0f * static_cast<float>(lw(P + 0x198)));
        }
        if (lw(P + 0x198) < frame_scale_i(10) && 0.2f < lwc1(P + 0x229c)) {  // a ledge just ahead
            Vec4 a = V(P + 0x80);
            sub_00233810_v(a, a, 0.5f);
            Vec4 b = a;
            const float k2 = u2f(0x3f59999au);
            b.x = b.x + cos_vu(lwc1(P + 0x98)) * k2;
            b.y = b.y + sin_vu(lwc1(P + 0x98)) * k2;
            if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) != 0) sh(P + 0x4a4, 1);
        }
    }
    const float floor = lwc1(P + 0x2d8), apex = lwc1(P + 0x410);
    if (apex < floor) {
        const float up = apex + fs6c() * 6.0f;
        swc1(P + 0x410, up);
        if (floor < up) swc1(P + 0x410, floor);
    }
    if (lwc1(P + 0x88) < lwc1(P + 0x410)) swc1(P + 0x410, lwc1(P + 0x88));
    if (lw(P + 0x420) < lw(P + 0x198) && sub_00233a78_v(V(P + 0xe0)) < u2f(0x3a83126fu)) {
        if (lh(P + 0x41c) == 0 || lw(P + 0x3d4) < lw(P + 0x198)) {
            sh(P + 0x41e, 1);
            if (lwc1(P + 0x3f8) != 0.0f) swc1(P + 0x4a0, lwc1(P + 0x3f8));
        }
    }
    if (lbu(P + 0x20a4) == 0) hero_run_lean();
    if (lbu(P + 0x20a4) == 1) hero_body_lean();
    if (lw(P + 0x2084) == 0x10 || lw(P + 0x2084) == 0xd) {
        trail_update(P + 0x1670, (frame_scale_i(0xc) < lw(P + 0x198) ? 1 : 0) << 1);
    }
    if (lw(P + 0x2084) == 0x10 && frame_scale_i(8) < lw(P + 0x198) && lwc1(P + 0xaa8) < 28.0f) {
        Vec4 c;
        sub_00233660_v(c, u2f(0x3f4ccccdu), 0.0f, u2f(0x3e99999au));
        const int32_t n = mobys_in_sphere_v(c, u2f(0x3f666666u), 0, lwu(P + 0x2080), nullptr);
        if (n != 0) {
            Vec4 d;
            vec3_scale(d, V(P + 0x100), 7.0f);
            for (int32_t i = 0; i < n; ++i) {
                const uint32_t m = lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4);
                if (moby_in_range_class_500(m) != 0) sub_00259bc8_v(lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4), lwu(P + 0x2080), 0x10000, P + 0x80, d, 1.0f);
            }
        }
    }
    if (lw(P + 0x2084) == 0x3c && lh(P + 0x41e) == 0 && fs6c() * 1.5f < lwc1(P + 0xe8)) hero_smoke_fx();
    hero_vertical_velocity();
    hero_air_wall_check();
}

// stop the hero sound held in slot P+off (when it still plays on the hero moby) and free it
// orig: 0x00217970 (inlined, e.g. 0x21e8e0)
void sound_slot_stop(uint32_t off) {
    const int32_t s = lw(P + off);
    if (s != -1) {
        const uint32_t e = 0x0013e550 + static_cast<uint32_t>(s * 0x70);
        if (lwu(e + 0x88) == lwu(P + 0x2080) && lbu(e + 0x74) != 0) sub_0028d918(s);
    }
    sw(P + off, 0xffffffffu);
}

// the shared end of states 6 / 0x2d / 0x77: ease the velocity by +0x940 and cap the fall at
// -50 vm
// orig: 0x00217970 (0x21e5d8)
void slide_end() {
    sub_002334d0(P + 0xe0, P + 0xe0, lwc1(P + 0x940));
    const float lim = -(fs6c() * 50.0f);
    if (sub_00233a78_v(V(P + 0xe0)) < lim) sub_00233b20(P + 0xe0, P + 0xe0, lim);
}

// Sliding (states 6 / 0x2d; 0x2d also tilts the hero moby by the slope +0x2dc and flags the
// steep part in +0x2088).
// orig: 0x00217970 (block 0x21e3f8)
void slide_update() {
    if (lw(P + 0x2084) == 0x2d) {
        const float target = 4.0f < lwc1(P + 0x2dc) ? u2f(0x3edf66f3u) : 0.0f;
        Vec4 r = V(P + 0x90);
        const float k = lwc1(0x0015ed64), k1 = u2f(0x3ca3d70au), k2 = u2f(0x3e99999au), k3 = u2f(0x40278d36u);
        spring_angle_v(r.y, P + 0xa4, 0, target, k * k1, k * k2, fs6c() * k3);
        const float kk = lwc1(0x0015ed64);
        spring_angle_v(r.x, P + 0xa0, 0, 0.0f, kk * k1, kk * k2, fs6c() * k3);
        sub_002091d8(lwc1(P + 0xa0), lwc1(P + 0xa4), 0.0f);
        if (1.0f < lwc1(P + 0x2dc)) sw(P + 0x2088, 1);
    }
    hero_stick_speed();
    if (lw(P + 0x1c4) == 0) hero_horizontal_velocity();
    hero_run_lean();
    sub_002132b8(u2f(0x3f333333u), fs6c() * 0.0f);
    slide_end();
}

// Sliding to a stop (state 0x77): ease the hero moby's tilt +0x948 / +0x94c towards +0x950 /
// +0x954, drag the velocity.
// orig: 0x00217970 (block 0x21e544)
void slide_stop_update() {
    const float k = u2f(0x410ba058u);
    sb(P + 0x20ac, 1);
    approach_f(P + 0x948, lwc1(P + 0x950), fs70() * k);
    approach_f(P + 0x94c, lwc1(P + 0x954), fs70() * k);
    sub_002091d8(lwc1(P + 0x948), lwc1(P + 0x94c), 0.0f);
    const float len = vec2_length(V(P + 0xe0));
    vec2_clamp_length(V(P + 0xe0), V(P + 0xe0), lwc1(0x0015ed60) * u2f(0xbdccccd0u) * len + len);
    slide_end();
}

// Crouch walk (state 0x81): slow stick move, the turn spring or (camera strafe held) the turn
// towards the camera heading, speed by stick and footing; in deep water (+0x2dc > 0.7) float at
// the surface, else ease the height to the floor +0x2d8.
// orig: 0x00217970 (block 0x21e628)
void crouch_update() {
    hero_sound_slot(4, lwu(P + 0x2080), 0x12);
    const bool deep = u2f(0x3f333333u) < lwc1(P + 0x2dc);
    const float slow = fs6c() * 4.0f;
    hero_stick_move(0, lwc1(P + 0x229c) * (deep ? slow : fs6c() * 5.75f));
    constexpr uint32_t PAD = 0x0013c940;
    const float stick = lwc1(P + 0x229c);
    if (0.2f < stick && (lw(PAD + 0x1a0) & 3) == 0) {
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k * u2f(0x3cdd2f1bu), k * u2f(0x3e99999au), fs6c() * u2f(0x40bde44eu) * stick);
    }
    if ((lw(PAD + 0x1a0) & 3) != 0) {
        const float k = lwc1(0x0015ed64);
        const float keep = lwc1(P + 0x180);
        swc1(P + 0x180, lwc1(OV(0x00166dd8)));
        sub_00211e30(0, k * u2f(0x3c8b4396u), k * u2f(0x3e99999au), fs6c() * u2f(0x4096cbe4u));
        swc1(P + 0x180, keep);
    }
    const float s = lwc1(P + 0x229c), f = fs70();
    float spd = s < 0.2f ? f * 3.5f : f * 7.0f;
    if (lbu(P + 0x12e2) != 0) spd = s < 0.2f ? f * u2f(0x3f99999au) : f * u2f(0x406ccccdu);
    sub_00213e68(spd);
    hero_run_lean();
    hero_wall_slide(1);
    if (deep) {
        sub_002338d0(P + 0xe0, P + 0xe0);
        sub_002334d0(P + 0xe0, P + 0xe0, slow * u2f(0x3f3851ecu));
        swc1(P + 0x304, lwc1(P + 0xe8));
    } else {
        const float k = lwc1(0x0015ed64);
        spring_to(P + 0x88, P + 0x304, lwc1(P + 0x2d8) + u2f(0x3e2e147bu), k * u2f(0x3ca3d70au), k * u2f(0x3eb33333u), fs6c() * 2.5f);
    }
}

// Hanging / hand-over-hand (state 8): the hold sound by the item in slot 3, stick move, turn,
// speed, then float the velocity and settle the height +0x410 down to the hero.
// orig: 0x00217970 (block 0x21e8cc)
void hang_update() {
    if (sub_0020d498(3) == 2) {
        sound_slot_stop(0x2228);
        hero_sound_slot(3, lwu(P + 0x2080), 2);
    } else {
        sound_slot_stop(0x2224);
        hero_sound_slot(4, lwu(P + 0x2080), 0x12);
    }
    const float sp = fs6c() * (lw(P + 0x11a8) == 3 ? 5.0f : 3.0f);
    hero_stick_move(0, sp);
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3ccccccdu), k * u2f(0x3e99999au), fs6c() * u2f(0x41490fdbu));
    sub_00212088(fs70() * 15.0f, fs70() * 7.0f);
    sub_002120d8(99999.0f);
    hero_run_lean();
    sub_002338d0(P + 0xe0, P + 0xe0);
    sub_002334d0(P + 0xe0, P + 0xe0, sp * u2f(0x3f3851ecu));
    if (lwc1(P + 0x88) < lwc1(P + 0x410)) approach_f(P + 0x410, lwc1(P + 0x88), fs6c() * 4.0f);
    hero_air_wall_check();
}

// the wrench slam's approach: chase the locked target (+0xa54) at 4.5 between 2 and 5 units,
// else keep the heading; `ease` is the speed easing
// orig: 0x00217970 (block 0x21a92c)
void slam_chase(float ease) {
    if (lwu(P + 0xa54) != 0) {
        const float d = vec2_distance(V(P + 0x80), V(lwu(P + 0xa54) + 0x10));
        if (2.0f < d && d < 5.0f) swc1(P + 0x190, fs6c() * 4.5f);
        sub_00212088(fs70() * ease, fs70() * ease);
        swc1(P + 0xe0, cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x194));
        swc1(P + 0xe4, sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x194));
    }
}

// The wrench slam (state 0x14): combo window flag +0x10a8, approach and turn to the target, the
// lift (eased to a height-dependent speed) then the dive, the ground hit through the moby
// sphere test (record class 0x47, impulse 2) when landing on a platform.
// orig: 0x00217970 (block 0x21a92c)
void slam_update() {
    if (lw(P + 0xa9c) != 0 || lwc1(P + 0xaa8) < static_cast<float>(lw(OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c + 0x18))) sh(P + 0x10a8, 2);
    swc1(P + 0x190, 0.0f);
    if (lbu(P + 0x12e2) != 0) {
        if (lwu(P + 0xa54) != 0) {
            slam_chase(4.0f);
        } else {
            sub_00212088(fs70() + fs70(), fs70() + fs70());
            sub_00233ba0(P + 0xe0, P + 0xe0, lwc1(P + 0x194));
        }
    } else if (lwu(P + 0xa54) != 0) {
        slam_chase(25.0f);
    } else {
        sub_00212088(fs70() * 15.0f, fs70() * 15.0f);
        sub_00233ba0(P + 0xe0, P + 0xe0, lwc1(P + 0x194));
    }
    if (lw(P + 0xa58) != 0 || lwu(P + 0xa54) != 0) {
        const float k = lwc1(0x0015ed64);
        swc1(P + 0x180, lwc1(P + 0xa5c));
        sub_00211e30(0, k * u2f(0x3d4ccccdu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
    } else if (lwc1(P + 0xaa8) < 25.0f) {
        hero_lock_heading(8.0f, u2f(0x3f860a92u), u2f(0x3f860a92u));
    }
    const int32_t lift = frame_scale_i(9);
    float up = fs6c() * u2f(0x40f66666u);
    const float depth = lwc1(P + 0x2dc);
    if (2.25f < depth) {
        up = up * 0.5f;
    } else if (u2f(0x3fe66666u) < depth) {
        up = up * u2f(0x3f333333u);
    }
    const float rate = fs70() * 150.0f;
    const int32_t dive = frame_scale_i(0xe);
    if (lw(P + 0xa9c) == 0 && 23.5f < lwc1(P + 0xaa8) && lwc1(P + 0x108) < 0.0f && lh(P + 0x30e) != 0) {
        Vec4 c = V(P + 0x80);
        sub_002336e8_v(c, c, -0.5f);
        Vec4 dir = {};
        dir.x = cos_vu(lwc1(P + 0x98)) * 2.0f;
        dir.y = sin_vu(lwc1(P + 0x98)) * 2.0f;
        alignas(16) uint32_t rec[12] = {};
        std::memcpy(rec, &dir, 16);
        rec[4] = lwu(P + 0x2080);
        rec[5] = 0x10000;
        const float two = 2.0f;
        std::memcpy(&rec[7], &two, 4);
        rec[8] = 1;
        rec[3] = 0x45afdf66u;
        rec[6] = 0u | (1u << 8) | (0x47u << 16);
        mobys_in_sphere_v(c, u2f(0x3ecccccdu), 0x10, lwu(P + 0x2080), rec);
    }
    sub_00211870(27.0f);
    if (lw(P + 0x198) < lift) {
        float v = sub_00233a78_v(V(P + 0xe0));
        approach_f_v(v, up, rate);
        sub_00233b20(P + 0xe0, P + 0xe0, v);
    } else if (dive < lw(P + 0x198)) {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 130.0f);
        if (sub_00233a78_v(V(P + 0xe0)) < 0.0f) swc1(P + 0xa70, 1.0f);
        const float lim = fs6c() * -37.0f;
        if (sub_00233a78_v(V(P + 0xe0)) < lim) sub_00233b20(P + 0xe0, P + 0xe0, lim);
    }
}

// the attack states' ground follow-up: ease towards the platform or rest, no slope push
// orig: 0x00217970 (0x221264 / 0x22071c)
void attack_ground_end() {
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x120, fs70() * 25.0f);
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// the combo window: +0x10a8 = 2 while the swing is before the step's frame `off` of table
// 0x17bc28 (or the swing is scripted, +0xa9c)
// orig: 0x00217970 (blocks 0x21ae00 / 0x21b1d0)
void combo_window(uint32_t off) {
    if (lw(P + 0xa9c) != 0 || lwc1(P + 0xaa8) < static_cast<float>(lw(OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c + off))) sh(P + 0x10a8, 2);
}

// turn to the locked target with the attack spring (a heading clamp when `soft`)
// orig: 0x00217970 (blocks 0x21ae00 / 0x21b1d0)
void attack_turn(int32_t soft) {
    const float k = lwc1(0x0015ed64);
    swc1(P + 0x180, lwc1(P + 0xa5c));
    sub_00211e30(soft, k * u2f(0x3d4ccccdu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
}

// The wrench spin (state 0x15): combo window, turning to the target unless chained from state 1
// early, the throw point (0x217658) at frame 33, then on footing a back hop at frame 84 or the
// ground speed limits.
// orig: 0x00217970 (block 0x21ae00)
void spin_update() {
    swc1(P + 0x190, 0.0f);
    combo_window(0x18);
    bool chained = false;
    if (lw(P + 0x2090) == 1) chained = frame_scale_i(0x14) < lw(P + 0x2098);
    if (!chained) {
        if (lw(P + 0xa58) != 0 || lwu(P + 0xa54) != 0) {
            int32_t soft = 0;
            if (lh(P + 0x308) == 0) {
                const float diff = angle_sub(lwc1(P + 0xa5c), lwc1(P + 0x98));
                const float d = sub_0025bb38(1, lwc1(P + 0xa5c), lwc1(P + 0x98));
                soft = u2f(0x408ba058u) < d ? 0 : 1;
                if (0.0f < diff) swc1(P + 0xa90, fpu_div(1.0f, d + 1.0f));
            }
            attack_turn(soft);
        } else {
            const bool aim = lw(P + 0xa9c) != 0 || !(6.0f < lwc1(P + 0xaa8));
            if (aim && lbu(P + 0x20b3) == 0) hero_lock_heading(14.0f, u2f(0x3f060a92u), u2f(0x3f490fdbu));
        }
    }
    if (lw(P + 0xa9c) == 0 && sub_00211870(33.0f) != 0) {
        if (lbu(P + 0x12e2) != 0 && lwc1(P + 0x160) < fs6c() + fs6c()) {
            const float k = u2f(0x3fa66666u);
            swc1(P + 0xe0, lwc1(P + 0xe0) + cos_vu(lwc1(P + 0x98)) * (fs6c() * k));
            swc1(P + 0xe4, lwc1(P + 0xe4) + sin_vu(lwc1(P + 0x98)) * (fs6c() * k));
            V(P + 0x130) = V(P + 0xe0);
        }
        hero_hold_moby_start();
    }
    if (lbu(P + 0x12e2) != 0) {
        if (sub_00211870(84.0f) != 0) {
            const float a = angle_add(lwc1(P + 0x98), u2f(0x40490fdbu));
            float sp = fs6c() * 2.5f;
            if ((lw(0x0013cae0) & 0xa) != 0) sp = fs6c() * 3.0f;
            swc1(P + 0xe0, lwc1(P + 0xe0) + cos_vu(a) * sp);
            swc1(P + 0xe4, lwc1(P + 0xe4) + sin_vu(a) * sp);
            V(P + 0x130) = V(P + 0xe0);
        }
        hero_slide_move();
        return;
    }
    sub_00212088(fs70() * 37.0f, fs70() * 28.0f);
    sub_002120d8(99999.0f);
    attack_ground_end();
}

// The wrench combo swing (state 0x13): on footing just slide; else the combo window, a lunge
// towards the locked target in the first frames (+0xa6c), the swing speed by step and frame,
// the animation rate +0xa90 eased to 1, the turn to the target, speed limits, ground follow-up.
// orig: 0x00217970 (block 0x21b1d0)
void swing_update() {
    if (lbu(P + 0x12e2) != 0) {
        hero_slide_move();
        return;
    }
    combo_window(0x20);
    const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
    swc1(P + 0x190, 0.0f);
    if (lwu(P + 0xa54) != 0 && lw(P + 0x198) < frame_scale_i(4)) {
        float d = vec2_distance(V(lwu(P + 0xa54) + 0x10), V(P + 0x80)) - 0.5f;
        if (d < 0.0f) d = 0.0f;
        float reach, slack, cap;
        if (lw(e + 4) == 0) {
            reach = 0.75f; slack = u2f(0x3f4ccccdu); cap = 4.0f;
        } else if (lw(e + 4) == 1) {
            reach = u2f(0x3f19999au); slack = u2f(0x3fd9999au); cap = 4.0f;
        } else {
            reach = u2f(0x3fa66666u); slack = u2f(0x3fd9999au); cap = u2f(0x3ff33333u);
        }
        const float gap = d - reach;
        if (abs_f(gap) < slack + 3.0f) {
            float r = fpu_div(gap, u2f(0x3f666666u));
            if (cap < r) r = cap;
            if (1.0f < r) swc1(P + 0xa6c, r);
        }
    }
    const int32_t step = lw(e + 4);
    const float t = lwc1(P + 0xaa8);
    if (step == 0) {
        if (t < 18.0f) swc1(P + 0x190, fs6c() * u2f(0x408ccccdu) * lwc1(P + 0xa6c));
    } else if (step == 1) {
        if (lw(P + 0xa9c) != 0 || t < 9.0f) {
            const float k = 8.0f < t ? 3.0f : 7.0f < t ? 4.5f : u2f(0x40b66666u);
            swc1(P + 0x190, fs6c() * k * lwc1(P + 0xa6c));
        }
    } else if (step == 2) {
        if (lw(P + 0xa9c) != 0 || t < 12.0f) swc1(P + 0x190, fs6c() * u2f(0x406ccccdu) * lwc1(P + 0xa6c));
    }
    approach_f(P + 0xa90, 1.0f, u2f(0x3e4ccccdu));
    if (lw(P + 0xa58) != 0 || lwu(P + 0xa54) != 0) {
        if (lw(e + 4) == 1 && 0.0f < angle_sub(lwc1(P + 0xa5c), lwc1(P + 0x98))) {
            swc1(P + 0xa90, fpu_div(1.0f, angle_dist(lwc1(P + 0xa5c), lwc1(P + 0x98)) + 1.0f));
        }
        attack_turn(0);
    } else {
        const bool late = lw(e + 4) == 1 && lw(P + 0xa9c) == 0 && 7.0f < lwc1(P + 0xaa8);
        if (!late) hero_lock_heading(5.0f, u2f(0x3f490fdbu), u2f(0x3f490fdbu));
    }
    sub_00212088(fs70() * 37.0f, fs70() * 28.0f);
    sub_002120d8(lw(P + 0xa58) != 0 ? lwc1(P + 0xa5c) : 99999.0f);
    attack_ground_end();
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
}

// The backflip / side hop push (states 0x21 / 0x7a): the speed decays, the velocity follows the
// hop heading +0xa68 (rising in water), facing away from it.
// orig: 0x00217970 (block 0x21b69c)
void hop_update() {
    approach_f(P + 0x194, 0.0f, fs70() * 24.0f);
    swc1(P + 0xe0, cos_vu(lwc1(P + 0xa68)) * lwc1(P + 0x194));
    swc1(P + 0xe4, sin_vu(lwc1(P + 0xa68)) * lwc1(P + 0x194));
    if (0.0f < lwc1(P + 0x2dc)) {
        swc1(P + 0xe8, lwc1(P + 0x118) - fs70() * 25.0f);
    } else {
        swc1(P + 0xe8, 0.0f);
    }
    swc1(P + 0x180, angle_add(lwc1(P + 0xa68), u2f(0x40490fdbu)));
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3d4ccccdu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
}

// one hit sphere of the hyper-strike sweep: 0.8 ahead at `yaw`, 0.55 up
// orig: 0x00217970 (block 0x21b794)
void sweep_probe(float yaw, uint32_t ignore, const uint32_t* rec, uint32_t weapon) {
    Vec4 c = {};
    c.x = cos_vu(yaw) * u2f(0x3f4ccccdu);
    c.y = sin_vu(yaw) * u2f(0x3f4ccccdu);
    c.z = u2f(0x3f0ccccdu);
    vec3_add(c, c, V(P + 0x80));
    wrench_hit_sparks(weapon, mobys_in_sphere_v(c, u2f(0x3f4ccccdu), 0, ignore, rec));
}

// The wrench hyper strike (state 0x20): turn to the target, the lunge between frames 10 and 18,
// the trail, then on footing slide (with a forward kick) or slow down; while the lunge lasts,
// three hit spheres sweep ahead (impulse 3 up 1.3) with sparks on the wrench.
// orig: 0x00217970 (block 0x21b794)
void strike_update() {
    if (lw(P + 0xa58) != 0 || lwu(P + 0xa54) != 0) {
        const float k = lwc1(0x0015ed64);
        swc1(P + 0x180, lwc1(P + 0xa5c));
        sub_00211e30(0, k * u2f(0x3d4ccccdu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
    } else if (lw(P + 0x198) < frame_scale_i(4)) {
        hero_lock_heading(11.0f, u2f(0x3f5f66f3u), -1.0f);
    }
    const int32_t from = frame_scale_i(10);
    swc1(P + 0x190, 0.0f);
    const int32_t to = from + frame_scale_i(8);
    if (from < lw(P + 0x198) && lw(P + 0x198) < to) swc1(P + 0x190, fs6c() * 18.0f);
    if (lw(P + 0x198) == frame_scale_i(8)) {
        const uint32_t list = P + 0x1670;
        sub_00262500(lwu(P + 0x2080), list);
        sub_00262528(list, 0x30, 2);
        sub_00262528(list, 0x17, 4);
        sub_00262528(list, 0xc, 6);
    }
    trail_update(P + 0x1670, to < lw(P + 0x198) ? 5 : 0);
    if (lbu(P + 0x12e2) != 0) {
        if (from < lw(P + 0x198) && lw(P + 0x198) < from + frame_scale_i(7) && lwc1(P + 0x168) < fs6c() * 8.5f) {
            const float k = u2f(0x3f99999au);
            swc1(P + 0xe0, lwc1(P + 0xe0) + cos_vu(lwc1(P + 0x98)) * (fs6c() * k));
            swc1(P + 0xe4, lwc1(P + 0xe4) + sin_vu(lwc1(P + 0x98)) * (fs6c() * k));
            V(P + 0x130) = V(P + 0xe0);
        }
        hero_slide_move();
    } else {
        if (from < lw(P + 0x198)) sub_00212088(fs70() * 190.0f, fs70() * 90.0f);
        sub_002120d8(lw(P + 0xa58) != 0 ? lwc1(P + 0xa5c) : 99999.0f);
    }
    if (lw(P + 0x2084) == 0x20 && from < lw(P + 0x198) && lw(P + 0x198) < to + frame_scale_i(4)) {
        const uint32_t w = lwu(P + 0x1090);
        if (w != 0) {
            Vec4 d = {};
            d.x = cos_vu(lwc1(P + 0x98)) * 2.0f;
            d.y = sin_vu(lwc1(P + 0x98)) * 2.0f;
            d.z = u2f(0x3fa66666u);
            alignas(16) uint32_t rec[12] = {};
            std::memcpy(rec, &d, 16);
            rec[4] = lwu(P + 0x2080);
            rec[5] = 0x30000;
            const float three = 3.0f;
            std::memcpy(&rec[7], &three, 4);
            rec[8] = 1;
            rec[3] = 0x45afdf66u;
            rec[6] = 0u | (3u << 8) | (static_cast<uint32_t>(lhu(w + 0xa6)) << 16);
            const float yaw = lwc1(P + 0x98);
            sweep_probe(yaw, w, rec, w);
            sweep_probe(angle_add(lwc1(P + 0x98), u2f(0xbf5f66f3u)), lwu(P + 0x2080), rec, w);
            sweep_probe(angle_add(lwc1(P + 0x98), u2f(0x3f5f66f3u)), w, rec, w);
        }
    }
    if (frame_scale_i(10) < lw(P + 0x198)) hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    hero_wall_slide(1);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x110, fs70() * 24.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// The wrench throw stance (state 0x23): turn to the target (or a heading clamp), speed limits,
// stick to the platform or fall.
// orig: 0x00217970 (block 0x21bca0)
void throw_stance_update() {
    swc1(P + 0x190, 0.0f);
    if (lw(P + 0xa58) != 0) {
        const float k = lwc1(0x0015ed64);
        swc1(P + 0x180, lwc1(P + 0xa5c));
        sub_00211e30(0, k * u2f(0x3d23d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x41702845u));
    } else {
        hero_lock_heading(11.0f, u2f(0x3f5f66f3u), -1.0f);
    }
    sub_00212088(fs70() * 30.0f, fs70() * 35.0f);
    sub_002120d8(99999.0f);
    if (lh(P + 0x30e) != 0) {
        swc1(P + 0xe8, lwc1(P + 0x108) - fs70() * 25.0f);
        sub_002121c0();
    } else {
        swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 54.0f);
    }
}

// Walking on a narrow beam (state 0x3f): the speed drops as the stick turns away from the beam
// (stick direction in hero space, fully stopped past 70 degrees), turn spring, limits.
// orig: 0x00217970 (block 0x21bdb4)
void beam_update() {
    hero_stick_speed();
    Vec4 s;
    mat4_transform(s, V(P + 0x170), M(P + 0x40));
    const float a = abs_f(atan2_f(s.x, s.y));
    const float lim = u2f(0x3f9c61aau);
    if (lim < a) {
        swc1(P + 0x190, 0.0f);
    } else {
        float f = (lim - a) + (lim - a);
        if (1.0f < f) f = 1.0f;
        swc1(P + 0x190, lwc1(P + 0x190) * f);
    }
    const float k = lwc1(0x0015ed64), g = lwc1(P + 0x229c) + u2f(0x3eb33333u);
    sub_00211e30(0, k * u2f(0x3ccccccdu) * g, k * u2f(0x3e99999au), fs6c() * u2f(0x40a78d36u) * g);
    sub_00212088(fs70() * 7.5f, fs70() * 8.5f);
    sub_002120d8(99999.0f);
    if (lw(P + 0x2fc) != 0) {
        V(P + 0x80) = V(P + 0x2a0);
        sw(P + 0x2dc, 0);
        sh(P + 0x30e, 0);
    }
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x110, fs70() * 24.0f);
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// Hoverboard / skating (state 0x2f): slide, wobble parameters, then by the hero moby's
// animation: 0x37 leans the board effect by the turn rate, 0x6d tilts it along the velocity;
// float on a platform or drop velocity.
// orig: 0x00217970 (block 0x21c438)
void board_update(uint32_t hm) {
    const float k2 = u2f(0x3e4ccccdu), k04 = u2f(0x3d23d70au);
    hero_slide_move();
    fx_params_set_0(k04, k2);
    fx_params_set_1(k04, k2);
    fx_params_set_2(k04, k2);
    fx_params_set_3(u2f(0x3ca3d70au), k2);
    constexpr OvConst<0x0017a680u> FX{};
    const uint32_t anim = lbu(hm + 0x53);
    if (anim == 0x37) {
        float t = lwc1(P + 0x188);
        const float lim = u2f(0x3fb33333u);
        if (lim < t) t = lim;
        if (t < -lim) t = -lim;
        swc1(FX + 0x60, t * u2f(0xbe0f5c29u));
        swc1(FX + 0x118, t * u2f(0x3e99999au));
        swc1(FX + 0x1c0, t * u2f(0xbe6147aeu));
        swc1(FX + 0x278, t * u2f(0x3ecccccdu));
        swc1(FX + 0x274, abs_f(t) * u2f(0xbdb851ecu));
        swc1(FX + 0x270, abs_f(t) * u2f(0x3da3d70au));
    } else if (anim == 0x6d) {
        const float rel = angle_add(angle_sub(atan2_f(lwc1(P + 0x100), lwc1(P + 0x104)), lwc1(P + 0x98)), u2f(0x40490fdbu));
        float tilt = fpu_div(lwc1(P + 0x164), fs6c() * 3.0f) * u2f(0x3eb2b8c2u);
        if (u2f(0x3edf66f3u) < tilt) tilt = u2f(0x3edf66f3u);
        const float c = cos_vu(rel) * tilt;
        swc1(FX + 0x114, c);
        swc1(FX + 0x110, -sin_vu(rel) * tilt);
    }
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x100, fs70() * 25.0f);
    } else {
        sw(P + 0xe8, 0);
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 150.0f);
    }
}

// Swimming at the surface (state 0x31): the paddle sound, wake puffs and a bubble outside level
// 15, heading along the drift, turn spring by speed, slow sink, the body roll +0x90 by the turn
// and pitch +0x94 after 100 frames, swim wobble parameters.
// orig: 0x00217970 (block 0x21c674)
void surface_swim_update(uint32_t hm) {
    hero_sound_slot(2, hm, lw(0x0015ed84) == 0xf ? 0x1d : 6);
    if (lw(0x0015ed84) != 0xf) {
        hero_smoke_puffs(4, 2);
        const float q = 0.25f;
        Vec4 pos = {};
        pos.x = lwc1(P + 0x80) + rand_range_f(-q, q);
        pos.y = lwc1(P + 0x84) + rand_range_f(-q, q);
        pos.z = lwc1(P + 0x88) + rand_range_f(u2f(0xbdcccccdu), u2f(0x3ecccccdu));
        Vec4 vel;
        vec3_scale(vel, V(P + 0x100), u2f(0x3f666666u));
        const float a = u2f(0x3f333333u), b = u2f(0xbf333333u);
        vel.x = vel.x + rand_range_f(fs6c() * b, fs6c() * a);
        vel.y = vel.y + rand_range_f(fs6c() * b, fs6c() * a);
        vel.z = vel.z + rand_range_f(fs6c(), fs6c() + fs6c());
        spawn_particle_2f_v(pos, vel, rand_range_f(37800.0f, 75600.0f));
    }
    const float drift = vec3_length(V(P + 0xf0));
    if (fs6c() * 0.5f < drift) {
        swc1(P + 0x180, atan2_f(lwc1(P + 0xf0), lwc1(P + 0xf4)));
    } else {
        swc1(P + 0x180, lwc1(P + 0x98));
    }
    const float k = lwc1(0x0015ed64), k2 = u2f(0x3e4ccccdu);
    const float yaw = lwc1(P + 0x98);
    sub_00211e30(0, drift * 0.125f * k, k * k2, fs6c() * u2f(0x40860a92u));
    float turn = angle_dist(lwc1(P + 0x98), yaw);
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * 5.0f);
    const float sink = lwc1(P + 0x108) - fs70() * 50.0f, cap = fs70() * -40.0f;
    swc1(P + 0xe8, sink);
    if (cap < sink) swc1(P + 0xe8, cap);
    const float tmax = fs6c() * u2f(0x3fb2b8c2u);
    if (tmax < turn) turn = tmax;
    turn = turn * 9.0f;
    spring_angle(P + 0x90, P + 0x9e4, 0, turn * 0.5f, u2f(0x3be56042u), u2f(0x3e2e147bu), fs6c() * u2f(0x3f9c61aau));
    if (frame_scale_i(100) < lw(P + 0x198)) {
        const float kk = lwc1(0x0015ed64);
        spring_angle(P + 0x94, P + 0x9e8, 0, -lwc1(P + 0x9d4), kk * u2f(0x3d0f5c29u), kk * u2f(0x3e99999au), fs6c() * u2f(0x3ff5be0bu));
    }
    fx_params_set_1(u2f(0x3c1374bcu), u2f(0x3e6147aeu));
    fx_params_set_2(u2f(0x3d23d70au), k2);
    fx_params_set_3(u2f(0x3ca3d70au), k2);
    constexpr OvConst<0x0017a680u> FX{};
    swc1(FX + 0x118, turn * u2f(0x40066666u));
    swc1(FX + 0x110, turn * 3.5f);
}

// Rail grind (state 0x74): sparks under the hand while on the grind animation, snap to the
// closest point of the rail path +0x5f0, then (unless leaving, +0x600) advance along it at the
// grind speed +0x604, face the rail direction (+0x610 yaw / +0x614 pitch), and pull the hero onto
// the rail with the spring speed +0x618.
// orig: 0x00217970 (block 0x21dbf8)
void grind_update() {
    Vec4 hand;
    sub_0020d2f8_v(hand);
    const uint32_t hm = lwu(P + 0x2080);
    if (lbu(hm + 0x53) == 0x66) {
        hero_sound_slot(0, hm, 0);
        if ((lw(OV(0x0015f5cc)) & 1) != 0) {
            const Vec4 pos = hand;
            Vec4 vel;
            vec3_scale(vel, V(P + 0x100), u2f(0x3ecccccdu));
            const float a = angle_add(lwc1(P + 0x610), rand_range_signed_f(0.0f, u2f(0x3f060a92u)));
            const float r = rand_range_f(fs6c() * 0.0f, fs6c() * 4.0f);
            vel.x = vel.x + cos_vu(a) * r;
            vel.y = vel.y + sin_vu(a) * r;
            vel.z = rand_range_f(fs6c() * 3.0f, fs6c() * 5.0f);
            vel.w = -(fs70() * 35.0f);
            spawn_particle_19_v(pos, vel, 0);
        }
    }
    const uint32_t path = lwu(P + 0x5f0);
    auto* idx = static_cast<int32_t*>(ee_ram_ptr(P + 0x5f4));
    auto* frac = static_cast<float*>(ee_ram_ptr(P + 0x5f8));
    path_closest_point_v(path, hand, V(P + 0x5e0), idx, frac, lw(P + 0x60c), 999.0f, 8.0f, 2.5f);
    if (lw(P + 0x600) != 0) {
        V(P + 0xe0) = V(P + 0x110);
        sw(P + 0x600, lwu(P + 0x600) + 1);
        swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 24.0f);
        return;
    }
    Vec4 next;
    path_advance_v(lwu(P + 0x5f0), next, idx, frac, lw(P + 0x60c), lwc1(P + 0x604));
    const int32_t ahead = step_index(lwu(P + 0x5f0), lw(P + 0x5f4), 1, lw(P + 0x60c));
    Vec4 d;
    vec3_sub(d, V(lwu(P + 0x5f0) + 0x10 + static_cast<uint32_t>(ahead) * 16), next);
    if (vec3_length(d) < u2f(0x3a83126fu)) {
        if (lw(P + 0x5f4) == ahead) {
            sw(P + 0x600, static_cast<uint32_t>(frame_scale_i(1)));
            return;
        }
    } else {
        swc1(P + 0x610, atan2_f(d.x, d.y));
        swc1(P + 0x614, atan2_f(vec2_length(d), d.z));
    }
    const float k = lwc1(0x0015ed64);
    swc1(P + 0x180, lwc1(P + 0x610));
    sub_00211e30(0, k * u2f(0x3c9374bcu), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x40cea5a1u));
    approach_f(P + 0x604, fs6c() * 14.0f, fs70() * 9.0f);
    vec3_sub(V(P + 0xe0), next, V(P + 0x5e0));
    vec3_sub(d, V(P + 0x5e0), hand);
    float pull = -vec3_length(d);
    const float kk = lwc1(0x0015ed64);
    spring_to_v(pull, P + 0x618, 0.0f, kk * u2f(0x3d4ccccdu), kk * u2f(0x3e99999au), fs6c() * 10.0f);
    const float len = vec3_length(d);
    if (len < lwc1(P + 0x618)) swc1(P + 0x618, len);
    if (lwc1(P + 0x618) < len) vec3_set_length(d, d, lwc1(P + 0x618));
    vec3_add(V(P + 0x80), V(P + 0x80), d);
}

// Hanging from a ledge (states 0x18 / 0x19; 0x4a / 0x4b pull up slower): face the wall
// (+0x4e4 + pi), level the pitch, pull towards the hang point +0x4d0, ease the grab height
// +0x4f0 to the hero.
// orig: 0x00217970 (blocks 0x21dfd0 / 0x21e088)
void ledge_hang_update(bool climb) {
    const float k = lwc1(0x0015ed64);
    swc1(P + 0x180, angle_add(lwc1(P + 0x4e4), u2f(0x40490fdbu)));
    float lim;
    if (!climb) {
        sub_00211e30(0, k * u2f(0x3d23d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x40c90fdbu));
        turn_towards(P + 0x94, 0, 0.0f, fs6c() * u2f(0x4096cbe4u));
        lim = fs6c() * 4.0f;
    } else {
        sub_00211e30(0, k * u2f(0x3ca3d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x4096cbe4u));
        lim = fs6c() * u2f(0x3fd9999au);
    }
    vec3_sub(V(P + 0xe0), V(P + 0x4d0), V(P + 0x80));
    vec3_clamp_length_v(V(P + 0xe0), lim);
    approach_f(P + 0x4f0, lwc1(P + 0x88), fs6c() * 3.0f);
}

// Shimmying along a ledge (0x1a left / 0x1b right; in water 0x4d / 0x4e): probe the ledge a step
// to the side; with both grabs found and the ledge turning less than 30 degrees, face the mean
// wall direction and move towards the midpoint of the two hang points at the animation's speed
// (table 0x1c3cb0, in water 0x1c3cf0 and in 3D).
// orig: 0x00217970 (blocks 0x21e148 / 0x21ff84)
void shimmy_update(bool water) {
    sw(P + 0xe0, 0);
    if (water) swc1(P + 0xa90, u2f(0x3fa66666u));
    sw(P + 0xe4, 0);
    sw(P + 0xe8, 0);
    const Vec4 home = V(P + 0x80);
    const float side = lw(P + 0x2084) == (water ? 0x4d : 0x1a) ? u2f(0x3fc90fdbu) : u2f(0xbfc90fdbu);
    const float k3 = u2f(0x3e99999au);
    Vec4 off = {};
    off.x = cos_vu(angle_add(lwc1(P + 0x98), side)) * k3;
    off.y = sin_vu(angle_add(lwc1(P + 0x98), side)) * k3;
    Vec4 probe;
    vec3_add(probe, home, off);
    hero_ledge_grab_check();
    if (lw(P + 0x4e8) == 0) return;
    const Vec4 a = V(P + 0x4d0);
    const float pi = u2f(0x40490fdbu);
    float fa = angle_add(lwc1(P + 0x4e4), pi);
    V(P + 0x80) = probe;
    hero_ledge_grab_check();
    V(P + 0x80) = home;
    if (lw(P + 0x4e8) == 0) return;
    const Vec4 b = V(P + 0x4d0);
    const float fb = angle_add(lwc1(P + 0x4e4), pi);
    if (u2f(0x3f060a92u) < angle_dist(fa, fb)) return;
    fa = angle_add(angle_sub(fa, fb) * 0.5f, fa);
    Vec4 mid;
    vec3_add(mid, a, b);
    vec3_scale(mid, mid, 0.5f);
    const float k = lwc1(0x0015ed64);
    swc1(P + 0x180, fa);
    sub_00211e30(0, k * u2f(0x3d23d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x40c90fdbu));
    float unused = lwc1(P + 0x164);  // (eased on the stack, never read back)
    approach_f_v(unused, lwc1(P + 0x190) * fs6c(), fs70() * 5.0f);
    vec3_sub(V(P + 0xe0), mid, V(P + 0x80));
    const int32_t i = float_to_int(lwc1(P + 0xaa8));
    if (water) {
        vec3_set_length(V(P + 0xe0), V(P + 0xe0), lwc1(OV(0x001c3cf0) + static_cast<uint32_t>(i) * 4) * 0.5f);
    } else {
        vec2_set_length(V(P + 0xe0), V(P + 0xe0), lwc1(OV(0x001c3cb0) + static_cast<uint32_t>(i) * 4) * 0.5f);
    }
}

// Skidding (state 0x16): the horizontal speed drops by 4 (10 after frame 10 off a platform),
// edge brake on footing, then ease the velocity to rest.
// orig: 0x00217970 (block 0x21f2f0)
void skid_update() {
    float v = vec2_length(V(P + 0xe0));
    float drop = fs70() * 4.0f;
    if (lh(P + 0x30e) == 0 && frame_scale_i(10) < lw(P + 0x198)) drop = fs70() * 10.0f;
    v = v - drop;
    if (v < 0.0f) v = 0.0f;
    if (lbu(P + 0x12e7) != 0) hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    vec2_set_length(V(P + 0xe0), V(P + 0xe0), v);
    sub_002334d0(P + 0xe0, P + 0xe0, u2f(0x3b83126fu));
}

// Walking to a scripted spot (state 0x65; the target is +0x1640): steer and set the speed
// towards it (by movement mode +0x20a4), stand when within 0.2.
// orig: 0x00217970 (block 0x21f3a0)
void walk_to_update() {
    Vec4 d;
    vec3_sub(d, V(P + 0x1640), V(P + 0x80));
    swc1(P + 0x180, atan2_f(d.x, d.y));
    const float dist = vec2_length(d);
    if (0.2f < dist) {
        swc1(P + 0x229c, 1.0f);
        swc1(P + 0x190, dist);
        swc1(P + 0x180, atan2_f(d.x, d.y));
        constexpr OvConst<0x0017bdb8u> T{};
        const float sp = dist < lwc1(T + 0xc) ? lwc1(T + 8) : lwc1(T + 0x18);
        swc1(P + 0x190, sp * fs6c());
        if (lbu(P + 0x20a4) == 3) {
            swc1(P + 0x190, fs6c() * 1.5f);
        } else if (lbu(P + 0x20a4) == 1) {
            swc1(P + 0x190, fs6c() * u2f(0x3f666666u));
        }
    } else {
        swc1(P + 0x229c, 0.5f);
        swc1(P + 0x180, lwc1(P + 0x164c));
        sw(P + 0x190, 0);
    }
    const uint32_t mode = lbu(P + 0x20a4);
    const float k = lwc1(0x0015ed64);
    if (mode == 3) {
        sub_00211e30(0, k * u2f(0x3cf5c28fu), k * u2f(0x3e99999au), fs6c() * u2f(0x40b2b8c2u));
    } else if (mode == 1) {
        sub_00211e30(0, k * u2f(0x3cf5c28fu), k * u2f(0x3e99999au), fs6c() * u2f(0x40fb53d1u));
    } else if (mode == 0) {
        if (lw(P + 0x2088) == 1) {
            sub_00211e30(0, k * u2f(0x3c83126fu), k * u2f(0x3e19999au), fs6c() * u2f(0x411f2c8du));
        } else {
            sub_00211f38();
        }
        if (lbu(P + 0x20a4) == 0) hero_run_lean();
    }
    sub_00212088(fs70() * 7.5f * 1.0f, fs70() * 8.5f);
    sub_002120d8(99999.0f);
    if (lbu(P + 0x20a4) == 0) hero_wall_slide(1);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x100, fs70() * 25.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// Waiting at the scripted spot (state 0x66): drop any push away from it, stand.
// orig: 0x00217970 (block 0x21f654)
void wait_at_update() {
    Vec4 d;
    vec3_sub(d, V(P + 0x1640), V(P + 0x80));
    if (vec3_dot(d, V(P + 0x150)) < 0.0f) vec_clear(V(P + 0x150));
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * 12.0f);
    hero_wall_slide(0);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x100, fs70() * 25.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
}

// The crouching wrench swing (state 0x51): lunge speed early in steps 0 / 1; inside the step's
// hit window the wrench hits around the midpoint of the hand joints (chain points 0/1 or 2/3),
// then the swing's turn and limits as in 0x13, sticking to the ground.
// orig: 0x00217970 (block 0x21f880)
void crouch_swing_update() {
    swc1(P + 0x190, 0.0f);
    const uint32_t e = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
    if (lw(e + 4) == 0 || lw(e + 4) == 1) {
        if (lwc1(P + 0xaa8) < 5.5f) swc1(P + 0x190, fs6c() * 3.5f);
    }
    if (lw(P + 0xa9c) == 0) {
        const float t = lwc1(P + 0xaa8);
        if (static_cast<float>(lw(e + 0x1c)) <= t && t <= static_cast<float>(lw(e + 0x20))) {
            const uint32_t id = lw(e + 4) == 1 ? 0u : 2u;
            Vec4 a, b, c;
            moby_chain_point_v(lwu(P + 0x2080), id, a);
            moby_chain_point_v(lwu(P + 0x2080), id | 1u, b);
            vec3_add(c, a, b);
            vec3_scale(c, c, 0.5f);
            moby_sphere_contact_v(lwu(P + 0x2080), c, 0x10000, 0, 1, 0, 0.25f, 1.0f, 1.0f);
        }
    }
    approach_f(P + 0xa90, 1.0f, u2f(0x3e4ccccdu));
    if (lw(P + 0xa58) != 0 || lwu(P + 0xa54) != 0) {
        if (lw(e + 4) == 1 && 0.0f < angle_sub(lwc1(P + 0xa5c), lwc1(P + 0x98))) {
            swc1(P + 0xa90, fpu_div(1.0f, angle_dist(lwc1(P + 0xa5c), lwc1(P + 0x98)) + 1.0f));
        }
        attack_turn(0);
    } else {
        const bool late = lw(e + 4) == 1 && lw(P + 0xa9c) == 0 && 7.0f < lwc1(P + 0xaa8);
        if (!late) hero_lock_heading(5.0f, u2f(0x3f490fdbu), u2f(0x3f490fdbu));
    }
    sub_00212088(fs70() * 37.0f, fs70() * 28.0f);
    sub_002120d8(lw(P + 0xa58) != 0 ? lwc1(P + 0xa5c) : 99999.0f);
    if (lh(P + 0x30e) != 0) {
        swc1(P + 0xe8, lwc1(P + 0x108) - fs70() * 25.0f);
        sub_002121c0();
    } else {
        swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 54.0f);
    }
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
}

// Swimming underwater, idle (state 0x4f): slow stick move and turn, float the velocity.
// orig: 0x00217970 (block 0x21fba0)
void swim_idle_update() {
    const float sp = fs6c() + fs6c();
    hero_stick_move(0, sp);
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3d23d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x410ba058u));
    sub_00212088(fs70() * 15.0f, fs70() * 7.0f);
    sub_002120d8(99999.0f);
    sub_002338d0(P + 0xe0, P + 0xe0);
    sub_002334d0(P + 0xe0, P + 0xe0, sp * u2f(0x3f3851ecu));
}

// (enhancement: Clank = Modern's values, #57: the walk 5 units/s (the original's 3), its acceleration 25 / braking 30
// (7.5 / 8.5), the jump's air steering 5 units/s eased at 20 (3 / 12))
constexpr float CLANK_WALK = 5.0f, CLANK_ACCEL = 25.0f, CLANK_BRAKE = 30.0f, CLANK_AIR = 5.0f, CLANK_AIR_EASE = 20.0f;

// Wading (states 0x44 / 0x50): slower stick move, the turn spring (tighter when slow), speed
// cut by the turn, skid target, wall slide, ground follow-up.
// orig: 0x00217970 (block 0x21fd28)
void wade_update() {
    // (enhancement: Clank = Modern, #57: Clank's walk (state 0x44) faster, quicker to speed up and to turn)
    const bool clank = advanced_clank_modern() && lw(P + 0x2084) == 0x44;
    float turn = angle_dist(lwc1(P + 0x98), lwc1(P + 0x180));
    const float sp = fs6c() * (clank ? CLANK_WALK : 3.0f);
    hero_stick_move(0, sp);
    turn = turn * u2f(0x3e99999au);
    const float g = lwc1(P + 0x229c) + u2f(0x3eb33333u);
    float slow = 1.0f - turn;
    if (slow < 0.0f) slow = 0.0f;
    const float k = lwc1(0x0015ed64);
    if (clank) {  // (the tighter turn of the slow walk, its rate up)
        sub_00211e30(0, k * u2f(0x3d0f5c29u) * g * 1.5f, k * u2f(0x3e19999au), fs6c() * u2f(0x411996c7u) * g * 1.5f);
    } else if (lwc1(P + 0x160) < sp * 0.5f) {
        sub_00211e30(0, k * u2f(0x3d0f5c29u) * g, k * u2f(0x3e19999au), fs6c() * u2f(0x411996c7u) * g);
    } else {
        sub_00211e30(0, k * u2f(0x3c03126fu) * g, k * u2f(0x3e19999au), fs6c() * u2f(0x411f2c8du) * g);
    }
    sub_00212088(fs70() * (clank ? CLANK_ACCEL : 7.5f) * slow, fs70() * (clank ? CLANK_BRAKE : 8.5f));
    if (lh(P + 0x3b8) != 0 && lh(P + 0x308) == 0) {
        sub_002120d8(lwc1(P + 0x180));
    } else {
        sub_002120d8(99999.0f);
    }
    hero_wall_slide(1);
    ground_end();
}

// Giant Clank's punch combo (state 0x60, step +0xa60): stand and turn, the hand hits (chain
// points 3 / 4, impulse 4 forward and up) inside each step's frame window, the arc of three
// shove spheres in front, the lunge speed, wall slide and ground follow-up.
// orig: 0x00217970 (block 0x220294)
void clank_punch_update() {
    hero_stick_move(0, 1.0f);
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3ca3d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x3ff5be0bu));
    sw(P + 0x190, 0);
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * u2f(0x4149999au));
    if (lw(P + 0xa9c) == 0) {
        const uint32_t hm = lwu(P + 0x2080);
        Vec4 d = {};
        d.x = cos_vu(lwc1(P + 0x98));
        d.y = sin_vu(lwc1(P + 0x98));
        alignas(16) uint32_t rec[12] = {};
        std::memcpy(rec, &d, 16);
        rec[4] = hm;
        rec[5] = 0xb0000;
        const float four = 4.0f, one = 1.0f;
        std::memcpy(&rec[7], &four, 4);
        rec[8] = 1;
        std::memcpy(&rec[2], &one, 4);
        rec[3] = 0x45afdf66u;
        rec[6] = 7u | (1u << 8) | (static_cast<uint32_t>(lhu(hm + 0xa6)) << 16);
        auto hit = [&rec](uint32_t joint) {
            Vec4 p;
            moby_chain_point_v(lwu(P + 0x2080), joint, p);
            mobys_in_sphere_v(p, 2.0f, 0x10, lwu(P + 0x2080), rec);
        };
        const int32_t step = lw(P + 0xa60);
        const float t = lwc1(P + 0xaa8);
        if (step == 1) {
            if (7.0f < t && t < 14.0f) hit(4);
        } else if (step == 0) {
            if (7.0f < t && t < 14.0f) hit(3);
        } else if (step == 2) {
            if (14.0f < t && t < 21.0f) {
                hit(3);
                hit(4);
            }
        }
        const float q = u2f(0x3f490fdbu);
        for (int32_t i = 0; i < 3; ++i) {
            Vec4 p;
            sub_002335c8_v(p, u2f(0x3fe66666u), angle_add(u2f(0xbf490fdbu), static_cast<float>(i) * q), u2f(0x3fb33333u));
            moby_sphere_contact_v(lwu(P + 0x2080), p, 0x30000, 0, 1, 0, u2f(0x400ccccdu), 40.0f, 1.0f);
        }
    }
    const int32_t step = lw(P + 0xa60);
    if (step >= 0 && step < 2) {
        const float t = lwc1(P + 0xaa8);
        if (lw(P + 0xa9c) == 0 && 7.0f <= t && t < 15.0f) swc1(P + 0x190, fs6c() * 21.0f);
        sub_00212088(fs70() * 180.0f, fs70() * 140.0f);
        sub_002120d8(99999.0f);
    } else if (step == 2) {
        const float t = lwc1(P + 0xaa8);
        if (lw(P + 0xa9c) == 0 && 11.0f <= t && t < 20.0f) swc1(P + 0x190, fs6c() * 30.0f);
        sub_00212088(fs70() * 150.0f, fs70() * 120.0f);
        sub_002120d8(99999.0f);
    }
    hero_wall_slide(1);
    attack_ground_end();
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
}

// Giant Clank's ground pound (state 0x61): stand still, the animation rate fast between frames
// 22 and 55, the shock moby 0x5f3 placed 3 ahead and 5 up before frame 22.
// orig: 0x00217970 (block 0x22077c)
void clank_pound_update(uint32_t hm) {
    const float k = u2f(0x4149999au);
    sw(P + 0x190, 0);
    sub_00212088(0.0f, fs70() * k);
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * k);
    if (lw(P + 0xa9c) == 0) {
        const float t = lwc1(P + 0xaa8);
        if (22.0f < t && t < 55.0f) {
            swc1(P + 0xa90, 1.0f);
        } else if (t <= 22.0f) {
            Vec4 p;
            vec3_set_length(p, V(P + 0x00), 3.0f);
            p.z = p.z + 5.0f;
            vec3_add_inplace(p, V(hm + 0x10));
            place_moby_5f3_v(hm, p);
            swc1(P + 0xa90, 0.5f);
        }
    }
    ground_end();
}

// Giant Clank firing (state 0x5f): stand and turn; at frame 4 a missile from each hand (chain
// points 3 / 4, 1.4 ahead).
// orig: 0x00217970 (block 0x2208d4)
void clank_fire_update() {
    const float k = u2f(0x4149999au);
    sw(P + 0x190, 0);
    const float zero = lwc1(P + 0x190);
    sub_00212088(zero, fs70() * k);
    hero_stick_move(0, 1.0f);
    const float kk = lwc1(0x0015ed64);
    sub_00211e30(0, kk * u2f(0x3c23d70au), kk * u2f(0x3e4ccccdu), fs6c() * u2f(0x3f9c61aau));
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * k);
    if (sub_00211870(4.0f) != 0) {
        const float d = u2f(0x3fb33333u);
        for (uint32_t i = 0; i < 2; ++i) {
            Vec4 p;
            moby_chain_point_v(lwu(P + 0x2080), i + 3, p);
            p.x = p.x + cos_vu(lwc1(P + 0x98)) * d;
            p.y = p.y + sin_vu(lwc1(P + 0x98)) * d;
            const float yaw = lwc1(P + 0x98);
            spawn_projectile_100_v(lwu(P + 0x2080), p, 0, yaw, zero, 0.5f, yaw, zero);
        }
    }
    ground_end();
}

// Giant Clank stomping down (state 0x5c): stick move, the landing shove sphere under him,
// velocity and fall limits.
// orig: 0x00217970 (block 0x220a54)
void clank_land_update() {
    hero_stick_move(0, fs6c() * 6.0f);
    Vec4 p = V(P + 0x80);
    p.z = p.z - 1.0f;
    moby_sphere_contact_v(lwu(P + 0x2080), p, 0x30000, 0, 1, 0, 3.5f, 40.0f, 1.0f);
    if (lw(P + 0x1c4) == 0) hero_horizontal_velocity();
    sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 12.0f);
    const float lim = fs6c() * -10.0f;
    if (lwc1(P + 0xe8) < lim) swc1(P + 0xe8, lim);
}

// Giant Clank walking (state 0x5b): the shove arc in front, the footstep camera shake at frames
// 30 / 0, stick move and turn, speed cut by the turn, the body sway parameters by the turn rate.
// orig: 0x00217970 (block 0x220b28)
void clank_walk_update() {
    const float turn = angle_dist(lwc1(P + 0x98), lwc1(P + 0x180));
    const float q = u2f(0x3f490fdbu);
    for (int32_t i = 0; i < 3; ++i) {
        Vec4 p;
        sub_002335c8_v(p, u2f(0x3fe66666u), angle_add(u2f(0xbf490fdbu), static_cast<float>(i) * q), u2f(0x3fb33333u));
        moby_sphere_contact_v(lwu(P + 0x2080), p, 0x30000, 0, 1, 0, u2f(0x402ccccdu), 40.0f, 1.0f);
    }
    if (sub_00211870(30.0f) != 0 || sub_00211870(0.0f) != 0) {
        swc1(OV(0x00166de0), u2f(0x3dcccccdu));
        sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x14)));
    }
    hero_stick_move(0, fs6c() * 6.0f);
    const float k3 = u2f(0x3e99999au);
    float slow = 1.0f - turn * k3;
    const float g = lwc1(P + 0x229c) + u2f(0x3eb33333u);
    if (slow < 0.0f) slow = 0.0f;
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3cac0831u) * g, k * k3, fs6c() * u2f(0x4011361eu) * g);
    sub_00212088(fs70() * 7.5f * slow, fs70() * 8.5f);
    float t = lwc1(P + 0x188);
    const float lim = u2f(0x3fd9999au);
    if (lim < t) t = lim;
    else if (t < -lim) t = -lim;
    const float kf = lwc1(0x0015ed64);
    constexpr OvConst<0x0017a680u> FX{};
    swc1(FX + 0x1494, kf * u2f(0x3c23d70au));
    swc1(FX + 0x1334, kf * u2f(0x3c75c28fu));
    swc1(FX + 0x13e4, kf * u2f(0x3ca3d70au));
    swc1(FX + 0x13a8, t * u2f(0x3f11eb85u));
    swc1(FX + 0x13e8, kf * k3);
    swc1(FX + 0x12f0, -t * u2f(0x3e0f5c29u));
    swc1(FX + 0x12f8, t * 0.25f);
    swc1(FX + 0x1498, kf * k3);
    swc1(FX + 0x1338, kf * k3);
    if (lh(P + 0x3b8) != 0 && lh(P + 0x308) == 0) {
        sub_002120d8(lwc1(P + 0x180));
    } else {
        sub_002120d8(99999.0f);
    }
    hero_wall_slide(1);
    ground_end();
}

// the airborne drag shared by states 0x75 / 0x76: no vertical speed, shrink the horizontal
// velocity by the frame-scaled drag
// orig: 0x00217970 (blocks 0x2190b4 / 0x219138)
void dive_drag() {
    sw(P + 0xe8, 0);
    const float len = vec2_length(V(P + 0xe0));
    vec2_clamp_length(V(P + 0xe0), V(P + 0xe0), lwc1(0x0015ed60) * u2f(0xbda3d708u) * len + len);
}

// Diving into water (state 0x76): a fading smoke trail (fewer puffs each 3 frames), drag.
// orig: 0x00217970 (block 0x2190b4)
void dive_update() {
    int32_t n = 10 - lw(P + 0x198) / 3;
    if (!(n < 9)) n = 8;
    hero_smoke_puffs(n, 0);
    dive_drag();
    hero_wall_slide(0);
}

// Diving (state 0x75): smoke, drag, then the swim height spring (0x227d40).
// orig: 0x00217970 (block 0x219138)
void dive_75_update() {
    sub_00209540(0xf, 0x1e);
    dive_drag();
    hero_wall_slide(0);
    sub_00227d40();
}

// Underwater swimming drift (states 0x6a / 0x82): the push velocity +0x150 decays, bubbles from
// the head on the +0x8f0 timer (sooner early in the dive), and the sink speed +0x8fc eases to
// -1.5 vm.
// orig: 0x00217970 (block 0x21918c)
void underwater_update() {
    float len = vec3_length(V(P + 0x150));
    approach_f_v(len, 0.0f, fs70() * 4.0f);
    vec3_set_length(V(P + 0x150), V(P + 0x150), len);
    V(P + 0xe0) = V(P + 0x150);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x8f0))) == 2) {
        Vec4 p;
        moby_chain_point_v(lwu(P + 0x2080), 4, p);
        Vec4 off = {};
        off.x = u2f(0x3ecccccdu);
        mat3_transform(off, off, M(lwu(P + 0x2080) + 0xc0));
        vec3_add(p, p, off);
        Vec4 v = V(P + 0x100);
        vec3_set_length(off, off, fs6c() * 1.5f);
        vec3_add(v, v, off);
        v.z = v.z + (fs6c() + fs6c());
        spawn_particle_22_v(p, v, 25200.0f, -1.0f);
        if (lw(P + 0x198) < frame_scale_i(0x28)) {
            const int32_t lo = frame_scale_i(2);
            const int32_t t = rand_range(lo, frame_scale_i(5));
            sw(P + 0x8f0, static_cast<uint32_t>(t + lw(P + 0x198) / 5));
        } else if (frame_scale_i(0x46) < lw(P + 0x198)) {
            const int32_t lo = frame_scale_i(10);
            sw(P + 0x8f0, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x14))));
        }
    }
    if (0.0f < lwc1(P + 0x8fc)) sw(P + 0x8fc, 0);
    approach_f(P + 0x8fc, -(fs6c() * 1.5f), fs70() * 1.5f);
    swc1(P + 0xe8, lwc1(P + 0x8fc));
}

// Swimming at the surface (states 0x36 / 0x37): splash and wake by speed, the wake puff timer
// +0x90a, slow stick move (minimum speed while the stroke button is held), the turn spring;
// state 0x36 paddles on the stroke timer +0x698 along the facing with the stroke curve (table
// 0x17bfc0), 0x37 floats.
// orig: 0x00217970 (block 0x2193a4)
void surface_update() {
    if (lw(P + 0x2084) == 0x37) sub_00209540(0xf, 0x1e);
    if (fs6c() * 1.5f < lwc1(P + 0x164)) sub_00209848(4, 0xc);
    if (lw(P + 0x2084) == 0x36 && fs6c() < lwc1(P + 0x164)) sub_00209638(0, 1);
    if (lh(P + 0x90a) != 0) {
        const uint32_t v = lhu(P + 0x90a) - 1;
        sh(P + 0x90a, v);
        int32_t n = static_cast<int16_t>(v) >> 3;
        if (!(n < 7)) n = 6;
        hero_smoke_puffs(n, 0);
    }
    const float three = 3.0f;
    swc1(P + 0xa90, u2f(0x3f19999au));
    hero_stick_move(0, fs6c() * three);
    constexpr uint32_t PAD = 0x0013c940;
    if (lw(PAD + 0x1d4) == 0) {
        const float lo = fs6c() * three * 0.5f;
        if (lwc1(P + 0x190) < lo) swc1(P + 0x190, lo);
    }
    const float k = lwc1(0x0015ed64);
    sub_00211e30(0, k * u2f(0x3be56042u), k * u2f(0x3da3d70au), fs6c() * u2f(0x40a78d36u));
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x698)));
    if (lw(P + 0x2084) == 0x36) {
        if (lw(PAD + 0x1d4) != 0 && lw(P + 0x698) != 0) swc1(P + 0x190, fs6c() * 3.0f);
        if (u2f(0x3f490fdbu) < abs_f(lwc1(P + 0x188))) sw(P + 0x190, 0);
        sub_00212088(fs70() * 4.0f, fs70() * 5.0f);
        const float c = sub_00209018(OV(0x0017bfc0), 0x1e);
        const float s = lwc1(P + 0x194) * c * 5.5f;
        swc1(P + 0xe0, cos_vu(lwc1(P + 0x98)) * s);
        sw(P + 0xe8, 0);
        swc1(P + 0xe4, sin_vu(lwc1(P + 0x98)) * s);
    } else {
        sw(P + 0x194, 0);
        vec_clear(V(P + 0xe0));
    }
    if (lw(P + 0x2084) == 0x36 && lw(P + 0xa9c) == 0) {
        const float t = lwc1(P + 0xaa8);
        if (0.0f < t && t < 10.0f) hero_smoke_puffs(4, 1);
    }
    sub_00216710(fs70() * u2f(0x40866666u));
    hero_wall_slide(0);
    sub_00227d40();
}

// Riding a platform moby 0x155 (states 0x38..0x3a): follow its position (at most 2 vm a frame)
// and turn with it (at most 9.42 vm).
// orig: 0x00217970 (block 0x21969c)
void ride_update() {
    const uint32_t m = lwu(P + 0x2fc);
    if (m == 0 || lh(m + 0xa6) != 0x155) return;
    Vec4 t;
    vec_clear(t);
    vec3_add(t, t, V(lwu(P + 0x2fc) + 0x10));
    vec3_sub(V(P + 0xe0), t, V(P + 0x80));
    vec3_clamp_length_v(V(P + 0xe0), fs6c() + fs6c());
    float d = angle_sub(lwc1(lwu(P + 0x2fc) + 0x48), lwc1(P + 0x98));
    const float lim = fs6c() * u2f(0x4116cbe4u);
    if (lim < d) {
        d = lim;
    } else if (d < -lim) {
        d = -lim;
    }
    swc1(P + 0x98, angle_add(lwc1(P + 0x98), d));
}

// Talking / holding still (state 0x30): footing push, edge brake, stand, the ground follow-up;
// once the hero moby finished animation 0x36, back to the item's idle animation.
// orig: 0x00217970 (block 0x21976c)
void talk_update() {
    if (lbu(P + 0x12e2) != 0) {
        sub_002132b8(u2f(0x3f333333u), fs6c() * 0.0f);
    } else {
        sub_002132b8(0.5f, fs6c() + fs6c());
    }
    const float k = u2f(0x4149999au);
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    hero_stick_move(0, 1.0f);
    sub_00212088(fs70() * k, fs70() * k);
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * k);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x100, fs70() * 25.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
    const uint32_t hm = lwu(P + 0x2080);
    if ((lbu(hm + 0x70) & 2) == 0 || lbu(hm + 0x52) != 0x36) return;
    const uint32_t set = sub_00205000(0);
    hero_anim_set(set, 0, static_cast<float>(frame_scale_i(5)));
}

// Swingshot target lock (states 0x24 / 0x2e): stand, the ground follow-up, face the target
// moby +0x964.
// orig: 0x00217970 (block 0x2198fc)
void swing_aim_update(uint32_t hm) {
    const float k = u2f(0x4149999au);
    hero_edge_brake(u2f(0x406ccccdu), 0.0f);
    swc1(P + 0x190, 0.0f);
    sub_00212088(fs70() * k, fs70() * k);
    vec_clear(V(P + 0xe0));
    sub_00216710(fs70() * k);
    if (lh(P + 0x30e) != 0) {
        sub_002334d0(P + 0xe0, P + 0x110, fs70() * 24.0f);
        sub_002121c0();
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 54.0f);
    }
    const uint32_t t = lwu(P + 0x964);
    if (t == 0) return;
    const float a = atan2_f(lwc1(t + 0x10) - lwc1(hm + 0x10), lwc1(t + 0x14) - lwc1(hm + 0x14));
    const float kk = lwc1(0x0015ed64);
    swc1(P + 0x180, a);
    sub_00211e30(0, kk * u2f(0x3ca3d70au), kk * u2f(0x3e19999au), fs6c() * u2f(0x40c90fdbu));
}

// Swingshot pull (state 0x25): towards the target moby +0x964 at the reel speed +0x960 (eased up
// while reeling, +0x2088 0; braking once the arrival time is known, 1), the frames left +0x96c,
// pitch towards the target until arrival is near (+0x974), lift in the last 30 frames.
// orig: 0x00217970 (block 0x219a0c)
void swing_pull_update(uint32_t hm) {
    const Vec4 tgt = V(lwu(P + 0x964) + 0x10);
    swc1(P + 0x970, vec3_distance(V(P + 0x80), tgt));
    swc1(P + 0x96c, static_cast<float>(frame_scale_i(0x3c)));
    if (0.0f < lwc1(P + 0x960)) swc1(P + 0x96c, fpu_div(lwc1(P + 0x970), lwc1(P + 0x160)));
    const float brake = fs70() * 42.0f;
    if (lw(P + 0x2088) == 0) {
        approach_f(P + 0x960, fs6c() * 27.0f, fs70() * 80.0f);
        const uint32_t t = lwu(P + 0x964);
        const float a = atan2_f(lwc1(t + 0x10) - lwc1(hm + 0x10), lwc1(t + 0x14) - lwc1(hm + 0x14));
        const float k = lwc1(0x0015ed64);
        swc1(P + 0x180, a);
        sub_00211e30(0, k * u2f(0x3cf5c28fu), k * u2f(0x3e19999au), fs6c() * u2f(0x40ea927fu));
    } else if (lw(P + 0x2088) == 1) {
        approach_f(P + 0x960, 0.0f, brake);
    }
    float hi = 0.0f, lo = 0.0f;
    const float c = -(lwc1(P + 0x970) - u2f(0x3ff33333u));
    if (solve_quadratic(&hi, &lo, -(fs70() * 42.0f) * 0.5f, lwc1(P + 0x960), c) > 0 && 0.0f < hi) {
        swc1(P + 0x96c, hi);
        sw(P + 0x2088, 1);
    }
    if (lwc1(P + 0x96c) < static_cast<float>(frame_scale_i(0x14))) sw(P + 0x974, 1);
    if (lw(P + 0x974) != 0) {
        turn_towards(P + 0x94, 0, 0.0f, fs6c() * u2f(0x4075be0bu));
    } else {
        const float d = vec2_distance(V(P + 0x80), tgt);
        const float pitch = atan2_f(d, tgt.z - lwc1(P + 0x88));
        turn_towards(P + 0x94, 0, angle_add(-pitch, u2f(0x3fc90fdbu)), fs6c() * u2f(0x40cea5a1u));
    }
    const float side = u2f(0xbfc90fdbu), k57 = u2f(0xbf11eb85u);
    Vec4 s = {};
    s.x = cos_vu(angle_add(lwc1(P + 0x98), side)) * k57;
    s.y = sin_vu(angle_add(lwc1(P + 0x98), side)) * k57;
    vec3_add(s, s, V(P + 0x80));
    Vec4 d;
    vec3_sub(d, V(lwu(P + 0x964) + 0x10), s);
    vec3_set_length(V(P + 0xe0), d, lwc1(P + 0x960));
    if (lwc1(P + 0x96c) < static_cast<float>(frame_scale_i(0x1e))) {
        const float left = static_cast<float>(frame_scale_i(0x1e)) - lwc1(P + 0x96c);
        swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 29.0f * left);
    }
}

// Swingshot swing, hanging below the hook (state 0x26): the rope length +0x97c eases to +0x980,
// gravity, and the velocity is constrained to keep that distance from the target moby +0x964.
// orig: 0x00217970 (block 0x219d74)
void swing_hang_update() {
    approach_f(P + 0x97c, lwc1(P + 0x980), fs6c() * 10.0f);
    V(P + 0xe0) = V(P + 0x100);
    vec3_scale(V(P + 0xe0), V(P + 0xe0), u2f(0x3f666666u));
    swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 15.0f);
    const Vec4 hook = V(lwu(P + 0x964) + 0x10);
    swc1(P + 0x970, vec3_distance(V(P + 0x80), hook));
    Vec4 next, rope, at;
    vec3_add(next, V(P + 0x80), V(P + 0xe0));
    vec3_sub(rope, next, hook);
    vec3_set_length(rope, rope, lwc1(P + 0x97c));
    vec3_add(at, hook, rope);
    vec3_sub(V(P + 0xe0), at, V(P + 0x80));
}

// Swinging on a swing target (state 0x2c, target +0x994): the start animation, the hero's yaw
// following the target's facing, the body tilt along the rope, stick pumping, damping and the
// speed cap, the rope constraint (length +0x9a4 sprung to +0x9a0), the push +0x9b4 while
// starting, and at the bottom of each arc (+0x9bc) the next swing animation at the pendulum's
// rate.
// orig: 0x00217970 (block 0x219e70)
void swing_update(uint32_t hm) {
    if ((lw(P + 0xa98) & 2) != 0 && lbu(hm + 0x53) == 0x34) {
        hero_anim_set(0x35, 10, static_cast<float>(frame_scale_i(0xc)));
        swc1(P + 0xa90, u2f(0x3e99999au));
    }
    const float vz0 = lwc1(P + 0xe8);
    const uint32_t t = lwu(P + 0x994);
    if (t != 0 && lwu(t + 0x78) != 0 && lw(lwu(t + 0x78) + 0x34) != 0) {
        float yaw = lwc1(t + 0x48);
        if (90.0f < angle_dist(yaw, lwc1(P + 0x98))) yaw = angle_add(yaw, u2f(0x40490fdbu));
        const float k = lwc1(0x0015ed64);
        spring_angle(P + 0x9ac, P + 0x9b8, 0, yaw, k * u2f(0x3ca3d70au), k * u2f(0x3e99999au), 0.0f);
    }
    {
        const float k = lwc1(0x0015ed64);
        swc1(P + 0x180, lwc1(P + 0x9ac));
        sub_00211e30(0, k * u2f(0x3ca3d70au), k * u2f(0x3e99999au), fs6c() * u2f(0x4096cbe4u));
    }
    Vec4 d;
    vec3_sub(d, V(lwu(P + 0x994) + 0x10), V(P + 0x80));
    vec3_set_length(d, d, 1.0f);
    if (d.z < u2f(0x3e6b851fu)) d.z = u2f(0x3e6b851fu);
    {
        Vec4 ang;
        vec_clear(ang);
        ang.z = lwc1(P + 0x98);
        Mat4 rot, inv;
        euler_to_mat4_vu(rot, ang);
        mat3_transpose(inv, rot);
        mat4_transform(d, d, inv);
        const float pitch = -atan2_f(sqrt_f(d.x * d.x + d.z * d.z), d.y);
        const float roll = atan2_f(d.z, d.x);
        const float k = lwc1(0x0015ed64);
        const float k1 = u2f(0x3d23d70au), k2 = u2f(0x3e4ccccdu), k3 = u2f(0x411996c7u);
        spring_angle(P + 0x94, P + 0xa4, 0, roll, k * k1, k * k2, fs6c() * k3);
        const float kk = lwc1(0x0015ed64);
        spring_angle(P + 0x90, P + 0xa0, 0, pitch, kk * k1, kk * k2, fs6c() * k3);
    }
    const bool below = lwc1(P + 0x88) < lwc1(lwu(P + 0x994) + 0x18);
    Vec4 tmp;
    if (lh(P + 0x99c) != 0) {
        if (below && frame_scale_i(0x28) < lw(P + 0x198) && 0.25f < lwc1(P + 0x229c)) {
            hero_stick_move(0, 1.0f);
            float pump = fs6c() * u2f(0x3e570a3du) * lwc1(P + 0x229c);
            if (fs6c() * 5.0f < pump) pump = fs6c() * 5.0f;
            Vec4 v = {};
            v.x = cos_vu(lwc1(P + 0x180)) * pump;
            v.y = sin_vu(lwc1(P + 0x180)) * pump;
            vec3_add(V(P + 0xe0), V(P + 0xe0), v);
        }
        if (lh(P + 0x99c) != 0) {
            const float dd = vec2_distance(V(P + 0x80), V(lwu(P + 0x994) + 0x10));
            if (dd < lwc1(P + 0x9a0) * u2f(0x3fc8f5c3u) || 1.5f < lwc1(P + 0x2dc)) {
                sub_002334d0(P + 0xe0, P + 0x100, fs70() * 40.0f);
                if (lh(P + 0x9be) != 0 && 0.0f < sub_00233a78_v(V(P + 0xe0))) {
                    vec3_sub(tmp, V(P + 0x80), V(lwu(P + 0x994) + 0x10));
                    sub_002334d0(P + 0xe0, P + 0xe0, fs70() * 57.0f);
                }
            }
            const float drag = vec3_length(V(P + 0xe0)) * (fs6c() * u2f(0x3c75c28fu));
            vec3_scale(tmp, V(P + 0xe0), -1.0f);
            vec3_set_length(tmp, tmp, drag);
            vec3_add(V(P + 0xe0), V(P + 0xe0), tmp);
        }
    }
    const uint32_t t2 = lwu(P + 0x994);
    if (t2 != 0 && lwu(t2 + 0x78) != 0) {
        const uint32_t pv = lwu(t2 + 0x78);
        float len = vec3_length(V(P + 0xe0));
        if (lwc1(pv + 0xc) < len) {
            approach_f_v(len, lwc1(pv + 0xc), fs70() * 15.0f);
            vec3_set_length(V(P + 0xe0), V(P + 0xe0), len);
        }
    }
    Vec4 side = {};
    {
        const float a = angle_add(lwc1(P + 0x98), u2f(0xbfc90fdbu));
        side.x = cos_vu(a) * u2f(0x3e19999au);
        side.y = sin_vu(a) * u2f(0x3e19999au);
        vec3_add(side, side, V(P + 0x80));
    }
    Vec4 rope;
    vec3_add(tmp, side, V(P + 0xe0));
    vec3_sub(rope, tmp, V(lwu(P + 0x994) + 0x10));
    vec3_set_length(rope, rope, lwc1(P + 0x9a4));
    vec3_add(tmp, V(lwu(P + 0x994) + 0x10), rope);
    vec3_sub(V(P + 0xe0), tmp, side);
    const float old = lwc1(P + 0x9a4);
    spring_to(P + 0x9a4, P + 0x9a8, lwc1(P + 0x9a0), lwc1(P + 0x9c4), lwc1(P + 0x9c8), lwc1(P + 0x9cc));
    const float grow = lwc1(P + 0x9a4) - old;
    if (lwc1(P + 0x9a4) < vec3_length(rope)) {
        vec3_set_length(rope, rope, grow);
        vec3_add(V(P + 0x80), V(P + 0x80), rope);
    }
    if (lh(P + 0x9be) != 0 && below) {
        const float target = fs6c() * 14.0f;
        float h = vec2_length(V(P + 0xe0));
        if (h < target) {
            const float k = lwc1(0x0015ed64);
            spring_to_v(h, P + 0x9b4, target, k * u2f(0x3d408312u), k * u2f(0x3e99999au), 0.0f);
            Vec4 v = {};
            v.x = cos_vu(lwc1(P + 0x9ac)) * lwc1(P + 0x9b4);
            v.y = sin_vu(lwc1(P + 0x9ac)) * lwc1(P + 0x9b4);
            vec3_add(V(P + 0xe0), V(P + 0xe0), v);
        }
    }
    if (lh(P + 0x99c) == 0 || lh(P + 0x30e) == 0) return;
    if (!(vz0 < 0.0f && 0.0f < lwc1(P + 0xe8)) && lh(P + 0x9bc) == 0) return;
    sh(P + 0x9be, 0);
    const float rate = sqrt_f(fpu_div(lwc1(P + 0x9a0), lwc1(P + 0x9b0))) * u2f(0x40490fdbu);
    bool forward;
    if (lh(P + 0x9bc) != 0) {
        forward = lh(P + 0x9bc) == 1;
    } else {
        const Vec4 v = V(P + 0x100);
        forward = 0.0f < sub_00213350_v(v);
    }
    if (lbu(hm + 0x53) != 0x35) {
        if (forward) {
            hero_anim_set(0x35, 6, static_cast<float>(frame_scale_i(0x11)));
            sh(P + 0x9bc, 1);
        } else {
            hero_anim_set(0x35, 0x15, static_cast<float>(frame_scale_i(9)));
            sh(P + 0x9bc, 2);
        }
        return;
    }
    const uint32_t m = lwu(P + 0x2080);
    if (lbu(m + 0x52) != lbu(m + 0x53)) return;
    sub_00208f20(forward ? 42.0f : 12.0f, rate, 0.5f, -1.0f);
    sh(P + 0x9bc, 0);
}

// The comet strike (state 0x22): the lift then the dive (gravity +0x4b0, the dive flag +0x4b8),
// the hit sphere at the feet on landing (impulse 1, class record 0x30000) and the camera shake on
// the first frame off a platform.
// orig: 0x00217970 (block 0x21a720)
void comet_update() {
    sw(P + 0x190, 0);
    sub_00212088(fs70() * 30.0f, fs70() * 30.0f);
    sub_00233ba0(P + 0xe0, P + 0xe0, lwc1(P + 0x194));
    const int32_t lift = frame_scale_i(0xc);
    const float up = fs6c() * u2f(0x40b66666u), rate = fs70() * 70.0f;
    const int32_t dive = frame_scale_i(0x21);
    if (lw(P + 0x198) < lift) {
        float v = sub_00233a78_v(V(P + 0xe0));
        approach_f_v(v, up, rate);
        sub_00233b20(P + 0xe0, P + 0xe0, v);
    } else if (dive < lw(P + 0x198)) {
        sub_002334d0(P + 0xe0, P + 0xe0, lwc1(P + 0x4b0));
        if (sub_00233a78_v(V(P + 0xe0)) < 0.0f) swc1(P + 0x4b8, 1.0f);
    }
    if (lwc1(P + 0x4b8) != 0.0f && lh(P + 0x30e) != 0) {
        Vec4 p = V(P + 0x80);
        p.z = p.z - 0.5f;
        Vec4 d = {};
        d.x = cos_vu(lwc1(P + 0x98));
        d.y = sin_vu(lwc1(P + 0x98));
        alignas(16) uint32_t rec[12] = {};  // (the original leaves +0xc and +0x18 as stack garbage)
        std::memcpy(rec, &d, 16);
        rec[4] = lwu(P + 0x2080);
        rec[5] = 0x30000;
        const float one = 1.0f;
        std::memcpy(&rec[7], &one, 4);
        rec[8] = 1;
        mobys_in_sphere_v(p, u2f(0x3f4ccccdu), 0x10, lwu(P + 0x2080), rec);
    }
    if (lwc1(P + 0x4b8) == 0.0f || lh(P + 0x30e) != 0) return;
    if (lw(P + 0x4c4) == 0) {
        swc1(OV(0x00166de0), u2f(0x3e4ccccdu));
        sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x28)));
    }
    sw(P + 0x4c4, lwu(P + 0x4c4) + 1);
}

// Rail grind (states 0x28..0x2b, 0x42). The rail is a path (+0x560: count, points from +0x10)
// with index +0x564 / fraction +0x568, step direction +0x578, loop flag +0x584; a second rail
// +0x594 (+0x598 / +0x59c, loop +0x5a0) is blended in over 48 frames when switching (+0x590,
// count +0x5a4). +0x570 counts the frames since leaving; +0x574 is the speed (+0x5b0 base,
// +0x5ac slope term), +0x57c / +0x580 the rail yaw / pitch, +0x5a8 the lean side (-1 none,
// 0 / 1 slope, 2 / 3 stick), +0x5d0 / +0x5d4 the body lean spring, +0x5dc the snap spring.
// orig: 0x00217970 (block 0x21ca68)
int32_t* pi32(uint32_t a) { return static_cast<int32_t*>(ee_ram_ptr(a)); }
float* pf32(uint32_t a) { return static_cast<float*>(ee_ram_ptr(a)); }

void grind_rail_update() {
    const uint32_t st0 = lwu(P + 0x2084);
    if (st0 - 0x29u < 2u || st0 == 0x42) {
        sound_slot_stop(0x2218);
    } else {
        hero_sound_slot(0, lwu(P + 0x2080), 0);
    }
    Vec4 sp0{}, sp10{}, sp20{}, sp30{};
    bool probed = false;
    if (lw(P + 0x2084) == 0x28 || lw(P + 0x2084) == 0x2b) {  // sparks off the rail
        sp0 = V(P + 0x80);
        const float a = angle_add(lwc1(P + 0x57c), rand_range_signed_f(0.0f, u2f(0x3f060a92u)));
        const float k = fs6c();
        const float r = rand_range_f(k * 4.0f, k * 7.0f);
        sp10.x = cos_vu(a) * r;
        sp10.y = sin_vu(a) * r;
        sp10.z = 0.0f;
        const float k2 = fs6c();
        sp10.z = rand_range_f(k2 * 3.0f, k2 * 5.0f);
        sp10.w = -(fs70() * 30.0f);
        spawn_particle_19_v(sp0, sp10, 0);
    }
    const uint32_t item = lwu(P + 0x10b8);
    const bool s7 = lwu(OV(0x00179ac0) + item * 0x4c + 0x18) != 0 && item != 0xc;
    const bool s8 = s7 && lbu(P + 0x20aa) != 0;
    moby_chain_point_v(lwu(P + 0x2080), 9, V(P + 0x510));
    moby_chain_point_v(lwu(P + 0x2080), 0xe, V(P + 0x520));
    moby_chain_point_v(lwu(P + 0x2080), 0, V(P + 0x530));
    if (lwu(P + 0x5a8) - 2u < 2u) sw(P + 0x5a8, 0xffffffffu);
    path_closest_point(lwu(P + 0x560), P + 0x80, P + 0x500, pi32(P + 0x564), pf32(P + 0x568), lw(P + 0x584),
                       999.0f, 2.0f, 2.5f);
    if (lw(P + 0x590) != 0) {
        path_closest_point(lwu(P + 0x594), P + 0x80, P + 0x540, pi32(P + 0x598), pf32(P + 0x59c), lw(P + 0x5a0),
                           999.0f, 2.0f, 2.5f);
    }
    bool leaving = false;
    if (u2f(0x3f5eb852u) < lwc1(P + 0x2dc)) {
        const uint32_t st = lwu(P + 0x2084);
        if (st != 0x29 && st != 0x42 && lw(P + 0x590) == 0) leaving = frame_scale_i(0x1e) < lw(P + 0x19c);
    }
    if (lw(P + 0x570) != 0 || leaving) {  // off the rail: carry the platform velocity
        V(P + 0xe0) = V(P + 0x110);
        if (lwu(P + 0x2084) - 0x29u < 2u) {
            hero_vertical_velocity();
        } else {
            swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 24.0f);
        }
        sw(P + 0x570, lwu(P + 0x570) + 1);
        return;
    }
    float step = lwc1(P + 0x574) * static_cast<float>(lw(P + 0x578));
    if (lw(P + 0x2084) == 0x42) step = lwc1(P + 0x5cc) * static_cast<float>(lw(P + 0x578));
    path_advance_v(lwu(P + 0x560), sp0, pi32(P + 0x564), pf32(P + 0x568), lw(P + 0x584), step);
    if (lw(P + 0x590) != 0) {
        path_advance_v(lwu(P + 0x594), sp10, pi32(P + 0x598), pf32(P + 0x59c), lw(P + 0x5a0), step);
    }
    if (frame_scale_i(0x5a) < lw(P + 0x19c)) {  // look ahead: lean into the bend
        const float ahead = frame_scale_f(30.0f) * lwc1(P + 0x160);
        sp20 = V(P + 0x80);
        int32_t idx = lw(P + 0x564);
        float frac = lwc1(P + 0x568);
        const int32_t r1 = path_advance_v(lwu(P + 0x560), sp20, &idx, &frac, lw(P + 0x584), ahead);
        sp30 = V(P + 0x80);
        idx = lw(P + 0x564);
        frac = lwc1(P + 0x568);
        const int32_t r2 = path_advance_v(lwu(P + 0x560), sp30, &idx, &frac, lw(P + 0x584), ahead + u2f(0x3dcccccdu));
        probed = true;
        if ((r1 | r2) == 0 || lw(P + 0x584) != 0) {
            const float d = atan2_f(sp30.x - sp20.x, sp30.y - sp20.y);
            float lean = angle_sub(lwc1(P + 0x57c), d) * u2f(0x3ebd70a4u);
            if (u2f(0x3f490fdbu) < lean) lean = u2f(0x3f490fdbu);
            if (lean < u2f(0xbf490fdbu)) lean = u2f(0xbf490fdbu);
            fx_params_set_0(u2f(0x3cf5c28fu), u2f(0x3e4ccccdu));
            swc1(OV(0x0017a6e4), lw(P + 0x58c) != 0 ? -lean : lean);
        }
    }
    {
        const uint32_t st = lwu(P + 0x2084);
        if (st != 0x29 && st != 0x42 && st != 0x2a) swc1(OV(0x0017a6e0), atan2_f(vec2_length(V(P + 0x100)), lwc1(P + 0x108)));
    }
    if (s7) {  // holding an item: arm and body pose
        const float k = lwc1(0x0015ed64);
        fx_params_set_3(k * u2f(0x3c8b4396u), k * u2f(0x3e99999au));
        const float k1 = lwc1(0x0015ed64);
        fx_params_set_1(k1 * u2f(0x3cdd2f1bu), k1 * u2f(0x3e99999au));
        const float k2 = lwc1(0x0015ed64);
        fx_params_set_2(k2 * u2f(0x3cdd2f1bu), k2 * u2f(0x3e99999au));
        constexpr OvConst<0x0017a680u> FX{};
        const uint32_t tgt = lwu(P + 0x5d8);
        if (tgt != 0) {
            vec3_sub(sp20, V(tgt + 0x10), V(P + 0x80));
            mat3_transform(sp20, sp20, M(P + 0x40));
            const float ang = atan2_f(sp20.x, sp20.y);
            float x, w;
            if (lbu(lwu(P + 0x2080) + 0x52) == 0x31) {
                x = angle_sub(ang, u2f(0x3fc90fdbu));
                w = u2f(0x3e4ccccdu);
            } else {
                x = angle_sub(ang, u2f(0xbfc90fdbu));
                w = 0.5f;
            }
            swc1(FX + 0x278, x * u2f(0x3e99999au));
            swc1(FX + 0x1c8, x * w);
        } else {
            const uint32_t a = lbu(lwu(P + 0x2080) + 0x53);
            if (a == 0x4b) {
                swc1(FX + 0x278, angle_add(u2f(0x3e0efa35u), lwc1(P + 0x5d0)));
                swc1(FX + 0x118, angle_add(u2f(0xbd8efa35u), lwc1(P + 0x5d0) * 0.5f));
                swc1(FX + 0x1c8, angle_add(u2f(0xbd8efa35u), lwc1(P + 0x5d0) * 0.5f));
            } else if (a == 0x31 || a == 0x4a) {
                swc1(FX + 0x278, angle_add(u2f(0xbf51ff7eu), lwc1(P + 0x5d0)));
                swc1(FX + 0x118, angle_add(u2f(0x3f3ba866u), fpu_div(lwc1(P + 0x5d0), u2f(0x3fd9999au))));
                swc1(FX + 0x1c8, angle_add(u2f(0x3f3ba866u), fpu_div(lwc1(P + 0x5d0), u2f(0x3fd9999au))));
            }
        }
    }
    {  // heading along the rail
        const uint32_t path = lwu(P + 0x560);
        const int32_t next = step_index(path, lw(P + 0x564), lw(P + 0x578), lw(P + 0x584));
        vec3_sub(sp20, V(path + 0x10 + static_cast<uint32_t>(next) * 16u), sp0);
        if (vec3_length(sp20) < u2f(0x3a83126fu)) {
            if (lw(P + 0x564) == next) {
                sw(P + 0x570, static_cast<uint32_t>(frame_scale_i(1)));
                return;
            }
        } else {
            swc1(P + 0x57c, atan2_f(sp20.x, sp20.y));
            swc1(P + 0x580, atan2_f(vec2_length(sp20), sp20.z));
        }
    }
    swc1(P + 0x180, angle_add(lwc1(P + 0x57c), lwc1(P + 0x588)));
    {  // turn to it
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k * u2f(0x3c75c28fu), k * u2f(0x3e19999au), fs6c() * u2f(0x40c90fdbu));
    }
    const uint32_t st = lwu(P + 0x2084);
    if ((st - 0x29u < 2u && lw(P + 0x418) == 0) || st == 0x42) {  // riding along without grinding
        if (lw(P + 0x5c4) != 0) {
            const float yaw = atan2_f(lwc1(P + 0x550) - lwc1(P + 0x80), lwc1(P + 0x554) - lwc1(P + 0x84));
            if (angle_dist(lwc1(P + 0x98), yaw) < u2f(0x3fc90fdbu)) {
                const float k = lwc1(0x0015ed64);
                swc1(P + 0x180, yaw);
                sub_00211e30(0, k * u2f(0x3d23d70au), k * u2f(0x3e4ccccdu), fs6c() * u2f(0x40c90fdbu));
            }
        }
        if (lw(P + 0x590) != 0 && !(frame_scale_i(0x30) < lw(P + 0x5a4))) {  // blend onto the new rail
            Vec4 d;
            vec3_sub(d, sp0, sp10);
            sw(P + 0x5a4, lwu(P + 0x5a4) + 1);
            vec3_scale(d, d, fpu_div(static_cast<float>(lw(P + 0x5a4)), static_cast<float>(frame_scale_i(0x30))));
            const float t = fpu_div(static_cast<float>(lw(P + 0x5a4) - 1), static_cast<float>(frame_scale_i(0x30)));
            vec3_sub(sp30, sp0, sp10);
            vec3_scale(sp30, sp30, t);
            swc1(P + 0x88, lwc1(P + 0x88) + (d.z - sp30.z));
            vec3_add(sp30, sp10, d);
            vec3_sub(d, sp30, V(P + 0x80));
            swc1(P + 0xe0, d.x);
            swc1(P + 0xe4, d.y);
            vec3_sub(d, sp0, V(P + 0x500));
            swc1(P + 0x88, lwc1(P + 0x88) + d.z);
            const bool blending = !(frame_scale_i(0x30) < lw(P + 0x5a4));
            if (lwc1(P + 0x88) < lwc1(P + 0x508) && blending) {  // not below the rail while blending
                swc1(P + 0x88, lwc1(P + 0x508));
            }
        } else {
            Vec4 d;
            vec3_sub(d, sp0, V(P + 0x500));
            swc1(P + 0xe0, d.x);
            swc1(P + 0xe4, d.y);
            swc1(P + 0x88, lwc1(P + 0x88) + d.z);
            vec3_sub(sp30, V(P + 0x500), V(P + 0x80));
            sp30.z = 0.0f;
            if (fs6c() < vec3_length(sp30)) vec3_set_length(sp30, sp30, fs6c());
            swc1(P + 0x80, lwc1(P + 0x80) + sp30.x);
            swc1(P + 0x84, lwc1(P + 0x84) + sp30.y);
        }
        if (lw(P + 0x2084) == 0x42) {
            swc1(P + 0xe8, lwc1(P + 0xe8) - fs70() * 23.0f);
        } else {
            hero_vertical_velocity();
        }
        return;
    }
    // grinding proper
    const float top = fs6c() * 12.0f;
    sw(P + 0x590, 0);
    if (0.5f < lwc1(P + 0x229c) && !s8) {  // the stick picks the lean side
        hero_stick_move(0, 1.0f);
        const float HALF_PI = u2f(0x3fc90fdbu);
        const float rel = angle_dist(lwc1(P + 0x180), lwc1(OV(0x00166dd8)));
        if (angle_dist(rel, HALF_PI) < u2f(0x3f9c61aau) || angle_dist(rel, u2f(0xbfc90fdbu)) < u2f(0x3f9c61aau)) {
            const float x = angle_sub(lwc1(P + 0x180), lwc1(OV(0x00166dd8)));
            sw(P + 0x5a8, angle_dist(x, HALF_PI) < u2f(0x3f490fdbu) ? 2u : 3u);
        }
    }
    float target = 0.0f;
    float k = lwc1(0x0015ed64);
    if (0.2f < lwc1(P + 0x229c) && s8) {  // aiming: lean towards the stick
        hero_stick_move(0, 1.0f);
        target = angle_sub(lwc1(P + 0x180), lwc1(P + 0x57c));
        if (u2f(0x3f490fdbu) < target) {
            target = u2f(0x3f490fdbu);
        } else if (target < u2f(0xbf490fdbu)) {
            target = u2f(0xbf490fdbu);
        }
        k = lwc1(0x0015ed64);
    }
    spring_angle(P + 0x5d0, P + 0x5d4, 0, target, k * u2f(0x3c75c28fu), k * u2f(0x3e99999au), 0.0f);
    approach_f(P + 0x5b0, top, fs70() * 15.0f);
    {  // the slope ahead
        int32_t idx = lw(P + 0x564);
        float frac = lwc1(P + 0x568);
        sp20 = V(P + 0x80);
        path_advance_v(lwu(P + 0x560), sp20, &idx, &frac, lw(P + 0x584), u2f(0x3e99999au));
    }
    const float slope = atan2_f(vec2_distance(V(P + 0x500), sp20), sp20.z - lwc1(P + 0x508));
    float goal;
    if (0.0f < slope) {  // uphill
        goal = slope * u2f(0x3ff47645u) * (fs6c() * -2.5f);
        if (u2f(0x3e860a92u) < slope && lw(P + 0x5a8) != 2 && lw(P + 0x5a8) != 3) {
            sw(P + 0x5a8, 0);
        } else if (slope < u2f(0x3e0efa35u) && lw(P + 0x5a8) == 0) {
            sw(P + 0x5a8, 0xffffffffu);
        }
    } else {  // downhill
        goal = -slope * u2f(0x403758b4u) * (fs6c() * 8.0f);
        if (slope < u2f(0xbe449809u) && lw(P + 0x5a8) != 2 && lw(P + 0x5a8) != 3) {
            sw(P + 0x5a8, 1);
        } else if (u2f(0xbdfa35ddu) < slope && lw(P + 0x5a8) == 1) {
            sw(P + 0x5a8, 0xffffffffu);
        }
    }
    if (lwc1(P + 0x5ac) < goal) {
        approach_f(P + 0x5ac, goal, fs70() * 8.0f);
    } else {
        approach_f(P + 0x5ac, goal, fs70() + fs70());
    }
    if (fs6c() * 12.0f < lwc1(P + 0x5ac)) swc1(P + 0x5ac, fs6c() * 12.0f);
    if (lwc1(P + 0x5ac) < fs6c() * -2.5f) swc1(P + 0x5ac, fs6c() * -2.5f);
    swc1(P + 0x574, lwc1(P + 0x5b0) + lwc1(P + 0x5ac));
    if (lwc1(P + 0x574) < 0.0f) swc1(P + 0x574, 0.0f);
    vec3_sub(V(P + 0xe0), sp0, V(P + 0x500));
    vec3_sub(sp20, V(P + 0x500), V(P + 0x80));
    {  // snap onto the rail through a spring
        const float vmax = fs6c() * 7.0f;
        float gap = -vec3_length(sp20);
        const float kk = lwc1(0x0015ed64);
        spring_to_v(gap, P + 0x5dc, 0.0f, kk * u2f(0x3cf5c28fu), kk * u2f(0x3e99999au), vmax);
        const float len = vec3_length(sp20);
        if (len < lwc1(P + 0x5dc)) swc1(P + 0x5dc, len);
        if (lwc1(P + 0x5dc) < len) vec3_set_length(sp20, sp20, lwc1(P + 0x5dc));
        vec3_add(V(P + 0x80), V(P + 0x80), sp20);
    }
    if (lwc1(P + 0x88) < lwc1(P + 0x508) && lwc1(P + 0x2dc) < 1.0f) swc1(P + 0x88, lwc1(P + 0x508));
    if (lw(P + 0x2084) != 0x2b || !(lw(P + 0x198) < frame_scale_i(0x14))) return;
    // 0x2b, the first frames: hit what is at the landing point
    const float yaw = angle_add(lwc1(P + 0x57c), rand_range_signed_f(u2f(0x3eb2b8c2u), u2f(0x3f1c61aau)));
    Vec4 dir{};
    dir.x = cos_vu(yaw);
    dir.y = sin_vu(yaw);
    dir.z = 0.0f;
    alignas(16) uint32_t rec[12] = {};  // (the original leaves +0xc, and +0x18 unless probed, as stack garbage)
    std::memcpy(rec, &dir, 16);
    if (probed) std::memcpy(&rec[6], &sp30.z, 4);
    rec[4] = lwu(P + 0x2080);
    rec[5] = 0x10000;
    const float one = 1.0f;
    std::memcpy(&rec[7], &one, 4);
    rec[8] = 1;
    Vec4 c = V(P + 0x80);
    c.z = c.z + u2f(0x3f19999au);
    mobys_in_sphere_v(c, 1.0f, 0x10, lwu(P + 0x2080), rec);
}

// Swimming (states 0x33 stroke, 0x34 drift, 0x35 the hydro-pack). The pad's two pressure
// buttons (0x40 at +0x128, 0x80 at +0x12c) pitch the body down / up through the spring
// +0x94 / +0x8e8 (counters +0x900 / +0x902 ease the response in); holding 0x40 hard for 1.5 s
// counts a stat (0x1418a0); 0x910 counts quick taps. Bubbles leave the mouth at random
// intervals (+0x8f0); with the hydro-pack (moby +0x1180, class 0x261) its two nozzles stream.
// The velocity follows the pitch and the speed +0x194; the roll +0x90 the turn rate.
// orig: 0x00217970 (block 0x21823c)
void swim_update() {
    constexpr uint32_t PAD = 0x0013c940;
    if (lw(P + 0x2084) == 0x35) {
        hero_sound_slot(5, lwu(P + 0x2080), 0x13);
    } else {
        sound_slot_stop(0x222c);
    }
    if (lh(P + 0x90a) != 0) {  // the entry rumble
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(P + 0x90a)));
        int32_t n = static_cast<int32_t>(static_cast<uint32_t>(lhu(P + 0x90a)) << 16) >> 18;
        if (!(n < 9)) n = 8;
        hero_smoke_puffs(n, 0);
    }
    if (lw(P + 0x2084) == 0x33 && lw(P + 0xa9c) == 0 && 0.0f < lwc1(P + 0xaa8) && lwc1(P + 0xaa8) < 10.0f) {
        hero_smoke_puffs(4, 1);
    }
    if (lw(P + 0x2084) == 0x34 && lw(P + 0x2090) == 0x35 && lw(P + 0x198) < frame_scale_i(0xf)) {
        hero_smoke_puffs((frame_scale_i(0x14) - lw(P + 0x198)) / 3 + 1, 1);
    }
    if (lw(P + 0x2088) == 1) {  // diving in
        if (lw(P + 0x198) == 1) {
            hero_land_fx(3, 0x10, 0);
            const uint32_t regs = lwu(OV(0x001612d0));
            if (regs != 0) {
                region_height_splash(regs, lw(OV(0x001612d8)), 0, lwc1(P + 0x80), lwc1(P + 0x84), u2f(0x3ecccccdu),
                                     u2f(0xbe99999au));
            }
            sh(P + 0x90a, static_cast<uint32_t>(frame_scale_i(0x46)));
        }
        const int32_t t = lw(P + 0x8e0) + 1;
        sw(P + 0x8e0, static_cast<uint32_t>(t));
        if (frame_scale_i(0x28) < t) sw(P + 0x2088, 0);
    }
    // the pitch target from the pressure buttons
    float vmax = fs6c() * u2f(0x3f32b8c2u);
    const float p128 = lwc1(PAD + 0x128), p12c = lwc1(PAD + 0x12c);
    const float press = p128 < p12c ? p12c : p128;
    const float k = lwc1(0x0015ed64);
    const float d = k * 0.2f, kk = k * u2f(0x3c75c28fu);
    vmax = vmax + (fs6c() * u2f(0x3f9c61aau) - vmax) * press;
    float target = 0.0f;
    if ((lwu(PAD + 0x1a0) & 0xc0) == 0x80 || lw(P + 0x2088) == 1) {  // up
        float f = p12c * 1.5f;
        if (1.0f < f) f = 1.0f;
        if (f < 0.2f) f = 0.2f;
        if (lw(PAD + 0x1dc) != 0x79) {
            sh(P + 0x900, 0);
            sh(P + 0x902, lhu(P + 0x902) + 1u);
            f = fpu_div(static_cast<float>(lh(P + 0x902)) * 0.5f, static_cast<float>(frame_scale_i(0x14))) + 0.5f;
            if (1.0f < f) f = 1.0f;
        }
        if (lw(P + 0x2088) == 1 && f < u2f(0x3eb33333u)) f = u2f(0x3eb33333u);
        target = f * u2f(0x3fae40f1u);
    } else if ((lwu(PAD + 0x1a0) & 0x40) != 0) {  // down
        float f = p128 * 1.5f;
        if (1.0f < f) f = 1.0f;
        if (f < 0.2f) f = 0.2f;
        if (lw(PAD + 0x1dc) != 0x79) {
            sh(P + 0x902, 0);
            sh(P + 0x900, lhu(P + 0x900) + 1u);
            f = fpu_div(static_cast<float>(lh(P + 0x900)) * 0.5f, static_cast<float>(frame_scale_i(0x14))) + 0.5f;
            if (1.0f < f) f = 1.0f;
        }
        if (static_cast<float>(lw(P + 0x22a0)) < 1500.0f) f = 1.0f;
        target = f * u2f(0xbfae40f1u);
    } else {
        sh(P + 0x902, 0);
        sh(P + 0x900, 0);
    }
    if ((lwu(PAD + 0x1a0) & 0x40) != 0) {  // held down: the long-dive stat
        sh(P + 0x90e, lhu(P + 0x90e) + 1u);
        if (0.75f < lwc1(PAD + 0x128)) {
            const uint32_t c = lhu(P + 0x90c) + 1u;
            sh(P + 0x90c, c);
            constexpr uint32_t STAT = 0x00141848 + 0x58;  // (hero_stat_use, its count only after 1.5 s)
            if (frame_scale_i(0x5a) < static_cast<int16_t>(c) && !(0xfffeu < lhu(STAT))) sh(STAT, lhu(STAT) + 1u);
            const int32_t secs = frame_scale_i(lw(0x0015eea4)) / 600;
            if (static_cast<int32_t>(lhu(STAT + 2)) < secs) sh(STAT + 2, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 600));
            sw(STAT + 4, lwu(STAT + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
        } else {
            sh(P + 0x90c, 0);
        }
    } else {
        sh(P + 0x90c, 0);
        sh(P + 0x90e, 0);
    }
    if ((lwu(PAD + 0x1a4) & 0x40) != 0) {  // taps
        if (lw(OV(0x0015f5cc)) - lw(P + 0x914) < frame_scale_i(0x46)) sw(P + 0x910, lwu(P + 0x910) + 1);
        sw(P + 0x914, lwu(OV(0x0015f5cc)));
    }
    if (frame_scale_i(0x46) < lh(P + 0x90e)) sw(P + 0x910, 0);
    if ((lwu(PAD + 0x1a0) & 0x80) != 0) sw(P + 0x910, 0);
    if (lw(P + 0x2084) == 0x34) target = 0.0f;
    const float pitch0 = lwc1(P + 0x94);
    spring_angle(P + 0x94, P + 0x8e8, 0, target, kk, d, vmax);
    const float dpitch = angle_sub(lwc1(P + 0x94), pitch0);
    hero_stick_move(0, 1.0f);
    {  // turn
        const float k2 = lwc1(0x0015ed64);
        sub_00211e30(0, k2 * u2f(0x3be56042u), k2 * u2f(0x3da3d70au), fs6c() * u2f(0x40a78d36u));
    }
    swc1(P + 0x8ec, lwc1(P + 0x184));
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x8f4)));
    float speed;
    if (lw(P + 0x2088) == 1) {
        speed = fs6c() * 6.0f;
    } else if (lw(P + 0x2084) == 0x35) {
        speed = (lwu(PAD + 0x1a0) & 0xa) != 0 ? fs6c() * 7.0f : fs6c() * 7.0f * u2f(0x3ecccccdu);
    } else {
        speed = fs6c() * 3.0f;
    }
    swc1(P + 0x190, speed);
    float stick = lwc1(P + 0x229c);
    if ((lwu(PAD + 0x1a0) & 0xc0) != 0 || lw(P + 0x2088) == 1) {
        if (stick < 1.0f) stick = 1.0f;
    }
    if (u2f(0x3e19999au) < lwc1(P + 0x229c)) {
        const float lo = lw(P + 0x2084) == 0x35 ? u2f(0x3ecccccdu) : u2f(0x3eb33333u);
        if (stick < lo) stick = lo;
    }
    if (lw(P + 0x2084) != 0x35 || (lwu(PAD + 0x1a0) & 0xa) == 0) swc1(P + 0x190, lwc1(P + 0x190) * stick);
    const int32_t bub = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(P + 0x8f0)));
    if (bub != 0) {  // a bubble from the mouth
        Vec4 pos, off{};
        moby_chain_point_v(lwu(P + 0x2080), 4, pos);
        off.x = u2f(0x3ecccccdu);
        mat3_transform(off, off, M(lwu(P + 0x2080) + 0xc0));
        vec3_add(pos, pos, off);
        Vec4 vel = V(P + 0x100);
        vec3_set_length(off, off, fs6c() * 1.5f);
        vec3_add(vel, vel, off);
        vel.z = vel.z + (fs6c() + fs6c());
        spawn_particle_22_v(pos, vel, u2f(0x46c4e000u), -1.0f);
        int32_t lo, hi;
        if (static_cast<float>(rand_mod(100)) < 40.0f) {
            lo = frame_scale_i(4);
            hi = frame_scale_i(0xb);
        } else {
            lo = frame_scale_i(0x28);
            hi = frame_scale_i(0x5a);
        }
        sw(P + 0x8f0, static_cast<uint32_t>(rand_range(lo, hi)));
    }
    if (lw(P + 0x2084) == 0x35) {  // the hydro-pack's nozzles
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(P + 0x8f8))) != 0) {
            const uint32_t pack = lwu(P + 0x1180);
            if (pack != 0 && lh(pack + 0xa6) == 0x261) {
                for (int32_t i = 0; i < 4; ++i) {
                    Vec4 pos, off{};
                    moby_chain_point_v(pack, static_cast<uint32_t>(i % 2), pos);
                    off.x = u2f(0xbe19999au);
                    mat3_transform(off, off, M(lwu(P + 0x2080) + 0xc0));
                    vec3_add(pos, pos, off);
                    pos.x = pos.x + rand_range_f(0.0f, u2f(0x3cf5c28fu));
                    pos.y = pos.y + rand_range_f(0.0f, u2f(0x3cf5c28fu));
                    pos.z = pos.z + rand_range_f(0.0f, u2f(0x3cf5c28fu));
                    Vec4 vel = V(P + 0x100);
                    Vec4 s = {};  // (the original's w lane here is stack garbage)
                    vec_from_spherical_v(s, fs6c() * -0.75f, lwc1(P + 0x98), lwc1(P + 0x94));
                    vec3_add(vel, s, vel);
                    const float top = fs6c() * 7.0f;
                    sh(P + 0x8f8, static_cast<uint32_t>(float_to_int(fpu_div(top - lwc1(P + 0x194), top) *
                                                                     static_cast<float>(frame_scale_i(5)))));
                    spawn_particle_22_v(pos, vel, rand_range_f(u2f(0x45c4e000u), u2f(0x4613a800u)), -1.0f);
                }
            }
        }
    }
    if (lw(P + 0x2084) == 0x35) {
        sub_00212088(fs70() * 13.0f, fs70() * 8.0f);
    } else {
        sub_00212088(fs70() * 7.0f, fs70() * 4.0f);
    }
    const float pitch = angle_add(-lwc1(P + 0x94), 0.0f);
    const uint32_t st = lwu(P + 0x2084);
    if (st == 0x35) {
        vec_from_spherical_v(V(P + 0xe0), lwc1(P + 0x194), lwc1(P + 0x98), pitch);
    } else if (st == 0x33) {  // the stroke's speed curve
        float v = lwc1(P + 0x194) * sub_00209018(OV(0x0017bfc0), 0x1e) * 5.0f;
        if (lw(P + 0x2090) == 0x35 && lw(P + 0x198) < frame_scale_i(0xf) && v < fs6c() * 4.5f) v = fs6c() * 4.5f;
        vec_from_spherical_v(V(P + 0xe0), v, lwc1(P + 0x98), pitch);
    } else if (st == 0x34) {  // drifting: the velocity +0x150 dies down
        float v = vec3_length(V(P + 0x150));
        approach_f_v(v, 0.0f, lw(P + 0x2090) == 0x35 ? fs70() * 8.0f : fs70() * 4.0f);
        vec3_set_length(V(P + 0x150), V(P + 0x150), v);
        V(P + 0xe0) = V(P + 0x150);
    }
    hero_wall_slide(0);
    const float roll = lwc1(P + 0x8ec) * cos_vu(pitch) * -7.0f;
    spring_angle(P + 0x90, P + 0x8e4, 0, roll, u2f(0x3be56042u), u2f(0x3e2e147bu), fs6c() * u2f(0x3f9c61aau));
    fx_params_set_1(u2f(0x3c1374bcu), u2f(0x3e6147aeu));
    fx_params_set_2(u2f(0x3d23d70au), 0.2f);
    fx_params_set_3(u2f(0x3ca3d70au), 0.2f);
    constexpr OvConst<0x0017a680u> FX{};
    const float bend = dpitch * 47.0f;
    swc1(FX + 0x114, bend);
    swc1(FX + 0x118, roll * 1.5f);
    swc1(FX + 0x110, roll * u2f(0x3fb33333u));
    if (u2f(0x3f5f66f3u) < bend) {
        swc1(FX + 0x114, u2f(0x3f5f66f3u));
    } else if (bend < u2f(0xbf5f66f3u)) {
        swc1(FX + 0x114, u2f(0xbf5f66f3u));
    }
    const float b40 = dpitch * 40.0f, r11 = roll * u2f(0x3f8ccccdu);
    swc1(FX + 0x414, u2f(0x3cf5c28fu));
    swc1(FX + 0x418, u2f(0x3e6147aeu));
    swc1(FX + 0x3d8, r11);
    swc1(FX + 0x3d4, b40);
    swc1(FX + 0x364, u2f(0x3cf5c28fu));
    swc1(FX + 0x368, u2f(0x3e6147aeu));
    swc1(FX + 0x324, b40);
    swc1(FX + 0x328, r11);
}

// The per-state update. Each case is one target of the jump table 0x1e7e20.
// orig: 0x00217970
void update_state(uint32_t state, uint32_t hm) {
    switch (state) {
    case 0x00: case 0x01: case 0x03: case 0x04: case 0x17: case 0x1d: case 0x1e: case 0x1f:
    case 0x3d: case 0x70: case 0x71: case 0x72: case 0x80:
        idle_update();
        return;
    case 0x7f: {  // warp-out: the effect moby 0x52a follows the hero moby and fades in
        if (lwu(P + 0x6a4) == 0) {
            const uint32_t m = moby_spawn(0x52a);
            sw(P + 0x6a4, m);
            if (m != 0) {
                V(m + 0x10) = V(P + 0x80);
                V(m + 0x40) = V(P + 0x90);
                sh(m + 0x32, 0x40);
                sb(lwu(P + 0x6a4) + 0x31, 1);
                sb(lwu(P + 0x6a4) + 0x30, 0);
                sb(lwu(P + 0x6a4) + 0x23, 0);
                const uint32_t e = lwu(P + 0x6a4);
                sh(e + 0x34, lhu(e + 0x34) | 0x800u);
                moby_update_transform(lwu(P + 0x6a4));
                moby_sound_start(0, 0, lwu(P + 0x6a4));
            }
        }
        if (lwu(P + 0x6a4) != 0) {
            const int32_t n = float_to_int(lwc1(0x0015ed60) * 5.0f);
            const uint32_t m = lwu(P + 0x6a4);
            sb(m + 0x23, lbu(m + 0x23) + static_cast<uint32_t>(n));
            if (!(lbu(lwu(P + 0x6a4) + 0x23) < 0x81)) sb(lwu(P + 0x6a4) + 0x23, 0x80);
            const uint32_t hm = lwu(P + 0x2080), e = lwu(P + 0x6a4);
            V(e + 0x10) = V(hm + 0x10);
            V(e + 0x40) = V(hm + 0x40);
            moby_update_transform(e);
        }
        sw(P + 0xe8, 0);
        const float len = vec2_length(V(P + 0xe0));
        vec2_clamp_length(V(P + 0xe0), V(P + 0xe0), lwc1(0x0015ed60) * u2f(0xbda3d708u) * len + len);
        sub_00227d40();
        return;
    }
    case 0x02:
    case 0x73:
        run_update();
        return;
    case 0x07: case 0x09: case 0x0a: case 0x0b: case 0x0c: case 0x0d: case 0x0e: case 0x0f:
    case 0x10: case 0x11: case 0x12: case 0x1c: case 0x3c: case 0x49: case 0x4c: case 0x5e:
    case 0x69:
        jump_update();
        return;
    case 0x06:
    case 0x2d:
        slide_update();
        return;
    case 0x77:
        slide_stop_update();
        return;
    case 0x81:
        crouch_update();
        return;
    case 0x08:
        hang_update();
        return;
    case 0x14:
        slam_update();
        return;
    case 0x13:
        swing_update();
        return;
    case 0x15:
        spin_update();
        return;
    case 0x21:
    case 0x7a:
        hop_update();
        return;
    case 0x20:
        strike_update();
        return;
    case 0x23:
        throw_stance_update();
        return;
    case 0x3f:
        beam_update();
        return;
    case 0x2f:
        board_update(hm);
        return;
    case 0x31:
        surface_swim_update(hm);
        return;
    case 0x74:
        grind_update();
        return;
    case 0x18:
    case 0x19:
        ledge_hang_update(false);
        return;
    case 0x4a:
    case 0x4b:
        ledge_hang_update(true);
        return;
    case 0x1a:
    case 0x1b:
        shimmy_update(false);
        return;
    case 0x4d:
    case 0x4e:
        shimmy_update(true);
        return;
    case 0x44:
    case 0x50:
        wade_update();
        return;
    case 0x16:
        skid_update();
        return;
    case 0x65:
        walk_to_update();
        return;
    case 0x66:
        wait_at_update();
        return;
    case 0x51:
        crouch_swing_update();
        return;
    case 0x4f:
        swim_idle_update();
        return;
    case 0x60:
        clank_punch_update();
        return;
    case 0x61:
        clank_pound_update(hm);
        return;
    case 0x5f:
        clank_fire_update();
        return;
    case 0x5c:
        clank_land_update();
        return;
    case 0x5b:
        clank_walk_update();
        return;
    case 0x76:
        dive_update();
        return;
    case 0x75:
        dive_75_update();
        return;
    case 0x6a:
    case 0x82:
        underwater_update();
        return;
    case 0x36:
    case 0x37:
        surface_update();
        return;
    case 0x38:
    case 0x39:
    case 0x3a:
        ride_update();
        return;
    case 0x30:
        talk_update();
        return;
    case 0x24:
    case 0x2e:
        swing_aim_update(hm);
        return;
    case 0x25:
        swing_pull_update(hm);
        return;
    case 0x26:
        swing_hang_update();
        return;
    case 0x2c:
        swing_update(hm);
        return;
    case 0x22:
        comet_update();
        return;
    case 0x40:
    case 0x67:
    case 0x63:
    case 0x64:
        vec_clear(V(P + 0xe0));
        return;
    case 0x68: {  // bounce: fall slowly while still far below the target height
        vec_clear(V(P + 0xe0));
        const float need = static_cast<float>(lw(P + 0x1660) + 1) * 0.5f;
        float v = fs6c();
        if (need < lwc1(P + 0x2f4) - lwc1(P + 0x88)) v = v * 0.25f;
        swc1(P + 0xe8, -v);
        return;
    }
    case 0x7b:
        vec_clear(V(P + 0xe0));
        swc1(P + 0xe8, -fs6c());
        return;
    case 0x7c:
        sub_00217450();
        vec_clear(V(P + 0xe0));
        swc1(P + 0xe8, -fs6c());
        return;
    case 0x7d:
        vec_clear(V(P + 0xe0));
        swc1(P + 0xe8, -(fs6c() * u2f(0x3eb33333u)));
        return;
    case 0x43:
    case 0x47:
        stand_update(false);
        return;
    case 0x53:
    case 0x57:
    case 0x59:
    case 0x5a:
    case 0x62:
        stand_update(true);
        return;
    case 0x46:
    case 0x5d:
    case 0x56:
        air_drift();
        return;
    case 0x52:
    case 0x58:
    case 0x79:
        glide_update();
        return;
    case 0x45:  // (enhancement: Clank = Modern, #57: more control in Clank's jump)
        if (advanced_clank_modern()) paddle_update(CLANK_AIR, CLANK_AIR_EASE, -10.0f);
        else paddle_update(3.0f, 12.0f, -10.0f);
        return;
    case 0x55:
        paddle_update(1.5f, 21.0f, -15.0f);
        return;
    case 0x54: {
        const float turn = angle_dist(lwc1(P + 0x98), lwc1(P + 0x180));
        hero_stick_move(0, fs6c() * 1.5f);
        const float minv = fs6c() * 1.5f * u2f(0x3f0ccccdu);
        if (lwc1(P + 0x190) < minv) swc1(P + 0x190, minv);
        float slow = 1.0f - turn * u2f(0x3e99999au);
        const float g = lwc1(P + 0x229c) + u2f(0x3eb33333u);
        if (slow < 0.0f) slow = 0.0f;
        const float k = lwc1(0x0015ed64);
        sub_00211e30(0, k * u2f(0x3d0f5c29u) * g, k * u2f(0x3dcccccdu), fs6c() * u2f(0x4011361eu) * g);
        sub_00212088(fs70() * 7.5f * slow, fs70() * 8.5f);
        if (lh(P + 0x3b8) != 0 && lh(P + 0x308) == 0) {
            sub_002120d8(lwc1(P + 0x180));
        } else {
            sub_002120d8(99999.0f);
        }
        hero_wall_slide(1);
        ground_end();
        return;
    }
    case 0x33: case 0x34: case 0x35:
        swim_update();
        return;
    case 0x28: case 0x29: case 0x2a: case 0x2b: case 0x42:
        grind_rail_update();
        return;
    default:  // 5, 0x27, 0x32, 0x3b, 0x3e, 0x41, 0x48, 0x6b..0x6f, 0x78, 0x7e: nothing per frame
        return;
    }
}
}  // namespace

// orig: (helper) the state 0x74 block of 0x00217970 (grind_update), for Kerwan's own state update
void hero_grind_update_v() { grind_update(); }

// orig: 0x00217970 — per-frame update of the hero's current state (PLAYER+0x2084); a change of
// the sub-state +0x2088 resets the timer +0x1a0, a change of the hero moby's animation +0x53
// resets +0x1a4.
void hero_state_update() {
    if (on_blarg()) { blg_call(blg_hero_state_update_u); return; }  // (level 6's own: L6:0x0022e020)
    if (on_batalia()) { bat_call(bat_hero_state_update_u); return; }  // (level 8's own: L8:0x002284e0)
    if (on_pokitaru()) { pok_call(pok_hero_state_update_u); return; }  // (level 11's own: L11:0x00246b70)
    if (on_hoven()) { hov_call(hov_hero_state_update_u); return; }  // (level 12's own: L12:0x00238900)
    if (on_oltanis()) { olt_call(olt_hero_state_update_u); return; }  // (level 14's own: L14:0x00228cd0)
    if (on_quartu()) { qua_call(qua_hero_state_update_u); return; }  // (level 15's own: L15:0x0020ed50)
    if (on_kalebo()) { kal_call(kal_hero_state_update_u); return; }  // (level 16's own: L16:0x00214598)
    if (on_veldin_finale()) { vf_call(vf_hero_state_update_u); return; }  // (level 18's own: L18:0x0021eac8)
    if (on_veldin_orbit()) { vo_call(vo_hero_state_update_u); return; }  // (level 17's own: L17:0x00214760)
    if (on_rilgar()) { ril_call(ril_hero_state_update_u); return; }  // (Rilgar's own: L5:0x00244a70)
    if (on_l1_family()) return hero_state_update_l1();
    const uint32_t hm = lwu(P + 0x2080);
    const uint32_t sub = lwu(P + 0x2088);
    const uint32_t anim = lbu(hm + 0x53);
    const uint32_t state = lwu(P + 0x2084);
    if (state < 0x83) update_state(state, hm);
    if (sub != lwu(P + 0x2088)) sw(P + 0x1a0, 0);
    if (anim != lbu(hm + 0x53)) sw(P + 0x1a4, 0);
}

}  // namespace rac1
