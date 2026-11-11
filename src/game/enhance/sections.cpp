// See sections.h.
#include <game/enhance/sections.h>

#include <formats/host_meshes.h>
#include <formats/scene_wad.h>
#include <game/enhance/context.h>
#include <game/enhance/hero_modes.h>
#include <game/enhance/leveldata.h>
#include <game/enhance/section_world.h>
#include <game/enhance/session.h>
#include <game/enhance/session_runtime.h>
#include <game/enhance/setpieces.h>
#include <game/math/vecmath.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/pause_menu.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_world_renderer.h>
#include <host/level_slots.h>
#include <host/mod_code.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <future>
#include <memory>
#include <map>
#include <set>
#include <span>
#include <sstream>

namespace rac1 {

bool g_sections_on = false;
bool g_section_equiv = [] {
    const char* e = std::getenv("RAC1_SECTION_EQUIV");
    return e != nullptr && std::atoi(e) != 0;
}();

namespace {

constexpr uint32_t P = 0x0013f350u;         // the player block
constexpr uint32_t LEVEL = 0x0015ed84u;     // the game's level number (the base's)
constexpr uint32_t GRID_OUT = 0x80807f7fu;  // a moby's grid rectangle outside the grid
constexpr OvConst<0x0016ca40u> S{};         // the level's settings (the background's words at +0x23c)

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

bool trace() {
    static const bool t = std::getenv("RAC1_SECTION_TRACE") != nullptr && std::atoi(std::getenv("RAC1_SECTION_TRACE")) != 0;
    return t;
}

// the commit's budget a frame (RAC1_SECTION_BUDGET_MS, default 2)
double budget_ms() {
    static const double b = [] {
        const char* e = std::getenv("RAC1_SECTION_BUDGET_MS");
        const double v = e != nullptr ? std::atof(e) : 2.0;
        return v > 0.0 ? v : 2.0;
    }();
    return b;
}

struct Entry {
    std::string name;
    float pos[3] = {0, 0, 0};
    float yaw = 0;
};
struct Portal {
    std::string name, entry;
    float lo[3] = {}, hi[3] = {};
    int to = 0;
    int fade = 0;
    bool inside = false;
};
struct ImportPlace {
    int level = -1;
    uint32_t cls = 0;
    float pos[3] = {};
    float yaw = 0;
    std::vector<std::pair<std::string, std::string>> binds;
};
struct Bind {
    int32_t uid = 0;
    uint32_t cls = 0;
    std::string slot, value;
};
// an object of the section in the moby array: a placement of its gameplay file (uid), or a class of another level (imp: its
// line's index)
struct Obj {
    uint32_t moby = 0;
    int32_t uid = -1;   // (+0xb2 as spawned)
    uint32_t cls = 0;
    int imp = -1;
    uint32_t f34 = 0;   // (frozen: its +0x34 flags before)
    int32_t src = -1;   // (a world section's: its placement's index in its level's file)
    bool boss = false;  // (stage 10: one of its section's bosses: a `boss` line's class, or the plugin's boss_set)
    float at[4] = {};   // (a boss the plugin spawned: where it was when set, to spawn it again after a death)
    uint32_t cptr = 0;  // (its class's block as placed, +0x24: its class id +0xa6 may change, its own code's doing - Umbris's 529 becomes 533)
};
// a moby spawned by one of a section's objects (or by one of those), while it was in play: taken out with them when the section is
// left (frozen with them), and with a boss placed again after a death
struct Kid {
    uint32_t root = 0;  // (the section's object it comes from)
    uint32_t cptr = 0;  // (+0x24 and +0xb2 as spawned: its slot not reused since)
    int16_t b2 = 0;
    uint32_t f34 = 0;   // (frozen: its +0x34 flags before)
};
// the files of a section as the worker read them
struct Bytes {
    std::vector<uint8_t> coll, sky, gp;
    std::vector<uint8_t> own_gp;  // (stage 10: a world's section's own gameplay file: the mod's rails in it, added to its level's)
    double ms = 0;
    std::string err;
    std::unique_ptr<WorldBytes> world;  // (stage 8: a game level's world, its files from the disc)
};
enum class St { Absent, Loading, Committing, Ready };

struct Section {
    std::string name;
    int32_t policy = SECTION_REMOVE;
    int32_t respawn = -1;  // (-1: the session's)
    bool f_coll = false, f_sky = false, f_gp = false;
    int sky_level = -1, music = -2;
    bool has_fog = false, has_bg = false, has_death = false;
    int fog[3] = {}, bg[3] = {};
    float death = 0;
    std::string lights;
    size_t mesh_first = 0, mesh_count = 0;
    std::vector<Entry> entries;
    std::vector<Portal> portals;
    std::vector<ImportPlace> imports;
    std::vector<Bind> binds;
    // the streaming
    St st = St::Absent;
    std::future<Bytes> fut;
    Bytes b;
    uint64_t req_frame = 0;
    double req_ms = 0;
    uint32_t coll = 0, coll_tree = 0, coll_list = 0, sky = 0, gp = 0;
    uint32_t rails = 0;      // (stage 7) its rail table (the game's 0x20-byte records, in its heap block), 0 none
    int32_t rail_count = 0;
    int32_t own_rail_first = 0;  // (stage 10: a world's section: where the mod's own rails start in its table, after its level's)
    int step = 0;
    size_t copied = 0;
    int commit_frames = 0;
    double commit_ms = 0, commit_max = 0;
    bool music_known = false;
    uint32_t music_words[15] = {};
    // its objects
    bool spawned = false, frozen = false;
    std::vector<Obj> objs;
    std::map<uint32_t, Kid> kids;  // (the mobys its objects spawned, by slot)
    std::set<int32_t> dead;   // (uids killed since it was entered: not placed again until a death there)
    std::set<int> dead_imp;   // (the same for its classes of other levels, by line)
    uint32_t shared = 0;      // (its gameplay's shared pvar data, made at the first placement after an entry)
    // (stage 8) a game level's world as its scenery (`world <level>`): its level, its objects (their uids there -> the session's),
    // its build in the session heap, its fog words, the groups its objects make in its level's registry
    int world = -1;
    bool zones_all = false;  // (`zones all`: every zone of its world drawn, the zone grid's choice by the camera's place not asked)
    std::map<int32_t, int32_t> wuids;
    std::unique_ptr<WorldBuild> wb;
    WorldHud hud;  // (stage 10) its level's HUD pictures the session lacks (a boss's health bar)
    bool has_fogw = false;
    uint32_t fogw[4] = {};
    std::vector<int32_t> groups;
    // (stage 9) its hero line: the level whose hero code runs Ratchet in it (-1 the session base's), the mode he is in there
    // (clank, giantclank, board; "" none) and the object of it spawned for him (0 none)
    int hero_level = -1;
    bool has_hero = false;  // (stage 10: its own hero line; a section without one runs main's hero code)
    std::string hero_mode;
    uint32_t hero_obj = 0;
    // (stage 10) its bosses: the `boss` lines' classes (level, class), the uids of its placed bosses beaten (never placed again in
    // this session's visit), how many were beaten, the bosses the plugin spawned and named (boss_set)
    std::vector<std::pair<int, uint32_t>> boss_cls;
    std::set<int32_t> beaten;
    std::set<int> beaten_imp;
    int beaten_n = 0;
    std::vector<Obj> pboss;
    // (ABI 17) `fresh`: every entry puts its objects back as at the first (reset_section); the uids of its own placements (a section
    // of the mod's own: their killed-for-good bits in the session's level state, taken when its gameplay file is in)
    bool fresh = false;
    bool entered = false;  // (entered since it was last put back: what its objects wrote in the level's state is there)
    std::vector<int32_t> puids;
};

// main's state as the game set it at the arrival
struct Main {
    WorldState world;    // (stage 8) the session's own world state, lights and fog words (the arrival's)
    WorldLights lights;
    uint32_t fogw[4] = {};
    uint32_t coll_tree = 0, coll_list = 0, sky = 0;
    uint32_t bg[3] = {};
    uint8_t fog[3] = {};
    uint32_t death = 0;
    uint32_t music[15] = {};
    uint32_t rails = 0, rail_count = 0;  // (stage 7) its rail table (the level's load's)
};

struct Pending {
    bool on = false;
    int to = 0;
    Entry e;
    bool has_entry = false;
    int fade = 0;
    int phase = 0;  // 0 waiting for the section, 1 fading out, 2 fading in
    int k = 0;
    std::string why;
};

std::vector<Section> g_secs;
std::string g_build, g_mod_dir;
int g_cur = -1;
int g_start = 0;
std::string g_start_entry;
Main g_main;
Pending g_pend;
int g_fade_alpha = 0;
uint64_t g_frame = 0;
uint32_t g_cur_music[15] = {};
bool g_worlds = false;  // (stage 8: a section of the session is a game level's world)
bool g_mode_pending = false;  // (stage 9: the current section's hero mode to enter at the next frame of play)

std::string owner_files(const Section& s) { return "section " + s.name; }
std::string owner_objects(const Section& s) { return "section " + s.name + " objects"; }

// an object still in play in its slot: its uid there, its class id or (a class id its code changed) its class's block
bool alive(const Obj& o) {
    return static_cast<int8_t>(lbu(o.moby + 0x20)) >= 0 && lh(o.moby + 0xb2) == static_cast<int16_t>(o.uid) &&
           (lhu(o.moby + 0xa6) == o.cls || (o.cptr != 0 && lwu(o.moby + 0x24) == o.cptr));
}

bool kid_alive(uint32_t m, const Kid& k) {
    return static_cast<int8_t>(lbu(m + 0x20)) >= 0 && lwu(m + 0x24) == k.cptr && lh(m + 0xb2) == k.b2;
}

// the mobys section `s`'s objects spawned taken out of play (`root` 0: all; else those of that object), logged by class: their count
size_t kids_retire(Section& s, uint32_t root) {
    std::map<uint32_t, size_t> by;
    size_t n = 0;
    for (auto it = s.kids.begin(); it != s.kids.end();) {
        if (root != 0 && it->second.root != root) {
            ++it;
            continue;
        }
        if (kid_alive(it->first, it->second)) {
            ++by[lhu(it->first + 0xa6)];
            moby_retire(it->first);
            ++n;
        }
        it = s.kids.erase(it);
    }
    if (n != 0) {
        std::string l;
        for (const auto& [c, k] : by) l += (l.empty() ? "" : ", ") + std::to_string(c) + " x" + std::to_string(k);
        std::printf("[section] %s: %zu mobys its %s spawned taken out with %s (%s)\n", s.name.c_str(), n, root != 0 ? "boss" : "objects",
                    root != 0 ? "it: the fight from its start" : "them", l.c_str());
    }
    return n;
}

// ---- section_spawn_placements: gameplay_load's moby part (level_load.cpp) for one section's gameplay file ----
struct SpawnOpts {
    bool test = false;                        // the equivalence test: every placement in slots from the array's start, the game's rule
    const std::set<int32_t>* only = nullptr;  // (the session's) these uids only
    const std::set<int32_t>* skip = nullptr;  // (the session's) not these uids
    std::string owner;                        // the heap blocks' owner (pvars, shared data)
    uint32_t* shared = nullptr;               // the shared pvar data's block (made when 0, else reused)
    // (stage 8, a game level's world) its placements' uids on their level -> the session's (the others not placed), the level whose
    // classes they are (the session's ids for them: an import's, else the base's own), the world's zone masks
    const std::map<int32_t, int32_t>* uids = nullptr;
    int cls_level = -1;
    const WorldBuild* world = nullptr;
};
struct Spawned {
    uint32_t moby = 0;
    int32_t uid = 0;
    uint32_t cls = 0;
    int32_t pvar = -1;  // (its pvar table index)
    int32_t src = -1;   // (a world's: its placement's index in its level's file, as the level's groups and pvar links name it)
};
struct SpawnResult {
    std::vector<Spawned> mobys;
    std::vector<int16_t> map;  // (each placement's slot index, -1 not placed)
    std::map<int32_t, uint32_t> moved;  // (pvar table index -> its block)
    uint32_t shared = 0;
    size_t refused = 0;  // (the spawn rule's: killed for good)
    size_t full = 0;     // (no room in the moby array)
    size_t absent = 0;   // (a world's: its class not in the session)
};

// orig: (host) DESIGN 7.4: the placements of the gameplay file at `H` (decompressed in EE memory) placed in the running session as
// gameplay_load places a level's: each passing its spawn rule into a free slot of the moby array (moby_spawn's; the test's: in
// order from the array's start, as the native load), moby_init, its record's fields, its pvar block (into the session heap,
// packed in placement order as pvar_move packs them), the fix-ups of its pvars (the words naming a placement -> its slot, the
// offsets into its own block, the pointers into the shared pvar data). The session's rule differs in one point: an enemy killed
// "for the visit" (flags 8) is placed again (DECISIONS 4). The groups (+0x48) are not placed (a section of the mod's own has
// none: the hook for stage 8's game worlds as sections)
SpawnResult section_spawn_placements(uint32_t H, const SpawnOpts& o) {
    SpawnResult r;
    const uint32_t mp = H + lwu(H + 0x44);
    const int32_t n1 = lw(mp);
    const uint32_t base = lwu(OV(0x0015ffd8));
    r.map.assign(n1 > 0 ? static_cast<size_t>(n1) : 0, -1);
    uint32_t s1 = mp + 0x10;
    uint32_t spawned = 0;
    for (int32_t k = 0; k < n1; ++k) {
        const uint32_t rec_end = s1 + static_cast<uint32_t>((lw(s1) >> 2) << 2);
        s1 += 4;
        const uint32_t wl = lwu(s1);
        s1 += 4;
        const uint32_t flags = lwu(s1);
        s1 += 4;
        int32_t uid = lw(s1);
        const int32_t src_uid = uid;
        if (o.uids != nullptr) {  // (a world's: its level's uid -> the session's; not one of the section's objects: not placed)
            const auto it = o.uids->find(uid);
            if (it == o.uids->end()) {
                s1 = rec_end;
                continue;
            }
            uid = it->second;
        }
        s1 += 4;
        const int32_t s6 = lw(s1);
        s1 += 4;
        const int32_t s7 = lw(s1);
        s1 += 4;
        int32_t s4 = s6, s5 = s6;
        uint32_t a2 = 0xfe;
        bool skip = false;
        const auto bit = [uid](uint32_t b) { return (lw(b + static_cast<uint32_t>(uid >> 5) * 4) >> (uid & 0x1f)) & 1; };
        const auto half_up = [](int32_t v) { return (v + static_cast<int32_t>(static_cast<uint32_t>(v) >> 31)) >> 1; };
        if (flags != 0) {
            a2 = (flags & 0x10) != 0 ? static_cast<uint32_t>(sub_00284d90(lwu(0x0015ed84), uid)) & 0xff : 0xff;
            const uint32_t lvbits = 0x0014c190 + (lwu(0x0015ed84) << 8);
            if (lbu(OV(0x001bb684) + static_cast<uint32_t>(uid)) != 0) {
                skip = true;
            } else if ((flags & 3) != 0) {
                if (lbu(OV(0x0015fc88) + wl) == 0xff) {
                    if ((flags & 2) == 0) {
                        skip = true;
                    } else {
                        s4 = s7;
                        if (bit(OV(0x001ba4d0)) != 0) {
                            s5 = half_up(s4 + 1);
                        } else {
                            s5 = s4;
                        }
                    }
                } else if ((flags & 1) == 0) {
                    skip = true;
                } else if (bit(lvbits) != 0) {
                    s5 = half_up(s6 + 1);
                }
            } else if ((flags & 8) != 0) {
                skip = o.test && bit(OV(0x001ba4d0)) != 0;  // (the session's: back after a death, DECISIONS 4)
            } else if ((flags & 0xc) == 4) {
                skip = bit(lvbits) != 0;
            }
        }
        uint32_t cls = lwu(s1);
        if (o.cls_level >= 0 && cls != 0) {  // (a world's: the session's id of its level's class)
            int32_t id = session_import_id(o.cls_level, cls);
            if (id < 0) id = session_import_id(lw(0x0015ed84), cls);
            if (id < 0) {
                ++r.absent;
                s1 = rec_end;
                continue;
            }
            cls = static_cast<uint32_t>(id);
        }
        bool want = !skip;
        if (!o.test) {
            if (cls == 0) want = false;  // (the hero's record: a gameplay file's first)
            if (o.only != nullptr && o.only->count(uid) == 0) want = false;
            if (o.skip != nullptr && o.skip->count(uid) != 0) want = false;
            if (skip && cls != 0 && (o.only == nullptr || o.only->count(uid) != 0)) ++r.refused;
        }
        if (!want) {
            s1 = rec_end;
            continue;
        }
        uint32_t m = 0;
        if (o.test) {
            m = base + spawned * 0x100;
            moby_init(m, g_session_classes ? session_placed_class(cls) : cls);
        } else {
            m = moby_spawn(cls);  // (a free slot of the moby array, moby_init'ed)
            if (m == 0) {
                ++r.full;
                s1 = rec_end;
                continue;
            }
        }
        r.map[static_cast<size_t>(k)] = static_cast<int16_t>((m - base) >> 8);
        s1 += 4;
        sh(m + 0xb2, static_cast<uint32_t>(uid));
        sb(m + 0xb1, a2);
        sh(m + 0xb4, static_cast<uint32_t>(s5));
        sb(m + 0xb0, wl);
        sh(m + 0xb6, static_cast<uint32_t>(s4));
        if (lw(m + 0x24) != 0) swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * lwc1(s1));
        s1 += 4;
        sh(m + 0x32, lhu(s1));
        s1 += 4;
        sb(m + 0x30, lbu(s1));
        s1 += 0xc;
        for (uint32_t q = 0; q < 0xc; q += 4) sw(m + 0x10 + q, lwu(s1 + q));
        s1 += 0xc;
        for (uint32_t q = 0; q < 0xc; q += 4) sw(m + 0x40 + q, lwu(s1 + q));
        s1 += 0xc;
        sb(m + 0x21, lbu(s1));
        s1 += 4;
        const int32_t ground = lw(s1);
        s1 += 4;
        const float above = lwc1(s1);
        s1 += 4;
        if (ground != 0) {
            const float z = floor_probe(0.5f, m + 0x10, 0);
            if (0.0f < z) swc1(m + 0x18, z + above);
        }
        s1 += 4;
        sh(m + 0x36, 0x7f80);
        sw(m + 0x78, lwu(s1));
        const int32_t pvar = lw(s1);
        s1 += 4;
        if (lw(s1) == 0) sh(m + 0x36, 0);
        s1 += 4;
        if (lw(m + 0x24) != 0) sh(lwu(m + 0x24) + 0x44, lhu(lwu(m + 0x24) + 0x44) | lhu(s1));
        sh(m + 0x34, lhu(m + 0x34) | lhu(s1));
        s1 += 4;
        if (lw(m + 0x24) != 0) moby_update_transform(m);
        const uint32_t rr = lwu(s1), g = lwu(s1 + 4), b = lwu(s1 + 8), lt = lwu(s1 + 0xc);
        s1 += 0x10;
        const uint32_t col = (b << 16) + (g << 8) + rr;
        sw(m + 0x80, col);
        moby_set_color_packed(m, static_cast<int64_t>(static_cast<int32_t>(col)), 0, 0, 0);
        sw(m + 0x38, lt);
        if (lw(s1) != -1) level_slot_set(m, lw(s1));
        if (o.world != nullptr) world_moby_mask(*o.world, m, src_uid);  // (a world's: its zone mask, by its uid there)
        r.mobys.push_back({m, uid, cls, pvar, k});
        ++spawned;
        s1 = rec_end;
    }
    // the pvars: each placed moby's block moved out of the file's data (packed in placement order, as pvar_move does)
    if (lw(H + 0x58) != 0) {
        const uint32_t data = H + lwu(H + 0x58), tab = H + lwu(H + 0x54);
        uint32_t total = 0;
        for (const Spawned& sp : r.mobys)
            if (sp.pvar >= 0) total += lwu(tab + static_cast<uint32_t>(sp.pvar) * 8 + 4);
        const uint32_t blk = total != 0 ? session_heap_alloc(total + 0x10, o.owner) : 0;  // (+0x10: mem_copy128's last quadword)
        uint32_t at = blk;
        for (const Spawned& sp : r.mobys) {
            if (sp.pvar < 0) {
                sw(sp.moby + 0x78, 0);
                continue;
            }
            const uint32_t t = static_cast<uint32_t>(sp.pvar) * 8 + tab;
            const int32_t size = lw(t + 4);
            if (blk == 0) {  // (no heap: the moby without its pvars, out of play)
                sw(sp.moby + 0x78, 0);
                moby_retire(sp.moby);
                continue;
            }
            sw(sp.moby + 0x78, at);
            mem_copy128(at, data + lwu(t), size);
            r.moved[sp.pvar] = at;
            at += static_cast<uint32_t>(size);
        }
    }
    // the pvar words naming a placement -> its slot
    if (lw(H + 0x50) != 0) {
        uint32_t a = H + lwu(H + 0x50);
        for (int32_t i = lw(a); i >= 0; a += 8, i = lw(a)) {
            const auto it = r.moved.find(i);
            if (it == r.moved.end()) continue;
            const uint32_t w = it->second + lwu(a + 4);
            const int32_t v = lw(w);
            if (v >= 0) sw(w, static_cast<uint32_t>(static_cast<int32_t>(v < n1 ? r.map[static_cast<size_t>(v)] : -1)));
        }
    }
    // the pvar words holding an offset into their own block
    if (lw(H + 0x5c) != 0) {
        uint32_t a = H + lwu(H + 0x5c);
        for (int32_t i = lw(a); i >= 0; a += 8, i = lw(a)) {
            const auto it = r.moved.find(i);
            if (it == r.moved.end()) continue;
            const uint32_t w = it->second + lwu(a + 4);
            sw(w, lwu(w) + it->second);
        }
    }
    // the pointers into the level's shared pvar data (one block a section: made at the first placement, kept for the next ones)
    if (lw(H + 0x4c) != 0) {
        uint32_t p = H + lwu(H + 0x4c);
        const int32_t size = lw(p);
        const int32_t n = lw(p + 4);
        p += 0x10;
        uint32_t sb_ = o.shared != nullptr ? *o.shared : 0;
        if (sb_ == 0 && size > 0) {
            sb_ = session_heap_alloc(static_cast<uint32_t>(size) + 0x10, o.owner);
            if (sb_ != 0) mem_copy128(sb_, p, size);
            if (o.shared != nullptr) *o.shared = sb_;
        }
        r.shared = sb_;
        p += static_cast<uint32_t>(size);
        for (int32_t k = n; k > 0; --k, p += 8) {
            const uint32_t w = lwu(p);
            const auto it = r.moved.find(static_cast<int32_t>(w & 0xffff));
            if (it == r.moved.end() || sb_ == 0) continue;
            sw(it->second + static_cast<uint32_t>(static_cast<int32_t>(w) >> 16), sb_ + lwu(p + 4));
        }
    }
    return r;
}

// ---- the streaming ----
bool read_all(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

// orig: (host) the worker's part: a section's files read, its gameplay file decompressed
Bytes read_section(std::string dir, bool c, bool s, bool g) {
    const double t0 = now_ms();
    Bytes b;
    if (c && !read_all(dir + "/collision.bin", b.coll)) b.err = dir + "/collision.bin cannot be read";
    if (s && !read_all(dir + "/sky.bin", b.sky)) b.err = dir + "/sky.bin cannot be read";
    if (g) {
        std::vector<uint8_t> packed;
        if (!read_all(dir + "/gameplay.bin", packed) ||
            !decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(packed.data()), packed.size()), b.gp))
            b.err = dir + "/gameplay.bin cannot be read";
    }
    b.ms = now_ms() - t0;
    return b;
}

