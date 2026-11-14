// Eudora's legged walkers (header eudora_walker.h: the record's layout): the engine that walks a two-legged moby on its
// gaits' animations, its legs bent toward the ground, its body kept on it, stepping up and down, running, jumping and
// landing. Called by class 217 (eudora_biped.cpp) and, through the generator's glue, by class 563 (generated). Addresses
// are Eudora's (`orig: L4:`) but for the four Veldin functions; Eudora-only data is L4_ADDR, the base ELF's are bare.
#include <game/ovl/eudora_walker.h>

#include <game/collision/collision.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/shared_class437.h>
#include <game/ovl/shared_little.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }       // the frame's time scale
float frame_rate() { return lwc1(0x0015ed68); }  // (the animation rate's scale)
float turn_dt() { return lwc1(0x0015ed60); }  // (the time scale of the turns)

// gait `slot` of the walker (+0x70 + 4 * slot)
uint32_t gait(uint32_t rec, uint32_t slot) { return lwu(rec + 0x70 + slot * 4); }
// the animation sequence of set `set` of the moby's class (its table +0x48)
uint32_t anim_seq(uint32_t m, uint32_t set) { return lwu(lwu(m + 0x24) + 0x48 + set * 4); }
// the frame `x` of gait `g`, wrapped into its length
int32_t gait_wrap(uint32_t g, float x) { return ee_rem(float_to_int(x), float_to_int(lwc1(g + 0x10))); }

// Gait `g` played from the key nearest the frame `start`, blended over `frames`; the legs' bends reset.
void gait_play(uint32_t m, uint32_t rec, uint32_t g, float start, int32_t frames) {
    const int32_t key = anim_seq_key_at(anim_seq(m, lwu(g)), start);
    moby_anim_set(m, lwu(g), key, frames);
    walker_legs_reset(rec);
}

// A step of gait `slot` on one foot (`foot` 0: the frames +0x38 / +0x40, 4: +0x3c / +0x44): blended over a quarter of
// the foot's lift at the walker's `speed`, from an eighth into it; the state `next`.
void gait_step(uint32_t m, uint32_t rec, uint32_t slot, uint32_t foot, float speed, uint32_t next) {
    const uint32_t g = gait(rec, slot);
    const int32_t frames = float_to_int(fpu_div(fpu_mul(lwc1(g + 0x38 + foot), 0.25f), fpu_div(speed, lwc1(g + 8))));
    const int32_t start =
        gait_wrap(g, fpu_sub(fpu_add(lwc1(g + 0x40 + foot), fpu_mul(lwc1(g + 0x38 + foot), 0.125f)), lwc1(g + 0xc)));
    gait_play(m, rec, g, static_cast<float>(start), frames);
    sb(rec + 0xb7, next);
}

// Back to walking (gait 0, state 1) from a step down on one foot: blended over the whole lift, from half into it.
void gait_walk_on(uint32_t m, uint32_t rec, uint32_t foot, float speed) {
    const uint32_t g = gait(rec, 0);
    const int32_t frames = float_to_int(fpu_div(lwc1(g + 0x38 + foot), fpu_div(speed, lwc1(g + 8))));
    const int32_t start =
        gait_wrap(g, fpu_sub(fpu_add(lwc1(g + 0x40 + foot), fpu_mul(lwc1(g + 0x38 + foot), 0.5f)), lwc1(g + 0xc)));
    gait_play(m, rec, g, static_cast<float>(start), frames);
    sb(rec + 0xb7, 1);
}

// Gait `slot` from its start key (+0x14, an integer), blended over 10 frames.
void gait_from_start(uint32_t m, uint32_t rec, uint32_t slot) {
    const uint32_t g = gait(rec, slot);
    gait_play(m, rec, g, static_cast<float>(lw(g + 0x14)), 10);
}

// Standing (state 0): the idle animation (+0xb0).
void walker_stand(uint32_t m, uint32_t rec) {
    moby_anim_set(m, lbu(rec + 0xb0), 0, 10);
    walker_legs_reset(rec);
    sb(rec + 0xb7, 0);
}

// The point half a stride of gait `g` ahead (heading `yaw`) of the foot +0x10 (`left`) or +0x30, into the frame word
// `at` (its w lane as the frame has it), raised by `up`; the floor's height there into +0xe4.
void floor_ahead(uint32_t rec, uint32_t g, float yaw, bool left, float up, uint32_t at) {
    swc1(at, fpu_mul(fpu_mul(cos_vu(yaw), lwc1(g + 4)), 0.5f));
    swc1(at + 4, fpu_mul(fpu_mul(sin_vu(yaw), lwc1(g + 4)), 0.5f));
    sw(at + 8, 0);
    vec3_add(V(at), V(at), V(left ? rec + 0x10 : rec + 0x30));
    if (up != 0.0f) swc1(at + 8, fpu_add(lwc1(at + 8), up));
    swc1(rec + 0xe4, floor_probe(0.5f, at, 0));
}

// A jump asked (+0xee bit 1) when a foot lands: jumping (bit 2), the floor's height kept; the take-off on the foot down.
void jump_start(uint32_t m, uint32_t rec, uint32_t landed, float speed) {
    sh(rec + 0xee, (lhu(rec + 0xee) & 0xfffeu) | 2u);
    swc1(rec + 0xe4, lwc1(m + 0x18));
    if (landed & 1) gait_step(m, rec, 11, 0, speed, 0xf);
    else gait_step(m, rec, 9, 4, speed, 0xe);
}

