// See mod_code.h and sdk/rac1_mod.h. Not a port: the host's plugin loader and the services of its C interface, written on
// top of the game's own functions.
#include <host/mod_code.h>

#include <host/level_slots.h>
#include <runtime/iop/snd989_iop.h>
#include <game/enhance/advanced.h>
#include <game/enhance/class_profiles.h>
#include <game/enhance/leveldata.h>
#include <game/enhance/pool.h>
#include <game/enhance/session.h>
#include <game/enhance/session_runtime.h>
#include <game/ovl/arrival.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/vendor_tr.h>
#include <game/ovl/pause_menu.h>
#include <game/render/mrp_lod.h>
#include <game/render/render_view.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/mods.h>
#include <formats/host_meshes.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <fstream>
#include <sstream>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>
#include <vector>

#if defined(_WIN32)
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#else
#include <dlfcn.h>
#endif

#include "../../../sdk/rac1_mod.h"

namespace rac1 {

bool g_mod_code_any = false;
bool g_mod_code_objects = false;
bool g_mod_update_all = false;
bool g_mod_weapons_limited = false;

namespace {

// The mods' block of EE memory (the PS2 kernel's first megabyte, unused by the port's kernel: 0xe0000.. the level mods' texts,
// level_slots.cpp): 0xe1000..0xe2000 ours (the HUD's strings, the collision's vectors), 0xe2000..0xef800 the plugins'
// (ee_alloc), 0xef800..0xf0000 the level's own sounds' definitions (64 of 0x20 bytes). The ADV block starts at 0xf0000
// (advanced.cpp).
constexpr uint32_t SCRATCH = 0x000e1000u, STRINGS = 0x000e1100u, STRING_SIZE = 0x100, STRING_COUNT = 15;
constexpr uint32_t HEAP = 0x000e2000u, HEAP_END = 0x000ef400u;
// (ABI 5) 0xdf000..0xe0000 below it: the paths the base level's profiled enemies are given (Kalebo III's: leveldata.cpp's
// BASE_MEM, class_profiles.h), written only on a level mod's level
// (ABI 4) the frame's lights of the plugins (light_add), 0xef400..0xef800: their count, then MOD_LIGHT_MAX of 0x20 bytes (x y z
// radius, r g b, 0): in the game's memory, so that a frame's drawing reads its own frame's (the copy taken at its prepare,
// orig_main.cpp's draw_memory_take; mrp_lighting.cpp's mod_lights_add)
constexpr uint32_t SFX_DEFS = 0x000ef800u, SFX_MAX = 64;
constexpr uint32_t PC_ITEM = 0x300;  // our item in save<N>.pc: a plugin's bytes (level mods' items are 0x200)
constexpr uint32_t PC_VERSION = 1;
constexpr uint32_t LEVEL = 0x0015ed84u;

// An angle (radians) folded into -pi..pi. The game's rotation matrix (euler_to_mat3_vu, the VU0 program 0x1a3) folds its
// angles only once: past about 3 pi / 2 its sine polynomial runs away and the model grows without bound (a plugin turning an
// object by adding to its angle every frame made it a huge plane across the level after a few seconds). The game's own code
// keeps its angles in range; what a plugin gives us is folded here. An angle already in range is kept as it is.
float fold_angle(float a) {
    constexpr float kPi = 3.14159265f;
    if (!std::isfinite(a)) return 0.0f;
    if (a >= -kPi && a <= kPi) return a;
    return static_cast<float>(std::remainder(static_cast<double>(a), 2.0 * 3.14159265358979323846));
}

using MainFn = int (*)(const rac1_api*, rac1_mod*);
using AbiFn = unsigned (*)();

// (ABI 2) a class's behaviour as a plugin defined it (class_define)
struct ClassDef {
    std::string name;
    rac1_class_def d{};
};

struct Plugin {
    const ModInfo* mod = nullptr;
    void* lib = nullptr;
    rac1_mod cb{};
    uint64_t rng = 0;
    bool loaded_item = false;  // (a save's .pc read) it had an item of this plugin
    std::vector<ClassDef> classes;
};

// ---- (ABI 2) the level's own content: level.sdk of a level mod's build (the composer's, src/setup/level_compose.cpp) ----
constexpr int32_t CARRIER = 500;         // the objects' moby: the base level's bolt crate (its collision box, its hits)
constexpr float CARRIER_EDGE = 24.0f;    // the crate's box's edge at scale 1 (measured: 1 unit at its class's 1 / 24), its bottom at the origin
constexpr uint32_t MAX_OBJECTS = 256;

struct SdkClass {
    std::string name;
    int32_t mesh = -1;          // its model: the host meshes' class
    float size = 1.0f;          // its hit box's edge
    float scale = 1.0f;         // its model's scale
    bool ghost = false;         // no collision
    std::vector<std::array<uint8_t, 4>> palette;
    Plugin* owner = nullptr;    // its behaviour's plugin (class_define), null none
    const ClassDef* def = nullptr;
};
struct SdkPlaced {
    int32_t cls = 0;
    float pos[3] = {};
    float yaw = 0;
    std::string params;
};
struct SdkSound {
    std::string name;
    int32_t index = 0;          // in the bank
    int32_t volume = 0x400;
    float near_dist = 10, far_dist = 60;  // (not "near" and "far": <windows.h> defines both as nothing, GitHub issue #142)
    bool loop = false;
};
struct SdkTrigger {
    std::string name;
    float lo[3] = {}, hi[3] = {};
    bool inside = false;
};
struct SdkPath {  // (ABI 8) level.txt's path lines (and the plugins' path_create, after them)
    std::string name;
    std::vector<std::array<float, 3>> pts;
};
struct Obj {
    bool live = false;
    uint16_t gen = 0;
    int32_t cls = -1;
    uint32_t moby = 0;
    uint32_t coll = 0;          // its moby's collision (+0x94) while solid
    bool shown = true, solid = true;
    float scale = 1.0f;
    int32_t placed = -1;        // its object line, -1 spawned
    std::vector<uint8_t> state;
};
struct Sdk {
    bool on = false;            // the current level has a level.sdk
    std::vector<SdkClass> classes;
    std::vector<SdkPlaced> placed;
    std::vector<SdkSound> sounds;
    std::vector<SdkTrigger> triggers;
    std::vector<SdkPath> paths;
    std::vector<Obj> objs;
    std::vector<int16_t> of_slot;  // moby slot -> object index + 1 (0 none)
    uint32_t bank = 0;          // the sound bank's handle on the game's IOP (0 none)
    bool spawned = false;       // the placed objects spawned (the level's first frame)
    bool update_all = false;    // level.txt's update_all: every moby updated wherever the camera is (moby_visible_queue)
} g_sdk;
uint64_t g_frames = 0;

std::vector<Plugin> g_plugins;
Plugin* g_cur = nullptr;   // the plugin whose callback runs (its log's name, its files, its allocations)
bool g_tried = false;
bool g_in_hud = false;
bool g_started = true;     // the current level's start given (false: at its next frame)
int g_level_slot = -1;     // the level the plugins were last told of (its slot)
uint32_t g_heap = HEAP;
uint32_t g_hud_string = 0;
std::vector<std::vector<uint8_t>> g_carried;  // the .pc items of plugins not loaded (written back as read)
rac1_api g_api{};

// a callback of `p` run with it current (no callback while the renderer replays a frame, unless `in_replay`)
template <typename F>
void with(Plugin& p, F&& f) {
    Plugin* prev = g_cur;
    g_cur = &p;
    f();
    g_cur = prev;
}

bool addr_ok(uint32_t a, uint32_t n) { return a != 0 && a < ee_game_ram_end() && n <= ee_game_ram_end() - a; }

void say(const char* what, uint32_t a) {
    std::fprintf(stderr, "[mod %s] %s at %#x: not in the game's memory, ignored\n", g_cur != nullptr ? g_cur->mod->folder.c_str() : "?", what, a);
}

uint32_t moby_base() { return lwu(OV(0x0015ffd8)); }

// ---- the services (sdk/rac1_mod.h) ----
void api_log(const char* t) { std::printf("[mod %s] %s\n", g_cur != nullptr ? g_cur->mod->folder.c_str() : "?", t != nullptr ? t : ""); }

int api_mod_path(const char* name, char* out, size_t cap) {
    if (g_cur == nullptr || name == nullptr || out == nullptr) return 0;
    const std::string s = (g_cur->mod->dir / name).string();
    if (s.size() + 1 > cap) return 0;
    std::memcpy(out, s.c_str(), s.size() + 1);
    return 1;
}

uint32_t api_ov(uint32_t a) { return ovl_xlate_data(a); }
void api_sw(uint32_t a, uint32_t v) { addr_ok(a, 4) ? sw(a, v) : say("a write", a); }
void api_sh(uint32_t a, uint32_t v) { addr_ok(a, 2) ? sh(a, v) : say("a write", a); }
void api_sb(uint32_t a, uint32_t v) { addr_ok(a, 1) ? sb(a, v) : say("a write", a); }
void api_swf(uint32_t a, float v) { addr_ok(a, 4) ? swc1(a, v) : say("a write", a); }

uint32_t api_ee_alloc(uint32_t size) {
    const uint32_t at = (g_heap + 15) & ~15u;
    if (size == 0 || size > HEAP_END - at) {
        std::fprintf(stderr, "[mod %s] ee_alloc(%u): the mods' block is full\n", g_cur != nullptr ? g_cur->mod->folder.c_str() : "?", size);
        return 0;
    }
    for (uint32_t i = 0; i < size; i += 4) sw(at + i, 0);
    g_heap = at + size;
    return at;
}

int api_replaying() { return ee_journal_active() ? 1 : 0; }

uint32_t api_moby_by_uid(int32_t uid) {
    if (uid < 0 || uid >= 0x8000) return 0;
    // (the load's uid map, 0x1ac780, is not kept after it: the mobys looked through, live ones only)
    for (uint32_t m = moby_base(), end = lwu(OV(0x0015ffe0)); m != 0 && m <= end; m += 0x100)
        if (lh(m + 0xb2) == uid && static_cast<int8_t>(lbu(m + 0x20)) >= 0) return m;
    return 0;
}

// the class `cls` is one of the level's (registered: 0x197bc0[cls] its index, 0x197a00[index] the class)
bool class_on_level(int32_t cls) {
    if (cls < 0 || cls >= 0x800) return false;
    const uint32_t idx = lbu(OV(0x00197bc0) + static_cast<uint32_t>(cls));
    return lh(OV(0x00197a00) + idx * 2) == cls;
}

uint32_t api_moby_spawn(int32_t cls) {
    if (!class_on_level(cls)) {
        std::fprintf(stderr, "[mod %s] moby_spawn(%d): the level has no such class\n", g_cur != nullptr ? g_cur->mod->folder.c_str() : "?", cls);
        return 0;
    }
    return moby_spawn(static_cast<uint32_t>(cls));
}

bool is_moby(uint32_t m) {
    const uint32_t b = moby_base();
    return m >= b && m < lwu(OV(0x0015ffe0)) + 0x100 && (m - b) % 0x100 == 0;
}

void api_moby_remove(uint32_t m) {
    if (is_moby(m)) moby_retire(m);
}

void api_moby_moved(uint32_t m) {
    if (!is_moby(m) || lwu(m + 0x24) == 0) return;
    for (uint32_t k = 0; k < 3; ++k) {  // (a rotation the plugin wrote: folded, as object_place's)
        const float a = lwc1(m + 0x40 + 4 * k);
        if (!(a >= -3.14159265f && a <= 3.14159265f)) swc1(m + 0x40 + 4 * k, fold_angle(a));
    }
    moby_update_transform(m);
}

uint32_t api_moby_link(uint32_t m, uint32_t off) {
    if (!is_moby(m) || lwu(m + 0x78) == 0) return 0;
    const int32_t idx = lw(lwu(m + 0x78) + off);
    return idx < 0 ? 0 : moby_base() + static_cast<uint32_t>(idx) * 0x100;
}

void api_moby_set_link(uint32_t m, uint32_t off, uint32_t other) {
    if (!is_moby(m) || lwu(m + 0x78) == 0) return;
    sw(lwu(m + 0x78) + off, other == 0 || !is_moby(other) ? 0xffffffffu : (other - moby_base()) >> 8);
}

int api_collide_line(const float from[3], const float to[3], float hit[3]) {
    for (int i = 0; i < 3; ++i) {
        swc1(SCRATCH + 4 * static_cast<uint32_t>(i), from[i]);
        swc1(SCRATCH + 0x10 + 4 * static_cast<uint32_t>(i), to[i]);
    }
    swc1(SCRATCH + 0xc, 1.0f);
    swc1(SCRATCH + 0x1c, 1.0f);
    const int32_t r = segment_collide(SCRATCH, SCRATCH + 0x10, 0, 0, 0);
    if (r != 0 && hit != nullptr)
        for (int i = 0; i < 3; ++i) hit[i] = lwc1(OV(0x00173e60) + 4 * static_cast<uint32_t>(i));
    return r != 0 ? 1 : 0;
}

uint32_t api_hero() { return moby_base(); }

void api_hero_place(const float pos[3], float yaw) {
    for (int i = 0; i < 3; ++i) swc1(SCRATCH + 0x20 + 4 * static_cast<uint32_t>(i), pos[i]);
    swc1(SCRATCH + 0x2c, 1.0f);
    swc1(SCRATCH + 0x30, 0.0f);
    swc1(SCRATCH + 0x34, 0.0f);
    swc1(SCRATCH + 0x38, fold_angle(yaw));
    swc1(SCRATCH + 0x3c, 0.0f);
    hero_place(SCRATCH + 0x20, SCRATCH + 0x30, 0, 1);  // (state 0, standing, as the game's own teleports: -1 left a blend between two animations without its second, a crash)
}

int32_t api_bolts() { return lw(0x0015ed98); }
void api_bolts_add(int32_t n) { sw(0x0015ed98, static_cast<uint32_t>(std::max(0, lw(0x0015ed98) + n))); }
int32_t api_health() { return lw(0x001415f8); }

int32_t api_sound(int32_t idx, uint32_t m) {
    if (idx < 0 || idx >= lw(OV(0x0015f574))) return -1;
    return sound_play_indexed(idx, 0, m != 0 && is_moby(m) ? m : moby_base());
}

void api_music(int32_t track) {
    if (track < 0 || track >= 15) return;
    music_stop_all();
    music_start(track, 1, 0x400);
}

void api_smoke(const float pos[3], const float vel[3], float size) {
    const Vec4 a{pos[0], pos[1], pos[2], 1.0f}, v{vel[0], vel[1], vel[2], 0.0f};
    smoke_burst_v(a, a, v, size, size);
}

uint32_t hud_string(const char* text);

void api_hud_text(int32_t x, int32_t y, uint32_t abgr, const char* text) {
    if (!g_in_hud || text == nullptr) return;
    const uint32_t s = hud_string(text);
    text_draw(x, y, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(abgr))), s, -1, effect_tex0(1), OV(0x001c3150));
}

