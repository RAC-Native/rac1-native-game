// The main menu's "Exit" entry (an enhancement): the last button of the title's main menu, below New Game, Load Game,
// Options and Updates, asks to close the game and, confirmed, closes it as closing the window does (the host's loop
// ends: the settings, the audio and the renderer shut down, the log's last lines). Not a port: built from the game's own
// menu data, as the Updates button (update_notice.cpp):
//
// - The button: a copy of the root page's Options button (a one-entry list) in slot 7 (a slot every main menu page keeps
//   hidden), with the root's animation 0x14: the boxes' column, one row below Updates' 0x13. Its entry opens our page
//   (action 3). Up / down go round New Game, Load Game, Options, Updates, Exit.
// - The page: a copy of the pause menu's Quit Game page (0x1b5be0, in every overlay and in the boot module) with a copy of
//   its question widget (0x1b5c68: update 0x27c410, draw 0x27c4d8). The widget's own code runs it: triangle back to the
//   main menu, Start / Select / L1 close the menu; circle, which in the pause menu leaves the game for the title, closes
//   the game instead (exit_menu_confirm, a hook in menu_quit_update). The two answers are the game's own (circle Yes,
//   triangle No); the question and its line are ours (exit_menu_text_id, a hook in menu_quit_draw).
//
// Only where a program is closed by the player: not on Android or the consoles (the system closes the game there).
// Never with the enhancements off (the headless tests, the oracle): the original's menu as it is.
//
// EE RAM block XM (0xfa800..0xfb000): past the Language page's (language.cpp, up to 0xfa800), before the Advanced
// block's texts (0xfb000), in the PS2 kernel's first megabyte: nothing of the game or the runtime lives there.
#include <game/enhance/advanced.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdio>
#include <cstring>

