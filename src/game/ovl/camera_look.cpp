// Ported overlay code: camera type 1 (id 4), the first-person look. It takes over while the
// hero is in state 4 (or 0x51) for a few frames, from the follow camera or types 0xa / 0xe / 2,
// outside the volumes that forbid it: the view sits at the hero's head (higher on the ladder
// and the swingshot, by the hero's mode +0x20a4) and the sticks (or the pad's buttons) turn it
// with springs: yaw about the hero's up axis, pitch (inverted unless the option 0x15eddc) held
// within 84 degrees, leaning forward when looking down past 64 degrees. On the way out the
// follow camera is turned back toward the hero's facing.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350, PAD = 0x0013c940;
constexpr OvConst<0x00166e00u> ACTIVE{};
constexpr OvConst<0x00166e04u> PREV{};
constexpr OvConst<0x00166c80u> VIEW{}; constexpr OvConst<0x00166e40u> AXIS{};
// orig: (helper) the hero's up row *(0x1413d0) + 0xe0, unit length
Vec4 hero_up() {
    Vec4 up;
    vec3_set_length(up, V(lwu(0x001413d0) + 0xe0), 1.0f);
    return up;
}
// orig: (helper) repeated in 0x002e9cc0, 0x002e9dd0 — the head height by the hero's mode +0x20a4
float head_height(float walk) {
    const uint32_t mode = lbu(P + 0x20a4);
    if (mode == 2) return 9.5f;
    if (mode == 1) return walk;
    return 1.6f;
}
// orig: (helper) repeated in 0x002ea488, 0x002ea810 — whether `row` has turned 80 degrees or
// more from the hero's facing, both projected off the axis 0x166e40 (-1: a projection is 0).
int32_t turned_from_hero(uint32_t row) {
    const uint32_t h = lwu(0x001413d0) + 0xc0;
    Vec4 t, a, b;
    vec3_scale(t, V(AXIS), vec3_dot(V(AXIS), V(row)));
    vec3_sub(a, V(row), t);
    vec3_scale(t, V(AXIS), vec3_dot(V(AXIS), V(h)));
    vec3_sub(b, V(h), t);
    float n = vec3_length(b);
    if (n == 0.0f) return -1;
    n = n * vec3_length(a);
    if (n == 0.0f) return -1;
    const float ang = u2f(0x3fc90fdbu) - asin_f(fpu_div(vec3_dot(b, a), n));
    return u2f(0x3fb2b8c2u) <= ang ? 1 : 0;
}
// orig: (helper) repeated in 0x002ea488 — the follow camera turned toward the hero's facing
void turn_to_facing(float speed) {
    const uint32_t dir = ee_stack_alloc(0x10);
    swc1(dir, cos_vu(lwc1(P + 0x98)));
    swc1(dir + 4, sin_vu(lwc1(P + 0x98)));
    sw(dir + 8, 0);
    cam0_turn_dir(dir, speed, lwc1(dir + 8));
    ee_stack_free(0x10);
}
// orig: (helper) repeated in 0x002ea488 — the camera types the look takes over from
bool look_from(int32_t type) { return type == 0 || type == 10 || type == 0xe || type == 2; }
// orig: (helper) repeated in 0x002e9f38 — the stick (+a / +b, or the pad's buttons by bit) as -1 .. 1
float stick(uint32_t a, uint32_t b, uint32_t plus, uint32_t minus) {
    float f = -lwc1(PAD + a);
    if (f == 0.0f) f = -lwc1(PAD + b);
    if (f == 0.0f) f = int_to_float(static_cast<int32_t>((lw(PAD + 0x1a0) >> plus) & 1));
    if (f == 0.0f) f = -int_to_float(static_cast<int32_t>((lw(PAD + 0x1a0) >> minus) & 1));
    return f;
}
}  // namespace

// The view at the hero's head (the hero's position, the up axis times the head height, plus
// the offset +0x140).
// orig: 0x002e9cc0
void cam_look_follow(uint32_t cam) {
    const Vec4 up = hero_up();
    const float k = head_height(lbu(0x0015edb3) != 0 ? u2f(0x3f99999au) : u2f(0x3f666666u));
    Vec4 off;
    vec3_scale(off, up, k);
    vec3_add(V(cam + 0x30), V(P + 0x80), off);
    vec3_add(V(cam + 0x30), V(P + 0x140), V(cam + 0x30));
}

