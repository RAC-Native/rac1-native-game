// Aridia's own moby classes: the Sand Shark's nest, which sends its group's sharks leaping out (hand-ported from Aridia's
// overlay, `orig: L2:`). Aridia-only data is L2_ADDR; the base ELF's (the difficulty tables 0x14d590 / 0x14ee90, the frame
// time 0x15ed6c) are bare. Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }

// the pvars of the moby at index `i` of the moby table
uint32_t moby_pvars_at(int32_t i) { return lwu(lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8) + 0x78); }

// the nest's lair (pvar +0xe0, -1 none) counts the nests out of it at +0x150
void lair_nests_add(uint32_t pv, int32_t d) {
    const int32_t i = lw(pv + 0xe0);
    if (i < 0) return;
    const uint32_t lpv = moby_pvars_at(i);
    sw(lpv + 0x150, static_cast<uint32_t>(lw(lpv + 0x150) + d));
}

// h -= sub as halfwords; at most 0 left: 1
void count_down_by(uint32_t h, uint32_t sub) {
    const uint32_t d = (lhu(h) - sub) & 0xffffu;
    sh(h, d);
    if (!(static_cast<int16_t>(d) > 0)) sh(h, 1);
}
}  // namespace

// A shark leaps out of the nest: state 0x1b (its spray, then the leap), shown and solid (+0x34 bits 0 and 6 cleared),
// the animation set 0xb, at `pos` with the velocity `vel` (pvar +0x40; EE vectors), turned along it.
// orig: L2:0x002d6118
void nest_shark_launch_l2(uint32_t shark, uint32_t pos, uint32_t vel) {
    const uint32_t pv = lwu(shark + 0x78);
    sb(shark + 0x20, 0x1b);
    sh(shark + 0x34, lhu(shark + 0x34) & 0xffbeu);
    if (lbu(shark + 0x53) != 0xb) moby_anim_set(shark, 0xb, 0, 1);
    lq_sq(shark + 0x10, pos);
    lq_sq(pv + 0x40, vel);
    swc1(shark + 0x48, atan2_f(lwc1(vel), lwc1(vel + 4)));
    moby_update_transform(shark);
}

// The nest's hits (mask 0x330000): out of its lair when told (+0x2e 2: gone); unless stunned (the hit's state 1) or dying
// (4), the health +0x20 down (a hit of state 3 empties it) and a pulse (+0x60): knocked back (the move record +0x70, set
// 3; at a damage of 1 or more state 3, the animation set 2), or at 0 thrown (state 4, the animation set 3) with its bolts;
// then its pulse and the countdown +0xbc.
// orig: L2:0x002dcfe0
void nest_hits_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(pv + 0x2e) == 2) {
        sb(pv + 0x2e, 1);
        lair_nests_add(pv, -1);
        moby_retire(m);
        return;
    }
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t dir = sp, hit_state = sp + 0x10, hit_dmg = sp + 0x14, yaw = sp + 0x18, yaw2 = sp + 0x1c;
    swc1(hit_dmg, 0.0f);
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    {
        int32_t st = lw(hit_state);
        float dmg = lwc1(hit_dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_dmg, dmg);
    }
    if (hit != 0 && lw(hit_state) != 1 && lbu(m + 0x20) != 4) {
        if (lw(hit_state) == 3) swc1(pv + 0x20, 0.0f);
        else swc1(pv + 0x20, fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg)));
        sw(pv + 0x94, 4);
        if (0.0f < lwc1(pv + 0x20)) {
            swc1(pv + 0x88, fpu_mul(dt(), 4.0f));
            swc1(pv + 0x8c, fpu_mul(dt(), 3.0f));
            lq_sq(dir, hit + 0x10);
            vec_yaw_marked_v(V(dir), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, 3, 1, 0, lwc1(yaw));
            sb(pv + 0x67, 0x78);
            if (1.0f <= lwc1(hit_dmg)) {
                sb(m + 0x20, 3);
                moby_anim_set(m, 2, 0, 0);
            }
        } else {
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0xfa);
            sb(m + 0x20, 4);
            swc1(pv + 0x8c, fpu_mul(dt(), 3.0f));
            swc1(pv + 0x88, fpu_mul(dt(), 11.0f));
            lq_sq(dir, hit + 0x10);
            vec_yaw_marked_v(V(dir), *static_cast<float*>(ee_ram_ptr(yaw2)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, 3, 1, 0, lwc1(yaw2));
            swc1(pv + 0xc0, 9.0f);
            swc1(pv + 0xc4, 19.0f);
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, 0);
            moby_drop_bolts(m, 0, -1);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    countdown_u8(m + 0xbc);
    ee_stack_free(0x70);
}

