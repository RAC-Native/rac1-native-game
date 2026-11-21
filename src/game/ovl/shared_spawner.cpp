// Classes 455 (the Spawner) and 545 (the pods it throws) with their helpers: one port of the copies Kerwan (L3), Batalia
// (L8) and Oltanis (L14) carry. The copies are the same code relinked (their bodies compared with the relocated fields
// masked): every address they name is the engine's (OV: the moby table 0x15ffd8, the moby list 0x15ffe4, the camera
// 0x166dc0, the boxes 0x1600ec, the collision result 0x173e40.., the zero vector 0x15f580) or the base ELF's (bare: the
// hero 0x13f3d0, the frame scales 0x15ed60 / 0x15ed70); the levels' own words are only the jump tables, which the port
// replaces by switches on the index. Header shared_spawner.h.
//
// The Spawner's pvars: its health +0x20, the colour pulse +0x60, the explosion's record +0x70, the trigger mode +0x82,
// the spawn flags +0x83 (8: a random spread), its next state when the hero comes +0x84, the spawn pattern +0x85, the
// live count +0x86 / the most +0x87, the spawn timer +0x88 / its interval +0x8a, the pods' life +0x8c, the inner radius
// +0x90, the outer radius +0x9c, four boxes +0xa0, the trigger box +0xd0, the pods' gravity +0xdc and speed +0xe4, the
// moby indices of its mobys +0xe8 (18; -1 none). A pod's pvars: its velocity +0, its Spawner +0x10, its timer +0x14,
// the moby it brought +0x18, the flags +0x1c, the sound timer +0x20.
#include <game/ovl/shared_spawner.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/metal.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position
constexpr uint32_t POD_CLASS = 0x221;      // class 545
constexpr uint32_t LIST_LEN = 18;          // the Spawner's moby indices (+0xe8)
const float kPi = u2f(0x40490fdbu);

float frame_scale() { return lwc1(0x0015ed60); }
float frame_time_sq() { return lwc1(0x0015ed70); }

bool gone(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    return st == 0xfe || st == 0xfd;
}

// the moby of a Spawner's list entry (a moby index)
uint32_t indexed_moby(int32_t idx) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }

void count_one(uint32_t pv) { sb(pv + 0x86, lbu(pv + 0x86) + 1); }

// +0x86 grows by every live pod on the level's moby list (*0x15ffe4, linked by +0x28)
void count_live_pods(uint32_t pv) {
    for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28))
        if (lh(o + 0xa6) == static_cast<int32_t>(POD_CLASS) && !gone(o)) count_one(pv);
}
}  // namespace

// orig: L3:0x002beba0 L8:0x002e6168 L14:0x002e1758
uint32_t class455_box_point(int32_t box, uint32_t out) {
    if (box == -1) return 1;
    const uint32_t b = lwu(OV(0x001600ec)) + (static_cast<uint32_t>(box) << 7);
    swc1(out, rand_range_f(-1.0f, 1.0f));
    swc1(out + 4, rand_range_f(-1.0f, 1.0f));
    swc1(out + 8, rand_range_f(-1.0f, 1.0f));
    mat3_transform(V(out), V(out), M(b));
    vec3_add(V(out), V(out), V(b + 0x30));
    return 0;
}