void api_camera_position(float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = lwc1(OV(0x00166dc0) + 4 * static_cast<uint32_t>(i));
}

// (ABI 4) the pad as read this frame, before input_hold kept it from the game (mod_code_pad_read)
constexpr uint32_t PAD = 0x0013c940;
bool g_pad_seen = false;
uint32_t g_pad_held = 0, g_pad_pressed = 0;
uint32_t api_pad_held() { return g_pad_seen ? g_pad_held : lwu(PAD + 0x1b0); }
uint32_t api_pad_pressed() { return g_pad_seen ? g_pad_pressed : lwu(PAD + 0x1c4); }

uint32_t api_random() {
    if (g_cur == nullptr) return 0;
    uint64_t& x = g_cur->rng;  // (xorshift64*: the plugin's own, seeded at each level's start)
    x ^= x >> 12;
    x ^= x << 25;
    x ^= x >> 27;
    return static_cast<uint32_t>((x * 0x2545F4914F6CDD1Dull) >> 32);
}

int api_call_port(uint32_t veldin_addr, uint32_t arg) {
    const uint32_t at = ovl_xlate_code(veldin_addr);
    if (at == 0 || !ovl_moby_fn_ported(at)) return 0;
    ovl_call_moby_fn(at, arg);
    return 1;
}

// ---- (ABI 2) the level's objects (docs/sdk.md, "Objects") ----
const char* cur_name() { return g_cur != nullptr ? g_cur->mod->folder.c_str() : "?"; }

uint32_t obj_handle(size_t i) { return static_cast<uint32_t>(g_sdk.objs[i].gen) << 16 | static_cast<uint32_t>(i + 1); }

int obj_index(uint32_t h) {
    const uint32_t i = (h & 0xffffu) - 1;
    if (h == 0 || i >= g_sdk.objs.size()) return -1;
    const Obj& o = g_sdk.objs[i];
    return o.live && o.gen == (h >> 16) ? static_cast<int>(i) : -1;
}

int obj_of_moby(uint32_t m) {
    if (!g_sdk.on || !is_moby(m)) return -1;
    const uint32_t slot = (m - moby_base()) >> 8;
    if (slot >= g_sdk.of_slot.size() || g_sdk.of_slot[slot] == 0) return -1;
    const int i = g_sdk.of_slot[slot] - 1;
    return g_sdk.objs[static_cast<size_t>(i)].live && g_sdk.objs[static_cast<size_t>(i)].moby == m ? i : -1;
}

// the carrier's scale: its crate box's edge the class's size times the object's scale
void obj_carrier_scale(const Obj& o) {
    const SdkClass& c = g_sdk.classes[static_cast<size_t>(o.cls)];
    swc1(o.moby + 0x2c, c.size * o.scale / CARRIER_EDGE);
}

// a class's callback for object `i`, run with its plugin current (none: nothing)
template <typename F>
void obj_call(size_t i, F&& f) {
    const SdkClass& c = g_sdk.classes[static_cast<size_t>(g_sdk.objs[i].cls)];
    if (c.def == nullptr || c.owner == nullptr) return;
    with(*c.owner, [&] { f(c.def->d); });
}

uint32_t obj_spawn(int32_t cls, const float pos[3], float yaw, int32_t placed) {
    if (!g_sdk.on || cls < 0 || static_cast<size_t>(cls) >= g_sdk.classes.size() || pos == nullptr) return 0;
    if (!class_on_level(CARRIER)) {
        std::fprintf(stderr, "[sdk] the base level has no class %d (the objects' carrier): no objects\n", CARRIER);
        return 0;
    }
    size_t i = 0;
    while (i < g_sdk.objs.size() && g_sdk.objs[i].live) ++i;
    if (i == g_sdk.objs.size()) {
        if (g_sdk.objs.size() >= MAX_OBJECTS) {
            std::fprintf(stderr, "[sdk] %u objects already: no more\n", MAX_OBJECTS);
            return 0;
        }
        g_sdk.objs.emplace_back();  // (reserved at the level's start: the references stay)
    }
    const uint32_t m = moby_spawn(static_cast<uint32_t>(CARRIER));
    if (m == 0) {
        std::fprintf(stderr, "[sdk] the game's mobys are all used: no object\n");
        return 0;
    }
    const SdkClass& c = g_sdk.classes[static_cast<size_t>(cls)];
    Obj& o = g_sdk.objs[i];
    o.live = true;
    o.cls = cls;
    o.moby = m;
    o.scale = 1.0f;
    o.placed = placed;
    o.shown = true;
    // the carrier: no class code (its crate's never runs), hidden from the game's draw (our model is drawn instead),
    // updated at any distance; its collision box the class's size, or none (ghost)
    sw(m + 0x74, 0);
    sb(m + 0x30, 0xff);
    sh(m + 0x34, lhu(m + 0x34) | 1u);
    for (uint32_t k = 0; k < 3; ++k) swc1(m + 0x10 + 4 * k, pos[k]);
    swc1(m + 0x40, 0.0f);
    swc1(m + 0x44, 0.0f);
    swc1(m + 0x48, fold_angle(yaw));
    o.coll = lwu(m + 0x94);
    o.solid = !c.ghost;
    if (c.ghost) sw(m + 0x94, 0);
    obj_carrier_scale(o);
    moby_update_transform(m);
    const uint32_t slot = (m - moby_base()) >> 8;
    if (slot >= g_sdk.of_slot.size()) g_sdk.of_slot.resize(slot + 1, 0);
    g_sdk.of_slot[slot] = static_cast<int16_t>(i + 1);
    o.state.assign(c.def != nullptr ? c.def->d.state_size : 0, 0);
    const uint32_t h = obj_handle(i);
    if (c.def != nullptr && c.def->d.spawn != nullptr) {
        rac1_spawn sp{};
        sp.size = sizeof sp;
        for (int k = 0; k < 3; ++k) sp.pos[k] = pos[k];
        sp.yaw = yaw;
        sp.params = placed >= 0 ? g_sdk.placed[static_cast<size_t>(placed)].params.c_str() : "";
        sp.placed = placed >= 0 ? 1 : 0;
        sp.index = placed;
        obj_call(i, [&](const rac1_class_def& d) { d.spawn(d.user, h, &sp); });
    }
    return obj_index(h) >= 0 ? h : 0;  // (its spawn may have removed it)
}

void obj_remove(int i, bool retire) {
    Obj& o = g_sdk.objs[static_cast<size_t>(i)];
    if (!o.live) return;
    const uint32_t h = obj_handle(static_cast<size_t>(i));
    obj_call(static_cast<size_t>(i), [&](const rac1_class_def& d) {
        if (d.remove != nullptr) d.remove(d.user, h);
    });
    if (!o.live || o.gen != (h >> 16)) return;  // (removed meanwhile)
    if (retire && is_moby(o.moby) && static_cast<int8_t>(lbu(o.moby + 0x20)) >= 0) moby_retire(o.moby);
    const uint32_t slot = (o.moby - moby_base()) >> 8;
    if (slot < g_sdk.of_slot.size()) g_sdk.of_slot[slot] = 0;
    o.live = false;
    ++o.gen;
    o.state.clear();
    o.state.shrink_to_fit();
}

// the hit `m` took (one of `kinds`), consumed
bool take_hit(uint32_t m, uint32_t kinds, rac1_hit& h) {
    if (!is_moby(m)) return false;
    const uint32_t e = sub_0025a420(m, kinds, 0);
    if (e == 0) return false;
    h = rac1_hit{};
    h.size = sizeof h;
    h.from = lwu(e + 0x20);
    h.kinds = lwu(e + 0x24);
    h.strength = lwc1(e + 0x2c);
    for (uint32_t k = 0; k < 3; ++k) {
        h.point[k] = lwc1(e + 4 * k);
        h.dir[k] = lwc1(e + 0x10 + 4 * k);
    }
    sb(m + 0xa4, 0xff);
    return true;
}

// an output struct the caller sized (its first word; 0: ours)
template <typename T>
void fill_out(T* out, const T& v) {
    if (out == nullptr) return;
    const uint32_t n = out->size == 0 || out->size > sizeof(T) ? static_cast<uint32_t>(sizeof(T)) : out->size;
    const uint32_t keep = out->size;
    std::memcpy(out, &v, n);
    out->size = keep == 0 ? static_cast<uint32_t>(sizeof(T)) : keep;
}

