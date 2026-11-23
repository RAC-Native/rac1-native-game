// Novalis's (level 1) variants of the hero's movement functions: Veldin's code minus
// the features Novalis's build does not have (the +0x20a4 movement modes 1 / 2, the +0x12e2 flag,
// states 0x2d / 0x3f / 0x4c, the lava surface, sub-state 0xf). Addresses are Novalis's.
#include <game/collision/collision.h>
#include <game/enhance/advanced.h>
#include <game/ovl/hero_l1.h>
#include <game/lib/libvu0.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Pick the hero's gait targets (PLAYER+0x228/0x22c/0x230) from the movement mode +0x20a4 (0 on foot,
// 3; Novalis's build has no modes 1 / 2) and the current state, then ease the live values
// PLAYER+0x220/0x224 towards them and spring the height PLAYER+0x234 (Veldin's 0x211380).
// Aridia's build (L2:0x002203e8) is the same code without the sub-state 0x11 / 0x12 branch (+0x22c / +0x228 cleared).
// orig: L1:0x00231ae0 L2:0x002203e8
// equipped boots/mode PLAYER+0x20a4 and the current state, then ease the live values
// PLAYER+0x220/0x224 towards them and spring the height PLAYER+0x234 (unless the hero is
// standing on a moving platform far from its anchor).
void hero_gait_update_l1() {
    const uint32_t P = g::PLAYER;
    switch (lbu(P + 0x20a4)) {
    case 0:
        swc1(P + 0x228, u2f(0x3f4ccccdu)); swc1(P + 0x230, u2f(0x3ee66666u)); swc1(P + 0x22c, u2f(0x3f333333u));
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
        } else if (!on_aridia() && static_cast<uint32_t>(sub - 0x11) < 2u) {
            sw(P + 0x22c, 0);
            sw(P + 0x228, 0);
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

// Stick move (hero_stick_move mode 0, scale 1) with the speed PLAYER+0x190 mapped per state (Veldin's
// 0x211be8 without state 0x3f): the walk/run speed of the table 0x17bdb8 (+8 below +0xc, else +0x18)
// times vm, and in state 0x73 max(0.8 x, 2.5 vm).
// Aridia's build (L2:0x00220b80) keeps state 0x3f (the beam) and has no state 0x73 clause.
// orig: L1:0x00232290 L2:0x00220b80
// mapped per state: state 0x3f: max(3.5 vm * stick, 2 vm); otherwise the walk/run speed of the
// table 0x17bdb8 (+8 below +0xc, else +0x18) times vm, and in state 0x73 max(0.8 x, 2.5 vm).
void hero_stick_speed_l1() {
    hero_stick_move(0, 1.0f);
    const uint32_t P = g::PLAYER;
    if (on_aridia() && lw(g::HERO_STATE) == 0x3f) {
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
    if (on_aridia() || lw(g::HERO_STATE) != 0x73) return;
    const float v = lwc1(P + 0x190) * u2f(0x3f4ccccdu);
    const float lo = lwc1(0x0015ed6c) * 2.5f;
    swc1(P + 0x190, v < lo ? lo : v);
}

// Veldin's 0x212318 without the movement mode 1 / 2 probe sizes.
// orig: L1:0x00232978 L2:0x00221258
void hero_wall_slide_l1(int32_t facing) {
    const uint32_t P = g::PLAYER;
    if (lh(P + 0x308) != 0) return;
    if (sub_002339d0(P + 0xe0) < lwc1(0x0015ed6c) * u2f(0x3dcccccdu)) return;
    float lift = u2f(0x3ebd70a4u), len = 0.5f;
    if (static_cast<uint32_t>(lw(P + 0x208c) - 0x11) < 2u) {
        lift = 0.0f;
        len = u2f(0x3f333333u);
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
    if (segment_collide_v(s2, b, 4, lwu(P + 0x2080), nullptr) != 0 && 0 < coll_hit_flags_signed()) surf = sub_001f35d8();
    float pitch = 0.0f;
    if (segment_collide_v(s1, a, 4, lwu(P + 0x2080), nullptr) != 0 && 0 < coll_hit_flags_signed()) {
        pitch = atan2_f(coll_hit_normal().z, vec2_length(coll_hit_normal()));
    }
    if (!(u2f(0x3f5f66f3u) <= pitch) && surf != 8 && surf != 0xc) return;
    coll_hit_clear_normal_z();
    sw(P + 0x240, coll_hit_moby());
    Vec4 n = coll_hit_normal();
    vu0_normalize(n, n);
    coll_hit_set_normal(n);
    const float into = -lwc1(P + 0xe0) * n.x - lwc1(P + 0xe4) * n.y;
    if (!(0.0f < into)) return;
    vec3_scale(n, n, into);
    coll_hit_set_normal(n);
    vec3_add(V(P + 0xe0), V(P + 0xe0), n);
}

// Veldin's 0x2127b8 without the lava surface (kind 3: +0x2f4) and the deadly-water check of states 0x7c / 0x7d.
// Aridia's build (L2:0x002216a0) has the lava surface's own probe (surface kind 3: the height +0x2f4,
// a type-4 segment below the hit) but still not the deadly-water check.
// orig: L1:0x00232dc0 L2:0x002216a0
void hero_ground_update_l1() {
    const uint32_t P = g::PLAYER;
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
            const float h = surface_height(coll_hit_point_addr(), P + 0x280);
            swc1(P + 0x2f0, h);
            if (lwc1(P + 0x88) < h && lw(P + 0x2f8) <= 0) sw(P + 0x2f8, 1);
            if (segment_collide_v(a, b, 0x24, lwu(P + 0x2080), nullptr) == 0) return false;
            int32_t k = sub_001f3608();
            const int32_t lvl = lw(0x0015ed84);
            if (lvl == 1 || lvl == 0x12) k = 3;
            sb(P + 0x12ed, static_cast<uint32_t>(k));
        }
        if (lh(P + 0x12e0) == 0xd) {
            const float cz = coll_hit_point().z;
            swc1(P + 0x2f0, cz);
            a.z = cz - u2f(0x3c23d70au);
            if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) return false;
        }
        if (on_aridia() && lh(P + 0x12e0) == 3) {
            const float cz = coll_hit_point().z;
            swc1(P + 0x2f4, cz);
            a.z = cz - u2f(0x3c23d70au);
            if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) return false;
        }
        if (lh(P + 0x12e0) == 0xb) {
            swc1(P + 0x2f4, coll_hit_point().z);
            return false;
        }
        const uint32_t hit = coll_hit_moby();
        if (hit != 0) {
            if (moby_in_range_class_500(hit) != 0) sh(P + 0x30a, 1);
            const uint32_t pw = moby_pvar_word0(coll_hit_moby());
            if (pw != 0 && (lhu(pw + 0x1e) & 8) != 0) sh(P + 0x30a, 1);
        }
        if (coll_hit_flags_signed() <= 0) return true;
        V(P + 0x2a0) = coll_hit_point();
        swc1(P + 0x2d8, coll_hit_point().z);
        swc1(P + 0x2dc, vec3_distance(V(P + 0x80), V(P + 0x2a0)));
        sw(P + 0x2fc, coll_hit_moby());
        vu0_normalize(V(P + 0x270), coll_hit_normal());
        swc1(P + 0x2e0, sub_00233d30(coll_hit_normal_addr()));
        swc1(P + 0x2ec, atan2_f(coll_hit_normal().x, coll_hit_normal().y));
        Vec4 d;
        vec3_sub(d, V(P + 0x2a0), V(P + 0x80));
        if (0.0f < vec3_dot(d, V(P + 0x270))) swc1(P + 0x2dc, -lwc1(P + 0x2dc));
        if (lwc1(P + 0x2dc) < u2f(0x3ca3d70au)) {
            sh(P + 0x30c, 0);
            if (lwc1(P + 0x2e0) <= u2f(0x3f5f66f3u) || lbu(P + 0x20b3) == 1 || lw(P + 0x208c) == 0x16) {
                sh(P + 0x30e, 0);
                sw(P + 0xa80, coll_hit_moby());
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
        const float dz = coll_hit_point().z - lwc1(P + 0x2d8);
        swc1(P + 0x2c0 + 4 * i, dz);
        if (u2f(0x3ecccccdu) < abs_f(dz)) sw(P + 0x2c0 + 4 * i, 0);
        Vec4 n;
        vu0_normalize(n, coll_hit_normal());
        float sa[2];
        sub_00216078_v(n, sa);
        if (-q < sa[1] && sa[1] < q) swc1(P + 0x2c8 + 4 * i, sa[1]);
        if (-q < sa[0] && sa[0] < q) swc1(P + 0x2d0 + 4 * i, sa[0]);
    }
}

// Push the hero out of geometry (Veldin's 0x2133a8) without the sub-state 0xf sphere.
// Aridia's build (L2:0x00222270) is the same code, also without the sub-state 0x11 arm of the loop, and with the
// collision flags 0x324 (0x24 on Novalis's and Veldin's builds).
// orig: L1:0x00233940 L2:0x00222270
int32_t hero_collide_push_l1(int32_t mode) {
    const uint32_t P = g::PLAYER;
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
    const uint32_t flags = lw(P + 0x2084) == 0x7f ? 0xd24u : (on_aridia() ? 0x324u : 0x24u);
    for (int32_t n = 0; n < 8; ++n) {
        int32_t hit;
        if (lbu(P + 0x20b3) != 0) {
            hit = sphere_collide(P + 0x80, lwc1(0x0015ed60) * u2f(0x3ecccccdu), flags, lwu(P + 0x2080));
        } else if (!on_aridia() && lw(P + 0x208c) == 0x11) {
            hit = sphere_collide(P + 0x80, u2f(0x3f19999au), flags, lwu(P + 0x2080));
        } else {
            float h = lwc1(P + 0x220) - lwc1(P + 0x224);
            if (h < u2f(0x3d4ccccdu)) h = u2f(0x3d4ccccdu);
            const int32_t a = capsule_collide(P + 0x80, lwc1(P + 0x234), h, flags, lwu(P + 0x2080));
            const int32_t b = sphere_mesh_collide(P + 0x80, lwc1(P + 0x234));
            hit = a | b;
        }
        if (hit == 0) break;
        V(P + 0x80) = coll_hit_pushed();
        V(P + 0x200) = coll_hit_normal();
        V(P + 0x210) = coll_hit_point();
        sw(P + 0x23c, coll_hit_moby());
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

// Veldin's 0x213880 without the sub-state tests (0xd / 0xe / 0xf) of the velocity limit. Aridia's build
// (L2:0x002226d8) has the 0xd / 0xe tests (no velocity limit in those sub-states) but not 0xf.
// orig: L1:0x00233de0 L2:0x002226d8 L3:0x0020bbd8
void hero_move_integrate_l1() {
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
        if (!((on_aridia() || on_kerwan()) && (sub == 0xd || sub == 0xe)) &&
            lw(P + 0x1cc) == 0 && r < vec3_length(V(P + 0xe0))) vec3_set_length(V(P + 0xe0), V(P + 0xe0), r);
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

// Accelerate the hero's horizontal velocity 0x13f430 towards the stick target (Veldin's 0x213e68 without
// the copy of the velocity to +0x50).
// orig: L1:0x00234358 L2:0x00222c68
// target (yaw PLAYER+0x180, speed +0x190) by at most dt x (1..3, faster when reversing);
// modes 1/2: approach the speed along the local frame instead.
void sub_00213e68_l1(float dt) {
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

// Hero horizontal velocity PLAYER+0xe0 per state (Veldin's 0x214658, Novalis's build): 0xa (frozen), 0x10
// (gravity ramp, speed +0x3f4), 0x1c (anim-timed dash +0x45c along the facing), 0xb/0xc (wall jump), 0x11
// (dive along +0x470), else ground steering (no state 0x2d, 0x4c, no +0x12e2 / +0x4a8 slope handling).
// Aridia's build (L2:0x00223450) is the same code plus the state 0x2d clause (turn in place).
// orig: L1:0x00234b40 L2:0x00223450 L3:0x0020c950
// (gravity ramp, speed +0x3f4), 0x1c/0x4c (anim-timed dash +0x45c along the facing), 0xb/0xc
// (wall jump: +0x458 along +0x438 plus +0x454 sideways), 0x11 (dive along +0x470), 0x2d
// (turn in place), else ground steering; then velocity damping (+0x150 = +0x920) and drag.
void hero_horizontal_velocity_l1() {
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
    if (st == 0x1c) {
        const float t = lwc1(0x0015ed64);
        if (frame_scale_i(0x28) < lw(P + 0x198)) {
            sub_00211e30(0, t * u2f(0x3d23d70au), t * u2f(0x3e4ccccdu), lwc1(0x0015ed6c) * u2f(0x41702845u));
            sub_00213e68(lwc1(0x0015ed70) * 20.0f);
        }
        const float at = lwc1(P + 0xaa8);
        if (13.0f <= at && at <= 30.0f) {
            const float k = lwc1(OV(0x00166c00u - 0x7e90));
            if (27.0f < at) {
                approach_f(P + 0x45c, 0.0f, k * 8.0f);
            } else {
                const float vm = lwc1(0x0015ed6c);
                approach_f(P + 0x45c, vm + vm, k * 8.0f);
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
    if ((on_aridia() || on_kerwan()) && st == 0x2d) {  // (Aridia's build only: turn in place)
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
            const float rate = lwc1(P + 0x944);
            float sp = vec2_length(V(VEL));
            approach_f_v(sp, 0.0f, rate);
            vec2_set_length(V(VEL), V(VEL), sp);
            return;
        }
        drag = lh(P + 0x30a) == 0 ? k + k : k * 6.0f;
        if (advanced_rac3_movement()) drag = lh(P + 0x30a) == 0 ? k * 8.0f : k * 11.0f;  // enhancement: R&C 3's air braking
    }
    sub_00213e68(drag);
}

}  // namespace rac1
