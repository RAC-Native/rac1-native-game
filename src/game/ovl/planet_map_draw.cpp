// Ported overlay code: the planet map's drawing (see planet_map.h): the map image and its explored mask, the marker slots, the markers (overlapping ones
// pushed apart, rotated arrows, their text boxes), the hero's arrow; the planet picture
// (layered, scrolling); the legend.
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/planet_map.h>
#include <host/level_slots.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001841f0u> MAP{};
constexpr OvConst<0x001611c0u> CUR{};   // the frame packet's cursor
constexpr OvConst<0x0017e4d8u> HUD{};   // the HUD frames (+0xc z, +0x20 frames, +0x24 textures)
constexpr uint32_t SPR = 0x70000000;   // the scratchpad (the markers' rectangles)
constexpr uint32_t OFS = 0x0013e510;   // the screen offset x (+4 y)
constexpr float HALF_PI = 1.57079637f;  // 0x3fc90fdb
// orig: (helper) EE CVT.W.S: truncate toward zero, saturating
int32_t cvt_w_s(float f) {
    if (f >= 2147483648.0f) return INT32_MAX;
    if (f <= -2147483648.0f) return INT32_MIN;
    return static_cast<int32_t>(f);
}
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
// orig: (helper) a GS XYZ2 word: (x, y) moved by the screen offset 0x13e510 / 0x13e514, the HUD's z
uint64_t xyz(int32_t x, int32_t y, int32_t z) {
    return sx64(x + lw(OFS) - 8) | (sx64(y + lw(OFS + 4) - 8) << 16) | (sx64(z) << 32);
}
float* fp(uint32_t a) { return static_cast<float*>(ee_ram_ptr(a)); }
}  // namespace

// Debug text queued at (x, y) in `colour` (0x16a840, its copy in the pool *0x15efc0).
// orig: 0x001f3680
void debug_text(int32_t x, int32_t y, uint32_t colour, uint32_t s) {
    const int32_t n = lw(OV(0x0015efc4));
    const uint32_t r = OV(0x0016a840) + static_cast<uint32_t>(n) * 16;
    const uint32_t pool = lwu(OV(0x0015efc0));
    sw(r, static_cast<uint32_t>(x));
    sw(r + 4, static_cast<uint32_t>(y));
    sw(r + 8, colour);
    sw(r + 0xc, pool);
    sw(OV(0x0015efc4), static_cast<uint32_t>(n + 1));
    std::vector<uint64_t> a(1, s);
    const int32_t len = libc_sprintf(pool, OV(0x0015efc8), a);
    sw(OV(0x0015efc0), lwu(OV(0x0015efc0)) + static_cast<uint32_t>(len + 1));
}

// Debug text centred on x (the glyph widths 0x169b00); the left x.
// orig: 0x001f39e0
int32_t debug_text_centred(int32_t x, int32_t y, uint32_t colour, uint32_t s) {
    int32_t w = 0;
    if (lbu(s) != 0) {
        uint32_t p = s;
        uint32_t c;
        do {
            uint32_t k = (lbu(p) - 0x20u) & 0xffu;
            ++p;
            c = lbu(p);
            if (!(k < 0x60u)) k = 0x20;
            w += lw(OV(0x00169b00) + k * 4);
        } while (c != 0);
    }
    const int32_t x0 = x - (w >> 1);
    debug_text(x0, y, colour, s);
    return x0;
}

// Text in the medium font (TEX0 2, font 0x1c34f0) / its width.
// orig: 0x001fb3f0
void text_med(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    text_draw(x, y, colour, s, n, effect_tex0(2), OV(0x001c34f0));
}
// orig: 0x001fb0b0
int32_t text_width_med(uint32_t s, int32_t n) { return text_width(s, n, OV(0x001c34f0)); }

