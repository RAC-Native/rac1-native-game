// Ported overlay code: the full-screen menu's drawing (game mode 3). Each widget of the page
// shown gets the screen box of its menu moby's bounds (projected with the menu camera), the
// widgets are drawn into a texture of that size (the GS frame moved to a buffer at the top of
// its memory, 0x2010b0) then copied to the screen as a sprite; the menu mobys' panels (2D) over
// them. The menu mobys themselves are 3D (the VU moby draw 0x24ff08 / 0x24ff88: TODO_PORT,
// the host draws mobys). See pause_menu.h
#include <game/lib/libgraph.h>
#include <game/enhance/mods_menu.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/pause_menu.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};      // the menu's state
constexpr OvConst<0x001b9e90u> MOBYS{};  // its 14 mobys
constexpr OvConst<0x001b23c0u> SLOTS{};  // the slots in use
constexpr OvConst<0x001611c0u> CUR{};    // the frame packet's cursor
// orig: (helper) repeated in 0x0023caa8 / 0x0023cc30 / 0x0023bd20 / 0x002010b0 / 0x002a0fc8
void cnt_direct_tag(uint32_t p, uint32_t qwc) {
    sw(p, 0x10000000u | qwc);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000000u | qwc);
}
// orig: (helper) repeated in 0x0023caa8 / 0x0023cc30: a sprite corner, pixels (+ the offset
// 0x13e510 / 0x13e514 less a half pixel) or 1/16 pixels, its z bits
uint64_t corner(int32_t x, int32_t y, int32_t pixels, uint64_t z) {
    const int32_t gx = pixels != 0 ? x + lw(0x0013e510) - 8 : x * 16 + lw(0x0013e510) - 0x10;
    const int32_t gy = pixels != 0 ? y + lw(0x0013e514) - 8 : y * 16 + lw(0x0013e514) - 0x10;
    return static_cast<uint64_t>(static_cast<int64_t>(gx)) | (static_cast<uint64_t>(static_cast<int64_t>(gy)) << 16) | z;
}
// orig: (helper) the smallest power of two from 2^7 not below n (its exponent)
int32_t log2_from_128(int32_t n) {
    int32_t s = 7;
    if (0x80 < n) {
        ++s;
        while ((1 << s) < n) ++s;
    }
    return s;
}
}  // namespace

// orig: 0x001fb038 — the text's colour codes off (0x15f45c)
void text_colour_codes_off() { sw(OV(0x0015f45c), 0); }

// Mode 3's drawing: the menu, unless the frame is skipped (0x15f5d8).
// orig: 0x001f8f50
void menu_draw_mode3() {
    if (lw(OV(0x0015f5d8)) == 0) menu_draw(0);
}

// A flat sprite (PRIM 0x46: sprite, blended) in colour `rgba` from (x0, y0) to (x1, y1):
// `pixels` nonzero for coordinates in 1/16 pixels... in pixels (else whole pixels scaled), z
// the top bits 0xfffff0.
// orig: 0x0023caa8
void gs_flat_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint64_t rgba, int32_t pixels) {
    const uint32_t p = lwu(CUR);
    cnt_direct_tag(p, 3);
    sw(CUR, p + 0x10);
    sd(p + 0x10, 0x4400000000000001ull);
    sd(p + 0x18, 0x4410);
    sd(p + 0x20, 0x46);
    sd(p + 0x28, rgba);
    constexpr uint64_t z = 0x00fffff000000000ull;
    sd(p + 0x30, corner(x0, y0, pixels, z));
    sd(p + 0x38, corner(x1, y1, pixels, z));
    sw(CUR, lwu(CUR) + 0x30);
}

// The same with the z `z` (bits 32..63).
// orig: 0x0023cc30
void gs_flat_rect_z(int32_t x0, int32_t y0, int32_t x1, int32_t y1, uint64_t rgba, uint64_t z, int32_t pixels) {
    const uint32_t p = lwu(CUR);
    cnt_direct_tag(p, 3);
    sw(CUR, p + 0x10);
    sd(p + 0x10, 0x4400000000000001ull);
    sd(p + 0x18, 0x4410);
    sd(p + 0x20, 0x46);
    sd(p + 0x28, rgba);
    const uint64_t zz = z << 32;
    sd(p + 0x30, corner(x0, y0, pixels, zz));
    sd(p + 0x38, corner(x1, y1, pixels, zz));
    sw(CUR, lwu(CUR) + 0x30);
}

