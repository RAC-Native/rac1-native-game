// See ovl_reloc.h.
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_sky.h>
#include <game/ovl/variants.h>
#include <game/ovl/region_core.h>
#include <runtime/kernel/disc.h>
#include <io/asset_paths.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <fstream>
#include <map>
#include <mutex>
#include <set>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace rac1 {

uint32_t g_ovl_gen = 0;
uint32_t g_ctx_levels = 0;
uint32_t g_region_gen = 0;
uint32_t (*g_ctx_xlate)(int unit, uint32_t a) = nullptr;
// orig: (host) see ovl_reloc.h's lvl_page
uint32_t g_page_poison = std::getenv("RAC1_PAGE_POISON") != nullptr ? 0x01000000u : 0u;

namespace {

int g_level = 0;
bool g_identity = true;  // the ports' addresses as they are (NTSC-U Veldin's image, or no overlay)
std::map<uint32_t, uint32_t> g_data;               // Veldin -> level (the maps' data lines)
std::map<uint32_t, uint32_t> g_code;               // Veldin -> level (fn lines)
std::map<uint32_t, uint32_t> g_code_rev;           // level -> Veldin
std::unordered_map<uint32_t, uint32_t> g_code_size;  // Veldin function -> its size (bytes)
std::set<uint32_t> g_code_kept;                     // Veldin addresses handed out untranslated
// The extents of the engine's data blocks (levelN.ext, tools/overlay_match/extents.py): Veldin's start -> [level start, size],
// and by the level's start; empty when the file is missing (another region's disc, an old setup): today's rules then
struct Extent {
    uint32_t veldin, level, size;
};
std::map<uint32_t, Extent> g_ext_v, g_ext_l;
// Veldin -> level code pointers the matched code builds (lui + addiu: callbacks, table entries)
// whose function is not the same code in the level (not an `fn` line: a variant); and back
std::map<uint32_t, uint32_t> g_code_ptr, g_code_ptr_rev;

// orig: (host) another region's disc: one unit's runs of the region table (region_levelN.txt /
// region_boot.txt: the NTSC-U level N / boot module -> the disc's, tools/overlay_match/region_maps.py apply)
struct RegionRuns {
    using Run = std::array<uint32_t, 3>;  // lo, hi, displacement
    std::vector<Run> code;                // functions lo..hi (exclusive)
    std::vector<Run> data;                // mapped data addresses lo..hi (both inclusive), sorted
    std::map<uint32_t, uint32_t> entry, entry_rev;  // the entries of the functions the region changed
    std::map<uint32_t, uint32_t> starts_rev;        // those that start a function of the disc's (not `inner`): disc -> NTSC-U
                                                    // (entry_rev holds the same: the `inner` ones are forward only)
    uint32_t code_base = 0, code_end = 0;           // the NTSC-U code section
    // orig: (host) a code address (0: in a function the region changed, not its entry)
    uint32_t code_fwd(uint32_t a) const {
        for (const auto& r : code)
            if (a >= r[0] && a < r[1]) return a + r[2];
        const auto e = entry.find(a);
        return e != entry.end() ? e->second : 0;
    }
    // orig: (host) back to NTSC-U (0: none)
    uint32_t code_back(uint32_t p) const {
        for (const auto& r : code)
            if (p - r[2] >= r[0] && p - r[2] < r[1]) return p - r[2];
        const auto e = entry_rev.find(p);
        return e != entry_rev.end() ? e->second : 0;
    }
    // orig: (host) a data address: inside a run, its displacement; between two runs of the same
    // displacement, that one; less than 0x100 above a run's last address (an offset into its last
    // object), that run's; 0 otherwise (region_maps.py's Region.data_fwd, checked against the direct
    // maps on every literal: region_maps.py check-levels)
    uint32_t data_fwd(uint32_t a) const {
        auto it = std::upper_bound(data.begin(), data.end(), a, [](uint32_t v, const Run& r) { return v < r[0]; });
        if (it == data.begin()) return 0;
        const Run& r = *std::prev(it);
        if (a <= r[1]) return a + r[2];
        if (it != data.end() && (*it)[2] == r[2]) return a + r[2];
        return a - r[1] < 0x100 ? a + r[2] : 0;
    }
};
// orig: (host) see RegionRuns: unit n's (a level, or OVL_FRONT)
RegionRuns region_load(int unit) {
    RegionRuns rr;
    const std::string path = io::asset_path(unit == OVL_FRONT ? std::string("generated/overlay_map/region_boot.txt")
                                                              : "generated/overlay_map/region_level" + std::to_string(unit) + ".txt");
    std::ifstream in(path);
    if (!in) std::fprintf(stderr, "[ovl_reloc] %s missing (./setup.sh): its addresses kept\n", path.c_str());
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind, d;
        uint32_t lo = 0, hi = 0;
        s >> kind >> std::hex >> lo >> hi >> d;
        const uint32_t disp = d.empty() || d == "inner" ? 0 : static_cast<uint32_t>(std::stoll(d, nullptr, 16));  // (`entry n p inner`: no displacement)
        if (kind == "code") rr.code.push_back({lo, hi, disp});
        else if (kind == "data") rr.data.push_back({lo, hi, disp});
        else if (kind == "entry") {
            rr.entry[lo] = hi;
            // (`inner`: an address inside a function the region changed that a port or a generator names. Back to
            // NTSC-U, an address there is the stand-in's label, its NTSC-U entry plus the offset: region_to_ntsc)
            if (d != "inner") {
                rr.entry_rev[hi] = lo;
                rr.starts_rev[hi] = lo;
            }
        } else if (kind == "base") {
            rr.code_base = lo;
            rr.code_end = hi;
        }
    }
    std::sort(rr.data.begin(), rr.data.end());
    return rr;
}
// orig: (host) see RegionRuns: every unit's, read once (the levels 0..18 and the boot module)
const RegionRuns& region_runs(int unit) {
    static const std::map<int, RegionRuns> all = [] {
        std::map<int, RegionRuns> m;
        for (int n = 0; n < 19; ++n) m[n] = region_load(n);
        m[OVL_FRONT] = region_load(OVL_FRONT);
        return m;
    }();
    static const RegionRuns none;
    const auto it = all.find(unit);
    return it != all.end() ? it->second : none;
}

