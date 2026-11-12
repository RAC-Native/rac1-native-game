// Novalis's own moby classes, part 2h: the Amoeboids (classes 572 / 865 / 866) and their helpers: the level's wall
// paths (the table 0x1b0930 of paths: count +0, points 0x10 apart from +0x10, a point's w a bit mask of the points it
// sees) and the way around them. Header novalis_mobys2.h. Addresses are Novalis's (`orig: L1:`); Novalis-only data
// is L1_ADDR, the base ELF's are bare. Registered in novalis_mobys2.cpp.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/metal.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
// the level's path `i` (Novalis's table 0x1b0930)
uint32_t level_path(int32_t i) { return lwu(L1_ADDR(0x001b0930) + static_cast<uint32_t>(i) * 4); }
uint32_t path_point(uint32_t path, int32_t i) { return path + 0x10 + static_cast<uint32_t>(i) * 16; }
float dt_l1() { return lwc1(0x0015ed6c); }  // the frame's time scale
}  // namespace

// The points of path `path` both point sets reach: `want` & `have`, `have` grown up to 32 times by what its points see
// (each point's w as a mask); 0 when they never meet.
// orig: L1:0x00276c40
uint32_t nov_276c40_l1(int32_t path, uint32_t want, uint32_t have) {
    const uint32_t p = level_path(path);
    for (int32_t round = 0; round < 32; ++round) {
        if ((want & have) != 0) return want & have;
        uint32_t grown = have;
        for (int32_t i = 0; i < 32; ++i)
            if (((static_cast<int32_t>(have) >> i) & 1) != 0) grown |= lwu(p + 0x1c + static_cast<uint32_t>(i) * 16);
        have = grown;
    }
    return 0;
}

// The points of path `path` seen from `pos` (bit i: point i): no wall path of the list `walls` (`n` indices) crosses the
// line to it - with a `radius`, neither line from the two sides of a body that wide (the side offset is the line
// crossed with a zero vector, as the original computes it).
// orig: L1:0x00276a48
uint32_t nov_276a48_l1(uint32_t walls, int32_t n, int32_t path, uint32_t pos, float radius) {
    const uint32_t p = level_path(path);
    uint32_t seen = 0;
    for (int32_t i = 0; i < lw(p);) {
        const int32_t next = i + 1;
        bool blocked = false;
        const Vec4& pt = V(path_point(p, i));
        for (int32_t k = 0; k < n && !blocked; ++k) {
            const uint32_t wall = lwu(walls + static_cast<uint32_t>(k) * 4);
            if (radius == 0.0f) {
                blocked = path_crossed_v(wall, V(pos), pt) != 0;
                continue;
            }
            const Vec4 zero{};
            Vec4 side{};
            vec3_sub(side, V(pos), pt);
            vec3_cross_ba(side, side, zero);
            vec3_set_length(side, side, radius);
            Vec4 left{}, right{};
            vec3_sub(left, V(pos), side);
            vec3_add(right, V(pos), side);
            blocked = path_crossed_v(wall, left, pt) != 0 || path_crossed_v(wall, right, pt) != 0;
        }
        if (!blocked) seen |= 1u << (i & 31);
        i = next;
    }
    return seen;
}

// Where to head from `from` to get to `to` past the wall paths of the list `walls` (`n` indices): `to` itself when no
// wall is in the way (the lines from both sides of a body `radius` wide); else the point of path `path` nearest `to`
// (in the plane) of those both ends reach through the path's points. 1 with the point in `out`, 0 when there is none.
// orig: L1:0x00276fe8
uint32_t nov_276fe8_l1(uint32_t walls, int32_t n, int32_t path, uint32_t from, uint32_t to, uint32_t out, float radius) {
    bool blocked = false;
    for (int32_t k = 0; k < n && !blocked; ++k) {
        const uint32_t wall = lwu(walls + static_cast<uint32_t>(k) * 4);
        if (radius == 0.0f) {
            blocked = path_crossed_v(wall, V(from), V(to)) != 0;
            continue;
        }
        Vec4 side{};
        vec3_sub(side, V(from), V(to));
        vec3_set_length(side, side, radius);
        side = Vec4{side.y, -side.x, side.z, side.x};  // (turned a quarter in the plane; its w the old x, as written)
        Vec4 left{}, right{};
        vec3_sub(left, V(from), side);
        vec3_add(right, V(from), side);
        blocked = path_crossed_v(wall, left, V(to)) != 0 || path_crossed_v(wall, right, V(to)) != 0;
    }
    if (!blocked) {
        lq_sq(out, to);
        return 1;
    }
    const uint32_t p = level_path(path);
    const uint32_t seen_from = nov_276a48_l1(walls, n, path, from, radius);
    const uint32_t seen_to = nov_276a48_l1(walls, n, path, to, radius);
    const uint32_t reach = nov_276c40_l1(path, seen_from, seen_to);
    if (reach == 0) return 0;
    int32_t best = -1;
    float best_d = u2f(0x461c4000u);
    for (int32_t i = 0; i < lw(p); ++i) {
        if (((static_cast<int32_t>(reach) >> i) & 1) == 0) continue;
        const float d = vec2_distance(V(path_point(p, i)), V(to));
        if (d < best_d) {
            best_d = d;
            best = i;
        }
    }
    if (best < 0) return 0;
    lq_sq(out, path_point(p, best));
    return 1;
}

