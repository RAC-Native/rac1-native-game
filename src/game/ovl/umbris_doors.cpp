// Umbris's own moby classes: class 1013, the gates and platforms that slide down (classes 0x3f5 / 0x429 up: their move
// turned over) by a set distance when another moby reaches a state (a switch pressed, a guard beaten), and back again —
// hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's
// (`orig: L7:`); the base ELF's are bare (the speed 0x15ed60, the sound slots 0x13e550, the level's flags 0x14c050).
//
// Its pvars: its home +0x0 (+0x8 z), a sound slot +0xc, its sounds +0xe (moving) and +0xf (stopped), the moby that
// drives it +0x10 (-1: none; a level flag byte at +0xb0 not 0xff: none), a level switch +0x14 (0x184528: 1 while it is
// open), the driver's state that opens it +0x18 and closes it +0x16, the distance +0x1c.
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>

namespace rac1 {

namespace {
// the driver's state (-1: no driver, gone or dying: `gone` set)
int32_t driver_state(uint32_t pv, bool& gone) {
    gone = true;
    const int32_t i = lw(pv + 0x10);
    if (i == -1) return -1;
    const uint32_t d = lwu(L7_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8);
    if (d == 0) return -1;
    const uint32_t s = lbu(d + 0x20);
    if (s == 0xfe || s == 0xfd) return -1;
    gone = false;
    return static_cast<int32_t>(s);
}
void sound(int32_t idx, uint32_t m) { moby_sound_start(idx, 0, m); }
// the level switch +0x14 (-1: none) set to `v`
void level_switch(uint32_t pv, uint32_t v) {
    const int32_t i = lh(pv + 0x14);
    if (i != -1) sw(L7_ADDR(0x00184528) + static_cast<uint32_t>(i << 2), v);
}
// a step of `dz` on z (turned over for classes 0x3f5 / 0x429) through its orientation +0xc0
void step(uint32_t m, float dz) {
    Vec4 d{0.0f, 0.0f, 0.0f, 0.0f};
    d.y = dz;
    Vec4 v = d;
    const int32_t cls = lh(m + 0xa6);
    if (cls == 0x3f5 || cls == 0x429) v.y = -v.y;
    mat3_transform(d, v, M(m + 0xc0));
    vec3_add(V(m + 0x10), V(m + 0x10), d);
}
}  // namespace

// Class 1013 (+0x20, the table L7:0x2116a0: 0 set up, 1 closed, 2 opening, 3 open, 4 closing).
// orig: L7:0x0030cf90
void umb_class1013_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    Vec4 home{lwc1(pv + 0x0), lwc1(pv + 0x4), lwc1(pv + 0x8), 0.0f};
    if (lbu(m + 0xb0) != 0xff && lbu(0x0014c050 + lbu(m + 0xb0) + (lwu(0x0015ed84) << 4)) == 0xff) sw(pv + 0x10, 0xffffffffu);
    bool gone = false;
    switch (lbu(m + 0x20)) {
        case 0:  // L7:0x30d034
            swc1(pv + 0x0, lwc1(m + 0x10));
            swc1(pv + 0x4, lwc1(m + 0x14));
            sh(pv + 0xc, 0xffff);
            swc1(pv + 0x8, lwc1(m + 0x18));
            level_switch(pv, 0);
            sb(m + 0x20, 1);
            return;
        case 1: {  // L7:0x30d070
            const int32_t s = driver_state(pv, gone);
            if (!gone && s != lh(pv + 0x16)) return;
            const int32_t snd = static_cast<int8_t>(lbu(pv + 0xe));
            if (snd != -1) sound(snd, m);
            sb(m + 0x20, 2);
            return;
        }
        case 2: {  // L7:0x30d0d4
            const int32_t s = driver_state(pv, gone);
            if (!gone && s == lw(pv + 0x18)) {
                sound(static_cast<int8_t>(lbu(pv + 0xe)), m);
                sb(m + 0x20, 4);
                return;
            }
            if (lwc1(pv + 0x1c) < vec2_distance(V(m + 0x10), home)) {
                const int32_t snd = static_cast<int8_t>(lbu(pv + 0xf));
                if (snd != -1) sound(snd, m);
                level_switch(pv, 1);
                sb(m + 0x20, 3);
                return;
            }
            step(m, -fpu_mul(lwc1(0x0015ed60), u2f(0x3c23d70au)));
            return;
        }
        case 3: {  // L7:0x30d214
            const int32_t s = driver_state(pv, gone);
            if (gone || s != lw(pv + 0x18)) return;
            const int32_t snd = static_cast<int8_t>(lbu(pv + 0xe));
            if (snd != -1) sound(snd, m);
            sb(m + 0x20, 4);
            return;
        }
        case 4: {  // L7:0x30d274
            const int32_t s = driver_state(pv, gone);
            if (gone || s == lh(pv + 0x16)) {
                sound(static_cast<int8_t>(lbu(pv + 0xe)), m);
                sb(m + 0x20, 2);
                return;
            }
            const float near = fpu_mul(lwc1(0x0015ed60), u2f(0x3c23d70au));
            if (vec2_distance(V(m + 0x10), home) <= near) {
                // home: put there, its sound stopped, the stop sound, the switch off
                swc1(m + 0x10, lwc1(pv + 0x0));
                swc1(m + 0x14, lwc1(pv + 0x4));
                sw(m + 0x1c, 0);
                swc1(m + 0x18, lwc1(pv + 0x8));
                const int32_t slot = lh(pv + 0xc);
                if (slot != -1) {
                    const uint32_t e = 0x0013e550 + static_cast<uint32_t>(slot * 0x70);
                    if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
                }
                sh(pv + 0xc, 0xffff);
                const int32_t snd = static_cast<int8_t>(lbu(pv + 0xf));
                if (snd != -1) sound(snd, m);
                level_switch(pv, 0);
                sb(m + 0x20, 1);
                return;
            }
            step(m, near);
            return;
        }
        default:  // 5 (L7:0x30d404) and past the table
            return;
    }
}

}  // namespace rac1
