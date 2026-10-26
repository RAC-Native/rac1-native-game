// _devtools/dev_menu.cpp — openly-invented debug menu for rac1-native (ADR 0003; not a port).
// Everything here pokes the game's EE memory or the host's own knobs; the game's code is not
// changed by it. The EE addresses are the ones the ported code uses (src/game/ovl).

#include "dev_menu.h"
#include "dev_unlocks.h"
#include "item_give_quiet.h"
#include "dev_profiler.h"

#include <game/ovl/ovl_reloc.h>
#include <game/debug_hooks.h>
#include <game/ovl/cutscene.h>
#include <game/boot/front_end.h>
#include <game/enhance/advanced.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/variants_l16.h>
#include <game/ovl/variants_l5.h>
#include <game/port_stubs.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_fx.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_particles.h>
#include <game/enhance/graphics.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_shadows.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <game/render/shader_manager.h>
#include <runtime/iop/snd989_iop.h>
#include <runtime/ipu/movie_hw.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vk/vk_context.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>
#include <glm/gtc/type_ptr.hpp>
#include <backends/imgui_impl_glfw.h>
#include <backends/imgui_impl_opengl3.h>
#include <backends/imgui_impl_vulkan.h>
#include <imgui.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <string>
#include <vector>

namespace rac1 {
// (mrp_particles.cpp: 1 rain, snow and ash kept out from under roofs, RAC1_PARTICLES_SKY; declared here, not in mrp_particles.h - the
// game's generated code is fingerprinted with the headers of src/game, and a debug switch must not ask for ./setup.sh again)
extern int g_particles_sky;
}  // namespace rac1

namespace rac1::devtools {

namespace {
// the game's memory (see the ported code for each)
constexpr uint32_t P = 0x0013f350;          // the player
constexpr uint32_t HEALTH = P + 0x22a8;     // nanotech (health)
constexpr uint32_t HEALTH_MAX = 0x0015eda0;
constexpr uint32_t BOLTS = 0x0015ed98;
constexpr OvConst<0x0016c058u> CTRL{};       // the frame's control block: +8 subsystem bits, +0x18 debug view
constexpr uint32_t OWNED = 0x0013d4c0;      // items owned [0x25]
constexpr uint32_t AMMO = 0x0013d428;       // items' ammo [0x25]
constexpr OvConst<0x001c40b0u> AMMO_ROWS{};  // items' ammo rows (0x18): +8 uses ammo, +0x12 maximum
constexpr OvConst<0x00179ac0u> ITEM_ROWS{};  // items (0x4c): +8 kind
constexpr uint32_t SFX_VOL = 0x0015edf0, MUSIC_VOL = 0x0015edec;
constexpr int ITEMS = 0x25;
constexpr uint32_t HERO_MOBY = P + 0x2080;  // the hero moby
constexpr uint32_t POS = P + 0x80, ROT = P + 0x90;  // the hero's position, angles (+0x98 yaw)
constexpr uint32_t VEL = P + 0xe0, VEL_H = P + 0x100;  // velocity, horizontal velocity
constexpr uint32_t POS_RING = P + 0x1b00;  // the last 32 positions (the stuck check)
constexpr uint32_t SLOTS = P + 0x1090;     // the hero's item slots (0x50 each): +0 / +4 their mobys
constexpr uint32_t ITEM_REQUEST = P + 0x20b8;  // the item in hand asked for (what the quick select writes)
constexpr uint32_t CHECKPOINT = 0x0013e090, CHECKPOINT_ROT = 0x0013e0a0;
constexpr OvConst<0x00166dc0u> CAMERA{};
constexpr OvConst<0x0015ffd8u> MOBYS{};
constexpr OvConst<0x0015ffdcu> MOBYS_STATIC_END{};
// the enemy classes of Veldin with a port (src/game/ovl/enemy_749.cpp, enemy_1440.cpp)
constexpr int ENEMY_CLASSES[] = {749, 1440};

DevSettings g_settings;
GLFWwindow* g_window = nullptr;
bool g_imgui_vk = false;  // (ImGui drawn by Vulkan)
bool g_menu = false;  // the menu and the debug tools: only with --debug-tools / RAC1_DEBUG_TOOLS=1
bool g_prev_toggle = false;
constexpr const char* DISPLAY_FILE = "devmenu_display.txt";
int g_ammo_max_cache[ITEMS] = {};

// The game's language 0x15ed88: 0 English, 2 French, 3 German,
// 4 Spanish, 5 Italian (1 has no texts on the NTSC-U disc). The game's own language page (the
// main menu's Options > Language: page 0x1b8a40, list 0x1b8b68, entries 0x1b8b20) lists these
// five in this order; choosing one is the list's action 9 (0x278840, ported in pause_menu.cpp),
// which only writes the entry's value to 0x15ed88. Its readers that pick per use (voices, the
// menus' pictures, the HUD's digit separators) follow at once; the text table (menus, subtitles,
// help) is copied for the language by the level load (0x241940) only: at the next start here.
constexpr uint32_t LANGUAGE = 0x0015ed88;
struct Language { int32_t value; const char* name; };
constexpr Language LANGUAGES[] = {{0, "English"}, {2, "French"}, {3, "German"}, {4, "Spanish"}, {5, "Italian"}};
int g_language = -1;             // the saved choice (-1: none, the game's own)
int g_boot = 1;                  // the game's own boot at the next start (saved "boot")
int32_t g_language_loaded = -1;  // 0x15ed88 when the level (its texts) was loaded
bool language_valid(int v) {
    return std::any_of(std::begin(LANGUAGES), std::end(LANGUAGES), [v](const Language& l) { return l.value == v; });
}
// the saved language of devmenu_display.txt, -1 if none
int language_saved() {
    int v = -1;
    if (FILE* f = std::fopen(DISPLAY_FILE, "r")) {
        char key[32];
        while (std::fscanf(f, "%31s", key) == 1)
            if (std::strcmp(key, "language") == 0 && std::fscanf(f, "%d", &v) != 1) v = -1;
        std::fclose(f);
    }
    return language_valid(v) ? v : -1;
}

int32_t r32(uint32_t a) { return ee_read<int32_t>(a); }
void w32(uint32_t a, int32_t v) { ee_write<int32_t>(a, v); }
float rf(uint32_t a) { return ee_read<float>(a); }
void wf(uint32_t a, float v) { ee_write<float>(a, v); }
uint32_t ru(uint32_t a) { return ee_read<uint32_t>(a); }

struct Vec3 { float x = 0, y = 0, z = 0; };
Vec3 rv(uint32_t a) { return {rf(a), rf(a + 4), rf(a + 8)}; }
void wv(uint32_t a, const Vec3& v) { wf(a, v.x); wf(a + 4, v.y); wf(a + 8, v.z); }
float dist(const Vec3& a, const Vec3& b) { return std::sqrt((a.x - b.x) * (a.x - b.x) + (a.y - b.y) * (a.y - b.y) + (a.z - b.z) * (a.z - b.z)); }

// --- the mobys ---
// the live mobys: the list from 0x15ffd8 to the end marker (state 0xff), spawned ones included
template <typename F>
void for_each_moby(F&& f) {
    const uint32_t start = ru(MOBYS);
    if (start == 0) return;
    for (uint32_t m = start; m < 0x01fe0000u && ee_read<uint8_t>(m + 0x20) != 0xff; m += 0x100) f(m);
}
bool moby_live(uint32_t m) { return static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)) >= 0; }
int moby_class(uint32_t m) { return ee_read<int16_t>(m + 0xa6); }
bool is_enemy_class(int c) { return std::find(std::begin(ENEMY_CLASSES), std::end(ENEMY_CLASSES), c) != std::end(ENEMY_CLASSES); }

// the hero's own mobys (the hero, the items in his slots), never frozen by "freeze all"
bool hero_owned(uint32_t m) {
    if (m == ru(HERO_MOBY)) return true;
    for (uint32_t k = 0; k < 6; ++k)
        if (ru(SLOTS + k * 0x50) == m || ru(SLOTS + k * 0x50 + 4) == m) return true;
    return false;
}

struct Freeze {
    bool all = false;
    bool enemies = false;
    std::set<int> classes;
    std::set<uint32_t> mobys;
} g_freeze;

bool moby_frozen(uint32_t m) {
    const Freeze& f = g_freeze;
    if (f.mobys.count(m) != 0) return true;
    const int c = moby_class(m);
    if (f.classes.count(c) != 0) return true;
    if (f.enemies && is_enemy_class(c)) return true;
    return f.all && !hero_owned(m);
}
void update_freeze_hook() {
    const Freeze& f = g_freeze;
    g_debug_moby_frozen = (f.all || f.enemies || !f.classes.empty() || !f.mobys.empty()) ? &moby_frozen : nullptr;
}

// --- the hero ---
// a teleport: the position (hero, his moby, the position ring) set, the velocities cleared; the
// game's next frame finds the ground (or makes him fall)
void hero_teleport(const Vec3& p, const float* yaw = nullptr) {
    wv(POS, p);
    wv(P + 0xc0, p);
    for (uint32_t i = 0; i < 32; ++i) wv(POS_RING + i * 16, p);
    wv(VEL, {});
    wv(VEL_H, {});
    if (yaw != nullptr) wf(ROT + 8, *yaw);
    const uint32_t hm = ru(HERO_MOBY);
    if (hm != 0) {
        wv(hm + 0x10, p);
        if (yaw != nullptr) wf(hm + 0x48, *yaw);
    }
}

struct SavedPos { bool set = false; Vec3 p; float yaw = 0; };
std::array<SavedPos, 8> g_saved_pos;
const char* POS_FILE = "devmenu_positions.txt";

void positions_save() {
    if (FILE* f = std::fopen(POS_FILE, "w")) {
        for (const SavedPos& s : g_saved_pos)
            std::fprintf(f, "%d %f %f %f %f\n", s.set ? 1 : 0, s.p.x, s.p.y, s.p.z, s.yaw);
        std::fclose(f);
    }
}
void positions_load() {
    if (FILE* f = std::fopen(POS_FILE, "r")) {
        for (SavedPos& s : g_saved_pos) {
            int set = 0;
            if (std::fscanf(f, "%d %f %f %f %f", &set, &s.p.x, &s.p.y, &s.p.z, &s.yaw) != 5) break;
            s.set = set != 0;
        }
        std::fclose(f);
    }
}

// --- the items ---
// the item names (the community's R&C1 item ids; the ported code agrees on 8 the wrench, 10 the
// bomb glove, the packs' / helmets' kinds and which ones take ammo). The game's own name text
// (0x1b0340) is not readable through the ported text lookup yet.
const char* const ITEM_NAMES[ITEMS] = {
    "", "", "Heli-Pack", "Thruster-Pack", "Hydro-Pack", "Sonic Summoner", "O2 Mask", "Pilot's Helmet",
    "Wrench", "Suck Cannon", "Bomb Glove", "Devastator", "Swingshot", "Visibomb Gun", "Taunter", "Blaster",
    "Pyrocitor", "Mine Glove", "Walloper", "Tesla Claw", "Glove of Doom", "Morph-o-Ray", "Hydrodisplacer", "R.Y.N.O.",
    "Drone Device", "Decoy Glove", "Trespasser", "Metal Detector", "Magneboots", "Grindboots", "Hoverboard", "Hologuise",
    "PDA", "Map-o-Matic", "Bolt Grabber", "Persuader", ""};
std::string item_name(int id) { return id >= 0 && id < ITEMS ? ITEM_NAMES[id] : ""; }
int item_kind(int id) { return r32(ITEM_ROWS + static_cast<uint32_t>(id) * 0x4c + 8); }
bool item_owned(int id) { return ee_read<uint8_t>(OWNED + static_cast<uint32_t>(id)) != 0; }
// in hand: owned, then asked for as the quick select does (item 0x18 has its own flag there)
void item_equip(int id) {
    if (!item_owned(id)) item_give_quiet(id, 0);
    if (id == 0x18) ee_write<uint8_t>(P + 0x1ff5, 1);
    else w32(ITEM_REQUEST, id);
}

// --- the free camera ---
struct FreeCam {
    bool on = false;
    Vec3 eye;
    float yaw = 0, pitch = 0;
    float speed = 15.0f;
    double last = 0;
} g_cam;

void hero_move_set(bool on);
void freecam_start() {
    hero_move_set(false);
    g_cam.eye = rv(CAMERA);
    const Vec3 h = rv(POS);
    const float dx = h.x - g_cam.eye.x, dy = h.y - g_cam.eye.y, dz = h.z - g_cam.eye.z;
    g_cam.yaw = std::atan2(dy, dx);
    g_cam.pitch = std::atan2(dz, std::sqrt(dx * dx + dy * dy));
    g_cam.last = glfwGetTime();
    g_cam.on = true;
}
void freecam_stop() {
    g_cam.on = false;
    g_view_debug = false;
}
Vec3 freecam_dir() {
    return {std::cos(g_cam.pitch) * std::cos(g_cam.yaw), std::cos(g_cam.pitch) * std::sin(g_cam.yaw), std::sin(g_cam.pitch)};
}
// WASD / left stick move, Q / E (L1 / R1) down / up, IJKL / arrows / right stick look, shift faster
void freecam_update(GLFWwindow* win) {
    const double now = glfwGetTime();
    const float dt = static_cast<float>(std::min(now - g_cam.last, 0.1));
    g_cam.last = now;
    float mx = 0, my = 0, mz = 0, lx = 0, ly = 0;
    const bool keys = !ImGui::GetIO().WantCaptureKeyboard;
    auto key = [&](int k) { return keys && glfwGetKey(win, k) == GLFW_PRESS; };
    if (key(GLFW_KEY_W)) my += 1;
    if (key(GLFW_KEY_S)) my -= 1;
    if (key(GLFW_KEY_D)) mx += 1;
    if (key(GLFW_KEY_A)) mx -= 1;
    if (key(GLFW_KEY_E)) mz += 1;
    if (key(GLFW_KEY_Q)) mz -= 1;
    if (key(GLFW_KEY_J) || key(GLFW_KEY_LEFT)) lx -= 1;
    if (key(GLFW_KEY_L) || key(GLFW_KEY_RIGHT)) lx += 1;
    if (key(GLFW_KEY_I) || key(GLFW_KEY_UP)) ly += 1;
    if (key(GLFW_KEY_K) || key(GLFW_KEY_DOWN)) ly -= 1;
    float boost = key(GLFW_KEY_LEFT_SHIFT) ? 4.0f : 1.0f;
    GLFWgamepadstate gp;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gp)) {
        auto dz = [](float v) { return std::fabs(v) < 0.15f ? 0.0f : v; };
        mx += dz(gp.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        my -= dz(gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        lx += dz(gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_X]);
        ly -= dz(gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_Y]);
        if (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS) mz += 1;
        if (gp.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS) mz -= 1;
        if (gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f) boost = 4.0f;
    }
    g_cam.yaw -= lx * 2.0f * dt;
    g_cam.pitch = std::clamp(g_cam.pitch + ly * 1.5f * dt, -1.5f, 1.5f);
    const Vec3 f = freecam_dir();
    const Vec3 r{std::sin(g_cam.yaw), -std::cos(g_cam.yaw), 0.0f};
    const float v = g_cam.speed * boost * dt;
    g_cam.eye.x += (f.x * my + r.x * mx) * v;
    g_cam.eye.y += (f.y * my + r.y * mx) * v;
    g_cam.eye.z += (f.z * my + mz) * v;
    g_view_debug = true;
    g_view_debug_eye = glm::vec3(g_cam.eye.x, g_cam.eye.y, g_cam.eye.z);
    g_view_debug_at = g_view_debug_eye + glm::vec3(f.x, f.y, f.z);
}

// --- moving the hero by hand (like the original's debug move): the stick moves him through the air
// and walls relative to the game camera, held there every game frame (no gravity); off, he falls ---
struct HeroMove {
    bool on = false;
    Vec3 pos;
    float yaw = 0;
    float speed = 10.0f;
    double last = 0;
    bool prev_key = false;
} g_move;

