// Ported overlay code: the level's camera modifiers, camera types 5 (id 0x11) and 6 (id 0x17).
// They never take over themselves (their accept function returns 0): while the hero is in their
// volume (a box, cylinder, sphere or polygon) and they hold the follow camera's claim (its work
// +0x220), they steer the follow camera (type 0): its direction, distance, height and look
// height, its rates and modes, by their level data (the entry's +0x1c).
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/kalebo_mobys.h>
#include <game/ovl/variants_l16.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x00166e00u> ACTIVE{};
constexpr uint32_t HERO = 0x0013f3d0, AXIS = 0x0013f5e0;
const float DEG = u2f(0x3c8efa35u);  // pi / 180
// orig: (helper) the level camera entry (0x20 each at *(0x15ef50)) of a camera
uint32_t entry(uint32_t cam) { return lvd::camera(lh(cam + 0x84)); }
// orig: (helper) the follow camera's work area (the active camera's +0x70)
uint32_t work() { return lwu(lwu(ACTIVE) + 0x70); }
}  // namespace

// The follow camera's claim given back by `cam`: 1 when it held it, 0 when not, -1 when the
// active camera is not the follow camera.
// orig: 0x002e8388
int32_t cam_mod_release(uint32_t cam) {
    const uint32_t c = lwu(ACTIVE);
    if (lh(c + 0x86) != 0) return -1;
    const uint32_t w = lwu(c + 0x70);
    if (lwu(w + 0x220) != cam) return 0;
    sw(w + 0x220, 0);
    return 1;
}

// The follow camera's claim taken by `cam`: when free, its own, or held by a modifier of lower
// priority (the data's +0x1c); 1 when `cam` holds it, else 0 (-1: not the follow camera).
// orig: 0x002e83c0
int32_t cam_mod_claim(uint32_t cam) {
    const uint32_t c = lwu(ACTIVE);
    if (lh(c + 0x86) != 0) return -1;
    const uint32_t w = lwu(c + 0x70);
    const uint32_t cur = lwu(w + 0x220);
    if (cur == 0) {
        sw(w + 0x220, cam);
        return 1;
    }
    if (cur == cam) return 1;
    const uint32_t a = lwu(entry(cur) + 0x1c), b = lwu(entry(cam) + 0x1c);
    if (lbu(a + 0x1c) < lbu(b + 0x1c)) {
        sw(w + 0x220, cam);
        return 1;
    }
    return 0;
}

// The follow camera's work +0x22c (with the follow camera active).
// orig: 0x002e8490
void cam0_set_22c(float v) {
    if (lh(lwu(ACTIVE) + 0x86) == 0) swc1(work() + 0x22c, v);
}

// The follow camera's work +0x1cc (with the follow camera active).
// orig: 0x002e8540
void cam0_set_1cc(float v) {
    if (lh(lwu(ACTIVE) + 0x86) == 0) swc1(work() + 0x1cc, v);
}

// The follow camera's work +0x124 / +0x128, each when not 0.
// orig: 0x002e85d8
void cam0_set_124(float a, float b) {
    if (on_camera_level(KALEBO)) { kal_cam0_set_124_u(0, 0, 0, 0, 0, 0, 0, 0, a, b, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); return; }  // (level 16's own: L16:0x002ee790, the work's +0x11c / +0x120)
    if (lh(lwu(ACTIVE) + 0x86) != 0) return;
    const uint32_t w = work() + 0x40;
    if (a != 0.0f) swc1(w + 0xe4, a);
    if (b != 0.0f) swc1(w + 0xe8, b);
}

// The follow camera's flags +0x1b8 | 3.
// orig: 0x002e8680
void cam0_set_flags3() {
    if (lh(lwu(ACTIVE) + 0x86) != 0) return;
    const uint32_t w = work() + 0x1a8;
    sw(w + 0x10, lwu(w + 0x10) | 3u);
}

