// The hoverboard races' camera (camera type 8 of Rilgar and Kalebo III), one port of both levels' copies: see
// shared_race_camera.h for the records. Every address is the engine's (OV) or the base ELF's, but the follow's lean scale, the
// level's own word (Rilgar's 0x1621a8, Kalebo III's 0x162198).
#include <game/ovl/shared_race_camera.h>

#include <game/enhance/level_mod_hero.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350u;       // the hero (its matrix +0, flags +0x88d / +0x88e, speed +0x2dc, boost +0x30e)
constexpr uint32_t HERO_POS = 0x0013f3d0u;
constexpr uint32_t HERO_AXIS = 0x0013f5e0u;  // the hero's up axis
constexpr uint32_t HERO_MOBY = 0x001413d0u;

// the camera's entry parameters (the level's camera table *(0x15ef50), the entry +0x84, its +0x1c)
uint32_t entry_params(uint32_t cam) {
    return lwu(lwu(OV(0x0015ef50)) + (static_cast<uint32_t>(lh(cam + 0x84)) << 5) + 0x1c);
}

// `cur` stepped by `step` toward `target`, `target` itself when within the step
float step_toward(float cur, float target, float step) {
    if (abs_f(fpu_sub(cur, target)) < step) return target;
    return cur < target ? fpu_add(cur, step) : fpu_sub(cur, step);
}

// pi/2 - asin(dot / len): the angle of a direction off a plane (len 0 taken as 1e-4)
float angle_off(float dot, float len) {
    if (len == 0.0f) len = u2f(0x38d1b717u);
    return fpu_sub(u2f(0x3fc90fdbu), asin_f(fpu_div(dot, len)));
}

// the level's own lean scale word
uint32_t lean_scale() { return on_level(RILGAR) ? L5_ADDR(0x001621a8) : L16_ADDR(0x00162198); }
}  // namespace

// orig: L5:0x003294c0 L16:0x002f3fc8
void race_cam_params(float dist, float height, float lift, float boost_dist, float boost_height, float boost_lift, float k,
                     float damp) {
    const uint32_t c = lwu(OV(0x00166e00));
    const uint32_t d = lwu(c + 0x70);
    swc1(d + 0x18c, boost_lift);
    swc1(d + 0x184, dist);
    swc1(d + 0x188, height);
    swc1(d + 0x180, lift);
    swc1(d + 0x190, boost_dist);
    swc1(d + 0x194, boost_height);
    const uint32_t d2 = lwu(c + 0x70);  // (read again after the stores)
    swc1(d2 + 0x10, k < 0.0f ? u2f(0x3c23d70au) : k);
    swc1(d2 + 0x14, damp < 0.0f ? u2f(0x3e4ccccdu) : damp);
}

// orig: L5:0x00329358 L16:0x002f3e60
void race_cam_anchor(uint32_t cam) {
    const uint32_t d = lwu(cam + 0x70);
    const uint32_t point = d + 0x80;
    Vec4 up_off{};
    mat4_transform(up_off, Vec4{0.0f, 0.0f, u2f(0x3f333333u), 0.0f}, M(HERO));
    vec3_add(V(point), V(HERO_POS), up_off);
    lq_sq(d + 0x1d0, point);
    // the tracks' floors: Rilgar's near (286.16, 447.82) 62.4 within 48, else 67.2; Kalebo III's 77.5
    const uint32_t planet = level_mod_camera_planet(lwu(0x0015ed84));  // (enhancement: the race camera in a world's section)
    float floor = 0.0f;
    bool floored = false;
    if (planet == 5) {
        const Vec4 centre{u2f(0x438f147bu), u2f(0x43dfe8f6u), u2f(0x427d7ae1u), 1.0f};
        floor = vec2_distance(centre, V(point)) < 48.0f ? u2f(0x4279999au) : u2f(0x42866666u);
        floored = true;
    } else if (planet == 0x10) {
        floor = 77.5f;
        floored = true;
    }
    if (floored && lwc1(point + 0x8) < floor) swc1(point + 0x8, floor);
    vec3_set_length(V(point + 0x20), V(HERO_AXIS), -1.0f);
}