// A textured sprite from (x, y) (1/16 pixels) of size w x h, the texels (u, v) to
// (u + 2^(lw+4), v + 2^(lh+4)) (1/16 texels), alpha `a`, TEX0 `t`.
// orig: 0x0023c108
void sprite_uv(uint64_t t, int32_t x, int32_t y, int32_t lw_, int32_t lh_, int32_t w, int32_t h, int32_t u, int32_t v, int32_t a) {
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000005);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000005);
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    const int32_t u1 = u + static_cast<int32_t>(1u << ((lw_ + 4) & 31));
    const int32_t v1 = v + static_cast<int32_t>(1u << ((lh_ + 4) & 31));
    sd(q, 0x7400000000008001ull);
    sd(q + 8, 0x05353106);
    sd(q + 0x18, 0x156);
    sd(q + 0x28, sx64(u) | (sx64(v) << 16));
    sd(q + 0x10, t);
    sd(q + 0x20, (sx64(a) << 24) | 0x7f7f7f);
    sd(q + 0x38, sx64(u1) | (sx64(v1) << 16));
    sd(q + 0x30, xyz(x, y, lw(HUD + 0xc)));
    sd(q + 0x48, 0);
    sd(q + 0x40, xyz(x + w, y + h, lw(HUD + 0xc)));
    sw(CUR, lwu(CUR) + 0x50);
}

// A textured quad centred on (x, y), half sizes (hw, hh), turned by `ang`, its texture
// (tw x th texels, a multiple of 1/16) in TEX0 `t`.
// orig: 0x0023c2a0
void sprite_rotated(int32_t tw, int32_t th, uint64_t t, float x, float y, float hw, float hh, float ang) {
    const uint32_t b = ee_stack_alloc(0x70);  // the frame: U +0, V +0x10, the centre +0x20, the corners +0x30..+0x60
    swc1(b + 0x20, x);
    swc1(b + 0x24, y);
    swc1(b, fpu_mul(hh, sin_vu(ang)));
    swc1(b + 4, fpu_mul(hh, cos_vu(ang)));
    swc1(b + 0x10, fpu_mul(-hw, cos_vu(ang)));
    swc1(b + 0x14, fpu_mul(hw, sin_vu(ang)));
    vec3_add(V(b + 0x30), V(b + 0x20), V(b));
    vec3_sub(V(b + 0x30), V(b + 0x30), V(b + 0x10));
    vec3_add(V(b + 0x40), V(b + 0x20), V(b));
    vec3_add(V(b + 0x40), V(b + 0x40), V(b + 0x10));
    vec3_sub(V(b + 0x50), V(b + 0x20), V(b));
    vec3_sub(V(b + 0x50), V(b + 0x50), V(b + 0x10));
    vec3_sub(V(b + 0x60), V(b + 0x20), V(b));
    vec3_add(V(b + 0x60), V(b + 0x60), V(b + 0x10));
    const uint32_t p = lwu(CUR);
    sw(p, 0x10000007);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000007);
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    const uint32_t su = static_cast<uint32_t>(tw) << 4, sv = static_cast<uint32_t>(th) << 20;
    sd(q, 0xb400000000008001ull);
    sd(q + 0x10, t);
    sd(q + 0x28, sx64(static_cast<int32_t>(su)));
    sd(q + 8, 0x53535353106ull);
    sd(q + 0x18, 0x154);
    sd(q + 0x20, 0x807f7f7full);
    const int32_t z = lw(HUD + 0xc);
    sd(q + 0x38, sx64(static_cast<int32_t>(sv + su)));
    sd(q + 0x30, xyz(cvt_w_s(lwc1(b + 0x30)), cvt_w_s(lwc1(b + 0x34)), z));
    sd(q + 0x48, 0);
    sd(q + 0x40, xyz(cvt_w_s(lwc1(b + 0x40)), cvt_w_s(lwc1(b + 0x44)), z));
    sd(q + 0x50, xyz(cvt_w_s(lwc1(b + 0x50)), cvt_w_s(lwc1(b + 0x54)), z));
    sd(q + 0x58, sx64(static_cast<int32_t>(sv)));
    sd(q + 0x68, 0);
    sd(q + 0x60, xyz(cvt_w_s(lwc1(b + 0x60)), cvt_w_s(lwc1(b + 0x64)), z));
    sw(CUR, lwu(CUR) + 0x70);
    ee_stack_free(0x70);
}

