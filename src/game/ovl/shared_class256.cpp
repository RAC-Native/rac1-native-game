// Class 256's update: one port of the copies Quartu and Veldin carry (the same code relinked). Each level keeps its own
// read-only data (per level below: the burst's effect description, 0x50 bytes, and the two six-colour tables of its
// sparks right after it); the rest is the engine's (OV: the camera 0x166dc0 and its shake 0x166c80 + 0x160 / + 0x168, the
// collision's results 0x173e40.., the hit list 0x177f00, the rocket list 0x1b0770, the smoke's texture 0x1b20dc, the
// particle load 0x15f5d0, the camera effects' switch 0x15f30c) or the base ELF's (bare: the frame time 0x15ed6c, its square
// 0x15ed70, the frame rate's step 0x15ed64, the hero 0x13f350 and his moby 0x1413d0). Header shared_class256.h.
//
// The pvars: +0 the target's last place, +0x10 its owner (not hit), +0x14 its life, +0x18 the target, +0x1c the height aimed
// at above it, +0x20 the speed, +0x24 / +0x28 the turn rates, +0x2c / +0x30 the heading it keeps without a target, +0x34 the
// speed it reaches for, +0x38 leading the target, +0x3c / +0x3e the share of the hero's velocity it carries (a countdown over
// its start).
#include <game/ovl/shared_class256.h>

#include <game/math/vecmath.h>
#include <game/ovl/devastator.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rocket.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr float TWO_PI = 6.2831854820251465f;    // (0x40c90fdb)
constexpr float PI = 3.1415927410125732f;        // (0x40490fdb)
constexpr float THREE_HALF_PI = 4.71238899230957f;  // (0x4096cbe4)

// the level's burst effect description (0x50 bytes), then its sparks' two colour tables (six words each, at +0x50 / +0x68)
uint32_t burst_data() { return on_level(VELDIN_FINALE) ? L18_ADDR(0x001f1ee0) : L15_ADDR(0x001e2610); }

float dt() { return lwc1(0x0015ed6c); }

void copy_words(uint32_t to, uint32_t from, uint32_t bytes) {
    for (uint32_t k = 0; k < bytes; k += 4) sw(to + k, lwu(from + k));
}

// The time to meet the target (the later root of the lead's quadratic when both are positive, else the positive one; -1
// when neither is)
float meet_time(float b, float a, float c_neg) {
    const float disc = fpu_sub(fpu_mul(b, b), fpu_mul(fpu_mul(a, 4.0f), c_neg));
    const float root = sqrt_f(disc);
    const float nb = -b, a2 = fpu_add(a, a);
    const float t1 = fpu_div(fpu_add(nb, root), a2);
    const float t2 = fpu_div(fpu_sub(nb, root), a2);
    if (0.0f < t1 && 0.0f < t2) return !(t2 < t1) ? t2 : t1;
    if (0.0f < t1) return t1;
    if (0.0f < t2) return t2;
    return -1.0f;
}
}  // namespace