// A HUD frame's sprite over (x, y) w x h (1/16 pixels), alpha `a`, the whole texture, the HUD's
// z (0x17e4e4).
// orig: 0x0023bd20
void hud_sprite_xyz(int32_t frame, int32_t x, int32_t y, int32_t w, int32_t h, int32_t a) {
    constexpr OvConst<0x0017e4d8u> H{};
    const uint32_t t = lwu(H + 0x24) + static_cast<uint32_t>(lh(lwu(H + 0x20) + static_cast<uint32_t>(frame) * 4 + 2)) * 8;
    const int32_t tw = 1 << lbu(t + 6), th = 1 << lbu(t + 7);
    const uint32_t p = lwu(CUR);
    cnt_direct_tag(p, 5);
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    sd(p + 0x10, 0x7400000000008001ull);
    sd(q + 8, 0x05353106);
    sd(q + 0x10, hud_tex0(frame));
    sd(q + 0x20, (static_cast<uint64_t>(static_cast<int64_t>(a)) << 24) | 0x7f7f7f);
    sd(q + 0x18, 0x156);
    sd(q + 0x28, 0);
    const uint64_t zt = static_cast<uint64_t>(static_cast<int64_t>(lw(H + 0xc))) << 32;
    const int32_t x0 = x + lw(0x0013e510) - 8, y0 = y + lw(0x0013e514) - 8;
    sd(q + 0x38, static_cast<uint64_t>(static_cast<int64_t>((th << 20) + (tw << 4))));
    sd(q + 0x30, static_cast<uint64_t>(static_cast<int64_t>(x0)) | (static_cast<uint64_t>(static_cast<int64_t>(y0)) << 16) | zt);
    const int32_t x1 = x + w + lw(0x0013e510) - 8, y1 = y + h + lw(0x0013e514) - 8;
    const uint64_t zt2 = static_cast<uint64_t>(static_cast<int64_t>(lw(H + 0xc))) << 32;
    sd(q + 0x48, 0);
    sd(q + 0x40, static_cast<uint64_t>(static_cast<int64_t>(x1)) | (static_cast<uint64_t>(static_cast<int64_t>(y1)) << 16) | zt2);
    sw(CUR, lwu(CUR) + 0x50);
}

// `bytes` of EE memory at `src` (rounded up to 16 KB) to the GS at block `dbp` (+ 1/256 per
// byte): IMAGE transfers of 64 x 64 32-bit pixels, the data by REF tags.
// orig: 0x002a0f18
void gs_store_rows(uint32_t dst, int32_t dbp, int32_t bytes) {
    int32_t left = (bytes + 0x3fff) & -0x4000;
    if (left <= 0) return;
    const uint32_t lp = ee_stack_alloc(0xc0);  // (the transfer's block at the frame's sp)
    uint32_t off = 0;
    do {
        gs_set_def_store_image(lp, static_cast<int32_t>((static_cast<uint32_t>(dbp) + off) << 8) >> 16, 1, 0, 0, 0, 0x40, 0x40);
        left -= 0x4000;
        // (FlushCache(0) 0x118a80: the data cache written back, no memory effect on the host)
        gs_exec_store_image(lp, dst + off);
        off += 0x4000;
        gs_sync_path(0, 0);
    } while (left > 0);
    ee_stack_free(0xc0);
}

