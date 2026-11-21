// Veldin's finale (level 18): how the boss (class 1422, still generated: veldin_finale_classes_tr_5.cpp) moves and picks
// its attacks, hand-written (part 2 of its port): its home point stepped toward a goal, the charge at the hero and the
// orbit about the arena (both clamped to the arena's box), the floating platform it flies to, the next attack, a
// bearing against a moby's yaw. The generated boss calls them through tools/codegen/gen/mkall/veldin_finale_classes.glue.
// Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR (the fight's tuning words
// L18:0x162338 .. 0x16237c), the base ELF's bare (the hero 0x13f350, the frame time 0x15ed6c and its square 0x15ed70).
// Declared in veldin_finale_own.h.
//
// The boss's pvars used here: +0x25c the box of the charge, +0x344 the arena's box (the level's box list), +0x34c the
// fight's phase, +0x350 the last attack (its state), +0x364 no charge, +0x374 its speed toward the goal, +0x398 the group
// of floating platforms, +0x3c0 its home point (where it hovers: the update moves it there), +0x3c8 its height.
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
constexpr int16_t FLOAT_CLASS = 0x24b;  // class 587, the floating platforms
constexpr uint32_t HERO_AT = 0x0013f3d0;  // the hero's position (0x13f350 + 0x80)

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale
uint32_t first_moby() { return lwu(L18_ADDR(0x0015ffd8)); }

// the centre of the boss's arena box (pvars +0x344)
uint32_t arena(uint32_t pv) { return lvd::box(lvd::BOXES, lw(pv + 0x344)); }

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
}  // namespace

// The boss's home point (pvars +0x3c0) stepped toward the EE point `goal` on the ground plane: its speed (+0x374) sought
// toward the distance (seek_1d, accelerations ten times the frame time's square, at most `max`), the home moved by that
// speed along the way. 1 when it was there already (within 0.1, the speed below 0.1), else 0.
// orig: L18:0x002f6878
uint32_t vf_boss_home_step_l18(uint32_t m, uint32_t goal, float max) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t home = pv + 0x3c0;
    Frame f(0x60);
    const uint32_t step = f.sp, pos = f.sp + 0x10;
    swc1(pos, 0.0f);
    vec3_sub(V(step), V(goal), V(home));
    swc1(step + 8, 0.0f);
    const float dist = vec3_length(V(step));
    const float accel = fpu_mul(lwc1(0x0015ed70), 10.0f);
    seek_1d(pos, pv + 0x374, dist, accel, accel, max);
    vec3_set_length(V(step), V(step), lwc1(pv + 0x374));
    vec3_add(V(home), V(home), V(step));
    if (!(dist < 0.1f)) return 0;
    return lwc1(pv + 0x374) < 0.1f ? 1u : 0u;
}

// The charge (state 19): the boss's home stepped toward the hero's point kept within the arena (L18:0x16237c from the
// arena's centre while the hero is in his mode 0xf, else L18:0x162378; on the arena's floor), at the hero's speed
// (0x13f4b0) plus a share of the frame's 4.75 by how far the boss is past L18:0x16234c from it, kept within 4.75 .. 20
// times the frame time.
// orig: L18:0x002f6960
void vf_boss_charge_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Frame f(0x60);
    const uint32_t goal = f.sp;
    vec3_sub(V(goal), V(HERO_AT), V(arena(pv) + 0x30));
    sw(goal + 8, 0);
    vec3_clamp_length(goal, lw(0x001413dc) == 0xf ? lwc1(L18_ADDR(0x0016237c)) : lwc1(L18_ADDR(0x00162378)));
    vec3_add(V(goal), V(goal), V(arena(pv) + 0x30));
    float speed = lwc1(0x0013f4b0);
    const float d = vec2_distance(V(m + 0x10), V(goal));
    const float reach = lwc1(L18_ADDR(0x0016234c));
    const float t = dt();
    const float lo = fpu_mul(t, 4.75f);
    const float past = fpu_div(fpu_sub(d, reach), reach);
    const float hi = fpu_mul(t, 20.0f);
    speed = fpu_add(speed, fpu_mul(past, lo));
    vf_boss_home_step_l18(m, goal, clamp_hi_lo(speed, lo, hi));
}

