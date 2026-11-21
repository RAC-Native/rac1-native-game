// Veldin's finale (level 18): the boss (class 1422, Drek's ship), hand-written (part 4 of its port): its update, a state
// machine of 28 states (the jump table L18:0x1f2b50 read as a switch on the state byte). Every frame: the hero's ride on a
// floating platform counted, the preamble (hits, health, the attached mobys, the target, the phase change:
// veldin_finale_boss_frame.cpp), the scene's Drek model scaled, the attached Drek's joint scale, the floor band near the
// camera; then the state; then the tail (the fight's camera, the exhaust smoke, its home, hover and springs, its glow and
// its draw). Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR (the fight's words
// L18:0x162360 .. 0x16239c, the scene record L18:0x16d260), the base ELF's bare (the hero 0x13f350, his moby 0x1413d0, the
// frame time 0x15ed6c, its square 0x15ed70 and 0x15ed64). Declared in veldin_finale_mobys.h.
//
// The states: 0 the set-up; 1 waiting for the hero (scene 0 for Giant Clank, the polygon, the three boxes: the fight's
// phases 0 .. 2, the end); 2 the fight on foot (scene 1 at the phase change); 3 waiting for the camera move; 4 the hero
// off the floating platforms (scene 2); 5 the phase-3 camera move; 7 a scene running; 8 waiting for the camera move's
// end; 9 the energy ball; 10 the flight to a floating platform; 11 back to the arena; 12 the attack chooser; 13 the rings;
// 14 the energy ball again; 15 the shell barrage; 16 the bomb throw; 17 the robots; 18 the lunge; 19 the charge; 20 the
// divers; 21 the phase-2 end; 26 the death; 27 the end (scenes 3 / 4 / 5, the credits, movie 0xb, menu 0x21). States 6
// and 22 .. 25 run the tail only.
//
// The pvars (besides those veldin_finale_boss_frame.cpp names): +0xb0 the target's moby (the record +0x70), +0x1c0 the
// Drek model's joint record, +0x200 the gates' group, +0x204 .. 0x20c the floating platforms' groups, +0x210 the robots'
// group, +0x214 the divers' group, +0x220 the camera move's moby index, +0x250 .. 0x25c / +0x294 / +0x298 / +0x2a0 boxes,
// +0x260 .. 0x26c paths, +0x270 the polygon's path, +0x274 the Giant Clank box, +0x280 .. 0x288 the lunge's boxes, +0x29c
// the bombs' group, +0x2b0 the checkpoint, +0x320 the energy ball's goal, +0x330 the gate, +0x33c the energy ball,
// +0x340 / +0x348 the path flown and its point, +0x350 the last attack, +0x354 .. 0x35c timers, +0x3a8 the lunge's box,
// +0x3ac the end's count, +0x3b4 the glow's phase, +0x3b8 its colour, +0x3bc the rings' spread, +0x3d0 .. 0x3e0 the hover.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/credits_tr.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/movie_mode.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/region_core.h>
#include <game/ovl/shared_flash.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;      // the hero's block
constexpr uint32_t HERO_AT = 0x0013f3d0;   // his position (+0x80)
constexpr uint32_t HERO_MODE = 0x001413dc;  // his movement mode (+0x208c)
constexpr uint32_t SOUND_SLOTS = 0x0013e550;
constexpr float TWO_PI = 6.2831855f;

float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // its square
uint32_t table_moby(uint32_t index) { return lwu(L18_ADDR(0x0015ffd8)) + (index << 8); }
uint32_t box_centre(int32_t i) { return lvd::box(lvd::BOXES, i) + 0x30; }
uint32_t path_last(uint32_t path) { return path + (lwu(path) << 4); }  // a path's last point

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

// animation `set` started over `frames` frames (scaled) unless it plays already
void anim_to(uint32_t m, uint32_t set, int32_t frames) {
    if (lbu(m + 0x53) != set) moby_anim_set(m, set, 0, frame_scale_i(frames));
}

// the boss turned toward the point (x, y) (yaw_seek on +0x48 with the speed +0x370: a turn a frame's square of 2 pi)
void face(uint32_t m, uint32_t pv, float x, float y) {
    const float yaw = atan2_f(fpu_sub(x, lwc1(m + 0x10)), fpu_sub(y, lwc1(m + 0x14)));
    const float turn = fpu_mul(dt_sq(), TWO_PI);
    yaw_seek(m + 0x48, pv + 0x370, yaw, turn, turn, fpu_mul(dt(), TWO_PI));
}
void face(uint32_t m, uint32_t pv, uint32_t at) { face(m, pv, lwc1(at + 0), lwc1(at + 4)); }

// the boss shown with its class's colour again (flags 0x41 off)
void show_again(uint32_t m) {
    sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
}

// the boss's sound slot (+0x3b0) stopped while it still plays for the boss, forgotten (-1)
void slot_stop(uint32_t m, uint32_t pv) {
    const int32_t slot = lw(pv + 0x3b0);
    if (slot != -1) {
        const uint32_t e = SOUND_SLOTS + static_cast<uint32_t>(slot * 0x70);
        if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
    }
    sw(pv + 0x3b0, 0xffffffffu);
}

// the same when sub_0028d8c0 says the boss's sound runs
void sound_stop(uint32_t m, uint32_t pv) {
    if (sub_0028d8c0(m, lw(pv + 0x3b0)) != 0) slot_stop(m, pv);
}

// one of the boss's phases started from state 1 (the phase's path +0x260 + 4k: its last point the home), the health
// scaled by `share`, the arena box `box`
void phase_start(uint32_t m, uint32_t pv, int32_t phase, uint32_t path_word, float share, uint32_t box_word) {
    show_again(m);
    if (phase >= 0) sw(pv + 0x34c, static_cast<uint32_t>(phase));
    swc1(pv + 0x38c, fpu_mul(lwc1(pv + 0x38c), share));
    sb(m + 0x20, 0xc);
    const uint32_t path = lvd::path(lw(pv + path_word));
    sw(pv + 0x340, path);
    lq_sq(pv + 0x3c0, path_last(path));
    sw(pv + 0x344, lwu(pv + box_word));
}

// a shell at the hero (states 2 and 15): from the joint point `from` (sp + 0x30) toward `to` (sp + 0x20), launched along
// the boss's facing at 20 a frame (sp + 0x40), its yaw 5 .. 30 degrees either way, its pitch 5 .. 10 down, its frames
// three times the hero's distance (60 .. 120, scaled), the flags `flags`
void aimed_shell(uint32_t m, uint32_t from, uint32_t vel, uint32_t to, uint32_t flags) {
    vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(dt(), 20.0f));
    const float yaw = rand_range_signed_f(u2f(0x3db2b8c2u), u2f(0x3f060a92u));
    const float pitch = -rand_range_f(u2f(0x3db2b8c2u), u2f(0x3e32b8c2u));
    const float n = clamp_hi_lo(fpu_mul(vec2_distance(V(m + 0x10), V(HERO_AT)), 3.0f), 60.0f, 120.0f);
    vf_shell_spawn_l18(from, vel, to, float_to_int(frame_scale_f(n)), flags, yaw, pitch);
}

