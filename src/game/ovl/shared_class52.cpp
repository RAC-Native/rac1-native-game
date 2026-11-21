// Class 52: one port of the copies Gaspar (L9) and Kalebo III (L16) carry (the same code relinked; compared word by word:
// only relocated words differ). Each level keeps its own tuning record (per level below: +0 the knock-back's spin, +8 its
// push, +0xc / +0x10 the death's lift and push, +0x14 the glow's scale); the rest is the engine's (OV: the camera
// 0x166dc0, the game's mode 0x15f5c4, the path table 0x1b04b0) or the base ELF's (bare: the hero's height 0x13f3d8 and
// moby 0x1413d0, the sound channels 0x13e550, the frame's time 0x15ed6c and its square 0x15ed70, the frame scale
// 0x15ed60). Header shared_class52.h.
//
// The pvars: +0x20 the health, +0x38 a request to deal the search timer anew, +0x60 the colour pulse, +0x70 the move record
// (+0x80 .. +0x98 its knock-back, +0xc0 / +0xc4), +0xd0 the walker record, +0xf4 its speed, +0x120 the target's point,
// +0x160 the target, +0x164 2 when out of reach, +0x180 its path (an index of the path table), +0x184 the life timer,
// +0x188 the search's range, +0x18c the yaw's rate, +0x190 the search timer, +0x194 the sound's channel, +0x198 whether
// it closes in, +0x280 the joint's scale.
#include <game/ovl/shared_class52.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
// the level's tuning record (+0 .. +0x14)
uint32_t tuning() { return on_level(KALEBO) ? L16_ADDR(0x00161428) : L9_ADDR(0x001613f8); }

// the sound on channel `slot` (a moby's: its pvars +0x194) stopped when it is still this moby's and playing
void stop_own_sound(uint32_t m, int32_t slot) {
    if (slot == -1) return;
    const uint32_t ch = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
    if (lwu(ch + 0x88) != m || lbu(ch + 0x74) == 0) return;
    sub_0028d918(slot);
}

// the group's members (the moby's +0x21, 0xff none) told 1
void tell_group(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group != 0xff) moby_group_set_bc_l1(group, 1);
}