void freecam_stop();
void hero_move_set(bool on) {
    if (on == g_move.on) return;
    if (on) {
        if (g_cam.on) freecam_stop();
        g_move.pos = rv(POS);
        g_move.yaw = rf(ROT + 8);
        g_move.last = glfwGetTime();
    }
    g_move.on = on;
}
// WASD / left stick move (camera relative), E / Q or R1 / L1 up / down, Shift / R2 faster
void hero_move_update(GLFWwindow* win) {
    const double now = glfwGetTime();
    const float dt = static_cast<float>(std::min(now - g_move.last, 0.1));
    g_move.last = now;
    float mx = 0, my = 0, mz = 0;
    const bool keys = !ImGui::GetIO().WantCaptureKeyboard;
    auto key = [&](int k) { return keys && glfwGetKey(win, k) == GLFW_PRESS; };
    if (key(GLFW_KEY_W)) my += 1;
    if (key(GLFW_KEY_S)) my -= 1;
    if (key(GLFW_KEY_D)) mx += 1;
    if (key(GLFW_KEY_A)) mx -= 1;
    if (key(GLFW_KEY_E)) mz += 1;
    if (key(GLFW_KEY_Q)) mz -= 1;
    float boost = key(GLFW_KEY_LEFT_SHIFT) ? 4.0f : 1.0f;
    GLFWgamepadstate gp;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &gp)) {
        auto dz = [](float v) { return std::fabs(v) < 0.15f ? 0.0f : v; };
        mx += dz(gp.axes[GLFW_GAMEPAD_AXIS_LEFT_X]);
        my -= dz(gp.axes[GLFW_GAMEPAD_AXIS_LEFT_Y]);
        if (gp.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] == GLFW_PRESS) mz += 1;
        if (gp.buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] == GLFW_PRESS) mz -= 1;
        if (gp.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] > 0.1f) boost = 4.0f;
    }
    // the camera's heading on the ground plane (the camera towards the hero)
    const Vec3 c = rv(CAMERA);
    float fx = g_move.pos.x - c.x, fy = g_move.pos.y - c.y;
    const float fl = std::sqrt(fx * fx + fy * fy);
    if (fl > 0.01f) { fx /= fl; fy /= fl; } else { fx = std::cos(g_move.yaw); fy = std::sin(g_move.yaw); }
    const float rx = fy, ry = -fx;
    const float v = g_move.speed * boost * dt;
    const float dx = (fx * my + rx * mx) * v, dy = (fy * my + ry * mx) * v;
    g_move.pos.x += dx;
    g_move.pos.y += dy;
    g_move.pos.z += mz * v;
    if (dx != 0.0f || dy != 0.0f) g_move.yaw = std::atan2(dy, dx);
    hero_teleport(g_move.pos, &g_move.yaw);
}

// --- the state snapshot (EE RAM + scratchpad): the game's state lives there (the ports keep it at
// the original addresses); the host's (sound voices, render caches) is not in it ---
std::vector<uint8_t> g_snap_ram, g_snap_spr;
int g_snap_pending = 0;  // 1 save, 2 load (done between game frames)

// the weapon's ammo maximum: the table's +0xe, where pickups stop and what the HUD shows (+0x12 is the amount a purchase gives)
int ammo_max(int id) {
    const uint32_t row = AMMO_ROWS + static_cast<uint32_t>(id) * 0x18;
    if (ee_read<uint16_t>(row + 8) == 0) return 0;
    return ee_read<uint16_t>(row + 0xe);
}

// the group volumes from the options' sound / music (as the game's sound start does, 0x28ca88)
void apply_volumes() {
    const int32_t sfx = r32(SFX_VOL);
    w32(0x0013e59c, r32(MUSIC_VOL));
    w32(0x0013e598, sfx * 8 / 10);
    w32(0x0013e5a0, sfx * 8 / 10);
    w32(0x0013e5a4, sfx * 7 / 10);
    w32(0x0013e5a8, sfx * 7 / 10);
    w32(0x0013e5ac, sfx);
}

// (the window, view mode, field of view, v-sync and frame cap moved to Options > Graphics,
// src/game/enhance/graphics.h, which took them from this file once)
void display_save() {
    if (FILE* f = std::fopen(DISPLAY_FILE, "w")) {
        std::fprintf(f, "pipeline %d\n", g_settings.pipeline ? 1 : 0);
        std::fprintf(f, "game_thread %d\n", g_settings.game_thread ? 1 : 0);
        if (g_language >= 0) std::fprintf(f, "language %d\n", g_language);
        std::fprintf(f, "boot %d\n", g_boot);
        std::fclose(f);
    }
}

void tab_game() {
    DevSettings& s = g_settings;
    ImGui::Checkbox("Pause", &s.paused);
    ImGui::SameLine();
    if (ImGui::Button("Step one frame")) s.step_once = true;
    ImGui::Checkbox("Pause while the menu is open", &s.pause_when_open);
    ImGui::SliderFloat("Game speed", &s.speed, 0.1f, 4.0f, "%.2fx");
    ImGui::SameLine();
    if (ImGui::Button("1x")) s.speed = 1.0f;
    ImGui::Checkbox("Pad drives the menu while open", &s.block_game_input);
    ImGui::Separator();
    ImGui::Text("Level %d   mode %d   frame %d", r32(0x0015ed84), r32(OV(0x0015f5c4)), r32(OV(0x0015f5cc)));
    ImGui::Checkbox("Stats overlay", &s.show_stats);
    {  // the screen fade in one host frame (the default) or shown frame by frame (Options > Advanced > Fades)
        bool original = advanced_original_fades();
        if (ImGui::Checkbox("Original fades (the screen fades shown, as on the PS2)", &original)) advanced_set_original_fades(original);
    }
    ImGui::Separator();
    // the language, as the game's own Options > Language page sets it (its action 9: 0x15ed88)
    const int32_t lang = r32(LANGUAGE);
    int li = -1;
    for (int i = 0; i < static_cast<int>(std::size(LANGUAGES)); ++i)
        if (LANGUAGES[i].value == lang) li = i;
    ImGui::SetNextItemWidth(160);
    if (ImGui::BeginCombo("Language", li >= 0 ? LANGUAGES[li].name : "?")) {
        for (int i = 0; i < static_cast<int>(std::size(LANGUAGES)); ++i)
            if (ImGui::Selectable(LANGUAGES[i].name, i == li) && i != li) {
                w32(LANGUAGE, LANGUAGES[i].value);
                g_language = LANGUAGES[i].value;
                display_save();
            }
        ImGui::EndCombo();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("(0x15ed88 = %d)", lang);
    if (lang != g_language_loaded)
        ImGui::TextWrapped("Voices and the menus' pictures follow at once. The texts (menus, subtitles, help) are loaded with the "
                           "level (0x241940), in the language set then: they change at the next start.");
    ImGui::Separator();
    ImGui::TextUnformatted("State snapshot (experimental: the EE RAM + scratchpad, in memory)");
    if (ImGui::Button("Save state")) g_snap_pending = 1;
    ImGui::SameLine();
    ImGui::BeginDisabled(g_snap_ram.empty());
    if (ImGui::Button("Load state")) g_snap_pending = 2;
    ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled(g_snap_ram.empty() ? "(none saved)" : "(one saved)");
    ImGui::TextWrapped("Sounds playing and the renderer's caches are the host's, not in the snapshot.");
}

// --- Hoverboard anywhere (dev_hoverboard_apply: what it does) ---
constexpr int BOARD_CLASS = 439;
constexpr uint32_t BOARD_RIDDEN = P + 0x86c;  // the board the hero rides
constexpr uint32_t LEVEL = 0x0015ed84;
int g_board_pending = 0;     // 1 get on, 2 get off
bool g_board_on = false;     // on the board because of the menu
int32_t g_board_level = -1;  // the level it was asked on

// orig: (host) the levels with the board (their own hero controllers ride it)
bool hoverboard_level() { return on_rilgar() || on_kalebo(); }

// orig: (host) the nearest standing board (class 439, state 0) of the level, 0 if none
uint32_t hoverboard_find() {
    uint32_t best = 0;
    float best_d = 1e30f;
    const Vec3 hp = rv(POS);
    for_each_moby([&](uint32_t m) {
        if (moby_class(m) != BOARD_CLASS || ee_read<uint8_t>(m + 0x20) != 0) return;
        const float d = dist(rv(m + 0x10), hp);
        if (d < best_d) best = m, best_d = d;
    });
    return best;
}

// orig: (host) whether the hero rides a board (his board sub-state)
bool hoverboard_riding() { return ru(HERO_MOBY) != 0 && r32(P + 0x208c) == 0x16; }

void tab_player() {
    DevSettings& s = g_settings;
    int hp = r32(HEALTH), hpmax = r32(HEALTH_MAX), bolts = r32(BOLTS);
    if (ImGui::InputInt("Nanotech (health)", &hp)) w32(HEALTH, std::max(0, hp));
    if (ImGui::InputInt("Nanotech maximum", &hpmax)) w32(HEALTH_MAX, std::clamp(hpmax, 1, 80));
    if (ImGui::Button("Refill nanotech")) w32(HEALTH, r32(HEALTH_MAX));
    ImGui::SameLine();
    if (ImGui::Button("Max nanotech (80)")) { w32(HEALTH_MAX, 80); w32(HEALTH, 80); }
    if (ImGui::InputInt("Bolts", &bolts, 100, 1000)) w32(BOLTS, std::clamp(bolts, 0, 9999999));
    if (ImGui::Button("+1000 bolts")) w32(BOLTS, std::min(r32(BOLTS) + 1000, 9999999));
    ImGui::SameLine();
    if (ImGui::Button("+100000 bolts")) w32(BOLTS, std::min(r32(BOLTS) + 100000, 9999999));
    ImGui::Checkbox("God mode (nanotech held at its maximum)", &s.god_mode);
    ImGui::Checkbox("Infinite bolts", &s.infinite_bolts);
    ImGui::SameLine();
    if (ImGui::Button("Kill Ratchet")) dev_kill_hero();
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("The game's own death: nanotech 0 and the hero in his death state (0x3d, what the locomotion's check sets\n"
                          "when the nanotech is gone); the animation plays, then the game's lost-hero path fades out and reloads the checkpoint.");
    ImGui::Separator();
    if (ru(HERO_MOBY) == 0) {
        ImGui::TextDisabled("No hero moby yet.");
        return;
    }
    {  // Hoverboard anywhere (dev_hoverboard_apply)
        const bool level_ok = hoverboard_level();
        const bool free_board = level_ok && hoverboard_find() != 0;
        bool board = dev_hoverboard_on();
        ImGui::BeginDisabled(!board && !free_board);
        if (ImGui::Checkbox("Hoverboard anywhere", &board)) dev_hoverboard_set(board);
        ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled))
            ImGui::SetTooltip("Gets on the hoverboard race's board where Ratchet stands, as the board itself does when he walks up\n"
                              "to it, and off again when unticked. Only on Rilgar and Kalebo III: the other levels' hero code has no\n"
                              "board state and their files no board. The race's own rules still hold: on Rilgar the board wipes out\n"
                              "away from the race's track and puts him back on it; on Kalebo III anywhere above the water is fine.");
        ImGui::SameLine();
        if (!level_ok) ImGui::TextDisabled("(only on Rilgar and Kalebo III, the levels with the race's board)");
        else if (!board && !free_board) ImGui::TextDisabled("(the level's board is in use)");
    }
    bool mv = g_move.on;
    if (ImGui::Checkbox("Move Ratchet (F2 / L3)", &mv)) hero_move_set(mv);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(160);
    ImGui::SliderFloat("Move speed", &g_move.speed, 1.0f, 60.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    if (g_move.on)
        ImGui::TextDisabled("WASD / left stick move (from the camera), E Q / R1 L1 up / down, Shift / R2 faster; off: he falls.");
    ImGui::Text("State %d   mode %d   ground mode %d", r32(P + 0x2084), r32(P + 0x208c), ee_read<uint8_t>(P + 0x20a4));
    const Vec3 v = rv(VEL);
    ImGui::Text("Velocity %.3f %.3f %.3f", v.x, v.y, v.z);
    // the position: edited, then applied as a teleport
    static Vec3 edit;
    static bool editing = false;
    if (!editing) edit = rv(POS);
    ImGui::SetNextItemWidth(300);
    ImGui::DragFloat3("Position", &edit.x, 0.1f, -10000.0f, 10000.0f, "%.2f");
    editing = ImGui::IsItemActive();
    if (ImGui::IsItemDeactivatedAfterEdit()) hero_teleport(edit);
    float yaw = rf(ROT + 8);
    ImGui::SetNextItemWidth(300);
    if (ImGui::SliderAngle("Facing", &yaw, -180.0f, 180.0f)) hero_teleport(rv(POS), &yaw);
    ImGui::TextUnformatted("Nudge:");
    static float step = 2.0f;
    const Vec3 p = rv(POS);
    ImGui::SameLine(); if (ImGui::Button("+X")) hero_teleport({p.x + step, p.y, p.z});
    ImGui::SameLine(); if (ImGui::Button("-X")) hero_teleport({p.x - step, p.y, p.z});
    ImGui::SameLine(); if (ImGui::Button("+Y")) hero_teleport({p.x, p.y + step, p.z});
    ImGui::SameLine(); if (ImGui::Button("-Y")) hero_teleport({p.x, p.y - step, p.z});
    ImGui::SameLine(); if (ImGui::Button("Up")) hero_teleport({p.x, p.y, p.z + step});
    ImGui::SameLine(); if (ImGui::Button("Down")) hero_teleport({p.x, p.y, p.z - step});
    ImGui::SameLine(); ImGui::SetNextItemWidth(90); ImGui::InputFloat("##step", &step, 0, 0, "%.1f");
    ImGui::SameLine(); if (ImGui::Button("Forward")) {
        const float y = rf(ROT + 8);
        hero_teleport({p.x + std::cos(y) * step, p.y + std::sin(y) * step, p.z});
    }
    const Vec3 cp = rv(CHECKPOINT);
    ImGui::Text("Checkpoint %.1f %.1f %.1f", cp.x, cp.y, cp.z);
    ImGui::SameLine();
    if (ImGui::Button("Go##cp")) { const float y = rf(CHECKPOINT_ROT + 8); hero_teleport(cp, &y); }
    ImGui::SameLine();
    if (ImGui::Button("Set here")) { wv(CHECKPOINT, rv(POS)); wv(CHECKPOINT_ROT, rv(ROT)); }
    const Vec3 cam = rv(CAMERA);
    ImGui::Text("Camera %.1f %.1f %.1f", cam.x, cam.y, cam.z);
    ImGui::SameLine();
    if (ImGui::Button("Hero to the camera")) hero_teleport(g_cam.on ? g_cam.eye : cam);
    ImGui::Separator();
    ImGui::TextUnformatted("Saved positions (kept in devmenu_positions.txt)");
    for (int i = 0; i < static_cast<int>(g_saved_pos.size()); ++i) {
        SavedPos& sp = g_saved_pos[static_cast<size_t>(i)];
        ImGui::PushID(i);
        if (ImGui::Button("Save")) { sp = {true, rv(POS), rf(ROT + 8)}; positions_save(); }
        ImGui::SameLine();
        ImGui::BeginDisabled(!sp.set);
        if (ImGui::Button("Go")) hero_teleport(sp.p, &sp.yaw);
        ImGui::EndDisabled();
        ImGui::SameLine();
        if (sp.set) ImGui::Text("%d: %.1f %.1f %.1f", i + 1, sp.p.x, sp.p.y, sp.p.z);
        else ImGui::TextDisabled("%d: -", i + 1);
        ImGui::PopID();
    }
}

