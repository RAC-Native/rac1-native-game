// Two of Rilgar's draw callbacks, hand-ported from Rilgar's overlay (`orig: L5:`): class 79's second draw (the air
// traffic's contrails) and class 838's (the five-sided glow). Both lay out quad_draw descriptors in their stack frame
// (16 / 10 of 0x90 bytes: +0 the four points, +0x40 their colours, +0x50 their (s, t), +0x70 CLAMP, +0x78 TEX0, +0x80
// TEX1, +0x88 ALPHA: quad_draw.h) and draw them for every moby of their group that is of the class and in its state 1.
// Declared in rilgar_part2.h, registered in rilgar_part2.cpp. The level's words (the colours, the blend selectors, the
// template corners) are L5_ADDR.
//
// VU0: neither function has a VU0 instruction of its own; their callees (quad_draw, point_on_screen, sin_vu, rgba_lerp,
// mat3_copy_affine, vec3_add) are the host ports the generated code called before, so the VU0 registers they leave are
// those of the generated code.
#include <game/math/vecmath.h>
#include <game/lib/libc.h>
#include <game/ovl/kalebo_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/rilgar_part2.h>
#include <game/ovl/variants_l5.h>

namespace rac1 {

namespace {
using kalebo::Frame;
using kalebo::for_each_group_moby;
using kalebo::quad_blend;
constexpr uint64_t TEX1 = 0x0000ff9000000260ull;  // (the descriptors' TEX1 word)
constexpr uint32_t QUAD = 0x90;                   // a descriptor's size
}  // namespace

// Class 79's contrails: sixteen descriptors (one per segment of a trail), their colours fading along the trail (the level's
// two colours 0x161408 / 0x16140c, by the segment's place over the trail's length 0x1613e0, which crawls with the frame
// counter), then for every class-79 moby of the group in state 1 whose point is on screen, its three trails (pvars +0x20,
// +0x120, +0x220: rings of 16 points, the newest at +0x322, +0x320 of them in use) drawn segment by segment as quads
// 0.3 high. Nothing while 0x1613f0 is set; the trails are skipped while 0x1613f4 is.
// orig: L5:0x002d7920
void ril_class79_draw_b_l5(uint32_t m) {
    if (lw(L5_ADDR(0x001613f0)) != 0) return;
    const Frame f(0xa10);
    const uint32_t sp = f.sp;
    swc1(L5_ADDR(0x001613e0), lbu(0x0015edb2) == 0 ? 5.0f : 16.0f);

    for (int32_t seg = 0; seg < 16; ++seg) {
        const uint32_t d = sp + static_cast<uint32_t>(seg) * QUAD;
        sd(d + 0x78, effect_tex0(0x13));
        sd(d + 0x80, TEX1);
        sd(d + 0x88, quad_blend(L5_ADDR(0x001613f8)));
        sd(d + 0x70, 0);
        const float crawl =
            fpu_div(int_to_float((lw(L5_ADDR(0x0015f5cc)) + 3) & 3), fpu_mul(lwc1(L5_ADDR(0x001613e0)), 4.0f));
        uint32_t corner = L5_ADDR(0x001cdd70);  // (the template's four (s, t))
        for (int32_t k = 0; k < 4; ++k, corner += 8) {
            swc1(d + 0x50 + static_cast<uint32_t>(k) * 8, lwc1(corner));
            swc1(d + 0x54 + static_cast<uint32_t>(k) * 8, lwc1(corner + 4));
            float t = fpu_div(int_to_float(seg - ((k >> 1) - 1)), fpu_add(lwc1(L5_ADDR(0x001613e0)), crawl));
            if (1.0f < t) t = 1.0f;
            else if (t < 0.0f) t = 0.0f;
            sw(d + 0x40 + static_cast<uint32_t>(k) * 4, rgba_lerp(lwu(L5_ADDR(0x00161408)), lwu(L5_ADDR(0x0016140c)), t));
        }
    }

    const uint32_t lift = sp + 0x900;  // the two offsets of a segment's ends: (0, 0, 0.15) and (0, 0, -0.15)
    libc_memset(lift, 0, 0x20);
    swc1(lift + 8, 0.15f);
    swc1(lift + 0x18, -0.15f);
    const uint32_t at = sp + 0x920;
    for_each_group_moby(lbu(m + 0x21), [&](uint32_t o) {
        if (lh(o + 0xa6) != 0x4f || lbu(o + 0x20) != 1) return;
        const uint32_t pv = lwu(o + 0x78);
        V(at) = V(o + 0x10);
        swc1(at + 0xc, 20.0f);
        if (point_on_screen(at, 512.0f) == -1) return;
        for (int32_t seg = 0; seg < lh(pv + 0x320); ++seg) {
            const uint32_t d = sp + static_cast<uint32_t>(seg * static_cast<int32_t>(QUAD));
            const int32_t newest = (lh(pv + 0x322) - seg + 0xf) & 0xf;
            for (uint32_t trail = 0; trail < 3; ++trail) {
                if (lw(L5_ADDR(0x001613f4)) != 0) continue;
                const uint32_t ring = pv + (trail << 8) + 0x20;
                for (int32_t k = 0; k < 4; ++k) {
                    const uint32_t point = ring + (static_cast<uint32_t>((newest + (k >> 1)) & 0xf) << 4);
                    vec3_add(V(d + static_cast<uint32_t>(k) * 0x10), V(point), V(lift + static_cast<uint32_t>(k & 1) * 0x10));
                }
                quad_draw(d, 0);
            }
        }
    });
}

// Class 838's glow: a five-sided ring of quads (two descriptors per side: the inner band from the template 0x20b7a0, the
// outer from 0x20b760, each side 1 higher than the one before), its colours pulsing round it (a sine over 64 frames between
// the level's colour pairs 0x161d3c / 0x161d40 and 0x161d44 / 0x161d48, a fifth of a turn apart per side), drawn in the
// frame of every class-838 moby of the group in state 1.
// orig: L5:0x0030de90
void ril_class838_draw_l5(uint32_t m) {
    const Frame f(0x6a0);
    const uint32_t sp = f.sp;
    float phase = fpu_mul(fpu_div(int_to_float(lw(L5_ADDR(0x0015f5cc)) & 0x3f), 63.0f), 6.283180236816406f);
    for (int32_t side = 0; side < 5; ++side) {
        const float pulse = fpu_mul(fpu_add(sin_vu(phase), 1.0f), 0.5f);
        const uint32_t outer_colour = rgba_lerp(lwu(L5_ADDR(0x00161d3c)), lwu(L5_ADDR(0x00161d40)), pulse);
        const uint32_t inner_colour = rgba_lerp(lwu(L5_ADDR(0x00161d44)), lwu(L5_ADDR(0x00161d48)), pulse);
        phase = angle_add(phase, 1.2566370964050293f);
        const float rise = static_cast<float>(side + 1);
        for (int32_t band = 0; band < 2; ++band) {
            const uint32_t d = sp + static_cast<uint32_t>(band * 0x2d0 + side * static_cast<int32_t>(QUAD));
            const uint64_t tex0 = effect_tex0(lw(L5_ADDR(0x00161d98)));
            sd(d + 0x88, quad_blend(L5_ADDR(0x00161d70)));
            sd(d + 0x78, tex0);
            sd(d + 0x80, TEX1);
            sd(d + 0x70, 0);
            const uint32_t corners = band == 0 ? L5_ADDR(0x0020b7a0) : L5_ADDR(0x0020b760);
            for (uint32_t k = 0; k < 4; ++k) {
                swc1(d + 0x50 + k * 8, lwc1(L5_ADDR(0x0020b740) + k * 8));
                swc1(d + 0x54 + k * 8, lwc1(L5_ADDR(0x0020b744) + k * 8));
                sw(d + 0x40 + k * 4, band == 0 ? inner_colour : outer_colour);
                V(d + k * 0x10) = V(corners + k * 0x10);
                swc1(d + k * 0x10 + 8, fpu_add(lwc1(d + k * 0x10 + 8), rise));
            }
        }
    }

    const uint32_t frame = sp + 0x5a0;
    for_each_group_moby(lbu(m + 0x21), [&](uint32_t o) {
        if (lh(o + 0xa6) != 0x346 || lbu(o + 0x20) != 1) return;
        mat3_copy_affine(M(frame), M(o + 0xc0));
        V(frame + 0x30) = V(o + 0x10);
        swc1(frame + 0x3c, 1.0f);
        for (uint32_t side = 0; side < 5; ++side) {
            quad_draw(sp + side * QUAD, frame);
            quad_draw(sp + 0x2d0 + side * QUAD, frame);
        }
    });
}

}  // namespace rac1
