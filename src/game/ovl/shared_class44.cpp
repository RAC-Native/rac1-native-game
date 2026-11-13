// Class 44 of Quartu (level 15) and Drek's fleet (17): a guard. It walks its path (pvars +0x124, its point +0x13c), stopping
// at each point; seeing the hero (the target the path's polygon picks, +0xd0) it calls its group, raises its alarm (a class
// 408 moby, +0x128), turns and shoots (class 934's shots); hit, it is knocked back or dies; an ally calling (class 635) or a
// guard of its group draws its look. One port of both levels' copies, the same code relinked: its tunings and the frame's
// alert are the level's own data (one block, per level below), the turns' angle tables the level's own, the glows' draw
// callback the level's own copy; the moby table 0x15ffd8, the group lists 0x1ab840, the paths 0x1b04b0, the camera 0x166dc0,
// the frame counters 0x15f5c4 / 0x15f5cc are the engine's (OV); the hero (0x13f350.., its moby 0x1413d0, its state 0x1413d4,
// its alert 0x141630), the frame times 0x15ed64 / 0x15ed6c / 0x15ed70 the base ELF's. Header shared_class44.h.
//
// The pvars: +0x20 its health, +0x38 a new watch time asked, +0x60 its colour pulse, +0x70 its knock-back, +0xd0 its target's
// point, +0x100 its aim point, +0x110 its target moby, +0x114 its view (1 seen, 2 lost), +0x120 its home path, +0x124 its
// path, +0x128 its alarm, +0x12c / +0x150 two class-78 mobys it breaks, +0x130 its watch time (a float timer), +0x134 its
// speed and +0x138 its turn rate, +0x13c its point on the path, +0x144 at a point, +0x148 its stop (frames) and +0x14c its
// timer, +0x154 .. +0x158 timers (i16), +0x15a the hero in sight, +0x15b alerted, +0x15c / +0x15d the state and animation
// after a turn, +0x15e .., +0x15f called, +0x160 its home, +0x168 its height there, +0x16c its home facing, +0x170 .. +0x178
// a turn's start, end and size, +0x17c its ally group, +0x180 .. +0x18c four mobys waited for, +0x190 an ally, +0x194 calling,
// +0x196 the time until it looks for allies, +0x198 a guard it looks at and +0x19c for how long, +0x19e shot, +0x19f looking,
// +0x1a0 / +0x220 its head and body joints, +0x2a0 its glows' points, +0x2d0 .. +0x2dc four camera boxes.
#include <game/ovl/shared_class44.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/shared_class408.h>
#include <game/ovl/shared_class78.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
constexpr int32_t GUARD_CLASS = 0x2c;  // (44)
constexpr int32_t ALLY_CLASS = 0x27b;  // (635)
constexpr int32_t WRENCH = 0x47;
constexpr uint32_t HERO = 0x0013f350;
constexpr uint32_t HERO_HEAD = 0x0013f420;
constexpr uint32_t HERO_MOBY = 0x001413d0;
constexpr uint32_t HERO_STATE = 0x001413d4;
constexpr uint32_t HERO_DEAD = 0x65;
constexpr uint32_t HERO_ALERT = 0x00141630;
constexpr float DEG = 0.01745329238474369f;  // (0x3c8efa35)

// the level's words (one block): +0 the frame of the last alert, +4 the guards' scale, +8 / +0xc the knock-backs' and the
// dying's gravity, +0x10 .. +0x24 their speeds, +0x28 the shots' speed, +0x2c the guards held
uint32_t words(uint32_t o) { return (on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161388) : L15_ADDR(0x001613e8)) + o; }
// the level's angle tables of the turning animations 10 / 11 / 12 (degrees per animation frame)
uint32_t turn_table(uint32_t set) {
    if (set == 0xa) return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001cc2e0) : L15_ADDR(0x001cc060);
    if (set == 0xb) return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001cc260) : L15_ADDR(0x001cbfe0);
    if (set == 0xc) return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001cc360) : L15_ADDR(0x001cc0e0);
    return 0;
}
// the level's copy of the glows' draw callback
uint32_t glows_draw() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x002a2aa8) : L15_ADDR(0x0029aba8); }

uint32_t first_moby() { return lwu(OV(0x0015ffd8)); }
uint32_t moby_at(int32_t index) { return first_moby() + (static_cast<uint32_t>(index) << 8); }
float frame_dt() { return lwc1(0x0015ed6c); }
float frame_dt_sq() { return lwc1(0x0015ed70); }
bool anim_ended(uint32_t m) { return (lbu(m + 0x70) & 2) != 0; }
// the animation `set` started (over `frames` frames) unless it runs already
void anim_to(uint32_t m, uint32_t set, int32_t frames = 0xa) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}
// the state set, and the animation `set` started unless it runs already
void to_state(uint32_t m, uint32_t state, uint32_t set, int32_t frames = 0xa) {
    sb(m + 0x20, state);
    anim_to(m, set, frames);
}
// the group's list (0x1ab840): the first entry, 0 when none
uint32_t group_list(uint32_t group) { return lwu(OV(0x001ab840) + (group << 2)); }
// its path and the point it walks to
uint32_t path_of(uint32_t pv) { return lwu(OV(0x001b04b0) + (lwu(pv + 0x124) << 2)); }
uint32_t path_point(uint32_t pv) { return path_of(pv) + 0x10 + (lwu(pv + 0x13c) << 4); }
// on to the path's next point (when it stood at one)
void next_point(uint32_t pv) {
    if (lwu(pv + 0x144) == 0) return;
    sw(pv + 0x13c, static_cast<uint32_t>(ee_rem(lw(pv + 0x13c) + 1, lw(path_of(pv)))));
}
// the angle (as the original measures it) from the moby to the point `p`
float facing_to(uint32_t m, uint32_t p) {
    return angle_dist(lwc1(m + 0x48), atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14))));
}

// seeing the hero: alerted, then (first time) the call (state 11, animation 1) and after it `state` / `set`, else straight
void engage(uint32_t m, uint32_t pv, uint32_t state, uint32_t set) {
    sb(m + 0xbc, 1);
    if (lbu(pv + 0x15f) == 0) {
        sb(pv + 0x15f, 1);
        sb(m + 0x20, 0xb);
        sb(pv + 0x15c, state);
        sb(pv + 0x15d, set);
        anim_to(m, 1);
        return;
    }
    to_state(m, state, set);
}

