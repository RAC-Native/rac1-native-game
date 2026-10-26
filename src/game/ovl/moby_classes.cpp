// Ported overlay code: the update functions of the moby classes (moby +0x74, called by the
// update pass 0x264848 through ovl_call_moby_fn) and their helpers.
#include <cmath>
#include <game/ovl/debris_piece.h>
#include <game/ovl/fx1471.h>
#include <game/ovl/ovl_reloc.h>
#include <cstring>
#include <utility>
#include <stdexcept>

#include <game/math/vecmath.h>
#include <game/ovl/ammo_pickup.h>
#include <game/ovl/ovl.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl_common.h>
#include <game/lib/libc.h>
#include <game/port_stubs.h>

namespace rac1 {

// Queues a draw callback (function, argument) for the frame (64 at most; 0x16da80 / 0x16db80,
// count 0x15f428).
// orig: 0x001f9598
void draw_callback_add(uint32_t fn, uint32_t arg) {
    const int32_t n = lw(OV(0x0015f428));
    if (!(n < 0x40)) return;
    sw(OV(0x0015f428), static_cast<uint32_t>(n + 1));
    sw(OV(0x0016da80) + static_cast<uint32_t>(n) * 4, fn);
    sw(OV(0x0016db80) + static_cast<uint32_t>(n) * 4, arg);
}

// Class 607 (at the hero): animation 6 over 10 frames while the hero's state 0x1413d4 is 8.
// orig: 0x002d37d0
void moby_607_update(uint32_t m) {
    if (lw(0x001413d4) == 8 && lbu(m + 0x53) != 6) moby_anim_set(m, 6, 0, frame_scale_i(10));
}

// Class 530: started (state 1, +0xbc = 1, pvars +0x40 = 0); in mode 2 it is hidden (flag 1,
// +0x31 = 0) unless 0x16c890 is 3 or more and not 4; the world matrix of its chain 0 into its
// pvars; when shown its draw callback 0x2d19b0 is queued.
// orig: 0x002d1e80
void moby_530_update(uint32_t m) {
    if (lbu(m + 0x20) == 0) {
        sb(m + 0xbc, 1);
        sb(m + 0x20, 1);
        sw(lwu(m + 0x78) + 0x40, 0);
    }
    uint32_t flags = lhu(m + 0x34);
    const int32_t v = lw(OV(0x0016c890));
    if (lw(OV(0x0015f5c4)) == 2 && (v < 3 || v == 4)) {
        sb(m + 0x31, 0);
        flags |= 1u;
    } else {
        sb(m + 0x31, 1);
        flags &= 0xfffeu;
    }
    sh(m + 0x34, flags);
    moby_chain_world_one(m, 0, lwu(m + 0x78));
    if (lbu(m + 0x31) != 0) draw_callback_add(OVF(0x002d19b0), m);
}

// Classes 1781 / 1782 (plants): animation 1 (over 6 frames) when the hero is within 1 of them
// and moving faster than twice the frame time 0x15ed6c; back to 0 once it looped.
// orig: 0x002e3ca8
void moby_plant_update(uint32_t m) {
    if (vec3_distance(V(0x0013f3d0), V(m + 0x10)) < 1.0f) {
        const float speed = vec3_length(V(0x0013f450));
        if (lwc1(0x0015ed6c) + lwc1(0x0015ed6c) < speed && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(6));
    }
    if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x53) == 1) moby_anim_set(m, 0, 0, frame_scale_i(6));
}

// Class 1060's light flicker refresh: once per level-setup count 0x15f5cc (kept in *(pvars +8))
// when it has a group, the draw callback 0x2df3d8 is queued.
// orig: 0x002df4a8
void moby_1060_light_mark(uint32_t m) {
    if (lbu(m + 0x21) == 0xff) return;
    const uint32_t n = lwu(OV(0x0015f5cc));
    if (n == 0) return;
    const uint32_t p = lwu(lwu(m + 0x78) + 8);
    if (n == lwu(p)) return;
    sw(p, n);
    sub_001f93e0(OVF(0x002df3d8), m);
}

// Class 1060 (a pulsing light): the phase (pvars +0, degrees turned to radians at the start)
// advances by the rate 0x161b98 degrees a frame; the colour between 0x161b90 and 0x161b94 by
// its sine, kept in pvars +4 (with the alpha 0x20, or 0x38 off Veldin and level 0x12) and set as
// the moby's colour.
// orig: 0x002df4f8
void moby_1060_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        swc1(p, lwc1(p) * u2f(0x3c8efa35u));
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    float rate = lwc1(OV(0x00161b98)) * u2f(0x3c8efa35u);
    rate = rate * lwc1(0x0015ed6c);
    const float a = angle_add(lwc1(p), rate);
    swc1(p, a);
    const float sn = sin_vu(a);
    const uint32_t c = rgba_lerp(lwu(OV(0x00161b90)), lwu(OV(0x00161b94)), sn * 0.5f + 0.5f);
    const int32_t lvl = lw(0x0015ed84);
    const uint32_t alpha = (lvl == 0 || lvl == 0x12) ? 0x20u : 0x38u;
    const uint32_t r = c & 0xffu, g = (static_cast<uint32_t>(static_cast<int32_t>(c) >> 8)) & 0xffu, b = (static_cast<uint32_t>(static_cast<int32_t>(c) >> 16)) & 0xffu;
    sw(p + 4, (alpha << 24) | (b << 16) | (g << 8) | r);
    moby_1060_light_mark(m);
    moby_set_color_hi(m, r, b, c >> 24);
}

// Class 809 (the level's scrolling texture timer): at the start the periods 0x16137c / 0x161380
// / 0x161388 from 0x161368.. in frames, with their reciprocals 0x161384 / 0x16138c; then the
// offset 0x161374 runs down by 0x161378 a frame (0x15ed60), wrapped into (-8, 0].
// orig: 0x002a4cd8
void moby_809_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sw(OV(0x00161390), 0);
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        sw(OV(0x0016137c), static_cast<uint32_t>(frame_scale_i(lw(OV(0x00161368)))));
        const int32_t b = frame_scale_i(lw(OV(0x0016136c)));
        sw(OV(0x00161380), static_cast<uint32_t>(b));
        swc1(OV(0x00161384), fpu_div(1.0f, int_to_float(b)));
        const int32_t c = frame_scale_i(lw(OV(0x00161370)));
        sw(OV(0x00161388), static_cast<uint32_t>(c));
        swc1(OV(0x0016138c), fpu_div(1.0f, static_cast<float>(c)));
    } else if (st == 1) {
        float o = lwc1(OV(0x00161374)) - lwc1(OV(0x00161378)) * lwc1(0x0015ed60);
        swc1(OV(0x00161374), o);
        if (o <= -8.0f) {
            o = o + 8.0f;
            swc1(OV(0x00161374), o);
        }
    }
}

// A point along a path (count at +0, points 0x10 apart from +0x10, w the roll): at position t
// (point index + fraction; past the end it wraps, or with `loop` 0 it stops on the last
// segment) the position into `pos`, and unless flags bit 1 the angles into `rot` (0, the pitch
// negated, the heading, the roll negated), each turned from this segment's toward the next
// one's by the fraction.
// orig: 0x00262e40
void path_point(float t, uint32_t path, int32_t loop, uint32_t pos, uint32_t rot, uint32_t flags) {
    bool clamp = false;
    int32_t i = float_to_int(t);
    const int32_t n = lw(path);
    if (loop == 0 && !(i < n - 2)) {
        i = n - 2;
        clamp = true;
    }
    float frac = t - static_cast<float>(i);
    if (clamp && 1.0f < frac) frac = 1.0f;
    int32_t i1 = i + 1, i2 = i + 2;
    if (!(i2 < n)) {
        i1 = ee_rem(i1, n);
        i2 = ee_rem(i2, n);
    }
    const Vec4 p0 = V(path + 0x10 + static_cast<uint32_t>(i) * 0x10);
    const Vec4 p1 = V(path + 0x10 + static_cast<uint32_t>(i1) * 0x10);
    vec3_lerp(V(pos), p0, p1, frac);
    if ((flags & 1u) != 0) return;
    Vec4 d;
    vec3_sub(d, p1, p0);
    const float yaw1 = atan2_f(d.x, d.y);
    float pitch = atan2_f(vec2_length(d), d.z);
    const float pitch1 = pitch;
    float yaw2 = yaw1, roll1 = p0.w, roll2 = 0.0f;
    if (clamp) {
        roll1 = 0.0f;
    } else {
        const Vec4 p2 = V(path + 0x10 + static_cast<uint32_t>(i2) * 0x10);
        vec3_sub(d, p2, p1);
        yaw2 = atan2_f(d.x, d.y);
        pitch = atan2_f(vec2_length(d), d.z);
        roll2 = p1.w;
    }
    sw(rot, 0);
    swc1(rot + 4, -angle_add(angle_sub(pitch, pitch1) * frac, pitch1));
    swc1(rot + 8, angle_add(angle_sub(yaw2, yaw1) * frac, yaw1));
    swc1(rot + 0xc, -angle_add(angle_sub(roll2, roll1) * frac, roll1));
}

// Class 1564 (a flyer on a path 0x1b04b0[pvars +0x60]): at the start the speed (0x161c98 a
// second over the first segment's length, pvars +0x74), whether the path closes (+0x70), the
// position along it (+0x68 of the points, +0x6c) and the scale times 0x161c9c (none: retired);
// then it follows the path (pitch halved), rises by +0x64 and advances, wrapping; when its
// animation looped it flaps (1) while not climbing one time in three, else glides (0).
// orig: 0x002e3a88
void moby_1564_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    const int32_t which = lw(p + 0x60);
    const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(which) * 4);
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        if (which == -1) {
            moby_retire(m);
            return;
        }
        const float d1 = vec3_distance(V(path + 0x10), V(path + 0x20));
        const float d2 = vec3_distance(V(path + 0x10), V(path + lwu(path) * 0x10));
        float sp = lwc1(OV(0x00161c98)) * lwc1(0x0015ed6c);
        swc1(p + 0x74, fpu_div(sp, d1));
        sw(p + 0x70, abs_f(d1 - d2) < u2f(0x3dcccccdu) ? 1u : 0u);
        swc1(p + 0x6c, lwc1(p + 0x68) * static_cast<float>(lw(path)));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        swc1(m + 0x2c, lwc1(m + 0x2c) * lwc1(OV(0x00161c9c)));
    } else if (st != 1) {
        return;
    }
    const float z0 = lwc1(m + 0x18);
    path_point(lwc1(p + 0x6c), path, lw(p + 0x70), m + 0x10, m + 0x40, 0);
    swc1(m + 0x44, lwc1(m + 0x44) * 0.5f);
    swc1(m + 0x18, lwc1(m + 0x18) + lwc1(p + 0x64));
    float t = lwc1(p + 0x6c) + lwc1(p + 0x74);
    swc1(p + 0x6c, t);
    const float n = static_cast<float>(lw(path));
    if (n < t) swc1(p + 0x6c, t - n);
    if ((lbu(m + 0x70) & 2u) == 0) return;
    if (!(z0 < lwc1(m + 0x18)) && rand_mod(3) == 0) {
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 10);
        return;
    }
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, 10);
}

// Class 1413 (Veldin's help triggers): the help messages (0x203908) not shown yet (their counts
// 0x141968..) for the hero in the boxes of pvars +0x10, +0, +4, +8, the first one at once, and,
// when one of the mobys pvars +0x14.. (ids) is gone, the next help by the hero's progress
// 0x1415f8.
// orig: 0x002e0988
void moby_1413_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
    }
    constexpr uint32_t HERO = 0x0013f3d0;
    if (lhu(0x00141980) == 0) sub_00203908(3, 3);
    if (lhu(0x00141d08) == 0 && point_in_box(HERO, lw(p + 0x10)) != 0) sub_00203908(0x4e2a, 0x74);
    if (lhu(0x001419b0) == 0 && point_in_box(HERO, lw(p)) != 0 && lhu(0x00141850) == 0) sub_00203908(5, 9);
    if (lhu(0x001419a8) == 0 && lhu(0x00141848) == 0 && point_in_box(HERO, lw(p + 4)) != 0) sub_00203908(4, 8);
    if (lhu(0x00141968) == 0 && point_in_box(HERO, lw(p + 8)) != 0) sub_00203908(0, 0);
    if (lhu(0x00141970) != 0 || lhu(0x00141978) != 0) return;
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t id = lw(p + 0x14 + k * 4);
        if (id == -1) continue;
        const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(id) << 8);
        if (o != 0) {
            const uint32_t st = lbu(o + 0x20);
            if (st != 0xfe && st != 0xfd) continue;
        }
        if (lw(0x001415f8) < 4) {
            sub_00203908(1, 1);
        } else {
            sub_00203908(2, 2);
        }
    }
}

// A countdown byte: 1 when already 0; else it steps down (at least from 1) and gives 2 when it
// reaches 0, 0 before.
// orig: 0x001fefc8
int32_t countdown_u8(uint32_t p) {
    const uint32_t v = lbu(p);
    if (v == 0) return 1;
    const int32_t n = static_cast<int32_t>(v < 1u ? 1u : v) - 1;
    sb(p, static_cast<uint32_t>(n));
    return n > 0 ? 0 : 2;
}

// Whether a point (w: its weight) of radius r is on screen (VU0): its view-space position by the
// rows 0x16ccc0.. from the camera 0x16ccf0 (both 1024 times world units) and the frustum terms
// 0x16cd00..0x16cd30. -1 behind the near plane or past the far one, 0 outside the sides, 1 in.
// orig: 0x002002e0
int64_t point_on_screen_v(const Vec4& pt, float r) {
    const Vec4 cam = V(OV(0x0016ccf0));
    const Vec4 r0 = V(OV(0x0016ccc0)), r1 = V(OV(0x0016ccd0)), r2 = V(OV(0x0016cce0));
    const Vec4 a = V(OV(0x0016cd00)), b = V(OV(0x0016cd10)), c = V(OV(0x0016cd20)), d = V(OV(0x0016cd30));
    Vec4 p = pt;
    p = Vec4{vuc(p.x * cam.w), vuc(p.y * cam.w), vuc(p.z * cam.w), vuc(p.w * cam.w)};
    const float dx = vuc(p.x - cam.x), dy = vuc(p.y - cam.y), dz = vuc(p.z - cam.z);
    const float vx = vuc(vuc(vuc(r0.x * dx) + vuc(r1.x * dy)) + vuc(r2.x * dz));
    const float vy = vuc(vuc(vuc(r0.y * dx) + vuc(r1.y * dy)) + vuc(r2.y * dz));
    const float vz = vuc(vuc(vuc(r0.z * dx) + vuc(r1.z * dy)) + vuc(r2.z * dz));
    const float rr = vuc(r * cam.w);
    const float near_x = vuc(0.0f + p.w), near_y = vuc(0.0f - p.w);
    const float e8x = vuc(c.x * p.w), e8y = vuc(c.y * p.w);
    const float s1x = vuc(near_x + vz), s1y = vuc(near_y + vz);
    const float e5x = vuc(a.x * vz), e5y = vuc(a.y * vz);
    const float ax = std::fabs(vx), ay = std::fabs(vy);
    const float e7x = vuc(e8x * c.z), e7y = vuc(e8y * c.z);
    const float t3x = vuc(0.0f - s1x), t3y = vuc(rr - s1y);
    const float e9x = vuc(e5x * b.x), e9y = vuc(e5y * b.y);
    const float e6x = vuc(ax - e8x), e6y = vuc(ay - e8y);
    const float f8x = vuc(ax + e7x), f8y = vuc(ay + e7y);
    const float f4x = vuc(s1x - d.x), f4y = vuc(s1y - d.y);
    const float f7x = vuc(e5x - e6x), f7y = vuc(e5y - e6y);
    const float g8x = vuc(e9x - f8x), g8y = vuc(e9y - f8y);
    if (std::signbit(t3y) || !std::signbit(t3x)) return -1;
    if (std::signbit(f7y) || std::signbit(f7x)) return -1;
    if (std::signbit(g8y) || std::signbit(f4y)) return 0;
    if (std::signbit(g8x) || !std::signbit(f4x)) return 0;
    return 1;
}
// orig: 0x002002e0 (EE-address form)
int64_t point_on_screen(uint32_t pt, float r) { return point_on_screen_v(V(pt), r); }

// The members of a moby group (lists 0x1ab840[group], halfword ids, the last with bit 15),
// walked with the cursor 0x16010c (entry), 0x160110 (id), 0x160114 (moby): `out` gets each in
// turn until one passes the filter (a, b): (0, 0) a live one, (1, 0) any, (1, 1) a dead one,
// (0, 1) none. 0 when `out` holds one, -1 at the end (or for a group past 0x15fff4).
// orig: 0x002591d0
int32_t group_first_r(uint32_t& out, int32_t group, int32_t a, int32_t b) {
    if (group < 0 || lw(OV(0x0015fff4)) < group) {
        out = 0;
        return -1;
    }
    out = 0;
    sw(OV(0x00160114), 0);
    const uint32_t list = lwu(OV(0x001ab840) + static_cast<uint32_t>(group) * 4);
    sw(OV(0x0016010c), list);
    if (list == 0) return -1;
    const uint32_t id = lhu(list) & 0x7fffu;
    sh(OV(0x00160110), id);
    const uint32_t m = lwu(OV(0x0015ffd8)) + (id << 8);
    sw(OV(0x00160114), m);
    out = m;
    const bool dead = static_cast<int8_t>(lbu(m + 0x20)) < 0;
    if (a != 0) {
        if (b == 0 || dead) return 0;
    } else if (b == 0 && !dead) {
        return 0;
    }
    return group_next_r(out, lwu(OV(0x00160114)), a, b);
}
// orig: 0x002591d0 (EE-address form)
int32_t group_first(uint32_t out, int32_t group, int32_t a, int32_t b) {
    uint32_t o = 0;
    const int32_t r = group_first_r(o, group, a, b);
    sw(out, o);
    return r;
}

// The next member after `cur` (see group_first); from another moby the walk restarts at its
// group's list and goes on past it.
// orig: 0x002592b8
int32_t group_next_r(uint32_t& out, uint32_t cur, int32_t a, int32_t b) {
    out = 0;
    uint32_t e;
    if (cur == lwu(OV(0x00160114))) {
        e = lwu(OV(0x0016010c));
        if (lh(e) < 0) return -1;
    } else {
        if (lw(OV(0x0015fff4)) < static_cast<int32_t>(lbu(cur + 0x21))) return -1;
        sw(OV(0x00160114), 0);
        const uint32_t list = lwu(OV(0x001ab840) + lbu(cur + 0x21) * 4);
        sw(OV(0x0016010c), list);
        if (list == 0) return -1;
        sw(OV(0x0016010c), list - 2);
        for (;;) {
            const uint32_t q = lwu(OV(0x0016010c));
            e = q + 2;
            sw(OV(0x0016010c), e);
            const uint32_t id = lhu(q + 2) & 0x7fffu;
            sh(OV(0x00160110), id);
            const uint32_t m = lwu(OV(0x0015ffd8)) + (id << 8);
            sw(OV(0x00160114), m);
            if (lh(q + 2) < 0) return -1;
            if (cur == m) break;
        }
    }
    for (;;) {
        sw(OV(0x0016010c), e + 2);
        const uint32_t id = lhu(e + 2) & 0x7fffu;
        sh(OV(0x00160110), id);
        const uint32_t m = lwu(OV(0x0015ffd8)) + (id << 8);
        sw(OV(0x00160114), m);
        out = m;
        const bool dead = static_cast<int8_t>(lbu(m + 0x20)) < 0;
        if (a != 0) {
            if (b == 0 || dead) return 0;
        } else if (b == 0 && !dead) {
            return 0;
        }
        e = lwu(OV(0x0016010c));
        if (lh(e) < 0) {
            out = 0;
            return -1;
        }
    }
}
// orig: 0x002592b8 (EE-address form)
int32_t group_next(uint32_t out, uint32_t cur, int32_t a, int32_t b) {
    uint32_t o = 0;
    const int32_t r = group_next_r(o, cur, a, b);
    sw(out, o);
    return r;
}

// Class 500's companion 0x326 for crate kind 0x1f5: spawned at the crate (state 0, +0x30 0x40,
// flags 0x41, colour 0; pvars +0xc the crate). Returns it (0 if none or the crate is gone).
// orig: 0x002d8138
uint32_t crate_spawn_companion(uint32_t m) {
    if (static_cast<int8_t>(lbu(m + 0x20)) < 0) return 0;
    const uint32_t c = moby_spawn(0x326);
    if (c == 0) return 0;
    sb(c + 0x30, 0x40);
    sb(c + 0x20, 0);
    sw(lwu(c + 0x78) + 0xc, m);
    sw(c + 0x94, 0);
    sh(c + 0x34, lhu(c + 0x34) | 0x41u);
    lq_sq(c + 0x10, m + 0x10);
    return c;
}

// The moby's hit state record (pvars +0x10) when it takes hits (flag 0x20).
// orig: 0x0025c368
uint32_t moby_hit_state(uint32_t m) {
    if (m == 0 || (lhu(m + 0x34) & 0x20u) == 0) return 0;
    return lwu(lwu(m + 0x78) + 0x10);
}

