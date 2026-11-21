// The bridge guard (guard.h; docs/env-vars.md, RAC1_BRIDGE_GUARD / RAC1_BRIDGE_RANGES).
//
// A ranges file (RAC1_BRIDGE_RANGES=<file>; the built-in sets below are written the same way), one range per line, `#` starts
// a comment, addresses in hex:
//   name lo hi        Veldin's addresses [lo, hi): translated onto the current level's copy of the object (ovl_xlate_data,
//                     the same translation as OV()), again whenever the translation changes
//   name! lo hi       an absolute address range (the scratchpad 0x70000000.., the base ELF's data): never translated
//   name@ table       the contents a level-data table points to, found in EE memory: `paths` (each path's points), `volumes`
//                     (the 0x90-byte records and the three lists of 0x80-byte records), `rails` (the 0x20-byte rail records),
//                     `cameras` (the 0x20-byte camera placements), `zones` (the zone grid with its masks, the octant box)
//                     - again when the table's words are written, when the translation changes, and every 2^20 accesses
// A name given on several lines is one range of several pieces. Ranges must not overlap: where they do, the earlier piece
// keeps the overlapping bytes (a warning says so).
//
// The census file (RAC1_BRIDGE_GUARD=census[:<file>]) is mapped into memory and holds the counts as they are made, so it is
// complete however the process ends (the game leaves through std::_Exit, which runs no exit handler). Its layout (host byte
// order; `bom` says which), read by tools/bridge/guard_report.py:
//   0x000  "R1BGUARD"  u32 bom 0x01020304  u32 version 1  u32 header bytes  u32 slots  u32 ranges  u32 flags
//   0x020  u64 the anchor's address in this run  u64 hits dropped (the table was full)
//   0x030  the anchor's symbol name (64 bytes)  0x070 the executable (512 bytes)  0x270 the ranges asked for (256 bytes)
//   0x3f0  u64 the unwindings made to find a caller past the accessors  u64 the accesses counted
//   0x400  the ranges' names, 48 bytes each, by id
//   header bytes on: `slots` slots of (u64 key, u64 count); key 0 is an empty slot. key = caller << 16 | range id << 3 |
//   scoped << 2 | kind, the caller being the calling function's start (or the call's return address when it cannot be
//   found) minus the anchor's address, as a 48-bit two's complement number: the report adds the anchor's address in the
//   binary's symbol table.
#include <game/bridge/guard.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <platform/platform.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <mutex>
#include <sstream>
#include <string>
#include <unordered_set>
#include <vector>

#if !defined(_WIN32)
#include <fcntl.h>
#include <sys/mman.h>
#include <unistd.h>
#endif
#if defined(__GNUC__) && (defined(__linux__) || defined(__APPLE__))
#include <unwind.h>
#define RAC1_GUARD_UNWIND 1
#endif
#if defined(__linux__)
#include <elf.h>
#endif

// orig: (host) the census's reference point: its address in the run and in the binary's symbol table give the callers' addresses
extern "C" [[gnu::noinline, gnu::used]] void rac1_bridge_guard_anchor() { __asm__ volatile(""); }

