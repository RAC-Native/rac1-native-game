// orig: (host) The MRP world's terrain on the GPU (see mrp_tfrag_gpu.h).
#include <game/render/mrp_tfrag_gpu.h>
#include <profiler/host_profiler.h>
#include <game/render/mrp_world_static.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/vu0_macro.h>
#include <game/render/mrp_world_assets.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>
#define GLFW_INCLUDE_NONE
#include <GLFW/glfw3.h>

#include <algorithm>
#include <atomic>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <set>
#include <unordered_map>

namespace rac1::mrpw {

namespace {

float f_of(uint32_t u) {
    float f;
    std::memcpy(&f, &u, 4);
    return f;
}
uint32_t u_of(float f) {
    uint32_t u;
    std::memcpy(&u, &f, 4);
    return u;
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

// an image's qword lane (the header's fields are V4_16 unsigned: the low 16 bits of the lane's word)
uint32_t lane(const std::vector<uint8_t>& img, uint32_t q, int l) {
    if (16u * q + 16u > img.size()) return 0;
    uint32_t v;
    std::memcpy(&v, img.data() + 16u * q + 4u * static_cast<uint32_t>(l), 4);
    return v;
}
uint32_t u16l(const std::vector<uint8_t>& img, uint32_t q, int l) { return lane(img, q, l) & 0xffff; }
int32_t i16l(const std::vector<uint8_t>& img, uint32_t q, int l) { return static_cast<int16_t>(lane(img, q, l) & 0xffff); }

// orig: (host) the emission's static walk (world_mrp.cpp tf_emit, micro 0x45d / 0x382 / 0x51d / 0x652 and their strip change 0x332 /
// 0x5fd): per sent vertex its vertex info, its strip position, whether its clip flags count; the runs of strips between two texture
// switches (the GS state changes only there and at the strips' tags, whose PRIM is the same constant)
bool build_emission(const std::vector<uint8_t>& img, TfragPlan& p) {
    const uint32_t nq = static_cast<uint32_t>(img.size() / 16);
    if (nq < 5) return false;
    const uint32_t tex = u16l(img, 4, 3), strips = u16l(img, 4, 2), index = u16l(img, 3, 1);
    uint32_t si = strips;
    if (si >= nq) return false;
    int32_t n = i16l(img, si, 0) + 0x80;  // (the first strip always switches the texture)
    auto new_run = [&p](int32_t t) {
        p.events.push_back({0, static_cast<uint16_t>(t)});
        p.runs.push_back({p.ntris, 0});
    };
    const int32_t t0 = i16l(img, si, 2);
    if (t0 < 0) return false;
    new_run(static_cast<int32_t>(tex) + t0);
    ++si;
    uint32_t ix = 0;
    bool strip_first = false;
    for (int guard = 0; guard < 100000; ++guard) {
        if (n <= 0 || n > 0x400) return false;
        for (int32_t v = 0; v < n; ++v, ++ix) {
            if (index + ix / 4 >= nq) return false;
            const uint32_t via = u16l(img, index + ix / 4, static_cast<int>(ix % 4));
            if (via + 1 > nq) return false;
            TfragPlanVertex pv;
            pv.via = static_cast<uint16_t>(via);
            pv.pos = static_cast<uint8_t>(std::min<int32_t>(v, 2));
            pv.counts = !strip_first;
            strip_first = false;
            pv.run = static_cast<uint16_t>(p.runs.size() - 1);
            p.verts.push_back(pv);
            if (v >= 2) {
                ++p.ntris;
                ++p.runs.back().ntri;
            }
        }
        strip_first = true;
        if (si >= nq) return false;
        const int32_t nx = i16l(img, si, 0);
        if (nx > 0) {
            ++si;
            n = nx;
            continue;
        }
        const int32_t eop = i16l(img, si, 1), tz = i16l(img, si, 2);
        ++si;
        if (nx == 0) return p.verts.size() < 0x10000;
        n = nx + 0x80;
        if (eop < 0 && tz < 0) continue;  // (a new kick without a texture switch: the state goes on)
        if (tz < 0) return false;
        new_run(static_cast<int32_t>(tex) + tz);
    }
    return false;
}

// orig: (host) the steps of a start (world_mrp.cpp tfrag_kick: micro 0x008 / 0x00a / 0x00e / 0x010; the far strips and B: none), and
// whether a step's iterations depend on each other (a later iteration reading what an earlier one wrote, or the reverse): then run in
// order. The vertex infos are the image's (no step before a morph changes them).
void build_steps(const std::vector<uint8_t>& img, uint32_t code, TfragPlan& p, uint32_t& serial) {
    auto H = [&img](int q, int l) { return u16l(img, static_cast<uint32_t>(q), l); };
    const uint32_t common = H(0, 0), lod01 = H(0, 2), lod0 = H(1, 0), recs = H(1, 2);
    auto T = [&](uint32_t n) { p.steps.push_back({0, false, static_cast<uint16_t>(recs), 0, static_cast<uint16_t>(n), 0}); };
    auto M = [&](uint32_t vi, uint32_t pi, uint32_t n, uint32_t band, bool subst) {
        TfragStep s{static_cast<uint8_t>(subst ? 2 : 1), false, static_cast<uint16_t>(vi), static_cast<uint16_t>(pi), static_cast<uint16_t>(n), static_cast<uint16_t>(band)};
        // writes: each vertex's record (its vertex info's w) and, with substitutions, the vertex info; reads: the parents' records and
        // the parent vertex info
        std::set<uint32_t> written, vias;
        for (uint32_t k = 0; k < n; ++k) {
            written.insert(u16l(img, vi + k, 3));
            written.insert(u16l(img, vi + k, 3) + 1);
            vias.insert(vi + k);
        }
        for (uint32_t k = 0; k < n && !s.serial; ++k) {
            const uint32_t pvi = lane(img, pi + k / 4, static_cast<int>(k % 4)) & 0xffff;
            const uint32_t r1 = u16l(img, pvi, 3), r2 = u16l(img, vi + k, 2);
            if (vias.count(pvi) != 0 || written.count(r1) != 0 || written.count(r2) != 0 || written.count(r1 + 1) != 0 || written.count(r2 + 1) != 0) s.serial = true;
        }
        if (written.size() != 2u * n) s.serial = true;
        if (s.serial) ++serial;
        p.steps.push_back(s);
    };
    auto F = [&](uint32_t list, uint32_t idx2, uint32_t n, uint32_t band) {
        TfragStep s{3, false, static_cast<uint16_t>(list), static_cast<uint16_t>(idx2), static_cast<uint16_t>(n), static_cast<uint16_t>(band)};
        for (uint32_t j = 0; j < n && !s.serial; ++j) {
            const uint32_t vi = lane(img, idx2 + j / 4, static_cast<int>(j % 4)) & 0xffff;
            if (vi >= list && vi < list + n) s.serial = true;
        }
        if (s.serial) ++serial;
        p.steps.push_back(s);
    };
    switch (code) {
    case 0x008:
        T(common);
        M(H(2, 1), H(3, 2), lod01, 0x29a, true);
        F(H(2, 2), H(3, 3), H(0, 3), 0x29a);
        p.form = 0;
        break;
    case 0x00a:
        T(common);
        M(H(2, 1), H(3, 2), lod01, 0x29a, false);
        p.form = 0;
        break;
    case 0x00e:
        T(common);
        M(H(2, 1), H(3, 2), lod01, 0x29a, false);
        M(H(2, 3), H(4, 0), lod0, 0x29b, true);
        F(H(3, 0), H(4, 1), H(1, 1), 0x29b);
        p.form = 0;
        break;
    case 0x010:
        T(common + lod01);
        M(H(2, 3), H(4, 0), lod0, 0x29b, false);
        p.form = 0;
        break;
    case 0x006: case 0x014: p.form = 3; break;
    default: p.form = 4; break;  // (tfrag B's 0x002)
    }
}

// The plan's GPU words: [0] steps | form << 8, [1] vertices, [2] triangles; per step 3 words (kind | serial << 8 | band << 16, a | b << 16,
// n); per vertex 2 words (its vertex info | strip position << 16 | counts << 18, its run | its triangle << 16 when it closes one)
constexpr uint32_t kMaxPlanVerts = 8192;
void build_plan(const std::vector<uint8_t>& img, uint32_t code, TfragPlan& p, TfragLevel& L) {
    p = TfragPlan{};
    build_steps(img, code, p, L.serial_steps);
    p.ok = build_emission(img, p) && p.verts.size() <= kMaxPlanVerts && p.runs.size() < 0x10000;
    if (!p.ok) {
        ++L.plan_fail;
        return;
    }
    ++L.plans;
    p.gpu_first = static_cast<uint32_t>(L.plan_words.size());
    std::vector<uint32_t>& w = L.plan_words;
    w.push_back(static_cast<uint32_t>(p.steps.size()) | static_cast<uint32_t>(p.form) << 8);
    w.push_back(static_cast<uint32_t>(p.verts.size()));
    w.push_back(p.ntris);
    for (const TfragStep& s : p.steps) {
        w.push_back(s.kind | (s.serial ? 0x100u : 0u) | static_cast<uint32_t>(s.band) << 16);
        w.push_back(s.a | static_cast<uint32_t>(s.b) << 16);
        w.push_back(s.n);
    }
    uint32_t tri = 0;
    for (const TfragPlanVertex& v : p.verts) {
        w.push_back(v.via | static_cast<uint32_t>(v.pos) << 16 | (v.counts ? 1u << 18 : 0u));
        w.push_back(v.run | (v.pos >= 2 ? tri++ : 0u) << 16);
    }
}

}  // namespace

// orig: (host) see mrp_tfrag_gpu.h
const TfragPlan* TfragEntryGpu::plan(int lod, uint32_t code, bool b) const {
    const TfragPlan* p = nullptr;
    if (b) p = lod == 2 ? &nearb : nullptr;
    else if (lod == 0 && (code == 0x006 || code == 0x00c)) p = &far6;
    else if (lod == 1 && code == 0x008) p = &mid8;
    else if (lod == 1 && code == 0x00a) p = &mida;
    else if (lod == 2 && code == 0x00e) p = &neare;
    else if (lod == 2 && code == 0x010) p = &near10;
    else if (lod == 2 && code == 0x014) p = &near14;
    return p != nullptr && p->ok ? p : nullptr;
}

// orig: (host) see mrp_tfrag_gpu.h. The entry table *(0x160f4c) (0x40 bytes each, *(0x160f50) of them: world_cull.cpp 0x294368's t8 /
// t9); an entry's data at *(entry + 0x10) (t3), its lit colours at data + *(u16)(entry + 0x1e) (the chain's colour REF, 0x29477c..),
// the near image's colour count (entry +0x26 >> 2) x 4
bool tfrag_level_build(TfragLevel& L, const World& w) {
    static uint64_t generations = 0;
    L = TfragLevel{};
    L.generation = ++generations;  // (the same object is rebuilt at every level change)
    L.table = ru(OV(0x00160f4cu));
    const int32_t n = static_cast<int32_t>(ru(OV(0x00160f50u)));
    L.count = n > 0 && n < 0x10000 && L.table != 0 ? static_cast<uint32_t>(n) : 0;
    // assets with no tfrag: the scene is not the EE's level - the loading screen's flight (an empty world) -
    // and the table is the level being left or loaded (its entries' data freed or half written): none of it is this scene's
    if (w.tfrags.empty()) {
        L.table = 0;
        L.count = 0;
    }
    L.entries.resize(w.tfrags.size());
    L.by_ee.assign(L.count, -1);
    for (size_t i = 0; i < w.tfrags.size(); ++i) {
        const TfragEntry& te = w.tfrags[i];
        TfragEntryGpu& e = L.entries[i];
        if (te.index >= L.count) {
            ++L.mismatched;
            continue;
        }
        e.ee = L.table + 0x40 * te.index;
        // the entry as EE memory holds it against the file's (the sphere, the offsets and counts; +0x10 is the data's address there)
        bool same = true;
        for (uint32_t o = 0; o < 0x40 && same; o += 4) {
            if (o == 0x10 || o == 0x30 || o == 0x34) continue;  // (the data pointer; the lighting's dirty byte / point-light nibbles)
            uint32_t h;
            std::memcpy(&h, te.header.data() + o, 4);
            const uint32_t mask = o == 0x38 ? 0xffffu : 0xffffffffu;  // (+0x3a / +0x3b: the zone's mask and cell, set at the load)
            same = (ru(e.ee + o) & mask) == (h & mask);
        }
        const uint32_t data = ru(e.ee + 0x10);
        if (!same && L.mismatched < 3) {
            std::printf("[mrpw tfrag] entry %u at %08x differs from the asset's:", te.index, e.ee);
            for (uint32_t o = 0; o < 0x40; o += 4) {
                uint32_t h;
                std::memcpy(&h, te.header.data() + o, 4);
                if (o != 0x10 && ru(e.ee + o) != h) std::printf(" +%02x %08x/%08x", o, ru(e.ee + o), h);
            }
            std::printf("\n");
        }
        if (!same || data == 0 || data >= 0x02000000u) {
            ++L.mismatched;
            continue;
        }
        // the images from the data EE memory holds (the texture prims as the level load rewrote them)
        TfragEntry copy;
        copy.data.resize(te.data.size());
        for (size_t b = 0; b < copy.data.size(); ++b) copy.data[b] = static_cast<uint8_t>(rb(data + static_cast<uint32_t>(b)));
        for (int l = 0; l < 3; ++l) {
            TfragImage im;
            im.refs = te.lods[static_cast<size_t>(l)].refs;
            tfrag_image(copy, l, im);
            e.image[static_cast<size_t>(l)] = std::move(im.image);
            e.colour_addr[static_cast<size_t>(l)] = te.lods[static_cast<size_t>(l)].colour_addr;
            e.colour_count[static_cast<size_t>(l)] = te.lods[static_cast<size_t>(l)].colour_count;
        }
        e.colour_ee = data + rh(e.ee + 0x1e);
        e.colour_n = std::max<uint32_t>({e.colour_count[0], e.colour_count[1], e.colour_count[2]});
        const auto& im0 = e.image[0];
        const auto& im1 = e.image[1];
        const auto& im2 = e.image[2];
        build_plan(im0, 0x006, e.far6, L);
        build_plan(im1, 0x008, e.mid8, L);
        build_plan(im1, 0x00a, e.mida, L);
        build_plan(im2, 0x00e, e.neare, L);
        build_plan(im2, 0x010, e.near10, L);
        build_plan(im2, 0x014, e.near14, L);
        build_plan(im2, 0x002, e.nearb, L);
        for (int l = 0; l < 3; ++l) {
            const auto& im = e.image[static_cast<size_t>(l)];
            e.gpu_image[static_cast<size_t>(l)] = static_cast<uint32_t>(L.images.size());
            for (size_t q = 0; q + 16 <= im.size(); q += 16) {
                std::array<uint32_t, 4> v;
                std::memcpy(v.data(), im.data() + q, 16);
                L.images.push_back(v);
            }
        }
        e.colour_slot = L.colour_words;
        L.colour_words += e.colour_n;
        e.ok = true;
        L.by_ee[te.index] = static_cast<int32_t>(i);
    }
    L.ok = true;
    std::printf("[mrpw tfrag] level entries %u (EE table %u), not matched %u; plans %u (%u failed), steps run in order %u\n", static_cast<uint32_t>(L.entries.size()), L.count,
                L.mismatched, L.plans, L.plan_fail, L.serial_steps);
    return true;
}

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_colours_update(TfragColours& c, const TfragLevel& L) {
    ++c.updates;
    bool all = c.all_dirty;
    if (c.level != &L || c.mirror.size() != L.colour_words) {
        c.level = &L;
        c.mirror.assign(L.colour_words, 0);
        all = true;
    }
    if (all) {
        c.dirty.clear();
        c.dirty_words.clear();
    }
    for (const TfragEntryGpu& e : L.entries) {
        if (!e.ok || e.colour_n == 0) continue;
        const void* p = ee_ram_ptr_read(e.colour_ee);
        if (p == nullptr) continue;
        uint32_t* m = c.mirror.data() + e.colour_slot;
        if (!all && std::memcmp(m, p, 4 * e.colour_n) == 0) continue;
        std::memcpy(m, p, 4 * e.colour_n);
        ++c.entries_changed;
        if (all) continue;
        if (!c.dirty.empty() && c.dirty.back().first + c.dirty.back().second == e.colour_slot) c.dirty.back().second += e.colour_n;
        else c.dirty.push_back({e.colour_slot, e.colour_n});
        c.dirty_words.insert(c.dirty_words.end(), m, m + e.colour_n);
    }
    c.all_dirty = all;
}

// ---- the CPU form of the GPU program ----------------------------------------------------------------------------------------

namespace {

// The program's memory as the GPU program keeps it: the image (image-relative qwords), the constants 0x290..0x29d.
struct Mem {
    std::vector<std::array<uint32_t, 4>> q;
    std::array<std::array<uint32_t, 4>, 14> k{};
    std::array<uint32_t, 4> zero{};
    std::array<uint32_t, 4>& at(uint32_t a) {
        if (a < q.size()) return q[a];
        zero = {};
        return zero;
    }
    float f(uint32_t a, int l) { return f_of(at(a)[static_cast<size_t>(l)]); }
    float kf(uint32_t c, int l) const { return f_of(k[c - 0x290][static_cast<size_t>(l)]); }
    uint32_t u16(uint32_t a, int l) { return at(a)[static_cast<size_t>(l)] & 0xffff; }
};
struct V4 {
    float v[4] = {};
};
V4 ld(Mem& m, uint32_t a) {
    V4 r;
    for (int l = 0; l < 4; ++l) r.v[l] = m.f(a, l);
    return r;
}
V4 itof0(Mem& m, uint32_t a) {
    V4 r;
    for (int l = 0; l < 4; ++l) r.v[l] = static_cast<float>(static_cast<int32_t>(m.at(a)[static_cast<size_t>(l)]));
    return r;
}
void st(Mem& m, uint32_t a, const V4& v) {
    for (int l = 0; l < 4; ++l) m.at(a)[static_cast<size_t>(l)] = u_of(v.v[l]);
}
V4 xform(Mem& m, const V4& p) {
    V4 o;
    for (int l = 0; l < 4; ++l) {
        float a = m.f(8, l);
        a = a + m.f(5, l) * p.v[0];
        a = a + m.f(6, l) * p.v[1];
        o.v[l] = a + m.f(7, l) * p.v[2];
    }
    return o;
}
float fmin_(float a, float b) { return b < a ? b : a; }  // (std::min(a, b))
float fmax_(float a, float b) { return a < b ? b : a; }  // (std::max(a, b))
int32_t ftoi(float v) {
    if (std::isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0f)) return 0x7fffffff;
    if (!(v > -2147483648.0f)) return static_cast<int32_t>(0x80000000u);
    return static_cast<int32_t>(v);
}

void run_step(Mem& m, const TfragStep& s) {
    switch (s.kind) {
    case 0:  // T (micro 0x04f / 0x052)
        for (uint32_t k = 0; k < s.n; ++k) {
            const uint32_t a = s.a + 2 * k;
            const V4 p = xform(m, itof0(m, a));
            const V4 c = itof0(m, a + 1);
            st(m, a, p);
            st(m, a + 1, c);
        }
        break;
    case 1: case 2: {  // M / MS (micro 0x093 / 0x08a / 0x156 / 0x160)
        const float kx = m.kf(s.band, 0), ky = m.kf(s.band, 1), cx = m.kf(s.band + 2, 0), cy = m.kf(s.band + 2, 1), thr = m.kf(s.band, 3);
        const float mx = m.kf(0x291, 0), my = m.kf(0x291, 1);
        for (uint32_t k = 0; k < s.n; ++k) {
            const uint32_t via = s.a + k;
            const uint32_t rec = m.u16(via, 3);
            const uint32_t pvi = m.at(s.b + k / 4)[k % 4] & 0xffff;
            const uint32_t r1 = m.u16(pvi, 3), r2 = m.u16(via, 2);
            const V4 p = xform(m, itof0(m, rec));
            const V4 c = itof0(m, rec + 1);
            const V4 P1 = ld(m, r1), C1 = ld(m, r1 + 1), P2 = ld(m, r2), C2 = ld(m, r2 + 1);
            float fx = cx * 1.0f + kx * p.v[3], fy = cy * 1.0f + ky * p.v[3];
            fx = fmax_(fmin_(fx, mx), 0.0f);
            fy = fmax_(fmin_(fy, my), 0.0f);
            V4 po, co;
            for (int l = 0; l < 4; ++l) {
                const float S = P1.v[l] + P2.v[l], SC = C1.v[l] + C2.v[l];
                po.v[l] = S * fx + p.v[l] * fy;
                co.v[l] = SC * fx + c.v[l] * fy;
            }
            if (s.kind == 2 && (P1.v[3] - thr) < 0.0f && (P2.v[3] - thr) < 0.0f) {
                m.at(via) = m.at(pvi);
                continue;
            }
            st(m, rec, po);
            st(m, rec + 1, co);
        }
        break;
    }
    default: {  // F (micro 0x278 / 0x27d)
        const float thr = m.kf(s.band, 3);
        for (uint32_t j = 0; j < s.n; ++j) {
            const uint32_t la = s.a + j;
            const uint32_t vi = m.at(s.b + j / 4)[j % 4] & 0xffff;
            const uint32_t ra = m.u16(la, 2), rb2 = m.u16(vi, 3);
            if ((m.f(ra, 3) - thr) < 0.0f && (m.f(rb2, 3) - thr) < 0.0f) m.at(la) = m.at(vi);
        }
        break;
    }
    }
}

}  // namespace

// orig: (host) see mrp_tfrag_gpu.h (the steps in their order; the emission per sent vertex as world_mrp.cpp tf_emit)
bool tfrag_exec_cpu(const TfragEntryGpu& e, const TfragPlan& p, int lod, const uint32_t* colours, const float view[16], const uint32_t consts[4 * 14],
                    std::vector<TfragCpuVertex>& out, std::vector<uint8_t>* adc_out) {
    out.clear();
    const std::vector<uint8_t>& img = e.image[static_cast<size_t>(lod)];
    Mem m;
    m.q.resize(img.size() / 16);
    std::memcpy(m.q.data(), img.data(), m.q.size() * 16);
    for (uint32_t r = 0; r < 4; ++r)
        for (int l = 0; l < 4; ++l) m.at(5 + r)[static_cast<size_t>(l)] = u_of(view[4 * r + static_cast<uint32_t>(l)]);
    for (uint32_t c = 0; c < 14; ++c)
        for (int l = 0; l < 4; ++l) m.k[c][static_cast<size_t>(l)] = consts[4 * c + static_cast<uint32_t>(l)];
    const uint32_t ca = e.colour_addr[static_cast<size_t>(lod)], cn = e.colour_count[static_cast<size_t>(lod)];
    for (uint32_t k = 0; k < cn; ++k) {
        const uint32_t w = colours != nullptr ? colours[k] : 0;
        m.at(ca + 2 * k) = {w & 0xff, (w >> 8) & 0xff, (w >> 16) & 0xff, w >> 24};
    }
    for (const TfragStep& s : p.steps) run_step(m, s);
    // the emission (tf_emit)
    const uint32_t tex = m.u16(4, 3);
    int form = p.form;
    const bool bform = form == 4;
    if (bform) form = 3;
    const float stoff = (form == 1 || bform) ? m.f(tex + 1, 3) : m.kf(0x291, 2);
    float c0[4], c4[4], c5[4], c6[4];
    for (int l = 0; l < 4; ++l) {
        c0[l] = m.kf(0x290, l);
        c4[l] = m.kf(0x298, l);
        c5[l] = m.kf(0x295, l);
        c6[l] = m.kf(0x296, l);
    }
    const size_t nv = p.verts.size();
    std::vector<V4> P(nv);
    std::vector<uint8_t> f6(nv, 0);
    for (size_t j = 0; j < nv; ++j) {  // phase 1: the positions and the guard band's flags
        const uint32_t rec = m.u16(p.verts[j].via, 3);
        P[j] = form <= 1 ? ld(m, rec) : xform(m, itof0(m, rec));
        if (form == 1 || form == 3) {
            V4 g;
            for (int l = 0; l < 4; ++l) g.v[l] = P[j].v[l] * c6[l];
            const float w = std::fabs(g.v[3]);
            uint8_t f = 0;
            if (g.v[0] > w) f |= 1;
            if (g.v[0] < -w) f |= 2;
            if (g.v[1] > w) f |= 4;
            if (g.v[1] < -w) f |= 8;
            if (g.v[2] > w) f |= 16;
            if (g.v[2] < -w) f |= 32;
            f6[j] = f;
        }
    }
    if (adc_out != nullptr) adc_out->assign(nv, 0);
    out.resize(nv);
    for (size_t j = 0; j < nv; ++j) {  // phase 2: the vertex
        const TfragPlanVertex& pv = p.verts[j];
        const uint32_t via = pv.via, rec = m.u16(via, 3);
        V4 Pj = P[j];
        TfragCpuVertex& o = out[j];
        o.hp = {Pj.v[0], Pj.v[1], Pj.v[2], Pj.v[3]};
        float stq[3] = {m.f(via, 0) - stoff, m.f(via, 1) - stoff, (1.0f + stoff) - stoff};
        bool adc = false;
        if ((form == 1 || form == 3) && !bform) {
            uint32_t h = pv.counts ? f6[j] : 0;
            if (pv.pos >= 1 && p.verts[j - 1].counts) h |= f6[j - 1];
            if (pv.pos >= 2 && p.verts[j - 2].counts) h |= f6[j - 2];
            adc = h != 0;
        }
        const float q = c0[0] / Pj.v[3];
        for (int l = 0; l < 3; ++l) {
            Pj.v[l] = Pj.v[l] * q;
            stq[l] = stq[l] * q;
        }
        for (int l = 0; l < 4; ++l) Pj.v[l] = Pj.v[l] + c5[l];
        if (form == 3) {
            Pj.v[3] = fmin_(Pj.v[3], c0[2]);
            Pj.v[3] = fmax_(Pj.v[3], c0[1]);
        } else {
            Pj.v[3] = fmax_(Pj.v[3], c0[1]);
            Pj.v[3] = fmin_(Pj.v[3], c0[2]);
        }
        if (adc) Pj.v[3] = Pj.v[3] + c0[3];
        if (adc_out != nullptr) (*adc_out)[j] = adc ? 1 : 0;
        o.stq = {u_of(stq[0]), u_of(stq[1]), u_of(stq[2]), 0};
        if (form <= 1) {
            const V4 C = ld(m, rec + 1);
            for (int l = 0; l < 4; ++l) o.rgba[static_cast<size_t>(l)] = static_cast<uint32_t>(ftoi(C.v[l] * c4[l]));
        } else {
            o.rgba = m.at(rec + 1);
        }
        for (int l = 0; l < 4; ++l) o.xyzf[static_cast<size_t>(l)] = static_cast<uint32_t>(ftoi(Pj.v[l] * 16.0f));
    }
    return true;
}

// orig: (host) see mrp_tfrag_gpu.h
uint32_t tfrag_selfcheck(const TfragEntryGpu& e, const TfragPlan& p, int lod, uint32_t code, bool b, const uint32_t* colours, const float view[16],
                         const uint32_t consts[4 * 14], uint64_t& compared, const uint8_t* vu_mem, uint32_t base) {
    std::vector<TfragCpuVertex> mine;
    tfrag_exec_cpu(e, p, lod, colours, view, consts, mine);
    std::vector<uint8_t> mem(0x4000, 0);
    const std::vector<uint8_t>& img = e.image[static_cast<size_t>(lod)];
    std::memcpy(mem.data(), img.data(), std::min<size_t>(img.size(), 0x290 * 16));
    std::memcpy(mem.data() + 5 * 16, view, 64);
    std::memcpy(mem.data() + 0x290 * 16, consts, 14 * 16);
    const uint32_t ca = e.colour_addr[static_cast<size_t>(lod)], cn = e.colour_count[static_cast<size_t>(lod)];
    for (uint32_t k = 0; k < cn; ++k) {
        const uint32_t w = colours != nullptr ? colours[k] : 0;
        const uint32_t q[4] = {w & 0xff, (w >> 8) & 0xff, (w >> 16) & 0xff, w >> 24};
        std::memcpy(mem.data() + 16 * (ca + 2 * k), q, 16);
    }
    if (vu_mem == nullptr) base = 0;
    else std::memcpy(mem.data(), vu_mem, 0x4000);
    std::vector<Kicked> ks;
    if (b) tfrag_b_kick(mem.data(), base, ks, true);
    else tfrag_kick(mem.data(), base, code, ks, true);
    uint32_t bad = 0;
    size_t j = 0;
    for (const Kicked& k : ks)
        for (size_t i = 0; i < k.q.size(); ++i) {
            if (k.part[i] != 4) continue;
            ++compared;
            if (j >= mine.size() || i < 2) {
                ++bad;
                ++j;
                continue;
            }
            const TfragCpuVertex& v = mine[j++];
            const bool same = k.q[i] == v.xyzf && k.q[i - 1] == v.rgba && k.q[i - 2][0] == v.stq[0] && k.q[i - 2][1] == v.stq[1] && k.q[i - 2][2] == v.stq[2] &&
                              (k.hp.empty() || std::memcmp(k.hp[i].data(), v.hp.data(), 16) == 0);
            if (!same) {
                ++bad;
                static int shown = 0;
                if (shown++ < 12)
                    std::printf("[mrpw tfrag] selfcheck code %x lod %d vertex %zu: xyzf %08x %08x %08x %08x / %08x %08x %08x %08x rgba %x %x %x %x / %x %x %x %x stq %08x %08x %08x / %08x %08x %08x\n",
                                code, lod, j - 1, k.q[i][0], k.q[i][1], k.q[i][2], k.q[i][3], v.xyzf[0], v.xyzf[1], v.xyzf[2], v.xyzf[3], k.q[i - 1][0], k.q[i - 1][1], k.q[i - 1][2],
                                k.q[i - 1][3], v.rgba[0], v.rgba[1], v.rgba[2], v.rgba[3], k.q[i - 2][0], k.q[i - 2][1], k.q[i - 2][2], v.stq[0], v.stq[1], v.stq[2]);
            }
        }
    if (j != mine.size()) bad += 1;
    return bad;
}

// ---- the host's decisions (LOD > 0 / widescreen) ------------------------------------------------------------------------------

namespace {
TfragSnapshot g_tsnap;
float rf(uint32_t a) { return f_of(ru(a)); }
std::array<float, 4> rq(uint32_t a) { return {rf(a), rf(a + 4), rf(a + 8), rf(a + 12)}; }
// (the LOD slider's scaling, mrp_lod.cpp scaled / scaled_int: "unlimited" = 1e6 units, 0x3fffffff x 1024 integers)
float scaled_f(float d, float s) { return s == 1.0f ? d : s == 0.0f ? 1.0e6f : std::min(d * s, 1.0e6f); }
int32_t scaled_i(int32_t d, float s) {
    if (s == 1.0f) return d;
    if (s == 0.0f) return 0x3fffffff;
    const double v = static_cast<double>(d) * static_cast<double>(s);
    return v > static_cast<double>(0x3fffffff) ? 0x3fffffff : static_cast<int32_t>(v);
}
bool neg(float f) { return (u_of(f) & 0x80000000u) != 0; }  // (bltz / bgez on a lane moved to a GPR: the sign bit)
}  // namespace

// orig: (host) see mrp_tfrag_gpu.h. The pass's loads (world_cull.cpp 0x294368): vf28 = 0x166dc0 x 1024, vf29..vf31 = 0x166c80.., vf12..
// vf15 = 0x166d80.., vf23 = 0x16cae0 (its y the draw distance 0x160f80), vf24 = 0x16caf0, vf26 = 0x16cc20, vf27 = 0x16cc30, vf22 =
// 0x160f20, the bands 0x160f70.., the zones 0x173d00 (SPR 0x3b00), the enable *(0x169ff0 + 0x10). Called on the game's thread (inside
// the journal when the view is widened: the widened values are read)
void tfrag_note_pass(bool handle, float scale, float widen) {
    TfragSnapshot& S = g_tsnap;
    S.handle = handle;
    S.scale = scale;
    S.widen = widen;
    for (uint32_t r = 0; r < 3; ++r) S.rot[r] = rq(OV(0x00166c80u) + 0x10 * r);
    S.cam = rq(OV(0x00166dc0u));
    for (uint32_t r = 0; r < 4; ++r) S.clip[r] = rq(OV(0x00166d80u) + 0x10 * r);
    S.c23 = rq(OV(0x0016cae0u));
    S.c24 = rq(OV(0x0016caf0u));
    S.c26 = rq(OV(0x0016cc20u));
    S.c27 = rq(OV(0x0016cc30u));
    S.c22 = rq(OV(0x00160f20u));
    for (uint32_t k = 0; k < 3; ++k) S.bands[k] = static_cast<int32_t>(ru(OV(0x00160f70u) + 4 * k));
    S.dist = ru(OV(0x00160f80u));
    for (uint32_t k = 0; k < 0x80; ++k) S.zone[k] = static_cast<uint8_t>(rb(OV(0x00173d00u) + k));
    S.enabled = static_cast<int32_t>(ru(OV(0x00169ff0u) + 0x10)) != 0;
    S.valid = true;
}

// orig: (host) see mrp_tfrag_gpu.h
const TfragSnapshot& tfrag_snapshot() { return g_tsnap; }

// orig: (host) see mrp_tfrag_gpu.h: 0x294368's per-entry work, in its order and with its VU0 macro operations (vu0_macro.h lanes: the
// VU float rules), the chain's order kept: the entries in the table's order with their LOD code, then those its box test sends to the
// clipping program (code 2; the list holds 127, the next ones are not drawn)
void tfrag_decide(const TfragLevel& L, const TfragSnapshot& S, bool check, std::vector<std::pair<uint32_t, uint8_t>>& out) {
    using namespace vu0m;
    out.clear();
    if (!L.ok || !S.valid || !S.enabled || L.count == 0) return;
    (void)check;  // (the check makes them at the snapshot's settings too: the game's own at LOD 0 / 4:3, the shadow run's otherwise)
    const float s = S.scale;
    // the LOD bands scaled as mrp_lod.cpp patch_tfrag scales them. (The widened shadow run lost that scaling: its patch_widen runs
    // projection_setup 0x1f7980 after patch_tfrag, whose lod_bands_set 0x293f00 writes 0x160f70.. again from the unscaled 0x160f60..; the
    // LOD slider then changed only tfrag's draw distance in widescreen. RAC1_MRPW_TFRAG_SHADOW_BANDS=1, testing: the bands as that shadow had them)
    static const bool shadow_bands = std::getenv("RAC1_MRPW_TFRAG_SHADOW_BANDS") != nullptr;
    const float sb = shadow_bands && S.widen > 1.0f ? 1.0f : s;
    int32_t band[3];
    for (int k = 0; k < 3; ++k) band[k] = scaled_i(S.bands[static_cast<size_t>(k)], sb);
    const float dist = scaled_f(f_of(S.dist), s);
    // vf28 = camera x 1024; vf1.xyz = -vf28; vf21 = the clip matrix's translation by it (ACC chain), vf18..vf20 its rows
    float c28[4], c1[3];
    for (int l = 0; l < 4; ++l) c28[l] = mul(S.cam[static_cast<size_t>(l)], 1024.0f);
    for (int l = 0; l < 3; ++l) c1[l] = sub(0.0f, c28[l]);
    float m21[4];
    for (int l = 0; l < 4; ++l) {
        float a = mul(S.clip[0][static_cast<size_t>(l)], c1[0]);
        a = madd(a, S.clip[1][static_cast<size_t>(l)], c1[1]);
        a = madd(a, S.clip[2][static_cast<size_t>(l)], c1[2]);
        m21[l] = madd(a, S.clip[3][static_cast<size_t>(l)], 1.0f);
    }
    float m18[3][4];
    for (int r = 0; r < 3; ++r)
        for (int l = 0; l < 4; ++l) m18[r][l] = mul(S.clip[static_cast<size_t>(r)][static_cast<size_t>(l)], 1.0f);
    const float c23x = S.c23[0], c23y = add(0.0f, dist);
    std::vector<uint32_t> clip_list;
    for (uint32_t k = 0; k < L.count; ++k) {
        const uint32_t e = L.table + 0x40 * k;
        const float sph[4] = {rf(e), rf(e + 4), rf(e + 8), rf(e + 12)};
        float v2[3];
        for (int l = 0; l < 3; ++l) v2[l] = sub(sph[l], c28[l]);
        const float r = sph[3];
        const float v3x = add(0.0f, r), v3y = sub(0.0f, r);
        float rv[3];
        for (int l = 0; l < 3; ++l) {
            float a = mul(S.rot[0][static_cast<size_t>(l)], v2[0]);
            a = madd(a, S.rot[1][static_cast<size_t>(l)], v2[1]);
            rv[l] = madd(a, S.rot[2][static_cast<size_t>(l)], v2[2]);
        }
        const uint32_t cell = rb(e + 0x3b), mask = rb(e + 0x3a);
        if (cell >= 0x80 || (mask & S.zone[cell]) == 0) continue;
        const float v8x = mul(S.c26[0], r), v8y = mul(S.c26[1], r);
        const float v1x = add(v3x, rv[2]), v1y = add(v3y, rv[2]);
        const float v5x = mul(S.c24[0], rv[2]), v5y = mul(S.c24[1], rv[2]);
        const float ax = std::fabs(rv[0]), ay = std::fabs(rv[1]);
        const float d3x = sub(c23x, v1x), d3y = sub(c23y, v1y);
        const float v6x = sub(ax, v8x), v6y = sub(ay, v8y);
        const float p8x = add(ax, v8x), p8y = add(ay, v8y);
        const float v4x = sub(v1x, S.c27[0]), v4y = sub(v1y, S.c27[1]);
        const float v7x = sub(v5x, v6x), v7y = sub(v5y, v6y);
        const float w8x = sub(v5x, p8x), w8y = sub(v5y, p8y);
        if (neg(d3y)) continue;   // (beyond the draw distance)
        if (!neg(d3x)) continue;  // (before the near plane)
        const int32_t zp = static_cast<int32_t>(vu0m::ftoi(v1x, 1.0f)), zm = static_cast<int32_t>(vu0m::ftoi(v1y, 1.0f));
        if (neg(v7y) || neg(v7x)) continue;  // (outside the sides)
        const bool across = neg(w8y) || neg(v4y) || neg(w8x) || !neg(v4x);
        if (across) {
            // the box test (L_294978): the 8 corners (s16 at data + *(u16)(e + 0x38), x 64) through the clip matrix, VCLIPW unscaled (all
            // outside one plane: culled) and scaled by vf22.xy (any outside: the clipping program)
            const uint32_t data = ru(e + 0x10);
            const uint32_t at = rh(e + 0x38) + data;
            uint32_t and_u = 0x3f, or_s = 0;
            for (uint32_t c = 0; c < 8; ++c) {
                float cp[3];
                for (uint32_t l = 0; l < 3; ++l) cp[l] = static_cast<float>(static_cast<int32_t>(static_cast<int16_t>(rh(at + 8 * c + 2 * l))) * 64);
                Vf p;
                for (int l = 0; l < 4; ++l) {
                    float a = mul(m18[0][l], cp[0]);
                    a = madd(a, m18[1][l], cp[1]);
                    a = madd(a, m18[2][l], cp[2]);
                    p.v[l] = madd(a, m21[l], 1.0f);
                }
                uint32_t fu = 0;
                clipw(fu, p, p.v[3]);
                and_u &= fu & 0x3f;
                p.v[0] = mul(p.v[0], S.c22[0]);
                p.v[1] = mul(p.v[1], S.c22[1]);
                uint32_t fs = 0;
                clipw(fs, p, p.v[3]);
                or_s |= fs & 0x3f;
            }
            if (and_u != 0) continue;
            if (or_s != 0) {
                if (clip_list.size() < 127) clip_list.push_back(k);
                continue;
            }
        }
        uint8_t code;
        auto le0 = [](int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)) <= 0; };  // (sub; blez)
        if (rb(e + 0x27) != 0 || le0(band[0], zm)) code = 0x06;
        else if (le0(band[0], zp)) code = 0x08;
        else if (le0(band[1], zm)) code = 0x0a;
        else if (le0(band[1], zp)) code = 0x0e;
        else if (le0(band[2], zm) || le0(band[2], zp)) code = 0x10;
        else code = 0x14;
        out.push_back({k, code});
    }
    for (uint32_t k : clip_list) out.push_back({k, 2});
}

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_shadow_decide(const TfragLevel& L, const TfragSnapshot& S, const CasterVolume& v, std::vector<std::pair<uint32_t, uint8_t>>& out,
                         std::vector<uint16_t>& views) {
    out.clear();
    views.clear();
    if (!v.ok || !L.ok || !S.valid || !S.enabled || L.count == 0) return;
    std::vector<std::pair<uint32_t, uint8_t>> vis;
    tfrag_decide(L, S, false, vis);
    std::vector<uint8_t> drawn(L.count, 0);
    for (const auto& [k, code] : vis)
        if (k < L.count && code != 2) drawn[k] = 1;  // (the clipping program's draws are cut to the view: these cast whole from here)
    int32_t band[3];
    for (int k = 0; k < 3; ++k) band[k] = scaled_i(S.bands[static_cast<size_t>(k)], S.scale);
    static int dbg = 0;
    if (dbg > 0) {
        --dbg;
        std::printf("[tfrag-shadow] bands %d %d %d\n", band[0], band[1], band[2]);
        for (const auto& [k, code] : vis) {
            const uint32_t e = L.table + 0x40 * k;
            float z = 0.0f;
            for (int l = 0; l < 3; ++l) z += S.rot[static_cast<size_t>(l)][2] * (rf(e + 4 * l) - S.cam[static_cast<size_t>(l)] * 1024.0f);
            std::printf("[tfrag-shadow]   drawn entry %u code %x z %.0f r %.0f\n", k, code, z, rf(e + 12));
        }
    }
    // (the frame's own tfrags are not drawn into the shadow maps: their LOD follows the view's depth - an entry seen at mid distance is a
    // coarser morph than the same entry behind the camera - so the shadow maps take every entry here, at the LOD its distance gives)
    static const bool keep_drawn = std::getenv("RAC1_SHADOW_TFRAG_KEEP") != nullptr;  // (testing: the frame's draws kept, the rest added)
    for (uint32_t k = 0; k < L.count; ++k) {
        if (keep_drawn && drawn[k] != 0) continue;
        const uint32_t e = L.table + 0x40 * k;
        static const bool zones = std::getenv("RAC1_SHADOW_ZONES") != nullptr && std::atoi(std::getenv("RAC1_SHADOW_ZONES")) != 0;  // (testing)
        const uint32_t cell = rb(e + 0x3b), mask = rb(e + 0x3a);
        if (zones && (cell >= 0x80 || (mask & S.zone[cell]) == 0)) continue;
        const float sph[4] = {rf(e), rf(e + 4), rf(e + 8), rf(e + 12)};  // (x 1024: the pass compares it with the camera x 1024)
        const float wsph[4] = {sph[0] / 1024.0f, sph[1] / 1024.0f, sph[2] / 1024.0f, sph[3] / 1024.0f};
        const uint16_t vw = v.views(wsph);
        if (vw == 0) continue;
        static const bool trace_hi = std::getenv("RAC1_SHADOW_TFRAG_HIGH") != nullptr;  // (testing: the entries high above the camera)
        const float along = (wsph[0] - S.cam[0]) * v.sun[0] + (wsph[1] - S.cam[1]) * v.sun[1] + (wsph[2] - S.cam[2]) * v.sun[2];
        if (trace_hi && along > 150.0f) {
            static int shown = 0;
            if (shown++ < 60)
                std::printf("[tfrag-high] entry %u sphere %.1f %.1f %.1f r %.1f (camera %.1f %.1f %.1f, along the sun %.0f) drawn %d zone cell %u mask %02x on %02x\n", k, wsph[0], wsph[1], wsph[2], wsph[3], S.cam[0], S.cam[1], S.cam[2], along, drawn[k],
                            rb(e + 0x3b), rb(e + 0x3a), rb(e + 0x3b) < 0x80 ? S.zone[rb(e + 0x3b)] : 0u);
        }
        // the distance (x 1024, as the pass's depth: the same bands) and the bands' codes (tfrag_decide's)
        float z = 0.0f;
        if (keep_drawn) {
            for (int l = 0; l < 3; ++l) z += S.rot[static_cast<size_t>(l)][2] * (sph[l] - S.cam[static_cast<size_t>(l)] * 1024.0f);
        } else {
            for (int l = 0; l < 3; ++l) z += (sph[l] - S.cam[static_cast<size_t>(l)] * 1024.0f) * (sph[l] - S.cam[static_cast<size_t>(l)] * 1024.0f);
            z = std::sqrt(z);
        }
        const int32_t zp = static_cast<int32_t>(sph[3] + z), zm = static_cast<int32_t>(z - sph[3]);
        auto le0 = [](int32_t a, int32_t b) { return static_cast<int32_t>(static_cast<uint32_t>(a) - static_cast<uint32_t>(b)) <= 0; };
        uint8_t code;
        if (rb(e + 0x27) != 0 || le0(band[0], zm)) code = 0x06;
        else if (le0(band[0], zp)) code = 0x08;
        else if (le0(band[1], zm)) code = 0x0a;
        else if (le0(band[1], zp)) code = 0x0e;
        else if (le0(band[2], zm) || le0(band[2], zp)) code = 0x10;
        else code = 0x14;
        out.push_back({k, code});
        views.push_back(vw);
    }
    // (by their views: the renderer's batches, one per views, stay few)
    std::vector<size_t> ord(out.size());
    for (size_t i = 0; i < ord.size(); ++i) ord[i] = i;
    std::stable_sort(ord.begin(), ord.end(), [&views](size_t a, size_t b) { return views[a] < views[b]; });
    std::vector<std::pair<uint32_t, uint8_t>> o2;
    std::vector<uint16_t> v2;
    for (size_t i : ord) {
        o2.push_back(out[i]);
        v2.push_back(views[i]);
    }
    out.swap(o2);
    views.swap(v2);
}