// A shark of the nest's group (+0x21; 0xff none) that is in its lair (class 0x244 in state 0x1a): told the nest is its
// lair (+0xb8), its count +0xb4 at most the nest's, and the nest's count lowered by it (at least 1). The shark (0: none).
// orig: L2:0x002dd2a8
uint32_t nest_find_shark_l2(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return 0;
    for (uint32_t e = lvd::group(static_cast<int32_t>(group));; e += 2) {
        const uint32_t s = lwu(OV(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(s + 0xa6) == 0x244 && lbu(s + 0x20) == 0x1a) {
            sw(s + 0xb8, m);
            if (lh(m + 0xb4) < lh(s + 0xb4)) sh(s + 0xb4, lhu(m + 0xb4));
            count_down_by(m + 0xb4, lhu(s + 0xb4));
            return s;
        }
        if (lh(e) < 0) return 0;
    }
}

// The Sand Shark's nest (pvars: its lair +0xe0, the wait +0xd4 / timer +0xd8, the move record +0x70): state 0 sets up
// (its count +0xb4 less the difficulty's 0x14d790 row, 3 health, the wait), 1 waits, then opens (2) when a shark of its
// group is in its lair; 2 at the animation's frame 12 sends one out (nest_shark_launch_l2: 1.5 over the nest, toward the
// shark's home at 2 a frame, rising at 6), and closes (1) when the animation has played out; 3 knocked back (back to 1);
// 4 thrown: on the ground, its blast, four debris and gone.
// orig: L2:0x002dcb38
void nest_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    nest_hits_l2(m);
    if (static_cast<int8_t>(lbu(m + 0x20)) < 0) return;
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L2_ADDR(0x001673c0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    switch (lbu(m + 0x20)) {  // (the original's jump table at L2:0x1faa10)
    case 0: {  // (0x002dcbd8)
        count_down_by(m + 0xb4, lhu(0x0014d590u + (lbu(m + 0xb1) << 2) + 0x202));
        sb(m + 0x20, 1);
        swc1(pv + 0x20, 3.0f);
        sh(pv + 0x24, 3);
        sb(pv + 0x2e, 1);
        sw(pv + 0xd8, static_cast<uint32_t>(frame_scale_i(lw(pv + 0xd4))));
        lair_nests_add(pv, 1);
        return;
    }
    case 1:  // (0x002dcc64)
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xd8))) == 0) return;
        if (nest_find_shark_l2(m) == 0) {
            sw(pv + 0xd8, static_cast<uint32_t>(frame_scale_i(lw(pv + 0xd4))));
            return;
        }
        sb(m + 0x20, 2);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        return;
    case 2: {  // (0x002dccbc)
        if ((lbu(m + 0x70) & 2) != 0) {
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
            return;
        }
        if (!(moby_anim_time(m) == 12.0f)) return;
        int32_t k = lw(0x0014ee90u + (lbu(m + 0xb0) << 2));
        if (!(k < 21)) k = 0x14;
        const int32_t extra = frame_scale_i(k * 15);
        sw(pv + 0xd8, static_cast<uint32_t>(frame_scale_i(lw(pv + 0xd4) + extra)));
        const uint32_t shark = nest_find_shark_l2(m);
        if (shark == 0) return;
        if (lw(pv + 0xe0) >= 0) {  // (the lair's count of sharks out, +0x14c)
            const uint32_t lpv = moby_pvars_at(lw(pv + 0xe0));
            sw(lpv + 0x14c, lwu(lpv + 0x14c) + 1);
        }
        const uint32_t spv = lwu(shark + 0x78);
        const uint32_t sp = ee_stack_alloc(0xa0);
        const uint32_t vel = sp + 0x20, at = sp + 0x30;
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 1.5f));
        vec3_sub(V(vel), V(spv + 0x220), V(m + 0x10));
        vec2_set_length(V(vel), V(vel), fpu_add(dt(), dt()));
        swc1(vel + 8, fpu_mul(dt(), 6.0f));
        nest_shark_launch_l2(shark, at, vel);
        ee_stack_free(0xa0);
        return;
    }
    case 3:  // (0x002dcdf8)
        enemy_physics(m, pv + 0x70);
        if ((lbu(m + 0x70) & 2) == 0) return;
        sb(m + 0x20, 1);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
        return;
    case 4: {  // (0x002dce48)
        if ((enemy_physics(m, pv + 0x70) & 0x40) == 0) return;
        lair_nests_add(pv, -1);
        const uint32_t sp = ee_stack_alloc(0xa0);
        const uint32_t at = sp + 0x20;
        lq_sq(at, m + 0x10);
        explosion_effect(m, pv + 0x40, at, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
        const uint32_t z = L2_ADDR(0x0015f580);  // (a zero vector)
        for (const uint32_t cls : {0x6e4u, 0x6e4u, 0x6e5u, 0x6e5u}) debris_spawn(m, cls, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
        ee_stack_free(0xa0);
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}

}  // namespace rac1
