// See session_runtime.h.
#include <game/enhance/session_runtime.h>

#include <game/enhance/class_profiles.h>
#include <game/enhance/hero_modes.h>
#include <game/enhance/leveldata.h>
#include <game/enhance/sections.h>
#include <game/enhance/session.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/globals.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/save_menus.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <host/level_slots.h>
#include <host/mod_code.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/mods.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>

namespace rac1 {

bool g_session_rt = false;

namespace {

constexpr uint32_t P = 0x0013f350u;          // the player block (the hero's state)
constexpr uint32_t LEVEL = 0x0015ed84u;      // the game's level number (a level mod's: its base's)
constexpr uint32_t GRID_OUT = 0x80807f7fu;   // a moby's grid rectangle outside the grid (moby_retire's, moby_init's)
constexpr uint32_t PC_ITEM = 0x201;          // our item in save<N>.pc (the level mods' items are 0x200, the plugins' 0x300)
constexpr uint32_t PC_VERSION = 2;           // (1: stage 5's, one section; 2: the sections, each its objects gone, the current one's checkpoint)

struct Checkpoint {
    std::string name;
    float pos[3] = {0, 0, 0};
    float yaw = 0;
    int section = 0;  // (the section it is in: sections.h, 0 main)
};

// a placed object as the arrival left it (the soft resets' source), and the game's spawn rule of its placement
struct Placed {
    uint32_t moby = 0;
    int32_t uid = -1;
    uint32_t flags = 0, wl = 0;  // (its placement record's flags and +4 byte: gameplay_load's spawn rule)
    uint8_t rec[0x100] = {};
    std::vector<uint8_t> pv;
    bool left_alive = false;     // (main left by a portal: it was in play then)
    uint32_t flags34 = 0;        // (frozen: its +0x34 flags before)
};

struct Rt {
    std::string build, id;
    std::vector<Checkpoint> checkpoints;  // level.txt's (session.txt), the first "start" (where Ratchet starts)
    Checkpoint cur;
    int32_t policy = SESSION_RESPAWN_RESET;
    bool loaded = false;                  // the arrival's snapshot taken
    std::vector<Placed> placed;
    Placed hero;
    bool hero_ok = false;
    uint32_t respawns = 0;
    // a loaded save's item for this level: its checkpoint taken at the arrival, Ratchet put there once it is over
    bool place_pending = false;
    int place_frames = 0;
    uint64_t start_frame = 0;
    // level.txt's moby lines' slots (session.txt's bind lines): uid, class, slot, value
    struct Bind {
        int32_t uid = 0;
        uint32_t cls = 0;
        std::string slot, value;
    };
    std::vector<Bind> binds;
};
Rt g_rt;

// a loaded save's session item (its level's session not entered yet; kept to be written back when its mod is off)
struct Loaded {
    bool valid = false;
    std::string id;
    Checkpoint cp;
    std::string cp_section;     // (the checkpoint's section by name: "main" or a section's)
    uint32_t respawns = 0;
    std::vector<int32_t> gone;  // (main's placed objects gone for good at the save)
    std::vector<std::pair<std::string, std::vector<int32_t>>> section_gone;  // (each other section's objects gone at the save)
    std::vector<std::pair<std::string, std::vector<uint8_t>>> plugins;
    std::vector<uint8_t> raw;
};
Loaded g_loaded;

bool trace() {
    static const bool t = std::getenv("RAC1_SESSION_TRACE") != nullptr && std::atoi(std::getenv("RAC1_SESSION_TRACE")) != 0;
    return t;
}

// the pvars' size of a placed object: up to the next placed object's pvars (gameplay_load moves them one after the other),
// at most 4 KB
uint32_t pvars_size(uint32_t pv, uint32_t lo, uint32_t hi) {
    uint32_t next = pv + 0x1000;
    for (uint32_t m = lo; m < hi; m += 0x100) {
        const uint32_t p = lwu(m + 0x78);
        if (p > pv && p < next) next = p;
    }
    return next - pv;
}

// gameplay_load's spawn rule (level_load.cpp, the mobys) as a session's reset keeps it: a placement with these flags not
// placed again by the game's restart because the level's records keep it killed for good (the checkpoint's state byte, the
// missions' rules, the level's own bits: a crate broken, a pickup taken). The game's restart also keeps an enemy killed
// "for the visit" dead (flags 8: the visit's bits 0x1ba4d0); a session brings it back (DECISIONS 4: a battle's enemies
// come back after a death)
bool stays_dead(const Placed& p) {
    if (p.flags == 0 || p.uid < 0) return false;
    const uint32_t uid = static_cast<uint32_t>(p.uid);
    const auto bit = [uid](uint32_t base) { return (lw(base + (uid >> 5) * 4) >> (uid & 0x1f)) & 1; };
    const uint32_t lvbits = 0x0014c190u + (lwu(LEVEL) << 8);
    if (lbu(OV(0x001bb684) + uid) != 0) return true;
    if ((p.flags & 3) != 0) {
        if (lbu(OV(0x0015fc88) + p.wl) == 0xff) return (p.flags & 2) == 0;
        return (p.flags & 1) == 0;
    }
    if ((p.flags & 8) != 0) return false;
    if ((p.flags & 0xc) == 4) return bit(lvbits) != 0;
    return false;
}

// a moby's record (and pvars) put back as `s` has them: out of the grid first, its update chain's link and animation list
// kept (the frame's bookkeeping), then placed in the grid again where it stands
void restore(const Placed& s, bool keep_anim) {
    const uint32_t m = s.moby;
    moby_grid_move(m, GRID_OUT);
    const uint32_t chain = lwu(m + 0x28), anim = lwu(m + 0x60);
    for (uint32_t i = 0; i < 0x100; i += 4) {
        uint32_t w;
        std::memcpy(&w, s.rec + i, 4);
        sw(m + i, w);
    }
    sw(m + 0x28, chain);
    sw(m + 0x60, keep_anim ? anim : 0u);
    sw(m + 0xa0, GRID_OUT);
    const uint32_t pv = lwu(m + 0x78);
    for (size_t i = 0; i + 4 <= s.pv.size() && pv != 0; i += 4) {
        uint32_t w;
        std::memcpy(&w, s.pv.data() + i, 4);
        sw(pv + static_cast<uint32_t>(i), w);
    }
}

void take(Placed& p, uint32_t m, uint32_t lo, uint32_t hi) {
    p.moby = m;
    for (uint32_t i = 0; i < 0x100; ++i) p.rec[i] = lbu(m + i);
    const uint32_t pv = lwu(m + 0x78);
    if (pv != 0 && pv < ee_game_ram_end()) {
        p.pv.resize(pvars_size(pv, lo, hi));
        for (uint32_t i = 0; i < p.pv.size(); ++i) p.pv[i] = lbu(pv + i);
    }
}

Checkpoint start_checkpoint() {
    for (const Checkpoint& c : g_rt.checkpoints)
        if (c.name == "start") return c;
    return g_rt.checkpoints.empty() ? Checkpoint{} : g_rt.checkpoints.front();
}

// Ratchet put at `c` (the game's hero_place: his position and his moby's, his matrices, his state 0, the camera behind him)
void hero_put(const Checkpoint& c) {
    const uint32_t buf = ee_stack_alloc(0x20);
    for (uint32_t k = 0; k < 3; ++k) swc1(buf + 4 * k, c.pos[k]);
    swc1(buf + 0xc, 0.0f);
    sw(buf + 0x10, 0);
    sw(buf + 0x14, 0);
    swc1(buf + 0x18, c.yaw);
    sw(buf + 0x1c, 0);
    hero_place(buf, buf + 0x10, 0, 1);
    ee_stack_free(0x20);
}

void u32_put(std::vector<uint8_t>& d, uint32_t v) {
    for (int i = 0; i < 4; ++i) d.push_back(static_cast<uint8_t>(v >> (8 * i)));
}
void f32_put(std::vector<uint8_t>& d, float f) {
    uint32_t v;
    std::memcpy(&v, &f, 4);
    u32_put(d, v);
}
void str_put(std::vector<uint8_t>& d, const std::string& s) {
    u32_put(d, static_cast<uint32_t>(s.size()));
    d.insert(d.end(), s.begin(), s.end());
}

}  // namespace

// orig: (host) see session_runtime.h
void session_rt_level(const std::string& build_dir, const std::string& level_id) {
    if (g_session_rt) {
        std::printf("[session] the session \"%s\" left (%u respawns)\n", g_rt.id.c_str(), g_rt.respawns);
        ld_base_reset();  // (the base's view's own paths and groups in the heap: given back)
    }
    g_session_rt = false;
    g_rt = Rt{};
    sections_level("");  // (the sections of the session before dropped: sections.h)
    hero_modes_level(!build_dir.empty(), false, level_slots_on() && level_slot() >= 0 ? level_slot_base(level_slot()) : 0);  // (hero_modes.h)
    if (build_dir.empty()) return;
    hero_modes_build_file(build_dir);  // (a format 1 / 2 level's `hero generic`)
    std::ifstream in(build_dir + "/session.txt");
    if (!in) return;
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind;
        if (!(s >> kind)) continue;
        if (kind == "checkpoint") {
            Checkpoint c;
            if (s >> c.name >> c.pos[0] >> c.pos[1] >> c.pos[2] >> c.yaw) g_rt.checkpoints.push_back(c);
        } else if (kind == "bind") {
            Rt::Bind b;
            if (s >> b.uid >> b.cls >> b.slot >> b.value) g_rt.binds.push_back(b);
        } else if (kind == "respawn") {
            std::string p;
            s >> p;
            g_rt.policy = p == "keep" ? SESSION_RESPAWN_KEEP : SESSION_RESPAWN_RESET;
        }
    }
    if (!session_memory_begin()) {
        std::fprintf(stderr, "[session] %s: no session memory: the level runs as a format 2 level (deaths load it again)\n", build_dir.c_str());
        return;
    }
    g_session_rt = true;
    hero_modes_level(true, true, 0);  // (a session: every empty slot of the hero's, its hero contexts)
    g_rt.build = build_dir;
    g_rt.id = level_id;
    g_rt.cur = start_checkpoint();
    std::printf("[session] the session \"%s\": %zu checkpoints, respawn %s; no level load until it is left\n", level_id.c_str(), g_rt.checkpoints.size(),
                g_rt.policy == SESSION_RESPAWN_KEEP ? "keep" : "reset");
    sections_level(build_dir);  // (its sections, when it has some: sections.h)
}