// the boss's home stepped toward the point `pt` of the path it flies (+0x340 / +0x348) at its speed +0x374: the next
// point when it is reached
void path_step(uint32_t pv, uint32_t step) {
    const uint32_t pt = lwu(pv + 0x340) + (lwu(pv + 0x348) << 4) + 0x10;
    vec3_sub(V(step), V(pt), V(pv + 0x3c0));
    const float speed = lwc1(pv + 0x374);
    if (vec3_length(V(step)) <= speed) sw(pv + 0x348, lwu(pv + 0x348) + 1);
    else vec3_set_length(V(step), V(step), speed);
    vec3_add(V(pv + 0x3c0), V(pv + 0x3c0), V(step));
}

// ---- the states

// 0: the set-up (home = its place, shown, health 350, the first phase's path and platforms, the camera move's moby; the
// three platform groups hidden)
void state_setup(uint32_t m, uint32_t pv) {
    lq_sq(pv + 0x3c0, m + 0x10);
    const uint32_t flags = lhu(m + 0x34) | 0x41u;
    sb(m + 0x20, 1);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0x200);
    sh(m + 0x34, flags);
    sw(m + 0x94, 0);
    sb(pv + 0x28, 3);
    swc1(pv + 0x20, 500.0f);
    swc1(pv + 0x30, 4.0f);
    sh(pv + 0x24, 0x1f4);
    sw(pv + 0x340, lvd::path(lw(pv + 0x26c)));
    sw(pv + 0x398, lwu(pv + 0x204));
    sw(pv + 0x348, 1);
    sw(pv + 0x368, 0xffffffffu);
    swc1(pv + 0x38c, 350.0f);
    sw(pv + 0x334, table_moby(lwu(pv + 0x220)));
    swc1(pv + 0x3d0, 0.75f);
    sw(pv + 0x374, 0);
    for (uint32_t k = 0; k < 3; ++k) vf_group_hide_l18(lw(pv + 0x204 + k * 4));
}

// 1: waiting for the hero
void state_wait(uint32_t m, uint32_t pv) {
    if (point_in_box(HERO_AT, lw(pv + 0x274)) != 0) {
        const uint8_t form = lbu(0x001413f4);
        if (form == 2) {  // Giant Clank in the box: scene 0
            sb(m + 0x20, 7);
            sh(m + 0x34, lhu(m + 0x34) | 0x41u);
            cutscene_start(0);
            sw(pv + 0x39c, form);
            sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x258)));
            sw(pv + 0x344, lwu(pv + 0x25c));
            anim_to(m, 0xa, 0xa);
            return;
        }
    }
    const uint32_t poly = lvd::path(lw(pv + 0x270));
    if (point_in_polygon(HERO_AT, poly + 0x10, lw(poly)) != 0) {  // the hero on the floating platforms' side
        show_again(m);
        sb(m + 0x20, 0xa);
        const uint32_t path = lvd::path(lw(pv + 0x260));
        sw(pv + 0x340, path);
        lq_sq(pv + 0x3c0, path + 0x10);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(HERO_AT + 0), lwc1(pv + 0x3c0)), fpu_sub(lwc1(HERO_AT + 4), lwc1(pv + 0x3c4))));
        vf_group_show_l18(lw(pv + 0x204));
        swc1(pv + 0x38c, fpu_mul(lwc1(pv + 0x38c), u2f(0x3f5b6db7u)));  // (6 / 7)
        return;
    }
    if (vec3_distance(V(HERO_AT), V(box_centre(lw(pv + 0x250)))) < 50.0f) {  // phase 0
        phase_start(m, pv, -1, 0x260, u2f(0x3f5b6db7u), 0x250);
        return;
    }
    if (vec3_distance(V(HERO_AT), V(box_centre(lw(pv + 0x254)))) < 50.0f) {  // phase 1
        phase_start(m, pv, 1, 0x264, u2f(0x3f36db6eu), 0x254);  // (5 / 7)
        return;
    }
    if (!(vec3_distance(V(HERO_AT), V(box_centre(lw(pv + 0x258)))) < 50.0f)) return;
    if (lw(L18_ADDR(0x001623a0)) != 0) {  // the fight won already: the end (state 27)
        vf_gates_open_l18(lw(pv + 0x200));
        sound_stop(m, pv);
        const uint32_t flags = lhu(m + 0x34);
        sb(m + 0x20, 0x1b);
        sw(m + 0x94, 0);
        sh(m + 0x34, flags | 0x41u);
        sw(pv + 0x34c, 7);
        return;
    }
    phase_start(m, pv, 2, 0x268, u2f(0x3f124925u), 0x258);  // phase 2 (4 / 7)
    sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x168)));
}

// 2: the fight on foot: turned to the hero, the orbit; at 301 health the phase change (scene 1: the hero placed at box
// +0x2a0, the checkpoint); else, in animation 0xa, a shell at its frames 1 and 16 when the hero's point is level with it
void state_on_foot(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    vf_boss_orbit_l18(m);
    if (lwc1(pv + 0x38c) <= 301.0f) {
        if (lbu(0x001413f4) != 0) {
            sw(lwu(0x001413d0) + 0x94, 0);
            const uint32_t hm = lwu(0x001413d0);
            sh(hm + 0x34, lhu(hm + 0x34) | 0x41u);
        }
        hero_mode_exit();
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x2a0));
        hero_place(box + 0x30, box + 0x70, 0, 1);
        checkpoint_mark_collected(lwu(pv + 0x2b0));
        checkpoint_save(HERO + 0x80, HERO + 0x90);
        vf_group_show_l18(lw(pv + 0x204));
        cutscene_start(1);
        const uint32_t flags = lhu(m + 0x34) | 0x41u;
        sb(m + 0x20, 7);
        sh(m + 0x34, flags);
        if (lbu(m + 0x53) != 0xa) moby_anim_set(m, 0xa, 0, frame_scale_i(0x14));
        sw(pv + 0x39c, 0xa);
        sw(pv + 0x348, 0);
        sw(pv + 0x374, 0);
        const uint32_t path = lvd::path(lw(pv + 0x260));
        sw(pv + 0x340, path);
        lq_sq(pv + 0x3c0, path + 0x10);
        swc1(m + 0x48, atan2_f(fpu_sub(lwc1(HERO + 0x80), lwc1(pv + 0x3c0)), fpu_sub(lwc1(HERO + 0x84), lwc1(pv + 0x3c4))));
        sw(pv + 0x368, 0xffffffffu);
        return;
    }
    if (lbu(m + 0x53) == 0xa && (anim_crossed(m, 1.0f) != 0 || anim_crossed(m, 16.0f) != 0)) {
        const uint32_t to = sp + 0x20, from = sp + 0x30;
        lq_sq(to, HERO + 0xc0);
        if (abs_f(fpu_sub(lwc1(to + 8), lwc1(pv + 0x3c8))) < 5.0f) {
            const float frame = static_cast<float>(static_cast<int32_t>(lbu(m + 0x50)));
            moby_chain_point_v(m, frame < 15.0f ? 2 : 3, V(from));
            aimed_shell(m, from, sp + 0x40, to, 0);
        }
    }
    if ((lbu(m + 0x70) & 2) != 0) anim_to(m, 0xa, 0x14);
}

