// Ported overlay code: camera types 2 / 3 / 4 of the type table 0x1ea880 (ids 5 / 6 / 7), their
// init / update / exit functions and the helpers under them (the cosine eases 0x257ef0..0x258050,
// the polar point 0x1ed580, the collision sweep of the view 0x1ed6a8).
//
// Type 2 (id 5), the scripted camera: the mode +0x88 (set by 0x2eaaa0 and its siblings, not
// ported here) picks the motion: 1 snaps to the target position w+0x80 / angles w+0x90, 2
// cosine-eases there over the timer w+0xe0 (length w+0xe4), 3 swings round the hero (yaw /
// pitch / distance w+0xfc.. eased toward those of the target, the centre w+0x110 toward the
// hero), anything else follows the target with springs (position w+0x00.., angles w+0x30..).
// Type 3 (id 6), the orbit: yaw / pitch / distance / two offsets at cam+0x50.. sprung toward
// w+0x14.. about the centre w+0xb0 (sprung toward w+0x80 unless w+0x9c).
// Type 4 (id 7): the hero states 0x24..0x26 (the moby *(P+0x964)) and 0x2c (the moby
// *(P+0x994)): a yaw w+0xb8 sprung behind the hero (or toward the nearest moby of class 0x323
// / 0x2f6 listed for the ridden moby), the view pulled in by the level collision and pitched
// toward the hero.
#include <game/ovl/camera_types.h>

#include <game/ovl/ovl_reloc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/level/level_data.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350, HERO_POS = 0x0013f3d0;
constexpr OvConst<0x00166e00u> ACTIVE{};
constexpr OvConst<0x00166e04u> PREV{};
constexpr OvConst<0x00166c80u> VIEW{};
// orig: (helper) a 32-bit EE word as the int32_t& the timer helpers take
int32_t& ref32(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
// orig: (helper) repeated in 0x002ead60, 0x002eae20, 0x002eb278, 0x002eb480 — the rows from the
// Euler angles at cam+0x40 (built on the stack, rows 0..2 copied)
void rows_from_angles(uint32_t cam) {
    Mat4 m;
    euler_to_mat4_vu(m, V(cam + 0x40));
    mat3_copy(M(cam), m);
}
// orig: (helper) repeated in 0x002eb278, 0x002eb480 — the angles +0x40 from the orbit angles
// +0x50: pitch + pitch offset, pi + yaw + roll offset (the last write wins over the first)
void orbit_angles(uint32_t cam) {
    swc1(cam + 0x44, angle_add(lwc1(cam + 0x54), lwc1(cam + 0x5c)));
    const float f = angle_add(u2f(0x40490fdbu), lwc1(cam + 0x50));
    swc1(cam + 0x48, f);
    const float g = angle_add(f, lwc1(cam + 0x60));
    sw(cam + 0x4c, 0);
    swc1(cam + 0x48, g);
}
}  // namespace

// The five words of a spring block cleared.
// orig: 0x001eb0b0
void cam_springs_clear(uint32_t p) {
    sw(p + 0x10, 0);
    sw(p, 0);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0);
}

// d = s with its first angle turned by `yaw` (the four floats after it copied).
// orig: 0x001eb0c8
void cam_angles_copy_turned(uint32_t d, uint32_t s, float yaw) {
    swc1(d, angle_add(lwc1(s), yaw));
    swc1(d + 4, lwc1(s + 4));
    swc1(d + 8, lwc1(s + 8));
    swc1(d + 0xc, lwc1(s + 0xc));
    swc1(d + 0x10, lwc1(s + 0x10));
}

// out = base + the point at {yaw, pitch, distance} (ang +0 / +4 / +8); out.w = 0.
// orig: 0x001ed580
void camera_polar_point(uint32_t out, uint32_t ang, uint32_t base) {
    float c = cos_vu(lwc1(ang + 4));
    swc1(out, lwc1(ang + 8) * c * cos_vu(lwc1(ang)));
    c = cos_vu(lwc1(ang + 4));
    swc1(out + 4, lwc1(ang + 8) * c * sin_vu(lwc1(ang)));
    const float s = sin_vu(lwc1(ang + 4));
    sw(out + 0xc, 0);
    swc1(out + 8, lwc1(ang + 8) * s);
    vec3_add(V(out), V(out), V(base));
}

