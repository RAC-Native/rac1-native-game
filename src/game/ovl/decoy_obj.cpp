// Ported overlay code: Decoy Glove decoy object (classes 0xcb / gold 0x76c, update 0x002c4380).
#include <game/ovl/decoy_obj.h>

#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350u;
constexpr uint32_t GOLD_DECOY = 0x0013e539u;
}  // namespace

// orig: 0x002c3440
void decoy_obj_2c3440(uint32_t p0) {
    const uint32_t pos = ee_stack_alloc(0x10);
    const float lo = u2f(0xbe4ccccdu);
    const float hi = u2f(0x3e4ccccdu);

    for (int32_t i = 9; i >= 0; --i) {
        V(pos) = V(p0 + 0x10);
        const float dx = rand_range_f(lo, hi);
        swc1(pos + 0x0, fpu_add(lwc1(pos + 0x0), dx));
        const float dy = rand_range_f(lo, hi);
        swc1(pos + 0x4, fpu_add(lwc1(pos + 0x4), dy));
        const float dz = rand_range_f(lo, hi);
        swc1(pos + 0x8, fpu_add(lwc1(pos + 0x8), dz));

        const uint32_t r = (static_cast<uint32_t>(libc_rand()) + 0x30u) & 0x3fu;
        const uint32_t g = (static_cast<uint32_t>(libc_rand()) + 0x20u) & 0x3fu;
        const uint32_t b = static_cast<uint32_t>(libc_rand()) & 0x2fu;
        const float sz = rand_range_f(u2f(0x47c35000u), u2f(0x49127c00u));
        const int32_t life = frame_scale_i(ee_rem(libc_rand(), 0x28) + 0xa);
        spawn_particle_05(pos, r, g, b, life, sz, 0.0f);
    }

    moby_list_remove(p0, OV(0x001b0830u));
    moby_retire(p0);
    ee_stack_free(0x10);
}

// orig: 0x002c4328
void decoy_obj_2c4328(uint32_t p0) {
    const uint32_t surf = static_cast<uint32_t>(sub_001f35d8());
    // (the surface's case by its index: the original's switch over it, compiled to the jump table 0x1e9c20; the disc's
    // table is not read, so it works on a build whose own code moved the case labels)
    switch (surf) {
    case 0: case 1: case 3: case 8: case 11: case 12: case 13:  // (0x002c4364)
        decoy_obj_2c3440(p0);
        break;
    case 2: case 4: case 5: case 6: case 7: case 9: case 10:  // (0x002c436c)
        break;
    }
}

