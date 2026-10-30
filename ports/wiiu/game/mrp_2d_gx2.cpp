// The MRP's 2D drawn with GX2 on the Wii U (how a caller uses it: mrp_2d_gx2.h). A port of mrp_2d.cpp's OpenGL submit (gl_submit)
// over the same CPU build, read through mrp_2d_backend.inc (mrp2d::build: the vertices in frame pixels, the runs in the chain's
// order with their state), with what the GS replica's GX2 backend (gs_draw_gx2.cpp) learned on the console:
//
// - the vertices: the build's, mapped to clip space here (mrp_2d.cpp's VS: the frame's x about its centre by the run's x scale, a
//   render target's GS pixels; the GS's row 0 at GX2's top in both, a target's row 0 its texture's row 0), plus their window pixel
//   position (the DATE test's, gl_FragCoord in mrp_2d.cpp), 64 bytes each, one ring allocation a call, a run drawn from its first
//   vertex (GX2DrawEx's first vertex); 32-bit floats as the PPC writes them;
// - per run the pixel shader's uniform block (std140, written with each 32-bit word swapped: GX2 reads uniform blocks as
//   little-endian words), the GS's state as mrp_2d.cpp's uniforms: TME / TFX / TCC, the alpha test and its fail modes, FGE and
//   FOGCOL, the raw alpha of a render target, the screen-reading modes, the mip levels;
// - the textures: one GX2 texture per decoded image (TexImage::id: an image never changes under its id), linear, the texels'
//   bytes as the PPC holds them with the component map putting them back (gs_draw_gx2.cpp's make_texture); returned to a pool by
//   size (g_pool) when the image is gone (a texture the game changed) or unused for kTexKeep calls, refilled in place for the next
//   image of that size, and destroyed after kTexKeep calls unused in the pool, or past kTexCap, at a level change all
//   (mrp_2d_clear_caches);
// - the render targets (a FRAME page the frame samples): a colour buffer per (page, width, height) as large as the build says
//   (Item::rt_w / rt_h, scaled by RAC1_2D_RT_SCALE, noted back with mrp2d::rt_size_note), cleared when made or when a page switches
//   between sizes, kept across frames, the raw GS alpha in them; a run that blends with the source alpha into one is drawn twice,
//   the colour then the raw alpha (gs_draw_gx2.cpp: GX2 has no second output here);
// - the primitives that read the screen (Item::alone: DATE, the frame sampled as a texture): the colour buffer's box around them
//   copied first (GX2CopySurfaceEx) into a texture of the screen's size, which the pixel shader reads with texelFetch (the copy's
//   rows from the top: its y flipped from the build's GL rows); the copy changes the GPU's state, so the context and this call's
//   state are set again after it.
//
// Not drawn (yet): a render target's own depth (mrp_2d.cpp's Item::rt_depth: its Z test - ZTST NEVER draws nothing, the others are
// drawn untested, as gs_draw_gx2.cpp), the host's 3D into a target (type 8: the menus' item models, not ported), the frame's stored
// picture (type 7: no Viewport::screen_picture here), the game's projected 3D on the GIF path (left out by the build, no host depth:
// BuildOptions::drop_gif_3d). The AFAIL split needs Z writes on: none here. Host code.
#include "mrp_2d_gx2.h"

#include "gs_draw_gx2.h"
#include "mrp_world_gx2.h"

#include <game/render/mrp_2d.h>
#include <game/render/mrp_2d_backend.inc>

#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <whb/gfx.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <string>
#include <tuple>
#include <unordered_map>
#include <utility>
#include <vector>

#include "gx2_counters.h"  // (after every GX2 header: the frame's GX2 work counted)

