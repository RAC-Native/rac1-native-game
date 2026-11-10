// See twin.h.
#include <game/enhance/twin.h>

#include <game/enhance/class_profiles.h>
#include <game/enhance/context.h>
#include <game/enhance/leveldata.h>
#include <game/enhance/pool.h>
#include <game/enhance/session.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

uint32_t g_twin_moby = 0;

namespace {

constexpr uint32_t IMPURE_PTR = 0x0012f76cu, RAND_SEED = 0x58u;  // (newlib's reentrancy block: libc_rand's seed)
constexpr uint32_t HEALTH = 0x001415f8u;
constexpr uint32_t TWIN_SEED = 0x2545f491u;  // (the twin's own random seed at its start, in both runs)

// One moby of the capture: the twin, or a moby it names (an attached part: Kalebo III's robot's 357)
struct Member {
    uint32_t moby = 0, cls = 0, pvars = 0, pvars_size = 0, block = 0, block_size = 0;
    float pos[3] = {};
    uint8_t rec[0x100] = {};
    std::vector<uint8_t> pv;
};
// What the native run takes from its level, and the session run starts from
struct Capture {
    uint32_t level = 0, cls = 0, hero = 0, pool_lo = 0, pool_hi = 0;
    float hero_pos[3] = {}, hero_yaw = 0;
    uint32_t hero_state = 0;
    uint32_t clock = 0;     // (the game's frame clock 0x15f5cc at the capture: classes keep times of it in their pvars)
    std::vector<Member> m;  // (the twin first)
    // the level's paths (the path table 0x1b04b0's entries: a point count, 12 bytes, then 16-byte points), for a view of the
    // level's own in the session (ctx_view: the level data a class reads, DESIGN 6.2)
    std::vector<std::vector<uint8_t>> paths;
    // the level's moby groups the twin is in or names (its +0x21, its profile's group slots): each its index and its members
    // (indices into m: the members taken with the twin, -1 one not taken)
    std::vector<std::pair<uint32_t, std::vector<int32_t>>> groups;
};
constexpr uint32_t PATH_TABLE = 0x001b04b0u, PATH_COUNT = 0x00160104u, TWIN_PATH_MAX = 176;
constexpr uint32_t CLOCK = 0x0015f5ccu;  // (the game's frame clock: a class's timers are times of it)

struct Twin {
    bool on = false;
    int level = -1;
    uint32_t cls = 0;
    uint64_t at = 0;
    std::string file, log;
    float spawn_at[3] = {};
    bool spawn_at_set = false;
    bool started = false, native = false, hero_placed = false;
    // (RAC1_TWIN_RESPAWN=<frames>, a session run: the twin spawned again that long after it is gone - the long runs)
    uint64_t respawn = 0, gone = 0;
    size_t spawns = 0;
    bool views_made = false;
    uint64_t start = 0;
    std::map<uint32_t, uint32_t> seed;  // (each member's own random seed: the twin's group, by moby)
    uint32_t pvars = 0, pvars_size = 0, block = 0, block_size = 0, hero = 0;
    float origin[3] = {};
    FILE* out = nullptr;
    // the mobys its update spawned since the last frame logged (class, place)
    struct Spawned {
        uint16_t cls;
        float pos[3];
    };
    std::vector<Spawned> spawned;
    std::map<uint32_t, size_t> spawned_all;  // (class -> count since its start: the 60-frame lines)
    std::vector<uint8_t> free_before;
    bool alone = false;
    std::vector<uint32_t> held;  // (RAC1_TWIN_ALONE: the mobys live at its start, sorted)
};
Twin g_t;

uint32_t pool_lo() { return lwu(OV(0x0015ffd8)); }
uint32_t pool_hi() { return lwu(OV(0x0015ffe0)); }
bool live(uint32_t m) { return static_cast<int8_t>(lbu(m + 0x20)) >= 0; }

// the pvars' size: up to the next moby's pvars (a level's pvars are one block, each moby's after the other's), at most 4 KB
uint32_t pvars_size(uint32_t pv) {
    uint32_t next = pv + 0x1000;
    for (uint32_t m = pool_lo(); m <= pool_hi() && lbu(m + 0x20) != 0xff; m += 0x100) {
        const uint32_t p = lwu(m + 0x78);
        if (p > pv && p < next) next = p;
    }
    return next - pv;
}

void put(FILE* f, const void* p, size_t n) { std::fwrite(p, 1, n, f); }
void put32(FILE* f, uint32_t v) { put(f, &v, 4); }
bool get(FILE* f, void* p, size_t n) { return std::fread(p, 1, n, f) == n; }

void hero_put(const float pos[3], float yaw, int32_t state = -1) {
    const uint32_t buf = ee_stack_alloc(0x20);
    for (int k = 0; k < 3; ++k) swc1(buf + 4 * static_cast<uint32_t>(k), pos[k]);
    swc1(buf + 0xc, 0.0f);
    sw(buf + 0x10, 0);
    sw(buf + 0x14, 0);
    swc1(buf + 0x18, yaw);
    sw(buf + 0x1c, 0);
    hero_place(buf, buf + 0x10, state, 1);
    ee_stack_free(0x20);
}

// the log's header: the run's kind and the addresses and origin its words are compared by
void log_open() {
    if (g_t.log.empty()) return;
    g_t.out = std::fopen(g_t.log.c_str(), "wb");
    if (g_t.out == nullptr) {
        std::fprintf(stderr, "[twin] %s cannot be written\n", g_t.log.c_str());
        return;
    }
    put(g_t.out, "TWL1", 4);
    const uint32_t head[] = {g_t.native ? 0u : 1u, static_cast<uint32_t>(g_t.level), g_t.cls, g_twin_moby, g_t.pvars, g_t.pvars_size, g_t.block,
                             g_t.block_size, g_t.hero, g_t.native ? 0u : ctx_image(g_t.level)};
    put(g_t.out, head, sizeof head);
    put(g_t.out, g_t.origin, 12);
}

// one frame's line: the moby's record and pvars, the hero's place and health, what it spawned
void log_frame(uint64_t frame) {
    const uint32_t m = g_twin_moby;
    const uint32_t k = static_cast<uint32_t>(frame - g_t.start);
    const uint32_t h = lwu(g::HERO_MOBY);
    if (g_t.out != nullptr) {
        put32(g_t.out, k);
        uint8_t rec[0x100];
        for (uint32_t i = 0; i < 0x100; ++i) rec[i] = lbu(m + i);
        put(g_t.out, rec, sizeof rec);
        std::vector<uint8_t> pv(g_t.pvars_size);
        for (uint32_t i = 0; i < g_t.pvars_size; ++i) pv[i] = lbu(g_t.pvars + i);
        put(g_t.out, pv.data(), pv.size());
        float hp[3] = {lwc1(h + 0x10), lwc1(h + 0x14), lwc1(h + 0x18)};
        put(g_t.out, hp, 12);
        put32(g_t.out, lwu(HEALTH));
        put32(g_t.out, lwu(g::HERO_STATE));
        put32(g_t.out, static_cast<uint32_t>(g_t.spawned.size()));
        for (const auto& s : g_t.spawned) {
            put32(g_t.out, s.cls);
            put(g_t.out, s.pos, 12);
        }
        std::fflush(g_t.out);
    }
    for (const auto& sp : g_t.spawned) ++g_t.spawned_all[sp.cls];
    if (k % 60 == 0) {
        std::string all;
        for (const auto& [c, n] : g_t.spawned_all) all += (all.empty() ? "" : ",") + std::to_string(c) + "x" + std::to_string(n);
        std::printf("[twin] %s frame +%u: state %02x at %.2f %.2f %.2f (from its start), hero at %.2f %.2f %.2f, health %d, %zu spawned (in all: %s)\n",
                    g_t.native ? "native" : "session", k, lbu(m + 0x20), lwc1(m + 0x10) - g_t.origin[0], lwc1(m + 0x14) - g_t.origin[1],
                    lwc1(m + 0x18) - g_t.origin[2], lwc1(h + 0x10) - g_t.origin[0], lwc1(h + 0x14) - g_t.origin[1], lwc1(h + 0x18) - g_t.origin[2],
                    lw(HEALTH), g_t.spawned.size(), all.empty() ? "none" : all.c_str());
    }
    g_t.spawned.clear();
}

Member take(uint32_t m) {
    Member e;
    e.moby = m;
    e.cls = lhu(m + 0xa6);
    e.pvars = lwu(m + 0x78);
    e.pvars_size = e.pvars != 0 ? pvars_size(e.pvars) : 0;
    e.block = lwu(m + 0x24);
    const PoolClass* row = pool_open(true) ? pool_find(g_t.level, e.cls) : nullptr;
    e.block_size = row != nullptr ? row->block_size : 0;
    for (int i = 0; i < 3; ++i) e.pos[i] = lwc1(m + 0x10 + 4 * static_cast<uint32_t>(i));
    for (uint32_t i = 0; i < 0x100; ++i) e.rec[i] = lbu(m + i);
    e.pv.resize(e.pvars_size);
    for (uint32_t i = 0; i < e.pvars_size; ++i) e.pv[i] = lbu(e.pvars + i);
    return e;
}

bool write_capture(const Capture& c) {
    FILE* f = std::fopen(g_t.file.c_str(), "wb");
    if (f == nullptr) return false;
    put(f, "TWN4", 4);
    const uint32_t head[] = {c.level, c.cls, c.hero, c.pool_lo, c.pool_hi, static_cast<uint32_t>(c.m.size()), c.hero_state, c.clock};
    put(f, head, sizeof head);
    put(f, c.hero_pos, 12);
    put(f, &c.hero_yaw, 4);
    for (const Member& e : c.m) {
        const uint32_t mh[] = {e.moby, e.cls, e.pvars, e.pvars_size, e.block, e.block_size};
        put(f, mh, sizeof mh);
        put(f, e.pos, 12);
        put(f, e.rec, 0x100);
        put(f, e.pv.data(), e.pv.size());
    }
    put32(f, static_cast<uint32_t>(c.paths.size()));
    for (const auto& p : c.paths) {
        put32(f, static_cast<uint32_t>(p.size()));
        put(f, p.data(), p.size());
    }
    put32(f, static_cast<uint32_t>(c.groups.size()));
    for (const auto& [g, members] : c.groups) {
        put32(f, g);
        put32(f, static_cast<uint32_t>(members.size()));
        for (const int32_t k : members) put32(f, static_cast<uint32_t>(k));
    }
    std::fclose(f);
    return true;
}

bool read_capture(Capture& c) {
    FILE* f = std::fopen(g_t.file.c_str(), "rb");
    if (f == nullptr) return false;
    char magic[4];
    uint32_t head[8];
    bool ok = get(f, magic, 4) && (std::memcmp(magic, "TWN3", 4) == 0 || std::memcmp(magic, "TWN4", 4) == 0) && get(f, head, sizeof head) && get(f, c.hero_pos, 12) &&
              get(f, &c.hero_yaw, 4);
    const bool v4 = ok && magic[3] == '4';
    if (ok) {
        c.level = head[0], c.cls = head[1], c.hero = head[2], c.pool_lo = head[3], c.pool_hi = head[4], c.hero_state = head[6], c.clock = head[7];
        c.m.resize(std::min<uint32_t>(head[5], 16));
        for (Member& e : c.m) {
            uint32_t mh[6];
            ok = ok && get(f, mh, sizeof mh) && get(f, e.pos, 12) && get(f, e.rec, 0x100);
            if (!ok) break;
            e.moby = mh[0], e.cls = mh[1], e.pvars = mh[2], e.pvars_size = std::min<uint32_t>(mh[3], 0x1000), e.block = mh[4], e.block_size = mh[5];
            e.pv.resize(e.pvars_size);
            ok = get(f, e.pv.data(), e.pv.size());
        }
        uint32_t np = 0;
        if (ok && get(f, &np, 4))
            for (uint32_t k = 0; k < np && k < TWIN_PATH_MAX && ok; ++k) {
                uint32_t n = 0;
                ok = get(f, &n, 4) && n <= 0x10000;
                if (!ok) break;
                c.paths.emplace_back(n);
                ok = get(f, c.paths.back().data(), n);
            }
        uint32_t ng = 0;
        if (ok && v4 && get(f, &ng, 4))
            for (uint32_t k = 0; k < ng && k < LD_GROUP_MAX && ok; ++k) {
                uint32_t g = 0, n = 0;
                ok = get(f, &g, 4) && get(f, &n, 4) && n <= 0x800;
                std::vector<int32_t> members(ok ? n : 0);
                for (uint32_t i = 0; i < members.size() && ok; ++i) ok = get(f, &members[i], 4);
                if (ok) c.groups.emplace_back(g, std::move(members));
            }
    }
    std::fclose(f);
    return ok && !c.m.empty();
}

// the native run: the live moby of the class nearest the hero, and the mobys it names, taken and written to the file
bool capture() {
    const uint32_t h = lwu(g::HERO_MOBY);
    uint32_t best = 0;
    float bd = 1e30f;
    for (uint32_t m = pool_lo(); m <= pool_hi() && lbu(m + 0x20) != 0xff; m += 0x100) {
        if (lhu(m + 0xa6) != g_t.cls || !live(m)) continue;
        const float dx = lwc1(m + 0x10) - lwc1(h + 0x10), dy = lwc1(m + 0x14) - lwc1(h + 0x14), dz = lwc1(m + 0x18) - lwc1(h + 0x18);
        const float d = dx * dx + dy * dy + dz * dz;
        if (d < bd) {
            bd = d;
            best = m;
        }
    }
    if (best == 0) {
        std::fprintf(stderr, "[twin] no live moby of class %u on level %d\n", g_t.cls, g_t.level);
        return false;
    }
    Capture c;
    c.level = static_cast<uint32_t>(g_t.level);
    c.cls = g_t.cls;
    c.hero = h;
    c.pool_lo = pool_lo();
    c.pool_hi = pool_hi() + 0x100;
    for (int i = 0; i < 3; ++i) c.hero_pos[i] = lwc1(h + 0x10 + 4 * static_cast<uint32_t>(i));
    c.hero_yaw = lwc1(h + 0x48);
    c.hero_state = lwu(g::HERO_STATE);
    c.clock = lwu(OV(CLOCK));
    c.m.reserve(16);  // (t below stays valid)
    c.m.push_back(take(best));
    // the mobys it names (its record's and pvars' words into the level's moby pool): taken with it
    const Member& t = c.m[0];
    for (uint32_t i = 0; i + 4 <= 0x100 + t.pvars_size && c.m.size() < 16; i += 4) {
        uint32_t w = 0;
        std::memcpy(&w, i < 0x100 ? t.rec + i : t.pv.data() + (i - 0x100), 4);
        if (i == 0x28) continue;  // (+0x28: the frame's chain of the mobys to update, moby_visible_queue: bookkeeping)
        if (w < c.pool_lo || w >= c.pool_hi || ((w - c.pool_lo) & 0xffu) != 0 || w == best || w == h) continue;
        bool have = false;
        for (const Member& e : c.m) have |= e.moby == w;
        if (have || lbu(w + 0x20) >= 0xfe) continue;
        c.m.push_back(take(w));
        std::printf("[twin] native: its %s +%#x names moby %08x of class %u (state %02x): taken with it\n", i < 0x100 ? "record" : "pvars",
                    i < 0x100 ? i : i - 0x100, w, c.m.back().cls, lbu(w + 0x20));
    }
    // the level's moby groups the twin is in or names (its +0x21, its profile's group slots), their members taken with it (the
    // session makes them again, at the same indices, in its context's registry: what the class's code reads by index)
    {
        const uint32_t self = (best - c.pool_lo) >> 8;
        std::set<uint32_t> gs;
        if (lbu(best + 0x21) < LD_GROUP_MAX) gs.insert(lbu(best + 0x21));
        const uint32_t count = std::min<uint32_t>(lwu(OV(LD_GROUP_COUNT)), LD_GROUP_MAX);
        auto members_of = [](uint32_t g) {
            std::vector<uint32_t> v;
            const uint32_t list = lwu(OV(LD_GROUPS) + 4 * g);
            for (uint32_t a = list; list != 0 && v.size() < 0x800; a += 2) {
                const uint32_t e = lhu(a);
                v.push_back(e & 0x7fffu);
                if ((e & 0x8000u) != 0) break;
            }
            return v;
        };
        for (uint32_t g = 0; g < count; ++g)
            for (const uint32_t k : members_of(g))
                if (k == self) gs.insert(g);
        if (const Profile* prof = profile_find(g_t.level, g_t.cls)) {
            std::istringstream slots(profile_slots_text(prof));
            std::string w;
            while (slots >> w) {
                const size_t colon = w.find(':');
                uint32_t off = 0;
                std::string kind;
                if (colon == std::string::npos || w.substr(colon + 1) != "group" || !profile_slot(prof, w.substr(0, colon), &off, &kind) || off + 4 > t.pvars_size) continue;
                int32_t v = 0;
                std::memcpy(&v, t.pv.data() + off, 4);
                if (v >= 0 && v < static_cast<int32_t>(LD_GROUP_MAX)) gs.insert(static_cast<uint32_t>(v));
            }
        }
        for (const uint32_t g : gs) {
            std::vector<int32_t> idx;
            size_t taken = 0;
            for (const uint32_t k : members_of(g)) {
                const uint32_t m = c.pool_lo + (k << 8);
                int32_t at = -1;
                for (size_t i = 0; i < c.m.size(); ++i)
                    if (c.m[i].moby == m) at = static_cast<int32_t>(i);
                if (at < 0 && m < c.pool_hi && live(m) && m != h && c.m.size() < 16) {
                    c.m.push_back(take(m));
                    at = static_cast<int32_t>(c.m.size() - 1);
                    ++taken;
                }
                idx.push_back(at);
            }
            std::printf("[twin] native: group %u (%zu members, %zu taken with it)%s\n", g, idx.size(), taken, lbu(best + 0x21) == g ? ": its own (+0x21)" : "");
            c.groups.emplace_back(g, std::move(idx));
        }
    }
    // the level's paths (RAC1_TWIN_PATHS=1: the session's twin sees them, through a view of its context)
    if (const char* e = std::getenv("RAC1_TWIN_PATHS"); e != nullptr && std::atoi(e) != 0) {
        const uint32_t n = std::min<uint32_t>(lwu(OV(PATH_COUNT)), TWIN_PATH_MAX);
        for (uint32_t k = 0; k < n; ++k) {
            const uint32_t p = lwu(OV(PATH_TABLE) + 4 * k);
            const uint32_t pts = p != 0 ? std::min<uint32_t>(lwu(p), 0x400) : 0;
            std::vector<uint8_t> b(p != 0 ? 0x10 + 0x10 * pts : 0);
            for (uint32_t i = 0; i < b.size(); ++i) b[i] = lbu(p + i);
            c.paths.push_back(std::move(b));
        }
        std::printf("[twin] native: the level's %u paths taken\n", n);
    }
    if (!write_capture(c)) {
        std::fprintf(stderr, "[twin] %s cannot be written\n", g_t.file.c_str());
        return false;
    }
    hero_put(c.hero_pos, c.hero_yaw);  // (the hero put again where he is: a hero just placed, as the session's will be)
    g_twin_moby = best;
    for (const Member& e : c.m) g_t.seed[e.moby] = TWIN_SEED + e.cls;
    g_t.pvars = t.pvars;
    g_t.pvars_size = t.pvars_size;
    g_t.block = t.block;
    g_t.block_size = t.block_size;
    g_t.hero = h;
    std::memcpy(g_t.origin, t.pos, 12);
    std::printf("[twin] native: moby %08x of class %u at %.2f %.2f %.2f taken (pvars %08x+%#x, block %08x+%#x), the hero %.2f %.2f %.2f from it\n", best,
                g_t.cls, t.pos[0], t.pos[1], t.pos[2], t.pvars, t.pvars_size, t.block, t.block_size, c.hero_pos[0] - t.pos[0], c.hero_pos[1] - t.pos[1],
                c.hero_pos[2] - t.pos[2]);
    return true;
}

// the session run, a frame before the twin's spawn: the hero put at the native run's place relative to it, facing the same
// way (a frame for the game's copies of his place to follow him: the native hero stood there already)
bool hero_first() {
    Capture c;
    if (!read_capture(c) || static_cast<int>(c.level) != g_t.level || c.cls != g_t.cls) {
        std::fprintf(stderr, "[twin] %s is not a capture of %d:%u (the native run makes it)\n", g_t.file.c_str(), g_t.level, g_t.cls);
        return false;
    }
    const float* o = g_t.spawn_at_set ? g_t.spawn_at : c.m[0].pos;
    float hp[3];
    for (int k = 0; k < 3; ++k) hp[k] = o[k] + c.hero_pos[k] - c.m[0].pos[k];
    hero_put(hp, c.hero_yaw, static_cast<int32_t>(c.hero_state));
    return true;
}

// the session run: the capture's mobys spawned, their records and pvars with their pointers moved to the session's
bool spawn() {
    Capture c;
    if (!read_capture(c) || static_cast<int>(c.level) != g_t.level || c.cls != g_t.cls) {
        std::fprintf(stderr, "[twin] %s is not a capture of %d:%u (the native run makes it)\n", g_t.file.c_str(), g_t.level, g_t.cls);
        return false;
    }
    const Member& t = c.m[0];
    const float* o = g_t.spawn_at_set ? g_t.spawn_at : t.pos;
    const uint32_t h = lwu(g::HERO_MOBY);
    // the game's frame clock as the native run's at the capture (the class's pvars hold times of it: a turret's next shot)
    if (lwu(OV(CLOCK)) != c.clock) {
        std::printf("[twin] session: the frame clock %u set to the native run's %u\n", lwu(OV(CLOCK)), c.clock);
        sw(OV(CLOCK), c.clock);
    }
    // each member's moby, block and pvars in the session
    std::vector<uint32_t> sm(c.m.size()), sblk(c.m.size()), spv(c.m.size());
    for (size_t i = 0; i < c.m.size(); ++i) {
        sm[i] = moby_spawn(c.m[i].cls);
        if (sm[i] == 0 || lhu(sm[i] + 0xa6) != c.m[i].cls) {
            std::fprintf(stderr, "[twin] class %u could not be spawned (not imported?)\n", c.m[i].cls);
            return false;
        }
        sblk[i] = lwu(sm[i] + 0x24);
        spv[i] = c.m[i].pvars_size != 0 ? session_heap_alloc(c.m[i].pvars_size, "twin") : 0;
    }
    size_t moved[6] = {}, vec = 0;
    // a word of the native run as the session's: into a member's class block, pvars or record, the hero, the level's own
    // data or code
    // (`level`: a word of the pvars, where the level's own data and functions may be named; a moby record's other words
    // in that range are packed bytes: 0x0024ff00, say)
    auto rebase = [&](uint32_t w, bool level) -> uint32_t {
        for (size_t i = 0; i < c.m.size(); ++i) {
            const Member& e = c.m[i];
            if (e.block != 0 && w >= e.block && w - e.block < e.block_size) return ++moved[0], sblk[i] + (w - e.block);
            if (spv[i] != 0 && w >= e.pvars && w - e.pvars < e.pvars_size) return ++moved[1], spv[i] + (w - e.pvars);
            if (w >= e.moby && w - e.moby < 0x100) return ++moved[5], sm[i] + (w - e.moby);
        }
        if (w >= c.hero && w - c.hero < 0x100) return ++moved[2], h + (w - c.hero);
        if (w >= c.pool_lo && w < c.pool_hi) return ++moved[3], 0u;  // (another moby of the level: none here)
        if (level && w >= CONTEXT_BASE && w < 0x00400000u) {
            uint32_t to = w;
            const CtxKind k = ctx_classify(g_t.level, w, &to);
            // (code: a function's start only, a callback; data: any place)
            const bool fn = (k == CtxKind::EngineCode) || (k == CtxKind::OwnCode && ovl_level_moby_fn_has(g_t.level, w));
            if (k == CtxKind::OwnData || k == CtxKind::EngineData || fn) return ++moved[4], to;
        }
        return w;
    };
    // the places of the level (a vec3 within 64 units of the native twin: its home, a target) moved with it
    auto shift = [&](uint8_t* b, size_t n) {
        for (size_t i = 0; i + 12 <= n; i += 4) {
            float v[3];
            std::memcpy(v, b + i, 12);
            bool near = true;
            for (int k = 0; k < 3; ++k) near &= std::isfinite(v[k]) && std::fabs(v[k] - t.pos[k]) < 64.0f && v[k] != 0.0f;
            if (!near) continue;
            for (int k = 0; k < 3; ++k) v[k] += o[k] - t.pos[k];
            std::memcpy(b + i, v, 12);
            ++vec;
            i += 8;
        }
    };
    // (kept from the session's moby_init / moby_spawn: its class index and header, the update chain's link, its update
    // function, its pvars, its grid rectangle, its index)
    const auto keep = [](uint32_t i) { return i == 0x22 || (i >= 0x24 && i < 0x2c) || (i >= 0x74 && i < 0x7c) || (i >= 0xa0 && i < 0xb0 && i != 0xa4 && i != 0xa5); };
    for (size_t n = 0; n < c.m.size(); ++n) {
        const Member& e = c.m[n];
        const uint32_t m = sm[n];
        uint8_t rec[0x100];
        std::memcpy(rec, e.rec, 0x100);
        shift(rec + 0x10, 0xf0);
        for (uint32_t i = 0; i < 0x100; i += 4) {
            uint32_t w = 0;
            std::memcpy(&w, rec + i, 4);
            if (!keep(i) && !keep(i + 1) && !keep(i + 2) && !keep(i + 3)) {
                sw(m + i, rebase(w, false));
                continue;
            }
            for (uint32_t b = 0; b < 4; ++b)
                if (!keep(i + b)) sb(m + i + b, rec[i + b]);
        }
        if (spv[n] != 0) {
            std::vector<uint8_t> p = e.pv;
            shift(p.data(), p.size());
            for (uint32_t i = 0; i + 4 <= e.pvars_size; i += 4) {
                uint32_t w = 0;
                std::memcpy(&w, p.data() + i, 4);
                sw(spv[n] + i, rebase(w, true));
            }
            sw(m + 0x78, spv[n]);
        }
        // (its moby group +0x21: the session has none of its level's groups, and a moby of a group is updated only through
        // its group's list: none, as an import_spawn's)
        sb(m + 0x21, 0xff);
        moby_update_transform(m);
        g_t.seed[m] = TWIN_SEED + e.cls;
    }
    // the level's paths, its context's own view of the path table (their points moved with the twin)
    if (!c.paths.empty() && !g_t.views_made) {
        g_t.views_made = true;
        const uint32_t table = session_heap_alloc(TWIN_PATH_MAX * 4 + 16, "twin");
        sw(table + TWIN_PATH_MAX * 4, static_cast<uint32_t>(c.paths.size()));
        for (size_t k = 0; k < c.paths.size(); ++k) {
            const auto& b = c.paths[k];
            if (b.empty()) continue;
            const uint32_t p = session_heap_alloc(static_cast<uint32_t>(b.size()), "twin");
            for (uint32_t i = 0; i < b.size(); ++i) sb(p + i, b[i]);
            for (uint32_t i = 0x10; i + 0x10 <= b.size(); i += 0x10)
                for (uint32_t a = 0; a < 3; ++a) swc1(p + i + 4 * a, lwc1(p + i + 4 * a) + o[a] - t.pos[a]);
            sw(table + 4 * static_cast<uint32_t>(k), p);
        }
        // (into its context's registry, leveldata.h: what its sites and the engine's functions it calls read; without one, a
        // view of the path table and its count)
        if (ld_paths_take(g_t.level, table, static_cast<uint32_t>(c.paths.size()))) {
            std::printf("[twin] session: level %d's %zu paths in its context's registry\n", g_t.level, c.paths.size());
            const float d[3] = {o[0] - t.pos[0], o[1] - t.pos[1], o[2] - t.pos[2]};
            ld_registry_move(g_t.level, d);
        } else {
            ctx_view(g_t.level, PATH_TABLE, TWIN_PATH_MAX * 4, table);
            ctx_view(g_t.level, PATH_COUNT, 4, table + TWIN_PATH_MAX * 4);
            std::printf("[twin] session: level %d's %zu paths in a view of its context's own\n", g_t.level, c.paths.size());
        }
    }
    // the level's groups it is in or names: made again at their indices in its context's registry, of the session's mobys
    for (const auto& [g, members] : c.groups) {
        std::vector<uint32_t> list;
        for (const int32_t k : members)
            if (k >= 0 && static_cast<size_t>(k) < sm.size()) list.push_back(sm[static_cast<size_t>(k)]);
        const bool ok = !list.empty() && ld_group_set(g_t.level, static_cast<int32_t>(g), list.data(), static_cast<int>(list.size()));
        std::printf("[twin] session: group %u of level %d: %zu of its %zu members %s\n", g, g_t.level, list.size(), members.size(),
                    ok ? "in its context's registry" : "not made (no registry, or none of them taken)");
    }
    g_twin_moby = sm[0];
    g_t.pvars = spv[0];
    g_t.pvars_size = t.pvars_size;
    g_t.block = sblk[0];
    g_t.block_size = t.block_size;
    g_t.hero = h;
    std::memcpy(g_t.origin, o, 12);
    std::printf("[twin] session: moby %08x of class %u spawned at %.2f %.2f %.2f from the capture (pvars %08x+%#x, block %08x), %zu more with it; words "
                "moved: blocks %zu, pvars %zu, records %zu, hero %zu, other mobys %zu (to 0), level %d's %zu; places moved %zu\n",
                sm[0], c.cls, o[0], o[1], o[2], spv[0], t.pvars_size, sblk[0], c.m.size() - 1, moved[0], moved[1], moved[5], moved[2], moved[3], g_t.level,
                moved[4], vec);
    return true;
}

}  // namespace

