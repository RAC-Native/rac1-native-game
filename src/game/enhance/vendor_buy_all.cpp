// The Vendor's "All Ammo" entry (see vendor_buy_all.h). Not a port: everything here is new, built on the game's own
// Vendor state (the item list 0x1ca4c0), its prices (the item table 0x1c40b0), its ammo function and its texts.
//
// The game's data used (Veldin's addresses, relocated):
//   the item list 0x1ca4c0: +0 the screens' state, +0x1c the Vendor moby (its sounds), +0x40 opened by the PDA, +0x4c the
//     wheel's scroll, +0x58 the cursor, +0x5c the purchase's step (0: none), the entries from +0xd0 (0x14 bytes: +0 the
//     item, +4 1 for an ammo entry, +8 0 (a purchase starts only when it is 0), +0x10 the item's word), their count +0x210
//     (item_list_build 0x29a400; the 17th entry would overlap the count: 16 at most);
//   the item table 0x1c40b0 (0x18 bytes an item): +8 a round's price, +0xa at the PDA, +0xe the most ammo;
//   the items' HUD table 0x179ac0 (0x4c bytes an item): +0x38 the tile's HUD animation (frame 2: the ammo tile);
//   the ammo 0x13d428[item], the ammo bought 0x13dd70[item], the bolts 0x15ed98;
//   0x161e18 / 0x161e10: the screens busy (an opening, the quantity box): the list does not move; 0x161e30 the pad's
//     buttons of the frame before (a press counts once); 0x161e38 the cursor moved (the hologram made again).
#include <game/enhance/vendor_buy_all.h>

#include <game/enhance/advanced.h>

#include <game/lib/libc.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vendor_tr.h>

#include <cstdio>
#include <cstdlib>

