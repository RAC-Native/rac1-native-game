// See level_registry.h.
#include <game/level/level_registry.h>

#include <game/enhance/leveldata.h>
#include <game/enhance/sections.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <bit>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <set>
#include <utility>

namespace rac1::lvd {

int g_ld_verify = 0;

namespace {

// ---- the registries ----
std::unique_ptr<LevelRegistry> g_base;
int g_base_ovl = -1000;               // the overlay translation it was built under (ovl_reloc_level)
constexpr int KEYS = 32;              // leveldata.cpp's g_reg[32]
std::unique_ptr<LevelRegistry> g_twin[KEYS];
uint32_t g_twin_block[KEYS] = {};     // the session heap block lay_out filled
uint32_t g_twin_rails[KEYS] = {};     // its rail table pointer as laid out (another one: a section's, ld_rails_put)

// leveldata.cpp's registry block (its Registry, `R_*`): the objects' words at these offsets, as Veldin's tables are
constexpr uint32_t R_PATHS = 0x000, R_VOLUMES = 0x2c0, R_CAMERAS = 0x300, R_RAILS = 0x308;

bool g_env_read = false;
void read_env() {
    if (g_env_read) return;
    g_env_read = true;
    if (const char* e = std::getenv("RAC1_LD_VERIFY"); e != nullptr) g_ld_verify = std::max(0, std::atoi(e));
}

uint32_t le32(const std::vector<uint8_t>& b, uint32_t off) {
    if (off > b.size() || b.size() - off < 4) return 0;
    return static_cast<uint32_t>(b[off]) | static_cast<uint32_t>(b[off + 1]) << 8 | static_cast<uint32_t>(b[off + 2]) << 16 |
           static_cast<uint32_t>(b[off + 3]) << 24;
}
void put_le32(std::vector<uint8_t>& b, uint32_t off, uint32_t v) {
    if (off > b.size() || b.size() - off < 4) return;
    for (uint32_t k = 0; k < 4; ++k) b[off + k] = static_cast<uint8_t>(v >> (8 * k));
}

void fill(LevelRegistry& r, GameplayLevelData&& d) {
    r.has_paths = d.has_paths;
    r.has_volumes = d.has_volumes;
    r.has_cameras = d.has_cameras;
    r.has_rails = d.has_rails;
    for (size_t l = 0; l < 3; ++l) {
        r.has_shapes[l] = d.has_shapes[l];
        r.shapes[l] = std::move(d.shapes[l]);
    }
    r.paths = std::move(d.paths);
    r.path_data_size = d.path_data_size;
    r.volumes = std::move(d.volumes);
    r.cameras.clear();
    for (const LdCamera& c : d.cameras) r.cameras.push_back(Camera{c, 0});
    r.rails = std::move(d.rails);
    r.rail_data_size = d.rail_data_size;
    r.cam_volumes.bytes = std::move(d.cam_volumes);
}

// The zone grid at EE `g` copied, its extent walked as zone_grid_cell walks it (overlay_entry.cpp): the rows, then the masks
// up to the highest index a cell names. Bounded: a grid that does not look like one (a row outside the RAM, more than 2^20
// cells) gives none.
void copy_zone_grid(uint32_t g, ZoneGrid& out) {
    out.bytes.clear();
    const uint32_t ram = ee_game_ram_end();
    if (g == 0 || g >= ram - 16) return;
    const uint32_t nz = lhu(g + 6);
    if (nz > 1024) return;
    uint32_t end = g + 8 + 2 * nz;
    int32_t top = -1;
    uint32_t cells = 0;
    for (uint32_t z = 0; z < nz; ++z) {
        const uint32_t zr = lhu(g + 8 + 2 * z);
        if (zr == 0) continue;
        const uint32_t yr = g + zr * 4;
        if (yr >= ram - 4) return;
        const uint32_t ny = lhu(yr + 2);
        if (ny > 1024 || yr + 4 + 2 * ny >= ram) return;
        end = std::max(end, yr + 4 + 2 * ny);
        for (uint32_t y = 0; y < ny; ++y) {
            const uint32_t yo = lhu(yr + 4 + 2 * y);
            if (yo == 0) continue;
            const uint32_t xr = g + yo * 4;
            if (xr >= ram - 4) return;
            const uint32_t nx = lhu(xr + 2);
            if (nx > 1024 || xr + 4 + 2 * nx >= ram || (cells += nx) > (1u << 20)) return;
            end = std::max(end, xr + 4 + 2 * nx);
            for (uint32_t x = 0; x < nx; ++x) {
                const uint32_t e = lhu(xr + 4 + 2 * x);
                if (e != 0xffff) top = std::max(top, static_cast<int32_t>(e));
            }
        }
    }
    if (top >= 0) {
        const uint32_t masks = g + lwu(g);
        if (masks >= ram || static_cast<uint64_t>(masks) + (static_cast<uint64_t>(top) + 1) * 0x80 > ram) return;
        end = std::max(end, masks + (static_cast<uint32_t>(top) + 1) * 0x80);
    }
    // (EE memory is a byte image, as the file it came from: the bytes copied as they are)
    const uint8_t* src = ee_get_ram_base() + g;
    out.bytes.assign(src, src + (end - g));
}

// ---- the comparison ----
// The EE side of a registry: the addresses of the words that hold each object's pointer and count (Veldin's tables through
// OV() for the engine's, a leveldata.cpp block's for a registry of the open engine)
struct View {
    uint32_t path_table = 0, path_count = 0, path_data = 0;
    uint32_t vol = 0, vol_n = 0;
    uint32_t shape[3] = {}, shape_n[3] = {};
    uint32_t cam = 0, cam_n = 0;
    uint32_t rail = 0, rail_n = 0;
    uint32_t cam_volumes = 0;  // the grid pointer's word (0: not in this view)
    uint32_t zones = 0;        // the zone grid pointer's word (0: not in this view)
    enum class Rails { Exact, Prefix, Skip } rails = Rails::Exact;
};

View engine_view() {
    View v;
    v.path_table = OV(0x001b04b0);
    v.path_count = OV(0x00160104);
    v.path_data = OV(0x00160108);
    v.vol = OV(0x001600e4);
    v.vol_n = OV(0x001600e8);
    v.shape[0] = OV(0x001600ec);
    v.shape_n[0] = OV(0x001600f0);
    v.shape[1] = OV(0x001600f4);
    v.shape_n[1] = OV(0x001600f8);
    v.shape[2] = OV(0x001600fc);
    v.shape_n[2] = OV(0x00160100);
    v.cam = OV(0x0015ef50);
    v.cam_n = OV(0x0015ef54);
    v.rail = OV(0x0015f70c);
    v.rail_n = OV(0x0015f710);
    v.cam_volumes = OV(0x0015ef80);
    v.zones = OV(0x0015f600);
    return v;
}

View block_view(uint32_t b) {
    View v;
    v.path_table = b + R_PATHS;
    v.vol = b + R_VOLUMES;
    v.vol_n = b + R_VOLUMES + 4;
    for (uint32_t l = 0; l < 3; ++l) {
        v.shape[l] = b + R_VOLUMES + 8 + 8 * l;
        v.shape_n[l] = b + R_VOLUMES + 12 + 8 * l;
    }
    v.path_count = b + R_VOLUMES + 0x20;
    v.path_data = b + R_VOLUMES + 0x24;
    v.cam = b + R_CAMERAS;
    v.cam_n = b + R_CAMERAS + 4;
    v.rail = b + R_RAILS;
    v.rail_n = b + R_RAILS + 4;
    return v;
}

// A difference's handler: true when it is a run-time write the native side follows (the slot then takes the EE value)
using Sink = bool (*)(void* ctx, const RegistryDiff& d);

class Comparer {
public:
    Comparer(Sink sink, void* ctx, size_t max) : sink_(sink), ctx_(ctx), max_(max) {}
    size_t found() const { return found_; }
    bool full() const { return found_ >= max_; }

