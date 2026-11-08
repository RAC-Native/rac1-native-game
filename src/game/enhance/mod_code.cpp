// See mod_code.h and sdk/rac1_mod.h. Not a port: the host's plugin loader and the services of its C interface, written on
// top of the game's own functions.
#include <host/mod_code.h>

#include <host/level_slots.h>
#include <game/ovl/arrival.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/veldin_mobys.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/mods.h>

#include <algorithm>
#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
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

namespace {

// The mods' block of EE memory (the PS2 kernel's first megabyte, unused by the port's kernel: 0xe0000.. the level mods' texts,
// level_slots.cpp): 0xe1000..0xe2000 ours (the HUD's strings, the collision's vectors), 0xe2000..0xf0000 the plugins'
// (ee_alloc). The ADV block starts at 0xf0000 (advanced.cpp).
constexpr uint32_t SCRATCH = 0x000e1000u, STRINGS = 0x000e1100u, STRING_SIZE = 0x100, STRING_COUNT = 15;
constexpr uint32_t HEAP = 0x000e2000u, HEAP_END = 0x000f0000u;
constexpr uint32_t PC_ITEM = 0x300;  // our item in save<N>.pc: a plugin's bytes (level mods' items are 0x200)
constexpr uint32_t PC_VERSION = 1;
constexpr uint32_t RAM_SIZE = 0x02000000u;
constexpr uint32_t LEVEL = 0x0015ed84u;

using MainFn = int (*)(const rac1_api*, rac1_mod*);
using AbiFn = unsigned (*)();

struct Plugin {
    const ModInfo* mod = nullptr;
    void* lib = nullptr;
    rac1_mod cb{};
    uint64_t rng = 0;
    bool loaded_item = false;  // (a save's .pc read) it had an item of this plugin
};

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

bool addr_ok(uint32_t a, uint32_t n) { return a != 0 && a < RAM_SIZE && n <= RAM_SIZE - a; }

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
    if (is_moby(m) && lwu(m + 0x24) != 0) moby_update_transform(m);
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
    swc1(SCRATCH + 0x38, yaw);
    swc1(SCRATCH + 0x3c, 0.0f);
    hero_place(SCRATCH + 0x20, SCRATCH + 0x30, -1, 1);
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

void api_hud_text(int32_t x, int32_t y, uint32_t abgr, const char* text) {
    if (!g_in_hud || text == nullptr) return;
    const uint32_t s = STRINGS + (g_hud_string++ % STRING_COUNT) * STRING_SIZE;
    uint32_t i = 0;
    for (; text[i] != 0 && i + 1 < STRING_SIZE; ++i) {
        const char c = text[i];
        sb(s + i, c >= 0x20 && c < 0x7f ? static_cast<uint32_t>(static_cast<uint8_t>(c)) : static_cast<uint32_t>('?'));
    }
    sb(s + i, 0);
    text_draw(x, y, static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(abgr))), s, -1, effect_tex0(1), OV(0x001c3150));
}

void api_camera_position(float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = lwc1(OV(0x00166dc0) + 4 * static_cast<uint32_t>(i));
}

uint32_t api_pad_held() { return lwu(0x0013c940 + 0x1b0); }
uint32_t api_pad_pressed() { return lwu(0x0013c940 + 0x1c4); }

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

// the level the plugins see now
void level_started() {
    g_started = true;
    const int slot = level_slot();
    g_level_slot = slot;
    const ModInfo* lm = slot >= kFirstLevelSlot ? mods_level_at_slot(slot) : nullptr;
    g_mod_code_objects = false;
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
}

}  // namespace

// orig: (none) — enhancement (mod_code.h)
void mod_code_start() {
    if (!g_tried) load_all();
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_level_load() {
    if (!g_tried) load_all();
    if (!g_mod_code_any) return;
    if (g_started && g_level_slot >= 0)
        for (Plugin& p : g_plugins)
            if (p.cb.level_end != nullptr) with(p, [&] { p.cb.level_end(p.cb.user); });
    g_started = false;
    g_mod_code_objects = false;
}

// orig: (none) — enhancement (mod_code.h)
void mod_code_frame() {
    if (!g_mod_code_any || ee_journal_active()) return;
    if (!g_started) level_started();
    for (Plugin& p : g_plugins)
        if (p.cb.frame != nullptr) with(p, [&] { p.cb.frame(p.cb.user); });
    static const char* test = std::getenv("RAC1_MOD_TEST");
    static uint64_t frame = 0;
    ++frame;
    if (test != nullptr)
        for (Plugin& p : g_plugins)
            if (p.cb.test != nullptr) with(p, [&] { p.cb.test(p.cb.user, test, frame); });
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
    g_in_hud = true;
    for (Plugin& p : g_plugins)
        if (p.cb.hud != nullptr) with(p, [&] { p.cb.hud(p.cb.user); });
    g_in_hud = false;
}

// orig: (none) — enhancement (mod_code.h)
bool mod_code_object_update(uint32_t m) {
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
void mod_code_save_loaded() {
    for (Plugin& p : g_plugins)
        if (!p.loaded_item && p.cb.load != nullptr) with(p, [&] { p.cb.load(p.cb.user, nullptr, 0); });
}

}  // namespace rac1