int api_class_define(const rac1_class_def* d) {
    if (g_cur == nullptr || d == nullptr || d->name == nullptr || d->name[0] == 0 || g_cur->classes.size() >= 64) return 0;
    ClassDef c;
    c.name = d->name;
    std::memcpy(&c.d, d, std::min<size_t>(d->size != 0 ? d->size : sizeof(rac1_class_def), sizeof(rac1_class_def)));
    if (d->size < sizeof(rac1_class_def) && d->size != 0)
        std::memset(reinterpret_cast<char*>(&c.d) + d->size, 0, sizeof(rac1_class_def) - d->size);
    c.d.size = sizeof(rac1_class_def);
    c.d.name = nullptr;
    for (ClassDef& e : g_cur->classes)
        if (e.name == c.name) {
            e = c;
            return 1;
        }
    g_cur->classes.push_back(c);  // (bound at a level's start: classes are not looked at meanwhile)
    return 1;
}

int32_t api_class_find(const char* name) {
    if (name == nullptr) return -1;
    for (size_t i = 0; i < g_sdk.classes.size(); ++i)
        if (g_sdk.classes[i].name == name) return static_cast<int32_t>(i);
    return -1;
}

uint32_t api_object_spawn(int32_t cls, const float pos[3], float yaw) { return obj_spawn(cls, pos, yaw, -1); }

void api_object_remove(uint32_t h) {
    const int i = obj_index(h);
    if (i >= 0) obj_remove(i, true);
}

uint32_t api_object_moby(uint32_t h) {
    const int i = obj_index(h);
    return i >= 0 ? g_sdk.objs[static_cast<size_t>(i)].moby : 0;
}

uint32_t api_object_of_moby(uint32_t m) {
    const int i = obj_of_moby(m);
    return i >= 0 ? obj_handle(static_cast<size_t>(i)) : 0;
}

int32_t api_object_class(uint32_t h) {
    const int i = obj_index(h);
    return i >= 0 ? g_sdk.objs[static_cast<size_t>(i)].cls : -1;
}

void* api_object_state(uint32_t h) {
    const int i = obj_index(h);
    if (i < 0 || g_sdk.objs[static_cast<size_t>(i)].state.empty()) return nullptr;
    return g_sdk.objs[static_cast<size_t>(i)].state.data();
}

// key=value in a placed object's words
bool param_of(const std::string& params, const char* key, std::string& v) {
    std::istringstream in(params);
    std::string w;
    const size_t kn = std::strlen(key);
    while (in >> w)
        if (w.size() > kn && w.compare(0, kn, key) == 0 && w[kn] == '=') {
            v = w.substr(kn + 1);
            return true;
        }
    return false;
}

int api_object_param(uint32_t h, const char* key, char* out, size_t cap) {
    const int i = obj_index(h);
    if (i < 0 || key == nullptr || g_sdk.objs[static_cast<size_t>(i)].placed < 0) return 0;
    std::string v;
    if (!param_of(g_sdk.placed[static_cast<size_t>(g_sdk.objs[static_cast<size_t>(i)].placed)].params, key, v)) return 0;
    if (out != nullptr && cap > 0) {
        const size_t n = std::min(v.size(), cap - 1);
        std::memcpy(out, v.data(), n);
        out[n] = 0;
    }
    return 1;
}

uint32_t api_object_named(const char* name) {
    if (name == nullptr) return 0;
    for (size_t i = 0; i < g_sdk.objs.size(); ++i) {
        const Obj& o = g_sdk.objs[i];
        std::string v;
        if (o.live && o.placed >= 0 && param_of(g_sdk.placed[static_cast<size_t>(o.placed)].params, "name", v) && v == name) return obj_handle(i);
    }
    return 0;
}

void api_object_place(uint32_t h, const float pos[3], const float rot[3], float scale) {
    const int i = obj_index(h);
    if (i < 0) return;
    Obj& o = g_sdk.objs[static_cast<size_t>(i)];
    for (uint32_t k = 0; k < 3; ++k) {
        if (pos != nullptr) swc1(o.moby + 0x10 + 4 * k, pos[k]);
        if (rot != nullptr) swc1(o.moby + 0x40 + 4 * k, fold_angle(rot[k]));
    }
    if (scale > 0.0f) {
        o.scale = scale;
        obj_carrier_scale(o);
    }
    moby_update_transform(o.moby);
}

void api_object_show(uint32_t h, int on) {
    const int i = obj_index(h);
    if (i >= 0) g_sdk.objs[static_cast<size_t>(i)].shown = on != 0;
}

void api_object_solid(uint32_t h, int on) {
    const int i = obj_index(h);
    if (i < 0) return;
    Obj& o = g_sdk.objs[static_cast<size_t>(i)];
    o.solid = on != 0;
    sw(o.moby + 0x94, o.solid ? o.coll : 0);
}

int32_t api_object_count() {
    int32_t n = 0;
    for (const Obj& o : g_sdk.objs) n += o.live ? 1 : 0;
    return n;
}

uint32_t api_object_at(int32_t k) {
    for (size_t i = 0; i < g_sdk.objs.size(); ++i)
        if (g_sdk.objs[i].live && k-- == 0) return obj_handle(i);
    return 0;
}

// ---- (ABI 2) queries ----
int api_collide_ray(const float from[3], const float to[3], uint32_t ignore, uint32_t flags, rac1_ray_hit* out) {
    if (from == nullptr || to == nullptr) return 0;
    const Vec4 a{from[0], from[1], from[2], 1.0f}, b{to[0], to[1], to[2], 1.0f};
    const int32_t r = segment_collide_v(a, b, flags & 0x19u, ignore != 0 && is_moby(ignore) ? ignore : 0, nullptr);
    if (r == 0) return 0;
    rac1_ray_hit h{};
    h.size = sizeof h;
    const uint32_t outb = OV(0x00173e40);
    for (uint32_t k = 0; k < 3; ++k) {
        h.point[k] = lwc1(outb + 0x20 + 4 * k);
        h.normal[k] = lwc1(outb + 0x40 + 4 * k);
    }
    const float nl = std::sqrt(h.normal[0] * h.normal[0] + h.normal[1] * h.normal[1] + h.normal[2] * h.normal[2]);
    for (float& c : h.normal) c = nl > 0.0f && std::isfinite(nl) ? c / nl : 0.0f;  // (the test leaves it unscaled)
    const uint32_t m = lwu(outb + 0x18);
    h.moby = is_moby(m) ? m : 0;
    h.object = api_object_of_moby(h.moby);
    fill_out(out, h);
    return 1;
}

float dist2(const float p[3], uint32_t m) {
    float d = 0;
    for (uint32_t k = 0; k < 3; ++k) {
        const float x = lwc1(m + 0x10 + 4 * k) - p[k];
        d += x * x;
    }
    return d;
}

int32_t nearest(std::vector<std::pair<float, uint32_t>>& v, uint32_t* out, int32_t max) {
    std::stable_sort(v.begin(), v.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const int32_t n = std::min<int32_t>(static_cast<int32_t>(v.size()), std::max(0, max));
    if (out != nullptr)
        for (int32_t k = 0; k < n; ++k) out[k] = v[static_cast<size_t>(k)].second;
    return out != nullptr ? n : static_cast<int32_t>(v.size());
}

int32_t api_objects_near(const float pos[3], float radius, uint32_t* out, int32_t max) {
    if (pos == nullptr) return 0;
    std::vector<std::pair<float, uint32_t>> v;
    for (size_t i = 0; i < g_sdk.objs.size(); ++i) {
        if (!g_sdk.objs[i].live) continue;
        const float d = dist2(pos, g_sdk.objs[i].moby);
        if (d <= radius * radius) v.emplace_back(d, obj_handle(i));
    }
    return nearest(v, out, max);
}

int32_t api_mobys_near(const float pos[3], float radius, int32_t cls, uint32_t* out, int32_t max) {
    if (pos == nullptr) return 0;
    std::vector<std::pair<float, uint32_t>> v;
    for (uint32_t m = moby_base(), end = lwu(OV(0x0015ffe0)); m != 0 && m <= end; m += 0x100) {
        const int8_t st = static_cast<int8_t>(lbu(m + 0x20));
        if (st == -1) break;
        if (st < 0 || (cls >= 0 && lh(m + 0xa6) != cls)) continue;
        const float d = dist2(pos, m);
        if (d <= radius * radius) v.emplace_back(d, m);
    }
    return nearest(v, out, max);
}

// ---- (ABI 2) damage ----
void api_hit_send(uint32_t m, uint32_t kinds, float strength, uint32_t from) {
    if (!is_moby(m)) return;
    if (from != 0 && !is_moby(from)) from = 0;
    const uint32_t at = from != 0 ? from : m;
    for (uint32_t k = 0; k < 3; ++k) swc1(SCRATCH + 0x40 + 4 * k, lwc1(at + 0x10 + 4 * k));
    swc1(SCRATCH + 0x4c, 1.0f);
    sub_00259bc8_v(m, from, kinds, SCRATCH + 0x40, Vec4{0.0f, 0.0f, 0.0f, 0.0f}, strength);
}

int api_hero_damage(int32_t amount, uint32_t from) {
    const uint32_t h = moby_base();
    if (amount <= 0 || h == 0) return 0;
    if (from != 0 && !is_moby(from)) from = 0;
    // as the game's enemies hurt him (a Novalis one, 0x00259bc8 with kind 1): at his position 0.75 up, a push of 0.2 away
    // from the attacker (none: from where he faces)
    float dx, dy;
    if (from != 0) {
        dx = lwc1(h + 0x10) - lwc1(from + 0x10);
        dy = lwc1(h + 0x14) - lwc1(from + 0x14);
    } else {
        dx = -std::cos(lwc1(h + 0x48));
        dy = -std::sin(lwc1(h + 0x48));
    }
    const float len = std::sqrt(dx * dx + dy * dy);
    if (len > 1e-4f) {
        dx = dx / len * 0.2f;
        dy = dy / len * 0.2f;
    } else {
        dx = 0.2f;
        dy = 0.0f;
    }
    for (uint32_t k = 0; k < 3; ++k) swc1(SCRATCH + 0x40 + 4 * k, lwc1(h + 0x10 + 4 * k));
    swc1(SCRATCH + 0x48, lwc1(SCRATCH + 0x48) + 0.75f);
    swc1(SCRATCH + 0x4c, 1.0f);
    sub_00259bc8_v(h, from, RAC1_HIT_HERO, SCRATCH + 0x40, Vec4{dx, dy, 0.0f, 0.0f}, static_cast<float>(amount));
    return 1;
}

int api_hit_take(uint32_t m, uint32_t kinds, rac1_hit* out) {
    rac1_hit h{};
    if (!take_hit(m, kinds, h)) return 0;
    fill_out(out, h);
    return 1;
}

// ---- (ABI 2) sounds ----
int32_t api_sfx_find(const char* name) {
    if (name == nullptr) return -1;
    for (size_t i = 0; i < g_sdk.sounds.size(); ++i)
        if (g_sdk.sounds[i].name == name) return static_cast<int32_t>(i);
    return -1;
}

// slot `slot` (sound_start's 0x13e550 + slot * 0x70: +0x74 its state, +0x78 its definition) plays the level's sound
// `sound` (-1: any of the level's)
int api_sfx_active(int32_t slot, int32_t sound) {
    if (slot < 0 || slot >= 0x1e || g_sdk.bank == 0) return 0;
    const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot) * 0x70, def = lwu(e + 0x78);
    if (lbu(e + 0x74) == 0 || def < SFX_DEFS || def >= SFX_DEFS + SFX_MAX * 0x20) return 0;
    return sound < 0 || def == SFX_DEFS + static_cast<uint32_t>(sound) * 0x20 ? 1 : 0;
}

int32_t api_sfx_play(int32_t sound, uint32_t m, const float pos[3]) {
    if (g_sdk.bank == 0 || sound < 0 || static_cast<size_t>(sound) >= g_sdk.sounds.size()) return -1;
    if (m != 0 && !is_moby(m)) m = 0;
    uint32_t at = 0;
    if (m == 0 && pos != nullptr) {
        for (uint32_t k = 0; k < 3; ++k) swc1(SCRATCH + 0x60 + 4 * k, pos[k]);
        swc1(SCRATCH + 0x6c, 1.0f);
        at = SCRATCH + 0x60;
    }
    const int32_t s = sound_start(SFX_DEFS + static_cast<uint32_t>(sound) * 0x20, g_sdk.sounds[static_cast<size_t>(sound)].loop ? 4u : 0u, m, at, 0x400);
    if (s >= 0) sw(0x0013e550 + static_cast<uint32_t>(s) * 0x70 + 0x88, m);  // (the moby it follows, as sound_play_indexed)
    return s;
}