// 3: waiting for the camera move (class 644 in its state 3): the new home, the next phase's platforms shown and the
// last one's hidden, the energy ball spawned at joint 1 (300 frames); state 9
void state_pan_wait(uint32_t m, uint32_t pv, uint32_t sp) {
    if (state_query_finale(lwu(pv + 0x334)) != 2) return;
    lq_sq(pv + 0x3c0, pv + 0x300);
    lq_sq(m + 0x40, pv + 0x310);
    sb(m + 0x20, 9);
    anim_to(m, 7, 0x14);
    vf_group_show_l18(lw(pv + 0x398));
    vf_group_hide_l18(lw(pv + 0x204 + static_cast<uint32_t>(lw(pv + 0x34c) - 1) * 4));
    moby_chain_point_v(m, 1, V(sp + 0x20));
    sw(pv + 0x33c, vf_ball_spawn_l18(m, sp + 0x20, 0x12c));
}

// the hero on the floor (not on a floating platform, not hanging: 0x13f65e)
bool hero_off_platforms() {
    if (lh(0x0013f65e) != 0) return false;
    const uint32_t ride = lwu(0x0013f64c);
    return ride == 0 || lh(ride + 0xa6) != 0x24b;
}

// 4: the hero back on the floor: scene 2 (the nearest gate, the platform groups hidden), then state 5
void state_hero_down(uint32_t m, uint32_t pv) {
    if (!hero_off_platforms()) return;
    sw(L18_ADDR(0x00162360), 1);
    sb(m + 0x20, 7);
    sw(pv + 0x39c, 5);
    sw(pv + 0x330, vf_gate_nearest_l18(HERO + 0x80, lw(pv + 0x200)));
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    cutscene_start(2);
    for (uint32_t k = 0; k < 3; ++k) vf_group_hide_l18(lw(pv + 0x204 + k * 4));
}

// 5: the phase-3 camera move: from 6 beside the lift (+0x21c) round to 45 degrees, the hero's box +0x298, Clank's
// line first (the ninth word 1); the lift sent up (1800 frames); state 8
void state_lift_pan(uint32_t m, uint32_t pv, uint32_t sp) {
    const uint32_t lift_at = table_moby(lwu(pv + 0x21c)) + 0x10;
    sw(L18_ADDR(0x00162390), 0);
    const uint32_t first = sp + 0x20, last = sp + 0x30, first_angles = sp + 0x40, last_angles = sp + 0x50, hero = sp + 0x60;
    swc1(first + 0, fpu_mul(cos_vu(0.0f), 6.0f));
    swc1(first + 8, 0.0f);
    swc1(first + 4, fpu_mul(sin_vu(0.0f), 6.0f));
    swc1(last + 0, fpu_mul(cos_vu(u2f(0x3f490fdbu)), 6.0f));
    swc1(last + 8, 0.0f);
    swc1(last + 4, fpu_mul(sin_vu(u2f(0x3f490fdbu)), 6.0f));
    vec3_add(V(first), V(first), V(lift_at));
    vec3_add(V(last), V(last), V(lift_at));
    swc1(first + 8, fpu_add(lwc1(first + 8), 5.0f));
    swc1(last + 8, fpu_add(lwc1(last + 8), 7.0f));
    vec_clear(V(first_angles));
    swc1(first_angles + 8, u2f(0x40490fdbu));  // (pi)
    vec_clear(V(last_angles));
    swc1(last_angles + 8, u2f(0x407b53d1u));  // (5 pi / 4)
    swc1(first_angles + 4, u2f(0x3e32b8c2u));
    swc1(last_angles + 4, u2f(0x3f060a92u));
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x298));
    lq_sq(hero, box + 0x30);
    swc1(hero + 0xc, lwc1(box + 0x78));
    sw(sp + 0, 1);  // (the move's ninth word, on the original's stack)
    vf_pan_start_l18(lwu(pv + 0x334), first, last, first_angles, last_angles, hero, lw(L18_ADDR(0x00162394)),
                     lw(L18_ADDR(0x00162398)), 1);
    sw(0x00141414, 3);
    sb(m + 0x20, 8);
    vf_lift_start_l18(table_moby(lwu(pv + 0x21c)), frame_scale_i(0x708));
}

// 7: a scene running (the level's mode 2): by the scene's number and frame (the record L18:0x16d260: +0x30, +0x34)
// scene 2 opens the gate +0x330 between frames 2600 and 2870, scene 3 the gates between 950 and 960 and saves the
// checkpoint at box +0x294 once (L18:0x1623a0). Once the scene is over: shown again, the state after it (+0x39c; 5: the
// frame scale 0x15f3fc back to 1; 0x19: the hero lost).
void state_scene(uint32_t m, uint32_t pv) {
    const int32_t mode = lw(L18_ADDR(0x0015f5c4));
    if (mode != 2) {
        show_again(m);
        const uint8_t after = lbu(pv + 0x39c);
        sb(m + 0x20, after);
        if (after == 5) swc1(L18_ADDR(0x0015f3fc), 1.0f);
        if (lbu(m + 0x20) == 0x19) hero_lost();
        return;
    }
    const uint32_t scene = L18_ADDR(0x0016d260);
    const int32_t number = lw(scene + 0x30);
    if (number == mode) {
        if (lw(scene + 0x34) < frame_scale_i(0xa28)) return;
        if (frame_scale_i(0xb36) < lw(scene + 0x34)) return;
        vf_gate_open_l18(lwu(pv + 0x330));
        return;
    }
    if (number != 3) return;
    if (frame_scale_i(0x3b6) < lw(scene + 0x34) && lw(scene + 0x34) < frame_scale_i(0x3c0)) vf_gates_open_l18(lw(pv + 0x200));
    if (lw(L18_ADDR(0x001623a0)) != 0) return;
    const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x294));
    checkpoint_save(box + 0x30, box + 0x70);
    sw(L18_ADDR(0x001623a0), 1);
}

// 8: waiting for the camera move's end (flags 0x41 while it runs): state 12, animation 0
void state_pan_end(uint32_t m, uint32_t pv) {
    sh(m + 0x34, lhu(m + 0x34) | 0x41u);
    if (!(1.0f <= vf_pan_progress_l18(lwu(pv + 0x334)))) return;
    const uint32_t flags = lhu(m + 0x34) & 0xffbeu;
    sb(m + 0x20, 0xc);
    sh(m + 0x34, flags);
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0xa));
}