// orig: 0x002a0fc8
void gs_upload_rows(uint32_t src, int32_t dbp, int32_t bytes) {
    int32_t left = (bytes + 0x3fff) & -0x4000;
    int32_t off = 0;
    while (left > 0) {
        const uint32_t p = lwu(CUR);
        cnt_direct_tag(p, 6);
        left -= 0x4000;
        const uint32_t lp = p + 0x10;
        sw(CUR, lp);
        gs_set_def_load_image(lp, static_cast<int32_t>(static_cast<uint32_t>(dbp + off) << 8) >> 16, 1, 0, 0, 0, 0x40, 0x40);
        const uint32_t v = lwu(CUR);
        const uint32_t a = src + static_cast<uint32_t>(off);
        off += 0x4000;
        sw(CUR, v + 0x60);
        sw(v + 0x60, 0x30000400);
        sw(lwu(CUR) + 4, a);
        sw(lwu(CUR) + 8, 0);
        sw(lwu(CUR) + 0xc, 0x50000400);
        sw(CUR, lwu(CUR) + 0x10);
    }
}

// The scratchpad's moby block (0x70003a00, 0xe00 bytes): filled with 0x40000000, saved to /
// loaded from 0x196f80.
// orig: 0x0024fdc8
void spr_moby_fill() { mem_fill32(0x70003a00, 0x40000000, 0x380); }
// orig: 0x0024fdf0
void spr_moby_save() { mem_copy128(OV(0x00196f80), 0x70003a00, 0x380); }
// orig: 0x0024fe20
void spr_moby_load() { mem_copy128(0x70003a00, OV(0x00196f80), 0x380); }

// The screen box of the bounds p0 / p1 (EE vectors) seen from the menu camera (0x166dc0, its
// view 0x166d80; scaled by 1024, projected, the screen scale 0x16cbd0 / 0x16cbd4 and the
// centre 0x13e508 / 0x13e50c, 1/4): x, y, w, h stored at the addresses given.
// orig: 0x00299f40
void menu_box_project(uint32_t p0, uint32_t p1, uint32_t w, uint32_t h, uint32_t x, uint32_t y) {
    Vec4 a = V(p0), b = V(p1);
    vec3_sub(a, a, V(OV(0x00166dc0)));
    vec3_sub(b, b, V(OV(0x00166dc0)));
    vec3_scale(a, a, 1024.0f);
    vec3_scale(b, b, 1024.0f);
    b.w = 1.0f;
    a.w = 1.0f;
    mat4_transform(a, a, M(OV(0x00166d80)));
    mat4_transform(b, b, M(OV(0x00166d80)));
    const float ia = fpu_div(1.0f, a.w);
    a.y = fpu_mul(a.y, ia);
    a.x = fpu_mul(a.x, ia);
    const float ib = fpu_div(1.0f, b.w);
    b.x = fpu_mul(b.x, ib);
    const float ib2 = fpu_div(1.0f, b.w);
    b.y = fpu_mul(b.y, ib2);
    const float sx = lwc1(OV(0x0016ca40) + 0x190), sy = lwc1(OV(0x0016ca40) + 0x194);
    a.x = fpu_mul(a.x, sx);
    a.y = fpu_mul(a.y, sy);
    b.x = fpu_mul(b.x, sx);
    b.y = fpu_mul(b.y, sy);
    sw(x, static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(a.x, 0.25f), int_to_float(lw(0x0013e500 + 8))))));
    sw(y, static_cast<uint32_t>(float_to_int(fpu_add(fpu_mul(a.y, 0.25f), int_to_float(lw(0x0013e500 + 0xc))))));
    sw(w, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(b.x, a.x), 0.25f))));
    sw(h, static_cast<uint32_t>(float_to_int(fpu_mul(fpu_sub(b.y, a.y), 0.25f))));
}

