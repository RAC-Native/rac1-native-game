// Novalis's (level 1) variants of the hero's ledge checks: Veldin's code minus the
// +0x20a4 == 1 movement mode (Novalis's build has no such branch). Addresses are Novalis's.
#include <game/collision/collision.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Ledge check from pos (facing PLAYER+0x98, compared with *ref_yaw), as Veldin's 0x20c758 without the
// +0x20a4 == 1 sizes: top 1.5, high 1.15, step 0.3, drop -0.7, min 1.8.
// orig: L1:0x0022d838 L2:0x0021c228
int32_t hero_ledge_check_v_l1(const Vec4& pos, const float* ref_yaw) {
    const uint32_t P = g::PLAYER;
    if (lh(P + 0x308) == 2) return 0;
    float drop = u2f(0xbf333333u), step = u2f(0x3e99999au), top = 1.5f, high = u2f(0x3f933333u), min = u2f(0x3fe66666u);
    Vec4 p = pos;
    p.x = p.x + cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    p.y = p.y + sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    bool found = false;
    Vec4 a, b;
    for (int32_t i = 0; i < 4 && !found; ++i) {
        p.x = p.x + cos_vu(lwc1(P + 0x98)) * step * 0.25f;
        p.y = p.y + sin_vu(lwc1(P + 0x98)) * step * 0.25f;
        a = p;
        a.z = a.z + (top + u2f(0x3e19999au));
        b = p;
        b.z = b.z + high;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) continue;
        const int32_t t = sub_001f35d8();
        if (t == 9 || t == 0xc) continue;
        if (atan2_f(coll_hit_normal().z, vec2_length(coll_hit_normal())) < u2f(0x3eb2b8c2u)) found = true;
    }
    if (!found) return 0;
    if (pos.z + top < coll_hit_point().z) return 0;
    const Vec4 c = coll_hit_point();
    const float dir = atan2_f(c.x - pos.x, c.y - pos.y);
    if (c.z - lwc1(P + 0x2d8) < min) return 0;
    bool wall = false;
    float wall_yaw = 0.0f;
    for (int32_t j = 0; j < 5; ++j) {
        a = pos;
        b = c;
        const float r = lwc1(P + 0x234) + lwc1(P + 0x234);
        b.x = b.x + cos_vu(dir) * r;
        b.y = b.y + sin_vu(dir) * r;
        const float z = b.z + drop * (1.0f - fpu_div(static_cast<float>(j), 5.0f));
        b.z = z;
        a.z = z;
        if (segment_collide_v(a, b, 2, 0, nullptr) != 0) {
            wall = true;
            wall_yaw = atan2_f(coll_hit_normal().x, coll_hit_normal().y);
            break;
        }
    }
    if (!wall) return 0;
    if (!(angle_dist(*ref_yaw, angle_add(wall_yaw, u2f(0x40490fdbu))) < u2f(0x3f91361eu))) return 0;
    for (float d = 0.0f;;) {
        a = c;
        a.x = a.x + cos_vu(wall_yaw) * d;
        a.y = a.y + sin_vu(wall_yaw) * d;
        b = a;
        a.z = a.z + 0.5f;
        b.z = b.z - 0.5f;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) break;
        d = d + u2f(0x3d8f5c29u);
        if (!(d < u2f(0x4019999au))) return 0;
    }
    const float d = angle_dist(angle_add(wall_yaw, u2f(0x40490fdbu)), *ref_yaw);
    return u2f(0x3f060a92u) < d ? 0 : 1;
}