// Is `pos` inside the level cylinder `idx` (0x80 each at *(0x1600fc): its centre +0x30, the
// matrix +0x40 to unit space): within radius 1 across and -1 .. 1 along. Not for idx < 0.
// orig: 0x0025fa38
int32_t point_in_cylinder(uint32_t pos, int32_t idx) {
    if (idx < 0) return 0;
    const uint32_t c = lvd::box(lvd::CYLINDERS, idx);
    Vec4 d, t;
    vec3_sub(d, V(pos), V(c + 0x30));
    d.w = 0.0f;
    mat3_transform(t, d, *static_cast<const Mat4*>(ee_ram_ptr(c + 0x40)));
    if (!(vec2_length(t) < 1.0f)) return 0;
    if (!(-1.0f <= t.z)) return 0;
    return t.z <= 1.0f ? 1 : 0;
}

// Is `pos` inside the level sphere `idx` (0x80 each at *(0x1600f4), as the cylinders). Not
// for idx < 0.
// orig: 0x0025faf0
int32_t point_in_sphere(uint32_t pos, int32_t idx) {
    if (idx < 0) return 0;
    const uint32_t c = lvd::box(lvd::SPHERES, idx);
    Vec4 d, t;
    vec3_sub(d, V(pos), V(c + 0x30));
    d.w = 0.0f;
    mat3_transform(t, d, *static_cast<const Mat4*>(ee_ram_ptr(c + 0x40)));
    return vec3_length(t) < 1.0f ? 1 : 0;
}

// Is the hero in the type-5 modifier's volume (its box +0xc, second box +0x4c, cylinder +0x10,
// sphere +8, polygon +0x14 of the set 0x1b04b0); kinds 2 (hero state 0x1a) and 4 (hero mode 3)
// count while they run (+0x2e). Never with another camera than the follow camera.
// orig: 0x002ec7e8
int32_t cam_mod_inside(uint32_t cam) {
    const uint32_t d = lwu(entry(cam) + 0x1c);
    if (lh(lwu(ACTIVE) + 0x86) != 0) return 0;
    bool on = false;
    const int32_t kind = lh(d + 0x2c);
    if (kind == 2 && lw(0x001413dc) == 0x1a && 0 < lh(d + 0x2e)) on = true;
    else if (kind == 4 && lw(0x001415d4) == 3) on = 0 < lh(d + 0x2e);
    if (lw(d + 0xc) >= 0 && (on || point_in_box(HERO, lw(d + 0xc)) != 0)) return 1;
    if (lw(d + 0x4c) >= 0 && (on || point_in_box(HERO, lw(d + 0x4c)) != 0)) return 1;
    if (lw(d + 0x10) >= 0 && (on || point_in_cylinder(HERO, lw(d + 0x10)) != 0)) return 1;
    if (lw(d + 8) >= 0 && (on || point_in_sphere(HERO, lw(d + 8)) != 0)) return 1;
    const int32_t poly = lw(d + 0x14);
    if (poly < 0) return 0;
    if (on) return 1;
    const uint32_t p = lvd::path(poly);
    return point_in_polygon(HERO, p + 0x10, lw(p)) != 0 ? 1 : 0;
}