// 9: the energy ball's phase: to the path's first point (20 at most); the ball held at joint 1 and launched at the arena
// once the camera move is between 0.5 and 0.7 (animation 9); past 0.729 animation 0, state 10 when home, the phase's
// four rafts sunk
void state_ball(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    lq_sq(sp + 0x20, lwu(pv + 0x340) + 0x10);
    const uint32_t there = vf_boss_home_step_l18(m, sp + 0x20, 20.0f);
    const float p = vf_pan_progress_l18(lwu(pv + 0x334));
    if (lbu(m + 0x53) == 7) {
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 8, 0, frame_scale_i(0xa));
    } else {
        vec3_sub(V(sp + 0x30), V(lwu(pv + 0x340) + 0x10), V(pv + 0x3c0));
        vec3_set_length(V(sp + 0x30), V(sp + 0x30), fpu_mul(dt(), 5.0f));
        vec3_add(V(pv + 0x3c0), V(pv + 0x3c0), V(sp + 0x30));
    }
    if (0.5f < p && p < 0.7f) {
        if (lbu(m + 0x53) != 9) moby_anim_set(m, 9, 0, 0);
        if (lw(pv + 0x33c) == 0) return;
        moby_chain_point_v(m, 1, V(sp + 0x30));
        lq_sq(sp + 0x40, box_centre(lw(pv + 0x344)));
        swc1(sp + 0x48, fpu_add(lwc1(sp + 0x48), 10.0f));
        swc1(sp + 0x48, floor_probe_v(0.5f, V(sp + 0x40), 0));  // (its result is not used: the ball goes to the box's centre)
        vf_ball_launch_l18(lwu(pv + 0x33c), sp + 0x30, box_centre(lw(pv + 0x344)), 1, 1.0f);
        sw(pv + 0x33c, 0);
        return;
    }
    if (u2f(0x3f3a9fbeu) <= p) {  // (0.729)
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
        if (there != 0) sb(m + 0x20, 0xa);
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = lw(pv + 0x34c) == 1 ? lwu(pv + 0x230 + k * 4) : lwu(pv + 0x240 + k * 4);
            vf_raft_sink_l18(table_moby(idx));
        }
    }
    if (lw(pv + 0x33c) == 0) return;
    moby_chain_point_v(m, 1, V(sp + 0x30));
    float k = fpu_add(p, p);
    if (1.0f < k) k = 1.0f;
    vf_ball_launch_l18(lwu(pv + 0x33c), sp + 0x30, L18_ADDR(0x0015f580), 0, k);
}

// 10: the flight to a floating platform along the path +0x340 (speed up to 12 a frame); shells at the platform it picks
// at its animation's frames 1 and 16; past the path's last but one point animation 0 and state 11
void state_fly(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    approach_f(pv + 0x374, fpu_mul(dt(), 12.0f), fpu_mul(dt_sq(), 20.0f));
    if (lwc1(pv + 0x374) != 0.0f) path_step(pv, sp + 0x20);
    const uint32_t fl = vf_boss_float_pick_l18(m);
    if (fl != 0 && (anim_crossed(m, 1.0f) != 0 || anim_crossed(m, 16.0f) != 0)) {
        const uint32_t at = sp + 0x20, vel = sp + 0x30, to = sp + 0x40;
        lq_sq(to, fl + 0x10);
        lq_sq(at, pv + 0x3c0);
        vec3_add(V(at), V(at), V(m + 0xc0));
        swc1(at + 8, fpu_add(lwc1(at + 8), 6.0f));
        vec3_set_length(V(vel), V(m + 0xc0), fpu_mul(dt(), 10.0f));
        const float yaw = rand_range_signed_f(u2f(0x3f060a92u), u2f(0x3f490fdbu));
        const float pitch = -rand_range_f(u2f(0x3eb2b8c2u), u2f(0x3f060a92u));
        vf_shell_spawn_l18(at, vel, to, frame_scale_i(0x3c), 5, yaw, pitch);
    }
    if (lw(pv + 0x348) != lw(lwu(pv + 0x340)) - 2) return;
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
    sb(m + 0x20, 0xb);
}

// 11: back to the arena along the path (speed up to 10 a frame); at its end, with the hero within 50 and on the floor:
// home at the path's last point, the phase's arena box; state 12 (phases 0, 1), 4 (phase 2 before the hero went down)
// or 0x15 (the gate farthest round)
void state_return(uint32_t m, uint32_t pv, uint32_t sp) {
    if (lw(pv + 0x348) != lw(lwu(pv + 0x340))) {
        approach_f(pv + 0x374, fpu_mul(dt(), 10.0f), fpu_mul(dt_sq(), 20.0f));
        path_step(pv, sp + 0x20);
        return;
    }
    if (!(vec2_distance(V(m + 0x10), V(HERO_AT)) < 50.0f)) return;
    if (!hero_off_platforms()) return;
    sw(pv + 0x374, 0);
    lq_sq(pv + 0x3c0, path_last(lwu(pv + 0x340)));
    const int32_t phase = lw(pv + 0x34c);
    sw(pv + 0x344, lwu(pv + 0x250 + static_cast<uint32_t>(phase) * 4));
    if (phase != 2) {
        sb(m + 0x20, 0xc);
        return;
    }
    if (lw(L18_ADDR(0x00162360)) == 0) {
        sb(m + 0x20, 4);
        return;
    }
    sb(m + 0x20, 0x15);
    sw(pv + 0x330, vf_gate_farthest_round_l18(pv + 0x3c0, HERO_AT, lw(pv + 0x200)));
}

// 12: the attack chooser: turned to the target, the orbit; at the animation's end the next attack (the table
// L18:0x1f2bc0 on the attack - 13: 18 and any other go to the bomb throw), its animation and timers
void state_choose(uint32_t m, uint32_t pv, uint32_t sp, uint32_t target) {
    face(m, pv, target + 0x10);
    vf_boss_orbit_l18(m);
    if ((lbu(m + 0x70) & 2) == 0) return;
    const int32_t next = vf_boss_next_attack_l18(m);
    switch (static_cast<uint32_t>(next - 13)) {  // (the jump table L18:0x1f2bc0)
    case 0: {  // (0x002f41e4) the rings
        sb(m + 0x20, 0xd);
        anim_to(m, 2, 0x14);
        sw(pv + 0x354, 0);
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(rand_mod(3) * 0x5a + 0x14a)));
        sw(pv + 0x35c, static_cast<uint32_t>(frame_scale_i((6 - lw(pv + 0x34c)) * 0x1e)));
        swc1(pv + 0x3bc, rand_range_f(u2f(0xbeaa7efau), u2f(0x3eaa7efau)));
        break;
    }
    case 1: {  // (0x002f4308) the energy ball, after the lunge to the lunge box farthest round from the hero
        sb(m + 0x20, 0x12);
        if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
        float best = 0.0f;
        const uint32_t c = sp + 0x20;
        for (uint32_t k = 0; k < 3; ++k) {  // (the bearing from the hero and from the box, both seen from the home)
            lq_sq(c, box_centre(lw(pv + 0x280 + k * 4)));
            const float from_hero = atan2_f(fpu_sub(lwc1(pv + 0x3c0), lwc1(HERO + 0x80)), fpu_sub(lwc1(pv + 0x3c4), lwc1(HERO + 0x84)));
            const float from_box = atan2_f(fpu_sub(lwc1(pv + 0x3c0), lwc1(c + 0)), fpu_sub(lwc1(pv + 0x3c4), lwc1(c + 4)));
            const float d = angle_dist(from_hero, from_box);
            if (best < d) {
                best = d;
                sw(pv + 0x3a8, lwu(pv + 0x280 + k * 4));
            }
        }
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x384)));
        sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0xb4)));
        break;
    }
    case 2:  // (0x002f4194) the shell barrage
        sb(m + 0x20, 0xf);
        anim_to(m, 0xa, 0x14);
        sw(pv + 0x354, 0);
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x258)));
        sw(pv + 0x35c, static_cast<uint32_t>(frame_scale_i(0x168)));
        break;
    case 4:  // (0x002f4274) the robots
        sb(m + 0x20, 0x11);
        anim_to(m, 0xb, 0x14);
        sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0xa)));
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x258)));
        break;
    case 6: {  // (0x002f43f8) the charge
        sb(m + 0x20, 0x13);
        anim_to(m, 1, 0x14);
        sw(pv + 0x354, vec2_distance(V(m + 0x10), V(HERO_AT)) < 16.0f ? static_cast<uint32_t>(frame_scale_i(0x5a)) : 0u);
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x258)));
        sw(pv + 0x35c, static_cast<uint32_t>(frame_scale_i(0x258)));
        sw(pv + 0x338, vf_burst_spawn_l18(m, pv + 0x3c0, 5.8f));
        break;
    }
    default:  // (0x002f42b8: 16, 18 and outside the table) the bomb throw
        sb(m + 0x20, 0x10);
        anim_to(m, 6, 0x14);
        sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0xf)));
        sw(pv + 0x358, static_cast<uint32_t>(frame_scale_i(0x168)));
        break;
    }
}