// The view of a render target W x H (the screen globals 0x13e500.., the projection 0x16cc40..
// from the aspect `aspect` and the depth constants), the projection rebuilt (0x1f7980).
// orig: 0x001f7fa0
void render_target_view(int32_t w, int32_t h, float aspect, float a, float b, float c, float d) {
    const uint32_t S = 0x0013e500, R = OV(0x0016ca40);
    const int32_t hh = h >> 1, hw = w >> 1;
    sw(S + 0x1c, static_cast<uint32_t>((hh + 0x800) << 4));
    swc1(R + 0xb0, aspect);
    sw(S, static_cast<uint32_t>(w));
    sw(S + 0x10, static_cast<uint32_t>((0x800 - hw) << 4));
    sw(S + 0x14, static_cast<uint32_t>((0x800 - hh) << 4));
    sw(S + 0x18, static_cast<uint32_t>((hw + 0x800) << 4));
    swc1(R + 0xa4, 524288.0f);
    swc1(R + 0xa0, 32.0f);
    sw(S + 4, static_cast<uint32_t>(h));
    sw(S + 8, static_cast<uint32_t>(hw));
    sw(S + 0xc, static_cast<uint32_t>(hh));
    swc1(R + 0x200, fpu_mul(int_to_float(w), 0.5f));
    const float fh = fpu_mul(int_to_float(h), 0.5f);
    swc1(R + 0x22c, d);
    swc1(R + 0x218, a);
    swc1(R + 0x21c, b);
    swc1(R + 0x228, c);
    swc1(R + 0x20c, fpu_mul(fh, 4.0f));
    swc1(R + 0x208, fpu_mul(lwc1(R + 0x200), 4.0f));
    swc1(R + 0x204, fh);
    projection_setup();
}

// The GS drawing moved to a 2^lw x 2^lh 32-bit buffer at byte `addr` of its memory (TEX0 of
// it for the copy kept in 0x15eed0): the frame packet gets the draw environment (0x121fc8, the
// frame base and the z buffer 0x1518e6 / 0x1518ec put in) and a clearing sprite.
// orig: 0x002010b0
void render_target_set(int32_t lw2, int32_t lh2, int32_t addr) {
    constexpr uint32_t G = 0x00151780;
    const int32_t e = lw2 - 6;
    const int32_t tbw = 1 << (0 < e ? e : 1);
    const uint64_t tex0 = static_cast<uint64_t>(static_cast<int64_t>(addr >> 8)) | (static_cast<uint64_t>(static_cast<int64_t>(tbw)) << 14) |
                          (static_cast<uint64_t>(static_cast<int64_t>(lw2)) << 26) | (static_cast<uint64_t>(static_cast<int64_t>(lh2)) << 30) | 0x400000000ull;
    sh(G + 0x166, static_cast<uint32_t>(addr >> 13));
    sh(G + 0x160, static_cast<uint32_t>(1 << lw2));
    sh(G + 0x162, static_cast<uint32_t>(1 << lh2));
    sd(0x0015eed0, tex0);
    mem_clear128(lwu(CUR), 0xf0);
    const uint32_t p = lwu(CUR);
    cnt_direct_tag(p, 0xe);
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    sd(p + 0x10, 0x1000000000000008ull);
    sd(q + 8, 0xe);
    const uint32_t env = lwu(CUR) + 0x10;
    const uint32_t s1 = lwu(CUR);
    sw(CUR, env);
    gs_set_def_draw_env(env, lh(G + 0x164), lh(G + 0x160), lh(G + 0x162), 3, 0);
    sd(s1 + 0x10, (ld(s1 + 0x10) & ~0x1ffull) | static_cast<uint64_t>(lh(G + 0x166) & 0x1ff));
    sd(env + 0x10, static_cast<uint64_t>(static_cast<int64_t>(lh(G + 0x16e))) | (static_cast<uint64_t>(lhu(G + 0x16c) & 0xf) << 24));
    const uint32_t c = lwu(CUR);
    const uint32_t a = c + 0x80;
    sw(CUR, a);
    sd(c + 0x80, 0x1000000000000001ull);
    sd(a + 8, 0xe);
    sd(a + 0x10, 0x30003);
    sd(a + 0x18, 0x47);
    sd(a + 0x20, 0x4400000000008001ull);
    sd(a + 0x28, 0x4410);
    sd(a + 0x30, 0x106);
    sd(a + 0x38, 0);
    const int32_t th = lh(G + 0x162), tw = lh(G + 0x160);
    sd(a + 0x40, static_cast<uint64_t>(static_cast<int64_t>(0x8000 - tw * 8)) | (static_cast<uint64_t>(static_cast<int64_t>(0x8000 - th * 8)) << 16));
    const int32_t th2 = lh(G + 0x162), tw2 = lh(G + 0x160);
    sd(a + 0x48, static_cast<uint64_t>(static_cast<int64_t>(tw2 * 8 + 0x8000)) | (static_cast<uint64_t>(static_cast<int64_t>(th2 * 8 + 0x7ff0)) << 16));
    sw(CUR, lwu(CUR) + 0x50);
}

