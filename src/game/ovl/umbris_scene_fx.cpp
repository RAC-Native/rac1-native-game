// Umbris's own moby classes: the arena ammo resupply spawner (class 1142) and the two cutscene smoke controllers (classes
// 1552 and 1789), and the level's own build of the scenes' extras (cutscene_level_extras) — hand-ported from Umbris's own
// overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses
// are Umbris's (`orig: L7:`); Umbris-only data is L7_ADDR, the base ELF's are bare (the ammo counts 0x13d428, the hero
// 0x13f3d0), the paths the level-data facade's.
#include <game/level/level_data.h>
#include <game/level/level_write.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/veldin_mobys.h>
#include <game/port_stubs.h>

namespace rac1 {

// Class 1142, the ammo resupply spawner along path +0x4 triggered when the hero enters volume +0x0. State 0 measures each
// segment's length into its point +0xc lane and sums the path's perimeter into +0x20. State 1 walks the level's weapon list
// 0x161ca0 (-1-terminated) and, for each weapon whose current ammo ratio falls in [+0x8/100, +0xc/100), drops pickups along
// the path more than 12 units away from moby +0x24, then retires into state 2.
// orig: L7:0x0031d2e0
void umb_class1142_update_l7(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st >= 2) return;
    const uint32_t pv = lwu(m + 0x78);

    if (st == 0) {
        bool missing = false;
        if (lw(pv + 0x0) == -1) {
            debug_printf_stub();
            missing = true;
        }
        if (lw(pv + 0x4) == -1) {
            debug_printf_stub();
            missing = true;
        }
        if (missing) {
            debug_printf_stub();
            moby_retire(m);
            return;
        }
        sw(pv + 0x20, 0);
        uint32_t path = lvd::path(lw(pv + 0x4));
        if (lw(path) > 0) {
            for (int32_t i = 0; i < lw(path); ++i) {
                path = lvd::path(lw(pv + 0x4));
                const int32_t n = lw(path);
                const int32_t next = ee_rem(i + 1, n);
                const uint32_t p0 = path + static_cast<uint32_t>(i << 4) + 0x10;
                const uint32_t p1 = path + static_cast<uint32_t>(next << 4) + 0x10;
                const float seg_len = vec3_distance(V(p0), V(p1));
                path = lvd::path(lw(pv + 0x4));
                lvd::set_path_f32(path + static_cast<uint32_t>(i << 4) + 0x1c, seg_len);
                path = lvd::path(lw(pv + 0x4));
                const float cur = lwc1(pv + 0x20);
                swc1(pv + 0x20, fpu_add(cur, lwc1(path + static_cast<uint32_t>(i << 4) + 0x1c)));
            }
        }
        sb(m + 0x20, 1);
        return;
    }

    if (point_in_box(0x0013f3d0u, lw(pv + 0x0)) == 0) return;

    const uint32_t sp = ee_stack_alloc(0x20);
    for (int32_t idx = 0; lw(L7_ADDR(0x00161ca0) + static_cast<uint32_t>(idx << 2)) != -1; ++idx) {
        const uint32_t item = lwu(L7_ADDR(0x00161ca0) + static_cast<uint32_t>(idx << 2));
        const float have_f = int_to_float(lw(0x0013d428u + (item << 2)));
        const uint32_t row = static_cast<uint32_t>(static_cast<int32_t>(L7_ADDR(0x001c4130)) + static_cast<int32_t>(item) * 0x18);
        const float max_f = int_to_float(static_cast<int32_t>(lhu(row + 0xe)));
        const float ratio = fpu_div(have_f, max_f);
        float step = rand_range_f(0.0f, lwc1(pv + 0x20));

        if (!(fpu_div(lwc1(pv + 0x8), 100.0f) <= ratio)) continue;
        if (!(ratio < fpu_div(lwc1(pv + 0xc), 100.0f))) continue;

        const float target_ammo = fpu_mul(max_f, fpu_div(lwc1(pv + 0x10), 100.0f));
        const float cap_ammo = fpu_mul(max_f, fpu_div(lwc1(pv + 0x14), 100.0f));
        const float need = fpu_sub(target_ammo, have_f);
        const bool pick_need = lw(pv + 0x18) != 0 ? (cap_ammo < need) : (need < cap_ammo);
        const float give_f = pick_need ? need : cap_ammo;

        int32_t total_give = float_to_int(fpu_add(give_f, 0.5f));
        int32_t per_crate = total_give;
        const int32_t q = ee_div(total_give, lw(pv + 0x1c));
        int32_t path_idx = 0;
        float path_frac = 0.0f;
        const uint32_t path = lvd::path(lw(pv + 0x4));
        if (q != 0) per_crate = q;
        debug_printf_stub();
        if (total_give <= 0) continue;

        int32_t rem_give = total_give - per_crate;
        for (;;) {
            for (;;) {
                path_advance_v(path, V(sp), &path_idx, &path_frac, 1, step);
                const int32_t avoid_i = lw(pv + 0x24);
                if (avoid_i == -1) break;
                const uint32_t avoid_m = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(avoid_i) << 8);
                if (12.0f < vec2_distance(V(avoid_m + 0x10), V(sp))) break;
                step = fpu_add(step, 10.0f);
            }
            total_give = rem_give;
            debug_printf_stub();
            pickup_spawn(sp, item, per_crate, 0);
            const float advance = rand_range_f(fpu_div(lwc1(pv + 0x20), 10.0f), 2.0f);
            const bool less = rem_give < per_crate;
            step = fpu_add(step, advance);
            if (less) {
                per_crate = rem_give;
                debug_printf_stub();
            }
            const bool more = rem_give > 0;
            rem_give = total_give - per_crate;
            if (!more) break;
        }
    }
    ee_stack_free(0x20);
    sb(m + 0x20, 2);
}

