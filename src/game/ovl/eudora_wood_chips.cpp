// Eudora's class 1516, the wood chips the Woodcut Bot (class 563) throws as it chops: twenty at a time, spawned by its
// helper L4:0x2e4fe0 with a random turn, a velocity and a size. A chip flies along its velocity under gravity, spinning;
// once its timer has run out it may bounce off the level (as many times as its bounce count; the bot's chips have none);
// 3 below where it started it shrinks and fades out, then is gone. Header eudora_mobys.h; registered in eudora_mobys.cpp.
// Addresses are Eudora's (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
//
// pvars: the bot +0, the starting height +4, the bounce radius +8, the bounces left +0xc, the bounce timer +0xe, the
// velocity +0x10, the spin +0x20 / +0x24 / +0x28 (about x, y, z). States: 0 flying, 1 fading out.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
const float kDegree = u2f(0x3c8efa35u);     // (pi / 180)

// a spin after a bounce: a random signed angle of the level's range (L4:0x161d78 .. 0x161d7c degrees) per frame
float chip_spin() {
    return fpu_mul(fpu_mul(rand_range_signed_f(lwc1(L4_ADDR(0x00161d78)), lwc1(L4_ADDR(0x00161d7c))), kDegree), dt());
}
}  // namespace

// orig: L4:0x002e5178
void eud_class1516_update_l4(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x70);
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10, vel = pv + 0x10;
    lq_sq(sp, pos);  // (the original keeps the old position at sp + 0; nothing reads it)
    swc1(pv + 0x18, fpu_sub(lwc1(pv + 0x18), fpu_mul(dt_sq(), 20.0f)));  // gravity
    vec3_add(V(pos), V(pos), V(vel));
    swc1(m + 0x40, angle_add(lwc1(m + 0x40), lwc1(pv + 0x20)));
    swc1(m + 0x44, angle_add(lwc1(m + 0x44), lwc1(pv + 0x24)));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(pv + 0x28)));
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002e522c) flying: 3 below its start fading out; else a bounce off the level while it moves into it
        if (lwc1(m + 0x18) < fpu_sub(lwc1(pv + 4), 3.0f)) {
            sb(m + 0x20, 1);
            break;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0xe))) == 0) break;
        if (lh(pv + 0xc) == 0) break;
        if (sphere_collide(pos, lwc1(pv + 8), 2, 0) == 0) break;
        if (!(vec3_dot(V(vel), coll_hit_normal()) < 0.0f)) break;
        V(pos) = coll_hit_pushed();
        sh(pv + 0xc, lhu(pv + 0xc) - 1);
        vec3_reflect(V(vel), V(vel), coll_hit_normal());
        vec3_scale(V(vel), V(vel), 0.75f);
        swc1(pv + 0x20, 0.0f);
        swc1(pv + 0x24, chip_spin());
        swc1(pv + 0x28, chip_spin());
        break;
    case 1: {  // (0x002e533c) fading out: shrinking, its alpha down by 4 a frame, then gone
        swc1(m + 0x2c, fpu_mul(lwc1(m + 0x2c), u2f(0x3f666666u)));
        const uint32_t alpha = lbu(m + 0x23);
        if (alpha < 4) {
            moby_retire(m);
        } else {
            sb(m + 0x23, alpha - 4);
        }
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x70);
}

}  // namespace rac1