// A moby's reaction to the hit record `hit` (+0x10 push, +0x1c 5627.95 once applied, +0x20 the
// attacker, +0x28 the damage kind, +0x29 its strength, +0x2a the weapon class, +0x2c damage):
// the push scaled by the table 0x1b01c8[kind][hit +0x28] (a wrench, 0x47, pushes away from the
// hero); the reaction 0..0x11 by damage kind and strength, its animation from 0x1b0180[state
// +8][reaction] (returned), the state +0x1c / +0x14 / +0x16 (repeat timer, last weapon), the
// damage zeroed when the hit does not count (by `mask`), and *state 0 / 1 (hit) / 2..4.
// orig: 0x0025a478
int32_t moby_hit_react_r(uint32_t m, uint32_t hit, uint32_t hs, uint32_t mask, int32_t& state, float* dmg, int32_t* weapon, int32_t kind) {
    state = 0;
    if (weapon != nullptr) *weapon = -1;
    if (hit != 0 && lwu(hit + 0x20) != 0) {
        const int32_t c = lh(lwu(hit + 0x20) + 0xa6);
        if ((c == 0x131 || c == 0xa8) && hs != 0) swc1(hit + 0x2c, lwc1(OV(0x00160080) + lbu(hs + 8) * 4));
    }
    if (m == 0) {
        if (hs != 0) {
            sh(hs + 0x1c, 0);
            sh(hs + 0x14, 0xffff);
        }
        sh(hs + 0x16, 0xffff);
        state = 0;
        return 0;
    }
    if (hs == 0) {
        hs = moby_pvar_word0(m);
        if (hs == 0) {
            state = 0;
            return 0;
        }
    }
    if (kind == 4) kind = static_cast<int32_t>(lbu(hs + 8));
    if (hit == 0) {
        timer_tick_i16(ee_ref<int16_t>(hs + 0x1c));
        sh(hs + 0x16, 0xffff);
        state = 1;
        if (dmg != nullptr) *dmg = 0.0f;
        return 0xb;
    }
    const float applied = u2f(0x45afdf66u);
    uint32_t weap;
    if (lwc1(hit + 0x1c) == applied) {
        const uint32_t push = hit + 0x10;
        vec3_scale(V(push), V(push), lwc1(OV(0x001b01c8) + lbu(hit + 0x28) * 4 + static_cast<uint32_t>(kind << 5)));
        weap = lhu(hit + 0x2a);
        swc1(hit + 0x1c, applied);
        if (weap == 0x47) {  // the wrench: away from the hero
            constexpr uint32_t P = 0x0013f350;
            Vec4 away{};
            away.x = cos_vu(atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84)));
            away.y = sin_vu(atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84)));
            away.z = 1.0f;
            vec3_set_length(away, away, vec3_length(V(push)) * u2f(0x3f333333u));
            vec3_scale(V(push), V(push), u2f(0x3e99999au));
            Vec4 sum;
            vec3_add(sum, V(push), away);
            V(push) = sum;
            vec2_set_length(V(push), V(push), 1.0f);
        }
    } else {
        const uint32_t att = lwu(hit + 0x20);
        weap = att != 0 ? static_cast<uint32_t>(static_cast<int32_t>(lh(att + 0xa6))) : 0x47u;
        sb(hit + 0x28, 8);
    }
    const uint32_t dk = lbu(hit + 0x28), str = lbu(hit + 0x29);
    int32_t react = 1, st = 2;
    bool store = true;
    switch (dk) {
        case 0:
            if (lhu(hit + 0x2a) - 1u < 2u) react = 0;
            break;
        case 1:
            if (str < 2) {
                react = 0xb;
            } else {
                state = 2;
                store = false;
                react = str == 2 ? 5 : 9;
            }
            break;
        case 2: react = str < 2 ? 8 : 0xd; break;
        case 3:
            if (str < 2) {
                react = 0xa;
            } else if (str < 3) {
                react = 0xf;
            } else {
                state = 2;
                store = false;
                react = 0x10;
                sh(hs + 0x14, 0xffff);
                sh(hs + 0x1c, 0);
            }
            break;
        case 4: react = 0xe; break;
        case 5:
            if (str < 2) {
                st = 3;
                react = 4;
            } else {
                st = 4;
                react = 7;
            }
            break;
        case 7: react = str < 2 ? 0xc : 0x11; break;
        default: break;
    }
    if (store) state = st;
    if (weapon != nullptr) *weapon = static_cast<int32_t>(weap);
    const uint32_t r = static_cast<uint32_t>(react);
    int32_t anim = lbu(OV(0x001b0180) + lbu(hs + 8) + r * 4);
    if (lh(hs + 0x14) != static_cast<int32_t>(weap)) sh(hs + 0x1c, 0);
    if (timer_tick_i16(ee_ref<int16_t>(hs + 0x1c)) != 0) {
        int32_t t = 0x1e;
        switch (react) {
            case 0: t = 0x25; break;
            case 1: t = 0xf; break;
            case 4: t = 0x3c; break;
            case 7: t = lbu(0x0013e533) != 0 ? 0x2d : 0x3c; break;
            case 11: t = 0x3c; break;
            default: break;
        }
        sh(hs + 0x1c, static_cast<uint32_t>(frame_scale_i(t)));
    } else {
        uint32_t keep = 0;
        bool test = true;
        switch (react) {
            case 0: case 1:
                if ((mask & 0x10u) == 0) anim = 0xb;
                keep = mask & 0x20u;
                break;
            case 4:
                if ((mask & 1u) == 0) anim = 0xb;
                keep = mask & 2u;
                break;
            case 7:
                if ((mask & 4u) == 0) anim = 0xb;
                keep = mask & 8u;
                break;
            case 11:
                anim = 0xc;
                test = false;
                break;
            default:
                sw(hit + 0x2c, 0);
                anim = 0xd;
                test = false;
                break;
        }
        if (test) {
            if (keep == 0) state = 1;
            sw(hit + 0x2c, 0);
        }
    }
    sh(hs + 0x16, weap);
    sh(hs + 0x14, weap);
    if (dmg != nullptr) *dmg = lwc1(hit + 0x2c);
    const uint32_t h = moby_hit_state(m);
    if (h != 0) sb(h + 0x2e, react == 4 ? 1u : 0u);
    return anim;
}

// orig: 0x0025a478 (EE-address form: the outputs are addresses, 0 for none)
int32_t moby_hit_react(uint32_t m, uint32_t hit, uint32_t hs, uint32_t mask, uint32_t state, uint32_t dmg, uint32_t weapon, int32_t kind) {
    int32_t st = lw(state), w = weapon != 0 ? lw(weapon) : 0;
    float d = dmg != 0 ? lwc1(dmg) : 0.0f;
    const int32_t r = moby_hit_react_r(m, hit, hs, mask, st, dmg != 0 ? &d : nullptr, weapon != 0 ? &w : nullptr, kind);
    sw(state, static_cast<uint32_t>(st));
    if (weapon != 0) sw(weapon, static_cast<uint32_t>(w));
    if (dmg != 0) swc1(dmg, d);
    return r;
}

// Where the segment from -> to (in the xy plane) first crosses the wall path 0x1b04b0[path]
// (its segments with a point w set): `out` the point at that fraction (from -> to over the
// whole segment when none). 1 when it crosses.
// orig: 0x00261b48
int32_t path_crossing_v(int32_t path, const Vec4& from, const Vec4& to, Vec4& out) {
    Mat4 basis{};
    vec3_sub(basis.r[0], to, from);
    basis.r[0].z = 0.0f;
    const float len = vec3_length(basis.r[0]);
    vec3_set_length(basis.r[0], basis.r[0], fpu_div(1.0f, len));
    vec_clear(basis.r[1]);
    basis.r[1].x = basis.r[0].y;
    basis.r[1].y = -basis.r[0].x;
    vec_clear(basis.r[2]);
    basis.r[2].z = 1.0f;
    const uint32_t pts = lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4);
    Vec4 p;
    vec3_sub(p, V(pts + 0x10), from);
    mat3_transform(p, p, basis);
    float best = 1.0f;
    int32_t found = 0;
    for (int32_t i = 1; i < lw(lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4)); ++i) {
        Vec4 q;
        vec3_sub(q, V(lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4) + 0x10 + static_cast<uint32_t>(i) * 0x10), from);
        mat3_transform(q, q, basis);
        if ((p.w != 0.0f || q.w != 0.0f) && q.y * p.y < 0.0f) {
            float t = fpu_div(p.x - q.x, p.y - q.y);
            t = t * -q.y;
            t = t + q.x;
            if (0.0f < t && t < best) {
                best = t;
                found = 1;
            }
        }
        p = q;
    }
    vec3_lerp(out, from, to, best);
    return found;
}
// orig: 0x00261b48 (EE-address form)
int32_t path_crossing(int32_t path, uint32_t from, uint32_t to, uint32_t out) {
    Vec4 o = V(out);
    const int32_t r = path_crossing_v(path, V(from), V(to), o);
    V(out) = o;
    return r;
}

// Spawns the pickup of item `item` (class 0x179ac0[item].+0x3a) at `pos`: its amount (`count`,
// or the item's default 0x1c40b0[item].+0xc), the item, `n` (as a float), a 16-frame timer, its
// start height, placed and lit like the hero. Returns it (0: none).
// orig: 0x002c52a8
uint32_t pickup_spawn(uint32_t pos, uint32_t item, int32_t count, int32_t n) {
    uint32_t m = 0;
    if (0 < lh(OV(0x00179ac0) + item * 0x4c + 0x3a)) m = moby_spawn(static_cast<uint32_t>(lh(OV(0x00179ac0) + item * 0x4c + 0x3a)));
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x20, 1);
    const uint32_t p = lwu(m + 0x78);
    sb(m + 0x31, 1);
    sb(m + 0xbc, 0);
    lq_sq(m + 0x10, pos);
    sw(p, count == -1 ? lhu(OV(0x001c40b0) + item * 0x18 + 0xc) : static_cast<uint32_t>(count));
    sw(p + 8, item);
    swc1(p + 0xc, static_cast<float>(n));
    sw(p + 0x20, static_cast<uint32_t>(frame_scale_i(0x10)));
    swc1(p + 0x28, lwc1(m + 0x18));
    moby_update_transform(m);
    moby_copy_hero_color(m);
    return m;
}

// The ammo a crate gives (*kind; picked again when unset or the crate's own drop flag
// 0x14c190[level][+0xb2] is set): one of the owned weapons 0x15edd0.. (owned 0x13d4c0) short of
// their ammo 0x1c40b0[w].+0xe (held 0x13d428), else any owned weapon with ammo, else 10.
// Returns 1 (4 in 5) or 2.
// orig: 0x00257218
int32_t crate_ammo_kind_r(uint32_t m, int32_t& kind) {
    bool pick = kind <= 0;
    if (!pick && m != 0) {
        const uint32_t b = lhu(m + 0xb2);
        const int32_t word = static_cast<int32_t>(static_cast<int16_t>(b)) >> 5;
        pick = ((lw(0x0014c190 + static_cast<uint32_t>(word) * 4 + lwu(0x0015ed84) * 0x100) >> (b & 0x1f)) & 1) != 0;
    }
    if (pick) {
        auto owned = [](uint32_t i) { return lbu(0x0013d4c0 + (lbu(0x0015edd0 + i) & 0x3fu)) != 0; };
        auto maxammo = [](uint32_t i) { return lhu(OV(0x001c40b0) + (lbu(0x0015edd0 + i) & 0x3fu) * 0x18 + 0xe); };
        auto ammo = [](uint32_t i) { return lw(0x0013d428 + (lbu(0x0015edd0 + i) & 0x3fu) * 4); };
        int32_t with_max = 0, own = 0, short_of = 0;
        if (lbu(0x0015edd0) != 0xff) {
            for (uint32_t i = 0;;) {
                if (owned(i)) {
                    ++own;
                    if (maxammo(i) != 0) ++with_max;
                    short_of += ammo(i) < static_cast<int32_t>(maxammo(i)) ? 1 : 0;
                }
                ++i;
                if (!(i < 0xc) || lbu(0x0015edd0 + i) == 0xff) break;
            }
        }
        int32_t idx = -1;
        uint32_t v = 10;
        if (short_of != 0) {
            int32_t r = rand_mod(short_of);
            if (!(r < 0)) {
                idx = 0;
                for (;;) {
                    if (owned(static_cast<uint32_t>(idx)) && ammo(static_cast<uint32_t>(idx)) < static_cast<int32_t>(maxammo(static_cast<uint32_t>(idx)))) --r;
                    if (r < 0) break;
                    ++idx;
                }
            }
            v = lbu(0x0015edd0 + static_cast<uint32_t>(idx)) & 0x3fu;
        } else if (own != 0) {
            int32_t r = rand_mod(with_max);
            if (!(r < 0)) {
                idx = 0;
                for (;;) {
                    if (owned(static_cast<uint32_t>(idx)) && maxammo(static_cast<uint32_t>(idx)) != 0) --r;
                    if (r < 0) break;
                    ++idx;
                }
            }
            v = lbu(0x0015edd0 + static_cast<uint32_t>(idx)) & 0x3fu;
        }
        kind = static_cast<int32_t>(v);
    }
    return rand_mod(5) != 0 ? 1 : 2;
}
// orig: 0x00257218 (EE-address form)
int32_t crate_ammo_kind(uint32_t m, uint32_t kind) {
    int32_t k = lw(kind);
    const int32_t r = crate_ammo_kind_r(m, k);
    sw(kind, static_cast<uint32_t>(k));
    return r;
}

// A bolt flies off toward `target`: at most one jingle (sound 0) every 3 frames (0x1613ac), else
// a random yaw (+-30) and pitch (0..30 degrees) when not given; on a bounce (state 3) it springs
// up 3..7; its turn axis (velocity x the way to the target) and a turn of 0x1613a8 * (1/3..1)
// degrees a frame (pvars +0x30), the target +0x60; state 4. 0 when already flying.
// orig: 0x002a7210
int32_t bolt_fly_to(uint32_t m, uint32_t target, float yaw, float pitch) {
    const uint32_t p = lwu(m + 0x78);
    if (lbu(m + 0x20) == 4) return 0;
    const int32_t t = frame_scale_i(3);
    const int32_t last = lw(OV(0x001613ac)), now = lw(OV(0x0015f5cc));
    if (last + t < now || now < last) {
        sw(OV(0x001613ac), static_cast<uint32_t>(now));
        moby_sound_start(0, 0x20, m);
    } else {
        if (yaw == 0.0f) yaw = rand_range_f(-30.0f, 30.0f) * u2f(0x3c8efa35u);
        if (pitch == 0.0f) pitch = rand_range_f(0.0f, 30.0f) * u2f(0x3c8efa35u);
    }
    if (lbu(m + 0x20) == 3) {
        Mat4 rot;
        euler_to_mat3_vu(rot, V(p + 0x10));
        const float up = rand_range_f(3.0f, 7.0f);
        vec3_set_length(V(p + 0x20), rot.r[2], up * lwc1(0x0015ed6c));
    }
    swc1(p + 0x4c, yaw);
    swc1(p + 0x48, pitch);
    sw(p + 0x64, 0);
    sw(p + 0x60, target);
    Vec4 to;
    vec3_sub(to, V(target + 0x10), V(m + 0x10));
    Vec4 axis;
    vec3_cross_ba(axis, V(p + 0x20), to);
    vec3_set_length(V(p + 0x30), axis, 1.0f);
    const float rate = lwc1(OV(0x001613a8));
    float a = rand_range_f(rate * u2f(0x3eaa7efau), rate) * u2f(0x3c8efa35u);
    a = a * lwc1(0x0015ed6c);
    quat_axis_angle(V(p + 0x30), V(p + 0x30), a);
    sb(m + 0x20, 4);
    return 1;
}

// Spawns a bolt of `value` (class 0xd: 1, 0xe: 5, 0xf: 20, 0x10: 50, scaled 0.65..0.75 of its
// class) at `pos` from the moby `src` (its root parent kept at +0xb8; lit like it, or by its
// light record), heading for `dir` with a random spin and life (up to 600 frames); with flags
// bit 2 it flies to the hero at once (bit 0x10: a random yaw). None spawned: the value goes to
// the bolt count 0x15ed98. Returns the bolt (0: none).
// orig: 0x002a7438
uint32_t bolt_spawn_v(uint32_t src, const Vec4& pos, const Vec4& dir, uint32_t flags, int32_t value, uint32_t tag) {
    uint32_t m;
    float sc;
    if (value == 5) {
        m = moby_spawn(0xe);
        sc = u2f(0x3f333333u);
    } else if (value < 6) {
        m = moby_spawn(0xd);
        sc = u2f(0x3f266666u);
    } else if (value == 0x14) {
        m = moby_spawn(0xf);
        sc = 0.75f;
    } else if (value == 0x32) {
        m = moby_spawn(0x10);
        sc = u2f(0x3f19999au);
    } else {
        m = moby_spawn(0xd);
        sc = u2f(0x3f266666u);
    }
    if (m == 0) {
        sw(0x0015ed98, static_cast<uint32_t>(lw(0x0015ed98) + value));
        return 0;
    }
    const uint32_t p = lwu(m + 0x78);
    sb(p + 0x40, tag);
    uint32_t root = src;
    if (src != 0 && lwu(src + 0xb8) != 0) {
        root = lwu(src + 0xb8);
        while (lwu(root + 0xb8) != 0) root = lwu(root + 0xb8);
    }
    sh(m + 0x34, lhu(m + 0x34) | 0x100u);
    sw(m + 0xb8, root);
    sb(m + 0x73, 0x14);
    sb(m + 0x30, 0x40);
    sh(m + 0x32, 0x40);
    sb(m + 0x20, 1);
    sb(m + 0x31, 1);
    swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * sc);
    euler_to_mat3_vu(M(m + 0xc0), V(m + 0x40));
    const uint32_t light = (lhu(src + 0x34) & 0x20u) != 0 ? lwu(lwu(src + 0x78) + 0xc) : 0u;
    if (light != 0) {
        moby_set_color_hi(m, lbu(light + 4), lbu(light + 5), lbu(light + 6));
    } else {
        sd(m + 0x38, ld(src + 0x38));
    }
    sh(p + 0x6e, static_cast<uint32_t>(rand_range(0, frame_scale_i(0x258))));
    sh(p + 0x6c, 0xffff);
    V(m + 0x10) = pos;
    V(p + 0x20) = dir;
    if (rand_mod(2) != 0) sb(p + 0x55, lbu(p + 0x55) | 1u);
    swc1(p + 0x50, rand_angle());
    swc1(p + 0x58, rand_angle());
    swc1(p + 0x68, lwc1(m + 0x18));
    float spin = rand_range_f(0.0f, lwc1(OV(0x001613a8))) * u2f(0x3c8efa35u);
    spin = spin * lwc1(0x0015ed6c);
    Vec4 away;
    vec3_sub(away, V(src + 0x10), V(m + 0x10));
    Vec4 axis;
    vec3_cross_ba(axis, V(p + 0x20), away);
    vec3_set_length(V(p + 0x30), axis, 1.0f);
    quat_axis_angle(V(p + 0x30), V(p + 0x30), spin);
    if ((flags & 2u) != 0) {
        float yaw = 0.0f;
        if ((flags & 0x10u) != 0) yaw = rand_range_f(-30.0f, 30.0f) * u2f(0x3c8efa35u);
        bolt_fly_to(m, lwu(0x001413d0), yaw, 0.0f);
    }
    moby_update_transform(m);
    return m;
}
// orig: 0x002a7438 (EE-address form)
uint32_t bolt_spawn(uint32_t src, uint32_t pos, uint32_t dir, uint32_t flags, int32_t value, uint32_t tag) {
    return bolt_spawn_v(src, V(pos), V(dir), flags, value, tag);
}

// A moby drops lo..hi bolts' worth (doubled with 0x15eea0 / 0x15ee20; with the drop rate
// 0x15ee2c / 0x15ee28 at a 60 Hz frame the amount is scaled up (to 5x) or down (to 1/10), at
// most 500 (1000)) as 50 / 20 / 5 / 1 bolts; with flags 8 the pieces are regrouped to about
// three. Each bolt starts half a unit up, thrown 0..3 out and 3.7..6 up a frame (plus the
// moby's own velocity with flags 4), kept inside the wall path `path` when one is given; the
// bolts fly to the hero (bolt_spawn flags) when 0x15f598 says the moby is not a respawning one.
// orig: 0x00260a88
void bolts_drop(uint32_t m, int32_t lo, int32_t hi, uint32_t flags, int32_t path) {
    Vec4 base{};
    uint32_t v = lwu(OV(0x0015f598));
    if ((flags & 0x20u) == 0 && lbu(m + 0xb0) != 0xff && !(lhu(m + 0xa6) - 0x1f4u < 0x29u)) {
        sw(OV(0x0015f598), lbu(OV(0x0015fc88) + lbu(m + 0xb0)) == 0xff ? 1u : 0u);
        v = lwu(OV(0x0015f598));
    }
    const uint32_t to_hero = v != 0 ? ((flags & 0x20u) == 0 ? 1u : 0u) : 0u;
    if (flags == 0) return;
    if (lo == 0 && hi == 0) return;
    if ((flags & 4u) != 0) {
        const uint32_t h = moby_pvar_word0(m);
        if (h != 0) base = V(h + 0x20);
    }
    if (!(0 < lo)) lo = 1;
    int32_t n = rand_mod(hi - lo + 1) + lo;
    const bool twice = lw(0x0015eea0) != 0 || lw(0x0015ee20) != 0;
    if (twice) n = n << 1;
    if (n < 0x1f4 && to_hero != 0 && lw(0x0015ee28) != 0) {
        const float f = static_cast<float>(frame_scale_i(0x3c)) * 60.0f;
        int32_t r = ee_div(static_cast<int32_t>(static_cast<uint32_t>(float_to_int(f)) * lwu(0x0015ee2c)), lw(0x0015ee28));
        if (twice) r = r / 2;
        if (r < 0x96) {
            if (r < 0x5b) {
                debug_printf_stub();
                n = n * 5;
            } else {
                const int32_t k = (0xbe - r) / 0x14;
                debug_printf_stub();
                n = n * k;
            }
            if (lw(0x0015eea0) != 0) {
                if (!(n < 0x3e9)) n = 0x3e8;
            } else if (!(n < 0x1f5)) {
                n = 0x1f4;
            }
        } else if (!(r < 0x12d)) {
            if (!(r < 0x160)) {
                debug_printf_stub();
                n = n / 10;
            } else {
                const int32_t k = 0x168 - r;
                debug_printf_stub();
                n = n * (k / 0x50);
            }
            if (!(0 < n)) n = 1;
        }
    }
    int32_t n50 = 0, n20 = 0, n5 = 0, n1;
    int32_t rest = n;
    while (!(rest < 300)) {
        rest -= 0x32;
        ++n50;
    }
    while (!(rest < 200)) {
        rest -= 0x14;
        ++n20;
    }
    while (!(rest < 0x1e)) {
        rest -= 0x19;
        ++n20;
        ++n5;
    }
    while (!(rest < 7)) {
        rest -= 5;
        ++n5;
    }
    n1 = rest;
    if (!(n1 + n5 + n20 + n50 < 4) && (flags & 8u) != 0) {  // regroup into few pieces
        int32_t a50 = 3, a20 = 0, a5 = 0, a1 = 0, totA = 0x96;
        int32_t b1 = 3, b5 = 0, b20 = 0, b50 = 0, totB = 3;
        if (n < 0x96) {
            int32_t v50 = 0x96, v20 = 0, v5 = 0;
            for (;;) {
                if (a50 != 0) {
                    v50 -= 0x32;
                    --a50;
                    v20 += 0x14;
                    ++a20;
                } else if (a20 != 0) {
                    v20 -= 0x14;
                    --a20;
                    v5 += 5;
                    ++a5;
                } else if (a5 != 0) {
                    v5 -= 5;
                    --a5;
                    ++a1;
                }
                totA = v50 + v20 + v5 + a1;
                if (!(n < totA) || totA < 4) break;
            }
        }
        if (3 < n) {
            int32_t v20 = b20 * 0x14, v50 = b50 * 0x32, v5 = b5 * 5;
            for (;;) {
                if (b1 != 0) {
                    --b1;
                    v5 += 5;
                    ++b5;
                } else if (b5 != 0) {
                    v5 -= 5;
                    --b5;
                    v20 += 0x14;
                    ++b20;
                } else if (b20 != 0) {
                    v20 -= 0x14;
                    --b20;
                    v50 += 0x32;
                    ++b50;
                }
                totB = v50 + v20 + v5 + b1;
                if (!(totB < n) || !(totB < 0x96)) break;
            }
        }
        if (totA - n < n - totB) {
            n1 = a1;
            n5 = a5;
            n20 = a20;
            n50 = a50;
        } else {
            n1 = b1;
            n5 = b5;
            n20 = b20;
            n50 = b50;
        }
    }
    while (n1 != 0 || n5 != 0 || n20 != 0 || n50 != 0) {
        Vec4 pos = V(m + 0x10);
        pos.z = pos.z + 0.5f;
        const float a = rand_angle();
        Vec4 vel{};
        vel.x = cos_vu(a) * (rand_range_f(0.0f, 3.0f) * lwc1(0x0015ed6c));
        vel.y = sin_vu(a) * (rand_range_f(0.0f, 3.0f) * lwc1(0x0015ed6c));
        vel.z = 0.0f;
        vel.z = rand_range_f(u2f(0x406ccccdu), 6.0f) * lwc1(0x0015ed6c);
        // (w: an unset stack lane in the original)
        Vec4 side = Vec4{vel.x, vel.y, 0.0f, 0.0f};
        vec3_set_length(side, side, 0.5f);
        vec3_add(pos, pos, side);
        if (!(path < 0)) {  // keep the bolt's flight inside the wall path
            const uint32_t pts = lwu(OV(0x001b04b0) + static_cast<uint32_t>(path) * 4);
            const float half_g = lwc1(0x0015ed70) * u2f(0x412ccccdu) * -0.5f;
            float r_hi = 0.0f, r_lo = 0.0f;
            float t = 120.0f;
            const int32_t roots = solve_quadratic(&r_hi, &r_lo, half_g, vel.z - half_g, pos.z - lwc1(pts + 0x18));
            if (1.0f <= static_cast<float>(roots) && 0.0f < r_hi && r_hi < t) t = r_hi;
            Vec4 end;
            vec3_scale(end, vel, t);
            vec3_add(end, end, V(m + 0x10));
            Vec4 hit{};
            path_crossing_v(path, V(m + 0x10), end, hit);
            vec3_sub(end, hit, pos);
            end.z = 0.0f;
            Vec4 back;
            vec3_set_length(back, end, 0.25f);
            vec3_sub(end, end, back);
            vec3_scale(end, end, fpu_div(1.0f, t));
            vel.x = end.x;
            vel.y = end.y;
        }
        vec3_add(vel, vel, base);
        int32_t value;
        if (n50 != 0) {
            value = 0x32;
            --n50;
        } else if (n20 != 0) {
            value = 0x14;
            --n20;
        } else if (n5 != 0) {
            value = 5;
            --n5;
        } else {
            value = 1;
            --n1;
        }
        bolt_spawn_v(m, pos, vel, flags, value, to_hero);
    }
}

