// Pokitaru's (level 11) pufferfish / beach-crab enemy (class 1246, L11:0x314318), its carry hook +0
// (L11:0x316160), and its helpers (0x315968, 0x315f58, 0x316090), ported by hand.
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/pokitaru_own.h>

#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_moby_helpers.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/walker.h>

namespace rac1 {

// Class 1246 preamble: scale, water-death / raft-visibility checks, hit reaction, taunt timer, target selection.
// orig: L11:0x00315968
void pok_315968_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L11_ADDR(0x001622ccu))));
    swc1(pv + 0x1a4, fpu_mul(lwc1(L11_ADDR(0x001622fcu)), lwc1(0x0015ed6cu)));

    const int32_t raft_idx = lw(pv + 0x254);
    const uint32_t st0 = lbu(m + 0x20);
    if (raft_idx != -1) {
        if (st0 != 0xb && st0 != 0x9 && st0 != 0x12 && st0 != 0x11 && st0 != 0x0 && st0 != 0xc && st0 != 0xd) {
            const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (static_cast<uint32_t>(raft_idx) << 8);
            if (lwc1(m + 0x18) < fpu_add(lwc1(raft + 0x18), 0.5f)) {
                enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
                moby_retire(m);
            }
        }
    }
    if (lbu(m + 0x20) == 0xc && lw(pv + 0x254) != -1) {
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        if (moby_in_state(raft, 5) != 0 && lbu(m + 0x31) == 0) {
            moby_retire(m);
            return;
        }
        const uint32_t raft2 = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        if (lbu(raft2 + 0x20) >= 2) {
            sb(m + 0x31, 1);
            sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        }
    }

    const uint32_t sp = ee_stack_alloc(0x20);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    int32_t hit_st = 0;
    float dmg = 0.0f;
    const int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, hit_st, &dmg, nullptr, 4);
    if (hit_st != 1 && lbu(m + 0x20) != 0xb) {
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        int32_t mode = 3;
        if (hp <= 0.0f) {
            mode = 1;
        } else if (lh(lwu(hit + 0x20) + 0xa6) != 0x47) {
            mode = react;
        }
        sw(pv + 0x140, static_cast<uint32_t>(float_to_int(512.0f)));
        swc1(pv + 0x148, u2f(0x3e4ccccdu));
        swc1(pv + 0x134, 0.0f);
        swc1(pv + 0x130, fpu_mul(lwc1(L11_ADDR(0x001622d0u)), lwc1(0x0015ed70u)));
        sw(pv + 0x144, 9);
        sb(pv + 0x15d, 0);
        switch (mode) {  // (jump table L11:0x21b710)
        case 9: case 10:  // (0x00315bcc)
            sb(pv + 0x117, 0xfa);
            break;
        case 3: case 4: case 5: case 6: case 7: case 8: {  // (0x00315bd8)
            const float dt = lwc1(0x0015ed6cu);
            swc1(pv + 0x170, 7.5f);
            swc1(pv + 0x174, 11.0f);
            swc1(pv + 0x138, fpu_mul(lwc1(L11_ADDR(0x001622d8u)), dt));
            swc1(pv + 0x13c, fpu_mul(lwc1(L11_ADDR(0x001622d4u)), dt));
            const uint32_t src = lwu(hit + 0x20);
            float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14)));
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), yaw, *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
            move_state_start(m, pv + 0x120, 3, 1, 0, yaw);
            sb(m + 0x20, (lbu(m + 0x20) < 12) ? 0x9u : 0x11u);
            sb(pv + 0x117, 0x78);
            break;
        }
        case 1: case 2: {  // (0x00315c90)
            const float dt = lwc1(0x0015ed6cu);
            const float dt2 = lwc1(0x0015ed70u);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            swc1(pv + 0x130, fpu_mul(lwc1(L11_ADDR(0x001622d0u)), dt2));
            swc1(pv + 0x170, 7.5f);
            swc1(pv + 0x174, 11.0f);
            swc1(pv + 0x138, fpu_mul(lwc1(L11_ADDR(0x001622e0u)), dt));
            swc1(pv + 0x13c, fpu_mul(lwc1(L11_ADDR(0x001622dcu)), dt));
            const uint32_t src = lwu(hit + 0x20);
            float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14)));
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), yaw, *static_cast<float*>(ee_ram_ptr(pv + 0x138)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x13c)));
            move_state_start(m, pv + 0x120, 3, 1, 0, yaw);
            sb(m + 0x20, (lbu(m + 0x20) < 12) ? 0xbu : 0x12u);
            sb(pv + 0x117, 0xf0);
            moby_drop_bolts(m, 0, -1);
            break;
        }
        default:  // (0x00315d74: 0, 11, >=12)
            break;
        }
        color_pulse_start(m, pv + 0x110);
    }

    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x110);

    if (lw(pv + 0x244) != 0) {
        const uint32_t st = lbu(m + 0x20);
        if (st == 2 || (st == 3 && (lbu(m + 0x70) & 2u) != 0)) {
            sb(m + 0x20, 0xa);
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(0xa));
        }
    }
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x244, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x244)));
    float range = lwc1(pv + 0x23c);
    if (lw(pv + 0x244) != 0) range = fpu_add(range, 64.0f);
    swc1(pv + 0x248, range);

    const uint32_t poly = lvd::path(lw(pv + 0x230));
    const uint32_t res = target_select(m, pv + 0x1d0, 0, 0, poly + 0x10, lw(poly), range);
    if (res != 2) {
        const uint32_t ref_pos = (lbu(m + 0x20) == 1) ? (lvd::path(lw(pv + 0x234)) + 0x20) : (m + 0x10);
        lq_sq(sp, ref_pos);
        if (lwc1(pv + 0x248) < vec2_distance(V(sp), V(pv + 0x1d0)) ||
            3.0f < fpu_sub(lwc1(sp + 0x8), lwc1(pv + 0x1d8))) {
            sw(pv + 0x214, 2);
        }
    }
    if (lwu(pv + 0x210) == 0) {
        sw(pv + 0x210, lwu(0x001413d0u));
        lq_sq(pv + 0x1d0, 0x0013f3d0u);
    }
    ee_stack_free(0x20);
}

