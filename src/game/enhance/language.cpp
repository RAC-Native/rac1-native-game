// The Language page (see language.h). Not a port: everything here is new, written on top of the original's menu data
// so that the page looks and behaves like the game's own, as the Advanced page (advanced.cpp) it is made like: a copy
// of the Camera page (0x1b5a60), its title label and its settings widget (0x27ef00 / 0x27f0a8), with our rows.
//
// Each row's byte is a "step" byte (two values, both the row's value text, made at each lookup, as the Graphics page's
// rows): cross steps it, language_setting_changed moves the setting on and puts the byte back to 0. Left / right step
// back / forward (language_settings_dpad).
//
// EE RAM block LNG (0xfa200..0xfa800): past the Vendor's 0xfa000..0xfa100 (vendor_buy_all.cpp), before the Advanced
// page's texts at 0xfb000, in the PS2 kernel's first megabyte: nothing of the game or the runtime lives there.
#include <game/enhance/language.h>

#include <game/enhance/advanced.h>
#include <game/enhance/movie_subtitles.h>
#include <game/enhance/voice_language.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdio>
#include <cstring>
#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t LNG = 0x000fa200u;
constexpr uint32_t PAGE = LNG;                 // the page (a copy of Camera's)
constexpr uint32_t TITLE = LNG + 0x90;         // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = LNG + 0xe0;   // its settings widget (a copy of Camera's)
constexpr uint32_t STEPS = LNG + 0x130;        // the rows' step bytes (by Row: the settings widget writes them)
constexpr uint32_t ROWS = LNG + 0x140;         // the widget's rows (0x18 bytes, ended by text 0)
constexpr uint32_t STRINGS = LNG + 0x200;      // the texts, made at each lookup (..0xfa800)
constexpr uint32_t STRING_SIZE = 0x80;

constexpr uint32_t LANGUAGE = 0x0015ed88u;   // the game's language: the text's
constexpr uint32_t SUBTITLES = 0x0015ee40u;  // the game's Subtitles switch (one of its options, in its save)

// The game's data the page is made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};

enum Row : uint32_t { R_TEXT, R_VOICE, R_SUBTITLES, R_MOVIE_SUBTITLES, ROW_COUNT };

// The texts' ids (after the Controls page's 0x7fa0..0x7fb6), each made at its lookup into its string
constexpr int32_t TEXT_FIRST = 0x7fc0;
enum : int32_t { I_TITLE, I_HELP, I_NAME, I_VALUE = I_NAME + ROW_COUNT, I_COUNT = I_VALUE + ROW_COUNT };
static_assert(I_COUNT * STRING_SIZE <= 0x600, "the Language page's texts end at 0xfa800");

// per language (0x15ed88: 0 English, 2 French (in English for now, as the other pages: the font's accented letters not
// looked at yet), 3 German, 4 Spanish, 5 Italian)
// (Movie Subtitles: our own, made at the setup with speech to text, movie_subtitles.h; off by default)
enum : int32_t { T_TITLE, T_HELP, T_TEXT, T_VOICE, T_SUBTITLES, T_MOVIE_SUBTITLES, T_OFF, T_ON, T_SAME, T_COUNT };
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Language", "The language of the texts, of the voices and the subtitles", "Text", "Voice", "Subtitles", "Movie Subtitles", "Off", "On", "Same as Text"},
    {"Sprache", "Die Sprache der Texte, der Stimmen und die Untertitel", "Text", "Stimmen", "Untertitel", "Film-Untertitel", "Aus", "An", "Wie der Text"},
    {"Idioma", "El idioma de los textos, de las voces y los subtitulos", "Texto", "Voces", "Subtitulos", "Subtitulos de videos", "No", "Si", "Como el texto"},
    {"Lingua", "La lingua dei testi, delle voci e i sottotitoli", "Testo", "Voci", "Sottotitoli", "Sottotitoli filmati", "No", "Si", "Come il testo"},
};
// The languages' names in each of those, by the game's codes (0 English, 1 Japanese, 2 French, 3 German, 4 Spanish,
// 5 Italian)
constexpr const char* NAMES[4][6] = {
    {"English", "Japanese", "French", "German", "Spanish", "Italian"},
    {"Englisch", "Japanisch", "Franzoesisch", "Deutsch", "Spanisch", "Italienisch"},
    {"Ingles", "Japones", "Frances", "Aleman", "Espanol", "Italiano"},
    {"Inglese", "Giapponese", "Francese", "Tedesco", "Spagnolo", "Italiano"},
};
// The text languages, in the order of the game's own list
constexpr uint32_t TEXT_LANGUAGES[] = {0, 2, 3, 4, 5};