// A broken moby's bolts: its drop recorded (0x14c190[level] / 0x1ba4d0 bits by +0xb2, the slot
// bytes 0x1baa24 / 0x1bb684), the amount +0xb4 (or, for a respawning group +0xb1, what is left
// of +0xb6 over the last count 0x14d592), dropped as bolts_drop(amount -+ a quarter) with the
// flags for the hero's state, the bolt count 0x15ffbc and `flags`.
// orig: 0x00257470
void moby_drop_bolts(uint32_t m, uint32_t flags, int32_t path) {
    float amount = static_cast<float>(lh(m + 0xb4));
    debug_printf_stub();
    if (lbu(m + 0xb1) == 0xfe) return;
    const uint32_t b2 = lhu(m + 0xb2);
    if (static_cast<int16_t>(b2) < 0) return;
    const uint32_t level = lwu(0x0015ed84);
    const uint32_t word = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(b2)) >> 5) * 4;
    sw(0x0014c190 + word + level * 0x100, lwu(0x0014c190 + word + level * 0x100) | (1u << (b2 & 0x1f)));
    sw(OV(0x001ba4d0) + word, lwu(OV(0x001ba4d0) + word) | (1u << (b2 & 0x1f)));
    const uint32_t idx = static_cast<uint32_t>(static_cast<int32_t>(lh(m + 0xb2)));
    sb(OV(0x001baa24) + idx, lbu(m + 0xb0) + 2);
    const uint32_t b0 = lbu(m + 0xb0);
    if (b0 == 0xff || (lbu(OV(0x0015fc88) + b0) != 0xff && lbu(0x0014c050 + level * 0x10 + b0) == 0xff)) sb(OV(0x001bb684) + idx, b0 + 2);
    const int8_t grp = static_cast<int8_t>(lbu(m + 0xb1));
    if (!(grp < 0)) {
        const int32_t total = lh(m + 0xb6);
        int32_t left = total - lh(0x0014d590 + static_cast<uint32_t>(grp) * 4 + level * 0x100 + 2);
        if (!((total + 1) / 2 < left)) left = 0;
        if (left != 0) amount = static_cast<float>(left);
        if (amount < 1.0f) amount = 1.0f;
        debug_printf_stub();
    }
    uint32_t f = (flags & 0x100u) != 0 ? 5u : 1u;
    if (lbu(0x00141402) != 0 || (flags & 0x200u) != 0) f |= 2u;
    if (lw(0x001413d4) == 0x10 && vec3_distance(V(0x0013f3d0), V(m + 0x10)) < 4.0f) f |= 2u;
    const int32_t bolts = lw(OV(0x0015ffbc));
    if ((flags & 0x400u) != 0) f |= 0x10u;
    if (bolts < 100 || (flags & 0x800u) != 0) f |= 8u;
    if (!(0x31 < bolts)) f |= 2u;
    if (amount < 0.0f) amount = 0.0f;
    if (!(0.0f < amount)) return;
    int32_t spread = 0;
    if (8.0f <= amount) {
        spread = float_to_int(amount * 0.25f);
    } else if (2.0f <= amount) {
        spread = 1;
    }
    const int32_t c = float_to_int(amount);
    bolts_drop(m, c - spread, c + spread, f, path);
}

// A crate breaks: its contents. A crate (not 0x1f5) whose +0xc6 is clear just drops its bolts
// (riding a moby +0xf0: zone 0x7f80, the frame transform computed; on level 0xf the crate at
// (166, 193) drops them with no wall path and 0x15f598 cleared for the call). Otherwise the
// pickups of its kind +0xcb (crate_ammo_kind: 1 or 2) fly out at a random angle and speed, each
// aimed, when it has a wall path +0xc0, to land before it (the fall time to the path's height,
// capped at 120 frames); then the bolts (+0xb4 cleared first).
// orig: 0x002d0538
void crate_break_drops(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const bool plain = lh(pv + 0xc6) == 0 && lh(m + 0xa6) != 0x1f5;
    if (plain) {
        Vec4 d;
        vec_clear(d);
        uint32_t level = lwu(0x0015ed84);
        if (lw(pv + 0xf0) != 0) {
            if (m != 0) sh(m + 0x36, 0x7f80);
            Vec4 pos, ang;
            sub_00260400_v(m, lwu(pv + 0xf0), V(m + 0x10), V(m + 0x40), pos, ang);
            vec3_sub(d, pos, V(m + 0x10));
            level = lwu(0x0015ed84);
        }
        if (level == 0xf && abs_f(lwc1(m + 0x10) - 166.0f) < 6.0f && abs_f(lwc1(m + 0x14) - 193.0f) < 6.0f) {
            const uint32_t saved = lwu(OV(0x0015f598));
            sw(OV(0x0015f598), 0);
            moby_drop_bolts(m, 0x100, -1);
            sw(OV(0x0015f598), saved);
            return;
        }
        moby_drop_bolts(m, 0x100, lw(pv + 0xc0));
        return;
    }
    if (lh(m + 0xa6) != 0x1f5) {
        static constexpr int32_t kinds[12] = {-1, 0xa, 0x10, 0xf, 0x14, 0x11, 0xb, 0x18, 0xd, 0x19, 0x13, 0x17};
        int32_t kind = -1;
        const uint32_t k = lbu(pv + 0xcb);
        if (k < 0xc) kind = kinds[k];
        const int32_t n = crate_ammo_kind_r(m, kind);
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t pk = pickup_spawn(m + 0x10, static_cast<uint32_t>(kind), -1, 0);
            if (pk == 0) break;
            const uint32_t q = lwu(pk + 0x78);
            sh(q + 4, lhu(pv + 0xc4));
            const float a = rand_angle();
            const float speed = rand_range_f(0.0f, lwc1(OV(0x001618c8))) * lwc1(0x0015ed6c);
            swc1(q + 0x10, cos_vu(a) * speed);
            swc1(q + 0x18, 0.0f);
            swc1(q + 0x14, sin_vu(a) * speed);
            const float vz = rand_range_f(lwc1(OV(0x001618cc)), lwc1(OV(0x001618d0))) * lwc1(0x0015ed6c);
            swc1(q + 0x18, vz);
            if (lw(pv + 0xc0) < 0) continue;
            const float g = lwc1(0x0015ed70) * u2f(0x412ccccdu) * -0.5f;
            const uint32_t pts = lwu(OV(0x001b04b0) + lwu(pv + 0xc0) * 4);
            float r_hi, r_lo;
            const int32_t roots = solve_quadratic(&r_hi, &r_lo, g, vz - g, lwc1(pk + 0x18) - lwc1(pts + 0x18));
            float t = 120.0f;
            if (1.0f <= static_cast<float>(roots) && 0.0f < r_hi && r_hi < 120.0f) t = r_hi;
            Vec4 to, hit, back;
            vec3_scale(to, V(q + 0x10), t);
            vec3_add(to, to, V(m + 0x10));
            path_crossing_v(lw(pv + 0xc0), V(m + 0x10), to, hit);
            vec3_sub(to, hit, V(pk + 0x10));
            to.z = 0.0f;
            vec3_set_length(back, to, 0.25f);
            vec3_sub(to, to, back);
            vec3_scale(to, to, fpu_div(1.0f, t));
            swc1(q + 0x10, to.x);
            swc1(q + 0x14, to.y);
        }
    }
    if (lh(m + 0xb4) != 0) sh(m + 0xb4, 0);
    moby_drop_bolts(m, 0x100, -1);
}

// A crate shatters. The break sound (at most every other frame 0x1618d4; the 0x1fa crate its
// own), its link +0xa0/+0xa4 in the stacked-crate chain bridged, a puff of 6 kind 0xd particles,
// the class's chunks (0x95 or by class 0x1f4..0x1ff) laid out at the 0x1de6f0 offsets turned to
// the camera side (snapped to 1.57 rad) and thrown out 1..5 / up 4..9, more small ones for
// the moby budget 0x15ffbc; the crate goes (state 3, hidden, +0xbc 15 frames). With `fx`: the
// pickup delay +0xca, sparks (10, fewer far away or on a busy frame 0x1618d8) and shards.
// orig: 0x002d09b8
void crate_shatter(uint32_t m, uint32_t fx, uint32_t src) {
    const uint32_t pv = lwu(m + 0x78);
    bool busy = false;
    int32_t now = lw(OV(0x0015f5cc));
    if (!(abs_i(now - lw(OV(0x001618d4))) < 2)) {
        if (lh(m + 0xa6) == 0x1fa) {
            moby_sound_play(0, 0, m, 0x1f4);
        } else {
            moby_sound_start(0, 0, m);
        }
        sw(OV(0x001618d4), lwu(OV(0x0015f5cc)));
        now = lw(OV(0x0015f5cc));
    }
    if (!(abs_i(now - lw(OV(0x001618d8))) < 2)) {
        sw(OV(0x001618d8), lwu(OV(0x0015f5cc)));
    } else {
        busy = true;
    }
    if (lw(pv + 0xa0) != 0) {
        sw(lwu(lwu(pv + 0xa0) + 0x78) + 0xa4, lwu(pv + 0xa4));
        const uint32_t q = lwu(lwu(pv + 0xa0) + 0x78);
        sw(q + 0xac, lwu(q + 0xac) | 4u);
    }
    if (lw(pv + 0xa4) != 0) {
        if (moby_in_range_class_500(lwu(lwu(lwu(pv + 0xa4) + 0x78) + 0xa0)) == 0) debug_trap();
        sw(lwu(lwu(pv + 0xa4) + 0x78) + 0xa0, lwu(pv + 0xa0));
        const uint32_t q = lwu(lwu(pv + 0xa4) + 0x78);
        sw(q + 0xac, lwu(q + 0xac) | 4u);
    }
    {
        Vec4 puff = V(m + 0x10);
        puff.z = puff.z + u2f(0x3e99999au);
        spawn_particle_0d_v(puff, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f89999au), u2f(0x3d4ccccdu), 50000.0f);
        for (int32_t i = 4; i >= 0; --i) {
            spawn_particle_0d_v(puff, 1, 0x40808080u, u2f(0x3e99999au), u2f(0x3f8147aeu), u2f(0x3f88f5c3u), u2f(0x3d4ccccdu), 50000.0f);
        }
    }
    (void)src;  // (the original also takes src's position, or the hero-to-crate vector, unused)
    static constexpr uint32_t chunk_cls[12] = {0x165, 0x15c, 0x162, 0x15f, 0x168, 0x16b, 0x16e, 0x171, 0x171, 0x171, 0x171, 0x165};
    const uint32_t k = static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(m + 0xa6) - 0x1f4)));
    const uint32_t cls = k < 0xc ? chunk_cls[k] : 0x95u;
    const int32_t budget = lw(OV(0x0015ffbc));
    int32_t big, small;
    if (!(budget < 0xc9)) {
        big = 4; small = 4;
    } else if (!(budget < 0x97)) {
        big = 2; small = 2;
    } else if (!(budget < 0x65)) {
        big = 2; small = 1;
    } else {
        big = 1; small = 0;
    }
    const float step = u2f(0x3fc8f5c3u);
    float yaw = atan2_f(lwc1(OV(0x00166dc0)) - lwc1(m + 0x10), lwc1(OV(0x00166dc4)) - lwc1(m + 0x14));
    yaw = angle_sub(yaw, lwc1(m + 0x48)) + u2f(0x40490fdau);
    yaw = angle_wrap(static_cast<float>(float_to_int(fpu_div(yaw, step))) * step);
    Vec4 ang;
    vec_clear(ang);
    ang.z = yaw;
    Mat4 rot;
    euler_to_mat3_vu(rot, ang);
    Vec4 pos, vel, r;
    for (int32_t i = 0; i < big; ++i) {
        mat3_transform(pos, V(OV(0x001de6f0) + static_cast<uint32_t>(i) * 0x10), rot);
        vec3_add(pos, pos, V(m + 0x10));
        r = V(OV(0x001de750) + static_cast<uint32_t>(i) * 0x10);
        r.z = angle_add(r.z, yaw);
        vec3_sub(vel, pos, V(m + 0x10));
        vel.z = 0.0f;
        vec3_set_length(vel, vel, rand_range_f(1.0f, 5.0f) * lwc1(0x0015ed6c));
        vel.z = rand_range_f(4.0f, 9.0f) * lwc1(0x0015ed6c);
        const uint32_t c = chunk_spawn_v(m, vel, pos, r, cls, static_cast<uint32_t>(rand_mod(2)), 1.0f, 1.0f);
        if (c != 0) {
            lq_sq(c + 0x40, OV(0x001de750) + static_cast<uint32_t>(i) * 0x10);
            swc1(c + 0x48, angle_add(lwc1(c + 0x48), yaw));
            moby_update_transform(c);
        }
    }
    for (int32_t i = 0; i < small; ++i) {
        pos.x = rand_range_f(-0.5f, 0.5f);
        pos.y = rand_range_f(-0.5f, 0.5f);
        pos.z = rand_range_f(0.0f, 1.0f);
        vec3_add(pos, pos, V(m + 0x10));
        const float sp = rand_range_f(6.0f, 12.0f) * lwc1(0x0015ed6c);
        rand_sphere_point_v(vel, sp, sp);
        if (vel.z < 0.0f) vel.z = -vel.z;
        int32_t tag = 1;
        r.x = rand_angle();
        r.z = rand_angle();
        if (rand_mod(6) == 0) tag = float_to_int(rand_range_f(3.0f, 6.0f));
        chunk_spawn_v(m, vel, pos, r, cls + 1, static_cast<uint32_t>(tag), 0.25f, 0.5f);
    }
    sw(m + 0x94, 0);
    sb(m + 0x20, 3);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0xf)));
    if (fx == 0) return;
    if (lw(0x001413dc) == 0x16) {
        sb(pv + 0xca, static_cast<uint32_t>(frame_scale_i(5)));
    } else {
        sb(pv + 0xca, 1);
    }
    Vec4 at;
    vec3_set_length(at, V(m + 0xe0), 0.5f);
    vec3_add(at, at, V(m + 0x10));
    Vec4 d;
    vec3_sub(d, V(m + 0x10), V(OV(0x00166dc0)));
    Vec4 still;
    vec_clear(still);
    const float dist = vec3_length(d);
    int32_t n = 10;
    if (dist < 8.0f) n = float_to_int(dist) + 2;
    float near = 0.0f;
    if (dist < 7.0f) near = 7.0f - dist;
    if (busy && !(n < 7)) n = 6;
    for (int32_t i = n; 0 < i; --i) {
        float speed = rand_range_f(8.0f, 10.0f) * lwc1(0x0015ed6c);
        speed = speed - near * lwc1(0x0015ed6c);
        const uint32_t color = lwu(OV(0x001e9e80) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const uint32_t c2 = lwu(OV(0x001e9e98) + static_cast<uint32_t>(rand_mod(6)) * 4);
        const int32_t f15 = frame_scale_i(0xf);
        const int32_t life = rand_range(f15, frame_scale_i(0x14));
        const int32_t f19 = frame_scale_i(0x19);
        const int32_t fade = rand_range(f19, frame_scale_i(0x1e));
        spawn_particle_0b_v(at, still, color, c2, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), 0, 0, 400000.0f, speed);
    }
    if (lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && 9.0f < dist && !busy) {
        shard_spawn_v(m, at, still, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, 4.0f);
        shard_spawn_v(m, at, still, static_cast<uint32_t>(frame_scale_i(0x18)), 0x7f, 0x20, 0, 0x20, 4.0f);
        shard_spawn_v(m, at, still, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7f, 0x40, 0, 0x30, 4.0f);
    }
    shard_spawn_v(m, at, still, static_cast<uint32_t>(frame_scale_i(0x1b)), 0x60, 0x10, 0, 0x40, 3.5f);
    shard_spawn_v(m, at, still, static_cast<uint32_t>(frame_scale_i(0x1d)), 0x20, 0, 0, 0x20, 3.0f);
}

// A stack of crates (linked down by pvars +0xa4, up by +0xa0) falls, once a frame (+0xb4),
// from the bottom crate up: a crate the hero lands on from above (within 0.55 in its frame,
// state 1 or 5) shatters and drops its contents; the fall speed +0x48 grows by 9.8 * the
// gravity step 0x15ed70, within +-0.3. Riding a moby +0xf0: the height +0xf4 eases to the crate
// below's (+1) or to 0 and the crate follows the moby. Otherwise it falls until the ground
// (a collision probe down) or the crate below stops it; a hard landing of a 0x1f9 crate in
// state 1 sets it (or the one below) to state 5.
// orig: 0x002d1428
void crate_stack_update(uint32_t m) {
    if (lw(lwu(m + 0x78) + 0xb4) == lw(OV(0x0015f5cc))) return;
    uint32_t c = m;
    while (lwu(lwu(c + 0x78) + 0xa4) != 0) c = lwu(lwu(c + 0x78) + 0xa4);
    Vec4 rel, start, end, ground, pos, ang, d;
    while (c != 0) {
        const uint32_t s = c;
        const uint32_t p = lwu(s + 0x78);
        const uint32_t f = lwu(p + 0xac);
        c = lwu(p + 0xa0);
        if ((f & 4u) == 0) sw(p + 0xac, f & ~0x10u);
        sw(p + 0xac, lwu(p + 0xac) & ~4u);
        if (moby_in_range_class_500(s) == 0) return;
        if ((lwu(p + 0xac) & 2u) != 0) return;
        sw(p + 0xb4, lwu(OV(0x0015f5cc)));
        if (lbu(s + 0x20) == 0) continue;
        vec3_sub(rel, V(0x0013f3d0), V(s + 0x10));
        mat3_transform(rel, rel, M(s + 0xc0));
        if (abs_f(rel.x) < u2f(0x3f0ccccdu) && abs_f(rel.y) < u2f(0x3f0ccccdu) && rel.z < 0.0f &&
            -(lwc1(0x0013f570) + lwc1(0x0013f584)) < rel.z && (lbu(s + 0x20) == 1 || lbu(s + 0x20) == 5)) {
            crate_shatter(s, lh(s + 0xa6) == 0x1f9 ? 1u : 0u, 0);
            crate_break_drops(s);
        }
        const float g = lwc1(0x0015ed70);
        float vz = lwc1(p + 0x48) - g * u2f(0x411ccccdu);
        swc1(p + 0x48, vz);
        vz = vz < u2f(0xbe99999au) ? u2f(0xbe99999au) : vz;
        swc1(p + 0x48, vz);
        vz = u2f(0x3e99999au) < vz ? u2f(0x3e99999au) : vz;
        swc1(p + 0x48, vz);
        if (lw(p + 0xf0) != 0) {
            const uint32_t below = lwu(p + 0xa4);
            if (below == 0) {
                const float v = lwc1(p + 0x4c) - g * 10.0f;
                swc1(p + 0x4c, v);
                if (approach_f(p + 0xf4, 0.0f, abs_f(v)) == 0.0f) swc1(p + 0x4c, 0.0f);
            } else {
                const float v = lwc1(p + 0x4c) - g * 10.0f;
                swc1(p + 0x4c, v);
                const float target = lwc1(lwu(below + 0x78) + 0xf4) + 1.0f;
                if (approach_f(p + 0xf4, target, abs_f(v)) == 0.0f) sw(p + 0x4c, 0);
            }
            Vec4 local = V(p + 0xd0);
            local.z = local.z + lwc1(p + 0xf4);
            if (s != 0) sh(s + 0x36, 0x7f80);
            pos = V(s + 0x10);
            ang = V(s + 0x40);
            sub_002604f0_v(s, lwu(p + 0xf0), local, V(p + 0xe0), V(s + 0x10), V(s + 0x40));
            if ((lwu(moby_pvar_word2(lwu(p + 0xf0)) + 0x3c) & 4u) != 0) {
                V(p + 0xd0) = local;
                swc1(p + 0xd8, lwc1(p + 0xd8) - lwc1(p + 0xf4));
            }
            vec3_sub(d, V(s + 0x10), pos);
            d.w = lwc1(p + 0x4c);
            V(p + 0x40) = d;
            attach_frame_set_v(p + 0x60, V(p + 0x40), ang, V(s + 0x40));
            continue;
        }
        bool hit;
        const uint32_t below = lwu(p + 0xa4);
        if (below == 0) {
            start = V(s + 0x10);
            end = V(s + 0x10);
            start.z = start.z + lwc1(p + 0xb0) * 0.75f;
            end.z = end.z + lwc1(p + 0x48);
            hit = segment_collide_v(start, end, 2, 0, nullptr) != 0;
            ground = V(OV(0x00173e60));
        } else {
            ground = V(below + 0x10);
            ground.z = ground.z + 1.0f;
            hit = lwc1(s + 0x18) + lwc1(p + 0x48) < ground.z;
        }
        if (!hit) {
            sw(p + 0xac, lwu(p + 0xac) | 4u);
            vec3_add(V(s + 0x10), V(s + 0x10), V(p + 0x40));
            continue;
        }
        if (lwc1(0x0015ed6c) * 0.5f < vec3_length(V(p + 0x40))) {
            if (lh(s + 0xa6) == 0x1f9 && lbu(s + 0x20) == 1) {
                sb(s + 0xbc, 0);
                sb(s + 0x20, 5);
            } else {
                const uint32_t b = lwu(p + 0xa4);
                if (b != 0 && lh(b + 0xa6) == 0x1f9 && lbu(b + 0x20) == 1) {
                    sb(b + 0x20, 5);
                    sb(lwu(p + 0xa4) + 0xbc, 0);
                }
            }
        }
        swc1(s + 0x18, ground.z);
        sw(p + 0x48, 0);
    }
}