// the yaw turned toward `target`'s place (at most 2.78 pi a second, eased by 4 pi)
void seek_target(uint32_t m, uint32_t pv, uint32_t target) {
    const float yaw = atan2_f(fpu_sub(lwc1(target + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(target + 0x14), lwc1(m + 0x14)));
    const float rate = fpu_mul(lwc1(0x0015ed70), u2f(0x410ba058u));          // (8.73)
    const float most = fpu_mul(lwc1(0x0015ed6c), u2f(0x41490fdbu));          // (4 pi)
    yaw_seek(m + 0x48, pv + 0x18c, yaw, rate, rate, most);
}

// the knock-back from the hit's source (the hit record `hit`; its vector copied to `v`, the yaw to `yaw_word`): the move
// record's turn and push, then its move state
void knock_back(uint32_t m, uint32_t pv, uint32_t hit, uint32_t v, uint32_t yaw_word) {
    const uint32_t src = lwu(hit + 0x20);
    const float yaw = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(src + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(src + 0x14)));
    swc1(yaw_word, yaw);
    lq_sq(v, hit + 0x10);
    vec_yaw_marked_v(V(v), *static_cast<float*>(ee_ram_ptr(yaw_word)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                     *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
    move_state_start(m, pv + 0x70, 4, 1, 0, lwc1(yaw_word));
}
}  // namespace

// orig: L9:0x002c6230 L16:0x002a1928
void class52_hit(uint32_t m) {
    if (lbu(m + 0x20) == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t t = tuning();
    // the glow eases toward the class's size times the level's scale
    const float glow = lwc1(m + 0x2c);
    swc1(m + 0x2c, fpu_add(glow, fpu_mul(fpu_sub(fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(t + 0x14)), glow), u2f(0x3dcccccdu))));  // (0.1)
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x184))) == 2) {  // (its life ran out)
        sb(m + 0x20, 7);
        return;
    }
    const uint32_t pulse = pv + 0x60;
    // a pulse at 10, 20, 40, 60, 120 and 179 frames before the end
    const int32_t left = lw(pv + 0x184);
    if (left == frame_scale_i(0xa) || left == frame_scale_i(0x14) || left == frame_scale_i(0x28) || left == frame_scale_i(0x3c) ||
        left == frame_scale_i(0x78) || left == frame_scale_i(0xb3)) {
        sb(pv + 0x67, 0xf0);
        color_pulse_start(m, pulse);
    }
    const uint32_t sp = ee_stack_alloc(0xa0);
    int32_t st = 0;
    float dmg = 0.0f;
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000u), 0);
    int32_t r = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    if (st != 1 && lbu(m + 0x20) != 6) {
        const float health = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, health);
        if (health <= 0.0f) r = 1;
        const int32_t glint = float_to_int(512.0f);
        const float ft2 = lwc1(0x0015ed70);
        swc1(pv + 0x98, 0.0f);
        sw(pv + 0x94, 1);
        sb(pv + 0xad, 0);
        sw(pv + 0x90, static_cast<uint32_t>(glint));
        swc1(pv + 0x80, fpu_mul(lwc1(t), ft2));
        swc1(pv + 0x84, fpu_mul(ft2, 30.0f));
        moby_sound_start(8, 0, m);
        const uint32_t kind = static_cast<uint32_t>(r);
        if (kind == 1 || kind == 2) {  // (killed: lifted and pushed away, the bolts, state 6)  (0x002c64f8)
            const float ft = lwc1(0x0015ed6c);
            sh(m + 0x34, lhu(m + 0x34) & 0xefff);
            swc1(pv + 0x80, fpu_mul(lwc1(t), lwc1(0x0015ed70)));
            swc1(pv + 0xc0, 14.0f);
            swc1(pv + 0xc4, 28.0f);
            swc1(pv + 0x88, fpu_mul(lwc1(t + 0x10), ft));
            swc1(pv + 0x8c, fpu_mul(lwc1(t + 0xc), ft));
            knock_back(m, pv, hit, sp, sp + 0x1c);
            moby_drop_bolts(m, 0, -1);
            sb(m + 0x20, 6);
            sb(pv + 0x67, 0xf0);
        } else if (kind >= 3 && kind <= 8) {  // (knocked back: state 5)  (0x002c6444)
            swc1(pv + 0xc0, 3.0f);
            swc1(pv + 0xc4, 6.0f);
            swc1(pv + 0x88, fpu_mul(lwc1(t + 8), lwc1(0x0015ed6c)));
            sw(pv + 0x8c, 0);
            knock_back(m, pv, hit, sp, sp + 0x18);
            sb(m + 0x20, 5);
            tell_group(m);
            sb(pv + 0x67, 0x78);
        } else if (kind == 9 || kind == 10) {  // (0x002c6438)
            sb(pv + 0x67, 0xfa);
        }
        color_pulse_start(m, pulse);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pulse);
    // the search timer: dealt anew on request (or a flag of the moby's), 3 .. 4 seconds
    if (lw(pv + 0x38) != 0 || lbu(m + 0xbc) == 1) {
        sw(pv + 0x190, static_cast<uint32_t>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f)))));
        sw(pv + 0x38, 0);
        sb(m + 0xbc, 0);
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x190)));
    const uint32_t point = pv + 0x120;
    if (rand_mod(4) != 0) {
        // three frames in four: the target's place followed while it lives
        const uint32_t target = lwu(pv + 0x160);
        if (target == 0 || lbu(target + 0x20) == 0xfe || lbu(target + 0x20) == 0xfd) {
            sw(pv + 0x160, 0);
            sw(pv + 0x164, 2);
        } else {
            lq_sq(point, target + 0x10);
        }
    } else {
        // else a search: anything within 24 while the search timer runs, else one in its path's boxes within 12
        int32_t found;
        if (lw(pv + 0x190) != 0) {
            swc1(pv + 0x188, 24.0f);
            found = target_select_any(m, point, 24.0f);
        } else {
            swc1(pv + 0x188, 12.0f);
            const uint32_t path = lwu(OV(0x001b04b0) + (lwu(pv + 0x180) << 2));
            found = target_select(m, point, 0, 0, path + 0x10, lw(path), 12.0f);
        }
        if (found != 2 && lw(pv + 0x190) == 0) {
            if (lwc1(pv + 0x188) < vec2_distance(V(m + 0x10), V(point))) sw(pv + 0x164, 2);
            else if (3.0f < abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0x128)))) sw(pv + 0x164, 2);
        }
    }
    if (lwu(pv + 0x160) == 0) sw(pv + 0x160, lwu(0x001413d0));
    ee_stack_free(0xa0);
}

