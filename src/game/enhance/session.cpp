// See session.h.
#include <game/enhance/session.h>

#include <game/enhance/context.h>
#include <game/enhance/pool.h>
#include <game/enhance/twin.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/iop/snd989_iop.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <algorithm>
#include <map>
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
// the session classes: id -> source and row
struct SessionClass {
    int level;
    uint32_t cls, fn, word, hdr;
};
std::map<uint32_t, SessionClass> g_session_ids;
// the classes a session imports from other levels (RAC1_SESSION_IMPORT, the twin run; later level.txt's `import`)
std::vector<std::pair<int, uint32_t>> g_imports;
// their levels' sound banks, loaded as host banks (the level's whole bank for now): level -> handle
std::map<int, uint32_t> g_banks;

// level `level`'s bank as a host bank (loaded once), 0 when it cannot be had
uint32_t level_bank(int level) {
    if (const auto it = g_banks.find(level); it != g_banks.end()) return it->second;
    std::vector<uint8_t> b;
    const uint32_t h = pool_sound_bank(level, b) ? snd989_host_bank_load(b) : 0;
    g_banks[level] = h;
    if (h == 0) std::fprintf(stderr, "[session] level %d's sound bank could not be loaded: its classes are silent\n", level);
    else std::printf("[session] level %d's sound bank loaded (%zu KB): host bank %#x\n", level, b.size() >> 10, h);
    return h;
}

// a pool class's block copied into the session heap (its owner "pool L:C"): its address, 0 when it cannot be had
uint32_t heap_copy(const PoolClass& c, const char* why) {
    const uint8_t* b = pool_block(c);
    const std::string owner = "pool " + std::to_string(c.level) + ":" + std::to_string(c.id);
    const uint32_t a = b != nullptr ? session_heap_alloc(c.block_size, owner) : 0;
    if (a == 0) {
        std::fprintf(stderr, "[session] class %d:%u: no room for its block (%#x bytes)\n", c.level, c.id, c.block_size);
        return 0;
    }
    std::memcpy(ee_get_ram_base() + a, b, c.block_size);
    std::printf("[session] class %d:%u's pool block at %08x (%#x bytes): %s\n", c.level, c.id, a, c.block_size, why);
    return a;
}
}  // namespace

bool g_session_classes = false;

