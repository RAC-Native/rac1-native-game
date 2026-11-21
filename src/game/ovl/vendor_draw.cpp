// The Vendor's screens' draw (game mode 5): the frame of the game rendered once and its picture stored (the screens'
// background, put back every later frame), the Vendor and the selected item's hologram, the Vendor's six screens (each
// drawn into a 512 x 128 texture, then laid on the screen's place on the Vendor model with its static and its glass), the
// quantity box, the HUD. See vendor_screens.h.
//
// The GS packets go into the frame's packet buffer (its cursor 0x1611c0, CUR below) word for word as the original writes
// them: the oracle compares all of the EE memory.
#include <game/ovl/vendor_screens.h>

#include <game/enhance/fixes102.h>
#include <game/enhance/vendor_buy_all.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/save_menus.h>
#include <game/ovl/vendor_frame.h>
#include <game/ovl/vendor_tr.h>
#include <game/port_stubs.h>

namespace rac1 {

using namespace vendor;

namespace {
constexpr OvConst<0x001611c0u> CUR{};  // the frame's packet buffer's cursor
constexpr uint32_t SCREEN_OFS_X = 0x0013e510u, SCREEN_OFS_Y = 0x0013e514u;  // the GS's drawing offset (pixels)
constexpr uint32_t GREY = NOT_ADDR(0x202020u);
constexpr uint64_t WHITE = 0x80f0f0f0u;  // the screens' text colour
// The GS registers the screens set with sub_002a0ba8 (an A+D write into the packet buffer)
constexpr uint32_t GS_TEX1 = 0x14, GS_ALPHA = 0x42, GS_TEST = 0x47, GS_CLAMP = 0x08;
constexpr uint64_t ALPHA_BLEND = 0x8000000044ull, ALPHA_SCREEN = 0x8000000064ull;
// The texture of the frame picture rendered into (0x15eed0)
constexpr uint32_t TARGET_TEX0 = 0x0015eed0u;


// The calls the Vendor's "All Ammo" entry steps in at (vendor_buy_all.h), by the original call's return address
constexpr uint32_t SITE_SIGN_GLYPHS = NOT_ADDR(0x0029db1cu), SITE_SIGN_PHRASE = NOT_ADDR(0x0029dd68u),
                   SITE_STRIP_AMMO = NOT_ADDR(0x0029dfc0u), SITE_STRIP_WEAPON = NOT_ADDR(0x0029dfd0u),
                   SITE_STRIP_SPRITE = NOT_ADDR(0x0029dfecu), SITE_WHEEL_AMMO = NOT_ADDR(0x0029e124u),
                   SITE_WHEEL_WEAPON = NOT_ADDR(0x0029e134u), SITE_WHEEL_SPRITE = NOT_ADDR(0x0029e158u),
                   SITE_PRICE_THOUSANDS = NOT_ADDR(0x0029de80u), SITE_PRICE = NOT_ADDR(0x0029de94u),
                   SITE_MONITOR_AMMO_NAME = NOT_ADDR(0x0029e248u), SITE_MONITOR_AMMO = NOT_ADDR(0x0029e270u),
                   SITE_MONITOR_WEAPON_NAME = NOT_ADDR(0x0029e48cu), SITE_BOX_TEXT = NOT_ADDR(0x0029e89cu),
                   SITE_BOX_TEXT2 = NOT_ADDR(0x0029e8e4u), SITE_PROMPT = NOT_ADDR(0x0029ea78u),
                   SITE_PROMPT_EMPTY = NOT_ADDR(0x0029eaa8u), SITE_QUANTITY_TITLE = NOT_ADDR(0x0029ec90u),
                   SITE_QUANTITY_COUNT = NOT_ADDR(0x0029ecc4u), SITE_QUANTITY_TOTAL = NOT_ADDR(0x0029edf8u),
                   SITE_QUANTITY_MESSAGE = NOT_ADDR(0x0029ee28u), SITE_CONFIRM_1 = NOT_ADDR(0x0029eea8u),
                   SITE_CONFIRM_2 = NOT_ADDR(0x0029eed0u), SITE_CONFIRM_3 = NOT_ADDR(0x0029eef8u),
                   SITE_CONFIRM_NO_BOLTS = NOT_ADDR(0x0029ef28u);

// orig: (helper) a 32-bit value as the R5900 keeps it in a 64-bit register (sign-extended)
constexpr uint64_t sx(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
constexpr uint64_t sx(uint32_t v) { return sx(static_cast<int32_t>(v)); }

// orig: (helper) the entry at the cursor and its item
uint32_t cursor_entry() { return entry(lw(VS + CURSOR)); }
int32_t cursor_item() { return lw(cursor_entry() + E_ITEM); }
uint32_t item_rec(int32_t item) { return ITEMS + static_cast<uint32_t>(item) * 0x18; }
bool ammo_short(int32_t item) { return lw(AMMO + static_cast<uint32_t>(item) * 4) < static_cast<int32_t>(lhu(item_rec(item) + 0xe)); }
int32_t round_price(int32_t item) { return lhu(item_rec(item) + (lw(VS + PDA) != 0 ? 0xau : 0x8u)); }
int32_t weapon_price(int32_t item) { return lw(item_rec(item) + (lbu(SECOND_PRICES) != 0 ? 4u : 0u)); }
uint32_t text(uint32_t site, int32_t id) { return vendor_buy_all_text(site, id); }

// orig: (helper) a CNT + DIRECT tag of `qwc` quadwords at `p`
void cnt_direct(uint32_t p, uint32_t qwc) {
    sw(p + 0x0, 0x10000000u | qwc);
    sw(p + 0x4, 0);
    sw(p + 0x8, 0);
    sw(p + 0xc, 0x50000000u | qwc);
}
}  // namespace

namespace vendor {

// A flat rectangle (x0, y0)..(x1, y1) of the render target `tw` x `th` (pixels from its corner), colour `rgba`,
// blended (ALPHA 0x64 meanwhile).
// orig: 0x00201560
void gs_target_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t tw, int32_t th, uint32_t rgba) {
    dma_cnt_vif(0x13000000u);
    sub_002a0ba8(GS_ALPHA, 0x64);
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 6);
    const int32_t gy0 = (y0 << 4) + 0x8000 - (th << 3), gy1 = (y1 << 4) + 0x8000 - (th << 3);
    const int32_t gx0 = (x0 << 4) + 0x8000 - (tw << 3), gx1 = (x1 << 4) + 0x8000 - (tw << 3);
    sw(CUR, p + 0x10);
    sd(p + 0x10, 0x1000000000000001ull);  // GIF tag: one A+D (TEST)
    sd(p + 0x18, 0xe);
    sd(p + 0x20, 0x33003);
    sd(p + 0x28, GS_TEST);
    sd(p + 0x30, 0x2400000000000001ull);  // GIF tag: PRIM, RGBAQ
    sd(p + 0x38, 0x10);
    sd(p + 0x40, 0x106);  // a sprite, its UVs in texels
    sd(p + 0x48, rgba);
    sd(p + 0x50, 0x2400000000008001ull);  // GIF tag (EOP): XYZ2, XYZ2
    sd(p + 0x58, 0x44);
    sd(p + 0x60, sx(gx0) | (sx(gy0) << 16));
    sd(p + 0x68, sx(gx1) | (sx(gy1) << 16));
    sw(CUR, lwu(CUR) + 0x60);
    sub_002a0ba8(GS_ALPHA, ALPHA_BLEND);
    dma_cnt_vif(0x13000000u);
}

// The render target `w` x `h` cleared to 0x80008080 by sprites 32 pixels wide (ALPHA 0x4a meanwhile).
// orig: 0x002013b0
void gs_target_clear(int32_t w, int32_t h) {
    const int32_t strips = w / 32;
    sub_002a0ba8(GS_ALPHA, 0x800000004aull);
    const uint32_t p = lwu(CUR);
    cnt_direct(p, static_cast<uint32_t>(strips + 5));
    const uint32_t q = p + 0x10;
    sw(CUR, q);
    sd(p + 0x10, 0x1000000000000001ull);  // GIF tag: one A+D (TEST)
    sd(p + 0x58, 0x44);
    sd(p + 0x18, 0xe);
    sd(p + 0x20, 0x32003);
    sd(p + 0x28, GS_TEST);
    sd(p + 0x30, 0x2400000000000001ull);  // GIF tag: PRIM, RGBAQ
    sd(p + 0x38, 0x10);
    sd(p + 0x40, 0x146);  // a sprite, its UVs in texels, blended
    sd(p + 0x48, 0x80008080ull);
    sd(p + 0x50, 0x2400000000008000ull | sx(strips | 0x8000));  // GIF tag (EOP): `strips` x (XYZ2, XYZ2)
    if (strips > 0) {
        const uint64_t top = sx(0x8000 - (h << 3)) << 16, bottom = sx((h << 3) + 0x7ff0) << 16;
        int32_t left = -(w << 3) + 0x8000, right = -(w << 3) + 0x8200;
        uint32_t d = p + 0x60;
        for (int32_t i = 0; i < strips; ++i, d += 0x10) {
            sd(d, sx(left) | top);
            sd(d + 8, sx(right) | bottom);
            left += 0x200;
            right += 0x200;
        }
    }
    sw(CUR, lwu(CUR) + static_cast<uint32_t>(strips) * 16 + 0x50);
}

// A textured quad (a triangle strip) at (x, y), w x h pixels (floats), its texels (u, v)..(u + du, v + dv), colour
// `rgba`, texture `tex0`.
// orig: 0x001fa418
void gs_textured_quad(int32_t u, int32_t v, int32_t du, int32_t dv, uint64_t rgba, uint64_t tex0, float x, float y, float w, float h) {
    const int32_t x0 = float_to_int(fpu_mul(x, 16.0f)) + lw(SCREEN_OFS_X) - 8;
    const float right = fpu_add(x, w);
    const int32_t x1 = float_to_int(fpu_mul(right, 16.0f)) + lw(SCREEN_OFS_X) - 8;
    const int32_t y0 = float_to_int(fpu_mul(y, 16.0f)) + lw(SCREEN_OFS_Y) - 8;
    const float bottom = fpu_add(y, h);
    const int32_t y1 = float_to_int(fpu_mul(bottom, 16.0f)) + lw(SCREEN_OFS_Y) - 8;
    const uint32_t p = lwu(CUR);
    cnt_direct(p, 7);
    sw(CUR, p + 0x10);
    lq_sq(p + 0x10, OV(0x00160900));  // the GIF tag (REGLIST: TEX0, PRIM, then RGBAQ and four UV / XYZ2 pairs)
    sw(CUR, p + 0x20);
    constexpr uint64_t Z = 0x00fffff000000000ull;
    const auto xyz = [](int32_t px, int32_t py) { return sx(px) | (sx(py) << 16) | Z; };
    const auto uv = [](int32_t tu, int32_t tv) { return sx((static_cast<uint32_t>(tv) << 20) + (static_cast<uint32_t>(tu) << 4)); };
    sd(p + 0x20, tex0);
    sd(p + 0x28, 0x154);  // PRIM: a triangle strip, textured, blended, its UVs in texels
    sd(p + 0x30, rgba);
    sd(p + 0x38, uv(u, v));
    sd(p + 0x40, xyz(x0, y0));
    sd(p + 0x48, uv(u + du, v));
    sd(p + 0x50, xyz(x1, y0));
    sd(p + 0x58, uv(u, v + dv));
    sd(p + 0x60, xyz(x0, y1));
    sd(p + 0x68, uv(u + du, v + dv));
    sd(p + 0x70, xyz(x1, y1));
    sd(p + 0x78, 0);
    sw(CUR, lwu(CUR) + 0x60);
}

// `text` drawn right-aligned at x (the medium font, the effect texture 2, the font 0x1c34f0).
// orig: 0x001fb888
void text_right(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n) {
    const int32_t w = text_width_med(s, n);
    const uint64_t tex = effect_tex0(2);
    text_draw(x - w, y, colour, s, n, tex, OV(0x001c34f0));
}

// The screen box of the bounds p0 / p1 seen from the camera (0x166dc0, its view 0x166d80; scaled by 1024, projected, the
// screen scale 0x16cbd0 / 0x16cbd4 and the centre 0x13e508 / 0x13e50c, 1/4) in floats (menu_box_project 0x299f40's in
// integers).
// orig: 0x0029a148
Box box_project(const Vec4& p0, const Vec4& p1) {
    Vec4 a = p0, b = p1;
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
    b.x = fpu_mul(b.x, fpu_div(1.0f, b.w));
    b.y = fpu_mul(b.y, fpu_div(1.0f, b.w));
    const float sx_ = lwc1(OV(0x0016cbd0)), sy_ = lwc1(OV(0x0016cbd4));
    a.x = fpu_mul(a.x, sx_);
    a.y = fpu_mul(a.y, sy_);
    b.x = fpu_mul(b.x, sx_);
    b.y = fpu_mul(b.y, sy_);
    Box r;
    r.x = fpu_add(fpu_mul(a.x, 0.25f), static_cast<float>(lw(0x0013e508)));
    r.y = fpu_add(fpu_mul(a.y, 0.25f), static_cast<float>(lw(0x0013e50c)));
    r.w = fpu_mul(fpu_sub(b.x, a.x), 0.25f);
    r.h = fpu_mul(fpu_sub(b.y, a.y), 0.25f);
    return r;
}

// A screen's texture as a render target of 2^lw x 2^lh (its frame buffer 0x3ff000 less its size, 8 KB aligned), the view
// for it (aspect `aspect`), TEST and ALPHA set.
// orig: 0x0029f2d0
void target_begin(int32_t lw_, int32_t lh_, float aspect) {
    const int32_t sum = lw_ + lh_ < 17 ? lw_ + lh_ : 16;
    const int32_t fbp = ((NOT_ADDR(0x3ff000) - (4 << sum)) >> 13) << 13;
    render_target_set(lw_, lh_, fbp);
    render_target_view(1 << lw_, 1 << lh_, aspect, 0.0f, 524288.0f, 255.0f, 0.0f);
    sub_002a0ba8(GS_TEST, 0x30000);
    sub_002a0ba8(GS_ALPHA, ALPHA_BLEND);
}

// The frame's display buffer back as the target.
// orig: 0x0029f390
void target_end() {
    packet_call_display();
    screen_setup();
    projection_setup();
}

// --- the six screens' panels (drawn into the 512 x 128 target) ------------------------------------------------------

// The sign's text in the marquee's font (0x1ca118: each glyph's texels, -1 for none; 0x1ca218: its advance), from x
// (scrolled) at y, its glyphs `scale` times 9 pixels; the characters ' '..'Z', 'b' blinking the next one (30 frames on,
// 10 off).
// orig: 0x0029dac8
void sign_glyphs(uint32_t s, int32_t x0, int32_t y, float scale) {
    const int32_t frame = vendor_buy_all_hud_frame(SITE_SIGN_GLYPHS, 0xe935, 0, static_cast<uint32_t>(y), 0);
    const uint64_t tex = hud_tex0(frame);
    int32_t x = x0;
    bool blink = false;
    // (the original's line break, the character 0x10a, can never be a byte)
    for (uint32_t c = lbu(s); c != 0; c = lbu(s)) {
        ++s;
        const int32_t g = static_cast<int32_t>(c) - 0x20;
        if (g == 0x42) blink = true;
        if (g >= 59) continue;
        const uint32_t glyph = OV(0x001ca118) + static_cast<uint32_t>(g) * 4;
        if (lw(glyph) == -1 && g != 0) continue;
        bool hidden = false;
        if (blink) {
            blink = false;
            const int32_t period = frame_scale_i(0x1e) + frame_scale_i(0xa);
            const int32_t t = ee_rem(lw(OV(0x0015f3f8)), period);
            hidden = t < frame_scale_i(0xa);
        }
        if (g != 0 && !hidden) {
            const uint32_t w = lwu(glyph);
            const int32_t tv = static_cast<int32_t>(w >> 20), tu = static_cast<int32_t>(w & 0xffff) >> 4;
            if (x + 9 > 0 && x < 256) {
                const int32_t size = float_to_int(fpu_mul(int_to_float(9), scale));
                const int32_t size2 = float_to_int(fpu_mul(int_to_float(9), scale));
                glyph_sprite(x, y, size, size2, tu, tv, 9, 9, 0x80404040u, tex);
            }
        }
        x += float_to_int(fpu_mul(int_to_float(lw(OV(0x001ca218) + static_cast<uint32_t>(g) * 4)), scale));
    }
}

// Screen 0, the sign: its text scrolling (a new phrase of the 24 at 0x1ca0b8 once it is through), its edges.
// orig: 0x0029dcd8
void panel_sign() {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    const int32_t len = static_cast<int32_t>(libc_strlen(lwu(VS + SIGN)));
    const int32_t scroll = lw(VS + SIGN_SCROLL);
    if (len - ee_div(scroll, 20) > 0) {
        sw(VS + SIGN_SCROLL, static_cast<uint32_t>(scroll + 2));
    } else {
        const uint32_t phrase = OV(0x001ca0b8) + static_cast<uint32_t>(rand_mod(0x18)) * 4;
        vendor_sign_text(text(SITE_SIGN_PHRASE, lw(phrase)));
    }
    sign_glyphs(lwu(VS + SIGN), -lw(VS + SIGN_SCROLL), 8, 2.0f);
    gs_target_rect(0, 0, 4, 0x40, 0x200, 0x80, 0);
    gs_target_rect(0xe2, 0, 0xe6, 0x40, 0x200, 0x80, 0);
}

// Screen 2, the Vendor's head.
// orig: 0x0029dde8
void panel_head() {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    if (lw(HEAD_UP) == 1) moby_draw_list(lwu(VS + HEAD), 1);
}

// A price into `dst`: "%d", or "%d,%03d" from 1000.
// orig: 0x0029de40
void price_format(uint32_t dst, int32_t v) {
    if (v < 1000) {
        vendor_buy_all_sprintf(SITE_PRICE, dst, OV(0x00161160), {sx(v)});
    } else {
        vendor_buy_all_sprintf(SITE_PRICE_THOUSANDS, dst, OV(0x00161158), {sx(ee_div(v, 1000)), sx(ee_rem(v, 1000))});
    }
}

// Screen 5, the item list: up to 7 entries a strip of tiles (the cursor's frame pulsing), from 8 a wheel of 11 tiles that
// scrolls to the cursor (the frame pulsing once it is still).
// orig: 0x0029dea0
void panel_list() {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    const auto pulse = [] { return static_cast<uint32_t>((abs_i(((lw(VS + FRAMES) << 2) & 0x3f) - 0x20) + 0x40) * 0x10202); };
    if (lw(VS + COUNT) < 8) {
        const int32_t x = lw(VS + CURSOR) * 0x38;
        gs_target_rect(x + 8, 2, x + 0x40, 0x3a, 0x200, 0x80, pulse() | 0x80000000u);
        gs_target_rect(x + 0xa, 4, x + 0x3e, 0x38, 0x200, 0x80, 0x80000000u);
        if (lw(VS + COUNT) <= 0) return;
        int32_t tx = 0xc;
        for (int32_t i = 0; i < lw(VS + COUNT); ++i, tx += 0x38) {
            const uint32_t e = entry(i);
            const uint32_t key = lhu(HUD_ITEMS + lwu(e + E_ITEM) * 0x4c + 0x38);
            const bool ammo = lw(e + E_AMMO) == 1;
            const int32_t frame = vendor_buy_all_hud_frame(ammo ? SITE_STRIP_AMMO : SITE_STRIP_WEAPON, key, ammo ? 2 : 0, e + E_AMMO, VS);
            vendor_buy_all_hud_sprite(SITE_STRIP_SPRITE, frame, tx, 6, 0x30, 0x30, 0x80);
        }
        return;
    }
    const int32_t scroll = lw(VS + SCROLL);
    if (scroll < 0) {
        sw(VS + SCROLL, static_cast<uint32_t>(scroll + 4));
    } else if (scroll > 0) {
        sw(VS + SCROLL, static_cast<uint32_t>(scroll - 4));
    } else {
        gs_target_rect(0xb0, 2, 0xe8, 0x3a, 0x200, 0x80, pulse() - 0x80000000u);
        gs_target_rect(0xb2, 4, 0xe6, 0x38, 0x200, 0x80, 0x80000000u);
    }
    int32_t tx = lw(VS + SCROLL) - 0x64;
    for (int32_t slot = -2; slot < 9; ++slot, tx += 0x38) {
        const int32_t n = lw(VS + COUNT);
        const int32_t k = ee_rem(n * 2 + slot + lw(VS + CURSOR) - 3, n);
        const uint32_t e = entry(k);
        const uint32_t key = lhu(HUD_ITEMS + lwu(e + E_ITEM) * 0x4c + 0x38);
        const bool ammo = lw(e + E_AMMO) == 1;
        const int32_t frame = vendor_buy_all_hud_frame(ammo ? SITE_WHEEL_AMMO : SITE_WHEEL_WEAPON, key, ammo ? 2 : 0,
                                                       static_cast<uint32_t>(tx), static_cast<uint32_t>(slot));
        vendor_buy_all_hud_sprite(SITE_WHEEL_SPRITE, frame, tx, 6, 0x30, 0x30, sx(0x80808080u));
    }
}

namespace {
// orig: (helper) a price struck out: seven red rows over the text right-aligned at 0x76 (`width` its width)
void price_strike(int32_t width) {
    for (int32_t k = 0; k < 7; ++k) {
        const uint64_t colour = static_cast<uint64_t>(0x20 + 0x10 * k) << 24 | 0x959544u;
        gs_flat_rect(0x75 + k - width, 0x6d, 0x7b - k, 0x70, colour, 0);
    }
}
}  // namespace

// Screen 1, the item monitor: the item and its hologram's ring, its name, its price (an ammo's for one round, "Ammo"
// under its name). Opened by the PDA an ammo's Vendor price is struck out over the PDA's; with 0x13d4e3 a weapon's first
// price is greyed and struck out over its second.
// orig: 0x0029e198
void panel_monitor() {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    moby_draw_list(lwu(VS + MOBYS) + 0x100, 1);
    moby_draw_list(lwu(VS + MOBYS), 1);
    const uint32_t buf = ee_stack_alloc(0x100);  // (the original's frame: the price's text)
    const int32_t item = cursor_item();
    if (lw(cursor_entry() + E_AMMO) == 1) {
        text_med(6, 8, WHITE, text(SITE_MONITOR_AMMO_NAME, lw(HUD_ITEMS + static_cast<uint32_t>(item) * 0x4c)), -1);
        text_med(0x18, 0x18, WHITE, text(SITE_MONITOR_AMMO, TXT(0x4f5d)), -1);
        price_format(buf, lhu(item_rec(cursor_item()) + 8));
        text_right(0x76, 0x65, WHITE, buf, -1);
        if (lw(VS + PDA) != 0) {
            price_strike(text_width_small(buf, -1));
            price_format(buf, round_price(cursor_item()));
            text_right(0x76, 0x55, WHITE, buf, -1);
        }
    } else {
        text_med(6, 8, WHITE, text(SITE_MONITOR_WEAPON_NAME, lw(HUD_ITEMS + static_cast<uint32_t>(item) * 0x4c)), -1);
        price_format(buf, lw(item_rec(cursor_item())));
        text_right(0x76, 0x65, lbu(SECOND_PRICES) == 0 ? WHITE : 0x80808080u, buf, -1);
        if (lbu(SECOND_PRICES) != 0) {
            price_strike(text_width_small(buf, -1));
            price_format(buf, weapon_price(cursor_item()));
            text_right(0x76, 0x55, WHITE, buf, -1);
        }
    }
    ee_stack_free(0x100);
}

namespace {
// orig: (helper) a text box record (0x18 bytes, text_box_draw 0x1fc4a8) cleared at `b`
void box_clear(uint32_t b) { libc_memset(b, 0, 0x18); }
}  // namespace

// Screen 3, the purchase's message box (w x h): while a purchase is under way, the text 0x5234 twice, the second centred
// under the first. (The North American build shows 0x5234 for each of its three cases: an ammo full 0x5233, short of
// bolts 0x5233, else 0x5234 for ammo and 0x524e for a weapon; the 1.02 build shows the case's own text.)
// orig: 0x0029e6c0
void panel_message(int32_t w, int32_t h) {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    const uint32_t b = ee_stack_alloc(0x20);
    box_clear(b);
    sh(b + 0x8, static_cast<uint32_t>(w >> 1));
    sh(b + 0xa, static_cast<uint32_t>((h >> 1) - 7));
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 1);
    sh(b + 0x2, static_cast<uint32_t>(h));
    sh(b + 0x6, static_cast<uint32_t>(w));
    int32_t msg = TXT(0x5234);
    if (lw(VS + STEP) != 0) {
        const int32_t item = cursor_item();
        if (lw(cursor_entry() + E_AMMO) == 1 && !ammo_short(item)) {
            msg = TXT(0x5233);
        } else if (lw(cursor_entry() + E_AMMO) == 1) {
            msg = lw(BOLTS) < round_price(cursor_item()) ? TXT(0x5233) : TXT(0x5234);
        } else {
            msg = lw(BOLTS) < weapon_price(cursor_item()) ? TXT(0x5233) : TXT(0x524e);
        }
    }
    // (1.02 Fixes, fix 26: the 1.02 build's 0x0029f9b0 shows the case's own text, and none for a weapon the bolts pay,
    // whose quantity box asks Yes / No)
    int32_t shown = TXT(0x5234);
    if (fixes102_on(26)) {
        if (msg == TXT(0x524e)) msg = 0;
        shown = msg;
    }
    if (msg != 0) {
        sh(b + 0x12, lhu(b + 0x12) | 4u);
        text_box_draw(b, WHITE, text(SITE_BOX_TEXT, shown), -1);
        sh(b + 0x12, lhu(b + 0x12) ^ 4u);
        sh(b + 0xa, static_cast<uint32_t>((h - lh(b + 0xe)) >> 1));
        text_box_draw(b, WHITE, text(SITE_BOX_TEXT2, shown), -1);
    }
    ee_stack_free(0x20);
}

