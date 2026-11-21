// Umbris's own moby classes: the hovering proximity mine (class 1110) and the parent-tethered homing mine (class 1112,
// retired with parent cooldown via 0x319698) — hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in
// umbris_mobys.cpp. Addresses are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are bare (the speed
// 0x15ed60, the frame time 0x15ed6c, its square 0x15ed70), the hit record the collision facades'.
//
// Class 1110's pvars: its hit state +0x20 (+0x38 an optional trigger moby), its home +0x60, its chase speed +0x70, its
// group +0x74 (-1: none), its chase lifetime +0x78 and its vertical bob timer +0x7c.
// Class 1112's pvars: its hit state +0x20 (+0x38 an optional trigger moby), its velocity +0x60, its home +0x70, its anchor
// position +0x80, its lifetime / respawn timer +0x98, its initial speed +0x9c, its parent moby +0xa0 (whose pvars hold +0x7c
// speed, +0x84 respawn cooldown, +0x88 wake cone in degrees, +0x8c wake range), its yaw / pitch rates +0xa4 / +0xa8 and its
// bob timer +0xac.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// whether a moby class is one of the mines / dispensers that a chasing mine's contact probe ignores
bool mine_ignored_class(int32_t cls) {
    return cls == 0x363 || cls == 0x456 || cls == 0x458 || cls == 0x365 || cls == 0x367;
}

// vertical bob (+0xbc: 1 rising for 7 frames, 2 falling for 7 frames, then 0)
void mine_bob(uint32_t m, uint32_t timer_addr) {
    const uint32_t bob = lbu(m + 0xbc);
    if (bob == 1) {
        const float step = fpu_mul(lwc1(0x0015ed60), u2f(0x3dcccccdu));  // (0.1)
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), step));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer_addr))) != 0) {
            sw(timer_addr, static_cast<uint32_t>(frame_scale_i(7)));
            sb(m + 0xbc, 2);
        }
    } else if (bob == 2) {
        const float step = fpu_mul(lwc1(0x0015ed60), u2f(0x3dcccccdu));
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), step));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer_addr))) != 0) {
            sb(m + 0xbc, 0);
        }
    }
}

// standard mine explosion burst along `vel_addr`
void mine_explode(uint32_t m, uint32_t vel_addr) {
    explosion_effect(m, vel_addr, 0, 3, 3, 5, 2, 0, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
}
}  // namespace

