// Eudora's class 481: a float that bobs on the water a Hydrodisplacer pump (class 280, shared_class280.cpp; its pvars +0
// the work done, 0 .. 1) lowers. It is set 15 above where it was placed and lowered by 15 times the work of the pump whose
// moby index its pvars +0x230 hold (no pump: the whole 15; a moby there of another class: not lowered); it bobs by 0.75
// over its 118-frame animation and turns its three joints (their angles +0x224 .. +0x22c) at the level's rate
// (L4:0x16195c, degrees a second) on springs. Every frame its move is recorded in its ride frame (+0x60) so that what
// stands on it moves with it. A moby with no pvars is taken off. States: 0 set up, 1 floats. Header eudora_mobys.h;
// registered in eudora_mobys.cpp. Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are
// bare.
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
constexpr int32_t PUMP_CLASS = 0x118;
const float kTwoPi = u2f(0x40c90fdbu);
const float kDegree = u2f(0x3c8efa35u);  // (pi / 180)
float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// the level's moby of index `idx` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t idx) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }
}  // namespace

// pvars: the height +0x220 (15 above its placement), the joints' spring records +0xa0 + 0x80 * i (each with its angle at
// +0x68, i.e. +0x108 + 0x80 * i), the joints' angles +0x224 .. +0x22c, the pump's moby index +0x230 (-1: none), the ride
// frame +0x60 (the move of this frame at +0x70), +0x3e set to 0xd.
// orig: L4:0x002cdda0
void eud_class481_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x100);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, was = sp, moved = sp + 0x10;  // (EE frame locals: the place before, the move)
    lq_sq(was, pos);
    if (pv == 0) {
        moby_retire(m);
        ee_stack_free(0x100);
        return;
    }
    switch (lbu(m + 0x20)) {
    case 0:  // set up: a random animation rate, 15 up, three random joint angles
        swc1(m + 0x58, rand_range_f(0.5f, u2f(0x3f4ccccdu)));
        swc1(pv + 0x220, fpu_add(lwc1(m + 0x18), 15.0f));
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        sh(pv + 0x3e, 0xd);
        for (uint32_t i = 0; i < 3; ++i) swc1(pv + 0x224 + i * 4, rand_angle());
        break;
    case 1: {  // floating
        const float bob = fpu_mul(cos_vu(angle_wrap(fpu_div(fpu_mul(moby_anim_time(m), kTwoPi), 118.0f))), 0.75f);
        for (uint32_t i = 0; i < 3; ++i) {
            const float turn = fpu_mul(fpu_mul(lwc1(L4_ADDR(0x0016195c)), kDegree), dt());
            const float angle = angle_add(lwc1(pv + 0x224 + i * 4), turn);
            swc1(pv + 0x224 + i * 4, angle);
            swc1(pv + 0x108 + i * 0x80, angle);
            const float step = lwc1(0x0015ed64);  // (the frame's time step)
            joint_node_spring(m, pv + 0xa0 + i * 0x80, i, fpu_mul(step, u2f(0x3cf5c28fu)), fpu_mul(step, u2f(0x3e99999au)));
        }
        float lowered = 0.0f;
        if (const int32_t pump = lw(pv + 0x230); pump == -1) {
            lowered = 15.0f;
        } else if (const uint32_t p = moby_at(pump); lh(p + 0xa6) == PUMP_CLASS) {
            lowered = fpu_mul(lwc1(lwu(p + 0x78)), 15.0f);
        }
        swc1(m + 0x18, fpu_sub(fpu_add(lwc1(pv + 0x220), bob), lowered));
        break;
    }
    default:
        break;
    }
    vec3_sub(V(moved), V(pos), V(was));
    attach_frame_set(pv + 0x60, moved, m + 0x40, m + 0x40);
    ee_stack_free(0x100);
}

}  // namespace rac1
