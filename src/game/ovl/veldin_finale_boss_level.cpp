// Veldin's finale (level 18): what the boss (class 1422: its update in veldin_finale_boss.cpp) reads and sets
// in the level around it, hand-written (part 2 of its port): the scripted camera move (class 644) started and its
// progress, the rafts (class 1381) held and sunk with the ties they free, the record its pvar word 0 names dimmed by the
// hero's distance, the hero's ride on a floating platform counted, the scene's Drek model scaled. The boss's update (veldin_finale_boss.cpp) calls them (and its still-generated preamble L18:0x2f5e80 calls
// 0x2dfaa0, 0x2f1cc8, 0x265ff8 the same way). Level 18 only: the addresses are the finale's (`orig: L18:`), its own data
// L18_ADDR, the base ELF's bare (the hero 0x13f350, the frame time's square 0x15ed70). Declared in veldin_finale_own.h.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_attach.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr int16_t RAFT_CLASS = 0x565;   // class 1381
constexpr int16_t FLOAT_CLASS = 0x24b;  // class 587, the floating platforms
constexpr int16_t DREK_CLASS = 0x4e1;   // class 1249, Drek's model in the scenes

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};
}  // namespace

// ---- the scripted camera move (class 644, veldin_finale_camera_pan.cpp)

// The camera move `pan` started (its state 2): its pvars' first / last points (+0 / +0x10), first / last angles (+0x20
// / +0x30) and the hero's place (+0x40) copied from the EE vectors given, the progress and its speed at 0, the
// acceleration a third of the frame time's square, the top speed 1 / frames (`frames` scaled to the frame rate), the
// wait `wait` frames (scaled), `clank` its +0x78 (Clank's line first); the hero's hold timer 0x13f510 `frames` (scaled).
// (The boss passes the ninth word on its stack: the glue reads it there.)
// orig: L18:0x002dfaa0
void vf_pan_start_l18(uint32_t pan, uint32_t first, uint32_t last, uint32_t first_angles, uint32_t last_angles,
                      uint32_t hero_at, int32_t frames, int32_t wait, uint32_t clank) {
    const uint32_t pv = lwu(pan + 0x78);
    sw(pv + 0x60, 0);
    sw(pv + 0x64, 0);
    lq_sq(pv + 0x00, first);
    lq_sq(pv + 0x10, last);
    lq_sq(pv + 0x20, first_angles);
    lq_sq(pv + 0x30, last_angles);
    lq_sq(pv + 0x40, hero_at);
    sb(pan + 0x20, 2);
    swc1(pv + 0x68, fpu_mul(lwc1(0x0015ed70), 0.333f));
    swc1(pv + 0x70, fpu_div(1.0f, static_cast<float>(frame_scale_i(frames))));
    sw(pv + 0x74, static_cast<uint32_t>(frame_scale_i(wait)));
    sw(pv + 0x78, clank);
    sw(0x0013f510, static_cast<uint32_t>(frame_scale_i(frames)));
}

// The camera move's progress (its pvars' +0x60, 0 .. 1).
// orig: L18:0x002dfb90
float vf_pan_progress_l18(uint32_t pan) { return lwc1(lwu(pan + 0x78) + 0x60); }

// ---- the rafts (class 1381, veldin_finale_raft.cpp)

// A raft sent off (its state 1, its pvars' +0x330 / +0x344 cleared; only a moby of the raft's class), then the
// platforms its records name (pvars +0x30, +0x338 of them) put where it takes them, whatever the moby's class.
// orig: L18:0x002f1cc8
void vf_raft_hold_l18(uint32_t o) {
    const uint32_t pv = lwu(o + 0x78);
    if (lh(o + 0xa6) == RAFT_CLASS) {
        sb(o + 0x20, 1);
        sw(pv + 0x330, 0);
        sw(pv + 0x344, 0);
    }
    attach_carry(o, pv + 0x30, lw(pv + 0x338));
}

// The level's ties of kind `kind` (its index in L18:0x1c6500, the kind's header in L18:0x1c6200: +0x26 the count, +0x28
// the instances, 0x20 bytes each) standing inside the box `box` (x and y within the box's -1 .. 1, the height not
// looked at) freed: their +0xc 0, +0x14 1.0.
// orig: L18:0x002f1d08
void vf_ties_free_l18(uint32_t kind, int32_t box) {
    const uint32_t index = L18_ADDR(0x001c6500) + kind;
    auto header = [index] { return lwu(L18_ADDR(0x001c6200) + (static_cast<uint32_t>(lbu(index)) << 2)); };
    if (lh(header() + 0x26) <= 0) return;
    Frame f(0x80);
    Vec4& local = V(f.sp);
    for (int32_t i = 0; i < lh(header() + 0x26); ++i) {
        const uint32_t b = lvd::box(lvd::BOXES, box);
        vec3_sub(local, V(lwu(header() + 0x28) + (static_cast<uint32_t>(i) << 5)), V(b + 0x30));
        mat3_transform(local, local, M(lvd::box(lvd::BOXES, box) + 0x40));
        if (!(abs_f(lwc1(f.sp + 0)) < 1.0f)) continue;
        if (!(abs_f(lwc1(f.sp + 4)) < 1.0f)) continue;
        sw(lwu(header() + 0x28) + (static_cast<uint32_t>(i) << 5) + 0xc, 0);
        swc1(lwu(header() + 0x28) + (static_cast<uint32_t>(i) << 5) + 0x14, 1.0f);
    }
}

