// Breakable props of Blarg (level 6: classes 1655, 1660, 1664) and Umbris (7: classes 1650, 1663). Each class's update is
// the same code bound to its own classes: three debris classes (1655, 1650, 1663) or one debris class spawned twice (1660,
// 1664), and the broken piece left in its place. Nothing of it is the level's own: the debris' zero vectors are the
// engine's (OV 0x15f580), the bolts the shared plant_bolts_drop_l1 (the levels' copies L6:0x272a68 / L7:0x28c398).
// Header shared_breakables.h.
//
// States: 0 start, 1 standing (a hit with damage breaks it), 2 breaking.
#include <game/ovl/shared_breakables.h>

#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t HIT_MASK = 0x10000;

void breakable_update(uint32_t m, const uint32_t* debris, int32_t pieces, uint32_t broken) {
    const uint32_t hit = sub_0025a420(m, HIT_MASK, 0);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x20, 1);
        return;
    case 1:
        if (hit != 0 && 0.0f < lwc1(hit + 0x2c)) sb(m + 0x20, 2);
        return;
    case 2: {
        moby_sound_start(0, 0, m);
        const uint32_t zero = OV(0x0015f580);
        plant_bolts_drop_l1(m);
        for (int32_t i = 0; i < pieces; ++i) debris_spawn(m, debris[i], m + 0x10, m + 0x40, 0, 0, zero, zero, zero, 0.0f);
        const uint32_t o = moby_spawn(broken);
        if (o != 0) {
            sb(o + 0x31, 1);
            sh(o + 0x32, 0xff);
            lq_sq(o + 0x10, m + 0x10);
            lq_sq(o + 0x40, m + 0x40);
            sd(o + 0x38, ld(m + 0x38));
            moby_update_transform(o);
        }
        moby_retire(m);
        return;
    }
    default:
        return;
    }
}
}  // namespace

// orig: L6:0x00309860
void class1655_update(uint32_t m) {
    static constexpr uint32_t debris[] = {0x679, 0x67a, 0x67b};
    breakable_update(m, debris, 3, 0x678);
}

// orig: L7:0x0031f3d8
void class1650_update(uint32_t m) {
    static constexpr uint32_t debris[] = {0x674, 0x675, 0x676};
    breakable_update(m, debris, 3, 0x673);
}

// orig: L7:0x0031f5b0
void class1663_update(uint32_t m) {
    static constexpr uint32_t debris[] = {0x68e, 0x68f, 0x690};
    breakable_update(m, debris, 3, 0x68d);
}

// orig: L6:0x00309e08
void class1660_update(uint32_t m) {
    static constexpr uint32_t debris[] = {0x67e, 0x67e};
    breakable_update(m, debris, 2, 0x67d);
}

// orig: L6:0x00309fa8
void class1664_update(uint32_t m) {
    static constexpr uint32_t debris[] = {0x682, 0x682};
    breakable_update(m, debris, 2, 0x681);
}

}  // namespace rac1