// orig: (host) see ovl_reloc.h's region_xlate: 0 when the address has no translation
uint32_t region_fwd(int unit, uint32_t a) {
    if (a < 0x0015ef00u) return a;  // (the base ELF below the overlays: the same on every region's disc)
    const RegionRuns& rr = region_runs(unit);
    if (rr.code_end != 0 ? (a >= rr.code_base && a < rr.code_end) : (!rr.code.empty() && a >= rr.code.front()[0]))
        return rr.code_fwd(a);
    return rr.data_fwd(a);
}

// orig: (host) an untranslated address, reported once
void log_once(const char* what, uint32_t a, const char* how = "has no translation: kept") {
    static std::set<std::pair<std::string, uint32_t>> seen;
    if (seen.insert({what, a}).second) std::fprintf(stderr, "[ovl_reloc] level %d: %s %08x %s\n", g_level, what, a, how);
}


// orig: (host) RAC1_PERF_CODE_MEMO (docs/performance.md): ovl_code_to_veldin's answers kept by address. A level's code address
// that is not a function's start (a jump table's label: every `switch` of the transliterated code asks for one) goes through
// three tree searches; the answer depends only on the translation's maps, so it is kept in a direct-mapped table, emptied
// whenever the maps change (ovl_reloc_set_level, an address ovl_xlate_code keeps). A slot is one word, the address above the
// answer: read and written whole, whichever thread asks. 0: no memo; 2: both, compared (ovl_code_memo_report's account).
constexpr uint32_t kCodeMemoSlots = 1024;
std::atomic<uint64_t> g_code_memo[kCodeMemoSlots];
uint64_t g_code_memo_calls = 0, g_code_memo_hits = 0, g_code_memo_checked = 0, g_code_memo_bad = 0;
const int g_code_memo_mode = [] {
    const char* e = std::getenv("RAC1_PERF_CODE_MEMO");
    return e == nullptr || *e == '\0' ? 1 : std::atoi(e);
}();
void code_memo_clear() {
    for (std::atomic<uint64_t>& m : g_code_memo) m.store(0, std::memory_order_relaxed);
}
}  // namespace