namespace rac1 {

namespace {

using mrp2d::Built;
using mrp2d::Item;
using mrp2d::Key;

// The vertex as the shaders read it (ports/wiiu/shaders/mrp_2d.vert)
struct GV {
    float x, y;  // clip space
    float r, g, b, a;
    float s, t, q;
    float f;
    float c0, c1, c2, c3;
    float wx, wy;  // the window pixel position, from the colour buffer's top left
};
static_assert(sizeof(GV) == 64);

// The pixel shader's uniform block (mrp_2d.frag's Mrp2dPixel, std140)
struct alignas(16) Block {
    float fog_col[4];
    int32_t a[4];  // TME, TFX, TCC, ATE
    int32_t b[4];  // ATST, AREF, AFAIL, FGE
    int32_t c[4];  // the raw GS alpha written, the AFAIL pass, the frame sampled, DATE
    int32_t d[4];  // DATM, mip levels after the first, the first level, LCM
    int32_t e[4];  // MXL, the window's height, 0, 0
    float mip[4];  // K, 2^L
    float scr_map[4];
    bool operator==(const Block& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};
static_assert(sizeof(Block) == 128);

bool g_ready = false;
WHBGfxShaderGroup g_sh{};
uint32_t g_block_loc = 0, g_block_size = sizeof(Block);
// the samplers' locations: uTexture, uScreen, uMip1..3
uint32_t g_smp_loc[5] = {0, 1, 2, 3, 4};

// The vertices and blocks, appended to a ring in MEM2 (gs_draw_gx2.cpp's): before the ring wraps over what the GPU may still read the
// CPU waits for it only if the current frame's allocations exceed half the ring.
const bool g_ring_nowait = std::getenv("RAC1_WIIU_RING_NOWAIT") == nullptr || std::atoi(std::getenv("RAC1_WIIU_RING_NOWAIT")) != 0;
constexpr uint32_t kRingBytes = 8u << 20;
uint8_t* g_ring = nullptr;
uint32_t g_ring_off = 0;
Gx2RingUse g_ring_use;  // (Gx2RingUse: this host frame's bytes and the frame before's)
uint64_t g_ring_wraps = 0;

void* ring_alloc(uint32_t bytes, uint32_t align) {
    uint32_t off = (g_ring_off + align - 1) & ~(align - 1);
    if (bytes > kRingBytes) return nullptr;
    if (off + bytes > kRingBytes) {
        g_ring_use.cur += (kRingBytes - g_ring_off) + bytes;
        if (!g_ring_nowait || g_ring_use.must_wait(kRingBytes)) {
            GX2DrawDone();
            g_ring_use.waited(bytes);
        }
        off = 0;
        ++g_ring_wraps;
    } else {
        g_ring_use.cur += (off - g_ring_off) + bytes;
    }
    g_ring_off = off + bytes;
    return g_ring + off;
}

// ---- RAC1_2D_TEX_STATS=N (testing): the GPU textures' churn, a line every N calls ----

struct TexStats {
    uint64_t made = 0, reused = 0, freed = 0, rt_made = 0, calls = 0;
    int64_t bytes = 0, peak = 0, rt_bytes = 0;
    std::map<std::string, uint64_t> src;            // made / reused since the last line, by source
    std::unordered_map<uint64_t, std::string> tag;  // image id -> its source
};
TexStats g_ts;
const int g_ts_every = [] {
    const char* e = std::getenv("RAC1_2D_TEX_STATS");
    return e == nullptr ? 0 : std::max(1, std::atoi(e));
}();

void ts_tag(const gs::TexImage* img, uint64_t tex0) {
    if (g_ts_every == 0 || img == nullptr || g_ts.tag.count(img->id) != 0) return;
    char b[64];
    std::snprintf(b, sizeof b, "psm%02x %dx%d tbp%04x", static_cast<unsigned>((tex0 >> 20) & 0x3f), img->w, img->h, static_cast<unsigned>(tex0 & 0x3fff));
    g_ts.tag[img->id] = b;
}

void ts_made(const gs::TexImage* img, bool reused) {
    if (g_ts_every == 0) return;
    if (reused) {
        ++g_ts.reused;
    } else {
        ++g_ts.made;
        g_ts.bytes += static_cast<int64_t>(img->w) * img->h * 4;
        g_ts.peak = std::max(g_ts.peak, g_ts.bytes + g_ts.rt_bytes);
    }
    const auto t = g_ts.tag.find(img->id);
    ++g_ts.src[(t != g_ts.tag.end() ? t->second : std::string("?")) + (reused ? " reused" : " made")];
}

void ts_freed(int w, int h) {
    if (g_ts_every == 0) return;
    ++g_ts.freed;
    g_ts.bytes -= static_cast<int64_t>(w) * h * 4;
}

void ts_rt(int64_t before, int64_t after) {
    if (g_ts_every == 0) return;
    ++g_ts.rt_made;
    g_ts.rt_bytes += after - before;
    g_ts.peak = std::max(g_ts.peak, g_ts.bytes + g_ts.rt_bytes);
}

void ts_print(size_t held, size_t pool_count) {
    if (g_ts_every == 0 || ++g_ts.calls % static_cast<uint64_t>(g_ts_every) != 0) return;
    std::printf("[2d-tex] call %llu: made %llu reused %llu destroyed %llu rt-made %llu | held %zu pooled %zu, %.1f MB + targets %.1f MB (peak %.1f MB)\n",
                static_cast<unsigned long long>(g_ts.calls), static_cast<unsigned long long>(g_ts.made), static_cast<unsigned long long>(g_ts.reused),
                static_cast<unsigned long long>(g_ts.freed), static_cast<unsigned long long>(g_ts.rt_made), held, pool_count,
                static_cast<double>(g_ts.bytes) / 1048576.0, static_cast<double>(g_ts.rt_bytes) / 1048576.0, static_cast<double>(g_ts.peak) / 1048576.0);
    for (const auto& [s, n] : g_ts.src) std::printf("[2d-tex]   %s: %llu\n", s.c_str(), static_cast<unsigned long long>(n));
    g_ts.src.clear();
    if (g_ts.tag.size() > 65536) g_ts.tag.clear();
    std::fflush(stdout);
}

// ---- the textures ----

constexpr size_t kTexCap = 48u << 20;
constexpr uint64_t kTexKeep = 600;  // (mrp_2d.cpp's: unused for this many calls, given back to the pool / destroyed)
struct TexEntry {
    GX2Texture tex{};
    uint32_t bytes = 0;
    uint64_t last_use = 0;
    int w = 0, h = 0;
    std::weak_ptr<const gs::TexImage> src;  // (empty: an image nobody holds, kept by its use alone)
    bool owned = false;
};
struct FreeTex {
    GX2Texture tex{};
    uint32_t bytes = 0;
    uint64_t since = 0;     // the call it entered the pool
    uint64_t last_use = 0;  // the call it was last sampled by the GPU
};
std::unordered_map<uint64_t, TexEntry> g_tex;             // by the image's id
std::map<std::pair<int, int>, std::vector<FreeTex>> g_pool;  // by (w, h)
size_t g_tex_bytes = 0;
uint64_t g_call = 0;
GX2Texture g_white{}, g_black{};

// the calls the GPU may still be drawing from (a texture last sampled by one of them waited for before it is written again or freed): the
// frame before's with one frame in flight, and ((2026-11-01) RAC1_WIIU_FRAMES_IN_FLIGHT=2) the frame before last's - a call or two
// (the GamePad's) a frame
uint64_t gpu_calls() { return g_gx2_in_flight > 1 ? 4 : 2; }

size_t pooled() {
    size_t n = 0;
    for (const auto& [k, l] : g_pool) n += l.size();
    return n;
}

void fill_texture(GX2Texture& t, int w, int h, const uint32_t* src) {
    GX2Surface& s = t.surface;
    auto* dst = static_cast<uint32_t*>(s.image);
    for (int y = 0; y < h; ++y) std::memcpy(dst + static_cast<size_t>(y) * s.pitch, src + static_cast<size_t>(y) * w, static_cast<size_t>(w) * 4);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, s.image, s.imageSize);
}

// a w x h texture of `src`'s texels (rows of w; r | g << 8 | b << 16 | a << 24, the bytes in memory a, b, g, r on the PPC: the
// component map puts them back - gs_draw_gx2.cpp's) in MEM2, linear; false: no memory for it
bool make_texture(GX2Texture& t, int w, int h, const uint32_t* src) {
    t = GX2Texture{};
    GX2Surface& s = t.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = static_cast<uint32_t>(w);
    s.height = static_cast<uint32_t>(h);
    s.depth = 1;
    s.mipLevels = 1;
    s.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    s.aa = GX2_AA_MODE1X;
    s.use = GX2_SURFACE_USE_TEXTURE;
    s.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    s.swizzle = 0;
    t.viewFirstMip = 0;
    t.viewNumMips = 1;
    t.viewFirstSlice = 0;
    t.viewNumSlices = 1;
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_A, GX2_SQ_SEL_B, GX2_SQ_SEL_G, GX2_SQ_SEL_R);
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&t);
    s.image = MEMAllocFromDefaultHeapEx(s.imageSize, static_cast<int32_t>(s.alignment));
    if (s.image == nullptr) return false;
    fill_texture(t, w, h, src);
    return true;
}

void tex_free(GX2Texture& t) {
    if (t.surface.image != nullptr) MEMFreeToDefaultHeap(t.surface.image);
    t.surface.image = nullptr;
}

