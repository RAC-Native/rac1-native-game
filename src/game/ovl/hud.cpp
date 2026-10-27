// Ported overlay code: the HUD. 13 element slots of 0x90 bytes at 0x17dc50 (requests 0x235700,
// kept alive 0x235e18), each with its update (+0x14) and draw (+0x18) functions. The HUD data
// (0x17e4d8: +0x18 its header, +0x1c the animations, +0x20 the frames (palette, texture), +0x24
// the textures, +0x28 the palettes) comes from the level; its textures go into the GS memory on
// demand (the upload queue 0x16cd80). Sprites and text are GS packets (REGLIST) in the frame
// packet; positions in 1/16 pixels from the screen area origin 0x13e510 / 0x13e514.
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libgraph.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <game/render/mrp_lod.h>
#include <game/render/render_view.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001611c0u> CUR{};
constexpr OvConst<0x0017e4d8u> H{};
// a GS XYZ2 at pixel (x, y): 1/16 pixels from the screen origin, the pixel's centre (-8), z
// orig: (helper) repeated in 0x0023b4d8 / 0x0023b860 / 0x001fa290
uint64_t gs_xyz(int32_t x, int32_t y, uint64_t z) {
    const int32_t gx = x * 16 + lw(0x0013e510) - 8, gy = y * 16 + lw(0x0013e514) - 8;
    return static_cast<uint64_t>(static_cast<int64_t>(gx)) | (static_cast<uint64_t>(static_cast<int64_t>(gy)) << 16) | z;
}
// orig: (helper) repeated in 0x0023b4d8 / 0x0023b860 / 0x001fa290
void cnt_direct(uint32_t p, uint32_t qwc) {
    sw(p, 0x10000000u | qwc);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000000u | qwc);
}
// orig: (host) the side of the frame the HUD element in slot `e` is anchored to, for a HUD wider than 4:3 (render_view_hud_note):
// its layout flags (+0x60, the slot's own: hud_element_place aligns it left of x with 4, right of x with 8, centres it on x
// without; hud_element_offset slides it out toward the same side): 1 left, 2 right, 0 centred (not moved). Read only.
// The slots (the same layout on every level): 0 x 20 top left (the weapon's ammo, hero mode 2's bar, a race's), 1 x 256 top
// centre (health, Gemlik's boss), 2 x 492 top right (bolts), 3 x 20 middle left (the quick select, Gadgebot's commands),
// 4 x 492 middle right (the air meter, the Morph-o-Ray), 5 x 20 bottom left, 6 x 256 bottom centre, 7 x 492 bottom right
// (the Metal Detector; a race's, Skid's), 8..11 x 492 middle right, 12 x 142 flags 0 (the help panel: centred)
uint8_t hud_element_side(uint32_t e) {
    const uint32_t f = lwu(e + 0x60);
    return (f & 4) != 0 ? 1 : (f & 8) != 0 ? 2 : 0;
}
}  // namespace

// The HUD frame `i` of animation `key` (its first frame + i): 0 when the animation is missing,
// i is past its count, or its palette or texture is not loaded (negative / bit 31).
// orig: 0x0023b208
int32_t hud_frame(uint32_t key, int32_t i) {
    const int32_t k = sub_00234e48(key);
    const uint32_t e = lwu(H + 0x1c) + static_cast<uint32_t>(k) * 8;
    if (lhu(e) == 0xffff) return 0;
    if (!(i < static_cast<int32_t>(lhu(e + 2)))) return 0;
    const int32_t idx = static_cast<int32_t>(lhu(e + 4)) + i;
    const uint32_t f = lwu(H + 0x20) + static_cast<uint32_t>(idx) * 4;
    if (lw(lwu(H + 0x28) + static_cast<uint32_t>(lh(f)) * 8) < 0) return 0;
    if ((lwu(lwu(H + 0x24) + static_cast<uint32_t>(lh(f + 2)) * 8) & 0x80000000u) != 0) return 0;
    return idx;
}