// orig: (host) see ovl_reloc.h
void ovl_reloc_set_level(int level) {
    g_data.clear();
    g_code.clear();
    g_code_rev.clear();
    g_code_kept.clear();
    g_code_size.clear();
    g_code_ptr.clear();
    g_code_ptr_rev.clear();
    g_ext_v.clear();
    g_ext_l.clear();
    g_level = level == OVL_NONE ? 0 : level;
    g_identity = true;
    code_memo_clear();
    ++g_ovl_gen;
    ++g_region_gen;  // (the loaded level's sites are native again; a context's level is foreign)
    // Veldin is the identity on the NTSC-U disc only: on another region's disc its overlay is the
    // region's own build, translated like any level's (level0.txt)
    if (level == OVL_NONE || (level == 0 && !disc_other_region())) return;
    g_identity = false;
    const std::string path = io::asset_path(level == OVL_FRONT ? std::string("generated/overlay_map/front_reloc.txt")
                                                               : "generated/overlay_map/level" + std::to_string(level) + ".txt");
    std::ifstream in(path);
    if (!in) {
        std::fprintf(stderr, "[ovl_reloc] %s missing (setup.sh): level %d kept on Veldin's addresses\n", path.c_str(), level);
        g_level = 0;
        g_identity = true;
        return;
    }
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind;
        uint32_t v = 0, l = 0, n = 0;
        s >> kind >> std::hex >> v >> l >> n;
        if (kind == "data") g_data[v] = l;
        else if (kind == "fn") {
            g_code[v] = l;
            g_code_rev[l] = v;
            g_code_size[v] = n;
        }
    }
    // the engine's data blocks with their extents (a level's own build only: not the boot module's, nor another region's)
    if (level != OVL_FRONT && !disc_other_region()) {
        std::ifstream ext(io::asset_path("generated/overlay_map/level" + std::to_string(level) + ".ext"));
        while (std::getline(ext, line)) {
            std::istringstream s(line);
            std::string kind;
            Extent e{};
            s >> kind >> std::hex >> e.veldin >> e.level >> e.size;
            if (kind != "ext" || e.size == 0) continue;
            g_ext_v[e.veldin] = e;
            g_ext_l[e.level] = e;
        }
        if (g_ext_v.empty())
            std::fprintf(stderr, "[ovl_reloc] level %d: no extents (%s/generated/overlay_map/level%d.ext: ./setup.sh): the nearest mapped address's rule\n", level,
                         io::asset_root().c_str(), level);
    }
    // the rows' level addresses are NTSC-U level N's: on another region's disc, the region
    // table's (a function the region changed: its entry, the NTSC-U port standing in; none: the row dropped)
    const bool other = region_other() && level != OVL_FRONT;
    auto add_row = [&](const VariantRow& r) {
        uint32_t l = r.level;
        if (other && (l = region_fwd(level, r.level)) == 0) {
            log_once("variant", r.level, "is code the region changed, its entry unknown: its row dropped");
            return;
        }
        g_code[r.veldin] = l;
        g_code_rev[l] = r.veldin;
        g_code_size[r.veldin] = r.size;
    };
    {  // the level's variants (hand-checked, variants_l1.cpp): the level's own function stands in for Veldin's
        size_t n = 0;
        const VariantRow* rows = ovl_variants(level, &n);
        for (size_t i = 0; i < n; ++i) add_row(rows[i]);
    }
    {  // the level's own sky dispatcher (sky_levels.cpp): Veldin's 0x23d968 is another function on every level
        VariantRow row;
        if (sky_dispatcher_row(level, &row)) add_row(row);
    }
    for (const auto& [v, l] : g_data)  // (the data lines in Veldin's code: code pointers)
        if (v >= 0x001eaa00u && v < 0x002f0cd0u && g_code.count(v) == 0) {
            g_code_ptr[v] = l;
            g_code_ptr_rev[l] = v;
        }
}

// orig: (host) see ovl_reloc.h
int ovl_reloc_level() { return g_level; }

// orig: (host) see ovl_reloc.h
bool ovl_reloc_identity() { return g_identity; }

namespace {
// orig: (host) the rule without extents: an offset into a mapped object (a port's folded field offset) is the nearest
// mapped address below, close enough, with its displacement -- certain when the mapped address above moved by the same
// amount (the block between them moved as one), else a guess (`*how` says so; kept, without a translation, too)
uint32_t nearest_rule(uint32_t a, const char** how) {
    *how = nullptr;
    auto hi = g_data.upper_bound(a);
    if (hi != g_data.begin()) {
        const auto lo = std::prev(hi);
        const uint32_t d = lo->second - lo->first;
        if (hi != g_data.end() && hi->second - hi->first == d) return a + d;
        if (a - lo->first < 0x100) {
            *how = "guessed from the object below";
            return a + d;
        }
    }
    *how = "has no translation: kept";
    return a;
}

// orig: (host) Veldin's address `a` inside an engine block of the level: its address there (with `*found`), else a
uint32_t ext_fwd(uint32_t a, bool* found) {
    auto it = g_ext_v.upper_bound(a);
    if (it != g_ext_v.begin() && a - std::prev(it)->first < std::prev(it)->second.size) {
        const Extent& e = std::prev(it)->second;
        *found = true;
        return a - e.veldin + e.level;
    }
    *found = false;
    return a;
}
}  // namespace