// the attack's end: back to the chooser (state 12), the attack kept as the last one (+0x350)
void attack_done(uint32_t m, uint32_t pv, uint32_t attack) {
    sw(pv + 0x350, attack);
    sb(m + 0x20, 0xc);
}

// 13: the rings: animation 2 (its end: 4), at animation 4's frame 1 a ring (class 624) from joint 0 toward the target
// along the facing turned by the spread +0x3bc (half the target's distance, at most 30, a frame), its smoke, a new spread;
// otherwise animation 3; done when both timers +0x358 / +0x35c run out (animation 5)
void state_rings(uint32_t m, uint32_t pv, uint32_t sp, uint32_t target) {
    face(m, pv, target + 0x10);
    vf_boss_orbit_l18(m);
    if (lw(pv + 0x354) < lw(pv + 0x358) + lw(pv + 0x35c)) swc1(pv + 0x128, atan2_f(1.0f, lwc1(pv + 0x3bc)));
    if (lbu(m + 0x53) == 2) {
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 4, 0, frame_scale_i(0x14));
    } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) != 0 && lw(pv + 0x358) != 0) {
        anim_to(m, 4, 0x14);
    }
    if (lbu(m + 0x53) == 4) {
        if (anim_crossed(m, 1.0f) != 0) {
            sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x5a)));
            sw(pv + 0x360, static_cast<uint32_t>(frame_scale_i(0x12c)));
            const uint32_t at = sp + 0x20, dir = sp + 0x30, side = sp + 0x40;
            moby_chain_point_v(m, 0, V(at));
            lq_sq(dir, m + 0xc0);
            vec3_set_length(V(side), V(m + 0xd0), lwc1(pv + 0x3bc));
            vec3_add(V(dir), V(dir), V(side));
            const float d = clamp_hi_lo(fpu_mul(vec2_distance(V(at), V(target + 0x10)), 0.5f), 0.0f, 30.0f);
            vec3_set_length(V(dir), V(dir), fpu_mul(d, dt()));
            vf_ring_spawn_l18(at, dir, m, 64.0f, fpu_mul(dt(), 18.0f));
            vf_boss_smoke_l18(m, at);
            swc1(pv + 0x3bc, rand_range_signed_f(0.133f, 0.333f));
        } else if ((lbu(m + 0x70) & 2) != 0) {
            anim_to(m, 3, 0x14);
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358))) == 0) return;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x35c))) == 0) return;
    attack_done(m, pv, 0xd);
    anim_to(m, 5, 0x14);
}

// 14: the energy ball again: turned to its goal +0x320, the bearing to it from joint 0x13 into the second spring's record
// (+0x140), the hover down 3; animation 8; with +0x354 running: the ball spawned at joint 1 (L18:0x16239c frames), held
// there (the launch's share by the timer, its goal the target's place until the last 15 frames), at the timer's end
// thrown at 180 beyond the goal (animation 9, 120 frames again); done when +0x358 is over
void state_ball_again(uint32_t m, uint32_t pv, uint32_t sp, uint32_t target) {
    face(m, pv, pv + 0x320);
    const uint32_t mouth = sp + 0x20, at = sp + 0x30;
    moby_chain_point_v(m, 0x13, V(mouth));
    lq_sq(at, pv + 0x320);
    vf_boss_bearing_l18(m, pv + 0x140, mouth, at);
    swc1(pv + 0x3d8, -3.0f);
    if ((lbu(m + 0x70) & 2) != 0) anim_to(m, 8, 0x14);
    if (lw(pv + 0x354) != 0) {
        if (lw(pv + 0x33c) == 0) {
            moby_chain_point_v(m, 1, V(at));
            sw(pv + 0x33c, vf_ball_spawn_l18(m, at, static_cast<uint32_t>(frame_scale_i(lw(L18_ADDR(0x0016239c))))));
        } else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) != 0) {
            if (lbu(m + 0x53) != 9) moby_anim_set(m, 9, 0, 0);
            sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x78)));
            moby_chain_point_v(m, 1, V(at));
            const uint32_t to = sp + 0x40;
            vec3_sub(V(to), V(pv + 0x320), V(at));
            swc1(to + 8, 2.0f);
            vec2_set_length(V(to), V(to), 180.0f);
            vec3_add(V(to), V(to), V(at));
            vf_ball_launch_l18(lwu(pv + 0x33c), at, to, 1, 1.0f);
            sw(pv + 0x33c, 0);
            if (lw(pv + 0x358) == 0) attack_done(m, pv, 0xe);
        } else {
            const float share = clamp_hi_lo(fpu_sub(1.0f, fpu_div(int_to_float(lw(pv + 0x354)), frame_scale_f(90.0f))), 0.0f, 1.0f);
            moby_chain_point_v(m, 1, V(at));
            vf_ball_launch_l18(lwu(pv + 0x33c), at, pv + 0x320, 0, share);
            if (frame_scale_i(0xf) < lw(pv + 0x354)) lq_sq(pv + 0x320, target + 0x10);
        }
    }
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358)));
}

// 15: the shell barrage: at its animation's frames 1 and 16 a shell at a point about the target (or the hero, when the
// target record says 2; one in five at the target's point itself), kept within 29 of the arena while the hero is in his
// mode 0xf, on the floor, level with the boss; from joint 2 / 3, its smoke; done when +0x358 is over, or early with
// more than 25 damage taken in the hero's mode 0xf before phase 4
void state_barrage(uint32_t m, uint32_t pv, uint32_t sp, uint32_t target) {
    face(m, pv, target + 0x10);
    vf_boss_orbit_l18(m);
    if (anim_crossed(m, 1.0f) != 0 || anim_crossed(m, 16.0f) != 0) {
        const uint32_t aim = sp + 0x20, from = sp + 0x30;
        const int32_t k = rand_mod(5);
        if (k == 0 && lw(pv + 0xb4) != 2) {
            lq_sq(aim, pv + 0x70);
        } else {
            lq_sq(from, lw(pv + 0xb4) == 2 ? HERO_AT : pv + 0x70);
            const float a = rand_angle();
            const float r = rand_range_f(1.0f, 10.0f);
            swc1(aim + 0, fpu_mul(cos_vu(a), r));
            const float s = sin_vu(a);
            sw(aim + 8, 0);
            swc1(aim + 4, fpu_mul(s, r));
            vec3_add(V(aim), V(aim), V(from));
        }
        if (lw(HERO_MODE) == 0xf) {
            lq_sq(from, box_centre(lw(pv + 0x344)));
            vec3_sub(V(aim), V(aim), V(from));
            if (29.0f < vec2_length(V(aim))) vec2_set_length(V(aim), V(aim), 29.0f);
            vec3_add(V(aim), V(aim), V(from));
        }
        swc1(aim + 8, floor_probe_v(0.5f, V(aim), 0));
        if (abs_f(fpu_sub(lwc1(aim + 8), lwc1(pv + 0x3c8))) < 5.0f) {
            moby_chain_point_v(m, anim_crossed(m, 1.0f) != 0 ? 2 : 3, V(from));
            aimed_shell(m, from, sp + 0x40, aim, 2);
            vf_boss_smoke_l18(m, from);
        }
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358))) != 0) {
        attack_done(m, pv, 0xf);
        return;
    }
    if (!(25.0f < lwc1(pv + 0x390))) return;
    if (!(static_cast<float>(lw(pv + 0x34c)) < 4.0f)) return;
    attack_done(m, pv, 0xf);
}

