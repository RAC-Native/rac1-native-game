// Eudora's flocks (header eudora_flock.h): class 86 spawns its flock of class 85 about its place on its first frame and
// steers each member every frame after (boids: kept inside an area, toward a leader and home, apart from the others,
// off the floor and the walls, a wander), while the flock itself circles its place or moves along a path ahead of its
// nearest member. Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
// Registered in eudora_mobys.cpp.
#include <game/ovl/eudora_flock.h>

#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
float turn_dt() { return lwc1(0x0015ed60); }  // (the time scale of the turns)
}  // namespace

// orig: L4:0x001f3038
int32_t flock_nearest_point_l4(uint32_t m, uint32_t pts, int32_t n) {
    int32_t nearest = -1;
    float best = 32767.0f;
    for (int32_t i = 0; i < n; ++i) {
        const float d = vec2_distance(V(m + 0x10), V(pts + static_cast<uint32_t>(i) * 16));
        if (d < best) {
            best = d;
            nearest = i;
        }
    }
    return nearest;
}

// orig: L4:0x0024c4f0
int32_t polygon_outside_edge_l4(uint32_t p, uint32_t pts, int32_t n) {
    const float px = lwc1(p), py = lwc1(p + 4);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t a = pts + static_cast<uint32_t>(i) * 16, b = pts + static_cast<uint32_t>(ee_rem(i + 1, n)) * 16;
        const float ax = lwc1(a), ay = lwc1(a + 4);
        const float side = fpu_sub(fpu_mul(fpu_sub(lwc1(b), ax), fpu_sub(py, ay)), fpu_mul(fpu_sub(lwc1(b + 4), ay), fpu_sub(px, ax)));
        if (0.0f < side) return i + 1;
    }
    return 0;
}

// orig: L4:0x0024c660
float segment_closest_l4(uint32_t out, uint32_t a, uint32_t b, uint32_t p) {
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t ab = sp, cross = sp + 0x10, pa = sp + 0x20, side = sp + 0x30;
    vec3_sub(V(ab), V(b), V(a));
    vec3_sub(V(pa), V(a), V(p));
    // the line through `p` across the segment (its z and w as the frame has them)
    swc1(side, lwc1(ab + 4));
    swc1(side + 4, -lwc1(ab));
    vec3_add(V(side), V(p), V(side));
    vec3_sub(V(cross), V(p), V(side));
    const float det = fpu_sub(fpu_mul(lwc1(ab + 4), lwc1(cross)), fpu_mul(lwc1(ab), lwc1(cross + 4)));
    float t = 0.0f;
    if (abs_f(det) < u2f(0x38d1b717u)) {  // (parallel: the segment's start)
        lq_sq(out, a);
    } else {
        const float inv = fpu_div(1.0f, det);
        t = fpu_mul(fpu_sub(fpu_mul(lwc1(cross + 4), lwc1(pa)), fpu_mul(lwc1(cross), lwc1(pa + 4))), inv);
        float s = t < 0.0f ? 0.0f : t;
        if (1.0f < s) s = 1.0f;
        vec3_scale(V(out), V(ab), s);
        vec3_add(V(out), V(out), V(a));
    }
    ee_stack_free(0xa0);
    return t;
}

// orig: L4:0x0024c7d8
int32_t polygon_closest_l4(uint32_t out, uint32_t p, uint32_t pts, int32_t n) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t near = sp, off = sp + 0x10;
    const int32_t outside = 0 < polygon_outside_edge_l4(p, pts, n) ? 1 : 0;
    float best = 0.0f;
    for (int32_t i = 0; i < n; ++i) {
        segment_closest_l4(near, pts + static_cast<uint32_t>(i) * 16, pts + static_cast<uint32_t>(ee_rem(i + 1, n)) * 16, p);
        vec3_sub(V(off), V(p), V(near));
        const float d = vec3_length(V(off));
        if (i == 0 || d < best) {
            best = d;
            lq_sq(out, near);
        }
    }
    ee_stack_free(0xc0);
    return outside;
}