// orig: (host) see ovl_reloc.h
uint32_t ovl_xlate_data(uint32_t a) {
    if (g_identity) return a;
    const auto it = g_data.find(a);
    if (it != g_data.end()) return it->second;
    if (!g_ext_v.empty()) {
        // an offset into an engine block (a port's folded field offset, an element of an engine array): its displacement
        bool found = false;
        const uint32_t l = ext_fwd(a, &found);
        if (!found) log_once("data", a, "is in no engine object of the level (its own data, or Veldin's): kept");
        static const bool diff = std::getenv("RAC1_OVL_EXT_DIFF") != nullptr;
        if (diff) {  // (testing) where the extents and the nearest mapped address's rule disagree
            const char* how = nullptr;
            const uint32_t old = nearest_rule(a, &how);
            if (old != l) {
                char msg[96];
                std::snprintf(msg, sizeof msg, "the extents give %08x, the nearest mapped address's rule %08x", l, old);
                log_once("data", a, msg);
            }
        }
        return l;
    }
    const char* how = nullptr;
    const uint32_t l = nearest_rule(a, &how);
    if (how != nullptr) log_once("data", a, how);
    return l;
}

// orig: (host) see ovl_reloc.h
uint32_t ovl_data_to_veldin(uint32_t a) {
    if (g_identity) return a;
    auto it = g_ext_l.upper_bound(a);
    if (it == g_ext_l.begin()) return 0;
    const Extent& e = std::prev(it)->second;
    return a - e.level < e.size ? a - e.level + e.veldin : 0;
}

// orig: (host) see ovl_reloc.h
bool ovl_has_extents() { return g_identity || !g_ext_v.empty(); }

// orig: (host) see ovl_reloc.h
uint32_t ovl_code_lookup(uint32_t a) {
    if (g_identity) return a;
    const auto it = g_code.find(a);
    if (it != g_code.end()) return it->second;
    // a code pointer the level's matched code builds (lui + addiu; the maps' data lines): the
    // level's own function there, even when it is not the same code as Veldin's (a variant:
    // its dispatch finds no port and logs it)
    const auto ptr = g_data.find(a);
    if (ptr != g_data.end()) return ptr->second;
    // inside a mapped function (a callback address in the middle of one): its displacement
    auto lo = g_code.upper_bound(a);
    if (lo != g_code.begin()) {
        auto start = std::prev(lo);
        if (a - start->first < g_code_size[start->first]) return a + (start->second - start->first);
    }
    return 0;
}

// orig: (host) see ovl_reloc.h
uint32_t ovl_xlate_code(uint32_t a) {
    if (g_identity) return a;
    if (const uint32_t l = ovl_code_lookup(a); l != 0) return l;
    // a code pointer the matched code builds, to a function that is other code in the level
    // (a variant): the level's own function
    if (const auto p = g_code_ptr.find(a); p != g_code_ptr.end()) return p->second;
    log_once("code", a);
    if (g_code_kept.insert(a).second) code_memo_clear();  // (ovl_code_to_veldin now answers `a` for it)
    return a;
}

namespace {
uint32_t code_to_veldin_search(uint32_t a);
}  // namespace

// orig: (host) see ovl_reloc.h
void ovl_code_memo_report() {
    if (g_code_memo_calls == 0) return;
    std::printf("[perf] code memo (RAC1_PERF_CODE_MEMO=%d): %llu translations of a level's code address, %llu from the memo", g_code_memo_mode,
                static_cast<unsigned long long>(g_code_memo_calls), static_cast<unsigned long long>(g_code_memo_hits));
    if (g_code_memo_mode == 2)
        std::printf("; check: %llu compared, %llu differing", static_cast<unsigned long long>(g_code_memo_checked), static_cast<unsigned long long>(g_code_memo_bad));
    std::printf("\n");
}

