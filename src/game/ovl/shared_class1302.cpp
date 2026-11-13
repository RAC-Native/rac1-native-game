// Class 1302 of Blarg (L6) and Orxon (L10), the Gadgebots' entrance (a sign counting the Gadgebots still to come, which opens
// the entrance's mobys when none is left): its update and its draw callback, one port of the two levels' copies, which are
// the same code relinked (the normalised texts compared). The level's own data is one block of the mesh's tables and one
// of the sign's tunings (per level below); the camera 0x166dc0, its yaw 0x166dd8 and the moby table 0x15ffd8 are the
// engine's (OV); the frame time 0x15ed6c, the planet 0x15ed84, the collected checkpoints 0x14c050, the hero's mode 0x1413f4
// and the mirrored-display flag 0x15edb4 are the base ELF's. Header shared_class1302.h.
//
// The pvars: the four mobys it opens +0 .. +0xc (indices in the moby table, -1 none; the first -1: the update does
// nothing), the count to show +0x10 and the one left +0x14, the bob's phase +0x18, the texture's scroll +0x1c, the ring's
// growth +0x20 (0 .. 1), the size +0x24 (0: hidden), the colour pulse's phase +0x28.
#include <game/ovl/shared_class1302.h>

#include <game/math/vecmath.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l10.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
// The level's mesh (its own data, one block): the points +0 (16 bytes each), their texture coordinates +0x90 (pairs of
// floats), their colours +0xd8 (a word each), the four quads' corners +0x100 (four point indices each).
uint32_t mesh(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x001da110) + o;
    return L6_ADDR(0x001fe8d0) + o;
}

// The sign's tunings (the level's own data, one block): the bob's height +0 and period +4 (seconds a turn), the colour
// pulse's period +8, the digits' colour swing +0xc, the full size +0x10.
uint32_t tunings(uint32_t o) {
    if (on_level(ORXON)) return L10_ADDR(0x00161f24) + o;
    return L6_ADDR(0x001622dc) + o;
}
// the draw callback (the level's copy of class1302_draw)
uint32_t draw_callback() { return on_level(ORXON) ? L10_ADDR(0x002e7520) : L6_ADDR(0x00307578); }

float frame_time() { return lwc1(0x0015ed6c); }

// the angle at `a` advanced by a turn every `period` seconds (the frame's share), stored and returned
float spin(uint32_t a, float period) {
    // (360 / period degrees, in radians)
    const float step = fpu_mul(fpu_mul(fpu_div(u2f(0x43b40000u), period), u2f(0x3c8efa35u)), frame_time());
    const float v = angle_add(lwc1(a), step);
    swc1(a, v);
    return v;
}

// the size +0x24 shrunk by two frames' time, down to 0
void shrink(uint32_t pv) {
    const float v = fpu_sub(lwc1(pv + 0x24), fpu_add(frame_time(), frame_time()));
    swc1(pv + 0x24, v);
    if (v < 0.0f) swc1(pv + 0x24, 0.0f);
}

// State 1: the sign grows in while the camera is within 32 units and shrinks away past 36; shown, it is drawn, bobs and
// (the hero not Clank-sized: mode 1) shows the count left in digits whose colour pulses with the moby's. With nothing left
// to count, or the entrance's checkpoint already collected, it opens its mobys and goes to state 2.
void wait_for_bots(uint32_t m, uint32_t pv) {
    const uint32_t sp = ee_stack_alloc(0x30);
    const uint32_t at = sp, digit = sp + 0x10;
    lq_sq(at, m + 0x10);
    swc1(at + 8, fpu_add(lwc1(at + 8), u2f(0x40300000u)));  // (2.75 above the moby)
    const uint32_t cam = OV(0x00166dc0);
    if (vec3_distance(V(cam), V(m + 0x10)) < u2f(0x42000000u)) {  // (32)
        const float full = lwc1(tunings(0x10));
        const float v = fpu_add(lwc1(pv + 0x24), fpu_add(frame_time(), frame_time()));
        swc1(pv + 0x24, v);
        if (full < v) swc1(pv + 0x24, full);
    } else if (u2f(0x42100000u) < vec3_distance(V(cam), V(m + 0x10))) {  // (36)
        shrink(pv);
    }
    if (0.0f < lwc1(pv + 0x24)) {
        draw_callback_add(draw_callback(), m);
        const float bob = spin(pv + 0x18, lwc1(tunings(4)));
        swc1(at + 8, fpu_add(lwc1(at + 8), fpu_mul(lwc1(tunings(0)), cos_vu(bob))));
        const int32_t swing = float_to_int(fpu_mul(lwc1(tunings(0xc)), cos_vu(lwc1(pv + 0x28))));
        const uint32_t colour = 0x20000000u | (static_cast<uint32_t>(ee_div(swing, 3) + 0xd) << 16) |
                                (static_cast<uint32_t>(swing / 2 + 0x78) << 8) | static_cast<uint32_t>(swing + 0x96);
        if (lbu(0x001413f4) == 1) {
            const int32_t left = lw(pv + 0x14);
            if (left < 10) {
                debug_particle_l1(at, m, static_cast<uint32_t>(left + 2), colour, 0x7f, 4, 0xff, 0.5f);
            } else {
                // two digits side by side across the moby's facing (+0x48), the tens first; mirrored while 0x15edb4 is set
                const bool mirrored = lbu(0x0015edb4) != 0;
                const float tens_at = mirrored ? u2f(0x3e19999au) : u2f(0xbecccccdu);  // (0.15 / -0.4)
                const float ones_at = mirrored ? u2f(0xbecccccdu) : u2f(0x3e19999au);
                const float side = u2f(0x3fc90fdbu);  // (pi / 2)
                auto place = [m, at, digit, side](float offset) {
                    swc1(digit, fpu_mul(cos_vu(angle_add(lwc1(m + 0x48), side)), offset));
                    swc1(digit + 4, fpu_mul(sin_vu(angle_add(lwc1(m + 0x48), side)), offset));
                    swc1(digit + 8, 0.0f);
                    vec3_add_inplace(V(digit), V(at));
                };
                place(tens_at);
                debug_particle_l1(digit, m, static_cast<uint32_t>(ee_div(lw(pv + 0x14), 10) + 2), colour, 0x7f, 4, 0xff, 0.5f);
                place(ones_at);
                debug_particle_l1(digit, m, static_cast<uint32_t>(ee_rem(lw(pv + 0x14), 10) + 2), colour, 0x7f, 4, 0xff, 0.5f);
            }
        }
    }
    ee_stack_free(0x30);
    if (lw(pv + 0x14) > 0 && lbu(0x0014c050 + (lwu(0x0015ed84) << 4) + lbu(m + 0xb0)) != 0xff) return;
    const uint32_t first = lwu(OV(0x0015ffd8));
    for (uint32_t k = 0; k < 4; ++k) {
        const int32_t idx = lw(pv + k * 4);
        if (idx != -1) class1302_open(m, first + (static_cast<uint32_t>(idx) << 8));
    }
    sb(m + 0x20, 2);
}

