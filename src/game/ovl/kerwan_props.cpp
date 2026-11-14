// Kerwan's own moby classes, part 1: the small props, citizens and mechanisms. Header
// kerwan_mobys.h. Addresses are Kerwan's (`orig: L3:`); data only Kerwan
// has is L3_ADDR, the globals the engine shares with Veldin are the level's own (OV), the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/kerwan_mobys.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

// Classes 915 / 916 / 917 (Fred 1 / 2 / 3, Kerwan's citizens that the game never shows): the distance to the hero,
// the angle towards him and its difference to the moby's own facing are computed and thrown away, then the moby
// retires on its first update (cut content). Orxon's class 915 is the same code (the base ELF's data and Veldin's functions
// only): this port serves it.
// orig: L3:0x002dc310 L10:0x002d7fe8
void fred_update_l3(uint32_t m) {
    (void)vec3_distance(V(m + 0x10), V(0x0013f3d0));
    const float to = atan2_f(fpu_sub(lwc1(0x0013f3d0), lwc1(m + 0x10)), fpu_sub(lwc1(0x0013f3d4), lwc1(m + 0x14)));  // (the hero's x, y)
    (void)angle_dist(to, lwc1(m + 0x48));
    moby_retire(m);
}

// Class 1827 (Lightpost): state 0 -> 1; in state 1 a hit record (sub_0025a420 with the mask 0x10000) with a
// positive damage (+0x2c) -> state 2; state 2 bursts: the sound, a bolt drop (0x251da8, the same code as Novalis's
// plant bolts), a debris burst of class 0x725 and a copy of class 0x724 (0x2523f8, Novalis's moby clone), and the
// moby is removed.
// orig: L3:0x002e2948
void lightpost_update_l3(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        bool hurt = false;
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) hurt = true;
        if (hurt) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    debris_burst(m, 0x725, 1, 0x725, 1, 1, 2);
    moby_clone_l1(m, 0x724);
    moby_retire(m);
}

// Class 817 (Statue): as the lightpost up to state 2, which plays the sound, drops the bolts, throws three debris
// classes (0x727, 0x728, 0x729) from its place and retires (no clone).
// orig: L3:0x002d3cd8
void statue_update_l3(uint32_t m) {
    const uint32_t hit = sub_0025a420(m, 0x10000, 0);
    const uint32_t st = lbu(m + 0x20);
    if (st == 1) {
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        sb(m + 0x20, 1);
        return;
    }
    if (st != 2) return;
    moby_sound_start(0, 0, m);
    plant_bolts_drop_l1(m);
    const uint32_t z = OV(0x0015f580);
    debris_spawn(m, 0x727, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    debris_spawn(m, 0x728, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    debris_spawn(m, 0x729, m + 0x10, m + 0x40, 0, 0, z, z, z, 0.0f);
    moby_retire(m);
}

// Class 825 (Robo Shack Sign): always drawn (+0x30 = 0xff, +0x32 = 0xff), turns about z by 0.4795 (fs6c x
// 0x3ef5be0b) a frame and sets the attached frame (pvar +0x20) from the zero position and the angles as they were.
// orig: L3:0x002d3e58
void robo_shack_sign_update_l3(uint32_t m) {
    const Vec4 zero = {0.0f, 0.0f, 0.0f, 0.0f};
    const Vec4 base = V(m + 0x40);
    sb(m + 0x30, 0xff);
    sh(m + 0x32, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(0x0015ed6c) * u2f(0x3ef5be0bu)));
    attach_frame_set_v(pv + 0x20, zero, base, V(m + 0x40));
}

// Class 997 (Garage Door): state 0 keeps its position (pvar +0..) and goes to 1 when the checkpoint-style flag of
// its index (+0xb2: the byte at 0x1bb230 + index + 0x454, or the bit in the level's table 0x14c190) says it was
// opened before; state 1 rises by fs6c a frame until 4 above the pvar's +8, then 2; state 2 holds at that height.
// orig: L3:0x002dca30
void garage_door_update_l3(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    const uint32_t pv = lwu(m + 0x78);
    if (st == 1) {
        const float z = lwc1(m + 0x18) + lwc1(0x0015ed6c);
        swc1(m + 0x18, z);
        if (lwc1(pv + 8) + 4.0f <= z) sb(m + 0x20, 2);
        return;
    }
    if (st == 0) {
        lq_sq(pv, m + 0x10);
        const uint32_t idx16 = lhu(m + 0xb2);
        const int32_t idx = static_cast<int16_t>(idx16);
        if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
        if (((lwu(0x0014c190 + (level << 8) + (static_cast<uint32_t>(idx >> 5) << 2)) >> (idx16 & 0x1f)) & 1u) != 0) sb(m + 0x20, 1);
        return;
    }
    if (st == 2) swc1(m + 0x18, lwc1(pv + 8) + 4.0f);
}

// Class 1210 (Metropolis Train Carriage): skipped when its flags +0x34 have bit 1; the first frame sets the pvar's
// +0x60 (0), +0x64 (0), +0x68 (4), +0x7e (0xd), the distance from the chain point 0 to its place at +0xc4, keeps its
// place / angles at pvar +0xa0 / +0xb0, +0x5c = 3 and draws always; each frame it sets the attached frame (pvar
// +0x20) from the move since the last frame (place - the kept place, the kept angles, its angles) and keeps the
// new place and angles.
// orig: L3:0x002953f8
void train_carriage_update_l3(uint32_t m) {
    if ((lhu(m + 0x34) & 2u) != 0) return;
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0) {
        Vec4 pt;
        sw(pv + 0x60, 0);
        sh(pv + 0x64, 0);
        sb(pv + 0x68, 4);
        sh(pv + 0x7e, 0xd);
        moby_chain_point_v(m, 0, pt);
        swc1(pv + 0xc4, vec3_distance(V(m + 0x10), pt));
        lq_sq(pv + 0xa0, m + 0x10);
        lq_sq(pv + 0xb0, m + 0x40);
        sw(pv + 0x5c, 3);
        sb(m + 0x30, 0xff);
        sh(m + 0x32, 0xff);
    }
    Vec4 d = {0.0f, 0.0f, 0.0f, 0.0f};
    vec3_sub(d, V(m + 0x10), V(pv + 0xa0));
    attach_frame_set_v(pv + 0x20, d, V(pv + 0xb0), V(m + 0x40));
    lq_sq(pv + 0xa0, m + 0x10);
    lq_sq(pv + 0xb0, m + 0x40);
}

}  // namespace rac1