// orig: (host) the worker's part for a section of a game level's world (stage 8): its own files (a collision or sky of its own),
// the level's from the disc (section_world.h: its world's blocks, its collision and sky unless its own, its gameplay file)
Bytes read_section_world(std::string dir, int level, bool pal, bool c, bool s, bool g) {
    const double t0 = now_ms();
    Bytes b = read_section(dir, c, s, g);
    if (!b.err.empty()) return b;
    b.own_gp = std::move(b.gp);  // (stage 10: its own rails)
    b.world = std::make_unique<WorldBytes>();
    std::string err;
    if (!world_read(level, pal, !c, !s, *b.world, err)) {
        b.err = err;
        return b;
    }
    if (!c) b.coll = std::move(b.world->coll);
    if (!s) b.sky = std::move(b.world->sky);
    b.gp = std::move(b.world->gp);
    b.ms = now_ms() - t0;
    return b;
}

void request(int idx) {
    Section& s = g_secs[static_cast<size_t>(idx)];
    if (idx == 0 || s.st != St::Absent) return;
    s.st = St::Loading;
    s.req_frame = g_frame;
    s.req_ms = now_ms();
    if (s.world >= 0) mrp_world_prefetch(s.world);  // (the modern renderer's: its world asset read on a worker too)
    if (s.world >= 0)
        s.fut = std::async(std::launch::async, read_section_world, g_build + "/section_" + s.name, s.world, lw(0x0015ed80) != 0, s.f_coll, s.f_sky, s.f_gp);
    else
        s.fut = std::async(std::launch::async, read_section, g_build + "/section_" + s.name, s.f_coll, s.f_sky, s.f_gp);
    if (trace()) std::printf("[section] %s: streaming asked at frame %llu\n", s.name.c_str(), static_cast<unsigned long long>(g_frame));
}

// (stage 8) a world section's groups emptied in its level's registry (its objects' blocks about to go)
void groups_clear(Section& s) {
    for (const int32_t g : s.groups) ld_group_put(ld_world_key(s.world), g, nullptr, 0, owner_objects(s));
    s.groups.clear();
}

void free_section(Section& s) {
    if (s.fut.valid()) s.fut.wait();
    groups_clear(s);
    session_heap_free_owner(owner_files(s));
    session_heap_free_owner(owner_objects(s));
    s.coll = s.coll_tree = s.coll_list = s.sky = s.gp = 0;
    s.rails = 0;
    s.rail_count = 0;
    s.own_rail_first = 0;
    s.shared = 0;
    s.wb.reset();
    s.hud = WorldHud{};
    s.st = St::Absent;
    s.b = Bytes{};
    s.step = 0;
    s.copied = 0;
}

