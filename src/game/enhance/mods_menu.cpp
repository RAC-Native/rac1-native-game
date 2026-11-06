// The Mods page (see mods_menu.h). Not a port: everything here is new, made as the Controls page (controls.cpp): a copy
// of the Camera page (0x1b5a60) with its settings widget (0x27ef00 / 0x27f0a8) in the tall box of page 0x1b6678 and a copy
// of the Options help label (0x1b4b68) in the Options help box. Each row's byte is a "step" byte (two values, both the
// row's value text, made at each lookup): cross steps it, mods_setting_changed acts and puts it back to 0.
//
// EE RAM block MOD (0xf7400..0xf8000): past the Controls page's texts (controls.cpp: up to 0xf7400), in the PS2 kernel's
// first megabyte: nothing of the game or the runtime lives there.
#include <game/enhance/mods_menu.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/mods.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t MOD = 0x000f7400u;
constexpr uint32_t PAGE = MOD;                 // the page (a copy of Camera's)
constexpr uint32_t TITLE = MOD + 0x90;         // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = MOD + 0xe0;   // its settings widget (a copy of Camera's)
constexpr uint32_t HELP_WIDGET = MOD + 0x130;  // its help label (a copy of Options')
constexpr uint32_t STEPS = MOD + 0x180;        // the shown rows' step bytes (the settings widget writes them)
constexpr uint32_t ROW_HELP = MOD + 0x190;     // the help label's texts, one per shown row
constexpr uint32_t NOTICE = MOD + 0x1c0;       // the "Mods on" line's text
constexpr uint32_t ROWS = MOD + 0x200;         // the settings widget's rows (0x18 bytes, ended by text 0)
constexpr uint32_t STRINGS = MOD + 0x340;      // the texts, made at each lookup (..0xf7c00)

// The game's data the page is made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b6678u> TALL_PAGE{};  // its slot 3 box: the Graphics page's settings box
constexpr uint32_t LANGUAGE = 0x0015ed88u;   // the game's language

constexpr int VISIBLE = 10;  // the rows the box shows at once

// The texts' ids: 0x7d00..0x7d16, below the update notice's 0x7e00.. (the ids past 0x7f00 are the other pages': the Controls
// page's reach 0x7ff5)
constexpr int32_t TEXT_FIRST = 0x7d00;
enum : int32_t { I_TITLE = 0, I_OPTIONS_HELP = 1, I_NAME = 2, I_VALUE = I_NAME + VISIBLE, I_HELP = I_VALUE + VISIBLE, I_COUNT };
static_assert(TEXT_FIRST + I_COUNT <= 0x7e00, "the Mods page's text ids end where the update notice's begin");
constexpr uint32_t STRING_SIZE[] = {0x40, 0x80, 0x30, 0x20, 0x180};  // title, Options help, names, values, help
static_assert(STRINGS + 0x40 + 0x80 + 0x30 * VISIBLE + 0x20 * VISIBLE + 0x180 <= 0x000f8000u, "the Mods page's texts end at 0xf8000");

// per language (0x15ed88: 0 English, 2 French (in English for now, as the other pages), 3 German, 4 Spanish, 5 Italian)
enum : int32_t { T_TITLE, T_OPTIONS_HELP, T_ON, T_OFF, T_ON_NEXT, T_OFF_NEXT, T_BROKEN, T_BY, T_VERSION, T_NEXT_START, T_BROKEN_HELP, T_NOTICE, T_COUNT };
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Mods", "The mods in the mods folder, on or off", "On", "Off", "On at restart", "Off at restart", "Not loaded", "by", "version",
     "The change applies at the next start.", "Its mod.json could not be read.", "Mods on"},
    {"Mods", "Die Mods im Ordner mods, an oder aus", "An", "Aus", "An nach Neustart", "Aus nach Neustart", "Nicht geladen", "von", "Version",
     "Die Aenderung gilt ab dem naechsten Start.", "Seine mod.json ist nicht lesbar.", "Mods an"},
    {"Mods", "Los mods de la carpeta mods, si o no", "Si", "No", "Si al reiniciar", "No al reiniciar", "No cargado", "de", "version",
     "El cambio se aplica al proximo inicio.", "Su mod.json no se puede leer.", "Mods activos"},
    {"Mod", "Le mod nella cartella mods, attive o no", "Si", "No", "Si al riavvio", "No al riavvio", "Non caricata", "di", "versione",
     "La modifica vale dal prossimo avvio.", "Il suo mod.json non si legge.", "Mod attive"},
};

int g_top = 0;       // the first mod the window shows
int g_prev_sel = 0;  // the selection at the last frame (the window moves when up / down go past its ends)
bool g_built = false;

