// Hoven's (level 12) class 1274 (the Blarg planetary-defense dropship / drill rig with 5 attached turrets and 18 falling
// debris pieces) and its four private helpers (0x306350, 0x3064e8, 0x3054b0, 0x306828). Level 12 only: `orig: L12:`, level
// data through lvd::*, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/hydro.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/shared_attach.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_small.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

void dropship_hover_wobble(uint32_t m, uint32_t pv) {
    sub_00262b00(m, pv + 0x5cc, pv + 0x5c8, u2f(0x3f333333u), fpu_mul(lwc1(0x0015ed6cu), u2f(0x3f9c61aau)));
    const float deg = u2f(0x3c8efa35u);
    const float rx = fpu_mul(lwc1(L12_ADDR(0x00161f28)), deg);
    const float dt = lwc1(0x0015ed6cu);
    const float ry = fpu_mul(lwc1(L12_ADDR(0x00161f2c)), deg);
    sub_00262b80(m, pv + 0x5e4, pv + 0x5e8, lwc1(L12_ADDR(0x00161f30)), fpu_mul(rx, dt), fpu_mul(ry, dt));
}
}  // namespace

// Initialises the fall gravity (`pv + 0x570 + 4 * idx`) and tumble rates (`pv + 0x450 / +0x454 / +0x45c + 0x10 * idx`) of
// piece `idx` (`idx == 17` is the main hull `m` itself, else `0x1fb688[idx]`).
// orig: L12:0x00306350
void dropship_piece_init(uint32_t m, int32_t idx) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t part = m;
    uint32_t word_off = 0x44;
    if (idx != 17) {
        word_off = static_cast<uint32_t>(idx << 2);
        part = lwu(L12_ADDR(0x0015ffd8)) + (lwu(L12_ADDR(0x001fb688) + word_off) << 8);
    }

    const float dt2 = lwc1(0x0015ed70u);
    const float kMinSpin = u2f(0x3d567750u);
    const uint32_t grav_slot = pv + 0x570 + word_off;
    const float g0 = rand_range_f(fpu_mul(dt2, u2f(0x406ccccdu)), fpu_mul(dt2, 5.0f));
    const float dt1 = lwc1(0x0015ed6cu);
    swc1(grav_slot, g0);
    const float sx = rand_range_signed_f(fpu_mul(dt1, kMinSpin), fpu_mul(dt1, u2f(0x3e0efa35u)));
    const float dt1b = lwc1(0x0015ed6cu);
    const uint32_t ang_slot = pv + static_cast<uint32_t>(idx << 4);
    swc1(ang_slot + 0x450, sx);
    swc1(ang_slot + 0x454, rand_range_signed_f(fpu_mul(dt1b, kMinSpin), fpu_mul(dt1b, u2f(0x3e32b8c2u))));

    const int32_t cls = lh(part + 0xa6);
    if (cls == 0x4fd) {
        const float spring_k = rand_range_f(u2f(0x3a83126fu), u2f(0x3b9374bcu));
        const float dt2b = lwc1(0x0015ed70u);
        swc1(ang_slot + 0x45c, spring_k);
        swc1(grav_slot, rand_range_f(fpu_mul(dt2b, 5.5f), fpu_mul(dt2b, 9.0f)));
    } else if (cls == 0x500) {
        swc1(grav_slot, fpu_mul(lwc1(0x0015ed70u), 13.0f));
    }
}