// The crate classes' update (0x1f4..0x1ff). Each frame: drop dead stack links, the "nothing on
// top" flag, the stack fall, the hit entry (a hit of type 0x1000000 only "kills" it) and, for
// the 0x1fb..0x1fd crates, the hit reaction. Then by state:
//  0 placed: settled, flags 0x14; 0x1ff crates get +0xc6 100 (on level 0x12 with +0xc8 they
//    wait hidden, state 6), 0x1f5 crates their companion;
//  1 idle: the 0x1f9 (nitro) crate is armed (state 5) when the hero stands on / holds it or it
//    is killed; a hit breaks it (nitro: armed; 0x1f6 only by a 0x20000 hit; the 0x1fb..0x1fd
//    ones take steps 0x1fc, 0x1fd, 0x1fe through state 2) -> shatter and contents;
//  2 hit flash: back to 1 when +0xbc runs out;
//  3 broken: a nitro crate's blast grows for 20 frames (hits at 2x / 3x its radius), others
//    are retired or (+0xc8) wait hidden to respawn (state 6);
//  4 delay +0xbc then idle; 5 armed: counts down from 3 s (beeps, flashes red), then blows;
//  6 waiting: once off screen and +0xc9 runs out it respawns, on the group's crate at its spot
//    (stacked) or on the floor; on level 0x12 only when few weapons are short of ammo.
// orig: 0x002cf218
void crate_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0xa0) != 0 && moby_in_range_class_500(lwu(pv + 0xa0)) == 0) sw(pv + 0xa0, 0);
    if (lw(pv + 0xa4) != 0 && moby_in_range_class_500(lwu(pv + 0xa4)) == 0) sw(pv + 0xa4, 0);
    const uint32_t hs = pv + 0x20;
    sw(pv + 0xfc, lwu(OV(0x0015f5cc)));
    if (lw(pv + 0xa0) == 0) {
        sh(hs + 0x1e, lhu(hs + 0x1e) | 1u);
    } else {
        sh(hs + 0x1e, lhu(hs + 0x1e) & 0xfffeu);
    }
    if (lw(pv + 0xb4) != lw(OV(0x0015f5cc)) && lbu(m + 0x20) != 0 && (lwu(pv + 0xac) & 0xcu) != 0) crate_stack_update(m);
    uint32_t hit = sub_0025a420(m, 0x01830000u, 0);
    bool killed = false;
    if (hit != 0 && lwu(hit + 0x24) == 0x01000000u) {
        hit = 0;
        killed = true;
    }
    int32_t hstate = 0, weapon = 0;
    float dmg = 0.0f;
    if (static_cast<uint32_t>(lhu(m + 0xa6)) - 0x1fbu < 3u) moby_hit_react_r(m, hit, hs, 0, hstate, &dmg, &weapon, 4);
    const uint32_t level = lwu(0x0015ed84);
    switch (lbu(m + 0x20)) {
    case 0: {
        vec_clear(V(pv + 0x40));
        if (level == 0x12 && lh(m + 0xa6) == 0x1ff && lbu(pv + 0xc8) != 0) {
            sb(m + 0x20, 6);
            sw(m + 0x94, 0);
            sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        } else {
            sb(m + 0x20, 1);
        }
        sw(pv + 0xa8, 0);
        if ((lwu(pv + 0xac) & 1u) == 0) crate_settle(m);
        sw(pv + 0xac, lwu(pv + 0xac) | 0x14u);
        if (lh(m + 0xa6) == 0x1ff) {
            sh(pv + 0xc6, 100);
        } else if (lh(m + 0xa6) == 0x1f5) {
            crate_spawn_companion(m);
        }
        break;
    }
    case 1: {
        if (lh(m + 0xa6) == 0x1f9) {
            constexpr uint32_t P = 0x0013f350;
            if (lwu(P + 0x240) == m || lwu(P + 0x23c) == m || (lwu(P + 0x2fc) == m && lh(P + 0x30e) == 0) || lwu(P + 0x4f8) == m || killed) {
                sb(m + 0xbc, 0);
                sb(m + 0x20, 5);
                break;
            }
        }
        if (hit == 0) break;
        if (!(0.0f < lwc1(hit + 0x2c))) break;
        bool breaks = true;
        if (lh(m + 0xa6) == 0x1f6 && (lwu(hit + 0x24) & 0x20000u) == 0) breaks = false;
        if (lh(m + 0xa6) == 0x1f9) {
            sb(m + 0x20, 5);
            uint32_t t = 1;
            if ((lwu(hit + 0x24) & 0x20000u) == 0) t = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(5.0f, 15.0f))));
            sb(m + 0xbc, t);
            sd(pv + 0xb8, ld(m + 0x38));
            breaks = false;
        }
        if (static_cast<uint32_t>(lhu(m + 0xa6)) - 0x1fbu < 3u) {
            if ((lwu(hit + 0x24) & 0x20000u) != 0) sh(m + 0xa6, 0x1fe);
            if (hstate == 3) sh(m + 0xa6, 0x1fe);
            if ((lwu(hit + 0x24) & 0x500000u) != 0 || weapon == 0xb4) {
                int32_t c = lh(m + 0xa6) + 1;
                if (!(c < 0x1ff)) c = 0x1fe;
                sh(m + 0xa6, static_cast<uint32_t>(c));
            }
            if (lh(m + 0xa6) != 0x1fe) {
                const uint32_t c = lhu(m + 0xa6) + 1u;
                sh(m + 0xa6, c);
                const uint32_t idx = lbu(OV(0x00197bc0) + static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(c))));
                sb(m + 0x22, idx);
                sw(m + 0x24, lwu(OV(0x00197300) + idx * 4));
                sub_0024ebc0(m);
                sb(m + 0x72, lbu(lwu(m + 0x24) + 0xe));
                sb(m + 0x20, 2);
                sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0xf)));
                breaks = false;
            }
        }
        if (!breaks) break;
        crate_shatter(m, lh(m + 0xa6) == 0x1f9 ? 1u : 0u, hit);
        if (lbu(pv + 0xc8) != 0 && hit != 0 && lw(hit + 0x20) != 0 && (lh(lwu(hit + 0x20) + 0xa6) == 0x2cd || lh(lwu(hit + 0x20) + 0xa6) == 0x22c)) {
            const uint32_t keep = lhu(m + 0xb4);
            sh(m + 0xb4, 0);
            crate_break_drops(m);
            sh(m + 0xb4, keep);
            break;
        }
        crate_break_drops(m);
        break;
    }
    case 2: {
        const uint32_t t = (lbu(m + 0xbc) - 1u) & 0xffu;
        sb(m + 0xbc, t);
        if (t == 0xff) {
            sb(m + 0xa4, 0xff);
            sb(m + 0x20, 1);
        }
        break;
    }
    case 3: {
        if (lh(m + 0xa6) == 0x1f9 && !(frame_scale_i(0x14) < static_cast<int32_t>(lbu(pv + 0xca)))) {
            // the hit record: zero vector, the crate, the kind, (+0x18: an unset stack word in
            // the original), 2.0, 0
            uint32_t rec[9] = {0, 0, 0, 0, m, 1, 0, f2u(2.0f), 0};
            Vec4 at;
            vec3_set_length(at, V(m + 0xe0), 0.5f);
            vec3_add(at, at, V(m + 0x10));
            const float r = fpu_div(int_to_float(lbu(pv + 0xca)), frame_scale_f(20.0f));
            mobys_in_sphere_v(at, r + r, 0x10, m, rec);
            rec[5] = 0x30000;
            mobys_in_sphere_v(at, r * 3.0f, 0x10, lwu(0x001413d0), rec);
            sb(pv + 0xca, lbu(pv + 0xca) + 1u);
            break;
        }
        if (lbu(pv + 0xc8) == 0) {
            moby_retire(m);
            return;
        }
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 1u) & 0xefffu);
        sb(m + 0x31, 0);
        if (!(lh(m + 0xb4) < 2)) sh(m + 0xb4, 1);
        sb(m + 0x20, 6);
        sb(pv + 0xc9, static_cast<uint32_t>(level == 0x12 ? rand_range(300, 0x2d0) : rand_range(0x1e, 0x3c)));
        break;
    }
    case 4:
        if (countdown_u8(m + 0xbc) != 0) {
            sb(m + 0x20, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        }
        break;
    case 5: {
        if (lbu(m + 0xbc) == 0) {
            const int32_t s = frame_scale_i(0x3c);
            sb(m + 0xbc, static_cast<uint32_t>(s * 3 - 1));
            sd(pv + 0xb8, ld(m + 0x38));
        } else {
            const int32_t t = lbu(m + 0xbc);
            if (frame_scale_i(0xf) < t && hit != 0) sb(m + 0xbc, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(5.0f, 15.0f)))));
        }
        int32_t ph = static_cast<int32_t>(lbu(m + 0xbc)) % frame_scale_i(0x3c);
        if (ph == frame_scale_i(0x3c) - 3 || static_cast<int32_t>(lbu(m + 0xbc)) == frame_scale_i(0xd)) moby_sound_start(1, 0x20, m);
        ph = static_cast<int32_t>(lbu(m + 0xbc)) % frame_scale_i(0x3c);
        if (frame_scale_i(0x28) < ph || static_cast<int32_t>(lbu(m + 0xbc)) < frame_scale_i(0xf)) {
            moby_set_color_hi(m, 0xc8, 0x40, 0x40);
        } else {
            sd(m + 0x38, ld(pv + 0xb8));
        }
        if (lbu(m + 0xbc) == 1 || lw(0x001413dc) == 0x16) {
            sd(m + 0x38, ld(pv + 0xb8));
            crate_shatter(m, 1, 0);
            crate_break_drops(m);
        }
        sb(m + 0xbc, lbu(m + 0xbc) - 1u);
        break;
    }
    case 6: {
        Vec4 top = V(m + 0x10);
        top.z = top.z + 0.5f;
        top.w = 1.0f;
        if (point_on_screen_v(top, static_cast<float>(lh(m + 0x32))) != -1) break;
        if (countdown_u8(pv + 0xc9) == 0) break;
        auto show = [m] {
            sb(m + 0x31, 1);
            sb(m + 0x20, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        };
        if (lbu(m + 0x21) == 0xff) {
            show();
            break;
        }
        bool go = true;
        if (level == 0x12) {
            int32_t own = 0, low = 0;
            for (uint32_t i = 0; i < 0x25; ++i) {
                if (lbu(0x0013d4c0 + i) == 0) continue;
                ++own;
                const uint32_t mx = lhu(OV(0x001c40b0) + i * 0x18 + 0xe);
                if (mx != 0 && static_cast<float>(lw(0x0013d428 + i * 4)) < static_cast<float>(static_cast<int32_t>(mx)) * u2f(0x3f4ccccdu)) ++low;
            }
            if (low < 3 && !(own < 3)) go = false;
            if (own < 3 && low != own) go = false;
        }
        if (!go) break;
        uint32_t c = 0;
        group_first_r(c, lbu(m + 0x21), 0, 0);
        while (c != 0) {
            if (m != c && !(0.1f < vec2_distance(V(m + 0x10), V(c + 0x10))) && lbu(c + 0x20) == 1 && lw(lwu(c + 0x78) + 0xa0) == 0) {
                V(m + 0x10) = V(c + 0x10);
                swc1(m + 0x18, lwc1(m + 0x18) + 1.0f);
                sw(lwu(c + 0x78) + 0xa0, m);
                sw(pv + 0xa0, 0);
                sw(pv + 0xa4, c);
                show();
                sw(pv + 0xac, lwu(pv + 0xac) | 4u);
                break;
            }
            group_next_r(c, c, 0, 0);
        }
        if (lbu(m + 0x20) == 1) break;
        swc1(m + 0x18, floor_probe(0.5f, m + 0x10, 0));
        sb(m + 0x20, 1);
        sw(pv + 0xa4, 0);
        sw(pv + 0xa0, 0);
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    }
    default:
        break;
    }
    sb(m + 0xa4, 0xff);
}

// A bolt is picked up: its value (class 0xe 5, 0xf 20, 0x10 50, else 1) added to the source's
// group count (0x14d592, unless +0xb1 has bit 7), the bolts 0x15ed98, the drop rate 0x15ee2c
// (only for bolts of a breakable +0x40, or with 0x15f598 those from the level's own mobys) and
// the level's total 0x13df38; the counter shown (0x235700); two sparkle particles.
// orig: 0x002a6b70
void bolt_collect(uint32_t m) {
    int32_t v = 1;
    const int32_t c = lh(m + 0xa6);
    if (c == 0xe) {
        v = 5;
    } else if (c == 0xf) {
        v = 0x14;
    } else if (c == 0x10) {
        v = 0x32;
    }
    const uint32_t src = lwu(m + 0xb8);
    if (src != 0 && (lbu(src + 0xb1) & 0x80u) == 0) {
        const uint32_t h = 0x0014d590 + 2 + lbu(src + 0xb1) * 4 + lwu(0x0015ed84) * 0x100;
        sh(h, lhu(h) + static_cast<uint32_t>(v));
    }
    sw(0x0015ed98, static_cast<uint32_t>(lw(0x0015ed98) + v));
    const uint32_t p = lwu(m + 0x78);
    if (lbu(p + 0x40) != 0 || (lw(OV(0x0015f598)) != 0 && m < lwu(OV(0x0015ffdc)))) sw(0x0015ee2c, static_cast<uint32_t>(lw(0x0015ee2c) + v));
    const uint32_t t = 0x0013df38 + lwu(0x0015ed84) * 4;
    sw(t, static_cast<uint32_t>(lw(t) + v));
    camera_request(2, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), 0x0015ed98, 0x0098967f);
    // (dir.w: an unset stack word in the original)
    Vec4 dir = Vec4{};
    Vec4 at;
    rand_sphere_point_v(dir, lwc1(0x0015ed6c) * u2f(0x3f333333u), lwc1(0x0015ed6c));
    vec3_scale(at, dir, 10.0f);
    vec3_add(at, at, V(m + 0x10));
    const int32_t r = rand_mod(2);
    int32_t spin = r != 0 ? r : r - 1;
    const float k = rand_range_f(u2f(0x3ecccccdu), 0.5f);
    spawn_particle_35_v(at, static_cast<uint32_t>(frame_scale_i(0x19)), 0x7f207f7fu, 0, static_cast<uint32_t>(spin), dir, k * u2f(0x3e4ccccdu), k, 0.0f);
    spin = -spin;
    spawn_particle_35_v(at, static_cast<uint32_t>(frame_scale_i(0x19)), 0x7f7f7f7fu, 1, static_cast<uint32_t>(spin), dir, k * u2f(0x3e0f5c29u), k * u2f(0x3f333333u), 0.0f);
}

// A bolt's motion while attached / spinning: its frame (pvars +0 position, +0x10 angles, carried
// by the moby +0x5c it lies on), the spin +0x50 / +0x58 (direction +0x55 bit 0; per class 0xd
// .. 0x10 the spin rate, tilt, height and offsets) into its matrix and position; the frame
// returned in `out_pos` / `out_ang`. A bolt (state 3) lying on a moby that went away (state
// 0xfe / 0xfd) or no longer touches anything falls (state 1) with that moby's speed.
// orig: 0x002a6de8
void bolt_spin_place_v(uint32_t m, Vec4* out_pos, Vec4* out_ang) {
    const uint32_t p = lwu(m + 0x78);
    // (left unset by 0x2604f0 when the moby carries nothing: stack words in the original)
    Vec4 pos = Vec4{};
    Vec4 ang = Vec4{};
    if (lw(p + 0x5c) != 0) {
        sub_002604f0_v(m, lwu(p + 0x5c), V(p), V(p + 0x10), pos, ang);
    } else {
        pos = V(p);
        ang = V(p + 0x10);
    }
    float rate, tilt, up, fwd, r;
    switch (lh(m + 0xa6)) {
    case 0xd: rate = u2f(0x3d23d70au); tilt = -0.25f; up = u2f(0x3d47ae14u); fwd = u2f(0x3ea66666u); r = u2f(0x3e19999au); break;
    case 0xe: rate = u2f(0x3cf5c28fu); tilt = -0.5f; up = u2f(0x3cc8b439u); fwd = u2f(0x3eb33333u); r = u2f(0x3e6b851fu); break;
    case 0xf: rate = u2f(0x3ccccccdu); tilt = u2f(0xbecccccdu); up = u2f(0x3d666666u); fwd = 0.375f; r = u2f(0x3e6b851fu); break;
    default: rate = u2f(0x3c8f5c29u); tilt = u2f(0xbeb33333u); up = u2f(0x3e0a3d71u); fwd = u2f(0x3e99999au); r = 0.25f; break;
    }
    if ((lbu(p + 0x55) & 1u) != 0) {
        swc1(p + 0x50, angle_add(lwc1(p + 0x50), rate));
        swc1(p + 0x58, angle_add(lwc1(p + 0x58), u2f(0xbc4ccccdu)));
    } else {
        swc1(p + 0x50, angle_sub(lwc1(p + 0x50), rate));
        swc1(p + 0x58, angle_sub(lwc1(p + 0x58), u2f(0xbc4ccccdu)));
    }
    Vec4 spin = V(p + 0x50);
    spin.y = tilt;
    Mat4 a, b;
    euler_to_mat3_vu(a, spin);
    euler_to_mat3_vu(b, ang);
    mat3_mul(M(m + 0xc0), b, a);
    vec3_scale(V(m + 0x10), b.r[2], up);
    Vec4 off;
    vec3_scale(off, V(m + 0xc0), fwd);
    vec3_add(V(m + 0x10), V(m + 0x10), off);
    vec3_add(V(m + 0x10), V(m + 0x10), pos);
    if (out_pos != nullptr) *out_pos = pos;
    if (out_ang != nullptr) *out_ang = ang;
    if (lbu(m + 0x20) != 3) return;
    const uint32_t on = lwu(p + 0x5c);
    if (on == 0 || lbu(p + 0x57) != 0) return;
    const uint32_t st = lbu(on + 0x20);
    const bool falls = st == 0xfe || st == 0xfd || sphere_collide(m + 0x10, r, 0x22, m) == 0;
    if (!falls) return;
    sb(m + 0x20, 1);
    vec_clear(V(p + 0x30));
    swc1(p + 0x3c, 1.0f);
    const uint32_t w = moby_pvar_word2(lwu(p + 0x5c));
    if (w != 0) {
        V(p + 0x20) = V(w + 0x10);
    } else {
        vec_clear(V(p + 0x20));
    }
    Vec4 s, t;
    vec3_set_length(s, b.r[2], lwc1(0x0015ed6c));
    vec3_set_length(t, V(m + 0xc0), lwc1(0x0015ed6c));
    vec3_add(s, s, t);
    vec3_add(V(p + 0x20), V(p + 0x20), s);
    vec3_add(V(m + 0x10), V(m + 0x10), V(p + 0x20));
    sw(p + 0x5c, 0);
}
// orig: 0x002a6de8 (EE-address form)
void bolt_spin_place(uint32_t m, uint32_t out_pos, uint32_t out_ang) {
    bolt_spin_place_v(m, out_pos != 0 ? &V(out_pos) : nullptr, out_ang != 0 ? &V(out_ang) : nullptr);
}

