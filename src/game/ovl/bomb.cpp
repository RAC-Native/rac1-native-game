// Ported overlay code: the bomb glove's bomb (class 0x79). Its pvars: +0 the velocity, +0x10 the
// position it is headed for, +0x20 the surface normal it hit, +0x30 / +0x40 the moby it stuck to
// (its position, the bomb's offset from it), +0x50 the glove moby, +0x54 the frames left,
// +0x56 the glove's lock, +0x60 the moby it stuck to, +0x64 the frames stuck there.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
constexpr OvConst<0x00173e40u> HIT{};  // the collision's result: +0x18 the moby hit, +0x1c the face, +0x20 the point, +0x40 the normal
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }
}  // namespace

// Where the bomb `m` (pvars `pv`) lands: from the glove (in the hand) or where it is, the arc
// stepped 10 frames at a time under gravity until it hits something (the level, or a moby to
// stick to) or its frames run out; `mode` (0: only a moby hit counts, 2: no sound on a moby
// with pvars word 2). The landing point is pulled 5% back toward the camera (0x166dc0) and the
// surface normal kept; 0 when nothing was hit in time (or the hero is in state 0x72 / a
// cutscene runs).
// orig: 0x002ad410
int32_t bomb_trace(uint32_t pv, uint32_t m, int32_t mode) {
    bool hit = false;
    int32_t stuck = 0;
    if (lw(P + 0x2084) == 0x72) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    Vec4 pos, vel, from, prev;
    prev = V(m + 0x10);
    int32_t frames;
    if (lbu(m + 0x20) == 0) {
        const float r = u2f(0x3f5be9fbu);
        pos.x = cos_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * r;
        pos.y = sin_vu(angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)))) * r;
        pos.z = 0.0f;
        pos.w = 0.0f;  // (a stack leftover in the original; only xyz are used)
        vec3_add(pos, pos, V(P + 0x80));
        pos.z = pos.z + u2f(0x3efb4cc2u);
        vel = V(pv);
        if (lw(P + 0x208c) == 0xf) vec3_add(vel, vel, V(P + 0x100));
        if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0 && mode != 0) vec3_add(vel, vel, V(P + 0x100));
        frames = frame_scale_i(0x12c);
    } else {
        const uint32_t gpv = lwu(lwu(pv + 0x50) + 0x78);
        pos = V(m + 0x10);
        vel = V(pv);
        frames = lh(pv + 0x54);
        if (lw(gpv + 0x50) != 0) sh(pv + 0x56, 1);
    }
    // stick to the moby hit (its offset kept from the second frame on)
    auto stick = [&](uint32_t o) {
        Vec4 d{0.0f, 0.0f, 0.5f, 0.0f};
        if (lwu(pv + 0x60) != o) {
            sw(pv + 0x64, 0);
            sw(pv + 0x60, o);
        } else {
            sw(pv + 0x64, lwu(pv + 0x64) + 1);
            Vec4 t;
            vec3_sub(t, V(o + 0x10), V(pv + 0x30));
            V(pv + 0x40) = t;
        }
        V(pv + 0x30) = V(o + 0x10);
        stuck = 1;
        V(pv + 0x10) = V(o + 0x10);
        hit = true;
        vec3_add(d, d, V(pv + 0x10));
        floor_probe_v(0.5f, d, 0);
    };
    // too soon to hit the hero / the glove again
    auto too_soon = [&](uint32_t o) {
        if (o != lwu(P + 0x2080) && o != lwu(pv + 0x50)) return false;
        const int32_t a = frame_scale_i(0x12c);
        const int32_t b = frame_scale_i(0xa);
        return !(frames < a - b);
    };
    from = V(lwu(P + 0x2080) + 0x10);
    const float z0 = pos.z;
    from.z = z0;
    if (segment_collide_v(from, pos, 0x10, lwu(P + 0x2080), nullptr) != 0 && (sub_001f35d8() != 0 || 0.0f < vel.z)) {
        const uint32_t o = lwu(HIT + 0x18);
        if (0 < lw(HIT + 0x1c)) {
            V(pv + 0x10) = V(OV(0x00173e60));
            hit = true;
        } else if (o != 0 && !too_soon(o)) {
            stick(o);
        }
    }
    if (lbu(0x0013e52a) != 0 && lw(P + 0x2084) == 1) {
        const float vz = vel.z;
        vec3_scale(vel, vel, 1.5f);
        vel.z = fpu_div(vz, 1.5f);
    }
    for (;;) {
        if (timer_tick_i32(frames) != 0 || hit) break;
        prev = pos;
        for (int32_t i = 9; i >= 0; --i) {
            vec3_add(pos, pos, vel);
            vel.z = vel.z - lwc1(0x0015ed70) * 11.0f;
        }
        if (segment_collide_v(prev, pos, 0x10, m, nullptr) == 0) continue;
        if (sub_001f35d8() == 0 && 0.0f < vel.z) continue;
        const uint32_t o = lwu(HIT + 0x18);
        if (o != 0 && !(0 < lw(HIT + 0x1c))) {
            if (pos.z - z0 < 2.0f) {
                if (too_soon(o)) continue;
                stick(o);
                break;
            }
        }
        if (!(0 < lw(HIT + 0x1c))) continue;
        V(pv + 0x10) = V(OV(0x00173e60));
        break;
    }
    if (frames == 0) return 0;
    if (mode == 0) {
        if (lwu(OV(0x00173e58)) == 0) return 0;
        if (moby_pvar_word2(lwu(OV(0x00173e58))) == 0) return 0;
    }
    if (!(mode == 2 && lwu(OV(0x00173e58)) != 0 && moby_pvar_word2(lwu(OV(0x00173e58))) != 0)) sub_001f93e0(OVF(0x002acbf0), m);
    if (stuck == 0) {
        const uint32_t o = lwu(pv + 0x60);
        if (o != 0) {
            const uint32_t st = lbu(o + 0x20);
            if (st == 0xfe || st == 0xfd || (lhu(o + 0x34) & 0x1000) == 0 || frame_scale_i(0x1e) < lw(pv + 0x64)) {
                sw(pv + 0x64, 0);
                sw(pv + 0x60, 0);
            }
        }
    }
    V(pv + 0x20) = V(OV(0x00173e80));
    Vec4 d;
    vec3_sub(d, V(pv + 0x10), V(OV(0x00166dc0)));
    vec3_scale(d, d, u2f(0x3f733333u));
    vec3_add(V(pv + 0x10), d, V(OV(0x00166dc0)));
    return 1;
}