// States 1 .. 11: walking, stepping up and down.
void walker_walk(uint32_t m, uint32_t rec, uint32_t state, uint32_t landed, float speed, float yaw, uint32_t at) {
    if (state <= 6 && (landed & 5) != 0) sh(rec + 0xee, lhu(rec + 0xee) & 0xfff7u);
    const uint32_t slot = lwu(L4_ADDR(0x001ca8e0) + state * 4);  // (the state's gait)
    if (lwc1(m + 0x58) < 0.25f && angle_dist(lwc1(m + 0x48), yaw) < u2f(0x3dbd3c19u) && speed < fpu_mul(dt(), u2f(0x3e428f5cu))) {
        walker_stand(m, rec);
        return;
    }
    const uint32_t cur = gait(rec, slot);
    if (lbu(m + 0x52) != lwu(cur)) return;
    // fast enough, both feet down: running (state 13), its animation at the same phase of the stride
    const uint32_t run = gait(rec, 7);
    const float run_speed = fpu_mul(fpu_add(lwc1(run + 8), lwc1(gait(rec, 0) + 8)), 0.375f);
    if (run_speed < speed && run_speed < lwc1(rec + 0xc0) && (lbu(rec + 0xb6) & 0x3a) == 0) {
        const float t = moby_anim_time(m);
        const int32_t a = ee_rem(float_to_int(fpu_add(fpu_sub(lwc1(cur + 0x48), t), lwc1(cur + 0x10))), float_to_int(lwc1(cur + 0x10)));
        const int32_t b = ee_rem(float_to_int(fpu_add(fpu_sub(lwc1(cur + 0x4c), t), lwc1(cur + 0x10))), float_to_int(lwc1(cur + 0x10)));
        const int32_t key = float_to_int(fpu_sub(lwc1(run + (a < b ? 0x48 : 0x4c)), lwc1(run + 0xc)));
        const int32_t start = ee_rem(key + float_to_int(lwc1(run + 0x10)), float_to_int(lwc1(run + 0x10)));
        const int32_t frames = float_to_int(fpu_div(10.0f, fpu_div(speed, lwc1(run + 8))));
        gait_play(m, rec, run, static_cast<float>(start), frames);
        sb(rec + 0xb7, 0xd);
        return;
    }
    if ((landed & 5) == 0) return;
    if (lhu(rec + 0xee) & 1) {
        jump_start(m, rec, landed, speed);
        return;
    }
    // a foot down: the floor half a stride ahead of it, against the stride's own rise
    const bool left = (landed & 1) != 0;
    floor_ahead(rec, cur, yaw, left, 0.0f, at);
    if (state - 1 >= 11) return;
    const float floor = lwc1(rec + 0xe4);
    const float rise = fpu_add(fpu_sub(floor, lwc1(at + 8)), fpu_mul(fpu_mul(lwc1(cur + 4), 0.5f), sin_vu(lwc1(rec + 0xb8))));
    const float step = fpu_mul(lwc1(rec + 0xe0), 0.75f);
    const float drop = fpu_mul(-lwc1(rec + 0xe0), 0.75f);
    // (each a strict comparison of the original: `step < rise` and `rise < step` are not each other's negation)
    const bool up = step < rise, down = rise < drop, below_step = rise < step;
    auto step_down = [&] { sh(rec + 0xee, lhu(rec + 0xee) | 8u); };
    switch (state) {  // (the table L4:0x1db470, indexed by the state - 1)
    case 1:  // (0x002937d8)
        if (up) gait_step(m, rec, left ? 2 : 1, left ? 0 : 4, speed, left ? 3 : 2);
        else if (down) {
            step_down();
            gait_step(m, rec, left ? 5 : 4, left ? 0 : 4, speed, left ? 8 : 7);
        }
        break;
    case 2:  // (0x00293a00)
        if (down) {
            step_down();
            gait_step(m, rec, 5, 0, speed, 8);
        } else if (below_step) sb(rec + 0xb7, 5);
        else gait_step(m, rec, 3, 0, speed, 4);
        break;
    case 3:  // (0x00293b98)
        if (down) {
            step_down();
            gait_step(m, rec, 4, 4, speed, 7);
        } else if (below_step) sb(rec + 0xb7, 6);
        else gait_step(m, rec, 3, 4, speed, 4);
        break;
    case 4:  // (0x00293db0)
        if (down) {
            step_down();
            gait_step(m, rec, left ? 5 : 4, left ? 0 : 4, speed, left ? 8 : 7);
        } else if (below_step) gait_step(m, rec, left ? 1 : 2, left ? 0 : 4, speed, left ? 5 : 6);
        break;
    case 5:  // (0x0029414c)
        if (down) {
            step_down();
            gait_step(m, rec, 4, 4, speed, 7);
        } else if (up) sb(rec + 0xb7, 2);
        else gait_step(m, rec, 0, 4, speed, 1);
        break;
    case 6:  // (0x002942e8)
        if (down) {
            step_down();
            gait_step(m, rec, 5, 0, speed, 8);
        } else if (up) sb(rec + 0xb7, 3);
        else gait_step(m, rec, 0, 0, speed, 1);
        break;
    case 7:  // (0x00294484)
        if (up) gait_step(m, rec, 2, 0, speed, 3);
        else if (drop < rise) sb(rec + 0xb7, 0xa);
        else gait_step(m, rec, 6, 0, speed, 9);
        break;
    case 8:  // (0x00294554)
        if (up) gait_step(m, rec, 1, 4, speed, 2);
        else if (drop < rise) sb(rec + 0xb7, 0xb);
        else gait_step(m, rec, 6, 4, speed, 9);
        break;
    case 9:  // (0x002946a4)
        if (up) gait_step(m, rec, left ? 2 : 1, left ? 0 : 4, speed, left ? 3 : 2);
        else if (drop < rise) gait_step(m, rec, left ? 4 : 5, left ? 0 : 4, speed, left ? 0xa : 0xb);
        break;
    case 10:  // (0x002948c8)
        if (up) gait_step(m, rec, 1, 4, speed, 2);
        else if (down) sb(rec + 0xb7, 7);
        else gait_walk_on(m, rec, 4, speed);
        break;
    default:  // 11 (0x00294a4c)
        if (up) gait_step(m, rec, 2, 0, speed, 3);
        else if (down) sb(rec + 0xb7, 8);
        else gait_walk_on(m, rec, 0, speed);
        break;
    }
}