// orig: (host) see session_runtime.h
void session_rt_checkpoint_add(const std::string& name, int section, const float pos[3], float yaw) {
    Checkpoint c;
    c.name = name;
    std::memcpy(c.pos, pos, 12);
    c.yaw = yaw;
    c.section = section;
    g_rt.checkpoints.push_back(c);
}

// orig: (host) see session_runtime.h
void session_rt_checkpoint_enter(const std::string& name, int section, const float pos[3], float yaw) {
    if (!g_session_rt) return;
    Checkpoint c;
    c.name = name;
    std::memcpy(c.pos, pos, 12);
    c.yaw = yaw;
    c.section = section;
    g_rt.cur = c;
}

// orig: (host) see session_runtime.h
int session_rt_checkpoint_section() { return g_session_rt ? g_rt.cur.section : 0; }

// orig: (host) see session_runtime.h
void session_rt_hero_put(const float pos[3], float yaw) {
    Checkpoint c;
    std::memcpy(c.pos, pos, 12);
    c.yaw = yaw;
    hero_put(c);
}

// orig: (host) see session_runtime.h
bool session_bind(uint32_t m, int level, uint32_t cls, const std::string& slot, const std::string& value) {
    const Profile* prof = profile_find(level, cls);
    uint32_t off = 0;
    std::string kind;
    if (m == 0 || lwu(m + 0x78) == 0 || prof == nullptr || !profile_slot(prof, slot, &off, &kind)) {
        std::fprintf(stderr, "[session] level.txt's object of class %d:%u: no slot %s to set\n", level, cls, slot.c_str());
        return false;
    }
    const uint32_t pv = lwu(m + 0x78);
    if (kind == "path" || kind == "area") {
        std::vector<float> xyz;
        uint32_t mem = 0;
        if (!mod_code_path_points(value, xyz) || xyz.empty()) {
            std::fprintf(stderr, "[session] level.txt's object of class %d:%u: slot %s: no path \"%s\"\n", level, cls, slot.c_str(), value.c_str());
            return false;
        }
        const int np = static_cast<int>(std::min<size_t>(xyz.size() / 3, 64));
        const int32_t s = ld_path_reserve(level, np, &mem);
        if (s < 0) return false;
        float pts[64][3];
        for (int k = 0; k < np; ++k)
            for (int c = 0; c < 3; ++c) pts[k][c] = xyz[static_cast<size_t>(3 * k + c)];
        ld_path_set(mem, pts, np);
        sw(pv + off, static_cast<uint32_t>(s));
    } else {
        sw(pv + off, value == "none" ? 0xffffffffu : static_cast<uint32_t>(std::strtol(value.c_str(), nullptr, 0)));
    }
    return true;
}