// the rotation about z by the camera's yaw (into `matrix`, its angles at `angles`)
void facing(uint32_t matrix, uint32_t angles) {
    swc1(angles + 8, lwc1(OV(0x00166dd8)));
    sw(angles, 0);
    sw(angles + 4, 0);
    euler_to_mat3_vu(M(matrix), V(angles));
}

// a value that grows by 0.01 a frame (`wrap`: back by 1 past 1, else to 0)
float grow(uint32_t a, bool wrap) {
    const float v = fpu_add(lwc1(a), u2f(0x3c23d70au));
    swc1(a, v);
    if (1.0f < v) {
        if (wrap) swc1(a, fpu_sub(v, 1.0f));
        else sw(a, 0);
    }
    return lwc1(a);
}
}  // namespace

// The four quads of the level's mesh (the moby's size scaling x and y), turned to the camera's yaw about the moby, with the
// effect texture 0x18 scrolling down (+0x1c, a hundredth a frame); then a quad of the effect texture 0x1b over them, its
// corners spreading (1.2 x the growth, x the size) and rising (1.1 + 1.4 x the growth, the far edge 0.2 lower) as it grows,
// its alpha fading from 128 to 0.
// orig: L6:0x00307578 L10:0x002e7520
void class1302_draw(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0x270);
    const uint32_t quad = sp, colours = sp + 0x40, uvs = sp + 0x50, regs = sp + 0x70, turned = sp + 0x90;
    const uint32_t angles = sp + 0xd0, matrix = sp + 0xe0, corners = sp + 0x110, ring = sp + 0x150;
    const uint32_t pv = lwu(m + 0x78);
    facing(matrix, angles);
    grow(pv + 0x1c, true);
    sd(regs + 0x18, 0x0000004000000044ull);
    sd(regs + 0x10, 1);
    sd(regs, 0);
    sd(regs + 8, effect_tex0(0x18));
    const float px = lwc1(m + 0x10), py = lwc1(m + 0x14), pz = lwc1(m + 0x18);
    for (uint32_t q = 0; q < 4; ++q) {
        const uint32_t e = mesh(0x100) + q * 0x10;
        for (uint32_t k = 0; k < 4; ++k) mat3_transform(V(turned + k * 0x10), V(mesh(0) + (lwu(e + k * 4) << 4)), M(matrix));
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t t = turned + k * 0x10, c = quad + k * 0x10;
            swc1(c, fpu_add(px, fpu_mul(lwc1(t), lwc1(pv + 0x24))));
            swc1(c + 4, fpu_add(py, fpu_mul(lwc1(t + 4), lwc1(pv + 0x24))));
            swc1(c + 8, fpu_add(pz, lwc1(t + 8)));
        }
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t idx = lwu(e + k * 4);
            sw(colours + k * 4, lwu(mesh(0xd8) + (idx << 2)));
            swc1(uvs + k * 8, lwc1(mesh(0x90) + (idx << 3)));
            swc1(uvs + k * 8 + 4, fpu_add(lwc1(mesh(0x94) + (idx << 3)), lwc1(pv + 0x1c)));
        }
        quad_draw(quad, 0);
    }

    sd(regs + 8, effect_tex0(0x1b));
    grow(pv + 0x20, false);
    for (uint32_t k = 0; k < 4; ++k)
        for (uint32_t w = 0; w < 0x10; w += 4) sw(ring + k * 0x10 + w, 0);
    const float spread = u2f(0x3f99999au), rise = u2f(0x3fb33333u), base = u2f(0x3f8ccccdu), drop = u2f(0x3e4ccccdu);
    auto out = [pv, spread](bool negative) {  // (+-) the growth x 1.2 x the size
        const float g = negative ? -lwc1(pv + 0x20) : lwc1(pv + 0x20);
        return fpu_mul(fpu_mul(g, spread), lwc1(pv + 0x24));
    };
    auto up = [pv, rise, base, drop](bool far) {
        const float h = fpu_add(fpu_mul(lwc1(pv + 0x20), rise), base);
        return far ? fpu_sub(h, drop) : h;
    };
    swc1(ring, out(true));
    swc1(ring + 4, out(true));
    swc1(ring + 8, up(false));
    swc1(ring + 0x10, out(false));
    swc1(ring + 0x14, out(true));
    swc1(ring + 0x18, up(true));
    swc1(ring + 0x20, out(true));
    swc1(ring + 0x24, out(false));
    swc1(ring + 0x28, up(false));
    swc1(ring + 0x30, out(false));
    swc1(ring + 0x34, out(false));
    swc1(ring + 0x38, up(true));
    for (uint32_t k = 0; k < 4; ++k) lq_sq(corners + k * 0x10, ring + k * 0x10);
    facing(matrix, angles);
    for (uint32_t k = 0; k < 4; ++k) mat3_transform(V(turned + k * 0x10), V(corners + k * 0x10), M(matrix));
    for (uint32_t k = 0; k < 4; ++k) {
        const uint32_t t = turned + k * 0x10, c = quad + k * 0x10;
        swc1(c, fpu_add(px, lwc1(t)));
        swc1(c + 4, fpu_add(py, lwc1(t + 4)));
        swc1(c + 8, fpu_add(pz, lwc1(t + 8)));
    }
    const uint32_t colour = (static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(1.0f, lwc1(pv + 0x20)), 128.0f))) << 24) | 0x808080u;
    for (uint32_t k = 0; k < 4; ++k) sw(colours + k * 4, colour);
    static const float kUv[8] = {0.0f, 0.0f, 1.0f, 0.0f, 0.0f, 1.0f, 1.0f, 1.0f};
    for (uint32_t k = 0; k < 8; ++k) swc1(uvs + k * 4, kUv[k]);
    quad_draw(quad, 0);
    ee_stack_free(0x270);
}

