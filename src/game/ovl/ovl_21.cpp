// Ported overlay code 0x00210000..0x0021ffff.
#include <game/enhance/advanced.h>
#include <game/lib/libvu0.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>

namespace rac1 {

// orig: 0x00211360
int32_t moby_is_class_0x47(uint32_t moby) {
    if (moby == 0) return 0;
    return lh(moby + 0xa6) == 0x47 ? 1 : 0;
}

// orig: 0x00211870 — true when v lies in (limit - window, limit) with
// limit = PLAYER+0xaa8 and window = PLAYER+0xaac (window bound inclusive).
int32_t sub_00211870(float v) {
    const float limit = lwc1(g::PLAYER + 0xaa8);
    if (!(v < limit)) return 0;
    const float d = limit - v;
    return d <= lwc1(g::PLAYER + 0xaac) ? 1 : 0;
}

// orig: 0x00216ec0 — store (a, b, c) in the first free entry of the 8-entry
// table at PLAYER+0x21d8 (stride 8, entry free when its first s16 is 0).
void sub_00216ec0(int16_t a, int16_t b, int16_t c) {
    uint32_t p = g::PLAYER + 0x21d8;
    for (int32_t i = 0; i < 8; ++i, p += 8) {
        if (lh(p) == 0) {
            sh(p + 2, static_cast<uint16_t>(a));
            sh(p + 0, 1);
            sh(p + 6, static_cast<uint16_t>(c));
            sh(p + 4, static_cast<uint16_t>(b));
            return;
        }
    }
}

// orig: 0x00210c80 — scale xy by s and z by z, or (flag 0) set the xy length to s and z to z.
void sub_00210c80_v(Vec4& v, int32_t flag, float s, float z) {
    if (flag != 0) {
        const float x = v.x * s, y = v.y * s, zz = v.z * z;
        v.x = x;
        v.y = y;
        v.z = zz;
    } else {
        vec2_set_length(v, v, s);
        v.z = z;
    }
}
// orig: 0x00210c80 (EE-address form)
void sub_00210c80(uint32_t v, int32_t flag, float s, float z) { sub_00210c80_v(V(v), flag, s, z); }

// orig: 0x00211830 — stick magnitude of the move vector 0x141070, capped at 1.
float sub_00211830() {
    const float m = vec2_length(V(0x00141070));
    return 1.0f < m ? 1.0f : m;
}

// orig: 0x00213350 — dot(v, (cos yaw, sin yaw, 0)) with yaw = PLAYER+0x98.
float sub_00213350_v(const Vec4& v) {
    const Vec4 a = v;
    Vec4 dir;
    dir.x = cos_vu(lwc1(g::PLAYER + 0x98));
    dir.y = sin_vu(lwc1(g::PLAYER + 0x98));
    dir.z = 0.0f;
    dir.w = 0.0f;
    return vec3_dot(a, dir);
}
// orig: 0x00213350 (EE-address form)
float sub_00213350(uint32_t v) { return sub_00213350_v(V(v)); }

// orig: 0x00213de8 — scale four vectors around 0x13f450 and the float 0x13f4e4 by s.
void sub_00213de8(float s) {
    constexpr uint32_t b = 0x0013f450;
    vec3_scale(V(b), V(b), s);
    vec3_scale(V(b + 0x10), V(b + 0x10), s);
    vec3_scale(V(b - 0x20), V(b - 0x20), s);
    vec3_scale(V(b + 0x50), V(b + 0x50), s);
    swc1(b - 0x100 + 0x194, lwc1(b - 0x100 + 0x194) * s);
}

// orig: 0x00212088 — approach PLAYER+0x194 towards PLAYER+0x190, with `up` as the step
// while below it and `down` otherwise.
float sub_00212088(float up, float down) {
    const float target = lwc1(g::PLAYER + 0x190);
    if (lwc1(g::PLAYER + 0x194) < target) return approach_f(g::PLAYER + 0x194, target, up);
    return approach_f(g::PLAYER + 0x194, target, down);
}

// orig: 0x002132b8 — 0x13fc70 = s * spherical(PLAYER+0x930 r, +0x938 a, +0x934 b), with its
// length reduced by `dead` (zero inside the dead zone).
void sub_002132b8(float s, float dead) {
    constexpr uint32_t v = 0x0013fc70;
    vec_from_spherical(v, lwc1(g::PLAYER + 0x930), lwc1(g::PLAYER + 0x938), lwc1(g::PLAYER + 0x934));
    vec3_scale(V(v), V(v), s);
    const float len = vec3_length(V(v));
    if (len <= dead) {
        vec_clear(V(v));
        return;
    }
    vec3_set_length(V(v), V(v), len - dead);
}

// orig: 0x00216078 — pitch/yaw of v in the hero's local frame: out[0] = atan2(|xz|, y),
// out[1] = -atan2(z, x).
void sub_00216078_v(const Vec4& v, float* out) {
    Vec4 l = v;
    Mat4 m, mt;
    euler_to_mat4_vu(m, V(lwu(g::HERO_MOBY) + 0x40));
    mat3_transpose(mt, m);
    mat4_transform(l, l, mt);
    const float h = sqrt_f(l.x * l.x + l.z * l.z);
    out[0] = atan2_f(h, l.y);
    out[1] = -atan2_f(l.z, l.x);
}
// orig: 0x00216078 (EE-address form)
void sub_00216078(uint32_t v, float* out) { sub_00216078_v(V(v), out); }

// orig: 0x002120d8 — set the hero velocity 0x13f430 to speed PLAYER+0x194 along `yaw` (when it
// is <= pi, else PLAYER+0x98) and pitch PLAYER+0x2e4; in move modes 1/2 along the hero's local
// x axis instead. The w lane comes from an uninitialised stack slot in the original (port: 0).
void sub_002120d8(float yaw) {
    float a = lwc1(g::PLAYER + 0x98);
    if (yaw <= u2f(0x40490fdbu)) a = yaw;
    if (advanced_strafing()) a = lwc1(g::PLAYER + 0x180);  // enhancement: the strafe moves along the stick, not the facing
    const float speed = lwc1(g::PLAYER + 0x194);
    Vec4 v{0.0f, 0.0f, 0.0f, 0.0f};
    if (lbu(g::PLAYER + 0x20b3) != 0) {
        v.x = cos_vu(0.0f) * speed;
        v.y = sin_vu(0.0f) * speed;
        v.z = 0.0f;
        Mat4 m;
        euler_to_mat4_vu(m, V(lwu(g::PLAYER + 0x2080) + 0x40));
        mat4_transform(v, v, m);
    } else {
        vec_from_spherical_v(v, speed, a, lwc1(g::PLAYER + 0x2e4));
    }
    V(0x0013f430) = v;
}

// orig: 0x00216710 — reduce the horizontal part of 0x13f4a0 by `dead` and add it to 0x13f430.
void sub_00216710(float dead) {
    constexpr uint32_t v = 0x0013f4a0;
    const float l = sub_002339d0(v);
    if (!(u2f(0x3a83126fu) < l)) {
        vec_clear(V(v));
        return;
    }
    sub_00233b20(v, v, 0.0f);
    float n = l - dead;
    if (n < 0.0f) n = 0.0f;
    sub_00233ba0(v, v, n);
    vec3_add(V(v - 0x70), V(v - 0x70), V(v));
}

// orig: 0x002126b8 — positions a (hero pos raised by ha) and b (raised by hb): along -0x13f5e0
// (flag set, then kept above ground by sub_0025d238) or along z (b.z clamped at 0).
void sub_002126b8_v(Vec4& a, Vec4& b, int32_t flag, float ha, float hb) {
    constexpr uint32_t UP = 0x0013f5e0, POS = 0x0013f3d0;
    if (flag != 0) {
        Vec4 t;
        vec3_set_length(t, V(UP), -hb);
        vec3_add(b, V(POS), t);
        vec3_set_length(t, V(UP), -ha);
        vec3_add(a, V(POS), t);
        const Vec4 l = a;
        sub_0025d238_v(l, b);
        return;
    }
    const Vec4 p = V(POS);
    b = p;
    const float z = b.z + hb;
    b.z = z;
    if (z < 0.0f) b.z = 0.0f;
    a = p;
    a.z = a.z + ha;
}
// orig: 0x002126b8 (EE-address form)
void sub_002126b8(uint32_t a, uint32_t b, int32_t flag, float ha, float hb) { sub_002126b8_v(V(a), V(b), flag, ha, hb); }

// orig: 0x00211cf8 — compose the hero rotation 0x13f3e0 with Euler angles (x, y, z).
void sub_00211cf8(float x, float y, float z) {
    constexpr uint32_t ROT = 0x0013f3e0;
    Mat4 a, b;
    const Vec4 ang{x, y, z, 0.0f};
    euler_to_mat4_vu(a, ang);
    euler_to_mat4_vu(b, V(ROT));
    mat4_mul(b, b, a);
    mat_to_euler_v(b, ROT);
}

// orig: 0x002121c0 — while sliding on a steep slope (normal PLAYER+0x200 steeper than
// 50 degrees), remove the hero's velocity component into the slope and push it down.
void sub_002121c0() {
    if (lh(g::PLAYER + 0x30e) == 0) return;
    if (!(lwc1(g::PLAYER + 0x2dc) < u2f(0x3e19999au))) return;
    if (!(0.0f < lwc1(g::PLAYER + 0x108))) return;
    if (lbu(g::PLAYER + 0x257) == 0) return;
    if (!(0.0f < lwc1(g::PLAYER + 0x208))) return;
    const uint32_t n = g::PLAYER + 0x200;
    const float a = atan2_f(lwc1(g::PLAYER + 0x208), vec2_length(V(n)));
    if (!(u2f(0x3f5f66f3u) <= a)) return;
    Vec4 t = V(n);
    t.z = 0.0f;
    vu0_normalize(t, t);
    const float d = -lwc1(g::PLAYER + 0xe0) * t.x - lwc1(g::PLAYER + 0xe4) * t.y;
    if (!(0.0f < d)) return;
    vec3_scale(t, t, d);
    vec3_add(V(g::PLAYER + 0xe0), V(g::PLAYER + 0xe0), t);
    const float k = lwc1(0x0015ed70) * 54.0f;
    swc1(g::PLAYER + 0xe8, lwc1(g::PLAYER + 0xe8) - (k + k));
}

// orig: 0x00215ef8 — in state 2, drive the held item's (PLAYER+0x1184) sway parameters in the
// effect block FX_PARAMS from the hero's turn rate PLAYER+0x188.
void sub_00215ef8() {
    uint32_t m = lwu(g::PLAYER + 0x1184);
    if (m == 0) return;
    const float base = lwc1(0x0015ee18);
    if (lbu(0x0015edb3) != 0) {
        swc1(OV(0x0017b38c), base);
        m = lwu(g::PLAYER + 0x1184);
    }
    const float k = fpu_div(1024.0f, lwc1(m + 0x2c));
    if (lw(g::HERO_STATE) != 2) return;
    float t = lwc1(g::PLAYER + 0x188);
    swc1(g::FX_PARAMS + 0xd04, u2f(0x3ca3d70au));
    swc1(g::FX_PARAMS + 0xd08, u2f(0x3eb33333u));
    const float lim = u2f(0x3fb33333u);
    if (lim < t) t = lim;
    else if (t < -lim) t = -lim;
    float r = -t * u2f(0x3f99999au);
    const float rl = u2f(0x3fa78d36u);
    if (rl < r) r = rl;
    else if (r < -rl) r = -rl;
    swc1(g::FX_PARAMS + 0xcc8, r);
    const float tenth = u2f(0x3dcccccdu);
    float a = abs_f(t) * tenth + tenth;
    if (u2f(0x3d8f5c29u) < a) a = u2f(0x3d8f5c29u);
    swc1(g::FX_PARAMS + 0xcf8, a * k);
}

// orig: 0x00211d78 — spring the hero's yaw (velocity 0x13f4d4, rest 0x13f4d8) towards the
// direction of 0x13f4c0 seen through the matrix 0x13f390, then apply the step (sub_00211cf8).
void sub_00211d78(int32_t dir, float k, float d, float vmax) {
    constexpr uint32_t V0 = 0x0013f4c0;
    Vec4 v;
    mat4_transform(v, V(V0), M(V0 - 0x130));
    const float yaw = atan2_f(v.x, v.y);
    float ang = 0.0f;
    swc1(V0 + 0x18, spring_angle_v(ang, V0 + 0x14, dir, yaw, k, d, vmax));
    sub_00211cf8(0.0f, 0.0f, ang);
}

// orig: 0x00217450 — burst of 10 sparks (particle 4) around the hero's feet.
void sub_00217450() {
    const int32_t life = frame_scale_i(0x28);
    for (int32_t n = 9; n >= 0; --n) {
        const int32_t a = float_to_int(fpu_div(rand_range_f(u2f(0x45834000u), u2f(0x45e5b000u)), 1000.0f));
        const int32_t b = float_to_int(fpu_div(rand_range_f(u2f(0x46a41000u), u2f(0x46f61801u)), 1000.0f));
        const float k = lwc1(0x0015ed6c);
        const float speed = rand_range_f(k * u2f(0x3e19999au), k * u2f(0x3f333333u));
        Vec4 pos = V(0x0013f3d0);
        if (lw(0x0015ed84) != 6) pos.z = lwc1(g::PLAYER + 0x2f4);
        const float yaw = rand_angle();
        const float pitch = rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3fb2b8c2u));
        const float r = rand_range_f(u2f(0x3d4ccccdu), lwc1(g::PLAYER + 0x234) + u2f(0x3e4ccccdu));
        Vec4 vel{0.0f, 0.0f, 0.0f, 0.0f};  // w: uninitialised stack lane in the original
        vec_from_spherical_v(vel, speed, yaw, pitch);
        pos.x = pos.x + cos_vu(yaw) * r;
        pos.y = pos.y + sin_vu(yaw) * r;
        spawn_particle_4_v(pos, vel, 0x7000c0f0u, 0x70f0u, static_cast<uint32_t>(life), static_cast<uint32_t>(a),
                           static_cast<uint32_t>(b), 1);
    }
}