// Screen 4, the prompt: 0x4ee0 with no purchase under way; during one, 0x4ee0 for an ammo (0x524b for a weapon) the bolts
// pay, nothing for an ammo full or an entry they do not pay.
// orig: 0x0029e918
void panel_prompt() {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    int32_t id = 0;
    if (lw(VS + STEP) == 0) {
        id = TXT(0x4ee0);
    } else {
        const bool ammo = lw(cursor_entry() + E_AMMO) == 1;
        if (ammo && !ammo_short(cursor_item())) return;
        if (ammo) {
            if (lw(BOLTS) < round_price(cursor_item())) return;
            id = TXT(0x4ee0);
        } else {
            // (1.02 Fixes, fix 26: the 1.02 build's 0x0029fc08 prompts nothing for a weapon: its quantity box has the "No")
            if (fixes102_on(26)) return;
            if (lw(BOLTS) < weapon_price(cursor_item())) return;
            id = TXT(0x524b);
        }
    }
    text_centered_med(0x28, 0x14, WHITE, text(lw(VS + STEP) == 0 ? SITE_PROMPT_EMPTY : SITE_PROMPT, id), -1);
}

// The quantity box's screen (w x h): an ammo full (0x5236), short of bolts (0x5238), else the quantity between "<" and
// ">" and the total price (an ammo), or the weapon's three lines (0x5239, 0x524e, 0x524b); a weapon the bolts do not pay:
// 0x5238.
// orig: 0x0029ead8
void panel_quantity(int32_t w, int32_t h) {
    gs_target_rect(0, 0, 0x200, 0x80, 0x200, 0x80, 0);
    const uint32_t f = ee_stack_alloc(0x120);  // (the original's frame: the box record, then the text's buffer at +0x20)
    const uint32_t b = f, buf = f + 0x20;
    box_clear(b);
    sh(b + 0x8, static_cast<uint32_t>(w / 2));
    sh(b + 0xa, static_cast<uint32_t>(h / 2));
    sh(b + 0xc, static_cast<uint32_t>(w - 8));
    sh(b + 0xe, static_cast<uint32_t>(h - 8));
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 3);
    sh(b + 0x2, static_cast<uint32_t>(h));
    sh(b + 0x6, static_cast<uint32_t>(w));
    int32_t message = 0;
    const bool ammo = lw(cursor_entry() + E_AMMO) == 1;
    if (ammo && !ammo_short(cursor_item())) {
        message = TXT(0x5236);
    } else if (lw(cursor_entry() + E_AMMO) == 1) {
        if (lw(BOLTS) < round_price(cursor_item())) {
            message = TXT(0x5238);
        } else {
            moby_draw_list(lwu(VS + MOBYS) + 0x500, 1);
            sh(b + 0x12, 1);
            sh(b + 0xa, 6);
            text_box_draw(b, WHITE, text(SITE_QUANTITY_TITLE, TXT(0x5237)), -1);
            vendor_buy_all_sprintf(SITE_QUANTITY_COUNT, buf, OV(0x00161160), {sx(lw(QUANTITY))});
            const int32_t q = lw(QUANTITY);
            text_med(q < 100 ? (q < 10 ? 0x25 : 0x1e) : 0x17, 0x32, WHITE, buf, -1);
            text_med(0xc, 0x32, WHITE, OV(0x00161168), -1);   // "<"
            text_med(0x40, 0x32, WHITE, OV(0x00161170), -1);  // ">"
            const int32_t total = lw(QUANTITY) * round_price(cursor_item());
            vendor_buy_all_sprintf(SITE_QUANTITY_TOTAL, buf, OV(0x00161160), {sx(total)});
            text_right(0x40, 0x4c, WHITE, buf, -1);
        }
    } else if (lw(BOLTS) < weapon_price(cursor_item())) {
        text_box_draw(b, WHITE, text(SITE_CONFIRM_NO_BOLTS, TXT(0x5238)), -1);
    } else {
        text_med(6, 6, WHITE, text(SITE_CONFIRM_1, TXT(0x5239)), -1);
        text_med(2, 0x20, WHITE, text(SITE_CONFIRM_2, TXT(0x524e)), -1);
        text_med(2, 0x3e, WHITE, text(SITE_CONFIRM_3, TXT(0x524b)), -1);
    }
    if (message != 0) text_box_draw(b, WHITE, text(SITE_QUANTITY_MESSAGE, message), -1);
    ee_stack_free(0x120);
}

