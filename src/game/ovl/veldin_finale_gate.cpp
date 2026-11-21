// Veldin's finale (level 18) class 583: the gates (four on the level). At the start each puts a frame (class 1771) where it
// stands and two leaves (class 1776, the second turned half round); open (+0x24 its opening, not 0) the leaves stand apart
// along their own y and the gate is raised (+0x34); it shuts (+0x24 0, with a sound) when its wait +0x2c ends (unless
// +0x30), when the hero stands in his state 0x22, or in the level's mode 2 from the boss fight's phase 4 on; shutting, it
// lowers, then the leaves close. A gate standing on a moby is taken away, frame and leaves, in the mode 2 at the phase 3.
// Level 18 only: the addresses are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in
// veldin_finale_mobys.cpp.
//
// The pvars: +0 the gate's base, +0x10 its ground's height, +0x14 / +0x18 the leaves, +0x1c the frame, +0x20 the leaves'
// opening now, +0x24 the opening to reach, +0x28 the speed of the move, +0x2c the wait before it shuts, +0x30 no wait,
// +0x34 its rise, +0x38 it stands on a moby.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t HERO = 0x0013f350;       // the hero (+0x30e, his state +0x2084)

// an EE stack frame of the original's size (its locals are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

// a moby's drawn scale: its class's scale +0x24 by the level's L18:0x161998
void scale_set(uint32_t c) { swc1(c + 0x2c, fpu_mul(lwc1(lwu(c + 0x24) + 0x24), lwc1(L18_ADDR(0x00161998)))); }

// a part of the gate: shown (+0x30 0x40, +0x31 1, +0x32 `shade`), scaled, turned as the gate
void part_setup(uint32_t c, uint32_t gate, uint32_t shade) {
    sb(c + 0x30, 0x40);
    sh(c + 0x32, shade);
    sb(c + 0x31, 1);
    scale_set(c);
    lq_sq(c + 0x40, gate + 0x40);
}

// the shut sound, the opening to reach 0
void shut(uint32_t m, uint32_t pv) {
    moby_sound_start(0, 0, m);
    sw(pv + 0x24, 0);
}
}  // namespace

// orig: L18:0x002d6600
void vf_class583_update_l18(uint32_t m) {
    const Frame f(144);
    const uint32_t sp = f.sp, pv = lwu(m + 0x78), pos = m + 0x10;
    scale_set(m);
    switch (lbu(m + 0x20)) {
    case 0: {  // on the ground: its parts put (0x002d665c)
        sw(m + 0x94, 0);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        swc1(m + 0x18, floor_probe_v(0.5f, V(pos), 0));
        lq_sq(pv + 0, pos);
        if (coll_hit_moby() != 0) sw(pv + 0x38, 1);
        const uint32_t frame = moby_spawn(0x6eb);
        part_setup(frame, m, 0x56);
        lq_sq(frame + 0x10, pv + 0);
        moby_update_transform(frame);
        sw(pv + 0x1c, frame);
        const uint32_t left = moby_spawn(0x6f0);
        sw(pv + 0x14, left);
        part_setup(left, m, 0x56);
        moby_update_transform(left);
        const uint32_t right = moby_spawn(0x6f0);
        sw(pv + 0x18, right);
        part_setup(right, m, 0x40);
        swc1(right + 0x48, angle_add(lwc1(right + 0x48), u2f(0x40490fd0u)));  // (pi)
        moby_update_transform(right);
        sb(m + 0x20, 1);
        swc1(pv + 0x10, lwc1(m + 0x18));
        swc1(pv + 0x34, lwc1(L18_ADDR(0x001619a0)));
        sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
        return;
    }
    case 1:
        break;
    default:
        return;
    }
    // state 1 (0x002d67f0)
    const int32_t mode = lw(L18_ADDR(0x0015f5c4));
    if (mode == 2 && lw(pv + 0x38) != 0 && lw(L18_ADDR(0x0016d290)) == 3) {
        for (const uint32_t o : {0x14u, 0x18u, 0x1cu})
            if (lwu(pv + o) != 0) moby_retire(lwu(pv + o));
        moby_retire(m);
    }
    if (lwc1(pv + 0x24) != 0.0f) {
        bool shuts = false;
        if (lw(pv + 0x30) == 0 && timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x2c))) == 2) shuts = true;
        else if (lw(L18_ADDR(0x0015f5c4)) == 2 && !(lw(L18_ADDR(0x0016d290)) < 4)) shuts = true;
        if (shuts) shut(m, pv);
        if (lh(HERO + 0x30e) == 0 && lw(g::HERO_STATE) == 0x22) shut(m, pv);
    }
    const float goal = lwc1(pv + 0x24);
    if (goal != 0.0f) {
        if (lwc1(pv + 0x20) != goal) {  // the leaves part
            seek_1d(pv + 0x20, pv + 0x28, goal, fpu_mul(dt_sq(), 4.0f), fpu_mul(dt_sq(), 4.0f), fpu_mul(dt(), 4.0f));
        } else {  // then the gate rises
            seek_1d(pv + 0x34, pv + 0x28, lwc1(L18_ADDR(0x0016199c)), fpu_mul(dt_sq(), 16.0f), fpu_mul(dt_sq(), 4.0f), fpu_mul(dt(), 8.0f));
        }
    } else {
        const float low = lwc1(L18_ADDR(0x001619a0));
        if (lwc1(pv + 0x34) != low) {  // the gate lowers
            seek_1d(pv + 0x34, pv + 0x28, low, fpu_mul(dt_sq(), 16.0f), fpu_mul(dt_sq(), 4.0f), fpu_mul(dt(), 8.0f));
        } else {  // then the leaves close
            seek_1d(pv + 0x20, pv + 0x28, goal, fpu_mul(dt_sq(), 4.0f), fpu_mul(dt_sq(), 4.0f), fpu_mul(dt(), 4.0f));
        }
    }
    // each leaf back from the base along its own y by the opening (and a fifth of it along its z)
    for (const uint32_t o : {0x14u, 0x18u}) {
        const uint32_t leaf = lwu(pv + o);
        if (leaf == 0) continue;
        vec3_set_length(V(leaf + 0x10), V(leaf + 0xd0), -lwc1(pv + 0x20));
        vec3_set_length(V(sp), V(leaf + 0xe0), fpu_mul(-lwc1(pv + 0x20), 0.2f));
        vec3_add(V(leaf + 0x10), V(leaf + 0x10), V(sp));
        vec3_add(V(leaf + 0x10), V(leaf + 0x10), V(pv));
        moby_update_transform(leaf);
    }
    swc1(m + 0x18, fpu_add(lwc1(pv + 0x10), lwc1(pv + 0x34)));
}

}  // namespace rac1
