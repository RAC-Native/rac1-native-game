// See section_world.h.
#include <game/enhance/section_world.h>

#include <formats/scene_wad.h>
#include <game/enhance/session.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/world_lighting.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <set>
#include <span>

namespace rac1 {

namespace {

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

bool read_all(const std::string& path, std::vector<uint8_t>& out) {
    std::ifstream f(path, std::ios::binary);
    if (!f) return false;
    out.assign(std::istreambuf_iterator<char>(f), std::istreambuf_iterator<char>());
    return true;
}

uint32_t rd32(const std::vector<uint8_t>& v, size_t o) {
    uint32_t x = 0;
    if (o + 4 <= v.size()) std::memcpy(&x, v.data() + o, 4);
    return x;
}

// The engine's world state (Veldin's addresses; OV() on the base). The words: the tfrag entries (0x160f4c, their count, the
// LOD distances and their integers), the textures' counts, the zone grid and its mode, the tie classes' count and instances
// (bounds, their end, data, count), the shrubs' (count, bounds, their end, data, colours, and 0x160b74)
constexpr uint32_t kWords[] = {0x00160f4cu, 0x00160f50u, 0x00160f60u, 0x00160f64u, 0x00160f68u, 0x00160f70u, 0x00160f74u, 0x00160f78u,
                               0x00160f54u, 0x00160fd4u, 0x001604acu, 0x0015f600u, 0x0016c074u, 0x00160fbcu, 0x00160fc0u, 0x00160fc4u,
                               0x00160fc8u, 0x00160fccu, 0x0016048cu, 0x00160490u, 0x00160494u, 0x00160498u, 0x0016049cu, 0x001604a0u,
                               0x00160b74u};
// the tables (from, to): the LOD bands; the tfrag textures' table; the tie classes' headers, ids, id map and textures' table, their
// radii, their palettes; the shrubs' headers, ids, id map and textures' table, their radii, their palettes (the per-frame
// arrays between them are not the level's: left)
constexpr uint32_t kTables[][2] = {{0x001c28f0u, 0x001c2930u}, {0x001c4a00u, 0x001c4d00u}, {0x001c5800u, 0x001c6b00u}, {0x001c6f00u, 0x001c7100u},
                                   {0x001c7700u, 0x001c7f00u}, {0x001bc030u, 0x001bc9b0u}, {0x001bcdb0u, 0x001bceb0u}, {0x001bd3b0u, 0x001bd7b0u}};
// the lights: the light grid (its points, its records), the directional lights and what the load clears after them
constexpr uint32_t kLights[][2] = {{0x0017ecc0u, 0x001803c0u}, {0x001804c0u, 0x00180640u}};
constexpr uint32_t kGridCount = 0x0015fbc4u;

std::atomic<int> g_renderer_level{-1};

// orig: (helper) a Veldin overlay address of the tables above on the loaded base (OV() for a value known at run time)
uint32_t ov(uint32_t a) { return ovl_xlate_data(a); }

// orig: (helper) level_setup.cpp's texture table: each entry's GS address (the texels' base + +0) with log2(+4) in the top bits
void tex_table(uint32_t dst, uint32_t src, int32_t n, uint32_t texels) {
    for (int32_t i = 0; i < n; ++i, src += 0x10, dst += 4) {
        const int32_t b = highest_bit_index(lhu(src + 4));
        sw(dst, texels + lwu(src) + (static_cast<uint32_t>(b) << 28));
    }
}

// orig: (helper) level_load.cpp's: the instance matrix's four quadwords
void copy_mat(uint32_t d, uint32_t s) {
    for (uint32_t o = 0; o < 0x40; o += 0x10) lq_sq(d + o, s + o);
}
// orig: (helper) level_load.cpp's mask_bit
uint32_t mask_bit(int32_t v) { return static_cast<uint32_t>((v >> 3) << 8) | (1u << (static_cast<uint32_t>(v) & 7)); }
// orig: (helper) level_load.cpp's find_id: the index of `id` among the n (8-byte, id at +4) records at `p`, n if absent
int32_t find_id(uint32_t p, int32_t n, int32_t id) {
    for (int32_t k = 0; k < n; ++k)
        if (lw(p + static_cast<uint32_t>(k) * 8 + 4) == id) return k;
    return n;
}

// The section's state the engine's for a step, the engine's own put back after it
struct Swap {
    WorldBuild& w;
    WorldState e;
    WorldLights el;
    bool lights;
    Swap(WorldBuild& b, bool with_lights) : w(b), lights(with_lights) {
        world_state_get(e);
        world_state_put(w.st);
        if (lights) {
            world_lights_get(el);
            world_lights_put(w.lights);
        }
    }
    ~Swap() {
        world_state_get(w.st);
        world_state_put(e);
        if (lights) world_lights_put(el);
    }
};

constexpr uint32_t kBatch = 256;      // (instances a step)
constexpr uint32_t kLightBatch = 24;   // (tie instances lit a step: about 1 ms)
constexpr int32_t kTfragBatch = 96;    // (tfrag entries lit a step)

}  // namespace

// orig: (host) see section_world.h
bool world_read(int level, bool pal, bool coll, bool sky, WorldBytes& out, std::string& err) {
    char dir[64];
    std::snprintf(dir, sizeof dir, "assets/disc/levels/level%02d/", level);
    std::vector<uint8_t> wad, packed;
    if (!read_all(std::string(dir) + "data.wad", wad) || wad.size() < 0x58) {
        err = std::string(dir) + "data.wad cannot be read";
        return false;
    }
    auto part = [&](int i, std::vector<uint8_t>& v) {
        const uint32_t o = rd32(wad, 8 * static_cast<size_t>(i)), n = rd32(wad, 8 * static_cast<size_t>(i) + 4);
        if (static_cast<size_t>(o) + n > wad.size()) return false;
        v.assign(wad.begin() + o, wad.begin() + o + n);
        return true;
    };
    std::vector<uint8_t> core;
    if (!part(2, out.index) || out.index.size() < 0xc0 || !part(10, packed) ||
        !decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(packed.data()), packed.size()), core)) {
        err = std::string(dir) + "data.wad: its asset data cannot be read";
        return false;
    }
    out.core = core.size();
    const std::vector<uint8_t>& I = out.index;
    auto H = [&](uint32_t f) { return rd32(I, f); };
    // every block the index names starts somewhere: a block ends at the next start
    std::set<uint32_t> starts;
    auto name = [&](uint32_t v) {
        if (v < core.size()) starts.insert(v);
    };
    for (uint32_t f : {0x08u, 0x0cu, 0x10u, 0x14u, 0x60u, 0x64u, 0x68u, 0x7cu, 0xa0u, 0xa4u, 0xa8u}) name(H(f));
    for (uint32_t k = 0; k < H(0x18) && H(0x1c) + 32 * k + 4 <= I.size(); ++k) name(rd32(I, H(0x1c) + 32 * k));
    for (uint32_t k = 0; k < H(0x20) && H(0x24) + 32 * k + 4 <= I.size(); ++k) name(rd32(I, H(0x24) + 32 * k));
    for (uint32_t k = 0; k < H(0x28) && H(0x2c) + 0x30 * k + 4 <= I.size(); ++k) name(rd32(I, H(0x2c) + 0x30 * k));
    for (uint32_t k = 0; k < 256 && H(0x78) + 4 * k + 4 <= I.size(); ++k) name(rd32(I, H(0x78) + 4 * k));
    for (uint32_t k = 0; k < H(0x80) && H(0x84) + 16 * k + 4 <= I.size(); ++k) name(rd32(I, H(0x84) + 16 * k));
    starts.insert(static_cast<uint32_t>(core.size()));
    auto block = [&](uint32_t lo, const char* what, std::vector<uint8_t>* into) {
        if (lo >= core.size()) return;
        const uint32_t hi = *starts.upper_bound(lo);
        if (into != nullptr) {
            into->assign(core.begin() + lo, core.begin() + hi);
            return;
        }
        WorldBytes::Range r;
        r.lo = lo;
        r.what = what;
        r.b.assign(core.begin() + lo, core.begin() + hi);
        out.ranges.push_back(std::move(r));
    };
    block(H(0x08), "tfrags", nullptr);
    if (H(0x0c) != 0) block(H(0x0c), "zone grid", nullptr);
    block(H(0x60), "texels", nullptr);
    for (uint32_t k = 0; k < H(0x20); ++k) block(rd32(I, H(0x24) + 32 * k), "tie class", nullptr);
    for (uint32_t k = 0; k < H(0x28); ++k) block(rd32(I, H(0x2c) + 0x30 * k), "shrub class", nullptr);
    if (coll) block(H(0x14), "collision", &out.coll);
    if (sky && H(0x10) != 0 && H(0x14) > H(0x10) && H(0x14) <= core.size()) out.sky.assign(core.begin() + H(0x10), core.begin() + H(0x14));
    if (!read_all(std::string(dir) + (pal ? "gameplay_pal.bin" : "gameplay_ntsc.bin"), packed) ||
        !decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(packed.data()), packed.size()), out.gp)) {
        err = std::string(dir) + "its gameplay file cannot be read";
        return false;
    }
    return true;
}