// Steps all detached pieces (`0x1fb688[0..17]`), retiring sub-pieces below `z = 20` and returning early once the main hull
// (`idx == 17`) drops below 20.
// orig: L12:0x003064e8
void dropship_pieces_fall(uint32_t m) {
    const Frame f(192);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    sw(sp + 0x0, 0);
    for (int32_t i = 0; i < 18; ++i) {
        const uint32_t slot_addr = L12_ADDR(0x001fb688) + static_cast<uint32_t>(i * 4);
        const int32_t slot = lw(slot_addr);
        if (slot != 0) {
            uint32_t part = m;
            bool active = true;
            if (i != 17) {
                if (slot == -1) active = false;
                else part = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(slot) << 8);
            }
            if (active) {
                if (i == 17) {
                    if (lwc1(L12_ADDR(0x00161f40)) == -1.0f) {
                        lq_sq(L12_ADDR(0x00161f40), part + 0x10);
                        lq_sq(L12_ADDR(0x00161f50), part + 0x40);
                    }
                    lq_sq(L12_ADDR(0x00161f60), part + 0x10);
                    lq_sq(L12_ADDR(0x00161f70), part + 0x40);
                    lq_sq(part + 0x10, L12_ADDR(0x00161f40));
                    lq_sq(part + 0x40, L12_ADDR(0x00161f50));
                }
                if (lwc1(part + 0x18) < 20.0f) {
                    if (i == 17) return;
                    moby_retire(part);
                    sw(slot_addr, 0xffffffffu);
                }
                const uint32_t p_slot = pv + static_cast<uint32_t>(i * 0x10);
                const float vz = fpu_sub(lwc1(p_slot + 0x338), lwc1(pv + 0x570 + lwu(sp + 0x0)));
                swc1(p_slot + 0x338, vz);
                swc1(part + 0x18, fpu_add(lwc1(part + 0x18), vz));

                const int32_t cls = lh(part + 0xa6);
                if (cls == 0x500) {
                    swc1(part + 0x40, angle_add(lwc1(part + 0x40), fpu_mul(lwc1(0x0015ed6cu), u2f(0x3eb2b8c2u))));
                    spring_angle(part + 0x44, p_slot + 0x454, 0, u2f(0x3fc90fdbu), u2f(0x3b03126fu), u2f(0x3e99999au), 0.0f);
                } else if (cls == 0x4fd) {
                    swc1(part + 0x40, angle_add(lwc1(part + 0x40), lwc1(p_slot + 0x450)));
                    spring_angle(part + 0x44, p_slot + 0x454, 0, u2f(0xbfc90fdbu), lwc1(p_slot + 0x45c), u2f(0x3e99999au), 0.0f);
                } else if (cls == 0x4fa) {
                    const float dt = lwc1(0x0015ed6cu);
                    swc1(part + 0x40, angle_add(lwc1(part + 0x40), fpu_mul(dt, u2f(0xbf5f66f3u))));
                    const float next_y = angle_add(lwc1(part + 0x44), lwc1(p_slot + 0x454));
                    const float dtb = lwc1(0x0015ed6cu);
                    swc1(part + 0x44, next_y);
                    swc1(part + 0x14, fpu_add(lwc1(part + 0x14), fpu_mul(dtb, -4.0f)));
                } else {
                    swc1(part + 0x14, fpu_add(lwc1(part + 0x14), fpu_mul(lwc1(0x0015ed6cu), -4.0f)));
                    swc1(part + 0x40, angle_add(lwc1(part + 0x40), lwc1(p_slot + 0x450)));
                    swc1(part + 0x44, angle_add(lwc1(part + 0x44), lwc1(p_slot + 0x454)));
                }

                if (i == 17) {
                    lq_sq(L12_ADDR(0x00161f40), part + 0x10);
                    lq_sq(L12_ADDR(0x00161f50), part + 0x40);
                }
                moby_update_transform(part);
            }
        }
        sw(sp + 0x0, lwu(sp + 0x0) + 4u);
    }
}