// (stage 8) a world section's moby groups (its gameplay file's +0x48, as gameplay_load makes them: lists of its placements' indices
// - "uids" in the game's naming, through its map 0x1ac780, which is by placement) in its level's registry (its classes read them there), naming its objects in play now; the lists the
// section's objects' blocks
void world_groups(Section& s) {
    const int key = ld_world_key(s.world);  // (the base level's own world: its registry's key, leveldata.h)
    for (const int32_t g : s.groups) ld_group_put(key, g, nullptr, 0, owner_objects(s));
    s.groups.clear();
    if (s.gp == 0 || lw(s.gp + 0x48) == 0 || !ld_registry_has(key)) return;
    std::map<int32_t, uint32_t> by_src;
    for (const Obj& o : s.objs)
        if (o.src >= 0 && alive(o)) by_src[o.src] = o.moby;
    uint32_t a = s.gp + lwu(s.gp + 0x48);
    const int32_t n = std::min<int32_t>(lw(a), static_cast<int32_t>(LD_GROUP_MAX));
    const uint32_t lists = a + 0x10 + static_cast<uint32_t>(lw(a)) * 4;
    a += 0x10;
    std::vector<uint32_t> mobys;
    for (int32_t g = 0; g < n; ++g, a += 4) {
        const int32_t off = lw(a);
        if (off < 0) continue;
        mobys.clear();
        for (uint32_t rd = lists + static_cast<uint32_t>(off);; rd += 2) {
            const auto it = by_src.find(static_cast<int32_t>(lhu(rd) & 0x7fffu));
            if (it != by_src.end()) mobys.push_back(it->second);
            if ((lh(rd) & 0x8000) != 0) break;
        }
        if (mobys.empty()) continue;
        if (ld_group_put(key, g, mobys.data(), static_cast<int>(mobys.size()), owner_objects(s))) s.groups.push_back(g);
    }
}

// (a world's) its level's cameras' pvar words naming a placement (the gameplay file's +0x50 list, as for the objects' pvars:
// Umbris's boss camera, type 0x12, names the Snagglebeast) made the slots of the section's objects placed from them (-1: never
// placed). The level's own load leaves them as they are, a placement's index being its slot there; in a section the objects
// are elsewhere in the moby array, and the camera turned toward an empty slot's (0, 0, 0)
void world_camera_links(const Section& s) {
    if (s.gp == 0 || lw(s.gp + 0x50) == 0 || lw(s.gp + 8) == 0 || lw(s.gp + 0x54) == 0 || !ld_registry_has(ld_world_key(s.world))) return;
    std::map<int32_t, uint32_t> by_src;
    for (const Obj& o : s.objs)
        if (o.src >= 0 && alive(o)) by_src[o.src] = o.moby;
    std::map<int32_t, uint32_t> cams;  // (a camera's pvar index -> its pvar block in the registry)
    const uint32_t c0 = s.gp + lwu(s.gp + 8);
    for (int32_t k = 0; k < lw(c0); ++k) {
        const int32_t pv = lw(c0 + 0x10 + static_cast<uint32_t>(k) * 0x20 + 0x1c);
        const uint32_t blk = ld_camera_pvars(ld_world_key(s.world), static_cast<uint32_t>(k));
        if (pv >= 0 && blk != 0) cams[pv] = blk;
    }
    if (cams.empty()) return;
    const uint32_t tab = s.gp + lwu(s.gp + 0x54), data = s.gp + lwu(s.gp + 0x58), base = lwu(OV(0x0015ffd8));
    for (uint32_t a = s.gp + lwu(s.gp + 0x50); lw(a) >= 0; a += 8) {
        const auto it = cams.find(lw(a));
        if (it == cams.end()) continue;
        const int32_t v = lw(data + lwu(tab + static_cast<uint32_t>(lw(a)) * 8) + lwu(a + 4));  // (the level's own value: its placement)
        if (v < 0) continue;
        const auto m = by_src.find(v);
        const uint32_t w = it->second + lwu(a + 4);
        if (m != by_src.end()) sw(w, (m->second - base) >> 8);
        else if (lw(w) == v) sw(w, 0xffffffffu);  // (never placed: none, the game's own word for it; once placed, its slot kept: out of play there)
    }
}

// a section's objects placed (its gameplay's placements but the uids in `skip`, or only those in `only`; its classes of other
// levels but those in `skip_imp`, or only those in `only_imp`), their slots set (level.txt's moby lines' `slot=value`)
// (stage 10) A world's level state: the per-level records the game keeps for the current level and its objects read (its row of
// the collected-things table 0x14c050, its killed-for-good bits 0x14c190, its group counts 0x14d590: the rows of the session's
// level number, the base's) and the level's live copy of its collected things (0x15fc88, made from its row at a level's start).
// In a session these are the level mod's own (level_slots swaps them in for its level), and a world section's objects would
// read and write the session's: a world's level gets rows of its own, kept by the session for the visit (a first visit's, all
// zero, when the session starts), put in while its section is current and while its objects are placed or placed again. The
// planet's real records (its row of its own level number) are never touched: a boss beaten in a session stays to beat on the
// planet itself (DECISIONS 8)
std::map<int, std::vector<uint8_t>> g_rows;  // (each world level's rows while not in; -1 the session's own)
int g_rows_level = -1;                       // (whose rows are the engine's)
uint32_t g_rows_lv = 0;                      // (the session's level number: its rows' row)
constexpr uint32_t kRowSizes[4] = {0x10, 0x100, 0x100, 0x10};
uint32_t row_addr(int k) {
    const uint32_t lv = g_rows_lv;
    switch (k) {
    case 0: return 0x0014c050u + lv * 0x10;
    case 1: return 0x0014c190u + lv * 0x100;
    case 2: return 0x0014d590u + lv * 0x100;
    default: return OV(0x0015fc88u);
    }
}
std::vector<uint8_t> rows_read() {
    std::vector<uint8_t> b;
    for (int k = 0; k < 4; ++k)
        for (uint32_t i = 0; i < kRowSizes[k]; ++i) b.push_back(static_cast<uint8_t>(lbu(row_addr(k) + i)));
    return b;
}
void rows_write(const std::vector<uint8_t>& b) {
    size_t at = 0;
    for (int k = 0; k < 4; ++k)
        for (uint32_t i = 0; i < kRowSizes[k]; ++i, ++at) sb(row_addr(k) + i, at < b.size() ? b[at] : 0u);
}
// orig: (host) level `level`'s rows (-1 the session's own) made the engine's: the level whose rows were
int rows_put(int level) {
    const int was = g_rows_level;
    if (level == was) return was;
    if (was == -1) g_rows_lv = lwu(LEVEL);  // (taken while the session's own are in: its level number now)
    g_rows[was] = rows_read();
    const auto it = g_rows.find(level);
    rows_write(it != g_rows.end() ? it->second : std::vector<uint8_t>{});
    g_rows_level = level;
    return was;
}

// (stage 10) an object of class `cls` (the session's id) is one of section `s`'s bosses by its `boss` lines
bool boss_class(const Section& s, uint32_t cls) {
    for (const auto& [l, c] : s.boss_cls) {
        int32_t id = session_import_id(l, c);
        if (id < 0 && l == lw(LEVEL)) id = static_cast<int32_t>(c);
        if (id >= 0 && static_cast<uint32_t>(id) == cls) return true;
    }
    return false;
}

size_t spawn_objects(Section& s, const std::set<int32_t>* only, const std::set<int>* only_imp, size_t* refused) {
    size_t n = 0;
    const int rows_was = s.world >= 0 && g_worlds ? rows_put(s.world) : g_rows_level;  // (stage 10: the spawn rules read its level's state)
    if (s.gp != 0 && lw(s.gp + 0x44) != 0 && lw(s.gp + lwu(s.gp + 0x44)) > 0) {  // (a gameplay file of rails only: no placements)
        SpawnOpts o;
        std::set<int32_t> skip = s.beaten;  // (stage 10: a boss beaten is never placed again in the visit)
        if (only == nullptr) skip.insert(s.dead.begin(), s.dead.end());
        o.only = only;
        o.skip = &skip;
        o.owner = owner_objects(s);
        o.shared = &s.shared;
        if (s.world >= 0) {  // (stage 8: a game level's world: its objects the composer chose, by their uids there)
            o.uids = &s.wuids;
            o.cls_level = s.world;
            o.world = s.wb.get();
        }
        const SpawnResult r = section_spawn_placements(s.gp, o);
        if (refused != nullptr) *refused += r.refused;
        if (r.full != 0) std::fprintf(stderr, "[section] %s: the moby array is full: %zu objects not placed\n", s.name.c_str(), r.full);
        if (r.absent != 0) std::fprintf(stderr, "[section] %s: %zu objects of level %d's classes the session lacks: not placed\n", s.name.c_str(), r.absent, s.world);
        for (const Spawned& sp : r.mobys) {
            s.objs.push_back({sp.moby, static_cast<int16_t>(lh(sp.moby + 0xb2)), sp.cls, -1, 0, sp.src});
            s.objs.back().boss = boss_class(s, sp.cls);
            s.objs.back().cptr = lwu(sp.moby + 0x24);
            for (const Bind& b : s.binds)
                if (b.uid == sp.uid && b.cls == sp.cls) session_bind(sp.moby, lw(LEVEL), sp.cls, b.slot, b.value);
            ++n;
        }
    }
    for (size_t j = 0; j < s.imports.size(); ++j) {
        const int ji = static_cast<int>(j);
        if (only_imp != nullptr ? only_imp->count(ji) == 0 : (only != nullptr || s.dead_imp.count(ji) != 0)) continue;
        if (s.beaten_imp.count(ji) != 0) continue;  // (stage 10: a boss beaten)
        const ImportPlace& ip = s.imports[j];
        const int32_t id = session_import_id(ip.level, ip.cls);
        if (id < 0) {
            std::fprintf(stderr, "[section] %s: class %d:%u is not in the session (not imported): not placed\n", s.name.c_str(), ip.level, ip.cls);
            continue;
        }
        const uint32_t m = session_import_spawn(static_cast<uint32_t>(id), ip.pos, ip.yaw);
        if (m == 0) {
            std::fprintf(stderr, "[section] %s: class %d:%u cannot be spawned (no room)\n", s.name.c_str(), ip.level, ip.cls);
            continue;
        }
        for (const auto& [slot, value] : ip.binds) session_bind(m, ip.level, ip.cls, slot, value);
        s.objs.push_back({m, static_cast<int16_t>(lh(m + 0xb2)), lhu(m + 0xa6), ji, 0});
        s.objs.back().boss = boss_class(s, lhu(m + 0xa6));
        s.objs.back().cptr = lwu(m + 0x24);
        ++n;
    }
    s.spawned = true;
    if (s.world >= 0) {
        world_groups(s);
        world_camera_links(s);
    }
    if (g_worlds) rows_put(rows_was);
    return n;
}

void freeze(Section& s) {
    for (Obj& o : s.objs) {
        if (!alive(o)) continue;
        o.f34 = lhu(o.moby + 0x34);
        sh(o.moby + 0x34, o.f34 | 2u);  // (not queued: neither updated nor drawn)
        moby_grid_move(o.moby, GRID_OUT);
    }
    for (auto& [m, k] : s.kids) {
        if (!kid_alive(m, k)) continue;
        k.f34 = lhu(m + 0x34);
        sh(m + 0x34, k.f34 | 2u);
        moby_grid_move(m, GRID_OUT);
    }
    s.frozen = true;
}

void unfreeze(Section& s) {
    for (const Obj& o : s.objs) {
        if (!alive(o)) continue;
        sh(o.moby + 0x34, (lhu(o.moby + 0x34) & ~2u) | (o.f34 & 2u));
        moby_update_transform_placed(o.moby);  // (in the grid again where it stands)
    }
    for (const auto& [m, k] : s.kids) {
        if (!kid_alive(m, k)) continue;
        sh(m + 0x34, (lhu(m + 0x34) & ~2u) | (k.f34 & 2u));
        moby_update_transform_placed(m);
    }
    s.frozen = false;
}

// the section's objects as it is left, by its policy: their counts (in play then)
size_t leave_objects(Section& s, int idx) {
    const int32_t pol = s.policy;
    size_t n = idx == 0 ? session_rt_main_leave(pol) : 0;
    if (pol == SECTION_KEEP) return n + s.objs.size();
    if (pol == SECTION_FREEZE) {
        for (const Obj& o : s.objs) n += alive(o) ? 1 : 0;
        freeze(s);
        return n;
    }
    for (const Obj& o : s.objs) {
        if (alive(o)) {
            moby_retire(o.moby);
            ++n;
        } else if (o.imp >= 0) {
            s.dead_imp.insert(o.imp);
        } else {
            s.dead.insert(o.uid);
        }
    }
    // what they spawned (a boss's helpers and effects): their level's code must not go on running outside its section
    kids_retire(s, 0);
    s.objs.clear();
    s.spawned = false;
    s.frozen = false;
    groups_clear(s);
    session_heap_free_owner(owner_objects(s));
    s.shared = 0;
    return n;
}