namespace {
// orig: (helper) repeated in 0x002adb30 — a spray direction: a random one in the plane across
// `up` plus up to `lift` along it, at `lo` .. `hi` times `k` (times the frame scale), plus the
// push `push` (frame +0x80); the frame's +0x100 / +0xf0 / +0xa0 hold the steps.
void spray_dir(uint32_t f, uint32_t up, float lo, float hi, float k) {
    const uint32_t d = f + 0xf0, t = f + 0xa0;
    V(f + 0x100) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
    swc1(f + 0x100, rand_range_f(-1.0f, 1.0f));
    swc1(f + 0x104, rand_range_f(-1.0f, 1.0f));
    V(d) = V(f + 0x100);
    vec3_set_length(V(t), V(up), vec3_dot(V(d), V(up)));
    vec3_sub(V(d), V(d), V(t));
    vec3_set_length(V(t), V(up), rand_range_f(0.0f, 1.0f));
    vec3_add(V(d), V(d), V(t));
    const float r = k * rand_range_f(lo, hi);
    vec3_set_length(V(d), V(d), r * lwc1(0x0015ed6c));
    vec3_add(V(d), V(d), V(f + 0x80));
}
}  // namespace

// The bomb's update. Held (state 0) its glow pulses; thrown (state 1) it flies under gravity
// with a fuse of sparks (the table at 0x1613f0), bounces once off the ground (a splash in the
// height regions, a dust ring) and otherwise explodes on what it hits or when its frames
// (+0x54) run out: it hits the mobys around (a radius growing with the glove's upgrade
// 0x13e52a), throws debris and sparks, flashes, smokes over water, shakes the camera by the
// distance and leaves an effect (the description at 0x1e98d0); then (state 2) its blast grows
// for 15 frames before it is retired. Out of the hand when the hero cannot hold it.
// orig: 0x002adb30
void bomb_update(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x290);  // the original's frame (its records go by address)
    auto done = [&] { ee_stack_free(0x290); };
    for (uint32_t i = 0; i < 0x50; i += 16) lq_sq(f + 0x10 + i, OV(0x001e98d0) + i);
    const float fs = lwc1(0x0015ed6c);
    V(f + 0x80) = Vec4{0.0f, 0.0f, fs + fs, 0.0f};
    V(f + 0x90) = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t s8 = m + 0x10;
    {
        const uint32_t h = lwu(P + 0x1090);
        if (!(h != 0 && lh(h + 0xa6) == 0xc0) && lbu(m + 0x20) == 0) {
            moby_retire(m);
            done();
            return;
        }
        if (lbu(P + 0x20a4) != 0 && lbu(m + 0x20) == 0) {
            moby_retire(m);
            done();
            return;
        }
        bool show;
        if (lbu(P + 0x20a5) != 0) show = lbu(m + 0x20) == 0;
        else if (lbu(P + 0x20af) == 0) show = false;
        else show = lbu(m + 0x20) == 0;
        sh(m + 0x34, show ? (lhu(m + 0x34) | 0x41u) : (lhu(m + 0x34) & 0xffbeu));
    }
    if (lh(lwu(pv + 0x50) + 0xa6) == 0xc0 && lh(pv + 0x56) == 0 && 0.0f < vec3_length(V(pv))) {
        if (lwu(P + 0x2fc) != 0 && moby_pvar_word2(lwu(P + 0x2fc)) != 0) {
            if (bomb_trace(pv, m, 0) == 0) bomb_trace(pv, m, 2);
        } else {
            bomb_trace(pv, m, 1);
        }
    }
    auto blast = [&] {  // state 2: the blast grows, then the bomb goes
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        const float big = (int_to_float(lbu(0x0013e52a)) + 1.0f) * 3.0f - 0.5f;
        const float t = fpu_div(int_to_float(lbu(m + 0xbc)), static_cast<float>(frame_scale_i(0xf)));
        const float r = lerp_f(0.5f, big, t);
        const int32_t n = mobys_in_sphere(s8, r, 0x10, m, 0);
        V(f + 0xb0) = V(m + 0x10);
        moby_list_hit(m, f + 0xb0, OV(0x00177f00), n, 0, 0x00830000, 2, 1, 2.0f, 1.0f, 1.0f);
        const uint32_t k = (lbu(m + 0xbc) + 1u) & 0xffu;
        sb(m + 0xbc, k);
        if (frame_scale_i(0xf) < static_cast<int32_t>(k)) moby_retire(m);
        done();
    };
    if (lbu(m + 0x20) != 1) {
        if (lbu(m + 0x20) == 2) {
            blast();
            return;
        }
        const uint32_t k = lhu(pv + 0x6a) + 1u;
        sh(pv + 0x6a, k);
        const float a = angle_wrap_mod(static_cast<float>(static_cast<int16_t>(k)) * u2f(0x3d8b4396u));
        const uint32_t v = static_cast<uint32_t>(float_to_int(sin_vu(a) * 96.0f) + 0x9f);
        sw(m + 0x90, (v << 16) | (v << 8) | 0xff000000u | v);
        if (lbu(m + 0x20) == 2) {
            blast();
            return;
        }
        done();
        return;
    }

    // --- thrown: the fuse fades to white, sparks, flight
    V(f + 0xb0) = Vec4{128.0f, 128.0f, 128.0f, 255.0f};
    timer_tick_i16(h16(pv + 0x6a));
    const float fuse = fpu_div(int_to_float(lh(pv + 0x6a)), int_to_float(frame_scale_i(0x1e)));
    const int32_t fr = float_to_int((lwc1(f + 0xb0) - 255.0f) * fuse + 255.0f);
    const int32_t fg = float_to_int((lwc1(f + 0xb4) - 64.0f) * fuse + 64.0f);
    const int32_t fb = float_to_int((lwc1(f + 0xb8) - 64.0f) * fuse + 64.0f);
    sw(m + 0x90, (static_cast<uint32_t>(fb) << 16) | (static_cast<uint32_t>(fg) << 8) | 0xff000000u | static_cast<uint32_t>(fr));
    if (lh(pv + 0x68) == 0 && (lw(OV(0x0015f5cc)) & 7) == 0 && 0 < lw(OV(0x0016142c))) {
        for (int32_t i = 0; i < lw(OV(0x0016142c));) {
            ++i;
            const uint32_t a = f + 0xc0, b = f + 0xd0;
            vec3_scale(V(a), V(pv), lwc1(OV(0x001613f4)));
            rand_sphere_point(f + 0xe0, 0.0f, lwc1(OV(0x001613f8)) * lwc1(0x0015ed6c));
            vec3_add(V(a), V(f + 0xe0), V(a));
            vec_clear(V(b));
            swc1(a + 0xc, rand_range_f(lwc1(OV(0x0016140c)), lwc1(OV(0x00161410))));
            swc1(b + 0xc, rand_range_f(lwc1(OV(0x00161414)), lwc1(OV(0x00161418))));
            const uint32_t c0 = rgba_lerp(lwu(OV(0x0016141c)), lwu(OV(0x00161420)), rand_range_f(0.0f, 1.0f));
            const uint32_t c1 = rgba_lerp(lwu(OV(0x00161424)), lwu(OV(0x00161428)), rand_range_f(0.0f, 1.0f));
            float r = rand_range_f(0.0f, 1.0f);
            const int32_t n = float_to_int(frame_scale_f(static_cast<float>(lw(OV(0x001613fc))) * r + 1.0f));
            r = rand_range_f(-lwc1(OV(0x00161408)), lwc1(OV(0x00161408)));
            const int32_t h32 = float_to_int(frame_scale_f(static_cast<float>(lw(OV(0x00161400))) * (r + 1.0f)));
            r = rand_range_f(-lwc1(OV(0x00161408)), lwc1(OV(0x00161408)));
            const int32_t h34 = float_to_int(frame_scale_f(static_cast<float>(lw(OV(0x00161404))) * (r + 1.0f)));
            const float dz = lwc1(OV(0x001613f0)) * lwc1(0x0015ed70) * static_cast<float>(h32);
            swc1(b + 8, lwc1(b + 8) - dz);
            spawn_particle_02(s8, a, b, c0, c1, static_cast<uint32_t>(n), static_cast<uint32_t>(h32), static_cast<uint32_t>(h34),
                              lw(OV(0x00161430)));
        }
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(0x0015ed6c) * u2f(0x40060a92u)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(0x0015ed6c) * u2f(0x40fb53d1u)));
    V(f + 0x70) = V(s8);
    vec3_add(V(s8), V(s8), V(pv));
    if (lh(pv + 0x68) == 0) {
        swc1(pv + 8, lwc1(pv + 8) - lwc1(0x0015ed70) * 11.0f);
    } else {
        swc1(pv + 8, lwc1(pv + 8) - fpu_div(lwc1(0x0015ed70) * 11.0f, 10.0f));
        if (rand_mod(frame_scale_i(6) - 1) == 0) {  // dust where it rolls
            const float h = surface_height(s8, f + 0xc0);
            vec3_scale(V(f + 0xc0), V(pv), 0.25f);
            const float r = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu));
            spawn_particle_22(s8, f + 0xc0, r * 210000.0f, h);
        }
    }
    // the flight's collision record (frame +0xc0): along the velocity, this moby, its class
    sw(f + 0xd0, m);
    sw(f + 0xd4, 0x00830000);
    if (lh(pv + 0x54) < frame_scale_i(0x12c) - frame_scale_i(0xa)) sw(f + 0xd4, lwu(f + 0xd4) | 1u);
    sw(f + 0xe0, 1);
    swc1(f + 0xdc, 2.0f);
    V(f + 0xc0) = V(pv);
    vec2_set_length(V(f + 0xc0), V(f + 0xc0), 1.0f);
    swc1(f + 0xc8, 1.0f);
    swc1(f + 0xcc, u2f(0x45afdf66u));
    sb(f + 0xd8, 2);
    sh(f + 0xda, lhu(m + 0xa6));
    sb(f + 0xd9, 1);
    if (lh(pv + 0x68) != 0) sh(pv + 0x68, lhu(pv + 0x68) + 1u);
    if (segment_collide(f + 0x70, s8, 0x10, m, f + 0xc0) == 0) {
        if (timer_tick_i16(h16(pv + 0x54)) != 0) {
            sb(m + 0xbc, 1);
            vec3_add(V(f + 0x80), V(f + 0x80), V(pv));
        }
    } else {
        bool check = true;  // look at what it hit
        if (lh(pv + 0x68) == 0) {
            if (sub_001f35d8() == 0 && lwc1(pv + 8) < 0.0f) {
                // the first bounce: a splash, a dust ring
                region_height_splash(lwu(OV(0x001612d0)), lw(OV(0x001612d8)), 1, lwc1(m + 0x10), lwc1(m + 0x14), 0.5f, u2f(0xbeb33333u));
                sh(pv + 0x68, 1);
                vec_clear(V(pv));
                swc1(pv + 8, lwc1(0x0015ed6c) * -1.5f);
                swc1(f + 0xf0, lwc1(m + 0x10));
                swc1(f + 0xf4, lwc1(m + 0x14));
                swc1(f + 0xf8, lwc1(OV(0x00173e68)));
                const uint32_t o = spawn_moby_307(f + 0xf0, 2.0f);
                if (o != 0) sb(o + 0x23, 0x70);
                for (int32_t i = 0xf; i >= 0; --i) {
                    const float a = rand_angle();
                    const float r = rand_range_f(lwc1(0x0015ed6c) * 0.0f, lwc1(0x0015ed6c) * 3.0f);
                    swc1(f + 0x100, cos_vu(a) * r);
                    swc1(f + 0x104, sin_vu(a) * r);
                    swc1(f + 0x108, rand_range_f(lwc1(0x0015ed6c) * 3.0f, lwc1(0x0015ed6c) * 6.5f));
                    const int32_t k = rand_range(0x5a, 0x78);
                    spawn_particle_23(f + 0xf0, f + 0x100, rand_mod(2), static_cast<uint32_t>(k));
                }
                check = false;
            } else {
                check = sub_001f35d8() != 0;
            }
        }
        if (check) {
            const uint32_t o = lwu(HIT + 0x18);
            bool boom = false;
            if (o == 0) {
                boom = 0 < lw(HIT + 0x1c);
            } else if ((o == lwu(P + 0x2080) || o == lwu(pv + 0x50)) && !(lh(pv + 0x54) < frame_scale_i(0x12c) - frame_scale_i(0xa))) {
                boom = false;
            } else {
                boom = lwu(o + 0x74) != lwu(OV(0x00160770)) || lh(o + 0xa6) == 0x336;
            }
            if (boom) {
                sb(m + 0xbc, 1);
                V(s8) = V(HIT + 0x20);
                vec3_reflect(V(f + 0x80), V(pv), V(HIT + 0x40));
                vec3_set_length(V(f + 0x80), V(f + 0x80), lwc1(0x0015ed6c) + lwc1(0x0015ed6c));
                vec3_set_length(V(f + 0x90), V(HIT + 0x40), 1.0f);
            }
        }
    }
    if (lbu(m + 0xbc) == 0) {
        if (lbu(m + 0x20) == 2) {
            blast();
            return;
        }
        done();
        return;
    }

    // --- the explosion
    const float big = int_to_float(lbu(0x0013e52a)) + 1.0f;
    vec3_scale(V(f + 0x80), V(f + 0x80), big);
    vec3_sub(V(f + 0x60), V(OV(0x00166dc0)), V(s8));
    const float dist = vec3_length(V(f + 0x60));
    if (lh(pv + 0x68) < frame_scale_i(0x14)) {
        const int32_t n = mobys_in_sphere(s8, big * 0.5f, 0x10, m, 0);
        V(f + 0xf0) = V(m + 0x10);
        moby_list_hit(m, f + 0xf0, OV(0x00177f00), n, 0, 0x00830000, 2, 1, 2.0f, 1.0f, 1.0f);
        sb(m + 0x20, 2);
    }
    for (int32_t i = 9; i >= 0; --i) {
        spray_dir(f, f + 0x90, 3.5f, 6.5f, big);
        const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x78));
        debris_7a_spawn(s8, f + 0xf0, static_cast<uint32_t>(life), 0, lbu(0x0013e52a));
    }
    if (lh(pv + 0x68) < frame_scale_i(0x14)) {
        for (int32_t i = 3; i >= 0; --i) {
            spray_dir(f, f + 0x90, 6.5f, 10.0f, big);
            const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x5a));
            debris_7a_spawn(s8, f + 0xf0, static_cast<uint32_t>(life), 1, lbu(0x0013e52a));
        }
        // one toward the camera
        V(f + 0x100) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
        swc1(f + 0x100, rand_range_f(-1.0f, 1.0f));
        swc1(f + 0x104, rand_range_f(-1.0f, 1.0f));
        swc1(f + 0x108, rand_range_f(-1.0f, 1.0f));
        swc1(f + 0x68, lwc1(f + 0x68) + dist * 0.5f);
        V(f + 0xf0) = V(f + 0x100);
        vec3_set_length(V(f + 0xf0), V(f + 0xf0), fpu_div(dist, 5.0f) * lwc1(0x0015ed6c));
        vec3_set_length(V(f + 0x60), V(f + 0x60), (dist + dist) * lwc1(0x0015ed6c));
        vec3_add(V(f + 0xf0), V(f + 0xf0), V(f + 0x60));
        vec3_clamp_length_vu(V(f + 0xf0), V(f + 0xf0), lwc1(0x0015ed6c) * 10.0f);
        const int32_t life = rand_range(frame_scale_i(0x3c), frame_scale_i(0x5a));
        debris_7a_spawn(s8, f + 0xf0, static_cast<uint32_t>(life), 1, lbu(0x0013e52a));
        // sparks, more the nearer the camera
        int32_t n = 4;
        if (dist < 6.0f) n = float_to_int(dist) + 1;
        const float close = dist < 7.0f ? 7.0f - dist : 0.0f;
        for (int32_t i = n; i > 0; --i) {
            float speed = big * rand_range_f(8.0f, 10.0f);
            for (uint32_t k = 0; k < 0x18; k += 4) sw(f + 0x110 + k, lwu(OV(0x001e9920) + k));
            for (uint32_t k = 0; k < 0x18; k += 4) sw(f + 0x130 + k, lwu(OV(0x001e9938) + k));
            speed = speed * lwc1(0x0015ed6c) - close * lwc1(0x0015ed6c);
            const uint32_t c0 = color_swap_channels(lwu(f + 0x110 + static_cast<uint32_t>(rand_mod(6)) * 4), lbu(0x0013e52a));
            const uint32_t c1 = color_swap_channels(lwu(f + 0x130 + static_cast<uint32_t>(rand_mod(6)) * 4), lbu(0x0013e52a));
            const float size = big * 400000.0f;
            const int32_t count = rand_range(frame_scale_i(0xf), frame_scale_i(0x14));
            const int32_t b38 = rand_range(frame_scale_i(0x19), frame_scale_i(0x1e));
            spawn_particle_0b(s8, f + 0x80, c0, c1, static_cast<uint32_t>(count), static_cast<uint32_t>(b38), 0, 0, size, speed);
        }
        auto flash = [&](uint32_t r, uint32_t g, uint32_t b, int32_t frames, uint32_t a, float scale) {
            sw(f + 0x15c, r);
            sw(f + 0x160, g);
            sw(f + 0x164, b);
            swap3_by_mask(*static_cast<uint32_t*>(ee_ram_ptr(f + 0x15c)), *static_cast<uint32_t*>(ee_ram_ptr(f + 0x160)),
                          *static_cast<uint32_t*>(ee_ram_ptr(f + 0x164)), lbu(0x0013e52a));
            fx_4a8_spawn(m, s8, f + 0x80, static_cast<uint32_t>(frame_scale_i(frames)), lbu(f + 0x15c), lbu(f + 0x160), lbu(f + 0x164), a, scale);
        };
        if (9.0f < dist) {
            const float scale = big * 4.0f;
            fx_4a8_spawn(m, s8, f + 0x80, static_cast<uint32_t>(frame_scale_i(0xf)), 0x7f, 0x7f, 0x7f, 0x20, scale);
            sw(f + 0x150, 0x7f);
            sw(f + 0x154, 0x20);
            sw(f + 0x158, 0);
            swap3_by_mask(*static_cast<uint32_t*>(ee_ram_ptr(f + 0x150)), *static_cast<uint32_t*>(ee_ram_ptr(f + 0x154)),
                          *static_cast<uint32_t*>(ee_ram_ptr(f + 0x158)), lbu(0x0013e52a));
            fx_4a8_spawn(m, s8, f + 0x80, static_cast<uint32_t>(frame_scale_i(0x18)), lbu(f + 0x150), lbu(f + 0x154), lbu(f + 0x158), 0x20, scale);
        }
        flash(0x7f, 0x3f, 0, 0x14, 0x30, big * 4.0f);
        flash(0x60, 0x10, 0, 0x1b, 0x40, big * 3.5f);
        flash(0x20, 0, 0, 0x1d, 0x20, big * 3.0f);
    }
    if (!(lh(pv + 0x68) < frame_scale_i(0x14))) {  // it rolled long: a ring of dust instead
        swc1(m + 0x18, lwc1(m + 0x18) + 1.0f);
        const float h = surface_height(s8, f + 0xf0);
        swc1(m + 0x18, lwc1(m + 0x18) - 1.0f);
        for (int32_t i = 0x95; i >= 0; --i) {
            V(f + 0x110) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
            swc1(f + 0x110, rand_range_f(-1.0f, 1.0f));
            swc1(f + 0x114, rand_range_f(-1.0f, 1.0f));
            swc1(f + 0x118, rand_range_f(-0.5f, 2.0f));
            V(f + 0x100) = V(f + 0x110);
            const float r = rand_range_f(lwc1(0x0015ed6c) * 4.0f, lwc1(0x0015ed6c) * 8.0f);
            vec3_set_length(V(f + 0x100), V(f + 0x100), big * r);
            const float sz = rand_range_f(u2f(0x3d4ccccdu), u2f(0x3dcccccdu));
            spawn_particle_22(s8, f + 0x100, sz * 210000.0f, h);
        }
    }
    sw(pv, 0);
    sb(m + 0xbc, 0);
    if (!(lh(pv + 0x68) < frame_scale_i(0x14))) moby_sound_play(0x16, 0, m, 0);
    else if (lbu(0x0013e52a) != 0) moby_sound_start(1, 0, m);
    else moby_sound_start(0, 0, m);
    if (lh(pv + 0x68) != 0) {  // over water: steam and droplets
        V(f + 0xf0) = V(m + 0x10);
        swc1(f + 0xf8, lwc1(f + 0xf8) + 3.5f);
        const float top = floor_probe(0.5f, f + 0xf0, 0);
        const float low = floor_probe_20(f + 0xf0);
        if (top != low && top - low < u2f(0x3f99999au)) {
            for (int32_t i = 0x63; i >= 0; --i) {
                const float k = lwc1(0x0015ed6c);
                V(f + 0x110) = Vec4{0.0f, 0.0f, 0.0f, 0.0f};
                swc1(f + 0x110, rand_range_f(k * -0.75f, k * 0.75f));
                swc1(f + 0x114, rand_range_f(lwc1(0x0015ed6c) * -0.75f, lwc1(0x0015ed6c) * 0.75f));
                swc1(f + 0x118, rand_range_f(lwc1(0x0015ed6c) * 4.0f, lwc1(0x0015ed6c) * 14.0f));
                V(f + 0x100) = V(f + 0x110);
                float r = rand_range_f(0.0f, u2f(0x3eb33333u));
                const float a = rand_angle();
                swc1(f + 0x110, cos_vu(a) * r);
                swc1(f + 0x114, sin_vu(a) * r);
                swc1(f + 0x118, 0.0f);
                vec3_add(V(f + 0x110), V(f + 0x110), V(s8));
                r = r * lwc1(0x0015ed6c) * 8.0f;
                swc1(f + 0x118, top + 0.5f);
                swc1(f + 0x108, lwc1(f + 0x108) - r);
                const float sz = rand_range_f(0.25f, 1.0f);
                spawn_particle_40(f + 0x110, f + 0x100, 0, 0x167f6060u, 0x007f6060u, 1, sz, top, lwc1(0x0015ed70) * 20.0f);
            }
            for (int32_t i = 0x13; i >= 0; --i) {
                const uint32_t c0 = rgba_lerp_ovl(0x7f000000u, 0x7f7f6060u, rand_range_f(0.25f, 1.0f));
                const uint32_t c1 = rgba_lerp_ovl(0u, 0x003f3030u, rand_range_f(0.5f, 1.0f));
                swc1(f + 0x110, rand_range_f(-3.0f, 3.0f));
                swc1(f + 0x114, rand_range_f(-3.0f, 3.0f));
                swc1(f + 0x118, rand_range_f(lwc1(0x0015ed6c) * 4.0f, lwc1(0x0015ed6c) * 14.0f));
                vec3_set_length(V(f + 0x110), V(f + 0x110), rand_range_f(1.5f, 3.0f) * lwc1(0x0015ed6c));
                const float z = rand_range_f(3.0f, 6.0f) * lwc1(0x0015ed6c);
                V(f + 0x100) = V(m + 0x10);
                swc1(f + 0x118, z);
                swc1(f + 0x108, top);
                const float sz = rand_range_f(u2f(0x46f61801u), u2f(0x474d1400u));
                const int32_t n = rand_range(frame_scale_i(0x5a), frame_scale_i(0x96));
                spawn_particle_0f(f + 0x100, f + 0x110, c0, c1, static_cast<uint32_t>(n), 1, lbu(lwu(OV(0x001b20ac))), -1, sz);
            }
        }
    }
    // the camera shake by the distance, the effect left
    if (dist < 20.0f) swc1(OV(0x00166de0), 0.4f - dist * u2f(0x3c8f5c29u));
    else swc1(OV(0x00166de0), u2f(0x3d4cccd0u));
    sw(OV(0x00166de8), static_cast<uint32_t>(frame_scale_i(0x19)));
    vec3_distance(V(s8), V(0x0013f3d0));
    effect_spawn(f + 0x10, s8, 0, 0);
    if (lbu(m + 0x20) == 2) {
        blast();
        return;
    }
    moby_retire(m);
    done();
}