// A screen's static and its rolling band (screen `k`, its quad w x h): the static (the effect texture 0x1a at a random
// place) while its timer 0x1ca720[k] runs (2 a frame up to 256; screen 0's always, at least 0x18), restarted at random
// (1 in 700 a frame); the band (0x1c, screens 1 on) while 0x1ca740[k] runs (up to 512, restarted 1 in 360); screen 6's
// glass (0x19).
// orig: 0x0029ef68
void screen_static(int32_t k, float w, float h) {
    sub_002a0ba8(GS_TEST, 0x32003);
    const uint32_t timer = OV(0x001ca720) + static_cast<uint32_t>(k) * 4;
    bool draw = true;
    if (lw(timer) != 0) sw(timer, static_cast<uint32_t>(lw(timer) + 2));
    else if (k != 0) draw = false;
    if (draw) {
        int32_t t = lw(timer);
        if (k == 0 && !(0x17 < t)) t = 0x18;
        const float fx = static_cast<float>(rand_mod(200));
        const float fy = static_cast<float>(rand_mod(200));
        const int32_t level = 0x80 - abs_i(t - 0x80);
        sub_002a0ba8(GS_CLAMP, 0);
        const int32_t a = level << 1;
        const int32_t alpha = a < 129 ? a : 0x80;
        sub_002a0ba8(GS_ALPHA, static_cast<uint64_t>(static_cast<uint32_t>(alpha)) << 32 | 0x68);
        const float du = fpu_add(w, fx), dv = fpu_add(h, fy);
        const float u0 = fpu_add(fx, 0.0f), v0 = fpu_add(fy, 0.0f);
        gs_textured_quad(static_cast<int32_t>(f2u(eer::cvt_w(u0))), static_cast<int32_t>(f2u(eer::cvt_w(v0))),
                         static_cast<int32_t>(f2u(eer::cvt_w(du))), static_cast<int32_t>(f2u(eer::cvt_w(dv))), 0x808080u,
                         effect_tex0(0x1a), 0.0f, 0.0f, w, h);
        if (!(lw(timer) < 256)) sw(timer, 0);
    }
    if (lw(timer) == 0 && rand_mod(0x2bc) == 0) sw(timer, 2);
    sub_002a0ba8(GS_CLAMP, 0);
    sub_002a0ba8(GS_ALPHA, ALPHA_BLEND);
    if (k > 0) {
        const uint32_t band = OV(0x001ca740) + static_cast<uint32_t>(k) * 4;
        const int32_t b = lw(band);
        if (b != 0) {
            sw(band, static_cast<uint32_t>(b + 2));
            const int32_t du = static_cast<int32_t>(f2u(eer::cvt_w(w)));
            int32_t strength = 0x100 - abs_i(b - 0xfe);
            if (!(strength < 81)) strength = 0x50;
            const float pos = int_to_float(0x200 - lw(band));
            const float bh = fpu_add(h, 16.0f);
            const float y = fpu_mul(pos, 0.03125f);
            const uint64_t colour = sx(static_cast<int32_t>(static_cast<uint32_t>(strength) << 24)) | NOT_ADDR(0x505050u);
            const int32_t dv = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(bh, 1.5f))));
            gs_textured_quad(0, 0, du, dv, colour, effect_tex0(0x1c), 0.0f, -y, w, bh);
            if (!(lw(band) < 512)) sw(band, 0);
        } else if (rand_mod(0x168) == 0) {
            sw(band, 2);
        }
    }
    if (k == 6) gs_textured_quad(0, 0, 0x40, 0x40, 0x80808080u, effect_tex0(0x19), 0.0f, 0.0f, w, h);
}