// The bolt classes' update (0xd..0x10). Hidden in mode 2. States:
//  0 spawned: resting (3), spin direction, its frame from the floor under it (or where it is),
//    its rest height, a sparkle timer, scale by value;
//  1 flying: gravity, tumble (+0x30 quaternion); past the hero (below him) it flies to him; on
//    touching the ground it lies there (+0x5c on a moby), bounces (or is caught by the hero's
//    magnet range 0x1415d8 / 0x1415dc, flying to him) and, on flat ground, settles (2);
//  2 settling: eases into its rest frame and height over +0x56 frames, then rests (3);
//  3 resting: spinning, bobbing, now and then a sparkle (0x16ea40); taken by the hero in
//    range; a far one from a dropped moby (budget 0x15ffbc under 200) vanishes when hidden;
//  4 taken: flies into the hero (pulled and turned to his collection point), then counts
//    (0x2a6b70) and goes, its drop recorded.
// orig: 0x002a5dd8
void bolt_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    constexpr uint32_t P = 0x0013f350;
    if (lw(OV(0x0015f5c4)) == 2) {
        sb(m + 0x31, 0);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
    } else {
        sb(m + 0x31, 1);
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    }
    auto ground_frame = [m, p] {  // the frame of the ground hit 0x173e40 (normal +0x40)
        V(p) = V(OV(0x00173e60));
        vec_clear(V(p + 0x10));
        swc1(p + 0x18, atan2_f(lwc1(OV(0x00173e80)), lwc1(OV(0x00173e84))));
        const float l = vec2_length(V(OV(0x00173e80)));
        swc1(p + 0x14, atan2_f(lwc1(OV(0x00173e88)), l));
    };
    auto ride = [m, p] {  // lying on a moby that carries things
        const uint32_t on = lwu(OV(0x00173e58));
        if (on != 0 && moby_pvar_word2(on) != 0) {
            sw(p + 0x5c, lwu(OV(0x00173e58)));
            sub_00260668(m, lwu(OV(0x00173e58)), p, p + 0x10, p, p + 0x10);
        }
    };
    switch (lbu(m + 0x20)) {
    case 0: {
        sb(m + 0x20, 3);
        sb(m + 0x73, 0x14);
        sh(m + 0x34, lhu(m + 0x34) | 0x100u);
        sb(m + 0x30, 0x20);
        sh(m + 0x32, 0x20);
        if (rand_mod(2) == 0) sb(p + 0x55, lbu(p + 0x55) | 1u);
        Vec4 a, b;
        vec3_set_length(a, V(m + 0xe0), u2f(0x3e4ccccdu));
        vec3_add(a, a, V(m + 0x10));
        vec3_set_length(b, V(m + 0xe0), -3.0f);
        vec3_add(b, b, V(m + 0x10));
        const int32_t hit = segment_collide_v(a, b, 0x22, 0, nullptr);
        if (lbu(p + 0x54) == 0 && hit != 0) {
            ground_frame();
            ride();
        } else {
            V(p) = V(m + 0x10);
            vec_clear(V(p + 0x10));
        }
        swc1(p + 0x50, rand_angle());
        swc1(p + 0x58, rand_angle());
        swc1(p + 0x68, lwc1(m + 0x18));
        sh(p + 0x6e, static_cast<uint32_t>(rand_range(0, frame_scale_i(0x258))));
        sh(p + 0x6c, 0xffff);
        float k;
        switch (lh(m + 0xa6)) {
        case 0xd: k = u2f(0x3f266666u); break;
        case 0xe: k = u2f(0x3f333333u); break;
        case 0xf: k = 0.75f; break;
        case 0x10: k = u2f(0x3f19999au); break;
        default: return;
        }
        swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * k);
        return;
    }
    case 1: {
        swc1(p + 0x28, lwc1(p + 0x28) - lwc1(0x0015ed70) * u2f(0x412ccccdu));
        vec3_add(V(m + 0x10), V(m + 0x10), V(p + 0x20));
        Mat4 t;
        quat_to_mat3(V(p + 0x30), t);
        mat3_mul(M(m + 0xc0), t, M(m + 0xc0));
        float r;
        switch (lh(m + 0xa6)) {
        case 0xd: r = u2f(0x3e19999au); break;
        case 0xe: case 0xf: r = u2f(0x3e6b851fu); break;
        default: r = 0.25f; break;
        }
        if (lwc1(m + 0x18) < lwc1(p + 0x68) - 2.0f && lwc1(m + 0x18) < lwc1(P + 0x88) - 2.0f) {
            const float yaw = rand_range_f(-15.0f, 15.0f) * u2f(0x3c8efa35u);
            bolt_fly_to(m, lwu(P + 0x2080), yaw, 0.0f);
            return;
        }
        if (sphere_collide(m + 0x10, r, 0x22, m) == 0) return;
        ground_frame();
        Vec4 n;
        vec3_sub(n, V(m + 0x10), V(OV(0x00173e60)));
        vec3_reflect(V(p + 0x20), V(p + 0x20), n);
        if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < lwc1(P + 0x2288) && abs_f(lwc1(m + 0x18) - lwc1(P + 0x88)) < lwc1(P + 0x228c)) {
            if (lw(P + 0x22a8) == 0) return;
            const float sp = rand_range_f(3.0f, 7.0f);
            vec3_set_length(V(p + 0x20), V(p + 0x20), sp * lwc1(0x0015ed6c));
            bolt_fly_to(m, lwu(P + 0x2080), 0.0f, 0.0f);
            return;
        }
        const float flat = u2f(0x3f490fdbu);
        if (!(atan2_f(n.z, vec2_length(n)) < flat)) return;
        if (!(atan2_f(lwc1(OV(0x00173e88)), vec2_length(V(OV(0x00173e80)))) < flat)) return;
        sb(m + 0x20, 2);
        const uint32_t t2 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(30.0f, 40.0f))));
        sb(p + 0x56, t2);
        sb(p + 0x57, t2);
        ride();
        Vec4 e;
        mat_to_euler_r(M(m + 0xc0), e);
        quat_from_euler_v(V(p + 0x30), e);
        return;
    }
    case 2: {
        Vec4 rest;
        bolt_spin_place_v(m, &rest, nullptr);
        rest.z = rest.z + 0.25f;
        Vec4 e, q;
        mat_to_euler_r(M(m + 0xc0), e);
        quat_from_euler_v(q, e);
        const float step = fpu_div(1.0f, int_to_float(lbu(p + 0x57)));
        float f = fpu_div(int_to_float(lbu(p + 0x57)), int_to_float(lbu(p + 0x56)));
        quat_nlerp(V(p + 0x30), V(p + 0x30), q, step);
        vec3_lerp(V(m + 0x10), V(m + 0x10), rest, f);
        f = f - 0.5f;
        const float k = static_cast<float>(static_cast<int32_t>(lbu(p + 0x56))) * u2f(0x3ccccccdu);
        f = f * f;
        f = f - 0.25f;
        f = -f;
        f = f * k;
        swc1(m + 0x18, lwc1(m + 0x18) + f);
        quat_to_mat3(V(p + 0x30), M(m + 0xc0));
        const uint32_t left = (lbu(p + 0x57) - 1u) & 0xffu;
        sb(p + 0x57, left);
        if (left != 0) return;
        sb(m + 0x20, 3);
        const uint32_t t3 = static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(10.0f, 20.0f))));
        sb(p + 0x56, t3);
        sb(p + 0x57, t3);
        return;
    }
    case 3: {
        const float d = vec2_distance(V(m + 0x10), V(0x0013f3d0));
        if (d < lwc1(P + 0x2288) && abs_f(lwc1(m + 0x18) - lwc1(P + 0x88)) < lwc1(P + 0x228c)) {
            bolt_fly_to(m, lwu(P + 0x2080), 0.0f, 0.0f);
            return;
        }
        if (lwu(OV(0x0015ffdc)) < m && lw(OV(0x0015ffbc)) < 200 && 48.0f < d && lbu(m + 0x31) == 0) {
            moby_retire(m);
            return;
        }
        if (lbu(m + 0x31) == 0 && lw(p + 0x5c) == 0) {
            const uint32_t v = lbu(p + 0x57);
            if (v != 0) sb(p + 0x57, v - 1u);
            return;
        }
        bolt_spin_place_v(m, nullptr, nullptr);
        if (lbu(p + 0x57) != 0) {
            float f = fpu_div(int_to_float(lbu(p + 0x57)), int_to_float(lbu(p + 0x56)));
            const float k = static_cast<float>(static_cast<int32_t>(lbu(p + 0x56))) * u2f(0x3c4ccccdu);
            f = f - 0.5f;
            f = f * f;
            f = f - 0.25f;
            f = -f;
            f = f * k;
            swc1(m + 0x18, lwc1(m + 0x18) + f);
            sb(p + 0x57, lbu(p + 0x57) - 1u);
        }
        const uint32_t self = static_cast<uint32_t>(static_cast<int32_t>(m - lwu(OV(0x0015ffd8))) >> 8);
        Vec4 at;
        if (!(lh(p + 0x6c) < 0)) {
            mat3_transform(at, V(OV(0x001d73d0) + static_cast<uint32_t>(lh(m + 0xa6)) * 16), M(m + 0xc0));
            const uint32_t e = OV(0x0016ea40) + static_cast<uint32_t>(lh(p + 0x6c)) * 0x20;
            vec3_add(V(e), at, V(m + 0x10));
            if (lh(e + 0x14) != static_cast<int32_t>(self)) sh(p + 0x6c, 0xffff);
            return;
        }
        const uint32_t t = (lhu(p + 0x6e) - 1u) & 0xffffu;
        sh(p + 0x6e, t);
        if (0 < static_cast<int16_t>(t)) return;
        mat3_transform(at, V(OV(0x001d73d0) + static_cast<uint32_t>(lh(m + 0xa6)) * 16), M(m + 0xc0));
        vec3_add(at, at, V(m + 0x10));
        sh(p + 0x6c, static_cast<uint32_t>(sparkle_add(at, self, u2f(0x3fb33333u))));
        const int32_t lo = frame_scale_i(0x12c);
        sh(p + 0x6e, static_cast<uint32_t>(rand_range(lo, frame_scale_i(0x258))));
        return;
    }
    case 4: {
        bool in_front = false;
        Vec4 a, b;
        if (lh(lwu(p + 0x60) + 0xa6) == 0) {
            vec3_sub(a, V(m + 0x10), V(0x0013f5f0));
            mat4_transform(a, a, M(0x0013f390));
            if (a.z < 0.0f) in_front = true;
        }
        const float dt = lwc1(0x0015ed6c);
        if (in_front) {
            vec3_set_length(a, V(0x0013f370), dt * 7.0f);
            vec3_sub(b, a, V(p + 0x20));
            vec3_clamp_length_v(b, lwc1(0x0015ed70) * 50.0f);
            vec3_add(V(p + 0x20), V(p + 0x20), b);
        } else {
            const float cap = lwc1(0x0015ed70) * 10.0f;
            if (cap < vec3_length(V(p + 0x20))) {
                vec3_set_length(a, V(p + 0x20), cap);
                vec3_sub(V(p + 0x20), V(p + 0x20), a);
            }
            approach_f(p + 0x64, lwc1(0x0015ed6c) * 48.0f, lwc1(0x0015ed70) * 16.0f);
            const float lag = lwc1(0x0015ed6c) * 8.0f;
            const float hs = lwc1(P + 0x160);
            if (lag < hs) {
                vec3_set_length(b, V(P + 0x100), (hs - lag) * 0.5f);
                vec3_add(V(m + 0x10), V(m + 0x10), b);
            }
        }
        Mat4 t;
        quat_to_mat3(V(p + 0x30), t);
        mat3_mul(M(m + 0xc0), t, M(m + 0xc0));
        vec3_add(V(m + 0x10), V(m + 0x10), V(p + 0x20));
        Vec4 to;
        vec3_sub(to, V(0x0013f420), V(m + 0x10));
        if (lwc1(p + 0x64) < vec3_length(to)) {
            if (!(lwc1(p + 0x4c) == 0.0f && lwc1(p + 0x48) == 0.0f)) {
                mat3_transform(to, to, M(0x0013f390));
                Vec4 ang;
                vec_clear(ang);
                ang.y = lwc1(p + 0x48);
                ang.z = lwc1(p + 0x4c);
                Mat4 r;
                euler_to_mat3_vu(r, ang);
                mat3_transform(to, to, r);
                mat3_transform(to, to, M(0x0013f350));
            }
            vec3_set_length(to, to, lwc1(p + 0x64));
            vec3_add(V(m + 0x10), V(m + 0x10), to);
            return;
        }
        bolt_collect(m);
        if (m < lwu(OV(0x0015ffdc))) {
            const int32_t slot = lh(m + 0xb2);
            sb(OV(0x001baa24) + static_cast<uint32_t>(slot), lbu(m + 0xb0) + 2u);
            const uint32_t b0 = lbu(m + 0xb0);
            if (b0 == 0xff || (lbu(OV(0x0015fc88) + b0) != 0xff && lbu(0x0014c050 + lwu(0x0015ed84) * 0x10 + b0) == 0xff)) {
                sb(OV(0x001bb684) + static_cast<uint32_t>(static_cast<int32_t>(lh(m + 0xb2))), b0 + 2u);
            }
        }
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}

// A message on the HUD for `frames` (-1: 3 s): the text `id` (0x203ad8) formatted with `value`
// into 0x179118, its timer 0x15f640.
// orig: 0x00263db0
void hud_message_show(int32_t id, uint32_t value, int32_t frames) {
    if (frames == -1) frames = frame_scale_i(0xb4);
    libc_sprintf(OV(0x00179118), sub_00203ad8(id), {value});
    sw(OV(0x0015f640), static_cast<uint32_t>(frames));
}

// Adds `n` ammo to weapon `w` (0x13d428[w]), capped at its maximum 0x1c40b0[w].+0xe (none: no
// cap). Returns the excess (0 when it all fit).
// orig: 0x00233e40
int32_t ammo_add(uint32_t w, int32_t n) {
    const uint32_t a = 0x0013d428 + w * 4, rec = OV(0x001c40b0) + w * 0x18;
    const int32_t v = lw(a) + n;
    sw(a, static_cast<uint32_t>(v));
    const uint32_t mx = lhu(rec + 0xe);
    if (mx == 0) return 0;
    const int32_t over = v - static_cast<int32_t>(mx);
    if (!(0 < over)) return 0;
    sw(a, mx);
    return over;
}

// An ammo pickup is taken: the ammo +0 for weapon +8 added; nothing when it was full. The HUD
// says how much (the weapon's text +0x36 for one, +0x34 for more); the gain counted at 0x13de08.
// A pickup taken only in part stays; one taken whole shrinks away (state 2), with the pickup
// sound at most every 10 frames (0x161838). Returns 1 when any was taken.
// orig: 0x002c5be8
int32_t pickup_collect(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    if (lbu(m + 0x20) == 2) return 0;
    const int32_t before = lw(0x0013d428 + lwu(p + 8) * 4);
    const float excess = static_cast<float>(ammo_add(lwu(p + 8), lw(p)));
    const uint32_t w = lwu(p + 8);
    const int32_t now = lw(0x0013d428 + w * 4);
    if (!(before < now)) return 0;
    const int32_t got = now - before;
    const uint32_t rec = OV(0x00179ac0) + w * 0x4c;
    hud_message_show(got == 1 ? lh(rec + 0x36) : lh(rec + 0x34), static_cast<uint32_t>(got), -1);
    if (excess < static_cast<float>(lw(p))) {
        const uint32_t w2 = lwu(p + 8);
        const uint32_t t = 0x0013de08 + w2 * 4;
        sw(t, static_cast<uint32_t>(lw(t) + (lw(0x0013d428 + w2 * 4) - before)));
        sb(m + 0x20, 2);
        swc1(m + 0x2c, lwc1(m + 0x2c) * (lwc1(0x0015ed60) * u2f(0x3e4cccd0u) + 1.0f));
        vec_clear(V(p + 0x10));
        swc1(p + 0x24, lwc1(0x0015ed6c) * 8.0f);
        const int32_t since = abs_i(lw(OV(0x0015f5cc)) - lw(OV(0x00161838)));
        if (frame_scale_i(10) < since) {
            moby_sound_play(0, 0, m, 0xd5);
            sw(OV(0x00161838), lwu(OV(0x0015f5cc)));
        }
    }
    return 1;
}

// Whether the moby is one of the ammo pickup classes (0xcc, 0xd5, 0xd6, 0xde, 0xdf, 0xe1, 0xe2,
// 0x3ee).
// orig: 0x0025e3f8
int32_t moby_is_pickup(uint32_t m) {
    switch (lh(m + 0xa6)) {
    case 0xe2: case 0xcc: case 0xde: case 0x3ee: case 0xd6: case 0xe1: case 0xd5: case 0xdf: return 1;
    default: return 0;
    }
}

// The wrench pulls in what lies within 2.7 of it: bolts (class kind +0x46 0x13) fly to the
// hero, ammo pickups are taken; walking the update list 0x15ffe4 (+0x28) until a moby that is
// gone (state 0xfe / 0xfd) or one is taken.
// orig: 0x002a82c0
void wrench_pull_pickups(uint32_t m) {
    for (uint32_t c = lwu(OV(0x0015ffe4)); c != 0; c = lwu(c + 0x28)) {
        const uint32_t st = lbu(c + 0x20);
        if (st == 0xfe || st == 0xfd) return;
        if (lw(c + 0x24) != 0 && lh(lwu(c + 0x24) + 0x46) == 0x13) {
            if (!(vec3_distance(V(c + 0x10), V(m + 0x10)) < u2f(0x402ccccdu))) continue;
            const float yaw = rand_range_f(-30.0f, 30.0f) * u2f(0x3c8efa35u);
            if (bolt_fly_to(c, lwu(0x0013f350 + 0x2080), yaw, 0.0f) != 0) return;
            continue;
        }
        if (moby_is_pickup(c) == 0) continue;
        if (!(vec3_distance(V(c + 0x10), V(m + 0x10)) < u2f(0x402ccccdu))) continue;
        if (pickup_collect(c) != 0) return;
    }
}

// The sparks where the wrench hit (0x173e60): 20 streaks and 10 flashes thrown back toward
// the camera's side (within pi of the view's pitch), and, for a weapon kind +0x78 of 0x2b, the
// spark pairs.
// orig: 0x002a8028
void wrench_hit_burst(uint32_t m) {
    Vec4 dir;  // the view vector, then (x, y, z only) each spark's direction
    vec3_sub(dir, V(OV(0x00166dc0)), V(OV(0x00173e60)));
    const float pi = u2f(0x40490fdbu);
    const float pitch = -atan2_f(vec2_length(dir), dir.z);
    // (the original also takes atan2_f(dir.x, dir.y) here and drops it)
    Vec4 at;
    for (int32_t i = 0x12; i >= -1; --i) {
        const float b = angle_add(rand_range_signed_f(0.0f, pi), pitch);
        const float a = angle_add(rand_range_signed_f(0.0f, pi), pitch);
        vec_from_spherical_v(dir, u2f(0x3e4ccccdu), a, b);
        vec3_scale(at, dir, rand_range_f(1.0f, 3.0f));
        vec3_add(at, at, V(OV(0x00173e60)));
        spawn_particle_13_v(at, dir);
    }
    for (int32_t i = 8; i >= -1; --i) {
        at.x = lwc1(OV(0x00173e60)) + rand_range_signed_f(0.0f, 0.1f);
        at.y = lwc1(OV(0x00173e64)) + rand_range_signed_f(0.0f, 0.1f);
        at.z = lwc1(OV(0x00173e68)) + rand_range_signed_f(0.0f, 0.1f);
        const float b = angle_add(rand_range_signed_f(0.0f, pi), pitch);
        const float a = angle_add(rand_range_signed_f(0.0f, pi), pitch);
        vec_from_spherical_v(dir, u2f(0x3d4ccccdu), a, b);
        const int32_t lo = frame_scale_i(10);
        const int32_t n = rand_range(lo, frame_scale_i(0x14));
        spawn_particle_15_v(at, dir, 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(n), 1, 20000.0f);
    }
    if (lh(lwu(m + 0x78) + 0x78) == 0x2b) spark_pairs(OV(0x00173e60), 10);
}

// Whether the moby hit last (0x173e58) is "soft" (its pvar record +9 == 1).
// orig: 0x002a7d90
int32_t hit_moby_soft() {
    const uint32_t h = lwu(OV(0x00173e58));
    const uint32_t r = h != 0 ? moby_pvar_word0(h) : 0u;
    if (r == 0) return 0;
    return lbu(r + 9) == 1 ? 1 : 0;
}

// Whether the moby hit last (0x173e58) has flag 2 in its pvar record +0x1e.
// orig: 0x002a7de0
int32_t hit_moby_flag2() {
    const uint32_t r = moby_pvar_word0(lwu(OV(0x00173e58)));
    if (r == 0) return 0;
    return (lhu(r + 0x1e) & 2u) != 0 ? 1 : 0;
}

// Whether a moby is not a crate (0 for none) or is the 0x1f6 crate.
// orig: 0x0025e368
int32_t moby_not_crate_or_1f6(uint32_t m) {
    if (m == 0) return 0;
    if (moby_in_range_class_500(m) == 0) return 1;
    return lh(m + 0xa6) == 0x1f6 ? 1 : 0;
}

// Turns the moby: its angles +0x40 composed with the rotation (x, y, z).
// orig: 0x00262480
void moby_rotate_by(uint32_t m, float x, float y, float z) {
    // (w of the angle vector: an unset stack word in the original, unused)
    const Vec4 r = Vec4{x, y, z, 0.0f};
    Mat4 a, b;
    euler_to_mat4_vu(a, r);
    euler_to_mat4_vu(b, V(m + 0x40));
    mat4_mul(b, b, a);
    mat_to_euler_v(b, m + 0x40);
}

// Whether the swept segment from (a0, a1) to (b0, b1), in `n` steps, hits anything (0x1efc70,
// flags 0): the first hit's step, as 1.
// orig: 0x00259d08
int32_t swept_collide(const Vec4& a0, const Vec4& a1, const Vec4& b0, const Vec4& b1, uint32_t ignore, const uint32_t* rec, int32_t n) {
    const float step = fpu_div(1.0f, int_to_float(n));
    Vec4 d0, d1, p0, p1;
    vec3_sub(d0, a0, b0);
    vec3_sub(d1, a1, b1);
    float t = step;
    for (int32_t i = 0; i < n; ++i) {
        vec3_scale(p0, d0, t);
        vec3_scale(p1, d1, t);
        vec3_add(p0, p0, b0);
        vec3_add(p1, p1, b1);
        if (segment_collide_v(p0, p1, 0, ignore, rec) != 0) return 1;
        t = t + step;
    }
    return 0;
}

// Scales a throw vector by how far it turns from the hero's facing 0x13f3e8: full speed
// straight ahead, 70% at 1.833 rad or more.
// orig: 0x002a8418
void throw_turn_scale_v(Vec4& v) {
    const float lim = u2f(0x3fea927fu);
    float d = angle_dist(lwc1(0x0013f3e8), atan2_f(v.x, v.y));
    if (lim < d) d = lim;
    const float len = vec3_length(v);
    const float k = fpu_div(lim - d, lim) * u2f(0x3e99999au) + u2f(0x3f333333u);
    vec3_set_length(v, v, len * k);
}
// orig: 0x002a8418 (EE-address form)
void throw_turn_scale(uint32_t v) { throw_turn_scale_v(V(v)); }

// The wrench follows the hero's swing: with the hero in the swing state 0x3b, the wrench
// takes his animation 0x40 / 0x41 (as 0x13 / 0x12, matching his frame) or else blends to set
// 0xb over 7 frames; in the state 0x3b sub-state with 0x13f4e8 == 1, set 1 over 17 frames.
// orig: 0x002a7c70
void wrench_follow_anim(uint32_t m) {
    constexpr uint32_t P = 0x0013f350;
    if (lw(P + 0x2084) == 0x3b) {
        const uint32_t h = lwu(P + 0x2080);
        const uint32_t a = lbu(h + 0x53);
        uint32_t set = 0xb;
        if (a == 0x40) set = 0x13;
        if (a == 0x41) set = 0x12;
        if (set - 0x12u < 2u) {
            sb(m + 0x53, set);
            sb(m + 0x52, set);
            swc1(m + 0x54, lwc1(lwu(P + 0x2080) + 0x54));
            sb(m + 0x50, lbu(lwu(P + 0x2080) + 0x50));
            const uint32_t f51 = lbu(lwu(P + 0x2080) + 0x51);
            sb(m + 0x51, f51);
            const uint32_t h2 = lwu(P + 0x2080);
            if (lbu(h2 + 0x52) != lbu(h2 + 0x53)) sb(m + 0x50, f51);
            sub_0024ebc0(m);
            return;
        }
        moby_anim_set(m, set, 0, frame_scale_i(7));
        return;
    }
    if (lw(P + 0x2090) == 0x3b && lw(P + 0x198) == 1) moby_anim_set(m, 1, 0, frame_scale_i(0x11));
}

namespace {
// The hero's sound channel P+0x221c stopped (0x28d918) when it still plays on the hero, and
// forgotten.
// orig: (helper) repeated in 0x002a84c8
void wrench_sound_stop() {
    constexpr uint32_t P = 0x0013f350;
    const int32_t ch = lw(P + 0x221c);
    if (ch != -1) {
        const uint32_t c = 0x0013e550 + static_cast<uint32_t>(ch) * 0x70;
        if (lwu(c + 0x88) == lwu(P + 0x2080) && lbu(c + 0x74) != 0) sub_0028d918(ch);
    }
    sw(P + 0x221c, 0xffffffffu);
}
}  // namespace

