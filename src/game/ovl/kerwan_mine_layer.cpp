// Kerwan's own moby classes: class 578, the mine layer of Metropolis, a walker that patrols its path and drops a spread of five
// mines (class 627) behind it at a time. Header kerwan_mobys.h, registered in kerwan_mobys.cpp. Addresses are Kerwan's
// (`orig: L3:`); data only Kerwan has is L3_ADDR, the engine's words shared with Veldin are the level's own (OV), the base ELF's
// are bare.
//
// Its pvars: +0x20 its health, +0x38 alerted, +0x40 the walk's out record, +0x60 the colour pulse, +0x70 the move record, +0xd0
// the walker record, +0xe8 its fall's speed, +0x120 its path follower (+0x120 the point, +0x124 the direction, +0x130 its path),
// +0x134 its path's index, +0x150 the turn's speed, +0x154 the wait between spreads, +0x156 the wait before it may turn back,
// +0x15c the alert's time, +0x160 the joint it scales.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x80 his position)

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the heading from the moby to (x, y)
float heading_to(uint32_t m, float x, float y) { return atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14))); }

// The heading from the hero to the moby.
float away_from_hero(uint32_t m) {
    return atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(HERO + 0x80)), fpu_sub(lwc1(m + 0x14), lwc1(HERO + 0x84)));
}

// Its end: an explosion half a unit above its place, its bolts, removed.
void mine_layer_blow_up(uint32_t m, uint32_t pv, uint32_t at) {
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), 0.5f));
    explosion_effect(m, pv + 0x40, at, 5, 2, 4, -1, 1, 1, -1, 0, 0.0f, 0.0f, 2.0f, 1.0f, 9.0f, 1.0f, 15.0f);
    moby_drop_bolts(m, 0, -1);
    moby_retire(m);
}
}  // namespace

// The mine layer's hits (kinds 0x330000): a hit the hit reaction does not shrug off costs its damage; by the reaction's kind (or
// 1 when its health is gone), away from the hero with the level's jump numbers: 1 its end (state 5, the move state 5, drawn
// apart), 3 / 6 and 4 / 5 knocked back (state 4, the move states 3 and 4); each with its colour pulse.
// orig: L3:0x002ca808 (the jump table L3:0x1e3240 on the reaction's kind - 1)
void mine_layer_hits_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t from = sp + 0x18;
    const uint32_t pv = lwu(m + 0x78);
    int32_t state = 0;
    float damage = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t kind = moby_hit_react_r(m, hit, pv + 0x20, 0, state, &damage, nullptr, 4);
    if (state != 1 && lbu(m + 0x20) != 5) {
        const uint32_t src = lwu(hit + 0x20);
        if (src == 0 || src == lwu(0x001403e0)) swc1(from, away_from_hero(m));
        else swc1(from, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14))));
        const float health = fpu_sub(lwc1(pv + 0x20), damage);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) kind = 1;
        auto knock = [&](uint32_t g, uint32_t height, uint32_t gravity, uint32_t move_set) {
            swc1(pv + 0x80, fpu_mul(lwc1(g), dt_sq()));
            jump_arc_set(pv + 0x70, lwc1(height), lwc1(gravity));
            sb(pv + 0xad, 0);
            sw(pv + 0x94, 9);
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(from)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, move_set, 1, 0, away_from_hero(m));
        };
        switch (kind) {
        case 1:  // (0x002caaa4)
            knock(L3_ADDR(0x001618f8), L3_ADDR(0x00161908), L3_ADDR(0x0016190c), 5);
            swc1(pv + 0xc0, 8.0f);
            swc1(pv + 0xc4, 15.0f);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0xfa);
            sb(m + 0x20, 5);
            break;
        case 3: case 6:  // (0x002ca950)
            knock(L3_ADDR(0x001618fc), L3_ADDR(0x00161900), L3_ADDR(0x00161904), 3);
            swc1(pv + 0xc0, 5.0f);
            swc1(pv + 0xc4, 10.0f);
            sb(m + 0x20, 4);
            break;
        case 4: case 5:  // (0x002ca9f4)
            knock(L3_ADDR(0x001618fc), L3_ADDR(0x00161900), L3_ADDR(0x00161904), 4);
            swc1(pv + 0xc0, 7.5f);
            swc1(pv + 0xc4, 15.0f);
            sb(m + 0x20, 4);
            break;
        default:  // 2 (0x002cab64) and beyond the table
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    ee_stack_free(0x80);
}

