// The update notice and the main menu's "Updates" ("Aggiornamenti") page (see update_notice.h).
// Not a port: everything here is new, written on top of the original's menu data so that the panels look
// and behave like the game's own pages.
//
// 1. The main menu's "Updates" entry (slot 5 of the root page 0x1b86c8 / 0x1d45c8, anim 0x13, right below
//    "Options") opens the Updates page (a copy of Camera's page 0x1b5a60 with Options' help box in slot 1):
//    - Update Check (On / Off)
//    - Update Channel (Stable / Development)
//    - Check Now (button: checks GitHub on the fly and either opens the Update Available notice or shows
//      "Up to date" / "Offline")
//    Opening the main menu on the title screen (after pressing Start) triggers the automatic background
//    update check once per run when Update Check is On.
//
// 2. The Update Available notice is a copy of the pause menu's Options page (0x1b4950: its title, list,
//    help label and picture): our title, a list of three entries (Update Now, Not Now, Skip Version), the
//    help label showing the selected entry's text, and our picture ("NEW", advanced.cpp).
//
// EE RAM blocks UPD (0xf8000..0xf9000) and UM (0xf9000..0xfa000), in the PS2 kernel's first megabyte.
#include <game/enhance/update_notice.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdio>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t UPD = 0x000f8000u;
constexpr uint32_t PAGE = UPD;              // the notice page (a copy of Options')
constexpr uint32_t TITLE = UPD + 0x90;      // its title (a copy of Options' title)
constexpr uint32_t LIST = UPD + 0xe0;       // its list (a copy of Options' list)
constexpr uint32_t LABEL = UPD + 0x130;     // its help label (a copy of Options')
constexpr uint32_t PICTURE = UPD + 0x180;   // its picture (a copy of Options')
constexpr uint32_t ENTRIES = UPD + 0x200;   // the list's entries (0xc each, ended by text 0)
constexpr uint32_t HELP = UPD + 0x240;      // the help label's texts, one per entry
constexpr uint32_t PICTURES = UPD + 0x260;  // the picture's images (sector, count), one per entry
constexpr uint32_t STRINGS = UPD + 0x300;   // the texts, made at each lookup
constexpr uint32_t STRING_SIZE = 0x1c0;

// The main menu's Updates ("Aggiornamenti") page and its root-page button (0xf9000..0xfa000)
constexpr uint32_t UM = 0x000f9000u;
constexpr uint32_t U_PAGE = UM;                  // the Updates page (a copy of Camera's)
constexpr uint32_t U_TITLE = UM + 0x90;          // its title (a copy of Camera's label)
constexpr uint32_t U_ROWS_WIDGET = UM + 0xe0;    // its settings widget (a copy of Camera's)
constexpr uint32_t U_HELP_WIDGET = UM + 0x130;   // its help label (a copy of Options')
constexpr uint32_t U_ROOT_WIDGET = UM + 0x180;   // the root page's Updates button (slot 5)
constexpr uint32_t U_ROOT_ENTRY = UM + 0x1d0;    // its 1-entry list
constexpr uint32_t U_STEP_CHECK = UM + 0x1f0;    // the Check Now row's step byte
constexpr uint32_t U_ROWS = UM + 0x200;          // the settings widget's rows (0x18 each, ended by text 0)
constexpr uint32_t U_ROW_HELP = UM + 0x260;      // the help label's texts, one per row
constexpr uint32_t U_STRINGS = UM + 0x300;       // the Updates page's texts
constexpr uint32_t U_STRING_SIZE = 0x100;

// The Update Check / Channel bytes in the Advanced block (advanced.cpp saves them in advanced_options.txt)
constexpr uint32_t UPDATE_OFF = 0x000f0004u;
constexpr uint32_t UPDATE_DEV = 0x000f0005u;

// The game's data the pages are made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b6678u> TALL_PAGE{};
constexpr OvConst<0x001b86c8u> ROOT_PAGE{};
constexpr OvConst<0x001b8798u> ROOT_NEW_GAME{};  // the main menu's New Game button (a one-entry list): an entry to copy