void api_sfx_stop(int32_t slot) {
    if (api_sfx_active(slot, -1)) sub_0028d918(slot);
}

// ---- (ABI 2) triggers, time ----
int32_t api_trigger_find(const char* name) {
    if (name == nullptr) return -1;
    for (size_t i = 0; i < g_sdk.triggers.size(); ++i)
        if (g_sdk.triggers[i].name == name) return static_cast<int32_t>(i);
    return -1;
}

int api_trigger_inside(int32_t i) { return i >= 0 && static_cast<size_t>(i) < g_sdk.triggers.size() && g_sdk.triggers[static_cast<size_t>(i)].inside ? 1 : 0; }

uint64_t api_frame_count() { return g_frames; }

// ---- (ABI 3) the base level's objects at run time: the spawn pools (level.txt's spawn lines) ----
// The composer places each pool's objects as level.txt's moby lines are placed (copies of the base level's first placement
// of the class, their pvars copied); when the level has loaded they are taken out of play (0xfd: a slot of the level's
// placed range, which the game's moby_spawn never gives out again), each one's moby and pvars kept as the load left
// them. base_spawn starts one again where the plugin says: its moby and pvars as kept (the pvars' own pointers stay
// right: the same block), not in the grid (moby_update_transform puts it there), its bolts on (+0xb1, moby_drop_bolts).
// (its profile's data and host behaviours, class_profiles.h: the paths of Kalebo III's enemies, ABI 5)
using PoolObj = ProfObj;
std::vector<PoolObj> g_pool;

// a started pool object still lives (its moby its class's, its pvars its own, in play)
bool pool_alive(const PoolObj& o) { return prof_alive(o); }

void pool_take(int slot) {
    g_pool.clear();
    const ModInfo* lm = slot >= kFirstLevelSlot ? mods_level_at_slot(slot) : nullptr;
    if (lm == nullptr || lm->level_pools.empty()) return;
    size_t n = 0;
    for (const ModInfo::LevelPool& lp : lm->level_pools)
        for (int uid : lp.uids) {
            const uint32_t m = api_moby_by_uid(uid);
            if (m == 0 || lh(m + 0xa6) != lp.cls) {
                std::fprintf(stderr, "[sdk] the spawn pool's object %d (class %d) is not on the level\n", uid, lp.cls);
                continue;
            }
            PoolObj o;
            o.cls = lp.cls;
            o.moby = m;
            o.pvars = lwu(m + 0x78);
            o.m.resize(0x100);
            for (uint32_t i = 0; i < 0x100; ++i) o.m[i] = static_cast<uint8_t>(lbu(m + i));
            if (o.pvars != 0 && lp.pvar_size > 0 && addr_ok(o.pvars, static_cast<uint32_t>(lp.pvar_size))) {
                o.pv.resize(static_cast<size_t>(lp.pvar_size));
                for (uint32_t i = 0; i < o.pv.size(); ++i) o.pv[i] = static_cast<uint8_t>(lbu(o.pvars + i));
            }
            moby_retire(m);
            g_pool.push_back(std::move(o));
            ++n;
        }
    std::printf("[sdk] level slot %d: %zu objects in the spawn pools (base_spawn)\n", slot, n);
    profiles_pool_take(g_pool, lw(LEVEL));
}

uint32_t api_base_spawn(int32_t cls, const float pos[3], float yaw) {
    if (pos == nullptr) return 0;
    for (size_t i = 0; i < g_pool.size(); ++i) {
        PoolObj& o = g_pool[i];
        if (o.cls != cls || o.refused) continue;
        if (o.out && pool_alive(o)) continue;
        if (o.out) o.out = false;  // (gone: its room free again)
        if (static_cast<int8_t>(lbu(o.moby + 0x20)) >= 0) continue;  // (in play otherwise: not ours to take)
        const uint32_t m = o.moby;
        const uint32_t grid = lwu(m + 0xa0);  // (the grid's rectangle as the retire left it: none)
        for (uint32_t k = 0; k < 0x100; ++k) sb(m + k, o.m[k]);
        sw(m + 0xa0, grid);
        for (uint32_t k = 0; k < o.pv.size(); ++k) sb(o.pvars + k, o.pv[k]);
        sb(m + 0xb1, 0xff);  // (its bolts: moby_drop_bolts drops none for 0xfe, a placement with no spawn rule)
        for (uint32_t k = 0; k < 3; ++k) swc1(m + 0x10 + 4 * k, pos[k]);
        swc1(m + 0x48, fold_angle(yaw));
        moby_update_transform(m);
        o.out = true;
        o.start[0] = pos[0], o.start[1] = pos[1], o.start[2] = pos[2], o.start[3] = fold_angle(yaw);
        profiles_spawned(g_pool, i);
        ++o.gen;
        if (o.gen == 0) o.gen = 1;
        return static_cast<uint32_t>(o.gen) << 16 | static_cast<uint32_t>(i + 1);
    }
    return 0;
}

uint32_t api_base_moby(uint32_t h) {
    const size_t i = (h & 0xffffu) - 1;
    if (h == 0 || i >= g_pool.size()) return 0;
    PoolObj& o = g_pool[i];
    if (o.gen != (h >> 16) || !o.out) return 0;
    if (!pool_alive(o)) {
        o.out = false;
        return 0;
    }
    return o.moby;
}

int32_t api_base_free(int32_t cls) {
    int32_t n = 0;
    for (PoolObj& o : g_pool) {
        if (o.cls != cls) continue;
        if (o.out && !pool_alive(o)) o.out = false;
        if (!o.out && static_cast<int8_t>(lbu(o.moby + 0x20)) < 0) ++n;
    }
    return n;
}

// (ABI 4 / 5) base_hunt, base_option and Kalebo III's enemies on a level mod: their profiles (data/pool/profiles) and host
// behaviours, class_profiles.cpp

// ---- (ABI 6) classes of other levels (the open engine: session.h) ----
int32_t api_import_class(const char* name) {
    int l = -1;
    uint32_t c = 0;
    return name != nullptr && pool_name(name, &l, &c) ? session_import_id(l, c) : -1;
}

uint32_t api_import_spawn(int32_t cls, const float pos[3], float yaw) {
    if (cls < 0 || pos == nullptr || lw(OV(0x0015f5c4)) != 0) return 0;
    if (static_cast<uint32_t>(cls) < 2048 && lbu(OV(0x00197bc0) + static_cast<uint32_t>(cls)) == 0xffu) return 0;
    return session_import_spawn(static_cast<uint32_t>(cls), pos, yaw);
}

int32_t api_import_load(const char* name) {
    int l = -1;
    uint32_t c = 0;
    if (name == nullptr || !pool_name(name, &l, &c)) return -1;
    return session_import_load(l, c, "plugin");
}

// ---- (ABI 9) sessions (session_runtime.h) ----
int api_session_on() { return g_session_rt ? 1 : 0; }

int api_checkpoint_set(const char* name) { return name != nullptr && session_checkpoint_set(name) ? 1 : 0; }

int api_checkpoint_at(const float pos[3], float yaw, const char* name) {
    return pos != nullptr && session_checkpoint_at(pos, yaw, name != nullptr ? name : "") ? 1 : 0;
}

int32_t api_checkpoint_get(float pos[3], float* yaw, char* name, int32_t cap) {
    if (!g_session_rt) return -1;
    const std::string n = session_checkpoint(pos, yaw);
    if (name != nullptr && cap > 0) {
        const size_t k = std::min<size_t>(n.size(), static_cast<size_t>(cap) - 1);
        std::memcpy(name, n.data(), k);
        name[k] = 0;
    }
    return static_cast<int32_t>(session_respawns());
}

int32_t api_respawn_policy(int32_t policy) { return session_respawn_policy(policy); }

// a loaded save's session bytes of the plugins (session_load), given after their level_start
std::vector<std::pair<std::string, std::vector<uint8_t>>> g_session_bytes;

void session_bytes_give() {
    for (const auto& [folder, bytes] : g_session_bytes)
        for (Plugin& p : g_plugins)
            if (p.mod->folder == folder && p.cb.size >= offsetof(rac1_mod, session_load) + sizeof(p.cb.session_load) && p.cb.session_load != nullptr)
                with(p, [&] { p.cb.session_load(p.cb.user, bytes.data(), static_cast<uint32_t>(bytes.size())); });
    g_session_bytes.clear();
}

// ---- (ABI 8) the level data of imported classes (class_profiles.h) ----
// the plugins' paths (path_create, handles 0x10001..; level.txt's are 1..): the level's start empties them
std::vector<SdkPath> g_plugin_paths;

const SdkPath* path_of(int32_t h) {
    if (h >= 1 && static_cast<size_t>(h) <= g_sdk.paths.size()) return &g_sdk.paths[static_cast<size_t>(h) - 1];
    if (h > 0x10000 && static_cast<size_t>(h - 0x10000) <= g_plugin_paths.size()) return &g_plugin_paths[static_cast<size_t>(h - 0x10000) - 1];
    return nullptr;
}

int32_t api_path_find(const char* name) {
    if (name == nullptr) return 0;
    for (size_t i = 0; i < g_sdk.paths.size(); ++i)
        if (g_sdk.paths[i].name == name) return static_cast<int32_t>(i + 1);
    return 0;
}

int32_t api_path_create(const float* xyz, int32_t n) {
    if (xyz == nullptr || n < 1 || n > 64 || g_plugin_paths.size() >= 4096) return 0;
    SdkPath p;
    for (int32_t i = 0; i < n; ++i) {
        if (!std::isfinite(xyz[3 * i]) || !std::isfinite(xyz[3 * i + 1]) || !std::isfinite(xyz[3 * i + 2])) return 0;
        p.pts.push_back({xyz[3 * i], xyz[3 * i + 1], xyz[3 * i + 2]});
    }
    g_plugin_paths.push_back(std::move(p));
    return static_cast<int32_t>(0x10000 + g_plugin_paths.size());
}

int api_object_bind_path(uint32_t m, const char* slot, int32_t path) {
    const SdkPath* p = path_of(path);
    if (slot == nullptr || p == nullptr || !is_moby(m)) return 0;
    float pts[64][3];
    for (size_t i = 0; i < p->pts.size() && i < 64; ++i)
        for (int c = 0; c < 3; ++c) pts[i][c] = p->pts[i][static_cast<size_t>(c)];
    return profiles_bind_points(m, slot, pts, static_cast<int>(std::min<size_t>(p->pts.size(), 64)));
}

int api_object_bind(uint32_t m, const char* slot, uint32_t value) {
    if (slot == nullptr || !is_moby(m)) return 0;
    const ProfObj* o = profiles_obj(m);
    uint32_t off = 0;
    std::string kind;
    if (o == nullptr || !profile_slot(o->prof, slot, &off, &kind) || kind == "path" || kind == "area") return 0;
    if (kind == "moby") value = value == 0 ? 0xffffffffu : is_moby(value) ? (value - moby_base()) >> 8 : 0xffffffffu;
    return profiles_bind_word(m, slot, value);
}

int32_t api_group_create(uint32_t owner, const uint32_t* mobys, int32_t n) {
    const ProfObj* o = is_moby(owner) ? profiles_obj(owner) : nullptr;  // (an imported object, or the base level's own: ABI 9)
    if (o == nullptr || mobys == nullptr || n < 1 || n > 64) return -1;
    for (int32_t i = 0; i < n; ++i)
        if (!is_moby(mobys[i])) return -1;
    return ld_group_create(o->level, mobys, n);
}

