// Pokitaru's (level 11) teleporter (class 318), ported by hand from the level's own code (L11:0x2f2518 and its helpers
// 0x2f2cd0, 0x2f3040): a pad with three rotating pieces (class 315) that sends the hero to its partner pad (pvar +0, or
// +4 once the checkpoint +0xc is taken) on the triangle button, its glow drawn by the level's callback 0x2f2d58 while it
// fades. The original's order of calls and float operations kept.
#include <game/ovl/pokitaru_own.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pokitaru_mobys.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l11.h>

namespace rac1 {

namespace {
bool taken(int32_t index) {  // the level's checkpoint `index` (0x14c050's row of the level) taken (0xff)
    return lbu(0x0014c050 + static_cast<uint32_t>(index) + (static_cast<uint32_t>(lw(0x0015ed84)) << 4)) == 0xff;
}
// the pad `m` and its three pieces (pvar +0x14 ..) hidden (+0x34 bit 0, not drawn, no class model +0x94)
void hide_all(uint32_t m, uint32_t pv) {
    sb(m + 0x31, 0);
    sw(m + 0x94, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t p = lwu(pv + 0x14 + (k << 2));
        sh(p + 0x34, lhu(p + 0x34) | 1u);
        sb(p + 0x31, 0);
        sw(p + 0x94, 0);
    }
}
// the fade's two values (pvar +0x20 / +0x24, their speeds +0x28 / +0x2c) toward `target` at the level's rates
float fade_seek(uint32_t pos, uint32_t vel, float target) {
    const float t2 = lwc1(0x0015ed70);
    return seek_1d(pos, vel, target, fpu_mul(lwc1(L11_ADDR(0x00161910u)), t2), fpu_mul(lwc1(L11_ADDR(0x00161914u)), t2),
                   fpu_mul(lwc1(L11_ADDR(0x00161918u)), lwc1(0x0015ed6c)));
}
}  // namespace

// The prompt of the pad's kind (pvar +0x10: 0, 3 or the others; out of range none).
// orig: L11:0x002f2cd0
void pok_2f2cd0_l11(uint32_t m) {
    switch (lwu(lwu(m + 0x78) + 0x10)) {  // (jump table L11:0x21b080)
    case 0:  // (0x002f2d04)
        hud_message_request(0xb, TXT(0x2b0d));
        break;
    case 3:  // (0x002f2d18)
        hud_message_request(0xb, TXT(0x2b0b));
        break;
    case 1: case 2: case 4:  // (0x002f2d2c)
        hud_message_request(0xb, TXT(0x2b0c));
        break;
    default:  // (0x002f2d40)
        hud_message_request(0xb, 0);
        break;
    }
}

// The pad `m` and its three pieces (pvar +0x14 .., those there) shown: drawn, their class's model at +0x94.
// orig: L11:0x002f3040
void pok_2f3040_l11(uint32_t m) {
    sb(m + 0x31, 1);
    sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    sw(m + 0x94, lwu(lwu(m + 0x24) + 0x10));
    const uint32_t pv = lwu(m + 0x78);
    for (uint32_t k = 0; k < 3; ++k) {
        const uint32_t p = lwu(pv + 0x14 + (k << 2));
        if (p == 0) continue;
        sh(p + 0x34, lhu(p + 0x34) & 0xfffeu);
        sb(p + 0x31, 1);
        sw(p + 0x94, lwu(lwu(p + 0x24) + 0x10));
    }
}

// Class 318: state 0 spawns the three pieces (class 315, 120 degrees apart, the hero moby's colour words +0x38) and
// hides the pad while its checkpoint (pvar +8) is not taken; 1 shows it once taken and, with the hero standing on it,
// shows the prompt and on the triangle (pad bit 0x10, the prompt 0xb up) starts (2: the sound, the camera's target,
// the sphere's contact); 2 fades in (+0x20, then +0x24), then (unless the prompt is still up) 3 when the hero's
// teleport answer (0x233f38) is 2 (the glow 1.95), 4 when it is 0; 3 lets the glow fade (drawn), then puts the hero on
// the partner pad, starts the music its pvar names (+0x30 / +0x34), shows the partner in its state 4 and comes back
// to 1; 4 (the partner) notes the hero below 138 (0x13d3dd), lets the glow come up (drawn), fades out, then 1.
// orig: L11:0x002f2518
void pok_class318_update_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const float dt = lwc1(0x0015ed6c);
    switch (lbu(m + 0x20)) {  // (jump table L11:0x21b060)
    case 0: {  // (0x002f2578)
        sb(m + 0x20, 1);
        const float third = u2f(0x40060a92u);  // (120 degrees)
        for (int32_t k = 0; k < 3; ++k) {
            const uint32_t slot = pv + 0x14 + (static_cast<uint32_t>(k) << 2);
            sw(slot, moby_spawn(0x13b));
            sh(lwu(slot) + 0x32, 0x40);
            sb(lwu(slot) + 0x31, 1);
            const uint32_t hero = lwu(0x001413d0);
            sw(lwu(slot) + 0x38, lwu(hero + 0x38));
            sw(lwu(slot) + 0x3c, lwu(hero + 0x3c));
            sh(lwu(slot) + 0x34, lhu(m + 0x34));
            const uint32_t p = lwu(slot);
            lq_sq(p + 0x10, m + 0x10);
            lq_sq(p + 0x40, m + 0x40);
            swc1(lwu(slot) + 0x48, angle_add(lwc1(p + 0x48), fpu_mul(int_to_float(k), third)));
        }
        if (lw(pv + 8) >= 0 && !taken(lw(pv + 8))) hide_all(m, pv);
        teleporter_pieces_update(m);
        break;
    }
    case 1: {  // (0x002f26ac)
        if (lw(pv) == -1) break;
        if ((lhu(m + 0x34) & 1u) != 0 && lw(pv + 8) >= 0 && taken(lw(pv + 8))) pok_2f3040_l11(m);
        if (lwu(0x0013f64c) != m || lh(0x0013f65e) != 0) break;
        pok_2f2cd0_l11(m);
        if ((lwu(0x0013cae4) & 0x10u) == 0 || lw(L11_ADDR(0x0015f594u)) != 0xb) break;
        moby_sound_start(0, 0, m);
        sb(m + 0x20, 2);
        sw(pv + 0x2c, 0);
        sw(pv + 0x28, 0);
        hero_cam_target_set(m + 0x10, lwc1(m + 0x48), 0);
        moby_sphere_contact(m, m + 0x10, 0x10000, 0, 1, 0, 1.5f, 20.0f, 1.0f);
        break;
    }
    case 2: {  // (0x002f27bc)
        const int32_t answer = sub_00233f38();
        if (lwc1(pv + 0x20) < 1.0f) {
            fade_seek(pv + 0x20, pv + 0x28, 1.0f);
        } else if (lwc1(pv + 0x24) < 1.0f) {
            fade_seek(pv + 0x24, pv + 0x2c, 1.0f);
        } else if (!(lw(L11_ADDR(0x0015f594u)) == 0xb && 0.25f < lwc1(pv + 0x20))) {
            if (answer == 2) {
                swc1(pv + 0x3c, u2f(0x3ff9999au));
                sb(m + 0x20, 3);
            } else if (answer == 0) {
                sb(m + 0x20, 4);
            }
        }
        teleporter_pieces_update(m);
        break;
    }
    case 3: {  // (0x002f28b0)
        if (lwc1(pv + 0x3c) != 0.0f) {
            sub_001f93e0(L11_ADDR(0x002f2d58u), m);
            approach_f(pv + 0x3c, 0.0f, fpu_mul(dt, 4.0f));
            break;
        }
        uint32_t to = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv) << 8);
        if (lw(pv + 4) != -1 && taken(lw(pv + 0xc))) to = lwu(L11_ADDR(0x0015ffd8u)) + (lwu(pv + 4) << 8);
        const uint32_t tpv = lwu(to + 0x78);
        const uint32_t at = ee_stack_alloc(0x10);
        lq_sq(at, to + 0x10);
        swc1(at + 8, fpu_add(lwc1(at + 8), u2f(0x3e4ccccdu)));
        hero_place(at, to + 0x40, 0, 1);
        ee_stack_free(0x10);
        sb(0x001413f5, 1);
        if (lw(pv + 0x30) != -1 && lw(pv + 0x34) != -1) music_track_request(lw(pv + 0x30), lwu(pv + 0x34));
        pok_2f3040_l11(to);
        sb(to + 0x20, 4);
        sw(tpv + 0x3c, 0);
        sw(tpv + 0x28, 0);
        sw(tpv + 0x2c, 0);
        swc1(tpv + 0x24, 1.0f);
        swc1(tpv + 0x20, 1.0f);
        moby_sound_start(1, 0, to);
        sb(m + 0x20, 1);
        sw(pv + 0x20, 0);
        sw(pv + 0x24, 0);
        sw(pv + 0x28, 0);
        sw(pv + 0x2c, 0);
        swc1(pv + 0x3c, u2f(0x3ff9999au));
        teleporter_pieces_update(m);
        break;
    }
    case 4: {  // (0x002f2a38)
        if (lwc1(0x0013f3d8) < 138.0f) sb(0x0013d3dd, 1);
        const float glow = u2f(0x3ff9999au);  // (1.95)
        if (lwc1(pv + 0x3c) < glow) {
            sub_001f93e0(L11_ADDR(0x002f2d58u), m);
            approach_f(pv + 0x3c, glow, fpu_mul(dt, 4.0f));
        } else if (0.0f < lwc1(pv + 0x24)) {
            fade_seek(pv + 0x24, pv + 0x28, 0.0f);
        } else if (0.0f < lwc1(pv + 0x20)) {
            fade_seek(pv + 0x20, pv + 0x2c, 0.0f);
        } else {
            sb(m + 0x20, 1);
        }
        teleporter_pieces_update(m);
        break;
    }
    default:
        break;
    }
}

