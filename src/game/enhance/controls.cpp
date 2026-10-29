// The Controls page (see controls.h). Not a port: everything here is new, written on top of the
// original's menu data so that the page
// looks and behaves like the game's own, as the Graphics page (graphics.cpp) it is made like:
// a copy of the Camera page (0x1b5a60) with its settings widget (0x27ef00 / 0x27f0a8) in the tall
// box of page 0x1b6678 and a copy of the Options help label (0x1b4b68) in the Options help box.
//
// Rows: the device (gamepad / keyboard and mouse), the PS2 pad's controls (their names with the
// game font's button glyphs), look inversion, the mouse's sensitivity, reset to defaults. Each row's
// byte is a "step" byte (two values, both the row's value text, made at each lookup): cross steps
// it, controls_setting_changed acts and puts it back to 0. The rows are more than the widget's box
// shows at once: the widget is given a window of them (ROWS), moved when up / down go past its
// ends (controls_settings_dpad). The help label (flag 0x80: the focused widget's +0x40, where the
// selection +0x38 is mirrored) shows one text made from the selected row and what is happening
// (the input awaited, the result of a rebinding, a swap).
//
// EE RAM block CTL (0xf6000..0xf8000): the end of the Graphics page's block (graphics.cpp: up to
// 0xf6000 used), in the PS2 kernel's first megabyte: nothing of the game or the runtime lives there.
#include <game/enhance/controls.h>

#include <game/enhance/advanced.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <algorithm>
#include <cstdio>
#include <cstring>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t CTL = 0x000f6000u;
constexpr uint32_t PAGE = CTL;                 // the page (a copy of Camera's)
constexpr uint32_t TITLE = CTL + 0x90;         // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = CTL + 0xe0;   // its settings widget (a copy of Camera's)
constexpr uint32_t HELP_WIDGET = CTL + 0x130;  // its help label (a copy of Options')
constexpr uint32_t STEPS = CTL + 0x180;        // the shown rows' step bytes (the settings widget writes them)
constexpr uint32_t ROW_HELP = CTL + 0x1a0;     // the help label's texts, one per shown row
constexpr uint32_t ROWS = CTL + 0x200;         // the settings widget's rows (0x18 bytes, ended by text 0)
constexpr uint32_t STRINGS = CTL + 0x480;      // the texts, made at each lookup (..0xf7400)

// The game's data the page is made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b6678u> TALL_PAGE{};  // its slot 3 box: the Graphics page's settings box
constexpr uint32_t PAD = 0x0013c940;          // g_pad_state (+0x1c4 the buttons pressed this frame)

// The page's rows: the device, the host's 17 remap rows, then five settings
#if defined(__WIIU__)
enum : int { R_FIRST_CONTROL = 0, CONTROL_ROWS = 17, R_INVERT = R_FIRST_CONTROL + CONTROL_ROWS, R_RESET, ROW_COUNT, R_DEVICE = -1, R_MOUSE_MODE = -2, R_SENS = -3, R_AIM_SENS = -4 };
#else
enum : int { R_DEVICE = 0, R_FIRST_CONTROL = 1, CONTROL_ROWS = 17, R_INVERT = R_FIRST_CONTROL + CONTROL_ROWS, R_MOUSE_MODE, R_SENS, R_AIM_SENS, R_RESET, ROW_COUNT };
#endif
constexpr int C_FIRST_GROUP = 12, C_LAST_GROUP = 14;  // the remap rows of four (the D-pad, the sticks)
constexpr int VISIBLE = 10;  // the rows the box shows at once (the widget spreads its rows over the box's height)
constexpr int SENS[] = {25, 50, 75, 100, 125, 150, 200, 250, 300, 400};  // the mouse's sensitivity, percent