// ---- the GPU program --------------------------------------------------------------------------------------------------------

// orig: (host) see mrp_tfrag_gpu.h
bool tfrag_gpu_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_MRPW_TFRAG_GPU");
        return e == nullptr || std::atoi(e) != 0;
    }();
    return on;
}

// orig: (host) see mrp_tfrag_gpu.h
bool tfrag_host_enabled() {
    static const bool on = [] {
        const char* e = std::getenv("RAC1_MRPW_TFRAG_HOST");
        return e == nullptr || std::atoi(e) != 0;
    }();
    return on;
}

namespace {

// The compute program: one workgroup per draw; the image in shared memory, the steps in their order (each step's iterations spread
// over the workgroup unless the plan says they depend on each other), the emission in two phases (the positions and the guard band's
// flags; then each vertex with its ADC, its triangle). Every float operation is the CPU reference's (world_mrp.cpp tf_transform /
// tf_morph / tf_fix / tf_emit), in its order, with the float rules the game's thread runs it with (ADR 0011: add / sub / mul / the
// division toward zero, denormals flushed: the GPU's nearest results corrected by error-free transforms), the world point in double
// toward zero as the renderer's GsFront::vertex makes it on that thread.
const char* kCs = R"(#version 460 core
layout(local_size_x = 64) in;
struct Draw { uint image, nq, plan, colour, col_addr, col_count, view, consts, vbase, ibase, rbase, flags, wview, p1, p2, p3; };
layout(std430, binding = 0) readonly buffer Draws { Draw draws[]; };
layout(std430, binding = 1) readonly buffer Images { uvec4 images[]; };
layout(std430, binding = 2) readonly buffer Plans { uint plans[]; };
layout(std430, binding = 3) readonly buffer Cols { uint cols[]; };
layout(std430, binding = 4) readonly buffer Views { double views[]; };
layout(std430, binding = 5) readonly buffer VuViews { float vuviews[]; };
layout(std430, binding = 6) readonly buffer Consts { uvec4 consts[]; };
layout(std430, binding = 7) readonly buffer Runs { uvec4 runs[]; };
struct V { float wp0, wp1, wp2, wp3, vu0, vu1, vu2, vu3, s, t, q, lodq; uint rgba; float fog; uint word, layer, st0, st1; };
layout(std430, binding = 8) writeonly buffer OutV { V outv[]; };
layout(std430, binding = 9) writeonly buffer OutI { uint outi[]; };

shared uvec4 vu[0x148];
shared uvec4 kc[14];
shared uint fl[2048];  // the guard band's flags, a byte per sent vertex (8192 at most)

// ---- the EE / VU float rules (ADR 0011): every add / sub / mul rounded toward zero, denormals flushed (operands and results). The GPU
// rounds to nearest: each operation's exact error is found by an error-free transform (two-sum; fma for the product, its one rounding
// exact here) and the result stepped one ulp toward zero when the nearest one lies beyond the exact value.
float ftz(float x) {
    uint u = floatBitsToUint(x);
    return (u & 0x7f800000u) == 0u ? uintBitsToFloat(u & 0x80000000u) : x;
}
// `s` the nearest result, `e` the exact value less it: toward zero
float chop(float s, float e) {
    if (e == 0.0 || s == 0.0 || isinf(s) || isnan(s)) return ftz(s);
    if ((e < 0.0) != (s < 0.0)) s = uintBitsToFloat(floatBitsToUint(s) - 1u);
    return ftz(s);
}
float addz(float a, float b) {
    precise float x = ftz(a);
    precise float y = ftz(b);
    precise float s = x + y;
    precise float bb = s - x;
    precise float e = (x - (s - bb)) + (y - bb);
    return chop(s, e);
}
float subz(float a, float b) { return addz(a, -b); }
float mulz(float a, float b) {
    precise float x = ftz(a);
    precise float y = ftz(b);
    precise float p = x * y;
    precise float e = fma(x, y, -p);
    return chop(p, e);
}
vec4 add4(vec4 a, vec4 b) { return vec4(addz(a.x, b.x), addz(a.y, b.y), addz(a.z, b.z), addz(a.w, b.w)); }
vec4 mul4(vec4 a, vec4 b) { return vec4(mulz(a.x, b.x), mulz(a.y, b.y), mulz(a.z, b.z), mulz(a.w, b.w)); }
vec4 mul4s(vec4 a, float b) { return vec4(mulz(a.x, b), mulz(a.y, b), mulz(a.z, b), mulz(a.w, b)); }
// a / b toward zero (the CPU reference's `/` on the toward-zero thread: the VU's DIV rule there): the quotient refined to the nearest,
// then one step toward zero when that lies beyond the exact value (its exact residual a - q b, by fma, of the sign that says so)
float divz(float a, float b) {
    a = ftz(a);
    b = ftz(b);
    precise float q = a / b;
    if (isinf(q) || isnan(q) || q == 0.0) return ftz(q);
    precise float r = fma(-q, b, a);
    precise float q2 = fma(r, 1.0 / b, q);
    float best = q2;
    precise float br = abs(fma(-q2, b, a));
    float qn = uintBitsToFloat(floatBitsToUint(q2) + 1u), qp = uintBitsToFloat(floatBitsToUint(q2) - 1u);
    precise float rn = abs(fma(-qn, b, a));
    precise float rp = abs(fma(-qp, b, a));
    if (rn < br) { best = qn; br = rn; }
    if (rp < br) best = qp;
    precise float res = fma(-best, b, a);  // a - best b = b (a / b - best): beyond the exact value when its sign is not a's
    if (res != 0.0 && ((res < 0.0) != (a < 0.0))) best = uintBitsToFloat(floatBitsToUint(best) - 1u);
    return ftz(best);
}
// the doubles of the renderer's world point (host code on the same thread: toward zero too)
double dstep(double s) {
    uvec2 u = unpackDouble2x32(s);
    if (u.x == 0u) { u.x = 0xffffffffu; u.y -= 1u; } else u.x -= 1u;
    return packDouble2x32(u);
}
double dchop(double s, double e) {
    if (e == 0.0LF || s == 0.0LF) return s;
    if ((e < 0.0LF) != (s < 0.0LF)) s = dstep(s);
    return s;
}
double daddz(double a, double b) {
    precise double s = a + b;
    precise double bb = s - a;
    precise double e = (a - (s - bb)) + (b - bb);
    return dchop(s, e);
}
double dmulz(double a, double b) {
    precise double p = a * b;
    precise double e = fma(a, b, -p);
    return dchop(p, e);
}
// a double narrowed to a float toward zero (then flushed)
float d2fz(double d) {  // (by the bits: the mantissa's top 23 bits kept)
    uvec2 u = unpackDouble2x32(d);
    uint sign = u.y & 0x80000000u;
    int e = int((u.y >> 20) & 0x7ffu) - 1023 + 127;
    if (e <= 0) return uintBitsToFloat(sign);
    if (e >= 255) return uintBitsToFloat(sign | 0x7f7fffffu);
    return uintBitsToFloat(sign | uint(e) << 23 | (u.y & 0xfffffu) << 3 | u.x >> 29);
}

uvec4 at(uint a) { return a < 0x148u ? vu[a] : uvec4(0u); }
float fa(uint a, uint l) { return uintBitsToFloat(at(a)[l]); }
float kf(uint c, uint l) { return uintBitsToFloat(kc[c - 0x290u][l]); }
uint u16(uint a, uint l) { return at(a)[l] & 0xffffu; }
vec4 ld(uint a) { return uintBitsToFloat(at(a)); }
vec4 itof0(uint a) { return vec4(ivec4(at(a))); }  // (the records' s16 and the colours' bytes: exact)
void st(uint a, vec4 v) { if (a < 0x148u) vu[a] = floatBitsToUint(v); }
// p = T + R0 x + R1 y + R2 z (the FMAC chain's order), the view at 5..8
vec4 xform(vec4 p) {
    vec4 a = ld(8u);
    a = add4(a, mul4s(ld(5u), p.x));
    a = add4(a, mul4s(ld(6u), p.y));
    return add4(a, mul4s(ld(7u), p.z));
}
float fmin_(float a, float b) { return b < a ? b : a; }
float fmax_(float a, float b) { return a < b ? b : a; }
int ftoi(float v) {
    if (isnan(v)) return 0x7fffffff;
    if (!(v < 2147483648.0)) return 0x7fffffff;
    if (!(v > -2147483648.0)) return int(0x80000000u);
    return int(v);
}

void step_iter(uint kind, uint band, uint a, uint b, uint k) {
    if (kind == 0u) {  // T (micro 0x04f / 0x052): the record through the view at 5..8, its colour ITOF0
        uint ad = a + 2u * k;
        vec4 p = xform(itof0(ad));
        vec4 c = itof0(ad + 1u);
        st(ad, p);
        st(ad + 1u, c);
    } else if (kind <= 2u) {  // M / MS (micro 0x093 / 0x08a, band A 0x29a; 0x156 / 0x160, band B 0x29b): f = clamp(c + k p.w), p' = (P1 + P2) f.x + p f.y
        float kx = kf(band, 0u), ky = kf(band, 1u), cx = kf(band + 2u, 0u), cy = kf(band + 2u, 1u), thr = kf(band, 3u);
        float mx = kf(0x291u, 0u), my = kf(0x291u, 1u);
        uint via = a + k;
        uint rec = u16(via, 3u);
        uint pvi = at(b + k / 4u)[k % 4u] & 0xffffu;
        uint r1 = u16(pvi, 3u), r2 = u16(via, 2u);
        vec4 p = xform(itof0(rec));
        vec4 c = itof0(rec + 1u);
        vec4 P1 = ld(r1), C1 = ld(r1 + 1u), P2 = ld(r2), C2 = ld(r2 + 1u);
        float fx = addz(mulz(cx, 1.0), mulz(kx, p.w));
        float fy = addz(mulz(cy, 1.0), mulz(ky, p.w));
        fx = fmax_(fmin_(fx, mx), 0.0);
        fy = fmax_(fmin_(fy, my), 0.0);
        vec4 S = add4(P1, P2);
        vec4 SC = add4(C1, C2);
        vec4 po = add4(mul4s(S, fx), mul4s(p, fy));
        vec4 co = add4(mul4s(SC, fx), mul4s(c, fy));
        float d1 = subz(P1.w, thr);
        float d2 = subz(P2.w, thr);
        if (kind == 2u && d1 < 0.0 && d2 < 0.0) {
            vu[via] = at(pvi);
            return;
        }
        st(rec, po);
        st(rec + 1u, co);
    } else {  // F (micro 0x278 / 0x27d): the list entry takes the second index's vertex info when both records are past the threshold
        float thr = kf(band, 3u);
        uint la = a + k;
        uint vi = at(b + k / 4u)[k % 4u] & 0xffffu;
        uint ra = u16(la, 2u), rb = u16(vi, 3u);
        float d1 = subz(fa(ra, 3u), thr);
        float d2 = subz(fa(rb, 3u), thr);
        if (d1 < 0.0 && d2 < 0.0) vu[la] = at(vi);
    }
}

void main() {
    Draw d = draws[gl_WorkGroupID.x];
    uint tid = gl_LocalInvocationID.x;
    for (uint q = tid; q < 0x148u; q += 64u) vu[q] = q < d.nq ? images[d.image + q] : uvec4(0u);
    if (tid < 14u) kc[tid] = consts[d.consts * 14u + tid];
    for (uint j = tid; j < 2048u; j += 64u) fl[j] = 0u;
    barrier();
    if (tid < 16u) vu[5u + tid / 4u][tid % 4u] = floatBitsToUint(vuviews[d.view * 16u + tid]);
    barrier();
    for (uint k = tid; k < min(d.col_count, 0x148u); k += 64u) {
        uint w = d.colour != 0xffffffffu ? cols[d.colour + k] : 0u;
        if (d.col_addr + 2u * k < 0x148u) vu[d.col_addr + 2u * k] = uvec4(w & 0xffu, (w >> 8) & 0xffu, (w >> 16) & 0xffu, w >> 24);
    }
    barrier();
    uint P = d.plan;
    uint hdr = plans[P];
    uint nsteps = min(hdr & 0xffu, 4u);  // (the loops' bounds clamped - the CPU refuses such a plan before)
    int form = int((hdr >> 8) & 7u);
    uint nv = min(plans[P + 1u], 8192u);
    uint sp = P + 3u;
    for (uint si = 0u; si < nsteps; ++si) {
        uint w0 = plans[sp], w1 = plans[sp + 1u], n = min(plans[sp + 2u], 1312u);
        sp += 3u;
        uint kind = w0 & 0xffu, band = w0 >> 16, a = w1 & 0xffffu, b = w1 >> 16;
        if ((w0 & 0x100u) != 0u) {
            if (tid == 0u)
                for (uint k = 0u; k < n; ++k) step_iter(kind, band, a, b, k);
        } else {
            for (uint k = tid; k < n; k += 64u) step_iter(kind, band, a, b, k);
        }
        memoryBarrierShared();
        barrier();
    }
    uint vp = sp;  // the vertices' words
    bool bform = form == 4;
    if (bform) form = 3;
    // (a caster draw of the modern lighting, flags bit 31, keeps every triangle: the guard band's ADC drops the ones off the
    // view, which still cast)
    bool guard = (form == 1 || form == 3) && (d.flags & 0x80000000u) == 0u;
    uint tex = u16(4u, 3u);
    float stoff = (form == 1 || bform) ? fa(tex + 1u, 3u) : kf(0x291u, 2u);
    vec4 c0 = uintBitsToFloat(kc[0]), c4 = uintBitsToFloat(kc[8]), c5 = uintBitsToFloat(kc[5]), c6 = uintBitsToFloat(kc[6]);
    if (guard) {  // phase 1: the guard band's flags (0x382 / 0x652: CLIPW of P x C6 against its w)
        for (uint j = tid; j < nv; j += 64u) {
            uint rec = u16(plans[vp + 2u * j] & 0xffffu, 3u);
            vec4 Pj = form <= 1 ? ld(rec) : xform(itof0(rec));
            vec4 g = mul4(Pj, c6);
            float w = abs(g.w);
            uint f = 0u;
            if (g.x > w) f |= 1u;
            if (g.x < -w) f |= 2u;
            if (g.y > w) f |= 4u;
            if (g.y < -w) f |= 8u;
            if (g.z > w) f |= 16u;
            if (g.z < -w) f |= 32u;
            if (f != 0u) atomicOr(fl[j >> 2], f << (8u * (j & 3u)));
        }
        memoryBarrierShared();
        barrier();
    }
    for (uint j = tid; j < nv; j += 64u) {  // phase 2: the vertices (0x45d / 0x382 / 0x51d / 0x652, strip change 0x332 / 0x5fd): Q = C0.x / P.w, STQ, F, RGBA, ADC
        uint v0 = plans[vp + 2u * j], v1 = plans[vp + 2u * j + 1u];
        uint via = v0 & 0xffffu, pos = (v0 >> 16) & 3u;
        uint rec = u16(via, 3u);
        vec4 Pj = form <= 1 ? ld(rec) : xform(itof0(rec));
        vec4 hp = Pj;
        float s0 = subz(fa(via, 0u), stoff);
        float t0 = subz(fa(via, 1u), stoff);
        float q0 = subz(addz(1.0, stoff), stoff);
        bool adc = false;
        if (guard && !bform) {
            uint h = 0u;
            if ((v0 & (1u << 18)) != 0u) h |= (fl[j >> 2] >> (8u * (j & 3u))) & 0xffu;
            if (pos >= 1u && (plans[vp + 2u * (j - 1u)] & (1u << 18)) != 0u) h |= (fl[(j - 1u) >> 2] >> (8u * ((j - 1u) & 3u))) & 0xffu;
            if (pos >= 2u && (plans[vp + 2u * (j - 2u)] & (1u << 18)) != 0u) h |= (fl[(j - 2u) >> 2] >> (8u * ((j - 2u) & 3u))) & 0xffu;
            adc = h != 0u;
        }
        float q = divz(c0.x, Pj.w);
        float qq = mulz(q0, q);
        float ss = mulz(s0, q);
        float tt = mulz(t0, q);
        float fw = addz(Pj.w, c5.w);
        if (form == 3) { fw = fmin_(fw, c0.z); fw = fmax_(fw, c0.y); }
        else { fw = fmax_(fw, c0.y); fw = fmin_(fw, c0.z); }
        if (adc) fw = addz(fw, c0.w);
        uvec4 rgba;
        if (form <= 1) {
            vec4 C = mul4(ld(rec + 1u), c4);
            rgba = uvec4(ftoi(C.x), ftoi(C.y), ftoi(C.z), ftoi(C.w));
        } else {
            rgba = at(rec + 1u);
        }
        uint xyzf3 = uint(ftoi(mulz(fw, 16.0)));
        // the renderer's vertex (mrp_world_renderer.cpp GsFront::vertex, the world projection): the world point (double, then narrowed),
        // STQ x the positive w, the GS's Q, RGBA, F; the run's texture and GS state
        uint wv = d.wview * 16u;
        double h0 = double(hp.x), h1 = double(hp.y), h2 = double(hp.z), h3 = double(hp.w);
        double x0 = daddz(daddz(daddz(dmulz(views[wv + 0u], h0), dmulz(views[wv + 4u], h1)), dmulz(views[wv + 8u], h2)), dmulz(views[wv + 12u], h3));
        double x1 = daddz(daddz(daddz(dmulz(views[wv + 1u], h0), dmulz(views[wv + 5u], h1)), dmulz(views[wv + 9u], h2)), dmulz(views[wv + 13u], h3));
        double x2 = daddz(daddz(daddz(dmulz(views[wv + 2u], h0), dmulz(views[wv + 6u], h1)), dmulz(views[wv + 10u], h2)), dmulz(views[wv + 14u], h3));
        double x3 = daddz(daddz(daddz(dmulz(views[wv + 3u], h0), dmulz(views[wv + 7u], h1)), dmulz(views[wv + 11u], h2)), dmulz(views[wv + 15u], h3));
        float kk = mulz(qq, hp.w);
        float sgn = kk < 0.0 ? -1.0 : 1.0;
        float Wg = mulz(hp.w, sgn);
        float ws = Wg != 0.0 ? Wg : 1.0;
        V o;
        o.wp0 = d2fz(x0); o.wp1 = d2fz(x1); o.wp2 = d2fz(x2); o.wp3 = d2fz(x3);
        o.vu0 = 0.0; o.vu1 = 0.0; o.vu2 = 0.0; o.vu3 = 0.0;
        o.s = mulz(ss, ws); o.t = mulz(tt, ws); o.q = mulz(qq, ws);
        o.lodq = qq;
        o.rgba = (rgba.x & 0xffu) | (rgba.y & 0xffu) << 8 | (rgba.z & 0xffu) << 16 | (rgba.w & 0xffu) << 24;
        o.fog = float((xyzf3 >> 4) & 0xffu);
        uvec4 run = runs[d.rbase + (v1 & 0xffffu)];
        o.word = run.x; o.layer = run.y; o.st0 = run.z; o.st1 = run.w;
        outv[d.vbase + j] = o;
        if (pos >= 2u) {
            uint t = d.ibase + 3u * (v1 >> 16);
            uint c = d.vbase + j;
            if (adc) { outi[t] = c; outi[t + 1u] = c; outi[t + 2u] = c; }
            else { outi[t] = c - 2u; outi[t + 1u] = c - 1u; outi[t + 2u] = c; }
        }
    }
}
)";

