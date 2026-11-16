// Umbris's own moby classes: two things the Snagglebeast (class 1106) throws — class 1046, the ring of its stomp (it
// grows, fades in and out, and pushes back the hero and the level's pushable mobys where its edge passes: 0x30e0e0), and
// class 1049 (umbris.shock_wave: a ball that falls, spins, sparks every third frame and explodes on what it hits) —
// hand-ported from Umbris's own overlay. Header umbris_mobys.h, registered in umbris_mobys.cpp. Addresses are Umbris's
// (`orig: L7:`); the base ELF's are bare (the frame time 0x15ed6c, its square 0x15ed70, the hero's moby 0x1413d0), the
// hit record the collision facades'.
#include <game/collision/collision.h>
#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_level_helpers.h>
#include <game/ovl/umbris_mobys.h>
#include <game/ovl/variants_l7.h>
#include <game/ovl/weapons.h>

namespace rac1 {

// Moby `t` pushed by the ring `m` (pvars `pv`: its radius +0x8, its height +0x10, its push +0x1c): when below the ring's
// top (its z plus 0.3 of the height) and within 0.3 of its edge, pushed outward from a point just inside the edge.
// orig: L7:0x0030e0e0
void umb_30e0e0_l7(uint32_t t, uint32_t m, uint32_t pv) {
    if (!(lwc1(t + 0x18) < fpu_add(lwc1(m + 0x18), fpu_mul(lwc1(pv + 0x10), u2f(0x3e99999au))))) return;
    const float d = vec2_distance(V(m + 0x10), V(t + 0x10));
    if (!(abs_f(fpu_sub(d, lwc1(pv + 0x8))) < u2f(0x3e99999au))) return;
    // (the push's point and direction: the original's stack, passed by address)
    const uint32_t sp = ee_stack_alloc(0x20);
    Vec4 dir;
    vec3_sub(dir, V(t + 0x10), V(m + 0x10));
    Vec4 at;
    vec3_set_length(at, dir, fpu_sub(lwc1(pv + 0x8), u2f(0x3d4ccccdu)));
    vec3_set_length(dir, dir, 1.0f);
    dir.z = 1.0f;
    V(sp + 0x0) = dir;
    V(sp + 0x10) = at;
    sub_00259bc8(t, m, 0x10001, sp + 0x10, sp + 0x0, lwc1(pv + 0x1c));
    ee_stack_free(0x20);
}

// Class 1046, the ring: its pvars its first and last radius +0x0 / +0x4, the radius +0x8 (its scale) growing by +0xc a
// frame; its alpha +0x23 up over the first fifth, down over the last; retired at its last radius. Drawn (L7:0x30dcf0);
// the hero's moby and the list 0x15ffe4's (+0x28 the next) pushed.
// orig: L7:0x0030e1f8
void umb_class1046_update_l7(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float r = fpu_add(lwc1(pv + 0x8), lwc1(pv + 0xc));
    swc1(pv + 0x8, r);
    swc1(m + 0x2c, r);
    const float lo = lwc1(pv + 0x0);
    const float t = fpu_div(fpu_sub(lwc1(pv + 0x8), lo), fpu_sub(lwc1(pv + 0x4), lo));
    if (t < u2f(0x3e4ccccdu))
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(t, 5.0f), 127.0f))));
    else if (u2f(0x3f4ccccdu) < t)
        sb(m + 0x23, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_mul(fpu_sub(1.0f, t), 5.0f), 127.0f))));
    else
        sb(m + 0x23, 0x80);
    if (1.0f <= t) {
        moby_retire(m);
        return;
    }
    sub_001f93e0(L7_ADDR(0x0030dcf0), m);
    umb_30e0e0_l7(lwu(0x001413d0), m, pv);
    for (uint32_t o = lwu(L7_ADDR(0x0015ffe4)); o != 0; o = lwu(o + 0x28)) umb_30e0e0_l7(o, m, pv);
}

// Class 1049, the ball: its pvars the velocity +0x0 (gravity 9.8 on z), the moby its collisions ignore +0x10. It spins
// (a turn a second on +0x48), grows to 2.8 times its class's size by 1.2 a frame, sparks every third frame of the level;
// hitting something (not the beast's 0x415 / 0x419 / 0x452): the explosion when shown (else a sound), retired; out of the
// level's box: retired.
// orig: L7:0x0030e458
void umb_class1049_update_l7(uint32_t m) {
    const uint32_t pos = m + 0x10;
    const uint32_t pv = lwu(m + 0x78);
    const Vec4 old = V(pos);
    vec3_add(V(pos), V(pos), V(pv));
    const float g = fpu_mul(lwc1(0x0015ed70), u2f(0x411ccccdu));
    const float spin = fpu_mul(lwc1(0x0015ed6c), u2f(0x40c90fdbu));
    swc1(pv + 0x8, fpu_sub(lwc1(pv + 0x8), g));
    swc1(m + 0x48, fpu_add(lwc1(m + 0x48), spin));
    const float size = lwc1(m + 0x2c);
    if (size < fpu_mul(lwc1(lwu(m + 0x24) + 0x24), u2f(0x40333333u))) swc1(m + 0x2c, fpu_mul(size, u2f(0x3f99999au)));
    if (ee_rem(lw(L7_ADDR(0x0015f5cc)), 3) == 0) chain_spark_spawn(m, 0, 400000.0f, 5.0f);
    bool hit = segment_collide_v(old, V(pos), 0, lwu(pv + 0x10), nullptr) != 0;
    if (!hit) hit = sphere_collide(pos, u2f(0x3dcccccdu), 0, lwu(pv + 0x10)) != 0;
    if (hit) {
        const uint32_t hm = coll_hit_moby();
        const int32_t c = hm != 0 ? lh(hm + 0xa6) : 0;
        if (hm == 0 || (c != 0x415 && c != 0x419 && c != 0x452)) {
            if (lbu(m + 0x31) != 0)
                explosion_effect(m, pv, 0, 0, 3, 5, 0, 0, 1, -1, 0, 2.0f, 1.0f, 0.0f, 0.0f, 1000.0f, u2f(0x3f99999au), 10.0f);
            else
                moby_sound_start(0, 0, m);
            moby_retire(m);
            return;
        }
    }
    for (uint32_t k = 0; k < 3; ++k) {
        const float c = lwc1(pos + k * 4);
        if (c < 2.0f || 1021.0f < c) {
            moby_retire(m);
            return;
        }
    }
}

}  // namespace rac1