// orig: 0x00211e30 — turn the hero towards PLAYER+0x180 (spring_angle on the yaw
// PLAYER+0x98, velocity +0x184, rest +0x188); in move modes 1/2 via sub_00211d78.
void sub_00211e30(int32_t dir, float k, float d, float vmax) {
    if (lbu(g::PLAYER + 0x20b3) != 0) {
        sub_00211d78(dir, k, d, vmax);
        return;
    }
    const float target = advanced_turn_target(lwc1(g::PLAYER + 0x180), k, d, vmax);  // enhancement: the modern aim's heading
    swc1(g::PLAYER + 0x188, spring_angle(g::PLAYER + 0x98, g::PLAYER + 0x184, dir, target, k, d, vmax));
}

// orig: 0x00211e98 — turn the hero towards its target heading with the spring constants of
// the current kind (sub_0020d498(0) == 0x10: faster).
void sub_00211e98() {
    const float t = lwc1(0x0015ed64);
    if (sub_0020d498(0) == 0x10)
        sub_00211e30(0, t * u2f(0x3bc49ba6u), t * u2f(0x3d8f5c29u), lwc1(0x0015ed6c) * u2f(0x415f66f3u));
    else
        sub_00211e30(0, t * u2f(0x3b03126fu), t * u2f(0x3d8f5c29u), lwc1(0x0015ed6c) * u2f(0x40df66f3u));
}

// orig: 0x00211f38 — turn the hero with constants scaled by the stick input PLAYER+0x229c
// (x1.1/x1.2 while PLAYER+0x3be is set; fixed slow values for kinds 15/16/0x15 when
// PLAYER+0x20a8 is set).
void sub_00211f38() {
    const float in = (lwc1(g::PLAYER + 0x229c) + 1.0f) * 0.5f;
    const float t = lwc1(0x0015ed64);
    const float a = t * u2f(0x3c9ba5e3u);
    const float b = lwc1(0x0015ed6c) * u2f(0x411f2c8du);
    float d = t * u2f(0x3dcccccdu);
    float k = a * in;
    float vmax = b * in;
    if (lh(g::PLAYER + 0x3be) != 0) {
        k = k * u2f(0x3f8ccccdu);
        vmax = vmax * u2f(0x3f99999au);
    }
    const int32_t kind = sub_0020d498(0);
    if (!(kind < 0xf) && (kind < 0x11 || kind == 0x15)) {
        const float t2 = lwc1(0x0015ed64);
        if (lbu(g::PLAYER + 0x20a8) != 0) {
            k = t2 * u2f(0x3b449ba6u);
            d = t2 * u2f(0x3d23d70au);
            vmax = lwc1(0x0015ed6c) * u2f(0x408ba058u);
        }
    }
    sub_00211e30(0, k, d, vmax);
}

// orig: 0x00213e68 — accelerate the hero's horizontal velocity 0x13f430 towards the stick
// target (yaw PLAYER+0x180, speed +0x190) by at most dt x (1..3, faster when reversing);
// modes 1/2: approach the speed along the local frame instead.
void sub_00213e68(float dt) {
    if (on_batalia()) { bat_callf(bat_sub_213e68_u, dt); return; }  // (level 8's own: L8:0x002256e8)
    if (on_pokitaru()) { pok_callf(pok_sub_213e68_u, dt); return; }  // (level 11's own: L11:0x00243d78)
    if (on_quartu()) { qua_callf(qua_sub_00213e68_u, dt); return; }  // (level 15's own: L15:0x0020bd50)
    if (on_kalebo()) { kal_callf(kal_sub_00213e68_u, dt); return; }  // (level 16's own: L16:0x002117a0)
    if (on_veldin_finale()) { vf_callf(vf_sub_213e68_u, dt); return; }  // (level 18's own: L18:0x0021bac8)
    if (on_l1_family() || on_rilgar() || on_blarg()) return sub_00213e68_l1(dt);
    constexpr uint32_t VEL = 0x0013f430;
    const uint32_t mode = lbu(g::PLAYER + 0x20b3);
    if (mode == 0) {
        const float yaw = lwc1(g::PLAYER + 0x180);
        float speed = lwc1(g::PLAYER + 0x190);
        if (lw(g::PLAYER + 0x208c) == 4 && lh(g::PLAYER + 0x30a) == 0 && lh(g::PLAYER + 0x4a4) != 0 &&
            lbu(g::PLAYER + 0x257) != 0)
            speed = lwc1(0x0015ed6c) * u2f(0x40066666u);
        if (lh(g::PLAYER + 0x30a) != 0 && lw(g::HERO_STATE) == 0xe && lwc1(0x0015ed6c) < speed) speed = lwc1(0x0015ed6c);
        Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
        d.x = cos_vu(yaw) * speed;
        d.y = sin_vu(yaw) * speed;
        d.z = lwc1(g::PLAYER + 0xe8);
        float f = -(lwc1(g::PLAYER + 0xe0) * d.x + lwc1(g::PLAYER + 0xe4) * d.y);
        const float ld = vec2_length(d);
        const float lv = vec2_length(V(g::PLAYER + 0xe0));
        if (ld == 0.0f || lv == 0.0f) f = 0.0f;
        else f = fpu_div(fpu_div(f, ld), lv);
        f = (f + 1.0f) * 1.5f;
        const float lim = lw(g::HERO_STATE) == 0x81 ? u2f(0x3fd9999au) : 3.0f;
        if (lim < f) f = lim;
        if (f < 1.0f) f = 1.0f;
        vec3_sub(d, d, V(VEL));
        const float step = dt * f;
        if (step < vec2_length(d)) vec2_set_length(d, d, step);
        vec3_add(V(VEL), V(VEL), d);
        lq_sq(VEL + 0x50, VEL);
        const Vec4 t = V(VEL + 0x840);
        swc1(g::PLAYER + 0x194, lwc1(g::PLAYER + 0x168) - sub_00213350_v(t));
        return;
    }
    if (!(mode < 3)) return;
    float s = sub_002339d0(VEL);
    approach_f_v(s, lwc1(VEL + 0xb0), dt);
    sub_00233ba0(VEL, VEL, 0.0f);
    sub_00233708(VEL, VEL, s);
}

// orig: 0x00216de8 — play hero sound idx (on the vehicle moby PLAYER+0xa88, moved to the hero
// position, while PLAYER+0x20a4 == 3).
int32_t hero_sound_start(int32_t idx, uint32_t flags) {
    uint32_t m;
    if (lbu(g::PLAYER + 0x20a4) == 3) {
        m = lwu(g::PLAYER + 0xa88);
        if (m == 0) return -1;
        lq_sq(m + 0x10, g::PLAYER + 0x80);
    } else {
        m = lwu(g::PLAYER + 0x2080);
    }
    return moby_sound_start(idx, flags, m);
}

// orig: 0x00216e48 — keep hero sound slot `slot` playing sound idx on `owner`.
void hero_sound_slot(int32_t slot, uint32_t owner, int32_t idx) {
    const uint32_t p = g::PLAYER + 0x2218 + static_cast<uint32_t>(slot << 2);
    if (lw(p) == -1) sw(p, static_cast<uint32_t>(moby_sound_start(idx, 4, owner)));
    sw(g::PLAYER + 0x2238 + static_cast<uint32_t>(slot << 2), owner);
}

// orig: 0x00217368 — sparks on the n wrench-hit mobys of 0x177f00 (class kinds 5 and 9),
// with the hit sound once per swing (PLAYER+0xa64).
void wrench_hit_sparks(uint32_t moby, int32_t n) {
    if (n == 0) return;
    if (lw(g::PLAYER + 0xa64) == 0) {
        moby_sound_start(1, 0, moby);
        sw(g::PLAYER + 0xa64, 1);
    }
    if (n <= 0) return;
    uint32_t p = OV(0x00177f00);
    for (int32_t left = n; left != 0; --left, p += 4) {
        const uint32_t m = lwu(p);
        int32_t kind = 0;
        if (m != 0) {
            const uint32_t c = lwu(m + 0x24);
            if (c != 0) kind = lh(c + 0x46);
        }
        if (kind != 5 && kind != 9) continue;
        Vec4 at = V(lwu(p) + 0x10);
        sub_002336e8_v(at, at, 0.5f);
        spark_pairs_v(at, 4);
    }
}

// orig: 0x00216130 — align the hero matrix (copy of PLAYER, into m) to the up vector `up`:
// measure the tilt, rotate into the tilt plane, apply the tilt (directly into out_ang when
// given, else sprung with velocity 0x13f3f4) and rotate back; the Euler angles go to out_ang
// or to the hero rotation 0x13f3e0.
void align_to_up_v(const Vec4& up, Mat4& m, uint32_t out_ang, float k, float d, float vmax) {
    for (uint32_t r = 0; r < 4; ++r) m.r[r] = M(g::PLAYER).r[r];
    float sprung = 0.0f;
    const float c = vec3_dot(up, m.r[2]);
    const float tilt = atan2_f(c, sqrt_f(1.0f - c * c));
    Vec4 axis;
    vec3_cross_ba(axis, up, m.r[2]);
    Mat4 r1, r2;
    Vec4 a{0.0f, 0.0f, 0.0f, 0.0f};
    float yaw = 0.0f, pitch = 0.0f;
    if (axis.z < u2f(0x3f7d70a4u)) {
        yaw = atan2_f(axis.x, axis.y);
        a.x = 0.0f;
        a.y = 0.0f;
        a.z = -yaw;
        euler_to_mat4_fpu(r1, a);
        mat4_mul(m, r1, m);
        mat4_transform(axis, axis, r1);
        pitch = atan2_f(axis.z, axis.x);
        a.z = 0.0f;
        a.x = 0.0f;
        a.y = -pitch;
        euler_to_mat4_fpu(r2, a);
        mat4_mul(m, r2, m);
        mat4_transform(axis, axis, r2);
    }
    if (out_ang != 0) {
        a.z = tilt;
        a.x = 0.0f;
    } else {
        spring_angle_v(sprung, 0x0013f3f4, 0, tilt, k, d, vmax);
        a.x = 0.0f;
        a.z = sprung;
    }
    a.y = 0.0f;
    euler_to_mat4_fpu(r1, a);
    mat4_mul(m, r1, m);
    a.y = pitch;
    a.z = 0.0f;
    euler_to_mat4_fpu(r2, a);
    mat4_mul(m, r2, m);
    a.z = yaw;
    a.y = 0.0f;
    euler_to_mat4_fpu(r1, a);
    mat4_mul(m, r1, m);
    mat_to_euler_v(m, out_ang != 0 ? out_ang : 0x0013f3e0u);
}
// orig: 0x00216130 (EE-address form)
void align_to_up(uint32_t up, uint32_t m, uint32_t out_ang, float k, float d, float vmax) {
    align_to_up_v(V(up), M(m), out_ang, k, d, vmax);
}

// orig: 0x00212ff0 — keep the hero on the ground: in move modes 1/2 pull the position towards
// the contact point PLAYER+0x2a0 (or snap to it); in mode 0 settle the height PLAYER+0x88 on the
// floor PLAYER+0x2d8 (snap, or spring with velocity PLAYER+0x304).
void sub_00212ff0() {
    if (lbu(g::PLAYER + 0x20b3) != 0) {
        const float into = vec3_dot(V(g::PLAYER + 0xe0), V(g::PLAYER + 0x270));
        if (lh(g::PLAYER + 0x30e) != 0) return;
        const float h = lwc1(g::PLAYER + 0x2dc);
        if (!(h < 0.0f)) return;
        if (h < into + u2f(0x38d1b717u) || lw(g::HERO_STATE) == 0x3e) {
            lq_sq(g::PLAYER + 0x80, g::PLAYER + 0x2a0);
            return;
        }
        Vec4 t;
        vec3_sub(t, V(g::PLAYER + 0x2a0), V(g::PLAYER + 0x80));
        float l = vec3_length(t);
        const float k = lwc1(0x0015ed60);
        if (l < k * u2f(0x3e570a3du)) {
            const float lim = k * u2f(0x3d4ccccdu);
            if (lim < l) l = lim;
        } else {
            l = l * 0.5f;
        }
        vec3_set_length(t, t, l);
        vec3_add(V(0x0013f3d0), V(0x0013f3d0), t);
        return;
    }
    if (lh(g::PLAYER + 0x30e) != 0 || !(lwc1(g::PLAYER + 0x88) < lwc1(g::PLAYER + 0x2d8))) {
        sw(g::PLAYER + 0x304, 0);
        return;
    }
    const float gap = abs_f(lwc1(g::PLAYER + 0x88) - lwc1(g::PLAYER + 0x2d8));
    if (gap < abs_f(lwc1(g::PLAYER + 0xe8)) + u2f(0x3c23d70au)) {
        bool snap = lbu(g::PLAYER + 0x257) == 0;
        if (!snap) snap = lwc1(g::PLAYER + 0x234) * 0.5f < vec2_distance(V(g::PLAYER + 0x210), V(g::PLAYER + 0x80));
        if (snap) {
            swc1(g::PLAYER + 0x88, lwc1(g::PLAYER + 0x2d8));
            return;
        }
    }
    // the spring rate is the per-frame scale gp-0x7e9c, except after falling (velocity +0x108 < 0)
    float t = lwc1(OV(0x00166c00u - 0x7e9c));
    if (!(gap < abs_f(lwc1(g::PLAYER + 0xe8)) + u2f(0x3c23d70au))) {
        const float v = lwc1(g::PLAYER + 0x108);
        if (v < 0.0f) {
            const float n = lwc1(g::PLAYER + 0x88) - v;
            swc1(g::PLAYER + 0x88, n);
            if (lwc1(g::PLAYER + 0x2d8) < n) swc1(g::PLAYER + 0x88, lwc1(g::PLAYER + 0x2d8));
            t = lwc1(0x0015ed64);
        }
    }
    const float vm = lwc1(0x0015ed6c);
    spring_to(g::PLAYER + 0x88, g::PLAYER + 0x304, lwc1(g::PLAYER + 0x2d8), t * u2f(0x3d6978d5u), t * u2f(0x3e99999au), vm + vm);
}