// orig: (host) see session_runtime.h
size_t session_rt_main_leave(int32_t policy) {
    if (!g_session_rt) return 0;
    size_t n = 0;
    for (Placed& p : g_rt.placed) {
        p.left_alive = static_cast<int8_t>(lbu(p.moby + 0x20)) >= 0;
        if (!p.left_alive) continue;
        if (policy == SECTION_KEEP) {
            ++n;
            continue;
        }
        if (policy == SECTION_FREEZE) {
            p.flags34 = lhu(p.moby + 0x34);
            sh(p.moby + 0x34, p.flags34 | 2u);  // (not queued: neither updated nor drawn)
            moby_grid_move(p.moby, GRID_OUT);
        } else {
            moby_retire(p.moby);
        }
        ++n;
    }
    if (trace()) std::printf("[session] main left: %zu placed objects %s\n", n, policy == SECTION_FREEZE ? "frozen" : policy == SECTION_KEEP ? "kept" : "removed");
    return n;
}

// orig: (host) see session_runtime.h
size_t session_rt_main_enter(int32_t policy) {
    if (!g_session_rt) return 0;
    size_t n = 0;
    for (Placed& p : g_rt.placed) {
        if (!p.left_alive) continue;
        p.left_alive = false;
        if (policy == SECTION_FREEZE) {
            if (static_cast<int8_t>(lbu(p.moby + 0x20)) < 0) continue;
            sh(p.moby + 0x34, (lhu(p.moby + 0x34) & ~2u) | (p.flags34 & 2u));
            moby_update_transform(p.moby);
        } else if (policy == SECTION_REMOVE && static_cast<int8_t>(lbu(p.moby + 0x20)) < 0 && !stays_dead(p)) {
            restore(p, false);  // (back as the arrival placed it: it was there when main was left)
            moby_update_transform(p.moby);
        }
        n += static_cast<int8_t>(lbu(p.moby + 0x20)) >= 0 ? 1 : 0;
    }
    return n;
}