// orig: 0x002c3af8
int32_t decoy_obj_2c3af8(uint32_t p0, uint32_t p1, uint32_t p2) {
    if (lw(P + 0x2084) == 0x72) return 0;
    if (lw(OV(0x0015f5c4u)) != 0) return 0;

    Vec4 prev_pos = V(p1 + 0x10);
    Vec4 cur_pos{};
    Vec4 vel{};
    int32_t ticks = 0;
    int32_t landed = 0;

    if (lbu(p1 + 0x20) == 1) {
        float ang = angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
        cur_pos.x = fpu_mul(cos_vu(ang), u2f(0x3f5be9fbu));
        ang = angle_add(u2f(0xbeb953dfu), atan2_f(lwc1(P + 0x670), lwc1(P + 0x674)));
        cur_pos.z = 0.0f;
        cur_pos.y = fpu_mul(sin_vu(ang), u2f(0x3f5be9fbu));
        vec3_add(cur_pos, cur_pos, V(P + 0x80));
        cur_pos.z = fpu_add(cur_pos.z, u2f(0x3ec8198fu));

        vel = V(p0 + 0x10);
        if (lw(P + 0x208c) == 0xf) {
            vec3_add(vel, vel, V(P + 0x100));
        }
        const uint32_t plat = lwu(P + 0x2fc);
        if (plat != 0 && moby_pvar_word2(plat) != 0 && p2 != 0) {
            vec3_add(vel, vel, V(P + 0x100));
        }
        ticks = frame_scale_i(0x12c);
    } else {
        const uint32_t hand = lwu(p0 + 0x40);
        const uint32_t hpv = lwu(hand + 0x78);
        cur_pos = V(p1 + 0x10);
        vel = V(p0 + 0x10);
        ticks = lh(p0 + 0x44);
        if (lwu(hpv + 0x50) != 0) {
            sh(p0 + 0x46, 1u);
        }
    }

    vel.z = fpu_sub(vel.z, fpu_mul(lwc1(0x0015ed6cu), 0.5f));
    const uint32_t hero = lwu(P + 0x2080);
    Vec4 hero_start = V(hero + 0x10);
    hero_start.z = cur_pos.z;

    if (segment_collide_v(hero_start, cur_pos, 0x10u, hero, nullptr) != 0) {
        if (sub_001f35d8() != 0 || !(0.0f < vel.z)) {
            const uint32_t cr = OV(0x00173e40u);
            const uint32_t hit_m = lwu(cr + 0x18);
            if (hit_m != 0) {
                if (lw(cr + 0x1c) > 0) {
                    V(p0 + 0x20) = V(OV(0x00173e60u));
                    landed = 1;
                } else {
                    bool accept = true;
                    if (hit_m == lwu(P + 0x2080) || hit_m == lwu(p0 + 0x40)) {
                        const int32_t thr = frame_scale_i(0x12c) - frame_scale_i(0xa);
                        if (!(ticks < thr)) accept = false;
                    }
                    if (accept) {
                        V(p0 + 0x20) = V(hit_m + 0x10);
                        Vec4 probe{0.0f, 0.0f, 0.5f, 0.0f};
                        vec3_add(probe, probe, V(p0 + 0x20));
                        (void)floor_probe_v(0.5f, probe, 0u);
                        landed = 1;
                    }
                }
            } else if (lw(cr + 0x1c) > 0) {
                V(p0 + 0x20) = V(OV(0x00173e60u));
                landed = 1;
            }
        }
    }

    if (timer_tick_i32(ticks) == 0 && landed == 0) {
        for (;;) {
            prev_pos = cur_pos;
            for (int32_t s = 9; s >= 0; --s) {
                vec3_add(cur_pos, cur_pos, vel);
                vel.z = fpu_sub(vel.z, fpu_mul(lwc1(0x0015ed70u), u2f(0x41100000u)));
            }
            if (segment_collide_v(prev_pos, cur_pos, 0x10u, p1, nullptr) != 0) {
                if (sub_001f35d8() != 0 || !(0.0f < vel.z)) {
                    const uint32_t cr = OV(0x00173e40u);
                    const uint32_t hit_m = lwu(cr + 0x18);
                    if (hit_m != 0) {
                        if (lw(cr + 0x1c) > 0) {
                            V(p0 + 0x20) = V(OV(0x00173e60u));
                            break;
                        }
                        bool accept = true;
                        if (hit_m == lwu(P + 0x2080) || hit_m == lwu(p0 + 0x40)) {
                            const int32_t thr = frame_scale_i(0x12c) - frame_scale_i(0xa);
                            if (!(ticks < thr)) accept = false;
                        }
                        if (accept) {
                            V(p0 + 0x20) = V(hit_m + 0x10);
                            Vec4 probe{0.0f, 0.0f, 0.5f, 0.0f};
                            vec3_add(probe, probe, V(p0 + 0x20));
                            (void)floor_probe_v(0.5f, probe, 0u);
                            break;
                        }
                    } else if (lw(cr + 0x1c) > 0) {
                        V(p0 + 0x20) = V(OV(0x00173e60u));
                        break;
                    }
                }
            }
            if (timer_tick_i32(ticks) != 0) break;
        }
    }

    if (ticks == 0) return 0;

    if (p2 == 0) {
        const uint32_t hm = lwu(OV(0x00173e58u));
        if (hm == 0 || moby_pvar_word2(hm) == 0) return 0;
    }

    bool draw_ring = true;
    if (p2 == 2) {
        const uint32_t hm = lwu(OV(0x00173e58u));
        if (hm != 0 && moby_pvar_word2(hm) != 0) draw_ring = false;
    }
    if (draw_ring) {
        sub_001f93e0(OVF(0x002c31c0u), p1);
    }

    V(p0 + 0x30) = V(OV(0x00173e80u));
    vec3_sub(prev_pos, V(p0 + 0x20), V(OV(0x00166dc0u)));
    vec3_scale(prev_pos, prev_pos, u2f(0x3f733333u));
    vec3_add(V(p0 + 0x20), prev_pos, V(OV(0x00166dc0u)));
    return 1;
}

