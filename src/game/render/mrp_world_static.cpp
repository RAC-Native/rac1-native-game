// orig: (host) The MRP world's static data (see mrp_world_static.h).
#include <game/render/mrp_world_static.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vu0_regs.h>
#include <game/render/mrp_world_assets.h>
#include <runtime/kernel/ee_memory.h>
#include "io/host_endian.h"

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>

namespace rac1::mrpw {

// orig: (host) see mrp_world_static.h
ColourSource colour_source() {
    ColourSource s;
    s.tie_data = lwu(OV(0x00160fc8u));
    const int32_t tn = lw(OV(0x00160fccu));
    s.tie_count = tn > 0 && tn < 0x10000 && s.tie_data != 0 ? static_cast<uint32_t>(tn) : 0;
    s.shrub_cols = lwu(OV(0x001604a0u));
    const int32_t sn = lw(OV(0x00160490u));
    s.shrub_count = sn > 0 && sn < 0x10000 && s.shrub_cols != 0 ? static_cast<uint32_t>(sn) : 0;
    return s;
}

// orig: (host) see mrp_world_static.h
int64_t ResidentColours::tie_slot(uint32_t addr) const {
    if (src.tie_count == 0 || addr < src.tie_data + kTieColourOffset) return -1;
    const uint32_t d = addr - src.tie_data - kTieColourOffset;
    if (d % kTieDataStride != 0 || d / kTieDataStride >= src.tie_count) return -1;
    return static_cast<int64_t>(d / kTieDataStride) * kTieColours;
}

// orig: (host) see mrp_world_static.h
int64_t ResidentColours::shrub_slot(uint32_t addr) const {
    if (src.shrub_count == 0 || addr < src.shrub_cols) return -1;
    const uint32_t d = addr - src.shrub_cols;
    if (d % kShrubColourStride != 0 || d / kShrubColourStride >= src.shrub_count) return -1;
    return static_cast<int64_t>(shrub_first()) + static_cast<int64_t>(d / kShrubColourStride) * kShrubColours;
}

namespace {
// orig: (host) one table's instances compared with the mirror from `first` (u32 index), `n` instances of `words` colours at
// EE `base` + `stride` x i (+ `off`)
void compare_table(ResidentColours& rc, uint32_t first, uint32_t n, uint32_t words, uint32_t base, uint32_t stride, uint32_t off, bool all) {
    for (uint32_t i = 0; i < n; ++i) {
        const uint32_t at = first + i * words;
        const void* p = ee_ram_ptr_read(base + stride * i + off);
        if (p == nullptr) continue;
        uint32_t* m = rc.mirror.data() + at;
        if (!all && std::memcmp(m, p, words * 4) == 0) continue;
        std::memcpy(m, p, words * 4);
        ++rc.instances_changed;
        if (all) continue;
        if (!rc.dirty.empty() && rc.dirty.back().first + rc.dirty.back().second == at) rc.dirty.back().second += words;
        else rc.dirty.push_back({at, words});
        rc.dirty_words.insert(rc.dirty_words.end(), m, m + words);
    }
}
}  // namespace

// orig: (host) see mrp_world_static.h
void resident_colours_update(ResidentColours& rc, const ColourSource& src) {
    ++rc.updates;
    bool all = rc.all_dirty;
    if (!(src == rc.src) || rc.mirror.size() != static_cast<size_t>(src.tie_count) * kTieColours + static_cast<size_t>(src.shrub_count) * kShrubColours) {
        rc.src = src;
        rc.mirror.assign(static_cast<size_t>(src.tie_count) * kTieColours + static_cast<size_t>(src.shrub_count) * kShrubColours, 0);
        all = true;
    }
    if (all) {
        rc.dirty.clear();
        rc.dirty_words.clear();
    }
    compare_table(rc, 0, src.tie_count, kTieColours, src.tie_data, kTieDataStride, kTieColourOffset, all);
    compare_table(rc, rc.shrub_first(), src.shrub_count, kShrubColours, src.shrub_cols, kShrubColourStride, 0, all);
    rc.all_dirty = all;  // (the draw sends the whole table and clears it)
}

namespace {
// (the Wii U port) the instances relit since the last update: tie indices, then shrub indices; `over` past kTouchMax notes
constexpr size_t kTouchMax = 4096;
std::vector<uint32_t> g_touch[2];
bool g_touch_over = false;
// (the Wii U port) the host's last drawn entries by kind (relight_visible_mark)
std::vector<uint8_t> g_vis[3];
bool g_vis_have[3] = {};
}  // namespace

// orig: (host) see mrp_world_static.h
void relight_visible_clear() {
    for (int k = 0; k < 3; ++k) {
        std::fill(g_vis[k].begin(), g_vis[k].end(), uint8_t{0});
        g_vis_have[k] = false;
    }
}

// orig: (host) see mrp_world_static.h
void relight_visible_mark(int kind, uint32_t index) {
    if (kind < 0 || kind > 2) return;
    g_vis_have[kind] = true;
    if (index >= 0x10000) return;  // (~0u: the kind's culling ran, nothing marked by it)
    std::vector<uint8_t>& v = g_vis[kind];
    if (v.size() <= index) v.resize(index + 1, 0);
    v[index] = 1;
}

// orig: (host) see mrp_world_static.h
int relight_visible(int kind, uint32_t index) {
    if (kind < 0 || kind > 2 || !g_vis_have[kind]) return -1;
    return index < g_vis[kind].size() ? g_vis[kind][index] : 0;
}

// orig: (host) see mrp_world_static.h
void resident_colours_touch(bool shrub, uint32_t index) {
    if (g_touch_over) return;
    if (g_touch[0].size() + g_touch[1].size() >= kTouchMax) {
        g_touch_over = true;
        return;
    }
    std::vector<uint32_t>& v = g_touch[shrub ? 1 : 0];
    if (v.empty() || v.back() != index) v.push_back(index);
}

// orig: (host) see mrp_world_static.h
void resident_colours_update_touched(ResidentColours& rc, const ColourSource& src, bool drop) {
    const bool none = g_touch[0].empty() && g_touch[1].empty() && !g_touch_over;
    if (drop || none) {
    } else if (g_touch_over || !(src == rc.src) ||
               rc.mirror.size() != static_cast<size_t>(src.tie_count) * kTieColours + static_cast<size_t>(src.shrub_count) * kShrubColours) {
        resident_colours_update(rc, src);
    } else {
        for (int k = 0; k < 2; ++k) {  // (ascending slots: the ties' first, as compare_table's ranges want them)
            std::vector<uint32_t>& v = g_touch[k];
            std::sort(v.begin(), v.end());
            v.erase(std::unique(v.begin(), v.end()), v.end());
            for (uint32_t i : v) {
                if (k == 0 && i < src.tie_count) {
                    compare_table(rc, i * kTieColours, 1, kTieColours, src.tie_data + i * kTieDataStride, kTieDataStride, kTieColourOffset, false);
                } else if (k == 1 && i < src.shrub_count) {
                    compare_table(rc, rc.shrub_first() + i * kShrubColours, 1, kShrubColours, src.shrub_cols + i * kShrubColourStride, kShrubColourStride, 0, false);
                }
            }
        }
    }
    g_touch[0].clear();
    g_touch[1].clear();
    g_touch_over = false;
}

// ---- stage 2 ---------------------------------------------------------------------------------------------------------------

namespace {
StaticSnapshot g_snap;

float rf(uint32_t a) {
    const void* p = ee_ram_ptr_read(a);
    if (p == nullptr) return 0.0f;
    float f;
    std::memcpy(&f, p, 4);
    return f;
}
uint32_t ru(uint32_t a) {
    const void* p = ee_ram_ptr_read(a);
    if (p == nullptr) return 0;
    uint32_t v;
    std::memcpy(&v, p, 4);
    return v;
}
uint32_t rh(uint32_t a) { return ru(a & ~3u) >> (8 * (a & 2)) & 0xffff; }
uint32_t rb(uint32_t a) { return ru(a & ~3u) >> (8 * (a & 3)) & 0xff; }
std::array<float, 4> rq(uint32_t a) { return {rf(a), rf(a + 4), rf(a + 8), rf(a + 12)}; }
float bits_f(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
// (the LOD slider's scaling, mrp_lod.cpp: unlimited = 1e6)
float scaled(float d, float s) { return s == 1.0f ? d : s == 0.0f ? 1.0e6f : std::min(d * s, 1.0e6f); }
// FTOI4 / FTOI12 as the VU (saturated, toward zero)
int32_t ftoi(float f, float k) {
    const float v = f * k;
    if (std::isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0f)) return 0x7fffffff;
    if (!(v > -2147483648.0f)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(v);
}
}  // namespace

// orig: (host) see mrp_world_static.h. The addresses are the passes' own loads (world_cull.cpp 0x295fa0 / 0x286f10: vf24..vf31
// from 0x166c80 + 0x140 / 0 / 0x10 / 0x20 / 0x80..0xb0; vf18..vf22 from 0x16ca40 + 0x220 / 0xa0 / 0xb0 / 0x1e0; the zones 0x173d00
// copied to SPR 0x3000; the wind's table 0x1c3ef0 (SPR 0x3500 / 0x3b00) and constants 0x1c3ff0 (SPR 0x3e80 / 0x3c00), the
// clock 0x15f3f8 (SPR 0x3ea0 / 0x3c20); the maxima *(0x160fe0) / *(0x1604a4))
void static_note_pass(int kind, bool handle, float scale, float widen, float widen_y) {
    if (kind != 1 && kind != 2) return;
    StaticSnapshot& S = g_snap;
    S.on[kind - 1] = handle;
    S.scale = scale;
    S.widen = widen;
    S.widen_y = widen_y;
    for (uint32_t r = 0; r < 3; ++r) S.rot[r] = rq(OV(0x00166c80u) + 0x10 * r);
    for (uint32_t r = 0; r < 4; ++r) S.a[r] = rq(OV(0x00166d00u) + 0x10 * r);
    S.cam = rq(OV(0x00166dc0u));
    S.fog = rq(OV(0x0016cc60u));
    S.near = rq(OV(0x0016cae0u));
    S.tan = rq(OV(0x0016caf0u));
    S.rad = rq(OV(0x0016cc20u));
    S.c23 = rq(OV(0x0016cc30u));
    S.box = rq(OV(0x00160f90u));
    for (uint32_t r = 0; r < 4; ++r) S.clip[r] = rq(OV(0x00166d80u) + 0x10 * r);
    S.frame = ru(OV(0x0015f3f8u));
    for (uint32_t k = 0; k < 0x80; ++k) S.zone[k] = static_cast<uint8_t>(rb(OV(0x00173d00u) + k));
    for (uint32_t k = 0; k < 0x100; ++k) S.sine[k] = static_cast<int8_t>(rb(OV(0x001c3ef0u) + k));
    for (uint32_t k = 0; k < 8; ++k) S.wind[k] = rf(OV(0x001c3ff0u) + 4 * k);
    S.tie_max = ru(OV(0x00160fe0u));
    S.shrub_max = ru(OV(0x001604a4u));
    S.shrub_data = ru(OV(0x0016049cu));
    if (kind == 2) {  // (the shrub far variants' inputs, 0x287c04..: vf28..vf31 from 0x166c80 + 0x40.., vf21..vf23 from 0x16ca40 + 0x220 / 0x210 / 0x1a0)
        auto rw = [](uint32_t a) { return std::array<uint32_t, 4>{ru(a), ru(a + 4), ru(a + 8), ru(a + 12)}; };
        for (uint32_t r = 0; r < 4; ++r) S.vm[r] = rq(OV(0x00166cc0u) + 0x10 * r);
        S.vf22 = rq(OV(0x0016cc50u));
        S.vf23 = rq(OV(0x0016cbe0u));
        for (uint32_t q = 0; q < 4; ++q) S.vhdr[q] = rw(OV(0x001c2c10u) + 0x10 * q);
        for (uint32_t q = 0; q < 3; ++q) S.vhdr2[q] = rw(OV(0x001c2be0u) + 0x10 * q);
        for (uint32_t q = 0; q < 4; ++q) S.vtpl[q] = rw(OV(0x00160b80u) + 0x10 * q);
        for (uint32_t q = 0; q < 8; ++q) S.vcorner[q] = rq(OV(0x001c2cb0u) + 0x10 * q);
    }
    S.valid = true;
}

namespace {
// orig: (host) the class's packets' texture prims as the chain sends them to VU1 (tie: 5 qwords per slot from the class's prims
// *(rec + 0x2c) + the packet's source offset, world_cull.cpp 0x296cbc; shrub: the packet's own VIF list as the class holds it,
// its prims at 1 + tags + 4 t of its image)
void add_tex(StaticLevel& L, StaticPacket& sp, uint32_t addr, uint32_t qwords) {
    sp.tex_first = static_cast<uint32_t>(L.tex.size());
    for (uint32_t q = 0; q < qwords; ++q) L.tex.push_back({ru(addr + 16 * q), ru(addr + 16 * q + 4), ru(addr + 16 * q + 8), ru(addr + 16 * q + 12)});
    sp.ntex = qwords;
}
bool tie_matches(uint32_t rec, const TieClass& c) {
    if (c.header.size() < 0x44) return false;
    // (the LOD distances +0x10..+0x18, the packet / prim counts +0x20, the bytes +0x30..+0x43; +0x1c, +0x24.. and the lists are the
    // pass's / the level load's at run time)
    for (uint32_t o : {0x10u, 0x14u, 0x18u, 0x20u, 0x30u, 0x34u, 0x38u, 0x3cu, 0x40u}) {
        const uint32_t h = le32(c.header.data() + o);  // (the file's bytes: little-endian)
        if (ru(rec + o) != h) return false;
    }
    return true;
}
}  // namespace

// orig: (host) see mrp_world_static.h
bool static_level_build(StaticLevel& L, const World& w) {
    L = StaticLevel{};
    // assets with no tie / shrub class: the loading screen's flight (an empty world), not the EE's level - its
    // class tables are the level being left or loaded: none of their records is read
    const int32_t tn = w.ties.empty() ? 0 : lw(OV(0x00160fbcu));
    for (int32_t c = 0; c < tn && c < 4096; ++c) {
        StaticTieClass sc;
        sc.rec = lwu(OV(0x001c5800u) + 4 * static_cast<uint32_t>(c));
        const TieClass* cls = nullptr;
        if (static_cast<size_t>(c) < w.ties.size() && tie_matches(sc.rec, w.ties[static_cast<size_t>(c)])) cls = &w.ties[static_cast<size_t>(c)];
        for (size_t k = 0; cls == nullptr && k < w.ties.size(); ++k)
            if (tie_matches(sc.rec, w.ties[k])) cls = &w.ties[k];
        if (cls == nullptr) {
            ++L.tie_unmatched;
        } else {
            const uint32_t adgif = ru(sc.rec + 0x2c);
            for (const TiePacket& p : cls->packets) {
                if (p.lod > 2) continue;
                StaticPacket sp;
                sp.tie = &p;
                sp.tex_first = static_cast<uint32_t>(L.tex.size());
                for (int32_t prim : p.prims) add_tex(L, sp, adgif + 0x50 * static_cast<uint32_t>(prim), 5);
                sp.tex_first = static_cast<uint32_t>(L.tex.size()) - 5 * static_cast<uint32_t>(p.prims.size());
                sp.ntex = 5 * static_cast<uint32_t>(p.prims.size());
                sc.lod[p.lod].push_back(sp);
            }
        }
        L.ties.push_back(std::move(sc));
    }
    const int32_t sn = w.shrubs.empty() ? 0 : lw(OV(0x0016048cu));
    for (int32_t c = 0; c < sn && c < 4096; ++c) {
        StaticShrubClass sc;
        sc.rec = lwu(OV(0x001bc030u) + 4 * static_cast<uint32_t>(c));
        const int32_t np = static_cast<int16_t>(rh(sc.rec + 0x28));
        const ShrubClass* cls = nullptr;
        auto matches = [&](const ShrubClass& k) {
            if (static_cast<int32_t>(k.packets.size()) != np) return false;
            for (int32_t i = 0; i < np; ++i)
                if (static_cast<size_t>(ru(sc.rec + 0x44 + 8 * static_cast<uint32_t>(i))) != k.packets[static_cast<size_t>(i)].vif.size()) return false;
            return true;
        };
        if (static_cast<size_t>(c) < w.shrubs.size() && matches(w.shrubs[static_cast<size_t>(c)])) cls = &w.shrubs[static_cast<size_t>(c)];
        if (cls == nullptr) {
            ++L.shrub_unmatched;
        } else {
            for (int32_t i = 0; i < np; ++i) {
                const ShrubPacket& p = cls->packets[static_cast<size_t>(i)];
                const uint32_t off = ru(sc.rec + 0x40 + 8 * static_cast<uint32_t>(i));
                const uint32_t at = off < 0x00100000u ? sc.rec + off : off;
                ShrubPacket tmp;
                tmp.vif.resize(p.vif.size());
                for (size_t b = 0; b < tmp.vif.size(); ++b) tmp.vif[b] = static_cast<uint8_t>(rb(at + static_cast<uint32_t>(b)));
                const std::vector<uint8_t> img = shrub_image(tmp);
                StaticPacket sp;
                sp.shrub = &p;
                sp.tex_first = static_cast<uint32_t>(L.tex.size());
                const uint32_t q0 = 1 + static_cast<uint32_t>(p.header[1]);
                for (uint32_t q = 0; q < 4 * p.textures.size(); ++q) {
                    std::array<uint32_t, 4> v{};
                    if (16 * (q0 + q) + 16 <= img.size()) std::memcpy(v.data(), img.data() + 16 * (q0 + q), 16);
                    L.tex.push_back(v);
                }
                sp.ntex = 4 * static_cast<uint32_t>(p.textures.size());
                sc.packets.push_back(sp);
            }
        }
        L.shrubs.push_back(std::move(sc));
    }
    L.ok = true;
    std::printf("[mrpw static] level classes: %zu ties (%u not matched), %zu shrubs (%u not matched), %zu texture qwords\n", L.ties.size(), L.tie_unmatched, L.shrubs.size(),
                L.shrub_unmatched, L.tex.size());
    return true;
}

namespace {
// The per-instance part shared by the two passes (world_cull.cpp, tie 0x2962d4.. / shrub 0x287234..): the frustum's sphere tests.
struct Cull {
    float tx, ty, kx, ky, near, c23x, c23y;
    // orig: (host) the sphere (x, y, z, r) against the view (vf2 = rotation (sphere - camera); vf1.xy = (z + r, z - r); vf3 = (near - (z +
    // r), (dist - r) - (z - r)); vf7 = t z - (|xy| - k r)): culled when vf3.y < 0, vf3.x >= 0 or vf7 < 0; `zv` the view depth; `clipf`
    // the "needs clipping" test (vf8 = t z - (|xy| + k r) < 0, or z + r - vf23.x >= 0, or z - r - vf23.y < 0: v1 = vf8 | (vf4 ^ the x sign))
    bool visible(const StaticSnapshot& S, const float sph[4], float dist, float& zv, bool& clipf) const {
        const float dx = sph[0] - S.cam[0], dy = sph[1] - S.cam[1], dz = sph[2] - S.cam[2];
        float v[3];
        for (int l = 0; l < 3; ++l) {
            float acc = S.rot[0][static_cast<size_t>(l)] * dx;
            acc = acc + S.rot[1][static_cast<size_t>(l)] * dy;
            v[l] = acc + S.rot[2][static_cast<size_t>(l)] * dz;
        }
        const float r = sph[3];
        zv = v[2];
        const float zr = r + zv, zmr = -r + zv;
        const float d3x = near - zr, d3y = (dist - r) - zmr;
        if (d3y < 0.0f || d3x >= 0.0f) return false;
        const float v7x = tx * zv - (std::fabs(v[0]) - kx * r), v7y = ty * zv - (std::fabs(v[1]) - ky * r);
        if (!(v7x >= 0.0f && v7y >= 0.0f)) return false;
        const float v8x = tx * zv - (std::fabs(v[0]) + kx * r), v8y = ty * zv - (std::fabs(v[1]) + ky * r);
        clipf = std::signbit(v8x) || std::signbit(v8y) || !std::signbit(zr - c23x) || std::signbit(zmr - c23y);
        return true;
    }
};

// orig: (host) the tie pass's second test for an instance across the frustum's edge (0x296404..): the class's 8 box corners (record
// +0xa0..+0x110) through the instance's matrix (rows x scale, (position - camera) x 1024) and the clip matrix 0x166d80, VCLIPW against w:
// culled when all the corners are outside one plane (the AND of the unscaled corners' flags, cfc2 before the 0x160f90 scaling, non-zero;
// the scaled ones only choose tie A / B). `xs` / `ys`: the widening (the 4:3 clip matrix's x / y narrowed by it; the shadow runs kept it 4:3)
bool box_outside(const StaticSnapshot& S, uint32_t rec, uint32_t data, float xs, float ys) {
    float m[4][4];
    const std::array<float, 4> pos = rq(data + 0x30);
    float rows[4][3];
    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t l = 0; l < 3; ++l) rows[r][l] = rf(data + 0x10 * r + 4 * l) * pos[3];
    for (uint32_t l = 0; l < 3; ++l) rows[3][l] = (pos[l] - S.cam[l]) * 1024.0f;
    for (int r = 0; r < 4; ++r)
        for (int l = 0; l < 4; ++l) {
            float acc = S.clip[0][static_cast<size_t>(l)] * rows[r][0];
            acc = acc + S.clip[1][static_cast<size_t>(l)] * rows[r][1];
            acc = acc + S.clip[2][static_cast<size_t>(l)] * rows[r][2];
            m[r][l] = r == 3 ? acc + S.clip[3][static_cast<size_t>(l)] : acc;
        }
    uint32_t all = 0x3f;
    for (uint32_t k = 0; k < 8; ++k) {
        const std::array<float, 4> c = rq(rec + 0xa0 + 0x10 * k);
        float p[4];
        for (int l = 0; l < 4; ++l) {
            float acc = m[0][l] * c[0];
            acc = acc + m[1][l] * c[1];
            acc = acc + m[2][l] * c[2];
            p[l] = acc + m[3][l];
        }
        p[0] = p[0] * xs;
        p[1] = p[1] * ys;
        const float w = std::fabs(p[3]);
        const uint32_t fl = (p[0] > w ? 1u : 0u) | (p[0] < -w ? 2u : 0u) | (p[1] > w ? 4u : 0u) | (p[1] < -w ? 8u : 0u) | (p[2] > w ? 16u : 0u) | (p[2] < -w ? 32u : 0u);
        all &= fl;
    }
    return all != 0;
}

// orig: (host) the instance's rows (tie 0x296888.. / shrub 0x2874dc..): the matrix rows (data +0, +0x10, +0x20) times the scale (+0x3c),
// the position (+0x30) less the camera, the wind's shear when the class sways (0x296a4c / 0x287618), the position x 1024, then
// R_i = A M_i (no translation), T = A P + A3
void instance_rows(const StaticSnapshot& S, uint32_t data, uint32_t sway, bool shrub, float rows[4][4]) {
    float m[3][3];
    const std::array<float, 4> pos = rq(data + 0x30);
    for (uint32_t r = 0; r < 3; ++r)
        for (uint32_t l = 0; l < 3; ++l) m[r][l] = rf(data + 0x10 * r + 4 * l) * pos[3];
    float p[3] = {pos[0] - S.cam[0], pos[1] - S.cam[1], pos[2] - S.cam[2]};
    if (sway != 0) {
        const float* w8 = S.wind.data();
        const float* w9 = S.wind.data() + 4;
        float d2 = p[0] * p[0] + p[1] * p[1];
        d2 = d2 + 1.0f * (p[2] * p[2]);
        if (!(w9[1] - d2 < 0.0f)) {
            const float fade = 1.0f - d2 * w9[2];
            const uint32_t h = shrub ? data : (data >> 9) & 0x1ff;  // (the tie hashes (data >> 9) & 0x1ff (srl / andi after 0x296a4c), the shrub the address itself (no shift after 0x287618))
            const uint32_t v0 = 67 * h + S.frame;
            uint32_t v1 = 123 * h + S.frame;
            if ((sway >> 1) != 0) v1 += S.frame;
            auto sn = [&S](uint32_t i) { return static_cast<float>(static_cast<int32_t>(S.sine[i & 0xff]) << 5) / 4096.0f; };
            float mm = sn(v0) * sn(v0 >> 1);
            mm = mm * w9[0];
            mm = mm + (1.0f - w9[0]);
            mm = mm * w8[0];
            float sx = sn(v1 + 0x40) * w8[1] + mm * w8[2];
            sx = sx * fade;
            float sy = sn(v1 >> 1) * w8[1] + mm * w8[3];
            sy = sy * fade;
            if ((sway >> 1) == 0) {
                sx = sx * w9[3];
                sy = sy * w9[3];
            }
            for (int r = 0; r < 3; ++r) {
                m[r][0] = m[r][0] + sx * m[r][2];
                m[r][1] = m[r][1] + sy * m[r][2];
            }
        }
    }
    for (float& c : p) c = c * 1024.0f;  // (vf24.w: vmulx.w vf24, vf0, vf1 with vf1.x = 0x44800000)
    for (int l = 0; l < 4; ++l) {
        for (int r = 0; r < 3; ++r) {
            float acc = S.a[0][static_cast<size_t>(l)] * m[r][0];
            acc = acc + S.a[1][static_cast<size_t>(l)] * m[r][1];
            rows[r][l] = acc + S.a[2][static_cast<size_t>(l)] * m[r][2];
        }
        float acc = S.a[0][static_cast<size_t>(l)] * p[0];
        acc = acc + S.a[1][static_cast<size_t>(l)] * p[1];
        acc = acc + S.a[2][static_cast<size_t>(l)] * p[2];
        rows[3][l] = acc + S.a[3][static_cast<size_t>(l)];
    }
}
}  // namespace

namespace {
// orig: (host) the shrub far variants' quads as the shrub pass writes them into its chain (0x287c04..0x288010, world_render.md 18.4): the
// header (0x1c2c10), the opaque ones (alpha 0x80) per class, then the fading ones' header (0x1c2be0) and those (alpha & 0x7f) per class;
// before a class's first quad its transfer (0x160b90 and the variant record's +0x10..+0x30: TEX1 / TEX0 / MIPTBP1), per quad 0x160bb0
// (the GIF tag) and its four corners (STQ, RGBA, XYZF2). The corners as 0x287df4..0x287f4c computes them on VU0 (macro mode): d = position
// - camera, n = d / |d|, the basis (n.x, n.y, 0), (n.y, -n.x, 0), (0, 0, 1), d x 1024 through the matrix 0x166cc0; the unit corner's y / z
// scaled by the variant record's +4 / +8 and the instance's +0x2c halfwords / 4096 (z offset by +0xc of it) and the position's w; then
// Q = vf22.x / w, F = clamp(w + vf23.w), X / Y / Z = xyz Q + vf23, FTOI4. The pass's DMA tags / VIF DIRECT codes are left out (the
// walker takes the DIRECT data alone). The list's PS2 capacity (0x300 entries, SPR 0x2000..0x2c00) is not kept.
void variant_stream(const StaticSnapshot& S, const StaticLevel& L, const std::vector<std::vector<std::pair<uint32_t, uint32_t>>>& variants, StaticFrame& f) {
    using namespace vu0m;
    auto& out = f.variant_gif;
    auto f2w = [](float v) { uint32_t u; std::memcpy(&u, &v, 4); return u; };
    auto rw = [](uint32_t a) { return std::array<uint32_t, 4>{ru(a), ru(a + 4), ru(a + 8), ru(a + 12)}; };
    for (uint32_t q = 1; q < 4; ++q) out.push_back(S.vhdr[q]);
    for (const uint32_t sec : {0x80u, 0x7fu}) {
        if (sec == 0x7fu)
            for (uint32_t q = 1; q < 3; ++q) out.push_back(S.vhdr2[q]);
        for (size_t c = 0; c < L.shrubs.size() && c < variants.size(); ++c) {
            if (variants[c].empty()) continue;
            const uint32_t vrec = ru(L.shrubs[c].rec + 0x1c);
            const std::array<float, 4> v16 = rq(vrec);
            bool first = vrec != 0;
            for (const auto& [idx, alpha] : variants[c]) {
                if ((alpha & sec) == 0) continue;
                if (first) {
                    out.push_back(S.vtpl[1]);
                    for (uint32_t q = 1; q < 4; ++q) out.push_back(rw(vrec + 0x10 * q));
                    first = false;
                }
                out.push_back(S.vtpl[3]);
                ++f.variants;
                const uint32_t data = S.shrub_data + 0x40 * idx;
                const std::array<float, 4> pos = rq(data + 0x30);
                const uint32_t col = ru(data + 0x1c) | (alpha << 24), hw = ru(data + 0x2c);
                const std::array<uint32_t, 4> rgba{col & 0xff, (col >> 8) & 0xff, (col >> 16) & 0xff, col >> 24};
                const float d[3] = {sub(pos[0], S.cam[0]), sub(pos[1], S.cam[1]), sub(pos[2], S.cam[2])};
                const float v3x = static_cast<float>(static_cast<int32_t>(hw & 0xffff)) * (1.0f / 4096.0f);
                const float v3y = static_cast<float>(static_cast<int32_t>(hw >> 16)) * (1.0f / 4096.0f);
                const float n2 = madd(add(mul(d[0], d[0]), mul(d[1], d[1])), 1.0f, mul(d[2], d[2]));
                const float q = vrsqrt(1.0f, n2);
                const float v15y = mul(mul(v16[1], v3x), pos[3]), v15z = mul(mul(v16[2], v3y), pos[3]), v15w = mul(mul(v16[3], v3y), pos[3]);
                const float nx = mul(d[0], q), ny = mul(d[1], q);
                Vf m[4];
                for (int r = 0; r < 4; ++r)
                    for (int l = 0; l < 4; ++l) m[r].v[l] = S.vm[static_cast<size_t>(r)][static_cast<size_t>(l)];
                Vf b[4];
                b[0] = transform(m, add(0.0f, nx), add(0.0f, ny), 0.0f, 0.0f);
                b[1] = transform(m, add(0.0f, ny), sub(0.0f, nx), 0.0f, 0.0f);
                b[2] = transform(m, 0.0f, 0.0f, 1.0f, 0.0f);
                b[3] = transform(m, mul(d[0], 1024.0f), mul(d[1], 1024.0f), mul(d[2], 1024.0f), 1.0f);
                for (uint32_t k = 0; k < 4; ++k) {
                    const std::array<float, 4>& cn = S.vcorner[k];
                    std::array<float, 4> st = S.vcorner[4 + k];
                    const float cy = mul(cn[1], v15y), cz = add(mul(cn[2], v15z), v15w);
                    Vf p = transform(b, cn[0], cy, cz, 1.0f);
                    const float qq = div(S.vf22[0], p.v[3]);
                    p.v[3] = vmini(vmax(add(p.v[3], S.vf23[3]), S.fog[3]), S.fog[2]);
                    for (int l = 0; l < 3; ++l) {
                        p.v[l] = add(mul(p.v[l], qq), S.vf23[static_cast<size_t>(l)]);
                        st[static_cast<size_t>(l)] = mul(st[static_cast<size_t>(l)], qq);
                    }
                    out.push_back({f2w(st[0]), f2w(st[1]), f2w(st[2]), f2w(st[3])});
                    out.push_back(rgba);
                    out.push_back({vu0m::ftoi(p.v[0], 16.0f), vu0m::ftoi(p.v[1], 16.0f), vu0m::ftoi(p.v[2], 16.0f), vu0m::ftoi(p.v[3], 16.0f)});
                }
            }
        }
    }
}
}  // namespace

// orig: (host) see mrp_world_static.h
void static_frame_build(const StaticLevel& L, const ResidentColours& rc, StaticFrame& f, bool check) {
    const StaticSnapshot& S = g_snap;
    f.insts.clear();
    f.draws[0].clear();
    f.draws[1].clear();
    f.on[0] = f.on[1] = false;
    f.tested[0] = f.tested[1] = f.visible[0] = f.visible[1] = 0;
    f.variant_gif.clear();
    f.variants = 0;
    if (!L.ok || !S.valid) return;
    f.on[0] = check || S.on[0];
    f.on[1] = check || S.on[1];
    const float s = check ? 1.0f : S.scale, widen = check ? 1.0f : S.widen, widen_y = check || !(widen > 1.0f) ? 1.0f : S.widen_y;
    Cull cu{};
    cu.tx = S.tan[0];
    cu.ty = S.tan[1];
    cu.kx = S.rad[0];
    cu.ky = S.rad[1];
    if (widen > 1.0f) {  // (as mrp_lod.cpp patch_widen: the x tangent widened, its radius factor sqrt(1 + t^2))
        cu.tx = S.tan[0] * widen;
        cu.kx = std::sqrt(1.0f + cu.tx * cu.tx);
    }
    if (widen_y > 1.0f) {  // (the y tangent by the field of view's scale, as patch_widen)
        cu.ty = S.tan[1] * widen_y;
        cu.ky = std::sqrt(1.0f + cu.ty * cu.ty);
    }
    cu.near = S.near[0] * (1.0f / 1024.0f);  // (vf19.xy x 0x3a800000)
    cu.c23x = S.c23[0] * (1.0f / 1024.0f);   // (vf23.xy x 0x3a800000)
    cu.c23y = S.c23[1] * (1.0f / 1024.0f);
    // (the box test's widening; RAC1_MRPW_STATIC_BOX43=1, testing: the 4:3 clip matrix as the shadow runs had it)
    static const bool box43 = std::getenv("RAC1_MRPW_STATIC_BOX43") != nullptr;
    const float boxx = widen > 1.0f && !box43 ? 1.0f / widen : 1.0f, boxy = widen_y > 1.0f && !box43 ? 1.0f / widen_y : 1.0f;
    // ---- ties (0x295fa0): per class record, its instances' bounds (0x20 bytes: sphere, +0x10 the instance data, +0x14 the draw
    // distance, +0x18 the zones' mask, +0x19 the zone), the LOD lists (near / mid: LOD 0, far: LOD 1, beyond: LOD 2), emitted per
    // LOD, per packet, its instances in the list's order (each new one at its head)
    if (f.on[0]) {
        const float tmax = bits_f(S.tie_max);
        for (const StaticTieClass& c : L.ties) {
            if (c.lod[0].empty() && c.lod[1].empty() && c.lod[2].empty()) continue;
            const uint32_t flags = rh(c.rec + 0x24);
            if ((flags & 9) != 0) continue;
            const uint32_t sway = (flags & 6) >> 1;
            const float d16[3] = {scaled(rf(c.rec + 0x10), s), scaled(rf(c.rec + 0x14), s), scaled(rf(c.rec + 0x18), s)};
            const uint32_t bounds = ru(c.rec + 0x28), n = rh(c.rec + 0x26);
            std::vector<uint32_t> lists[3];
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t b = bounds + 0x20 * i;
                ++f.tested[0];
                const uint32_t mask = rb(b + 0x18), cell = rb(b + 0x19);
                if (cell >= 0x80 || (mask & S.zone[cell]) == 0) continue;
                const uint32_t dbits = ru(b + 0x14);
                const float dist = scaled(bits_f(static_cast<int32_t>(dbits) < static_cast<int32_t>(S.tie_max) ? dbits : S.tie_max), s);
                (void)tmax;
                const float sph[4] = {rf(b), rf(b + 4), rf(b + 8), rf(b + 12)};
                float zv = 0;
                bool clipf = false;
                if (!cu.visible(S, sph, dist, zv, clipf)) continue;
                if (clipf && box_outside(S, c.rec, ru(b + 0x10), boxx, boxy)) continue;
                ++f.visible[0];
                const float d = std::max(0.0f, zv);
                StaticInst si;
                int lod = 0;
                si.morph[2] = 256.0f;  // (the near / beyond paths: 0x43800000 in z, prot3w)
                if (!(d16[0] - d >= 0.0f)) {
                    if (d16[2] - d < 0.0f) {
                        lod = 2;
                    } else {
                        float q;
                        if (d16[1] - d >= 0.0f) {
                            q = (d - d16[0]) / (d16[1] - d16[0]);
                        } else {
                            lod = 1;
                            q = (d - d16[1]) / (d16[2] - d16[1]);
                        }
                        si.morph[0] = q;
                        si.morph[3] = 256.0f * q;
                        si.morph[2] = 256.0f - si.morph[3];
                    }
                }
                float fog = S.fog[1] + d * S.fog[0];
                fog = std::min(fog, S.fog[2]);
                fog = std::max(S.fog[3], fog);
                si.fog = (static_cast<uint32_t>(ftoi(fog, 16.0f)) >> 4) & 0xff;
                const uint32_t data = ru(b + 0x10);
                const int64_t slot = rc.tie_slot(data + kTieColourOffset);
                if (slot < 0) continue;
                si.slot = static_cast<uint32_t>(slot);
                instance_rows(S, data, sway, false, si.rows);
                lists[lod].push_back(static_cast<uint32_t>(f.insts.size()));
                f.insts.push_back(si);
            }
            for (int lod = 0; lod < 3; ++lod)
                for (const StaticPacket& pk : c.lod[lod])
                    for (auto it = lists[lod].rbegin(); it != lists[lod].rend(); ++it) f.draws[0].push_back({&pk, *it});
        }
    }
    // ---- shrubs (0x286f10): per class record (+0x14 flags, +0x16 instances, +0x18 their bounds: sphere, +0x10 the draw distance, +0x17 the
    // far variant's switch (units), +0x18 the instance's index), the fade near the draw distance (alpha = min((dist - z) 4096, 0x8000)
    // >> 8); the opaque ones per class, the faded ones after all the classes
    if (f.on[1]) {
        std::vector<std::vector<uint32_t>> faded(L.shrubs.size());
        // (per class its far variants' entries in the pass's order: the instance's index, the variant's alpha; SPR 0x2000 / 0x3600)
        std::vector<std::vector<std::pair<uint32_t, uint32_t>>> variants(L.shrubs.size());
        for (const StaticShrubClass& c : L.shrubs) {
            if (c.packets.empty()) continue;
            const uint32_t flags = rh(c.rec + 0x14);
            if ((flags & 1) != 0) continue;
            const uint32_t sway = (flags & 6) >> 1;
            const bool variant = ru(c.rec + 0x1c) != 0 && s != 0.0f;  // (at the slider's max the far variant is off)
            const uint32_t bounds = ru(c.rec + 0x18), n = rh(c.rec + 0x16);
            std::vector<uint32_t> opaque;
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t b = bounds + 0x20 * i;
                ++f.tested[1];
                const uint32_t dbits = ru(b + 0x10);
                const float dist = scaled(bits_f(static_cast<int32_t>(dbits) < static_cast<int32_t>(S.shrub_max) ? dbits : S.shrub_max), s);
                const float sph[4] = {rf(b), rf(b + 4), rf(b + 8), rf(b + 12)};
                float zv = 0;
                bool clipf = false;
                if (!cu.visible(S, sph, dist, zv, clipf)) continue;
                const uint32_t idx = rh(b + 0x18);
                // the shrub's alpha source: the fade near the draw distance (vf3.y = (dist - r) - (z - r), FTOI12), or with a far variant
                // (0x2873a0..): the switch distance sw (bounds +0x17, units) against z12 = FTOI12(max(0, z)) (vf5.y): nearer than sw the
                // shrub alone (0x8000 + (sw - z) 4096), within 8 units past it both (the variant's alpha (z - sw) 4096 >> 8, the shrub's the
                // rest), beyond it (or sw 0) the variant alone with half the draw distance's fade
                const int32_t fade12 = ftoi(dist - zv, 4096.0f);
                int64_t src = fade12;
                int32_t valpha = -1;
                if (variant) {
                    uint32_t sw = rb(b + 0x17);
                    if (s != 1.0f) {  // (mrp_lod.cpp patch_shrub: the switch scaled, capped at 255 units)
                        const double dd = static_cast<double>(sw) * static_cast<double>(s);
                        sw = static_cast<uint32_t>(dd > 255.0 ? 255.0 : dd);
                    }
                    int64_t a2 = 0x8000;
                    bool alone = false;
                    if (sw != 0) {
                        const int64_t z12 = ftoi(std::max(0.0f, zv), 4096.0f);
                        src = 0x8000 + static_cast<int64_t>(sw) * 4096 - z12;
                        a2 = z12 - static_cast<int64_t>(sw) * 4096;
                        if (a2 < 0) alone = true;
                        else a2 = std::min<int64_t>(a2, 0x8000);
                    }
                    if (!alone) {
                        a2 >>= 8;
                        if (a2 == 0x80) {
                            valpha = static_cast<int32_t>(std::min<int64_t>(fade12 >> 1, 0x8000) >> 8);
                            src = -1;  // (the shrub itself not drawn)
                        } else {
                            valpha = static_cast<int32_t>(a2);
                        }
                    }
                }
                ++f.visible[1];
                if (valpha >= 0) variants[static_cast<size_t>(&c - L.shrubs.data())].push_back({idx, static_cast<uint32_t>(valpha)});
                const int64_t slot = rc.shrub_slot(rc.src.shrub_cols + kShrubColourStride * idx);
                if (slot < 0) continue;
                StaticInst si;
                si.slot = static_cast<uint32_t>(slot);
                instance_rows(S, S.shrub_data + 0x40 * idx, sway, true, si.rows);
                if (src < 0) continue;
                si.alpha = static_cast<uint32_t>(std::min<int64_t>(src, 0x8000)) >> 8;
                const uint32_t at = static_cast<uint32_t>(f.insts.size());
                f.insts.push_back(si);
                if (si.alpha == 0x80) opaque.push_back(at);
                else faded[static_cast<size_t>(&c - L.shrubs.data())].push_back(at);
            }
            for (const StaticPacket& pk : c.packets)
                for (uint32_t at : opaque) f.draws[1].push_back({&pk, at});
        }
        // (the faded ones after every class's opaque ones, per class, per packet: 0x287a04.., the counts at SPR 0x3680)
        for (size_t k = 0; k < L.shrubs.size(); ++k)
            for (const StaticPacket& pk : L.shrubs[k].packets)
                for (uint32_t at : faded[k]) f.draws[1].push_back({&pk, at});
        variant_stream(S, L, variants, f);
    }
}