// The TEX0 of HUD frame `idx`: its palette and texture (8 bytes each: address +0, GS block +4,
// log2 width / height +6 / +7) made resident in the GS memory (blocks from 0x15ee74, queued for
// upload, at most 0x40 a frame; a missing pair queues a placeholder); textures below 0x15ee8c
// are the level's resident ones (PSMT8H).
// orig: 0x0023b2b8
uint64_t hud_tex0(int32_t idx) {
    const uint32_t f = lwu(H + 0x20) + static_cast<uint32_t>(idx) * 4;
    const uint32_t pal = lwu(H + 0x28) + static_cast<uint32_t>(lh(f)) * 8;
    const uint32_t tex = lwu(H + 0x24) + static_cast<uint32_t>(lh(f + 2)) * 8;
    bool queued = false;
    if (lhu(pal + 4) == 0 || lhu(tex + 4) == 0) {
        const uint32_t q = OV(0x0016cd80) + lwu(OV(0x0015f418)) * 0x10;
        sw(q, lwu(pal));
        sh(q + 4, 0);
        sh(q + 6, 0x3ff0);
        sw(q + 8, lwu(pal));
        sh(q + 0xe, 0x3ff0);
        sb(q + 0xd, 5);
        sb(q + 0xc, 5);
    }
    if (lhu(pal + 4) == 0) {
        const int32_t a = lw(0x0015ee74);
        const int32_t n = lw(OV(0x0015f418));
        sh(pal + 4, static_cast<uint32_t>(a >> 8));
        sw(0x0015ee74, static_cast<uint32_t>(a + 0x400));
        if (n < 0x40) {
            const uint32_t q = OV(0x0016cd80) + static_cast<uint32_t>(n) * 0x10;
            queued = true;
            sw(q, lwu(pal));
            sh(q + 4, 0);
            sh(q + 6, lhu(pal + 4));
        }
    }
    if (lhu(tex + 4) == 0) {
        const int32_t a = lw(0x0015ee74);
        uint32_t big = lbu(tex + 7);
        if (big < lbu(tex + 6)) big = lbu(tex + 6);
        sh(tex + 4, static_cast<uint32_t>(a >> 8));
        sw(0x0015ee74, static_cast<uint32_t>(a + static_cast<int32_t>(1u << ((big * 2) & 31))));
        const int32_t n = lw(OV(0x0015f418));
        if (n < 0x40) {
            const uint32_t q = OV(0x0016cd80) + static_cast<uint32_t>(n) * 0x10;
            queued = true;
            sw(q + 8, lwu(tex));
            sb(q + 0xc, lbu(tex + 6));
            sb(q + 0xd, lbu(tex + 7));
            sh(q + 0xe, lhu(tex + 4));
        }
    }
    if (queued) sw(OV(0x0015f418), lwu(OV(0x0015f418)) + 1);
    int32_t bw = static_cast<int32_t>(lbu(tex + 6)) - 6;
    if (!(-1 < bw)) bw = 0;
    const uint64_t psm = static_cast<int32_t>(lhu(tex + 4)) < (lw(0x0015ee8c) >> 8) ? 0x1b : 0x13;
    return static_cast<uint64_t>(lhu(tex + 4)) | (static_cast<uint64_t>(1u << bw) << 14) | (psm << 20) | (static_cast<uint64_t>(lbu(tex + 6)) << 26) |
           (static_cast<uint64_t>(lbu(tex + 7)) << 30) | (static_cast<uint64_t>(lhu(pal + 4)) << 37) | (1ull << 34) | (1ull << 63);
}

// A HUD sprite of frame `idx` at (x, y), w x h pixels, the whole texture, alpha `a` (grey 0x7f).
// orig: 0x0023b4d8
void hud_sprite(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a) {
    const uint32_t tex = lwu(H + 0x24) + static_cast<uint32_t>(lh(lwu(H + 0x20) + static_cast<uint32_t>(idx) * 4 + 2)) * 8;
    const uint32_t th = 1u << (lbu(tex + 7) & 31), tw = 1u << (lbu(tex + 6) & 31);
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 5);
    const uint32_t g = p + 0x10;
    sw(CUR, g);
    sd(g, 0x7400000000008001ull);
    sd(g + 8, 0x5353106);
    const uint64_t tex0 = hud_tex0(idx);
    const uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(lw(H + 0xc))) << 32;
    sd(g + 0x10, tex0);
    sd(g + 0x20, (a << 24) | 0x7f7f7f);
    sd(g + 0x18, 0x156);
    sd(g + 0x28, 0);
    sd(g + 0x38, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((th << 20) + (tw << 4)))));
    sd(g + 0x30, gs_xyz(x, y, z));
    sd(g + 0x48, 0);
    sd(g + 0x40, gs_xyz(x + w, y + h, z));
    sw(CUR, lwu(CUR) + 0x50);
}

// A HUD sprite of frame `idx` at (x, y), w x h pixels of its texture one to one (the texture
// coordinates w, h), alpha `a` (grey 0x7f).
// orig: 0x0023b6c0
void hud_sprite_texels(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a) {
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 5);
    const uint32_t g = p + 0x10;
    sw(CUR, g);
    sd(g, 0x7400000000008001ull);
    sd(g + 8, 0x5353106);
    const uint64_t tex0 = hud_tex0(idx);
    const uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(lw(H + 0xc))) << 32;
    sd(g + 0x10, tex0);
    sd(g + 0x20, (a << 24) | 0x7f7f7f);
    sd(g + 0x18, 0x156);
    sd(g + 0x28, 0);
    const uint32_t uv = (static_cast<uint32_t>(h) << 20) + (static_cast<uint32_t>(w) << 4);
    sd(g + 0x30, gs_xyz(x, y, z));
    sd(g + 0x38, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(uv))));
    sd(g + 0x48, 0);
    sd(g + 0x40, gs_xyz(x + w, y + h, z));
    sw(CUR, lwu(CUR) + 0x50);
}