// Class 1552, cutscene ship / prop smoke emitter: while a scene plays (0x15f5c4 == 2), trails smoke on scene mobys in shots
// 3 / 4 and bursts particle 0x17 clouds on chain points 0..3 at frames 0xa8a (slot 7) and 0xace (slot 6) of scene 2.
// orig: L7:0x0031eba8
void umb_class1552_update_l7(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (lw(L7_ADDR(0x0015f5c4)) != 2) return;

    const uint32_t cs = L7_ADDR(0x0016c8e0);
    const uint32_t shot_off = lwu(L7_ADDR(0x0016c910)) - 3u;
    if (shot_off < 2u) {
        const uint32_t slot = st < shot_off ? 0u : 2u;
        scene_moby_smoke(lwu(cs + (slot << 2) + 0x178));
    }
    if (lw(cs + 0x30) != 2) return;

    uint32_t burst_slot = lwu(cs + 0x34) == static_cast<uint32_t>(frame_scale_i(0xa8a)) ? 7u : 0u;
    if (lwu(cs + 0x34) == static_cast<uint32_t>(frame_scale_i(0xace))) burst_slot = 6u;
    if (burst_slot == 0) return;

    const uint32_t target = lwu(cs + (burst_slot << 2) + 0x178);
    const uint32_t sp = ee_stack_alloc(0x10);
    for (uint32_t cp = 0; cp < 4; ++cp) {
        moby_chain_point_v(target, cp, V(sp));
        for (int k = 9; k >= 0; --k) {
            const float size = rand_range_f(20000.0f, 200000.0f);
            const uint32_t alpha = static_cast<uint32_t>(rand_range(0x20, 0x80));
            const uint32_t color = (alpha << 24) | 0x007f7f7fu;
            const int32_t mag = rand_range(0, 4);
            const int32_t spin = rand_mod(2) == 0 ? mag : -mag;
            const uint32_t p =
                spawn_particle_17(sp, spin, L7_ADDR(0x0015f580), color, u2f(0x3f19999au), 1.0f, u2f(0x3f8147aeu), size);
            if (p != 0) {
                const int32_t life = frame_scale_i(rand_range(0x3c, 0x5a));
                sh(p + 0xa, static_cast<uint32_t>(life));
                sw(p + 0x24, 2);
                sb(p + 0x2a, static_cast<uint32_t>(static_cast<int32_t>(color) >> 24));
                sb(p + 0x2b, lbu(p + 0xa));
            }
        }
    }
    ee_stack_free(0x10);
}

// Class 1789, cutscene exhaust smoke controller: in shots 1 and 2 of a playing scene (slots 3 and 4 of 0x16c8e0 + 0x178),
// emits three smoke bursts from chain points 0..2 along their axes from chain points 3..5.
// orig: L7:0x0031f900
void umb_class1789_update_l7(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (lw(L7_ADDR(0x0015f5c4)) != 2) return;

    const uint32_t cs = L7_ADDR(0x0016c8e0);
    const uint32_t shot = lwu(L7_ADDR(0x0016c910));
    if (shot - 1u >= 2u) return;
    const uint32_t slot = shot == 2u ? 4u : (shot == 1u ? 3u : 0u);
    const uint32_t target = lwu(cs + (slot << 2) + 0x178);

    const uint32_t sp = ee_stack_alloc(0x90);
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t pt_a = sp + k * 0x10;
        const uint32_t pt_b = sp + 0x30 + k * 0x10;
        const uint32_t dir = sp + 0x60 + k * 0x10;
        moby_chain_point_v(target, k, V(pt_a));
        moby_chain_point_v(target, k + 3, V(pt_b));
        vec3_sub(V(dir), V(pt_a), V(pt_b));
        vec3_set_length(V(dir), V(dir), lwc1(L7_ADDR(0x00161cd8)));
        smoke_burst(pt_a, pt_a, dir, lwc1(L7_ADDR(0x00161cdc)), lwc1(L7_ADDR(0x00161ce0)));
    }
    ee_stack_free(0x90);
}

// The scenes' level extras, Umbris's own build of the engine's cutscene_level_extras (cutscene.cpp dispatches here on level
// 7): in the level's scene 4 (the scene record 0x16c8e0's +0x30), when its fourth moby (+0x178's fourth word, +0x188) is a
// class 0x214, that moby's draw callback for this frame (the level's 0x2ba260, as class 529's, umbris_director.cpp).
// orig: L7:0x00265bd8
void umb_cutscene_level_extras_l7() {
    if (lw(0x0015ed84) != 7) return;  // (the level's number)
    const uint32_t scene = L7_ADDR(0x0016c8e0);
    if (lw(scene + 0x30) != 4) return;
    const uint32_t m = lwu(scene + 0x188);
    if (m == 0 || lh(m + 0xa6) != 0x214) return;
    sub_001f93e0(L7_ADDR(0x002ba260), m);
}

}  // namespace rac1