namespace rac1 {

namespace {
constexpr OvConst<0x001ca4c0u> LIST{};
constexpr OvConst<0x001c40b0u> ITEMS{};
constexpr OvConst<0x00179ac0u> HUD_ITEMS{};
constexpr OvConst<0x0015f5c4u> MODE{};
constexpr OvConst<0x00161e18u> BUSY_OPEN{};
constexpr OvConst<0x00161e10u> BUSY_QUANTITY{};
constexpr OvConst<0x00161e30u> PAD_BEFORE{};
constexpr OvConst<0x00161e38u> CURSOR_MOVED{};
constexpr uint32_t AMMO = 0x0013d428, AMMO_BOUGHT = 0x0013dd70, BOLTS = 0x0015ed98;
constexpr uint32_t PAD = 0x0013c940;
constexpr uint32_t CROSS = 0x40;
constexpr uint32_t MARK = 0x4c4c41u;  // the entry's +8 ("ALL"): any word but 0
constexpr int32_t MAX_ENTRIES = 16;
// The entry's texts in EE memory (the text lookups return addresses): the block VBA (0xfa000..0xfa100), in the PS2
// kernel's first megabyte as the Advanced block (advanced.cpp, 0xf0000..0xfa000 used by the menus): nothing of the
// game or the runtime lives there
constexpr uint32_t VBA = 0x000fa000u;
constexpr uint32_t SIGN = VBA, TITLE1 = VBA + 0x40, TITLE2 = VBA + 0x60;

// The entry's texts by the game's language (0x15ed88: 0 English, 2 French, 3 German, 4 Spanish, 5 Italian; the font
// has no accented capitals looked at yet, so none are used): the sign's line, the monitor's two title lines
struct Texts {
    const char* sign;
    const char* title1;
    const char* title2;
};
constexpr Texts TEXTS[6] = {
    {"ALL AMMO", "All", "Ammo"},
    {"ALL AMMO", "All", "Ammo"},
    {"TOUTES LES MUNITIONS", "Toutes les", "munitions"},
    {"ALLE MUNITION", "Alle", "Munition"},
    {"TODA LA MUNICION", "Toda la", "municion"},
    {"TUTTE LE MUNIZIONI", "Tutte le", "munizioni"},
};

int g_price_calls = -1;     // the monitor's prices drawn for the entry this frame (-1: the monitor is not on it)
bool g_tile = false;        // the strip's next sprite is the entry's tile

// orig: (none) — enhancement: the entry's texts in the game's language
const Texts& texts() {
    const uint32_t lang = lwu(0x0015ed88);
    return TEXTS[lang < 6 ? lang : 0];
}

// orig: (none) — enhancement: testing, RAC1_VENDOR_TRACE=<n>: every n frames of the Vendor's screens, its state and
// each entry of its list (the item, its ammo / the most, a round's price; the entry "ALL")
int trace_every() {
    static const int n = std::getenv("RAC1_VENDOR_TRACE") != nullptr ? std::atoi(std::getenv("RAC1_VENDOR_TRACE")) : 0;
    return n;
}

// orig: (none) — enhancement: the list's entry `i`
uint32_t entry(int32_t i) { return LIST + 0xd0 + static_cast<uint32_t>(i) * 0x14; }

// orig: (none) — enhancement: the list's entry `i` is the All Ammo entry
bool is_all(int32_t i) { return i >= 0 && i < lw(LIST + 0x210) && i < MAX_ENTRIES && lwu(entry(i) + 8) == MARK; }

// orig: (none) — enhancement: the cursor is on the All Ammo entry (the setting on, the Vendor's screens up)
bool on_all() { return advanced_vendor_buy_all() && lw(MODE) == 5 && is_all(lw(LIST + 0x58)); }

// orig: (none) — enhancement: a round of ammo's price for the item `w`, the PDA's or the Vendor's (`pda` -1: by who
// opened the screens)
int32_t round_price(uint32_t w, int pda = -1) {
    const bool p = pda < 0 ? lw(LIST + 0x40) != 0 : pda != 0;
    return static_cast<int32_t>(lhu(ITEMS + w * 0x18 + (p ? 0xau : 8u)));
}

// orig: (none) — enhancement: the list's entry `i`: one of the game's ammo entries, its item in `w`
bool ammo_entry(int32_t i, uint32_t& w) {
    const uint32_t e = entry(i);
    w = lwu(e);
    return lw(e + 4) == 1 && lwu(e + 8) != MARK && w < 0x25;
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

// orig: (none) — enhancement: a host string into EE memory at `a` (at most `n` bytes with its end)
uint32_t ee_string(uint32_t a, const char* s, uint32_t n) {
    uint32_t i = 0;
    for (; s[i] != 0 && i + 1 < n; ++i) sb(a + i, static_cast<uint8_t>(s[i]));
    sb(a + i, 0);
    return a;
}

// orig: (none) — enhancement: the sign's text (the screens' marquee, 0x29b070), a game text id (0x203ad8)
void sign_text_id(int32_t id) { ovl_29b070_tr(sub_00203ad8(id), 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f); }

// orig: (none) — enhancement: a Vendor sound (the moby's sound `k`: 0 error, 2 full, 7 purchased)
void vendor_sound(int32_t k) { moby_sound_start(k, 0, lwu(LIST + 0x1c)); }

// orig: (none) — enhancement: the purchase: the steps of the game's ammo purchase (0x29ba78, its step 4) for each ammo
// entry in the list's order, as many rounds as the bolts pay for; then once the Vendor's thanks, the list rebuilt (the
// entry first again, the cursor on it), the hologram made again, the sign's message and the sound
void buy_all() {
    const int32_t n = lw(LIST + 0x210);
    int32_t bought = 0;
    bool short_of_bolts = false;
    for (int32_t i = 0; i < n && i < MAX_ENTRIES; ++i) {
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
        const uint32_t before = lwu(entry(0));
        ovl_29b4d8_tr(3, 0, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f);  // (the Vendor's thanks, as after an ammo purchase)
        item_list_build(lw(LIST + 0x40) > 0 ? 1 : 0);
        sw(LIST + 0x58, 0);
        if (is_all(0) && lwu(entry(0)) != before) sw(CURSOR_MOVED, 1);  // (the entry's weapon changed: the game makes the hologram again)
        sign_text_id(0x4f5f);
        vendor_sound(7);
    } else if (short_of_bolts) {
        sign_text_id(0x4f60);
        vendor_sound(0);
    } else {
        vendor_sound(2);
    }
}

// orig: (none) — enhancement: the HUD animation of the item `w`'s tile (0x179ac0[w] +0x38)
uint32_t tile_key(uint32_t w) { return lhu(HUD_ITEMS + w * 0x4c + 0x38); }
}  // namespace

// orig: (none) — enhancement (vendor_buy_all.h)
int32_t vendor_buy_all_price(int32_t* weapons, int pda) {
    int32_t total = 0, count = 0;
    const int32_t n = lw(LIST + 0x210);
    for (int32_t i = 0; i < n && i < MAX_ENTRIES; ++i) {
        uint32_t w = 0;
        if (!ammo_entry(i, w)) continue;
        const int32_t missing = rounds_missing(w);
        if (missing <= 0) continue;
        total += missing * round_price(w, pda);
        ++count;
    }
    if (weapons != nullptr) *weapons = count;
    return total;
}

// orig: (none) — enhancement (vendor_buy_all.h): the entry first, when the list has an ammo entry and room for one more.
// Its item is the list's weapon not full whose round is the cheapest (else the first weapon): the game shows that weapon
// for the entry (the hologram, the monitor's picture, the ammo counter), one the purchase still fills
void vendor_buy_all_list_built() {
    if (!advanced_vendor_buy_all()) return;
    const int32_t n = lw(LIST + 0x210);
    if (n <= 0 || n >= MAX_ENTRIES) return;
    int32_t first = -1, cheapest = -1, best = 0;
    for (int32_t i = 0; i < n; ++i) {
        uint32_t w = 0;
        if (!ammo_entry(i, w)) continue;
        if (first < 0) first = i;
        if (rounds_missing(w) > 0 && (cheapest < 0 || round_price(w) < best)) {
            cheapest = i;
            best = round_price(w);
        }
    }
    if (first < 0) return;
    const uint32_t w = lwu(entry(cheapest >= 0 ? cheapest : first));
    for (int32_t i = n; i > 0; --i)
        for (uint32_t o = 0; o < 0x14; o += 4) sw(entry(i) + o, lwu(entry(i - 1) + o));
    sw(entry(0), w);
    sw(entry(0) + 4, 1);
    sw(entry(0) + 8, MARK);
    sw(entry(0) + 0x10, lwu(OV(0x001c2108) + w * 0x14));
    sw(LIST + 0x210, static_cast<uint32_t>(n + 1));
}

// orig: (none) — enhancement (vendor_buy_all.h)
void vendor_buy_all_input() {
    static uint32_t frames = 0;  // (the Vendor's frames, for the trace: the game's frame counters stand still there)
    if (const int t = trace_every(); t > 0 && lw(MODE) == 5 && frames++ % static_cast<uint32_t>(t) == 0) {
        std::printf("[vendor] state %d step %d busy %d/%d scroll %d cursor %d count %d pda %d bolts %d price %d |", lw(LIST), lw(LIST + 0x5c),
                    lw(BUSY_OPEN), lw(BUSY_QUANTITY), lw(LIST + 0x4c), lw(LIST + 0x58), lw(LIST + 0x210), lw(LIST + 0x40), lw(BOLTS),
                    advanced_vendor_buy_all() ? vendor_buy_all_price() : -1);
        for (int32_t i = 0; i < lw(LIST + 0x210) && i < MAX_ENTRIES; ++i) {
            uint32_t w = 0;
            if (is_all(i)) std::printf(" ALL(%02x)", lwu(entry(i)));
            else if (ammo_entry(i, w)) std::printf(" %02x:%d/%d@%d", w, lw(AMMO + w * 4), lhu(ITEMS + w * 0x18 + 0xe), round_price(w));
            else std::printf(" %02x:-", w);
        }
        std::printf("\n");
    }
    if (!on_all() || !list_idle()) return;
    // (Cross as the game reads it, 0x29c648: pressed this frame and not the frame before; the game itself starts no
    // purchase on the entry, its +8 word not 0)
    if ((lwu(PAD + 0x1c4) & CROSS) != 0 && (lwu(PAD_BEFORE) & CROSS) == 0) buy_all();
}

// orig: (none) — enhancement (vendor_buy_all.h): on the entry, the item monitor's two title lines (0x29e198, an ammo
// entry's: the weapon's name, then "Ammo") and the sign's text when the cursor arrives on it (0x29c648's four moves)
uint32_t vendor_buy_all_text(uint32_t ra, int32_t id) {
    if (on_all()) {
        switch (ra) {
            case NOT_ADDR(0x0029e248u):
                g_price_calls = 0;
                return ee_string(TITLE1, texts().title1, 0x20);
            case NOT_ADDR(0x0029e270u): return ee_string(TITLE2, texts().title2, 0x20);
            case NOT_ADDR(0x0029d080u):
            case NOT_ADDR(0x0029d1a4u):
            case NOT_ADDR(0x0029d290u):
            case NOT_ADDR(0x0029d3d0u): return ee_string(SIGN, texts().sign, 0x40);
            default: break;
        }
    }
    return sub_00203ad8(id);
}

// orig: (none) — enhancement (vendor_buy_all.h): on the entry, the monitor's prices (0x29de40, its two formats: under
// 1000, and the thousands with the language's separator) are the totals: first the Vendor's, then (opened by the PDA,
// over the struck one) the PDA's
int32_t vendor_buy_all_sprintf(uint32_t ra, uint32_t dst, uint32_t fmt, const std::vector<uint64_t>& args) {
    if ((ra == NOT_ADDR(0x0029de80u) || ra == NOT_ADDR(0x0029de94u)) && g_price_calls >= 0 && on_all()) {
        const int32_t v = vendor_buy_all_price(nullptr, g_price_calls++ == 0 ? 0 : 1);
        if (v < 1000) return libc_sprintf(dst, OV(0x00161160), {static_cast<uint64_t>(static_cast<int64_t>(v))});
        return libc_sprintf(dst, OV(0x00161158),
                            {static_cast<uint64_t>(static_cast<int64_t>(v / 1000)), static_cast<uint64_t>(static_cast<int64_t>(v % 1000))});
    }
    return libc_sprintf(dst, fmt, args);
}

// orig: (none) — enhancement (vendor_buy_all.h): the strip's tile of the entry (0x29dea0: a short list's tiles in the
// list's order, s0 the entry's +4; a long list's wheel, tile s1 = -2 .. 6 around the cursor, the entry (2 n + s1 +
// cursor - 3) mod n): its sprite is drawn by vendor_buy_all_hud_sprite
int32_t vendor_buy_all_hud_frame(uint32_t ra, uint32_t key, int32_t i, uint32_t s0, uint32_t s1) {
    g_tile = false;
    if (advanced_vendor_buy_all() && lw(MODE) == 5) {
        const int32_t n = lw(LIST + 0x210);
        int32_t k = -1;
        if (ra == NOT_ADDR(0x0029dfc0u) || ra == NOT_ADDR(0x0029dfd0u)) {
            k = static_cast<int32_t>((s0 - (LIST + 0xd4)) / 0x14);
        } else if ((ra == NOT_ADDR(0x0029e124u) || ra == NOT_ADDR(0x0029e134u)) && n > 0) {
            k = (2 * n + static_cast<int32_t>(s1) + lw(LIST + 0x58) - 3) % n;
        }
        g_tile = is_all(k);
    }
    return hud_frame(key, i);
}

// orig: (none) — enhancement (vendor_buy_all.h): the entry's tile: the tiles of the list's first four weapons, a
// quarter each (2 x 2; with fewer weapons they repeat), with the game's own HUD sprites
void vendor_buy_all_hud_sprite(uint32_t ra, int32_t idx, int32_t x, int32_t y, int32_t w, int32_t h, uint64_t a) {
    if (!g_tile || (ra != NOT_ADDR(0x0029dfecu) && ra != NOT_ADDR(0x0029e158u))) return hud_sprite(idx, x, y, w, h, a);
    g_tile = false;
    uint32_t items[4];
    int32_t m = 0;
    for (int32_t i = 0; i < lw(LIST + 0x210) && i < MAX_ENTRIES && m < 4; ++i) {
        uint32_t it = 0;
        if (ammo_entry(i, it)) items[m++] = it;
    }
    if (m < 2) return hud_sprite(idx, x, y, w, h, a);
    for (int32_t q = 0; q < 4; ++q) {
        const int32_t f = hud_frame(tile_key(items[q % m]), 0);
        hud_sprite(f, x + (q & 1) * (w / 2), y + (q >> 1) * (h / 2), w / 2, h / 2, a);
    }
}

// orig: (none) — enhancement (vendor_buy_all.h): the screens' opening (0x29a6b0, after the list is built and the cursor
// put in its middle): the cursor on the entry (the wheel's scroll is 0 either way)
void vendor_buy_all_camera_reset(uint32_t ra) {
    if (ra == NOT_ADDR(0x0029a838u) && advanced_vendor_buy_all() && is_all(0)) sw(LIST + 0x58, 0);
    camera_reset();
}

}  // namespace rac1