// The view moved from the active camera's drawn position (+0x64) to `pos` in steps under
// 0.9 radius, each step pushed out of the collision (up to six tries, the active camera's +0x89
// set when it was); then the view position 0x166dc0 (unless 0x16c06c). With the active camera's
// +0x8a set, `pos` is taken as it is (and +0x8a cleared).
// orig: 0x001ed6a8
void camera_collide_sweep(uint32_t pos, float radius) {
    const uint32_t a = lwu(ACTIVE);
    const Vec4 start = {lwc1(a + 0x64), lwc1(a + 0x68), lwc1(a + 0x6c), 0.0f};
    if (lh(a + 0x8a) != 0) {
        if (lw(OV(0x0016c06c)) == 0) lq_sq(OV(0x00166dc0), pos);
        sh(a + 0x8a, 0);
        return;
    }
    Vec4 step;
    vec3_sub(step, V(pos), start);
    const float len = vec3_length(step);
    const int32_t n = float_to_int(fpu_div(len, radius * u2f(0x3f666666u))) + 1;
    vec3_div(step, step, int_to_float(n));
    V(pos) = start;
    for (int32_t i = 0; i < n; ++i) {
        vec3_add(V(pos), V(pos), step);
        for (int32_t k = 0; k < 6; ++k) {
            if (sphere_collide(pos, radius, lwu(OV(0x0015ef5c)), lwu(P + 0x2080)) == 0) break;
            lq_sq(pos, OV(0x00173e70));
            const uint32_t c = lwu(ACTIVE);
            if (lbu(c + 0x89) == 0) sb(c + 0x89, 1);
        }
    }
    if (lw(OV(0x0016c06c)) == 0) lq_sq(OV(0x00166dc0), pos);
}

// x, y, z cleared (w kept).
// orig: 0x001ff250
void vec3_clear_xyz(uint32_t p) {
    sd(p, 0);
    sw(p + 8, 0);
}

// The angle from a toward b (the short way) at t (0..1) on a cosine ease.
// orig: 0x00257ef0
float angle_cos_ease(float a, float b, float t) {
    const float d = angle_sub(b, a);
    const float c = cos_vu(fpu_mul(t, u2f(0x40490fdbu)));
    return angle_add(a, fpu_mul(d, (1.0f - c) * 0.5f));
}

// out = a + (b - a) at t (0..1) on a cosine ease (a at 0, b at 1, whole quadwords).
// orig: 0x00257f70
void vec3_cos_ease(uint32_t out, uint32_t a, uint32_t b, float t) {
    if (t == 0.0f) {
        lq_sq(out, a);
        return;
    }
    if (t == 1.0f) {
        lq_sq(out, b);
        return;
    }
    const float c = cos_vu(fpu_mul(t, u2f(0x40490fdbu)));
    const float s = (1.0f - c) * 0.5f;
    Vec4 d;
    vec3_sub(d, V(b), V(a));
    vec3_scale(d, d, s);
    vec3_add(V(out), V(a), d);
}

// The three angles from a toward b (each the short way) at t on a cosine ease.
// orig: 0x00258050
void angles_cos_ease(uint32_t out, uint32_t a, uint32_t b, float t) {
    const float c = cos_vu(fpu_mul(t, u2f(0x40490fdbu)));
    const float s = (1.0f - c) * 0.5f;
    for (uint32_t o = 0; o < 0xc; o += 4) {
        const float d = angle_sub(lwc1(b + o), lwc1(a + o));
        swc1(out + o, angle_add(lwc1(a + o), fpu_mul(d, s)));
    }
}