// a class-934 shot from the chain point 1 (`chain` the chain list's one entry, at `list` on the stack) toward its aim point:
// along the joint's own direction when it turns less than 45 degrees from the aim, else along the aim
void shoot(uint32_t m, uint32_t pv, uint32_t sp, uint32_t list, uint32_t chain, bool reversed) {
    const uint32_t along = sp, aim = sp + 0x10, at = sp + 0x20, frame = sp + 0x30;
    sw(list, chain);
    moby_chain_world(m, 1, list, frame);
    moby_joint_positions(m, 1, list, at);
    const float speed = fpu_mul(lwc1(words(0x28)), frame_dt());
    vec3_set_length(V(along), V(frame + 0x10), reversed ? -speed : speed);
    vec3_sub(V(aim), V(pv + 0x100), V(at));
    vec3_set_length(V(aim), V(aim), fpu_mul(lwc1(words(0x28)), frame_dt()));
    const float quarter = u2f(0x3f490fdbu);  // (45 degrees)
    const float yaw = atan2_f(lwc1(along), lwc1(along + 4));
    if (quarter < angle_dist(yaw, atan2_f(lwc1(aim), lwc1(aim + 4)))) {
        class934_shot_spawn(m, along, at);
        return;
    }
    const float pitch = atan2_f(vec2_length(V(along)), lwc1(along + 8));
    if (quarter < angle_dist(pitch, atan2_f(vec2_length(V(aim)), lwc1(aim + 8)))) class934_shot_spawn(m, along, at);
    else class934_shot_spawn(m, aim, at);
}
}  // namespace