int32_t g_voice = -1;      // the Voice row's choice as kept in the preferences (voice_language.h; RAC1_VOICE_LANGUAGE over it)
int32_t g_subtitles = -1;  // the Subtitles choice: -1 never chosen (the game's own), 0 off, 1 on
bool g_front = false;      // the page is the main menu's (the Text row shown)

// orig: (none) — enhancement: the table of the text language now
uint32_t table() {
    switch (lwu(LANGUAGE)) {
        case 3: return 1;
        case 4: return 2;
        case 5: return 3;
        default: return 0;
    }
}

// orig: (none) — enhancement: a row's value as text
const char* value_text(uint32_t row) {
    const uint32_t t = table();
    if (row == R_TEXT) {
        const uint32_t l = lwu(LANGUAGE);
        return NAMES[t][l < 6 ? l : 0];
    }
    if (row == R_VOICE) {
        const int32_t c = g_voice == 1 && !voice_japanese_available() ? -1 : g_voice;  // (the disc gone: the text's)
        return c >= 0 && c < 6 ? NAMES[t][c] : TEXTS[t][T_SAME];
    }
    if (row == R_MOVIE_SUBTITLES) return TEXTS[t][movie_subtitles_choice() ? T_ON : T_OFF];
    return TEXTS[t][lbu(SUBTITLES) != 0 ? T_ON : T_OFF];
}

// orig: (none) — enhancement: the string of text `i` (I_...) written and its address returned
uint32_t string_put(int32_t i, const char* s) {
    const uint32_t at = STRINGS + static_cast<uint32_t>(i) * STRING_SIZE;
    ee_zero(at, STRING_SIZE);  // (the PS2's bytes: a big-endian host's words hold them swizzled)
    ee_copy_in(at, s, std::min<size_t>(std::strlen(s), STRING_SIZE - 1));
    return at;
}

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}

// orig: (none) — enhancement: `n` stepped by `dir` (+1 / -1) within 0..count-1, wrapping
uint32_t wrap(uint32_t n, int32_t dir, uint32_t count) {
    return static_cast<uint32_t>((static_cast<int32_t>(n) + dir + static_cast<int32_t>(count)) % static_cast<int32_t>(count));
}

// orig: (none) — enhancement: row `row` stepped by `dir` (+1 / -1): the setting changed, the preferences written
void row_step(uint32_t row, int32_t dir) {
    if (row == R_TEXT) {
        uint32_t at = 0;
        for (uint32_t i = 0; i < 5; ++i)
            if (TEXT_LANGUAGES[i] == lwu(LANGUAGE)) at = i;
        sw(LANGUAGE, TEXT_LANGUAGES[wrap(at, dir, 5)]);  // (as the game's own list's action 9; console.txt keeps it)
        std::printf("[language] Text: %u\n", lwu(LANGUAGE));
    } else if (row == R_VOICE) {
        const std::vector<int32_t> c = voice_choices();
        uint32_t at = 0;
        for (size_t i = 0; i < c.size(); ++i)
            if (c[i] == g_voice) at = static_cast<uint32_t>(i);
        g_voice = c[wrap(at, dir, static_cast<uint32_t>(c.size()))];
        voice_choice_set(g_voice);
        std::printf("[language] Voice: %s\n", voice_choice_name(g_voice));
    } else if (row == R_MOVIE_SUBTITLES) {
        movie_subtitles_set(!movie_subtitles_choice());
        std::printf("[language] Movie Subtitles: %s\n", movie_subtitles_choice() ? "on" : "off");
    } else {
        g_subtitles = lbu(SUBTITLES) != 0 ? 0 : 1;
        sb(SUBTITLES, static_cast<uint32_t>(g_subtitles));
        std::printf("[language] Subtitles: %s\n", g_subtitles != 0 ? "on" : "off");
    }
    sb(STEPS + row, 0);
    advanced_prefs_save();
}
}  // namespace

