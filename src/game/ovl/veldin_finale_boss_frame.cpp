// Veldin's finale (level 18): what the boss (class 1422: its update in veldin_finale_boss.cpp) does every frame
// around its state, hand-written (part 3 of its port): the preamble (its size, the hits and the damage that count, its
// health against the phase's floor, its death, the two attached mobys, the target, the lift's end, the phase change with
// its scripted camera move), the camera that follows the fight, and the smoke and sparks at its mouth. The boss's update
// (veldin_finale_boss.cpp) calls them. Level 18 only: the addresses are the finale's
// (`orig: L18:`), its own data L18_ADDR (the tuning words L18:0x162330 .. 0x16244c, the camera slots L18:0x167680 ..
// 0x16a290), the base ELF's bare (the hero 0x13f350 and his moby 0x1413d0, the sound slots 0x13e550, the frame time
// 0x15ed6c and its square 0x15ed70). Declared in veldin_finale_own.h.
//
// The boss's pvars used here: +0x20 its hit state, +0x60 its colour pulse, +0x70 the target record (target_select's
// 0x48 bytes, the moby at +0xb0), +0x200 the gates' group, +0x204 + 4k the floating platforms' group of phase k, +0x218
// the target's path, +0x21c the lift, +0x224 the level camera, +0x228 / +0x250 / +0x260 + 4k the boxes and path of phase
// k's camera move, +0x230 .. +0x23c the four rafts, +0x300 .. +0x318 the home of the next phase, +0x330 the gate, +0x334
// the camera move's moby, +0x338 the burst, +0x340 the phase's path, +0x344 the arena's box, +0x348, +0x34c the phase,
// +0x354 / +0x358 timers, +0x360 the camera's timer, +0x364 no charge, +0x368 the camera request's key, +0x36c its health
// as an integer, +0x378 / +0x37c the camera seeks' speeds, +0x38c its health, +0x390 / +0x394 the damage taken in and out
// of the hero's mode 0xf, +0x39c the state after a scene, +0x3a0 / +0x3a4 the attached mobys, +0x3b0 its sound slot,
// +0x3c0 its home, +0x3ec the hero's ride seen.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/devastator.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_AT = 0x0013f3d0;     // the hero's position (0x13f350 + 0x80)
constexpr uint32_t HERO_MOBY = 0x001413d0;   // the hero's moby
constexpr uint32_t HERO_MODE = 0x001413dc;   // his movement mode (0xf: the one the boss's camera and damage test)
constexpr uint32_t SOUND_SLOTS = 0x0013e550;  // the sound slots, 0x70 bytes each (+0x88 the owner, +0x74 playing)

uint32_t first_moby() { return lwu(L18_ADDR(0x0015ffd8)); }
uint32_t table_moby(uint32_t index) { return first_moby() + (index << 8); }
uint32_t box_centre(int32_t i) { return lvd::box(lvd::BOXES, i) + 0x30; }

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// `v` kept to lo .. hi (hi tested first)
float clamp_hi_lo(float v, float lo, float hi) {
    if (hi < v) return hi;
    if (v < lo) return lo;
    return v;
}

// the camera request `pv` + 0x368 holds (-1: none) let go
void camera_release(uint32_t pv) {
    const int32_t key = lw(pv + 0x368);
    if (key == -1) return;
    sub_002359f8(static_cast<uint32_t>(key), 0);
    sw(pv + 0x368, 0xffffffffu);
}

// the boss's sound (its slot pv + 0x3b0) stopped when sub_0028d8c0 says it runs: the slot stopped while it still plays
// for the boss, the slot forgotten (-1)
void sound_stop(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x3b0)) == 0) return;
    const int32_t slot = lw(pv + 0x3b0);
    if (slot != -1) {
        const uint32_t e = SOUND_SLOTS + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x3b0, 0xffffffffu);
}