// orig: L15:0x002979d8 L17:0x0029f8d8
void class44_update(uint32_t m) {
    if (lwu(words(0x2c)) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (lwu(OV(0x0015f5c4)) == 2) {  // (a scene: hidden)
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        return;
    }
    // the camera in its boxes (+0x2d0 / +0x2d4: out of them; +0x2d8 / +0x2dc: in them, the first tested as box -1, as the
    // original does): it is not seen then
    const uint32_t cam = OV(0x00166dc0);
    uint32_t hidden = 0;
    if (lw(pv + 0x2d0) != -1) {
        hidden = point_in_box(cam, lw(pv + 0x2d0)) == 0 ? 1u : 0u;
        if (lw(pv + 0x2d4) != -1 && point_in_box(cam, lw(pv + 0x2d4)) != 0) hidden = 0;
    } else if (lw(pv + 0x2d8) != -1) {
        hidden = static_cast<uint32_t>(point_in_box(cam, -1));
        if (lw(pv + 0x2dc) != -1) hidden |= static_cast<uint32_t>(point_in_box(cam, lw(pv + 0x2dc)));
    }
    class44_think(m, hidden);
    const uint32_t state0 = lbu(m + 0x20);
    if (hidden != 0 && state0 != 0x15 && state0 != 0x17) {
        sw(m + 0x94, 0);
        sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
        return;
    }
    sh(m + 0x34, (lhu(m + 0x34) & 0xffbeu) | 0x1000u);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    const uint32_t target = lwu(pv + 0x110);
    moby_set_70(pv + 0x1a0, 2.5f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(cam)) < 28.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x16);
    }
    if (lwu(HERO_STATE) == HERO_DEAD) sh(pv + 0x156, 0);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x154)));
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x156)));
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x19c)));
    if (lh(pv + 0x156) != 0 && lw(pv + 0x128) != -1) {  // alerted: its alarm kept up while it is
        const uint32_t alarm = moby_at(lw(pv + 0x128));
        if (lwu(lwu(alarm + 0x78) + 8) != 0) class408_raise(alarm, 0xb4);
    }
    if (lbu(m + 0xbc) == 0) sb(pv + 0x15f, 0);

    const uint32_t sp = ee_stack_alloc(0x100);
    // the ally to look at, found now and then (calling, or one time in 150 frames' worth)
    auto look_for_ally = [m, pv](bool& turned) {
        turned = false;
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x196))) == 0) return;
        const uint32_t ally = class44_find_ally(m);
        sw(pv + 0x190, ally);
        if (ally == 0) return;
        if (lh(pv + 0x194) == 0 && rand_mod(frame_scale_i(0x96)) != 0) return;
        class44_turn_toward(m, pv, lwu(pv + 0x190) + 0x10, 0xa, 0xf);
        turned = true;
    };
    // its alarm (when down) raised for `frames`, or lowered at once (-1)
    auto alarm_raise = [pv](int32_t frames) {
        if (lw(pv + 0x128) == -1) return;
        const uint32_t alarm = moby_at(lw(pv + 0x128));
        if (lwu(lwu(alarm + 0x78) + 8) != 0) class408_raise(alarm, frames);
    };
    // back to its watch: state 9 (animation 13), its alarm raised a while
    auto back_to_watch = [m, alarm_raise] {
        to_state(m, 9, 0xd);
        alarm_raise(frame_scale_i(0x1e));
    };
    // the pause at a point of its path ends: the ally's call, the hero, or on along the path
    auto at_point = [m, pv](uint32_t after_state, uint32_t after_set) {
        if (lw(pv + 0x124) == -1) return;
        next_point(pv);
        sw(pv + 0x144, 0);
        sw(pv + 0x14c, static_cast<uint32_t>(frame_scale_i(0x3c)));
        engage(m, pv, after_state, after_set);
    };
    // the shot states' common end: the hero dead, the alarm, the turn to its target, the next state
    auto shot_end = [m, pv](uint32_t wide_state, uint32_t wide_set, bool lost_resets) {
        if (!anim_ended(m)) return;
        const auto settle = [m, pv] {
            to_state(m, 0xc, 0);
            sb(pv + 0x19e, 0);
        };
        if (lwu(HERO_STATE) == HERO_DEAD) {
            settle();
            return;
        }
        const int32_t a = lw(pv + 0x128);
        if (a != -1 && lwu(lwu(moby_at(a) + 0x78) + 8) == 0 && class44_group_calm(m) != 0) {
            to_state(m, 0xe, 0xe, 0x14);
            sb(pv + 0x19e, 0);
            return;
        }
        if (u2f(0x3f490fdbu) < facing_to(m, pv + 0xd0)) {
            class44_turn_toward(m, pv, pv + 0xd0, wide_state, wide_set);
            sb(pv + 0x19e, 0);
            return;
        }
        if (lost_resets) {  // (state 19: lost, settle; else state 16, animation 3)
            if (lwu(pv + 0x114) == 2) settle();
            else {
                to_state(m, 0x10, 3);
                sb(pv + 0x19e, 0);
            }
        } else {  // (state 17: seen, state 18, animation 5; lost, settle)
            if (lwu(pv + 0x114) != 2) {
                to_state(m, 0x12, 5);
                sb(pv + 0x19e, 0);
            } else {
                settle();
            }
        }
    };
    // the shot states' start: one shot, once, from 0.85 to 3 frames into the animation
    auto shot_fire = [m, pv, sp](uint32_t chain, uint32_t list_at, bool reversed) {
        sh(pv + 0x19c, 0);
        sh(pv + 0x194, 0);
        class44_alert_group(m);
        if (lbu(pv + 0x19e) != 0) return;
        const float t = moby_anim_time(m);
        if (!(0.85f <= t) || !(t < 3.0f)) return;
        shoot(m, pv, sp, sp + list_at, chain, reversed);
        sb(pv + 0x19e, 1);
    };
    // the guard dies: its class-78 mobys broken, its sound, a burst, its bolts
    auto die = [m, pv] {
        if (lw(pv + 0x12c) != -1) {
            class78_break(moby_at(lw(pv + 0x12c)));
            sw(pv + 0x12c, 0xffffffffu);
        }
        if (lw(pv + 0x150) != -1) {
            class78_break(moby_at(lw(pv + 0x150)));
            sw(pv + 0x150, 0xffffffffu);
        }
        moby_sound_start(0xd, 0, m);
        explosion_burst(m, m + 0x10, -1, 0.5f, 13.0f);
        moby_drop_bolts(m, 0, -1);
        moby_retire(m);
    };

    enum class End { Common, Return };
    End end = End::Common;
    switch (lbu(m + 0x20)) {  // (the table L15:0x1e2560)
    case 0: {  // (0x00297bf8) the start: on its home path, else gone
        const int32_t home = lw(pv + 0x120);
        if (home == -1 || lwu(lwu(OV(0x001b04b0) + (static_cast<uint32_t>(home) << 2))) == 0) {
            moby_retire(m);
            end = End::Return;
            break;
        }
        sh(pv + 0x24, 3);
        swc1(pv + 0x20, 3.0f);
        sb(pv + 0x28, 2);
        sb(pv + 0x29, 0);
        lq_sq(pv + 0x160, m + 0x10);
        const float facing = lwc1(m + 0x48);
        if (lw(pv + 0x148) < 5) sw(pv + 0x148, 5);
        swc1(pv + 0x16c, facing);
        sw(pv + 0x144, 1);
        sb(m + 0x20, 2);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        for (uint32_t k = 0; k < 4; ++k)  // (waiting for its mobys: state 1)
            if (lw(pv + 0x180 + k * 4) != -1) {
                sb(m + 0x20, 1);
                break;
            }
        sb(m + 0xbc, 0);
        break;
    }
    case 1:  // (0x00297ccc) waiting until one of its mobys wakes
        for (uint32_t k = 0; k < 4; ++k)
            if (!(lbu(moby_at(lw(pv + 0x180 + k * 4)) + 0x20) < 2)) {
                sb(m + 0x20, 2);
                break;
            }
        break;
    case 2:
    case 3: {  // (0x00297d30) on its path: stopped (2) or waiting at a point (3)
        if (lwu(pv + 0x114) != 2) {
            engage(m, pv, 0x10, 3);
            break;
        }
        if (lh(pv + 0x156) != 0 && lbu(pv + 0x15b) == 0) {  // alerted by another: to the hero
            if (lbu(pv + 0x15a) != 0 || lw(pv + 0x124) == -1) {
                class44_turn_toward(m, pv, target + 0x10, 0x10, 3);
                break;
            }
            next_point(pv);
            sw(pv + 0x144, 0);
            engage(m, pv, 0xd, 0xe);
            break;
        }
        if (lh(pv + 0x19c) != 0) {  // a guard to look at
            class44_turn_toward(m, pv, lwu(pv + 0x198) + 0x10, 0xa, 0xf);
            break;
        }
        bool turned;
        look_for_ally(turned);
        if (turned) break;
        if (lbu(m + 0x20) == 2) {
            if (lw(pv + 0x124) != -1) {  // on: a pause at the point (unless a guard ahead of it pauses)
                if (lwu(pv + 0x144) != 0 && class44_path_first(m) == 0) break;
                sw(pv + 0x14c, static_cast<uint32_t>(frame_scale_i(lw(pv + 0x148))));
                sb(m + 0x20, 3);
                break;
            }
            // no path: home, watching; a look about now and then
            if (lh(pv + 0x158) != 0 && lwu(HERO_STATE) == 0x59) {
                if (lw(pv + 0x150) == -1 && lw(pv + 0x12c) == -1 && (lh(pv + 0x154) != 0 || rand_mod(0x78) != 0)) break;
                sb(pv + 0x15d, 0);
                sb(pv + 0x15c, 2);
                to_state(m, 5, 2);
                break;
            }
            class44_turn_to(m, pv, 2, 0, lwc1(pv + 0x16c));
            break;
        }
        if (lbu(m + 0x20) != 3) break;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x14c))) == 0) break;
        next_point(pv);
        sw(pv + 0x144, 0);
        class44_turn_toward(m, pv, path_point(pv), 4, 0xd);
        break;
    }
    case 4: {  // (0x00298378) walking to its point
        if (class44_walk_to(m, path_point(pv), fpu_add(frame_dt(), frame_dt())) != 0) {
            sb(m + 0x20, 2);
            sw(pv + 0x144, 1);
            anim_to(m, 0);
            break;
        }
        if (lwu(pv + 0x114) != 2) {
            engage(m, pv, 0x10, 3);
            break;
        }
        if (lh(pv + 0x156) != 0 && lbu(pv + 0x15b) == 0) {
            if (lbu(pv + 0x15a) != 0) {
                class44_turn_toward(m, pv, target + 0x10, 0x10, 3);
                break;
            }
            sw(pv + 0x14c, static_cast<uint32_t>(frame_scale_i(0x3c)));
            engage(m, pv, 0xd, 0xe);
            break;
        }
        if (lh(pv + 0x19c) != 0) {
            class44_turn_toward(m, pv, lwu(pv + 0x198) + 0x10, 0xa, 0xf);
            break;
        }
        if (lh(pv + 0x158) != 0 && lwu(HERO_STATE) == 0x59) {
            if (lw(pv + 0x150) == -1 && lw(pv + 0x12c) == -1 && lh(pv + 0x154) != 0) break;
            const uint32_t s = lbu(m + 0x20);
            sb(pv + 0x15d, 0xd);
            sb(pv + 0x15c, s);
            to_state(m, 5, 2);
            break;
        }
        bool turned;
        look_for_ally(turned);
        break;
    }
    case 5: {  // (0x002980c8) a look about: its class-78 mobys broken, then back
        if (!anim_ended(m)) break;
        sh(pv + 0x154, static_cast<uint32_t>(frame_scale_i(0xf0)));
        if (lw(pv + 0x150) != -1) {
            class78_break(moby_at(lw(pv + 0x150)));
            sw(pv + 0x150, 0xffffffffu);
        }
        if (lw(pv + 0x12c) != -1) {
            class78_break(moby_at(lw(pv + 0x12c)));
            sw(pv + 0x12c, 0xffffffffu);
        }
        const uint32_t after = lbu(pv + 0x15c);
        sb(m + 0x20, after);
        if (after == 2) anim_to(m, 0);
        else if (after == 4) anim_to(m, 0xd);
        break;
    }
    case 6:
    case 7:
    case 8: {  // (0x00298190) turning (left, right, about): its facing from the animation's angle table
        if (lwu(pv + 0x114) != 2 && facing_to(m, pv + 0xd0) < u2f(0x3f060a92u)) {
            engage(m, pv, 0x10, 3);
            break;
        }
        const uint32_t set = lbu(m + 0x52);
        const uint32_t table = turn_table(set);
        if (anim_ended(m)) {
            const int32_t last = float_to_int(class_anim_last_time(lbu(m + 0x22), set));
            if (table != 0)
                swc1(m + 0x48, angle_add(lwc1(pv + 0x170), fpu_mul(lwc1(table + (static_cast<uint32_t>(last) << 2)), DEG)));
            to_state(m, lbu(pv + 0x15c), lbu(pv + 0x15d));
            break;
        }
        if (lbu(m + 0x52) != lbu(m + 0x53) || table == 0) break;
        const float t = moby_anim_time(m);
        const int32_t i = float_to_int(t);
        const float frac = fpu_sub(t, int_to_float(i));
        const uint32_t row = table + (static_cast<uint32_t>(i) << 2);
        const float a = lwc1(row), b = lwc1(row + 4);
        swc1(m + 0x48, angle_add(lwc1(pv + 0x170), fpu_mul(fpu_add(a, fpu_mul(fpu_sub(b, a), frac)), DEG)));
        break;
    }
    case 9: {  // (0x002986b0) back home
        if (0.2f < vec3_distance(V(m + 0x10), V(pv + 0x160))) {
            class44_walk_to(m, pv + 0x160, fpu_add(frame_dt(), frame_dt()));
            break;
        }
        if (lw(pv + 0x124) == -1) {
            class44_turn_to(m, pv, 2, 0, lwc1(pv + 0x16c));
            break;
        }
        sb(m + 0xbc, 0);
        sw(pv + 0x144, 0);
        class44_turn_toward(m, pv, path_point(pv), 2, 0);
        break;
    }
    case 10: {  // (0x0029801c) an ally's call heard
        if (lwu(pv + 0x114) != 2) {
            engage(m, pv, 0x10, 3);
            break;
        }
        if (!anim_ended(m)) break;
        sb(m + 0x20, 2);
        anim_to(m, 0);
        sh(pv + 0x196, static_cast<uint32_t>(frame_scale_i(0x168)));
        sh(pv + 0x194, 0);
        break;
    }
    case 11: {  // (0x00298898) the call: its alarm woken, its group alerted
        if (lw(pv + 0x128) != -1) class408_wake(moby_at(lw(pv + 0x128)));
        if (!anim_ended(m)) break;
        if (lwu(HERO_STATE) == HERO_DEAD) {
            to_state(m, 0xc, 0);
            break;
        }
        class44_alert_group(m);
        const int32_t a = lw(pv + 0x128);
        if (a != -1 && lwu(lwu(moby_at(a) + 0x78) + 8) == 0 && class44_group_calm(m) != 0) {
            to_state(m, 0xe, 0xe, 0x14);  // (to its alarm)
            break;
        }
        const uint32_t after = lbu(pv + 0x15c);
        if (after == 0x10) {
            class44_turn_toward(m, pv, target + 0x10, 0x10, 3);
            break;
        }
        if (after != 0xff) sb(m + 0x20, after);
        const uint32_t set = lbu(pv + 0x15d);
        if (set != 0xff) anim_to(m, set);
        break;
    }
    case 12: {  // (0x002991c8) watching: the hero seen, lost, or alerted by another
        if (lwu(HERO_STATE) == HERO_DEAD) {
            to_state(m, 9, 0xd);
            alarm_raise(-1);
            break;
        }
        const uint32_t view = lwu(pv + 0x114);
        if (view != 2 && lbu(pv + 0x15b) == 0) {
            class44_turn_toward(m, pv, pv + 0xd0, 0x10, 3);
            break;
        }
        if (lh(pv + 0x156) == 0) {
            back_to_watch();
            break;
        }
        if (lbu(pv + 0x15a) != 0) {
            if (view == 2 || lbu(pv + 0x15b) != 0) class44_turn_toward(m, pv, target + 0x10, 0xc, 0);
            else class44_turn_toward(m, pv, target + 0x10, 0x10, 3);
            break;
        }
        at_point(0xd, 0xe);
        break;
    }
    case 13: {  // (0x002985ac) running along its path to the hero
        if (class44_walk_to(m, path_point(pv), fpu_mul(frame_dt(), 3.5f)) != 0) {
            sb(m + 0x20, 0xc);
            sw(pv + 0x144, 1);
            anim_to(m, 0);
            break;
        }
        if (lwu(pv + 0x114) != 2 && lbu(pv + 0x15b) == 0) {
            engage(m, pv, 0x10, 3);
            break;
        }
        if (lh(pv + 0x156) == 0) {
            back_to_watch();
            break;
        }
        sw(pv + 0x14c, static_cast<uint32_t>(frame_scale_i(0x3c)));
        break;
    }
    case 14: {  // (0x002989a8) running to its alarm, to 1.5 from it
        const uint32_t alarm = moby_at(lw(pv + 0x128));
        class408_wake(alarm);
        const uint32_t to = sp;
        vec3_sub(V(to), V(moby_at(lw(pv + 0x128)) + 0x10), V(m + 0x10));
        vec3_set_length(V(to), V(to), fpu_sub(vec3_length(V(to)), 1.5f));
        vec3_add(V(to), V(to), V(m + 0x10));
        if (class44_walk_to(m, to, fpu_mul(frame_dt(), 4.0f)) != 0) to_state(m, 0xf, 9);
        break;
    }
    case 15: {  // (0x00298a68) at its alarm: raising it
        if (lbu(m + 0x52) == lbu(m + 0x53) && 12.0f <= moby_anim_time(m)) class408_raise(moby_at(lw(pv + 0x128)), 0xb4);
        if (!anim_ended(m)) break;
        class408_raise(moby_at(lw(pv + 0x128)), 0xb4);
        class44_turn_toward(m, pv, target + 0x10, 0x14, 0xe);
        break;
    }
    case 16:
    case 18: {  // (0x00298b84 / 0x00298afc) aiming: then shooting (17 / 19)
        sh(pv + 0x19c, 0);
        sh(pv + 0x194, 0);
        class44_alert_group(m);
        if (!anim_ended(m)) break;
        if (lwu(HERO_STATE) == HERO_DEAD) {
            to_state(m, 0xc, 0);
            break;
        }
        const bool low = lbu(m + 0x20) == 0x10;
        sb(m + 0x20, low ? 0x11u : 0x13u);
        sb(pv + 0x19e, 0);
        anim_to(m, low ? 4u : 6u);
        break;
    }
    case 17:  // (0x00298ec8) shooting (low)
        shot_fire(3, 0x74, true);
        shot_end(0x12, 5, false);
        break;
    case 19:  // (0x00298c0c) shooting (high)
        shot_fire(2, 0x70, false);
        shot_end(0x10, 3, true);
        break;
    case 20: {  // (0x00298750) back home after a chase
        if (0.2f < vec3_distance(V(m + 0x10), V(pv + 0x160))) {
            class44_walk_to(m, pv + 0x160, fpu_mul(frame_dt(), 3.0f));
            break;
        }
        if (lh(pv + 0x156) == 0) {
            sb(m + 0xbc, 0);
            sw(pv + 0x144, 0);
            if (lw(pv + 0x124) == -1) class44_turn_to(m, pv, 2, 0, lwc1(pv + 0x16c));
            else class44_turn_toward(m, pv, path_point(pv), 2, 0);
            break;
        }
        if (u2f(0x3f490fdbu) < facing_to(m, pv + 0xd0) && lbu(pv + 0x15b) == 0) {
            class44_turn_toward(m, pv, pv + 0xd0, 0x10, 3);
            break;
        }
        to_state(m, 0xc, 0);
        break;
    }
    case 21: {  // (0x00299428) knocked back: landing (back to it), off the ground (dies)
        const uint32_t r = enemy_physics(m, pv + 0x70);
        if ((r & 0x40) == 0) {
            if ((r & 0x120) != 0) {
                moby_retire(m);
                end = End::Return;
            }
            break;
        }
        floor_probe_v(0.5f, V(m + 0x10), 0);
        if (sub_001f35d8() == -1 && fpu_sub(lwc1(pv + 0x168), 5.0f) < lwc1(m + 0x18)) {
            end = End::Return;
            if (lwu(HERO_STATE) == HERO_DEAD) {
                to_state(m, 0xc, 0);
                break;
            }
            sb(m + 0xbc, 1);
            if (lbu(pv + 0x15f) != 0) {
                to_state(m, 0x10, 3);
                break;
            }
            sb(pv + 0x15f, 1);
            sb(m + 0x20, 0xb);
            sb(pv + 0x15c, 0x10);
            sb(pv + 0x15d, 3);
            anim_to(m, 1);
            break;
        }
        die();
        end = End::Return;
        break;
    }
    case 23: {  // (0x002995cc) dying: gone on landing or off the ground
        if ((enemy_physics(m, pv + 0x70) & 0x160) == 0) break;
        die();
        end = End::Return;
        break;
    }
    default:  // (22, and past the table)
        break;
    }
    if (end == End::Return) {
        ee_stack_free(0x100);
        return;
    }

    // the frame's end: its look, the hero's alert of the frame (the highest of the guards'), its glow's colour pulsing faster
    // as it is more alert, its glows' points and their draw
    class44_aim(m);
    int32_t alert = 2;
    if (lh(pv + 0x156) == 0 && lwu(pv + 0x114) == 2) alert = 0 < lh(pv + 0x158) ? 1 : 0;
    const uint32_t frame = lwu(OV(0x0015f5cc));
    if (frame != lwu(words(0)) || lh(HERO_ALERT) < alert) {
        sw(words(0), frame);
        sh(HERO_ALERT, static_cast<uint32_t>(alert));
    }
    int32_t period = frame_scale_i(0xaa);
    if (alert == 2) period = frame_scale_i(0x32);
    else if (alert == 1) period = frame_scale_i(0x5a);
    const float span = int_to_float(period);
    const int32_t phase = ee_rem(lw(OV(0x0015f5cc)), period);
    const float pi = u2f(0x40490fdbu);
    float a = fpu_div(static_cast<float>(phase), span);
    a = fpu_mul(fpu_add(a, a), pi);
    const float wave = sin_vu(fpu_add(a, u2f(0xc0490fdbu)));
    const int32_t r20 = float_to_int(fpu_mul(wave, 20.0f));
    int32_t g = float_to_int(fpu_mul(wave, 70.0f)) + 0xb4;
    const int32_t b = float_to_int(fpu_mul(wave, 10.0f)) + 0x14;
    int32_t r = r20 + 0x32;
    if (!(g < 256)) g = 0xff;
    if (!(r < 256)) r = 0xff;
    uint32_t colour;
    if (alert == 2)
        colour = (0x80u << 24) | (static_cast<uint32_t>(b) << 16) |
                 (static_cast<uint32_t>((r + static_cast<int32_t>(static_cast<uint32_t>(r) >> 31)) >> 1) << 8) | static_cast<uint32_t>(g);
    else
        colour = (0x80u << 24) | (static_cast<uint32_t>(b) << 16) | (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(r);
    sw(m + 0x90, rgba_lerp(lwu(m + 0x90), colour, 0.1f));
    for (uint32_t k = 0; k < 3; ++k) moby_chain_point_v(m, k + 4, V(pv + 0x2a0 + k * 0x10));
    sub_001f93e0(glows_draw(), m);
    ee_stack_free(0x100);
}