// orig: 0x002163f0 — tilt the hero to its ground normal (PLAYER+0x270, or -0x290 in move mode
// 2) with align_to_up; in move mode 1 on a slope keep the point 0.8 above the origin fixed.
void sub_002163f0() {
    float k = lwc1(0x0015ed64) * u2f(0x3e99999au);
    Vec4 up{0.0f, 0.0f, 1.0f, 0.0f}, top{0.0f, 0.0f, 0.0f, 0.0f}, top2{0.0f, 0.0f, 0.0f, 0.0f};
    top2.z = u2f(0x3f4ccccdu);
    top.z = u2f(0x3f4ccccdu);
    mat4_transform(top, top, M(g::PLAYER));
    up = V(g::PLAYER + 0x270);
    if (lbu(g::PLAYER + 0x20b3) == 2) vec3_scale(up, V(g::PLAYER + 0x290), -1.0f);
    if (lbu(g::PLAYER + 0x20b3) == 1) {
        if (lh(g::PLAYER + 0x30e) < frame_scale_i(10) || lwc1(g::PLAYER + 0x2dc) < 1.0f) up = V(g::PLAYER + 0x270);
    }
    if (lh(g::PLAYER + 0x30e) != 0 && lh(g::PLAYER + 0x1f8) != 0) k = lwc1(0x0015ed64) * u2f(0x3a83126fu);
    Mat4 m;
    align_to_up_v(up, m, 0, k, k, 0.0f);
    if (lbu(g::PLAYER + 0x20b3) != 1) return;
    if (lh(g::PLAYER + 0x30e) == 0) return;
    mat4_transform(top2, top2, m);
    vec3_sub(V(g::PLAYER + 0x80), V(g::PLAYER + 0x80), top2);
    vec3_add(V(g::PLAYER + 0x80), V(g::PLAYER + 0x80), top);
}

// orig: 0x00216f10 — tick the 8 delayed hero sounds (PLAYER+0x21d8, see sub_00216ec0): when
// an entry's timer (+4) expires, clear it and play sound +2 with flags +6.
void hero_delayed_sounds() {
    uint32_t e = g::PLAYER + 0x21d8;
    for (int32_t k = 7; k >= 0; --k, e += 8) {
        if (timer_tick_i16(ee_ref<int16_t>(e + 4)) != 2) continue;
        const int32_t idx = lh(e + 2);
        const int32_t flags = lh(e + 6);
        sh(e + 0, 0);
        hero_sound_start(idx, static_cast<uint32_t>(flags));
    }
}

// orig: 0x002165b8 — per-frame hero tilt: on the ground (move mode != 0, or PLAYER+0x1f8 set)
// align to the ground normal (sub_002163f0, modes 1..2 only); otherwise, unless the state
// forbids it, spring the pitch/roll PLAYER+0x90/0x94 back to zero.
void hero_tilt_update() {
    int32_t mode = lbu(g::PLAYER + 0x20b3);
    if (lh(g::PLAYER + 0x1f8) != 0) mode = 1;
    if (mode != 0) {
        if (mode < 3) sub_002163f0();
        return;
    }
    const int32_t st = lw(g::HERO_STATE);
    if (static_cast<uint32_t>(st - 0x25) < 2u) return;
    const int32_t sub = lw(g::PLAYER + 0x208c);
    if (sub == 0x11 || sub == 0x15 || sub == 0x16) return;
    if (st == 0x77 || st == 0x31 || st == 0x2d || st == 0x2c) return;
    const float zero = -0.0f;
    float t = lwc1(0x0015ed64);
    float vm = lwc1(0x0015ed6c);
    spring_angle(g::PLAYER + 0x90, g::PLAYER + 0xa0, 0, zero, t * u2f(0x3c75c28fu), t * u2f(0x3e99999au), vm * u2f(0x40a78d36u));
    t = lwc1(0x0015ed64);
    vm = lwc1(0x0015ed6c);
    spring_angle(g::PLAYER + 0x94, g::PLAYER + 0xa4, 0, zero, t * u2f(0x3c75c28fu), t * u2f(0x3e99999au), vm * u2f(0x40a78d36u));
}

// orig: 0x00211380 — pick the hero's gait targets (PLAYER+0x228/0x22c/0x230) from the
// equipped boots/mode PLAYER+0x20a4 and the current state, then ease the live values
// PLAYER+0x220/0x224 towards them and spring the height PLAYER+0x234 (unless the hero is
// standing on a moving platform far from its anchor).
void hero_gait_update() {
    if (on_blarg()) { blg_call(blg_hero_gait_update_u); return; }  // (level 6's own: L6:0x00228360)
    if (on_batalia()) { bat_call(bat_hero_gait_update_u); return; }  // (level 8's own: L8:0x00222d80)
    if (on_pokitaru()) { pok_call(pok_hero_gait_update_u); return; }  // (level 11's own: L11:0x002414e8)
    if (on_hoven()) { hov_call(hov_hero_gait_update_u); return; }  // (level 12's own: L12:0x00232b18)
    if (on_oltanis()) { olt_call(olt_hero_gait_update_u); return; }  // (level 14's own: L14:0x00222f38)
    if (on_quartu()) { qua_call(qua_hero_gait_update_u); return; }  // (level 15's own: L15:0x00209450)
    if (on_kalebo()) { kal_call(kal_hero_gait_update_u); return; }  // (level 16's own: L16:0x0020ec10)
    if (on_veldin_finale()) { vf_call(vf_hero_gait_update_u); return; }  // (level 18's own: L18:0x002190f0)
    if (on_veldin_orbit()) { vo_call(vo_hero_gait_update_u); return; }  // (level 17's own: L17:0x0020e1c0)
    if (on_l1_family() || on_rilgar()) return hero_gait_update_l1();
    const uint32_t P = g::PLAYER;
    switch (lbu(P + 0x20a4)) {
    case 0:
        swc1(P + 0x228, u2f(0x3f4ccccdu)); swc1(P + 0x230, u2f(0x3ee66666u)); swc1(P + 0x22c, u2f(0x3f333333u));
        break;
    case 1:
        swc1(P + 0x228, u2f(0x3f199999u)); swc1(P + 0x230, u2f(0x3e99999au)); swc1(P + 0x22c, u2f(0x3ee66667u));
        break;
    case 2:
        swc1(P + 0x228, u2f(0x40a80000u)); swc1(P + 0x230, u2f(0x40700000u)); swc1(P + 0x22c, u2f(0x408e6666u));
        break;
    case 3:
        swc1(P + 0x228, u2f(0x3f4ccccdu)); swc1(P + 0x230, u2f(0x3ee66666u)); swc1(P + 0x22c, u2f(0x3f19999au));
        break;
    default:
        break;
    }
    const int32_t sub = lw(P + 0x208c);
    if (sub == 4) {
        if (lw(P + 0x420) < lw(P + 0x198) && lh(P + 0x41e) == 0) swc1(P + 0x22c, lwc1(P + 0x434));
    } else {
        const int32_t st = lw(g::HERO_STATE);
        if (st == 6) {
            swc1(P + 0x22c, 0.5f);
        } else if (st == 4) {
            swc1(P + 0x228, u2f(0x3eb33334u));
        } else if (static_cast<uint32_t>(sub - 0x11) < 2u) {
            sw(P + 0x22c, 0);
            sw(P + 0x228, 0);
        } else if (lbu(P + 0x12e2) != 0 && lw(P + 0x300) != 0) {
            swc1(P + 0x228, u2f(0x3f4ccccdu));
            swc1(P + 0x22c, u2f(0x3f666666u));
        } else if (lw(g::HERO_STATE) == 0x7f) {
            swc1(P + 0x230, u2f(0x3f4ccccdu));
        }
    }
    if (lbu(P + 0x257) != 0 && lw(P + 0x2094) != 0x12 && lw(P + 0x208c) != 0x11 && lbu(P + 0x12e4) == 0) {
        const float d = vec2_distance(V(P + 0x210), V(P + 0x80));
        if (!(lwc1(P + 0x234) * 0.5f < d)) return;
    }
    const float r = u2f(0x3ca3d70au);
    approach_f(P + 0x220, lwc1(P + 0x228), lwc1(0x0015ed60) * r);
    approach_f(P + 0x224, lwc1(P + 0x22c), lwc1(0x0015ed60) * r);
    const float t = lwc1(0x0015ed64);
    spring_to(P + 0x234, P + 0x238, lwc1(P + 0x230), t * r, t * u2f(0x3e99999au), lwc1(0x0015ed6c) * 4.0f);
}

// orig: 0x00215b68 — body lean from the turn rate PLAYER+0x188: sets the spine/head bone
// offsets in 0x17a680 (+0x10e0/+0x10e8/+0x1190/+0x1194/+0x1198/+0x1240) for states 0x44
// and 0x4f, and for sub-states 2/4.
void hero_body_lean() {
    constexpr OvConst<0x0017a680u> B{};
    const uint32_t P = g::PLAYER;
    const int32_t st = lw(g::HERO_STATE);
    if (st == 0x44) {
        float t = lwc1(P + 0x188) * 0.5f;
        if (u2f(0x3f333333u) < t) {
            t = u2f(0x3f333333u);
        } else if (t < u2f(0xbf333333u)) {
            t = u2f(0xbf333333u);
        }
        const float a = abs_f(t);
        const float k = lwc1(0x0015ed6c) * 3.0f;
        t = (t * a) * 1.25f;
        float f = fpu_div(lwc1(P + 0x164), k);
        const float n = -t;
        const float lean = n * u2f(0x3e99999au);
        swc1(B + 0x1240, lean);
        if (1.0f < f) f = 1.0f;
        if (f < 0.0f) f = 0.0f;
        swc1(B + 0x10e8, t);
        swc1(B + 0x1240, lean * f);
        swc1(B + 0x10e0, n * u2f(0x3eb33333u));
        swc1(B + 0x1198, t * u2f(0x3faccccdu));
        swc1(B + 0x1194, abs_f(t) * 0.25f);
        return;
    }
    if (st == 0x4f) {
        float t = lwc1(P + 0x188) * u2f(0x3fcccccdu);
        if (1.25f < t) {
            t = 1.25f;
        } else if (t < -1.25f) {
            t = -1.25f;
        }
        const float lean = -t * u2f(0x3f051eb8u);
        swc1(B + 0x1240, lean);
        if (u2f(0x3e8f5c29u) < lean) {
            swc1(B + 0x1240, u2f(0x3e8f5c29u));
        } else if (lean < u2f(0xbe8f5c29u)) {
            swc1(B + 0x1240, u2f(0xbe8f5c29u));
        }
        const float h = t * u2f(0x3fd9999au);
        swc1(B + 0x1198, h);
        swc1(B + 0x10e8, t * u2f(0x3f147ae1u));
        if (u2f(0x3f333333u) < h) {
            swc1(B + 0x1198, u2f(0x3f333333u));
        } else if (h < u2f(0xbf333333u)) {
            swc1(B + 0x1198, u2f(0xbf333333u));
        }
        const float r = abs_f(t) * u2f(0x3e4ccccdu);
        swc1(B + 0x1194, r);
        if (u2f(0x3e4ccccdu) < r) {
            swc1(B + 0x1194, u2f(0x3e4ccccdu));
        } else if (r < u2f(0xbe4ccccdu)) {
            swc1(B + 0x1194, u2f(0xbe4ccccdu));
        }
        swc1(B + 0x1190, abs_f(t) * u2f(0x3e23d70au));
        return;
    }
    const int32_t sub = lw(P + 0x208c);
    if (sub != 2 && sub != 4) return;
    float t = lwc1(P + 0x188);
    if (u2f(0x3fb33333u) < t) {
        t = u2f(0x3fb33333u);
    } else if (t < u2f(0xbfb33333u)) {
        t = u2f(0xbfb33333u);
    }
    swc1(B + 0x10e8, t * u2f(0x3e947ae1u));
    swc1(B + 0x1198, t * u2f(0x3f5eb852u));
}

// orig: 0x00214fe8 — while PLAYER+0x308 is set, aim the hero's head/arm bones (offsets in
// 0x17a680) along the view direction: d = -z axis of the matrix PLAYER+0x40, its x/y turned
// into clamped bone angles (limit depends on the state and on PLAYER+0x2fc), and refresh the
// effect parameters (fx_params_set_1).
void hero_aim_bones() {
    constexpr OvConst<0x0017a680u> B{};
    const uint32_t P = g::PLAYER;
    if (lh(P + 0x308) == 0) return;
    Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
    d.z = -1.0f;
    mat4_transform(d, d, M(P + 0x40));
    const float t = lwc1(0x0015ed64);
    fx_params_set_1(t * u2f(0x3c54fdf4u), t * u2f(0x3e99999au));
    const int32_t st = lw(g::HERO_STATE);
    float lim = u2f(0x3edf66f3u);
    if (st == 0x3f) {
        lim = u2f(0x3e97e9d8u);
    } else if (st == 0x70) {
        lim = u2f(0x3db2b8c2u);
    } else if (st == 0x71 || st == 4) {
        lim = u2f(0x3e449809u);
    }
    if (lw(P + 0x2fc) != 0 && u2f(0x3e20d97cu) < lim) lim = u2f(0x3e20d97cu);
    float yaw = d.x * u2f(0x3f32b8c2u);
    if (lim < yaw) {
        yaw = lim;
    } else if (yaw < -lim) {
        yaw = -lim;
    }
    float pitch = -d.y * u2f(0x3f32b8c2u);
    swc1(B + 0x114, yaw);
    swc1(B + 0x274, -yaw * 0.5f);
    if (lim < pitch) {
        pitch = lim;
    } else if (pitch < -lim) {
        pitch = -lim;
    }
    const float k = u2f(0x3f9c61aau);
    float arm = -d.x * k;
    swc1(B + 0x110, pitch);
    swc1(B + 0x270, -pitch * 0.25f);
    if (u2f(0x3f7eadaeu) < arm) arm = u2f(0x3f7eadaeu);
    if (arm < u2f(0xbdfa35ddu)) arm = u2f(0xbdfa35ddu);
    if (u2f(0x3f59999au) < d.z) {
        if (arm < u2f(0x3fb2b8c2u)) arm = u2f(0x3fb2b8c2u);
    } else if (0.5f < d.z) {
        if (arm < k) arm = k;
    }
    float roll = d.y * k;
    const float half = arm * 0.5f;
    swc1(B + 0x954, arm * u2f(0x3f11eb85u));
    swc1(B + 0xab4, half);
    swc1(B + 0xb64, arm * u2f(0x3eb33333u));
    swc1(B + 0xa04, half);
    if (k < roll) {
        roll = k;
    } else if (roll < -k) {
        roll = -k;
    }
    swc1(B + 0xb60, roll);
    const float r7 = roll * u2f(0x3f333333u);
    swc1(B + 0xab0, r7);
    swc1(B + 0x950, r7);
    swc1(B + 0xa00, r7);
}

