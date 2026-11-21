// The scrolling textures of Batalia (level 8), Pokitaru (11) and Hoven (12): meshes of the level (their points, colours and
// texture coordinates in its tables) drawn each frame with their texture moved by an offset that scrolls by its speed, and
// the helpers they share (the colours' tint, a wave). Each function's copies are the same code bound to other tables of the
// level: the copies are the "sets" below, each with its own tables (level words, per set). The frame's time scale 0x15ed7c
// is the base ELF's. Header shared_scroll.h.
#include <game/ovl/shared_scroll.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l11.h>
#include <game/ovl/variants_l12.h>
#include <game/ovl/variants_l8.h>
#include <game/ovl/vu0_regs.h>

namespace rac1 {

namespace {
float frame_dt() { return lwc1(0x0015ed7c); }

// A set of meshes: the buffer their moved coordinates go to, and its tables (4 bytes a mesh): colours, points, coordinates,
// counts; the offsets (8 bytes each); the meshes.
struct MeshSet {
    uint32_t buffer, colours, points, coords, counts, offsets;
    int32_t meshes;
};
MeshSet mesh_set(int32_t set) {
    switch (set) {
    case 0: return {L8_ADDR(0x001e8360), L8_ADDR(0x001e2828), L8_ADDR(0x001e2798), L8_ADDR(0x001e27c8), L8_ADDR(0x001ddf40), L8_ADDR(0x00161e10), 11};
    case 1: return {L8_ADDR(0x001e8360), L8_ADDR(0x001ddf20), L8_ADDR(0x001ddec0), L8_ADDR(0x001ddee0), L8_ADDR(0x001db518), L8_ADDR(0x00161e20), 8};
    case 2: return {L8_ADDR(0x001f8560), L8_ADDR(0x001f4c48), L8_ADDR(0x001f4c00), L8_ADDR(0x001f4c18), L8_ADDR(0x001f01c8), L8_ADDR(0x001623f8), 6};
    case 3: return {L11_ADDR(0x001ff770), L11_ADDR(0x001ff730), L11_ADDR(0x001ff6d0), L11_ADDR(0x001ff6f0), L11_ADDR(0x001f9a78), L11_ADDR(0x00162598), 8};
    case 4: return {L11_ADDR(0x001ff770), L11_ADDR(0x001f9a48), L11_ADDR(0x001f99b8), L11_ADDR(0x001f99e8), L11_ADDR(0x001f4978), L11_ADDR(0x001625a8), 11};
    case 5: return {L11_ADDR(0x00217770), L11_ADDR(0x002078d8), L11_ADDR(0x00207860), L11_ADDR(0x00207888), L11_ADDR(0x00200728), L11_ADDR(0x00162618), 9};
    case 6: return {L11_ADDR(0x00217770), L11_ADDR(0x00217740), L11_ADDR(0x002176f8), L11_ADDR(0x00217710), L11_ADDR(0x00215268), L11_ADDR(0x00162628), 5};
    case 8: return {L8_ADDR(0x001e8360), L8_ADDR(0x001e8310), L8_ADDR(0x001e8268), L8_ADDR(0x001e82a0), L8_ADDR(0x001e2858), L8_ADDR(0x001e8348), 13};
    case 9: return {L8_ADDR(0x001f8560), L8_ADDR(0x001f8528), L8_ADDR(0x001f84c8), L8_ADDR(0x001f84e8), L8_ADDR(0x001f4c60), L8_ADDR(0x001f8548), 8};
    case 10: return {L11_ADDR(0x00217770), L11_ADDR(0x00215220), L11_ADDR(0x00215148), L11_ADDR(0x00215190), L11_ADDR(0x00207900), L11_ADDR(0x00217758), 17};
    case 11: return {L8_ADDR(0x001f8560), L8_ADDR(0x001623e8), L8_ADDR(0x001623d0), L8_ADDR(0x001623d8), L8_ADDR(0x001623c8), L8_ADDR(0x00162408), 2};
    default: return {L12_ADDR(0x00208b00), L12_ADDR(0x00208ae0), L12_ADDR(0x00208a98), L12_ADDR(0x00208ab0), L12_ADDR(0x00205a80), L12_ADDR(0x001620f8), 5};
    }
}

// a scroll's speeds and offsets (8 bytes each)
struct ScrollSet {
    uint32_t speeds, offsets;
};
ScrollSet scroll_set(int32_t set) {
    switch (set) {
    case 0: return {L8_ADDR(0x00161de0), L8_ADDR(0x00161e10)};
    case 1: return {L8_ADDR(0x00161df8), L8_ADDR(0x00161e20)};
    case 2: return {L8_ADDR(0x001623a0), L8_ADDR(0x001623f8)};
    case 3: return {L8_ADDR(0x001623b8), L8_ADDR(0x00162408)};
    case 4: return {L11_ADDR(0x00162508), L11_ADDR(0x00162598)};
    case 5: return {L11_ADDR(0x00162520), L11_ADDR(0x001625a8)};
    case 6: return {L11_ADDR(0x001625d8), L11_ADDR(0x00162618)};
    case 7: return {L11_ADDR(0x001625f0), L11_ADDR(0x00162628)};
    case 8: return {L8_ADDR(0x001db500), L8_ADDR(0x001e8348)};
    case 9: return {L8_ADDR(0x001ef790), L8_ADDR(0x001f8548)};
    case 10: return {L11_ADDR(0x001f14d0), L11_ADDR(0x001ff750)};
    default: return {L11_ADDR(0x00200710), L11_ADDR(0x00217758)};
    }
}

// one coordinate of an offset moved by its speed, wrapped by 1 above 1 and below -1
void scroll(uint32_t offset, uint32_t speed) {
    const float one = 1.0f;
    float v = fpu_add(lwc1(offset), fpu_mul(lwc1(speed), frame_dt()));
    swc1(offset, v);
    if (one < v) swc1(offset, fpu_sub(v, one));
    if (lwc1(offset) < -one) swc1(offset, fpu_add(lwc1(offset), one));
}

// one phase moved by `step`, wrapped by 2 above 1 and below -1
void wave_phase(uint32_t phase, float step) {
    const float v = fpu_add(lwc1(phase), step);
    swc1(phase, v);
    if (1.0f < v) swc1(phase, fpu_sub(v, 2.0f));
    if (lwc1(phase) < -1.0f) swc1(phase, fpu_add(lwc1(phase), 2.0f));
}
}  // namespace

// The VU0 does it in macro mode, two pairs at a time: vf11 = (u, v, u, vf1.w + v) from the offset, with vf1's x, y, z cleared
// (its w as the VU0 holds it: the original's), each quadword of `src` plus vf11 to `dst`, an odd last pair's two words alone.
// orig: L8:0x00258090 L11:0x002739c0 L12:0x002668d4
void uv_offset_pairs(uint32_t dst, uint32_t src, int32_t n, uint32_t offset) {
    using namespace vu0m;
    eer::Q128 uv{};
    uv.lo = ld(offset);
    eer::qmtc2(11, uv);
    Vf* r = vf();
    eer::vmacro();
    addbc(r[1], r[0], r[0].v[0], XYZ);
    eer::vmacro();
    addbc(r[11], r[1], r[11].v[0], Z);
    eer::vmacro();
    addbc(r[11], r[1], r[11].v[1], W);
    int32_t quads = static_cast<int32_t>(static_cast<uint32_t>(n) >> 1);
    const bool odd = (n & 1) != 0;
    eer::lqc2_(3, src);
    src += 0x10;
    do {  // (as the original: at least one quadword)
        eer::vmacro();
        r[2] = r[3];
        --quads;
        eer::lqc2_(3, src);
        src += 0x10;
        eer::vmacro();
        add(r[2], r[2], r[11], XYZW);
        eer::sqc2_(dst, 2);
        dst += 0x10;
    } while (quads != 0);
    if (!odd) return;
    eer::vmacro();
    add(r[3], r[3], r[11], XYZW);
    sd(dst, eer::qmfc2(3).lo);
}

// orig: L8:0x002f0c18 L8:0x002f0cf0 L8:0x00308e90 L11:0x0031d9a0 L11:0x0031da78 L11:0x0031e780 L11:0x0031e858 L12:0x0030bd90
//       L8:0x002f0b40 L8:0x00308db8 L11:0x0031e6a8 L8:0x00308f68
void uv_scroll_meshes(int32_t set, uint32_t index) {
    const MeshSet s = mesh_set(set);
    for (int32_t i = 0; i < s.meshes; ++i) {
        const uint32_t at = static_cast<uint32_t>(i) * 4;
        uv_offset_pairs(s.buffer, lwu(s.coords + at), lw(s.counts + at), s.offsets + (index << 3));
        prim_list_packet(lw(s.counts + at), lwu(s.points + at), lwu(s.colours + at), s.buffer, 1);
    }
}

namespace {
// Batalia's two draws of three layers of scrolling meshes: the GS state (the alpha blend, the test, the clamp and the
// fog's colour), the shadows' VU1 set-up, then per layer its texture, each set's alpha (the level's words; the second
// layer's scaled by the level's fade byte) and its meshes: the first layer's three sets of offsets, the second's two, the
// third's one.
struct LayerDraw {
    uint64_t blend;                // the first layer's alpha blend mode (0x44 or 0x64)
    uint32_t alpha0, alpha1, alpha2, fade, alpha3, alpha4, alpha5;  // the level's words (fade: a byte)
    int32_t set0, set1, set2;      // the layers' mesh sets
};
void layers_draw(const LayerDraw& d) {
    sub_002a0ba8(0x42, (0x8000ull << 24) | 0x44);  // (ALPHA_1)
    sub_002a0ba8(8, 0);                             // (TEST_1)
    sub_002a0ba8(0x14, (0xff90ull << 32) | 0x260);  // (CLAMP_1)
    sub_002a0ba8(0x47, 0x513f1);                    // (TEST_2)
    shadows_vu1_setup();
    auto alpha = [](uint32_t a, uint64_t mode) { sub_002a0ba8(0x42, (static_cast<uint64_t>(a) << 32) | mode); };
    auto faded = [&d](uint32_t a) { return static_cast<uint32_t>(static_cast<int32_t>(lwu(a) * lbu(d.fade)) >> 8); };
    sub_002a0ba8(6, effect_tex0(0x3a));  // (TEX0_1)
    alpha(lwu(d.alpha0), d.blend);
    uv_scroll_meshes(d.set0, 0);
    sub_002a0ba8(6, effect_tex0(0x3b));
    alpha(faded(d.alpha1), 0x62);
    uv_scroll_meshes(d.set0, 1);
    alpha(faded(d.alpha2), 0x68);
    uv_scroll_meshes(d.set0, 2);
    sub_002a0ba8(6, effect_tex0(0x3c));
    alpha(lwu(d.alpha3), 0x48);
    uv_scroll_meshes(d.set1, 0);
    alpha(lwu(d.alpha4), 0x48);
    uv_scroll_meshes(d.set1, 1);
    sub_002a0ba8(6, effect_tex0(0x3d));
    alpha(lwu(d.alpha5), 0x48);
    uv_scroll_meshes(d.set2, 0);
}
}  // namespace

// orig: L8:0x002f0dc8
void class600_draw(uint32_t) {
    layers_draw({0x44, L8_ADDR(0x00161dd0), L8_ADDR(0x00161dd4), L8_ADDR(0x00161dd8), L8_ADDR(0x00161e08), L8_ADDR(0x00161df0),
                 L8_ADDR(0x00161df4), L8_ADDR(0x00161e00), 8, 0, 1});
}

// orig: L8:0x00309030
void class1649_draw(uint32_t) {
    layers_draw({0x64, L8_ADDR(0x00162390), L8_ADDR(0x00162394), L8_ADDR(0x00162398), L8_ADDR(0x001623f0), L8_ADDR(0x001623b0),
                 L8_ADDR(0x001623b4), L8_ADDR(0x001623c0), 9, 2, 11});
}

// orig: L8:0x002f1378 L8:0x002f1460 L8:0x003095e0 L8:0x003096c8 L11:0x0031e120 L11:0x0031e208 L11:0x0031ef80 L11:0x0031f068
//       L8:0x002f12a0 L8:0x00309508 L11:0x0031e048 L11:0x0031eea8
void uv_scroll_step(int32_t set, uint32_t index) {
    const ScrollSet s = scroll_set(set);
    const uint32_t at = index << 3;
    scroll(s.offsets + at, s.speeds + at);
    scroll(s.offsets + at + 4, s.speeds + at + 4);
}

// orig: L8:0x002f0f68 L8:0x003091d0 L11:0x0031dd10 L11:0x0031eb70 L12:0x0030bba0
void mesh_colours_scale(uint32_t colours, int32_t meshes, uint32_t counts, uint32_t full, float r, float g, float b, float a) {
    const float max = 255.0f;
    for (int32_t i = 0; i < meshes; ++i) {
        const uint32_t at = static_cast<uint32_t>(i) * 4;
        for (int32_t k = 0; k < lw(counts + at); ++k) {
            const uint32_t c = lwu(lwu(colours + at) + static_cast<uint32_t>(k) * 4);
            float rr, gg, bb, aa;
            if (full == 0) {
                rr = fpu_mul(r, int_to_float(static_cast<int32_t>(c & 0xff)));
                gg = fpu_mul(g, int_to_float(static_cast<int32_t>((c & 0xff00) >> 8)));
                bb = fpu_mul(b, int_to_float(static_cast<int32_t>((c >> 16) & 0xff)));
                aa = fpu_mul(a, int_to_float(static_cast<int32_t>(c >> 24)));
            } else {
                rr = fpu_mul(r, max);
                aa = fpu_mul(a, int_to_float(static_cast<int32_t>(c >> 24)));
                gg = fpu_mul(g, max);
                bb = fpu_mul(b, max);
            }
            const uint32_t na = static_cast<uint32_t>(float_to_int(aa));
            const uint32_t nb = static_cast<uint32_t>(float_to_int(bb));
            const uint32_t ng = static_cast<uint32_t>(float_to_int(gg));
            const uint32_t nr = static_cast<uint32_t>(float_to_int(rr));
            sw(lwu(colours + at) + static_cast<uint32_t>(k) * 4, (na << 24) + (nb << 16) + (ng << 8) + nr);
        }
    }
}

// orig: L8:0x002f1158 L8:0x003093c0 L11:0x0031df00 L11:0x0031ed60
void wave_step(uint32_t out, uint32_t phase, float su, float sv, float amp) {
    wave_phase(phase, fpu_mul(su, frame_dt()));
    wave_phase(phase + 4, fpu_mul(sv, frame_dt()));
    const float pi = u2f(0x40490fdbu);
    swc1(out, fpu_mul(sin_vu(fpu_mul(lwc1(phase), pi)), amp));
    swc1(out + 4, fpu_mul(sin_vu(fpu_mul(lwc1(phase + 4), pi)), amp));
}

}  // namespace rac1