// orig: 0x002c40a0
void decoy_obj_2c40a0(uint32_t p0) {
    const uint32_t dpv = lwu(p0 + 0x78);
    const int32_t hero_grounded = lh(P + 0x1e2);
    if (lw(dpv + 0x68) == 0) {
        if (hero_grounded == 0) return;
        if (!(vec2_distance(V(P + 0x80), V(p0 + 0x10)) < 1.0f)) return;
        if (!(abs_f(fpu_sub(lwc1(P + 0x88), lwc1(p0 + 0x18))) < u2f(0x3fa66666u))) return;
        sw(dpv + 0x68, 1u);
    }

    Vec4 diff{};
    vec3_sub(diff, V(p0 + 0x10), V(P + 0x80));
    diff.z = 0.0f;
    float pen = fpu_sub(vec2_length(diff), u2f(0x3fb33333u));
    if (0.0f < pen) pen = 0.0f;
    if (hero_grounded == 0) pen = 0.0f;

    const float fs64 = lwc1(0x0015ed64u);
    const float fs6c = lwc1(0x0015ed6cu);
    const float k002 = u2f(0x3ca3d70au);
    (void)spring_to_v(pen, dpv + 0x60, 0.0f, fpu_mul(fs64, k002), fpu_mul(fs64, u2f(0x3e99999au)), fpu_mul(fs6c, 4.0f));

    if (pen == 0.0f && lwc1(dpv + 0x60) == 0.0f) {
        const float fh = floor_probe_v(0.5f, V(p0 + 0x10), 0u);
        if (abs_f(fpu_sub(lwc1(p0 + 0x18), fh)) < k002) {
            sw(dpv + 0x68, 0);
        }
    }

    const float dlen = vec3_length(diff);
    if (dlen < lwc1(dpv + 0x60)) {
        swc1(dpv + 0x60, dlen);
    }
    const float spd = lwc1(dpv + 0x60);
    if (spd < dlen) {
        vec3_set_length(diff, diff, spd);
    }

    (void)walker_move_v(p0, diff, 0, 0.5f, u2f(0x3ecccccdu), 0.0f);
    const float fh = floor_probe_v(0.5f, V(p0 + 0x10), 0u);
    if (lwc1(p0 + 0x18) < fh) {
        swc1(p0 + 0x18, fh);
        swc1(dpv + 0x64, 0.0f);
    } else {
        const float vz = fpu_sub(lwc1(dpv + 0x64), fpu_mul(lwc1(0x0015ed70u), 24.0f));
        swc1(dpv + 0x64, vz);
        const float nz = fpu_add(lwc1(p0 + 0x18), vz);
        swc1(p0 + 0x18, nz);
        if (nz < fh) {
            swc1(p0 + 0x18, fh);
        }
    }
}

