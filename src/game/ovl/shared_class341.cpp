// Class 341, a pad the hero steps on: one port of the copies five levels carry (Rilgar, Umbris, Pokitaru, Hoven, the
// finale's Veldin). The copies are the same code relinked (compared with the relocated fields masked); their one level-own
// datum is the level's five glow colours (a table of the level's own data, per level below); the rest is the engine's
// (OV: the moby table 0x15ffd8) or the base ELF's (bare: the hero 0x13f350 and his moby 0x1413d0, the frame time
// 0x15ed6c). Header shared_class341.h.
//
// The pad's pvars: the moby it follows (an index) +0, the glow's phase +0x18 and strength +0x1c.
#include <game/ovl/shared_class341.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x2fc the moby he stands on)

// The level's glow colours (its own data, five words): unlit +0, pressed +4 / +8, not pressed +0xc / +0x10.
uint32_t glow_colours() {
    if (on_level(RILGAR)) return L5_ADDR(0x001618c0);
    if (on_level(UMBRIS)) return L7_ADDR(0x00161870);
    if (on_level(POKITARU)) return L11_ADDR(0x00161944);
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x00161910);
    return L12_ADDR(0x001619b8);
}
}  // namespace

// With the hero on the pad its glow grows to full in half a second, and it is pressed while the hero's moby
// (0x1403e0) is in state 0x16; without him it fades out in a second. Lit, its colour pulses (a full cycle a second)
// between the two pressed or the two not pressed colours, blended from the unlit one by the glow's strength.
// orig: L5:0x002f81b8 L7:0x002f5640 L11:0x002f31e8 L12:0x002ec5b0 L18:0x002ce938
void class341_glow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float dt = lwc1(0x0015ed6c);
    if (lwu(HERO + 0x2fc) == m) {
        approach_f(pv + 0x1c, 1.0f, fpu_add(dt, dt));
        sb(m + 0xbc, lwu(0x001403e0) != 0 && lw(0x00140408) == 0x16 ? 1u : 0u);
    } else {
        approach_f(pv + 0x1c, 0.0f, dt);
    }
    const uint32_t colours = glow_colours();
    if (lwc1(pv + 0x1c) == 0.0f) {
        sw(m + 0x90, lwu(colours));
        return;
    }
    const float phase = angle_add(lwc1(pv + 0x18), fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu)));
    swc1(pv + 0x18, phase);
    const float t = fpu_mul(fpu_add(sin_vu(phase), 1.0f), 0.5f);
    const uint32_t lit = lbu(m + 0xbc) == 1 ? rgba_lerp(lwu(colours + 4), lwu(colours + 8), t)
                                             : rgba_lerp(lwu(colours + 0xc), lwu(colours + 0x10), t);
    sw(m + 0x90, rgba_lerp(lwu(colours), lit, lwc1(pv + 0x1c)));
}

// The pad's animation (+0x53 the set playing): from rest (0) it presses (1) when the moby it follows is pressed (its
// +0xbc bit 1) and the hero does not stand on it in state 0x13; once pressed (1, its +0x70 bit 2) it holds (2); from
// holding it releases (3) when the moby's bit 2 is set, the same way; released, back to rest. Then its glow.
// orig: L5:0x002f8080 L7:0x002f5508 L11:0x002f30b0 L12:0x002ec478 L18:0x002ce800
void class341_update(uint32_t m) {
    const bool stood_on = lwu(HERO + 0x2fc) == m && lw(0x001413dc) == 0x13;
    const uint32_t pv = lwu(m + 0x78);
    // the followed moby's +0xbc has `bit`, and the hero does not hold the pad down
    auto followed = [m, pv, stood_on](uint32_t bit) {
        const int32_t idx = lw(pv);
        if (idx < 0) return false;
        const uint32_t o = lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8);
        return (lbu(o + 0xbc) & bit) != 0 && !stood_on;
    };
    switch (lbu(m + 0x53)) {
    case 0:
        if (followed(1)) moby_anim_set(m, 1, 0, 0);
        break;
    case 1:
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 2, 0, 0);
        break;
    case 2:
        if (followed(2)) moby_anim_set(m, 3, 0, 0);
        break;
    case 3:
        if ((lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 0, 0, 0);
        break;
    default:
        break;
    }
    class341_glow(m);
}

}  // namespace rac1