namespace {
// orig: (helper) a screen's place on the Vendor model: its corner joint and its two sides (the joints k*4 .. k*4+2 of the
// moby `m`), inset by the screen's margins (0x1ca318[k]: left, bottom, right, top)
struct ScreenPlace {
    Vec4 corner, side0, side1;
};
ScreenPlace screen_place(uint32_t m, int32_t k, uint32_t margins) {
    const uint32_t f = ee_stack_alloc(0x40);  // (the joint list and the three joints: the original's frame)
    const uint32_t list = f, out = f + 0x10;
    sw(list + 0, static_cast<uint32_t>(k * 4));
    sw(list + 4, static_cast<uint32_t>(k * 4 + 1));
    sw(list + 8, static_cast<uint32_t>(k * 4 + 2));
    moby_joint_positions(m, 3, list, out);
    ScreenPlace s;
    s.corner = V(out);
    vec3_sub(s.side0, V(out + 0x10), V(out));
    vec3_sub(s.side1, V(out + 0x20), V(out));
    ee_stack_free(0x40);
    Vec4 t;
    float len = vec3_length(s.side0);
    vec3_set_length(t, s.side0, lwc1(margins + 0));
    vec3_add(s.corner, s.corner, t);
    const float m2 = lwc1(margins + 8);
    vec3_set_length(s.side0, s.side0, fpu_sub(len, fpu_add(m2, m2)));
    len = vec3_length(s.side1);
    vec3_set_length(t, s.side1, lwc1(margins + 4));
    vec3_add(s.corner, s.corner, t);
    const float m3 = lwc1(margins + 0xc);
    vec3_set_length(s.side1, s.side1, fpu_sub(len, fpu_add(m3, m3)));
    return s;
}
}  // namespace