// orig: (host) see session_runtime.h
void session_rt_loaded() {
    if (!g_session_rt) return;
    const uint32_t lo = lwu(OV(0x0015ffd8)), hi = lwu(OV(0x0015ffdc));
    // the placements' spawn rules (the gameplay file is still where gameplay_load decompressed it): by uid
    std::map<int32_t, std::pair<uint32_t, uint32_t>> rule;
    {
        const uint32_t H = lwu(OV(0x00173e04));
        const uint32_t mp = H + lwu(H + 0x44);
        const int32_t n1 = lw(mp);
        uint32_t s1 = mp + 0x10;
        for (int32_t k = 0; k < n1 && k < 0x800; ++k) {
            const int32_t sz = lw(s1);
            if (sz <= 0) break;
            rule[lw(s1 + 0xc)] = {lwu(s1 + 8), lwu(s1 + 4)};
            s1 += static_cast<uint32_t>((sz >> 2) << 2);
        }
    }
    // level.txt's moby lines' slots (their profile's): a path's points into the base's view (the session heap), a word
    size_t bound = 0;
    for (const Rt::Bind& b : g_rt.binds) {
        uint32_t m = 0;
        for (uint32_t q = lo; q < hi && m == 0; q += 0x100)
            if (lh(q + 0xb2) == b.uid && lhu(q + 0xa6) == b.cls) m = q;
        if (session_bind(m, lw(LEVEL), b.cls, b.slot, b.value)) ++bound;
    }
    if (!g_rt.binds.empty()) std::printf("[session] level.txt's objects: %zu of %zu slots set (their profiles')\n", bound, g_rt.binds.size());
    g_rt.placed.clear();
    g_rt.hero_ok = false;
    for (uint32_t m = lo; m < hi; m += 0x100) {
        if (lhu(m + 0xa6) == 0 && !g_rt.hero_ok) {  // (the hero: the first moby of class 0, as hero_init takes it)
            take(g_rt.hero, m, lo, hi);
            g_rt.hero_ok = true;
            continue;
        }
        if (static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;  // (out of play at the arrival: a spawn pool's, the plugin's)
        Placed p;
        take(p, m, lo, hi);
        p.uid = lh(m + 0xb2);
        const auto r = rule.find(p.uid);
        if (r != rule.end()) p.flags = r->second.first, p.wl = r->second.second;
        g_rt.placed.push_back(std::move(p));
    }
    g_rt.loaded = true;
    g_rt.respawns = 0;
    g_rt.cur = start_checkpoint();
    if (g_rt.cur.name.empty() && g_rt.hero_ok) {  // (no checkpoint line: where the level starts Ratchet)
        g_rt.cur.name = "start";
        std::memcpy(g_rt.cur.pos, g_rt.hero.rec + 0x10, 12);
        std::memcpy(&g_rt.cur.yaw, g_rt.hero.rec + 0x48, 4);
    }
    sections_arrival();  // (the session's sections: main's state taken, its classes of other levels placed, the others streamed in)
    if (const int s = sections_start(); s > 0) {  // (level.txt's start line: the session starts in that section, once the landing is over)
        float pos[3] = {0, 0, 0}, yaw = 0;
        std::string name;
        if (sections_start_entry(pos, &yaw, &name)) {
            g_rt.cur = Checkpoint{};
            g_rt.cur.name = name;
            std::memcpy(g_rt.cur.pos, pos, 12);
            g_rt.cur.yaw = yaw;
            g_rt.cur.section = s;
            g_rt.place_pending = true;
            g_rt.place_frames = 0;
        }
    }
    if (g_loaded.valid && g_loaded.id == g_rt.id) {  // (a save made in the session: back at its checkpoint)
        g_rt.cur = g_loaded.cp;
        g_rt.cur.section = sections_index(g_loaded.cp_section.empty() ? "main" : g_loaded.cp_section);
        if (g_rt.cur.section < 0) g_rt.cur.section = 0;
        for (const auto& [name, uids] : g_loaded.section_gone) sections_gone(name, uids);
        g_rt.respawns = g_loaded.respawns;
        g_rt.place_pending = true;
        g_rt.place_frames = 0;
        size_t gone = 0;
        for (const int32_t uid : g_loaded.gone)
            for (Placed& p : g_rt.placed)
                if (p.uid == uid && static_cast<int8_t>(lbu(p.moby + 0x20)) >= 0) {
                    moby_retire(p.moby);
                    ++gone;
                }
        for (const auto& [folder, bytes] : g_loaded.plugins) mod_code_session_load(folder, bytes.data(), static_cast<uint32_t>(bytes.size()));
        std::printf("[session] the save's session: checkpoint \"%s\" (%.2f %.2f %.2f), %u respawns, %zu placed objects gone (%zu taken out now), %zu "
                    "plugins' bytes\n",
                    g_rt.cur.name.c_str(), g_rt.cur.pos[0], g_rt.cur.pos[1], g_rt.cur.pos[2], g_rt.respawns, g_loaded.gone.size(), gone, g_loaded.plugins.size());
        g_loaded = Loaded{};
    }
    std::printf("[session] the arrival: %zu placed objects kept for the soft resets, the hero's record %s; checkpoint \"%s\" (%.2f %.2f %.2f)\n",
                g_rt.placed.size(), g_rt.hero_ok ? "kept" : "missing", g_rt.cur.name.c_str(), g_rt.cur.pos[0], g_rt.cur.pos[1], g_rt.cur.pos[2]);
}

// orig: (host) see session_runtime.h
void session_respawn() {
    ++g_rt.respawns;
    hero_modes_respawn_begin();  // (in Clank's or Giant Clank's form: Ratchet's own moby the hero's again first: hero_modes.h)
    // (sections: the checkpoint's section made the current one first, at once, no fade: sections.h)
    sections_respawn_prepare(g_rt.cur.section);
    // the hero: his moby as the arrival placed it, at the checkpoint, then as gameplay_load's end starts him (the player block
    // cleared, dropped onto the floor there, the camera behind him)
    uint32_t hm = lwu(P + 0x2080);
    if (g_rt.hero_ok) {
        if (hm == 0) hm = g_rt.hero.moby;
        Placed h = g_rt.hero;
        h.moby = hm;
        std::memcpy(h.rec + 0x10, g_rt.cur.pos, 12);
        std::memcpy(h.rec + 0x48, &g_rt.cur.yaw, 4);
        restore(h, true);
    } else if (hm != 0) {
        for (uint32_t k = 0; k < 3; ++k) swc1(hm + 0x10 + 4 * k, g_rt.cur.pos[k]);
        swc1(hm + 0x48, g_rt.cur.yaw);
    }
    sub_001f83d0();
    hero_init();
    hero_level_reset();
    mem_fill32(OV(0x00166c80), 0, 0x3a0);
    camera_init();
    camera_update();
    sub_00266d48();
    sub_001fb028();
    if (const uint32_t ship = lwu(0x0013e030); ship != 0 && lwu(P + 0x2080) != 0) sd(ship + 0x38, ld(lwu(P + 0x2080) + 0x38));
    // the placed objects, by the policy (the current section's: main's here, another's in sections.cpp)
    size_t back = 0, kept_dead = 0;
    const int32_t policy = sections_respawn_policy(g_rt.policy);
    if (sections_current() > 0) {
        sections_respawn_objects(policy, &back, &kept_dead);
    } else if (policy == SESSION_RESPAWN_RESET) {
        sections_respawn_objects(policy, &back, &kept_dead);  // (main's classes of other levels placed, when it has some)
        for (const Placed& p : g_rt.placed) {
            if (static_cast<int8_t>(lbu(p.moby + 0x20)) >= 0) continue;  // (alive: as it is)
            if (stays_dead(p)) {
                ++kept_dead;
                continue;
            }
            restore(p, false);
            moby_update_transform(p.moby);
            ++back;
        }
    }
    if (hm != 0) moby_update_transform(hm);
    std::printf("[session] respawn %u at checkpoint \"%s\" (%.2f %.2f %.2f)%s: no level load; placed objects %s: %zu back, %zu killed for good\n", g_rt.respawns,
                g_rt.cur.name.c_str(), g_rt.cur.pos[0], g_rt.cur.pos[1], g_rt.cur.pos[2],
                sections_any() ? (" in section " + sections_name(sections_current())).c_str() : "", policy == SESSION_RESPAWN_KEEP ? "kept" : "reset", back,
                kept_dead);
    sections_hero_respawned();  // (stage 9: the section's hero mode again, at the next frame of play)
    mod_code_respawn(g_rt.respawns, g_rt.cur.name.c_str(), g_rt.cur.pos, g_rt.cur.yaw);
}

// orig: (host) see session_runtime.h
void session_rt_frame(uint64_t frame) {
    if (!g_session_rt || !g_rt.loaded) return;
    if (g_rt.start_frame == 0) g_rt.start_frame = frame;
    sections_frame(frame);  // (the sections' streaming, portals, fades: sections.h)
    if (!g_rt.place_pending) return;
    // a loaded save: once the arrival is over (the landing's scene done: game mode 0 for a few frames, the hero standing)
    if (lw(OV(0x0015f5c4)) != 0 || lwu(P + 0x2080) == 0 || lbu(P + 0x20b1) != 0) {
        g_rt.place_frames = 0;
        return;
    }
    if (++g_rt.place_frames < 10) return;
    if (g_rt.cur.section != sections_current() && !sections_switch_now(g_rt.cur.section)) return;  // (its section streamed in first)
    g_rt.place_pending = false;
    hero_put(g_rt.cur);
    std::printf("[session] Ratchet at checkpoint \"%s\" (%.2f %.2f %.2f)%s once the arrival is over\n", g_rt.cur.name.c_str(), g_rt.cur.pos[0], g_rt.cur.pos[1],
                g_rt.cur.pos[2], sections_any() ? (" in section " + sections_name(g_rt.cur.section)).c_str() : "");
}

// orig: (host) see session_runtime.h
void session_rt_test_frame(uint64_t frame) {
    // testing: RAC1_SESSION_SAVE_AT=<frame>:<slot> saves the game into the card's slot (as the pause menu's Save page:
    // save_game_image, the card's operation 0x13; save<slot>.pc with it), RAC1_SESSION_LOAD_AT=<frame>:<slot> loads it (as
    // the Load page: the card's operation 0xd, then the level left for the save's). A card: RAC1_MEMCARD_DIR
    struct Op {
        uint64_t at = 0;
        int32_t slot = -1;
        int step = 0;  // 0 waiting, 1 asked, 2 done
    };
    static Op save_op, load_op;
    static bool read = false;
    if (!read) {
        read = true;
        auto parse = [](const char* e, Op& o) {
            unsigned long long f = 0;
            int sl = -1;
            if (e != nullptr && std::sscanf(e, "%llu:%d", &f, &sl) == 2 && sl >= 0 && sl < 5) o.at = f, o.slot = sl;
        };
        parse(std::getenv("RAC1_SESSION_SAVE_AT"), save_op);
        parse(std::getenv("RAC1_SESSION_LOAD_AT"), load_op);
    }
    if (save_op.slot < 0 && load_op.slot < 0) return;
    constexpr uint32_t S = 0x0013d290u;
    const bool ready = lw(S + 0xd4) < 3 && lw(S + 0xdc) < 0;
    static uint32_t buf = 0;
    // (as the pages do when they open: the card's slots' summaries read first, operation 0x15: a card just seen is checked)
    auto summaries = [&](Op& o) {
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 0x15);
        o.step = 1;
    };
    if (save_op.slot >= 0 && save_op.step == 0 && frame >= save_op.at && lw(OV(0x0015f5c4)) == 0 && ready) {
        summaries(save_op);
    } else if (save_op.step == 1 && ready) {
        const uint32_t size = save_list_size(OV(0x001845c0)) + save_list_size(OV(0x001848c0)) * 0x14 + 8;
        if (buf == 0) buf = session_memory_on() ? session_heap_alloc(size, "save test") : 0;
        if (buf == 0) {
            std::fprintf(stderr, "[session] save test: no room for the save's image\n");
            save_op.slot = -1;
            return;
        }
        // (a run started by RAC1_GOTO never read the card, so the port's record still says "unchecked" (+0xac -1, set when
        // the card was seen) and the game refuses to save over it; a game loaded from the title read it: as that one)
        if (lw(S + lw(S + 0xc4) * 0xb8 + 0xac) < 0) sw(S + lw(S + 0xc4) * 0xb8 + 0xac, 0);
        save_game_image(buf, save_op.slot);
        save_op.step = 2;
        std::printf("[session] save test: the game saved into slot %d at frame %llu (%u bytes)\n", save_op.slot, static_cast<unsigned long long>(frame), size);
    } else if (save_op.step == 2 && ready) {
        save_op.step = 3;
        std::printf("[session] save test: the card's save done (error %d)\n", lw(S + 0xe4));
    }
    if (load_op.slot >= 0 && load_op.step == 0 && frame >= load_op.at && lw(OV(0x0015f5c4)) == 0 && ready) {
        summaries(load_op);
    } else if (load_op.step == 1 && ready) {
        sw(S + 0xc0, 0);
        sw(S + 0x14, static_cast<uint32_t>(load_op.slot));
        sw(S + 0xe0, 0);
        sw(S + 0xdc, 0xd);
        load_op.step = 2;
        std::printf("[session] load test: slot %d asked at frame %llu\n", load_op.slot, static_cast<unsigned long long>(frame));
    } else if (load_op.step == 2 && ready) {
        load_op.step = 3;
        if (lw(S + 0xe4) != 0) {
            std::printf("[session] load test: the card's error %d\n", lw(S + 0xe4));
            return;
        }
        // (the Load page's end, save_menus.cpp's load_slots_update: the volumes from the save's settings, the level left)
        const int32_t fx = lw(0x0015edf0);
        sw(S + 0xf4, 1);
        constexpr uint32_t T = 0x0013e550;
        sw(T + 0x4c, lwu(0x0015edec));
        const int32_t fx8 = fx * 8 / 10, fx7 = (fx * 8 - fx) / 10;
        sw(T + 0x48, static_cast<uint32_t>(fx8));
        sw(T + 0x50, static_cast<uint32_t>(fx8));
        sw(T + 0x54, static_cast<uint32_t>(fx7));
        sw(T + 0x5c, static_cast<uint32_t>(fx));
        sw(T + 0x58, static_cast<uint32_t>(fx7));
        level_exit_to(lw(LEVEL));
        sh(0x0013e05a, 0);
        std::printf("[session] load test: loaded, the level left for level %d\n", lw(LEVEL));
    }
}