// Draw into a 2^lw x 2^lh texture: the buffer below the GS memory's top 0x15ee8c (`second`:
// the one at 0x1518ee), its view (aspect, depth 0 .. 524288, 255), z test off (`second`) or
// always, alpha blend (0x44).
// orig: 0x001fc928
void render_to_texture_begin(int32_t lw2, int32_t lh2, int32_t second, float aspect) {
    int32_t blocks;
    if (second != 0) {
        blocks = lh(0x001518ee);
    } else {
        int32_t s = lw2 + lh2;
        if (!(s < 0x11)) s = 0x10;
        blocks = (lw(0x0015ee8c) - (4 << s)) >> 13;
    }
    render_target_set(lw2, lh2, blocks << 13);
    render_target_view(1 << lw2, 1 << lh2, aspect, 0.0f, 524288.0f, 255.0f, 0.0f);
    if (second != 0) sub_002a0ba8(0x47, 0);
    else sub_002a0ba8(0x47, 0x30000);
    sub_002a0ba8(0x42, 0x8000000044ull);
}

// Back to the screen: its packet, its view, the projection.
// orig: 0x001fca18
void render_to_texture_end() {
    packet_call_display();
    screen_setup();
    projection_setup();
}

// The highlight colour of an entry `t` frames into its highlight (0x160274 frames from c0 to
// c1; -1: 0x80ffa888 / 0x8020ffff).
// orig: 0x00279320
uint32_t menu_highlight_colour(int32_t t, int32_t c0, int32_t c1) {
    const int32_t k = -1 < t ? t : 0;
    const uint32_t a = c0 != -1 ? static_cast<uint32_t>(c0) : 0x80ffa888u;
    const uint32_t b = c1 != -1 ? static_cast<uint32_t>(c1) : 0x8020ffffu;
    float f;
    if (frame_scale_i(lw(OV(0x00160274))) < k) {
        f = 1.0f;
    } else {
        const int32_t n = frame_scale_i(lw(OV(0x00160274)));
        const int32_t d = frame_scale_i(lw(OV(0x00160274)));
        f = fpu_sub(1.0f, fpu_div(int_to_float(n - k), int_to_float(d)));
    }
    return rgba_lerp(a, b, f);
}