// orig: 0x002118c8 — left stick (PLAYER+0x1d20/0x1d24) to move direction: mode 1 snaps near-axis
// input; length clamped to 1. No input: speed +0x190 = 0, heading +0x180 kept, direction +0x170 =
// hero x axis. Else heading = stick angle (mode 2/3: 4/8 directions) + hero yaw (mode 4) or camera
// yaw, direction = stick through the camera matrix 0x166fd0, speed = scale * its length.
void hero_stick_move(int32_t mode, float scale) {
    const uint32_t P = g::PLAYER;
    Vec4 s{0.0f, 0.0f, 0.0f, 0.0f};  // z/w: uninitialised stack lanes in the original
    s.y = lwc1(P + 0x1d24);
    s.x = lwc1(P + 0x1d20);
    if (mode == 1) {
        if (u2f(0x3f333333u) < abs_f(s.y) && abs_f(s.x) < u2f(0x3df5c28fu)) s.x = 0.0f;
        if (u2f(0x3f333333u) < abs_f(s.x) && abs_f(s.y) < u2f(0x3df5c28fu)) s.y = 0.0f;
    }
    const float len = vec2_length(s);
    if (1.0f < len) vec3_div(s, s, len);
    if (s.x == 0.0f && s.y == 0.0f) {
        swc1(P + 0x190, 0.0f);
        if (lbu(P + 0x20b3) != 0) {
            swc1(P + 0x180, sub_002332d0());
        } else {
            swc1(P + 0x180, lwc1(P + 0x98));
        }
        Vec4 ax{0.0f, 0.0f, 0.0f, 0.0f};
        ax.x = 1.0f;
        mat3_transform(V(P + 0x170), ax, M(static_cast<uint32_t>(lw(g::HERO_MOBY)) + 0xc0));
        return;
    }
    swc1(P + 0x180, atan2_f(-s.y, -s.x));
    constexpr float PI = 3.14159274f;  // 0x40490fdb
    if (mode == 2) {
        const int32_t q = float_to_int((lwc1(P + 0x180) + PI) * u2f(0x3f22f987u) + 0.5f);
        swc1(P + 0x180, int_to_float(q & 3) * u2f(0x3fc90fdbu) - PI);
    }
    if (mode == 3) {
        const int32_t q = float_to_int((lwc1(P + 0x180) + PI) * u2f(0x3fa2f987u) + 0.5f);
        swc1(P + 0x180, int_to_float(q & 7) * u2f(0x3f490fd8u) - PI);
    }
    swc1(P + 0x180, angle_add(lwc1(P + 0x180), mode == 4 ? lwc1(P + 0x98) : lwc1(OV(0x00166dd8))));
    const float x = s.x, y = s.y;
    s.y = -x;
    s.x = -y;
    s.z = 0.0f;
    s.w = 0.0f;
    if (lbu(0x0015edb4) != 0) s.y = x;
    mat3_transform(V(P + 0x170), s, M(OV(0x00166fd0)));
    swc1(P + 0x190, scale * vec2_length(s));
}

void hero_stick_speed_orig();
// orig: 0x00211be8 — the original's body below, then (enhancement) the speed reduced
// while the modern controls aim
void hero_stick_speed() {
    if (on_blarg()) { blg_call(blg_hero_stick_speed_u); return; }  // (level 6's own: L6:0x00228b40)
    if (on_pokitaru()) { pok_call(pok_hero_stick_speed_u); return; }  // (level 11's own: L11:0x00241c98)
    if (on_oltanis()) { olt_call(olt_hero_stick_speed_u); return; }  // (level 14's own: L14:0x00223710)
    if (on_quartu()) { qua_call(qua_hero_stick_speed_u); return; }  // (level 15's own: L15:0x00209c40)
    hero_stick_speed_orig();
    advanced_move_speed();
}
// orig: 0x00211be8 — stick move (hero_stick_move mode 0, scale 1) with the speed PLAYER+0x190
// mapped per state: state 0x3f: max(3.5 vm * stick, 2 vm); otherwise the walk/run speed of the
// table 0x17bdb8 (+8 below +0xc, else +0x18) times vm, and in state 0x73 max(0.8 x, 2.5 vm).
void hero_stick_speed_orig() {
    if (on_l1_family() || on_rilgar()) return hero_stick_speed_l1();
    hero_stick_move(0, 1.0f);
    const uint32_t P = g::PLAYER;
    if (lw(g::HERO_STATE) == 0x3f) {
        const float vm = lwc1(0x0015ed6c);
        const float lo = vm + vm;
        const float v = vm * 3.5f * lwc1(P + 0x190);
        swc1(P + 0x190, v < lo ? lo : v);
        return;
    }
    const float s = lwc1(P + 0x190);
    if (0.0f < s) {
        constexpr OvConst<0x0017bdb8u> T{};
        const float v = s < lwc1(T + 0xc) ? lwc1(T + 8) : lwc1(T + 0x18);
        swc1(P + 0x190, v * lwc1(0x0015ed6c));
    }
    if (lw(g::HERO_STATE) != 0x73) return;
    const float v = lwc1(P + 0x190) * u2f(0x3f4ccccdu);
    const float lo = lwc1(0x0015ed6c) * 2.5f;
    swc1(P + 0x190, v < lo ? lo : v);
}

// orig: 0x00214ed8 — lock-on heading: unless already locked (PLAYER+0xa58), take the facing (or,
// with the stick held, +0x229c > 0.5, the stick heading and lock), pick the best target in that
// direction (find_best_target) and store the heading towards it (+0xa5c, target in +0xa54);
// without a target keep the facing.
void hero_lock_heading(float range, float b, float c) {
    const uint32_t P = g::PLAYER;
    if (lw(P + 0xa58) != 0) return;
    float yaw = lwc1(P + 0x98);
    if (0.5f < lwc1(P + 0x229c)) {
        hero_stick_move(0, 1.0f);
        sw(P + 0xa58, 1);
        sw(P + 0xa54, 0);
        yaw = lwc1(P + 0x180);
    }
    const uint32_t t = find_best_target(range, yaw, b, c);
    if (t == 0 || (lw(P + 0xa54) != 0 && lw(P + 0xa58) == 0)) {
        swc1(P + 0xa5c, yaw);
        return;
    }
    const float a = atan2_f(lwc1(t + 0x10) - lwc1(P + 0x80), lwc1(t + 0x14) - lwc1(P + 0x84));
    sw(P + 0xa54, t);
    swc1(P + 0xa5c, a);
}

// orig: 0x00214108 — hero vertical velocity (PLAYER+0xe0 along the up axis): state 0xe (jump):
// boost up to 7 vm, cut by the height gained (+0x80 - +0x400) and the heli/thruster pack, for 8
// frames; else the charged-jump impulse +0x428/+0x42c/+0x430 and the scripted velocity curve
// +0x3d0 (table +0x3e4, step +0x3e8/+0x3ec, value +0x3f0). Then gravity: dropped to -48 vm while
// +0x198 < +0x420, else gravity +0x4a0 with the fall speed clamped (>= -50 vm, >= ground vel - 0.1).
void hero_vertical_velocity() {
    const uint32_t P = g::PLAYER;
    const uint32_t VEL = P + 0xe0;
    if (lw(g::HERO_STATE) == 0xe) {
        float boost = lwc1(0x0015ed6c) * 7.0f;
        bool pack = false;
        if (lbu(0x0013d4c2) != 0 && lh(P + 0x22d8) == 0) pack = sub_0020d498(3) == 3;
        Vec4 d;
        vec3_sub(d, V(P + 0x80), V(P + 0x400));
        const float h = sub_00233a78_v(d);
        if (pack) {
            if (u2f(0x402ccccdu) < h) {
                boost = boost * u2f(0x3ef0a3d7u);
            } else if (u2f(0x40066666u) < h) {
                boost = boost * u2f(0x3f333333u);
            }
        } else {
            if (u2f(0x40066666u) < h) {
                boost = boost * u2f(0x3ee66666u);
            } else if (u2f(0x3fd9999au) < h) {
                boost = boost * u2f(0x3f333333u);
            }
        }
        // (enhancement, R&C 3 gives every double jump the x1.25 kick R&C1 keeps for the pack of kind 3)
        if ((sub_0020d498(3) == 3 || advanced_rac3_movement()) && lh(P + 0x22d8) == 0) boost = boost * 1.25f;
        if (lw(P + 0x198) < frame_scale_i(8)) {
            float up = sub_00233a78(VEL);
            if (up < boost) approach_f_v(up, boost, lwc1(0x0015ed70) * 150.0f);
            sub_00233b20(VEL, VEL, up);
        }
    } else {
        const bool idle = lwc1(P + 0x428) == 0.0f && lwc1(P + 0x42c) == 0.0f;
        bool charge = (lw(0x0013cae0) & 0x40) != 0 || idle || lw(g::HERO_STATE) == 0x12;
        if (charge) charge = lwc1(P + 0x430) < lwc1(P + 0x48c);
        // (enhancement, with R&C 3 the charge lasts the state's ramp +0x498, 17 frames, not 15)
        if (charge) charge = !((advanced_rac3_movement() ? static_cast<int32_t>(lh(P + 0x498)) : frame_scale_i(0xf)) < lw(P + 0x198));
        if (charge) {
            const float top = lwc1(P + 0x48c);
            const float inc = fpu_div(top - lwc1(P + 0x488), static_cast<float>(lh(P + 0x498)));
            const float c = lwc1(P + 0x430) + inc;
            swc1(P + 0x430, c);
            if (top < c) swc1(P + 0x430, top);
            const float c2 = lwc1(P + 0x430);
            const float v = sqrt_f((c2 + c2) * lwc1(P + 0x4a0));
            const float a = lwc1(P + 0x428);
            swc1(P + 0x428, a + ((v - lwc1(P + 0x42c)) - a));
        }
        if (!(lw(P + 0x198) < lw(P + 0x420)) && 0.0f < lwc1(P + 0x428)) {
            sub_00233810(VEL, VEL, lwc1(P + 0x428));
            const float b = lwc1(P + 0x42c);
            const float a = lwc1(P + 0x428);
            swc1(P + 0x428, 0.0f);
            swc1(P + 0x42c, b + a);
        }
        const int32_t t = lw(P + 0x198);
        if (lh(P + 0x41c) != 0 && !(t < lw(P + 0x3d0)) && t < lw(P + 0x3d4)) {
            const uint32_t S = P + 0x3d0;
            sw(S + 0x1c, static_cast<uint32_t>(lw(S + 0x1c) + 1));
            for (;;) {
                const int32_t idx = lw(S + 0x18);
                if (idx != -1 && lw(S + 0x1c) < frame_scale_i(lw(static_cast<uint32_t>(lw(S + 0x14)) + static_cast<uint32_t>(idx) * 12 + 8))) break;
                const int32_t n = lw(S + 0x18) + 1;
                sw(S + 0x1c, 0);
                sw(S + 0x18, static_cast<uint32_t>(n));
                const float e = lwc1(static_cast<uint32_t>(lw(S + 0x14)) + static_cast<uint32_t>(n) * 12);
                if (!(e == u2f(0xc97423f0u))) swc1(S + 0x20, e * lwc1(0x0015ed70));
            }
            if (lw(S + 0x1c) > 0) {
                const float k = lwc1(OV(0x00166c00u - 0x7e90));
                const float e = lwc1(static_cast<uint32_t>(lw(S + 0x14)) + static_cast<uint32_t>(lw(S + 0x18)) * 12 + 4);
                swc1(S + 0x20, lwc1(S + 0x20) + e * k);
            }
            sub_00233810(VEL, VEL, lwc1(S + 0x20));
        }
    }
    const int32_t st = lw(g::HERO_STATE);
    if ((st == 0x1c || st == 0x4c) && lw(P + 0x198) < frame_scale_i(10)) return;
    if (lw(P + 0x198) < lw(P + 0x420)) {
        sub_00233b20(VEL, VEL, 0.0f);
        sub_002334d0(VEL, VEL, lwc1(0x0015ed70) * 48.0f);
        return;
    }
    sub_002334d0(VEL, VEL, lwc1(P + 0x4a0));
    float up = sub_00233a78(VEL);
    const float floor_up = sub_00233a78(P + 0x110) - u2f(0x3dcccccdu);
    if (up < floor_up) up = floor_up;
    const float vmax = -(lwc1(0x0015ed6c) * 50.0f);
    if (up < vmax) up = vmax;
    sub_00233b20(VEL, VEL, up);
}

