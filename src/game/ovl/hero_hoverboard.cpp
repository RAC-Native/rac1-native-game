// Hoverboard race controller (states 0x6b..0x6f, Zoomerator item 0x24, Rilgar L5 & Kalebo III L16)
// and Kalebo III's slide/board state 0x3e.
#include <game/ovl/hero_hoverboard.h>

#include <game/enhance/level_mod_hero.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l16.h>

namespace rac1 {

namespace {

constexpr uint32_t P = g::PLAYER;

template <uint32_t A>
inline uint32_t hb_addr() {
    return on_rilgar() ? L5_ADDR(A) : L16_ADDR(A);
}
#define HB_ADDR(a) hb_addr<(a)>()

inline int32_t tick_i16(uint32_t addr) {
    int16_t v = lh(addr);
    const int32_t r = timer_tick_i16(v);
    sh(addr, static_cast<uint32_t>(v));
    return r;
}

inline int32_t tick_i32(uint32_t addr) {
    int32_t v = lw(addr);
    const int32_t r = timer_tick_i32(v);
    sw(addr, static_cast<uint32_t>(v));
    return r;
}

// Smooth stick input (-PAD+0x1070, PAD+0x1074) into (state+8, state+4), clamped to [-1, 1].
// orig: L5:0x00237488 / L16:0x00206f80
void board_stick_smooth(float out[2], uint32_t state, float rate) {
    const float sx = -lwc1(0x00141070u);
    const float sy = lwc1(0x00141074u);
    approach_f(state + 0x8, sx, rate);
    if (1.0f < lwc1(state + 0x8)) swc1(state + 0x8, 1.0f);
    if (lwc1(state + 0x8) < -1.0f) swc1(state + 0x8, -1.0f);
    out[0] = lwc1(state + 0x8);
    approach_f(state + 0x4, sy, rate);
    if (1.0f < lwc1(state + 0x4)) swc1(state + 0x4, 1.0f);
    if (lwc1(state + 0x4) < -1.0f) swc1(state + 0x4, -1.0f);
    out[1] = lwc1(state + 0x4);
}

// Compute respawn position and yaw on the hoverboard race track.
// orig: L5:0x00254358 / L16:0x00225ac0
void board_respawn_point(uint32_t pos_out, uint32_t rot_out) {
    const uint32_t bm = lwu(P + 0x86c);
    if (bm == 0) {
        V(pos_out) = V(P + 0x80);
        V(rot_out) = V(P + 0x90);
        return;
    }
    if (level_mod_board_back(pos_out, rot_out)) return;  // enhancement: the board outside a race
    const uint32_t pv = lwu(bm + 0x78);
    const uint32_t path = lwu(HB_ADDR(0x001b0930u) + (lwu(pv + 0x24) << 2));
    int32_t best = 0;
    float best_d = u2f(0x4b18967fu);
    int32_t cnt = lw(path);
    if (cnt > 0) {
        for (int32_t i = 0; i < cnt; i += 2) {
            const uint32_t p = path + static_cast<uint32_t>(i) * 0x10;
            const float to_pt = atan2_f(lwc1(p + 0x10) - lwc1(P + 0x80), lwc1(p + 0x14) - lwc1(P + 0x84));
            const int32_t n = lw(path);
            const int32_t next_i = ee_rem(i + n + 2, n);
            const uint32_t pn = path + static_cast<uint32_t>(next_i) * 0x10;
            const float seg_dir = atan2_f(lwc1(pn + 0x10) - lwc1(p + 0x10), lwc1(pn + 0x14) - lwc1(p + 0x14));
            if (u2f(0x3fc90fdbu) < angle_dist(to_pt, seg_dir)) {
                if (lw(0x0015ed84u) == 0x10) {
                    const uint32_t alt = lwu(P + 0x8b4);
                    if (alt != 0) {
                        const float z0 = lwc1(alt + static_cast<uint32_t>(lh(P + 0x8c8) << 4) + 0x18);
                        const float z1 = lwc1(alt + static_cast<uint32_t>(lh(P + 0x898) << 4) + 0x18);
                        const float zref = z0 < z1 ? z1 : z0;
                        if (4.0f < abs_f(zref - lwc1(p + 0x18))) {
                            cnt = lw(path);
                            continue;
                        }
                    }
                }
                const float d = vec2_distance(V(P + 0x80), V(p + 0x10));
                if (d < best_d) {
                    best_d = d;
                    best = i;
                }
            }
            cnt = lw(path);
        }
    }
    const uint32_t p0 = path + static_cast<uint32_t>(best) * 0x10;
    V(pos_out) = V(p0 + 0x10);
    Vec4 probe = V(pos_out);
    probe.z = probe.z + 2.0f;
    swc1(pos_out + 0x8, floor_probe_v(0.5f, probe, 0));
    vec_clear(V(rot_out));
    const uint32_t p1 = path + static_cast<uint32_t>(best + 1) * 0x10;
    swc1(rot_out + 0x8, atan2_f(lwc1(p1 + 0x10) - lwc1(p0 + 0x10), lwc1(p1 + 0x14) - lwc1(p0 + 0x14)));
}

void board_place_at_respawn() {
    const uint32_t sp = ee_stack_alloc(0x20);
    board_respawn_point(sp, sp + 0x10);
    swc1(P + 0x874, lwc1(sp + 0x18));
    hero_place(sp, sp + 0x10, 0x6b, 1);
    ee_stack_free(0x20);
}

// Airborne hoverboard animation selector (Zoomerator variant vs plain).
// orig: L5:0x002551b8 / L16:0x00226920
uint32_t board_air_anim() {
    return lbu(P + 0x8ce) != 0 ? 0x7fu : 0x55u;
}

// Grounded hoverboard animation selector (boost vs cruise, Zoomerator vs plain).
// orig: L5:0x002551d0 / L16:0x00226938
uint32_t board_ground_anim() {
    if (lh(P + 0x884) != 0) return lbu(P + 0x8ce) != 0 ? 0x7eu : 0x68u;
    return lbu(P + 0x8ce) != 0 ? 0x7du : 0x52u;
}

// Reset the hoverboard race countdown / HUD state.
// orig: L5:0x00264298 / L16:0x002369a8
void board_race_hud_reset() {
    const uint32_t h = lwu(HB_ADDR(0x0015f980u));
    if (h != 0xffffffffu) {
        sub_002359f8(h, 0);
        sw(HB_ADDR(0x0015f980u), 0xffffffffu);
    }
    const uint32_t seq = lwu(HB_ADDR(0x0015f978u)) + 1u;
    sw(HB_ADDR(0x0015f97cu), 0);
    sw(HB_ADDR(0x0015f978u), seq);
}

// Per-frame race progress: lap/waypoint tracking, wrong-way respawn, AI racer rankings, finish.
// orig: L5:0x00254608 / L16:0x00225d70
void board_race_progress() {
    sw(P + 0x894, lwu(P + 0x894) + 1u);
    const uint32_t bm = lwu(P + 0x86c);
    if (bm == 0) return;
    const uint32_t pv = lwu(bm + 0x78);
    const int32_t track_idx = lw(pv + 0x20);
    if (track_idx == -1) return;
    const uint32_t path = lwu(HB_ADDR(0x001b0930u) + (static_cast<uint32_t>(track_idx) << 2));
    const uint32_t planet0 = level_mod_board_planet(lwu(0x0015ed84u));
    sw(P + 0x8b4, path);
    if (planet0 == 0x10u) {
        Vec4 sc0{u2f(0x43230000u), u2f(0x42ee0000u), u2f(0x42a00000u), 0.0f};
        if (vec3_distance(V(P + 0x80), sc0) < 11.0f) {
            const uint16_t wp = lhu(P + 0x898);
            if (static_cast<uint32_t>(wp) - 3u >= 68u) {
                if (static_cast<int16_t>(wp) >= 3) sh(P + 0x89a, lhu(P + 0x89a) + 1u);
                sh(P + 0x898, 0xd);
            }
        }
        Vec4 sc1{u2f(0x435f0000u), u2f(0x438c8000u), u2f(0x42ba0000u), 0.0f};
        if (vec3_distance(V(P + 0x80), sc1) < 7.0f && lh(P + 0x898) < 59) {
            sh(P + 0x898, 0x3b);
        }
    }

    const int32_t path_len = lw(path);
    const int32_t cur_wp = lh(P + 0x898);
    const int32_t next_wp = ee_rem(cur_wp + path_len + 1, path_len);
    const uint32_t p_cur = path + static_cast<uint32_t>(cur_wp) * 0x10;
    const uint32_t p_next = path + static_cast<uint32_t>(next_wp) * 0x10;
    const float wp_dir = atan2_f(lwc1(p_next + 0x10) - lwc1(p_cur + 0x10), lwc1(p_next + 0x14) - lwc1(p_cur + 0x14));
    const float hero_dir = atan2_f(lwc1(P + 0x80) - lwc1(p_cur + 0x10), lwc1(P + 0x84) - lwc1(p_cur + 0x14));
    if (angle_dist(wp_dir, hero_dir) < u2f(0x3fc90fdbu)) {
        sh(P + 0x898, static_cast<uint32_t>(next_wp));
        if (next_wp == 0) sh(P + 0x89a, lhu(P + 0x89a) + 1u);
    }

    int32_t low_z = 0;
    if (level_mod_board_planet(lwu(0x0015ed84u)) == 0x10u) {
        const uint32_t pw = path + static_cast<uint32_t>(lh(P + 0x898)) * 0x10;
        if (lwc1(P + 0x88) < lwc1(pw + 0x18) - 2.0f) {
            low_z = 1;
            if (static_cast<int32_t>(lbu(P + 0x8ae)) < frame_scale_i(0x46)) {
                sb(P + 0x8ae, static_cast<uint32_t>(frame_scale_i(0x46)));
            }
        }
    }

    const int32_t path_len2 = lw(path);
    const int32_t ref_wp = lh(P + 0x8c8);
    const int32_t ref_next = ee_rem(ref_wp + path_len2 + 1, path_len2);
    const uint32_t pr0 = path + static_cast<uint32_t>(ref_wp) * 0x10;
    const uint32_t pr1 = path + static_cast<uint32_t>(ref_next) * 0x10;
    const float ref_dir = atan2_f(lwc1(pr1 + 0x10) - lwc1(pr0 + 0x10), lwc1(pr1 + 0x14) - lwc1(pr0 + 0x14));
    bool wrong_way = low_z != 0;
    if (!wrong_way) {
        if (u2f(0x3fc90fdbu) < angle_dist(lwc1(P + 0x98), ref_dir) && lbu(P + 0x88d) == 0) {
            wrong_way = true;
        } else if (lbu(P + 0x8af) == 0) {
            sb(P + 0x8ae, 0);
        }
    }
    if (wrong_way) {
        const uint8_t cnt = static_cast<uint8_t>(lbu(P + 0x8ae) + 1u);
        sb(P + 0x8ae, cnt);
        if (frame_scale_i(0xb4) < static_cast<int32_t>(cnt)) {
            if (lw(HB_ADDR(0x0015f680u)) == 0) {
                sw(HB_ADDR(0x0015f680u), 1);
                sw(HB_ADDR(0x0015f684u), 0);
                swc1(HB_ADDR(0x0015f67cu), lwc1(0x0015ed60u) * u2f(0x3da3d70au));
            }
            sb(P + 0x8af, 1);
            if (lw(HB_ADDR(0x0015f684u)) == 1) {
                sb(P + 0x8af, 0);
                const uint32_t sp = ee_stack_alloc(0x20);
                board_respawn_point(sp, sp + 0x10);
                sb(P + 0x8ae, 0);
                swc1(P + 0x874, lwc1(sp + 0x18));
                hero_place(sp, sp + 0x10, 0x6b, 1);
                ee_stack_free(0x20);
                const uint32_t alt = lwu(P + 0x8b4);
                if (alt != 0) {
                    const int32_t idx = path_nearest_l1(P + 0x80, alt, 0.0f);  // (Rilgar's 0x2a0260, Kalebo III's 0x271250)
                    if (idx < lh(P + 0x898)) sh(P + 0x898, static_cast<uint32_t>(idx));
                }
            }
        }
    }

    if (lw(pv + 0x40) > 0) {
        for (int32_t i = 0; i < lw(pv + 0x40); ++i) {
            const uint32_t wp_addr = P + 0x7b0 + static_cast<uint32_t>(i) * 2u;
            const uint32_t lap_addr = P + 0x7a0 + static_cast<uint32_t>(i) * 2u;
            const int32_t n = lw(path);
            const int32_t rw = lh(wp_addr);
            const int32_t rn = ee_rem(rw + n + 1, n);
            const uint32_t r0 = path + static_cast<uint32_t>(rw) * 0x10;
            const uint32_t r1 = path + static_cast<uint32_t>(rn) * 0x10;
            const float rdir = atan2_f(lwc1(r1 + 0x10) - lwc1(r0 + 0x10), lwc1(r1 + 0x14) - lwc1(r0 + 0x14));
            const uint32_t moby_list = lwu(HB_ADDR(0x0015ffd8u));
            const uint32_t rm = moby_list + (lwu(pv + 0x2c + static_cast<uint32_t>(i) * 4u) << 8);
            const Vec4 rpos = V(rm + 0x10);
            const float mdir = atan2_f(rpos.x - lwc1(r0 + 0x10), rpos.y - lwc1(r0 + 0x14));
            if (angle_dist(rdir, mdir) < u2f(0x3fc90fdbu)) {
                sh(wp_addr, static_cast<uint32_t>(rn));
                if (rn == 0) sh(lap_addr, lhu(lap_addr) + 1u);
            }
        }
    }

    sw(P + 0x8c0, 1);
    if (lw(pv + 0x40) > 0) {
        const int32_t my_lap = lh(P + 0x89a);
        for (int32_t i = 0; i < lw(pv + 0x40); ++i) {
            const int32_t rlap = lh(P + 0x7a0 + static_cast<uint32_t>(i) * 2u);
            const int32_t rwp = lh(P + 0x7b0 + static_cast<uint32_t>(i) * 2u);
            if (my_lap < rlap || (rlap == my_lap && !(rwp < lh(P + 0x898)))) {
                sw(P + 0x8c0, lwu(P + 0x8c0) + 1u);
            }
        }
    }

    if (lw(pv + 0x40) > 0) {
        int32_t n_racers = lw(pv + 0x40);
        for (int32_t i = 0; i < n_racers; ++i) {
            const uint32_t rank_addr = P + 0x7c0 + static_cast<uint32_t>(i) * 2u;
            const uint32_t lap_addr = P + 0x7a0 + static_cast<uint32_t>(i) * 2u;
            const uint32_t wp_addr = P + 0x7b0 + static_cast<uint32_t>(i) * 2u;
            sh(rank_addr, 1);
            const int32_t my_lap = lh(P + 0x89a);
            const int32_t ilap = lh(lap_addr);
            if (ilap < my_lap || (ilap == my_lap && lh(wp_addr) < lh(P + 0x898))) {
                sh(rank_addr, 2);
            }
            n_racers = lw(pv + 0x40);
            if (n_racers > 0) {
                for (int32_t j = 0; j < n_racers; ++j) {
                    const int32_t cur_ilap = lh(lap_addr);
                    const int32_t jlap = lh(P + 0x7a0 + static_cast<uint32_t>(j) * 2u);
                    if (cur_ilap < jlap ||
                        (cur_ilap == jlap && lh(wp_addr) < lh(P + 0x7b0 + static_cast<uint32_t>(j) * 2u))) {
                        sh(rank_addr, lhu(rank_addr) + 1u);
                    }
                    n_racers = lw(pv + 0x40);
                }
            }
        }
    }

    if (lh(P + 0x89a) < 3) return;
    if (lbu(0x0013d388u) != 0) sw(HB_ADDR(0x0015f6a8u), 1);

    const uint32_t planet1 = level_mod_board_planet(lwu(0x0015ed84u));
    if (planet1 == 5u) {
        sw(0x0015ee38u, lwu(0x0015ee38u) + 1u);
    } else if (planet1 == 0x10u) {
        sw(0x0015ee3cu, lwu(0x0015ee3cu) + 1u);
    }

    const uint32_t planet2 = level_mod_board_planet(lwu(0x0015ed84u));
    if (planet2 == 5u) {
        const int32_t par = frame_scale_i(lw(0x0015ed80u) == 0 ? 0x1644 : 0x1680);
        if (!(par < lw(P + 0x894)) && lbu(0x0013d411u) == 0) {
            sb(0x0013d411u, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        const int32_t prize = lw(pv + 0x28);
        if (prize == -1) return;
        if (lbu(0x0013d388u) == 0 && lw(P + 0x8c0) == 1) {
            sb(lwu(HB_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(prize) << 8) + 0xbc, 1);
        } else {
            sw(P + 0x890, lwu(HB_ADDR(0x0015ffd8u)) + (lwu(pv + 0x28) << 8));
            screen_open(0, 0);
        }
        const int32_t best_t = lw(0x0015ee58u);
        if (best_t == 0) {
            sw(0x0015ee58u, lwu(P + 0x894));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        } else if (lw(P + 0x894) < best_t) {
            sw(0x0015ee58u, lwu(P + 0x894));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        }
        if (lw(0x0015ee68u) < lw(P + 0x8a8)) {
            sw(0x0015ee68u, lwu(P + 0x8a8));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        }
        sb(lwu(HB_ADDR(0x0015ffd8u)) + (lwu(pv + 0x28) << 8) + 0x30, 0xff);
        const uint32_t ch = lwu(P + 0x880);
        if (ch != 0xffffffffu) {
            sub_002359f8(ch, 0);
            sw(P + 0x880, 0xffffffffu);
        }
        hud_weapon_element_release();
    } else if (planet2 == 0x10u) {
        const int32_t prize = lw(pv + 0x28);
        if (prize == -1) return;
        if (lbu(0x0013d4dfu) == 0 && lw(P + 0x8c0) == 1) {
            sb(lwu(HB_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(prize) << 8) + 0xbc, 1);
        } else {
            sw(P + 0x890, lwu(HB_ADDR(0x0015ffd8u)) + (lwu(pv + 0x28) << 8));
            screen_open(0, 0);
        }
        const int32_t best_t = lw(0x0015ee5cu);
        if (best_t == 0) {
            sw(0x0015ee5cu, lwu(P + 0x894));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        } else if (lw(P + 0x894) < best_t) {
            sw(0x0015ee5cu, lwu(P + 0x894));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        }
        if (lw(0x0015ee6cu) < lw(P + 0x8a8)) {
            sw(0x0015ee6cu, lwu(P + 0x8a8));
            sw(0x0015ed98u, lwu(0x0015ed98u) + 0x3e8u);
        }
        sb(lwu(HB_ADDR(0x0015ffd8u)) + (lwu(pv + 0x28) << 8) + 0x30, 0xff);
        const uint32_t ch = lwu(P + 0x880);
        if (ch != 0xffffffffu) {
            sub_002359f8(ch, 0);
            sw(P + 0x880, 0xffffffffu);
        }
        hud_weapon_element_release();
    }
}

// Trick animations and air/landing transitions on the hoverboard.
// orig: L5:0x00253850 / L16:0x00224fb8
void board_tricks_anim_update() {
    sh(P + 0x888, static_cast<uint32_t>(float_to_int(hero_fall_time(60.0f, lwc1(P + 0x860), -1.0f))));
    if (lbu(P + 0x88d) != 0) {
        const uint32_t btn = lwu(0x0013cae0u);
        if ((btn & 0xfu) != 0) {
            sb(P + 0x88f, 0);
            if ((btn & 0x8u) != 0) {
                sb(P + 0x88f, 1);
            } else if ((btn & 0x1u) != 0) {
                sb(P + 0x88f, 2);
            } else if ((btn & 0x2u) != 0) {
                sb(P + 0x88f, 3);
            }
            const uint32_t hm = lwu(P + 0x2080);
            const uint32_t trick = lbu(P + 0x88f);
            const uint32_t anim = trick + 0x69u;
            if (lbu(hm + 0x53) != anim) {
                hero_anim_set(anim, 2, static_cast<float>(frame_scale_i(5)));
                const uint32_t pair = HB_ADDR(0x00179bc0u) + (lbu(P + 0x88f) << 3);
                sub_002325e0(lw(pair), lw(pair + 4));
            } else {
                if (static_cast<float>(lw(P + 0xab0)) - 2.0f < lwc1(P + 0xaa8)) {
                    sw(P + 0x6e0 + (trick << 2), 1);
                    sw(P + 0x6f8, lwu(P + 0x6f8) + 1u);
                }
            }
        }
    }

    if (lh(P + 0x30e) == 0 || (lwu(0x0013cae0u) & 0xfu) == 0) {
        const uint32_t hm = lwu(P + 0x2080);
        if (static_cast<uint32_t>(lbu(hm + 0x53)) - 0x69u < 4u && lw(P + 0xa9c) == 0) {
            const int32_t cur_f = lbu(hm + 0x51);
            if (lw(P + 0xab0) < cur_f && cur_f < lw(P + 0xab4)) {
                int32_t ft = lh(P + 0x888);
                if (frame_scale_i(0x1e) < ft) {
                    hero_anim_set(board_air_anim(), 9, static_cast<float>(frame_scale_i(0x11)));
                } else {
                    ft = lh(P + 0x888);
                    if (ft < frame_scale_i(0xa)) ft = frame_scale_i(0xa);
                    hero_anim_set(board_air_anim(), 0xd, static_cast<float>(ft));
                }
            }
        }
    }

    if (lh(P + 0x8bc) != 0) {
        const uint32_t cur = lbu(lwu(P + 0x2080) + 0x53);
        if (cur == 0x52 || cur == 0x7d || cur == 0x68 || cur == 0x7e) {
            hero_anim_set(board_air_anim(), 0xb, static_cast<float>(frame_scale_i(0xa)));
        }
    }

    if ((lwu(P + 0xa98) & 2u) != 0 && lh(P + 0x30c) != 0) {
        hero_anim_set(board_air_anim(), 9, static_cast<float>(frame_scale_i(7)));
    }
}

// Boost rings (0x473), boost gates (0x474), and Zoomerator pickups (0x1d6) along the track.
// orig: L5:0x00253b48 / L16:0x002252b0
void board_track_pickups_update() {
    const uint32_t bm = lwu(P + 0x86c);
    if (bm == 0) return;
    const uint32_t pv = lwu(bm + 0x78);

    const int32_t g0 = lw(pv + 0x44);
    if (g0 != -1) {
        uint32_t m = 0;
        group_first_r(m, g0, 0, 0);
        while (m != 0) {
            if (lh(m + 0xa6) == 0x473) {
                if (countdown_u8(m + 0xbc) != 0) {
                    Vec4 d;
                    vec3_sub(d, V(m + 0x10), V(P + 0xd0));
                    Mat4 inv;
                    mat4_identity(inv);
                    mat3_transpose(inv, M(m + 0xc0));
                    mat4_transform(d, d, inv);
                    if (abs_f(d.x) < 0.5f && abs_f(d.y) < u2f(0x4039999au) && abs_f(d.z) < u2f(0x4039999au)) {
                        sw(P + 0x8c4, lwu(P + 0x8c4) + lwu(lwu(m + 0x78)));
                        sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0x3c)));
                        const uint32_t snd_m = lwu(P + 0x86c);
                        if (snd_m != 0) moby_sound_start(1, 0, snd_m);
                    }
                }
            }
            group_next_r(m, m, 0, 0);
        }
    }