// orig: (none) — enhancement: the table of the text language now
const char* const* texts() {
    switch (lwu(LANGUAGE)) {
        case 3: return TEXTS[1];
        case 4: return TEXTS[2];
        case 5: return TEXTS[3];
        default: return TEXTS[0];
    }
}

// orig: (none) — enhancement: a manifest's text in the game's font: printable ASCII only (others '?')
std::string font_safe(const std::string& s) {
    std::string out;
    for (const char c : s) out += c >= 0x20 && c < 0x7f ? c : '?';
    return out;
}

int mod_count() { return static_cast<int>(mods_list().size()); }

// orig: (none) — enhancement: row `i`'s value (the choice, and whether it waits for the next start)
std::string row_value(int i) {
    if (i < 0 || i >= mod_count()) return "";
    const ModInfo& m = mods_list()[static_cast<size_t>(i)];
    const char* const* t = texts();
    if (!m.error.empty()) return t[T_BROKEN];
    if (m.enabled != m.active) return t[m.enabled ? T_ON_NEXT : T_OFF_NEXT];
    return t[m.enabled ? T_ON : T_OFF];
}

// orig: (none) — enhancement: the help box's text: the selected mod's description, author, version
std::string help_text(int i) {
    if (i < 0 || i >= mod_count()) return "";
    const ModInfo& m = mods_list()[static_cast<size_t>(i)];
    const char* const* t = texts();
    if (!m.error.empty()) return font_safe(m.folder) + ": " + t[T_BROKEN_HELP];
    std::string s = font_safe(m.description);
    std::string who;
    if (!m.author.empty()) who = std::string(t[T_BY]) + " " + font_safe(m.author);
    if (!m.version.empty()) who += (who.empty() ? "" : ", ") + std::string(t[T_VERSION]) + " " + font_safe(m.version);
    if (!who.empty()) s += (s.empty() ? "" : " (") + who + (s.empty() ? "" : ")");
    if (m.enabled != m.active) s += (s.empty() ? "" : " ") + std::string(t[T_NEXT_START]);
    return s;
}

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}
// orig: (none) — enhancement: the page `page`'s slots holding `from` pointed at `to`
void slot_replace(uint32_t page, uint32_t from, uint32_t to) {
    for (uint32_t i = 0; i < 14; ++i)
        if (lwu(page + 0x44 + i * 4) == from) sw(page + 0x44 + i * 4, to);
}

// orig: (none) — enhancement: the widget's rows for the window from g_top (as many as the mods, ten at most)
void rows_write() {
    uint32_t row = ROWS;
    const int n = std::min(VISIBLE, mod_count());
    for (int i = 0; i < n; ++i, row += 0x18) {
        const uint32_t vt = static_cast<uint32_t>(TEXT_FIRST + I_VALUE + i);
        sw(row, static_cast<uint32_t>(TEXT_FIRST + I_NAME + i));
        sw(row + 4, STEPS + static_cast<uint32_t>(i));
        sw(row + 8, vt);  // (two values, both the value's text: cross steps the byte, mods_setting_changed acts)
        sw(row + 0xc, vt);
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sb(STEPS + static_cast<uint32_t>(i), 0);
        sw(ROW_HELP + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(TEXT_FIRST + I_HELP));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
}

// orig: (none) — enhancement: mod `i` switched on / off (applies at the next start)
void row_step(int i) {
    if (i < 0 || i >= mod_count()) return;
    mods_set_enabled(static_cast<size_t>(i), !mods_list()[static_cast<size_t>(i)].enabled);
}
}  // namespace

// orig: (none) — enhancement (mods_menu.h)
uint32_t mods_page_install(uint32_t parent) {
    g_built = false;
    if (g_advanced_disabled || mods_list().empty()) return 0;
    g_top = 0;
    g_prev_sel = 0;
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    ee_copy(HELP_WIDGET, OPTIONS_HELP, 0x50);
    sw(TITLE + 0x34, static_cast<uint32_t>(TEXT_FIRST + I_TITLE));
    rows_write();
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    sw(ROWS_WIDGET + 0x40, 0);
    // the help label: the focused widget's +0x40 (the selection, mirrored) as the index into its texts (flag 0x80); its
    // box the Options page's help box (slot 1), the rows in page 0x1b6678's taller box (as controls.cpp)
    sw(HELP_WIDGET + 0x30, 0x80);
    sw(HELP_WIDGET + 0x34, ROW_HELP);
    sw(PAGE + 0x38, parent);
    sw(PAGE + 0x40, ROWS_WIDGET);
    slot_replace(PAGE, CAMERA_TITLE, TITLE);
    slot_replace(PAGE, CAMERA_SETTINGS, ROWS_WIDGET);
    sw(PAGE + 0xc, lwu(TALL_PAGE + 0xc));
    sw(PAGE + 4, lwu(OPTIONS_PAGE + 4));
    sw(PAGE + 0x44 + 4, HELP_WIDGET);
    sw(PAGE + 0x80, 0);
    g_built = true;
    return PAGE;
}

