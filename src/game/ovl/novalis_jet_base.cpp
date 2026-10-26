// Novalis's own moby classes, part 2e: the aircraft base (the jets, the ship, their wingmen). Header
// novalis_mobys2.h. Addresses are Novalis's (`orig: L1:`); Novalis-only data is
// L1_ADDR, the globals the engine shares with Veldin are the level's own (OV), the base ELF's are bare.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/weapons.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// The bank of an aircraft: its +0x130 / +0x134 (the sideways and up sway scale, both 0: none) times the up / side
// axes of its rotation (from its angles), added to `out`.
// orig: L1:0x002f5040
void jet_banking_l1(uint32_t m, uint32_t out) {
    const uint32_t buf = ee_stack_alloc(0x100);
    const uint32_t pv = lwu(m + 0x78);
    const float f20 = u2f(0x3f800000u);
    sd(buf, 0);
    sd(buf + 8, 0);
    swc1(buf, f20);
    if (lwc1(pv + 0x130) == 0.0f && lwc1(pv + 0x134) == 0.0f) {
        ee_stack_free(0x100);
        return;
    }
    const uint32_t s0 = buf + 0x50, s2 = buf + 0x10, s1 = buf + 0x20, s3 = buf + 0x30;
    euler_to_mat4_vu(M(s0), V(m + 0x40));
    mat4_transform(V(s2), V(buf), M(s0));
    lq_sq(buf + 0x40, buf + 0x10);
    swc1(buf + 0x48, lwc1(buf + 0x48) - f20);
    vec3_cross_ba(V(s1), V(s2), V(buf + 0x40));
    vec3_cross_ba(V(s3), V(s2), V(s1));
    vec3_scale(V(s1), V(s1), lwc1(pv + 0x130));
    vec3_scale(V(s3), V(s3), lwc1(pv + 0x134));
    vec3_add(V(out), V(out), V(s1));
    vec3_add(V(out), V(out), V(s3));
    ee_stack_free(0x100);
}

// A smoke particle (kind 0xa, alloc_particle) at `pos` with the velocity `vel` (the vector at +0x20): the colour
// (0.425, 0.425, 0.225, 1), a random size 60900..90000 (+0xc), its life frame_scale(0x57) frames.
// orig: L1:0x0027f660
void smoke_particle_l1(uint32_t pos, uint32_t vel) {
    const uint32_t s1 = alloc_particle(0xa);
    if (s1 == 0) return;
    const uint32_t s0 = s1 + 0x20;
    lq_sq(s1 + 0x10, pos);
    const float f20 = u2f(0x3ed9999au), f21 = u2f(0x3e666666u), f22 = u2f(0x3f800000u);
    sw(s1 + 4, rgba_pack(f20, f20, f21, f22));
    const int32_t v0 = float_to_int(u2f(0x41000000u));
    sb(s1 + 1, 0);
    sb(s1 + 9, static_cast<uint32_t>(v0 - 0x60));
    sb(s1 + 3, 0x48);
    const float sz = rand_range_f(u2f(0x476de400u), u2f(0x47b05e00u));
    swc1(s1 + 0xc, sz);
    sb(s1 + 8, 0);
    sb(s1 + 2, lbu(lwu(OV(0x001b2084) + 8)));
    swc1(s0 + 0x14, f20);
    swc1(s0 + 0x18, f21);
    swc1(s0 + 0x1c, f22);
    swc1(s0 + 0x10, f20);
    sh(s1 + 0xa, static_cast<uint32_t>(frame_scale_i(0x57)));
    lq_sq(s0, vel);
}