// orig: L5:0x00329540 L16:0x002f4048
void race_cam_basis(uint32_t cam) {
    const uint32_t d = lwu(cam + 0x70);
    race_cam_anchor(cam);
    const uint32_t point = d + 0x80;
    Vec4 back{}, up{}, lift{}, at{}, dir{};
    vec3_set_length(back, V(lwu(HERO_MOBY) + 0xc0), 1.0f);  // (the board's facing, at the distance behind the point)
    vec3_set_length(back, back, lwc1(d + 0xe0));
    vec3_sub(V(cam + 0x30), V(point), back);
    vec3_set_length(up, V(HERO_AXIS), -1.0f);
    vec3_set_length(back, up, lwc1(d + 0xf0));  // (the height)
    vec3_add(V(cam + 0x30), V(cam + 0x30), back);
    vec3_set_length(lift, up, lwc1(point + 0x4c));
    vec3_add(at, lift, V(point));
    vec3_sub(dir, at, V(cam + 0x30));
    vec3_set_length(V(cam), dir, 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), up);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_set_length(V(cam + 0x20), V(cam + 0x20), 1.0f);
    lq_sq(cam + 0x40, cam);
}

// orig: L5:0x003296e0 L16:0x002f41e8
void race_cam_setup(uint32_t cam) {
    const uint32_t par = entry_params(cam);
    sw(par + 0x48, 0);
    const uint32_t d = lwu(cam + 0x70);
    swc1(d + 0x184, lwc1(par + 0x28));
    swc1(d + 0x188, lwc1(par + 0x2c));
    swc1(d + 0x180, lwc1(par + 0x30));
    swc1(d + 0x190, lwc1(par + 0x34));
    swc1(d + 0x194, lwc1(par + 0x38));
    swc1(d + 0x18c, lwc1(par + 0x3c));
    vec_clear(V(d + 0x1b0));
    vec_clear(V(d + 0x1c0));
    {
        const uint32_t point = lwu(cam + 0x70) + 0x80;
        sw(point + 0x40, 5);
        sw(point + 0x44, lwu(HERO_MOBY));
        swc1(point + 0x48, u2f(0x3e567750u));
        swc1(point + 0x58, u2f(0x3ae4c388u));
        swc1(point + 0x5c, u2f(0xbb8efa35u));
        swc1(point + 0x4c, lwc1(d + 0x180));
    }
    {
        const uint32_t b = lwu(cam + 0x70);
        swc1(b + 0xe0, lwc1(d + 0x184));
        swc1(b + 0xe4, 0.5f);
        swc1(b + 0xf0, lwc1(d + 0x188));
    }
    {
        const uint32_t b = lwu(cam + 0x70);
        swc1(b + 0x10, u2f(0x3c23d70au));
        swc1(b + 0x14, u2f(0x3e4ccccdu));
        sw(b + 0x18, 0);
        sh(b + 0x1c, 0);
        vec_clear(V(b));
    }
    {
        const uint32_t lean = lwu(cam + 0x70) + 0x100;
        vec_clear(V(lean));
        vec_clear(V(lean + 0x20));
        sw(lean + 0x40, 0);
        swc1(lean + 0x10, lwc1(par + 0x0));
        swc1(lean + 0x34, lwc1(par + 0x20));
        sw(lean + 0x3c, 0);
        swc1(lean + 0x38, lwc1(par + 0x24));
    }
    {
        const uint32_t b = lwu(cam + 0x70) + 0x30;
        swc1(b + 0x20, u2f(0x3f4ccccdu));
        swc1(b + 0x24, u2f(0x3e99999au));
        swc1(b + 0x40, u2f(0x3faaaaabu));
        sw(b + 0x28, 0);
        sh(b + 0x2c, 0);
        vec_clear(V(b));
    }
    race_cam_params(lwc1(par + 0x28), lwc1(par + 0x2c), lwc1(par + 0x30), lwc1(par + 0x34), lwc1(par + 0x38), lwc1(par + 0x3c),
                    lwc1(par + 0x40), lwc1(par + 0x44));
    race_cam_basis(cam);
    sh(cam + 0x7e, 0);
}

