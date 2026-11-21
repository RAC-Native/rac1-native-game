// Eudora's class 427: a soldier with a gun (a separate moby, class 499, carried at its joint 1). It keeps between two
// paths (pvars +0xe0 / +0xe4, which one: +0xe8): it stands and turns toward its target, runs to cover (the point of its
// path farthest from the target), runs at the target once alerted (staying inside its path's polygon), raises its gun and
// fires a burst of shots when the camera looks its way, and strikes with the gun butt close up. A transport ship can
// drop it in its jump (state 4). Hit, it recoils or is knocked back, sliding away from the shot, and then changes paths;
// killed, it is thrown in an arc and retired where it lands. Header eudora_mobys.h; registered in eudora_mobys.cpp.
// Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
#include <game/collision/collision.h>
#include <game/level/level_data.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_arcs.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_shell.h>
#include <game/ovl/shared_spawns.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }         // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }      // (its square)
float rate_dt() { return lwc1(0x0015ed64); }    // (the time scale of the turns' rates and the springs)
const float kPi = u2f(0x40490fdbu);
const float kQuarterPi = u2f(0x3f490fdbu);
const float k15Deg = u2f(0x3e860a92u);
const float k30Deg = u2f(0x3f060a92u);
}  // namespace

// The soldier gone: its gun retired, its arcs updated, itself retired.
// orig: L4:0x002c4808
void trooper_retire_l4(uint32_t m) {
    if (const uint32_t gun = lwu(lwu(m + 0x78) + 0xf8); gun != 0) moby_retire(gun);
    arcs_update(m);
    moby_retire(m);
}

