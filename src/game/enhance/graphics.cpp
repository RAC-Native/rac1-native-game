// The Graphics page and the new game's mode popup (see graphics.h). Not a port: everything here is
// new, written on top of the original's menu data
// so that the pages look and behave like the game's own.
//
// The Graphics page is a copy of the Camera page (0x1b5a60), as the Advanced page: its title and
// its settings widget (0x27ef00 / 0x27f0a8: rows of a name, the byte it sets and up to 4 value
// texts, cross cycling the byte modulo their count). Our rows have more values than 4 (levels of
// detail, sizes, percents): each row's byte is a "step" byte with two values whose texts are both
// the row's value text, made here from the setting's real value; when the widget steps the byte,
// graphics_setting_changed steps the real value and puts the byte back to 0. Left / right step our
// rows back / forward (graphics_settings_dpad, a hook in the widget's update that only acts on our
// widgets). A help label (a copy of Options' help label 0x1b4b68, its box the Options page's help
// box) shows the selected row's help: the widget's selection +0x38 is mirrored at +0x40, which the
// label reads (its flag 0x80: the focused widget's +0x40). The rows are more than the box shows at
// once (the widget spreads them over its height): the widget is given a window of VISIBLE of them,
// moved when up / down go past its ends (as the Controls page, controls.cpp).
//
// The popup is a copy of the pause menu's Options page (0x1b4950: its title, list, help label and
// picture) with three entries, Original mode / Modern mode / Experimental, each with the original New Game entry's
// action (4) and page: picking one runs New Game as the original does, after the mode is applied
// (graphics_new_game_choice, a hook in the list's action 4). The main menu's New Game entry points
// at the popup (action 3).
//
// EE RAM block GFX (0xf2000..0xf8000), in the PS2 kernel's first megabyte as the Advanced block
// (0xf0000..0xf2000): nothing of the game or the runtime lives there.
#include <game/enhance/graphics.h>

#include <game/enhance/advanced.h>
#include <game/enhance/controls.h>
#include <game/enhance/update_notice.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>

#include <platform/platform.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t GFX = 0x000f2000u;
constexpr uint32_t VALUES = GFX;              // the settings' real values, one byte each (by Setting)
constexpr uint32_t STEPS = GFX + 0x20;        // the shown rows' step bytes (the settings widget writes them)
constexpr uint32_t POP_ENTRIES = GFX + 0x100;  // the popup's list entries (0xc each)
constexpr uint32_t POP_HELP = GFX + 0x140;    // its help label's texts
constexpr uint32_t POP_PICTURES = GFX + 0x160;  // its picture's images (sector, count)
constexpr uint32_t ROOT_ENTRY = GFX + 0x180;  // the main menu's New Game entry, pointed at the popup
constexpr uint32_t PAGE = GFX + 0x200;        // the Graphics page (a copy of Camera's)
constexpr uint32_t TITLE = GFX + 0x290;       // its title (a copy of Camera's label)
constexpr uint32_t ROWS_WIDGET = GFX + 0x2e0;  // its settings widget (a copy of Camera's)
constexpr uint32_t HELP_WIDGET = GFX + 0x330;  // its help label (a copy of Options')
constexpr uint32_t ROWS = GFX + 0x400;        // the settings widget's shown rows (0x18 bytes, ended by text 0)
constexpr uint32_t ROW_HELP = GFX + 0x540;    // the help label's texts, one per shown row (after the rows: VISIBLE x 0x18 and the end)
constexpr uint32_t POP_PAGE = GFX + 0x600;    // the popup's page (a copy of Options')
constexpr uint32_t POP_TITLE = GFX + 0x690;
constexpr uint32_t POP_LIST = GFX + 0x6e0;
constexpr uint32_t POP_LABEL = GFX + 0x730;
constexpr uint32_t POP_PICTURE = GFX + 0x780;
constexpr uint32_t MAIN_ENTRIES = GFX + 0x800;  // the main menu's Options entries, ours added
constexpr uint32_t STRINGS = GFX + 0x1000;    // the texts, made at each lookup (0xa0 each; the Controls page from 0xf6000)
constexpr uint32_t STRING_SIZE = 0xa0;
constexpr int VISIBLE = 10;  // the rows the box shows at once

// The game's data the pages are made from (Veldin's addresses, relocated: also in the boot module)
constexpr OvConst<0x001b4950u> OPTIONS_PAGE{};
constexpr OvConst<0x001b4a30u> OPTIONS_PICTURE{};
constexpr OvConst<0x001b4af8u> OPTIONS_LIST{};
constexpr OvConst<0x001b4b68u> OPTIONS_HELP{};
constexpr OvConst<0x001b5a60u> CAMERA_PAGE{};
constexpr OvConst<0x001b5ae8u> CAMERA_TITLE{};
constexpr OvConst<0x001b5ba0u> CAMERA_SETTINGS{};
constexpr OvConst<0x001b6678u> TALL_PAGE{};  // a page (state 0x1a) whose slot 3 box is tall enough for our rows
// The main menu (its pages are in every build; the boot module's, 0x1d45c8.., are the ones run: relocated)
constexpr OvConst<0x001b8798u> ROOT_NEW_GAME{};  // the root page's New Game button (a one-entry list)
constexpr OvConst<0x001b86c8u> ROOT_PAGE{};
constexpr int32_t TEXT_NEW_GAME = 0x4f2a;        // the game's "New Game"

// Our menu pictures (advanced.cpp makes them: sectors past the end of the disc)
constexpr uint32_t PICTURE_SECTOR = 0xadf00000u;
enum : uint32_t { PIC_ORIGINAL = 2, PIC_MODERN = 3, PIC_EXPERIMENTAL = 5 };  // (advanced.cpp's PIC_ ids)

// The settings
enum Setting : uint32_t { S_RENDERER, S_DISPLAY, S_MONITOR, S_WINDOW, S_SCALE, S_ASPECT, S_FIT, S_LOD, S_FOV, S_VSYNC, S_CAP, S_LIGHTING, S_DYN_SHADOWS, S_SEC_LIGHTS, S_SHADOW_DIST, S_AO, S_MIPS, S_SHADOW_STR, S_SUN_RAYS, S_COUNT };
constexpr float SHADOW_DISTS[] = {40.0f, 80.0f, 140.0f, 0.0f};  // Low, Medium, High, Maximum (0: as far as the cascades reach)
constexpr int SHADOW_STRS[] = {50, 60, 70, 80, 90, 100};  // the sun's shadows' strength, percent of the key light they hide
constexpr int SHADOW_STR_DEFAULT = 2;
constexpr int SCALES[] = {50, 75, 100, 125, 150, 175, 200};
constexpr int FOVS[] = {75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 130, 140, 150};
constexpr int LODS[] = {0, 25, 50, 75, 100};  // Original, Low, Medium, High, Maximum: the LOD slider (mrp_lod.h)
constexpr int CAPS[] = {30, 60, 120, 144, 240, 0};
struct Size { int w, h; };
// (from the original's own 640 x 480, for a CRT or a small screen: 4:3 and 16:9 sizes)
constexpr Size SIZES[] = {{640, 480}, {854, 480}, {800, 600}, {1024, 576}, {960, 720}, {1024, 768}, {1280, 720}, {1280, 960},
                          {1366, 768}, {1600, 900}, {1600, 1200}, {1920, 1080}, {2560, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}};
constexpr uint32_t SIZE_COUNT = sizeof(SIZES) / sizeof(Size);

