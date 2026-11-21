// Class 78's draw callback: one port of the copies Quartu and Drek's fleet carry (the same code relinked). Each level keeps
// its own words (the blends' selectors, the layers' colours, the sway's periods and kinds, the corners' texture
// coordinates, the two quads' corners: per level below); the frame counter is the engine's (OV 0x15f3f8). Header
// shared_class78_draw.h.
#include <game/ovl/shared_class78_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// the level's blend selectors: four tables of two words (the even layers' and the odd layers')
uint32_t selectors() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x00161478) : L15_ADDR(0x001614d8); }
// the level's table block: +0 the six layers' colours, +0x20 their sway periods (two floats a layer: s and t), +0x60
// whether each sways by a cosine (else a sine), +0xa0 the four corners' (s, t), +0xc0 the second quad's corners, +0x100
// the first quad's
uint32_t tables() { return on_level(VELDIN_ORBIT) ? L17_ADDR(0x001ce4b0) : L15_ADDR(0x001ce230); }
}  // namespace

// Two quads (the second's two far corners transparent) in the moby's frame, drawn six times, each layer 0.1 further along x
// with its own colour (faded by pvar +0x14 frames out of 60 when set) and blend, and its corners' (s, t) swaying: the
// frame counter modulo the layer's period (times 360) as degrees, less 180, through a sine or a cosine, an eighth of it.
// The second quad's (s, t) are the first's last two corners', its far ones an eighth on.
// orig: L15:0x002a3138 L17:0x002a8808
void class78_draw(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x250);
    const uint32_t a = sp, b = sp + 0x90, frame = sp + 0x120;
    mat3_copy_affine(M(frame), M(m + 0xc0));
    lq_sq(frame + 0x30, m + 0x10);
    swc1(frame + 0x3c, 1.0f);
    const uint64_t tex_a = effect_tex0(0x2c);
    sd(a + 0x78, tex_a);
    sd(a + 0x80, 0x0000ff9000000260ull);
    sd(a + 0x70, 0);
    const uint64_t tex_b = effect_tex0(0x2c);
    sd(b + 0x78, tex_b);
    sd(b + 0x80, 0x0000ff9000000260ull);
    sd(b + 0x70, 0);
    sw(b + 0x4c, 0);
    sw(b + 0x44, 0);
    const uint32_t t = tables();
    for (uint32_t k = 0; k < 4; ++k) {
        lq_sq(a + k * 0x10, t + 0x100 + k * 0x10);
        lq_sq(b + k * 0x10, t + 0xc0 + k * 0x10);
    }
    const uint32_t sel = selectors();
    const float deg_to_rad = u2f(0x3c8efa35u);
    for (int32_t layer = 0; layer < 6; ++layer) {
        const uint32_t odd = static_cast<uint32_t>(layer & 1) << 2;
        // (the selectors loaded sign-extended and shifted as doublewords)
        auto word = [sel, odd](uint32_t o) { return static_cast<uint64_t>(static_cast<int64_t>(lw(sel + o + odd))); };
        const uint64_t blend = word(0) | word(8) << 2 | word(0x10) << 4 | word(0x18) << 6 | 0x8000000000ull;
        sd(a + 0x88, blend);
        sd(b + 0x88, blend);
        const uint32_t base = lwu(t + static_cast<uint32_t>(layer) * 4);
        uint32_t colour = base;
        const int32_t fade = lw(pv + 0x14);
        if (fade != 0) colour = rgba_lerp(base, 0, fpu_div(static_cast<float>(fade), 60.0f));
        for (uint32_t k = 0; k < 4; ++k) sw(a + 0x40 + k * 4, colour);
        sw(b + 0x40, colour);
        sw(b + 0x48, colour);
        for (uint32_t k = 0; k < 4; ++k) {
            for (uint32_t j = 0; j < 2; ++j) {
                const uint32_t lj = static_cast<uint32_t>(layer) * 8 + j * 4;
                const float period = lwc1(t + 0x20 + lj);
                const int32_t n = float_to_int(fpu_mul(period, 360.0f));
                const int32_t at = ee_rem(lw(OV(0x0015f3f8)), n);
                const float deg = fpu_sub(fpu_div(int_to_float(at), lwc1(t + 0x20 + lj)), 180.0f);
                const float sway = lw(t + 0x60 + lj) != 0 ? fpu_mul(cos_vu(fpu_mul(deg, deg_to_rad)), 0.125f)
                                                          : fpu_mul(sin_vu(fpu_mul(deg, deg_to_rad)), 0.125f);
                swc1(a + 0x50 + k * 8 + j * 4, fpu_add(lwc1(t + 0xa0 + k * 8 + j * 4), sway));
            }
        }
        for (uint32_t j = 0; j < 2; ++j) {
            const float step = fpu_mul(static_cast<float>(j), 0.125f);
            swc1(b + 0x50 + j * 4, lwc1(a + 0x58 + j * 4));
            swc1(b + 0x58 + j * 4, fpu_add(lwc1(a + 0x58 + j * 4), step));
            swc1(b + 0x60 + j * 4, lwc1(a + 0x68 + j * 4));
            swc1(b + 0x68 + j * 4, fpu_add(lwc1(a + 0x68 + j * 4), step));
        }
        quad_draw(a, frame);
        quad_draw(b, frame);
        for (uint32_t k = 0; k < 4; ++k) {
            swc1(a + k * 0x10, fpu_add(lwc1(a + k * 0x10), 0.1f));
            swc1(b + k * 0x10, fpu_add(lwc1(b + k * 0x10), 0.1f));
        }
    }
    ee_stack_free(0x250);
}

}  // namespace rac1