// The texts. Ids from TEXT_FIRST (after the Graphics page's 0x7f40..0x7f9f); per language (0x15ed88:
// 0 English, 2 French (in English for now), 3 German, 4 Spanish, 5 Italian)
constexpr int32_t TEXT_FIRST = 0x7fa0;
enum : int32_t {
    T_TITLE, T_OPTIONS_HELP, T_DEVICE, T_GAMEPAD, T_KBM, T_INVERT, T_SENS, T_RESET, T_OFF, T_ON,
    T_CROSS, T_CIRCLE, T_SQUARE, T_TRIANGLE, T_START, T_SELECT, T_DPAD, T_LSTICK, T_RSTICK, T_NEXT, T_PREV,
    T_MOUSE_MODE, T_DIRECT, T_STICK, T_AIM_SENS, H_MOUSE_MODE, H_AIM_SENS, A_NEXT, A_PREV,
    A_CROSS, A_CIRCLE, A_SQUARE, A_TRIANGLE, A_L1, A_R1, A_L3, A_START, A_SELECT, A_LSTICK, A_RSTICK,
    H_DEVICE, H_NO_PAD, H_BUTTON, H_KEY, H_GROUP_PAD, H_GROUP_KEY, H_INVERT, H_SENS, H_RESET,
    W_BUTTON, W_KEY, W_GROUP_PAD, W_GROUP_KEY, W_MOUSE, W_CANCEL, D_UP, D_DOWN, D_LEFT, D_RIGHT,
    M_DONE, M_SWAPPED, M_CANCELLED, M_RESET,
    T_COUNT
};
// (the ids of the page's own strings: the title, the Options help, the shown rows' names and values, the help)
enum : int32_t { I_TITLE = 0, I_OPTIONS_HELP = 1, I_NAME = 2, I_VALUE = I_NAME + VISIBLE, I_HELP = I_VALUE + VISIBLE, I_COUNT };
constexpr uint32_t STRING_SIZE[] = {0x40, 0x80, 0x40, 0x80, 0x200};  // title, Options help, names, values, help