    const int32_t period = frame_scale_i(0x32);
    const float pf = int_to_float(period);
    const int32_t rem = ee_rem(lw(HB_ADDR(0x0015f5ccu)), period);
    const float phase = fpu_div(static_cast<float>(rem), pf);
    const float s = sin_vu((phase + phase) * u2f(0x40490fdbu) + u2f(0xc0490fdbu));
    const uint32_t r_col = static_cast<uint32_t>(float_to_int(s * 48.0f)) + 0x68u;
    uint32_t g_col = static_cast<uint32_t>(float_to_int(s * 64.0f)) + 0xd0u;
    const uint32_t b_col = static_cast<uint32_t>(float_to_int(s * 24.0f)) + 0x3cu;
    if (!(static_cast<int32_t>(g_col) < 256)) g_col = 0xffu;

    const int32_t g1 = lw(pv + 0x48);
    if (g1 != -1 && lh(P + 0x8bc) == 0) {
        uint32_t m = 0;
        group_first_r(m, g1, 0, 0);
        if (m != 0) {
            const uint32_t rgba = 0x80000000u | (b_col << 16) | (g_col << 8) | r_col;
            while (m != 0) {
                if (lh(m + 0xa6) == 0x474) {
                    sh(m + 0x34, lhu(m + 0x34) | 0x10u);
                    sw(m + 0x90, rgba);
                    if (countdown_u8(m + 0xbc) != 0) {
                        Vec4 d;
                        vec3_sub(d, V(m + 0x10), V(P + 0x80));
                        Mat4 inv;
                        mat4_identity(inv);
                        mat3_transpose(inv, M(m + 0xc0));
                        mat4_transform(d, d, inv);
                        if (abs_f(d.x) < u2f(0x40333333u) && abs_f(d.y) < 1.25f && abs_f(d.z) < 1.5f) {
                            sw(P + 0x8c4, lwu(P + 0x8c4) + lwu(lwu(m + 0x78)));
                            sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0x3c)));
                            const uint32_t snd_m = lwu(P + 0x86c);
                            if (snd_m != 0) moby_sound_start(1, 0, snd_m);
                        }
                    }
                }
                group_next_r(m, m, 0, 0);
            }
        }
    }

    const int32_t g2 = lw(pv + 0x4c);
    if (g2 != -1) {
        uint32_t m = 0;
        group_first_r(m, g2, 0, 0);
        while (m != 0) {
            if (lh(m + 0xa6) == 0x1d6 && countdown_u8(m + 0xbc) != 0) {
                if (vec3_distance(V(P + 0xd0), V(m + 0x10)) < 1.5f) {
                    sb(m + 0xbc, static_cast<uint32_t>(frame_scale_i(0x12c)));
                    if (lw(P + 0x10b8) != 0x24) {
                        sw(P + 0x20b8, 0x24);
                        sb(P + 0x20ac, 0);
                        hero_slot_update(0);
                        sb(P + 0x20ac, 1);
                    }
                    const uint32_t charges = lbu(P + 0x8ce) + 1u;
                    sb(P + 0x8ce, (charges & 0xffu) < 4u ? charges : 3u);
                    sw(P + 0x20b8, 0x24);
                    sw(0x0013d4b8u, lwu(0x0013d4b8u) + 1u);
                }
            }
            group_next_r(m, m, 0, 0);
        }
    }
}

