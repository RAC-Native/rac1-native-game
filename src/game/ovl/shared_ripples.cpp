// The rippling surfaces: one port of the copies Quartu and Drek's fleet carry of their update (eight classes, the same code
// relinked with the class's own draw) and of two of their draws (the same code relinked, Drek's fleet's with fewer meshes or
// points: those counts are per level here, as are the levels' own tables below). The frame counter 0x15f5cc is the
// engine's (OV), the frame time 0x15ed6c the base ELF's; the VU1 set-up is Veldin's (shadows_vu1_setup, 0x1fc740: the
// levels' copies of it are the same code). Header shared_ripples.h.
//
// The pvars: +0 the last frame's phase, +4 the phase.
#include <game/ovl/shared_ripples.h>

#include <game/math/vecmath.h>
#include <game/ovl/nanotech_draw.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shadows.h>
#include <game/ovl/variants.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/variants_l17.h>

namespace rac1 {

namespace {
// A surface's tuning words: +0 the blend's alpha word, +4 the texture, +8 the colour's alpha, +0xc the ripples' spacing,
// +0x10 their period (frames), +0x14 their height, +0x18 the scroll's speed; and the scroll's four rates (two halves: s, t),
// after them (Drek's fleet's rings: a word further on).
struct Surface {
    uint32_t words, rates;
    int32_t meshes;  // (the rings: the meshes in the tables below; the pool: its points)
    // the rings' per-mesh tables (a word each): their points' counts, the drawn points, the drawn (s, t), the colours, the
    // points at rest, the (s, t) at rest; the pool's own arrays the same (no counts) and its prim list's four words
    uint32_t counts, outs, uv_outs, colours, points, uvs;
};

Surface rings() {
    if (on_level(VELDIN_ORBIT))
        return {L17_ADDR(0x00162248), L17_ADDR(0x00162268), 2, L17_ADDR(0x00162210), L17_ADDR(0x00162218), L17_ADDR(0x00162220), L17_ADDR(0x00162228),
                L17_ADDR(0x00162230), L17_ADDR(0x00162238)};
    return {L15_ADDR(0x001620ec), L15_ADDR(0x00162108), 3, L15_ADDR(0x00162080), L15_ADDR(0x00162090), L15_ADDR(0x001620a0), L15_ADDR(0x001620b0),
            L15_ADDR(0x001620c0), L15_ADDR(0x001620d0)};
}

Surface pool() {
    if (on_level(VELDIN_ORBIT))
        return {L17_ADDR(0x00162374), L17_ADDR(0x00162390), 45, L17_ADDR(0x00162340), L17_ADDR(0x001ddba0), L17_ADDR(0x001dddc0), L17_ADDR(0x001ddf28),
                L17_ADDR(0x001dd760), L17_ADDR(0x001dd980)};
    return {L15_ADDR(0x00162154), L15_ADDR(0x00162170), 68, L15_ADDR(0x00162120), L15_ADDR(0x001de200), L15_ADDR(0x001de530), L15_ADDR(0x001de750),
            L15_ADDR(0x001ddba0), L15_ADDR(0x001dded0)};
}

// The GS state, the phase stepped (one turn every `+0x10` frames, scaled), and the colour (the moby's, the surface's alpha);
// returns the scroll's time (the frame counter by the speed and the frame time).
float begin(uint32_t m, uint32_t w, uint32_t* colour) {
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t sp = ee_stack_alloc(0x20);
    const float scroll = fpu_mul(int_to_float(lw(OV(0x0015f5cc))), fpu_mul(lwc1(w + 0x18), lwc1(0x0015ed6c)));
    sub_002a0ba8(6, effect_tex0(lw(w + 4)));
    sub_002a0ba8(0x42, static_cast<uint64_t>(lwu(w)) << 32 | 0x44);
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x14, 0x0000ff9000000260ull);
    swc1(pv, lwc1(pv + 4));
    const float turn = fpu_mul(fpu_mul(fpu_div(360.0f, frame_scale_f(lwc1(w + 0x10))), u2f(0x3c8efa35u)), lwc1(0x0015ed6c));
    swc1(pv + 4, angle_add(lwc1(pv + 4), turn));
    moby_get_color_hi(m, sp, sp + 4, sp + 8);
    *colour = lwu(w + 8) << 24 | lwu(sp + 8) << 16 | lwu(sp + 4) << 8 | lwu(sp);
    ee_stack_free(0x20);
    return scroll;
}

// One point of a surface: at rest `p` (relative), drawn at `out` about the moby, raised by the ripple at its distance from
// the centre
void ripple_point(uint32_t m, uint32_t w, uint32_t p, uint32_t out_x, uint32_t out_y, uint32_t out_z) {
    const uint32_t pv = lwu(m + 0x78);
    const float r = sqrt_f(fpu_add(fpu_mul(lwc1(p), lwc1(p)), fpu_mul(lwc1(p + 4), lwc1(p + 4))));
    const float a = angle_wrap(fpu_div(fpu_mul(float_fmod_vu(r, lwc1(w + 0xc)), u2f(0x40c90fdbu)), lwc1(w + 0xc)));  // (2 pi)
    const float s = sin_vu(angle_add(a, lwc1(pv + 4)));
    swc1(out_x, fpu_add(lwc1(p), lwc1(m + 0x10)));
    const float height = fpu_mul(lwc1(w + 0x14), s);
    swc1(out_y, fpu_add(lwc1(p + 4), lwc1(m + 0x14)));
    swc1(out_z, fpu_add(fpu_add(lwc1(p + 8), lwc1(m + 0x18)), height));
}

// one (s, t) of a surface scrolled by the half `half`'s rates (wrapped to 0..1)
void scroll_uv(uint32_t all_rates, int32_t half, float time, uint32_t at_rest, uint32_t out) {
    const uint32_t rates = all_rates + static_cast<uint32_t>(half) * 8;
    swc1(out, fpu_add(lwc1(at_rest), float_fmod_vu(fpu_mul(time, lwc1(rates)), 1.0f)));
    swc1(out + 4, fpu_add(lwc1(at_rest + 4), float_fmod_vu(fpu_mul(time, lwc1(rates + 4)), 1.0f)));
}
}  // namespace

