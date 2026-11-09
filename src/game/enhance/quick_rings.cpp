// Three quick select rings (see src/host/quick_rings.h). Not a port: everything here is new, on top of the game's quick
// select (quick_select.cpp: its slots 0x17dac0, 0x1c bytes each, +0 the icon's animation, +0x18 the item) and the pause
// menu's Quick Select page (pause_menu_quick.cpp: its items at w+0x30).
#include <host/quick_rings.h>
#include <host/mod_code.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>

#include <cstdio>
#include <cstring>

namespace rac1 {

namespace {
constexpr uint32_t PAD = 0x0013c940;    // pad 1's state (pad_build)
constexpr uint32_t RING1 = 0x00141ea0;  // the game's quick select items (ring 1), in its save
constexpr uint32_t OWNED = 0x0013d4c0;  // the items owned (a byte each)
constexpr uint32_t ITEMS = 0x25;        // the item ids
constexpr uint32_t PC_ITEM = 0x400;     // our item in save<N>.pc (advanced.cpp's ids are below 0x100, the level mods' 0x200,
                                        // the mods' code's 0x300)
constexpr uint8_t PC_VERSION = 1;
// the pad's shoulder buttons (the low byte: L2 1, R2 2, L1 4, R1 8), triangle 0x10
constexpr uint32_t L2 = 1, R2 = 2, L1 = 4, R1 = 8, SHOULDERS = 0xf, TRIANGLE = 0x10;
// Our texts' strings, made at each lookup: the block 0xfa100..0xfa200 (in the PS2 kernel's first megabyte, as the Advanced
// block's: after the Vendor's 0xfa000..0xfa100, before the Language page's 0xfa200): "Ring N" +0, the button names +0x80
constexpr uint32_t QRG = 0x000fa100u;
constexpr int32_t TEXT_FIRST = 0x7a00;  // "Ring 1".."Ring 3" (0x7a00..0x7a02)
constexpr uint32_t LABEL = QRG, BUTTONS = QRG + 0x80;  // (the buttons: 4 strings of 4 bytes, L1 R1 L2 R2)

uint8_t g_rings[2][8] = {};  // rings 2 and 3 (0 an empty place)
int g_current = 0;           // the ring shown / last used (0..2)
int g_switch = 0;            // a ring switch asked for by the pad (-1 left, 1 right; summed)
uint32_t g_prev_raw = 0;     // pad 1's held buttons at its last build
uint32_t g_taken = 0;        // the shoulder buttons pressed with the quick select open: the game does not see them until let go
int g_fade = 0;              // frames since the last ring switch (the icons fade in)
bool g_closed = false;      // the closing noted (the log)
int g_menu_ring = 0;         // the ring the pause menu's page edits
uint8_t g_kept[2][8] = {};   // the rings over the challenge mode's new game
int g_kept_current = 0;

// orig: (none) — enhancement: the ring `r` wrapped to 0..2
int ring_wrap(int r) { return ((r % 3) + 3) % 3; }

// orig: (none) — enhancement: the quick select open (slot 3 of the HUD's elements, its update 0x237ba0), in gameplay
bool quick_select_open() { return lw(OV(0x0015f5c4)) == 0 && lwu(OV(0x0017dc50) + 3 * 0x90 + 0x14) == OVF(0x00237ba0); }

// orig: (none) — enhancement: the quick select's slots made from the ring shown, as quick_select_start makes them from ring 1
// (an item a plugin's weapon_allow disallows: the place shown empty)
void slots_fill() {
    for (uint32_t i = 0; i < 8; ++i) {
        uint32_t item = quick_rings_item(g_current, static_cast<int>(i));
        if (!mod_code_weapon_allowed(item)) item = 0;
        sw(OV(0x0017dac0) + i * 0x1c + 0x18, item);
        sw(OV(0x0017dac0) + i * 0x1c, lhu(item * 0x4c + OV(0x00179ac0) + 0x38));
    }
}

// orig: (none) — enhancement: the fade's length (the quick select's own, 0x15f780), at least 1
int fade_frames() {
    const int n = lw(OV(0x0015f780));
    return n < 1 ? 1 : n;
}

// orig: (none) — enhancement: a button's glyph as an EE string (the small font's characters 0x14..0x17: L1 R1 L2 R2, as the
// pause page's own L1 / R1 texts 0x160398 / 0x1603a0 are written)
uint32_t button_text(uint32_t glyph) {
    const uint32_t s = BUTTONS + (glyph & 3) * 4;
    sw(s, glyph);
    return s;
}

// orig: (none) — enhancement: three dots centred on (cx, y), the ring `lit` lit: colour `col`, the others grey, alpha `alpha`
void ring_dots(int32_t cx, int32_t y, int lit, uint64_t col, int32_t alpha) {
    const uint64_t a = static_cast<uint64_t>(alpha) << 24;
    for (int k = 0; k < 3; ++k) {
        const int32_t dx = cx + (k - 1) * 0x10;
        gs_flat_rect(dx - 3, y - 3, dx + 5, y + 5, a, 0);  // (the shadow)
        gs_flat_rect(dx - 4, y - 4, dx + 4, y + 4, k == lit ? col : a | 0x605848u, 0);
    }
}

// orig: (none) — enhancement: the items of ring `r` written from the 8 words at `src`
void ring_write(int r, uint32_t src) {
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t item = lwu(src + i * 4);
        if (r == 0) sw(RING1 + i * 4, item);
        else g_rings[r - 1][i] = static_cast<uint8_t>(item < ITEMS ? item : 0);
    }
}
}  // namespace