// The view put at the hero's head, upright, facing across the previous camera's right row.
// orig: 0x002e9dd0
void cam_look_place(uint32_t cam) {
    const Vec4 up = hero_up();
    Vec4 off;
    vec3_scale(off, up, head_height(u2f(0x3f666666u)));
    vec3_add(V(cam + 0x30), V(P + 0x80), off);
    V(cam + 0x20) = up;
    vec3_cross_ba(V(cam + 0x10), V(lwu(PREV)), up);
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam), up, V(cam + 0x10));
    V(cam + 0x40) = V(cam);
}

// The look's springs cleared (+0x70: yaw, pitch, lean and their speeds) and the view placed.
// orig: 0x002e9ef0
void cam_look_reset(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    for (uint32_t o = 0; o < 0x1c; o += 4) sw(w + o, 0);
    cam_look_place(cam);
    sh(cam + 0x7e, 0);
}

// orig: 0x002e9f38
void cam_look_update(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    cam_look_follow(cam);
    const Vec4 up = hero_up();
    if (lh(VIEW + 0x270) == 0 && lbu(P + 0x20a5) == 0) sb(P + 0x20a5, 1);
    const float lo = lwc1(OV(0x00166c00u - 0x4e40)), hi = lwc1(OV(0x00166c00u - 0x4e3c));
    // the yaw: about the hero's up axis
    float s = stick(0x108, 0x100, 15, 13);
    float gain = abs_f(lwc1(w));
    gain = lo + (hi - lo) * gain;
    const float yaw = camera_spring(lwc1(w), s, lwc1(OV(0x00166c00u - 0x4e38)), lwc1(OV(0x00166c00u - 0x4e34)), lwc1(OV(0x00166c00u - 0x4e30)), w + 4);
    swc1(w, yaw);
    if (yaw != 0.0f) vec_rotate_axis(V(cam), V(cam), up, gain * yaw);
    if (lwc1(P + 0x14c) != 0.0f && lh(P + 0x308) == 0) vec_rotate_axis(V(cam), V(cam), up, lwc1(P + 0x14c));
    // the pitch
    s = stick(0x10c, 0x104, 12, 14);
    if (lh(VIEW + 0x270) != 0 && lw(P + 0x2090) == 2) s = 0.0f;
    if (lw(0x0015eddc) == 0) s = -s;
    gain = abs_f(lwc1(w + 8));
    gain = lo + (hi - lo) * gain;
    const float pitch = camera_spring(lwc1(w + 8), s, lwc1(OV(0x00166c00u - 0x4e38)), lwc1(OV(0x00166c00u - 0x4e34)), lwc1(OV(0x00166c00u - 0x4e30)), w + 0xc);
    swc1(w + 8, pitch);
    if (pitch != 0.0f) {
        Vec4 side;
        vec3_cross_ba(side, V(cam), up);
        const float limit = u2f(0x3f8efa35u);
        float turn = gain * lwc1(w + 8);
        float to = abs_f(angle_add(lwc1(w + 0x10), turn));
        if (limit < to && abs_f(lwc1(w + 0x10)) < to) turn = turn + (0.0f - turn) * fpu_div(angle_sub(to, limit), u2f(0x3eb2b8c4u));
        // the lean forward when looking down past 64 degrees
        const bool lean = u2f(0x3f8ec104u) <= to && 0.0f < lwc1(w + 0x10);
        swc1(w + 0x14, camera_spring(lwc1(w + 0x14), lean ? 0.5f : 0.0f, u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f, w + 0x18));
        Vec4 t = {lwc1(cam), lwc1(cam + 4), 0.0f, 0.0f};
        vec3_set_length(t, t, lwc1(w + 0x14));
        vec3_add(V(cam + 0x30), V(cam + 0x30), t);
        if (sphere_collide_v(V(cam + 0x30), u2f(0x3e99999au), 0x12, lwu(0x001413d0)) != 0)
            swc1(w + 0x14, camera_spring(lwc1(w + 0x14), 0.0f, u2f(0x3ba3d70au), u2f(0x3e4ccccdu), 0.0f, w + 0x18));
        const float next = angle_add(lwc1(w + 0x10), turn);
        if (u2f(0x3fbba866u) < abs_f(next)) turn = angle_sub(0.0f < next ? u2f(0x3fbba866u) : u2f(0xbfbba866u), lwc1(w + 0x10));
        vec_rotate_axis(V(cam), V(cam), side, turn);
        swc1(w + 0x10, angle_add(lwc1(w + 0x10), turn));
    }
    vec3_set_length(V(cam), V(cam), 1.0f);
    vec3_cross_ba(V(cam + 0x10), V(cam), up);
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
}