// the pool's unused textures first, then the least recently used past kTexCap, never one this call used; the GPU waited for first
void tex_evict_cap() {
    if (g_tex_bytes <= kTexCap) return;
    bool waited = false;
    for (auto pt = g_pool.begin(); pt != g_pool.end() && g_tex_bytes > kTexCap * 3 / 4;) {
        auto& l = pt->second;
        while (!l.empty() && g_tex_bytes > kTexCap * 3 / 4) {
            FreeTex f = l.front();
            l.erase(l.begin());
            if (!waited && f.last_use + gpu_calls() > g_call) {
                GX2DrawDone();
                waited = true;
            }
            ts_freed(pt->first.first, pt->first.second);
            tex_free(f.tex);
            g_tex_bytes -= f.bytes;
        }
        pt = l.empty() ? g_pool.erase(pt) : std::next(pt);
    }
    if (g_tex_bytes <= kTexCap * 3 / 4) return;
    std::vector<std::pair<uint64_t, uint64_t>> order;
    for (const auto& [id, e] : g_tex)
        if (e.last_use < g_call) order.emplace_back(e.last_use, id);
    std::sort(order.begin(), order.end());
    for (const auto& [use, id] : order) {
        if (g_tex_bytes <= kTexCap * 3 / 4) break;
        if (!waited) {
            GX2DrawDone();
            waited = true;
        }
        auto it = g_tex.find(id);
        ts_freed(it->second.w, it->second.h);
        tex_free(it->second.tex);
        g_tex_bytes -= it->second.bytes;
        g_tex.erase(it);
    }
}

// after each call (mrp_2d.cpp's evict): the textures of the images gone given back to the pool; every 64 calls those unused for
// kTexKeep calls too, and the pool's unused for as long destroyed
void tex_evict() {
    const bool old = g_call % 64 == 0;
    for (auto it = g_tex.begin(); it != g_tex.end();) {
        TexEntry& t = it->second;
        if ((t.owned && t.src.expired()) || (old && g_call - t.last_use > kTexKeep)) {
            g_pool[{t.w, t.h}].push_back(FreeTex{t.tex, t.bytes, g_call, t.last_use});
            it = g_tex.erase(it);
        } else {
            ++it;
        }
    }
    if (!old) return;
    bool waited = false;
    for (auto pt = g_pool.begin(); pt != g_pool.end();) {
        auto& l = pt->second;
        for (size_t i = 0; i < l.size();) {
            if (g_call - l[i].since > kTexKeep) {
                if (!waited && l[i].last_use + gpu_calls() > g_call) {
                    GX2DrawDone();
                    waited = true;
                }
                ts_freed(pt->first.first, pt->first.second);
                tex_free(l[i].tex);
                g_tex_bytes -= l[i].bytes;
                l.erase(l.begin() + static_cast<long>(i));
            } else {
                ++i;
            }
        }
        pt = l.empty() ? g_pool.erase(pt) : std::next(pt);
    }
}

// the GX2 texture of a decoded image, made at its first use or refilled in place from the pool when one of its size is free
const GX2Texture* texture_of(const mrp2d::TexRef& r) {
    const gs::TexImage* img = r.img;
    if (img == nullptr || img->w <= 0 || img->h <= 0 || img->px.size() < static_cast<size_t>(img->w) * static_cast<size_t>(img->h)) return nullptr;
    auto it = g_tex.find(img->id);
    if (it != g_tex.end()) {
        it->second.last_use = g_call;
        if (!it->second.owned && r.own != nullptr && *r.own != nullptr) {
            it->second.src = *r.own;
            it->second.owned = true;
        }
        return &it->second.tex;
    }
    TexEntry e;
    e.w = img->w;
    e.h = img->h;
    if (r.own != nullptr && *r.own != nullptr) {
        e.src = *r.own;
        e.owned = true;
    }
    const auto pool = g_pool.find({img->w, img->h});
    if (pool != g_pool.end() && !pool->second.empty()) {
        FreeTex f = pool->second.front();
        pool->second.erase(pool->second.begin());
        if (pool->second.empty()) g_pool.erase(pool);
        if (f.last_use + gpu_calls() > g_call) GX2DrawDone();
        fill_texture(f.tex, img->w, img->h, img->px.data());
        e.tex = f.tex;
        e.bytes = f.bytes;
        e.last_use = g_call;
        TexEntry& slot = g_tex[img->id];
        slot = e;
        ts_made(img, true);
        return &slot.tex;
    }
    if (!make_texture(e.tex, img->w, img->h, img->px.data())) {
        tex_evict_cap();
        if (!make_texture(e.tex, img->w, img->h, img->px.data())) return nullptr;
    }
    e.bytes = e.tex.surface.imageSize;
    e.last_use = g_call;
    g_tex_bytes += e.bytes;
    TexEntry& slot = g_tex[img->id];
    slot = e;
    ts_made(img, false);
    tex_evict_cap();
    return &g_tex.find(img->id)->second.tex;
}

// ---- the render targets: a colour buffer per (frame page, GS width, GS height), its texture a view of the same surface ----

int g_rt_s = 1;  // GPU pixels per GS pixel (1: the GS's own; RAC1_2D_RT_SCALE)

int rt_scale_for(const gs::Viewport& v, float fb_h) {
    static const int s_env = [] {
        const char* e = std::getenv("RAC1_2D_RT_SCALE");
        return e == nullptr ? 1 : std::clamp(std::atoi(e), 0, 8);
    }();
    if (s_env != 0) return s_env;
    return std::clamp(static_cast<int>(std::ceil(static_cast<float>(v.h) / std::max(fb_h, 1.0f) - 1.0f / 64.0f)), 1, 8);
}

struct RtKey {
    uint32_t fbp = 0;
    int w = 0, h = 0;  // GS pixels
    bool operator<(const RtKey& o) const { return std::tie(fbp, w, h) < std::tie(o.fbp, o.w, o.h); }
};
struct Rt {
    GX2ColorBuffer cb{};
    GX2Texture tex{};
    uint32_t bytes = 0;
    int w = 0, h = 0;  // GS pixels
    int s = 1;         // GPU pixels per GS pixel (g_rt_s)
};
std::map<RtKey, Rt> g_rt;
std::map<uint32_t, std::pair<int, int>> g_rt_cur;  // page -> current (w, h) in GS pixels

void rt_free(Rt& r) {
    if (r.cb.surface.image != nullptr) MEMFreeToDefaultHeap(r.cb.surface.image);
    r = Rt{};
}

Rt* rt_find_cur(uint32_t fbp) {
    const auto c = g_rt_cur.find(fbp);
    if (c == g_rt_cur.end()) return nullptr;
    const auto it = g_rt.find(RtKey{fbp, c->second.first, c->second.second});
    return it != g_rt.end() ? &it->second : nullptr;
}