void tab_mobys() {
    Freeze& f = g_freeze;
    bool changed = false;
    changed |= ImGui::Checkbox("Freeze enemies", &f.enemies);
    ImGui::SameLine();
    changed |= ImGui::Checkbox("Freeze all mobys (not the hero's)", &f.all);
    ImGui::SameLine();
    if (ImGui::Button("Unfreeze")) { f = {}; changed = true; }
    if (ImGui::Button("Retire all enemies")) for_each_moby([](uint32_t m) { if (moby_live(m) && is_enemy_class(moby_class(m))) moby_retire(m); });
    ImGui::SameLine();
    ImGui::TextDisabled("enemy classes: 749, 1440");
    // the classes alive, with their counts
    struct ClassInfo { int alive = 0; int all = 0; uint32_t fn = 0; };
    std::map<int, ClassInfo> classes;
    for_each_moby([&](uint32_t m) {
        ClassInfo& c = classes[moby_class(m)];
        ++c.all;
        if (moby_live(m)) ++c.alive;
        if (c.fn == 0) c.fn = ru(m + 0x74);
    });
    if (ImGui::CollapsingHeader("Classes")) {
        if (ImGui::BeginTable("classes", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 200))) {
            ImGui::TableSetupColumn("Class");
            ImGui::TableSetupColumn("Alive / all");
            ImGui::TableSetupColumn("Update");
            ImGui::TableSetupColumn("Frozen");
            ImGui::TableSetupColumn("");
            ImGui::TableHeadersRow();
            for (const auto& [cls, c] : classes) {
                ImGui::PushID(cls);
                ImGui::TableNextRow();
                ImGui::TableNextColumn(); ImGui::Text("%d%s", cls, is_enemy_class(cls) ? " (enemy)" : cls == 0 ? " (hero)" : "");
                ImGui::TableNextColumn(); ImGui::Text("%d / %d", c.alive, c.all);
                ImGui::TableNextColumn();
                if (c.fn == 0) ImGui::TextDisabled("-");
                else ImGui::Text("%06x%s", c.fn, ovl_moby_fn_ported(c.fn) ? "" : " (unported)");
                ImGui::TableNextColumn();
                bool fr = f.classes.count(cls) != 0;
                if (ImGui::Checkbox("##fr", &fr)) { if (fr) f.classes.insert(cls); else f.classes.erase(cls); changed = true; }
                ImGui::TableNextColumn();
                if (cls != 0 && ImGui::SmallButton("Retire")) {
                    const int want = cls;
                    for_each_moby([want](uint32_t m) { if (moby_live(m) && moby_class(m) == want && !hero_owned(m)) moby_retire(m); });
                }
                ImGui::PopID();
            }
            ImGui::EndTable();
        }
    }
    // the mobys near the hero
    static float radius = 30.0f;
    static bool only_live = true;
    ImGui::SetNextItemWidth(200);
    ImGui::SliderFloat("Within", &radius, 5.0f, 500.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::SameLine();
    ImGui::Checkbox("Alive only", &only_live);
    const Vec3 hp = rv(POS);
    struct Row { uint32_t m; float d; };
    std::vector<Row> rows;
    for_each_moby([&](uint32_t m) {
        if (only_live && !moby_live(m)) return;
        const float d = dist(rv(m + 0x10), hp);
        if (d <= radius) rows.push_back({m, d});
    });
    std::sort(rows.begin(), rows.end(), [](const Row& a, const Row& b) { return a.d < b.d; });
    ImGui::Text("%zu mobys", rows.size());
    if (ImGui::BeginTable("mobys", 6, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 260))) {
        ImGui::TableSetupColumn("Moby");
        ImGui::TableSetupColumn("Class");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Dist");
        ImGui::TableSetupColumn("Frozen");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (const Row& r : rows) {
            const uint32_t m = r.m;
            ImGui::PushID(static_cast<int>(m));
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%08x%s", m, hero_owned(m) ? " *" : "");
            ImGui::TableNextColumn(); ImGui::Text("%d", moby_class(m));
            ImGui::TableNextColumn(); ImGui::Text("%d", static_cast<int8_t>(ee_read<uint8_t>(m + 0x20)));
            ImGui::TableNextColumn(); ImGui::Text("%.1f", r.d);
            ImGui::TableNextColumn();
            bool fr = f.mobys.count(m) != 0;
            if (ImGui::Checkbox("##fr", &fr)) { if (fr) f.mobys.insert(m); else f.mobys.erase(m); changed = true; }
            ImGui::TableNextColumn();
            if (ImGui::SmallButton("Go to")) { Vec3 t = rv(m + 0x10); t.z += 2.0f; hero_teleport(t); }
            if (!hero_owned(m) && moby_live(m)) {
                ImGui::SameLine();
                if (ImGui::SmallButton("Retire")) moby_retire(m);
            }
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
    if (changed) update_freeze_hook();
}

void tab_camera() {
    bool on = g_cam.on;
    if (ImGui::Checkbox("Free camera", &on)) { if (on) freecam_start(); else freecam_stop(); }
    ImGui::SliderFloat("Speed", &g_cam.speed, 1.0f, 100.0f, "%.0f", ImGuiSliderFlags_Logarithmic);
    ImGui::TextWrapped("WASD / left stick move, Q E / L1 R1 down / up, IJKL / arrows / right stick look, "
                       "Shift / R2 faster. The pad drives the camera, not the hero, while it is on "
                       "(Pause in Game stops the world meanwhile). The game still culls from its own camera.");
    if (g_cam.on) {
        ImGui::Text("Eye %.1f %.1f %.1f", g_cam.eye.x, g_cam.eye.y, g_cam.eye.z);
        if (ImGui::Button("Hero here")) hero_teleport(g_cam.eye);
        ImGui::SameLine();
        if (ImGui::Button("Back to the hero")) freecam_start();
    }
    const Vec3 c = rv(CAMERA);
    ImGui::Text("Game camera %.1f %.1f %.1f", c.x, c.y, c.z);
}

void tab_items() {
    DevSettings& s = g_settings;
    ImGui::Checkbox("Infinite ammo", &s.infinite_ammo);
    if (ImGui::Button("Give all items")) {
        for (int id = 1; id < ITEMS; ++id) if (!item_owned(id)) item_give_quiet(id, 0);
    }
    ImGui::SameLine();
    if (ImGui::Button("Refill ammo")) {
        for (int id = 0; id < ITEMS; ++id) if (ammo_max(id) > 0) w32(AMMO + static_cast<uint32_t>(id) * 4, ammo_max(id));
    }
    ImGui::SameLine();
    if (ImGui::Button("Take all but the wrench")) {
        for (int id = 1; id < ITEMS; ++id) if (id != 8) ee_write<uint8_t>(OWNED + static_cast<uint32_t>(id), 0);
    }
    const int in_hand = r32(P + 0x10b8);
    ImGui::Text("In hand %d %s   selected %d   asked for %d", in_hand, item_name(in_hand).c_str(), r32(0x00141660), r32(ITEM_REQUEST));
    ImGui::TextDisabled("Equip: owned, then asked for in hand as the quick select does (kind 0 items).");
    if (ImGui::BeginTable("items", 5, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 380))) {
        ImGui::TableSetupColumn("Item");
        ImGui::TableSetupColumn("Kind");
        ImGui::TableSetupColumn("Owned");
        ImGui::TableSetupColumn("Ammo");
        ImGui::TableSetupColumn("");
        ImGui::TableHeadersRow();
        for (int id = 1; id < ITEMS; ++id) {
            ImGui::PushID(id);
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            const std::string name = item_name(id);
            if (id == in_hand) ImGui::TextColored(ImVec4(1.0f, 0.85f, 0.3f, 1.0f), "0x%02x %s", id, name.c_str());
            else ImGui::Text("0x%02x %s", id, name.c_str());
            ImGui::TableNextColumn();
            ImGui::Text("%d", item_kind(id));
            ImGui::TableNextColumn();
            bool owned = item_owned(id);
            if (ImGui::Checkbox("##own", &owned)) {
                if (owned) item_give_quiet(id, 0);
                else ee_write<uint8_t>(OWNED + static_cast<uint32_t>(id), 0);
            }
            ImGui::TableNextColumn();
            if (ammo_max(id) > 0) {
                int a = r32(AMMO + static_cast<uint32_t>(id) * 4);
                ImGui::SetNextItemWidth(120);
                if (ImGui::SliderInt("##ammo", &a, 0, ammo_max(id))) w32(AMMO + static_cast<uint32_t>(id) * 4, a);
            }
            ImGui::TableNextColumn();
            if (item_kind(id) == 0 && ImGui::SmallButton("Equip")) item_equip(id);
            ImGui::PopID();
        }
        ImGui::EndTable();
    }
}

// the renderer: the Modern Rendering Pipeline (what the player gets, with all its optimisations) or the VU
// emulation (the world, the sky, the mobys and the effects by the original VU1 programs, ported, and the GS replica: the
// faithful reference)
void renderer_set(bool mrp) {
    g_world_mrp = mrp;
    g_world_faithful = !mrp;
    g_moby_renderer = mrp ? MobyRenderer::kMrp : MobyRenderer::kFaithful;
    g_moby_vu1_pass = true;
}

void tab_rendering() {
    const bool is_vk = rac1::vk::is_active();
    ImGui::SeparatorText("Graphics API");
    ImGui::Text("%s (Options > Graphics > Renderer, RAC1_RENDERER: from the next start)", is_vk ? "Vulkan 1.3" : "OpenGL 4.6");
    const bool mrp = g_world_mrp && g_moby_renderer == MobyRenderer::kMrp;
    const bool vu = g_world_faithful && !g_world_mrp && g_moby_renderer == MobyRenderer::kFaithful;
    ImGui::SeparatorText("Renderer");
    if (ImGui::RadioButton("Modern Rendering Pipeline (MRP)", mrp)) renderer_set(true);
    ImGui::SameLine();
    if (ImGui::RadioButton("VU emulation (faithful reference)", vu)) renderer_set(false);
    if (!mrp && !vu) ImGui::TextDisabled("Another combination, set by the testing variables (RAC1_MRP=0, RAC1_WORLD_MRP, RAC1_MOBY_*...).");
    ImGui::SeparatorText("Layers");
    ImGui::Checkbox("Terrain (tfrag)", &g_render_tfrag);
    ImGui::SameLine(220);
    ImGui::Checkbox("Instances (tie)", &g_render_tie);
    ImGui::SameLine(440);
    ImGui::Checkbox("Foliage (shrub)", &g_render_shrub);
    ImGui::Checkbox("Mobys", &g_render_moby);
    ImGui::SameLine(220);
    ImGui::Checkbox("Sky", &g_render_sky);
    ImGui::SameLine(440);
    ImGui::Checkbox("Effects (particles)", &g_render_fx);
    ImGui::Checkbox("2D (the game's GS packets: HUD, text)", &g_render_gs2d);
    if (mrp && ImGui::CollapsingHeader("MRP options")) {
        const char* projs[] = {"World projection (one depth space with the MRP mobys)", "Original VU1 projection (4:3 frame)"};
        ImGui::Combo("World projection", &g_mrp_world_projection, projs, 2);
        const char* mips[] = {"Faithful mip (the GS's level)", "Full texture (every mip, trilinear)"};
        ImGui::Combo("World textures", &g_mrp_world_mip, mips, 2);
        bool fog = g_mrp_world_fog != 0;
        if (ImGui::Checkbox("World fog (the GS's FGE / FOGCOL)", &fog)) g_mrp_world_fog = fog ? 1 : 0;
        ImGui::Combo("Moby projection", &g_mrp_moby_projection, projs, 2);
        ImGui::Combo("Moby textures", &g_mrp_moby_mip, mips, 2);
        bool mrp_fog = g_mrp_moby_fog != 0;
        if (ImGui::Checkbox("Moby fog (the GS's FGE / FOGCOL)", &mrp_fog)) g_mrp_moby_fog = mrp_fog ? 1 : 0;
        bool mrp_sh = g_mrp_shadows != 0;
        if (ImGui::Checkbox("Drop shadows", &mrp_sh)) g_mrp_shadows = mrp_sh ? 1 : 0;
        // the particles: the MRP's (world projection, resident textures, a draw or two) or the GS replica's
        ImGui::Checkbox("MRP effects (off: the GS replica draws the particles)", &g_mrp_fx);
        // the level of detail slider: set in Options > Graphics
        ImGui::TextDisabled("Level of detail %d (Options > Graphics): distances x%.1f.", g_mrp_lod, mrp_lod_scale(g_mrp_lod));
        // the widescreen's side bands: what the 4:3 game culls there, drawn by the shadow runs
        ImGui::Checkbox("Fill the widescreen sides (costs CPU)", &g_mrp_wide_fill);
        ImGui::TextDisabled(g_mrp_wide_fill ? "Widescreen: the world, mobys and effects beyond the 4:3 view drawn (~11 ms of CPU a frame at 5120 x 1440)."
                                            : "Widescreen: the sides show only what the 4:3 game draws (objects may be missing there).");
    }
    ImGui::SeparatorText("Debug views");
    ImGui::Checkbox("Wireframe", &g_render_wireframe);
    const char* shades[] = {"Normal", "Lighting only (vertex colours)", "Textures only (no lighting)", "Flat"};
    if (ImGui::Combo("Shading", &g_render_shade, shades, 4) && !is_vk) ShaderManager::get().set_debug_shade(g_render_shade);
}

void tab_display() {
    DevSettings& s = g_settings;
    int w = 0, h = 0, fw = 0, fh = 0;
    glfwGetWindowSize(g_window, &w, &h);
    glfwGetFramebufferSize(g_window, &fw, &fh);
    ImGui::Text("Window %dx%d  (framebuffer %dx%d)", w, h, fw, fh);
    ImGui::TextDisabled("Display mode, window size, render resolution, aspect ratio, level of detail, field of view,\n"
                        "v-sync and frame cap: the game's Options > Graphics.");
    if (ImGui::Checkbox("Pipelined frames (faster, one frame of latency)", &s.pipeline)) display_save();
    ImGui::BeginDisabled(!s.pipeline);
    if (ImGui::Checkbox("Game update on its own thread (beside the pipelined frame's drawing)", &s.game_thread)) display_save();
    ImGui::EndDisabled();
    ImGui::TextDisabled("The world of a frame is built while the next game frame runs, then drawn (RAC1_PIPELINE=0 / 1).");
    const float tx = rf(OV(0x0016caf0)), ty = rf(OV(0x0016caf4));
    if (tx > 0.0f && ty > 0.0f)  // the x tangent with the widening in it (render_view_projection)
        ImGui::Text("View mode %d: horizontal %.1f deg, vertical %.1f deg  (game tangents %.3f / %.3f, widened x%.2f, fov x%.2f)", g_view_mode,
                    2.0f * std::atan(tx) * 57.29578f, 2.0f * std::atan(ty) * 57.29578f, tx, ty, g_view_widen, g_view_fov_scale);
    if (g_world_mrp && mrp_lod_frame_widen() > 1.0f)
        ImGui::TextDisabled("MRP: the world, sky and mobys the game culled at its 4:3 sides are drawn (x%.2f).", mrp_lod_frame_widen());
}

void tab_audio() {
    ImGui::SliderFloat("Output gain", &g_snd989_gain, 0.0f, 2.0f);
    ImGui::SameLine();
    if (ImGui::Button(g_snd989_gain == 0.0f ? "Unmute" : "Mute")) g_snd989_gain = g_snd989_gain == 0.0f ? 1.0f : 0.0f;
    ImGui::SliderFloat("Movie volume", &g_movie_volume, 0.0f, 2.0f);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("A factor on the movies' sound (1 = as the original's 989snd voices make it: group 5, the sound effects slider,\n"
                          "pan centre; the soundtracks are mastered louder than the game's sounds). Not part of the original.");
    int sfx = r32(SFX_VOL), mus = r32(MUSIC_VOL);
    if (ImGui::SliderInt("Sound effects (option)", &sfx, 0, 0x400)) { w32(SFX_VOL, sfx); apply_volumes(); }
    if (ImGui::SliderInt("Music (option)", &mus, 0, 0x400)) { w32(MUSIC_VOL, mus); apply_volumes(); }
    ImGui::Checkbox("Trace 989snd commands (stderr)", &g_snd989_trace);
    ImGui::Text("Sounds playing: %d   bank %08x", snd989_iop_active_sounds(), ee_read<uint32_t>(0x0015ed5c));
    if (ImGui::BeginTable("slots", 4, ImGuiTableFlags_RowBg | ImGuiTableFlags_ScrollY, ImVec2(0, 240))) {
        ImGui::TableSetupColumn("Slot");
        ImGui::TableSetupColumn("State");
        ImGui::TableSetupColumn("Sound");
        ImGui::TableSetupColumn("Handle");
        ImGui::TableHeadersRow();
        for (uint32_t i = 0; i < 30; ++i) {
            const uint32_t e = 0x0013e5c0 + i * 0x70;
            const uint8_t st = ee_read<uint8_t>(e + 4);
            if (st == 0) continue;
            const uint32_t def = ee_read<uint32_t>(e + 8);
            ImGui::TableNextRow();
            ImGui::TableNextColumn(); ImGui::Text("%u", i);
            ImGui::TableNextColumn(); ImGui::Text("%u", st);
            ImGui::TableNextColumn(); ImGui::Text("%d", def != 0 ? ee_read<int16_t>(def + 0x1a) : -1);
            ImGui::TableNextColumn(); ImGui::Text("%08x", ee_read<uint32_t>(e));
        }
        ImGui::EndTable();
    }
}

// The planets (level numbers 0..18) and how much of each the port can run.
const char* const PLANETS[19] = {"Veldin (tutorial)", "Novalis", "Aridia", "Kerwan (Metropolis)", "Eudora", "Rilgar", "Nebula G34",
                                 "Umbris", "Batalia", "Gaspar", "Orxon", "Pokitaru", "Hoven", "Oltanis Orbit", "Oltanis",
                                 "Quartu", "Kalebo III", "Veldin Orbit", "Veldin (finale)"};