using PfnDispatchCompute = void(APIENTRYP)(GLuint, GLuint, GLuint);
using PfnMemoryBarrier = void(APIENTRYP)(GLbitfield);
PfnDispatchCompute p_dispatch = nullptr;
PfnMemoryBarrier p_barrier = nullptr;
constexpr GLenum kSsbo = 0x90D2, kComputeShader = 0x91B9;
constexpr GLbitfield kVertexAttribBarrier = 0x1, kElementBarrier = 0x2, kBufferUpdateBarrier = 0x200;

struct Gpu {
    bool tried = false, ok = false;
    GLuint prog = 0;
    GLuint buf[10] = {};  // draws, images, plans, colours, views, vu views, constants, runs, out vertices, out indices
    size_t cap[10] = {};
    uint64_t level_gen = 0;
    size_t colour_size = 0;
    // what the static buffers hold, for the dispatch's validation: the images' qwords, the plans' words (a copy), each
    // plan's first word -> its runs, the colours' words
    size_t image_q = 0;
    std::vector<uint32_t> plan_words;
    std::unordered_map<uint32_t, uint32_t> plan_runs;
    size_t colour_words = 0;
    uint64_t refused = 0;
} g_gpu;

struct VkGpu {
    bool tried = false, ok = false;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    VkPipelineLayout pipe_layout = VK_NULL_HANDLE;
    VkShaderModule cs_mod = VK_NULL_HANDLE;
    VkPipeline pipe = VK_NULL_HANDLE;
    vk::GpuBuffer img_buf{};
    vk::GpuBuffer plan_buf{};
    // the resident colours, one buffer per frame slot (the frame before's compute may still read its own while this one is written):
    // each slot's copy at the colours' update `col_at` (0: none - written whole), brought up to date by the dirty ranges of the
    // uploads it missed (this one's, and the one before's kept in `prev_dirty`), else from the mirror whole
    vk::GpuBuffer col_buf[vk::kFramesInFlight]{};
    uint64_t col_at[vk::kFramesInFlight]{};
    uint32_t col_fi = 0;  // (the slot the frame's dispatch binds)
    uint64_t last_at = 0, prev_at = 0;  // (the colours' update at the last upload, at the one before it)
    std::vector<std::pair<uint32_t, uint32_t>> prev_dirty;  // (the last upload's dirty ranges and words)
    std::vector<uint32_t> prev_words;
    vk::GpuBuffer out_v[vk::kFramesInFlight]{};
    vk::GpuBuffer out_i[vk::kFramesInFlight]{};
    uint32_t active_fi = 0;
    uint64_t level_gen = 0;
    size_t colour_size = 0;
    uint64_t colour_updates = 0;
    const TfragLevel* colour_level = nullptr;
    size_t image_q = 0;
    std::vector<uint32_t> plan_words;
    std::unordered_map<uint32_t, uint32_t> plan_runs;
    size_t colour_words = 0;
    uint64_t refused = 0;
    // (a level's first dispatches timed, tfrag_gpu_dispatch: the level, the draws and milliseconds so far, the frames, done)
    uint64_t timed_gen = 0;
    uint32_t timed_draws = 0, timed_frames = 0;
    double timed_ms = 0.0, timed_worst = 0.0;
    bool timed_done = false;
} g_vk_gpu;
// (the GPU path given up for the rest of the session: its dispatch too slow, tfrag_gpu_dispatch; read by the game's thread too)
std::atomic<bool> g_vk_given_up{false};