// The orbit about the arena (states 2, 12 and the attacks): the boss's bearing from the arena's centre turned at most
// 10 degrees toward the side 135 degrees round from the hero's bearing, the goal on that bearing at the radius
// L18:0x162370 (state 2) or L18:0x162374; the home stepped there at 10 a frame from outside the arena's radius + 1, 20
// while the hero is in his mode 0xf, else at a speed by the hero's distance (L18:0x162350 .. 0x16235c, other speeds in
// states 13 and 15); its height (+0x3c8) brought toward the box's centre's (4 a frame).
// orig: L18:0x002f6aa0
void vf_boss_orbit_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Frame f(0x70);
    const uint32_t goal = f.sp;
    const float hero_bearing = atan2_f(fpu_sub(lwc1(HERO_AT + 0), lwc1(arena(pv) + 0x30)),
                                       fpu_sub(lwc1(HERO_AT + 4), lwc1(arena(pv) + 0x34)));
    const float bearing = atan2_f(fpu_sub(lwc1(pv + 0x3c0), lwc1(arena(pv) + 0x30)),
                                  fpu_sub(lwc1(pv + 0x3c4), lwc1(arena(pv) + 0x34)));
    const float side = angle_sub(bearing, hero_bearing) < 0.0f ? -2.3561945f : 2.3561945f;
    float turn = angle_sub(angle_add(hero_bearing, side), bearing);
    turn = clamp_hi_lo(turn, -0.17453292f, 0.17453292f);
    const float a = angle_add(turn, bearing);
    const uint32_t radius = lbu(m + 0x20) == 2 ? L18_ADDR(0x00162370) : L18_ADDR(0x00162374);
    swc1(goal + 0, fpu_mul(cos_vu(a), lwc1(radius)));
    const float s = sin_vu(a);
    sw(goal + 8, 0);
    swc1(goal + 4, fpu_mul(s, lwc1(radius)));
    vec3_add(V(goal), V(goal), V(arena(pv) + 0x30));
    float speed;
    if (fpu_add(lwc1(L18_ADDR(0x00162374)), 1.0f) < vec2_distance(V(pv + 0x3c0), V(arena(pv) + 0x30))) {
        speed = 10.0f;
    } else if (lw(0x001413dc) == 0xf) {
        speed = 20.0f;
    } else {
        const uint8_t state = lbu(m + 0x20);
        float near = lwc1(L18_ADDR(0x0016235c)), far = lwc1(L18_ADDR(0x00162358));
        float slow = lwc1(L18_ADDR(0x00162354)), fast = lwc1(L18_ADDR(0x00162350));
        if (state == 0xd) {
            slow = 2.0f;
            fast = 5.5f;
            far = fpu_mul(far, 0.5f);
            near = fpu_mul(near, 0.5f);
        } else if (state == 0xf) {
            slow = 6.0f;
            far = fpu_mul(far, 1.5f);
            fast = 10.0f;
            near = fpu_mul(near, 1.5f);
        }
        const float d = clamp_hi_lo(vec2_distance(V(m + 0x10), V(HERO_AT)), near, far);
        const float k = fpu_sub(1.0f, fpu_div(fpu_sub(d, near), fpu_sub(far, near)));
        speed = fpu_add(fpu_mul(fpu_sub(fast, slow), k), slow);
    }
    vf_boss_home_step_l18(m, goal, fpu_mul(speed, dt()));
    approach_f(pv + 0x3c8, lwc1(arena(pv) + 0x38), fpu_mul(dt(), 4.0f));
}

