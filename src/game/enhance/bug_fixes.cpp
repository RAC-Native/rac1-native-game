// Bug Fixes (see bug_fixes.h). Not a port: the page is new, made as the Mods page (mods_menu.cpp): a copy of the Camera
// page (0x1b5a60) with its settings widget (0x27ef00 / 0x27f0a8) in the tall box of page 0x1b6678 and a copy of the
// Options help label (0x1b4b68) in the Options help box. Each row's byte is the fix's switch (0 off, 1 on): cross cycles
// it (the widget), left / right too (bug_fixes_settings_dpad).
//
// To add a fix: a number in bug_fixes.h, a row in FIXES (its preferences key, its name and help texts), its test
// bug_fix_on(n) where the game should change, and its section in docs/advanced.md "Bug Fixes".
#include <game/enhance/bug_fixes.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/rilgar_mobys.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t BFX = 0x000f1000u;
constexpr uint32_t PAGE = BFX;                 // the page (a copy of Camera's)
constexpr uint32_t TITLE = BFX + 0x90;         // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = BFX + 0xe0;   // its settings widget (a copy of Camera's)
constexpr uint32_t HELP_WIDGET = BFX + 0x130;  // its help label (a copy of Options')
constexpr uint32_t SWITCHES = BFX + 0x180;     // the fixes' bytes, by row (the settings widget writes them)
constexpr uint32_t ROW_HELP = BFX + 0x1c0;     // the help label's texts, one per row
constexpr uint32_t ROWS = BFX + 0x200;         // the settings widget's rows (0x18 bytes, ended by text 0)
constexpr uint32_t STRINGS = BFX + 0x400;      // the texts in the game's language, made at each lookup (..0xf2000)

// The game's data the page is made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b6678u> TALL_PAGE{};  // its slot 3 box: the Graphics page's settings box
constexpr uint32_t LANGUAGE = 0x0015ed88u;   // the game's language

// The texts (ids TEXT_FIRST + T_*): the page's own, then a name and a help per fix
constexpr int32_t TEXT_FIRST = 0x7b00;
enum : int32_t { T_TITLE, T_OFF, T_ON, T_RACE_MUSIC, H_RACE_MUSIC, T_FAUCET_RIDE, H_FAUCET_RIDE, T_COUNT };
constexpr uint32_t STRING_SIZE = 0xa0;
static_assert(TEXT_FIRST + T_COUNT <= 0x7c00, "the Bug Fixes texts end where the Exit page's begin");
static_assert(STRINGS + T_COUNT * STRING_SIZE <= 0x000f2000u, "the Bug Fixes texts end at the Graphics block");
// per language (0x15ed88: 0 English, 2 French (in English for now, as the other pages), 3 German, 4 Spanish, 5 Italian)
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Bug Fixes", "Off", "On", "Race Music After Quitting",
     "Quitting the hoverboard race from its results brings the planet's music back (the original keeps the race's).",
     "Hydrodisplacer Faucets",
     "Ratchet no longer slips off a Hydrodisplacer faucet when he steps onto it from some sides (the water then stayed)."},
    {"Fehlerkorrekturen", "Aus", "An", "Rennmusik nach Abbruch",
     "Wer das Hoverboard-Rennen bei den Ergebnissen beendet, hoert wieder die Musik des Planeten (im Original laeuft die "
     "Rennmusik weiter).",
     "Hydrobeamer-Haehne",
     "Ratchet rutscht nicht mehr von einem Hahn des Hydrobeamers, wenn er ihn von manchen Seiten betritt (das "
     "Wasser blieb dann)."},
    {"Correcciones", "No", "Si", "Musica tras abandonar la carrera",
     "Abandonar la carrera de aerotabla desde sus resultados devuelve la musica del planeta (el original deja la de la "
     "carrera).",
     "Grifos del Hidrodesplazador",
     "Ratchet ya no se cae de un grifo del Hidrodesplazador al subir desde algunos lados (el agua se quedaba)."},
    {"Correzioni", "No", "Si", "Musica dopo l'abbandono",
     "Abbandonando la gara di hoverboard dai risultati torna la musica del pianeta (l'originale lascia quella della gara).",
     "Rubinetti dell'Idrosostitutore",
     "Ratchet non scivola piu' da un rubinetto dell'Idrosostitutore salendoci da certi lati (l'acqua allora restava)."},
};