// ---- type 2 (id 5) ------------------------------------------------------------------------

// The springs: position k 0.3 / damping 0.3 / max 1 (+0x10.., on +0x1c), angles 0.05 / 0.3 / 1
// (+0x50.., on +0x5c), their speeds cleared; the rows from the angles.
// orig: 0x002ead60
void cam5_init(uint32_t cam) {
    sw(lwu(cam + 0x70) + 0xc0, 4);
    const uint32_t a = lwu(cam + 0x70) + 0x30;
    swc1(a + 0x20, u2f(0x3d4ccccdu));
    swc1(a + 0x24, u2f(0x3e99999au));
    swc1(a + 0x28, 1.0f);
    sh(a + 0x2c, 1);
    vec_clear(V(a));
    const uint32_t w = lwu(cam + 0x70);
    swc1(w + 0x14, u2f(0x3e99999au));
    swc1(w + 0x18, 1.0f);
    sh(w + 0x1c, 1);
    swc1(w + 0x10, u2f(0x3e99999au));
    vec_clear(V(w));
    rows_from_angles(cam);
    sh(cam + 0x7e, 0);
}

// orig: 0x002eae20
void cam5_step(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    const uint32_t tgt = w + 0x80, t = w + 0xe0;
    const uint32_t mode = lbu(cam + 0x88);
    auto snap = [&] {
        lq_sq(cam + 0x30, tgt);
        lq_sq(cam + 0x40, w + 0x90);
    };
    if (mode == 1) {
        snap();
    } else if (mode == 2) {
        timer_tick_i32(ref32(t));
        const float n = int_to_float(lw(t + 4));
        const float f = fpu_div(static_cast<float>(lw(t + 4) - lw(w + 0xe0)), n);
        vec3_cos_ease(cam + 0x30, cam + 0x30, tgt, f);
        angles_cos_ease(cam + 0x40, cam + 0x40, w + 0x90, f);
    } else if (mode == 3) {
        if (timer_tick_i32(ref32(t)) != 0) {
            snap();
        } else {
            const float n = int_to_float(lw(t + 4));
            const float f = fpu_div(static_cast<float>(lw(t + 4) - lw(w + 0xe0)), n);
            sub_001ed630(t + 8, tgt, HERO_POS);
            const float dyaw = angle_sub(lwc1(t + 8), lwc1(t + 0x1c));
            const float vyaw = angle_sub(lwc1(tgt + 0x18), lwc1(OV(0x00166dd8)));
            const float c = u2f(0x3eb2b8c2u);
            bool cross = false;  // the swing the other way round (both turns over 20 degrees, opposite)
            if (c < dyaw && vyaw < -c) cross = true;
            else if (dyaw < -c && c < vyaw) cross = true;
            if (cross) {
                float d = lwc1(t + 8) - lwc1(t + 0x1c);
                if (d < 0.0f) {
                    if (0.0f < vyaw) d = -d;
                } else if (0.0f < d) {
                    if (vyaw < 0.0f) d = -d;
                }
                swc1(t + 0x1c, lwc1(t + 0x1c) + cos_ease(0.0f, d, f));
            } else {
                swc1(t + 0x1c, angle_cos_ease(lwc1(t + 0x1c), lwc1(t + 8), f));
            }
            const float e = cubic_interp(lwc1(t + 0x44), 0.0f, 1.0f, lwc1(t + 0x48), f);
            const float d0 = lwc1(t + 0x24);
            swc1(t + 0x24, d0 + (lwc1(t + 0x10) - d0) * e);
            swc1(t + 0x20, angle_cos_ease(lwc1(t + 0x20), lwc1(t + 0xc), f));
            vec3_cos_ease(t + 0x30, t + 0x30, HERO_POS, f);
            camera_polar_point(cam + 0x30, t + 0x1c, t + 0x30);
            angles_cos_ease(cam + 0x40, cam + 0x40, w + 0x90, f);
        }
    } else {
        if (lh(w + 0x1c) == 1) {
            swc1(cam + 0x30, camera_spring(lwc1(cam + 0x30), lwc1(w + 0x80), lwc1(w + 0x10), lwc1(w + 0x14), lwc1(w + 0x18), w));
            swc1(cam + 0x34, camera_spring(lwc1(cam + 0x34), lwc1(tgt + 4), lwc1(w + 0x10), lwc1(w + 0x14), lwc1(w + 0x18), w + 4));
            swc1(cam + 0x38, camera_spring(lwc1(cam + 0x38), lwc1(tgt + 8), lwc1(w + 0x10), lwc1(w + 0x14), lwc1(w + 0x18), w + 8));
        }
        const uint32_t a = w + 0x30;
        if (lh(a + 0x2c) == 1) {
            swc1(cam + 0x40, camera_angle_spring(lwc1(cam + 0x40), lwc1(tgt + 0x10), lwc1(a + 0x20), lwc1(a + 0x24), lwc1(a + 0x28), a));
            swc1(cam + 0x44, camera_angle_spring(lwc1(cam + 0x44), lwc1(tgt + 0x14), lwc1(a + 0x20), lwc1(a + 0x24), lwc1(a + 0x28), w + 0x34));
            swc1(cam + 0x48, camera_angle_spring(lwc1(cam + 0x48), lwc1(tgt + 0x18), lwc1(a + 0x20), lwc1(a + 0x24), lwc1(a + 0x28), w + 0x38));
        }
    }
    rows_from_angles(cam);
}