// Ledge grab while jumping / falling, as Veldin's 0x20bed0 without the +0x20a4 == 1 branches (its
// extra grab states 0x4d..0x4f and its smaller sizes): substate 4 rising slower than 3.5 x frame scale
// or with +0x41e, substate 2, states 8 / 0x1a / 0x1b (not state 0xe below 37 on +0xaa8).
// orig: L1:0x0022d090 L2:0x0021ba80
void hero_ledge_grab_check_l1() {
    const uint32_t P = g::PLAYER;
    if (lh(P + 0x308) == 2) return;
    sw(P + 0x4e8, 0);
    if (lh(P + 0x1b0) != 0) return;
    bool ok = false;
    const float fs = lwc1(0x0015ed6c);
    if (lbu(P + 0x20a4) == 0) {
        const int32_t sub = lw(P + 0x208c);
        if (sub == 4 && (lh(P + 0x41e) != 0 || lwc1(P + 0xe8) < fs * 3.5f)) {
            ok = true;
        } else if (sub == 2) {
            ok = true;
        } else {
            const int32_t st = lw(P + 0x2084);
            if (st == 8 || st == 0x1a || st == 0x1b) ok = true;
        }
        if (lw(P + 0x2084) == 0xe && lwc1(P + 0xaa8) < 37.0f) ok = false;
    }
    if (!ok) return;
    float drop = u2f(0xbf333333u), step = u2f(0x3e99999au), top = 1.5f, high = u2f(0x3f933333u), hang = u2f(0xbfb70a3du), min = u2f(0x3fe66666u), back = u2f(0x3ee66666u);
    const float g = lw(P + 0x208c) == 4 ? lwc1(P + 0x4a0) : lwc1(P + 0x940);
    Vec4 p;
    sub_00259508_v(p, V(P + 0x80), V(P + 0x110), frame_scale_i(2), g);
    p.x = p.x + cos_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    p.y = p.y + sin_vu(lwc1(P + 0x98)) * lwc1(P + 0x234);
    bool found = false;
    Vec4 a, b;
    for (int32_t i = 0; i < 4 && !found; ++i) {
        p.x = p.x + cos_vu(lwc1(P + 0x98)) * step * 0.25f;
        p.y = p.y + sin_vu(lwc1(P + 0x98)) * step * 0.25f;
        a = p;
        a.z = a.z + (top + u2f(0x3e19999au));
        b = p;
        b.z = b.z + high;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) continue;
        const int32_t t = sub_001f35d8();
        if (t == 9 || t == 0xc || t == 0) continue;
        if (atan2_f(coll_hit_normal().z, vec2_length(coll_hit_normal())) < u2f(0x3eb2b8c2u)) found = true;
    }
    if (!found) return;
    const uint32_t hit = coll_hit_moby();
    if (hit != 0) {
        const uint32_t pw = moby_pvar_word0(hit);
        const uint32_t w2 = moby_pvar_word2(coll_hit_moby());
        bool grab = false;
        if (pw != 0 && (lhu(pw + 0x1e) & 1) != 0) {
            grab = true;
        } else if (w2 != 0) {
            grab = (lw(w2 + 0x3c) & 1) != 0;
        }
        if (!grab) return;
    }
    sw(P + 0x4f8, coll_hit_moby());
    if (lwc1(P + 0x88) + top < coll_hit_point().z) return;
    const Vec4 c = coll_hit_point();
    Vec4 c5 = c;
    c5.z = c5.z + u2f(0x3d4ccccdu);  // (sp+0x20)
    Vec4 h = V(P + 0x80);
    h.z = c5.z;
    if (segment_collide_v(h, c5, 4, lwu(P + 0x2080), nullptr) != 0) return;
    Vec4 cap = coll_hit_point();
    cap.z = cap.z + u2f(0x3f19999au);
    if (capsule_collide_v(cap, u2f(0x3ee66666u), u2f(0x3f19999au), 4, lwu(P + 0x2080)) != 0) {
        sw(P + 0x4f4, lwu(P + 0x4f4) | 1u);
    } else {
        sw(P + 0x4f4, lwu(P + 0x4f4) & 0xfffffffeu);
    }
    const float dir = atan2_f(c.x - lwc1(P + 0x80), c.y - lwc1(P + 0x84));
    swc1(P + 0x4e0, c.z);
    if (c.z - lwc1(P + 0x2d8) < min) return;
    bool wall = false;
    for (int32_t j = 0; j < 5; ++j) {
        a = V(P + 0x80);
        b = c;
        const float r = lwc1(P + 0x234) + lwc1(P + 0x234);
        b.x = b.x + cos_vu(dir) * r;
        b.y = b.y + sin_vu(dir) * r;
        const float z = b.z + drop * (1.0f - fpu_div(static_cast<float>(j), 5.0f));
        b.z = z;
        a.z = z;
        if (segment_collide_v(a, b, 2, 0, nullptr) != 0) {
            wall = true;
            swc1(P + 0x4e4, atan2_f(coll_hit_normal().x, coll_hit_normal().y));
            break;
        }
    }
    if (!wall) return;
    if (!(angle_dist(lwc1(P + 0x98), angle_add(lwc1(P + 0x4e4), u2f(0x40490fdbu))) < u2f(0x3f91361eu))) return;
    for (float d = 0.0f;;) {
        a = c;
        a.x = a.x + cos_vu(lwc1(P + 0x4e4)) * d;
        a.y = a.y + sin_vu(lwc1(P + 0x4e4)) * d;
        b = a;
        a.z = a.z + 0.5f;
        b.z = b.z - 0.5f;
        if (segment_collide_v(a, b, 4, lwu(P + 0x2080), nullptr) == 0) break;
        d = d + u2f(0x3cf5c28fu);
        if (!(d < u2f(0x4019999au))) return;
    }
    Vec4 e = a;  // the last walk probe's start (sp+0x60)
    const float k = -(u2f(0x3cf5c28fu) * 0.5f);
    e.z = c.z;
    e.x = e.x + cos_vu(lwc1(P + 0x4e4)) * k;
    e.y = e.y + sin_vu(lwc1(P + 0x4e4)) * k;
    V(P + 0x4d0) = e;
    swc1(P + 0x4d0, lwc1(P + 0x4d0) + cos_vu(lwc1(P + 0x4e4)) * back);
    const float sy = sin_vu(lwc1(P + 0x4e4)) * back;
    sw(P + 0x4e8, 1);
    swc1(P + 0x4d4, lwc1(P + 0x4d4) + sy);
    swc1(P + 0x4d8, lwc1(P + 0x4d8) + hang);
}

}  // namespace rac1