// orig: (host) see session_runtime.h
void session_checkpoint_game(const float pos[3], float yaw) {
    if (!g_session_rt) return;
    Checkpoint c;
    c.name = "game";
    std::memcpy(c.pos, pos, 12);
    c.yaw = yaw;
    c.section = sections_current() < 0 ? 0 : sections_current();
    g_rt.cur = c;
    if (trace()) std::printf("[session] the game's checkpoint at %.2f %.2f %.2f\n", pos[0], pos[1], pos[2]);
}

// orig: (host) see session_runtime.h
bool session_checkpoint_set(const std::string& name) {
    if (!g_session_rt) return false;
    for (const Checkpoint& c : g_rt.checkpoints)
        if (c.name == name) {
            g_rt.cur = c;
            if (trace()) std::printf("[session] checkpoint \"%s\"\n", name.c_str());
            return true;
        }
    return false;
}

// orig: (host) see session_runtime.h
bool session_checkpoint_at(const float pos[3], float yaw, const std::string& name) {
    if (!g_session_rt || !std::isfinite(pos[0]) || !std::isfinite(pos[1]) || !std::isfinite(pos[2]) || !std::isfinite(yaw)) return false;
    Checkpoint c;
    c.name = name.empty() ? "plugin" : name.substr(0, 31);
    std::memcpy(c.pos, pos, 12);
    c.yaw = yaw;
    c.section = sections_current() < 0 ? 0 : sections_current();
    g_rt.cur = c;
    return true;
}

