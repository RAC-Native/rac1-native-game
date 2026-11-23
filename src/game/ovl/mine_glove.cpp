// Ported overlay code: the Mine Glove (item 0x11, hand moby class 0xbe). Its hand moby's update
// 0x2c1ad0 keeps a mine (class 0x4a, spawned by 0x2a9ed0) in the glove, aims (the strafe aim along
// the camera, else straight ahead of the hero) and, on the throw (state 3 of the glove), sets the
// mine flying (0x2aa008) on the arc 0x2c1928 computes. The glove's pvars: +0x40 the mine's place
// in the hand, +0x50 the mine held, +0x54 the refill timer, +0x58 the throw timer, +0x5c its first
// frames.
#include <game/collision/collision.h>
#include <game/enhance/advanced.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr uint32_t PAD = 0x0013c940;
}  // namespace

// The velocity written to `out` (EE address) of a mine thrown from `from` toward `target` (cut to
// `range` away): the horizontal direction's yaw, its speed the one that reaches the target's
// distance (at least `lo`) on the arc under gravity `g` (the vertical rise `dz` allowed for),
// at most the one for `range`; the vertical component is the same speed.
// orig: 0x002c1928
void mine_throw_velocity_v(const Vec4& from, const Vec4& target, uint32_t out, float g, float lo, float range) {
    const float half = (g * range) * 0.5f;
    Vec4 t;
    if (range < vec3_distance(from, target)) {
        vec3_sub(t, target, from);
        vec3_set_length(t, t, range - u2f(0x3c23d70au));
        vec3_add(t, t, from);
    } else {
        t = target;
    }
    float dist = vec2_distance(from, t);
    float dz = t.z - from.z;
    const float yaw = atan2_f(t.x - from.x, t.y - from.y);
    if (dist < lo) dist = lo;
    float k;
    if (dist <= dz) {
        dz = dist - u2f(0x3c23d70au);
        k = dist - dz;
    } else {
        k = dist - dz;
    }
    float q = fpu_div((g * dist) * dist, k + k);
    if (half < q) q = half;
    const float speed = sqrt_f(q);
    swc1(out, cos_vu(yaw) * speed);
    swc1(out + 8, speed);
    swc1(out + 4, sin_vu(yaw) * speed);
}
// orig: 0x002c1928
void mine_throw_velocity(uint32_t from, uint32_t target, uint32_t out, float g, float lo, float range) {
    mine_throw_velocity_v(V(from), V(target), out, g, lo, range);
}

// A mine (class 0x4a) for the glove `hand` at `pos`: half size, a random spin (pvars +0x38 /
// +0x3c from 0x15ed6c), stamped with the frame counter (+0x5c).
// orig: 0x002a9ed0
uint32_t mine_spawn(uint32_t hand, uint32_t pos) {
    const uint32_t m = moby_spawn(0x4a);
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x2c, lwc1(m + 0x2c) * 0.5f);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0x7f);
    sb(m + 0x31, 1);
    sb(m + 0x20, 0);
    sw(pv + 0x30, hand);
    sh(pv + 0x36, 0);
    sw(pv + 0x78, 0);
    V(m + 0x10) = V(pos);
    const float PI = u2f(0x40490fdbu);
    vec_clear(V(pv));
    const float k = u2f(0x3f860a92u);
    swc1(pv + 0x38, rand_range_f(lwc1(0x0015ed6c) * k, lwc1(0x0015ed6c) * PI));
    swc1(pv + 0x3c, rand_range_f(lwc1(0x0015ed6c) * k, lwc1(0x0015ed6c) * PI));
    sw(pv + 0x68, 0xffffffffu);
    swc1(pv + 0x6c, 0.0f);
    sw(pv + 0x5c, lwu(OV(0x0015f5cc)));
    swc1(pv + 0x64, 0.0f);
    swc1(pv + 0x60, 0.0f);
    swc1(pv + 0x70, 0.0f);
    sw(pv + 0x74, 0);
    sw(m + 0x94, 0);
    moby_update_transform(m);
    return m;
}