// orig: (host) see mrp_world_static.h
bool CasterVolume::reaches(const float sph[4]) const {
    if (!casc) return views(sph) != 0;
    for (int k = 0; k < 3; ++k) {
        const float d[3] = {sph[0] - cen[k][0], sph[1] - cen[k][1], sph[2] - cen[k][2]};
        const float t = std::clamp(d[0] * sun[0] + d[1] * sun[1] + d[2] * sun[2], 0.0f, back[k]);
        const float e[3] = {d[0] - sun[0] * t, d[1] - sun[1] * t, d[2] - sun[2] * t};
        const float lim = rad[k] + sph[3];
        if (e[0] * e[0] + e[1] * e[1] + e[2] * e[2] <= lim * lim) return true;
    }
    return false;
}

// orig: (host) see mrp_world_static.h
uint16_t CasterVolume::views(const float sph[4]) const {
    uint16_t m = 0;
    for (int k = 0; k < 3 && casc; ++k) {
        const float d[3] = {sph[0] - cen[k][0], sph[1] - cen[k][1], sph[2] - cen[k][2]};
        const float t = std::clamp(d[0] * sun[0] + d[1] * sun[1] + d[2] * sun[2], 0.0f, back[k]);
        const float e[3] = {d[0] - sun[0] * t, d[1] - sun[1] * t, d[2] - sun[2] * t};
        const float lim = rad[k] + sph[3];
        if (e[0] * e[0] + e[1] * e[1] + e[2] * e[2] <= lim * lim) m |= static_cast<uint16_t>(1u << k);
    }
    for (int i = 0; i < 8; ++i) {
        if (!(pts[i][3] > 0.0f)) continue;
        const float d[3] = {sph[0] - pts[i][0], sph[1] - pts[i][1], sph[2] - pts[i][2]};
        const float lim = pts[i][3] + sph[3];
        if (d[0] * d[0] + d[1] * d[1] + d[2] * d[2] <= lim * lim) m |= static_cast<uint16_t>(1u << (3 + i));
    }
    return m;
}