// The debris of a downed jet (class 0x5e6): at the jet's position and angles, scaled by `scale`, its parent (pvar
// +0x24) the jet, flying along `dir` (a random 0.8..1.2 times its length), with a random spin (+0x10 about +-0.5,
// +0x14 about +-3, +0x18 about +-1 degrees a frame) and a life of frame_scale(200) frames (+0x20).
// orig: L1:0x0030be70
uint32_t jet_debris_spawn_l1(uint32_t m, uint32_t dir, float scale) {
    const uint32_t s1 = moby_spawn(0x5e6);
    if (s1 == 0) return s1;
    const uint32_t s0 = lwu(s1 + 0x78);
    sw(s0 + 0x24, m);
    lq_sq(s1 + 0x10, m + 0x10);
    const float f0 = lwc1(s1 + 0x2c) * scale;
    lq_sq(s1 + 0x40, m + 0x40);
    sb(s1 + 0x20, 0);
    sb(s1 + 0x31, 1);
    sb(s1 + 0x30, 0xff);
    sh(s1 + 0x32, 0xff);
    swc1(s1 + 0x2c, f0);
    const float len = rand_range_f(u2f(0x3f4ccccdu), u2f(0x3f99999au));
    vec3_set_length(V(s0), V(dir), len);
    const float z = 0.0f;
    swc1(s0 + 0x10, rand_range_signed_f(z, u2f(0x3c0efa35u)));
    swc1(s0 + 0x14, rand_range_signed_f(u2f(0x3d567750u), u2f(0x3e0efa35u)));
    swc1(s0 + 0x18, rand_range_signed_f(z, u2f(0x3c8efa35u)));
    sw(s0 + 0x20, static_cast<uint32_t>(frame_scale_i(200)));
    moby_update_transform(s1);
    return s1;
}

