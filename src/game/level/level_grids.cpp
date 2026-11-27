// The level's two lookup grids as host copies (see level_grids.h): the parses of the zone grid and of the camera-volume grid
// from EE memory, the kept copies, the switch, the check mode and the zone masks' copies.
#include <game/level/level_grids.h>

#include <game/math/vecmath.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>

namespace rac1 {

namespace {

// The EE bytes a parse reads: RAM from 4 KB up to the RAM's end, through its base pointer (a parse is the bridge: it is not
// a reader the bridge guard's census looks for), little-endian as the EE stores them (collision_scene.cpp's rules).
bool ram_ok(uint32_t a, uint64_t n) { return a >= 0x1000u && static_cast<uint64_t>(a) + n <= ee_ram_size(); }
const uint8_t* ram(uint32_t a) { return ee_get_ram_base() + a; }
uint32_t rd16(uint32_t a) {
    const uint8_t* p = ram(a);
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8;
}
uint32_t rd32(uint32_t a) {
    const uint8_t* p = ram(a);
    return static_cast<uint32_t>(p[0]) | static_cast<uint32_t>(p[1]) << 8 | static_cast<uint32_t>(p[2]) << 16 | static_cast<uint32_t>(p[3]) << 24;
}

std::pair<uint32_t, uint32_t> extent(const std::vector<std::pair<uint32_t, uint32_t>>& src) {
    uint32_t lo = 0xffffffffu, hi = 0;
    for (const auto& [a, n] : src) {
        lo = std::min(lo, a);
        hi = std::max(hi, a + n);
    }
    return src.empty() ? std::make_pair(0u, 0u) : std::make_pair(lo, hi);
}

size_t bytes_of(const std::vector<std::pair<uint32_t, uint32_t>>& src) {
    size_t n = 0;
    for (const auto& r : src) n += r.second;
    return n;
}

// FNV-1a (64 bits) over the bytes of the ranges, in order
uint64_t hash_ranges(const std::vector<std::pair<uint32_t, uint32_t>>& src) {
    uint64_t h = 0xcbf29ce484222325ull;
    for (const auto& [a, n] : src) {
        if (!ram_ok(a, n)) return h ^ 0x5bd1e995u;  // (the RAM shrank: not the bytes parsed)
        const uint8_t* p = ram(a);
        uint32_t i = 0;
        for (; i + 8 <= n; i += 8) {
            uint64_t v;
            std::memcpy(&v, p + i, 8);
            h = (h ^ v) * 0x100000001b3ull;
        }
        for (; i < n; ++i) h = (h ^ p[i]) * 0x100000001b3ull;
        h = (h ^ n) * 0x100000001b3ull;
    }
    return h;
}

// the most any level's grid holds, far above: a pointer to something else
constexpr size_t kMaxEntries = size_t{1} << 22;
constexpr size_t kMaxRecords = size_t{1} << 20;

}  // namespace

// ---- the zone grid ----

struct ZoneGridParser {
    ZoneGridCopy& s;
    std::map<std::pair<uint32_t, int>, int32_t> rows;  // (address, kind) -> row index