// orig: (host) see twin.h
void twin_start_tests() {
    const char* e = std::getenv("RAC1_SESSION_TWIN");
    if (e == nullptr || *e == 0) return;
    unsigned l = 0, c = 0;
    unsigned long long f = 0;
    const char* file = std::getenv("RAC1_TWIN_FILE");
    if (std::sscanf(e, "%u:%u@%llu", &l, &c, &f) != 3 || l >= 19 || file == nullptr) {
        std::fprintf(stderr, "[twin] RAC1_SESSION_TWIN=<level>:<class>@<frame> with RAC1_TWIN_FILE=<file>\n");
        return;
    }
    g_t.on = true;
    g_t.level = static_cast<int>(l);
    g_t.cls = c;
    g_t.at = f;
    g_t.file = file;
    if (const char* lg = std::getenv("RAC1_TWIN_LOG")) g_t.log = lg;
    if (const char* al = std::getenv("RAC1_TWIN_ALONE")) g_t.alone = std::atoi(al) != 0;
    if (const char* rs = std::getenv("RAC1_TWIN_RESPAWN")) g_t.respawn = std::strtoull(rs, nullptr, 10);
    if (const char* at = std::getenv("RAC1_TWIN_AT"))
        g_t.spawn_at_set = std::sscanf(at, "%f,%f,%f", &g_t.spawn_at[0], &g_t.spawn_at[1], &g_t.spawn_at[2]) == 3;
    // a session run imports the class and the classes of the mobys taken with it (a capture made already); on its own
    // level the imports are the level's own classes
    session_import_add(g_t.level, c);
    if (Capture cap; read_capture(cap) && static_cast<int>(cap.level) == g_t.level && cap.cls == c)
        for (const Member& m : cap.m) session_import_add(g_t.level, m.cls);
    session_memory_begin();
    std::printf("[twin] class %d:%u from frame %llu (%s)\n", g_t.level, c, f, file);
}

