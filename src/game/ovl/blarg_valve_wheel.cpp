// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1118, a wheel that turns two geared mobys and, once
// spun up, waits for Ratchet's button (a scene: the camera at a box rises while bursts of particles go off at the marked
// boxes it passes, then swings behind Ratchet and the scene's moby is set going), and its particle bursts. Header
// blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data is L6_ADDR (the scene's
// camera angles 0x162198 .. 0x1621b0, the bursts' tuning 0x1621b4 / 0x1621b8 and colour tables 0x2019a0 / 0x2019b8), the
// base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position, +0x98 his yaw)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
const float kQuarterPi = u2f(0x3f490fdbu);
const float kDegree = u2f(0x3c8efa35u);  // pi / 180

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the moby of the level's moby table at index `i`
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the scene's screen fade from the frame count `t`: 1 - t / (10 frames)
void wheel_fade(int32_t t) {
    swc1(L6_ADDR(0x0015f3fc), fpu_sub(1.0f, fpu_div(int_to_float(t), static_cast<float>(frame_scale_i(0xa)))));
}

// The camera behind Ratchet's box `box` (2.5 back along y, 1 up) at `at`, its rotation at `rot`: pitched -15 degrees,
// with Ratchet's yaw.
void camera_behind(int32_t box, uint32_t at, uint32_t rot) {
    lq_sq(at, lvd::box(lvd::BOXES, box) + 0x30);
    const float z = fpu_add(lwc1(at + 8), 1.0f);
    const float y = fpu_sub(lwc1(at + 4), 2.5f);
    swc1(at + 8, z);
    swc1(at + 4, y);
    vec_clear(V(rot));
    swc1(rot + 4, u2f(0xbe860a92u));  // -15 degrees
    swc1(rot + 8, lwc1(PLAYER + 0x98));
}
}  // namespace