// Our menu picture (advanced.cpp makes it: a sector past the end of the disc)
constexpr uint32_t PICTURE_SECTOR = 0xadf00000u;
constexpr uint32_t PIC_UPDATE = 6;  // (advanced.cpp's PIC_UPDATE: "NEW")

// The notice's texts. Ids from TEXT_FIRST (before the Advanced page's 0x7f00..; the game's are 0x4e00..0x5300)
constexpr int32_t TEXT_FIRST = 0x7e00;
enum : int32_t { I_TITLE, I_UPDATE, I_OK, I_SKIP, I_HELP_UPDATE, I_HELP_OK, I_HELP_SKIP, I_COUNT };  // the ids the widgets are given
static_assert(0x300 + I_COUNT * STRING_SIZE <= 0x1000, "the notice's texts end where its block does");

// The Updates page's texts (0x7e10..)
constexpr int32_t U_TEXT_FIRST = 0x7e10;
enum : int32_t {
    I_U_TITLE, I_U_CHECK, I_U_ON, I_U_OFF, I_U_CHANNEL, I_U_STABLE, I_U_DEV, I_U_CHECK_NOW, I_U_CHECK_VAL,
    I_U_HELP_CHECK, I_U_HELP_CHANNEL, I_U_HELP_NOW, I_U_COUNT
};
static_assert(0x300 + I_U_COUNT * U_STRING_SIZE <= 0x1000, "the Updates page's texts end where its block does");