// orig: L15:0x0029a570 L17:0x002a2470
void class44_think(uint32_t m, uint32_t quiet) {
    if (lbu(m + 0x20) == 0) return;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(words(4))));
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    int32_t st = 0;
    float dmg = 0.0f;
    int32_t react = moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
    if (st != 1 && lbu(m + 0x20) != 0x17) {  // hit
        if (lbu(0x001413f4) == 0) class44_alert_group(m);
        const float health = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, health);
        const bool wrench = lh(lwu(hit + 0x20) + 0xa6) == WRENCH;
        if (health <= 0.0f) react = 1;
        else if (wrench) react = 3;
        sw(pv + 0x90, static_cast<uint32_t>(float_to_int(512.0f)));
        swc1(pv + 0x98, 0.5f);
        sw(pv + 0x94, 9);
        swc1(pv + 0x80, fpu_mul(lwc1(words(8)), frame_dt_sq()));
        sb(pv + 0xad, 0);
        // the reaction (the table L15:0x1e25c0): 1, 2 dying; 3 .. 8 knocked back; 9, 10 a flash
        const uint32_t kind = static_cast<uint32_t>(react);
        if (kind >= 1 && kind <= 8) {
            const bool dying = kind <= 2;
            const float dt = frame_dt(), dt2 = frame_dt_sq();
            if (dying) {  // (0x0029a794)
                const float g = fpu_mul(lwc1(words(0xc)), dt2);
                const float s1 = fpu_mul(lwc1(words(0x20)), dt);
                const float s2 = fpu_mul(lwc1(words(0x1c)), dt);
                const float s3 = fpu_mul(lwc1(words(0x24)), dt);
                sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
                swc1(pv + 0x80, g);
                swc1(pv + 0x88, s1);
                swc1(pv + 0x8c, s2);
                swc1(pv + 0x84, s3);
                swc1(pv + 0xc0, 8.0f);
                swc1(pv + 0xc4, 16.0f);
            } else {  // (0x0029a6cc)
                const float g = fpu_mul(lwc1(words(8)), dt2);
                const float s1 = fpu_mul(lwc1(words(0x14)), dt);
                const float s2 = fpu_mul(lwc1(words(0x10)), dt);
                const float s3 = fpu_mul(lwc1(words(0x18)), dt);
                swc1(pv + 0x80, g);
                swc1(pv + 0x88, s1);
                swc1(pv + 0x8c, s2);
                swc1(pv + 0xc0, 5.0f);
                swc1(pv + 0xc4, 10.0f);
                swc1(pv + 0x84, s3);
            }
            const uint32_t by = lwu(hit + 0x20);
            const uint32_t yaw = sp + (dying ? 0x1c : 0x18);
            const float away = atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(by + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(by + 0x14)));
            swc1(yaw, away);
            lq_sq(sp, hit + 0x10);
            vec_yaw_marked_v(V(sp), *static_cast<float*>(ee_ram_ptr(yaw)), *static_cast<float*>(ee_ram_ptr(pv + 0x88)),
                             *static_cast<float*>(ee_ram_ptr(pv + 0x8c)));
            move_state_start(m, pv + 0x70, dying ? 8 : 7, 1, 0, lwc1(yaw));
            sb(m + 0x20, dying ? 0x17u : 0x15u);
            sb(m + 0xbc, 2);
            sb(pv + 0x67, dying ? 0xf0u : 0x78u);
        } else if (kind == 9 || kind == 10) {  // (0x0029a6c0)
            sb(pv + 0x67, 0xfa);
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    color_pulse_update(m, pv + 0x60);
    if (quiet != 0) {
        ee_stack_free(0x90);
        return;
    }
    // its watch time, dealt again when asked
    if (lwu(pv + 0x38) != 0) {
        const float t = static_cast<float>(float_to_int(frame_scale_f(rand_range_f(180.0f, 240.0f))));
        sw(pv + 0x38, 0);
        swc1(pv + 0x130, t);
    }
    float_timer_tick(pv + 0x130);
    // the hero's head in sight (within 64, nothing between but its class-78 moby)
    if (vec3_distance(V(m + 0x10), V(HERO_HEAD)) < 64.0f) {
        lq_sq(sp, m + 0x10);
        swc1(sp + 8, fpu_add(lwc1(sp + 8), 1.0f));
        const uint32_t ignore = lw(pv + 0x12c) == -1 ? 0u : moby_at(lw(pv + 0x12c));
        sb(pv + 0x15a, segment_collide_v(V(sp), V(HERO_HEAD), 2, ignore, nullptr) == 0 ? 1u : 0u);
    } else {
        sb(pv + 0x15a, 0);
    }
    // its target along its home path's polygon
    const uint32_t poly = lwu(OV(0x001b04b0) + (lwu(pv + 0x120) << 2));
    if (target_select(m, pv + 0xd0, 0, 0, poly + 0x10, lw(poly), 128.0f) == 2) {
        sb(pv + 0x19f, 0);
        sb(pv + 0x15b, 1);
        sh(pv + 0x158, 0);
    } else {
        uint32_t seen = 0;
        bool front = facing_to(m, pv + 0xd0) < u2f(0x3fc90fdbu);  // (within 90 degrees)
        if (front && lh(0x0013f658) != 0 && !(abs_f(fpu_sub(lwc1(m + 0x18), lwc1(pv + 0xd8))) < 4.0f)) front = false;
        if (front || lbu(m + 0x20) == 0x10 || lbu(m + 0x20) == 0x11) seen = lbu(pv + 0x15a);
        if (lwu(pv + 0x114) == 1) {
            sb(pv + 0x15e, lbu(pv + 0x15b) + 1);
        } else if (lbu(0x001413f4) == 3) {
            if (!(lbu(pv + 0x15e) < 2)) sb(pv + 0x15b, 1);
        } else {
            sb(pv + 0x15e, 0);
        }
        sb(pv + 0x19f, 1);
        if (seen == 0) {  // not in sight: lost
            if (lbu(pv + 0x15a) == 0) sb(pv + 0x15b, 1);
            sb(pv + 0x19f, 0);
            sw(pv + 0x114, 2);
            sh(pv + 0x158, 0);
        } else {
            bool lose;
            if (lh(pv + 0x156) != 0 && lbu(pv + 0x15b) == 0) {
                lose = lbu(pv + 0x15e) != 0 && lwu(pv + 0x114) != 1;
            } else if (lbu(0x001413f4) == 3) {
                lose = true;
            } else {
                lose = lbu(pv + 0x15e) != 0 && lwu(pv + 0x114) != 1;
            }
            if (lose) {
                sw(pv + 0x114, 2);
                sh(pv + 0x158, 1);
            } else if (lbu(0x001413f4) != 3 && lwu(pv + 0x114) != 2) {
                sb(pv + 0x15b, 0);
            }
        }
        if (lwu(OV(0x0015f5c4)) == 2) sw(pv + 0x114, 2);
    }
    if (lwu(pv + 0x110) == 0) sw(pv + 0x110, lwu(HERO_MOBY));
    ee_stack_free(0x90);
}