// The step, then (with w+0x120 set) the view pushed out of the collision (radius 0.5).
// orig: 0x002eb218
void cam5_update(uint32_t cam) {
    const uint32_t t = lwu(cam + 0x70) + 0xe0;
    cam5_step(cam);
    if (lw(t + 0x40) != 0) {
        sb(cam + 0x89, 0);
        camera_collide_sweep(cam + 0x30, 0.5f);
    }
}

// ---- type 3 (id 6) ------------------------------------------------------------------------

// The position from the orbit angles about the centre w+0xb0 (the yaw turned by w+0xc8 first),
// then the angles and the rows.
// orig: 0x002eb278
void cam6_place(uint32_t cam) {
    const uint32_t c = lwu(cam + 0x70) + 0xb0;
    camera_polar_point(cam + 0x30, cam + 0x50, c);
    const float yaw = angle_add(lwc1(cam + 0x50), lwc1(c + 0x18));
    sw(cam + 0x40, 0);
    swc1(cam + 0x50, yaw);
    orbit_angles(cam);
    rows_from_angles(cam);
}

// The orbit's spring constants, the speeds cleared, the view placed.
// orig: 0x002eb3b0
void cam6_reset(uint32_t cam) {
    const float k4 = u2f(0x3d23d70au), k7 = u2f(0x3d8f5c29u), k3 = u2f(0x3e99999au), k99 = u2f(0x3f7d70a4u);
    sw(lwu(cam + 0x70) + 0xf0, 0);
    const uint32_t w = lwu(cam + 0x70);
    sw(w + 0x9c, 0);
    sw(w + 0xa8, 0);
    sw(w + 0xac, 0);
    swc1(w + 0xa4, k3);
    swc1(w + 0x48, k99);
    swc1(w + 0x4c, k4);
    swc1(w + 0x34, 1.0f);
    swc1(w + 0x38, k7);
    swc1(w + 0xa0, k3);
    swc1(w + 0x3c, k4);
    swc1(w + 0x40, k4);
    swc1(w + 0x44, k4);
    swc1(w + 0x28, k7);
    swc1(w + 0x2c, 1.0f);
    swc1(w + 0x30, k7);
    cam_springs_clear(w + 0x50);
    cam_springs_clear(w);
    vec3_clear_xyz(w + 0x90);
    cam6_place(cam);
    sh(cam + 0x7e, 0);
}

