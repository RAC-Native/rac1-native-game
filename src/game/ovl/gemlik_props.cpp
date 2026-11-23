// Gemlik Base's (level 13) classes 352, 389, 404 and 1403: a glow that grows over its maker, a piece of a fighter
// flying off, a field that keeps mobys out of its box until the moby it guards is destroyed, and a mark that follows
// a moby and then a path. Level 13 only: the addresses
// are Gemlik's (`orig: L13:`), the level's own data L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// Class 352: pvars +0 its maker, +4 its strength (0 .. 1: its alpha 60 times it), +8 its texture's scroll. States (the
// table L13:0x1f4e90): 0 set up, 1 grows to 4 times its class's scale while it fades in (2 % of a second a frame), 2
// holds while its maker lives, 3 shrinks back while it fades out, 4 gone. Its texture scrolls 0x40 a frame (but in 4).
// Class 389: pvars +0x10 its velocity, +0x20 a quadword cleared, +0x30 its spin, +0x3c the moby it belongs to (class
// 0x184, a fighter of Qwark's fleet: gone, it goes), +0x40 its time. States (the table L13:0x1f4f90): 0 set up, 1 its
// scale and draw words taken from its owner, 2 nothing, 3 flies (bounces off what it meets but the fleet's own mobys:
// then explodes), 4 explodes. It goes out of the space's bounds (10 .. 1013), or its time out out of sight.
// Class 404: pvars +0 the moby it guards (an index, -1 none), +4 its strength, +8 / +0xa its box's half width and
// height (16 bits; the box turned with the moby), +0xc its texture's scroll, +0xe a region it clears when it goes (-1
// none). States: 0 set up, 1 stands (pushes mobys out of its box) until the guarded moby is gone or its save bit (the
// level's +0xb2 bit) set, 2 fades (5 % of a second a frame, its alpha 20 times its strength), 3 gone. Its texture
// scrolls 0xc0 a frame (but in 3).
// Class 1403: pvars +0 the moby it follows (an index, -1 none: it stays), +4 a path (lvd::path, -1 none), +8 / +0xc the
// path's point it is at and its distance to it, +0x10 the frames since its leader stopped. State 0 sets up (the
// path's points' w lanes given each segment's length, the original writing the level's data); every frame it is
// retired with its leader gone; while the leader's state is below 7 it is at the leader's place; then, on the path:
// state 1 the nearest point of the path to the leader (100 at most) while that goes on along the path or for 30
// frames, then state 2: it walks the path to its end (a quarter a frame), state 3 there.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_path_ahead.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/weapons.h>

namespace rac1 {

using namespace gml;

namespace {
// the level's moby of index `idx` (not -1)
uint32_t moby_at(int32_t idx) { return lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
}  // namespace

// orig: L13:0x002e84d8
void gml_class352_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    // its scale eased 10 % a frame toward `target`; its class's scale
    auto ease_scale = [m](float target) {
        const float s = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(s, fpu_mul(fpu_sub(target, s), 0.1f)));
    };
    auto class_scale = [m] { return lwc1(lwu(m + 0x24) + 0x24); };
    auto alpha = [m, pv] { sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(lwc1(pv + 4), 60.0f)))); };
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4e90)
    case 0:  // (0x002e8518)
        sb(m + 0x20, 1);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    case 1: {  // (0x002e8530): grows, fades in
        ease_scale(fpu_mul(class_scale(), 4.0f));
        const float a = fpu_add(lwc1(pv + 4), fpu_mul(step60(), u2f(0x3ca3d70au)));  // (0.02)
        swc1(pv + 4, a);
        if (1.0f <= a) {
            swc1(pv + 4, 1.0f);
            sb(m + 0x20, 2);
        }
        alpha();
        break;
    }
    case 2: {  // (0x002e85a8): holds while its maker lives
        ease_scale(fpu_mul(class_scale(), 4.0f));
        const uint32_t maker = lwu(pv);
        if (maker == 0 || !alive(maker)) sb(m + 0x20, 3);
        break;
    }
    case 3: {  // (0x002e860c): shrinks, fades out
        ease_scale(class_scale());
        const float a = fpu_sub(lwc1(pv + 4), fpu_mul(step60(), u2f(0x3ca3d70au)));
        swc1(pv + 4, a);
        if (a <= 0.0f) {
            swc1(pv + 4, 0.0f);
            sb(m + 0x20, 4);
        }
        alpha();
        break;
    }
    case 4:  // (0x002e8690)
        moby_retire(m);
        return;
    default:
        break;
    }
    // its texture's scroll (by 0x40, kept within 0 .. 0x1000)
    const int32_t at = lw(pv + 8);
    int32_t step = -0x40, next = at - 0x40;
    if (!(next < 4097)) {
        next = at - 0x1040;
        step = -0x1040;
    } else if (next < 0) {
        next = at + 0xfc0;
        step = 0xfc0;
    }
    sw(pv + 8, static_cast<uint32_t>(next));
    class_uv_scroll(lwu(m + 0x24), 0, step);
}