// orig: (none) — enhancement (quick_rings.h)
int quick_rings_shown() { return quick_rings_count() > 1 ? g_current : 0; }

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_item(int ring, int place) {
    if (place < 0 || place >= 8) return 0;
    if (ring <= 0 || ring > 2) return lwu(RING1 + static_cast<uint32_t>(place) * 4);
    return g_rings[ring - 1][place];
}

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_shown_item(int32_t place) {
    if (quick_rings_shown() == 0) return lwu(RING1 + static_cast<uint32_t>(place) * 4);  // (the game's read, as it was)
    return quick_rings_item(g_current, place);
}

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_pad(uint32_t pad, uint32_t held) {
    if (pad != PAD) return held;
    const uint32_t pressed = held & ~g_prev_raw;
    g_prev_raw = held;
    if (quick_rings_count() < 3) {
        g_taken = 0;
        g_switch = 0;
        return held;
    }
    if (quick_select_open()) {
        g_taken |= pressed & SHOULDERS;
        if ((pressed & SHOULDERS) != 0) std::printf("[quick rings] shoulder buttons 0x%x kept from the game\n", pressed & SHOULDERS);
        if ((held & TRIANGLE) != 0) {
            if ((pressed & (L1 | L2)) != 0) --g_switch;
            if ((pressed & (R1 | R2)) != 0) ++g_switch;
        }
    }
    g_taken &= held;  // (let go: the game sees the button again from its next press)
    return held & ~g_taken;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_start(uint32_t) {
    g_switch = 0;
    g_closed = false;
    g_fade = fade_frames();
    if (quick_rings_count() < 3) return;
    std::printf("[quick rings] opened on ring %d (in hand 0x%x)\n", g_current + 1, lwu(0x0013f350 + 0x10b8));
    if (g_current != 0) slots_fill();
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_update(uint32_t e) {
    if (quick_rings_count() < 3) return;
    if (g_fade < fade_frames()) ++g_fade;
    const int d = g_switch;
    g_switch = 0;
    if ((lwu(PAD + 0x1c0) & TRIANGLE) == 0) {  // (Triangle let go: the ring is closing)
        if (!g_closed) {
            const int32_t sel = lw(e + 0x74);
            std::printf("[quick rings] closed on ring %d, place %d, item 0x%x\n", g_current + 1, sel,
                        sel >= 0 && sel < 8 ? lwu(OV(0x0017dac0) + static_cast<uint32_t>(sel) * 0x1c + 0x18) : 0u);
        }
        g_closed = true;
        return;
    }
    g_closed = false;
    if (d == 0) return;
    g_current = ring_wrap(g_current + d);
    std::printf("[quick rings] ring %d shown\n", g_current + 1);
    slots_fill();
    // the selection kept on its place when the new ring has an item there, else none (the stick or the d-pad picks one)
    const int32_t sel = lw(e + 0x74);
    if (sel >= 0 && sel < 8 && lw(OV(0x0017dac0) + static_cast<uint32_t>(sel) * 0x1c) == 0) sw(e + 0x74, 0xffffffffu);
    sb(e + 0x71, 0);  // (the choice's name fades in, as when the selection moves)
    g_fade = 0;
}

// orig: (none) — enhancement (quick_rings.h)
int32_t quick_rings_icon_alpha(int32_t a) {
    if (quick_rings_count() < 3) return a;
    const int n = fade_frames();
    return g_fade >= n ? a : a * g_fade / n;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_draw(uint32_t, int32_t x, int32_t y, int32_t alpha) {
    if (quick_rings_count() < 3) return;
    const int32_t w = lw(OV(0x0015f718));
    const uint64_t a = static_cast<uint64_t>(static_cast<int64_t>(alpha));
    const int32_t top = y - 0x16;  // a line above the box
    // the arrows (the pause page's frame 0xe99e, smaller: the left one as its L1's, the right one mirrored), the L1 / R1
    // glyphs beside them, the three dots between
    const int32_t arrow = hud_frame(0xe99e, 0);
    hud_sprite(arrow, x + 2, top + 0x14, 0x14, -0x14, a);
    hud_sprite(arrow, x + w - 2, top + 0x14, -0x14, -0x14, a);
    const uint64_t col = (static_cast<uint64_t>(alpha) << 24) | 0xffa888u;
    text_centered_small(x + 0x22, top + 2, col, button_text(0x14), -1);
    text_centered_small(x + w - 0x22, top + 2, col, button_text(0x15), -1);
    ring_dots(x + lw(OV(0x0015f720)), top + 0xa, g_current, col, alpha);
}

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_menu_item(uint32_t, int place) {
    if (place == 0) g_menu_ring = quick_rings_shown();  // (the page opens on the ring the quick select shows)
    return quick_rings_item(g_menu_ring, place);
}

// orig: (none) — enhancement (quick_rings.h)
bool quick_rings_menu_focus_out(uint32_t w) {
    if (quick_rings_count() < 3 || g_menu_ring == 0) return false;
    ring_write(g_menu_ring, w + 0x30);
    return true;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_menu_update(uint32_t w) {
    if (quick_rings_count() < 3) return;
    const uint32_t pressed = lwu(PAD + 0x1c4);
    const int d = ((pressed & R2) != 0 ? 1 : 0) - ((pressed & L2) != 0 ? 1 : 0);
    if (d == 0) return;
    ring_write(g_menu_ring, w + 0x30);
    g_menu_ring = ring_wrap(g_menu_ring + d);
    for (uint32_t i = 0; i < 8; ++i) sw(w + 0x30 + i * 4, quick_rings_item(g_menu_ring, static_cast<int>(i)));
    moby_sound_start(1, 0x11, lwu(w + 0x14));
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_menu_draw(uint32_t w) {
    if (quick_rings_count() < 3) return;
    const int32_t wd = lw(w + 0x20), h = lw(w + 0x24);
    const uint64_t col = 0x80ffa888ull;
    // the L2 / R2 arrows in the bottom corners (the L1 / R1 ones of 0x27e768 are in the top corners: arrow y 0x27, glyph
    // y 0xf), the arrows the other way up; the ring edited in the wheel's centre (empty on this page), its dots under it
    const int32_t arrow = hud_frame(0xe99e, 0);
    hud_sprite(arrow, 8, h - 0x27, 0x20, 0x20, 0x80);
    hud_sprite(arrow, wd - 10, h - 0x27, -0x20, 0x20, 0x80);
    text_small(0x28, h - 0x1b, col, button_text(0x16), -1);
    text_small(wd - 0x3c, h - 0x1b, col, button_text(0x17), -1);
    const uint32_t label = quick_rings_text(TEXT_FIRST + g_menu_ring);
    text_small((wd - text_width_small(label, -1)) / 2, h / 2 - 0xe, col, label, -1);
    ring_dots(wd / 2, h / 2 + 8, g_menu_ring, col, 0x80);
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_give(uint32_t id) {
    if (quick_rings_count() < 3 || id == 0 || id >= ITEMS) return;
    for (auto& ring : g_rings)
        for (uint8_t& place : ring)
            if (place == 0) {
                place = static_cast<uint8_t>(id);
                return;
            }
}

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_pc_items(std::vector<uint8_t>& d) {
    bool any = g_current != 0;
    for (const auto& ring : g_rings)
        for (uint8_t place : ring) any = any || place != 0;
    if (!any) return 0;
    constexpr uint32_t SIZE = 2 + 16;
    d.push_back(static_cast<uint8_t>(PC_ITEM));
    d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
    d.push_back(static_cast<uint8_t>(SIZE));
    d.push_back(0);
    d.push_back(PC_VERSION);
    d.push_back(static_cast<uint8_t>(g_current));
    for (const auto& ring : g_rings) d.insert(d.end(), ring, ring + 8);
    return 1;
}

// orig: (none) — enhancement (quick_rings.h)
bool quick_rings_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (id != PC_ITEM) return false;
    if (size < 18 || p[0] < 1) return true;  // (not readable: the rings left empty)
    g_current = p[1] <= 2 ? p[1] : 0;
    for (uint32_t r = 0; r < 2; ++r)
        for (uint32_t i = 0; i < 8; ++i) {
            const uint8_t item = p[2 + r * 8 + i];
            g_rings[r][i] = item < ITEMS ? item : 0;
        }
    std::printf("[quick rings] rings 2 and 3 loaded with the save (ring %d last used)\n", g_current + 1);
    return true;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_pc_reset() {
    std::memset(g_rings, 0, sizeof g_rings);
    g_current = 0;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_new_game() { quick_rings_pc_reset(); }

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_challenge_keep() {
    std::memcpy(g_kept, g_rings, sizeof g_kept);
    g_kept_current = g_current;
}

// orig: (none) — enhancement (quick_rings.h)
void quick_rings_challenge_restore() {
    for (uint32_t r = 0; r < 2; ++r)
        for (uint32_t i = 0; i < 8; ++i) {
            const uint8_t item = g_kept[r][i];
            g_rings[r][i] = item != 0 && lbu(OWNED + item) != 0 ? item : 0;
        }
    g_current = g_kept_current;
}

// orig: (none) — enhancement (quick_rings.h)
uint32_t quick_rings_text(int32_t id) {
    if (id < TEXT_FIRST || id >= TEXT_FIRST + 3) return 0;
    // per language (0x15ed88: 0 English, 2 French, 3 German, 4 Spanish, 5 Italian)
    static const char* const WORD[6] = {"Ring", "Ring", "Anneau", "Ring", "Anillo", "Anello"};
    uint32_t lang = lwu(0x0015ed88);
    if (lang >= 6) lang = 0;
    char s[0x20];
    std::snprintf(s, sizeof s, "%s %d", WORD[lang], id - TEXT_FIRST + 1);
    std::memcpy(ee_ram_ptr(LABEL), s, sizeof s);
    return LABEL;
}

}  // namespace rac1
