// Class 408's tint: one port of the copies Quartu and Drek's fleet carry (the same code relinked). Each level keeps its own
// words (per level below); the frame counter is the engine's (OV 0x15f5cc). Header shared_class408_tint.h.
#include <game/ovl/shared_class408_tint.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the level's words: +0 the strength's middle, +0x10 the colour (its high byte, signed, also the scale), +0x14 the
// strength's swing, +0x18 the period (frames)
uint32_t words() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161990) : L15_ADDR(0x00161abc); }
}  // namespace

// The strength: the middle plus the swing times the sine of the frame counter's place in the period (scaled), at least 0;
// the colour register written with the colour word's low byte, its "past 8" and "past 16" flags, and its high byte times
// the strength.
// orig: L15:0x002cbd88 L17:0x002cc740
void class408_tint(uint32_t) {
    const uint32_t w = words();
    const int32_t period = frame_scale_i(lw(w + 0x18));
    const float frames = int_to_float(period);
    const int32_t at = ee_rem(lw(OV(0x0015f5cc)), period);
    const float wave = sin_vu(fpu_mul(fpu_div(static_cast<float>(at), frames), u2f(0x40c90fd0u)));  // (2 pi)
    float strength = fpu_add(lwc1(w), fpu_mul(lwc1(w + 0x14), wave));
    if (strength < 0.0f) strength = 0.0f;
    const int32_t alpha = float_to_int(fpu_mul(static_cast<float>(static_cast<int8_t>(lbu(w + 0x13))), strength));
    const int32_t colour = lw(w + 0x10);
    sub_001fa050(colour & 0xff, colour < 9 ? 0 : 1, colour < 17 ? 0 : 1, alpha);
}

}  // namespace rac1