// Hit, then by its state: 0 set up (no path: gone), 1 hover and wait (its target in reach: 2), 2 / 4 drift after the target
// (within 6 in 2: 3), 3 the cry (4), 5 knocked back (slowed down: 4), 6 dying (on the ground or a wall: a burst, gone),
// 7 its life over (an explosion, gone). Nothing while the game is not playing (mode 2).
// orig: L9:0x002c5990 L16:0x002a1088
void class52_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    class52_hit(m);
    const uint32_t target = lwu(pv + 0x160);
    joint_node_scale(m, 0, pv + 0x280, u2f(0x402ccccdu));  // (2.7)
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(OV(0x00166dc0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    if (lw(OV(0x0015f5c4)) == 2) return;
    const uint32_t state = lbu(m + 0x20);
    if (state >= 8) return;
    auto sound_on = [m, pv] {
        if (lw(pv + 0x194) == -1) sw(pv + 0x194, static_cast<uint32_t>(moby_sound_start(1, 4, m)));
    };
    auto life_timer = [pv] {
        if (lw(pv + 0x184) == 0) sw(pv + 0x184, static_cast<uint32_t>(frame_scale_i(0xb4)));
    };
    // (the state table 0x208350 on Gaspar: 0 -> 0x002c5a5c, 1 -> 0x002c5b28, 2 / 4 -> 0x002c5d48, 3 -> 0x002c5cbc,
    // 5 -> 0x002c602c, 6 -> 0x002c60b8, 7 -> 0x002c6144)
    switch (state) {
    case 0: {
        swc1(pv + 0x20, 3.0f);
        sh(pv + 0x24, 3);
        sb(pv + 0x28, 2);
        sb(pv + 0x5a, 6);
        sb(pv + 0x58, 8);
        if (lw(pv + 0x180) == -1) {
            debug_printf_stub();  // ("no path" with its uid, a stub)
            moby_retire(m);
            return;
        }
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0xd8, 0.5f);
        swc1(pv + 0xdc, 0.5f);
        sw(pv + 0xd0, static_cast<uint32_t>(float_to_int(512.0f)));
        swc1(pv + 0xf4, fpu_mul(lwc1(0x0015ed6c), 7.75f));
        sb(m + 0x20, 1);
        if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0xa));
        sw(pv + 0x194, 0xffffffffu);
        return;
    }
    case 1: {
        // hover half a unit over the floor, turn to the target, now and then another idle animation
        const float floor = floor_probe_v(0.5f, V(m + 0x10), 0);
        const float z = lwc1(m + 0x18);
        swc1(m + 0x18, fpu_add(z, fpu_mul(fpu_sub(fpu_add(floor, 0.5f), z), u2f(0x3dcccccdu))));  // (0.1)
        seek_target(m, pv, target);
        if ((lbu(m + 0x70) & 2) != 0) {
            if (rand_mod(0x13) == 0) {
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, rand_range(7, 0xc));
            } else {
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, rand_range(7, 0xe));
            }
            swc1(m + 0x58, rand_range_f(u2f(0x3f733333u), u2f(0x3f866666u)));  // (0.95 .. 1.05)
        }
        if (lw(pv + 0x164) == 2) return;
        tell_group(m);
        sound_on();
        sw(pv + 0x198, 1);
        sb(m + 0x20, 2);
        return;
    }
    case 3: {
        if ((lbu(m + 0x70) & 2) == 0) return;
        tell_group(m);
        sw(pv + 0x198, 1);
        sb(m + 0x20, 4);
        sound_on();
        if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, frame_scale_i(0xa));
        life_timer();
        return;
    }
    case 2:
    case 4: {
        const float z0 = lwc1(m + 0x18);
        seek_target(m, pv, target);
        const float d = vec2_distance(V(m + 0x10), V(target + 0x10));
        if (u2f(0x3ff33333u) < d) sw(pv + 0x198, 1);  // (1.9)
        if (d < u2f(0x3fb33333u)) sw(pv + 0x198, 0);  // (1.4)
        // the step: ahead at its speed while it closes in, else none; its rise toward a height over the hero's, capped by
        // the frame
        const uint32_t sp = ee_stack_alloc(0x90);
        const uint32_t dir = sp + 0x20;
        float rise;
        if (lw(pv + 0x198) != 0) {
            swc1(dir, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0xf4)));
            swc1(dir + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0xf4)));
            sw(dir + 8, 0);
            const float over = fpu_add(lwc1(0x0013f3d8), u2f(0x3f4ccccdu));  // (0.8)
            const float k = lwc1(m + 0x18) < over ? u2f(0x3dcccccdu) : u2f(0x3d4ccccdu);  // (0.1 below, else 0.05)
            rise = fpu_mul(fpu_sub(over, z0), k);
        } else {
            sd(dir, 0);
            sd(dir + 8, 0);
            const float over = fpu_add(lwc1(0x0013f3d8), u2f(0x3e4ccccdu));  // (0.2)
            const float k = lwc1(m + 0x18) < over ? u2f(0x3dcccccdu) : u2f(0x3d4ccccdu);
            rise = fpu_mul(fpu_sub(over, z0), k);
        }
        const float scale = lwc1(0x0015ed60);
        const float up = fpu_add(lwc1(dir + 8), rise);
        const float most = fpu_mul(scale, u2f(0x3e99999au));  // (0.3)
        swc1(dir + 8, up);
        if (most < up) swc1(dir + 8, most);
        else {
            const float least = fpu_mul(scale, u2f(0xbe99999au));  // (-0.3)
            if (up < least) swc1(dir + 8, least);
        }
        walker_move(m, dir, 0, 0.0f, 0.5f, 0.0f);
        ee_stack_free(0x90);
        if (!(d < 6.0f) || lbu(m + 0x20) != 2) return;
        sb(m + 0x20, 3);
        if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, frame_scale_i(0xa));
        return;
    }
    case 5: {
        const float z = lwc1(m + 0x18);
        enemy_physics(m, pv + 0x70);
        swc1(m + 0x18, z);
        sw(pv + 0x78, 0);
        const float ft = lwc1(0x0015ed6c);
        if (!(vec3_length(V(pv + 0x70)) < fpu_add(ft, ft))) return;
        sound_on();
        sw(pv + 0x198, 1);
        sb(m + 0x20, 4);
        life_timer();
        return;
    }
    case 6:
        stop_own_sound(m, lw(pv + 0x194));
        sw(pv + 0x194, 0xffffffffu);
        if ((enemy_physics(m, pv + 0x70) & 0x121) == 0) return;
        explosion_burst(m, m + 0x10, 0, 0.5f, 13.0f);
        moby_retire(m);
        return;
    default: {  // 7
        moby_drop_bolts(m, 0, -1);
        stop_own_sound(m, lw(pv + 0x194));
        sw(pv + 0x194, 0xffffffffu);
        const uint32_t sp = ee_stack_alloc(0x90);
        const uint32_t none = sp + 0x20;
        vec_clear(V(none));
        explosion_effect(m, none, m + 0x10, 0xa, 3, 0x10, 0, 0, 0, -1, 0, 2.0f, 1.0f, 4.0f, 2.0f, 9.0f, 1.0f, 15.0f);
        ee_stack_free(0x90);
        moby_retire(m);
        return;
    }
    }
}

}  // namespace rac1