// Trick spin / combo score tally while airborne.
// orig: L5:0x00254058 / L16:0x002257c0
void board_trick_score_update() {
    if (lbu(0x0013d388u) == 0) return;
    if (lw(P + 0x8a8) != 0) {
        hero_stat_use(0x00141938u);
    }
    const float thresh = u2f(0x40543b67u);
    const float two_pi = u2f(0x40c90fdbu);
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t ang_addr = P + 0x6d0 + i * 4u;
        const uint32_t cnt_addr = P + 0x700 + i * 4u;
        if (thresh < abs_f(lwc1(ang_addr))) {
            float a = lwc1(ang_addr);
            const uint32_t c = lwu(cnt_addr);
            a = 0.0f < a ? a - two_pi : a + two_pi;
            swc1(ang_addr, a);
            sw(cnt_addr, c + 1u);
        }
    }
    sw(P + 0x70c, 0);
    for (uint32_t i = 0; i < 3; ++i) {
        const int32_t c = lw(P + 0x700 + i * 4u);
        if (lw(P + 0x70c) < c) sw(P + 0x70c, static_cast<uint32_t>(c));
    }
    const int32_t max_spins = lw(P + 0x70c);
    sw(P + 0x710, 0);
    if (max_spins > 0) {
        uint32_t sum = 0, step = 0x64;
        for (int32_t k = max_spins; k != 0; --k, step += 0x32) sum += step;
        sw(P + 0x710, sum);
    }
    sw(P + 0x6f0, 0);
    for (uint32_t i = 0; i < 4; ++i) {
        if (lw(P + 0x6e0 + i * 4u) != 0) sw(P + 0x6f0, lwu(P + 0x6f0) + 1u);
    }
    const uint32_t mult_tbl = on_rilgar() ? L5_ADDR(0x002169f8u) : L16_ADDR(0x001e65b8u);
    swc1(P + 0x6f4, lwc1(mult_tbl + (lwu(P + 0x6f0) << 2)));
    const int32_t denom = float_to_int(frame_scale_f(1.0f) * 60.0f);
    const int32_t q = ee_div(lw(P + 0x6f8) * 0x64, denom);
    const int32_t base = float_to_int(static_cast<float>(q)) + lw(P + 0x6f0) * 0x19;
    sw(P + 0x6fc, static_cast<uint32_t>(base));
    const int32_t bonus = float_to_int(static_cast<float>(base) * lwc1(P + 0x6f4));
    sw(P + 0x6fc, static_cast<uint32_t>(bonus));
    sw(P + 0x714, lwu(P + 0x710) + static_cast<uint32_t>(bonus));
}