// The texts. Ids from TEXT_FIRST (after the Advanced page's 0x7f00..); per language (0x15ed88: 0
// English, 2 French (in English for now), 3 German, 4 Spanish, 5 Italian)
constexpr int32_t TEXT_FIRST = 0x7f40;
enum : int32_t {
    T_GRAPHICS, T_OPTIONS_HELP,
    T_RENDERER, T_DISPLAY, T_MONITOR, T_WINDOW_SIZE, T_SCALE, T_ASPECT, T_FIT, T_LOD, T_FOV, T_VSYNC, T_CAP, T_LIGHTING, T_DYN_SHADOWS, T_SEC_LIGHTS, T_SHADOW_DIST, T_AO, T_MIPS, T_SHADOW_STR, T_SUN_RAYS,
    H_RENDERER, H_DISPLAY, H_MONITOR, H_WINDOW_SIZE, H_SCALE, H_ASPECT, H_FIT, H_LOD, H_FOV, H_VSYNC, H_CAP, H_LIGHTING, H_DYN_SHADOWS, H_SEC_LIGHTS, H_SHADOW_DIST, H_AO, H_MIPS, H_SHADOW_STR, H_SUN_RAYS,
    T_WINDOW, T_FULLSCREEN, T_43, T_169, T_FILL, T_BARS, T_STRETCH,
    T_ORIGINAL, T_LOW, T_MEDIUM, T_HIGH, T_MAXIMUM, T_OFF, T_ON, T_UNLIMITED, T_SEC_GAME, T_SEC_ALL, T_FULL,
    T_MODE_ORIGINAL, T_MODE_MODERN, T_MODE_EXPERIMENTAL, H_MODE_ORIGINAL, H_MODE_MODERN, H_MODE_EXPERIMENTAL,
    T_COUNT,
    V_FIRST = T_COUNT  // the shown rows' value texts (made from the values): V_FIRST + slot
};
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Graphics", "Display, picture shape, detail and view of the PC version",
     "Renderer", "Display", "Monitor", "Window Size", "Render Resolution", "Aspect Ratio", "Fit to Screen", "Level of Detail", "Field of View", "V-Sync", "Frame Rate Cap", "Lighting", "Dynamic Shadows", "Secondary Lights", "Shadow Distance", "Ambient Occlusion", "Texture Detail", "Shadow Strength", "Sun Rays",
     "The graphics API used to draw the game.", "In a window or on the whole screen.", "Which screen the window is put on.", "The window's size (in window mode).", "The 3D's resolution, times the screen's. The HUD stays sharp.",
     "4:3 as the original, 16:9, or the view widened to fill the screen.", "When the picture's shape is not the screen's: black bars, or stretched.",
     "How far objects keep their detail and stay drawn.", "x1.00 is the original's view.", "Waits for the screen's refresh: no tearing.",
     "The most frames drawn a second. The game itself always runs at 60.",
     "Real sun shadows, light per pixel. Original: as on the PS2.",
     "Shadows of explosions, weapons and lamps. Costs more.",
     "Explosions' and weapons' light; All: lamps too.",
     "How far characters and objects cast shadows.",
     "Soft darkening where surfaces meet. Costs more.",
     "Full: every texture level, smooth. Original (PS2): the GS's own mip choice.",
     "How dark the sun's shadows are. Lower: the shade lit by the sky.",
     "Shafts of sunlight through openings toward the sun. Costs a little.",
     "Window", "Fullscreen", "4:3 Original", "16:9", "Fill Screen", "Black Bars", "Stretch",
     "Original", "Low", "Medium", "High", "Maximum", "Off", "On", "Unlimited", "Game's", "All", "Full",
     "Original", "Modern", "Experimental",
     "As on the PS2: 4:3, original detail and controls. All can be changed in Options.",
     "Widescreen, more detail, modern aiming and movement. All can be changed in Options.",
     "Modern plus all the new lighting: shadows, lights, occlusion. Experimental, heavy."},
    {"Grafik", "Anzeige, Bildformat, Details und Sicht der PC-Version",
     "Renderer", "Anzeige", "Bildschirm", "Fenstergrosse", "Renderauflosung", "Seitenverhaltnis", "Bildanpassung", "Detailstufe", "Sichtfeld", "V-Sync", "Bildratenlimit", "Beleuchtung", "Dynamische Schatten", "Zusatzlichter", "Schattendistanz", "Umgebungsverdeckung", "Texturdetail", "Schattenstarke", "Sonnenstrahlen",
     "Die Grafik-API, mit der das Spiel gezeichnet wird.", "Im Fenster oder im Vollbild.", "Auf welchem Bildschirm das Fenster liegt.", "Die Grosse des Fensters (im Fenstermodus).", "Die Auflosung der 3D-Grafik, mal die des Bildschirms. Das HUD bleibt scharf.",
     "4:3 wie im Original, 16:9 oder auf den Bildschirm erweitert.", "Wenn das Bildformat nicht passt: schwarze Balken oder gestreckt.",
     "Wie weit Objekte ihre Details behalten und gezeichnet werden.", "x1.00 ist die Sicht des Originals.", "Wartet auf den Bildaufbau: kein Tearing.",
     "Hochstens so viele Bilder pro Sekunde. Das Spiel lauft immer mit 60.",
     "Echte Sonnenschatten, Licht pro Pixel. Original: wie PS2.",
     "Schatten von Explosionen, Waffen, Lampen. Aufwendiger.",
     "Licht von Explosionen und Waffen; Alle: auch Lampen.",
     "Wie weit Figuren und Objekte Schatten werfen.",
     "Weiche Abdunklung, wo Flachen sich treffen. Aufwendiger.",
     "Voll: alle Texturstufen, weich. Original (PS2): die Mip-Wahl der GS.",
     "Wie dunkel die Sonnenschatten sind. Weniger: vom Himmel erhellt.",
     "Lichtstrahlen der Sonne durch Offnungen. Etwas aufwendiger.",
     "Fenster", "Vollbild", "4:3 Original", "16:9", "Fullen", "Balken", "Strecken",
     "Original", "Niedrig", "Mittel", "Hoch", "Maximum", "Aus", "An", "Unbegrenzt", "Des Spiels", "Alle", "Voll",
     "Original", "Modern", "Experimentell",
     "Wie auf der PS2: 4:3, Original-Details und -Steuerung. Alles in Optionen anderbar.",
     "Breitbild, mehr Details, modernes Zielen und Bewegen. Alles in Optionen anderbar.",
     "Modern plus die neue Beleuchtung: Schatten, Lichter, Verdeckung. Experimentell, aufwendig."},
    {"Graficos", "Pantalla, formato, detalle y vista de la version PC",
     "Renderizador", "Pantalla", "Monitor", "Tamano de ventana", "Resolucion 3D", "Relacion de aspecto", "Ajuste", "Nivel de detalle", "Campo de vision", "V-Sync", "Limite de FPS", "Iluminacion", "Sombras dinamicas", "Luces secundarias", "Distancia de sombras", "Oclusion ambiental", "Detalle de texturas", "Intensidad de sombras", "Rayos de sol",
     "La API grafica con la que se dibuja el juego.", "En una ventana o a pantalla completa.", "En que pantalla se abre la ventana.", "El tamano de la ventana (en modo ventana).", "La resolucion del 3D, por la de la pantalla. El HUD sigue nitido.",
     "4:3 como el original, 16:9 o la vista ampliada a la pantalla.", "Si el formato no es el de la pantalla: bandas negras o estirado.",
     "Hasta donde los objetos conservan su detalle y se dibujan.", "x1.00 es la vista del original.", "Espera al refresco de la pantalla: sin cortes.",
     "Los cuadros por segundo como maximo. El juego siempre va a 60.",
     "Sombras reales del sol, luz por pixel. Original: como PS2.",
     "Sombras de explosiones, armas y lamparas. Mas pesado.",
     "Luz de explosiones y armas; Todas: tambien lamparas.",
     "Hasta donde personajes y objetos dan sombra.",
     "Sombreado suave donde las superficies se juntan. Mas pesado.",
     "Completo: todos los niveles, suave. Original (PS2): el mip de la GS.",
     "Lo oscuras que son las sombras del sol. Menos: luz del cielo.",
     "Haces de sol a traves de las aberturas. Algo mas pesado.",
     "Ventana", "Completa", "4:3 Original", "16:9", "Llenar", "Bandas", "Estirar",
     "Original", "Bajo", "Medio", "Alto", "Maximo", "No", "Si", "Sin limite", "Del juego", "Todas", "Completo",
     "Original", "Moderno", "Experimental",
     "Como en PS2: 4:3, detalle y controles originales. Todo cambia en Opciones.",
     "Panoramico, mas detalle, apuntado y movimiento modernos. Todo cambia en Opciones.",
     "Moderno y la nueva iluminacion al maximo: sombras, luces, oclusion. Experimental, pesado."},
    {"Grafica", "Schermo, formato, dettaglio e visuale della versione PC",
     "Renderer", "Schermo", "Monitor", "Finestra", "Risoluzione 3D", "Formato", "Adattamento", "Livello di dettaglio", "Campo visivo", "V-Sync", "Limite FPS", "Illuminazione", "Ombre dinamiche", "Luci secondarie", "Distanza ombre", "Occlusione ambientale", "Dettaglio texture", "Intensita ombre", "Raggi di sole",
     "L'API grafica con cui viene disegnato il gioco.", "In una finestra o a schermo intero.", "Su quale schermo va la finestra.", "La dimensione della finestra (in modalita finestra).", "La risoluzione del 3D, per quella dello schermo. L'HUD resta nitido.",
     "4:3 come l'originale, 16:9 o la visuale allargata allo schermo.", "Se il formato non e quello dello schermo: bande nere o stirato.",
     "Fin dove gli oggetti tengono il dettaglio e restano disegnati.", "x1.00 e la visuale dell'originale.", "Attende il refresh dello schermo: niente tearing.",
     "I fotogrammi al secondo al massimo. Il gioco va sempre a 60.",
     "Ombre reali del sole, luce per pixel. Originale: come PS2.",
     "Ombre di esplosioni, armi e lampade. Piu pesante.",
     "Luce di esplosioni e armi; Tutte: anche le lampade.",
     "Fin dove personaggi e oggetti fanno ombra.",
     "Ombreggiatura morbida dove le superfici si toccano. Piu pesante.",
     "Pieno: tutti i livelli, morbido. Originale (PS2): il mip della GS.",
     "Quanto sono scure le ombre del sole. Meno: luce del cielo.",
     "Fasci di luce del sole attraverso le aperture. Un po' piu pesante.",
     "Finestra", "Intero", "4:3 Originale", "16:9", "Riempi", "Bande nere", "Stira",
     "Originale", "Basso", "Medio", "Alto", "Massimo", "No", "Si", "Illimitato", "Del gioco", "Tutte", "Pieno",
     "Originale", "Moderno", "Sperimentale",
     "Come su PS2: 4:3, dettaglio e controlli originali. Tutto si cambia in Opzioni.",
     "Panoramico, piu dettaglio, mira e movimento moderni. Tutto si cambia in Opzioni.",
     "Moderno e la nuova illuminazione al massimo: ombre, luci, occlusione. Sperimentale, pesante."},
};