// orig: L15:0x0029a2a8 L17:0x002a21a8
void class44_aim(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x90);
    const uint32_t at = sp, from = sp + 0x10, rel = sp + 0x20;
    uint32_t src = 0;
    if (lh(pv + 0x194) != 0 && lwu(pv + 0x190) != 0) src = lwu(pv + 0x190) + 0x10;
    else if (lh(pv + 0x19c) != 0 && lwu(pv + 0x198) != 0) src = lwu(pv + 0x198) + 0x10;
    else if (lbu(pv + 0x19f) != 0) src = pv + 0x100;
    float yaw = 0.0f, pitch = 0.0f;
    const bool aiming = (lbu(m + 0x20) - 0x10u) < 2u;  // (states 16, 17)
    if (src != 0) {
        lq_sq(at, src);
        lq_sq(from, m + 0x10);
        swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
        vec3_sub(V(rel), V(at), V(from));
        yaw = angle_sub(atan2_f(lwc1(rel), lwc1(rel + 4)), lwc1(m + 0x48));
        pitch = -atan2_f(vec2_length(V(rel)), lwc1(rel + 8));
        const float lim = aiming ? u2f(0x3fc90fdbu) : u2f(0x3f860a92u);  // (90 / 60 degrees)
        if (lim < yaw) yaw = lim;
        else if (yaw < -lim) yaw = -lim;
        const float tilt = u2f(0x3f060a92u);  // (30 degrees)
        if (tilt < pitch) pitch = tilt;
        else if (pitch < -tilt) pitch = -tilt;
    }
    if ((lbu(m + 0x20) - 0x10u) < 2u) {  // the body takes up to 60 degrees, the head the rest
        const float lim = u2f(0x3f860a92u);
        float body = yaw;
        if (lim < yaw) body = lim;
        else if (yaw < -lim) body = -lim;
        swc1(pv + 0x288, body);
        swc1(pv + 0x208, fpu_sub(yaw, lwc1(pv + 0x288)));
    } else {
        swc1(pv + 0x208, fpu_mul(yaw, u2f(0x3f333333u)));
        swc1(pv + 0x288, fpu_mul(yaw, u2f(0x3e99999au)));
    }
    const float dt = lwc1(0x0015ed64);
    swc1(pv + 0x204, pitch);
    joint_node_spring(m, pv + 0x1a0, 1, fpu_mul(u2f(0x3ca3d70au), dt), fpu_mul(u2f(0x3e99999au), dt));
    joint_node_spring(m, pv + 0x220, 0, fpu_mul(u2f(0x3ca3d70au), lwc1(0x0015ed64)), fpu_mul(u2f(0x3e99999au), lwc1(0x0015ed64)));
    ee_stack_free(0x90);
}