// Teleporter glow ring draw-list callback: builds 32 vertical quads around the pad at radius pvar +0x3c,
// adjusting the hero-visible flag (0x1413f5) as the ring contracts past 0.65.
// orig: L11:0x002f2d58
void pok_cb_2f2d58_l11(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0xd0);
    const uint32_t mat = sp + 0x90;
    mat3_copy_affine(M(mat), M(m + 0xc0));
    lq_sq(mat + 0x30, m + 0x10);
    sd(sp + 0x70, 0);
    sd(sp + 0x78, effect_tex0(lw(L11_ADDR(0x00161930u))));
    sd(sp + 0x80, (0xff90ULL << 32) | 0x260ULL);
    const uint64_t alpha = static_cast<uint64_t>(lwu(L11_ADDR(0x00161920u))) |
                           (static_cast<uint64_t>(lwu(L11_ADDR(0x00161924u))) << 2) |
                           (static_cast<uint64_t>(lwu(L11_ADDR(0x00161928u))) << 4) |
                           (static_cast<uint64_t>(lwu(L11_ADDR(0x0016192cu))) << 6) |
                           (0x8000ULL << 24);
    sd(sp + 0x88, alpha);
    const uint32_t col = lwu(L11_ADDR(0x00161934u));
    for (uint32_t k = 0; k < 4; ++k) {
        swc1(sp + 0x50 + (k << 3), lwc1(L11_ADDR(0x001d3530u) + (k << 3)));
        swc1(sp + 0x54 + (k << 3), lwc1(L11_ADDR(0x001d3530u) + (k << 3) + 4));
        sw(sp + 0x40 + (k << 2), col);
    }
    const float radius = lwc1(pv + 0x3c);
    float half_h = u2f(0x3dcccccd);
    if (radius < u2f(0x3f933333u)) {
        const float t = fpu_sub(u2f(0x3f933333u), radius);
        float t2 = fpu_add(t, t);
        if (1.0f < t2) t2 = 1.0f;
        else if (t2 < 0.0f) t2 = 0.0f;
        half_h = fpu_add(t2, u2f(0x3dcccccd));
        sb(0x001413f5u, (radius < u2f(0x3f266666u)) ? 1u : 0u);
    }
    const float two_pi = u2f(0x40c90fd0u);
    const float pi = u2f(0x40490fd0u);
    for (int32_t seg = 0; seg < 32; ++seg) {
        for (int32_t v = 0; v < 4; ++v) {
            const uint32_t dst = sp + (static_cast<uint32_t>(v) << 4);
            const float ang = fpu_sub(fpu_mul(fpu_mul(static_cast<float>(seg + (v >> 1)), two_pi), 0.03125f), pi);
            swc1(dst + 0x0, fpu_mul(cos_vu(ang), lwc1(pv + 0x3c)));
            swc1(dst + 0x4, fpu_mul(sin_vu(ang), lwc1(pv + 0x3c)));
            const float z_base = lwc1(L11_ADDR(0x00161938u));
            const float z = ((v & 1) != 0) ? fpu_add(z_base, half_h) : fpu_sub(z_base, half_h);
            swc1(dst + 0x8, fpu_add(z, 0.0f));
            swc1(dst + 0xc, 1.0f);
        }
        quad_draw(sp, mat);
    }
    ee_stack_free(0xd0);
}

}  // namespace rac1