// orig: (host) see section_world.h
void world_state_get(WorldState& s) {
    s.words.clear();
    s.tables.clear();
    for (const uint32_t a : kWords) s.words.push_back(lwu(ov(a)));
    for (const auto& t : kTables) {
        const uint32_t lo = ov(t[0]), n = t[1] - t[0];
        const uint8_t* p = static_cast<const uint8_t*>(ee_ram_ptr(lo));
        s.tables.insert(s.tables.end(), p, p + n);
    }
}

// orig: (host) see section_world.h
void world_state_put(const WorldState& s) {
    if (s.words.size() != std::size(kWords)) return;
    for (size_t i = 0; i < std::size(kWords); ++i) sw(ov(kWords[i]), s.words[i]);
    size_t at = 0;
    for (const auto& t : kTables) {
        const uint32_t n = t[1] - t[0];
        std::memcpy(ee_ram_ptr_write(ov(t[0]), n), s.tables.data() + at, n);
        at += n;
    }
}

// orig: (host) see section_world.h
void world_lights_get(WorldLights& l) {
    l.grid_count = lwu(OV(kGridCount));
    l.bytes.clear();
    for (const auto& t : kLights) {
        const uint8_t* p = static_cast<const uint8_t*>(ee_ram_ptr(ov(t[0])));
        l.bytes.insert(l.bytes.end(), p, p + (t[1] - t[0]));
    }
}