// A list of entries' texts (the pause menu's buttons) in its box (+0x18..+0x24): font small /
// big (flag 4) / 0x1c34f0 (flag 8), the widest entry (flag 0x4000) centred, the line height
// (flag 0x10) or the box's height shared; the selected entry highlighted, the disabled ones
// (action 0) dark; each text with its shadow. 2 (1 when the box is too narrow: flag 8 set).
// orig: 0x00278e10
int32_t menu_button_draw(uint32_t w) {
    const bool focused = lwu(lwu(MS + 4) + 0x40) == w;
    const uint32_t fl0 = lwu(w + 0x30);
    uint32_t font = OV(0x001c3150);
    int32_t tex = 1, lh0 = 0xc;
    if ((fl0 & 4) != 0) {
        tex = 3;
        font = OV(0x001c3890);
        lh0 = 0xe;
    }
    if ((fl0 & 8) != 0) {
        tex = 2;
        font = OV(0x001c34f0);
        lh0 = 0xa;
    }
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x2004b);
    texture_block_begin(0);
    int32_t count = 0;
    for (uint32_t p = lwu(w + 0x34); lh(p) != 0; p += 0xc) ++count;
    const uint32_t flags = lwu(w + 0x30);
    int32_t step;
    if ((flags & 0x10) != 0) step = lh0 + 3;
    else step = ee_div(lw(w + 0x24), count + 1);
    const int32_t cx = lw(w + 0x20) >> 1;
    int32_t widest = 0;
    int32_t y = step - lh0 / 2 - 1;
    if ((flags & 0x4000) != 0 && lh(lwu(w + 0x34)) != 0) {
        for (uint32_t off = 0; lh(lwu(w + 0x34) + off) != 0; off += 0xc) {
            const int32_t tw = text_width(sub_00203ad8(lh(lwu(w + 0x34) + off)), -1, font);
            if (widest < tw) widest = tw;
        }
    }
    const uint32_t fl = lwu(w + 0x30);
    const int32_t bw = lw(w + 0x20);
    if ((fl & 0x20000) != 0 && bw < widest + 6 && (fl & 8) == 0) {
        sw(w + 0x30, fl | 8);
        return 1;
    }
    if (bw < widest) widest = bw;
    if (lh(lwu(w + 0x34)) != 0) {
        int32_t idx = 0;
        for (uint32_t off = 0; lh(lwu(w + 0x34) + off) != 0; off += 0xc, ++idx) {
            const bool sel = focused && lw(w + 0x40) == idx;
            const uint32_t e = lwu(w + 0x34) + off;
            const bool active = lh(e + 2) != 0;
            uint64_t colour;
            if ((lwu(w + 0x30) & 2) != 0) colour = 0xffffffff80ffa888ull;
            else if (sel && active) colour = static_cast<uint64_t>(menu_highlight_colour(lh(e + 0xa), -1, -1));
            else if (sel) colour = 0xffffffff80006060ull;
            else if (active) colour = static_cast<uint64_t>(menu_highlight_colour(lh(e + 0xa), -1, -1));
            else colour = 0xffffffff80303030ull;
            uint32_t s = sub_00203ad8(lh(lwu(w + 0x34) + off));
            if (lh(lwu(w + 0x34) + off + 2) == 2) s = sub_00203ad8(0x4f54);
            int32_t x = cx - (text_width(s, -1, font) >> 1);
            const uint32_t f2 = lwu(w + 0x30);
            if ((f2 & 0x40) != 0) x = 4;
            else if ((f2 & 0x4000) != 0) x = cx - (widest >> 1);
            text_colour_codes_off();
            text_draw(x + lw(OV(0x00160278)), y + lw(OV(0x0016027c)), 0x80000000ull, s, -1, effect_tex0(tex), font);
            sub_001fb028();
            if ((lwu(w + 0x30) & 0x80) != 0) text_colour_codes_off();
            text_draw(x, y, colour, s, -1, effect_tex0(tex), font);
            y += step;
            if (lh(lwu(w + 0x34) + off + 8) != 0) {
                text_colour_codes_off();
                const uint32_t sub = sub_00203ad8(lh(lwu(w + 0x34) + off + 8));
                text_draw(x + lw(OV(0x00160278)), y + lw(OV(0x0016027c)), 0x80000000ull, sub, -1, effect_tex0(tex), font);
                if ((lwu(w + 0x30) & 0x80) == 0) sub_001fb028();
                const uint32_t sub2 = sub_00203ad8(lh(lwu(w + 0x34) + off + 8));
                text_draw(x, y, colour, sub2, -1, effect_tex0(tex), font);
                y += step;
            }
            if ((lwu(w + 0x30) & 0x80) != 0) sub_001fb028();
        }
    }
    texture_block_end();
    return 2;
}

