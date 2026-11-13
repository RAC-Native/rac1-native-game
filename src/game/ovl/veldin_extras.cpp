// Veldin's engine functions first reached unported on Rilgar (see veldin_extras.h).
#include <game/ovl/veldin_extras.h>

#include <game/ovl/helmet_tr.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/particle_28_tr.h>
#include <game/ovl/particle_types_tr.h>
#include <game/ovl/particle_types2_tr.h>
#include <game/ovl/shared_classes_tr.h>
#include <game/ovl/shared_particle_kinds.h>
#include <game/ovl/shared_particles.h>

namespace rac1 {

// orig: 0x002d7f38
void class775_update(uint32_t m) {
    const float f0 = fpu_mul(lwc1(m + 0x2c), u2f(0x3f833333u));  // lui at,0x3f83; ori at,at,0x3333; mul.s f0,f0,f1
    const uint32_t v0 = static_cast<uint32_t>(lbu(m + 0x23)) - 3u;  // addiu v0,v0,-3
    sb(m + 0x23, v0);
    swc1(m + 0x2c, f0);  // (the branch's delay slot)
    if ((v0 & 0xffu) < 4u) moby_retire(m);  // sltiu v1,v1,4; beqz; jal 0x24eb68
}

// orig: (none) — registers the functions for the indirect calls (ovl_call_moby_fn)
void veldin_extras_register() {
    ovl_register_moby_fn(0x002d7f38, class775_update);
    ovl_register_moby_fn(0x00274518, particle_43_update);  // (particle type 0x43)
    ovl_register_moby_fn(0x00271c88, particle_30_update);  // (particle type 0x30)
    ovl_register_moby_fn(0x00271df8, particle_32_update);  // (particle type 0x32)
    ovl_register_moby_fn(0x00271ec8, particle_33_update);  // (particle type 0x33)
    ovl_register_moby_fn(0x00268d48, particle_00_update);  // (particle type 0: met on Batalia)
    ovl_register_moby_fn(0x00268fe8, particle_01_update);  // (particle type 1: met on Batalia)
    ovl_register_moby_fn(0x00275860, particle_4d_update);  // (particle type 0x4d: met on Batalia)
    ovl_register_moby_fn(0x002710a0, particle_2b_update);  // (particle type 0x2b: met on Batalia)
    ovl_register_moby_fn(0x00275510, particle_4a_update);  // (particle type 0x4a, the Sandmouse's: met on Batalia)
    ovl_register_moby_fn(0x00273448, particle_3d_update);  // (particle type 0x3d)
    ovl_register_moby_fn(0x00273ee0, particle_41_update);  // (particle type 0x41)
    ovl_register_moby_fn(0x00272bc0, particle_3a_update);  // (particle type 0x3a)
    ovl_register_moby_fn(0x00271d20, bullet_particle_update);  // (particle type 0x31: the bullets of Kerwan's helicopter, L3:0x260210; shared_particles.cpp)
    ovl_register_moby_fn(0x0026a880, particle_0a_update);  // (particle type 0x0a, smoke: Novalis' jets L1:0x27f660, Kerwan's class 845 L3:0x258a30, Eudora L4:0x25d0e8)
    ovl_register_moby_fn(0x0026ba68, particle_0e_update);  // (particle type 0x0e: class 156, L1:0x280970, L6:0x27a850, L8:0x275bd8, L9:0x28a428)
    ovl_register_moby_fn(0x0026fc38, particle_27_update);  // (particle type 0x27: Batalia L8:0x279f00, from bat_2b8228_l8)
    ovl_register_moby_fn(0x00275320, particle_49_update);  // (particle type 0x49: Batalia's class 1400, L8:0x27f5f0)
    ovl_register_moby_fn(0x0026eb88, particle_1f_update);  // (particle type 0x1f: Giant Clank's streaks, L15:0x25e2c0; L18:0x270af0)
    ovl_register_moby_fn(0x00275eb0, beam_particle_update);  // (particle type 0x50: Drek's fleet's laser beams, L17:0x26fdd0 from class 1368; shared_particle_kinds.cpp)
    // the particle types no level's code spawns (0x2a: only its own update 0x270948, through 0x270758), registered so that none can
    // hang (an unregistered update logs TODO_PORT and the particle neither moves nor expires): oracle batch 340
    ovl_register_moby_fn(0x002694f8, particle_03_update);  // (particle type 0x03)
    ovl_register_moby_fn(0x0026a418, particle_07_update);  // (particle type 0x07)
    ovl_register_moby_fn(0x0026a6f0, particle_09_update);  // (particle type 0x09)
    ovl_register_moby_fn(0x0026c3c8, particle_11_update);  // (particle type 0x11)
    ovl_register_moby_fn(0x0026c860, particle_14_update);  // (particle type 0x14)
    ovl_register_moby_fn(0x0026d470, particle_18_update);  // (particle type 0x18)
    ovl_register_moby_fn(0x0026e3a8, particle_1d_update);  // (particle type 0x1d)
    ovl_register_moby_fn(0x0026e670, particle_1e_update);  // (particle type 0x1e)
    ovl_register_moby_fn(0x0026f048, timed_particle_update);  // (particle type 0x21; shared_particles.cpp)
    ovl_register_moby_fn(0x0026f898, particle_24_update);  // (particle type 0x24)
    ovl_register_moby_fn(0x0026fa00, particle_25_update);  // (particle type 0x25)
    ovl_register_moby_fn(0x0026fb00, particle_26_update);  // (particle type 0x26)
    ovl_register_moby_fn(0x00270948, particle_2a_update);  // (particle type 0x2a)
    ovl_register_moby_fn(0x00272438, particle_36_update);  // (particle type 0x36)
    ovl_register_moby_fn(0x002738b0, timed_particle_update);  // (particle type 0x3f; shared_particles.cpp)
    ovl_register_moby_fn(0x002756f0, particle_4b_update);  // (particle type 0x4b)
    ovl_register_moby_fn(0x00275810, particle_4c_update);  // (particle type 0x4c)
    ovl_register_moby_fn(0x002b4198, class167_update_tr);  // (class 167, the Thruster-Pack's jets)
    ovl_register_moby_fn(0x002b3628, class167_draw_tr);    // (its draw-list callback)
    ovl_register_moby_fn(0x002e03a0, helmet_update_tr);    // (class 1290, the Pilot's Helmet pickup: Gaspar)
    ovl_register_moby_fn(0x0026ff10, particle_28_update);  // (particle type 0x28: Orxon)
}

}  // namespace rac1
