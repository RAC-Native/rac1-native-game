// Batalia (level 8) own moby classes, hand-ported: class 333, the Blarg grenadier of the fort's walls. It runs from one
// post of its group to the next (the group's other mobys, one trooper a post, its +0xbc marking it taken), ducks behind
// it, rises when Ratchet is between 8 and 20 away and it stands in the camera's view, and throws its grenades (class 248)
// at him; hit, it is knocked back or, killed, thrown, its bolts dropped, and once down the death arcs flash over it while
// it fades. With its helpers: the grenade's spawn and the choice of the next post. Registered in batalia_mobys.cpp.
// Addresses are level 8's (`orig: L8:`); its own data is L8_ADDR (the class's tuning at 0x161978 .. 0x161990, the
// camera's position 0x1675c0 and yaw 0x1675d8), the base ELF's are bare.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/batalia_own.h>
#include <game/ovl/doomlet.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_arcs.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
constexpr uint32_t PLAYER = 0x0013f350;  // the hero (+0x80 his position)
constexpr uint32_t HERO_POS = PLAYER + 0x80;
constexpr uint32_t HERO_MOBY = 0x001413d0;  // the hero's moby

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)

// the heading from (x0, y0) to (x1, y1), as the original computes it
float heading(float x0, float y0, float x1, float y1) { return atan2_f(fpu_sub(x1, x0), fpu_sub(y1, y0)); }

const float kCameraView = u2f(0x3f0efa35u);  // (32 degrees)

// the angle between the camera's yaw and the moby `m`'s direction seen from the camera (in its view: under kCameraView)
float camera_angle(uint32_t m) {
    const float from_camera = heading(lwc1(L8_ADDR(0x001675c0)), lwc1(L8_ADDR(0x001675c4)), lwc1(m + 0x10), lwc1(m + 0x14));
    return angle_dist(lwc1(L8_ADDR(0x001675d8)), from_camera);
}

// the animation set `set` started over `frames` frames, unless it plays
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frames);
}

// the moby's turn towards `target` (its rate pvar +0x12c)
void turn_to(uint32_t m, uint32_t pv, uint32_t target) {
    const float to = heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(target + 0x10), lwc1(target + 0x14));
    yaw_turn_damped(m, pv + 0x12c, to, 0.02f, 0.3f, 0.2f);
}

// the next post (grenadier_next_post_l8) in place of the current one (+0x120): the current one freed, the next one
// marked taken; with none found the current one stays and is marked again
void take_next_post(uint32_t m, uint32_t pv) {
    const uint32_t old = lwu(pv + 0x120);
    const uint32_t next = grenadier_next_post_l8(m);
    sw(pv + 0x120, next != 0 ? next : old);
    sb(old + 0xbc, 0);
    sb(lwu(pv + 0x120) + 0xbc, 1);
}

// the knock-back (move set `set`) away from Ratchet with the speeds `out` / `up` (+0x88 / +0x8c)
void knock_back(uint32_t m, uint32_t pv, uint32_t set, float out, float up) {
    sw(pv + 0x94, 9);
    swc1(pv + 0x88, out);
    swc1(pv + 0x8c, up);
    swc1(pv + 0x80, fpu_mul(dt_sq(), 30.0f));
    swc1(pv + 0x84, u2f(0x3a03126fu));  // 0.0005
    sb(pv + 0xad, 0);
    move_state_start(m, pv + 0x70, set, 1, 0, heading(lwc1(HERO_POS), lwc1(HERO_POS + 4), lwc1(m + 0x10), lwc1(m + 0x14)));
    swc1(pv + 0xc0, 6.0f);
    swc1(pv + 0xc4, 10.0f);
}
}  // namespace