// orig: (host) see mrp_world_static.h: the cascades' spheres as mrp_lighting.cpp fits them (the view's axes: the rotation's columns;
// the tangents the passes cull with, widened as the view is)
CasterVolume caster_volume(const ShadowCull& c) {
    const StaticSnapshot& S = g_snap;
    CasterVolume v;
    if (!c.on || !S.valid) return v;
    float axis[3][3];
    for (int l = 0; l < 3; ++l)
        for (int r = 0; r < 3; ++r) axis[l][r] = S.rot[static_cast<size_t>(r)][static_cast<size_t>(l)];
    // (the view the shadows are drawn for: the renderer's projection - the cascades are fitted to it - wider than the passes' culling
    // with the field of view scaled; its sides' receivers must find their casters too)
    const float tx = c.tan_x > 0.0f ? std::max(c.tan_x, S.tan[0]) : S.tan[0] * std::max(1.0f, S.widen);
    const float ty = c.tan_y > 0.0f ? std::max(c.tan_y, S.tan[1]) : S.tan[1] * std::max(1.0f, S.widen_y);
    float near_d = 0.1f;
    for (int k = 0; k < 3; ++k) {
        const float far_d = c.splits[k];
        float pts[8][3], m[3] = {0, 0, 0};
        int n = 0;
        for (float d : {near_d, far_d})
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sx = -1; sx <= 1; sx += 2, ++n)
                    for (int l = 0; l < 3; ++l) {
                        pts[n][l] = S.cam[static_cast<size_t>(l)] + axis[0][l] * (sx * d * tx) + axis[1][l] * (sy * d * ty) + axis[2][l] * d;
                        m[l] += pts[n][l] / 8.0f;
                    }
        float r = 0.0f;
        for (const auto& q : pts) r = std::max(r, std::sqrt((q[0] - m[0]) * (q[0] - m[0]) + (q[1] - m[1]) * (q[1] - m[1]) + (q[2] - m[2]) * (q[2] - m[2])));
        for (int l = 0; l < 3; ++l) v.cen[k][l] = m[l];
        v.rad[k] = r * 1.1f * c.grow;
        near_d = far_d * 0.9f;
    }
    for (int l = 0; l < 3; ++l) v.sun[l] = c.sun[l];
    std::memcpy(v.pts, c.pts, sizeof v.pts);
    v.casc = c.cascades;
    for (int k = 0; k < 3; ++k) {  // (the capsule reaches the cut plane, `back` from the camera along the sun)
        float along = 0.0f;
        for (int l = 0; l < 3; ++l) along += (S.cam[static_cast<size_t>(l)] - v.cen[k][l]) * c.sun[l];
        v.back[k] = std::max(0.0f, c.back + along) + v.rad[k];
    }
    v.ok = true;
    return v;
}