// 16: the bomb throw: every 15 frames a walking bomb of the group +0x29c thrown from 3 ahead of the boss (3 up) at a
// floor point 3 .. 29 about the arena's centre level with the boss (a second's flight, its fuse 18 .. 20 seconds);
// done when +0x358 is over
void state_bombs(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    vf_boss_orbit_l18(m);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) != 0) {
        const uint32_t centre = sp + 0x20, from = sp + 0x30, to = sp + 0x40;
        lq_sq(centre, box_centre(lw(pv + 0x344)));
        vec3_set_length(V(from), V(m + 0xc0), 3.0f);
        vec3_add(V(from), V(from), V(m + 0x10));
        swc1(from + 8, fpu_add(lwc1(from + 8), 3.0f));
        const float a = rand_angle();
        const float r = rand_range_f(3.0f, 29.0f);
        swc1(to + 0, fpu_mul(cos_vu(a), r));
        const float s = sin_vu(a);
        sw(to + 8, 0);
        swc1(to + 4, fpu_mul(s, r));
        vec3_add(V(to), V(to), V(centre));
        swc1(to + 8, fpu_add(lwc1(to + 8), 5.0f));
        swc1(to + 8, floor_probe_v(0.5f, V(to), 0));
        if (abs_f(fpu_sub(lwc1(to + 8), lwc1(pv + 0x3c8))) < 5.0f) {
            const float d = vec2_distance(V(to), V(from));
            const float speed = fpu_div(d, static_cast<float>(frame_scale_i(0x3c)));
            const int32_t fuse = float_to_int(frame_scale_f(fpu_mul(rand_range_f(18.0f, 20.0f), 60.0f)));
            vf_bomb_throw_l18(m, lw(pv + 0x29c), from, to, static_cast<uint32_t>(fuse), speed);
        }
        sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0xf)));
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358))) == 0) return;
    attack_done(m, pv, 0x10);
}

// 17: the robots: animation 0xc; at its timer's end (or with +0x354 at 30 frames) a robot of the group +0x210 launched
// at a floor point 4.3 to one side of the home (a second's flight from the frame's sp + 0x30, which this state never
// writes: the original's frame word); more than 10 out: a long wait; fewer than 3 out with the wait long: done at once;
// done when +0x358 is over (animation 0xd)
void state_robots(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    vf_boss_orbit_l18(m);
    if ((lbu(m + 0x70) & 2) != 0) anim_to(m, 0xc, 0x14);
    bool launch;
    if (lbu(m + 0x53) == 0xc && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) != 0) launch = true;
    else launch = lw(pv + 0x354) == frame_scale_i(0x1e);
    if (launch) {
        const uint32_t at = sp + 0x20;
        vec3_set_length(V(at), V(m + 0xd0), lw(pv + 0x354) == 0 ? -4.3f : 4.3f);
        vec3_add(V(at), V(at), V(pv + 0x3c0));
        swc1(at + 8, floor_probe_v(0.5f, V(at), 0));
        if (abs_f(fpu_sub(lwc1(at + 8), lwc1(pv + 0x3c8))) < 1.0f) {
            const float d = vec2_distance(V(at), V(sp + 0x30));
            const float speed = fpu_div(d, static_cast<float>(frame_scale_i(0x3c)));
            if (vf_robot_launch_l18(lw(pv + 0x210), at, at, speed) != 0 && lw(pv + 0x354) == 0)
                sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x3c)));
        }
    }
    const int32_t out = vf_robots_out_l18(lw(pv + 0x210));
    if (!(out < 11)) {
        if (lw(pv + 0x354) < frame_scale_i(0x3c)) sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x3e8)));
    } else if (out < 3 && frame_scale_i(0x3c) < lw(pv + 0x354)) {
        sw(pv + 0x358, 0);
    }
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358))) == 0) return;
    attack_done(m, pv, 0x11);
    anim_to(m, 0xd, 0x14);
}

// 18: the lunge to the lunge box +0x3a8 (12 a frame); there: the energy ball (state 14, animation 7) at the target
void state_lunge(uint32_t m, uint32_t pv, uint32_t target) {
    face(m, pv, pv + 0x70);
    if (vf_boss_home_step_l18(m, box_centre(lw(pv + 0x3a8)), fpu_mul(dt(), 12.0f)) == 0) return;
    sb(m + 0x20, 0xe);
    anim_to(m, 7, 0x14);
    lq_sq(pv + 0x320, target + 0x10);
}

// 19: the charge at the hero (after +0x354's wait), the burst moved along, its sound (level sound 0xe); it stops (state
// 12, animation 0, the last attack 0x13, the sound stopped) when +0x358 is over (and in the hero's mode 0xf +0x35c too),
// the hero in his mode 0xf near his path's end, in his state 0x16 within 12, or past L18:0x162378 + 5.8 of the box +0x258
void state_charge(uint32_t m, uint32_t pv) {
    face(m, pv, HERO_AT);
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) != 0) vf_boss_charge_l18(m);
    vf_burst_move_l18(lwu(pv + 0x338), pv + 0x3c0, 5.8f);
    if (sub_0028d8c0(m, lw(pv + 0x3b0)) == 0) sw(pv + 0x3b0, static_cast<uint32_t>(moby_sound_start(0xe, 4, m)));
    bool stop = false;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x358))) != 0) {
        if (lw(HERO_MODE) != 0xf) stop = true;
        else if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x35c))) != 0) stop = true;
    }
    if (lw(HERO_MODE) == 0xf) {
        const uint32_t path = lwu(HERO + 0x560);
        if (vec2_distance(V(HERO + 0x80), V(path + (lwu(path) << 4))) < 3.0f) stop = true;
    }
    if (lw(HERO + 0x2084) == 0x16 && vec2_distance(V(m + 0x10), V(HERO + 0x80)) < 12.0f) stop = true;
    if (fpu_add(lwc1(L18_ADDR(0x00162378)), 5.8f) < vec2_distance(V(HERO_AT), V(box_centre(lw(pv + 0x258))))) stop = true;
    if (!stop) return;
    slot_stop(m, pv);
    sb(m + 0x20, 0xc);
    if (lbu(m + 0x53) != 0) moby_anim_set(m, 0, 0, frame_scale_i(0x14));
    sw(pv + 0x350, 0x13);
}