// Turns class 1246 toward `target_pos` (offset by pvar +0x24c) and steps forward during frames 8..22 of its walk cycle.
// orig: L11:0x00315f58
void pok_315f58_l11(uint32_t m, uint32_t target_pos) {
    const uint32_t pv = lwu(m + 0x78);
    const float ang = angle_add(atan2_f(fpu_sub(lwc1(target_pos), lwc1(m + 0x10)),
                                        fpu_sub(lwc1(target_pos + 4), lwc1(m + 0x14))),
                                lwc1(pv + 0x24c));
    const float acc = fpu_mul(lwc1(0x0015ed70u), u2f(0x410ba058u));
    const float max_v = fpu_mul(lwc1(0x0015ed6cu), u2f(0x41490fdbu));
    yaw_seek(m + 0x48, pv + 0x250, ang, acc, acc, max_v);
    const float t = moby_anim_time(m);
    if (lbu(m + 0x52) == lbu(m + 0x53) && 8.0f < t && t < 22.0f) {
        swc1(pv + 0x1a4, fpu_mul(lwc1(L11_ADDR(0x001622fcu)), lwc1(0x0015ed6cu)));
        const uint32_t sp = ee_stack_alloc(0x20);
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(sp + 0x0, fpu_add(c, c));
        const float s = sin_vu(lwc1(m + 0x48));
        swc1(sp + 0x4, fpu_add(s, s));
        sw(sp + 0x8, 0);
        walker_record_step(1.0f, m, pv + 0x180, sp, sp + 0x10);
        ee_stack_free(0x20);
    }
}

// Counts class 1246 mates in the moby's group currently in states 13..17 (on the raft).
// orig: L11:0x00316090
int32_t pok_316090_l11(uint32_t m) {
    const uint32_t grp = lbu(m + 0x21);
    if (grp == 0xff) return 0;
    uint32_t e = lvd::group(static_cast<int32_t>(grp));
    if (e == 0) return 0;
    int32_t count = 0;
    const uint32_t mobys = lwu(L11_ADDR(0x0015ffd8u));
    for (;; e += 2) {
        const uint16_t raw = lhu(e);
        const uint32_t o = mobys + (static_cast<uint32_t>(raw & 0x7fffu) << 8);
        if (lh(o + 0xa6) == 0x4de) {
            const uint32_t st = lbu(o + 0x20);
            if (st >= 13 && st < 18) count = static_cast<int16_t>(count + 1);
        }
        if (static_cast<int16_t>(raw) < 0) break;
    }
    return count;
}