// %1 / %2: replaced (the font's '%' is a button glyph: never shown)
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Controls", "The buttons, keys and mouse that play the game, changed for each PS2 control",
     "Device", "Gamepad", "Keyboard / Mouse", "Invert Look", "Mouse Sensitivity", "Reset to Defaults", "Off", "On",
     "Cross", "Circle", "Square", "Triangle", "Start", "Select", "D-pad", "Left Stick", "Right Stick", "Next Weapon", "Previous Weapon",
     "Mouse Mode", "Direct", "Stick", "Aim Sensitivity",
     "Direct: the mouse turns the camera and the aim by its movement. Stick: the mouse as the right stick.",
     "How far the mouse turns the aim with Modern aiming, of the mouse sensitivity.", "The quick select's next item; its ring's next slot while it is open.", "The quick select's previous item.",
     "Jump.", "Fire the equipped item.", "Swing the wrench.", "Quick select.", "First-person view.", "Crouch.", "Show the HUD.", "Pause menu.", "Map.", "Move.", "Camera.",
     "The device whose controls are shown and changed.", "No gamepad connected.",
     "Press %1 to change it, then press the new button.", "Press %1 to change it, then press the new key or mouse button.",
     "Press %1 to change it, then move a stick or press the D-pad.", "Press %1 to change it, then press the keys for up, down, left and right, or move the mouse.",
     "Up and down of the camera swapped (right stick and mouse).", "How far the mouse turns the camera.", "Puts back the default controls of this device.",
     "Press the new button for %1.", "Press the new key or mouse button for %1.", "Move a stick or press the D-pad for %1.", "Press the key for %1: %2.",
     " Or move the mouse.", " Nothing for 5 seconds cancels.", "up", "down", "left", "right",
     "%1: %2.", " Swapped with %1.", "Nothing pressed: unchanged.", "Default controls back for the %1."},
    {"Steuerung", "Tasten, Maus und Gamepad fur jede PS2-Taste andern",
     "Gerat", "Gamepad", "Tastatur / Maus", "Blick umkehren", "Mausempfindlichkeit", "Standard", "Aus", "An",
     "Kreuz", "Kreis", "Quadrat", "Dreieck", "Start", "Select", "Steuerkreuz", "Linker Stick", "Rechter Stick", "Nachste Waffe", "Vorherige Waffe",
     "Mausmodus", "Direkt", "Stick", "Zielempfindlichkeit",
     "Direkt: die Maus dreht Kamera und Zielen mit ihrer Bewegung. Stick: die Maus als rechter Stick.",
     "Wie weit die Maus beim modernen Zielen dreht, von der Mausempfindlichkeit.", "Das nachste Objekt der Schnellauswahl.", "Das vorherige Objekt der Schnellauswahl.",
     "Springen.", "Ausgeruestetes benutzen.", "Schraubenschlussel.", "Schnellauswahl.", "Ego-Ansicht.", "Ducken.", "HUD zeigen.", "Pausenmenu.", "Karte.", "Bewegen.", "Kamera.",
     "Das Gerat, dessen Steuerung gezeigt und geandert wird.", "Kein Gamepad verbunden.",
     "%1 zum Andern, dann die neue Taste drucken.", "%1 zum Andern, dann die neue Taste oder Maustaste drucken.",
     "%1 zum Andern, dann einen Stick bewegen oder das Steuerkreuz drucken.", "%1 zum Andern, dann die Tasten fur oben, unten, links und rechts drucken oder die Maus bewegen.",
     "Kamera oben und unten vertauscht (rechter Stick und Maus).", "Wie weit die Maus die Kamera dreht.", "Die Standardsteuerung dieses Gerats.",
     "Die neue Taste fur %1 drucken.", "Die neue Taste oder Maustaste fur %1 drucken.", "Einen Stick bewegen oder das Steuerkreuz drucken fur %1.", "Die Taste fur %1 drucken: %2.",
     " Oder die Maus bewegen.", " 5 Sekunden nichts: Abbruch.", "oben", "unten", "links", "rechts",
     "%1: %2.", " Getauscht mit %1.", "Nichts gedruckt: unverandert.", "Standardsteuerung fur %1 zuruck."},
    {"Controles", "Botones, teclas y raton para cada control de PS2",
     "Dispositivo", "Mando", "Teclado / Raton", "Invertir vista", "Sensibilidad del raton", "Restablecer", "No", "Si",
     "Equis", "Circulo", "Cuadrado", "Triangulo", "Start", "Select", "Cruceta", "Stick izquierdo", "Stick derecho", "Arma siguiente", "Arma anterior",
     "Modo del raton", "Directo", "Stick", "Sensibilidad al apuntar",
     "Directo: el raton gira la camara y la mira con su movimiento. Stick: el raton como stick derecho.",
     "Cuanto gira la mira el raton con el apuntado moderno, de la sensibilidad del raton.", "El objeto siguiente de la seleccion rapida.", "El objeto anterior de la seleccion rapida.",
     "Saltar.", "Usar lo equipado.", "Llave.", "Seleccion rapida.", "Vista en primera persona.", "Agacharse.", "Mostrar el HUD.", "Menu de pausa.", "Mapa.", "Moverse.", "Camara.",
     "El dispositivo cuyos controles se muestran y cambian.", "No hay mando conectado.",
     "Pulsa %1 para cambiarlo y luego el nuevo boton.", "Pulsa %1 para cambiarlo y luego la nueva tecla o boton del raton.",
     "Pulsa %1 para cambiarlo y luego mueve un stick o pulsa la cruceta.", "Pulsa %1 para cambiarlo y luego las teclas de arriba, abajo, izquierda y derecha, o mueve el raton.",
     "Arriba y abajo de la camara invertidos (stick derecho y raton).", "Cuanto gira la camara el raton.", "Vuelven los controles por defecto de este dispositivo.",
     "Pulsa el nuevo boton para %1.", "Pulsa la nueva tecla o boton del raton para %1.", "Mueve un stick o pulsa la cruceta para %1.", "Pulsa la tecla para %1: %2.",
     " O mueve el raton.", " 5 segundos sin nada: se cancela.", "arriba", "abajo", "izquierda", "derecha",
     "%1: %2.", " Intercambiado con %1.", "Nada pulsado: sin cambios.", "Controles por defecto para %1."},
    {"Controlli", "Tasti, mouse e gamepad per ogni comando della PS2",
     "Dispositivo", "Gamepad", "Tastiera / Mouse", "Inverti visuale", "Sensibilita mouse", "Ripristina", "No", "Si",
     "Croce", "Cerchio", "Quadrato", "Triangolo", "Start", "Select", "Croce direzionale", "Levetta sinistra", "Levetta destra", "Arma successiva", "Arma precedente",
     "Modo mouse", "Diretto", "Levetta", "Sensibilita mira",
     "Diretto: il mouse gira telecamera e mira col suo movimento. Levetta: il mouse come levetta destra.",
     "Quanto il mouse gira la mira moderna, della sensibilita del mouse.", "L'oggetto successivo della selezione rapida.", "L'oggetto precedente della selezione rapida.",
     "Salto.", "Usa l'oggetto equipaggiato.", "Chiave inglese.", "Selezione rapida.", "Visuale in prima persona.", "Accucciati.", "Mostra l'HUD.", "Menu di pausa.", "Mappa.", "Movimento.", "Telecamera.",
     "Il dispositivo di cui si vedono e cambiano i controlli.", "Nessun gamepad collegato.",
     "Premi %1 per cambiarlo, poi il nuovo tasto.", "Premi %1 per cambiarlo, poi il nuovo tasto o tasto del mouse.",
     "Premi %1 per cambiarlo, poi muovi una levetta o premi la croce direzionale.", "Premi %1 per cambiarlo, poi i tasti per su, giu, sinistra e destra, o muovi il mouse.",
     "Su e giu della telecamera invertiti (levetta destra e mouse).", "Quanto il mouse gira la telecamera.", "Rimette i controlli predefiniti di questo dispositivo.",
     "Premi il nuovo tasto per %1.", "Premi il nuovo tasto o tasto del mouse per %1.", "Muovi una levetta o premi la croce direzionale per %1.", "Premi il tasto per %1: %2.",
     " Oppure muovi il mouse.", " 5 secondi senza nulla: annulla.", "su", "giu", "sinistra", "destra",
     "%1: %2.", " Scambiato con %1.", "Niente premuto: invariato.", "Controlli predefiniti per %1."},
};

