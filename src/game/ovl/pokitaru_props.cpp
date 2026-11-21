// Pokitaru's (level 11) small props, ported by hand from the level's own code: class 23 (a pickup that ends the resort
// owner's errand), class 65 (a gate that swings open, with its twin) and class 361 (a height trigger that sets the
// hero's checkpoint index). The original's order of calls and float operations kept.
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

namespace {
constexpr uint32_t kHeroPos = 0x0013f3d0;  // the hero's position (the hero's block 0x13f350 + 0x80)
}  // namespace

// Class 23: state 0 retires it once it was taken (the flag 0x13d4c6), else lifts it by 0.75, shows it (+0x34 bit 0)
// and goes to 1 (idle); state 2 (set by the level) waits for the hero within 2 (in the plane): it marks itself taken and
// the resort owner's errand done (0x13d4ee), asks for the level change and the goal's announcement, and goes.
// (State 2 also turns its facing by 3/2 pi a second and drops the result: the original never stores it.)
// orig: L11:0x002cb668
void pok_class23_update_l11(uint32_t m) {
    switch (lbu(m + 0x20)) {
    case 0:
        if (lbu(0x0013d4c6) != 0) {
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        sh(m + 0x34, lhu(m + 0x34) | 1u);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 0.75f));
        break;
    case 2:
        if (vec2_distance(V(m + 0x10), V(kHeroPos)) < 2.0f) {
            sb(0x0013d4c6, 1);
            sb(0x0013d4ee, 1);
            level_change_request(0, -1);
            sub_00203908(TXT(0x2af8), 0x3a);
            moby_retire(m);
        }
        break;
    default:
        break;
    }
}

// Class 65: a gate. State 0 keeps its closed facing (pvar +4) and, unless it is the twin itself (+0x34 bit 15), spawns
// its twin (class 65 at its place and angles, marked as the twin, its pvar +8 on the first); 1 waits closed (the
// resort owner's helper 0x2cb990 sets 2); 2 swings it open (12 degrees from the closed facing, the twin the other way)
// at the rates 20 degrees / s^2 and 45 degrees / s, then 3 (open).
// orig: L11:0x002cb810
void pok_class65_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        const float facing = lwc1(m + 0x48);
        sb(m + 0x20, 1);
        swc1(pv + 4, facing);
        if ((lhu(m + 0x34) & 0x8000u) != 0) return;
        const uint32_t twin = moby_spawn(0x41);
        sh(twin + 0x32, lhu(m + 0x32));
        sb(twin + 0x30, lbu(m + 0x30));
        sh(twin + 0x34, lhu(m + 0x34) | 0x8000u);
        lq_sq(twin + 0x10, m + 0x10);
        lq_sq(twin + 0x40, m + 0x40);
        sw(pv + 8, twin);
        swc1(lwu(twin + 0x78) + 4, lwc1(m + 0x48));
        break;
    }
    case 2: {
        const float swing = u2f(0x3e54adc8u);  // (11.9 degrees)
        const float target = (lhu(m + 0x34) & 0x8000u) != 0 ? angle_sub(lwc1(pv + 4), swing) : angle_add(lwc1(pv + 4), swing);
        const float accel = fpu_mul(lwc1(0x0015ed70), u2f(0x3eb2b8c2u));
        if (yaw_seek(m + 0x48, pv, target, accel, accel, fpu_mul(lwc1(0x0015ed6c), u2f(0x3f490fdbu))) == 0.0f) sb(m + 0x20, 3);
        break;
    }
    default:
        break;
    }
}

// Class 361: state 0 hides it from the moby pass (+0x30 0xff) and goes to 1, or retires it without an index
// (pvar +0 -1); state 1 sets the hero's checkpoint index (0x14162a) to its pvar's half +0 while the hero is above
// 194 and not in the state 0x32.
// orig: L11:0x002f3350
void pok_class361_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:
        sb(m + 0x30, 0xff);
        sb(m + 0x20, 1);
        if (lw(pv) == -1) moby_retire(m);
        break;
    case 1:
        if (u2f(0x43420000u) < lwc1(0x0013f3d8) && lw(0x001413d4) != 0x32) sh(0x0014162a, lhu(pv));
        break;
    default:
        break;
    }
}

}  // namespace rac1
