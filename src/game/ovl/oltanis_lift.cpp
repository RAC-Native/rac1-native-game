// Oltanis (level 14), class 903: a lift that rides a path between its two ends.
// Hidden until the hero enters its box (pvar +0xcc), it rests at one end (sub-state m+0xbc 0: the first point, 1: the
// last) and leaves when the hero stands on it (once he has stepped off it before: +0xb8) or is nearer the other end;
// sub-state 2 glides along the path (the place +0xa4 from 0 to 1, +0xb0 seconds a trip), pauses while the hero stands
// under it, and carries what stands on it (attach_frame_set). Until the hero has entered its activation box (+0xc0;
// remembered in the level's save bits by the moby's uid) it stays at its end.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l14.h>

#include <cstdint>

namespace rac1 {

namespace {

constexpr uint32_t kHeroPos = 0x0013f3d0u;

void stop_platform_sound(uint32_t m, uint32_t pv) {
    const int32_t ch = lw(pv + 0xc4);
    if (ch != -1) {
        const uint32_t slot = 0x0013e550u + static_cast<uint32_t>(ch * 0x70);
        if (lwu(slot + 0x88) == m && lbu(slot + 0x74) != 0) {
            sub_0028d918(ch);
        }
    }
    sw(pv + 0xc4, static_cast<uint32_t>(-1));
}

// orig: L14:0x002ac2b8 -- spring platform toward its resting path endpoint
void platform_spring_to_end_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t path = lvd::path(lw(pv + 0xb4));
    const uint32_t pt_addr = (0.5f < lwc1(pv + 0xa4))
                                 ? (path + (static_cast<uint32_t>(lw(path)) << 4))
                                 : (path + 0x10u);
    const Vec4 target = V(pt_addr);
    const float k = u2f(0x3ba3d70au);     // 0.005f
    const float damp = u2f(0x3e4ccccdu);  // 0.2f
    spring_to(m + 0x10, pv + 0xd0, target.x, k, damp, 0.0f);
    spring_to(m + 0x14, pv + 0xd4, target.y, k, damp, 0.0f);
    spring_to(m + 0x18, pv + 0xd8, target.z, k, damp, 0.0f);
}

}  // namespace