// State 13: running; slow enough, back to walking; a foot down: a jump asked, or the floor ahead of it. The speed kept
// at the run's least.
void walker_run(uint32_t m, uint32_t rec, uint32_t landed, float speed, float yaw, uint32_t at) {
    const uint32_t run = gait(rec, 7), walk = gait(rec, 0);
    const float least = fpu_mul(fpu_add(lwc1(run + 8), lwc1(walk + 8)), u2f(0x3eb33333u));
    if (lbu(m + 0x52) == lwu(run) && (lbu(rec + 0xb6) & 0x30) == 0) {
        const uint32_t feet = lbu(rec + 0xb6);
        if (speed < least && lwc1(rec + 0xc0) <= least) {
            // walking again: from a third of the foot's lift (the first foot when it is down, else the second)
            const uint32_t foot = (feet & 3) != 0 ? 0 : 4;
            const float lift = lwc1(walk + 0x38 + foot);
            const int32_t start =
                gait_wrap(walk, fpu_sub(fpu_add(fpu_div(fpu_add(lift, lift), 3.0f), lwc1(walk + 0x40 + foot)), lwc1(walk + 0xc)));
            gait_play(m, rec, walk, static_cast<float>(start), 10);
            sb(rec + 0xb7, 1);
        } else if ((landed & 5) != 0) {
            if (lhu(rec + 0xee) & 1) jump_start(m, rec, landed, speed);
            else floor_ahead(rec, walk, yaw, (landed & 1) != 0, 1.0f, at);
        }
    }
    if (lwc1(rec + 0xc0) < least) swc1(rec + 0xc0, least);
}

// States 14 / 15: taking off (gait 9 / 11, `end_slot`); the feet off the ground: the vertical speed for the goal +0x60,
// in the air, heading to it. The take-off played: landing (the set +0xb3, state 16); falling, near enough the landing:
// the landing gait (`land_slot` 10 / 12) blended over half the frames left, state `next` (17 / 18).
void walker_take_off(uint32_t m, uint32_t rec, uint32_t landed, bool level, uint32_t end_slot, uint32_t land_slot, uint32_t next) {
    if (landed & 0x30) {
        const float d = level ? vec2_distance(V(m + 0x10), V(rec + 0x60)) : vec3_distance(V(m + 0x10), V(rec + 0x60));
        (void)libgcc_extendsfdf2(d);  // (the distance printed by the original's debug print, L4:0x1611e0's format)
        debug_printf_stub();
        swc1(rec + 0xd8, sub_0025abf0(m + 0x10, rec + 0x60, nullptr, lwc1(rec + 0xc0), -lwc1(rec + 0xdc)));
        sh(rec + 0xee, lhu(rec + 0xee) | 4u);
        swc1(rec + 0x6c, atan2_f(fpu_sub(lwc1(rec + 0x60), lwc1(m + 0x10)), fpu_sub(lwc1(rec + 0x64), lwc1(m + 0x14))));
    }
    const float t = moby_anim_time(m);
    const uint32_t g = gait(rec, end_slot);
    if (fpu_sub(fpu_add(lwc1(g + 0xc), lwc1(g + 0x10)), lwc1(m + 0x58)) <= t) {
        moby_anim_set(m, lbu(rec + 0xb3), 0, 10);
        walker_legs_reset(rec);
        sb(rec + 0xb7, 0x10);
        return;
    }
    if ((lhu(rec + 0xee) & 4) == 0 || !(lwc1(rec + 0xd8) < 0.0f)) return;
    const float fall = walker_fall_time(m, rec);
    const uint32_t land = gait(rec, land_slot);
    float lead = fpu_sub(lwc1(land + 0x44), lwc1(land + 0xc));
    lead = fpu_add(lead, lead);
    if (!(fall < fpu_add(lead, 15.0f))) return;
    int32_t frames = float_to_int(fpu_sub(fall, lead)) >> 1;
    if (frames <= -1) frames = 0;
    gait_play(m, rec, land, 0.0f, frames);
    sb(rec + 0xb7, next);
}