// The same turned half a turn (a strip of 4, the texture's corners swapped).
// orig: 0x0023b860
void hud_sprite_flip(int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a) {
    const uint32_t tex = lwu(H + 0x24) + static_cast<uint32_t>(lh(lwu(H + 0x20) + static_cast<uint32_t>(idx) * 4 + 2)) * 8;
    const uint32_t th = 1u << (lbu(tex + 7) & 31), tw = 1u << (lbu(tex + 6) & 31);
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 7);
    const uint32_t g = p + 0x10;
    sw(CUR, g);
    sd(g, 0xb400000000008001ull);
    sd(g + 8, 0x53535353106ull);
    const uint64_t tex0 = hud_tex0(idx);
    const uint64_t z = static_cast<uint64_t>(static_cast<int64_t>(lw(H + 0xc))) << 32;
    auto sx = [](uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); };
    sd(g + 0x20, (a << 24) | 0x7f7f7f);
    sd(g + 0x28, sx(tw << 4));
    sd(g + 0x10, tex0);
    sd(g + 0x18, 0x154);
    sd(g + 0x38, sx((th << 20) + (tw << 4)));
    sd(g + 0x30, gs_xyz(x, y + h, z));
    sd(g + 0x48, 0);
    sd(g + 0x40, gs_xyz(x, y, z));
    sd(g + 0x58, sx(th << 20));
    sd(g + 0x50, gs_xyz(x + w, y + h, z));
    sd(g + 0x68, 0);
    sd(g + 0x60, gs_xyz(x + w, y, z));
    sw(CUR, lwu(CUR) + 0x70);
}

// HUD frame `idx` (none when negative) of element `el` at its offset (+0x48 / +0x4a) plus (x,
// y), its texture's size; `flags` 1 centred, 2 half size (centred in place), 4 double size.
// orig: 0x0023b120
void hud_sprite_at(uint32_t el, int32_t idx, int32_t x, int32_t y, uint32_t flags, uint64_t a) {
    if (idx < 0) return;
    y += lh(el + 0x4a);
    x += lh(el + 0x48);
    const uint32_t tex = lwu(H + 0x24) + static_cast<uint32_t>(lh(lwu(H + 0x20) + static_cast<uint32_t>(idx) * 4 + 2)) * 8;
    const int32_t th = 1 << (lbu(tex + 7) & 31), tw = 1 << (lbu(tex + 6) & 31);
    int32_t w = tw, h = th;
    if ((flags & 1) != 0) {
        x -= w >> 1;
        y -= h >> 1;
    }
    if ((flags & 2) != 0) {
        x += w >> 2;
        y += h >> 2;
        w >>= 1;
        h >>= 1;
    }
    if ((flags & 4) != 0) {
        x -= tw >> 1;
        y -= th >> 1;
        w <<= 1;
        h <<= 1;
    }
    hud_sprite(idx, x, y, w, h, a);
}

// A HUD box (animation 0x7580: its end piece, middle and the end turned) from x, w wide.
// orig: 0x0023cea8
void hud_box(int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a) {
    const int32_t mid = hud_frame(0x7580, 0), end = hud_frame(0x7580, 1);
    hud_sprite(end, x, y, 0x20, h, a);
    hud_sprite(mid, x + 0x20, y, w - 0x40, h, a);
    hud_sprite_flip(end, x + w - 0x20, y, 0x20, h, a);
}

// The width of `s` (up to n characters) in the font of the glyph table `t` (4 bytes a
// character: u, v, y offset, advance).
// orig: 0x001fb040
int32_t text_width(uint32_t s, int32_t n, uint32_t t) {
    int32_t w = 0;
    if (n == 0 || lbu(s) == 0) return 0;
    int32_t k = 0;
    while (true) {
        const uint32_t c = lbu(s);
        ++k;
        ++s;
        const int32_t a = static_cast<int8_t>(lbu(t + c * 4 + 3));
        if (a != 0) w += a;
        if (k == n) return w;
        if (lbu(s) == 0) return w;
    }
}
// orig: 0x001fb090 — text_width in the small font 0x1c3150
int32_t text_width_small(uint32_t s, int32_t n) { return text_width(s, n, OV(0x001c3150)); }
// orig: 0x001fb0d0 — text_width in the font 0x1c3890
int32_t text_width_big(uint32_t s, int32_t n) { return text_width(s, n, OV(0x001c3890)); }

