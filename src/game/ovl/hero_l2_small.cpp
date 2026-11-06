// Aridia's (level 2) own variants of the small hero-controller functions: the same
// source as Veldin's, built with other features, so the code differs. Addresses are Aridia's.
// Veldin-equivalent globals go through OV (the level's own address); the player block is in the
// base ELF (the same on every overlay).
#include <game/enhance/fixes102.h>
#include <game/ovl/hero_l2.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;

// the hero's feet within 1 of the height h (0.25 above them) and below it, coming down
// orig: (helper) the hazard tests of hero_surface_update (L2 blocks 0x21b864 / 0x21b8fc / 0x21b9cc)
bool sinking_into(float h) {
    if (!(abs_f(h - (lwc1(P + 0x88) + 0.25f)) < 1.0f)) return false;
    return 0.0f < h - lwc1(P + 0x88);
}
}  // namespace

// The surface under the hero (Veldin's 0x20b960), Aridia's build: the flags (+0x12e0 consumed and reset
// to -1; +0x12ed kept) of kind 2 (deep water: +0x12e7; the 0xad moby +0x10e0 sets +0x308), 3 (lava:
// +0x12e6), 0xb (+0x12eb), 0xd (+0x12ec), 8 / 0xc (+0x12ea), 9 (+0x12ee), and on level 0xd a type-0xb wall
// at the gait centre sinks the hero (0x7b). Not in this build: the wading kinds 0xe / 0 (+0x12e4, the
// depth +0x22a4), the slippery kind 4 (+0x12e3, state 0x31), 7 (+0x12e2) and the deadly water 1
// (+0x12e5, states 0x7d / 0x3c / 0x7c). Then the states the flags force: 0x7f for +0x12ec once sunk in
// and coming down, the lava's 0x68 / 0x7b and the quicksand's 0x7b.
// orig: L2:0x0021b698
void hero_surface_update_l2() {
    const uint32_t keep = lbu(P + 0x12ed);
    const int32_t kind = lh(P + 0x12e0);
    mem_clear128(P + 0x12e0, 0x10);
    sb(P + 0x12ed, keep);
    sh(P + 0x12e0, 0xffff);
    sb(P + 0x20a9, 0);
    sh(P + 0x308, 0);
    if (kind == -1) return;
    if (kind == 2 && (lh(P + 0x30c) == 0 || lwc1(P + 0x2dc) < u2f(0x3e99999au))) {
        sb(P + 0x12e7, 1);
        if (lbu(P + 0x20a4) == 0) {
            const uint32_t m = lwu(P + 0x10e0);
            if (m != 0 && lh(m + 0xa6) == 0xad) sh(P + 0x308, 1);
        }
    }
    if (kind == 3) sb(P + 0x12e6, 1);
    if (lw(0x0015ed84) == 0xd && lw(P + 0x2084) != 0x7b) {
        const float r = lwc1(P + 0x234) + u2f(0x3cf5c28fu);
        if (sphere_collide_v(V(P + 0xd0), r, 2, 0) != 0 && sub_001f35d8() == 0xb) {
            hero_set_state(0x7b, 1);
            return;
        }
    }
    if (kind == 0xb) sb(P + 0x12eb, 1);
    if (kind == 2) sb(P + 0x12e7, 1);
    if (kind == 0xd) sb(P + 0x12ec, 1);
    if (kind == 8) sb(P + 0x12ea, 1);
    if (kind == 9) sb(P + 0x12ee, 1);
    if (kind == 0xc) sb(P + 0x12ea, 1);
    if (lbu(P + 0x12ec) != 0 && lw(P + 0x2084) != 0x7f && sinking_into(lwc1(P + 0x2f0)) && lwc1(P + 0x108) < 0.0f) {
        hero_mode3_exit();
        hero_set_state(0x7f, 1);
        return;
    }
    if (lbu(P + 0x12e6) != 0 && lw(P + 0x2084) != 0x68 && lw(P + 0x2084) != 0x7b && sinking_into(lwc1(P + 0x2f4))) {  // lava
        const bool falling = lw(P + 0x2084) != 0x69 || lh(P + 0x41e) != 0;
        if (falling && lwc1(P + 0x108) < 0.0f) {
            hero_mode3_exit();
            hero_set_state(lw(P + 0x22a8) != 0 ? 0x68u : 0x7bu, 1);
            return;
        }
    }
    if (lbu(P + 0x12eb) == 0 || lw(P + 0x2084) == 0x7b) return;  // quicksand
    if (!sinking_into(lwc1(P + 0x2f4)) || !(lwc1(P + 0x108) < 0.0f)) return;
    if (gameplay102_quicksand_lost()) return;  // (1.02 Gameplay, fix 9: the 1.02 build's Aridia, 0x21c048)
    hero_mode3_exit();
    hero_set_state(0x7b, 1);
}

}  // namespace rac1
