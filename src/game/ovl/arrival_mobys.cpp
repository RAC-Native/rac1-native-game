// The mobys of the ship sequences (game mode 6): the ship's thrusters, Clank and Ratchet riding
// it, the shadow probe of the scene's mobys, the scripted camera, Ratchet put back on the ground
// at the end.
#include <game/ovl/arrival.h>
#include <game/enhance/fixes102.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x0016c860u> SC{};  // the scene
constexpr uint32_t G = 0x0013e030;   // the ship sequence
constexpr uint32_t P = 0x0013f350;   // the hero
}  // namespace

// Six exhaust particles of the ship `ship`: at the ship's nozzle offsets 0x1bdc60[variant * 0x60
// + i * 0x10] through its matrix, a random spread of *(0x15ed6c) sideways and a quarter of it
// about `z` up, colours 0x24c0c0c0 / 0x14c0c0c0, 4 frames (plus a random 0..4).
// orig: 0x002903c8
void ship_thrusters(uint32_t ship, float z) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t s5 = sp + 0x10, s6 = sp + 0x20, s3 = sp + 0x30;
    for (int32_t i = 0; i < 6;) {
        float f13 = lwc1(0x0015ed6c);
        swc1(s5, rand_range_f(-f13, f13));
        f13 = lwc1(0x0015ed6c);
        swc1(s5 + 4, rand_range_f(-f13, f13));
        const float f1 = lwc1(0x0015ed6c);
        const float f0 = rand_range_f(f1 * -0.25f, f1 * 0.25f);
        swc1(s5 + 0xc, 0.4f);
        swc1(s5 + 8, z + f0);
        lq_sq(s6, s5);
        swc1(s6 + 0xc, 0.6f);
        lq_sq(s3, OV(0x001bdc60) + static_cast<uint32_t>(lh(G + 0x26) * 0x60) + static_cast<uint32_t>(i) * 0x10);
        ++i;
        mat3_transform(V(s3), V(s3), M(ship + 0xc0));
        vec3_add(V(s3), V(s3), V(ship + 0x10));
        const int32_t s2 = frame_scale_i(4), s1 = frame_scale_i(4), s0 = frame_scale_i(4);
        const int32_t r = rand_mod(frame_scale_i(4));
        sw(sp, 0xffffffffu);
        spawn_particle_02(s3, s5, s6, 0x24c0c0c0u, 0x14c0c0c0u, static_cast<uint32_t>(s2), static_cast<uint32_t>(s1), static_cast<uint32_t>(s0 + r), -1);
    }
    ee_stack_free(0x40);
}

// Clank (class 10, or 0x1a3 / 0x555) in a ship sequence: always drawn (+0x34 | 0x10), his
// colour pulsing with sin(2 pi t / 100 frames) (+0x90), the glow of his eyes (0x208038) at his
// joint 6 (class 0x1a3, when more than 2 off his height) or 1, his anim node (0x17c380 /
// 0x17c3c0 / 0x17c400) set up once when 0x15edb3.
// orig: 0x00207430
void clank_ship_update(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x10);
    sh(m + 0x34, lhu(m + 0x34) | 0x10u);
    const int32_t s0 = frame_scale_i(0x64);
    const float f0 = int_to_float(s0);
    const int32_t v1 = ee_rem(lw(OV(0x0015f5cc)), s0);
    float f12 = static_cast<float>(v1);
    f12 = fpu_div(f12, f0);
    f12 = f12 + f12;
    f12 = f12 * u2f(0x40490fdbu);
    f12 = f12 + u2f(0xc0490fdbu);
    const float f21 = sin_vu(f12);
    const float f20 = f21 * 24.0f;
    const int32_t s1 = float_to_int(f20);
    const int32_t t = float_to_int(f21 * 52.0f);
    const int32_t v0 = float_to_int(f20);
    sw(m + 0x90, (static_cast<uint32_t>(v0 + 0x4c) << 16) | 0x80000000u | (static_cast<uint32_t>(t + 0xa2) << 8) | static_cast<uint32_t>(s1 + 0x44));
    if (lh(m + 0xa6) == 0x1a3) {
        moby_chain_point(m, 6, sp);
        const float z = lwc1(sp + 8) - 0.07f;
        const float d = lwc1(m + 0x18) - z;
        swc1(sp + 8, z);
        if (2.0f < abs_f(d)) sub_00208038(sp, 0x200000c0u, 0.25f, u2f(0x3e19999au));
    } else {
        moby_chain_point(m, 1, sp);
        swc1(sp + 8, lwc1(sp + 8) - u2f(0x3be56042u));
        sub_00208038(sp, 0x200000c0u, u2f(0x3d408312u), u2f(0x3e19999au));
    }
    const int16_t cls = lh(m + 0xa6);
    uint32_t node = OV(0x0017c380);
    if (cls == 0x1a3) node = OV(0x0017c3c0);
    else if (cls == 0x555) node = OV(0x0017c400);
    if (lbu(0x0015edb3) != 0 && lbu(node + 1) == 0) {
        float s;
        if (static_cast<int16_t>(lhu(m + 0xa6)) == 0x1a3) {
            sub_0024f590(m, 1, node);
            s = lwc1(0x0015ee18);
        } else {
            sub_0024f590(m, 2, node);
            s = lwc1(0x0015ee18);
        }
        swc1(node + 0x28, s);
        swc1(node + 0x20, s);
        swc1(node + 0x24, s);
    }
    ee_stack_free(0x10);
}