// Where the segment `from` -> `to` (in the plane) first crosses the level's path `path` (its edges with a point's w
// set, in the segment's frame): `out` the point at that fraction (`to` when none); 1 when it crosses.
// orig: L1:0x00276820
uint32_t nov_276820_l1(int32_t path, uint32_t from, uint32_t to, uint32_t out) {
    Mat4 frame{};
    vec3_sub(frame.r[0], V(to), V(from));
    frame.r[0].z = 0.0f;
    const float len = vec3_length(frame.r[0]);
    vec3_set_length(frame.r[0], frame.r[0], fpu_div(1.0f, len));
    vec_clear(frame.r[1]);
    frame.r[1].x = frame.r[0].y;
    frame.r[1].y = -frame.r[0].x;
    vec_clear(frame.r[2]);
    const uint32_t p = level_path(path);
    Vec4 prev{};
    vec3_sub(prev, V(p + 0x10), V(from));
    mat3_transform(prev, prev, frame);
    float best = 1.0f;
    uint32_t found = 0;
    for (int32_t i = 1; i < lw(level_path(path)); ++i) {
        Vec4 cur{};
        vec3_sub(cur, V(path_point(level_path(path), i)), V(from));
        mat3_transform(cur, cur, frame);
        if (!(prev.w == 0.0f && cur.w == 0.0f) && fpu_mul(cur.y, prev.y) < 0.0f) {
            const float t = fpu_add(fpu_mul(fpu_div(fpu_sub(prev.x, cur.x), fpu_sub(prev.y, cur.y)), -cur.y), cur.x);
            if (0.0f < t && t < best) {
                best = t;
                found = 1;
            }
        }
        prev = cur;
    }
    vec3_lerp(V(out), V(from), V(to), best);
    return found;
}

// A particle of type 0x34 at `pos` (0.05 higher) for `life` frames, size `size` (x 210000), its two words `a` / `b`
// and its two floats `size` / `f`; its rate 1 / life.
// orig: L1:0x00287158
uint32_t nov_287158_l1(uint32_t pos, uint32_t a, uint32_t b, int32_t life, float size, float f) {
    const uint32_t pp = alloc_particle(0x34);
    if (pp == 0) return 0;
    lq_sq(pp + 0x10, pos);
    sw(pp + 4, a);
    sb(pp + 3, 0x44);
    swc1(pp + 0x18, fpu_add(lwc1(pp + 0x18), u2f(0x3d4ccccdu)));
    sb(pp + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x20));
    sb(pp + 1, 1);
    sb(pp + 2, lbu(lwu(L1_ADDR(0x001b25d0))));
    sh(pp + 0xa, static_cast<uint32_t>(life));
    swc1(pp + 0xc, fpu_mul(size, u2f(0x484d1400u)));
    sb(pp + 8, static_cast<uint32_t>(rand_mod(0xff)));
    sw(pp + 0x28, a);
    sw(pp + 0x2c, b);
    swc1(pp + 0x20, size);
    swc1(pp + 0x24, f);
    swc1(pp + 0x30, fpu_div(1.0f, int_to_float(life)));
    return pp;
}

namespace {
// the moby of an entry of a group list (Novalis's 0x1abcc0: halves, the moby index in the low 15 bits, the last with
// bit 15 set)
uint32_t group_moby(uint32_t e) { return lwu(L1_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8); }
}  // namespace

// An Amoeboid splits: the next size of its class (0x23c -> 0x361 -> 0x362) waiting in its group (+0x21) in state 0xc
// takes its place (its look, flags, position, angles, health +0x20 and size +0x224). That moby, or 0 when there is none.
// orig: L1:0x002ef350
uint32_t nov_2ef350_l1(uint32_t m) {
    const int32_t cls = lh(m + 0xa6);
    const int32_t want = cls == 0x23c ? 0x361 : cls == 0x361 ? 0x362 : -1;
    uint32_t e = lwu(L1_ADDR(0x001abcc0) + (static_cast<uint32_t>(lbu(m + 0x21)) << 2));
    if (e == 0) return 0;
    for (;; e += 2) {
        const uint32_t o = group_moby(e);
        if (lbu(o + 0x20) == 0xc && lh(o + 0xa6) == want) {
            const uint32_t opv = lwu(o + 0x78), mpv = lwu(m + 0x78);
            sb(o + 0x30, lbu(m + 0x30));
            sh(o + 0x32, lhu(m + 0x32));
            sb(o + 0x31, 1);
            sd(o + 0x38, ld(m + 0x38));
            sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10));
            sh(o + 0x34, lhu(m + 0x34));
            lq_sq(o + 0x10, m + 0x10);
            lq_sq(o + 0x40, m + 0x40);
            swc1(opv + 0x20, lwc1(mpv + 0x20));
            swc1(opv + 0x224, lwc1(mpv + 0x224));
            moby_update_transform(o);
            return o;
        }
        if (lh(e) < 0) return 0;
    }
}