// States 17 / 18: landing (gait 10 / 12): the animation's rate so that its touch-down frame comes with the fall's end;
// landed (a second or less left): the rate to it, the feet down (4), state `next` (19 / 20), no longer in the air.
void walker_land(uint32_t m, uint32_t rec, uint32_t slot, uint32_t next) {
    const uint32_t g = gait(rec, slot);
    if (lbu(m + 0x52) != lwu(g)) return;
    const float fall = walker_fall_time(m, rec);
    const float now = fpu_add(int_to_float(lbu(m + 0x50)), lwc1(m + 0x54));
    const float least = fpu_add(now, u2f(0x3dcccccdu));
    if (1.0f < fall) {
        float to = fpu_sub(fpu_sub(fpu_add(lwc1(g + 0x44), 2.0f), fpu_mul(fall, 0.5f)), lwc1(g + 0xc));
        if (to < least) to = least;
        swc1(m + 0x58, fpu_mul(frame_rate(), fpu_sub(to, now)));
        return;
    }
    float to = fpu_sub(lwc1(g + 0x44), lwc1(g + 0xc));
    if (to < least) to = least;
    swc1(m + 0x58, fpu_mul(frame_rate(), fpu_sub(to, now)));
    sb(rec + 0xb6, 4);
    sb(rec + 0xb7, next);
    sh(rec + 0xee, lhu(rec + 0xee) & 0xfffbu);
}

// States 19 / 20: the first foot down after the landing: running (gait 7, state 13) from the foot's lift, the floor ahead.
void walker_touch_down(uint32_t m, uint32_t rec, uint32_t landed, uint32_t foot, float speed, float yaw, uint32_t at) {
    if ((landed & 1) == 0) return;
    const uint32_t g = gait(rec, 7);
    const int32_t frames = float_to_int(fpu_div(fpu_add(fpu_mul(lwc1(g + 0x38 + foot), 0.25f), 2.0f), fpu_div(speed, lwc1(g + 8))));
    const int32_t start =
        gait_wrap(g, fpu_sub(fpu_add(fpu_add(lwc1(g + 0x40 + foot), 1.0f), fpu_mul(lwc1(g + 0x38), 0.125f)), lwc1(g + 0xc)));
    gait_play(m, rec, g, static_cast<float>(start), frames);
    sh(rec + 0xee, lhu(rec + 0xee) & 0xfffdu);
    sw(rec + 0xd8, 0);
    sb(rec + 0xb6, 1);
    sb(rec + 0xb7, 0xd);
    floor_ahead(rec, g, yaw, true, 1.0f, at);
}
}  // namespace

// orig: 0x0024ed18
int32_t anim_seq_key_at(uint32_t seq, float time) {
    const int32_t t = static_cast<int16_t>(float_to_int(fpu_mul(time, 16.0f)));
    const int32_t n = static_cast<int32_t>(lbu(seq + 0x10));
    const uint32_t keys = seq + 0x1c;
    for (int32_t i = 0; i < n; ++i) {
        const int32_t kt = lh(lwu(keys + static_cast<uint32_t>(i) * 4) + 4);
        if (t >= kt) continue;
        if (i > 0 && t - lh(lwu(keys + static_cast<uint32_t>(i - 1) * 4) + 4) < kt - t) return i - 1;
        return i;
    }
    return -1;
}

// orig: 0x002a13f0
uint32_t gait_phase_flags(uint32_t gait, float time) {
    // `time` within the range lo .. hi (a range past the loop's end wraps)
    auto within = [time](float lo, float hi) { return lo <= hi ? (lo <= time && time <= hi) : (lo <= time || time <= hi); };
    uint32_t flags = 0;
    for (uint32_t foot = 0; foot < 2; ++foot) {
        if (within(lwc1(gait + 0x40 + foot * 4), lwc1(gait + 0x48 + foot * 4))) flags |= 1u << (foot * 2);
        if (within(lwc1(gait + 0x28 + foot * 4), lwc1(gait + 0x30 + foot * 4))) flags |= 2u << (foot * 2);
    }
    if (flags != 0) return flags;
    const float a = lwc1(gait + 0x18), b = lwc1(gait + 0x1c);
    if (time < a || fpu_add(a, 1.0f) < time) flags |= 0x10;
    if (time < b || fpu_add(b, 1.0f) < time) flags |= 0x20;
    return flags;
}

// orig: 0x002a1538
void walker_legs_reset(uint32_t rec) {
    for (uint32_t leg = rec; leg < rec + 0x160; leg += 0xb0) {
        if (lbu(leg + 0x161) != 0) quat_from_axis_angle(V(leg + 0x170), 2, 0.0f);
        if (lbu(leg + 0x121) != 0) quat_from_axis_angle(V(leg + 0x130), 2, 0.0f);
        sw(leg + 0x104, 0);
        sw(leg + 0xfc, 0);
    }
}

// orig: 0x002a15c8
float walker_fall_time(uint32_t m, uint32_t rec) {
    const float g = lwc1(rec + 0xdc);
    const float v = fpu_add(lwc1(rec + 0xd8), fpu_mul(g, 0.5f));
    const float disc = fpu_add(fpu_mul(v, v), fpu_mul(fpu_add(g, g), fpu_sub(lwc1(m + 0x18), lwc1(rec + 0x68))));
    if (!(0.0f < disc)) return u2f(0x47c35000u);  // 100000
    const float r = sqrt_f(disc);
    return fpu_div(r <= v ? fpu_sub(v, r) : fpu_add(v, r), lwc1(rec + 0xdc));
}

// orig: L4:0x002927d0
uint32_t walker_phase_flags_l4(uint32_t m, uint32_t rec, float time) {
    const uint32_t set = lbu(m + 0x52);
    uint32_t playing = 0;  // (the last gait of the set playing)
    for (uint32_t slot = 0; slot < 13; ++slot)
        if (lwu(gait(rec, slot)) == set) playing = gait(rec, slot);
    if (playing == 0) return lbu(rec + 0xb6);
    return gait_phase_flags(playing, time);
}