// one of the boss's attached mobys (pvars `slot`: class `cls`, spawned the first time, shown, its animation 1) placed on
// joint `joint` of `on`: its colour / flags taken from the boss
void attached_place(uint32_t m, uint32_t pv, uint32_t slot, uint32_t cls, uint32_t on_slot, uint32_t joint, uint32_t mat) {
    if (lwu(pv + slot) == 0) {
        sw(pv + slot, moby_spawn(cls));
        sb(lwu(pv + slot) + 0x30, 0xff);
        sb(lwu(pv + slot) + 0x31, 1);
        moby_anim_set(lwu(pv + slot), 1, 0, 0);
    }
    const uint32_t a = lwu(pv + slot);
    sh(a + 0x32, lhu(m + 0x32));
    sh(a + 0x34, lhu(m + 0x34) | 0x100u);
    const uint32_t on = on_slot == 0 ? m : lwu(pv + on_slot);
    moby_chain_world_one(on, joint, mat);
    mat3_copy(M(a + 0xc0), M(mat));
    moby_chain_point_v(on, joint, V(a + 0x10));
    moby_update_transform(a);
}

// The phase's damage (states 2 and 12 .. 19: the hit counted): the hit's moby of class 0xb1 counts three times, of class
// 0x131 two thirds; added to +0x390 while the hero is in his mode 0xf, else to +0x394. When it is not 0: in state 2 a
// hit of kind 0x80000 plays animation 0xe; the health (+0x38c) lowered by it, kept at the phase's floor (true when it
// crossed it now); the colour pulse; at 0 health the death (state 0x1a: the camera request and the sound let go).
// `dmg` is the EE word the hit reaction wrote.
bool take_damage(uint32_t m, uint32_t pv, uint32_t hit, uint32_t dmg, float floor) {
    const uint32_t by = lwu(hit + 0x20);  // (the original reads it without testing the hit: the null page's word for none)
    if (by != 0) {
        const int16_t cls = lh(by + 0xa6);
        if (cls == 0xb1) swc1(dmg, fpu_mul(lwc1(dmg), 3.0f));
        else if (cls == 0x131) swc1(dmg, fpu_mul(lwc1(dmg), 0.666f));
    }
    if (lw(HERO_MODE) == 0xf) swc1(pv + 0x390, fpu_add(lwc1(pv + 0x390), lwc1(dmg)));
    else swc1(pv + 0x394, fpu_add(lwc1(pv + 0x394), lwc1(dmg)));
    if (lwc1(dmg) == 0.0f) return false;
    if (lbu(m + 0x20) == 2 && hit != 0 && (lwu(hit + 0x24) & 0x80000u) != 0 && lbu(m + 0x53) != 0xe)
        moby_anim_set(m, 0xe, 5, 1);
    const float health = lwc1(pv + 0x38c);
    const float left = fpu_sub(health, lwc1(dmg));
    const bool crossed = floor <= health && left < floor;
    swc1(pv + 0x38c, left < floor ? floor : left);
    sb(pv + 0x67, 0xf0);
    color_pulse_start(m, pv + 0x60);
    if (lwc1(pv + 0x38c) != 0.0f) return crossed;
    camera_release(pv);  // the death
    sb(m + 0x20, 0x1a);
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
    sound_stop(m, pv);
    sw(pv + 0x358, 0);
    return false;
}