size_t enter_objects(Section& s, int idx) {
    size_t n = idx == 0 ? session_rt_main_enter(s.policy) : 0;
    if (!s.spawned) return n + spawn_objects(s, nullptr, nullptr, nullptr);
    if (s.frozen) unfreeze(s);
    for (const Obj& o : s.objs) n += alive(o) ? 1 : 0;
    return n;
}

// (ABI 17) the killed-for-good bit of placement `uid` in the session's own level state (the level records the game keeps for the
// level mod's level: the bits a destroyed crate sets, which the spawn rule reads): cleared. The state is the engine's own while no
// world section is current, else the session keeps it (rows_put)
void session_bit_clear(int32_t uid) {
    if (uid < 0 || uid >= 2048) return;
    const uint32_t off = static_cast<uint32_t>(uid >> 5) * 4, bit = 1u << (uid & 0x1f);
    if (g_rows_level == -1) {
        const uint32_t a = 0x0014c190u + lwu(LEVEL) * 0x100 + off;
        sw(a, lwu(a) & ~bit);
        return;
    }
    const auto it = g_rows.find(-1);
    if (it == g_rows.end() || it->second.size() < 0x10 + 0x100) return;
    uint32_t w = 0;
    std::memcpy(&w, it->second.data() + 0x10 + off, 4);
    w &= ~bit;
    std::memcpy(it->second.data() + 0x10 + off, &w, 4);
}

// (ABI 17) placement `uid`'s visit state in the game's tables the spawn rule reads besides the level's records: its visit bit (0x1ba4d0:
// killed for the visit), its live state byte (0x1baa24: set when it breaks) and the checkpoint's copy of it (0x1bb684: a checkpoint's save
// keeps a placement killed for good, the boss's own at its death): cleared
void visit_state_clear(int32_t uid) {
    if (uid < 0 || uid >= 0x7ff) return;
    const uint32_t w = static_cast<uint32_t>(uid >> 5) * 4;
    sw(OV(0x001ba4d0) + w, lwu(OV(0x001ba4d0) + w) & ~(1u << (uid & 0x1f)));
    sb(OV(0x001baa24) + static_cast<uint32_t>(uid), 0);
    sb(OV(0x001bb684) + static_cast<uint32_t>(uid), 0);
}

// (ABI 17) section `s` as at its first entry in the session: what is in play of it (its objects, the mobys they spawned, the bosses the
// plugin named and their spawns) taken out, what was killed or beaten in it forgotten (placed again at the next entry, the bosses to
// beat again: boss_beaten 0), its objects' blocks freed. The level state its objects wrote (the bits and flags of the spawn rule) put
// back: a world section's level has rows of its own (the rows of its world's level, shared by every section of that world), a
// section of the mod's own clears its placements' bits in the session's (the card's). Never the current section: its objects are in play
// (and a death's reset is the respawn policy's)
void reset_section(Section& s, const char* why) {
    size_t out = 0;
    for (const Obj& o : s.objs)
        if (alive(o)) {
            moby_retire(o.moby);
            ++out;
        }
    size_t plug = 0;
    for (const Obj& o : s.pboss)
        if (o.moby != 0 && alive(o)) {
            moby_retire(o.moby);
            ++plug;
        }
    const size_t kids = kids_retire(s, 0);
    const size_t forgot = s.dead.size() + s.dead_imp.size() + s.beaten.size() + s.beaten_imp.size();
    std::set<int32_t> gone(s.dead);
    gone.insert(s.beaten.begin(), s.beaten.end());
    for (const Obj& o : s.objs)
        if (o.imp < 0) gone.insert(o.uid);
    const int beaten = s.beaten_n;
    s.objs.clear();
    s.pboss.clear();
    s.dead.clear();
    s.dead_imp.clear();
    s.beaten.clear();
    s.beaten_imp.clear();
    s.beaten_n = 0;
    s.spawned = s.frozen = false;
    s.entered = false;
    groups_clear(s);
    session_heap_free_owner(owner_objects(s));
    s.shared = 0;
    std::set<int32_t> uids(s.puids.begin(), s.puids.end());
    for (const auto& [from, to] : s.wuids) uids.insert(to);
    for (const int32_t u : gone) uids.insert(u);
    for (const int32_t u : uids) visit_state_clear(u);
    const char* rows = "none";
    if (g_worlds && s.world >= 0) {
        g_rows.erase(s.world);
        if (g_rows_level == s.world) rows_write({});  // (another section of the same world is current: its level state is the engine's now)
        rows = "its world's level state back to a first visit's";
    } else if (s.world < 0 && !s.puids.empty()) {
        for (const int32_t u : s.puids) session_bit_clear(u);
        rows = "its placements' killed-for-good bits cleared";
    }
    std::printf("[section] %s reset (%s): %zu objects in play taken out, %zu mobys they spawned and %zu of the plugin's, %zu killed or beaten forgotten (%d bosses "
                "beaten), level state: %s, the visit's bits and state bytes of its %zu uids cleared; placed again at its next entry\n",
                s.name.c_str(), why, out, kids, plug, forgot, beaten, rows, uids.size());
}

void hero_pos(float p[3]) {
    const uint32_t hm = lwu(P + 0x2080);
    for (uint32_t k = 0; k < 3; ++k) p[k] = hm != 0 ? lwc1(hm + 0x10 + 4 * k) : 0.0f;
}

// (stage 9) a section's hero mode: the game's (hero_modes.h) by its level.txt name
int32_t mode_of(const std::string& m) {
    return m == "clank" ? HERO_MODE_CLANK : m == "giantclank" ? HERO_MODE_GIANT_CLANK : m == "board" ? HERO_MODE_BOARD : HERO_MODE_NONE;
}

// out of section `s`'s hero mode (when Ratchet is in one), the object spawned for it taken out
void hero_mode_leave(Section& s) {
    if (s.hero_mode.empty()) return;
    if (hero_mode_now() != HERO_MODE_NONE) hero_mode_set(HERO_MODE_NONE, 0, ("leaving section " + s.name).c_str());
    if (s.hero_obj != 0 && static_cast<int8_t>(lbu(s.hero_obj + 0x20)) >= 0) moby_retire(s.hero_obj);
    s.hero_obj = 0;
}

// the current section's hero mode entered where Ratchet stands (its object spawned there): at the first frame of play after
// the switch (or a respawn, or the arrival)
void hero_mode_frame() {
    if (!g_mode_pending || g_cur < 0) return;
    Section& s = g_secs[static_cast<size_t>(g_cur)];
    if (s.hero_mode.empty()) {
        g_mode_pending = false;
        return;
    }
    if (lw(OV(0x0015f5c4)) != 0 || lwu(P + 0x2080) == 0 || lbu(P + 0x20b1) != 0 || g_pend.on) return;  // (not playing yet: a fade, a scene)
    g_mode_pending = false;
    const int32_t mode = mode_of(s.hero_mode);
    if (hero_mode_now() == mode) return;
    const int level = mode == HERO_MODE_CLANK ? 10 : mode == HERO_MODE_GIANT_CLANK ? 15 : s.hero_level;
    const int32_t id = session_import_id(level, hero_mode_class(mode));
    float p[3];
    hero_pos(p);
    const float yaw = lwc1(lwu(P + 0x2080) + 0x48);
    if (s.hero_obj != 0 && static_cast<int8_t>(lbu(s.hero_obj + 0x20)) >= 0) moby_retire(s.hero_obj);
    s.hero_obj = id >= 0 ? session_import_spawn(static_cast<uint32_t>(id), p, yaw) : 0;
    if (s.hero_obj == 0) {
        std::fprintf(stderr, "[section] %s: its hero mode %s: no object of class %d:%u could be spawned\n", s.name.c_str(), s.hero_mode.c_str(), level,
                     hero_mode_class(mode));
        return;
    }
    hero_mode_set(mode, s.hero_obj, ("section " + s.name).c_str());
}

void apply_music(const uint32_t w[15]) {
    if (std::memcmp(w, g_cur_music, sizeof g_cur_music) == 0) return;
    std::memcpy(g_cur_music, w, sizeof g_cur_music);
    level_music_words_put(w);
    music_stop_all();
    music_start(lh(0x00151708), 1, 0x400);
}

// the current section's state made the game's: collision, sky, settings, music, lights, the meshes drawn
void apply_state(int idx) {
    const Section& s = g_secs[static_cast<size_t>(idx)];
    // (stage 8) a game level's world: its tables, instances, zone grid and lights the engine's, its fog's words, its level data the
    // engine's objects, the modern renderer drawing its level's world; another section: the session's own
    const bool world = idx != 0 && s.world >= 0 && s.wb != nullptr;
    if (g_worlds) {  // (a session with game worlds only: the others' switches are stage 6's)
        world_state_put(world ? s.wb->st : g_main.world);
        if (world && s.zones_all) sw(OV(0x0016c074), 0);  // (the zone mode: 0 every zone visible, zones_update's; the world's own 2 the grid's)
        world_lights_put(world ? s.wb->lights : g_main.lights);
        for (uint32_t k = 0; k < 4; ++k) sw(OV(0x0015f448) + 4 * k, world && s.has_fogw ? s.fogw[k] : g_main.fogw[k]);
        if (world) ld_engine_take(ld_world_key(s.world));  // (the base level's own world: its registry's key, leveldata.h)
        else ld_engine_restore();
        world_renderer_level(world ? s.world : -1);
        world_hud_put(world ? &s.hud : nullptr);  // (stage 10: its level's HUD pictures)
        rows_put(world ? s.world : -1);           // (stage 10: its level's state, the session's own outside)
    }
    const bool own_coll = idx != 0 && s.coll_tree != 0;
    sw(OV(0x00173e40), own_coll ? s.coll_tree : g_main.coll_tree);
    sw(OV(0x0015f618), own_coll ? s.coll_list : g_main.coll_list);
    sw(OV(0x0016051c), idx != 0 && s.sky != 0 ? s.sky : g_main.sky);
    for (uint32_t k = 0; k < 3; ++k) {
        sw(S + 0x23c + 4 * k, idx != 0 && s.has_bg ? static_cast<uint32_t>(s.bg[k]) : g_main.bg[k]);
        sb(OV(0x0015f444) + k, idx != 0 && s.has_fog ? static_cast<uint32_t>(s.fog[k]) : g_main.fog[k]);
    }
    fog_setup();
    bg_color_set(lwu(S + 0x23c), lwu(S + 0x240), lwu(S + 0x244));
    if (idx != 0 && s.has_death) swc1(OV(0x0015f638), s.death);
    else sw(OV(0x0015f638), g_main.death);
    // (stage 7) its grind rails the game's (the hero's rail search reads the current section's): none when it has none
    sw(OV(0x0015f70c), idx != 0 ? s.rails : g_main.rails);
    sw(OV(0x0015f710), idx != 0 ? static_cast<uint32_t>(s.rail_count) : g_main.rail_count);
    // (a world's) the same table its level's own code sees (its sites read its registry's rails: a rail camera's test of the rail
    // Ratchet grinds, Batalia's type 3, compared another copy's points and let go of him every other frame)
    if (g_worlds && world) ld_rails_put(ld_world_key(s.world), s.rails, static_cast<uint32_t>(s.rail_count));
    setpieces_section(idx);
    uint32_t w[15] = {};
    if (idx == 0 || s.music == -2 || !s.music_known) std::memcpy(w, g_main.music, sizeof w);
    else if (s.music >= 0) std::memcpy(w, s.music_words, sizeof w);
    apply_music(w);
    mrp_user_lights_section(idx != 0 && !s.lights.empty() ? g_mod_dir + "/level/" + s.lights : std::string());
    // (stage 10) its world's camera code (its camera types, hooks and callbacks: a boss's camera on its planet's world), then (stage 9)
    // its hero code (and camera code): its hero line's level, or the session base's
    if (g_worlds) hero_camera_level_set(world ? s.world : -1, ("section " + s.name).c_str());
    // (stage 10: a section without a hero line runs main's hero code: main's `hero generic` holds in a world's section too; not
    // main's mode)
    hero_build_set(idx == 0 || s.has_hero ? s.hero_level : g_secs[0].hero_level, ("section " + s.name).c_str());
    static const bool all = std::getenv("RAC1_SECTION_DRAW_ALL") != nullptr && std::atoi(std::getenv("RAC1_SECTION_DRAW_ALL")) != 0;  // (testing)
    mrpw::host_meshes_sections_set(all ? 0xffffffffu : 1u << static_cast<uint32_t>(idx));
}

bool in_box(const Portal& pt, const float p[3]) {
    for (int k = 0; k < 3; ++k)
        if (p[k] < pt.lo[k] || p[k] > pt.hi[k]) return false;
    return true;
}