// orig: L15:0x00299dd8 L17:0x002a1cd8
uint32_t class44_find_ally(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const int32_t group = lw(pv + 0x17c);
    if (group == -1) return 0;
    uint32_t e = group_list(static_cast<uint32_t>(group));
    if (e == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t from = sp, to = sp + 0x10;
    lq_sq(from, m + 0x10);
    swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
    float best_d = 0.0f;
    uint32_t best = 0;
    for (;; e += 2) {
        const uint32_t idx = lhu(e) & 0x7fffu;
        const uint32_t o = first_moby() + (idx << 8);
        const uint32_t st = lbu(o + 0x20);
        if (lh(o + 0xa6) == ALLY_CLASS && !(st < 6) && st < 16) {
            const float ang = angle_dist(lwc1(m + 0x48), atan2_f(fpu_sub(lwc1(o + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(o + 0x14), lwc1(m + 0x14))));
            const uint32_t now = lbu(first_moby() + (idx << 8) + 0x20);
            if (now == 7 || now == 0xf) sh(pv + 0x194, 1);  // (it calls)
            if (lh(pv + 0x194) != 0 || ang < u2f(0x3f9c61aau)) {  // (within 70 degrees)
                lq_sq(to, first_moby() + (idx << 8) + 0x10);
                swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));  // (the original raises its own point again each time)
                if (segment_collide_v(V(from), V(to), 2, 0, nullptr) == 0) {
                    const float d = vec3_distance(V(from), V(to));
                    if (best == 0 || d < best_d) {
                        best_d = d;
                        best = first_moby() + (idx << 8);
                    }
                }
            }
        }
        if (lh(e) < 0) break;
    }
    ee_stack_free(0xa0);
    if (lh(pv + 0x194) != 0) return best;
    if (8.0f < best_d) best = 0;
    return best;
}