// orig: (host) see twin.h
void twin_frame(uint64_t frame) {
    if (!g_t.on) return;
    if (!g_t.started) {
        if (frame < g_t.at || lw(OV(0x0015f5c4)) != 0) return;  // (in play: game mode 0)
        g_t.native = ovl_reloc_level() == g_t.level;
        if (!g_t.native && !g_t.hero_placed) {
            g_t.hero_placed = true;
            if (!hero_first()) g_t.on = false;
            return;
        }
        // (the native run takes it when the hero stands still: his motion is not carried into the session, his place is)
        if (g_t.native && lw(g::HERO_STATE) != 0 && frame < g_t.at + 300) return;
        g_t.started = g_t.native ? capture() : spawn();
        if (!g_t.started) {
            g_t.on = false;
            return;
        }
        g_t.start = frame;
        if (g_t.alone) {
            const uint32_t h = lwu(g::HERO_MOBY);
            for (uint32_t m = pool_lo(); m <= pool_hi() && lbu(m + 0x20) != 0xff; m += 0x100)
                if (live(m) && m != h && g_t.seed.count(m) == 0) g_t.held.push_back(m);
            std::printf("[twin] alone: %zu mobys of the level held\n", g_t.held.size());
        }
        log_open();
    }
    if (g_twin_moby == 0 && !g_t.native && g_t.respawn != 0 && frame >= g_t.gone + g_t.respawn && lw(OV(0x0015f5c4)) == 0) {
        g_t.seed.clear();
        if (spawn()) std::printf("[twin] session: spawned again (%zu) at frame %llu\n", ++g_t.spawns, static_cast<unsigned long long>(frame));
        else g_t.respawn = 0;
    }
    if (g_twin_moby != 0) {
        log_frame(frame);
        if (!live(g_twin_moby) || lhu(g_twin_moby + 0xa6) != g_t.cls) {
            std::printf("[twin] %s: the twin is gone at frame +%llu (state %02x)\n", g_t.native ? "native" : "session",
                        static_cast<unsigned long long>(frame - g_t.start), lbu(g_twin_moby + 0x20));
            g_twin_moby = 0;
            g_t.gone = frame;
            if (g_t.out != nullptr) std::fclose(g_t.out), g_t.out = nullptr;
        }
    }
}