// orig: 0x00214658 — hero horizontal velocity PLAYER+0xe0 per state: 0xa (frozen), 0x10
// (gravity ramp, speed +0x3f4), 0x1c/0x4c (anim-timed dash +0x45c along the facing), 0xb/0xc
// (wall jump: +0x458 along +0x438 plus +0x454 sideways), 0x11 (dive along +0x470), 0x2d
// (turn in place), else ground steering; then velocity damping (+0x150 = +0x920) and drag.
void hero_horizontal_velocity() {
    if (on_blarg()) { blg_call(blg_hero_horizontal_velocity_u); return; }  // (level 6's own: L6:0x0022b438)
    if (on_batalia()) { bat_call(bat_hero_horizontal_velocity_u); return; }  // (level 8's own: L8:0x00225ed0)
    if (on_pokitaru()) { pok_call(pok_hero_horizontal_velocity_u); return; }  // (level 11's own: L11:0x00244560)
    if (on_hoven()) { hov_call(hov_hero_horizontal_velocity_u); return; }  // (level 12's own: L12:0x00235c08)
    if (on_oltanis()) { olt_call(olt_hero_horizontal_velocity_u); return; }  // (level 14's own: L14:0x00226020)
    if (on_quartu()) { qua_call(qua_hero_horizontal_velocity_u); return; }  // (level 15's own: L15:0x0020c538)
    if (on_kalebo()) { kal_call(kal_hero_horizontal_velocity_u); return; }  // (level 16's own: L16:0x00211f88)
    if (on_veldin_finale()) { vf_call(vf_hero_horizontal_velocity_u); return; }  // (level 18's own: L18:0x0021c2b0)
    if (on_rilgar()) { ril_call(ril_hero_horizontal_velocity_u); return; }  // (Rilgar's own: L5:0x00242258)
    if (on_l1_family()) return hero_horizontal_velocity_l1();
    const uint32_t P = g::PLAYER;
    const uint32_t VEL = P + 0xe0;
    const int32_t st = lw(g::HERO_STATE);
    if (st == 0xa) {
        sub_002132b8(u2f(0x3f333333u), lwc1(0x0015ed6c) * 0.0f);
        lq_sq(P + 0x150, P + 0x920);
        swc1(P + 0x194, lwc1(P + 0x164));
        return;
    }
    if (st == 0x10) {
        if (frame_scale_i(0x36) < lw(P + 0x198)) {
            const float k = lwc1(OV(0x00166c00u - 0x7e90));
            approach_f(P + 0x4a0, k * 25.0f, k * 7.0f);
        }
        if (lw(P + 0x198) < frame_scale_i(0x37)) {
            swc1(P + 0x190, lwc1(P + 0x3f4));
        } else {
            swc1(P + 0x190, lwc1(OV(0x00166c00u - 0x7e94)) * 3.5f);
        }
        const float k = lwc1(0x0015ed70);
        sub_00212088(k * 44.0f, k * 45.0f);
        sub_00233ba0(VEL, VEL, 0.0f);
        sub_00233708(VEL, VEL, lwc1(P + 0x194));
        return;
    }
    if (st == 0x1c || st == 0x4c) {
        float lo, hi, stop, go;
        if (st == 0x1c) {
            const float t = lwc1(0x0015ed64);
            if (frame_scale_i(0x28) < lw(P + 0x198)) {
                sub_00211e30(0, t * u2f(0x3d23d70au), t * u2f(0x3e4ccccdu), lwc1(0x0015ed6c) * u2f(0x41702845u));
                sub_00213e68(lwc1(0x0015ed70) * 20.0f);
            }
            lo = 13.0f; hi = 30.0f; stop = 27.0f; go = 0.0f;
        } else {
            lo = 5.0f; hi = 18.0f; stop = 15.0f; go = 1.0f;
        }
        const float at = lwc1(P + 0xaa8);
        if (lo <= at && at <= hi) {
            const float k = lwc1(OV(0x00166c00u - 0x7e90));
            if (stop < at) {
                approach_f(P + 0x45c, 0.0f, k * 8.0f);
            } else if (go == 0.0f) {
                const float vm = lwc1(0x0015ed6c);
                approach_f(P + 0x45c, vm + vm, k * 8.0f);
            } else {
                approach_f(P + 0x45c, lwc1(0x0015ed6c) * 1.25f, k * 8.0f);
            }
        }
        swc1(P + 0xe0, cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x45c));
        swc1(P + 0xe4, sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x45c));
        return;
    }
    if (static_cast<uint32_t>(st - 0xb) < 2u) {
        const float t = lwc1(OV(0x00166c00u - 0x7e9c));
        const float vm = lwc1(0x0015ed6c);
        swc1(P + 0x180, lwc1(P + 0x43c));
        sub_00211e30(0, t * u2f(0x3d23d70au), t * u2f(0x3e4ccccdu), vm * u2f(0x40df66f3u));
        if (lw(P + 0x450) == 3) {
            sw(P + 0x194, 0);
        } else {
            swc1(P + 0x194, lwc1(P + 0x454));
        }
        if (advanced_strafe_flip_active()) {  // enhancement: R&C 3's strafe flip speed
            approach_f(P + 0x458, advanced_strafe_flip_speed(), lwc1(0x0015ed70) * 47.0f);
        } else if (lw(g::HERO_STATE) == 0xb) {
            if (lh(P + 0x41e) != 0 && lw(P + 0x44c) < frame_scale_i(0xc)) {
                approach_f(P + 0x458, 0.0f, lwc1(OV(0x00166c00u - 0x7e90)) * 8.0f);
            } else {
                approach_f(P + 0x458, lwc1(0x0015ed6c) * u2f(0x40866666u), lwc1(0x0015ed70) * 8.0f);
            }
        }
        swc1(P + 0xe0, cos_vu(lwc1(P + 0x438)) * lwc1(P + 0x458));
        swc1(P + 0xe4, sin_vu(lwc1(P + 0x438)) * lwc1(P + 0x458));
        const float side = lwc1(P + 0x454) * sin_vu(angle_dist(lwc1(P + 0x43c), lwc1(P + 0x438)));
        swc1(P + 0xe0, lwc1(P + 0xe0) + cos_vu(lwc1(P + 0x43c)) * side);
        swc1(P + 0xe4, lwc1(P + 0xe4) + sin_vu(lwc1(P + 0x43c)) * side);
        sub_002132b8(u2f(0x3f333333u), lwc1(0x0015ed6c) * 0.0f);
        lq_sq(P + 0x150, P + 0x920);
        return;
    }
    if (st == 0x11) {
        swc1(P + 0x180, lwc1(P + 0x47c));
        lq_sq(P + 0x170, P + 0x470);
        const float t = lwc1(OV(0x00166c00u - 0x7e9c));
        if (frame_scale_i(9) < lw(P + 0x198)) {
            sub_00211e30(-1, t * u2f(0x3d2c0831u), t * u2f(0x3e4ccccdu), lwc1(0x0015ed6c) * u2f(0x413de44eu));
        }
        if (!(lw(P + 0x198) < frame_scale_i(9))) {
            const float k = u2f(0x409ccccdu);
            if (lh(P + 0x308) == 0) {
                swc1(P + 0xe0, cos_vu(lwc1(P + 0x180)) * (lwc1(0x0015ed6c) * k));
                swc1(P + 0xe4, sin_vu(lwc1(P + 0x180)) * (lwc1(0x0015ed6c) * k));
            } else {
                sub_00233ba0(VEL, VEL, 0.0f);
                Vec4 d;
                vec3_set_length(d, V(P + 0x170), lwc1(0x0015ed6c) * k);
                vec3_add(V(VEL), V(VEL), d);
            }
        }
        swc1(P + 0x194, lwc1(P + 0x164));
        return;
    }
    if (st == 0x2d) {
        const float t = lwc1(0x0015ed64);
        const float vm = lwc1(0x0015ed6c);
        float yaw = lwc1(P + 0x98);
        swc1(P + 0x188, spring_angle_v(yaw, P + 0x184, 0, lwc1(P + 0x180), t * u2f(0x3c9374bcu), t * u2f(0x3e4ccccdu), vm * u2f(0x40ea927fu)));
        sub_002091d8(0.0f, 0.0f, angle_sub(yaw, lwc1(P + 0x98)));
    } else {
        const float t = lwc1(OV(0x00166c00u - 0x7e9c));
        sub_00211e30(0, t * u2f(0x3d23d70au), t * u2f(0x3e4ccccdu), lwc1(P + 0x414));
    }
    sub_002132b8(u2f(0x3f333333u), lwc1(0x0015ed6c) * 0.0f);
    lq_sq(P + 0x150, P + 0x920);
    const float slow = lwc1(OV(0x0017bdd0)) * lwc1(0x0015ed6c) * u2f(0x3e4ccccdu);
    const float k = lwc1(0x0015ed70);
    float drag = k * 20.0f;
    if (lwc1(P + 0x190) < slow) {
        if (lw(P + 0x208c) == 2) {
            float rate = lwc1(P + 0x944);
            if (lbu(P + 0x12e2) != 0) rate = k * u2f(0x3e2e147bu);
            float sp = vec2_length(V(VEL));
            approach_f_v(sp, 0.0f, rate);
            vec2_set_length(V(VEL), V(VEL), sp);
            return;
        }
        drag = lh(P + 0x30a) == 0 ? k + k : k * 6.0f;
        if (advanced_rac3_movement()) drag = lh(P + 0x30a) == 0 ? k * 8.0f : k * 11.0f;  // enhancement: R&C 3's air braking
    }
    if (lh(P + 0x4a8) != 0 && lbu(P + 0x12e2) != 0) {
        drag = lwc1(OV(0x00166c00u - 0x7e90)) * u2f(0x3e2e147bu);
        if (lw(g::HERO_STATE) == 0xf) {
            hero_stick_move(0, lwc1(0x0015ed6c) * 2.5f);
            drag = lwc1(0x0015ed70) * 3.0f;
        }
        const float slope = lwc1(P + 0x2e0);
        if (u2f(0x3e860a92u) < slope) {
            const float f = lwc1(OV(0x00166c00u - 0x7e90)) * 9.0f;
            sw(P + 0x190, 0);
            drag = fpu_div(slope * f, u2f(0x3f490fdbu));
        }
    }
    sub_00213e68(drag);
}

// orig: 0x00215340 — bone offsets (FX_PARAMS) for running/strafing from the turn rate
// PLAYER+0x188: state 2 (run; sharper with +0x2088 == 1, weighted by speed +0x164), states
// 8/0x81 (slide/skid; 0x81 also leans into the velocity +0x100), else sub-states 2/4.
void hero_run_lean() {
    const uint32_t B = g::FX_PARAMS;
    const uint32_t P = g::PLAYER;
    const int32_t st = lw(g::HERO_STATE);
    const float c04 = u2f(0x3d23d70au), c2 = u2f(0x3e4ccccdu);
    if (st == 2) {
        fx_params_set_0(c04, c2);
        fx_params_set_1(c04, c2);
        fx_params_set_2(c04, c2);
        fx_params_set_3(u2f(0x3ca3d70au), c2);
        if (lw(P + 0x2088) == 1) {
            float t = lwc1(P + 0x188);
            if (u2f(0x3fb33333u) < t) {
                t = u2f(0x3fb33333u);
            } else if (t < u2f(0xbfb33333u)) {
                t = u2f(0xbfb33333u);
            }
            const float vm = lwc1(0x0015ed6c);
            const float n = -t;
            float k = fpu_div(lwc1(P + 0x164), vm * u2f(0x40b66666u));
            const float lean = n * u2f(0x3e0f5c29u);
            swc1(B + 0x60, lean);
            if (1.0f < k) k = 1.0f;
            if (k < 0.0f) k = 0.0f;
            const float c16 = u2f(0x3e23d70au), c35 = u2f(0x3eb33333u);
            swc1(B + 0x60, lean * k);
            swc1(B + 0x118, t * c16);
            swc1(B + 0x1c0, t * u2f(0xbdf5c28fu));
            swc1(B + 0x278, t * u2f(0x3f266666u));
            swc1(B + 0x274, abs_f(t) * u2f(0xbe19999au));
            const float r = n * u2f(0x3e99999au);
            swc1(B + 0x958, n * c35);
            swc1(B + 0x270, abs_f(t) * c16);
            swc1(B + 0xb68, r);
            swc1(B + 0xa08, r);
            swc1(B + 0xab8, r);
            swc1(B + 0x954, abs_f(t) * c35);
            swc1(B + 0xa04, abs_f(t) * c2);
            return;
        }
        float t = lwc1(P + 0x188) * 0.5f;
        if (u2f(0x3f4ccccdu) < t) {
            t = u2f(0x3f4ccccdu);
        } else if (t < u2f(0xbf4ccccdu)) {
            t = u2f(0xbf4ccccdu);
        }
        t = (t * abs_f(t)) * 1.25f;
        swc1(B + 0x118, t);
        swc1(B + 0x278, t);
        const float a = abs_f(t);
        const float n = -t;
        const float r = n * u2f(0x3e99999au);
        swc1(B + 0x958, n * u2f(0x3eb33333u));
        swc1(B + 0xb68, r);
        swc1(B + 0x274, a * 0.25f);
        swc1(B + 0xa08, r);
        swc1(B + 0xab8, r);
        return;
    }
    if (st == 8 || st == 0x81) {
        const float c08 = u2f(0x3da3d70au), c008 = u2f(0x3c03126fu), c015 = u2f(0x3c75c28fu);
        fx_params_set_0(c008, c08);
        fx_params_set_1(c015, c08);
        fx_params_set_2(c015, c08);
        fx_params_set_3(c008, u2f(0x3dcccccdu));
        float t = lwc1(P + 0x188) * u2f(0x3fcccccdu);
        if (1.25f < t) {
            t = 1.25f;
        } else if (t < -1.25f) {
            t = -1.25f;
        }
        if (lw(g::HERO_STATE) == 0x81) {
            t = lwc1(P + 0x188) * u2f(0x3f4ccccdu);
            if (u2f(0x3f8ccccdu) < t) {
                t = u2f(0x3f8ccccdu);
            } else if (t < u2f(0xbf8ccccdu)) {
                t = u2f(0xbf8ccccdu);
            }
        }
        const float n = -t;
        const float lean = n * u2f(0x3f051eb8u);
        swc1(B + 0x60, lean);
        if (u2f(0x3e8f5c29u) < lean) {
            swc1(B + 0x60, u2f(0x3e8f5c29u));
        } else if (lean < u2f(0xbe8f5c29u)) {
            swc1(B + 0x60, u2f(0xbe8f5c29u));
        }
        const float h = t * u2f(0x3f99999au);
        swc1(B + 0x278, h);
        swc1(B + 0x118, t * u2f(0x3f147ae1u));
        swc1(B + 0x1c0, t * u2f(0xbe8f5c29u));
        if (u2f(0x3f19999au) < h) {
            swc1(B + 0x278, u2f(0x3f19999au));
        } else if (h < u2f(0xbf19999au)) {
            swc1(B + 0x278, u2f(0xbf19999au));
        }
        const float r = abs_f(t) * c2;
        swc1(B + 0x274, r);
        if (c2 < r) {
            swc1(B + 0x274, c2);
        } else if (r < u2f(0xbe4ccccdu)) {
            swc1(B + 0x274, u2f(0xbe4ccccdu));
        }
        swc1(B + 0x270, abs_f(t) * u2f(0x3e23d70au));
        if (lw(g::HERO_STATE) == 0x81) {
            if (lwc1(0x0015ed6c) * 0.5f < lwc1(P + 0x164)) {
                const float d = angle_sub(atan2_f(lwc1(P + 0x100), lwc1(P + 0x104)), lwc1(P + 0x98));
                const float sp = lwc1(P + 0x164) * 3.0f;
                swc1(B + 0x60, -sin_vu(d) * sp);
                const float x = lwc1(B + 0x60);
                swc1(B + 0x64, cos_vu(d) * sp);
                const float lim = u2f(0x3edf66f3u);
                if (lim < x) {
                    swc1(B + 0x60, lim);
                } else if (x < -lim) {
                    swc1(B + 0x60, -lim);
                }
                const float y = lwc1(B + 0x64);
                if (lim < y) {
                    swc1(B + 0x64, lim);
                } else if (y < -lim) {
                    swc1(B + 0x64, -lim);
                }
            }
            swc1(B + 0x64, angle_add(lwc1(B + 0x64), u2f(0xbdfa35ddu)));
        }
        const float r3 = n * u2f(0x3e99999au);
        swc1(B + 0x958, n * u2f(0x3eb33333u));
        swc1(B + 0xb68, r3);
        swc1(B + 0xa08, r3);
        swc1(B + 0xab8, r3);
        return;
    }
    const int32_t sub = lw(P + 0x208c);
    if (sub != 2 && sub != 4) return;
    fx_params_set_0(c04, c2);
    fx_params_set_1(c04, c2);
    fx_params_set_2(c04, c2);
    fx_params_set_3(u2f(0x3ca3d70au), c2);
    float t = lwc1(P + 0x188);
    if (u2f(0x3fb33333u) < t) {
        t = u2f(0x3fb33333u);
    } else if (t < u2f(0xbfb33333u)) {
        t = u2f(0xbfb33333u);
    }
    const float n = -t;
    const float r = n * u2f(0x3e99999au);
    swc1(B + 0x958, n * u2f(0x3eb33333u));
    swc1(B + 0x118, t * u2f(0x3e947ae1u));
    swc1(B + 0x1c0, t * u2f(0xbe8a3d71u));
    swc1(B + 0xb68, r);
    swc1(B + 0x278, t * u2f(0x3f5eb852u));
    swc1(B + 0xa08, r);
    swc1(B + 0xab8, r);
}