// orig: L4:0x002922d0
float walker_floor_below_l4(uint32_t pos) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t to = sp, from = sp + 0x10;
    lq_sq(to, pos);
    float z = fpu_sub(lwc1(to + 8), 2.5f);
    if (z < u2f(0x3dcccccdu)) z = u2f(0x3dcccccdu);
    swc1(to + 8, z);
    lq_sq(from, pos);
    swc1(from + 8, fpu_add(lwc1(from + 8), 0.75f));
    const float floor = segment_collide(from, to, 2, 0, 0) != 0 ? coll_hit_point().z : 0.0f;
    ee_stack_free(0x30);
    return floor;
}

// orig: L4:0x00292828
uint32_t walker_ground_follow_l4(uint32_t m, uint32_t rec) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t normal = sp, ahead = sp + 0x10;
    uint32_t steep = 0;
    bool ledge = false;  // (running onto a floor ahead: the body raised to it)
    float dz = 0.0f, weight = 0.0f;
    if (lhu(rec + 0xee) & 4) {  // in the air
        const float floor = floor_probe(0.5f, m + 0x10, 0);
        const float z = lwc1(m + 0x18);
        if (z < fpu_sub(floor, u2f(0x3d4ccccdu))) {
            dz = fpu_sub(z, floor);
        } else {
            const float vz = lwc1(rec + 0xd8);
            const bool falling = vz < 0.0f;
            dz = -1.0f;
            if (falling && z < fpu_sub(fpu_add(floor, u2f(0x3dcccccdu)), vz)) dz = fpu_sub(floor, z);
        }
        sw(rec + 0xb8, 0);
    } else {
        // the feet on the ground: the floors under them, the normals of the flat ones
        vec_clear(V(normal));
        float floors = 0.0f;
        uint32_t bit = 1;
        for (uint32_t foot = rec; foot < rec + 0x40; foot += 0x20, bit <<= 2) {
            if ((lbu(rec + 0xb6) & bit) == 0) continue;
            const float floor = walker_floor_below_l4(foot);
            floors = fpu_add(floors, floor);
            if (!(0.0f < floor)) continue;
            const Vec4 n = coll_hit_normal();
            if (fpu_add(abs_f(n.x), abs_f(n.y)) < abs_f(n.z)) {
                weight = fpu_add(weight, 4.0f);
                vec3_add(V(normal), V(normal), n);
                dz = fpu_add(dz, fpu_sub(floor, lwc1(foot + 8)));
            }
        }
        if (floors == 0.0f) {
            const float floor = floor_probe(0.5f, m + 0x10, 0);
            if (0.0f < floor) {
                weight = fpu_add(weight, 4.0f);
                dz = fpu_add(dz, fpu_sub(floor, lwc1(m + 0x18)));
            }
        }
        // the pitch of the slope along the heading
        if (lwc1(normal) == 0.0f && lwc1(normal + 4) == 0.0f && lwc1(normal + 8) == 0.0f) {
            swc1(rec + 0xb8, 0.0f);
        } else {
            swc1(ahead, cos_vu(lwc1(m + 0x48)));
            swc1(ahead + 4, sin_vu(lwc1(m + 0x48)));
            sw(ahead + 8, 0);
            vec3_cross_ba(V(ahead), V(ahead), V(normal));
            vec3_cross_ba(V(ahead), V(ahead), V(normal));
            swc1(rec + 0xb8, atan2_f(vec2_length(V(ahead)), lwc1(ahead + 8)));
        }
        if (u2f(0x3f490fd8u) < abs_f(lwc1(rec + 0xb8))) steep = 2;
        // running onto a floor ahead within 30 degrees: the body pitched and raised to it
        if (lbu(rec + 0xb7) == 0xd && 0.0f < lwc1(rec + 0xe4) && (lbu(rec + 0xb6) & 0xf) == 0) {
            const float pitch = -atan2_f(lwc1(gait(rec, 7) + 4), fpu_sub(lwc1(rec + 0xe4), lwc1(m + 0x18)));
            if (abs_f(pitch) < u2f(0x3f060a92u)) {
                ledge = true;
                swc1(rec + 0xb8, pitch);
                weight = fpu_add(weight, 4.0f);
                dz = fpu_add(dz, fpu_sub(lwc1(rec + 0xe4), lwc1(m + 0x18)));
            }
        }
    }
    if (weight != 0.0f) dz = fpu_div(dz, weight);
    const float small = u2f(0x3ccccccdu);  // 0.025
    if (lhu(rec + 0xee) & 8) {  // stepping down: never pulled down
        if (dz < -small) dz = 0.0f;
        swc1(rec + 0xd8, 0.0f);
    } else if (!(-small < dz)) {  // falling
        dz = fpu_sub(lwc1(rec + 0xd8), lwc1(rec + 0xdc));
        swc1(rec + 0xd8, dz);
    } else {
        if (small < dz && !ledge) dz = 0.0f;
        sw(rec + 0xd8, 0);
    }
    for (uint32_t p = rec; p <= rec + 0x50; p += 0x10) swc1(p + 8, fpu_add(lwc1(p + 8), dz));
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), dz));
    ee_stack_free(0x100);
    return steep;
}