// The pod: its Spawner, position and velocity, a random tilt, its life +0x16 and timer +0x14 (500 frames), the Spawner's
// draw words (+0x30 / +0x32), a thousandth of its class's scale (it grows in flight).
// orig: L3:0x002c45d8 L8:0x002ee5b0 L14:0x002e7198
uint32_t class455_pod_spawn(uint32_t owner, uint32_t pos, uint32_t vel, int32_t life, uint32_t flags) {
    const uint32_t c = moby_spawn(POD_CLASS);
    if (c == 0) return 0;
    const uint32_t pv = lwu(c + 0x78);
    sw(pv + 0x10, owner);
    lq_sq(c + 0x10, pos);
    lq_sq(pv, vel);
    swc1(c + 0x40, rand_range_f(-kPi, kPi));
    swc1(c + 0x44, rand_range_f(-kPi, kPi));
    sw(pv + 0x18, 0);
    sh(pv + 0x14, static_cast<uint32_t>(frame_scale_i(0x1f4)));
    sh(pv + 0x16, static_cast<uint32_t>(life));
    sb(c + 0x30, lbu(lwu(pv + 0x10) + 0x30));
    sb(c + 0x31, 1);
    sh(c + 0x32, lhu(lwu(pv + 0x10) + 0x32));
    sb(c + 0x20, 0);
    swc1(c + 0x2c, fpu_mul(lwc1(lwu(c + 0x24) + 0x24), u2f(0x3a83126fu)));
    sw(pv + 0x1c, flags);
    sw(pv + 0x20, 0);
    moby_update_transform(c);
    return c;
}