// A raft in its state 1 hit by the boss's energy ball: the ties of the kinds 0x802, 0x908 and 0x954 in its box (pvars
// +0xc, none when -1) freed, its state 2, its group's platforms' +0xbc 1. Nothing for another class or state.
// orig: L18:0x002f1c38
void vf_raft_sink_l18(uint32_t o) {
    if (lh(o + 0xa6) != RAFT_CLASS) return;
    const uint32_t pv = lwu(o + 0x78);
    if (lbu(o + 0x20) != 1) return;
    if (lw(pv + 0xc) != -1) {
        vf_ties_free_l18(0x802, lw(pv + 0xc));
        vf_ties_free_l18(0x908, lw(pv + 0xc));
        vf_ties_free_l18(0x954, lw(pv + 0xc));
    }
    sb(o + 0x20, 2);
    vf_group_platforms_flag_l18(lbu(o + 0x21), 1);
}

// ---- the boss and the hero

// The byte +0xa of the record the moby's pvar word 0 names (moby_pvar_word0: none without flag 0x20 of +0x34) set
// from the hero's distance: distance * 8 * `scale` - 1, kept to 0 .. 255. Nothing for no moby or no record. (A function
// of level 18 alone: it sits in the engine's code where Veldin has 0x2637e8 and scene_moby_smoke, between the level's
// copies of glow_draw and joint_node_scale; no other level's overlay has its body.)
// orig: L18:0x00265ff8
void vf_record_by_hero_distance_l18(uint32_t m, float scale) {
    if (m == 0) return;
    const uint32_t rec = moby_pvar_word0(m);
    if (rec == 0) return;
    const float d = vec3_distance(V(0x0013f3d0), V(m + 0x10));
    const int32_t v = static_cast<int32_t>(static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(d, 8.0f), scale))) - 1u);
    sb(rec + 0xa, static_cast<uint32_t>(v >= 256 ? 0xff : v <= -1 ? 0 : v));
}

// The hero on a floating platform, counted once (L18:0x1623a8): the moby the hero stands on (0x13f64c, while
// 0x13f650 says he stands on one) kept in the pvars' +0x3e8; the first frame the hero is in his state 0x77 the boss
// stops looking (+0x3e4 1), and when the moby kept is a floating platform the count goes up.
// orig: L18:0x002f5e18
void vf_hero_float_count_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv + 0x3e4) != 0) return;
    if (lw(0x0013f650) != 0) sw(pv + 0x3e8, lwu(0x0013f64c));
    if (lw(0x001413d4) != 0x77) return;
    sw(pv + 0x3e4, 1);
    const uint32_t o = lwu(pv + 0x3e8);
    if (o == 0) return;
    if (lh(o + 0xa6) != FLOAT_CLASS) return;
    sw(L18_ADDR(0x001623a8), lwu(L18_ADDR(0x001623a8)) + 1);
}

// In a scene (the level's mode 2, the flag 0x15edb0): each Drek model (class 1249) of the scene's moby list (L18:0x16d260
// + 0x178, its count +0x44) scaled by L18:0x1623a4 through the joint node L18:0x17ce40 (set up first, when its +1 is 0).
// orig: L18:0x002f2b18
void vf_drek_scale_l18() {
    if (lw(L18_ADDR(0x0015f5c4)) != 2) return;
    if (lbu(0x0015edb0) == 0) return;
    const uint32_t scene = L18_ADDR(0x0016d260);
    if (lh(L18_ADDR(0x0016d2a4)) <= 0) return;
    for (int32_t i = 0; i < lh(scene + 0x44); ++i) {
        const uint32_t o = lwu(scene + 0x178 + static_cast<uint32_t>(i) * 4);
        if (lh(o + 0xa6) != DREK_CLASS) continue;
        const uint32_t node = L18_ADDR(0x0017ce40);
        if (lbu(node + 1) != 0) continue;
        sub_0024f590(o, 0, node);
        const float s = lwc1(L18_ADDR(0x001623a4));
        swc1(node + 0x28, s);
        swc1(node + 0x20, s);
        swc1(node + 0x24, s);
    }
}

}  // namespace rac1