// the switch itself (DESIGN 7.3), in one frame: the leaving section's objects by its policy, the entering one's state and
// objects, Ratchet at the entry (the camera cut behind him)
void do_switch(int to, const Entry* e, const std::string& why) {
    const double t0 = now_ms();
    const int from = g_cur;
    Section& F = g_secs[static_cast<size_t>(from)];
    Section& T = g_secs[static_cast<size_t>(to)];
    hero_mode_leave(F);  // (stage 9: out of its hero mode first, his own moby the hero's)
    const size_t left = leave_objects(F, from);
    // (ABI 17) fresh: every entry is the first. A world section's first entry finds it as it was placed (its level state is the session's
    // own, empty at the arrival); one of the mod's own writes the card's records (a crate broken stays so in a save): put back at every entry
    if (T.fresh && (T.entered || T.world < 0)) reset_section(T, "fresh: every entry is the first");
    const double t1 = now_ms();
    g_cur = to;
    apply_state(to);
    const double t2 = now_ms();
    const size_t in = enter_objects(T, to);
    T.entered = true;
    const double t3 = now_ms();
    if (e != nullptr) {
        session_rt_hero_put(e->pos, e->yaw);
        session_rt_checkpoint_enter(e->name, to, e->pos, e->yaw);
    }
    // (stage 8) into or out of a game level's world: the camera system made again from the cameras now the engine's (its level's,
    // or the session's), as a level start makes it (the default camera is a level's)
    const int wf = from != 0 && F.wb != nullptr ? F.world : -1, wt = to != 0 && T.wb != nullptr ? T.world : -1;
    if (wf != wt) {
        camera_init();
        camera_update();
    }
    float hp[3];
    hero_pos(hp);
    for (Portal& pt : T.portals) pt.inside = in_box(pt, hp);  // (an entry inside a portal's box: it waits to be left first)
    g_mode_pending = !T.hero_mode.empty();  // (stage 9: its hero mode entered at the next frame of play)
    const double t4 = now_ms();
    std::printf("[section] frame %llu: %s -> %s%s%s (%s): the switch %.3f ms (objects left %.3f, state %.3f, objects entered %.3f, Ratchet %.3f); %s's objects "
                "%s: %zu, %s's in play: %zu; collision %s, sky %s, music %s; the session heap %u KB\n",
                static_cast<unsigned long long>(g_frame), F.name.c_str(), T.name.c_str(), e != nullptr ? " at entry " : "", e != nullptr ? e->name.c_str() : "",
                why.c_str(), t4 - t0, t1 - t0, t2 - t1, t3 - t2, t4 - t3, F.name.c_str(),
                F.policy == SECTION_FREEZE ? "frozen" : F.policy == SECTION_KEEP ? "kept running" : "removed", left, T.name.c_str(), in,
                to != 0 && T.coll_tree != 0 ? "its own" : "main's", to != 0 && T.sky != 0 ? ("level " + std::to_string(T.sky_level) + "'s").c_str() : "main's",
                to != 0 && T.music == -1 ? "none" : to != 0 && T.music >= 0 ? ("level " + std::to_string(T.music) + "'s").c_str() : "main's",
                session_heap_used() >> 10);
    mod_code_section_switch(F.name.c_str(), T.name.c_str(), e != nullptr ? e->name.c_str() : "");
}

const Entry* entry_of(const Section& s, const std::string& name) {
    for (const Entry& e : s.entries)
        if (e.name == name) return &e;
    return name.empty() && !s.entries.empty() ? &s.entries.front() : nullptr;
}

// a step of a section's commit (copies of at most 64 KB): false when it is done (ready)
bool commit_step(Section& s, int idx) {
    auto copy = [&](uint32_t dst, const std::vector<uint8_t>& src) {  // (a chunk of `src` at `s.copied`: true when all copied)
        const size_t n = std::min<size_t>(0x10000, src.size() - s.copied);
        if (n != 0) std::memcpy(ee_ram_ptr_write(dst + static_cast<uint32_t>(s.copied), static_cast<uint32_t>(n)), src.data() + s.copied, n);
        s.copied += n;
        if (s.copied < src.size()) return false;
        s.copied = 0;
        return true;
    };
    switch (s.step) {
    case 0: {  // the heap blocks it owns
        const std::string own = owner_files(s);
        s.coll = s.b.coll.empty() ? 0 : session_heap_alloc(static_cast<uint32_t>(s.b.coll.size()), own);
        s.sky = s.b.sky.empty() ? 0 : session_heap_alloc(static_cast<uint32_t>(s.b.sky.size()) + 0x100, own);
        if (s.sky != 0) s.sky = (s.sky + 0xff) & ~0xffu;  // (as the level's asset data places a sky: 256-byte aligned)
        s.gp = s.b.gp.empty() ? 0 : session_heap_alloc(static_cast<uint32_t>(s.b.gp.size()) + 0x10, own);
        if (s.world >= 0 && s.gp != 0 && s.b.world != nullptr) {  // (stage 8: its world's blocks)
            s.wb = std::make_unique<WorldBuild>();
            if (!world_build_begin(*s.wb, s.world, *s.b.world, s.gp, own)) s.wb.reset();
        }
        if ((!s.b.coll.empty() && s.coll == 0) || (!s.b.sky.empty() && s.sky == 0) || (!s.b.gp.empty() && s.gp == 0) || (s.world >= 0 && s.wb == nullptr)) {
            std::fprintf(stderr, "[section] %s: no room in the session heap: not streamed in\n", s.name.c_str());
            free_section(s);
            return false;
        }
        ++s.step;
        return true;
    }
    case 1:
        if (s.coll == 0 || copy(s.coll, s.b.coll)) ++s.step;
        return true;
    case 2:  // the collision's list relocated (level_collision_set's), its tree and list kept for the switch
        if (s.coll != 0) {
            const uint32_t p = s.coll;
            s.coll_tree = lw(p) != 0 ? p + lwu(p) : g_main.coll_tree;
            s.coll_list = g_main.coll_list;
            if (lw(p + 4) != 0) {
                const uint32_t l = p + lwu(p + 4);
                s.coll_list = l;
                uint32_t e = l + 0x1c;
                for (int32_t i = 0; i < lw(l); ++i, e += 0x10) sw(e, l + lwu(e));
            }
        }
        ++s.step;
        return true;
    case 3:
        if (s.sky == 0 || copy(s.sky, s.b.sky)) ++s.step;
        return true;
    case 4:  // the sky relocated as the level setup does (sub_0023f848), the current sky's pointer kept
        if (s.sky != 0) {
            const uint32_t keep = lwu(OV(0x0016051c));
            sub_0023f848(s.sky);
            sw(OV(0x0016051c), keep);
            if (trace())
                for (int32_t i = 0; i < lh(s.sky + 6); ++i)
                    std::printf("[section] %s's sky %08x: list %d at %08x, %d entries\n", s.name.c_str(), s.sky, i, lwu(s.sky + 0x20 + 4 * static_cast<uint32_t>(i)),
                                lw(lwu(s.sky + 0x20 + 4 * static_cast<uint32_t>(i))));
        }
        ++s.step;
        return true;
    case 5:
        if (s.gp == 0 || copy(s.gp, s.b.gp)) {
            if (s.world < 0 && s.gp != 0 && lw(s.gp + 0x44) != 0) {  // (its placements' uids: reset_section clears their killed-for-good bits)
                s.puids.clear();
                uint32_t r = s.gp + lwu(s.gp + 0x44);
                const int32_t n1 = lw(r);
                r += 0x10;
                for (int32_t k = 0; k < n1 && k < 4096; ++k) {
                    s.puids.push_back(lw(r + 12));
                    r += static_cast<uint32_t>((lw(r) >> 2) << 2);
                }
            }
            // (stage 7) its rail table, as the level's load makes it from the block +0x74 (level_load.cpp's grind lines): the
            // records in a block of its own, their paths in the gameplay file's copy
            // (stage 10) a world's section: its level's rails (their indices its objects know), then the mod's own (its own gameplay
            // file, in the heap beside: the SDK's rails find them after the level's, sections_rail_first)
            uint32_t own = 0;
            if (!s.b.own_gp.empty()) {
                own = session_heap_alloc(static_cast<uint32_t>(s.b.own_gp.size()) + 0x10, owner_files(s));
                if (own != 0) std::memcpy(ee_ram_ptr_write(own, static_cast<uint32_t>(s.b.own_gp.size())), s.b.own_gp.data(), s.b.own_gp.size());
            }
            const uint32_t files[2] = {s.gp, own};
            int32_t total = 0;
            for (const uint32_t G : files)
                if (G != 0 && lw(G + 0x74) != 0) total += std::max(lw(G + lwu(G + 0x74)), 0);
            s.rails = total > 0 ? session_heap_alloc(static_cast<uint32_t>(total) * 0x20, owner_files(s)) : 0;
            if (s.rails != 0) {
                int32_t at = 0;
                for (const uint32_t G : files) {
                    if (G == 0 || lw(G + 0x74) == 0) continue;
                    const uint32_t p = G + lwu(G + 0x74);
                    const int32_t n = lw(p);
                    const uint32_t pts = p + lwu(p + 4);
                    for (int32_t k = 0; k < n; ++k, ++at) {
                        const uint32_t d = s.rails + static_cast<uint32_t>(at) * 0x20, r = p + 0x10 + static_cast<uint32_t>(k) * 0x20;
                        for (uint32_t w = 0; w < 0x10; w += 4) sw(d + w, lwu(r + w));
                        sw(d + 0x10, pts + lwu(p + 0x10 + static_cast<uint32_t>(n) * 0x20 + static_cast<uint32_t>(k) * 4));
                        sw(d + 0x14, lwu(r + 0x10));
                        sw(d + 0x18, 0);
                        sw(d + 0x1c, 0);
                    }
                    if (G == s.gp && own != 0) s.own_rail_first = at;
                }
                s.rail_count = at;
            }
            // (stage 8) a world's settings, as gameplay_load reads them from its gameplay file, unless the section gives its own: the
            // background, the fog's colour and words, the death height; its level's music
            if (s.world >= 0 && s.gp != 0) {
                const uint32_t st = s.gp + lwu(s.gp);
                if (!s.has_bg)
                    for (uint32_t k = 0; k < 3; ++k) s.bg[k] = static_cast<int>(lwu(st + 4 * k));
                if (!s.has_fog)
                    for (uint32_t k = 0; k < 3; ++k) s.fog[k] = static_cast<int>(lbu(st + 0xc + 4 * k));
                s.has_bg = s.has_fog = true;
                for (uint32_t k = 0; k < 4; ++k) s.fogw[k] = lwu(st + 0x18 + 4 * k);
                s.has_fogw = true;
                if (!s.has_death) {
                    s.death = lwc1(st + 0x28);
                    s.has_death = true;
                }
                if (s.music == -2) s.music = s.world;
                if (s.sky_level < 0) s.sky_level = s.world;
            }
            ++s.step;
        }
        return true;
    case 6:  // its music (another level's tracks: that level's header)
        if (s.music >= 0) s.music_known = level_music_words_of(s.music, s.music_words);
        ++s.step;
        return true;
    case 7:  // (stage 8) its world: copied, registered and lit a step at a time; its level's context and registry (its level data)
        if (s.wb != nullptr && s.b.world != nullptr) {
            if (world_build_step(*s.wb, *s.b.world, owner_files(s))) return true;
            world_hud_build(s.hud, s.world, *s.b.world, owner_files(s));  // (stage 10: its level's HUD pictures the session lacks)
        }
        ++s.step;
        return true;
    case 8:  // its objects placed, frozen until it is entered
        if (!s.spawned) {
            spawn_objects(s, nullptr, nullptr, nullptr);
            if (idx != g_cur) freeze(s);
        }
        ++s.step;
        return true;
    default:
        return false;
    }
}