// 20: the divers: every 20 frames the group +0x214's divers in their state 1 launched from 4 above home; none left:
// state 12
void state_divers(uint32_t m, uint32_t pv, uint32_t sp) {
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x354))) == 0) return;
    sw(pv + 0x354, static_cast<uint32_t>(frame_scale_i(0x14)));
    lq_sq(sp + 0x20, pv + 0x3c0);
    swc1(sp + 0x28, fpu_add(lwc1(sp + 0x28), 4.0f));
    if (vf_divers_launch_l18(m, lw(pv + 0x214), sp + 0x20) == 0) return;
    sb(m + 0x20, 0xc);
}

// 21: the phase-2 end: to the gate +0x330 (opened, 12 a frame); there the lift sent up for a wait by the phase (the
// table L18:0x1f2be0: 3600 .. 600 frames, scaled twice as the original does), state 0x14 in phase 4, else 12, the next
// phase, the charge kept off; L18:0x162390 set: state 5
void state_phase_end(uint32_t m, uint32_t pv) {
    face(m, pv, lwu(pv + 0x330) + 0x10);
    vf_gate_open_l18(lwu(pv + 0x330));
    if (vf_boss_home_step_l18(m, lwu(pv + 0x330) + 0x10, fpu_mul(dt(), 12.0f)) != 0) {
        int32_t wait;
        switch (lwu(pv + 0x34c)) {  // (the jump table L18:0x1f2be0)
        case 0: wait = 0xe10; break;  // (0x002f55e8)
        case 1: wait = 0xa8c; break;  // (0x002f55f0)
        case 2: wait = 0x708; break;  // (0x002f55f8)
        case 3: wait = 0x4b0; break;  // (0x002f5600)
        case 4: wait = 0x384; break;  // (0x002f5608)
        default: wait = 0x258; break;  // (0x002f5610: 5 and outside the table)
        }
        vf_lift_start_l18(table_moby(lwu(pv + 0x21c)), frame_scale_i(frame_scale_i(wait)));
        const int32_t phase = lw(pv + 0x34c);
        sw(pv + 0x354, 0);
        sb(m + 0x20, phase == 4 ? 0x14 : 0xc);
        sw(pv + 0x364, 1);
        sw(pv + 0x34c, static_cast<uint32_t>(lw(pv + 0x34c) + 1));
    }
    if (lw(L18_ADDR(0x00162390)) != 0) sb(m + 0x20, 5);
}

// 26: the death: every 20 frames an explosion 6 from the home within 30 degrees of the facing, at frame 120 a big one
// 3 ahead (animation 0xe); at animation 0xe's frame 30 the end (state 27), the lift stopped
void state_death(uint32_t m, uint32_t pv, uint32_t sp) {
    face(m, pv, HERO_AT);
    vf_boss_orbit_l18(m);
    sw(pv + 0x358, static_cast<uint32_t>(lw(pv + 0x358) + 1));
    const uint32_t at = sp + 0x20;
    if (lw(pv + 0x358) % frame_scale_i(0x14) == 0) {  // (the original divides with a trap on 0: never 0 here)
        const float off = rand_range_f(-30.0f, 30.0f);
        const float a = angle_add(lwc1(m + 0x48), fpu_mul(off, u2f(0x3c8efa35u)));
        swc1(at + 0, fpu_mul(cos_vu(a), 6.0f));
        const float s = sin_vu(a);
        sw(at + 8, 0);
        swc1(at + 4, fpu_mul(s, 6.0f));
        swc1(at + 8, rand_range_f(3.0f, 8.0f));
        vec3_add(V(at), V(at), V(pv + 0x3c0));
        explosion_effect(m, L18_ADDR(0x0015f580), at, 0x14, 6, 0x20, -1, 0, 0, -1, 0, 0.0f, 0.0f, 1.0f, 2.5f, 9.0f, 1.0f, 0.0f);
    }
    if (lw(pv + 0x358) == frame_scale_i(0x78)) {
        if (lbu(m + 0x53) != 0xe) moby_anim_set(m, 0xe, 5, 1);
        swc1(at + 0, fpu_mul(cos_vu(lwc1(m + 0x48)), 3.0f));
        const float s = sin_vu(lwc1(m + 0x48));
        swc1(at + 8, 6.0f);
        swc1(at + 4, fpu_mul(s, 3.0f));
        vec3_add(V(at), V(at), V(pv + 0x3c0));
        explosion_effect(m, L18_ADDR(0x0015f580), at, 0x28, 0xa, 0x20, -1, 0, 0, -1, 0, 0.0f, 0.0f, 2.5f, 5.0f, 9.0f, 2.0f, 10.0f);
    }
    if (lbu(m + 0x52) != lbu(m + 0x53) || lbu(m + 0x52) != 0xe) return;
    if (anim_crossed(m, 30.0f) == 0) return;
    sb(m + 0x20, 0x1b);
    vf_lift_stop_l18(table_moby(lwu(pv + 0x21c)));
}

// 27: the end, by its step +0x34c: 7: scene 3 with the walk-to record (the hero placed at box +0x294 when the scene ends);
// 8: the hero on a gate in his mode 0x22 (else the gates opened) for 40 frames: scene 4; 9: the scripted camera and scene
// 5; 10: the credits; 11: movie 0xb; 12: the end menu (0x21), the attached mobys and the boss gone. Each step once the
// level is back in its mode 0 (L18:0x15f5c4). Returns false when the boss is gone (no tail).
bool state_end(uint32_t m, uint32_t pv, uint32_t sp) {
    sw(m + 0x94, 0);
    sh(m + 0x34, (lhu(m + 0x34) | 0x41u) & 0xefffu);
    const int32_t step = lw(pv + 0x34c);
    if (step < 7) {
        cutscene_start(3);
        sw(pv + 0x34c, 7);
        // the walk-to flag of the scene record: a byte at +0x4a on the PAL 2.00 disc (the 1.02 build's own code: `sb v0,
        // 0x4a(a1)` there, `sh v0, 0x46(a1)` on NTSC-U and PAL 1.00), as hero_walk_to_l1 writes it
        const uint32_t scene = L18_ADDR(0x0016d260);
        if (region_core_moved()) sb(scene + 0x4a, 1);
        else sh(scene + 0x46, 1);
        const uint32_t box = lvd::box(lvd::BOXES, lw(pv + 0x294));
        lq_sq(scene + 0x10, box + 0x30);
        lq_sq(scene + 0x20, box + 0x70);
        sb(m + 0x20, 7);
        sw(pv + 0x39c, 0x1b);
        return true;
    }
    if (step == 7) {
        const uint32_t ride = lwu(0x0013f64c);
        const bool on_gate = ride != 0 && lh(0x0013f65e) == 0 && lh(ride + 0xa6) == 0x247 && lw(0x001413d4) == 0x22;
        int32_t count = lw(pv + 0x3ac);
        if (count == 0) {
            if (on_gate) sw(pv + 0x3ac, 1);
            else vf_gates_open_l18(lw(pv + 0x200));
            count = lw(pv + 0x3ac);
            if (count == 0) return true;
        }
        sw(pv + 0x3ac, static_cast<uint32_t>(count + 1));
        if (frame_scale_i(0x28) < count) {
            cutscene_start(4);
            sw(pv + 0x34c, 8);
            sb(m + 0x20, 7);
            sw(pv + 0x39c, 0x1b);
        }
        return true;
    }
    if (step == 8) {
        const uint32_t from = sp + 0x20, rot = sp + 0x30;
        for (uint32_t k = 0; k < 0x10; k += 4) {
            sw(from + k, 0);
            sw(rot + k, 0);
        }
        swc1(from + 0, u2f(0x44252666u));  // (660.6, 481.4, 112.6)
        swc1(from + 4, u2f(0x43f0b333u));
        swc1(from + 8, u2f(0x42e13333u));
        swc1(rot + 4, u2f(0xbdf5c28fu));  // (-0.12, -2.76)
        swc1(rot + 8, u2f(0xc030a3d7u));
        camera_scripted_set(from, rot, 1, 0, 0);
        cutscene_start(5);
        sw(pv + 0x34c, 9);
        return true;
    }
    const int32_t mode = lw(L18_ADDR(0x0015f5c4));
    if (step == 9 && mode == 0) {
        credits_start_tr(0);  // (the original passes a leftover register; the port reads none)
        sw(pv + 0x34c, 0xa);
        return true;
    }
    if (step == 0xa && mode == 0) {
        movie_story_play(0xb);
        sw(pv + 0x34c, 0xb);
        return true;
    }
    if (step != 0xb || lw(L18_ADDR(0x0015f5c4)) != 0) return true;
    sw(L18_ADDR(0x00162360), 0);
    sw(L18_ADDR(0x001623a0), 0);
    menu_open(0x21);
    moby_retire(lwu(pv + 0x3a0));
    if (lwu(pv + 0x3a4) != 0) moby_retire(lwu(pv + 0x3a4));
    moby_retire(m);
    return false;
}