// The next phase (from phase 0 or 1, state 3): its path (+0x340: its first point the boss's new home's way), the
// floating platforms' group, the new home +0x300 (25 from the old arena's centre toward that point, on the ground), and
// the scripted camera move (class 644, +0x334) from above the home to the hero's box (+0x228) seen from 15 behind it,
// its frames / wait L18:0x162384 / 0x162388. The five vectors in the original's frame (sp + 0x50 .. 0xa0).
void phase_next(uint32_t m, uint32_t pv, uint32_t sp) {
    sb(m + 0x20, 3);
    const int32_t old = lw(pv + 0x34c);
    const int32_t next = old + 1;
    sw(pv + 0x34c, static_cast<uint32_t>(next));
    const uint32_t path = lvd::path(lw(pv + 0x260 + static_cast<uint32_t>(next) * 4));
    sw(pv + 0x340, path);
    sw(pv + 0x348, 1);
    sw(pv + 0x398, lwu(pv + 0x204 + static_cast<uint32_t>(next) * 4));
    sw(pv + 0x374, 0);
    const uint32_t home = pv + 0x300;
    const uint32_t arena = box_centre(lw(pv + 0x250 + static_cast<uint32_t>(old) * 4));
    lq_sq(home, path + 0x10);
    vec3_sub(V(home), V(home), V(arena));
    vec2_set_length(V(home), V(home), 25.0f);
    sw(home + 8, 0);
    vec3_add(V(home), V(home), V(arena));
    vec_clear(V(pv + 0x310));
    swc1(pv + 0x318, atan2_f(fpu_sub(lwc1(arena + 0), lwc1(home + 0)), fpu_sub(lwc1(arena + 4), lwc1(home + 4))));

    const uint32_t first = sp + 0x50, last = sp + 0x60, first_angles = sp + 0x70, last_angles = sp + 0x80,
                   hero = sp + 0x90, back = sp + 0xa0;
    lq_sq(first, home);  // 20 along the new home's bearing, 3 up
    swc1(first + 0, fpu_add(lwc1(first + 0), fpu_mul(cos_vu(lwc1(pv + 0x318)), 20.0f)));
    swc1(first + 4, fpu_add(lwc1(first + 4), fpu_mul(sin_vu(lwc1(pv + 0x318)), 20.0f)));
    swc1(first + 8, fpu_add(lwc1(first + 8), 3.0f));
    lq_sq(hero, box_centre(lw(pv + 0x228 + static_cast<uint32_t>(old) * 4)));  // the hero's place: on the floor below his box
    swc1(hero + 8, fpu_add(lwc1(hero + 8), 20.0f));
    swc1(hero + 8, floor_probe_v(0.5f, V(hero), 0));
    lq_sq(last, hero);  // 15 from it away from the arena, 15 up
    vec3_sub(V(back), V(arena), V(last));
    sw(back + 8, 0);
    vec3_set_length(V(back), V(back), -15.0f);
    vec3_add(V(last), V(last), V(back));
    swc1(last + 8, fpu_add(lwc1(last + 8), 15.0f));
    swc1(hero + 0xc, atan2_f(fpu_sub(lwc1(hero + 0), lwc1(last + 0)), fpu_sub(lwc1(hero + 4), lwc1(last + 4))));
    vec_clear(V(first_angles));  // looking at the new home from the first point, 10 degrees down
    swc1(first_angles + 8, atan2_f(fpu_sub(lwc1(home + 0), lwc1(first + 0)), fpu_sub(lwc1(home + 4), lwc1(first + 4))));
    swc1(first_angles + 4, -0.17453292f);
    vec_clear(V(last_angles));  // at the path's first point from the last one, 20 degrees up
    swc1(last_angles + 8, atan2_f(fpu_sub(lwc1(path + 0x10), lwc1(last + 0)), fpu_sub(lwc1(path + 0x14), lwc1(last + 4))));
    swc1(last_angles + 4, 0.3490658402f);
    sw(sp + 0, 0);  // (the move's ninth word, on the original's stack)
    vf_pan_start_l18(lwu(pv + 0x334), first, last, first_angles, last_angles, hero, lw(L18_ADDR(0x00162384)),
                     lw(L18_ADDR(0x00162388)), 0);
}

// The last phase's end (phase 2 on, state 0x15): the sound stopped, animation 0, the gate farthest round from the hero
// (+0x330), the damage counts cleared.
void phase_last(uint32_t m, uint32_t pv) {
    sb(m + 0x20, 0x15);
    sound_stop(m, pv);
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
    sw(pv + 0x330, vf_gate_farthest_round_l18(pv + 0x3c0, HERO_AT, lw(pv + 0x200)));
    sw(pv + 0x394, 0);
    sw(pv + 0x390, 0);
}
}  // namespace