// orig: L5:0x0032a6e0 L16:0x002f51e8
void race_cam_init(uint32_t cam) {
    const uint32_t from = lwu(OV(0x00166e04));
    lq_sq(cam + 0x30, from + 0x30);
    lq_sq(cam, from);
    lq_sq(cam + 0x10, from + 0x10);
    lq_sq(cam + 0x20, from + 0x20);
    mat_to_quat_at(cam + 0x40, cam);
    race_cam_setup(cam);
}

// orig: L5:0x003298d8 L16:0x002f43e0
void race_cam_orbit(uint32_t cam) {
    const uint32_t d = lwu(cam + 0x70);
    const uint32_t point = d + 0x80, springs = d + 0x150;
    Vec4 up{};
    vec3_set_length(up, V(HERO_AXIS), -1.0f);
    if (lh(d + 0x1c) == 0 && lbu(HERO + 0x88e) != 0) {
        // the orbit starts: its angles about the way up and the side, and its distance, measured from where the camera is
        sh(d + 0x1c, 1);
        Vec4 side{}, ahead{}, rel{}, along{}, flat{}, unit{}, turned{};
        vec3_cross_ba(side, V(lwu(HERO_MOBY) + 0xe0), up);
        vec3_set_length(side, side, 1.0f);
        vec3_cross_ba(ahead, up, side);
        vec3_set_length(ahead, ahead, 1.0f);
        vec3_sub(rel, V(cam + 0x30), V(point));
        vec3_set_length(along, up, vec3_dot(rel, up));
        vec3_sub(flat, rel, along);
        float yaw = angle_off(vec3_dot(ahead, flat), vec3_length(flat));
        vec3_set_length(unit, flat, 1.0f);
        if (vec3_dot(side, unit) < 0.0f) yaw = -yaw;
        swc1(cam + 0x50, yaw);
        vec_rotate_axis(turned, ahead, up, yaw);
        const float pitch = angle_off(vec3_dot(turned, rel), vec3_length(rel));
        vec3_set_length(unit, rel, 1.0f);
        swc1(cam + 0x54, vec3_dot(up, unit) < 0.0f ? pitch : -pitch);
        swc1(cam + 0x58, vec3_length(rel));
        V(d + 0xb0) = ahead;
        // the springs' rests (pi, -1.536, 5), speeds 0, tuning 0.005 / 0.2
        swc1(springs + 0x0, u2f(0x40490fdbu));
        swc1(springs + 0x4, u2f(0xbfc49809u));
        swc1(springs + 0x8, 5.0f);
        swc1(springs + 0x14, 0.0f);
        swc1(springs + 0x20, u2f(0x3ba3d70au));
        swc1(springs + 0x2c, u2f(0x3e4ccccdu));
        swc1(springs + 0xc, 0.0f);
        swc1(springs + 0x10, 0.0f);
        swc1(springs + 0x18, u2f(0x3ba3d70au));
        swc1(springs + 0x24, u2f(0x3e4ccccdu));
        swc1(springs + 0x1c, u2f(0x3ba3d70au));
        swc1(springs + 0x28, u2f(0x3e4ccccdu));
    }
    if (lh(d + 0x1c) != 1) return;
    // the position from the angles and the distance, then the three sprung toward their rests
    Vec4 back{}, turned{}, axis{}, off{};
    vec3_set_length(back, V(point + 0x30), lwc1(cam + 0x58));
    vec_rotate_axis(turned, back, up, lwc1(cam + 0x50));
    vec3_cross_ba(axis, turned, up);
    vec3_set_length(axis, axis, 1.0f);
    vec_rotate_axis(off, turned, axis, lwc1(cam + 0x54));
    vec3_add(V(cam + 0x30), V(point), off);
    swc1(cam + 0x50, camera_angle_spring(lwc1(cam + 0x50), lwc1(springs + 0x0), lwc1(springs + 0x18), lwc1(springs + 0x24), 0.0f, springs + 0xc));
    swc1(cam + 0x54, camera_angle_spring(lwc1(cam + 0x54), lwc1(springs + 0x4), lwc1(springs + 0x1c), lwc1(springs + 0x28), 0.0f, springs + 0x10));
    swc1(cam + 0x58, camera_spring(lwc1(cam + 0x58), lwc1(springs + 0x8), lwc1(springs + 0x20), lwc1(springs + 0x2c), 0.0f, springs + 0x14));
    if (lbu(HERO + 0x88e) == 0) {  // (the orbit over: the position springs' speeds turned back)
        sh(d + 0x1c, 0);
        vec3_scale(V(d), V(d), -1.0f);
    }
}