// The Vendor's six screens: each drawn into the 512 x 128 target (its panel, its static), then laid on its place on the
// Vendor model (the moby `m`) as a quad of the target's texture; while the screens open / close they grow / shrink from
// their middles.
// orig: 0x0029f3c0
void screens_draw(uint32_t m) {
    for (int32_t k = 0; k < 6; ++k) {
        ScreenPlace s = screen_place(m, k, OV(0x001ca318) + static_cast<uint32_t>(k) * 16);
        Vec4 far;
        vec3_add(far, s.corner, s.side0);
        vec3_add(far, far, s.side1);
        const Box full = box_project(s.corner, far);
        if (lw(CLOSING) != 0 || lw(OPENING) != 0) {
            float grow = 1.0f;
            if (lw(OPENING) != 0) grow = fpu_mul(int_to_float(8 - lw(OPEN_TIMER)), 0.125f);
            if (lw(CLOSING) != 0) grow = fpu_mul(int_to_float(lw(CLOSE_TIMER)), 0.125f);
            Vec4 t;
            float len = vec3_length(s.side0);
            float rest = fpu_sub(1.0f, grow);
            vec3_set_length(t, s.side0, fpu_mul(fpu_mul(len, rest), 0.5f));
            vec3_set_length(s.side0, s.side0, fpu_mul(len, grow));
            vec3_add(s.corner, s.corner, t);
            len = vec3_length(s.side1);
            rest = fpu_mul(len, rest);
            vec3_set_length(t, s.side1, fpu_mul(rest, 0.5f));
            vec3_set_length(s.side1, s.side1, fpu_mul(len, grow));
            vec3_add(s.corner, s.corner, t);
        }
        Vec4 c2, c3;
        vec3_add(c2, s.corner, s.side1);
        vec3_add(c3, c2, s.side0);
        const Box shown = box_project(s.corner, c3);
        target_begin(9, 7, 1.0f);
        gs_target_clear(0x200, 0x200);
        sub_002a0ba8(GS_ALPHA, ALPHA_SCREEN);
        // (the original jumps through its table 0x1e9510; the NTSC-U cases in the comments)
        switch (k) {
        case 0: panel_sign(); break;     // (0x0029f790)
        case 1: panel_monitor(); break;  // (0x0029f7a0)
        case 2: panel_head(); break;     // (0x0029f7b0)
        case 3:  // (0x0029f7c0)
            panel_message(static_cast<int32_t>(f2u(eer::cvt_w(shown.w))), static_cast<int32_t>(f2u(eer::cvt_w(shown.h)))); break;
        case 4: panel_prompt(); break;  // (0x0029f7e8)
        default: panel_list(); break;   // 5 (0x0029f7f8)
        }
        screen_static(k, shown.w, shown.h);
        target_end();
        gs_target_clear(0x200, 0x200);
        sub_002a0ba8(GS_ALPHA, ALPHA_SCREEN);
        sub_002a0ba8(GS_CLAMP, 5);
        packet_ref_1c2f00();
        const uint64_t tex = ld(TARGET_TEX0);
        // the target's texels: its whole width, its height scaled by 13/14 at 50 Hz (the PAL build's taller frame)
        int32_t du, dv;
        if (lw(VIDEO_50HZ) != 0) {
            const float th = fpu_mul(full.h, std::bit_cast<float>(0x3f6db6dcu));
            du = static_cast<int32_t>(f2u(eer::cvt_w(fpu_sub(full.w, 1.0f))));
            dv = float_to_int(fpu_sub(th, 1.0f));
        } else {
            du = static_cast<int32_t>(f2u(eer::cvt_w(fpu_sub(full.w, 1.0f))));
            dv = static_cast<int32_t>(f2u(eer::cvt_w(fpu_sub(full.h, 1.0f))));
        }
        gs_textured_quad(0, 0, du, dv, 0x80808080u, tex, shown.x, shown.y, shown.w, shown.h);
        sub_002a0ba8(GS_CLAMP, 0);
    }
}

