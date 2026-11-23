// Ported overlay code: the bomb glove. Its hand moby's update 0x2c26c8 keeps a bomb (class 0x79,
// spawned by 0x2ace70) in the glove, aims (the strafe aim along the camera, else the nearest
// target in front, list 0x1aba00) and, on the throw (states 3 / 4 of the glove), sets the bomb
// flying (0x2acfd8) on the arc 0x2c2488 computes. The glove's pvars: +0x40 the bomb's place in
// the hand, +0x50 the bomb held, +0x54 the throw timer, +0x58 its first frames.
#include <game/collision/collision.h>
#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
// orig: (helper) repeated in 0x002c26c8 / 0x002acfd8 — the throw's range: 8.5, more with the
// strafe aim (R1 / L1 held) by the glove's upgrade 0x13e52a
float bomb_range() {
    if (advanced_camera_aim(lwu(PAD + 0x1a0))) return int_to_float(lbu(0x0013e52a)) * 2.5f + 8.5f;
    return 8.5f;
}
}  // namespace

// The velocity `out` of a bomb thrown from `from` toward `target` (at most `range` away, else
// toward the hero's facing when the target is behind or too close): horizontal speed
// 8.5 * 0x15ed6c along the direction, vertical from the arc 0x25abf0 under gravity `g`, capped.
// orig: 0x002c2488
void bomb_throw_velocity_v(const Vec4& from, const Vec4& target, Vec4& out, float g, float range) {
    Vec4 t;
    if (range < vec3_distance(from, target)) {
        vec3_sub(t, target, from);
        vec3_set_length(t, t, range - u2f(0x3c23d70au));
        vec3_add(t, t, from);
    } else {
        t = target;
    }
    float a = atan2_f(t.x - from.x, t.y - from.y);
    bool facing = u2f(0x3f5f66f3u) < angle_dist(lwc1(P + 0x98), a);
    if (!facing) facing = vec2_distance(from, t) < 1.5f;
    if (facing) {
        a = lwc1(P + 0x98);
        Vec4 d;
        d.x = cos_vu(a) * 1.5f;
        d.y = sin_vu(lwc1(P + 0x98)) * 1.5f;
        d.z = 0.0f;
        d.w = 0.0f;
        vec3_add(d, d, from);
        t.x = d.x;
        t.y = d.y;
    }
    const float speed = lwc1(0x0015ed6c) * 8.5f;
    out.x = cos_vu(a) * speed;
    out.z = 0.0f;
    out.y = sin_vu(a) * speed;
    const uint32_t f = ee_stack_alloc(0x20);
    V(f) = from;
    V(f + 0x10) = t;
    out.z = sub_0025abf0(f, f + 0x10, nullptr, speed, -g);
    ee_stack_free(0x20);
    float k;
    if (advanced_camera_aim(lwu(PAD + 0x1a0))) k = fpu_div(int_to_float(lbu(0x0013e52a)) * 2.5f + 8.5f, speed);
    else k = fpu_div(8.5f, speed);
    const float cap = g * k * 0.5f;
    if (cap < out.z) out.z = cap;
}
// orig: 0x002c2488
void bomb_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float range) {
    bomb_throw_velocity_v(V(from), V(target), V(out), g, range);
}

