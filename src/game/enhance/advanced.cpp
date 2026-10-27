// The "Advanced" options page (see advanced.h). Not a port: everything here is new, written on
// top of the original's menu data so that the page looks
// and behaves like the game's own pages. It is a copy of the Camera page (0x1b5a60): its title
// label and its "settings" widget (0x27ef00 / 0x27f0a8: rows of a name, the byte it sets and up
// to 4 value texts; cross cycles the byte), with our rows.
//
// To add a setting: a byte in SETTINGS, its texts in TEXTS, a row in SETTINGS_ROWS, and read the
// byte where the game should change.
//
// The block ADV (EE RAM 0xf0000..0xf2000) is in the PS2 kernel's first megabyte: the port's
// kernel is the host's, so nothing of the game or the runtime lives there.
#include <game/enhance/advanced.h>
#include <game/enhance/controls.h>
#include <game/enhance/graphics.h>
#include <game/enhance/update_notice.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <game/lib/libmc.h>

#include <platform/platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

namespace rac1 {

bool g_advanced_disabled = false;

// (enhancement, advanced.cpp) Options > Sound's output row with 5.1 / 7.1. The game's row is Mono / Stereo, its byte
// 0x15ede8 (the game's options, in its save; 0 mono); 5.1 and 7.1 come after Stereo: the host's choice (the audio
// device's channels, kept in the preferences), the game's byte left at Stereo, so nothing the game keeps or reads changes.
// - advanced_audio_channels: the device's channels, 2 / 6 (5.1) / 8 (7.1) (RAC1_AUDIO_CHANNELS over the preferences;
//   2 while every enhancement is off)
// - advanced_sound_output_cycle: cross on the row: the next of Mono, Stereo, 5.1, 7.1 (the game's byte 0 for Mono, 1
//   for the others; the device reopened); false while every enhancement is off (the game's own toggle then)
// - advanced_sound_output_text: the row's text when the game's byte says Stereo: "5.1" / "7.1", else `stereo_text`
int advanced_audio_channels();
bool advanced_sound_output_cycle(uint32_t mono_byte);
int32_t advanced_sound_output_text(int32_t stereo_text);
// the host's device reopened with that many channels (audio_out_set_channels, set by the host's start; none in the oracle)
void (*g_advanced_audio_reopen)(int channels) = nullptr;

namespace {
constexpr uint32_t ADV = 0x000f0000u;
constexpr uint32_t SETTINGS = ADV;             // one byte per setting (the settings widget writes them)
constexpr uint32_t QS_PAUSE = SETTINGS + 0;    // the world paused while the quick select is open
constexpr uint32_t CONTROLS = SETTINGS + 1;    // 1: the modern controls (L2 aims with the strafe, R2 fires)
constexpr uint32_t MOVEMENT = SETTINGS + 2;    // 1: the strafe with R&C 1's movement, 2: R&C 3's on-foot movement
constexpr uint32_t FADES = SETTINGS + 3;       // 1: the original's screen fades shown (0: run in one host frame)
constexpr uint32_t UPDATE_OFF = SETTINGS + 4;  // 1: no update check at start (update_notice.h)
constexpr uint32_t UPDATE_DEV = SETTINGS + 5;  // 1: the update check's development channel (main), 0 the releases
constexpr uint32_t VENDOR_ALL = SETTINGS + 6;  // 1: the Vendor's "All Ammo" entry (vendor_buy_all.h)
constexpr uint32_t ENTRIES = ADV + 0x680;      // the Options list's entries, ours added (0x100 before ten rows overflowed into HELP)
constexpr uint32_t HELP = ADV + 0x180;         // the Options help label's texts, ours added
constexpr uint32_t PICTURES = ADV + 0x600;     // the Options picture's images (sector, count), ours added (0x1c0 before two more rows did not fit)
constexpr uint32_t PAGE = ADV + 0x200;         // our page (a copy of Camera's)
constexpr uint32_t TITLE = ADV + 0x300;        // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = ADV + 0x380;  // its settings widget (a copy of Camera's)
constexpr uint32_t ROWS = ADV + 0x400;         // the widget's rows (0x18 bytes, ended by text 0)
constexpr uint32_t STRINGS = ADV + 0x800;      // our texts, per language

// Our menu pictures: a picture table entry (sector, count) whose sector is PICTURE_SECTOR + n is
// made here instead of read from the disc (sectors that high are past the end of the disc)
constexpr uint32_t PICTURE_SECTOR = 0xadf00000u;
enum : uint32_t { PIC_PLACEHOLDER, PIC_GRAPHICS, PIC_ORIGINAL, PIC_MODERN, PIC_CONTROLS, PIC_EXPERIMENTAL, PIC_UPDATE, PIC_COUNT };  // (graphics.cpp: 2 / 3 / 5; update_notice.cpp: 6)

// The game's data the page is made from (Veldin's addresses, relocated)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4af8u> OPTIONS_LIST{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b4a30u> OPTIONS_PICTURE{};  // its image is the list's selected entry's (+0x30)
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b8888u> MAIN_OPTIONS_PAGE{};  // the main menu's Options (the boot module's)

// Our text ids, after the game's (0x4e00..0x5300 in the level text tables)
constexpr int32_t TEXT_FIRST = 0x7f00;
enum : int32_t { T_ADVANCED, T_HELP, T_QUICK_SELECT, T_NORMAL, T_PAUSE, T_CONTROLS, T_CLASSIC, T_MODERN, T_MOVEMENT, T_ORIGINAL, T_STRAFING, T_RAC3, T_FADES, T_FAST, T_FADES_ORIGINAL, T_SOUND_51, T_SOUND_71, T_BUY_ALL, T_OFF, T_ON, T_COUNT };
// per language (0x15ed88: 0 English, 2 French, 3 German, 4 Spanish, 5 Italian; French in English
// for now, the font's accented letters not looked at yet)
constexpr const char* TEXTS[6][T_COUNT] = {
    {"Advanced", "Extra settings of the PC version", "Quick Select", "Normal", "Pause", "Aiming", "Original", "Modern", "Movement", "Original", "Strafing", "RAC 3", "Fades", "Fast", "Original", "5.1", "7.1", "Buy All Ammo", "Off", "On"},
    {"Advanced", "Extra settings of the PC version", "Quick Select", "Normal", "Pause", "Aiming", "Original", "Modern", "Movement", "Original", "Strafing", "RAC 3", "Fades", "Fast", "Original", "5.1", "7.1", "Buy All Ammo", "Off", "On"},
    {"Advanced", "Extra settings of the PC version", "Quick Select", "Normal", "Pause", "Aiming", "Original", "Modern", "Movement", "Original", "Strafing", "RAC 3", "Fades", "Fast", "Original", "5.1", "7.1", "Buy All Ammo", "Off", "On"},
    {"Erweitert", "Extra-Einstellungen der PC-Version", "Schnellwahl", "Normal", "Pause", "Zielen", "Original", "Modern", "Bewegung", "Original", "Strafing", "RAC 3", "Blenden", "Schnell", "Original", "5.1", "7.1", "Alle Munition", "Aus", "An"},
    {"Avanzado", "Ajustes extra de la version PC", "Seleccion rapida", "Normal", "Pausa", "Apuntado", "Original", "Moderno", "Movimiento", "Original", "Strafing", "RAC 3", "Fundidos", "Rapidos", "Originales", "5.1", "7.1", "Toda la municion", "No", "Si"},
    {"Avanzate", "Impostazioni extra della versione PC", "Scelta rapida", "Normale", "Pausa", "Mira", "Originale", "Moderna", "Movimento", "Originale", "Strafing", "RAC 3", "Dissolvenze", "Rapide", "Originali", "5.1", "7.1", "Tutte le munizioni", "No", "Si"},
};
constexpr uint32_t STRING_SIZE = 0x30;
static_assert(0x800 + 6 * T_COUNT * STRING_SIZE <= 0x2000, "the Advanced page's texts end where the Graphics page's block begins");

// The page's rows: the name in the settings file, the id in the saves' files (never reused),
// the byte, the name's text, the values' texts (the byte's value is the index; value 0 is the
// default: the original's behaviour, or for a console compromise removed ("faithful
// to the game's code, not to the PS2's limits") the port's - Fades: 0 fast, 1 the original's),
// and whether the row is kept with the saves
struct Row {
    const char* key;
    uint16_t id;
    uint32_t byte;
    int32_t name;
    int32_t values[4];
    bool in_saves = true;
};
constexpr Row SETTINGS_ROWS[] = {
    {"quick_select_pause", 1, QS_PAUSE, T_QUICK_SELECT, {T_NORMAL, T_PAUSE, -1, -1}},
    {"controls", 2, CONTROLS, T_CONTROLS, {T_CLASSIC, T_MODERN, -1, -1}},
    {"movement", 3, MOVEMENT, T_MOVEMENT, {T_ORIGINAL, T_STRAFING, T_RAC3, -1}},
    {"fades", 4, FADES, T_FADES, {T_FAST, T_FADES_ORIGINAL, -1, -1}},
    {"vendor_buy_all", 5, VENDOR_ALL, T_BUY_ALL, {T_OFF, T_ON, -1, -1}},
};
constexpr uint32_t ROW_COUNT = sizeof(SETTINGS_ROWS) / sizeof(Row);

bool g_loaded = false;
uint8_t g_saved[ROW_COUNT] = {};

// orig: (none) — enhancement: RAC1_FADES=original|fast: -1 unset, else the Fades value
int fades_env() {
    static const int v = [] {
        const char* e = std::getenv("RAC1_FADES");
        if (e == nullptr) return -1;
        if (std::strcmp(e, "original") == 0) return 1;
        if (std::strcmp(e, "fast") == 0) return 0;
        std::fprintf(stderr, "RAC1_FADES=%s: original or fast\n", e);
        return -1;
    }();
    return v;
}
// The Fades setting while every enhancement is off (headless runs, the oracle): RAC1_FADES, else fast;
// advanced_set_original_fades sets it
int g_fades_disabled = -1;

// orig: (none) — enhancement: a row's number of values
uint32_t row_values(const Row& r) {
    uint32_t n = 0;
    while (n < 4 && r.values[n] >= 0) ++n;
    return n;
}

// --- The preferences (the settings at start, before any save is loaded) ------------------------
// A text file of "name value" lines: $RAC1_ADVANCED_FILE, else $XDG_CONFIG_HOME/rac1-native/
// advanced_options.txt, else ~/.config/rac1-native/advanced_options.txt (Windows: %APPDATA%\rac1-native\; platform::config_dir). (Older versions kept advanced_options.txt in the working directory: read once when the new one
// does not exist yet.)

// orig: (none) — enhancement: the preferences file's path
std::filesystem::path prefs_path() {
    if (const char* f = std::getenv("RAC1_ADVANCED_FILE")) return f;
    if (const std::filesystem::path d = platform::config_dir(); !d.empty()) return d / "advanced_options.txt";
    return "advanced_options.txt";
}

int g_audio_channels = 0;   // Options > Sound's 5.1 / 7.1 (6 / 8), else 2; 0: not read yet (advanced_audio_channels)
std::string g_update_skip;  // the update "Skip Version" chose (update_notice.h): a release's tag, or main's commit

// orig: (none) — enhancement: a preferences file's "name value" lines, each given to `line`; false when none
template <typename F>
bool prefs_lines(const std::filesystem::path& path, F line) {
    FILE* f = platform::open_file(path, "r");
    if (f == nullptr) return false;
    char buf[256], name[64], value[128];
    while (std::fgets(buf, sizeof buf, f) != nullptr)
        if (std::sscanf(buf, "%63s %127s", name, value) == 2) line(name, value);
    std::fclose(f);
    return true;
}

// orig: (none) — enhancement: the settings from a preferences file; false when none
bool prefs_read(const std::filesystem::path& path) {
    return prefs_lines(path, [](const char* name, const char* value) {
        if (std::strcmp(name, "update_skip") == 0) {
            g_update_skip = value;
            return;
        }
        if (std::strcmp(name, "audio_channels") == 0) return;  // (advanced_audio_channels)
        char* end = nullptr;
        const unsigned long v = std::strtoul(value, &end, 10);
        if (end == value || *end != 0) return;
        if (std::strcmp(name, "update_check_off") == 0 && v < 2) {
            sb(UPDATE_OFF, static_cast<uint32_t>(v));
            return;
        }
        if (std::strcmp(name, "update_channel") == 0 && v < 2) {
            sb(UPDATE_DEV, static_cast<uint32_t>(v));
            return;
        }
        for (const Row& r : SETTINGS_ROWS)
            if (std::strcmp(name, r.key) == 0 && v < row_values(r)) sb(r.byte, static_cast<uint32_t>(v));
    });
}

// orig: (none) — enhancement: the settings as they are now, noted as saved
void settings_mark_saved() {
    for (uint32_t i = 0; i < ROW_COUNT; ++i) g_saved[i] = lbu(SETTINGS_ROWS[i].byte);
}

// orig: (none) — enhancement: the preferences read, once
void settings_load() {
    if (g_loaded) return;
    g_loaded = true;
    if (g_advanced_disabled) return;  // (every setting 0: off)
    if (!prefs_read(prefs_path()) && std::getenv("RAC1_ADVANCED_FILE") == nullptr) prefs_read("advanced_options.txt");
    if (lbu(CONTROLS) != 0 && lbu(MOVEMENT) == 0) sb(MOVEMENT, 1);  // (Aiming = Modern needs the strafe)
    if (fades_env() >= 0) sb(FADES, static_cast<uint32_t>(fades_env()));  // (the switch over the preferences, not saved unless changed)
    settings_mark_saved();
}

// orig: (none) — enhancement: the settings to the preferences file
void settings_save() {
    const std::filesystem::path path = prefs_path();
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    std::string text;
    for (const Row& r : SETTINGS_ROWS) text += std::string(r.key) + " " + std::to_string(lbu(r.byte)) + "\n";
    text += "update_check_off " + std::to_string(lbu(UPDATE_OFF)) + "\n";
    text += "update_channel " + std::to_string(lbu(UPDATE_DEV)) + "\n";
    if (!g_update_skip.empty()) text += "update_skip " + g_update_skip + "\n";
    if (g_audio_channels > 2) text += "audio_channels " + std::to_string(g_audio_channels) + "\n";
    // (through a temporary file: a crash or a full disk never leaves the settings half written)
    if (!platform::write_file_replace(path, text)) std::fprintf(stderr, "[advanced] cannot write %s\n", path.string().c_str());
    settings_mark_saved();
}

// --- The settings in the saves -----------------------------------------------------------------
// Next to each save<N>.bin the game writes, a save<N>.pc of ours (the game's files are left as the
// game writes them: the original game, PCSX2 or a PS2 read the card as before and ignore ours):
//   +0 "R1PC", +4 u16 the format's version (1), +6 u16 the item count, then the items: u16 id,
//   u16 size, the data. A reader skips the ids it does not know and keeps its current value for
//   the ids missing (an older save, or a save made before a setting existed): new versions read
//   old files and old versions read new ones. A save without the file keeps the settings as they are.
constexpr uint16_t SAVE_VERSION = 1;

// orig: (none) — enhancement: the save directory's card path ("BASCUS-97199RATCHET": the game's
// own name at 0x13d1d0, the product code put in by 0x24b410) + "/save<n>.pc"
std::string save_file_path(int32_t n) {
    std::string dir;
    for (uint32_t a = 0x0013d1d0; dir.size() < 64; ++a) {
        const char c = static_cast<char>(lbu(a));
        if (c == 0) break;
        dir += c;
    }
    return dir + "/save" + std::to_string(n) + ".pc";
}

// orig: (none) — enhancement: the settings as a save file's bytes
std::vector<uint8_t> save_file_make() {
    std::vector<uint8_t> d = {'R', '1', 'P', 'C'};
    auto u16 = [&d](uint32_t v) {
        d.push_back(static_cast<uint8_t>(v));
        d.push_back(static_cast<uint8_t>(v >> 8));
    };
    u16(SAVE_VERSION);
    u16(0);  // (the count, below)
    uint32_t count = 0;
    for (const Row& r : SETTINGS_ROWS) {
        if (!r.in_saves) continue;
        u16(r.id);
        u16(1);
        d.push_back(lbu(r.byte));
        ++count;
    }
    graphics_save_items(d, count);  // (the Graphics page's picture settings and the mode, ids 32..)
    d[6] = static_cast<uint8_t>(count);
    d[7] = static_cast<uint8_t>(count >> 8);
    return d;
}

// orig: (none) — enhancement: a save file's settings applied; false when it is not one
bool save_file_apply(const std::vector<uint8_t>& d) {
    if (d.size() < 8 || d[0] != 'R' || d[1] != '1' || d[2] != 'P' || d[3] != 'C') return false;
    auto u16 = [&d](size_t at) { return static_cast<uint32_t>(d[at] | (d[at + 1] << 8)); };
    const uint32_t count = u16(6);
    size_t at = 8;
    for (uint32_t i = 0; i < count && at + 4 <= d.size(); ++i) {
        const uint32_t id = u16(at), size = u16(at + 2);
        at += 4;
        if (at + size > d.size()) break;
        for (const Row& r : SETTINGS_ROWS)
            if (r.in_saves && r.id == id && size >= 1 && d[at] < row_values(r)) sb(r.byte, d[at]);
        graphics_save_item(id, d.data() + at, size);
        at += size;
    }
    return true;
}

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_menu_install() {
    if (g_advanced_disabled) return;
    settings_load();

    // the texts (all languages: the lookup picks)
    for (uint32_t lang = 0; lang < 6; ++lang)
        for (uint32_t t = 0; t < T_COUNT; ++t) {
            char* s = static_cast<char*>(ee_ram_ptr(STRINGS + (lang * T_COUNT + t) * STRING_SIZE));
            std::strncpy(s, TEXTS[lang][t], STRING_SIZE - 1);
            s[STRING_SIZE - 1] = 0;
        }

    // the page: Camera's with our title and rows, its parent the Options page
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    sw(TITLE + 0x34, TEXT_FIRST + T_ADVANCED);
    uint32_t row = ROWS;
    for (const Row& r : SETTINGS_ROWS) {
        sw(row, static_cast<uint32_t>(TEXT_FIRST + r.name));
        sw(row + 4, r.byte);
        for (uint32_t k = 0; k < 4; ++k) sw(row + 8 + k * 4, r.values[k] >= 0 ? static_cast<uint32_t>(TEXT_FIRST + r.values[k]) : 0u);
        row += 0x18;
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    // its parent: the Options page it is opened from (the main menu's in the boot module)
    const bool front = ovl_reloc_level() == OVL_FRONT;
    sw(PAGE + 0x38, front ? static_cast<uint32_t>(MAIN_OPTIONS_PAGE) : static_cast<uint32_t>(OPTIONS_PAGE));
    const uint32_t graphics_page = graphics_page_install(front ? static_cast<uint32_t>(MAIN_OPTIONS_PAGE) : static_cast<uint32_t>(OPTIONS_PAGE));
    const uint32_t controls = controls_page_install(front ? static_cast<uint32_t>(MAIN_OPTIONS_PAGE) : static_cast<uint32_t>(OPTIONS_PAGE));
    sw(PAGE + 0x40, ROWS_WIDGET);
    for (uint32_t i = 0; i < 14; ++i) {
        const uint32_t slot = PAGE + 0x44 + i * 4;
        if (lwu(slot) == CAMERA_TITLE) sw(slot, TITLE);
        else if (lwu(slot) == CAMERA_SETTINGS) sw(slot, ROWS_WIDGET);
    }
    sw(PAGE + 0x80, 0);

    // the Options list: our entries (Graphics, Controls, then Advanced) before the last one (Quit
    // Game), once per overlay load
    const uint32_t list = OPTIONS_LIST, help = OPTIONS_HELP;
    if (lwu(list + 0x34) != ENTRIES) {
        const uint32_t src = lwu(list + 0x34), help_src = lwu(help + 0x34);
        const uint32_t picture = OPTIONS_PICTURE, pictures_src = lwu(picture + 0x30);
        int32_t n = 0;
        while (lh(src + static_cast<uint32_t>(n) * 0xc) != 0) ++n;
        uint32_t d = ENTRIES, h = HELP, pic = PICTURES;
        for (int32_t i = 0; i < n; ++i) {
            if (i == n - 1) {
                sh(d, static_cast<uint32_t>(graphics_title_text()));
                sh(d + 2, 3);  // action 3: the page +4
                sw(d + 4, graphics_page);
                sh(d + 8, 0);
                sh(d + 0xa, 0);
                d += 0xc;
                sw(h, static_cast<uint32_t>(graphics_help_text()));
                h += 4;
                sw(pic, PICTURE_SECTOR + PIC_GRAPHICS);
                sw(pic + 4, 1);
                pic += 8;
                sh(d, static_cast<uint32_t>(controls_title_text()));  // (Controls)
                sh(d + 2, 3);
                sw(d + 4, controls);
                sh(d + 8, 0);
                sh(d + 0xa, 0);
                d += 0xc;
                sw(h, static_cast<uint32_t>(controls_help_text()));
                h += 4;
                sw(pic, PICTURE_SECTOR + PIC_CONTROLS);
                sw(pic + 4, 1);
                pic += 8;
                sh(d, TEXT_FIRST + T_ADVANCED);
                sh(d + 2, 3);  // action 3: the page +4
                sw(d + 4, PAGE);
                sh(d + 8, 0);
                sh(d + 0xa, 0);
                d += 0xc;
                sw(h, TEXT_FIRST + T_HELP);
                h += 4;
                sw(pic, PICTURE_SECTOR + PIC_PLACEHOLDER);  // our picture
                sw(pic + 4, 1);
                pic += 8;
            }
            ee_copy(d, src + static_cast<uint32_t>(i) * 0xc, 0xc);
            sw(h, lwu(help_src + static_cast<uint32_t>(i) * 4));
            ee_copy(pic, pictures_src + static_cast<uint32_t>(i) * 8, 8);
            d += 0xc;
            h += 4;
            pic += 8;
        }
        for (uint32_t i = 0; i < 0xc; i += 4) sw(d + i, 0);
        sw(h, 0);
        sw(list + 0x34, ENTRIES);
        sw(help + 0x34, HELP);
        sw(picture + 0x30, PICTURES);  // (without it, the last entry's picture was read past the table)
    }
}

// orig: (none) — enhancement (advanced.h)
uint32_t advanced_text(int32_t id) {
    if (const uint32_t s = graphics_text(id)) return s;
    if (const uint32_t s = controls_text(id)) return s;
    if (const uint32_t s = update_notice_text(id)) return s;
    if (id < TEXT_FIRST || id >= TEXT_FIRST + T_COUNT) return 0;
    uint32_t lang = lwu(0x0015ed88);
    if (lang >= 6) lang = 0;
    return STRINGS + (lang * T_COUNT + static_cast<uint32_t>(id - TEXT_FIRST)) * STRING_SIZE;
}

namespace {
uint32_t g_fresh_pictures[6] = {};  // the menu buffers holding a picture just made (at most 5)

// orig: (none) — enhancement: the placeholder picture: "PC+" in big pixel letters with their
// shadow on a blue gradient, framed; drawn in the panel's part of the image (the Options
// picture shows its top left 0x93 x 0x85 texels)
void picture_placeholder(uint32_t buf, uint32_t which) {
    constexpr int W = 256, H = 256, VW = 0x93, VH = 0x85;
    constexpr uint8_t TEXT = 0xf0, SHADOW = 0xf1, FRAME = 0xf2;
    // the header ("2FIP", +8 / +0xc the size), the palette (+0x20: 256 RGBA words, alpha 0x80 =
    // opaque, in the GS's CSM1 order: bits 3 and 4 of an index swapped), the pixels (+0x420)
    for (uint32_t i = 0; i < 0x20; i += 4) sw(buf + i, 0);
    sw(buf, 0x50494632u);
    sw(buf + 8, W);
    sw(buf + 0xc, H);
    auto colour = [buf](uint32_t i, uint32_t r, uint32_t g, uint32_t b) {
        const uint32_t k = (i & 0xe7u) | ((i & 8u) << 1) | ((i & 0x10u) >> 1);
        sw(buf + 0x20 + k * 4, r | (g << 8) | (b << 16) | (0x80u << 24));
    };
    for (uint32_t i = 0; i < 256; ++i) colour(i, 0x10 + i * 0x28 / 128, 0x18 + i * 0x40 / 128, 0x40 + i * 0x50 / 128);
    colour(TEXT, 0x88, 0xa8, 0xff);
    colour(SHADOW, 0x04, 0x06, 0x10);
    colour(FRAME, 0x50, 0x68, 0xa0);
    uint8_t* px = static_cast<uint8_t*>(ee_ram_ptr(buf + 0x420));
    for (int y = 0; y < H; ++y)
        for (int x = 0; x < W; ++x) px[y * W + x] = static_cast<uint8_t>(y < VH ? (VH - 1 - y) * 127 / VH : 0);
    for (int y = 0; y < VH; ++y)
        for (int x = 0; x < VW; ++x)
            if (x < 3 || y < 3 || x >= VW - 3 || y >= VH - 3) px[y * W + x] = FRAME;
    // the letters (5 x 7, rows from the top, bit 4 the left column), 7 texels a dot: "PC+" (the
    // Advanced page), "GFX" (Graphics), "PS2" / "PC+" (the new game's Original / Modern modes), "NEW" (the update notice)
    static constexpr uint8_t P[7] = {0x1e, 0x11, 0x11, 0x1e, 0x10, 0x10, 0x10}, C[7] = {0x0e, 0x11, 0x10, 0x10, 0x10, 0x11, 0x0e},
                             PLUS[7] = {0x00, 0x04, 0x04, 0x1f, 0x04, 0x04, 0x00}, G[7] = {0x0e, 0x11, 0x10, 0x17, 0x11, 0x11, 0x0f},
                             F[7] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x10}, X[7] = {0x11, 0x11, 0x0a, 0x04, 0x0a, 0x11, 0x11},
                             S_[7] = {0x0f, 0x10, 0x10, 0x0e, 0x01, 0x01, 0x1e}, TWO[7] = {0x0e, 0x11, 0x01, 0x02, 0x04, 0x08, 0x1f},
                             A[7] = {0x0e, 0x11, 0x11, 0x1f, 0x11, 0x11, 0x11}, E[7] = {0x1f, 0x10, 0x10, 0x1e, 0x10, 0x10, 0x1f}, D[7] = {0x1e, 0x11, 0x11, 0x11, 0x11, 0x11, 0x1e},
                             N[7] = {0x11, 0x19, 0x15, 0x13, 0x11, 0x11, 0x11}, W_[7] = {0x11, 0x11, 0x11, 0x15, 0x15, 0x15, 0x0a};
    const uint8_t* const WORDS[PIC_COUNT][3] = {{P, C, PLUS}, {G, F, X}, {P, S_, TWO}, {P, C, PLUS}, {P, A, D}, {E, X, P}, {N, E, W_}};
    const uint8_t* const* GLYPHS = WORDS[which < PIC_COUNT ? which : 0];
    constexpr int S = 7, GW = 5 * S, GAP = S, TW = 3 * GW + 2 * GAP;
    constexpr int X0 = (VW - TW) / 2, Y0 = (VH - 7 * S) / 2;
    for (int pass = 0; pass < 2; ++pass) {  // the shadow, then the letters
        const int off = pass == 0 ? 3 : 0;
        for (int g = 0; g < 3; ++g)
            for (int row = 0; row < 7 * S; ++row)
                for (int col = 0; col < GW; ++col)
                    if ((GLYPHS[g][row / S] >> (4 - col / S)) & 1) {
                        const int x = X0 + g * (GW + GAP) + col + off, y = Y0 + row + off;
                        px[y * W + x] = pass == 0 ? SHADOW : TEXT;
                    }
    }
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
bool advanced_picture(uint32_t sector, uint32_t buf) {
    if (sector < PICTURE_SECTOR || sector >= PICTURE_SECTOR + PIC_COUNT) {
        advanced_picture_take(buf);  // a game picture read there: no longer ours
        return false;
    }
    picture_placeholder(buf, sector - PICTURE_SECTOR);
    for (uint32_t& b : g_fresh_pictures)
        if (b == 0 || b == buf) {
            b = buf;
            break;
        }
    return true;
}

// orig: (none) — enhancement (advanced.h)
bool advanced_picture_take(uint32_t buf) {
    for (uint32_t& b : g_fresh_pictures)
        if (b == buf) {
            b = 0;
            return true;
        }
    return false;
}

// orig: (none) — enhancement (advanced.h)
void advanced_save_written(int32_t slot) {
    settings_load();
    if (mc_card_file_write(save_file_path(slot), save_file_make()))
        std::printf("[advanced] settings saved with save %d (%s)\n", slot, save_file_path(slot).c_str());
}

// orig: (none) — enhancement (advanced.h)
void advanced_save_loaded(int32_t slot) {
    settings_load();
    std::vector<uint8_t> d;
    if (!mc_card_file_read(save_file_path(slot), d)) return;  // an older save: the settings kept
    if (save_file_apply(d)) {
        if (lbu(CONTROLS) != 0 && lbu(MOVEMENT) == 0) sb(MOVEMENT, 1);  // (Aiming = Modern needs the strafe)
        std::printf("[advanced] settings loaded with save %d\n", slot);
    }
}

namespace {
void combo_table_sync();  // the wrench section below
void legs_update();       // the strafe's legs, below
void backpedal_show();
void backpedal_restore();
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_frame() {
    if (g_advanced_disabled) return;
    settings_load();
    combo_table_sync();
    for (uint32_t i = 0; i < ROW_COUNT; ++i)
        if (lbu(SETTINGS_ROWS[i].byte) != g_saved[i]) {
            settings_save();
            break;
        }
}

// orig: (none) — enhancement (advanced.h)
bool advanced_world_paused() {
    if (lbu(QS_PAUSE) == 0) return false;
    if ((lwu(0x0013c940 + 0x1c0) & 0x10) == 0) return false;  // triangle held
    // the quick select (slot 3 of the HUD's elements, its update +0x14: 0x237ba0, or 0x238b80
    // for the one the hero opens in his other mode)
    const uint32_t update = lwu(OV(0x0017dc50) + 3 * 0x90 + 0x14);
    return update != 0 && (update == OVF(0x00237ba0) || update == OVF(0x00238b80));
}

// orig: (none) — enhancement (advanced.h)
bool advanced_vendor_buy_all() {
    if (g_advanced_disabled) return false;
    settings_load();
    return lbu(VENDOR_ALL) != 0;
}

// orig: (none) — enhancement (advanced.h)
bool advanced_original_fades() {
    if (g_advanced_disabled) return g_fades_disabled >= 0 ? g_fades_disabled != 0 : fades_env() == 1;
    settings_load();
    return lbu(FADES) != 0;
}

// orig: (none) — enhancement (advanced.h)
void advanced_set_original_fades(bool on) {
    if (g_advanced_disabled) {
        g_fades_disabled = on ? 1 : 0;
        return;
    }
    settings_load();
    sb(FADES, on ? 1u : 0u);
}

// --- The modern controls ----------------------------------------------------------
// With Controls = Modern, in gameplay (game mode 0): L2 held aims — the game no longer sees L2 (no
// first-person view on it: L1 keeps it), Ratchet turns to the camera's heading (0x166dd8, the
// heading the game's own crouch walk turns to with L2 / R2) and keeps facing it while the left
// stick moves him in any direction at a reduced speed; the right stick, which turns the camera in
// the original, turns the aim with it. R2 fires: it is the circle button to the game (circle
// still works). In the menus nothing changes (circle there confirms Quit Game).

namespace {
constexpr uint32_t PAD = 0x0013c940;
constexpr uint32_t HERO = 0x0013f350;          // the player block (g::PLAYER)
constexpr OvConst<0x00166dd8u> CAMERA_YAW{};  // the camera's heading (0x166dc0 +0x18)
constexpr float AIM_SPEED = 1.0f;              // the move speed while aiming, of the stick's (0.7 felt slow)
constexpr float LEG_TWIST_SIGN = 1.0f;         // the turn's sense in the joints' space (see advanced_pose_hook)
constexpr float AIM_SNAP = 0.6f;               // within this of the aim (radians) Ratchet is put on it
constexpr float AIM_DISTANCE = 80.0f;          // the screen's centre ray, this far from the camera
bool g_aim_button = false;                     // L2 held (taken from the game's view)
bool g_aim_turned = false;                     // the hero's turn spring ran this frame
bool g_strafe_flip = false;                    // the flip 0xb under way came from the strafe
bool g_aim_was = false;                        // aiming (as of the camera's update)
float g_aim_yaw = 0.0f;                        // the aim's heading (the camera is held on it)
bool g_pitch_was = false;                      // the modern aim (its pitch) held last frame

// orig: (none) — enhancement: R&C 3's stick tilt (UYA 0x4ea9a8): the mean of the stick's square and
// round lengths, at most 1 (R&C1 0x211830: the round length) — full sooner on the diagonals
float rac3_tilt() {
    const float x = lwc1(HERO + 0x1d20), y = lwc1(HERO + 0x1d24);
    const float sq = abs_f(x) < abs_f(y) ? abs_f(y) : abs_f(x);
    const float t = (sq + sqrt_f(x * x + y * y)) * 0.5f;
    return t < 1.0f ? t : 1.0f;
}

// orig: (none) — enhancement: the hero's states the aim works in: on the ground, standing or
// moving (0 standing, 2 running, 3 stopping, 0x73 the slower run), and the jump family's
// (jump_update, 0x217970 block 0x21ead0: 7, 9..0x12, 0x1c, 0x3c, 0x49, 0x4c, 0x5e, 0x69)
bool modern_aim() { return lbu(CONTROLS) != 0 && lbu(MOVEMENT) != 0; }  // orig: (none) — Aiming = Modern (needs RAC 3)

// orig: (none) — enhancement: the states the strafe works in
bool aim_state(int32_t st) {
    if (st == 0 || st == 2 || st == 3 || st == 0x73) return true;
    return st == 7 || (st >= 9 && st <= 0x12) || st == 0x1c || st == 0x3c || st == 0x49 || st == 0x4c || st == 0x5e || st == 0x69;
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
uint32_t advanced_pad_remap(uint32_t pad, uint32_t held) {
    if (pad != PAD) return held;
    settings_load();
    g_aim_button = false;
    // only with Movement = RAC 3, in gameplay (circle confirms Quit Game in the menus)
    if (lbu(MOVEMENT) == 0 || lw(OV(0x0015f5c4)) != 0) return held;
    if (modern_aim()) {                     // Aiming = Modern: L2 aims (strafing), R2 fires
        g_aim_button = (held & 0x1u) != 0;
        if ((held & 0x2u) != 0) held |= 0x20u;  // R2: circle
    } else {                                // R&C 3's strafe on L2 or R2 (UYA hero_tick 0x4ea708: pad & 3)
        g_aim_button = (held & 0x3u) != 0;
    }
    return held & ~0x3u;                    // L2 / R2 not seen as such (L1 / R1 keep the view / crouch)
}

// orig: (none) — enhancement (advanced.h)
bool advanced_strafing() {
    const int32_t st = lw(HERO + 0x2084);
    // (with the modern aim also the gloves' standing throw 0x23: the aim held through it, so the
    // gloves' camera aim and its pitch are those of the reticle when the throw leaves the hand)
    return g_aim_button && lbu(MOVEMENT) != 0 && lw(OV(0x0015f5c4)) == 0 && (aim_state(st) || (st == 0x23 && modern_aim()));
}

// orig: (none) — enhancement (advanced.h)
bool advanced_aiming() { return modern_aim() && advanced_strafing(); }

// orig: (none) — enhancement (advanced.h)
float advanced_turn_target(float target, float& k, float& d, float& vmax) {
    if (!advanced_strafing()) return target;
    g_aim_turned = true;
    // a stiffer spring than the crouch walk's turn to the camera (0x217970, block 0x21e628: k
    // 0.017, d 0.3, 4.7 vm), so that Ratchet keeps up with the aim turned by the right stick
    const float t = lwc1(0x0015ed64);
    k = t * 0.06f;
    d = t * 0.45f;
    vmax = lwc1(0x0015ed6c) * 14.0f;
    const float aim = g_aim_was ? g_aim_yaw : lwc1(CAMERA_YAW);
    // close to the aim: held on it (a spring trails a turning aim); else the spring turns him
    if (abs_f(angle_sub(aim, lwc1(HERO + 0x98))) < AIM_SNAP) {
        swc1(HERO + 0x98, aim);
        swc1(HERO + 0x184, 0.0f);
    }
    return aim;
}

// orig: (none) — enhancement (advanced.h)
void advanced_move_speed() {
    if (advanced_rac3_movement()) {
        // R&C 3 (UYA 0x4eadd0): the run's speed proportional to
        // the stick, 6.17 u/s at full tilt (R&C1: the table's two steps, 0.9 / 5.7); state 0x73 as
        // R&C1's (x0.8, at least 2.5); not in state 0x3f
        const int32_t st = lw(HERO + 0x2084);
        if (st != 0x3f && 0.0f < lwc1(HERO + 0x190)) {
            const float fs = lwc1(0x0015ed6c);
            float v = 6.17f * fs * rac3_tilt();
            if (st == 0x73) {
                v = v * 0.8f;
                if (v < fs * 2.5f) v = fs * 2.5f;
            }
            swc1(HERO + 0x190, v);
        }
    }
    if (advanced_strafing()) swc1(HERO + 0x190, lwc1(HERO + 0x190) * AIM_SPEED);
}

// orig: (none) — enhancement (advanced.h)
void advanced_aim_before_hero() {
    g_aim_turned = false;
    // a strafe flip ends with the state 0xb: any later 0xb (R1's crouch flip) is the original's
    if (lw(HERO + 0x2084) != 0xb) g_strafe_flip = false;
    backpedal_restore();
}

namespace {
constexpr OvConst<0x00166e00u> ACTIVE_CAMERA{};  // the active camera object (rows +0..+0x2f, position +0x30)
constexpr float AIM_TURN_RATE = 0.045f;          // radians a frame at full right stick (about 2.7 rad/s)
float g_saved_rx = 0.0f;                         // the right stick's x, kept from the camera while aiming
float g_saved_ry = 0.0f;                         // and its y
float g_aim_pitch = 0.0f;                        // the aim's pitch (the camera is held on it)
constexpr float AIM_PITCH_RATE = 0.03f;          // radians a frame at full right stick
constexpr float AIM_PITCH_MIN = -0.75f, AIM_PITCH_MAX = 0.55f;  // looking down / up
constexpr float AIM_PIVOT_HEIGHT = 1.2f;         // the camera turns in pitch about this point above the hero
constexpr int32_t PITCH_RELEASE_FRAMES = 30;
constexpr float AIM_PITCH_SIGN = -1.0f;          // vec_rotate_axis's sense about the side row (measured)     // after the aim, the pitch eased back to the camera's own
int32_t g_pitch_release = 0;

// orig: (none) — enhancement: the pitch of the camera's forward row (+0) of the camera `cam`
// (atan2_f(x, y) is atan2(y, x), as the game uses it: `yaw = atan2_f(dir.x, dir.y)` in the Blaster)
float camera_pitch(uint32_t cam) { return atan2_f(sqrt_f(lwc1(cam) * lwc1(cam) + lwc1(cam + 4) * lwc1(cam + 4)), lwc1(cam + 8)); }

// orig: (none) — enhancement: the camera `cam` turned rigidly by `a` about its side row (+0x10)
// through the point (px, py, pz): its rows and its position
void camera_pitch_turn(uint32_t cam, float a, float px, float py, float pz) {
    const Vec4 axis = V(cam + 0x10);
    for (uint32_t r = 0; r < 0x30; r += 0x10) vec_rotate_axis(V(cam + r), V(cam + r), axis, a);
    Vec4 rel = Vec4{lwc1(cam + 0x30) - px, lwc1(cam + 0x34) - py, lwc1(cam + 0x38) - pz, 0.0f};
    vec_rotate_axis(rel, rel, axis, a);
    swc1(cam + 0x30, px + rel.x);
    swc1(cam + 0x34, py + rel.y);
    swc1(cam + 0x38, pz + rel.z);
}
constexpr int32_t RELEASE_FRAMES = 60;           // after the aim, the view eased from it to the camera's own heading
int32_t g_release = 0;                           // frames of that left
constexpr float STICK_TURN = 0.2f;               // the right stick's x past this: the player turning the camera

// orig: (none) — enhancement: (x, y) of the vector at `a` turned by `c` / `s` about the up axis
void turn_vec(uint32_t a, float c, float s) {
    const float x = lwc1(a), y = lwc1(a + 4);
    swc1(a, x * c - y * s);
    swc1(a + 4, x * s + y * c);
}
// orig: (none) — enhancement: the point at `a` turned about the up axis through (px, py)
void turn_point(uint32_t a, float px, float py, float c, float s) {
    const float x = lwc1(a) - px, y = lwc1(a + 4) - py;
    swc1(a, px + x * c - y * s);
    swc1(a + 4, py + x * s + y * c);
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_aim_before_camera() {
    // the strafe holds the heading (the right stick's x turns it); the modern aim also the pitch
    const bool aim = advanced_strafing(), pitch_aim = aim && modern_aim();
    if (aim && !g_aim_was) {
        g_aim_yaw = lwc1(CAMERA_YAW);
        g_release = 0;
    }
    if (pitch_aim && !g_pitch_was) {
        const uint32_t cam = lwu(ACTIVE_CAMERA);
        g_aim_pitch = cam != 0 ? camera_pitch(cam) : 0.0f;
        g_pitch_release = 0;
    }
    if (!aim && g_aim_was) g_release = RELEASE_FRAMES;
    if (!pitch_aim && g_pitch_was) g_pitch_release = PITCH_RELEASE_FRAMES;
    g_aim_was = aim;
    g_pitch_was = pitch_aim;
    g_saved_rx = lwc1(PAD + 0x100);
    g_saved_ry = lwc1(PAD + 0x104);
    if (!aim) return;
    // the right stick turns the aim (the camera's own stick turn, 0x2e89b0, sees no x), the same way
    // as the camera: its input a = x, negated when the option 0x15ede0 is 0, turns the heading by +a
    // (measured: RAC1_SIM_INPUT=stick)
    const float a = lw(0x0015ede0) == 0 ? -g_saved_rx : g_saved_rx;
    g_aim_yaw = angle_add(g_aim_yaw, a * AIM_TURN_RATE);
    swc1(PAD + 0x100, 0.0f);
    if (!pitch_aim) return;  // the strafe leaves the pitch to the camera
    // its y the aim's pitch, the same way as the camera (its input b = y, negated unless the option
    // 0x15eddc is 0, pitches the view by -b), held where it is let go (the camera's own pitch springs
    // back to its rest: 0x2e8bb8)
    const float b = lw(0x0015eddc) == 0 ? g_saved_ry : -g_saved_ry;
    float pitch = g_aim_pitch - b * AIM_PITCH_RATE;
    if (pitch < AIM_PITCH_MIN) pitch = AIM_PITCH_MIN;
    if (AIM_PITCH_MAX < pitch) pitch = AIM_PITCH_MAX;
    g_aim_pitch = pitch;
    swc1(PAD + 0x104, 0.0f);
}

namespace {
constexpr float SHOULDER_DIST = 3.0f;    // the aiming camera: this far behind the pivot above the hero,
constexpr float SHOULDER_SIDE = 0.75f;   // this far to the right (on the screen: shoulder_side),
constexpr float SHOULDER_UP = 0.1f;      // this far up
constexpr float SHOULDER_RATE = 0.125f;  // eased in / out over 8 frames
float g_shoulder = 0.0f;

// orig: (none) — enhancement: SHOULDER_SIDE in the world: the mirrored levels (0x15edb4) draw the world's left on the screen's
// right, so the view goes to the world's left to stay over the right shoulder (issue #95)
float shoulder_side() { return lbu(0x0015edb4) != 0 ? -SHOULDER_SIDE : SHOULDER_SIDE; }

// orig: (none) — enhancement: the aiming view over the right shoulder (eased in while aiming, out
// after), and in any case kept out of the walls: when the view turned by the aim (or moved here)
// ends up behind a wall seen from the pivot above the hero, it is put in front of the wall. Only
// the view the renderer reads (0x166dc0) moves: the follow camera's own position is the state its
// springs go on from (writing it there drew the camera in after the aim)
void aim_shoulder(uint32_t cam, bool aiming) {
    const float target = aiming ? 1.0f : 0.0f;
    if (g_shoulder < target) g_shoulder = g_shoulder + SHOULDER_RATE < target ? g_shoulder + SHOULDER_RATE : target;
    else g_shoulder = target < g_shoulder - SHOULDER_RATE ? g_shoulder - SHOULDER_RATE : target;
    const float w = g_shoulder * g_shoulder * (3.0f - 2.0f * g_shoulder);  // smoothstep
    const Vec4 pivot = Vec4{lwc1(0x0013f3d0), lwc1(0x0013f3d4), lwc1(0x0013f3d8) + AIM_PIVOT_HEIGHT, 0.0f};
    Vec4 pos = V(cam + 0x30);
    const float side = shoulder_side();
    if (0.0f < w) {
        const Vec4 f = V(cam);  // the camera's forward row (already on the aim's heading and pitch)
        const float h = g_aim_yaw;
        const Vec4 shoulder = Vec4{pivot.x - f.x * SHOULDER_DIST + sin_vu(h) * side, pivot.y - f.y * SHOULDER_DIST - cos_vu(h) * side,
                      pivot.z - f.z * SHOULDER_DIST + SHOULDER_UP, pos.w};
        pos.x = pos.x + (shoulder.x - pos.x) * w;
        pos.y = pos.y + (shoulder.y - pos.y) * w;
        pos.z = pos.z + (shoulder.z - pos.z) * w;
    }
    // out of the walls: the segment from the shoulder (the pivot moved right as far as the view is
    // over the shoulder) to the camera, with the follow camera's own collision flags (0x94,
    // camera_follow.cpp: what blocks the camera, not every plant and pole); backed against a wall,
    // the view ends beside the head, not in it, and is raised when very close
    const float h = g_aim_yaw;
    const Vec4 from = Vec4{pivot.x + sin_vu(h) * side * w, pivot.y - cos_vu(h) * side * w, pivot.z, 0.0f};
    if (segment_collide_v(from, pos, 0x94, lwu(HERO + 0x2080), nullptr) != 0) {
        const Vec4 hit = V(OV(0x00173e60));
        Vec4 d;
        vec3_sub(d, from, hit);
        const float len = vec3_length(d);
        const float back = len < 0.3f ? len : 0.3f;  // a margin in front of the wall
        if (0.0f < len) {
            pos.x = hit.x + d.x / len * back;
            pos.y = hit.y + d.y / len * back;
            pos.z = hit.z + d.z / len * back;
        }
        Vec4 r;
        vec3_sub(r, pos, from);
        const float near = vec3_length(r);
        if (near < 1.2f) pos.z = pos.z + (1.2f - near) * 0.6f;
    }
    lq_sq(OV(0x00166dc0), cam + 0x30);
    swc1(OV(0x00166dc0), pos.x);
    swc1(OV(0x00166dc4), pos.y);
    swc1(OV(0x00166dc8), pos.z);
}

// orig: (none) — enhancement: while aiming the camera held on the aim's pitch, turned about the point
// above the hero; after the aim eased back to its own pitch over PITCH_RELEASE_FRAMES
void aim_pitch_hold(uint32_t cam, bool aiming) {
    float weight = 1.0f;
    if (!aiming) {
        if (g_pitch_release <= 0) return;
        weight = static_cast<float>(g_pitch_release) / static_cast<float>(PITCH_RELEASE_FRAMES);
        weight = weight * weight * (3.0f - 2.0f * weight);  // smoothstep
        --g_pitch_release;
    }
    const float d = (g_aim_pitch - camera_pitch(cam)) * weight;
    if (abs_f(d) < 1e-5f) return;
    camera_pitch_turn(cam, d * AIM_PITCH_SIGN, lwc1(0x0013f3d0), lwc1(0x0013f3d4), lwc1(0x0013f3d8) + AIM_PIVOT_HEIGHT);
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_aim_after_camera() {
    swc1(PAD + 0x100, g_saved_rx);
    swc1(PAD + 0x104, g_saved_ry);
    // while aiming the camera is held on the aim. When the aim ends the follow camera's own state
    // (its points and offset, which kept following the hero) is turned onto the aim once; its
    // springs then reach the aim's heading in about 30 frames, while the view eases from the aim
    // back to the camera's own heading (a cut back to it was a jolt)
    const bool aiming = g_aim_was && advanced_strafing();  // the heading held (strafe or aim)
    const bool modern = aiming && g_pitch_was;              // the pitch and the shoulder view too
    if (!aiming && g_release <= 0 && g_pitch_release <= 0 && g_shoulder <= 0.0f) return;
    const uint32_t cam = lwu(ACTIVE_CAMERA);
    if (cam == 0 || lh(cam + 0x86) != 0 || lw(OV(0x0016c06c)) != 0) {  // the follow camera only
        g_release = 0;
        g_pitch_release = 0;
        g_shoulder = 0.0f;
        return;
    }
    if (aiming || g_release > 0) {
        const bool first = !aiming && g_release == RELEASE_FRAMES;
        // after the aim the view eased from the aim's heading to the camera's own (holding it fully
        // locked the right stick out of the camera for the whole release: the stick kept the
        // camera from catching up); the right stick turning the camera lets go 4 times as fast
        float weight = 1.0f;
        if (!aiming) {
            weight = static_cast<float>(g_release) / static_cast<float>(RELEASE_FRAMES);
            weight = weight * weight * (3.0f - 2.0f * weight);  // smoothstep
            g_release -= abs_f(g_saved_rx) < STICK_TURN ? 1 : 4;
            if (g_release < 0) g_release = 0;
        }
        float d = angle_sub(g_aim_yaw, lwc1(CAMERA_YAW));
        if (!aiming && abs_f(d) < 0.005f) g_release = 0;  // caught up
        if (!first) d = d * weight;
        if (1e-5f <= abs_f(d)) {
            // the camera turned back onto the aim's heading about the hero: its rows and its
            // position (not the follow camera's own points and offset while aiming: turning those
            // every frame drew it in to the hero, 6.7 to 0.9 in 240 frames of strafing)
            const float c = cos_vu(d), sn = sin_vu(d);
            const float px = lwc1(0x0013f3d0), py = lwc1(0x0013f3d4);
            for (uint32_t r = 0; r < 0x30; r += 0x10) turn_vec(cam + r, c, sn);
            turn_point(cam + 0x30, px, py, c, sn);
            if (first) {
                const uint32_t w = lwu(cam + 0x70);  // the points 0x2e46b8 carries with the hero, the offset +0x130
                if (w != 0) {
                    for (const uint32_t o : {0x0u, 0x40u, 0xa0u, 0xd0u}) turn_point(w + o, px, py, c, sn);
                    turn_vec(w + 0x130, c, sn);
                }
            }
        }
    }
    aim_pitch_hold(cam, modern);
    // the view the renderer reads, as camera_update makes it from the active camera (its position
    // moved over the shoulder / out of the walls)
    aim_shoulder(cam, modern);
    lq_sq(OV(0x00166fd0), cam);
    lq_sq(OV(0x00166fe0), cam + 0x10);
    lq_sq(OV(0x00166ff0), cam + 0x20);
    Mat4 m;
    mat3_copy_affine(m, M(OV(0x00166fd0)));
    mat_to_euler_r(m, V(OV(0x00166dd0)));
    // the Mirrored Levels cheat (0x15edb4): the view's side row the cross product, as camera_update leaves it (copying the
    // camera's rows undid the mirror while aiming: the level drawn unmirrored with the controls still swapped, issue #95)
    if (lbu(0x0015edb4) != 0) vec3_cross_ba(V(OV(0x00166fe0)), V(OV(0x00166ff0)), V(OV(0x00166fd0)));
}

// orig: (none) — enhancement (advanced.h)
void advanced_aim_after_hero() {
    legs_update();
    backpedal_show();
    // the reticle at the screen's centre while aiming (the Blaster, item 0xf, draws its own: its
    // first-person reticle, blaster.cpp), for every item in hand but the wrench (8)
    if (advanced_aiming()) {
        const uint32_t item = lwu(HERO + 0x10b8), m = lwu(HERO + 0x2080);
        if (item != 0xf && item != 8 && m != 0 && lbu(HERO + 0x20ac) == 0) cam_effect_add(m, 0xff0f0fffu, nullptr, 0x26, -1, 1.0f, 0.0f, 90.0f);
    }
    if (!advanced_strafing() || g_aim_turned) return;
    // standing still: no state turned the hero this frame, turn him to the camera here
    float k = 0.0f, d = 0.0f, vmax = 0.0f;
    const float target = advanced_turn_target(0.0f, k, d, vmax);
    swc1(HERO + 0x188, spring_angle(HERO + 0x98, HERO + 0x184, 0, target, k, d, vmax));
}

// orig: (none) — enhancement (advanced.h)
bool advanced_camera_aim(uint32_t pad_held) { return (pad_held & 5u) != 0 || advanced_aiming(); }

namespace {
// orig: (none) — enhancement: the shot from `from` toward the point under the screen's centre (the
// first thing the screen's centre ray meets within AIM_DISTANCE, not the hero; else that far), of
// length `speed`
Vec4 aim_shot(const Vec4& from, float speed) {
    // the camera's forward (the view matrix 0x166c80 transposed, as the blaster's first-person aim)
    Mat4 t;
    mat3_transpose(t, M(OV(0x00166c80)));
    Vec4 f = Vec4{0.0f, 0.0f, 1.0f, 0.0f};
    mat4_transform(f, f, t);
    vec3_set_length(f, f, 1.0f);
    const Vec4 eye = V(OV(0x00166dc0));
    Vec4 aim;
    vec3_scale(aim, f, AIM_DISTANCE);
    vec3_add(aim, aim, eye);
    if (segment_collide_v(eye, aim, 0, lwu(HERO + 0x2080), nullptr) != 0 && lwu(OV(0x00173e40) + 0x18) != lwu(HERO + 0x2080)) {
        const Vec4 hit = V(OV(0x00173e60));
        Vec4 to;
        vec3_sub(to, hit, eye);
        if (2.0f < vec3_length(to)) aim = hit;  // (a hit right at the camera: ignored)
    }
    Vec4 d;
    vec3_sub(d, aim, from);
    vec3_set_length(d, d, speed);
    return d;
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_aim_direction(uint32_t from, uint32_t dir, float speed) { V(dir) = aim_shot(V(from), speed); }

// orig: (none) — enhancement (advanced.h)
void advanced_aim_shot(uint32_t from, uint32_t dir) {
    if (!advanced_aiming()) return;
    V(dir) = aim_shot(V(from), vec3_length(V(dir)));
}

// orig: (none) — enhancement (advanced.h)
void advanced_aim_shot_hand(uint32_t hand, uint32_t dir) {
    if (!advanced_aiming()) return;
    Vec4 from;
    moby_chain_point_v(hand, 0, from);
    V(dir) = aim_shot(from, vec3_length(V(dir)));
}

// orig: (none) — enhancement (advanced.h)
void advanced_aim_frame_hand(uint32_t hand, uint32_t rows) {
    if (!advanced_aiming() || lh(HERO + 0x22c8) == 1) return;  // (leaning on a wall: the hand's own frame, as the original)
    Vec4 from;
    moby_chain_point_v(hand, 0, from);
    const Vec4 f = aim_shot(from, 1.0f);  // forward (row 0), the side (row 1) level, up (row 2) square to both
    Vec4 side{-f.y, f.x, 0.0f, 0.0f};
    if (vec2_length(side) < 1e-4f) return;
    vec3_set_length(side, side, 1.0f);
    Vec4 up{f.y * side.z - f.z * side.y, f.z * side.x - f.x * side.z, f.x * side.y - f.y * side.x, 0.0f};
    for (int i = 0; i < 3; ++i) {
        const Vec4& r = i == 0 ? f : i == 1 ? side : up;
        swc1(rows + i * 0x10, r.x);
        swc1(rows + i * 0x10 + 4, r.y);
        swc1(rows + i * 0x10 + 8, r.z);
    }
}

// orig: (none) — enhancement (advanced.h)
bool advanced_aim_angles(uint32_t from, float& yaw, float& pitch) {
    if (!advanced_aiming()) return false;
    const Vec4 d = aim_shot(V(from), 1.0f);
    yaw = atan2_f(d.x, d.y);
    pitch = -atan2_f(vec2_length(d), d.z);
    return true;
}

// --- R&C 3's on-foot movement -------------------------------------------------------
// With Movement = RAC 3: the values and rules UYA's hero code uses where R&C1's differ, from the
// study (UYA level-1 addresses cited). Applied inside the
// ported functions (run_update, run_input, enter_jump, hero_stick_speed), the original's path
// untouched with Movement = Original.

// orig: (none) — enhancement (advanced.h)
bool advanced_rac3_movement() { return lbu(MOVEMENT) == 2; }

// orig: (none) — enhancement (advanced.h)
void advanced_rac3_along_stick() {
    // UYA: the hero moves along the stick (not the facing) while slower than 1.7 u/s, until the
    // turn left is under 5° (R&C1: under 0.3 u/s with the stick past 0.85, until 3°)
    const float fs = lwc1(0x0015ed6c);
    if (lwc1(HERO + 0x164) < fs * 1.7f) sh(HERO + 0x3b8, 1);
    if (abs_f(lwc1(HERO + 0x188)) < 0.0873f || lbu(HERO + 0x20a8) != 0) sh(HERO + 0x3b8, 0);
}

// orig: (none) — enhancement (advanced.h)
float advanced_rac3_run_turn() {
    // UYA (state 2 block of 0x4f2090): the turn springs per gait (per frame; R&C1 running
    // 0.008 / 0.15 / 0.1658), no acceleration while turning more than 45° with the stick under
    // 0.9, the target cut at a low stick
    const float t = lwc1(0x0015ed64), fs60 = lwc1(0x0015ed6c) * 60.0f;
    const float stick = lwc1(HERO + 0x229c), turn = abs_f(lwc1(HERO + 0x188));
    const bool quick = lh(HERO + 0x3be) != 0;
    if (lw(HERO + 0x2088) == 1 && !quick) {
        sub_00211e30(0, t * 0.037f, t * 0.13f, fs60 * 0.2036f);
    } else if (quick) {
        sub_00211e30(0, t * 0.035f, t * 0.27f, fs60 * 0.1949f);
    } else {
        float k = 0.02f * stick, vmax = 0.064f * stick;
        k = k < 0.01f ? 0.01f : (0.022f < k ? 0.022f : k);
        vmax = vmax < 0.032f ? 0.032f : (0.0931f < vmax ? 0.0931f : vmax);
        sub_00211e30(0, t * k, t * 0.27f, fs60 * vmax);
    }
    if (stick < 0.75f && 0.611f < turn) {
        float c = 1.0f - 0.4f * turn;
        if (c < 0.0f) c = 0.0f;
        swc1(HERO + 0x190, lwc1(HERO + 0x190) * c);
    }
    return stick < 0.9f && 0.785f < turn ? 0.0f : 1.0f;
}

// orig: (none) — enhancement (advanced.h)
void advanced_rac3_stop() {
    // UYA: the stick let go while running: straight to standing, the speed and the drift vector
    // capped to 1.5 u/s (R&C1: 25 frames more running, or the skid state 3 at speed)
    const float cap = lwc1(0x0015ed6c) * 1.5f;
    if (cap < lwc1(HERO + 0x194)) swc1(HERO + 0x194, cap);
    // the drift vector +0x150 and the velocity +0xe0 (0x13f430, horizontal): without the latter the
    // hero still slid 26 frames
    for (const uint32_t v : {0x150u, 0xe0u, 0x100u, 0x110u, 0x130u}) {
        const float dx = lwc1(HERO + v), dy = lwc1(HERO + v + 4);
        const float len = sqrt_f(dx * dx + dy * dy);
        if (cap < len) {
            swc1(HERO + v, dx * cap / len);
            swc1(HERO + v + 4, dy * cap / len);
        }
    }
}

// orig: (none) — enhancement (advanced.h)
void advanced_rac3_jump() {
    if (!advanced_rac3_movement()) return;
    // UYA (0x4ed860 and the jump 7 / 9 entry): take-off 1 frame after the press (R&C1 5), height
    // 1.42 rising to 2.70 while held over 17 frames (R&C1 1.47 / 2.62 / 15; jump 9 1.0 / 2.7), air
    // speed 6.17 (R&C1 5.7)
    const int32_t st = lw(HERO + 0x2084);
    if (st == 0xe) {  // the double jump: air speed 4.75 (R&C1 3.5; UYA)
        swc1(HERO + 0x424, lwc1(0x0015ed6c) * 4.75f);
        return;
    }
    if (st == 0xb) {  // the flips (R1 + flick + cross): take-off after 1 frame (R&C1 5; UYA)
        sw(HERO + 0x420, static_cast<uint32_t>(frame_scale_i(1)));
        return;
    }
    if (st != 7 && st != 9) return;
    swc1(HERO + 0x430, 1.42f);
    swc1(HERO + 0x488, 1.42f);
    swc1(HERO + 0x48c, 2.70f);
    sw(HERO + 0x420, static_cast<uint32_t>(frame_scale_i(1)));
    sh(HERO + 0x498, static_cast<uint32_t>(frame_scale_i(17)));
    swc1(HERO + 0x424, lwc1(0x0015ed6c) * 6.17f);
}

// orig: (none) — enhancement (advanced.h)
bool advanced_rac3_running_jump() {
    // UYA: the running jump 9 when the stick is past 0.85 and not strafing (R&C1: running gait,
    // facing within 60° of the stick, faster than 0.85 × 5.7)
    return 0.85f < rac3_tilt() && !advanced_strafing();
}

// --- the wrench ---

namespace {
// R&C 3's combo table values where R&C1's differ (UYA 0x2f6310 vs R&C1 0x17bc28, rows of 0x2c
// bytes, animation times): row 0 next swing 26 → 23, end 31 → 26; row 1 next 12 → 11, end 23 →
// 15; row 2 restart 23 → 19; row 3 (the throw) jump-cancel 87 → 35
struct ComboPatch {
    uint32_t offset;
    int32_t value;
};
constexpr ComboPatch COMBO_RAC3[] = {
    {0 * 0x2c + 0x10, 23}, {0 * 0x2c + 0x18, 26}, {1 * 0x2c + 0x10, 11}, {1 * 0x2c + 0x18, 15},
    {2 * 0x2c + 0x10, 19}, {3 * 0x2c + 0x14, 35},
};
constexpr uint32_t COMBO_COUNT = sizeof(COMBO_RAC3) / sizeof(ComboPatch);
uint32_t g_combo_table = 0;          // the table patched (the overlay's), 0 none
int32_t g_combo_orig[COMBO_COUNT];   // its values before

// orig: (none) — enhancement: the combo table of the current overlay holds R&C 3's values while
// Movement = RAC 3 (the originals put back otherwise; an overlay loaded again brings its own)
void combo_table_sync() {
    constexpr OvConst<0x0017bc28u> COMBO{};
    const uint32_t table = COMBO;
    if (g_combo_table != 0) {  // still ours? (a level load writes the overlay's data again)
        bool ours = g_combo_table == table;
        for (uint32_t i = 0; ours && i < COMBO_COUNT; ++i) ours = lw(table + COMBO_RAC3[i].offset) == COMBO_RAC3[i].value;
        if (!ours) g_combo_table = 0;
    }
    const bool want = advanced_rac3_movement();
    if (want && g_combo_table == 0) {
        for (uint32_t i = 0; i < COMBO_COUNT; ++i) {
            g_combo_orig[i] = lw(table + COMBO_RAC3[i].offset);
            sw(table + COMBO_RAC3[i].offset, static_cast<uint32_t>(COMBO_RAC3[i].value));
        }
        g_combo_table = table;
    } else if (!want && g_combo_table != 0) {
        for (uint32_t i = 0; i < COMBO_COUNT; ++i) sw(g_combo_table + COMBO_RAC3[i].offset, static_cast<uint32_t>(g_combo_orig[i]));
        g_combo_table = 0;
    }
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
bool advanced_rac3_swing_exit(uint32_t step) {
    if (!advanced_rac3_movement() || lw(HERO + 0xa9c) != 0) return false;
    const int32_t st = lw(HERO + 0x2084);
    if (st != 0x13 && st != 0x15) return false;
    // UYA 0x503b68 (cases 0x13 / 0x15): late in the animation (time > 34; for the throw
    // two frames after the wrench leaves) with the stick past 0.9, or past the row's end with it
    // past 0.8, run instead of standing; the wrench comes back to the hand by itself
    const float t = lwc1(HERO + 0xaa8), tilt = lwc1(HERO + 0x229c);
    const bool late = 34.0f < t && 0.9f < tilt;
    const bool ended = static_cast<float>(lw(step + 0x18)) < t && 0.8f < tilt;
    if (!late && !ended) return false;
    hero_set_state(2, 0);
    return true;
}

// orig: (none) — enhancement (advanced.h)
int32_t advanced_rac3_swing_jump_back(int32_t back) {
    // UYA 0x503b68 (cases 0x13 / 0x15): the look-back (t - row[+0xc]) * 2, but a fixed 8 in the
    // throw. R&C1's row 3 has +0xc = 83, so with its jump-cancel lowered to 35 (combo_table_sync)
    // the computed look-back stays negative until time 83 and no jump press would count. (UYA's 8 is
    // a plain count of its 60 Hz frames: here frame-scaled, as R&C1's own look-backs - 7 at 50 Hz)
    if (!advanced_rac3_movement() || lw(HERO + 0x2084) != 0x15) return back;
    return frame_scale_i(8);
}

// orig: (none) — enhancement (advanced.h)
bool advanced_rac3_wrench_out() {
    // R&C1 needs no such guard: its hero stays in the throw (0x15) until the wrench is back (row 3
    // ends at 91, reached after the catch). Leaving it early (advanced_rac3_swing_exit, the jump
    // cancel at 35) needs UYA's: its item input 0x501678 returns while the thrown flag P+0x121a is
    // set, its crouch (0x503b68 case 4) throws only with it clear, and its throw start 0x4f1a08
    // does nothing while it is set. R&C1's flag: +0x10aa
    // (set by hero_hold_moby_start, cleared by the wrench moby when it is back in the hand).
    return advanced_rac3_movement() && lbu(HERO + 0x10aa) != 0;
}

// orig: (none) — enhancement (advanced.h)
bool advanced_strafe_flip() {
    // UYA: while strafing, cross with the stick past 0.9 and not forward (quadrant 2 of
    // hero_stick_quadrant 0x221df8: 3 back, 1 right, 0 left) is the flip 0xb, its animation by the
    // quadrant (0x1c left, 0x1d right, 0x1f back). With the modern aim, whatever the movement.
    return advanced_strafing() && 0.9f < lwc1(HERO + 0x229c) && hero_stick_quadrant() != 2;
}


// orig: (none) — enhancement (advanced.h)
void advanced_strafe_flip_entered() {
    if (lw(HERO + 0x2084) != 0xb) return;
    // UYA's strafe flip (its jump entry 0x4fc518 and the
    // velocity 0x4ede80): along the stick's heading (+0x438, set by the entry) only, the run's
    // speed not kept sideways (+0x454 = 0, +0x43c on the same heading), starting at 5 u/s (R&C1 0)
    // and taking off 1 frame after the press (R&C1 5); hero_horizontal_velocity then approaches
    // advanced_strafe_flip_speed at 47 / 3600 (R&C1 4.2 at 8 / 3600). About 5.2 units of ground
    // (R&C1's flip ~3.2)
    swc1(HERO + 0x43c, lwc1(HERO + 0x438));
    sw(HERO + 0x454, 0);
    if (!advanced_rac3_movement()) return;
    swc1(HERO + 0x458, lwc1(0x0015ed6c) * 5.0f);
    sw(HERO + 0x420, static_cast<uint32_t>(frame_scale_i(1)));
    g_strafe_flip = true;
}

// orig: (none) — enhancement (advanced.h)
bool advanced_strafe_flip_active() {
    if (g_strafe_flip && (lw(HERO + 0x2084) != 0xb || !advanced_rac3_movement())) g_strafe_flip = false;
    return g_strafe_flip;
}

// orig: (none) — enhancement (advanced.h)
float advanced_strafe_flip_speed() {
    // UYA: max(5.5 x the stick's tilt, 2) u/s
    float v = 5.5f * rac3_tilt();
    if (v < 2.0f) v = 2.0f;
    return v * lwc1(0x0015ed6c);
}

// --- the strafe's legs -----------------------------------------------------------
// R&C1 has no strafe clips (its flips 0x1c..0x1f were labelled so). While aiming and moving, the
// run's animation is reused: sideways, the pelvis, legs and tail are turned towards the movement
// (their joint matrices turned about the moby's vertical axis after the pose: the chest, arms and
// head stay on the aim); backwards, the run's frames are shown in reverse and the legs turned onto
// the backward line.

namespace {
constexpr float PI_F = 3.14159265f;
float g_leg_twist = 0.0f;  // radians the lower body is turned (eased)
bool g_backpedal = false;
struct ForwardFrame {      // the run's real frames, kept while the reverse ones are shown
    bool saved = false;
    uint32_t moby = 0;
    uint8_t f0 = 0, f1 = 0;
    float pos = 0.0f;
    uint8_t shown0 = 0, shown1 = 0, clip = 0;  // the reverse frames written, and the clip they belong to
} g_forward;

// orig: (none) — enhancement: the lower body's turn and the backpedal for this frame
void legs_update() {
    float target = 0.0f;
    bool back = false;
    const int32_t st = lw(HERO + 0x2084);
    if (advanced_strafing() && (st == 2 || st == 0x73) && lwc1(0x0015ed6c) * 0.5f < lwc1(HERO + 0x164)) {
        const float d = angle_sub(lwc1(HERO + 0x180), lwc1(HERO + 0x98));  // the movement from the facing
        const float ad = abs_f(d);
        if (2.0f < ad) {
            back = true;
            target = angle_sub(d, d < 0.0f ? -PI_F : PI_F);
        } else if (0.3f < ad) {
            target = d;
        }
    }
    const float step = 0.15f;
    if (g_leg_twist < target) g_leg_twist = g_leg_twist + step < target ? g_leg_twist + step : target;
    else g_leg_twist = target < g_leg_twist - step ? g_leg_twist - step : target;
    g_backpedal = back;
}

// orig: (none) — enhancement: the run's frames shown backwards (after the hero's update; the real
// ones put back before the next, so the game's own stepping 0x232670 goes on forward)
void backpedal_show() {
    const uint32_t m = lwu(HERO + 0x2080);
    if (!g_backpedal || m == 0 || lbu(m + 0x52) != lbu(m + 0x53)) return;
    const uint32_t n = lbu(m + 0x7e);  // the clip's frame count
    if (n < 2) return;
    g_forward = {true, m, static_cast<uint8_t>(lbu(m + 0x50)), static_cast<uint8_t>(lbu(m + 0x51)), lwc1(m + 0x54), 0, 0, static_cast<uint8_t>(lbu(m + 0x53))};
    g_forward.shown0 = static_cast<uint8_t>((n - 1 - g_forward.f0 % n) & 0xff);
    g_forward.shown1 = static_cast<uint8_t>((n - 1 - g_forward.f1 % n) & 0xff);
    sb(m + 0x50, g_forward.shown0);
    sb(m + 0x51, g_forward.shown1);
}

// orig: (none) — enhancement: the run's real frames back before the hero's update. Only while the
// moby still shows the reverse frames of the same clip: a scene that started in between (game
// mode 2: no hero update ran, the scene poses him) has changed them, and the frames kept from
// before it are stale (putting them back would change the game's state)
void backpedal_restore() {
    if (!g_forward.saved) return;
    g_forward.saved = false;
    if (lwu(HERO + 0x2080) != g_forward.moby) return;
    const uint32_t m = g_forward.moby;
    if (lbu(m + 0x53) != g_forward.clip || lbu(m + 0x50) != g_forward.shown0 || lbu(m + 0x51) != g_forward.shown1) return;
    sb(g_forward.moby + 0x50, g_forward.f0);
    sb(g_forward.moby + 0x51, g_forward.f1);
    swc1(g_forward.moby + 0x54, g_forward.pos);
}
}  // namespace

// orig: (none) — enhancement (advanced.h)
void advanced_pose_hook(uint32_t rec) {
    // the turn is eased by legs_update, after the hero's update: only in gameplay (game mode 0). In
    // a scene, a menu or the ship the hero is posed by them and the turn is dropped (issue #40: a
    // scene started while strafing kept Ratchet's lower body turned, frozen, for the whole scene)
    if (lw(OV(0x0015f5c4)) != 0) {
        g_leg_twist = 0.0f;
        g_backpedal = false;
        return;
    }
    if (abs_f(g_leg_twist) < 0.01f || lhu(rec + 0x10) != 111) return;  // Ratchet: 111 joints
    // the joint matrices on the scratchpad (0x40 each): the pelvis 1, the legs 92..103 and the tail
    // 104..110 turned about the moby's vertical axis
    const float c = cos_vu(g_leg_twist * LEG_TWIST_SIGN), sn = sin_vu(g_leg_twist * LEG_TWIST_SIGN);
    auto turn = [c, sn](uint32_t j) {
        const uint32_t mtx = 0x70000000u + j * 0x40;
        for (uint32_t r = 0; r < 0x40; r += 0x10) {
            const float x = lwc1(mtx + r), y = lwc1(mtx + r + 4);
            swc1(mtx + r, x * c - y * sn);
            swc1(mtx + r + 4, x * sn + y * c);
        }
    };
    turn(1);
    for (uint32_t j = 92; j <= 110; ++j) turn(j);
}

bool update_menu_setting_changed(uint32_t byte);
bool update_menu_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed);

// orig: (none) — enhancement (advanced.h)
void advanced_setting_changed(uint32_t byte) {
    if (graphics_setting_changed(byte)) return;  // (a Graphics row)
    if (controls_setting_changed(byte)) return;  // (a Controls row)
    if (update_menu_setting_changed(byte)) {     // (an Updates row)
        if (!g_advanced_disabled && g_loaded) settings_save();
        return;
    }
    // Aiming = Modern needs a strafe (Strafing or RAC 3): turning it on when Movement is Original
    // turns Strafing on; turning Movement back to Original turns Aiming off
    if (byte == CONTROLS && lbu(CONTROLS) != 0 && lbu(MOVEMENT) == 0) sb(MOVEMENT, 1);
    if (byte == MOVEMENT && lbu(MOVEMENT) == 0) sb(CONTROLS, 0);
    // saved now: a row changed at the title's main menu would otherwise wait for the next gameplay frame
    // (advanced_frame), and be lost by quitting from the menu (the preferences are loaded by then: the page's
    // install reads them)
    if (!g_advanced_disabled && g_loaded) settings_save();
}

// orig: (none) — enhancement (advanced.h)
AdvancedUpdatePrefs advanced_update_prefs() {
    AdvancedUpdatePrefs p;
    if (g_advanced_disabled) {
        p.check = false;
        return p;
    }
    if (g_loaded) {
        p.check = lbu(UPDATE_OFF) == 0;
        p.development = lbu(UPDATE_DEV) != 0;
        p.skipped = g_update_skip;
        return p;
    }
    // (at start, before the game's memory holds the settings: the file read for these alone - once: the update check asks
    // every host frame, and nothing writes the file before the settings are loaded)
    static bool read = false;
    static AdvancedUpdatePrefs from_file;
    if (!read) {
        read = true;
        std::string skip;
        prefs_lines(prefs_path(), [&skip](const char* name, const char* value) {
            if (std::strcmp(name, "update_check_off") == 0) from_file.check = std::strcmp(value, "0") == 0;
            else if (std::strcmp(name, "update_channel") == 0) from_file.development = std::strcmp(value, "1") == 0;
            else if (std::strcmp(name, "update_skip") == 0) skip = value;
        });
        from_file.skipped = skip;
    }
    return from_file;
}

// orig: (none) — enhancement (advanced.h)
void advanced_update_skip(const std::string& id) {
    if (g_advanced_disabled) return;
    settings_load();
    g_update_skip = id;
    settings_save();
}

// --- Options > Sound: 5.1 / 7.1 ---
// The game's own row is Mono / Stereo, the byte 0x15ede8 (the game's options, in its save; 0 mono). Ours adds 5.1 and
// 7.1 after Stereo: the host's choice (the audio device's channels, kept in the preferences as audio_channels 6 / 8),
// the game's byte left at Stereo, so that nothing the game keeps or reads changes.

// orig: (none) — enhancement (advanced.h)
int advanced_audio_channels() {
    if (const char* e = std::getenv("RAC1_AUDIO_CHANNELS")) {  // (testing, and headless dumps)
        const int v = std::strcmp(e, "5.1") == 0 ? 6 : std::strcmp(e, "7.1") == 0 ? 8 : std::atoi(e);
        return v == 6 || v == 8 ? v : 2;
    }
    if (g_advanced_disabled) return 2;
    if (g_audio_channels == 0) {
        g_audio_channels = 2;
        prefs_lines(prefs_path(), [](const char* name, const char* value) {
            if (std::strcmp(name, "audio_channels") == 0) {
                const int v = std::atoi(value);
                g_audio_channels = v == 6 || v == 8 ? v : 2;
            }
        });
    }
    return g_audio_channels;
}

// orig: (none) — enhancement (advanced.h)
bool advanced_sound_output_cycle(uint32_t mono_byte) {
    if (g_advanced_disabled) return false;
    settings_load();
    const int ch = advanced_audio_channels();
    const int now = lw(mono_byte) == 0 ? 0 : ch == 6 ? 2 : ch == 8 ? 3 : 1;  // Mono, Stereo, 5.1, 7.1
    const int next = (now + 1) % 4;
    sw(mono_byte, next == 0 ? 0u : 1u);
    constexpr int kChannels[4] = {2, 2, 6, 8};
    g_audio_channels = kChannels[next];
    if (g_loaded) settings_save();
    if (g_advanced_audio_reopen != nullptr) g_advanced_audio_reopen(advanced_audio_channels());  // (RAC1_AUDIO_CHANNELS over the menu)
    return true;
}

// orig: (none) — enhancement (advanced.h)
int32_t advanced_sound_output_text(int32_t stereo_text) {
    if (g_advanced_disabled) return stereo_text;
    const int ch = advanced_audio_channels();
    return ch == 6 ? TEXT_FIRST + T_SOUND_51 : ch == 8 ? TEXT_FIRST + T_SOUND_71 : stereo_text;
}

// orig: (none) — enhancement (advanced.h)
uint32_t advanced_page() { return PAGE; }
// orig: (none) — enhancement (advanced.h)
int32_t advanced_title_text() { return TEXT_FIRST + T_ADVANCED; }

// orig: (none) — enhancement (graphics.h): Original: every Advanced setting off; Modern: on. Fades is
// not an enhancement but a console compromise removed (its 0 is the port's fast fade): left as it is
void advanced_apply_mode(uint32_t mode) {
    if (g_advanced_disabled) return;
    settings_load();
    const uint32_t v = mode == GAME_MODE_MODERN || mode == GAME_MODE_EXPERIMENTAL ? 1u : 0u;
    sb(QS_PAUSE, v);
    sb(CONTROLS, v);
    sb(MOVEMENT, v != 0 ? 2u : 0u);
    settings_save();
}

// orig: (none) — enhancement (advanced.h)
bool advanced_settings_dpad(uint32_t w, uint32_t e) {
    const uint32_t pressed = lwu(PAD + 0x1c4);
    if (controls_settings_dpad(w, e, pressed)) return true;
    if (graphics_settings_dpad(w, e, pressed)) return true;
    if (update_menu_settings_dpad(w, e, pressed)) return true;
    if (w != ROWS_WIDGET || (pressed & 0xa000u) == 0) return false;
    // our Advanced rows: left / right step the value back / forward (the widget's cross steps it forward)
    const uint32_t b = lwu(e + 4);
    for (const Row& r : SETTINGS_ROWS)
        if (r.byte == b) {
            const uint32_t n = row_values(r);
            sb(b, (lbu(b) + ((pressed & 0x8000u) != 0 ? n - 1 : 1)) % n);
            advanced_setting_changed(b);
            return true;
        }
    return false;
}

}  // namespace rac1