// The screens' glass (the effect texture 0x19 on screens 1 to 5, drawn as quads at their places on the model). Also the
// update's draw callback in the opening and the closing (draw_callback_add, the moby `m` the Vendor).
// orig: 0x0029f990
void screens_glass(uint32_t m) {
    for (int32_t k = 1; k < 6; ++k) {
        ScreenPlace s = screen_place(m, k, OV(0x001ca318) + static_cast<uint32_t>(k) * 16);
        Vec4 far;
        vec3_add(far, s.corner, s.side0);
        vec3_add(far, far, s.side1);
        (void)box_project(s.corner, far);  // (its box unused)
        // the quad's descriptor (quad_draw.h): the four corners, their colours, their texels, CLAMP, TEX0, TEX1, ALPHA
        const uint32_t d = ee_stack_alloc(0x90);
        V(d + 0x00) = s.corner;
        vec3_add(V(d + 0x10), s.corner, s.side0);
        vec3_add(V(d + 0x20), s.corner, s.side1);
        vec3_add(V(d + 0x30), V(d + 0x20), s.side0);
        sd(d + 0x88, ALPHA_BLEND);
        sd(d + 0x70, 5);
        sd(d + 0x80, 1);
        for (uint32_t c = 0; c < 4; ++c) sw(d + 0x40 + c * 4, 0x80808080u);
        sd(d + 0x78, effect_tex0(0x19));
        for (uint32_t c = 0; c < 4; ++c) swc1(d + c * 0x10 + 0xc, 1.0f);
        const float edge = std::bit_cast<float>(0x3f7c0000u);  // 0.984375
        swc1(d + 0x50, 0.0f);
        swc1(d + 0x54, 0.0f);
        swc1(d + 0x58, edge);
        swc1(d + 0x5c, 0.0f);
        swc1(d + 0x60, 0.0f);
        swc1(d + 0x64, edge);
        swc1(d + 0x68, edge);
        swc1(d + 0x6c, edge);
        sub_002a0ba8(GS_TEST, 0x32003);
        quad_draw(d, 0);
        ee_stack_free(0x90);
    }
}