// The wheel's particle bursts at `at`: 0x1621b8 bursts of type 0x0b, each in a colour picked at random from the six of the
// table 0x2019a0 and a second one from the six of 0x2019b8, 15 .. 20 particles of a 25 .. 30-frame life, sized
// 0x1621b4 * 400000, at 8 .. 10 times 0x1621b4 a frame.
// orig: L6:0x00304818
void wheel_burst_l6(uint32_t at) {
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t still = sp;  // (the bursts' velocity: none)
    vec_clear(V(still));
    for (int32_t i = 0; i < lw(L6_ADDR(0x001621b8)); ++i) {
        const float speed = fpu_mul(rand_range_f(8.0f, 10.0f), dt());
        const uint32_t color = lwu(L6_ADDR(0x002019a0) + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const uint32_t second = lwu(L6_ADDR(0x002019b8) + (static_cast<uint32_t>(rand_mod(6)) << 2));
        const float size = fpu_mul(lwc1(L6_ADDR(0x001621b4)), 400000.0f);
        const int32_t count = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t life = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(at, still, color, second, static_cast<uint32_t>(count), static_cast<uint32_t>(life), 0, 0, size,
                          fpu_mul(speed, lwc1(L6_ADDR(0x001621b4))));
    }
    ee_stack_free(0x110);
}

// Class 1118, a wheel geared to two mobys. pvars: +0 the box of the scene's camera, +4 the box Ratchet is put at, +8 ..
// +0x28 nine boxes marked along the camera's rise (their rotation z the point, in hundreds of degrees of the rise, where
// their burst goes off), +0x2c / +0x30 the turn (or the scene's rise) and its rate (seek_1d), +0x34 the moby set going at
// the end (state 2), +0x38 / +0x3c the two geared mobys, +0x40 a timer, +0x44 a minute's count (set at the end), +0x48.
// States: 0 set up (turned back 45 degrees, its byte +0x30 0xff, +0x48 -1) (1); 1 waits (another moby sets 2); 2 spins up to a full
// turn rate (1), turning by its rate times 45 degrees a frame, the two geared mobys the other way; then its sound 1 and
// the base flag 0x13d3aa (3); 3 within 3.7 of Ratchet the help text 0x1783, and his button (0x13cae4 bit 4) while it is
// shown: the sound 2 (4, 30 frames); 4 the wait, then the screen fades, the scripted camera at the box +0, the scripted
// mode (0x15f404) (5); the fade's last 10 frames; 5 the fade back in, the camera rises 18 a unit of the rise (seek_1d to 2),
// each marked box passed bursts (wheel_burst_l6) with the sound 0; at 1.075 Ratchet is put at the box +4, the camera goes
// behind him, the sound 0 (6); 6 the camera settles behind Ratchet; at 2 he is let go (hydro_2eac18(3)), the moby +0x34
// set to state 2 (7); 7 done.
// orig: L6:0x00304098
void blg_class1118_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    // (the table 0x201980: one case a state, 0 .. 6; the states from 7 on do nothing)
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        swc1(m + 0x40, angle_sub(lwc1(m + 0x40), kQuarterPi));
        sb(m + 0x20, 1);
        sw(pv + 0x48, 0xffffffffu);
        break;
    case 2: {
        seek_1d(pv + 0x2c, pv + 0x30, 1.0f, dt_sq(), dt_sq(), dt());
        const float turn = fpu_mul(lwc1(pv + 0x30), kQuarterPi);
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), turn));
        const uint32_t gear_a = table_moby(lw(pv + 0x38));
        swc1(gear_a + 0x40, angle_sub(lwc1(gear_a + 0x40), turn));
        const uint32_t gear_b = table_moby(lw(pv + 0x3c));
        swc1(gear_b + 0x40, angle_sub(lwc1(gear_b + 0x40), turn));
        moby_update_transform(table_moby(lw(pv + 0x38)));
        moby_update_transform(table_moby(lw(pv + 0x3c)));
        if (1.0f <= lwc1(pv + 0x2c)) {
            sb(m + 0x20, 3);
            moby_sound_start(1, 0, m);
            sb(0x0013d3aa, 1);
        }
        break;
    }
    case 3: {
        if (!(vec2_distance(V(HERO_POS), V(m + 0x10)) < 3.7f)) break;
        const bool shown = hud_message_request(4, TXT(0x1783)) != 0;
        if ((lwu(0x0013cae4) & 0x10u) == 0 || !shown) break;
        moby_sound_start(2, 0, m);
        hud_message_release(4);
        sb(m + 0x20, 4);
        sw(pv + 0x40, static_cast<uint32_t>(frame_scale_i(0x1e)));
        break;
    }
    case 4: {
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x40))) != 0) {
            const uint32_t cam = lvd::box(lvd::BOXES, lw(pv + 0x0));
            const uint32_t at = sp, rot = sp + 0x10;
            lq_sq(at, cam + 0x30);
            lq_sq(rot, cam + 0x70);
            sb(m + 0x20, 5);
            sw(pv + 0x2c, 0);
            sw(pv + 0x30, 0);
            screen_fade(frame_scale_i(0xf));
            camera_scripted_set(at, rot, 2, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
            camera_angles_set(lwc1(L6_ADDR(0x00162198)), lwc1(L6_ADDR(0x0016219c)), lwc1(L6_ADDR(0x001621a0)),
                              lwc1(L6_ADDR(0x001621a4)), lwc1(L6_ADDR(0x001621a8)), lwc1(L6_ADDR(0x001621b0)));
            uid5_set_pvar_80(at);
            uid5_set_pvar_90(rot);
            sw(L6_ADDR(0x0015f404), 1);
        }
        const int32_t t = lw(pv + 0x40);
        if (t < frame_scale_i(0xa)) wheel_fade(t);
        break;
    }
    case 5: {
        const int32_t t = lw(pv + 0x40);
        if (t < frame_scale_i(0xa)) {
            sw(pv + 0x40, static_cast<uint32_t>(t + 1));
            wheel_fade(t + 1);
        }
        const float before = lwc1(pv + 0x2c);
        seek_1d(pv + 0x2c, pv + 0x30, 2.0f, dt_sq(), dt_sq(), fpu_mul(dt(), 0.5f));
        // the camera: the box +0's, risen 18 a unit of the rise
        const uint32_t at = sp;
        lq_sq(at, lvd::box(lvd::BOXES, lw(pv + 0x0)) + 0x30);
        swc1(at + 8, fpu_add(lwc1(at + 8), fpu_mul(lwc1(pv + 0x2c), 18.0f)));
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(lvd::box(lvd::BOXES, lw(pv + 0x0)) + 0x70);
        // the marked boxes the rise passed this frame burst
        const float from = fpu_mul(fpu_mul(before, 100.0f), kDegree);
        const float to = fpu_mul(fpu_mul(lwc1(pv + 0x2c), 100.0f), kDegree);
        for (uint32_t k = 0; k < 9; ++k) {
            const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x8 + k * 4));
            const float mark = lwc1(box + 0x78);
            if (from < mark && mark < to) {
                wheel_burst_l6(box + 0x30);
                level_sound_play_l6(0, 0, m);
            }
        }
        if (u2f(0x3f89999au) <= lwc1(pv + 0x2c)) {  // (1.075)
            const uint32_t place = lvd::box(lvd::BOXES, lw(pv + 0x4));
            hero_place(place + 0x30, place + 0x70, 0x72, 0);
            screen_fade(frame_scale_i(0xa));
            sb(m + 0x20, 6);
            hydro_2eac18(3);
            const uint32_t to_at = sp + 0x20, to_rot = sp + 0x10;
            camera_behind(lw(pv + 0x4), to_at, to_rot);
            camera_scripted_set(to_at, to_rot, 2, static_cast<uint32_t>(frame_scale_i(0x12c)), 0);
            swc1(L6_ADDR(0x00167560), 0.3f);
            sw(L6_ADDR(0x00167568), static_cast<uint32_t>(frame_scale_i(0x2d)));
            level_sound_play_l6(0, 0, m);
        }
        break;
    }
    case 6: {
        seek_1d(pv + 0x2c, pv + 0x30, 2.0f, dt_sq(), dt_sq(), fpu_add(dt(), dt()));
        const uint32_t at = sp + 0x10, rot = sp;
        camera_behind(lw(pv + 0x4), at, rot);
        uid5_set_pvar_80(at);
        uid5_set_pvar_90(rot);
        if (2.0f <= lwc1(pv + 0x2c)) {
            sb(m + 0x20, 7);
            sw(pv + 0x44, static_cast<uint32_t>(frame_scale_i(0xe10)));
            const uint32_t place = lvd::box(lvd::BOXES, lw(pv + 0x4));
            hero_place(place + 0x30, place + 0x70, 0, 0);
            hydro_2eac18(3);
            moby_state_hook(table_moby(lw(pv + 0x34)), 2);
        }
        break;
    }
    default:  // (1, and 7 on)
        break;
    }
    ee_stack_free(0x90);
}

}  // namespace rac1
