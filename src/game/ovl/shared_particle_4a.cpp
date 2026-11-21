// Particle type 0x4a's spawn: one port of the copies twelve levels carry, the same code relinked (the normalised texts
// compared). Its one level-own word is the pointer to the level's texture byte of the type (per level below); the rest is
// engine code (alloc_particle) and the base ELF's random (libc_rand). Header shared_particle_4a.h.
//
// The particle: +1 0, +2 the texture, +3 its flags (0x44, additive 0x48), +4 the record's word, +8 a random byte, +9 its
// size (4 - 0x60), +0xa its life, +0xc 0, +0x10 its place, +0x20 its velocity, +0x2c the moby, +0x30 its place in the
// moby's coordinates, +0x3c the record.
#include <game/ovl/shared_particle_4a.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

namespace {
// the level's word that points at the type's texture byte (its own data)
uint32_t texture_word() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x001b2628);
    case ARIDIA: return L2_ADDR(0x001b27a8);
    case KERWAN: return L3_ADDR(0x001b22a8);
    case EUDORA: return L4_ADDR(0x001b2328);
    case RILGAR: return L5_ADDR(0x001b2628);
    case BLARG: return L6_ADDR(0x001b2928);
    case BATALIA: return L8_ADDR(0x001b29a8);
    case POKITARU: return L11_ADDR(0x001b2ba8);
    case HOVEN: return L12_ADDR(0x001b2628);
    case GEMLIK: return L13_ADDR(0x001b24a8);
    case OLTANIS: return L14_ADDR(0x001b28a8);
    default: return L17_ADDR(0x001b2aa8);
    }
}
}  // namespace

// orig: L1:0x0028a7a8 L2:0x002768c8 L3:0x002637f0 L4:0x00267eb0 L5:0x0029f5f0 L6:0x00284190 L8:0x0027f950 L11:0x0029a738 L12:0x0028de98 L13:0x00281100 L14:0x0027d310 L17:0x0026f1a8
uint32_t particle_4a_spawn(uint32_t m, uint32_t pos, uint32_t vel, uint32_t rec, uint32_t additive) {
    const uint32_t p = alloc_particle(0x4a);
    if (p == 0) return 0;
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t local = sp, undo = sp + 0x10;
    sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) - 0x60));
    sb(p + 3, additive != 0 ? 0x48u : 0x44u);
    sb(p + 1, 0);
    sb(p + 2, lbu(lwu(texture_word())));
    sb(p + 8, static_cast<uint32_t>(libc_rand()));
    sw(p + 0xc, 0);
    lq_sq(p + 0x10, pos);
    swc1(p + 0x20, lwc1(vel));
    swc1(p + 0x24, lwc1(vel + 4));
    swc1(p + 0x28, lwc1(vel + 8));
    vec3_sub(V(local), V(pos), V(m + 0x10));
    for (uint32_t k = 0; k < 0x40; k += 4) sw(undo + k, 0);  // (its translation row stays 0)
    mat3_transpose(M(undo), M(m + 0xc0));
    mat4_transform(V(local), V(local), M(undo));
    swc1(p + 0x30, lwc1(local));
    swc1(p + 0x34, lwc1(local + 4));
    swc1(p + 0x38, lwc1(local + 8));
    sw(p + 0x2c, m);
    sw(p + 0x3c, rec);
    sh(p + 0xa, lhu(rec + 0xc));
    sw(p + 4, lwu(rec));
    ee_stack_free(0xc0);
    return p;
}

}  // namespace rac1
