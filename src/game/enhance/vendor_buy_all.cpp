// The Vendor's "All Ammo" entry (see vendor_buy_all.h). Not a port: everything here is new, built on the game's own
// Vendor state (the item list 0x1ca4c0), its prices (the item table 0x1c40b0), its ammo function and its texts.
//
// The game's data used (Veldin's addresses, relocated):
//   the item list 0x1ca4c0: +0 the screens' state, +0x1c the Vendor moby (its sounds), +0x40 opened by the PDA,
//     +0x58 the cursor, +0x5c the purchase's step (0: none), the entries from +0xd0 (0x14 bytes: +0 the item, +4 1 for
//     an ammo entry), their count +0x210 (item_list_build 0x29a400);
//   the item table 0x1c40b0 (0x18 bytes an item): +8 a round's price, +0xa at the PDA, +0xe the most ammo;
//   the ammo 0x13d428[item], the ammo bought 0x13dd70[item], the bolts 0x15ed98;
//   0x161e18 / 0x161e10: the screens busy (an opening, the quantity box): the list does not move.
#include <game/enhance/vendor_buy_all.h>

#include <game/enhance/advanced.h>

#include <game/lib/libc.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/vendor_tr.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <vector>

namespace rac1 {

namespace {
constexpr OvConst<0x001ca4c0u> LIST{};
constexpr OvConst<0x001c40b0u> ITEMS{};
constexpr OvConst<0x0015f5c4u> MODE{};
constexpr OvConst<0x00161e18u> BUSY_OPEN{};
constexpr OvConst<0x00161e10u> BUSY_QUANTITY{};
constexpr uint32_t AMMO = 0x0013d428, AMMO_BOUGHT = 0x0013dd70, BOLTS = 0x0015ed98;
constexpr uint32_t PAD = 0x0013c940;
constexpr uint32_t LEFT = 0x8000, RIGHT = 0x2000, CROSS = 0x40;

bool g_selected = false;  // the entry is the selection (the game's cursor stays on the list's first item)

// The entry's texts by the game's language (0x15ed88: 0 English, 2 French, 3 German, 4 Spanish, 5 Italian; the font
// has no accented capitals looked at yet, so none are used): the sign's line, the monitor's two title lines, the
// tile's word over the game's own "AMMO"
struct Texts {
    const char* sign;
    const char* title1;
    const char* title2;
    const char* tile;
};
constexpr Texts TEXTS[6] = {
    {"ALL AMMO", "All", "Ammo", "ALL"},
    {"ALL AMMO", "All", "Ammo", "ALL"},
    {"TOUTES LES MUNITIONS", "Toutes les", "munitions", "TOUT"},
    {"ALLE MUNITION", "Alle", "Munition", "ALLE"},
    {"TODA LA MUNICION", "Toda la", "municion", "TODO"},
    {"TUTTE LE MUNIZIONI", "Tutte le", "munizioni", "TUTTO"},
};

// orig: (none) — enhancement: the entry's texts in the game's language
const Texts& texts() {
    const uint32_t lang = lwu(0x0015ed88);
    return TEXTS[lang < 6 ? lang : 0];
}

// orig: (none) — enhancement: testing, RAC1_VENDOR_TRACE=<n>: every n frames of the Vendor's screens, its state, the
// entry and each ammo entry of its list (the item, its ammo / the most, a round's price)
int trace_every() {
    static const int n = std::getenv("RAC1_VENDOR_TRACE") != nullptr ? std::atoi(std::getenv("RAC1_VENDOR_TRACE")) : 0;
    return n;
}

// orig: (none) — enhancement: a round of ammo's price for the item `w` (the PDA's when the screens were opened by it)
int32_t round_price(uint32_t w) {
    return static_cast<int32_t>(lhu(ITEMS + w * 0x18 + (lw(LIST + 0x40) != 0 ? 0xau : 8u)));
}

// orig: (none) — enhancement: the list's entry `i`: an ammo entry, its item in `w`
bool ammo_entry(int32_t i, uint32_t& w) {
    const uint32_t e = LIST + 0xd0 + static_cast<uint32_t>(i) * 0x14;
    w = lwu(e);
    return lw(e + 4) == 1 && w < 0x25;
}

// orig: (none) — enhancement: the rounds the item `w` lacks (0 when full)
int32_t rounds_missing(uint32_t w) {
    const int32_t most = static_cast<int32_t>(lhu(ITEMS + w * 0x18 + 0xe));
    const int32_t have = lw(AMMO + w * 4);
    return most > have ? most - have : 0;
}

// orig: (none) — enhancement: the list's screens are up and still (no opening, purchase or quantity box under way)
bool list_idle() {
    return lw(MODE) == 5 && lw(LIST) == 1 && lw(LIST + 0x5c) == 0 && lw(BUSY_OPEN) == 0 && lw(BUSY_QUANTITY) == 0 && lw(LIST + 0x210) > 0;
}

// orig: (none) — enhancement: the sign's text (the screens' marquee, 0x29b070), `s` a host string
void sign_text(const char* s) {
    const uint32_t n = static_cast<uint32_t>(std::strlen(s)) + 1;
    const uint32_t a = ee_stack_alloc(n);
    for (uint32_t i = 0; i < n; ++i) sb(a + i, static_cast<uint8_t>(s[i]));
    ovl_29b070_tr(a, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);
    ee_stack_free(n);
}

// orig: (none) — enhancement: the sign's text, a game text id (0x203ad8)
void sign_text_id(int32_t id) { ovl_29b070_tr(sub_00203ad8(id), 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) — enhancement: a Vendor sound (the moby's sound `k`: 0 error, 1 the cursor, 2 full, 7 purchased)
void vendor_sound(int32_t k) { moby_sound_start(k, 0, lwu(LIST + 0x1c)); }

// orig: (none) — enhancement: the pad's buttons `m` hidden from the game this frame (held and pressed; +0x348, what the
// next frame's edges are made from, keeps them: no false press when they are let go)
void pad_hide(uint32_t m) {
    for (const uint32_t o : {0x1a0u, 0x1a4u, 0x1c0u, 0x1c4u}) sw(PAD + o, lwu(PAD + o) & ~m);
}

// orig: (none) — enhancement: the purchase: the steps of the game's ammo purchase (0x29ba78, its step 4) for each ammo
// entry in the list's order, as many rounds as the bolts pay for; then once the Vendor's thanks, the list rebuilt, the
// sign's message and the sound
void buy_all() {
    const int32_t n = lw(LIST + 0x210);
    int32_t bought = 0;
    bool short_of_bolts = false;
    for (int32_t i = 0; i < n; ++i) {
        uint32_t w = 0;
        if (!ammo_entry(i, w)) continue;
        const int32_t price = round_price(w);
        const int32_t missing = rounds_missing(w);
        if (price <= 0 || missing <= 0) continue;
        const int32_t bolts = lw(BOLTS);
        const int32_t rounds = bolts / price < missing ? bolts / price : missing;
        if (rounds < missing) short_of_bolts = true;
        if (rounds <= 0) continue;
        const int32_t got = rounds - ammo_add(w, rounds);
        sw(AMMO_BOUGHT + w * 4, static_cast<uint32_t>(lw(AMMO_BOUGHT + w * 4) + got));
        sw(BOLTS, static_cast<uint32_t>(lw(BOLTS) - price * got));
        bought += got;
    }
    if (bought > 0) {
        ovl_29b4d8_tr(3, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);  // (the Vendor's thanks, as after an ammo purchase)
        item_list_build(lw(LIST + 0x40) > 0 ? 1 : 0);
        sign_text_id(0x4f5f);
        vendor_sound(7);
    } else if (short_of_bolts) {
        sign_text_id(0x4f60);
        vendor_sound(0);
    } else {
        vendor_sound(2);
    }
}

// orig: (none) — enhancement: the sign's text for the entry: the game's own description of the cursor's item when it
// goes back to the list
void sign_item_text() {
    uint32_t w = lwu(LIST + 0xd0 + static_cast<uint32_t>(lw(LIST + 0x58)) * 0x14);
    sign_text_id(lw(OV(0x001c2108) + w * 0x14 + 0x10));
}
}  // namespace

// orig: (none) — enhancement (vendor_buy_all.h)
int32_t vendor_buy_all_price(int32_t* weapons) {
    int32_t total = 0, count = 0;
    const int32_t n = lw(LIST + 0x210);
    for (int32_t i = 0; i < n && i < 16; ++i) {
        uint32_t w = 0;
        if (!ammo_entry(i, w)) continue;
        const int32_t missing = rounds_missing(w);
        if (missing <= 0) continue;
        total += missing * round_price(w);
        ++count;
    }
    if (weapons != nullptr) *weapons = count;
    return total;
}

// orig: (none) — enhancement (vendor_buy_all.h)
void vendor_buy_all_input() {
    static uint32_t frames = 0;  // (the Vendor's frames, for the trace: the game's frame counters stand still there)
    if (const int t = trace_every(); t > 0 && lw(MODE) == 5 && frames++ % static_cast<uint32_t>(t) == 0) {
        std::printf("[vendor] state %d step %d busy %d/%d scroll %d cursor %d count %d pda %d sel %d bolts %d price %d |", lw(LIST), lw(LIST + 0x5c),
                    lw(BUSY_OPEN), lw(BUSY_QUANTITY), lw(LIST + 0x4c), lw(LIST + 0x58), lw(LIST + 0x210), lw(LIST + 0x40), g_selected ? 1 : 0, lw(BOLTS),
                    advanced_vendor_buy_all() ? vendor_buy_all_price() : -1);
        for (int32_t i = 0; i < lw(LIST + 0x210) && i < 16; ++i) {
            uint32_t w = 0;
            if (ammo_entry(i, w)) std::printf(" %02x:%d/%d@%d", w, lw(AMMO + w * 4), lhu(ITEMS + w * 0x18 + 0xe), round_price(w));
            else std::printf(" %02x:-", w);
        }
        std::printf("\n");
    }
    if (!advanced_vendor_buy_all() || lw(MODE) != 5) {
        g_selected = false;
        return;
    }
    if (!list_idle()) {
        if (lw(LIST) != 1) g_selected = false;  // (the screens closing or not up)
        return;
    }
    const uint32_t pressed = lwu(PAD + 0x1c4);
    if (!g_selected) {
        if (lw(LIST + 0x58) == 0 && (pressed & LEFT) != 0) {
            g_selected = true;
            pad_hide(LEFT);
            vendor_sound(1);
            sign_text(texts().sign);
        }
        return;
    }
    if (lw(LIST + 0x58) != 0) {  // (the game's cursor moved all the same: the list is the selection again)
        g_selected = false;
        return;
    }
    pad_hide(LEFT | RIGHT | CROSS);
    if ((pressed & RIGHT) != 0) {
        g_selected = false;
        vendor_sound(1);
        sign_item_text();
    } else if ((pressed & CROSS) != 0) {
        buy_all();
    }
}

namespace {
// The Vendor's 2D, in the game's 2D units (512 x 448 for the 4:3 frame; measured on the game's own screens): the item
// strip's tiles are 50 wide, 51.5 high (y 338.5 .. 390), 56 apart, its highlight 55 x 56 (y 336 .. 392) in lines 1.5
// thick; with fewer than 8 items the strip's frame is 56 n + 32 wide, centred on 256, the tiles fixed in it; with 8 or
// more it is a wheel turning under the highlight in the middle, a tile 40 of the list's scroll (+0x4c) units away
constexpr float CENTRE = 256.0f, PITCH = 56.0f, TILE_W = 50.0f, TILE_Y0 = 338.5f, TILE_Y1 = 390.0f;
constexpr float HL_HALF = 27.5f, HL_Y0 = 336.0f, HL_Y1 = 392.0f, HL_LINE = 1.5f;
constexpr float WHEEL_X0 = 53.0f, WHEEL_X1 = 459.0f;  // (the wheel's window, inside its frame)
// the item monitor's screen (its picture is drawn into a texture on the monitor), its title lines and price
constexpr float MON_X0 = 14.0f, MON_Y0 = 87.0f, MON_X1 = 163.0f, MON_Y1 = 209.0f;
constexpr uint64_t TILE_BLUE = 0x80d06964ull, HIGHLIGHT = 0x805cb0b0ull, BLACK = 0x80000000ull;
constexpr uint64_t WHITE = 0x80f0f0f0ull, GREY = 0x80808080ull, GLYPH = 0x80100808ull, SHORT = 0x806060f0ull;

// orig: (none) — enhancement: a flat rectangle in 2D units (the game's 0x23caa8, in 1/16 units)
void rect(float x0, float y0, float x1, float y1, uint64_t rgba) {
    gs_flat_rect(static_cast<int32_t>(x0 * 16.0f), static_cast<int32_t>(y0 * 16.0f), static_cast<int32_t>(x1 * 16.0f),
                 static_cast<int32_t>(y1 * 16.0f), rgba, 1);
}

// orig: (none) — enhancement: a frame of lines `t` thick around (x0, y0) .. (x1, y1)
void frame(float x0, float y0, float x1, float y1, float t, uint64_t rgba) {
    rect(x0, y0, x1, y0 + t, rgba);
    rect(x0, y1 - t, x1, y1, rgba);
    rect(x0, y0, x0 + t, y1, rgba);
    rect(x1 - t, y0, x1, y1, rgba);
}

// orig: (none) — enhancement: a host string from x in the game's medium font (0x1fb3f0; the string on the EE stack)
void text(const char* s, int32_t x, int32_t y, uint64_t colour) {
    const uint32_t n = static_cast<uint32_t>(std::strlen(s)) + 1;
    const uint32_t a = ee_stack_alloc(n);
    for (uint32_t i = 0; i < n; ++i) sb(a + i, static_cast<uint8_t>(s[i]));
    text_med(x, y, colour, a, -1);
    ee_stack_free(n);
}
// orig: (none) — enhancement: a host string in the game's big font scaled by `scale`, centred on x (0x1fb4f0)
void text_big_scaled(const char* s, float x, float y, uint64_t colour, float scale) {
    const uint32_t n = static_cast<uint32_t>(std::strlen(s)) + 1;
    const uint32_t a = ee_stack_alloc(n);
    for (uint32_t i = 0; i < n; ++i) sb(a + i, static_cast<uint8_t>(s[i]));
    const float w = static_cast<float>(text_width_big(a, -1)) * scale;
    text_draw_f(colour, a, -1, effect_tex0(3), OV(0x001c3890), x - w * 0.5f, y, scale);
    ee_stack_free(n);
}
// orig: (none) — enhancement: the same ending at x (the medium font)
void text_end(const char* s, int32_t x, int32_t y, uint64_t colour) {
    const uint32_t n = static_cast<uint32_t>(std::strlen(s)) + 1;
    const uint32_t a = ee_stack_alloc(n);
    for (uint32_t i = 0; i < n; ++i) sb(a + i, static_cast<uint8_t>(s[i]));
    text_med(x - text_width_med(a, -1), y, colour, a, -1);
    ee_stack_free(n);
}

// orig: (none) — enhancement: a price as the game writes it: from 1000 up, its format 0x160150 with the thousands, the
// language's separator (0x1e9120) and the rest, as the talk prices (0x266970)
std::string price_text(int32_t v) {
    if (v < 1000) return std::to_string(v);
    const uint32_t buf = ee_stack_alloc(0x40);
    const int32_t r6 = lw(0x0015ed88) % 6;
    const std::vector<uint64_t> args = {static_cast<uint64_t>(static_cast<int64_t>(v / 1000)),
                                        static_cast<uint64_t>(static_cast<int64_t>(lw(OV(0x001e9120) + static_cast<uint32_t>(r6 * 4)))),
                                        static_cast<uint64_t>(static_cast<int64_t>(v % 1000))};
    libc_sprintf(buf, OV(0x00160150), args);
    std::string out;
    for (uint32_t q = buf; lbu(q) != 0 && out.size() < 0x3f; ++q) out += static_cast<char>(lbu(q));
    ee_stack_free(0x40);
    return out;
}

// orig: (none) — enhancement: the centre x of the list's tile k (k = -1: the place before the first item); false when a
// long list's wheel does not show it whole
bool tile_x(int32_t k, float& x) {
    const int32_t n = lw(LIST + 0x210);
    if (n < 8) {
        x = CENTRE + (static_cast<float>(k) - static_cast<float>(n - 1) * 0.5f) * PITCH;
        return true;
    }
    x = CENTRE + (static_cast<float>((k - lw(LIST + 0x58)) * 40 + lw(LIST + 0x4c))) * (PITCH / 40.0f);
    return x - HL_HALF >= WHEEL_X0 && x + HL_HALF <= WHEEL_X1;
}
}  // namespace

// orig: (none) — enhancement (vendor_buy_all.h)
void vendor_buy_all_draw() {
    if (!advanced_vendor_buy_all() || lw(MODE) != 5 || lw(LIST) != 1 || lw(LIST + 0x210) <= 0) return;
    const int32_t n = lw(LIST + 0x210);
    int32_t weapons = 0;
    const int32_t price = vendor_buy_all_price(&weapons);
    const bool none = weapons == 0;
    const Texts& t = texts();

    // the entry's tile: before the first item - left of the strip's frame (a short list), or the wheel's place before
    // the first item (a long one)
    float x = 0.0f, scale = 1.0f;
    bool shown = true;
    if (n < 8) {
        const float frame_left = CENTRE - (PITCH * 0.5f * static_cast<float>(n) + 16.0f);
        x = frame_left - 3.0f - HL_HALF;
        if (x - HL_HALF < 2.0f) {  // (7 items: the frame leaves 44 units; the tile made smaller)
            scale = (frame_left - 5.0f) / (2.0f * HL_HALF);
            x = 2.0f + HL_HALF * scale;
        }
    } else {
        shown = tile_x(-1, x);
    }
    const float half = TILE_W * 0.5f * scale, mid = (TILE_Y0 + TILE_Y1) * 0.5f, h = (TILE_Y1 - TILE_Y0) * 0.5f * scale;
    if (shown) {
        rect(x - HL_HALF * scale, HL_Y0, x + HL_HALF * scale, HL_Y1, BLACK);
        rect(x - half, mid - h, x + half, mid + h, none ? 0x80706868ull : TILE_BLUE);
        // (the word over the game's own "AMMO", which every ammo tile has in its picture; both in the big font, narrowed
        // to the tile)
        text_big_scaled(t.tile, x, mid - 21.0f * scale, none ? GREY : GLYPH, (std::strlen(t.tile) > 3 ? 0.8f : 1.0f) * scale);
        text_big_scaled("AMMO", x, mid + 6.0f * scale, none ? GREY : WHITE, 0.75f * scale);
        if (g_selected) frame(x - HL_HALF * scale, HL_Y0, x + HL_HALF * scale, HL_Y1, HL_LINE, HIGHLIGHT);
    }
    if (!g_selected) return;

    // the game's highlight on the first item hidden (the strip's black over its lines)
    float x0 = 0.0f;
    if (tile_x(0, x0)) {
        // (the highlight pulses: covered up to the next tiles, 31 from the middle, and the frame's inside, 333 .. 394.5)
        constexpr float GAP = 30.5f;
        rect(x0 - GAP, 333.0f, x0 + GAP, TILE_Y0, BLACK);
        rect(x0 - GAP, TILE_Y1, x0 + GAP, 394.5f, BLACK);
        rect(x0 - GAP, HL_Y0, x0 - TILE_W * 0.5f, HL_Y1, BLACK);
        rect(x0 + TILE_W * 0.5f, HL_Y0, x0 + GAP, HL_Y1, BLACK);
    }

    // the item monitor: the entry's title and its price (greyed when every weapon is full, in red when the bolts fall
    // short: then the purchase fills what they pay for)
    rect(MON_X0, MON_Y0, MON_X1, MON_Y1, BLACK);
    text(t.title1, 19, 92, none ? GREY : WHITE);
    text(t.title2, 36, 108, none ? GREY : WHITE);
    text_end(price_text(price).c_str(), 150, 186, none ? GREY : (price > lw(BOLTS) ? SHORT : WHITE));
}

}  // namespace rac1