// The Spawner. Near the camera (28 units) it keeps its floor band. It counts its live mobys and pods (+0x86), takes hits
// (mask 0xc30000; its health +0x20, at 0 it drops its bolts and goes to state 8), then by its state: 0 retires its mobys
// and waits (state 1); 1 waits for the hero within the outer radius, then goes to the state its +0x84 names; 2..7 spawn;
// 8 explodes into three pieces and leaves; anything else back to 1. While spawning, when triggered (mode +0x82: 0 the
// hero within the outer radius, 1 in the trigger box, else either the box or the inner radius), its timer +0x88 just ran
// out and it has room for more (with no interval +0x8a only while none lives), it throws pods: one per interval, or as
// many as it has room for. Each pod aims by the pattern +0x85: 0 around the Spawner at evenly spread angles, 1 toward the
// hero, 2 the first free of its four boxes, 3 a random box, 4 the boxes in turn (5: the last aim again).
// orig: L3:0x002bef68 L8:0x002e6530 L14:0x002e1b20
void class455_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(OV(0x00166dc0))) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    if (pv == 0) return;
    const uint32_t sp = ee_stack_alloc(0x160);
    // (the aim's vectors live in the frame, as the original's: a pattern that finds no box aims at what they held before)
    const uint32_t taken = sp + 0x20, at = sp + 0x30, step = sp + 0x40, from = sp + 0x50, spot = sp + 0x60, cand = sp + 0x70;
    const uint32_t hit_state = sp + 0x80;

    sb(pv + 0x86, 0);
    for (uint32_t i = 0; i < LIST_LEN; ++i) {
        const int32_t idx = lw(pv + 0xe8 + i * 4);
        if (idx == -1) continue;
        const uint32_t o = indexed_moby(idx);
        if (o != 0 && !gone(o)) count_one(pv);
    }
    count_live_pods(pv);

    const float hero_dist = vec2_distance(V(pos), V(HERO_POS));
    const bool within_outer = hero_dist < lwc1(pv + 0x9c);
    const bool within_inner = hero_dist < lwc1(pv + 0x90);
    const bool timer_out = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x88))) != 0;
    bool room = lbu(pv + 0x86) < lbu(pv + 0x87);
    bool in_box = false;
    if (lw(pv + 0xd0) != -1) in_box = point_in_box(HERO_POS, lw(pv + 0xd0)) != 0;
    bool triggered;
    switch (lbu(pv + 0x82)) {
    case 0: triggered = within_outer; break;
    case 1: triggered = in_box; break;
    default: triggered = in_box || within_inner; break;
    }
    if (lh(pv + 0x8a) == 0 && lbu(pv + 0x86) != 0) room = false;

    // ---- the hit
    const uint32_t hit = sub_0025a420(m, 0xc30000u, 0);
    {
        int32_t st = lw(hit_state);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, nullptr, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
    }
    if (lw(hit_state) >= 2) {
        const float health = fpu_sub(lwc1(pv + 0x20), lwc1(hit + 0x2c));
        swc1(pv + 0x20, health);
        if (0.0f < health) {
            sb(pv + 0x67, 0x5a);
            color_pulse_start(m, pv + 0x60);
        } else {
            moby_drop_bolts(m, 0, -1);
            sb(m + 0x20, 8);
            sb(pv + 0x67, 0x78);
            color_pulse_start(m, pv + 0x60);
        }
    }

    // ---- the state (the jump table L8:0x1fc070 (L3:0x1e2fe0, L14:0x1fc700), 10 entries, by its index)
    sb(m + 0xa4, 0xff);
    bool spawning = false;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002e6840) its mobys retired; then waiting
        for (uint32_t i = 0; i < LIST_LEN; ++i) {
            const int32_t idx = lw(pv + 0xe8 + i * 4);
            if (idx == -1) continue;
            const uint32_t o = indexed_moby(idx);
            if (o != 0 && !gone(o)) moby_retire(o);
        }
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sb(pv + 0x5a, 3);
        sb(pv + 0x58, 8);
        sb(pv + 0x29, 0);
        sb(m + 0x20, 1);
        break;
    }
    case 1:  // (0x002e68d8) waiting for the hero
        if (within_outer) {
            uint32_t next = 1;
            switch (lbu(pv + 0x84)) {  // (the jump table L8:0x1fc0a0 (L3:0x1e3010, L14:0x1fc730), 7 entries, by its index)
            case 0: next = 2; break;  // (0x002e690c)
            case 1: next = 3; break;  // (0x002e691c)
            case 2: next = 7; break;  // (0x002e692c)
            case 3: next = 2; break;  // (0x002e693c)
            case 4: next = 5; break;  // (0x002e694c)
            case 5: next = 5; break;  // (0x002e6964)
            default: next = 1; break; // (0x002e6974; also past the table)
            }
            sb(m + 0x20, next);
        }
        break;
    case 2: case 3: case 4: case 5: case 6: case 7:  // (0x002e69ac / 0x002e69bc / 0x002e698c / 0x002e699c)
        spawning = true;
        break;
    case 8: {  // (0x002e69cc) destroyed
        const uint32_t none = OV(0x0015f580);  // (a zero vector)
        explosion_effect(m, pv + 0x70, 0, 0x1e, 8, 0x14, 1, 1, 1, -1, 0, 0.0f, 0.0f, 8.0f, 4.0f, 18.0f, 1.0f, 15.0f);
        for (const uint32_t piece : {0x6c5u, 0x6c6u, 0x6c7u}) debris_spawn(m, piece, pos, m + 0x40, 0, 0, none, none, none, 0.0f);
        moby_retire(m);
        ee_stack_free(0x160);
        return;
    }
    default:  // (0x002e6adc: 9, and past the table)
        sb(m + 0x20, 1);
        break;
    }

    // ---- the spawns
    if (spawning && triggered && timer_out && room) {
        libc_memset(taken, 0, 0x10);
        const bool spread = (lbu(pv + 0x83) & 8) != 0;
        // the step from the Spawner to the aimed spot, its length spread at random (from its length to 3)
        auto aim_at_spot = [&] {
            vec3_sub(V(step), V(spot), V(at));
            const float len = vec3_length(V(step));
            if (spread) vec3_set_length(V(step), V(step), rand_range_f(len, 3.0f));
            lq_sq(from, spot);
        };
        // the step at least 3 long, at most the inner radius; the spot it reaches dropped to the floor below
        auto land_step = [&] {
            vec3_clamp_length_vu(V(step), V(step), lwc1(pv + 0x90));
            vec3_add(V(from), V(at), V(step));
            const float floor = floor_probe_v(0.5f, V(from), 0);
            if (floor != 0.0f) swc1(from + 8, floor);
        };
        // the boxes from the n-th one that is set (+0xa0, four), the first whose point can be made
        auto box_from = [&](int32_t n) {
            for (uint32_t i = 0; i < 4; ++i) {
                const int32_t box = lw(pv + 0xa0 + i * 4);
                if (box != -1) --n;
                if (n == 0 && class455_box_point(box, spot) == 0) break;
            }
        };
        auto boxes_set = [&] {
            int32_t n = 0;
            for (uint32_t i = 0; i < 4; ++i)
                if (lw(pv + 0xa0 + i * 4) != -1) ++n;
            return n;
        };
        while (lh(pv + 0x8a) != 0 ? lh(pv + 0x88) == 0 : lbu(pv + 0x86) < lbu(pv + 0x87)) {
            moby_chain_point_v(m, 0, V(at));
            switch (lbu(pv + 0x85)) {  // (the jump table L8:0x1fc0c0 (L3:0x1e3030, L14:0x1fc750), 6 entries, by its index)
            case 0: {  // (0x002e6b6c) around the Spawner: the count's share of a turn, half the hero's distance away
                const float count = int_to_float(static_cast<int32_t>(lbu(pv + 0x86)));
                const float slots = int_to_float(static_cast<int32_t>(lbu(pv + 0x87) + 1));
                const float angle = angle_wrap_mod(fpu_sub(fpu_div(fpu_mul(fpu_add(count, count), kPi), slots), kPi));
                float radius = fpu_mul(vec2_distance(V(pos), V(HERO_POS)), 0.5f);
                if (spread) radius = rand_range_f(radius, 3.0f);
                if (radius < 3.0f) radius = 3.0f;
                swc1(step, fpu_mul(cos_vu(angle), radius));
                swc1(step + 4, fpu_mul(sin_vu(angle), radius));
                sw(step + 8, 0);
                land_step();
                break;
            }
            case 1: {  // (0x002e6c54) halfway toward the hero
                vec3_sub(V(step), V(HERO_POS), V(at));
                sw(step + 8, 0);
                vec3_scale(V(step), V(step), 0.5f);
                if (spread) vec3_set_length(V(step), V(step), rand_range_f(vec3_length(V(step)), 3.0f));
                if (vec3_length(V(step)) < 3.0f) vec3_set_length(V(step), V(step), 3.0f);
                land_step();
                break;
            }
            case 2: {  // (0x002e6d4c) the first box not taken in this frame whose point can be made
                float best = 100000.0f;
                int32_t pick = -1;
                for (uint32_t i = 0; i < 4; ++i) {
                    if (lw(taken + i * 4) == 1) continue;
                    if (class455_box_point(lw(pv + 0xa0 + i * 4), cand) != 0) continue;
                    const float d = vec2_distance(V(pos), V(HERO_POS));  // (the Spawner's distance: the first box wins)
                    if (!(d < best)) continue;
                    lq_sq(spot, cand);
                    best = d;
                    pick = static_cast<int32_t>(i);
                }
                if (pick != -1) sw(taken + static_cast<uint32_t>(pick) * 4, 1);
                aim_at_spot();
                break;
            }
            case 3: {  // (0x002e6e00) a random box
                const int32_t n = boxes_set();
                box_from(ee_rem(libc_rand(), n) + 1);
                aim_at_spot();
                break;
            }
            case 4: {  // (0x002e6e9c) the boxes in turn
                const int32_t n = boxes_set();
                box_from(ee_rem(static_cast<int32_t>(lbu(pv + 0x86)), n) + 1);
                aim_at_spot();
                break;
            }
            default:  // (0x002e6f84: 5, and past the table) the last aim
                break;
            }
            // the pod's speed along the step, its velocity to reach the spot under its gravity
            const float speed = fpu_mul(vec3_length(V(step)), fpu_mul(lwc1(pv + 0xe4), frame_scale()));
            sw(step + 8, 0);
            vec3_set_length(V(step), V(step), speed);
            vec3_set_length(V(spot), V(step), 0.05f);
            metal_25f8e0(spot, 0.025f);
            vec3_add_inplace(V(spot), V(at));
            const float gravity = -fpu_mul(lwc1(pv + 0xdc), frame_time_sq());
            swc1(step + 8, sub_0025abf0(spot, from, nullptr, speed, gravity));
            const uint32_t pod = class455_pod_spawn(m, spot, step, lh(pv + 0x8c), lbu(pv + 0x83));
            if (pod != 0) sb(pod + 0x30, 0xff);
            sh(pv + 0x88, static_cast<uint32_t>(frame_scale_i(lh(pv + 0x8a))));
            count_one(pv);
        }
    }
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x160);
}