// orig: (host) see session_runtime.h
std::string session_checkpoint(float pos[3], float* yaw) {
    if (!g_session_rt) return "";
    if (pos != nullptr) std::memcpy(pos, g_rt.cur.pos, 12);
    if (yaw != nullptr) *yaw = g_rt.cur.yaw;
    return g_rt.cur.name;
}

// orig: (host) see session_runtime.h
int32_t session_respawn_policy(int32_t policy) {
    if (!g_session_rt) return -1;
    if (policy == SESSION_RESPAWN_RESET || policy == SESSION_RESPAWN_KEEP) g_rt.policy = policy;
    return g_rt.policy;
}

// orig: (host) see session_runtime.h
uint32_t session_respawns() { return g_session_rt ? g_rt.respawns : 0; }

// orig: (host) see session_runtime.h
uint32_t session_rt_pc_items(std::vector<uint8_t>& d) {
    std::vector<uint8_t> item;
    if (g_session_rt && g_rt.loaded) {
        u32_put(item, PC_VERSION);
        str_put(item, g_rt.id);
        // its sections (main first): each the checkpoint when it is the checkpoint's section ("" else), the respawns, its placed
        // objects gone (main's: for good; another's: killed, not to come back until a death there)
        const int ns = sections_count() > 0 ? sections_count() : 1;
        u32_put(item, static_cast<uint32_t>(ns));
        std::vector<int32_t> gone;
        for (const Placed& p : g_rt.placed)
            if (static_cast<int8_t>(lbu(p.moby + 0x20)) < 0 && stays_dead(p)) gone.push_back(p.uid);
        size_t gone_all = gone.size();
        for (int s = 0; s < ns; ++s) {
            str_put(item, s == 0 ? std::string("main") : sections_name(s));
            const bool here = g_rt.cur.section == s;
            str_put(item, here ? g_rt.cur.name : std::string());
            for (const float f : g_rt.cur.pos) f32_put(item, here ? f : 0.0f);
            f32_put(item, here ? g_rt.cur.yaw : 0.0f);
            u32_put(item, g_rt.respawns);
            if (s > 0) {
                gone.clear();
                sections_gone_of(s, gone);
                gone_all += gone.size();
            }
            u32_put(item, static_cast<uint32_t>(gone.size()));
            for (const int32_t u : gone) u32_put(item, static_cast<uint32_t>(u));
        }
        // the plugins' session bytes (session_save, ABI 9)
        std::vector<std::pair<std::string, std::vector<uint8_t>>> blobs;
        mod_code_session_save(blobs);
        u32_put(item, static_cast<uint32_t>(blobs.size()));
        for (const auto& [folder, bytes] : blobs) {
            str_put(item, folder);
            u32_put(item, static_cast<uint32_t>(bytes.size()));
            item.insert(item.end(), bytes.begin(), bytes.end());
        }
        std::printf("[session] the save's session item: \"%s\", %d sections, checkpoint \"%s\" in %s, %zu placed objects gone, %zu plugins' bytes (%zu bytes)\n",
                    g_rt.id.c_str(), ns, g_rt.cur.name.c_str(), g_rt.cur.section == 0 ? "main" : sections_name(g_rt.cur.section).c_str(), gone_all, blobs.size(),
                    item.size());
    } else if (g_loaded.valid && mods_level_by_id(g_loaded.id) == nullptr) {
        item = g_loaded.raw;  // (a session whose mod is off or missing: written back as it was read)
    }
    if (item.empty() || item.size() > 0xffff) return 0;
    d.push_back(static_cast<uint8_t>(PC_ITEM));
    d.push_back(static_cast<uint8_t>(PC_ITEM >> 8));
    d.push_back(static_cast<uint8_t>(item.size()));
    d.push_back(static_cast<uint8_t>(item.size() >> 8));
    d.insert(d.end(), item.begin(), item.end());
    return 1;
}