int32_t api_object_slots(uint32_t m, char* out, int32_t cap) {
    const ProfObj* o = is_moby(m) ? profiles_obj(m) : nullptr;
    if (o == nullptr) return 0;
    const std::string t = profile_slots_text(o->prof);
    if (out != nullptr && cap > 0) {
        const size_t k = std::min<size_t>(t.size(), static_cast<size_t>(cap) - 1);
        std::memcpy(out, t.data(), k);
        out[k] = 0;
    }
    return static_cast<int32_t>(std::count(t.begin(), t.end(), ':'));
}

int32_t api_import_release(const char* name) {
    int l = -1;
    uint32_t c = 0;
    if (name == nullptr || !pool_name(name, &l, &c)) return 0;
    return static_cast<int32_t>(session_import_unload(l, c));
}

// (ABI 5) base_option: a class's choice for an object base_spawn has just started (before its first update): its profile's
// options (class_profiles.h)
int api_base_option(uint32_t h, int32_t option, int32_t value) {
    const size_t i = (h & 0xffffu) - 1;
    if (h == 0 || i >= g_pool.size()) return 0;
    PoolObj& o = g_pool[i];
    if (o.gen != (h >> 16) || !pool_alive(o) || !o.adapted || lbu(o.moby + 0x20) != 0) return 0;
    return std::max<int32_t>(0, profiles_option(g_pool, i, option, value));
}

uint32_t pool_index_of(uint32_t m) {
    for (size_t i = 0; i < g_pool.size(); ++i)
        if (g_pool[i].moby == m && pool_alive(g_pool[i])) return static_cast<uint32_t>(i + 1);
    return 0;
}

// (ABI 5) base_handle: the handle of a moby base_spawn started (one a dropship brought, found with mobys_near)
uint32_t api_base_handle(uint32_t m) {
    const uint32_t pi = m != 0 ? pool_index_of(m) : 0;
    return pi == 0 ? 0 : static_cast<uint32_t>(g_pool[pi - 1].gen) << 16 | pi;
}

int api_base_hunt(uint32_t m, int on) {
    if (!is_moby(m) || static_cast<int8_t>(lbu(m + 0x20)) < 0) return 0;
    const int r = profiles_hunt(g_pool, m, on);  // (its profile's hunt: class_profiles.h)
    return r >= 0 ? r : on == 0 ? 1 : 0;
}

// ---- (ABI 5) the weapons: owned, equipped, allowed, ammo ----
// The game's tables: the items owned 0x13d4c0[0x25] (a byte each, item_give 0x260860 sets it), the ammo 0x13d428[item] and its
// most, the item table 0x1c40b0 (0x18 a row: +8 non-zero for an item that takes ammo, +0xe the most: what the HUD, the ammo
// pickups 0x2a5bd8 and the quick select stop at), the item asked in hand P+0x20b8 (what the quick select writes when Triangle
// is let go, 0x237ba0; the hero's slot update 0x20fea0 then switches, its animation and its sound as for the quick select;
// the Drone Device 0x18 by P+0x1ff5), the item in hand P+0x10b8, the quick select's ring 0x141ea0[8].
constexpr uint32_t W_P = 0x0013f350u, W_OWNED = 0x0013d4c0u, W_AMMO = 0x0013d428u, W_RING = 0x00141ea0u, W_WRENCH = 8;
constexpr OvConst<0x001c40b0u> W_ROWS{};
// the pause menu's Weapons page's items (the weapons: 0x09 Suck Cannon .. 0x19 Decoy Glove, not the Swingshot 0x0c nor the
// Hydrodisplacer 0x16)
bool weapon_item(uint32_t i) { return i >= 0x09 && i <= 0x19 && i != 0x0c && i != 0x16; }
bool item_owned(uint32_t i) { return i < 0x25 && lbu(W_OWNED + i) != 0; }
uint8_t g_allow[0x25];  // weapon_allow's (g_mod_weapons_limited: only these)
uint32_t g_w_last = W_WRENCH;  // the last item in hand that was allowed
uint32_t g_w_wait = 0;          // frames a weapon not allowed has stayed in hand while a switch to an allowed one was pending

int32_t api_weapons_owned(int32_t* out, int32_t max) {
    int32_t n = 0;
    for (uint32_t i = 0; i < 0x25; ++i)
        if (weapon_item(i) && item_owned(i)) {
            if (out != nullptr && n < max) out[n] = static_cast<int32_t>(i);
            ++n;
        }
    return n;
}

// where the hero could open the quick select (hero_input_update 0x229b70's conditions, as advanced.cpp's item_cycle): standing,
// walking, running, crouching, the jumps and falls the quick select opens in, strafing; not swinging the wrench nor throwing
// it, not in a vehicle, a ship or on a hoverboard (+0x20a4 not 0 or 3), not while an item is being switched or used (+0x10ac
// 2, +0x20ac / +0x20a7), nor with the game's input taken (0x15f594)
bool hero_can_switch() {
    const int32_t st = lw(W_P + 0x2084);
    const bool aim = st == 0 || st == 2 || st == 3 || st == 0x73 || st == 7 || (st >= 9 && st <= 0x12) || st == 0x1c || st == 0x3c || st == 0x49 ||
                     st == 0x4c || st == 0x5e || st == 0x69;
    const uint32_t mode = lbu(W_P + 0x20a4);
    if (!aim || lbu(W_P + 0x10ac) == 2 || lw(OV(0x0015f594)) != 0 || (mode != 0 && mode != 3)) return false;
    if (!(lbu(W_P + 0x20ac) == 0 || mode == 1 || lh(W_P + 0x1b2) != 0)) return false;
    if (!(lbu(W_P + 0x20a7) == 0 || mode == 1 || lh(W_P + 0x1b2) != 0)) return false;
    return true;
}

// the quick select's equip (0x237ba0 on Triangle's release): the request, the ammo clamped to its most
void weapon_request(uint32_t item) {
    if (item == 0x18) sb(W_P + 0x1ff5, 1);
    else sw(W_P + 0x20b8, item);
    const uint32_t row = item * 0x18 + W_ROWS;
    if (lhu(row + 8) != 0) {
        const uint32_t a = W_AMMO + item * 4;
        const int32_t mx = static_cast<int32_t>(lhu(row + 0xe));
        if (mx < lw(a)) sw(a, static_cast<uint32_t>(mx));
    }
}

int api_weapon_equip(int32_t item) {
    if (g_in_hud || ee_journal_active() || moby_base() == 0 || lw(OV(0x0015f5c4)) != 0) return 0;
    const uint32_t i = static_cast<uint32_t>(item);
    if (item < 0 || !weapon_item(i) || !item_owned(i) || !mod_code_weapon_allowed(i)) return 0;
    if (lwu(W_P + 0x10b8) == i) return 1;  // (in hand already)
    if (!hero_can_switch()) return 0;
    weapon_request(i);
    return 1;
}

void api_weapon_allow(const int32_t* items, int32_t n) {
    std::memset(g_allow, 0, sizeof g_allow);
    g_mod_weapons_limited = items != nullptr && n > 0;
    g_w_last = W_WRENCH;
    g_w_wait = 0;
    if (!g_mod_weapons_limited) return;
    for (int32_t k = 0; k < n; ++k)
        if (items[k] >= 0 && items[k] < 0x25) g_allow[items[k]] = 1;
}

int api_ammo_fill(int32_t item) {
    if (item < 0 || item >= 0x25 || !item_owned(static_cast<uint32_t>(item))) return 0;
    const uint32_t row = static_cast<uint32_t>(item) * 0x18 + W_ROWS;
    if (lhu(row + 8) == 0) return 0;
    sw(W_AMMO + static_cast<uint32_t>(item) * 4, lhu(row + 0xe));
    return 1;
}

// an item the hand may go to while weapon_allow limits the weapons: the wrench, an allowed weapon
bool weapon_target_ok(uint32_t i) { return i == W_WRENCH || (i < 0x25 && weapon_item(i) && g_allow[i] != 0); }

// after the frame's updates, while weapon_allow limits them: a weapon not allowed asked in hand (the game's own return to the
// weapon before a gadget, Triangle's double press in a level's own build of the hero code, an item given with its equip) not
// taken; one in hand (asked and switched within the frame) put back for the last item in hand that was allowed (else the
// wrench) - but not while a switch to an allowed item is under way: asked (+0x20b8: weapon_equip's, the quick select's) or
// taken by the hero's slot update (0x20fea0 moves the request into the slot's kind +0x20d4 and clears it; the hand +0x10b8
// is 0 while the old item goes away, then the new one, some frames later). So weapon_allow({new}) and weapon_equip(new), in
// either order, in one frame or across frames, end with the new weapon in hand, not the wrench. (A switch still pending
// after 2 s is overruled.)
void weapons_frame() {
    const uint32_t req = lwu(W_P + 0x20b8);
    if (req < 0x25 && weapon_item(req) && g_allow[req] == 0) sw(W_P + 0x20b8, 0);
    const uint32_t now = lwu(W_P + 0x10b8);
    const bool bad = now < 0x25 && weapon_item(now) && g_allow[now] == 0;
    if (!bad) {
        g_w_wait = 0;
        if (now != 0 && now < 0x25 && (now == W_WRENCH || weapon_item(now))) g_w_last = now;
        return;
    }
    const uint32_t asked = lwu(W_P + 0x20b8), taking = lwu(W_P + 0x20d4);
    const bool pending = weapon_target_ok(asked) || (asked == 0 && taking != now && weapon_target_ok(taking));
    if (pending && ++g_w_wait <= 120) return;
    if (asked != 0 && !weapon_target_ok(asked)) return;  // (another item asked: a gadget's own switch)
    g_w_wait = 0;
    const bool ok = g_w_last == W_WRENCH || (weapon_item(g_w_last) && item_owned(g_w_last) && g_allow[g_w_last] != 0);
    weapon_request(ok ? g_w_last : W_WRENCH);
}


int api_light_add(const float pos[3], const float rgb[3], float intensity, float radius) {
    if (pos == nullptr || rgb == nullptr || g_in_hud || ee_journal_active()) return 0;
    if (!(radius > 0.0f) || !(intensity > 0.0f) || !std::isfinite(radius) || !std::isfinite(intensity)) return 0;
    const uint32_t n = lwu(MOD_LIGHTS);
    if (n >= MOD_LIGHT_MAX) return 0;
    const uint32_t e = MOD_LIGHTS + 0x10 + n * 0x20;
    for (uint32_t k = 0; k < 3; ++k) {
        if (!std::isfinite(pos[k]) || !std::isfinite(rgb[k])) return 0;
        swc1(e + 4 * k, pos[k]);
        swc1(e + 0x10 + 4 * k, std::clamp(rgb[k], 0.0f, 1.0f) * std::min(intensity, 8.0f));
    }
    swc1(e + 0xc, std::min(radius, 60.0f));
    sw(e + 0x1c, 0);
    sw(MOD_LIGHTS, n + 1);
    return 1;
}

// ---- (ABI 4) the vendor, the pad held, the HUD's panels, anchors and sizes ----
int api_vendor_open() {
    // as the game's own item 0x20 opens them (hero_item_use: 0x29a6b0 with no Vendor moby), in play only (game mode 0), not
    // from the HUD callback nor while the renderer replays a frame
    if (g_in_hud || ee_journal_active() || lw(OV(0x0015f5c4)) != 0 || moby_base() == 0) return 0;
    vendor_screen_start_tr(0);
    return lw(OV(0x0015f5c4)) == 5 ? 1 : 0;
}

// input_hold: the pad kept from the game while on (but Start and Select: the pause menu, the map), in play (game mode 0)
// only; the buttons still held when it is let go stay the game's no more until released (a Cross that closed the plugin's
// menu does not jump)
constexpr uint32_t HOLD_KEEP = 0x0900u;  // (Start 0x800, Select 0x100)
bool g_hold = false;
uint32_t g_hold_sticky = 0;
bool g_pad_masked = false;  // (the game's pad fields masked by us: the real ones put back before the next read)
uint32_t g_pad_real_1a0 = 0, g_pad_real_1b0 = 0, g_pad_real_348 = 0;

