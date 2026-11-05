// The Display and Graphics pages and the new game's mode popup (see graphics.h). Not a port: everything here is
// new, written on top of the original's menu data
// so that the pages look and behave like the game's own.
//
// Two pages of the same make: Display (the window, the picture's shape, the view, the detail: the PS2's compromises
// removed) and Graphics (the switch Original / Enhanced, a preset, then every addition of ours: the modern lighting, the
// materials, grass, ambient particles). Graphics: Original draws the original's picture whatever the rows under it hold (they are
// hidden and kept); the one gate is graphics_enhanced(), through which apply() sets the renderer's switches.
//
// Each page is a copy of the Camera page (0x1b5a60), as the Advanced page: its title and
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
// New Game opens two windows, each a copy of the pause menu's Options page (0x1b4950: its title, list, help label and
// picture). The first, the mode: Original / Modern (the gameplay rows of the Advanced page, advanced_apply_mode), each
// entry going to the second window (action 3). The second, the graphics preset: Current / Original / Low / Medium / High
// (the Graphics page only; the Display page is never touched), each with the original New Game entry's action (4) and
// page: picking one runs New Game as the original does, after the mode and the preset are applied
// (graphics_new_game_choice, a hook in the list's action 4). The main menu's New Game entry points at the first window
// (action 3).
//
// EE RAM block GFX (0xf2000..0xf8000), in the PS2 kernel's first megabyte as the Advanced block
// (0xf0000..0xf2000): nothing of the game or the runtime lives there.
#include <game/enhance/graphics.h>

#include <game/enhance/advanced.h>
#include <game/enhance/controls.h>
#include <game/enhance/language.h>
#include <game/enhance/update_notice.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_particles.h>
#include <game/render/mrp_reflect.h>
#include <game/render/mrp_rt.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>

#include <platform/platform.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace rac1 {