void commit_frame() {
    // one section committing at a time; the others' bytes wait (the worker's reads go on)
    int which = -1;
    for (size_t i = 1; i < g_secs.size() && which < 0; ++i)
        if (g_secs[i].st == St::Committing) which = static_cast<int>(i);
    for (size_t i = 1; i < g_secs.size() && which < 0; ++i) {
        Section& s = g_secs[i];
        if (s.st != St::Loading || s.fut.wait_for(std::chrono::seconds(0)) != std::future_status::ready) continue;
        s.b = s.fut.get();
        if (s.req_frame == 0) s.req_frame = g_frame;  // (asked at the arrival, before the session's first frame)
        if (!s.b.err.empty()) {
            std::fprintf(stderr, "[section] %s: %s: not streamed in\n", s.name.c_str(), s.b.err.c_str());
            s.st = St::Absent;
            continue;
        }
        s.st = St::Committing;
        s.step = 0;
        s.copied = 0;
        s.commit_frames = 0;
        s.commit_ms = s.commit_max = 0;
        which = static_cast<int>(i);
    }
    if (which < 0) return;
    Section& s = g_secs[static_cast<size_t>(which)];
    const double t0 = now_ms();
    while (now_ms() - t0 < budget_ms())
        if (!commit_step(s, which)) break;
    const double dt = now_ms() - t0;
    ++s.commit_frames;
    s.commit_ms += dt;
    s.commit_max = std::max(s.commit_max, dt);
    if (s.st == St::Absent) return;  // (failed)
    if (s.step < 9) return;
    s.st = St::Ready;
    std::printf("[section] %s streamed in: asked at frame %llu, read and decompressed on the worker in %.2f ms, committed in %d frames (at most %.3f ms a "
                "frame, %.3f ms in all; the budget %.1f ms), ready at frame %llu (%.1f ms after it was asked); collision %zu bytes, sky %zu, gameplay %zu, "
                "%zu objects placed (%s), %d rails\n",
                s.name.c_str(), static_cast<unsigned long long>(s.req_frame), s.b.ms, s.commit_frames, s.commit_max, s.commit_ms, budget_ms(),
                static_cast<unsigned long long>(g_frame), now_ms() - s.req_ms, s.b.coll.size(), s.b.sky.size(), s.b.gp.size(), s.objs.size(),
                which == g_cur ? "in play" : "frozen until it is entered", s.rail_count);
    if (s.wb != nullptr) {
        const WorldBuild& w = *s.wb;
        size_t copied = 0;
        for (const WorldBuild::At& a : w.map) copied += a.size;
        std::printf("[section] %s: level %d's world: %zu blocks, %.1f MB copied of its %.1f MB of asset data; %u tfrags, %u tie classes with %u instances, %u shrub "
                    "classes with %u instances, zone grid %s, %u effect textures%s; built on the game's thread in %.2f ms over the commit's frames; in the session heap %.1f MB (world "
                    "%.1f, collision %.1f, sky %.2f, gameplay %.1f); the session heap %u KB\n",
                    s.name.c_str(), s.world, w.map.size(), static_cast<double>(copied) / 1048576.0, static_cast<double>(s.b.world != nullptr ? s.b.world->core : 0) / 1048576.0,
                    w.tfrag_n, lwu(s.wb->index + 0x20), w.tie_n, lwu(s.wb->index + 0x28), w.shrub_n, w.masks ? "its own" : "none", w.fx_n,
                    w.fx_out != 0 ? (" (" + std::to_string(w.fx_out) + " past their block)").c_str() : "", w.ms,
                    static_cast<double>(w.bytes + s.b.coll.size() + s.b.sky.size() + s.b.gp.size()) / 1048576.0, static_cast<double>(w.bytes) / 1048576.0,
                    static_cast<double>(s.b.coll.size()) / 1048576.0, static_cast<double>(s.b.sky.size()) / 1048576.0, static_cast<double>(s.b.gp.size()) / 1048576.0,
                    session_heap_used() >> 10);
        std::printf("[section] %s: the world's steps' longest calls (ms): copy %.2f, registrations %.2f, tie instances %.2f, their lighting %.2f, shrub "
                    "instances %.2f, their lighting %.2f, tfrag lighting %.2f, masks %.2f\n",
                    s.name.c_str(), w.step_max[0], w.step_max[1], w.step_max[2], w.step_max[3], w.step_max[4], w.step_max[5], w.step_max[6], w.step_max[7]);
    }
    s.b = Bytes{};  // (the host's copies dropped: the heap holds them)
}

bool ready(int idx) { return idx == 0 || (idx > 0 && static_cast<size_t>(idx) < g_secs.size() && g_secs[static_cast<size_t>(idx)].st == St::Ready); }

void ask_switch(int to, const Entry* e, int fade, const std::string& why) {
    g_pend = Pending{};
    g_pend.on = true;
    g_pend.to = to;
    if (e != nullptr) {
        g_pend.e = *e;
        g_pend.has_entry = true;
    }
    g_pend.fade = std::clamp(fade, 0, 120);
    g_pend.why = why;
    request(to);
}

}  // namespace

// orig: (host) see sections.h
void sections_moby_spawned(uint32_t moby, uint32_t by) {
    if (!g_sections_on || moby == 0) return;
    for (Section& s : g_secs) s.kids.erase(moby);  // (its slot used again)
    if (by == 0 || by == moby) return;
    for (Section& s : g_secs) {
        if (!s.spawned) continue;
        uint32_t root = 0;
        for (const Obj& o : s.objs)
            if (o.moby == by && alive(o)) root = by;
        for (const Obj& o : s.pboss)
            if (o.moby == by && o.moby != 0 && alive(o)) root = by;
        if (root == 0)
            if (const auto it = s.kids.find(by); it != s.kids.end() && kid_alive(by, it->second)) root = it->second.root;
        if (root == 0) continue;
        Kid k;
        k.root = root;
        k.cptr = lwu(moby + 0x24);
        k.b2 = lh(moby + 0xb2);
        if (s.frozen) {  // (spawned by a frozen one: frozen with it)
            k.f34 = lhu(moby + 0x34);
            sh(moby + 0x34, k.f34 | 2u);
        }
        s.kids[moby] = k;
        return;
    }
}

// orig: (host) see sections.h
bool sections_world_object(uint32_t moby) {
    if (!g_sections_on || g_cur <= 0) return false;
    const Section& s = g_secs[static_cast<size_t>(g_cur)];
    if (s.world < 0) return false;
    for (const Obj& o : s.objs)
        if (o.moby == moby) return alive(o);
    const auto it = s.kids.find(moby);
    return it != s.kids.end() && kid_alive(moby, it->second);
}

// orig: (host) see sections.h
bool sections_boss_set(uint32_t moby) {
    if (!g_sections_on || g_cur < 0 || moby == 0 || static_cast<int8_t>(lbu(moby + 0x20)) < 0) return false;
    Section& s = g_secs[static_cast<size_t>(g_cur)];
    for (Obj& o : s.objs)
        if (o.moby == moby && alive(o)) {
            o.boss = true;
            return true;
        }
    for (const Obj& o : s.pboss)
        if (o.moby == moby) return true;
    Obj o;
    o.moby = moby;
    o.uid = static_cast<int16_t>(lh(moby + 0xb2));
    o.cls = lhu(moby + 0xa6);
    o.cptr = lwu(moby + 0x24);
    o.boss = true;
    for (int k = 0; k < 3; ++k) o.at[k] = lwc1(moby + 0x10 + 4 * static_cast<uint32_t>(k));
    o.at[3] = lwc1(moby + 0x48);
    s.pboss.push_back(o);
    return true;
}

// orig: (host) see sections.h
int sections_boss_beaten(const std::string& name) {
    if (!g_sections_on) return -1;
    const int i = name.empty() ? g_cur : sections_index(name);
    if (i < 0 || static_cast<size_t>(i) >= g_secs.size()) return -1;
    return g_secs[static_cast<size_t>(i)].beaten_n;
}

// orig: (host) see sections.h
int32_t sections_rail_first(int index) {
    if (!g_sections_on || index <= 0 || static_cast<size_t>(index) >= g_secs.size()) return 0;
    return g_secs[static_cast<size_t>(index)].own_rail_first;
}

// orig: (host) see sections.h
int sections_save_begin() { return g_worlds ? rows_put(-1) : -1; }

// orig: (host) see sections.h
bool sections_switching() { return g_sections_on && g_pend.on; }

// orig: (host) see sections.h
void sections_save_end(int level) {
    if (g_worlds && level >= 0) rows_put(level);
}

// orig: (host) see sections.h
void sections_level(const std::string& build_dir) {
    if (g_sections_on) {
        ld_engine_restore();
        world_renderer_level(-1);
        for (Section& s : g_secs) free_section(s);
        mrpw::host_meshes_sections_set(0xffffffffu);
        mrp_user_lights_section("");
        std::printf("[section] the session's sections dropped\n");
    }
    if (g_worlds) rows_put(-1);  // (stage 10: the session's own level state back before the level mod's records are kept)
    g_rows.clear();
    g_rows_level = -1;
    g_sections_on = false;
    g_worlds = false;
    world_hud_reset();
    g_secs.clear();
    g_cur = -1;
    g_start = 0;
    g_start_entry.clear();
    g_pend = Pending{};
    g_fade_alpha = 0;
    g_build = build_dir;
    if (build_dir.empty()) return;
    std::ifstream in(build_dir + "/sections.txt");
    if (!in) return;
    g_mod_dir = level_slot_mod_dir(level_slot());
    auto sec = [](int i) -> Section& {
        if (static_cast<size_t>(i) >= g_secs.size()) g_secs.resize(static_cast<size_t>(i) + 1);
        return g_secs[static_cast<size_t>(i)];
    };
    std::string line;
    size_t nportals = 0, ncheck = 0;
    while (std::getline(in, line)) {
        std::istringstream l(line);
        std::string k;
        int i = 0;
        if (!(l >> k)) continue;
        if (k == "sections") continue;
        if (!(l >> i) || i < 0 || i > 31) continue;
        if (k == "start") {
            g_start = i;
            l >> g_start_entry;
            continue;
        }
        Section& s = sec(i);
        if (k == "section") {
            std::string pol, rsp;
            l >> s.name >> pol >> rsp;
            s.policy = pol == "freeze" ? SECTION_FREEZE : pol == "keep" ? SECTION_KEEP : SECTION_REMOVE;
            s.respawn = rsp == "keep" ? SESSION_RESPAWN_KEEP : rsp == "reset" ? SESSION_RESPAWN_RESET : -1;
        } else if (k == "files") {
            int c = 0, sk = 0, g = 0;
            l >> c >> sk >> g;
            s.f_coll = c != 0, s.f_sky = sk != 0, s.f_gp = g != 0;
        } else if (k == "meshes") {
            l >> s.mesh_first >> s.mesh_count;
        } else if (k == "sky") {
            l >> s.sky_level;
        } else if (k == "music") {
            l >> s.music;
        } else if (k == "fog") {
            s.has_fog = static_cast<bool>(l >> s.fog[0] >> s.fog[1] >> s.fog[2]);
        } else if (k == "background") {
            s.has_bg = static_cast<bool>(l >> s.bg[0] >> s.bg[1] >> s.bg[2]);
        } else if (k == "death") {
            s.has_death = static_cast<bool>(l >> s.death);
        } else if (k == "lights") {
            l >> s.lights;
        } else if (k == "entry") {
            Entry e;
            if (l >> e.name >> e.pos[0] >> e.pos[1] >> e.pos[2] >> e.yaw) s.entries.push_back(e);
        } else if (k == "checkpoint") {
            std::string name;
            float p[3], yaw = 0;
            if (l >> name >> p[0] >> p[1] >> p[2] >> yaw) {
                session_rt_checkpoint_add(name, i, p, yaw);
                ++ncheck;
            }
        } else if (k == "portal") {
            Portal pt;
            if (l >> pt.name >> pt.lo[0] >> pt.lo[1] >> pt.lo[2] >> pt.hi[0] >> pt.hi[1] >> pt.hi[2] >> pt.to >> pt.entry >> pt.fade) {
                s.portals.push_back(pt);
                ++nportals;
            }
        } else if (k == "spawn") {
            ImportPlace ip;
            int j = 0;
            if (l >> j >> ip.level >> ip.cls >> ip.pos[0] >> ip.pos[1] >> ip.pos[2] >> ip.yaw) s.imports.push_back(ip);
        } else if (k == "ibind") {
            int j = 0;
            std::string slot, value;
            if (l >> j >> slot >> value && j >= 0 && static_cast<size_t>(j) < s.imports.size()) s.imports[static_cast<size_t>(j)].binds.emplace_back(slot, value);
        } else if (k == "bind") {
            Bind b;
            if (l >> b.uid >> b.cls >> b.slot >> b.value) s.binds.push_back(b);
        } else if (k == "world") {
            l >> s.world;
        } else if (k == "zones") {
            std::string w;
            l >> w;
            s.zones_all = w == "all";
        } else if (k == "hero") {
            l >> s.hero_level >> s.hero_mode;
            if (s.hero_mode == "-") s.hero_mode.clear();
            s.has_hero = true;
        } else if (k == "wuid") {
            int32_t from = 0, to = 0;
            if (l >> from >> to) s.wuids[from] = to;
        } else if (k == "fresh") {  // (ABI 17: every entry as the first)
            s.fresh = true;
        } else if (k == "boss") {  // (stage 10)
            int bl = -1;
            uint32_t bc = 0;
            if (l >> bl >> bc) s.boss_cls.emplace_back(bl, bc);
        }
    }
    if (g_secs.empty() || g_secs[0].name != "main") {
        g_secs.clear();
        std::fprintf(stderr, "[section] %s/sections.txt: no main section: the session has none\n", build_dir.c_str());
        return;
    }
    g_secs[0].st = St::Ready;
    g_sections_on = true;
    g_worlds = false;
    for (const Section& s : g_secs) g_worlds = g_worlds || s.world >= 0;
    std::printf("[section] the session's sections: %zu (", g_secs.size());
    for (size_t i = 0; i < g_secs.size(); ++i) std::printf("%s%s", i == 0 ? "" : ", ", g_secs[i].name.c_str());
    std::printf("), %zu portals, %zu checkpoints of their own; only the current one's meshes drawn\n", nportals, ncheck);
}