void api_input_hold(int on) {
    if (on != 0) {
        g_hold = true;
        return;
    }
    if (g_hold) g_hold_sticky = (g_pad_real_1a0 | g_pad_real_1b0) & ~HOLD_KEEP;
    g_hold = false;
}

// the anchor of a HUD element (RAC1_HUD_*): the side whose edge it moves to with a HUD wider than 4:3 (render_view_hud_note,
// as the game's own side elements; not on the vendor's screens, 4:3 there)
uint8_t hud_side(int32_t anchor) {
    if (anchor != 1 && anchor != 2) return 0;
    if (!render_view_hud_wide() || mrp_lod_shadow_slot() >= 0 || lw(OV(0x0015f5c4)) == 5) return 0;
    return static_cast<uint8_t>(anchor);
}
constexpr OvConst<0x001611c0u> HUD_CUR{};  // (the packet's cursor)

uint32_t hud_string(const char* text) {
    const uint32_t s = STRINGS + (g_hud_string++ % STRING_COUNT) * STRING_SIZE;
    uint32_t i = 0;
    for (; text[i] != 0 && i + 1 < STRING_SIZE; ++i) {
        const char c = text[i];
        sb(s + i, c >= 0x20 && c < 0x7f ? static_cast<uint32_t>(static_cast<uint8_t>(c)) : static_cast<uint32_t>('?'));
    }
    sb(s + i, 0);
    return s;
}

void api_hud_rect(int32_t x, int32_t y, int32_t w, int32_t h, uint32_t abgr, int32_t anchor) {
    if (!g_in_hud || w <= 0 || h <= 0) return;
    const uint8_t side = hud_side(anchor);
    const uint32_t at = lwu(HUD_CUR);
    gs_flat_rect(x, y, x + w, y + h, static_cast<uint64_t>(abgr), 0);
    if (side != 0) render_view_hud_note(at, lwu(HUD_CUR), side);
}

void api_hud_text_ex(int32_t x, int32_t y, uint32_t abgr, float scale, int32_t anchor, const char* text) {
    if (!g_in_hud || text == nullptr || !(scale > 0.0f) || !std::isfinite(scale)) return;
    scale = std::min(scale, 8.0f);
    const uint8_t side = hud_side(anchor);
    const uint32_t s = hud_string(text);
    const uint32_t at = lwu(HUD_CUR);
    const uint64_t c = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(abgr)));
    if (scale == 1.0f) text_draw(x, y, c, s, -1, effect_tex0(1), OV(0x001c3150));
    else text_draw_f(c, s, -1, effect_tex0(1), OV(0x001c3150), static_cast<float>(x), static_cast<float>(y), scale);
    if (side != 0) render_view_hud_note(at, lwu(HUD_CUR), side);
}

int32_t api_hud_text_width(const char* text, float scale) {
    if (text == nullptr || !(scale > 0.0f)) return 0;
    // (the small font's advances, as text_width_small counts them: a copy in host memory, the HUD's strings are the HUD's)
    const uint32_t t = OV(0x001c3150);
    int32_t w = 0;
    for (const char* p = text; *p != 0; ++p) {
        const uint8_t c = static_cast<uint8_t>(*p >= 0x20 && *p < 0x7f ? *p : '?');
        w += static_cast<int8_t>(lbu(t + c * 4u + 3));
    }
    return static_cast<int32_t>(static_cast<float>(w) * std::min(scale, 8.0f) + 0.5f);
}

// ---- loading ----
void* lib_open(const std::string& path, std::string& err) {
#if defined(_WIN32)
    const int n = MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, nullptr, 0);
    std::wstring w(static_cast<size_t>(n > 0 ? n : 1), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, path.c_str(), -1, w.data(), n);
    HMODULE h = LoadLibraryW(w.c_str());
    if (h == nullptr) err = "LoadLibrary failed (" + std::to_string(GetLastError()) + ")";
    return reinterpret_cast<void*>(h);
#else
    void* h = dlopen(path.c_str(), RTLD_NOW | RTLD_LOCAL);
    if (h == nullptr) err = dlerror();
    return h;
#endif
}

void* lib_sym(void* lib, const char* name) {
#if defined(_WIN32)
    return reinterpret_cast<void*>(GetProcAddress(reinterpret_cast<HMODULE>(lib), name));
#else
    return dlsym(lib, name);
#endif
}

void lib_close(void* lib) {
#if defined(_WIN32)
    FreeLibrary(reinterpret_cast<HMODULE>(lib));
#else
    dlclose(lib);
#endif
}

void shutdown_all() {
    for (Plugin& p : g_plugins) {
        if (p.cb.shutdown != nullptr) with(p, [&] { p.cb.shutdown(p.cb.user); });
        // (the libraries stay mapped: the process ends)
    }
    g_plugins.clear();
    g_mod_code_any = false;
}

void load_all() {
    g_tried = true;
    const std::vector<const ModInfo*> mods = mods_code();
    if (mods.empty()) return;
    g_api.abi = RAC1_MOD_ABI;
    g_api.size = sizeof(rac1_api);
    g_api.ram = ee_get_ram_base();
    g_api.log = api_log;
    g_api.mod_path = api_mod_path;
    g_api.ov = api_ov;
    g_api.sw = api_sw;
    g_api.sh = api_sh;
    g_api.sb = api_sb;
    g_api.swf = api_swf;
    g_api.ee_alloc = api_ee_alloc;
    g_api.replaying = api_replaying;
    g_api.moby_by_uid = api_moby_by_uid;
    g_api.moby_spawn = api_moby_spawn;
    g_api.moby_remove = api_moby_remove;
    g_api.moby_moved = api_moby_moved;
    g_api.moby_link = api_moby_link;
    g_api.moby_set_link = api_moby_set_link;
    g_api.collide_line = api_collide_line;
    g_api.hero = api_hero;
    g_api.hero_place = api_hero_place;
    g_api.bolts = api_bolts;
    g_api.bolts_add = api_bolts_add;
    g_api.health = api_health;
    g_api.sound = api_sound;
    g_api.music = api_music;
    g_api.smoke = api_smoke;
    g_api.hud_text = api_hud_text;
    g_api.camera_position = api_camera_position;
    g_api.pad_held = api_pad_held;
    g_api.pad_pressed = api_pad_pressed;
    g_api.random = api_random;
    g_api.call_port = api_call_port;
    g_api.class_define = api_class_define;
    g_api.class_find = api_class_find;
    g_api.object_spawn = api_object_spawn;
    g_api.object_remove = api_object_remove;
    g_api.object_moby = api_object_moby;
    g_api.object_of_moby = api_object_of_moby;
    g_api.object_class = api_object_class;
    g_api.object_state = api_object_state;
    g_api.object_param = api_object_param;
    g_api.object_named = api_object_named;
    g_api.object_place = api_object_place;
    g_api.object_show = api_object_show;
    g_api.object_solid = api_object_solid;
    g_api.object_count = api_object_count;
    g_api.object_at = api_object_at;
    g_api.collide_ray = api_collide_ray;
    g_api.objects_near = api_objects_near;
    g_api.mobys_near = api_mobys_near;
    g_api.hero_damage = api_hero_damage;
    g_api.hit_send = api_hit_send;
    g_api.hit_take = api_hit_take;
    g_api.sfx_find = api_sfx_find;
    g_api.sfx_play = api_sfx_play;
    g_api.sfx_stop = api_sfx_stop;
    g_api.sfx_active = api_sfx_active;
    g_api.trigger_find = api_trigger_find;
    g_api.trigger_inside = api_trigger_inside;
    g_api.frame_count = api_frame_count;
    g_api.base_spawn = api_base_spawn;
    g_prof_base_spawn = api_base_spawn;
    g_prof_pool = &g_pool;
    g_api.base_moby = api_base_moby;
    g_api.base_free = api_base_free;
    g_api.vendor_open = api_vendor_open;
    g_api.input_hold = api_input_hold;
    g_api.hud_rect = api_hud_rect;
    g_api.hud_text_ex = api_hud_text_ex;
    g_api.hud_text_width = api_hud_text_width;
    g_api.base_hunt = api_base_hunt;
    g_api.light_add = api_light_add;
    g_api.base_option = api_base_option;
    g_api.base_handle = api_base_handle;
    g_api.weapons_owned = api_weapons_owned;
    g_api.weapon_equip = api_weapon_equip;
    g_api.weapon_allow = api_weapon_allow;
    g_api.ammo_fill = api_ammo_fill;
    g_api.import_class = api_import_class;
    g_api.import_spawn = api_import_spawn;
    g_api.import_load = api_import_load;
    g_api.import_release = api_import_release;
    g_api.path_find = api_path_find;
    g_api.path_create = api_path_create;
    g_api.object_bind_path = api_object_bind_path;
    g_api.object_bind = api_object_bind;
    g_api.group_create = api_group_create;
    g_api.object_slots = api_object_slots;
    g_api.session_on = api_session_on;
    g_api.checkpoint_set = api_checkpoint_set;
    g_api.checkpoint_at = api_checkpoint_at;
    g_api.checkpoint_get = api_checkpoint_get;
    g_api.respawn_policy = api_respawn_policy;
    g_plugins.reserve(mods.size());  // (g_cur points into it)
    for (const ModInfo* m : mods) {
        std::string err;
        void* lib = lib_open(m->plugin.string(), err);
        if (lib == nullptr) {
            std::fprintf(stderr, "[mods] %s: its code cannot be loaded: %s\n", m->folder.c_str(), err.c_str());
            mods_code_failed(m, "its code cannot be loaded (" + err + ")");
            continue;
        }
        const auto abi = reinterpret_cast<AbiFn>(lib_sym(lib, "rac1_mod_abi"));
        const auto main_fn = reinterpret_cast<MainFn>(lib_sym(lib, "rac1_mod_main"));
        const unsigned want = abi != nullptr ? abi() : 0;
        if (main_fn == nullptr || abi == nullptr || want == 0 || want > RAC1_MOD_ABI) {
            const std::string why = main_fn == nullptr || abi == nullptr ? "no rac1_mod_main / rac1_mod_abi (sdk/rac1_mod.h)"
                                                                         : "built for the mod ABI " + std::to_string(want) + ", this game has " + std::to_string(RAC1_MOD_ABI) +
                                                                               ": needs a newer game";
            std::fprintf(stderr, "[mods] %s: its code is not used: %s\n", m->folder.c_str(), why.c_str());
            mods_code_failed(m, "its code is not used: " + why);
            lib_close(lib);
            continue;
        }
        Plugin p;
        p.mod = m;
        p.lib = lib;
        p.cb.size = sizeof(rac1_mod);
        g_plugins.push_back(p);
        Plugin& q = g_plugins.back();
        q.classes.reserve(64);  // (class_define's; the level's classes point into it)
        int r = -1;
        with(q, [&] { r = main_fn(&g_api, &q.cb); });
        if (r != 0) {
            std::fprintf(stderr, "[mods] %s: its code did not start (rac1_mod_main returned %d)\n", m->folder.c_str(), r);
            mods_code_failed(m, "its code did not start (rac1_mod_main returned " + std::to_string(r) + ")");
            g_plugins.pop_back();
            continue;  // (its library stays: it may have registered something)
        }
        if (q.cb.size > sizeof(rac1_mod) || q.cb.size < offsetof(rac1_mod, level_start)) q.cb.size = sizeof(rac1_mod);
        // (a plugin built with an older header: the callbacks past its table's size are none)
        if (q.cb.size < sizeof(rac1_mod)) std::memset(reinterpret_cast<char*>(&q.cb) + q.cb.size, 0, sizeof(rac1_mod) - q.cb.size);
        std::printf("[mods] %s: its code loaded (%s, mod ABI %u)\n", m->folder.c_str(), m->plugin.generic_string().c_str(), want);
    }
    g_mod_code_any = !g_plugins.empty();
    if (g_mod_code_any) std::atexit(shutdown_all);
}

