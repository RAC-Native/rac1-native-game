// Class 193: one port of the copies Gaspar and Quartu carry (the same code relinked). Each level keeps its own words (the
// knock-back's two speeds and the record its pvar +0xd0 points at: per level below); the rest is the engine's (OV: the path
// table 0x1b04b0, the class's moby list 0x15ffe4, the camera 0x166dc0) or the base ELF's (bare: the hero 0x13f3d0, the
// difficulty byte 0x1413f4, the frame scales 0x15ed6c / 0x15ed70). The shared helpers it calls: the group's word
// (moby_group_set_bc_l1), the walker record (walker_record_init_l1) and the path's next point (path_next_point_l1).
// Header shared_class193.h.
//
// The pvars: +0x20 the health, +0x26 the hit's timer, +0x38 a request to rest, +0x110 the colour pulse, +0x120 the move
// record, +0x180 the walker record, +0x190 its speed, +0x198 the fall, +0x1d0 its home, +0x1f0 the yaw's rate, +0x1f4 /
// +0x1f8 the side it veers to (radians, degrees), +0x1fc the sight range, +0x204 the speed, +0x208 / +0x210 / +0x214 /
// +0x224 timers, +0x216 the reach about +0x218 (a moby), +0x21c the path home (-1 none), +0x228 hidden for the difficulty.
#include <game/ovl/shared_class193.h>

#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/walker.h>
#include <game/ovl/weapons.h>
#include <game/lib/libc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position

// the level's words: +0 / +4 the knock-back's two speeds; +0x10 the record the pvar +0xd0 points at
uint32_t words() { return on_level(QUARTU) ? L15_ADDR(0x001619c8) : L9_ADDR(0x001618c8); }

// a random animation length (10 .. 12 frames at 60 Hz)
int32_t some_frames() { return frame_scale_i(rand_range(10, 0xd)); }
// the animation `set` over a random length unless it is the one set
void anim_unless(uint32_t m, uint32_t set) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, some_frames());
}
// the heading from the moby to the point `p`
float heading_to(uint32_t m, uint32_t p) {
    return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14)));
}
// the turn toward `yaw` (yaw_turn_damped through the yaw's rate +0x1f0)
void turn_damped(uint32_t m, uint32_t pv, float yaw, float accel, float damp, float vmax) {
    yaw_turn_damped(m, pv + 0x1f0, yaw, accel, damp, vmax);
}
// the point `len` ahead along the moby's heading (flat) into `dir`
void ahead(uint32_t m, uint32_t dir, float len) {
    swc1(dir, fpu_mul(cos_vu(lwc1(m + 0x48)), len));
    swc1(dir + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), len));
    sw(dir + 8, 0);
}
// the point twice its heading ahead, the walker's step toward it: the step's result (bit 2: blocked, 4: a drop)
uint32_t step_ahead(uint32_t m, uint32_t pv, uint32_t way, uint32_t dir) {
    const float c = cos_vu(lwc1(m + 0x48));
    swc1(dir, fpu_add(c, c));
    const float s = sin_vu(lwc1(m + 0x48));
    sw(dir + 8, 0);
    swc1(dir + 4, fpu_add(s, s));
    vec3_add(V(way), V(m + 0x10), V(dir));
    return walker_record_step(1.0f, m, pv + 0x180, way, dir);
}
// the way point toward `goal` into `way`: along the path home when it has one
void way_toward(uint32_t m, uint32_t pv, uint32_t goal, uint32_t way) {
    const int32_t path = lw(pv + 0x21c);
    if (path != -1) path_next_point_l1(m, lwu(OV(0x001b04b0) + (static_cast<uint32_t>(path) << 2)), goal, way);
    else lq_sq(way, goal);
}
// a new veer: its side flipped at random, a time of 30 .. 90 frames
void new_veer(uint32_t pv) {
    if ((rand_mod(0x100) & 1) != 0) swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
    swc1(pv + 0x1f4, fpu_mul(lwc1(pv + 0x1f8), u2f(0x3c8efa35u)));  // (1 degree)
    sw(pv + 0x210, static_cast<uint32_t>(frame_scale_i(static_cast<int32_t>(f2u(eer::cvt_w(rand_range_f(30.0f, 90.0f)))))));
}
// the fall: the speed +0x198 down by gravity (9.8), stopped on the floor `floor`
void fall_to(uint32_t m, uint32_t pv, float floor) {
    const float v = fpu_sub(lwc1(pv + 0x198), fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu)));  // (9.8)
    swc1(pv + 0x198, v);
    const float z = fpu_add(lwc1(m + 0x18), v);
    swc1(m + 0x18, z);
    if (z < floor) {
        sw(pv + 0x198, 0);
        swc1(m + 0x18, floor);
    }
}
// its group's word +0xbc set (its group called)
void call_group(uint32_t m) {
    if (lbu(m + 0x21) != 0xff) moby_group_set_bc_l1(lbu(m + 0x21), 1);
}
}  // namespace

