// Quartu (level 15) class 221 (L15:0x002c2938), ported by hand from the level's own code:
// a spring-jointed factory plant/emitter that coordinates with its moby group, turns to a random
// angle, plays an animation cycle while emitting particles (type 0x15), and shatters into three
// debris pieces + plant bolts when hit.
// The original's order of calls and float operations is preserved.
#include <game/ovl/quartu_mobys.h>

#include <game/level/level_data.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// Spring-jointed factory emitter / plant (class 221).
// orig: L15:0x002c2938
void qua_class221_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);

    if (lbu(m + 0x20) != 6) {
        const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) {
            sb(m + 0x20, 6);
        }
    }

    if (lbu(m + 0x20) != 0) {
        const float scale = lwc1(0x0015ed64u);
        swc1(pv + 0x68, lwc1(pv + 0x84));
        joint_node_spring(m, pv, 0, fpu_mul(scale, u2f(0x3cf5c28fu)), fpu_mul(scale, u2f(0x3e99999au)));
    }

    const uint8_t state = lbu(m + 0x20);
    if (state >= 7) return;

    auto reset_wait_timer = [pv]() {
        const float lo = frame_scale_f(fpu_mul(lwc1(L15_ADDR(0x00161a00u)), 60.0f));
        const float hi = frame_scale_f(fpu_mul(lwc1(L15_ADDR(0x00161a04u)), 60.0f));
        sh(pv + 0x80, static_cast<uint16_t>(float_to_int(rand_range_f(lo, hi))));
    };

    // Jump table L15:0x001e2d70 (7 entries)
    switch (state) {
    case 0: {  // (0x002c2a20)
        reset_wait_timer();
        sw(pv + 0x88, 0);
        sw(pv + 0x84, 0);
        sh(pv + 0x82, 0);
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        break;
    }
    case 1: {  // (0x002c2a7c)
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x80))) == 0) return;
        if (!(vec3_distance(V(L15_ADDR(0x001673c0u)), V(m + 0x10)) < 42.0f)) return;

        const uint8_t grp = lbu(m + 0x21);
        bool ready = true;
        if (grp != 0xff) {
            const uint32_t list = lvd::group(static_cast<int32_t>(grp));
            if (list == 0) return;
            const uint32_t pool = lwu(L15_ADDR(0x0015ffd8u));
            const int16_t self_cls = lh(m + 0xa6);
            int active = 0;
            for (uint32_t p = list;; p += 2) {
                const uint16_t entry = lhu(p);
                const uint32_t mate = pool + (static_cast<uint32_t>(entry & 0x7fffu) << 8);
                const uint8_t mst = lbu(mate + 0x20);
                if (static_cast<int8_t>(mst) >= 0 && lh(mate + 0xa6) == self_cls && mst != 1) {
                    ++active;
                }
                if (static_cast<int16_t>(entry) < 0) break;
            }
            if (active != 0) {
                sh(pv + 0x80, static_cast<uint16_t>(frame_scale_i(0x3c)));
                ready = false;
            }
        }
        if (ready) {
            swc1(pv + 0x8c, rand_range_f(u2f(0xbe567750u), u2f(0x3e567750u)));
            sb(m + 0x20, 2);
        }
        break;
    }
    case 2: {  // (0x002c2b88)
        sh(pv + 0x82, static_cast<uint16_t>(rand_range(0, 1)));
        if (angle_dist(lwc1(pv + 0x84), lwc1(pv + 0x8c)) < u2f(0x3c0efa35u)) {
            const uint32_t anim = static_cast<uint32_t>((static_cast<int32_t>(lh(pv + 0x82)) << 1) + 1);
            moby_anim_set(m, anim, 0, frame_scale_i(6));
            sb(m + 0x20, 3);
        }
        const float dt2 = lwc1(0x0015ed70u);
        const float dt1 = lwc1(0x0015ed6cu);
        const float acc = fpu_mul(dt2, u2f(0x3fc90fdbu));
        const float dec = fpu_mul(dt2, u2f(0x3f490fdbu));
        const float vmax = fpu_mul(dt1, u2f(0x3f490fdbu));
        swc1(pv + 0x84, angle_seek_rate(pv + 0x88, lwc1(pv + 0x84), lwc1(pv + 0x8c), acc, dec, vmax));
        break;
    }
    case 3: {  // (0x002c2c3c)
        const uint32_t expected = static_cast<uint32_t>((static_cast<int32_t>(lh(pv + 0x82)) << 1) + 1);
        if (lbu(m + 0x53) == expected && (lbu(m + 0x70) & 2) != 0) {
            sh(pv + 0x80, static_cast<uint16_t>(frame_scale_i(rand_range(0xf, 0x2d))));
            const uint32_t anim = static_cast<uint32_t>((static_cast<int32_t>(lh(pv + 0x82)) << 1) + 2);
            moby_anim_set(m, anim, 0, frame_scale_i(3));
            swc1(pv + 0x90, rand_angle());
            sb(m + 0x20, 4);
        }
        break;
    }
    case 4: {  // (0x002c2cb8)
        if (vec3_distance(V(m + 0x10), V(L15_ADDR(0x001673c0u))) < 20.0f &&
            rand_mod(2) == 0 && lbu(m + 0x31) != 0) {
            const float spd_max = lwc1(L15_ADDR(0x001619fcu));
            const float s1 = fpu_mul(rand_range_f(fpu_div(spd_max, 5.0f), spd_max), lwc1(0x0015ed6cu));
            const float s2 = fpu_mul(rand_range_f(0.0f, fpu_div(lwc1(L15_ADDR(0x001619fcu)), 5.0f)), lwc1(0x0015ed6cu));
            const float rand_ang = rand_angle();
            Vec4 vel{};
            vel.x = fpu_mul(cos_vu(lwc1(pv + 0x90)), s1);
            vel.y = fpu_mul(sin_vu(lwc1(pv + 0x90)), s1);
            vel.z = 0.0f;
            Vec4 jitter{};
            jitter.x = fpu_mul(cos_vu(rand_ang), s2);
            jitter.y = fpu_mul(sin_vu(rand_ang), s2);
            jitter.z = 0.0f;
            vec3_add_inplace(vel, jitter);
            Vec4 pos{};
            moby_chain_point_v(m, 1, pos);
            const int32_t life_lo = frame_scale_i(lw(L15_ADDR(0x001619f4u)));
            const int32_t life_hi = frame_scale_i(lw(L15_ADDR(0x001619f8u)));
            const uint32_t life = static_cast<uint32_t>(rand_range(life_lo, life_hi));
            const uint32_t col = lwu(L15_ADDR(0x001619ecu));
            spawn_particle_15_v(pos, vel, col, col, life, 1, 40000.0f);
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x80))) != 0) {
            const uint32_t anim = static_cast<uint32_t>((static_cast<int32_t>(lh(pv + 0x82)) << 1) + 3);
            moby_anim_set(m, anim, 0, frame_scale_i(6));
            sb(m + 0x20, 5);
        }
        break;
    }
    case 5: {  // (0x002c2e40)
        const uint32_t expected = static_cast<uint32_t>((static_cast<int32_t>(lh(pv + 0x82)) << 1) + 3);
        if (lbu(m + 0x53) == expected && (lbu(m + 0x70) & 2) != 0) {
            reset_wait_timer();
            moby_anim_set(m, 0, 0, frame_scale_i(6));
            sb(m + 0x20, 1);
        }
        break;
    }
    case 6: {  // (0x002c2ec8)
        moby_sound_start(0, 0, m);
        explosion_burst(m, m + 0x10, -1, 1.5f, 13.0f);
        plant_bolts_drop_l1(m);
        const uint32_t zero_vec = L15_ADDR(0x0015f580u);
        debris_spawn(m, 0x74b, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        debris_spawn(m, 0x74c, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        debris_spawn(m, 0x74d, m + 0x10, m + 0x40, 0, 0, zero_vec, zero_vec, zero_vec, 0.0f);
        moby_retire(m);
        break;
    }
    }
}

}  // namespace rac1