// The explosion's debris (class 0x7a, 0x2af450): flies and spins (pvars +0x10 / +0x14), falls
// (14.6 g), retired below 0 on any axis or when its frames (+0x18) run out, shrinking over the
// last quarter of them; with flag 1 (+0xbc) it trails a spark (0x269958) of the colours at
// 0x161438, their channels swapped by the flag's other bits.
// orig: 0x002af5b8
void debris_7a_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if ((lbu(m + 0xbc) & 1) != 0) {
        const uint32_t f = ee_stack_alloc(0x30);  // the colours, the spark's direction (by address)
        for (int32_t i = 0; i >= 0; --i) {
            for (uint32_t k = 0; k < 0x10; k += 4) sw(f + k, lwu(OV(0x00161438) + k));
            Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
            d.x = rand_range_f(-1.0f, 1.0f);
            d.y = rand_range_f(-1.0f, 1.0f);
            d.z = rand_range_f(-1.0f, 1.0f);
            V(f + 0x10) = d;
            vec3_set_length(V(f + 0x10), V(f + 0x10), lwc1(0x0015ed6c));
            const uint32_t sw_bits = lbu(m + 0xbc) >> 1;
            if ((lbu(m + 0xbc) & 0xfe) != 0) {
                const uint32_t c0 = color_swap_channels(lwu(f + static_cast<uint32_t>(rand_mod(4)) * 4), sw_bits);
                const uint32_t c1 = color_swap_channels(0x4fff, lbu(m + 0xbc) >> 1);
                spawn_particle_4(m + 0x10, f + 0x10, c0, c1, static_cast<uint32_t>(frame_scale_i(0x2d)), 0x32, 0x78, 1);
            } else {
                const uint32_t k = static_cast<uint32_t>(rand_mod(4));
                const uint32_t n = static_cast<uint32_t>(frame_scale_i(0x2d));
                spawn_particle_4(m + 0x10, f + 0x10, lwu(f + k * 4), 0x4fff, n, 0x32, 0x78, 1);
            }
        }
        ee_stack_free(0x30);
    }
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x10)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x14)));
    vec3_add(V(m + 0x10), V(m + 0x10), V(pv));
    swc1(pv + 8, lwc1(pv + 8) - lwc1(0x0015ed70) * u2f(0x4169999au));
    if (lwc1(m + 0x10) < 0.0f || lwc1(m + 0x14) < 0.0f || lwc1(m + 0x18) < 0.0f) {
        moby_retire(m);
        return;
    }
    if (lh(pv + 0x18) < lh(pv + 0x1a) / 4) {
        const float a = int_to_float(lh(pv + 0x18));
        const float b = int_to_float(lh(pv + 0x1a) / 4);
        swc1(m + 0x2c, fpu_div(lwc1(pv + 0x1c) * a, b));
    }
    if (timer_tick_i16(h16(pv + 0x18)) != 0) moby_retire(m);
}

}  // namespace rac1