// The orbit angles sprung toward w+0x14.. (the yaw turned by w+0xc8), the centre toward w+0x80
// (or put there with w+0x9c), the position and rows from them.
// orig: 0x002eb480
void cam6_step(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    const uint32_t c = w + 0xb0;
    const float yaw = angle_add(lwc1(c + 0x18), lwc1(w + 0x14));
    swc1(cam + 0x50, camera_angle_spring(lwc1(cam + 0x50), yaw, lwc1(w + 0x28), lwc1(w + 0x3c), lwc1(w + 0x50), w));
    swc1(cam + 0x54, camera_angle_spring(lwc1(cam + 0x54), lwc1(w + 0x18), lwc1(w + 0x2c), lwc1(w + 0x40), lwc1(w + 0x54), w + 4));
    swc1(cam + 0x58, camera_spring(lwc1(cam + 0x58), lwc1(w + 0x1c), lwc1(w + 0x30), lwc1(w + 0x44), lwc1(w + 0x58), w + 8));
    swc1(cam + 0x5c, camera_angle_spring(lwc1(cam + 0x5c), lwc1(w + 0x20), lwc1(w + 0x34), lwc1(w + 0x48), lwc1(w + 0x5c), w + 0xc));
    swc1(cam + 0x60, camera_angle_spring(lwc1(cam + 0x60), lwc1(w + 0x24), lwc1(w + 0x38), lwc1(w + 0x4c), lwc1(w + 0x60), w + 0x10));
    if (lw(w + 0x9c) == 0) {
        swc1(w + 0xb0, camera_spring(lwc1(w + 0xb0), lwc1(w + 0x80), lwc1(w + 0xa0), lwc1(w + 0xa4), lwc1(w + 0xa8), w + 0x90));
        swc1(c + 4, camera_spring(lwc1(c + 4), lwc1(w + 0x84), lwc1(w + 0xa0), lwc1(w + 0xa4), lwc1(w + 0xa8), w + 0x94));
        swc1(c + 8, camera_spring(lwc1(c + 8), lwc1(w + 0x88), lwc1(w + 0xa0), lwc1(w + 0xa4), lwc1(w + 0xa8), w + 0x98));
    } else {
        lq_sq(c, w + 0x80);
    }
    camera_polar_point(cam + 0x30, cam + 0x50, c);
    swc1(cam + 0x40, lwc1(w + 0xac));
    orbit_angles(cam);
    rows_from_angles(cam);
}

// The centre 2 above the hero (w+0xc0: the hero's +0x10 row), the target there, the orbit
// targets cleared but the distance 4.64, the orbit angles from them, then the reset.
// orig: 0x002ebb00
void cam6_init(uint32_t cam) {
    const uint32_t w0 = lwu(cam + 0x70);
    const uint32_t c = w0 + 0xb0;
    lq_sq(c, HERO_POS);
    swc1(c + 8, lwc1(c + 8) + 2.0f);
    lq_sq(w0 + 0xc0, HERO_POS + 0x10);
    const uint32_t w = lwu(cam + 0x70);
    lq_sq(w + 0x80, c);
    cam_springs_clear(w + 0x14);
    swc1(w + 0x1c, u2f(0x40947ae1u));
    cam_angles_copy_turned(cam + 0x50, w + 0x14, 0.0f);
    cam6_reset(cam);
}

// orig: 0x002ebbb0
void cam6_update(uint32_t cam) { cam6_step(cam); }

// ---- type 4 (id 7) ------------------------------------------------------------------------