// the summary lines of tools/overlay_match's map of a level (assets/generated/overlay_map)
std::vector<std::string> overlay_map_summary(int level) {
    static std::map<int, std::vector<std::string>> cache;
    auto it = cache.find(level);
    if (it != cache.end()) return it->second;
    std::vector<std::string> out;
    const std::string path = "assets/generated/overlay_map/level" + std::to_string(level) + ".txt";
    if (FILE* f = std::fopen(path.c_str(), "r")) {
        char line[512];
        while (std::fgets(line, sizeof line, f) != nullptr && line[0] == '#') {
            std::string s(line + 2);
            while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
            if (s.find("found once") != std::string::npos || s.find("class table") != std::string::npos ||
                s.find("ported overlay") != std::string::npos)
                out.push_back(s);
        }
        std::fclose(f);
    }
    cache[level] = out;
    return out;
}

void tab_levels() {
    const int32_t cur = r32(0x0015ed84);
    ImGui::Text("Level %d: %s   game mode %d", cur, cur >= 0 && cur < 19 ? PLANETS[cur] : "?", r32(OV(0x0015f5c4)));
    ImGui::TextWrapped(
        "Going to a planet runs the game's own level change: the request 0x24d430 (the level's state into the save game), "
        "then 0x28eeb0 (the next level 0x15f5c0, the main loop left); the level exit 0x292e08 loads it behind the loading "
        "screen (the ship's flight, drawn by 0x2900a0) and the level starts as at boot.");
    ImGui::TextWrapped(
        "What runs: the ported code is Veldin's build of the engine. Every level has its own overlay; ~80%% of Veldin's "
        "functions are the same code in each, but the level's own code and tables are not (tools/overlay_match). "
        "The host loads Veldin's overlay image for every level: the level's geometry, textures, statics, mobys and its "
        "gameplay data are its own; mobys whose class is missing from Veldin's class table (0x1ea300) have no update "
        "function and stay inert; unported update functions are logged once (TODO_PORT).");
    ImGui::Separator();
    static int selected = 3;
    if (ImGui::BeginTable("planets", 3, ImGuiTableFlags_RowBg | ImGuiTableFlags_BordersInnerV | ImGuiTableFlags_ScrollY, ImVec2(0, 260))) {
        ImGui::TableSetupColumn("#", ImGuiTableColumnFlags_WidthFixed, 24);
        ImGui::TableSetupColumn("Planet");
        ImGui::TableSetupColumn("Disc data", ImGuiTableColumnFlags_WidthFixed, 110);
        ImGui::TableHeadersRow();
        for (int i = 0; i < 19; ++i) {
            ImGui::TableNextRow();
            ImGui::TableNextColumn();
            ImGui::Text("%d", i);
            ImGui::TableNextColumn();
            if (ImGui::Selectable(PLANETS[i], selected == i, ImGuiSelectableFlags_SpanAllColumns)) selected = i;
            ImGui::TableNextColumn();
            static int have[19] = {};
            if (have[i] == 0) have[i] = disc_has_level(i) ? 1 : 2;
            ImGui::TextUnformatted(have[i] == 1 ? "ready" : "missing");
        }
        ImGui::EndTable();
    }
    for (const std::string& s : overlay_map_summary(selected)) ImGui::BulletText("%s", s.c_str());
    if (selected != 0 && overlay_map_summary(selected).empty())
        ImGui::TextDisabled("(run setup.sh for assets/generated/overlay_map: the level's overlay compared)");
    const bool busy = r32(OV(0x0015f570)) != 0;
    if (busy) ImGui::BeginDisabled();
    if (boot_active()) {  // the game's own start: its title left for the planet, as a loaded game leaves it
        if (ImGui::Button("Skip the boot: go to this planet")) boot_skip_to(selected);
    } else if (ImGui::Button(selected == cur ? "Reload this planet" : "Go to this planet")) {
        level_change_request(0, selected);  // 0x24d430: the level's state saved (as the ship's map does)
        level_exit_to(selected);            // 0x28eeb0: the next level, the main loop left
    }
    if (busy) ImGui::EndDisabled();
    ImGui::SameLine();
    ImGui::TextDisabled("(testing: RAC1_LEVEL=N starts on level N)");
    ImGui::Separator();
    bool boot = g_boot != 0;
    if (ImGui::Checkbox("Start with the game's boot (intro movie, title, main menu)", &boot)) {
        g_boot = boot ? 1 : 0;
        display_save();
    }
    ImGui::TextDisabled("(at the next start; off: straight to the level, as RAC1_BOOT=0; a level asked with --level / RAC1_LEVEL skips it too)");
}