// The ground under a moby of a sequence (+0x7f set): two segments down from its bounding sphere
// (+0..+0xc, offset back along the sun's direction 0x1aeb80) against the collision (0x22); the
// floor heights to +0x84 (lower, -1/4) and +0x88 (higher, +1/4, at most +0x84 + 4); none: both 0.
// orig: 0x0025a1e0
void moby_shadow_probe(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t s1 = sp + 0x10, s2 = sp + 0x20, s3 = sp + 0x30;
    lq_sq(sp, OV(0x001aeb80));
    vec2_set_length(V(sp), V(sp), 1.0f);
    vec3_scale(V(s1), V(m), 0.0009765625f);
    lq_sq(s2, s1);
    const float f4 = lwc1(m + 0xc);
    const float f20 = 0.000732421875f;
    float f0 = lwc1(sp) * f4;
    float f1 = lwc1(sp + 4) * f4;
    float f3 = lwc1(s2);
    float f2 = lwc1(s2 + 4);
    f0 = f0 * f20;
    f1 = f1 * f20;
    f3 = f3 - f0;
    f2 = f2 - f1;
    swc1(s2, f3);
    swc1(s2 + 4, f2);
    lq_sq(s3, s2);
    swc1(s3 + 8, lwc1(s3 + 8) - 8.0f);
    if (segment_collide(s2, s3, 0x22, 0, 0) == 0) {
        sw(m + 0x88, 0);
        sw(m + 0x84, 0);
        ee_stack_free(0x40);
        return;
    }
    lq_sq(s2, s1);
    const float f5 = lwc1(m + 0xc);
    const float f21 = lwc1(OV(0x00173e68));
    float g3 = lwc1(s2 + 8);
    float g2 = lwc1(sp) * f5;
    float g1 = lwc1(sp + 4) * f5;
    g3 = g3 + f5 * u2f(0x3a000000u);
    float f12 = -lwc1(sp + 8);
    float g4 = lwc1(s2);
    float g0 = lwc1(s2 + 4);
    const float g5 = g3 - f21;
    swc1(s2 + 8, g3);
    g1 = g1 * f20;
    float f22 = f21;
    g2 = g2 * f20;
    f12 = fpu_div(g5, f12);
    g0 = g0 + g1;
    g4 = g4 + g2;
    swc1(s2 + 4, g0);
    swc1(s2, g4);
    vec3_set_length(V(sp), V(sp), f12);
    vec3_add(V(s3), V(s2), V(sp));
    if (segment_collide(s2, s3, 0x22, 0, 0) != 0) f22 = lwc1(OV(0x00173e68));
    swc1(m + 0x84, min_f(f21, f22) - 0.25f);
    const float hi = max_f(f21, f22) + 0.25f;
    const float cap = lwc1(m + 0x84) + 4.0f;
    swc1(m + 0x88, hi);
    if (cap < hi) swc1(m + 0x88, cap);
    ee_stack_free(0x40);
}

// The hero's camera target: the point `pos` (0x140990, its w = t at 0x14099c), the flag
// 0x1409a0; the hero's state 0x65 unless in 0x65..0x67.
// orig: 0x00233ee8
void hero_cam_target_set(uint32_t pos, float t, uint32_t flag) {
    lq_sq(P + 0x1640, pos);
    const int32_t st = lw(P + 0x2084);
    swc1(P + 0x164c, t);
    sw(P + 0x1650, flag);
    if (!(static_cast<uint32_t>(st - 0x65) < 3)) hero_set_state(0x65, 1);
}

