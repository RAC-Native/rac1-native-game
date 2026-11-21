// Gemlik Base's (level 13) class 127: a part that swings open and closed (its angles turned by a set amount) when a
// switch moby reaches a state. Level 13 only: the addresses are Gemlik's (`orig: L13:`), the level's own data
// L13_ADDR, the base ELF's bare. Registered in gemlik_mobys.cpp.
//
// Its pvars: +0x00 its closed angles, +0x0c how far open (0 .. 1), +0x10 its switch (a moby index, -1 none: it then
// opens and closes by itself), +0x14 / +0x18 the switch's states that close / open it, +0x1c / +0x20 / +0x24 the
// angles it opens by (degrees), +0x2c the seconds a swing takes, +0x30 / +0x34 its sounds (-1 none), +0x38 its scale
// (relative to its class's; 0 keeps it), +0x3c open at the start. Its +0xb0 (0xff none) is a switch byte of the level's
// row (0x14c050, the row of 0x15ed84): 0xff there forgets the switch. States (the table L13:0x1f4800): 0 set up, 1
// closed, 2 opening, 3 open, 4 closing, 5 nothing.
#include <game/math/vecmath.h>
#include <game/ovl/gemlik_mobys.h>
#include <game/ovl/gemlik_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l13.h>

namespace rac1 {

using namespace gml;

// orig: L13:0x002c7f38
void gml_class127_update_l13(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t flag = lbu(m + 0xb0);
    if (flag != 0xff && lbu(0x0014c050u + flag + (lwu(0x0015ed84) << 4)) == 0xff) sw(pv + 0x10, 0xffffffffu);
    // the switch's state; -1 when there is none (no index, no moby, gone or retiring)
    auto switch_state = [pv]() -> int32_t {
        const int32_t i = lw(pv + 0x10);
        if (i == -1) return -1;
        const uint32_t s = lwu(L13_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8);
        if (s == 0 || !alive(s)) return -1;
        return static_cast<int32_t>(lbu(s + 0x20));
    };
    auto sound = [m, pv](uint32_t which) {
        if (lw(pv + 0x30) != -1) moby_sound_start(lw(pv + which), 0, m);
    };
    // a swing step of 1 / (60 s) of the way, its angles turned by as much (`dir` +1 opening, -1 closing)
    auto swing = [m, pv](bool opening) {
        const float k = fpu_div(1.0f, fpu_mul(lwc1(pv + 0x2c), 60.0f));
        swc1(pv + 0xc, opening ? fpu_add(lwc1(pv + 0xc), k) : fpu_sub(lwc1(pv + 0xc), k));
        for (uint32_t a = 0; a < 0xc; a += 4) {
            const float by = fpu_mul(fpu_mul(lwc1(pv + 0x1c + a), u2f(0x3c8efa35u)), k);
            swc1(m + 0x40 + a, opening ? angle_add(lwc1(m + 0x40 + a), by) : angle_sub(lwc1(m + 0x40 + a), by));
        }
    };
    switch (lbu(m + 0x20)) {  // (the table L13:0x1f4800)
    case 0:  // (0x002c7fc4)
        lq_sq(pv + 0x00, m + 0x40);
        sb(m + 0x20, lwu(pv + 0x3c) != 0 ? 2 : 1);
        sw(pv + 0xc, 0);
        if (lwc1(pv + 0xc) < lwc1(pv + 0x38)) swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(pv + 0x38)));
        return;
    case 1: {  // (0x002c8014): closed; opens when the switch is in its +0x14 state (at once without a switch)
        const int32_t s = switch_state();
        if (s != -1 && s != lw(pv + 0x14)) return;
        if (lw(pv + 0x30) != -1) moby_sound_start(lw(pv + 0x30), 0, m);
        sw(pv + 0xc, 0);
        sb(m + 0x20, 2);
        return;
    }
    case 2: {  // (0x002c8078): opening; closes when the switch is in its +0x18 state
        const int32_t s = switch_state();
        if (s != -1 && s == lw(pv + 0x18)) {
            if (lw(pv + 0x30) != -1) moby_sound_start(lw(pv + 0x30), 0, m);
            sb(m + 0x20, 4);
            return;
        }
        swing(true);
        if (!(1.0f <= lwc1(pv + 0xc))) return;
        for (uint32_t a = 0; a < 0xc; a += 4) swc1(m + 0x40 + a, angle_add(lwc1(pv + a), fpu_mul(lwc1(pv + 0x1c + a), u2f(0x3c8efa35u))));
        swc1(pv + 0xc, 1.0f);
        sb(m + 0x20, 3);
        if (lw(pv + 0x34) != -1) moby_sound_start(lw(pv + 0x30), 0, m);  // (the original tests +0x34 and plays +0x30)
        return;
    }
    case 3: {  // (0x002c81d4): open; closes when the switch is in its +0x18 state
        const int32_t s = switch_state();
        if (s == -1 || s != lw(pv + 0x18)) return;
        swc1(pv + 0xc, 1.0f);
        if (lw(pv + 0x30) != -1) moby_sound_start(lw(pv + 0x30), 0, m);
        sb(m + 0x20, 4);
        return;
    }
    case 4: {  // (0x002c823c): closing; opens again when the switch is in its +0x14 state (at once without a switch)
        const int32_t s = switch_state();
        if (s == -1 || s == lw(pv + 0x14)) {
            if (lw(pv + 0x30) != -1) moby_sound_start(lw(pv + 0x30), 0, m);
            sb(m + 0x20, 2);
            return;
        }
        swing(false);
        if (!(lwc1(pv + 0xc) <= 0.0f)) return;
        for (uint32_t a = 0; a < 0xc; a += 4) swc1(m + 0x40 + a, lwc1(pv + a));
        swc1(pv + 0xc, 0.0f);
        sound(0x34);
        sb(m + 0x20, 1);
        return;
    }
    default:  // (5 and beyond: nothing)
        return;
    }
}

}  // namespace rac1