// A bomb (class 0x79) for the glove `owner` at `pos`: a random spin, its speeds from 0x15ed6c.
// orig: 0x002ace70
uint32_t bomb_spawn(uint32_t owner, uint32_t pos) {
    const uint32_t m = moby_spawn(0x79);
    if (m == 0) return 0;
    const float PI = u2f(0x40490fdbu);
    const uint32_t pv = lwu(m + 0x78);
    Vec4 r{};
    r.x = rand_range_f(-PI, PI);
    r.y = rand_range_f(-PI, PI);
    r.z = rand_range_f(-PI, PI);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    sw(pv + 0x50, owner);
    sh(pv + 0x56, 0);
    sh(pv + 0x6a, 0);
    sw(pv + 0x60, 0);
    swc1(pv + 0x6c, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
    sw(pv + 0x64, 0);
    V(m + 0x10) = V(pos);
    vec_clear(V(pv));
    V(m + 0x40) = r;
    const float k = u2f(0x3f860a92u);
    {
        const float s = lwc1(0x0015ed6c);
        swc1(pv + 0x58, rand_range_f(s * k, s * PI));
    }
    {
        const float s = lwc1(0x0015ed6c);
        swc1(pv + 0x5c, rand_range_f(s * k, s * PI));
    }
    moby_update_transform(m);
    return m;
}

// The thrown bomb's start: in flight (state 1), its velocity `vel` (pvars +0); at a lock-on
// target (+0x60, +0x64 set) the arc leads it (behind the hero it is thrown ahead instead);
// timers +0x6a / +0x54; a wall between the hero and `from` drops it there (+0xbc).
// orig: 0x002acfd8
void bomb_throw_start(uint32_t from, uint32_t bomb, uint32_t vel) {
    const uint32_t pv = lwu(bomb + 0x78);
    sh(pv + 0x6a, static_cast<uint32_t>(frame_scale_i(0x1e)));
    sb(bomb + 0x20, 1);
    V(pv) = V(vel);
    bool add_hero = lw(P + 0x208c) == 0xf;
    if (!add_hero && lwu(P + 0x2fc) != 0) add_hero = moby_pvar_word2(lwu(P + 0x2fc)) != 0;
    if (add_hero) {
        vec3_add(V(pv), V(pv), V(P + 0x100));
    } else if (lwu(pv + 0x60) != 0) {
        const uint32_t tgt = lwu(pv + 0x60);
        const int32_t cls = lh(tgt + 0xa6);
        if (cls == 0x56e || cls == 0x58e) {
            vec3_add(V(pv), V(pv), V(pv + 0x40));
        } else if (lwu(pv + 0x64) != 0) {
            float f20 = vec2_length(V(vel));
            Vec4 dir;
            vec3_sub(dir, V(P + 0x80), V(lwu(pv + 0x60) + 0x10));
            vec3_set_length(dir, dir, 1.0f);
            const float along = vec3_dot(dir, V(pv + 0x40));
            f20 = along + f20;
            float s = fpu_div(vec2_distance(V(from), V(lwu(pv + 0x60) + 0x10)), f20);
            Vec4 lead;
            if (0.0f < along) {
                s = s * along;
                vec3_scale(lead, dir, s);
            } else {
                vec3_scale(lead, V(pv + 0x40), s);
            }
            Vec4 aim;
            vec3_add(aim, lead, V(lwu(pv + 0x60) + 0x10));
            lead = aim;
            const float face = atan2_f(lwc1(P + 0x670), lwc1(P + 0x674));
            const float to = atan2_f(lead.x - lwc1(P + 0x80), lead.y - lwc1(P + 0x84));
            if (u2f(0x3fc90fdbu) < angle_dist(face, to)) {
                float c = cos_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
                c = c * vec2_distance(V(P + 0x80), lead);
                aim.x = c * 0.5f;
                float sn = sin_vu(atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
                sn = sn * vec2_distance(V(P + 0x80), lead);
                aim.z = 0.0f;
                aim.y = sn * 0.5f;
                Vec4 ahead;
                vec3_add(ahead, aim, V(P + 0x80));
                lead = ahead;
            }
            Vec4 v;
            bomb_throw_velocity_v(V(from), lead, v, lwc1(0x0015ed70) * 11.0f, bomb_range());
            vec3_scale(v, v, u2f(0x3f266666u));
            vec3_scale(V(pv), V(pv), u2f(0x3eb33333u));
            vec3_add(aim, V(pv), v);
            V(pv) = aim;
        }
    }
    sh(pv + 0x54, static_cast<uint32_t>(frame_scale_i(0x12c)));
    V(bomb + 0x10) = V(from);
    Vec4 start = V(lwu(P + 0x2080) + 0x10);
    start.z = lwc1(from + 8);
    if (segment_collide_v(start, V(from), 0, lwu(P + 0x2080), nullptr) != 0) {
        sb(bomb + 0xbc, 1);
        V(bomb + 0x10) = coll_hit_point();
    }
    if (lbu(0x0013e52a) != 0 && lw(P + 0x2084) == 1) {
        const float z = lwc1(pv + 8);
        vec3_scale(V(pv), V(pv), 1.5f);
        swc1(pv + 8, fpu_div(z, 1.5f));
    }
}

// The bomb glove's frame (see the file's head): the bomb kept in the hand (its chain point 0
// plus (0, -0.09, -0.02) in the hand's frame); the throw started on the hero's throw frame
// (state 0x23 at 16 frames, or a quick 0x1e with the button 0x13cae4); while a bomb is held,
// its arc toward the aim; then the glove's state (0x1e9c00): 3 throws it (counted in the stats
// 0x1416d0 ..), 6 drops it.
// orig: 0x002c26c8
void bomb_glove_update(uint32_t m) {
    if (m == 0) return;
    Vec4 hand_off{};
    hand_off.y = u2f(0xbdb851ecu);
    hand_off.z = u2f(0xbca3d70au);
    const uint32_t pv = lwu(m + 0x78);
    Mat4 cam, cam_t;
    mat3_copy(cam, M(OV(0x00166c80)));
    mat3_transpose(cam_t, cam);
    if (pv == 0) return;
    {
        const int32_t n = lw(pv + 0x58);
        if (n < 3) {
            sw(pv + 0x58, static_cast<uint32_t>(n + 1));
            return;
        }
    }
    Vec4 p0;
    moby_chain_point_v(m, 0, p0);
    Mat4 w;
    moby_chain_world_one_v(m, 0, w);
    Vec4 off;
    mat4_transform(off, hand_off, w);
    vec3_add(V(pv + 0x40), p0, off);
    if (lwu(pv + 0x50) != 0) {
        sw(lwu(pv + 0x50) + 0x98, 1);
        const uint32_t b = lwu(pv + 0x50);
        if (b == 0 || lbu(b + 0x20) == 0xfe || lbu(b + 0x20) == 0xfd) sw(pv + 0x50, 0);
    } else {
        sw(pv + 0x50, 0);
    }
    if (lw(P + 0x2084) == 1) hero_set_state(0x1e, 1);
    bool thrown = false;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x54))) != 0 && lbu(P + 0x20ac) == 0) {
        if (lbu(P + 0x20a8) != 0 && lw(P + 0x1bc) == frame_scale_i(0x11)) thrown = true;
        if (!thrown && (lwu(0x0013cae4) & lwu(P + 0x10a0)) != 0 && lw(P + 0x2084) == 0x1e && sub_00233e98(-1) != 0) thrown = true;
        if (!thrown && lw(P + 0x2084) == 0x23 && lw(P + 0x198) == frame_scale_i(0x10)) thrown = true;
        if (thrown) {
            hero_sound_start(0x1a, 0);
            sub_00233db8(-1, 1);
            sb(m + 0x20, 3);
        }
    }
    Vec4 from{};  // sp70: the throw's start
    if (lwu(pv + 0x50) != 0) {
        const uint32_t bpv = lwu(lwu(pv + 0x50) + 0x78);
        Vec4 fwd{};
        fwd.x = 1.0f;
        mat3_transform(fwd, fwd, M(P + 0x640));
        const float k = u2f(0x3f5be9fbu), lean = u2f(0xbeb953dfu);
        float range_best = 15.0f;
        from.x = cos_vu(angle_add(lean, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * k;
        from.y = sin_vu(angle_add(lean, atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * k;
        from.z = 0.0f;
        vec3_add(from, from, V(P + 0x80));
        from.z = from.z + u2f(0x3efb4cc2u);
        float yaw = atan2_f(fwd.x, fwd.y);
        Vec4 target;
        vec3_scale(target, fwd, bomb_range());
        vec3_add(target, target, from);
        if (advanced_camera_aim(lwu(PAD + 0x1a0)) && lbu(P + 0x20ac) == 0) {
            // the strafe aim: along the camera's forward, clamped in pitch; a hit on the way is the target
            Vec4 d{};
            d.z = 1.0f;
            mat4_transform(d, d, cam_t);
            bool hit = false;
            (void)angle_sub(lwc1(lwu(P + 0x2080) + 0x48), atan2_f(d.x, d.y));  // (result unused)
            yaw = atan2_f(d.x, d.y);
            float pitch = atan2_f(vec2_length(d), d.z);
            if (u2f(0x3f17e9d8u) < pitch) pitch = u2f(0x3f17e9d8u);
            if (pitch < u2f(0xbfb2b8c2u)) pitch = u2f(0xbfb2b8c2u);
            vec_from_spherical_v(d, bomb_range(), yaw, pitch);
            const float len = vec2_length(d);
            float slope = len == 0.0f ? fpu_div(d.z, u2f(0x3c23d70au)) : fpu_div(d.z, len);
            const float g11 = lwc1(0x0015ed70) * 11.0f;
            float q;
            if (advanced_camera_aim(lwu(PAD + 0x1a0))) q = g11 * (int_to_float(lbu(0x0013e52a)) * 2.5f + 8.5f) * 0.5f;
            else q = g11 * 8.5f * 0.5f;
            const float s = sqrt_f(q);
            const float reach = fpu_div((s + s) * s * (1.0f - slope), lwc1(0x0015ed70) * 11.0f);
            slope = slope * reach;
            if (0.0f < reach) {
                d.x = cos_vu(yaw) * reach;
                d.y = sin_vu(yaw) * reach;
                d.z = slope;
                Vec4 end;
                vec3_add(end, V(OV(0x00166dc0)), d);
                if (segment_collide_v(V(OV(0x00166dc0)), end, 2, m, nullptr) != 0 && coll_hit_moby() != lwu(P + 0x2080)) {
                    target = coll_hit_point();
                    hit = true;
                    yaw = atan2_f(target.x - from.x, target.y - from.y);
                }
                if (!hit) {
                    target.x = cos_vu(yaw) * reach * 5.0f;
                    target.y = sin_vu(yaw) * reach * 5.0f;
                    target.z = slope * 5.0f;
                    vec3_add(target, target, V(OV(0x00166dc0)));
                }
            }
        } else {
            // the nearest targetable moby (flag 0x1000) in front (within 45°, or 90° when closer
            // than 4), below 15 away, not too steep, in sight
            sw(P + 0xa50, 0);
            for (uint32_t l = OV(0x001aba00); lwu(l) != 0; l += 4) {
                const uint32_t o = lwu(l);
                const uint32_t pw = moby_pvar_word0(o);
                if (static_cast<int8_t>(lbu(o + 0x20)) < 0 || pw == 0) continue;
                if ((lhu(o + 0x34) & 0x1000) == 0) continue;
                Vec4 c = V(o + 0x10);
                c.z = c.z + lwc1(pw + 0x10);
                const float dist = vec2_distance(from, c);
                const float a = atan2_f(c.x - from.x, c.y - from.y);
                const float pitch = atan2_f(vec2_distance(from, c), c.z - from.z);
                const float off_a = angle_dist(yaw, a);
                if (!(off_a < u2f(0x3f490fdbu))) {
                    if (!(dist < 4.0f)) continue;
                    if (!(off_a < u2f(0x3fc90fdbu))) continue;
                }
                if (!(dist < range_best)) continue;
                if (!(pitch < u2f(0x3f75be0bu))) continue;
                if (!(c.z - from.z < 2.0f)) continue;
                if (segment_collide_v(V(OV(0x00166dc0)), c, 6, m, nullptr) != 0) continue;
                range_best = dist;
                sw(P + 0xa50, o);
                yaw = a;
                target = c;
            }
        }
        float rng;
        if (advanced_camera_aim(lwu(PAD + 0x1a0))) rng = int_to_float(lbu(0x0013e52a)) * 2.5f + 8.5f;
        else rng = 8.5f;
        bomb_throw_velocity_v(from, target, V(bpv), lwc1(0x0015ed70) * 11.0f, rng);
    }
    const uint32_t st = lbu(m + 0x20);
    switch (st) {
    case 0:
        sw(pv + 0x50, 0);
        sb(m + 0x20, (lbu(m + 0x70) & 2) != 0 ? 2u : 1u);
        [[fallthrough]];
    case 1:
        if ((lbu(m + 0x70) & 2) != 0) sb(m + 0x20, 2);
        break;
    case 3:
        if (lwu(pv + 0x50) == 0) {
            sw(pv + 0x50, bomb_spawn(m, pv + 0x40));
            break;
        } else {
            const uint32_t b = lwu(pv + 0x50);
            const uint32_t f = ee_stack_alloc(0x10);
            V(f) = from;
            bomb_throw_start(f, b, lwu(b + 0x78));
            ee_stack_free(0x10);
            const uint32_t c = lhu(0x001416d0);
            if (!(0xfffe < c)) sh(0x001416d0, c + 1);
            const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
            if (static_cast<int32_t>(lhu(0x001416d2)) < q) sh(0x001416d2, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
            sw(0x001416d4, lwu(0x001416d4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
            sw(pv + 0x50, 0);
            sb(m + 0x20, 4);
            sw(pv + 0x54, static_cast<uint32_t>(frame_scale_i(0x14)));
        }
        break;
    case 4:
        if (lw(P + 0x2084) != 0x23 && lbu(P + 0x20a8) == 0) sb(m + 0x20, 2);
        break;
    case 5:
        sb(m + 0x20, 6);
        break;
    case 6:
        if (lwu(pv + 0x50) == 0) {
            if (sub_00233e98(-1) != 0 || lbu(P + 0x20a8) != 0) sw(pv + 0x50, bomb_spawn(m, pv + 0x40));
            return;
        }
        moby_retire(lwu(pv + 0x50));
        break;
    default:
        break;
    }
    if (lwu(pv + 0x50) != 0) {
        V(lwu(pv + 0x50) + 0x10) = V(pv + 0x40);
        return;
    }
    if (sub_00233e98(-1) != 0 || lbu(P + 0x20a8) != 0) sw(pv + 0x50, bomb_spawn(m, pv + 0x40));
}

// orig: (none) — the glove's update for the hand moby's indirect call.
void bomb_glove_fns_register() {
    ovl_register_moby_fn(0x002c26c8, bomb_glove_update);
    ovl_register_moby_fn(0x002adb30, bomb_update);
    ovl_register_moby_fn(0x002af5b8, debris_7a_update);
}

}  // namespace rac1