// The PS2 controls' names (the host's remap rows) and what the game does with them (-1: no one action), the game font's
// glyph before the name (0: none)
struct ControlRow {
    int32_t name;  // a text, or -1: `fixed`
    const char* fixed;
    int32_t action;
    char glyph;
};
constexpr ControlRow CONTROL_DEF[CONTROL_ROWS] = {
    {T_CROSS, "", A_CROSS, '\x10'},    {T_CIRCLE, "", A_CIRCLE, '\x11'}, {T_SQUARE, "", A_SQUARE, '\x13'}, {T_TRIANGLE, "", A_TRIANGLE, '\x12'},
    {-1, "L1", A_L1, '\x14'},           {-1, "R1", A_R1, '\x15'},         {-1, "L2", -1, '\x16'},           {-1, "R2", -1, '\x17'},
    {-1, "L3", A_L3, 0},                {-1, "R3", -1, 0},                {T_START, "", A_START, 0},         {T_SELECT, "", A_SELECT, 0},
    {T_DPAD, "", -1, 0},                {T_LSTICK, "", A_LSTICK, '\x19'}, {T_RSTICK, "", A_RSTICK, '\x18'},
    {T_NEXT, "", A_NEXT, 0},            {T_PREV, "", A_PREV, 0},
};

struct State {
    ControlsHost* host = nullptr;
    int device = -1;  // 0 the gamepad, 1 the keyboard / mouse (-1: chosen at the first install)
    int top = 0;      // the first row shown
    int prev_sel = 0;  // the widget's selection the last frame
    int waiting_row = -1;  // a rebinding's row (page row)
    int msg_row = -1;      // the row the message is about (-1: none)
    std::string msg;
} cs_;

// orig: (none) — enhancement: the language's texts (French in English for now, as graphics.cpp)
const char* const* texts() {
    switch (lwu(0x0015ed88)) {
        case 3: return TEXTS[1];
        case 4: return TEXTS[2];
        case 5: return TEXTS[3];
        default: return TEXTS[0];
    }
}

// orig: (none) — enhancement: `fmt` with %1 / %2 replaced
std::string fill(const char* fmt, const std::string& a, const std::string& b = "") {
    std::string out;
    for (const char* p = fmt; *p != 0; ++p) {
        if (p[0] == '%' && (p[1] == '1' || p[1] == '2')) {
            out += p[1] == '1' ? a : b;
            ++p;
        } else {
            out += *p;
        }
    }
    return out;
}