// orig: (host) see sections.h
void sections_arrival() {
    if (!g_sections_on) return;
    if (g_worlds) {
        world_state_get(g_main.world);
        world_lights_get(g_main.lights);
        for (uint32_t k = 0; k < 4; ++k) g_main.fogw[k] = lwu(OV(0x0015f448) + 4 * k);
        ld_engine_restore();
        world_renderer_level(-1);
    }
    g_main.coll_tree = lwu(OV(0x00173e40));
    g_main.coll_list = lwu(OV(0x0015f618));
    g_main.sky = lwu(OV(0x0016051c));
    for (uint32_t k = 0; k < 3; ++k) {
        g_main.bg[k] = lwu(S + 0x23c + 4 * k);
        g_main.fog[k] = static_cast<uint8_t>(lbu(OV(0x0015f444) + k));
    }
    g_main.death = lwu(OV(0x0015f638));
    g_main.rails = lwu(OV(0x0015f70c));
    g_main.rail_count = lwu(OV(0x0015f710));
    level_music_words_get(g_main.music);
    std::memcpy(g_cur_music, g_main.music, sizeof g_cur_music);
    g_cur = 0;
    mrpw::host_meshes_sections_set(1u);
    mrp_user_lights_section("");
    g_spawn_watch = true;  // (moby_spawn tells sections_moby_spawned: the mobys the sections' objects spawn)
    for (Section& s : g_secs) {  // (a save's load or the ship's return: fresh)
        s.objs.clear();
        s.kids.clear();
        s.spawned = s.frozen = s.entered = false;
        for (Portal& pt : s.portals) pt.inside = false;
    }
    Section& m = g_secs[0];
    const size_t n = spawn_objects(m, nullptr, nullptr, nullptr);
    float hp[3];
    hero_pos(hp);
    for (Portal& pt : m.portals) pt.inside = in_box(pt, hp);
    // (stage 8) the game worlds' levels' contexts and registries (their level data, the engine's while their section is current): made
    // now, during the arrival (their files read, the context's image relocated: 10 to 20 ms a level), not within a frame's budget
    for (size_t i = 1; i < g_secs.size(); ++i) {
        const int L = g_secs[i].world;
        if (L < 0) continue;
        if (L == lw(LEVEL)) {  // (the base level's own world: a registry of its level data, no context: the running code is the base's)
            if (!ld_registry_make(LD_BASE_WORLD))
                std::fprintf(stderr, "[section] %s: level %d's world registry cannot be made: its level data is not the engine's there\n", g_secs[i].name.c_str(), L);
            continue;
        }
        if (!ctx_create(L) || !ld_registry_make(L))
            std::fprintf(stderr, "[section] %s: level %d's context or registry cannot be made: its level data is not the engine's there\n", g_secs[i].name.c_str(), L);
    }
    // (stage 9) the hero lines' levels' contexts made now too; main's hero code and mode (the mode once the landing is over)
    for (const Section& s : g_secs)
        if (s.hero_level >= 0 && s.hero_level != lw(LEVEL) && !ctx_create(s.hero_level))
            std::fprintf(stderr, "[section] %s: level %d's context cannot be made: its hero code cannot run Ratchet\n", s.name.c_str(), s.hero_level);
    hero_build_set(m.hero_level, "section main at the arrival");
    m.hero_obj = 0;
    g_mode_pending = !m.hero_mode.empty();
    for (size_t i = 1; i < g_secs.size(); ++i) request(static_cast<int>(i));  // (every section streamed in while the landing plays)
    std::printf("[section] the arrival: main's collision %08x, sky %08x taken; %zu classes of other levels placed in main; %zu sections streaming in\n",
                g_main.coll_tree, g_main.sky, n, g_secs.size() - 1);
}

void bosses_frame(Section& s);  // (stage 10: below, beside the soft reset of the bosses)

// orig: (host) see sections.h
void sections_frame(uint64_t frame) {
    if (!g_sections_on || g_cur < 0) return;
    g_frame = frame;
    commit_frame();
    hero_mode_frame();
    // a switch asked (a portal, the plugin, a respawn's): once its section is in, with its fade
    if (g_pend.on) {
        if (!ready(g_pend.to)) {
            request(g_pend.to);
            return;
        }
        if (g_pend.phase == 0) {
            g_pend.phase = g_pend.fade > 0 ? 1 : 3;
            g_pend.k = 0;
        }
        if (g_pend.phase == 1) {  // (fading out)
            ++g_pend.k;
            g_fade_alpha = 0x80 * g_pend.k / g_pend.fade;
            if (g_pend.k < g_pend.fade) return;
            g_pend.phase = 3;
        }
        if (g_pend.phase == 3) {
            const Entry* e = g_pend.has_entry ? &g_pend.e : nullptr;
            if (g_pend.to != g_cur) {
                do_switch(g_pend.to, e, g_pend.why);
            } else if (e != nullptr) {  // (the current section's own entry: Ratchet put there)
                session_rt_hero_put(e->pos, e->yaw);
                session_rt_checkpoint_enter(e->name, g_cur, e->pos, e->yaw);
            }
            g_pend.phase = g_pend.fade > 0 ? 2 : 4;
            g_pend.k = g_pend.fade;
            if (g_pend.phase == 4) {
                g_pend.on = false;
                g_fade_alpha = 0;
            }
            return;
        }
        if (g_pend.phase == 2) {  // (fading in)
            --g_pend.k;
            g_fade_alpha = g_pend.fade > 0 ? 0x80 * g_pend.k / g_pend.fade : 0;
            if (g_pend.k <= 0) {
                g_pend.on = false;
                g_fade_alpha = 0;
            }
        }
        return;
    }
    // the portals of the current section: Ratchet stepping into one's box
    if (lw(OV(0x0015f5c4)) != 0 || lwu(P + 0x2080) == 0) return;  // (in play only)
    float hp[3];
    hero_pos(hp);
    Section& c = g_secs[static_cast<size_t>(g_cur)];
    bosses_frame(c);  // (stage 10)
    for (Portal& pt : c.portals) {
        const bool in = in_box(pt, hp);
        const bool entered = in && !pt.inside;
        pt.inside = in;
        if (!entered || pt.to < 0 || static_cast<size_t>(pt.to) >= g_secs.size()) continue;
        const Entry* e = entry_of(g_secs[static_cast<size_t>(pt.to)], pt.entry);
        ask_switch(pt.to, e, pt.fade, "portal " + pt.name);
        if (!ready(pt.to))
            std::printf("[section] frame %llu: portal %s: %s not streamed in yet: Ratchet passes once it is\n", static_cast<unsigned long long>(frame), pt.name.c_str(),
                        g_secs[static_cast<size_t>(pt.to)].name.c_str());
        break;
    }
}

// orig: (host) see sections.h
void sections_hud() {
    if (!g_sections_on || g_fade_alpha <= 0) return;
    gs_flat_rect(0, 0, 513, 449, static_cast<uint64_t>(static_cast<uint32_t>(std::min(g_fade_alpha, 0x80)) << 24), 0);  // (513 x 449: setpieces_hud's)
}

// orig: (host) see sections.h
bool sections_any() { return g_sections_on; }
// orig: (host) see sections.h
bool sections_world(int index) { return g_sections_on && index > 0 && static_cast<size_t>(index) < g_secs.size() && g_secs[static_cast<size_t>(index)].world >= 0; }
// orig: (host) see sections.h
int sections_count() { return g_sections_on ? static_cast<int>(g_secs.size()) : 0; }
// orig: (host) see sections.h
int sections_current() { return g_sections_on ? g_cur : -1; }
// orig: (host) see sections.h (the section's own sky as apply_state made it the game's)
int sections_sky_level() {
    if (!g_sections_on || g_cur <= 0 || static_cast<size_t>(g_cur) >= g_secs.size()) return -1;
    const Section& s = g_secs[static_cast<size_t>(g_cur)];
    return s.sky != 0 && lwu(OV(0x0016051c)) == s.sky ? s.sky_level : -1;
}
// orig: (host) see sections.h
int sections_world() {
    if (!g_sections_on || g_cur < 0 || static_cast<size_t>(g_cur) >= g_secs.size()) return -1;
    return g_secs[static_cast<size_t>(g_cur)].world;
}
// orig: (host) see sections.h
std::string sections_name(int index) { return g_sections_on && index >= 0 && static_cast<size_t>(index) < g_secs.size() ? g_secs[static_cast<size_t>(index)].name : std::string(); }
// orig: (host) see sections.h
int sections_index(const std::string& name) {
    if (!g_sections_on) return name == "main" ? 0 : -1;
    for (size_t i = 0; i < g_secs.size(); ++i)
        if (g_secs[i].name == name) return static_cast<int>(i);
    return -1;
}
// orig: (host) see sections.h
int sections_start() { return g_sections_on ? g_start : 0; }
// orig: (host) see sections.h
bool sections_start_entry(float pos[3], float* yaw, std::string* name) {
    if (!g_sections_on || g_start <= 0 || static_cast<size_t>(g_start) >= g_secs.size()) return false;
    const Entry* e = entry_of(g_secs[static_cast<size_t>(g_start)], g_start_entry);
    if (e == nullptr) return false;
    std::memcpy(pos, e->pos, 12);
    *yaw = e->yaw;
    *name = e->name;
    return true;
}
// orig: (host) see sections.h
bool sections_switch_now(int index) {
    if (!g_sections_on || index < 0 || static_cast<size_t>(index) >= g_secs.size()) return true;
    if (index == g_cur) return true;
    if (!ready(index)) {
        request(index);
        return false;
    }
    do_switch(index, nullptr, "a checkpoint there");
    return true;
}

// orig: (host) see sections.h
void sections_respawn_prepare(int index) {
    if (!g_sections_on || index < 0 || static_cast<size_t>(index) >= g_secs.size() || index == g_cur) return;
    if (!ready(index)) {  // (its section not in: the death's checkpoint taken in the current one's first entry)
        std::fprintf(stderr, "[section] the checkpoint's section %s is not streamed in: Ratchet stays in %s\n", g_secs[static_cast<size_t>(index)].name.c_str(),
                     g_secs[static_cast<size_t>(g_cur)].name.c_str());
        return;
    }
    g_pend = Pending{};
    g_fade_alpha = 0;
    do_switch(index, nullptr, "a death: the checkpoint's section");
}

// orig: (host) see sections.h
void sections_hero_respawned() {
    if (!g_sections_on || g_cur < 0) return;
    g_mode_pending = !g_secs[static_cast<size_t>(g_cur)].hero_mode.empty();
}

// orig: (host) see sections.h
int32_t sections_respawn_policy(int32_t session_policy) {
    if (!g_sections_on || g_cur <= 0) return session_policy;
    const int32_t r = g_secs[static_cast<size_t>(g_cur)].respawn;
    return r >= 0 ? r : session_policy;
}

// orig: (host) see sections.h
void sections_respawn_objects(int32_t policy, size_t* back, size_t* kept_dead) {
    if (!g_sections_on || g_cur < 0 || policy != SESSION_RESPAWN_RESET) return;
    Section& s = g_secs[static_cast<size_t>(g_cur)];
    g_pend = Pending{};
    g_fade_alpha = 0;
    if (!s.spawned) {  // (a section entered before its objects: placed now)
        *back += spawn_objects(s, nullptr, nullptr, nullptr);
        return;
    }
    // the killed ones (and those killed before it was last left) placed again as at its entry, into free slots
    std::set<int32_t> uids = s.dead;
    std::set<int> imps = s.dead_imp;
    std::vector<Obj> keep;
    std::vector<int32_t> again;  // (stage 10: its bosses in play taken out, placed again: the fight from its start)
    for (const Obj& o : s.objs) {
        if (o.boss && alive(o)) {
            moby_retire(o.moby);
            kids_retire(s, o.moby);  // (what it spawned: its fight from its start)
            if (o.imp >= 0) imps.insert(o.imp);
            else uids.insert(o.uid);
            again.push_back(o.imp >= 0 ? -1 - o.imp : o.uid);
            continue;
        }
        if (alive(o)) {
            keep.push_back(o);
        } else if (o.imp >= 0) {
            imps.insert(o.imp);
        } else {
            uids.insert(o.uid);
        }
    }
    s.objs = keep;
    s.dead.clear();
    s.dead_imp.clear();
    size_t refused = 0;
    if (!uids.empty() || !imps.empty()) *back += spawn_objects(s, &uids, &imps, &refused);
    *kept_dead += refused;
    // (stage 10) the bosses placed again: the plugin told (their new objects); those the plugin spawned and named, spawned again
    for (const int32_t a : again)
        for (const Obj& o : s.objs)
            if (o.boss && alive(o) && (a >= 0 ? o.imp < 0 && o.uid == a : o.imp == -1 - a)) {
                std::printf("[section] %s: its boss (class %u) placed again after the death: moby %08x, the fight from its start\n", s.name.c_str(), o.cls, o.moby);
                mod_code_boss(s.name.c_str(), static_cast<int32_t>(o.cls), o.moby, 2);
            }
    for (Obj& o : s.pboss) {
        if (o.moby == 0) continue;
        if (alive(o)) {
            moby_retire(o.moby);
            kids_retire(s, o.moby);
        }
        o.moby = session_import_spawn(o.cls, o.at, o.at[3]);
        if (o.moby == 0) continue;
        o.uid = static_cast<int16_t>(lh(o.moby + 0xb2));
        o.cptr = lwu(o.moby + 0x24);
        std::printf("[section] %s: its boss (class %u, the plugin's) spawned again after the death: moby %08x\n", s.name.c_str(), o.cls, o.moby);
        mod_code_boss(s.name.c_str(), static_cast<int32_t>(o.cls), o.moby, 2);
    }
}