// Crash / fall / landing checks for states 0x6b and 0x6c; returns 1 when it changed state.
// orig: L5:0x00255208 / L16:0x00226970
int32_t board_crash_and_land_check() {
    const uint32_t OUTB = HB_ADDR(0x00174240u);
    if (lwc1(0x0015ed6cu) * 12.0f < lwc1(P + 0x164)) {
        Vec4 top;
        sub_00233660_v(top, 0.0f, 0.0f, 0.75f);
        Vec4 fwd = V(P + 0xe0);
        vec3_set_length(fwd, fwd, u2f(0x3f0ccccdu));
        vec3_add(fwd, fwd, V(P + 0xe0));
        fwd.z = 0.0f;
        Vec4 end;
        vec3_add(end, top, fwd);
        if (segment_collide_v(top, end, 2, 0, nullptr) != 0 &&
            lw(OUTB + 0x18) == 0 && lw(OUTB + 0x1c) != 0 &&
            u2f(0x3f9c61aau) < atan2_f(lwc1(OUTB + 0x48), vec2_length(V(OUTB + 0x40)))) {
            const float wall_yaw = angle_add(atan2_f(lwc1(OUTB + 0x40), lwc1(OUTB + 0x44)), u2f(0x40490fdbu));
            swc1(P + 0x8a4, wall_yaw);
            if (angle_dist(atan2_f(lwc1(P + 0xe0), lwc1(P + 0xe4)), lwc1(P + 0x8a4)) < u2f(0x3f490fdbu)) {
                hero_set_state(lwc1(P + 0x2dc) < 1.0f ? 0x6fu : 0x6du, 1);
                return 1;
            }
        }
    }

    if (lbu(P + 0x257) != 0) {
        const float actual = vec2_length(V(P + 0x100));
        if (actual < vec2_length(V(P + 0xe0)) * 0.25f) {
            hero_set_state(0x6d, 1);
            return 1;
        }
    }

    if (level_mod_board_free()) {  // enhancement: the board outside a race
        if (level_mod_board_fell(lwc1(P + 0x88))) {
            hero_set_state(0x6e, 1);
            return 1;
        }
        if (lbu(P + 0x12ea) != 0 && lwc1(P + 0x2dc) < u2f(0x3dcccccdu)) {
            hero_set_state(0x6d, 1);
            return 1;
        }
    } else {
        const uint32_t pl = level_mod_board_planet(lwu(0x0015ed84u));
        if (pl == 5u && lwc1(P + 0x88) < u2f(0x4275999au)) {
            swc1(P + 0x8a0, u2f(0x4275999au));
            hero_set_state(0x6e, 1);
            return 1;
        }
        if (pl == 0x10u) {
            if (lwc1(P + 0x88) < u2f(0x42980000u)) {
                swc1(P + 0x8a0, u2f(0x42980000u));
                hero_set_state(0x6e, 1);
                return 1;
            }
            if (lbu(P + 0x12ea) != 0 && lwc1(P + 0x2dc) < u2f(0x3dcccccdu)) {
                hero_set_state(0x6d, 1);
                return 1;
            }
        }
    }

    if (lh(P + 0x30c) == 0) {
        const uint32_t alt = lwu(P + 0x8b4);
        if (alt != 0) {
            const int32_t idx = path_nearest_l1(P + 0x80, alt, 0.0f);  // (Rilgar's 0x2a0260, Kalebo III's 0x271250)
            const int32_t prev = step_index(lwu(P + 0x8b4), idx, -1, 1);
            const uint32_t raw_pl = lwu(0x0015ed84u);
            sh(P + 0x8c8, static_cast<uint32_t>(idx));
            if (raw_pl == 5u) {
                const uint32_t path = lwu(P + 0x8b4);
                const float z_prev = lwc1(path + (static_cast<uint32_t>(prev) << 4) + 0x18);
                const float z_idx = lwc1(path + (static_cast<uint32_t>(idx) << 4) + 0x18);
                const float z_lo = z_idx < z_prev ? z_idx : z_prev;
                const float xy_d = vec2_distance(V(P + 0x80), V(path + (static_cast<uint32_t>(idx) << 4) + 0x10));
                if (0.5f < z_lo - lwc1(P + 0x88) && 4.0f < xy_d) {
                    hero_set_state(0x6d, 1);
                    return 1;
                }
            }
        }

        if (lh(P + 0x30c) == 0 && lh(P + 0x8bc) != 0 && lh(P + 0x89c) == 0) {
            const uint32_t hm = lwu(P + 0x2080);
            const uint32_t cur = lbu(hm + 0x53);
            if ((cur == 0x55 || cur == 0x7f) && lbu(hm + 0x51) < 13u) {
                hero_anim_set(board_air_anim(), 0xe, static_cast<float>(frame_scale_i(7)));
            }
        }
    }

    int32_t landed = static_cast<uint32_t>(lh(P + 0x30c)) < 1u ? 1 : 0;
    if (lbu(P + 0x257) != 0 && abs_f(lwc1(P + 0x218) - lwc1(P + 0x2d8)) < u2f(0x3d4ccccdu)) {
        landed = 1;
    }
    if (!landed) return 0;
    if (lh(P + 0x89c) == 0) return 0;
    if (!(lwc1(P + 0xe8) < 0.0f)) return 0;

    const float max_tilt = u2f(0x3f860a92u);
    if (max_tilt < abs_f(lwc1(P + 0x94)) || max_tilt < abs_f(lwc1(P + 0x90)) ||
        u2f(0x3fb2b8c2u) < angle_dist(lwc1(P + 0x98), lwc1(P + 0x874))) {
        hero_set_state(0x6d, 1);
        return 1;
    }

    const uint32_t hm0 = lwu(P + 0x2080);
    if (static_cast<uint32_t>(lbu(hm0 + 0x53)) - 0x69u < 4u) {
        const float min_f = static_cast<float>(lw(HB_ADDR(0x0015f698u) + (lbu(P + 0x88f) << 2)));
        if (lwc1(P + 0xaa8) < min_f) {
            hero_set_state(0x6d, 1);
            return 1;
        }
    }

    if (lbu(0x0013d388u) != 0) {
        if (lw(0x0015ed84u) == 5 && lw(P + 0x6f0) == 4 && !(lw(P + 0x70c) < 4) && lbu(0x0013d40fu) == 0) {
            sb(0x0013d40fu, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        sw(P + 0x714, 0);
    }

    if (lw(P + 0x2084) == 0x6c) {
        hero_set_state(0x6b, 0);
        const uint32_t cur = lbu(lwu(P + 0x2080) + 0x53);
        if (cur != 0x55 && cur != 0x7f) {
            hero_anim_set(board_air_anim(), 0xe, static_cast<float>(frame_scale_i(7)));
        }
        return 1;
    }

    const uint32_t cur = lbu(lwu(P + 0x2080) + 0x53);
    if (cur != 0x55 && cur != 0x7f) {
        hero_anim_set(board_air_anim(), 0xe, static_cast<float>(frame_scale_i(7)));
    }
    return 0;
}

// Spawn a Zoomerator projectile moby (class 0x5c3).
// orig: L5:0x002d4168 / L16:0x002a7400
uint32_t spawn_zoomerator_projectile(uint32_t owner, const Vec4& pos, uint32_t target, float yaw, float pitch) {
    const uint32_t m = moby_spawn(0x5c3);
    if (m != 0) {
        const uint32_t pv = lwu(m + 0x78);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sb(m + 0x31, 1);
        swc1(m + 0x48, yaw);
        swc1(m + 0x44, pitch);
        sb(m + 0x20, 0);
        sw(pv, owner);
        V(m + 0x10) = pos;
        sw(pv + 0x8, target);
        sw(pv + 0x14, 0);
        sw(pv + 0x18, 0);
        sh(pv + 0x6, 0);
        swc1(pv + 0x10, (lwc1(0x0015ed6cu) * 30.0f) * u2f(0x3f333333u));
        swc1(m + 0x2c, lwc1(m + 0x2c) * u2f(0x40133333u));
        sh(pv + 0x4, static_cast<uint32_t>(frame_scale_i(0x12c)));
    }
    return m;
}

// Kalebo state 0x3e velocity helper.
// orig: L16:0x0020f8b0
void kalebo_3e_velocity() {
    V(P + 0x770) = V(P + 0x100);
    Vec4 dir;
    vec3_set_length(dir, V(P), lwc1(P + 0x194));
    vec3_add(V(P + 0x770), V(P + 0x770), dir);
    V(P + 0xe0) = V(P + 0x770);
    vec3_clamp_length_v(V(P + 0xe0), lwc1(P + 0x85c) + lwc1(P + 0x868));
}

}  // namespace

void hero_hoverboard_moby_sync() {
    if (static_cast<uint32_t>(lw(P + 0x208c) - 0x15) >= 2u) return;
    const uint32_t m = lwu(P + 0x86c);
    if (m == 0) return;
    V(m + 0x10) = V(P + 0x80);
    Vec4 off{};
    off.z = u2f(0x3e570a3du);
    const uint32_t hm = lwu(P + 0x2080);
    mat3_transform(off, off, M(hm + 0xc0));
    Mat4 rot;
    euler_to_mat4_vu(rot, V(HB_ADDR(0x0017ab20u)));
    mat4_transform(off, off, rot);
    vec3_add(V(m + 0x10), V(m + 0x10), off);
    Mat4 base, prod;
    mat3_copy_affine(base, M(hm + 0xc0));
    mat4_mul(prod, base, rot);
    mat3_copy(M(m + 0xc0), prod);
}

void hero_hoverboard_pair_table() {
    const uint32_t m = lwu(P + 0x86c);
    if (m != 0 && static_cast<uint32_t>(lw(P + 0x208c) - 0x15) < 2u) {
        sw(P + 0x2278, m);
        sw(P + 0x227c, HB_ADDR(0x0017c068u));
    }
}

// Zoomerator (item 0x24) in hero_item_use.
// orig: L5:0x0025265c / L16:0x00223dc4
void hero_item_use_zoomerator(uint32_t mask) {
    if (lbu(P + 0x8ce) != 0 && lhu(0x00141d80u) == 0 && lhu(0x00141920u) == 0) {
        const uint16_t raw = static_cast<uint16_t>(lhu(P + 0x8ca) + 1u);
        sh(P + 0x8ca, raw);
        const int32_t cur = static_cast<int16_t>(raw);
        const int32_t lim = float_to_int(static_cast<float>(frame_scale_i(0x14)) * 60.0f);
        if (lim < cur) sub_00203908(0x3e81, 0x83);
    }
    const uint32_t slot_moby = lwu(P + 0x1090);
    if (slot_moby == 0) return;
    if (countdown_u8(P + 0x8cf) == 2) {
        const uint32_t c = lbu(P + 0x8ce);
        if (c == 0) return;
        sb(P + 0x8ce, c - 1u);
    }
    if (lbu(P + 0x8ce) == 0 || lbu(P + 0x8cf) != 0) return;
    if (pad_button_history(mask, frame_scale_i(7), nullptr) == 0) return;

    uint32_t target = 0;
    const uint32_t bm = lwu(P + 0x86c);
    if (bm != 0) {
        const uint32_t pv = lwu(bm + 0x78);
        float best_score = u2f(0x4b18967fu);
        for (int32_t i = 0; i < 5; ++i) {
            const int32_t idx = lw(pv + 0x2c + static_cast<uint32_t>(i) * 4u);
            if (idx == -1) continue;
            const uint32_t m = lwu(HB_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(idx) << 8);
            const float yaw_to = atan2_f(lwc1(m + 0x10) - lwc1(P + 0x80), lwc1(m + 0x14) - lwc1(P + 0x84));
            const float dyaw = angle_dist(lwc1(P + 0x98), yaw_to);
            if (u2f(0x3f490fdbu) < dyaw) continue;
            const float dxy = vec2_distance(V(P + 0x80), V(m + 0x10));
            const float dpitch = angle_dist(atan2_f(dxy, lwc1(m + 0x18) - lwc1(P + 0x88)), 0.0f);
            if (u2f(0x3f490fdbu) < dpitch) continue;
            const float d2 = vec2_distance(V(m + 0x10), V(P + 0x80));
            const float yterm = d2 * dyaw;
            const float pterm = d2 * dpitch;
            const float score = (d2 + (yterm + yterm)) + (pterm + pterm);
            if (target == 0 || score < best_score) {
                target = m;
                best_score = score;
            }
        }
    }

    sb(slot_moby + 0xbc, static_cast<uint32_t>(frame_scale_i(7)));
    sb(P + 0x8cf, static_cast<uint32_t>(frame_scale_i(0x32)));
    hero_stat_use(0x00141920u);
    moby_sound_start(0, 0, slot_moby);
    Vec4 fire_pos;
    moby_chain_point_v(slot_moby, 0, fire_pos);
    spawn_zoomerator_projectile(slot_moby, fire_pos, target, lwc1(P + 0x98), lwc1(P + 0x94));
}

// Kalebo state 0x3e set_state.
// orig: L16:0x0021f060
void hero_state_3e_set(int32_t mode) {
    sw(P + 0x208c, 0x15);
    sw(P + 0x2284, 8);
    sb(P + 0x20a7, 1);
    V(P + 0x770) = V(P);
    swc1(P + 0x860, lwc1(0x0015ed70u) * 18.0f);
    if (mode != 0) {
        hero_anim_set(0x52, 0, static_cast<float>(frame_scale_i(5)));
    }
}

// Kalebo state 0x3e state_update.
// orig: L16:0x00216204
void hero_state_3e_update() {
    Vec4 dir;
    vec3_set_length(dir, V(P + 0x100), 1.0f);
    const float dot_n = vec3_dot(V(P + 0x290), dir);
    const int32_t steep = dot_n < u2f(0xbf4ccccdu) ? 1 : 0;
    const int32_t half_steep = dot_n < -0.5f ? 1 : 0;

    const float proj = vec3_dot(V(P + 0x270), V(P + 0x770));
    Vec4 along;
    vec3_set_length(along, V(P + 0x270), proj);
    vec3_sub(V(P + 0x770), V(P + 0x770), along);
    vec3_set_length(V(P + 0x770), V(P + 0x770), 1.0f);

    if (lh(P + 0x30e) == 0) {
        if (lbu(P + 0x88e) != 0) {
            sh(P + 0x886, static_cast<uint32_t>(frame_scale_i(0xf)));
            V(P + 0x750) = V(P + 0x100);
        }
        sb(P + 0x88e, 0);
        if (lh(P + 0x30e) == 0) {
            V(P + 0x760) = V(P + 0x80);
            const float accel = lwc1(0x0015ed70u) * 10.0f;
            swc1(P + 0x190, lwc1(P + 0x85c) + lwc1(P + 0x868));
            sub_00212088(accel, accel);
            kalebo_3e_velocity();
        }
    }
    if (lh(P + 0x30e) != 0) {
        if (lbu(P + 0x88e) == 0) {
            sub_002120d8(u2f(0x47c34f80u));
        } else {
            V(P + 0xe0) = V(P + 0x100);
            Vec4 corr;
            vec3_sub(corr, V(P + 0x760), V(P + 0x80));
            sub_002338d0_v(corr, corr);
            const float max_corr = lwc1(0x0015ed6cu) * u2f(0x3ecccccdu);
            if (max_corr < vec3_length(corr)) {
                vec3_set_length(corr, corr, max_corr);
                vec3_add(V(P + 0x80), V(P + 0x80), corr);
            }
        }
    }

    if (lh(P + 0x30e) != 0 && lh(P + 0x88a) == 0 && vec3_dot(V(P + 0x290), V(P + 0x100)) < 0.0f) {
        if (steep != 0 || (half_steep != 0 && lbu(P + 0x88c) != 0)) {
            sb(P + 0x88e, 1);
            V(P + 0xe0) = V(P + 0x290);
            vec3_scale(V(P + 0xe0), V(P + 0xe0), -lwc1(P + 0x85c));
            const float k = lwc1(0x0015ed6cu);
            float push = k * 1.5f;
            if (lbu(P + 0x88c) != 0) push = push + k * 3.0f;
            Vec4 lift;
            vec3_set_length(lift, V(P + 0x290), -push);
            vec3_add(V(P + 0xe0), V(P + 0xe0), lift);
            V(P + 0x100) = V(P + 0xe0);
        }
        sb(P + 0x88c, 0);
        swc1(P + 0x854, -vec3_dot(V(P + 0x290), V(P + 0x100)));
    }

    const uint16_t air = lhu(P + 0x30e);
    sh(P + 0x88a, air);
    if (static_cast<int16_t>(air) == 0) {
        fx_params_set_0(u2f(0x3c03126fu), u2f(0x3dcccccdu));
        const float tilt = lwc1(P + 0x184) * -12.0f;
        const uint32_t fx = L16_ADDR(0x0017aae0u);
        swc1(fx + 0x60, u2f(0x3f0efa35u) < tilt ? u2f(0x3f0efa35u) : tilt);
        if (tilt < u2f(0xbf0efa35u)) swc1(fx + 0x60, u2f(0xbf0efa35u));
    }
    if (lh(P + 0x30e) != 0) {
        if (lbu(P + 0x88e) == 0) {
            fx_params_set_0(u2f(0x3c75c28fu), u2f(0x3da3d70au));
            Vec4 loc;
            mat4_transform(loc, V(P + 0x100), M(P + 0x40));
            const float pitch = -atan2_f(vec2_length(loc), loc.z);
            const uint32_t fx = L16_ADDR(0x0017aae0u);
            swc1(fx + 0x64, u2f(0x3f1c61aau) < pitch ? u2f(0x3f1c61aau) : pitch);
            if (lwc1(fx + 0x64) < u2f(0xbf1c61aau)) swc1(fx + 0x64, u2f(0xbf1c61aau));
        }
    }

    if (lh(P + 0x30e) == 0) {
        V(P + 0x740) = V(P + 0x270);
        if (lh(P + 0x30e) == 0 && pad_button_history(0x40, frame_scale_i(9), nullptr) != 0) {
            if (steep != 0 || half_steep != 0) {
                sb(P + 0x88c, 1);
            } else {
                const float nd = -vec3_dot(V(P + 0x290), V(P + 0x100));
                float hop = lwc1(0x0015ed6cu) * 5.0f;
                if (0.0f < nd) hop = hop + nd;
                swc1(P + 0x854, hop);
                sub_002338d0(P + 0xe0, P + 0xe0);
                sub_00233810(P + 0xe0, P + 0xe0, lwc1(P + 0x854));
                sh(P + 0x30e, 1);
            }
        }
    }

    float st[2];
    board_stick_smooth(st, P + 0x8d0, lwc1(0x0015ed64u) * u2f(0x3dcccccdu));
    if (lbu(P + 0x88e) != 0) {
        sub_00211cf8(0.0f, 0.0f, st[0] * (lwc1(0x0015ed6cu) * u2f(0x40c90fdbu)));
    } else if (tick_i16(P + 0x886) == 0) {
        V(P + 0x170) = V(P + 0x750);
        const float dt2 = lwc1(0x0015ed64u);
        sub_00211e30(0, dt2 * u2f(0x3d23d70au), dt2 * u2f(0x3e99999au), lwc1(0x0015ed6cu) * u2f(0x40df66f3u));
    } else if (u2f(0x3e4ccccdu) < lwc1(P + 0x229c)) {
        const float vmax = (lwc1(0x0015ed6cu) * u2f(0x3fb2b8c2u)) * lwc1(P + 0x229c);
        swc1(P + 0x180, atan2_f(-st[1], -st[0]));
        hero_stick_move(0, 1.0f);
        const float dt2 = lwc1(0x0015ed64u);
        sub_00211e30(0, dt2 * u2f(0x3b83126fu), dt2 * u2f(0x3e4ccccdu), vmax);
    } else {
        sw(P + 0x184, 0);
    }

    const int32_t air2 = lh(P + 0x30e);
    if (air2 == 0) {
        const float target_spd = lwc1(0x0015ed6cu) * 20.0f;
        const float slope_spd = -vec3_dot(V(P + 0x100), V(P + 0x290));
        if (lwc1(0x0015ed6cu) * 0.5f < abs_f(slope_spd)) {
            if (slope_spd < 0.0f) {
                swc1(P + 0x85c, lwc1(P + 0x85c) + slope_spd * u2f(0x3a03126fu));
            }
            const float hi = lwc1(0x0015ed6cu) * 27.0f;
            if (hi < lwc1(P + 0x85c)) swc1(P + 0x85c, hi);
            const float lo = lwc1(0x0015ed6cu) * 11.0f;
            if (lwc1(P + 0x85c) < lo) swc1(P + 0x85c, lo);
        }
        const float dt = lwc1(0x0015ed70u);
        float delta = target_spd - lwc1(P + 0x85c);
        const float max_up = dt * 9.0f;
        if (max_up < delta) delta = max_up;
        const float max_dn = dt * u2f(0xbf333333u);
        if (delta < max_dn) delta = max_dn;
        swc1(P + 0x85c, lwc1(P + 0x85c) + delta);
    } else if (frame_scale_i(8) < air2 && lbu(P + 0x88e) != 0) {
        const float cur = lwc1(P + 0x85c);
        const float diff = lwc1(P + 0x160) - cur;
        if (0.0f < diff) swc1(P + 0x85c, cur + diff);
    }

    if (lh(P + 0x30e) != 0) {
        sub_002338d0(P + 0xe0, P + 0xe0);
        const float vz = lwc1(P + 0x854) - lwc1(P + 0x860);
        swc1(P + 0x854, vz);
        sub_002334d0(P + 0xe0, P + 0xe0, -vz);
    } else {
        sub_002334d0(P + 0xe0, P + 0xe0, lwc1(0x0015ed70u) * 20.0f);
    }
}

// Hoverboard states 0x6b..0x6f in hero_set_state.
// orig: L5:0x0024dbb8..0x0024e0c8 / L16:0x0021f0c4..0x0021f5d4
void hero_hoverboard_set_state(uint32_t state, int32_t mode) {
    switch (state) {
        case 0x6b: {
            sw(P + 0x208c, 0x16);
            const uint32_t prev_st = lwu(P + 0x2090);
            sw(P + 0x2284, 8);
            sb(P + 0x20b2, 1);
            sb(P + 0x20a7, 1);
            sh(P + 0x89c, 0);
            sh(P + 0x8bc, 0);
            if (prev_st == 0x6d || prev_st == 0x6f || prev_st == 0x6e) {
                const uint32_t alt = lwu(P + 0x8b4);
                if (alt != 0) {
                    const int32_t idx = path_nearest_l1(P + 0x80, alt, 0.0f);  // (Rilgar's 0x2a0260, Kalebo III's 0x271250)
                    if (idx < lh(P + 0x898)) sh(P + 0x898, static_cast<uint32_t>(idx));
                }
                sb(P + 0x8af, 0);
                sh(P + 0x8bc, 0);
                sh(P + 0x89c, 0);
                sw(P + 0x8c4, 0);
                sw(P + 0x8b8, 0);
                sb(P + 0x8ce, 0);
            }
            if (lw(P + 0x2094) == 0x16) {
                swc1(P + 0x854, -vec3_dot(V(P + 0x290), V(P + 0x100)));
            } else {
                if (lw(0x0015ed84u) == 5) {
                    swc1(P + 0x80, u2f(0x43770000u));
                    swc1(P + 0x84, u2f(0x438f8000u));
                    swc1(P + 0x88, u2f(0x42980000u));
                    swc1(P + 0x98, u2f(0xc0066666u));
                }
                const uint32_t bm = lwu(P + 0x86c);
                mem_fill32(P + 0x6c0, 0, 0x210);
                sw(P + 0x86c, bm);
                const uint32_t pv = lwu(bm + 0x78);
                sw(P + 0x890, lwu(HB_ADDR(0x0015ffd8u)) + (lwu(pv + 0x28) << 8));
                swc1(P + 0x860, lwc1(0x0015ed70u) * 22.0f);
                swc1(P + 0x87c, u2f(0x3f9c61aau));
                sw(P + 0x880, 0xffffffffu);
                if (lbu(0x0013d388u) != 0) {
                    board_race_hud_reset();
                    const uint32_t cb0 = on_rilgar() ? L5_ADDR(0x00262ae8u) : L16_ADDR(0x002351f8u);
                    const uint32_t cb1 = on_rilgar() ? L5_ADDR(0x00262b58u) : L16_ADDR(0x00235268u);
                    const uint32_t cb2 = on_rilgar() ? L5_ADDR(0x00262f50u) : L16_ADDR(0x00235660u);
                    const uint32_t dur = static_cast<uint32_t>(float_to_int(static_cast<float>(frame_scale_i(0x11)) * 60.0f));
                    sw(P + 0x880, static_cast<uint32_t>(camera_request(0x10, 0xffff, cb0, cb1, cb2, P + 0x864, dur)));
                }
                if (level_mod_board_hud_off()) {  // enhancement: the board outside a race
                    sw(P + 0x874, lwu(P + 0x98));
                } else {
                    const uint32_t cb3 = on_rilgar() ? L5_ADDR(0x002661a0u) : L16_ADDR(0x002388b0u);
                    const uint32_t cb4 = on_rilgar() ? L5_ADDR(0x002661e8u) : L16_ADDR(0x002388f8u);
                    const uint32_t cb5 = on_rilgar() ? L5_ADDR(0x00266320u) : L16_ADDR(0x00238a30u);
                    const uint32_t cb6 = on_rilgar() ? L5_ADDR(0x00266710u) : L16_ADDR(0x00238e20u);
                    sw(P + 0x874, lwu(P + 0x98));
                    camera_request(5, 0xffff, cb3, cb4, cb5, 0, 0);
                    camera_request(7, 0xffff, cb3, cb4, cb6, 0, 0);
                }
            }
            if (mode != 0) {
                const uint32_t anim = lbu(P + 0x8ce) != 0 ? 0x7du : 0x52u;
                hero_anim_set(anim, 0, static_cast<float>(frame_scale_i(5)));
            }
            break;
        }
        case 0x6c: {
            sw(P + 0x208c, 0x16);
            sw(P + 0x2284, 8);
            sb(P + 0x20a7, 1);
            sw(P + 0x870, 0);
            if (mode != 0) {
                const uint32_t anim = board_air_anim();
                hero_anim_set(anim, 5, static_cast<float>(frame_scale_i(8)));
            }
            break;
        }
        case 0x6d: {
            sb(P + 0x20a7, 1);
            sw(P + 0x2284, 0);
            sub_00206c08();
            sw(P + 0x8c4, 0);
            vec3_scale(V(P + 0xe0), V(P + 0x110), u2f(0x3f333333u));
            swc1(P + 0xe8, lwc1(0x0015ed6cu) * 10.0f);
            hero_anim_set(0x61, 3, static_cast<float>(frame_scale_i(4)));
            break;
        }
        case 0x6e: {
            sb(P + 0x20a7, 1);
            sw(P + 0x2284, 0);
            sub_00206c08();
            sw(P + 0x8c4, 0);
            sh(P + 0x8be, static_cast<uint32_t>(frame_scale_i(0x46)));
            swc1(P + 0x2f0, lwc1(P + 0x8a0));
            const int32_t spk = float_to_int(abs_f(lwc1(P + 0x108)) * 300.0f);
            hero_land_fx(3, spk < 41 ? spk : 0x28, 1);
            hero_sound_start(3, 0);
            sh(P + 0x90a, static_cast<uint32_t>(frame_scale_i(0x4b)));
            const float dip = lwc1(P + 0x108) * u2f(0x3e99999au);
            const float dip_min = lwc1(0x0015ed6cu) * u2f(0xc0b66666u);
            swc1(P + 0x694, dip < dip_min ? dip_min : dip);
            const float water_z = lwc1(P + 0x8a0);
            swc1(P + 0x69c, water_z);
            sw(P + 0x6a0, 0);
            const float bob = (lwc1(P + 0x88) - water_z) - u2f(0xbdf5c28fu);
            swc1(P + 0x690, u2f(0x3e4ccccdu) < bob ? u2f(0x3e4ccccdu) : bob);
            if (bob < u2f(0xbe4ccccdu)) swc1(P + 0x690, u2f(0xbe4ccccdu));
            vec_clear(V(P + 0xe0));
            V(P + 0x150) = V(P + 0x110);
            sub_002338d0(P + 0x150, P + 0x150);
            vec3_clamp_length_v(V(P + 0x150), lwc1(0x0015ed6cu) * 9.5f);
            sw(P + 0xff8, 0x68);
            if (mode != 0) {
                hero_anim_set(0x3a, 0, static_cast<float>(frame_scale_i(0x11)));
            }
            break;
        }
        case 0x6f: {
            sb(P + 0x20a7, 1);
            sw(P + 0x2284, 0);
            sub_00206c08();
            sw(P + 0x8c4, 0);
            sw(P + 0xff8, 0x68);
            if (mode != 0) {
                hero_anim_set(0x62, 4, static_cast<float>(frame_scale_i(4)));
            }
            break;
        }
        default:
            break;
    }
}

// Hoverboard states 0x6b..0x6f in hero_state_update.
// orig: L5:0x00246744..0x002475bc / L16:0x00216a54..0x002178cc
void hero_hoverboard_state_update(uint32_t state) {
    switch (state) {
        case 0x6b:
        case 0x6c: {
            if (lwc1(P + 0x80) < 5.0f || lwc1(P + 0x84) < 5.0f) {
                board_place_at_respawn();
            }

            if (lh(P + 0x8bc) == 0) {
                if (u2f(0x3e99999au) < lwc1(P + 0x8b8) - lwc1(P + 0x2d8)) sh(P + 0x8bc, 1);
            } else if (lh(P + 0x30c) == 0 && lh(P + 0x89c) == 0) {
                sh(P + 0x8bc, 0);
            }
            swc1(P + 0x8b8, lwc1(P + 0x2d8));

            const bool boost_chg = lh(P + 0x884) != 0 && lwc1(P + 0x868) != lwc1(0x0015ed6cu) * 11.0f;
            if (boost_chg && lw(P + 0x17ac) == 0) {
                sub_00262500(lwu(P + 0x2080), P + 0x1670);
                sub_00262528(P + 0x1670, 0x28, 1);
                sub_00262528(P + 0x1670, 0x14, 2);
                sub_00262528(P + 0x1670, 0xa, 3);
            }
            const int32_t fade = (lh(P + 0x884) != 0 && lwc1(P + 0x868) != lwc1(0x0015ed6cu) * 11.0f) ? 0 : 1;
            trail_update(P + 0x1670, fade);

            const uint32_t bm = lwu(P + 0x86c);
            if (bm != 0) {
                if (lh(P + 0x884) != 0) {
                    hero_sound_slot(7, bm, 2);
                } else {
                    const int32_t s7 = lw(P + 0x2234);
                    if (s7 != -1) {
                        const uint32_t ch = 0x0013e550u + static_cast<uint32_t>(s7 * 0x70);
                        if (lwu(ch + 0x88) == bm && lbu(ch + 0x74) != 0) sub_0028d918(s7);
                    }
                    sw(P + 0x2234, 0xffffffffu);
                }
                if (lh(P + 0x8bc) == 0) {
                    hero_sound_slot(6, lwu(P + 0x86c), 0);
                } else {
                    const int32_t s6 = lw(P + 0x2230);
                    if (s6 != -1) {
                        const uint32_t ch = 0x0013e550u + static_cast<uint32_t>(s6 * 0x70);
                        if (lwu(ch + 0x88) == lwu(P + 0x86c) && lbu(ch + 0x74) != 0) sub_0028d918(s6);
                    }
                    sw(P + 0x2230, 0xffffffffu);
                }
            }

            const uint32_t trick_btn = lh(P + 0x89c) != 0 && (lwu(0x0013cae0u) & 0xfu) != 0 ? 1u : 0u;
            sb(P + 0x88d, trick_btn);

            const bool wants_boost = (((lwu(0x0013cae0u) & 0x80u) != 0 || lh(P + 0x8cc) != 0) && lw(P + 0x864) != 0) ||
                                     lw(P + 0x8c4) != 0;
            if (wants_boost) {
                tick_i16(P + 0x8cc);
                if (lh(P + 0x884) == 0) sh(P + 0x8cc, static_cast<uint32_t>(frame_scale_i(0x14)));
                const float dt2 = lwc1(0x0015ed64u);
                const float dt1 = lwc1(0x0015ed6cu);
                sh(P + 0x884, 1);
                spring_to(P + 0x87c, P + 0x858, u2f(0x3f9c61aau), dt2 * u2f(0x3c23d70au), dt2 * u2f(0x3e99999au),
                          dt1 * u2f(0x3f32b8c2u));
                approach_f(P + 0x868, lwc1(0x0015ed6cu) * 11.0f, lwc1(0x0015ed70u) * 20.0f);
                if (lw(P + 0x8c4) != 0) {
                    tick_i32(P + 0x8c4);
                } else {
                    tick_i32(P + 0x864);
                }
                const uint32_t bm2 = lwu(P + 0x86c);
                if (bm2 != 0) sb(bm2 + 0xbc, 1);
            } else {
                const float dt2 = lwc1(0x0015ed64u);
                const float dt1 = lwc1(0x0015ed6cu);
                sh(P + 0x884, 0);
                spring_to(P + 0x87c, P + 0x858, u2f(0x3f9c61aau), dt2 * u2f(0x3be56042u), dt2 * u2f(0x3e99999au),
                          dt1 * u2f(0x3f060a92u));
                approach_f(P + 0x868, 0.0f, lwc1(0x0015ed70u) * 10.0f);
            }

            Mat4 unused_mat;
            if (lh(P + 0x30e) == 0) {
                if (lwc1(P + 0x2dc) < 1.0f) {
                    vec3_set_length(V(P + 0x790), V(P + 0x100), 1.0f);
                    V(P + 0x780) = V(P + 0x270);
                    vec3_set_length(V(P + 0x780), V(P + 0x780), 1.0f);
                }
                const float dt = lwc1(0x0015ed70u);
                align_to_up_v(V(P + 0x780), unused_mat, 0, dt * u2f(0x42517084u), dt * u2f(0x3b64c388u), 0.0f);
            } else if (lbu(P + 0x88d) == 0) {
                vec_clear(V(P + 0x780));
                const float dt = lwc1(0x0015ed70u);
                const float dt2 = lwc1(0x0015ed64u);
                swc1(P + 0x788, 1.0f);
                align_to_up_v(V(P + 0x780), unused_mat, 0, dt * u2f(0x422e886eu), dt2 * u2f(0x3e4ccccdu), 0.0f);
            }

            const float dt0 = lwc1(0x0015ed70u);
            if (lbu(P + 0x257) != 0) {
                approach_f(P + 0x85c, 0.0f, dt0 * 5.0f);
            } else {
                approach_f(P + 0x85c, lwc1(0x0015ed6cu) * 15.0f, dt0 * 17.0f);
            }
            const float accel = lwc1(0x0015ed70u) * 36.0f;
            swc1(P + 0x190, lwc1(P + 0x85c) + lwc1(P + 0x868));
            sub_00212088(accel, accel);

            if (lh(P + 0x30e) == 0) {
                swc1(P + 0x874, lwc1(P + 0x98));
                sw(P + 0x878, 0);
                sub_002120d8(u2f(0x47c34f80u));
            } else {
                sub_002120d8(lwc1(P + 0x874));
                if (lbu(P + 0x88d) == 0 && abs_f(lwc1(P + 0x94)) < u2f(0x3f32b8c2u) &&
                    abs_f(lwc1(P + 0x90)) < u2f(0x3f32b8c2u)) {
                    const float dt2 = lwc1(0x0015ed64u);
                    spring_angle(P + 0x98, P + 0x878, 0, lwc1(P + 0x874), dt2 * u2f(0x3d23d70au),
                                 dt2 * u2f(0x3e99999au), lwc1(0x0015ed6cu) * u2f(0x40df66f3u));
                }
            }

            if (lh(P + 0x30c) != 0 && lbu(P + 0x88d) != 0) {
                sw(P + 0x878, 0);
                sw(P + 0x184, 0);
                vec_clear(V(P + 0x720));
            } else {
                float st[2];
                board_stick_smooth(st, P + 0x720, lwc1(0x0015ed64u) * u2f(0x3e2e147bu));
                const float dir = angle_add(lwc1(HB_ADDR(0x001671d8u)), atan2_f(-st[1], st[0]));
                const float stick_mag = lwc1(P + 0x229c);
                swc1(P + 0x180, dir);
                if (u2f(0x3e4ccccdu) < stick_mag) {
                    const float dt1 = lwc1(0x0015ed6cu);
                    const float dt2 = lwc1(0x0015ed64u);
                    float k = dt2 * u2f(0x3ba3d70au);
                    const float d = dt2 * u2f(0x3e99999au);
                    float vmax = (dt1 * u2f(0x403de44eu)) * stick_mag;
                    if (lh(P + 0x30c) != 0) {
                        k = dt2 * u2f(0x3b727bb3u);
                        vmax = (dt1 * u2f(0x3fdf66f3u)) * stick_mag;
                    }
                    const uint32_t sp = ee_stack_alloc(0x10);
                    swc1(sp, lwc1(P + 0x184));
                    sub_00211e30(0, k, d, vmax);
                    if (lh(P + 0x30e) != 0) {
                        spring_angle(P + 0x874, sp, 0, lwc1(P + 0x180), k, d, vmax);
                    }
                    ee_stack_free(0x10);
                }
            }

            const float lean_turn = lwc1(P + 0x229c) < u2f(0x3e4ccccdu) ? 0.0f : lwc1(P + 0x184);
            fx_params_set_3(u2f(0x3ca3d70au), u2f(0x3e4ccccdu));
            const uint32_t fx = HB_ADDR(0x0017aae0u);
            const float r1 = lean_turn * -22.0f;
            const float r0 = lean_turn * 37.0f;
            swc1(HB_ADDR(0x0017ad54u), r1);
            swc1(HB_ADDR(0x0017ad58u), r0);
            if (0.0f < lean_turn) swc1(HB_ADDR(0x0017ad50u), r1);

            fx_params_set_0(u2f(0x3c03126fu), u2f(0x3dcccccdu));
            const float tilt = lwc1(fx + 0x60) + lean_turn * -11.0f;
            swc1(fx + 0x60, u2f(0x3f29c91fu) < tilt ? u2f(0x3f29c91fu) : tilt);
            if (tilt < u2f(0xbf29c91fu)) swc1(fx + 0x60, u2f(0xbf29c91fu));

            fx_params_set_1(u2f(0x3c449ba6u), u2f(0x3e6147aeu));
            swc1(fx + 0x114, lean_turn * 25.0f);
            swc1(fx + 0x110, lean_turn * -10.0f);

            if (lh(P + 0x30e) != 0 && lbu(P + 0x88d) != 0) {
                const Vec4 pivot = V(HB_ADDR(0x00179be0u) + (lbu(P + 0x88f) << 4));
                sw(P + 0x184, 0);
                float ts[2];
                board_stick_smooth(ts, P + 0x6c0, lwc1(0x0015ed64u) * u2f(0x3e4ccccdu));
                const float rate = lwc1(0x0015ed6cu) * u2f(0x41437a14u);
                const float dx = -ts[0] * rate;
                const float dy = -ts[1] * rate;
                swc1(P + 0x6d0, lwc1(P + 0x6d0) + dx);
                swc1(P + 0x6d4, lwc1(P + 0x6d4) + dy);
                sub_002090d0_v(pivot, dx, dy, 0.0f);
            }

            board_race_progress();

            const float base_vz = lbu(P + 0x257) == 0 ? lwc1(P + 0x108) : lwc1(P + 0x128);
            swc1(P + 0xe8, base_vz - lwc1(P + 0x860));
            if (lh(P + 0x30e) == 0) {
                const float vz0 = lwc1(P + 0x108);
                if (vz0 < 0.0f) {
                    if (lwc1(P + 0xe8) < vz0) swc1(P + 0xe8, vz0);
                } else {
                    const float floor_cap = -lwc1(P + 0x860) * 1.5f;
                    if (lwc1(P + 0xe8) < floor_cap) swc1(P + 0xe8, floor_cap);
                }
            }

            if (lh(P + 0x89c) == 0 && lh(P + 0x30e) < frame_scale_i(5) && lwc1(P + 0x2dc) < 0.5f) {
                swc1(P + 0xe8, lwc1(P + 0xe8) - lwc1(0x0015ed70u) * 20.0f);
            }
            if (lwc1(P + 0x88) < lwc1(P + 0x2d8)) swc1(P + 0x88, lwc1(P + 0x2d8));

            Vec4 center;
            vec3_scale(center, V(P + 0x100), 3.0f);
            vec3_add(center, center, V(P + 0xd0));
            int32_t nhits = mobys_in_sphere_v(center, u2f(0x3f333333u), 0, lwu(P + 0x2080), nullptr);
            if (nhits != 0) {
                Vec4 push;
                vec3_scale(push, V(P + 0x100), 7.0f);
                if (nhits > 0) {
                    uint32_t list = HB_ADDR(0x00178300u);
                    for (; nhits != 0; --nhits, list += 4) {
                        const uint32_t m = lwu(list);
                        if (moby_in_range_class_500(m) != 0 || lh(m + 0xa6) == 0x85) {
                            sub_00259bc8_v(m, lwu(P + 0x2080), 0x10000u, P + 0x80, push, 1.0f);
                        }
                    }
                }
            }
            break;
        }
        case 0x6d: {
            const uint32_t bm = lwu(P + 0x86c);
            if (bm != 0) sb(bm + 0xbc, 2);
            float spd = vec2_length(V(P + 0xe0));
            approach_f_v(spd, 0.0f, lwc1(0x0015ed70u) * 15.0f);
            vec2_set_length(V(P + 0xe0), V(P + 0xe0), spd);
            const float g = lwc1(0x0015ed70u) * 27.0f;
            sub_002334d0(P + 0xe0, P + 0x120, g);
            const int32_t ft = float_to_int(hero_fall_time(60.0f, g, -1.0f));
            if (ft != 0 && 1.0f < static_cast<float>(0x10) - lwc1(P + 0xaa8)) {
                sub_00208f20(static_cast<float>(0x10), static_cast<float>(ft), 0.5f, -1.0f);
                if (u2f(0x3f99999au) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x3f99999au));
                if (lwc1(P + 0xa90) < u2f(0x3ea8f5c3u)) swc1(P + 0xa90, u2f(0x3ea8f5c3u));
                if (lwc1(P + 0x2dc) < 0.5f && lwc1(P + 0xa90) < 1.0f) swc1(P + 0xa90, 1.0f);
            } else {
                swc1(P + 0xa90, 1.0f);
            }
            const float rate = u2f(0x40c37a14u);
            turn_towards(P + 0x94, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            turn_towards(P + 0x90, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            break;
        }
        case 0x6e: {
            const uint32_t bm = lwu(P + 0x86c);
            if (bm != 0) sb(bm + 0xbc, 2);
            const float rate = u2f(0x40c37a14u);
            vec_clear(V(P + 0xe0));
            sub_00216710(lwc1(0x0015ed70u) * 4.5f);
            sub_00227d40();
            turn_towards(P + 0x94, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            turn_towards(P + 0x90, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            break;
        }
        case 0x6f: {
            const float rate = u2f(0x40c37a14u);
            vec_clear(V(P + 0xe0));
            turn_towards(P + 0x94, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            turn_towards(P + 0x90, 0, 0.0f, lwc1(0x0015ed6cu) * rate);
            turn_towards(P + 0x98, 0, lwc1(P + 0x8a4), lwc1(0x0015ed6cu) * u2f(0x410ba058u));
            break;
        }
        default:
            break;
    }
}

// Hoverboard states 0x6b..0x6f in hero_input_update; returns true to bypass hero_input_update's epilogue.
// orig: L5:0x0025a160..0x0025a954 / L16:0x0022b660..0x0022be54
bool hero_hoverboard_input_update(uint32_t state) {
    switch (state) {
        case 0x6b: {
            tick_i16(P + 0x89e);
            if (board_crash_and_land_check() != 0) return false;

            if (lh(P + 0x89c) != 0) {
                if (lh(P + 0x30e) == 0) {
                    sh(P + 0x8bc, 0);
                    sh(P + 0x89c, 0);
                    sh(P + 0x89e, static_cast<uint32_t>(frame_scale_i(5)));
                }
                if (lh(P + 0x89c) != 0) sh(P + 0x89c, lhu(P + 0x89c) + 1u);
            }

            swc1(P + 0x7d0 + static_cast<uint32_t>(lh(P + 0x8b2) << 2), lwc1(P + 0x2dc));
            const uint16_t raw_cnt = static_cast<uint16_t>(lhu(P + 0x8b0) + 1u);
            sh(P + 0x8b0, raw_cnt);
            if (!(static_cast<int16_t>(raw_cnt) < 33)) sh(P + 0x8b0, 0x20);
            const int32_t b2 = lh(P + 0x8b2);
            const int32_t b2_next = b2 + 1;
            const int32_t b2_div = (-1 < b2_next ? b2_next : b2 + 0x20) >> 5;
            sh(P + 0x8b2, static_cast<uint32_t>(b2_next - (b2_div << 5)));

            float max_drop = 0.0f;
            for (int32_t i = 0; i < frame_scale_i(0x1e); ++i) {
                const int32_t cur_b2 = lh(P + 0x8b2) - i;
                const int32_t lim_b0 = lh(P + 0x8b0) - 1;
                const int32_t sh_in = cur_b2 + 0x20;
                const int32_t q = (-1 < sh_in ? sh_in : cur_b2 + 0x3f) >> 5;
                int32_t slot = sh_in - (q << 5);
                if (lim_b0 < slot) slot = lim_b0;
                const float d = lwc1(P + 0x7d0 + (static_cast<uint32_t>(slot) << 2));
                if (max_drop < d) max_drop = d;
            }

            Vec4 vel_up = V(P + 0x110);
            float vz_up = vel_up.z;
            if (u2f(0xbccccccdu) < vz_up) {
                max_drop = sin_vu(lwc1(P + 0x2e0)) * lwc1(P + 0x164);
                if (max_drop < 0.0f) max_drop = 0.0f;
                const float half_spd = lwc1(P + 0x164) * 0.5f;
                if (half_spd < max_drop) max_drop = half_spd;
                vz_up = vel_up.z;
            } else {
                max_drop = vz_up;
            }
            if (vz_up < 0.0f) vel_up.z = 0.0f;
            if (max_drop < 0.0f) max_drop = 0.0f;

            if (lbu(P + 0x12e8) != 0) sh(P + 0x8ac, static_cast<uint32_t>(frame_scale_i(7)));
            tick_i16(P + 0x8ac);

            if (lh(P + 0x89c) == 0) {
                const bool can_jump = (lh(P + 0x30e) < frame_scale_i(4) || lh(P + 0x8bc) == 0) && lh(P + 0x89e) == 0;
                if (can_jump && pad_button_history(0x40, frame_scale_i(9), nullptr) != 0) {
                    sh(P + 0x8bc, 1);
                    sh(P + 0x89c, 1);
                    mem_fill32(P + 0x6c0, 0, 0x60);
                    if (lh(P + 0x8ac) != 0) {
                        const float jz = max_drop + lwc1(0x0015ed6cu) * u2f(0x416b3333u);
                        swc1(P + 0x108, jz);
                        swc1(P + 0x118, jz);
                        swc1(P + 0x128, jz);
                        hero_set_state(0x6c, 1);
                        return false;
                    }
                    const int32_t blend = frame_scale_i(0xa);
                    hero_anim_set(board_air_anim(), 5, static_cast<float>(blend));
                    const uint32_t bm = lwu(P + 0x86c);
                    if (bm != 0) moby_anim_set(bm, 6, 5, blend);
                    const float jz = max_drop + lwc1(0x0015ed6cu) * 5.5f;
                    swc1(P + 0x108, jz);
                    swc1(P + 0x118, jz);
                    swc1(P + 0x128, jz);
                    return false;
                }
            }

            if (lh(P + 0x89c) != 0 && lh(P + 0x89c) < frame_scale_i(0x11) && lw(P + 0x2084) != 0x6c) {
                if ((lwu(0x0013cae0u) & 0x40u) != 0 || lh(P + 0x89c) < frame_scale_i(4)) {
                    const float add = lwc1(0x0015ed6cu) * u2f(0x3f170a3du);
                    const float z1 = vel_up.z + add;
                    const float z0 = lwc1(P + 0x108) + add;
                    swc1(P + 0x128, z1);
                    swc1(P + 0x108, z0);
                    swc1(P + 0x118, z1);
                }
            }

            board_tricks_anim_update();
            board_track_pickups_update();
            board_trick_score_update();

            const uint32_t hm = lwu(P + 0x2080);
            const uint32_t cur_a = lbu(hm + 0x53);
            if ((cur_a == 0x55 || cur_a == 0x7f) && lbu(hm + 0x51) < 12u) {
                const int32_t ft = lh(P + 0x888);
                if (ft != 0) sub_00208f20(27.0f, static_cast<float>(ft), 0.5f, -1.0f);
                if (u2f(0x3f99999au) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x3f99999au));
            }

            const uint32_t g_anim = board_ground_anim();
            const uint32_t now_a = lbu(hm + 0x53);
            if ((now_a == 0x52 || now_a == 0x7d) && (g_anim == 0x68 || g_anim == 0x7e)) {
                hero_anim_set(g_anim, 0, static_cast<float>(frame_scale_i(7)));
            } else if ((now_a == 0x68 || now_a == 0x7e) && (g_anim == 0x52 || g_anim == 0x7d)) {
                hero_anim_set(g_anim, 0, static_cast<float>(frame_scale_i(0x11)));
            }

            if (lh(P + 0x30c) == 0) {
                swc1(P + 0xa90, 1.0f);
                const uint32_t ca = lbu(lwu(P + 0x2080) + 0x53);
                if ((ca == 0x55 || ca == 0x7f) && lwc1(P + 0xaa8) < 25.0f) {
                    hero_anim_set(board_air_anim(), 0xe, static_cast<float>(frame_scale_i(8)));
                }
            }

            if ((lwu(P + 0xa98) & 2u) != 0 && lbu(lwu(P + 0x2080) + 0x53) != g_anim) {
                hero_anim_set(g_anim, 0, static_cast<float>(frame_scale_i(0xa)));
            }
            return false;
        }
        case 0x6c: {
            const uint32_t cur_a = lbu(lwu(P + 0x2080) + 0x53);
            if (cur_a == 0x55 || cur_a == 0x7f) {
                const int16_t ft = static_cast<int16_t>(float_to_int(hero_fall_time(60.0f, lwc1(P + 0x860), -1.0f)));
                sh(P + 0x888, static_cast<uint16_t>(ft));
                if (ft != 0) sub_00208f20(27.0f, static_cast<float>(ft), 0.5f, -1.0f);
                if (u2f(0x400ccccdu) < lwc1(P + 0xa90)) swc1(P + 0xa90, u2f(0x400ccccdu));
            }
            if (board_crash_and_land_check() != 0) return false;
            board_tricks_anim_update();
            board_track_pickups_update();
            board_trick_score_update();
            return false;
        }
        case 0x6d: {
            bool fell_into_6e = false;
            if (level_mod_board_free()) {  // enhancement: the board outside a race
                fell_into_6e = level_mod_board_fell(lwc1(P + 0x88));
            } else {
                const uint32_t pl = level_mod_board_planet(lwu(0x0015ed84u));
                if (pl == 5u && lwc1(P + 0x88) < u2f(0x4275999au)) {
                    swc1(P + 0x8a0, u2f(0x4275999au));
                    fell_into_6e = true;
                } else if (pl == 0x10u && lwc1(P + 0x88) < u2f(0x42980000u)) {
                    swc1(P + 0x8a0, u2f(0x42980000u));
                    fell_into_6e = true;
                }
            }
            if (fell_into_6e) {
                hero_set_state(0x6e, 1);
                const uint16_t raw = static_cast<uint16_t>(static_cast<uint32_t>(frame_scale_i(0x46)) - lhu(P + 0x198));
                sh(P + 0x8be, raw);
                if (static_cast<int16_t>(raw) < frame_scale_i(0x1e)) {
                    sh(P + 0x8be, static_cast<uint32_t>(frame_scale_i(0x1e)));
                }
                return true;  // immediate return from hero_input_update (skips epilogue)
            }
            if ((lwu(P + 0xa98) & 2u) != 0) board_place_at_respawn();
            return false;
        }
        case 0x6e: {
            if (lwc1(P + 0x80) < 5.0f || lwc1(P + 0x84) < 5.0f || lh(P + 0x8be) < lw(P + 0x198)) {
                board_place_at_respawn();
            }
            return false;
        }
        case 0x6f: {
            if ((lwu(P + 0xa98) & 2u) != 0) board_place_at_respawn();
            return false;
        }
        default:
            return false;
    }
}

}  // namespace rac1
