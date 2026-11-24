// The level's collision geometry as host structures (see collision_scene.h): the parses of the tree and of the hero's mesh
// list from EE memory, the kept parts, the switch and the check mode.
#include <game/collision/collision_scene.h>

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
// a reader the bridge guard's census looks for), little-endian as the EE stores them.
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

// the lowest address and the end of the ranges
std::pair<uint32_t, uint32_t> extent(const std::vector<std::pair<uint32_t, uint32_t>>& src) {
    uint32_t lo = 0xffffffffu, hi = 0;
    for (const auto& [a, n] : src) {
        lo = std::min(lo, a);
        hi = std::max(hi, a + n);
    }
    return src.empty() ? std::make_pair(0u, 0u) : std::make_pair(lo, hi);
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

// the most any level's data holds, far above: a pointer to something else
constexpr size_t kMaxEntries = size_t{1} << 22;
constexpr size_t kMaxWords = size_t{1} << 23;
constexpr int32_t kMaxChunks = 1 << 16;

}  // namespace

// ---- the tree ----

struct CollTreeParser {
    CollTreeScene& s;
    std::map<std::pair<uint32_t, int>, int32_t> rows;  // (address, kind) -> row index
    std::map<uint32_t, int32_t> cells;                 // cell word -> cell index

    bool src(uint32_t a, uint64_t n) {
        if (!ram_ok(a, n)) return false;
        s.src_.emplace_back(a, static_cast<uint32_t>(n));
        return true;
    }
    // a cell word's fetched words: ((word >> 8) + tree) aligned down to 16 bytes, (word & 0xff) quadwords
    int32_t cell(uint32_t w) {
        const auto it = cells.find(w);
        if (it != cells.end()) return it->second;
        const uint32_t a = ((w >> 8) + s.tree_) & ~15u, n = (w & 0xffu) * 4;
        if (s.words_.size() + n > kMaxWords || !src(a, uint64_t{n} * 4)) return -2;
        const int32_t c = static_cast<int32_t>(s.cells_.size());
        s.cells_.push_back(CollTreeScene::CellData{w, static_cast<uint32_t>(s.words_.size())});
        for (uint32_t i = 0; i < n; ++i) s.words_.push_back(rd32(a + i * 4));
        cells.emplace(w, c);
        return c;
    }
    // a y row (kind 1: its entries u32 offsets of x rows) or an x row (kind 2: its entries cell words) at `a`
    int32_t row(uint32_t a, int kind) {
        const auto it = rows.find({a, kind});
        if (it != rows.end()) return it->second;
        if (!ram_ok(a, 4)) return -2;
        const int32_t first = static_cast<int32_t>(rd16(a)), count = static_cast<int32_t>(rd16(a + 2));
        if (s.ent_.size() + static_cast<size_t>(count) > kMaxEntries || !src(a, 4 + uint64_t(count) * 4)) return -2;
        const int32_t r = static_cast<int32_t>(s.rows_.size());
        const uint32_t at = static_cast<uint32_t>(s.ent_.size());
        s.rows_.push_back(CollTreeScene::Row{first, count, at});
        s.ent_.resize(s.ent_.size() + static_cast<size_t>(count), -1);
        rows.emplace(std::make_pair(a, kind), r);
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t e = rd32(a + 4 + static_cast<uint32_t>(i) * 4);
            if (e == 0) continue;
            const int32_t v = kind == 1 ? row(e + s.tree_, 2) : cell(e);
            if (v < -1) return -2;
            s.ent_[at + static_cast<uint32_t>(i)] = v;
        }
        return r;
    }
    bool run() {
        const uint32_t t = s.tree_;
        if (!ram_ok(t, 4)) return false;
        const int32_t first = static_cast<int32_t>(rd16(t)), count = static_cast<int32_t>(rd16(t + 2));
        if (!src(t, 4 + uint64_t(count) * 2)) return false;
        s.z_ = CollTreeScene::Row{first, count, 0};
        s.ent_.assign(static_cast<size_t>(count), -1);
        for (int32_t i = 0; i < count; ++i) {
            const uint32_t e = rd16(t + 4 + static_cast<uint32_t>(i) * 2) << 2;
            if (e == 0) continue;
            const int32_t v = row(e + t, 1);
            if (v < -1) return false;
            s.ent_[static_cast<uint32_t>(i)] = v;
        }
        return true;
    }
};