// orig: (host) see ovl_reloc.h
uint32_t ovl_code_to_veldin(uint32_t a) {
    if (g_ctx_levels != 0 && ctx_is_tag(a)) return 0;  // (a session: a level's own function, through its context: no Veldin one)
    if (g_identity) return a;
    if (g_code_memo_mode == 0 || a == 0) return code_to_veldin_search(a);
    ++g_code_memo_calls;
    std::atomic<uint64_t>& slot = g_code_memo[(a * 0x9e3779b9u) >> 22];
    const uint64_t e = slot.load(std::memory_order_relaxed);
    if (static_cast<uint32_t>(e >> 32) == a) {
        ++g_code_memo_hits;
        if (g_code_memo_mode == 2) {
            ++g_code_memo_checked;
            const uint32_t v = code_to_veldin_search(a);
            if (v != static_cast<uint32_t>(e)) {
                if (g_code_memo_bad++ < 8) std::fprintf(stderr, "[perf] code memo: %08x kept as %08x, is %08x\n", a, static_cast<uint32_t>(e), v);
                return v;
            }
        }
        return static_cast<uint32_t>(e);
    }
    const uint32_t v = code_to_veldin_search(a);
    slot.store(static_cast<uint64_t>(a) << 32 | v, std::memory_order_relaxed);
    return v;
}

namespace {
// orig: (host) ovl_code_to_veldin's search of the maps
uint32_t code_to_veldin_search(uint32_t a) {
    const auto it = g_code_rev.find(a);
    if (it != g_code_rev.end()) return it->second;
    if (g_code_kept.count(a) != 0) return a;  // (a Veldin address the translation kept: its port)
    auto lo = g_code_rev.upper_bound(a);  // (inside a mapped function, as above)
    if (lo != g_code_rev.begin()) {
        auto start = std::prev(lo);
        if (a - start->first < g_code_size[start->second]) return a - start->first + start->second;
    }
    // the level's variant of a Veldin function (a code pointer pair): Veldin's port stands in
    if (const auto p = g_code_ptr_rev.find(a); p != g_code_ptr_rev.end()) return p->second;
    return g_code_kept.count(a) != 0 ? a : 0;  // (a Veldin address the translation kept)
}
}  // namespace

// orig: (host) see ovl_reloc.h
bool region_other() {
    static const bool other = disc_other_region();
    return other;
}

// orig: (host) see ovl_reloc.h
uint32_t region_xlate(int unit, uint32_t a) {
    if (!region_other()) return a;
    const uint32_t v = region_fwd(unit, a);
    static std::mutex mu;  // (the sites' first uses: any thread)
    std::lock_guard<std::mutex> lock(mu);
    static std::set<std::pair<int, uint32_t>> seen;
    const char* who = unit == OVL_FRONT ? "boot module" : "level";
    if (v == 0) {
        if (seen.insert({unit, a}).second)
            std::fprintf(stderr, "[region] %s %d: %08x has no translation (code the region changed): kept\n", who, unit, a);
        return a;
    }
    const RegionRuns& rr = region_runs(unit);
    if (rr.entry.count(a) != 0 && seen.insert({unit, a}).second)
        std::fprintf(stderr, "[region] %s %d: %08x is a function the region changed: the port at that address runs for the disc's at %08x\n", who, unit, a, v);
    return v;
}

// orig: (host) see ovl_reloc.h
uint32_t region_site_xlate(int unit, uint32_t a) {
    if (g_ctx_levels != 0 && unit >= 0 && unit < 32 && (g_ctx_levels >> unit & 1u) != 0 && unit != g_level && g_ctx_xlate != nullptr)
        return g_ctx_xlate(unit, a);
    return region_xlate(unit, a);
}

// orig: (host) see ovl_reloc.h
uint32_t region_to_ntsc(int unit, uint32_t p) {
    if (!region_other() || p < 0x0015ef00u) return p;
    const RegionRuns& rr = region_runs(unit);
    const uint32_t n = rr.code_back(p);
    if (n != 0) return n;
    // (a build whose core moved, PAL 2.00) an address inside a function the region changed: the generated stand-in of it
    // (tools/overlay_match/region_view.py's StandIn) is the disc's code placed at the NTSC-U entry, so its labels are the
    // NTSC-U entry plus the offset in the disc's function: a `case` of its jump table, say. The function is the nearest
    // changed entry below, up to the next one or the next code run.
    if (region_core_moved() && !rr.starts_rev.empty()) {
        auto it = rr.starts_rev.upper_bound(p);
        if (it != rr.starts_rev.begin()) {
            --it;
            const uint32_t q = it->first;
            bool inside = it == std::prev(rr.starts_rev.end()) || p < std::next(it)->first;
            for (const auto& r : rr.code)
                if (r[0] + r[2] > q && r[0] + r[2] <= p) inside = false;
            if (inside && p - q < 0x10000u) return it->second + (p - q);
        }
    }
    return p;
}

}  // namespace rac1
