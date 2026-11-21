// Hoven's (level 12) classes 1269 (homing seeker drone released by class 1281) and 1281 (the floor hatch that opens and
// releases up to 12 drones from `pv + 0xc0`, with helpers 0x3076e0 and 0x307ab8). Level 12 only: `orig: L12:`, collision
// hits through coll_hit_*, Hoven-only data L12_ADDR, base ELF bare. Registered in hoven_mobys.cpp.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
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
}  // namespace

// Class 1269's update: rises out of its parent hatch (`m + 0xb8`), curves toward its target (`target_select_any`, steering
// offset `pv + 0x80` within 7 units), hovers above the floor, and explodes on impact or weapon hit (mask 0x230000).
// orig: L12:0x00304e00
void hov_class1269_update_l12(uint32_t m) {
    const Frame f(320);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78);
    target_select_any(m, sp + 0x20, 8.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000u), 0);
    {
        int32_t react_st = lw(sp + 0xd0);
        moby_hit_react_r(m, hit, pv + 0x20, 0, react_st, nullptr, nullptr, 4);
        sw(sp + 0xd0, static_cast<uint32_t>(react_st));
    }

    const bool near_target = vec2_distance(V(m + 0x10), V(sp + 0x20)) < 0.25f;
    const int32_t react_st = lw(sp + 0xd0);
    bool killed = near_target;
    if (!killed && react_st >= 2 && hit != 0) {
        const uint32_t att = lwu(hit + 0x20);
        if (lh(att + 0xa6) != 0x4f5) killed = true;
    }
    if (killed) {
        sw(sp + 0x8, 0xffffffffu);
        sw(sp + 0x0, 0);
        sw(sp + 0x10, 0);
        explosion_effect(m, L12_ADDR(0x0015f580), 0, 3, 3, 5, 2, 0, 0, -1, 0, 0.0f, 0.0f, 1.0f, 0.5f, 4.0f, u2f(0x3f333333u), 7.0f);
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
        return;
    }

    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        lq_sq(pv + 0x60, m + 0x10);
        float side_yaw = u2f(0x3f32b8c2u);
        sb(m + 0x20, 1);
        swc1(pv + 0x80, side_yaw);
        const uint32_t parent = lwu(m + 0xb8);
        if (parent != 0 && lbu(parent + 0xbc) != 0) swc1(pv + 0x80, -side_yaw);
        sw(pv + 0x84, 0);
    } else if (st == 1) {
        sw(pv + 0x84, lwu(pv + 0x84) + 1u);
        if (lw(pv + 0x84) < frame_scale_i(0x1e)) {
            const int32_t rise_len = frame_scale_i(0x1e);
            const float num = fpu_mul(static_cast<float>(lw(pv + 0x84)), u2f(0x40490fdbu));
            const float arc = sin_vu(fpu_div(num, static_cast<float>(rise_len)));
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(arc, u2f(0x3d8f5c29u))));
        }
        if (frame_scale_i(0xf) < lw(pv + 0x84)) {
            swc1(pv + 0x70, fpu_add(lwc1(pv + 0x70), fpu_mul(lwc1(0x0015ed70u), 30.0f)));
        }
        if (frame_scale_i(0x1e) < lw(pv + 0x84)) {
            if (lbu(m + 0x53) == 0) moby_anim_set(m, 1, 0, 2);
            if (lbu(m + 0x53) == 1 && (lbu(m + 0x70) & 2u) != 0) moby_anim_set(m, 2, 0, 2);
        }
        const float max_spd = fpu_mul(lwc1(0x0015ed6cu), 5.75f);
        if (max_spd < lwc1(pv + 0x70)) swc1(pv + 0x70, max_spd);

        const float direct_yaw = atan2_f(fpu_sub(lwc1(sp + 0x20), lwc1(m + 0x10)), fpu_sub(lwc1(sp + 0x24), lwc1(m + 0x14)));
        float move_yaw = direct_yaw;
        if (vec2_distance(V(m + 0x10), V(sp + 0x20)) < 7.0f) {
            move_yaw = angle_add(direct_yaw, lwc1(pv + 0x80));
        }
        swc1(sp + 0x70, fpu_mul(cos_vu(move_yaw), lwc1(pv + 0x70)));
        const float sy = fpu_mul(sin_vu(move_yaw), lwc1(pv + 0x70));
        sw(sp + 0x78, 0);
        swc1(sp + 0x74, sy);
        vec3_add(V(m + 0x10), V(m + 0x10), V(sp + 0x70));

        if (frame_scale_i(0x1e) < lw(pv + 0x84)) {
            swc1(pv + 0x78, angle_add(lwc1(pv + 0x78), fpu_mul(lwc1(0x0015ed6cu), u2f(0x40a78d36u))));
            swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x74)));
            const float bob = fpu_mul(sin_vu(lwc1(pv + 0x78)), u2f(0x3dcccccdu));
            swc1(pv + 0x74, bob);
            swc1(m + 0x18, fpu_add(lwc1(m + 0x18), bob));
        }
        if (frame_scale_i(0x1e) < lw(pv + 0x84)) {
            float flr = floor_probe_v(0.5f, V(m + 0x10), 0);
            const float hero_flr = lwc1(0x0013f628u);
            if (flr < hero_flr) flr = hero_flr;
            const float diff = fpu_sub(flr, lwc1(m + 0x18));
            if (diff < 0.5f && -3.0f < diff) {
                const float k = lwc1(0x0015ed64u), dt = lwc1(0x0015ed6cu);
                spring_to(m + 0x18, pv + 0x7c, fpu_add(flr, u2f(0x3f147ae1u)), fpu_mul(k, u2f(0x3ca3d70au)),
                          fpu_mul(k, u2f(0x3e99999au)), fpu_mul(dt, 4.0f));
            }
        }
        if (frame_scale_i(0x1e) < lw(pv + 0x84)) {
            const float kPush = u2f(0x3e99999au);
            swc1(sp + 0xb0, fpu_mul(cos_vu(direct_yaw), kPush));
            swc1(sp + 0xb4, fpu_mul(sin_vu(direct_yaw), kPush));
            swc1(sp + 0xbc, u2f(0x45afdf66u));
            swc1(sp + 0xb8, u2f(0x3f99999au));
            sub_00259888(1.0f, sp + 0x80, static_cast<int32_t>(m), 0x10001, sp + 0xb0);
            lq_sq(sp + 0xc0, m + 0x10);
            const float r_moby = u2f(0x3ecccccdu);
            swc1(sp + 0xc8, fpu_add(lwc1(sp + 0xc8), r_moby));
            bool impact = false;
            if (mobys_in_sphere(sp + 0xc0, r_moby, 0x10, m, sp + 0x80) != 0) {
                const uint32_t hit_m = coll_hit_moby();
                if (hit_m != 0 && lh(hit_m + 0xa6) != 0x4f5 && hit_m != lwu(m + 0xb8)) impact = true;
            }
            if (!impact) {
                lq_sq(sp + 0xc0, m + 0x10);
                const float r_world = u2f(0x3eb33333u);
                swc1(sp + 0xc8, fpu_add(lwc1(sp + 0xc8), r_world));
                if (sphere_collide(sp + 0xc0, r_world, 0, m) != 0) impact = true;
            }
            if (impact) sb(m + 0x20, 2);
        }
    } else if (st == 2) {
        sw(sp + 0x0, 9);
        sw(sp + 0x8, 0xffffffffu);
        sw(sp + 0x10, 0);
        explosion_effect(m, L12_ADDR(0x0015f580), 0, 5, 0xf, 0x19, 2, 0, 9, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 4.0f, 1.0f, 7.0f);
        moby_retire(m);
        return;
    }

    floor_light_add(m, 1.0f);
}