// A glyph sprite (a strip of 4): (x, y) w x h pixels from texels (u, v) tw x th, colour,
// TEX0 (the GIF tag of the packet 0x160900).
// orig: 0x001fa290
void glyph_sprite(int32_t x, int32_t y, int32_t w, int32_t h, int32_t u, int32_t v, int32_t tw, int32_t th, uint64_t colour, uint64_t tex0) {
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 7);
    sw(CUR, p + 0x10);
    for (uint32_t k = 0; k < 0x10; k += 4) sw(p + 0x10 + k, lwu(OV(0x00160900) + k));
    sw(CUR, p + 0x20);
    const uint64_t z = 0xfffff000000000ull;
    auto uv = [](int32_t a, int32_t b) { return static_cast<uint64_t>(static_cast<int64_t>((b << 20) + (a << 4))); };
    const uint32_t g = p + 0x20;
    sd(g, tex0);
    sd(g + 8, 0x154);
    sd(g + 0x10, colour);
    sd(g + 0x18, uv(u, v));
    sd(g + 0x20, gs_xyz(x, y, z));
    sd(g + 0x28, uv(u + tw, v));
    sd(g + 0x30, gs_xyz(x + w, y, z));
    sd(g + 0x38, uv(u, v + th));
    sd(g + 0x40, gs_xyz(x, y + h, z));
    sd(g + 0x48, uv(u + tw, v + th));
    sd(g + 0x50, gs_xyz(x + w, y + h, z));
    sd(g + 0x58, 0);
    sw(CUR, lwu(CUR) + 0x60);
}

// The TEX0 of effect texture `i` (0x16d180, 16 bytes each: the TEX0 once made, texels +8 and
// palette +0xa in quadwords from 0x15f420, log2 size +0xc / +0xe): made resident (blocks from
// 0x15ee74) and queued for upload the first time in the frame.
// orig: 0x001f9298
uint64_t effect_tex0(int32_t i) {
    const uint32_t e = OV(0x0016d180) + static_cast<uint32_t>(i) * 0x10;
    if (ld(e) != 0) return ld(e);
    const int32_t wl = lh(e + 0xc), hl = lh(e + 0xe);
    int32_t bw = wl - 6;
    if (!(-1 < bw)) bw = 0;
    int32_t a = lw(0x0015ee74);
    const int32_t pbp = a >> 8;
    a += 0x400;
    const int32_t tbp = a >> 8;
    const uint64_t t = static_cast<uint64_t>(static_cast<int64_t>(tbp)) | (static_cast<uint64_t>(1u << (bw & 31)) << 14) |
                       (static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(lhu(e + 0xc)))) << 26) | 0x1300000ull |
                       (static_cast<uint64_t>(static_cast<int64_t>(static_cast<int16_t>(lhu(e + 0xe)))) << 30) |
                       (static_cast<uint64_t>(static_cast<int64_t>(pbp)) << 37) | (1ull << 34) | (1ull << 63);
    sw(0x0015ee74, static_cast<uint32_t>(a + (1 << ((wl + hl) & 31))));
    sd(e, t);
    const int32_t n = lw(OV(0x0015f418));
    if (n < 0x40) {
        const uint32_t q = OV(0x0016cd80) + static_cast<uint32_t>(n) * 0x10;
        sh(q + 6, static_cast<uint32_t>(pbp));
        sw(q, lwu(OV(0x0015f420)) + lhu(e + 0xa) * 0x10);
        sh(q + 4, 0);
        sw(q + 8, lwu(OV(0x0015f420)) + lhu(e + 8) * 0x10);
        sh(q + 0xe, static_cast<uint32_t>(tbp));
        sb(q + 0xc, lbu(e + 0xc));
        sb(q + 0xd, lbu(e + 0xe));
        sw(OV(0x0015f418), static_cast<uint32_t>(n + 1));
    }
    return ld(e);
}