// (step 3) the lights placed by hand: the level's list, a light added at Ratchet or the camera, edited, saved
// (data/lights/level<N>.txt: our own art choices, not the game's); markers on the screen, the selected light pulsing
int g_light_sel = -1;
int g_light_level = -1;
bool g_light_markers = true;
bool g_light_dirty = false;
int g_class_sel = -1;  // (the lights per object class: the selected one)
void lights_markers(const std::vector<UserLight>& L) {
    int vp[4];
    if (!mrp_world_last_viewport(vp) || g_window == nullptr) return;
    float m[16];
    mrp_world_view_proj(m);
    const glm::mat4 VP = glm::make_mat4(m);
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(g_window, &fw, &fh);
    const ImGuiIO& io = ImGui::GetIO();
    const float rs = graphics_render_scale() > 0.0f ? graphics_render_scale() : 1.0f;
    const float sx = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f, sy = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    for (size_t i = 0; i < L.size(); ++i) {
        const glm::vec4 c = VP * glm::vec4(L[i].pos[0], L[i].pos[1], L[i].pos[2], 1.0f);
        if (c.w < 0.05f) continue;
        const float nx = c.x / c.w, ny = c.y / c.w;
        if (nx < -1.2f || nx > 1.2f || ny < -1.2f || ny > 1.2f) continue;
        const float x = (static_cast<float>(vp[0]) + (nx * 0.5f + 0.5f) * static_cast<float>(vp[2])) / rs;
        const float yb = (static_cast<float>(vp[1]) + (ny * 0.5f + 0.5f) * static_cast<float>(vp[3])) / rs;
        const ImVec2 p(x / sx, (static_cast<float>(fh) - yb) / sy);
        const bool sel = static_cast<int>(i) == g_light_sel;
        const ImU32 col = IM_COL32(static_cast<int>(L[i].col[0] * 255.0f), static_cast<int>(L[i].col[1] * 255.0f), static_cast<int>(L[i].col[2] * 255.0f), 255);
        dl->AddCircleFilled(p, sel ? 7.0f : 5.0f, col);
        dl->AddCircle(p, sel ? 11.0f : 7.0f, sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(0, 0, 0, 200), 0, 2.0f);
        char t[8];
        std::snprintf(t, sizeof t, "%zu", i);
        dl->AddText(ImVec2(p.x + 9.0f, p.y - 8.0f), IM_COL32(255, 255, 255, 230), t);
    }
}
// The sun editor: the level's own sun direction (data/lights/levelNN.txt, "sun <azimuth> <elevation>"), applied at once while
// playing; Save writes the level's file (its comments, the lights placed by hand and the classes turned off kept)
bool g_sun_dirty = false;
int g_sun_level = -1;
bool g_interior_show = true;
int g_interior_sel = -1;
// the interiors' boxes drawn over the picture (their edges projected with the world's view), the selected one white
void interior_boxes(const std::vector<SunInterior>& B) {
    int vp[4];
    if (!mrp_world_last_viewport(vp) || g_window == nullptr) return;
    float m[16];
    mrp_world_view_proj(m);
    const glm::mat4 VP = glm::make_mat4(m);
    int fw = 0, fh = 0;
    glfwGetFramebufferSize(g_window, &fw, &fh);
    const ImGuiIO& io = ImGui::GetIO();
    const float rs = graphics_render_scale() > 0.0f ? graphics_render_scale() : 1.0f;
    const float sx = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f, sy = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    ImDrawList* dl = ImGui::GetForegroundDrawList();
    auto proj = [&](const glm::vec3& w, ImVec2& p) {
        const glm::vec4 c = VP * glm::vec4(w, 1.0f);
        if (c.w < 0.05f) return false;
        const float x = (static_cast<float>(vp[0]) + (c.x / c.w * 0.5f + 0.5f) * static_cast<float>(vp[2])) / rs;
        const float yb = (static_cast<float>(vp[1]) + (c.y / c.w * 0.5f + 0.5f) * static_cast<float>(vp[3])) / rs;
        p = ImVec2(x / sx, (static_cast<float>(fh) - yb) / sy);
        return true;
    };
    for (size_t i = 0; i < B.size(); ++i) {
        glm::vec3 c[8];
        for (int k = 0; k < 8; ++k) c[k] = glm::vec3(k & 1 ? B[i].hi[0] : B[i].lo[0], k & 2 ? B[i].hi[1] : B[i].lo[1], k & 4 ? B[i].hi[2] : B[i].lo[2]);
        const ImU32 col = static_cast<int>(i) == g_interior_sel ? IM_COL32(255, 255, 255, 255) : IM_COL32(255, 160, 40, 220);
        for (int a = 0; a < 8; ++a)
            for (int bit = 1; bit < 8; bit <<= 1) {
                const int b = a | bit;
                if (b == a) continue;
                ImVec2 p0, p1;
                if (proj(c[a], p0) && proj(c[b], p1)) dl->AddLine(p0, p1, col, 2.0f);
            }
    }
}
// the level's ambient particles ("particles" lines of its file: mrp_particles.h), edited live, saved with the file
void sun_particles() {
    ImGui::Separator();
    ImGui::Text("Ambient particles (ours; Options > Graphics > Ambient Particles)");
    int q = g_ambient_particles;
    bool ch = ImGui::RadioButton("Off", &q, 0);
    ImGui::SameLine();
    ch |= ImGui::RadioButton("Low", &q, 1);
    ImGui::SameLine();
    ch |= ImGui::RadioButton("High", &q, 2);
    if (ch) g_ambient_particles = q;  // (this run only: the option sets the preferences)
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("For this run (Options > Graphics > Ambient Particles keeps the choice)");
    bool game = g_particles_show_game != 0;
    if (ImGui::Checkbox("Draw the game's own rain / snow as well", &game)) g_particles_show_game = game ? 1 : 0;
    bool sky = g_particles_sky != 0;
    if (ImGui::Checkbox("Keep rain / snow / ash out from under roofs", &sky)) g_particles_sky = sky ? 1 : 0;
    int count = 0;
    float cpu = 0.0f, gpu = 0.0f;
    mrp_particles_stats(&count, &cpu, &gpu);
    ImGui::Text("%d particles drawn, %.3f ms CPU (the GPU's: the Profiler tab, \"Ambient particles\"); the game's rain / snow %s", count, cpu,
                mrp_particles_hide_game() ? "replaced" : "drawn");
    std::vector<ParticleSpec>& P = mrp_user_particles();
    const glm::mat4 iv = glm::inverse(render_view_matrix());
    const glm::vec3 cam(iv[3][0], iv[3][1], iv[3][2]);
    const char* kinds[kPartKinds];
    for (int k = 0; k < kPartKinds; ++k) kinds[k] = mrp_particle_kind_name(k);
    for (size_t i = 0; i < P.size(); ++i) {
        ParticleSpec& p = P[i];
        ImGui::PushID(static_cast<int>(2000 + i));
        bool c = ImGui::Checkbox("##on", &p.on);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(110.0f);
        c |= ImGui::Combo("##kind", &p.kind, kinds, kPartKinds);
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        c |= ImGui::SliderFloat("density", &p.density, 0.0f, 4.0f, "%.2f");
        ImGui::SameLine();
        if (ImGui::SmallButton("remove")) {
            P.erase(P.begin() + static_cast<std::ptrdiff_t>(i));
            g_sun_dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::Indent();
        c |= ImGui::Checkbox("own colour", &p.has_col);
        if (p.has_col) {
            ImGui::SameLine();
            c |= ImGui::ColorEdit3("##col", p.col, ImGuiColorEditFlags_NoInputs);
        }
        ImGui::SameLine();
        ImGui::SetNextItemWidth(160.0f);
        c |= ImGui::DragFloat2("wind x y", p.wind, 0.05f, -20.0f, 20.0f, "%.2f");
        ImGui::SameLine();
        ImGui::SetNextItemWidth(80.0f);
        c |= ImGui::SliderFloat("gusts", &p.gust, 0.0f, 4.0f, "%.2f");
        ImGui::SameLine();
        c |= ImGui::Checkbox("from the lava", &p.src_lava);
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Rising from the level's lava (its collision's lava faces near the camera) instead of filling the air around the camera");
        if (p.src_lava) {
            ImGui::SameLine();
            ImGui::SetNextItemWidth(80.0f);
            c |= ImGui::SliderFloat("rise", &p.rise, 0.0f, 4.0f, p.rise == 0.0f ? "the kind's" : "%.2f");
        }
        if (p.kind == kPartRain || p.kind == kPartSnow) {
            ImGui::SameLine();
            c |= ImGui::Checkbox("keep the game's", &p.keep);
        }
        c |= ImGui::Checkbox("only in a box", &p.has_box);
        if (p.has_box) {
            ImGui::SameLine();
            if (ImGui::SmallButton("around the camera")) {
                for (int k = 0; k < 3; ++k) {
                    p.lo[k] = std::round((cam[k] - (k == 2 ? 6.0f : 20.0f)) * 10.0f) / 10.0f;
                    p.hi[k] = std::round((cam[k] + (k == 2 ? 6.0f : 20.0f)) * 10.0f) / 10.0f;
                }
                c = true;
            }
            c |= ImGui::DragFloat3("min", p.lo, 0.1f);
            c |= ImGui::DragFloat3("max", p.hi, 0.1f);
        }
        ImGui::Unindent();
        if (c) g_sun_dirty = true;
        ImGui::PopID();
    }
    static int add_kind = 0;
    ImGui::SetNextItemWidth(110.0f);
    ImGui::Combo("##addkind", &add_kind, kinds, kPartKinds);
    ImGui::SameLine();
    if (ImGui::Button("Add a particles line")) {
        ParticleSpec p;
        p.kind = add_kind;
        P.push_back(p);
        g_sun_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Save##particles")) g_sun_dirty = !mrp_user_lights_save();
    ImGui::TextWrapped("Applied at once. Saved as \"particles <kind> <density> [r g b] [wind x y] [gust g] [box x0 y0 z0 x1 y1 z1] [source lava] [rise r] [keep]\" lines in the "
                       "level's file (a line not edited is written back as it was).");
}
void tab_sun() {
    mrp_user_lights_ensure();  // (the level's file: also with the modern lighting off - the particles' lines)
    const int level = mrp_user_lights_level();
    if (level != g_sun_level) {
        g_sun_level = level;
        g_sun_dirty = false;
    }
    if (level < 0 || g_modern_shadows == 0) {
        ImGui::TextWrapped("The sun is the modern lighting's: Options > Graphics > Lighting (not Original).");
        if (level < 0) return;
    }
    float az = 0.0f, el = 45.0f, gaz = 0.0f, gel = 0.0f;
    bool own = mrp_user_sun(&az, &el);
    mrp_lighting_game_sun(&gaz, &gel);
    ImGui::Text("Level %d%s   %s", level, g_sun_dirty ? " (not saved)" : "", mrp_user_lights_path());
    ImGui::Text("The game's key light: azimuth %.1f, elevation %.1f degrees (toward the light; azimuth from +x toward +y)", gaz, gel);
    bool use_game = !own;
    bool ch = false;
    if (ImGui::Checkbox("Use the game's key light", &use_game)) {
        if (!use_game && !own) {  // (starting from the game's own)
            az = gaz;
            el = gel;
        }
        own = !use_game;
        ch = true;
    }
    if (own) {
        ch |= ImGui::SliderFloat("Azimuth (yaw)", &az, -180.0f, 180.0f, "%.1f deg");
        ch |= ImGui::SliderFloat("Elevation (pitch)", &el, 5.0f, 90.0f, "%.1f deg");
    }
    if (ch) {
        mrp_user_sun_set(own, az, el);
        g_sun_dirty = true;
    }
    // the level's Sun Rays ("sun_rays off" in its file): off here whatever Options > Graphics > Sun Rays says
    if (bool rays_off = mrp_user_sun_rays_off(); ImGui::Checkbox("Sun Rays off on this level", &rays_off)) {
        mrp_user_sun_rays_set_off(rays_off);
        g_sun_dirty = true;
    }
    if (ImGui::IsItemHovered()) ImGui::SetTooltip("Saved as \"sun_rays off\" in the level's file (the Sun Rays option still sets the other levels')");
    if (ImGui::Button("Save")) g_sun_dirty = !mrp_user_lights_save();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        mrp_user_lights_reload();
        g_sun_dirty = false;
    }
    ImGui::TextWrapped("Applied at once (the shadow maps are drawn again for the new sun). Only where the game has a sun: its "
                       "colour and weight are the game's, and a shadow still takes off the key light's term the game lit with. "
                       "Save writes the whole level file (the Lights tab's unsaved edits too).");
    // the level's interiors: boxes (the camera inside: the sun off) and light sets (the hero lit with one: the sun off)
    ImGui::Separator();
    int hero_set = -1;
    const float in = mrp_lighting_interior(&hero_set);
    std::vector<SunInterior>& B = mrp_user_interiors();
    std::vector<int>& sets = mrp_user_interior_sets();
    const glm::mat4 iv = glm::inverse(render_view_matrix());
    const glm::vec3 cam(iv[3][0], iv[3][1], iv[3][2]);
    ImGui::Text("Interiors (the sun off: the original's look). Camera %.1f %.1f %.1f, inside %.2f; the hero's light set %d", cam.x, cam.y, cam.z, in, hero_set);
    ImGui::Checkbox("Show the boxes", &g_interior_show);
    if (ImGui::Button("Add a box around the camera")) {
        SunInterior b;
        for (int i = 0; i < 3; ++i) {
            const float r = i == 2 ? 4.0f : 8.0f;
            b.lo[i] = cam[i] - r;
            b.hi[i] = cam[i] + r;
        }
        B.push_back(b);
        g_interior_sel = static_cast<int>(B.size()) - 1;
        g_sun_dirty = true;
    }
    ImGui::SameLine();
    if (ImGui::Button("Add the hero's light set") && hero_set >= 0 && std::find(sets.begin(), sets.end(), hero_set) == sets.end()) {
        sets.push_back(hero_set);
        g_sun_dirty = true;
    }
    for (size_t i = 0; i < sets.size(); ++i) {
        ImGui::PushID(static_cast<int>(1000 + i));
        ImGui::Text("light set %d", sets[i]);
        ImGui::SameLine();
        if (ImGui::SmallButton("remove")) {
            sets.erase(sets.begin() + static_cast<std::ptrdiff_t>(i));
            g_sun_dirty = true;
            ImGui::PopID();
            break;
        }
        ImGui::PopID();
    }
    if (g_interior_sel >= static_cast<int>(B.size())) g_interior_sel = -1;
    for (size_t i = 0; i < B.size(); ++i) {
        char t[96];
        std::snprintf(t, sizeof t, "box %zu  %.1f %.1f %.1f .. %.1f %.1f %.1f", i, B[i].lo[0], B[i].lo[1], B[i].lo[2], B[i].hi[0], B[i].hi[1], B[i].hi[2]);
        if (ImGui::Selectable(t, static_cast<int>(i) == g_interior_sel)) g_interior_sel = static_cast<int>(i);
    }
    if (g_interior_sel >= 0) {
        SunInterior& b = B[static_cast<size_t>(g_interior_sel)];
        bool ch = ImGui::DragFloat3("Min", b.lo, 0.1f);
        ch |= ImGui::DragFloat3("Max", b.hi, 0.1f);
        if (ImGui::Button("Delete the box")) {
            B.erase(B.begin() + g_interior_sel);
            g_interior_sel = -1;
            ch = true;
        }
        if (ch) g_sun_dirty = true;
    }
    if (g_interior_show) interior_boxes(B);
    sun_particles();
}
// The 3D's place on the window, as the markers above project with it: the world's view-projection, the last viewport (in the render
// scale's buffer), the framebuffer's height, the render scale and ImGui's framebuffer scale
struct ScreenMap {
    glm::mat4 VP{1.0f};
    int vp[4] = {0, 0, 1, 1};
    int fh = 0;
    float rs = 1.0f, sx = 1.0f, sy = 1.0f;
};
bool screen_map(ScreenMap& m) {
    if (!mrp_world_last_viewport(m.vp) || g_window == nullptr || m.vp[2] <= 0 || m.vp[3] <= 0) return false;
    float vpm[16];
    mrp_world_view_proj(vpm);
    m.VP = glm::make_mat4(vpm);
    int fw = 0;
    glfwGetFramebufferSize(g_window, &fw, &m.fh);
    const ImGuiIO& io = ImGui::GetIO();
    m.rs = graphics_render_scale() > 0.0f ? graphics_render_scale() : 1.0f;
    m.sx = io.DisplayFramebufferScale.x > 0.0f ? io.DisplayFramebufferScale.x : 1.0f;
    m.sy = io.DisplayFramebufferScale.y > 0.0f ? io.DisplayFramebufferScale.y : 1.0f;
    return true;
}
bool world_to_screen(const ScreenMap& m, const float w[3], ImVec2& p) {
    const glm::vec4 c = m.VP * glm::vec4(w[0], w[1], w[2], 1.0f);
    if (c.w < 0.05f) return false;
    const float x = (static_cast<float>(m.vp[0]) + (c.x / c.w * 0.5f + 0.5f) * static_cast<float>(m.vp[2])) / m.rs;
    const float yb = (static_cast<float>(m.vp[1]) + (c.y / c.w * 0.5f + 0.5f) * static_cast<float>(m.vp[3])) / m.rs;
    p = ImVec2(x / m.sx, (static_cast<float>(m.fh) - yb) / m.sy);
    return true;
}
// a point of the window (ImGui's coordinates) as the 3D view's u, v (0..1 from the bottom left: mrp_lighting_screen_point's); false off it
bool screen_to_uv(const ScreenMap& m, ImVec2 p, float* u, float* v) {
    const float fx = p.x * m.sx * m.rs, fy = (static_cast<float>(m.fh) - p.y * m.sy) * m.rs;
    *u = (fx - static_cast<float>(m.vp[0])) / static_cast<float>(m.vp[2]);
    *v = (fy - static_cast<float>(m.vp[1])) / static_cast<float>(m.vp[3]);
    return *u >= 0.0f && *u <= 1.0f && *v >= 0.0f && *v <= 1.0f;
}
// orig: (host) the Lights tab's target: the world point under the crosshair (the 3D's centre) or, with "pick with the mouse", where the
// left button was clicked on the picture outside the menu's windows (the depth there read over the next frames, then kept: the camera
// may move afterwards), with the surface there (mrp_world_surface_at), the objects (mrp_lighting_pick) and the surface's normal turned
// toward the camera (a moby's or none: toward the camera) - a light added there sits off the surface along it
struct Aim {
    bool mouse = true;     // (left click on the picture picks)
    bool clicked = false;  // (a clicked point in use, not the crosshair)
    float u = 0.5f, v = 0.5f;
    int wait = 0;          // (frames before the clicked point's depth is taken: the read comes a frame or two late)
    bool hit = false;
    float wp[3] = {0.0f, 0.0f, 0.0f}, n[3] = {0.0f, 0.0f, 1.0f};
    SurfacePick sp;
    LightPick mp;
    float last[3] = {1e30f, 0.0f, 0.0f};
} g_aim;
void aim_take(bool hit, const float w[3]) {
    g_aim.hit = hit;
    if (!hit) {
        g_aim.sp = SurfacePick{};
        g_aim.mp = LightPick{};
        return;
    }
    for (int k = 0; k < 3; ++k) g_aim.wp[k] = w[k];
    g_aim.sp = mrp_world_surface_at(w);
    g_aim.mp = mrp_lighting_pick(w);
    const glm::mat4 iv = glm::inverse(render_view_matrix());
    const glm::vec3 to_cam = glm::vec3(iv[3]) - glm::vec3(w[0], w[1], w[2]);
    glm::vec3 n = glm::length(to_cam) > 0.0f ? glm::normalize(to_cam) : glm::vec3(0.0f, 0.0f, 1.0f);
    if (g_aim.sp.kind >= 0) {
        glm::vec3 sn(g_aim.sp.normal[0], g_aim.sp.normal[1], g_aim.sp.normal[2]);
        if (glm::dot(sn, to_cam) < 0.0f) sn = -sn;
        n = sn;
    }
    for (int k = 0; k < 3; ++k) g_aim.n[k] = n[k];
}
// once a frame while the Lights tab is shown: the click taken, the point brought up to date, the crosshair / the clicked point drawn
void aim_update() {
    ScreenMap sm;
    const bool map = screen_map(sm);
    const ImGuiIO& io = ImGui::GetIO();
    if (g_aim.mouse && map && ImGui::IsMouseClicked(ImGuiMouseButton_Left) && !io.WantCaptureMouse) {
        float u = 0, v = 0;
        if (screen_to_uv(sm, io.MousePos, &u, &v)) {
            g_aim.clicked = true;
            g_aim.u = u;
            g_aim.v = v;
            g_aim.wait = 4;
        }
    }
    float w[3] = {0.0f, 0.0f, 0.0f};
    if (g_aim.clicked) {
        if (g_aim.wait > 0) {
            const bool h = mrp_lighting_screen_point(g_aim.u, g_aim.v, w);
            if (--g_aim.wait == 0) aim_take(h, w);
        }
        ImVec2 p;
        if (g_aim.hit && map && world_to_screen(sm, g_aim.wp, p)) {  // (the clicked point, where it is now)
            ImDrawList* dl = ImGui::GetForegroundDrawList();
            dl->AddCircle(p, 9.0f, IM_COL32(0, 0, 0, 200), 0, 4.0f);
            dl->AddCircle(p, 9.0f, IM_COL32(120, 255, 120, 255), 0, 2.0f);
            dl->AddLine(ImVec2(p.x - 4.0f, p.y), ImVec2(p.x + 4.0f, p.y), IM_COL32(120, 255, 120, 255), 2.0f);
            dl->AddLine(ImVec2(p.x, p.y - 4.0f), ImVec2(p.x, p.y + 4.0f), IM_COL32(120, 255, 120, 255), 2.0f);
            const float tip[3] = {g_aim.wp[0] + g_aim.n[0] * 0.6f, g_aim.wp[1] + g_aim.n[1] * 0.6f, g_aim.wp[2] + g_aim.n[2] * 0.6f};
            ImVec2 q;
            if (world_to_screen(sm, tip, q)) dl->AddLine(p, q, IM_COL32(120, 255, 120, 200), 2.0f);  // (the normal: where a light added goes)
        }
        return;
    }
    const bool h = mrp_lighting_screen_point(0.5f, 0.5f, w);
    if (h != g_aim.hit || (h && std::fabs(w[0] - g_aim.last[0]) + std::fabs(w[1] - g_aim.last[1]) + std::fabs(w[2] - g_aim.last[2]) > 0.01f)) {  // (the search when the point moves)
        for (int k = 0; k < 3; ++k) g_aim.last[k] = w[k];
        aim_take(h, w);
    }
    if (ImGuiViewport* v = ImGui::GetMainViewport()) {  // (the crosshair: the window's centre)
        const ImVec2 c(v->Pos.x + v->Size.x * 0.5f, v->Pos.y + v->Size.y * 0.5f);
        ImDrawList* dl = ImGui::GetForegroundDrawList();
        dl->AddLine(ImVec2(c.x - 10.0f, c.y), ImVec2(c.x + 10.0f, c.y), IM_COL32(255, 255, 255, 220), 2.0f);
        dl->AddLine(ImVec2(c.x, c.y - 10.0f), ImVec2(c.x, c.y + 10.0f), IM_COL32(255, 255, 255, 220), 2.0f);
    }
}
// the target's line under the tab's buttons: the mode, the point, the surface
void aim_ui() {
    ImGui::Checkbox("Pick with the mouse", &g_aim.mouse);
    if (ImGui::IsItemHovered())
        ImGui::SetTooltip("Left click on the picture (outside the menu's windows): the surface and the object there are picked and kept.\n"
                          "Without a click: the crosshair at the centre.");
    ImGui::SameLine();
    if (g_aim.clicked) {
        if (ImGui::SmallButton("Back to the crosshair")) {
            g_aim.clicked = false;
            g_aim.last[0] = 1e30f;
        }
        ImGui::SameLine();
    }
    if (!g_aim.hit) ImGui::TextDisabled("%s: nothing (the sky, or the lighting off)", g_aim.clicked ? (g_aim.wait > 0 ? "Click: reading" : "Click") : "Crosshair");
    else ImGui::Text("%s at %.2f %.2f %.2f", g_aim.clicked ? "Clicked" : "Crosshair", g_aim.wp[0], g_aim.wp[1], g_aim.wp[2]);
}
// the light added at the target: off the surface along its normal
bool aim_light_pos(float out[3]) {
    if (!g_aim.hit) return false;
    for (int k = 0; k < 3; ++k) out[k] = g_aim.wp[k] + g_aim.n[k] * 0.4f;
    return true;
}
// the surface in hand of "Emissive surfaces and materials" (kind -1 none), shared with the Textures tab
EmissiveSurface g_emis_cur{-1, 0, 0, 1.0f, 0.0f};
bool g_emis_follow = true, g_emis_preview = false;