// orig: (none) — enhancement (language.h)
uint32_t language_page_install(uint32_t parent, bool front) {
    g_front = front;
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    sw(TITLE + 0x34, static_cast<uint32_t>(TEXT_FIRST + I_TITLE));
    uint32_t row = ROWS;
    for (uint32_t r = front ? R_TEXT : R_VOICE; r < ROW_COUNT; ++r, row += 0x18) {
        const uint32_t value = static_cast<uint32_t>(TEXT_FIRST + I_VALUE) + r;
        sw(row, static_cast<uint32_t>(TEXT_FIRST + I_NAME) + r);
        sw(row + 4, STEPS + r);
        sw(row + 8, value);  // (two values, both the value's text: cross steps the byte, language_setting_changed the setting)
        sw(row + 0xc, value);
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sb(STEPS + r, 0);
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    sw(PAGE + 0x38, parent);
    sw(PAGE + 0x40, ROWS_WIDGET);
    for (uint32_t i = 0; i < 14; ++i) {
        const uint32_t slot = PAGE + 0x44 + i * 4;
        if (lwu(slot) == CAMERA_TITLE) sw(slot, TITLE);
        else if (lwu(slot) == CAMERA_SETTINGS) sw(slot, ROWS_WIDGET);
    }
    sw(PAGE + 0x80, 0);
    return PAGE;
}

// orig: (none) — enhancement (language.h)
uint32_t language_page() { return lwu(PAGE + 0x40) == ROWS_WIDGET ? PAGE : 0; }
// orig: (none) — enhancement (language.h)
int32_t language_title_text() { return TEXT_FIRST + I_TITLE; }
// orig: (none) — enhancement (language.h)
int32_t language_help_text() { return TEXT_FIRST + I_HELP; }

// orig: (none) — enhancement (language.h)
uint32_t language_text(int32_t id) {
    if (id < TEXT_FIRST || id >= TEXT_FIRST + I_COUNT) return 0;
    const int32_t k = id - TEXT_FIRST;
    const char* const* t = TEXTS[table()];
    if (k == I_TITLE) return string_put(k, t[T_TITLE]);
    if (k == I_HELP) return string_put(k, t[T_HELP]);
    if (k < I_VALUE) return string_put(k, t[T_TEXT + (k - I_NAME)]);
    return string_put(k, value_text(static_cast<uint32_t>(k - I_VALUE)));
}

// orig: (none) — enhancement (language.h)
bool language_setting_changed(uint32_t byte) {
    if (byte < STEPS || byte >= STEPS + ROW_COUNT) return false;
    if (lbu(byte) != 0) row_step(byte - STEPS, 1);
    return true;
}

// orig: (none) — enhancement (language.h)
bool language_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (w != ROWS_WIDGET || (pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b < STEPS || b >= STEPS + ROW_COUNT) return false;
    row_step(b - STEPS, (pressed & 0x8000u) != 0 ? -1 : 1);
    return true;
}

// orig: (none) — enhancement (language.h)
void language_main_entries(uint32_t entries, uint32_t n) {
    // the game's Language entry: action 3 (a page), the page's focused widget a list whose flag 0x800 ticks the entry of
    // the language 0x15ed88 (the game's language list, 0x1b8b68 in the boot module)
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t e = entries + i * 0xc;
        if (lh(e + 2) != 3 || lwu(e + 4) == 0) continue;
        const uint32_t list = lwu(lwu(e + 4) + 0x40);
        if (list == 0 || (lwu(list + 0x30) & 0x800u) == 0) continue;
        sw(e + 4, PAGE);
        return;
    }
}

// orig: (none) — enhancement (language.h)
bool language_pref_read(const char* name, const char* value) {
    if (std::strcmp(name, "voice_language") == 0) {
        const int32_t c = voice_choice_from_name(value);
        if (c >= -1) {
            g_voice = c;
            voice_choice_set(c);
        }
        return true;
    }
    if (std::strcmp(name, "movie_subtitles") == 0) {
        movie_subtitles_set(std::strcmp(value, "on") == 0);
        return true;
    }
    if (std::strcmp(name, "subtitles") == 0) {
        if (std::strcmp(value, "on") == 0) g_subtitles = 1;
        else if (std::strcmp(value, "off") == 0) g_subtitles = 0;
        return true;
    }
    return std::strcmp(name, "voice_step") == 0;  // (the Advanced page's Voice row before this page: its byte, ignored)
}

// orig: (none) — enhancement (language.h)
std::string language_prefs_text() {
    std::string s = std::string("voice_language ") + voice_choice_name(g_voice) + "\n";
    if (g_subtitles >= 0) s += std::string("subtitles ") + (g_subtitles != 0 ? "on" : "off") + "\n";
    s += std::string("movie_subtitles ") + (movie_subtitles_choice() ? "on" : "off") + "\n";
    return s;
}

// orig: (none) — enhancement (language.h)
void language_game_state_read() {
    if (g_advanced_disabled) return;
    advanced_prefs_load();
    if (g_subtitles >= 0) sb(SUBTITLES, static_cast<uint32_t>(g_subtitles));
}

// orig: (none) — enhancement (language.h)
void language_switch_changed(uint32_t byte) {
    if (g_advanced_disabled || byte != SUBTITLES) return;
    g_subtitles = lbu(SUBTITLES) != 0 ? 1 : 0;
    advanced_prefs_save();
}

}  // namespace rac1