// orig: (host) (stage 10) the current section's bosses: one that left play since the last frame (not taken out by the session: a death's
// reset, the section left) is beaten: the log, the plugin's boss callback; a placed one is never placed again in the visit
void bosses_frame(Section& s) {
    auto beaten = [&](Obj& o, const char* whose) {
        ++s.beaten_n;
        std::printf("[section] frame %llu: %s: its boss (class %u%s, moby %08x) beaten (%d in this visit); the session goes on\n",
                    static_cast<unsigned long long>(g_frame), s.name.c_str(), o.cls, whose, o.moby, s.beaten_n);
        mod_code_boss(s.name.c_str(), static_cast<int32_t>(o.cls), o.moby, 1);
        o.boss = false;
    };
    for (Obj& o : s.objs)
        if (o.boss && !alive(o)) {
            if (o.imp < 0) s.beaten.insert(o.uid);
            else s.beaten_imp.insert(o.imp);
            beaten(o, "");
        }
    for (Obj& o : s.pboss)
        if (o.moby != 0 && !alive(o)) {
            beaten(o, ", the plugin's");
            o.moby = 0;
        }
}

// orig: (host) see sections.h
void sections_gone_of(int index, std::vector<int32_t>& uids) {
    if (!g_sections_on || index <= 0 || static_cast<size_t>(index) >= g_secs.size()) return;
    const Section& s = g_secs[static_cast<size_t>(index)];
    if (s.fresh) {  // (ABI 17: it is entered fresh whatever it held: a save holds nothing of it)
        uids.clear();
        return;
    }
    std::set<int32_t> g = s.dead;
    for (const Obj& o : s.objs)
        if (o.imp < 0 && !alive(o)) g.insert(o.uid);
    uids.assign(g.begin(), g.end());
}

// orig: (host) see sections.h
void sections_gone(const std::string& name, const std::vector<int32_t>& uids) {
    const int i = sections_index(name);
    if (i <= 0) return;
    Section& s = g_secs[static_cast<size_t>(i)];
    if (s.fresh) return;  // (ABI 17)
    for (const int32_t u : uids) s.dead.insert(u);
    if (!s.spawned) return;
    for (Obj& o : s.objs)  // (placed already, frozen: the save's dead taken out)
        if (o.imp < 0 && s.dead.count(o.uid) != 0 && alive(o)) moby_retire(o.moby);
}

// orig: (host) see sections.h
int section_load(const std::string& name) {
    const int i = sections_index(name);
    if (!g_sections_on || i < 0) return 0;
    request(i);
    return 1;
}
// orig: (host) see sections.h
int section_ready(const std::string& name) {
    const int i = sections_index(name);
    if (!g_sections_on || i < 0) return -1;
    return ready(i) ? 1 : 0;
}
// orig: (host) see sections.h
int section_enter(const std::string& name, const std::string& entry, int32_t fade) {
    const int i = sections_index(name);
    if (!g_sections_on || i < 0) return 0;
    const Entry* e = entry_of(g_secs[static_cast<size_t>(i)], entry);
    if (e == nullptr) return 0;
    ask_switch(i, e, fade, "the plugin's section_enter");
    return 1;
}
// orig: (host) see sections.h
int section_unload(const std::string& name) {
    const int i = sections_index(name);
    if (!g_sections_on || i <= 0 || i == g_cur || (g_pend.on && g_pend.to == i)) return 0;
    Section& s = g_secs[static_cast<size_t>(i)];
    for (const Obj& o : s.objs)
        if (alive(o)) moby_retire(o.moby);
        else if (o.imp >= 0) s.dead_imp.insert(o.imp);
        else s.dead.insert(o.uid);
    kids_retire(s, 0);
    s.objs.clear();
    s.spawned = s.frozen = false;
    free_section(s);
    std::printf("[section] %s unloaded at frame %llu (its heap blocks given back)\n", s.name.c_str(), static_cast<unsigned long long>(g_frame));
    return 1;
}
// orig: (host) see sections.h
int section_reset(const std::string& name) {
    if (!g_sections_on) {
        std::fprintf(stderr, "[section] section_reset(%s): no session with sections\n", name.c_str());
        return 0;
    }
    const int i = sections_index(name);
    if (i < 0) {
        std::fprintf(stderr, "[section] section_reset(%s): no such section\n", name.c_str());
        return 0;
    }
    if (i == 0) {
        std::fprintf(stderr, "[section] section_reset(main): not allowed: main's objects are the session's own (a death's soft reset: the respawn policy)\n");
        return 0;
    }
    if (i == g_cur) {
        std::fprintf(stderr, "[section] section_reset(%s): not allowed on the current section (its objects are in play; leave it first)\n", name.c_str());
        return 0;
    }
    if (g_pend.on && g_pend.to == i) {
        std::fprintf(stderr, "[section] section_reset(%s): not allowed: a switch into it is pending\n", name.c_str());
        return 0;
    }
    reset_section(g_secs[static_cast<size_t>(i)], "the plugin's section_reset");
    return 1;
}
// orig: (host) see sections.h
int32_t section_policy(const std::string& name, int32_t policy) {
    const int i = sections_index(name);
    if (!g_sections_on || i < 0) return -1;
    Section& s = g_secs[static_cast<size_t>(i)];
    if (policy == SECTION_REMOVE || policy == SECTION_FREEZE || policy == SECTION_KEEP) s.policy = policy;
    return s.policy;
}

// orig: (host) see sections.h
void section_equiv_native([[maybe_unused]] uint32_t H) {
    const int level = lw(LEVEL);
    const uint32_t lo = lwu(OV(0x0015ffd8)), hi = lwu(OV(0x0015ffdc));
    const uint32_t n = (hi - lo) >> 8;
    // the level's gameplay file from the disc, decompressed into the heap (the native load's own copy at H has its pvar table
    // rewritten by pvar_move)
    char path[96];
    std::snprintf(path, sizeof path, "assets/disc/levels/level%02d/%s", level, lw(0x0015ed80) != 0 ? "gameplay_pal.bin" : "gameplay_ntsc.bin");
    std::vector<uint8_t> packed, gp;
    if (!session_memory_on() || !read_all(path, packed) ||
        !decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(packed.data()), packed.size()), gp)) {
        std::printf("[section] equivalence level %d: cannot run (%s; a session's memory: RAC1_SESSION_MEM=1)\n", level, path);
        return;
    }
    const uint32_t H2 = session_heap_alloc(static_cast<uint32_t>(gp.size()) + 0x10, "section equivalence");
    std::memcpy(ee_ram_ptr_write(H2, static_cast<uint32_t>(gp.size())), gp.data(), gp.size());
    // the native load: its records, its pvar blocks (their table's sizes), its placement -> slot map
    const uint32_t tab2 = H2 + lwu(H2 + 0x54);
    std::vector<std::vector<uint8_t>> rec(n), pv(n);
    std::vector<uint32_t> pva(n);
    for (uint32_t k = 0; k < n; ++k) {
        const uint32_t m = lo + k * 0x100;
        rec[k].resize(0x100);
        for (uint32_t i = 0; i < 0x100; ++i) rec[k][i] = static_cast<uint8_t>(lbu(m + i));
        pva[k] = lwu(m + 0x78);
    }
    const uint32_t mp = H2 + lwu(H2 + 0x44);
    const int32_t n1 = lw(mp);
    std::vector<int16_t> nmap(n1 > 0 ? static_cast<size_t>(n1) : 0);
    for (int32_t k = 0; k < n1; ++k) nmap[static_cast<size_t>(k)] = static_cast<int16_t>(lh(OV(0x001ac780) + static_cast<uint32_t>(k) * 2));
    // each native moby's pvar index (by its placement), its block's bytes
    std::vector<int32_t> pidx(n, -1);
    {
        uint32_t s1 = mp + 0x10;
        for (int32_t k = 0; k < n1; ++k) {
            const uint32_t rec_end = s1 + static_cast<uint32_t>((lw(s1) >> 2) << 2);
            const int16_t slot = nmap[static_cast<size_t>(k)];
            if (slot >= 0 && static_cast<uint32_t>(slot) < n) pidx[static_cast<size_t>(slot)] = lw(s1 + 0x58);
            s1 = rec_end;
        }
    }
    for (uint32_t k = 0; k < n; ++k) {
        if (pidx[k] < 0 || pva[k] == 0 || lw(H2 + 0x58) == 0) continue;
        const uint32_t size = lwu(tab2 + static_cast<uint32_t>(pidx[k]) * 8 + 4);
        pv[k].resize(size);
        for (uint32_t i = 0; i < size; ++i) pv[k][i] = static_cast<uint8_t>(lbu(pva[k] + i));
    }
    // the native mobys out of the grid and cleared (as gameplay_load found the array), then placed again by section_spawn_placements
    for (uint32_t k = 0; k < n; ++k) moby_grid_move(lo + k * 0x100, GRID_OUT);
    mem_fill32(lo, 0, static_cast<int32_t>(n << 8));
    // (the moby queries' stamp counter, which the mobys' ground probes count up and leave in +0x9c: as gameplay_load set it
    // before its mobys, 0; at the end it is the native load's again)
    const uint32_t stamp = lwu(OV(0x00173e50));
    sw(OV(0x00173e50), 0);
    SpawnOpts o;
    o.test = true;
    o.owner = "section equivalence";
    const SpawnResult r = section_spawn_placements(H2, o);
    const bool stamp_same = lwu(OV(0x00173e50)) == stamp;
    // the self-offset and shared-data words of each table index (rebased, not compared as they are)
    std::map<int32_t, std::set<uint32_t>> self_w, shared_w;
    if (lw(H2 + 0x5c) != 0)
        for (uint32_t a = H2 + lwu(H2 + 0x5c); lw(a) >= 0; a += 8) self_w[lw(a)].insert(lwu(a + 4));
    if (lw(H2 + 0x4c) != 0) {
        uint32_t p = H2 + lwu(H2 + 0x4c);
        const int32_t size = lw(p), cnt = lw(p + 4);
        p += 0x10 + static_cast<uint32_t>(size);
        for (int32_t k = 0; k < cnt; ++k, p += 8) shared_w[static_cast<int32_t>(lwu(p) & 0xffff)].insert(static_cast<uint32_t>(static_cast<int32_t>(lwu(p)) >> 16));
    }
    size_t rec_ok = 0, pv_ok = 0, pv_words = 0, rebased = 0, diffs = 0;
    bool map_ok = r.map.size() == nmap.size();
    for (size_t k = 0; map_ok && k < nmap.size(); ++k) {
        map_ok = r.map[k] == nmap[k];
        if (!map_ok) std::printf("[section] equivalence level %d: placement %zu: native slot %d, placed %d\n", level, k, nmap[k], r.map[k]);
    }
    int32_t shared_native_delta = 0;
    bool shared_delta_known = false;
    for (uint32_t k = 0; k < n && k < r.mobys.size(); ++k) {
        const uint32_t m = lo + k * 0x100;
        bool same = r.mobys[k].moby == m;
        for (uint32_t i = 0; i < 0x100 && same; ++i) {
            if (i >= 0x78 && i < 0x7c) continue;  // (the pvar pointer: rebased below)
            if (lbu(m + i) != rec[k][i]) {
                same = false;
                if (diffs++ < 8) std::printf("[section] equivalence level %d: moby %u +0x%02x: native %02x, placed %02x\n", level, k, i, rec[k][i], lbu(m + i));
            }
        }
        const uint32_t mine = lwu(m + 0x78);
        if (same && ((mine == 0) != (pva[k] == 0))) same = false;
        rec_ok += same ? 1 : 0;
        if (pv[k].empty()) {
            pv_ok += same ? 1 : 0;
            continue;
        }
        bool pv_same = true;
        const auto& sw_ = self_w[pidx[k]];
        const auto& shw = shared_w[pidx[k]];
        for (uint32_t i = 0; i + 4 <= pv[k].size(); i += 4) {
            uint32_t nw;
            std::memcpy(&nw, pv[k].data() + i, 4);
            const uint32_t mw = lwu(mine + i);
            ++pv_words;
            if (sw_.count(i) != 0) {  // (an offset into its own block)
                if (nw - pva[k] == mw - mine) {
                    ++rebased;
                    continue;
                }
            } else if (shw.count(i) != 0 && r.shared != 0) {  // (a pointer into the shared data: the same distance from its block)
                const int32_t d = static_cast<int32_t>(mw - r.shared);
                if (!shared_delta_known) {
                    shared_native_delta = static_cast<int32_t>(nw) - d;
                    shared_delta_known = true;
                }
                if (static_cast<int32_t>(nw) - d == shared_native_delta) {
                    ++rebased;
                    continue;
                }
            } else if (nw == mw) {
                continue;
            }
            pv_same = false;
            if (diffs++ < 8) std::printf("[section] equivalence level %d: moby %u's pvars +0x%02x: native %08x, placed %08x\n", level, k, i, nw, mw);
        }
        pv_ok += pv_same && same ? 1 : 0;
    }
    const bool ok = r.mobys.size() == n && rec_ok == n && pv_ok == n && map_ok && stamp_same;
    std::printf("[section] equivalence level %d: %zu mobys placed by section_spawn_placements, the native load %u; records equal %zu, pvar blocks equal %zu "
                "(%zu words, %zu rebased: own offsets and the shared data's pointers), the placements' slots %s, the query stamp %s: %s\n",
                level, r.mobys.size(), n, rec_ok, pv_ok, pv_words, rebased, map_ok ? "equal" : "DIFFERENT", stamp_same ? "equal" : "DIFFERENT", ok ? "ok" : "DIFFERENT");
}

}  // namespace rac1