// It speeds up towards its speed (+0x34), turns towards its target (leading it when +0x38 is set, the camera shaken with
// the distance to it while the camera effects are on) or to its kept heading, trails smoke, a glow and a spark, and moves
// (carrying a share of the hero's velocity). It leaves when it falls out of the level's box, when its life runs out or when
// it is 260 from the hero; it bursts on the world or on a moby other than the hero's and its owner's: the mobys within 2
// hurt, sparks, debris thrown towards the camera, puffs, shards, the camera shaken, the burst's sound and effect.
// orig: L15:0x0029d6c0 L18:0x002aa3b0
void class256_update(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x3c0);
    // the frame's places (the original's offsets)
    const uint32_t effect = f, to_cam = f + 0x50, was = f + 0x60, refl = f + 0x70, norm = f + 0x80, puff = f + 0x90;
    const uint32_t vel = f + 0xa0, side = f + 0xb0, carry = f + 0xc0, d0 = f + 0xd0, e0 = f + 0xe0, tgt = f + 0xf0;
    const uint32_t lead = f + 0x100, k1 = f + 0x110, k2 = f + 0x150, rot = f + 0x190, msin = f + 0x1d0, mcos = f + 0x210;
    const uint32_t col_a = f + 0x250, col_b = f + 0x270, col_c = f + 0x260, col_d = f + 0x280, spray = f + 0x2a0, rnd = f + 0x2b0;
    const uint32_t pv = lwu(m + 0x78), pos = m + 0x10;
    const uint32_t data = burst_data();
    copy_words(effect, data, 0x50);
    uint32_t direct = 0;
    V(norm) = Vec4{};
    swc1(norm + 8, fpu_mul(dt(), 8.0f));
    lq_sq(refl, norm);
    V(norm) = Vec4{};
    swc1(norm + 8, 1.0f);

    const uint32_t target = lwu(pv + 0x18);
    {
        const float speed = lwc1(pv + 0x20);
        swc1(pv + 0x20, fpu_add(speed, fpu_mul(fpu_div(fpu_sub(lwc1(pv + 0x34), speed), 10.0f), lwc1(0x0015ed64))));
    }
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x3c)));
    {
        const float share = int_to_float(lh(pv + 0x3c));
        vec3_scale(V(carry), V(0x0013f490), fpu_div(share, int_to_float(lh(pv + 0x3e))));
    }
    if (lw(OV(0x0015f30c)) != 0 && target != 0) {
        const float d = fpu_div(vec3_distance(V(OV(0x00166dc0)), V(target + 0x10)), 50.0f);
        lq_sq(d0, target + 0x10);
        swc1(d0 + 8, fpu_add(lwc1(d0 + 8), lwc1(pv + 0x1c)));
        const float strength = abs_f(fpu_sub(0.5f, fpu_mul(d, 0.35f)));
        cam_effect_add(m, 0xff0fffffu, &V(d0), 0xd, -1, strength, 0.0f, 90.0f);
    }

    // the heading it turns to
    float yaw_goal = 0.0f, pitch_goal = 0.0f;
    const bool homing = target != 0 && lw(pv + 0x14) < frame_scale_i(0x96) - frame_scale_i(0) &&
                        static_cast<int8_t>(lbu(target + 0x20)) >= 0;
    if (homing) {
        float t = 0.0f;
        lq_sq(tgt, target + 0x10);
        swc1(tgt + 8, fpu_add(lwc1(tgt + 8), lwc1(pv + 0x1c)));
        vec3_sub(V(d0), V(tgt), V(pos));
        if (lw(pv + 0x38) != 0) {
            vec3_sub(V(e0), V(tgt), V(pv));
            rocket_1ff2c0(lead, e0, carry);
            lq_sq(pv, tgt);
            const float v2 = vec3_length_sq_vu(V(e0));
            float b = fpu_mul(lwc1(e0), lwc1(d0));
            const float by = fpu_mul(lwc1(e0 + 4), lwc1(d0 + 4));
            const float bz = fpu_mul(lwc1(e0 + 8), lwc1(d0 + 8));
            const float speed = lwc1(pv + 0x20);
            b = fpu_mul(fpu_add(fpu_add(b, by), bz), -2.0f);
            const float a = fpu_sub(fpu_mul(speed, speed), v2);
            t = meet_time(b, a, -vec3_length_sq_vu(V(d0)));
        }
        uint32_t aim = d0;
        if (0.0f < t && t < static_cast<float>(frame_scale_i(0x96))) {
            vec3_scale(V(lead), V(e0), t);
            vec3_add(V(lead), V(lead), V(d0));
            aim = lead;
        }
        yaw_goal = atan2_f(lwc1(aim), lwc1(aim + 4));
        pitch_goal = -atan2_f(vec2_length(V(aim)), lwc1(aim + 8));
    } else if (lw(pv + 0x14) < frame_scale_i(0x96) - frame_scale_i(0)) {
        yaw_goal = lwc1(pv + 0x2c);
        pitch_goal = lwc1(pv + 0x30);
    } else {
        yaw_goal = lwc1(m + 0x48);
        pitch_goal = lwc1(m + 0x44);
    }
    {
        // (a quicker turn with a target: its rate limit a turn a second, else three quarters of one)
        const float limit = target != 0 ? TWO_PI : THREE_HALF_PI;
        const float sq = lwc1(0x0015ed70);
        const float yaw = angle_seek_rate(pv + 0x24, lwc1(m + 0x48), yaw_goal, fpu_mul(sq, TWO_PI), fpu_mul(sq, PI), fpu_mul(dt(), limit));
        const float sq2 = lwc1(0x0015ed70);
        const float pitch_now = lwc1(m + 0x44);
        const float rate = fpu_mul(dt(), limit);
        swc1(m + 0x48, yaw);
        swc1(m + 0x44, angle_seek_rate(pv + 0x28, pitch_now, pitch_goal, fpu_mul(sq2, TWO_PI), fpu_mul(sq2, PI), rate));
    }
    vec_from_spherical_v(V(vel), lwc1(pv + 0x20), lwc1(m + 0x48), -lwc1(m + 0x44));

    // the trail: a smoke puff behind it drifting at random, a glow at its tail, a spark off its axis
    V(e0) = Vec4{};
    swc1(e0, rand_range_f(-1.0f, 1.0f));
    swc1(e0 + 4, rand_range_f(-1.0f, 1.0f));
    swc1(e0 + 8, rand_range_f(-1.0f, 1.0f));
    lq_sq(d0, e0);
    vec3_set_length(V(d0), V(d0), fpu_mul(rand_range_f(0.1f, 0.2f), dt()));
    {
        const float back = rand_range_f(fpu_mul(dt(), 0.1f), dt());
        vec3_set_length(V(puff), V(vel), -back);
    }
    vec3_add(V(d0), V(d0), V(puff));
    vec3_set_length(V(puff), V(vel), fpu_mul(rand_range_f(0.0f, 1.0f), lwc1(pv + 0x20)));
    vec3_add(V(puff), V(puff), V(pos));
    {
        const uint32_t p = static_cast<uint32_t>(dev_2712b8(puff, d0, static_cast<uint32_t>(frame_scale_i(0x3c)), 0x7f, 0x606060, 3,
                                                            40000.0f, 1000.0f, 1.0f, u2f(0xb951b717u), 0.0f));  // (-0.0002)
        if (p != 0) {
            const uint32_t tex = lbu(lwu(OV(0x001b20dc)));
            sb(p + 3, 0x44);
            sb(p + 2, tex);
        }
    }
    dev_2712b8(pos, d0, static_cast<uint32_t>(frame_scale_i(6)), 0x7f, 0xb0b0b0, 3, 40000.0f, 1000.0f, 1.0f,
               u2f(0xb9d1b717u), 0.0f);  // (-0.0004)
    V(e0) = Vec4{};
    swc1(e0 + 8, u2f(0x3ca3d70au));  // (0.02)
    mat3_transform(V(e0), V(e0), M(m + 0xc0));
    vec_rotate_axis(V(e0), V(e0), V(m + 0xc0), rand_angle());
    spawn_particle_15_v(V(puff), V(e0), 0x4f007fffu, 0x1fffffffu, static_cast<uint32_t>(frame_scale_i(5)), 1, 20000.0f);

    // the move
    lq_sq(was, pos);
    vec3_add_inplace(V(vel), V(carry));
    vec3_add(V(pos), V(pos), V(vel));
    auto leave = [&] {
        moby_list_remove(m, OV(0x001b0770));
        moby_retire(m);
        ee_stack_free(0x3c0);
    };
    if (lwc1(pos) < 0.0f || lwc1(pos + 4) < 0.0f || lwc1(pos + 8) < 0.0f) return leave();
    // the hit it gives (from its heading, 3 of damage, kinds 0x830000) and the segment it moved along
    sw(e0 + 4, 0x830000);
    swc1(e0 + 0xc, 3.0f);
    sw(e0, m);
    sw(tgt, 1);
    lq_sq(d0, vel);
    vec2_set_length(V(d0), V(d0), 1.0f);
    swc1(d0 + 0xc, u2f(0x45afdf66u));  // (5627.92)
    sb(e0 + 8, 3);
    sh(e0 + 0xa, lhu(m + 0xa6));
    swc1(d0 + 8, 1.0f);
    sb(e0 + 9, 1);
    const uint32_t hero_moby = lwu(0x001413d0);
    if (segment_collide(was, pos, 0, hero_moby, d0) != 0) {
        const uint32_t res = OV(0x00173e40);
        const uint32_t hit = lwu(res + 0x18);
        uint32_t wall = 0;
        if (hit == 0) {
            if (lw(res + 0x1c) > 0) {
                sb(m + 0xbc, 1);
                lq_sq(pos, res + 0x20);
                vec3_sub(V(side), V(res + 0x50), V(res + 0x60));
                wall = res + 0x40;
            }
        } else if (hit != lwu(0x001413d0)) {
            if (hit != lwu(pv + 0x10)) {
                sb(m + 0xbc, 2);
                lq_sq(pos, res + 0x20);
                vec3_sub(V(side), V(res + 0x50), V(res + 0x60));
                wall = res + 0x40;
                direct = m;
            }
        }
        if (wall != 0) {
            vec3_set_length(V(side), V(side), 1.0f);
            vec3_reflect(V(refl), V(vel), V(wall));
            vec3_set_length(V(refl), V(refl), fpu_add(dt(), dt()));
            vec3_set_length(V(norm), V(wall), 1.0f);
        }
    } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14))) != 0 ||
               260.0f < vec2_distance(V(pos), V(0x0013f3d0))) {
        return leave();
    }
    if (lbu(m + 0xbc) == 0) {
        ee_stack_free(0x3c0);
        return;
    }

    // the burst
    {
        const int32_t n = mobys_in_sphere(pos, 2.0f, 0x10, m, 0);
        lq_sq(lead, pos);
        moby_list_hit(m, lead, OV(0x00177f00), n, direct, 0x810000, 3, 1, 3.0f, 1.0f, 1.0f);
    }
    // (the cross-product matrix of the burst's normal, and its square: the sparks spread about the plane it hit)
    {
        const float nx = lwc1(norm), ny = lwc1(norm + 4), nz = lwc1(norm + 8);
        sw(k1, 0); swc1(k1 + 4, nz); swc1(k1 + 8, -ny); sw(k1 + 0xc, 0);
        swc1(k1 + 0x10, -nz); sw(k1 + 0x14, 0); swc1(k1 + 0x18, nx); sw(k1 + 0x1c, 0);
        swc1(k1 + 0x20, ny); swc1(k1 + 0x24, -nx); sw(k1 + 0x28, 0); sw(k1 + 0x2c, 0);
        for (uint32_t k = 0x30; k < 0x40; k += 4) sw(k1 + k, 0);
        mat4_mul(M(k2), M(k1), M(k1));
    }
    for (int i = 0; i < 10; ++i) {
        if (lbu(m + 0xbc) == 1) {  // (off the world: about the side, turned about the normal at random)
            const float ang = rand_angle();
            rocket_1ffd48(mcos, k2, fpu_sub(1.0f, cos_vu(ang)));
            rocket_1ffd48(msin, k1, sin_vu(ang));
            rocket_1ffe20(msin, msin, mcos);
            mat4_identity(M(mcos));
            rocket_1ffe20(rot, msin, mcos);
            mat4_transform(V(lead), V(side), M(rot));
            vec3_set_length(V(lead), V(lead), fpu_mul(rand_range_f(8.5f, 16.5f), dt()));
        } else {
            const float speed = fpu_mul(rand_range_f(8.5f, 16.5f), dt());
            const float yaw = rand_angle();
            vec_from_spherical_v(V(lead), speed, yaw, rand_angle());
        }
        swc1(lead + 8, fpu_add(lwc1(lead + 8), fpu_mul(dt(), 5.0f)));
        vec3_add(V(lead), V(lead), V(refl));
        const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x78));
        spawn_particle_0f(pos, lead, 0x4f007fffu, 0x1f00007fu, static_cast<uint32_t>(life), 1, -1, -1, 40000.0f);
    }
    // debris thrown about and towards the camera
    V(col_a) = Vec4{};
    swc1(col_a, rand_range_f(-1.0f, 1.0f));
    swc1(col_a + 4, rand_range_f(-1.0f, 1.0f));
    swc1(col_a + 8, rand_range_f(-1.0f, 1.0f));
    lq_sq(lead, col_a);
    vec3_sub(V(to_cam), V(OV(0x00166dc0)), V(pos));
    const float cam_dist = vec3_length(V(to_cam));
    {
        const float scatter = fpu_div(cam_dist, 5.0f);
        swc1(to_cam + 8, fpu_add(lwc1(to_cam + 8), fpu_mul(cam_dist, 0.5f)));
        vec3_set_length(V(lead), V(lead), fpu_mul(scatter, dt()));
    }
    vec3_set_length(V(to_cam), V(to_cam), fpu_mul(fpu_add(cam_dist, cam_dist), dt()));
    vec3_add(V(lead), V(lead), V(to_cam));
    vec3_clamp_length_vu(V(lead), V(lead), fpu_mul(dt(), 10.0f));
    debris_7a_spawn(pos, lead, static_cast<uint32_t>(rand_range(frame_scale_i(0x3c), frame_scale_i(0x5a))), 0, 0);
    // puffs: fewer and slower the nearer the camera
    const int32_t puffs = cam_dist < 6.0f ? float_to_int(cam_dist) / 2 : 3;
    const float close = cam_dist < 7.0f ? fpu_sub(7.0f, cam_dist) : 0.0f;
    for (int32_t i = puffs; i > 0; --i) {
        const float r = rand_range_f(8.0f, 10.0f);
        copy_words(col_c, data + 0x50, 0x18);
        copy_words(col_d, data + 0x68, 0x18);
        const float speed = fpu_sub(fpu_mul(r, dt()), fpu_mul(close, dt()));
        const uint32_t c0 = col_c + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t c1 = col_d + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const int32_t n1 = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        const int32_t n2 = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
        spawn_particle_0b(pos, refl, lwu(c0), lwu(c1), static_cast<uint32_t>(n1), static_cast<uint32_t>(n2), 0, 0, 400000.0f, speed);
        const int32_t n3 = rand_range(frame_scale_i(5), frame_scale_i(0xa));
        const int32_t n4 = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
        spawn_particle_0b(pos, refl, 0x7fffffffu, 0x00ffffffu, static_cast<uint32_t>(n3), static_cast<uint32_t>(n4), 0, 0, 400000.0f,
                          fpu_mul(speed, 0.5f));
    }
    // sparks
    for (int i = 0; i < 10; ++i) {
        copy_words(col_a, data + 0x50, 0x18);
        copy_words(col_b, data + 0x68, 0x18);
        V(rnd) = Vec4{};
        swc1(rnd, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 4, rand_range_f(-1.0f, 1.0f));
        swc1(rnd + 8, rand_range_f(-1.0f, 1.0f));
        lq_sq(spray, rnd);
        vec3_set_length(V(spray), V(spray), fpu_mul(rand_range_f(0.0f, 3.0f), dt()));
        const uint32_t c0 = col_a + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const uint32_t c1 = col_b + (static_cast<uint32_t>(rand_mod(6)) << 2);
        const int32_t life = rand_range(frame_scale_i(0x14), frame_scale_i(0x23));
        spawn_particle_08(pos, spray, lwu(c0), lwu(c1), life, 200000.0f);
    }
    // shards (two grey ones while the particle load is low and the camera is off)
    if (lwc1(OV(0x0015f5d0)) < u2f(0x3f733333u) && 9.0f < cam_dist) {  // (0.95)
        shard_spawn(m, pos, refl, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, 4.0f);
        shard_spawn(m, pos, refl, static_cast<uint32_t>(frame_scale_i(0x18)), 0x7f, 0x7f, 0x7f, 0x20, 4.0f);
    }
    shard_spawn(m, pos, refl, static_cast<uint32_t>(frame_scale_i(0x14)), 0x7f, 0x7f, 0, 0x30, 4.0f);
    shard_spawn(m, pos, refl, static_cast<uint32_t>(frame_scale_i(0x13)), 0xff, 0xff, 0xff, 0x20, 2.0f);
    // the camera shaken (harder the nearer), the burst's sound and effect, and gone
    sw(vel, 0);
    sb(m + 0xbc, 0);
    const uint32_t shake = OV(0x00166c80);
    if (cam_dist < 20.0f) swc1(shake + 0x160, fpu_sub(0.4f, fpu_mul(cam_dist, u2f(0x3c8f5c29u))));  // (0.0175)
    else swc1(shake + 0x160, u2f(0x3d4cccd0u));  // (0.05)
    sw(shake + 0x168, static_cast<uint32_t>(frame_scale_i(0x19)));
    moby_sound_start(0, 0, m);
    effect_spawn(effect, pos, 0, 0);
    leave();
}

}  // namespace rac1