// The update. The hits (but while dying, 0x63, or carried, 7): a hit of the class 0xb0 / 0xb1's kind early in the hit
// timer counts as a full one; the group called; the damage knocks it back (6) or kills it (0x63). Out of the hero's 40, it
// rests; hidden on the difficulty that leaves it out; its target found within its sight range. Then the state: 0 set
// up, 1 idle (sees its target near: 2, or wanders off: 3), 2 / 8 / 10 / 11 watching, 3 charging, 4 striking, 5 going
// home, 6 knocked back, 7 carried, 9 getting up, 12 backing off, 13 pushed, 0x63 falling dead. On the floor again after a
// fall it may burst.
// orig: L9:0x002e27d8 L15:0x002bc608
void class193_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x160);
    // its place at the start, the target's record (its moby at +0x40), the step, the way point, the target's offset
    const uint32_t start = sp, target = sp + 0x10, target_moby = sp + 0x50, dir = sp + 0x60, way = sp + 0x70, off = sp + 0x80;
    const uint32_t result = sp + 0x90, damage = sp + 0x94;
    const uint32_t pos = m + 0x10;
    int32_t found = 2;
    const uint32_t first = lbu(m + 0x20);
    if (first != 0x63 && first != 7) {
        swc1(damage, 0.0f);
        const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
        {
            int32_t st = lw(result);
            float d = lwc1(damage);
            moby_hit_react_r(m, hit, pv + 0x20, 0, st, &d, nullptr, 4);
            sw(result, static_cast<uint32_t>(st));
            swc1(damage, d);
        }
        timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x26)));
        if (lh(pv + 0x26) < frame_scale_i(0x2d) && hit != 0) {
            if (lhu(lwu(hit + 0x20) + 0xa6) - 0xb0u < 2u) swc1(hit + 0x2c, lh(pv + 0x26) != 0 ? 0.0f : 1.0f);
            call_group(m);
            const float dealt = lwc1(hit + 0x2c);
            if (dealt != 0.0f) {
                const uint32_t w = words();
                const float dt = lwc1(0x0015ed6c);
                const float health = fpu_sub(lwc1(pv + 0x20), dealt);
                const float back = fpu_mul(lwc1(w), dt), up = fpu_mul(lwc1(w + 4), dt);
                sw(pv + 0x144, 9);
                swc1(pv + 0x130, u2f(0x3c03126fu));  // (0.008)
                swc1(pv + 0x134, u2f(0x3a03126fu));  // (0.0005)
                swc1(pv + 0x20, health);
                swc1(pv + 0x138, up);
                swc1(pv + 0x13c, back);
                sb(pv + 0x15d, 0);
                const float strong = u2f(0x3faccccdu);  // (1.35: the hit of the weapon 0xb4)
                if (health <= 0.0f) {
                    const float up8 = fpu_mul(lwc1(0x0015ed6c), 8.0f), back10 = fpu_mul(lwc1(0x0015ed6c), 10.0f);
                    sh(m + 0x34, lhu(m + 0x34) & 0xefff);
                    swc1(pv + 0x138, up8);
                    swc1(pv + 0x13c, back10);
                    if (lhu(hit + 0x2a) == 0xb4) {
                        swc1(pv + 0x13c, fpu_mul(back10, strong));
                        swc1(pv + 0x138, fpu_mul(up8, strong));
                    }
                    const float away = atan2_f(fpu_sub(lwc1(pos), lwc1(target)), fpu_sub(lwc1(pos + 4), lwc1(target + 4)));
                    move_state_start(m, pv + 0x120, 5, 1, 0, away);
                    swc1(pv + 0x170, 11.0f);
                    swc1(pv + 0x174, 18.0f);
                    moby_drop_bolts(m, lbu(0x001413f4) == 2 ? 0x200u : 0u, -1);
                    sw(m + 0x94, 0);
                    sb(m + 0x20, 0x63);
                    sb(pv + 0x117, 0x78);
                } else {
                    if (lhu(hit + 0x2a) == 0xb4) {
                        swc1(pv + 0x13c, fpu_mul(back, strong));
                        swc1(pv + 0x138, fpu_mul(up, strong));
                    }
                    const float away = atan2_f(fpu_sub(lwc1(pos), lwc1(target)), fpu_sub(lwc1(pos + 4), lwc1(target + 4)));
                    move_state_start(m, pv + 0x120, 6, 1, 0, away);
                    swc1(pv + 0x170, 5.0f);
                    swc1(pv + 0x174, 10.0f);
                    sb(m + 0x20, 6);
                    sb(pv + 0x117, 0xfa);
                    sh(pv + 0x26, static_cast<uint32_t>(frame_scale_i(0x3c)));
                }
                color_pulse_start(m, pv + 0x110);
            }
        }
        sb(m + 0xa4, 0xff);
        joint_node_scale(m, 0, pv + 0x230, u2f(0x40066666u));  // (2.1)
        if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(OV(0x00166dc0))) < 38.0f) {
            moby_floor_band(m);
            sb(m + 0x7f, 0x1e);
        }
        if (40.0f < vec2_distance(V(pos), V(HERO_POS))) {
            ee_stack_free(0x160);
            return;
        }
        if (lw(pv + 0x228) != 0) {
            if (lbu(0x001413f4) != 2) {
                sb(m + 0x30, 0xff);
                sw(m + 0x94, 0);
                sh(m + 0x34, (lhu(m + 0x34) | 0x41) & 0xefff);
                ee_stack_free(0x160);
                return;
            }
            const uint32_t flags = lhu(m + 0x34) & 0xffbe;
            sh(m + 0x34, flags);
            sb(m + 0x30, 0x40);
            sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
            sh(m + 0x34, flags | 0x1000);
        }
        if (lw(pv + 0x38) != 0) sh(pv + 0x214, static_cast<uint32_t>(frame_scale_i(0xf0)));
        sw(pv + 0x38, 0);
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x208)));
        swc1(pv + 0x1fc, timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x214))) == 0 ? 20.0f : 12.0f);
        found = target_select_any(m, target, lwc1(pv + 0x1fc));
    }
    lq_sq(start, pos);
    const bool rested = 0 < timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x224)));
    float floor = -1.0f;
    // the target near enough to go for: seen within `reach` (or already close: found 0 / 1) and within 8 in height
    auto in_reach = [&](float reach) {
        vec3_sub(V(dir), V(target), V(pos));
        if (!(found < 2) && !(vec3_length(V(dir)) < reach)) return false;
        return abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))) < 8.0f;
    };
    enum class Exit { Tail, Done, Retire, Burst };
    auto run = [&]() -> Exit {
        const uint32_t state = lbu(m + 0x20);
        if (state >= 100) return Exit::Tail;
        switch (state) {  // (the level's jump table of a hundred states)
        case 0: {  // (L9:0x002e2cc8) set up at its home
            lq_sq(pv + 0x1d0, pos);
            sb(m + 0x20, 1);
            sb(pv + 0x29, 0);
            sh(pv + 0x214, 0);
            sw(pv + 0x208, 0);
            swc1(pv + 0x20, 1.0f);
            sh(pv + 0x24, 1);
            sb(pv + 0x5a, 2);
            sb(pv + 0x58, 8);
            walker_record_init_l1(pv + 0x180);
            const float speed = fpu_mul(lwc1(pv + 0x204), lwc1(0x0015ed6c));
            sw(pv + 0x180, 0x38d);
            swc1(pv + 0x188, 2.0f);
            swc1(pv + 0x18c, 2.0f);
            swc1(pv + 0x1a4, speed);
            if ((libc_rand() & 1) != 0) sh(m + 0x34, lhu(m + 0x34) | 0x8000);
            sw(pv + 0xd0, words() + 0x10);
            anim_unless(m, 2);
            floor = floor_probe_v(0.5f, V(pos), 0);
            swc1(m + 0x18, floor);
            return Exit::Tail;
        }
        case 1:  // (L9:0x002e2db8) idle: its target near (at random): watching; or off on a veer (in its mode 1)
            if (in_reach(fpu_add(lwc1(pv + 0x1fc), 4.0f)) && rand_mod(0x13) == 0) {
                sb(m + 0x20, 2);
                anim_unless(m, 4);
                return Exit::Tail;
            }
            if (lbu(m + 0xbc) != 1 || rand_mod(0x13) != 0) return Exit::Tail;
            new_veer(pv);
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
            sb(m + 0xbc, 0);
            return Exit::Tail;
        case 2: case 8: case 10: case 11: {  // (L9:0x002e2efc) watching: turned to the target; then charging
            turn_damped(m, pv, heading_to(m, target), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));  // (0.02, 0.3, 0.1)
            bool charge = false;
            if (in_reach(lwc1(pv + 0x1fc)) && rand_mod(0x13) == 0) {
                call_group(m);
                charge = true;
            } else if (lbu(m + 0xbc) == 1 && rand_mod(0x13) == 0) {
                charge = true;
            }
            if (charge) {
                new_veer(pv);
                sb(m + 0x20, 3);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
                sb(m + 0xbc, 0);
            }
            if (lbu(m + 0x53) == 4 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 2, 0, some_frames());
            return Exit::Tail;
        }
        case 3: case 12: {  // (L9:0x002e30e8, 0x002e39ac) charging the target (3, veering) or backing off toward home (12)
            const bool charging = state == 3;
            float steep = abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8)));
            steep = fpu_div(steep, vec2_distance(V(pos), V(target)));
            way_toward(m, pv, charging ? target : pv + 0x1d0, way);
            if (charging)
                turn_damped(m, pv, fpu_add(heading_to(m, way), lwc1(pv + 0x1f4)), u2f(0x3d4ccccdu), u2f(0x3e99999au),
                            u2f(0x3e4ccccdu));  // (0.05, 0.3, 0.2)
            else
                turn_damped(m, pv, heading_to(m, way), u2f(0x3d4ccccdu), u2f(0x3e99999au), u2f(0x3e4ccccdu));
            const uint32_t blocked = step_ahead(m, pv, way, dir);
            if (charging && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x210))) != 0) {
                const int32_t t = frame_scale_i(static_cast<int32_t>(f2u(eer::cvt_w(rand_range_f(30.0f, 90.0f)))));
                const float veer = lwc1(pv + 0x1f4);
                sw(pv + 0x210, static_cast<uint32_t>(t));
                swc1(pv + 0x1f4, -veer);
            }
            vec3_sub(V(off), V(target), V(pv + 0x1d0));
            // at a drop, or close to the target and (charging: not blocked; backing off: not too steep): striking (4)
            bool strike = (blocked & 4) != 0;
            if (!strike && vec2_distance(V(pos), V(target)) < 1.5f)
                strike = charging ? (blocked & 2) == 0 : steep <= 1.0f;
            if (strike) {
                sb(m + 0x20, 4);
                if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, some_frames());
                sb(m + 0xbc, 0);
                return Exit::Tail;
            }
            if (charging) {
                // too slow, too steep or blocked: going home (5) after a rest
                if (lwc1(pv + 0x190) < lwc1(0x0015ed6c) || 1.0f < steep || (blocked & 2) != 0) {
                    sw(pv + 0x224, static_cast<uint32_t>(frame_scale_i(0x32)));
                    sb(m + 0x20, 5);
                    sb(m + 0xbc, 0);
                    return Exit::Tail;
                }
                // the target near its home: the group called; else in its mode 1 it goes on, else home (5)
                if (vec3_length(V(off)) < fpu_add(lwc1(pv + 0x1fc), 4.0f) && abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))) < 8.0f) {
                    call_group(m);
                    sb(m + 0xbc, 0);
                    return Exit::Tail;
                }
                if (lbu(m + 0xbc) == 1) {
                    sb(m + 0xbc, 0);
                    return Exit::Tail;
                }
                sb(m + 0x20, 5);
                sb(m + 0xbc, 0);
                return Exit::Tail;
            }
            if (!(steep <= 1.0f)) {
                if ((blocked & 2) == 0) {
                    if ((lbu(m + 0x70) & 2) != 0) {
                        rand_mod(4);
                        const uint32_t set = rand_mod(4) != 0 ? 2u : 4u;
                        moby_anim_set(m, set, 0, some_frames());
                    }
                    sb(m + 0xbc, 0);
                    return Exit::Tail;
                }
            } else if ((blocked & 2) == 0) {
                sb(m + 0x20, 5);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
                sb(m + 0xbc, 0);
                return Exit::Tail;
            }
            // blocked backing off: idle (1) once rested, else as above
            if (rested) {
                sb(m + 0x20, 1);
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, some_frames());
                sb(m + 0xbc, 0);
                return Exit::Tail;
            }
            if ((lbu(m + 0x70) & 2) != 0) {
                rand_mod(4);
                const uint32_t set = rand_mod(4) != 0 ? 2u : 4u;
                moby_anim_set(m, set, 0, some_frames());
            }
            sb(m + 0xbc, 0);
            return Exit::Tail;
        }
        case 4: {  // (L9:0x002e33c8) striking: at its animation's frame 13 a blow on the target close ahead; then charging
            turn_damped(m, pv, heading_to(m, target), u2f(0x3d4ccccdu), u2f(0x3e99999au), u2f(0x3e4ccccdu));
            if (lbu(m + 0x52) == lbu(m + 0x53) && moby_anim_time(m) == 13.0f &&
                abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))) < 0.5f && vec2_distance(V(pos), V(target)) < 2.0f &&
                angle_dist(heading_to(m, target), lwc1(m + 0x48)) < u2f(0x3e860a92u)) {  // (15 degrees)
                ahead(m, dir, u2f(0x3e4ccccdu));  // (0.2)
                lq_sq(way, target);
                swc1(way + 8, fpu_add(lwc1(way + 8), 0.75f));
                sub_00259bc8(lwu(target_moby), m, 1, way, dir, 1.0f);
            }
            if ((lbu(m + 0x70) & 2) != 0 && 1.5f < vec2_distance(V(pos), V(target))) {
                sb(m + 0x20, 3);
                if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
            }
            floor = floor_probe_v(0.5f, V(pos), 0);
            fall_to(m, pv, floor);
            return Exit::Tail;
        }
        case 5: {  // (L9:0x002e360c) going home: there, idle (1); its target near again after a rest: charging; blocked:
                   // back where it was, pushed (0xc)
            way_toward(m, pv, pv + 0x1d0, way);
            turn_damped(m, pv, heading_to(m, way), u2f(0x3ca3d70au), u2f(0x3e99999au), u2f(0x3dcccccdu));
            const uint32_t blocked = step_ahead(m, pv, way, dir);
            if (vec2_distance(V(pos), V(pv + 0x1d0)) < 2.0f) {
                sb(m + 0x20, 1);
                if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, some_frames());
            }
            vec3_sub(V(off), V(target), V(pv + 0x1d0));
            if (rested) {
                bool charge = false;
                if (vec3_length(V(off)) < lwc1(pv + 0x1fc)) {
                    call_group(m);
                    charge = true;
                } else {
                    charge = lbu(m + 0xbc) == 1;
                }
                if (charge) {
                    sb(m + 0x20, 3);
                    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
                    sb(m + 0xbc, 0);
                    return Exit::Tail;
                }
            }
            if ((blocked & 2) != 0) {
                swc1(pos, lwc1(start));
                swc1(pos + 4, lwc1(start + 4));
                sb(m + 0x20, 0xc);
                if (lbu(m + 0x53) != 4) moby_anim_set(m, 4, 0, some_frames());
            }
            sb(m + 0xbc, 0);
            return Exit::Tail;
        }
        case 6:  // (L9:0x002e3854) knocked back: landed, getting up (9); below the world, gone
            if ((enemy_physics(m, pv + 0x120) & 1) == 0) return lwc1(m + 0x18) < 0.0f ? Exit::Retire : Exit::Tail;
            sb(m + 0x20, 9);
            if (lbu(m + 0x53) != 3) moby_anim_set(m, 3, 0, some_frames());
            return Exit::Done;
        case 7:  // (L9:0x002e38c4) carried: let go, idle (1)
            if (carry_update(m, pv + 0x120) == 0) return Exit::Tail;
            sb(m + 0x20, 1);
            if (lbu(m + 0x53) != 2) moby_anim_set(m, 2, 0, 0);
            sh(pv + 0xc8, 0);
            return Exit::Tail;
        case 9:  // (L9:0x002e3904) getting up: past its animation's frame 29, charging (3)
            if (!(29.0f < moby_anim_time(m))) return Exit::Tail;
            sb(m + 0x20, 3);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
            return Exit::Done;
        case 13: {  // (L9:0x002e3c18) pushed along its heading, kept 2 from the others of its class; out of its reach,
                    // going home (5)
            if (lbu(m + 0x52) != 0 && lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
            {
                const float c = cos_vu(lwc1(m + 0x48));
                swc1(dir, fpu_mul(c, fpu_mul(lwc1(pv + 0x204), lwc1(0x0015ed6c))));
                const float s = sin_vu(lwc1(m + 0x48));
                sw(dir + 8, 0);
                swc1(dir + 4, fpu_mul(s, fpu_mul(lwc1(pv + 0x204), lwc1(0x0015ed6c))));
            }
            vec3_add(V(way), V(pos), V(dir));
            for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) {
                if (o == m || static_cast<int8_t>(lbu(o + 0x20)) < 0) continue;
                const int32_t cls = lh(o + 0xa6);
                if (cls != 0 && cls != 0xc1) continue;
                if (!(vec2_distance(V(way), V(o + 0x10)) < 2.0f)) continue;
                const float z = lwc1(m + 0x18);
                const float side = angle_sub(lwc1(m + 0x48), heading_to(m, o + 0x10));
                float turn = fpu_mul(lwc1(0x0015ed6c), u2f(0x3fc90fdbu));  // (pi / 2)
                if (!(0.0f < side)) turn = -turn;
                swc1(m + 0x48, angle_add(lwc1(m + 0x48), turn));
                vec3_sub(V(way), V(pos), V(o + 0x10));
                vec3_set_length(V(way), V(way), 2.0f);
                vec3_add(V(way), V(way), V(o + 0x10));
                swc1(way + 8, z);
            }
            lq_sq(pos, way);
            floor = floor_probe_v(0.5f, V(pos), 0);
            fall_to(m, pv, floor);
            const uint32_t anchor = lwu(pv + 0x218);
            if (anchor != 0 && !(int_to_float(lh(pv + 0x216)) < vec2_distance(V(pos), V(anchor + 0x10)))) {
                sb(m + 0xbc, 0);
                return Exit::Tail;
            }
            sb(m + 0x20, 5);
            if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, some_frames());
            sb(m + 0xbc, 0);
            return Exit::Tail;
        }
        case 0x63: {  // (L9:0x002e3964) falling dead: on the ground it bursts; below the world, gone
            if ((enemy_physics(m, pv + 0x120) & 0x40) != 0) {
                vec_clear(V(dir));
                return Exit::Burst;
            }
            return lwc1(m + 0x18) < 0.0f ? Exit::Retire : Exit::Tail;
        }
        default:  // (L9:0x002e3e8c)
            return Exit::Tail;
        }
    };
    Exit exit = run();
    if (exit == Exit::Tail) {
        // on the floor (within 0.1) where the floor kills it: it bursts
        if (floor == -1.0f) floor = floor_probe_v(0.5f, V(pos), 0);
        if (floor != 0.0f && fpu_sub(lwc1(m + 0x18), floor) < u2f(0x3dcccccdu) && sub_001f35d8() == 1) exit = Exit::Burst;
        else color_pulse_update(m, pv + 0x110);
    }
    if (exit == Exit::Burst) explosion_burst(m, pos, 6, 0.5f, 13.0f);
    if (exit == Exit::Burst || exit == Exit::Retire) moby_retire(m);
    ee_stack_free(0x160);
}

}  // namespace rac1