// the target of page `fbp` at GS size w x h (scaled by g_rt_s), kept per (fbp, w, h) so a page drawn at two sizes in a frame
// (the pause menu's) is not reallocated; cleared when made or when the page switches from another size; nullptr: no memory
Rt* render_target(uint32_t fbp, int w, int h, bool& gpu_cleared) {
    gpu_cleared = false;
    const auto cur_it = g_rt_cur.find(fbp);
    const bool size_changed = cur_it != g_rt_cur.end() && (cur_it->second.first != w || cur_it->second.second != h);
    const RtKey key{fbp, w, h};
    Rt& r = g_rt[key];
    if (r.cb.surface.image != nullptr && r.w == w && r.h == h && r.s == g_rt_s) {
        g_rt_cur[fbp] = {w, h};
        mrp2d::rt_size_note(fbp, w, h);
        if (size_changed) {
            GX2ClearColor(&r.cb, 0.0f, 0.0f, 0.0f, 0.0f);
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), r.cb.surface.image,
                          r.cb.surface.imageSize);
            gpu_cleared = true;
        }
        return &r;
    }
    int64_t before = 0;
    if (r.cb.surface.image != nullptr) {
        GX2DrawDone();
        before = r.bytes;
        rt_free(r);
    }
    GX2Surface& s = r.cb.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = static_cast<uint32_t>(w * g_rt_s);
    s.height = static_cast<uint32_t>(h * g_rt_s);
    s.depth = 1;
    s.mipLevels = 1;
    s.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    s.aa = GX2_AA_MODE1X;
    s.use = static_cast<GX2SurfaceUse>(GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER);
    s.tileMode = GX2_TILE_MODE_DEFAULT;
    s.swizzle = 0;
    r.cb.viewMip = 0;
    r.cb.viewFirstSlice = 0;
    r.cb.viewNumSlices = 1;
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitColorBufferRegs(&r.cb);
    s.image = MEMAllocFromDefaultHeapEx(s.imageSize, static_cast<int32_t>(s.alignment));
    if (s.image == nullptr) {
        if (before != 0) ts_rt(before, 0);
        g_rt.erase(key);
        return nullptr;
    }
    std::memset(s.image, 0, s.imageSize);  // (transparent black, mrp_2d.cpp's clear: zero bytes are zero in any tiling)
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), s.image, s.imageSize);
    r.tex.surface = s;
    r.tex.viewFirstMip = 0;
    r.tex.viewNumMips = 1;
    r.tex.viewFirstSlice = 0;
    r.tex.viewNumSlices = 1;
    r.tex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2InitTextureRegs(&r.tex);
    r.bytes = s.imageSize;
    r.w = w;
    r.h = h;
    r.s = g_rt_s;
    ts_rt(before, r.bytes);
    g_rt_cur[fbp] = {w, h};
    mrp2d::rt_size_note(fbp, w, h);
    return &r;
}

// ---- the screen's copies (one per colour buffer size: the TV's, the GamePad's) ----

std::map<std::pair<uint32_t, uint32_t>, GX2Texture> g_scr;

// a texture laid out as the colour buffer `cb` (its format and tiling: GX2CopySurfaceEx copies between alike surfaces); nullptr: no memory
GX2Texture* screen_copy_of(const GX2ColorBuffer& cb) {
    const auto key = std::make_pair(cb.surface.width, cb.surface.height);
    auto it = g_scr.find(key);
    if (it != g_scr.end() && it->second.surface.format == cb.surface.format) return &it->second;
    if (it != g_scr.end()) {
        GX2DrawDone();
        tex_free(it->second);
        g_scr.erase(it);
    }
    GX2Texture t{};
    GX2Surface& s = t.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = cb.surface.width;
    s.height = cb.surface.height;
    s.depth = 1;
    s.mipLevels = 1;
    s.format = cb.surface.format;
    s.aa = GX2_AA_MODE1X;
    s.use = GX2_SURFACE_USE_TEXTURE;
    s.tileMode = cb.surface.tileMode;
    s.swizzle = 0;
    t.viewFirstMip = 0;
    t.viewNumMips = 1;
    t.viewFirstSlice = 0;
    t.viewNumSlices = 1;
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&t);
    s.image = MEMAllocFromDefaultHeapEx(s.imageSize, static_cast<int32_t>(s.alignment));
    if (s.image == nullptr) return nullptr;
    std::memset(s.image, 0, s.imageSize);
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_TEXTURE), s.image, s.imageSize);
    return &(g_scr[key] = t);
}

// ---- the log (every 300 calls) ----

enum DrawSkip { kDsRtOff, kDsRtMem, kDsRtZNever, kDsHost3d, kDsPicture, kDsNoTex, kDsScissor, kDsScreenRead, kDsDebug, kDsN };
const char* const kDsName[kDsN] = {"rt (targets off: the GamePad's call)", "rt (no memory)", "rt ZTST never", "host 3d (type 8)", "picture (type 7)",
                                   "no texture", "empty scissor", "screen read (RAC1_WIIU_2D_DEBUG 16)", "debug (one draw a call)"};
const char* const kBuildSkipName[mrp2d::kSkN] = {"fx", "picture", "host 3d", "type", "skip_fbp", "sub", "no sample point", "edge sliver", "crop", "GIF 3d"};
struct Log {
    uint64_t prims = 0, in_runs = 0, runs = 0, draws = 0, copies = 0;
    uint64_t build_skip[mrp2d::kSkN] = {};
    uint64_t draw_skip[kDsN] = {};
    double build_ms = 0.0, submit_ms = 0.0;
} g_log;

Built g_built;
// (mrp_2d_gx2_prebuild) the draw g_built was made for ahead: its screen (null: none), frame and range
const GX2ColorBuffer* g_pre_screen = nullptr;
const MrpChainFrame* g_pre_frame = nullptr;
size_t g_pre_from = 0, g_pre_to = 0;

}  // namespace

bool mrp_2d_gx2_ready() { return g_ready; }