// the tail, every frame after the state: the fight's camera; the exhaust smoke between chain points 8 and 9 while shown;
// the boss the level's boss (0x13f928) from phase 2; its place its home, the hover (+0x3d4 a half turn a second, its
// height sought within +0x3d0 and the push +0x3d8, then 0); the two joint springs; the glow's colour; its draw queued
void tail(uint32_t m, uint32_t pv, uint32_t sp) {
    vf_boss_camera_l18(m);
    if (lbu(m + 0x31) != 0) {
        const uint32_t a = sp + 0x20, b = sp + 0x30, d = sp + 0x40;
        moby_chain_point_v(m, 8, V(a));
        moby_chain_point_v(m, 9, V(b));
        vec3_sub(V(d), V(a), V(b));
        vec3_set_length(V(d), V(d), 0.1f);
        smoke_burst(a, a, d, 600000.0f, 0.0f);
    }
    if (!(lw(pv + 0x34c) < 2)) sw(0x0013f928, m);
    lq_sq(m + 0x10, pv + 0x3c0);
    const float bob = angle_add(lwc1(pv + 0x3d4), fpu_mul(dt(), u2f(0x40490fdbu)));
    swc1(pv + 0x3d4, bob);
    const float s = sin_vu(bob);
    const float rate = fpu_mul(dt_sq(), 4.0f);
    seek_1d(pv + 0x3dc, pv + 0x3e0, fpu_add(fpu_mul(lwc1(pv + 0x3d0), s), lwc1(pv + 0x3d8)), rate, rate, fpu_mul(dt(), 4.0f));
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), lwc1(pv + 0x3dc)));
    sw(pv + 0x3d8, 0);
    const float k = lwc1(0x0015ed64);
    joint_node_spring(m, pv + 0xc0, 7, fpu_mul(k, 0.03f), fpu_mul(k, 0.3f));
    joint_node_spring(m, pv + 0x140, 0x13, fpu_mul(lwc1(0x0015ed64), 0.03f), fpu_mul(lwc1(0x0015ed64), 0.3f));
    const float glow = angle_add(lwc1(pv + 0x3b4), fpu_mul(dt(), u2f(0x405f66f3u)));
    swc1(pv + 0x3b4, glow);
    const uint32_t c = rgba_lerp(0x30c8c8c8u, 0x1c393939u, fpu_add(fpu_mul(sin_vu(glow), 0.5f), 0.5f));
    sw(m + 0x90, c);
    const uint32_t r = (c & 0xffu) / 3, b3 = ((c >> 16) & 0xffu) / 3;
    sw(pv + 0x3b8, (c & 0xff000000u) | (b3 << 16) | (c & 0xff00u) | r);
    if (lbu(m + 0x31) != 0) sub_001f93e0(L18_ADDR(0x002f7880), m);
}

}  // namespace

// The boss's update (class 1422).
// orig: L18:0x002f2bf0
void vf_class1422_update_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Frame f(0x100);
    const uint32_t sp = f.sp;
    vf_hero_float_count_l18(m);
    vf_boss_preamble_l18(m);
    const uint32_t target = lwu(pv + 0xb0);  // (the target record's moby, as the preamble left it)
    vf_drek_scale_l18();
    const uint32_t drek = lwu(pv + 0x3a4);
    if (drek != 0 && static_cast<int8_t>(lbu(drek + 0x20)) >= 0) joint_node_scale(drek, 0, pv + 0x1c0, lwc1(L18_ADDR(0x001623a4)));
    if (lbu(m + 0x31) != 0 && vec3_distance(V(m + 0x10), V(L18_ADDR(0x001677c0))) < 32.0f) {
        moby_floor_band(m);
        sb(m + 0x7f, 0x1a);
    }
    switch (lbu(m + 0x20)) {  // (the jump table L18:0x1f2b50)
    case 0: state_setup(m, pv); break;                     // (0x002f2ccc)
    case 1: state_wait(m, pv); break;                      // (0x002f2ef4)
    case 2: state_on_foot(m, pv, sp); break;               // (0x002f32bc)
    case 3: state_pan_wait(m, pv, sp); break;              // (0x002f3f80)
    case 4: state_hero_down(m, pv); break;                 // (0x002f3d28)
    case 5: state_lift_pan(m, pv, sp); break;              // (0x002f3dc4)
    case 7: state_scene(m, pv); break;                     // (0x002f2dc4)
    case 8: state_pan_end(m, pv); break;                   // (0x002f4030)
    case 9: state_ball(m, pv, sp); break;                  // (0x002f3648)
    case 0xa: state_fly(m, pv, sp); break;                 // (0x002f394c)
    case 0xb: state_return(m, pv, sp); break;              // (0x002f3bb8)
    case 0xc: state_choose(m, pv, sp, target); break;      // (0x002f40f4)
    case 0xd: state_rings(m, pv, sp, target); break;       // (0x002f4ac8)
    case 0xe: state_ball_again(m, pv, sp, target); break;  // (0x002f4498)
    case 0xf: state_barrage(m, pv, sp, target); break;     // (0x002f4730)
    case 0x10: state_bombs(m, pv, sp); break;              // (0x002f4db8)
    case 0x11: state_robots(m, pv, sp); break;             // (0x002f4fbc)
    case 0x12: state_lunge(m, pv, target); break;          // (0x002f5458)
    case 0x13: state_charge(m, pv); break;                 // (0x002f5210)
    case 0x14: state_divers(m, pv, sp); break;             // (0x002f4090)
    case 0x15: state_phase_end(m, pv); break;              // (0x002f5530)
    case 0x1a: state_death(m, pv, sp); break;              // (0x002f568c)
    case 0x1b:                                             // (0x002f5940)
        if (!state_end(m, pv, sp)) return;
        break;
    default: break;  // (6, 0x16 .. 0x19 and past the table: 0x002f5bb4, the tail only)
    }
    tail(m, pv, sp);
}

}  // namespace rac1