// Text `s` (up to n characters) at (x, y) in the font of table `t` (TEX0 `tex0`): characters 8
// .. 15 switch to colour 0x16c838[c - 8] (with 0x15f45c), 0x80..0xa7 get an accent glyph, those
// below 0x20 a wide grey one (24 x 16); the colour is kept in 0x16c838 unless 0x15f460.
// orig: 0x001fb0f0
void text_draw(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n, uint64_t tex0, uint32_t t) {
    if (lw(OV(0x0015f460)) == 0) sw(OV(0x0016c838), static_cast<uint32_t>(colour));
    if (n == 0 || lbu(s) == 0) return;
    int32_t k = 0;
    while (true) {
        const uint32_t c = lbu(s);
        if (c - 8 < 8) {
            if (lw(OV(0x0015f45c)) != 0)
                colour = (colour & 0xff000000ull) | (lwu(OV(0x0016c838) + (c - 8) * 4) & 0xffffff);
        } else if (static_cast<int8_t>(lbu(t + c * 4 + 3)) != 0) {
            if (((c + 0x80) & 0xff) < 0x28) {
                const uint32_t g = t + (c + 0x40) * 4;
                glyph_sprite(x + static_cast<int8_t>(lbu(g + 3)), y + static_cast<int8_t>(lbu(g + 2)), 0x10, 0x10, lbu(g), lbu(g + 1), 0x10, 0x10, colour, tex0);
            }
            const uint32_t c2 = lbu(s);
            const uint32_t g = t + c2 * 4;
            if (c2 < 0x20) {
                const int64_t grey = (static_cast<int64_t>(colour & 0xff) + static_cast<int64_t>((colour >> 8) & 0xff) + static_cast<int64_t>((colour >> 16) & 0xff));
                const int32_t m = static_cast<int32_t>(grey) / 3;
                const int32_t hi = static_cast<int32_t>(colour & 0xff000000ull);
                const uint64_t col = static_cast<uint64_t>(static_cast<int64_t>(m + hi + (m << 16) + (m << 8)));
                glyph_sprite(x, y + static_cast<int8_t>(lbu(g + 2)), 0x18, 0x10, lbu(g), lbu(g + 1), 0x18, 0x10, col, tex0);
            } else if (c2 > 0x20) {
                glyph_sprite(x, y + static_cast<int8_t>(lbu(g + 2)), 0x10, 0x10, lbu(g), lbu(g + 1), 0x10, 0x10, colour, tex0);
            }
            x += static_cast<int8_t>(lbu(t + lbu(s) * 4 + 3));
        }
        ++k;
        if (k == n) return;
        ++s;
        if (lbu(s) == 0) return;
    }
}

// Text centred on x in the big font (effect texture 3); returns its left edge.
// orig: 0x001fbad8
int32_t text_centered(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    x -= text_width_big(s, n) >> 1;
    text_draw(x, y, colour, s, n, effect_tex0(3), OV(0x001c3890));
    return x;
}

// Text ending at x in the small font (effect texture 1).
// orig: 0x001fb7f8
void text_right(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    const int32_t w = text_width_small(s, n);
    text_draw(x - w, y, colour, s, n, effect_tex0(1), OV(0x001c3150));
}

// A message: text centred at (x, y) on a box (alpha the colour's, at most 0x50).
// orig: 0x0023cf80
void hud_message_box(int32_t x, int32_t y, uint64_t colour, uint32_t s) {
    int32_t a = static_cast<int32_t>(colour) >> 24;
    if (!(a < 0x51)) a = 0x50;
    const int32_t l = text_centered(x + 1, y + 1, colour & 0xffffffffff000000ull, s, -1);
    hud_box(l - 0x20, y - 8, (x - (l - 0x20)) * 2, 0x20, static_cast<uint64_t>(static_cast<int64_t>(a)));
    text_centered(x, y, colour, s, -1);
}

// The elements' draw functions (+0x18) at depth 0xfffff0, then the help message 0x179118 (and
// 0x179168 while 0x15f640 is above 1000) fading in while 0x15f640 runs (alpha 0x15f644, y
// 0x15f648, 100 when none).
// orig: 0x0023af48
void hud_draw() {
    if (lw(H + 0x30) != 0 || lw(OV(0x0015f404)) != 0) {
        sw(H + 0x30, 0);
        return;
    }
    sw(H + 0xc, 0xfffff0);
    // (host: a HUD wider than 4:3, Options > Graphics > HUD: each side element's chain range noted, render_view_hud_note; in the
    // real run, not in the widened view's effects shadow (mrp_lod_effects_shadow runs this pass first, its packets put back); not on
    // the Vendor's screens, game mode 5, whose 4:3 layout the bolts element is a part of)
    const bool wide = render_view_hud_wide() && mrp_lod_shadow_slot() < 0 && lw(OV(0x0015f5c4)) != 5;
    for (uint32_t e = OV(0x0017dc50), i = 0; i < 13; ++i, e += 0x90)
        if (lwu(e + 0x18) != 0) {
            const uint8_t side = wide ? hud_element_side(e) : 0;
            const uint32_t at = side != 0 ? lwu(CUR) : 0;
            ovl_call_moby_fn(lwu(e + 0x18), e);
            if (side != 0) render_view_hud_note(at, lwu(CUR), side);  // (host)
        }
    const int32_t t = lw(OV(0x0015f640));
    if ((t == 0 && lw(OV(0x0015f644)) == 0) || lw(OV(0x0015f5c4)) != 0) {
        sw(OV(0x0015f648), 100);
        return;
    }
    if (t != 0) {
        const int32_t v = lw(OV(0x0015f644)) + 0x80 / frame_scale_i(8);
        sw(OV(0x0015f644), static_cast<uint32_t>(v));
        if (!(v < 0x81)) sw(OV(0x0015f644), 0x80);
    } else {
        const int32_t v = lw(OV(0x0015f644)) - 0x80 / frame_scale_i(8);
        sw(OV(0x0015f644), static_cast<uint32_t>(v));
        if (v < 0) sw(OV(0x0015f644), 0);
    }
    const uint64_t col = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((lwu(OV(0x0015f644)) << 24) + 0xf0f0f0)));
    hud_message_box(0x100, lw(OV(0x0015f648)), col, OV(0x00179118));
    if (lbu(OV(0x00179168)) != 0 && !(lw(OV(0x0015f640)) < 0x3e9)) hud_message_box(0x100, lw(OV(0x0015f648)), col, OV(0x00179168));
    if (lw(OV(0x0015f640)) != 0) sw(OV(0x0015f640), lwu(OV(0x0015f640)) - 1);
    if (lw(OV(0x0015f640)) == 1000) sw(OV(0x0015f640), 0);
}