// orig: (none) — enhancement: a PS2 control's name (the glyph first when the font has one)
std::string control_name(int c, bool glyph) {
    const ControlRow& r = CONTROL_DEF[c];
    std::string s = r.name >= 0 ? texts()[r.name] : r.fixed;
    if (glyph && r.glyph != 0) s = std::string(1, r.glyph) + " " + s;
    return s;
}

// orig: (none) — enhancement: a page row's name
std::string row_name(int row) {
    const char* const* t = texts();
    if (row == R_DEVICE) return t[T_DEVICE];
    if (row == R_INVERT) return t[T_INVERT];
    if (row == R_SENS) return t[T_SENS];
    if (row == R_MOUSE_MODE) return t[T_MOUSE_MODE];
    if (row == R_AIM_SENS) return t[T_AIM_SENS];
    if (row == R_RESET) return t[T_RESET];
    return control_name(row - R_FIRST_CONTROL, true);
}

std::string device_name(int d) { return texts()[d == 0 ? T_GAMEPAD : T_KBM]; }

// orig: (none) — enhancement: a page row's value
std::string row_value(int row) {
    const char* const* t = texts();
    if (row == R_DEVICE) return device_name(cs_.device);
    if (row == R_RESET) return "";
    if (cs_.host == nullptr) return "-";
    if (row == R_INVERT) return t[cs_.host->invert_look() ? T_ON : T_OFF];
    if (row == R_MOUSE_MODE) return t[cs_.host->mouse_direct() ? T_DIRECT : T_STICK];
    if (row == R_SENS || row == R_AIM_SENS) {
        const int p = static_cast<int>((row == R_SENS ? cs_.host->mouse_sensitivity() : cs_.host->aim_sensitivity()) * 100.0f + 0.5f);
        char b[16];
        std::snprintf(b, sizeof b, "x%d.%02d", p / 100, p % 100);  // (the font's '%' is a button)
        return b;
    }
    if (row == cs_.waiting_row) return "...";
    return cs_.host->row_value(row - R_FIRST_CONTROL, cs_.device);
}

// orig: (none) — enhancement: the selected row's help, or what is happening on it
std::string help_text(int row) {
    const char* const* t = texts();
    if (cs_.host != nullptr && cs_.waiting_row >= 0) {  // a new input awaited
        const int c = cs_.waiting_row - R_FIRST_CONTROL;
        const bool group = c >= C_FIRST_GROUP && c <= C_LAST_GROUP, pad = cs_.device == 0;
        int step = 0;
        cs_.host->rebind_waiting(&step);
        std::string s;
        if (!group) s = fill(t[pad ? W_BUTTON : W_KEY], control_name(c, true));
        else if (pad) s = fill(t[W_GROUP_PAD], control_name(c, true));
        else s = fill(t[W_GROUP_KEY], control_name(c, true), t[D_UP + std::clamp(step, 0, 3)]) + (step == 0 && c != 12 ? t[W_MOUSE] : "");
        return s + t[W_CANCEL];
    }
    if (row == cs_.msg_row && !cs_.msg.empty()) return cs_.msg;
    switch (row) {
        case R_DEVICE: {
            std::string s = t[H_DEVICE];
            const std::string pad = cs_.host != nullptr ? cs_.host->pad_name() : "";
            return s + " " + (pad.empty() ? std::string(t[H_NO_PAD]) : device_name(0) + ": " + pad + ".");
        }
        case R_INVERT: return t[H_INVERT];
        case R_SENS: return t[H_SENS];
        case R_MOUSE_MODE: return t[H_MOUSE_MODE];
        case R_AIM_SENS: return t[H_AIM_SENS];
        case R_RESET: return t[H_RESET];
        default: break;
    }
    const int c = row - R_FIRST_CONTROL;
    const std::string confirm = cs_.host != nullptr ? cs_.host->row_value(0, cs_.device) : "-";  // (what presses Cross now)
    std::string s;
    if (CONTROL_DEF[c].action >= 0) s = std::string(t[CONTROL_DEF[c].action]) + " ";
    const bool pad = cs_.device == 0;
    if (c >= C_FIRST_GROUP && c <= C_LAST_GROUP) return s + fill(t[pad ? H_GROUP_PAD : H_GROUP_KEY], confirm);
    return s + fill(t[pad ? H_BUTTON : H_KEY], confirm);
}