// The flock's parameters (L4:0x166130, 8 words): +0 the pull along the member's own +0x20 (0 here), +4 the pull toward
// the leader and home (twice the members' count), +8 the pull into the area, +0xc the push off the floor and walls,
// +0x10 the members' radius, +0x14 1, +0x18 the steering's length and +0x1c the speed, both set per member each frame.
// A member's pvars: +0 the steering, +0x10 the velocity, +0x20 its own direction, +0x30 the wander (+0x40 its size,
// +0x44 its timer), +0x48 1 to fly (0: on the ground), +0x4c the neighbours' range, +0x50 its leader, +0x54 its home,
// +0x5c the path whose area holds it (-1: none), +0x5e the classes it keeps away from (+0x60, their pushes +0x70 and
// ranges +0x90).
// orig: L4:0x001f30e0
void flock_steer_l4(uint32_t members, int32_t count, uint32_t m, uint32_t pv, uint32_t params) {
    const uint32_t sp = ee_stack_alloc(0x130);
    const uint32_t edge = sp, pull = sp + 0x10, stale = sp + 0x20, from = sp + 0x30, to = sp + 0x40, normal = sp + 0x50;
    const uint32_t pos = m + 0x10;
    // (`stale`: a stack quadword the original reads without writing it, as the frame holds it)
    vec_clear(V(pv));
    const float reach = vec3_distance(V(pos), V(stale));
    bool outside = false;
    uint32_t hit = 0;  // a moby too near: the steering away from it alone
    const int32_t area = lh(pv + 0x5c);
    uint32_t path = 0;
    int32_t nearest = 0;
    auto add_pull = [&](float len) {
        vec3_set_length(V(pull), V(pull), len);
        vec3_add(V(pv), V(pv), V(pull));
    };
    // pulled with the cube of the distance (the factor k)
    auto add_cubed = [&](float k) {
        const float d = vec3_length(V(pull));
        add_pull(fpu_mul(d, fpu_mul(fpu_mul(d, d), k)));
    };
    if (area != -1) {
        // the area: the path's polygon; inside, pushed off its edge (inverse square); outside, pulled back in
        path = lvd::path(area);
        nearest = flock_nearest_point_l4(m, path + 0x10, lw(path));
        outside = polygon_closest_l4(edge, pos, path + 0x10, lw(path)) != 0;
        vec3_sub(V(pull), V(edge), V(pos));
        if (!outside) {
            const float d = vec3_length(V(pull));
            add_pull(d == 0.0f ? -1000.0f : fpu_div(-lwc1(params + 8), fpu_mul(d, d)));
        } else {
            const float d = fpu_add(vec3_length(V(pull)), 3.0f);
            add_pull(reach < d ? fpu_mul(d, fpu_mul(lwc1(params + 8), d)) : 10000.0f);
        }
    }
    if (lwc1(params) != 0.0f) {
        vec3_set_length(V(pull), V(pv + 0x20), lwc1(params));
        vec3_add(V(pv), V(pv), V(pull));
    }
    if (lwu(pv + 0x50) == m) {  // the leader itself: on along its velocity
        lq_sq(pull, pv + 0x10);
        add_pull(lwc1(params + 4));
    } else {
        if (area != -1) {
            // the path: the mean of the point nearest the stale one (of the nearest point and its two neighbours) and
            // its two neighbours; pulled toward it (from inside only)
            const int32_t n = lw(path);
            auto point = [&](int32_t i) { return path + 0x10 + static_cast<uint32_t>(i) * 16; };
            int32_t pick = nearest;
            float far = vec3_distance(V(point(pick)), V(stale));
            const int32_t next = nearest + 1 < n ? nearest + 1 : 0;
            float d = vec3_distance(V(point(next)), V(stale));
            if (far < d) {
                pick = next;
                far = d;
            }
            const int32_t prev = nearest - 1 >= 0 ? nearest - 1 : n - 1;
            d = vec3_distance(V(point(prev)), V(stale));
            if (far < d) pick = prev;
            const int32_t before = -1 < pick - 1 ? pick - 1 : n - 1;
            const int32_t after = pick + 1 < n ? pick + 1 : 0;
            vec3_add(V(pull), V(point(after)), V(point(before)));
            vec3_add(V(pull), V(pull), V(point(pick)));
            vec3_scale(V(pull), V(pull), u2f(0x3eaaaaabu));
            if (!outside) {
                vec3_sub(V(pull), V(pull), V(pos));
                add_cubed(lwc1(params + 8));
            }
        }
        // the leader and home, when nearer than the stale point
        if (lwu(pv + 0x50) != 0) {
            vec3_sub(V(pull), V(lwu(pv + 0x50) + 0x10), V(pos));
            if (vec3_length(V(pull)) < reach) add_cubed(lwc1(params + 4));
        }
        if (const uint32_t home = lwu(pv + 0x54); home != 0) {
            swc1(pull, fpu_sub(lwc1(home), lwc1(pos)));  // (x, y and z only: the w lane as it was)
            swc1(pull + 4, fpu_sub(lwc1(home + 4), lwc1(pos + 4)));
            swc1(pull + 8, fpu_sub(lwc1(home + 8), lwc1(pos + 8)));
            if (vec3_length(V(pull)) < reach) add_cubed(lwc1(params + 4));
        }
        // away from the other members and the hero (the last): those of the classes listed within range; one nearer
        // than the members' radius ends the search (the steering then away from it alone, below)
        for (int32_t i = 0; i <= count; ++i) {
            const uint32_t o = i < count ? lwu(members + static_cast<uint32_t>(i) * 4) : lwu(0x001413d0);
            if (static_cast<int8_t>(lbu(o + 0x20)) < 0 || o == m) continue;
            const float d = vec3_distance(V(pos), V(o + 0x10));
            if (lwc1(pv + 0x4c) < d) continue;
            const int32_t kinds = static_cast<int16_t>(lhu(pv + 0x5e));
            int32_t k = 0;
            while (lh(pv + 0x60 + static_cast<uint32_t>(k) * 2) != lh(o + 0xa6) && ++k < kinds) {
            }
            if (!(k < kinds)) continue;
            if (d < lwc1(params + 0x10)) {
                hit = o;
                break;
            }
            float push = fpu_div(d, lwc1(pv + 0x90 + static_cast<uint32_t>(k) * 4));
            push = push == 0.0f ? 1000.0f : fpu_div(lwc1(pv + 0x70 + static_cast<uint32_t>(k) * 4), fpu_mul(push, push));
            vec3_sub(V(pull), V(pos), V(o + 0x10));
            add_pull(push);
        }
    }
    if (lwc1(pv + 0x48) == 0.0f) {
        // on the ground: off a wall ahead (the line from 1.5 above to the next place), up a slope steeper than 0.4
        vec3_add(V(from), V(pos), V(pv + 0x10));
        vec3_add(V(to), V(pos), V(pv + 0x10));
        swc1(to + 8, u2f(0x3c23d70au));
        swc1(from + 8, fpu_add(lwc1(from + 8), 1.5f));
        if (segment_collide(from, to, 2, m, 0) != 0 && 0 < static_cast<int32_t>(coll_hit_flags()) && lwc1(m + 0x18) < coll_hit_point().z) {
            V(normal) = coll_hit_normal();
            const float slope = atan2_f(lwc1(normal + 8), vec2_length(V(normal)));
            vec2_set_length(V(normal), V(normal), 1.0f);
            vec3_scale(V(normal), V(normal), lwc1(params + 0xc));
            vec3_add(V(pv), V(pv), V(normal));
            if (u2f(0x3ecccccdu) < slope) {
                vec3_scale(V(normal), V(normal), fpu_mul(lwc1(params + 0xc), fpu_mul(slope, 10000.0f)));
                vec3_add(V(pv), V(pv), V(normal));
            }
            if (u2f(0x3e4ccccdu) < fpu_sub(lwc1(m + 0x18), coll_hit_point().z)) {
                vec2_set_length(V(normal), V(pv + 0x10), -fpu_mul(lwc1(params + 0xc), 1000.0f));
                vec3_add(V(pv), V(pv), V(normal));
            }
        }
    } else if (sphere_collide(pos, lwc1(params + 0x10), 4, m) != 0) {  // flying: pushed out of what it meets, and up
        vec3_sub(V(pull), coll_hit_pushed(), V(pos));
        swc1(pull + 8, fpu_add(lwc1(pull + 8), lwc1(params + 0xc)));
        add_pull(lwc1(params + 0xc));
    }
    // the steering's length (flat on the ground; its z over the flight factor in the air)
    if (lwc1(pv + 0x48) == 0.0f) {
        vec2_set_length(V(pv), V(pv), lwc1(params + 0x18));
        swc1(pv + 8, 0.0f);
    } else {
        vec3_set_length(V(pv), V(pv), lwc1(params + 0x18));
        swc1(pv + 8, fpu_div(lwc1(pv + 8), lwc1(pv + 0x48)));
    }
    // the wander, picked again every 45 .. 90 frames
    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x44))) != 0) {
        sh(pv + 0x44, static_cast<uint32_t>(float_to_int(rand_range_f(45.0f, 90.0f))));
        swc1(pv + 0x30, fpu_sub(rand_range_f(0.0f, lwc1(pv + 0x40)), fpu_mul(lwc1(pv + 0x40), 0.5f)));
        swc1(pv + 0x34, fpu_sub(rand_range_f(0.0f, lwc1(pv + 0x40)), fpu_mul(lwc1(pv + 0x40), 0.5f)));
        if (lwc1(pv + 0x48) == 0.0f) swc1(pv + 0x38, 0.0f);
        else swc1(pv + 0x38, fpu_sub(rand_range_f(0.0f, lwc1(pv + 0x40)), fpu_mul(lwc1(pv + 0x40), 0.5f)));
    }
    vec3_add(V(pv), V(pv), V(pv + 0x30));
    if (hit != 0) {
        vec3_sub(V(pull), V(pos), V(hit + 0x10));
        if (lwc1(pv + 0x48) == 0.0f) vec2_set_length(V(pv), V(pull), lwc1(params + 0x18));
        else vec3_set_length(V(pv), V(pull), lwc1(params + 0x18));
    }
    ee_stack_free(0x130);
}