// The map's marker slots (+0x1c, +0xb0 of them; 16 bytes: x, y, a HUD frame or -1 for a square
// of colour +0xc) placed on the map (x0, y0) .. (x1, y1) (1/16 pixels), those on screen drawn.
// orig: 0x0024a8e8
void map_slots_draw(int32_t x0, int32_t y0, int32_t x1, int32_t y1) {
    int32_t n = lw(MAP + 0xb0) - 1;
    uint32_t s = lwu(MAP + 0x1c);
    if (n == -1) return;
    const uint32_t xy = ee_stack_alloc(0x10);
    do {
        sub_0024a7e8(fp(xy), fp(xy + 4), lw(0x0015ed84), lwc1(s), lwc1(s + 4));
        const int32_t x = x0 + cvt_w_s(fpu_mul(static_cast<float>(x1 - x0), lwc1(xy)));
        const int32_t y = y0 + cvt_w_s(fpu_mul(static_cast<float>(y1 - y0), lwc1(xy + 4)));
        if (!(x < -0x199) && !(y < -0x199) && x < 0x219a && y < 0x1b9a) {
            const int32_t frame = lw(s + 8);
            if (frame == -1) {
                const int32_t r = lw(OV(0x0015fe48));
                gs_flat_rect(x - r, y - r, x + r, y + r, lwu(s + 0xc), 1);
            } else {
                const int32_t f = hud_frame(static_cast<uint32_t>(frame), lw(s + 0xc));
                const float z = lwc1(MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x224)) * 4);
                const uint32_t tex = lwu(HUD + 0x24) + static_cast<uint32_t>(lh(static_cast<uint32_t>(f) * 4 + lwu(HUD + 0x20) + 2)) * 8;
                const float k = fpu_div(fpu_add(fpu_add(z, z), 5.0f), 13.0f);
                const uint32_t tw = lbu(tex + 6), th = lbu(tex + 7);
                const int32_t hx = cvt_w_s(fpu_mul(k, static_cast<float>(static_cast<int32_t>(1u << ((tw + 3) & 31)))));
                const int32_t hy = cvt_w_s(fpu_mul(k, static_cast<float>(static_cast<int32_t>(1u << ((th + 3) & 31)))));
                const int32_t ww = cvt_w_s(fpu_mul(k, static_cast<float>(static_cast<int32_t>(1u << ((tw + 4) & 31)))));
                const int32_t hh = cvt_w_s(fpu_mul(k, static_cast<float>(static_cast<int32_t>(1u << ((th + 4) & 31)))));
                hud_sprite_xyz(f, x - hx, y - hy, ww, hh, 0x80);
            }
        }
        s += 0x10;
        --n;
    } while (n != -1);
    ee_stack_free(0x10);
}