// orig: 0x002ebbd8
void cam7_init(uint32_t cam) {
    const int32_t level = lw(0x0015ed84);
    const uint32_t s = lwu(cam + 0x70) + 0x40;
    float h = 1.5f;
    sw(s + 0x40, 0);
    if (level == 0xe && static_cast<uint32_t>(lw(P + 0x2084) - 0x2c) < 2u) h = 2.0f;
    swc1(s + 0x4c, h);
    swc1(s + 0x48, u2f(0x3e567750u));
    swc1(s + 0x58, u2f(0x3ae4c388u));
    swc1(s + 0x5c, u2f(0xbb8efa35u));
    sw(s + 0x44, lwu(P + 0x2080));
    const uint32_t d = lwu(cam + 0x70) + 0xa0;
    swc1(d, u2f(0x40947ae1u));
    swc1(d + 0x14, u2f(0x3d4ccccdu));
    swc1(d + 4, 0.5f);
    swc1(d + 0x10, 2.0f);
    const uint32_t w = lwu(cam + 0x70);
    sw(w + 0x18, 0);
    swc1(w + 0x10, u2f(0x3c23d70au));
    swc1(w + 0x14, u2f(0x3e99999au));
    const int32_t n = frame_scale_i(0x78);
    sh(w + 0x1c, 0);
    sh(w + 0x1e, static_cast<uint32_t>(n));
    vec_clear(V(w));
    const uint32_t v = lwu(cam + 0x70);
    swc1(v + 0x34, u2f(0x3ca3d70au));
    swc1(v + 0x38, u2f(0x3e4ccccdu));
    sw(v + 0x3c, 0);
    sw(v + 0x30, 0);
    const uint32_t w3 = lwu(cam + 0x70);
    const uint32_t yaw = w3 + 0xb8;
    sw(yaw + 4, 0);
    const uint32_t prev = lwu(PREV);
    lq_sq(cam + 0x30, prev + 0x30);
    lq_sq(cam, prev);
    lq_sq(cam + 0x10, prev + 0x10);
    lq_sq(cam + 0x20, prev + 0x20);
    lq_sq(cam + 0x40, cam);
    sh(cam + 0x7e, 0);
    uint32_t moby;
    if (static_cast<uint32_t>(lw(P + 0x2084) - 0x2c) < 2u) {
        swc1(s + 0x30, cos_vu(lwc1(P + 0x9ac)));
        swc1(s + 0x34, sin_vu(lwc1(P + 0x9ac)));
        sw(s + 0x38, 0);
        swc1(w3 + 0xb8, lwc1(P + 0x9ac));
        moby = lwu(P + 0x994);
    } else {
        vec3_sub(V(s + 0x30), V(lwu(P + 0x964) + 0x10), V(cam + 0x30));
        sw(s + 0x38, 0);
        sw(s + 0x3c, 0);
        vec3_set_length(V(s + 0x30), V(s + 0x30), 1.0f);
        swc1(w3 + 0xb8, atan2_f(lwc1(s + 0x30), lwc1(s + 0x34)));
        moby = lwu(P + 0x964);
    }
    sw(yaw + 8, moby);
    sh(VIEW + 0x270, 1);
    sb(VIEW + 0x273, 2);
    sw(VIEW + 0x2f4, static_cast<uint32_t>(frame_scale_i(0x3c)));
}