// A grenade (class 248) thrown by `thrower` from `at` with the velocity `vel` (EE vectors): state 1, its pvars +0x30 the
// thrower, +0x20 a fuse of 300 frames, +0x2c 1, +0x24 0, +0x10 the velocity; a random roll, its yaw along the velocity.
// The grenade (0: none spawned).
// orig: L8:0x002d22e0
uint32_t grenade_spawn_l8(uint32_t thrower, uint32_t at, uint32_t vel) {
    const uint32_t g = moby_spawn(0xf8);
    if (g == 0) return 0;
    const uint32_t gp = lwu(g + 0x78);
    sh(g + 0x32, 0xff);
    sb(g + 0x30, 0xff);
    sb(g + 0x20, 1);
    sb(g + 0x31, 1);
    sw(gp + 0x30, thrower);
    sw(gp + 0x20, static_cast<uint32_t>(frame_scale_i(0x12c)));
    swc1(gp + 0x2c, 1.0f);
    sw(gp + 0x24, 0);
    lq_sq(g + 0x10, at);
    lq_sq(gp + 0x10, vel);
    swc1(g + 0x40, fpu_mul(rand_range_f(-180.0f, 180.0f), u2f(0x3c8efa35u)));  // (degrees to radians)
    swc1(g + 0x48, fpu_add(atan2_f(lwc1(vel), lwc1(vel + 4)), u2f(0x3fc90fdbu)));  // (+ pi / 2)
    moby_update_transform(g);
    return g;
}

// The grenadier `m`'s next post: of its group's (+0x21) mobys not grenadiers (class 333) nor taken (+0xbc), with the
// angle between the post's and Ratchet's directions from it: among those within 90 degrees the one at the widest angle;
// past that, by distance, one 16 or more away nearer than the best so far (or the best so far nearer than 16), one
// nearer than 16 farther than the best so far. 0: none (or no list).
// orig: L8:0x002daa10
uint32_t grenadier_next_post_l8(uint32_t m) {
    uint32_t e = lvd::group(static_cast<int32_t>(lbu(m + 0x21)));
    if (e == 0) return 0;
    uint32_t best = 0;
    float best_dist = 0.0f;
    float best_angle = 0.0f;
    for (;; e += 2) {
        const uint32_t post = lwu(L8_ADDR(0x0015ffd8)) + ((lhu(e) & 0x7fffu) << 8);
        if (lh(post + 0xa6) != 0x14d && lbu(post + 0xbc) == 0) {
            const float to_post = heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(post + 0x10), lwc1(post + 0x14));
            const float to_hero = heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(HERO_POS), lwc1(HERO_POS + 4));
            const float angle = angle_dist(to_post, to_hero);
            const float dist = vec3_distance(V(m + 0x10), V(post + 0x10));
            bool take;
            if (angle < u2f(0x3fc90fdbu)) {  // (pi / 2)
                take = best_angle < angle;
            } else if (16.0f <= dist) {
                take = dist < best_dist || best_dist < 16.0f;
            } else if (dist < 16.0f) {
                take = best_dist < dist;
            } else {
                take = false;  // (a distance that is not a number)
            }
            if (take) {
                best = post;
                best_dist = dist;
                best_angle = angle;
            }
        }
        if (lh(e) < -1) return best;
    }
}