// orig: L4:0x001f3a80
void flock_member_move_l4(uint32_t m, uint32_t pv, uint32_t params, float lift) {
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t at = sp;
    vec3_add(V(pv + 0x10), V(pv + 0x10), V(pv));
    vec3_set_length(V(pv + 0x10), V(pv + 0x10), lwc1(params + 0x1c));
    swc1(pv + 0x18, fpu_div(lwc1(pv + 0x18), lwc1(pv + 0x48)));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x10));
    for (int32_t i = 0; i < 6; ++i) {
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), lift));
        if (sphere_collide(at, fpu_mul(lwc1(params + 0x10), 0.25f), 4, m) == 0) break;
        V(m + 0x10) = coll_hit_pushed();
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lift));
    }
    swc1(m + 0x48, angle_add(atan2_f(lwc1(pv + 0x10), lwc1(pv + 0x14)), u2f(0x40490fdbu)));
    ee_stack_free(0x90);
}

// orig: L4:0x0029eb20
int32_t path_advance_l4(uint32_t m, uint32_t path, int32_t idx, int32_t dir, float ahead) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t leg = sp, rel = sp + 0x10;
    const int32_t n = lw(path);
    auto point = [&](int32_t i) { return path + 0x10 + static_cast<uint32_t>(i) * 16; };
    // the point after `i` in the direction `dir` (turning back at the ends)
    auto next = [&](int32_t i) {
        if (i == n - 1) return 0 < dir ? 0 : n - 2;
        if (i != 0) return i + dir;
        return dir < 1 ? n - 1 : 1;
    };
    int32_t to = next(idx);
    vec3_sub(V(leg), V(point(to)), V(point(idx)));
    const float length = vec3_length(V(leg));
    vec3_set_length(V(leg), V(leg), 1.0f);
    vec3_sub(V(rel), V(m + 0x10), V(point(idx)));
    float along = fpu_add(vec3_dot(V(rel), V(leg)), ahead);
    if (length < along) {  // past the leg's end: on the next leg
        idx = to;
        to = next(idx);
        vec3_sub(V(leg), V(point(to)), V(point(idx)));
        along = fpu_sub(along, length);
        vec3_set_length(V(leg), V(leg), 1.0f);
    }
    vec3_set_length(V(leg), V(leg), along);
    vec3_add(V(m + 0x10), V(leg), V(point(idx)));
    ee_stack_free(0xd0);
    return idx;
}