// orig: (host) RAC1_TFRAG_CPU: 1 the tfrags made on the CPU, 0 on the GPU whatever the device (the automatic choices off), -1 unset
int tfrag_cpu_env() {
    static const int v = [] {
        const char* e = std::getenv("RAC1_TFRAG_CPU");
        return e == nullptr || *e == '\0' ? -1 : std::atoi(e) != 0 ? 1 : 0;
    }();
    return v;
}

void vk_tfrag_shutdown() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        if (g_vk_gpu.pipe != VK_NULL_HANDLE) vkDestroyPipeline(dev, g_vk_gpu.pipe, nullptr);
        if (g_vk_gpu.cs_mod != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_gpu.cs_mod, nullptr);
        if (g_vk_gpu.pipe_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g_vk_gpu.pipe_layout, nullptr);
        if (g_vk_gpu.set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(dev, g_vk_gpu.set_layout, nullptr);
    }
    vk::destroy_buffer_now(g_vk_gpu.img_buf);
    vk::destroy_buffer_now(g_vk_gpu.plan_buf);
    for (uint32_t i = 0; i < vk::kFramesInFlight; ++i) {
        vk::destroy_buffer_now(g_vk_gpu.col_buf[i]);
        vk::destroy_buffer_now(g_vk_gpu.out_v[i]);
        vk::destroy_buffer_now(g_vk_gpu.out_i[i]);
    }
    g_vk_gpu = VkGpu{};
}