// orig: 0x00217658 — take hold of the class 0x47 moby at +0x1090: in states 1 / 0x1e past the
// first 0x14 frames (aimed: heading / pitch toward the moby's target, else from the camera),
// sound 0x1b; flag +0x10aa, moby mode 0xa, its velocity / orientation from the aim (or the hero
// rotation), pitch rate 23 * dt, effect table 0x140b00 entries and animation set 6.
void hero_hold_moby_start() {
    const uint32_t P = g::PLAYER;
    if (advanced_rac3_wrench_out()) return;  // enhancement: R&C 3 starts no throw while the wrench is out (UYA 0x4f1a08)
    constexpr OvConst<0x00166c80u> CAM{};
    bool aimed = false;
    const int32_t st = lw(P + 0x2084);
    if (st == 1 || st == 0x1e) aimed = frame_scale_i(0x14) < lw(P + 0x198);
    if (aimed) {
        const uint32_t mob = lwu(P + 0x1090);
        bool target = false;
        if (mob != 0 && moby_is_class_0x47(mob) != 0) {
            const uint32_t t = lwu(mob + 0x78);
            if (lh(t + 0x7e) != 0) {
                target = true;
                Vec4 o = {}, d = {};
                sub_00233660_v(o, 0.5f, 0.0f, 0.5f);
                vec3_sub(d, V(t + 0x50), o);
                swc1(P + 0xa74, atan2_f(d.x, d.y));
                swc1(P + 0xa78, atan2_f(vec2_length(d), d.z));
            }
        }
        if (!target) {
            swc1(P + 0xa74, lwc1(CAM + 0x158));
            swc1(P + 0xa78, angle_add(-lwc1(CAM + 0x154), u2f(0x3dfa35ddu)));
        }
    }
    const uint32_t m = lwu(P + 0x1090);
    if (moby_is_class_0x47(m) == 0) return;
    sb(P + 0x10aa, 1);
    if (aimed) hero_sound_start(0x1b, 0);
    sb(m + 0x20, 0xa);
    const uint32_t t = lwu(m + 0x78);
    if (lwc1(P + 0x248) < u2f(0x3f99999au) && u2f(0x3f29c91fu) < lwc1(P + 0x250) && lbu(P + 0x254) == 0) sh(t + 0x76, 1);
    vec_clear(V(t + 0x40));
    if (!aimed) sub_00233708(t + 0x40, t + 0x40, 1.0f);
    else vec_from_spherical_v(V(t + 0x40), 1.0f, lwc1(P + 0xa74), lwc1(P + 0xa78));
    Vec4 ang = {};
    Mat4 rot = {}, cam = {}, aff = {};
    euler_to_mat3_vu(rot, ang);
    if (aimed) {
        ang.x = lwc1(CAM + 0x150);
        ang.y = lwc1(CAM + 0x154);
        ang.z = lwc1(CAM + 0x158);
        euler_to_mat3_vu(cam, ang);
        mat3_mul(rot, cam, rot);
    } else {
        mat3_mul(rot, M(lwu(P + 0x2080) + 0xc0), rot);
    }
    mat3_copy_affine(aff, rot);
    mat_to_euler_r(aff, V(m + 0x40));
    sw(t + 0x64, 0);
    swc1(t + 0x60, lwc1(0x0015ed6c) * 23.0f);
    sub_00262500(m, 0x00140b00);
    sub_00262528(0x00140b00, 0x30, 3);
    sub_00262528(0x00140b00, 0x17, 5);
    moby_anim_set(m, 6, 0, frame_scale_i(5));
}

// Brake check on the hero's velocity PLAYER+0xe0 (skipped while not moving
// sideways, while +0x30e is set, and when +0x229c > 0.5 unless in state 6, +0x1f4 or +0x12e7):
// the step k * velocity (2 * velocity for +0x2084 == 0x20), stretched to min_len sideways when
// shorter, ends at a point ahead; a probe from 0.3 above it down to 0.2 below (0.35 for
// +0x2084 == 0x20, 0.7 for +0x20a4 == 2) along the hero's up axis looks for ground (flag 4,
// ignoring PLAYER+0x2080). Unless it finds a triangle whose normal pitch is at most ~50 degrees,
// sub_00213de8(0) zeroes the hero's velocities.
// orig: 0x00217118
void hero_edge_brake(float k, float min_len) {
    const uint32_t P = g::PLAYER;
    if (sub_002339d0(P + 0xe0) == 0.0f) return;
    if (0.5f < lwc1(P + 0x229c)) {
        if (lw(P + 0x208c) != 6 && lh(P + 0x1f4) == 0 && lbu(P + 0x12e7) == 0) return;
    }
    if (lh(P + 0x30e) != 0) return;
    const float s = lw(P + 0x2084) == 0x20 ? 2.0f : k;
    Vec4 a;
    vec3_scale(a, V(P + 0xe0), s);
    if (0.0f < min_len && sub_002339d0_v(a) < min_len) {
        sub_00233ba0_v(a, a, 0.0f);
        sub_00233708_v(a, a, min_len);
    }
    sub_00233b20_v(a, a, 0.0f);
    vec3_add(a, a, V(P + 0x80));
    Vec4 b = a;
    sub_00233810_v(a, a, u2f(0x3e99999au));
    float drop = u2f(0xbe4ccccdu);
    if (lw(P + 0x2084) == 0x20) drop = u2f(0xbeb33333u);
    if (lbu(P + 0x20a4) == 2) drop = u2f(0xbf333333u);
    sub_00233810_v(b, b, drop);
    float brake = 1.0f;
    if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) != 0 && 0 < lw(OV(0x00173e5c))) {
        if (sub_00233d30(OV(0x00173e80)) <= u2f(0x3f5f66f3u)) brake = 0.0f;
    }
    if (brake != 0.0f) sub_00213de8(0.0f);
}