// The map of the planet drawn (+0x228): its image (HUD frame 0xe999 of the planet) scaled by the
// zoom and scrolled, the explored mask over it (the GS buffer +0x240), the marker slots' squares
// (+0x2c), the markers (their places +0x18 / +0x1c on the map, overlaps pushed apart, rotated ones
// with flag 0x100 and their turns 0x400 / 0x800 / 0x1000, their text boxes), the hero's arrow on
// the hero's planet, the marker slots (+0x1c). Without a map, a debug text (0x1e8b18).
// orig: 0x002471a8
void planet_map_draw() {
    if (lw(MAP + 0x24) == 0) {
        texture_block_begin(0);
        debug_text_centred(0x100, static_cast<int32_t>(static_cast<int16_t>(lhu(0x001518d2))) >> 1, 0x80909090u, OV(0x001e8b18));
        texture_block_end();
        return;
    }
    if (lw(MAP + 0x228) < 0) return;
    texture_block_begin(0);
    const int32_t sign = lbu(0x0015edb4) != 0 ? -1 : 1;
    const uint32_t i4 = static_cast<uint32_t>(lw(MAP + 0x228)) * 4;
    const float z = lwc1(MAP + 0xb4 + i4);
    const int32_t sx = lw(MAP + 0x104 + i4) >> 15, sy = lw(MAP + 0x154 + i4) >> 15;
    const int32_t zx = cvt_w_s(fpu_mul(z, static_cast<float>(sx)));
    const int32_t zw = cvt_w_s(fpu_mul(z, 8192.0f));
    const int32_t zy = cvt_w_s(fpu_mul(z, static_cast<float>(sy)));
    const int32_t step = cvt_w_s(fpu_mul(z, 512.0f));
    const int32_t y0 = 0x800 - zy;
    const int32_t y1 = y0 + zw;
    const int32_t x0 = 0x1000 - zx * sign;
    const int32_t x1 = x0 + zw * sign;
    const int32_t c0 = ee_div(x0 + step - 1, step);
    const int32_t r0 = ee_div(y0 + step - 1, step);
    const int32_t c1 = ee_div(step + 0x2000 - x1 - 1, step);
    const int32_t r1 = ee_div(step + 0x2000 - y1 - 1, step);
    const int32_t px0 = x0 - step * c0, py0 = y0 - step * r0;
    const int32_t px1 = x1 + step * c1, py1 = y1 + step * r1;
    const int32_t u1 = ((c0 + c1) << 9) + 0x2000;
    const int32_t v1 = ((r0 + r1) << 9) + 0x2000;
    sub_002a0ba8(8, 0);
    sub_002a0ba8(0x47, 0);
    uint32_t p = lwu(CUR);
    sw(p, 0x10000005);
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000005);
    sw(CUR, lwu(CUR) + 0x10);
    const uint64_t tex = hud_tex0(hud_frame(0xe999, lw(MAP + 0x228)));
    uint32_t q = lwu(CUR);
    sd(q + 8, 0x05353106);
    sd(q + 0x10, tex);
    sd(q, 0x7400000000008001ull);
    sd(q + 0x18, 0x156);
    sd(q + 0x20, 0x80808080ull);
    sd(q + 0x28, 0);
    const int32_t zz = lw(MAP + 0x18);
    sd(q + 0x38, sx64(u1) | (sx64(v1) << 16));
    sd(q + 0x30, xyz(px0, py0, zz));
    sd(q + 0x48, 0);
    sd(q + 0x40, xyz(px1, py1, zz));
    sw(CUR, lwu(CUR) + 0x50);
    sub_002a0ba8(8, 5);
    sub_002a0ba8(0x47, 0x60b);
    // the explored mask: the same rectangle, the texture at the GS address +0x240
    p = lwu(CUR);
    sw(p, 0x10000005);
    const uint64_t cbp = ((tex >> 37) & 0x3fff) << 37;
    sw(p + 4, 0);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000005);
    const uint64_t mtex = sx64(lw(MAP + 0x240) >> 8) | 0x25320000ull | cbp | 0x640000000ull | 0x8000000000000000ull;
    q = p + 0x10;
    sw(CUR, q);
    sd(q, 0x7400000000008001ull);
    sd(q + 8, 0x05353106);
    sd(q + 0x10, mtex);
    sd(q + 0x18, 0x156);
    sd(q + 0x20, 0x80808080ull);
    sd(q + 0x28, 0);
    sd(q + 0x38, 0x20002000);
    sd(q + 0x30, xyz(x0, y0, lw(MAP + 0x18)));
    sd(q + 0x48, 0);
    sd(q + 0x40, xyz(x1, y1, lw(MAP + 0x18)));
    sw(CUR, lwu(CUR) + 0x50);
    sub_002a0ba8(0x47, 0x360b);
    if (lw(MAP + 0x2c) != 0 && lw(MAP + 8) != 0) {
        const int32_t w = x1 - x0, h = y1 - y0;
        for (uint32_t k = 0; k < 8; ++k) {
            const int32_t a = lw(MAP + 0x30 + k * 0x10);
            if (a < 0) continue;
            const int32_t row = a / 16, col = a - (a / 16) * 16;
            gs_flat_rect(x0 + (col * w) / 16, y0 + (row * h) / 16, x0 + (col * w + w) / 16, y0 + (row * h + h) / 16, 0x20000000ull, 1);
        }
    }
    if (lwu(MAP + 0x20) != 0) {
        const float zoom = lwc1(MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4);
        const float base = fpu_div(fpu_add(fpu_add(zoom, zoom), 5.0f), 13.0f);
        // the markers' rectangles on the scratchpad
        if ((lhu(lwu(MAP + 0x20) + 4) & 4) == 0) {
            uint32_t off = 0, out = SPR;
            do {
                const uint32_t r = lwu(MAP + 0x20) + off;
                if (lw(r + 0x24) != 0 && lhu(r + 6) != 0 && (lhu(r + 4) & 1) == 0) {
                    const float big = (lhu(r + 4) & 0x80) != 0 ? 1.5f : 1.0f;
                    const int32_t f = hud_frame(lhu(r + 6), lh(r + 8));
                    const uint32_t r2 = lwu(MAP + 0x20) + off;
                    const float fx = fpu_add(static_cast<float>(x0), fpu_mul(lwc1(r2 + 0x18), static_cast<float>(x1 - x0)));
                    const float fy = fpu_add(static_cast<float>(y0), fpu_mul(lwc1(r2 + 0x1c), static_cast<float>(y1 - y0)));
                    const uint32_t tx = lwu(HUD + 0x24) + static_cast<uint32_t>(lh(static_cast<uint32_t>(f) * 4 + lwu(HUD + 0x20) + 2)) * 8;
                    const float s = fpu_mul(big, (lhu(r2 + 4) & 0x200) != 0 ? lwc1(MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4) : base);
                    const int32_t ax = cvt_w_s(fpu_sub(fx, fpu_mul(s, static_cast<float>(static_cast<int32_t>(1u << ((lbu(tx + 6) + 3) & 31))))));
                    sw(out, static_cast<uint32_t>(ax));
                    const int32_t bx = cvt_w_s(fpu_add(static_cast<float>(ax), fpu_mul(s, static_cast<float>(static_cast<int32_t>(1u << ((lbu(tx + 6) + 4) & 31))))));
                    sw(out + 8, static_cast<uint32_t>(bx));
                    const int32_t ay = cvt_w_s(fpu_sub(fy, fpu_mul(s, static_cast<float>(static_cast<int32_t>(1u << ((lbu(tx + 7) + 3) & 31))))));
                    sw(out + 4, static_cast<uint32_t>(ay));
                    const int32_t by = cvt_w_s(fpu_add(static_cast<float>(ay), fpu_mul(s, static_cast<float>(static_cast<int32_t>(1u << ((lbu(tx + 7) + 4) & 31))))));
                    sw(out + 0xc, static_cast<uint32_t>(by));
                }
                off += 0x28;
                out += 0x10;
            } while ((lhu(lwu(MAP + 0x20) + off + 4) & 4) == 0);
        }
        // overlapping markers pushed apart (along their smallest overlap)
        if ((lhu(lwu(MAP + 0x20) + 0x2c) & 4) == 0) {
            int32_t i = 0;
            do {
                const uint32_t ri = lwu(MAP + 0x20) + static_cast<uint32_t>(i * 0x28);
                const int32_t j0 = i + 1;
                if (lw(ri + 0x24) != 0 && lhu(ri + 6) != 0 && (lhu(ri + 4) & 3) == 0 &&
                    (lhu(lwu(MAP + 0x20) + static_cast<uint32_t>(j0 * 0x28) + 4) & 4) == 0) {
                    const uint32_t a = SPR + static_cast<uint32_t>(i) * 16;
                    uint32_t b = SPR + static_cast<uint32_t>(j0) * 16;
                    uint32_t jo = static_cast<uint32_t>(j0 * 0x28);
                    do {
                        const int32_t t0 = lw(b + 8) - lw(a);
                        const int32_t t1 = lw(a + 8) - lw(b);
                        const int32_t t2 = lw(b + 0xc) - lw(a + 4);
                        const int32_t t3 = lw(a + 0xc) - lw(b + 4);
                        const uint32_t rj = lwu(MAP + 0x20) + jo;
                        if (t0 > 0 && t1 > 0 && t2 > 0 && t3 > 0 && lw(rj + 0x24) != 0 && lhu(rj + 6) != 0 && (lhu(rj + 4) & 3) == 0) {
                            int32_t ax = 0, ay = 0, bx = 0, by = 0;
                            if (!(t1 < t0) && !(t2 < t0) && !(t3 < t0)) {
                                ax = t0 >> 1;
                                bx = ax - t0;
                            } else if (!(t2 < t1) && !(t3 < t1)) {
                                bx = t1 >> 1;
                                ax = bx - t1;
                            } else if (!(t3 < t2)) {
                                ay = t2 >> 1;
                                by = ay - t2;
                            } else {
                                by = t3 >> 1;
                                ay = by - t3;
                            }
                            sw(a, static_cast<uint32_t>(lw(a) + ax));
                            sw(a + 8, static_cast<uint32_t>(lw(a + 8) + ax));
                            sw(a + 4, static_cast<uint32_t>(lw(a + 4) + ay));
                            sw(a + 0xc, static_cast<uint32_t>(lw(a + 0xc) + ay));
                            sw(b, static_cast<uint32_t>(lw(b) + bx));
                            sw(b + 8, static_cast<uint32_t>(lw(b + 8) + bx));
                            sw(b + 4, static_cast<uint32_t>(lw(b + 4) + by));
                            sw(b + 0xc, static_cast<uint32_t>(lw(b + 0xc) + by));
                        }
                        jo += 0x28;
                        b += 0x10;
                    } while ((lhu(lwu(MAP + 0x20) + jo + 4) & 4) == 0);
                }
                i = j0;
            } while ((lhu(lwu(MAP + 0x20) + static_cast<uint32_t>(i * 0x28) + 0x2c) & 4) == 0);
        }
        // the markers
        if ((lhu(lwu(MAP + 0x20) + 4) & 4) == 0) {
            uint32_t off = 0, rect = SPR;
            int32_t idx = 0;
            const uint32_t sp = ee_stack_alloc(0xa0);  // the text +0, the box +0x80
            do {
                const uint32_t r = lwu(MAP + 0x20) + off;
                if (lw(r + 0x24) != 0 && (lhu(r + 4) & 1) == 0 && lhu(r + 6) != 0) {
                    const uint32_t key = lhu(r + 6);
                    int32_t fi = lh(r + 8);
                    if ((lhu(r + 4) & 0x40) != 0)
                        gs_flat_rect(lw(rect) - 0x20, lw(rect + 4) - 0x20, lw(rect + 8) + 0x20, lw(rect + 0xc) + 0x20, 0x80000000ull, 1);
                    float s = (lhu(lwu(MAP + 0x20) + off + 4) & 0x200) != 0 ? lwc1(MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4) : base;
                    const uint32_t r3 = lwu(MAP + 0x20) + off;
                    if ((lhu(r3 + 4) & 0x100) != 0) {
                        float ang = lwc1(r3 + 0x20);
                        int32_t size = 0x20;
                        float hw = fpu_mul(s, 256.0f), hh = hw;
                        if ((lhu(r3 + 4) & 0x400) != 0) {
                            size = 0x40;
                            ang = angle_add(ang, HALF_PI);
                            const uint32_t r4 = lwu(MAP + 0x20) + off;
                            hw = fpu_mul(s, lwc1(OV(0x0015fe20)));
                            hh = fpu_mul(s, lwc1(OV(0x0015fe24)));
                            if ((lwu(0x0013d5bc + static_cast<uint32_t>(lh(r4) << 4)) & 2) != 0) fi = fi + 1;
                        }
                        if ((lhu(lwu(MAP + 0x20) + off + 4) & 0x800) != 0) {
                            size = 0x40;
                            ang = angle_add(ang, HALF_PI);
                            const uint32_t r4 = lwu(MAP + 0x20) + off;
                            hw = fpu_mul(s, lwc1(OV(0x0015fe18)));
                            hh = fpu_mul(s, lwc1(OV(0x0015fe1c)));
                            if ((lwu(0x0013d5bc + static_cast<uint32_t>(lh(r4) << 4)) & 2) != 0) fi = fi + 1;
                        }
                        if ((lhu(lwu(MAP + 0x20) + off + 4) & 0x1000) != 0) {
                            ang = angle_add(ang, HALF_PI);
                            hw = fpu_mul(s, lwc1(OV(0x0015fe28)));
                            hh = fpu_mul(s, lwc1(OV(0x0015fe2c)));
                        }
                        const float cx = fpu_mul(static_cast<float>(lw(rect) + lw(rect + 8)), 0.5f);
                        const float cy = fpu_mul(static_cast<float>(lw(rect + 4) + lw(rect + 0xc)), 0.5f);
                        const uint64_t t = hud_tex0(hud_frame(key, fi));
                        sprite_rotated(size, 0x20, t, cx, cy, hw, hh, ang);
                    } else {
                        const int32_t f = hud_frame(key, fi);
                        hud_sprite_xyz(f, lw(rect), lw(rect + 4), lw(rect + 8) - lw(rect), lw(rect + 0xc) - lw(rect + 4), 0x80);
                    }
                    const uint32_t r5 = lwu(MAP + 0x20) + off;
                    if ((lhu(r5 + 4) & 0x10) != 0) {
                        const int32_t bw = static_cast<int16_t>(lhu(r5 + 0xe));
                        const int32_t dx = lh(r5 + 0x12), dy = lh(r5 + 0x14), bh = lh(r5 + 0x10);
                        int32_t tx, ty;
                        if (dx == 0) tx = ((lw(rect) + lw(rect + 8)) >> 5) - (bw / 2);
                        else tx = ((dx > 0 ? lw(rect + 8) : lw(rect)) >> 4) - (bw / 2) + dx;
                        if (dy == 0) ty = ((lw(rect + 4) + lw(rect + 0xc)) >> 5) - (bh / 2);
                        else ty = ((dy > 0 ? lw(rect + 0xc) : lw(rect + 4)) >> 4) - (bh / 2) + dy;
                        const int32_t ty1 = ty + bh, tx1 = tx + bw;
                        box_rounded(ty, ty1, tx, tx1, 0x40);
                        map_marker_text(idx, sp);
                        const uint32_t box = sp + 0x80;
                        libc_memset(box, 0, 0x18);
                        sh(box + 0x10, 0xf);
                        sh(box + 2, static_cast<uint32_t>(ty1));
                        sh(box + 6, static_cast<uint32_t>(tx1));
                        sh(box + 8, static_cast<uint32_t>(tx + bw / 2));
                        sh(box + 0xa, static_cast<uint32_t>(ty + 4));
                        sh(box + 0x12, 1);
                        sh(box, static_cast<uint32_t>(ty));
                        sh(box + 4, static_cast<uint32_t>(tx));
                        text_box_draw(box, 0x80ffa888ull, sp, -1);
                    }
                }
                off += 0x28;
                ++idx;
                rect += 0x10;
            } while ((lhu(lwu(MAP + 0x20) + off + 4) & 4) == 0);
            ee_stack_free(0xa0);
        }
    }
    // the hero's arrow on the hero's planet
    if (lw(0x0015ed84) == lw(MAP + 0x228)) {
        const int32_t f = hud_frame(0xe99a, 5);
        const float zoom = lwc1(MAP + 0xb4 + static_cast<uint32_t>(lw(MAP + 0x228)) * 4);
        const float k = fpu_div(fpu_mul(fpu_add(fpu_mul(zoom, 4.0f), 10.0f), 0.75f), 13.0f);
        float ang = lwc1(0x0013f3e8);
        if ((lw(0x001413dc) ^ 0xf) == 0) ang = angle_add(ang, HALF_PI);
        const uint32_t xy = ee_stack_alloc(0x10);
        if (lw(OV(0x0015fdf8)) != 0) {
            sub_0024a7e8(fp(xy), fp(xy + 4), lw(0x0015ed84) + 0x64, lwc1(0x0013f3d0), lwc1(0x0013f3d4));
            ang = angle_add(ang, HALF_PI);
        } else {
            sub_0024a7e8(fp(xy), fp(xy + 4), lw(0x0015ed84), lwc1(0x0013f3d0), lwc1(0x0013f3d4));
        }
        const float w = static_cast<float>(x1 - x0), h = static_cast<float>(y1 - y0);
        swc1(xy, fpu_add(static_cast<float>(x0), fpu_mul(lwc1(xy), w)));
        swc1(xy + 4, fpu_add(static_cast<float>(y0), fpu_mul(lwc1(xy + 4), h)));
        if (lbu(0x0015edb4) != 0) ang = angle_sub(-angle_add(ang, HALF_PI), HALF_PI);
        const float sz = fpu_mul(k, 256.0f);
        sprite_rotated(0x40, 0x40, hud_tex0(f), lwc1(xy), lwc1(xy + 4), sz, sz, ang);
        ee_stack_free(0x10);
    }
    texture_block_end();
    if (lwu(MAP + 0x1c) != 0) {
        texture_block_begin(0);
        map_slots_draw(x0, y0, x1, y1);
        texture_block_end();
    }
}