// orig: L13:0x002ece00
void gml_class389_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t owner = lwu(pv + 0x3c);
    if (owner == 0 || lh(owner + 0xa6) != 0x184 || !alive(owner)) {
        moby_retire(m);
        return;
    }
    sh(m + 0x34, (lhu(m + 0x34) & 0xfffcu) | (lhu(lwu(pv + 0x3c) + 0x34) & 3u));  // (drawn / hidden as its owner)
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4f90)
    case 0:  // (0x002ece94)
        sb(m + 0x20, 1);
        sq_zero(pv + 0x20);
        return;
    case 1: {  // (0x002eceac): its owner's scale in its own class's terms, its draw words
        const uint32_t o = lwu(pv + 0x3c);
        swc1(m + 0x2c, fpu_div(fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(o + 0x2c)), lwc1(lwu(o + 0x24) + 0x24)));
        sd(m + 0x38, ld(lwu(pv + 0x3c) + 0x38));
        return;
    }
    case 3: {  // (0x002ecee0): flies, slowing, spinning
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x10));
        vec3_scale(V(pv + 0x10), V(pv + 0x10), u2f(0x3f7eb852u));  // (0.995)
        if (sphere_collide(m + 0x10, 2.0f, 0, m) != 0) {
            const uint32_t hit = coll_hit_moby();
            bool own = false;  // (Qwark's fleet: its ships and their parts)
            if (hit != 0) {
                switch (lh(hit + 0xa6)) {
                case 0x184: case 0x185: case 0x186: case 0x187: case 0x188: case 0x189: case 0x18a: case 0x18b: case 0x18c:
                case 0x18f: case 0x190: case 0x191:
                    own = true;
                    break;
                default:
                    break;
                }
            }
            if (!own) {  // it bounces, and explodes
                vec3_reflect(V(pv + 0x10), V(pv + 0x10), coll_hit_normal());
                sb(m + 0x20, 4);
            }
        }
        for (uint32_t k = 0; k < 0xc; k += 4) swc1(m + 0x40 + k, angle_add(lwc1(m + 0x40 + k), lwc1(pv + 0x30 + k)));
        for (uint32_t k = 0; k < 0xc; k += 4) swc1(pv + 0x30 + k, fpu_mul(lwc1(pv + 0x30 + k), u2f(0x3f7fbe77u)));  // (0.999)
        for (uint32_t k = 0x10; k < 0x1c; k += 4) {
            if (lwc1(m + k) < 10.0f || 1013.0f < lwc1(m + k)) {
                moby_retire(m);
                return;
            }
        }
        if (tick32(pv + 0x40) != 0 && lbu(m + 0x31) == 0) moby_retire(m);
        return;
    }
    case 4:  // (0x002ed0cc)
        explosion_effect(m, pv + 0x10, 0, 0xa, 3, 0x10, -1, 1, 1, -1, 0, 2.0f, 4.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        moby_retire(m);
        return;
    default:  // (2 and beyond 4: 0x002ed13c)
        return;
    }
}