bool mrp_2d_gx2_init() {
    if (g_ready) return true;
    std::vector<uint8_t> gsh;
    if (std::FILE* f = std::fopen("shaders/mrp_2d.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_sh, 0, gsh.data())) {
        std::printf("[wiiu] mrp_2d_gx2: shaders/mrp_2d.gsh not loaded\n");
        return false;
    }
    // (the attributes the compiler kept: one it dropped is not found, and is not needed; 32-bit floats, WHB's default swap)
    WHBGfxInitShaderAttribute(&g_sh, "aPos", 0, offsetof(GV, x), GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aColor", 0, offsetof(GV, r), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aTex", 0, offsetof(GV, s), GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aFog", 0, offsetof(GV, f), GX2_ATTRIB_FORMAT_FLOAT_32);
    WHBGfxInitShaderAttribute(&g_sh, "aClamp", 0, offsetof(GV, c0), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aWin", 0, offsetof(GV, wx), GX2_ATTRIB_FORMAT_FLOAT_32_32);
    if (!WHBGfxInitFetchShader(&g_sh)) {
        std::printf("[wiiu] mrp_2d_gx2: fetch shader not made\n");
        WHBGfxFreeShaderGroup(&g_sh);
        return false;
    }
    const GX2PixelShader* ps = g_sh.pixelShader;
    bool block = false;
    for (uint32_t i = 0; i < ps->uniformBlockCount; ++i)
        if (std::strcmp(ps->uniformBlocks[i].name, "Mrp2dPixel") == 0) {
            g_block_loc = ps->uniformBlocks[i].offset;
            g_block_size = (std::max<uint32_t>(ps->uniformBlocks[i].size, sizeof(Block)) + 15u) & ~15u;
            block = true;
        }
    const char* const smp_names[5] = {"uTexture", "uScreen", "uMip1", "uMip2", "uMip3"};
    int samplers = 0;
    for (int k = 0; k < 5; ++k)
        for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
            if (std::strcmp(ps->samplerVars[i].name, smp_names[k]) == 0) {
                g_smp_loc[k] = ps->samplerVars[i].location;
                ++samplers;
            }
    g_ring = static_cast<uint8_t*>(MEMAllocFromDefaultHeapEx(kRingBytes, GX2_UNIFORM_BLOCK_ALIGNMENT));
    const uint32_t white = 0xffffffffu, black = 0;
    const bool tex = g_ring != nullptr && make_texture(g_white, 1, 1, &white) && make_texture(g_black, 1, 1, &black);
    if (!block || samplers != 5 || !tex) {
        std::printf("[wiiu] mrp_2d_gx2: shader block %d samplers %d memory %d\n", block, samplers, tex);
        tex_free(g_white);
        tex_free(g_black);
        if (g_ring != nullptr) MEMFreeToDefaultHeap(g_ring);
        g_ring = nullptr;
        WHBGfxFreeShaderGroup(&g_sh);
        g_sh = WHBGfxShaderGroup{};
        return false;
    }
    g_ring_off = 0;
    g_ready = true;
    std::printf("[wiiu] mrp_2d_gx2: up (block %u bytes, samplers %u %u %u %u %u)\n", g_block_size, g_smp_loc[0], g_smp_loc[1], g_smp_loc[2], g_smp_loc[3], g_smp_loc[4]);
    return true;
}

void mrp_2d_gx2_shutdown(bool gpu_idle) {
    if (!g_ready) return;
    if (!gpu_idle) GX2DrawDone();
    for (auto& [id, e] : g_tex) tex_free(e.tex);
    g_tex.clear();
    for (auto& [k, l] : g_pool)
        for (FreeTex& f : l) tex_free(f.tex);
    g_pool.clear();
    g_tex_bytes = 0;
    for (auto& [k, r] : g_rt) rt_free(r);
    g_rt.clear();
    g_rt_cur.clear();
    mrp2d::rt_sizes_clear();
    for (auto& [k, t] : g_scr) tex_free(t);
    g_scr.clear();
    tex_free(g_white);
    tex_free(g_black);
    MEMFreeToDefaultHeap(g_ring);
    g_ring = nullptr;
    WHBGfxFreeShaderGroup(&g_sh);
    g_sh = WHBGfxShaderGroup{};
    g_ready = false;
}

// orig: (host) see mrp_2d_backend.inc (mrp_2d_clear_caches on the Wii U)
size_t mrp_2d_gx2_clear_caches() {
    const size_t n = g_tex.size() + pooled();
    if (!g_ready || (n == 0 && g_rt.size() <= g_rt_cur.size())) return n;
    GX2DrawDone();
    for (auto& [id, e] : g_tex) {
        ts_freed(e.w, e.h);
        tex_free(e.tex);
    }
    g_tex.clear();
    for (auto& [k, l] : g_pool)
        for (FreeTex& f : l) {
            ts_freed(k.first, k.second);
            tex_free(f.tex);
        }
    g_pool.clear();
    g_tex_bytes = 0;
    // (the targets' buffers of their other sizes too: the targets themselves stay, as the game's memory does)
    for (auto it = g_rt.begin(); it != g_rt.end();) {
        const auto cur = g_rt_cur.find(it->first.fbp);
        if (cur == g_rt_cur.end() || cur->second.first != it->first.w || cur->second.second != it->first.h) {
            ts_rt(it->second.bytes, 0);
            rt_free(it->second);
            it = g_rt.erase(it);
        } else {
            ++it;
        }
    }
    return n;
}

// orig: (host) see mrp_2d_backend.inc
size_t mrp_2d_gx2_texture_count() { return g_tex.size(); }

void mrp_2d_gx2_prebuild(const MrpChainFrame& f, const GX2ColorBuffer* screen, float FB_W, float FB_H, const gs::Viewport& vp, size_t from, size_t to) {
    g_pre_screen = nullptr;
    if (!g_ready || screen == nullptr) return;
    const int win_w = static_cast<int>(screen->surface.width), win_h = static_cast<int>(screen->surface.height);
    if (f.prims.empty() || from >= std::min(to, f.prims.size()) || win_w <= 0 || win_h <= 0 || FB_W <= 0.0f || FB_H <= 0.0f) return;
    mrp2d::BuildOptions opt;
    opt.drop_gif_3d = true;
    mrp2d::build(f, win_w, win_h, FB_W, FB_H, nullptr, vp, from, to, g_built, opt);
    g_pre_screen = screen;
    g_pre_frame = &f;
    g_pre_from = from;
    g_pre_to = to;
}

// orig: (host) see mrp_2d_gx2.h
void mrp_2d_gx2_prebuild_drop() { g_pre_screen = nullptr; }

// orig: (host) see mrp_2d_gx2.h
void mrp_2d_gx2_draw(const MrpChainFrame& f, const GX2ColorBuffer* screen, float FB_W, float FB_H, const gs::Viewport& vp, bool targets, size_t from, size_t to) {
    if (!g_ready || screen == nullptr) return;
    const int win_w = static_cast<int>(screen->surface.width), win_h = static_cast<int>(screen->surface.height);
    if (f.prims.empty() || from >= std::min(to, f.prims.size()) || win_w <= 0 || win_h <= 0 || FB_W <= 0.0f || FB_H <= 0.0f) return;
    static const int s_debug = std::getenv("RAC1_WIIU_2D_DEBUG") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_2D_DEBUG")) : 0;
    static const bool s_log = std::getenv("RAC1_WIIU_2D_LOG") == nullptr || std::atoi(std::getenv("RAC1_WIIU_2D_LOG")) != 0;
    ++g_call;
    g_ring_use.frame_start();
    const auto t0 = std::chrono::steady_clock::now();
    const gs::Viewport& v = vp;
    if (targets) g_rt_s = rt_scale_for(v, FB_H);
    if (g_ts_every != 0) {
        const size_t pend = std::min(to, f.prims.size());
        for (size_t pi = from; pi < pend; ++pi) {
            const gs::Primitive& p = f.prims[pi];
            if (p.tex != nullptr) ts_tag(p.tex.get(), p.st.tex0);
            for (int m = 0; m < p.mip_n && m < 3; ++m)
                if (p.mip_tex[m] != nullptr) ts_tag(p.mip_tex[m].get(), p.mip_tex0[m]);
        }
    }
    // (the build: the game's projected 3D on the GIF path left out - no host depth here, its effects not drawn yet; or the one
    // mrp_2d_gx2_prebuild made for this call)
    if (!(g_pre_screen == screen && g_pre_frame == &f && g_pre_from == from && g_pre_to == to)) {
        mrp2d::BuildOptions opt;
        opt.drop_gif_3d = true;
        mrp2d::build(f, win_w, win_h, FB_W, FB_H, nullptr, v, from, to, g_built, opt);
    }
    g_pre_screen = nullptr;
    const Built& b = g_built;
    const auto t1 = std::chrono::steady_clock::now();
    g_log.prims += b.prims;
    for (int k = 0; k < mrp2d::kSkN; ++k) g_log.build_skip[k] += b.skipped[k];
    GX2ContextState* ctx = screen == WHBGfxGetTVColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();

    // the vertices to clip space (mrp_2d.cpp's VS), their window positions, in one ring allocation
    GV* gv = nullptr;
    if (!b.verts.empty()) {
        gv = static_cast<GV*>(ring_alloc(static_cast<uint32_t>(b.verts.size() * sizeof(GV)), GX2_VERTEX_BUFFER_ALIGNMENT));
        if (gv == nullptr) return;
        const float vx0 = static_cast<float>(v.x), vy0 = static_cast<float>(win_h - v.y - v.h), vw = static_cast<float>(v.w), vh = static_cast<float>(v.h);
        for (const Item& it : b.items) {
            if (it.kind != Item::kDraw) continue;
            const bool rt = it.key.to_rt;
            const float fw = rt ? static_cast<float>(it.rt_w) : FB_W, fh = rt ? static_cast<float>(it.rt_h) : FB_H;
            const float sx = it.key.sx;
            for (uint32_t i = it.first; i < it.first + it.count; ++i) {
                const mrp2d::V& q = b.verts[i];
                GV& o = gv[i];
                o.x = rt ? q.x / fw * 2.0f - 1.0f : (q.x / fw * 2.0f - 1.0f) * sx;
                o.y = 1.0f - q.y / fh * 2.0f;
                o.r = q.r; o.g = q.g; o.b = q.b; o.a = q.a;
                o.s = q.s; o.t = q.t; o.q = q.q;
                o.f = q.f;
                o.c0 = q.c0; o.c1 = q.c1; o.c2 = q.c2; o.c3 = q.c3;
                o.wx = rt ? q.x : vx0 + (o.x + 1.0f) * 0.5f * vw;
                o.wy = rt ? q.y : vy0 + (1.0f - o.y) * 0.5f * vh;
            }
        }
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, gv, static_cast<uint32_t>(b.verts.size() * sizeof(GV)));
    }

    // the target bound: -1 the screen (bound by the caller), else a render target's page; -2 none known (after a screen copy / clear)
    int bound = -2;
    auto set_screen_viewport = [&] {  // (GX2's y from the top, the Viewport's from the bottom)
        GX2SetViewport(static_cast<float>(v.x), static_cast<float>(win_h - v.y - v.h), static_cast<float>(v.w), static_cast<float>(v.h), 0.0f, 1.0f);
    };
    auto set_call_state = [&] {
        GX2SetShaderMode(g_sh.vertexShader->mode);
        GX2SetFetchShader(&g_sh.fetchShader);
        GX2SetVertexShader(g_sh.vertexShader);
        GX2SetPixelShader(g_sh.pixelShader);
        GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
        GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
        GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
        GX2SetLineWidth(1.0f);
        if (gv != nullptr) GX2SetAttribBuffer(0, static_cast<uint32_t>(b.verts.size() * sizeof(GV)), sizeof(GV), gv);
    };
    set_call_state();
    // leaving a render target: what was drawn into it written out of the colour cache, the texture cache's view of it dropped
    auto leave_target = [&] {
        if (bound < 0) return;
        if (const Rt* r = rt_find_cur(static_cast<uint32_t>(bound)))
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), r->cb.surface.image,
                          r->cb.surface.imageSize);
    };
    auto bind = [&](int want) {
        if (want == bound) return;
        leave_target();
        if (want >= 0) {
            const Rt* r = rt_find_cur(static_cast<uint32_t>(want));
            if (r == nullptr) return;
            GX2SetColorBuffer(&r->cb, GX2_RENDER_TARGET_0);
            GX2SetViewport(0.0f, 0.0f, static_cast<float>(r->w * r->s), static_cast<float>(r->h * r->s), 0.0f, 1.0f);
        } else {
            GX2SetColorBuffer(screen, GX2_RENDER_TARGET_0);
            set_screen_viewport();
        }
        bound = want;
    };
    const void* last_blk = nullptr;  // the last uniform block written (reused while it holds the same, until the ring wraps)
    uint64_t last_blk_wraps = 0;
    Block last_blk_val{};
    auto set_block = [&](const Block& blk) {
        if (last_blk == nullptr || last_blk_wraps != g_ring_wraps || !(last_blk_val == blk)) {
            void* ub = ring_alloc(g_block_size, GX2_UNIFORM_BLOCK_ALIGNMENT);
            if (ub == nullptr) return false;
            std::memset(ub, 0, g_block_size);
            std::memcpy(ub, &blk, sizeof(Block));
            for (uint32_t i = 0; i < sizeof(Block) / 4; ++i) static_cast<uint32_t*>(ub)[i] = __builtin_bswap32(static_cast<uint32_t*>(ub)[i]);
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), ub, g_block_size);
            last_blk = ub;
            last_blk_wraps = g_ring_wraps;
            last_blk_val = blk;
        }
        GX2SetPixelUniformBlock(g_block_loc, g_block_size, last_blk);
        return true;
    };
    auto set_mask = [](uint32_t cm) {
        const auto none = static_cast<GX2ChannelMask>(0);
        GX2SetTargetChannelMasks(static_cast<GX2ChannelMask>(cm), none, none, none, none, none, none, none);
    };
    auto set_texture = [](int unit, const GX2Texture* t, bool lin, bool ws, bool wt) {
        GX2Sampler smp;
        GX2InitSampler(&smp, GX2_TEX_CLAMP_MODE_CLAMP, lin ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT);
        GX2InitSamplerClamping(&smp, ws ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP, wt ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                               GX2_TEX_CLAMP_MODE_CLAMP);
        GX2SetPixelTexture(t, g_smp_loc[unit]);
        GX2SetPixelSampler(&smp, g_smp_loc[unit]);
    };

    uint64_t runs_drawn = 0;
    for (const Item& it : b.items) {
        if (it.kind == Item::kPicture) { ++g_log.draw_skip[kDsPicture]; continue; }  // (not made: no Viewport::screen_picture here)
        if (it.kind == Item::kHost3d) { ++g_log.draw_skip[kDsHost3d]; continue; }    // TODO: the menus' item models into their targets
        const Key& k = it.key;
        if ((s_debug & 1) != 0 && runs_drawn >= 1) { ++g_log.draw_skip[kDsDebug]; continue; }
        if (it.alone && (s_debug & 16) != 0) { ++g_log.draw_skip[kDsScreenRead]; continue; }
        if (k.to_rt) {
            // (drawn by the first call of a frame only: the second, the GamePad's, samples what the first drew at each size in order)
            if (!targets) {
                const uint32_t fbp = static_cast<uint32_t>(k.want);
                if (g_rt.count(RtKey{fbp, it.rt_w, it.rt_h}) != 0) {
                    g_rt_cur[fbp] = {it.rt_w, it.rt_h};
                    mrp2d::rt_size_note(fbp, it.rt_w, it.rt_h);
                }
                ++g_log.draw_skip[kDsRtOff];
                continue;
            }
            // (no depth buffer of a target's own: ZTST NEVER draws nothing, the others untested)
            if (k.ztest && k.ztst == 0) { ++g_log.draw_skip[kDsRtZNever]; continue; }
            const Rt* cur = rt_find_cur(static_cast<uint32_t>(k.want));
            if (cur == nullptr || cur->w != it.rt_w || cur->h != it.rt_h || cur->s != g_rt_s) {
                if (bound == k.want) bind(-1);
                bool gpu_cleared = false;
                if (render_target(static_cast<uint32_t>(k.want), it.rt_w, it.rt_h, gpu_cleared) == nullptr) { ++g_log.draw_skip[kDsRtMem]; continue; }
                if (gpu_cleared) {
                    if (ctx != nullptr) GX2SetContextState(ctx);
                    set_call_state();
                    bound = -2;
                    last_blk = nullptr;
                }
            }
        }
        // the textures it samples (made at their first use)
        const GX2Texture* tex = nullptr;
        const GX2Texture* mips[3] = {nullptr, nullptr, nullptr};
        if (k.tme && (s_debug & 2) == 0) {
            if (k.tex.rt >= 0) {
                if (bound == k.tex.rt) bind(-1);
                if (const Rt* r = rt_find_cur(static_cast<uint32_t>(k.tex.rt))) tex = &r->tex;
            } else {
                tex = texture_of(k.tex);
            }
            if (tex == nullptr) { ++g_log.draw_skip[kDsNoTex]; continue; }
            for (int m = 0; m < k.mip_n && m < 3; ++m) mips[m] = texture_of(k.mip[m]);
        }
        // the scissor: a target's in its GS pixels scaled by r.s (row 0 GX2's top, as the target's row 0 is), the screen's from the
        // bottom left in window pixels to GX2's from the top left inside the buffer
        uint32_t sc[4];
        {
            int gx0, gx1, gy0, gy1;
            if (k.to_rt) {
                const Rt* r = rt_find_cur(static_cast<uint32_t>(k.want));
                const int rs = r != nullptr ? r->s : g_rt_s;
                gx0 = std::clamp(k.sc[0] * rs, 0, it.rt_w * rs);
                gx1 = std::clamp((k.sc[0] + k.sc[2]) * rs, 0, it.rt_w * rs);
                gy0 = std::clamp(k.sc[1] * rs, 0, it.rt_h * rs);
                gy1 = std::clamp((k.sc[1] + k.sc[3]) * rs, 0, it.rt_h * rs);
            } else {
                gx0 = std::clamp(k.sc[0], 0, win_w);
                gx1 = std::clamp(k.sc[0] + k.sc[2], 0, win_w);
                const int top = win_h - (k.sc[1] + k.sc[3]);
                gy0 = std::clamp(top, 0, win_h);
                gy1 = std::clamp(top + k.sc[3], 0, win_h);
            }
            if (gx1 <= gx0 || gy1 <= gy0) { ++g_log.draw_skip[kDsScissor]; continue; }
            sc[0] = static_cast<uint32_t>(gx0);
            sc[1] = static_cast<uint32_t>(gy0);
            sc[2] = static_cast<uint32_t>(gx1 - gx0);
            sc[3] = static_cast<uint32_t>(gy1 - gy0);
        }
        // the screen under the primitive copied (its window box; the build's rows from the bottom, the copy's from the top)
        const GX2Texture* scr = &g_black;
        if (it.alone && (s_debug & 8) == 0) {
            bind(-1);
            GX2Texture* cp = screen_copy_of(*screen);
            if (cp != nullptr) {
                if (it.copy[2] > it.copy[0] && it.copy[3] > it.copy[1]) {
                    GX2Rect rect{it.copy[0], win_h - it.copy[3], it.copy[2], win_h - it.copy[1]};
                    GX2Point at{it.copy[0], win_h - it.copy[3]};
                    GX2Invalidate(GX2_INVALIDATE_MODE_COLOR_BUFFER, screen->surface.image, screen->surface.imageSize);
                    GX2CopySurfaceEx(&screen->surface, 0, 0, &cp->surface, 0, 0, 1, &rect, &at);
                    GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, cp->surface.image, cp->surface.imageSize);
                    ++g_log.copies;
                    // (the copy changes the GPU's state: the context, then this call's state, set again)
                    if (ctx != nullptr) GX2SetContextState(ctx);
                    set_call_state();
                    bound = -2;
                    last_blk = nullptr;
                }
                scr = cp;
            }
        }
        bind(k.want);
        const bool lin = k.lin != 0, ws = k.wrap_s != 0, wt = k.wrap_t != 0;
        set_texture(0, tex != nullptr ? tex : &g_white, lin, ws, wt);
        set_texture(1, scr, false, false, false);
        for (int m = 0; m < 3; ++m) set_texture(2 + m, mips[m] != nullptr ? mips[m] : &g_white, k.lin_min != 0, ws, wt);
        // (testing, Gx2Perf::pixels_off: 8 x 8 pixels at the box's corner - the draws' GPU time without their pixels')
        GX2SetScissor(sc[0], sc[1], g_gx2_perf.pixels_off ? std::min<uint32_t>(sc[2], 8) : sc[2], g_gx2_perf.pixels_off ? std::min<uint32_t>(sc[3], 8) : sc[3]);
        Block blk{};
        blk.fog_col[0] = static_cast<float>(k.fogcol & 0xff) / 255.0f;
        blk.fog_col[1] = static_cast<float>((k.fogcol >> 8) & 0xff) / 255.0f;
        blk.fog_col[2] = static_cast<float>((k.fogcol >> 16) & 0xff) / 255.0f;
        blk.a[0] = tex != nullptr ? k.tme : 0;
        blk.a[1] = k.tfx;
        blk.a[2] = k.tcc;
        blk.a[3] = k.ate;
        blk.b[0] = k.atst;
        blk.b[1] = k.aref;
        blk.b[2] = k.afail;
        blk.b[3] = k.fge;
        blk.c[1] = 0;  // (the AFAIL split's Z pass: no Z writes here)
        blk.c[2] = k.scr;
        blk.c[3] = k.dt;
        blk.d[0] = k.datm;
        blk.d[1] = k.mip_n;
        blk.d[2] = k.mip_lo;
        blk.d[3] = k.lcm;
        blk.e[0] = k.mxl;
        blk.e[1] = win_h;
        blk.mip[0] = k.mip_k;
        blk.mip[1] = k.mip_l;
        std::memcpy(blk.scr_map, it.scr_map, sizeof blk.scr_map);
        const uint32_t m = k.fbmsk;
        const uint32_t cm = ((m & 0xffu) != 0xffu ? GX2_CHANNEL_MASK_R : 0) | ((m & 0xff00u) != 0xff00u ? GX2_CHANNEL_MASK_G : 0) |
                            ((m & 0xff0000u) != 0xff0000u ? GX2_CHANNEL_MASK_B : 0) | ((m & 0xff000000u) != 0xff000000u ? GX2_CHANNEL_MASK_A : 0);
        const uint64_t prim = k.blend && (s_debug & 4) == 0 ? (1ull << 6) : 0;
        const GX2PrimitiveMode mode = k.lines ? GX2_PRIMITIVE_MODE_LINES : GX2_PRIMITIVE_MODE_TRIANGLES;
        const bool reads_as = gs::blend_reads_src_alpha_gx2(prim, k.alpha);
        if (k.to_rt && reads_as && (cm & GX2_CHANNEL_MASK_A) != 0) {
            // a render target keeps the GS's raw alpha while its blend reads the source alpha as the GS's (0x80 = 1.0): mrp_2d.cpp
            // blends with the second output (dual-source); here two passes of the same pixels - the colour blended (the alpha
            // channel masked), then the alpha alone, unblended, raw (mrp_2d.cpp's alpha blend is ONE / ZERO: the same result)
            blk.c[0] = 0;
            if (set_block(blk)) {
                set_mask(cm & ~static_cast<uint32_t>(GX2_CHANNEL_MASK_A));
                gs::set_blend_gx2(prim, k.alpha);
                GX2DrawEx(mode, it.count, it.first, 1);
                ++g_log.draws;
            }
            blk.c[0] = 1;
            if (set_block(blk)) {
                set_mask(GX2_CHANNEL_MASK_A);
                gs::set_blend_gx2(0, 0);
                GX2DrawEx(mode, it.count, it.first, 1);
                ++g_log.draws;
            }
        } else {
            // the screen: the alpha as 0x80 = 1.0; a render target: the raw alpha (no factor reads the source alpha, or the alpha
            // channel is not written: the source alpha as 0x80 = 1.0 then)
            blk.c[0] = k.to_rt && !reads_as ? 1 : 0;
            if (set_block(blk)) {
                set_mask(cm);
                gs::set_blend_gx2(prim, k.alpha);
                GX2DrawEx(mode, it.count, it.first, 1);
                ++g_log.draws;
            }
        }
        ++runs_drawn;
        ++g_log.runs;
    }
    if (bound != -1) bind(-1);  // (the target left: its drawing written out for the next call's sampling)
    g_log.in_runs += b.drawn;  // (the build's primitives in runs; the runs this call did not draw are counted apart)

    // the state left as the header says
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
                             GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, 1.0f);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
    tex_evict();
    ts_print(g_tex.size(), pooled());

    const auto t2 = std::chrono::steady_clock::now();
    g_log.build_ms += std::chrono::duration<double, std::milli>(t1 - t0).count();
    g_log.submit_ms += std::chrono::duration<double, std::milli>(t2 - t1).count();
    if (s_log && g_call % 300 == 0) {
        std::printf("[wiiu] mrp_2d_gx2: last 300 calls: %llu primitives, %llu in runs, %llu runs drawn (%llu draws, %llu screen copies); left out by the build:",
                    static_cast<unsigned long long>(g_log.prims), static_cast<unsigned long long>(g_log.in_runs), static_cast<unsigned long long>(g_log.runs),
                    static_cast<unsigned long long>(g_log.draws), static_cast<unsigned long long>(g_log.copies));
        for (int k = 0; k < mrp2d::kSkN; ++k)
            if (g_log.build_skip[k] != 0) std::printf(" %s %llu,", kBuildSkipName[k], static_cast<unsigned long long>(g_log.build_skip[k]));
        std::printf(" runs not drawn:");
        for (int k = 0; k < kDsN; ++k)
            if (g_log.draw_skip[k] != 0) std::printf(" %s %llu,", kDsName[k], static_cast<unsigned long long>(g_log.draw_skip[k]));
        std::printf(" %zu targets, %zu+%zu textures (%zu KB); CPU %.2f ms a call (build %.2f, submit %.2f)\n", g_rt.size(), g_tex.size(), pooled(), g_tex_bytes / 1024,
                    (g_log.build_ms + g_log.submit_ms) / 300.0, g_log.build_ms / 300.0, g_log.submit_ms / 300.0);
        std::fflush(stdout);
        g_log = Log{};
    }
}

}  // namespace rac1