// The wrench (class 71). In hand (state 0): held by the hero (+0xb8), the trail drawn
// (0x2a7780), its animation following the hero's swing (sets 1, 0xe..0x10, combo 0x17bc28 by
// P+0xa60), the swing arc P+0x9f0..0xa20 from the hero's hand and the wrench tip; inside the
// combo's active frames it hits what the arc sweeps (0x259d08) or touches (0x1f2868) with
// strength 1 (2 in the hyper-strike 0x14), the first hit a frame (P+0xa64) sounding and
// sparking; a hard hit (hit flag 2) bounces the hero (state 0x21) back from it. Thrown
// (states 0xa out, 0xb back): it spins, keeps over the floor, flies out on +0x40 / +0x60
// slowing, then homes back to the hero's hand (P+0x1070), pulling in pickups, hitting what it
// passes or touches.
// orig: 0x002a84c8
void wrench_update(uint32_t m) {
    constexpr uint32_t P = 0x0013f350;
    const uint32_t p = lwu(m + 0x78);
    sb(P + 0x10ac, 0);
    if (lw(P + 0x10b4) == 3) return;
    const uint32_t state = lbu(m + 0x20);
    const float dt = lwc1(0x0015ed6c);
    if (state == 0) {
        sw(m + 0xb8, lwu(P + 0x2080));
        const uint32_t hs0 = lwu(P + 0x2084);
        if (hs0 == 1) {
            if ((lwu(0x0013cae0) & 0xau) != 0) cam_effect_add(m, 0xff917267u, nullptr, 0x25, -1, 1.0f, 0.0f, 90.0f);
            sh(p + 0x7e, 0);
            Vec4 aim;
            vec_from_spherical_v(aim, 10.0f, lwc1(OV(0x00166dd8)), -lwc1(OV(0x00166dd4)));
            vec3_add(aim, aim, V(OV(0x00166dc0)));
            if (segment_collide_v(V(OV(0x00166dc0)), aim, 4, lwu(P + 0x2080), nullptr) != 0) {
                sh(p + 0x7e, hs0);
                V(p + 0x50) = V(OV(0x00173e60));
            }
        }
        draw_callback_add(OVF(0x002a7780), m);
        const uint32_t a = lbu(m + 0x53);
        if (a != 0) {
            if (a == 0xe) {
                if ((lbu(m + 0x70) & 2u) == 0) return;
                moby_anim_set(m, 1, 0, frame_scale_i(5));
                return;
            }
            const bool side = ((a - 0xfu) & 0xffu) < 2u;
            if (lbu(P + 0x20ae) != 0) {
                if (!side) {
                    moby_anim_set(m, 0xf, 0, frame_scale_i(4));
                    return;
                }
                if ((lbu(m + 0x70) & 2u) == 0 || a != 0xf) return;
                moby_anim_set(m, 0x10, 0, frame_scale_i(5));
                return;
            }
            if (side) {
                moby_anim_set(m, 0xe, 0, frame_scale_i(2));
                return;
            }
        }
        if ((lbu(m + 0x70) & 2u) != 0 && lw(P + 0x2084) != 0x3b && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
        wrench_follow_anim(m);
        const uint32_t hs1 = lwu(P + 0x2084);
        if (!(hs1 == 0x13 || hs1 == 0x21 || hs1 == 0x2b || hs1 == 0x70 || hs1 == 0x14)) {
            const uint32_t a2 = lbu(m + 0x53);
            if (!(a2 < 3) && a2 < 6) moby_anim_set(m, 1, 0, frame_scale_i(4));
        }
        const uint32_t hm = lwu(P + 0x2080);
        if (lbu(hm + 0x53) != lbu(hm + 0x52) || !(static_cast<int32_t>(lbu(hm + 0x51)) - static_cast<int32_t>(lbu(hm + 0x50)) < 3)) sh(p + 0x74, lhu(p + 0x74) + 1u);
        swc1(m + 0x58, lwc1(P + 0xa90));
        const uint32_t combo = OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c;
        if (lw(P + 0x208c) != 6 && lw(P + 0x2084) != 0x2b) return;
        bool live = false;  // inside the combo's active frames
        bool hard = false;
        if (lw(P + 0xa9c) == 0) {
            const float t = lwc1(P + 0xaa8);
            live = !(t < static_cast<float>(lw(combo + 0x1c))) && !(static_cast<float>(lw(combo + 0x20)) < t);
        }
        if (lh(p + 0x7c) != lh(p + 0x74)) {
            const uint32_t h = lwu(P + 0x2080);
            const uint32_t an = lbu(h + 0x52);
            if (an == lbu(h + 0x53)) {
                bool reset = false;
                float from = 0.0f;
                if (an == 0x17 || an == 0x18) {
                    reset = true;
                } else if (an == 0x19) {
                    reset = true;
                    from = 12.0f;
                } else if (an == 0x2b) {
                    reset = true;
                    from = 23.0f;
                }
                if (reset && from <= lwc1(P + 0xaa8)) {
                    swc1(p + 0x70, 1.0f);
                    sh(p + 0x7c, lhu(p + 0x74));
                }
            }
        }
        float turn = 0.0f;
        float yaw = lwc1(P + 0x98);
        V(P + 0xa20) = V(P + 0xa00);
        V(P + 0xa10) = V(P + 0x9f0);
        moby_chain_point(m, 1, P + 0xa00);
        moby_chain_point(lwu(P + 0x2080), 0, P + 0x9f0);
        const uint32_t hs2 = lwu(P + 0x2084);
        if (hs2 == 0x13 || hs2 == 0x70 || hs2 == 0x2b) {
            yaw = atan2_f(lwc1(P + 0xa00) - lwc1(P + 0x80), lwc1(P + 0xa04) - lwc1(P + 0x84));
            const float q = lw(OV(0x0017bc28) + lwu(P + 0xa60) * 0x2c + 4) == 1 ? u2f(0xbfc90fdbu) : u2f(0x3fc90fdbu);
            yaw = angle_add(yaw, q);
        }
        Vec4 arm;
        vec3_sub(arm, V(P + 0xa00), V(P + 0x9f0));
        vec3_set_length(arm, arm, vec3_length(arm) + u2f(0x3e2e147bu));
        if (lw(P + 0x208c) != 0xf && lw(P + 0x2084) != 0x70 && lw(P + 0x2084) != 0x14) {
            turn = angle_dist(lwc1(P + 0x98), atan2_f(lwc1(P + 0xa00) - lwc1(P + 0x80), lwc1(P + 0xa04) - lwc1(P + 0x84)));
            if (u2f(0x3fc90fdbu) < turn) live = false;
            throw_turn_scale_v(arm);
        }
        vec3_add(V(P + 0xa00), V(P + 0x9f0), arm);
        const uint32_t hs_now = lwu(P + 0x2084);
        if (hs_now != 0x2b && hs_now != 0x70 && turn < u2f(0x3f9c61aau) && frame_scale_i(10) < lw(P + 0x198)) {
            Vec4 a0 = V(0x0013f3d0);
            a0.z = a0.z + 0.5f;
            const float dir = atan2_f(lwc1(P + 0xa00) - lwc1(P + 0x80), lwc1(P + 0xa04) - lwc1(P + 0x84));
            Vec4 a1 = V(0x0013f3d0);
            a1.z = a1.z + 0.5f;
            Vec4 reach;
            reach.x = cos_vu(dir) * u2f(0x3fb33333u);
            reach.y = sin_vu(dir) * u2f(0x3fb33333u);
            reach.z = 0.0f;
            reach.w = 0.0f;  // (an unset stack word in the original)
            throw_turn_scale_v(reach);
            vec3_add(a1, a1, reach);
            if (lw(P + 0xa64) == 0 && segment_collide_v(a0, a1, 2, 0, nullptr) != 0 && sub_001f35d8() != 0) {
                const uint32_t h = lwu(OV(0x00173e58));
                if (h == 0 || moby_not_crate_or_1f6(h) != 0) {
                    sw(P + 0xa64, 1);
                    moby_sound_start(2, 0, m);
                    wrench_hit_burst(m);
                    if (lw(OV(0x00173e58)) != 0) {
                        Vec4 d;
                        d.x = cos_vu(yaw);
                        d.y = sin_vu(yaw);
                        d.z = 0.0f;
                        d.w = 0.0f;  // (an unset stack word in the original)
                        sub_00259bc8_v(lwu(OV(0x00173e58)), m, 0x10000, OV(0x00173e60), d, 1.0f);
                    }
                }
            }
        }
        if (lw(P + 0x2084) == 0x14 && sub_00211870(27.0f) != 0) {
            moby_sound_start(2, 0, m);
            Vec4 a0 = V(P + 0xa00);
            Vec4 a1 = a0;
            sub_002336e8_v(a0, a0, u2f(0x3f333333u));
            sub_002336e8_v(a1, a1, u2f(0xbf666666u));
            if (segment_collide_v(a0, a1, 2, 0, nullptr) != 0) wrench_hit_burst(m);
        }
        if (live) {
            float reach = 1.0f;
            int32_t strength = 1;
            if (lw(P + 0x2084) == 0x14) {
                reach = u2f(0x3fc66666u);
                strength = 2;
            }
            Vec4 d;
            d.x = cos_vu(yaw);
            d.y = sin_vu(yaw);
            d.z = 0.0f;
            d.w = 0.0f;  // (an unset stack word in the original)
            // the hit record (0x259888): direction, the wrench, kind 0x10000, (0, 1, class), strength, 1
            uint32_t rec[9];
            Vec4 rv = d;
            vec2_set_length(rv, rv, reach);
            rv.z = 1.0f;
            rv.w = 6000.0f;
            std::memcpy(rec, &rv, 16);
            rec[4] = m;
            rec[5] = 0x10000;
            rec[6] = 0x100u | (static_cast<uint32_t>(lhu(m + 0xa6)) << 16);
            rec[7] = f2u(static_cast<float>(strength));
            rec[8] = 1;
            if (swept_collide(V(P + 0x9f0), V(P + 0xa00), V(P + 0xa10), V(P + 0xa20), lwu(P + 0x2080), rec, 5) != 0) {
                const uint32_t h = lwu(OV(0x00173e58));
                if (h != 0) {
                    if (hit_moby_flag2() != 0) {
                        hard = true;
                    } else if (lw(P + 0xa64) == 0) {
                        const uint32_t h2 = lwu(OV(0x00173e58));
                        if (!(h2 != 0 && lw(h2 + 0x24) != 0 && lh(lwu(h2 + 0x24) + 0x46) == 0x14)) {
                            sw(P + 0xa64, 1);
                            moby_sound_start(hit_moby_soft(), 0, m);
                            wrench_hit_burst(m);
                        }
                    }
                }
            }
            Vec4 tip, at;
            vec3_sub(tip, V(P + 0xa00), V(P + 0x9f0));
            vec3_set_length(tip, tip, vec3_length(tip) - u2f(0x3dae147bu));
            vec3_add(at, tip, V(P + 0x9f0));
            float r = u2f(0x3eb33333u);
            if (lw(P + 0x208c) == 0xf) r = u2f(0x3f333333u);
            if (lw(P + 0x2084) == 0x14) r = u2f(0x3ef0a3d7u);
            if (mobys_in_sphere_v(at, r, 0, lwu(P + 0x2080), rec) != 0) {
                const uint32_t h = lwu(OV(0x00177f00));
                if (!(h != 0 && lw(h + 0x24) != 0 && lh(lwu(h + 0x24) + 0x46) == 0x12)) {
                    if (hit_moby_flag2() != 0) {
                        hard = true;
                    } else if (lw(P + 0xa64) == 0) {
                        sw(P + 0xa64, 1);
                        moby_sound_start(hit_moby_soft(), 0, m);
                        wrench_hit_burst(m);
                    }
                }
            }
        }
        if (!hard) return;
        hero_set_state(0x21, 1);
        const uint32_t h = lwu(OV(0x00173e58));
        if (h != 0) {
            swc1(P + 0xa68, atan2_f(lwc1(P + 0x80) - lwc1(h + 0x10), lwc1(P + 0x84) - lwc1(h + 0x14)));
        } else {
            swc1(P + 0xa68, angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        }
        return;
    }
    if (!(state < 0xc) || state < 0xa) return;
    // thrown
    trail_update(0x00140b00, 0);
    sw(m + 0xb8, lwu(P + 0x2080));
    sb(P + 0x10ac, 2);
    if (state == 0xa && lw(P + 0x221c) == -1) sw(P + 0x221c, static_cast<uint32_t>(moby_sound_start(0xe, 4, lwu(P + 0x2080))));
    moby_rotate_by(m, 0.0f, 0.0f, lwc1(0x0015ed6c) * u2f(0x41c37a14u));
    if (lw(P + 0xa64) != 0) {
        int32_t t = lw(p + 0x6c);
        const int32_t r = timer_tick_i32(t);
        sw(p + 0x6c, static_cast<uint32_t>(t));
        if (r != 0) sw(P + 0xa64, 0);
    }
    if (lbu(P + 0x20b3) == 0) {
        const float fl = floor_probe(0.5f, m + 0x10, 0);
        if (lwc1(m + 0x18) - fl < u2f(0x3f4ccccdu)) {
            const float target = fl + u2f(0x3f0ccccdu);
            if (lwc1(m + 0x18) < target) {
                approach_f(m + 0x18, target, lwc1(0x0015ed6c) * 4.0f);
            } else {
                approach_f(m + 0x18, target, lwc1(0x0015ed6c));
            }
        }
    }
    const Vec4 before = V(m + 0x10);
    Vec4 step;
    if (lbu(m + 0x20) == 0xa) {
        vec3_scale(step, V(P + 0x140), u2f(0x3f333333u));
        vec3_add(V(m + 0x10), V(m + 0x10), step);
        vec3_set_length(step, V(p + 0x40), lwc1(p + 0x60));
        vec3_add(V(m + 0x10), V(m + 0x10), step);
        const float sp = lwc1(p + 0x64) + lwc1(0x0015ed74) * 170.0f;
        swc1(p + 0x64, sp);
        approach_f(p + 0x60, 0.0f, sp);
        if (lwc1(p + 0x60) == 0.0f) sb(m + 0x20, 0xb);
    } else {
        wrench_pull_pickups(m);
        bool home = false;
        vec3_sub(step, V(P + 0x1070), V(m + 0x10));
        const float acc = lwc1(p + 0x64) + lwc1(0x0015ed70) * u2f(0x3f666666u);
        const float spd = lwc1(p + 0x60) + acc;
        swc1(p + 0x64, acc);
        swc1(p + 0x60, spd);
        const float dist = vec3_length(step);
        if (fpu_div(dist, lwc1(p + 0x60)) < static_cast<float>(frame_scale_i(5)) && lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(5));
        if (dist <= lwc1(p + 0x60)) {
            swc1(p + 0x60, dist);
            home = true;
        }
        vec3_set_length(step, step, lwc1(p + 0x60));
        vec3_add(V(m + 0x10), V(m + 0x10), step);
        if (fpu_div(dist, lwc1(p + 0x60)) < static_cast<float>(frame_scale_i(4)) && lw(P + 0xab0) != -1) {
            sub_00232640(2);
            sw(P + 0xab4, 0x1a);
            wrench_sound_stop();
        }
        if (home) {
            hero_sound_start(5, 0);
            wrench_sound_stop();
            retire_children(P + 0x17b0);
            sb(P + 0x10aa, 0);
            sb(m + 0x20, 0);
        }
    }
    bool touched = false;
    if (segment_collide_v(before, V(m + 0x10), 2, 0, nullptr) != 0 && sub_001f35d8() != 0) {
        const uint32_t h = lwu(OV(0x00173e58));
        if (h == 0 || moby_in_range_class_500(h) == 0) touched = lw(OV(0x00173e5c)) != 0;
    }
    if (touched || lh(p + 0x76) != 0) {
        sh(p + 0x76, 0);
        if (lw(P + 0xa64) == 0) {
            sw(p + 0x6c, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sw(P + 0xa64, 1);
            moby_sound_start(2, 0, m);
        }
        if (lbu(m + 0x20) == 0xa) {
            wrench_hit_burst(m);
            swc1(p + 0x60, lwc1(p + 0x60) * 0.5f);
            sb(m + 0x20, 0xb);
        }
    }
    const float ahead = lbu(m + 0x20) == 0xb ? u2f(0xbebd70a4u) : u2f(0x3f99999au);
    Vec4 fwd;
    vec3_set_length(fwd, V(p + 0x40), ahead);
    uint32_t rec[9];
    Vec4 rv = fwd;
    rv.z = 1.0f;
    rv.w = 6000.0f;
    std::memcpy(rec, &rv, 16);
    rec[4] = m;
    rec[5] = 0x10000;
    rec[6] = 0x100u | ((lhu(P + 0xa7c) + 1u) << 16);
    rec[7] = f2u(1.0f);
    rec[8] = 1;
    Vec4 tip;
    moby_chain_point_v(m, 1, tip);
    if (mobys_in_sphere_v(tip, u2f(0x3ecccccdu), 0, lwu(P + 0x2080), rec) == 0) {
        moby_chain_point_v(m, 0, tip);
        if (mobys_in_sphere_v(tip, u2f(0x3ecccccdu), 0, lwu(P + 0x2080), rec) == 0) return;
    }
    if (lw(P + 0xa64) != 0) return;
    const uint32_t h = lwu(OV(0x00173e58));
    if (h == 0) return;
    if (lw(h + 0x24) != 0 && lh(lwu(h + 0x24) + 0x46) == 0x14) return;
    const uint32_t r = moby_pvar_word0(h);
    if (r != 0 && !(lh(r + 0x1c) < 2) && lh(r + 0x14) == static_cast<int32_t>(rec[6] >> 16)) return;
    Vec4 mid;
    vec4_lerp_v(mid, tip, V(lwu(OV(0x00173e58)) + 0x10), 0.5f);
    spark_pairs_v(mid, 5);
    sw(p + 0x6c, static_cast<uint32_t>(frame_scale_i(0x1e)));
    sw(P + 0xa64, 1);
    moby_sound_start(hit_moby_soft(), 0, m);
}

// Class 1007 update: a marker that sits where the active camera +0x180 is (5, 5, 5 without
// one), dropped onto the hero's plane (his up axis +0xe0) at his height; parked at
// (15, 15, 15) when it is 2.5 or more off that plane, within 0.1 of the hero, the camera looks
// within 3/4 pi of the hero's facing, or the hero is still (speed P+0x164 under 0.1 dt).
// orig: 0x002a4e48
void moby_1007_update(uint32_t m) {
    const uint32_t C = OV(0x00166c80), P = 0x0013f350;
    const uint32_t cam = lwu(C + 0x180);
    if (cam == 0) {
        swc1(m + 0x10, 5.0f);
        swc1(m + 0x18, 5.0f);
        swc1(m + 0x14, 5.0f);
        return;
    }
    V(m + 0x10) = V(cam + 0x30);
    Vec4 d, t;
    vec3_sub(d, V(m + 0x10), V(0x0013f3d0));
    float k = vec3_dot(d, V(lwu(P + 0x2080) + 0xe0));
    vec3_scale(t, V(lwu(P + 0x2080) + 0xe0), k);
    vec3_sub(d, d, t);
    if (!(abs_f(k) < 2.5f)) return;
    if (vec3_length(d) < 0.1f || angle_dist(lwc1(P + 0x98), lwc1(C + 0x158)) < u2f(0x4016cbe4u) || lwc1(P + 0x164) < lwc1(0x0015ed6c) * 0.1f) {
        swc1(m + 0x10, 15.0f);
        swc1(m + 0x18, 15.0f);
        swc1(m + 0x14, 15.0f);
        return;
    }
    k = vec3_dot(V(m + 0x10), V(lwu(P + 0x2080) + 0xe0));
    vec3_scale(t, V(lwu(P + 0x2080) + 0xe0), k);
    vec3_sub(d, V(m + 0x10), t);
    k = vec3_dot(V(0x0013f3d0), V(lwu(P + 0x2080) + 0xe0));
    if (lw(P + 0x208c) == 0x11) return;
    vec3_scale(t, V(lwu(P + 0x2080) + 0xe0), k);
    vec3_add(V(m + 0x10), d, t);
}

// Ends effect slot `i` of the class-1471 manager: its state 0x161bf8[i] and value 0x161c88[i]
// cleared, its two regions 0x161c38[i] / 0x161c48[i] released.
// orig: 0x002e20f8
void fx1471_slot_end(int32_t i) {
    const uint32_t o = static_cast<uint32_t>(i) * 4;
    sw(OV(0x00161bf8) + o, 0);
    sw(OV(0x00161c88) + o, 0);
    if (lw(OV(0x00161c38) + o) != -1) {
        region_clear(lwu(OV(0x00161c38) + o));
        sw(OV(0x00161c38) + o, 0xffffffffu);
    }
    if (lw(OV(0x00161c48) + o) != -1) {
        region_clear(lwu(OV(0x00161c48) + o));
        sw(OV(0x00161c48) + o, 0xffffffffu);
    }
}

// Class 1471 update: an effect manager with three slots (state 0x161bf8: 1 idle, 2/3 starting
// -> 4 running, 4 asked to end). First frame: the slots cleared, always drawn. Then each
// requested slot starts (0x2e2250 with its value 0x161c88) and, when any started, its draw
// callback 0x2e2af0 is queued.
// orig: 0x002e1df0
void moby_1471_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        for (uint32_t i = 0; i < 3; ++i) {
            sw(OV(0x00161bf8) + i * 4, 0);
            sw(OV(0x00161c88) + i * 4, 0);
        }
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;
    int32_t started = 0;
    for (int32_t i = 0; i < 3; ++i) {
        const uint32_t o = static_cast<uint32_t>(i) * 4;
        const int32_t v = lw(OV(0x00161bf8) + o);
        if (v == 1) continue;
        if (v == 4) {
            fx1471_slot_end(i);
            continue;
        }
        if (v == 0) continue;
        ++started;
        fx1471_effect_start(lwu(OV(0x00161c88) + o), static_cast<uint32_t>(i));
        sw(OV(0x00161bf8) + o, 4);
    }
    if (started != 0) draw_callback_add(OVF(0x002e2af0), m);
}

// The floor band under a moby: a probe 0.5 above to 16 below (not under 0.5) its position; on
// a hit +0x84 / +0x88 are the hit height -+0.2, else both 0.
// orig: 0x0025a120
void moby_floor_band(uint32_t m) {
    Vec4 down = V(m + 0x10);
    down.z = down.z - 16.0f;
    if (down.z < 0.5f) down.z = 0.5f;
    Vec4 up = V(m + 0x10);
    up.z = up.z + 0.5f;
    if (segment_collide_v(up, down, 0x22, 0, nullptr) != 0) {
        swc1(m + 0x84, lwc1(OV(0x00173e68)) - u2f(0x3e4ccccdu));
        swc1(m + 0x88, lwc1(OV(0x00173e68)) + u2f(0x3e4ccccdu));
    } else {
        sw(m + 0x88, 0);
        sw(m + 0x84, 0);
    }
}

// The floor under a point (0x259698, half a unit up, flags 0x20).
// orig: 0x00259710
float floor_probe_20_v(const Vec4& pos) { return floor_probe_v(0.5f, pos, 0x20); }
// orig: 0x00259710 (EE-address form)
float floor_probe_20(uint32_t pos) { return floor_probe_v(0.5f, V(pos), 0x20); }

// Turns a moby's yaw +0x48 towards `target` with a damped spin *vel: accel times the heading
// error (as a fraction of pi/20, within +-1) less damp times the spin, within +-vmax (0: no
// limit) and never past the target.
// orig: 0x00258278
void yaw_turn_damped(uint32_t m, uint32_t vel, float target, float accel, float damp, float vmax) {
    const float err = angle_sub(target, lwc1(m + 0x48));
    float k = fpu_div(err, u2f(0x3e20d97cu));
    if (1.0f < k) {
        k = 1.0f;
    } else if (k < -1.0f) {
        k = -1.0f;
    }
    const float v0 = lwc1(vel);
    float v = v0 + (accel * k - damp * v0);
    swc1(vel, v);
    if (vmax != 0.0f) {
        if (vmax < v) {
            swc1(vel, vmax);
        } else if (v < -vmax) {
            swc1(vel, -vmax);
        }
    }
    if (abs_f(err) < lwc1(vel)) {
        swc1(vel, abs_f(err));
    } else if (lwc1(vel) < -abs_f(err)) {
        swc1(vel, -abs_f(err));
    }
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(vel)));
}

// Turns the angle *ang towards `target` with a spin *vel that speeds up by `accel` (up to vmax,
// and only as fast as it can still stop by `decel`) and slows to stop on it; a spin the wrong
// way is braked first. Returns the spin, or the error left when it lands on the target.
// orig: 0x0025be00
float yaw_seek(uint32_t ang, uint32_t vel, float target, float accel, float decel, float vmax) {
    const float err = angle_sub(target, lwc1(ang));
    const float v = lwc1(vel);
    if (!(0.0f <= v * err) || err == 0.0f) {
        turn_towards(vel, 0, 0.0f, decel);
        swc1(ang, angle_add(lwc1(vel), lwc1(ang)));
        return lwc1(vel);
    }
    const float stop = fpu_div(v * v, decel) * 0.5f;
    if (abs_f(err) < stop) {
        const float run = abs_f(err) + abs_f(lwc1(vel));
        if (stop < run) {
            approach_f(vel, 0.0f, decel);
        } else {
            approach_f(vel, 0.0f, decel * u2f(0x3f8ccccdu));
        }
    } else {
        float top = sqrt_f((decel + decel) * err);
        if (vmax < top) top = vmax;
        if (err < 0.0f) {
            turn_towards(vel, 0, -top, accel);
        } else {
            turn_towards(vel, 0, top, accel);
        }
    }
    if (abs_f(lwc1(vel)) < abs_f(err)) {
        swc1(ang, angle_add(lwc1(vel), lwc1(ang)));
        return lwc1(vel);
    }
    swc1(ang, target);
    return err;
}

namespace {
// orig: 0x001251e8 — (base ELF, VU0 macro) d = a * s on all four lanes (vmulx).
Vec4 vec4_scale_vu(const Vec4& a, float s) {
    auto r = [](float f) {
        uint32_t u = f2u(f);
        const uint32_t e = u & 0x7f800000u;
        if (e == 0) u &= 0x80000000u;
        else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
        return u2f(u);
    };
    return Vec4{r(a.x * s), r(a.y * s), r(a.z * s), r(a.w * s)};
}
// orig: (helper) a random point within radius r into p's x y z (rejection sampling, as 0x25aca0
// does inline)
void point_in_ball(Vec4& p, float r) {
    do {
        p.x = rand_range_f(-r, r);
        p.y = rand_range_f(-r, r);
        p.z = rand_range_f(-r, r);
    } while (!(vec3_length(p) <= r));
}
}  // namespace