// ---- (ABI 2) the level's own content: its level.sdk read at its load, its objects spawned at its start ----
// level.sdk (the composer's, src/setup/level_compose.cpp; one item a line, the words separated by tabs):
//   sdk 1
//   class <name> <mesh> <size> <scale> <ghost 0|1> <colours> [r g b a ...]
//   object <class index> <x> <y> <z> <yaw> [<key=value words>]
//   sound <name> <bank index> <volume 0..1024> <near> <far> <loop 0|1>
//   trigger <name> <x0> <y0> <z0> <x1> <y1> <z1>
bool sdk_read(const std::string& path, Sdk& out) {
    std::ifstream f(path);
    if (!f) return false;
    std::string line;
    bool head = false;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string k;
        in >> k;
        if (k == "sdk") {
            int v = 0;
            in >> v;
            head = v == 1;
            if (!head) return false;
        } else if (k == "class") {
            SdkClass c;
            int ghost = 0, n = 0;
            in >> c.name >> c.mesh >> c.size >> c.scale >> ghost >> n;
            c.ghost = ghost != 0;
            for (int i = 0; i < n && i < 64; ++i) {
                int r = 0, g = 0, b = 0, a = 0;
                in >> r >> g >> b >> a;
                c.palette.push_back({static_cast<uint8_t>(r), static_cast<uint8_t>(g), static_cast<uint8_t>(b), static_cast<uint8_t>(a)});
            }
            if (!in || !(c.size > 0) || !(c.scale > 0)) return false;
            out.classes.push_back(std::move(c));
        } else if (k == "object") {
            SdkPlaced o;
            in >> o.cls >> o.pos[0] >> o.pos[1] >> o.pos[2] >> o.yaw;
            if (!in || o.cls < 0 || static_cast<size_t>(o.cls) >= out.classes.size()) return false;
            std::getline(in >> std::ws, o.params);
            out.placed.push_back(std::move(o));
        } else if (k == "sound") {
            SdkSound d;
            int loop = 0;
            in >> d.name >> d.index >> d.volume >> d.near_dist >> d.far_dist >> loop;
            if (!in) return false;
            d.loop = loop != 0;
            out.sounds.push_back(std::move(d));
        } else if (k == "update_all") {
            int v = 0;
            in >> v;
            out.update_all = v != 0;
        } else if (k == "trigger") {
            SdkTrigger t;
            in >> t.name >> t.lo[0] >> t.lo[1] >> t.lo[2] >> t.hi[0] >> t.hi[1] >> t.hi[2];
            if (!in) return false;
            for (int l = 0; l < 3; ++l)
                if (t.hi[l] < t.lo[l]) std::swap(t.lo[l], t.hi[l]);
            out.triggers.push_back(std::move(t));
        } else if (k == "path") {
            SdkPath pa;
            size_t n = 0;
            in >> pa.name >> n;
            for (size_t i = 0; i < n && in; ++i) {
                std::array<float, 3> v{};
                in >> v[0] >> v[1] >> v[2];
                pa.pts.push_back(v);
            }
            if (!in || pa.pts.empty()) return false;
            out.paths.push_back(std::move(pa));
        }
    }
    return head;
}

// the level `slot` loads: its content read (none on the game's levels)
void sdk_level_load(int slot) {
    g_sdk = Sdk{};
    g_plugin_paths.clear();
    g_mod_update_all = false;
    mrpw::host_meshes_dynamic_set({});
    if (slot < kFirstLevelSlot) return;
    const std::string dir = level_slot_build_dir(slot);
    if (dir.empty()) return;
    const std::string path = dir + "/level.sdk";
    std::error_code ec;
    if (!std::filesystem::exists(path, ec)) return;
    Sdk s;
    if (!sdk_read(path, s)) {
        std::fprintf(stderr, "[sdk] %s cannot be read (run the setup again)\n", path.c_str());
        return;
    }
    s.on = true;
    s.objs.reserve(MAX_OBJECTS);
    g_sdk = std::move(s);
    g_mod_update_all = g_sdk.update_all;
    if (g_mod_update_all) std::printf("[sdk] level slot %d: every moby updated wherever the camera is (update_all)\n", slot);
    // the classes' behaviour: the level's own mod's plugin first, then the others' in load order
    const ModInfo* lm = mods_level_at_slot(slot);
    for (SdkClass& c : g_sdk.classes)
        for (int pass = 0; pass < 2 && c.def == nullptr; ++pass)
            for (Plugin& p : g_plugins) {
                if ((pass == 0) != (p.mod == lm)) continue;
                for (const ClassDef& d : p.classes)
                    if (d.name == c.name) {
                        c.def = &d;
                        c.owner = &p;
                    }
                if (c.def != nullptr) break;
            }
    std::printf("[sdk] level slot %d: %zu classes, %zu objects, %zu sounds, %zu triggers%s\n", slot, g_sdk.classes.size(), g_sdk.placed.size(), g_sdk.sounds.size(),
                g_sdk.triggers.size(), g_sdk.paths.empty() ? "" : (", " + std::to_string(g_sdk.paths.size()) + " paths").c_str());
    for (const SdkClass& c : g_sdk.classes)
        std::printf("[sdk] class \"%s\": %s\n", c.name.c_str(), c.def != nullptr ? ("its behaviour: " + c.owner->mod->folder + "'s plugin").c_str() : "no behaviour (no plugin defines it)");
}