// orig: L4:0x00292d48
void walker_legs_aim_l4(uint32_t m, uint32_t rec, uint32_t flags) {
    const uint32_t sp = ee_stack_alloc(0x140);
    const uint32_t joint = sp, tilt = sp + 0x40;
    const float k = u2f(0x3dcccccdu), rate = u2f(0x3d0efa35u);  // 0.1, 2 degrees
    uint32_t mask = 3;
    for (uint32_t leg = rec; leg < rec + 0x160; leg += 0xb0, mask <<= 2) {
        moby_chain_world_one(m, lbu(leg + 0xf0), joint);
        float heading = atan2_f(lwc1(joint + 0x10), lwc1(joint + 0x14));
        const float bend = angle_add(angle_add(angle_sub(lwc1(leg + 0xf8), heading), lwc1(leg + 0x104)), lwc1(leg + 0xfc));
        heading = angle_sub(heading, bend);
        const uint32_t state = lbu(rec + 0xb7);
        if (state != 0 && (flags & mask) != 0) {
            // the foot down: the knee toward none (running) or half the bend; the hip takes the rest
            const float to = state == 0xd ? 0.0f : fpu_mul(bend, 0.5f);
            swc1(leg + 0x104, angle_seek_rate(leg + 0x108, lwc1(leg + 0x104), to, k, k, fpu_mul(turn_dt(), rate)));
            swc1(leg + 0xfc, angle_sub(bend, lwc1(leg + 0x104)));
            sw(leg + 0x100, 0);
        } else {
            swc1(leg + 0x104, angle_seek_rate(leg + 0x108, lwc1(leg + 0x104), 0.0f, k, k, fpu_mul(turn_dt(), rate)));
            swc1(leg + 0xfc, angle_seek_rate(leg + 0x100, lwc1(leg + 0xfc), 0.0f, k, k, fpu_mul(turn_dt(), rate)));
        }
        if (lwc1(leg + 0x104) < lwc1(leg + 0x10c)) swc1(leg + 0x104, lwc1(leg + 0x10c));
        if (lwc1(leg + 0x110) < lwc1(leg + 0x104)) swc1(leg + 0x104, lwc1(leg + 0x110));
        if (lwc1(leg + 0xfc) < lwc1(leg + 0x10c)) swc1(leg + 0xfc, lwc1(leg + 0x10c));
        if (lwc1(leg + 0x110) < lwc1(leg + 0xfc)) swc1(leg + 0xfc, lwc1(leg + 0x110));
        const float knee = angle_add(lwc1(leg + 0x104), heading);
        swc1(leg + 0xf8, angle_add(lwc1(leg + 0xfc), knee));
        if (lbu(leg + 0x161) != 0) quat_from_axis_angle(V(leg + 0x170), 2, lwc1(leg + 0x104));
        if (lbu(leg + 0x121) != 0) {
            quat_from_axis_angle(V(leg + 0x130), 2, lwc1(leg + 0xfc));
            if ((flags & mask) != 0) {  // the foot down: the hip against the body's pitch
                quat_from_axis_angle(V(tilt), 0, -lwc1(m + 0x40));
                quat_mul(V(leg + 0x130), V(leg + 0x130), V(tilt));
            }
        }
    }
    ee_stack_free(0x140);
}

// orig: L4:0x00293128
void walker_gait_update_l4(uint32_t m, uint32_t rec, uint32_t landed, float speed, float yaw, float accel) {
    const uint32_t sp = ee_stack_alloc(0xb0);
    const uint32_t state = lbu(rec + 0xb7);
    switch (state) {  // (the table L4:0x1db410)
    case 0: {  // (0x00293198) standing: off at a run, or walking once moving or turning for four frames
        sw(rec + 0xe4, 0);
        if (u2f(0x3a51b717u) < accel && fpu_mul(lwc1(gait(rec, 0) + 8), u2f(0x3faccccdu)) < speed) {
            gait_from_start(m, rec, 7);
            sb(rec + 0xb7, 0xd);
        } else {
            if (!(fpu_mul(dt(), u2f(0x3e570a3du)) < speed) && !(u2f(0x3dd67750u) < angle_dist(lwc1(m + 0x48), yaw))) break;
            const uint32_t count = (lhu(rec + 0xec) + 1) & 0xffffu;
            sh(rec + 0xec, count);
            if (static_cast<int16_t>(count) < 4) break;
            gait_from_start(m, rec, 0);
            sb(rec + 0xb7, 1);
        }
        const uint32_t g = gait(rec, lbu(rec + 0xb7) == 0xd ? 7 : 0);
        sb(rec + 0xb6, gait_phase_flags(g, static_cast<float>(lw(g + 0x14))));
        sh(rec + 0xec, 0);
        break;
    }
    case 1: case 2: case 3: case 4: case 5: case 6:  // (0x00293308)
    case 7: case 8: case 9: case 10: case 11:  // (0x00293320)
        walker_walk(m, rec, state, landed, speed, yaw, sp);
        break;
    case 12:  // (0x00294fd8)
        walker_stand(m, rec);
        break;
    case 13:  // (0x00294c50)
        walker_run(m, rec, landed, speed, yaw, sp);
        break;
    case 14:  // (0x00295164)
        walker_take_off(m, rec, landed, false, 9, 10, 0x11);
        break;
    case 15:  // (0x00294ffc)
        walker_take_off(m, rec, landed, true, 11, 12, 0x12);
        break;
    case 16: {  // (0x00295304) the landing set playing: the landing gait 12 blended over half the frames left
        if (lbu(m + 0x52) != lbu(rec + 0xb3)) break;
        const float fall = walker_fall_time(m, rec);
        const uint32_t land = gait(rec, 12);
        float lead = fpu_sub(lwc1(land + 0x44), lwc1(land + 0xc));
        lead = fpu_add(lead, lead);
        if (!(fall < fpu_add(lead, 15.0f))) break;
        gait_play(m, rec, land, 0.0f, float_to_int(fpu_sub(fall, lead)) >> 1);
        sb(rec + 0xb7, 0x12);
        break;
    }
    case 17:  // (0x0029544c)
        walker_land(m, rec, 10, 0x13);
        break;
    case 18:  // (0x002953ac)
        walker_land(m, rec, 12, 0x14);
        break;
    case 19:  // (0x002955d4)
        walker_touch_down(m, rec, landed, 4, speed, yaw, sp);
        break;
    case 20:  // (0x00295570)
        walker_touch_down(m, rec, landed, 0, speed, yaw, sp);
        break;
    default:
        break;
    }
    ee_stack_free(0xb0);
}