// orig: L5:0x00329ce8 L16:0x002f47f0
void race_cam_follow(uint32_t cam) {
    const uint32_t d = lwu(cam + 0x70);
    const float rest_lift = lwc1(d + 0x180);
    const uint32_t lean = d + 0x100;
    const uint32_t par = entry_params(cam);
    const uint32_t point = d + 0x80, dist = d + 0xe0, lift = point + 0x4c;
    Vec4 up{};
    vec3_set_length(up, V(HERO_AXIS), -1.0f);
    Vec4 anchor = V(point);
    bool followed = false;
    if (lh(d + 0x1c) == 0) {
        // the place behind the point: the camera's offset off the way up kept at the distance, the height along it, the drift
        Vec4 rel{}, along{}, flat{}, place{}, ahead{};
        vec3_sub(rel, V(cam + 0x30), anchor);
        vec3_set_length(along, V(d + 0xa0), vec3_dot(rel, V(d + 0xa0)));
        vec3_sub(flat, rel, along);
        vec3_set_length(flat, flat, lwc1(dist));
        vec3_add(place, anchor, flat);
        vec3_set_length(along, V(d + 0xa0), lwc1(dist + 0x10));
        vec3_add(ahead, place, along);
        vec3_add(ahead, ahead, V(d + 0x1b0));
        swc1(cam + 0x30, camera_spring(lwc1(cam + 0x30), ahead.x, lwc1(d + 0x10), lwc1(d + 0x14), lwc1(d + 0x18), d + 0x0));
        swc1(cam + 0x34, camera_spring(lwc1(cam + 0x34), ahead.y, lwc1(d + 0x10), lwc1(d + 0x14), lwc1(d + 0x18), d + 0x4));
        swc1(cam + 0x38, camera_spring(lwc1(cam + 0x38), ahead.z, lwc1(d + 0x10), lwc1(d + 0x14), lwc1(d + 0x18), d + 0x8));
        // the way from where the camera was (+0x64) in six steps: the first point the ground holds lifts the height
        Vec4 probe{lwc1(cam + 0x64), lwc1(cam + 0x68), lwc1(cam + 0x6c), 0.0f}, step{};
        vec3_sub(step, V(cam + 0x30), probe);
        vec3_scale(step, step, u2f(0x3e2aaaabu));
        int32_t i = 0;
        for (; i < 6; ++i) {
            if (sphere_collide_v(probe, 0.5f, 0x12, 0) != 0) {
                const float rise = fpu_sub(lwc1(OV(0x00173e78)), anchor.z);
                swc1(dist + 0x10, camera_spring(lwc1(dist + 0x10), rise, u2f(0x3d4ccccdu), u2f(0x3e4ccccdu), 0.0f, par + 0x48));
                break;
            }
            vec3_add(probe, probe, step);
        }
        if (i == 6) {  // (nothing in the way: the drift and the height back to theirs)
            for (uint32_t k = 0; k < 3; ++k)
                swc1(d + 0x1b0 + k * 4, camera_spring(lwc1(d + 0x1b0 + k * 4), 0.0f, u2f(0x3ca3d70au), u2f(0x3e4ccccdu), u2f(0x3d4ccccdu),
                                                      d + 0x1c0 + k * 4));
            swc1(dist + 0x10, camera_spring(lwc1(dist + 0x10), lwc1(d + 0x188), u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f, par + 0x48));
        }
        // the distances toward their targets: the boost's while the board goes fast (+0x30e, speed above 2)
        if (lh(HERO + 0x30e) != 0 && 2.0f < lwc1(HERO + 0x2dc)) {
            swc1(dist, step_toward(lwc1(dist), lwc1(d + 0x190), u2f(0x3c75c28fu)));
            if (i == 6) swc1(dist + 0x10, step_toward(lwc1(dist + 0x10), lwc1(d + 0x194), u2f(0x3d4ccccdu)));
            swc1(lift, step_toward(lwc1(lift), lwc1(d + 0x18c), u2f(0x3ca3d70au)));
        } else {
            swc1(dist, step_toward(lwc1(dist), lwc1(d + 0x184), u2f(0x3cf5c28fu)));
            swc1(dist + 0x10, step_toward(lwc1(dist + 0x10), lwc1(d + 0x188), u2f(0x3dcccccdu)));
            swc1(lift, step_toward(lwc1(lift), rest_lift, u2f(0x3d23d70au)));
        }
        followed = true;
    } else {
        // the orbit: the board's facing kept, the distances eased back
        lq_sq(d + 0x1a0, lwu(HERO_MOBY) + 0xe0);
        swc1(dist, step_toward(lwc1(dist), lwc1(d + 0x184), u2f(0x3c75c28fu)));
        swc1(dist + 0x10, step_toward(lwc1(dist + 0x10), lwc1(d + 0x188), u2f(0x3d4ccccdu)));
        swc1(lift, step_toward(lwc1(lift), rest_lift, u2f(0x3ca3d70au)));
    }

    race_cam_orbit(cam);
    lq_sq(point, d + 0x1d0);
    anchor = V(point);
    // the basis toward the point lifted; then the lean (sideways, by the board's tilt while it follows and faces the way)
    Vec4 lifted{}, target{}, dir{};
    vec3_set_length(lifted, V(d + 0xa0), lwc1(lift));
    vec3_add(target, anchor, lifted);
    vec3_sub(dir, target, V(cam + 0x30));
    vec3_set_length(dir, dir, 1.0f);
    const float facing = vec3_dot(V(cam), dir);
    vec3_cross_ba(V(cam + 0x10), dir, up);
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), dir);
    float tilt = 0.0f;
    if (followed && !(abs_f(facing) < 0.5f) && lbu(HERO + 0x88d) == 0) {
        Vec4 board{}, part{};
        vec3_set_length(board, V(lwu(HERO_MOBY) + 0xc0), 1.0f);
        vec3_set_length(part, V(cam + 0x20), vec3_dot(board, V(cam + 0x20)));
        vec3_sub(board, board, part);
        tilt = fpu_mul(vec3_dot(board, V(cam + 0x10)), lwc1(lean_scale()));
    }
    swc1(lean + 0x40, camera_spring(lwc1(lean + 0x40), tilt, lwc1(lean + 0x34), lwc1(lean + 0x38), lwc1(lean + 0x3c), lean + 0x20));
    if (u2f(0x3727c5acu) < abs_f(lwc1(lean + 0x40))) {
        Vec4 shift{};
        vec3_set_length(shift, V(cam + 0x10), lwc1(lean + 0x40));
        vec3_add(target, target, shift);
    }
    vec3_sub(dir, target, V(cam + 0x30));
    vec3_set_length(V(cam), dir, 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), up);
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
}

// orig: L5:0x0032a758 L16:0x002f5260
void race_cam_update(uint32_t cam) {
    race_cam_anchor(cam);
    race_cam_follow(cam);
}

}  // namespace rac1
