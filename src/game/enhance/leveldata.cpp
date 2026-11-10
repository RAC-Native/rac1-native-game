// See leveldata.h.
#include <game/enhance/leveldata.h>

#include <game/enhance/context.h>
#include <game/enhance/pool.h>
#include <game/enhance/session.h>
#include <game/enhance/session_runtime.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <vector>

namespace rac1 {

uint8_t g_ld_class_level[0x1ec0];
bool g_ld_any = false;

namespace {

int32_t rd32(const std::vector<uint8_t>& g, size_t o) {
    int32_t v = 0;
    if (o + 4 <= g.size()) std::memcpy(&v, g.data() + o, 4);
    return v;
}

// The base's own objects (the session's view): the paths added from the table's end live in the mods' block of EE memory
// (0xdf000..0xe0000, below the level mods' texts: level_slots.cpp), as the SDK's ABI 5 put Kalebo III's enemies' paths there
constexpr uint32_t BASE_MEM = 0x000df000u, BASE_MEM_END = 0x000e0000u;
constexpr OvConst<LD_PATH_TABLE> PATH_TABLE{};
constexpr OvConst<0x00160104u> PATH_COUNT{};
uint32_t g_base_mem = BASE_MEM;
int32_t g_base_slot = LD_PATH_MAX;
// (a session, format 3: the base's view's own paths and groups in the session heap instead of the mods' block; the slots
// given back are taken again first)
const char* const BASE_OWNER = "level data base";
std::map<int32_t, uint32_t> g_base_added;  // path index -> its heap block
std::vector<int32_t> g_base_free;

// A context's registry: one block of the session heap (its owner "level data L") holding the objects, in the layout below,
// its level's level data after them; the paths added later each a block of their own
constexpr uint32_t R_PATHS = 0x000, R_VOLUMES = 0x2c0, R_CAMERAS = 0x300, R_RAILS = 0x308, R_GROUP_COUNT = 0x310, R_GROUPS = 0x340,
                   R_DATA = 0x500;
struct Registry {
    int level = 0;
    uint32_t block = 0, size = 0;
    int32_t own_paths = 0;          // its level's own paths (indices 0..own_paths-1)
    int32_t next_slot = LD_PATH_MAX;  // paths added from the end
    std::vector<int32_t> free_slots;
    std::map<int32_t, uint32_t> added;  // path index -> its block
    uint32_t shared = 0;            // its level's shared pvar data (a block)
};
std::unique_ptr<Registry> g_reg[32];

std::string owner(int level) { return "level data " + std::to_string(level); }

uint32_t align16(uint32_t v) { return (v + 15u) & ~15u; }

// the registry's objects (offset in its block, Veldin's object, size)
struct Obj {
    uint32_t off, veldin, size;
};
constexpr Obj OBJS[] = {{R_PATHS, LD_PATH_TABLE, LD_PATH_MAX * 4}, {R_VOLUMES, LD_VOLUMES, LD_VOLUMES_SIZE}, {R_CAMERAS, LD_CAMERAS, 8},
                        {R_RAILS, LD_RAILS, 8},                     {R_GROUP_COUNT, LD_GROUP_COUNT, 4},       {R_GROUPS, LD_GROUPS, LD_GROUP_MAX * 4}};

// the level data of `g` (a gameplay file) laid out in the registry at `r` (the block's start), its data from R_DATA: the
// paths, cameras (and their pvars), the volumes and the three lists, the rails, as the level's load lays them out
// (level_load.cpp); the groups none (they name the level's own mobys). Returns the bytes used (0: measure only, when `r` 0)
uint32_t lay_out(const std::vector<uint8_t>& g, uint32_t r, int32_t* own_paths) {
    auto H = [&](uint32_t f) { return static_cast<uint32_t>(rd32(g, f)); };
    uint32_t at = R_DATA;
    auto put = [&](uint32_t off, const uint8_t* src, uint32_t n) {
        if (r != 0) std::memcpy(ee_get_ram_base() + r + off, src, n);
    };
    auto w32 = [&](uint32_t off, uint32_t v) {
        if (r != 0) sw(r + off, v);
    };
    const uint32_t tab = H(0x54), data = H(0x58);
    auto pvars_of = [&](int32_t idx) -> uint32_t {  // (a pvar block copied: its address)
        if (idx < 0 || tab == 0 || data == 0) return 0;
        const int32_t o = rd32(g, tab + 8 * static_cast<uint32_t>(idx)), n = rd32(g, tab + 8 * static_cast<uint32_t>(idx) + 4);
        if (o < 0 || n <= 0 || data + static_cast<uint32_t>(o) + static_cast<uint32_t>(n) > g.size()) return 0;
        const uint32_t a = at;
        put(a, g.data() + data + o, static_cast<uint32_t>(n));
        at = align16(at + static_cast<uint32_t>(n));
        return r + a;
    };
    *own_paths = 0;
    // the cameras
    if (const uint32_t p0 = H(0x8); p0 != 0 && p0 + 0x10 <= g.size()) {
        const uint32_t c = static_cast<uint32_t>(rd32(g, p0));
        const uint32_t base = at;
        at = align16(at + c * 0x20);
        w32(R_CAMERAS, r + base);
        w32(R_CAMERAS + 4, c);
        uint32_t p = p0 + 0x10;
        for (uint32_t k = 0; k < c && p + 0x20 <= g.size(); ++k, p += 0x20) {
            const uint32_t e = base + k * 0x20;
            w32(e + 0xc, H(p));
            for (uint32_t o = 0; o < 0xc; o += 4) w32(e + o, H(p + 4 + o));
            for (uint32_t o = 0; o < 0xc; o += 4) w32(e + 0x10 + o, H(p + 0x10 + o));
            w32(e + 0x1c, pvars_of(rd32(g, p + 0x1c)));
        }
    }
    // the paths
    if (const uint32_t p = H(0x70); p != 0 && p + 0x10 <= g.size()) {
        const int32_t n = std::min<int32_t>(rd32(g, p), static_cast<int32_t>(LD_PATH_MAX));
        const uint32_t src = p + H(p + 4), size = H(p + 8);
        if (src + size <= g.size() && n >= 0) {
            const uint32_t d = at;
            put(d, g.data() + src, size);
            at = align16(at + size);
            w32(R_VOLUMES + 0x20, static_cast<uint32_t>(n));
            w32(R_VOLUMES + 0x24, r + d);
            for (int32_t k = 0; k < n; ++k) w32(R_PATHS + 4 * static_cast<uint32_t>(k), r + d + H(p + 0x10 + 4 * static_cast<uint32_t>(k)));
            *own_paths = n;
        }
    }
    // the rails
    if (const uint32_t p0 = H(0x74); p0 != 0 && p0 + 0x10 <= g.size()) {
        const int32_t n = rd32(g, p0);
        const uint32_t src = p0 + H(p0 + 4), size = H(p0 + 8);
        if (n >= 0 && src + size <= g.size()) {
            const uint32_t recs = at;
            at = align16(at + static_cast<uint32_t>(n) * 0x20);
            const uint32_t pts = at;
            put(pts, g.data() + src, size);
            at = align16(at + size);
            uint32_t p = p0 + 0x10;
            for (int32_t k = 0; k < n; ++k, p += 0x20) {
                const uint32_t e = recs + static_cast<uint32_t>(k) * 0x20;
                put(e, g.data() + p, 0x10);
                w32(e + 0x14, H(p + 0x10));
            }
            for (int32_t k = 0; k < n; ++k, p += 4) w32(recs + static_cast<uint32_t>(k) * 0x20 + 0x10, r + pts + H(p));
            w32(R_RAILS, r + recs);
            w32(R_RAILS + 4, static_cast<uint32_t>(n));
        }
    }
    // the 0x90-byte records (read 0x80 apart, as the load does) and the three lists of 0x80-byte records
    if (const uint32_t p = H(0x6c); p != 0 && p + 0x10 <= g.size()) {
        const uint32_t n = H(p);
        const uint32_t d = at;
        at = align16(at + n * 0x90);
        for (uint32_t k = 0; k < n && p + 0x10 + k * 0x80 + 0x90 <= g.size(); ++k) put(d + k * 0x90, g.data() + p + 0x10 + k * 0x80, 0x90);
        w32(R_VOLUMES, r + d);
        w32(R_VOLUMES + 4, n);
    }
    for (uint32_t l = 0; l < 3; ++l) {
        const uint32_t p = H(0x60 + 4 * l);
        if (p == 0 || p + 0x10 > g.size()) continue;
        const uint32_t n = H(p);
        const uint32_t d = at;
        if (p + 0x10 + n * 0x80 <= g.size()) put(d, g.data() + p + 0x10, n * 0x80);
        at = align16(at + n * 0x80);
        w32(R_VOLUMES + 8 + 8 * l, r + d);
        w32(R_VOLUMES + 12 + 8 * l, n);
    }
    return at;
}

}  // namespace

// orig: (host) see leveldata.h
bool ld_registry_has(int level) { return level >= 0 && level < 32 && g_reg[level] != nullptr; }

// orig: (host) see leveldata.h
bool ld_registry_make(int level) {
    if (level < 0 || level >= 19) return false;
    if (g_reg[level] != nullptr || level == lw(0x0015ed84)) return true;
    if (!ctx_exists(level)) return false;
    std::vector<uint8_t> g;
    if (!pool_leveldata(level, g)) {
        std::fprintf(stderr, "[ld] level %d: no level data in the pool: its classes read an empty registry\n", level);
        g.clear();
    }
    auto R = std::make_unique<Registry>();
    R->level = level;
    int32_t n = 0;
    R->size = g.empty() ? R_DATA : lay_out(g, 0, &n);
    R->block = session_heap_alloc(R->size, owner(level));
    if (R->block == 0) return false;
    if (!g.empty()) lay_out(g, R->block, &R->own_paths);
    // its shared pvar data (the level's block: (size, count, -, -), the data, then the pointers)
    if (const uint32_t p = static_cast<uint32_t>(rd32(g, 0x4c)); p != 0 && p + 0x10 <= g.size()) {
        const uint32_t size = static_cast<uint32_t>(rd32(g, p));
        if (size != 0 && p + 0x10 + size <= g.size() && (R->shared = session_heap_alloc(size, owner(level))) != 0)
            std::memcpy(ee_get_ram_base() + R->shared, g.data() + p + 0x10, size);
    }
    for (const Obj& o : OBJS) ctx_view(level, o.veldin, o.size, R->block + o.off);
    std::printf("[ld] level %d's registry at %08x (%#x bytes): %d paths, %u cameras, %u volumes, %u rails of its own; groups none (its level's "
                "name its own mobys); shared pvar data %s\n",
                level, R->block, R->size, R->own_paths, lwu(R->block + R_CAMERAS + 4), lwu(R->block + R_VOLUMES + 4), lwu(R->block + R_RAILS + 4),
                R->shared != 0 ? "copied" : "none");
    g_reg[level] = std::move(R);
    return true;
}

// orig: (host) see leveldata.h
void ld_registry_drop(int level) {
    if (!ld_registry_has(level)) return;
    session_heap_free_owner(owner(level));
    g_reg[level].reset();
    for (uint8_t& l : g_ld_class_level)
        if (l == level + 1) l = 0;
}

// orig: (host) see leveldata.h
void ld_registry_drop_all() {
    for (int l = 0; l < 32; ++l) ld_registry_drop(l);
    std::memset(g_ld_class_level, 0, sizeof g_ld_class_level);
    g_ld_any = false;
}

// orig: (host) see leveldata.h
void ld_base_reset() {
    g_base_mem = BASE_MEM;
    g_base_slot = LD_PATH_MAX;
    if (session_memory_on()) session_heap_free_owner(BASE_OWNER);
    g_base_added.clear();
    g_base_free.clear();
}

// orig: (host) see leveldata.h
int32_t ld_path_reserve(int level, int n, uint32_t* mem) {
    const uint32_t size = 0x10 + 0x10 * static_cast<uint32_t>(n);
    if (!ld_registry_has(level) && g_session_rt) {  // (the base's own objects in a session: the session heap)
        int32_t slot = -1;
        if (!g_base_free.empty()) {
            slot = g_base_free.back();
            g_base_free.pop_back();
        } else if (g_base_slot - 1 >= lw(PATH_COUNT)) {
            slot = --g_base_slot;
        }
        if (slot < 0) return -1;
        const uint32_t a = session_heap_alloc(size, BASE_OWNER);
        if (a == 0) {
            g_base_free.push_back(slot);
            return -1;
        }
        g_base_added[slot] = a;
        sw(PATH_TABLE + static_cast<uint32_t>(slot) * 4, a);
        *mem = a;
        return slot;
    }
    if (!ld_registry_has(level)) {  // (the base's own objects)
        if (g_base_slot - 1 < lw(PATH_COUNT) || g_base_mem + size > BASE_MEM_END) return -1;
        const int32_t slot = --g_base_slot;
        *mem = g_base_mem;
        g_base_mem += size;
        for (uint32_t k = 0; k < size; k += 4) sw(*mem + k, 0);
        sw(PATH_TABLE + static_cast<uint32_t>(slot) * 4, *mem);
        return slot;
    }
    Registry& R = *g_reg[level];
    int32_t slot = -1;
    if (!R.free_slots.empty()) {
        slot = R.free_slots.back();
        R.free_slots.pop_back();
    } else if (R.next_slot - 1 >= R.own_paths) {
        slot = --R.next_slot;
    }
    if (slot < 0) return -1;
    const uint32_t a = session_heap_alloc(size, owner(level));
    if (a == 0) {
        R.free_slots.push_back(slot);
        return -1;
    }
    R.added[slot] = a;
    sw(R.block + R_PATHS + static_cast<uint32_t>(slot) * 4, a);
    *mem = a;
    return slot;
}

// orig: (host) see leveldata.h
void ld_path_release(int level, int32_t index) {
    if (!ld_registry_has(level)) {  // (the base's: given back in a session only, the mods' block is a level's)
        const auto it = g_base_added.find(index);
        if (it == g_base_added.end()) return;
        session_heap_free(it->second);
        g_base_added.erase(it);
        sw(PATH_TABLE + static_cast<uint32_t>(index) * 4, 0);
        g_base_free.push_back(index);
        return;
    }
    Registry& R = *g_reg[level];
    const auto it = R.added.find(index);
    if (it == R.added.end()) return;
    session_heap_free(it->second);
    R.added.erase(it);
    sw(R.block + R_PATHS + static_cast<uint32_t>(index) * 4, 0);
    R.free_slots.push_back(index);
}

// orig: (host) see leveldata.h
void ld_path_set(uint32_t mem, const float (*pts)[3], int n) {
    sw(mem, static_cast<uint32_t>(n));
    for (int k = 0; k < n; ++k)
        for (uint32_t c = 0; c < 4; ++c) swc1(mem + 0x10 + 0x10 * static_cast<uint32_t>(k) + 4 * c, c < 3 ? pts[k][c] : 0.0f);
}

// orig: (host) see leveldata.h
uint32_t ld_path_mem(int level, int32_t index) {
    if (index < 0 || index >= static_cast<int32_t>(LD_PATH_MAX)) return 0;
    if (!ld_registry_has(level)) return lwu(PATH_TABLE + static_cast<uint32_t>(index) * 4);
    return lwu(g_reg[level]->block + R_PATHS + static_cast<uint32_t>(index) * 4);
}

// orig: (host) see leveldata.h
int32_t ld_path_count(int level) { return ld_registry_has(level) ? g_reg[level]->own_paths : lw(PATH_COUNT); }

// orig: (host) see leveldata.h
bool ld_level_path(int level, int32_t index, std::vector<float>& xyz) {
    xyz.clear();
    std::vector<uint8_t> g;
    if (!pool_leveldata(level, g)) return false;
    const uint32_t p = static_cast<uint32_t>(rd32(g, 0x70));
    if (p == 0 || index < 0 || index >= rd32(g, p)) return false;
    const uint32_t a = p + static_cast<uint32_t>(rd32(g, p + 4)) + static_cast<uint32_t>(rd32(g, p + 0x10 + 4 * static_cast<uint32_t>(index)));
    const int32_t n = rd32(g, a);
    for (int32_t k = 0; k < n && a + 0x10 + 0x10 * static_cast<uint32_t>(k) + 12 <= g.size(); ++k)
        for (uint32_t c = 0; c < 3; ++c) {
            float f = 0;
            std::memcpy(&f, g.data() + a + 0x10 + 0x10 * static_cast<uint32_t>(k) + 4 * c, 4);
            xyz.push_back(f);
        }
    return !xyz.empty();
}

// orig: (host) see leveldata.h
int32_t ld_group_create(int level, const uint32_t* mobys, int n) {
    if (n <= 0) return -1;
    const bool own = ld_registry_has(level);
    const uint32_t table = own ? g_reg[level]->block + R_GROUPS : OV(LD_GROUPS);
    const uint32_t count = own ? g_reg[level]->block + R_GROUP_COUNT : OV(LD_GROUP_COUNT);
    int32_t g = -1;
    for (uint32_t k = LD_GROUP_MAX; k-- > 0;)  // (from the end: the level's own groups below)
        if (lw(table + 4 * k) == 0) {
            g = static_cast<int32_t>(k);
            break;
        }
    if (g < 0) return -1;
    // (the list: a registry's in the heap; the base's own in a session in the heap too, else in the mods' block after the
    // paths there)
    uint32_t a = 0;
    if (own) {
        a = session_heap_alloc(static_cast<uint32_t>(n) * 2, owner(level));
    } else if (g_session_rt) {
        a = session_heap_alloc(static_cast<uint32_t>(n) * 2, BASE_OWNER);
    } else if (g_base_mem + align16(static_cast<uint32_t>(n) * 2) <= BASE_MEM_END) {
        a = g_base_mem;
        g_base_mem += align16(static_cast<uint32_t>(n) * 2);
    }
    if (a == 0) return -1;
    const uint32_t base = lwu(OV(0x0015ffd8));
    for (int k = 0; k < n; ++k) sh(a + 2 * static_cast<uint32_t>(k), ((mobys[k] - base) >> 8) | (k == n - 1 ? 0x8000u : 0u));
    sw(table + 4 * static_cast<uint32_t>(g), a);
    if (lw(count) < g + 1) sw(count, static_cast<uint32_t>(g + 1));
    return g;
}

// orig: (host) see leveldata.h
bool ld_group_set(int level, int32_t g, const uint32_t* mobys, int n) {
    if (!ld_registry_has(level) || g < 0 || g >= static_cast<int32_t>(LD_GROUP_MAX) || n <= 0) return false;
    Registry& R = *g_reg[level];
    const uint32_t a = session_heap_alloc(static_cast<uint32_t>(n) * 2, owner(level));
    if (a == 0) return false;
    const uint32_t base = lwu(OV(0x0015ffd8));
    for (int k = 0; k < n; ++k) sh(a + 2 * static_cast<uint32_t>(k), ((mobys[k] - base) >> 8) | (k == n - 1 ? 0x8000u : 0u));
    sw(R.block + R_GROUPS + 4 * static_cast<uint32_t>(g), a);
    if (lw(R.block + R_GROUP_COUNT) < g + 1) sw(R.block + R_GROUP_COUNT, static_cast<uint32_t>(g + 1));
    return true;
}

// ---- the swap ----
namespace {
uint32_t g_saved[LD_PATH_MAX + LD_GROUP_MAX + 32];
}

// orig: (host) see leveldata.h
int ld_enter(uint32_t moby) {
    const uint32_t cls = lhu(moby + 0xa6);
    const int level = cls < 0x1ec0 ? g_ld_class_level[cls] - 1 : -1;
    if (level < 0 || !ld_registry_has(level)) return -1;
    const uint32_t r = g_reg[level]->block;
    uint32_t* s = g_saved;
    for (const Obj& o : OBJS) {
        const uint32_t b = ovl_xlate_data(o.veldin);
        for (uint32_t k = 0; k < o.size; k += 4) {
            *s++ = lwu(b + k);
            sw(b + k, lwu(r + o.off + k));
        }
    }
    return level;
}

// orig: (host) see leveldata.h
void ld_leave(int level) {
    if (level < 0 || !ld_registry_has(level)) return;
    const uint32_t r = g_reg[level]->block;
    const uint32_t* s = g_saved;
    for (const Obj& o : OBJS) {
        const uint32_t b = ovl_xlate_data(o.veldin);
        for (uint32_t k = 0; k < o.size; k += 4) {
            sw(r + o.off + k, lwu(b + k));
            sw(b + k, *s++);
        }
    }
}

// ---- placements ----
// orig: (host) see leveldata.h
bool ld_placement(int level, int32_t uid, int32_t cls, LdPlacement& out) {
    std::vector<uint8_t> g;
    out = LdPlacement{};
    if (!pool_leveldata(level, g)) return false;
    const uint32_t mo = static_cast<uint32_t>(rd32(g, 0x44)), tab = static_cast<uint32_t>(rd32(g, 0x54)), data = static_cast<uint32_t>(rd32(g, 0x58));
    if (mo == 0 || mo + 0x10 > g.size()) return false;
    const int32_t n1 = rd32(g, mo);
    size_t p = mo + 0x10;
    for (int32_t i = 0; i < n1 && p + 0x78 <= g.size(); ++i) {
        const int32_t sz = rd32(g, p);
        if (sz < 0x78 || p + static_cast<size_t>(sz) > g.size()) break;
        const int32_t u = rd32(g, p + 0xc), c = rd32(g, p + 0x18);
        if ((uid >= 0 && u == uid) || (uid < 0 && c == cls)) {
            out.uid = u;
            out.rec.assign(g.begin() + static_cast<long>(p), g.begin() + static_cast<long>(p) + sz);
            const int32_t pi = rd32(g, p + 0x58);
            if (pi >= 0 && tab != 0 && data != 0) {
                const int32_t off = rd32(g, tab + 8 * static_cast<uint32_t>(pi)), size = rd32(g, tab + 8 * static_cast<uint32_t>(pi) + 4);
                if (off >= 0 && size > 0 && data + static_cast<uint32_t>(off) + static_cast<uint32_t>(size) <= g.size()) {
                    out.pvars.assign(g.begin() + data + off, g.begin() + data + off + size);
                    // (pvar index, offset) rows to the first negative index: the links (their words uids), the own pointers
                    if (const uint32_t a = static_cast<uint32_t>(rd32(g, 0x50)); a != 0)
                        for (uint32_t r = a; r + 8 <= g.size() && rd32(g, r) >= 0; r += 8)
                            if (rd32(g, r) == pi) {
                                const uint32_t o = static_cast<uint32_t>(rd32(g, r + 4));
                                int32_t v = -1;
                                if (o + 4 <= out.pvars.size()) std::memcpy(&v, out.pvars.data() + o, 4);
                                out.links.push_back({o, v});
                            }
                    if (const uint32_t a = static_cast<uint32_t>(rd32(g, 0x5c)); a != 0)
                        for (uint32_t r = a; r + 8 <= g.size() && rd32(g, r) >= 0; r += 8)
                            if (rd32(g, r) == pi) out.ptrs.push_back(static_cast<uint32_t>(rd32(g, r + 4)));
                    if (const uint32_t s = static_cast<uint32_t>(rd32(g, 0x4c)); s != 0 && s + 0x10 <= g.size()) {
                        const int32_t ds = rd32(g, s), dc = rd32(g, s + 4);
                        for (int32_t k = 0; ds >= 0 && k < dc; ++k) {
                            const size_t e = s + 0x10 + static_cast<size_t>(ds) + 8 * static_cast<size_t>(k);
                            if (e + 8 > g.size()) break;
                            const uint32_t w = static_cast<uint32_t>(rd32(g, e));
                            if (static_cast<int32_t>(w & 0xffffu) == pi) out.shared.push_back({w >> 16, static_cast<uint32_t>(rd32(g, e + 4))});
                        }
                    }
                }
            }
            return true;
        }
        p += static_cast<size_t>(sz);
    }
    return false;
}

// orig: (host) see leveldata.h
bool ld_paths_take(int level, uint32_t table, uint32_t n) {
    if (!ld_registry_has(level) || n > LD_PATH_MAX) return false;
    Registry& R = *g_reg[level];
    for (uint32_t k = 0; k < LD_PATH_MAX; ++k) sw(R.block + R_PATHS + 4 * k, k < n ? lwu(table + 4 * k) : 0u);
    sw(R.block + R_VOLUMES + 0x20, n);
    R.own_paths = static_cast<int32_t>(n);
    R.next_slot = LD_PATH_MAX;
    R.free_slots.clear();
    for (const auto& [i, a] : R.added) session_heap_free(a);
    R.added.clear();
    return true;
}

// orig: (host) see leveldata.h
void ld_registry_move(int level, const float d[3]) {
    if (!ld_registry_has(level)) return;
    const uint32_t r = g_reg[level]->block;
    size_t n = 0;
    for (uint32_t l = 0; l < 3; ++l) {  // (the three lists of 0x80-byte volumes: their centre at +0x30)
        const uint32_t a = lwu(r + R_VOLUMES + 8 + 8 * l), c = lwu(r + R_VOLUMES + 12 + 8 * l);
        for (uint32_t k = 0; a != 0 && k < c; ++k, ++n)
            for (uint32_t i = 0; i < 3; ++i) swc1(a + k * 0x80 + 0x30 + 4 * i, lwc1(a + k * 0x80 + 0x30 + 4 * i) + d[i]);
    }
    std::printf("[ld] level %d's registry: its %zu volumes moved by %.2f %.2f %.2f\n", level, n, d[0], d[1], d[2]);
}

// orig: (host) see leveldata.h
uint32_t ld_shared_data(int level) { return ld_registry_has(level) ? g_reg[level]->shared : 0; }

}  // namespace rac1