// The HUD's allocator (0x17e4e8..0x17e4ec): 0x64000 bytes from 0x173e0c.
// orig: 0x002355c8
void hud_alloc_reset() {
    sw(H + 0x10, lwu(OV(0x00173e0c)));
    sw(H + 0x14, lwu(OV(0x00173e0c)) + 0x64000);
}
// `n` bytes (16-aligned) from the HUD's allocator; 0 when full.
// orig: 0x002355f0
uint32_t hud_alloc(int32_t n) {
    if (lw(H + 0x10) == 0) hud_alloc_reset();
    if (lw(H + 0x14) - lw(H + 0x10) < n) return 0;
    const uint32_t r = lwu(H + 0x10);
    sw(H + 0x10, r + ((static_cast<uint32_t>(n) + 0xf) & 0xfffffff0u));
    return r;
}

// The element request with serial `serial` gets the flags `f` (+0x24; also current +4 when not
// pending).
// orig: 0x002359f8
void hud_request_flags(int32_t serial, uint32_t f) {
    int32_t i = 0;
    for (; i < 0xd; ++i)
        if (lw(OV(0x0017dc50) + static_cast<uint32_t>(i) * 0x90 + 0x64) == serial) break;
    if (!(i < 0xd)) return;
    const uint32_t e = OV(0x0017dc50) + static_cast<uint32_t>(i) * 0x90;
    sw(e + 0x24, f);
    if (lw(e + 0x68) == 0) sw(e + 4, f);
}

// The HUD's start: the serials, each slot emptied (no request -1, flags 0x10000, faded out -6),
// the 0x2800 / 0x1400 buffers 0x15fa70 / 0x15fa7c (once) and the first cleared.
// orig: 0x00234e98
void hud_init() {
    sw(H, 0);
    sw(H + 4, 0);
    for (uint32_t i = 0; i < 0xd; ++i) {
        const uint32_t e = OV(0x0017dc50) + i * 0x90;
        sw(e + 0x64, 0xffffffffu);
        sw(e + 0x20, 0x10000);
        camera_request(i, 0xffff, 0, 0, 0, 0, 1);
        sw(e + 0x7c, 0);
        sw(e + 0x6c, 0xfffffffau);
        sw(e + 4, 0);
        sw(e + 0x24, 0);
    }
    if (lw(OV(0x0015fa70)) == 0) {
        sw(OV(0x0015fa70), hud_alloc(0x2800));
        sw(OV(0x0015fa7c), hud_alloc(0x1400));
    }
    const uint32_t b = lwu(OV(0x0015fa70));
    sw(OV(0x0015fa78), b + 0x2800);
    sw(OV(0x0015fa74), b);
    mem_clear128(b, 0x2800);
    sb(lwu(OV(0x0015fa70)) + 0x20, 0xff);
}

// The current weapon's ammo element (slot 0, key 60000 + the item, for items with ammo; none
// while 0x1413f4 (P+0x20a4)), or with P+0x20a4 = 2 the element 0xffff on P+0x1630.
// orig: 0x0023adb8
void hud_weapon_element() {
    if (lw(OV(0x0015f968)) != 0) return;
    constexpr uint32_t P = 0x0013f350;
    int32_t item = lw(P + 0x10b8);
    const uint32_t mode = lbu(P + 0x20a4);
    if (lhu(OV(0x001c40b0) + static_cast<uint32_t>(item) * 0x18 + 8) == 0) item = 0;
    if (mode != 0) item = 0;
    if (item != 0) {
        sw(OV(0x0015f96c), static_cast<uint32_t>(item));
        sw(OV(0x0015f970), static_cast<uint32_t>(camera_request(0x10, static_cast<uint32_t>(item) + 60000, OVF(0x0023a760), OVF(0x0023cdb8), OVF(0x0023a7a8),
                                                           0x0013d428 + static_cast<uint32_t>(item) * 4,
                                                           lhu(OV(0x001c40b0) + static_cast<uint32_t>(item) * 0x18 + 0xe))));
        return;
    }
    if (mode == 2) {
        if (lw(OV(0x0015f970)) != -1) {
            hud_request_flags(lw(OV(0x0015f970)), 0);
            sw(OV(0x0015f970), 0xffffffffu);
        }
        sh(P + 0x1634, static_cast<uint32_t>(camera_request(0x10, 0xffff, OVF(0x00235d80), OVF(0x00235ea0), OVF(0x0023a640), P + 0x1630, 200)));
        return;
    }
    if (lw(OV(0x0015f970)) != -1) {
        hud_request_flags(lw(OV(0x0015f970)), 0);
        sw(OV(0x0015f970), 0xffffffffu);
    }
}