// The fixes, one row each (in their numbers' order): its number, its preferences key, its name and help texts
struct Fix {
    int number;
    const char* key;
    int32_t name;
    int32_t help;
};
constexpr Fix FIXES[] = {
    {BUG_FIX_RACE_MUSIC, "bugfix_race_music", T_RACE_MUSIC, H_RACE_MUSIC},
    {BUG_FIX_FAUCET_RIDE, "bugfix_faucet_ride", T_FAUCET_RIDE, H_FAUCET_RIDE},
};
constexpr uint32_t FIX_COUNT = sizeof(FIXES) / sizeof(Fix);
static_assert(FIX_COUNT <= 10 && ROW_HELP + FIX_COUNT * 4 <= ROWS && ROWS + (FIX_COUNT + 1) * 0x18 <= STRINGS,
              "the page holds ten rows at most (the box shows ten)");

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

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}
// orig: (none) — enhancement: the page `page`'s slots holding `from` pointed at `to`
void slot_replace(uint32_t page, uint32_t from, uint32_t to) {
    for (uint32_t i = 0; i < 14; ++i)
        if (lwu(page + 0x44 + i * 4) == from) sw(page + 0x44 + i * 4, to);
}

// orig: (none) — enhancement: RAC1_BUG_FIXES's fixes as bits (bit n: fix n): a comma-separated list of numbers, `all`,
// `none` (or empty: none); -1 unset
int64_t env_fixes() {
    static const int64_t v = [] {
        const char* e = std::getenv("RAC1_BUG_FIXES");
        if (e == nullptr) return int64_t{-1};
        int64_t bits = 0;
        const std::string list = e;
        size_t at = 0;
        while (at <= list.size()) {
            size_t end = list.find(',', at);
            if (end == std::string::npos) end = list.size();
            const std::string item = list.substr(at, end - at);
            at = end + 1;
            if (item.empty() || item == "none") continue;
            if (item == "all") {
                for (const Fix& f : FIXES) bits |= int64_t{1} << f.number;
                continue;
            }
            char* stop = nullptr;
            const long n = std::strtol(item.c_str(), &stop, 10);
            bool known = false;
            for (const Fix& f : FIXES) known = known || f.number == n;
            if (*stop != 0 || !known) {
                std::fprintf(stderr, "RAC1_BUG_FIXES: %s is not one of the fixes (1..%u, all, none)\n", item.c_str(), FIX_COUNT);
                continue;
            }
            bits |= int64_t{1} << n;
        }
        return bits;
    }();
    return v;
}
}  // namespace

// orig: (none) — enhancement (bug_fixes.h)
bool bug_fix_on(int fix) {
    if (g_advanced_disabled) return false;
    const int64_t env = env_fixes();
    if (env >= 0) return ((env >> fix) & 1) != 0;
    advanced_prefs_load();
    for (uint32_t i = 0; i < FIX_COUNT; ++i)
        if (FIXES[i].number == fix) return lbu(SWITCHES + i) != 0;
    return false;
}

// orig: (none) — enhancement (bug_fixes.h)
bool bug_fixes_pref_read(const char* name, const char* value) {
    for (uint32_t i = 0; i < FIX_COUNT; ++i)
        if (std::strcmp(name, FIXES[i].key) == 0) {
            sb(SWITCHES + i, std::strcmp(value, "1") == 0 ? 1u : 0u);
            return true;
        }
    return false;
}

// orig: (none) — enhancement (bug_fixes.h)
std::string bug_fixes_prefs_text() {
    std::string s;
    for (uint32_t i = 0; i < FIX_COUNT; ++i) s += std::string(FIXES[i].key) + " " + std::to_string(lbu(SWITCHES + i) != 0 ? 1 : 0) + "\n";
    return s;
}

