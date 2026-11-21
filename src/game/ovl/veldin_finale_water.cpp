// Veldin's finale (level 18) class 1402: a pool's water that stands at one of two heights (its pvars +0 / +4) by the
// level's flag byte 0x13d406. Set up (state 0) it takes the next water index (the level's counter L18:0x1622f8) and waits
// (state 1); the first frame after, it is put where its turn of the flag (+8, turned over on each frame the flag is set)
// and the hero's byte 0x141400 say: at +4 (state 3) or at +0 (state 2). In state 2 it seeks +0 and goes to state 3 on its
// bit 8 of +0xbc (setting the level's L18:0x1622fc); in state 3 it seeks +4 and goes to state 2 on the bit 4 (clearing
// L18:0x1622fc, setting the flag when its turn is 0). Moving, it takes its group's mobys with it, and for its class's
// mobys the height of their water record (L18:0x1db500, 0x1190 bytes each). Fix 46 (the PAL 2.00 disc's build): seeking
// +0, it carries a swimming Ratchet up with it. Every frame it queues its draw by its kind (+0x14: L18:0x2f2970,
// 0x2f2620 with the ripple's phase L18:0x162308 turning, 0x2f27c8) on the level's draw list. Level 18 only: the addresses
// are the finale's (`orig: L18:`), its own data L18_ADDR, the base ELF's bare. Registered in veldin_finale_mobys.cpp;
// the helper's declaration in veldin_finale_own.h.
//
// The pvars: +0 / +4 its two heights, +8 its turn of the flag, +0x14 its kind, +0x18 its speed, +0x1c its water index.
#include <game/enhance/fixes102.h>
#include <game/level/level_data.h>
#include <game/math/vecmath.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/veldin_finale_mobys.h>
#include <game/ovl/veldin_finale_own.h>

namespace rac1 {

namespace {
float dt() { return lwc1(0x0015ed6c); }     // the frame's time scale
float dt_sq() { return lwc1(0x0015ed70); }  // (its square)
constexpr uint32_t FLAG = 0x0013d406;       // the level's flag byte: the water high
constexpr uint32_t HERO_BYTE = 0x00141400;  // (the hero's block +0x20b0)
constexpr int32_t WATER_CLASS = 0x57a;      // (1402)

// the water's height sought toward `goal` (4 a second)
void seek(uint32_t m, uint32_t pv, float goal) {
    seek_1d(m + 0x18, pv + 0x18, goal, fpu_mul(dt_sq(), 4.0f), fpu_mul(dt_sq(), 4.0f), fpu_mul(dt(), 4.0f));
}

// fix 46 (the 1.02 build's, PAL 2.00 disc): while the water is more than 0.1 from `goal`, Ratchet swimming (his state
// +0x208c 0x11 / 0x12) below its surface (+0x2f0) gets its height
void carry_swimmer(uint32_t m, float goal) {
    if (!fixes102_finale_water_carry()) return;
    if (!(u2f(0x3dcccccdu) < abs_f(fpu_sub(lwc1(m + 0x18), goal)))) return;
    if (lw(g::PLAYER + 0x208c) - 0x11u >= 2u) return;
    const float z = lwc1(m + 0x18);
    if (lwc1(g::PLAYER + 0x2f0) < z) swc1(g::PLAYER + 0x2f0, z);
}
}  // namespace

// The water's group at its height: each moby of its group (+0x21) at the water's z, and for the class 1402 ones their
// water record's (L18:0x1db500 + index * 0x1190, +8).
// orig: L18:0x002f2598
void vf_water_group_l18(uint32_t m) {
    const uint32_t first = lwu(L18_ADDR(0x0015ffd8));
    for (uint32_t e = lvd::group(lbu(m + 0x21));; e += 2) {
        const uint32_t o = first + ((lhu(e) & 0x7fff) << 8);
        swc1(o + 0x18, lwc1(m + 0x18));
        if (lh(o + 0xa6) == WATER_CLASS)
            swc1(L18_ADDR(0x001db500) + static_cast<uint32_t>(lw(lwu(o + 0x78) + 0x1c) * 0x1190) + 8, lwc1(m + 0x18));
        if (lh(e) < 0) break;
    }
}

// orig: L18:0x002f2310
void vf_class1402_update_l18(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002f2368)
        if (lw(L18_ADDR(0x001622fc)) != 0) sb(HERO_BYTE, lbu(L18_ADDR(0x001622fc)));
        sw(pv + 0x1c, lwu(L18_ADDR(0x001622f8)));
        sw(L18_ADDR(0x001622f8), lwu(L18_ADDR(0x001622f8)) + 1);
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 1);
        sh(m + 0x32, 0);
        sw(m + 0x94, 0);
        break;
    case 1: {  // where the flag puts it (0x002f23ac)
        if (lbu(FLAG) != 0) sw(pv + 8, (lwu(pv + 8) + 1) & 1);
        const uint32_t turn = lwu(pv + 8);
        sw(L18_ADDR(0x001622f8), 0);
        if (turn == 0 || lbu(HERO_BYTE) != 0) {
            sb(m + 0x20, 3);
            swc1(m + 0x18, lwc1(pv + 4));
        } else {
            sb(m + 0x20, 2);
            swc1(m + 0x18, lwc1(pv + 0));
        }
        break;
    }
    case 2:  // toward +0 (0x002f2478)
        if ((lbu(m + 0xbc) & 8) != 0) {
            sb(m + 0x20, 3);
            sw(L18_ADDR(0x001622fc), 1);
        }
        sb(m + 0xbc, 2);
        vf_water_group_l18(m);
        seek(m, pv, lwc1(pv + 0));
        carry_swimmer(m, lwc1(pv + 0));
        break;
    case 3:  // toward +4 (0x002f240c)
        if ((lbu(m + 0xbc) & 4) != 0) {
            sw(L18_ADDR(0x001622fc), 0);
            sb(m + 0x20, 2);
            if (lw(pv + 8) == 0) sb(FLAG, 1);
        }
        seek(m, pv, lwc1(pv + 4));
        vf_water_group_l18(m);
        sb(m + 0xbc, 1);
        break;
    default:
        break;
    }
    // (0x002f24d4) its draw by its kind
    switch (lw(pv + 0x14)) {
    case 0:
        sub_001f93e0(L18_ADDR(0x002f2970), m);
        return;
    case 1: {  // (the ripple's phase turned)
        float phase = fpu_add(lwc1(L18_ADDR(0x00162308)), fpu_mul(lwc1(L18_ADDR(0x0016230c)), dt()));
        swc1(L18_ADDR(0x00162308), phase);
        if (1.0f < phase) {
            phase = fpu_sub(phase, 1.0f);
            swc1(L18_ADDR(0x00162308), phase);
        }
        sub_001f93e0(L18_ADDR(0x002f2620), m);
        return;
    }
    case 2:
        sub_001f93e0(L18_ADDR(0x002f27c8), m);
        return;
    default:
        return;
    }
}

}  // namespace rac1
