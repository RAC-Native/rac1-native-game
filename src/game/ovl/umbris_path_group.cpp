// Umbris's own moby classes: class 871, the leader of a group of mobys that move round a looped path one after
// another (spread along it at the start, each put back at its start when it reaches the leader's segment again), which
// can be shot down (its health +0x20; destroyed: the group told 3, an explosion) — hand-ported from Umbris's own overlay,
// with 0x30af40 (a member put back as new). Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are
// Umbris's (`orig: L7:`); the frame time 0x15ed6c is the base ELF's.
//
// The leader's pvars: whether it can be hit +0x94 and is shown +0x95, its hit state +0x20 (health), its colour pulse
// +0x60, its path +0x70 (-1: it does nothing), its group +0x74, the number of members +0x78 (cut to those there), their
// speed +0x7c and spacing +0x80, whether they are moved +0x84 (-1: no), its nearest point +0x90. A member's: the leader
// +0xa0, its move +0x60, where it is +0x80 (+0x70 its start), its segment +0x90 (+0x94 the next), its timer +0x98.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// A member put back as new: state 0, its class's flags +0x44 and size +0x24, full alpha, the grid's whole rectangle,
// its class's draw distance, the hero's colour, the transform; its hit state's health +0x0 its maximum (+0x4).
// orig: L7:0x0030af40
void umb_30af40_l7(uint32_t m) {
    sb(m + 0x20, 0);
    const uint32_t cls = lwu(m + 0x24);
    sb(m + 0xbc, 0);
    sh(m + 0x34, lhu(cls + 0x44));
    sh(m + 0x32, 0xff);
    swc1(m + 0x2c, lwc1(cls + 0x24));
    sb(m + 0x30, 0xff);
    sb(m + 0x31, 1);
    moby_grid_move(m, 0x80807f7fu);
    sb(m + 0x71, 0xff);
    sh(m + 0x36, 0x7f80);
    sb(m + 0x72, 0xff);
    sb(m + 0xa4, 0xff);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    moby_copy_hero_color(m);
    moby_update_transform(m);
    const uint32_t hs = moby_pvar_word0(m);
    if (hs == 0) return;
    swc1(hs + 0x0, static_cast<float>(lh(hs + 0x4)));
}