// the bounds the program's loops run to (a plan holds at most 3 steps, kMaxPlanVerts vertices; a step's count is one of
// the image's lists, never more than the 0x148 qwords' four lanes): the program clamps to them too
constexpr uint32_t kMaxSteps = 4, kMaxStepN = 4 * 0x148, kImageQ = 0x148;

// orig: (host) a frame's draw checked against what the buffers hold before it is dispatched (a draw whose image, plan,
// colours, view, constants, runs or output slots lie outside them, or whose plan's loop counts exceed the program's bounds, is
// refused: the program's loops take their bounds from the plan, and a stale or wrong plan once hung the GPU - Veldin's plans read
// with Novalis's draws). Null when it is fine, else why not.
const char* draw_refused_common(const TfragGpuDraw& d, const TfragGpuFrame& f, size_t image_q,
                                const std::vector<uint32_t>& w, const std::unordered_map<uint32_t, uint32_t>& plan_runs,
                                size_t colour_words) {
    if (d.nq == 0 || d.image > image_q || d.nq > image_q - d.image) return "image";
    const auto pr = plan_runs.find(d.plan);
    if (pr == plan_runs.end()) return "plan (not a plan's first word)";
    const size_t P = d.plan;
    if (P + 3 > w.size()) return "plan header";
    const uint32_t nsteps = w[P] & 0xff, form = (w[P] >> 8) & 0xff, nv = w[P + 1], ntri = w[P + 2];
    if (nsteps > kMaxSteps || form > 4 || nv > kMaxPlanVerts || ntri > nv) return "plan counts";
    if (P + 3 + 3 * static_cast<size_t>(nsteps) + 2 * static_cast<size_t>(nv) > w.size()) return "plan size";
    for (uint32_t s = 0; s < nsteps; ++s)
        if ((w[P + 3 + 3 * s] & 0xff) > 3 || w[P + 3 + 3 * s + 2] > kMaxStepN) return "plan step";
    if (d.vbase > f.nverts || nv > f.nverts - d.vbase) return "vertex slots";
    if (d.ibase > f.nidx || 3 * ntri > f.nidx - d.ibase) return "triangle slots";
    if ((static_cast<size_t>(d.view) + 1) * 16 > f.vu_views.size()) return "VU view";
    if ((static_cast<size_t>(d.wview) + 1) * 16 > f.views.size()) return "world view";
    if ((static_cast<size_t>(d.consts) + 1) * 56 > f.consts.size()) return "constants";
    if (d.col_count > kImageQ || (d.colour != 0xffffffffu && static_cast<size_t>(d.colour) + d.col_count > colour_words)) return "colours";
    if (static_cast<size_t>(d.rbase) + pr->second > f.runs.size() / 4) return "runs";
    return nullptr;
}