// Wall push-back: while moving sideways faster than 0.1 x frame scale (+0x15ed6c) and +0x308 is
// clear, two probes (flag 4, ignoring the hero moby) run horizontally from the hero, at lift h
// (0.37; 0 in substates 0x11/0x12, 0.35 / 1.5 for +0x20a4 == 1 / 2) out to len (0.5; 0.7, 0.27,
// 5) along the velocity (or the facing, when `facing` or nearly still), and at 0.3 h out to
// 1.4 len (its surface type kept). When the first finds a wall pitched at least ~50 degrees, or
// the second a surface of type 8 or 12, the wall moby goes to PLAYER+0x240 and the velocity's
// part into the wall's horizontal normal is removed.
// orig: 0x00212318
void hero_wall_slide(int32_t facing) {
    if (on_blarg()) { blg_call(blg_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 6's own: L6:0x002291d0)
    if (on_batalia()) { bat_call(bat_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 8's own: L8:0x00223c60)
    if (on_pokitaru()) { pok_call(pok_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 11's own: L11:0x00242380)
    if (on_hoven()) { hov_call(hov_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 12's own: L12:0x00233a38)
    if (on_oltanis()) { olt_call(olt_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 14's own: L14:0x00223de8)
    if (on_quartu()) { qua_call(qua_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 15's own: L15:0x0020a328)
    if (on_kalebo()) { kal_call(kal_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 16's own: L16:0x0020fb68)
    if (on_veldin_finale()) { vf_call(vf_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 18's own: L18:0x0021a010)
    if (on_veldin_orbit()) { vo_call(vo_hero_wall_slide_u, static_cast<uint32_t>(facing)); return; }  // (level 17's own: L17:0x0020f128)
    if (on_l1_family() || on_rilgar()) return hero_wall_slide_l1(facing);
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lh(P + 0x308) != 0) return;
    if (sub_002339d0(P + 0xe0) < lwc1(0x0015ed6c) * u2f(0x3dcccccdu)) return;
    float lift = u2f(0x3ebd70a4u), len = 0.5f;
    if (static_cast<uint32_t>(lw(P + 0x208c) - 0x11) < 2u) {
        lift = 0.0f;
        len = u2f(0x3f333333u);
    } else if (lbu(P + 0x20a4) == 1) {
        lift = u2f(0x3eb33333u);
        len = u2f(0x3e8a3d71u);
    } else if (lbu(P + 0x20a4) == 2) {
        lift = 1.5f;
        len = 5.0f;
    }
    Vec4 s1 = V(P + 0x80);
    s1.z = s1.z + lift;
    Vec4 s2 = V(P + 0x80);
    s2.z = s2.z + lift * u2f(0x3e99999au);
    Vec4 a{0.0f, 0.0f, 0.0f, 0.0f}, b{0.0f, 0.0f, 0.0f, 0.0f};
    if (facing == 0 && !(vec2_length(V(P + 0xe0)) < u2f(0x3c23d70au))) {
        a = V(P + 0xe0);
        a.z = 0.0f;
        vec3_scale(a, a, fpu_div(len, vec2_length(a)));
        b = a;
        vec3_scale(b, b, u2f(0x3fb33333u));
    } else {
        const float k = u2f(0x3fb33333u);
        a.x = cos_vu(lwc1(P + 0x98)) * len;
        a.y = sin_vu(lwc1(P + 0x98)) * len;
        a.z = 0.0f;
        b.x = cos_vu(lwc1(P + 0x98)) * len * k;
        b.y = sin_vu(lwc1(P + 0x98)) * len * k;
        b.z = 0.0f;
    }
    vec3_add(a, a, s1);
    int32_t surf = -1;
    vec3_add(b, b, s2);
    if (segment_collide_v(s2, b, 4, lwu(P + 0x2080), nullptr) != 0 && 0 < lw(OUTB + 0x1c)) surf = sub_001f35d8();
    float pitch = 0.0f;
    if (segment_collide_v(s1, a, 4, lwu(P + 0x2080), nullptr) != 0 && 0 < lw(OUTB + 0x1c)) {
        pitch = atan2_f(lwc1(OUTB + 0x48), vec2_length(V(OUTB + 0x40)));
    }
    if (!(u2f(0x3f5f66f3u) <= pitch) && surf != 8 && surf != 0xc) return;
    sw(OUTB + 0x48, 0);
    sw(P + 0x240, lwu(OUTB + 0x18));
    Vec4& n = V(OUTB + 0x40);
    vu0_normalize(n, n);
    const float into = -lwc1(P + 0xe0) * n.x - lwc1(P + 0xe4) * n.y;
    if (!(0.0f < into)) return;
    vec3_scale(n, n, into);
    vec3_add(V(P + 0xe0), V(P + 0xe0), n);
}

// Push the hero out of geometry: the body centre (PLAYER+0x80 raised by +0x224, or 0.6 along the
// up axis in move mode 1 / with +0x1f8, or lowered by +0x224 through sub_002334d0 in the other
// modes) is collided up to 8 times — on foot as a capsule (radius +0x234, height +0x220 - +0x224,
// at least 0.05) plus the mesh sphere test, as a 0.6 sphere in substate 0x11, a 0.45 x scale
// sphere in substate 0xf, a 0.4 x scale sphere in the other move modes — each hit moving it to
// the pushed-out centre (0x173e70) and recording the normal +0x200, contact +0x210, moby +0x23c
// and +0x257. Flags 0x24 (0xd24 in state 0x7f); +0x1cc skips all (1). Returns 1 unless the push
// exceeded 1.5 x the radius +0x234: then -1, and with mode 0xf the move is cut to the radius +0x234
// (components clamped to +-512 first).
// orig: 0x002133a8
int32_t hero_collide_push(int32_t mode) {
    if (on_blarg()) return static_cast<int32_t>(blg_call(blg_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 6's own: L6:0x0022a1d0)
    if (on_rilgar()) return static_cast<int32_t>(ril_call(ril_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (Rilgar's own: L5:0x00240ed0)
    if (on_pokitaru()) return static_cast<int32_t>(pok_call(pok_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 11's own: L11:0x00243348)
    if (on_hoven()) return static_cast<int32_t>(hov_call(hov_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 12's own: L12:0x002349e8)
    if (on_oltanis()) return static_cast<int32_t>(olt_call(olt_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 14's own: L14:0x00224db0)
    if (on_quartu()) return static_cast<int32_t>(qua_call(qua_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 15's own: L15:0x0020b320)
    if (on_kalebo()) return static_cast<int32_t>(kal_call(kal_hero_collide_push_u, static_cast<uint32_t>(mode)).v0);  // (level 16's own: L16:0x00210b60)
    if (on_l1_family()) return hero_collide_push_l1(mode);
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    if (lw(P + 0x1cc) != 0) return 1;
    const Vec4 old = V(P + 0x80);
    Vec4 off;
    vec_clear(off);
    if (lh(P + 0x22da) != 0) {
        hero_path_push_out();
        sh(P + 0x22da, 0);
    }
    if (lw(P + 0x208c) != 0x11) {
        const uint32_t mm = lbu(P + 0x20b3);
        if (mm == 0 && lh(P + 0x1f8) == 0) {
            off.z = lwc1(P + 0x224);
        } else if (mm == 1 || lh(P + 0x1f8) != 0) {
            sub_00233810_v(off, off, u2f(0x3f19999au));
        } else {
            sub_002334d0_v(off, off, -lwc1(P + 0x224));
        }
    }
    vec3_add(V(P + 0x80), V(P + 0x80), off);
    const uint32_t flags = lw(P + 0x2084) == 0x7f ? 0xd24u : 0x24u;
    for (int32_t n = 0; n < 8; ++n) {
        int32_t hit;
        if (lbu(P + 0x20b3) != 0) {
            hit = sphere_collide(P + 0x80, lwc1(0x0015ed60) * u2f(0x3ecccccdu), flags, lwu(P + 0x2080));
        } else if (lw(P + 0x208c) == 0x11) {
            hit = sphere_collide(P + 0x80, u2f(0x3f19999au), flags, lwu(P + 0x2080));
        } else if (lw(P + 0x208c) == 0xf) {
            hit = sphere_collide(P + 0x80, lwc1(0x0015ed60) * u2f(0x3ee66666u), flags, lwu(P + 0x2080));
        } else {
            float h = lwc1(P + 0x220) - lwc1(P + 0x224);
            if (h < u2f(0x3d4ccccdu)) h = u2f(0x3d4ccccdu);
            const int32_t a = capsule_collide(P + 0x80, lwc1(P + 0x234), h, flags, lwu(P + 0x2080));
            const int32_t b = sphere_mesh_collide(P + 0x80, lwc1(P + 0x234));
            hit = a | b;
        }
        if (hit == 0) break;
        V(P + 0x80) = V(OUTB + 0x30);
        V(P + 0x200) = V(OUTB + 0x40);
        V(P + 0x210) = V(OUTB + 0x20);
        sw(P + 0x23c, lwu(OUTB + 0x18));
        sb(P + 0x257, 1);
    }
    vec3_sub(V(P + 0x80), V(P + 0x80), off);
    Vec4 d;
    vec3_sub(d, V(P + 0x80), old);
    if (!(lwc1(P + 0x234) * 1.5f < vec3_length(d))) return 1;
    if (mode != 0xf) return -1;
    auto clamp = [](float& v) {
        if (512.0f < v) {
            v = 512.0f;
        } else if (v < -512.0f) {
            v = -512.0f;
        }
    };
    clamp(d.x);
    clamp(d.y);
    clamp(d.z);
    vec3_set_length(d, d, lwc1(P + 0x234));
    vec3_add(V(P + 0x80), old, d);
    return -1;
}

// Resolve the hero's collisions: hero_collide_push(i) for i = 0..15 until one succeeds; after
// each failure the hero steps back through its position history (ring of 32 at PLAYER+0x1b00,
// index +0x21b0, depth +0x21b4, kept at 1 or more).
// orig: 0x002137a8
void hero_collide_resolve() {
    const uint32_t P = g::PLAYER;
    for (int32_t i = 0; i < 0x10; ++i) {
        if (hero_collide_push(i) == 1) return;
        if (lw(P + 0x21b4) >= 2) {
            sw(P + 0x21b4, static_cast<uint32_t>(lw(P + 0x21b4) - 1));
            sw(P + 0x21b0, static_cast<uint32_t>((lw(P + 0x21b0) + 0x1f) % 32));
        }
        V(P + 0x80) = V(P + 0x1b00 + static_cast<uint32_t>(lw(P + 0x21b0)) * 0x10);
    }
}

// Ground movement with drift (states 0x13 / 0x15): stick input, turning (sub_00211e30), a push
// of length PLAYER+0x190 x 5.9 x frame scale along +0x180 (0x13: 0.4 along the facing once
// +0x229c < 0.2; 0x15: none) plus the knock-back +0x920 when +0x930 > 0. The velocity +0xe0
// moves from +0x130 towards it by at most an acceleration (2.45 x +0x190 + 2 x the braking share
// of the push, x 0x15ed70; 1.1 with *(0x13cb14); 11 early in state 0x13 when slow, 13 when fast,
// at least the knock-back share), slopes steeper than 5 degrees pull it downhill (+0x270 /
// +0x2e0), +0x3b0 = forward share, hero_wall_slide, a slope just started scales it by cos, and
// the hero snaps to the ground +0x2d8 within 0.15.
// orig: 0x002167d0
void hero_slide_move() {
    const uint32_t P = g::PLAYER;
    hero_stick_move(0, 1.0f);
    sub_00211e30(0, lwc1(0x0015ed64) * u2f(0x3ca3d70au), lwc1(0x0015ed64) * u2f(0x3e4ccccdu), lwc1(0x0015ed6c) * u2f(0x406a927fu));
    const int32_t st = lw(P + 0x2084);
    if (st == 0x13) {
        if (lwc1(P + 0x229c) < u2f(0x3e4ccccdu)) swc1(P + 0x180, lwc1(P + 0x98));
        swc1(P + 0x190, u2f(0x3ecccccdu));
    } else if (st == 0x15) {
        sw(P + 0x190, 0);
    }
    const float k59 = u2f(0x40bccccdu);
    Vec4 push{0.0f, 0.0f, 0.0f, 0.0f};
    push.x = cos_vu(lwc1(P + 0x180)) * lwc1(P + 0x190) * (lwc1(0x0015ed6c) * k59);
    push.y = sin_vu(lwc1(P + 0x180)) * lwc1(P + 0x190) * (lwc1(0x0015ed6c) * k59);
    push.z = 0.0f;
    const Vec4 push0 = push;
    Vec4 knock{0.0f, 0.0f, 0.0f, 0.0f};
    if (0.0f < lwc1(P + 0x930)) {
        sub_002132b8(u2f(0x3f333333u), lwc1(0x0015ed6c) * 0.0f);
        knock = V(P + 0x920);
        vec3_add(push, push, V(P + 0x920));
        vec_clear(V(P + 0x920));
    }
    float brake = -(lwc1(P + 0xe0) * push.x + lwc1(P + 0xe4) * push.y);
    if (0.0f < brake) {
        brake = fpu_div(brake, vec2_length(push));
        brake = fpu_div(brake, vec2_length(V(P + 0xe0)));
    }
    if (1.0f < brake) brake = 1.0f;
    if (brake < 0.0f) brake = 0.0f;
    const float f70 = lwc1(0x0015ed70);
    float acc = f70 * u2f(0x401ccccdu) * lwc1(P + 0x190) + (f70 + f70) * brake;
    if (lw(0x0013cb14) != 0) acc = f70 * u2f(0x3f8ccccdu);
    if (lw(P + 0x2084) == 0x13 && lw(P + 0x198) < frame_scale_i(0xf) && lwc1(P + 0x160) < lwc1(0x0015ed6c) * 3.0f) acc = lwc1(0x0015ed70) * 11.0f;
    if (lwc1(0x0015ed6c) * k59 < lwc1(P + 0x160)) acc = lwc1(0x0015ed70) * 13.0f;
    if (0.0f < lwc1(P + 0x930)) {
        float along = -vec3_dot(push0, knock);
        const float l = vec3_length(push0);
        if (0.0f < l) along = fpu_div(along, l);
        const float kl = vec2_length(knock);
        const float share = fpu_div(kl - along, kl);
        const float f4 = lwc1(0x0015ed70) * 3.0f;
        float a = f4 * share;
        if (lwc1(P + 0x229c) < u2f(0x3e99999au) && a < f4) a = f4;
        if (acc < a) acc = a;
    }
    Vec4 d;
    vec3_sub(d, push, V(P + 0x130));
    d.z = 0.0f;
    if (acc < vec3_length(d)) vec3_set_length(d, d, acc);
    vec3_add(V(P + 0xe0), V(P + 0x130), d);
    if (u2f(0x3db2b8c2u) < lwc1(P + 0x2e0)) {
        const float a = atan2_f(lwc1(P + 0x270), lwc1(P + 0x274));
        const float pi4 = u2f(0x3f490fdbu);
        const float sp = fpu_div(lwc1(0x0015ed6c) * 8.0f * lwc1(P + 0x2e0), pi4);
        const float lim = fpu_div(lwc1(0x0015ed70) * u2f(0x41080000u) * lwc1(P + 0x2e0), pi4);
        Vec4 sv{0.0f, 0.0f, 0.0f, 0.0f};
        sv.x = cos_vu(a) * sp;
        sv.y = sin_vu(a) * sp;
        sv.z = 0.0f;
        Vec4 e;
        vec3_sub(e, sv, V(P + 0xe0));
        e.z = 0.0f;
        if (lim < vec3_length(e)) vec3_set_length(e, e, lim);
        vec3_add(V(P + 0xe0), V(P + 0xe0), e);
    }
    Vec4 v = V(P + 0xe0);
    v.z = 0.0f;
    swc1(P + 0x3b0, vec3_dot(v, push));
    const float pl = vec3_length(push);
    if (0.0f < pl) swc1(P + 0x3b0, fpu_div(lwc1(P + 0x3b0), pl * (lwc1(0x0015ed6c) * k59)));
    hero_wall_slide(0);
    if (lwc1(P + 0x3b4) < u2f(0x3c8efa35u) && u2f(0x3db2b8c2u) < lwc1(P + 0x2e4)) {
        vec3_scale(V(P + 0xe0), V(P + 0xe0), cos_vu(lwc1(P + 0x2e4)));
    }
    swc1(P + 0x3b4, lwc1(P + 0x2e4));
    if (abs_f(lwc1(P + 0x88) - lwc1(P + 0x2d8)) < u2f(0x3e19999au)) swc1(P + 0x88, lwc1(P + 0x2d8));
}

// Ground probe of the hero: resets the ground state (+0x270 normal, +0x278 = 1, +0x2a0 point,
// +0x2d8 height, +0x2dc = 32 distance, +0x2e0 slope, +0x2fc moby, +0x30a; +0x30c / +0x30e
// counters up; +0x2f8 counting while set) and looks down 48 from 2.2 radii above (2.9 in
// substate 2; along the up axis when not in move mode 0) with flag 2: the surface type goes to
// +0x12e0 and the sound kind to +0x12ed; on plain ground the water/floor height +0x2f0 (normal
// +0x280) is taken and the probe repeated with flags 0x24 (level 1 / 0x12: sound kind 3); types
// 13 (+0x2f0) and 3 (+0x2f4) probe again just below their surface with flag 4, type 11 only
// records +0x2f4, type 1 is ignored in states 0x7c / 0x7d. A contact sets the ground point,
// height, distance (negative when under it), moby, normal, slope +0x2e0 and its yaw +0x2ec;
// within 0.02 the hero stands (+0x30c = 0; and +0x30e = 0, +0xa80 = moby on walkable slopes, in
// move mode 1 or substate 0x16). +0x300 counts airborne frames (+0x2b0 last ground point).
// +0x290 = the down axis of the move mode. Standing on the ground (< 0.25, not in substates
// 3..6, 0xf, 0x15) the slope angles +0x2e4 / +0x2e8 (within 45 degrees) are kept and two side
// probes (left / right, 0.2 ahead from 0.45 up down to the first probe's end, flags 0x22) fill
// the side heights +0x2c0 (within 0.4) and slope angles +0x2c8 / +0x2d0.
// orig: 0x002127b8
void hero_ground_update() {
    if (on_blarg()) { blg_call(blg_hero_ground_update_u); return; }  // (level 6's own: L6:0x00229650)
    if (on_batalia()) { bat_call(bat_hero_ground_update_u); return; }  // (level 8's own: L8:0x002240a8)
    if (on_pokitaru()) { pok_call(pok_hero_ground_update_u); return; }  // (level 11's own: L11:0x002427c8)
    if (on_hoven()) { hov_call(hov_hero_ground_update_u); return; }  // (level 12's own: L12:0x00233e80)
    if (on_oltanis()) { olt_call(olt_hero_ground_update_u); return; }  // (level 14's own: L14:0x00224230)
    if (on_quartu()) { qua_call(qua_hero_ground_update_u); return; }  // (level 15's own: L15:0x0020a7a0)
    if (on_kalebo()) { kal_call(kal_hero_ground_update_u); return; }  // (level 16's own: L16:0x0020ffb0)
    if (on_veldin_finale()) { vf_call(vf_hero_ground_update_u); return; }  // (level 18's own: L18:0x0021a488)
    if (on_rilgar()) { ril_call(ril_hero_ground_update_u); return; }  // (Rilgar's own: L5:0x002402a8)
    if (on_l1_family()) return hero_ground_update_l1();
    const uint32_t P = g::PLAYER, OUTB = OV(0x00173e40);
    swc1(P + 0x278, 1.0f);
    sh(P + 0x30e, lhu(P + 0x30e) + 1u);
    sh(P + 0x30c, lhu(P + 0x30c) + 1u);
    sw(P + 0x270, 0);
    sw(P + 0x274, 0);
    sw(P + 0x2e0, 0);
    sw(P + 0x2d8, 0);
    vec_clear(V(P + 0x2a0));
    sh(P + 0x30a, 0);
    swc1(P + 0x2dc, 32.0f);
    sw(P + 0x2fc, 0);
    if (lw(P + 0x2f8) != 0) sw(P + 0x2f8, static_cast<uint32_t>(lw(P + 0x2f8) + 1));
    const float ha = lwc1(P + 0x234) * (lw(P + 0x208c) == 2 ? u2f(0x4039999au) : u2f(0x400ccccdu));
    Vec4 a, b;
    sub_002126b8_v(a, b, lbu(P + 0x20b3) == 0 ? 0 : 1, ha, -48.0f);
    // returns whether the airborne counter / water flag tail runs
    auto probe = [&]() -> bool {
        if (segment_collide_v(a, b, 2, lwu(P + 0x2080), nullptr) == 0) return true;
        const int32_t snd = sub_001f3608();
        sb(P + 0x12ed, static_cast<uint32_t>(snd != -1 ? snd : 3));
        const int32_t t = sub_001f35d8();
        sh(P + 0x12e0, static_cast<uint32_t>(t));
        if ((static_cast<uint32_t>(t) << 16) == 0) {
            const float h = surface_height(OUTB + 0x20, P + 0x280);
            swc1(P + 0x2f0, h);
            if (lwc1(P + 0x88) < h && lw(P + 0x2f8) <= 0) sw(P + 0x2f8, 1);
            if (segment_collide_v(a, b, 0x24, lwu(P + 0x2080), nullptr) == 0) return false;
            int32_t k = sub_001f3608();
            const int32_t lvl = lw(0x0015ed84);
            if (lvl == 1 || lvl == 0x12) k = 3;
            sb(P + 0x12ed, static_cast<uint32_t>(k));
        }
        if (lh(P + 0x12e0) == 0xd) {
            const float cz = lwc1(OUTB + 0x28);
            swc1(P + 0x2f0, cz);
            a.z = cz - u2f(0x3c23d70au);
            if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) return false;
        }
        if (lh(P + 0x12e0) == 3) {
            const float cz = lwc1(OUTB + 0x28);
            swc1(P + 0x2f4, cz);
            a.z = cz - u2f(0x3c23d70au);
            if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) return false;
        }
        if (lh(P + 0x12e0) == 0xb) {
            swc1(P + 0x2f4, lwc1(OUTB + 0x28));
            return false;
        }
        if (lh(P + 0x12e0) == 1 && static_cast<uint32_t>(lw(P + 0x2084) - 0x7c) < 2u) return false;
        const uint32_t hit = lwu(OUTB + 0x18);
        if (hit != 0) {
            if (moby_in_range_class_500(hit) != 0) sh(P + 0x30a, 1);
            const uint32_t pw = moby_pvar_word0(lwu(OUTB + 0x18));
            if (pw != 0 && (lhu(pw + 0x1e) & 8) != 0) sh(P + 0x30a, 1);
        }
        if (lw(OUTB + 0x1c) <= 0) return true;
        V(P + 0x2a0) = V(OUTB + 0x20);
        swc1(P + 0x2d8, lwc1(OUTB + 0x28));
        swc1(P + 0x2dc, vec3_distance(V(P + 0x80), V(P + 0x2a0)));
        sw(P + 0x2fc, lwu(OUTB + 0x18));
        vu0_normalize(V(P + 0x270), V(OUTB + 0x40));
        swc1(P + 0x2e0, sub_00233d30(OUTB + 0x40));
        swc1(P + 0x2ec, atan2_f(lwc1(OUTB + 0x40), lwc1(OUTB + 0x44)));
        Vec4 d;
        vec3_sub(d, V(P + 0x2a0), V(P + 0x80));
        if (0.0f < vec3_dot(d, V(P + 0x270))) swc1(P + 0x2dc, -lwc1(P + 0x2dc));
        if (lwc1(P + 0x2dc) < u2f(0x3ca3d70au)) {
            sh(P + 0x30c, 0);
            if (lwc1(P + 0x2e0) <= u2f(0x3f5f66f3u) || lbu(P + 0x20b3) == 1 || lw(P + 0x208c) == 0x16) {
                sh(P + 0x30e, 0);
                sw(P + 0xa80, lwu(OUTB + 0x18));
            }
        }
        return true;
    };
    if (probe()) {
        if (lh(P + 0x30e) != 0) {
            sw(P + 0x300, 0);
        } else {
            sw(P + 0x300, static_cast<uint32_t>(lw(P + 0x300) + 1));
            V(P + 0x2b0) = V(P + 0x2a0);
        }
        if (lwc1(P + 0x2f0) < lwc1(P + 0x88)) sw(P + 0x2f8, 0);
    }
    // the down axis of the move mode
    vec_clear(V(P + 0x290));
    const uint32_t mode = lbu(P + 0x20b3);
    if (mode == 1) {
        V(P + 0x290) = V(P + 0x270);
        vec3_scale(V(P + 0x290), V(P + 0x290), -1.0f);
    } else if (mode == 0) {
        swc1(P + 0x298, -1.0f);
    } else if (mode == 2) {
        vec3_sub(V(P + 0x290), V(P + 0xb0), V(P + 0x80));
        vec3_set_length(V(P + 0x290), V(P + 0x290), 1.0f);
    }
    for (uint32_t o : {0x2e4u, 0x2e8u, 0x2d0u, 0x2d4u, 0x2c8u, 0x2ccu, 0x2c0u, 0x2c4u}) sw(P + o, 0);
    const int32_t sub = lw(P + 0x208c);
    if (sub == 0xf || sub == 0x15 || sub == 6 || sub == 4 || sub == 5 || sub == 3) return;
    if (!(lwc1(P + 0x2dc) < 0.25f)) return;
    const float q = u2f(0x3f490fdbu);
    float ang[2];
    sub_00216078_v(V(P + 0x270), ang);
    if (-q < ang[1] && ang[1] < q) swc1(P + 0x2e4, ang[1]);
    if (-q < ang[0] && ang[0] < q) swc1(P + 0x2e8, ang[0]);
    for (uint32_t i = 0; i < 2; ++i) {
        const float side = i == 1 ? -u2f(0x3fc90fdbu) : u2f(0x3fc90fdbu);
        Vec4 s0{0.0f, 0.0f, 0.0f, 0.0f}, s1{0.0f, 0.0f, 0.0f, 0.0f};
        sub_002335c8_v(s0, u2f(0x3e4ccccdu), side, u2f(0x3ee66666u));
        sub_002335c8_v(s1, u2f(0x3e4ccccdu), side, u2f(0xbee66666u));
        if (segment_collide_v(s0, b, 0x22, lwu(P + 0x2080), nullptr) == 0) {
            sw(P + 0x2c0 + 4 * i, 0);
            swc1(P + 0x2c8 + 4 * i, lwc1(P + 0x2e4));
            swc1(P + 0x2d0 + 4 * i, lwc1(P + 0x2e8));
            continue;
        }
        const float dz = lwc1(OUTB + 0x28) - lwc1(P + 0x2d8);
        swc1(P + 0x2c0 + 4 * i, dz);
        if (u2f(0x3ecccccdu) < abs_f(dz)) sw(P + 0x2c0 + 4 * i, 0);
        Vec4 n;
        vu0_normalize(n, V(OUTB + 0x40));
        float sa[2];
        sub_00216078_v(n, sa);
        if (-q < sa[1] && sa[1] < q) swc1(P + 0x2c8 + 4 * i, sa[1]);
        if (-q < sa[0] && sa[0] < q) swc1(P + 0x2d0 + 4 * i, sa[0]);
    }
}

// Integrate the hero's motion: ride platforms, clamp the velocity +0xe0 to the radius - 0.02 per
// frame (sideways only in states 0x14 / 0x22, where it also cannot sink faster than the ground
// distance +0x2dc; in xy in substate 0xf; not in 0xd / 0xe; none with +0x1cc), move by it and by
// the knock-back +0x920, collide (carry +0xf0: resolve, ground, +0x100 = moved, sub_00212ff0;
// else just the ground probe), then derive the effective velocities: +0x110 (3D), +0x130 (along
// the ground plane) and +0x120 (sideways in the move frame) are the velocity's share along the
// actual move, +0x160 / +0x164 / +0x168 speeds; a carry +0xf0 moves and collides once more
// (+0xfc kept, then to +0x14c), +0x140 = carry move, +0x100 = total move, +0x16c = its climb
// ratio (+-0.5), and the move is capped at 52 x frame scale.
// orig: 0x00213880
void hero_move_integrate() {
    if (on_rilgar()) { ril_call(ril_hero_move_integrate_u); return; }  // (Rilgar's own: L5:0x002413e0)
    if (on_pokitaru()) { pok_call(pok_hero_move_integrate_u); return; }  // (level 11's own: L11:0x002437e8)
    if (on_hoven()) { hov_call(hov_hero_move_integrate_u); return; }  // (level 12's own: L12:0x00234e88)
    if (on_quartu()) { qua_call(qua_hero_move_integrate_u); return; }  // (level 15's own: L15:0x0020b7c0)
    if (on_kalebo()) { kal_call(kal_hero_move_integrate_u); return; }  // (level 16's own: L16:0x00211098)
    if (on_l1_family()) return hero_move_integrate_l1();
    const uint32_t P = g::PLAYER;
    const Vec4 old = V(P + 0x80);
    hero_platform_ride();
    const float r = lwc1(P + 0x234) - u2f(0x3ca3d70au);
    const int32_t st = lw(P + 0x2084);
    if (st == 0x22 || st == 0x14) {
        if (lw(P + 0x1cc) == 0 && r < sub_002339d0(P + 0xe0)) vec3_set_length(V(P + 0xe0), V(P + 0xe0), r);
        float lim = -lwc1(P + 0x2dc);
        if (sub_00233a78(P + 0xe0) < lim) {
            if (0.0f < lim) lim = 0.0f;
            sub_00233b20(P + 0xe0, P + 0xe0, lim);
        }
    } else {
        const int32_t sub = lw(P + 0x208c);
        if (sub == 0xf) {
            if (lw(P + 0x1cc) == 0 && r < vec2_length(V(P + 0xe0))) vec2_set_length(V(P + 0xe0), V(P + 0xe0), r);
        } else if (sub != 0xd && sub != 0xe) {
            if (lw(P + 0x1cc) == 0 && r < vec3_length(V(P + 0xe0))) vec3_set_length(V(P + 0xe0), V(P + 0xe0), r);
        }
    }
    vec3_add(V(P + 0x80), V(P + 0x80), V(P + 0xe0));
    vec3_add(V(P + 0x80), V(P + 0x80), V(P + 0x920));
    vec_clear(V(P + 0x920));
    sw(P + 0x23c, 0);
    sb(P + 0x257, 0);
    const float eps = u2f(0x38d1b717u);
    if (vec3_length(V(P + 0xf0)) <= eps) {
        hero_collide_resolve();
        hero_ground_update();
        vec3_sub(V(P + 0x100), V(P + 0x80), old);
        sub_00212ff0();
    } else {
        hero_ground_update();
    }
    auto project = [](uint32_t dst, const Vec4& v) {  // dst (a move) := v's share along it, >= 0
        vec3_set_length(V(dst), V(dst), 1.0f);
        float d = vec3_dot(V(dst), v);
        if (d < 0.0f) d = 0.0f;
        vec3_set_length(V(dst), v, d);
    };
    vec3_sub(V(P + 0x110), V(P + 0x80), old);
    V(P + 0x130) = V(P + 0x110);
    V(P + 0x120) = V(P + 0x110);
    project(P + 0x110, V(P + 0xe0));
    Vec4 g = V(P + 0xe0);
    sub_002338d0_v(g, g);
    sub_002338d0(P + 0x130, P + 0x130);
    project(P + 0x130, g);
    Vec4 h = V(P + 0xe0);
    sub_00233ba0_v(h, h, 0.0f);
    sub_00233ba0(P + 0x120, P + 0x120, 0.0f);
    project(P + 0x120, h);
    swc1(P + 0x160, vec3_length(V(P + 0x110)));
    swc1(P + 0x164, vec2_length(V(P + 0x110)));
    const Vec4 v110 = V(P + 0x110);
    const float up = sub_00213350_v(v110);
    swc1(P + 0x168, up);
    if (up < 0.0f) swc1(P + 0x168, 0.0f);
    const Vec4 before = V(P + 0x80);
    if (eps < vec3_length(V(P + 0xf0))) {
        vec3_add(V(P + 0x80), V(P + 0x80), V(P + 0xf0));
        const float fc = lwc1(P + 0xfc);
        vec_clear(V(P + 0xf0));
        swc1(P + 0xfc, fc);
        hero_collide_resolve();
        hero_ground_update();
        vec3_sub(V(P + 0x100), V(P + 0x80), old);
        sub_00212ff0();
    }
    vec3_sub(V(P + 0x140), V(P + 0x80), before);
    const float fc = lwc1(P + 0xfc);
    sw(P + 0xfc, 0);
    swc1(P + 0x14c, fc);
    vec3_sub(V(P + 0x100), V(P + 0x80), old);
    sw(P + 0x16c, 0);
    if (u2f(0x3b83126fu) < lwc1(P + 0x164)) {
        const float k = fpu_div(lwc1(P + 0x108), lwc1(P + 0x164));
        swc1(P + 0x16c, k);
        if (0.5f < k) {
            swc1(P + 0x16c, 0.5f);
        } else if (k < -0.5f) {
            swc1(P + 0x16c, -0.5f);
        }
    }
    const float cap = lwc1(0x0015ed6c) * 52.0f;
    if (cap < lwc1(P + 0x160)) {
        vec3_scale(V(P + 0x100), V(P + 0x100), fpu_div(cap, lwc1(P + 0x160)));
        swc1(P + 0x160, lwc1(0x0015ed6c) * 52.0f);
    }
}

}  // namespace rac1