// A mine (class 627) dropped by `layer` at `at` moving `vel` (EE vectors): drawn as far as the layer is, its colour and its
// layer's lighting words, its spin (the frame's rate x 0.008 and x 0.0005), its speed and rise, its bounce numbers, the move
// record started along `vel`, its layer kept (+0x68). The mine, or 0.
// orig: L3:0x002cc888
uint32_t mine_layer_drop_l3(uint32_t layer, uint32_t at, uint32_t vel) {
    const uint32_t mine = moby_spawn(0x273);
    if (mine == 0) return 0;
    sh(mine + 0x32, lhu(layer + 0x32));
    sb(mine + 0x31, 1);
    sb(mine + 0x30, lbu(layer + 0x32));
    sd(mine + 0x38, ld(layer + 0x38));
    lq_sq(mine + 0x10, at);
    const float rate = lwc1(0x0015ed64);
    const uint32_t mpv = lwu(mine + 0x78);
    swc1(mpv + 0x10, fpu_mul(rate, u2f(0x3c03126fu)));
    swc1(mpv + 0x14, fpu_mul(rate, u2f(0x3a03126fu)));
    swc1(mpv + 0x18, vec2_length(V(vel)));
    sw(mpv + 0x20, 0x400);
    swc1(mpv + 0x28, 1.0f);
    swc1(mpv + 0x30, 0.75f);
    swc1(mpv + 0x34, 0.5f);
    swc1(mpv + 0x38, u2f(0x3f266666u));
    swc1(mpv + 0x48, u2f(0x3e4ccccdu));
    swc1(mpv + 0x4c, u2f(0x3c23d70au));
    swc1(mpv + 0x1c, lwc1(vel + 8));
    sb(mpv + 0x3d, 0);
    sw(mpv + 0x24, 1);
    move_state_start(mine, mpv, 0, 1, 0, atan2_f(lwc1(vel), lwc1(vel + 4)));
    sw(mpv + 0x68, layer);
    sw(mpv + 0x6c, 0);
    sb(mine + 0x20, 1);
    return mine;
}