    bool src(uint32_t a, uint64_t n) {
        if (!ram_ok(a, n)) return false;
        s.src_.emplace_back(a, static_cast<uint32_t>(n));
        return true;
    }
    // a y row (kind 1: its entries x rows' offsets / 4) or an x row (kind 2: its entries mask indices) at `a`; -2: not held
    int32_t row(uint32_t a, int kind) {
        const auto it = rows.find({a, kind});
        if (it != rows.end()) return it->second;
        if (!ram_ok(a, 4)) return -2;
        const int32_t first = static_cast<int32_t>(rd16(a)), count = static_cast<int32_t>(rd16(a + 2));
        if (s.ent_.size() + static_cast<size_t>(count) > kMaxEntries || !src(a, 4 + uint64_t(count) * 2)) return -2;
        const int32_t r = static_cast<int32_t>(s.rows_.size());
        const uint32_t at = static_cast<uint32_t>(s.ent_.size());
        s.rows_.push_back(ZoneGridCopy::Row{first, count, at, a});
        s.ent_.resize(s.ent_.size() + static_cast<size_t>(count), -1);
        s.raw_.resize(s.raw_.size() + static_cast<size_t>(count), 0);
        rows.emplace(std::make_pair(a, kind), r);
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t v = rd16(a + 4 + static_cast<uint32_t>(i) * 2);
            s.raw_[at + static_cast<uint32_t>(i)] = static_cast<uint16_t>(v);
            if (kind == 1) {
                if (v == 0) continue;
                const int32_t x = row(s.g_ + v * 4, 2);
                if (x < -1) return -2;
                s.ent_[at + static_cast<uint32_t>(i)] = x;
            } else {
                ++s.cells_;
                if (v == 0xffff) continue;
                s.ent_[at + static_cast<uint32_t>(i)] = static_cast<int32_t>(v);
                s.top_ = std::max(s.top_, static_cast<int32_t>(v));
            }
        }
        return r;
    }
    bool run() {
        const uint32_t g = s.g_;
        if (!ram_ok(g, 8)) return false;
        s.moff_ = rd32(g);
        const int32_t first = static_cast<int32_t>(rd16(g + 4)), count = static_cast<int32_t>(rd16(g + 6));
        if (!src(g, 8 + uint64_t(count) * 2)) return false;
        s.z_ = ZoneGridCopy::Row{first, count, 0, g + 4};
        s.ent_.assign(static_cast<size_t>(count), -1);
        s.raw_.assign(static_cast<size_t>(count), 0);
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t v = rd16(g + 8 + static_cast<uint32_t>(i) * 2);
            s.raw_[static_cast<uint32_t>(i)] = static_cast<uint16_t>(v);
            if (v == 0) continue;
            const int32_t y = row(g + v * 4, 1);
            if (y < -1) return false;
            s.ent_[static_cast<uint32_t>(i)] = y;
        }
        s.masks_ = g + s.moff_;
        if (s.top_ >= 0) {
            const uint32_t base = s.masks_ & ~15u, n = static_cast<uint32_t>(s.top_ + 1) * 0x80u;
            if (!src(base, n)) return false;
            s.mbytes_.assign(ram(base), ram(base) + n);
        }
        return true;
    }
};

std::shared_ptr<const ZoneGridCopy> ZoneGridCopy::parse(uint32_t g) {
    auto s = std::make_shared<ZoneGridCopy>();
    s->g_ = g;
    ZoneGridParser p{*s, {}};
    if (!p.run()) return nullptr;
    s->hash_ = hash_ranges(s->src_);
    return s;
}

template <bool Check>
uint32_t ZoneGridCopy::cell(int32_t x, int32_t y, int32_t z, bool* bad) const {
    auto chk = [&](uint32_t a, uint32_t v, bool word) {
        if constexpr (Check) {
            const uint32_t e = word ? ee_read<uint32_t>(a) : ee_read<uint16_t>(a);
            if (e != v) {
                ld_grids_differs("zone grid", g_, a, v, e);
                *bad = true;
            }
        }
    };
    chk(g_, moff_, true);
    chk(z_.ee, static_cast<uint32_t>(z_.first), false);
    chk(z_.ee + 2, static_cast<uint32_t>(z_.count), false);
    z -= z_.first;
    if (z < 0 || !(z < z_.count)) return 0;
    const uint32_t zi = z_.at + static_cast<uint32_t>(z);
    chk(z_.ee + 4 + static_cast<uint32_t>(z) * 2, raw_[zi], false);
    const int32_t yr = ent_[zi];
    if (yr < 0) return 0;
    const Row& Y = rows_[static_cast<uint32_t>(yr)];
    chk(Y.ee, static_cast<uint32_t>(Y.first), false);
    chk(Y.ee + 2, static_cast<uint32_t>(Y.count), false);
    y -= Y.first;
    if (y < 0 || !(y < Y.count)) return 0;
    const uint32_t yi = Y.at + static_cast<uint32_t>(y);
    chk(Y.ee + 4 + static_cast<uint32_t>(y) * 2, raw_[yi], false);
    const int32_t xr = ent_[yi];
    if (xr < 0) return 0;
    const Row& X = rows_[static_cast<uint32_t>(xr)];
    chk(X.ee, static_cast<uint32_t>(X.first), false);
    chk(X.ee + 2, static_cast<uint32_t>(X.count), false);
    x -= X.first;
    if (x < 0 || !(x < X.count)) return 0;
    const uint32_t xi = X.at + static_cast<uint32_t>(x);
    chk(X.ee + 4 + static_cast<uint32_t>(x) * 2, raw_[xi], false);
    const int32_t e = ent_[xi];
    return e < 0 ? 0u : masks_ + static_cast<uint32_t>(e) * 0x80u;
}
template uint32_t ZoneGridCopy::cell<false>(int32_t, int32_t, int32_t, bool*) const;
template uint32_t ZoneGridCopy::cell<true>(int32_t, int32_t, int32_t, bool*) const;