// per language (0x15ed88: 0 English, 2 French (in English for now), 3 German, 4 Spanish, 5 Italian); {v} the
// new version, {i} the installed one, {n} the commits, {s} the setup's command. The release notes are GitHub's (English).
enum : int32_t {
    T_TITLE, T_UPDATE, T_OK, T_SKIP, T_RELEASE, T_COMMITS, T_COMMIT, T_DO_UPDATE, T_HOW_LATER, T_SKIP_RELEASE, T_SKIP_COMMITS,
    T_U_TITLE, T_U_CHECK, T_U_ON, T_U_OFF, T_U_CHANNEL, T_U_STABLE, T_U_DEV, T_U_CHECK_NOW,
    T_U_CHECKING, T_U_UP_TO_DATE, T_U_OFFLINE,
    T_U_HELP_CHECK, T_U_HELP_CHANNEL, T_U_HELP_NOW_IDLE, T_U_HELP_NOW_CHECKING, T_U_HELP_NOW_LATEST, T_U_HELP_NOW_OFFLINE,
    T_COUNT
};
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Update Available", "Update Now", "Not Now", "Skip Version",
     "Version {v} is out (you have {i}).", "{n} new commits on main (you have {i}).", "1 new commit on main (you have {i}).",
     "Downloads and builds the update, then restarts the game.", "To update later: {s} --update",
     "Not shown again for version {v}.", "Not shown again until main has newer commits.",
     "Updates", "Update Check", "On", "Off", "Update Channel", "Stable", "Development", "Check Now",
     "...", "Up to date", "Offline",
     "Check for a newer version when opening the main menu.",
     "Stable checks numbered releases. Development checks the latest commits on main.",
     "Check GitHub now for a newer version ({i} installed).",
     "Checking GitHub for updates...",
     "You have the latest version ({i}).",
     "Could not reach GitHub to check for updates."},
    {"Update verfugbar", "Jetzt aktualisieren", "Spater", "Uberspringen",
     "Version {v} ist da (installiert: {i}).", "{n} neue Commits auf main (installiert: {i}).", "1 neuer Commit auf main (installiert: {i}).",
     "Ladt das Update herunter, baut es und startet das Spiel neu.", "Spater aktualisieren: {s} --update",
     "Fur Version {v} nicht mehr angezeigt.", "Erst bei neueren Commits auf main wieder angezeigt.",
     "Updates", "Update-Suche", "An", "Aus", "Update-Kanal", "Stabil", "Entwicklung", "Jetzt prufen",
     "...", "Aktuell", "Offline",
     "Beim Offnen des Hauptmenus nach einer neueren Version suchen.",
     "Stabil pruft Releases. Entwicklung pruft die neuesten Commits auf main.",
     "Jetzt auf GitHub nach einer neueren Version suchen ({i} installiert).",
     "Suche auf GitHub nach Updates...",
     "Die neueste Version ist installiert ({i}).",
     "GitHub konnte nicht erreicht werden."},
    {"Actualizacion disponible", "Actualizar ahora", "Ahora no", "Omitir version",
     "Ya esta la version {v} (tienes la {i}).", "{n} commits nuevos en main (tienes {i}).", "1 commit nuevo en main (tienes {i}).",
     "Descarga y compila la actualizacion y reinicia el juego.", "Para actualizar despues: {s} --update",
     "No se mostrara mas para la version {v}.", "No se mostrara hasta que haya commits nuevos en main.",
     "Actualizaciones", "Buscar versiones", "Si", "No", "Canal", "Estable", "Desarrollo", "Comprobar ahora",
     "...", "Al dia", "Sin red",
     "Buscar una version nueva al abrir el menu principal.",
     "Estable busca versiones publicadas. Desarrollo busca los ultimos commits en main.",
     "Buscar ahora una version mas reciente en GitHub ({i} instalada).",
     "Buscando actualizaciones en GitHub...",
     "Tienes la ultima version ({i}).",
     "No se pudo conectar con GitHub."},
    {"Aggiornamento disponibile", "Aggiorna ora", "Non ora", "Salta versione",
     "E uscita la versione {v} (hai la {i}).", "{n} nuovi commit su main (hai {i}).", "1 nuovo commit su main (hai {i}).",
     "Scarica e compila l'aggiornamento, poi riavvia il gioco.", "Per aggiornare dopo: {s} --update",
     "Non sara piu mostrato per la versione {v}.", "Non sara piu mostrato finche non ci sono nuovi commit su main.",
     "Aggiornamenti", "Cerca aggiornamenti", "Si", "No", "Canale", "Stabile", "Sviluppo", "Controlla ora",
     "...", "Aggiornato", "Offline",
     "Controlla se c'e una nuova versione all'apertura del menu principale.",
     "Stabile controlla le release. Sviluppo controlla gli ultimi commit su main.",
     "Controlla subito su GitHub se c'e una nuova versione ({i} installata).",
     "Controllo aggiornamenti su GitHub in corso...",
     "Hai gia l'ultima versione ({i}).",
     "Impossibile contattare GitHub per controllare gli aggiornamenti."},
};

struct State {
    bool offered = false, shown = false, update_now = false;
    bool auto_check_done = false, auto_check_pending = false, manual_check_pending = false;
    int check_status = 0;  // 0 idle, 1 checking, 2 up to date, 3 offline
    std::string installed_ver;
    UpdateOffer offer;
} st_;

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}
// orig: (none) — enhancement: the page `page`'s slots holding `from` pointed at `to`
void slot_replace(uint32_t page, uint32_t from, uint32_t to) {
    for (uint32_t i = 0; i < 14; ++i)
        if (lwu(page + 0x44 + i * 4) == from) sw(page + 0x44 + i * 4, to);
}

// orig: (none) — enhancement: the language's texts
const char* const* texts() {
    switch (lwu(0x0015ed88)) {
        case 3: return TEXTS[1];
        case 4: return TEXTS[2];
        case 5: return TEXTS[3];
        default: return TEXTS[0];
    }
}

// orig: (none) — enhancement: a text with its {v} {i} {n} {s} filled in
std::string fill(const char* t) {
    const UpdateOffer& o = st_.offer;
    const std::string& inst = !o.installed.empty() ? o.installed : st_.installed_ver;
    std::string out;
    for (const char* p = t; *p != 0; ++p) {
        if (p[0] == '{' && p[1] != 0 && p[2] == '}') {
            switch (p[1]) {
                case 'v': out += o.version; break;
                case 'i': out += inst; break;
                case 'n': out += std::to_string(o.commits); break;
                case 's': out += o.windows ? "setup.cmd" : "./setup.sh"; break;
                default: break;
            }
            p += 2;
            continue;
        }
        out += *p;
    }
    return out;
}