// The aircraft base update (the jets' common part; classes 0x33.., 0x336, 0x46a.. in the table at 0x20b410; the
// moby's pvar +0x60 is a path follower, +0xa0.. the spline state):
//  - the moby is "off" the screen when moby_on_screen says -1 (classes other than 0x336); its group (+0x21) is put to
//    state 3 (timer 5 frames in pvar +0x11c) when none of its members is on the screen and it is not at rest;
//  - the engine sounds (some classes) and the hit sounds, the timers;
//  - state 0: the path (follower +0x14) looked up, the closed loop trimmed, the nearest point, the first segment,
//    the start pose and the chain points (the wingmen) placed; then state 1 (or 3 when the group timer is set);
//    state 1: the flight along the Hermite spline of the path (speed pvar +0xfc, +0x124 / +0x128 the wobble, the
//    heading / bank / pitch turned damped toward the motion unless off the screen), the wingmen, the smoke, and in
//    levels 3 / 9 the hits (the explosion 0x273310); state 2: back to 0, sound stopped; state 3: waits its timer.
// orig: L1:0x002f5168
void jet_update_base_l1(uint32_t m) {
    const uint32_t buf = ee_stack_alloc(0xc0);
    const uint32_t pv = lwu(m + 0x78);
    uint32_t off = 0;  // s8: 1 when off the screen
    const uint32_t zero_v = buf + 0x30;
    sd(zero_v, 0);
    sd(zero_v + 8, 0);
    if (lh(m + 0xa6) != 0x336) {
        const float r = int_to_float(lh(m + 0x32));
        const int64_t on = moby_on_screen_l1(m, r);
        off = (~on) == 0 ? 1u : 0u;
    }
    const uint32_t s2 = pv + 0x60;
    auto done = [&] { ee_stack_free(0xc0); };
    if (pv == 0) {
        done();
        return;
    }
    if (lw(L1_ADDR(0x00161b00)) != 0) {
        done();
        return;
    }
    // ---- the group wake-up
    if (lbu(m + 0x21) != 0xff) {
        const uint32_t st0 = lbu(m + 0x20);
        if (st0 != 0 && st0 != 3) {
            const uint32_t out = buf + 0xb0;
            uint32_t cur = 0;
            int32_t s1 = 0;
            group_first_r(cur, static_cast<int32_t>(lbu(m + 0x21)), 0, 0);
            sw(out, cur);
            bool skip = false;
            if (lwu(out) != m) {
                skip = true;
            } else {
                for (;;) {
                    const uint32_t s0 = lwu(out);
                    if (s0 == 0) break;
                    const float f0 = int_to_float(lh(s0 + 0x32));
                    if (moby_on_screen_l1(s0, f0) == -1) {
                        uint32_t o = lwu(out);
                        group_next_r(o, lwu(out), 0, 0);
                        sw(out, o);
                        continue;
                    }
                    s1 = 1;
                    break;
                }
                if (s1 != 0) skip = true;
            }
            if (!skip) {
                uint32_t o = 0;
                group_first_r(o, static_cast<int32_t>(lbu(m + 0x21)), 0, 0);
                sw(out, o);
                if (lwu(out) != 0) {
                    do {
                        const uint32_t mem = lwu(out);
                        const uint32_t s0 = lwu(mem + 0x78);
                        sb(mem + 0x20, 3);
                        sh(s0 + 0x11c, static_cast<uint32_t>(frame_scale_i(5)));
                        uint32_t o2 = lwu(out);
                        group_next_r(o2, lwu(out), 0, 0);
                        sw(out, o2);
                    } while (lwu(out) != 0);
                }
            }
        }
    }
    // ---- the engine / hit sounds
    {
        const int32_t cls = lh(m + 0xa6);
        if (cls == 0x33 || cls == 0x46a || cls == 0x473 || cls == 0x474) {
            if (!((static_cast<uint32_t>(lw(OV(0x0015f5cc))) & 7u) != ((m >> 8) & 7u)) && !(lh(pv + 0x13a) > 0) && lbu(m + 0x20) != 0) {
                if (sub_0028d8c0(m, lh(pv + 0x11e)) == 0) sh(pv + 0x11e, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            }
        }
    }
    {
        const int32_t k = static_cast<int16_t>(static_cast<uint32_t>(lhu(m + 0xa6) - 0x33u) & 0xffffu);
        if (static_cast<uint32_t>(k) < 0x18u) {
            const bool tick = (k == 0 || k == 8 || k == 22);
            const bool tick2 = (k == 12 || k == 16);
            if (tick) {
                bool roll = true;
                if (lh(pv + 0x138) != -1) {
                    if (timer_tick_i16(ee_ref<int16_t>(pv + 0x138)) == 0) {
                        roll = false;
                    } else if (vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < u2f(0x41f00000u)) {
                        moby_sound_start(1, 0, m);
                    }
                }
                if (roll) {
                    const int32_t lo = lh(pv + 0x13c);
                    const int32_t r = rand_range(lo, lo + lh(pv + 0x13e));
                    sh(pv + 0x138, static_cast<uint32_t>(frame_scale_i(r)));
                }
            } else if (tick2) {
                bool roll = true;
                if (lh(pv + 0x138) != -1) {
                    if (timer_tick_i16(ee_ref<int16_t>(pv + 0x138)) == 0) {
                        roll = false;
                    } else {
                        const int32_t r = rand_mod(100);
                        moby_sound_start((r & 1) == 0 ? 2 : 1, 0, m);
                    }
                }
                if (roll) {
                    const int32_t lo = lh(pv + 0x13c);
                    const int32_t r = rand_range(lo, lo + lh(pv + 0x13e));
                    sh(pv + 0x138, static_cast<uint32_t>(frame_scale_i(r)));
                }
            }
        }
    }
    // ---- the states
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        const int32_t pn = lw(s2 + 0x14);
        if (pn != -1) {
            const uint32_t path = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(pn) << 2));
            const float d = vec3_distance(V(path + 0x10), V(path + (static_cast<uint32_t>(lw(path)) << 4)));
            if (d < u2f(0x3f000000u)) sw(path, static_cast<uint32_t>(lw(path) - 1));
        } else {
            debug_printf_stub();
        }
        sb(m + 0x30, 0xff);
        sh(pv + 0x11e, 0xffffu);
        const float f20 = u2f(0x43fa0000u);
        if (f20 < lwc1(pv + 0x130)) {
            const float r = rand_range_f(u2f(0xbf800000u), u2f(0x3f800000u));
            swc1(pv + 0x130, fpu_div(r * lwc1(pv + 0x130), u2f(0x447a0000u)));
        }
        if (f20 < lwc1(pv + 0x134)) {
            const float r = rand_range_f(u2f(0xbf800000u), u2f(0x3f800000u));
            swc1(pv + 0x134, fpu_div(r * lwc1(pv + 0x134), u2f(0x447a0000u)));
        }
        if (lw(s2 + 0x14) == -1) {
            sb(m + 0x20, 2);
            done();
            return;
        }
        const int32_t idx = static_cast<int32_t>(m - lwu(OV(0x0015ffd8)));
        const int32_t q = (((idx >> 8) + static_cast<int32_t>(static_cast<uint32_t>(idx) >> 31)) >> 1);
        const uint32_t pos = m + 0x10;
        sb(pv + 0x121, static_cast<uint32_t>(ee_rem(q, 7)));
        path_follower_init_l1(s2);
        if (lbu(s2 + 5) == 0) {
            int32_t a1 = lw(s2);
            if (!(a1 > 0)) {
                const int32_t a0 = path_nearest_l1(pos, lwu(s2 + 0x10), 0.0f);
                sw(s2, static_cast<uint32_t>(a0));
                if (static_cast<int8_t>(lbu(s2 + 4)) < 0) {
                    if (a0 == 0) sw(s2, 0);
                } else if (a0 == lw(lwu(s2 + 0x10)) - 1) {
                    sw(s2, 0);
                }
                a1 = lw(s2);
            }
            const uint32_t v0 = lwu(s2 + 0x10);
            const float d = vec3_distance(V(pos), V(v0 + (static_cast<uint32_t>(a1) << 4) + 0x10));
            swc1(s2 + 0xc, d);
            const uint32_t v1 = lwu(s2 + 0x10) + (lwu(s2) << 4);
            const float f1 = lwc1(v1 + 0x14), f0 = lwc1(v1 + 0x10);
            const float f12 = f0 - lwc1(m + 0x10), f13 = f1 - lwc1(m + 0x14);
            swc1(s2 + 0x18, atan2_f(f12, f13));
            sb(s2 + 5, 2);
            sw(s2 + 8, 0);
            sw(s2 + 0x1c, 0);
        }
        {
            const uint32_t s0 = pv + 0xa0;
            lq_sq(pv + 0xe0, lwu(s2 + 0x10) + (lwu(s2) << 4) + 0x10);
            swc1(pv + 0xf0, u2f(0x3f800000u));
            path_spline_tangent_l1(s0, s2, 0, lwc1(pv + 0xf4), lwc1(pv + 0xf8));
            const uint32_t v1 = lwu(s2) << 4, a1b = lwu(s2 + 0x10);
            lq_sq(pv + 0xb0, v1 + a1b + 0x10);
            lq_sq(pv + 0xc0, v1 + a1b + 0x10);
            sw(m + 0x40, 0);
            const float len = vec2_length(V(s0));
            const float pitch = atan2_f(len, lwc1(pv + 0xa8));
            swc1(m + 0x44, -pitch);
            const float yaw = atan2_f(lwc1(pv + 0xa0) - lwc1(zero_v), lwc1(pv + 0xa4) - lwc1(zero_v + 4));
            swc1(m + 0x48, yaw);
            sw(pv + 0x104, 0);
            sw(pv + 0x10c, 0);
            lq_sq(m + 0x10, pv + 0xe0);
            jet_banking_l1(m, pos);
            const float f2 = lwc1(pv + 0x12c);
            sb(pv + 0x121, 7);
            float f0 = 0.0f;
            bool set = true;
            if (u2f(0x40c90fdbu) < f2) {
                f0 = f2 * u2f(0x3c8efa35u);
            } else {
                const float f1b = lwc1(pv + 0x124);
                if (f1b == 0.0f) {
                    set = false;
                } else if (f2 != u2f(0xbf800000u)) {
                    set = false;
                } else {
                    f0 = rand_angle();
                }
            }
            if (set) swc1(pv + 0x12c, f0);
            lq_sq(pv + 0xd0, pv + 0xc0);
            path_measure_l1(s2, lbu(pv + 0x120) & 0x20u, m, lwc1(pv + 0xf4), lwc1(pv + 0xf8));
        }
        const uint32_t fl = lbu(pv + 0x120);
        if ((fl & 0x1e) != 0 && (fl & 1) == 0) {
            const uint32_t pts = buf + 0x60, dirs = buf + 0xa0;
            uint32_t s2l = pts, s6 = pv + 0x140;
            for (int32_t s5 = 0; s5 < 4; ++s5, s2l += 0x10, s6 += 4) {
                if ((lbu(pv + 0x120) & (2u << s5)) == 0) continue;
                if (lw(s6) == -1) continue;
                const uint32_t s1 = lwu(OV(0x0015ffd8)) + (lwu(s6) << 8);
                if (s1 == 0) continue;
                const uint32_t s0 = lwu(s1 + 0x78);
                if (s0 == 0) continue;
                sw(s0 + 0x70, lwu(s0 + 0x70) | 0x1000u);
                moby_chain_point_v(m, static_cast<uint32_t>(s5), V(s2l));
                vec3_sub(V(dirs), V(s2l), V(pos));
                vec3_set_length(V(dirs), V(dirs), 1.0f);
                lq_sq(s1 + 0x10, s2l);
                lq_sq(s0 + 0x80, dirs);
            }
        }
        if (lh(pv + 0x11c) > 0) {
            sb(m + 0x20, 3);
            done();
            return;
        }
        const int32_t level = lw(0x0015ed84);
        if (level == 3 || level == 9) sh(pv + 0x13a, 0);
        sb(m + 0x20, 1);
        done();
        return;
    }
    if (st == 2) {
        if (lw(s2 + 0x14) == -1) {
            done();
            return;
        }
        sb(m + 0x20, 0);
        const int32_t a1 = lh(pv + 0x11e);
        moby_sound_release_l1(m, a1);
        sh(pv + 0x11e, 0xffffu);
        done();
        return;
    }
    if (st == 3) {
        if (timer_tick_i16(ee_ref<int16_t>(pv + 0x11c)) == 0) {
            done();
            return;
        }
        sh(pv + 0x11c, 0);
        sb(m + 0x20, 1);
        done();
        return;
    }
    if (st != 1) {
        done();
        return;
    }
    // ---- state 1: the flight
    lq_sq(buf + 0x40, m + 0x40);
    lq_sq(buf + 0x50, m + 0x10);
    const uint32_t s1 = pv + 0xa0;
    if (u2f(0x3f800000u) <= lwc1(pv + 0xf0)) {
        swc1(pv + 0xf0, lwc1(pv + 0xf0) - u2f(0x3f800000u));
        const int32_t s0 = path_index_advance_l1(s2);
        lq_sq(pv + 0x90, pv + 0xa0);
        path_spline_tangent_l1(s1, s2, 1, lwc1(pv + 0xf4), lwc1(pv + 0xf8));
        lq_sq(pv + 0xb0, pv + 0xc0);
        const int32_t nx = path_index_wrap_l1(s2, s0 + 1);
        lq_sq(pv + 0xc0, lwu(s2 + 0x10) + (static_cast<uint32_t>(nx) << 4) + 0x10);
    }
    const uint32_t s0 = pv + 0xd0, s7 = pv + 0xb0, s6 = pv + 0xc0, s5 = pv + 0x90;
    const float sinv = sin_vu(lwc1(pv + 0x12c));
    const float f2 = lwc1(0x0015ed60);
    float f1 = lwc1(pv + 0x124) * f2;
    const float f5 = lwc1(pv + 0xfc);
    float f13 = lwc1(pv + 0x128);
    const float f4 = lwc1(pv + 0xbc);
    f13 = f13 * f2;
    const float f3 = u2f(0x3c8efa35u);
    f1 = f1 * sinv;
    f13 = f13 * f3;
    f1 = f1 * f2;
    const float f20 = f5 + f1;
    float f24 = fpu_div(f20, f4);
    swc1(pv + 0x12c, angle_add(lwc1(pv + 0x12c), f13));
    lq_sq(pv + 0xe0, pv + 0xd0);
    hermite_l1(s0, s7, s6, s5, s1, fpu_add(lwc1(pv + 0xf0), f24));
    if ((lbu(pv + 0x120) & 0x40u) == 0) {
        const float d = vec3_distance(V(pv + 0xe0), V(s0));
        if (d != 0.0f) {
            const float f0 = fpu_div(f20, d);
            f24 = fpu_mul(f24, f0);
            hermite_l1(s0, s7, s6, s5, s1, fpu_add(lwc1(pv + 0xf0), f24));
        }
    }
    if (off == 0) {
        const float f21 = u2f(0x3c8efa35u);
        float f20b = static_cast<float>(lh(pv + 0x122)) * f21;
        const float f22 = u2f(0x41200000u), f23 = u2f(0x40a00000u);
        (void)angle_sub(lwc1(m + 0x48), f20b);
        const float dx = lwc1(pv + 0xd0) - lwc1(pv + 0xe0), dy = lwc1(pv + 0xd4) - lwc1(pv + 0xe4);
        float hd = atan2_f(dx, dy);
        hd = angle_add(hd, f20b);
        hd = angle_sub(hd, lwc1(m + 0x48));
        const float f2b = lwc1(pv + 0x110);
        float k = fpu_div(u2f(0x3f800000u), f2b);
        k = k * lwc1(0x0015ed60);
        f20b = hd * k;
        swc1(m + 0x48, angle_add(f20b, lwc1(m + 0x48)));
        vec3_sub(V(buf + 0x20), V(s0), V(pv + 0xe0));
        const float len = vec2_length(V(buf + 0x20));
        const float ang = atan2_f(len, lwc1(buf + 0x28));
        float f16 = lwc1(pv + 0x108);
        const float tgt = -ang;
        const float f1c = lwc1(0x0015ed70), f0c = lwc1(0x0015ed6c);
        float f14 = f16 * f22;
        float f15 = f16 * f23;
        f16 = f16 * f21;
        f14 = f14 * f21;
        f15 = f15 * f21;
        f16 = f16 * f0c;
        f14 = f14 * f1c;
        swc1(m + 0x44, angle_seek_rate(pv + 0x10c, lwc1(m + 0x44), tgt, f14, f15 * f1c, f16));
        float w = angle_wrap_mod(f20b * lwc1(pv + 0x114));
        w = angle_sub(lwc1(m + 0x40), w);
        const float w2 = angle_wrap_mod(w * lwc1(pv + 0x118));
        f16 = lwc1(pv + 0x100);
        const float f15b = lwc1(0x0015ed70);
        float f22b = f16 * f22;
        float f23b = f16 * f23;
        const float f0d = lwc1(0x0015ed6c);
        f16 = f16 * f21;
        f22b = f22b * f21;
        f23b = f23b * f21;
        f16 = f16 * f0d;
        const float f14b = f22b * f15b;
        swc1(m + 0x40, angle_seek_rate(pv + 0x104, lwc1(m + 0x40), w2, f14b, f23b * f15b, f16));
    } else {
        vec_clear(V(m + 0x40));
        swc1(m + 0x48, atan2_f(lwc1(pv + 0xd0) - lwc1(pv + 0xe0), lwc1(pv + 0xd4) - lwc1(pv + 0xe4)));
    }
    swc1(pv + 0xf0, fpu_add(lwc1(pv + 0xf0), f24));
    lq_sq(m + 0x10, pv + 0xe0);
    if (off == 0) jet_banking_l1(m, m + 0x10);
    if ((lbu(pv + 0x120) & 0x1fu) != 0) {
        const float f21 = u2f(0x3c83126fu), f20c = u2f(0x3cfdf3b6u);
        int32_t s7i = 0;
        uint32_t s6i = static_cast<uint32_t>(-4);
        for (int32_t s5i = 0; s5i < 5; ++s5i, s6i += 4) {
            if (((lbu(pv + 0x120) >> s5i) & 1u) == 0) continue;
            const uint32_t sp60 = buf + 0x60, sp70 = buf + 0x70;
            moby_chain_point_v(m, static_cast<uint32_t>(s7i), V(sp60));
            ++s7i;
            vec3_sub(V(sp70), V(sp60), V(m + 0x10));
            if ((lbu(pv + 0x120) & 1u) == 0) {
                const int32_t v0 = lw(pv + s6i + 0x140);
                if (v0 == -1) continue;
                const uint32_t s1c = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(v0) << 8);
                if (s1c == 0) continue;
                const uint32_t s0c = lwu(s1c + 0x78);
                if (s0c == 0) continue;
                vec3_set_length(V(sp70), V(sp70), u2f(0x3f800000u));
                sw(s0c + 0x70, lwu(s0c + 0x70) | 0x1000u);
                lq_sq(s1c + 0x10, sp60);
                lq_sq(s0c + 0x80, sp70);
            } else {
                if (lbu(pv + 0x121) != 0) continue;
                float r = rand_range_f(f21, f20c);
                vec3_set_length(V(sp70), V(sp70), r * lwc1(0x0015ed60));
                smoke_particle_l1(sp60, sp70);
                r = rand_range_f(f21, f20c);
                vec3_set_length(V(sp70), V(sp70), r * lwc1(0x0015ed60));
                smoke_particle_l1(sp60, sp70);
            }
        }
        if (lbu(pv + 0x121) == 0) sb(pv + 0x121, static_cast<uint32_t>(frame_scale_i(7)));
        sb(pv + 0x121, lbu(pv + 0x121) - 1u);
    }
    const int32_t level = lw(0x0015ed84);
    if (level == 3 || level == 9) {
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x210000), 0);
        const bool boom = hit != 0 && lhu(hit + 0x28) != 0x102;  // (a hit of kind 0x102: no explosion)
        if (boom) {
            const uint32_t sp60 = buf + 0x60;
            vec_clear(V(sp60));
            if (lh(m + 0xa6) == 0x33) {
                const float f20d = u2f(0xc02fced9u);
                swc1(sp60, cos_vu(lwc1(m + 0x48)) * f20d);
                const float sn = sin_vu(lwc1(m + 0x48));
                sw(buf + 0x68, 0);
                swc1(sp60 + 4, sn * f20d);
            }
            const uint32_t s1d = m + 0x10;
            vec3_sub(V(buf + 0x80), V(s1d), V(buf + 0x50));
            const uint32_t s0e = buf + 0x70;
            lq_sq(s0e, buf + 0x80);
            vec3_scale(V(s0e), V(s0e), u2f(0x3f000000u));
            vec3_add(V(sp60), V(sp60), V(s1d));
            sh(pv + 0x13a, static_cast<uint32_t>(frame_scale_i(0x258)));
            const int32_t cls = lh(m + 0xa6);
            const int32_t t2 = (cls == 0x532 || cls == 0x53b || cls == 0x53c) ? 1 : -1;
            sw(buf, 1);
            sw(buf + 8, 0xffffffffu);
            sw(buf + 0x10, 0);
            explosion_effect(m, s0e, sp60, 0x14, 3, 4, t2, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 100000.0f, 3.0f, 15.0f);  // jal L1:0x273310 (Veldin 0x25e450): the stack words 1 / -1 / 0, f12..f18 as loaded before the call
        }
    }
    if (!(level == 3 || level == 9)) {
        done();
        return;
    }
    sw(OV(0x00160770), L1_ADDR(0x002f5168));
    sb(m + 0xa4, 0xff);
    if (lh(pv + 0x13a) == 0) {
        done();
        return;
    }
    const int32_t tk = timer_tick_i16(ee_ref<int16_t>(pv + 0x13a));
    if (tk != 0 && off != 0) {
        const uint32_t v1 = lwu(m + 0x24);
        sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
        sw(m + 0x94, lwu(v1 + 0x10));
        done();
        return;
    }
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    moby_sound_release_l1(m, lh(pv + 0x11e));
    sh(pv + 0x11e, 0xffffu);
    sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
    done();
}

}  // namespace rac1