// Class 871, the group's leader (+0x20: 0 the members spread along the path, 1 moving them).
// orig: L7:0x0030b000
void umb_class871_update_l7(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    if (lbu(pv + 0x94) != 0) {
        // hit: the damage off its health; none left: the group told 3, the explosion, retired
        const uint32_t hs = pv + 0x20;
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00230000), 0);
        int32_t state = 0;
        const int32_t r = moby_hit_react_r(m, hit, hs, 0, state, nullptr, nullptr, 4);
        if (r == 1 || r == 2) sw(hs, 0);  // (the table L7:0x211620: 1, 2 -> L7:0x30b0a8, the rest 0x30b0ac)
        if (state >= 2) {
            const float dmg = lwc1(hit + 0x2c);
            const float health = lwc1(hs);
            if (health <= dmg) {
                sw(hs, 0);
                moby_group_set_state(lw(pv + 0x74), 3);
                const uint32_t sp = ee_stack_alloc(0x10);
                vec_clear(V(sp));
                explosion_effect(m, sp, 0, 10, 3, 0x10, -1, 1, 1, -1, 0, 0.0f, 0.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
                ee_stack_free(0x10);
                moby_retire(m);
                return;
            }
            swc1(hs, fpu_sub(health, dmg));
            sb(pv + 0x67, 0xfa);
            sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(60)));
            color_pulse_start(m, pv + 0x60);
        }
        sb(m + 0xa4, 0xff);
    }
    color_pulse_update(m, pv + 0x60);
    if (lw(pv + 0x70) == -1) return;
    const uint32_t path = lvd::path(lw(pv + 0x70));
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        // (the group's iterator and the walk's state are the original's stack: group_next and path_advance_v take them
        // by address)
        const uint32_t sp = ee_stack_alloc(0x20);
        const uint32_t at = sp + 0x0, it = sp + 0x10, seg = sp + 0x14, frac = sp + 0x18;
        sw(it, 0);
        sw(frac, 0);
        V(at) = V(path + 0x10);
        // each point's +0xc: its segment's length (the path closed)
        for (int32_t i = 0; i < lw(path); ++i) {
            const int32_t next = ee_rem(i + 1, lw(path));
            swc1(path + (static_cast<uint32_t>(i) << 4) + 0x1c,
                 vec3_distance(V(path + (static_cast<uint32_t>(i) << 4) + 0x10), V(path + (static_cast<uint32_t>(next) << 4) + 0x10)));
        }
        if (group_first(it, lw(pv + 0x74), 1, 0) != 0) {
            debug_printf_stub();  // (no group: L7:0x211550)
            ee_stack_free(0x20);
            return;
        }
        const int32_t near = path_nearest_l1(m + 0x10, path, 0.0f);
        sw(pv + 0x90, static_cast<uint32_t>(near));
        sw(seg, static_cast<uint32_t>(near));
        for (int32_t k = 0; k < lw(pv + 0x78); ++k) {
            const uint32_t mem = lwu(it);
            const uint32_t mp = lwu(mem + 0x78);
            sw(mp + 0xa0, m);
            V(mem + 0x10) = V(at);
            V(mp + 0x80) = V(at);
            V(mp + 0x70) = V(at);
            sw(mp + 0x90, lwu(seg));
            sw(mp + 0x94, lwu(seg) + 1);
            if (group_next(it, lwu(it), 1, 0) != 0) {
                debug_printf_stub();  // (fewer members than +0x78: L7:0x211580 / 0x2115d0)
                sw(pv + 0x78, static_cast<uint32_t>(k + 1));
                break;
            }
            int32_t s = lw(seg);
            float f = lwc1(frac);
            path_advance_v(path, V(at), &s, &f, 1, lwc1(pv + 0x80));
            sw(seg, static_cast<uint32_t>(s));
            swc1(frac, f);
        }
        ee_stack_free(0x20);
        if (lbu(pv + 0x94) == 0) {
            sw(m + 0x94, 0);
            sh(m + 0x34, lhu(m + 0x34) & 0xefff);
        }
        if (lbu(pv + 0x95) == 0) sh(m + 0x34, lhu(m + 0x34) | 1);
        sb(m + 0x20, 1);
    } else if (st == 1) {
        const uint32_t sp = ee_stack_alloc(0x10);
        const uint32_t it = sp;
        sw(it, 0);
        group_first(it, lw(pv + 0x74), 1, 0);
        for (int32_t k = 0; k < lw(pv + 0x78); ++k) {
            const uint32_t mem = lwu(it);
            const uint32_t mp = lwu(mem + 0x78);
            float left = vec3_distance(V(mp + 0x80), V(path + (lwu(mp + 0x90) << 4) + 0x10));
            int32_t seg = lw(mp + 0x90);
            path_advance_v(path, V(mp + 0x80), &seg, &left, 1, fpu_mul(lwc1(pv + 0x7c), lwc1(0x0015ed6c)));
            sw(mp + 0x90, static_cast<uint32_t>(seg));
            sw(mp + 0x94, lwu(mp + 0x90) + 1);
            // a member gone (none, retired 0xfe, dying 0xfd) is moved along (+0x84 not -1): its move, its place; at
            // the leader's segment again when its timer runs out, put back as new
            const uint32_t cur = lwu(it);
            const bool gone = cur == 0 || lbu(cur + 0x20) == 0xfe || lbu(cur + 0x20) == 0xfd;
            if (gone && lw(pv + 0x84) != -1) {
                vec3_sub(V(mp + 0x60), V(mp + 0x80), V(cur + 0x10));
                V(cur + 0x10) = V(mp + 0x80);
                if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(mp + 0x98))) != 0 && lw(mp + 0x90) == lw(pv + 0x90))
                    umb_30af40_l7(cur);
            }
            group_next(it, cur, 1, 0);
        }
        ee_stack_free(0x10);
    }
    floor_light_add(m, 1.5f);
}

}  // namespace rac1