// Class 86, a flock: on its first frame (+0x94 0) its place is its home (+0); then it spawns its +0x90 members (class
// 85, their slots +0x10) about it; each frame after, it circles its home at the radius +0x9c (+0xa0 -1; the angle +0x98)
// or moves along the path +0xa0 (its point +0xa4) ahead of its nearest member, and steers its members at a speed of its
// radius. The level's first flock sets the flocks' parameters (L4:0x166130, from the count +0xa8 of class 85 mobys).
// orig: L4:0x0029ecf8
void eud_class86_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t params = L4_ADDR(0x00166130);
    const float per_member = u2f(0x4004ac08u);  // 2.073
    if (lw(L4_ADDR(0x001613c0)) == 0) {
        libc_memset(sp, 0, 0x20);
        swc1(sp + 4, fpu_mul(int_to_float(lw(pv + 0xa8)), per_member));
        swc1(sp + 0xc, 2.0f);
        swc1(sp + 0x10, 1.0f);
        swc1(sp + 0x14, 1.0f);
        sw(L4_ADDR(0x001613c0), 1);
        for (uint32_t i = 0; i < 0x20; i += 4) sw(L4_ADDR(0x00166130) + i, lwu(sp + i));
    }
    if (lw(pv + 0x94) == 0) {
        lq_sq(pv, m + 0x10);
        sw(pv + 0x98, 0);
    } else if (lw(pv + 0xa8) == 0) {
        // the members on the level (the live mobys of class 85)
        for (uint32_t o = lwu(L4_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28))
            if (static_cast<int8_t>(lbu(o + 0x20)) >= 0 && lh(o + 0xa6) == 0x55) sw(pv + 0xa8, lwu(pv + 0xa8) + 1);
        swc1(L4_ADDR(0x00166134), fpu_mul(int_to_float(lw(pv + 0xa8)), per_member));
    }
    if (lw(pv + 0xa0) == -1) {  // circling its home
        const float a = angle_add(lwc1(pv + 0x98), fpu_mul(turn_dt(), u2f(0x3b64c389u)));
        swc1(pv + 0x98, a);
        swc1(m + 0x10, fpu_add(lwc1(pv), fpu_mul(lwc1(pv + 0x9c), cos_vu(a))));
        swc1(m + 0x14, fpu_add(lwc1(pv + 4), fpu_mul(lwc1(pv + 0x9c), sin_vu(lwc1(pv + 0x98)))));
    } else if (lw(pv + 0x94) != 0) {  // along its path, ahead of its nearest member by what it lacks of 3
        float nearest = 10000.0f;
        for (int32_t i = 0; i < lw(pv + 0x90); ++i) {
            const float d = vec2_distance(V(m + 0x10), V(lwu(pv + 0x10 + static_cast<uint32_t>(i) * 4) + 0x10));
            if (d < nearest) nearest = d;
        }
        const float ahead = nearest < 3.0f ? fpu_sub(3.0f, nearest) : 0.0f;
        sw(pv + 0xa4, static_cast<uint32_t>(path_advance_l4(m, lvd::path(lw(pv + 0xa0)), lw(pv + 0xa4), 1, ahead)));
    }
    if (lw(pv + 0x94) == 0) {
        // the flock spawned about its place
        sw(pv + 0x94, 1);
        sw(pv + 0xa8, 0);
        for (int32_t i = 0; i < lw(pv + 0x90); ++i) {
            const uint32_t slot = pv + 0x10 + static_cast<uint32_t>(i) * 4;
            sw(slot, 0);
            const uint32_t b = moby_spawn(0x55);
            if (b == 0) continue;
            const uint32_t bp = lwu(b + 0x78);
            sw(slot, b);
            sb(b + 0x30, 0x7f);
            sh(b + 0x32, 0x7f);
            swc1(b + 0x2c, fpu_mul(lwc1(b + 0x2c), rand_range_f(u2f(0x3f333333u), u2f(0x3fa66666u))));
            swc1(b + 0x48, rand_angle());
            swc1(b + 0x58, rand_range_f(u2f(0x3f19999au), u2f(0x3f99999au)));
            vec_clear(V(bp + 0x10));
            sh(bp + 0x44, 0);
            swc1(bp + 0x4c, 10.0f);
            lq_sq(b + 0x10, m + 0x10);
            const float yaw = rand_angle(), pitch = rand_angle();
            const float r = rand_range_f(u2f(0x3f19999au), u2f(0x3f666666u));
            swc1(b + 0x10, fpu_add(lwc1(b + 0x10), fpu_mul(fpu_mul(cos_vu(yaw), sin_vu(pitch)), r)));
            swc1(b + 0x14, fpu_add(lwc1(b + 0x14), fpu_mul(fpu_mul(sin_vu(yaw), sin_vu(pitch)), r)));
            swc1(b + 0x18, fpu_add(lwc1(b + 0x18), fpu_mul(cos_vu(pitch), r)));
            sw(bp + 0x50, 0);
            sw(bp + 0x54, m + 0x10);
            sw(bp + 0x58, 0);
            swc1(bp + 0x70, u2f(0x3fd8d4feu));  // (the push from its fellows (class 85) and from the hero (class 494))
            swc1(bp + 0x74, u2f(0x3fd8d4feu));
            swc1(bp + 0x94, 5.0f);
            swc1(bp + 0x48, u2f(0x3f872b02u));
            sh(bp + 0x5c, 0xffff);
            swc1(bp + 0x40, u2f(0x3b449ba6u));
            sh(bp + 0x5e, 2);
            sh(bp + 0x60, 0x55);
            swc1(bp + 0x90, 1.0f);
            sh(bp + 0x62, 0x1ee);
            moby_update_transform(b);
        }
    } else {
        // the members steered: the steering's length and the speed from the flock's radius, the frame and their size
        const float k = u2f(0x40f81893u);  // 7.753
        for (int32_t i = 0; i < lw(pv + 0x90); ++i) {
            const uint32_t b = lwu(pv + 0x10 + static_cast<uint32_t>(i) * 4);
            const uint32_t bp = lwu(b + 0x78);
            const float base = fpu_mul(fpu_div(fpu_mul(lwc1(pv + 0x9c), k), 1800.0f), turn_dt());
            const float size = fpu_mul(fpu_add(lwc1(b + 0x58), 1.0f), 0.5f);
            swc1(params + 0x18, fpu_mul(fpu_mul(fpu_mul(fpu_mul(base, size), u2f(0x3d4ccccdu)), lwc1(0x0015ed64)), size));
            swc1(params + 0x1c, fpu_mul(base, size));
            flock_steer_l4(pv + 0x10, lw(pv + 0x90), b, bp, params);
            flock_member_move_l4(b, bp, params, 0.0f);
        }
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