// orig: L15:0x00299880 L17:0x002a1780
uint32_t class44_turn_to(uint32_t m, uint32_t pv, uint32_t state, uint32_t set, float angle) {
    const float turn = -angle_sub(lwc1(m + 0x48), angle);
    sw(pv + 0x144, 0);
    swc1(pv + 0x174, angle);
    swc1(pv + 0x170, lwc1(m + 0x48));
    swc1(pv + 0x178, turn);
    sb(pv + 0x15c, state);
    sb(pv + 0x15d, set);
    const float third = u2f(0x40060a92u);  // (120 degrees)
    if (third < turn || turn < -third) {  // about (state 8, animation 12)
        sb(m + 0x20, 8);
        if (0.0f < lwc1(pv + 0x178)) swc1(pv + 0x178, fpu_add(lwc1(pv + 0x178), u2f(0xc0c90fdbu)));
        anim_to(m, 0xc);
        return 1;
    }
    const float eighth = u2f(0x3f490fdbu);  // (45 degrees)
    if (turn < -eighth) {
        to_state(m, 6, 0xa);
        return 1;
    }
    if (eighth < turn) {
        to_state(m, 7, 0xb);
        return 1;
    }
    to_state(m, state, set);
    return 0;
}

// orig: L15:0x00299a68 L17:0x002a1968
uint32_t class44_turn_toward(uint32_t m, uint32_t pv, uint32_t point, uint32_t state, uint32_t set) {
    const float angle = atan2_f(fpu_sub(lwc1(point), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14)));
    return class44_turn_to(m, pv, state, set, angle);
}