// The page's rows: the setting, its name, its help, the preferences' key
struct Row {
    Setting s;
    int32_t name, help;
};
// (grouped: the renderer and window, the picture, the view, the light)
constexpr Row ROWS_DEF[] = {
    {S_RENDERER, T_RENDERER, H_RENDERER},
    {S_DISPLAY, T_DISPLAY, H_DISPLAY},       {S_MONITOR, T_MONITOR, H_MONITOR},                {S_WINDOW, T_WINDOW_SIZE, H_WINDOW_SIZE},          {S_VSYNC, T_VSYNC, H_VSYNC},
    {S_CAP, T_CAP, H_CAP},                   {S_SCALE, T_SCALE, H_SCALE},                       {S_ASPECT, T_ASPECT, H_ASPECT},
    {S_FIT, T_FIT, H_FIT},                   {S_FOV, T_FOV, H_FOV},                             {S_LOD, T_LOD, H_LOD},
    {S_LIGHTING, T_LIGHTING, H_LIGHTING},    {S_SHADOW_STR, T_SHADOW_STR, H_SHADOW_STR},       {S_SHADOW_DIST, T_SHADOW_DIST, H_SHADOW_DIST},     {S_DYN_SHADOWS, T_DYN_SHADOWS, H_DYN_SHADOWS},
    {S_SEC_LIGHTS, T_SEC_LIGHTS, H_SEC_LIGHTS},   {S_AO, T_AO, H_AO},   {S_SUN_RAYS, T_SUN_RAYS, H_SUN_RAYS},
    {S_MIPS, T_MIPS, H_MIPS},
};
constexpr int ROW_COUNT = static_cast<int>(sizeof(ROWS_DEF) / sizeof(Row));
static_assert(T_COUNT + VISIBLE <= 0x60, "the Graphics page's text ids end where the Controls page's begin (0x7fa0)");
static_assert(0x1000 + (T_COUNT + VISIBLE) * STRING_SIZE <= 0x4000, "the Graphics page's texts end where the Controls page's block begins");

// The state the host reads (C++ side: the window is not the game's)
struct State {
    // (the defaults: Vulkan, window, 100 %, 4:3 with bars, original LOD, 100 %, v-sync, 60, original lighting, no dynamic shadows, all secondary lights)
    int value[S_COUNT] = {0, 0, 0, 0, 2, 0, 0, 0, 5, 1, 1, 0, 0, 2, 3, 0, 0, SHADOW_STR_DEFAULT, 0};  // (the monitor: the first of the host's list; shadow distance: Maximum; no ambient occlusion; full textures; shadow strength 70 %)
    std::vector<std::string> monitors;  // the host's monitors as it named them (graphics_set_monitors), GLFW's order
    std::string monitor_name;           // the monitor chosen, by name: what the preferences keep (the order is not stable)
    int top = 0, prev_sel = 0;  // the page's window: the first row shown; the selection the last frame
    int win_w = 960, win_h = 720;                      // the window mode's size
    bool host_fullscreen = false;
    uint32_t mode = GAME_MODE_NONE;
    GraphicsDisplay display;
    uint32_t serial = 0;
    bool loaded = false, dirty = false;
    int scale_override = -1;
    int mips_seen[2] = {1, 1};  // the mip globals as the settings left them: a change by the debug menu is taken into the row
} gs_;

// orig: (none) — enhancement: the number of values of a setting
uint32_t value_count(Setting s) {
    switch (s) {
        case S_RENDERER: case S_DISPLAY: case S_FIT: case S_VSYNC: case S_DYN_SHADOWS: case S_MIPS: return 2;
        case S_AO: return 6;  // Off, SSAO, SSAO+, HBAO, GTAO, GTAO 1/2
        case S_SEC_LIGHTS: return 3;
        case S_SHADOW_DIST: return 4;  // Off, the game's (its point lights per pixel), all (the automatic emitters too)
        case S_WINDOW: return SIZE_COUNT + 1;  // (the last: the current size, when not in the list)
        case S_SCALE: return sizeof(SCALES) / sizeof(int);
        case S_ASPECT: return 3;
        case S_LOD: return sizeof(LODS) / sizeof(int);
        case S_SHADOW_STR: return sizeof(SHADOW_STRS) / sizeof(int);
        case S_SUN_RAYS: return 3;  // Off, Low, High
        case S_LIGHTING: return 4;  // Original, Low, Medium, High (mrp_lighting.h: off, the maps at 1024 / 2048 / 4096)
        case S_FOV: return sizeof(FOVS) / sizeof(int);
        case S_CAP: return sizeof(CAPS) / sizeof(int);
        case S_MONITOR: return static_cast<uint32_t>(gs_.monitors.size());  // (the host's list; one before GLFW is up: the row shows '?')
        default: return 1;
    }
}