// the level's first frame: its sounds' bank and definitions, its placed objects
void sdk_level_start() {
    if (!g_sdk.on || g_sdk.spawned) return;
    g_sdk.spawned = true;
    if (!g_sdk.sounds.empty()) {
        const std::string path = level_slot_build_dir(g_level_slot) + "/sounds.bnk";
        std::ifstream f(path, std::ios::binary);
        std::vector<uint8_t> b((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
        g_sdk.bank = b.empty() ? 0 : snd989_host_bank_load(b);
        if (g_sdk.bank == 0) {
            std::fprintf(stderr, "[sdk] %s: no sound bank (run the setup again): the level's own sounds are silent\n", path.c_str());
        } else {
            // the game's sound definitions (sound_start's; 0x20 bytes): full volume within `near`, none past `far`, the loop
            // byte (a looping sound needs flag 4), the sound's index in the bank and the bank's handle
            for (size_t i = 0; i < g_sdk.sounds.size() && i < SFX_MAX; ++i) {
                const SdkSound& d = g_sdk.sounds[i];
                const uint32_t e = SFX_DEFS + static_cast<uint32_t>(i) * 0x20;
                swc1(e + 0x00, d.near_dist);
                swc1(e + 0x04, d.far_dist);
                sw(e + 0x08, 0);
                sw(e + 0x0c, static_cast<uint32_t>(d.volume));
                sw(e + 0x10, 0);
                sw(e + 0x14, 0);
                sb(e + 0x18, d.loop ? 1 : 0);
                sb(e + 0x19, 2);  // (blocked by walls, as the game's world sounds)
                sh(e + 0x1a, static_cast<uint32_t>(d.index));
                sw(e + 0x1c, g_sdk.bank);
            }
            if (g_sdk.sounds.size() > SFX_MAX) std::fprintf(stderr, "[sdk] more than %u sounds: the rest are silent\n", SFX_MAX);
            std::printf("[sdk] the level's sounds: bank %#x, %zu sounds\n", g_sdk.bank, std::min<size_t>(g_sdk.sounds.size(), SFX_MAX));
        }
    }
    uint32_t n = 0;
    for (size_t i = 0; i < g_sdk.placed.size(); ++i) {
        const SdkPlaced& o = g_sdk.placed[i];
        n += obj_spawn(o.cls, o.pos, o.yaw, static_cast<int32_t>(i)) != 0 ? 1 : 0;
    }
    if (!g_sdk.placed.empty()) std::printf("[sdk] %u of %zu placed objects spawned\n", n, g_sdk.placed.size());
}

// the level is left: its objects' remove callbacks, its bank unloaded
void sdk_level_end() {
    if (!g_sdk.on) return;
    for (size_t i = 0; i < g_sdk.objs.size(); ++i) obj_remove(static_cast<int>(i), false);  // (the level's mobys go with it)
    if (g_sdk.bank != 0) snd989_host_bank_unload(g_sdk.bank);
    g_sdk.bank = 0;
}

// a frame: Ratchet against the triggers
void sdk_triggers() {
    const uint32_t h = moby_base();
    if (h == 0) return;
    float p[3];
    for (uint32_t k = 0; k < 3; ++k) p[k] = lwc1(h + 0x10 + 4 * k);
    for (size_t i = 0; i < g_sdk.triggers.size(); ++i) {
        SdkTrigger& t = g_sdk.triggers[i];
        bool in = true;
        for (int l = 0; l < 3; ++l) in = in && p[l] >= t.lo[l] && p[l] <= t.hi[l];
        if (in == t.inside) continue;
        t.inside = in;
        for (Plugin& q : g_plugins)
            if (q.cb.trigger != nullptr) with(q, [&] { q.cb.trigger(q.cb.user, static_cast<int32_t>(i), t.name.c_str(), in ? 1 : 0); });
    }
}

// the frame's objects for the modern renderer: each shown one's model at its moby's matrix
void sdk_publish() {
    std::vector<mrpw::HostMeshInstance> v;
    for (const Obj& o : g_sdk.objs) {
        if (!o.live || !o.shown || static_cast<int8_t>(lbu(o.moby + 0x20)) < 0) continue;
        const SdkClass& c = g_sdk.classes[static_cast<size_t>(o.cls)];
        if (c.mesh < 0) continue;
        mrpw::HostMeshInstance in;
        in.cls = static_cast<uint32_t>(c.mesh);
        for (uint32_t r = 0; r < 3; ++r)
            for (uint32_t l = 0; l < 3; ++l) in.rot[r][l] = lwc1(o.moby + 0xc0 + r * 0x10 + 4 * l);
        for (uint32_t l = 0; l < 3; ++l) in.pos[l] = lwc1(o.moby + 0x10 + 4 * l);
        in.scale = c.scale * o.scale;
        in.palette = c.palette;
        v.push_back(std::move(in));
    }
    mrpw::host_meshes_dynamic_set(std::move(v));
}

// the level the plugins see now
void level_started() {
    g_started = true;
    const int slot = level_slot();
    g_level_slot = slot;
    const ModInfo* lm = slot >= kFirstLevelSlot ? mods_level_at_slot(slot) : nullptr;
    g_mod_code_objects = g_sdk.on;
    sdk_level_start();
    for (Plugin& p : g_plugins) {
        rac1_level_info info{};
        info.level = lw(LEVEL);
        info.slot = slot;
        info.level_id = lm != nullptr ? lm->level_id.c_str() : "";
        info.mine = lm != nullptr && lm == p.mod ? 1 : 0;
        info.objects = info.mine != 0 ? lm->level_objects : 0;
        if (info.mine != 0 && p.cb.object_update != nullptr && info.objects > 0) g_mod_code_objects = true;
        p.rng = 0x9e3779b97f4a7c15ull ^ (static_cast<uint64_t>(slot) + 1) * 0xbf58476d1ce4e5b9ull;
        if (p.cb.level_start != nullptr) with(p, [&] { p.cb.level_start(p.cb.user, &info); });
    }
    if (!g_session_bytes.empty()) session_bytes_give();  // (a save's session: the plugins' bytes after their start)
}

}  // namespace

// orig: (none) — enhancement (mod_code.h): weapon_allow's list
bool mod_code_weapon_allowed_slow(uint32_t item) { return item >= 0x25 || !weapon_item(item) || g_allow[item] != 0; }

// orig: (none) — enhancement (mod_code.h)
void mod_code_start() {
    if (!g_tried) load_all();
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_level_load() {
    if (!g_tried) load_all();
    if (!g_mod_code_any && level_slot() < kFirstLevelSlot) return;  // (the original's levels, no plugin: nothing)
    if (g_started && g_level_slot >= 0) {
        sdk_level_end();
        for (Plugin& p : g_plugins)
            if (p.cb.level_end != nullptr) with(p, [&] { p.cb.level_end(p.cb.user); });
    }
    g_started = false;
    g_mod_code_objects = false;
    profiles_level();
    if (lwu(MOD_LIGHTS) != 0) sw(MOD_LIGHTS, 0);
    g_hold = false;  // (input_hold: the pad the game's again on every level's start)
    g_mod_weapons_limited = false;  // (weapon_allow: all again)
    g_hold_sticky = 0;
    sdk_level_load(level_slot());
    // (the hooks' gate: a plugin, or the level's own content)
    g_mod_code_any = !g_plugins.empty() || g_sdk.on;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_level_loaded() {
    if (level_slot() < kFirstLevelSlot) {
        g_pool.clear();
        profiles_pool_take(g_pool, lw(LEVEL));  // (none)
        return;
    }
    pool_take(level_slot());
    if (g_session_rt) session_rt_loaded();  // (a session's arrival: its soft resets' snapshot, session_runtime.h)
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_frame() {
    if (!g_mod_code_any || ee_journal_active()) return;
    if (!g_started) level_started();
    // the world held (Advanced > pause in the quick select: no hero, mobys, particles nor camera this frame): the
    // triggers, the hunters' and Kalebo III's enemies' logic and the plugins' frame callbacks wait too (their timers,
    // their objects moved by hand); their lights stay as they were, the HUD callback still draws
    const bool held = advanced_world_paused();
    if (g_sdk.on && !held) sdk_triggers();
    if (profiles_any() && !held) profiles_frame(g_pool);  // (base_hunt's hunters, Kalebo III's enemies: class_profiles.h)
    if (!held) profiles_session_frame();                   // (a session's imported spawns with a profile)
    if (g_mod_weapons_limited) weapons_frame();
    if (!held && lwu(MOD_LIGHTS) != 0) sw(MOD_LIGHTS, 0);  // (light_add: the frame's own, from its frame callbacks)
    for (Plugin& p : g_plugins)
        if (p.cb.frame != nullptr && !held) with(p, [&] { p.cb.frame(p.cb.user); });
    static const char* test = std::getenv("RAC1_MOD_TEST");
    const uint64_t frame = ++g_frames;
    if (test != nullptr)
        for (Plugin& p : g_plugins)
            if (p.cb.test != nullptr) with(p, [&] { p.cb.test(p.cb.user, test, frame); });
    if (g_sdk.on) sdk_publish();
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_pad_before(uint32_t pad) {
    if (pad != PAD || !g_pad_masked) return;
    // (the edges of the next read from the real buttons: the previous held sets as the pad gave them)
    sw(PAD + 0x348, g_pad_real_348);
    sw(PAD + 0x1b0, g_pad_real_1b0);
    g_pad_masked = false;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_pad_after(uint32_t pad) {
    if (pad != PAD) return;
    g_pad_seen = true;
    g_pad_held = lwu(PAD + 0x1b0);
    g_pad_pressed = lwu(PAD + 0x1c4);
    g_pad_real_1a0 = lwu(PAD + 0x1a0);
    g_pad_real_1b0 = g_pad_held;
    g_pad_real_348 = lwu(PAD + 0x348);
    g_hold_sticky &= g_pad_real_1a0 | g_pad_real_1b0;
    if ((!g_hold && g_hold_sticky == 0) || lw(OV(0x0015f5c4)) != 0) return;  // (the menus, the vendor: theirs)
    const uint32_t keep = g_hold ? HOLD_KEEP : ~g_hold_sticky;
    for (const uint32_t f : {0x1a0u, 0x1a4u, 0x1a8u, 0x1b0u, 0x1b4u, 0x1b8u, 0x1c0u, 0x1c4u, 0x1c8u, 0x348u}) sw(PAD + f, lwu(PAD + f) & keep);
    // (the presses' history the hero's moves read, pad_button_history: the entry this read wrote, before +0x18e moved on)
    const uint32_t last = PAD + 0x1e0 + static_cast<uint32_t>((lh(PAD + 0x18e) + 29) % 30) * 4;
    sw(last, lwu(last) & keep);
    if (g_hold) {
        for (uint32_t o = 0x100; o < 0x180; o += 4) sw(PAD + o, 0);  // (the sticks, the pressures and their copies)
        sw(PAD + 0x1d0, (lwu(PAD + 0x1a0) == 0) ? 1u : 0u);
        sw(PAD + 0x1d4, 1);
        sw(PAD + 0x1d8, 0);
    }
    g_pad_masked = true;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_camera() {
    if (!g_mod_code_any || ee_journal_active() || !g_started) return;
    for (Plugin& p : g_plugins)
        if (p.cb.camera != nullptr) with(p, [&] { p.cb.camera(p.cb.user); });
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_hud() {
    if (!g_mod_code_any || !g_started) return;
    if (lw(OV(0x0015f5c4)) == 5) return;  // (the vendor's screens, vendor_open: theirs alone)
    g_in_hud = true;
    for (Plugin& p : g_plugins)
        if (p.cb.hud != nullptr) with(p, [&] { p.cb.hud(p.cb.user); });
    g_in_hud = false;
}

// orig: (none) — enhancement (mod_code.h)
bool mod_code_object_update(uint32_t m) {
    if (const int i = obj_of_moby(m); i >= 0) {  // (ABI 2) a level object: its hit, its class's update (never in a replay)
        if (ee_journal_active()) return true;
        const uint32_t h = obj_handle(static_cast<size_t>(i));
        const SdkClass& c = g_sdk.classes[static_cast<size_t>(g_sdk.objs[static_cast<size_t>(i)].cls)];
        if (c.def == nullptr) return true;
        rac1_hit hit{};
        if (c.def->d.hit != nullptr && take_hit(m, RAC1_HIT_PLAYER, hit))
            obj_call(static_cast<size_t>(i), [&](const rac1_class_def& d) { d.hit(d.user, h, &hit); });
        if (c.def->d.update != nullptr && obj_index(h) >= 0) obj_call(static_cast<size_t>(i), [&](const rac1_class_def& d) { d.update(d.user, h); });
        return true;
    }
    if (ee_journal_active() || g_level_slot < kFirstLevelSlot) return false;
    const int32_t uid = lh(m + 0xb2);
    const int32_t hero_uid = lh(moby_base() + 0xb2);
    const ModInfo* lm = mods_level_at_slot(g_level_slot);
    if (lm == nullptr || uid < 1 || uid == hero_uid || uid > lm->level_objects + (hero_uid >= 1 && hero_uid <= lm->level_objects ? 1 : 0)) return false;
    for (Plugin& p : g_plugins) {
        if (p.mod != lm || p.cb.object_update == nullptr) continue;
        int r = 0;
        with(p, [&] { r = p.cb.object_update(p.cb.user, m, uid, lh(m + 0xa6)); });
        return r != 0;
    }
    return false;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_new_game() {
    g_carried.clear();
    if (!g_tried) load_all();
    for (Plugin& p : g_plugins)
        if (p.cb.new_game != nullptr) with(p, [&] { p.cb.new_game(p.cb.user); });
}

// orig: (none) — enhancement (mod_code.h)
uint32_t mod_code_pc_items(std::vector<uint8_t>& d) {
    uint32_t count = 0;
    auto put_item = [&](const std::vector<uint8_t>& item) {
        if (item.size() > 0xffff) return;
        d.push_back(static_cast<uint8_t>(PC_ITEM));
        d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
        d.push_back(static_cast<uint8_t>(item.size()));
        d.push_back(static_cast<uint8_t>(item.size() >> 8));
        d.insert(d.end(), item.begin(), item.end());
        ++count;
    };
    for (const std::vector<uint8_t>& c : g_carried) put_item(c);
    for (Plugin& p : g_plugins) {
        if (p.cb.save == nullptr) continue;
        const std::string& id = p.mod->folder;
        std::vector<uint8_t> buf(0xff00 - id.size());
        uint32_t n = 0;
        with(p, [&] { n = p.cb.save(p.cb.user, buf.data(), static_cast<uint32_t>(buf.size())); });
        if (n == 0) continue;
        if (n > buf.size()) n = static_cast<uint32_t>(buf.size());
        std::vector<uint8_t> item;
        auto u32 = [&item](uint32_t v) {
            for (int i = 0; i < 4; ++i) item.push_back(static_cast<uint8_t>(v >> (8 * i)));
        };
        u32(PC_VERSION);
        u32(static_cast<uint32_t>(id.size()));
        item.insert(item.end(), id.begin(), id.end());
        item.insert(item.end(), buf.begin(), buf.begin() + n);
        put_item(item);
    }
    return count;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_pc_reset() {
    if (!g_tried) load_all();  // (a save loaded from the title: before any level, the plugins must be there for their bytes)
    g_carried.clear();
    for (Plugin& p : g_plugins) p.loaded_item = false;
}

// orig: (none) — enhancement (mod_code.h)
bool mod_code_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (id != PC_ITEM) return false;
    auto u32 = [p, size](uint32_t at) { return at + 4 <= size ? static_cast<uint32_t>(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | static_cast<uint32_t>(p[at + 3]) << 24) : 0u; };
    const uint32_t nlen = u32(4);
    if (u32(0) == PC_VERSION && 8 + nlen <= size) {
        const std::string folder(reinterpret_cast<const char*>(p + 8), nlen);
        for (Plugin& q : g_plugins) {
            if (q.mod->folder != folder) continue;
            q.loaded_item = true;
            if (q.cb.load != nullptr) with(q, [&] { q.cb.load(q.cb.user, p + 8 + nlen, size - 8 - nlen); });
            return true;
        }
    }
    g_carried.emplace_back(p, p + size);  // (a plugin not loaded, or a newer version's item: written back as it was)
    return true;
}

// orig: (none) — enhancement (mod_code.h)
bool mod_code_path_points(const std::string& name, std::vector<float>& xyz) {
    xyz.clear();
    for (const SdkPath& p : g_sdk.paths)
        if (p.name == name) {
            for (const auto& v : p.pts) xyz.insert(xyz.end(), v.begin(), v.end());
            return true;
        }
    return false;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_respawn(uint32_t count, const char* checkpoint, const float pos[3], float yaw) {
    rac1_respawn r{};
    r.size = sizeof r;
    r.count = count;
    r.checkpoint = checkpoint;
    std::memcpy(r.pos, pos, sizeof r.pos);
    r.yaw = yaw;
    for (Plugin& p : g_plugins)
        if (p.cb.size >= offsetof(rac1_mod, respawn) + sizeof(p.cb.respawn) && p.cb.respawn != nullptr) with(p, [&] { p.cb.respawn(p.cb.user, &r); });
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_session_save(std::vector<std::pair<std::string, std::vector<uint8_t>>>& out) {
    for (Plugin& p : g_plugins) {
        if (p.cb.size < offsetof(rac1_mod, session_save) + sizeof(p.cb.session_save) || p.cb.session_save == nullptr) continue;
        std::vector<uint8_t> buf(0x4000);
        uint32_t n = 0;
        with(p, [&] { n = p.cb.session_save(p.cb.user, buf.data(), static_cast<uint32_t>(buf.size())); });
        if (n == 0) continue;
        buf.resize(std::min<size_t>(n, buf.size()));
        out.emplace_back(p.mod->folder, std::move(buf));
    }
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_session_load(const std::string& folder, const uint8_t* data, uint32_t size) {
    g_session_bytes.emplace_back(folder, std::vector<uint8_t>(data, data + size));
    if (g_started) session_bytes_give();
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_save_loaded() {
    for (Plugin& p : g_plugins)
        if (!p.loaded_item && p.cb.load != nullptr) with(p, [&] { p.cb.load(p.cb.user, nullptr, 0); });
}

}  // namespace rac1