// orig: (none) — enhancement (mods_menu.h)
uint32_t mods_page() { return g_built ? PAGE : 0; }
// orig: (none) — enhancement (mods_menu.h)
int32_t mods_title_text() { return TEXT_FIRST + I_TITLE; }
// orig: (none) — enhancement (mods_menu.h)
int32_t mods_help_text() { return TEXT_FIRST + I_OPTIONS_HELP; }

// orig: (none) — enhancement (mods_menu.h)
uint32_t mods_text(int32_t id) {
    const int32_t i = id - TEXT_FIRST;
    if (i < 0 || i >= I_COUNT || mods_list().empty()) return 0;
    std::string s;
    int kind = 0, slot = 0;  // (the string's kind: title, Options help, name, value, help; its slot)
    if (i == I_TITLE) {
        s = texts()[T_TITLE];
    } else if (i == I_OPTIONS_HELP) {
        s = texts()[T_OPTIONS_HELP];
        kind = 1;
    } else if (i < I_VALUE) {
        slot = i - I_NAME;
        const int m = g_top + slot;
        if (m < mod_count()) s = font_safe(mods_list()[static_cast<size_t>(m)].name);
        kind = 2;
    } else if (i < I_HELP) {
        slot = i - I_VALUE;
        s = row_value(g_top + slot);
        kind = 3;
    } else {
        s = help_text(g_top + lw(ROWS_WIDGET + 0x38));
        kind = 4;
    }
    uint32_t at = STRINGS;
    for (int k = 0; k < kind; ++k) at += STRING_SIZE[k] * (k >= 2 && k <= 3 ? VISIBLE : 1);
    at += STRING_SIZE[kind] * static_cast<uint32_t>(slot);
    ee_zero(at, STRING_SIZE[kind]);  // (the PS2's bytes: a big-endian host's words hold them swizzled)
    ee_copy_in(at, s.c_str(), std::min<size_t>(s.size(), STRING_SIZE[kind] - 1));
    return at;
}

// orig: (none) — enhancement (mods_menu.h)
bool mods_setting_changed(uint32_t byte) {
    if (!g_built || byte < STEPS || byte >= STEPS + VISIBLE) return false;
    sb(byte, 0);
    row_step(g_top + static_cast<int>(byte - STEPS));
    return true;
}

// orig: (none) — enhancement (mods_menu.h)
bool mods_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (!g_built || w != ROWS_WIDGET) return false;
    int sel = lw(w + 0x38);
    // the window moved when up / down go past its ends (the widget stops there: its rows end)
    if ((pressed & 0x4000u) != 0 && g_prev_sel == VISIBLE - 1 && sel == VISIBLE - 1 && g_top + VISIBLE < mod_count()) {
        ++g_top;
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    } else if ((pressed & 0x1000u) != 0 && g_prev_sel == 0 && sel == 0 && g_top > 0) {
        --g_top;
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    }
    sel = lw(w + 0x38);
    g_prev_sel = sel;
    sw(w + 0x40, static_cast<uint32_t>(sel));  // (the help label's index)
    if ((pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b < STEPS || b >= STEPS + VISIBLE) return false;
    row_step(g_top + static_cast<int>(b - STEPS));  // (left / right: two values, either way the other)
    return true;
}

// orig: (none) — enhancement (mods_menu.h)
void mods_notice_draw() {
    if (!mods_on()) return;  // (a vanilla run: never)
    const std::string s = std::string(texts()[T_NOTICE]) + ": " + std::to_string(mods_active().size());
    ee_zero(NOTICE, 0x40);  // (the PS2's bytes: a big-endian host's words hold them swizzled)
    ee_copy_in(NOTICE, s.c_str(), std::min<size_t>(s.size(), 0x3f));
    // the small font (effect texture 1, its table 0x1c3150), as the menus' buttons draw it (the GS's alpha blend and test), its
    // shadow first; at the frame's bottom left (0x13e504: the frame's height)
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0x2004b);
    const int32_t y = lw(0x0013e504) - 0x24;
    text_draw(0x19, y + 1, 0x80000000ull, NOTICE, -1, effect_tex0(1), OV(0x001c3150));
    text_draw(0x18, y, 0x80a0a0a0ull, NOTICE, -1, effect_tex0(1), OV(0x001c3150));
}

}  // namespace rac1