// A menu moby's panel (its box +0x50..+0x5c of its data +0x78, on screen): the backing frame
// 0xe99e, now and then a flicker (+0x48 / +0x4c: a random texel offset, fading in and out),
// the scan lines and the border.
// orig: 0x00281a70
void menu_panel_draw(uint32_t m) {
    if (m == 0) return;
    const uint32_t d = lwu(m + 0x78);
    if (d == 0) return;
    const int32_t x = lw(d + 0x50), y = lw(d + 0x54), w = lw(d + 0x58), h = lw(d + 0x5c);
    if (!(x < 0x200)) return;
    if (x + w < 0) return;
    const bool in = lw(0x0015ed80) != 0 ? y < 0x1c1 : y < 0x1a1;
    if (!in) return;
    if (y + h < 0) return;
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x42, 0x8000000044ull);
    hud_sprite_xyz(hud_frame(0xe99e, 7), x << 4, y << 4, w << 4, h << 4, 0x80);
    if (lw(d + 0x48) != 0) {
        sw(d + 0x4c, static_cast<uint32_t>(lw(d + 0x4c) + 2));
        const int32_t u = rand_mod(0xc8);
        const int32_t v = rand_mod(0xc8);
        int32_t a = (0x80 - abs_i(lw(d + 0x4c) - 0x80)) << 1;
        sub_002a0ba8(8, 0);
        if (!(a < 0x81)) a = 0x80;
        sub_002a0ba8(0x42, (static_cast<uint64_t>(static_cast<int64_t>(a)) << 32) | 0x68);
        glyph_sprite(x, y, w, h, u, v, w, h, 0x808080, effect_tex0(0x1a));
        if (!(lw(d + 0x4c) < 0x100)) sw(d + 0x48, 0);
    } else if (rand_mod(0x7d0) == 0) {
        sw(d + 0x4c, 0);
        sw(d + 0x48, 1);
    }
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x42, 0x8000000044ull);
    glyph_sprite(x, y, w, h, 0, 0, w, (h * 3) >> 1, 0x50606060, effect_tex0(0x1c));
    int32_t dw = -2, dh = -2;
    if (!(w < 0x4c)) dw = -1;
    if (!(h < 0x4c)) dh = -1;
    if (!(w < 0x97)) dw = 0;
    if (!(h < 0x97)) dh = 0;
    glyph_sprite(x + 1, y + 1, w + dw, h + dh, 1, 1, 0x3e, 0x3e, 0x80808080ull, effect_tex0(0x19));
}