// The type-5 modifier while the hero is inside (and it holds the claim, faces within 80
// degrees with +0x36, not pushed out by the pad with +0x48 on kind 5): counts its frames +0x2e
// (by kind: capped at 200, restarted by the stick, 1 frame), the first time takes the follow
// camera to its spot (kind 5), then turns the camera to its direction (+0x18 of the entry, by
// +0 degrees a frame), sets its distance +0x24, height +0x28, look height +0x30, +0x50, its
// flags (+0x54 / +0x56), mode (+0x34) and rates (+0x38 / +0x3c, eased in over 120 frames from
// those it found); kinds 10 / 11 set the follow camera's state +0x230. Outside, it lets go.
// orig: 0x002ec988
void cam_mod_update(uint32_t cam) {
    const uint32_t d = lwu(entry(cam) + 0x1c);
    const int32_t k0 = lh(d + 0x2c);
    if (k0 == 8 && lw(work() + 0x230) == 0) return;
    if (k0 == 6 && lw(work() + 0x230) != 0) return;
    auto leave = [&] {
        if (lh(d + 0x2e) != 0) sh(d + 0x4a, 1);
        sh(d + 0x2e, 0);
        cam_mod_release(cam);
    };
    if (cam_mod_inside(cam) == 0) {
        leave();
        return;
    }
    if (cam_mod_claim(cam) <= 0) {
        if (lh(d + 0x2e) != 0) sh(d + 0x4a, 1);
        sh(d + 0x2e, 0);
        return;
    }
    if (lh(d + 0x36) != 0 && u2f(0x3fb2b8c2u) <= angle_dist(lwc1(entry(cam) + 0x18), lwc1(OV(0x00166dd8)))) {
        leave();
        return;
    }
    if (lh(d + 0x48) != 0 && lh(d + 0x2c) == 5 && (lw(0x0013cae0) & 5) != 0) {
        leave();
        return;
    }
    if (lh(d + 0x2e) == 0) {
        const uint32_t w = lwu(lwu(OV(0x00166e00)) + 0x70);
        swc1(d + 0x40, lwc1(w + 0x11c));
        swc1(d + 0x44, lwc1(w + 0x120));
    }
    if (lw(d + 0x2c) == 5) {  // (the word: kind 5 with no frames counted yet) the camera to its spot
        sw(d, 0);
        const uint32_t c = lwu(ACTIVE), w = lwu(c + 0x70);
        const uint32_t a = w + 0x130, b = w + 0x40, s = w + 0x20;
        vec3_set_length(V(a), V(a), lwc1(d + 0x24));
        swc1(a + 0x2c, lwc1(d + 0x24));
        swc1(a + 0x30, lwc1(d + 0x28));
        swc1(b + 0xb0, lwc1(d + 0x30));
        V(w + 0x140) = V(a);
        Vec4 t;
        vec3_scale(t, V(AXIS), -lwc1(a + 0x30));
        vec3_add(V(w + 0x90), V(HERO), t);
        vec3_add(V(c + 0x30), V(w + 0x90), V(a));
        vec3_scale(t, V(AXIS), -lwc1(b + 0xb0));
        vec3_add(V(w + 0x80), V(HERO), t);
        V(w + 0xd0) = V(w + 0x80);
        V(b) = V(HERO);
        V(w + 0xa0) = V(b);
        V(w) = V(c + 0x30);
        V(w + 0x1f0) = V(c + 0x30);
        swc1(w + 0x200, lwc1(a + 0x2c) - vec3_length(V(a)));
        sw(s + 0x14, 0);
        swc1(s + 4, lwc1(b + 0xb0));
        const float z = lwc1(s + 0x14);
        swc1(s + 0xc, z);
        swc1(s + 8, lwc1(a + 0x30));
        swc1(s + 0x18, z);
        swc1(s + 0x10, z);
    }
    bool done = false;
    float span = int_to_float(frame_scale_i(0xc8));
    sh(d + 0x2e, lhu(d + 0x2e) + 1u);
    if (lh(d + 0x2c) == 10) {
        cam0_set_flags3();
        sw(work() + 0x230, 0x14d);
    }
    if (lh(d + 0x2c) == 0xb) {
        cam0_set_flags3();
        sw(work() + 0x230, 2);
    }
    const int32_t kind = lh(d + 0x2c);
    if (kind == 2 || kind == 4) {
        const uint32_t w = work();
        sw(w + 0x1b8, lwu(w + 0x1b8) | 3u);
        if (frame_scale_i(0xc8) < lh(d + 0x2e)) {
            done = true;
            sh(d + 0x2e, static_cast<uint32_t>(frame_scale_i(0xc8)));
        }
        cam0_set_flag226();
    } else if (kind == 1 || kind == 7 || kind == 10 || kind == 9) {
        if (lwc1(0x0013c940 + 0x100) != 0.0f || lwc1(0x0013c940 + 0x104) != 0.0f) sh(d + 0x2e, static_cast<uint32_t>(frame_scale_i(0xc8)));
        if (!(lh(d + 0x2e) < frame_scale_i(0xc8))) {
            done = true;
            if (0.0f < lwc1(OV(0x00166eb4))) sh(d + 0x2e, static_cast<uint32_t>(frame_scale_i(0xc8)));
        }
        if (!(lh(d + 0x2e) < frame_scale_i(0x190))) {
            done = false;
            sh(d + 0x2e, 1);
        }
    } else if (kind == 3) {
        span = int_to_float(frame_scale_i(1));
        if (frame_scale_i(1) < lh(d + 0x2e)) {
            done = true;
            sh(d + 0x2e, static_cast<uint32_t>(frame_scale_i(1)));
        }
    } else if (frame_scale_i(0xc8) < lh(d + 0x2e)) {
        done = true;
        sh(d + 0x2e, static_cast<uint32_t>(frame_scale_i(0xc8)));
    }
    // turn toward its direction
    const uint32_t dir = ee_stack_alloc(0x10);
    const float yaw = lwc1(entry(cam) + 0x18);
    swc1(dir, cos_vu(yaw));
    swc1(dir + 4, sin_vu(lwc1(entry(cam) + 0x18)));
    sw(dir + 8, 0);
    const float speed = lwc1(d) * DEG * fpu_div(static_cast<float>(lh(d + 0x2e)), span);
    const int32_t k = lh(d + 0x2c);
    if (!((k == 1 || k == 7 || k == 9) && done)) cam0_turn_dir(dir, speed, lwc1(d + 0x20) * DEG);
    ee_stack_free(0x10);
    const bool slow = k == 6 || k == 8;
    if (lwc1(d + 0x24) != 0.0f) {
        if (slow) {
            cam0_set_distance(0, lwc1(d + 0x24), u2f(0x3b03126fu));
            cam0_set_22c(lwc1(d + 0x24) + u2f(0x3fae147bu));
        } else {
            cam0_set_distance(0, lwc1(d + 0x24), u2f(0x3b449ba6u));
            cam0_set_flag226();
        }
    }
    if (lwc1(d + 0x28) != 0.0f) cam0_set_height(lwc1(d + 0x28), slow ? u2f(0x3b03126fu) : u2f(0x3b449ba6u));
    if (lwc1(d + 0x30) != 0.0f) cam0_set_look_height(0, lwc1(d + 0x30), slow ? u2f(0x3b03126fu) : u2f(0x3ba3d70au));
    if (lwc1(d + 0x50) != 0.0f) cam0_set_1cc(lwc1(d + 0x50) * DEG);
    if (lh(d + 0x54) != 0) sw(work() + 0x1b8, lwu(work() + 0x1b8) | 2u);
    if (lh(d + 0x56) != 0) sw(work() + 0x1b8, lwu(work() + 0x1b8) | 1u);
    if (lh(d + 0x34) == 0) cam0_set_mode(0);
    if (lwc1(d + 0x38) != 0.0f || lwc1(d + 0x3c) != 0.0f) {
        if (!(lh(d + 0x2e) < frame_scale_i(0x78))) {
            cam0_set_rates(lwc1(d + 0x38), lwc1(d + 0x3c));
        } else {
            const float t = fpu_div(static_cast<float>(lh(d + 0x2e)), int_to_float(frame_scale_i(0x78)));
            const float a0 = lwc1(d + 0x40), b0 = lwc1(d + 0x44);
            cam0_set_rates(a0 + (lwc1(d + 0x38) - a0) * t, b0 + (lwc1(d + 0x3c) - b0) * t);
        }
    }
    const int32_t k2 = lh(d + 0x2c);
    if (k2 == 1 || k2 == 7 || k2 == 10) {
        cam0_set_flag226();
        cam0_set_flag116();
        cam0_set_124(u2f(0x3c23d70au), u2f(0x3e4ccccdu));
    }
}

