// Class 832, the Visibomb's range: one port of the copies sixteen levels carry (all but Veldin, Kalebo III and Drek's fleet),
// the same code relinked (the normalised texts compared). Its addresses are the engine's (OV: the moby list 0x15ffe4, the
// zones' polygons 0x1b04b0, the draw flag 0x15f30c) or the base ELF's (bare: the hero's state 0x1413d4, the screen's size
// 0x13e500); the levels' own word is the address of the draw callback the update queues (per level below). The bomb's end
// is Veldin's visibomb_ball_2b5b20 (Novalis', Aridia's, Kerwan's, Eudora's and Rilgar's copies are its code, which the
// overlay maps pair with it; their units still transliterate it). Header shared_class832.h.
//
// The pvars: twenty zone records +0.. +0x13f (the polygon's path index +0 (-1 none), its floor +4 and ceiling +8, a box +0xc),
// the frames out of them +0x140.
#include <game/ovl/shared_class832.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l13.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l18.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/variants_l9.h>
#include <game/ovl/visibomb_ball.h>

namespace rac1 {

namespace {
constexpr uint32_t BOMB_CLASS = 0xac;  // class 172, the Visibomb's missile
constexpr uint32_t RECORDS = 0x140;    // twenty zone records of 0x10
}  // namespace

// orig: (host) the level's copy of the draw callback (the address the update queues and render_passes.cpp recognises)
uint32_t class832_draw_addr() {
    switch (ovl_reloc_level()) {
    case NOVALIS: return L1_ADDR(0x00302438);
    case ARIDIA: return L2_ADDR(0x002e4838);
    case KERWAN: return L3_ADDR(0x002d3ed8);
    case EUDORA: return L4_ADDR(0x002dbf58);
    case RILGAR: return L5_ADDR(0x0030d8b8);
    case BLARG: return L6_ADDR(0x002ea830);
    case UMBRIS: return L7_ADDR(0x00304c40);
    case BATALIA: return L8_ADDR(0x002fb948);
    case GASPAR: return L9_ADDR(0x002fb078);
    case ORXON: return L10_ADDR(0x002ce868);
    case POKITARU: return L11_ADDR(0x00302e60);
    case HOVEN: return L12_ADDR(0x002fa028);
    case GEMLIK: return L13_ADDR(0x002fcc08);
    case OLTANIS: return L14_ADDR(0x002f6128);
    case QUARTU: return L15_ADDR(0x002de080);
    case VELDIN_FINALE: return L18_ADDR(0x002e3f58);
    default: return 0;
    }
}

// The screen (0x13e500 x 0x13e504 pixels, from the corner) in 32-pixel cells, each a 32 x 32 sprite of effect texture 0x1e
// at a random texel offset, its alpha the count of frames out (+0x140) over 1.5 seconds' worth, of 127.5.
// orig: L1:0x00302438 L2:0x002e4838 L3:0x002d3ed8 L4:0x002dbf58 L5:0x0030d8b8 L6:0x002ea830 L7:0x00304c40 L8:0x002fb948 L9:0x002fb078 L10:0x002ce868 L11:0x00302e60 L12:0x002fa028 L13:0x002fcc08 L14:0x002f6128 L15:0x002de080 L18:0x002e3f58
void class832_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint64_t tex0 = effect_tex0(0x1e);
    const int32_t cols = lw(0x0013e500) / 32 + 1;
    const int32_t rows = lw(0x0013e504) / 32 + 1;
    const float half = int_to_float(0x20 >> 1);
    for (int32_t c = 0; c < cols; ++c) {
        const float x = int_to_float(c << 5);
        for (int32_t r = 0; r < rows; ++r) {
            const float y = int_to_float(r << 5);
            const float count = int_to_float(lw(pv + 0x140));
            const float most = int_to_float(frame_scale_i(0x5a));
            const int32_t alpha = float_to_int(fpu_div(fpu_mul(count, u2f(0x42ff0000u)), most));
            const uint32_t colour = (static_cast<uint32_t>(alpha) << 24) + 0xff7f7fu;
            const int32_t u = rand_mod(0x20);
            const int32_t v = rand_mod(0x20);
            glyph_sprite(float_to_int(fpu_sub(x, half)), float_to_int(fpu_sub(y, half)), 0x20, 0x20, u, v, 0x20, 0x20,
                         static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(colour))), tex0);
        }
    }
}

// Shown in front (+0x30). While the hero is aiming the Visibomb (state 0x1d) and its missile (the first class-172 moby of the
// moby list) flies, it counts the frames the missile spends out of every zone record (a zone holds it when inside the
// polygon between the floor and ceiling, or inside the box); in one the count is 0. While counting it queues the static
// (when the draw flag 0x15f30c allows) and past 1.5 seconds ends the missile.
// orig: L1:0x00302648 L2:0x002e4a48 L3:0x002d40e8 L4:0x002dc168 L5:0x0030dac8 L6:0x002eaa40 L7:0x00304e50 L8:0x002fbb58 L9:0x002fb288 L10:0x002cea78 L11:0x00303070 L12:0x002fa238 L13:0x002fce18 L14:0x002f6338 L15:0x002de290 L18:0x002e4168
void class832_update(uint32_t m) {
    sb(m + 0x30, 0xff);
    const uint32_t pv = lwu(m + 0x78);
    uint32_t bomb = 0;
    if (lw(0x001413d4) == 0x1d) {
        for (uint32_t o = lwu(OV(0x0015ffe4)); o != 0; o = lwu(o + 0x28))
            if (lh(o + 0xa6) == static_cast<int32_t>(BOMB_CLASS)) {
                bomb = o;
                break;
            }
    }
    if (bomb == 0) {
        sw(pv + 0x140, 0);
        return;
    }
    const uint32_t polys = OV(0x001b04b0);
    sw(pv + 0x140, lwu(pv + 0x140) + 1u);
    for (uint32_t rec = pv; rec < pv + RECORDS; rec += 0x10) {
        const int32_t poly_idx = lw(rec);
        if (poly_idx != -1 && lwc1(rec + 4) < lwc1(bomb + 0x18) && lwc1(bomb + 0x18) < lwc1(rec + 8)) {
            const uint32_t poly = lwu(polys + (static_cast<uint32_t>(poly_idx) << 2));
            if (point_in_polygon(bomb + 0x10, poly + 0x10, lw(poly)) != 0) {
                sw(pv + 0x140, 0);
                break;
            }
        }
        if (point_in_box(bomb + 0x10, lw(rec + 0xc)) != 0) {
            sw(pv + 0x140, 0);
            break;
        }
    }
    if (lw(pv + 0x140) <= 0) return;
    if (lwu(OV(0x0015f30c)) != 0) draw_callback_add(class832_draw_addr(), m);
    if (frame_scale_i(0x5a) < lw(pv + 0x140)) visibomb_ball_2b5b20(bomb, lwu(bomb + 0x78));
}

}  // namespace rac1