namespace rac1 {

namespace {

// ---- the built-in sets (Veldin's addresses; stage 5's plan, section 1, and its scan's table) ----
const char* const kSetColl =
    "coll_block 0x173e40 0x173e90\n"     // the collision block / hit record: +0 tree, +8 joint cache, +0x10 query counter,
                                         // +0x14 contact ring index, +0x18 hit moby, +0x1c flags, +0x20 point, +0x30 pushed
                                         // out, +0x40 normal
    "coll_tri 0x173e90 0x173ec0\n"       // the hit triangle / segment records (VU0 leftovers)
    "coll_hitlist 0x177f00 0x178000\n"   // the moby hit list (0-terminated)
    "coll_contacts 0x178100 0x179100\n"  // the 64-entry contact ring (64 bytes each)
    "coll_meshlist 0x15f618 0x15f61c\n"  // the hero mesh chunk list pointer (gp-0x75e8)
    "moby_grid 0x19b7e0 0x19f7e0\n";     // the 64 x 64 moby grid
const char* const kSetLd =
    "paths 0x1b04b0 0x1b0770\n"          // the path table (176 words)
    "paths 0x160104 0x16010c\n"          // its count and data pointer
    "paths_data@ paths\n"
    "volumes 0x1600e4 0x160104\n"        // the volumes' pointers and counts
    "volumes_data@ volumes\n"
    "cameras_ld 0x15ef50 0x15ef58\n"     // the camera placements' pointer, count
    "cameras_data@ cameras\n"
    "cam_volumes 0x15ef80 0x15ef84\n"    // the camera volume grid pointer
    "rails 0x15f70c 0x15f714\n"          // the grind rails' pointer, count
    "rails_data@ rails\n"
    "groups 0x1ab840 0x1aba00\n"         // the moby groups (112 words)
    "groups 0x15fff4 0x15fff8\n";        // their count
const char* const kSetZones =
    "zones 0x15f600 0x15f614\n"          // the zone grid pointer, the octant box pointer, the modes, the last list
    "zones_masks 0x173d00 0x173e00\n"    // the visible zones and the neighbours' union
    "zones_data@ zones\n";
const char* const kSetTables =
    "tab_items 0x179ac0 0x17a680\n"      // the item table (0x4c per item)
    "tab_save 0x1845c0 0x184a00\n"       // the save descriptors (game, levels)
    "tab_particles 0x1609a0 0x1609b0\n"  // the particle template quadword
    "tab_talkers 0x1c44b8 0x1c44c0\n"    // the talker ranges
    "tab_classes 0x197300 0x197ec0\n"    // the class registration tables
    "tab_classes 0x1ea300 0x1ea880\n"    // the class rows
    "tab_camtypes 0x1ea880 0x1eaa00\n"   // the camera types
    "tab_text 0x15f660 0x15f664\n";      // the text table pointer
const char* const kSetSpr =  // the scratchpad, in the areas the collision and the posing use (the plan's section 1.1)
    "spr_0000! 0x70000000 0x70001000\n"  // chunk list A
    "spr_1000! 0x70001000 0x70001fe0\n"  // chunk list B
    "spr_1fe0! 0x70001fe0 0x70002000\n"
    "spr_2000! 0x70002000 0x70003000\n"  // vertices
    "spr_3000! 0x70003000 0x70003200\n"  // the segment's crossings (x)
    "spr_3200! 0x70003200 0x70003400\n"  // (y)
    "spr_3400! 0x70003400 0x70003600\n"  // (z)
    "spr_3600! 0x70003600 0x70003700\n"
    "spr_3700! 0x70003700 0x70003800\n"  // (two saved registers' halves at 0x70003700..0x7000371c)
    "spr_3800! 0x70003800 0x70003c00\n"  // the merged crossings; moby_joints
    "spr_3c00! 0x70003c00 0x70003ffc\n"
    "spr_3ffc! 0x70003ffc 0x70004000\n";

std::string builtin_set(const std::string& name) {
    if (name == "coll") return kSetColl;
    if (name == "ld") return kSetLd;
    if (name == "zones") return kSetZones;
    if (name == "tables") return kSetTables;
    if (name == "spr") return kSetSpr;
    if (name == "all") return std::string(kSetColl) + kSetLd + kSetZones + kSetTables + kSetSpr;
    return {};
}

enum class Mode { Off, Census, Trap, Spr };
enum class SpecType { Veldin, Absolute, Derived };
enum class Derive { None, Paths, Volumes, Rails, Cameras, Zones };

struct Spec {
    int id = 0;
    SpecType type = SpecType::Veldin;
    uint32_t lo = 0, hi = 0;
    Derive derive = Derive::None;
};

struct Piece {
    uint32_t lo, hi;
    int id;
};

// one translation of the ranges (immutable once published; the old ones kept until the end: a few per run)
struct Table {
    std::vector<Piece> pieces;   // sorted, disjoint
    std::vector<Piece> sources;  // the words the derived ranges are read from (a write there: derive again)
    uint32_t gen = 0;            // g_ovl_gen it was made for
};

constexpr int kMaxRanges = (65536 - 0x400) / 48;
constexpr uint32_t kHeaderBytes = 65536;
constexpr uint32_t kCensusSlots = 1u << 18;
constexpr uint32_t kSprSlots = 1u << 19;  // (the spr mode's flows: (writer, reader, word, flags) slots of 4 words)
uint32_t kSlots = kCensusSlots;
uint32_t g_slot_words = 2;
constexpr uint32_t kRefreshEvery = 1u << 20;

Mode g_mode = Mode::Off;
std::string g_census_path;
std::string g_ranges_arg;
std::vector<std::string> g_names;
std::vector<Spec> g_specs;
bool g_has_derived = false;

std::mutex g_table_mutex;
std::atomic<const Table*> g_table{nullptr};
std::vector<const Table*> g_old_tables;  // (under g_table_mutex)
std::atomic<bool> g_dirty{true};

// the census
uint8_t* g_census = nullptr;  // header, then the slots
uint64_t* g_slots = nullptr;  // pairs (key, count)
uintptr_t g_anchor = 0;
std::unordered_set<uintptr_t> g_accessors;  // the memory accessors' starts: an access through one counts for its caller

thread_local int t_scope = 0;      // BridgeScope depth
thread_local bool t_in_guard = false;
thread_local uint32_t t_count = 0;  // accesses seen by this thread (the periodic re-derivation)

// ---- the ranges ----
uint32_t parse_hex(const std::string& s, bool* ok) {
    char* end = nullptr;
    const unsigned long v = std::strtoul(s.c_str(), &end, 16);
    *ok = end != nullptr && *end == 0 && !s.empty();
    return static_cast<uint32_t>(v);
}

int name_id(const std::string& name) {
    for (size_t i = 0; i < g_names.size(); ++i)
        if (g_names[i] == name) return static_cast<int>(i);
    g_names.push_back(name);
    return static_cast<int>(g_names.size() - 1);
}

bool parse_ranges(const std::string& text, const std::string& where) {
    std::istringstream in(text);
    std::string line;
    int n = 0;
    while (std::getline(in, line)) {
        ++n;
        if (const auto h = line.find('#'); h != std::string::npos) line.resize(h);
        std::istringstream s(line);
        std::string name, a, b;
        if (!(s >> name)) continue;
        Spec spec;
        bool ok = true;
        if (name.back() == '@') {
            name.pop_back();
            s >> a;
            spec.type = SpecType::Derived;
            static const std::pair<const char*, Derive> kinds[] = {{"paths", Derive::Paths}, {"volumes", Derive::Volumes}, {"rails", Derive::Rails},
                                                                   {"cameras", Derive::Cameras}, {"zones", Derive::Zones}};
            for (const auto& [k, d] : kinds)
                if (a == k) spec.derive = d;
            ok = spec.derive != Derive::None;
            g_has_derived = true;
        } else {
            if (name.back() == '!') {
                name.pop_back();
                spec.type = SpecType::Absolute;
            }
            bool ok1 = false, ok2 = false;
            if (s >> a >> b) {
                spec.lo = parse_hex(a, &ok1);
                spec.hi = parse_hex(b, &ok2);
            }
            ok = ok1 && ok2 && spec.lo < spec.hi;
        }
        if (!ok || name.empty()) {
            std::fprintf(stderr, "[bridge-guard] %s line %d not understood: %s\n", where.c_str(), n, line.c_str());
            return false;
        }
        spec.id = name_id(name);
        g_specs.push_back(spec);
    }
    return true;
}

// a word of EE memory (the guard's own reads: not seen by it), 0 outside the RAM
uint32_t ee_word(uint32_t a) { return a != 0 && ee_is_valid_addr(a, 4) && a < ee_ram_size() ? ee_read<uint32_t>(a) : 0u; }
uint16_t ee_half(uint32_t a) { return a != 0 && ee_is_valid_addr(a, 2) && a < ee_ram_size() ? ee_read<uint16_t>(a) : 0u; }

// a piece of `n` records of `size` bytes at the pointer in the word `ptr` (the count in the word `count`), when it is sane
void add_records(std::vector<Piece>& out, int id, uint32_t ptr, uint32_t count, uint32_t size) {
    const uint32_t p = ee_word(ptr), n = ee_word(count);
    if (p == 0 || n == 0 || n > 0x1000u || p >= ee_game_ram_end() || n * size > ee_game_ram_end() - p) return;  // (a table not set up: none)
    out.push_back({p, p + n * size, id});
}

// the derived pieces of `spec` now, and the words they were read from
void derive(const Spec& spec, std::vector<Piece>& out, std::vector<Piece>& sources) {
    auto src = [&](uint32_t lo, uint32_t hi) {
        const uint32_t l = ovl_xlate_data(lo);
        sources.push_back({l, l + (hi - lo), spec.id});
    };
    switch (spec.derive) {
    case Derive::Paths: {  // each path: its count, 12 bytes, then 16-byte points
        // (the level's paths only, below the count: the words past it keep the last levels' pointers; the paths the mods add
        // from the table's end are not followed)
        src(0x160104, 0x16010c);
        src(0x1b04b0, 0x1b0770);
        const uint32_t table = ovl_xlate_data(0x1b04b0);
        const uint32_t count = std::min<uint32_t>(ee_word(ovl_xlate_data(0x160104)), 176);
        for (uint32_t i = 0; i < count; ++i) {
            const uint32_t p = ee_word(table + i * 4);
            if (p == 0 || p >= ee_game_ram_end()) continue;
            const uint32_t n = ee_word(p);
            if (n > 0x1000u || 0x10 + n * 0x10 > ee_game_ram_end() - p) continue;
            out.push_back({p, p + 0x10 + n * 0x10, spec.id});
        }
        break;
    }
    case Derive::Volumes:
        src(0x1600e4, 0x160104);
        add_records(out, spec.id, ovl_xlate_data(0x1600e4), ovl_xlate_data(0x1600e8), 0x90);
        add_records(out, spec.id, ovl_xlate_data(0x1600ec), ovl_xlate_data(0x1600f0), 0x80);
        add_records(out, spec.id, ovl_xlate_data(0x1600f4), ovl_xlate_data(0x1600f8), 0x80);
        add_records(out, spec.id, ovl_xlate_data(0x1600fc), ovl_xlate_data(0x160100), 0x80);
        break;
    case Derive::Rails:
        src(0x15f70c, 0x15f714);
        add_records(out, spec.id, ovl_xlate_data(0x15f70c), ovl_xlate_data(0x15f710), 0x20);
        break;
    case Derive::Cameras:
        src(0x15ef50, 0x15ef58);
        add_records(out, spec.id, ovl_xlate_data(0x15ef50), ovl_xlate_data(0x15ef54), 0x20);
        break;
    case Derive::Zones: {
        src(0x15f600, 0x15f608);
        // the grid (overlay_entry.cpp zone_grid_cell): +0 the masks' offset, +4 the first z, +6 the z count, +8 the z rows'
        // offsets / 4; a row: its first index, its count, then the next level's offsets / 4 (0: none); a cell 0xffff: none,
        // else its 0x80-byte mask's index
        const uint32_t g = ee_word(ovl_xlate_data(0x15f600));
        const uint32_t ram = ee_game_ram_end();
        if (g != 0 && g < ram - 8) {
            // (bounded: while a level loads the pointer may still name the last level's memory; a grid that does not look like
            // one - rows outside the RAM, more than 2^20 cells - gives its header only)
            uint32_t end = g + 8 + 2u * ee_half(g + 6);
            int32_t last = -1;
            uint32_t cells = 0;
            bool sane = ee_half(g + 6) <= 1024;
            const uint32_t nz = sane ? ee_half(g + 6) : 0;
            for (uint32_t z = 0; z < nz && sane; ++z) {
                const uint32_t zr = ee_half(g + 8 + z * 2);
                if (zr == 0) continue;
                const uint32_t yr = g + zr * 4, ny = ee_half(yr + 2);
                cells += ny;
                sane = yr < ram - 4 - ny * 2 && ny <= 1024 && cells <= (1u << 20);
                if (sane) end = std::max(end, yr + 4 + ny * 2);
                for (uint32_t y = 0; y < ny && sane; ++y) {
                    const uint32_t yo = ee_half(yr + 4 + y * 2);
                    if (yo == 0) continue;
                    const uint32_t xr = g + yo * 4, nx = ee_half(xr + 2);
                    cells += nx;
                    sane = xr < ram - 4 - nx * 2 && nx <= 1024 && cells <= (1u << 20);
                    if (sane) end = std::max(end, xr + 4 + nx * 2);
                    for (uint32_t x = 0; x < nx && sane; ++x) {
                        const uint32_t e = ee_half(xr + 4 + x * 2);
                        if (e != 0xffff) last = std::max(last, static_cast<int32_t>(e));
                    }
                }
            }
            const uint32_t masks = g + ee_word(g);
            if (sane && last >= 0 && masks > g && masks < ram && static_cast<uint32_t>(last + 1) * 0x80 <= ram - masks)
                end = std::max(end, masks + static_cast<uint32_t>(last + 1) * 0x80);
            if (sane && end > g && end <= ram) out.push_back({g, (end + 3) & ~3u, spec.id});
        }
        const uint32_t box = ee_word(ovl_xlate_data(0x15f604));  // its corner, then 8 masks (one per octant)
        if (box != 0 && box < ee_game_ram_end() - 0x410) out.push_back({box, box + 0x410, spec.id});
        break;
    }
    case Derive::None:
        break;
    }
}

// a Veldin range on the current level: one piece when the object moved as one, else word by word
void translate(const Spec& spec, std::vector<Piece>& out) {
    const uint32_t lo = ovl_xlate_data(spec.lo), last = ovl_xlate_data(spec.hi - 1);
    if (last >= lo && last - lo == spec.hi - 1 - spec.lo) {
        out.push_back({lo, last + 1, spec.id});
        return;
    }
    for (uint32_t a = spec.lo; a < spec.hi; a += 4) {
        const uint32_t t = ovl_xlate_data(a);
        const uint32_t n = std::min<uint32_t>(4, spec.hi - a);
        if (!out.empty() && out.back().id == spec.id && out.back().hi == t)
            out.back().hi = t + n;
        else
            out.push_back({t, t + n, spec.id});
    }
}

const Table* build_table() {
    auto* t = new Table;
    t->gen = g_ovl_gen;
    for (const Spec& s : g_specs) {
        if (s.type == SpecType::Absolute)
            t->pieces.push_back({s.lo, s.hi, s.id});
        else if (s.type == SpecType::Veldin)
            translate(s, t->pieces);
        else
            derive(s, t->pieces, t->sources);
    }
    std::stable_sort(t->pieces.begin(), t->pieces.end(), [](const Piece& a, const Piece& b) { return a.lo < b.lo; });
    std::vector<Piece> merged;
    for (Piece p : t->pieces) {
        if (!merged.empty() && merged.back().id == p.id && p.lo <= merged.back().hi) {  // (one range's pieces: joined)
            merged.back().hi = std::max(merged.back().hi, p.hi);
            continue;
        }
        if (!merged.empty() && p.lo < merged.back().hi) {
            static int warned = 0;
            if (warned++ < 8)
                std::fprintf(stderr, "[bridge-guard] %s [%08x, %08x) overlaps %s: the earlier keeps the bytes\n", g_names[p.id].c_str(), p.lo, p.hi,
                             g_names[merged.back().id].c_str());
            p.lo = merged.back().hi;
            if (p.lo >= p.hi) continue;
        }
        if (!merged.empty() && merged.back().id == p.id && merged.back().hi == p.lo)
            merged.back().hi = p.hi;
        else
            merged.push_back(p);
    }
    t->pieces = std::move(merged);
    return t;
}

bool same_pieces(const Table* a, const Table* b) {
    auto eq = [](const std::vector<Piece>& x, const std::vector<Piece>& y) {
        return x.size() == y.size() && std::equal(x.begin(), x.end(), y.begin(), [](const Piece& p, const Piece& q) { return p.lo == q.lo && p.hi == q.hi && p.id == q.id; });
    };
    return a != nullptr && b != nullptr && eq(a->pieces, b->pieces) && eq(a->sources, b->sources);
}

// the ranges on the current translation (made again when it changed, or the derived ranges may have)
const Table* current_table() {
    const Table* t = g_table.load(std::memory_order_acquire);
    if (t != nullptr && t->gen == g_ovl_gen && !g_dirty.load(std::memory_order_relaxed)) return t;
    std::lock_guard<std::mutex> lock(g_table_mutex);
    t = g_table.load(std::memory_order_acquire);
    if (t != nullptr && t->gen == g_ovl_gen && !g_dirty.load(std::memory_order_relaxed)) return t;
    g_dirty.store(false, std::memory_order_relaxed);
    const Table* n = build_table();
    if (same_pieces(n, t) && t->gen == n->gen) {
        delete n;
        return t;
    }
    static const bool verbose = std::getenv("RAC1_BRIDGE_VERBOSE") != nullptr;
    if (t == nullptr || t->gen != n->gen || verbose) {
        std::fprintf(stderr, "[bridge-guard] ranges for level %d (translation %u): %zu pieces\n", ovl_reloc_level(), n->gen, n->pieces.size());
        if (verbose)
            for (const Piece& p : n->pieces) std::fprintf(stderr, "[bridge-guard]   %-16s %08x..%08x\n", g_names[p.id].c_str(), p.lo, p.hi);
    }
    if (t != nullptr) g_old_tables.push_back(t);
    g_table.store(n, std::memory_order_release);
    return n;
}

// ---- the callers ----
uintptr_t rel_of(uintptr_t a) { return a - g_anchor; }

#if defined(RAC1_GUARD_UNWIND)
// per thread: a code address's function start, whether that function is a memory accessor, and (an accessor's) how far its
// frame's CFA is above the CFA of the function it was calling at that address (learnt from one unwinding, then the walk past
// the accessors is a few loads: an unwinding per access costs microseconds, and the census sees tens of millions)
struct CallerCache {
    static constexpr size_t N = 1u << 16;
    uintptr_t pc[N] = {};
    uintptr_t fn[N] = {};
    intptr_t delta[N] = {};
    bool acc[N] = {};
    bool delta_known[N] = {};
    bool no_walk[N] = {};
};
thread_local CallerCache* t_cache = nullptr;

size_t cache_slot(uintptr_t pc, bool* accessor) {
    if (t_cache == nullptr) t_cache = new CallerCache;
    const size_t i = ((pc >> 1) ^ (pc >> 17)) & (CallerCache::N - 1);
    if (t_cache->pc[i] != pc) {
        uintptr_t f = reinterpret_cast<uintptr_t>(_Unwind_FindEnclosingFunction(reinterpret_cast<void*>(pc - 1)));
        if (f == 0) f = pc;
        t_cache->pc[i] = pc;
        t_cache->fn[i] = f;
        t_cache->acc[i] = g_accessors.count(f) != 0;
        t_cache->delta_known[i] = false;
        t_cache->no_walk[i] = false;
    }
    *accessor = t_cache->acc[i];
    return i;
}

struct Frames {
    uintptr_t ip[40];
    uintptr_t cfa[40];
    int n;
};

_Unwind_Reason_Code record(struct _Unwind_Context* ctx, void* arg) {
    auto* f = static_cast<Frames*>(arg);
    const uintptr_t ip = _Unwind_GetIP(ctx);
    if (ip == 0 || f->n == 40) return _URC_END_OF_STACK;
    f->ip[f->n] = ip;
    f->cfa[f->n] = _Unwind_GetCFA(ctx);
    ++f->n;
    return _URC_NO_REASON;
}

// (x86-64: a frame's return address is the word below its CFA, so the walk can read it; elsewhere each such access unwinds)
#if defined(__x86_64__)
std::atomic<bool> g_walk_ok{true};
#else
std::atomic<bool> g_walk_ok{false};
#endif

constexpr intptr_t kAdjustUnknown = INTPTR_MIN;
std::atomic<intptr_t> g_cfa_adjust{kAdjustUnknown};

// the accessors' frames from `ra0` up, by an unwinding: the first function past them, and each accessor address's CFA step
uintptr_t caller_by_unwinding(uintptr_t ra0, uintptr_t cfa_slow, uintptr_t fallback) {
    if (g_census != nullptr) __atomic_fetch_add(reinterpret_cast<uint64_t*>(g_census + 0x3f0), 1, __ATOMIC_RELAXED);
    Frames fr{};
    _Unwind_Backtrace(record, &fr);
    int k = 0;
    while (k < fr.n && fr.ip[k] != ra0) ++k;
    if (k == 0 || k >= fr.n) return fallback;
    // (__builtin_dwarf_cfa() is the frame's CFA up to a constant of the compiler's: that constant, learnt here once)
    const intptr_t adjust = static_cast<intptr_t>(fr.cfa[k - 1] - cfa_slow);
    intptr_t known = kAdjustUnknown;
    if (!g_cfa_adjust.compare_exchange_strong(known, adjust) && known != adjust && g_walk_ok.exchange(false))
        std::fprintf(stderr, "[bridge-guard] the walk past the accessors is off: ee_ram_ptr_slow's frame %zx, the unwinder's %zx\n",
                     static_cast<size_t>(cfa_slow), static_cast<size_t>(fr.cfa[k - 1]));
    for (int j = k; j < fr.n; ++j) {
        bool acc = false;
        const size_t s = cache_slot(fr.ip[j], &acc);
        if (!acc) return t_cache->fn[s];
        t_cache->delta[s] = static_cast<intptr_t>(fr.cfa[j] - fr.cfa[j - 1]);
        t_cache->delta_known[s] = true;
#if defined(__x86_64__)
        uintptr_t slot_word = 0;
        std::memcpy(&slot_word, reinterpret_cast<const void*>(fr.cfa[j] - 8), sizeof slot_word);
        if (j + 1 < fr.n && slot_word != fr.ip[j + 1]) {  // (this address's frame is not one the walk can step over: unwound each time)
            t_cache->delta_known[s] = false;
            t_cache->no_walk[s] = true;
            static std::atomic<int> told{0};
            if (told.fetch_add(1) < 4)
                std::fprintf(stderr, "[bridge-guard] an accessor's frame the walk cannot step over (at anchor%+lld): unwound each time\n",
                             static_cast<long long>(rel_of(fr.ip[j])));
        }
#endif
    }
    return fallback;
}
#endif

// the function an access counts for: the one that called ee_ram_ptr_slow, or past the memory accessors it went through.
// `cfa` is ee_ram_ptr_slow's own CFA (its return address `ra` is the word below it)
uintptr_t caller_function(const void* ra, const void* cfa) {
    const uintptr_t pc = reinterpret_cast<uintptr_t>(ra);
#if defined(RAC1_GUARD_UNWIND)
    bool acc = false;
    size_t s = cache_slot(pc, &acc);
    if (!acc) return t_cache->fn[s];
    const uintptr_t first = t_cache->fn[s];
    const intptr_t adjust = g_cfa_adjust.load(std::memory_order_relaxed);
    if (cfa != nullptr && adjust != kAdjustUnknown && g_walk_ok.load(std::memory_order_relaxed)) {
        uintptr_t at = pc, frame = reinterpret_cast<uintptr_t>(cfa) + static_cast<uintptr_t>(adjust);
        for (int depth = 0; depth < 16; ++depth) {
            if (!t_cache->delta_known[s] || t_cache->no_walk[s]) return caller_by_unwinding(pc, reinterpret_cast<uintptr_t>(cfa), first);
            frame += static_cast<uintptr_t>(t_cache->delta[s]);
            std::memcpy(&at, reinterpret_cast<const void*>(frame - 8), sizeof at);
            s = cache_slot(at, &acc);
            if (!acc) return t_cache->fn[s];
        }
        return first;
    }
    return caller_by_unwinding(pc, reinterpret_cast<uintptr_t>(cfa), first);
#else
    (void)cfa;
    return pc;
#endif
}

#if defined(__linux__) && defined(RAC1_GUARD_UNWIND)
// a mangled name of one of the memory accessors (the compiler's clones, `.isra.0` / `.cold` ..., included): ee_read<T> /
// ee_write<T> for any T (types local to a function too), ee_ram_ptr / _read / _write, ovl_common.h's, ee_regs.h's and vu0m's
// accessors, the bulk copies (mem_copy ...). (The scratchpad's DMA, eer::hw_sw / spr_dma_to ..., keeps its own name: the copy
// is its work)
bool accessor_name(const char* n) {
    if (std::strncmp(n, "_ZN4rac1", 8) != 0) return false;
    if (std::strstr(n, "7ee_readI") != nullptr || std::strstr(n, "8ee_writeI") != nullptr) return true;
    static const char* const kNames[] = {
        "_ZN4rac110ee_ram_ptrE", "_ZN4rac115ee_ram_ptr_readE", "_ZN4rac116ee_ram_ptr_writeE", "_ZN4rac13lbuE", "_ZN4rac12lhE",
        "_ZN4rac13lhuE", "_ZN4rac12lwE", "_ZN4rac13lwuE", "_ZN4rac12ldE", "_ZN4rac14lwc1E", "_ZN4rac12sbE", "_ZN4rac12shE",
        "_ZN4rac12swE", "_ZN4rac12sdE", "_ZN4rac14swc1E", "_ZN4rac11VE", "_ZN4rac11ME", "_ZN4rac15lq_sqE", "_ZN4rac18mem_copyE",
        "_ZN4rac111mem_copy128E", "_ZN4rac110mem_fill32E", "_ZN4rac112mem_clear128E", "_ZN4rac19mem_or128E", "_ZN4rac13eer3lw_E",
        "_ZN4rac13eer4lbu_E", "_ZN4rac13eer4lhu_E", "_ZN4rac13eer3ld_E", "_ZN4rac13eer3sw_E", "_ZN4rac13eer3sh_E", "_ZN4rac13eer3sb_E",
        "_ZN4rac13eer3sd_E", "_ZN4rac13eer2lqE", "_ZN4rac13eer2sqE", "_ZN4rac13eer5lqc2_E", "_ZN4rac13eer5sqc2_E", "_ZN4rac13eer4ldl_E",
        "_ZN4rac13eer4ldr_E", "_ZN4rac13eer3sdlE", "_ZN4rac13eer3sdrE", "_ZN4rac14vu0m4lqc2E", "_ZN4rac14vu0m4sqc2E"};
    for (const char* k : kNames)
        if (std::strncmp(n, k, std::strlen(k)) == 0) return true;
    // the files' own small load / store helpers (an anonymous namespace's: sq3, l3 ... of ovl_1e_1f.cpp and its neighbours)
    if (const char* a = std::strstr(n, "12_GLOBAL__N_1"); a != nullptr) {
        a += 14;
        if (*a == 'L') ++a;  // (internal linkage)
        char* end = nullptr;
        const long len = std::strtol(a, &end, 10);
        static const char* const kLocal[] = {"sq3", "sq3w", "sq4", "sq4f", "l3", "cl3", "ldv", "lq4", "lwf", "rd32", "u16x3", "load_vf", "store_vf"};
        if (end != a && len > 0 && len < 16 && end[len] == 'E')
            for (const char* k : kLocal)
                if (static_cast<long>(std::strlen(k)) == len && std::strncmp(end, k, static_cast<size_t>(len)) == 0) return true;
    }
    return false;
}

// the accessors' copies the program has, from its symbol table (the clones and the instantiations on local types, which no
// address taken here reaches); nothing when the table is not there (a stripped binary)
void accessors_from_symbols() {
    std::FILE* f = std::fopen("/proc/self/exe", "rb");
    if (f == nullptr) return;
    auto read_at = [f](long off, void* dst, size_t n) { return std::fseek(f, off, SEEK_SET) == 0 && std::fread(dst, 1, n, f) == n; };
    Elf64_Ehdr eh;
    std::vector<Elf64_Shdr> sh;
    if (read_at(0, &eh, sizeof eh) && std::memcmp(eh.e_ident, ELFMAG, SELFMAG) == 0 && eh.e_ident[EI_CLASS] == ELFCLASS64 && eh.e_shentsize == sizeof(Elf64_Shdr)) {
        sh.resize(eh.e_shnum);
        if (!read_at(static_cast<long>(eh.e_shoff), sh.data(), sh.size() * sizeof(Elf64_Shdr))) sh.clear();
    }
    for (const Elf64_Shdr& s : sh) {
        if (s.sh_type != SHT_SYMTAB || s.sh_link >= sh.size()) continue;
        std::vector<Elf64_Sym> syms(s.sh_size / sizeof(Elf64_Sym));
        std::vector<char> names(sh[s.sh_link].sh_size + 1, 0);
        if (!read_at(static_cast<long>(s.sh_offset), syms.data(), syms.size() * sizeof(Elf64_Sym)) ||
            !read_at(static_cast<long>(sh[s.sh_link].sh_offset), names.data(), names.size() - 1))
            break;
        uintptr_t bias = 0;
        bool have_bias = false;
        for (const Elf64_Sym& y : syms)
            if (y.st_name < names.size() && std::strcmp(names.data() + y.st_name, "rac1_bridge_guard_anchor") == 0) {
                bias = g_anchor - static_cast<uintptr_t>(y.st_value);
                have_bias = true;
            }
        if (!have_bias) break;
        size_t n = 0;
        for (const Elf64_Sym& y : syms)
            if (ELF64_ST_TYPE(y.st_info) == STT_FUNC && y.st_value != 0 && y.st_name < names.size() && accessor_name(names.data() + y.st_name)) {
                g_accessors.insert(static_cast<uintptr_t>(y.st_value) + bias);
                ++n;
            }
        std::fprintf(stderr, "[bridge-guard] %zu memory accessors' copies found in the symbol table\n", n);
        break;
    }
    std::fclose(f);
}
#endif

// the accessors' out-of-line copies (the compiler's choice per call site; one copy each in the program)
void collect_accessors() {
    auto add = [](auto fn) { g_accessors.insert(reinterpret_cast<uintptr_t>(fn)); };
#define RAC1_GUARD_RW(T)                                                                              \
    add(static_cast<T (*)(uint32_t)>(&ee_read<T>));                                                   \
    add(static_cast<void (*)(uint32_t, const T&)>(&ee_write<T>));
    RAC1_GUARD_RW(uint8_t)
    RAC1_GUARD_RW(int8_t)
    RAC1_GUARD_RW(uint16_t)
    RAC1_GUARD_RW(int16_t)
    RAC1_GUARD_RW(uint32_t)
    RAC1_GUARD_RW(int32_t)
    RAC1_GUARD_RW(uint64_t)
    RAC1_GUARD_RW(int64_t)
    RAC1_GUARD_RW(float)
    RAC1_GUARD_RW(double)
    RAC1_GUARD_RW(Vec4)
#undef RAC1_GUARD_RW
    add(&ee_ram_ptr);
    add(&ee_ram_ptr_read);
    add(&ee_ram_ptr_write);
    add(&lbu);
    add(&lh);
    add(&lhu);
    add(&lw);
    add(&lwu);
    add(&ld);
    add(&lwc1);
    add(&sb);
    add(&sh);
    add(&sw);
    add(&sd);
    add(&swc1);
    add(&V);
    add(&M);
    add(&lq_sq);
#if defined(__linux__) && defined(RAC1_GUARD_UNWIND)
    accessors_from_symbols();
#endif
}

// ---- the census ----
uint64_t mix(uint64_t k) {
    k ^= k >> 33;
    k *= 0xff51afd7ed558ccdull;
    k ^= k >> 33;
    k *= 0xc4ceb9fe1a85ec53ull;
    k ^= k >> 33;
    return k;
}

void census_add(int id, BridgeAccess kind, uintptr_t caller) {
    const uint64_t key = (static_cast<uint64_t>(rel_of(caller)) << 16) | (static_cast<uint64_t>(id) << 3) | (t_scope > 0 ? 4u : 0u) |
                         static_cast<uint64_t>(kind);
    __atomic_fetch_add(reinterpret_cast<uint64_t*>(g_census + 0x3f8), 1, __ATOMIC_RELAXED);
    for (uint32_t i = static_cast<uint32_t>(mix(key)) & (kSlots - 1), n = 0; n < kSlots; ++n, i = (i + 1) & (kSlots - 1)) {
        uint64_t* slot = g_slots + 2 * i;
        uint64_t k = __atomic_load_n(slot, __ATOMIC_ACQUIRE);
        if (k == 0) {
            uint64_t zero = 0;
            if (__atomic_compare_exchange_n(slot, &zero, key, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE))
                k = key;
            else
                k = zero;
        }
        if (k == key) {
            __atomic_fetch_add(slot + 1, 1, __ATOMIC_RELAXED);
            return;
        }
    }
    __atomic_fetch_add(reinterpret_cast<uint64_t*>(g_census + 0x28), 1, __ATOMIC_RELAXED);
}

void put_u32(uint32_t at, uint32_t v) { std::memcpy(g_census + at, &v, 4); }
void put_u64(uint32_t at, uint64_t v) { std::memcpy(g_census + at, &v, 8); }
void put_str(uint32_t at, uint32_t room, const std::string& s) { std::memcpy(g_census + at, s.c_str(), std::min<size_t>(s.size(), room - 1)); }

#if defined(_WIN32)
void census_write_at_exit() {  // (no shared file mapping here: the table is written at a normal exit only)
    if (std::FILE* f = std::fopen(g_census_path.c_str(), "wb")) {
        std::fwrite(g_census, 1, kHeaderBytes + static_cast<size_t>(kSlots) * 8 * g_slot_words, f);
        std::fclose(f);
    }
}
#endif

bool census_open() {
    const size_t bytes = kHeaderBytes + static_cast<size_t>(kSlots) * 8 * g_slot_words;
#if !defined(_WIN32)
    const int fd = ::open(g_census_path.c_str(), O_RDWR | O_CREAT | O_TRUNC, 0644);
    if (fd < 0 || ::ftruncate(fd, static_cast<off_t>(bytes)) != 0) {
        if (fd >= 0) ::close(fd);
        return false;
    }
    void* m = ::mmap(nullptr, bytes, PROT_READ | PROT_WRITE, MAP_SHARED, fd, 0);
    ::close(fd);
    if (m == MAP_FAILED) return false;
    g_census = static_cast<uint8_t*>(m);
#else
    g_census = static_cast<uint8_t*>(std::calloc(bytes, 1));
    if (g_census == nullptr) return false;
    std::atexit(census_write_at_exit);
#endif
    g_slots = reinterpret_cast<uint64_t*>(g_census + kHeaderBytes);
    std::memcpy(g_census, g_mode == Mode::Spr ? "R1BGSPR1" : "R1BGUARD", 8);
    put_u32(0x08, 0x01020304u);
    put_u32(0x0c, g_mode == Mode::Spr ? 2 : 1);
    put_u32(0x10, kHeaderBytes);
    put_u32(0x14, kSlots);
    put_u32(0x18, static_cast<uint32_t>(g_names.size()));
    put_u32(0x1c, 0);
    put_u64(0x20, g_anchor);
    put_u64(0x28, 0);
    put_str(0x30, 64, "rac1_bridge_guard_anchor");
    std::string exe;
    try {
        exe = platform::executable_path().string();
    } catch (...) {
    }
    put_str(0x70, 512, exe);
    put_str(0x270, 256, g_ranges_arg);
    for (size_t i = 0; i < g_names.size(); ++i) put_str(static_cast<uint32_t>(0x400 + i * 48), 48, g_names[i]);
    return true;
}

// ---- the scratchpad's data flow (RAC1_BRIDGE_GUARD=spr) ----
// Every scratchpad word carries the last write's tag: the innermost SprScope instance it was made in (0: none), that scope's
// kind, the writing function and the game frame (the base ELF's frame counter 0x15eea4). A read of a word whose writer
// instance is not on the reader's scope stack (or that no scope wrote) is a flow out of the writer's call: counted as
// (writer function, reader function, writer scope kind, reader's innermost scope kind, word, flags). A read whose value
// changed since the word's last seen value with no write seen in between was written by code the guard cannot see (the
// scratchpad's direct users): counted as such, and the word's writer becomes "unseen".
constexpr int kMaxScopeDepth = 8;
constexpr uint32_t kSprWords = 0x4000 / 4;
constexpr uint64_t kWriterUnseen = 1;    // (a writer function value no real function has: the anchor's offset is never 1)
constexpr uint64_t kWriterRestored = 2;  // (RAC1_BRIDGE_SPR_RESTORE put the word back after its writer's call)
constexpr uint8_t kFlowPointer = 1, kFlowNextFrame = 2, kFlowLaterFrame = 4, kFlowUnseenWrite = 8, kFlowWrite = 16;  // (16: a write record)

struct SprTag {
    uint32_t serial = 0;  // the innermost scope instance of the last write (0: none)
    uint8_t scope = 0;    // its kind
    bool written = false;
    bool seen_valid = false;
    uint32_t frame = 0;
    uint32_t seen = 0;   // the word's value at its last seen read (an unseen write changes it)
    uint64_t fn = 0;     // the writing function (the anchor's offset)
};
SprTag g_spr_tags[kSprWords];
std::atomic<uint32_t> g_spr_serial{0};
uint32_t g_spr_restore_mask = 0;  // RAC1_BRIDGE_SPR_RESTORE: the scope kinds whose writes are put back when their call returns

struct SprFrame {
    uint32_t serial;
    uint8_t scope;
    std::vector<std::pair<uint16_t, uint32_t>>* undo;  // (restore mode) the words this instance wrote first, their values before
};
struct SprStack {
    SprFrame f[kMaxScopeDepth];
    std::vector<uint16_t> written[kMaxScopeDepth];  // per open scope: the words its call wrote (their values are taken when it
                                                    // returns: a later read of a different value was written by unseen code)
    int depth = 0;
    int lost = 0;  // (deeper than kMaxScopeDepth: not tracked)
};
thread_local SprStack t_spr;

uint32_t spr_word(uint32_t w) {
    uint32_t v;
    std::memcpy(&v, ee_get_scratchpad_base() + w * 4, 4);
    return v;
}
uint32_t game_frame() {
    const uint8_t* ram = ee_get_ram_base();
    uint32_t v = 0;
    if (ram != nullptr) std::memcpy(&v, ram + 0x0015eea4u, 4);
    return v;
}
bool on_stack_kind(int scope) {
    for (int i = 0; i < t_spr.depth; ++i)
        if (t_spr.f[i].scope == scope) return true;
    return false;
}
bool on_stack(uint32_t serial) {
    for (int i = 0; i < t_spr.depth; ++i)
        if (t_spr.f[i].serial == serial) return true;
    return false;
}

void flow_add(uint64_t writer_fn, uint64_t reader_fn, uint8_t writer_scope, uint8_t reader_scope, uint32_t word, uint8_t flags) {
    const uint64_t meta = static_cast<uint64_t>(word) | (static_cast<uint64_t>(flags) << 16) | (static_cast<uint64_t>(writer_scope) << 24) |
                          (static_cast<uint64_t>(reader_scope) << 32) | (1ull << 63);  // (bit 63: never an empty slot)
    const uint64_t h = mix(writer_fn * 0x9e3779b97f4a7c15ull ^ reader_fn ^ (meta * 0xc2b2ae3d27d4eb4full));
    for (uint32_t i = static_cast<uint32_t>(h) & (kSlots - 1), n = 0; n < 64; ++n, i = (i + 1) & (kSlots - 1)) {
        uint64_t* slot = g_slots + 4 * i;
        uint64_t m = __atomic_load_n(slot + 2, __ATOMIC_ACQUIRE);
        if (m == 0) {
            // (claimed by its meta word, then filled: a reader of the file sees whole slots once the run is over)
            uint64_t zero = 0;
            if (__atomic_compare_exchange_n(slot + 2, &zero, ~0ull, false, __ATOMIC_ACQ_REL, __ATOMIC_ACQUIRE)) {
                slot[0] = writer_fn;
                slot[1] = reader_fn;
                __atomic_store_n(slot + 2, meta, __ATOMIC_RELEASE);
                __atomic_fetch_add(slot + 3, 1, __ATOMIC_RELAXED);
                return;
            }
            m = zero;
        }
        while (m == ~0ull) m = __atomic_load_n(slot + 2, __ATOMIC_ACQUIRE);  // (another thread filling it)
        if (m == meta && slot[0] == writer_fn && slot[1] == reader_fn) {
            __atomic_fetch_add(slot + 3, 1, __ATOMIC_RELAXED);
            return;
        }
    }
    __atomic_fetch_add(reinterpret_cast<uint64_t*>(g_census + 0x28), 1, __ATOMIC_RELAXED);
}

// (RAC1_BRIDGE_SPR_WATCH=<offset hex>[,<count>]: the first accesses of that word printed with a backtrace: who writes / reads it)
void spr_watch(uint32_t w, const char* what, uint64_t fn, uint8_t scope) {
    static const char* e = std::getenv("RAC1_BRIDGE_SPR_WATCH");
    static const uint32_t word = e != nullptr ? static_cast<uint32_t>(std::strtoul(e, nullptr, 16)) / 4 : ~0u;
    static std::atomic<int> left{e != nullptr && std::strchr(e, ',') != nullptr ? std::atoi(std::strchr(e, ',') + 1) : 6};
    static const char* which = e != nullptr && std::strchr(e, ':') != nullptr ? std::strchr(e, ':') + 1 : "";  // (",n:w" writes, ",n:f" flows)
    if (w != word || (*which == 'w' && what[0] != 'w') || (*which == 'f' && what[0] == 'w') || (*which == 'c' && (scope < 1 || scope > 5)) ||
        left.fetch_sub(1) <= 0)
        return;  // (",n:c": the accesses inside a collision routine's call)
    std::fprintf(stderr, "[bridge-guard] watch +0x%04x: %s by anchor%+lld in scope %s (frame %u; the word's call %u, this thread's innermost %u; value %08x)\n",
                 w * 4, what, static_cast<long long>(fn & (1ull << 47) ? static_cast<int64_t>(fn) - (1ll << 48) : static_cast<int64_t>(fn)),
                 g_names.size() > scope ? g_names[scope].c_str() : "?", game_frame(), g_spr_tags[w].serial,
                 t_spr.depth > 0 ? t_spr.f[t_spr.depth - 1].serial : 0u, spr_word(w));
    std::fflush(stderr);
    platform::print_backtrace(12);
}

void spr_access(uint32_t addr, uint32_t size, BridgeAccess kind, const void* caller, const void* cfa) {
    const uint32_t off = addr - 0x70000000u;
    if (off >= 0x4000u) return;
    // (the words reached: a write's own size; a host pointer's vector, 16 bytes; a read's - ee_read gives the slow path no
    // size - the word it starts in: a quadword read counts for its first word only)
    const uint32_t bytes = kind == BridgeAccess::Write ? std::max<uint32_t>(size, 1) : kind == BridgeAccess::Pointer ? 16u : 1u;
    const uint32_t first = off / 4, last = std::min<uint32_t>((off + bytes - 1) / 4, kSprWords - 1);
    const uint64_t fn = static_cast<uint64_t>(rel_of(caller_function(caller, cfa))) & 0x0000ffffffffffffull;
    const uint32_t frame = game_frame();
    const SprFrame* inner = t_spr.depth > 0 ? &t_spr.f[t_spr.depth - 1] : nullptr;
    for (uint32_t w = first; w <= last; ++w) {
        SprTag& tg = g_spr_tags[w];
        if (kind == BridgeAccess::Write) {
            if (inner != nullptr && tg.serial != inner->serial) {
                if (inner->undo != nullptr) inner->undo->push_back({static_cast<uint16_t>(w), spr_word(w)});
                t_spr.written[t_spr.depth - 1].push_back(static_cast<uint16_t>(w));
            }
            tg.serial = inner != nullptr ? inner->serial : 0;
            tg.scope = inner != nullptr ? inner->scope : 0;
            tg.fn = fn;
            tg.frame = frame;
            tg.written = true;
            tg.seen_valid = false;
            spr_watch(w, "write", fn, tg.scope);
            // (the writes too, by scope and function: which words a marked call's own code writes - the oracle's ignore list)
            if (inner != nullptr) flow_add(fn, 0, tg.scope, 0, w, kFlowWrite);
            continue;
        }
        // a read (or a pointer taken: counted as a read, flagged)
        const uint32_t value = spr_word(w);
        uint8_t flags = kind == BridgeAccess::Pointer ? kFlowPointer : 0;
        if (tg.seen_valid && tg.seen != value) {  // (written since by code the guard does not see)
            flags |= kFlowUnseenWrite;
            tg.serial = 0;
            tg.scope = 0;
            tg.fn = kWriterUnseen;
            tg.frame = frame;
            tg.written = true;
        }
        tg.seen = value;
        tg.seen_valid = true;
        if (!tg.written) continue;
        if (tg.serial != 0 && on_stack(tg.serial)) {  // (inside the writer's own call: not a flow)
            spr_watch(w, "read inside the writer's call", fn, inner != nullptr ? inner->scope : 0);
            continue;
        }
        const uint32_t df = frame - tg.frame;
        if (df == 1) flags |= kFlowNextFrame;
        if (df > 1) flags |= kFlowLaterFrame;
        flow_add(tg.fn, fn, tg.scope, inner != nullptr ? inner->scope : 0, w, flags);
        spr_watch(w, "a flow's read", fn, inner != nullptr ? inner->scope : 0);
    }
}

// ---- trap ----
[[noreturn]] void trap(uint32_t addr, uint32_t size, BridgeAccess kind, int id, const void* ra, const void* cfa) {
    static const char* const kinds[] = {"?", "read", "write", "pointer"};
    std::fprintf(stderr,
                 "[bridge-guard] TRAP: %s of %u bytes at %08x, range %s, by the function at anchor%+lld (return address anchor%+lld; "
                 "tools/bridge/guard_report.py trap <this log> names them)\n",
                 kinds[static_cast<int>(kind)], size, addr, g_names[id].c_str(), static_cast<long long>(rel_of(caller_function(ra, cfa))),
                 static_cast<long long>(rel_of(reinterpret_cast<uintptr_t>(ra))));
    std::fprintf(stderr, "[bridge-guard] anchor rac1_bridge_guard_anchor at %p\n", reinterpret_cast<void*>(g_anchor));
    std::fflush(stderr);
    platform::print_backtrace(32);
    std::abort();
}

const char* const kSprScopeNames[] = {"(none)", "segment_collide_v", "sphere_collide_v", "capsule_collide_v", "mobys_in_sphere_v",
                                       "sphere_mesh_collide", "wad_decompress", "armor_load", "gameplay_load"};
constexpr int kSprScopeKinds = static_cast<int>(sizeof kSprScopeNames / sizeof kSprScopeNames[0]);

// RAC1_BRIDGE_GUARD=spr[:<file>]: the scratchpad's flows (no ranges: the whole scratchpad), with RAC1_BRIDGE_SPR_RESTORE
bool spr_init(const std::string& path) {
    g_mode = Mode::Spr;
    g_census_path = path;
    g_ranges_arg = "spr";
    kSlots = kSprSlots;
    g_slot_words = 4;
    for (int i = 0; i < kSprScopeKinds; ++i) g_names.push_back(kSprScopeNames[i]);
    if (const char* r = std::getenv("RAC1_BRIDGE_SPR_RESTORE"); r != nullptr && *r != 0) {
        std::stringstream s(r);
        std::string name;
        while (std::getline(s, name, ',')) {
            if (name == "coll") g_spr_restore_mask |= 0x3eu;  // (the five collision routines)
            else if (name == "load") g_spr_restore_mask |= 0x1c0u;  // (wad_decompress, armor_load, gameplay_load)
            else if (name == "all") g_spr_restore_mask |= 0x1feu;
            else
                for (int i = 1; i < kSprScopeKinds; ++i)
                    if (name == kSprScopeNames[i]) g_spr_restore_mask |= 1u << i;
        }
    }
    g_anchor = reinterpret_cast<uintptr_t>(&rac1_bridge_guard_anchor);
    collect_accessors();
    if (!census_open()) {
        std::fprintf(stderr, "[bridge-guard] the flow file %s cannot be made: the guard is off\n", path.c_str());
        g_mode = Mode::Off;
        return false;
    }
    put_u32(0x1c, g_spr_restore_mask);
    g_spr_scopes_on = true;
    std::fprintf(stderr, "[bridge-guard] spr: the scratchpad's flows in %s%s: the inline memory accessors are off (slow)\n", path.c_str(),
                 g_spr_restore_mask != 0 ? " (RESTORE: the scopes' scratchpad writes are put back when they return - the game's state changes)" : "");
    return true;
}

}  // namespace

bool g_spr_scopes_on = false;

// orig: (host) see guard.h
void spr_scope_enter(SprScopeId id) {
    const int k = static_cast<int>(id);
    if (t_spr.depth >= kMaxScopeDepth) {
        ++t_spr.lost;
        return;
    }
    SprFrame& f = t_spr.f[t_spr.depth];
    f.serial = g_spr_serial.fetch_add(1, std::memory_order_relaxed) + 1;
    f.scope = static_cast<uint8_t>(k);
    f.undo = (g_spr_restore_mask >> k & 1u) != 0 && !on_stack_kind(k) ? new std::vector<std::pair<uint16_t, uint32_t>> : nullptr;
    t_spr.written[t_spr.depth].clear();
    ++t_spr.depth;
    if (g_census != nullptr && k < 16) __atomic_fetch_add(reinterpret_cast<uint64_t*>(g_census + 0x380 + 8 * k), 1, __ATOMIC_RELAXED);
}

// orig: (host) see guard.h
void spr_scope_leave() {
    if (t_spr.lost > 0) {
        --t_spr.lost;
        return;
    }
    if (t_spr.depth == 0) return;
    SprFrame& f = t_spr.f[--t_spr.depth];
    if (f.undo != nullptr) {  // (restore mode: the words this call wrote, and still holds, put back as they were before it)
        for (const auto& [w, old] : *f.undo) {
            SprTag& tg = g_spr_tags[w];
            if (tg.serial != f.serial) continue;
            std::memcpy(ee_get_scratchpad_base() + w * 4u, &old, 4);
            tg.serial = 0;
            tg.scope = 0;
            tg.fn = kWriterRestored;
            tg.seen = old;
            tg.seen_valid = true;
        }
        delete f.undo;
        f.undo = nullptr;
    }
    // the values this call left (a read that finds another value later: unseen code wrote it in between)
    for (const uint16_t w : t_spr.written[t_spr.depth]) {
        SprTag& tg = g_spr_tags[w];
        if (tg.serial != f.serial) continue;
        tg.seen = spr_word(w);
        tg.seen_valid = true;
    }
}

// orig: (host) see guard.h
bool bridge_guard_init() {
    const char* e = std::getenv("RAC1_BRIDGE_GUARD");
    if (e == nullptr || *e == 0 || std::strcmp(e, "0") == 0 || std::strcmp(e, "off") == 0) return false;
    const std::string arg = e;
    if (arg == "trap") {
        g_mode = Mode::Trap;
    } else if (arg.rfind("spr", 0) == 0 && (arg.size() == 3 || arg[3] == ':')) {
        return spr_init(arg.size() > 4 ? arg.substr(4) : "bridge_spr.bin");
    } else if (arg.rfind("census", 0) == 0 && (arg.size() == 6 || arg[6] == ':')) {
        g_mode = Mode::Census;
        g_census_path = arg.size() > 7 ? arg.substr(7) : "bridge_census.bin";
    } else {
        std::fprintf(stderr, "[bridge-guard] RAC1_BRIDGE_GUARD=%s: census[:<file>], spr[:<file>] or trap expected; the guard is off\n", e);
        return false;
    }
    const char* r = std::getenv("RAC1_BRIDGE_RANGES");
    g_ranges_arg = r != nullptr && *r != 0 ? r : "all";
    bool ok = true;
    if (std::ifstream f(g_ranges_arg); f) {
        std::stringstream text;
        text << f.rdbuf();
        ok = parse_ranges(text.str(), g_ranges_arg);
    } else {
        std::stringstream sets(g_ranges_arg);
        std::string name;
        while (ok && std::getline(sets, name, ',')) {
            const std::string text = builtin_set(name);
            if (text.empty()) {
                std::fprintf(stderr, "[bridge-guard] RAC1_BRIDGE_RANGES: %s is neither a file nor a set (coll, ld, zones, tables, spr, all)\n", name.c_str());
                ok = false;
            } else {
                ok = parse_ranges(text, name);
            }
        }
    }
    if (!ok || g_specs.empty() || g_names.size() > static_cast<size_t>(kMaxRanges) || g_names.size() > 8191) {
        std::fprintf(stderr, "[bridge-guard] no usable ranges: the guard is off\n");
        g_mode = Mode::Off;
        return false;
    }
    g_anchor = reinterpret_cast<uintptr_t>(&rac1_bridge_guard_anchor);
    collect_accessors();
    if (g_mode == Mode::Census && !census_open()) {
        std::fprintf(stderr, "[bridge-guard] the census file %s cannot be made: the guard is off\n", g_census_path.c_str());
        g_mode = Mode::Off;
        return false;
    }
    std::fprintf(stderr, "[bridge-guard] %s, %zu ranges (%s)%s%s: the inline memory accessors are off (slow)\n", g_mode == Mode::Trap ? "trap" : "census",
                 g_names.size(), g_ranges_arg.c_str(), g_mode == Mode::Census ? ", census in " : "", g_mode == Mode::Census ? g_census_path.c_str() : "");
    return true;
}

// orig: (host) see guard.h
bool bridge_guard_on() { return g_mode != Mode::Off; }

// orig: (host) see guard.h
void bridge_guard_access(uint32_t addr, uint32_t size, BridgeAccess kind, const void* caller, const void* cfa) {
    if (t_in_guard || g_mode == Mode::Off) return;
    if (g_mode == Mode::Spr) {
        if (addr - 0x70000000u < 0x4000u) {
            t_in_guard = true;
            spr_access(addr, size, kind, caller, cfa);
            t_in_guard = false;
        }
        return;
    }
    t_in_guard = true;  // (the guard's own reads of EE memory, and the translation's, are not seen)
    if (g_has_derived && ++t_count % kRefreshEvery == 0) g_dirty.store(true, std::memory_order_relaxed);
    const Table* t = current_table();
    const uint32_t end = addr + std::max<uint32_t>(size, 1);
    const auto& ps = t->pieces;
    auto it = std::partition_point(ps.begin(), ps.end(), [addr](const Piece& p) { return p.hi <= addr; });
    int last = -1;
    for (; it != ps.end() && it->lo < end; ++it) {
        if (it->id == last) continue;
        last = it->id;
        if (g_mode == Mode::Trap) {
            if (t_scope == 0) trap(addr, size, kind, it->id, caller, cfa);
        } else {
            census_add(it->id, kind, caller_function(caller, cfa));
        }
    }
    if (kind != BridgeAccess::Read && !t->sources.empty()) {
        for (const Piece& s : t->sources)
            if (addr < s.hi && s.lo < end) {
                g_dirty.store(true, std::memory_order_relaxed);
                break;
            }
    }
    t_in_guard = false;
}

// orig: (host) see guard.h
BridgeScope::BridgeScope() { ++t_scope; }
BridgeScope::~BridgeScope() { --t_scope; }

}  // namespace rac1