const char* draw_refused(const TfragGpuDraw& d, const TfragGpuFrame& f) {
    if (vk::is_active()) {
        return draw_refused_common(d, f, g_vk_gpu.image_q, g_vk_gpu.plan_words, g_vk_gpu.plan_runs, g_vk_gpu.colour_words);
    }
    return draw_refused_common(d, f, g_gpu.image_q, g_gpu.plan_words, g_gpu.plan_runs, g_gpu.colour_words);
}

// (a frame's buffer is specified anew each frame - the previous frame's storage orphaned, so that writing it never waits for the GPU
// still reading the previous one; its size kept at the largest seen)
void upload(int i, const void* data, size_t bytes, GLenum usage) {
    glBindBuffer(kSsbo, g_gpu.buf[i]);
    const size_t want = std::max<size_t>(bytes, 16);
    g_gpu.cap[i] = usage == GL_STATIC_DRAW ? want : std::max(g_gpu.cap[i], want);
    glBufferData(kSsbo, static_cast<GLsizeiptr>(g_gpu.cap[i]), nullptr, usage);
    if (bytes > 0 && data != nullptr) glBufferSubData(kSsbo, 0, static_cast<GLsizeiptr>(bytes), data);
}

}  // namespace

// orig: (host) see mrp_tfrag_gpu.h
bool tfrag_gpu_init() {
    if (vk::is_active()) {
        if (g_vk_gpu.tried) return g_vk_gpu.ok && !g_vk_given_up.load(std::memory_order_relaxed);
        g_vk_gpu.tried = true;
        VkDevice dev = vk::device();
        if (dev == VK_NULL_HANDLE) return false;
        if (!vk::has_float64()) return false;  // (the shader's doubles: without shaderFloat64 the tfrags are made on the CPU, #113)
        // (the CPU path where the doubles are known to be slow - an Intel GPU of generation 9 or older: on a UHD 620 a level's
        // first frame went past Windows' 2 s limit and the device was lost, #113 - or when asked)
        if (tfrag_cpu_env() == 1 || (tfrag_cpu_env() == -1 && vk::float64_slow())) {
            std::fprintf(stderr, "[mrpw tfrag] the tfrags made on the CPU (%s; RAC1_TFRAG_CPU=0 for the GPU)\n",
                         tfrag_cpu_env() == 1 ? "RAC1_TFRAG_CPU=1" : "an Intel GPU of generation 9 or older: slow doubles");
            return false;
        }
        VkDescriptorSetLayoutBinding bindings[10]{};
        for (uint32_t i = 0; i < 10; ++i) {
            bindings[i].binding = i;
            bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            bindings[i].descriptorCount = 1;
            bindings[i].stageFlags = VK_SHADER_STAGE_COMPUTE_BIT;
        }
        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 10;
        dslci.pBindings = bindings;
        if (vkCreateDescriptorSetLayout(dev, &dslci, nullptr, &g_vk_gpu.set_layout) != VK_SUCCESS) return false;

        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &g_vk_gpu.set_layout;
        if (vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_gpu.pipe_layout) != VK_SUCCESS) return false;

        g_vk_gpu.cs_mod = vk::create_shader_module(vk::shaders::kMrpTfragCs, sizeof(vk::shaders::kMrpTfragCs));
        if (g_vk_gpu.cs_mod == VK_NULL_HANDLE) return false;
        g_vk_gpu.pipe = vk::create_compute_pipeline(g_vk_gpu.pipe_layout, g_vk_gpu.cs_mod);
        if (g_vk_gpu.pipe == VK_NULL_HANDLE) return false;

        vk::register_shutdown_callback(vk_tfrag_shutdown);
        g_vk_gpu.ok = true;
        return true;
    }
    if (g_gpu.tried) return g_gpu.ok;
    g_gpu.tried = true;
    if (tfrag_cpu_env() == 1) {
        std::fprintf(stderr, "[mrpw tfrag] the tfrags made on the CPU (RAC1_TFRAG_CPU=1)\n");
        return false;
    }
    p_dispatch = reinterpret_cast<PfnDispatchCompute>(glfwGetProcAddress("glDispatchCompute"));
    p_barrier = reinterpret_cast<PfnMemoryBarrier>(glfwGetProcAddress("glMemoryBarrier"));
    if (p_dispatch == nullptr || p_barrier == nullptr) return false;
    const GLuint sh = glCreateShader(kComputeShader);
    glShaderSource(sh, 1, &kCs, nullptr);
    glCompileShader(sh);
    GLint ok = 0;
    glGetShaderiv(sh, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetShaderInfoLog(sh, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrpw tfrag] compute shader: %s\n", log);
        glDeleteShader(sh);
        return false;
    }
    g_gpu.prog = glCreateProgram();
    glAttachShader(g_gpu.prog, sh);
    glLinkProgram(g_gpu.prog);
    glDeleteShader(sh);
    glGetProgramiv(g_gpu.prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[4096];
        glGetProgramInfoLog(g_gpu.prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrpw tfrag] compute link: %s\n", log);
        return false;
    }
    glGenBuffers(10, g_gpu.buf);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 10, g_gpu.buf, "MRP world: tfrag GPU buffers");
    g_gpu.ok = true;
    return true;
}