// The boss's preamble, every frame before its state: its size (the class's by L18:0x162330), the record its pvar word 0
// names by the hero's distance; the hit (mask 0x330000; its own rings, shells and walking bombs not thrown back do not
// count) through the hit reaction (the state and damage words in the original's frame, sp + 0xb0 / 0xb4); the damage in
// states 2 and 12 .. 19 (take_damage); its hit byte cleared, its health as an integer (+0x36c), the colour pulse; the
// attached mobys (class 1423 on its joint 4, class 1249 on that one's joint 0: the matrix through sp + 0x10); the target
// (target_select into +0x70 along the path +0x218, 512 far; the hero's moby when none); in states 12 .. 19 the lift
// (+0x21c): at its top the charge (state 0x13, its timers and burst), lost the scene 7 (state 7, then 0x19); then the
// phase change when the health crossed the phase's floor or L18:0x16238c asks it (that also starts again from phase 0
// with the rafts held), while 0x1415f8 is set.
// orig: L18:0x002f5e80
void vf_boss_preamble_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Frame f(0x140);
    const uint32_t sp = f.sp, st = sp + 0xb0, dmg = sp + 0xb4;
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L18_ADDR(0x00162330))));
    vf_record_by_hero_distance_l18(m, 0.5f);
    const float phase = static_cast<float>(lw(pv + 0x34c));
    const float floor = fpu_mul(fpu_div(fpu_sub(fpu_sub(7.0f, phase), 2.0f), 7.0f), 350.0f);
    sw(dmg, 0);
    uint32_t hit = sub_0025a420(m, NOT_ADDR(0x00330000u), 0);
    if (hit != 0) {
        const uint32_t by = lwu(hit + 0x20);
        if (by != 0) {
            const int16_t cls = lh(by + 0xa6);
            if (cls == 0x270 || cls == 0x234 || (cls == 0x238 && lbu(by + 0xbc) == 0)) hit = 0;
        }
    }
    {
        int32_t react_st = lw(st);  // (stale: the frame's word, as the original reads it)
        float react_dmg = lwc1(dmg);
        moby_hit_react_r(m, hit, pv + 0x20, 0, react_st, &react_dmg, nullptr, 4);
        sw(st, static_cast<uint32_t>(react_st));
        swc1(dmg, react_dmg);
    }
    bool crossed = false;
    if (lw(st) != 1) {
        const uint8_t state = lbu(m + 0x20);
        if (state == 2 || static_cast<uint32_t>(state - 12) < 8) crossed = take_damage(m, pv, hit, dmg, floor);
    }
    sb(m + 0xa4, 0xff);
    sw(pv + 0x36c, static_cast<uint32_t>(float_to_int(lwc1(pv + 0x38c))));
    color_pulse_update(m, pv + 0x60);
    attached_place(m, pv, 0x3a0, 0x58f, 0, 4, sp + 0x10);
    attached_place(m, pv, 0x3a4, 0x4e1, 0x3a0, 0, sp + 0x10);
    const uint32_t path = lvd::path(lw(pv + 0x218));
    target_select(m, pv + 0x70, 0, 0, path + 0x10, lw(path), 512.0f);
    if (lw(pv + 0xb0) == 0) sw(pv + 0xb0, lwu(HERO_MOBY));

    if (static_cast<uint32_t>(lbu(m + 0x20) - 12) < 8) {
        const int32_t lift = vf_lift_poll_l18(table_moby(lwu(pv + 0x21c)));
        if (lift == 1) {  // the lift at its top: the charge
            sb(m + 0x20, 0x13);
            if (lbu(m + 0x53) != 1) moby_anim_set(m, 1, 0, frame_scale_i(0x14));
            sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x258)));
            sw(pv + 0x364, 0);
            sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x14)));
            sw(pv + 0x338, vf_burst_spawn_l18(m, pv + 0x3c0, 5.8f));
        } else if (lift == -1) {  // the hero lost: scene 7, then state 0x19
            sb(m + 0x20, 7);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            cutscene_start(7);
            sw(pv + 0x39c, 0x19);
        }
    }

    if (!crossed && lw(L18_ADDR(0x0016238c)) == 0) return;
    if (lw(0x001415f8) == 0) return;
    if (lw(L18_ADDR(0x0016238c)) != 0) {  // the fight started again: phase 0, the rafts held
        sw(pv + 0x34c, 0);
        for (uint32_t k = 0; k < 4; ++k) vf_raft_hold_l18(table_moby(lwu(pv + 0x230 + k * 4)));
        sw(L18_ADDR(0x0016238c), 0);
    }
    if (lw(pv + 0x34c) < 2) phase_next(m, pv, sp);
    else phase_last(m, pv);
}