// orig: (host) see session_runtime.h
void session_rt_pc_reset() { g_loaded = Loaded{}; }

// orig: (host) see session_runtime.h
bool session_rt_pc_item(uint32_t id, const uint8_t* p, uint32_t size) {
    if (id != PC_ITEM) return false;
    Loaded L;
    L.raw.assign(p, p + size);
    uint32_t at = 0;
    bool ok = true;
    auto u32 = [&](uint32_t& v) {
        ok = ok && at + 4 <= size;
        if (ok) v = static_cast<uint32_t>(p[at] | p[at + 1] << 8 | p[at + 2] << 16 | static_cast<uint32_t>(p[at + 3]) << 24);
        at += 4;
        return ok;
    };
    auto f32 = [&](float& f) {
        uint32_t v = 0;
        if (u32(v)) std::memcpy(&f, &v, 4);
        return ok;
    };
    auto str = [&](std::string& s) {
        uint32_t n = 0;
        if (!u32(n) || n > 256 || at + n > size) return ok = false;
        s.assign(reinterpret_cast<const char*>(p + at), n);
        at += n;
        return true;
    };
    uint32_t version = 0, sections = 0;
    if (!u32(version) || (version != 1 && version != PC_VERSION) || !str(L.id) || !u32(sections) || sections == 0 || sections > 32)
        return true;  // (another version's: not ours to read)
    for (uint32_t s = 0; s < sections && ok; ++s) {
        std::string name;
        Checkpoint c;
        uint32_t ngone = 0;
        str(name);
        str(c.name);
        f32(c.pos[0]), f32(c.pos[1]), f32(c.pos[2]), f32(c.yaw);
        u32(L.respawns);
        if (!u32(ngone) || ngone > 0x800) break;
        std::vector<int32_t> gone;
        for (uint32_t k = 0; k < ngone && ok; ++k) {
            uint32_t u = 0;
            if (u32(u)) gone.push_back(static_cast<int32_t>(u));
        }
        if (s == 0) L.gone = gone;
        else L.section_gone.emplace_back(name, gone);
        if (s == 0 || !c.name.empty()) {  // (version 1: main's; 2: the checkpoint's section's)
            L.cp = c;
            L.cp_section = name;
        }
    }
    uint32_t nplug = 0;
    if (ok && u32(nplug))
        for (uint32_t k = 0; k < nplug && k < 64 && ok; ++k) {
            std::string folder;
            uint32_t n = 0;
            if (!str(folder) || !u32(n) || at + n > size) break;
            L.plugins.emplace_back(folder, std::vector<uint8_t>(p + at, p + at + n));
            at += n;
        }
    L.valid = true;
    std::printf("[session] the save's session item read: \"%s\", checkpoint \"%s\" (%.2f %.2f %.2f) in %s\n", L.id.c_str(), L.cp.name.c_str(), L.cp.pos[0], L.cp.pos[1],
                L.cp.pos[2], L.cp_section.c_str());
    g_loaded = std::move(L);
    return true;
}

}  // namespace rac1