// After the landing (0x13e05c set): the floor 3/4 of the hero's velocity back from his position
// (probed from half a unit up); more than 0.3 off his height: the hero's camera target there.
// orig: 0x00234d40
void hero_ship_floor_fix() {
    const uint32_t sp = ee_stack_alloc(0x10);
    vec3_scale(V(sp), V(P), -0.75f);
    vec3_add(V(sp), V(sp), V(P + 0x80));
    swc1(sp + 8, lwc1(sp + 8) + 0.5f);
    const float h = floor_probe(0.5f, sp, 0);
    swc1(sp + 8, h);
    if (!(u2f(0x3e99999au) < abs_f(lwc1(P + 0x88) - h))) {
        hero_cam_target_set(P + 0x80, lwc1(P + 0x98), 1);
        lq_sq(P + 0x80, sp);
    }
    ee_stack_free(0x10);
}

// In game modes 2 / 6: the scene's classes 0x509 / 0x50a always drawn (+0x34 | 0x800, every slot
// read, empty or not), the hero's chains (0x209240), his held mobys (0x20f580, 0x234808), the
// held items 0x140520 / 0x140524 / 0x140570 shown (+0x34 & ~1).
// orig: 0x00234a50
void hero_ship_parts(uint32_t m) {
    const int32_t mode = lw(OV(0x0015f5c4));
    if (mode != 2 && mode != 6) return;
    for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
        const uint32_t a1 = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
        if (static_cast<uint32_t>(lhu(a1 + 0xa6) - 0x509u) < 2) sh(a1 + 0x34, lhu(a1 + 0x34) | 0x800u);
    }
    hero_chain_world(m);
    hero_slot_mobys_update();
    sub_00234808();
    for (uint32_t off : {0x11d0u, 0x11d4u, 0x1220u}) {
        const uint32_t h = lwu(P + off);
        if (h != 0) sh(h + 0x34, lhu(h + 0x34) & 0xfffeu);
    }
}

// The scene's hero (class 0): his scale *(0x15ee14) (at most 1.33 in game mode 6) on his anim
// node 0x17c340 (set up once), the backpack classes 0x1b1 / 0x50a / 0x509 of the scene scaled
// with it on levels 2, 9, 11 and 13; then 0x234a50.
// orig: 0x00234b38
void hero_ship_update(uint32_t m) {
    float f20 = lwc1(0x0015ee14);
    if (lw(OV(0x0015f5c4)) == 6 && u2f(0x3faa3d71u) < f20) f20 = u2f(0x3faa3d71u);
    constexpr OvConst<0x0017c340u> node{};
    if (lbu(node + 1) == 0) {
        sub_0024f590(m, 0x18, node);
        swc1(node + 0x28, f20);
        swc1(node + 0x20, f20);
        swc1(node + 0x24, f20);
    }
    const int32_t lv = lw(0x0015ed84);
    if (lv == 2 || lv == 9 || lv == 0xb || lv == 0xd) {
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t a0 = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            if (a0 == 0) continue;
            const int16_t cls = lh(a0 + 0xa6);
            if (cls == 0x1b1 || cls == 0x50a || cls == 0x509) swc1(a0 + 0x2c, lwc1(lwu(a0 + 0x24) + 0x24) * f20);
        }
    }
    if (fixes102_on(7)) {
        // (1.02 Fixes, fix 7: the 1.02 build, 0x2353b8, pulses the scene's Ratchet as hero_gadget_glow pulses him in
        // play, slower; on Veldin, the scene's frames 835 to 1080 (+0x34, scene +0x30 0) faster, between the second pair)
        const uint32_t PL = P;
        float ph = angle_add(lwc1(PL + 0x1628), lwc1(0x0015ed6c) * u2f(0x40060a92u));
        swc1(PL + 0x1628, ph);
        uint32_t c0 = 0x806e8c6eu, c1 = 0x805a3232u;
        if (lw(0x0015ed84) == 0 && lw(SC + 0x30) == 0 && !(lw(SC + 0x34) < frame_scale_i(0x343)) &&
            !(frame_scale_i(0x438) < lw(SC + 0x34))) {
            ph = angle_add(ph, lwc1(0x0015ed6c) * u2f(0x40bde44eu));
            swc1(PL + 0x1628, ph);
            c0 = 0x806eaafau;
            c1 = 0x80326e5au;
        }
        const float t = sin_vu(lwc1(PL + 0x1628)) * 0.5f + 0.5f;
        sw(m + 0x90, rgba_lerp(c0, c1, t));
    }
    hero_ship_parts(m);
}