// The HUD's frame: the weapon element, then each slot's fade (+0x6c: up to 0x1e while its
// timer +0x7c runs, held at 10 frames with flag 0x10 or 0x17e4dc; down to -6 after), a pending
// request started once faded out, the update function (+0x14) with `run`. The slots shown.
// orig: 0x0023ac78
int32_t hud_update(int32_t run) {
    hud_weapon_element();
    int32_t shown = 0;
    for (uint32_t e = OV(0x0017dc50); e < OV(0x0017dc50) + 0x750; e += 0x90) {
        if ((lwu(e + 4) & 0x10) != 0 || lw(H + 4) != 0) {
            if (lw(e + 0x7c) < frame_scale_i(10)) sw(e + 0x7c, static_cast<uint32_t>(frame_scale_i(10)));
        }
        const int32_t t = lw(e + 0x7c);
        bool up = false;
        if (t > 0) {
            sw(e + 0x7c, static_cast<uint32_t>(t - 1));
            up = t - 1 > 0;
        }
        if (up) {
            ++shown;
            if (lw(e + 0x6c) < 0x1e) sw(e + 0x6c, lwu(e + 0x6c) + 1);
        } else if (!(lw(e + 0x6c) < -5)) {
            sw(e + 0x6c, lwu(e + 0x6c) - 1);
        }
        if (lw(e + 0x68) != 0 && lw(e + 0x6c) == -6) camera_slot_start(e);
        if (run != 0 && lwu(e + 0x14) != 0) ovl_call_moby_fn(lwu(e + 0x14), e);
    }
    return shown;
}

// A texture's upload into the GS memory (8 bit, kept as PSMT8H at block `dbp`, 2^wl x 2^hl):
// at once by GIF DMA (`immediate`), or into the frame's packet as CNT (VIF NOP / DIRECT 6, the
// libgraph header) then a REF to the data (DIRECT).
// orig: 0x0023c7b0
void hud_tex_upload(uint32_t src, int32_t dbp, int32_t psm, int32_t wl, int32_t hl, int32_t immediate) {
    const int32_t w = 1 << wl;
    int32_t dbw = w >> 6;
    if (!(0 < dbw)) dbw = 1;
    const uint32_t qwc = 1u << (wl + hl - 4);
    uint32_t lp;
    uint32_t frame = 0;
    if (immediate == 0) {
        const uint32_t c = lwu(OV(0x001611c0));
        sw(c, 0x10000006);
        sw(c + 4, 0);
        sw(c + 8, 0);
        sw(c + 0xc, 0x50000006);
        lp = c + 0x10;
        sw(OV(0x001611c0), c + 0x70);
    } else {
        frame = ee_stack_alloc(0x60);
        lp = frame;
    }
    gs_set_def_load_image(lp, static_cast<int16_t>(dbp), static_cast<int16_t>(dbw), static_cast<int16_t>(psm), 0, 0,
                          static_cast<int16_t>(w), static_cast<int16_t>(1 << hl));
    if (immediate == 0) {
        const uint32_t c = lwu(OV(0x001611c0));
        sw(c, qwc | 0x30000000);
        sw(c + 4, src);
        sw(c + 8, 0);
        sw(c + 0xc, qwc | 0x50000000);
        sw(OV(0x001611c0), c + 0x10);
        return;
    }
    gs_exec_load_image(lp, src);  // (after FlushCache)
    ee_stack_free(0x60);
}

// The HUD data's texture and palette entries of set `set` (0 resident, 1 the level's) moved to
// their data at `base` (offsets, top bit cleared); once per load (header +0x74).
// orig: 0x002351b0
void hud_tex_relocate(int32_t set, uint32_t base) {
    const uint32_t loaded = lwu(H + 0x18) + 0x74 + static_cast<uint32_t>(set) * 4;
    if (lw(loaded) != 0) return;
    base = (base + 0xf) & 0xfffffff0u;
    sw(loaded, base);
    const uint32_t hdr = lwu(H + 0x18);
    int32_t i = set != 0 ? lw(hdr + 0x14 + static_cast<uint32_t>(set - 1) * 4) : 0;
    for (const int32_t n = lw(hdr + 0x14 + static_cast<uint32_t>(set) * 4); i < n; ++i) {
        const uint32_t e = lwu(H + 0x28) + static_cast<uint32_t>(i) * 8;
        sw(e, (lwu(e) & 0x7fffffffu) + base);
    }
    i = set != 0 ? lw(hdr + 0x34 + static_cast<uint32_t>(set - 1) * 4) : 0;
    for (const int32_t n = lw(hdr + 0x34 + static_cast<uint32_t>(set) * 4); i < n; ++i) {
        const uint32_t e = lwu(H + 0x24) + static_cast<uint32_t>(i) * 8;
        sw(e, (lwu(e) & 0x7fffffffu) + base);
    }
}