// Class 1110, the hovering proximity mine (+0x20, jump table L7:0x002119b0: 0 set up, 1 idle spin, 2 start chase anim,
// 3 chasing). Shot by anything other than 0x456 / 0x365 / 0x367: explodes, drops bolts and retires.
// orig: L7:0x00319040
void umb_class1110_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t tgt = sp + 0x20, step_v = sp + 0x70, hit_desc = sp + 0x80, sph = sp + 0xb0, push = sp + 0xc0;
    const uint32_t pos = m + 0x10;

    target_select_any(m, tgt, 8.0f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L7_ADDR(0x00166e40))) < 27.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }

    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0);
    int32_t hit_st = lw(sp + 0xd0);
    moby_hit_react_r(m, hit, pv + 0x20, 0, hit_st, nullptr, nullptr, 4);
    sw(sp + 0xd0, static_cast<uint32_t>(hit_st));
    if (hit_st >= 2 && hit != 0) {
        const int32_t by_cls = lh(lwu(hit + 0x20) + 0xa6);
        if (by_cls != 0x456 && by_cls != 0x365 && by_cls != 0x367) {
            vec_clear(V(step_v));
            mine_explode(m, step_v);
            moby_drop_bolts(m, 0, -1);
            moby_retire(m);
            ee_stack_free(0x160);
            return;
        }
    }

    switch (lbu(m + 0x20)) {  // (jump table L7:0x002119b0)
        case 0: {  // (0x003191ec)
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            V(pv + 0x60) = V(pos);
            const uint32_t draw_dist = lwu(lwu(m + 0x24) + 0x10);
            sb(m + 0x20, 1);
            sw(m + 0x94, draw_dist);
            break;
        }
        case 1: {  // (0x00319220)
            sw(pv + 0x78, static_cast<uint32_t>(frame_scale_i(0x258)));
            swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu))));  // (2 * pi)
            bool wake = false;
            const uint32_t link = lwu(pv + 0x38);
            if (link != 0 && vec2_distance(V(pos), V(link + 0x10)) < 20.0f &&
                abs_f(fpu_sub(lwc1(m + 0x18), lwc1(link + 0x18))) < 3.0f) {
                wake = true;
            } else if (lwu(sp + 0x60) != 0 && vec3_distance(V(pos), V(tgt)) < 8.0f) {
                wake = true;
            }
            if (wake) {
                sw(pv + 0x70, 0);
                sw(pv + 0x78, static_cast<uint32_t>(frame_scale_i(0x258)));
                sb(m + 0xbc, 1);
                sw(pv + 0x7c, static_cast<uint32_t>(frame_scale_i(7)));
                sb(m + 0x20, 3);
                const int32_t grp = lw(pv + 0x74);
                if (grp != -1) moby_group_set_state(grp, 2);
                sb(m + 0x20, 2);
            }
            break;
        }
        case 2: {  // (0x00319330)
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 10);
            sb(m + 0x20, 3);
            [[fallthrough]];
        }
        case 3: {  // (0x00319358)
            if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x52) == 1 && lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, 10);
            }
            const float next_spd = fpu_add(lwc1(pv + 0x70), fpu_mul(lwc1(0x0015ed70), 30.0f));
            const float vmax = fpu_mul(lwc1(0x0015ed6c), 6.0f);
            swc1(pv + 0x70, vmax < next_spd ? vmax : next_spd);

            mine_bob(m, pv + 0x7c);

            vec3_sub(V(step_v), V(tgt), V(pos));
            vec3_set_length(V(step_v), V(step_v), lwc1(pv + 0x70));
            vec3_add(V(pos), V(pos), V(step_v));
            if (64.0f < vec3_distance(V(pos), V(pv + 0x60))) {
                moby_retire(m);
                ee_stack_free(0x160);
                return;
            }

            sub_00259888(1.0f, hit_desc, static_cast<int32_t>(m), 0x10001, step_v);
            V(sph) = V(pos);
            swc1(sph + 0x8, fpu_add(lwc1(sph + 0x8), 0.75f));
            bool explode = false;
            if (mobys_in_sphere(sph, 0.75f, 0x10, m, hit_desc) != 0) {
                const uint32_t hm = coll_hit_moby();
                if (hm != 0 && !mine_ignored_class(lh(hm + 0xa6))) explode = true;
            }
            if (!explode) {
                V(sph) = V(pos);
                const float rad = u2f(0x3f0ccccdu);  // (0.55)
                swc1(sph + 0x8, fpu_add(lwc1(sph + 0x8), rad));
                if (sphere_collide(sph, rad, 0, m) != 0) {
                    vec3_sub(V(push), coll_hit_pushed(), V(sph));
                    vec3_add(V(pos), V(pos), V(push));
                }
                if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x78))) != 0) explode = true;
            }
            if (explode) {
                mine_explode(m, step_v);
                moby_retire(m);
                ee_stack_free(0x160);
                return;
            }
            break;
        }
        default:  // (4 and >= 5: 0x0031965c)
            break;
    }

    floor_light_add(m, u2f(0x3f333333u));  // (0.7)
    ee_stack_free(0x160);
}

// Retires class 1112 mine `m` after setting its +0x98 respawn timer from its parent's pvars +0x84 (when `parent_pv` != 0).
// orig: L7:0x00319698
void umb_319698_l7(uint32_t m, uint32_t parent_pv) {
    const uint32_t pv = lwu(m + 0x78);
    if (parent_pv != 0) {
        sw(pv + 0x98, static_cast<uint32_t>(frame_scale_i(lw(parent_pv + 0x84))));
    }
    moby_retire(m);
}