// Class 333, the Blarg grenadier. pvars: +0x20 its health (2), +0x29 1, +0x38 a request to rise (cleared every frame),
// +0x40 / +0xd0 its walk (walker record +0xd0: speeds +0xd8 / +0xdc 2, +0xf4 0x161978 a second, +0x10c / +0x110 / +0x114),
// +0x58 / +0x5a 17 / 10, +0x60 its colour pulse (+0x67 its length), +0x70 its knock-back move, +0x120 its post (a moby
// of its group), +0x124 its last grenade, +0x128 its throw timer, +0x12c its turn rate, +0x130 / +0x132 its fade's start
// (the level's frame count 0x1619d0) and brightness (40), +0x134 its post timer, +0x160 its joint 0x1619ac's node (scaled
// 3). States (the table 0x1fbd90, 0 .. 99; 100 before it): 0 set up (its first post, the animation 4), state 1; 1 runs to
// its post (doomlet_259028): there and facing it, Ratchet farther than 6: the jump (anim 5, state 6), else the next post
// at once; elsewhere the next post every 240 frames; 6 the jump onto the post (on the animation's end: the post's
// animation 1, itself on the post 0.1 lower, the animation 1, facing Ratchet; state 3); 3 behind the post, turning to
// Ratchet: it rises (the post's animation 1, its own 2; state 4) when its throw timer is out with Ratchet 8 .. 20 away and
// itself in the camera's view, or asked to (+0x38) with Ratchet farther than 0.5; 4 steps out 2 in front of the post: at
// the animation's end, Ratchet nearer than 6 or itself out of the camera's view, the next post (state 1, animation 4),
// else the throw (animation 3, state 5); 5 the throw: at the animation's frame 19 a grenade from its point 9 at Ratchet
// (up to 0x16197c's frames' reach short of him by 2 and 0x161988 a frame, 3 .. 10 out), the next one in 180 frames; at
// the animation's end state 1 (animation 4); 90 / 92 / 93 knocked back (enemy_physics): landed, the next post (state 1,
// animation 4); 91 the fall's end (animation 7, state 93); 99 killed: landed, state 100 with the death arcs, and gone;
// 100 fading out (40 to 0 over the level's frame count) with the death arcs (none of it after state 99 retires it). The
// others: nothing. Knocked back below a height of 5, it is gone. Hit (mask 0x330000, not in state 99): under 1 damage the colour
// pulse; else knocked back (state 90, the move 6); its health out: thrown (the move 9, the speeds 0x16198c / 0x161990),
// its bolts dropped, its post freed, state 99.
// orig: L8:0x002dabf0
void bat_class333_update_l8(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x110);
    const uint32_t target_rec = sp;  // the target record (+0x40 its moby, +0x44 the result)
    const uint32_t hit_state = sp + 0x80, hit_damage = sp + 0x84;
    sw(hit_damage, 0);
    joint_node_scale(m, lwu(L8_ADDR(0x001619ac)), pv + 0x160, 3.0f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L8_ADDR(0x001675c0))) < 29.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x17);
    }
    if (lbu(m + 0x20) == 3) sh(m + 0x34, lhu(m + 0x34) & 0xfbffu);
    else sh(m + 0x34, lhu(m + 0x34) | 0x400u);

    if (lbu(m + 0x20) == 100) {
        // fading out: its brightness from +0x132 down to 0 over the level's frame count
        const float total = int_to_float(lw(L8_ADDR(0x001619d0)));
        const float since = static_cast<float>(static_cast<int32_t>(lwu(L8_ADDR(0x001619d0)) - static_cast<uint32_t>(lh(pv + 0x130))));
        const int32_t from = lh(pv + 0x132);
        const float fade = fpu_mul(static_cast<float>(-from), fpu_div(since, total));
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_add(static_cast<float>(from), fade))));
        arcs_update(m);
        ee_stack_free(0x110);
        return;
    }

    // a hit
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x330000), 0);
    {
        int32_t st = lw(hit_state);  // (a stack word the original never writes before)
        float dmg = lwc1(hit_damage);
        moby_hit_react_r(m, hit, pv + 0x20, 0, st, &dmg, nullptr, 4);
        sw(hit_state, static_cast<uint32_t>(st));
        swc1(hit_damage, dmg);
    }
    if (lw(hit_state) != 1 && lbu(m + 0x20) != 0x63) {
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        const float dmg = lwc1(hit_damage);
        const float hp = fpu_sub(lwc1(pv + 0x20), dmg);
        swc1(pv + 0x20, hp);
        if (0.0f < hp) {
            if (dmg < 1.0f) {
                sb(pv + 0x67, 100);
            } else {
                knock_back(m, pv, 6, fpu_mul(dt(), 5.0f), fpu_mul(dt(), 7.0f));
                sb(pv + 0x67, 0x78);
                sb(m + 0x20, 0x5a);
            }
        } else {
            knock_back(m, pv, 9, fpu_mul(lwc1(L8_ADDR(0x0016198c)), dt()), fpu_mul(lwc1(L8_ADDR(0x00161990)), dt()));
            moby_drop_bolts(m, 0, -1);
            sb(pv + 0x67, 0xfa);
            sb(m + 0x20, 0x63);
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(lwu(pv + 0x120) + 0xbc, 0);
        }
        color_pulse_start(m, pv + 0x60);
    }

    sb(m + 0xa4, 0xff);
    sw(target_rec + 0x44, static_cast<uint32_t>(target_select_any(m, target_rec, 16.0f)));
    if (lw(target_rec + 0x40) == 0) sw(target_rec + 0x40, lwu(HERO_MOBY));
    const uint32_t target = lwu(target_rec + 0x40);

    switch (lbu(m + 0x20)) {  // (the table 0x1fbd90: states 0 .. 99; those not named here, and 100 and above: nothing)
    case 0: {  // (0x002dafbc)
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sb(pv + 0x29, 1);
        swc1(pv + 0x20, 2.0f);
        walker_record_init_l1(pv + 0xd0);
        swc1(pv + 0xd8, 2.0f);
        swc1(pv + 0xdc, 2.0f);
        swc1(pv + 0x10c, 0.03f);
        swc1(pv + 0x110, 0.3f);
        swc1(pv + 0x114, 0.25f);
        swc1(pv + 0xf4, fpu_mul(lwc1(L8_ADDR(0x00161978)), dt()));
        sb(pv + 0x58, static_cast<uint32_t>(float_to_int(17.0f)));
        sb(pv + 0x5a, static_cast<uint32_t>(float_to_int(10.4f)));
        vec_clear(V(pv + 0x40));
        const uint32_t post = grenadier_next_post_l8(m);
        sw(pv + 0x120, post);
        sb(post + 0xbc, 1);
        sw(pv + 0x128, static_cast<uint32_t>(frame_scale_i(0xb4)));
        anim_to(m, 4, 0);
        sb(m + 0x20, 1);
        sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
        break;
    }
    case 1: {  // (0x002db0a0) running to its post
        doomlet_259028(m, pv + 0xd0, lwu(pv + 0x120) + 0x10, pv + 0x40);
        const uint32_t post = lwu(pv + 0x120);
        if (vec3_distance(V(m + 0x10), V(post + 0x10)) < 3.0f &&
            angle_dist(lwc1(m + 0x48), heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(post + 0x10), lwc1(post + 0x14))) <
                u2f(0x3e860a92u)) {  // (15 degrees)
            if (6.0f < vec2_distance(V(m + 0x10), V(target + 0x10))) {
                anim_to(m, 5, 0xa);
                sb(m + 0x20, 6);
            } else {
                sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
                take_next_post(m, pv);
            }
            break;
        }
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x134))) != 0) {
            take_next_post(m, pv);
            sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
        }
        break;
    }
    case 6: {  // (0x002db1f8) the jump onto the post
        doomlet_259028(m, pv + 0xd0, lwu(pv + 0x120) + 0x10, pv + 0x40);
        if ((lbu(m + 0x70) & 2u) == 0) break;
        moby_anim_set(lwu(pv + 0x120), 1, 0, frame_scale_i(6));
        lq_sq(m + 0x10, lwu(pv + 0x120) + 0x10);
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 0.1f));
        anim_to(m, 1, 0xa);
        sb(m + 0x20, 3);
        sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
        swc1(m + 0x48, heading(lwc1(m + 0x10), lwc1(m + 0x14), lwc1(target + 0x10), lwc1(target + 0x14)));
        sw(pv + 0x128, static_cast<uint32_t>(frame_scale_i(0x78)));
        sw(m + 0x94, 0);
        break;
    }
    case 3: {  // (0x002db2d0) behind its post
        const float dist = vec2_distance(V(m + 0x10), V(target + 0x10));
        turn_to(m, pv, target);
        bool rise = false;
        if (dist < 20.0f && 8.0f < dist && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x128))) != 0 &&
            camera_angle(m) < kCameraView)
            rise = true;
        else if (lw(pv + 0x38) != 0 && 0.5f < dist)
            rise = true;
        if (!rise) break;
        moby_anim_set(lwu(pv + 0x120), 1, 0, frame_scale_i(6));
        anim_to(m, 2, frame_scale_i(7));
        sb(m + 0x20, 4);
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        break;
    }
    case 4: {  // (0x002db458) stepping out, 2 in front of the post
        const uint32_t spot = sp + 0x50;
        const float c = cos_vu(lwc1(m + 0x48));
        swc1(spot, fpu_add(c, c));
        const float s = sin_vu(lwc1(m + 0x48));
        swc1(spot + 4, fpu_add(s, s));
        sw(spot + 8, 0);
        vec3_add(V(spot), V(spot), V(lwu(pv + 0x120) + 0x10));
        if (0.2f < vec2_distance(V(m + 0x10), V(spot))) doomlet_259028(m, pv + 0xd0, spot, pv + 0x40);
        if ((lbu(m + 0x70) & 2u) == 0) break;
        // (out of the camera's view here is kCameraView < the angle; an angle that is not a number throws)
        if (vec2_distance(V(m + 0x10), V(target + 0x10)) < 6.0f || kCameraView < camera_angle(m)) {
            take_next_post(m, pv);
            anim_to(m, 4, frame_scale_i(7));
            sb(m + 0x20, 1);
            sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
        } else {
            anim_to(m, 3, frame_scale_i(6));
            sb(m + 0x20, 5);
        }
        break;
    }
    case 5: {  // (0x002db5e0) the throw
        turn_to(m, pv, target);
        if (lbu(m + 0x52) == lbu(m + 0x53) && moby_anim_time(m) == 19.0f) {
            const uint32_t aim = sp + 0x70, hand = sp + 0x50, vel = sp + 0x60, flight = sp + 0x88;
            vec3_sub(V(aim), V(target + 0x10), V(m + 0x10));
            sw(aim + 8, 0);
            const float dist = vec3_length(V(aim));
            const float speed = fpu_div(dist, frame_scale_f(lwc1(L8_ADDR(0x0016197c))));
            float reach = fpu_sub(dist, fpu_add(fpu_mul(lwc1(L8_ADDR(0x00161988)), speed), 2.0f));
            if (10.0f < reach) reach = 10.0f;
            else if (reach < 3.0f) reach = 3.0f;
            vec3_clamp_length(aim, reach);
            vec3_add(V(aim), V(aim), V(m + 0x10));
            moby_chain_point_v(m, 9, V(hand));
            vec3_sub(V(vel), V(target + 0x10), V(hand));
            vec2_set_length(V(vel), V(vel), speed);
            float t = lwc1(flight);  // (a stack word the original never writes before)
            swc1(vel + 8, sub_0025abf0(hand, aim, &t, speed, -fpu_mul(dt_sq(), 15.0f)));
            swc1(flight, t);
            const uint32_t g = grenade_spawn_l8(m, hand, vel);
            sw(pv + 0x124, g);
            if (g == 0) break;
            const uint32_t gp = lwu(g + 0x78);
            sw(gp + 0x20, static_cast<uint32_t>(frame_scale_i(0xb4)));
            sh(gp + 0xc, static_cast<uint32_t>(frame_scale_i(0xa)));
            sh(gp + 0xe, static_cast<uint32_t>(frame_scale_i(0xa)));
            sb(lwu(pv + 0x124) + 0x20, 1);
            const uint32_t last = lwu(pv + 0x124);
            sb(last + 0xbc, lbu(last + 0xbc) | 3u);
            sw(pv + 0x128, static_cast<uint32_t>(frame_scale_i(0xb4)));
            break;
        }
        if ((lbu(m + 0x70) & 2u) == 0) break;
        sb(m + 0x20, 1);
        anim_to(m, 4, 3);
        sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
        break;
    }
    case 91:  // (0x002db828) the fall's end
        if ((lbu(m + 0x70) & 2u) == 0) break;
        anim_to(m, 7, 3);
        sb(m + 0x20, 0x5d);
        break;
    case 90:
    case 92:
    case 93: {  // (0x002db860) knocked back
        if ((enemy_physics(m, pv + 0x70) & 0x40u) != 0) {
            const uint32_t old = lwu(pv + 0x120);
            const uint32_t next = grenadier_next_post_l8(m);
            if (next != 0) {
                sw(pv + 0x120, next);
                sb(old + 0xbc, 0);
                sb(lwu(pv + 0x120) + 0xbc, 1);
            } else {
                sw(pv + 0x120, old);
            }
            anim_to(m, 4, 0xa);
            sb(m + 0x20, 1);
            sw(pv + 0x134, static_cast<uint32_t>(frame_scale_i(0xf0)));
        }
        if (lwc1(m + 0x18) < 5.0f) {  // (out of the level)
            moby_retire(m);
            ee_stack_free(0x110);
            return;
        }
        break;
    }
    case 99:  // (0x002db900) killed, thrown
        if ((enemy_physics(m, pv + 0x70) & 0x40u) == 0) break;
        sw(m + 0x58, 0);
        sb(m + 0x20, 100);
        sw(m + 0x94, 0);
        sh(pv + 0x132, 0x28);
        sh(pv + 0x130, lhu(L8_ADDR(0x001619d0)));
        arcs_update(m);
        moby_retire(m);
        ee_stack_free(0x110);
        return;
    default:  // (0x002db94c: 2, 7 .. 89, 94 .. 98, and 101 and above outside the table)
        break;
    }
    color_pulse_update(m, pv + 0x60);
    sw(pv + 0x38, 0);
    ee_stack_free(0x110);
}

}  // namespace rac1