// The textures of set `set` sent into the GS memory from the cursor 0x15ee88 (each entry's +4
// gets its block), relocated to `base` first if not yet.
// orig: 0x00235490
void hud_tex_upload_set(int32_t set, uint32_t base, int32_t immediate) {
    if (lw(lwu(H + 0x18) + 0x74 + static_cast<uint32_t>(set) * 4) == 0) hud_tex_relocate(0, base);
    int32_t gsp = lw(0x0015ee88);
    int32_t i = set != 0 ? lw(lwu(H + 0x18) + 0x34 + static_cast<uint32_t>(set - 1) * 4) : 0;
    const int32_t n = lw(lwu(H + 0x18) + 0x34 + static_cast<uint32_t>(set) * 4);
    for (; i < n; ++i) {
        const uint32_t e = lwu(H + 0x24) + static_cast<uint32_t>(i) * 8;
        const int32_t dbp = gsp >> 8;
        const int32_t wl = lbu(e + 6), hl = lbu(e + 7);
        hud_tex_upload(lwu(e), dbp, 0x1b, wl, hl, immediate);
        gsp += (1 << (wl + hl)) * 4;
        sh(lwu(H + 0x24) + static_cast<uint32_t>(i) * 8 + 4, static_cast<uint32_t>(dbp));
    }
}

// The level's HUD data block `set` (level header +0x28) decompressed to `dst`; not relocated.
// orig: 0x00240088
void hud_data_decompress(int32_t set, uint32_t dst) {
    dst = (dst + 0xf) & 0xfffffff0u;
    if (dst != 0) {
        const uint32_t level = lwu(0x0015ee4c);
        wad_decompress(lwu(level + 0x28 + static_cast<uint32_t>(set) * 8) + level, dst);
    }
    sw(lwu(H + 0x18) + 0x74 + static_cast<uint32_t>(set) * 4, 0);
}

// The level's HUD data: the sizes 0x17e510 (the level header's blocks +0x2c.., 64-aligned), the
// header copied to the HUD's memory and its tables (H+0x1c..), the resident textures (+0x54:
// stashed in the IOP and uploaded at once), the level's set (+0x58), the stashed blocks
// (+0x5c / +0x60 / +0x64).
// orig: 0x0023fe10
void hud_data_load() {
    const uint32_t level = lwu(0x0015ee4c);
    for (uint32_t i = 0; i < 5; ++i) sw(OV(0x0017e510) + i * 4, (lwu(level + 0x2c + i * 8) + 0x3f) & 0xffffffc0u);
    const int32_t n = static_cast<int32_t>((lwu(level + 0x24) + 0x3f) & 0xffffffc0u);
    const uint32_t hdr = hud_alloc(n);
    mem_copy128(hdr, lwu(level + 0x20) + level, n);
    sw(H + 0x18, hdr);
    const uint32_t dst = lwu(OV(0x00173e08)) + 0x60000;
    sw(H + 0x1c, hdr + lwu(hdr + 4));
    sw(H + 0x20, hdr + lwu(hdr + 8));
    sw(H + 0x28, hdr + lwu(hdr + 0xc));
    sw(H + 0x24, hdr + lwu(hdr + 0x10));
    if (lw(hdr + 0x54) != 0) {
        const int32_t q = static_cast<int32_t>(((lwu(level + 0x2c) + 0x3f) & 0xffffffc0u) >> 4);
        hud_data_decompress(0, dst);
        const int32_t v = stash_put(lwu(level + 0x28) + level, q, q, OV(0x0015fc40));
        sw(lwu(H + 0x18) + 0x94, static_cast<uint32_t>(v));
        hud_tex_upload_set(0, dst, 1);
    }
    if (lw(lwu(H + 0x18) + 0x58) != 0) {
        const uint32_t b = hud_alloc(lw(lwu(H + 0x18) + 0x58));
        hud_data_decompress(1, b);
        hud_tex_relocate(1, b);  // (after FlushCache)
    }
    for (uint32_t k = 0; k < 3; ++k) {
        if (lw(lwu(H + 0x18) + 0x5c + k * 4) == 0) continue;
        const int32_t q = static_cast<int32_t>(((lwu(level + 0x3c + k * 8) + 0x3f) & 0xffffffc0u) >> 4);
        const int32_t v = stash_put(lwu(level + 0x38 + k * 8) + level, q, q, OV(0x0015fc50) + k * 0x10);
        sw(lwu(H + 0x18) + 0x9c + k * 4, static_cast<uint32_t>(v));
    }
}

}  // namespace rac1