// The thrown mine's start from `from`: in flight (state 1), its scale over 0.65, a small random
// vector (length 0.01..0.1 frame-scaled) added to its velocity (pvars +0, already set by 0x2c1928),
// the hero's velocity added when riding; a timer +0x34 of 120 frames; of the mines alive (same
// class, list 0x15ffe4) the eight-th oldest (+0x5c) is marked -1; a wall between the hero and
// `from` puts the mine at the hit, lifted by the normal and the velocity reflected and damped.
// orig: 0x002aa008
void mine_throw_start(const Vec4& from, uint32_t mine, uint32_t vel) {
    const uint32_t pv = lwu(mine + 0x78);
    Vec4 r{};
    r.x = rand_range_f(-1.0f, 1.0f);
    r.y = rand_range_f(-1.0f, 1.0f);
    r.z = rand_range_f(-1.0f, 1.0f);
    sb(mine + 0x20, 1);
    const float scale = fpu_div(lwc1(mine + 0x2c), u2f(0x3f266666u));
    swc1(mine + 0x2c, scale);
    const float gs = lwc1(0x0015ed6c);
    const float len = rand_range_f(gs * u2f(0x3c23d70au), gs * u2f(0x3dcccccdu));
    vec3_set_length(r, r, len);
    vec3_add(V(pv), r, V(vel));
    if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0) vec3_add(V(pv), V(pv), V(P + 0x100));
    sh(pv + 0x34, static_cast<uint32_t>(frame_scale_i(0x78)));
    uint32_t count = 0, oldest = 0;
    int32_t t0 = 0;
    for (uint32_t a = lwu(OV(0x0015ffe4)); a != 0; a = lwu(a + 0x28)) {
        const uint32_t st = lbu(a + 0x20);
        if (st == 0xfe || st == 0xfd) continue;
        if (lh(a + 0xa6) != lh(mine + 0xa6)) continue;
        if (a == mine) continue;
        const uint32_t apv = lwu(a + 0x78);
        if (oldest != 0 && !(lw(apv + 0x5c) < t0)) {
            ++count;
            continue;
        }
        t0 = lw(apv + 0x5c);
        oldest = a;
        ++count;
    }
    if (count >= 8) sw(lwu(oldest + 0x78) + 0x5c, 0xffffffffu);
    V(mine + 0x10) = from;
    Vec4 start = V(lwu(P + 0x2080) + 0x10);
    start.z = from.z;
    if (segment_collide_v(start, from, 0, lwu(P + 0x2080), nullptr) != 0) {
        V(mine + 0x10) = coll_hit_point();
        Vec4 push;
        vec3_set_length(push, coll_hit_normal(), lwc1(0x0015ed70) * u2f(0x411b3333u));
        vec3_add(V(mine + 0x10), V(mine + 0x10), push);
        vec3_reflect(V(pv), V(pv), coll_hit_normal());
        vec3_scale(V(pv), V(pv), u2f(0x3f19999au));
    }
}