// orig: L4:0x00295778
uint32_t walker_update_l4(uint32_t m, uint32_t rec, float speed, float yaw, float accel) {
    const uint32_t sp = ee_stack_alloc(0x180);
    const uint32_t list = sp, joints = sp + 0x20, move = sp + 0x80;
    const uint32_t kind = sp + 0x90, size = sp + 0x94, size2 = sp + 0x98, point = sp, offset = sp + 0x10, moved = sp + 0x20;
    const uint32_t pos = m + 0x10;
    const float t = lbu(m + 0x52) != 0xff ? moby_anim_time(m)
                                          : fpu_add(int_to_float(lbu(m + 0x51)), class_anim_first_time(lbu(m + 0x22), lbu(m + 0x53)));
    const uint32_t phase = walker_phase_flags_l4(m, rec, t);
    // the heading: to the jump's goal in the air; else to `yaw`, at half the rate (twice the damping) with no foot down
    if (lhu(rec + 0xee) & 4)
        swc1(m + 0x48, angle_seek_rate(rec + 0xc8, lwc1(m + 0x48), lwc1(rec + 0x6c), lwc1(rec + 0xcc), lwc1(rec + 0xd0), lwc1(rec + 0xd4)));
    else if (phase & 0xf)
        swc1(m + 0x48, angle_seek_rate(rec + 0xc8, lwc1(m + 0x48), yaw, lwc1(rec + 0xcc), lwc1(rec + 0xd0), lwc1(rec + 0xd4)));
    else
        swc1(m + 0x48, angle_seek_rate(rec + 0xc8, lwc1(m + 0x48), yaw, 0.0f, fpu_add(lwc1(rec + 0xd0), lwc1(rec + 0xd0)), lwc1(rec + 0xd4)));
    walker_legs_aim_l4(m, rec, phase);
    swc1(m + 0x44, angle_seek_rate(rec + 0xbc, lwc1(m + 0x44), lwc1(rec + 0xb8), u2f(0x3ba3d70au), u2f(0x3c23d70au),
                                   fpu_mul(turn_dt(), u2f(0x3c8efa35u))));
    // the six joints' places now: the move is the mean of the planted points' drift from them
    auto joint_list = [&] {
        sw(list + 0x00, lbu(rec + 0xf0));
        sw(list + 0x04, lbu(rec + 0xf1));
        sw(list + 0x08, lbu(rec + 0x1a0));
        sw(list + 0x0c, lbu(rec + 0x1a1));
        sw(list + 0x10, lbu(rec + 0xb4));
        sw(list + 0x14, lbu(rec + 0xb5));
    };
    joint_list();
    moby_joint_positions(m, 6, list, joints);
    if (lhu(rec + 0xee) & 4) {  // in the air: the speed along the heading to the goal
        swc1(move, fpu_mul(cos_vu(lwc1(rec + 0x6c)), lwc1(rec + 0xc0)));
        sw(move + 8, 0);
        swc1(move + 4, fpu_mul(sin_vu(lwc1(rec + 0x6c)), lwc1(rec + 0xc0)));
    } else {
        const uint32_t held = phase | (lbu(rec + 0xb6) & 0x30);
        float n = 0.0f;
        vec_clear(V(move));
        for (uint32_t i = 0; i < 6; ++i) {
            if ((held & (1u << i)) == 0) continue;
            n = fpu_add(n, 1.0f);
            vec3_add(V(move), V(move), V(rec + i * 0x10));
            vec3_sub(V(move), V(move), V(joints + i * 0x10));
        }
        vec3_div(V(move), V(move), n);
    }
    for (uint32_t i = 0; i < 6; ++i) vec3_add(V(rec + i * 0x10), V(joints + i * 0x10), V(move));
    vec3_add(V(pos), V(pos), V(move));
    // the speed toward `speed`, by at most `accel` (and +0xc4)
    const uint32_t before = lbu(rec + 0xb6);
    sb(rec + 0xb6, phase);
    const uint32_t landed = (phase ^ before) & phase;
    if (lwc1(rec + 0xc4) < accel) accel = lwc1(rec + 0xc4);
    const float v = lwc1(rec + 0xc0);
    if (v < speed) {
        swc1(rec + 0xc0, fpu_add(v, accel));
        if (speed < lwc1(rec + 0xc0)) swc1(rec + 0xc0, speed);
    } else if (speed < v) {
        swc1(rec + 0xc0, fpu_sub(v, accel));
        if (lwc1(rec + 0xc0) < speed) swc1(rec + 0xc0, speed);
    }
    uint32_t result = walker_ground_follow_l4(m, rec);
    walker_gait_update_l4(m, rec, landed, speed, yaw, accel);
    if ((lhu(rec + 0xee) & 4) == 0) {
        // the body's lift: twice the step's height in a step (gaits 1 .. 3, or the run with a foot up), else easing off
        moby_collision_point(m, 0, kind, point, size, size2);
        const uint32_t set = lbu(m + 0x53);
        if (set == lwu(gait(rec, 3)) || set == lwu(gait(rec, 1)) || set == lwu(gait(rec, 2)) ||
            (set == lwu(gait(rec, 7)) && (lbu(rec + 0xb6) & 0xf) != 0))
            swc1(rec + 0xe8, fpu_add(lwc1(rec + 0xe0), lwc1(rec + 0xe0)));
        else if (set == lbu(m + 0x52))
            swc1(rec + 0xe8, fpu_mul(lwc1(rec + 0xe8), u2f(0x3f666666u)));
        const float lift = lwc1(rec + 0xe8);
        swc1(point + 8, fpu_add(lwc1(point + 8), lift));
        swc1(size2, fpu_sub(lwc1(size2), lift));
        vec3_sub(V(offset), V(pos), V(point));
        for (int32_t i = 0; i < 8; ++i) {  // pushed out of what the capsule meets
            if (capsule_collide(point, lwc1(size), lwc1(size2), 4, m) == 0) break;
            V(point) = coll_hit_pushed();
            result |= 1;
        }
        vec3_sub(V(moved), V(point), V(pos));
        vec3_add(V(pos), V(point), V(offset));
        if (vec2_length(V(moved)) < fpu_mul(lwc1(rec + 0xc0), 0.5f)) result |= 2;
    }
    joint_list();
    moby_joint_positions(m, 6, list, rec);
    if (lbu(rec + 0xb7) == 0x12 && lbu(m + 0x52) == lwu(gait(rec, 12))) {
        ee_stack_free(0x180);
        return result;
    }
    // the animation's rate: the speed over the gait's own (turning on the spot: as at a slow walk)
    swc1(m + 0x58, 1.0f);
    if (lbu(rec + 0xb7) != 0) {
        for (uint32_t slot = 0; slot < 13; ++slot) {
            const uint32_t g = gait(rec, slot);
            if (lwu(g) != lbu(m + 0x52)) continue;
            const float slow = fpu_mul(dt(), u2f(0x3e428f5cu));
            float v_now = lwc1(rec + 0xc0);
            if (v_now < slow && u2f(0x3dbd3c19u) < angle_dist(lwc1(m + 0x48), yaw) && speed < slow) v_now = slow;
            swc1(m + 0x58, fpu_mul(frame_rate(), abs_f(fpu_div(v_now, lwc1(g + 8)))));
            break;
        }
    }
    ee_stack_free(0x180);
    return result;
}