// Aims, fires, damages and smokes the dropship's 5 turrets (`pv + 0x5d0`), detaching destroyed pieces and transitioning the
// dropship to state 4 once total health `pv + 0x5fc` reaches 0.
// orig: L12:0x003054b0
void dropship_turrets_update(uint32_t m) {
    const Frame f(288);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    sw(sp + 0x40, m);
    sw(sp + 0x48, sp + 0x20);
    sw(sp + 0x44, sp + 0x10);

    for (int32_t idx = 0; idx < 5; ++idx) {
        sw(sp + 0x50, static_cast<uint32_t>(idx << 2));
        sw(sp + 0x4c, static_cast<uint32_t>(idx + 1));
        const int32_t attach_idx = lw(pv + 0x5d0 + static_cast<uint32_t>(idx << 2));
        if (attach_idx == -1) continue;

        const uint32_t rec = pv + static_cast<uint32_t>(attach_idx * 0x30);
        const uint32_t tur = lwu(L12_ADDR(0x0015ffd8)) + (lwu(rec + 0x20) << 8);
        const uint32_t tpv = lwu(tur + 0x78);

        lq_sq(sp + 0x0, L12_ADDR(0x00167240));
        swc1(sp + 0x8, fpu_sub(lwc1(sp + 0x8), u2f(0x3f333333u)));
        lq_sq(sp + 0x10, tur + 0x10);
        swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 1.5f));
        const float cam_d2 = vec2_distance(V(sp + 0x10), V(sp + 0x0));
        const float pitch = atan2_f(cam_d2, fpu_sub(lwc1(sp + 0x8), lwc1(sp + 0x18)));
        const float k_spring = lwc1(0x0015ed64u);
        swc1(tpv + 0x60, pitch);
        joint_node_spring(tur, tpv, 0, fpu_mul(k_spring, u2f(0x3c03126fu)), fpu_mul(k_spring, u2f(0x3e99999au)));

        const float to_hero = atan2_f(fpu_sub(lwc1(0x0013f3d0u), lwc1(tur + 0x10)), fpu_sub(lwc1(0x0013f3d4u), lwc1(tur + 0x14)));
        const float goal_yaw = angle_sub(angle_add(to_hero, u2f(0xbfc90fdbu)), lwc1(m + 0x48));
        if (lh(tpv + 0x90) == 0) {
            const float dt = lwc1(0x0015ed6cu);
            const float rate =
                fpu_add(fpu_mul(dt, u2f(0x3f060a92u)), fpu_mul(fpu_mul(dt, u2f(0x3e32b8c2u)), static_cast<float>(idx)));
            if (turn_towards(rec + 0x18, 0, goal_yaw, rate) < u2f(0x3db2b8c2u)) {
                sh(tpv + 0x90, 1);
                swc1(tpv + 0x8c, (idx & 1) == 0 ? 1.0f : -1.0f);
            }
        } else {
            const float sweep_goal = angle_add(goal_yaw, fpu_mul(lwc1(tpv + 0x8c), u2f(0x3f0efa35u)));
            const float dt = lwc1(0x0015ed6cu);
            const float k2 = lwc1(0x0015ed64u);
            const float max_step =
                fpu_add(fpu_mul(dt, u2f(0x3e860a92u)), fpu_mul(static_cast<float>(idx), fpu_mul(dt, u2f(0x3d567750u))));
            const float rem = spring_angle(rec + 0x18, tpv + 0x94, 0, sweep_goal, fpu_mul(k2, u2f(0x3cb43958u)),
                                           fpu_mul(k2, u2f(0x3e99999au)), max_step);
            if (abs_f(rem) < u2f(0x3c8efa35u)) swc1(tpv + 0x8c, -lwc1(tpv + 0x8c));
        }

        const float hdist = vec2_distance(V(m + 0x10), V(0x0013f3d0u));
        const float hdz = abs_f(fpu_sub(lwc1(0x0013f3d8u), lwc1(m + 0x18)));
        const bool near_alert = frame_scale_i(0x4b0) < lw(L12_ADDR(0x00161f34)) && hdist < 80.0f && hdz < 4.0f;
        if (90.0f < hdist || 7.0f < hdz) sw(L12_ADDR(0x00161f34), 0);

        if ((lw(L12_ADDR(0x00161f00)) != 0 || near_alert) && 0.0f < lwc1(tpv + 0x80)) {
            timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(tpv + 0x98)));
            timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(tpv + 0x9c)));
            if (lh(tpv + 0x98) == 0) {
                if (angle_dist(lwc1(pv + static_cast<uint32_t>(attach_idx * 0x30) + 0x18), goal_yaw) < u2f(0x3d8efa35u)) {
                    sh(tpv + 0x9e, static_cast<uint32_t>(frame_scale_i(0x14)));
                }
                if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(tpv + 0x9e))) == 0) {
                    if (countdown_u8(tpv + 0x92) == 0 && countdown_u8(tpv + 0x93) != 0) {
                        moby_sound_start(0, 0, tur);
                        sb(tpv + 0x93, static_cast<uint8_t>(frame_scale_i(9)));
                    }
                    if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(tpv + 0x9c))) != 0) {
                        moby_chain_point_v(tur, lh(tpv + 0x9a) + 1, V(sp + 0x0));
                        sh(tpv + 0x9a, lh(tpv + 0x9a) < 1 ? 1u : 0u);
                        const float shot_yaw = angle_add(lwc1(tur + 0x48), u2f(0x3fc90fdbu));
                        float z_jit;
                        if (lw(L12_ADDR(0x00161f00)) != 0) {
                            lq_sq(sp + 0x20, L12_ADDR(0x00167240));
                            z_jit = rand_range_f(-1.5f, u2f(0x3e99999au));
                        } else {
                            lq_sq(sp + 0x20, 0x0013f420u);
                            z_jit = rand_range_signed_f(0.0f, 0.5f);
                        }
                        swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), z_jit));

                        int32_t spread = lw(L12_ADDR(0x00161f20)) - 3;
                        if (frame_scale_i(5) < spread) spread = frame_scale_i(5);
                        if (spread <= -1) spread = 0;
                        const float sf = static_cast<float>(spread);
                        const float extra_z = rand_range_f(fpu_mul(sf, u2f(0xbe99999au)), fpu_mul(sf, u2f(0x3e99999au)));
                        swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), extra_z));

                        const float shot_d2 = vec2_distance(V(sp + 0x0), V(sp + 0x20));
                        const float shot_pitch = atan2_f(shot_d2, fpu_sub(lwc1(sp + 0x28), lwc1(sp + 0x8)));
                        vec_from_spherical_v(V(sp + 0x10), fpu_mul(lwc1(0x0015ed6cu), 80.0f), shot_yaw, shot_pitch);

                        if (shot_launch(tur, sp + 0x0, sp + 0x10, frame_scale_i(0x96)) != 0) {
                            sb(tpv + 0x92, static_cast<uint8_t>(frame_scale_i(0x14)));
                            float dt = lwc1(0x0015ed6cu);
                            for (int32_t p = 0; p < 9; ++p) {
                                const float kSpd = u2f(0x3fd9999au);
                                swc1(sp + 0x30, rand_range_signed_f(0.0f, fpu_mul(dt, kSpd)));
                                swc1(sp + 0x34, rand_range_signed_f(0.0f, fpu_mul(lwc1(0x0015ed6cu), kSpd)));
                                swc1(sp + 0x38, rand_range_signed_f(0.0f, fpu_mul(lwc1(0x0015ed6cu), kSpd)));
                                const uint32_t r_ch = static_cast<uint32_t>(rand_range(0xaa, 0xf0));
                                const uint32_t g_ch = static_cast<uint32_t>(rand_range(0x6e, 0xc8));
                                const uint32_t col = 0x7f000000u | (g_ch << 8) | r_ch;
                                const uint32_t life = static_cast<uint32_t>(frame_scale_i(rand_range(0xf, 0x1e)));
                                const uint32_t rot = static_cast<uint32_t>(rand_range(0xb4, 0x10e));
                                spawn_particle_4(sp + 0x0, sp + 0x30, col, 0xff, life, 0x37, rot, 1);
                                dt = lwc1(0x0015ed6cu);
                            }
                        }

                        sh(tpv + 0x9c, static_cast<uint32_t>(frame_scale_i(7)));
                        int32_t cd_extra = lw(L12_ADDR(0x00161f20));
                        if (frame_scale_i(0xb) < cd_extra) cd_extra = frame_scale_i(0xb);
                        if (cd_extra <= -1) cd_extra = 0;
                        sh(tpv + 0x9c, lhu(tpv + 0x9c) + static_cast<uint32_t>(cd_extra));
                    }
                }
            }
        }

        if (lw(L12_ADDR(0x00161f04)) != 0 || !(0.0f < lwc1(tpv + 0x80))) {
            sb(tur + 0xa4, 0xff);
        } else {
            if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(tpv + 0x84))) != 0) {
                float dmg = 1.0f;
                uint32_t hit = sub_0025a420(tur, 0x50000u, 0);
                if (hit == 0) {
                    const int32_t alt_slot = lw(L12_ADDR(0x001fb670) + lwu(sp + 0x50));
                    uint32_t alt_m = 0;
                    if (alt_slot == 17) {
                        alt_m = m;
                    } else {
                        const int32_t am = lw(pv + static_cast<uint32_t>(alt_slot * 0x30) + 0x20);
                        if (am != -1) alt_m = lwu(L12_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(am) << 8);
                    }
                    if (alt_m != 0) {
                        dmg = u2f(0x3e4ccccdu);
                        hit = sub_0025a420(alt_m, 0x50000u, 0);
                        sb(alt_m + 0xa4, 0xff);
                    }
                }
                if (hit != 0) {
                    if (lw(L12_ADDR(0x00161f00)) == 0) {
                        sw(L12_ADDR(0x00161f34), static_cast<uint32_t>(frame_scale_i(0xfa0)));
                    } else {
                        sw(tpv + 0x84, static_cast<uint32_t>(frame_scale_i(0x14)));
                        const float hp = fpu_sub(lwc1(tpv + 0x80), dmg);
                        swc1(tpv + 0x80, hp);
                        if (hp <= 0.0f) {
                            const int16_t rem_hp = static_cast<int16_t>(lhu(pv + 0x5fc) - 0x14u);
                            sh(pv + 0x5fc, rem_hp >= 0 ? static_cast<uint16_t>(rem_hp) : 0u);
                            moby_sound_start(1, 0, tur);
                            if (lh(pv + 0x5fc) > 0) {
                                const int32_t pslot = lw(L12_ADDR(0x001fb670) + lwu(sp + 0x50));
                                if (pslot == 17) {
                                    sw(L12_ADDR(0x001fb6cc), 1);
                                } else {
                                    const uint32_t prec = pv + static_cast<uint32_t>(pslot * 0x30);
                                    sw(L12_ADDR(0x001fb688) + static_cast<uint32_t>(pslot << 2), lwu(prec + 0x20));
                                    sw(prec + 0x20, 0xffffffffu);
                                }
                                dropship_piece_init(m, pslot);
                                if (idx == 4) {
                                    for (int32_t k = 0; k < 17; ++k) {
                                        const uint32_t krec = pv + static_cast<uint32_t>(k * 0x30);
                                        const int32_t mi = lw(krec + 0x20);
                                        const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
                                        if (mi == -1) continue;
                                        const uint32_t pm = first + (static_cast<uint32_t>(mi) << 8);
                                        if (lh(pm + 0xa6) != 0x4fd) continue;
                                        sw(L12_ADDR(0x001fb688) + static_cast<uint32_t>(k << 2), static_cast<uint32_t>(mi));
                                        sw(krec + 0x20, 0xffffffffu);
                                        dropship_piece_init(m, k);
                                    }
                                }
                            }
                            for (int32_t p = 0; p < 130; ++p) {
                                lq_sq(sp + 0x0, tur + 0x10);
                                swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), rand_range_f(u2f(0x3dcccccdu), 0.5f)));
                                const float ang = rand_angle();
                                const float rad = rand_range_f(u2f(0x3dcccccdu), u2f(0x3f4ccccdu));
                                swc1(sp + 0x0, fpu_add(lwc1(sp + 0x0), fpu_mul(cos_vu(ang), rad)));
                                swc1(sp + 0x4, fpu_add(lwc1(sp + 0x4), fpu_mul(sin_vu(ang), rad)));
                                const int32_t life = rand_range(frame_scale_i(0x5a), frame_scale_i(0xb4));
                                const float dt = lwc1(0x0015ed6cu);
                                const float spd = rand_range_f(fpu_mul(dt, u2f(0x3f333333u)), fpu_mul(dt, 1.5f));
                                swc1(sp + 0x10, fpu_mul(cos_vu(ang), spd));
                                const float vsin = sin_vu(ang);
                                const float dtb = lwc1(0x0015ed6cu);
                                sw(sp + 0x18, 0);
                                swc1(sp + 0x14, fpu_mul(vsin, spd));
                                swc1(sp + 0x18, rand_range_f(dtb, fpu_mul(dtb, 5.5f)));
                                particle_16_spawn(sp + 0x0, sp + 0x10, 0x500a1414u, 0x000a0a0au, life, u2f(0x48cd1400u));
                            }
                            sw(tur + 0x94, 0);
                            sh(tur + 0x34, lhu(tur + 0x34) | 0x41u);
                        }
                    }
                }
            }
            sb(tur + 0xa4, 0xff);
        }

        const float cur_hp = lwc1(tpv + 0x80);
        if (0.0f < cur_hp && cur_hp < 10.0f) {
            const int32_t diff = 9 - float_to_int(cur_hp);
            const int32_t sev = (diff + static_cast<int32_t>(static_cast<uint32_t>(diff) >> 31)) >> 1;
            const int32_t fire_n = sev + 3;
            for (int32_t p = 0; p < fire_n; ++p) {
                lq_sq(sp + 0x0, tur + 0x10);
                swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), u2f(0x3f333333u)));
                const float ang = rand_angle();
                const float rad = rand_range_f(u2f(0x3dcccccdu), u2f(0x3ecccccdu));
                swc1(sp + 0x0, fpu_add(lwc1(sp + 0x0), fpu_mul(cos_vu(ang), rad)));
                swc1(sp + 0x4, fpu_add(lwc1(sp + 0x4), fpu_mul(sin_vu(ang), rad)));
                const int32_t life = rand_range(frame_scale_i(0x19), frame_scale_i(0x37));
                const float dt = lwc1(0x0015ed6cu);
                const float spd = rand_range_f(fpu_mul(dt, 0.5f), fpu_mul(dt, 3.0f));
                swc1(sp + 0x10, fpu_mul(cos_vu(ang), spd));
                const float vsin = sin_vu(ang);
                const float dtb = lwc1(0x0015ed6cu);
                swc1(sp + 0x14, fpu_mul(vsin, spd));
                sw(sp + 0x18, 0);
                float vz = rand_range_f(fpu_mul(dtb, 7.0f), fpu_mul(dtb, 17.0f));
                vz = fpu_div(fpu_mul(fpu_mul(vz, int_to_float(sev)), 1.5f), int_to_float(5));
                swc1(sp + 0x18, vz);
                const uint32_t g_ch = static_cast<uint32_t>(rand_range(0x46, 0xcd));
                particle_16_spawn(sp + 0x0, sp + 0x10, 0x460a00aau | (g_ch << 8), 0x000a0a46u, life, u2f(0x48cd1400u));
            }
            for (int32_t p = 0; p < 3; ++p) {
                lq_sq(sp + 0x0, tur + 0x10);
                const float ang = rand_angle();
                const float rad = rand_range_f(u2f(0x3dcccccdu), u2f(0x3f4ccccdu));
                swc1(sp + 0x0, fpu_add(lwc1(sp + 0x0), fpu_mul(cos_vu(ang), rad)));
                swc1(sp + 0x4, fpu_add(lwc1(sp + 0x4), fpu_mul(sin_vu(ang), rad)));
                const int32_t life = rand_range(frame_scale_i(0x28), frame_scale_i(0x46));
                const float dt = lwc1(0x0015ed6cu);
                const float spd = rand_range_f(fpu_mul(dt, u2f(0x3dcccccdu)), fpu_mul(dt, u2f(0x3ecccccdu)));
                swc1(sp + 0x10, fpu_mul(cos_vu(ang), spd));
                const float vsin = sin_vu(ang);
                const float dtb = lwc1(0x0015ed6cu);
                swc1(sp + 0x14, fpu_mul(vsin, spd));
                sw(sp + 0x18, 0);
                swc1(sp + 0x18, rand_range_f(fpu_mul(dtb, u2f(0x4019999au)), fpu_mul(dtb, 5.5f)));
                particle_16_spawn(sp + 0x0, sp + 0x10, 0x460a1414u, 0x000a0a0au, life, u2f(0x48f61801u));
            }
        }
    }

    dropship_pieces_fall(m);
    if (lh(pv + 0x5fc) <= 0 && lw(L12_ADDR(0x00161f04)) == 0) {
        sw(L12_ADDR(0x00161f24), 1);
        sb(m + 0x20, 4);
        sb(m + 0xbc, 0);
    }
}