// orig: (none) — enhancement (bug_fixes.h)
uint32_t bug_fixes_page_install(uint32_t parent) {
    g_built = false;
    if (g_advanced_disabled) return 0;
    advanced_prefs_load();
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    ee_copy(HELP_WIDGET, OPTIONS_HELP, 0x50);
    sw(TITLE + 0x34, static_cast<uint32_t>(TEXT_FIRST + T_TITLE));
    uint32_t row = ROWS;
    for (uint32_t i = 0; i < FIX_COUNT; ++i, row += 0x18) {
        sw(row, static_cast<uint32_t>(TEXT_FIRST + FIXES[i].name));
        sw(row + 4, SWITCHES + i);
        sw(row + 8, static_cast<uint32_t>(TEXT_FIRST + T_OFF));
        sw(row + 0xc, static_cast<uint32_t>(TEXT_FIRST + T_ON));
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sw(ROW_HELP + i * 4, static_cast<uint32_t>(TEXT_FIRST + FIXES[i].help));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    sw(ROWS_WIDGET + 0x40, 0);
    // the help label: the focused widget's +0x40 (the selection, mirrored) as the index into its texts (flag 0x80); its
    // box the Options page's help box (slot 1), the rows in page 0x1b6678's taller box (as mods_menu.cpp)
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

// orig: (none) — enhancement (bug_fixes.h)
uint32_t bug_fixes_page() { return g_built ? PAGE : 0; }
// orig: (none) — enhancement (bug_fixes.h)
int32_t bug_fixes_title_text() { return TEXT_FIRST + T_TITLE; }

// orig: (none) — enhancement (bug_fixes.h)
uint32_t bug_fixes_text(int32_t id) {
    const int32_t i = id - TEXT_FIRST;
    if (i < 0 || i >= T_COUNT || g_advanced_disabled) return 0;
    const uint32_t at = STRINGS + static_cast<uint32_t>(i) * STRING_SIZE;
    char* p = static_cast<char*>(ee_ram_ptr(at));
    std::strncpy(p, texts()[i], STRING_SIZE - 1);
    p[STRING_SIZE - 1] = 0;
    return at;
}

// orig: (none) — enhancement (bug_fixes.h)
bool bug_fixes_setting_changed(uint32_t byte) { return g_built && byte >= SWITCHES && byte < SWITCHES + FIX_COUNT; }

// orig: (none) — enhancement (bug_fixes.h)
bool bug_fixes_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (!g_built || w != ROWS_WIDGET) return false;
    sw(w + 0x40, lwu(w + 0x38));  // (the help label's index)
    if ((pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b < SWITCHES || b >= SWITCHES + FIX_COUNT) return false;
    sb(b, lbu(b) != 0 ? 0u : 1u);  // (two values: left / right either way the other)
    advanced_setting_changed(b);
    return true;
}

// orig: (none) — enhancement (bug_fixes.h)
void bug_fixes_race_quit() {
    if (lw(0x0015ed84) != 5 || !bug_fix_on(BUG_FIX_RACE_MUSIC)) return;  // (Rilgar's race alone changes the music)
    ril_28f5c8_l5(0, 8, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
    std::printf("[bug-fixes] race quit: the level's music back (track 0 over 8)\n");
}

// orig: (none) — enhancement (bug_fixes.h)
void bug_fixes_faucet_ride() {
    constexpr uint32_t P = 0x0013f350u;  // the hero (g::PLAYER)
    const int32_t st = lw(P + 0x2084);
    if (st < 0x38 || st > 0x3a) return;
    const uint32_t m = lwu(P + 0x2fc);  // the moby he stands on: the faucet while he rides it
    if (m == 0 || lh(m + 0xa6) != 0x155 || !bug_fix_on(BUG_FIX_FAUCET_RIDE)) return;
    // on each axis: on the origin, or close to it on the side away from zero, the hero is put one step short of it on
    // the side toward zero (the ride's pull, rounded toward zero, stops there)
    for (uint32_t k = 0; k < 2; ++k) {
        const float o = lwc1(m + 0x10 + k * 4), p = lwc1(P + 0x80 + k * 4);
        if (o == 0.0f || !(std::fabs(p) >= std::fabs(o)) || std::signbit(p) != std::signbit(o) || std::fabs(p - o) > 0.01f)
            continue;
        swc1(P + 0x80 + k * 4, std::nextafter(o, 0.0f));
    }
}

}  // namespace rac1