std::shared_ptr<const CollTreeScene> CollTreeScene::parse(uint32_t tree) {
    auto s = std::make_shared<CollTreeScene>();
    s->tree_ = tree;
    CollTreeParser p{*s, {}, {}};
    if (!p.run()) return nullptr;
    s->hash_ = hash_ranges(s->src_);
    return s;
}

uint64_t CollTreeScene::hash_ee() const { return hash_ranges(src_); }

size_t CollTreeScene::source_bytes() const {
    size_t n = 0;
    for (const auto& r : src_) n += r.second;
    return n;
}

std::pair<uint32_t, uint32_t> CollTreeScene::source_extent() const { return extent(src_); }

bool CollTreeScene::same_as(const CollTreeScene& o, std::string* why) const {
    auto no = [&](const char* w) {
        if (why != nullptr) *why = w;
        return false;
    };
    if (tree_ != o.tree_) return no("tree pointer");
    if (z_.first != o.z_.first || z_.count != o.z_.count) return no("z row");
    if (rows_.size() != o.rows_.size()) return no("row count");
    for (size_t i = 0; i < rows_.size(); ++i)
        if (rows_[i].first != o.rows_[i].first || rows_[i].count != o.rows_[i].count || rows_[i].at != o.rows_[i].at) return no("a row");
    if (ent_ != o.ent_) return no("an entry");
    if (cells_.size() != o.cells_.size()) return no("cell count");
    for (size_t i = 0; i < cells_.size(); ++i)
        if (cells_[i].word != o.cells_[i].word || cells_[i].at != o.cells_[i].at) return no("a cell word");
    if (words_ != o.words_) return no("a cell's words");
    return true;
}

std::vector<CollTreeScene::WalkCell> CollTreeScene::cells_walk() const {
    std::vector<WalkCell> out;
    for (int32_t zi = 0; zi < z_.count; ++zi) {
        const int32_t yr = ent_[z_.at + static_cast<uint32_t>(zi)];
        if (yr < 0) continue;
        const Row& Y = rows_[static_cast<uint32_t>(yr)];
        for (int32_t yi = 0; yi < Y.count; ++yi) {
            const int32_t xr = ent_[Y.at + static_cast<uint32_t>(yi)];
            if (xr < 0) continue;
            const Row& X = rows_[static_cast<uint32_t>(xr)];
            for (int32_t xi = 0; xi < X.count; ++xi) {
                const int32_t c = ent_[X.at + static_cast<uint32_t>(xi)];
                if (c >= 0) out.push_back(WalkCell{X.first + xi, Y.first + yi, z_.first + zi, cells_[static_cast<uint32_t>(c)].word});
            }
        }
    }
    return out;
}

// ---- the mesh list ----

std::shared_ptr<const CollMeshScene> CollMeshScene::parse(uint32_t list) {
    if (!ram_ok(list, 4)) return nullptr;
    auto s = std::make_shared<CollMeshScene>();
    s->list_ = list;
    s->count_ = static_cast<int32_t>(rd32(list));
    if (s->count_ > kMaxChunks) return nullptr;
    const int32_t n = std::max(s->count_, 0);
    s->src_.emplace_back(list, 4);
    if (n > 0) {
        if (!ram_ok(list + 0x10, uint64_t(n) * 0x10)) return nullptr;
        s->src_.emplace_back(list + 0x10, static_cast<uint32_t>(n) * 0x10);
    }
    s->chunks_.reserve(static_cast<size_t>(n));
    for (int32_t i = 0; i < n; ++i) {
        const uint32_t e = list + 0x10 + static_cast<uint32_t>(i) * 0x10;
        Chunk c{};
        for (uint32_t k = 0; k < 4; ++k) c.s[k] = static_cast<uint16_t>(rd16(e + k * 2));
        c.ntri = ram(e + 8)[0];
        c.nv = static_cast<int32_t>(rd16(e + 0xa));
        const uint32_t vp = rd32(e + 0xc);
        const uint32_t nr = vertex_reads(c.nv);
        const uint32_t tp = vp + static_cast<uint32_t>(c.nv) * 8;
        if (s->verts_.size() + nr * 4 > kMaxWords || !ram_ok(vp, uint64_t{nr} * 8) || !ram_ok(tp, uint64_t{c.ntri} * 4)) return nullptr;
        c.vat = static_cast<uint32_t>(s->verts_.size() / 4);
        for (uint32_t k = 0; k < nr * 4; ++k) s->verts_.push_back(static_cast<uint16_t>(rd16(vp + k * 2)));
        c.tat = static_cast<uint32_t>(s->tris_.size());
        const uint8_t* t = ram(tp);
        s->tris_.insert(s->tris_.end(), t, t + c.ntri * 4);
        s->src_.emplace_back(vp, nr * 8);
        if (c.ntri != 0) s->src_.emplace_back(tp, c.ntri * 4);
        s->chunks_.push_back(c);
    }
    s->hash_ = hash_ranges(s->src_);
    return s;
}