// orig: (none) — enhancement: the notice page built (its parent `parent`: the main menu's root page or the Updates page)
void page_build(uint32_t parent) {
    const uint32_t opt = OPTIONS_PAGE;
    ee_copy(PAGE, opt, 0x84);
    const uint32_t title = lwu(opt + 0x44), label = lwu(opt + 0x48), picture = lwu(opt + 0x4c), list = lwu(opt + 0x50);
    ee_copy(TITLE, title, 0x50);
    ee_copy(LABEL, label, 0x50);
    ee_copy(PICTURE, picture, 0x50);
    ee_copy(LIST, list, 0x50);
    sw(TITLE + 0x34, TEXT_FIRST + I_TITLE);
    // the entries: Update Now, Not Now and Skip Version, each a page (action 3) back to `parent`
    const uint32_t ng = lwu(ROOT_NEW_GAME + 0x34);
    constexpr uint32_t N = 3;
    static constexpr int32_t NAMES[N] = {I_UPDATE, I_OK, I_SKIP}, HELPS[N] = {I_HELP_UPDATE, I_HELP_OK, I_HELP_SKIP};
    for (uint32_t i = 0; i < N; ++i) {
        const uint32_t e = ENTRIES + i * 0xc;
        ee_copy(e, ng, 0xc);
        sh(e, static_cast<uint32_t>(TEXT_FIRST + NAMES[i]));
        sh(e + 2, 3);
        sw(e + 4, parent);
        sh(e + 0xa, 0);
        sw(HELP + i * 4, static_cast<uint32_t>(TEXT_FIRST + HELPS[i]));
        sw(PICTURES + i * 8, PICTURE_SECTOR + PIC_UPDATE);
        sw(PICTURES + i * 8 + 4, 1);
    }
    for (uint32_t i = 0; i < 0xc; i += 4) sw(ENTRIES + N * 0xc + i, 0);
    sw(HELP + N * 4, 0);
    sw(LIST + 0x34, ENTRIES);
    sw(LIST + 0x40, 0);  // (Update Now selected first)
    sw(LIST + 0x38, 0);
    sw(LIST + 0x3c, 0);
    sw(LABEL + 0x34, HELP);
    sw(PICTURE + 0x30, PICTURES);
    sw(PAGE + 0x38, parent);
    sw(PAGE + 0x40, LIST);
    slot_replace(PAGE, title, TITLE);
    slot_replace(PAGE, label, LABEL);
    slot_replace(PAGE, picture, PICTURE);
    slot_replace(PAGE, list, LIST);
    sw(PAGE + 5 * 4, parent == ROOT_PAGE ? 0x13u : 0x5u);
    sw(PAGE + 0x80, 0);
}
}  // namespace