// The nearest (within 120) visible moby of class 0x26f in group `group` that is alive (state below 0x7f); 0 when none.
// orig: L1:0x002ef478
uint32_t nov_2ef478_l1(uint32_t m, uint32_t group) {
    uint32_t e = lwu(L1_ADDR(0x001abcc0) + (group << 2));
    if (e == 0) return 0;
    uint32_t found = 0;
    float best = 120.0f;
    for (;; e += 2) {
        const uint32_t o = group_moby(e);
        if ((lhu(o + 0x34) & 0x1000u) != 0 && lh(o + 0xa6) == 0x26f && lbu(o + 0x20) < 0x7f) {
            const float d = vec3_distance(V(o + 0x10), V(m + 0x10));
            if (d < best) {
                best = d;
                found = group_moby(e);
            }
        }
        if (lh(e) < 0) return found;
    }
}

// The Amoeboid's splashes (Novalis's tuning block 0x1619ec..0x161a38): bursts from its position (jittered by its size
// +0x250) moving with `vel`, each of particles 0x02 between two jittered points with mixed colours.
// orig: L1:0x002ef770
void nov_2ef770_l1(uint32_t m, uint32_t vel) {
    const uint32_t mpv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t at = sp + 0x10, va = sp + 0x20, vb = sp + 0x30;
    for (int32_t k = 0; k < lw(L1_ADDR(0x001619ec)); ++k) {
        lq_sq(at, m + 0x10);
        metal_25f8e0(at, fpu_mul(lwc1(L1_ADDR(0x001619f4)), lwc1(mpv + 0x250)));
        swc1(at + 8, fpu_add(lwc1(at + 8), 1.0f));
        const float ang = rand_angle();
        const float speed = rand_range_f(lwc1(L1_ADDR(0x00161a00)), lwc1(L1_ADDR(0x00161a04)));
        const float rise = rand_range_f(lwc1(L1_ADDR(0x00161a08)), lwc1(L1_ADDR(0x00161a0c)));
        const float carry = rand_range_f(0.0f, lwc1(L1_ADDR(0x00161a38)));
        vec3_scale(V(va), V(vel), fpu_mul(carry, dt_l1()));
        swc1(va, fpu_add(lwc1(va), fpu_mul(cos_vu(ang), fpu_mul(speed, dt_l1()))));
        const float s = sin_vu(ang);
        const float t = dt_l1();
        swc1(va + 8, fpu_add(lwc1(va + 8), fpu_mul(rise, t)));
        swc1(va + 4, fpu_add(lwc1(va + 4), fpu_mul(s, fpu_mul(speed, t))));
        lq_sq(vb, va);
        const float frames = static_cast<float>(frame_scale_i(lw(L1_ADDR(0x00161a30))));
        swc1(vb + 8, fpu_sub(lwc1(vb + 8), fpu_mul(fpu_mul(lwc1(L1_ADDR(0x00161a10)), lwc1(0x0015ed70)), frames)));
        for (int32_t j = 0; j < lw(L1_ADDR(0x001619f0)); ++j) {
            const float r = rand_range_f(0.5f, 1.5f);
            swc1(va + 0xc, fpu_mul(r, lwc1(L1_ADDR(0x00161a14))));
            swc1(vb + 0xc, fpu_mul(r, lwc1(L1_ADDR(0x00161a18))));
            metal_25f8e0(at, lwc1(L1_ADDR(0x001619f8)));
            metal_25f8e0(vb, fpu_mul(lwc1(L1_ADDR(0x001619fc)), dt_l1()));
            const uint32_t c0 = rgba_lerp(lwu(L1_ADDR(0x00161a1c)), lwu(L1_ADDR(0x00161a24)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(L1_ADDR(0x00161a20)), lwu(L1_ADDR(0x00161a28)), rand_range_f(0.0f, 1.0f));
            const uint32_t n = static_cast<uint32_t>(frame_scale_i(lw(L1_ADDR(0x00161a2c))));
            const uint32_t h32 = static_cast<uint32_t>(frame_scale_i(lw(L1_ADDR(0x00161a30))));
            const float base = static_cast<float>(lw(L1_ADDR(0x00161a34)));
            const float life = rand_range_f(fpu_mul(base, 0.5f), fpu_mul(base, 2.5f));
            spawn_particle_02(at, va, vb, c0, c1, n, h32, static_cast<uint32_t>(float_to_int(frame_scale_f(life))), -1);
        }
    }
    ee_stack_free(0x110);
}

// The Amoeboid's drips (Novalis's tuning block 0x1619b0..0x1619e8, scaled by its size +0x250), while it is visible:
// now and then one of each of two kinds of particle 0x34 around it.
// orig: L1:0x002ef560
void nov_2ef560_l1(uint32_t m) {
    const uint32_t mpv = lwu(m + 0x78);
    if (lbu(m + 0x31) == 0) return;
    // one drip of a kind: its size range `lo` .. `hi` (floats), its life range `life_lo` .. `life_hi` (ints), its
    // particle's words `a` / `b`
    auto drip = [&](uint32_t lo, uint32_t hi, uint32_t life_lo, uint32_t life_hi, uint32_t a, uint32_t b) {
        const float ang = rand_angle();
        const float r = rand_range_f(0.0f, lwc1(L1_ADDR(0x001619e8)));
        const float size = fpu_mul(rand_range_f(0.0f, lwc1(lo)), lwc1(mpv + 0x250));
        const float f = fpu_mul(rand_range_f(lwc1(lo), lwc1(hi)), lwc1(mpv + 0x250));
        const int32_t life = float_to_int(rand_range_f(static_cast<float>(lw(life_lo)), static_cast<float>(lw(life_hi))));
        Vec4 at{};
        at.x = fpu_mul(cos_vu(ang), r);
        at.y = fpu_mul(sin_vu(ang), r);
        vec3_add(at, at, V(m + 0x10));
        const uint32_t sp = ee_stack_alloc(0x10);
        V(sp) = at;
        nov_287158_l1(sp, lwu(a), lwu(b), frame_scale_i(life), size, f);
        ee_stack_free(0x10);
    };
    if (rand_mod(lw(L1_ADDR(0x001619e0)) - 1) == 0)
        drip(L1_ADDR(0x001619b8), L1_ADDR(0x001619bc), L1_ADDR(0x001619c0), L1_ADDR(0x001619c4), L1_ADDR(0x001619b0), L1_ADDR(0x001619b4));
    if (rand_mod(lw(L1_ADDR(0x001619e4)) - 1) == 0)
        drip(L1_ADDR(0x001619d0), L1_ADDR(0x001619d4), L1_ADDR(0x001619d8), L1_ADDR(0x001619dc), L1_ADDR(0x001619c8), L1_ADDR(0x001619cc));
}

// The Amoeboid's frame before its state: the hit (mask 0x330000; ignored when a wall is between it and the source
// within 24 units): it splashes (0x2ef770) and loses a point of health +0x20, then splits into the two next sizes
// thrown 45 degrees either side of the hit (0x2ef350), or just goes when it was the smallest. Under the water (+0x258)
// it splashes and goes. Then its range +0x228 (20 more for 4 seconds after an alert), its target: what target_select
// finds on its path +0x24c (2 in +0x204 when it is out of range or 3 units off in height), a nearer class-0x26f moby of
// its group +0x248, else the hero; its colour pulsing.
// orig: L1:0x002eec68
void nov_2eec68_l1(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t eye = sp, from = sp + 0x10, hit_state = sp + 0x20, hit_dmg = sp + 0x24;
    sw(hit_dmg, 0);
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    if (hit != 0) {  // the hit counts only with a clear line from the source (when it is within 24)
        lq_sq(eye, m + 0x10);
        swc1(eye + 8, fpu_add(lwc1(eye + 8), 0.5f));
        const uint32_t src = lwu(hit + 0x20);
        if (src == 0) {
            lq_sq(from, hit);
        } else if (lh(src + 0xa6) == 0x47) {
            lq_sq(from, 0x0013f420);
        } else {
            const uint32_t cls = lwu(src + 0x24);
            if ((cls != 0 && lh(cls + 0x46) == 5) || lh(src + 0xa6) == 0xba) lq_sq(from, eye);
            else lq_sq(from, src + 0x10);
        }
        if (vec3_distance(V(eye), V(from)) < 24.0f && segment_collide_v(V(eye), V(from), 2, m, nullptr) != 0) hit = 0;
    }
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    const uint32_t state = lbu(m + 0x20);
    if (lw(hit_state) != 1 && lwc1(hit_dmg) != 0.0f && state != 0xb && state != 0xa && state != 8) {
        moby_sound_start(0, 0, m);
        const float k = u2f(0x3f4ccccdu);  // 0.8
        sb(pv + 0x9d, 0);
        swc1(pv + 0x70, u2f(0x3c03126fu));
        sw(pv + 0x84, 8);
        swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), 1.0f));
        sw(pv + 0x80, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(lwc1(pv + 0x250), k), 1024.0f))));
        swc1(pv + 0x88, fpu_mul(lwc1(pv + 0x250), k));
        nov_2ef770_l1(m, hit + 0x10);
        if (!(0.0f < lwc1(pv + 0x20))) {
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            moby_retire(m);
            ee_stack_free(0xd0);
            return;
        }
        // the two halves: thrown at the hit's yaw -45 / +45 degrees (the yaws kept in sp +0x28 / +0x2c)
        for (int32_t side = 0; side < 2; ++side) {
            const uint32_t o = nov_2ef350_l1(m);
            if (o == 0) continue;
            const uint32_t opv = lwu(o + 0x78);
            const uint32_t yaw = sp + 0x28 + static_cast<uint32_t>(side) * 4;
            sb(opv + 0x9d, 3);
            swc1(opv + 0x70, u2f(0x3c03126fu));
            sw(opv + 0x84, 8);
            sw(opv + 0x80, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(lwc1(pv + 0x250), k), 1024.0f))));
            const float t = dt_l1();
            swc1(opv + 0x88, fpu_mul(lwc1(pv + 0x250), k));
            swc1(opv + 0x78, fpu_mul(lwc1(L1_ADDR(0x001619a0)), t));
            swc1(opv + 0xb0, 7.0f);
            swc1(opv + 0xb4, 13.0f);
            swc1(opv + 0x7c, fpu_mul(lwc1(L1_ADDR(0x001619a4)), t));
            sw(opv + 0x84, lwu(opv + 0x84) | 4u);
            lq_sq(eye, hit + 0x10);
            vec_yaw_marked_v(V(eye), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(opv + 0x78)),
                             *static_cast<float*>(ee_ram_ptr(opv + 0x7c)));
            const float quarter = u2f(0x3f490fdbu);
            swc1(yaw, side == 0 ? angle_sub(lwc1(yaw), quarter) : angle_add(lwc1(yaw), quarter));
            move_state_start(o, opv + 0x60, 4, 8, 0, lwc1(yaw));
            swc1(o + 0x58, 1.0f);
            sb(o + 0x20, 8);
        }
        const uint32_t src = lwu(hit + 0x20);
        const uint32_t cls = src != 0 ? lwu(src + 0x24) : 0;
        moby_drop_bolts(m, (cls != 0 && lh(cls + 0x46) == 5) ? 0x800u : 0u, -1);
        moby_retire(m);
        ee_stack_free(0xd0);
        return;
    }
    sb(m + 0xa4, 0xff);
    const int32_t water = lw(pv + 0x258);
    if (water != 0 && lbu(m + 0x20) != 0 &&
        lwc1(m + 0x18) < lwc1(lwu(L1_ADDR(0x001612d0)) + static_cast<uint32_t>(water) * 0x1190 + 8)) {
        vec_clear(V(sp));
        nov_2ef770_l1(m, sp);
        moby_retire(m);
        ee_stack_free(0xd0);
        return;
    }
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x22c, static_cast<uint32_t>(frame_scale_i(0xf0)));
        sw(pv + 0x38, 0);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x22c))) == 0) swc1(pv + 0x228, fpu_add(lwc1(pv + 0x224), 20.0f));
    else swc1(pv + 0x228, lwc1(pv + 0x224));
    const int32_t leader = lw(pv + 0x260);
    if (leader <= 0 || lbu(lwu(L1_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(leader) << 8) + 0xbc) != 0) {
        const uint32_t path = level_path(lw(pv + 0x24c));
        if (target_select(m, pv + 0x1c0, 0, 0, path + 0x10, lw(path), lwc1(pv + 0x228)) != 2) {
            if (lwc1(pv + 0x228) < vec2_distance(V(pv + 0x210), V(pv + 0x1c0))) sw(pv + 0x204, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x1c8)))) sw(pv + 0x204, 2);
        }
    }
    const int32_t group = lw(pv + 0x248);
    if (group >= 0) {
        const uint32_t o = nov_2ef478_l1(m, static_cast<uint32_t>(group));
        if (o != 0) {
            const float d = vec2_distance(V(m + 0x10), V(o + 0x10));
            if (d < lwc1(pv + 0x228) && (lw(pv + 0x204) == 2 || d < vec2_distance(V(m + 0x10), V(lwu(pv + 0x200) + 0x10)))) {
                sw(pv + 0x200, o);
                sw(pv + 0x204, 1);
            }
        }
    }
    if (lwu(pv + 0x200) == 0) sw(pv + 0x200, lwu(0x001413d0));
    swc1(pv + 0x254, angle_add(lwc1(pv + 0x254), fpu_mul(dt_l1(), u2f(0x4116cbe4u))));
    const float pulse = fpu_mul(fpu_add(sin_vu(lwc1(pv + 0x254)), 1.0f), 0.5f);
    sw(m + 0x90, rgba_lerp(lwu(L1_ADDR(0x00161998)), lwu(L1_ADDR(0x0016199c)), pulse));
    ee_stack_free(0xd0);
}