// Activates the next drone slot (`pv + 0xc0 + 4 * *(pv + 0xf0)`) at the hatch's position, bursts 14 type-0x1c particles,
// and resets the drone's health word from its initial halfword.
// orig: L12:0x003076e0
void drone_hatch_spawn(uint32_t m) {
    const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t slot = lwu(pv + 0xc0 + (lwu(pv + 0xf0) << 2));
    const uint32_t child = first + (slot << 8);
    lq_sq(child + 0x10, m + 0x10);
    sb(child + 0x20, 0);
    sb(child + 0xbc, 0);
    const uint32_t cls = lwu(child + 0x24);
    swc1(child + 0x18, fpu_sub(lwc1(child + 0x18), u2f(0x3eb33333u)));
    sh(child + 0x34, lhu(cls + 0x44));
    sb(child + 0x30, 0xff);
    swc1(child + 0x2c, lwc1(cls + 0x24));
    sb(child + 0x31, 1);
    sh(child + 0x36, 0x7f80);
    sb(child + 0xa4, 0xff);
    sh(child + 0x32, 0xff);
    sb(child + 0x71, 0xff);
    sb(child + 0x72, 0xff);
    sw(child + 0x94, lwu(cls + 0x10));
    sd(child + 0x38, ld(m + 0x38));
    moby_update_transform(child);
    sw(child + 0xb8, m);

    float dt = lwc1(0x0015ed6cu);
    for (int32_t i = 0; i < 14; ++i) {
        const float spd = rand_range_f(fpu_mul(dt, 3.0f), fpu_mul(dt, 5.0f));
        spawn_particle_1c(m + 0x10, 0, spd);
        dt = lwc1(0x0015ed6cu);
    }

    const uint32_t w0 = moby_pvar_word0(child);
    if (w0 != 0) {
        swc1(w0, static_cast<float>(static_cast<int32_t>(lh(w0 + 4))));
    }
}