// The camera of the fight: the level camera's (+0x224) distance and height (its block's +0x34 / +0x38, sought at the
// speeds +0x378 / +0x37c: accelerations L18:0x1623b4 times the frame time's square, top speed L18:0x1623b0 times the
// frame time) between L18:0x1623c8 / 0x1623cc near and 0x1623c0 / 0x1623c4 far by the hero's distance from the home
// (L18:0x1623b8 .. 0x1623bc). The charge (state 0x13): 0x1623d0 / 0x1623d4 while the hero is in his mode 0xf or 0x42, the
// charge allowed, little damage taken (+0x390 <= 25), the timer +0x360 over; else 0x1623d8 / 0x1623dc with the hero
// within 28 of the arena's centre. States 6 and 9: 0x1623e0 / 0x1623e4. States 0, 1, 10, 11, 27: the first live type 0x12
// camera of the level's 48 flagged (its request let go); past state 1, once the hero rode a floating platform (+0x3ec)
// or with L18:0x1623a0 set, the scripted camera's distance and height set to 0x1623e0 / 0x1623e4 when the active one is
// of type 0. Any other state, while the boss lives: camera request 0x16 (key 0xffff, the level's three camera functions,
// the health +0x36c) every frame, its key in +0x368.
// orig: L18:0x002f7288
void vf_boss_camera_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t cam = lvd::camera_data(lw(pv + 0x224));
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x360)));
    const float lo = lwc1(L18_ADDR(0x001623b8)), hi = lwc1(L18_ADDR(0x001623bc));
    const float d = clamp_hi_lo(vec2_distance(V(HERO_AT), V(pv + 0x3c0)), lo, hi);
    const float k = fpu_div(fpu_sub(d, lo), fpu_sub(hi, lo));
    const float near_dist = lwc1(L18_ADDR(0x001623c8)), near_height = lwc1(L18_ADDR(0x001623cc));
    float dist = fpu_add(fpu_mul(fpu_sub(lwc1(L18_ADDR(0x001623c0)), near_dist), k), near_dist);
    float height = fpu_add(fpu_mul(fpu_sub(lwc1(L18_ADDR(0x001623c4)), near_height), k), near_height);
    const uint8_t state = lbu(m + 0x20);
    if (state == 0x13) {
        const bool close = lw(HERO_MODE) == 0xf || lw(0x001413d4) == 0x42;
        if (close && lw(pv + 0x364) == 0 && lwc1(pv + 0x390) <= 25.0f && lw(pv + 0x360) == 0) {
            dist = lwc1(L18_ADDR(0x001623d0));
            height = lwc1(L18_ADDR(0x001623d4));
        } else if (vec2_distance(V(HERO_AT), V(box_centre(lw(pv + 0x344)))) < 28.0f) {
            dist = lwc1(L18_ADDR(0x001623d8));
            height = lwc1(L18_ADDR(0x001623dc));
        }
    } else if (state == 9 || state == 6) {
        dist = lwc1(L18_ADDR(0x001623e0));
        height = lwc1(L18_ADDR(0x001623e4));
    } else if (state == 0 || state == 0xa || state == 0xb || state == 1 || state == 0x1b) {
        if (lw(pv + 0x3ec) == 0) {  // (the original tests it again in every turn of the loop below: the same answer)
            const uint32_t ride = lwu(0x0013f64c);
            if (ride != 0 && lh(ride + 0xa6) == 0x24b) sw(pv + 0x3ec, 1);
        }
        for (uint32_t i = 0; i < 48; ++i) {  // (the level's live cameras: the first of type 0x12)
            if (lw(L18_ADDR(0x0016a290) + i * 4) == 0) continue;
            const uint32_t c = L18_ADDR(0x00167b50) + i * 0xa0;
            if (lh(c + 0x86) != 0x12) continue;
            camera_type12_flag(c);
            camera_release(pv);
            if (state < 2) break;
            if (lw(pv + 0x3ec) == 0 && lw(L18_ADDR(0x001623a0)) == 0) break;
            const uint32_t active = lwu(L18_ADDR(0x00167680) + 0x180);
            if (active != 0 && lh(active + 0x86) == 0) {
                cam0_set_distance(0, lwc1(L18_ADDR(0x001623e0)), 0.003f);
                cam0_set_height(lwc1(L18_ADDR(0x001623e4)), 0.003f);
            }
            break;
        }
    } else if (0.0f < lwc1(pv + 0x38c)) {
        sw(pv + 0x368, static_cast<uint32_t>(camera_request(0x16, 0xffff, L18_ADDR(0x00239360), L18_ADDR(0x00239480),
                                                            L18_ADDR(0x0023cd60), pv + 0x36c, 0x15e)));
    }
    const float accel = fpu_mul(lwc1(L18_ADDR(0x001623b4)), lwc1(0x0015ed70));
    const float top = fpu_mul(lwc1(L18_ADDR(0x001623b0)), lwc1(0x0015ed6c));
    seek_1d(cam + 0x34, pv + 0x378, dist, accel, accel, top);
    seek_1d(cam + 0x38, pv + 0x37c, height, accel, accel, top);
}