// The menu's frame drawing: the captured frame back to the GS (0x2a0fc8), then (not skipped,
// not closing) the menu mobys (3D), each widget's box from its moby, the widgets drawn into
// textures and copied (flag 2 before the mobys' flush, the others after), the panels.
// orig: 0x002772c0
void menu_draw(int32_t skip) {
    sub_00200fd8();
    gs_upload_rows(lwu(MS + 0x10), lw(0x0015ee84), lw(0x0013e504) << 11);
    if (skip != 0) return;
    render_static_packets();
    sub_001fa050(0, 0, 0, 0x30);
    if (lw(MS) == 0x14) return;
    sub_002a0ba8(0x47, 0x5360b);
    moby_pass_start();  // (the menu mobys, the panels' frames, through the moby pass)
    spr_moby_fill();
    spr_moby_save();
    for (uint32_t i = 0; i < 0xe; ++i) {
        if (lwu(SLOTS + i * 4) == 0) continue;
        const uint32_t m = lwu(MOBYS + i * 4);
        if (m == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        moby_draw_list(m, 1);
    }
    const uint32_t page = lwu(MS + 4);
    const uint32_t ws = page != 0 ? page + 0x44 : 0;
    const uint32_t box = ee_stack_alloc(0x60);  // the bounds and the box by address
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t m = lwu(MOBYS + i * 4);
        if (m == 0 || lwu(SLOTS + i * 4) == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        const uint32_t d = lwu(m + 0x78);
        lq_sq(box, d);
        lq_sq(box + 0x10, d + 0x30);
        menu_box_project(box, box + 0x10, box + 0x20, box + 0x24, box + 0x28, box + 0x2c);
        const int32_t x = lw(box + 0x28) + 1, y = lw(box + 0x2c) + 1, bw = lw(box + 0x20), bh = lw(box + 0x24);
        if (ws != 0) {
            const uint32_t w = lwu(ws + i * 4);
            if (w != 0) {
                sw(w + 0x20, static_cast<uint32_t>(bw));
                sw(w + 0x24, static_cast<uint32_t>(bh));
                sw(w + 0x18, static_cast<uint32_t>(x));
                sw(w + 0x1c, static_cast<uint32_t>(y));
            }
        }
        sw(d + 0x50, static_cast<uint32_t>(x));
        sw(d + 0x54, static_cast<uint32_t>(y));
        sw(d + 0x58, static_cast<uint32_t>(bw));
        sw(d + 0x5c, static_cast<uint32_t>(bh));
        gs_flat_rect(x + 1, y + 1, x + bw - 1, y + bh - 1, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00160270)))), 0);
    }
    ee_stack_free(0x60);
    for (int32_t pass = 0; pass < 2; ++pass) {
        for (uint32_t i = 0; i < 0xe; ++i) {
            const uint32_t m = lwu(MOBYS + i * 4);
            if (m == 0 || ws == 0) continue;
            const uint32_t w = lwu(ws + i * 4);
            if (w == 0) continue;
            const uint32_t df = lwu(w + 0x10);
            if ((df & 4) != 0) continue;
            if (lwu(SLOTS + i * 4) == 0) continue;
            const uint32_t fn = lwu(w + 4);
            if (fn == 0) continue;
            if (i == 6 && lw(MS + 0xd8) == 0) continue;
            if (pass == 0 && (df & 2) == 0) continue;
            if (pass == 1 && (df & 2) != 0) continue;
            if ((df & 1) != 0) {
                menu_widget_call(fn, w, 0);
                continue;
            }
            const uint32_t d = lwu(m + 0x78);
            const int32_t bw = lw(d + 0x58), bh = lw(d + 0x5c), x = lw(d + 0x50), y = lw(d + 0x54);
            const int32_t sx = log2_from_128(bw);
            int32_t sy = log2_from_128(bh);
            while (!(sx + sy < 0x12)) --sy;
            render_to_texture_begin(sx, sy, pass != 0 ? 1 : 0, 1.0f);
            const int32_t tw = 1 << sx, th = 1 << sy;
            gs_flat_rect_z(0, 0, tw, th, static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x00160270)))), 0, 0);
            const uint32_t w2 = lwu(ws + i * 4);
            const int32_t r = menu_widget_call(lwu(w2 + 4), w2, 0);
            render_to_texture_end();
            sub_002a0ba8(0x42, 0x8000000064ull);
            sub_002a0ba8(0x47, 0x43);
            if ((r & 1) != 0) continue;
            int32_t u0 = 0, v0 = 0, u1 = tw, v1 = th;
            if ((r & 2) != 0) {
                u1 = bw;
                v1 = bh;
            } else if ((r & 8) != 0) {
                const int32_t dx = tw - bw, dy = th - bh;
                u0 = dx / 2;
                v0 = dy / 2;
                u1 = tw - u0;
                v1 = th - v0;
                if (tw < u1) u1 = tw;
                if (th < v1) v1 = th;
                if (u0 < 0) u0 = 0;
                if (v0 < 0) v0 = 0;
            } else if ((r & 4) != 0) {
                if (bw < bh) {
                    u0 = tw / 2 - ee_div(bw * u1, bh * 2);
                    u1 = u1 - u0;
                } else {
                    v0 = th / 2 - ee_div(bh * v1, bw * 2);
                    v1 = v1 - v0;
                }
            } else if ((r & 0x10) == 0) {
                continue;
            }
            glyph_sprite(x, y, bw, bh, u0, v0, u1 - u0, v1 - v0, 0x80808080ull, ld(0x0015eed0));
        }
        if (pass == 0) {
            spr_moby_load();
            moby_pass_end();
            dma_wait(0x10);
            moby_texture_slots();
        }
    }
    texture_block_begin(0);
    for (uint32_t i = 0; i < 0xe; ++i) {
        if (lwu(SLOTS + i * 4) == 0) continue;
        if (i == 6 && lw(MS + 0xd8) == 0) continue;
        menu_panel_draw(lwu(MOBYS + i * 4));
    }
    mods_notice_draw();  // (enhancement: "Mods on" while mods are on, mods_menu.h)
    texture_block_end();
}

}  // namespace rac1