// orig: (host) see section_world.h
void world_lights_put(const WorldLights& l) {
    if (l.bytes.empty()) return;
    sw(OV(kGridCount), l.grid_count);
    size_t at = 0;
    for (const auto& t : kLights) {
        const uint32_t n = t[1] - t[0];
        std::memcpy(ee_ram_ptr_write(ov(t[0]), n), l.bytes.data() + at, n);
        at += n;
    }
}

// orig: (host) see section_world.h
uint32_t WorldBuild::core(uint32_t off) const {
    for (const At& a : map)
        if (off >= a.lo && off < a.lo + a.size) return a.at + (off - a.lo);
    return 0;
}

// orig: (host) see section_world.h
bool world_build_begin(WorldBuild& w, int level, const WorldBytes& b, uint32_t gp, const std::string& owner) {
    w = WorldBuild{};
    w.level = level;
    w.gp = gp;
    uint32_t total = (static_cast<uint32_t>(b.index.size()) + 0xffu) & ~0xffu;
    for (const WorldBytes::Range& r : b.ranges) total += (static_cast<uint32_t>(r.b.size()) + 0xffu) & ~0xffu;
    w.block = session_heap_alloc(total + 0x100, owner);
    if (w.block == 0) return false;
    w.bytes = total + 0x100;
    uint32_t at = (w.block + 0xffu) & ~0xffu;  // (the blocks 256-byte aligned, as the level's asset data has them)
    w.index = at;
    std::memcpy(ee_ram_ptr_write(at, static_cast<uint32_t>(b.index.size())), b.index.data(), b.index.size());
    at += (static_cast<uint32_t>(b.index.size()) + 0xffu) & ~0xffu;
    for (const WorldBytes::Range& r : b.ranges) {
        w.map.push_back({r.lo, static_cast<uint32_t>(r.b.size()), at});
        at += (static_cast<uint32_t>(r.b.size()) + 0xffu) & ~0xffu;
    }
    world_state_get(w.st);  // (a start: the engine's words and tables, the counts made 0 at the registration)
    return true;
}