namespace {
constexpr uint32_t GFX = 0x000f2000u;
constexpr uint32_t VALUES = GFX;              // the settings' real values, one byte each (by Setting)
constexpr uint32_t STEPS = GFX + 0x20;        // the Graphics page's shown rows' step bytes (the settings widget writes them)
constexpr uint32_t DSTEPS = GFX + 0x30;       // the Display page's
constexpr uint32_t POP_ENTRIES = GFX + 0x100;  // the popup's list entries (0xc each)
constexpr uint32_t POP_HELP = GFX + 0x140;    // its help label's texts
constexpr uint32_t POP_PICTURES = GFX + 0x160;  // its picture's images (sector, count)
constexpr uint32_t ROOT_ENTRY = GFX + 0x180;  // the main menu's New Game entry, pointed at the popup
constexpr uint32_t NG_ENTRY = GFX + 0x1a0;    // the game's own New Game entry, kept (..0x1b8)
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
constexpr uint32_t MAIN_ENTRIES = GFX + 0x800;  // the main menu's Options entries, ours added (..0x884)
constexpr uint32_t DPAGE = GFX + 0x900;       // the Display page (a copy of Camera's), as the Graphics page's above
constexpr uint32_t DTITLE = GFX + 0x990;
constexpr uint32_t DROWS_WIDGET = GFX + 0x9e0;
constexpr uint32_t DHELP_WIDGET = GFX + 0xa30;
constexpr uint32_t DROWS = GFX + 0xb00;
constexpr uint32_t DROW_HELP = GFX + 0xc40;
constexpr uint32_t POP2_PAGE = GFX + 0xd00;   // the New Game's second window, the graphics preset (a copy of the popup's page)
constexpr uint32_t POP2_TITLE = GFX + 0xd90;
constexpr uint32_t POP2_LIST = GFX + 0xde0;
constexpr uint32_t POP2_LABEL = GFX + 0xe30;
constexpr uint32_t POP2_PICTURE = GFX + 0xe80;
// (the picture widget keeps state past the 0x50 bytes copied: up to +0x80 - entries right after it were overwritten)
constexpr uint32_t POP2_ENTRIES = GFX + 0xf00;  // its list entries (5 and the end, 0xc each)
constexpr uint32_t POP2_HELP = GFX + 0xf50;     // its help label's texts
constexpr uint32_t POP2_PICTURES = GFX + 0xf70;  // its picture's images (..0xf98)
constexpr uint32_t STRINGS = GFX + 0x1000;    // the texts, made at each lookup (0xa0 each; the Controls page from 0xf6000)
constexpr uint32_t STRING_SIZE = 0x80;  // (0xa0 until the Water, Foliage and Grass rows: the longest text is under 0x60)
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
enum : uint32_t { PIC_GFX = 1, PIC_ORIGINAL = 2, PIC_MODERN = 3 };  // (advanced.cpp's PIC_ ids)

// The settings
// (S_GFX: Graphics, 0 Original, 1 Enhanced; S_PRESET: no value of its own, the rows' match with PRESETS; S_LIGHTING: 0 Low,
// 1 Medium, 2 High - until the switch, 0 was Original and 1..3 Low..High)
enum Setting : uint32_t { S_DISPLAY, S_MONITOR, S_WINDOW, S_SCALE, S_ASPECT, S_FIT, S_LOD, S_FOV, S_VSYNC, S_CAP, S_LIGHTING, S_DYN_SHADOWS, S_SEC_LIGHTS, S_SHADOW_DIST, S_AO, S_MIPS, S_SHADOW_STR, S_SUN_RAYS, S_AO_STR, S_HUD, S_RENDERER, S_SURFACE, S_PARTICLES, S_GRASS, S_WREFL, S_GFX, S_PRESET, S_COUNT };
static_assert(S_COUNT <= 0x20, "the settings' values end where the step bytes begin");
constexpr float SHADOW_DISTS[] = {40.0f, 80.0f, 140.0f, 0.0f};  // Low, Medium, High, Maximum (0: as far as the cascades reach)
constexpr int SHADOW_STRS[] = {50, 60, 70, 80, 90, 100};  // the sun's shadows' strength, percent of the key light they hide
constexpr int SHADOW_STR_DEFAULT = 2;
constexpr int AO_STRS[] = {25, 50, 75, 100, 125};  // the ambient occlusion's intensity, percent of each mode's own strength
constexpr int AO_STR_DEFAULT = 2;
constexpr int SCALES[] = {50, 75, 100, 125, 150, 175, 200};
constexpr int FOVS[] = {75, 80, 85, 90, 95, 100, 105, 110, 115, 120, 130, 140, 150};
constexpr int LODS[] = {0, 25, 50, 75, 100};  // Original, Low, Medium, High, Maximum: the LOD slider (mrp_lod.h)
constexpr int CAPS[] = {30, 60, 120, 144, 240, 0};
// the HUD's widths (render_view.h g_hud_aspect): 4:3 (0, the original), 16:9, 21:9 (64:27, an ultrawide 2560 x 1080's), 32:9, the
// view's whole width (Fit Screen: any value wider than a view)
constexpr float HUD_ASPECTS[] = {0.0f, 16.0f / 9.0f, 64.0f / 27.0f, 32.0f / 9.0f, 1.0e6f};
struct Size { int w, h; };
// (from the original's own 640 x 480, for a CRT or a small screen: 4:3 and 16:9 sizes)
constexpr Size SIZES[] = {{640, 480}, {854, 480}, {800, 600}, {1024, 576}, {960, 720}, {1024, 768}, {1280, 720}, {1280, 960},
                          {1366, 768}, {1600, 900}, {1600, 1200}, {1920, 1080}, {2560, 1080}, {2560, 1440}, {3440, 1440}, {3840, 2160}};
constexpr uint32_t SIZE_COUNT = sizeof(SIZES) / sizeof(Size);

// The presets: the rows under Graphics they set (by index into each row's values), Low, Medium, High. Chosen for their cost at
// 5120 x 1440 (docs/settings.md#presets): Low the sun's shadows at 1024 with cheap materials, no occlusion or rays; Medium the maps
// at 2048, all the lights, GTAO at half resolution, Low rays; High the maps at 4096, dynamic shadows, MXAO, Low rays (High's were
// too strong: Daino, 2026-11-02 - stronger by hand), High materials, grass and particles. Water Reflections: Low the cube map, Medium
// and High the planar view (its cost: docs/materials.md#water-reflections)
constexpr Setting PRESET_ROWS[] = {S_LIGHTING, S_SHADOW_STR, S_SHADOW_DIST, S_DYN_SHADOWS, S_SEC_LIGHTS, S_AO, S_AO_STR, S_SUN_RAYS, S_SURFACE, S_GRASS, S_PARTICLES, S_WREFL};
constexpr int PRESET_COUNT = 3;  // (and Custom, 3: the rows match none)
constexpr int PRESETS[PRESET_COUNT][sizeof(PRESET_ROWS) / sizeof(Setting)] = {
    // light  strength  distance       dynamic  secondary   AO           AO int.  rays     surface  grass  particles  reflections
    {0,       2,        1,  /* 80 */   0,       1,          0,           2,       0,       1,       0,     1,         0},  // Low
    {1,       2,        2,  /* 140 */  0,       2,          5,  /* GTAO 1/2 */ 2, 1,     1,       1,     2,         1},  // Medium
    {2,       2,        3,  /* max */  1,       2,          9,  /* MXAO */     2, 1,     2,       2,     2,         1},  // High
};
constexpr int PRESET_DEFAULT = 2;  // (the rows' values before any choice: High, what Graphics: Enhanced first shows)

// The texts. Ids from TEXT_FIRST (after the Advanced page's 0x7f00..); per language (0x15ed88: 0
// English, 2 French (in English for now), 3 German, 4 Spanish, 5 Italian)
constexpr int32_t TEXT_FIRST = 0x7f40;
enum : int32_t {
    T_GRAPHICS, T_OPTIONS_HELP, T_DISPLAY_PAGE, H_DISPLAY_PAGE,
    T_DISPLAY, T_MONITOR, T_WINDOW_SIZE, T_SCALE, T_ASPECT, T_FIT, T_LOD, T_FOV, T_VSYNC, T_CAP, T_LIGHTING, T_DYN_SHADOWS, T_SEC_LIGHTS, T_SHADOW_DIST, T_AO, T_MIPS, T_SHADOW_STR, T_SUN_RAYS, T_AO_STR, T_HUD, T_RENDERER, T_SURFACE, T_PARTICLES, T_GRASS, T_WREFL, T_PRESET,
    H_DISPLAY, H_MONITOR, H_WINDOW_SIZE, H_SCALE, H_ASPECT, H_FIT, H_LOD, H_FOV, H_VSYNC, H_CAP, H_LIGHTING, H_DYN_SHADOWS, H_SEC_LIGHTS, H_SHADOW_DIST, H_AO, H_MIPS, H_SHADOW_STR, H_SUN_RAYS, H_AO_STR, H_HUD, H_RENDERER, H_SURFACE, H_PARTICLES, H_GRASS, H_WREFL, H_GFX, H_PRESET,
    T_WINDOW, T_FULLSCREEN, T_43, T_169, T_FILL, T_BARS, T_STRETCH, T_219, T_329, T_HUD_FIT,
    T_ORIGINAL, T_LOW, T_MEDIUM, T_HIGH, T_MAXIMUM, T_OFF, T_ON, T_UNLIMITED, T_SEC_GAME, T_SEC_ALL, T_FULL, T_ENHANCED, T_CUSTOM,
    T_MODE_ORIGINAL, T_MODE_MODERN, H_MODE_ORIGINAL, H_MODE_MODERN,
    T_NG_CURRENT, T_NG_ORIGINAL, T_NG_LOW, T_NG_MEDIUM, T_NG_HIGH, H_NG_CURRENT, H_NG_ORIGINAL, H_NG_LOW, H_NG_MEDIUM, H_NG_HIGH,
    T_COUNT
};
// The rows' value texts (made from the values at each lookup): VALUE_FIRST + page * VISIBLE + slot (0x7e80..: free ids between
// the Updates page's 0x7e10.. and the Advanced page's 0x7f00..)
constexpr int32_t VALUE_FIRST = 0x7e80;
constexpr const char* TEXTS[4][T_COUNT] = {
    {"Graphics", "The original picture, or our lighting, water and effects", "Display", "Window, picture shape, detail and view of the PC version",
     "Display Mode", "Monitor", "Window Size", "Render Resolution", "Aspect Ratio", "Fit to Screen", "Level of Detail", "Field of View", "V-Sync", "Frame Rate Cap", "Lighting", "Dynamic Shadows", "Secondary Lights", "Shadow Distance", "Ambient Occlusion", "Texture Detail", "Shadow Strength", "Sun Rays", "AO Intensity", "HUD", "Renderer", "Surface Detail", "Ambient Particles", "Grass", "Water Reflections", "Preset",
     "In a window or on the whole screen.", "Which screen the window is put on.", "The window's size (in window mode).", "The 3D's resolution, times the screen's. The HUD stays sharp.",
     "4:3 as the original, 16:9, or the view widened to fill the screen.", "When the picture's shape is not the screen's: black bars, or stretched.",
     "How far objects keep their detail and stay drawn.", "x1.00 is the original's view.", "Waits for the screen's refresh: no tearing.",
     "The most frames a second. The game makes 60: more shows nothing new.",
     "Real sun shadows, light per pixel: the shadow maps' size.",
     "Shadows of explosions, weapons and lamps. Costs more.",
     "Explosions' and weapons' light; All: lamps too.",
     "How far characters and objects cast shadows.",
     "Soft darkening where surfaces meet. Costs more.",
     "Full: every texture level, smooth. Original (PS2): the GS's own mip choice.",
     "How dark the sun's shadows are. Lower: the shade lit by the sky.",
     "Shafts of sunlight through openings. Higher: finer, costs more.",
     "How dark the ambient occlusion is. Characters get less of it.",
     "How wide the HUD spreads in a widened view. 4:3: as the original.",
     "The graphics API, from the next start. If Vulkan cannot start: OpenGL, and why.",
     "Shiny metal, wet ground, water and lava with the new lighting. High: reflections.",
     "Dust, embers, fireflies, rain and snow of our own. Not in the PS2 game.",
     "Blades of grass on the marked ground near the camera. Costs more.",
     "What the water mirrors. Low: sky, shores. Medium: everything. RT: ray traced (High: Ratchet too).",
     "Original: the PS2's picture. Enhanced: our lighting, water and effects below.",
     "Sets every row below at once. Changing one of them shows Custom.",
     "Window", "Fullscreen", "4:3 Original", "16:9", "Fill Screen", "Black Bars", "Stretch", "21:9", "32:9", "Fit Screen",
     "Original", "Low", "Medium", "High", "Maximum", "Off", "On", "Unlimited", "Game's", "All", "Full", "Enhanced", "Custom",
     "Original", "Modern",
     "As on the PS2: the original controls, cutscenes and Vendor. Changed in the pause menu.",
     "Quick select pause, modern aiming, RAC 3 movement, Buy All Ammo, cutscenes skipped.",
     "Current", "Original", "Low", "Medium", "High",
     "The Graphics page as it is now (Options > Graphics).",
     "The PS2's picture: Graphics Original.",
     "Our lighting and effects at the preset Low: the lightest.",
     "Our lighting and effects at the preset Medium.",
     "Our lighting and effects at the preset High: the heaviest."},
    {"Grafik", "Das Originalbild oder unsere Beleuchtung, Wasser und Effekte", "Anzeige", "Fenster, Bildformat, Details und Sicht der PC-Version",
     "Anzeigemodus", "Bildschirm", "Fenstergrosse", "Renderauflosung", "Seitenverhaltnis", "Bildanpassung", "Detailstufe", "Sichtfeld", "V-Sync", "Bildratenlimit", "Beleuchtung", "Dynamische Schatten", "Zusatzlichter", "Schattendistanz", "Umgebungsverdeckung", "Texturdetail", "Schattenstarke", "Sonnenstrahlen", "Verdeckungsstarke", "HUD", "Renderer", "Oberflachendetail", "Umgebungspartikel", "Gras", "Wasserspiegelung", "Voreinstellung",
     "Im Fenster oder im Vollbild.", "Auf welchem Bildschirm das Fenster liegt.", "Die Grosse des Fensters (im Fenstermodus).", "Die Auflosung der 3D-Grafik, mal die des Bildschirms. Das HUD bleibt scharf.",
     "4:3 wie im Original, 16:9 oder auf den Bildschirm erweitert.", "Wenn das Bildformat nicht passt: schwarze Balken oder gestreckt.",
     "Wie weit Objekte ihre Details behalten und gezeichnet werden.", "x1.00 ist die Sicht des Originals.", "Wartet auf den Bildaufbau: kein Tearing.",
     "Hochstens so viele Bilder pro Sekunde. Das Spiel macht 60: mehr zeigt nichts Neues.",
     "Echte Sonnenschatten, Licht pro Pixel: Grosse der Schattenkarten.",
     "Schatten von Explosionen, Waffen, Lampen. Aufwendiger.",
     "Licht von Explosionen und Waffen; Alle: auch Lampen.",
     "Wie weit Figuren und Objekte Schatten werfen.",
     "Weiche Abdunklung, wo Flachen sich treffen. Aufwendiger.",
     "Voll: alle Texturstufen, weich. Original (PS2): die Mip-Wahl der GS.",
     "Wie dunkel die Sonnenschatten sind. Weniger: vom Himmel erhellt.",
     "Lichtstrahlen der Sonne durch Offnungen. Hoher: feiner, aufwendiger.",
     "Wie dunkel die Umgebungsverdeckung ist. Figuren bekommen weniger.",
     "Wie breit das HUD in einer erweiterten Sicht ist. 4:3: wie im Original.",
     "Die Grafik-API, ab dem nachsten Start. Startet Vulkan nicht: OpenGL, mit Grund.",
     "Glanzendes Metall, nasser Boden, Wasser und Lava mit der neuen Beleuchtung. Hoch: Spiegelungen.",
     "Eigener Staub, Glut, Gluhwurmchen, Regen und Schnee. Nicht im PS2-Spiel.",
     "Grashalme auf dem markierten Boden nahe der Kamera. Aufwendiger.",
     "Was das Wasser spiegelt. Niedrig: Himmel, Ufer. Mittel: alles. RT: Raytracing (Hoch: mit Figuren).",
     "Original: das Bild der PS2. Verbessert: unsere Beleuchtung, Wasser und Effekte.",
     "Setzt alle Zeilen darunter auf einmal. Eine Anderung zeigt Eigene.",
     "Fenster", "Vollbild", "4:3 Original", "16:9", "Fullen", "Balken", "Strecken", "21:9", "32:9", "Ganze Breite",
     "Original", "Niedrig", "Mittel", "Hoch", "Maximum", "Aus", "An", "Unbegrenzt", "Des Spiels", "Alle", "Voll", "Verbessert", "Eigene",
     "Original", "Modern",
     "Wie auf der PS2: Original-Steuerung, -Szenen und -Handler. Im Pausemenu anderbar.",
     "Schnellwahl-Pause, modernes Zielen, RAC-3-Bewegung, alle Munition, Szenen ubersprungen.",
     "Aktuell", "Original", "Niedrig", "Mittel", "Hoch",
     "Die Grafik-Seite, wie sie jetzt ist (Optionen > Grafik).",
     "Das Bild der PS2: Grafik Original.",
     "Unsere Beleuchtung und Effekte, Voreinstellung Niedrig: die leichteste.",
     "Unsere Beleuchtung und Effekte, Voreinstellung Mittel.",
     "Unsere Beleuchtung und Effekte, Voreinstellung Hoch: die aufwendigste."},
    {"Graficos", "La imagen original, o nuestra iluminacion, agua y efectos", "Pantalla", "Ventana, formato, detalle y vista de la version PC",
     "Modo de pantalla", "Monitor", "Tamano de ventana", "Resolucion 3D", "Relacion de aspecto", "Ajuste", "Nivel de detalle", "Campo de vision", "V-Sync", "Limite de FPS", "Iluminacion", "Sombras dinamicas", "Luces secundarias", "Distancia de sombras", "Oclusion ambiental", "Detalle de texturas", "Intensidad de sombras", "Rayos de sol", "Intensidad de oclusion", "HUD", "Renderizador", "Detalle de superficies", "Particulas ambientales", "Hierba", "Reflejos del agua", "Preajuste",
     "En una ventana o a pantalla completa.", "En que pantalla se abre la ventana.", "El tamano de la ventana (en modo ventana).", "La resolucion del 3D, por la de la pantalla. El HUD sigue nitido.",
     "4:3 como el original, 16:9 o la vista ampliada a la pantalla.", "Si el formato no es el de la pantalla: bandas negras o estirado.",
     "Hasta donde los objetos conservan su detalle y se dibujan.", "x1.00 es la vista del original.", "Espera al refresco de la pantalla: sin cortes.",
     "Los cuadros por segundo como maximo. El juego hace 60: mas no muestra nada nuevo.",
     "Sombras reales del sol, luz por pixel: tamano de los mapas de sombras.",
     "Sombras de explosiones, armas y lamparas. Mas pesado.",
     "Luz de explosiones y armas; Todas: tambien lamparas.",
     "Hasta donde personajes y objetos dan sombra.",
     "Sombreado suave donde las superficies se juntan. Mas pesado.",
     "Completo: todos los niveles, suave. Original (PS2): el mip de la GS.",
     "Lo oscuras que son las sombras del sol. Menos: luz del cielo.",
     "Haces de sol a traves de las aberturas. Mas alto: mas fino, mas pesado.",
     "Lo oscura que es la oclusion ambiental. Los personajes, menos.",
     "Lo ancho que se extiende el HUD en una vista ampliada. 4:3: como el original.",
     "La API grafica, desde el siguiente inicio. Si Vulkan no arranca: OpenGL, y por que.",
     "Metal brillante, suelo mojado, agua y lava con la nueva iluminacion. Alto: reflejos.",
     "Polvo, brasas, luciernagas, lluvia y nieve propios. No en el juego de PS2.",
     "Briznas de hierba en el suelo marcado cerca de la camara. Mas pesado.",
     "Lo que refleja el agua. Bajo: cielo, orillas. Medio: todo. RT: trazado de rayos (Alto: personajes).",
     "Original: la imagen de PS2. Mejorado: nuestra iluminacion, agua y efectos.",
     "Ajusta todas las filas de abajo a la vez. Cambiar una muestra Personal.",
     "Ventana", "Completa", "4:3 Original", "16:9", "Llenar", "Bandas", "Estirar", "21:9", "32:9", "Todo el ancho",
     "Original", "Bajo", "Medio", "Alto", "Maximo", "No", "Si", "Sin limite", "Del juego", "Todas", "Completo", "Mejorado", "Personal",
     "Original", "Moderno",
     "Como en PS2: controles, escenas y Vendedor originales. Cambia en el menu de pausa.",
     "Pausa en seleccion, apuntado moderno, movimiento RAC 3, toda la municion, saltar escenas.",
     "Actual", "Original", "Bajo", "Medio", "Alto",
     "La pagina Graficos tal como esta ahora (Opciones > Graficos).",
     "La imagen de PS2: Graficos Original.",
     "Nuestra iluminacion y efectos, preajuste Bajo: el mas ligero.",
     "Nuestra iluminacion y efectos, preajuste Medio.",
     "Nuestra iluminacion y efectos, preajuste Alto: el mas pesado."},
    {"Grafica", "L'immagine originale, o la nostra illuminazione, acqua ed effetti", "Schermo", "Finestra, formato, dettaglio e visuale della versione PC",
     "Modalita schermo", "Monitor", "Finestra", "Risoluzione 3D", "Formato", "Adattamento", "Livello di dettaglio", "Campo visivo", "V-Sync", "Limite FPS", "Illuminazione", "Ombre dinamiche", "Luci secondarie", "Distanza ombre", "Occlusione ambientale", "Dettaglio texture", "Intensita ombre", "Raggi di sole", "Intensita occlusione", "HUD", "Renderer", "Dettaglio superfici", "Particelle ambientali", "Erba", "Riflessi dell'acqua", "Preset",
     "In una finestra o a schermo intero.", "Su quale schermo va la finestra.", "La dimensione della finestra (in modalita finestra).", "La risoluzione del 3D, per quella dello schermo. L'HUD resta nitido.",
     "4:3 come l'originale, 16:9 o la visuale allargata allo schermo.", "Se il formato non e quello dello schermo: bande nere o stirato.",
     "Fin dove gli oggetti tengono il dettaglio e restano disegnati.", "x1.00 e la visuale dell'originale.", "Attende il refresh dello schermo: niente tearing.",
     "I fotogrammi al secondo al massimo. Il gioco ne fa 60: di piu non mostra nulla di nuovo.",
     "Ombre reali del sole, luce per pixel: dimensione delle mappe d'ombra.",
     "Ombre di esplosioni, armi e lampade. Piu pesante.",
     "Luce di esplosioni e armi; Tutte: anche le lampade.",
     "Fin dove personaggi e oggetti fanno ombra.",
     "Ombreggiatura morbida dove le superfici si toccano. Piu pesante.",
     "Pieno: tutti i livelli, morbido. Originale (PS2): il mip della GS.",
     "Quanto sono scure le ombre del sole. Meno: luce del cielo.",
     "Fasci di luce del sole dalle aperture. Piu alto: piu fine, piu pesante.",
     "Quanto e scura l'occlusione ambientale. I personaggi ne hanno meno.",
     "Quanto si allarga l'HUD in una visuale allargata. 4:3: come l'originale.",
     "L'API grafica, dal prossimo avvio. Se Vulkan non parte: OpenGL, e il perche.",
     "Metallo lucido, terreno bagnato, acqua e lava con la nuova illuminazione. Alto: riflessi.",
     "Polvere, braci, lucciole, pioggia e neve nostre. Non nel gioco PS2.",
     "Fili d'erba sul terreno marcato vicino alla camera. Piu pesante.",
     "Cosa riflette l'acqua. Basso: cielo, rive. Medio: tutto. RT: ray tracing (Alto: anche Ratchet).",
     "Originale: l'immagine della PS2. Migliorata: la nostra illuminazione, acqua, effetti.",
     "Imposta tutte le righe sotto in una volta. Cambiarne una mostra Personale.",
     "Finestra", "Intero", "4:3 Originale", "16:9", "Riempi", "Bande nere", "Stira", "21:9", "32:9", "Tutta la larghezza",
     "Originale", "Basso", "Medio", "Alto", "Massimo", "No", "Si", "Illimitato", "Del gioco", "Tutte", "Pieno", "Migliorata", "Personale",
     "Originale", "Moderno",
     "Come su PS2: controlli, filmati e Venditore originali. Si cambia nel menu di pausa.",
     "Pausa nella scelta rapida, mira moderna, movimento RAC 3, tutte le munizioni, salta filmati.",
     "Attuale", "Originale", "Basso", "Medio", "Alto",
     "La pagina Grafica com'e ora (Opzioni > Grafica).",
     "L'immagine della PS2: Grafica Originale.",
     "La nostra illuminazione ed effetti, preset Basso: il piu leggero.",
     "La nostra illuminazione ed effetti, preset Medio.",
     "La nostra illuminazione ed effetti, preset Alto: il piu pesante."},
};

// Water Reflections' ray-traced values (value_text's words, not texts asked for by id - no id moved): Medium (RT), High (RT), per language
// as TEXTS ("(RT)" kept in every one)
#if !defined(__APPLE__)
// The Renderer row's help when the renderer running is not the row's choice (the value then reads "Vulkan (OpenGL)"): why,
// by RendererWhy (kNone: the row changed since the start); the font has no accented letters
constexpr const char* RENDERER_WHY[4][10] = {
    {"The renderer chosen runs from the next start.", "OpenGL now: this build has no Vulkan shaders. Run the setup again.",
     "OpenGL now: no Vulkan driver is installed.", "OpenGL now: the graphics driver has no Vulkan 1.3.", "OpenGL now: no GPU can show Vulkan's picture.",
     "OpenGL now: the GPU lacks features Vulkan needs (the log lists them).", "OpenGL now: Vulkan could not start (the log says why).",
     "OpenGL now: the last Vulkan start crashed. Choose Vulkan again to retry.", "OpenGL now: the GPU stopped in the last Vulkan run. Choose Vulkan again to retry.",
     "Chosen for this run by RAC1_RENDERER."},
    {"Die Wahl gilt ab dem nachsten Start.", "Jetzt OpenGL: dieser Build hat keine Vulkan-Shader. Setup erneut ausfuhren.",
     "Jetzt OpenGL: kein Vulkan-Treiber installiert.", "Jetzt OpenGL: der Grafiktreiber hat kein Vulkan 1.3.", "Jetzt OpenGL: keine GPU kann das Vulkan-Bild zeigen.",
     "Jetzt OpenGL: der GPU fehlen Funktionen fur Vulkan (siehe Log).", "Jetzt OpenGL: Vulkan konnte nicht starten (siehe Log).",
     "Jetzt OpenGL: der letzte Vulkan-Start ist abgesturzt. Vulkan erneut wahlen.", "Jetzt OpenGL: die GPU stoppte im letzten Vulkan-Lauf. Vulkan erneut wahlen.",
     "Fur diesen Lauf von RAC1_RENDERER gewahlt."},
    {"Lo elegido se aplica al siguiente inicio.", "Ahora OpenGL: esta version no tiene shaders de Vulkan. Ejecuta el setup otra vez.",
     "Ahora OpenGL: no hay driver de Vulkan instalado.", "Ahora OpenGL: el driver grafico no tiene Vulkan 1.3.", "Ahora OpenGL: ninguna GPU puede mostrar la imagen de Vulkan.",
     "Ahora OpenGL: a la GPU le faltan funciones de Vulkan (ver el log).", "Ahora OpenGL: Vulkan no pudo arrancar (ver el log).",
     "Ahora OpenGL: el ultimo inicio con Vulkan fallo. Elige Vulkan otra vez.", "Ahora OpenGL: la GPU se detuvo con Vulkan la ultima vez. Elige Vulkan otra vez.",
     "Elegido para esta ejecucion por RAC1_RENDERER."},
    {"La scelta vale dal prossimo avvio.", "Ora OpenGL: questa build non ha gli shader Vulkan. Rifai il setup.",
     "Ora OpenGL: nessun driver Vulkan installato.", "Ora OpenGL: il driver grafico non ha Vulkan 1.3.", "Ora OpenGL: nessuna GPU puo mostrare l'immagine Vulkan.",
     "Ora OpenGL: alla GPU mancano funzioni di Vulkan (vedi il log).", "Ora OpenGL: Vulkan non e partito (vedi il log).",
     "Ora OpenGL: l'ultimo avvio con Vulkan e fallito. Riscegli Vulkan per riprovare.", "Ora OpenGL: la GPU si e fermata con Vulkan l'ultima volta. Riscegli Vulkan.",
     "Scelto per questa esecuzione da RAC1_RENDERER."},
};
#endif
constexpr const char* RT_WORDS[4][2] = {{"Medium (RT)", "High (RT)"}, {"Mittel (RT)", "Hoch (RT)"}, {"Medio (RT)", "Alto (RT)"}, {"Medio (RT)", "Alto (RT)"}};

// The page's rows: the setting, its name, its help, the preferences' key
struct Row {
    Setting s;
    int32_t name, help;
};
// The Display page (0): the renderer, the window, the picture, the view, the detail
constexpr Row DISPLAY_ROWS[] = {
    {S_RENDERER, T_RENDERER, H_RENDERER},
    {S_DISPLAY, T_DISPLAY, H_DISPLAY},       {S_MONITOR, T_MONITOR, H_MONITOR},                {S_WINDOW, T_WINDOW_SIZE, H_WINDOW_SIZE},          {S_VSYNC, T_VSYNC, H_VSYNC},
    {S_CAP, T_CAP, H_CAP},                   {S_SCALE, T_SCALE, H_SCALE},                       {S_ASPECT, T_ASPECT, H_ASPECT},
    {S_FIT, T_FIT, H_FIT},                   {S_HUD, T_HUD, H_HUD},                             {S_FOV, T_FOV, H_FOV},                             {S_LOD, T_LOD, H_LOD},
    {S_MIPS, T_MIPS, H_MIPS},
};
// The Graphics page (1): the switch, the preset, then the additions (the light, the surfaces, the particles). With Graphics:
// Original only the switch is shown
constexpr Row GRAPHICS_ROWS[] = {
    {S_GFX, T_GRAPHICS, H_GFX},              {S_PRESET, T_PRESET, H_PRESET},
    {S_LIGHTING, T_LIGHTING, H_LIGHTING},    {S_SHADOW_STR, T_SHADOW_STR, H_SHADOW_STR},       {S_SHADOW_DIST, T_SHADOW_DIST, H_SHADOW_DIST},     {S_DYN_SHADOWS, T_DYN_SHADOWS, H_DYN_SHADOWS},
    {S_SEC_LIGHTS, T_SEC_LIGHTS, H_SEC_LIGHTS},   {S_AO, T_AO, H_AO},   {S_AO_STR, T_AO_STR, H_AO_STR},   {S_SUN_RAYS, T_SUN_RAYS, H_SUN_RAYS},
    {S_SURFACE, T_SURFACE, H_SURFACE},
    {S_WREFL, T_WREFL, H_WREFL},
    {S_GRASS, T_GRASS, H_GRASS},
    {S_PARTICLES, T_PARTICLES, H_PARTICLES},
};
enum : int { P_DISPLAY, P_GRAPHICS, PAGE_COUNT };
// A page's EE blocks and rows
struct PageDef {
    uint32_t page, title, rows_widget, help_widget, rows, row_help, steps;
    int32_t title_text;
    const Row* def;
    int count;
};
constexpr PageDef PAGES[PAGE_COUNT] = {
    {DPAGE, DTITLE, DROWS_WIDGET, DHELP_WIDGET, DROWS, DROW_HELP, DSTEPS, T_DISPLAY_PAGE, DISPLAY_ROWS, static_cast<int>(sizeof(DISPLAY_ROWS) / sizeof(Row))},
    {PAGE, TITLE, ROWS_WIDGET, HELP_WIDGET, ROWS, ROW_HELP, STEPS, T_GRAPHICS, GRAPHICS_ROWS, static_cast<int>(sizeof(GRAPHICS_ROWS) / sizeof(Row))},
};
static_assert(T_COUNT <= 0x60, "the Graphics page's text ids end where the Controls page's begin (0x7fa0)");
static_assert(VALUE_FIRST + PAGE_COUNT * VISIBLE <= 0x7f00, "the value texts' ids end where the Advanced page's begin");
static_assert(DROWS + (VISIBLE + 1) * 0x18 <= DROW_HELP && ROWS + (VISIBLE + 1) * 0x18 <= ROW_HELP, "a page's rows end before its help texts");
// orig: (none) — enhancement: every text fits its string with its end
constexpr bool texts_fit() {
    for (const auto& lang : TEXTS)
        for (const char* t : lang) {
            uint32_t n = 0;
            while (t[n] != 0) ++n;
            if (n >= STRING_SIZE) return false;
        }
    return true;
}
static_assert(texts_fit(), "a Graphics page text is longer than its string");
// the value words (T_WINDOW..T_CUSTOM) are never asked for by id (value_text puts them in the rows' value texts): no string of
// their own; the ids after them take their strings' places, and the value texts come after all of them
constexpr int32_t WORDS = T_CUSTOM - T_WINDOW + 1;
constexpr int32_t string_slot(int32_t k) { return k > T_CUSTOM ? k - WORDS : k; }
constexpr int32_t VALUE_SLOT = T_COUNT - WORDS;  // (the value texts' first string)
static_assert(0x1000 + (VALUE_SLOT + PAGE_COUNT * VISIBLE) * STRING_SIZE <= 0x4000, "the Graphics page's texts end where the Controls page's block begins");

// The state the host reads (C++ side: the window is not the game's)
struct State {
    // (the defaults: window, 100 %, 4:3 with bars, original LOD, 100 %, v-sync, 60, Graphics: Original with the rows under it
    // at the preset High; the monitor: the first of the host's list; full textures)
    int value[S_COUNT] = {};
    State() {
        value[S_SCALE] = 2;
        value[S_FOV] = 5;
        value[S_VSYNC] = 1;
        value[S_CAP] = 1;
        for (size_t i = 0; i < sizeof(PRESET_ROWS) / sizeof(Setting); ++i) value[PRESET_ROWS[i]] = PRESETS[PRESET_DEFAULT][i];
    }
    std::vector<std::string> monitors;  // the host's monitors as it named them (graphics_set_monitors), GLFW's order
    std::string monitor_name;           // the monitor chosen, by name: what the preferences keep (the order is not stable)
    int top[PAGE_COUNT] = {}, prev_sel[PAGE_COUNT] = {};  // each page's window: the first row shown; the selection the last frame
    int shown[PAGE_COUNT][32] = {};  // each page's rows shown (indexes into its PageDef's rows), and their count
    int shown_count[PAGE_COUNT] = {};
    // the switch as it acts now (graphics_enhanced): the setting, or the debug menu's live choice; the renderer's live switches
    // kept aside while the debug menu holds Original
    bool live_enh = false;
    bool stash_ok = false;
    bool installed = false;  // (the pages built: their rows follow the switch)
    int stash[8] = {};
    int win_w = 960, win_h = 720;                      // the window mode's size
    bool host_fullscreen = false;
    GraphicsDisplay display;
    uint32_t serial = 0;
    bool loaded = false, dirty = false;
    int out_w = 0, out_h = 0;  // the window's size, or the screen's in fullscreen (graphics_host_window; 0: not known yet)
    int scale_override = -1;
    int mips_seen[2] = {1, 1};  // the mip globals as the settings left them: a change by the debug menu is taken into the row
    // the renderer running (graphics_set_renderer_running; run_known false until the host says), why it is not the row's, the
    // row's value it started from (another value chosen since: "until restart"), the host's hook for a new choice
    bool run_known = false, run_vulkan = false;
    RendererWhy run_why = RendererWhy::kNone;
    int run_choice = 0, renderer_seen = 0;  // (renderer_seen: the row's value at the last change, for the hook)
    void (*chosen_hook)() = nullptr;
    // the Renderer row never chosen (`renderer auto`: Vulkan in a window, OpenGL headless; the row shows the renderer running),
    // and the row chosen by the player (`renderer_set 1`: a file without it, from 0.5.0..0.7.x whose `renderer opengl` was
    // the default then, is read as auto once). Never in State()'s value: S_RENDERER's EE byte stays 0 (the RAM hashes)
    bool renderer_auto = false, renderer_set = false;
} gs_;

// orig: (none) — enhancement: the number of values of a setting
uint32_t value_count(Setting s) {
    switch (s) {
        case S_DISPLAY: case S_FIT: case S_VSYNC: case S_DYN_SHADOWS: case S_MIPS: case S_RENDERER: case S_GFX: return 2;
        case S_PRESET: return PRESET_COUNT;  // (Custom is shown, never chosen)
        case S_AO: return 11;  // Off, SSAO, SSAO+, HBAO, GTAO, GTAO 1/2, SSAO 1/2, SSAO+ 1/2, HBAO 1/2, MXAO, MXAO 1/2 (the new ones after the old: the saves' values kept)
        case S_SEC_LIGHTS: return 3;
        case S_SHADOW_DIST: return 4;  // Off, the game's (its point lights per pixel), all (the automatic emitters too)
        case S_WINDOW: return SIZE_COUNT + 1;  // (the last: the current size, when not in the list)
        case S_SCALE: return sizeof(SCALES) / sizeof(int);
        case S_ASPECT: return 3;
        case S_HUD: return 5;  // 4:3 (the original), 16:9, 21:9, 32:9, the view's width
        case S_LOD: return sizeof(LODS) / sizeof(int);
        case S_SHADOW_STR: return sizeof(SHADOW_STRS) / sizeof(int);
        case S_SUN_RAYS: return 4;  // Off, Low, Medium, High (quality; the same look)
        case S_SURFACE: return 3;   // Off, Low, High (the materials, mrp_lighting.h g_surface_detail)
        case S_PARTICLES: return 3;  // Off, Low, High (mrp_particles.h)
        case S_GRASS: return 3;  // Off, Low, High (mrp_lighting.h g_grass)
        case S_WREFL: return 4;  // Low, Medium, Medium (RT), High (RT) (mrp_reflect.h g_water_reflections: 1..4; the RT ones where the renderer can trace)
        case S_AO_STR: return sizeof(AO_STRS) / sizeof(int);
        case S_LIGHTING: return 3;  // Low, Medium, High (mrp_lighting.h: the maps at 1024 / 2048 / 4096)
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

// orig: (none) — enhancement: a value's index in a list (the nearest)
template <typename T, size_t N>
int nearest(const T (&list)[N], T v) {
    int best = 0;
    for (size_t i = 1; i < N; ++i)
        if (std::abs(list[i] - v) < std::abs(list[best] - v)) best = static_cast<int>(i);
    return best;
}

constexpr size_t PRESET_ROW_COUNT = sizeof(PRESET_ROWS) / sizeof(Setting);
// orig: (none) — enhancement: the preset the rows `v` match (PRESET_COUNT: none of them, Custom)
int preset_of(const int* v) {
    for (int p = 0; p < PRESET_COUNT; ++p) {
        bool same = true;
        for (size_t i = 0; i < PRESET_ROW_COUNT && same; ++i) same = v[PRESET_ROWS[i]] == PRESETS[p][i];
        if (same) return p;
    }
    return PRESET_COUNT;
}
// orig: (none) — enhancement: a preset's values written into the rows `v`
void preset_set(int* v, int p) {
    for (size_t i = 0; i < PRESET_ROW_COUNT; ++i) v[PRESET_ROWS[i]] = PRESETS[p][i];
}

// The renderer's switches of the additions (each one's Off is the original's picture)
struct Additions {
    int modern, dyn, ao, rays, surface, particles, grass, refl;
};
// orig: (none) — enhancement: the renderer's switches as they are now
Additions additions_now() { return {g_modern_shadows, g_point_shadows, g_ao, g_sun_rays, g_surface_detail, g_ambient_particles, g_grass, g_water_reflections}; }
// orig: (none) — enhancement: the renderer's switches set
void additions_set(const Additions& a) {
    g_modern_shadows = a.modern;
    g_point_shadows = a.dyn;
    g_ao = a.ao;
    g_sun_rays = a.rays;
    g_surface_detail = a.surface;
    g_ambient_particles = a.particles;
    g_grass = a.grass;
    g_water_reflections = a.refl;
}

// orig: (none) — enhancement: THE GATE. The renderer's switches of the additions set from the rows `v`, every one off unless `on`
// (Graphics: Original draws the original's picture whatever the rows hold; their parameters - the maps' size, the lights' choice,
// the strengths, the shadows' reach - act only with the modern lighting on). `env`: the testing variables keep their choice
void additions_apply(const int* v, bool on, bool env) {
    auto ours = [env](const char* name) { return !env || std::getenv(name) == nullptr; };
    if (ours("RAC1_SHADOWS")) {
        g_modern_shadows = on ? 1 : 0;
        g_shadow_quality = v[S_LIGHTING];
    }
    if (ours("RAC1_POINT_SHADOWS")) g_point_shadows = on ? v[S_DYN_SHADOWS] : 0;
    if (ours("RAC1_POINTS")) g_modern_points = v[S_SEC_LIGHTS] != 0 ? 1 : 0;
    if (ours("RAC1_AUTO_LIGHTS")) g_auto_lights = v[S_SEC_LIGHTS] == 2 ? 1 : 0;
    if (ours("RAC1_SHADOW_MOBY_DIST")) g_shadow_moby_dist = SHADOW_DISTS[v[S_SHADOW_DIST]];
    if (ours("RAC1_AO")) g_ao = on ? v[S_AO] : 0;
    if (ours("RAC1_SUN_RAYS")) g_sun_rays = on ? v[S_SUN_RAYS] : 0;
    if (ours("RAC1_SURFACE_DETAIL")) g_surface_detail = on ? v[S_SURFACE] : 0;
    if (ours("RAC1_PARTICLES")) g_ambient_particles = on ? v[S_PARTICLES] : 0;
    if (ours("RAC1_GRASS")) g_grass = on ? v[S_GRASS] : 0;
    if (ours("RAC1_WATER_REFLECTIONS")) g_water_reflections = on ? v[S_WREFL] + 1 : 0;
    if (ours("RAC1_SHADOW_STRENGTH")) g_shadow_strength = SHADOW_STRS[v[S_SHADOW_STR]];
    if (ours("RAC1_AO_INTENSITY")) g_ao_intensity = AO_STRS[v[S_AO_STR]];
    gs_.live_enh = on;
    gs_.stash_ok = false;
}

// orig: (none) — enhancement: the rows under the switch `v` taken from the renderer's live values (the switches `a`)
void rows_from_live(int* v, const Additions& a) {
    v[S_LIGHTING] = std::clamp(g_shadow_quality, 0, 2);
    v[S_DYN_SHADOWS] = a.dyn != 0 ? 1 : 0;
    v[S_SEC_LIGHTS] = g_modern_points == 0 ? 0 : g_auto_lights != 0 ? 2 : 1;
    const float dist = g_shadow_moby_dist;  // (SHADOW_DISTS: 40, 80, 140, 0 the cascades' reach)
    v[S_SHADOW_DIST] = dist <= 0.0f ? 3 : dist < 60.0f ? 0 : dist < 110.0f ? 1 : 2;
    v[S_AO] = std::clamp(a.ao, 0, static_cast<int>(value_count(S_AO)) - 1);
    v[S_SUN_RAYS] = std::clamp(a.rays, 0, 3);
    v[S_SURFACE] = std::clamp(a.surface, 0, 2);
    v[S_PARTICLES] = std::clamp(a.particles, 0, 2);
    v[S_GRASS] = std::clamp(a.grass, 0, 2);
    v[S_WREFL] = std::clamp(a.refl - 1, 0, 3);
    v[S_SHADOW_STR] = nearest(SHADOW_STRS, g_shadow_strength);
    v[S_AO_STR] = nearest(AO_STRS, g_ao_intensity);
}

// orig: (none) — enhancement: the settings applied to the renderer and the window's request
void apply() {
    const int* v = gs_.value;
    if (!g_advanced_disabled) {  // (the tests: the renderer's own defaults and switches, RAC1_VIEW_MODE / RAC1_MRP_LOD)
        g_view_mode = v[S_ASPECT] == 2 ? 2 : (v[S_FIT] == 0 ? 1 : 0);
        g_view_frame_aspect = v[S_ASPECT] == 1 ? 16.0f / 9.0f : 4.0f / 3.0f;
        g_view_fov_scale = static_cast<float>(FOVS[v[S_FOV]]) / 100.0f;
        if (std::getenv("RAC1_HUD") == nullptr) g_hud_aspect = HUD_ASPECTS[v[S_HUD]];
        g_mrp_lod = LODS[v[S_LOD]];
        additions_apply(v, v[S_GFX] != 0, true);  // (the Graphics page: through the switch)
        mips_apply();
    }
    GraphicsDisplay d;
    // (the Renderer row: read by the host once, when it makes its window - a change takes effect at the next start;
    // auto is Vulkan in a window only; a headless run keeps OpenGL unless RAC1_RENDERER asks, `auto` included)
    d.vulkan = !g_advanced_disabled && !gs_.renderer_auto && v[S_RENDERER] != 0;
    d.vulkan_auto = !g_advanced_disabled && gs_.renderer_auto;
    if (const char* env = std::getenv("RAC1_RENDERER")) {
        d.vulkan_auto = std::strcmp(env, "auto") == 0;
        d.vulkan = d.vulkan_auto || std::strcmp(env, "vulkan") == 0 || std::strcmp(env, "vk") == 0;
    }
#if defined(__APPLE__)
    d.vulkan = true;  // (macOS: the Vulkan renderer's code on Metal, the one renderer there: docs/metal.md)
#endif
    d.fullscreen = v[S_DISPLAY] != 0;
    d.window_w = gs_.win_w;
    d.window_h = gs_.win_h;
    d.vsync = v[S_VSYNC] != 0;
    d.fps_cap = CAPS[v[S_CAP]];
    d.monitor = v[S_MONITOR];
    if (d.fullscreen != gs_.display.fullscreen || d.window_w != gs_.display.window_w || d.window_h != gs_.display.window_h || d.vsync != gs_.display.vsync ||
        d.fps_cap != gs_.display.fps_cap || d.monitor != gs_.display.monitor) {
        gs_.display = d;
        ++gs_.serial;
    }
    gs_.display.vulkan = d.vulkan;  // (no serial: the host reads it once, at its start)
    gs_.display.vulkan_auto = d.vulkan_auto;
}

// orig: (none) — enhancement: the real values written where the page shows them (EE)
// The values as they act, in the layout before the switch (the bytes the same as then - the headless runs' RAM hashes): the
// Lighting byte 0 Original, 1..3 Low..High; the switches under it 0 with Graphics: Original; S_GFX and S_PRESET not written
void values_to_ee() {
    const bool on = gs_.value[S_GFX] != 0;
    for (uint32_t i = 0; i < S_GFX; ++i) {
        int v = gs_.value[i];
        if (i == S_LIGHTING) v = on ? v + 1 : 0;
        else if (!on && (i == S_DYN_SHADOWS || i == S_AO || i == S_SUN_RAYS || i == S_SURFACE || i == S_PARTICLES || i == S_GRASS || i == S_WREFL)) v = 0;
        sb(VALUES + i, static_cast<uint32_t>(v));
    }
}

// --- the preferences: $RAC1_GRAPHICS_FILE, else the Advanced settings' directory /graphics_options.txt
// orig: (none) — enhancement: the preferences file's path
std::filesystem::path prefs_path() {
    if (const char* f = std::getenv("RAC1_GRAPHICS_FILE")) return f;
    if (const std::filesystem::path d = platform::config_dir(); !d.empty()) return d / "graphics_options.txt";
    return "graphics_options.txt";
}

// orig: (none) — enhancement: the preferences read; false when there are none
bool prefs_read(const std::filesystem::path& path) {
    FILE* f = platform::open_file(path, "r");
    if (f == nullptr) return false;
    char key[64];
    int old_lighting = -1;
    bool seen_graphics = false, seen_quality = false;
    char renderer[32] = {};  // ("" when there is no `renderer` line: a file from before 0.5.0)
    while (std::fscanf(f, "%63s", key) == 1) {
        int a = 0, b = 0;
        if (std::strcmp(key, "renderer") == 0) {  // (a name: opengl / vulkan / auto; the versions up to 0.7.x read auto as OpenGL)
            char name[32] = {};
            if (std::fscanf(f, "%31s", name) == 1) {
                std::memcpy(renderer, name, sizeof renderer);
                gs_.value[S_RENDERER] = std::strcmp(name, "vulkan") == 0 ? 1 : 0;
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
        if (std::fscanf(f, "%d", &a) != 1) {  // (a value that is not a number - a later version's, a hand edit: that line skipped)
            char rest[256];
            if (std::fgets(rest, sizeof rest, f) == nullptr) break;
            continue;
        }
        if (std::strcmp(key, "display") == 0) gs_.value[S_DISPLAY] = a != 0;
        else if (std::strcmp(key, "renderer_set") == 0) gs_.renderer_set = a != 0;
        else if (std::strcmp(key, "render_scale") == 0) gs_.value[S_SCALE] = nearest(SCALES, a);
        else if (std::strcmp(key, "aspect") == 0 && a >= 0 && a < 3) gs_.value[S_ASPECT] = a;
        else if (std::strcmp(key, "fit") == 0) gs_.value[S_FIT] = a != 0;
        else if (std::strcmp(key, "hud") == 0 && a >= 0 && a < 5) gs_.value[S_HUD] = a;
        else if (std::strcmp(key, "lod") == 0 && a >= 0 && a < 5) gs_.value[S_LOD] = a;
        else if (std::strcmp(key, "fov") == 0) gs_.value[S_FOV] = nearest(FOVS, a);
        else if (std::strcmp(key, "vsync") == 0) gs_.value[S_VSYNC] = a != 0;
        else if (std::strcmp(key, "fps_cap") == 0) gs_.value[S_CAP] = nearest(CAPS, a);
        else if ((std::strcmp(key, "lighting") == 0 || std::strcmp(key, "shadows") == 0) && a >= 0 && a < 4) old_lighting = a;  // ("shadows": before; below)
        else if (std::strcmp(key, "graphics") == 0) {
            gs_.value[S_GFX] = a != 0;
            seen_graphics = true;
        } else if (std::strcmp(key, "lighting_quality") == 0 && a >= 0 && a < 3) {
            gs_.value[S_LIGHTING] = a;
            seen_quality = true;
        }
        else if (std::strcmp(key, "dynamic_shadows") == 0) gs_.value[S_DYN_SHADOWS] = a != 0;
        else if (std::strcmp(key, "secondary_lights") == 0 && a >= 0 && a < 3) gs_.value[S_SEC_LIGHTS] = a;
        else if (std::strcmp(key, "shadow_distance") == 0 && a >= 0 && a < 4) gs_.value[S_SHADOW_DIST] = a;
        else if (std::strcmp(key, "ambient_occlusion") == 0 && a >= 0 && a < 11) gs_.value[S_AO] = a;
        else if (std::strcmp(key, "texture_detail") == 0) gs_.value[S_MIPS] = a != 0;  // (0 full, 1 original)
        else if (std::strcmp(key, "sun_rays") == 0 && a >= 0 && a < 3) gs_.value[S_SUN_RAYS] = a == 0 ? 0 : a + 1;  // (until 0.4.x: Off, Low, High - Low is Medium now)
        else if (std::strcmp(key, "sun_rays_quality") == 0 && a >= 0 && a < 4) gs_.value[S_SUN_RAYS] = a;
        else if (std::strcmp(key, "surface_detail") == 0 && a >= 0 && a < 3) gs_.value[S_SURFACE] = a;
        else if (std::strcmp(key, "shadow_strength") == 0) gs_.value[S_SHADOW_STR] = nearest(SHADOW_STRS, a);
        else if (std::strcmp(key, "ao_intensity") == 0) gs_.value[S_AO_STR] = nearest(AO_STRS, a);
        else if (std::strcmp(key, "ambient_particles") == 0 && a >= 0 && a < 3) gs_.value[S_PARTICLES] = a;
        // ("foliage" and "water", the Foliage and Water rows' until they were removed: read like any other key and ignored)
        else if (std::strcmp(key, "grass") == 0 && a >= 0 && a < 3) gs_.value[S_GRASS] = a;
        else if (std::strcmp(key, "water_reflections") == 0 && a >= 0 && a < 4) gs_.value[S_WREFL] = a;  // (0 Low, 1 Medium, 2 Medium (RT), 3 High (RT))
        // ("overscan", the Overscan row's until 0.4: read like any other key and ignored, the full frame always shown)
        // ("mode", the New Game mode until it became a playthrough's, kept with the saves alone: read like any other key
        // and ignored)
    }
    std::fclose(f);
    // the Renderer row: `vulkan` kept; `auto`, or a file whose row the player never chose (no `renderer_set`: the
    // `renderer opengl` every file got as the default from 0.5.0 to 0.7.x) moves to auto once; chosen, kept
    const bool explicit_vk = std::strcmp(renderer, "vulkan") == 0;
    gs_.renderer_auto = std::strcmp(renderer, "auto") == 0 || (!gs_.renderer_set && !explicit_vk);
    if (gs_.renderer_auto) gs_.value[S_RENDERER] = 0;
    if (gs_.renderer_auto && std::strcmp(renderer, "auto") != 0)
        std::printf("[graphics] renderer: '%s' never chosen in Options > Display: auto (Vulkan, OpenGL when it cannot start)\n", renderer[0] != 0 ? renderer : "(none)");
    // the preferences of before the switch (no `graphics` key): their `lighting` (0 Original, 1..3 Low..High) is the switch and
    // the Lighting row - Original: Graphics Original, the row at its default (High); Low..High: Enhanced with that value. (Ours
    // write `lighting` too, the same way, for the versions before: theirs is read only when `graphics` is not there)
    if (!seen_graphics) {
        if (old_lighting >= 0) {
            gs_.value[S_GFX] = old_lighting != 0 ? 1 : 0;
            if (old_lighting != 0 && !seen_quality) gs_.value[S_LIGHTING] = old_lighting - 1;
        }
        gs_.dirty = true;  // (written again in the new form at the first frame)
    }
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
    {
        const int* v = gs_.value;
        static constexpr const char* kFormat =
            "display %d\nmonitor %s\nwindow %d %d\nrender_scale %d\naspect %d\nfit %d\nlod %d\nfov %d\nvsync %d\nfps_cap %d\nlighting %d\ndynamic_shadows %d\nsecondary_lights %d\nshadow_distance %d\nambient_occlusion %d\ntexture_detail %d\nshadow_strength %d\nsun_rays_quality %d\nao_intensity %d\nambient_particles %d\nhud %d\nrenderer %s\nrenderer_set %d\nsurface_detail %d\ngrass %d\nwater_reflections %d\ngraphics %d\nlighting_quality %d\n";
        // (`lighting` as the versions before the switch read it: 0 Original, 1..3 Low..High)
        const int old_lighting = v[S_GFX] != 0 ? v[S_LIGHTING] + 1 : 0;
        auto format = [&](char* out, size_t n) {
            return std::snprintf(out, n, kFormat, v[S_DISPLAY], gs_.monitor_name.c_str(), gs_.win_w, gs_.win_h, SCALES[v[S_SCALE]], v[S_ASPECT], v[S_FIT], v[S_LOD], FOVS[v[S_FOV]], v[S_VSYNC], CAPS[v[S_CAP]], old_lighting, v[S_DYN_SHADOWS],
                     v[S_SEC_LIGHTS], v[S_SHADOW_DIST], v[S_AO], v[S_MIPS], SHADOW_STRS[v[S_SHADOW_STR]], v[S_SUN_RAYS], AO_STRS[v[S_AO_STR]], v[S_PARTICLES], v[S_HUD], gs_.renderer_auto ? "auto" : v[S_RENDERER] != 0 ? "vulkan" : "opengl", gs_.renderer_set ? 1 : 0, v[S_SURFACE], v[S_GRASS], v[S_WREFL],
                     v[S_GFX], v[S_LIGHTING]);
        };
        // (through a temporary file: a crash or a full disk never leaves the preferences half written)
        const int len = format(nullptr, 0);
        std::string text(static_cast<size_t>(std::max(len, 0)) + 1, '\0');
        format(text.data(), text.size());
        text.resize(static_cast<size_t>(std::max(len, 0)));
        if (!platform::write_file_replace(path, text)) std::fprintf(stderr, "[graphics] cannot write %s\n", path.string().c_str());
    }
    gs_.dirty = false;
}

void rows_write(int p);

// orig: (none) — enhancement: a value changed by the page (or New Game's preset): applied, the
// preferences to be written
void changed() {
    // (the Renderer row chosen: auto no more, written as chosen from now on)
    const bool renderer_chosen = gs_.run_known && gs_.value[S_RENDERER] != gs_.renderer_seen;
    if (renderer_chosen) {
        gs_.renderer_auto = false;
        gs_.renderer_set = true;
    }
    if (gs_.value[S_MONITOR] >= 0 && static_cast<size_t>(gs_.value[S_MONITOR]) < gs_.monitors.size()) gs_.monitor_name = gs_.monitors[gs_.value[S_MONITOR]];  // (the row's choice kept by name)
    apply();
    values_to_ee();
    gs_.dirty = true;
    // (the Renderer row chosen again: the host's safety net markers removed, vk_safety.h)
    if (renderer_chosen && gs_.chosen_hook != nullptr) gs_.chosen_hook();
    gs_.renderer_seen = gs_.value[S_RENDERER];
    // (the Graphics page's rows: the switch alone with Original, all of them with Enhanced)
    if (gs_.installed && (gs_.value[S_GFX] != 0) != (gs_.shown_count[P_GRAPHICS] > 1)) rows_write(P_GRAPHICS);
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
#if defined(__APPLE__)
        case S_RENDERER: return "Metal";
#else
        case S_RENDERER: {  // (the choice; the renderer running when it is another: "Vulkan (OpenGL)"; auto: the one running, its value)
            const char* chosen = v != 0 ? "Vulkan" : "OpenGL";
            if (!gs_.run_known || gs_.run_vulkan == (v != 0)) return chosen;
            return std::string(chosen) + (gs_.run_vulkan ? " (Vulkan)" : " (OpenGL)");  // (why: the row's help, renderer_help)
        }
#endif
        case S_DISPLAY: return t[v != 0 ? T_FULLSCREEN : T_WINDOW];
        case S_MONITOR: return v >= 0 && static_cast<size_t>(v) < gs_.monitors.size() ? gs_.monitors[v] : std::string("?");
        case S_WINDOW: std::snprintf(b, sizeof b, "%d x %d", gs_.win_w, gs_.win_h); return b;
        case S_SCALE: std::snprintf(b, sizeof b, "x%d.%02d", SCALES[v] / 100, SCALES[v] % 100); return b;  // (the font's '%' is a button)
        case S_ASPECT: return t[v == 0 ? T_43 : v == 1 ? T_169 : T_FILL];
        case S_FIT: return t[v != 0 ? T_STRETCH : T_BARS];
        case S_HUD: return t[v == 0 ? T_43 : v == 1 ? T_169 : v == 2 ? T_219 : v == 3 ? T_329 : T_HUD_FIT];
        case S_LOD: return t[T_ORIGINAL + v];
        case S_LIGHTING: return t[T_LOW + v];
        case S_GFX: return t[v != 0 ? T_ENHANCED : T_ORIGINAL];
        case S_PRESET: {
            const int p = preset_of(gs_.value);
            return t[p >= PRESET_COUNT ? T_CUSTOM : T_LOW + p];
        }
        case S_DYN_SHADOWS: return t[v != 0 ? T_ON : T_OFF];
        case S_SEC_LIGHTS: return t[v == 0 ? T_OFF : v == 1 ? T_SEC_GAME : T_SEC_ALL];
        case S_SHADOW_DIST: return t[T_LOW + v];
        case S_SUN_RAYS: return t[v == 0 ? T_OFF : T_LOW + v - 1];
        case S_SURFACE: return t[v == 0 ? T_OFF : v == 1 ? T_LOW : T_HIGH];
        case S_PARTICLES: return t[v == 0 ? T_OFF : v == 1 ? T_LOW : T_HIGH];
        case S_GRASS: return t[v == 0 ? T_OFF : v == 1 ? T_LOW : T_HIGH];
        case S_WREFL: return v >= 2 ? RT_WORDS[t == TEXTS[1] ? 1 : t == TEXTS[2] ? 2 : t == TEXTS[3] ? 3 : 0][v - 2] : t[v == 0 ? T_LOW : T_MEDIUM];
        case S_SHADOW_STR: std::snprintf(b, sizeof b, "%d", SHADOW_STRS[v]); return b;  // (the font's '%' is a button)
        case S_AO_STR: std::snprintf(b, sizeof b, "%d", AO_STRS[v]); return b;
        case S_AO: {
            static constexpr const char* AO_NAMES[] = {"SSAO", "SSAO+", "HBAO", "GTAO", "GTAO 1/2", "SSAO 1/2", "SSAO+ 1/2", "HBAO 1/2", "MXAO", "MXAO 1/2"};
            return v <= 0 ? t[T_OFF] : AO_NAMES[std::min(v, 10) - 1];
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

// orig: (none) — enhancement: the Renderer row's help: the usual one, or why the renderer running is not the row's choice
std::string renderer_help() {
    const char* const* t = texts();
#if !defined(__APPLE__)
    const int v = gs_.value[S_RENDERER];
    // (auto fell back to OpenGL: the row says OpenGL, the help why)
    const bool auto_fell = gs_.renderer_auto && gs_.run_known && !gs_.run_vulkan && gs_.run_why != RendererWhy::kNone && gs_.run_why != RendererWhy::kEnv;
    if (auto_fell || (gs_.run_known && gs_.run_vulkan != (v != 0))) {
        const int why = v != gs_.run_choice ? 0 : static_cast<int>(gs_.run_why);
        return RENDERER_WHY[t == TEXTS[1] ? 1 : t == TEXTS[2] ? 2 : t == TEXTS[3] ? 3 : 0][why];
    }
#endif
    return t[H_RENDERER];
}

// orig: (none) — enhancement: a value the display shows differently from the others (the rows step over the rest): the
// Screen's 16:9 not on a 16:9 display (there it is Fill's; a narrower one shows it with bars), the HUD's 16:9 / 21:9 / 32:9 only when narrower
// than the view (else they are Fit's). 4:3 (the original) and Fill / Fit always, so a HUD narrower than the view can always
// be picked; a value chosen before (another display) stays, shown as it is
bool value_supported(Setting s, int v) {
    constexpr float kSlack = 0.01f;
    if (s == S_WREFL && v >= 2) return mrp_rt_supported();  // (the RT values: Vulkan with ray queries; a saved one elsewhere plays as Medium)
    if (gs_.out_w <= 0 || gs_.out_h <= 0) return true;
    if (s == S_ASPECT && v == 1) return std::fabs(static_cast<float>(gs_.out_w) / static_cast<float>(gs_.out_h) - 16.0f / 9.0f) > kSlack;
    if (s == S_HUD && v >= 1 && v <= 3) return HUD_ASPECTS[v] < 4.0f / 3.0f * g_view_widen - kSlack;
    return true;
}

// orig: (none) — enhancement: a setting stepped by d (+1 / -1), wrapping, over the values the display supports
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
    if (s == S_PRESET) {  // the presets in turn (from Custom: the first forward, the last back), every row under it set
        const int p = preset_of(gs_.value);
        preset_set(gs_.value, p >= PRESET_COUNT ? (d > 0 ? 0 : PRESET_COUNT - 1) : (p + d + PRESET_COUNT) % PRESET_COUNT);
        changed();
        return;
    }
    const int n = static_cast<int>(value_count(s));
    if (n <= 0) return;  // (the monitor row before GLFW is up: nothing to step through)
    int v = gs_.value[s];
    for (int i = 0; i < n; ++i) {  // (the next value the display supports)
        v = (v + d + n) % n;
        if (value_supported(s, v)) break;
    }
    if (v == gs_.value[s]) return;
    gs_.value[s] = v;
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

// orig: (none) — enhancement: page `p`'s rows shown (the Graphics page with Graphics: Original: the switch alone)
void rows_shown(int p) {
    const PageDef& pd = PAGES[p];
    int n = 0;
    for (int i = 0; i < pd.count; ++i) {
        if (p == P_GRAPHICS && gs_.value[S_GFX] == 0 && pd.def[i].s != S_GFX) continue;
        gs_.shown[p][n++] = i;
    }
    gs_.shown_count[p] = n;
    gs_.top[p] = std::clamp(gs_.top[p], 0, std::max(0, n - VISIBLE));
}

// orig: (none) — enhancement: page `p`'s widget's rows for the window from its top (their names and helps; the values by slot)
void rows_write(int p) {
    const PageDef& pd = PAGES[p];
    rows_shown(p);
    uint32_t row = pd.rows;
    const int shown = std::min(VISIBLE, gs_.shown_count[p]);
    for (int i = 0; i < shown; ++i, row += 0x18) {
        const Row& r = pd.def[gs_.shown[p][gs_.top[p] + i]];
        const uint32_t vt = static_cast<uint32_t>(VALUE_FIRST + p * VISIBLE + i);
        sw(row, static_cast<uint32_t>(TEXT_FIRST + r.name));
        sw(row + 4, pd.steps + static_cast<uint32_t>(i));
        sw(row + 8, vt);  // (two values, both the value's text: cross steps the byte, graphics_setting_changed the setting)
        sw(row + 0xc, vt);
        sw(row + 0x10, 0);
        sw(row + 0x14, 0);
        sb(pd.steps + static_cast<uint32_t>(i), 0);
        sw(pd.row_help + static_cast<uint32_t>(i) * 4, static_cast<uint32_t>(TEXT_FIRST + r.help));
    }
    for (uint32_t i = 0; i < 0x18; i += 4) sw(row + i, 0);
}

// orig: (none) — enhancement: the setting of page `p`'s shown slot `slot` (S_COUNT: none)
Setting slot_setting(int p, int slot) {
    const int k = gs_.top[p] + slot;
    if (slot < 0 || k >= gs_.shown_count[p]) return S_COUNT;
    return PAGES[p].def[gs_.shown[p][k]].s;
}

// orig: (none) — enhancement: a row of page `p` stepped (the switch shows or hides the rows under it: changed)
void page_step(int, Setting s, int d) { step(s, d); }

// orig: (none) — enhancement: one of the two pages built from Camera's, its parent `parent`
void page_install(int p, uint32_t parent) {
    const PageDef& pd = PAGES[p];
    ee_copy(pd.page, CAMERA_PAGE, 0x84);
    ee_copy(pd.title, CAMERA_TITLE, 0x50);
    ee_copy(pd.rows_widget, CAMERA_SETTINGS, 0x50);
    ee_copy(pd.help_widget, OPTIONS_HELP, 0x50);
    sw(pd.title + 0x34, static_cast<uint32_t>(TEXT_FIRST + pd.title_text));
    gs_.top[p] = 0;
    gs_.prev_sel[p] = 0;
    rows_write(p);
    sw(pd.rows_widget + 0x34, pd.rows);
    sw(pd.rows_widget + 0x38, 0);
    sw(pd.rows_widget + 0x40, 0);
    // the help label: the focused widget's +0x40 (the settings widget's selection, mirrored) as
    // the index into its texts (flag 0x80); its box is the Options page's help box (slot 1)
    sw(pd.help_widget + 0x30, 0x80);
    sw(pd.help_widget + 0x34, pd.row_help);
    sw(pd.page + 0x38, parent);
    sw(pd.page + 0x40, pd.rows_widget);
    slot_replace(pd.page, CAMERA_TITLE, pd.title);
    slot_replace(pd.page, CAMERA_SETTINGS, pd.rows_widget);
    // the boxes (the menu mobys' animations): Camera's settings box holds 4 or 5 rows, ours are 10 shown: the
    // settings in the taller box of page 0x1b6678 (its slot 3), the help in the Options page's help box
    // (its slot 1), left of it (chosen by trying the game's boxes)
    sw(pd.page + 0xc, lwu(TALL_PAGE + 0xc));
    sw(pd.page + 4, lwu(OPTIONS_PAGE + 4));
    sw(pd.page + 0x44 + 4, pd.help_widget);
    sw(pd.page + 0x80, 0);
    gs_.installed = true;
}
}  // namespace

// orig: (none) — enhancement (graphics.h)
bool graphics_enhanced() { return gs_.live_enh; }

// orig: (none) — enhancement (graphics.h)
void graphics_set_enhanced_live(bool on) {
    if (on == gs_.live_enh) return;
    if (!on) {  // (the live values kept aside, every addition off)
        const Additions a = additions_now();
        static_assert(sizeof(Additions) == sizeof(gs_.stash), "the stash holds the switches");
        std::memcpy(gs_.stash, &a, sizeof a);
        additions_set(Additions{});
        gs_.live_enh = false;
        gs_.stash_ok = true;
        return;
    }
    if (gs_.stash_ok) {
        Additions a;
        std::memcpy(&a, gs_.stash, sizeof a);
        additions_set(a);
        gs_.live_enh = true;
        gs_.stash_ok = false;
    } else {
        additions_apply(gs_.value, true, false);  // (Original since the start: the player's rows)
    }
}

// orig: (none) — enhancement (graphics.h)
int graphics_live_preset() {
    int v[S_COUNT];
    std::memcpy(v, gs_.value, sizeof v);
    rows_from_live(v, additions_now());
    return preset_of(v);
}

// orig: (none) — enhancement (graphics.h)
void graphics_set_live_preset(int preset) {
    if (preset < 0 || preset >= PRESET_COUNT) return;
    int v[S_COUNT];
    std::memcpy(v, gs_.value, sizeof v);
    rows_from_live(v, additions_now());  // (the rows the preset does not set: as they are live)
    preset_set(v, preset);
    additions_apply(v, gs_.live_enh, false);
}

// orig: (none) — enhancement (graphics.h)
void graphics_load() {
    if (gs_.loaded) return;
    gs_.loaded = true;
    if (!g_advanced_disabled) {
        if (!prefs_read(prefs_path())) {
            prefs_migrate();
            gs_.renderer_auto = true;  // (a fresh file: `renderer auto`; State()'s S_RENDERER stays 0, the EE byte)
            gs_.dirty = true;
        }
    }
    apply();
    values_to_ee();
    // (every enhancement off - the tests: the renderer's own switches, the testing variables', are what acts)
    if (g_advanced_disabled) gs_.live_enh = g_modern_shadows != 0 || g_point_shadows != 0 || g_ao != 0 || g_sun_rays != 0 || g_surface_detail != 0 || g_ambient_particles != 0 || g_grass != 0;
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
    gs_.out_w = w;  // (the Screen and HUD rows' values: value_supported)
    gs_.out_h = h;
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
    page_install(P_DISPLAY, parent);
    page_install(P_GRAPHICS, parent);
    return PAGE;
}
// orig: (none) — enhancement (graphics.h)
uint32_t graphics_display_page() { return DPAGE; }

// orig: (none) — enhancement (graphics.h)
uint32_t graphics_text(int32_t id) {
    std::string s;
    int32_t slot = 0;  // (the string's)
    if (id >= VALUE_FIRST && id < VALUE_FIRST + PAGE_COUNT * VISIBLE) {  // a shown row's value
        const int p = (id - VALUE_FIRST) / VISIBLE, i = (id - VALUE_FIRST) % VISIBLE;
        const Setting st = slot_setting(p, i);
        if (st != S_COUNT) s = value_text(st);
        slot = VALUE_SLOT + (id - VALUE_FIRST);
    } else {
        if (id < TEXT_FIRST || id >= TEXT_FIRST + T_COUNT) return 0;
        const int32_t k = id - TEXT_FIRST;
        if (k >= T_WINDOW && k <= T_CUSTOM) return 0;  // (a value word: never asked for, see string_slot)
        s = k == H_RENDERER ? renderer_help() : texts()[k];
        slot = string_slot(k);
    }
    const uint32_t at = STRINGS + static_cast<uint32_t>(slot) * STRING_SIZE;
    char* p = static_cast<char*>(ee_ram_ptr(at));
    std::strncpy(p, s.c_str(), STRING_SIZE - 1);
    p[STRING_SIZE - 1] = 0;
    return at;
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
        for (size_t i = 0; i < gs_.monitors.size() && index < 0; ++i)
            if (gs_.monitors[i] == gs_.monitor_name) index = static_cast<int>(i);
    // (a name kept before two monitors of that name were told apart by their place, "Name (at x,y)": the first of them, #111)
    if (index < 0 && !gs_.monitor_name.empty())
        for (size_t i = 0; i < gs_.monitors.size() && index < 0; ++i)
            if (gs_.monitors[i].rfind(gs_.monitor_name + " (at ", 0) == 0) index = static_cast<int>(i);
    if (index >= 0) gs_.monitor_name = gs_.monitors[static_cast<size_t>(index)];
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
void graphics_set_renderer_running(bool vulkan, RendererWhy why) {
    gs_.display.vulkan = vulkan;
    gs_.run_known = true;
    // (auto: the row's value is the renderer running - not written to the EE byte, which only the row's changes write)
    if (gs_.renderer_auto) gs_.value[S_RENDERER] = vulkan ? 1 : 0;
    gs_.run_vulkan = vulkan;
    gs_.run_choice = gs_.renderer_seen = gs_.value[S_RENDERER];
    gs_.run_why = why == RendererWhy::kNone && vulkan != (gs_.value[S_RENDERER] != 0) ? RendererWhy::kEnv : why;
}
// orig: (none) — enhancement (graphics.h)
void graphics_set_renderer_chosen_hook(void (*fn)()) { gs_.chosen_hook = fn; }

namespace {
// orig: (none) — enhancement: the 14 menu mobys `mobys` put on page `to`'s boxes at once (graphics_menu_back / _forward)
void boxes_hold(uint32_t to, uint32_t mobys) {
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
}  // namespace

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
    if (cur != PAGE && cur != DPAGE && cur != POP_PAGE && cur != POP2_PAGE && cur != advanced_page() && cur != controls_page() && cur != language_page() && cur != update_notice_page() && cur != updates_page()) return;
    boxes_hold(to, mobys);
}

// orig: (none) — enhancement (graphics.h)
void graphics_menu_forward(uint32_t cur, uint32_t to, uint32_t mobys) {
    if (cur == POP_PAGE && to == POP2_PAGE) boxes_hold(to, mobys);
}

// orig: (none) — enhancement (graphics.h)
int32_t graphics_title_text() { return TEXT_FIRST + T_GRAPHICS; }
// orig: (none) — enhancement (graphics.h)
int32_t graphics_help_text() { return TEXT_FIRST + T_OPTIONS_HELP; }
// orig: (none) — enhancement (graphics.h)
int32_t graphics_display_title_text() { return TEXT_FIRST + T_DISPLAY_PAGE; }
// orig: (none) — enhancement (graphics.h)
int32_t graphics_display_help_text() { return TEXT_FIRST + H_DISPLAY_PAGE; }

// orig: (none) — enhancement (graphics.h)
bool graphics_setting_changed(uint32_t byte) {
    for (int p = 0; p < PAGE_COUNT; ++p) {
        const uint32_t steps = PAGES[p].steps;
        if (byte < steps || byte >= steps + static_cast<uint32_t>(VISIBLE)) continue;
        sb(byte, 0);
        const Setting s = slot_setting(p, static_cast<int>(byte - steps));
        if (s != S_COUNT) page_step(p, s, 1);
        return true;
    }
    return false;
}

// orig: (none) — enhancement (graphics.h)
bool graphics_settings_dpad(uint32_t w, uint32_t e, uint32_t pressed) {
    int p = 0;
    while (p < PAGE_COUNT && PAGES[p].rows_widget != w) ++p;
    if (p == PAGE_COUNT) return false;
    // the window moved when up / down go past its ends (the widget stops there: its rows end), as the Controls page
    int sel = lw(w + 0x38);
    if ((pressed & 0x4000u) != 0 && gs_.prev_sel[p] == VISIBLE - 1 && sel == VISIBLE - 1 && gs_.top[p] + VISIBLE < gs_.shown_count[p]) {
        ++gs_.top[p];
        rows_write(p);
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    } else if ((pressed & 0x1000u) != 0 && gs_.prev_sel[p] == 0 && sel == 0 && gs_.top[p] > 0) {
        --gs_.top[p];
        rows_write(p);
        moby_sound_start(1, 0x11, lwu(w + 0x14));
    }
    sel = lw(w + 0x38);
    gs_.prev_sel[p] = sel;
    sw(w + 0x40, static_cast<uint32_t>(sel));  // (the help label's index)
    if ((pressed & 0xa000u) == 0) return false;
    const uint32_t b = lwu(e + 4), steps = PAGES[p].steps;
    if (b >= steps && b < steps + static_cast<uint32_t>(VISIBLE)) {
        const Setting s = slot_setting(p, static_cast<int>(b - steps));
        if (s == S_COUNT) return false;
        page_step(p, s, (pressed & 0x8000u) != 0 ? -1 : 1);
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

// orig: (none) — enhancement (graphics.h)
void graphics_take_live() {
    if (g_advanced_disabled) return;
    int* v = gs_.value;
    v[S_GFX] = gs_.live_enh ? 1 : 0;
    // the rows from the live values: Enhanced's, or those kept aside by the live Original (none kept: Original since the start,
    // the rows as they are)
    if (gs_.live_enh) {
        rows_from_live(v, additions_now());
    } else if (gs_.stash_ok) {
        Additions a;
        std::memcpy(&a, gs_.stash, sizeof a);
        rows_from_live(v, a);
    }
    changed();
}

// --- the main menu -------------------------------------------------------------------------------

namespace {
// orig: (none) — enhancement: one of New Game's windows built at `page` from the boot module's copy of the pause menu's
// Options page `opt`: its title (text `title_text`), help label, picture and list (its blocks at page + 0x90 / + 0x130 /
// + 0x180 / + 0xe0, as both windows lay them out), the `n` entries copied from `entry` (the root's New Game entry) with
// action `action` to page `to` (0: the entry's own, New Game's), the names, helps and pictures given; its parent `parent`
void window_build(uint32_t page, uint32_t opt, int32_t title_text, uint32_t entries, uint32_t helps, uint32_t pictures, uint32_t n,
                  const int32_t* names, const int32_t* help_texts, const uint32_t* pics, uint32_t entry, uint32_t action, uint32_t to,
                  uint32_t parent) {
    const uint32_t title = lwu(opt + 0x44), label = lwu(opt + 0x48), picture = lwu(opt + 0x4c), list = lwu(opt + 0x50);
    const uint32_t ptitle = page + 0x90, plist = page + 0xe0, plabel = page + 0x130, ppicture = page + 0x180;
    ee_copy(page, opt, 0x84);
    ee_copy(ptitle, title, 0x50);
    ee_copy(plabel, label, 0x50);
    ee_copy(ppicture, picture, 0x50);
    ee_copy(plist, list, 0x50);
    sw(ptitle + 0x34, static_cast<uint32_t>(title_text));
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t e = entries + i * 0xc;
        ee_copy(e, entry, 0xc);
        sh(e, static_cast<uint32_t>(TEXT_FIRST + names[i]));
        if (to != 0) {
            sh(e + 2, action);
            sw(e + 4, to);
        }
        sh(e + 0xa, 0);
        sw(helps + i * 4, static_cast<uint32_t>(TEXT_FIRST + help_texts[i]));
        sw(pictures + i * 8, PICTURE_SECTOR + pics[i]);
        sw(pictures + i * 8 + 4, 1);
    }
    for (uint32_t i = 0; i < 0xc; i += 4) sw(entries + n * 0xc + i, 0);
    sw(helps + n * 4, 0);
    sw(plist + 0x34, entries);
    sw(plist + 0x40, 0);  // (the first entry selected: Original, Current)
    sw(plist + 0x38, 0);
    sw(plist + 0x3c, 0);
    sw(plabel + 0x34, helps);
    sw(ppicture + 0x30, pictures);
    sw(page + 0x38, parent);
    sw(page + 0x40, plist);
    slot_replace(page, title, ptitle);
    slot_replace(page, label, plabel);
    slot_replace(page, picture, ppicture);
    slot_replace(page, list, plist);
    sw(page + 5 * 4, 0x13);  // (slot 5: the root page's Updates button, held as the root leaves it)
    sw(page + 0x80, 0);
}
static_assert(POP_TITLE == POP_PAGE + 0x90 && POP_LIST == POP_PAGE + 0xe0 && POP_LABEL == POP_PAGE + 0x130 && POP_PICTURE == POP_PAGE + 0x180 &&
                  POP2_TITLE == POP2_PAGE + 0x90 && POP2_LIST == POP2_PAGE + 0xe0 && POP2_LABEL == POP2_PAGE + 0x130 && POP2_PICTURE == POP2_PAGE + 0x180,
              "New Game's windows lay out their widgets alike");
}  // namespace

// orig: (none) — enhancement (graphics.h)
void graphics_main_menu_install() {
    if (g_advanced_disabled) return;
    graphics_load();
    // the two windows: the pause menu's Options page (its copy in the boot module), our lists, helps, pictures
    const uint32_t opt = OPTIONS_PAGE;
    // the root's New Game entry (action 4, its page), kept: the main menu started again finds ours there
    if (lwu(ROOT_NEW_GAME + 0x34) != ROOT_ENTRY) ee_copy(NG_ENTRY, lwu(ROOT_NEW_GAME + 0x34), 0x18);
    const uint32_t ng = NG_ENTRY;
    // 1. the mode (Original, Modern), each entry to the second window (action 3)
    static constexpr int32_t MODE_NAMES[] = {T_MODE_ORIGINAL, T_MODE_MODERN};
    static constexpr int32_t MODE_HELPS[] = {H_MODE_ORIGINAL, H_MODE_MODERN};
    static constexpr uint32_t MODE_PICS[] = {PIC_ORIGINAL, PIC_MODERN};
    window_build(POP_PAGE, opt, TEXT_NEW_GAME, POP_ENTRIES, POP_HELP, POP_PICTURES, 2, MODE_NAMES, MODE_HELPS, MODE_PICS, ng, 3, POP2_PAGE, ROOT_PAGE);
    // 2. the graphics preset (Current, Original, Low, Medium, High), each entry New Game's own (action 4, its page)
    static constexpr int32_t PRESET_NAMES[] = {T_NG_CURRENT, T_NG_ORIGINAL, T_NG_LOW, T_NG_MEDIUM, T_NG_HIGH};
    static constexpr int32_t PRESET_HELPS[] = {H_NG_CURRENT, H_NG_ORIGINAL, H_NG_LOW, H_NG_MEDIUM, H_NG_HIGH};
    static constexpr uint32_t PRESET_PICS[] = {PIC_GFX, PIC_ORIGINAL, PIC_GFX, PIC_GFX, PIC_GFX};
    window_build(POP2_PAGE, opt, TEXT_FIRST + T_GRAPHICS, POP2_ENTRIES, POP2_HELP, POP2_PICTURES, 5, PRESET_NAMES, PRESET_HELPS, PRESET_PICS, ng, 4, 0, POP_PAGE);
    // the root's New Game: our entry, action 3 to the first window (its list's entries copied)
    if (lwu(ROOT_NEW_GAME + 0x34) != ROOT_ENTRY) {
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
    language_main_entries(MAIN_ENTRIES, n);  // (the game's Language entry: our Language page)
    uint32_t d = MAIN_ENTRIES + n * 0xc;
    const uint32_t gp = PAGE, ap = advanced_main_page();  // (Advanced: the global rows, when there are any)
    const struct { int32_t text; uint32_t page; } ours[] = {{TEXT_FIRST + T_DISPLAY_PAGE, DPAGE}, {TEXT_FIRST + T_GRAPHICS, gp}, {controls_title_text(), controls_page()}, {advanced_title_text(), ap}};
    for (const auto& o : ours) {
        if (o.page == 0) continue;
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
    if (w != POP2_LIST) return;
    // the first window's choice (its list keeps its selection), then the second's
    const uint32_t mode = lw(POP_LIST + 0x40) == 1 ? GAME_MODE_MODERN : GAME_MODE_ORIGINAL;
    const int32_t preset = lw(POP2_LIST + 0x40);  // 0 Current, 1 Original, 2..4 Low, Medium, High
    // the Graphics page alone: Current leaves it as it is, Original is Graphics Original (the rows under it kept), Low /
    // Medium / High Graphics Enhanced at that preset. The Display page is never touched
    if (preset == 1) {
        gs_.value[S_GFX] = 0;
        changed();
    } else if (preset >= 2 && preset <= 4) {
        gs_.value[S_GFX] = 1;
        preset_set(gs_.value, preset - 2);
        changed();
    }
    advanced_apply_mode(mode);
    static constexpr const char* PRESETS_NAMED[] = {"Current", "Original", "Low", "Medium", "High"};
    std::printf("[graphics] new game: %s mode, graphics %s\n", mode == GAME_MODE_ORIGINAL ? "Original" : "Modern",
                PRESETS_NAMED[preset >= 0 && preset <= 4 ? preset : 0]);
}

}  // namespace rac1