// orig: (host) the surface in hand (g_emis_cur): its class's textures stepped through, the preview, marked emissive or given a material,
// its parameters live (the Lights tab's "Emissive surfaces and materials" and the Textures tab)
void emis_in_hand() {
    EmissiveSurface& cur = g_emis_cur;
    bool& follow = g_emis_follow;
    bool& preview = g_emis_preview;
    static int new_type = MAT_EMISSIVE;  // (what "Mark" gives the surface in hand)
    std::vector<EmissiveSurface>& E = mrp_user_emissive();
    ImGui::Checkbox("Preview (pulsing magenta)", &preview);
    int idx = -1;
    for (size_t i = 0; i < E.size(); ++i)
        if (E[i].kind == cur.kind && E[i].o_class == cur.o_class && E[i].tex == cur.tex) idx = static_cast<int>(i);
    auto type_combo = [](const char* label, int* type) {
        bool ch = false;
        ImGui::SetNextItemWidth(140.0f);
        if (ImGui::BeginCombo(label, mrp_material_name(*type))) {
            for (int t = 0; t < MAT_COUNT; ++t)
                if (ImGui::Selectable(mrp_material_name(t), t == *type)) {
                    *type = t;
                    ch = true;
                }
            ImGui::EndCombo();
        }
        return ch;
    };
    auto set_defaults = [](EmissiveSurface& e) {
        const MatParam* mp = mrp_material_params(e.type);
        for (int i = 0; i < 3; ++i) e.p[i] = mp[i].name != nullptr ? mp[i].def : 0.0f;
        if (e.type == MAT_EMISSIVE) {
            e.strength = 1.0f;
            e.threshold = 0.0f;
        }
    };
    if (cur.kind < 0) {
        ImGui::TextDisabled("No surface in hand: aim at one, or pick one below.");
        mrp_emissive_preview(-1, 0, 0);
    } else {
        // the class's textures, stepped (a moby's: its local textures in use; a tie's: its prims; a shrub's: 0..15; a tfrag's: the level's)
        int ntex = 0;
        uint32_t moby_bits = 0;
        if (cur.kind == 3) moby_bits = mrp_moby_class_local_textures(cur.o_class);
        else mrp_world_class_texture(cur.kind, cur.o_class, cur.tex, &ntex);
        const int level_tex = cur.kind == 3 ? -1 : mrp_world_class_texture(cur.kind, cur.o_class, cur.tex);
        auto step = [&](int d) {
            follow = false;
            for (int n = 0; n < 64; ++n) {
                cur.tex += d;
                if (cur.kind == 3) {
                    cur.tex = (cur.tex + 32) % 32;
                    if (moby_bits == 0 || ((moby_bits >> cur.tex) & 1u) != 0) break;
                } else {
                    cur.tex = ntex > 0 ? (cur.tex + ntex) % ntex : 0;
                    if (mrp_world_class_texture(cur.kind, cur.o_class, cur.tex) >= 0) break;
                }
            }
        };
        if (ImGui::ArrowButton("##emis_prev", ImGuiDir_Left)) step(-1);
        ImGui::SameLine();
        if (ImGui::ArrowButton("##emis_next", ImGuiDir_Right)) step(1);
        ImGui::SameLine();
        const char* marked = idx >= 0 ? mrp_material_name(E[static_cast<size_t>(idx)].type) : nullptr;
        if (cur.kind == 3) ImGui::Text("In hand: moby %d tex %d (its textures: mask %08x)%s%s", cur.o_class, cur.tex, moby_bits, marked ? "  - " : "", marked ? marked : "");
        else ImGui::Text("In hand: %s %d tex %d of %d (the level's texture %d)%s%s", mrp_emissive_kind_name(cur.kind), cur.o_class, cur.tex, ntex, level_tex, marked ? "  - " : "", marked ? marked : "");
        mrp_emissive_preview(preview ? cur.kind : -1, cur.o_class, cur.tex);
        if (idx < 0) {
            type_combo("##mat_new", &new_type);
            ImGui::SameLine();
            if (ImGui::Button(new_type == MAT_EMISSIVE ? "Mark emissive" : "Give the material")) {
                EmissiveSurface e = cur;
                e.type = new_type;
                set_defaults(e);
                E.push_back(e);
                mrp_user_emissive_changed();
                g_light_dirty = true;
                preview = false;  // (the result itself shown)
            }
        } else {
            if (ImGui::Button("Unmark")) {
                E.erase(E.begin() + idx);
                mrp_user_emissive_changed();
                g_light_dirty = true;
            } else {
                EmissiveSurface& e = E[static_cast<size_t>(idx)];
                bool ch = false;  // (live: the shaders' slots read the list every frame)
                ImGui::SameLine();
                if (int ty = e.type; type_combo("Type##mat", &ty) && ty != e.type) {
                    e.type = ty;
                    set_defaults(e);
                    mrp_user_emissive_changed();  // (the slots' copy: a material hides while Surface Detail is Off)
                    ch = true;
                }
                if (e.kind == 3) {  // (a moby class is the same on every level: its line in data/lights/classes.txt)
                    ImGui::SameLine();
                    ch |= ImGui::Checkbox("For every level##emis", &e.global);
                    if (ImGui::IsItemHovered())
                        ImGui::SetTooltip("Saved in %s (every level that has this moby class) instead of this level's file.\n"
                                          "A level's own line for the same class and texture wins over it.", mrp_user_classes_path());
                }
                if (e.type == MAT_EMISSIVE) {
                    ch |= ImGui::SliderFloat("Strength##emis", &e.strength, 0.0f, 4.0f, "%.2f");
                    ch |= ImGui::SliderFloat("Threshold##emis", &e.threshold, 0.0f, 1.0f, "%.2f");
                    ImGui::TextDisabled("Strength: the original lighting's colour x this; threshold: the texels whose brightest channel is below it stay lit as before.");
                } else {
                    const MatParam* mp = mrp_material_params(e.type);
                    for (int i = 0; i < 3; ++i)
                        if (mp[i].name != nullptr) {
                            char lab[48];
                            std::snprintf(lab, sizeof lab, "%s##matp%d", mp[i].name, i);
                            ch |= ImGui::SliderFloat(lab, &e.p[i], mp[i].lo, mp[i].hi, "%.2f");
                        }
                    if (g_surface_detail == 0) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Surface Detail is Off: the material is not drawn.");
                }
                if (ch) g_light_dirty = true;
            }
        }
    }
}
// orig: (host) the Lights tab's "Emissive surfaces": the surface under the crosshair (the world's nearest triangle - tfrag, tie, shrub -
// or the moby there), its class's texture stepped through with a pulsing magenta preview, marked / unmarked, its strength and threshold
// live; "Suggest" lists bright textures of the level as candidates (never applied by themselves). Saved with the lights (Save above).
// The same surface can be given a material instead (docs/materials.md: metal, wet, puddle, lava, water, mud, quicksand) with its
// parameters, live; the level's rain and the reflections' sky colour beside them
void tab_emissive() {
    EmissiveSurface& cur = g_emis_cur;  // (the surface in hand: kind -1 none)
    bool& follow = g_emis_follow;
    bool& preview = g_emis_preview;
    static std::vector<EmissiveCandidate> cands;
    static int cands_level = -1;
    if (!ImGui::CollapsingHeader("Emissive surfaces and materials (emissive, material)")) {
        mrp_emissive_preview(-1, 0, 0);
        return;
    }
    std::vector<EmissiveSurface>& E = mrp_user_emissive();
    if (g_modern_shadows == 0) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Shown only with Lighting not Original.");
    {  // (Options > Graphics > Surface Detail, here too: the materials are drawn only when it is not Off)
        static const char* steps[] = {"Off", "Low", "High"};
        ImGui::SetNextItemWidth(120.0f);
        ImGui::Combo("Surface Detail (materials)", &g_surface_detail, steps, 3);
        ImGui::SameLine();
        if (bool rain = mrp_user_rain(); ImGui::Checkbox("Rain on this level", &rain)) {
            mrp_user_rain_set(rain);
            g_light_dirty = true;
        }
        if (ImGui::IsItemHovered()) ImGui::SetTooltip("Saved as \"rain on\": the puddles' and the water's rain ripples");
    }
    const bool hit = g_aim.hit;
    const SurfacePick& sp = g_aim.sp;
    const LightPick& mp = g_aim.mp;
    const char* where = g_aim.clicked ? "Clicked" : "Crosshair";
    const bool moby_near = hit && mp.moby && (mp.moby_part || mp.moby_dist < 3.0f);
    auto take_moby = [&] {
        const uint32_t bits = mrp_moby_class_local_textures(mp.moby_class);
        int first = 0;
        while (first < 31 && bits != 0 && ((bits >> first) & 1u) == 0) ++first;
        cur = EmissiveSurface{3, mp.moby_class, first, 1.0f, 0.0f};
    };
    if (!hit) {
        ImGui::TextDisabled("%s: nothing (the sky, or the lighting off)", where);
    } else {
        if (sp.kind >= 0)
            ImGui::Text("%s: %s %d tex %d (the level's %s texture %d), %.2f from the point", where, mrp_emissive_kind_name(sp.kind), sp.o_class, sp.tex,
                        mrp_emissive_kind_name(sp.kind), sp.texture, sp.dist);
        else
            ImGui::TextDisabled("%s: no tfrag, tie or shrub triangle within 0.5 units", where);
        if (moby_near) {
            ImGui::Text("moby class %d, %.1f units from its origin", mp.moby_class, mp.moby_dist);
            ImGui::SameLine();
            if (ImGui::SmallButton("Take the moby")) {
                take_moby();
                follow = false;
            }
        }
        if (follow && sp.kind >= 0) cur = EmissiveSurface{sp.kind, sp.o_class, sp.tex, 1.0f, 0.0f};
    }
    ImGui::Checkbox(g_aim.clicked ? "Follow the clicks" : "Follow the crosshair", &follow);
    ImGui::SameLine();
    emis_in_hand();
    int idx = -1;
    for (size_t i = 0; i < E.size(); ++i)
        if (E[i].kind == cur.kind && E[i].o_class == cur.o_class && E[i].tex == cur.tex) idx = static_cast<int>(i);
    ImGui::Text("Marked (%zu, at most %d):", E.size(), kEmissiveSlots);
    for (size_t i = 0; i < E.size(); ++i) {
        char t[128];
        if (E[i].type == MAT_EMISSIVE)
            std::snprintf(t, sizeof t, "%s %d tex %d  x%.2f  threshold %.2f%s##em%zu", mrp_emissive_kind_name(E[i].kind), E[i].o_class, E[i].tex, E[i].strength, E[i].threshold,
                          E[i].global ? "  (every level)" : "", i);
        else
            std::snprintf(t, sizeof t, "%s %d tex %d  %s %.2f %.2f %.2f%s##em%zu", mrp_emissive_kind_name(E[i].kind), E[i].o_class, E[i].tex, mrp_material_name(E[i].type), E[i].p[0],
                          E[i].p[1], E[i].p[2], E[i].global ? "  (every level)" : "", i);
        if (ImGui::Selectable(t, static_cast<int>(i) == idx)) {
            cur = E[i];
            follow = false;
        }
    }
    if (ImGui::Button("Suggest")) {
        cands = mrp_world_emissive_candidates();
        cands_level = mrp_user_lights_level();
    }
    ImGui::SameLine();
    ImGui::TextDisabled("tie / shrub textures with bright saturated or near-white texels: candidates only");
    if (cands_level != mrp_user_lights_level()) cands.clear();
    if (!cands.empty() && ImGui::BeginListBox("##emis_cands", ImVec2(-1.0f, 8.0f * ImGui::GetTextLineHeightWithSpacing()))) {
        for (size_t i = 0; i < cands.size(); ++i) {
            const EmissiveCandidate& c = cands[i];
            char t[128];
            std::snprintf(t, sizeof t, "%s %d tex %d (texture %d): %.0f %% glowing, %.0f %% near white##ec%zu", mrp_emissive_kind_name(c.kind), c.o_class, c.tex, c.texture,
                          100.0f * c.glow, 100.0f * c.white, i);
            if (ImGui::Selectable(t, cur.kind == c.kind && cur.o_class == c.o_class && cur.tex == c.tex)) {
                cur = EmissiveSurface{c.kind, c.o_class, c.tex, 1.0f, 0.0f};
                follow = false;
                preview = true;
            }
        }
        ImGui::EndListBox();
    }
}