// Random points on a moby's collision volumes (class +0x10: a list of 32-byte primitives, the
// last flagged in bit 31; joint-relative ones posed into the scratchpad first): the volumes
// enabled in `mask` (and by their flag 0x20000) are weighted by size (spheres 4/3 pi r^3,
// capsules plus pi/2 r^2 h, joint segments pi/2 r^2 len), density * total points (at most
// `max`) are drawn, each in a volume picked by weight, into `out`. Returns the count.
// orig: 0x0025aca0
int32_t moby_volume_points(float density, uint32_t m, int32_t max, Vec4* out, uint32_t mask) {
    const uint32_t col = lwu(lwu(m + 0x24) + 0x10);
    if (col == 0) return 0;
    if (lh(col + 2) != 0) moby_pose_matrices(m, lh(col + 2), 0);
    if (!(0 < lw(col + 4))) return 0;
    constexpr uint32_t S = 0x70000000u;
    const float k = u2f(0x3a800000u);  // 1/1024
    const float half_pi = u2f(0x3fc90fdbu);
    // the original's stack vectors (their unwritten lanes carry over between uses)
    Vec4 A{}, B{}, C{}, D{}, E{};
    float weight[64];
    uint32_t prim[64];
    int32_t count = 0;
    float total = 0.0f;
    uint32_t pr = col + 0x10;
    for (uint32_t idx = 0;; ++idx, pr += 0x20) {
        const uint32_t w0 = lwu(pr);
        const uint32_t type = w0 & 0xffu;
        if ((((mask >> (idx & 31u)) ^ 1u) & 1u) == 0 && (w0 & 0x20000u) != 0) {
            prim[count] = pr;
            float wgt = 0.0f;
            if (type == 3) {
                A = V(pr + 0x10);
                vec3_scale(A, A, lwc1(m + 0x2c));
                const float sc = lwc1(m + 0x2c);
                const float r = (A.w * sc) * k;
                const float h = lwc1(pr + 4) * (sc * k);
                wgt = ((r * u2f(0x408601feu)) * r) * r + ((r * half_pi) * r) * h;
            } else if (type < 3) {
                float r;
                if (type < 2) {
                    A = V(pr + 0x10);
                    vec4_scale(A, A, lwc1(m + 0x2c) * k);
                    r = A.w;
                } else {
                    A = V(S + lwu(pr + 4) * 16);
                    B = V(pr + 0x10);
                    vec3_add(A, A, B);
                    vec3_scale(A, A, lwc1(m + 0x2c) * k);
                    r = (lwc1(pr + 0xc) * lwc1(m + 0x2c)) * k;
                }
                wgt = ((r * u2f(0x40860a92u)) * r) * r;
            } else if (type == 4) {
                A = vec4_scale_vu(V(S + static_cast<uint32_t>(lh(pr + 4)) * 16), lwc1(m + 0x2c) * k);
                B = vec4_scale_vu(V(S + static_cast<uint32_t>(lh(pr + 6)) * 16), lwc1(m + 0x2c) * k);
                vec3_sub(C, A, B);
                const float len = vec3_length(C);
                const float r = (lwc1(pr + 0xc) * lwc1(m + 0x2c)) * k;
                wgt = ((r * half_pi) * r) * len;
            }
            // (other kinds: none exist; the original would add a stale stack weight)
            weight[count] = wgt;
            total = total + wgt;
            ++count;
        }
        if ((w0 >> 31) != 0) break;
    }
    int32_t n = static_cast<int32_t>(density * total + 0.5f);
    if (max < n) n = max;
    if (!(0 < n)) return n;
    for (int32_t i = 0; i < n; ++i) {
        float r = rand_range_f(0.0f, total);
        int32_t pick = 0;
        if (0 < count && !(r < weight[0])) {
            for (;;) {
                const float w = weight[pick];
                ++pick;
                r = r - w;
                if (!(pick < count)) break;
                if (r < weight[pick]) break;
            }
            if (!(pick < count)) pick = count - 1;  // (rounding: the original reads one past the list)
        }
        const uint32_t p = prim[pick];
        const uint32_t type = lbu(p);
        if (type == 3) {
            A = V(p + 0x10);
            vec3_scale(A, A, lwc1(m + 0x2c));
            const float r0 = A.w * lwc1(m + 0x2c);
            const float h0 = lwc1(p + 4) * lwc1(m + 0x2c);
            vec3_div(A, A, 1024.0f);
            const float rad = r0 * k;
            const float h = h0 * k;
            const float d = sqrt_f(rand_range_f(0.0f, rad * rad));
            const float z = rand_range_f(0.0f, h);
            const float a = rand_angle();
            B.x = cos_vu(a) * d;
            B.y = sin_vu(a) * d;
            B.z = z;
            vec3_add(B, B, V(m + 0x10));
            vec3_add(out[i], A, B);
        } else if (type < 3 || type == 4) {
            float r0;
            Vec4* q = &B;
            Vec4* c = &A;
            if (type < 2) {
                A = V(p + 0x10);
                vec3_scale(A, A, lwc1(m + 0x2c));
                r0 = A.w;
            } else if (type == 2) {
                A = V(S + lwu(p + 4) * 16);
                B = V(p + 0x10);
                vec3_add(A, A, B);
                vec3_scale(A, A, lwc1(m + 0x2c));
                r0 = lwc1(p + 0xc);
            } else {
                A = vec4_scale_vu(V(S + static_cast<uint32_t>(lh(p + 4)) * 16), lwc1(m + 0x2c));
                B = vec4_scale_vu(V(S + static_cast<uint32_t>(lh(p + 6)) * 16), lwc1(m + 0x2c));
                vec3_sub(C, B, A);
                vec3_scale(C, C, rand_range_f(0.0f, 1.0f));
                vec3_add(D, C, A);
                vec3_div(D, D, 1024.0f);
                r0 = lwc1(p + 0xc);
                q = &E;
                c = &D;
            }
            const float rad = (r0 * lwc1(m + 0x2c)) * k;
            if (type != 4) vec3_div(A, A, 1024.0f);
            point_in_ball(*q, rad);
            vec3_add(*q, *q, *c);
            mat3_transform(*q, *q, M(m + 0xc0));
            vec3_add(out[i], *q, V(m + 0x10));
        }
    }
    return n;
}

// Sparks off a hit moby: up to 20 points on its collision volumes in `mask` (14 per unit of
// volume, 0x25aca0), each thrown at 1.8 dt in a random direction plus `vel` (and 0.9 dt up);
// green on the levels flagged at 0x13e530, else red; 20..35 frames.
// orig: 0x0025c398
void moby_hit_sparks(uint32_t m, const Vec4& vel, uint32_t mask) {
    Vec4 pts[20];
    const int32_t n = moby_volume_points(14.0f, m, 0x14, pts, mask);
    if (n == 0 || !(0 < n)) return;
    // (v.w: an unset stack word in the original)
    Vec4 v = Vec4{};
    for (int32_t i = 0; i < n; ++i) {
        const float a = rand_angle();
        const float b = rand_angle();
        vec_from_spherical_v(v, lwc1(0x0015ed6c) * u2f(0x3fe66666u), a, b);
        vec3_add(v, v, vel);
        v.z = v.z + lwc1(0x0015ed6c) * u2f(0x3f666666u);
        const int32_t life = frame_scale_i(rand_range(0x14, 0x23));
        const int32_t fade = rand_range(0x28, 0x3c);
        if (lbu(0x0013e530) != 0) {
            spawn_particle_4_v(pts[i], v, 0x7000ffa0u, 0xff00u, static_cast<uint32_t>(life), 0x1e, static_cast<uint32_t>(fade), 1);
        } else {
            spawn_particle_4_v(pts[i], v, 0x7000a0ffu, 0xffu, static_cast<uint32_t>(life), 0x1e, static_cast<uint32_t>(fade), 1);
        }
    }
}

// The physics step of a walking enemy, with its motion record p: velocity +0 (z +8), gravity
// +0x10, ground friction +0x14, the speed it keeps +0x4c (none when grounded, +0x3e 2), the
// rise cap +0x1c, the body height +0x28, radius +0x20 (1/1024 units), flags +0x24, the
// state +0x3e (0 rising, 1 falling, 2 grounded). Gravity, hit sparks (+0x2e / +0x2f), friction;
// a fall onto the floor raises dust once (+0x5c, then the bounce damps for 7 frames); off a
// ledge a grounded walker stops; its animation speed +0x58 follows the jump arc to land on
// frame +0x54 (or +0x50 at the apex); it turns to +0x40; then it moves, pushes the mobys it
// touches (crates are hit), sweeps and collides with the world: the floor (normal z over
// 0.7071) grounds it, a wall bounces it (+0x30). Returns flags: 1 landed, 2 touched, 0x40 its
// animation ended, 0x100 out of the world or blocked.
// orig: 0x0025c698
uint32_t enemy_physics(uint32_t m, uint32_t p) {
    uint32_t r = 0;
    swc1(p + 8, lwc1(p + 8) - lwc1(p + 0x10));
    if (lbu(p + 0x2e) == 1 && lbu(p + 0x2f) != 0) moby_hit_sparks(m, V(p), lbu(p + 0x2f));
    float spd = vec2_length(V(p));
    approach_f_v(spd, lh(p + 0x3e) == 2 ? 0.0f : lwc1(p + 0x4c), lwc1(p + 0x14));
    vec2_set_length(V(p), V(p), spd);
    if (lwc1(p + 0x1c) < lwc1(p + 8)) swc1(p + 8, lwc1(p + 0x1c));
    Vec4 at = V(m + 0x10);
    at.z = at.z + lwc1(p + 0x28);
    float floor = floor_probe_v(0.5f, at, 0);
    Vec4 ahead;
    if (sub_001f35d8() == 0) {
        if (lwc1(p + 0x5c) == 0.0f && floor <= at.z && at.z + lwc1(p + 8) < floor) {
            swc1(p + 0x5c, 1.0f);
            at.z = floor;
            spawn_moby_307_v(at, 3.0f);
        }
        floor = floor_probe_20(m + 0x10);
    } else if ((lwu(p + 0x24) & 0x10u) != 0 || lh(p + 0x3e) == 2) {
        vec3_add(ahead, V(m + 0x10), V(p));
        const float f2 = floor_probe_20_v(ahead);
        if (f2 < floor - 0.25f) vec2_set_length(V(p), V(p), 0.0f);
    }
    if (lwc1(p + 0x5c) != 0.0f) {
        if (lwc1(p + 0x5c) < 4.0f) vec3_scale(V(p), V(p), u2f(0x3f333333u));
        const float b = lwc1(p + 0x5c);
        if (b < 7.0f) {
            swc1(p + 0x5c, b + 1.0f);
            swc1(p + 0x10, lwc1(p + 0x10) * 0.75f);
        }
    }
    {
        const float lo = 2.0f, hi = u2f(0x447f4000u);
        const float x = lwc1(m + 0x10), y = lwc1(m + 0x14), z = lwc1(m + 0x18);
        if (x < lo || hi < x || y < lo || hi < y || z < lo || hi < z) r = 0x100;
    }
    if (lwc1(p + 0x54) != -1.0f && lwc1(p + 0x50) != -1.0f) {
        const int32_t st = lh(p + 0x3e);
        if (st == 1) {
            if (lwc1(p + 0x54) <= moby_anim_time(m)) {
                sw(m + 0x58, 0);
            } else {
                const float a = lwc1(p + 0x10) * -0.5f;
                float t_hi = 0.0f, t_lo = 0.0f;
                const int32_t n = solve_quadratic(&t_hi, &t_lo, a, lwc1(p + 8) - a, lwc1(m + 0x18) - floor);
                float t = 60.0f;
                if (0 < n && 0.0f < t_hi && t_hi < 60.0f) t = t_hi;
                if (t < u2f(0x38d1b717u)) t = u2f(0x38d1b717u);
                float rate = fpu_div(lwc1(p + 0x54) - moby_anim_time(m), t);
                if (2.0f < rate) {
                    rate = 2.0f;
                } else if (rate < 0.0f) {
                    rate = 0.0f;
                }
                approach_f(m + 0x58, rate, lwc1(0x0015ed60) * 0.1f);
            }
        } else if (st == 0) {
            const float t = abs_f(fpu_div(lwc1(p + 8), lwc1(p + 0x10)));
            if (2.0f < t) {
                const float rate = fpu_div(lwc1(p + 0x50) - moby_anim_time(m), t);
                if (0.0f < rate) approach_f(m + 0x58, rate, lwc1(0x0015ed60) * 0.1f);
            }
        } else if (st == 2) {
            swc1(m + 0x58, 1.0f);
        }
    }
    if ((lbu(m + 0x70) & 2u) != 0) r |= 0x40;
    if ((lwu(p + 0x24) & 4u) == 0) yaw_turn_damped(m, p + 0x44, lwc1(p + 0x40), u2f(0x3ca3d70au), u2f(0x3e99999au), lwc1(p + 0x48));
    Vec4 from = V(m + 0x10);
    from.z = from.z + lwc1(p + 0x28);
    vec3_add(V(m + 0x10), V(m + 0x10), V(p));
    Vec4 to = V(m + 0x10);
    to.z = to.z + lwc1(p + 0x28);
    Vec4 probe;
    vec3_add(probe, to, V(p));
    const int32_t n = mobys_in_sphere_v(probe, int_to_float(lw(p + 0x20)) * u2f(0x3a8ccccdu), 0x10, m, nullptr);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t h = lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4);
        if (moby_in_range_class_500(h) != 0) {
            sub_00259bc8_v(lwu(OV(0x00177f00) + static_cast<uint32_t>(i) * 4), m, 0x10000, m + 0x10, V(p), 1.0f);
        } else if ((lwu(p + 0x24) & 0x20u) != 0 && lh(p + 0x3e) == 2) {
            r |= 0x100;
        }
    }
    const float k = u2f(0x3a800000u);
    if (static_cast<float>(lw(p + 0x20)) * k < vec3_length(V(p))) {
        segment_collide_v(from, to, 0x24, m, nullptr);
        to = V(OV(0x00173e60));
    }
    if (sphere_collide_v(to, int_to_float(lw(p + 0x20)) * k, 0x24, m) != 0) {
        r |= 2;
        V(m + 0x10) = V(OV(0x00173e70));
        swc1(m + 0x18, lwc1(m + 0x18) - lwc1(p + 0x28));
        Vec4 nrm, push;
        vec3_set_length(nrm, V(OV(0x00173e80)), 1.0f);
        if (u2f(0x3f34fdf4u) <= nrm.z) {
            const float d = vec3_dot(V(p), nrm);
            if (d < 0.0f) {
                if (lw(OV(0x00173e58)) == 0 || 0 < lw(OV(0x00173e5c))) {
                    r |= 1;
                    sh(p + 0x3e, 2);
                }
                vec3_set_length(push, nrm, d);
                vec3_sub(V(p), V(p), push);
                if (lh(p + 0x3e) == 2) {
                    float sp = vec3_length(V(p));
                    approach_f_v(sp, 0.0f, lwc1(p + 0x10));
                    vec3_set_length(V(p), V(p), sp);
                }
            }
        } else {
            const float d = vec3_dot(V(p), nrm);
            if (d < 0.0f) {
                vec3_set_length(push, nrm, d * -(lwc1(p + 0x30) + 1.0f));
                vec3_add(V(p), V(p), push);
            }
        }
    }
    if (lh(p + 0x3e) == 0 && lwc1(p + 8) < 0.0f) sh(p + 0x3e, 1);
    return r;
}

// Swaps the words *a / *b (mask 1), *b / *c (2), *c / *a (4), in that order.
// orig: 0x0025c088
void swap3_by_mask(uint32_t& a, uint32_t& b, uint32_t& c, uint32_t mask) {
    if ((mask & 1u) != 0) std::swap(a, b);
    if ((mask & 2u) != 0) std::swap(b, c);
    if ((mask & 4u) != 0) std::swap(c, a);
}

// A colour 0xaabbggrr with its r / g / b channels swapped by `mask` (0x25c088).
// orig: 0x0025c0e8
uint32_t color_swap_channels(uint32_t col, uint32_t mask) {
    if (mask == 0) return col;
    uint32_t r = col & 0xffu, g = (col & 0xff00u) >> 8, b = (col >> 16) & 0xffu;
    const uint32_t a = col >> 24;
    swap3_by_mask(r, g, b, mask);
    return (a << 24) | (b << 16) | (g << 8) | r;
}

// Spawns an effect moby (class 0x27f) at `pos` from the effect description `def` (its
// direction: `dir`, or def +0; colours +0x10..+0x1f and sizes +0x20..+0x30, draw distance
// +0x3c, the rest copied into its pvars), with `arg` at pvars +0x20; placed and hero-lit.
// orig: 0x002d3838
uint32_t effect_spawn(uint32_t def, uint32_t pos, uint32_t arg, uint32_t dir) {
    const uint32_t m = moby_spawn(0x27f);
    if (m == 0) return 0;
    sb(m + 0x30, 0xff);
    sb(m + 0x31, 1);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    const uint32_t p = lwu(m + 0x78);
    sb(m + 0x20, 0);
    sb(m + 0xbc, 0);
    lq_sq(m + 0x10, pos);
    lq_sq(p, dir != 0 ? dir : def);
    sw(p + 0x20, arg);
    sh(m + 0x32, lbu(def + 0x3c));
    for (uint32_t i = 0; i < 6; ++i) sb(p + 0x24 + i, lbu(def + 0x10 + i));
    sb(p + 0x68, lbu(def + 0x3d));
    sh(p + 0x6c, lhu(def + 0x40));
    for (uint32_t i = 0; i < 5; ++i) sb(p + 0x2a + i, lbu(def + 0x16 + i));
    sb(p + 0x69, lbu(def + 0x3e));
    sh(p + 0x6e, lhu(def + 0x42));
    for (uint32_t i = 0; i < 5; ++i) sb(p + 0x2f + i, lbu(def + 0x1b + i));
    sb(p + 0x6a, lbu(def + 0x3f));
    sh(p + 0x70, lhu(def + 0x44));
    for (uint32_t i = 0; i < 5; ++i) sw(p + 0x34 + i * 4, lwu(def + 0x20 + i * 4));
    sb(p + 0x6b, lbu(def + 0x48));
    sh(p + 0x72, lhu(def + 0x46));
    sw(p + 0x48, lwu(def + 0x34));
    sw(p + 0x4c, lwu(def + 0x38));
    sw(p + 0x64, 0xffffffffu);
    sb(p + 0x1c, lbu(def + 0x49));
    moby_update_transform(m);
    moby_copy_hero_color(m);
    return m;
}

// An enemy's death burst at `pos`: three sparks (colour pairs from 0x1e9030 / 0x1e9048 with
// red and green swapped) of size `scale`, two orange shards off `src` (if any), and the blast
// effect 0x1b02f0 sized `size` (none at 0; 13 when negative).
// orig: 0x0025f3e8
void enemy_death_burst(uint32_t src, uint32_t pos, float scale, float size) {
    Vec4 still;
    vec_clear(still);
    for (int32_t i = 0; i < 3; ++i) {
        const float fx = scale * 500000.0f;
        const float sp = rand_range_f(8.0f, 10.0f) * lwc1(0x0015ed6c);
        const uint32_t c0 = color_swap_channels(lwu(OV(0x001e9030) + static_cast<uint32_t>(rand_mod(6)) * 4), 1);
        const uint32_t c1 = color_swap_channels(lwu(OV(0x001e9048) + static_cast<uint32_t>(rand_mod(6)) * 4), 1);
        const int32_t a = frame_scale_i(0xf);
        const int32_t life = rand_range(a, frame_scale_i(0x14));
        const int32_t b = frame_scale_i(0x19);
        const int32_t fade = rand_range(b, frame_scale_i(0x1e));
        spawn_particle_0b_v(V(pos), still, c0, c1, static_cast<uint32_t>(life), static_cast<uint32_t>(fade), 0, 0, fx, sp * scale);
    }
    if (src != 0) {
        uint32_t r = 0x7f, g = 0x40, b = 0;
        swap3_by_mask(r, g, b, 1);
        shard_spawn_v(src, V(pos), still, static_cast<uint32_t>(frame_scale_i(0x14)), r & 0xff, g & 0xff, b & 0xff, 0x30, scale * 4.0f);
        r = 0x60; g = 0x20; b = 0;
        swap3_by_mask(r, g, b, 1);
        shard_spawn_v(src, V(pos), still, static_cast<uint32_t>(frame_scale_i(0x1d)), r & 0xff, g & 0xff, b & 0xff, 0x20, scale * 3.0f);
    }
    if (size == 0.0f) return;
    const float sz = 0.0f < size ? size : 13.0f;
    swc1(OV(0x001b02f0) + 0x20, sz);
    swc1(OV(0x001b02f0) + 0x24, sz);
    swc1(OV(0x001b02f0) + 0x28, sz);
    effect_spawn(OV(0x001b02f0), pos, 0, 0);
}

// An angle part way from a to b (the wrapped difference times t).
// orig: 0x0025b750
float angle_lerp(float a, float b, float t) { return angle_add(a, angle_sub(b, a) * t); }

// v rounded to n decimals (half up, truncated by the conversion).
// orig: 0x00261858
float round_decimals(int32_t n, float v) {
    int32_t k = 1;
    for (int32_t i = n; 0 < i; --i) k = k * 10;
    const float f = static_cast<float>(k);
    const float h = fpu_div(1.0f, f + f);
    return fpu_div(static_cast<float>(float_to_int((v + h) * f)), f);
}

// Whether the moby's animation crossed frame t this frame: past it (to 4 decimals, less than
// this frame's step +0x58 * +0x5c beyond).
// orig: 0x002618d8
int32_t anim_crossed(uint32_t m, float t) {
    const float now = moby_anim_time(m);
    const float past = round_decimals(4, now - t);
    const float step = round_decimals(4, lwc1(m + 0x58) * lwc1(m + 0x5c));
    return (t <= now && past < step) ? 1 : 0;
}

// Scales a joint node (+0x20..+0x28 to s), set up first (0x24f590) when the flag 0x15edb7
// allows it; otherwise an active node is released (0x24f608).
// orig: 0x00263ac8
void joint_node_scale(uint32_t m, uint32_t idx, uint32_t node, float sc) {
    if (lbu(0x0015edb7) != 0) {
        if (lbu(node + 1) == 0) sub_0024f590(m, idx, node);
        swc1(node + 0x28, sc);
        swc1(node + 0x20, sc);
        swc1(node + 0x24, sc);
        return;
    }
    if (lbu(node + 1) != 0) sub_0024f608(m, node);
}

// orig: 0x00125180 — (base ELF, VU0 macro) d = a's xyz normalised (len by vsqrt, 1/len by
// vdiv), w 0.
void vec3_normalize_vu(Vec4& d, const Vec4& a) {
    auto r = [](float f) {
        uint32_t u = f2u(f);
        const uint32_t e = u & 0x7f800000u;
        if (e == 0) u &= 0x80000000u;
        else if (e == 0x7f800000u) u = (u & 0x80000000u) | 0x7f7fffffu;
        return u2f(u);
    };
    const float l2 = r(r(r(a.x * a.x) + r(a.y * a.y)) + r(a.z * a.z));
    const float len = r(std::sqrt(std::fabs(l2)));
    const float q = len == 0.0f ? u2f(0x7f7fffffu) : r(1.0f / len);
    d = Vec4{r(a.x * q), r(a.y * q), r(a.z * q), 0.0f};
}