// orig: (host) see section_world.h
bool world_build_step(WorldBuild& w, const WorldBytes& b, const std::string& owner) {
    const double t0 = now_ms();
    const int ran = w.step;
    const uint32_t I = w.index, G = w.gp;
    auto Hx = [I](uint32_t f) { return lwu(I + f); };
    bool more = true;
    switch (w.step) {
    case 0: {  // the blocks copied, 64 KB a step
        if (w.r >= b.ranges.size()) {
            ++w.step;
            break;
        }
        const WorldBytes::Range& r = b.ranges[w.r];
        const size_t n = std::min<size_t>(0x10000, r.b.size() - w.copied);
        if (n != 0) std::memcpy(ee_ram_ptr_write(w.map[w.r].at + static_cast<uint32_t>(w.copied), static_cast<uint32_t>(n)), r.b.data() + w.copied, n);
        w.copied += n;
        if (w.copied >= r.b.size()) {
            ++w.r;
            w.copied = 0;
        }
        break;
    }
    case 1: {  // the registrations, as level_setup_body makes them: the textures' tables, the tfrags, the zone grid, the tie and shrub classes
        Swap sw_(w, false);
        const uint32_t texels = w.core(Hx(0x60));
        const uint32_t keep_texels = lwu(OV(0x0015f40c));
        sw(OV(0x0015f40c), texels);
        sw(OV(0x00160fbc), 0);
        sw(OV(0x0016048c), 0);
        sw(OV(0x00160f54), Hx(0x30));
        tex_table(OV(0x001c4a00), I + Hx(0x34), static_cast<int32_t>(Hx(0x30)), texels);
        sw(OV(0x00160fd4), Hx(0x40));
        tex_table(OV(0x001c6700), I + Hx(0x44), static_cast<int32_t>(Hx(0x40)), texels);
        sw(OV(0x001604ac), Hx(0x48));
        tex_table(OV(0x001bc5b0), I + Hx(0x4c), static_cast<int32_t>(Hx(0x48)), texels);
        moby_lod_set(w.core(Hx(0x08)), I + Hx(0x34));
        sw(OV(0x0015f600), Hx(0x0c) != 0 ? w.core(Hx(0x0c)) : 0u);
        sw(OV(0x0016c074), 0);
        uint32_t e = I + Hx(0x24);
        for (uint32_t i = 0; i < Hx(0x20); ++i, e += 0x20) tie_class_register(w.core(lwu(e)), I + Hx(0x44), e + 0x10, lwu(e + 4));
        e = I + Hx(0x2c);
        for (uint32_t i = 0; i < Hx(0x28); ++i, e += 0x30) shrub_class_register(w.core(lwu(e)), I + Hx(0x4c), e + 0x10, e + 0x20, lwu(e + 4));
        sw(OV(0x0015f40c), keep_texels);
        // no instances yet
        for (const uint32_t a : {0x00160fc0u, 0x00160fc4u, 0x00160fc8u, 0x00160fccu, 0x00160490u, 0x00160494u, 0x00160498u, 0x0016049cu, 0x001604a0u, 0x00160b74u})
            sw(ov(a), 0);
        w.tfrag_n = lwu(OV(0x00160f50));
        // its level's lights, as gameplay_load sets them (the directional lights, at most 12; the light grid)
        WorldLights keep;
        world_lights_get(keep);
        mem_clear128(OV(0x001802c0), 0x100);
        mem_clear128(OV(0x001804c0), 0x180);
        mem_clear128(OV(0x0017fec0), 0x400);
        {
            uint32_t p = G + lwu(G + 4);
            int32_t n = lw(p);
            p += 0x10;
            n = std::min(n, 0xc);
            if (n > 0) mem_copy(OV(0x0017fec0), p, n << 6);
        }
        if (lw(G + 0x80) == 0) {
            sw(OV(0x0015fbc4), 0);
        } else {
            uint32_t p = G + lwu(G + 0x80);
            const int32_t c = std::min(lw(p), 32);
            p += 0x10;
            sw(OV(0x0015fbc4), static_cast<uint32_t>(c));
            if (c > 0) {
                mem_copy(OV(0x0017ecc0), p, c << 4);
                mem_copy(OV(0x0017eec0), p + static_cast<uint32_t>(lw(G + lwu(G + 0x80))) * 16, c << 7);
                uint32_t q = OV(0x0017eec0);
                for (int32_t k = c; k != 0; --k, q += 0x80) {
                    if (lw(q + 0x48) < 0) sw(q + 0x48, 0xb);
                    if (lw(q + 0x4c) < 0) sw(q + 0x4c, 0xb);
                }
            }
        }
        world_lights_get(w.lights);
        world_lights_put(keep);
        ++w.step;
        break;
    }
    case 2: {  // the tie instances (gameplay_load's), a batch a step: 0x20-byte bounds, 0x1c0-byte data
        Swap sw_(w, false);
        const uint32_t base = G + lwu(G + 0x34);
        const uint32_t nt = lwu(base);
        if (w.k == 0) {
            w.tie_n = nt;
            const uint32_t bounds = nt != 0 ? session_heap_alloc(nt << 5, owner) : 0, data = nt != 0 ? session_heap_alloc(nt * 0x1c0, owner) : 0;
            if (nt != 0 && (bounds == 0 || data == 0)) {
                std::fprintf(stderr, "[section] world %d: no room in the session heap for its tie instances: none drawn\n", w.level);
                w.tie_n = 0;
                w.step = 4;
                break;
            }
            w.bytes += (nt << 5) + nt * 0x1c0;
            sw(OV(0x00160fc0), bounds);
            sw(OV(0x00160fcc), nt);
            sw(OV(0x00160fc8), data);
            sw(OV(0x00160fc4), bounds + nt * 32);
            w.last = -1;
        }
        uint32_t s4 = base + 0x10 + static_cast<uint32_t>(w.k) * 0xe0;
        const int32_t end = std::min<int32_t>(static_cast<int32_t>(w.tie_n), w.k + static_cast<int32_t>(kBatch));
        for (int32_t s7 = w.k; s7 < end; ++s7) {
            const uint32_t cls = lwu(s4);
            const uint32_t s6 = static_cast<uint32_t>(s7) * 0x1c0;
            const uint32_t s3 = lwu(OV(0x00160fc0)) + static_cast<uint32_t>(s7) * 32;
            const uint32_t ti = lbu(cls + OV(0x001c5b00));
            sb(s3 + 0x1a, ti);
            const uint32_t s5 = lwu(OV(0x00160fc8)) + s6;
            if (static_cast<int32_t>(ti & 0xff) != w.last) {
                sw(lwu(OV(0x001c5800) + ti * 4) + 0x28, s3);
                sh(lwu(OV(0x001c5800) + lbu(s3 + 0x1a) * 4u) + 0x26, 0);
                w.last = static_cast<int32_t>(lbu(s3 + 0x1a));
            }
            const auto hdr = [s3] { return lwu(OV(0x001c5800) + lbu(s3 + 0x1a) * 4u); };
            sh(hdr() + 0x26, lhu(hdr() + 0x26) + 1u);
            sw(s3 + 0x10, s5);
            const float d = static_cast<float>(lw(s4 + 4));
            sh(s3 + 0x1e, 0xffff);
            sb(s3 + 0x1b, 0);
            swc1(s3 + 0x14, d);
            sh(s3 + 0x1c, 0);
            const uint32_t w0c = lwu(s4 + 0xc);
            copy_mat(s5, s4 + 0x10);
            sh(s3 + 0x18, w0c);
            s4 += 0x50;
            swc1(s5 + 0x3c, lwc1(hdr() + 0x40));
            mat3_transform(V(s3), V(hdr() + 0x30), M(s5));
            float m = max_f(vec3_length(V(s5)), vec3_length(V(s5 + 0x10)));
            m = max_f(m, vec3_length(V(s5 + 0x20)));
            swc1(s3 + 0xc, lwc1(hdr() + 0x3c) * m);
            vec4_scale(V(s3), V(s3), lwc1(s5 + 0x3c));
            vec3_add(V(s3), V(s3), V(s5 + 0x30));
            swc1(lwu(OV(0x00160fc8)) + s6 + 0xc, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6))));
            swc1(lwu(OV(0x00160fc8)) + s6 + 0x1c, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6 + 0x10))));
            swc1(lwu(OV(0x00160fc8)) + s6 + 0x2c, fpu_div(1.0f, vec3_length(V(lwu(OV(0x00160fc8)) + s6 + 0x20))));
            mem_copy(s5 + 0x140, s4, 0x80);
            s4 += 0x80;
            sh(s3 + 0x1c, lhu(s4));
            s4 += 0x10;
        }
        w.k = end;
        if (w.k >= static_cast<int32_t>(w.tie_n)) {
            w.k = 0;
            ++w.step;
        }
        break;
    }
    case 3: {  // their static lighting (world_tie_lighting), a batch a step
        Swap sw_(w, true);
        const int32_t end = std::min<int32_t>(static_cast<int32_t>(w.tie_n), w.k + static_cast<int32_t>(kLightBatch));
        uint32_t v = 0x70000000u;
        for (int32_t i = w.k; i < end; ++i, v += 2) sh(v, static_cast<uint32_t>(i));
        sh(v, 0xffff);
        if (end > w.k) world_tie_lighting(0x70000000u);
        w.k = end;
        if (w.k >= static_cast<int32_t>(w.tie_n)) {
            w.k = 0;
            ++w.step;
        }
        break;
    }
    case 4: {  // the shrub instances (gameplay_load's), a batch a step: bounds, data (0x40 each), colours (0x60 each)
        Swap sw_(w, false);
        const uint32_t base = G + lwu(G + 0x3c);
        const uint32_t ns = lwu(base);
        if (w.k == 0) {
            w.shrub_n = ns;
            const uint32_t bounds = ns != 0 ? session_heap_alloc(ns << 5, owner) : 0, data = ns != 0 ? session_heap_alloc(ns << 6, owner) : 0,
                           cols = ns != 0 ? session_heap_alloc(ns * 0x60, owner) : 0;
            if (ns != 0 && (bounds == 0 || data == 0 || cols == 0)) {
                std::fprintf(stderr, "[section] world %d: no room in the session heap for its shrub instances: none drawn\n", w.level);
                w.shrub_n = 0;
                w.step = 6;
                break;
            }
            w.bytes += (ns << 5) + (ns << 6) + ns * 0x60;
            sw(OV(0x00160494), bounds);
            sw(OV(0x00160490), ns);
            sw(OV(0x0016049c), data);
            sw(OV(0x001604a0), cols);
            sw(OV(0x00160b74), cols);
            sw(OV(0x00160498), bounds + ns * 32);
            w.last = -1;
        }
        uint32_t s6 = base + 0x10 + static_cast<uint32_t>(w.k) * 0x70;
        const int32_t end = std::min<int32_t>(static_cast<int32_t>(w.shrub_n), w.k + static_cast<int32_t>(kBatch));
        for (int32_t i = w.k; i < end; ++i) {
            const uint32_t s5 = lwu(OV(0x00160494)) + static_cast<uint32_t>(i) * 32;
            const uint32_t s7 = lwu(OV(0x0016049c)) + static_cast<uint32_t>(i) * 64;
            const uint32_t si = lbu(lwu(s6) + OV(0x001bc1b0));
            sb(s5 + 0x1a, si);
            if (static_cast<int32_t>(si) != w.last) {
                w.last = static_cast<int32_t>(si);
                sw(lwu(OV(0x001bc030) + si * 4) + 0x18, s5);
                sh(lwu(OV(0x001bc030) + si * 4) + 0x16, 0);
            }
            const uint32_t s3 = OV(0x001bc030) + si * 4;
            sh(lwu(s3) + 0x16, lhu(lwu(s3) + 0x16) + 1u);
            sh(s5 + 0x18, static_cast<uint32_t>(i));
            const float d = lwc1(s6 + 4);
            swc1(s5 + 0x10, d);
            if (d < 16.0f) swc1(s5 + 0x10, 16.0f);
            sb(s5 + 0x1b, 0);
            sh(s5 + 0x1e, 0xffff);
            const uint32_t bb = lwu(lwu(s3) + 0x1c);
            if (bb != 0) {
                const int32_t r = float_to_int(lwc1(bb));
                sb(s5 + 0x17, static_cast<uint32_t>(r));
                const float f = int_to_float(r & 0xff) + 24.0f;
                if (lwc1(s5 + 0x10) < f) swc1(s5 + 0x10, f);
            }
            copy_mat(s7, s6 + 0x10);
            s6 += 0x50;
            swc1(s7 + 0x3c, lwc1(lwu(s3) + 0x20));
            float a = vec3_length(V(s7));
            a = (a + vec3_length(V(s7 + 0x10))) * 0.5f;
            const float bl = vec3_length(V(s7 + 0x20));
            int32_t r0 = float_to_int(a * 4096.0f);
            int32_t r1 = float_to_int(bl * 4096.0f);
            if (0x10000 < r1) r1 = 0x10000;
            if (0x10000 < r0) r0 = 0x10000;
            sw(s7 + 0x2c, static_cast<uint32_t>(r0) | static_cast<uint32_t>(r1) << 16);
            const uint32_t c0 = lwu(s6), c1 = lwu(s6 + 4), c2 = lwu(s6 + 8);
            s6 += 0x10;
            sw(s7 + 0xc, c2 << 16 | (c1 << 8 | 0x80000000u) | c0);
            sh(s5 + 0x1c, lhu(s6));
            s6 += 0x10;
            mat3_transform(V(s5), V(lwu(s3)), M(s7));
            float m = max_f(vec3_length(V(s7)), vec3_length(V(s7 + 0x10)));
            m = max_f(m, vec3_length(V(s7 + 0x20)));
            swc1(s5 + 0xc, lwc1(lwu(s3) + 0xc) * m);
            vec4_scale(V(s5), V(s5), lwc1(s7 + 0x3c));
            vec3_add(V(s5), V(s5), V(s7 + 0x30));
        }
        w.k = end;
        if (w.k >= static_cast<int32_t>(w.shrub_n)) {
            w.k = 0;
            ++w.step;
        }
        break;
    }
    case 5: {  // their static lighting (world_shrub_lighting) and average colour, a batch a step
        Swap sw_(w, true);
        const int32_t end = std::min<int32_t>(static_cast<int32_t>(w.shrub_n), w.k + static_cast<int32_t>(2 * kLightBatch));
        uint32_t v = 0x70000000u;
        for (int32_t k = w.k; k < end; ++k, v += 2) sh(v, static_cast<uint32_t>(k));
        sh(v, 0xffff);
        if (end > w.k) world_shrub_lighting(0x70000000u);
        for (int32_t k = w.k; k < end; ++k) {
            uint32_t a = lwu(OV(0x001604a0)) + static_cast<uint32_t>(k) * 0x60;
            int32_t r = 0, g = 0, bl = 0;
            for (int32_t n = 0; n < 24; ++n, a += 4) {
                r += lbu(a);
                g += lbu(a + 1);
                bl += lbu(a + 2);
            }
            const uint32_t e = static_cast<uint32_t>(k) * 64 + lwu(OV(0x0016049c));
            sw(e + 0x1c, static_cast<uint32_t>(bl / 24) << 16 | static_cast<uint32_t>(g / 24) << 8 | static_cast<uint32_t>(r / 24));
        }
        w.k = end;
        if (w.k >= static_cast<int32_t>(w.shrub_n)) {
            w.k = 0;
            ++w.step;
        }
        break;
    }
    case 6: {  // the tfrag entries' static lighting (world_tfrag_lighting), as gameplay_load lights them (0x3ff a call), a batch a step
        Swap sw_(w, true);
        const int32_t n = lw(OV(0x00160f50));
        if (w.k == 0 && n > 0) {
            uint32_t a = lwu(OV(0x00160f4c)) + 0x36;
            for (int32_t k = n; k != 0; --k, a += 0x40) sh(a, 0xffff);
        }
        if (w.k < n) {
            uint32_t v = 0x70003000u;
            int32_t c = 0;
            for (; c < kTfragBatch && w.k < n; ++c, ++w.k, v += 2) sh(v, static_cast<uint32_t>(w.k));
            sh(v, 0xffff);
            world_tfrag_lighting(0x70003000u);
        }
        if (w.k >= n) {
            w.k = 0;
            ++w.step;
        }
        break;
    }
    case 7: {  // the LOD / visibility masks (gameplay_load's: the gameplay file's +0x8c lists when they match the level's tfrags and ties)
        Swap sw_(w, false);
        if (lw(OV(0x0015f600)) != 0 && lw(G + 0x8c) != 0) {
            w.masks = true;
            const uint32_t s5 = G + lwu(G + 0x8c);
            const int32_t nl = lw(s5), ntie = lw(s5 + 4);
            uint32_t a1 = s5 + 0x10;
            bool lod_ok = lw(OV(0x00160f50)) == nl, tie_ok = static_cast<int32_t>(lwu(OV(0x00160fc4)) - lwu(OV(0x00160fc0))) >> 5 == ntie;
            if (lod_ok)
                for (int32_t k = 0; k < nl; ++k)
                    if ((lbu(lwu(OV(0x00160f4c)) + static_cast<uint32_t>(k) * 0x40 + 0x3d) ^ lwu(a1 + static_cast<uint32_t>(k) * 8 + 4)) != 0) lod_ok = false;
            {
                uint32_t a = lwu(OV(0x00160f4c)) + 0x3a, r = a1;
                for (int32_t k = lw(OV(0x00160f50)); k > 0; --k, a += 0x40, r += 8) sh(a, lod_ok ? mask_bit(lw(r)) : 0x7f80u);
            }
            const uint32_t ties = a1 + (static_cast<uint32_t>(nl) << 3);
            if (tie_ok)
                for (int32_t k = 0; k < ntie; ++k)
                    if ((lhu(lwu(OV(0x00160fc0)) + static_cast<uint32_t>(k) * 0x20 + 0x18) ^ lhu(ties + static_cast<uint32_t>(k) * 8 + 4)) != 0) tie_ok = false;
            for (uint32_t t = lwu(OV(0x00160fc0)), k = 0; t != lwu(OV(0x00160fc4)); t += 0x20, ++k) {
                if (tie_ok) {
                    sh(t + 0x18, mask_bit(lw(ties + k * 8)));
                } else {
                    const int32_t j = find_id(ties, ntie, lhu(t + 0x18));
                    sh(t + 0x18, j == ntie ? 0x7f80u : mask_bit(lw(ties + static_cast<uint32_t>(j) * 8)));
                }
            }
            sw(OV(0x0016c074), 2);
        } else {
            for (uint32_t t = lwu(OV(0x00160fc0)); t != lwu(OV(0x00160fc4)); t += 0x20) sh(t + 0x18, 0x7f80);
            uint32_t a = lwu(OV(0x00160f4c)) + 0x3a;
            for (int32_t k = lw(OV(0x00160f50)); k > 0; --k, a += 0x40) sh(a, 0x7f80);
            sw(OV(0x0016c074), 0);
        }
        ++w.step;
        break;
    }
    default:
        more = false;
        break;
    }
    const double dt = now_ms() - t0;
    w.ms += dt;
    const int si = std::clamp(ran, 0, 8);
    w.step_max[si] = std::max(w.step_max[si], dt);
    return more;
}