// orig: (host) see session.h
void session_level_begin() {
    size_t n = 0;
    std::map<std::string, int> owners;
    for (const auto& [a, b] : g_blocks)
        if (b.owner.rfind("pool ", 0) == 0) owners[b.owner] = 1;
    for (const auto& [o, k] : owners) n += session_heap_free_owner(o);
    g_session_ids.clear();
    g_session_classes = false;
    if (n != 0) std::printf("[session] a level setup: the pool classes' %zu blocks freed\n", n);
    ctx_drop_all();  // (the contexts of the level before: made again by the imports)
    for (const auto& [l, h] : g_banks)
        if (h != 0) snd989_host_bank_unload(h);
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

namespace {
// the session's imports registered: each class of another level from its pool block, under its own id (a second copy of
// a class the base has: a session id), its update function and +0x2c word through its level's context
void register_imports(int level, uint32_t tex) {
    if (g_imports.empty() || !g_on || !pool_open(true)) return;
    for (const auto& [from, cls] : g_imports) {
        if (from == level) continue;  // (the base's own class: native)
        const PoolClass* c = pool_find(from, cls);
        if (c == nullptr || c->block_size == 0) {
            std::fprintf(stderr, "[session] import %d:%u: no such class with data in the pool\n", from, cls);
            continue;
        }
        bool done = false;
        for (const auto& [id, sc] : g_session_ids) done |= sc.level == from && sc.cls == cls;
        if (done) continue;  // (registered already: a death's setup keeps the classes)
        if (lwu(OV(0x0015ffc0)) >= 224) {
            std::fprintf(stderr, "[session] import %d:%u: the class table is full (224)\n", from, cls);
            continue;
        }
        // its own id (a game class id is the same class on every level: the engine and the class's code test it), unless
        // the base has it: then a session id (2048..), for a second copy
        uint32_t id = cls;
        if (cls >= 2048 || lbu(OV(0x00197bc0) + cls) != 0xffu) {
            id = 2048;
            while (g_session_ids.count(id) != 0 || lbu(OV(0x00197bc0) + id) != 0) ++id;
            std::printf("[session] import %d:%u: the base has class %u: imported as session class %u\n", from, cls, cls, id);
        }
        if (!ctx_create(from)) continue;
        const std::string own = "pool " + std::to_string(from) + ":" + std::to_string(cls);
        const uint32_t remap = session_heap_alloc(16, own);
        const uint32_t a = remap != 0 ? heap_copy(*c, ("imported from level " + std::to_string(from)).c_str()) : 0;
        if (a == 0) continue;
        // (its texture slots on the base's texture 0: the faithful renderer draws a session's classes wrongly; the modern
        // renderer draws it from its own level's class file, the import list)
        for (uint32_t k = 0; k < 16; ++k) sb(remap + k, c->remap[k] == 0xffu ? 0xffu : 0u);
        const uint32_t fn = ctx_xlate(from, c->update_fn), word = ctx_xlate(from, c->word2c);
        g_session_ids[id] = SessionClass{from, cls, fn, word, a};
        g_session_classes = true;
        moby_class_register(a, tex, remap, id);
        mrp_moby_import(from, cls);
        // its sounds: its definitions' bank sounds (the pool's row: its level's asset index) in its level's bank
        if (lwu(a + 0x28) != 0 && lbu(a + 0xd) != 0) {
            const uint32_t bank = level_bank(from), t = lwu(a + 0x28);
            for (uint32_t j = 0; j < lbu(a + 0xd); ++j) {
                sh(t + 32 * j + 0x1a, j < c->sounds.size() && bank != 0 ? c->sounds[j] : 0xffffu);
                sw(t + 32 * j + 0x1c, bank);
            }
        }
        std::printf("[session] import %d:%u as class %u (index %u): update %08x -> %08x, +0x2c %08x -> %08x\n", from, cls, id,
                    lbu(OV(0x00197bc0) + id), c->update_fn, fn, c->word2c, word);
    }
}
}  // namespace

// orig: (host) see session.h
void session_import_add(int level, uint32_t cls) {
    for (const auto& im : g_imports)
        if (im.first == level && im.second == cls) return;
    g_imports.emplace_back(level, cls);
}

// orig: (host) see session.h
void session_classes_register(int level, uint32_t tex) {
    register_imports(level, tex);
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
    const uint32_t remap = session_heap_alloc(16, "pool " + std::to_string(level) + ":" + std::to_string(g_high_class));
    const uint32_t a = remap != 0 ? heap_copy(*c, ("registered as session class " + std::to_string(g_high_id)).c_str()) : 0;
    if (a == 0) return;
    std::memcpy(ee_get_ram_base() + remap, c->remap, 16);
    g_session_ids[g_high_id] = SessionClass{level, g_high_class, c->update_fn, c->word2c, a};
    g_session_classes = true;
    moby_class_register(a, tex, remap, g_high_id);
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
void session_start_tests() {
    twin_start_tests();  // (RAC1_SESSION_TWIN: twin.h)
    if (const char* t = std::getenv("RAC1_SESSION_HEAP_TEST"); t != nullptr && std::atoi(t) != 0) heap_test();
    const char* e = std::getenv("RAC1_SESSION_MEM");
    if (e != nullptr && std::atoi(e) != 0) session_memory_begin();
    if (const char* im = std::getenv("RAC1_SESSION_IMPORT"); im != nullptr && *im != 0) {  // (testing: <level>:<class>,...)
        std::string list = im;
        for (size_t p = 0; p < list.size();) {
            const size_t q = std::min(list.find(',', p), list.size());
            unsigned l = 0, c = 0;
            if (std::sscanf(list.substr(p, q - p).c_str(), "%u:%u", &l, &c) == 2 && l < 19) session_import_add(static_cast<int>(l), c);
            else std::fprintf(stderr, "[session] RAC1_SESSION_IMPORT: %s is not <level>:<class>\n", list.substr(p, q - p).c_str());
            p = q + 1;
        }
        if (!g_imports.empty() && session_memory_begin()) std::printf("[session] test: %zu classes imported from other levels\n", g_imports.size());
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