    // one word: the native slot (a float, a signed or unsigned word) against the EE word at `a`
    template <class T>
    void word(const char* obj, int32_t idx, uint32_t off, T& slot, uint32_t a) {
        const uint32_t ee = lwu(a);
        const uint32_t nat = std::bit_cast<uint32_t>(slot);
        if (nat == ee) return;
        if (diff(obj, idx, off, nat, ee)) slot = std::bit_cast<T>(ee);
    }
    void value(const char* obj, int32_t idx, uint32_t off, uint32_t nat, uint32_t ee) {
        if (nat != ee) diff(obj, idx, off, nat, ee);
    }
    // a little-endian byte image (a grid) against EE from `a`
    void bytes(const char* obj, std::vector<uint8_t>& b, uint32_t a) {
        const uint32_t n = static_cast<uint32_t>(b.size());
        for (uint32_t off = 0; off + 4 <= n && !full(); off += 4) {
            const uint32_t ee = lwu(a + off), nat = le32(b, off);
            if (nat != ee && diff(obj, -1, off, nat, ee)) put_le32(b, off, ee);
        }
        for (uint32_t off = n & ~3u; off < n && !full(); ++off) {
            const uint32_t ee = lbu(a + off);
            if (b[off] != ee && diff(obj, -1, off, b[off], ee)) b[off] = static_cast<uint8_t>(ee);
        }
    }
    void path(const char* obj, int32_t idx, PathSpline& p, uint32_t a) {
        word(obj, idx, 0, p.count, a);
        for (uint32_t k = 0; k < 3; ++k) word(obj, idx, 4 + 4 * k, p.head[k], a + 4 + 4 * k);
        for (size_t k = 0; k < p.points.size() && !full(); ++k) {
            const uint32_t q = a + 0x10 + 0x10 * static_cast<uint32_t>(k), o = 0x10 + 0x10 * static_cast<uint32_t>(k);
            word(obj, idx, o, p.points[k].x, q);
            word(obj, idx, o + 4, p.points[k].y, q + 4);
            word(obj, idx, o + 8, p.points[k].z, q + 8);
            word(obj, idx, o + 0xc, p.points[k].w, q + 0xc);
        }
    }
    void shape(const char* obj, int32_t idx, Shape& s, uint32_t a) {
        for (uint32_t k = 0; k < 16; ++k) word(obj, idx, 4 * k, s.matrix[k], a + 4 * k);
        for (uint32_t k = 0; k < 16; ++k) word(obj, idx, 0x40 + 4 * k, s.to_unit[k], a + 0x40 + 4 * k);
    }

private:
    bool diff(const char* obj, int32_t idx, uint32_t off, uint32_t nat, uint32_t ee) {
        RegistryDiff d{obj, idx, off, nat, ee};
        if (sink_ != nullptr && sink_(ctx_, d)) return true;
        ++found_;
        return false;
    }
    Sink sink_;
    void* ctx_;
    size_t max_;
    size_t found_ = 0;
};

// The registry `r` against the EE tables of `v`
void compare_view(LevelRegistry& r, const View& v, Comparer& c) {
    if (r.has_paths) {
        const uint32_t n = static_cast<uint32_t>(r.paths.size());
        c.value("path count", -1, 0, n, lwu(v.path_count));
        const uint32_t data = lwu(v.path_data);
        for (uint32_t i = 0; i < n && i < LD_PATH_MAX && !c.full(); ++i) {
            const uint32_t a = lwu(v.path_table + 4 * i);
            c.value("path table", static_cast<int32_t>(i), 0, r.paths[i].offset, a - data);
            if (a != 0) c.path("path", static_cast<int32_t>(i), r.paths[i], a);
        }
    }
    if (r.has_volumes) {
        c.value("volume count", -1, 0, static_cast<uint32_t>(r.volumes.size()), lwu(v.vol_n));
        const uint32_t base = lwu(v.vol);
        for (size_t i = 0; i < r.volumes.size() && !c.full(); ++i) {
            const uint32_t a = base + static_cast<uint32_t>(i) * 0x90;
            c.shape("volume", static_cast<int32_t>(i), r.volumes[i].shape, a);
            for (uint32_t k = 0; k < 4; ++k) c.word("volume", static_cast<int32_t>(i), 0x80 + 4 * k, r.volumes[i].tail[k], a + 0x80 + 4 * k);
        }
    }
    static const char* const kShape[3] = {"box", "sphere", "cylinder"};
    static const char* const kShapeN[3] = {"box count", "sphere count", "cylinder count"};
    for (size_t l = 0; l < 3; ++l) {
        if (!r.has_shapes[l]) continue;
        c.value(kShapeN[l], -1, 0, static_cast<uint32_t>(r.shapes[l].size()), lwu(v.shape_n[l]));
        const uint32_t base = lwu(v.shape[l]);
        for (size_t i = 0; i < r.shapes[l].size() && !c.full(); ++i) c.shape(kShape[l], static_cast<int32_t>(i), r.shapes[l][i], base + static_cast<uint32_t>(i) * 0x80);
    }
    if (r.has_cameras) {
        c.value("camera count", -1, 0, static_cast<uint32_t>(r.cameras.size()), lwu(v.cam_n));
        const uint32_t base = lwu(v.cam);
        for (size_t i = 0; i < r.cameras.size() && !c.full(); ++i) {
            const uint32_t a = base + static_cast<uint32_t>(i) * 0x20;
            const int32_t idx = static_cast<int32_t>(i);
            LdCamera& p = r.cameras[i].placement;
            for (uint32_t k = 0; k < 3; ++k) c.word("camera", idx, 4 * k, p.position[k], a + 4 * k);
            c.word("camera", idx, 0xc, p.type, a + 0xc);
            for (uint32_t k = 0; k < 3; ++k) c.word("camera", idx, 0x10 + 4 * k, p.rotation[k], a + 0x10 + 4 * k);
            c.word("camera", idx, 0x1c, r.cameras[i].pvars, a + 0x1c);
        }
    }
    if (r.has_rails && v.rails != View::Rails::Skip) {
        const uint32_t n = static_cast<uint32_t>(r.rails.size()), ee_n = lwu(v.rail_n);
        if (v.rails == View::Rails::Exact || ee_n < n) c.value("rail count", -1, 0, n, ee_n);
        const uint32_t base = lwu(v.rail);
        for (uint32_t i = 0; i < n && i < ee_n && !c.full(); ++i) {
            const uint32_t a = base + i * 0x20;
            const int32_t idx = static_cast<int32_t>(i);
            Rail& rl = r.rails[i];
            for (uint32_t k = 0; k < 3; ++k) c.word("rail", idx, 4 * k, rl.centre[k], a + 4 * k);
            c.word("rail", idx, 0xc, rl.radius, a + 0xc);
            c.word("rail", idx, 0x14, rl.loop, a + 0x14);
            if (const uint32_t s = lwu(a + 0x10); s != 0) c.path("rail spine", idx, rl.spine, s);
        }
    }
    if (v.cam_volumes != 0 && !r.cam_volumes.empty() && !c.full()) {
        if (const uint32_t g = lwu(v.cam_volumes); g != 0) c.bytes("camera-volume grid", r.cam_volumes.bytes, g);
    }
    if (v.zones != 0 && !r.zones.empty() && !c.full()) {
        if (const uint32_t g = lwu(v.zones); g != 0) c.bytes("zone grid", r.zones.bytes, g);
    }
}

// ---- the run-time writers the verify follows ----
// Each entry: a field of the tables' contents the game (or the open engine) rewrites at run time, found by G0's write census
// (orch/w5/g0/census) and by this verify on the nine tours; the native value takes the game's. Task L3a: these writes must
// reach the native registry (or the field stays EE state). The writers named are the census's.
struct Followed {
    const char* name;
    bool (*match)(const RegistryDiff& d);
};
bool is_path(const RegistryDiff& d) { return d.object == "path" || d.object == "rail spine"; }
// a point's lane (0 x, 4 y, 8 z, 0xc w); -1 the count word and the 12 bytes after it
int32_t point_lane(const RegistryDiff& d) { return d.offset < 0x10 ? -1 : static_cast<int32_t>((d.offset - 0x10) & 0xc); }
const Followed kFollowed[] = {
    // smoke_vent_update (0x2d6f28, smoke_vent.cpp), at a vent's first update: its box's +0x3c (the centre row's w lane) set to 1
    {"a smoke vent's box +0x3c set to 1 (smoke_vent_update)", [](const RegistryDiff& d) { return d.object == "box" && d.offset == 0x3c && d.ee == 0x3f800000u; }},
    // a shape's centre (+0x30 .. +0x38) moved by the class that owns it (a moving volume: Kerwan box 57, Pokitaru boxes 27 / 28;
    // the census: pok_2d14b0_l11, olt_3017f8_l14)
    {"a shape's centre moved (its class)", [](const RegistryDiff& d) {
         return (d.object == "box" || d.object == "sphere" || d.object == "cylinder" || d.object == "volume") && d.offset >= 0x30 && d.offset < 0x3c;
     }},
    // a point's w lane: the file's -1 replaced by the segment's length to the next point when a mover first measures the path
    // (lv3_264458_l3, path_measure_l1, path_follower_init_l1, hov_28eb00_l12 ... 50 functions in the census), or by a class's
    // own per-point value (Rilgar's and Pokitaru's small integers)
    {"a path point's w lane (its cached segment length or a class's value)", [](const RegistryDiff& d) { return is_path(d) && point_lane(d) == 0xc; }},
    // a point's z lane rounded by those measurers' z + 0.5 - 0.5 (lv3_264458_l3: one ulp)
    {"a path point's z lane rounded (z + 0.5 - 0.5)", [](const RegistryDiff& d) {
         return is_path(d) && point_lane(d) == 8 && (d.ee - d.native + 1u) <= 2u;
     }},
    // a path's count word lowered (a mover dropping a looped path's closing point: Novalis, Kerwan, Gemlik Base)
    {"a path's count word lowered", [](const RegistryDiff& d) {
         return is_path(d) && d.offset == 0 && static_cast<int32_t>(d.ee) >= 0 && static_cast<int32_t>(d.ee) < static_cast<int32_t>(d.native);
     }},
    // a path's points rewritten by a class (Kerwan paths 17 .. 28, Batalia 96, Pokitaru 14 / 29: bat_class679_update_l8, pok_2873b0_l11 ...)
    {"a path's points moved (its class)", [](const RegistryDiff& d) { return is_path(d) && point_lane(d) >= 0 && point_lane(d) < 0xc; }},
    // a rail's reach: -1 while a level mod's rail is off (setpieces.cpp rail_apply), 0 on Veldin 2 (vf_class582_update_l18), its
    // own value again after
    {"a rail's reach (+0xc) switched (setpieces rail_apply, vf_class582_update_l18)", [](const RegistryDiff& d) { return d.object == "rail" && d.offset == 0xc; }},
};

// ---- the verify's log ----
struct Log {
    std::set<std::string> said;        // (object, index) whose first difference was logged
    std::map<std::string, uint64_t> followed;  // a followed write's name (and object index) -> times
    uint64_t passes = 0, diffs = 0, follows = 0;
    bool as_engine = false;  // (a world's: compared as the engine's tables once)
};
Log g_log[KEYS + 1];  // [KEYS] the base's
uint64_t g_frame = 0;
bool g_at_build = false;  // (the comparison at a registry's build, not a frame's)

std::string when() {
    return g_at_build ? std::string("at its build") : "frame " + std::to_string(g_frame);
}

const char* key_name(int key, char* buf, size_t n) {
    if (key == KEY_BASE) return "base";
    std::snprintf(buf, n, "open-engine registry %d", key);
    return buf;
}

struct SinkCtx {
    int key;
    int level;
    bool follow;
    Log* log;
};

bool verify_sink(void* p, const RegistryDiff& d) {
    SinkCtx& s = *static_cast<SinkCtx*>(p);
    char kb[32];
    if (s.follow)
        for (const Followed& f : kFollowed)
            if (f.match(d)) {
                ++s.log->follows;
                const std::string what = std::string(f.name) + " (" + d.object + " " + std::to_string(d.index) + ")";
                if (s.log->followed[what]++ == 0)
                    std::printf("[ld-verify] level %d %s: a run-time write followed: %s +0x%x %08x -> %08x (%s)\n", s.level, key_name(s.key, kb, sizeof kb), what.c_str(),
                                d.offset, d.native, d.ee, when().c_str());
                return true;
            }
    ++s.log->diffs;
    const std::string id = d.object + " " + std::to_string(d.index);
    if (s.log->said.insert(id).second)
        std::printf("[ld-verify] DIFFERENCE level %d %s: %s (%s)\n", s.level, key_name(s.key, kb, sizeof kb), d.text().c_str(), when().c_str());
    return false;
}

void verify(LevelRegistry& r, const View& v, int key, bool follow) {
    Log& log = g_log[key == KEY_BASE ? KEYS : key];
    g_at_build = !follow;  // (at a build nothing is followed: no frame has run on the tables yet)
    SinkCtx ctx{key, r.level, follow, &log};
    Comparer c(verify_sink, &ctx, 1u << 30);
    compare_view(r, v, c);
    ++log.passes;
}

void summary(int key, const LevelRegistry& r) {
    Log& log = g_log[key == KEY_BASE ? KEYS : key];
    if (log.passes != 0) {
        char kb[32];
        std::printf("[ld-verify] level %d %s: %llu comparisons, %llu differing words, %llu run-time writes followed (%zu objects)\n", r.level, key_name(key, kb, sizeof kb),
                    static_cast<unsigned long long>(log.passes), static_cast<unsigned long long>(log.diffs), static_cast<unsigned long long>(log.follows), log.followed.size());
    }
    log = Log{};
}

void report_build(const LevelRegistry& r, int key, double us, const std::string& error) {
    if (g_ld_verify <= 0 && std::getenv("RAC1_LOAD_PROF") == nullptr) return;
    char kb[32];
    std::printf("[ld] level %d's native registry (%s): %zu paths, %zu volumes, %zu / %zu / %zu shapes, %zu cameras, %zu rails, grids %zu + %zu bytes; %zu KB of host "
                "memory, built in %.0f us%s%s\n",
                r.level, key_name(key, kb, sizeof kb), r.paths.size(), r.volumes.size(), r.shapes[0].size(), r.shapes[1].size(), r.shapes[2].size(), r.cameras.size(),
                r.rails.size(), r.cam_volumes.bytes.size(), r.zones.bytes.size(), r.host_bytes() >> 10, us, error.empty() ? "" : "; ", error.c_str());
}

// the base's tables are the engine's now: its level's overlay, its level
bool base_current() { return g_base != nullptr && ovl_reloc_level() == g_base_ovl && lw(0x0015ed84) == g_base->level; }

void drop_gone_twins() {
    for (int k = 0; k < KEYS; ++k)
        if (g_twin[k] != nullptr && !ld_registry_has(k)) {
            summary(k, *g_twin[k]);
            g_twin[k].reset();
            g_twin_block[k] = 0;
        }
}

}  // namespace

// ---- the grids' lookups ----
// orig: (host) see level_registry.h
uint32_t CamVolumeGrid::word(uint32_t off) const { return le32(bytes, off); }
// orig: (host) see level_registry.h
float CamVolumeGrid::f32(uint32_t off) const { return std::bit_cast<float>(word(off)); }
// orig: (host) see level_registry.h
uint32_t ZoneGrid::half(uint32_t off) const {
    if (off > bytes.size() || bytes.size() - off < 2) return 0;
    return static_cast<uint32_t>(bytes[off]) | static_cast<uint32_t>(bytes[off + 1]) << 8;
}
// orig: (host) see level_registry.h
uint32_t ZoneGrid::word(uint32_t off) const { return le32(bytes, off); }
// orig: (host) see level_registry.h (zone_grid_cell 0x1f7278's walk)
uint32_t ZoneGrid::cell_mask(int32_t x, int32_t y, int32_t z) const {
    if (bytes.empty()) return 0;
    z -= static_cast<int32_t>(half(4));
    if (z < 0 || !(z < static_cast<int32_t>(half(6)))) return 0;
    const uint32_t zr = half(8 + static_cast<uint32_t>(z) * 2);
    if (zr == 0) return 0;
    const uint32_t yr = zr * 4;
    y -= static_cast<int32_t>(half(yr));
    if (y < 0 || !(y < static_cast<int32_t>(half(yr + 2)))) return 0;
    const uint32_t yo = half(yr + 4 + static_cast<uint32_t>(y) * 2);
    if (yo == 0) return 0;
    const uint32_t xr = yo * 4;
    x -= static_cast<int32_t>(half(xr));
    if (x < 0 || !(x < static_cast<int32_t>(half(xr + 2)))) return 0;
    const uint32_t e = half(xr + 4 + static_cast<uint32_t>(x) * 2);
    return e == 0xffff ? 0u : word(0) + e * 0x80;
}

// orig: (host) see level_registry.h
size_t LevelRegistry::host_bytes() const {
    size_t n = sizeof *this;
    const auto path = [](const PathSpline& p) { return p.points.capacity() * sizeof(Vec4f); };
    n += paths.capacity() * sizeof(PathSpline);
    for (const PathSpline& p : paths) n += path(p);
    n += volumes.capacity() * sizeof(Volume);
    for (const auto& l : shapes) n += l.capacity() * sizeof(Shape);
    n += cameras.capacity() * sizeof(Camera);
    n += rails.capacity() * sizeof(Rail);
    for (const Rail& r : rails) n += path(r.spine);
    n += cam_volumes.bytes.capacity() + zones.bytes.capacity();
    return n;
}

// orig: (host) see level_registry.h
std::string RegistryDiff::text() const {
    char b[160];
    std::snprintf(b, sizeof b, "%s %d +0x%x: native %08x, EE %08x", object.c_str(), index, offset, native, ee);
    return b;
}

// orig: (host) see level_registry.h
const LevelRegistry* registry_base() { return g_base.get(); }

// orig: (host) see level_registry.h
const LevelRegistry* registry_of(int key) { return key >= 0 && key < KEYS ? g_twin[key].get() : nullptr; }

// orig: (host) see level_registry.h
bool registry_parse(std::span<const uint8_t> file, int level, LevelRegistry& out, std::string* error) {
    out = LevelRegistry{};
    out.level = level;
    GameplayLevelData d;
    if (!parse_gameplay_level_data(file, d, error)) return false;
    fill(out, std::move(d));
    return true;
}

// orig: (host) see level_registry.h
void registry_level_loaded(uint32_t gameplay) {
    read_env();
    const auto t0 = std::chrono::steady_clock::now();
    if (g_base != nullptr) summary(KEY_BASE, *g_base);
    drop_gone_twins();
    g_base.reset();
    const uint32_t ram = ee_ram_size();
    if (gameplay == 0 || gameplay >= ram) return;
    // (the file the load has just unpacked: the bytes it lays out, read in place; a byte image, as on the disc)
    const std::span<const uint8_t> file(ee_get_ram_base() + gameplay, ram - gameplay);
    auto r = std::make_unique<LevelRegistry>();
    std::string error;
    if (!registry_parse(file, lw(0x0015ed84), *r, &error)) {
        std::fprintf(stderr, "[ld] level %d: its gameplay file at %08x cannot be read: %s\n", lw(0x0015ed84), gameplay, error.c_str());
        return;
    }
    r->key = KEY_BASE;
    // (from the laid-out tables: the cameras' pvar blocks, the zone grid)
    if (r->has_cameras && lwu(OV(0x0015ef54)) == r->cameras.size())
        for (size_t k = 0; k < r->cameras.size(); ++k) r->cameras[k].pvars = lwu(lwu(OV(0x0015ef50)) + static_cast<uint32_t>(k) * 0x20 + 0x1c);
    copy_zone_grid(lwu(OV(0x0015f600)), r->zones);
    g_base = std::move(r);
    g_base_ovl = ovl_reloc_level();
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    report_build(*g_base, KEY_BASE, us, error);
    if (g_ld_verify > 0) {
        View v = engine_view();
        verify(*g_base, v, KEY_BASE, false);
    }
}

// orig: (host) see level_registry.h
void registry_context_made(int key, int level, const std::vector<uint8_t>& gameplay, uint32_t block) {
    read_env();
    if (key < 0 || key >= KEYS) return;
    const auto t0 = std::chrono::steady_clock::now();
    if (g_twin[key] != nullptr) summary(key, *g_twin[key]);
    g_twin[key].reset();
    auto r = std::make_unique<LevelRegistry>();
    std::string error;
    if (gameplay.empty() || !registry_parse(gameplay, level, *r, &error)) return;  // (no level data: leveldata.cpp's registry is empty too)
    r->key = key;
    // (lay_out caps the paths at the table's 176)
    if (r->paths.size() > LD_PATH_MAX) r->paths.resize(LD_PATH_MAX);
    // (from the laid-out block: the cameras' pvar blocks)
    const uint32_t cams = lwu(block + R_CAMERAS);
    if (r->has_cameras && cams != 0 && lwu(block + R_CAMERAS + 4) == r->cameras.size())
        for (size_t k = 0; k < r->cameras.size(); ++k) r->cameras[k].pvars = lwu(cams + static_cast<uint32_t>(k) * 0x20 + 0x1c);
    g_twin[key] = std::move(r);
    g_twin_block[key] = block;
    g_twin_rails[key] = lwu(block + R_RAILS);
    const double us = std::chrono::duration<double, std::micro>(std::chrono::steady_clock::now() - t0).count();
    report_build(*g_twin[key], key, us, error);
    if (g_ld_verify > 0) verify(*g_twin[key], block_view(block), key, false);
}

// orig: (host) see level_registry.h
void registry_verify_frame_slow(uint64_t frame) {
    if (frame % static_cast<uint64_t>(g_ld_verify) != 0) return;
    g_frame = frame;
    drop_gone_twins();
    // a world section is current: its registry's objects are the engine's (ld_engine_take), the base's put aside
    int taken = -1;
    if (g_ld_world) {
        const int w = sections_world();
        taken = w >= 0 ? ld_world_key(w) : -2;
    }
    View ev = engine_view();
    const int cur = sections_current();
    if (taken == -1) {
        if (base_current()) {
            // (a section of the mod's own: its rail table the engine's, not the level's)
            if (cur > 0) ev.rails = View::Rails::Skip;
            verify(*g_base, ev, KEY_BASE, true);
        }
    } else if (taken >= 0 && taken < KEYS && g_twin[taken] != nullptr) {
        ev.zones = 0;  // (the world's zone grid is its core data's: not in a registry made from its gameplay file)
        ev.rails = View::Rails::Prefix;  // (the section's table: its level's rails first, then the mod's own)
        if (g_log[taken].passes == 0 || !g_log[taken].as_engine) {
            char kb[32];
            std::printf("[ld-verify] level %d %s: compared as the engine's tables now (its world section is current, frame %llu)\n", g_twin[taken]->level,
                        key_name(taken, kb, sizeof kb), static_cast<unsigned long long>(frame));
            g_log[taken].as_engine = true;
        }
        verify(*g_twin[taken], ev, taken, true);
    }
    for (int k = 0; k < KEYS; ++k) {
        if (g_twin[k] == nullptr || k == taken) continue;
        View bv = block_view(g_twin_block[k]);
        if (lwu(bv.rail) != g_twin_rails[k]) bv.rails = View::Rails::Prefix;  // (ld_rails_put: a world section's table)
        verify(*g_twin[k], bv, k, true);
    }
}

// orig: (host) see level_registry.h
int registry_compare_base(std::vector<RegistryDiff>& out, size_t max, bool follow) {
    out.clear();
    if (g_base == nullptr) return -1;
    struct Ctx {
        std::vector<RegistryDiff>* out;
        size_t max;
        bool follow;
    } ctx{&out, max, follow};
    const Sink collect = [](void* p, const RegistryDiff& d) {
        Ctx& c = *static_cast<Ctx*>(p);
        if (c.follow)
            for (const Followed& f : kFollowed)
                if (f.match(d)) return true;
        if (c.out->size() < c.max) c.out->push_back(d);
        return false;
    };
    // (on a copy: the comparison writes nothing, the registry stays as built)
    LevelRegistry copy = *g_base;
    Comparer c(collect, &ctx, 1u << 30);
    compare_view(copy, engine_view(), c);
    return static_cast<int>(c.found());
}

// orig: (host) see level_registry.h
int registry_compare(const LevelRegistry& a, const LevelRegistry& b, std::vector<RegistryDiff>& out, size_t max) {
    out.clear();
    int n = 0;
    auto add = [&](const char* obj, int32_t idx, uint32_t off, uint32_t x, uint32_t y) {
        if (x == y) return;
        ++n;
        if (out.size() < max) out.push_back(RegistryDiff{obj, idx, off, x, y});
    };
    auto f = [](float v) { return std::bit_cast<uint32_t>(v); };
    auto path = [&](const char* obj, int32_t idx, const PathSpline& p, const PathSpline& q) {
        add(obj, idx, 0, static_cast<uint32_t>(p.count), static_cast<uint32_t>(q.count));
        for (uint32_t k = 0; k < 3; ++k) add(obj, idx, 4 + 4 * k, p.head[k], q.head[k]);
        add(obj, idx, 0x10, static_cast<uint32_t>(p.points.size()), static_cast<uint32_t>(q.points.size()));
        for (size_t k = 0; k < std::min(p.points.size(), q.points.size()); ++k) {
            const uint32_t o = 0x10 + 0x10 * static_cast<uint32_t>(k);
            add(obj, idx, o, f(p.points[k].x), f(q.points[k].x));
            add(obj, idx, o + 4, f(p.points[k].y), f(q.points[k].y));
            add(obj, idx, o + 8, f(p.points[k].z), f(q.points[k].z));
            add(obj, idx, o + 0xc, f(p.points[k].w), f(q.points[k].w));
        }
    };
    auto shape = [&](const char* obj, int32_t idx, const Shape& p, const Shape& q) {
        for (uint32_t k = 0; k < 16; ++k) add(obj, idx, 4 * k, f(p.matrix[k]), f(q.matrix[k]));
        for (uint32_t k = 0; k < 16; ++k) add(obj, idx, 0x40 + 4 * k, f(p.to_unit[k]), f(q.to_unit[k]));
    };
    add("paths present", -1, 0, a.has_paths, b.has_paths);
    add("path count", -1, 0, static_cast<uint32_t>(a.paths.size()), static_cast<uint32_t>(b.paths.size()));
    add("path data size", -1, 0, a.path_data_size, b.path_data_size);
    for (size_t i = 0; i < std::min(a.paths.size(), b.paths.size()); ++i) {
        add("path table", static_cast<int32_t>(i), 0, a.paths[i].offset, b.paths[i].offset);
        path("path", static_cast<int32_t>(i), a.paths[i], b.paths[i]);
    }
    add("volume count", -1, 0, static_cast<uint32_t>(a.volumes.size()), static_cast<uint32_t>(b.volumes.size()));
    for (size_t i = 0; i < std::min(a.volumes.size(), b.volumes.size()); ++i) {
        shape("volume", static_cast<int32_t>(i), a.volumes[i].shape, b.volumes[i].shape);
        for (uint32_t k = 0; k < 4; ++k) add("volume", static_cast<int32_t>(i), 0x80 + 4 * k, f(a.volumes[i].tail[k]), f(b.volumes[i].tail[k]));
    }
    for (size_t l = 0; l < 3; ++l) {
        add("shape count", static_cast<int32_t>(l), 0, static_cast<uint32_t>(a.shapes[l].size()), static_cast<uint32_t>(b.shapes[l].size()));
        for (size_t i = 0; i < std::min(a.shapes[l].size(), b.shapes[l].size()); ++i) shape("shape", static_cast<int32_t>(l * 0x10000 + i), a.shapes[l][i], b.shapes[l][i]);
    }
    add("camera count", -1, 0, static_cast<uint32_t>(a.cameras.size()), static_cast<uint32_t>(b.cameras.size()));
    for (size_t i = 0; i < std::min(a.cameras.size(), b.cameras.size()); ++i) {
        const LdCamera &p = a.cameras[i].placement, &q = b.cameras[i].placement;
        const int32_t idx = static_cast<int32_t>(i);
        add("camera", idx, 0, static_cast<uint32_t>(p.type), static_cast<uint32_t>(q.type));
        for (uint32_t k = 0; k < 3; ++k) add("camera", idx, 4 + 4 * k, f(p.position[k]), f(q.position[k]));
        for (uint32_t k = 0; k < 3; ++k) add("camera", idx, 0x10 + 4 * k, f(p.rotation[k]), f(q.rotation[k]));
        add("camera", idx, 0x1c, static_cast<uint32_t>(p.pvar_index), static_cast<uint32_t>(q.pvar_index));
    }
    add("rail count", -1, 0, static_cast<uint32_t>(a.rails.size()), static_cast<uint32_t>(b.rails.size()));
    for (size_t i = 0; i < std::min(a.rails.size(), b.rails.size()); ++i) {
        const Rail &p = a.rails[i], &q = b.rails[i];
        const int32_t idx = static_cast<int32_t>(i);
        for (uint32_t k = 0; k < 3; ++k) add("rail", idx, 4 * k, f(p.centre[k]), f(q.centre[k]));
        add("rail", idx, 0xc, f(p.radius), f(q.radius));
        add("rail", idx, 0x14, static_cast<uint32_t>(p.loop), static_cast<uint32_t>(q.loop));
        add("rail", idx, 0x10, p.spine.offset, q.spine.offset);
        path("rail spine", idx, p.spine, q.spine);
    }
    add("camera-volume grid size", -1, 0, static_cast<uint32_t>(a.cam_volumes.bytes.size()), static_cast<uint32_t>(b.cam_volumes.bytes.size()));
    for (uint32_t o = 0; o + 4 <= std::min(a.cam_volumes.bytes.size(), b.cam_volumes.bytes.size()); o += 4)
        add("camera-volume grid", -1, o, a.cam_volumes.word(o), b.cam_volumes.word(o));
    return n;
}

}  // namespace rac1::lvd