// Pulses vertex tint `part + 0x90` (`part + 0x34 |= 0x10`) across all non-0x4fe attached parts and the dropship hull.
// orig: L12:0x00306828
void dropship_pulse_tint(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (int32_t i = 0; i < 18; ++i) {
        const uint32_t part =
            i == 17 ? m : (lwu(L12_ADDR(0x0015ffd8)) + (lwu(pv + static_cast<uint32_t>(i * 0x30) + 0x20) << 8));
        if (lh(part + 0xa6) == 0x4fe) continue;

        const int32_t period = frame_scale_i(0xaa);
        const float pf = int_to_float(period);
        const int32_t rem = ee_rem(lw(L12_ADDR(0x0015f5cc)), period);
        float phase = fpu_div(static_cast<float>(rem), pf);
        phase = fpu_add(phase, phase);
        phase = fpu_mul(phase, u2f(0x40490fdbu));
        phase = fpu_add(phase, u2f(0xc0490fdbu));
        const float s = sin_vu(phase);
        const float s70 = fpu_mul(s, 70.0f);
        int32_t r_ch = float_to_int(s70) + 0xb4;
        int32_t g_ch = float_to_int(s70) + 0xb4;
        const uint32_t b_ch = static_cast<uint32_t>(float_to_int(fpu_mul(s, 10.0f)) + 0x46);
        if (g_ch >= 256) g_ch = 0xff;
        if (r_ch >= 256) r_ch = 0xff;
        const uint32_t col = 0x80000000u | (b_ch << 16) | (static_cast<uint32_t>(g_ch) << 8) | static_cast<uint32_t>(r_ch);
        const uint32_t flags = lhu(part + 0x34) | 0x10u;
        sw(part + 0x90, col);
        sh(part + 0x34, flags);
    }
}