// orig: L15:0x00299ae0 L17:0x002a19e0
uint32_t class44_walk_to(uint32_t m, uint32_t point, float speed) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x40);
    const float heading = atan2_f(fpu_sub(lwc1(point), lwc1(m + 0x10)), fpu_sub(lwc1(point + 4), lwc1(m + 0x14)));
    const float accel = fpu_mul(frame_dt_sq(), u2f(0x41490fdbu));  // (4 pi)
    yaw_seek(m + 0x48, pv + 0x138, heading, accel, accel, fpu_mul(frame_dt(), u2f(0x40df66f3u)));  // (400 degrees a second)
    const float rate = fpu_mul(frame_dt_sq(), 9.0f);
    if (facing_to(m, point) < u2f(0x3f490fdbu)) approach_f(pv + 0x134, speed, rate);
    else approach_f(pv + 0x134, 0.0f, rate);
    swc1(sp, fpu_mul(cos_vu(lwc1(m + 0x48)), lwc1(pv + 0x134)));
    const float y = fpu_mul(sin_vu(lwc1(m + 0x48)), lwc1(pv + 0x134));
    const float fall = -fpu_mul(frame_dt(), 0.5f);
    swc1(sp + 4, y);
    swc1(sp + 8, fall);
    walker_move(m, sp, 0, 0.5f, 0.5f, 0.0f);
    ee_stack_free(0x40);
    return vec2_distance(V(m + 0x10), V(point)) < 0.2f ? 1u : 0u;
}

// orig: L15:0x0029a0d0 L17:0x002a1fd0
void class44_alert_group(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lwu(HERO_STATE) == HERO_DEAD) {
        sh(pv + 0x156, 0);
        return;
    }
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) {
        sh(pv + 0x156, static_cast<uint32_t>(frame_scale_i(0xb4)));
        return;
    }
    uint32_t e = group_list(group);
    if (e == 0) return;
    const uint32_t sp = ee_stack_alloc(0xa0);
    const uint32_t from = sp, to = sp + 0x10;
    lq_sq(from, m + 0x10);
    swc1(from + 8, fpu_add(lwc1(from + 8), 1.0f));
    for (;; e += 2) {
        const uint32_t idx = lhu(e) & 0x7fffu;
        const uint32_t o = first_moby() + (idx << 8);
        if (static_cast<int8_t>(lbu(o + 0x20)) >= 0 && lh(o + 0xa6) == GUARD_CLASS) {
            lq_sq(to, o + 0x10);
            swc1(to + 8, fpu_add(lwc1(to + 8), 1.0f));
            if (segment_collide_v(V(to), V(from), 2, 0, nullptr) == 0) {
                const uint32_t opv = lwu(first_moby() + (idx << 8) + 0x78);
                const int32_t t = frame_scale_i(0x12c);
                sb(opv + 0x15b, 0);
                sh(opv + 0x156, static_cast<uint32_t>(t));
                class44_look_at(m, first_moby() + (idx << 8));
            }
        }
        if (lh(e) < 0) break;
    }
    ee_stack_free(0xa0);
}

// orig: L15:0x00299ca0 L17:0x002a1ba0
uint32_t class44_group_calm(uint32_t m) {
    const int32_t own = lw(lwu(m + 0x78) + 0x128);
    if (own == -1) return 0;
    const uint32_t alarm = moby_at(own);
    if (lwu(lwu(alarm + 0x78) + 8) != 0) return 0;
    if (lbu(m + 0x21) == 0xff) return 1;
    const float near = vec2_distance(V(m + 0x10), V(alarm + 0x10));
    for (uint32_t e = group_list(lbu(m + 0x21)); e != 0; e += 2) {
        const uint32_t o = first_moby() + ((lhu(e) & 0x7fffu) << 8);
        if (lbu(o + 0x20) == 0xe) return 0;
        if (lbu(o + 0xbc) == 1) {
            const int32_t theirs = lw(lwu(o + 0x78) + 0x128);
            if (theirs != -1 && vec2_distance(V(o + 0x10), V(first_moby() + (static_cast<uint32_t>(theirs) << 8) + 0x10)) < near) return 0;
        }
        if (lh(e) < 0) break;
    }
    return 1;
}

// orig: L15:0x00299ff8 L17:0x002a1ef8
uint32_t class44_path_first(uint32_t m) {
    const uint32_t group = lbu(m + 0x21);
    if (group == 0xff) return 1;
    uint32_t e = group_list(group);
    if (e == 0) return 1;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t table = first_moby();
    for (;; e += 2) {
        const uint32_t o = table + ((lhu(e) & 0x7fffu) << 8);
        if (lh(o + 0xa6) == GUARD_CLASS && o != 0) {
            const uint32_t st = lbu(o + 0x20);
            if (st != 0xfe && st != 0xfd && lbu(o + 0xbc) == 0) {
                const uint32_t opv = lwu(o + 0x78);
                if (lwu(opv + 0x144) == 0 && lw(opv + 0x124) != -1 && !(lw(opv + 0x13c) < lw(pv + 0x13c))) return 0;
            }
        }
        if (lh(e) < 0) break;
    }
    return 1;
}

// orig: L15:0x0029a248 L17:0x002a2148
void class44_look_at(uint32_t m, uint32_t o) {
    if (m == o) return;
    const uint32_t opv = lwu(o + 0x78);
    if (lh(opv + 0x19c) != 0 && rand_mod(0x10) != 0) return;
    sw(opv + 0x198, m);
    sh(opv + 0x19c, static_cast<uint32_t>(frame_scale_i(0xb4)));
}

}  // namespace rac1
