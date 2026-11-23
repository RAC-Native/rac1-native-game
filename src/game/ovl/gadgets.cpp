// Ported overlay code: the gadgets' own mobys. The Trespasser (item 0x1a, class 0xbc): its update
// 0x2c1718 is the hand moby's (shown while the pad's button mask 0x1403f0 is held) and, with a
// non-zero pvar +4, the pickup lying in the world (0x2c1778: it spins, and when the hero walks
// up to it, within 1 in the plane and 2 in height, gives the item).
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/weapons.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr uint32_t P = 0x0013f350;
}  // namespace

// The Trespasser pickup's frame: state 0 sets it on the floor (a probe 0.5 up, the hit's height
// 0x173e68 plus 0x161828), turned (+0x40 = pi / 2, +0x44 = pi) and state 1; state 1 spins it (+0x48
// by 0x161830) and, every tenth frame of the counter 0x15f5cc, when the hero's 0x22a8 flag is set
// and it is within 1 in the plane and 2 in height of the hero (P+0x80 / P+0x88), gives item 0x1a
// and goes to state 2; state 2 shrinks it (+0x2c to 0 by 0.02 of its class's +0x24) and retires it.
// orig: 0x002c1778
void trespasser_pickup_update(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0: {
        (void)floor_probe_v(0.5f, V(m + 0x10), 0);
        swc1(m + 0x40, u2f(0x3fc90fdbu));
        swc1(m + 0x44, u2f(0x40490fdbu));
        sb(m + 0x20, 1);
        swc1(m + 0x18, coll_hit_point().z + lwc1(OV(0x00166c00u - 0x53d8)));
        break;
    }
    case 1: {
        swc1(m + 0x48, angle_add(lwc1(m + 0x48), lwc1(OV(0x00166c00u - 0x53d0))));
        if (ee_rem(lw(OV(0x0015f5cc)), 10) != 0) break;
        if (lwu(P + 0x22a8) == 0) break;
        if (!(vec2_distance(V(m + 0x10), V(P + 0x80)) < 1.0f)) break;
        if (!(abs_f(lwc1(m + 0x18) - lwc1(P + 0x88)) < 2.0f)) break;
        item_give(0x1a, 1);
        sb(m + 0x20, 2);
        break;
    }
    case 2: {
        const uint32_t cls = lwu(m + 0x24);
        approach_f_v(*static_cast<float*>(ee_ram_ptr(m + 0x2c)), 0.0f, lwc1(cls + 0x24) * u2f(0x3ca3d70au));
        if (lwc1(m + 0x2c) == 0.0f) moby_retire(m);
        break;
    }
    default:
        break;
    }
}

// The Trespasser's moby update (hand moby and world pickup): with pvar +4 set the pickup's frame
// above, else the hand's visibility bit (+0xbc) follows the held buttons 0x13cae0 against 0x1403f0.
// orig: 0x002c1718
void trespasser_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lwu(pv + 4) != 0) {
        trespasser_pickup_update(m);
        return;
    }
    sb(m + 0xbc, (lwu(0x0013cae0) & lwu(0x001403f0)) != 0 ? 1u : 0u);
}

// orig: (none) — the gadgets' updates for the moby classes' indirect calls.
void gadgets_fns_register() {
    ovl_register_moby_fn(0x002c1718, trespasser_update);
}

}  // namespace rac1