// Class 404's push: the mobys within its box's sphere (mobys_in_sphere, flags 0x10) that stand inside its box (turned
// with it: within its half width + 0.7 across, its half height + 1.5 up) pushed out across the face they are nearest
// (push_without_direction, 0x10001) from the point of the face below them.
// orig: L13:0x002ed158
void gml_2ed158_l13(uint32_t m, uint32_t pv) {
    const Frame f(416);
    const uint32_t sp = f.sp;
    const uint32_t rot = sp, centre = sp + 0x30, rel = sp + 0x40, local = sp + 0x50, inv = sp + 0x60, at = sp + 0xa0;
    euler_to_mat3_vu(M(rot), V(m + 0x40));
    const int32_t half_w = lh(pv + 8), half_h = lh(pv + 0xa);
    const float r = fpu_mul(static_cast<float>(half_h < half_w ? half_w : half_h), 0.5f);
    sq_zero(centre);
    swc1(centre + 8, r);
    mat3_transform(V(centre), V(centre), M(rot));
    vec3_add(V(centre), V(centre), V(m + 0x10));
    const int32_t n = mobys_in_sphere(centre, r, 0x10, m, 0);
    if (n == 0) return;
    // (the original's copy of the list, below its frame)
    const uint32_t list = ee_stack_ptr() -= ((static_cast<uint32_t>(n) << 2) + 15u) & ~15u;
    libc_memcpy(list, coll_hit_list(), static_cast<uint32_t>(n) << 2);
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t o = lwu(list + (static_cast<uint32_t>(i) << 2));
        if (o == 0 || !alive(o)) continue;
        vec3_sub(V(rel), V(o + 0x10), V(m + 0x10));
        mat3_transpose(M(inv), M(rot));
        mat4_transform(V(local), V(rel), M(inv));
        lq_sq(at, local);
        {
            const float x = lwc1(at), lim = fpu_add(fpu_mul(static_cast<float>(lh(pv + 8)), 0.5f), u2f(0x3f333333u));  // (0.7)
            if (x < -lim) swc1(at, fpu_mul(static_cast<float>(-lh(pv + 8)), 0.5f));
            else if (lim < x) swc1(at, fpu_mul(static_cast<float>(lh(pv + 8)), 0.5f));
        }
        {
            const float z = lwc1(at + 8), lim = fpu_add(fpu_mul(static_cast<float>(lh(pv + 0xa)), 0.5f), 1.5f);
            if (z < -lim) swc1(at + 8, fpu_mul(static_cast<float>(-lh(pv + 0xa)), 0.5f));
            else if (lim < z) swc1(at + 8, fpu_mul(static_cast<float>(lh(pv + 0xa)), 0.5f));
        }
        sw(at + 4, 0);
        mat3_transform(V(at), V(at), M(rot));
        vec3_add(V(at), V(at), V(m + 0x10));
        const int32_t k = mobys_in_sphere(at, u2f(0x3f333333u), 0x10, m, 0);
        for (int32_t j = 0; j < k; ++j)
            if (coll_hit_list_at(j) == o) push_without_direction(o, m, 0x10001, at, 1.0f);
    }
    ee_stack_ptr() = sp;  // (the list given back; the frame freed on return)
}