// The backpack `pack` on the scene's hero `m`: at the world matrix of his chain 4, its pose
// encoded from the list 0x17a608 (class 0x1b1, 6) or 0x15f6a8 into 0x17c680.
// orig: 0x00234c78
void hero_backpack_attach(uint32_t m, uint32_t pack) {
    const uint32_t sp = ee_stack_alloc(0x40);
    moby_chain_world_one(m, 4, sp);
    lq_sq(pack + 0x10, sp + 0x30);
    mat3_copy(M(pack + 0xc0), M(sp));
    moby_update_transform_kept(pack);
    if (lh(pack + 0xa6) == 0x1b1) moby_pose_encode(OV(0x0017a608), OV(0x0017c680), lwu(pack + 0x24), 6, m);
    else moby_pose_encode(OV(0x0015f6a8), OV(0x0017c680), lwu(pack + 0x24), 0, m);
    sw(pack + 0x50, 0);
    sw(pack + 0x6c, OV(0x0017c680));
    sw(pack + 0x68, OV(0x0017c680));
    sw(pack + 0x54, 0);
    ee_stack_free(0x40);
}

// The scripted camera (the level camera of id 5) made the active one at `pos` / `rot` (also the
// view's 0x166dc0), kind `kind` (+0x88), t0 at its data +0x120; kind 2: a3 at +0xe4 / +0xe0;
// kind 3: the old view kept as its start, aimed at the hero (0x1ed630), then as kind 2.
// orig: 0x002eaaa0
void camera_scripted_set(uint32_t pos, uint32_t rot, int32_t kind, uint32_t a3, uint32_t t0) {
    const uint32_t sp = ee_stack_alloc(0x40);
    const uint32_t s0 = moby_table_find_by_uid(5);
    lq_sq(sp, pos);
    lq_sq(sp + 0x10, rot);
    lq_sq(sp + 0x20, OV(0x00166dc0));
    lq_sq(sp + 0x30, OV(0x00166dd0));
    camera_switch(s0);
    const uint32_t s3 = lwu(s0 + 0x70);
    const uint32_t s5 = s3 + 0xe0;
    sb(s0 + 0x7c, 7);
    sb(s0 + 0x7d, 1);
    lq_sq(s0 + 0x30, sp);
    lq_sq(OV(0x00166dc0), sp);
    lq_sq(s0 + 0x40, sp + 0x10);
    lq_sq(s3 + 0x80, sp);
    lq_sq(s3 + 0x90, sp + 0x10);
    sw(s5 + 0x40, t0);
    sb(s0 + 0x88, static_cast<uint32_t>(kind));
    if (kind == 3) {
        sw(s5 + 0x48, 0);
        swc1(s5 + 0x44, 1.0f);
        lq_sq(s0 + 0x30, sp + 0x20);
        lq_sq(s0 + 0x40, sp + 0x30);
        lq_sq(s3 + 0x110, 0x0013f3d0);
        sub_001ed630(s3 + 0xfc, sp + 0x20, 0x0013f3d0);
    } else if (kind != 2) {
        ee_stack_free(0x40);
        return;
    }
    sw(s5 + 4, a3);
    sw(s3 + 0xe0, a3);
    ee_stack_free(0x40);
}

// The hero's held moby cleared (0x1414e0 / 0x1414e4).
// orig: 0x002118b0
void hero_hold_clear() {
    sw(P + 0x194, 0);
    sw(P + 0x190, 0);
}

// The hero moby's animation: sequence `seq`, frames `frame` / `frame` + 1, blend 0.
// (1.02 Fixes, fix 6: the 1.02 build, 0x232df8, keeps the next frame +0x51 within the sequence: at most its frame
// count, the class's sequence `seq` (+0x24, +0x48 + seq * 4) +0x10, less 1)
// orig: 0x002325a0
void hero_anim_set(uint32_t seq, uint32_t frame) {
    const uint32_t v1 = lwu(P + 0x2080);
    sb(v1 + 0x51, frame + 1);
    if (fixes102_on(6)) {
        const int32_t last = static_cast<int32_t>(lbu(lwu(lwu(v1 + 0x24) + seq * 4 + 0x48) + 0x10)) - 1;
        if (last < static_cast<int32_t>((frame + 1) & 0xffu)) sb(v1 + 0x51, static_cast<uint32_t>(last));
    }
    sb(v1 + 0x53, seq);
    sb(v1 + 0x50, frame);
    sb(v1 + 0x52, seq);
    sw(v1 + 0x54, 0);
    sub_0024ebc0(v1);
}