// orig: (none) — enhancement: the widget's rows for the window from cs_.top
void rows_write() {
    uint32_t row = ROWS;
    for (int i = 0; i < VISIBLE; ++i, row += 0x18) {
        const uint32_t vt = static_cast<uint32_t>(TEXT_FIRST + T_COUNT + I_VALUE + i);
        sw(row, static_cast<uint32_t>(TEXT_FIRST + T_COUNT + I_NAME + i));
        sw(row + 4, STEPS + static_cast<uint32_t>(i));
        sw(row + 8, vt);  // (two values, both the value's text: cross steps the byte, controls_setting_changed acts)
        sw(row + 0xc, vt);
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sb(STEPS + static_cast<uint32_t>(i), 0);
        sw(ROW_HELP + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(TEXT_FIRST + T_COUNT + I_HELP));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
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

// orig: (none) — enhancement: a row's value stepped by d (cross: +1; left / right: -1 / +1)
void row_step(int row, int d) {
    ControlsHost* h = cs_.host;
    cs_.msg_row = -1;
    if (row == R_DEVICE) {
        cs_.device ^= 1;
        return;
    }
    if (h == nullptr) return;
    if (row == R_INVERT) {
        h->set_invert_look(!h->invert_look());
        return;
    }
    if (row == R_MOUSE_MODE) {
        h->set_mouse_direct(!h->mouse_direct());
        return;
    }
    if (row == R_SENS || row == R_AIM_SENS) {
        const int p = static_cast<int>((row == R_SENS ? h->mouse_sensitivity() : h->aim_sensitivity()) * 100.0f + 0.5f);
        constexpr int n = sizeof(SENS) / sizeof(int);
        int i = 0;
        while (i < n - 1 && SENS[i] < p) ++i;  // (the nearest at or above)
        i = (i + d + n) % n;
        if (row == R_SENS) h->set_mouse_sensitivity(static_cast<float>(SENS[i]) / 100.0f);
        else h->set_aim_sensitivity(static_cast<float>(SENS[i]) / 100.0f);
        return;
    }
    if (d < 0) return;  // (the controls and reset: cross only)
    if (row == R_RESET) {
        h->reset(cs_.device);
        cs_.msg_row = row;
        cs_.msg = fill(texts()[M_RESET], device_name(cs_.device));
        return;
    }
    cs_.waiting_row = row;
    h->rebind_begin(row - R_FIRST_CONTROL, cs_.device);
}

// orig: (none) — enhancement: the host's rebinding ended: the message
void rebind_poll() {
    if (cs_.host == nullptr || cs_.waiting_row < 0) return;
    std::vector<int> swapped;
    const int r = cs_.host->rebind_result(&swapped);
    if (r == 0) return;
    const char* const* t = texts();
    const int row = cs_.waiting_row, c = row - R_FIRST_CONTROL;
    cs_.waiting_row = -1;
    cs_.msg_row = row;
    if (r == 2) {
        cs_.msg = t[M_CANCELLED];
        return;
    }
    cs_.msg = fill(t[M_DONE], control_name(c, true), cs_.host->row_value(c, cs_.device));
    if (!swapped.empty()) {
        std::string names;
        for (const int s : swapped) names += (names.empty() ? "" : ", ") + control_name(s, true);
        cs_.msg += fill(t[M_SWAPPED], names);
    }
}
}  // namespace

// orig: (none) — enhancement (controls.h)
void controls_set_host(ControlsHost* host) { cs_.host = host; }

// orig: (none) — enhancement (controls.h)
uint32_t controls_page_install(uint32_t parent) {
#if defined(__WIIU__)
    cs_.device = 0;
#else
    if (cs_.device < 0) cs_.device = cs_.host != nullptr && !cs_.host->pad_name().empty() ? 0 : 1;  // (a pad connected: the pad first)
#endif
    cs_.top = 0;
    cs_.prev_sel = 0;
    cs_.msg_row = -1;
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    ee_copy(HELP_WIDGET, OPTIONS_HELP, 0x50);
    sw(TITLE + 0x34, static_cast<uint32_t>(TEXT_FIRST + T_COUNT + I_TITLE));
    rows_write();
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    sw(ROWS_WIDGET + 0x40, 0);
    // the help label: the focused widget's +0x40 (the selection, mirrored) as the index into its texts (flag 0x80); its
    // box the Options page's help box (slot 1), the settings in page 0x1b6678's taller box (as graphics.cpp)
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
    return PAGE;
}

// orig: (none) — enhancement (controls.h)
uint32_t controls_page() { return PAGE; }

// orig: (none) — enhancement (controls.h)
uint32_t controls_text(int32_t id) {
    // (the ids the widgets are given: TEXT_FIRST + T_COUNT + I_*; T_* index the TEXTS tables only)
    const int32_t i = id - TEXT_FIRST - T_COUNT;
    if (i < 0 || i >= I_COUNT) return 0;
    std::string s;
    int kind = 0, slot = 0;  // (the string's kind: title, Options help, name, value, help; its slot)
    if (i == I_TITLE) {
        s = texts()[T_TITLE];
        kind = 0;
    } else if (i == I_OPTIONS_HELP) {
        s = texts()[T_OPTIONS_HELP];
        kind = 1;
    } else if (i < I_VALUE) {
        slot = i - I_NAME;
        s = row_name(cs_.top + slot);
        kind = 2;
    } else if (i < I_HELP) {
        slot = i - I_VALUE;
        s = row_value(cs_.top + slot);
        kind = 3;
    } else {
        s = help_text(cs_.top + lw(ROWS_WIDGET + 0x38));
        kind = 4;
    }
    // the slots: title, Options help, names, values, help
    uint32_t at = STRINGS;
    for (int i = 0; i < kind; ++i) at += STRING_SIZE[i] * (i >= 2 && i <= 3 ? VISIBLE : 1);
    at += STRING_SIZE[kind] * static_cast<uint32_t>(slot);
    ee_zero(at, STRING_SIZE[kind]);
    ee_copy_in(at, s.c_str(), std::min<size_t>(s.size(), STRING_SIZE[kind] - 1));
    return at;
}

// orig: (none) — enhancement (controls.h)
int32_t controls_title_text() { return TEXT_FIRST + T_COUNT + I_TITLE; }
// orig: (none) — enhancement (controls.h)
int32_t controls_help_text() { return TEXT_FIRST + T_COUNT + I_OPTIONS_HELP; }

// orig: (none) — enhancement (controls.h)
bool controls_setting_changed(uint32_t byte) {
    if (byte < STEPS || byte >= STEPS + VISIBLE) return false;
    sb(byte, 0);
    if (cs_.waiting_row < 0) row_step(cs_.top + static_cast<int>(byte - STEPS), 1);
    return true;
}

// orig: (none) — enhancement (controls.h)
bool controls_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (w != ROWS_WIDGET) return false;
    rebind_poll();
    int sel = lw(w + 0x38);
    // the window moved when up / down go past its ends (the widget stops there: its rows end)
    if ((pressed & 0x4000u) != 0 && cs_.prev_sel == VISIBLE - 1 && sel == VISIBLE - 1 && cs_.top + VISIBLE < ROW_COUNT) {
        ++cs_.top;
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    } else if ((pressed & 0x1000u) != 0 && cs_.prev_sel == 0 && sel == 0 && cs_.top > 0) {
        --cs_.top;
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    }
    sel = lw(w + 0x38);
    if (cs_.top + sel != cs_.msg_row && cs_.waiting_row < 0) cs_.msg_row = -1;  // (a message is about its row: gone when another is chosen)
    cs_.prev_sel = sel;
    sw(w + 0x40, static_cast<uint32_t>(sel));  // (the help label's index)
    if ((pressed & 0xa000u) == 0 || cs_.waiting_row >= 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b < STEPS || b >= STEPS + VISIBLE) return false;
    row_step(cs_.top + static_cast<int>(b - STEPS), (pressed & 0x8000u) != 0 ? -1 : 1);
    return true;
}

}  // namespace rac1