// orig: (host) see mrp_tfrag_gpu.h
bool tfrag_gpu_ready() { return vk::is_active() ? g_vk_gpu.ok && !g_vk_given_up.load(std::memory_order_relaxed) : g_gpu.ok; }

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_gpu_level(const TfragLevel& L) {
    if (vk::is_active()) {
        if (!g_vk_gpu.ok || g_vk_gpu.level_gen == L.generation) return;
        g_vk_gpu.level_gen = L.generation;
        vk::upload_buffer(g_vk_gpu.img_buf, L.images.data(), std::max<size_t>(L.images.size() * 16, 16),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        vk::upload_buffer(g_vk_gpu.plan_buf, L.plan_words.data(), std::max<size_t>(L.plan_words.size() * 4, 16),
                          VK_BUFFER_USAGE_STORAGE_BUFFER_BIT);
        g_vk_gpu.image_q = L.images.size();
        g_vk_gpu.plan_words = L.plan_words;
        g_vk_gpu.plan_runs.clear();
        for (const TfragEntryGpu& e : L.entries)
            for (const TfragPlan* p : {&e.far6, &e.mid8, &e.mida, &e.neare, &e.near10, &e.near14, &e.nearb})
                if (p->ok) g_vk_gpu.plan_runs[p->gpu_first] = static_cast<uint32_t>(p->runs.size());
        return;
    }
    // (by the build's generation, not the object's address - the renderer rebuilds the same object at a level change: the
    // next level's draws once ran on the previous level's images and plans, and the GPU hung)
    if (!g_gpu.ok || g_gpu.level_gen == L.generation) return;
    g_gpu.level_gen = L.generation;
    upload(1, L.images.data(), L.images.size() * 16, GL_STATIC_DRAW);
    upload(2, L.plan_words.data(), L.plan_words.size() * 4, GL_STATIC_DRAW);
    glBindBuffer(kSsbo, 0);
    g_gpu.image_q = L.images.size();
    g_gpu.plan_words = L.plan_words;
    g_gpu.plan_runs.clear();
    for (const TfragEntryGpu& e : L.entries)
        for (const TfragPlan* p : {&e.far6, &e.mid8, &e.mida, &e.neare, &e.near10, &e.near14, &e.nearb})
            if (p->ok) g_gpu.plan_runs[p->gpu_first] = static_cast<uint32_t>(p->runs.size());
}

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_gpu_colours(TfragColours& c) {
    if (vk::is_active()) {
        if (!g_vk_gpu.ok) return;
        const size_t bytes = std::max<size_t>(c.mirror.size() * 4, 16);
        const uint32_t fi = vk::frame_index() % vk::kFramesInFlight;
        g_vk_gpu.col_fi = fi;
        if (c.all_dirty || bytes != g_vk_gpu.colour_size || g_vk_gpu.colour_level != c.level)
            for (uint32_t i = 0; i < vk::kFramesInFlight; ++i) g_vk_gpu.col_at[i] = 0;  // (every slot written whole when its turn comes)
        vk::GpuBuffer& buf = g_vk_gpu.col_buf[fi];
        if (buf.buffer == VK_NULL_HANDLE || buf.size < bytes) {
            vk::destroy_buffer(buf);
            vk::create_buffer(bytes, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, buf, true);
            g_vk_gpu.col_at[fi] = 0;
        }
        auto apply = [&](const std::vector<std::pair<uint32_t, uint32_t>>& ranges, const std::vector<uint32_t>& words) {
            size_t at = 0;
            uint8_t* dst = static_cast<uint8_t*>(buf.mapped);
            for (const auto& [first, n] : ranges) {
                if (static_cast<size_t>(first + n) * 4 <= buf.size) std::memcpy(dst + static_cast<size_t>(first) * 4, words.data() + at, static_cast<size_t>(n) * 4);
                at += n;
            }
            c.words_uploaded += at;
        };
        const uint64_t at_slot = g_vk_gpu.col_at[fi];
        if (buf.mapped != nullptr) {
            if (at_slot != 0 && at_slot == g_vk_gpu.last_at) {
                apply(c.dirty, c.dirty_words);  // (the slot at the last upload: this one's changes)
            } else if (at_slot != 0 && at_slot == g_vk_gpu.prev_at) {
                apply(g_vk_gpu.prev_dirty, g_vk_gpu.prev_words);  // (the slot at the upload before: the last one's changes, then this one's)
                apply(c.dirty, c.dirty_words);
            } else {
                if (!c.mirror.empty()) std::memcpy(buf.mapped, c.mirror.data(), c.mirror.size() * 4);
                c.words_uploaded += c.mirror.size();
            }
        }
        g_vk_gpu.col_at[fi] = c.updates;
        g_vk_gpu.prev_at = g_vk_gpu.last_at;
        g_vk_gpu.last_at = c.updates;
        g_vk_gpu.prev_dirty.swap(c.dirty);
        g_vk_gpu.prev_words.swap(c.dirty_words);
        g_vk_gpu.colour_size = bytes;
        g_vk_gpu.colour_words = c.mirror.size();
        g_vk_gpu.colour_level = c.level;
        g_vk_gpu.colour_updates = c.updates;
        c.all_dirty = false;
        c.dirty.clear();
        c.dirty_words.clear();
        return;
    }
    if (!g_gpu.ok) return;
    glBindBuffer(kSsbo, g_gpu.buf[3]);
    const size_t bytes = std::max<size_t>(c.mirror.size() * 4, 16);
    if (c.all_dirty || bytes != g_gpu.colour_size) {
        glBufferData(kSsbo, static_cast<GLsizeiptr>(bytes), nullptr, GL_DYNAMIC_DRAW);
        if (!c.mirror.empty()) glBufferSubData(kSsbo, 0, static_cast<GLsizeiptr>(c.mirror.size() * 4), c.mirror.data());
        g_gpu.colour_size = bytes;
        g_gpu.colour_words = c.mirror.size();
        c.words_uploaded += c.mirror.size();
        c.all_dirty = false;
    } else {
        size_t at = 0;
        for (const auto& [first, n] : c.dirty) {
            glBufferSubData(kSsbo, static_cast<GLintptr>(first) * 4, static_cast<GLsizeiptr>(n) * 4, c.dirty_words.data() + at);
            at += n;
        }
        c.words_uploaded += at;
    }
    c.dirty.clear();
    c.dirty_words.clear();
    glBindBuffer(kSsbo, 0);
}

// orig: (host) see mrp_tfrag_gpu.h
uint32_t tfrag_gpu_vbo() { return g_gpu.buf[8]; }
// orig: (host) see mrp_tfrag_gpu.h
uint32_t tfrag_gpu_ibo() { return g_gpu.buf[9]; }
uint64_t tfrag_gpu_vk_vbo() {
    return reinterpret_cast<uint64_t>(g_vk_gpu.out_v[g_vk_gpu.active_fi].buffer);
}
uint64_t tfrag_gpu_vk_ibo() {
    return reinterpret_cast<uint64_t>(g_vk_gpu.out_i[g_vk_gpu.active_fi].buffer);
}

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_gpu_dispatch(const TfragGpuFrame& f, uint32_t vertex_bytes) {
    if (vk::is_active()) {
        if (!g_vk_gpu.ok || f.draws.empty()) return;
        auto pw = [](size_t i) { return i < g_vk_gpu.plan_words.size() ? g_vk_gpu.plan_words[i] : 0u; };
        static std::vector<TfragGpuDraw> kept;
        kept.clear();
        uint32_t refused = 0;
        for (const TfragGpuDraw& d : f.draws) {
            if (const char* why = draw_refused(d, f); why != nullptr) {
                if (g_vk_gpu.refused++ < 8)
                    std::fprintf(stderr, "[mrpw tfrag] VK GPU draw refused (%s): image %u nq %u plan %u view %u consts %u wview %u colour %u+%u runs %u vbase %u ibase %u (frame: %zu draws, %u vertices, %u indices; level: %zu qwords, %zu plan words; the plan's words %08x %08x %08x)\n",
                                 why, d.image, d.nq, d.plan, d.view, d.consts, d.wview, d.colour, d.col_count, d.rbase, d.vbase, d.ibase, f.draws.size(), f.nverts, f.nidx,
                                 g_vk_gpu.image_q, g_vk_gpu.plan_words.size(), pw(d.plan), pw(d.plan + 1), pw(d.plan + 2));
                ++refused;
                continue;
            }
            kept.push_back(d);
        }
        const uint32_t fi = vk::frame_index() % vk::kFramesInFlight;
        g_vk_gpu.active_fi = fi;
        const size_t want_v = std::max<size_t>(static_cast<size_t>(f.nverts) * vertex_bytes, 64);
        const size_t want_i = std::max<size_t>(static_cast<size_t>(f.nidx) * 4, 64);
        if (g_vk_gpu.out_v[fi].size < want_v) {
            vk::destroy_buffer(g_vk_gpu.out_v[fi]);
            vk::create_buffer(want_v,
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_VERTEX_BUFFER_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, g_vk_gpu.out_v[fi]);
        }
        if (g_vk_gpu.out_i[fi].size < want_i) {
            vk::destroy_buffer(g_vk_gpu.out_i[fi]);
            vk::create_buffer(want_i,
                              VK_BUFFER_USAGE_STORAGE_BUFFER_BIT | VK_BUFFER_USAGE_INDEX_BUFFER_BIT |
                                  VK_BUFFER_USAGE_TRANSFER_DST_BIT | VK_BUFFER_USAGE_TRANSFER_SRC_BIT,
                              VK_MEMORY_PROPERTY_DEVICE_LOCAL_BIT, g_vk_gpu.out_i[fi]);
        }
        if (kept.empty()) return;

        auto alloc_copy = [](const void* src, size_t bytes) -> vk::RingAlloc {
            const size_t sz = std::max<size_t>(bytes, 16);
            vk::RingAlloc r = vk::alloc_ring(sz, 16);
            if (r.ptr != nullptr) {
                if (bytes > 0 && src != nullptr) std::memcpy(r.ptr, src, bytes);
                else std::memset(r.ptr, 0, sz);
            }
            return r;
        };
        const vk::RingAlloc r_draws = alloc_copy(kept.data(), kept.size() * sizeof(TfragGpuDraw));
        const vk::RingAlloc r_views = alloc_copy(f.views.data(), f.views.size() * 8);
        const vk::RingAlloc r_vuviews = alloc_copy(f.vu_views.data(), f.vu_views.size() * 4);
        const vk::RingAlloc r_consts = alloc_copy(f.consts.data(), f.consts.size() * 4);
        const vk::RingAlloc r_runs = alloc_copy(f.runs.data(), f.runs.size() * 4);

        vk::GpuBuffer& col_buf = g_vk_gpu.col_buf[g_vk_gpu.col_fi];  // (the slot tfrag_gpu_colours wrote this frame)
        if (col_buf.buffer == VK_NULL_HANDLE) {
            vk::create_buffer(16, VK_BUFFER_USAGE_STORAGE_BUFFER_BIT,
                              VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT,
                              col_buf, true);
        }

        VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_gpu.set_layout);
        VkDescriptorBufferInfo dbis[10]{};
        dbis[0] = {r_draws.buffer, r_draws.offset, std::max<VkDeviceSize>(kept.size() * sizeof(TfragGpuDraw), 16)};
        dbis[1] = {g_vk_gpu.img_buf.buffer, 0, VK_WHOLE_SIZE};
        dbis[2] = {g_vk_gpu.plan_buf.buffer, 0, VK_WHOLE_SIZE};
        dbis[3] = {col_buf.buffer, 0, VK_WHOLE_SIZE};
        dbis[4] = {r_views.buffer, r_views.offset, std::max<VkDeviceSize>(f.views.size() * 8, 16)};
        dbis[5] = {r_vuviews.buffer, r_vuviews.offset, std::max<VkDeviceSize>(f.vu_views.size() * 4, 16)};
        dbis[6] = {r_consts.buffer, r_consts.offset, std::max<VkDeviceSize>(f.consts.size() * 4, 16)};
        dbis[7] = {r_runs.buffer, r_runs.offset, std::max<VkDeviceSize>(f.runs.size() * 4, 16)};
        dbis[8] = {g_vk_gpu.out_v[fi].buffer, 0, VK_WHOLE_SIZE};
        dbis[9] = {g_vk_gpu.out_i[fi].buffer, 0, VK_WHOLE_SIZE};

        VkWriteDescriptorSet writes[10]{};
        for (uint32_t i = 0; i < 10; ++i) {
            writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            writes[i].dstSet = ds;
            writes[i].dstBinding = i;
            writes[i].descriptorCount = 1;
            writes[i].descriptorType = VK_DESCRIPTOR_TYPE_STORAGE_BUFFER;
            writes[i].pBufferInfo = &dbis[i];
        }
        vkUpdateDescriptorSets(vk::device(), 10, writes, 0, nullptr);

        vk::end_rendering();
        VkCommandBuffer cb = vk::cmd();
        if (refused != 0 && f.nidx > 0) {
            vkCmdFillBuffer(cb, g_vk_gpu.out_i[fi].buffer, 0, static_cast<VkDeviceSize>(f.nidx) * 4, 0);
            VkMemoryBarrier fill_bar{};
            fill_bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
            fill_bar.srcAccessMask = VK_ACCESS_TRANSFER_WRITE_BIT;
            fill_bar.dstAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
            vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_TRANSFER_BIT, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT, 0, 1, &fill_bar, 0, nullptr, 0, nullptr);
        }
        if (g_vk_gpu.timed_gen != g_vk_gpu.level_gen) {  // (a new level: timed again)
            g_vk_gpu.timed_gen = g_vk_gpu.level_gen;
            g_vk_gpu.timed_draws = g_vk_gpu.timed_frames = 0;
            g_vk_gpu.timed_ms = g_vk_gpu.timed_worst = 0.0;
            g_vk_gpu.timed_done = false;
        }
        if (!g_vk_gpu.timed_done) {
            // a level's first dispatches (until kSample draws in full chunks, or kFrames frames): in chunks of kChunk draws, each its
            // own submit waited for and timed on the GPU (two timestamps; a chunk less than full not counted - a few draws' time is
            // the GPU's start, not theirs), so that no submit of a slow GPU holds a whole frame's tfrags:
            // Windows resets a GPU whose work runs past about 2 s (#113). Too slow - the cost per draw, over a big level's frame
            // (kFrameDraws), past kSlowMs - and the tfrags are made on the CPU from the next frame on, for the rest of the session
            // (RAC1_TFRAG_CPU=0: never)
            constexpr uint32_t kChunk = 64, kSample = 512, kFrames = 30, kFrameDraws = 1500;  // (kChunk records: 4 KB, aligned for any device)
            constexpr double kSlowMs = 12.0;
            const uint32_t n = static_cast<uint32_t>(kept.size());
            for (uint32_t first = 0; first < n; first += kChunk) {
                const uint32_t cn = std::min(kChunk, n - first);
                VkDescriptorSet cds = vk::alloc_descriptor_set(g_vk_gpu.set_layout);
                VkDescriptorBufferInfo cdbis[10];
                std::copy(dbis, dbis + 10, cdbis);
                cdbis[0] = {r_draws.buffer, r_draws.offset + static_cast<VkDeviceSize>(first) * sizeof(TfragGpuDraw), static_cast<VkDeviceSize>(cn) * sizeof(TfragGpuDraw)};
                for (uint32_t i = 0; i < 10; ++i) {
                    writes[i].dstSet = cds;
                    writes[i].pBufferInfo = &cdbis[i];
                }
                vkUpdateDescriptorSets(vk::device(), 10, writes, 0, nullptr);
                const double c = vk::timed_submit([&](VkCommandBuffer tcb) {
                    vkCmdBindPipeline(tcb, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_gpu.pipe);
                    vkCmdBindDescriptorSets(tcb, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_gpu.pipe_layout, 0, 1, &cds, 0, nullptr);
                    vkCmdDispatch(tcb, cn, 1, 1);
                });
                if (c < 0.0) {  // (no timestamps: nothing to decide by)
                    g_vk_gpu.timed_done = true;
                    continue;
                }
                g_vk_gpu.timed_worst = std::max(g_vk_gpu.timed_worst, c);
                if (cn == kChunk) {
                    g_vk_gpu.timed_ms += c;
                    g_vk_gpu.timed_draws += cn;
                }
            }
            if (g_vk_gpu.timed_draws >= kSample || ++g_vk_gpu.timed_frames >= kFrames) {
                g_vk_gpu.timed_done = true;
                const double per = g_vk_gpu.timed_draws > 0 ? g_vk_gpu.timed_ms / g_vk_gpu.timed_draws : 0.0;
                const bool slow = tfrag_cpu_env() != 0 && g_vk_gpu.timed_draws > 0 && per * kFrameDraws > kSlowMs;
                std::fprintf(stderr, "[mrpw tfrag] the level's first dispatches timed: %u draws in full chunks of %u, %.2f ms (the longest chunk %.2f ms; %.4f ms a draw, %.1f ms for %u)%s\n",
                             g_vk_gpu.timed_draws, kChunk, g_vk_gpu.timed_ms, g_vk_gpu.timed_worst, per, per * kFrameDraws, kFrameDraws,
                             slow ? ": too slow, the tfrags made on the CPU from now on (RAC1_TFRAG_CPU=0 for the GPU)" : "");
                if (slow) g_vk_given_up.store(true, std::memory_order_relaxed);
            }
            cb = vk::cmd();
        } else {
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_gpu.pipe);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_COMPUTE, g_vk_gpu.pipe_layout, 0, 1, &ds, 0, nullptr);
            vkCmdDispatch(cb, static_cast<uint32_t>(kept.size()), 1, 1);
        }

        VkMemoryBarrier bar{};
        bar.sType = VK_STRUCTURE_TYPE_MEMORY_BARRIER;
        bar.srcAccessMask = VK_ACCESS_SHADER_WRITE_BIT;
        bar.dstAccessMask = VK_ACCESS_VERTEX_ATTRIBUTE_READ_BIT | VK_ACCESS_INDEX_READ_BIT | VK_ACCESS_TRANSFER_READ_BIT;
        vkCmdPipelineBarrier(cb, VK_PIPELINE_STAGE_COMPUTE_SHADER_BIT,
                             VK_PIPELINE_STAGE_VERTEX_INPUT_BIT | VK_PIPELINE_STAGE_TRANSFER_BIT, 0, 1, &bar, 0, nullptr, 0, nullptr);
        return;
    }
    if (!g_gpu.ok || f.draws.empty()) return;
    // every draw checked before the GPU runs it; the refused ones are left out, their triangle slots zeroed (degenerate)
    auto pw = [](size_t i) { return i < g_gpu.plan_words.size() ? g_gpu.plan_words[i] : 0u; };
    static std::vector<TfragGpuDraw> kept;
    kept.clear();
    uint32_t refused = 0;
    for (const TfragGpuDraw& d : f.draws) {
        if (const char* why = draw_refused(d, f); why != nullptr) {
            if (g_gpu.refused++ < 8)
                std::fprintf(stderr, "[mrpw tfrag] GPU draw refused (%s): image %u nq %u plan %u view %u consts %u wview %u colour %u+%u runs %u vbase %u ibase %u (frame: %zu draws, %u vertices, %u indices; level: %zu qwords, %zu plan words; the plan's words %08x %08x %08x)\n",
                             why, d.image, d.nq, d.plan, d.view, d.consts, d.wview, d.colour, d.col_count, d.rbase, d.vbase, d.ibase, f.draws.size(), f.nverts, f.nidx,
                             g_gpu.image_q, g_gpu.plan_words.size(), pw(d.plan), pw(d.plan + 1), pw(d.plan + 2));
            ++refused;
            continue;
        }
        kept.push_back(d);
    }
    upload(0, kept.data(), kept.size() * sizeof(TfragGpuDraw), GL_STREAM_DRAW);
    upload(4, f.views.data(), f.views.size() * 8, GL_STREAM_DRAW);
    upload(5, f.vu_views.data(), f.vu_views.size() * 4, GL_STREAM_DRAW);
    upload(6, f.consts.data(), f.consts.size() * 4, GL_STREAM_DRAW);
    upload(7, f.runs.data(), f.runs.size() * 4, GL_STREAM_DRAW);
    upload(8, nullptr, static_cast<size_t>(f.nverts) * vertex_bytes, GL_STREAM_DRAW);
    if (refused == 0) {
        upload(9, nullptr, static_cast<size_t>(f.nidx) * 4, GL_STREAM_DRAW);
    } else {
        static std::vector<uint32_t> zero;
        zero.assign(f.nidx, 0);
        upload(9, zero.data(), zero.size() * 4, GL_STREAM_DRAW);
    }
    if (kept.empty()) return;
    for (GLuint i = 0; i < 10; ++i) glBindBufferBase(kSsbo, i, g_gpu.buf[i]);
    glUseProgram(g_gpu.prog);
    p_dispatch(static_cast<GLuint>(kept.size()), 1, 1);
    p_barrier(kVertexAttribBarrier | kElementBarrier | kBufferUpdateBarrier);
    glUseProgram(0);
    for (GLuint i = 0; i < 10; ++i) glBindBufferBase(kSsbo, i, 0);
}

