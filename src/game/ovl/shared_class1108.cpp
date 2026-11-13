// Class 1108's draw callback: one port of the copies Blarg (L6), Quartu (L15), Drek's fleet (L17) and the finale's Veldin
// (L18) carry. The copies are the same code relinked but for where the time left is kept, a parameter here (Blarg's pvar word
// +0xb4, Quartu's and the fleet's half +0x12, the finale's word +0x6c). The frame counter 0x15f5cc is the engine's (OV); the
// level's own data is one block of the box's and the text's words (per level below). Header shared_class1108.h.
#include <game/ovl/shared_class1108.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// The level's words (its own data, one block): the text's place x +0 / y +4, its two colours +8 / +0xc, the box's edges
// +0x10 / +0x14 (from y) and +0x18 / +0x1c (from x), the box's colour +0x20.
uint32_t words(uint32_t o) {
    if (on_level(QUARTU)) return L15_ADDR(0x00161bd4) + o;
    if (on_level(VELDIN_ORBIT)) return L17_ADDR(0x00161a64) + o;
    if (on_level(VELDIN_FINALE)) return L18_ADDR(0x00161a1c) + o;
    return L6_ADDR(0x001620c8) + o;
}
// the time left (frames) in the moby's pvars, where each level's copy keeps it
int32_t time_left(uint32_t pv) {
    if (on_level(QUARTU) || on_level(VELDIN_ORBIT)) return lh(pv + 0x12);
    if (on_level(VELDIN_FINALE)) return lw(pv + 0x6c);
    return lw(pv + 0xb4);
}
}  // namespace

// The box; the text's colour pulsing (once a second) between the two; the time as "0M:SS:TX": its minutes (3600 frames' worth),
// the tens and units of seconds (600 / 60), the tenths (6), and a random digit.
// orig: L6:0x003021d8 L15:0x002d77c0 L17:0x002d5e38 L18:0x002d8098
void class1108_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x60);
    const uint32_t text = sp;
    const uint32_t pv = lwu(m + 0x78);
    const int32_t x = lw(words(0)), y = lw(words(4));
    box_rounded(lw(words(0x10)) + y, lw(words(0x14)) + y, lw(words(0x18)) + x, lw(words(0x1c)) + x, lwu(words(0x20)));
    const float a = fpu_mul(fpu_div(int_to_float(ee_rem(lw(OV(0x0015f5cc)), 0x3c)), 60.0f), u2f(0x40c90fd0u));
    float t = fpu_add(fpu_mul(sin_vu(a), 3.0f), 0.5f);
    if (1.0f < t) t = 1.0f;
    else if (t < 0.0f) t = 0.0f;
    const uint32_t colour = rgba_lerp(lwu(words(8)), lwu(words(0xc)), t);
    // a digit: the time's part below `unit` (frames) in `step`s
    auto digit = [pv](int32_t unit, int32_t step) {
        return static_cast<uint32_t>(ee_div(ee_rem(time_left(pv), frame_scale_i(unit)), frame_scale_i(step))) + 0x30u;
    };
    sb(text, 0x30);
    sb(text + 2, 0x3a);
    sb(text + 1, static_cast<uint32_t>(ee_div(time_left(pv), frame_scale_i(0xe10))) + 0x30u);
    sb(text + 3, digit(0xe10, 0x258));
    sb(text + 5, 0x3a);
    sb(text + 4, digit(0x258, 0x3c));
    sb(text + 6, digit(0x3c, 6));
    sb(text + 7, static_cast<uint32_t>(float_to_int(int_to_float(rand_mod(10)))) + 0x30u);
    text_centered(lw(words(0)), lw(words(4)), static_cast<uint64_t>(colour), text, 8);
    ee_stack_free(0x60);
}

}  // namespace rac1