// The distances eased (by the frame step 0x15ed6c), the yaw w+0xb8 sprung, the view put
// behind the hero along it and pulled in by the level collision.
// orig: 0x002ebe70
void cam7_track(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    const uint32_t yaw = w + 0xb8, s6 = w + 0x40, dist = w + 0xa0;
    const uint32_t ride = lwu(P + 0x994);
    float far, h;
    if (ride != 0 && lw(P + 0x2084) == 0x2c) {
        float f = lwc1(ride + 0x18) - lwc1(P + 0x88);
        const float top = lwc1(P + 0x9a0);
        if (top < f) f = top;
        else if (f < 0.0f) f = 0.0f;
        f = fpu_div(f, lwc1(P + 0x9a0));
        far = f + u2f(0x4069999au);
        h = f + f;
    } else {
        far = u2f(0x40947ae1u);
        h = 2.0f;
    }
    approach_f(lwu(lwu(ACTIVE) + 0x70) + 0xa0, far, lwc1(0x0015ed6c) * 8.0f);
    approach_f(lwu(lwu(ACTIVE) + 0x70) + 0xb0, h, lwc1(0x0015ed6c) * 4.0f);
    if (lw(0x0015ed84) == 0xe && lw(P + 0x2084) == 0x2c)
        approach_f(lwu(lwu(ACTIVE) + 0x70) + 0x8c, h + 0.25f, lwc1(0x0015ed6c) * 8.0f);
    else
        approach_f(lwu(lwu(ACTIVE) + 0x70) + 0x8c, h - 0.5f, lwc1(0x0015ed6c) * 8.0f);
    const uint32_t pos = cam + 0x30;
    const int32_t st = lw(P + 0x2084);
    bool spring = true;
    float target = 0.0f, k = u2f(0x3aa3d70au);
    // the yaw target: behind the hero (+0x98), the angle of the moby +0x84 (when +0x80 is 1), or
    // the nearest moby of class 0x323 / 0x2f6 in front, from the list 0x1ab840 of the ridden moby
    auto own = [&] { target = lw(s6 + 0x40) == 1 ? lwc1(lwu(s6 + 0x44) + 0x48) : lwc1(P + 0x98); };
    if (st == 0x2c) {
        const int32_t level = lw(0x0015ed84);
        bool list = false;
        if (level == 0xe || level == 7 || level == 9) {
            const uint32_t r = lwu(P + 0x994);
            list = r == 0 || lbu(r + 0x21) != 0xff;
        }
        if (!list) {
            target = lwc1(P + 0x98);
        } else {
            const uint32_t r = lwu(P + 0x994);
            uint32_t best = 0;
            float bd = u2f(0x447a0000u);
            uint32_t e = lvd::group(lbu(r + 0x21));
            if (e != 0) {
                int16_t v;
                do {
                    const uint32_t m = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
                    if (m != r && (lh(m + 0xa6) == 0x323 || lh(m + 0xa6) == 0x2f6)) {
                        Vec4 d, n;
                        vec3_sub(d, V(m + 0x10), V(r + 0x10));
                        vec3_set_length(n, d, 1.0f);
                        const Vec4 f = {cos_vu(lwc1(P + 0x98)), sin_vu(lwc1(P + 0x98)), 0.0f, 0.0f};
                        const float dot = vec3_dot(n, f);
                        if (0.0f < dot) {
                            const float l = vec3_length(d) - dot * 10.0f;
                            if (l < bd) {
                                best = m;
                                bd = l;
                            }
                        }
                    }
                    v = lh(e);
                    e += 2;
                } while (v >= 0);
            }
            if (best != 0) {
                Vec4 d;
                vec3_sub(d, V(best + 0x10), V(r + 0x10));
                target = atan2_f(d.x, d.y);
            } else {
                own();
            }
        }
    } else if (st == 0x25) {
        own();
        k = lwc1(OV(0x00161dd8));
    } else {
        spring = false;
    }
    if (spring) swc1(yaw, camera_angle_spring(lwc1(yaw), target, k, u2f(0x3e333333u), 0.0f, yaw + 4));
    swc1(s6 + 0x30, cos_vu(lwc1(yaw)));
    swc1(s6 + 0x34, sin_vu(lwc1(yaw)));
    sw(s6 + 0x38, 0);
    swc1(s6 + 0x3c, lwc1(s6 + 0x38));
    Vec4 at;
    vec3_set_length(at, V(s6 + 0x30), -lwc1(dist));
    at.z = at.z + lwc1(dist + 0x10);
    vec3_add(at, at, V(HERO_POS));
    if (segment_collide_v(V(pos), at, 0x12, 0, nullptr) == 0) {
        V(pos) = at;
        return;
    }
    Vec4 d;
    vec3_sub(d, V(OV(0x00173e60)), V(pos));
    const float l = vec3_length(d);
    if (0.5f < l) {
        vec3_scale(d, d, fpu_div(l - 0.5f, l));
        vec3_add(at, V(pos), d);
        for (int32_t i = 0; i < 5; ++i) {
            if (sphere_collide_v(at, 0.75f, 0, 0) == 0) break;
            at = V(OV(0x00173e70));
        }
    }
    V(pos) = at;
    vec3_sub(d, V(HERO_POS), V(pos));
    swc1(yaw, atan2_f(d.x, d.y));
    sw(yaw + 4, 0);
}