// The floating platform (class 587) of the boss's group (+0x398) to fly to: every platform of the group is sunk if it
// was waiting to (vf_float_sink_l18); those within L18:0x162338 of the home on the ground plane are rocked (waits 60
// frames and L18:0x16233c, scaled), and the nearest of them ahead of the boss (the cosine with its facing, +0xc0, above
// 0.333; nearer than 1024) is the one. 0 for none (or a group with no list).
// orig: L18:0x002f6e10
uint32_t vf_boss_float_pick_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    uint32_t e = lvd::group(lw(pv + 0x398));
    if (e == 0) return 0;
    Frame f(0xb0);
    const uint32_t to = f.sp;
    float best = 1024.0f;
    uint32_t pick = 0;
    for (;; e += 2) {
        const uint32_t off = (lhu(e) & 0x7fff) << 8;
        const uint32_t o = first_moby() + off;
        if (lh(o + 0xa6) == FLOAT_CLASS) {
            vf_float_sink_l18(o);
            if (vec2_distance(V(o + 0x10), V(pv + 0x3c0)) < lwc1(L18_ADDR(0x00162338))) {
                vec3_sub(V(to), V(o + 0x10), V(pv + 0x3c0));
                const int32_t rock = frame_scale_i(0x3c);
                vf_float_rock_l18(o, rock, frame_scale_i(lw(L18_ADDR(0x0016233c))));
                if (0.333f < vec3_dot(V(to), V(m + 0xc0))) {
                    const float d = vec3_length(V(to));
                    if (d < best) {
                        best = d;
                        pick = o;
                    }
                }
            }
        }
        if (lh(e) < 0) break;
    }
    return pick;
}

// The boss's next attack (its state): one of six drawn (rand_mod(6) through the table L18:0x1f2c00: 13 the rings, 14
// the energy ball, 15 the shell barrage, 16 the bomb throw, 17 the robots, 19 the charge) that the phase allows (the
// halfwords L18:0x1dfb40, six a phase), not the last one (+0x350), the charge only while allowed (+0x364 0) and with the
// hero within L18:0x162378 + 2 of its box (+0x25c), the robots not right after the energy ball; 100 draws at most (the
// last one drawn then).
// orig: L18:0x002f70b8
int32_t vf_boss_next_attack_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    int32_t next = lw(pv + 0x350);
    for (int32_t tries = 0;;) {
        const int32_t k = rand_mod(6);
        switch (static_cast<uint32_t>(k)) {  // (the jump table L18:0x1f2c00)
        case 0: next = 0xd; break;   // (0x002f7118)
        case 1: next = 0xe; break;   // (0x002f7120)
        case 2: next = 0xf; break;   // (0x002f7128)
        case 3: next = 0x10; break;  // (0x002f7130)
        case 4: next = 0x11; break;  // (0x002f7138)
        case 5: next = 0x13; break;  // (0x002f7140)
        default: break;
        }
        const uint32_t allow = L18_ADDR(0x001dfb40) + static_cast<uint32_t>(k) * 2 + lwu(pv + 0x34c) * 0xcu;
        int16_t ok = lh(allow);
        if (next == lw(pv + 0x350)) ok = 0;
        if (next == 0x13) {
            if (lw(pv + 0x364) != 0) ok = 0;
            else if (fpu_add(lwc1(L18_ADDR(0x00162378)), 2.0f) <
                     vec2_distance(V(HERO_AT), V(lvd::box(lvd::BOXES, lw(pv + 0x25c)) + 0x30)))
                ok = 0;
        }
        if (next == 0x11 && lw(pv + 0x350) == 0xe) ok = 0;
        ++tries;
        if (ok != 0) break;
        if (!(tries < 100)) break;
    }
    return next;
}

// The bearing from the EE point `from` to `to` against the moby `m`'s yaw (+0x48), into `out` + 0x68.
// orig: L18:0x002f7220
void vf_boss_bearing_l18(uint32_t m, uint32_t out, uint32_t from, uint32_t to) {
    const float a = atan2_f(fpu_sub(lwc1(to + 0), lwc1(from + 0)), fpu_sub(lwc1(to + 4), lwc1(from + 4)));
    swc1(out + 0x68, angle_sub(a, lwc1(m + 0x48)));
}

}  // namespace rac1