// Class 1112, the parent-tethered homing mine (+0x20, jump table L7:0x002119d0: 0 set up, 1 tethered on +0x80 waiting for
// a target inside its parent's range +0x8c and cone +0x88, 2 homing with damped yaw / pitch, 3 immediate explosion).
// orig: L7:0x003196e0
void umb_class1112_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t parent = lwu(pv + 0xa0);
    if (parent == 0) return;
    const uint32_t ppv = lwu(parent + 0x78);

    const uint32_t sp = ee_stack_alloc(0x170);
    const uint32_t tgt = sp + 0x20, acc_v = sp + 0x70, hit_desc = sp + 0x80, sph = sp + 0xb0, push = sp + 0xc0;
    const uint32_t pos = m + 0x10;
    const uint32_t vel = pv + 0x60;

    target_select_any(m, tgt, lwc1(ppv + 0x8c));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L7_ADDR(0x00166e40))) < 27.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }

    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0);
    int32_t hit_st = lw(sp + 0xd0);
    moby_hit_react_r(m, hit, pv + 0x20, 0, hit_st, nullptr, nullptr, 4);
    sw(sp + 0xd0, static_cast<uint32_t>(hit_st));
    if (hit_st >= 2 && hit != 0) {
        const int32_t by_cls = lh(lwu(hit + 0x20) + 0xa6);
        if (by_cls != 0x363 && by_cls != 0x458 && by_cls != 0x367) {
            mine_explode(m, vel);
            moby_drop_bolts(m, 0, -1);
            umb_319698_l7(m, ppv);
        }
    }

    const float two_pi = u2f(0x40c90fdbu);
    const float pi = u2f(0x40490fdbu);

    switch (lbu(m + 0x20)) {  // (jump table L7:0x002119d0)
        case 0: {  // (0x00319894)
            if (ppv == 0) break;
            sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
            vec_clear(V(vel));
            V(pv + 0x70) = V(pos);
            sw(m + 0x44, 0);
            swc1(m + 0x48, atan2_f(fpu_sub(lwc1(pv + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x84), lwc1(m + 0x14))));
            sb(pv + 0x29, 0);
            sb(m + 0x20, 1);
            sb(m + 0xbc, 0);
            break;
        }
        case 1: {  // (0x003198f4)
            vec3_sub(V(vel), V(pv + 0x80), V(pos));
            const float yaw = atan2_f(fpu_sub(lwc1(pv + 0x80), lwc1(m + 0x10)), fpu_sub(lwc1(pv + 0x84), lwc1(m + 0x14)));
            const float dt = lwc1(0x0015ed6c), dt2 = lwc1(0x0015ed70);
            yaw_turn_damped(m, pv + 0xa4, yaw, fpu_mul(dt2, two_pi), fpu_mul(dt, pi), fpu_mul(dt, two_pi));
            V(pos) = V(pv + 0x80);

            bool wake = false;
            const uint32_t link = lwu(pv + 0x38);
            if (link != 0 && vec2_distance(V(pos), V(link + 0x10)) < 20.0f &&
                abs_f(fpu_sub(lwc1(m + 0x18), lwc1(link + 0x18))) < 3.0f) {
                wake = true;
            } else if (lwu(sp + 0x60) != 0 && vec3_distance(V(pos), V(tgt)) < lwc1(ppv + 0x8c) &&
                       2.0f < vec2_distance(V(pos), V(lwu(pv + 0xa0) + 0x10))) {
                const float aim = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x4), lwc1(m + 0x14)));
                if (angle_dist(lwc1(m + 0x48), aim) < fpu_mul(lwc1(ppv + 0x88), u2f(0x3c8efa35u))) {
                    wake = true;
                }
            }
            if (wake) {
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, 10);
                swc1(pv + 0x9c, fpu_mul(lwc1(ppv + 0x7c), lwc1(0x0015ed6c)));
                sw(pv + 0x98, static_cast<uint32_t>(frame_scale_i(0x258)));
                sb(m + 0xbc, 1);
                sw(pv + 0xac, static_cast<uint32_t>(frame_scale_i(7)));
                sb(m + 0x20, 2);
            }
            break;
        }
        case 2: {  // (0x00319ad0)
            if ((lbu(m + 0x70) & 2u) != 0 && lbu(m + 0x52) == 1 && lbu(m + 0x53) != 2) {
                moby_anim_set(m, 2, 0, 10);
            }
            mine_bob(m, pv + 0xac);

            const float goal_yaw = atan2_f(fpu_sub(lwc1(tgt + 0x0), lwc1(m + 0x10)), fpu_sub(lwc1(tgt + 0x4), lwc1(m + 0x14)));
            const float dt0 = lwc1(0x0015ed6c), dt2_0 = lwc1(0x0015ed70);
            yaw_turn_damped(m, pv + 0xa4, goal_yaw, fpu_mul(dt2_0, pi), fpu_mul(dt0, pi), fpu_mul(dt0, two_pi));

            const float hdist = vec2_distance(V(pos), V(tgt));
            const float goal_pitch = atan2_f(hdist, fpu_sub(lwc1(tgt + 0x8), lwc1(m + 0x18)));
            const float dt1 = lwc1(0x0015ed6c), dt2_1 = lwc1(0x0015ed70);
            swc1(m + 0x44,
                 angle_seek_rate(pv + 0xa8, lwc1(m + 0x44), goal_pitch, fpu_mul(dt2_1, pi), fpu_mul(dt1, pi), fpu_mul(dt1, two_pi)));

            vec3_sub(V(acc_v), V(tgt), V(pos));
            if (lbu(m + 0xbc) != 0) sw(acc_v + 0x8, 0);
            vec3_set_length(V(acc_v), V(acc_v), fpu_mul(lwc1(0x0015ed70), 30.0f));
            vec3_add(V(vel), V(vel), V(acc_v));
            vec3_clamp_length_vu(V(vel), V(vel), fpu_mul(lwc1(0x0015ed6c), 10.0f));
            vec3_add(V(pos), V(pos), V(vel));
            if (64.0f < vec3_distance(V(pos), V(pv + 0x70))) {
                umb_319698_l7(m, ppv);
            }

            sub_00259888(1.0f, hit_desc, static_cast<int32_t>(m), 0x10001, vel);
            V(sph) = V(pos);
            swc1(sph + 0x8, fpu_add(lwc1(sph + 0x8), 0.75f));
            if (mobys_in_sphere(sph, 0.75f, 0x10, m, hit_desc) != 0) {
                const uint32_t hm = coll_hit_moby();
                if (hm != 0 && !mine_ignored_class(lh(hm + 0xa6))) {
                    mine_explode(m, vel);
                    umb_319698_l7(m, ppv);
                }
            }

            V(sph) = V(pos);
            const float rad = u2f(0x3f0ccccdu);  // (0.55)
            swc1(sph + 0x8, fpu_add(lwc1(sph + 0x8), rad));
            if (sphere_collide(sph, rad, 0, m) != 0) {
                vec3_sub(V(push), coll_hit_pushed(), V(sph));
                vec3_add(V(pos), V(pos), V(push));
            }
            if (lw(pv + 0x98) == 0) {
                umb_319698_l7(m, ppv);
            }
            break;
        }
        case 3: {  // (0x00319e90)
            mine_explode(m, vel);
            umb_319698_l7(m, ppv);
            break;
        }
        default:  // (4 and >= 5: 0x00319f00)
            break;
    }

    floor_light_add(m, u2f(0x3f333333u));  // (0.7)
    ee_stack_free(0x170);
}

}  // namespace rac1