void tab_lights() {
    std::vector<UserLight>& L = mrp_user_lights();
    const int level = mrp_user_lights_level();
    if (level != g_light_level) {
        g_light_level = level;
        g_light_sel = -1;
        g_light_dirty = false;
    }
    if (level < 0) {
        ImGui::TextWrapped("The lights load with the modern lighting: Options > Graphics > Lighting (not Original), Secondary Lights: All.");
        return;
    }
    ImGui::Text("Level %d: %zu lights%s   %s", level, L.size(), g_light_dirty ? " (not saved)" : "", mrp_user_lights_path());
    if (g_modern_shadows == 0 || g_modern_points == 0 || g_auto_lights == 0)
        ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Shown only with Lighting not Original and Secondary Lights: All.");
    ImGui::Checkbox("Markers", &g_light_markers);
    ImGui::SameLine();
    aim_update();  // (the target: the crosshair or the point clicked on the picture)
    aim_ui();
    const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
    const Vec3 hp = hero != 0 ? Vec3{rf(hero + 0x10), rf(hero + 0x14), rf(hero + 0x18)} : Vec3{};
    auto add = [&](float x, float y, float z) {
        UserLight l;
        if (g_light_sel >= 0 && g_light_sel < static_cast<int>(L.size())) l = L[static_cast<size_t>(g_light_sel)];  // (the selected one's look)
        l.pos[0] = x;
        l.pos[1] = y;
        l.pos[2] = z;
        l.on = true;
        L.push_back(l);
        g_light_sel = static_cast<int>(L.size()) - 1;
        g_light_dirty = true;
    };
    {  // (at the surface picked: off it along its normal)
        float ap[3];
        const bool can = aim_light_pos(ap);
        if (!can) ImGui::BeginDisabled();
        if (ImGui::Button(g_aim.clicked ? "Add at the clicked point" : "Add at the crosshair") && can) add(ap[0], ap[1], ap[2]);
        if (!can) ImGui::EndDisabled();
        if (ImGui::IsItemHovered(ImGuiHoveredFlags_AllowWhenDisabled)) ImGui::SetTooltip("0.4 units off the surface there, along its normal (the green line)");
        ImGui::SameLine();
    }
    if (ImGui::Button("Add at Ratchet") && hero != 0) add(hp.x, hp.y, hp.z + 1.5f);
    ImGui::SameLine();
    if (ImGui::Button("Add at camera")) {
        const glm::mat4 iv = glm::inverse(render_view_matrix());
        add(iv[3][0], iv[3][1], iv[3][2]);
    }
    ImGui::SameLine();
    if (ImGui::Button("Save")) g_light_dirty = !mrp_user_lights_save();
    ImGui::SameLine();
    if (ImGui::Button("Reload")) {
        mrp_user_lights_reload();
        g_light_sel = -1;
        g_light_dirty = false;
    }
    if (ImGui::BeginListBox("##lights", ImVec2(-1.0f, 7.0f * ImGui::GetTextLineHeightWithSpacing()))) {
        for (size_t i = 0; i < L.size(); ++i) {
            char t[96];
            std::snprintf(t, sizeof t, "%2zu  %7.2f %7.2f %7.2f   r %.1f  x%.2f%s", i, L[i].pos[0], L[i].pos[1], L[i].pos[2], L[i].radius, L[i].intensity, L[i].on ? "" : "  (off)");
            if (ImGui::Selectable(t, static_cast<int>(i) == g_light_sel)) g_light_sel = static_cast<int>(i);
        }
        ImGui::EndListBox();
    }
    if (g_light_sel >= static_cast<int>(L.size())) g_light_sel = -1;
    if (g_light_sel >= 0) {
        UserLight& l = L[static_cast<size_t>(g_light_sel)];
        bool ch = false;
        ch |= ImGui::DragFloat3("Position", l.pos, 0.05f);
        ch |= ImGui::ColorEdit3("Colour", l.col);
        ch |= ImGui::SliderFloat("Intensity", &l.intensity, 0.0f, 3.0f);
        ch |= ImGui::SliderFloat("Radius", &l.radius, 1.0f, 30.0f);
        ch |= ImGui::SliderFloat("Flicker", &l.flicker, 0.0f, 1.0f);
        ch |= ImGui::Checkbox("On", &l.on);
        if (ImGui::Button("Move to Ratchet") && hero != 0) {
            l.pos[0] = hp.x;
            l.pos[1] = hp.y;
            l.pos[2] = hp.z + 1.5f;
            ch = true;
        }
        ImGui::SameLine();
        if (float ap[3]; ImGui::Button("Move to the picked point") && aim_light_pos(ap)) {
            for (int k = 0; k < 3; ++k) l.pos[k] = ap[k];
            ch = true;
        }
        ImGui::SameLine();
        if (ImGui::Button("Ratchet to it")) hero_teleport({l.pos[0], l.pos[1], l.pos[2] - 1.5f});
        ImGui::SameLine();
        if (ImGui::Button("Duplicate")) add(l.pos[0] + 1.0f, l.pos[1], l.pos[2]);
        ImGui::SameLine();
        if (ImGui::Button("Delete")) {
            L.erase(L.begin() + g_light_sel);
            g_light_sel = -1;
            ch = true;
        }
        if (ch) g_light_dirty = true;
    }
    if (ImGui::CollapsingHeader("Automatic emitters (glowing tie classes)")) {  // (a class the heuristic got wrong turned off)
        const std::vector<EmitterClass> cls = mrp_world_emitter_classes();
        for (const EmitterClass& c : cls) {
            bool on = !mrp_user_lights_class_off(c.o_class);
            char t[128];
            std::snprintf(t, sizeof t, "class %d: %.0f %% glowing, hue %.0f, %d lights%s", c.o_class, c.share * 100.0f, c.hue, c.lights,
                          c.material ? " (a material: none)" : "");
            if (ImGui::Checkbox(t, &on)) {
                mrp_user_lights_class_set_off(c.o_class, !on);
                mrp_world_emitters_rebuild();
                g_light_dirty = true;
            }
        }
    }
    if (ImGui::CollapsingHeader("Lights per object class (emit)", ImGuiTreeNodeFlags_DefaultOpen)) {
        // the object under the crosshair (the 3D's centre: its depth, read each frame while this is open): a tie instance whose model's
        // box holds the point, the nearest moby; "emit for this class" puts a light at that point of every instance of the class
        std::vector<ClassLight>& C = mrp_user_class_lights();
        if (!g_aim.hit) {
            ImGui::TextDisabled("%s: nothing (the sky, or the lighting off)", g_aim.clicked ? "Clicked" : "Crosshair");
        } else {
            const LightPick& pk = g_aim.mp;
            auto emit = [&](bool moby, int cls, const float off[3], bool shrub = false, int joint = -1) {
                ClassLight cl;
                cl.moby = moby;
                cl.shrub = shrub;
                cl.o_class = cls;
                cl.joint = joint;
                for (int k = 0; k < 3; ++k) cl.off[k] = off[k];
                C.push_back(cl);
                g_class_sel = static_cast<int>(C.size()) - 1;
                g_light_dirty = true;
            };
            if (pk.tie) {
                ImGui::Text("tie class %d, the point at %.3f %.3f %.3f in its frame", pk.tie_class, pk.tie_off[0], pk.tie_off[1], pk.tie_off[2]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Emit for every tie of this class")) emit(false, pk.tie_class, pk.tie_off);
            }
            if (pk.shrub) {
                ImGui::Text("shrub class %d, the point at %.3f %.3f %.3f in its frame", pk.shrub_class, pk.shrub_off[0], pk.shrub_off[1], pk.shrub_off[2]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Emit for every shrub of this class")) emit(false, pk.shrub_class, pk.shrub_off, true);
            }
            if (pk.moby) {
                ImGui::Text("moby class %d (%.1f units from its origin), the point at %.3f %.3f %.3f in its frame", pk.moby_class, pk.moby_dist, pk.moby_off[0], pk.moby_off[1],
                            pk.moby_off[2]);
                ImGui::SameLine();
                if (ImGui::SmallButton("Emit for every moby of this class")) emit(true, pk.moby_class, pk.moby_off);
                if (pk.moby_joint >= 0) {  // (its pose: the nearest joint - a light that follows the animated part, a flying Buzzer's body)
                    ImGui::Text("  its joint %d, the point at %.3f %.3f %.3f in the joint's frame", pk.moby_joint, pk.moby_joint_off[0], pk.moby_joint_off[1], pk.moby_joint_off[2]);
                    ImGui::SameLine();
                    if (ImGui::SmallButton("Emit on this joint (follows the animation)")) emit(true, pk.moby_class, pk.moby_joint_off, false, pk.moby_joint);
                }
            }
            if (!pk.tie && !pk.moby && !pk.shrub) ImGui::TextDisabled("no tie, shrub or moby there (the terrain?)");
        }
        for (size_t i = 0; i < C.size(); ++i) {
            char t[160];
            char jn[24] = "";
            if (C[i].moby && C[i].joint >= 0) std::snprintf(jn, sizeof jn, "  joint %d", C[i].joint);
            std::snprintf(t, sizeof t, "%s %d  at %.2f %.2f %.2f%s  r %.1f  x%.2f%s%s##cl%zu", C[i].moby ? "moby" : C[i].shrub ? "shrub" : "tie", C[i].o_class, C[i].off[0], C[i].off[1],
                          C[i].off[2], jn, C[i].radius, C[i].intensity, C[i].on ? "" : "  (off)", C[i].global ? "  (every level)" : "", i);
            if (ImGui::Selectable(t, static_cast<int>(i) == g_class_sel)) g_class_sel = static_cast<int>(i);
        }
        if (g_class_sel >= static_cast<int>(C.size())) g_class_sel = -1;
        if (g_class_sel >= 0) {
            ClassLight& cl = C[static_cast<size_t>(g_class_sel)];
            bool ch = false;
            ch |= ImGui::DragFloat3("Offset (its frame)", cl.off, 0.02f);
            ch |= ImGui::ColorEdit3("Colour##cl", cl.col);
            ch |= ImGui::SliderFloat("Intensity##cl", &cl.intensity, 0.0f, 6.0f);
            ch |= ImGui::SliderFloat("Radius##cl", &cl.radius, 1.0f, 30.0f);
            ch |= ImGui::SliderFloat("Flicker##cl", &cl.flicker, 0.0f, 1.0f);
            ch |= ImGui::SliderFloat("Aimed down##cl", &cl.down, 0.0f, 1.0f);  // (a ceiling fixture: 1 all its light below it)
            ch |= ImGui::Checkbox("On##cl", &cl.on);
            if (cl.moby) {  // (a moby class is the same on every level: its line in data/lights/classes.txt; its joint: follows the pose)
                ImGui::SameLine();
                ch |= ImGui::Checkbox("For every level##cl", &cl.global);
                if (ImGui::IsItemHovered())
                    ImGui::SetTooltip("Saved in %s (every level that has this moby class) instead of this level's file.\n"
                                      "A level's own emit line for the same class wins over it.", mrp_user_classes_path());
                ImGui::SetNextItemWidth(120.0f);
                if (ImGui::InputInt("Joint (-1: the moby's own frame)##cl", &cl.joint)) {
                    cl.joint = std::max(cl.joint, -1);
                    ch = true;
                }
            }
            if (ImGui::Button("Delete##cl")) {
                C.erase(C.begin() + g_class_sel);
                g_class_sel = -1;
                ch = true;
            }
            if (ch) g_light_dirty = true;
        }
        ImGui::TextDisabled("Live; Save (above) writes them as \"emit\" lines. Secondary Lights: All.");
    }
    tab_emissive();
    g_class_light_selected = g_class_sel;
    g_user_light_selected = g_light_sel;
    if (g_light_markers) lights_markers(L);
}

// orig: (host) the Textures tab (a tester's request, #101): the level's loaded textures - the MRP's tfrag, tie, shrub and moby textures,
// with the classes using each - as a grid of thumbnails, filtered; a click puts the texture in hand (an emissive / material line by
// one of its classes, as the Lights tab's), with the pulsing preview. The thumbnails: one atlas made from the textures' pixels on the
// CPU (the renderers' decoded copies), an OpenGL texture or a Vulkan image ImGui draws
struct TexPage {
    int level = -1;
    std::vector<TextureEntry> list;
    GLuint gl = 0;
    vk::GpuImage vk{};
    VkDescriptorSet ds = VK_NULL_HANDLE;
    int aw = 0, ah = 0;  // (the atlas's size)
    int sel = -1;
} g_tex;
constexpr int kTexThumb = 64, kTexCols = 16;
ImTextureID tex_page_id() {
    if (g_imgui_vk) return reinterpret_cast<ImTextureID>(g_tex.ds);
    return static_cast<ImTextureID>(g_tex.gl);
}
void tex_page_free() {
    if (g_imgui_vk) {
        if (g_tex.ds != VK_NULL_HANDLE) {
            vk::wait_idle();  // (a debug page's rebuild: the descriptor set freed once no frame uses it)
            ImGui_ImplVulkan_RemoveTexture(g_tex.ds);
            g_tex.ds = VK_NULL_HANDLE;
        }
        vk::destroy_image(g_tex.vk);
    } else if (g_tex.gl != 0) {
        glDeleteTextures(1, &g_tex.gl);
        g_tex.gl = 0;
    }
}
void tex_page_build(int level) {
    tex_page_free();
    g_tex.level = level;
    g_tex.sel = -1;
    g_tex.list = mrp_world_texture_list(kTexThumb);
    for (TextureEntry& e : mrp_moby_texture_list(kTexThumb)) g_tex.list.push_back(std::move(e));
    if (g_tex.list.size() > 2048) g_tex.list.resize(2048);
    if (g_tex.list.empty()) return;
    const int rows = (static_cast<int>(g_tex.list.size()) + kTexCols - 1) / kTexCols;
    g_tex.aw = kTexCols * kTexThumb;
    g_tex.ah = rows * kTexThumb;
    std::vector<uint32_t> px(static_cast<size_t>(g_tex.aw) * g_tex.ah, 0xff000000u);
    for (size_t i = 0; i < g_tex.list.size(); ++i) {
        const int x0 = static_cast<int>(i % kTexCols) * kTexThumb, y0 = static_cast<int>(i / kTexCols) * kTexThumb;
        const std::vector<uint32_t>& t = g_tex.list[i].thumb;
        if (t.size() < static_cast<size_t>(kTexThumb) * kTexThumb) continue;
        for (int y = 0; y < kTexThumb; ++y) std::memcpy(&px[static_cast<size_t>(y0 + y) * g_tex.aw + x0], &t[static_cast<size_t>(y) * kTexThumb], kTexThumb * 4);
    }
    for (TextureEntry& e : g_tex.list) std::vector<uint32_t>().swap(e.thumb);  // (in the atlas now)
    if (g_imgui_vk) {
        if (!vk::create_image(static_cast<uint32_t>(g_tex.aw), static_cast<uint32_t>(g_tex.ah), VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_tex.vk))
            return;
        for (int y = 0; y < g_tex.ah; y += kTexThumb)  // (a row of thumbnails at a time: the frame's staging ring stays small)
            vk::upload_image_2d(g_tex.vk, &px[static_cast<size_t>(y) * g_tex.aw], static_cast<size_t>(g_tex.aw) * kTexThumb * 4, 0, y, g_tex.aw, kTexThumb);
        g_tex.ds = ImGui_ImplVulkan_AddTexture(vk::get_sampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE),
                                               g_tex.vk.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        GLint was = 0, unpack_buf = 0, row = 0, align = 4, active = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &active);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &was);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &unpack_buf);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &row);
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &align);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glGenTextures(1, &g_tex.gl);
        glBindTexture(GL_TEXTURE_2D, g_tex.gl);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, g_tex.aw, g_tex.ah, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(was));
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(unpack_buf));
        glPixelStorei(GL_UNPACK_ROW_LENGTH, row);
        glPixelStorei(GL_UNPACK_ALIGNMENT, align);
        glActiveTexture(static_cast<GLenum>(active));
    }
    std::printf("[lights] the Textures tab: %zu textures of level %d\n", g_tex.list.size(), level);
}
// the line kind / class / texture a texture's user gives (as an emissive line names it)
EmissiveSurface tex_user_surface(const TextureEntry& e, size_t u) {
    const std::pair<int, int>& us = e.users[u];
    return EmissiveSurface{e.kind, us.first, us.second, 1.0f, 0.0f};
}
// a texture with a line (emissive or a material) for one of its users
int tex_marked(const TextureEntry& e) {
    const std::vector<EmissiveSurface>& E = mrp_user_emissive();
    for (size_t i = 0; i < E.size(); ++i)
        for (const auto& us : e.users)
            if (E[i].kind == e.kind && E[i].o_class == us.first && E[i].tex == us.second) return static_cast<int>(i);
    return -1;
}
void tab_textures() {
    mrp_user_lights_ensure();
    const int level = mrp_user_lights_level();
    if (level < 0) {
        ImGui::TextWrapped("No level loaded yet.");
        return;
    }
    static bool kinds[4] = {true, true, true, true};
    static bool only_marked = false;
    static char filter[32] = "";
    static float size = 56.0f;
    if (level != g_tex.level || ImGui::Button("Refresh")) tex_page_build(level);
    ImGui::SameLine();
    ImGui::Text("Level %d: %zu textures", level, g_tex.list.size());
    if (g_modern_shadows == 0) ImGui::TextColored(ImVec4(1.0f, 0.8f, 0.3f, 1.0f), "Emissive surfaces and materials show only with Lighting not Original.");
    static const char* kn[4] = {"tfrag", "tie", "shrub", "moby"};
    for (int k = 0; k < 4; ++k) {
        if (k > 0) ImGui::SameLine();
        ImGui::Checkbox(kn[k], &kinds[k]);
    }
    ImGui::SameLine();
    ImGui::Checkbox("Only marked", &only_marked);
    ImGui::SetNextItemWidth(160.0f);
    ImGui::InputTextWithHint("##texfilter", "class or texture number", filter, sizeof filter);
    ImGui::SameLine();
    ImGui::SetNextItemWidth(140.0f);
    ImGui::SliderFloat("Size", &size, 32.0f, 128.0f, "%.0f");
    // the texture in hand (clicked here or picked in the Lights tab): its users, the marking
    if (g_tex.sel >= 0 && g_tex.sel < static_cast<int>(g_tex.list.size())) {
        const TextureEntry& e = g_tex.list[static_cast<size_t>(g_tex.sel)];
        ImGui::Separator();
        if (g_tex.ds != VK_NULL_HANDLE || g_tex.gl != 0) {
            const int i = g_tex.sel;
            const ImVec2 uv0(static_cast<float>((i % kTexCols) * kTexThumb) / g_tex.aw, static_cast<float>((i / kTexCols) * kTexThumb) / g_tex.ah);
            const ImVec2 uv1(uv0.x + static_cast<float>(kTexThumb) / g_tex.aw, uv0.y + static_cast<float>(kTexThumb) / g_tex.ah);
            ImGui::Image(tex_page_id(), ImVec2(96.0f, 96.0f), uv0, uv1);
            ImGui::SameLine();
        }
        ImGui::BeginGroup();
        ImGui::Text("%s texture %d (%dx%d), used by:", kn[e.kind], e.texture, e.w, e.h);
        for (size_t u = 0; u < e.users.size() && u < 48; ++u) {
            const EmissiveSurface su = tex_user_surface(e, u);
            char t[48];
            std::snprintf(t, sizeof t, "%s %d tex %d##tu%zu", kn[e.kind], su.o_class, su.tex, u);
            if (u % 4 != 0) ImGui::SameLine();
            const bool in_hand = g_emis_cur.kind == su.kind && g_emis_cur.o_class == su.o_class && g_emis_cur.tex == su.tex;
            if (ImGui::Selectable(t, in_hand, 0, ImVec2(150.0f, 0.0f))) {
                g_emis_cur = su;
                g_emis_follow = false;
            }
        }
        if (e.kind == 1 || e.kind == 2)
            ImGui::TextDisabled("(a tie's or a shrub's texture glows wherever the level uses it: one line is enough)");
        else if (e.kind == 3 && e.users.size() > 1)
            ImGui::TextDisabled("(a moby's line is its class's alone: one line per class)");
        ImGui::EndGroup();
        emis_in_hand();
        ImGui::Separator();
    } else {
        mrp_emissive_preview(-1, 0, 0);
        ImGui::TextDisabled("Click a texture to take it in hand (a line for emissive or a material, as in the Lights tab).");
    }
    if (g_tex.list.empty() || (g_tex.ds == VK_NULL_HANDLE && g_tex.gl == 0)) return;
    if (!ImGui::BeginChild("##texgrid", ImVec2(0.0f, 0.0f), true)) {
        ImGui::EndChild();
        return;
    }
    const float avail = ImGui::GetContentRegionAvail().x;
    const float step = size + ImGui::GetStyle().ItemSpacing.x;
    const int cols = std::max(1, static_cast<int>((avail + ImGui::GetStyle().ItemSpacing.x) / step));
    int shown = 0;
    for (size_t i = 0; i < g_tex.list.size(); ++i) {
        const TextureEntry& e = g_tex.list[i];
        if (!kinds[std::clamp(e.kind, 0, 3)]) continue;
        const int mk = tex_marked(e);
        if (only_marked && mk < 0) continue;
        if (filter[0] != 0) {  // (a number: the level's texture or one of the classes)
            char* end = nullptr;
            const long f = std::strtol(filter, &end, 10);
            bool ok = end != filter && f == e.texture;
            for (const auto& us : e.users) ok = ok || (end != filter && f == us.first);
            if (!ok) continue;
        }
        if (shown % cols != 0) ImGui::SameLine();
        ++shown;
        const ImVec2 uv0(static_cast<float>((i % kTexCols) * kTexThumb) / g_tex.aw, static_cast<float>((i / kTexCols) * kTexThumb) / g_tex.ah);
        const ImVec2 uv1(uv0.x + static_cast<float>(kTexThumb) / g_tex.aw, uv0.y + static_cast<float>(kTexThumb) / g_tex.ah);
        ImGui::PushID(static_cast<int>(i));
        const bool sel = static_cast<int>(i) == g_tex.sel;
        if (ImGui::ImageButton("##t", tex_page_id(), ImVec2(size, size), uv0, uv1)) {
            g_tex.sel = static_cast<int>(i);
            if (!e.users.empty()) g_emis_cur = tex_user_surface(e, 0);
            g_emis_follow = false;
            g_emis_preview = true;
        }
        const ImVec2 a = ImGui::GetItemRectMin(), b = ImGui::GetItemRectMax();
        if (sel) ImGui::GetWindowDrawList()->AddRect(a, b, IM_COL32(255, 255, 255, 255), 0.0f, 0, 3.0f);
        else if (mk >= 0) ImGui::GetWindowDrawList()->AddRect(a, b, mrp_user_emissive()[static_cast<size_t>(mk)].type == MAT_EMISSIVE ? IM_COL32(255, 220, 60, 255) : IM_COL32(80, 200, 255, 255), 0.0f, 0, 2.0f);
        if (ImGui::IsItemHovered()) {
            std::string u;
            for (size_t k = 0; k < e.users.size() && k < 12; ++k) u += std::string(k ? ", " : "") + kn[e.kind] + " " + std::to_string(e.users[k].first) + " tex " + std::to_string(e.users[k].second);
            if (e.users.size() > 12) u += ", ...";
            ImGui::SetTooltip("%s texture %d (%dx%d)\n%s%s", kn[e.kind], e.texture, e.w, e.h, u.empty() ? "(no class found)" : u.c_str(),
                              mk >= 0 ? (std::string("\nmarked: ") + mrp_material_name(mrp_user_emissive()[static_cast<size_t>(mk)].type)).c_str() : "");
        }
        ImGui::PopID();
    }
    ImGui::EndChild();
}

void tab_engine() {
    uint32_t bits = ee_read<uint32_t>(CTRL + 8);
    static const struct { uint32_t bit; const char* name; } B[] = {
        {1, "Hero update"}, {2, "Level / mobys update"}, {4, "Pad history / timers"}, {8, "Camera update"}, {0x10, "Single step (R1 steps)"}};
    ImGui::Text("Frame control 0x16c058 +8 = %08x", bits);
    for (const auto& b : B) {
        bool v = (bits & b.bit) != 0;
        if (ImGui::Checkbox(b.name, &v)) {
            bits = v ? (bits | b.bit) : (bits & ~b.bit);
            ee_write<uint32_t>(CTRL + 8, bits);
        }
    }
    int dv = r32(CTRL + 0x18);
    if (ImGui::SliderInt("Debug view (+0x18)", &dv, 0, 6)) w32(CTRL + 0x18, dv);
    ImGui::Separator();
    ImGui::Text("Mobys %08x..%08x", ee_read<uint32_t>(OV(0x0015ffd8)), ee_read<uint32_t>(OV(0x0015ffdc)));
    ImGui::Separator();
    ImGui::Text("Unported calls reached (%zu):", g_logged_port_stubs.size());
    if (ImGui::BeginChild("unported", ImVec2(0, 260), true)) {
        std::vector<std::pair<uint32_t, std::string>> v(g_logged_port_stubs.begin(), g_logged_port_stubs.end());
        std::sort(v.begin(), v.end());
        for (const auto& [a, c] : v) ImGui::Text("0x%06x  from %s", a, c.c_str());
    }
    ImGui::EndChild();
}
}  // namespace

