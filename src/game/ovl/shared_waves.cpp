// The rippling water meshes' draw callbacks: one port of each build, serving its copies (each copy the same code on its
// own mesh and words; compared with the relocated fields masked, and normalised: the differences are the mesh's tables
// and point count, the parameters here). The engine's words: the frame counter 0x15f5cc (OV); the base ELF's: the frame
// time 0x15ed6c. The meshes' VU1 set-up is each level's copy of Veldin's 0x1fc740 (the overlay maps pair them): the Veldin
// port. Header shared_waves.h.
//
// The moby's pvars: the ripple's phase +4 (its last +0).
#include <game/ovl/shared_waves.h>

#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {

// A ripple mesh's words and tables. The words (`w`): +0 the prim list's count, +0x20 its points, +0x28 its texture
// coordinates, +0x30 its colours; +0x34 the blend's fix, +0x38 its effect texture, +0x3c its alpha; +0x40 the ripples'
// spacing, +0x44 their period (seconds), +0x48 their height, +0x4c the texture's drift rate, +0x50 the mesh's scale; +0x58
// two pairs of texture drift factors (u, v).
struct Mesh {
    uint32_t w;       // the words
    uint32_t points;  // the mesh's points (12 bytes each, about its centre)
    uint32_t out;     // the points drawn (12 bytes each)
    uint32_t colours; // their colours (words)
    uint32_t uv;      // the texture coordinates (pairs)
    uint32_t uv_out;  // the texture coordinates drawn
    int32_t n;        // the points (the first build's; the second reads the count at +0)
};

// The mesh at the moby (scaled), each point raised by the ripple at its distance from the centre (the phase turning a
// full turn in the period), coloured the moby's colour with the alpha; then drawn twice, each with its texture coordinates
// drifted by the frame counter (by one of the two pairs of factors).
void waves_draw(uint32_t m, const Mesh& g) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t w = g.w;
    const float frames = int_to_float(lw(OV(0x0015f5cc)));
    const float drift = fpu_mul(frames, fpu_mul(lwc1(w + 0x4c), lwc1(0x0015ed6c)));
    sub_002a0ba8(6, effect_tex0(lw(w + 0x38)));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(static_cast<int64_t>(lw(w + 0x34))) << 32) | 0x44u);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, 0x0000ff9000000260ull);
    swc1(pv, lwc1(pv + 4));
    {
        const float period = frame_scale_f(lwc1(w + 0x44));
        const float step = fpu_mul(fpu_mul(fpu_div(360.0f, period), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));
        swc1(pv + 4, angle_add(lwc1(pv + 4), step));
    }
    moby_get_color_hi(m, sp + 0x10, sp + 0x14, sp + 0x18);
    const uint32_t colour = (lwu(w + 0x3c) << 24) | (lwu(sp + 0x18) << 16) | (lwu(sp + 0x14) << 8) | lwu(sp + 0x10);
    const float turn = u2f(0x40c90fdbu);
    for (int32_t i = 0; i < g.n; ++i) {
        const uint32_t p = g.points + static_cast<uint32_t>(i) * 0xc, o = g.out + static_cast<uint32_t>(i) * 0xc;
        const float x = lwc1(p), y = lwc1(p + 4);
        const float r = sqrt_f(fpu_add(fpu_mul(x, x), fpu_mul(y, y)));
        const float a = angle_wrap(fpu_div(fpu_mul(float_fmod_vu(r, lwc1(w + 0x40)), turn), lwc1(w + 0x40)));
        const float s = sin_vu(angle_add(a, lwc1(pv + 4)));
        const float k = lwc1(w + 0x50);
        swc1(o, fpu_add(fpu_mul(k, lwc1(p)), lwc1(m + 0x10)));
        swc1(o + 4, fpu_add(fpu_mul(k, lwc1(p + 4)), lwc1(m + 0x14)));
        const float rise = fpu_mul(lwc1(w + 0x48), s);
        swc1(o + 8, fpu_add(fpu_add(fpu_mul(k, lwc1(p + 8)), lwc1(m + 0x18)), rise));
        sw(g.colours + static_cast<uint32_t>(i) * 4, colour);
    }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        const uint32_t f = w + 0x58 + pass * 8;
        for (int32_t j = 0; j < g.n; ++j) {
            const uint32_t s = g.uv + static_cast<uint32_t>(j) * 8, d = g.uv_out + static_cast<uint32_t>(j) * 8;
            swc1(d, fpu_add(lwc1(s), float_fmod_vu(fpu_mul(drift, lwc1(f)), 1.0f)));
            swc1(d + 4, fpu_add(lwc1(s + 4), float_fmod_vu(fpu_mul(drift, lwc1(f + 4)), 1.0f)));
        }
        shadows_vu1_setup();
        prim_list_packet(lw(w), lwu(w + 0x20), lwu(w + 0x30), lwu(w + 0x28), 1);
    }
    ee_stack_free(0x80);
}

