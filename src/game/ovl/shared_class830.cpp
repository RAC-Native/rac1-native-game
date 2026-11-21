// Class 830, a floor switch: one port of the copies Rilgar, Pokitaru, Quartu, Drek's fleet and the finale's Veldin carry, the
// same code relinked (the normalised texts compared). Its addresses are the engine's (OV: the two flag tables 0x1ba5d0 /
// 0x1bb230 (+0x454 + the moby's flag index), the path table 0x1b04b0, the level's flag list 0x15fc88) or the base ELF's
// (bare: the hero 0x13f350, the level number 0x15ed84, the per-level flag words 0x14c190 and bytes 0x14c050, the frame time
// 0x15ed6c). Header shared_class830.h.
//
// The moby: its flag +0xb0 (0xff none), its flag index +0xb2 (s16). The pvars: a path +0 (-1 none) whose points' marks (+0xc)
// equal to the mark +4 it clears, a flag kind +8, the glow's phase +0xc.
#include <game/ovl/shared_class830.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;  // the hero (+0x2fc the moby he stands on, +0x30e)

// pressed: state 2, the lit colour, its byte +0xbc `asked`; the path's points marked as the switch's lose their mark
void press(uint32_t m, uint32_t pv, uint32_t asked) {
    sb(m + 0x20, 2);
    sw(m + 0x90, 0x80208020u);
    sb(m + 0xbc, asked);
    const int32_t idx = lw(pv);
    if (idx == -1) return;
    for (int32_t i = 0;; ++i) {
        const uint32_t path = lwu(OV(0x001b04b0) + (static_cast<uint32_t>(lw(pv)) << 2));
        if (!(i < lw(path))) break;
        const uint32_t pt = path + (static_cast<uint32_t>(i) << 4);
        if (lwc1(pt + 0x1c) == lwc1(pv + 4)) sw(pt + 0x1c, 0);
    }
}
}  // namespace

// State 0: its phase at random, a little lower (0.35); already pressed when its flag in the table 0x1bb230 or in the level's
// flag words (0x14c190: 32 a word, 0x100 bytes a level) is set, or (with a flag kind +8) when its flag byte of the level
// (0x14c050) is unset; else waiting (1). State 1: its glow pulses (a turn a second); pressed when the hero stands on it
// (+0x2fc) and +0x30e is clear: the flag tables marked (its flag + 2; the second table only when its flag is none or is the
// level list's or the level's byte is unset), sound 0.
// orig: L5:0x0030c5b0 L11:0x00302ae0 L15:0x002ddd00 L17:0x002dc078 L18:0x002e1508
void class830_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t level = lwu(0x0015ed84);
    switch (lbu(m + 0x20)) {
    case 0: {
        swc1(pv + 0xc, rand_angle());
        swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), u2f(0x3eb33333u)));
        const uint32_t raw = lhu(m + 0xb2);
        const int32_t idx = static_cast<int16_t>(raw);
        bool pressed = lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0;
        if (!pressed) {
            const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (level << 8));
            pressed = ((static_cast<int32_t>(word) >> (raw & 0x1f)) & 1) != 0;
        }
        if (!pressed && lwu(pv + 8) != 0) pressed = lbu(0x0014c050 + lbu(m + 0xb0) + (level << 4)) == 0xff;
        if (pressed) press(m, pv, 2);
        else sb(m + 0x20, 1);
        break;
    }
    case 1: {
        const float phase = angle_add(lwc1(pv + 0xc), fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu)));
        swc1(pv + 0xc, phase);
        int32_t c = float_to_int(fpu_mul(fpu_sub(fpu_mul(sin_vu(phase), 4.0f), 3.0f), 128.0f));
        if (c >= 129) c = 0x80;
        else if (!(0x1f < c)) c = 0x20;
        const uint32_t g = static_cast<uint32_t>(c);
        sw(m + 0x90, (g << 16) | (g << 8) | 0x80000000u | g);
        if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30e) != 0) break;
        const uint32_t at = static_cast<uint32_t>(static_cast<int32_t>(lh(m + 0xb2))) + 0x454;
        sb(OV(0x001ba5d0) + at, lbu(m + 0xb0) + 2u);
        const uint32_t flag = lbu(m + 0xb0);
        if (flag == 0xff || (lbu(OV(0x0015fc88) + flag) != 0xff && lbu(0x0014c050 + flag + (level << 4)) == 0xff))
            sb(OV(0x001bb230) + static_cast<uint32_t>(static_cast<int32_t>(lh(m + 0xb2))) + 0x454, flag + 2u);
        press(m, pv, 1);
        moby_sound_start(0, 0, m);
        break;
    }
    default:
        break;
    }
}

}  // namespace rac1