// State 0 tints the moby (0x40 grey) and starts; then each frame its draw is queued, its +0x32 cleared and its +0x34 bit 0
// set.
// orig: L15:0x002eb0a0 L15:0x002eb458 L15:0x002edf58 L15:0x002ee338 L17:0x002f08e0 L17:0x002f0ca0 L17:0x002f1060 L17:0x002f1418
void ripple_update(uint32_t m, uint32_t draw) {
    if (lbu(m + 0x20) == 0) {
        moby_set_color_hi(m, 0x40, 0x40, 0x40);
        sb(m + 0x20, 1);
        return;
    }
    draw_callback_add(draw, m);
    sh(m + 0x32, 0);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
}

void class1408_update(uint32_t m) { ripple_update(m, L15_ADDR(0x002eac90)); }
void class1409_update(uint32_t m) { ripple_update(m, L15_ADDR(0x002eb108)); }
void class1565_update(uint32_t m) { ripple_update(m, L15_ADDR(0x002edbe0)); }
void class1567_update(uint32_t m) { ripple_update(m, L15_ADDR(0x002edfc0)); }
void class1405_update(uint32_t m) { ripple_update(m, L17_ADDR(0x002f04d0)); }
void class1406_update(uint32_t m) { ripple_update(m, L17_ADDR(0x002f0948)); }
void class1407_update(uint32_t m) { ripple_update(m, L17_ADDR(0x002f0d08)); }
void class1426_update(uint32_t m) { ripple_update(m, L17_ADDR(0x002f10c8)); }

// The rings: each mesh's points rippled about the moby in the surface's colour, then twice (the scroll's two halves) their
// (s, t) scrolled and the meshes drawn as prim lists.
// orig: L15:0x002eac90 L17:0x002f04d0
void ripple_rings_draw(uint32_t m) {
    const Surface s = rings();
    uint32_t colour = 0;
    const float time = begin(m, s.words, &colour);
    for (int32_t i = 0; i < s.meshes; ++i) {
        const uint32_t at = static_cast<uint32_t>(i) * 4;
        for (int32_t j = 0; j < lw(s.counts + at); ++j) {
            const uint32_t p = lwu(s.points + at) + static_cast<uint32_t>(j) * 12;
            const uint32_t o = static_cast<uint32_t>(j) * 12;
            ripple_point(m, s.words, p, lwu(s.outs + at) + o, lwu(s.outs + at) + o + 4, lwu(s.outs + at) + o + 8);
            sw(lwu(s.colours + at) + static_cast<uint32_t>(j) * 4, colour);
        }
    }
    for (int32_t half = 0; half < 2; ++half) {
        for (int32_t i = 0; i < s.meshes; ++i) {
            const uint32_t at = static_cast<uint32_t>(i) * 4;
            for (int32_t j = 0; j < lw(s.counts + at); ++j) {
                const uint32_t o = static_cast<uint32_t>(j) * 8;
                scroll_uv(s.rates, half, time, lwu(s.uvs + at) + o, lwu(s.uv_outs + at) + o);
            }
        }
        shadows_vu1_setup();
        for (int32_t i = 0; i < s.meshes; ++i) {
            const uint32_t at = static_cast<uint32_t>(i) * 4;
            prim_list_packet(lw(s.counts + at), lwu(s.outs + at), lwu(s.colours + at), lwu(s.uv_outs + at), 1);
        }
    }
}

// The pool: one mesh of the level's count of points (its arrays the level's own), drawn as the rings are; its prim list's
// words are the four after the count (+0x20 the points, +0x28 the colours, +0x30 the (s, t)).
// orig: L15:0x002eb108 L17:0x002f10c8
void ripple_pool_draw(uint32_t m) {
    const Surface s = pool();
    uint32_t colour = 0;
    const float time = begin(m, s.words, &colour);
    for (int32_t j = 0; j < s.meshes; ++j) {
        const uint32_t o = static_cast<uint32_t>(j) * 12;
        ripple_point(m, s.words, s.points + o, s.outs + o, s.outs + o + 4, s.outs + o + 8);
        sw(s.colours + static_cast<uint32_t>(j) * 4, colour);
    }
    for (int32_t half = 0; half < 2; ++half) {
        for (int32_t j = 0; j < s.meshes; ++j) {
            const uint32_t o = static_cast<uint32_t>(j) * 8;
            scroll_uv(s.rates, half, time, s.uvs + o, s.uv_outs + o);
        }
        shadows_vu1_setup();
        prim_list_packet(lw(s.counts), lwu(s.counts + 0x20), lwu(s.counts + 0x30), lwu(s.counts + 0x28), 1);
    }
}

}  // namespace rac1