uint64_t CollMeshScene::hash_ee() const { return hash_ranges(src_); }

size_t CollMeshScene::source_bytes() const {
    size_t n = 0;
    for (const auto& r : src_) n += r.second;
    return n;
}

std::pair<uint32_t, uint32_t> CollMeshScene::source_extent() const { return extent(src_); }

bool CollMeshScene::same_as(const CollMeshScene& o, std::string* why) const {
    auto no = [&](const char* w) {
        if (why != nullptr) *why = w;
        return false;
    };
    if (list_ != o.list_ || count_ != o.count_ || chunks_.size() != o.chunks_.size()) return no("list or count");
    for (size_t i = 0; i < chunks_.size(); ++i) {
        const Chunk &a = chunks_[i], &b = o.chunks_[i];
        if (std::memcmp(a.s, b.s, sizeof a.s) != 0 || a.ntri != b.ntri || a.nv != b.nv || a.vat != b.vat || a.tat != b.tat) return no("a chunk");
    }
    if (verts_ != o.verts_) return no("vertices");
    if (tris_ != o.tris_) return no("triangles");
    return true;
}

// ---- the switch, the kept parts ----

namespace {

int env_int(const char* name, int dflt) {
    const char* v = std::getenv(name);
    return v != nullptr && *v != 0 ? std::atoi(v) : dflt;
}
std::atomic<bool> g_native{[] {  // (native unless RAC1_COLL_SCENE=ee)
    const char* v = std::getenv("RAC1_COLL_SCENE");
    return v == nullptr || std::strcmp(v, "ee") != 0;
}()};
std::atomic<bool> g_check{env_int("RAC1_COLL_SCENE_CHECK", 0) != 0};
const bool g_trace = std::getenv("RAC1_COLL_SCENE_TRACE") != nullptr;

std::atomic<uint64_t> g_gen{1};
std::mutex g_mu;  // the kept parts and the counts
CollSceneStats g_stats;
bool g_error_printed = false;

constexpr size_t kKeep = 4;  // the parts kept per kind

template <class S>
struct Kept {
    uint32_t ptr;
    uint64_t gen;
    std::shared_ptr<const S> s;  // null: the pointer is not held (read EE)
};
std::vector<Kept<CollTreeScene>> g_trees;  // most recent last
std::vector<Kept<CollMeshScene>> g_meshes;

template <class S>
struct Current {
    bool valid = false;
    uint32_t ptr = 0;
    uint64_t gen = 0;
    std::shared_ptr<const S> s;
};
thread_local Current<CollTreeScene> t_tree;
thread_local Current<CollMeshScene> t_mesh;

template <class S>
std::shared_ptr<const S> parse_timed(uint32_t ptr, const char* what) {
    const auto t0 = std::chrono::steady_clock::now();
    std::shared_ptr<const S> s = S::parse(ptr);
    const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    ++g_stats.parses;
    g_stats.parse_ms += ms;
    if (s == nullptr) ++g_stats.unheld;
    if (g_trace) {
        const std::pair<uint32_t, uint32_t> x = s != nullptr ? s->source_extent() : std::make_pair(0u, 0u);
        std::fprintf(stderr, "[coll-scene] %s 0x%08x: %s, %zu source bytes in 0x%08x .. 0x%08x, %.3f ms\n", what, ptr,
                     s != nullptr ? "parsed" : "not held (EE read)", s != nullptr ? s->source_bytes() : size_t{0}, x.first, x.second, ms);
    }
    return s;
}

// the part for `ptr`: a kept one whose bytes' hash is unchanged, or a new parse (g_mu held; called when this thread's
// pointer or the generation changed, so the hash is taken at a pointer switch, a level's load or a section's switch only)
template <class S>
std::shared_ptr<const S> part(std::vector<Kept<S>>& kept, uint32_t ptr, uint64_t gen, const char* what) {
    for (size_t i = 0; i < kept.size(); ++i) {
        const Kept<S> k = kept[i];
        if (k.ptr != ptr) continue;
        kept.erase(kept.begin() + static_cast<std::ptrdiff_t>(i));
        if (k.s == nullptr || k.s->hash_ee() != k.s->source_hash()) break;  // (parsed again below)
        ++g_stats.reused;
        if (g_trace) std::fprintf(stderr, "[coll-scene] %s 0x%08x: the kept part taken again (its bytes' hash unchanged)\n", what, ptr);
        kept.push_back(Kept<S>{ptr, gen, k.s});  // (the most recent last)
        return k.s;
    }
    std::shared_ptr<const S> s = parse_timed<S>(ptr, what);
    if (g_trace)
        std::fprintf(stderr, "[coll-scene] (%llu parses, %llu kept parts taken again, %llu found stale, %llu parse errors so far)\n",
                     static_cast<unsigned long long>(g_stats.parses), static_cast<unsigned long long>(g_stats.reused),
                     static_cast<unsigned long long>(g_stats.stale), static_cast<unsigned long long>(g_stats.parse_errors));
    kept.push_back(Kept<S>{ptr, gen, s});
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
        s = part(kept, ptr, gen, what);
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
                std::fprintf(stderr, "[coll-scene] ERROR: the %s parsed from 0x%08x differs from EE memory right after its parse\n", what, ptr);
            }
            return cur.s;
        }
        ++g_stats.stale;
        if (g_trace) std::fprintf(stderr, "[coll-scene] %s 0x%08x: differs from EE memory (check mode): parsed again\n", what, ptr);
        kept.erase(std::remove_if(kept.begin(), kept.end(), [ptr](const Kept<S>& k) { return k.ptr == ptr; }), kept.end());
        s = part(kept, ptr, gen, what);
    }
    cur.valid = true;
    cur.ptr = ptr;
    cur.gen = gen;
    cur.s = s;
    return s;
}

}  // namespace