// orig: L4:0x00295de8
uint32_t walker_init_l4(uint32_t m, uint32_t rec) {
    const uint32_t sp = ee_stack_alloc(0xf0);
    const uint32_t list = sp, joint = sp + 0x20;
    sw(list + 0x00, lbu(rec + 0xf0));
    sw(list + 0x04, lbu(rec + 0xf1));
    sw(list + 0x08, lbu(rec + 0x1a0));
    sw(list + 0x0c, lbu(rec + 0x1a1));
    sw(list + 0x10, lbu(rec + 0xb4));
    sw(list + 0x14, lbu(rec + 0xb5));
    moby_joint_positions(m, 6, list, rec);
    for (uint32_t leg = rec; leg < rec + 0x160; leg += 0xb0) {
        moby_chain_world_one(m, lbu(leg + 0xf0), joint);
        sw(leg + 0x104, 0);
        sw(leg + 0xfc, 0);
        swc1(leg + 0xf8, atan2_f(lwc1(joint + 0x10), lwc1(joint + 0x14)));
        if (lbu(leg + 0x161) == 0) sub_0024f590(m, lbu(leg + 0xf3), leg + 0x160);
        if (lbu(leg + 0x121) == 0) sub_0024f590(m, lbu(leg + 0xf2), leg + 0x120);
    }
    const float t = lbu(m + 0x52) != 0xff ? moby_anim_time(m)
                                          : fpu_add(int_to_float(lbu(m + 0x51)), class_anim_first_time(lbu(m + 0x22), lbu(m + 0x53)));
    // the state of the gait playing (the table L4:0x1ca938, a byte per gait), the speed it walks at
    sb(rec + 0xb6, 5);
    sw(rec + 0xc0, 0);
    sb(rec + 0xb7, 0);
    for (uint32_t slot = 0; slot < 13; ++slot) {
        const uint32_t g = gait(rec, slot);
        if (lwu(g) != lbu(m + 0x53)) continue;
        swc1(rec + 0xc0, abs_f(fpu_mul(lwc1(m + 0x58), lwc1(g + 8))));
        sb(rec + 0xb7, lbu(L4_ADDR(0x001ca938) + slot * 4));
    }
    const uint32_t flags = walker_phase_flags_l4(m, rec, t);
    sb(rec + 0xb6, flags);
    sh(rec + 0xec, 0);
    sw(rec + 0xb8, 0);
    sw(rec + 0xbc, 0);
    sw(rec + 0xc8, 0);
    sw(rec + 0xe8, 0);
    sw(rec + 0xd8, 0);
    sw(rec + 0xe4, 0);
    ee_stack_free(0xf0);
    return flags;
}

}  // namespace rac1