// The Mine Glove's frame (see the file's head). The mine held sits at the hand's chain point 0
// plus (0, -0.09, -0.02) in the hand's frame; the throw starts on the hero's throw frame (state
// 0x23 at 16 frames, or a quick 0x1e with the button 0x13cae4); while a mine is held its arc is
// aimed (the strafe aim from the camera's frame, else along the hero's facing, the reach and
// speed from the glove's upgrade 0x13e531); then the glove's state (jump table 0x1e9be0): 3
// throws it (stats 0x141708, the list 0x1b07b0), 6 drops it.
// orig: 0x002c1ad0
void mine_glove_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    {
        const int32_t n = lw(pv + 0x5c);
        if (n < 3) {
            sw(pv + 0x5c, static_cast<uint32_t>(n + 1));
            return;
        }
    }
    Vec4 hand_off{};
    hand_off.y = u2f(0xbdb851ecu);
    hand_off.z = u2f(0xbca3d70au);
    Mat4 cam, cam_t;
    mat3_copy(cam, M(OV(0x00166c80)));
    mat3_transpose(cam_t, cam);
    if (pv == 0) return;
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
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x58))) != 0 && lbu(P + 0x20ac) == 0) {
        bool thrown = false;
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
    if (lwu(pv + 0x50) != 0 && lbu(P + 0x20ac) == 0) {
        const uint32_t mpv = lwu(lwu(pv + 0x50) + 0x78);
        const bool up = lbu(0x0013e531) != 0;
        const bool strafe = advanced_camera_aim(lwu(PAD + 0x1a0));
        // the glove's reach scale: 4 (6 / 12 with the strafe aim; 12 with the upgrade)
        const float reach_k = up ? (strafe ? 12.0f : 4.0f) : (strafe ? 6.0f : 4.0f);
        Vec4 aim{};  // sp110
        aim.x = lwc1(0x0015ed6c) * reach_k;
        Vec4 d = aim;  // sp100
        mat3_transform(d, d, M(P + 0x640));
        from.x = cos_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu);
        from.y = sin_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * u2f(0x3f5be9fbu);
        from.z = 0.0f;
        vec3_add(from, from, V(P + 0x80));
        from.z = from.z + u2f(0x3efb4cc2u);
        (void)atan2_f(d.x, d.y);  // (result unused)
        Vec4 target;
        vec3_scale(target, d, lwc1(0x0015ed68) * 60.0f);
        vec3_add(target, target, from);
        if (strafe) {
            // the strafe aim: along the camera's frame, pitch clamped; a hit on the way is the target
            Vec4 e{};
            e.z = up ? 12.0f : 6.0f;
            bool hit = false;
            aim = e;
            mat4_transform(aim, aim, cam_t);
            (void)angle_sub(lwc1(lwu(P + 0x2080) + 0x48), atan2_f(aim.x, aim.y));  // (result unused)
            float yaw = atan2_f(aim.x, aim.y);
            float pitch = atan2_f(vec2_length(aim), aim.z);
            if (u2f(0x3f17e9d8u) < pitch) pitch = u2f(0x3f17e9d8u);
            if (pitch < u2f(0xbfb2b8c2u)) pitch = u2f(0xbfb2b8c2u);
            vec_from_spherical_v(aim, up ? 12.0f : 6.0f, yaw, pitch);
            const float len = vec2_length(aim);
            const float slope = len == 0.0f ? fpu_div(aim.z, u2f(0x3c23d70au)) : fpu_div(aim.z, len);
            const float g98 = lwc1(0x0015ed70) * u2f(0x411ccccdu);
            const float s = sqrt_f((g98 * (up ? 12.0f : 6.0f)) * 0.5f);
            const float reach = fpu_div(((s + s) * s) * (1.0f - slope), lwc1(0x0015ed70) * u2f(0x411ccccdu));
            if (0.0f < reach) {
                const float slope_reach = slope * reach;
                aim.x = cos_vu(yaw) * reach;
                aim.y = sin_vu(yaw) * reach;
                aim.z = slope_reach;
                Vec4 end;
                vec3_add(end, V(OV(0x00166dc0)), aim);
                if (segment_collide_v(V(OV(0x00166dc0)), end, 2, m, nullptr) != 0 && coll_hit_moby() != lwu(P + 0x2080)) {
                    target = coll_hit_point();
                    hit = true;
                    yaw = atan2_f(target.x - from.x, target.y - from.y);
                }
                if (!hit) {
                    target.x = (cos_vu(yaw) * reach) * 5.0f;
                    target.y = (sin_vu(yaw) * reach) * 5.0f;
                    target.z = slope_reach * 5.0f;
                    vec3_add(target, target, V(OV(0x00166dc0)));
                }
            }
        }
        mine_throw_velocity_v(from, target, mpv, lwc1(0x0015ed70) * u2f(0x411ccccdu), 1.0f, reach_k);
    }
    switch (lbu(m + 0x20)) {
    case 0:
        sw(pv + 0x50, 0);
        sb(m + 0x20, (lbu(m + 0x70) & 2) != 0 ? 2u : 1u);
        [[fallthrough]];
    case 1:
        if ((lbu(m + 0x70) & 2) != 0) sb(m + 0x20, 2);
        break;
    case 3: {
        const uint32_t mine = lwu(pv + 0x50);
        if (mine != 0) {
            mine_throw_start(from, mine, lwu(mine + 0x78));
            const uint32_t c = lhu(0x00141708);
            if (!(0xfffe < c)) sh(0x00141708, c + 1);
            const int32_t q = ee_div(frame_scale_i(lw(0x0015eea4)), 0x258);
            if (static_cast<int32_t>(lhu(0x0014170a)) < q) sh(0x0014170a, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 0x258)));
            sw(0x0014170c, lwu(0x0014170c) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
            sub_0025f780(mine, OV(0x001b07b0), 0x3f);
            sw(pv + 0x50, 0);
            sw(pv + 0x54, static_cast<uint32_t>(frame_scale_i(10)));
        }
        sb(m + 0x20, 4);
        sw(pv + 0x58, static_cast<uint32_t>(frame_scale_i(0x14)));
        break;
    }
    case 4:
        if (lw(P + 0x2084) != 0x23 && lbu(P + 0x20a8) == 0) sb(m + 0x20, 2);
        break;
    case 5:
        sb(m + 0x20, 6);
        break;
    case 6:
        if (lwu(pv + 0x50) != 0) {
            moby_retire(lwu(pv + 0x50));
            sw(pv + 0x50, 0);
        }
        break;
    default:
        break;
    }
    (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x54)));
    if (lwu(pv + 0x50) != 0) {
        V(lwu(pv + 0x50) + 0x10) = V(pv + 0x40);
        return;
    }
    if (lw(pv + 0x54) != 0) return;
    if (sub_00233e98(-1) != 0) sw(pv + 0x50, mine_spawn(m, pv + 0x40));
}

// orig: (none) — the glove's update for the hand moby's indirect call.
void mine_glove_fns_register() {
    ovl_register_moby_fn(0x002c1ad0, mine_glove_update);
    ovl_register_moby_fn(0x002aa670, mine_update);
}

}  // namespace rac1