// The quantity box's own screen (on the box moby `m`, its joints 0x161178): the box's panel drawn into the target with
// screen 6's static and glass, then the box moby, then the target as a sprite on the box.
// orig: 0x0029fcd0
void quantity_screen(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0x80);  // (the joint list, the three joints, the box's two corners and the box)
    const uint32_t list = f, out = f + 0x10, p0 = f + 0x40, p1 = f + 0x50, box = f + 0x60;
    sw(list + 0, lwu(OV(0x00161178)));
    sw(list + 4, lwu(OV(0x0016117c)));
    sw(list + 8, lwu(OV(0x00161180)));
    moby_joint_positions(m, 3, list, out);
    Vec4 corner = V(out), side0, side1, t;
    vec3_sub(side0, V(out + 0x10), V(out));
    vec3_sub(side1, V(out + 0x20), V(out));
    const float inset = std::bit_cast<float>(0x3d23d70au);  // 0.04
    float len = vec3_length(side0);
    vec3_set_length(t, side0, std::bit_cast<float>(0x3cf5c28fu));  // 0.03
    vec3_add(corner, corner, t);
    vec3_set_length(side0, side0, fpu_sub(len, std::bit_cast<float>(0x3d75c28fu)));  // - 0.06
    len = vec3_length(side1);
    vec3_set_length(t, side1, inset);
    vec3_add(corner, corner, t);
    vec3_set_length(side1, side1, fpu_sub(len, inset));
    Vec4 c2;
    vec3_add(c2, corner, side1);
    V(p0) = corner;
    vec3_add(V(p1), c2, side0);
    menu_box_project(p0, p1, box + 0, box + 4, box + 8, box + 0xc);
    target_begin(9, 7, 1.0f);
    gs_target_clear(0x200, 0x200);
    sub_002a0ba8(GS_ALPHA, ALPHA_SCREEN);
    panel_quantity(lw(box + 0), lw(box + 4));
    screen_static(6, static_cast<float>(lw(box + 0)), static_cast<float>(lw(box + 4)));
    target_end();
    gs_target_clear(0x200, 0x200);
    sub_002a0ba8(GS_ALPHA, ALPHA_SCREEN);
    moby_draw_list(m, 1);
    const int32_t w = lw(box + 0), h = lw(box + 4);
    glyph_sprite(lw(box + 8), lw(box + 0xc), w, h, 0, 0, w, h, 0x80808080u, ld(TARGET_TEX0));
    ee_stack_free(0x80);
}

// The screens' frames: four quads (corners 0x1ca480[i], their points 0x1ca380 turned by the Vendor's matrix, at its
// place), their colours 0x1ca458 and texels 0x1ca410 / 0x1ca414 scrolling (0x161184, 0.01 a frame) on the effect
// texture 0x18.
// orig: 0x0029ff58
void screens_frames() {
    float scroll = fpu_add(lwc1(OV(0x00161184)), std::bit_cast<float>(0x3c23d70au));
    swc1(OV(0x00161184), scroll);
    if (1.0f < scroll) swc1(OV(0x00161184), fpu_sub(scroll, 1.0f));
    const uint32_t d = ee_stack_alloc(0x90);  // the quad's descriptor (quad_draw.h)
    sd(d + 0x88, 0x4000000044ull);
    sd(d + 0x80, 1);
    sd(d + 0x70, 0);
    sd(d + 0x78, effect_tex0(0x18));
    const uint32_t points = OV(0x001ca380), texels = OV(0x001ca410), colours = OV(0x001ca458);
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t q = OV(0x001ca480) + i * 16;
        uint32_t idx[4];
        Vec4 t[4];
        for (uint32_t c = 0; c < 4; ++c) {
            idx[c] = lwu(q + c * 4);
            mat3_transform(t[c], V(points + (idx[c] << 4)), M(lwu(VS + VENDOR) + 0xc0));
        }
        const uint32_t pos = lwu(VS + VENDOR) + 0x10;
        for (uint32_t c = 0; c < 4; ++c) {
            swc1(d + c * 0x10 + 0, fpu_add(lwc1(pos + 0), t[c].x));
            swc1(d + c * 0x10 + 4, fpu_add(lwc1(pos + 4), t[c].y));
            swc1(d + c * 0x10 + 8, fpu_add(lwc1(pos + 8), t[c].z));
        }
        const float s = lwc1(OV(0x00161184));
        for (uint32_t c = 0; c < 4; ++c) {
            sw(d + 0x40 + c * 4, lwu(colours + idx[c] * 4));
            swc1(d + 0x50 + c * 8, lwc1(texels + idx[c] * 8));
            swc1(d + 0x54 + c * 8, fpu_add(lwc1(texels + 4 + idx[c] * 8), s));
        }
        quad_draw(d, 0);
    }
    ee_stack_free(0x90);
}