DevSettings& dev_settings() { return g_settings; }

bool dev_boot_wanted() {
    int v = 1;
    if (FILE* f = std::fopen(DISPLAY_FILE, "r")) {
        char key[32];
        while (std::fscanf(f, "%31s", key) == 1)
            if (std::strcmp(key, "boot") == 0 && std::fscanf(f, "%d", &v) != 1) v = 1;
        std::fclose(f);
    }
    return v != 0;
}

void dev_language_boot() {
    const int v = language_saved();
    if (v >= 0) w32(LANGUAGE, v);
}

void dev_menu_init(GLFWwindow* window, bool with_menu) {
    g_window = window;
    g_language_loaded = r32(LANGUAGE);  // the level (and its texts) is loaded by now
    positions_load();
    if (FILE* f = std::fopen(DISPLAY_FILE, "r")) {  // the display settings of the last session
        char key[32];
        while (std::fscanf(f, "%31s", key) == 1) {
            if (std::strcmp(key, "pipeline") == 0) { int v = 1; (void)std::fscanf(f, "%d", &v); g_settings.pipeline = v != 0; }
            else if (std::strcmp(key, "game_thread") == 0) { int v = 1; (void)std::fscanf(f, "%d", &v); g_settings.game_thread = v != 0; }
            else if (std::strcmp(key, "language") == 0) { int v = -1; (void)std::fscanf(f, "%d", &v); g_language = language_valid(v) ? v : -1; }
            else if (std::strcmp(key, "boot") == 0) { int v = 1; (void)std::fscanf(f, "%d", &v); g_boot = v != 0 ? 1 : 0; }
        }
        std::fclose(f);
    }
    g_menu = with_menu;
    if (!g_menu) return;  // (the display settings above only)
    IMGUI_CHECKVERSION();
    ImGui::CreateContext();
    ImGuiIO& io = ImGui::GetIO();
    io.IniFilename = nullptr;
    io.ConfigFlags |= ImGuiConfigFlags_NavEnableKeyboard | ImGuiConfigFlags_NavEnableGamepad;
    ImGui::StyleColorsDark();
    ImGui::GetStyle().ScaleAllSizes(1.2f);
    io.FontGlobalScale = 1.2f;
    g_imgui_vk = rac1::vk::is_active();
    if (g_imgui_vk) {
        ImGui_ImplGlfw_InitForVulkan(window, true);
        ImGui_ImplVulkan_LoadFunctions([](const char* name, void* inst) {
            return vkGetInstanceProcAddr(static_cast<VkInstance>(inst), name);
        }, rac1::vk::instance());
        ImGui_ImplVulkan_InitInfo info{};
        info.Instance = rac1::vk::instance();
        info.PhysicalDevice = rac1::vk::physical_device();
        info.Device = rac1::vk::device();
        info.QueueFamily = rac1::vk::graphics_queue_family();
        info.Queue = rac1::vk::graphics_queue();
        info.DescriptorPool = rac1::vk::imgui_descriptor_pool();
        info.MinImageCount = 2;
        info.ImageCount = 2;
        info.MSAASamples = VK_SAMPLE_COUNT_1_BIT;
        info.UseDynamicRendering = true;
        VkFormat col_fmt = rac1::vk::color_format();
        info.PipelineRenderingCreateInfo.sType = VK_STRUCTURE_TYPE_PIPELINE_RENDERING_CREATE_INFO;
        info.PipelineRenderingCreateInfo.colorAttachmentCount = 1;
        info.PipelineRenderingCreateInfo.pColorAttachmentFormats = &col_fmt;
        ImGui_ImplVulkan_Init(&info);
        ImGui_ImplVulkan_CreateFontsTexture();
    } else {
        ImGui_ImplGlfw_InitForOpenGL(window, true);
        ImGui_ImplOpenGL3_Init("#version 330");
    }
}

void dev_menu_shutdown() {
    g_debug_moby_frozen = nullptr;
    if (g_cam.on) freecam_stop();
    g_move.on = false;
    if (g_window == nullptr) return;
    if (!g_menu) { g_window = nullptr; return; }
    g_menu = false;
    tex_page_free();  // (the Textures tab's atlas)
    g_tex.level = -1;
    if (g_imgui_vk) {
        rac1::vk::wait_idle();
        ImGui_ImplVulkan_Shutdown();
    } else {
        ImGui_ImplOpenGL3_Shutdown();
    }
    ImGui_ImplGlfw_Shutdown();
    ImGui::DestroyContext();
    g_window = nullptr;
}

void dev_menu_poll(GLFWwindow* window) {
    if (!g_menu) return;  // (no menu, no moving Ratchet by hand without the debug tools)
    bool down = glfwGetKey(window, GLFW_KEY_F1) == GLFW_PRESS;
    GLFWgamepadstate s;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &s))
        down = down || s.buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] == GLFW_PRESS;
    if (down && !g_prev_toggle) g_settings.open = !g_settings.open;
    g_prev_toggle = down;
    bool mk = glfwGetKey(window, GLFW_KEY_F2) == GLFW_PRESS;
    if (glfwJoystickIsGamepad(GLFW_JOYSTICK_1) && glfwGetGamepadState(GLFW_JOYSTICK_1, &s))
        mk = mk || s.buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] == GLFW_PRESS;
    if (mk && !g_move.prev_key && ru(HERO_MOBY) != 0) hero_move_set(!g_move.on);
    g_move.prev_key = mk;
    if (g_move.on) hero_move_update(window);
    if (g_cam.on) freecam_update(window);
    if (g_snap_pending != 0) {  // between game frames
        constexpr size_t RAM = 64u * 1024u * 1024u;
        if (g_snap_pending == 1) {
            g_snap_ram.assign(ee_get_ram_base(), ee_get_ram_base() + RAM);
            g_snap_spr.assign(ee_get_scratchpad_base(), ee_get_scratchpad_base() + SPR_SIZE);
        } else if (!g_snap_ram.empty()) {
            std::memcpy(ee_get_ram_base(), g_snap_ram.data(), RAM);
            std::memcpy(ee_get_scratchpad_base(), g_snap_spr.data(), SPR_SIZE);
            g_freeze.mobys.clear();  // the spawned mobys may not be the same any more
            update_freeze_hook();
        }
        g_snap_pending = 0;
    }
}

bool dev_menu_owns_input() { return g_menu && ((g_settings.open && g_settings.block_game_input) || g_cam.on || g_move.on); }

// Kill Ratchet: asked from the menu (or RAC1_DIE_AT), done between game frames (dev_kill_apply, after the game's
// update) so the hero's update never sees a state change in the middle of its own frame.
static bool g_kill_pending = false;

void dev_kill_hero() { g_kill_pending = true; }

// The game's own death. The locomotion's input transition (0x229b70, the standing / moving states) does
// `nanotech <= 0 && sub-state != 0x14 && state in {0, 2}` -> hero_set_state(0x3d, 1); the other states (air,
// water, ...) never look at the nanotech, so this does the same from any state. 0x3d is the death: its
// animation ends, the input transition calls hero_lost (0x211250: counts, fades the screen, sets the respawn
// flag +0x20b1) and the game reloads the checkpoint as after a real hit. In the ship (mode 2) only the
// nanotech is cleared: its own death (0x5d, with the ship's teardown) is the hit path's, not reproduced here.
// orig: (host) the debug menu's Kill Ratchet
void dev_kill_apply() {
    if (!g_kill_pending) return;
    g_kill_pending = false;
    if (ru(HERO_MOBY) == 0) return;
    if (ee_read<uint8_t>(P + 0x20b1) != 0) return;  // already lost (the respawn flag)
    const uint32_t sub = ru(P + 0x208c), mode = ee_read<uint8_t>(P + 0x20a4);
    w32(HEALTH, 0);
    if (sub == 0x14 || mode == 2) return;  // already dying (0x3d, 0x47, 0x7f...) / in the ship
    hero_set_state(0x3d, 1);
}

// Hoverboard anywhere: the hoverboard of the races (Rilgar's, level 5, and Kalebo III's, 16) ridden anywhere on those levels.
// The board is the class-439 moby (Rilgar's L5:0x002f87a8, Kalebo's L16:0x002c7788 update): standing, its state +0x20 is 0
// and it waits for the hero within 5.5; then it hands itself to the hero (PLAYER+0x86c), puts him in his board state
// 0x6b (sub-state 0x16, the levels' own hero controller rides it), keeps its spot (pvars +0x10, when pvars +0x50 is 0)
// and goes to its riding state 1 (+0x30 0xff). Riding, it watches the hero: once his sub-state is not 0x16 any more it
// goes back to its spot and to state 0. The menu does the same mount as the board's own update, with the nearest board
// of the level, wherever the hero is: on Rilgar the board state's entry puts him on the race's start line (level 5's
// own code), so his position and facing are put back afterwards. Leaving is the game's hero_set_state(0, 1), as the
// race's own end does; the board then goes home by itself. Only those two levels: the other levels' hero controllers
// have no board state (0x6b does nothing there) and their WADs no board.
// Asked from the menu (or RAC1_HOVERBOARD_AT), done between game frames like the kill.

// orig: (host)
void dev_hoverboard_set(bool on) { g_board_pending = on ? 1 : 2; }
// orig: (host)
bool dev_hoverboard_on() { return g_board_on; }

// orig: (host) the debug menu's Hoverboard anywhere, between game frames
void dev_hoverboard_apply() {
    const int pending = g_board_pending;
    g_board_pending = 0;
    if (g_board_on && (r32(LEVEL) != g_board_level || !hoverboard_riding())) g_board_on = false;  // got off by itself / new level
    if (pending == 2) {
        if (g_board_on) hero_set_state(0, 1);
        g_board_on = false;
        return;
    }
    if (pending != 1 || g_board_on) return;
    if (!hoverboard_level() || ru(HERO_MOBY) == 0) return;
    if (ee_read<uint8_t>(P + 0x20b1) != 0 || r32(HEALTH) <= 0) return;  // lost / dying
    if (ee_read<uint8_t>(P + 0x20a4) != 0) return;                        // in another movement mode
    const int32_t sub = r32(P + 0x208c);
    if (sub == 0x14 || sub == 0x15 || sub == 0x16) return;  // dying / the board's own check (0x15, 0x16)
    const uint32_t board = hoverboard_find();
    if (board == 0) return;
    const Vec3 pos = rv(POS);
    const float yaw = rf(ROT + 8);
    // the board's mount (its update's state 0 once the hero is near)
    const uint32_t pv = ru(board + 0x78);
    w32(BOARD_RIDDEN, static_cast<int32_t>(board));
    hero_set_state(0x6b, 1);
    if (ru(pv + 0x50) == 0)
        for (uint32_t k = 0; k < 16; k += 4) w32(pv + 0x10 + k, r32(board + 0x10 + k));  // (its lq / sq)
    ee_write<uint8_t>(board + 0x20, 1);
    ee_write<uint8_t>(board + 0x30, 0xff);
    if (dist(rv(POS), pos) > 0.01f) {  // (Rilgar's entry: the race's start line, facing along the track)
        hero_teleport(pos, &yaw);
        wf(P + 0x874, yaw);  // (the entry's heading the board's crash check compares with: +0x98 there)
    }
    g_board_on = hoverboard_riding();
    g_board_level = r32(LEVEL);
}

void dev_menu_game_frame() {
    const DevSettings& s = g_settings;
    dev_kill_apply();
    dev_hoverboard_apply();
    if (g_move.on) hero_teleport(g_move.pos, &g_move.yaw);
    if (s.god_mode && r32(HEALTH) > 0) w32(HEALTH, r32(HEALTH_MAX));
    if (s.infinite_bolts) w32(BOLTS, 9999999);
    if (s.infinite_ammo)
        for (int id = 0; id < ITEMS; ++id)
            if (ammo_max(id) > 0 && ee_read<uint8_t>(OWNED + static_cast<uint32_t>(id)) != 0) w32(AMMO + static_cast<uint32_t>(id) * 4, ammo_max(id));
}

void dev_menu_draw(double update_ms, double frame_ms) {
    g_user_light_selected = -1;  // (the Lights tab's selection pulses while the tab is shown)
    g_class_light_selected = -1;
    {  // (the crosshair's depth read only while the Lights tab asks for it, frame by frame)
        float unused[3];
        mrp_lighting_screen_point(-1.0f, -1.0f, unused);
    }
    if (!g_menu || (!g_settings.open && !g_settings.show_stats && !profiler_overlay_wanted())) return;
    if (g_imgui_vk) ImGui_ImplVulkan_NewFrame();
    else ImGui_ImplOpenGL3_NewFrame();
    ImGui_ImplGlfw_NewFrame();
    ImGui::NewFrame();
    if (g_settings.show_stats) {
        ImGui::SetNextWindowPos(ImVec2(8, 8));
        ImGui::SetNextWindowBgAlpha(0.5f);
        ImGui::Begin("stats", nullptr, ImGuiWindowFlags_NoDecoration | ImGuiWindowFlags_AlwaysAutoResize | ImGuiWindowFlags_NoInputs);
        ImGui::Text("%.1f fps  frame %.2f ms  update %.2f ms", frame_ms > 0 ? 1000.0 / frame_ms : 0.0, frame_ms, update_ms);
        const uint32_t hero = ee_read<uint32_t>(OV(0x0015ffd8));
        if (hero != 0) ImGui::Text("hero %.1f %.1f %.1f  state %d", rf(hero + 0x10), rf(hero + 0x14), rf(hero + 0x18), r32(0x001413d4));
        ImGui::Text("nanotech %d/%d  bolts %d  sounds %d", r32(HEALTH), r32(HEALTH_MAX), r32(BOLTS), snd989_iop_active_sounds());
        ImGui::End();
    }
    profiler_overlay();
    if (g_settings.open) {
        ImGui::SetNextWindowSize(ImVec2(720, 600), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowPos(ImVec2(40, 110), ImGuiCond_FirstUseEver);
        ImGui::SetNextWindowBgAlpha(0.85f);
        if (ImGui::Begin("Debug menu (R3 / F1)", &g_settings.open)) {
            static const char* force = std::getenv("RAC1_DEVMENU_SHOT");  // testing: a tab shown
            auto tab = [](const char* name) {
                const bool sel = force != nullptr && std::strcmp(force, name) == 0;
                return ImGui::BeginTabItem(name, nullptr, sel ? ImGuiTabItemFlags_SetSelected : 0);
            };
            if (ImGui::BeginTabBar("tabs")) {
                if (tab("Game")) { tab_game(); ImGui::EndTabItem(); }
                if (tab("Player")) { tab_player(); ImGui::EndTabItem(); }
                if (tab("Items")) { tab_items(); ImGui::EndTabItem(); }
                if (tab("Unlocks")) { tab_unlocks(); ImGui::EndTabItem(); }  // (dev_unlocks.cpp)
                if (tab("Mobys")) { tab_mobys(); ImGui::EndTabItem(); }
                if (tab("Camera")) { tab_camera(); ImGui::EndTabItem(); }
                if (tab("Rendering")) { tab_rendering(); ImGui::EndTabItem(); }
                if (tab("Lights")) { tab_lights(); ImGui::EndTabItem(); }
                if (tab("Sun")) { tab_sun(); ImGui::EndTabItem(); }
                if (tab("Textures")) { tab_textures(); ImGui::EndTabItem(); }
                if (tab("Display")) { tab_display(); ImGui::EndTabItem(); }
                if (tab("Audio")) { tab_audio(); ImGui::EndTabItem(); }
                if (tab("Levels")) { tab_levels(); ImGui::EndTabItem(); }
                if (tab("Engine")) { tab_engine(); ImGui::EndTabItem(); }
                if (tab("Profiler")) { tab_profiler(); ImGui::EndTabItem(); }
                ImGui::EndTabBar();
            }
        }
        ImGui::End();
    }
    ImGui::Render();
    if (g_imgui_vk) rac1::vk::render_imgui(ImGui::GetDrawData());
    else ImGui_ImplOpenGL3_RenderDrawData(ImGui::GetDrawData());
}

}  // namespace rac1::devtools
