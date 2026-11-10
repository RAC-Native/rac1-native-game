// See session.h.
#include <game/enhance/session.h>
#include <game/enhance/session_runtime.h>

#include <game/enhance/class_profiles.h>
#include <game/enhance/context.h>
#include <game/enhance/leveldata.h>
#include <game/enhance/pool.h>
#include <game/enhance/twin.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl.h>
#include <runtime/iop/snd989_iop.h>
#include <runtime/kernel/ee_memory.h>

#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <algorithm>
#include <fstream>
#include <map>
#include <set>
#include <sstream>
#include <vector>

namespace rac1 {

namespace {

struct Block {
    uint32_t size;  // (aligned)
    std::string owner;
};
bool g_on = false;
uint32_t g_end = 0;                   // the heap's end: the session's RAM size
std::map<uint32_t, uint32_t> g_free;  // free runs: start -> size (coalesced)
std::map<uint32_t, Block> g_blocks;   // the blocks in use

bool trace() {
    static const bool t = [] {
        const char* e = std::getenv("RAC1_SESSION_TRACE");
        return e != nullptr && std::atoi(e) != 0;
    }();
    return t;
}

// a free run put back, joined with its neighbours
void free_run(uint32_t a, uint32_t n) {
    auto next = g_free.lower_bound(a);
    if (next != g_free.end() && a + n == next->first) {
        n += next->second;
        next = g_free.erase(next);
    }
    if (next != g_free.begin()) {
        auto prev = std::prev(next);
        if (prev->first + prev->second == a) {
            prev->second += n;
            return;
        }
    }
    g_free[a] = n;
}

void reset() {
    g_free.clear();
    g_blocks.clear();
    if (g_on && g_end > SESSION_HEAP_BASE) g_free[SESSION_HEAP_BASE] = g_end - SESSION_HEAP_BASE;
}

}  // namespace

// orig: (host) see session.h
bool session_memory_on() { return g_on; }

// orig: (host) see session.h
bool session_memory_begin() {
    if (g_on) return true;
    if (!ee_session_memory(ee_session_ram_max())) {
        std::fprintf(stderr, "[session] the EE memory could not be grown to %u MB: no session memory\n", ee_session_ram_max() >> 20);
        return false;
    }
    g_on = true;
    g_end = ee_ram_size();
    reset();
    std::printf("[session] memory on: EE memory %u MB, the heap %08x..%08x\n", g_end >> 20, SESSION_HEAP_BASE, g_end);
    return true;
}

// orig: (host) see session.h
void session_memory_end() {
    if (!g_on) return;
    g_on = false;
    reset();
    ee_session_memory(0);
    std::printf("[session] memory off: EE memory %u MB\n", ee_ram_size() >> 20);
}

namespace {
// (testing, RAC1_SESSION_HEAP_TEST=1) the memory and the heap exercised before the game starts: words written at the
// session RAM's ends read back directly and through the uncached mirror, blocks of 40 owners allocated and freed 100
// times (the heap back to one free run each time), the RAM back to 64 MB and on again (zeros above 64 MB). Logged as
// `[session] heap test: ok` or the first failure
void heap_test() {
    auto fail = [](const char* what) { std::printf("[session] heap test: FAILED (%s)\n", what); };
    if (!session_memory_begin()) return fail("no session memory");
    const uint32_t top = ee_ram_size() - 16;
    ee_write<uint32_t>(SESSION_HEAP_BASE, 0x12345678u);
    ee_write<uint32_t>(top, 0x9abcdef0u);
    if (ee_read<uint32_t>(SESSION_HEAP_BASE | 0x20000000u) != 0x12345678u || ee_read<uint32_t>(top | 0x20000000u) != 0x9abcdef0u)
        return fail("the uncached mirror of the session RAM");
    if (!ee_is_valid_addr(top, 16) || ee_is_valid_addr(top, 17) || ee_game_ram_end() != ee_ram_size()) return fail("the limits");
    ee_write<uint32_t>(SESSION_HEAP_BASE, 0);
    ee_write<uint32_t>(top, 0);
    uint32_t seed = 1;
    auto rnd = [&] { return seed = seed * 1103515245u + 12345u, seed >> 8; };
    for (int round = 0; round < 100; ++round) {
        for (int k = 0; k < 200; ++k) {
            const uint32_t a = session_heap_alloc(1 + rnd() % 0x40000, "test " + std::to_string(rnd() % 40));
            if (a == 0 || (a & (SESSION_HEAP_ALIGN - 1)) != 0 || a < SESSION_HEAP_BASE) return fail("an allocation");
            if (ee_read<uint32_t>(a) != 0) return fail("a block not zero-filled");
            ee_write<uint32_t>(a, 0xdeadbeefu);
        }
        for (int o = 0; o < 40; ++o) session_heap_free_owner("test " + std::to_string(o));
        if (session_heap_blocks() != 0 || g_free.size() != 1 || g_free.begin()->second != g_end - SESSION_HEAP_BASE) return fail("the heap not whole again");
    }
    ee_write<uint32_t>(0x05000000u, 1);
    session_memory_end();
    if (ee_ram_size() != 0x04000000u || ee_game_ram_end() != 0x02000000u || ee_is_valid_addr(0x05000000u, 4)) return fail("the memory back to 64 MB");
    session_memory_begin();
    if (ee_read<uint32_t>(0x05000000u) != 0) return fail("the session RAM not zero again");
    session_memory_end();
    std::printf("[session] heap test: ok (100 rounds of 200 blocks, the mirrors, the limits, the memory off and on)\n");
}
}  // namespace

namespace {
// the test's class (RAC1_SESSION_HIGH_CLASS): the class, its session id (0: none, the native id from the session heap)
uint32_t g_high_class = 0xffffffffu, g_high_id = 0;
// the session classes, by their id in the session
struct SessionClass {
    int level = 0;
    uint32_t cls = 0, fn = 0, word = 0, hdr = 0;
    uint32_t idx = 0, remap = 0;  // its class index, its texture remap (in the heap)
    bool imported = false;        // an import (or the stage 1 test's class)
    std::string why = {};         // what brought it: "imported", "spawned by 13:29"
    bool sounds = false;          // its sounds in its level's bank
};
std::map<uint32_t, SessionClass> g_session_ids;
// the classes a session imports from other levels (each with its closure): the testing switch's (RAC1_SESSION_IMPORT, the
// twin run), a level mod's (its build's imports.txt), the plugin's (import_load)
struct Import {
    int level;
    uint32_t cls;
    std::string from;  // "test", "mod", "plugin"
};
std::vector<Import> g_imports;
// their levels' sound banks, loaded as host banks (the level's whole bank: host memory, the sound driver's own copy; the
// IOP's 2 MB are not used): level -> handle and size; unloaded when the session has no class of the level left
struct Bank {
    uint32_t handle = 0;
    size_t bytes = 0;
};
std::map<int, Bank> g_banks;
// class indices given back by unloaded classes (taken again first: the class table's 224 never grows past what is resident)
std::vector<uint32_t> g_free_idx;
// what an unload frees a few frames later (the renderer's thread may still draw the frame before from a class block):
// heap owners, the frame from which they go
std::vector<std::pair<std::string, uint64_t>> g_unload_pending;
uint64_t g_frame = 0;
// the mobys spawned from a class's placement with pvars in the heap (import_spawn): moby -> its pvars block
std::map<uint32_t, uint32_t> g_spawn_pvars;

// level `level`'s bank as a host bank (loaded once), 0 when it cannot be had
uint32_t level_bank(int level) {
    if (const auto it = g_banks.find(level); it != g_banks.end()) return it->second.handle;
    std::vector<uint8_t> b;
    const uint32_t h = pool_sound_bank(level, b) ? snd989_host_bank_load(b) : 0;
    g_banks[level] = Bank{h, b.size()};
    if (h == 0) std::fprintf(stderr, "[session] level %d's sound bank could not be loaded: its classes are silent\n", level);
    else std::printf("[session] level %d's sound bank loaded (%zu KB): host bank %#x\n", level, b.size() >> 10, h);
    return h;
}

std::string pool_owner(int level, uint32_t cls) { return "pool " + std::to_string(level) + ":" + std::to_string(cls); }

// a pool class's block copied into the session heap (its owner "pool L:C"): its address, 0 when it cannot be had
uint32_t heap_copy(const PoolClass& c, const char* why) {
    const uint8_t* b = pool_block(c);
    const uint32_t a = b != nullptr ? session_heap_alloc(c.block_size, pool_owner(c.level, c.id)) : 0;
    if (a == 0) {
        std::fprintf(stderr, "[session] class %d:%u: no room for its block (%#x bytes)\n", c.level, c.id, c.block_size);
        return 0;
    }
    std::memcpy(ee_get_ram_base() + a, b, c.block_size);
    std::printf("[session] class %d:%u's pool block at %08x (%#x bytes): %s\n", c.level, c.id, a, c.block_size, why);
    return a;
}

// the base level has class `cls` (registered by its own setup, not by the session)
bool base_has(uint32_t cls) {
    if (cls >= 2048 || lbu(OV(0x00197bc0) + cls) == 0xffu) return false;
    return g_session_ids.count(cls) == 0;
}

// class index `idx` holds class id `id`
bool cls_entry_ok(uint32_t idx, uint32_t id) { return idx < 224 && lhu(OV(0x00197a00) + idx * 2) == id; }

// the class registered at a class index given back by an unload, else at the next one (moby_class_register takes
// *(0x15ffc0))
void register_at_free(uint32_t hdr, uint32_t tex, uint32_t remap, uint32_t id) {
    if (g_free_idx.empty()) {
        moby_class_register(hdr, tex, remap, id);
        return;
    }
    const uint32_t n = lwu(OV(0x0015ffc0));
    sw(OV(0x0015ffc0), g_free_idx.back());
    g_free_idx.pop_back();
    moby_class_register(hdr, tex, remap, id);
    sw(OV(0x0015ffc0), n);
}

uint32_t g_tex = 0;  // (the level's moby textures, for a class registered after its setup)

// one class of another level imported: its block from the pool, its level's context, registered under its own id (or a
// session id: a class the base has under that id with other bytes), its update function and +0x2c word through the context,
// its sounds pointed at its level's bank, the modern renderer's import list. False when it cannot be had (logged)
bool import_one(int from, uint32_t cls, const std::string& why) {
    const PoolClass* c = pool_find(from, cls);
    if (c == nullptr || c->block_size == 0) {
        std::fprintf(stderr, "[session] import %d:%u (%s): no such class with data in the pool\n", from, cls, why.c_str());
        return false;
    }
    if (lwu(OV(0x0015ffc0)) >= 224 && g_free_idx.empty()) {
        std::fprintf(stderr, "[session] import %d:%u: the class table is full (224)\n", from, cls);
        return false;
    }
    // its own id (a game class id is the same class on every level: the engine and the class's code test it), unless the
    // base has that id: then a session id (2048..), for a second copy
    uint32_t id = cls;
    if (cls >= 2048 || lbu(OV(0x00197bc0) + cls) != 0xffu) {
        id = 2048;
        while (g_session_ids.count(id) != 0 || lbu(OV(0x00197bc0) + id) != 0) ++id;
        std::printf("[session] import %d:%u: the base has class %u: imported as session class %u\n", from, cls, cls, id);
    }
    if (!ctx_create(from)) return false;
    ld_registry_make(from);  // (its level data: a registry of its context's own, leveldata.h)
    const std::string own = pool_owner(from, cls);
    const uint32_t remap = session_heap_alloc(16, own);
    const uint32_t a = remap != 0 ? heap_copy(*c, ("imported from level " + std::to_string(from) + ": " + why).c_str()) : 0;
    if (a == 0) {
        session_heap_free_owner(own);
        return false;
    }
    // (its texture slots on the base's texture 0: the faithful renderer draws a session's classes wrongly; the modern
    // renderer draws it from its own level's class file, the import list)
    for (uint32_t k = 0; k < 16; ++k) sb(remap + k, c->remap[k] == 0xffu ? 0xffu : 0u);
    const uint32_t fn = ctx_xlate(from, c->update_fn), word = ctx_xlate(from, c->word2c);
    SessionClass sc{from, cls, fn, word, a};
    sc.remap = remap;
    sc.imported = true;
    sc.why = why;
    g_session_ids[id] = sc;
    g_session_classes = true;
    g_spawn_watch = true;
    register_at_free(a, g_tex, remap, id);
    g_session_ids[id].idx = lbu(OV(0x00197bc0) + id);
    if (ld_registry_has(from) && id < 0x1ec0) {  // (its updates read its level's registry: the dispatch swaps it in)
        g_ld_class_level[id] = static_cast<uint8_t>(from + 1);
        g_ld_any = true;
    }
    mrp_moby_import(from, cls);
    // its sounds: its definitions' bank sounds (the pool's row: its level's asset index) in its level's bank
    if (lwu(a + 0x28) != 0 && lbu(a + 0xd) != 0) {
        const uint32_t bank = level_bank(from), t = lwu(a + 0x28);
        for (uint32_t j = 0; j < lbu(a + 0xd); ++j) {
            sh(t + 32 * j + 0x1a, j < c->sounds.size() && bank != 0 ? c->sounds[j] : 0xffffu);
            sw(t + 32 * j + 0x1c, bank);
        }
        g_session_ids[id].sounds = bank != 0;
    }
    std::printf("[session] import %d:%u as class %u (index %u, %s): update %08x -> %08x, +0x2c %08x -> %08x, %u sounds\n", from, cls, id,
                g_session_ids[id].idx, why.c_str(), c->update_fn, fn, c->word2c, word, lbu(a + 0xd));
    return true;
}

// the closure of the session's imports: each import, then what it spawns (its dependencies, pool/deps: its own code's and
// the engine's for it), each from the import's level, unless the base has it (the base's own then serves: the same id is
// the same class). In order, without repeats; the reasons for the log
struct Need {
    int level;
    uint32_t cls;
    std::string why;
    bool root;
};
// the classes of the mobys a class's first placement names (its parts, spawned with it: spawn_placement), unless its
// profile gives those slots something of its own
std::vector<uint32_t> linked_classes(int level, uint32_t cls) {
    std::vector<uint32_t> out;
    LdPlacement pl;
    if (!ld_placement(level, -1, static_cast<int32_t>(cls), pl)) return out;
    const Profile* prof = profile_find(level, cls);
    for (const auto& [o, uid] : pl.links) {
        LdPlacement lp;
        uint32_t c = 0;
        if (uid < 0 || profile_default_at(prof, o) || !ld_placement(level, uid, -1, lp) || lp.rec.size() < 0x78) continue;
        std::memcpy(&c, lp.rec.data() + 0x18, 4);
        if (c != cls && std::find(out.begin(), out.end(), c) == out.end()) out.push_back(c);
    }
    return out;
}

std::vector<Need> closure(int base) {
    std::vector<Need> out, todo;
    std::set<std::pair<int, uint32_t>> seen;
    for (const Import& im : g_imports) todo.push_back({im.level, im.cls, "imported (" + im.from + ")", true});
    for (size_t k = 0; k < todo.size(); ++k) {
        const Need n = todo[k];
        if (!seen.insert({n.level, n.cls}).second) continue;
        out.push_back(n);
        if (n.level == base) continue;
        const PoolDeps* d = pool_deps(n.level, n.cls);
        if (d == nullptr) continue;
        const std::string by = std::to_string(n.level) + ":" + std::to_string(n.cls);
        for (uint32_t c : d->own) todo.push_back({n.level, c, "spawned by " + by, false});
        for (uint32_t c : d->engine) todo.push_back({n.level, c, "spawned for " + by + " by the engine", false});
        for (uint32_t c : d->seen) todo.push_back({n.level, c, "seen spawned by " + by, false});
        for (uint32_t c : linked_classes(n.level, n.cls)) todo.push_back({n.level, c, "named by " + by + "'s placement (a part)", false});
    }
    return out;
}

// the report line of an import's dependencies: each with what the session does with it
void deps_report(int base, int level, uint32_t cls) {
    const PoolDeps* d = pool_deps(level, cls);
    if (d == nullptr) {
        std::printf("[session] %s (%d:%u): no dependencies known (no line in pool/deps): only the class itself\n", pool_name_of(level, cls).c_str(), level, cls);
        return;
    }
    auto list = [&](const std::vector<uint32_t>& v) {
        std::string s;
        for (uint32_t c : v) {
            bool imported = false;
            for (const auto& [id, sc] : g_session_ids) imported |= sc.level == level && sc.cls == c;
            s += (s.empty() ? "" : ", ") + std::to_string(c) + (imported ? " imported" : base_has(c) ? " the base's" : " MISSING");
        }
        return s.empty() ? std::string("none") : s;
    };
    std::printf("[session] %s (%d:%u) on level %d's engine: spawns %s; the engine for it %s; seen spawning %s; reads %s (%s)\n", pool_name_of(level, cls).c_str(),
                level, cls, base, list(d->own).c_str(), list(d->engine).c_str(), list(d->seen).c_str(), d->reads.c_str(),
                d->reads == "-" ? "no level data" : "level data: given by stage 4's views");
}

// the session's imports registered with their closure
void register_imports(int level) {
    if (g_imports.empty() || !g_on || !pool_open(true)) return;
    const std::vector<Need> need = closure(level);
    for (const Need& n : need) {
        if (n.level == level) {
            if (n.root) std::printf("[session] import %d:%u: the base's own class (native)\n", n.level, n.cls);
            continue;
        }
        bool done = false;
        for (const auto& [id, sc] : g_session_ids) done |= sc.level == n.level && sc.cls == n.cls;
        if (done) continue;  // (registered already: a death's setup keeps the classes)
        // a dependency the base has: the base's own (a class id is the same class on every level)
        if (!n.root && base_has(n.cls)) continue;
        import_one(n.level, n.cls, n.root ? "imported" : n.why);
    }
    for (const Need& n : need)
        if (n.root && n.level != level) deps_report(level, n.level, n.cls);
    // the imports' sounds checked: each definition's bank sound the pool row's (its level's asset index) and its bank its
    // level's host bank
    size_t classes = 0, defs = 0, bad = 0;
    for (const auto& [id, sc] : g_session_ids) {
        const PoolClass* c = sc.imported ? pool_find(sc.level, sc.cls) : nullptr;
        if (c == nullptr || lwu(sc.hdr + 0x28) == 0) continue;
        ++classes;
        const auto b = g_banks.find(sc.level);
        for (uint32_t j = 0, t = lwu(sc.hdr + 0x28); j < lbu(sc.hdr + 0xd); ++j, ++defs)
            bad += lhu(t + 32 * j + 0x1a) != (j < c->sounds.size() ? c->sounds[j] : 0xffffu) || b == g_banks.end() || lwu(t + 32 * j + 0x1c) != b->second.handle;
    }
    if (classes != 0)
        std::printf("[session] sounds: %zu imported classes, %zu definitions on their levels' banks, %zu differ from the pool's: %s\n", classes, defs, bad,
                    bad == 0 ? "ok" : "FAILED");
    session_memory_log();
}

// every moby of class id `id` taken out of play (the game's moby_retire), their spawned pvars freed: their count
size_t retire_class(uint32_t id) {
    size_t n = 0;
    const uint32_t lo = lwu(OV(0x0015ffd8)), hi = lwu(OV(0x0015ffe0));
    for (uint32_t m = lo; lo != 0 && m <= hi && lbu(m + 0x20) != 0xff; m += 0x100) {
        if (lbu(m + 0x20) >= 0xfd || lhu(m + 0xa6) != id) continue;
        moby_retire(m);
        ++n;
    }
    return n;
}

// class `id` out of the session: its mobys retired, its class table entries cleared (its index given back), its heap blocks
// freed a few frames later (with its context and bank when its level has no class left)
void unload_class(uint32_t id) {
    const auto it = g_session_ids.find(id);
    if (it == g_session_ids.end()) return;
    const SessionClass sc = it->second;
    const size_t n = retire_class(id);
    const uint32_t idx = sc.idx;
    if (cls_entry_ok(idx, id)) {
        sb(OV(0x00197bc0) + id, id < 2048 ? 0xffu : 0u);
        sh(OV(0x00197a00) + idx * 2, 0);
        sw(OV(0x00197300) + idx * 4, 0);
        sw(OV(0x00197680) + idx * 4, 0);
        sw(OV(0x0019a280) + idx * 4, 0);
        g_free_idx.push_back(idx);
    }
    mrp_moby_import_remove(sc.level, sc.cls);
    if (id < 0x1ec0) g_ld_class_level[id] = 0;
    g_session_ids.erase(it);
    g_unload_pending.push_back({pool_owner(sc.level, sc.cls), g_frame + 3});
    bool level_left = false;
    for (const auto& [i, o] : g_session_ids) level_left |= o.level == sc.level;
    if (!level_left) g_unload_pending.push_back({"level " + std::to_string(sc.level), g_frame + 3});
    std::printf("[session] class %d:%u (id %u, index %u) unloaded: %zu mobys of it retired%s\n", sc.level, sc.cls, id, idx, n,
                level_left ? "" : (", level " + std::to_string(sc.level) + "'s context and bank go with it").c_str());
}
}  // namespace

bool g_session_classes = false;
bool g_spawn_watch = false;
uint32_t g_spawn_updating = 0;

// orig: (host) see session.h
void session_level_begin() {
    size_t n = 0;
    std::set<std::string> owners;
    for (const auto& [a, b] : g_blocks)
        if (b.owner.rfind("pool ", 0) == 0 || b.owner == "twin" || b.owner == "spawn") owners.insert(b.owner);
    for (const std::string& o : owners) n += session_heap_free_owner(o);
    g_session_ids.clear();
    g_free_idx.clear();
    g_unload_pending.clear();
    g_spawn_pvars.clear();
    g_session_classes = false;
    g_spawn_watch = spawn_probe_on();
    if (n != 0) std::printf("[session] a level setup: the pool classes', the twin's and the spawns' %zu blocks freed\n", n);
    twin_level_begin();
    profiles_session_clear();
    ld_registry_drop_all();  // (the registries and contexts of the level before: made again by the imports)
    ctx_drop_all();
    for (const auto& [l, b] : g_banks)
        if (b.handle != 0) snd989_host_bank_unload(b.handle);
    g_banks.clear();
    mrp_moby_imports_clear();
}

// orig: (host) see session.h
uint32_t session_class_block(int level, uint32_t cls, uint32_t native) {
    if (cls != g_high_class || g_high_id != 0 || native == 0 || !g_on || !pool_open(true)) return native;
    const PoolClass* c = pool_find(level, cls);
    if (c == nullptr || c->block_size == 0) return native;
    const uint32_t a = heap_copy(*c, "registered in place of the level's own");
    return a != 0 ? a : native;
}

// orig: (host) see session.h
void session_import_add(int level, uint32_t cls) { session_import_request(level, cls, "test"); }

// orig: (host) see session.h
void session_import_request(int level, uint32_t cls, const char* from) {
    for (const auto& im : g_imports)
        if (im.level == level && im.cls == cls) return;
    g_imports.push_back(Import{level, cls, from});
}

// orig: (host) see session.h
void session_level_mod(const std::string& build_dir) {
    // (the mod's imports of the level before go; the testing switch's stay)
    g_imports.erase(std::remove_if(g_imports.begin(), g_imports.end(), [](const Import& im) { return im.from != "test"; }), g_imports.end());
    if (build_dir.empty()) return;
    std::ifstream in(build_dir + "/imports.txt");
    std::string line;
    size_t n = 0;
    while (std::getline(in, line)) {
        // (the composer's lines: the level's imports, "imported (...)", and their closure, which the session makes again from
        // the same dependencies: an import's dependencies go when it goes)
        std::istringstream s(line);
        std::string kind, name, why;
        int l = -1;
        uint32_t c = 0;
        if (s >> kind >> l >> c >> name >> why && kind == "import" && l >= 0 && l < 19 && why == "imported") {
            session_import_request(l, c, "mod");
            ++n;
        }
    }
    if (n != 0 && session_memory_begin()) std::printf("[session] the level mod imports %zu classes (%s/imports.txt)\n", n, build_dir.c_str());
}

// orig: (host) see session.h
void session_classes_register(int level, uint32_t tex) {
    g_tex = tex;
    register_imports(level);
    if (g_high_id == 0 || g_high_class == 0xffffffffu || !g_on || !pool_open(true)) return;
    const PoolClass* c = pool_find(level, g_high_class);
    if (c == nullptr || c->block_size == 0) {
        std::fprintf(stderr, "[session] level %d has no class %u with data: no session class %u\n", level, g_high_class, g_high_id);
        return;
    }
    // (registered already: a death's setup keeps the level's classes. The id -> index map 0x197bc0 is filled with 0xff for
    // the game's ids 0..2047 only; past them, to the moby textures 0x199a80, it is zero in the overlay's image and on every
    // level measured: room for the session ids, written by the registration)
    if (g_session_ids.count(g_high_id) != 0) return;
    const uint32_t remap = session_heap_alloc(16, pool_owner(level, g_high_class));
    const uint32_t a = remap != 0 ? heap_copy(*c, ("registered as session class " + std::to_string(g_high_id)).c_str()) : 0;
    if (a == 0) return;
    std::memcpy(ee_get_ram_base() + remap, c->remap, 16);
    g_session_ids[g_high_id] = SessionClass{level, g_high_class, c->update_fn, c->word2c, a};
    g_session_classes = true;
    moby_class_register(a, tex, remap, g_high_id);
    g_session_ids[g_high_id].idx = lbu(OV(0x00197bc0) + g_high_id);
}

// orig: (host) see session.h
uint32_t session_placed_class(uint32_t cls) { return cls == g_high_class && g_session_ids.count(g_high_id) != 0 ? g_high_id : cls; }

// orig: (host) see session.h
bool session_class_row(uint32_t cls, uint32_t* fn, uint32_t* word) {
    const auto it = g_session_ids.find(cls);
    if (it == g_session_ids.end()) return false;
    *fn = it->second.fn;
    *word = it->second.word;
    return true;
}

// orig: (host) see session.h
bool session_class_source(uint32_t cls, int* level, uint32_t* src) {
    const auto it = g_session_ids.find(cls);
    if (it == g_session_ids.end()) return false;
    *level = it->second.level;
    *src = it->second.cls;
    return true;
}

// orig: (host) see session.h
int32_t session_import_id(int level, uint32_t cls) {
    for (const auto& [id, sc] : g_session_ids)
        if (sc.level == level && sc.cls == cls) return static_cast<int32_t>(id);
    if (level == lw(0x0015ed84) && base_has(cls)) return static_cast<int32_t>(cls);  // (the base's own)
    return -1;
}

// orig: (host) see session.h
int32_t session_import_load(int level, uint32_t cls, const char* from) {
    if (!g_on && !session_memory_begin()) return -1;
    session_import_request(level, cls, from);
    if (g_tex == 0) return -1;  // (no level set up yet: registered at its setup)
    register_imports(lw(0x0015ed84));
    return session_import_id(level, cls);
}

// orig: (host) see session.h
size_t session_import_unload(int level, uint32_t cls) {
    const size_t before = g_imports.size();
    g_imports.erase(std::remove_if(g_imports.begin(), g_imports.end(), [&](const Import& im) { return im.level == level && im.cls == cls; }), g_imports.end());
    if (g_imports.size() == before) return 0;
    // what the remaining imports still need stays; the rest goes (the class and the dependencies only it needed)
    std::set<std::pair<int, uint32_t>> keep;
    for (const Need& n : closure(lw(0x0015ed84))) keep.insert({n.level, n.cls});
    std::vector<uint32_t> go;
    for (const auto& [id, sc] : g_session_ids)
        if (sc.imported && keep.count({sc.level, sc.cls}) == 0) go.push_back(id);
    for (uint32_t id : go) unload_class(id);
    return go.size();
}

namespace {
// a moby of session class `id` (level `level`'s class) from placement `pl` at pos / yaw, as the level's load places one
// (level_load.cpp): its scale, update distance, flags, colour and light, its pvars (their own pointers made addresses, those
// into its level's shared pvar data pointed at its registry's copy, the mobys they name spawned with it - an attached part,
// its own placement moved with it - when the session has their class and its profile gives the slot nothing of its own,
// else none); its spawn rule: drops its bolts when killed. `made`: the level's uids spawned so far (a part named twice, or
// naming its owner, is the one moby)
uint32_t spawn_placement(uint32_t id, int level, const LdPlacement& pl, const float pos[3], float yaw, int depth, std::map<int32_t, uint32_t>& made) {
    const bool have = pl.rec.size() >= 0x78;
    const uint32_t m = moby_spawn(id);
    if (m == 0) return 0;
    if (pl.uid >= 0) made[pl.uid] = m;
    auto r32 = [&](size_t o) {
        uint32_t v = 0;
        std::memcpy(&v, pl.rec.data() + o, 4);
        return v;
    };
    auto rf = [&](size_t o) {
        float v = 0;
        std::memcpy(&v, pl.rec.data() + o, 4);
        return v;
    };
    sb(m + 0xb1, 0xff);
    if (have) {
        sh(m + 0xb4, r32(0x10));
        sb(m + 0xb0, r32(0x4));
        sh(m + 0xb6, r32(0x10));
        if (lw(m + 0x24) != 0) swc1(m + 0x2c, lwc1(lwu(m + 0x24) + 0x24) * rf(0x1c));
        sh(m + 0x32, r32(0x20));
        sb(m + 0x30, r32(0x24));
        swc1(m + 0x40, rf(0x3c));
        swc1(m + 0x44, rf(0x40));
        if (lw(m + 0x24) != 0) sh(lwu(m + 0x24) + 0x44, lhu(lwu(m + 0x24) + 0x44) | (r32(0x60) & 0xffffu));
        sh(m + 0x34, lhu(m + 0x34) | (r32(0x60) & 0xffffu));
        if (r32(0x5c) == 0) sh(m + 0x36, 0);
        const uint32_t col = (r32(0x6c) << 16) + (r32(0x68) << 8) + r32(0x64);
        sw(m + 0x80, col);
        moby_set_color_packed(m, static_cast<int64_t>(static_cast<int32_t>(col)), 0, 0, 0);
        sw(m + 0x38, r32(0x70));
    }
    for (int k = 0; k < 3; ++k) swc1(m + 0x10 + 4 * static_cast<uint32_t>(k), pos[k]);
    swc1(m + 0x48, yaw);
    sb(m + 0x21, 0xff);
    if (have && r32(0x4c) != 0) {
        const float z = floor_probe(0.5f, m + 0x10, 0);
        if (0.0f < z) swc1(m + 0x18, z + rf(0x50));
    }
    if (have && !pl.pvars.empty()) {
        uint32_t pv = lwu(m + 0x78);
        if (pl.pvars.size() > 0x80) {  // (more than the spawn's 0x80 bytes: a block of the heap, freed with the moby)
            pv = session_heap_alloc(static_cast<uint32_t>(pl.pvars.size()), "spawn");
            if (pv == 0) {
                moby_retire(m);
                return 0;
            }
            g_spawn_pvars[m] = pv;
            sw(m + 0x78, pv);
        }
        for (uint32_t i = 0; i < pl.pvars.size(); ++i) sb(pv + i, pl.pvars[i]);
        for (uint32_t o : pl.ptrs)
            if (o + 4 <= pl.pvars.size()) sw(pv + o, lwu(pv + o) + pv);
        const uint32_t shared = ld_shared_data(level);
        for (const auto& [o, d] : pl.shared)
            if (o + 4 <= pl.pvars.size()) sw(pv + o, shared != 0 ? shared + d : 0);
        // the mobys it names: spawned with it (an attached part), unless its profile gives the slot something of its own
        const Profile* prof = profile_find(level, g_session_ids[id].cls);
        for (const auto& [o, uid] : pl.links) {
            if (o + 4 > pl.pvars.size()) continue;
            sw(pv + o, 0xffffffffu);
            if (uid < 0 || depth >= 2 || profile_default_at(prof, o)) continue;
            uint32_t part = 0;
            if (const auto it = made.find(uid); it != made.end()) {
                part = it->second;
            } else {
                LdPlacement lp;
                uint32_t pc = 0;
                if (ld_placement(level, uid, -1, lp) && lp.rec.size() >= 0x78) std::memcpy(&pc, lp.rec.data() + 0x18, 4);
                const int32_t pid = pc != 0 ? session_import_id(level, pc) : -1;
                if (pid >= 0 && have) {
                    // (its place and yaw relative to its owner's on their level, turned with the owner)
                    float lpos[3], opos[3];
                    std::memcpy(lpos, lp.rec.data() + 0x30, 12);
                    std::memcpy(opos, pl.rec.data() + 0x30, 12);
                    const float turn = yaw - rf(0x44), c = std::cos(turn), sn = std::sin(turn);
                    const float dx = lpos[0] - opos[0], dy = lpos[1] - opos[1];
                    const float at[3] = {pos[0] + dx * c - dy * sn, pos[1] + dx * sn + dy * c, pos[2] + lpos[2] - opos[2]};
                    float lyaw = 0;
                    std::memcpy(&lyaw, lp.rec.data() + 0x44, 4);
                    part = spawn_placement(static_cast<uint32_t>(pid), level, lp, at, std::remainder(lyaw + turn, 6.2831853f), depth + 1, made);
                }
            }
            if (part != 0) sw(pv + o, (part - lwu(OV(0x0015ffd8))) >> 8);
        }
    }
    if (lw(m + 0x24) != 0) moby_update_transform(m);
    return m;
}
}  // namespace

// orig: (host) see session.h
uint32_t session_import_spawn(uint32_t id, const float pos[3], float yaw) {
    const auto it = g_session_ids.find(id);
    if (it == g_session_ids.end()) return 0;
    const int level = it->second.level;
    const uint32_t cls = it->second.cls;
    // (its first placement on its own level: the pool's level data, else the pool's placements - no parts nor shared data then)
    LdPlacement pl;
    if (!ld_placement(level, -1, static_cast<int32_t>(cls), pl)) {
        PoolPlacement pp;
        if (pool_placement(level, cls, pp)) {
            pl.rec = pp.rec;
            pl.pvars = pp.pvars;
            pl.ptrs = pp.ptrs;
            for (uint32_t o : pp.links) pl.links.push_back({o, -1});
            for (uint32_t o : pp.shared) pl.shared.push_back({o, 0});
        }
    }
    std::map<int32_t, uint32_t> made;
    const uint32_t m = spawn_placement(id, level, pl, pos, yaw, 0, made);
    // its profile's defaults (its paths in its context's registry: class_profiles.h)
    if (m != 0) profiles_session_spawn(level, cls, m, pos, yaw);
    return m;
}

// orig: (host) see session.h
bool session_spawn_check(uint32_t cls) {
    if (g_spawn_probe) spawn_probe_note(cls);
    if (!g_session_classes || cls >= 0x1ec0) return true;
    const uint32_t idx = lbu(OV(0x00197bc0) + cls);
    if (cls < 2048 ? idx != 0xffu : g_session_ids.count(cls) != 0) return true;
    static std::set<std::pair<uint32_t, uint32_t>> told;
    const uint32_t by = g_spawn_updating != 0 ? lhu(g_spawn_updating + 0xa6) : 0xffffu;
    if (told.insert({by, cls}).second) {
        int l = -1;
        uint32_t src = by;
        session_class_source(by, &l, &src);
        std::fprintf(stderr, "[session] a moby of class %u%s spawned class %u, which the session lacks: refused (a dependency its closure misses)\n", by,
                     l >= 0 ? (" (" + std::to_string(l) + ":" + std::to_string(src) + ")").c_str() : "", cls);
    }
    return false;
}

namespace {
// the residency test (RAC1_SESSION_CYCLE=<level>:<class>@<frame>[,<rounds>[,<period>]]): from <frame>, in play, the class
// unloaded and loaded again <rounds> times (100), <period> frames apart (12), two of it spawned in front of the hero after
// each load; the heap, the class table and the renderer's import list logged at the start, after the first round and at the
// end: the end must match the first round's (the same classes and spawned mobys)
struct Cycle {
    int level = -1;
    uint32_t cls = 0;
    uint64_t at = 0, period = 12;
    int rounds = 100, done = 0;
    bool loaded = true, started = false;
    uint32_t used0 = 0, count0 = 0;
    size_t blocks0 = 0, runs0 = 0;
    int mrp0 = 0;
} g_cycle;

void cycle_frame(uint64_t frame) {
    Cycle& c = g_cycle;
    if (c.level < 0 || c.done >= c.rounds || frame < c.at || lw(OV(0x0015f5c4)) != 0) return;
    if (!c.started) {
        if (session_import_id(c.level, c.cls) < 0) return;
        c.started = true;
        c.at = frame;
        c.used0 = session_heap_used();
        c.blocks0 = g_blocks.size();
        c.runs0 = g_free.size();
        c.count0 = lwu(OV(0x0015ffc0));
        c.mrp0 = mrp_moby_imports_count();
        std::printf("[session] cycle test: %d:%u, %d rounds; at the start: heap %#x in %zu blocks (%zu free runs), class table %u, the MRP's imports %d\n",
                    c.level, c.cls, c.rounds, c.used0, c.blocks0, c.runs0, c.count0, c.mrp0);
    }
    if ((frame - c.at) % c.period != 0) return;
    if (c.loaded) {
        const size_t n = session_import_unload(c.level, c.cls);
        c.loaded = false;
        if (n == 0) std::fprintf(stderr, "[session] cycle test: %d:%u was not unloaded\n", c.level, c.cls);
        return;
    }
    const int32_t id = session_import_load(c.level, c.cls, "test");
    c.loaded = true;
    ++c.done;
    const uint32_t h = lwu(g::HERO_MOBY);
    for (int k = 0; k < 2 && id >= 0; ++k) {
        const float yaw = lwc1(h + 0x48);
        const float at[3] = {lwc1(h + 0x10) + (6.0f + 2.0f * k) * std::cos(yaw), lwc1(h + 0x14) + (6.0f + 2.0f * k) * std::sin(yaw), lwc1(h + 0x18) + 1.0f};
        session_import_spawn(static_cast<uint32_t>(id), at, yaw + 3.14159f);
    }
    if (c.done == 1) {  // (the reference: after the first round, its two spawned with their pvars, as after every round)
        c.used0 = session_heap_used();
        c.blocks0 = g_blocks.size();
        c.runs0 = g_free.size();
        c.count0 = lwu(OV(0x0015ffc0));
        c.mrp0 = mrp_moby_imports_count();
    }
    if (c.done % 10 == 0 || c.done == c.rounds)
        std::printf("[session] cycle test: round %d: heap %#x in %zu blocks (%zu free runs), class table %u, the MRP's imports %d\n", c.done,
                    session_heap_used(), g_blocks.size(), g_free.size(), lwu(OV(0x0015ffc0)), mrp_moby_imports_count());
    if (c.done == c.rounds) {
        // (the classes as at the start: the heap then must be the same size, after the frees pending)
        c.at = frame;
        c.level = -c.level - 2;  // (finished: checked below once the pending frees are done)
    }
}
void cycle_check() {
    Cycle& c = g_cycle;
    if (c.level >= -1 || !g_unload_pending.empty()) return;
    const int lv = -c.level - 2;
    c.level = -1;
    const bool ok = session_heap_used() == c.used0 && g_blocks.size() == c.blocks0 && lwu(OV(0x0015ffc0)) == c.count0 && mrp_moby_imports_count() == c.mrp0;
    std::printf("[session] cycle test %d:%u: %d rounds; heap %#x in %zu blocks (%zu free runs), class table %u, the MRP's imports %d: %s\n", lv, c.cls,
                c.done, session_heap_used(), g_blocks.size(), g_free.size(), lwu(OV(0x0015ffc0)), mrp_moby_imports_count(),
                ok ? "ok (as after the first round)" : "DIFFERENT from the first round");
}
}  // namespace

// orig: (host) see session.h
void session_frame(uint64_t frame) {
    g_frame = frame;
    if (g_spawn_probe && frame % 600 == 0) spawn_probe_end();  // (the probe's lines so far)
    session_rt_test_frame(frame);  // (testing: RAC1_SESSION_SAVE_AT / _LOAD_AT, nothing unless set)
    if (!g_on) return;
    if (g_session_rt) session_rt_frame(frame);  // (a format 3 level's session: session_runtime.h)
    cycle_frame(frame);
    cycle_check();
    // the spawns' heap pvars of mobys gone (out of play, or another class in the slot now)
    for (auto it = g_spawn_pvars.begin(); it != g_spawn_pvars.end();) {
        const uint32_t m = it->first;
        if (lbu(m + 0x20) >= 0xfd || lwu(m + 0x78) != it->second) {
            session_heap_free(it->second);
            it = g_spawn_pvars.erase(it);
        } else {
            ++it;
        }
    }
    if (g_unload_pending.empty()) return;
    for (auto it = g_unload_pending.begin(); it != g_unload_pending.end();) {
        if (frame < it->second) {
            ++it;
            continue;
        }
        const std::string what = it->first;
        it = g_unload_pending.erase(it);
        if (what.rfind("level ", 0) == 0) {
            const int l = std::atoi(what.c_str() + 6);
            bool again = false;  // (a class of the level imported again meanwhile)
            for (const auto& [id, sc] : g_session_ids) again |= sc.level == l;
            if (again) continue;
            ld_registry_drop(l);
            ctx_drop(l);
            if (const auto b = g_banks.find(l); b != g_banks.end()) {
                if (b->second.handle != 0) snd989_host_bank_unload(b->second.handle);
                g_banks.erase(b);
            }
        } else {
            session_heap_free_owner(what);
        }
    }
    if (g_unload_pending.empty()) session_memory_log();
}

// orig: (host) see session.h
void session_memory_log() {
    if (!g_on) return;
    uint32_t classes = 0, contexts = 0, other = 0, ld = 0;
    std::map<std::string, uint32_t> by;
    for (const auto& [a, b] : g_blocks) {
        if (b.owner.rfind("pool ", 0) == 0) classes += b.size;
        else if (b.owner.rfind("context ", 0) == 0) contexts += b.size;
        else if (b.owner.rfind("level data ", 0) == 0) ld += b.size;
        else other += b.size;
    }
    size_t bank_bytes = 0, imported = 0;
    for (const auto& [l, b] : g_banks) bank_bytes += b.bytes;
    for (const auto& [id, sc] : g_session_ids) imported += sc.imported;
    uint32_t free = 0;
    for (const auto& [a, n] : g_free) free += n;
    std::printf("[session] memory: %zu classes imported (%u KB), %zu contexts (%u KB), their level data %u KB, other blocks %u KB (spawned pvars, the twin's); "
                "heap %u KB used, %u KB free in %zu runs; %zu sound banks %zu KB (host memory); class table %u of 224 (%zu indices free); the MRP's import "
                "list %d classes\n",
                imported, classes >> 10, static_cast<size_t>(ctx_count()), contexts >> 10, ld >> 10, other >> 10, session_heap_used() >> 10, free >> 10, g_free.size(),
                g_banks.size(), bank_bytes >> 10, lwu(OV(0x0015ffc0)), g_free_idx.size(), mrp_moby_imports_count());
}

// ---- the spawn probe (RAC1_DEPS_PROBE=1, testing): which class spawns which, on the game's own levels ----
bool g_spawn_probe = false;
namespace {
std::map<std::pair<uint32_t, uint32_t>, size_t> g_probe;  // (spawner class or 0xffff: no moby's update, spawned class) -> count
int g_probe_level = -1;
void probe_log() {
    if (g_probe.empty()) return;
    std::map<uint32_t, std::string> by;
    for (const auto& [k, n] : g_probe) by[k.first] += (by[k.first].empty() ? "" : ",") + std::to_string(k.second) + "x" + std::to_string(n);
    for (const auto& [c, s] : by)
        std::printf("[deps] probe level %d: %s spawned %s\n", g_probe_level, c == 0xffffu ? "no moby's update" : ("class " + std::to_string(c)).c_str(), s.c_str());
}
}  // namespace
bool spawn_probe_on() { return g_spawn_probe; }
void spawn_probe_note(uint32_t cls) {
    if (g_probe_level != lw(0x0015ed84)) {
        probe_log();
        g_probe.clear();
        g_probe_level = lw(0x0015ed84);
    }
    ++g_probe[{g_spawn_updating != 0 ? lhu(g_spawn_updating + 0xa6) : 0xffffu, cls}];
}
void spawn_probe_end() { probe_log(); }

// orig: (host) see session.h
void session_start_tests() {
    twin_start_tests();  // (RAC1_SESSION_TWIN: twin.h)
    if (const char* t = std::getenv("RAC1_SESSION_HEAP_TEST"); t != nullptr && std::atoi(t) != 0) heap_test();
    const char* e = std::getenv("RAC1_SESSION_MEM");
    if (e != nullptr && std::atoi(e) != 0) session_memory_begin();
    if (const char* im = std::getenv("RAC1_SESSION_IMPORT"); im != nullptr && *im != 0) {  // (testing: <level>:<class>,...)
        std::string list = im;
        for (size_t p = 0; p < list.size();) {
            const size_t q = std::min(list.find(',', p), list.size());
            int l = 0;
            uint32_t c = 0;
            if (pool_name(list.substr(p, q - p), &l, &c)) session_import_add(l, c);
            else std::fprintf(stderr, "[session] RAC1_SESSION_IMPORT: %s is not <level>:<class> or a name of data/pool/names.txt\n", list.substr(p, q - p).c_str());
            p = q + 1;
        }
        if (!g_imports.empty() && session_memory_begin()) std::printf("[session] test: %zu classes imported from other levels\n", g_imports.size());
    }
    if (const char* e2 = std::getenv("RAC1_DEPS_PROBE"); e2 != nullptr && std::atoi(e2) != 0) {  // (testing: who spawns what)
        g_spawn_probe = true;
        g_spawn_watch = true;
        std::atexit(spawn_probe_end);
        std::printf("[deps] probe: every class a moby's update spawns, logged per level\n");
    }
    if (const char* cy = std::getenv("RAC1_SESSION_CYCLE"); cy != nullptr && *cy != 0) {  // (testing: unload and load again)
        char name[64] = {};
        unsigned long long at = 0, period = 12;
        int rounds = 100;
        int l = 0;
        uint32_t c = 0;
        if (std::sscanf(cy, "%63[^@]@%llu,%d,%llu", name, &at, &rounds, &period) >= 2 && pool_name(name, &l, &c) && rounds > 0 && period >= 6) {
            g_cycle.level = l;
            g_cycle.cls = c;
            g_cycle.at = at;
            g_cycle.rounds = rounds;
            g_cycle.period = period;
            session_import_add(l, c);
            session_memory_begin();
        } else {
            std::fprintf(stderr, "[session] RAC1_SESSION_CYCLE=%s: <level>:<class>@<frame>[,<rounds>[,<period> (6 or more)]]\n", cy);
        }
    }
    if (const char* h = std::getenv("RAC1_SESSION_HIGH_CLASS"); h != nullptr && *h != 0) {
        unsigned c = 0, id = 0;
        const int n = std::sscanf(h, "%u:%u", &c, &id);
        if (n < 1 || (n == 2 && (id < 2048 || id > 7871))) {
            std::fprintf(stderr, "[session] RAC1_SESSION_HIGH_CLASS=%s: <class> or <class>:<session id 2048..7871>\n", h);
        } else if (session_memory_begin()) {
            g_high_class = c;
            g_high_id = n == 2 ? id : 0;
            std::printf("[session] test: class %u from the session heap%s\n", c, n == 2 ? (", as session class " + std::to_string(id)).c_str() : "");
        }
    }
}

// orig: (host) see session.h
uint32_t session_heap_alloc(uint32_t size, const std::string& owner) {
    if (!g_on || size == 0) return 0;
    const uint32_t n = (size + SESSION_HEAP_ALIGN - 1) & ~(SESSION_HEAP_ALIGN - 1);
    for (auto it = g_free.begin(); it != g_free.end(); ++it) {  // (first fit: a session holds tens of blocks, not thousands)
        if (it->second < n) continue;
        const uint32_t a = it->first, left = it->second - n;
        g_free.erase(it);
        if (left != 0) g_free[a + n] = left;
        g_blocks[a] = Block{n, owner};
        if (trace()) std::printf("[session] heap: %08x+%#x for %s\n", a, n, owner.c_str());
        return a;
    }
    std::fprintf(stderr, "[session] heap full: %#x bytes for %s refused (%#x in use)\n", size, owner.c_str(), session_heap_used());
    return 0;
}

// orig: (host) see session.h
bool session_heap_free(uint32_t addr) {
    const auto it = g_blocks.find(addr);
    if (it == g_blocks.end()) return false;
    const uint32_t n = it->second.size;
    if (trace()) std::printf("[session] heap: %08x+%#x of %s freed\n", addr, n, it->second.owner.c_str());
    std::memset(ee_get_ram_base() + addr, 0, n);  // (a block is zero-filled when given)
    g_blocks.erase(it);
    free_run(addr, n);
    return true;
}

// orig: (host) see session.h
size_t session_heap_free_owner(const std::string& owner) {
    size_t k = 0;
    for (auto it = g_blocks.begin(); it != g_blocks.end();) {
        const uint32_t a = it->first;
        ++it;
        if (g_blocks.at(a).owner == owner) {
            session_heap_free(a);
            ++k;
        }
    }
    return k;
}

// orig: (host) see session.h
uint32_t session_heap_used() {
    uint32_t n = 0;
    for (const auto& [a, b] : g_blocks) n += b.size;
    return n;
}

// orig: (host) see session.h
size_t session_heap_blocks() { return g_blocks.size(); }

// orig: (host) see session.h
void session_heap_log() {
    uint32_t free = 0;
    for (const auto& [a, n] : g_free) free += n;
    std::printf("[session] heap: %#x bytes in %zu blocks, %#x free in %zu runs\n", session_heap_used(), g_blocks.size(), free, g_free.size());
    std::map<std::string, std::pair<uint32_t, size_t>> by;
    for (const auto& [a, b] : g_blocks) {
        by[b.owner].first += b.size;
        ++by[b.owner].second;
    }
    for (const auto& [o, v] : by) std::printf("[session]   %s: %#x bytes in %zu blocks\n", o.c_str(), v.first, v.second);
}

// orig: (host) see session.h
uint64_t session_heap_hash() {
    uint64_t h = 1469598103934665603ull;
    auto mix = [&](const uint8_t* p, size_t n) {
        for (size_t i = 0; i < n; ++i) {
            h ^= p[i];
            h *= 1099511628211ull;
        }
    };
    const uint8_t* ram = ee_get_ram_base();
    for (const auto& [a, b] : g_blocks) {
        mix(reinterpret_cast<const uint8_t*>(&a), 4);
        mix(reinterpret_cast<const uint8_t*>(&b.size), 4);
        mix(ram + a, b.size);
    }
    return h;
}

}  // namespace rac1
