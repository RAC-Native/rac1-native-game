// Class 1633, the Sandmouse's shot, and the Sandmouse's glow: one port of the copies ten levels carry (Novalis, Aridia, Kerwan, Eudora, Rilgar, Blarg,
// Batalia, Pokitaru, Hoven, Oltanis). The copies are the same code relinked (compared with the relocated fields masked);
// what they name is the engine's (OV: the camera 0x166dc0, the collision's hit point 0x173e60) or the base ELF's (bare:
// the hero 0x13f3d0, the frame scales 0x15ed60 / 0x15ed6c / 0x15ed70). Header shared_sandmouse.h.
//
// The shot's pvars: its pitch +0 and yaw +4 turn rates (angle_seek_rate's), the aimed pitch +8 and yaw +0xc, the
// shooter +0x10, its life +0x14.
#include <game/ovl/shared_sandmouse.h>

#include <game/math/vecmath.h>
#include <game/ovl/batalia_part2.h>
#include <game/ovl/blarg_part2.h>
#include <game/ovl/glows.h>
#include <game/ovl/hoven_mobys.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/rilgar_part2.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/variants_l4.h>
#include <game/ovl/variants_l5.h>
#include <game/ovl/variants_l6.h>
#include <game/ovl/variants_l8.h>

namespace rac1 {

// The levels' own builds of glow_draw 0x263618 (variants: their transliterations; Aridia's, Kerwan's and Eudora's are
// declared by no header of theirs)
TrRet lv2_264b50_l2(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L2:0x00264b50
TrRet lv3_2517b0_l3(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L3:0x002517b0
TrRet eud_255f08_l4(uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3, uint32_t t0, uint32_t t1, uint32_t t2, uint32_t t3, float f12, float f13, float f14, float f15, float f16, float f17, float f18, float f19);  // L4:0x00255f08

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position

// a glow through the level's own build of glow_draw (Novalis's is Veldin's code: the port)
void level_glow(uint32_t point, uint32_t rgba, float size, float toward) {
    using Glow = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float, float, float);
    Glow g = nullptr;
    switch (ovl_reloc_level()) {
    case ARIDIA: g = lv2_264b50_l2; break;
    case KERWAN: g = lv3_2517b0_l3; break;
    case EUDORA: g = eud_255f08_l4; break;
    case RILGAR: g = rl2_28d788_l5; break;
    case BLARG: g = bl2_272498_l6; break;
    case BATALIA: g = bt2_26d590_l8; break;
    case POKITARU: g = pok_288cc0_l11; break;
    case HOVEN: g = hov_27bcd0_l12; break;
    case OLTANIS: g = olt_26b1b8_l14; break;
    default: glow_draw(point, rgba, size, toward); return;
    }
    g(point, rgba, 0, 0, 0, 0, 0, 0, size, toward, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
}  // namespace

// orig: L1:0x0030de68 L2:0x002f0eb0 L3:0x002e1898 L4:0x002e68f8 L5:0x0031e730 L6:0x0030adb0 L8:0x0030ab50 L11:0x0031c728 L12:0x0030a618 L14:0x00309bb0
void sandmouse_glow(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 2; ++k) level_glow(pv + 0x70 + k * 0x10, 0x2528aa28u, 0.1f, u2f(0x3da3d70au));
}

// The shot grows to its class's scale; for its first frames (life above 35 - 1 frames' worth) it turns toward its aimed
// pitch and yaw, then keeps its own; it flies 40 units a second along them. Out of the level's box (2..1021), more than
// 64 units from the camera or its life out, it leaves; else its path this frame is tested against the world and the
// mobys (its shooter ignored) with a hit record of its own (pushing the hit away from the hero: damage 0.5, kinds
// 0x10001): on a hit it stops at the hit point and leaves when what it hit takes no damage (sub_001f35d8 not 0), else its
// life is cut to 3 frames.
// orig: L1:0x0030c9a8 L2:0x002efee0 L3:0x002e08c8 L4:0x002e5928 L5:0x0031d760 L6:0x003094f0 L8:0x00308920 L11:0x0031b758 L12:0x00309648 L14:0x00308be0
void class1633_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t pos = m + 0x10;
    const float full = lwc1(lwu(m + 0x24) + 0x24);
    if (lwc1(m + 0x2c) < full)
        swc1(m + 0x2c, fpu_add(lwc1(m + 0x2c), fpu_mul(fpu_mul(lwc1(0x0015ed60), u2f(0x3e570a3du)), full)));
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t was = sp, move = sp + 0x10, rec = sp + 0x20;

    // the aim: the pvars' for the first frames, then the shot's own
    const bool aiming = lh(pv + 0x14) < frame_scale_i(0x23) - frame_scale_i(1);
    const float aim_pitch = aiming ? lwc1(pv + 8) : lwc1(m + 0x48);
    const float aim_yaw = aiming ? lwc1(pv + 0xc) : lwc1(m + 0x44);
    const float pi = u2f(0x40490fdbu), three_halves_pi = u2f(0x4096cbe4u);
    {
        const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        swc1(m + 0x48, angle_seek_rate(pv, lwc1(m + 0x48), aim_pitch, fpu_mul(sq, three_halves_pi), fpu_mul(sq, pi), fpu_mul(dt, pi)));
    }
    {
        const float sq = lwc1(0x0015ed70), dt = lwc1(0x0015ed6c);
        swc1(m + 0x44, angle_seek_rate(pv + 4, lwc1(m + 0x44), aim_yaw, fpu_mul(sq, three_halves_pi), fpu_mul(sq, pi), fpu_mul(dt, pi)));
    }
    vec_from_spherical_v(V(move), fpu_mul(lwc1(0x0015ed6c), 40.0f), lwc1(m + 0x48), -lwc1(m + 0x44));
    lq_sq(was, pos);
    vec3_add(V(pos), V(pos), V(move));

    const float x = lwc1(pos), y = lwc1(pos + 4), z = lwc1(pos + 8);
    bool leave = x < 2.0f || 1021.0f < x || y < 2.0f || 1021.0f < y || z < 2.0f || 1021.0f < z;
    leave = leave || x < 0.0f || y < 0.0f || z < 0.0f;
    leave = leave || 64.0f < vec2_distance(V(pos), V(OV(0x00166dc0)));
    if (!leave) {
        // the hit record: its direction (from the hero, flat, unit, z 1), the shot, kinds 0x10001, damage 0.5, the class
        sw(rec + 0x14, 0x10001);
        swc1(rec + 0x1c, 0.5f);
        sw(rec + 0x10, m);
        sw(rec + 0x20, 1);
        vec3_sub(V(rec), V(pos), V(HERO_POS));
        vec2_set_length(V(rec), V(rec), 1.0f);
        swc1(rec + 8, 1.0f);
        swc1(rec + 0xc, u2f(0x45afdf66u));
        sb(rec + 0x19, 1);
        sh(rec + 0x1a, lhu(m + 0xa6));
        sb(rec + 0x18, 1);
        leave = timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 0x14))) != 0;
    }
    if (leave) {
        moby_retire(m);
    } else if (segment_collide_v(V(was), V(pos), 0, lwu(pv + 0x10), static_cast<const uint32_t*>(ee_ram_ptr(rec))) != 0) {
        if (sub_001f35d8() != 0) {
            lq_sq(pos, OV(0x00173e60));
            moby_retire(m);
        } else if (frame_scale_i(3) < lh(pv + 0x14)) {
            sh(pv + 0x14, static_cast<uint32_t>(frame_scale_i(3)));
        }
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