// orig: (host) see twin.h
void twin_level_begin() {
    if (!g_t.on) return;
    if (g_twin_moby != 0 && g_t.started) std::printf("[twin] a level setup: the twin is gone with the level\n");
    g_twin_moby = 0;
    g_t.views_made = false;
    g_t.held.clear();
    g_t.seed.clear();
    if (g_t.out != nullptr) std::fclose(g_t.out), g_t.out = nullptr;
}

// orig: (host) see twin.h
int twin_kind(uint32_t moby) {
    if (g_t.seed.count(moby) != 0) return 2;  // (the twin or a moby taken with it: its own random numbers)
    return !g_t.held.empty() && std::binary_search(g_t.held.begin(), g_t.held.end(), moby) ? 1 : 0;
}

// orig: (host) see twin.h
void twin_call(TwinFn fn, uint32_t moby) {
    // its own random numbers (the two levels' other mobys draw theirs from the game's seed in their own order)
    const uint32_t at = lwu(IMPURE_PTR) + RAND_SEED;
    const uint32_t game = lwu(at);
    uint32_t& seed = g_t.seed[moby];
    sw(at, seed);
    // the spawn range's free slots before: a slot taken during the update is a moby it spawned
    const uint32_t lo = lwu(OV(0x0015ffdc)), hi = lwu(OV(0x0015ffe0));
    g_t.free_before.assign(hi > lo ? (hi - lo) / 0x100 + 1 : 0, 0);
    for (uint32_t m = lo, i = 0; m <= hi && i < g_t.free_before.size(); m += 0x100, ++i) g_t.free_before[i] = lbu(m + 0x20) >= 0xfe;
    fn(moby);
    for (uint32_t m = lo, i = 0; m <= hi && i < g_t.free_before.size(); m += 0x100, ++i)
        if (g_t.free_before[i] != 0 && lbu(m + 0x20) < 0xfe)
            g_t.spawned.push_back({lhu(m + 0xa6), {lwc1(m + 0x10) - g_t.origin[0], lwc1(m + 0x14) - g_t.origin[1], lwc1(m + 0x18) - g_t.origin[2]}});
    seed = lwu(at);
    sw(at, game);
}

}  // namespace rac1