namespace rac1 {

// (update_notice.cpp's, declared here: the headers of src/game are the generated code's fingerprint)
uint32_t updates_root_widget();

namespace {
#if defined(__ANDROID__) || defined(__SWITCH__) || defined(__vita__) || defined(__3DS__) || defined(__PS4__) || defined(__ORBIS__)
constexpr bool EXIT_PLATFORM = false;  // (the system closes the game)
#else
constexpr bool EXIT_PLATFORM = true;
#endif

constexpr uint32_t XM = 0x000fa800u;
constexpr uint32_t PAGE = XM;                // the Exit page (a copy of Quit Game's)
constexpr uint32_t QUESTION = XM + 0x90;     // its question widget (a copy of Quit Game's)
constexpr uint32_t ROOT_WIDGET = XM + 0xe0;  // the root page's Exit button (slot 7)
constexpr uint32_t ROOT_ENTRY = XM + 0x130;  // its one-entry list (0x18)
constexpr uint32_t STRINGS = XM + 0x200;     // the texts, made at each lookup
constexpr uint32_t STRING_SIZE = 0x100;

constexpr uint32_t SLOT = 7;     // the button's slot on the root page
constexpr uint32_t SET = 0x14;   // its animation: the column's sixth row (frame 0, where the root page holds its boxes)
constexpr uint32_t HIDDEN = 7;   // the slot's animation on the main menu's pages: off the screen

constexpr OvConst<0x001b5be0u> QUIT_PAGE{};
constexpr OvConst<0x001b5c68u> QUIT_QUESTION{};
constexpr OvConst<0x001b86c8u> ROOT_PAGE{};
constexpr uint32_t PAD = 0x0013c940;  // pad 1

// The texts: ids 0x7c00.. (below the Mods page's 0x7d00..)
constexpr int32_t TEXT_FIRST = 0x7c00;
enum : int32_t { I_EXIT, I_QUESTION, I_LINE, I_COUNT };
static_assert(0x200 + I_COUNT * STRING_SIZE <= 0x800, "the Exit texts end where the block does");
// per language (0x15ed88: 0 English, 2 French (in English for now, as the other pages), 3 German, 4 Spanish, 5 Italian)
constexpr const char* TEXTS[4][I_COUNT] = {
    {"Exit", "Are you sure you want to close the game?", "Exit Game?"},
    {"Beenden", "Willst du das Spiel wirklich beenden?", "Spiel beenden?"},
    {"Salir", "Seguro que quieres cerrar el juego?", "Salir del juego?"},
    {"Esci", "Sei sicuro di voler chiudere il gioco?", "Uscire dal gioco?"},
};

bool g_installed = false;   // the button is on the root page
bool g_requested = false;   // Exit confirmed: the host closes the window

// orig: (none) — enhancement: n bytes (a multiple of 4) copied in EE RAM
void ee_copy(uint32_t dst, uint32_t src, uint32_t n) {
    for (uint32_t i = 0; i < n; i += 4) sw(dst + i, lwu(src + i));
}
}  // namespace

// orig: (none) — enhancement: the Exit button put on the main menu's root page and its page built (the main menu's
// start, after the Updates button: graphics_main_menu_install)
void exit_menu_install() {
    if (g_advanced_disabled || !EXIT_PLATFORM) return;
    const uint32_t w_new = lwu(ROOT_PAGE + 0x44 + 2 * 4);
    const uint32_t w_load = lwu(ROOT_PAGE + 0x44 + 3 * 4);
    const uint32_t w_opt = lwu(ROOT_PAGE + 0x44 + 4 * 4);
    if (w_new == 0 || w_opt == 0 || lwu(w_opt + 0x34) == 0) return;
    // the slot must be the hidden one the original gives it (never another enhancement's)
    if (lwu(ROOT_PAGE + 0x44 + SLOT * 4) != 0 && lwu(ROOT_PAGE + 0x44 + SLOT * 4) != ROOT_WIDGET) return;
    // the button above it: Updates (slot 5) when it is there, else Options
    const uint32_t w_upd = lwu(ROOT_PAGE + 0x44 + 5 * 4);
    const uint32_t above = w_upd != 0 && w_upd == updates_root_widget() ? w_upd : w_opt;

    // 1. The page: Quit Game's, its question ours, back to the root page
    ee_copy(PAGE, QUIT_PAGE, 0x84);
    ee_copy(QUESTION, QUIT_QUESTION, 0x50);
    for (uint32_t i = 0; i < 14; ++i)
        if (lwu(PAGE + 0x44 + i * 4) == QUIT_QUESTION) sw(PAGE + 0x44 + i * 4, QUESTION);
    sw(PAGE + 0x38, ROOT_PAGE);
    sw(PAGE + 0x40, QUESTION);
    if (above == w_upd) sw(PAGE + 5 * 4, 0x13);  // (slot 5: the Updates button, as New Game's windows hold it)
    sw(PAGE + SLOT * 4, SET);
    sw(PAGE + 0x80, 0);

    // 2. The button: a copy of Options', its entry to the page (action 3), in slot 7 under `above`
    ee_copy(ROOT_WIDGET, w_opt, 0x50);
    ee_copy(ROOT_ENTRY, lwu(w_opt + 0x34), 0x18);
    sh(ROOT_ENTRY, static_cast<uint32_t>(TEXT_FIRST + I_EXIT));
    sh(ROOT_ENTRY + 2, 3);
    sw(ROOT_ENTRY + 4, PAGE);
    sh(ROOT_ENTRY + 0xa, 0);
    sw(ROOT_WIDGET + 0x34, ROOT_ENTRY);
    sw(ROOT_WIDGET + 0x40, 0);
    sw(ROOT_WIDGET + 0x38, above);  // (up)
    sw(ROOT_WIDGET + 0x3c, w_new);  // (down: round to New Game)
    sw(above + 0x3c, ROOT_WIDGET);
    sw(w_new + 0x38, ROOT_WIDGET);
    sw(ROOT_PAGE + SLOT * 4, SET);
    sw(ROOT_PAGE + 0x44 + SLOT * 4, ROOT_WIDGET);
    // Load Game's and Options' pages: the button's box leaves with the others (as Updates' does)
    if (w_load != 0 && lwu(w_load + 0x34) != 0 && lwu(lwu(lwu(w_load + 0x34) + 4) + SLOT * 4) == HIDDEN)
        sw(lwu(lwu(w_load + 0x34) + 4) + SLOT * 4, SET);
    if (lwu(lwu(lwu(w_opt + 0x34) + 4) + SLOT * 4) == HIDDEN) sw(lwu(lwu(w_opt + 0x34) + 4) + SLOT * 4, SET);
    if (!g_installed) std::printf("[exit] the main menu's Exit entry added\n");
    g_installed = true;
}

// orig: (none) — enhancement: a page of ours opened from the root page (New Game's windows, the update notice): the Exit
// button's box held in its slot as the root page leaves it (as Updates' in slot 5)
void exit_menu_page_slot(uint32_t page) {
    if (g_installed && page != 0 && lwu(page + SLOT * 4) == HIDDEN) sw(page + SLOT * 4, SET);
}

// orig: (none) — enhancement: the Exit page's address (graphics_menu_back: leaving it, the root page's boxes), 0 when
// there is none
uint32_t exit_menu_page() { return g_installed ? PAGE : 0; }

// orig: (none) — enhancement: the root page's Exit button, its widget in slot 7 once installed (graphics_menu_back)
uint32_t exit_menu_root_widget() { return g_installed ? ROOT_WIDGET : 0; }
// orig: (none) — enhancement: the button's slot on the root page (graphics_menu_back)
uint32_t exit_menu_slot() { return SLOT; }
// orig: (none) — enhancement: the button's animation on the root page (graphics_menu_back)
uint32_t exit_menu_set() { return SET; }

// orig: (none) — enhancement: the question widget `w`'s update (menu_quit_update, before its circle): ours, circle
// pressed: the game closed (the host's window closed at its next frame); true when it was ours and circle was pressed
bool exit_menu_confirm(uint32_t w) {
    if (!g_installed || w != QUESTION) return false;
    if ((lwu(PAD + 0x1c4) & 0x20) == 0) return false;
    if (!g_requested) std::printf("[exit] Exit confirmed in the main menu: the game closes\n");
    g_requested = true;
    return true;
}

// orig: (none) — enhancement: the question widget `w` drawn (menu_quit_draw): its question (`line` false) or its line
// (true) text id, ours for our widget, else `id`
int32_t exit_menu_text_id(uint32_t w, bool line, int32_t id) {
    if (!g_installed || w != QUESTION) return id;
    return TEXT_FIRST + (line ? I_LINE : I_QUESTION);
}

// orig: (none) — enhancement: the host's loop: true once when Exit was confirmed
bool exit_menu_take() {
    if (!g_requested) return false;
    g_requested = false;
    return true;
}

// orig: (none) — enhancement: a text of ours (ids 0x7c00..), 0 for others
uint32_t exit_menu_text(int32_t id) {
    if (id < TEXT_FIRST || id >= TEXT_FIRST + I_COUNT) return 0;
    const char* const* t = TEXTS[0];
    switch (lwu(0x0015ed88)) {
        case 3: t = TEXTS[1]; break;
        case 4: t = TEXTS[2]; break;
        case 5: t = TEXTS[3]; break;
        default: break;
    }
    const uint32_t k = static_cast<uint32_t>(id - TEXT_FIRST);
    char* p = static_cast<char*>(ee_ram_ptr(STRINGS + k * STRING_SIZE));
    std::strncpy(p, t[k], STRING_SIZE - 1);
    p[STRING_SIZE - 1] = 0;
    return STRINGS + k * STRING_SIZE;
}

}  // namespace rac1