// The type-5 camera's accept function: runs the modifier (unless kind 1 / 10 is waiting on a
// state, kind 0xb needs hero mode 2, kind 2 hero state 0x1a, or it is done with +0x48 / +0x4a);
// never takes over (0).
// orig: 0x002ed230
int32_t cam_mod5_accept(uint32_t cam) {
    const uint32_t d = lwu(entry(cam) + 0x1c);
    if (lh(d + 0x48) == 1 && lh(d + 0x4a) != 0) return 0;
    const int32_t kind = lh(d + 0x2c);
    if (kind == 0xb && lbu(0x001413f4) != 2) return 0;
    if ((kind == 1 || kind == 10) && lh(d + 0x2e) == 0 && lh(0x0013f350 + 0x30c) != 0) {
        if (lw(0x0015ed84) != 0xe) return 0;
        if (lw(0x0013f350 + 0x2284) != 0xd) return 0;
    }
    if (kind == 2 && lw(0x001413dc) != 0x1a) {
        sh(d + 0x2e, 0);
        return 0;
    }
    cam_mod_update(cam);
    return 0;
}

// Is the hero in the type-6 modifier's volume (box +0xc, cylinder +0x10, sphere +8), in hero
// mode 1 with +0x46 / not with +0x44, the camera still with +0x34; when not, it is done (+0x26).
// orig: 0x002ed348
int32_t cam_mod6_inside(uint32_t cam) {
    const uint32_t c = lwu(ACTIVE);
    const uint32_t d = lwu(entry(cam) + 0x1c);
    const uint32_t w = lwu(c + 0x70);
    if (lh(c + 0x86) != 0) return 0;
    if (lh(d + 0x46) != 0 && lbu(0x001413f4) != 1) return 0;
    if (lh(d + 0x44) != 0 && lbu(0x001413f4) == 1) return 0;
    if (lw(d + 0x34) != 0) {
        if (u2f(0x3c23d70au) < abs_f(lwc1(w + 0x130 + 0x34))) return 0;
        if (u2f(0x3c23d70au) < abs_f(lwc1(w + 0x1a8 + 8))) return 0;
    }
    if (lw(d + 0xc) >= 0 && point_in_box(HERO, lw(d + 0xc)) != 0) return 1;
    if (lw(d + 0x10) >= 0 && point_in_cylinder(HERO, lw(d + 0x10)) != 0) return 1;
    if (lw(d + 8) >= 0 && point_in_sphere(HERO, lw(d + 8)) != 0) return 1;
    sh(d + 0x26, 1);
    return 0;
}