// orig: L3:0x002bec60 L8:0x002e6228 L14:0x002e1818
uint32_t class545_revive(uint32_t owner, uint32_t pos, int32_t cls) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    lq_sq(sp, pos);
    uint32_t pv = 0;
    if (owner == 0 || (pv = lwu(owner + 0x78)) == 0) {
        ee_stack_free(0xc0);
        return 0;
    }
    sb(pv + 0x86, 0);
    bool revived = false;
    uint32_t back = 0;
    for (uint32_t i = 0; i < LIST_LEN; ++i) {
        const int32_t idx = lw(pv + 0xe8 + i * 4);
        if (idx == -1) continue;
        const uint32_t o = indexed_moby(idx);
        if (o != 0 && !gone(o)) {
            count_one(pv);
            continue;
        }
        if (cls != -1 && lh(o + 0xa6) != cls) continue;
        if (revived) continue;
        // brought back: at the spot, state 0, its class's flags and scale, the Spawner's draw words
        count_one(pv);
        revived = true;
        lq_sq(o + 0x10, sp);
        sw(o + 0x1c, 0);
        sb(o + 0x20, 0);
        sb(o + 0xbc, 0);
        const uint32_t c = lwu(o + 0x24);
        const uint32_t flags = lhu(c + 0x44);
        sh(o + 0x34, flags);
        swc1(o + 0x2c, lwc1(c + 0x24));
        sb(o + 0x30, lbu(owner + 0x30));
        sb(o + 0x31, 1);
        sh(o + 0x32, lhu(owner + 0x32));
        if (lwu(c + 0x40) != 0) sh(o + 0x34, flags | 0x10u);
        if (lbu(lwu(o + 0x24) + 0xf) != 0) sh(o + 0x34, lhu(o + 0x34) | 0x400u);
        sb(o + 0x71, 0xff);
        sh(o + 0x36, 0x7f80);
        sb(o + 0x72, 0xff);
        sb(o + 0xa4, 0xff);
        if ((lbu(pv + 0x83) & 2) != 0) swc1(o + 0x2c, fpu_mul(lwc1(lwu(o + 0x24) + 0x24), u2f(0x3dcccccdu)));
        moby_set_anim(o, 0, 0);
        const uint32_t hooks = class_hook_10(o);
        if (hooks != 0) {
            sw(hooks + 0x6c, 0);
            sw(hooks + 0x94, 0);
            sh(hooks + 0x68, 0);
        }
        // its health from the Spawner's share (+0xb4), at least 1; the Spawner's share drops by it (at least 1 left)
        if (lh(owner + 0xb4) < lh(o + 0xb4)) sh(o + 0xb4, lhu(owner + 0xb4));
        if (lh(o + 0xb4) <= 0) sh(o + 0xb4, 1);
        const uint32_t left = lhu(owner + 0xb4) - lhu(o + 0xb4);
        sh(owner + 0xb4, left);
        if (static_cast<int16_t>(left) < 0) sh(owner + 0xb4, 1);
        sw(o + 0x94, lwu(lwu(o + 0x24) + 0x10));
        back = o;
        moby_copy_hero_color(o);
        moby_update_transform(o);
        const uint32_t w0 = moby_pvar_word0(o);
        if (w0 != 0) {
            const int32_t v = lh(w0 + 4);
            sw(w0 + 0x34, 0);
            sw(w0 + 0x30, 0);
            swc1(w0, int_to_float(v));
        }
    }
    count_live_pods(pv);
    if (back == 0) debug_printf_stub();  // (the original's message: no moby to bring back)
    ee_stack_free(0xc0);
    return back;
}

