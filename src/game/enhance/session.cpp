// See session.h.
#include <game/enhance/session.h>

#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <iterator>
#include <map>

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

// orig: (host) see session.h
void session_start_tests() {
    if (const char* t = std::getenv("RAC1_SESSION_HEAP_TEST"); t != nullptr && std::atoi(t) != 0) heap_test();
    const char* e = std::getenv("RAC1_SESSION_MEM");
    if (e != nullptr && std::atoi(e) != 0) session_memory_begin();
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