uint64_t ZoneGridCopy::hash_ee() const { return hash_ranges(src_); }
size_t ZoneGridCopy::source_bytes() const { return bytes_of(src_); }
std::pair<uint32_t, uint32_t> ZoneGridCopy::source_extent() const { return extent(src_); }

bool ZoneGridCopy::same_as(const ZoneGridCopy& o, std::string* why) const {
    auto no = [&](const char* w) {
        if (why != nullptr) *why = w;
        return false;
    };
    if (g_ != o.g_ || moff_ != o.moff_ || masks_ != o.masks_) return no("grid pointer or masks' offset");
    if (z_.first != o.z_.first || z_.count != o.z_.count) return no("z row");
    if (rows_.size() != o.rows_.size()) return no("row count");
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].first != o.rows_[i].first || rows_[i].count != o.rows_[i].count || rows_[i].at != o.rows_[i].at || rows_[i].ee != o.rows_[i].ee)
            return no("a row");
    if (ent_ != o.ent_ || raw_ != o.raw_) return no("an entry");
    if (top_ != o.top_ || mbytes_ != o.mbytes_) return no("the masks");
    return true;
}

// ---- the camera-volume grid ----

std::shared_ptr<const CamGridCopy> CamGridCopy::parse(uint32_t tab) {
    if ((tab & 3u) != 0 || !ram_ok(tab, uint64_t{kCells} * 4)) return nullptr;
    auto s = std::make_shared<CamGridCopy>();
    s->tab_ = tab;
    s->words_.resize(kCells);
    s->cell_.assign(kCells, -1);
    s->src_.emplace_back(tab, static_cast<uint32_t>(kCells) * 4);
    for (int32_t c = 0; c < kCells; ++c) s->words_[static_cast<uint32_t>(c)] = rd32(tab + static_cast<uint32_t>(c) * 4);
    std::map<uint32_t, int32_t> lists;  // list address -> list index (-2: not held)
    for (int32_t c = 0; c < kCells; ++c) {
        const uint32_t w = s->words_[static_cast<uint32_t>(c)];
        if (w == 0) continue;
        const uint32_t l = w + tab;
        int32_t li;
        const auto it = lists.find(l);
        if (c >= 64 * 64) {
            li = -2;  // (the 65 words past the table are the lists' bytes, read as a cell for x or y 64: EE memory for those)
        } else if (it != lists.end()) {
            li = it->second;
        } else {
            li = -2;
            if ((l & 3u) == 0 && ram_ok(l, 4)) {
                const int32_t count = static_cast<int32_t>(rd32(l));
                const uint32_t n = static_cast<uint32_t>(std::max(count, 0));
                if (s->recs_.size() + n <= kMaxRecords && (n == 0 || ram_ok(l + 0x10, uint64_t{n} * 0x30))) {
                    li = static_cast<int32_t>(s->lists_.size());
                    s->lists_.push_back(List{count, static_cast<uint32_t>(s->recs_.size()), l});
                    s->src_.emplace_back(l, 4);
                    for (uint32_t i = 0; i < n; ++i) {
                        const uint32_t e = l + 0x10 + i * 0x30;
                        Record r{};
                        for (uint32_t k = 0; k < 4; ++k) r.w[k] = rd32(e + k * 4);
                        r.type = static_cast<int32_t>(rd32(e + 0x10));
                        r.index = static_cast<int32_t>(rd32(e + 0x14));
                        s->recs_.push_back(r);
                        s->src_.emplace_back(e, 0x18);
                    }
                }
            }
            lists.emplace(l, li);
        }
        s->cell_[static_cast<uint32_t>(c)] = li;
        if (li == -2) ++s->unheld_;
    }
    s->all_held_ = true;
    for (int32_t c = 0; c < 64 * 64; ++c)
        if (s->cell_[static_cast<uint32_t>(c)] == -2) s->all_held_ = false;
    if (s->all_held_) {
        for (int32_t c = 0; c < 64 * 64; ++c) {
            const int32_t li = s->cell_[static_cast<uint32_t>(c)];
            if (li < 0) continue;
            const List& L = s->lists_[static_cast<uint32_t>(li)];
            for (int32_t i = 0; i < L.count; ++i)
                if (s->record(L, i).type == 5) s->pass_.push_back(s->record(L, i).index);
        }
    }
    s->hash_ = hash_ranges(s->src_);
    return s;
}