// The pod. State 0 flies: it grows to its class's scale, moves by its velocity, and bounces off what it meets (along its
// path ahead, then around it): off another pod it keeps its course a little faster or slower upward; off a moby it turns
// and, on gentle ground, speeds up; off the ground it turns and, on gentle ground, slows to a stop (state 1). Live mobys
// around push it away; its timer +0x14 out, it fades (state 2); it turns with its velocity and falls. State 1 lands: it
// sits on the floor and brings back one of its Spawner's mobys (state 2). State 2 shrinks while the moby it brought grows
// to its scale, and leaves once that one is grown or gone. Out of the height band 5..500 it leaves; else it is kept in
// the box 5..1018.
// orig: L3:0x002c4718 L8:0x002ee6f0 L14:0x002e72d8
void class545_update(uint32_t m) {
    if (m == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (pv == 0) return;
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t was = sp, ahead = sp + 0x10, push = sp + 0x20;
    const uint32_t pos = m + 0x10;
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x20)));
    lq_sq(was, pos);

    // off what it met (the collision's result: the moby +0x18, the normal +0x40)
    auto bounce = [&] {
        const uint32_t other = lwu(OV(0x00173e58));
        if (other != 0 && lh(other + 0xa6) == static_cast<int32_t>(POD_CLASS)) {
            swc1(pv + 8, fpu_mul(lwc1(pv + 8), rand_range_f(u2f(0x3f666666u), u2f(0x3f8ccccdu))));
        } else {
            vec3_reflect(V(pv), V(pv), V(OV(0x00173e80)));
            const float slope = atan2_f(lwc1(OV(0x00173e88)), vec2_length(V(OV(0x00173e80))));
            if (slope < u2f(0x3f32b8c2u) && sub_001f35d8() == -1) {  // (40 degrees)
                if (other != 0) {
                    vec3_scale(V(pv), V(pv), u2f(0x3f99999au));
                } else {
                    vec3_scale(V(pv), V(pv), 0.5f);
                    if (vec3_length(V(pv)) < u2f(0x3ccccccdu)) sb(m + 0x20, 1);
                }
            }
        }
        if (lw(pv + 0x20) == 0) {
            moby_sound_start(0, 0, m);
            sw(pv + 0x20, 0xc);
        }
    };

    bool leave = false;
    switch (lbu(m + 0x20)) {
    case 0: {  // flying
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_add(scale, fpu_mul(fpu_sub(lwc1(lwu(m + 0x24) + 0x24), scale), fpu_mul(frame_scale(), 0.05f))));
        vec3_add(V(pos), V(pos), V(pv));
        vec3_set_length(V(ahead), V(pv), 0.2f);
        vec3_add(V(ahead), V(ahead), V(pos));
        // (the pod and its Spawner kept out of the collision for the frame: their +0x98 / +0x94 saved and cleared)
        const uint32_t own_98 = lwu(m + 0x98), own_94 = lwu(m + 0x94);
        const uint32_t spawner_94 = lwu(lwu(pv + 0x10) + 0x94), spawner_98 = lwu(lwu(pv + 0x10) + 0x98);
        sw(m + 0x98, 0xffffffffu);
        sw(lwu(pv + 0x10) + 0x98, 0xffffffffu);
        sw(m + 0x94, 0);
        sw(lwu(pv + 0x10) + 0x94, 0);
        if (segment_collide_v(V(was), V(ahead), 4, m, nullptr) != 0) {
            vec3_sub(V(ahead), V(OV(0x00173e60)), V(ahead));
            vec3_set_length(V(ahead), V(ahead), u2f(0x3e5c28f6u));
            vec3_add(V(pos), V(pos), V(ahead));
            bounce();
        }
        if (sphere_collide(pos, 0.2f, 4, m) != 0) {
            vec3_sub(V(ahead), V(OV(0x00173e60)), V(OV(0x00173e70)));
            vec3_set_length(V(ahead), V(ahead), u2f(0x3c75c28fu));
            vec3_add(V(pos), V(OV(0x00173e70)), V(ahead));
            bounce();
        }
        for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
            if (o == m || gone(o)) continue;
            vec3_sub(V(push), V(pos), V(o + 0x10));
            sw(push + 8, 0);
            const float d = vec3_length(V(push));
            if (!(d < u2f(0x3fe66666u))) continue;  // (1.8)
            vec3_scale(V(push), V(push), fpu_mul(fpu_sub(u2f(0x3fe66666u), d), fpu_add(fpu_mul(frame_scale(), -0.5f), 1.0f)));
            vec3_add(V(pos), V(pos), V(push));
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x14))) != 0) sb(m + 0x20, 2);
        swc1(m + 0x40, angle_wrap_mod(lwc1(pv)));
        swc1(m + 0x44, angle_wrap_mod(lwc1(pv + 4)));
        const float frame = frame_scale();
        sw(m + 0x98, own_98);
        sw(lwu(pv + 0x10) + 0x98, spawner_98);
        sw(m + 0x94, own_94);
        sw(lwu(pv + 0x10) + 0x94, spawner_94);
        swc1(pv + 8, fpu_sub(lwc1(pv + 8), fpu_mul(frame_time_sq(), u2f(0x411ccccdu))));  // (gravity 9.8)
        swc1(m + 0x40, fpu_add(lwc1(m + 0x40), fpu_mul(frame, u2f(0x3c23d70au))));
        swc1(m + 0x44, fpu_add(lwc1(m + 0x44), fpu_mul(frame, u2f(0x3ca3d70au))));
        break;
    }
    case 1: {  // landing
        const float floor = floor_probe_v(0.5f, V(pos), 0);
        sw(m + 0x1c, 0);
        swc1(m + 0x18, fpu_add(floor, 0.2f));
        lq_sq(ahead, pos);
        sw(pv + 0x18, class545_revive(lwu(pv + 0x10), ahead, -1));
        sb(m + 0x20, 2);
        break;
    }
    case 2: {  // fading
        const float k = fpu_mul(frame_scale(), 0.05f);
        const float scale = lwc1(m + 0x2c);
        swc1(m + 0x2c, fpu_sub(scale, fpu_mul(scale, k)));
        const uint32_t brought = lwu(pv + 0x18);
        if (brought != 0 && (lw(pv + 0x1c) & 4) != 0) {
            const float s = lwc1(brought + 0x2c);
            swc1(brought + 0x2c, fpu_add(s, fpu_mul(fpu_sub(lwc1(lwu(brought + 0x24) + 0x24), s), k)));
        }
        if (lwc1(m + 0x2c) < fpu_mul(lwc1(lwu(m + 0x24) + 0x24), fpu_mul(frame_scale(), 0.05f))) {
            swc1(m + 0x2c, u2f(0x38d1b717u));
            const uint32_t b = lwu(pv + 0x18);
            leave = b == 0 || gone(b) || (lw(pv + 0x1c) & 4) == 0 ||
                    fpu_sub(lwc1(lwu(b + 0x24) + 0x24), u2f(0x3c23d70au)) <= lwc1(b + 0x2c);
        }
        break;
    }
    default:
        break;
    }
    if (!leave) {
        floor_light_add(m, fpu_div(fpu_mul(lwc1(m + 0x2c), u2f(0x3e99999au)), lwc1(lwu(m + 0x24) + 0x24)));
        const float z = lwc1(m + 0x18);
        leave = z < 5.0f || 500.0f < z;
    }
    if (leave) {
        moby_retire(m);
        ee_stack_free(0xf0);
        return;
    }
    for (uint32_t k = 0; k < 0xc; k += 4) {
        if (lwc1(pos + k) < 5.0f) swc1(pos + k, 5.0f);
        if (1018.0f < lwc1(pos + k)) swc1(pos + k, 1018.0f);
    }
    ee_stack_free(0xf0);
}

}  // namespace rac1