// orig: L13:0x002ed4a8
void gml_class404_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // set up: drawn faint, its colour
        sb(m + 0x20, 1);
        sb(m + 0x23, 0x14);
        sh(m + 0x34, lhu(m + 0x34) | 0xa08u);
        sh(pv + 0xe, 0xffff);
        swc1(pv + 4, 1.0f);
        moby_set_color_hi(m, 0x80, 0x80, 0x80);
        break;
    case 1: {  // stands; fades once the guarded moby is gone or destroyed (its save bit)
        const int32_t idx = lw(pv);
        if (idx == -1) {
            swc1(pv + 4, 1.0f);
            sb(m + 0x20, 2);
            debug_printf_stub();  // (the format L13:0x1f4fb0 with its class and uid: the release build prints nothing)
        } else {
            const uint32_t g = moby_at(idx);
            bool gone = g == 0 || !alive(g);
            if (!gone) {
                const uint32_t uid = lhu(g + 0xb2);
                const uint32_t word = 0x0014c190u + (lwu(0x0015ed84) << 8) + (static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(uid)) >> 5) << 2);
                gone = ((lw(word) >> (uid & 0x1f)) & 1) != 0;
            }
            if (gone) {
                swc1(pv + 4, 1.0f);
                sb(m + 0x20, 2);
            }
        }
        gml_2ed158_l13(m, pv);
        break;
    }
    case 2: {  // fades
        const float a = fpu_sub(lwc1(pv + 4), fpu_mul(step60(), 0.05f));
        swc1(pv + 4, a);
        if (a < 0.0f) sb(m + 0x20, 3);
        else sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(a, 20.0f))));
        break;
    }
    case 3: {  // gone (its region cleared)
        const int32_t region = lh(pv + 0xe);
        if (region != -1) {
            region_clear(static_cast<uint32_t>(region));
            sh(pv + 0xe, 0xffff);
        }
        moby_retire(m);
        return;
    }
    default:
        break;
    }
    // its texture's scroll (by 0xc0, kept within 0 .. 0x1000)
    const uint32_t next = lhu(pv + 0xc) + 0xc0;
    int32_t step = 0xc0;
    sh(pv + 0xc, next);
    if (!(static_cast<int16_t>(next) < 4097)) {
        sh(pv + 0xc, next - 0x1000);
        step = -0xf40;
    } else if (static_cast<int16_t>(next) < 0) {
        sh(pv + 0xc, next + 0x1000);
        step = 0x10c0;
    }
    class_uv_scroll(lwu(m + 0x24), step, 0);
}

// orig: L13:0x0030bb70
void gml_class1403_update_l13(uint32_t m) {
    const Frame f(128);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {  // set up: the path's segments' lengths into its points' w lanes
        sb(m + 0x30, 0xff);
        if (const int32_t pi = lw(pv + 4); pi != -1) {
            const uint32_t path = lvd::path(pi);
            for (int32_t i = 0; i < lw(path); ++i) {
                const uint32_t p = path + 0x10 + (static_cast<uint32_t>(i) << 4);
                const int32_t next = ee_rem(i + 1, lw(path));
                swc1(p + 0xc, vec3_distance(V(p), V(path + 0x10 + (static_cast<uint32_t>(next) << 4))));
            }
        }
        sw(pv + 0x8, 0);
        sw(pv + 0xc, 0);
        sb(m + 0x20, 1);
        sw(pv + 0x10, 0);
    }
    const int32_t idx = lw(pv);
    if (idx == -1) return;
    const uint32_t leader = lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
    if (leader == 0 || !alive(leader)) {
        moby_retire(m);
        return;
    }
    if (lbu(leader + 0x20) < 7) {
        lq_sq(m + 0x10, leader + 0x10);
        return;
    }
    sw(pv + 0x10, lwu(pv + 0x10) + 1);
    const int32_t pi = lw(pv + 4);
    if (pi == -1) return;
    const uint32_t path = lvd::path(pi);
    if (lbu(m + 0x20) == 1) {  // the path's point nearest the leader, while that goes on (or for 30 frames)
        int32_t seg = lw(sp + 0x10);
        float t = lwc1(sp + 0x14);
        path_closest_point_v(path, V(leader + 0x10), V(sp), &seg, &t, 0, 100.0f, 1.0f, 0.0f);
        sw(sp + 0x10, static_cast<uint32_t>(seg));
        swc1(sp + 0x14, t);
        if (lw(pv + 8) < seg || lwc1(pv + 0xc) < t || lw(pv + 0x10) < 30) {
            sw(pv + 8, static_cast<uint32_t>(seg));
            swc1(pv + 0xc, t);
            lq_sq(m + 0x10, sp);
        } else {
            sb(m + 0x20, 2);
        }
    }
    if (lbu(m + 0x20) == 2) {  // along the path to its end
        const int32_t at = lw(pv + 8);
        if (at < lw(path) - 1) {
            const int32_t next = path_point_ahead(m, path, m + 0x10, at, 1, fpu_mul(step60(), 0.25f));
            sw(pv + 8, static_cast<uint32_t>(next));
            swc1(pv + 0xc, vec3_distance(V(m + 0x10), V(path + 0x10 + (static_cast<uint32_t>(next) << 4))));
        } else {
            sb(m + 0x20, 3);
        }
    }
}

}  // namespace rac1