// orig: (none) — enhancement: installs the Updates button on the main menu's root page and builds the Updates page;
// on the first call (when the player opens the main menu on the title screen), triggers the automatic update check
void update_menu_main_install() {
    if (g_advanced_disabled) return;
    if (!st_.auto_check_done) {
        st_.auto_check_done = true;
        st_.auto_check_pending = true;
    }

    // 1. The Updates page: a copy of Camera's page with Options' help box in slot 1
    ee_copy(U_PAGE, CAMERA_PAGE, 0x84);
    ee_copy(U_TITLE, CAMERA_TITLE, 0x50);
    ee_copy(U_ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    ee_copy(U_HELP_WIDGET, OPTIONS_HELP, 0x50);
    sw(U_TITLE + 0x34, U_TEXT_FIRST + I_U_TITLE);
    sb(U_STEP_CHECK, 0);

    struct URow {
        int32_t name;
        uint32_t byte;
        int32_t v0, v1;
        int32_t help;
    };
    const URow rows[3] = {
        {I_U_CHECK, UPDATE_OFF, I_U_ON, I_U_OFF, I_U_HELP_CHECK},
        {I_U_CHANNEL, UPDATE_DEV, I_U_STABLE, I_U_DEV, I_U_HELP_CHANNEL},
        {I_U_CHECK_NOW, U_STEP_CHECK, I_U_CHECK_VAL, I_U_CHECK_VAL, I_U_HELP_NOW},
    };
    for (uint32_t i = 0; i < 3; ++i) {
        const uint32_t r = U_ROWS + i * 0x18;
        sw(r, static_cast<uint32_t>(U_TEXT_FIRST + rows[i].name));
        sw(r + 4, rows[i].byte);
        sw(r + 8, static_cast<uint32_t>(U_TEXT_FIRST + rows[i].v0));
        sw(r + 0xc, static_cast<uint32_t>(U_TEXT_FIRST + rows[i].v1));
        sw(r + 0x10, 0);
        sw(r + 0x14, 0);
        sw(U_ROW_HELP + i * 4, static_cast<uint32_t>(U_TEXT_FIRST + rows[i].help));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(U_ROWS + 3 * 0x18 + i, 0);
    sw(U_ROW_HELP + 3 * 4, 0);

    sw(U_ROWS_WIDGET + 0x34, U_ROWS);
    sw(U_ROWS_WIDGET + 0x38, 0);
    sw(U_ROWS_WIDGET + 0x40, 0);
    sw(U_HELP_WIDGET + 0x30, 0x80);  // (reads the focused widget's +0x40, mirrored from +0x38)
    sw(U_HELP_WIDGET + 0x34, U_ROW_HELP);
    sw(U_PAGE + 0x38, ROOT_PAGE);
    sw(U_PAGE + 0x40, U_ROWS_WIDGET);
    slot_replace(U_PAGE, CAMERA_TITLE, U_TITLE);
    slot_replace(U_PAGE, CAMERA_SETTINGS, U_ROWS_WIDGET);
    sw(U_PAGE + 0xc, lwu(TALL_PAGE + 0xc));
    sw(U_PAGE + 4, lwu(OPTIONS_PAGE + 4));
    sw(U_PAGE + 0x44 + 4, U_HELP_WIDGET);
    sw(U_PAGE + 5 * 4, 0x13);
    sw(U_PAGE + 0x80, 0);

    // 2. The root page's 4th button ("Updates" / "Aggiornamenti") in slot 5 (anim 0x13, below Options)
    const uint32_t w_new = lwu(ROOT_PAGE + 0x44 + 2 * 4);
    const uint32_t w_load = lwu(ROOT_PAGE + 0x44 + 3 * 4);
    const uint32_t w_opt = lwu(ROOT_PAGE + 0x44 + 4 * 4);
    if (w_new != 0 && w_opt != 0) {
        ee_copy(U_ROOT_WIDGET, w_opt, 0x50);
        ee_copy(U_ROOT_ENTRY, lwu(w_opt + 0x34), 0x18);
        sh(U_ROOT_ENTRY, static_cast<uint32_t>(U_TEXT_FIRST + I_U_TITLE));
        sh(U_ROOT_ENTRY + 2, 3);
        sw(U_ROOT_ENTRY + 4, U_PAGE);
        sh(U_ROOT_ENTRY + 0xa, 0);
        sw(U_ROOT_WIDGET + 0x34, U_ROOT_ENTRY);
        sw(U_ROOT_WIDGET + 0x40, 0);
        sw(U_ROOT_WIDGET + 0x38, w_opt);
        sw(U_ROOT_WIDGET + 0x3c, w_new);
        sw(w_opt + 0x3c, U_ROOT_WIDGET);
        sw(w_new + 0x38, U_ROOT_WIDGET);
        sw(ROOT_PAGE + 5 * 4, 0x13);
        sw(ROOT_PAGE + 0x44 + 5 * 4, U_ROOT_WIDGET);
        if (w_load != 0 && lwu(w_load + 0x34) != 0) sw(lwu(lwu(w_load + 0x34) + 4) + 5 * 4, 0x13);
        if (lwu(w_opt + 0x34) != 0) sw(lwu(lwu(w_opt + 0x34) + 4) + 5 * 4, 0x13);
    }
}

// orig: (none) — enhancement: the Updates page's address (graphics_menu_back)
uint32_t updates_page() { return U_PAGE; }

// orig: (none) — enhancement: the root page's Updates button, its widget in slot 5 once installed (graphics_menu_back)
uint32_t updates_root_widget() { return U_ROOT_WIDGET; }

// orig: (none) — enhancement: a row on the Updates page changed (Cross or Left/Right)
bool update_menu_setting_changed(uint32_t byte) {
    if (byte == U_STEP_CHECK) {
        sb(U_STEP_CHECK, 0);
        st_.manual_check_pending = true;
        return true;
    }
    if (byte == UPDATE_OFF || byte == UPDATE_DEV) {
        st_.check_status = 0;
        return true;
    }
    return false;
}

// orig: (none) — enhancement: the Updates page's settings widget (help index mirrored; Left/Right on rows 0 and 1)
bool update_menu_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (w != U_ROWS_WIDGET) return false;
    sw(w + 0x40, lwu(w + 0x38));  // (the help label's index)
    if ((pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b == UPDATE_OFF || b == UPDATE_DEV) {
        sb(b, lbu(b) ^ 1u);
        advanced_setting_changed(b);
        return true;
    }
    return b == U_STEP_CHECK;
}

// orig: (none) — enhancement: true once after the player opens the main menu on the title screen
bool update_notice_take_auto_check() {
    if (!st_.auto_check_pending) return false;
    st_.auto_check_pending = false;
    return true;
}

// orig: (none) — enhancement: true when the player pressed Check Now on the Updates page
bool update_notice_take_manual_check() {
    if (!st_.manual_check_pending) return false;
    st_.manual_check_pending = false;
    return true;
}

// orig: (none) — enhancement: sets the Check Now row's status (0 idle, 1 checking, 2 up to date, 3 offline) and installed version
void update_notice_set_status(int status, const std::string& installed) {
    st_.check_status = status;
    if (!installed.empty()) st_.installed_ver = installed;
}

// orig: (none) — enhancement (update_notice.h)
void update_notice_offer(const UpdateOffer& offer) {
    if (st_.offered && st_.offer.development == offer.development && st_.offer.id == offer.id) return;
    st_.offered = true;
    st_.shown = false;
    st_.offer = offer;
}

// orig: (none) — enhancement: offers `offer` and re-arms the notice even if already shown earlier (Check Now)
void update_notice_offer_manual(const UpdateOffer& offer) {
    st_.offered = true;
    st_.shown = false;
    st_.offer = offer;
}

// orig: (none) — enhancement (update_notice.h)
void update_notice_menu_frame(uint32_t ms, uint32_t root) {
    if (!st_.offered || st_.shown || g_advanced_disabled) return;
    // idle on the root page or on the Updates page: no page change or closing under way, no memory card screen asked
    const uint32_t cur = lwu(ms + 4);
    if ((cur != root && cur != U_PAGE) || lwu(ms + 8) != 0 || lw(ms) == 1 || lw(ms + 0xc) != 0 || (lwu(0x0015eeb4) & 1) != 0) return;
    st_.shown = true;
    page_build(cur);
    sw(ms + 8, PAGE);
    if (st_.offer.development)
        std::printf("[update] the notice shown: %d new commit(s) on main\n", st_.offer.commits);
    else
        std::printf("[update] the notice shown: version %s\n", st_.offer.version.c_str());
}

// orig: (none) — enhancement (update_notice.h)
void update_notice_choice(uint32_t w) {
    if (w != LIST || !st_.shown) return;
    const int32_t sel = lw(w + 0x40);
    if (sel == 0) {
        st_.update_now = true;
        std::printf("[update] Update Now chosen (%s)\n", st_.offer.id.c_str());
    } else if (sel == 2) {
        advanced_update_skip(st_.offer.id);
        std::printf("[update] %s skipped: not shown again\n", st_.offer.id.c_str());
    }
}

// orig: (none) — enhancement: true when the player chose Update Now on the notice
bool update_notice_update_now(bool* development) {
    if (!st_.update_now) return false;
    if (development != nullptr) *development = st_.offer.development;
    return true;
}

// orig: (none) — enhancement (update_notice.h)
uint32_t update_notice_page() { return st_.shown ? PAGE : 0; }

// orig: (none) — enhancement (update_notice.h)
uint32_t update_notice_text(int32_t id) {
    const char* const* t = texts();
    if (id >= U_TEXT_FIRST && id < U_TEXT_FIRST + I_U_COUNT) {
        const int32_t k = id - U_TEXT_FIRST;
        std::string s;
        switch (k) {
            case I_U_TITLE: s = t[T_U_TITLE]; break;
            case I_U_CHECK: s = t[T_U_CHECK]; break;
            case I_U_ON: s = t[T_U_ON]; break;
            case I_U_OFF: s = t[T_U_OFF]; break;
            case I_U_CHANNEL: s = t[T_U_CHANNEL]; break;
            case I_U_STABLE: s = t[T_U_STABLE]; break;
            case I_U_DEV: s = t[T_U_DEV]; break;
            case I_U_CHECK_NOW: s = t[T_U_CHECK_NOW]; break;
            case I_U_CHECK_VAL:
                if (st_.check_status == 1) s = t[T_U_CHECKING];
                else if (st_.check_status == 2) s = t[T_U_UP_TO_DATE];
                else if (st_.check_status == 3) s = t[T_U_OFFLINE];
                break;
            case I_U_HELP_CHECK: s = t[T_U_HELP_CHECK]; break;
            case I_U_HELP_CHANNEL: s = t[T_U_HELP_CHANNEL]; break;
            case I_U_HELP_NOW:
                if (st_.check_status == 1) s = t[T_U_HELP_NOW_CHECKING];
                else if (st_.check_status == 2) s = fill(t[T_U_HELP_NOW_LATEST]);
                else if (st_.check_status == 3) s = t[T_U_HELP_NOW_OFFLINE];
                else s = fill(t[T_U_HELP_NOW_IDLE]);
                break;
            default: break;
        }
        const uint32_t at = U_STRINGS + static_cast<uint32_t>(k) * U_STRING_SIZE;
        ee_zero(at, U_STRING_SIZE);
        ee_copy_in(at, s.c_str(), std::min<size_t>(s.size(), U_STRING_SIZE - 1));
        return at;
    }
    if (id < TEXT_FIRST || id >= TEXT_FIRST + I_COUNT || !st_.offered) return 0;
    const int32_t k = id - TEXT_FIRST;
    const UpdateOffer& o = st_.offer;
    const std::string news = fill(t[o.development ? (o.commits == 1 ? T_COMMIT : T_COMMITS) : T_RELEASE]);
    const std::string how = fill(t[T_HOW_LATER]);
    std::string s;
    switch (k) {
        case I_TITLE: s = t[T_TITLE]; break;
        case I_UPDATE: s = t[T_UPDATE]; break;
        case I_OK: s = t[T_OK]; break;
        case I_SKIP: s = t[T_SKIP]; break;
        case I_HELP_UPDATE:
            s = news;
            if (!o.development && !o.notes.empty()) s += " " + o.notes;
            s += std::string(" ") + t[T_DO_UPDATE];
            break;
        case I_HELP_OK:
            s = news + " " + how + ".";
            if (!o.development && !o.notes.empty()) s += " " + o.notes;
            break;
        case I_HELP_SKIP: s = fill(t[o.development ? T_SKIP_COMMITS : T_SKIP_RELEASE]) + " " + how + "."; break;
        default: break;
    }
    const uint32_t at = STRINGS + static_cast<uint32_t>(k) * STRING_SIZE;
    ee_zero(at, STRING_SIZE);
    ee_copy_in(at, s.c_str(), std::min<size_t>(s.size(), STRING_SIZE - 1));
    return at;
}

}  // namespace rac1