// Updates class 1281's detection range (`pv + 0xf8`, boosted by +5 while alert timer `pv + 0xfe` runs) and picks a target
// into `pv + 0x70` / `pv + 0xb0` (`pv + 0xb4 = 2` when the target is more than 3 units above or below).
// orig: L12:0x00307ab8
void drone_hatch_target(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) return;
    if (lwu(pv + 0x38) != 0) {
        sh(pv + 0xfe, static_cast<uint32_t>(frame_scale_i(0xb4)));
    }
    sw(pv + 0x38, 0);

    const int32_t expired = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xfe)));
    const float base_r = lwc1(pv + 0xf4);
    swc1(pv + 0xf8, expired != 0 ? base_r : fpu_add(base_r, 5.0f));

    if (target_select_any(m, pv + 0x70, lwc1(pv + 0xf8)) != 2) {
        if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x78)))) {
            sw(pv + 0xb4, 2);
        }
    }
    if (lwu(pv + 0xb0) == 0) {
        sw(pv + 0xb0, lwu(0x001413d0u));
        lq_sq(pv + 0x70, 0x0013f3d0u);
    }
}

// Class 1281's update (jump table L12:0x20ca20, states 0..5): 0 retires pre-placed drones in `pv + 0xc0..0xec` -> 1, 1 waits
// for the target in range -> 2, 2 plays open animation 1 -> 2 and enters 3, 3 spawns drones every 40 scaled frames until the
// list ends -> 4, 4 plays close animation 3 -> 0 and enters dormant state 5.
// orig: L12:0x00307840
void hov_class1281_update_l12(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    drone_hatch_target(m);
    const uint32_t st = lbu(m + 0x20);
    if (st >= 6) return;
    const uint32_t slots = pv + 0xc0;

    switch (st) {  // (jump table L12:0x0020ca20)
    case 0: {      // (0x00307898)
        for (uint32_t k = 0; k < 12; ++k) {
            const int32_t idx = lw(slots + k * 4);
            const uint32_t first = lwu(L12_ADDR(0x0015ffd8));
            if (idx == -1) continue;
            const uint32_t dm = first + (static_cast<uint32_t>(idx) << 8);
            if (dm == 0) continue;
            const uint32_t dst = lbu(dm + 0x20);
            if (dst == 0xfe || dst == 0xfd) continue;
            moby_retire(dm);
        }
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x003078fc)
        if (lw(pv + 0xb4) == 2) break;
        if (vec2_distance(V(m + 0x10), V(pv + 0x70)) < lwc1(pv + 0xf8)) {
            sb(m + 0x20, 2);
        }
        break;
    case 2:  // (0x00307930)
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(2));
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, frame_scale_i(2));
            sb(m + 0x20, 3);
        }
        break;
    case 3:  // (0x0030799c)
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xfc))) != 0) {
            const int32_t cur = lw(pv + 0xf0);
            if (lw(slots + static_cast<uint32_t>(cur << 2)) != -1) {
                drone_hatch_spawn(m);
                sb(m + 0xbc, lbu(m + 0xbc) < 1 ? 1u : 0u);
                sh(pv + 0xfc, static_cast<uint32_t>(frame_scale_i(0x28)));
            }
            sw(pv + 0xf0, lwu(pv + 0xf0) + 1u);
        }
        {
            const int32_t next = lw(pv + 0xf0);
            if (next == 12 || lw(slots + static_cast<uint32_t>(next << 2)) == -1) {
                sb(m + 0x20, 4);
            }
        }
        break;
    case 4:  // (0x00307a1c)
    {
        const uint32_t cur_anim = lbu(m + 0x53);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        if (cur_anim != 3) moby_anim_set(m, 3, 0, frame_scale_i(2));
        if ((lbu(m + 0x70) & 2u) != 0) {
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(2));
            sb(m + 0x20, 5);
        }
        break;
    }
    case 5:  // (0x00307a90)
    default:
        break;
    }
}

}  // namespace rac1