// Takes over (1) once the hero has been in state 4 / 0x51 for 7 frames (the follow camera set
// to priority 5 first), unless a volume at the hero forbids it (flag 1). Out of those states
// it turns the follow camera back toward the hero's facing for 30 frames, or switches to the
// first camera at once when the view was turned 80 degrees or more away.
// orig: 0x002ea488
int32_t cam_look_accept(uint32_t cam, uint32_t other) {
    const uint32_t d = lvd::camera_data(lh(cam + 0x84));
    const int32_t st = lw(P + 0x2284);
    if (st == lh(cam + 0x86) || st == 0x51) {
        const uint32_t act = lwu(ACTIVE);
        if (!look_from(lh(other + 0x86)) || !look_from(lh(act + 0x86))) {
            sh(d + 0x20, 0);
            sh(d + 0x22, 0);
        } else {
            const Vec4 at = V(P + 0x80);
            const uint32_t vol = cam_volume_find(at, 0.25f);
            if (vol != 0 && (lw(vol + 0x18) & 1) != 0) return 0;
            const uint32_t n = lhu(d + 0x20) + 1;
            sw(d + 0x24, 0);
            sh(d + 0x22, 1);
            sh(d + 0x20, n);
            if (!(static_cast<int16_t>(n) < frame_scale_i(7))) {
                if (lh(act + 0x86) == 0) sb(cam + 0x7c, 5);
                return 1;
            }
            turn_to_facing(u2f(0x3d8efa35u));
        }
    } else {
        const int32_t was = lh(d + 0x22);
        sh(d + 0x20, 0);
        if (was != 0) {
            sh(d + 0x22, 0);
            if (turned_from_hero(lwu(ACTIVE)) == 1) {
                camera_select_first();
                return 0;
            }
            sw(d + 0x24, static_cast<uint32_t>(frame_scale_i(0x1e)));
        }
    }
    if (lw(d + 0x24) != 0) {
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(d + 0x24)));
        turn_to_facing(u2f(0x3e0efa35u));
    }
    return 0;
}

// orig: 0x002ea7a0
void cam_look_init(uint32_t cam) {
    sh(VIEW + 0x270, lh(VIEW + 0x270) != 0 ? 2 : 1);
    sb(VIEW + 0x273, 0);
    swc1(VIEW + 0x288, u2f(0x3d4ccccdu));
    swc1(VIEW + 0x294, u2f(0x3d4ccccdu));
    cam_look_reset(cam);
    if (lwu(VIEW + 0x254) != 0) moby_sound_start(0, 0, lwu(VIEW + 0x254));
}

// Leaving the look (unless the hero is still in state 4 / 0x51): blend back (+0x7e = 2), or
// cut (4) when the view was turned 80 degrees or more from the hero's facing.
// orig: 0x002ea810
void cam_look_exit(uint32_t cam) {
    const int32_t st = lw(P + 0x2284);
    if (st == 0x51 || st == 4) return;
    if (lh(VIEW + 0x270) != 0 && turned_from_hero(cam) == 1) {
        sh(VIEW + 0x270, 0);
        sh(cam + 0x7e, 4);
        return;
    }
    sb(VIEW + 0x273, 0);
    sh(cam + 0x7e, 2);
    swc1(VIEW + 0x288, u2f(0x3c9374bcu));
    swc1(VIEW + 0x294, u2f(0x3c9374bcu));
}

}  // namespace rac1