// orig: (host) see mrp_tfrag_gpu.h
void tfrag_gpu_readback(const TfragGpuFrame& f, uint32_t vertex_bytes, std::vector<uint8_t>& verts, std::vector<uint32_t>& idx) {
    verts.assign(static_cast<size_t>(f.nverts) * vertex_bytes, 0);
    idx.assign(f.nidx, 0);
    if (vk::is_active()) {
        if (!g_vk_gpu.ok) return;
        const uint32_t fi = g_vk_gpu.active_fi;
        vk::flush_cmd();
        auto read_buf = [](const vk::GpuBuffer& src, void* dst, size_t bytes) {
            if (bytes == 0 || src.buffer == VK_NULL_HANDLE) return;
            vk::GpuBuffer stg{};
            if (!vk::create_buffer(bytes, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                   VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, stg, true))
                return;
            vk::submit_immediate([&](VkCommandBuffer cb) {
                VkBufferCopy region{0, 0, bytes};
                vkCmdCopyBuffer(cb, src.buffer, stg.buffer, 1, &region);
            });
            if (stg.mapped != nullptr) std::memcpy(dst, stg.mapped, bytes);
            vk::destroy_buffer_now(stg);
        };
        read_buf(g_vk_gpu.out_v[fi], verts.data(), verts.size());
        read_buf(g_vk_gpu.out_i[fi], idx.data(), idx.size() * 4);
        return;
    }
    if (!g_gpu.ok) return;
    glBindBuffer(kSsbo, g_gpu.buf[8]);
    if (!verts.empty()) glGetBufferSubData(kSsbo, 0, static_cast<GLsizeiptr>(verts.size()), verts.data());
    glBindBuffer(kSsbo, g_gpu.buf[9]);
    if (!idx.empty()) glGetBufferSubData(kSsbo, 0, static_cast<GLsizeiptr>(idx.size() * 4), idx.data());
    glBindBuffer(kSsbo, 0);
}

}  // namespace rac1::mrpw
