// The debris pieces' class update: the update function of the classes whose mobys
// debris_spawn (0x263fd8) breaks off a moby: on Veldin the robot 1440's pieces 0x78d..0x78f and
// 0x7ab, on Novalis the Peckbot's 0x6d3..0x6d5 (Novalis's copy L1:0x30cd18, the same code), and
// the other levels' copies of it. Proved against the original.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/debris_piece.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// A debris piece (pvars from debris_spawn: velocity +0, spin +0x10, size +0x20 (w its radius),
// frames +0x30, flags +0x34, gravity +0x38), by its state +0x20:
//  0 retired;
//  1 flying for its frames (+0x30); then 10 frames more (+0x30) in state 3, or in state 2 with
//    flags bit 0;
//  2 blown up where its size centre is: an enemy death burst (0x25f3e8, half its radius) with
//    flags bit 1, else the generic blast 0x25e450 (5 debris sprays, 3 sparks, 5 smoke puffs,
//    shards of its radius and half of it, at half its scale); then retired;
//  3 fading out: its alpha (+0x23) 128 * frames left / 10 frames; retired when they run out;
//  others: flying as in 1.
// Flying: turned by its spin about its size centre (the centre stays where it was), moved by its
// velocity, the velocity's z less its gravity; when the sphere of its radius at the centre hits
// the world (0x1f0d60) moving into the surface, the velocity is reflected off the contact normal
// (0x173e80), half its normal part added back, its speed kept, and the spin becomes the turn
// about (velocity x normal) by the spin's length.
// orig: 0x002e3d88
void debris_piece_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t st = lbu(m + 0x20);
    int32_t& frames = *static_cast<int32_t*>(ee_ram_ptr(pv + 0x30));
    Vec4 sp20;
    if (st == 1) {
        if (timer_tick_i32(frames) != 0) {
            sb(m + 0x20, (lw(pv + 0x34) & 1) != 0 ? 3u : 2u);
            sw(pv + 0x30, static_cast<uint32_t>(float_to_int(frame_scale_f(10.0f))));
            return;
        }
    } else if (st == 0) {
        moby_retire(m);
        return;
    } else if (st == 2) {
        mat3_transform(sp20, V(pv + 0x20), M(m + 0xc0));
        vec3_add(sp20, sp20, V(m + 0x10));
        const uint32_t at = ee_stack_alloc(0x10);  // (the original's sp + 0x20)
        V(at) = sp20;
        if ((lw(pv + 0x34) & 2) != 0) {
            enemy_death_burst(m, at, lwc1(pv + 0x2c) * 0.5f, 0.0f);  // ($a2 = -1 is passed too, unused)
        } else {
            const float r = lwc1(pv + 0x2c);
            const float half = r * 0.5f;
            explosion_effect(m, OV(0x0015f580), at, 5, 3, 5, -1, 0, 0, -1, 0, 0.0f, 0.0f, r, half, 9.0f, half, 0.0f);
        }
        ee_stack_free(0x10);
        moby_retire(m);
        return;
    } else if (st == 3) {
        if (timer_tick_i32(frames) != 0) {
            moby_retire(m);
            return;
        }
        const float ten = frame_scale_f(10.0f);
        const float a = fpu_mul(fpu_div(int_to_float(lw(pv + 0x30)), ten), 128.0f);  // (cvt.s.w, div.s, mul.s)
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(a)));
    }
    // flying
    Mat4 turn;
    euler_to_mat3_vu(turn, V(pv + 0x10));
    mat3_transform(sp20, V(pv + 0x20), M(m + 0xc0));
    mat3_mul(M(m + 0xc0), turn, M(m + 0xc0));
    Vec4 sp60, sp70, sp80;
    mat3_transform(sp60, V(pv + 0x20), M(m + 0xc0));
    vec3_sub(sp70, sp60, sp20);
    vec3_sub(V(m + 0x10), V(m + 0x10), sp70);
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    const float radius = lwc1(pv + 0x2c);
    swc1(pv + 8, lwc1(pv + 8) - lwc1(pv + 0x38));
    mat3_transform(sp80, V(pv + 0x20), M(m + 0xc0));
    vec3_add(sp80, sp80, V(m + 0x10));
    if (sphere_collide_v(sp80, radius, 0, m) == 0) return;
    Vec4 n = coll_hit_normal();
    vec3_set_length(n, n, 1.0f);
    coll_hit_set_normal(n);
    const float speed = vec3_length(V(pv));
    const float into = vec3_dot(V(pv), n);
    if (!(into < 0.0f)) return;
    vec3_reflect(V(pv), V(pv), n);
    Vec4 spd0;
    vec3_set_length(spd0, n, into * 0.5f);
    vec3_add(V(pv), V(pv), spd0);
    vec3_set_length(V(pv), V(pv), speed);
    Vec4 axis;
    vec3_cross_ba(axis, n, V(pv));
    vec3_set_length(axis, axis, 1.0f);
    quat_axis_angle(axis, axis, vec3_length(V(pv + 0x10)));
    Mat4 sp90;
    quat_to_mat4(axis, sp90);
    mat_to_euler_r(sp90, V(pv + 0x10));
}

}  // namespace rac1