// orig: (host) see section_world.h
void world_moby_mask(const WorldBuild& w, uint32_t m, int32_t uid) {
    if (!w.masks || w.gp == 0) return;
    const uint32_t G = w.gp;
    const uint32_t s5 = G + lwu(G + 0x8c);
    const int32_t nl = lw(s5), ntie = lw(s5 + 4), nm = lw(s5 + 8);
    const uint32_t a1 = s5 + 0x10 + (static_cast<uint32_t>(nl) << 3) + (static_cast<uint32_t>(ntie) << 3);
    if (lhu(m + 0x36) == 0x7f80) return;
    const int32_t k = find_id(a1, nm, uid);
    sh(m + 0x36, k == nm ? 0x7f80u : mask_bit(lw(a1 + static_cast<uint32_t>(k) * 8)));
}

// orig: (host) see section_world.h
void world_probe_frame(uint64_t frame) {
    static const char* env = std::getenv("RAC1_COLL_PROBE");
    if (env == nullptr) return;
    static unsigned long long at = 0;
    static float g[6] = {};
    static const bool ok = std::sscanf(env, "%llu:%f,%f,%f,%f,%f,%f", &at, &g[0], &g[1], &g[2], &g[3], &g[4], &g[5]) == 7 && g[4] > 0;
    if (!ok || frame != at) return;
    std::FILE* list = std::getenv("RAC1_COLL_PROBE_FILE") != nullptr ? std::fopen(std::getenv("RAC1_COLL_PROBE_FILE"), "w") : nullptr;
    uint64_t h = 1469598103934665603ull;
    int n = 0, hits = 0;
    double sum = 0;
    for (float y = g[1]; y <= g[3]; y += g[4])
        for (float x = g[0]; x <= g[2]; x += g[4]) {
            const Vec4 p{x, y, g[5], 1.0f};
            const float z = floor_probe_v(0.0f, p, 0);
            uint32_t b;
            std::memcpy(&b, &z, 4);
            h = (h ^ b) * 1099511628211ull;
            ++n;
            if (list != nullptr) std::fprintf(list, "%.1f %.1f %.4f\n", x, y, z);
            if (z > 0) {
                ++hits;
                sum += z;
            }
        }
    if (list != nullptr) std::fclose(list);
    std::printf("[probe] frame %llu, the collision %08x: %d points, %d hit the floor, their heights' sum %.3f, hash %016llx\n", static_cast<unsigned long long>(frame),
                lwu(OV(0x00173e40)), n, hits, sum, static_cast<unsigned long long>(h));
}

// orig: (host) see section_world.h
void world_renderer_level(int level) { g_renderer_level.store(level); }
// orig: (host) see section_world.h
int world_renderer_level_get() { return g_renderer_level.load(); }

}  // namespace rac1