// The smoke and sparks at the boss's mouth (the EE point `at`), along its facing (the x axis of its matrix +0xc0): its
// byte +0xbc counted down; L18:0x162408 puffs (dev_2712b8: 0x16240c .. 0x162410 out, spin by 0x162420 .. 0x162424, life
// 0x162428 .. 0x16242c frames scaled, sizes 0x162414 / 0x162418 / 0x162430 / 0x16241c) and L18:0x162438 sparks (particle
// 0x15: 0x16243c .. 0x162440 out, life 0x162444 .. 0x162448, colours 0x1623ec / 0x1623f0, size 0x16244c). The facing and
// each particle's offset in the original's frame (sp + 0, sp + 0x30).
// orig: L18:0x002f76a0
void vf_boss_smoke_l18(uint32_t m, uint32_t at) {
    Frame f(0xa0);
    const uint32_t dir = f.sp, out = f.sp + 0x30;
    swc1(dir + 0, 1.0f);  // (1, 0, 0, 1)
    sw(dir + 4, 0);
    sw(dir + 8, 0);
    swc1(dir + 0xc, 1.0f);
    countdown_u8(m + 0xbc);
    // (the original also turns (0.23, 0, 0, 1) by the matrix and adds the moby's position into sp + 0x10 / 0x20, which
    // nothing reads: left out)
    mat3_transform(V(dir), V(dir), M(m + 0xc0));
    for (int32_t i = 0; i < lw(L18_ADDR(0x00162408)); ++i) {
        const float len = rand_range_f(lwc1(L18_ADDR(0x0016240c)), lwc1(L18_ADDR(0x00162410)));
        vec3_set_length(V(out), V(dir), len);
        const float spin = rand_range_signed_f(static_cast<float>(lw(L18_ADDR(0x00162420))),
                                               static_cast<float>(lw(L18_ADDR(0x00162424))));
        const int32_t turn = float_to_int(spin);
        const int32_t life = frame_scale_i(rand_range(lw(L18_ADDR(0x00162428)), lw(L18_ADDR(0x0016242c))));
        dev_2712b8(at, out, static_cast<uint32_t>(life), 0x7f, 0xffffffu, static_cast<uint32_t>(turn),
                   lwc1(L18_ADDR(0x00162414)), lwc1(L18_ADDR(0x00162418)), lwc1(L18_ADDR(0x00162430)),
                   lwc1(L18_ADDR(0x0016241c)), 0.85f);
    }
    for (int32_t i = 0; i < lw(L18_ADDR(0x00162438)); ++i) {
        const float len = rand_range_f(lwc1(L18_ADDR(0x0016243c)), lwc1(L18_ADDR(0x00162440)));
        vec3_set_length(V(out), V(dir), len);
        const int32_t life = frame_scale_i(rand_range(lw(L18_ADDR(0x00162444)), lw(L18_ADDR(0x00162448))));
        spawn_particle_15_v(V(at), V(out), lwu(L18_ADDR(0x001623ec)), lwu(L18_ADDR(0x001623f0)),
                            static_cast<uint32_t>(life), 1, lwc1(L18_ADDR(0x0016244c)));
    }
}

}  // namespace rac1