bool coll_scene_native() { return g_native.load(std::memory_order_relaxed); }
void coll_scene_set_native(bool on) { g_native.store(on); }
bool coll_scene_check() { return g_check.load(std::memory_order_relaxed); }
void coll_scene_set_check(bool on) { g_check.store(on); }

std::shared_ptr<const CollTreeScene> coll_scene_tree(uint32_t tree) { return current(t_tree, g_trees, tree, "tree"); }
std::shared_ptr<const CollMeshScene> coll_scene_mesh(uint32_t list) { return current(t_mesh, g_meshes, list, "mesh list"); }
std::shared_ptr<const CollTreeScene> coll_scene_tree_stale(uint32_t tree, bool after_fresh) { return stale(t_tree, g_trees, tree, after_fresh, "tree"); }
std::shared_ptr<const CollMeshScene> coll_scene_mesh_stale(uint32_t list, bool after_fresh) {
    return stale(t_mesh, g_meshes, list, after_fresh, "mesh list");
}

void coll_scene_level_changed() {
    {
        const std::lock_guard<std::mutex> lk(g_mu);
        g_trees.clear();
        g_meshes.clear();
    }
    g_gen.fetch_add(1, std::memory_order_acq_rel);
}

void coll_scene_tree_set(uint32_t old_tree, uint32_t tree) {
    if (old_tree != tree) g_gen.fetch_add(1, std::memory_order_acq_rel);
}

CollSceneStats coll_scene_stats() {
    const std::lock_guard<std::mutex> lk(g_mu);
    return g_stats;
}

}  // namespace rac1