// Class 1274's update (jump table L12:0x20ca00, states 0..4): 0 binds its 17 parts and places itself at path `pv + 0x5c0`,
// 1 hovers until the hero enters trigger box `pv + 0x5ec` or turret mode `0x161f00` starts, 2 flies along path `pv + 0x5c0`
// while firing turrets, 3 holds station firing turrets, 4 detaches all pieces, sweeps the camera between boxes `pv + 0x608`
// and `pv + 0x60c`, and starts cutscene 7.
// orig: L12:0x003069d0
void hov_class1274_update_l12(uint32_t m) {
    const Frame f(208);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    if (lbu(m + 0x20) != 0 && lw(L12_ADDR(0x001fb6cc)) != 0) {
        lq_sq(m + 0x10, L12_ADDR(0x00161f60));
        lq_sq(m + 0x40, L12_ADDR(0x00161f70));
    }

    const uint32_t st = lbu(m + 0x20);
    if (st < 5) {
        switch (st) {  // (jump table L12:0x0020ca00)
        case 0: {      // (0x00306a6c)
            sw(L12_ADDR(0x001619b4), 0);
            if (lbu(0x0014c050u + (lwu(0x0015ed84u) << 4) + lbu(m + 0xb0)) == 0xff) {
                for (uint32_t k = 0; k < 17; ++k) {
                    const int32_t mi = lw(pv + 0x20 + k * 0x30);
                    const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
                    if (mi != -1) moby_retire(first + (static_cast<uint32_t>(mi) << 8));
                }
                moby_retire(m);
                return;
            }
            sw(L12_ADDR(0x00161f24), 0);
            attach_bind(m, pv, 17);
            const uint32_t path = lvd::path(lw(pv + 0x5c0));
            lq_sq(m + 0x10, path + 0x10);
            swc1(pv + 0x618, lwc1(m + 0x18));
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(path + 0x20), lwc1(path + 0x10)), fpu_sub(lwc1(path + 0x24), lwc1(path + 0x14))));
            if (lwc1(0x0013f3d8u) < 52.0f) swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 3.0f));
            const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
            for (uint32_t k = 0; k < 5; ++k) {
                const int32_t ai = lw(pv + 0x5d0 + k * 4);
                const uint32_t tm = first + (lwu(pv + static_cast<uint32_t>(ai * 0x30) + 0x20) << 8);
                swc1(lwu(tm + 0x78) + 0x80, 10.0f);
            }
            swc1(L12_ADDR(0x00161f40), -1.0f);
            for (int32_t k = 17; k >= 0; --k) {
                sw(L12_ADDR(0x001fb688) + static_cast<uint32_t>(k * 4), 0);
            }
            sw(pv + 0x604, 0xffffffffu);
            sh(pv + 0x5fc, 0x64);
            sb(m + 0x20, 1);
            break;
        }
        case 1: {  // (0x00306c18)
            const bool hide = point_in_box(L12_ADDR(0x00167240), lw(pv + 0x624)) != 0;
            const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
            sh(m + 0x34, hide ? (lhu(m + 0x34) | 0x41u) : (lhu(m + 0x34) & 0xffbeu));
            for (uint32_t k = 0; k < 17; ++k) {
                const int32_t mi = lw(pv + 0x20 + k * 0x30);
                if (mi != -1) {
                    const uint32_t pm = first + (static_cast<uint32_t>(mi) << 8);
                    sh(pm + 0x34, hide ? (lhu(pm + 0x34) | 0x41u) : (lhu(pm + 0x34) & 0xffbeu));
                }
            }
            constexpr uint32_t kHero = 0x0013f3d0u;
            const int32_t in_628 = point_in_box(kHero, lw(pv + 0x628));
            if (52.0f < lwc1(kHero + 8) && in_628 == 0) {
                const float dt = lwc1(0x0015ed6cu);
                approach_f(m + 0x18, lwc1(pv + 0x618), fpu_add(dt, dt));
            } else {
                float lift = in_628 != 0 ? 10.0f : 3.0f;
                if (point_in_box(kHero, lw(pv + 0x61c)) != 0) sw(pv + 0x620, static_cast<uint32_t>(frame_scale_i(0x5a)));
                if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x620))) == 0) lift = 19.0f;
                const float k = lwc1(0x0015ed64u), dt = lwc1(0x0015ed6cu);
                spring_to(m + 0x18, pv + 0x62c, fpu_add(lwc1(pv + 0x618), lift), fpu_mul(k, u2f(0x3cf5c28fu)),
                          fpu_mul(k, u2f(0x3e99999au)), fpu_mul(dt, u2f(0x402ccccdu)));
            }
            dropship_hover_wobble(m, pv);
            attach_carry(m, pv, 17);
            if (point_in_box(kHero, lw(pv + 0x5ec)) != 0 || lw(L12_ADDR(0x00161f00)) != 0) {
                sw(L12_ADDR(0x001619b4), 1);
                for (uint32_t k = 0; k < 5; ++k) {
                    const int32_t ai = lw(pv + 0x5d0 + k * 4);
                    const uint32_t first_m = lwu(L12_ADDR(0x0015ffd8));
                    const uint32_t tm = first_m + (lwu(pv + static_cast<uint32_t>(ai * 0x30) + 0x20) << 8);
                    sb(tm + 0xa4, 0xff);
                }
                sb(m + 0x20, 2);
            }
            break;
        }
        case 2: {  // (0x00306edc)
            dropship_hover_wobble(m, pv);
            const uint32_t path = lvd::path(lw(pv + 0x5c0));
            if (lh(pv + 0x5f8) < lw(path) - 11) {
                approach_f(pv + 0x5f0, fpu_mul(lwc1(0x0015ed6cu), 8.0f), fpu_mul(lwc1(0x0015ed70u), 10.0f));
            } else {
                approach_f(pv + 0x5f0, 0.0f, fpu_mul(lwc1(0x0015ed70u), 8.0f));
                if (lwc1(pv + 0x5f0) == 0.0f) {
                    sb(m + 0x20, 3);
                    break;
                }
            }
            const int32_t cur = lh(pv + 0x5f8);
            const int32_t next = ee_rem(cur + 1, lw(path));
            vec3_sub(V(sp), V(path + 0x10 + (static_cast<uint32_t>(next) << 4)),
                     V(path + 0x10 + (static_cast<uint32_t>(cur) << 4)));
            vec3_set_length(V(sp), V(sp), lwc1(pv + 0x5f0));
            vec3_add(V(m + 0x10), V(m + 0x10), V(sp));

            const uint32_t p_cur = path + (static_cast<uint32_t>(lh(pv + 0x5f8)) << 4);
            const uint32_t p_nxt = path + (static_cast<uint32_t>(next) << 4);
            float goal_yaw = atan2_f(fpu_sub(lwc1(p_nxt + 0x10), lwc1(p_cur + 0x10)),
                                     fpu_sub(lwc1(p_nxt + 0x14), lwc1(p_cur + 0x14)));
            if (lw(path) - 20 < lh(pv + 0x5f8)) goal_yaw = u2f(0x40437a14u);

            const float k = lwc1(0x0015ed64u), dt = lwc1(0x0015ed6cu);
            yaw_turn_damped(m, pv + 0x5f4, goal_yaw, fpu_mul(k, u2f(0x3ac49ba6u)), fpu_mul(k, u2f(0x3e99999au)),
                            fpu_mul(dt, u2f(0x3e32b8c2u)));

            const uint32_t pt_cur = path + 0x10 + (static_cast<uint32_t>(lh(pv + 0x5f8)) << 4);
            bool advance = vec2_distance(V(m + 0x10), V(pt_cur)) < 5.0f;
            if (!advance) {
                const uint32_t pt_chk = path + 0x10 + (static_cast<uint32_t>(lh(pv + 0x5f8)) << 4);
                const float brg = atan2_f(fpu_sub(lwc1(pt_chk + 0x0), lwc1(m + 0x10)),
                                          fpu_sub(lwc1(pt_chk + 0x4), lwc1(m + 0x14)));
                if (u2f(0x3fc90fdbu) < angle_dist(lwc1(m + 0x48), brg)) advance = true;
            }
            if (advance) {
                sh(pv + 0x5f8, static_cast<uint32_t>(ee_rem(lh(pv + 0x5f8) + 1, lw(path))));
            }
            attach_carry(m, pv, 17);
            dropship_turrets_update(m);
            break;
        }
        case 3: {  // (0x0030718c)
            const float k = lwc1(0x0015ed64u), dt = lwc1(0x0015ed6cu);
            sw(L12_ADDR(0x00161f34), lwu(L12_ADDR(0x00161f34)) + 1u);
            spring_angle(m + 0x48, pv + 0x5f4, -1, u2f(0x40437a14u), fpu_mul(k, u2f(0x3ac49ba6u)),
                         fpu_mul(k, u2f(0x3e99999au)), fpu_mul(dt, u2f(0x3e32b8c2u)));
            dropship_hover_wobble(m, pv);
            attach_carry(m, pv, 17);
            dropship_turrets_update(m);
            break;
        }
        case 4: {  // (0x00307288)
            if (lw(pv + 0x608) != -1 && lw(pv + 0x60c) != -1) {
                const uint32_t sub_st = lbu(m + 0xbc);
                if (sub_st == 0) {
                    sb(m + 0xbc, 1);
                    uid5_set_pvar_80(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x30);
                    uid5_set_pvar_90(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x70);
                    sw(L12_ADDR(0x001619a0), 1);
                    sw(L12_ADDR(0x0015f404), 1);
                } else if (sub_st == 1) {
                    if (lwc1(pv + 0x610) < 1.0f) {
                        const float accel = fpu_mul(lwc1(0x0015ed70u), u2f(0x3f2a7efau));
                        seek_1d(pv + 0x610, pv + 0x614, 1.0f, accel, accel, fpu_mul(lwc1(0x0015ed6cu), u2f(0x3ecccccdu)));
                        vec3_lerp(V(sp), V(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x30),
                                  V(lvd::box(lvd::BOXES, lw(pv + 0x60c)) + 0x30), lwc1(pv + 0x610));
                        vec_clear(V(sp + 0x10));

                        // (as in the original: roll's `angle_add` reads `sp + 0x14` before pitch is stored there, so roll stays
                        // at `b0 + 0x70` while pitch `+0x74` and yaw `+0x78` interpolate by `pv + 0x610`)
                        {
                            const float dx = angle_sub(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x60c)) + 0x70),
                                                       lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x70));
                            const float step_x = fpu_mul(dx, lwc1(pv + 0x610));
                            const float zero_y = lwc1(sp + 0x14);
                            swc1(sp + 0x10, step_x);
                            swc1(sp + 0x10, angle_add(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x70), zero_y));
                        }
                        {
                            const float dy = angle_sub(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x60c)) + 0x74),
                                                       lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x74));
                            const float step_y = fpu_mul(dy, lwc1(pv + 0x610));
                            swc1(sp + 0x14, step_y);
                            swc1(sp + 0x14, angle_add(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x74), step_y));
                        }
                        {
                            const float dz = angle_sub(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x60c)) + 0x78),
                                                       lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x78));
                            const float step_z = fpu_mul(dz, lwc1(pv + 0x610));
                            swc1(sp + 0x18, step_z);
                            swc1(sp + 0x18, angle_add(lwc1(lvd::box(lvd::BOXES, lw(pv + 0x608)) + 0x78), step_z));
                        }
                        uid5_set_pvar_80(sp);
                        uid5_set_pvar_90(sp + 0x10);
                    } else {
                        sb(m + 0xbc, 2);
                        sw(L12_ADDR(0x0015f404), 0);
                        hydro_2eac18(3);
                    }
                } else if (sub_st == 2) {
                    for (uint32_t k = 0; k < 17; ++k) {
                        const uint32_t rec_m = pv + k * 0x30 + 0x20;
                        const int32_t mi = lw(rec_m);
                        const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
                        if (mi != -1) {
                            const uint32_t pm = first + (static_cast<uint32_t>(mi) << 8);
                            if (static_cast<int8_t>(lbu(pm + 0x20)) >= 0) moby_retire(pm);
                            sw(rec_m, 0xffffffffu);
                        } else {
                            const int32_t fi = lw(L12_ADDR(0x001fb688) + k * 4);
                            if (fi > 0) {
                                const uint32_t pm = first + (static_cast<uint32_t>(fi) << 8);
                                if (static_cast<int8_t>(lbu(pm + 0x20)) >= 0) moby_retire(pm);
                            }
                        }
                    }
                    sw(L12_ADDR(0x001619b4), 0);
                    cutscene_start(7);
                    moby_retire(m);
                    return;
                }
            }

            if (lh(pv + 0x5fa) == 0) {
                for (uint32_t k = 0; k < 17; ++k) {
                    const uint32_t slot_addr = L12_ADDR(0x001fb688) + k * 4;
                    const uint32_t rec_m = pv + k * 0x30 + 0x20;
                    if (lw(slot_addr) == 0) sw(slot_addr, lwu(rec_m));
                    sw(rec_m, 0xffffffffu);
                }
                sw(L12_ADDR(0x001fb688) + 0x44, 1);
                for (int32_t k = 0; k < 18; ++k) dropship_piece_init(m, k);
            }
            sh(pv + 0x5fa, lhu(pv + 0x5fa) + 1u);
            dropship_pieces_fall(m);
            break;
        }
        }
    }

    dropship_pulse_tint(m);
    if (static_cast<uint32_t>(lbu(m + 0x20) - 1u) < 3u) {
        if (lw(pv + 0x604) == -1) {
            sw(pv + 0x604, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
        }
    } else {
        const int32_t ch = lw(pv + 0x604);
        if (ch != -1) {
            const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
            if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) sub_0028d918(ch);
        }
        sw(pv + 0x604, 0xffffffffu);
    }
}

}  // namespace rac1
