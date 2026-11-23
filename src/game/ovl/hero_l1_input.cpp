// Novalis's (level 1) variants of the hero's input / hurt functions. Addresses are
// Novalis's; the code is Veldin's minus the features Novalis's build does not have.
#include <game/collision/collision.h>
#include <game/ovl/hero_l1.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr uint32_t P = g::PLAYER;
float fs6c() { return lwc1(0x0015ed6c); }
}  // namespace

// A hit on the hero (Veldin's 0x210ce8) in Novalis's build: the hero moby's contact record (ring 0x178100,
// flag 1 only) when not already hurting (+0x1c0), not invulnerable (0x15f5c4) and not in the sub-states /
// state that ignore hits. Counts the hit, keeps the attacker in +0x2280. With `knock`, the hurt state
// (mode 0: the 0x80 / 0x82 grabs, under water 0x76 / 0x75, on a ledge 0x6d; mode 3: 0x56) and a push from the
// record's direction, the attacker or the facing added to the velocities (other modes: the push alone).
// There is no mode 1 / mode 2 handling, no ship shield and no sub-state 0xf special case.
// Aridia's build (L2:0x0021ff70, the same code) also has no under-water / 0x28f sub-state block
// (sub-states 0x12 / 0x11 / 3: the grab 0x82, the water hurt states 0x75 / 0x76).
// orig: L1:0x00231580 L2:0x0021ff70
int32_t hero_hurt_l1(int32_t knock) {
    if (lw(P + 0x208c) == 0x14 || lw(P + 0x208c) == 7) return 0;
    if (lw(P + 0x2084) == 0x32) return 0;
    if (lw(P + 0x1c0) != 0) return 0;
    if (lw(OV(0x0015f5c4)) != 0) return 0;
    sw(P + 0x2280, 0);
    const uint32_t hm = lwu(P + 0x2080);
    const uint32_t idx = lbu(hm + 0xa4);
    if (idx == 0xff) return 0;
    const uint32_t rec = coll_contact(idx);
    if (lwu(rec + 0x34) != hm) return 0;
    if ((lw(rec + 0x24) & 1) == 0) return 0;
    const uint32_t lvl = lwu(0x0015ed84);
    sw(0x0013df88 + lvl * 4, lwu(0x0013df88 + lvl * 4) + 1);
    sw(0x0015eea8, lwu(0x0015eea8) + 1);
    sw(P + 0x2280, lwu(rec + 0x20));
    if (knock == 0) return 1;
    sub_00206c08();
    int32_t scaled = 0;
    Vec4 push;  // (lanes the original leaves unset are stack garbage there)
    push.w = 0.0f;
    if ((lw(rec + 0x30) & 1) != 0) {
        push = V(rec + 0x10);
        if (lwc1(rec + 0x1c) == u2f(0x45afdf66u)) scaled = 1;
    } else if (lwu(rec + 0x20) != 0) {
        vec3_sub(push, V(P + 0x80), V(lwu(rec + 0x20) + 0x10));
    } else {
        push.x = cos_vu(angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        push.y = sin_vu(angle_add(lwc1(P + 0x98), u2f(0x40490fdbu)));
        push.z = 0.0f;
    }
    const uint32_t mode = lbu(P + 0x20a4);
    if (mode == 0) {
        const uint32_t att = lwu(P + 0x2280);
        const int32_t cls = att != 0 ? lh(att + 0xa6) : -1;
        if (att != 0 && (cls == 0x4eb || cls == 0x558)) {
            hero_set_state(0x80, 1);
            return 1;
        }
        const uint32_t sub = lwu(P + 0x208c);
        uint32_t st = 0x16;
        if (sub == 0x16) {  // hanging: let go
            hero_set_state(0x6d, 1);
            swc1(P + 0x128, fs6c() * 7.0f);
            return 1;
        }
        if (!on_aridia() && (sub == 0x12 || sub == 0x11 || sub == 3)) {
            if (att != 0 && cls == 0x28f) {
                hero_set_state(0x82, 1);
                return 1;
            }
            if (sub == 0x12) st = 0x75;
            if (sub == 0x11) {
                st = 0x76;
                const uint32_t l = lwu(0x0015ed84);
                if ((l == 0xf || l == 0x11) && att != 0 && (cls == 0x28f || cls == 0x7b || cls == 0x29d)) hero_sound_start(0x1c, 0);
            }
        }
        hero_set_state(st, 1);
        float s = fs6c() * u2f(0x40b66666u), z = fs6c() * u2f(0x4019999au);
        if (lbu(P + 0x12e7) != 0) {
            s = 0.0f;
            z = fs6c() * u2f(0x3fd9999au);
        }
        sub_00210c80_v(push, scaled, s, z);
        if (scaled != 0 && lbu(rec + 0x28) == 4) push.z = push.z + push.z;
    } else if (mode == 3) {
        hero_set_state(0x56, 1);
        sub_00210c80_v(push, scaled, fs6c() * 5.0f, fs6c() * u2f(0x4019999au));
    }
    vec3_add(V(P + 0xe0), V(P + 0xe0), push);
    vec3_add(V(P + 0x100), V(P + 0x100), push);
    return 1;
}

}  // namespace rac1