// orig: L14:0x002aba80
void olt_class903_update_l14(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t path_idx = lw(pv + 0xb4);
    if (path_idx == -1) return;

    const uint32_t path = lvd::path(path_idx);
    bool activated = false;

    const int32_t hide_box = lw(pv + 0xcc);
    if (hide_box != -1) {
        if (point_in_box(kHeroPos, hide_box) == 0) {
            sb(m + 0x30, 0xff);
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) | 0x41u));
            sw(m + 0x94, 0);
            return;
        }
        const int32_t hide_box2 = lw(pv + 0xcc);
        if (hide_box2 != -1 && point_in_box(kHeroPos, hide_box2) != 0) {
            sh(m + 0x34, static_cast<uint16_t>(lhu(m + 0x34) & 0xffbeu));
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sw(pv + 0xcc, static_cast<uint32_t>(-1));
        }
    }

    const int32_t act_box = lw(pv + 0xc0);
    if (act_box == -1) {
        activated = true;
    } else {
        bool triggered = (point_in_box(kHeroPos, act_box) != 0);
        if (!triggered) {
            const int16_t uid = lh(m + 0xb2);
            if (lbu(L14_ADDR(0x001bb930) + static_cast<uint32_t>(static_cast<int32_t>(uid)) + 0x454u) != 0) {
                triggered = true;
            } else {
                const uint32_t lvl = lwu(0x0015ed84);
                const uint32_t save_word = lwu(0x0014c190u + (lvl << 8) + (static_cast<uint32_t>(uid >> 5) << 2));
                if (((static_cast<int32_t>(save_word) >> (uid & 0x1f)) & 1) != 0) {
                    triggered = true;
                }
            }
        }
        if (triggered) {
            if (lw(pv + 0xc0) != -1) {
                const int16_t uid = lh(m + 0xb2);
                const uint32_t lvl = lwu(0x0015ed84);
                const uint32_t bit = 1u << (uid & 0x1f);
                const uint32_t save_addr = 0x0014c190u + (lvl << 8) + (static_cast<uint32_t>(uid >> 5) << 2);
                sw(save_addr, lwu(save_addr) | bit);
                const uint32_t lvl_addr = L14_ADDR(0x001babd0) + (static_cast<uint32_t>(uid >> 5) << 2);
                sw(lvl_addr, lwu(lvl_addr) | bit);
                sw(pv + 0xc0, static_cast<uint32_t>(-1));
            }
            activated = true;
        }
    }

    const Vec4 prev_pos = V(m + 0x10);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xba)));

    if (lbu(m + 0x20) == 0) {
        const uint8_t start_end = lbu(pv + 0xa0);
        sb(pv + 0x28, 4);
        sh(pv + 0x3e, 5);
        sw(pv + 0x20, 0);
        sh(pv + 0x24, 0);
        sb(m + 0xbc, start_end);
        const float t0 = (start_end != 0) ? 0.0f : 1.0f;
        swc1(pv + 0xa4, t0);
        uint32_t pt = path + 0x10u;
        if (float_to_int(t0) != 0) {
            pt += static_cast<uint32_t>(lw(path) - 1) << 4;
        }
        V(m + 0x10) = V(pt);
        sh(pv + 0xba, 0);
        sw(pv + 0xa8, 0);
        sw(pv + 0xbc, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
        sw(pv + 0xc4, static_cast<uint32_t>(-1));
    }

    const uint8_t substate = lbu(m + 0xbc);
    if (substate == 0) {
        if (activated) {
            bool depart = false;
            if (lh(pv + 0xb8) != 0 && moby_hero_on_l1(m) != 0) {
                depart = true;
            } else {
                const uint32_t last_pt = path + (static_cast<uint32_t>(lw(path)) << 4);
                const float d_last = vec3_distance(V(kHeroPos), V(last_pt));
                const float d_first = vec3_distance(V(kHeroPos), V(path + 0x10u));
                depart = (d_first < d_last);
            }
            if (depart) {
                sb(m + 0xbc, 2);
                const float dur = frame_scale_f(fpu_mul(lwc1(pv + 0xb0), 60.0f));
                sh(pv + 0xb8, 0);
                swc1(pv + 0xac, fpu_div(-1.0f, dur));
            }
        }
    } else if (substate == 1) {
        if (activated) {
            bool depart = false;
            if (lh(pv + 0xb8) != 0 && moby_hero_on_l1(m) != 0) {
                depart = true;
            } else {
                const float d_first = vec3_distance(V(kHeroPos), V(path + 0x10u));
                const uint32_t last_pt = path + (static_cast<uint32_t>(lw(path)) << 4);
                const float d_last = vec3_distance(V(kHeroPos), V(last_pt));
                depart = (d_last < d_first);
            }
            if (depart) {
                sb(m + 0xbc, 2);
                const float dur = frame_scale_f(fpu_mul(lwc1(pv + 0xb0), 60.0f));
                sh(pv + 0xb8, 0);
                swc1(pv + 0xac, fpu_div(1.0f, dur));
            }
        }
    } else if (substate == 2) {
        if (lw(pv + 0xc8) == 0 && moby_hero_on_l1(m) != 0) {
            sh(0x0013f544u, 4);
            sh(0x0013f542u, 4);
        }

        bool blocked = (lh(pv + 0xba) != 0);
        if (!blocked && lwc1(pv + 0xbc) < 0.0f &&
            vec2_distance(V(m + 0x10), V(kHeroPos)) < 2.0f &&
            abs_f(fpu_sub(lwc1(0x0013f3d8u), lwc1(m + 0x18))) < 4.0f &&
            1.0f < fpu_sub(lwc1(m + 0x18), lwc1(0x0013f3d8u))) {
            if (lh(pv + 0xba) == 0) {
                sh(pv + 0xba, static_cast<uint16_t>(frame_scale_i(30)));
            }
            blocked = true;
        }

        if (blocked) {
            sw(pv + 0xa8, 0);
            stop_platform_sound(m, pv);
        } else {
            if (sub_0028d8c0(m, lw(pv + 0xc4)) == 0) {
                sw(pv + 0xc4, static_cast<uint32_t>(moby_sound_start(0, 4, m)));
            }
            const float dt = lwc1(0x0015ed6cu);
            const float accel = lwc1(pv + 0xac);
            const float vel = fpu_add(lwc1(pv + 0xa8), fpu_mul(accel, dt));
            swc1(pv + 0xa8, vel);
            if (abs_f(accel) < abs_f(vel)) {
                swc1(pv + 0xa8, accel);
            }
            const float t = fpu_add(lwc1(pv + 0xa4), lwc1(pv + 0xa8));
            swc1(pv + 0xa4, t);
            if (0.5f < abs_f(fpu_sub(t, 0.5f))) {
                if (0.0f < lwc1(pv + 0xac)) {
                    sb(m + 0xbc, 0);
                    swc1(pv + 0xa4, 1.0f);
                } else {
                    sb(m + 0xbc, 1);
                    swc1(pv + 0xa4, 0.0f);
                }
                sw(pv + 0xa8, 0);
                sh(pv + 0xba, static_cast<uint16_t>(frame_scale_i(15)));
                stop_platform_sound(m, pv);
            }

            const int32_t n_seg = lw(path) - 1;
            const int32_t seg = float_to_int(fpu_mul(int_to_float(n_seg), lwc1(pv + 0xa4)));
            const float frac = fpu_sub(fpu_mul(int_to_float(lw(path) - 1), lwc1(pv + 0xa4)), int_to_float(seg));
            Vec4 target{};
            if (seg == lw(path) - 1) {
                target = V(path + 0x10u + (static_cast<uint32_t>(seg) << 4));
            } else {
                const uint32_t p0 = path + 0x10u + (static_cast<uint32_t>(seg) << 4);
                const uint32_t p1 = path + 0x20u + (static_cast<uint32_t>(seg) << 4);
                Vec4 tmp{};
                vec3_sub(tmp, V(p1), V(p0));
                vec3_scale(tmp, tmp, frac);
                vec3_add(target, tmp, V(p0));
            }
            const float k = u2f(0x3ba3d70au);     // 0.005f
            const float damp = u2f(0x3e4ccccdu);  // 0.2f
            spring_to(m + 0x10, pv + 0xd0, target.x, k, damp, 0.0f);
            spring_to(m + 0x14, pv + 0xd4, target.y, k, damp, 0.0f);
            spring_to(m + 0x18, pv + 0xd8, target.z, k, damp, 0.0f);
            swc1(pv + 0xbc, fpu_sub(lwc1(m + 0x18), prev_pos.z));
        }
    }

    if (lbu(m + 0xbc) != 2) {
        platform_spring_to_end_l14(m);
    }

    const uint32_t delta_addr = ee_stack_alloc(0x10);
    vec3_sub(V(delta_addr), V(m + 0x10), prev_pos);
    attach_frame_set(pv + 0x60, delta_addr, m + 0x40, m + 0x40);
    ee_stack_free(0x10);

    if (moby_hero_on_l1(m) == 0 && 2.0f < vec2_distance(V(m + 0x10), V(kHeroPos)) && lh(pv + 0xba) == 0) {
        sh(pv + 0xb8, 1);
    }
}

}  // namespace rac1