// orig: 0x002c4380
void decoy_object_update(uint32_t m) {
    uint32_t gold = lbu(GOLD_DECOY);
    const uint32_t dpv = lwu(m + 0x78);
    if (lh(m + 0xa6) == 0xcb) gold = 0;
    const float gold_f = int_to_float(static_cast<int32_t>(gold));

    const float max_coord = u2f(0x447f4000u);
    const float px = lwc1(m + 0x10);
    const float py = lwc1(m + 0x14);
    const float pz = lwc1(m + 0x18);
    if (px < 2.0f || max_coord < px || py < 2.0f || max_coord < py || pz < 2.0f || max_coord < pz) {
        moby_list_remove(m, OV(0x001b0830u));
        moby_retire(m);
        return;
    }

    const uint32_t equipped = lwu(P + 0x1090);
    if ((equipped == 0 || lh(equipped + 0xa6) != 0x232) && lbu(m + 0x20) == 1) {
        moby_list_remove(m, OV(0x001b0830u));
        moby_retire(m);
        return;
    }
    if (lbu(P + 0x20a4) != 0 && lbu(m + 0x20) == 1) {
        moby_list_remove(m, OV(0x001b0830u));
        moby_retire(m);
        return;
    }

    sw(dpv + 0x54, lwu(dpv + 0x54) + 1u);
    if (lbu(P + 0x20a5) != 0 && lbu(m + 0x20) < 2u) {
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    } else {
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    }

    if (lh(lwu(dpv + 0x40) + 0xa6) == 0x232 && lh(dpv + 0x46) == 0 && 0.0f < vec3_length(V(dpv + 0x10))) {
        const uint32_t plat = lwu(P + 0x2fc);
        if (plat != 0 && moby_pvar_word2(plat) != 0) {
            if (decoy_obj_2c3af8(dpv, m, 0) == 0) {
                (void)decoy_obj_2c3af8(dpv, m, 2);
            }
        } else {
            (void)decoy_obj_2c3af8(dpv, m, 1);
        }
    }

    const uint32_t st = lbu(m + 0x20);
    if (st >= 8u) {
        sb(m + 0x20, 0);
        return;
    }

    // (the state's case by its index: the original's switch over the state, compiled to the jump table 0x1e9c60; the
    // disc's table is not read, so the state works on a build whose own code moved the case labels: PAL 2.00)
    switch (st) {
    case 0: {  // (0x002c45f4)
        sb(m + 0x20, 1);
        sh(dpv + 0x44, 0);
        sw(m + 0x98, 0xffffffffu);
        sb(dpv + 0x6e, 0);
        sh(dpv + 0x6c, 0);
        return;
    }
    case 1: {  // (0x002c4614)
        const uint32_t owner = lwu(dpv + 0x40);
        sh(dpv + 0x44, 0);
        if (owner != 0 && (lbu(owner + 0x20) & 0xf0u) != 0xf0u) {
            const uint32_t eq = lwu(P + 0x1090);
            if (eq != 0 && lh(eq + 0xa6) == 0x232) return;
        }
        moby_list_remove(m, OV(0x001b0830u));
        moby_retire(m);
        return;
    }
    case 2: {  // (0x002c466c)
        const float max_s = lwc1(lwu(m + 0x24) + 0x24);
        const float cur_s = lwc1(m + 0x2c);
        if (cur_s < max_s) {
            swc1(m + 0x2c, fpu_add(cur_s, fpu_mul(lwc1(0x0015ed60u), u2f(0x3ca3d70au))));
        }

        swc1(dpv + 0x18, fpu_sub(lwc1(dpv + 0x18), fpu_mul(lwc1(0x0015ed70u), u2f(0x41100000u))));

        if (lh(OV(0x001b0830u)) > 0) {
            const uint32_t list = OV(0x001b0830u);
            const float push_k = u2f(0x3c75c28fu);
            int32_t idx = 1;
            uint32_t ep = list + 2u;
            do {
                const uint32_t other = lwu(OV(0x0015ffd8u)) + (static_cast<uint32_t>(lh(ep)) << 8);
                if (other != m) {
                    const uint32_t ost = lbu(other + 0x20);
                    if (ost >= 2u && other != 0 && ost != 0xfeu && ost != 0xfdu) {
                        Vec4 sep{};
                        vec3_sub(sep, V(m + 0x10), V(other + 0x10));
                        sep.z = 0.0f;
                        const float sl = vec3_length(sep);
                        if (sl < u2f(0x3fe66666u)) {
                            vec3_scale(sep, sep, fpu_mul(fpu_sub(u2f(0x3fe66666u), sl), push_k));
                            vec3_add(V(dpv + 0x10), V(dpv + 0x10), sep);
                        }
                    }
                }
                ++idx;
                ep += 2u;
            } while (!(lh(OV(0x001b0830u)) < idx));
        }

        if (lw(P + 0x2084) == 0x1e) {
            sw(lwu(P + 0x2080) + 0x94, 0);
        }

        const float k02 = u2f(0x3e4ccccdu);
        if ((walker_move_v(m, V(dpv + 0x10), 0, k02, k02, 0.0f) & 1) != 0) {
            Vec4 sp_pos = V(m + 0x10);
            sp_pos.z = fpu_add(sp_pos.z, k02);
            if (sphere_collide_v(sp_pos, u2f(0x3e6b851fu), 0, m) != 0) {
                if (lwc1(dpv + 0x18) < 0.0f) {
                    Vec4 hsep{};
                    vec3_sub(hsep, V(m + 0x10), V(P + 0x80));
                    hsep.z = 0.0f;
                    const float hl = vec3_length(hsep);
                    if (hl < u2f(0x3fe66666u)) {
                        vec3_scale(hsep, hsep, fpu_mul(fpu_sub(u2f(0x3fe66666u), hl), u2f(0x3c75c28fu)));
                        vec3_add(V(dpv + 0x10), V(dpv + 0x10), hsep);
                    }
                }

                vec3_reflect(V(dpv + 0x10), V(dpv + 0x10), V(OV(0x00173e80u)));
                const uint32_t cr = OV(0x00173e40u);
                if (lwu(cr + 0x18) != 0 && lw(cr + 0x1c) <= 0) {
                    vec3_scale(V(dpv + 0x10), V(dpv + 0x10), u2f(0x3f866666u));
                } else {
                    vec3_scale(V(dpv + 0x10), V(dpv + 0x10), u2f(0x3e4ccccdu));
                }

                const float k001 = u2f(0x3c23d70au);
                bool low_speed = vec3_length(V(dpv + 0x10)) < fpu_mul(lwc1(0x0015ed60u), k001);
                if (!low_speed) {
                    const uint32_t plat = lwu(P + 0x2fc);
                    if (plat != 0 && moby_pvar_word2(plat) != 0) {
                        const float vl = vec3_length(V(dpv + 0x10));
                        const float pl = vec3_length(V(P + 0x100));
                        if (vl < fpu_add(fpu_mul(lwc1(0x0015ed60u), k001), pl)) {
                            low_speed = true;
                        }
                    }
                }

                if (low_speed) {
                    const float nxy = vec2_length(V(cr + 0x40));
                    const float ang = atan2_f(lwc1(cr + 0x48), nxy);
                    if (ang < u2f(0x3f1c61aau)) {
                        const uint32_t cnt = lhu(dpv + 0x44) + 1u;
                        sh(dpv + 0x44, cnt);
                        bool settle = true;
                        if (lwu(cr + 0x18) != 0 && lw(cr + 0x1c) <= 0) {
                            if (!(frame_scale_i(0xf0) < static_cast<int32_t>(static_cast<int16_t>(cnt)))) {
                                settle = false;
                            }
                        }
                        if (settle) {
                            if (lbu(m + 0x53) != 1) {
                                moby_anim_set(m, 1, 0, frame_scale_i(0xa));
                            }
                            sw(m + 0x98, 0);
                            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
                            vec_clear(V(dpv + 0x10));
                            sb(m + 0x20, 3);
                            sh(dpv + 0x44, static_cast<uint32_t>(frame_scale_i(0x258)));
                            swc1(m + 0x18, lwc1(OV(0x00173e40u) + 0x28));

                            const Vec4 nrm = V(OV(0x00173e40u) + 0x40);
                            const float k01 = u2f(0x3dcccccdu);
                            float c = cos_vu(lwc1(m + 0x48));
                            float s = sin_vu(lwc1(m + 0x48));
                            const float rx = fpu_add(fpu_mul(nrm.x, c), fpu_mul(nrm.y, s));
                            c = cos_vu(lwc1(m + 0x48));
                            s = sin_vu(lwc1(m + 0x48));
                            const float ry = fpu_sub(fpu_mul(nrm.y, c), fpu_mul(nrm.x, s));
                            const float rz = nrm.z;
                            const float h = sqrt_f(fpu_add(fpu_mul(rx, rx), fpu_mul(rz, rz)));

                            swc1(dpv + 0x4c, -atan2_f(h, ry));
                            swc1(dpv + 0x50, atan2_f(rz, rx));

                            const float d0 = angle_sub(lwc1(dpv + 0x4c), lwc1(m + 0x40));
                            swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(d0, fpu_mul(lwc1(0x0015ed60u), k01))));

                            const float d1 = angle_sub(lwc1(dpv + 0x50), lwc1(m + 0x44));
                            swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(d1, fpu_mul(lwc1(0x0015ed60u), k01))));
                        }
                    }
                }

                decoy_obj_2c4328(m);
                if (countdown_u8(dpv + 0x6f) != 0) {
                    (void)moby_sound_start(0, 0, m);
                    sb(dpv + 0x6f, static_cast<uint32_t>(frame_scale_i(0x14)));
                } else {
                    metal_25f8e0(dpv + 0x10, lwc1(0x0015ed6cu));
                }
            }
        }

        if (lw(P + 0x2084) == 0x1e) {
            const uint32_t hero = lwu(P + 0x2080);
            sw(hero + 0x94, lwu(lwu(hero + 0x24) + 0x10));
        }
        return;
    }
    case 3: case 4: {  // (0x002c4c08)
        // enhancement: 1.02 Gameplay, fix 14 (the 1.02 build, 0x2c5f10): the decoy pushed by Ratchet swimming into it (fixes102.h)
        gameplay102_decoy_push(m);
        const uint32_t hit = sub_0025a420(m, 0x30001u, 0);

        const float k01 = u2f(0x3dcccccdu);
        const float d0 = angle_sub(lwc1(dpv + 0x4c), lwc1(m + 0x40));
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), fpu_mul(d0, fpu_mul(lwc1(0x0015ed60u), k01))));
        const float d1 = angle_sub(lwc1(dpv + 0x50), lwc1(m + 0x44));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), fpu_mul(d1, fpu_mul(lwc1(0x0015ed60u), k01))));

        decoy_obj_2c40a0(m);
        (void)timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(dpv + 0x5c)));
        (void)timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(dpv + 0x6c)));

        if (hit != 0) {
            const uint32_t attacker = lwu(hit + 0x20);
            bool take_hit = false;
            if (attacker != lwu(dpv + 0x58) || lw(dpv + 0x5c) <= 0) {
                sw(dpv + 0x58, attacker);
                take_hit = true;
            }
            if (take_hit) {
                sw(dpv + 0x5c, static_cast<uint32_t>(frame_scale_i(0x1e)));
                if (lh(dpv + 0x6c) == 0) {
                    // (the original's mask 0x00ffffff00000000 (0x2c4ce8): the bytes +0x6c .. +0x6e, the stun's timer and flag,
                    // not the countdown +0x6f)
                    if (lhu(hit + 0x28) == 0x101u && (ld(dpv + 0x68) & 0x00ffffff00000000ull) == 0) {
                        sh(dpv + 0x6c, static_cast<uint32_t>(frame_scale_i(0x64)));
                        sb(dpv + 0x6e, 1);
                    } else {
                        sb(dpv + 0x6e, 0);
                    }

                    (void)moby_sound_start(0, 0, m);
                    sb(dpv + 0x7, 0xfa);
                    swc1(dpv + 0x48, fpu_sub(lwc1(dpv + 0x48), lwc1(hit + 0x2c)));
                    color_pulse_start(m, dpv);

                    if (lwc1(dpv + 0x48) < 0.0f) {
                        const uint32_t ebuf = ee_stack_alloc(0x40);
                        const uint32_t jpos = ebuf;
                        const uint32_t rec = ebuf + 0x10;

                        const float dt = lwc1(0x0015ed60u);
                        const float hxy = fpu_mul(dt, 0.5f);
                        const float hz = fpu_mul(dt, u2f(0x3f333333u));
                        V(jpos) = Vec4{hxy, hxy, hz, 0.0f};
                        metal_25f8e0(jpos, hxy);

                        explosion_effect(m, jpos, 0, 0, 0, 0, 2, 1, 1, -1, gold, 0.0f, 0.0f, 4.0f, 3.0f, 9.0f, 1.0f, 12.0f);
                        if (gold != 0) {
                            hit_rec_init(rec, m, 0x810000u, fpu_add(gold_f, gold_f));
                            sb(rec + 0x19, 1);
                            sb(rec + 0x18, 2);
                            sh(rec + 0x1a, lhu(m + 0xa6));
                            (void)mobys_in_sphere(m + 0x10, fpu_mul(gold_f, 3.0f), 0x10u, m, rec);
                        }

                        ee_stack_free(0x40);
                        moby_list_remove(m, OV(0x001b0830u));
                        moby_retire(m);
                        return;
                    }
                }
            }
        }

        sb(m + 0xa4, 0xff);
        color_pulse_update(m, dpv);

        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, frame_scale_i(0xa));
            }
            swc1(m + 0x58, rand_range_f(u2f(0x3f333333u), u2f(0x3f866666u)));
        }

        if (lbu(m + 0x20) != 4) {
            const float fh0 = floor_probe_v(0.5f, V(m + 0x10), 0u);
            if (fh0 == 0.0f) return;
            decoy_obj_2c4328(m);

            if (fpu_add(fh0, u2f(0x3ca3d70au)) < lwc1(m + 0x18)) {
                swc1(dpv + 0x18, fpu_sub(lwc1(dpv + 0x18), fpu_mul(lwc1(0x0015ed70u), u2f(0x411ccccdu))));
                vec3_add(V(m + 0x10), V(m + 0x10), V(dpv + 0x10));
                const float fh1 = floor_probe_v(0.5f, V(m + 0x10), 0u);
                if (lwc1(m + 0x18) < fh1) {
                    swc1(m + 0x18, fh1);
                    vec_clear(V(dpv + 0x10));
                    const Vec4 nrm = V(OV(0x00173e80u));
                    float c = cos_vu(lwc1(m + 0x48));
                    float s = sin_vu(lwc1(m + 0x48));
                    const float rx = fpu_add(fpu_mul(nrm.x, c), fpu_mul(nrm.y, s));
                    c = cos_vu(lwc1(m + 0x48));
                    s = sin_vu(lwc1(m + 0x48));
                    const float ry = fpu_sub(fpu_mul(nrm.y, c), fpu_mul(nrm.x, s));
                    const float rz = nrm.z;
                    const float h = sqrt_f(fpu_add(fpu_mul(rx, rx), fpu_mul(rz, rz)));
                    swc1(dpv + 0x4c, -atan2_f(h, ry));
                    swc1(dpv + 0x50, atan2_f(rz, rx));
                }
            } else {
                vec_clear(V(dpv + 0x10));
            }

            const uint32_t floor_m = lwu(OV(0x00173e58u));
            if (floor_m != 0) {
                (void)sub_00260400(m, floor_m, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
            }
            return;
        }

        const float max_s = lwc1(lwu(m + 0x24) + 0x24);
        const float cur_s = lwc1(m + 0x2c);
        if (cur_s < max_s) {
            swc1(m + 0x2c, fpu_add(cur_s, fpu_mul(lwc1(0x0015ed60u), u2f(0x3ca3d70au))));
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(dpv + 0x44))) == 0) return;

        if (lbu(m + 0x53) != 0) {
            moby_anim_set(m, 0, 0, frame_scale_i(0x3c));
        }
        moby_list_remove(m, OV(0x001b0830u));
        sb(m + 0x20, 5);
        return;
    }
    case 5: {  // (0x002c5124)
        if (floor_probe_v(0.5f, V(m + 0x10), 0u) != 0.0f) {
            const uint32_t floor_m = lwu(OV(0x00173e58u));
            if (floor_m != 0) {
                (void)sub_00260400(m, floor_m, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
            }
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            swc1(m + 0x58, 0.0f);
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(dpv + 0x44))) != 0 || (lbu(m + 0x70) & 2u) != 0) {
            sb(m + 0x20, 6);
        }
        return;
    }
    case 6: {  // (0x002c51b0)
        if (floor_probe_v(0.5f, V(m + 0x10), 0u) != 0.0f) {
            const uint32_t floor_m = lwu(OV(0x00173e58u));
            if (floor_m != 0) {
                (void)sub_00260400(m, floor_m, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
            }
        }
        const float cur_s = lwc1(m + 0x2c);
        const float next_s = fpu_sub(cur_s, fpu_mul(cur_s, fpu_mul(lwc1(0x0015ed60u), u2f(0x3ca3d70au))));
        swc1(m + 0x2c, next_s);
        if (next_s < fpu_mul(lwc1(lwu(m + 0x24) + 0x24), u2f(0x3dcccccdu))) {
            moby_retire(m);
        }
        return;
    }
    case 7: {  // (0x002c5264)
        sb(m + 0x20, 0);
        return;
    }
    default:
        return;
    }
}

// orig: (none)
void decoy_obj_fns_register() {
    ovl_register_moby_fn(0x002c4380, decoy_object_update);
}

}  // namespace rac1