// The type-6 modifier while the hero is inside (holding the claim, the pad not pushing out):
// for its first frames the follow camera is put at its spot (the entry's +0 .. +8) with the
// distance / height / look height that gives along the hero's axis, then held there.
// orig: 0x002ed498
void cam_mod6_update(uint32_t cam) {
    const uint32_t d = lwu(entry(cam) + 0x1c);
    auto leave = [&](bool release) {
        if (lw(d + 0x20) != 0) sh(d + 0x26, 1);
        sw(d + 0x20, 0);
        if (release) cam_mod_release(cam);
    };
    if (cam_mod6_inside(cam) == 0) {
        leave(true);
        return;
    }
    if (cam_mod_claim(cam) <= 0) {
        leave(false);
        return;
    }
    if ((lw(0x0013cae0) & 5) != 0) {
        leave(true);
        return;
    }
    if (lw(d + 0x20) < 2) {
        const uint32_t e = entry(cam);
        Vec4 spot;
        spot.x = lwc1(e);
        spot.y = lwc1(e + 4);
        spot.z = lwc1(e + 8);
        spot.w = 0.0f;
        Vec4 rel, along, across;
        vec3_sub(rel, spot, V(HERO));
        const float dot = vec3_dot(rel, V(AXIS));
        vec3_scale(along, V(AXIS), dot);
        vec3_sub(across, rel, along);
        const float dist = vec3_length(across);
        const float a = lwc1(d + 0x2c);
        swc1(d + 0x38, dist);
        swc1(d + 0x3c, -dot);
        if (abs_f(a) == u2f(0x3fc90fdbu)) {
            sw(d + 0x40, 0);
        } else {
            const float t = fpu_div(sin_vu(a), cos_vu(a));
            swc1(d + 0x40, lwc1(d + 0x3c) - lwc1(d + 0x38) * t);
        }
        const uint32_t c = lwu(ACTIVE), w = lwu(c + 0x70);
        const uint32_t s5 = w + 0x130, s1 = w + 0x20, b = w + 0x40;
        V(c + 0x30) = spot;
        swc1(s5 + 0x2c, lwc1(d + 0x38));
        swc1(s5 + 0x30, lwc1(d + 0x3c));
        swc1(b + 0xb0, lwc1(d + 0x40));
        Vec4 t;
        vec3_scale(t, V(AXIS), -lwc1(s5 + 0x30));
        vec3_add(V(w + 0x90), V(HERO), t);
        vec3_sub(V(s5), spot, V(w + 0x90));
        V(w + 0x140) = V(s5);
        vec3_scale(t, V(AXIS), -lwc1(b + 0xb0));
        vec3_add(V(w + 0x80), V(HERO), t);
        V(w + 0xd0) = V(w + 0x80);
        V(b) = V(HERO);
        V(w + 0xa0) = V(b);
        V(w) = V(c + 0x30);
        V(w + 0x1f0) = V(c + 0x30);
        sw(w + 0x1d0 + 0x30, 0);
        sw(s1 + 0x14, 0);
        swc1(s1 + 4, lwc1(b + 0xb0));
        const float z = lwc1(s1 + 0x14);
        swc1(s1 + 0xc, z);
        swc1(s1 + 8, lwc1(s5 + 0x30));
        sh(s1 + 2, 0);
        swc1(s1 + 0x18, z);
        swc1(s1 + 0x10, z);
    }
    sw(d + 0x20, lwu(d + 0x20) + 1u);
    if (frame_scale_i(0xc8) < lw(d + 0x20)) sw(d + 0x20, static_cast<uint32_t>(frame_scale_i(0xc8)));
    if (lwc1(d + 0x38) != 0.0f) {
        cam0_set_distance(0, lwc1(d + 0x38), u2f(0x3b449ba6u));
        cam0_set_flag226();
    }
    if (lwc1(d + 0x3c) != 0.0f) cam0_set_height(lwc1(d + 0x3c), u2f(0x3b449ba6u));
    if (lwc1(d + 0x40) != 0.0f) cam0_set_look_height(0, lwc1(d + 0x40), u2f(0x3ba3d70au));
    if (lwc1(d + 0x28) != 0.0f) cam0_set_1cc(lwc1(d + 0x28) * DEG);
    if (lh(d + 0x24) == 0) cam0_set_mode(0);
}

// The type-6 camera's accept function: done (+0x26) means nothing; in a cutscene state an
// inside modifier is reset, else it runs; never takes over (0).
// orig: 0x002ed8b0
int32_t cam_mod6_accept(uint32_t cam) {
    const uint32_t d = lwu(entry(cam) + 0x1c);
    if (lh(d + 0x26) != 0) return 0;
    if (lw(OV(0x0015f5c4)) != 0 && cam_mod6_inside(cam) != 0) {
        sw(d + 0x20, 0);
        sh(d + 0x26, 0);
        return 0;
    }
    cam_mod6_update(cam);
    return 0;
}

}  // namespace rac1