// The map widget's draw: 0x2471a8 into its render target; 8.
// orig: 0x0027cf78
int32_t planet_map_widget_draw(uint32_t) {
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0xb);
    planet_map_draw();
    return 8;
}

// The planet picture: its three layers (TEX0 +0x258 / +0x260 / +0x268) over the whole target,
// scrolling (by 0x2ab a planet, 0x15f3f8 frames, the margin 0x1c2068 of the planet) or, for the
// planets 6 / 13 / 17, the second and third blinking; 4.
// orig: 0x0027e498
int32_t planet_picture_draw(uint32_t) {
    if (level_slots_map_row()) {  // enhancement: a level mod's row: its picture as it is, still (src/host/level_slots.h)
        if (!level_slots_map_planet_shown()) return 0;
        sub_002a0ba8(0x47, 0);  // (as the planets' surface: over the whole target, nothing round it)
        sub_002a0ba8(8, 0);
        sprite_uv(ld(MAP + 0x258), 0, 0, 7, 7, lh(0x00151780 + 0x160) << 4, lh(0x00151780 + 0x162) << 4, 0, 0, 0x80);
        return 4;
    }
    const int32_t pl = lw(MAP + 0x228);
    if (pl < 0) return 0;
    const int32_t w = lh(0x00151780 + 0x160) << 4, h = lh(0x00151780 + 0x162) << 4;
    if (pl == 6 || pl == 0xd || pl == 0x11) {
        sub_002a0ba8(0x47, 0);
        sub_002a0ba8(8, 5);
        sprite_uv(ld(MAP + 0x258), 0, 0, 7, 7, w, h, 0, 0, 0x80);
        sub_002a0ba8(0x47, 0x360b);
        if (ee_rem(lw(OV(0x0015f3f8)), 0x3c) < 0x28) sprite_uv(ld(MAP + 0x260), 0, 0, 7, 7, lh(0x00151780 + 0x160) << 4, lh(0x00151780 + 0x162) << 4, 0, 0, 0x80);
        if (ee_rem(lw(OV(0x0015f3f8)), 0x96) < 0x5a) sprite_uv(ld(MAP + 0x268), 0, 0, 7, 7, lh(0x00151780 + 0x160) << 4, lh(0x00151780 + 0x162) << 4, 0, 0, 0x80);
        return 4;
    }
    const int32_t t = lw(OV(0x0015f3f8)) + pl * 0x2ab;
    const int32_t m = lw(OV(0x001c2068) + static_cast<uint32_t>(pl) * 4);
    const int32_t u = t - (((t < 0 ? t + 0x7ff : t) >> 11) << 11);
    sub_002a0ba8(0x47, 0);
    sub_002a0ba8(8, 0);
    sprite_uv(ld(MAP + 0x258), m, m, 7, 7, (lh(0x00151780 + 0x160) << 4) - m * 2, (lh(0x00151780 + 0x162) << 4) - m * 2, u, 0, 0x80);
    sub_002a0ba8(0x47, 0x360b);
    sub_002a0ba8(8, 5);
    sprite_uv(ld(MAP + 0x260), m, m, 7, 7, (lh(0x00151780 + 0x160) << 4) - m * 2, (lh(0x00151780 + 0x162) << 4) - m * 2, 0, 0, 0x80);
    sprite_uv(ld(MAP + 0x268), m, m, 7, 7, (lh(0x00151780 + 0x160) << 4) - m * 2, (lh(0x00151780 + 0x162) << 4) - m * 2, 0, 0, 0x80);
    return 4;
}