// orig: (none) — enhancement: the index of a window size in SIZES (SIZE_COUNT: not there)
uint32_t size_index(int w, int h) {
    for (uint32_t i = 0; i < SIZE_COUNT; ++i)
        if (SIZES[i].w == w && SIZES[i].h == h) return i;
    return SIZE_COUNT;
}

// orig: (none) — enhancement: the texture detail applied: Full (0) the MRP's every-mip trilinear
// sampling, Original (1) the GS's own mip choice (the debug menu's "Faithful mip"). Live: the renderers read
// the two globals at every draw. The testing variables (RAC1_MRPW_MIP / RAC1_MRP_MIP) keep their choice for
// their part. Also run by the host once the variables have been read (graphics_mips_apply, graphics.h).
void mips_apply() {
    if (std::getenv("RAC1_MRPW_MIP") == nullptr) g_mrp_world_mip = gs_.value[S_MIPS] != 0 ? 0 : 1;
    if (std::getenv("RAC1_MRP_MIP") == nullptr) g_mrp_moby_mip = gs_.value[S_MIPS] != 0 ? 0 : 1;
    gs_.mips_seen[0] = g_mrp_world_mip;
    gs_.mips_seen[1] = g_mrp_moby_mip;
}

// orig: (none) — enhancement: the settings applied to the renderer and the window's request
void apply() {
    const int* v = gs_.value;
    if (!g_advanced_disabled) {  // (the tests: the renderer's own defaults and switches, RAC1_VIEW_MODE / RAC1_MRP_LOD)
        g_view_mode = v[S_ASPECT] == 2 ? 2 : (v[S_FIT] == 0 ? 1 : 0);
        g_view_frame_aspect = v[S_ASPECT] == 1 ? 16.0f / 9.0f : 4.0f / 3.0f;
        g_view_fov_scale = static_cast<float>(FOVS[v[S_FOV]]) / 100.0f;
        g_mrp_lod = LODS[v[S_LOD]];
        // (the environment's choices kept: testing)
        if (std::getenv("RAC1_SHADOWS") == nullptr) {
            g_modern_shadows = v[S_LIGHTING] != 0 ? 1 : 0;
            if (v[S_LIGHTING] != 0) g_shadow_quality = v[S_LIGHTING] - 1;
        }
        if (std::getenv("RAC1_POINT_SHADOWS") == nullptr) g_point_shadows = v[S_DYN_SHADOWS];
        if (std::getenv("RAC1_POINTS") == nullptr) g_modern_points = v[S_SEC_LIGHTS] != 0 ? 1 : 0;
        if (std::getenv("RAC1_AUTO_LIGHTS") == nullptr) g_auto_lights = v[S_SEC_LIGHTS] == 2 ? 1 : 0;
        if (std::getenv("RAC1_SHADOW_MOBY_DIST") == nullptr) g_shadow_moby_dist = SHADOW_DISTS[v[S_SHADOW_DIST]];
        if (std::getenv("RAC1_AO") == nullptr) g_ao = v[S_AO];
        if (std::getenv("RAC1_SUN_RAYS") == nullptr) g_sun_rays = v[S_SUN_RAYS];
        if (std::getenv("RAC1_SHADOW_STRENGTH") == nullptr) g_shadow_strength = SHADOW_STRS[v[S_SHADOW_STR]];
        mips_apply();
    }
    GraphicsDisplay d;
    d.vulkan = !g_advanced_disabled && v[S_RENDERER] == 0;
    if (const char* env = std::getenv("RAC1_RENDERER")) {
        if (std::strcmp(env, "opengl") == 0 || std::strcmp(env, "gl") == 0 || std::strcmp(env, "0") == 0) d.vulkan = false;
        else if (std::strcmp(env, "vulkan") == 0 || std::strcmp(env, "vk") == 0 || std::strcmp(env, "1") == 0) d.vulkan = true;
    }
    d.fullscreen = v[S_DISPLAY] != 0;
    d.window_w = gs_.win_w;
    d.window_h = gs_.win_h;
    d.vsync = v[S_VSYNC] != 0;
    d.fps_cap = CAPS[v[S_CAP]];
    d.monitor = v[S_MONITOR];
    if (d.vulkan != gs_.display.vulkan || d.fullscreen != gs_.display.fullscreen || d.window_w != gs_.display.window_w || d.window_h != gs_.display.window_h ||
        d.vsync != gs_.display.vsync || d.fps_cap != gs_.display.fps_cap || d.monitor != gs_.display.monitor) {
        gs_.display = d;
        ++gs_.serial;
    }
}

// orig: (none) — enhancement: the real values written where the page shows them (EE)
void values_to_ee() {
    for (uint32_t i = 0; i < S_COUNT; ++i) sb(VALUES + i, static_cast<uint32_t>(gs_.value[i]));
}

// --- the preferences: $RAC1_GRAPHICS_FILE, else the Advanced settings' directory /graphics_options.txt
// orig: (none) — enhancement: the preferences file's path
std::filesystem::path prefs_path() {
    if (const char* f = std::getenv("RAC1_GRAPHICS_FILE")) return f;
    if (const std::filesystem::path d = platform::config_dir(); !d.empty()) return d / "graphics_options.txt";
    return "graphics_options.txt";
}

// orig: (none) — enhancement: a percent's index in a list (the nearest)
template <size_t N>
int nearest(const int (&list)[N], int v) {
    int best = 0;
    for (size_t i = 1; i < N; ++i)
        if (std::abs(list[i] - v) < std::abs(list[best] - v)) best = static_cast<int>(i);
    return best;
}