uint64_t CamGridCopy::hash_ee() const { return hash_ranges(src_); }
size_t CamGridCopy::source_bytes() const { return bytes_of(src_); }
std::pair<uint32_t, uint32_t> CamGridCopy::source_extent() const { return extent(src_); }

bool CamGridCopy::same_as(const CamGridCopy& o, std::string* why) const {
    auto no = [&](const char* w) {
        if (why != nullptr) *why = w;
        return false;
    };
    if (tab_ != o.tab_) return no("grid pointer");
    if (words_ != o.words_ || cell_ != o.cell_) return no("a cell");
    if (lists_.size() != o.lists_.size()) return no("list count");
    for (size_t i = 0; i < lists_.size(); ++i)
        if (lists_[i].count != o.lists_[i].count || lists_[i].at != o.lists_[i].at || lists_[i].ee != o.lists_[i].ee) return no("a list");
    if (recs_.size() != o.recs_.size()) return no("record count");
    for (size_t i = 0; i < recs_.size(); ++i)
        if (std::memcmp(recs_[i].w, o.recs_[i].w, sizeof recs_[i].w) != 0 || recs_[i].type != o.recs_[i].type || recs_[i].index != o.recs_[i].index)
            return no("a record");
    if (pass_ != o.pass_ || all_held_ != o.all_held_) return no("the pass's spheres");
    return true;
}

// ---- the switch, the kept copies ----

namespace {

int env_int(const char* name, int dflt) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != 0 ? std::atoi(v) : dflt;
}
std::atomic<bool> g_native{[] {  // (native unless RAC1_LD_GRIDS=ee)
    const char* v = std::getenv("RAC1_LD_GRIDS");
    return v == nullptr || std::strcmp(v, "ee") != 0;
}()};
std::atomic<bool> g_check{env_int("RAC1_LD_GRIDS_CHECK", 0) != 0};
const bool g_trace = std::getenv("RAC1_LD_GRIDS_TRACE") != nullptr;

std::atomic<uint64_t> g_gen{1};
std::mutex g_mu;  // the kept copies and the counts
LdGridsStats g_stats;
bool g_error_printed = false;

constexpr size_t kKeep = 4;  // the copies kept per grid

template <class S>
struct Kept {
    uint32_t ptr;
    std::shared_ptr<const S> s;  // null: the pointer is not held (read EE)
};
std::vector<Kept<ZoneGridCopy>> g_zones;  // most recent last
std::vector<Kept<CamGridCopy>> g_cams;

template <class S>
struct Current {
    bool valid = false;
    uint32_t ptr = 0;
    uint64_t gen = 0;
    std::shared_ptr<const S> s;
};
thread_local Current<ZoneGridCopy> t_zone;
thread_local Current<CamGridCopy> t_cam;

template <class S>
void trace_parse(const S& s, uint32_t ptr, const char* what, double ms);
template <>
void trace_parse(const ZoneGridCopy& s, uint32_t ptr, const char* what, double ms) {
    const std::pair<uint32_t, uint32_t> x = s.source_extent();
    std::fprintf(stderr, "[ld-grids] %s 0x%08x: parsed, %zu rows, %zu cells, %d masks, %zu source bytes in 0x%08x .. 0x%08x, %.3f ms\n", what, ptr,
                 s.row_count(), s.cell_count(), s.mask_count(), s.source_bytes(), x.first, x.second, ms);
}
template <>
void trace_parse(const CamGridCopy& s, uint32_t ptr, const char* what, double ms) {
    const std::pair<uint32_t, uint32_t> x = s.source_extent();
    std::fprintf(stderr, "[ld-grids] %s 0x%08x: parsed, %zu lists, %zu records, %d cells read from EE, %zu source bytes in 0x%08x .. 0x%08x, %.3f ms\n",
                 what, ptr, s.list_count(), s.record_count(), s.unheld_cells(), s.source_bytes(), x.first, x.second, ms);
}