// A walker's step from `from` to `to` (both moved: `to` is where it ends, from's x / y follow):
// refused when the floor there is more than `step` from its height (`flags` 1: only up), slid
// along a wall the swept segment (raised by `up`, flags 2 also into the second collision set)
// meets when not steeper than `slope`, pushed out of what its sphere (radius r, `up` above) still
// touches (six tries; not with flags 2), and checked against the floor again. Returns 1 when
// it moved freely.
// orig: 0x00258490
int32_t walker_step_v(uint32_t m, Vec4& from, Vec4& to, uint32_t flags, float up, float r, float step, float slope) {
    int32_t ok = 1;
    const uint32_t f2 = flags & 2u;
    auto floor_check = [&] {
        to.z = to.z + up;
        const float fl = floor_probe_20_v(to);
        to.z = to.z - up;
        if (step < abs_f(fl - from.z)) {
            const bool down_ok = (flags & 1u) != 0;
            if ((!down_ok && fl < from.z) || from.z < fl) {
                to = from;
                ok = 0;
            }
        }
    };
    floor_check();
    Vec4 d, ahead, a, b;
    vec3_sub(d, to, from);
    vec3_set_length(ahead, d, r * u2f(0x3f99999au));
    Vec4 lift{};
    lift.z = up;
    vec3_add(a, from, lift);
    vec3_add(b, a, ahead);
    if (ok != 0 && segment_collide_v(a, b, f2 | 0x24u, m, nullptr) != 0 && 0 < lw(OV(0x00173e5c))) {
        const uint32_t n = OV(0x00173e80);
        if (slope <= atan2_f(lwc1(n + 8), vec2_length(V(n)))) {
            sw(n + 8, 0);
            vec3_normalize_vu(V(n), V(n));
            const float k = (-d.x * lwc1(n)) - d.y * lwc1(n + 4);
            if (0.0f < k) {
                vec3_scale(V(n), V(n), k);
                ok = 0;
                vec3_add(d, d, V(n));
            }
        }
        vec3_add(to, from, d);
    }
    if (f2 == 0) {
        const float h = up + r;
        for (int32_t i = 0; i < 6; ++i) {
            Vec4 c = to;
            c.z = c.z + h;
            if (sphere_collide_v(c, r, 0x24, m) == 0) break;
            const float ang = atan2_f(vec2_distance(to, V(OV(0x00173e60))), lwc1(OV(0x00173e68)) - to.z);
            if (lw(OV(0x00173e58)) != 0 || slope < ang) {
                to = V(OV(0x00173e70));
                ok = 0;
                to.z = to.z - h;
            }
        }
    }
    floor_check();
    from.x = to.x;
    from.y = to.y;
    return ok;
}
// orig: 0x00258490 (EE-address form)
int32_t walker_step(uint32_t m, uint32_t from, uint32_t to, uint32_t flags, float up, float r, float step, float slope) {
    return walker_step_v(m, V(from), V(to), flags, up, r, step, slope);
}

// A wanderer's step (record r: home +0, radius +0x10, speed +0x14, turn rate +0x18, range
// +0x1c, shyness distance +0x20, heading +0x24, timer +0x28 / +0x2a, turn interval +0x2c..+0x2e):
// now and then a new heading (+-pi/4..2.6), turned towards; it walks (0x258490), sits on the
// floor; blocked or out of range it heads home, near the hero it shies away.
// orig: 0x00261630
void wander_update(uint32_t m, uint32_t r, float up, float step) {
    if (lh(r + 0x28) == 0) {
        swc1(r + 0x24, lwc1(r + 0x24) + rand_range_signed_f(u2f(0x3f490fdbu), u2f(0x40278d36u)));
        sh(r + 0x2a, static_cast<uint32_t>(rand_range(lh(r + 0x2c), lh(r + 0x2e))));
        sh(r + 0x28, 1);
    } else {
        turn_towards(m + 0x48, 0, lwc1(r + 0x24), lwc1(r + 0x18));
        int16_t t = static_cast<int16_t>(lh(r + 0x2a));
        const int32_t done = timer_tick_i16(t);
        sh(r + 0x2a, static_cast<uint16_t>(t));
        if (done != 0) sh(r + 0x28, 0);
    }
    Vec4 to = V(m + 0x10);
    to.x = to.x + cos_vu(lwc1(m + 0x48)) * lwc1(r + 0x14);
    to.y = to.y + sin_vu(lwc1(m + 0x48)) * lwc1(r + 0x14);
    const int32_t moved = walker_step_v(m, V(m + 0x10), to, 0, up, lwc1(r + 0x10), step, u2f(0x3f060a92u));
    swc1(m + 0x18, floor_probe(0.5f, m + 0x10, 0));
    const float far = vec2_distance(V(m + 0x10), V(r));
    if (moved == 0 || lwc1(r + 0x1c) < far) {
        swc1(r + 0x24, atan2_f(lwc1(r) - lwc1(m + 0x10), lwc1(r + 4) - lwc1(m + 0x14)));
        const int32_t a = frame_scale_i(0x1e);
        sh(r + 0x2a, static_cast<uint32_t>(rand_range(a, frame_scale_i(0x5a))));
        sh(r + 0x28, 1);
        return;
    }
    const float near = vec2_distance(V(m + 0x10), V(0x0013f3d0));
    if (near < lwc1(r + 0x20)) {
        const float k = fpu_div(near, lwc1(r + 0x20));
        const float away = atan2_f(lwc1(m + 0x10) - lwc1(0x0013f3d0), lwc1(m + 0x14) - lwc1(0x0013f3d4));
        swc1(r + 0x24, angle_lerp(lwc1(r + 0x24), away, k));
    }
}

// A shard's frame (class 0x70): it grows to its scale over its life (+0x14 frames, count +0x1c),
// tumbles (pi a second on each angle), fades its alpha +0x23 from +0x1e over the last half and
// is retired when the count runs out.
// orig: 0x002acad8
void shard_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    const float pi = u2f(0x40490fdbu);
    float s = int_to_float(lw(p + 0x14) - lh(p + 0x1c));
    const float n = int_to_float(lw(p + 0x14));
    s = s * lwc1(p + 0x18);
    swc1(m + 0x2c, fpu_div(s, n));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(0x0015ed6c) * pi));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(0x0015ed6c) * pi));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(0x0015ed6c) * pi));
    int16_t t = static_cast<int16_t>(lh(p + 0x1c));
    const int32_t done = timer_tick_i16(t);
    sh(p + 0x1c, static_cast<uint16_t>(t));
    if (done != 0) {
        moby_retire(m);
        return;
    }
    const int32_t half = lw(p + 0x14) / 2;
    const int32_t now = lh(p + 0x1c);
    if (now < half) sb(m + 0x23, static_cast<uint32_t>(ee_div(now * lh(p + 0x1e), half)));
}

// A debris chunk's frame (crate pieces, classes 0x95 / 0x15c..): falls (velocity pvar +0x10,
// gravity 20), spins (+0x20.. a frame); below its start height +4 less 3 it starts fading
// (state 1: shrinking by *(gp-0x5790), alpha +0x23 down by *(gp-0x578c), then retired);
// with bounces left (+0xc, every +0xe frames) its sphere (radius +8) touching the level from
// above bounces it (reflected, 3/4 speed, a new random spin).
// orig: 0x002afa48
void chunk_update(uint32_t m) {
    const uint32_t p = lwu(m + 0x78);
    swc1(p + 0x18, lwc1(p + 0x18) - lwc1(0x0015ed70) * 20.0f);
    vec3_add(V(m + 0x10), V(m + 0x10), V(p + 0x10));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(p + 0x20)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(p + 0x24)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(p + 0x28)));
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        swc1(m + 0x2c, lwc1(m + 0x2c) * lwc1(OV(0x00166c00u - 0x5790)));
        const uint32_t a = lbu(m + 0x23);
        if (static_cast<int32_t>(a) < lw(OV(0x00166c00u - 0x578c))) {
            moby_retire(m);
        } else {
            sb(m + 0x23, a - lbu(OV(0x00166c00u - 0x578c)));
        }
        return;
    }
    if (st != 0) return;
    if (lwc1(m + 0x18) < lwc1(p + 4) - 3.0f) {
        sb(m + 0x20, 1);
        return;
    }
    int16_t t = static_cast<int16_t>(lh(p + 0xe));
    const int32_t due = timer_tick_i16(t);
    sh(p + 0xe, static_cast<uint16_t>(t));
    if (due == 0 || lh(p + 0xc) == 0) return;
    if (sphere_collide_v(V(m + 0x10), lwc1(p + 8), 2, 0) == 0) return;
    if (!(vec3_dot(V(p + 0x10), V(OV(0x00173e80))) < 0.0f)) return;
    V(m + 0x10) = V(OV(0x00173e70));
    sh(p + 0xc, lhu(p + 0xc) - 1u);
    vec3_reflect(V(p + 0x10), V(p + 0x10), V(OV(0x00173e80)));
    vec3_scale(V(p + 0x10), V(p + 0x10), 0.75f);
    swc1(p + 0x20, 0.0f);
    const float deg = u2f(0x3c8efa35u);
    swc1(p + 0x24, rand_range_signed_f(lwc1(OV(0x00166c00u - 0x57b8)), lwc1(OV(0x00166c00u - 0x57b4))) * deg * lwc1(0x0015ed6c));
    swc1(p + 0x28, rand_range_signed_f(lwc1(OV(0x00166c00u - 0x57b8)), lwc1(OV(0x00166c00u - 0x57b4))) * deg * lwc1(0x0015ed6c));
}

// A class 0x70 shard: tinted (r, g, b), group `grp`, at `pos` with random angles, its pvars the
// vector `v`, `w10`, `life` frames (+0x1c: life less 4 frames) and the scale it shrinks from
// (class scale * `scale`, starting at (life - (life - 4f)) / life of it).
// orig: 0x002ac910
uint32_t shard_spawn_v(uint32_t w10, const Vec4& pos, const Vec4& v, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t grp, float scale) {
    r &= 0xff; g &= 0xff; b &= 0xff; grp &= 0xff;
    const uint32_t m = moby_spawn(0x70);
    if (m == 0) return 0;
    const float pi = u2f(0x40490fdbu), npi = u2f(0xc0490fdbu);
    const uint32_t p = lwu(m + 0x78);
    Vec4 ang{};
    ang.x = rand_range_f(npi, pi);
    ang.y = rand_range_f(npi, pi);
    ang.z = rand_range_f(npi, pi);
    sb(m + 0x20, 0);
    sb(m + 0x23, grp);
    sb(m + 0x31, 1);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    moby_set_color_hi(m, r, g, b);
    swc1(p + 0x18, lwc1(m + 0x2c) * scale);
    sw(m + 0x2c, 0);
    sw(p + 0x10, w10);
    V(m + 0x10) = pos;
    V(p) = v;
    V(m + 0x40) = ang;
    sw(p + 0x14, life);
    const int32_t f4 = frame_scale_i(4);
    const uint32_t rest = lhu(p + 0x14) - static_cast<uint32_t>(f4);
    sh(p + 0x1c, rest);
    float s = int_to_float(lw(p + 0x14) - static_cast<int32_t>(static_cast<int16_t>(rest)));
    const float n = int_to_float(lw(p + 0x14));
    s = s * lwc1(p + 0x18);
    swc1(m + 0x2c, fpu_div(s, n));
    sh(p + 0x1e, grp);
    moby_update_transform(m);
    return m;
}
// orig: 0x002ac910 (EE-address form)
uint32_t shard_spawn(uint32_t w10, uint32_t pos, uint32_t v, uint32_t life, uint32_t r, uint32_t g, uint32_t b, uint32_t grp, float scale) {
    return shard_spawn_v(w10, V(pos), V(v), life, r, g, b, grp, scale);
}

// A debris chunk of class `cls` thrown off `src` (its colour): at `pos` with rotation `rot`,
// velocity `vel`, spinning +-0x161448..0x16144c degrees a frame on two axes, 20 frames,
// scaled by lo..hi (+8 a quarter of it), +0xc `tag`.
// orig: 0x002af8b0
uint32_t chunk_spawn_v(uint32_t src, const Vec4& vel, const Vec4& pos, const Vec4& rot, uint32_t cls, uint32_t tag, float lo, float hi) {
    const uint32_t m = moby_spawn(cls);
    if (m == 0) return 0;
    const uint32_t p = lwu(m + 0x78);
    sw(p, src);
    sh(m + 0x32, 0x7e);
    sb(m + 0x31, 1);
    sb(m + 0x30, 0xff);
    sb(m + 0x20, 0);
    sd(m + 0x38, ld(src + 0x38));
    swc1(m + 0x40, rand_angle());
    swc1(m + 0x44, rand_angle());
    swc1(m + 0x48, rand_angle());
    V(m + 0x10) = pos;
    V(m + 0x40) = rot;
    V(p + 0x10) = vel;
    const float deg = u2f(0x3c8efa35u);
    swc1(p + 4, lwc1(m + 0x18));
    const int32_t t = frame_scale_i(0x14);
    sw(p + 0x20, 0);
    sh(p + 0xe, static_cast<uint32_t>(t));
    swc1(p + 0x24, (rand_range_signed_f(lwc1(OV(0x00161448)), lwc1(OV(0x0016144c))) * deg) * lwc1(0x0015ed6c));
    swc1(p + 0x28, (rand_range_signed_f(lwc1(OV(0x00161448)), lwc1(OV(0x0016144c))) * deg) * lwc1(0x0015ed6c));
    const float k = rand_range_f(lo, hi);
    swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), k));  // (the PS2 FPU's product: a missing class's scale clamps)
    sh(p + 0xc, tag);
    swc1(p + 8, k * 0.25f);
    moby_update_transform(m);
    return m;
}
// orig: 0x002af8b0 (EE-address form)
uint32_t chunk_spawn(uint32_t src, uint32_t vel, uint32_t pos, uint32_t rot, uint32_t cls, uint32_t tag, float lo, float hi) {
    return chunk_spawn_v(src, V(vel), V(pos), V(rot), cls, tag, lo, hi);
}

// The debug trap (`teq zero, zero`): the game stops in the debugger. The port stops too.
// orig: 0x001ff008
bool g_debug_trap_is_noop = false;  // oracle tests only: the EE interpreter treats the trap as a no-op
void debug_trap() {
    if (g_debug_trap_is_noop) return;
    throw std::runtime_error("EE debug trap (0x1ff008): the original halts here");
}

namespace {
// Crates of kind 0x1f4 turn a quarter (one in two) when they stand level; crates of kind
// 0x1f5 whose pickup slot 0x15ee90[+0xb0] was taken after this one (pvars +0xf8) are gone.
// Returns false when the crate was retired.
// orig: (helper) repeated in 0x002cfcb8
bool crate_settle_start(uint32_t m, uint32_t p) {
    if (lh(m + 0xa6) == 0x1f4 && rand_mod(2) != 0 && lwc1(m + 0x40) == 0.0f && lwc1(m + 0x44) == 0.0f) {
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x3fc90fdbu)));
    }
    if (lh(m + 0xa6) == 0x1f5 && lbu(m + 0xb0) != 0xff && lw(0x0015ee90 + lbu(m + 0xb0) * 4) < lw(p + 0xf8)) {
        moby_retire(m);
        return false;
    }
    sw(p + 0xac, lwu(p + 0xac) | 1u);
    sh(p + 0xc4, lhu(m + 0xa8));
    swc1(p + 0xb0, 1.0f);
    return true;
}

// The segment down from `above` over the crate to z 0.1, against the level and the mobys.
// orig: (helper) repeated in 0x002cfcb8
int32_t crate_ray_down(uint32_t m, float above, uint32_t ignore) {
    Vec4 from = V(m + 0x10), to = V(m + 0x10);
    to.z = u2f(0x3dcccccdu);
    from.z = from.z + above;
    return segment_collide_v(from, to, 2, ignore, nullptr);
}
}  // namespace

// The crates settle at the start: each crate (alone, or every live crate of its group) is put on
// what is under it (the ground: its height; another crate: on top of it, stacked through
// pvars +0xa4 / +0xa0; a moving platform with pvars +8: riding it, 0x260668); then down each
// stack the platform ride and the path state (the paths 0x1b04b0[pvars +0xc0]) are handed up.
// orig: 0x002cfcb8
void crate_settle(uint32_t m0) {
    constexpr OvConst<0x00173e40u> HIT{};
    uint32_t p = lwu(m0 + 0x78);
    const uint32_t grp = lbu(m0 + 0x21);
    sw(p + 0xac, lwu(p + 0xac) | 4u);
    if (grp == 0xff) {
        const uint32_t m = m0;
        if (!crate_settle_start(m, p)) return;
        if (crate_ray_down(m, 0.5f, 0) == 0) {
            sw(p + 0xa4, 0);
            return;
        }
        const uint32_t hit = lwu(HIT + 0x18);
        if (hit == 0) {
            swc1(m + 0x18, lwc1(HIT + 0x28));
            sw(p + 0xa4, 0);
            return;
        }
        if (moby_in_range_class_500(hit) != 0) return;
        sw(p + 0xa4, 0);
        sw(p + 0xac, lwu(p + 0xac) | 8u);
        if (moby_pvar_word2(hit) == 0) return;
        if (m != 0) sh(m + 0x36, 0x7f80);
        sub_00260668(0, hit, m + 0x10, m + 0x40, p + 0xd0, p + 0xe0);
        sw(p + 0xf4, 0);
        sw(p + 0xf0, hit);
        return;
    }
    uint32_t c40 = 0, c44 = 0, c48 = 0;  // the walk cursors (the original's stack)
    group_first_r(c40, static_cast<int32_t>(grp), 0, 0);
    while (c40 != 0) {  // each crate onto what is under it
        const uint32_t m = c40;
        p = lwu(m + 0x78);
        if (moby_in_range_class_500(m) != 0 && crate_settle_start(m, p)) {
            if (crate_ray_down(m, 0.5f, 0) == 0) {
                sw(p + 0xa4, 0);
            } else if (const uint32_t hit = lwu(HIT + 0x18); hit == 0) {
                swc1(m + 0x18, lwc1(HIT + 0x28));
                sw(p + 0xa4, 0);
            } else if (moby_in_range_class_500(hit) == 0 && lhu(hit + 0xa6) - 0x128u >= 2u) {
                sw(p + 0xa4, 0);
                sb(m + 0x30, 0xff);
                sw(p + 0xac, lwu(p + 0xac) | 8u);
            } else if (lwu(hit + 0x78) == 0) {
                sw(p + 0xa4, 0);
            } else if (lbu(hit + 0x21) != lbu(m + 0x21)) {
                sw(p + 0xa4, 0);
            } else if (moby_in_range_class_500(hit) == 0) {
                sw(p + 0xcc, 0);
                sw(p + 0xa4, 0);
                swc1(m + 0x18, lwc1(HIT + 0x28));
            } else {  // on another crate of the group
                sw(p + 0xa4, lwu(HIT + 0x18));
                const uint32_t below = lwu(HIT + 0x18);
                lq_sq(m + 0x10, below + 0x10);
                vec3_add(V(m + 0x10), V(m + 0x10), V(below + 0xe0));
                const uint32_t bp = lwu(lwu(HIT + 0x18) + 0x78);
                if (moby_in_range_class_500(m) != 0) {
                    sw(bp + 0xa0, m);
                } else {
                    debug_trap();
                    sw(bp + 0xa0, 0);
                }
            }
        }
        group_next_r(c40, m, 0, 0);
    }
    group_first_r(c44, static_cast<int32_t>(grp), 0, 0);
    while (c44 != 0) {  // from each bottom crate up its stack
        const uint32_t m2 = c44;
        p = lwu(m2 + 0x78);
        if (moby_in_range_class_500(m2) != 0 && lwu(p + 0xa4) == 0) {
            const float up = lwc1(p + 0xb0) * 0.5f;
            if (crate_ray_down(m2, up, c40) != 0) {
                const uint32_t hit = lwu(HIT + 0x18);
                if (hit != 0 && moby_pvar_word2(hit) != 0) {
                    if (c40 != 0) sh(c40 + 0x36, 0x7f80);
                    sub_00260668(0, lwu(HIT + 0x18), m2 + 0x10, m2 + 0x40, p + 0xd0, p + 0xe0);
                    sw(p + 0xf4, 0);
                    sw(p + 0xac, lwu(p + 0xac) | 8u);
                    sw(p + 0xf0, lwu(HIT + 0x18));
                }
            }
            c40 = m2;
            while (c40 != 0) {
                const uint32_t cur = c40;
                p = lwu(cur + 0x78);
                const uint32_t below = lwu(p + 0xa4);
                const uint32_t q = below != 0 ? lwu(below + 0x78) : 0u;
                if (q != 0) {
                    sw(p + 0xcc, lwu(q + 0xcc));
                    if (lwu(q + 0xf0) != 0) {
                        sb(cur + 0x30, 0xff);
                        sw(p + 0xac, lwu(p + 0xac) | 8u);
                        lq_sq(p + 0xd0, q + 0xd0);
                        lq_sq(p + 0xe0, q + 0xe0);
                        sw(p + 0xf0, lwu(q + 0xf0));
                        swc1(p + 0xf4, lwc1(q + 0xf4) + 1.0f);
                    }
                }
                if ((lh(p + 0xc6) != 0 || lh(cur + 0xa6) == 0x1f5) && lwu(p + 0xa0) != 0) {
                    const int32_t idx = lw(p + 0xc0);
                    if (idx != -1) {
                        const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(idx) * 4);
                        const int32_t n = lw(path);
                        const uint32_t st = lwu(path + 0x1c);
                        if (n == 0 || st == 1 || st == 3 || st == 0x111) {
                            sw(p + 0xc0, 0xffffffffu);
                        } else if (st - 0x11u < 0x100u) {
                            if (static_cast<int32_t>(st - 0x10u) < n) {
                                sw(path + 0x1c, st + 1);
                            } else {
                                sw(p + 0xc0, 0xffffffffu);
                            }
                        } else {
                            sw(path + 0x1c, 0x11);
                            for (int32_t k = 0; k < lw(lwu(OV(0x001b04b0) + lwu(p + 0xc0) * 4)); ++k)
                                sw(lwu(OV(0x001b04b0) + lwu(p + 0xc0) * 4) + static_cast<uint32_t>(k) * 0x10 + 0x1c, 0);
                        }
                    }
                }
                c40 = lwu(p + 0xa0);
            }
        }
        group_next_r(c44, m2, 0, 0);
    }
    group_first_r(c40, static_cast<int32_t>(grp), 0, 0);
    while (c40 != 0) {  // (a distance pass whose result is not used)
        const uint32_t m = c40;
        if (moby_in_range_class_500(m) != 0) {
            group_next_r(c48, m, 0, 0);
            while (c48 != 0) {
                if (moby_in_range_class_500(c48) != 0) vec3_distance(V(m + 0x10), V(c48 + 0x10));
                group_next_r(c48, c48, 0, 0);
            }
        }
        group_next_r(c40, m, 0, 0);
    }
}

// The ported class update functions, by their EE address.
// orig: (none) — the table of ovl_call_moby_fn
void moby_class_fns_register() {
    ovl_register_moby_fn(0x002d37d0, moby_607_update);
    ovl_register_moby_fn(0x002e3d88, debris_piece_update);
    ovl_register_moby_fn(0x002d1e80, moby_530_update);
    ovl_register_moby_fn(0x002e3ca8, moby_plant_update);
    ovl_register_moby_fn(0x002df4f8, moby_1060_update);
    ovl_register_moby_fn(0x002a4cd8, moby_809_update);
    ovl_register_moby_fn(0x002e3a88, moby_1564_update);
    ovl_register_moby_fn(0x002e0988, moby_1413_update);
    ovl_register_moby_fn(0x002cf218, crate_update);
    ammo_pickup_fns_register();
    ovl_register_moby_fn(0x002a5dd8, bolt_update);
    ovl_register_moby_fn(0x002a84c8, wrench_update);
    ovl_register_moby_fn(0x002a4e48, moby_1007_update);
    ovl_register_moby_fn(0x002e1df0, moby_1471_update);
    ovl_register_moby_fn(0x002acad8, shard_update);
    ovl_register_moby_fn(0x002afa48, chunk_update);
    ovl_register_moby_fn(0x002e0b88, e1440_update);
    ovl_register_moby_fn(0x002d6f28, smoke_vent_update);
    ovl_register_moby_fn(0x002d89f0, nanotech_update);
    ovl_register_moby_fn(0x002db868, class_hook_retire);
    ovl_register_moby_fn(0x002d4610, e749_update);
}

}  // namespace rac1