// orig: (none) — enhancement: the preferences read; false when there are none
bool prefs_read(const std::filesystem::path& path) {
    FILE* f = platform::open_file(path, "r");
    if (f == nullptr) return false;
    char key[64];
    while (std::fscanf(f, "%63s", key) == 1) {
        int a = 0, b = 0;
        if (std::strcmp(key, "renderer") == 0) {
            char name[64] = {};
            if (std::fscanf(f, "%63s", name) == 1) {
                if (std::strcmp(name, "opengl") == 0 || std::strcmp(name, "gl") == 0 || std::strcmp(name, "1") == 0)
                    gs_.value[S_RENDERER] = 1;
                else if (std::strcmp(name, "vulkan") == 0 || std::strcmp(name, "vk") == 0 || std::strcmp(name, "0") == 0)
                    gs_.value[S_RENDERER] = 0;
            }
            continue;
        }
        if (std::strcmp(key, "monitor") == 0) {  // (the name, not the index: GLFW's order is not stable)
            // (the rest of the line: a monitor's name can hold spaces, and an empty one must not take the next line's key)
            char name[256] = {};
            if (std::fgets(name, sizeof name, f) != nullptr) {
                std::string n = name;
                while (!n.empty() && (n.back() == '\n' || n.back() == '\r' || n.back() == ' ')) n.pop_back();
                const size_t start = n.find_first_not_of(' ');
                gs_.monitor_name = start == std::string::npos ? std::string() : n.substr(start);
            }
            continue;
        }
        if (std::strcmp(key, "window") == 0) {
            if (std::fscanf(f, "%d %d", &a, &b) == 2 && a >= 320 && b >= 240 && a <= 16384 && b <= 16384) {
                gs_.win_w = a;
                gs_.win_h = b;
            }
            continue;
        }
        if (std::fscanf(f, "%d", &a) != 1) break;
        if (std::strcmp(key, "display") == 0) gs_.value[S_DISPLAY] = a != 0;
        else if (std::strcmp(key, "render_scale") == 0) gs_.value[S_SCALE] = nearest(SCALES, a);
        else if (std::strcmp(key, "aspect") == 0 && a >= 0 && a < 3) gs_.value[S_ASPECT] = a;
        else if (std::strcmp(key, "fit") == 0) gs_.value[S_FIT] = a != 0;
        else if (std::strcmp(key, "lod") == 0 && a >= 0 && a < 5) gs_.value[S_LOD] = a;
        else if (std::strcmp(key, "fov") == 0) gs_.value[S_FOV] = nearest(FOVS, a);
        else if (std::strcmp(key, "vsync") == 0) gs_.value[S_VSYNC] = a != 0;
        else if (std::strcmp(key, "fps_cap") == 0) gs_.value[S_CAP] = nearest(CAPS, a);
        else if ((std::strcmp(key, "lighting") == 0 || std::strcmp(key, "shadows") == 0) && a >= 0 && a < 4) gs_.value[S_LIGHTING] = a;  // ("shadows": before)
        else if (std::strcmp(key, "dynamic_shadows") == 0) gs_.value[S_DYN_SHADOWS] = a != 0;
        else if (std::strcmp(key, "secondary_lights") == 0 && a >= 0 && a < 3) gs_.value[S_SEC_LIGHTS] = a;
        else if (std::strcmp(key, "shadow_distance") == 0 && a >= 0 && a < 4) gs_.value[S_SHADOW_DIST] = a;
        else if (std::strcmp(key, "ambient_occlusion") == 0 && a >= 0 && a < 6) gs_.value[S_AO] = a;
        else if (std::strcmp(key, "texture_detail") == 0) gs_.value[S_MIPS] = a != 0;  // (0 full, 1 original)
        else if (std::strcmp(key, "sun_rays") == 0 && a >= 0 && a < 3) gs_.value[S_SUN_RAYS] = a;
        else if (std::strcmp(key, "shadow_strength") == 0) gs_.value[S_SHADOW_STR] = nearest(SHADOW_STRS, a);
        // ("overscan", the Overscan row's until 0.4: read like any other key and ignored, the full frame always shown)
        else if (std::strcmp(key, "mode") == 0 && a >= 0 && a <= 3) gs_.mode = static_cast<uint32_t>(a);
    }
    std::fclose(f);
    return true;
}

// orig: (none) — enhancement: the debug menu's old display settings (devmenu_display.txt: view_mode,
// fov_scale, vsync, fps_cap, window, fullscreen) taken once, when there are no preferences yet
void prefs_migrate() {
    FILE* f = std::fopen("devmenu_display.txt", "r");
    if (f == nullptr) return;
    char key[32];
    while (std::fscanf(f, "%31s", key) == 1) {
        int a = 0, b = 0, c = 0, d = 0;
        float x = 0.0f;
        if (std::strcmp(key, "view_mode") == 0 && std::fscanf(f, "%d", &a) == 1) {
            gs_.value[S_ASPECT] = a == 2 ? 2 : 0;
            gs_.value[S_FIT] = a == 0 ? 1 : 0;
        } else if (std::strcmp(key, "fov_scale") == 0 && std::fscanf(f, "%f", &x) == 1) {
            gs_.value[S_FOV] = nearest(FOVS, static_cast<int>(x * 100.0f + 0.5f));
        } else if (std::strcmp(key, "vsync") == 0 && std::fscanf(f, "%d", &a) == 1) {
            gs_.value[S_VSYNC] = a != 0;
        } else if (std::strcmp(key, "fps_cap") == 0 && std::fscanf(f, "%d", &a) == 1) {
            gs_.value[S_CAP] = nearest(CAPS, a);
        } else if (std::strcmp(key, "window") == 0 && std::fscanf(f, "%d %d %d %d", &a, &b, &c, &d) == 4) {
            if (c >= 320 && d >= 240) {
                gs_.win_w = c;
                gs_.win_h = d;
            }
        } else if (std::strcmp(key, "fullscreen") == 0 && std::fscanf(f, "%d", &a) == 1) {
            gs_.value[S_DISPLAY] = a != 0;
        }
    }
    std::fclose(f);
    std::printf("[graphics] the display settings taken from devmenu_display.txt\n");
}

// orig: (none) — enhancement: the preferences written
void prefs_write() {
    const std::filesystem::path path = prefs_path();
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (FILE* f = platform::open_file(path, "w")) {
        const int* v = gs_.value;
        std::fprintf(f, "renderer %s\ndisplay %d\nmonitor %s\nwindow %d %d\nrender_scale %d\naspect %d\nfit %d\nlod %d\nfov %d\nvsync %d\nfps_cap %d\nlighting %d\ndynamic_shadows %d\nsecondary_lights %d\nshadow_distance %d\nambient_occlusion %d\ntexture_detail %d\nshadow_strength %d\nsun_rays %d\nmode %u\n",
                     v[S_RENDERER] != 0 ? "opengl" : "vulkan", v[S_DISPLAY], gs_.monitor_name.c_str(), gs_.win_w, gs_.win_h, SCALES[v[S_SCALE]], v[S_ASPECT], v[S_FIT], v[S_LOD], FOVS[v[S_FOV]], v[S_VSYNC], CAPS[v[S_CAP]], v[S_LIGHTING], v[S_DYN_SHADOWS],
                     v[S_SEC_LIGHTS], v[S_SHADOW_DIST], v[S_AO], v[S_MIPS], SHADOW_STRS[v[S_SHADOW_STR]], v[S_SUN_RAYS], gs_.mode);
        std::fclose(f);
    }
    gs_.dirty = false;
}

// orig: (none) — enhancement: a value changed by the page (or a mode, or a save): applied, the
// preferences to be written
void changed() {
    if (gs_.value[S_MONITOR] >= 0 && static_cast<size_t>(gs_.value[S_MONITOR]) < gs_.monitors.size()) gs_.monitor_name = gs_.monitors[gs_.value[S_MONITOR]];  // (the row's choice kept by name)
    apply();
    values_to_ee();
    gs_.dirty = true;
}

// orig: (none) — enhancement: the language's texts (French in English for now: the font's accented
// letters not looked at yet)
const char* const* texts() {
    switch (lwu(0x0015ed88)) {
        case 3: return TEXTS[1];
        case 4: return TEXTS[2];
        case 5: return TEXTS[3];
        default: return TEXTS[0];
    }
}

// orig: (none) — enhancement: a row's value as text
std::string value_text(Setting s) {
    const char* const* t = texts();
    const int v = gs_.value[s];
    char b[48];
    switch (s) {
        case S_RENDERER: return v != 0 ? "OpenGL" : "Vulkan";
        case S_DISPLAY: return t[v != 0 ? T_FULLSCREEN : T_WINDOW];
        case S_MONITOR: return v >= 0 && static_cast<size_t>(v) < gs_.monitors.size() ? gs_.monitors[v] : std::string("?");
        case S_WINDOW: std::snprintf(b, sizeof b, "%d x %d", gs_.win_w, gs_.win_h); return b;
        case S_SCALE: std::snprintf(b, sizeof b, "x%d.%02d", SCALES[v] / 100, SCALES[v] % 100); return b;  // (the font's '%' is a button)
        case S_ASPECT: return t[v == 0 ? T_43 : v == 1 ? T_169 : T_FILL];
        case S_FIT: return t[v != 0 ? T_STRETCH : T_BARS];
        case S_LOD: return t[T_ORIGINAL + v];
        case S_LIGHTING: return t[T_ORIGINAL + v];
        case S_DYN_SHADOWS: return t[v != 0 ? T_ON : T_OFF];
        case S_SEC_LIGHTS: return t[v == 0 ? T_OFF : v == 1 ? T_SEC_GAME : T_SEC_ALL];
        case S_SHADOW_DIST: return t[T_LOW + v];
        case S_SUN_RAYS: return t[v == 0 ? T_OFF : v == 1 ? T_LOW : T_HIGH];
        case S_SHADOW_STR: std::snprintf(b, sizeof b, "%d", SHADOW_STRS[v]); return b;  // (the font's '%' is a button)
        case S_AO: {
            static constexpr const char* AO_NAMES[] = {"SSAO", "SSAO+", "HBAO", "GTAO", "GTAO 1/2"};
            return v <= 0 ? t[T_OFF] : AO_NAMES[std::min(v, 5) - 1];
        }
        case S_MIPS: return v != 0 ? std::string(t[T_ORIGINAL]) + " (PS2)" : std::string(t[T_FULL]);
        case S_FOV: std::snprintf(b, sizeof b, "x%d.%02d", FOVS[v] / 100, FOVS[v] % 100); return b;
        case S_VSYNC: return t[v != 0 ? T_ON : T_OFF];
        case S_CAP:
            if (CAPS[v] == 0) return t[T_UNLIMITED];
            std::snprintf(b, sizeof b, "%d", CAPS[v]);
            return b;
        default: return "";
    }
}