template <class S>
std::shared_ptr<const S> parse_timed(uint32_t ptr, const char* what) {
    const auto t0 = std::chrono::steady_clock::now();
    std::shared_ptr<const S> s = S::parse(ptr);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ++g_stats.parses;
    g_stats.parse_ms += ms;
    if (s == nullptr) ++g_stats.unheld;
    if (g_trace) {
        if (s != nullptr) trace_parse(*s, ptr, what, ms);
        else std::fprintf(stderr, "[ld-grids] %s 0x%08x: not held (EE read), %.3f ms\n", what, ptr, ms);
    }
    return s;
}

// the copy for `ptr`: a kept one whose bytes' hash is unchanged, or a new parse (g_mu held; called when this thread's pointer
// or the generation changed, so the hash is taken at a pointer's switch or a level's start only)
template <class S>
std::shared_ptr<const S> part(std::vector<Kept<S>>& kept, uint32_t ptr, const char* what) {
    for (size_t i = 0; i < kept.size(); ++i) {
        const Kept<S> k = kept[i];
        if (k.ptr != ptr) continue;
        kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(i));
        if (k.s == nullptr || k.s->hash_ee() != k.s->source_hash()) break;  // (parsed again below)
        ++g_stats.reused;
        if (g_trace) std::fprintf(stderr, "[ld-grids] %s 0x%08x: the kept copy taken again (its bytes' hash unchanged)\n", what, ptr);
        kept.push_back(Kept<S>{ptr, k.s});  // (the most recent last)
        return k.s;
    }
    std::shared_ptr<const S> s = parse_timed<S>(ptr, what);
    kept.push_back(Kept<S>{ptr, s});
    if (kept.size() > kKeep) kept.erase(kept.begin());
    return s;
}

template <class S>
std::shared_ptr<const S> current(Current<S>& cur, std::vector<Kept<S>>& kept, uint32_t ptr, const char* what) {
    const uint64_t gen = g_gen.load(std::memory_order_acquire);
    if (cur.valid && cur.ptr == ptr && cur.gen == gen) return cur.s;
    std::shared_ptr<const S> s;
    {
        const std::lock_guard<std::mutex> lk(g_mu);
        s = part(kept, ptr, what);
    }
    cur.valid = true;
    cur.ptr = ptr;
    cur.gen = gen;
    cur.s = s;
    return s;
}

template <class S>
std::shared_ptr<const S> stale(Current<S>& cur, std::vector<Kept<S>>& kept, uint32_t ptr, bool after_fresh, const char* what) {
    const uint64_t gen = g_gen.load(std::memory_order_acquire);
    std::shared_ptr<const S> s;
    {
        const std::lock_guard<std::mutex> lk(g_mu);
        if (after_fresh) {
            ++g_stats.parse_errors;
            if (!g_error_printed) {
                g_error_printed = true;
                std::fprintf(stderr, "[ld-grids] ERROR: the %s parsed from 0x%08x differs from EE memory right after its parse\n", what, ptr);
            }
            return cur.s;
        }
        ++g_stats.stale;
        if (g_trace) std::fprintf(stderr, "[ld-grids] %s 0x%08x: differs from EE memory (check mode): parsed again\n", what, ptr);
        kept.erase(std::remove_if(kept.begin(), kept.end(), [ptr](const Kept<S>& k) { return k.ptr == ptr; }), kept.end());
        s = part(kept, ptr, what);
    }
    cur.valid = true;
    cur.ptr = ptr;
    cur.gen = gen;
    cur.s = s;
    return s;
}

// the 0x80 mask bytes `m` (from the copy) against EE memory's at (src & ~15) on
bool mask_differs(const ZoneGridCopy& z, uint32_t src, const uint8_t* m) {
    bool bad = false;
    for (uint32_t k = 0; k < 0x80; k += 4) {
        uint32_t v;
        std::memcpy(&v, m + k, 4);
        const uint32_t a = (src & ~15u) + k, e = ee_read<uint32_t>(a);
        if (e != v) {
            ld_grids_differs("zone mask", z.grid(), a, v, e);
            bad = true;
        }
    }
    return bad;
}