// Class 1246 carry hook +0: saves the current state into moby +0xbc and enters state 0x14 while carried.
// orig: L11:0x00316160
int32_t pok_hook_316160_u(uint32_t a0, uint32_t a1, uint32_t a2, float f12) {
    int32_t res = carry_hook0(a0, a1, a2, f12);
    const uint32_t st = lbu(a0 + 0x20);
    if (res != 0) {
        if (st == 2 || st == 4 || st == 5 || st == 6 || st == 0xe || st == 0xf || st == 0x10 || st == 0x14) {
            if (st != 0x14) {
                sb(a0 + 0x20, 0x14);
                sb(a0 + 0xbc, st);
            }
        } else {
            res = 0;
        }
    } else if (st == 0x14) {
        sb(a0 + 0x20, lbu(a0 + 0xbc));
    }
    return res;
}

// Class 1246 update: beach/water/raft pufferfish enemy (states 0..20).
// orig: L11:0x00314318
void pok_class1246_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) != 0x14) {
        sh(pv + 0xc8, 0);
        pok_315968_l11(m);
    }
    const uint32_t tgt = lwu(pv + 0x210);
    joint_node_scale(m, 0, pv + 0x280, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L11_ADDR(0x001677c0u))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }

    auto anim_to = [&](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    auto start_chase = [&](uint32_t next_state) {
        const float spread = lwc1(L11_ADDR(0x001622e8u));
        swc1(pv + 0x24c, fpu_mul(rand_range_f(-spread, spread), u2f(0x3c8efa35u)));
        sb(m + 0x20, next_state);
        anim_to(1, frame_scale_i(0xa));
    };

    const uint32_t sp = ee_stack_alloc(0x60);
    bool do_splash = true;

    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b6b0)
    case 0: {  // (0x003143f4)
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, 2);
        sb(pv + 0x28, 1);
        swc1(pv + 0x30, 0.25f);
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(8.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(2.0f)));
        sb(pv + 0x29, 1);
        sw(pv + 0xd0, L11_ADDR(0x001622c0u));
        walker_record_init_l1(pv + 0x180);
        swc1(pv + 0x18c, 0.5f);
        swc1(pv + 0x188, 0.5f);
        swc1(pv + 0x184, 0.5f);
        sw(pv + 0x180, static_cast<uint32_t>(float_to_int(512.0f)));
        sw(pv + 0x1b8, lwu(pv + 0x1b8) | 0x10u);
        if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        const int32_t kind = lw(pv + 0x238);
        if (kind == 2) {
            sb(m + 0x20, 0x13);
            anim_to(4, frame_scale_i(0xa));
            lq_sq(m + 0x10, lvd::path(lw(pv + 0x234)) + 0x10);
            sw(pv + 0x25c, 1);
        } else if (kind == 1) {
            sb(m + 0x20, 1);
            const uint32_t p = lvd::path(lw(pv + 0x234));
            lq_sq(m + 0x10, p + 0x10);
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 1.0f));
            lq_sq(pv + 0x220, p + 0x20);
        } else if (lw(pv + 0x254) >= 0) {
            sb(m + 0x20, 0xc);
            swc1(pv + 0x258, rand_range_f(-3.0f, 3.0f));
            sb(m + 0x31, 0);
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) | 1u);
        } else {
            swc1(m + 0x18, floor_probe_20(m + 0x10));
            sb(m + 0x20, 2);
            anim_to(0, 5);
            lq_sq(pv + 0x220, m + 0x10);
        }
        break;
    }
    case 1: {  // (0x00314600)
        const uint32_t p = lvd::path(lw(pv + 0x234));
        const float ang = atan2_f(fpu_sub(lwc1(p + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(p + 0x24), lwc1(m + 0x14)));
        const float acc = fpu_mul(lwc1(0x0015ed70u), u2f(0x410ba058u));
        const float max_v = fpu_mul(lwc1(0x0015ed6cu), u2f(0x41490fdbu));
        yaw_seek(m + 0x48, pv + 0x250, ang, acc, acc, max_v);
        if (lw(pv + 0x214) != 2 && rand_mod(9) == 0) {
            sb(m + 0x20, 7);
            anim_to(0, 5);
            const uint32_t dst = lvd::path(lw(pv + 0x234)) + 0x20;
            vec3_sub(V(pv + 0x40), V(dst), V(m + 0x10));
            const float spd = fpu_mul(lwc1(L11_ADDR(0x001622ecu)), lwc1(0x0015ed6cu));
            vec2_set_length(V(pv + 0x40), V(pv + 0x40), spd);
            const float grav = -fpu_mul(lwc1(L11_ADDR(0x001622f0u)), lwc1(0x0015ed70u));
            swc1(pv + 0x48, sub_0025abf0(m + 0x10, lvd::path(lw(pv + 0x234)) + 0x20, nullptr, spd, grav));
        }
        break;
    }
    case 7: case 8: {  // (0x0031474c)
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x40));
        const float vz = fpu_sub(lwc1(pv + 0x48), fpu_mul(lwc1(L11_ADDR(0x001622f0u)), lwc1(0x0015ed70u)));
        swc1(pv + 0x48, vz);
        if (vz < 0.0f) {
            lq_sq(sp, m + 0x10);
            swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), 1.0f));
            const float flr = floor_probe_v(0.5f, V(sp), 0);
            if (lwc1(m + 0x18) < flr) {
                if (lbu(m + 0x20) == 7) {
                    swc1(m + 0x18, flr);
                    sb(m + 0x20, 2);
                } else {
                    sb(m + 0x20, 1);
                    swc1(m + 0x18, fpu_sub(flr, 1.0f));
                }
            }
        }
        break;
    }
    case 2: {  // (0x00314800)
        if (lw(pv + 0x214) != 2) {
            start_chase(4);
        } else if ((lbu(m + 0x70) & 2u) != 0 && rand_mod(3) != 0 && lbu(m + 0x31) != 0 &&
                   vec3_distance(V(m + 0x10), V(L11_ADDR(0x001677c0u))) < 32.0f) {
            const float a = rand_angle();
            const float r = rand_range_f(1.0f, 3.0f);
            swc1(pv + 0x260, fpu_mul(cos_vu(a), r));
            swc1(pv + 0x264, fpu_mul(sin_vu(a), r));
            sw(pv + 0x268, 0);
            vec3_add(V(pv + 0x260), V(pv + 0x260), V(pv + 0x220));
            sb(m + 0x20, 3);
            anim_to(1, frame_scale_i(0xa));
        }
        break;
    }
    case 3: {  // (0x00314934)
        pok_315f58_l11(m, pv + 0x260);
        if (lw(pv + 0x214) != 2) {
            start_chase(4);
        } else if ((lbu(m + 0x70) & 2u) != 0 && rand_mod(3) == 0) {
            sb(m + 0x20, 2);
            anim_to(0, frame_scale_i(0xa));
        }
        break;
    }
    case 4: {  // (0x003149e0)
        if ((lbu(m + 0x70) & 2u) != 0 && vec2_distance(V(m + 0x10), V(tgt + 0x10)) < lwc1(L11_ADDR(0x001622e4u))) {
            sb(m + 0x20, 5);
            anim_to(2, frame_scale_i(0xa));
        } else {
            pok_315f58_l11(m, tgt + 0x10);
        }
        if (lw(pv + 0x214) == 2) sb(m + 0x20, 6);
        break;
    }
    case 10: {  // (0x00314a68)
        vec3_sub(V(sp), V(pv + 0x1d0), V(m + 0x10));
        sw(sp + 0x8, 0);
        vec3_set_length(V(sp), V(sp), 2.5f);
        vec3_add(V(sp), V(sp), V(m + 0x10));
        if (path_crossed(lwu(pv + 0x230), m + 0x10, sp) != 0) {
            const uint32_t t = lwu(pv + 0x210);
            const float ang = atan2_f(fpu_sub(lwc1(t + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(t + 0x14), lwc1(m + 0x14)));
            const float acc = fpu_mul(lwc1(0x0015ed70u), u2f(0x41490fdbu));
            const float max_v = fpu_mul(lwc1(0x0015ed6cu), u2f(0x41490fdbu));
            yaw_seek(m + 0x48, pv + 0x250, ang, acc, acc, max_v);
        } else {
            pok_315f58_l11(m, pv + 0x1d0);
        }
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lw(pv + 0x214) != 2) sb(m + 0x20, 4);
            else if (lw(pv + 0x244) == 0) sb(m + 0x20, 6);
        }
        break;
    }
    case 5: {  // (0x00314b68)
        pok_315f58_l11(m, tgt + 0x10);
        if ((lbu(m + 0x70) & 2u) != 0 && lwc1(L11_ADDR(0x001622e4u)) < vec2_distance(V(m + 0x10), V(tgt + 0x10))) {
            sb(m + 0x20, 4);
            anim_to(1, frame_scale_i(0xa));
        } else if (lw(pv + 0x214) == 2) {
            sb(m + 0x20, 6);
        }
        lq_sq(sp, m + 0x10);
        swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), u2f(0x3eaa7efau)));
        moby_sphere_contact(m, sp, 1, 0, 1, 0, fpu_mul(lwc1(L11_ADDR(0x001622ccu)), u2f(0x3f19999au)), 1.0f, 1.0f);
        break;
    }
    case 6: {  // (0x00314c48)
        if (vec2_distance(V(m + 0x10), V(pv + 0x220)) < 1.0f) {
            sb(m + 0x20, 2);
            anim_to(0, frame_scale_i(0xa));
        } else if (lw(pv + 0x214) != 2) {
            start_chase(4);
        } else {
            pok_315f58_l11(m, pv + 0x220);
        }
        if ((lbu(m + 0x70) & 2u) != 0) anim_to(1, 5);
        break;
    }
    case 9: {  // (0x00314d40)
        const uint32_t phy = enemy_physics(m, pv + 0x120);
        if ((phy & 0x40u) != 0) {
            if (lwc1(m + 0x18) < 222.0f) {
                moby_retire(m);
                do_splash = false;
                break;
            }
            const uint32_t poly = lvd::path(lw(pv + 0x230));
            if (point_in_polygon(m + 0x10, poly + 0x10, lw(poly)) == 0) {
                moby_retire(m);
                do_splash = false;
                break;
            }
            sb(m + 0x20, 2);
            anim_to(0, frame_scale_i(0xa));
            do_splash = false;
        } else if ((phy & 0x120u) != 0) {
            moby_retire(m);
            do_splash = false;
        }
        break;
    }
    case 11: {  // (0x00314dc0)
        if ((enemy_physics(m, pv + 0x120) & 0x160u) != 0) {
            vec_clear(V(sp));
            enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
            moby_retire(m);
            do_splash = false;
        }
        break;
    }
    case 12: {  // (0x00314dfc)
        if (lw(pv + 0x214) == 2 || pok_316090_l11(m) >= 5) break;
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        mat3_transpose(M(sp + 0x10), M(raft + 0xc0));
        vec3_sub(V(sp), V(m + 0x10), V(raft + 0x10));
        mat4_transform(V(sp), V(sp), M(sp + 0x10));
        const float lx = lwc1(sp);
        if (lx < lwc1(pv + 0x258) && u2f(0xc0466666u) < lx) {
            swc1(sp + 0x50, lx);
            const float ly = (0.0f < lwc1(sp + 4)) ? rand_range_f(1.5f, 2.0f) : rand_range_f(-2.0f, -1.5f);
            swc1(sp + 0x54, ly);
            swc1(sp + 0x58, 1.0f);
            mat3_transform(V(sp + 0x50), V(sp + 0x50), M(raft + 0xc0));
            vec3_add(V(sp + 0x50), V(sp + 0x50), V(raft + 0x10));
            sb(m + 0x20, 0xd);
            anim_to(1, 5);
            vec3_sub(V(pv + 0x40), V(sp + 0x50), V(m + 0x10));
            const int32_t s1 = frame_scale_i(0x3c);
            const float dist = vec3_length(V(pv + 0x40));
            const float spd = fpu_mul(lwc1(L11_ADDR(0x001622ecu)), lwc1(0x0015ed6cu));
            swc1(m + 0x58, fpu_div(fpu_mul(static_cast<float>(s1), spd), dist));
            vec2_set_length(V(pv + 0x40), V(pv + 0x40), spd);
            const float grav = -fpu_mul(lwc1(L11_ADDR(0x001622f0u)), lwc1(0x0015ed70u));
            swc1(pv + 0x48, sub_0025abf0(m + 0x10, sp + 0x50, nullptr, spd, grav));
            swc1(m + 0x48, atan2_f(lwc1(pv + 0x40), lwc1(pv + 0x44)));
        }
        break;
    }
    case 13: {  // (0x00314fec)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        const uint32_t rpv = lwu(raft + 0x78);
        vec3_add(V(m + 0x10), V(m + 0x10), V(pv + 0x40));
        vec3_add(V(m + 0x10), V(m + 0x10), V(rpv + 0x70));
        const float vz = fpu_sub(lwc1(pv + 0x48), fpu_mul(lwc1(L11_ADDR(0x001622f0u)), lwc1(0x0015ed70u)));
        swc1(pv + 0x48, vz);
        if (vz < 0.0f) {
            lq_sq(sp, m + 0x10);
            swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), 1.0f));
            if (lwc1(m + 0x18) < floor_probe_v(0.5f, V(sp), 0)) {
                swc1(m + 0x58, 1.0f);
                sb(m + 0x20, 0xe);
            }
        }
        break;
    }
    case 14: {  // (0x003150a4)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        sub_00260400(m, raft, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        if (lw(pv + 0x214) != 2) start_chase(0xf);
        break;
    }
    case 15: {  // (0x0031512c)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        sub_00260400(m, raft, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        pok_315f58_l11(m, tgt + 0x10);
        if ((lbu(m + 0x70) & 2u) != 0 && vec2_distance(V(m + 0x10), V(tgt + 0x10)) < lwc1(L11_ADDR(0x001622e4u))) {
            sb(m + 0x20, 0x10);
            anim_to(2, frame_scale_i(0xa));
        }
        if (lw(pv + 0x214) == 2) sb(m + 0x20, 0xe);
        break;
    }
    case 16: {  // (0x003151e0)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        sub_00260400(m, raft, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        pok_315f58_l11(m, tgt + 0x10);
        if ((lbu(m + 0x70) & 2u) != 0 && lwc1(L11_ADDR(0x001622e4u)) < vec2_distance(V(m + 0x10), V(tgt + 0x10))) {
            sb(m + 0x20, 0xf);
            anim_to(1, frame_scale_i(0xa));
        } else if (lw(pv + 0x214) == 2) {
            sb(m + 0x20, 0xe);
        }
        lq_sq(sp, m + 0x10);
        swc1(sp + 0x8, fpu_add(lwc1(sp + 0x8), u2f(0x3eaa7efau)));
        moby_sphere_contact(m, sp, 1, 0, 1, 0, fpu_mul(lwc1(L11_ADDR(0x001622ccu)), u2f(0x3f19999au)), 1.0f, 0.0f);
        break;
    }
    case 17: {  // (0x003152fc)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        sub_00260400(m, raft, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        const uint32_t phy = enemy_physics(m, pv + 0x120);
        if ((phy & 0x40u) != 0) {
            if (lwc1(m + 0x18) < 222.0f) {
                moby_retire(m);
                do_splash = false;
                break;
            }
            const uint32_t poly = lvd::path(lw(pv + 0x230));
            if (point_in_polygon(m + 0x10, poly + 0x10, lw(poly)) == 0) {
                moby_retire(m);
                do_splash = false;
                break;
            }
            sb(m + 0x20, 0xe);
            anim_to(0, frame_scale_i(0xa));
            do_splash = false;
        } else if ((phy & 0x120u) != 0) {
            moby_retire(m);
            do_splash = false;
        }
        break;
    }
    case 18: {  // (0x003153d4)
        const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
        sub_00260400(m, raft, m + 0x10, m + 0x40, m + 0x10, m + 0x40);
        if ((enemy_physics(m, pv + 0x120) & 0x160u) != 0) {
            vec_clear(V(sp));
            enemy_death_burst(m, m + 0x10, 0.5f, 10.0f);
            moby_retire(m);
            do_splash = false;
        }
        break;
    }
    case 19: {  // (0x00315454)
        const uint32_t p = lvd::path(lw(pv + 0x234));
        lq_sq(sp, p + 0x10 + (lwu(pv + 0x25c) << 4));
        if (lw(pv + 0x254) >= 0) {
            const uint32_t raft = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 0x254) << 8);
            const float cap_z = fpu_sub(lwc1(raft + 0x18), 1.0f);
            if (cap_z < lwc1(sp + 0x8)) swc1(sp + 0x8, cap_z);
        }
        const float ang = atan2_f(fpu_sub(lwc1(sp), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 4), lwc1(m + 0x14)));
        const float acc = fpu_mul(lwc1(0x0015ed70u), u2f(0x41490fdbu));
        const float max_v = fpu_mul(lwc1(0x0015ed6cu), u2f(0x41490fdbu));
        yaw_seek(m + 0x48, pv + 0x250, ang, acc, acc, max_v);
        const float dt = lwc1(0x0015ed6cu);
        const float spd = fpu_add(dt, dt);
        swc1(sp + 0x10, fpu_mul(cos_vu(lwc1(m + 0x48)), spd));
        swc1(sp + 0x14, fpu_mul(sin_vu(lwc1(m + 0x48)), spd));
        const float dz = fpu_sub(lwc1(sp + 0x8), lwc1(m + 0x18));
        const float zlim = fpu_mul(dt, 4.0f);
        if (zlim < dz) swc1(sp + 0x18, zlim);
        else if (dz < -zlim) swc1(sp + 0x18, -zlim);
        else swc1(sp + 0x18, dz);
        walker_move(m, sp + 0x10, 0x10, 0.5f, 0.5f, 0.0f);
        if (vec3_distance(V(sp), V(m + 0x10)) < 1.0f) {
            const uint32_t p2 = lvd::path(lw(pv + 0x234));
            sw(pv + 0x25c, static_cast<uint32_t>(ee_rem(lw(pv + 0x25c) + 1, lw(p2))));
        }
        if (lwc1(m + 0x54) == 0.0f) {
            const float d = vec2_distance(V(m + 0x10), V(0x0013f3d0u));
            const uint32_t a = lbu(m + 0x53);
            if (a == 4) {
                if (d < 6.0f) moby_anim_set(m, 5, 0, frame_scale_i(0xa));
            } else if (a == 5) {
                moby_anim_set(m, 6, 0, frame_scale_i(0xa));
            } else if (a == 6) {
                if (8.0f < d) {
                    moby_anim_set(m, 7, 0, frame_scale_i(0xa));
                } else if (d < 4.0f) {
                    lq_sq(sp + 0x20, m + 0x10);
                    swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), 0.5f));
                    moby_sphere_contact(m, sp + 0x20, 1, 0, 1, 0, fpu_mul(lwc1(L11_ADDR(0x001622ccu)), u2f(0x3f0ccccdu)), 1.0f, 1.0f);
                }
            } else if (a == 7) {
                moby_anim_set(m, 4, 0, frame_scale_i(0xa));
            }
        }
        break;
    }
    case 20: {  // (0x00315790)
        if (lh(pv + 0xc8) != 0 && carry_update(m, pv + 0x120) != 0) {
            sb(m + 0x20, lbu(m + 0xbc));
            sh(pv + 0xc8, 0);
        }
        break;
    }
    default:
        break;
    }

    if (do_splash && lwc1(pv + 0x17c) != 0.0f) {
        const uint32_t st = lbu(m + 0x20);
        if (st == 9 || st == 0xb || st == 0x11 || st == 0x12) {
            for (int32_t k = 0; k < 3; ++k) {
                swc1(sp + 0x0, fpu_add(lwc1(m + 0x10), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
                swc1(sp + 0x4, fpu_add(lwc1(m + 0x14), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))));
                swc1(sp + 0x8, fpu_add(fpu_add(lwc1(m + 0x18), rand_range_signed_f(0.0f, u2f(0x3e4ccccdu))), u2f(0x3f4ccccdu)));
                const float a = rand_angle();
                const float r = fpu_mul(rand_range_f(u2f(0x3e4ccccdu), u2f(0x3fa66666u)), lwc1(0x0015ed6cu));
                swc1(sp + 0x10, fpu_mul(cos_vu(a), r));
                swc1(sp + 0x14, fpu_mul(sin_vu(a), r));
                swc1(sp + 0x18, fpu_mul(rand_range_f(u2f(0xbe4ccccdu), u2f(0x3ecccccd)), lwc1(0x0015ed6cu)));
                spawn_particle_22(sp, sp + 0x10, rand_range_f(2100.0f, 14700.0f), 222.5f);
            }
        }
    }
    ee_stack_free(0x60);
}

}  // namespace rac1