// orig: (none) — enhancement: a setting stepped by d (+1 / -1), wrapping
void step(Setting s, int d) {
    if (s == S_WINDOW) {  // the sizes' list, the current size counted where it falls
        uint32_t i = size_index(gs_.win_w, gs_.win_h);
        if (i == SIZE_COUNT) {  // a size of its own: the next / previous of the list by area
            const long area = static_cast<long>(gs_.win_w) * gs_.win_h;
            i = 0;
            while (i < SIZE_COUNT && static_cast<long>(SIZES[i].w) * SIZES[i].h < area) ++i;
            if (d < 0) i = i == 0 ? SIZE_COUNT - 1 : i - 1;
            else if (i == SIZE_COUNT) i = 0;
        } else {
            i = static_cast<uint32_t>((static_cast<int>(i) + d + static_cast<int>(SIZE_COUNT)) % static_cast<int>(SIZE_COUNT));
        }
        gs_.win_w = SIZES[i].w;
        gs_.win_h = SIZES[i].h;
        gs_.value[S_DISPLAY] = 0;  // (a size: the window mode)
        changed();
        return;
    }
    const int n = static_cast<int>(value_count(s));
    if (n <= 0) return;  // (the monitor row before GLFW is up: nothing to step through)
    gs_.value[s] = (gs_.value[s] + d + n) % n;
    changed();
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

// orig: (none) — enhancement: the widget's rows for the window from gs_.top (their names and helps; the values by slot)
void rows_write() {
    uint32_t row = ROWS;
    const int shown = std::min(VISIBLE, ROW_COUNT);
    for (int i = 0; i < shown; ++i, row += 0x18) {
        const Row& r = ROWS_DEF[gs_.top + i];
        const uint32_t vt = static_cast<uint32_t>(TEXT_FIRST + V_FIRST + i);
        sw(row, static_cast<uint32_t>(TEXT_FIRST + r.name));
        sw(row + 4, STEPS + static_cast<uint32_t>(i));
        sw(row + 8, vt);  // (two values, both the value's text: cross steps the byte, graphics_setting_changed the setting)
        sw(row + 0xc, vt);
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sb(STEPS + static_cast<uint32_t>(i), 0);
        sw(ROW_HELP + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(TEXT_FIRST + r.help));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
}
}  // namespace

// orig: (none) — enhancement (graphics.h)
void graphics_load() {
    if (gs_.loaded) return;
    gs_.loaded = true;
    if (!g_advanced_disabled) {
        if (!prefs_read(prefs_path())) {
            prefs_migrate();
            gs_.dirty = true;
        }
    }
    apply();
    values_to_ee();
}

// orig: (none) — enhancement (graphics.h)
const GraphicsDisplay& graphics_display() {
    graphics_load();
    return gs_.display;
}
// orig: (none) — enhancement (graphics.h)
uint32_t graphics_display_serial() { return gs_.serial; }

// orig: (none) — enhancement (graphics.h)
void graphics_host_window(int w, int h, bool fullscreen) {
    gs_.host_fullscreen = fullscreen;
    if (fullscreen || w < 320 || h < 240) return;
    if (w == gs_.win_w && h == gs_.win_h) return;
    gs_.win_w = w;  // (resized by hand: the current size)
    gs_.win_h = h;
    gs_.display.window_w = w;
    gs_.display.window_h = h;
    if (!g_advanced_disabled) gs_.dirty = true;
}

// orig: (none) — enhancement (graphics.h)
float graphics_render_scale() {
    if (gs_.scale_override > 0) return static_cast<float>(gs_.scale_override) / 100.0f;
    return static_cast<float>(SCALES[gs_.value[S_SCALE]]) / 100.0f;
}
// orig: (none) — enhancement (graphics.h)
void graphics_set_render_scale_percent(int pct) { gs_.scale_override = std::clamp(pct, 25, 400); }

// orig: (none) — enhancement (graphics.h)
uint32_t graphics_page_install(uint32_t parent) {
    graphics_load();
    values_to_ee();
    ee_copy(PAGE, CAMERA_PAGE, 0x84);
    ee_copy(TITLE, CAMERA_TITLE, 0x50);
    ee_copy(ROWS_WIDGET, CAMERA_SETTINGS, 0x50);
    ee_copy(HELP_WIDGET, OPTIONS_HELP, 0x50);
    sw(TITLE + 0x34, TEXT_FIRST + T_GRAPHICS);
    gs_.top = 0;
    gs_.prev_sel = 0;
    rows_write();
    sw(ROWS_WIDGET + 0x34, ROWS);
    sw(ROWS_WIDGET + 0x38, 0);
    sw(ROWS_WIDGET + 0x40, 0);
    // the help label: the focused widget's +0x40 (the settings widget's selection, mirrored) as
    // the index into its texts (flag 0x80); its box is the Options page's help box (slot 1)
    sw(HELP_WIDGET + 0x30, 0x80);
    sw(HELP_WIDGET + 0x34, ROW_HELP);
    sw(PAGE + 0x38, parent);
    sw(PAGE + 0x40, ROWS_WIDGET);
    slot_replace(PAGE, CAMERA_TITLE, TITLE);
    slot_replace(PAGE, CAMERA_SETTINGS, ROWS_WIDGET);
    // the boxes (the menu mobys' animations): Camera's settings box holds 4 or 5 rows, ours are 10 shown: the
    // settings in the taller box of page 0x1b6678 (its slot 3), the help in the Options page's help box
    // (its slot 1), left of it (chosen by trying the game's boxes)
    sw(PAGE + 0xc, lwu(TALL_PAGE + 0xc));
    sw(PAGE + 4, lwu(OPTIONS_PAGE + 4));
    sw(PAGE + 0x44 + 4, HELP_WIDGET);
    sw(PAGE + 0x80, 0);
    return PAGE;
}

// orig: (none) — enhancement (graphics.h)
uint32_t graphics_text(int32_t id) {
    if (id < TEXT_FIRST || id >= TEXT_FIRST + V_FIRST + VISIBLE) return 0;
    const int32_t k = id - TEXT_FIRST;
    std::string s;
    if (k < V_FIRST) s = texts()[k];
    else if (gs_.top + (k - V_FIRST) < ROW_COUNT) s = value_text(ROWS_DEF[gs_.top + (k - V_FIRST)].s);
    char* p = static_cast<char*>(ee_ram_ptr(STRINGS + static_cast<uint32_t>(k) * STRING_SIZE));
    std::strncpy(p, s.c_str(), STRING_SIZE - 1);
    p[STRING_SIZE - 1] = 0;
    return STRINGS + static_cast<uint32_t>(k) * STRING_SIZE;
}

uint32_t updates_page();
uint32_t updates_root_widget();
void update_menu_main_install();

// orig: (none) — enhancement (graphics.h)
void graphics_set_monitors(std::vector<std::string> names) {
    gs_.monitors = std::move(names);
    // the chosen monitor's name to its place in the host's list; not among them (or none chosen): the
    // first, and its name kept, so the row shows what the window is on
    int index = -1;
    if (!gs_.monitor_name.empty())
        for (size_t i = 0; i < gs_.monitors.size(); ++i)
            if (gs_.monitors[i] == gs_.monitor_name) index = static_cast<int>(i);
    if (index < 0) {
        const std::string wanted = gs_.monitor_name;
        index = gs_.monitors.empty() ? -1 : 0;
        gs_.monitor_name = index >= 0 ? gs_.monitors[0] : std::string();
        if (!wanted.empty())
            std::printf("[graphics] the monitor '%s' is not connected: '%s' used\n", wanted.c_str(), gs_.monitor_name.c_str());
    }
    gs_.value[S_MONITOR] = index;
    if (index >= 0) apply();  // (the host's window: the choice read from the preferences is in it)
}

// orig: (none) — enhancement (graphics.h)
void graphics_set_vulkan(bool vulkan) {
    gs_.value[S_RENDERER] = vulkan ? 0 : 1;
    gs_.display.vulkan = vulkan;
    values_to_ee();
}

// orig: (none) — enhancement (graphics.h)
void graphics_menu_back(uint32_t cur, uint32_t to, uint32_t mobys) {
    if (cur == 0 || to == 0) return;
    // (the Updates button in slot 5 of the root page, when it is there: not with the enhancements off, where slot 5 is
    // the original's and the game's own code alone moves its moby)
    if (to == ROOT_PAGE && lwu(ROOT_PAGE + 0x44 + 5 * 4) == updates_root_widget() && lwu(mobys + 5 * 4) != 0) {
        const uint32_t m5 = lwu(mobys + 5 * 4);
        const uint32_t seq5 = lwu(lwu(m5 + 0x24) + 0x48 + 0x13 * 4);
        moby_set_anim(m5, 0x13, static_cast<int32_t>(lbu(seq5 + 0x10)) - 1);
        swc1(m5 + 0x58, -1.0f);
    }
    if (cur != PAGE && cur != POP_PAGE && cur != advanced_page() && cur != controls_page() && cur != update_notice_page() && cur != updates_page()) return;
    // `to`'s boxes as the game leaves them: a page entered forward ends on its animations' last frame; a root page
    // (no parent: the menu's first page) is opened by its animations played back to their first frame
    const bool root = lwu(to + 0x38) == 0;
    for (uint32_t i = 0; i < 0xe; ++i) {
        const uint32_t m = lwu(mobys + i * 4);
        if (m == 0) continue;
        const uint32_t set = lwu(to + i * 4);
        const uint32_t seq = lwu(lwu(m + 0x24) + 0x48 + set * 4);
        moby_set_anim(m, set, root ? 0 : static_cast<int32_t>(lbu(seq + 0x10)) - 1);
        swc1(m + 0x58, root ? -1.0f : 1.0f);  // (held there: already at the end it plays to)
    }
}

// orig: (none) — enhancement (graphics.h)
int32_t graphics_title_text() { return TEXT_FIRST + T_GRAPHICS; }
// orig: (none) — enhancement (graphics.h)
int32_t graphics_help_text() { return TEXT_FIRST + T_OPTIONS_HELP; }

// orig: (none) — enhancement (graphics.h)
bool graphics_setting_changed(uint32_t byte) {
    if (byte < STEPS || byte >= STEPS + static_cast<uint32_t>(VISIBLE)) return false;
    sb(byte, 0);
    const int r = gs_.top + static_cast<int>(byte - STEPS);
    if (r < ROW_COUNT) step(ROWS_DEF[r].s, 1);
    return true;
}

// orig: (none) — enhancement (graphics.h)
bool graphics_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    if (w != ROWS_WIDGET) return false;
    // the window moved when up / down go past its ends (the widget stops there: its rows end), as the Controls page
    int sel = lw(w + 0x38);
    if ((pressed & 0x4000u) != 0 && gs_.prev_sel == VISIBLE - 1 && sel == VISIBLE - 1 && gs_.top + VISIBLE < ROW_COUNT) {
        ++gs_.top;
        rows_write();
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    } else if ((pressed & 0x1000u) != 0 && gs_.prev_sel == 0 && sel == 0 && gs_.top > 0) {
        --gs_.top;
        rows_write();
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    }
    sel = lw(w + 0x38);
    gs_.prev_sel = sel;
    sw(w + 0x40, static_cast<uint32_t>(sel));  // (the help label's index)
    if ((pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4);
    if (b >= STEPS && b < STEPS + static_cast<uint32_t>(VISIBLE) && gs_.top + static_cast<int>(b - STEPS) < ROW_COUNT) {
        step(ROWS_DEF[gs_.top + static_cast<int>(b - STEPS)].s, (pressed & 0x8000u) != 0 ? -1 : 1);
        return true;
    }
    return false;
}

// orig: (none) — enhancement (graphics.h)
void graphics_mips_apply() {
    if (!g_advanced_disabled) mips_apply();
}

// orig: (none) — enhancement (graphics.h)
void graphics_frame() {
    if (g_advanced_disabled) return;
    // the debug menu's texture toggles: both set the same way, the row follows
    if ((g_mrp_world_mip != gs_.mips_seen[0] || g_mrp_moby_mip != gs_.mips_seen[1]) && g_mrp_world_mip == g_mrp_moby_mip) {
        gs_.value[S_MIPS] = g_mrp_world_mip != 0 ? 0 : 1;
        gs_.mips_seen[0] = g_mrp_world_mip;
        gs_.mips_seen[1] = g_mrp_moby_mip;
        values_to_ee();
        gs_.dirty = true;
    }
    if (gs_.dirty) prefs_write();
}

// orig: (none) — enhancement (graphics.h): the picture settings and the mode, ids 32.. (1 byte each;
// the window's own settings are the machine's: not in the saves)
void graphics_save_items(std::vector<uint8_t>& d, uint32_t& count) {
    const struct { uint16_t id; int value; } items[] = {
        {32, gs_.value[S_SCALE]}, {33, gs_.value[S_ASPECT]}, {34, gs_.value[S_FIT]}, {35, gs_.value[S_LOD]}, {36, gs_.value[S_FOV]}, {37, static_cast<int>(gs_.mode)},
        {38, gs_.value[S_LIGHTING]}, {39, gs_.value[S_DYN_SHADOWS]}, {40, gs_.value[S_SEC_LIGHTS]},
        {41, gs_.value[S_SHADOW_DIST]}, {42, gs_.value[S_AO]}, {43, gs_.value[S_MIPS]}, {45, gs_.value[S_SHADOW_STR]}, {46, gs_.value[S_SUN_RAYS]},
    };
    for (const auto& it : items) {
        d.push_back(static_cast<uint8_t>(it.id));
        d.push_back(static_cast<uint8_t>(it.id >> 8));
        d.push_back(1);
        d.push_back(0);
        d.push_back(static_cast<uint8_t>(it.value));
        ++count;
    }
}

// orig: (none) — enhancement (graphics.h)
bool graphics_save_item(uint32_t id, const uint8_t* data, uint32_t size) {
    if (id < 32 || id > 46) return false;
    if (size < 1) return true;
    const int v = data[0];
    switch (id) {
        case 32: if (v < static_cast<int>(value_count(S_SCALE))) gs_.value[S_SCALE] = v; break;
        case 33: if (v < 3) gs_.value[S_ASPECT] = v; break;
        case 34: if (v < 2) gs_.value[S_FIT] = v; break;
        case 35: if (v < 5) gs_.value[S_LOD] = v; break;
        case 36: if (v < static_cast<int>(value_count(S_FOV))) gs_.value[S_FOV] = v; break;
        case 37: if (v <= 3) gs_.mode = static_cast<uint32_t>(v); break;
        case 38: if (v < 4) gs_.value[S_LIGHTING] = v; break;
        case 39: if (v < 2) gs_.value[S_DYN_SHADOWS] = v; break;
        case 40: if (v < 3) gs_.value[S_SEC_LIGHTS] = v; break;
        case 41: if (v < 4) gs_.value[S_SHADOW_DIST] = v; break;
        case 42: if (v < 6) gs_.value[S_AO] = v; break;
        case 43: if (v < 2) gs_.value[S_MIPS] = v; break;
        case 44: return true;  // (the Overscan row's until 0.4: ignored, the full frame always shown)
        case 46: if (v < 3) gs_.value[S_SUN_RAYS] = v; break;
        case 45: if (v < static_cast<int>(value_count(S_SHADOW_STR))) gs_.value[S_SHADOW_STR] = v; break;
        default: break;
    }
    changed();
    return true;
}

// --- the main menu -------------------------------------------------------------------------------

// orig: (none) — enhancement (graphics.h)
void graphics_main_menu_install() {
    if (g_advanced_disabled) return;
    graphics_load();
    // the popup: the pause menu's Options page (its copy in the boot module), our list, help, pictures
    const uint32_t opt = OPTIONS_PAGE;
    ee_copy(POP_PAGE, opt, 0x84);
    const uint32_t title = lwu(opt + 0x44), label = lwu(opt + 0x48), picture = lwu(opt + 0x4c), list = lwu(opt + 0x50);
    ee_copy(POP_TITLE, title, 0x50);
    ee_copy(POP_LABEL, label, 0x50);
    ee_copy(POP_PICTURE, picture, 0x50);
    ee_copy(POP_LIST, list, 0x50);
    sw(POP_TITLE + 0x34, TEXT_NEW_GAME);
    const uint32_t ng = lwu(ROOT_NEW_GAME + 0x34);  // the root's New Game entry (action 4, its page)
    constexpr uint32_t MODES = 3;
    static constexpr int32_t NAMES[MODES] = {T_MODE_ORIGINAL, T_MODE_MODERN, T_MODE_EXPERIMENTAL};
    static constexpr int32_t HELPS[MODES] = {H_MODE_ORIGINAL, H_MODE_MODERN, H_MODE_EXPERIMENTAL};
    static constexpr uint32_t PICS[MODES] = {PIC_ORIGINAL, PIC_MODERN, PIC_EXPERIMENTAL};
    for (uint32_t i = 0; i < MODES; ++i) {
        const uint32_t e = POP_ENTRIES + i * 0xc;
        ee_copy(e, ng, 0xc);
        sh(e, static_cast<uint32_t>(TEXT_FIRST + NAMES[i]));
        sh(e + 0xa, 0);
        sw(POP_HELP + i * 4, static_cast<uint32_t>(TEXT_FIRST + HELPS[i]));
        sw(POP_PICTURES + i * 8, PICTURE_SECTOR + PICS[i]);
        sw(POP_PICTURES + i * 8 + 4, 1);
    }
    for (uint32_t i = 0; i < 0xc; i += 4) sw(POP_ENTRIES + MODES * 0xc + i, 0);
    sw(POP_HELP + MODES * 4, 0);
    sw(POP_LIST + 0x34, POP_ENTRIES);
    sw(POP_LIST + 0x40, 0);  // (Original selected first)
    sw(POP_LIST + 0x38, 0);
    sw(POP_LIST + 0x3c, 0);
    sw(POP_LABEL + 0x34, POP_HELP);
    sw(POP_PICTURE + 0x30, POP_PICTURES);
    sw(POP_PAGE + 0x38, ROOT_PAGE);
    sw(POP_PAGE + 0x40, POP_LIST);
    slot_replace(POP_PAGE, title, POP_TITLE);
    slot_replace(POP_PAGE, label, POP_LABEL);
    slot_replace(POP_PAGE, picture, POP_PICTURE);
    slot_replace(POP_PAGE, list, POP_LIST);
    sw(POP_PAGE + 5 * 4, 0x13);
    sw(POP_PAGE + 0x80, 0);
    // the root's New Game: our entry, action 3 to the popup (its list's entries copied)
    if (ng != ROOT_ENTRY) {
        ee_copy(ROOT_ENTRY, ng, 0x18);
        sh(ROOT_ENTRY + 2, 3);
        sw(ROOT_ENTRY + 4, POP_PAGE);
        sw(ROOT_NEW_GAME + 0x34, ROOT_ENTRY);
    }
    update_menu_main_install();
}

// orig: (none) — enhancement (graphics.h)
void graphics_main_options_entries(uint32_t w) {
    if (g_advanced_disabled) return;
    const uint32_t src = lwu(w + 0x34);
    if (src == 0 || src == MAIN_ENTRIES) return;
    uint32_t n = 0;
    while (n < 6 && lh(src + n * 0xc) != 0) ++n;
    ee_copy(MAIN_ENTRIES, src, n * 0xc);
    uint32_t d = MAIN_ENTRIES + n * 0xc;
    const uint32_t gp = PAGE, ap = advanced_page();
    const struct { int32_t text; uint32_t page; } ours[] = {{TEXT_FIRST + T_GRAPHICS, gp}, {controls_title_text(), controls_page()}, {advanced_title_text(), ap}};
    for (const auto& o : ours) {
        sh(d, static_cast<uint32_t>(o.text));
        sh(d + 2, 3);  // action 3: the page +4
        sw(d + 4, o.page);
        sw(d + 8, 0);
        d += 0xc;
    }
    for (uint32_t i = 0; i < 0xc; i += 4) sw(d + i, 0);
    sw(w + 0x34, MAIN_ENTRIES);
}

// orig: (none) — enhancement (graphics.h)
void graphics_new_game_choice(uint32_t w) {
    if (w != POP_LIST) return;
    const int32_t sel = lw(w + 0x40);
    const uint32_t mode = sel <= 0 ? GAME_MODE_ORIGINAL : sel == 1 ? GAME_MODE_MODERN : GAME_MODE_EXPERIMENTAL;
    gs_.mode = mode;
    // the modern lighting: off in Original and Modern (its defaults: State), every option at its top in Experimental
    // (lighting High, dynamic shadows on, secondary lights All, shadow distance Maximum, ambient occlusion on)
    const bool experimental = mode == GAME_MODE_EXPERIMENTAL;
    gs_.value[S_LIGHTING] = experimental ? static_cast<int>(value_count(S_LIGHTING)) - 1 : 0;
    gs_.value[S_DYN_SHADOWS] = experimental ? 1 : 0;
    gs_.value[S_SEC_LIGHTS] = static_cast<int>(value_count(S_SEC_LIGHTS)) - 1;  // (All: the default, also the top)
    gs_.value[S_SHADOW_DIST] = static_cast<int>(value_count(S_SHADOW_DIST)) - 1;  // (Maximum: the default, also the top)
    gs_.value[S_AO] = experimental ? 1 : 0;
    if (mode == GAME_MODE_ORIGINAL) {
        gs_.value[S_ASPECT] = 0;
        gs_.value[S_LOD] = 0;
        gs_.value[S_MIPS] = 1;  // (the original's mips, like its detail)
    } else {
        gs_.value[S_ASPECT] = 2;
        gs_.value[S_LOD] = 3;
        gs_.value[S_MIPS] = 0;
    }
    changed();
    advanced_apply_mode(mode);
    std::printf("[graphics] new game: %s mode\n", mode == GAME_MODE_ORIGINAL ? "Original" : mode == GAME_MODE_MODERN ? "Modern" : "Experimental");
}

}  // namespace rac1