// Class 578, the mine layer. Each frame: its target (within 100), its joint scaled, its hits; alerted (+0x38) it notices the
// target within 28 for 240 frames, else 14. The state (+0x20):
// 0 its set-up (walker record, speeds, health 2, at its path's start, its size by the level's scale) -> 1;
// 1 turns to the target; one near and level -> 2;
// 2 at its animation's end -> 3 (the animation 2, its waits);
// 3 walks its path (doomlet_259028); at a point it goes on in its direction, or turns back when the target is nearer to the next
//   point than it is (not again for 120 frames); a target beyond 25 sends it back to 1; each spread wait (the level's) it drops
//   five mines from -2 to 2 across its side axis, thrown back and out, with its sound;
// 4 knocked back (enemy_physics): landed -> 1; below z 5 its end; 5 its end, on the ground or below z 5.
// Then in states 0 .. 3 its gravity and the floor (below z 5: its end), and its floor band within 32 of the camera.
// orig: L3:0x002c9eb8 (the jump table L3:0x1e3220 on the state, in order)
void kerwan_class578_update_l3(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x160);
    const uint32_t T = sp + 0x20;  // the target record
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t fol = pv + 0x120;
    target_select_any(m, T, 100.0f);
    joint_node_scale(m, 0, pv + 0x160, 4.5f);
    mine_layer_hits_l3(m);
    if (lw(pv + 0x38) != 0) {
        sw(pv + 0x38, 0);
        sw(pv + 0x15c, static_cast<uint32_t>(frame_scale_i(0xf0)));
    }
    const float notice = timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x15c))) == 0 ? 28.0f : 14.0f;
    auto anim_to = [&](uint32_t set, int32_t frames) {
        if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
    };
    auto path = [&] { return lwu(fol + 0x10); };
    auto step = [&] { return ee_rem(lw(fol) + lw(path()) + static_cast<int8_t>(lbu(fol + 4)), lw(path())); };
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002c9f8c)
        walker_record_init_l1(pv + 0xd0);
        sb(pv + 0x5a, 8);
        sb(pv + 0x58, 8);
        swc1(pv + 0x114, fpu_mul(dt(), u2f(0x40c90fdbu)));
        swc1(pv + 0x110, fpu_mul(dt_sq(), u2f(0x4196cbe4u)));
        swc1(pv + 0xf4, fpu_mul(dt(), 5.0f));
        swc1(pv + 0x10c, fpu_mul(dt_sq(), u2f(0x40c90fdbu)));
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, 2);
        sb(pv + 0x28, 1);
        swc1(pv + 0xdc, 2.0f);
        swc1(pv + 0xd8, 2.0f);
        sb(pv + 0x29, 0);
        const uint32_t p = lvd::path(lw(pv + 0x134));
        sw(pv + 0x130, p);
        lq_sq(m + 0x10, p + 0x10);
        sw(pv + 0x120, 1);
        sb(m + 0x20, 1);
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), lwc1(L3_ADDR(0x00161920))));
        anim_to(0, 0);
        break;
    }
    case 1:  // (0x002ca084)
        yaw_turn_damped(m, pv + 0x150, heading_to(m, lwc1(T), lwc1(T + 4)), u2f(0x3c23d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
        if (vec2_distance(V(m + 0x10), V(T)) < notice && abs_f(fpu_sub(lwc1(T + 8), lwc1(m + 0x18))) < 4.0f) {
            sb(m + 0x20, 2);
            anim_to(1, frame_scale_i(10));
        }
        break;
    case 2:  // (0x002ca14c)
        if ((lbu(m + 0x70) & 2u) == 0) break;
        sb(m + 0x20, 3);
        sh(pv + 0x156, static_cast<uint32_t>(frame_scale_i(0x78)));
        anim_to(2, frame_scale_i(10));
        swc1(m + 0x58, lwc1(L3_ADDR(0x0016191c)));
        sh(pv + 0x154, static_cast<uint32_t>(frame_scale_i(0x78)));
        break;
    case 3: {  // (0x002ca1b0)
        const uint32_t goal_off = (static_cast<uint32_t>(lw(fol)) << 4) + 0x10;
        doomlet_259028(m, pv + 0xd0, path() + goal_off, pv + 0x40);
        swc1(m + 0x58, lwc1(L3_ADDR(0x0016191c)));
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x156)));
        if (vec3_distance(V(m + 0x10), V(path() + goal_off)) < 1.0f) {
            bool advance = true;
            if (lh(pv + 0x156) == 0) {
                if (25.0f < vec2_distance(V(m + 0x10), V(T))) {
                    sb(m + 0x20, 1);
                    anim_to(0, frame_scale_i(10));
                    advance = false;
                } else {
                    const uint32_t next = path() + (static_cast<uint32_t>(step()) << 4) + 0x10;
                    const float from_target = vec2_distance(V(T), V(next));
                    if (from_target < vec2_distance(V(m + 0x10), V(next))) {  // the target is ahead: turn back
                        sb(fol + 4, static_cast<uint32_t>(-static_cast<int8_t>(lbu(fol + 4))));
                        sh(pv + 0x156, static_cast<uint32_t>(frame_scale_i(0x78)));
                    }
                }
            }
            if (advance) sw(fol, static_cast<uint32_t>(step()));
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x154))) == 0) break;
        // the spread: five mines across its side axis (+0xd0), thrown back (+0xc0) and out, each a bit up
        sh(pv + 0x154, static_cast<uint32_t>(frame_scale_i(lw(L3_ADDR(0x00161918)))));
        const uint32_t at = sp + 0x70, vel = sp + 0x80, back = sp + 0x90;
        lq_sq(at, m + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), 2.0f));
        moby_sound_play(1, 0, m, 0x273);
        for (int32_t k = -2; k < 3; ++k) {
            const float spread = fpu_mul(int_to_float(k), rand_range_f(u2f(0x3f666666u), u2f(0x3f8ccccdu)));
            vec2_set_length(V(at), V(m + 0xd0), fpu_mul(static_cast<float>(k), 0.5f));
            vec3_add(V(at), V(at), V(m + 0x10));
            swc1(at + 8, fpu_add(lwc1(at + 8), 2.0f));
            vec2_set_length(V(back), V(m + 0xc0), fpu_mul(rand_range_f(-1.0f, 1.0f), dt()));
            vec2_set_length(V(vel), V(m + 0xd0), fpu_mul(lwc1(L3_ADDR(0x00161910)), dt()));
            vec3_scale(V(vel), V(vel), spread);
            vec3_add(V(vel), V(vel), V(back));
            const float rise = rand_range_f(u2f(0x3f666666u), u2f(0x3f8ccccdu));
            swc1(vel + 8, fpu_mul(fpu_mul(lwc1(L3_ADDR(0x00161914)), dt()), rise));
            mine_layer_drop_l3(m, at, vel);
        }
        break;
    }
    case 4:  // (0x002ca4c8)
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0) {
            anim_to(0, frame_scale_i(10));
            sb(m + 0x20, 1);
            break;
        }
        if (lwc1(m + 0x18) < 5.0f) {
            mine_layer_blow_up(m, pv, sp + 0x70);
            ee_stack_free(0x160);
            return;
        }
        break;
    case 5:  // (0x002ca5a8)
        if ((enemy_physics(m, pv + 0x70) & 0x60u) != 0 || lwc1(m + 0x18) < 5.0f) {
            mine_layer_blow_up(m, pv, sp + 0x70);
            ee_stack_free(0x160);
            return;
        }
        break;
    default:
        break;
    }
    // gravity and the floor in states 0 .. 3
    if (lbu(m + 0x20) < 4) {
        swc1(pv + 0xe8, fpu_add(lwc1(pv + 0xe8), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0xe8), 2.0f)));
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
        swc1(m + 0x18, z);
        if (z < floor) {
            swc1(m + 0x18, floor);
            sw(pv + 0xe8, 0);
        } else if (z < 5.0f) {
            mine_layer_blow_up(m, pv, sp + 0x70);
            ee_stack_free(0x160);
            return;
        }
    }
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 32.0f) moby_floor_band(m);
    ee_stack_free(0x160);
}

}  // namespace rac1