// the copy's mask for `src` of the zone grid `g` (null: EE memory), checked in the check mode
const uint8_t* zone_mask(uint32_t g, uint32_t src, std::shared_ptr<const ZoneGridCopy>& keep) {
    if (!ld_grids_native()) return nullptr;
    keep = ld_grids_zone(g);
    if (keep == nullptr) return nullptr;
    const uint8_t* m = keep->mask(src);
    if (m == nullptr || !ld_grids_check() || !mask_differs(*keep, src, m)) return m;
    for (bool fresh = false;; fresh = true) {
        keep = ld_grids_zone_stale(g, fresh);
        if (keep == nullptr) return nullptr;
        m = keep->mask(src);
        if (m == nullptr || fresh || !mask_differs(*keep, src, m)) return m;
    }
}

}  // namespace

bool ld_grids_native() { return g_native.load(std::memory_order_relaxed); }
void ld_grids_set_native(bool on) { g_native.store(on); }
bool ld_grids_check() { return g_check.load(std::memory_order_relaxed); }
void ld_grids_set_check(bool on) { g_check.store(on); }

std::shared_ptr<const ZoneGridCopy> ld_grids_zone(uint32_t g) { return current(t_zone, g_zones, g, "zone grid"); }
std::shared_ptr<const CamGridCopy> ld_grids_cam(uint32_t tab) { return current(t_cam, g_cams, tab, "camera-volume grid"); }
std::shared_ptr<const ZoneGridCopy> ld_grids_zone_stale(uint32_t g, bool after_fresh) { return stale(t_zone, g_zones, g, after_fresh, "zone grid"); }
std::shared_ptr<const CamGridCopy> ld_grids_cam_stale(uint32_t tab, bool after_fresh) {
    return stale(t_cam, g_cams, tab, after_fresh, "camera-volume grid");
}

void ld_grids_differs(const char* what, uint32_t grid, uint32_t ee, uint32_t copy_value, uint32_t ee_value) {
    uint64_t n;
    {
        const std::lock_guard<std::mutex> lk(g_mu);
        n = ++g_stats.differs;
    }
    if (n <= 50) {
        std::fprintf(stderr, "[ld-grids] differs: %s 0x%08x at 0x%08x: the copy's 0x%08x, EE memory's 0x%08x\n", what, grid, ee, copy_value, ee_value);
    } else if ((n & (n - 1)) == 0) {
        std::fprintf(stderr, "[ld-grids] differs: %llu values so far\n", static_cast<unsigned long long>(n));
    }
}

void ld_grids_level_started() {
    {
        const std::lock_guard<std::mutex> lk(g_mu);
        g_zones.clear();
        g_cams.clear();
    }
    g_gen.fetch_add(1, std::memory_order_acq_rel);
}

void ld_grids_mask_copy(uint32_t g, uint32_t dst, uint32_t src) {
    std::shared_ptr<const ZoneGridCopy> keep;
    const uint8_t* m = zone_mask(g, src, keep);
    if (m == nullptr) {
        mem_copy128(dst, src, 0x80);
        return;
    }
    for (uint32_t k = 0; k < 0x80; k += 16) {  // (mem_copy128's stores: each quadword to (dst + k) & ~15)
        uint64_t lo, hi;
        std::memcpy(&lo, m + k, 8);
        std::memcpy(&hi, m + k + 8, 8);
        ee_write<uint64_t>((dst + k) & ~15u, lo);
        ee_write<uint64_t>(((dst + k) & ~15u) + 8, hi);
    }
}

void ld_grids_mask_or(uint32_t g, uint32_t dst, uint32_t a, uint32_t b) {
    std::shared_ptr<const ZoneGridCopy> keep;
    const uint8_t* m = zone_mask(g, b, keep);
    if (m == nullptr) {
        mem_or128(dst, a, b, 0x80);
        return;
    }
    for (uint32_t k = 0; k < 0x80; k += 16) {  // (mem_or128's: a's quadword read, ored with b's, stored to (dst + k) & ~15)
        uint64_t lo, hi;
        std::memcpy(&lo, m + k, 8);
        std::memcpy(&hi, m + k + 8, 8);
        lo = ee_read<uint64_t>((a + k) & ~15u) | lo;
        hi = ee_read<uint64_t>(((a + k) & ~15u) + 8) | hi;
        ee_write<uint64_t>((dst + k) & ~15u, lo);
        ee_write<uint64_t>(((dst + k) & ~15u) + 8, hi);
    }
}

LdGridsStats ld_grids_stats() {
    const std::lock_guard<std::mutex> lk(g_mu);
    return g_stats;
}

}  // namespace rac1
