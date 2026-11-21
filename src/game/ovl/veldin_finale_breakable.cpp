// Veldin's finale (level 18) class 885: a breakable piece (the update of class 885 and of class 892). At the start it is
// gone if the level's list of things broken for good says so (the byte of its index +0xb0 in the level's row of
// 0x14c050), else it waits (state 1) and marks the record its +0 names (3). Hit by the mask 0x80000 it puts its group
// (+0x21) in state 2 with a sound. The group's state 2 breaks it: a class 892 piece bursts into twenty particles where it
// stands and is gone; the others fly off away from the hero, spinning, falling (state 3), and blow up on what they meet or
// 16 below where they started. Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the
// base ELF's bare. Registered in veldin_finale_mobys.cpp.
//
// The pvars: +0 a record to mark, +8 the spin per frame, +0xc a second spin (set, never used), +0x10 the flight's
// velocity, +0x1c the height it started from.
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>
#include <game/ovl/weapons.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO_POS = 0x0013f3d0;   // the hero's position (0x13f350 + 0x80)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// the burst's colours: two tables of six words in the level's data (the first L18:0x1f29a0, the second L18:0x1f29b8)
uint32_t colour_a(int32_t k) { return lwu(L18_ADDR(0x001f29a0) + static_cast<uint32_t>(k) * 4); }
uint32_t colour_b(int32_t k) { return lwu(L18_ADDR(0x001f29b8) + static_cast<uint32_t>(k) * 4); }

// a point at the moby, raised by 0 .. 6
void raised_point(uint32_t m, uint32_t at) {
    lq_sq(at, m + 0x10);
    const float up = rand_range_f(0.0f, 6.0f);
    swc1(at + 8, fpu_add(lwc1(at + 8), up));
}

// a class 892 piece's burst: ten pairs of glows (type 0x0b) and ten sparks (type 8) flying off; `buf` two quadwords of the
// frame
void burst(uint32_t m, uint32_t buf) {
    const uint32_t at = buf, vel = buf + 0x10;
    for (int k = 0; k < 10; ++k) {
        const float size = fpu_mul(rand_range_f(8.0f, 10.0f), dt());
        raised_point(m, at);
        const int32_t ka = rand_mod(6);
        const int32_t kb = rand_mod(6);
        const int32_t life1 = rand_range(frame_scale_i(15), frame_scale_i(20));
        const int32_t life2 = rand_range(frame_scale_i(30), frame_scale_i(45));
        spawn_particle_0b(at, L18_ADDR(0x0015f580), colour_a(ka), colour_b(kb), static_cast<uint32_t>(life1), static_cast<uint32_t>(life2),
                          0, 0, 1500000.0f, size);
        const int32_t life3 = rand_range(frame_scale_i(5), frame_scale_i(10));
        const int32_t life4 = rand_range(frame_scale_i(15), frame_scale_i(20));
        spawn_particle_0b(at, L18_ADDR(0x0015f580), 0x7fffffffu, 0x00ffffffu, static_cast<uint32_t>(life3), static_cast<uint32_t>(life4), 0, 0,
                          1500000.0f, fpu_mul(size, 0.5f));
    }
    for (int k = 0; k < 10; ++k) {
        raised_point(m, at);
        sd(vel, 0);
        sd(vel + 8, 0);
        swc1(vel + 0, rand_range_f(-1.0f, 1.0f));
        swc1(vel + 4, rand_range_f(-1.0f, 1.0f));
        swc1(vel + 8, rand_range_f(-1.0f, 1.0f));
        const float speed = fpu_mul(rand_range_f(0.0f, 3.0f), dt());
        vec3_set_length(V(vel), V(vel), speed);
        const int32_t ka = rand_mod(6);
        const int32_t kb = rand_mod(6);
        const int32_t life = rand_range(frame_scale_i(30), frame_scale_i(45));
        spawn_particle_08(at, vel, colour_a(ka), colour_b(kb), life, 500000.0f);
    }
}

// the flight's start: a speed of 10 .. 15 a second away from the hero (flat), plus a random push; spins of up to 90 degrees
// a second either way
void launch(uint32_t m, uint32_t pv, uint32_t push) {
    const float speed = fpu_mul(rand_range_f(10.0f, 15.0f), dt());
    sw(m + 0x94, 0);
    sb(m + 0x20, 3);
    swc1(pv + 0x8, fpu_mul(fpu_mul(rand_range_signed_f(0.0f, 90.0f), u2f(0x3c8efa35u)), dt()));  // (degrees to radians)
    swc1(pv + 0xc, fpu_mul(fpu_mul(rand_range_signed_f(0.0f, 90.0f), u2f(0x3c8efa35u)), dt()));
    vec3_sub(V(pv + 0x10), V(m + 0x10), V(HERO_POS));
    swc1(pv + 0x18, 0.0f);
    vec3_set_length(V(pv + 0x10), V(pv + 0x10), speed);
    rand_sphere_point(push, speed, speed);
    vec3_add(V(pv + 0x10), V(pv + 0x10), V(push));
    swc1(pv + 0x18, abs_f(lwc1(pv + 0x18)));
    swc1(pv + 0x1c, lwc1(m + 0x18));
}
}  // namespace

// the moby group `group` in state 2 (class 885's breaking)
// orig: L18:0x002e9e50
void vf_group_break_l18(uint32_t group) {
    moby_group_set_state(static_cast<int32_t>(group), 2);
}

// orig: L18:0x002e9768
void vf_class885_update_l18(uint32_t m) {
    const Frame f(336);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    switch (lbu(m + 0x20)) {
    case 0: {  // (0x002e97ec)
        const uint32_t index = lbu(m + 0xb0);
        if (index != 0xff && lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + index) == 0xff) {  // (broken for good on this level)
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        const uint32_t rec = lwu(pv + 0);
        if (rec != 0) sw(rec, 3);
        return;
    }
    case 1:  // waiting for a hit (0x002e9838)
        if (sub_0025a420(m, 0x00080000, 0) != 0) {
            vf_group_break_l18(lbu(m + 0x21));
            moby_sound_play(0, 0, m, 0x375);
        }
        sb(m + 0xa4, 0xff);
        return;
    case 2:  // broken (0x002e9878)
        if (lh(m + 0xa6) == 0x37c) {  // (class 892)
            burst(m, sp);
            moby_retire(m);
        } else {
            launch(m, pv, sp);
        }
        return;
    case 3:  // flying (0x002e9d54)
        swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 8)));
        swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 8)));
        swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt_sq(), 20.0f)));
        vec3_add(V(pos), V(pos), V(pv + 0x10));
        if (!(lwc1(m + 0x18) < fpu_sub(lwc1(pv + 0x1c), 16.0f)) && sphere_collide(pos, 1.0f, 2, m) == 0) return;
        explosion_burst(m, pos, -1, 1.5f, 0.0f);
        moby_retire(m);
        return;
    default:
        return;
    }
}

}  // namespace rac1