// The map's legend: the key's texts (0x4eee, on a level 0x4eef, 0x4efa..0x4efc, 0x4ee0) in lines
// sharing the box, left aligned on the widest.
// orig: 0x0027cfb0
int32_t planet_map_legend_draw(uint32_t w) {
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0xb);
    texture_block_begin(0);
    int32_t wd = text_width_med(sub_00203ad8(0x4eee), -1);
    int32_t v = text_width_med(sub_00203ad8(0x4efa), -1);
    if (!(v < wd)) wd = v;
    if (lw(0x0015ed84) != 0) {
        v = text_width_med(sub_00203ad8(0x4eef), -1);
        if (!(v < wd)) wd = v;
    }
    for (int32_t id : {0x4efb, 0x4efc, 0x4ee0}) {
        v = text_width_med(sub_00203ad8(id), -1);
        if (!(v < wd)) wd = v;
    }
    int32_t x = (lw(w + 0x20) - wd) >> 1;
    if (!(1 < x)) x = 2;
    const int32_t step = ee_div(lw(w + 0x24), lw(0x0015ed84) != 0 ? 7 : 6);
    text_colour_codes_off();
    int32_t y = step - 6;
    text_med(x, y, 0x80ffa888ull, sub_00203ad8(0x4eee), -1);
    if (lw(0x0015ed84) != 0) {
        y += step;
        text_med(x, y, 0x80ffa888ull, sub_00203ad8(0x4eef), -1);
    }
    for (int32_t id : {0x4efa, 0x4efb, 0x4efc, 0x4ee0}) {
        y += step;
        text_med(x, y, 0x80ffa888ull, sub_00203ad8(id), -1);
    }
    sub_001fb028();
    texture_block_end();
    return 2;
}

}  // namespace rac1