// The nearest of the level's light zones (0x160040, 0x30 each, *(0x160014) of them) within 8 of
// the moby: its colour to +0x80 / +0x38 (0x2502d0, then the low word +0x1c), its fog (+0x27:
// +0x20 / +0x24..+0x26 to 0x13e5b4.. with 0x13e5bb | 7), its music track (+0x28) to 0x151708
// when no track is forced (0x15170e) and 0x1516f0's low 24 bits are 0xff0000.
// orig: 0x002500b8
void moby_light_zone_pick(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x10);
    int32_t best = -1;
    float d_best = u2f(0x497423f0u);
    for (int32_t i = 0; i < lw(OV(0x00160014)); ++i) {
        vec3_sub(V(sp), V(m + 0x10), V(lwu(OV(0x00160040)) + static_cast<uint32_t>(i) * 0x30));
        const float d = vec3_length(V(sp));
        if (d <= 8.0f && d < d_best) {
            d_best = d;
            best = i;
        }
    }
    if (best == -1) {
        debug_printf_stub();
        ee_stack_free(0x10);
        return;
    }
    const uint32_t s0 = static_cast<uint32_t>(best) * 0x30;
    uint32_t e = lwu(OV(0x00160040)) + s0;
    const uint32_t c = (lwu(e + 0x18) << 16) + (lwu(e + 0x14) << 8) + lwu(e + 0x10);
    sw(m + 0x80, c);
    moby_set_color_packed(m, static_cast<int32_t>(c), 0, 0, 0);
    sw(m + 0x38, lwu(lwu(OV(0x00160040)) + s0 + 0x1c));
    e = lwu(OV(0x00160040)) + s0;
    if (lbu(e + 0x27) != 0) {
        constexpr uint32_t F = 0x0013e550;
        sw(F + 0x64, lwu(e + 0x20));
        const uint32_t fl = lbu(F + 0x6b) | 7u;
        sb(F + 0x68, lbu(e + 0x24));
        sb(F + 0x69, lbu(e + 0x25));
        sb(F + 0x6b, fl);
        sb(F + 0x6a, lbu(e + 0x26));
    }
    e = lwu(OV(0x00160040)) + s0;
    if (lw(e + 0x28) >= 0) {
        constexpr uint32_t S = 0x001516d0;
        if (lh(S + 0x3e) == 0 && (lwu(S + 0x20) & 0xffffffu) == 0xff0000u) sh(S + 0x38, lhu(e + 0x28));
    }
    ee_stack_free(0x10);
}

// The hero put at `pos` turned by `rot` (his position 0x13f3d0 and his moby's, his matrices),
// what he held dropped, his motion cleared, his state `state` (-1: kept), his moby's animation
// restarted, 0x167014 = (his mode 0x1413dc == 0x11), his ground, the camera of type 0 when
// `cam`, his light zone.
// orig: 0x00216f90
void hero_place(uint32_t pos, uint32_t rot, int32_t state, int32_t cam) {
    constexpr uint32_t S1 = P + 0x80;
    lq_sq(S1, pos);
    lq_sq(lwu(P + 0x2080) + 0x10, pos);
    lq_sq(S1 + 0x10, rot);
    euler_to_mat4_vu(M(P), V(S1 + 0x10));
    euler_to_mat3_vu(M(lwu(P + 0x2080) + 0xc0), V(S1 + 0x10));
    hero_hold_clear();
    retire_children(S1 + 0x15f0);
    const uint32_t f = lwu(P + 0x364) & 0xfffffffeu & 0xfffffffdu;
    sh(P + 0x30e, 1);
    sw(P + 0x364, f);
    sw(P + 0x360, 0);
    vec_clear(V(S1 + 0x70));
    vec_clear(V(S1 + 0x60));
    vec_clear(V(S1 + 0x80));
    vec_clear(V(S1 + 0x90));
    vec_clear(V(S1 + 0xa0));
    vec_clear(V(S1 + 0xd0));
    vec_clear(V(S1 + 0x20));
    sw(P + 0x184, 0);
    sb(P + 0x22ca, 0);
    if (gameplay102_on(13)) sh(P + 0x22de, 0);  // (1.02 Gameplay, fix 13: the 1.02 build, 0x217718, stops the Hologuise's countdown)
    sub_00266d48();
    hero_hold_release();
    sub_00205538();
    if (state != -1) hero_set_state(static_cast<uint32_t>(state), 1);
    const uint32_t hm = lwu(P + 0x2080);
    hero_anim_set(lbu(hm + 0x53), lbu(hm + 0x51));
    sw(OV(0x00167014), lw(P + 0x208c) == 0x11 ? 1u : 0u);
    hero_ground_update();
    if (cam != 0) camera_select_first();
    moby_light_zone_pick(lwu(0x001413d0));
}

}  // namespace rac1