// The frame buffer's picture stored at `dst` (after a DMA wait and a vsync: the frame counted).
// orig: 0x001f99c0
void frame_picture_store(uint32_t dst) {
    dma_wait(1);
    vendor_vsync_wait(NOT_ADDR(0x001f99e0u));
    sw(OV(0x0015f3f8), static_cast<uint32_t>(lw(OV(0x0015f3f8)) + 1));
    sub_002a07f8();
    gs_store_rows(dst, lw(0x0015ee84), lw(0x0013e504) << 11);
}

namespace {
// orig: (helper) the draw's first frame (the draw's mode 0): the game's frame rendered (the Vendor hidden once opened),
// then, once the screens are open, its picture stored as their background, the camera's rows kept (for the way out),
// the item mobys made, the sign's buffer and the Vendor's animations' read started
void picture_frame() {
    if (lw(VS + OPENED) != 0) sh(lwu(VS + VENDOR) + 0x34, lhu(lwu(VS + VENDOR) + 0x34) | 1u);
    sw(OV(0x0015f3f4), 0x100ff);
    if (lw(VS + OPENED) != 0 || lw(VS + STATE) == 3) sw(OV(0x0015f3f4), 0x7f);
    render_frame();
    if (lw(VS + OPENED) == 0) return;
    sw(VS + DRAW_MODE, 1);
    sw(VS + OPENED, 0);
    dma_wait(1);
    vendor_vsync_wait(NOT_ADDR(0x002a0378u));
    sw(OV(0x0015f3f8), static_cast<uint32_t>(lw(OV(0x0015f3f8)) + 1));
    TODO_PORT(0x002a08c8, "DMA: packet kick")();  // (the host sends the frame's packets at the vsync above)
    sub_002a0858();
    sw(OV(0x001611cc), 0x60000);
    frame_picture_store(lwu(VS + SCENE_A));
    sh(lwu(VS + VENDOR) + 0x34, lhu(lwu(VS + VENDOR) + 0x34) & 0xfffeu);
    swc1(lwu(VS + VENDOR) + 0x2c, std::bit_cast<float>(0x3e115055u));
    // the camera's rows kept at 0x165f80 (the closing puts them back); opened by the PDA the screens' own camera
    const uint32_t r0 = OV(0x00166fd0), r1 = OV(0x00166fe0), r2 = OV(0x00166ff0);
    if (lw(VS + PDA) != 0) {
        lq_sq(VS + CAM_TARGET, OV(0x00166dd0));
        lq_sq(OV(0x00165f80), r0);
        lq_sq(OV(0x00165fa0), r2);
        lq_sq(OV(0x00165f90), r1);
        for (const uint32_t r : {r0, r1, r2}) {
            sd(r, 0);
            sd(r + 8, 0);
        }
        swc1(OV(0x00166ffc), 1.0f);
        swc1(r0, 1.0f);
        swc1(OV(0x00166fe4), 1.0f);
        swc1(OV(0x00166c80) + 0x378, 1.0f);
        render_view_setup();
    } else {
        lq_sq(OV(0x00165f80), r0);
        lq_sq(OV(0x00165fa0), r2);
        lq_sq(OV(0x00165f90), r1);
        vec3_cross_ba(V(r1), V(r0), V(r2));
    }
    const uint32_t mobys = lwu(VS + SCENE_B) + 0x100;
    sw(VS + MOBYS, mobys);
    for (const uint32_t ring : {0x000u, 0x500u}) {
        moby_init(lwu(VS + MOBYS) + ring, 0xd);
        sh(lwu(VS + MOBYS) + ring + 0x32, 0x40);
        moby_update_transform(lwu(VS + MOBYS) + ring);
        moby_set_color_packed(lwu(VS + MOBYS) + ring, GREY, 0xe, 0xe, 0);
    }
    vendor_hologram_make();
    const uint32_t sign = lwu(VS + SCENE_B) + 0x1400;
    sw(VS + SIGN, sign);
    mem_clear128(sign, 0x400);
    const uint32_t anims = lwu(VS + SIGN) + 0x400;
    sw(VS + ANIMS, anims);
    const bool reading = lh(STREAM_BUSY) == 0 && stream_disc_read(anims + 0x80000u, lwu(0x00137d18), lwu(0x00137d1c)) != 0;
    sw(ANIMS_READING, reading ? 1u : 0u);
}
}  // namespace

}  // namespace vendor

// The screens' draw: nothing while the draw is skipped (0x15f5d8); the draw's first mode renders the game's frame and
// keeps its picture; then every frame the picture put back, the view, the moby pass (the hologram's part on its stand,
// the Vendor), the screens' frames (a Vendor's), the six screens, the quantity box's screen during a purchase, the
// screens' glass, the HUD.
// orig: 0x002a02b0
void vendor_mode_draw_tr(uint32_t) {
    if (lw(SKIP_DRAW) != 0) return;
    sub_00200fd8();
    const int32_t mode = lw(VS + DRAW_MODE);
    if (mode == 0) {
        picture_frame();
        if (lw(VS + DRAW_MODE) != 1) return;
    } else if (mode != 1) {
        return;
    }
    gs_upload_rows(lwu(VS + SCENE_A), lw(0x0015ee84), lw(0x0013e504) << 11);
    texture_block_begin(0);
    render_view_setup();
    render_static_packets();
    moby_pass_start();
    spr_moby_fill();
    spr_moby_save();
    if (lw(CURSOR_MOVED) == 0 && lw(VS + PDA) == 0) {
        if (lw(cursor_entry() + E_AMMO) != 1) {
            moby_draw_list(lwu(VS + MOBYS) + 0x300, 1);
        } else if (lwu(OV(0x001c8ff0) + (lwu(cursor_entry() + E_ITEM) << 6) + 0x30) != 0) {
            moby_draw_list(lwu(VS + MOBYS) + 0x200, 1);
        }
    }
    moby_draw_list(lwu(VS + VENDOR), 1);
    if (lw(VS + PDA) == 0) screens_frames();
    screens_draw(lwu(VS + VENDOR));
    if (lw(VS + STEP) != 0) quantity_screen(lwu(VS + MOBYS) + 0x400);
    screens_glass(lwu(VS + VENDOR));
    sub_002a0ba8(GS_ALPHA, ALPHA_BLEND);
    hud_draw();
    texture_block_end();
    spr_moby_load();
    moby_pass_end();
    dma_wait(0x10);
    moby_texture_slots();
}

// The draw callback the update queues in the opening and the closing (the moby the Vendor).
// orig: 0x0029f990
void vendor_screens_glass_cb(uint32_t m) { screens_glass(m); }

}  // namespace rac1