// The view turned toward the hero (+w+0x8c above): the pitch it is off by, sprung (or in hero
// state 0x25 taken in a ramp w+0x30 up to 1); the rows rebuilt with up 0x166e30.
// orig: 0x002ec4a8
void cam7_aim(uint32_t cam) {
    const uint32_t w = lwu(cam + 0x70);
    Vec4 at = V(HERO_POS);
    at.z = at.z + lwc1(w + 0x8c);
    Vec4 dir;
    vec3_sub(dir, at, V(cam + 0x30));
    vec3_set_length(dir, dir, 1.0f);
    const float off = u2f(0x3fc90fdbu) - asin_f(vec3_dot(dir, V(cam)));
    float turn;
    if (lw(HERO_POS + 0x2004) == 0x25) {
        const float r = lwc1(w + 0x30) + u2f(0x3c8b4396u);
        swc1(w + 0x30, r);
        if (1.0f < r) swc1(w + 0x30, 1.0f);
        turn = off * lwc1(w + 0x30);
    } else {
        turn = camera_angle_spring(0.0f, off, u2f(0x3ca3d70au), u2f(0x3e333333u), 0.0f, w + 0x30);
    }
    Vec4 axis;
    vec3_cross_ba(axis, dir, V(cam + 0x40));
    vec_rotate_axis(V(cam), V(cam + 0x40), axis, turn);
    vec3_set_length(V(cam + 0x40), V(cam), 1.0f);
    lq_sq(cam, cam + 0x40);
    vec3_cross_ba(V(cam + 0x10), V(cam), V(OV(0x00166e30)));
    vec3_set_length(V(cam + 0x10), V(cam + 0x10), 1.0f);
    vec3_cross_ba(V(cam + 0x20), V(cam + 0x10), V(cam));
}

// orig: 0x002ec678
void cam7_step(uint32_t cam) {
    cam7_track(cam);
    cam7_aim(cam);
}

// When the hero's moby (+0x994 in state 0x2c, +0x964 in 0x24..0x26) is not the one w+0xc0
// was set up for: the previous camera takes this view, the camera starts again (the view
// timer 0x166f74 set to 45 frames). Then the step and the view pushed out of the collision.
// orig: 0x002ec6c0
void cam7_update(uint32_t cam) {
    const uint32_t yaw = lwu(cam + 0x70) + 0xb8;
    const int32_t st = lw(P + 0x2084);
    bool check = true;
    uint32_t moby = 0;
    if (st == 0x2c) moby = lwu(P + 0x994);
    else if (static_cast<uint32_t>(st - 0x24) < 3u) moby = lwu(P + 0x964);
    else check = false;
    if (check && lwu(yaw + 8) != moby) {
        const uint32_t prev = lwu(PREV);
        lq_sq(prev + 0x30, cam + 0x30);
        lq_sq(prev, cam);
        lq_sq(prev + 0x10, cam + 0x10);
        lq_sq(prev + 0x20, cam + 0x20);
        cam7_init(cam);
        sw(VIEW + 0x2f4, static_cast<uint32_t>(frame_scale_i(0x2d)));
    }
    cam7_step(cam);
    camera_collide_sweep(cam + 0x30, 0.5f);
    sw(lwu(cam + 0x70) + 0x80, 0);
}

// Leaving (unless the hero is still in the camera's state, or in state 6 by +0x2084): blend
// back (+0x7e = 3).
// orig: 0x002ec7b0
void cam7_exit(uint32_t cam) {
    if (lw(P + 0x2284) == lh(cam + 0x86)) return;
    if (lw(P + 0x2084) == 6) return;
    sb(cam + 0x7d, 0);
    sh(cam + 0x7e, 3);
}

}  // namespace rac1
