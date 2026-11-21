// Gaspar's own moby class 1201, the gun turret the hero mounts — hand-ported from Gaspar's own overlay. Header gaspar_mobys.h,
// registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR, the base ELF's are bare
// (the hero 0x13f350: his place +0x80, his turn +0x90, his moby +0x2080, his state +0x2084 / +0x208c, the moby he stands on
// +0x2fc; the pad 0x13c940 (+0x1a0 held, +0x1a4 pressed); the stick 0x141070 / 0x141074, its look-inversion 0x15eddc; the frame
// time 0x15ed6c, its square 0x15ed70, the time step 0x15ed60; the turret the camera follows 0x13e5bc).
//
// The pvars: its turn's quaternion +0x10, its pitch +0x44 and the yaw / pitch rates +0x60 / +0x64, its rest height +0x5c, the
// pause before the hero may mount it again +0x42, the camera's delay +0x52, the barrel that fires next +0x54, its light +0x68
// (blinking: +0x6c the frames since it lit).
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

namespace {
// a stack frame of EE memory for the vectors handed to the engine by address (cleared when it goes)
struct Frame {
    uint32_t sp, n;
    explicit Frame(uint32_t size) : sp(ee_stack_alloc(size)), n(size) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
const float kQuarter = u2f(0x3fc90fdbu);  // (pi / 2)
float dt() { return lwc1(0x0015ed6c); }

// back up toward its rest height (one unit a frame, at most to it)
void rise(uint32_t m, uint32_t pv) {
    const float z = lwc1(m + 0x18), rest = lwc1(pv + 0x5c);
    swc1(m + 0x18, z < rest ? fpu_add(z, dt()) : rest);
}

// the hero in a state that keeps him off it (0x14 or 7 at +0x208c)
bool hero_busy() { return lw(HERO + 0x208c) == 0x14 || lw(HERO + 0x208c) == 7; }

// a shot from the barrel `joint` (1 or 2): the shell at 60 a second along its aim, its sound, the camera shaken, five sparks
// off the muzzle spread from the aim (each a little slower than it), a flash
void fire(uint32_t m, uint32_t pv, uint32_t sp, uint32_t joint) {
    const uint32_t aim = sp + 0x30, muzzle = sp + 0x40, spark = sp + 0x50, rnd = sp + 0x60;
    const float speed = fpu_mul(dt(), 60.0f);
    vec_from_spherical_v(V(aim), speed, angle_add(kQuarter, lwc1(m + 0x48)), lwc1(pv + 0x44));
    moby_chain_point_v(m, joint, V(muzzle));
    shell_launch(m, muzzle, aim, frame_scale_i(0x5a));
    moby_sound_start(0, 0, m);
    const float shake = u2f(0x3cf5c28fu);
    swc1(L9_ADDR(0x00166f60), shake);
    sw(L9_ADDR(0x00166f68), static_cast<uint32_t>(frame_scale_i(10)));
    swc1(L9_ADDR(0x00166f70), shake);
    sw(L9_ADDR(0x00166f78), static_cast<uint32_t>(frame_scale_i(7)));
    sh(pv + 0x54, joint == 1 ? 0u : 1u);
    for (int i = 0; i < 5; ++i) {
        V(rnd) = Vec4{};
        swc1(rnd, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
        lq_sq(spark, rnd);
        const float len = vec3_length(V(aim));
        vec3_set_length(V(spark), V(spark), fpu_mul(len, fpu_add(fpu_mul(lwc1(0x0015ed60), u2f(0xbeb33334u)), 1.0f)));
        vec3_add(V(spark), V(aim), V(spark));
        vec3_set_length(V(spark), V(spark), rand_range_f(fpu_mul(dt(), 3.0f), fpu_mul(dt(), 6.0f)));
        const int32_t lo = frame_scale_i(5);
        const int32_t n = rand_range(lo, frame_scale_i(10));
        spawn_particle_1b(muzzle, spark, 0x5f2f4f6fu, static_cast<uint32_t>(n), 50000.0f);
    }
    vec3_set_length(V(aim), V(aim), fpu_mul(dt(), 0.5f));
    const int32_t lo = frame_scale_i(4);
    const int32_t n = rand_range(lo, frame_scale_i(7));
    spawn_particle_1b(muzzle, aim, 0x2f4f7f7fu, static_cast<uint32_t>(n), 500000.0f);
}

// the barrel's animation `set` (3 / 4): at its end (within a quarter of 1) the shot from `joint`; else, the animation over
// (+0x70 bit 2), the other barrel's
void barrel(uint32_t m, uint32_t pv, uint32_t sp, uint32_t joint, uint32_t other) {
    if (abs_f(fpu_sub(moby_anim_time(m), 1.0f)) <= 0.25f) {
        fire(m, pv, sp, joint);
    } else if ((lbu(m + 0x70) & 2) != 0) {
        moby_anim_set(m, other, 0, 1);
    }
}
}  // namespace

// Class 1201. Set up once (0): its rest height, its light off, the camera's turret. Each frame its seat (joint 3, 0.4 up).
// Mounting (states 4 and 5 only): the hero near the seat (within 1.2) standing on it, the pause over and he not busy, is put in
// the seat state 0x32 (+0xbc 1); next frame he is seated, turned with it, quarter-turned and moved 0.7 off (the hand's buttons
// cleared): with the button 0x10 pressed he gets off at once (+0xbc 2, state 4), else the camera takes the turret's view (+0x52
// 2: state 5). Off it (+0xbc 2) and away by 1.2: the pause 60 frames (+0xbc 0).
// 4, nobody in it: sunk 1 below its rest while the hero is within 4 (or on it), turning toward him (eased), its light blinking
// at its muzzle line; else rising back, its light out (its sound). 5, manned: turned by the stick (pitch within 30 degrees),
// firing on the button 0x20 (the barrels by turns: their animations 3 / 4, idle 5), the camera at its gun after its delay.
// orig: L9:0x00304c80
void gas_class1201_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const Frame f(0x70);
    const uint32_t sp = f.sp, seat = sp;
    sh(m + 0x34, lhu(m + 0x34) & 0xfffb);
    moby_chain_point_v(m, 3, V(seat));
    swc1(seat + 8, fpu_add(lwc1(seat + 8), 0.4f));
    if (!(lbu(m + 0x20) < 4)) {
        bool mount = false;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x42))) != 0 && lbu(m + 0xbc) == 1 && !hero_busy() &&
            lw(HERO + 0x2084) != 0x3d)
            mount = true;
        if (mount) {  // the hero seated
            lq_sq(HERO + 0x80, seat);
            lq_sq(HERO + 0x90, m + 0x40);
            const float turn = angle_add(kQuarter, lwc1(HERO + 0x98));
            swc1(HERO + 0x98, turn);
            const float k = u2f(0xbf333333u);
            swc1(sp + 0x10, fpu_mul(cos_vu(turn), k));
            const float y = fpu_mul(sin_vu(lwc1(HERO + 0x98)), k);
            sw(sp + 0x18, 0);
            swc1(sp + 0x14, y);
            vec3_add(V(HERO + 0x80), V(HERO + 0x80), V(sp + 0x10));
            mem_clear128(0x0013f430, 0x90);
            if ((lwu(PAD + 0x1a4) & 0x10) != 0) {  // off at once
                hydro_2eac18(0);
                hero_set_state(0, 1);
                sb(m + 0xbc, 2);
                sb(m + 0x20, 4);
                moby_anim_set(m, 0, 0, 1);
            } else {
                if (lbu(m + 0x20) != 5) {
                    sh(pv + 0x52, 2);
                    camera_scripted_set(m + 0x10, m + 0x40, 0, 0, 0);
                    if (lbu(m + 0x53) != 5) moby_anim_set(m, 5, 0, frame_scale_i(5));
                }
                sb(m + 0x20, 5);
            }
        } else if (lbu(m + 0xbc) == 2) {  // got off: the pause once he is away (or off it)
            if (1.2f < vec3_distance(V(seat), V(lwu(HERO + 0x2080) + 0x10)) || lwu(HERO + 0x2fc) != m) {
                sb(m + 0xbc, 0);
                sh(pv + 0x42, static_cast<uint32_t>(frame_scale_i(60)));
            }
        } else if (!hero_busy() && lw(HERO + 0x2084) != 0x3d && lwu(HERO + 0x2fc) == m &&
                   vec3_distance(V(seat), V(lwu(HERO + 0x2080) + 0x10)) < 1.2f && lh(pv + 0x42) == 0) {
            sb(m + 0xbc, 1);
            hero_set_state(0x32, 1);
        }
    }
    switch (lbu(m + 0x20)) {
        case 0:
            sub_0024f590(m, 0, pv);
            lq_sq(sp + 0x20, m + 0x10);
            spawn_like_gaspar(m, sp + 0x20);
            sb(m + 0x20, 4);
            swc1(pv + 0x5c, lwc1(m + 0x18));
            sw(pv + 0x44, 0);
            sw(m + 0x48, 0);
            sw(pv + 0x68, 0xffffffffu);
            sh(pv + 0x40, 0);
            sh(pv + 0x42, 0);
            sw(pv + 0x48, 0);
            sw(pv + 0x4c, 0);
            sw(0x0013e5bc, m);
            return;
        case 4: {
            if (!(vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 4.0f) && lwu(0x0013f64c) != m) {  // nobody near: back up, light out
                rise(m, pv);
                if (lwu(0x0013f64c) == m || lw(pv + 0x68) == -1) return;
                moby_sound_start(2, 0, m);
                region_clear(lwu(pv + 0x68));
                sw(pv + 0x68, 0xffffffffu);
                return;
            }
            // the hero near (or on it): sunk a little, turning toward him, its light blinking
            const float z = lwc1(m + 0x18);
            const float x = lwc1(m + 0x10), y = lwc1(m + 0x14);
            swc1(m + 0x18, fpu_add(z, fpu_mul(fpu_sub(fpu_sub(lwc1(pv + 0x5c), 1.0f), z), u2f(0x3de147aeu))));
            const float to = atan2_f(fpu_sub(lwc1(HERO + 0x80), x), fpu_sub(lwc1(HERO + 0x84), y));
            const float off = angle_add(angle_sub(to, lwc1(m + 0x48)), kQuarter);
            const float rate = fpu_mul(angle_add(lwc1(pv + 0x60), fpu_mul(off, u2f(0x3c23d70au))), u2f(0x3f666666u));
            swc1(pv + 0x60, rate);
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), rate));
            const uint32_t at = sp + 0x20;
            mat3_transform(V(at), V(L9_ADDR(0x00161cc0)), M(m + 0xc0));
            vec3_add(V(at), V(at), V(seat));
            const int32_t light = lw(pv + 0x68);
            if (light == -1) {
                sw(pv + 0x6c, 0);
                sw(pv + 0x68, static_cast<uint32_t>(light_add(at, 0.4f, 0.0f, 4.0f, 4.0f, 4.0f)));
                moby_sound_start(1, 0, m);
                return;
            }
            const uint32_t l = L9_ADDR(0x00180440) + (static_cast<uint32_t>(light) << 5);
            swc1(l + 0x10, lwc1(at));
            swc1(l + 0x14, lwc1(at + 4));
            swc1(l + 0x18, lwc1(at + 8));
            const int32_t n = lw(pv + 0x6c) + 1;
            sw(pv + 0x6c, static_cast<uint32_t>(n));
            if (n < 41 && (n & 4) != 0 && (n < 25 || (n & 0xa) != 0xa)) {  // (blinking: off)
                sw(l + 0x1c, 0);
                return;
            }
            swc1(l, 4.0f);
            swc1(l + 0x1c, 0.4f);
            return;
        }
        case 5: {
            sw(0x0013e5bc, m);
            rise(m, pv);
            // the stick (its y inverted with the look option)
            const float sx = lwc1(0x00141070), sy = lw(0x0015eddc) != 0 ? -lwc1(0x00141074) : lwc1(0x00141074);
            Vec4 stick{sx, sy, lwc1(sp + 0x28), lwc1(sp + 0x2c)};
            const float len = vec2_length(stick);
            if (1.0f < len) vec3_div(stick, stick, len);
            const float dt2 = lwc1(0x0015ed70);
            const float yaw = fpu_add(lwc1(pv + 0x60), fpu_mul(fpu_mul(dt2, u2f(0x40490fdbu)), -stick.x));
            swc1(pv + 0x60, yaw);
            const float pitch_rate = fpu_add(lwc1(pv + 0x64), fpu_mul(fpu_mul(dt2, u2f(0x3f490fdbu)), -stick.y));
            swc1(pv + 0x64, pitch_rate);
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), yaw));
            const float pitch = angle_add(lwc1(pv + 0x44), lwc1(pv + 0x64));
            swc1(pv + 0x44, pitch);
            const float lo = u2f(0xbf060a92u), hi = u2f(0x3f060a92u);
            if (pitch < lo || hi < pitch) {
                swc1(pv + 0x44, pitch < lo ? lo : hi);
                sw(pv + 0x64, 0);
            }
            const float k = u2f(0x3f7851ecu);
            const float pr = fpu_mul(lwc1(pv + 0x64), k);
            const float yr = fpu_mul(lwc1(pv + 0x60), k);
            swc1(pv + 0x64, pr);
            swc1(pv + 0x60, yr);
            quat_from_axis_angle(V(pv + 0x10), 1, -lwc1(pv + 0x44));
            if ((lwu(PAD + 0x1a0) & 0x20) == 0) {  // not firing: idle (5)
                if (lbu(m + 0x53) != 5 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 5, 0, 1);
            } else if ((lwu(PAD + 0x1a4) & 0x20) != 0) {  // pressed: the next barrel's animation
                moby_anim_set(m, lh(pv + 0x54) != 0 ? 3u : 4u, 0, 1);
            } else if (lbu(m + 0x52) == 3) {
                barrel(m, pv, sp, 1, 4);
            } else if (lbu(m + 0x52) == 4) {
                barrel(m, pv, sp, 2, 3);
            }
            // the camera at its gun once its delay ran out
            if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x52))) != 0) {
                moby_update_transform(m);
                sh(m + 0x34, lhu(m + 0x34) | 4);
                camera_flag_88();
                moby_chain_point_v(m, 0, V(sp + 0x20));
                const float head = angle_add(kQuarter, lwc1(m + 0x48));
                vec_from_spherical_v(V(sp + 0x30), lwc1(L9_ADDR(0x00161cb0)), head, lwc1(pv + 0x44));
                swc1(sp + 0x38, fpu_add(lwc1(sp + 0x38), lwc1(L9_ADDR(0x00161cb4))));
                vec3_add(V(sp + 0x40), V(sp + 0x20), V(sp + 0x30));
                uid5_set_pvar_80(sp + 0x40);
                sw(sp + 0x50, 0);
                swc1(sp + 0x54, -lwc1(pv + 0x44));
                swc1(sp + 0x58, angle_add(kQuarter, lwc1(m + 0x48)));
                uid5_set_pvar_90(sp + 0x50);
            }
            cam_effect_add(m, 0xff0f0fffu, nullptr, 0xd, -1, 1.0f, 0.0f, 90.0f);
            return;
        }
        default:
            return;
    }
}

}  // namespace rac1