// pvars: health +0x20 (the hit state +0x20 .. +0x30), the alert asked +0x38 (from outside) and its timer +0xfe, the colour
// pulse +0x60 (+0x67 its strength), the throw's move record +0x70 (its speeds +0x80 .. +0x8c, +0xc0 / +0xc4), the fall
// speed +0xd0, the height it dies 5 below +0xd4, the last frame's animation time +0xd8, its sight +0xdc, the two paths
// +0xe0 / +0xe4 (+0xe8: 0 the second, else the first; swapped after a knock-back), the turn's rate +0xec, the timers
// +0xf0 (hits ignored while it runs), +0xf2 (action), +0xff (the run's animation held), +0x108 (before it runs again),
// the knock-back's speed +0xf4, the gun +0xf8, dropped +0xfc (starts aiming), the shots left in a burst +0xfd, the
// springs of its joints 5 (+0x110, its lean +0x178) and 6 (+0x190, its lean +0x1f8). States: 0 set up, 1 stands,
// 2 runs to cover, 3 runs (at the target when alerted, else to cover), 4 jumps (dropped), 5 aims, 6 fires, 7 strikes,
// 8 recoils, 9 / 10 knocked back, 11 thrown dying.
// orig: L4:0x002c4850
void eud_class427_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x1d0);
    const uint32_t pv = lwu(m + 0x78);
    // the frame's words passed by address (at the original's offsets)
    const uint32_t target = sp, aim = sp + 0x50, cover = sp + 0x60, step = sp + 0x70, trial = sp + 0x80, up = sp + 0x90;
    const uint32_t record = sp + 0xa0, hit_state = sp + 0xd0, hit_dmg = sp + 0xd4, thrown_yaw = sp + 0xd8;
    const uint32_t hit_yaw = sp + 0xdc, hit_scale = sp + 0xe0;
    const uint32_t pos = m + 0x10;
    const bool alerted = lw(pv + 0x38) != 0 || lbu(pv + 0xfe) != 0;
    moby_set_70(pv + 0x110, 3.0f);
    if (lbu(m + 0x31) != 0 && vec3_distance(V(pos), V(L4_ADDR(0x00166f40))) < 27.0f) {  // (its floor band near the camera)
        moby_floor_band(m);
        sb(m + 0x7f, 0x15);
    }
    target_select_any(m, target, alerted ? fpu_add(lwc1(pv + 0xdc), lwc1(pv + 0xdc)) : lwc1(pv + 0xdc));
    const uint32_t victim = lwu(target + 0x40);
    const float dist = vec2_distance(V(pos), V(target));
    swc1(hit_dmg, 0.0f);
    const float frame = moby_anim_time(m);
    if (lbu(m + 0x20) != 0 && lbu(pv + 0x2e) != 1) {  // (taken off the level)
        if (const uint32_t gun = lwu(pv + 0xf8); gun != 0) moby_retire(gun);
        moby_retire(m);
        ee_stack_free(0x1d0);
        return;
    }
    if (lw(pv + 0xe0) == -1 || lw(pv + 0xe4) == -1) {  // no paths: (the original prints the moby's index) gone
        debug_printf_stub();
        trooper_retire_l4(m);
        ee_stack_free(0x1d0);
        return;
    }

    // ---- its cover: the point of its path farthest from the target (+0x20 of the target's record: the point aimed at)
    lq_sq(aim, target + 0x20);
    const uint32_t path = lvd::path(lh(pv + 0xe8) != 0 ? lw(pv + 0xe0) : lw(pv + 0xe4));
    float cover_dist = 0.0f;
    int32_t farthest = -1;
    for (int32_t i = 0; i < lw(path); ++i) {
        const float d = vec2_distance(V(aim), V(path + 0x10 + (static_cast<uint32_t>(i) << 4)));
        if (cover_dist < d) {
            cover_dist = d;
            farthest = i;
        }
    }
    if (farthest != -1) {
        lq_sq(cover, path + 0x10 + (static_cast<uint32_t>(farthest) << 4));
        cover_dist = vec2_distance(V(cover), V(pos));
    } else {
        lq_sq(cover, pos);
        cover_dist = 0.0f;
    }

    // ---- the hit (the weapons' mask 0x210000), ignored while +0xf0 runs
    const uint32_t hit = sub_0025a420(m, NOT_ADDR(0x210000), 0);
    int32_t reaction = moby_hit_react(m, hit, pv + 0x20, 0, hit_state, hit_dmg, 0, 4);
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xf0)));
    timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xf2)));
    if (hit != 0 && lbu(m + 0x20) < 11 && lh(pv + 0xf0) == 0) {
        swc1(hit_scale, 1.0f);
        const uint32_t src = lwu(hit + 0x20);
        const uint32_t from = (src == 0 || src == lwu(0x001403e0)) ? 0x0013f350 + 0x80 : src + 0x10;  // (the hero's)
        swc1(hit_yaw, atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(from)), fpu_sub(lwc1(m + 0x14), lwc1(from + 4))));
        const float hp = fpu_sub(lwc1(pv + 0x20), lwc1(hit_dmg));
        swc1(pv + 0x20, hp);
        if (hp <= 0.0f) reaction = 1;
        sb(pv + 0xfe, static_cast<uint32_t>(frame_scale_i(0xf0)));
        // slid back from the shot at `speed` (its yaw from the hit's vector), facing it
        auto slide_back = [&](float speed) {
            swc1(pv + 0xf4, speed);
            lq_sq(step, hit + 0x10);
            vec_yaw_marked(step, hit_yaw, pv + 0xf4, hit_scale);
            swc1(m + 0x48, angle_add(kPi, lwc1(hit_yaw)));
        };
        switch (static_cast<uint32_t>(reaction) < 12 ? reaction : 0) {  // (the table L4:0x1dbce0)
        case 1: case 2:  // (0x002c4c2c) killed: thrown in an arc (state 11), its bolts dropped
            sh(m + 0x34, lhu(m + 0x34) & 0xefffu);
            sb(pv + 0x67, 0xfa);
            swc1(pv + 0x84, fpu_mul(dt_sq(), 20.0f));
            swc1(pv + 0x8c, fpu_mul(dt(), 13.0f));
            swc1(pv + 0x80, fpu_mul(dt_sq(), 37.0f));
            swc1(pv + 0x88, fpu_mul(dt(), 17.0f));
            lq_sq(step, hit + 0x10);
            vec_yaw_marked(step, thrown_yaw, pv + 0x88, pv + 0x8c);
            move_state_start(m, pv + 0x70, 0xd, 1, 0, lwc1(thrown_yaw));
            swc1(pv + 0xc0, 12.0f);
            swc1(pv + 0xc4, 25.0f);
            sb(m + 0x20, 0xb);
            moby_drop_bolts(m, 0, -1);
            break;
        case 3: case 6: {  // (0x002c4d00) recoils (animation 8, state 8); half as far from the shots of classes 305 / 176
            sb(pv + 0x67, 0x96);
            moby_anim_set(m, 8, 0, frame_scale_i(5));
            sb(m + 0x20, 8);
            const int32_t cls = lh(lwu(hit + 0x20) + 0xa6);
            float speed = fpu_div(2.0f, frame_scale_f(15.0f));
            if (cls == 0x131 || cls == 0xb0) speed = fpu_mul(speed, 0.5f);
            slide_back(speed);
            break;
        }
        case 4: case 5:  // (0x002c4db4) knocked back (animation 0xc, state 10)
            sb(pv + 0x67, 0xfa);
            moby_anim_set(m, 0xc, 0, frame_scale_i(5));
            sb(m + 0x20, 0xa);
            slide_back(fpu_div(2.0f, frame_scale_f(15.0f)));
            break;
        default:  // 0, 7 .. 11 (0x002c4e34)
            break;
        }
        color_pulse_start(m, pv + 0x60);
    }
    sb(m + 0xa4, 0xff);
    if (lw(pv + 0x38) != 0) sb(pv + 0xfe, static_cast<uint32_t>(frame_scale_i(0xf0)));
    sw(pv + 0x38, 0);
    countdown_u8(pv + 0xfe);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x108)));

    // ---- the states
    auto anim_ended = [&] { return (lbu(m + 0x70) & 2) != 0; };
    auto anim = [&](uint32_t set, int32_t frames) { moby_anim_set(m, set, 0, frames); };
    auto heading_to = [&](uint32_t p) {
        return atan2_f(fpu_sub(lwc1(p), lwc1(m + 0x10)), fpu_sub(lwc1(p + 4), lwc1(m + 0x14)));
    };
    // the turn toward `heading` at the rate `accel`, at most `most` (scaled by the frame's time)
    auto turn = [&](float heading, float accel, float most) {
        yaw_turn_damped(m, pv + 0xec, heading, fpu_mul(rate_dt(), accel), fpu_mul(rate_dt(), u2f(0x3e99999au)),
                        fpu_mul(dt(), most));
    };
    // facing the point aimed at (within 15 degrees)
    auto facing_aim = [&] { return angle_dist(heading_to(aim), lwc1(m + 0x48)) < k15Deg; };
    // the point `speed` ahead along its heading, into `step`
    auto step_ahead = [&](float speed) {
        swc1(step, fpu_mul(cos_vu(lwc1(m + 0x48)), fpu_mul(dt(), speed)));
        sw(step + 8, 0);
        swc1(step + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), fpu_mul(dt(), speed)));
        vec3_add(V(step), V(step), V(pos));
    };
    auto height_to_target = [&] { return abs_f(fpu_sub(lwc1(m + 0x18), lwc1(target + 8))); };
    // standing (animation 0, state 1)
    auto stand = [&] {
        anim(0, frame_scale_i(0xa));
        sb(m + 0x20, 1);
    };
    // running (animation 0xb, state 3), blended over `frames` frames
    auto run = [&](int32_t frames) {
        anim(0xb, frame_scale_i(frames));
        sb(m + 0x20, 3);
    };
    // to cover (animation 0xa, state 2)
    auto to_cover = [&](int32_t frames) {
        anim(0xa, frame_scale_i(frames));
        sb(m + 0x20, 2);
    };
    // the strike (animation 7, state 7)
    auto strike = [&] {
        sb(m + 0x20, 7);
        anim(7, frame_scale_i(7));
    };
    // aiming (animation 3, state 5)
    auto take_aim = [&] {
        sb(m + 0x20, 5);
        anim(3, frame_scale_i(7));
    };
    // an idle: animation 0 or 1 at random
    auto idle = [&] { anim(static_cast<uint32_t>((libc_rand() ^ 1) & 1), frame_scale_i(0xa)); };

    switch (lbu(m + 0x20)) {  // (the table L4:0x1dbd10)
    case 0:  // (0x002c4e9c) set up: aiming when dropped, else standing (mirrored at random)
        sh(m + 0x34, lhu(m + 0x34) | 0x1000u);
        swc1(pv + 0x20, 2.0f);
        sh(pv + 0x24, 2);
        sb(pv + 0x28, 1);
        sb(pv + 0x2e, 1);
        sb(pv + 0x29, 1);
        sh(pv + 0xf0, 0);
        sb(pv + 0xfe, 0);
        sb(pv + 0xff, 0);
        sw(pv + 0xd0, 0);
        sb(pv + 0x58, 8);
        swc1(pv + 0xd4, lwc1(m + 0x18));
        sb(pv + 0x5a, 0xa);
        if (libc_rand() & 1) sh(m + 0x34, lhu(m + 0x34) | 0x8000u);
        if (lbu(pv + 0xfc) != 0) {
            anim(3, frame_scale_i(0xa));
            sb(m + 0x20, 5);
        } else {
            idle();
            sb(m + 0x20, 1);
        }
        break;
    case 1: {  // (0x002c4f78) standing: turned to the target; runs, takes cover, aims at it, or strikes it
        if (anim_ended()) idle();
        turn(heading_to(aim), u2f(0x3d23d70au), u2f(0x40ea927fu));
        if (alerted) {
            run(8);
            break;
        }
        if (2.5f < dist && 1.0f < cover_dist) {
            if (14.0f < dist) {
                to_cover(8);
                break;
            }
            if (dist < 8.0f && lw(pv + 0x108) == 0) {
                run(8);
                break;
            }
        }
        if (dist < 14.0f && 2.5f <= dist && height_to_target() < 8.0f) {
            if (lh(pv + 0xf2) != 0) break;
            // the target in its line of fire (a line half a unit up from it to the point aimed at hits the target)
            vec_clear(V(up));
            swc1(up + 8, 0.5f);
            vec3_add(V(step), V(pos), V(up));
            vec3_add(V(trial), V(aim), V(up));
            if (segment_collide(step, trial, 0, m, 0) == 0) break;
            if (coll_hit_moby() != victim) break;
            take_aim();
            break;
        }
        if (!(dist <= 2.5f)) break;
        const int32_t wait = lh(pv + 0xf2);
        if (wait <= 0) {
            strike();
            break;
        }
        if (frame_scale_i(0x14) < wait) sh(pv + 0xf2, static_cast<uint32_t>(frame_scale_i(0x14)));  // (cut to 20 frames)
        break;
    }
    case 2:  // (0x002c5204) running to cover (leaning into the turn); there, or the target near: standing
        if (dist < 13.0f || cover_dist < 1.0f) {
            stand();
            break;
        }
        turn(heading_to(cover), u2f(0x3cf5c28fu), u2f(0x4096cbe4u));
        {
            float lean = fpu_mul(lwc1(pv + 0xec), 30.0f);
            if (kQuarterPi < lean) {
                lean = kQuarterPi;
            } else if (lean < -kQuarterPi) {
                lean = -kQuarterPi;
            }
            swc1(pv + 0x178, fpu_mul(lean, u2f(0x3f333333u)));
            swc1(pv + 0x1f8, fpu_mul(lean, u2f(0x3ecccccdu)));
        }
        step_ahead(2.5f);
        walker_step(m, pos, step, 0, 0.5f, 1.0f, 0.5f, k30Deg);
        break;
    case 3:  // (0x002c53a0) running
        sw(pv + 0x108, static_cast<uint32_t>(frame_scale_i(0x64)));
        if (alerted) {  // at the target, kept inside its path's polygon (else standing still, animation 0)
            turn(heading_to(aim), u2f(0x3d23d70au), u2f(0x409c61aau));
            step_ahead(6.0f);
            lq_sq(trial, pos);
            uint32_t set = 0;
            if (walker_step(m, trial, step, 0, 0.5f, 1.0f, 0.5f, k30Deg) != 0 && point_in_polygon(trial, path + 0x10, lw(path)) != 0) {
                lq_sq(pos, trial);
                set = 0xb;
            }
            if (countdown_u8(pv + 0xff) != 0 && lbu(m + 0x53) != set) {
                anim(set, frame_scale_i(0xa));
                sb(pv + 0xff, static_cast<uint32_t>(frame_scale_i(0xf)));
            }
            if (lh(pv + 0xf2) != 0) break;
            if (dist < 1.0f && facing_aim()) {
                strike();
                sb(pv + 0xff, 0);
            } else if (dist < 14.0f) {
                take_aim();
                sb(pv + 0xff, 0);
            }
            break;
        }
        // to cover: striking at a target close and ahead, aiming at one beyond 9, standing once there
        if (dist < 1.0f && facing_aim()) {
            strike();
            break;
        }
        if (9.0f < dist) {
            take_aim();
            break;
        }
        if (cover_dist < 1.0f) {
            stand();
            break;
        }
        turn(heading_to(cover), u2f(0x3d0f5c29u), u2f(0x4096cbe4u));
        step_ahead(6.0f);
        walker_step(m, pos, step, 0, 0.5f, 1.0f, 0.5f, k30Deg);
        break;
    case 4:  // (0x002c57bc) jumping (dropped; animation 2, thrown up): landed, standing
        if (lbu(m + 0x53) != 2) {
            anim(2, frame_scale_i(7));
            swc1(pv + 0xd0, -fpu_mul(dt(), u2f(0x40f56042u)));
            break;
        }
        if (anim_ended()) stand();
        break;
    case 5:  // (0x002c5844) aiming (animation 4): runs, takes cover, strikes, fires, or stands
        turn(heading_to(aim), u2f(0x3cf5c28fu), u2f(0x40b2b8c2u));
        if (anim_ended() && lbu(m + 0x52) != 4) anim(4, frame_scale_i(7));
        if (!alerted && 2.5f < dist && 1.0f < cover_dist) {
            if (14.0f < dist) {
                to_cover(9);
                break;
            }
            if (dist < 8.0f && lw(pv + 0x108) == 0) {
                run(5);
                break;
            }
        }
        if (dist < 2.5f) {
            strike();
            break;
        }
        // seen by the camera (it looks within 35 degrees of it) and facing the target within 45: a burst of 3 shots
        if (lh(pv + 0xf2) == 0 && dist < 14.0f && height_to_target() < 4.0f &&
            angle_dist(lwc1(L4_ADDR(0x00166f58)), atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(L4_ADDR(0x00166f40))),
                                                          fpu_sub(lwc1(m + 0x14), lwc1(L4_ADDR(0x00166f44))))) < u2f(0x3f1c61aau) &&
            angle_dist(lwc1(m + 0x48), heading_to(aim)) < kQuarterPi) {
            sb(m + 0x20, 6);
            anim(5, frame_scale_i(7));
            sh(pv + 0xf2, static_cast<uint32_t>(frame_scale_i(0x1e)));
            sb(pv + 0xfd, 3);
            break;
        }
        if (14.0f < dist) {  // too far: lowering the gun (animation 6), standing or running
            sb(m + 0x20, alerted ? 3 : 1);
            anim(6, frame_scale_i(0xa));
            sh(pv + 0xf2, static_cast<uint32_t>(frame_scale_i(0x1e)));
        }
        break;
    case 6: {  // (0x002c5b6c) firing (animation 5): a shot at its frame 2; the burst over, aiming again
        turn(heading_to(target + 0x20), u2f(0x3d23d70au), u2f(0x4096cbe4u));
        if (dist < 2.5f) strike();
        if (lbu(m + 0x53) == 5 && lwc1(pv + 0xd8) < 2.0f && 2.0f <= frame) {
            if (const uint32_t gun = lwu(pv + 0xf8); gun != 0) {
                // from the gun's muzzle (its joint 0) at the point aimed at, turned no more than 15 degrees off its heading
                const uint32_t muzzle = step, vel = trial;
                moby_chain_point(gun, 0, muzzle);
                vec3_sub(V(vel), V(aim), V(muzzle));
                float yaw = atan2_f(lwc1(vel), lwc1(vel + 4));
                if (k15Deg < angle_dist(yaw, lwc1(m + 0x48))) yaw = lwc1(m + 0x48);
                const float pitch = atan2_f(vec2_length(V(vel)), lwc1(vel + 8));
                vec_from_spherical_v(V(vel), fpu_mul(dt(), 10.0f), yaw, pitch);
                moby_sound_start(7, 0, m);
                shot_launch(m, muzzle, vel, float_to_int(fpu_div(14.0f, fpu_mul(dt(), 10.0f))));
                countdown_u8(pv + 0xfd);
                break;
            }
        }
        if (!anim_ended() || lh(pv + 0xf2) != 0 || lbu(pv + 0xfd) != 0) break;
        sh(pv + 0xf2, static_cast<uint32_t>(frame_scale_i(0xb4)));
        sb(m + 0x20, 5);
        anim(4, frame_scale_i(0xa));
        break;
    }
    case 7:  // (0x002c5d90) striking: the blow at its frames 12 .. 13 within reach and ahead; then aiming
        turn(heading_to(aim), u2f(0x3d23d70au), u2f(0x40b2b8c2u));
        if (lbu(m + 0x52) == lbu(m + 0x53) && 12.0f <= frame && frame < 13.0f && height_to_target() < 2.0f &&
            vec2_distance(V(pos), V(aim)) < 1.0f && facing_aim()) {
            // the blow's record: its direction (length 1), the marker 0x45afdf66, the striker, its class
            const float k = u2f(0x3e4ccccdu);
            swc1(record, fpu_mul(cos_vu(lwc1(m + 0x48)), k));
            swc1(record + 4, fpu_mul(sin_vu(lwc1(m + 0x48)), k));
            sw(record + 8, 0);
            sw(record + 0x10, m);
            sw(record + 0x14, 1);
            swc1(record + 0x1c, 1.0f);
            vec2_set_length(V(record), V(record), 1.0f);
            swc1(record + 8, 1.0f);
            swc1(record + 0xc, u2f(0x45afdf66u));
            sb(record + 0x19, 1);
            sh(record + 0x1a, lhu(m + 0xa6));
            sb(record + 0x18, 0);
            lq_sq(step, aim);  // (the point struck, 0.75 up: not passed on)
            swc1(step + 8, fpu_add(lwc1(step + 8), 0.75f));
            if (victim != 0) moby_hit_record(victim, record);
        }
        if (anim_ended()) {
            sb(m + 0x20, 5);
            sh(pv + 0xf2, static_cast<uint32_t>(frame_scale_i(0x14)));
            anim(4, frame_scale_i(7));
        }
        break;
    case 8: case 9: case 10:  // (0x002c5fc8) recoiling / knocked back: sliding back, slowing; then the other path, standing
        if (0.0f < lwc1(pv + 0xf4)) {
            swc1(step, fpu_mul(cos_vu(angle_add(lwc1(m + 0x48), kPi)), lwc1(pv + 0xf4)));
            swc1(step + 8, 0.0f);
            swc1(step + 4, fpu_mul(sin_vu(angle_add(lwc1(m + 0x48), kPi)), lwc1(pv + 0xf4)));
            vec3_add(V(step), V(step), V(pos));
            const float t = fpu_mul(frame_scale_f(15.0f), frame_scale_f(15.0f));
            swc1(pv + 0xf4, fpu_add(lwc1(pv + 0xf4), fpu_div(-2.0f, t)));
            walker_step(m, pos, step, 1, 0.5f, 1.0f, 0.5f, k30Deg);
        }
        if (anim_ended()) {
            sh(pv + 0xe8, lhu(pv + 0xe8) ^ 1u);
            anim(lbu(m + 0x20) == 9 ? 9 : 0, frame_scale_i(9));
            sb(m + 0x20, 1);
            sw(pv + 0x108, static_cast<uint32_t>(frame_scale_i(0x78)));
        }
        break;
    case 11:  // (0x002c6110) thrown dying: gone once it lands (or dies in the fall)
        if (enemy_physics(m, pv + 0x70) & 0x60) {
            trooper_retire_l4(m);
            ee_stack_free(0x1d0);
            return;
        }
        break;
    default:
        break;
    }

    // ---- every frame: the colour pulse, the head and body toward the target, the springs, the gun, the fall
    color_pulse_update(m, pv + 0x60);
    if (const uint32_t state = lbu(m + 0x20); state == 1 || state == 3 || state == 4 || state == 5 || state == 6) {
        const uint32_t who = victim != 0 ? victim : lwu(0x001413d0);  // (else the hero's moby)
        if (vec2_distance(V(pos), V(who + 0x10)) < 24.0f) {  // its joints turned toward it, at most 55 degrees
            float look = angle_sub(atan2_f(fpu_sub(lwc1(who + 0x10), lwc1(m + 0x10)), fpu_sub(lwc1(who + 0x14), lwc1(m + 0x14))),
                                   lwc1(m + 0x48));
            const float most = u2f(0x3f75be0bu);
            if (most < look) {
                look = most;
            } else if (look < -most) {
                look = -most;
            }
            const float half = fpu_mul(look, 0.5f);
            swc1(pv + 0x1f8, half);
            swc1(pv + 0x178, half);
        }
    }
    if (lhu(m + 0x34) & 0x8000u) {  // (mirrored)
        swc1(pv + 0x178, -lwc1(pv + 0x178));
        swc1(pv + 0x1f8, -lwc1(pv + 0x1f8));
    }
    joint_node_spring(m, pv + 0x110, 5, fpu_mul(rate_dt(), u2f(0x3c9374bcu)), fpu_mul(rate_dt(), u2f(0x3e99999au)));
    joint_node_spring(m, pv + 0x190, 6, fpu_mul(rate_dt(), u2f(0x3cc49ba6u)), fpu_mul(rate_dt(), u2f(0x3e99999au)));
    if (const uint32_t gun = lwu(pv + 0xf8); gun != 0) {  // the gun at the joint 1
        const uint32_t joint = step;
        moby_chain_world_one(m, 1, joint);
        lq_sq(gun + 0x10, joint + 0x30);
        moby_anim_advance(gun);
        moby_update_transform(gun);
        mat3_copy(M(gun + 0xc0), M(joint));
        mat3_normalize_columns(gun + 0xc0);
        if (lhu(m + 0x34) & 1) {
            sh(gun + 0x34, lhu(gun + 0x34) | 0x41u);
        } else {
            sh(gun + 0x34, lhu(gun + 0x34) & 0xffbeu);
        }
        moby_update_transform_kept(gun);
        sh(gun + 0x34, lhu(gun + 0x34) | 6u);
    } else {  // (spawned at the joint 1: class 499)
        moby_chain_point(m, 1, step);
        sw(pv + 0xf8, spawn_like(m, step, 0x1f3));
    }
    // the fall (2 below to the floor)
    swc1(pv + 0xd0, fpu_add(lwc1(pv + 0xd0), fpu_mul(dt_sq(), u2f(0x411ccccdu))));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), fpu_sub(lwc1(pv + 0xd0), 2.0f)));
    const float floor = floor_probe(0.5f, pos, 0);
    const float z = fpu_sub(lwc1(m + 0x18), 2.0f);
    swc1(m + 0x18, z);
    if (z < floor) {
        swc1(m + 0x18, floor);
        sw(pv + 0xd0, 0);
    }
    if (lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0xd4), 5.0f)) {  // fallen off the level: its bolts dropped, gone
        moby_drop_bolts(m, 0, -1);
        trooper_retire_l4(m);
        ee_stack_free(0x1d0);
        return;
    }
    swc1(pv + 0xd8, frame);
    ee_stack_free(0x1d0);
}

}  // namespace rac1