// One of the entrance's mobys opened: a class-1028 moby in state 1 goes to 2; a class-1015 / 1282 moby in state 2 goes to 3
// with its sound; a class-1346 moby goes to state 2 with sound 0x434, its +0x30 set to 0xff, and the entrance's checkpoint
// (+0xb0) is marked collected.
// orig: L6:0x00307c48 L10:0x002e7bf0
void class1302_open(uint32_t m, uint32_t o) {
    switch (lh(o + 0xa6)) {
    case 1028:
        if (lbu(o + 0x20) == 1) sb(o + 0x20, 2);
        break;
    case 1015:
    case 1282:
        if (lbu(o + 0x20) == 2) {
            sb(o + 0x20, 3);
            moby_sound_start(0, 0, o);
        }
        break;
    case 1346:
        sb(o + 0x20, 2);
        moby_sound_play(0, 0, o, 0x434);
        sb(o + 0x30, 0xff);
        checkpoint_mark_collected(lbu(m + 0xb0));
        break;
    default:
        break;
    }
}

// Every frame, the moby's colour (+0x90) pulses between two greys (the pulse's period the level's). State 0 takes the count
// to show (+0x14 from +0x10), random phases, the full size, and goes to state 1 (wait_for_bots); in state 2 the sign
// shrinks away, drawn while it lasts.
// orig: L6:0x00307d30 L10:0x002e7cd8
void class1302_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(pv) == -1) return;
    const float pulse = spin(pv + 0x28, lwc1(tunings(8)));
    sw(m + 0x90, rgba_lerp(0x80464646u, 0x80828282u, fpu_mul(fpu_add(cos_vu(pulse), 1.0f), 0.5f)));
    switch (lbu(m + 0x20)) {
    case 0:
        sw(pv + 0x14, lwu(pv + 0x10));
        swc1(pv + 0x18, rand_angle());
        swc1(pv + 0x28, rand_angle());
        sb(m + 0x20, 1);
        swc1(pv + 0x24, lwc1(tunings(0x10)));
        break;
    case 1:
        wait_for_bots(m, pv);
        break;
    case 2:
        shrink(pv);
        if (0.0f < lwc1(pv + 0x24)) draw_callback_add(draw_callback(), m);
        break;
    default:
        break;
    }
}

}  // namespace rac1