// Classes 572 / 865 / 866 (0x23c / 0x361 / 0x362), the Amoeboids: blobs of three sizes that wander about their home
// (pvar +0x210), chase their target (+0x200, picked by 0x2eec68) round the walls (the wall list +0x240 and the way-point
// path +0x244: 0x276fe8), strike it at frame 15.5 of their attack, and split into two of the next size when hit.
// pvars: the move record +0x60, the walker record +0x170 (its speed +0x194 from the speed +0x23c), the yaw rate +0x220,
// the heading offset +0x234, the size +0x250, the target lost +0x204 (2), the alert timer +0x22c, the box +0x25c it
// wakes from, the water +0x258. States: 0 set up, 1 idle, 2 wanders home, 3 / 7 / 9 / 0x10 animation ends, 4 chases,
// 5 goes home, 6 attacks, 8 thrown, 0xc waits (split halves), 0xd hidden until the hero is in its box, 0xe carried,
// 0xf faces the target, 0x10 steps back from a wall.
// orig: L1:0x002edca0
void amoeboid_update_l1(uint32_t m) {
    if (lbu(m + 0x20) == 0xc) {  // (a half waiting for its split: hidden)
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        return;
    }
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, home = pv + 0x210;
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t step = sp, way = sp + 0x10;
    joint_node_scale(m, 0, pv + 0x270, u2f(0x4039999au));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L1_ADDR(0x00167240))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    nov_2eec68_l1(m);
    const uint32_t state = lbu(m + 0x20);
    const uint32_t target = lwu(pv + 0x200);
    const uint32_t tgt = target + 0x10;
    const float twenty_deg = u2f(0x3eb2b8c2u);
    auto anim_to = [&](uint32_t set, int32_t frames, int32_t frame = 0) {  // (frames as given: the caller scales them)
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, frame, frames);
    };
    // the walker's step along the heading (scaled by `k`: 1 or 2)
    auto walk = [&](bool twice) {
        float c = cos_vu(lwc1(m + 0x48));
        if (twice) c = fpu_add(c, c);
        swc1(step, c);
        float s = sin_vu(lwc1(m + 0x48));
        if (twice) s = fpu_add(s, s);
        sw(step + 8, 0);
        swc1(step + 4, s);
        walker_record_step(1.0f, m, pv + 0x170, step, pv + 0x40);
    };
    // the yaw turned toward (x, y) (plus `offset`): the slow turn (60 deg/s) or the fast one (720 deg/s)
    auto turn_to = [&](float x, float y, bool fast, float offset = 0.0f, bool with_offset = false) {
        float a = atan2_f(fpu_sub(x, lwc1(pos)), fpu_sub(y, lwc1(pos + 4)));
        if (with_offset) a = angle_add(a, offset);
        const float accel = fpu_mul(lwc1(0x0015ed70), fast ? u2f(0x41490fdbu) : u2f(0x3f860a92u));
        yaw_seek(m + 0x48, pv + 0x220, a, accel, accel, fpu_mul(dt_l1(), fast ? u2f(0x41c90fdbu) : u2f(0x40490fdbu)));
    };
    // the way to the target: through the walls with the Amoeboid's width, else with none
    auto find_way = [&](uint32_t from, uint32_t to, bool retry_from_to) {
        const float width = fpu_mul(lwc1(pv + 0x250), u2f(0x3f4ccccdu));
        if (nov_276fe8_l1(pv + 0x240, 1, lw(pv + 0x244), from, to, way, width) != 0) return true;
        // (on the way home the original retries from `to` to `to`: its register still held `to`)
        const uint32_t f2 = retry_from_to ? to : from;
        return nov_276fe8_l1(pv + 0x240, 1, lw(pv + 0x244), f2, to, way, 0.0f) > 0;
    };
    // a new wander: the heading offset re-rolled, the walker stopped
    auto new_wander = [&] {
        vec_clear(V(pv + 0x40));
        swc1(pv + 0x234, rand_range_f(-twenty_deg, twenty_deg));
    };
    bool drips = false;  // (the states that end with 0x2ef560)
    switch (state) {  // (the state's jump table L1:0x0020b130, 17 entries, by its index; 0xa..0xc: 0x002eec3c)
    case 0: {  // (0x002edd80) set up: its size by class, its health, its walker record; hidden (0xc / 0xd) unless in the water
        lq_sq(home, pos);
        const int32_t cls = lh(m + 0xa6);
        float size = lwc1(pv + 0x250);
        auto sized = [&](float b5a, float s) {
            sb(pv + 0x58, static_cast<uint32_t>(float_to_int(14.0f)));
            sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(b5a)));
            swc1(pv + 0x250, s);
            size = lwc1(pv + 0x250);
        };
        if (cls == 0x361) sized(u2f(0x40b33333u), u2f(0x3f333333u));
        else if (cls == 0x23c) sized(u2f(0x41266666u), 1.0f);
        else if (cls == 0x362) sized(4.0f, u2f(0x3efae148u));
        swc1(pv + 0x30, size);
        swc1(pv + 0x254, rand_angle());
        sw(pv + 0x130, lh(m + 0xa6) == 0x362 ? L1_ADDR(0x00161a50) : L1_ADDR(0x00161a40));
        sb(pv + 0x29, 1);
        if (lw(pv + 0x258) != 0) {
            sw(pv + 0x230, 1);
            swc1(pv + 0x20, 1.0f);
        } else {
            swc1(pv + 0x20, 3.0f);
        }
        if (lwc1(pv + 0x224) == 0.0f) swc1(pv + 0x224, 15.0f);
        walker_record_init_l1(pv + 0x170);
        swc1(pv + 0x178, 2.0f);
        swc1(pv + 0x17c, 2.0f);
        sw(pv + 0x170, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(lwc1(pv + 0x250), u2f(0x3f4ccccdu)), 1024.0f))));
        swc1(pv + 0x194, fpu_mul(lwc1(pv + 0x23c), dt_l1()));
        if (lw(pv + 0x230) != 0) {
            sb(m + 0x20, 1);
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            break;
        }
        sb(m + 0x20, lw(pv + 0x25c) != -1 ? 0xd : 0xc);
        sw(m + 0x94, 0);
        sw(m + 0x58, 0);
        sb(m + 0x31, 0);
        swc1(pos + 8, fpu_sub(lwc1(pos + 8), 20.0f));
        sh(m + 0x34, (lhu(m + 0x34) & 0xefffu) | 1u);
        break;
    }
    case 1:  // (0x002edfc4) idle: now and then looks for a way to the target (chase: 4), then faces it (0xf) or wanders (2)
    case 2:  // (0x002ee110) wanders home
        if (state == 2) {
            turn_to(lwc1(home), lwc1(home + 4), false);
            walk(false);
        }
        if (lw(pv + 0x204) != 2 && rand_mod(4) == 0) {
            const uint32_t out = state == 1 ? sp : way;
            const float width = fpu_mul(lwc1(pv + 0x250), u2f(0x3f4ccccdu));
            if (nov_276fe8_l1(pv + 0x240, 1, lw(pv + 0x244), pos, tgt, out, width) != 0) {
                if (state == 1) {
                    sb(m + 0x20, 4);
                    anim_to(1, frame_scale_i(0xa));
                    new_wander();
                } else {
                    sb(m + 0x20, 3);
                    anim_to(0, frame_scale_i(0xa));
                    new_wander();
                    swc1(pv + 0x194, fpu_mul(lwc1(pv + 0x23c), dt_l1()));
                }
            } else {
                sw(pv + 0x204, 2);
            }
        }
        if (lw(pv + 0x22c) != 0 && lw(pv + 0x204) == 2) {
            sb(m + 0x20, 0xf);
            anim_to(1, 5);
        } else if (state == 1) {
            swc1(pv + 0x194, fpu_mul(fpu_mul(lwc1(pv + 0x23c), 0.5f), dt_l1()));
            anim_to(1, 5);
            sb(m + 0x20, 2);
        }
        drips = state == 2;
        break;
    case 3:
    case 7:  // (0x002ee840) the animation done: chases
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 4);
        anim_to(1, frame_scale_i(0xa));
        break;
    case 4: {  // (0x002ee474) chases: attacks within reach (0x1619ac), else heads for the way point; lost: goes home (5)
        if (vec2_distance(V(pos), V(tgt)) < lwc1(L1_ADDR(0x001619ac))) {
            sb(m + 0x20, 6);
            swc1(pv + 0x194, fpu_mul(fpu_add(lwc1(pv + 0x23c), lwc1(pv + 0x23c)), dt_l1()));
            anim_to(2, frame_scale_i(0xa));
            break;
        }
        if (find_way(pos, tgt, false)) {
            turn_to(lwc1(way), lwc1(way + 4), true, lwc1(pv + 0x234), true);
            walk(true);
        } else {
            sw(pv + 0x204, 2);
        }
        if (lw(pv + 0x204) == 2) sb(m + 0x20, 5);
        drips = true;
        break;
    }
    case 5:  // (0x002ee888) goes home; there, chases again while it has a target
        if (vec2_distance(V(pos), V(home)) < 1.0f) {
            sb(m + 0x20, 1);
        } else if (find_way(pos, home, true)) {
            turn_to(lwc1(way), lwc1(way + 4), true, lwc1(pv + 0x234), true);
            walk(true);
        }
        if (lw(pv + 0x204) != 2) {
            sb(m + 0x20, 4);
            anim_to(1, frame_scale_i(3));
        }
        drips = true;
        break;
    case 6: {  // (0x002ee61c) attacks: steps in during frames 10..16, strikes at 15.5 within 2 units (and in height its size x 2)
        turn_to(lwc1(tgt), lwc1(tgt + 4), true);
        const float frame = moby_anim_scalar_l1(m);
        if (lbu(m + 0x52) == lbu(m + 0x53) && 10.0f <= frame && frame <= 16.0f) walk(true);
        if (anim_crossed(m, 15.5f) != 0 && vec2_distance(V(pos), V(tgt)) < 2.0f &&
            abs_f(fpu_sub(lwc1(tgt + 8), lwc1(pos + 8))) < fpu_add(lwc1(pv + 0x250), lwc1(pv + 0x250))) {
            swc1(step, cos_vu(lwc1(m + 0x48)));
            swc1(step + 4, sin_vu(lwc1(m + 0x48)));
            swc1(step + 0xc, u2f(0x45afdf66u));
            sw(step + 8, 0);
            lq_sq(way, tgt);
            swc1(way + 8, fpu_add(lwc1(way + 8), 0.75f));
            sub_00259bc8(target, m, 0x10001, way, step, 1.0f);
            break;
        }
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 7);
        anim_to(3, frame_scale_i(0xa));
        swc1(pv + 0x194, fpu_mul(lwc1(pv + 0x23c), dt_l1()));
        break;
    }
    case 8: {  // (0x002eea2c) thrown: landed where it can get home it carries on (9) from there; else it bursts
        const uint32_t r = enemy_physics(m, pv + 0x60);
        if ((r & 1) != 0) {
            if (nov_276fe8_l1(pv + 0x240, 1, lw(pv + 0x244), pos, home, sp, fpu_mul(lwc1(pv + 0x250), u2f(0x3f4ccccdu))) != 0) {
                moby_sound_start(4, 0, m);
                swc1(pv + 0x194, fpu_mul(lwc1(pv + 0x23c), dt_l1()));
                sb(m + 0x20, 9);
                anim_to(5, frame_scale_i(0xa), 4);
                lq_sq(home, pos);
            } else {
                nov_2ef770_l1(m, L1_ADDR(0x0015f580));
                moby_drop_bolts(m, 0, -1);
                moby_retire(m);
            }
        } else if ((r & 0x120) != 0) {
            moby_retire(m);
        }
        break;
    }
    case 9:  // (0x002eeb2c) the landing done: chases
        if ((lbu(m + 0x70) & 2) == 0) break;
        sb(m + 0x20, 4);
        anim_to(1, frame_scale_i(0xa));
        new_wander();
        swc1(pv + 0x194, fpu_mul(lwc1(pv + 0x23c), dt_l1()));
        break;
    case 0xd:  // (0x002eebac) hidden until the hero is in its box +0x25c: then drops in, 20 higher
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        if (point_in_box(0x0013f420, lw(pv + 0x25c)) == 0) break;
        {
            const uint32_t flags = lhu(m + 0x34) | 0x1000u;
            sb(m + 0x20, 1);
            sh(m + 0x34, flags);
            swc1(m + 0x58, 1.0f);
            swc1(pos + 8, fpu_add(lwc1(pos + 8), 20.0f));
            sb(m + 0x31, 1);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, flags & 0xfffeu);
        }
        break;
    case 0xe:  // (0x002eec20) carried (Veldin's carry_update, the same code on Novalis): back to idle when it settles
        if (carry_update(m, pv + 0x60) != 0) {
            sb(m + 0x20, 1);
            sh(pv + 0x128, 0);
        }
        break;
    case 0xf: {  // (0x002ee2b0) faces the target: strikes when within 2 units, takes a step back (0x10) on a wall near it
        turn_to(lwc1(tgt), lwc1(tgt + 4), false);
        walk(false);
        bool near_wall = false;
        if (nov_276820_l1(lw(pv + 0x240), pos, tgt, way) != 0 && vec3_distance(V(pos), V(way)) < 1.5f) near_wall = true;
        if (lw(pv + 0x22c) == 0) {
            sb(m + 0x20, 5);
            break;
        }
        if (near_wall) {
            sb(m + 0x20, 0x10);
            anim_to(0, frame_scale_i(0xa));
            break;
        }
        if (!(vec2_distance(V(pos), V(tgt)) < 2.0f)) break;
        sb(m + 0x20, 6);
        anim_to(2, frame_scale_i(0xa), 5);
        break;
    }
    case 0x10:  // (0x002ee43c) the step back done: goes home once the alert is over
        if ((lbu(m + 0x70) & 2) == 0) break;
        if (lw(pv + 0x22c) != 0) break;
        sb(m + 0x20, 5);
        anim_to(1, frame_scale_i(0x14));
        break;
    default:
        break;
    }
    if (drips) nov_2ef560_l1(m);
    ee_stack_free(0xb0);
}

}  // namespace rac1