// orig: (host) see mrp_world_static.h
void static_shadow_build(const StaticLevel& L, const ResidentColours& rc, StaticFrame& f, const ShadowCull& c) {
    const StaticSnapshot& S = g_snap;
    f.insts.clear();
    f.draws[0].clear();
    f.draws[1].clear();
    f.on[0] = f.on[1] = false;
    f.tested[0] = f.tested[1] = f.visible[0] = f.visible[1] = 0;
    if (!c.on || !L.ok || !S.valid) return;
    f.on[0] = f.on[1] = true;
    const CasterVolume vol = caster_volume(c);
    if (!vol.ok) return;
    // the LOD's distance as the pass computes it (Cull::visible's zv: the view depth, the same float operations), so that an instance
    // the frame draws too gets the same morph: a caster a little off its drawn self (the ground's ties) would shadow it
    auto cam_dist = [&](const float sph[4]) {
        const float dx = sph[0] - S.cam[0], dy = sph[1] - S.cam[1], dz = sph[2] - S.cam[2];
        float acc = S.rot[0][2] * dx;
        acc = acc + S.rot[1][2] * dy;
        const float zv = acc + S.rot[2][2] * dz;
        return std::max(0.0f, zv);
    };
    const float s = S.scale;
    // the frame's own decision (static_frame_build's / the passes': the frustum, the distances, the zones) per kind: what it draws casts
    // as drawn (the frame's packets, the clipping programs' included); only the rest is made here
    auto cull_of = [&](int kind, float& sc, float& boxx) {
        const bool host = S.on[kind];
        sc = host ? S.scale : 1.0f;
        const float widen = host ? S.widen : 1.0f;
        Cull cu{};
        cu.tx = S.tan[0];
        cu.ty = S.tan[1];
        cu.kx = S.rad[0];
        cu.ky = S.rad[1];
        if (widen > 1.0f) {
            cu.tx = S.tan[0] * widen;
            cu.kx = std::sqrt(1.0f + cu.tx * cu.tx);
            if (S.widen_y > 1.0f) {
                cu.ty = S.tan[1] * S.widen_y;
                cu.ky = std::sqrt(1.0f + cu.ty * cu.ty);
            }
        }
        cu.near = S.near[0] * (1.0f / 1024.0f);
        cu.c23x = S.c23[0] * (1.0f / 1024.0f);
        cu.c23y = S.c23[1] * (1.0f / 1024.0f);
        boxx = widen > 1.0f ? 1.0f / widen : 1.0f;
        return cu;
    };
    float tie_s = 1.0f, tie_box = 1.0f, shrub_s = 1.0f, shrub_box = 1.0f;
    const Cull tie_cu = cull_of(0, tie_s, tie_box);
    (void)shrub_s;
    (void)shrub_box;
    static const int kinds = std::getenv("RAC1_SHADOW_CAST_KINDS") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_CAST_KINDS")) : 3;  // (testing: 1 ties, 2 shrubs)
    static const bool trace = std::getenv("RAC1_SHADOW_CAST_TRACE") != nullptr;
    for (const StaticTieClass& tc : L.ties) {
        if ((kinds & 1) == 0) break;
        if (tc.lod[0].empty() && tc.lod[1].empty() && tc.lod[2].empty()) continue;
        const uint32_t flags = rh(tc.rec + 0x24);
        if ((flags & 9) != 0) continue;
        const uint32_t sway = (flags & 6) >> 1;
        const float d16[3] = {scaled(rf(tc.rec + 0x10), s), scaled(rf(tc.rec + 0x14), s), scaled(rf(tc.rec + 0x18), s)};
        const uint32_t bounds = ru(tc.rec + 0x28), n = rh(tc.rec + 0x26);
        std::vector<uint32_t> lists[3];
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t b = bounds + 0x20 * i;
            ++f.tested[0];
            static const bool zones = std::getenv("RAC1_SHADOW_ZONES") != nullptr && std::atoi(std::getenv("RAC1_SHADOW_ZONES")) != 0;  // (testing)
            const uint32_t mask = rb(b + 0x18), cell = rb(b + 0x19);
            if (zones && (cell >= 0x80 || (mask & S.zone[cell]) == 0)) continue;
            const float sph[4] = {rf(b), rf(b + 4), rf(b + 8), rf(b + 12)};
            uint16_t views = vol.views(sph);
            if (views == 0) continue;
            bool drawn = false;
            {  // (drawn by the frame: its own packets cast into the cascades)
                const uint32_t dbits = ru(b + 0x14);
                const float dist = scaled(bits_f(static_cast<int32_t>(dbits) < static_cast<int32_t>(S.tie_max) ? dbits : S.tie_max), tie_s);
                float zv = 0;
                bool clipf = false;
                // (only the clipping-free draws: an instance across the frustum's edge is drawn by tie B, its triangles clipped to the view -
                // its shadow cut at the screen's edge; the renderer keeps those out of the shadow maps and this whole one casts instead)
                drawn = tie_cu.visible(S, sph, dist, zv, clipf) && !clipf;
            }
            if (drawn) views &= static_cast<uint16_t>(~7u);  // (the point lights' cubes take every instance near them: the frame's
                                                             // draws are not drawn there)
            if (views == 0) continue;
            ++f.visible[0];
            // (the LOD of one not drawn by the frame by its distance - no drawn self to match: a far one behind the camera
            // takes its coarse model, not the finest the view depth's 0 would give; one drawn, the frame's own LOD)
            const float d = drawn ? cam_dist(sph)
                                  : std::max(cam_dist(sph), std::sqrt((sph[0] - S.cam[0]) * (sph[0] - S.cam[0]) + (sph[1] - S.cam[1]) * (sph[1] - S.cam[1]) +
                                                                      (sph[2] - S.cam[2]) * (sph[2] - S.cam[2])) - sph[3]);
            if (trace && sph[3] > 20.0f) std::printf("[shadow-cast] tie class %08x instance %u sphere %.1f %.1f %.1f r %.1f cam dist %.1f\n", tc.rec, i, sph[0], sph[1], sph[2], sph[3], d);
            StaticInst si;
            int lod = 0;
            si.morph[2] = 256.0f;
            if (!(d16[0] - d >= 0.0f)) {
                if (d16[2] - d < 0.0f) {
                    lod = 2;
                } else {
                    float q;
                    if (d16[1] - d >= 0.0f) {
                        q = (d - d16[0]) / (d16[1] - d16[0]);
                    } else {
                        lod = 1;
                        q = (d - d16[1]) / (d16[2] - d16[1]);
                    }
                    si.morph[0] = q;
                    si.morph[3] = 256.0f * q;
                    si.morph[2] = 256.0f - si.morph[3];
                }
            }
            const uint32_t data = ru(b + 0x10);
            const int64_t slot = rc.tie_slot(data + kTieColourOffset);
            if (slot < 0) continue;
            si.slot = static_cast<uint32_t>(slot);
            si.mask = views;
            instance_rows(S, data, sway, false, si.rows);
            lists[lod].push_back(static_cast<uint32_t>(f.insts.size()));
            f.insts.push_back(si);
        }
        for (int lod = 0; lod < 3; ++lod)
            for (const StaticPacket& pk : tc.lod[lod])
                for (uint32_t at : lists[lod]) f.draws[0].push_back({&pk, at});
    }
    for (const StaticShrubClass& sc : L.shrubs) {
        if ((kinds & 2) == 0) break;
        if (sc.packets.empty()) continue;
        const uint32_t flags = rh(sc.rec + 0x14);
        if ((flags & 1) != 0) continue;
        const uint32_t sway = (flags & 6) >> 1;
        const uint32_t bounds = ru(sc.rec + 0x18), n = rh(sc.rec + 0x16);
        std::vector<uint32_t> list;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t b = bounds + 0x20 * i;
            ++f.tested[1];
            const float sph[4] = {rf(b), rf(b + 4), rf(b + 8), rf(b + 12)};
            const uint16_t views = vol.views(sph);
            if (views == 0) continue;
            // (every shrub in the volume, the frame's too: the renderer keeps the frame's shrubs out of the shadow maps - their far
            // variants are camera-facing quads, the faded ones write no depth: their shadows came and went with the camera)
            ++f.visible[1];
            const uint32_t idx = rh(b + 0x18);
            const int64_t slot = rc.shrub_slot(rc.src.shrub_cols + kShrubColourStride * idx);
            if (slot < 0) continue;
            StaticInst si;
            si.slot = static_cast<uint32_t>(slot);
            si.mask = views;
            instance_rows(S, S.shrub_data + 0x40 * idx, sway, true, si.rows);
            list.push_back(static_cast<uint32_t>(f.insts.size()));
            f.insts.push_back(si);
        }
        for (const StaticPacket& pk : sc.packets)
            for (uint32_t at : list) f.draws[1].push_back({&pk, at});
    }
    // (by their views, so that the renderer's batches - one per views - stay few)
    for (int k = 0; k < 2; ++k)
        std::stable_sort(f.draws[k].begin(), f.draws[k].end(), [&f](const StaticDraw& a, const StaticDraw& b) { return f.insts[a.inst].mask < f.insts[b.inst].mask; });
}

}  // namespace rac1::mrpw
