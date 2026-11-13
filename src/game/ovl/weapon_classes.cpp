// Ported overlay code: the shared (Veldin-address) classes of weapon_classes.h that belong to no weapon's own file: class 1143
// (the Vendor Orb, 0x2df730), class 166 (0x2b3458) and particle type 5 (0x269c70); and the registrations of them all for
// the indirect calls (every level's own overlay maps its copy to Veldin's address).
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/weapon_classes.h>

namespace rac1 {

// Class 1143, the Vendor Orb (spawned by the Vendor's update 0x2a57a8, which also sets its no-update flag: never run in the
// game): it starts at a random spin on its chain point 1, then turns (+0x48) by its rate and spins its orb's quaternion
// (pvar +0x10, by +0x40) the other way twice as fast.
// orig: 0x002df730
void vendor_orb_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x73, 0x20);
        swc1(pv + 0x40, rand_angle());
        sub_0024f590(m, 1, pv);
        return;
    }
    if (st != 1) return;
    const float turn = fpu_mul(fpu_mul(lwc1(OV(0x00161bb0u)), u2f(0x3c8efa35u)), lwc1(0x0015ed6cu));
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), turn));
    const float spin = angle_add(lwc1(pv + 0x40), fpu_mul(turn, -2.0f));
    swc1(pv + 0x40, spin);
    quat_from_axis_angle(V(pv + 0x10), 2, spin);
}

// Class 166: four linked mobys (pvar +0x10..+0x1c, their indices; state 0 makes them pointers at +0x20..+0x2c). While one
// of them waits (state 0x50), every so often (+4, the level's period 0x161478) the first waiting one is released (0x51) at
// this moby's place. Hits count down its lives (+8, 3; again after +0xc, the level's 0x161480); retired at none.
// orig: 0x002b3458
void class166_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 0) {
        sb(m + 0x20, 1);
        const uint32_t first = lwu(OV(0x0015ffd8u));
        sw(pv + 0x2c, first + (lwu(pv + 0x1c) << 8));
        sw(pv + 0x04, lwu(OV(0x00161478u)));
        sw(pv + 0x08, 3);
        sw(pv + 0x20, first + (lwu(pv + 0x10) << 8));
        sw(pv + 0x24, first + (lwu(pv + 0x14) << 8));
        sw(pv + 0x28, first + (lwu(pv + 0x18) << 8));
        return;
    }
    if (st != 1) return;
    const uint32_t hit = sub_0025a420(m, 0x10000u, 0);
    timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0xc)));
    if (hit != 0 && lw(pv + 0xc) == 0) {
        sb(m + 0xa4, 0xff);
        const int32_t lives = lw(pv + 8) - 1;
        sw(pv + 0xc, lwu(OV(0x00161480u)));
        sw(pv + 8, static_cast<uint32_t>(lives));
        if (lives <= 0) {
            moby_retire(m);
            return;
        }
    }
    bool waiting = false;
    for (uint32_t k = 0; k < 4 && !waiting; ++k) waiting = lbu(lwu(pv + 0x20 + k * 4) + 0x20) == 0x50;
    if (!waiting) return;
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 4))) == 0) return;
    sw(pv + 4, lwu(OV(0x00161478u)));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t linked = lwu(pv + 0x20 + k * 4);
        if (lbu(linked + 0x20) != 0x50) continue;
        sb(linked + 0x20, 0x51);
        V(linked + 0x10) = V(m + 0x10);
        sb(linked + 0xa4, 0xff);
        return;
    }
}

// Particle type 5: grows by +0x24 a frame; in its last 0.1 s its alpha (+4's top byte) fades with its life (+0x20); freed
// when the life runs out.
// orig: 0x00269c70
void particle_05_update(uint32_t p) {
    if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(p + 0x20))) != 0) {
        free_particle(p);
        return;
    }
    swc1(p + 0xc, fpu_add(lwc1(p + 0xc), lwc1(p + 0x24)));
    if (!(lw(p + 0x20) < frame_scale_i(6))) return;
    const int32_t span = frame_scale_i(6);
    const int32_t life = lw(p + 0x20);
    const int32_t alpha = ee_div(life * 0x7f, span);
    sw(p + 4, (lwu(p + 4) & 0x00ffffffu) | (static_cast<uint32_t>(alpha) << 24));
}

// orig: (none) -- registers the functions for the indirect calls (ovl_call_moby_fn)
void weapon_classes_register() {
    ovl_register_moby_fn(0x002df730, vendor_orb_update);   // (class 1143, the Vendor Orb: spawned by the Vendor's update 0x2a57a8)
    ovl_register_moby_fn(0x002ce358, drone_update);        // (class 479, the drones the Drone Device spawns: 0x2cdcc0 / 0x2ce1f0)
    ovl_register_moby_fn(0x002c97e0, chicken_update);      // (class 270, the Morph-o-Ray's chicken: 0x2c9348)
    ovl_register_moby_fn(0x002caf10, chicken_hook_c);      // (class 270's hook +0xc, the table 0x1ea7d4: called by 0x2dc9f0)
    ovl_register_moby_fn(0x002cc210, class428_update);     // (class 428, spawned by the chicken's update through 0x2cbf68)
    ovl_register_moby_fn(0x002d7f88, class804_update);     // (class 804, on the Morph-o-Ray's hand moby, class 0xb9)
    ovl_register_moby_fn(0x002b3458, class166_update);     // (class 166)
    ovl_register_moby_fn(0x00236b10, morph_hud_start);     // (the Morph-o-Ray's HUD element, slot 4 key 0x7533, from 0x2bc7e8: start)
    ovl_register_moby_fn(0x00236d00, morph_hud_update);    // (its update)
    ovl_register_moby_fn(0x00236e50, morph_hud_draw);      // (its draw)
    ovl_register_moby_fn(0x00269c70, particle_05_update);  // (particle type 5, the table 0x1b1e80)
    ovl_register_moby_fn(0x002726d8, particle_37_update);  // (particle type 0x37: the drones' trails)
}

}  // namespace rac1