// The second build (Drek's fleet's): its point count the prim list's (+0), the mesh not scaled, the drift factors at +0x50.
void waves_draw_fleet(uint32_t m, const Mesh& g) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t w = g.w;
    const float frames = int_to_float(lw(OV(0x0015f5cc)));
    const float drift = fpu_mul(frames, fpu_mul(lwc1(w + 0x4c), lwc1(0x0015ed6c)));
    sub_002a0ba8(6, effect_tex0(lw(w + 0x38)));
    sub_002a0ba8(0x42, (static_cast<uint64_t>(static_cast<int64_t>(lw(w + 0x34))) << 32) | 0x44u);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, 0x0000ff9000000260ull);
    swc1(pv, lwc1(pv + 4));
    {
        const float period = frame_scale_f(lwc1(w + 0x44));
        const float step = fpu_mul(fpu_mul(fpu_div(360.0f, period), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));
        swc1(pv + 4, angle_add(lwc1(pv + 4), step));
    }
    moby_get_color_hi(m, sp + 0x10, sp + 0x14, sp + 0x18);
    const uint32_t colour = (lwu(w + 0x3c) << 24) | (lwu(sp + 0x18) << 16) | (lwu(sp + 0x14) << 8) | lwu(sp + 0x10);
    const float turn = u2f(0x40c90fdbu);
    for (int32_t i = 0; i < lw(w); ++i) {
        const uint32_t p = g.points + static_cast<uint32_t>(i) * 0xc, o = g.out + static_cast<uint32_t>(i) * 0xc;
        const float x = lwc1(p), y = lwc1(p + 4);
        const float r = sqrt_f(fpu_add(fpu_mul(x, x), fpu_mul(y, y)));
        const float a = angle_wrap(fpu_div(fpu_mul(float_fmod_vu(r, lwc1(w + 0x40)), turn), lwc1(w + 0x40)));
        const float s = sin_vu(angle_add(a, lwc1(pv + 4)));
        swc1(o, fpu_add(lwc1(p), lwc1(m + 0x10)));
        const float rise = fpu_mul(lwc1(w + 0x48), s);
        swc1(o + 4, fpu_add(lwc1(p + 4), lwc1(m + 0x14)));
        sw(g.colours + static_cast<uint32_t>(i) * 4, colour);
        swc1(o + 8, fpu_add(fpu_add(lwc1(p + 8), lwc1(m + 0x18)), rise));
    }
    for (uint32_t pass = 0; pass < 2; ++pass) {
        const uint32_t f = w + 0x50 + pass * 8;
        for (int32_t j = 0; j < lw(w); ++j) {
            const uint32_t s = g.uv + static_cast<uint32_t>(j) * 8, d = g.uv_out + static_cast<uint32_t>(j) * 8;
            swc1(d, fpu_add(lwc1(s), float_fmod_vu(fpu_mul(drift, lwc1(f)), 1.0f)));
            swc1(d + 4, fpu_add(lwc1(s + 4), float_fmod_vu(fpu_mul(drift, lwc1(f + 4)), 1.0f)));
        }
        shadows_vu1_setup();
        prim_list_packet(lw(w), lwu(w + 0x20), lwu(w + 0x30), lwu(w + 0x28), 1);
    }
    ee_stack_free(0x80);
}

}  // namespace

// orig: L15:0x002edbe0
void quartu_waves_draw_a(uint32_t m) {
    waves_draw(m, Mesh{L15_ADDR(0x001621b0), L15_ADDR(0x001de860), L15_ADDR(0x001decb0), L15_ADDR(0x001df048), L15_ADDR(0x001dea88),
                       L15_ADDR(0x001deed8), 46});
}

// orig: L15:0x002edfc0
void quartu_waves_draw_b(uint32_t m) {
    waves_draw(m, Mesh{L15_ADDR(0x00162220), L15_ADDR(0x001df100), L15_ADDR(0x001df940), L15_ADDR(0x001e0020), L15_ADDR(0x001df520),
                       L15_ADDR(0x001dfd60), 88});
}

// orig: L17:0x002f0948
void fleet_waves_draw_a(uint32_t m) {
    waves_draw_fleet(m, Mesh{L17_ADDR(0x00162280), L17_ADDR(0x001dbe20), L17_ADDR(0x001dc680), L17_ADDR(0x001dcd78), L17_ADDR(0x001dc250),
                             L17_ADDR(0x001dcab0), 0});
}

// orig: L17:0x002f0d08
void fleet_waves_draw_b(uint32_t m) {
    waves_draw_fleet(m, Mesh{L17_ADDR(0x001622e0), L17_ADDR(0x001dcee0), L17_ADDR(0x001dd320), L17_ADDR(0x001dd6a8), L17_ADDR(0x001dd100),
                             L17_ADDR(0x001dd540), 0});
}

}  // namespace rac1
