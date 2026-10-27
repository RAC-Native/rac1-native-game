// The MRP world's ties and shrubs drawn with GX2 on the Wii U (MRP_LITE.md phase 3; how a caller uses it: mrp_world_gx2.h). A port
// of mrp_world_renderer.cpp's GL draw (draw_frame, its GPU path batches) over the same CPU preparation, read through
// mrp_world_backend.inc:
//
// - the level: its packet templates (TVert, 28 bytes, built by the PPC: big-endian) and their u32 indices (relative to the template's
//   first vertex, big-endian: GX2_INDEX_TYPE_U32) copied once into MEM2 buffers; its texture buckets as GX2 2D-array textures with
//   their mips (as mrp_moby_gx2.cpp's: each layer's level written into a linear surface and copied into the tiled array by
//   GX2CopySurface; RGBA bytes in memory order);
// - the resident lit colours (one u32 per colour, R in the low byte): an R32UI 2D texture, 1024 texels a row, linear, the vertex
//   shader's texelFetch; the whole table written when the level's is new, then the ranges that changed (none while the relighting is
//   off: RAC1_WORLD_PASS_SKIP=6). Its words are written byte-swapped: GX2 reads the R32UI texels as little-endian words, as the
//   uniform blocks (seen on the console: the PPC's order gave the colours' bytes reversed); RAC1_WIIU_WORLD_RCSWAP=0 writes them as
//   the PPC holds them, and the debug bit 1024 flips the order the run has;
// - per draw command (no storage buffers, no indirect draws, no gl_BaseInstance): a uniform block with the draw's instance (InstGpu,
//   10 vec4) and its record (8 words), one GX2DrawIndexedEx with the template's first vertex as the draw's offset (base vertex); the
//   draw's texture (its unit in the frame's epoch: one bucket and sampler) bound before it when it changes; per call the frame's plain
//   uniforms; per batch and pass the pixel shader's block (the alpha test's pass, FGE, FOGCOL, the shade debug). All written into a
//   MEM2 ring with each 32-bit word byte-swapped (GX2 reads the uniform blocks as little-endian words: mrp_moby_gx2.cpp's block_of);
// - the GPU state per batch as the GL backend sets it: the GS's blending (gs::set_blend_gx2), the depth test (GEQUAL / GREATER as
//   LEQUAL / LESS on the host's depth, written when ZBUF's ZMSK is off and the test on), the alpha test's two passes (pass 1 the passing
//   pixels of the draws whose test is on, every pixel of the others; pass 2, RKey::pass2, the failing ones as AFAIL says: FB_ONLY the
//   colour, ZB_ONLY the depth alone, RGB_ONLY the colour without alpha);
// - the world's projection (g_mrp_world_projection 0, the PC's default: the instances' clip matrices), as the mobys' (mrp_moby_gx2_init),
//   whose depth the world shares. The game's own VU1 projection (mode 1, RAC1_WIIU_WORLD_PROJ=vu) repeats the GS's 16-bit screen
//   coordinates: an instance crossing the camera's plane wraps into planes over the screen (the VU's clipping programs keep those
//   from the PS2's GS; the static path draws its instances whole) - seen on the console.
//
// Not drawn (phase 3): the CPU path's batches (the static path makes none; the tie A state packet has no triangles), the tfrags, the
// sky, the shrub far variants, the casters, the modern lighting. Host code.
#include "mrp_world_gx2.h"

#include "gs_draw_gx2.h"

#include <game/ovl/ovl_common.h>
#include <game/render/mrp_world_backend.inc>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_emu.h>

#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/context.h>
#include <gx2/surface.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <whb/gfx.h>

#include <algorithm>
#include <chrono>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {

namespace {

constexpr uint32_t kColsLog2 = 10;  // (the colours' texture: 1 << kColsLog2 texels a row)

// The vertex shader's blocks (mrp_world.vert, std140)
struct alignas(16) FrameBlock {
    float frame[4];  // fb width, fb height, x scale, 0
    float xyoff[4];  // XYOFFSET x, y (1/16 px), depth a, depth b
    int32_t mode[4];  // the projection, the position's debug, the colours' row's log2, 0
};
static_assert(sizeof(FrameBlock) == 48);
struct alignas(16) DrawBlock {
    mrpwb::InstGpu inst;
    uint32_t cmd[8];
};
static_assert(sizeof(DrawBlock) == 192);
// The pixel shader's (mrp_world.frag's WorldPixel)
struct alignas(16) PixelBlock {
    float fog_col[4];
    int32_t a[4];  // the alpha test's pass, the shade debug, FGE, 1 the alpha test off (testing)
    bool operator==(const PixelBlock& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};
static_assert(sizeof(PixelBlock) == 32);

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

bool g_ready = false;
WHBGfxShaderGroup g_sh{};
struct BlockLoc {
    uint32_t loc = 0, size = 0;
    bool found = false;
};
BlockLoc g_frame_loc, g_draw_loc, g_pixel_loc;
uint32_t g_tex_loc = 0, g_cols_loc = 0;
bool g_tex_ok = false, g_cols_ok = false;
GX2Sampler g_samplers[16];  // [TEX1 / CLAMP bits: 0 MMAG linear, 1 MMIN linear, 2 wrap s, 3 wrap t] (mip mode 0: the GS's level, textureLod)
GX2Sampler g_cols_sampler;  // (the colours' texture: point, clamped)

// The uniform blocks, appended to a ring in MEM2 (mrp_moby_gx2.cpp's): before the ring wraps over what the GPU may still read the
// CPU waits for it (GX2DrawDone)
constexpr uint32_t kRingBytes = 16u << 20;
uint8_t* g_ring = nullptr;
uint32_t g_ring_off = 0;
uint64_t g_ring_wraps = 0;
void* ring_alloc(uint32_t bytes) {
    uint32_t off = (g_ring_off + GX2_UNIFORM_BLOCK_ALIGNMENT - 1) & ~(GX2_UNIFORM_BLOCK_ALIGNMENT - 1);
    if (bytes > kRingBytes) return nullptr;
    if (off + bytes > kRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_ring_wraps;
    }
    g_ring_off = off + bytes;
    return g_ring + off;
}
// a block of `size` bytes holding `data` (`bytes` of it; the rest left as it is: never read), each 32-bit word swapped (GX2 reads the
// shaders' blocks as little-endian words), written out of the CPU's cache
void* block_of(const void* data, uint32_t bytes, uint32_t size) {
    void* p = ring_alloc(size);
    if (p == nullptr) return nullptr;
    const uint32_t n = std::min(bytes, size);
    const auto* src = static_cast<const uint8_t*>(data);
    auto* dst = static_cast<uint32_t*>(p);
    for (uint32_t i = 0; i + 4 <= n; i += 4) {
        uint32_t w;
        std::memcpy(&w, src + i, 4);
        dst[i / 4] = __builtin_bswap32(w);
    }
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), p, size);
    return p;
}

void* mem_alloc(uint32_t bytes, uint32_t align) { return MEMAllocFromDefaultHeapEx(std::max<uint32_t>(bytes, 4), static_cast<int32_t>(align)); }
void mem_free(void*& p) {
    if (p != nullptr) MEMFreeToDefaultHeap(p);
    p = nullptr;
}

// ---- the level's data on the GPU ----------------------------------------------------------------------------------------

// its templates' vertices and indices (static for the level: copied once per level loaded)
struct Geometry {
    uint64_t loads = ~0ull;
    void* vb = nullptr;
    void* ib = nullptr;
    size_t v_count = 0, i_count = 0;
} g_geo;
void geo_free() {
    mem_free(g_geo.vb);
    mem_free(g_geo.ib);
    g_geo = Geometry{};
}
void geo_sync(const mrpwb::LevelView& L) {
    if (L.loads == g_geo.loads) return;
    GX2DrawDone();  // (the old buffers may still be read)
    geo_free();
    g_geo.loads = L.loads;
    const std::vector<mrpwb::TVert>& v = *L.tverts;
    const std::vector<uint32_t>& ix = *L.tidx;
    const uint32_t vbytes = static_cast<uint32_t>(v.size() * sizeof(mrpwb::TVert)), ibytes = static_cast<uint32_t>(ix.size() * sizeof(uint32_t));
    g_geo.vb = mem_alloc(vbytes, GX2_VERTEX_BUFFER_ALIGNMENT);
    g_geo.ib = mem_alloc(ibytes, GX2_INDEX_BUFFER_ALIGNMENT);
    if (g_geo.vb == nullptr || g_geo.ib == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: no memory for the level's %zu template vertices / %zu indices\n", v.size(), ix.size());
        geo_free();
        g_geo.loads = L.loads;  // (not tried again every frame)
        return;
    }
    if (vbytes > 0) std::memcpy(g_geo.vb, v.data(), vbytes);
    if (ibytes > 0) std::memcpy(g_geo.ib, ix.data(), ibytes);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g_geo.vb, std::max<uint32_t>(vbytes, 4));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g_geo.ib, std::max<uint32_t>(ibytes, 4));
    g_geo.v_count = v.size();
    g_geo.i_count = ix.size();
    std::printf("[wiiu] mrp_world_gx2: level %d: %zu template vertices (%u KB), %zu indices (%u KB)\n", L.id, v.size(), vbytes / 1024, ix.size(), ibytes / 1024);
    std::fflush(stdout);
}

// its texture arrays (one per bucket), made at the level's first draw
struct Textures {
    uint64_t loads = ~0ull;
    std::vector<GX2Texture> arrays;
} g_tex;
GX2Texture g_white{};  // a 1 x 1 array of one white layer (a draw without a texture: the shader's white texture or none)

void tex_free_one(GX2Texture& t) {
    mem_free(t.surface.image);
    mem_free(t.surface.mipmaps);
    t = GX2Texture{};
}
void tex_free() {
    for (GX2Texture& t : g_tex.arrays) tex_free_one(t);
    g_tex.arrays.clear();
    g_tex.loads = ~0ull;
}
// a w x h x layers RGBA8 array texture of `levels` mips (tiled as GX2 chooses), its memory allocated (nothing written)
bool tex_make(GX2Texture& t, uint32_t w, uint32_t h, uint32_t layers, uint32_t levels, GX2TileMode tile) {
    t = GX2Texture{};
    GX2Surface& s = t.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D_ARRAY;
    s.width = w;
    s.height = h;
    s.depth = layers;
    s.mipLevels = levels;
    s.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
    s.aa = GX2_AA_MODE1X;
    s.use = GX2_SURFACE_USE_TEXTURE;
    s.tileMode = tile;
    s.swizzle = 0;
    t.viewFirstMip = 0;
    t.viewNumMips = levels;
    t.viewFirstSlice = 0;
    t.viewNumSlices = layers;
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);  // (the texels' bytes R, G, B, A in memory: the mobys')
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&t);
    s.image = mem_alloc(s.imageSize, s.alignment);
    if (s.image == nullptr) return false;
    if (s.mipmapSize > 0) {
        s.mipmaps = mem_alloc(s.mipmapSize, s.alignment);
        if (s.mipmaps == nullptr) {
            mem_free(s.image);
            return false;
        }
    }
    return true;
}
// the level's buckets made (mrp_moby_gx2.cpp's tex_sync: each layer's level decoded by mrpwb::bucket_texels into a linear surface,
// copied by the GPU into the array's level and slice, the GPU waited for after each array). Changes the GPU's state (the caller sets
// its context again).
void tex_sync(const mrpwb::LevelView& L) {
    if (L.loads == g_tex.loads) return;
    GX2DrawDone();  // (the old arrays may still be sampled)
    tex_free();
    g_tex.loads = L.loads;
    const auto t0 = std::chrono::steady_clock::now();
    size_t bytes = 0, failed = 0, empty = 0;
    std::vector<uint32_t> px;
    std::vector<void*> temps;
    g_tex.arrays.resize(L.buckets);
    for (size_t bi = 0; bi < L.buckets; ++bi) {
        const mrpwb::BucketInfo bk = mrpwb::bucket_info(bi);
        GX2Texture& t = g_tex.arrays[bi];
        if (bk.layers == 0 || bk.levels == 0) {  // (none of the level's tables: a frame's bucket - the sky's -, not drawn here)
            ++empty;
            continue;
        }
        if (!tex_make(t, bk.w, bk.h, bk.layers, bk.levels, GX2_TILE_MODE_DEFAULT)) {
            tex_free_one(t);
            ++failed;
            continue;
        }
        bytes += t.surface.imageSize + t.surface.mipmapSize;
        for (uint32_t layer = 0; layer < bk.layers; ++layer)
            for (uint32_t l = 0; l < bk.levels; ++l) {
                GX2Surface lin{};
                lin.dim = GX2_SURFACE_DIM_TEXTURE_2D;
                lin.width = std::max(1u, bk.w >> l);
                lin.height = std::max(1u, bk.h >> l);
                lin.depth = 1;
                lin.mipLevels = 1;
                lin.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
                lin.aa = GX2_AA_MODE1X;
                lin.use = GX2_SURFACE_USE_TEXTURE;
                lin.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
                GX2CalcSurfaceSizeAndAlignment(&lin);
                lin.image = mem_alloc(lin.imageSize, lin.alignment);
                if (lin.image == nullptr) {
                    ++failed;
                    continue;
                }
                temps.push_back(lin.image);
                mrpwb::bucket_texels(bi, layer, l, px);
                auto* dst = static_cast<uint32_t*>(lin.image);
                for (uint32_t y = 0; y < lin.height && static_cast<size_t>(y + 1) * lin.width <= px.size(); ++y)
                    std::memcpy(dst + static_cast<size_t>(y) * lin.pitch, px.data() + static_cast<size_t>(y) * lin.width, lin.width * 4u);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, lin.image, lin.imageSize);
                GX2CopySurface(&lin, 0, 0, &t.surface, l, layer);
            }
        GX2DrawDone();
        for (void*& p : temps) mem_free(p);
        temps.clear();
        GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, t.surface.image, t.surface.imageSize);
        if (t.surface.mipmaps != nullptr) GX2Invalidate(GX2_INVALIDATE_MODE_TEXTURE, t.surface.mipmaps, t.surface.mipmapSize);
    }
    std::printf("[wiiu] mrp_world_gx2: level %d: %zu texture arrays, %zu KB (%zu not made, %zu without layers) in %.0f ms\n", L.id, g_tex.arrays.size(), bytes / 1024,
                failed, empty, std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    std::fflush(stdout);
}

int g_debug = 0;  // (RAC1_WIIU_WORLD_DEBUG's mode for this frame: mrp_world_gx2.h)

// the resident colours' texture: R32UI, (1 << kColsLog2) x rows, linear (the CPU writes its texels in place)
struct Colours {
    GX2Texture tex{};
    size_t words = 0;
    bool swapped = false;
} g_cols;
void cols_free() {
    mem_free(g_cols.tex.surface.image);
    g_cols = Colours{};
}
bool cols_make(uint32_t rows) {
    GX2Texture& t = g_cols.tex;
    t = GX2Texture{};
    GX2Surface& s = t.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = 1u << kColsLog2;
    s.height = rows;
    s.depth = 1;
    s.mipLevels = 1;
    s.format = GX2_SURFACE_FORMAT_UINT_R32;
    s.aa = GX2_AA_MODE1X;
    s.use = GX2_SURFACE_USE_TEXTURE;
    s.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    t.viewNumMips = 1;
    t.viewNumSlices = 1;
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&t);
    s.image = mem_alloc(s.imageSize, s.alignment);
    if (s.image == nullptr) return false;
    std::memset(s.image, 0, s.imageSize);
    return true;
}
// the colours' changes since the last call written (the whole table: a new level, its size changed, the byte order changed)
void cols_sync() {
    mrpwb::ColoursView cv;
    mrpwb::colours_view(cv);
    const std::vector<uint32_t>& m = *cv.words;
    static const bool rcswap = std::getenv("RAC1_WIIU_WORLD_RCSWAP") == nullptr || std::atoi(std::getenv("RAC1_WIIU_WORLD_RCSWAP")) != 0;
    const bool swap = rcswap != ((g_debug & 1024) != 0);
    const uint32_t rows = std::max<uint32_t>(1, static_cast<uint32_t>((m.size() + (1u << kColsLog2) - 1) >> kColsLog2));
    const bool all = cv.all || m.size() != g_cols.words || swap != g_cols.swapped || g_cols.tex.surface.image == nullptr;
    if (!all && cv.dirty->empty()) return;
    GX2DrawDone();  // (the frames drawn before may still read it)
    if (g_cols.tex.surface.image == nullptr || g_cols.tex.surface.height != rows) {
        cols_free();
        if (!cols_make(rows)) {
            std::printf("[wiiu] mrp_world_gx2: no memory for the resident colours' texture (%u rows)\n", rows);
            cols_free();
            return;
        }
    }
    GX2Surface& s = g_cols.tex.surface;
    auto* dst = static_cast<uint32_t*>(s.image);
    auto put = [&](uint32_t first, uint32_t n) {
        for (uint32_t i = first; i < first + n && i < m.size(); ++i) dst[static_cast<size_t>(i >> kColsLog2) * s.pitch + (i & ((1u << kColsLog2) - 1))] = swap ? __builtin_bswap32(m[i]) : m[i];
    };
    if (all) {
        put(0, static_cast<uint32_t>(m.size()));
        std::printf("[wiiu] mrp_world_gx2: resident colours: %zu words sent (%u rows, pitch %u, %s)\n", m.size(), rows, s.pitch, swap ? "byte-swapped" : "as the PPC holds them");
        std::fflush(stdout);
    } else {
        for (const auto& [first, n] : *cv.dirty) put(first, n);
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, s.image, s.imageSize);
    g_cols.words = m.size();
    g_cols.swapped = swap;
    mrpwb::colours_taken();
}

// ---- the per-call caches ----------------------------------------------------------------------------------------------
// The blocks written for a frame (the TV's call) reused by its next call (the GamePad's): valid while the frame and the ring's
// wraps are the same
struct Cache {
    uint64_t serial = ~0ull, wraps = ~0ull;
    std::vector<void*> draws;  // per draw command: its DrawBlock
    void* frame = nullptr;
} g_cache;
void cache_reset(uint64_t serial, size_t draws) {
    g_cache.serial = serial;
    g_cache.wraps = g_ring_wraps;
    g_cache.draws.assign(draws, nullptr);
    g_cache.frame = nullptr;
}

uint64_t g_tris = 0;
uint32_t g_draws_this_call = 0;
uint64_t g_calls = 0, g_draws = 0, g_batches = 0, g_refused = 0, g_skipped = 0;
double g_cpu_ms = 0.0;

}  // namespace

// orig: (host) see mrp_world_gx2.h
bool mrp_world_gx2_ready() { return g_ready; }

// orig: (host) see mrp_world_gx2.h
void mrp_world_gx2_debug_frame(uint64_t game_frame) {
    static const char* list = std::getenv("RAC1_WIIU_WORLD_DEBUG");
    static const uint64_t at = std::getenv("RAC1_WIIU_WORLD_DEBUG_AT") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_WORLD_DEBUG_AT"), nullptr, 10) : 0;
    int mode = 0;
    if (list != nullptr && game_frame >= at) {
        uint64_t band = (game_frame - at) / 50;
        const char* q = list;  // (past the list's end its last mode stays)
        for (const char* n = std::strchr(q, ','); band > 0 && n != nullptr; --band, n = std::strchr(q, ',')) q = n + 1;
        mode = std::atoi(q);
    }
    if (mode != g_debug) {
        std::printf("[wiiu] mrp_world_gx2: debug mode %d from game frame %llu\n", mode, static_cast<unsigned long long>(game_frame));
        std::fflush(stdout);
        g_cache.serial = ~0ull;  // (the frame's blocks written again: the mode is in them)
    }
    g_debug = mode;
}

// orig: (host) see mrp_world_gx2.h
bool mrp_world_gx2_init() {
    if (g_ready) return true;
    std::vector<uint8_t> gsh;
    // (testing, RAC1_WIIU_WORLD_GSH=<file>: another build of the shaders - a reduced one: the blocks and the samplers it lacks are not bound)
    const char* gsh_env = std::getenv("RAC1_WIIU_WORLD_GSH");
    const bool variant = gsh_env != nullptr && gsh_env[0] != 0;
    if (std::FILE* f = std::fopen(variant ? gsh_env : "shaders/mrp_world.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_sh, 0, gsh.data())) {
        std::printf("[wiiu] mrp_world_gx2: shaders/mrp_world.gsh not loaded\n");
        return false;
    }
    // The template vertex's attributes (mrpwb::TVert, 28 bytes, written by the PPC: big-endian), read as integers (ivec4 / uvec4):
    // the 16-bit ones swapped within each 16 bits (8_IN_16), the 8-bit ones not swapped (byte 0 is X), as mrp_moby_gx2.cpp's
    const auto s16x4 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_SIGNED | GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_16_16_16_16);
    const auto u8x4 = GX2_ATTRIB_FORMAT_UINT_8_8_8_8;
    struct A {
        const char* name;
        uint32_t offset;
        GX2AttribFormat format;
    };
    const A attrs[] = {
        {"aPos", offsetof(mrpwb::TVert, pos), s16x4},
        {"aDelta", offsetof(mrpwb::TVert, delta), s16x4},
        {"aUv", offsetof(mrpwb::TVert, uv), s16x4},
        {"aCol", offsetof(mrpwb::TVert, col), u8x4},
    };
    g_sh.numAttributes = 0;
    const GX2VertexShader* vs = g_sh.vertexShader;
    for (const A& a : attrs) {
        const GX2AttribVar* var = nullptr;
        for (uint32_t i = 0; i < vs->attribVarCount; ++i)
            if (std::strcmp(vs->attribVars[i].name, a.name) == 0) var = &vs->attribVars[i];
        if (var == nullptr) continue;  // (one the compiler dropped: not read)
        GX2AttribStream& s = g_sh.attributes[g_sh.numAttributes++];
        s = GX2AttribStream{};
        s.location = var->location;
        s.buffer = 0;
        s.offset = a.offset;
        s.format = a.format;
        s.type = GX2_ATTRIB_INDEX_PER_VERTEX;
        s.aluDivisor = 0;
        s.mask = GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
        s.endianSwap = a.format == u8x4 ? GX2_ENDIAN_SWAP_NONE : GX2_ENDIAN_SWAP_8_IN_16;
    }
    std::printf("[wiiu] mrp_world_gx2: %u attributes:", g_sh.numAttributes);
    for (uint32_t i = 0; i < g_sh.numAttributes; ++i) std::printf(" loc %u off %u fmt %#x", g_sh.attributes[i].location, g_sh.attributes[i].offset, g_sh.attributes[i].format);
    std::printf("\n");
    if (!WHBGfxInitFetchShader(&g_sh)) {
        std::printf("[wiiu] mrp_world_gx2: fetch shader not made\n");
        WHBGfxFreeShaderGroup(&g_sh);
        g_sh = WHBGfxShaderGroup{};
        return false;
    }
    auto find = [](const GX2UniformBlock* blocks, uint32_t n, const char* name, uint32_t min_size, BlockLoc& out) {
        for (uint32_t i = 0; i < n; ++i)
            if (std::strcmp(blocks[i].name, name) == 0) {
                out.loc = blocks[i].offset;
                out.size = (std::max(blocks[i].size, min_size) + 15u) & ~15u;
                out.found = true;
            }
    };
    find(vs->uniformBlocks, vs->uniformBlockCount, "WorldFrame", sizeof(FrameBlock), g_frame_loc);
    find(vs->uniformBlocks, vs->uniformBlockCount, "WorldDraw", sizeof(DrawBlock), g_draw_loc);
    const GX2PixelShader* ps = g_sh.pixelShader;
    find(ps->uniformBlocks, ps->uniformBlockCount, "WorldPixel", sizeof(PixelBlock), g_pixel_loc);
    for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
        if (std::strcmp(ps->samplerVars[i].name, "uTex") == 0) {
            g_tex_loc = ps->samplerVars[i].location;
            g_tex_ok = true;
        }
    for (uint32_t i = 0; i < vs->samplerVarCount; ++i)
        if (std::strcmp(vs->samplerVars[i].name, "uCols") == 0) {
            g_cols_loc = vs->samplerVars[i].location;
            g_cols_ok = true;
        }
    g_ring = static_cast<uint8_t*>(mem_alloc(kRingBytes, GX2_UNIFORM_BLOCK_ALIGNMENT));
    const bool white = g_ring != nullptr && tex_make(g_white, 1, 1, 1, 1, GX2_TILE_MODE_LINEAR_ALIGNED);
    if (variant)
        std::printf("[wiiu] mrp_world_gx2: %s: blocks frame %d draw %d pixel %d, samplers texture %d colours %d\n", gsh_env, g_frame_loc.found, g_draw_loc.found,
                    g_pixel_loc.found, g_tex_ok, g_cols_ok);
    if ((!variant && (!g_frame_loc.found || !g_draw_loc.found || !g_pixel_loc.found || !g_tex_ok || !g_cols_ok)) || !white) {
        std::printf("[wiiu] mrp_world_gx2: blocks frame %d draw %d pixel %d, samplers texture %d colours %d, memory %d\n", g_frame_loc.found, g_draw_loc.found,
                    g_pixel_loc.found, g_tex_ok, g_cols_ok, white);
        tex_free_one(g_white);
        if (g_ring != nullptr) MEMFreeToDefaultHeap(g_ring);
        g_ring = nullptr;
        WHBGfxFreeShaderGroup(&g_sh);
        g_sh = WHBGfxShaderGroup{};
        return false;
    }
    std::memset(g_white.surface.image, 0xff, g_white.surface.imageSize);  // (white in any tiling)
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, g_white.surface.image, g_white.surface.imageSize);
    // the samplers (mrp_world_renderer.cpp's init_gl, mip mode 0: the GS's level chosen in the shader, nearest between levels): the
    // filters from TEX1's MMAG / MMIN, the wraps from CLAMP's WMS / WMT
    for (uint32_t s = 0; s < 16; ++s) {
        GX2Sampler& smp = g_samplers[s];
        GX2InitSampler(&smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
        const GX2TexXYFilterMode mag = (s & 1) != 0 ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT;
        const GX2TexXYFilterMode mn = (s & 2) != 0 ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT;
        GX2InitSamplerXYFilter(&smp, mag, mn, GX2_TEX_ANISO_RATIO_NONE);
        GX2InitSamplerZMFilter(&smp, GX2_TEX_Z_FILTER_MODE_POINT, GX2_TEX_MIP_FILTER_MODE_POINT);
        GX2InitSamplerClamping(&smp, (s & 4) != 0 ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP, (s & 8) != 0 ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                               GX2_TEX_CLAMP_MODE_CLAMP);
        GX2InitSamplerLOD(&smp, 0.0f, 13.0f, 0.0f);
    }
    GX2InitSampler(&g_cols_sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
    g_ring_off = 0;
    // the world's projection, as the mobys' (mrp_moby_gx2_init), whose depth the world shares (testing: RAC1_WIIU_WORLD_PROJ=vu the
    // game's own VU1 projection onto its 4:3 frame)
    const char* pj = std::getenv("RAC1_WIIU_WORLD_PROJ");
    g_mrp_world_projection = pj != nullptr && std::strcmp(pj, "vu") == 0 ? 1 : 0;
    g_ready = true;
    std::printf("[wiiu] mrp_world_gx2: up (blocks: frame %u draw %u pixel %u bytes)\n", g_frame_loc.size, g_draw_loc.size, g_pixel_loc.size);
    return true;
}

// orig: (host) see mrp_world_gx2.h
void mrp_world_gx2_shutdown() {
    if (!g_ready) return;
    GX2DrawDone();
    tex_free();
    geo_free();
    cols_free();
    tex_free_one(g_white);
    MEMFreeToDefaultHeap(g_ring);
    g_ring = nullptr;
    g_cache = Cache{};
    WHBGfxFreeShaderGroup(&g_sh);
    g_sh = WHBGfxShaderGroup{};
    g_ready = false;
}

// orig: (host) see mrp_world_gx2.h
void mrp_world_gx2_draw(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp) {
    if (!g_ready || cb == nullptr) return;
    // (testing, RAC1_WIIU_WORLD_STAGE=<n>: how far a call goes - 0 / 1 nothing, 2 the level's textures made, 3 its geometry and colours
    // sent and the state set, no draws, 4 (the default) everything; as RAC1_WIIU_MRP_STAGE's, the depth's clear wiiu_gfx.cpp's)
    static const int stage = std::getenv("RAC1_WIIU_WORLD_STAGE") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_WORLD_STAGE")) : 4;
    if (stage <= 1) return;
    const auto t0 = std::chrono::steady_clock::now();
    const int win_w = static_cast<int>(cb->surface.width), win_h = static_cast<int>(cb->surface.height);
    GX2ContextState* ctx = cb == WHBGfxGetTVColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();
    mrpwb::FrameView F;
    mrpwb::LevelView L;
    const bool have = mrpwb::frame_view(F) && mrpwb::level_view(L);
    static uint64_t logged = 0;
    if (logged < 3 && have && !F.batches->empty()) {  // (the first frames' size, on the log)
        ++logged;
        std::printf("[wiiu] mrp_world_gx2: frame %llu: %zu batches, %zu draws, %zu instances, %zu epochs; level %d: %zu template vertices, %zu indices, %zu buckets\n",
                    static_cast<unsigned long long>(F.serial), F.batches->size(), F.draws->size(), F.insts->size(), F.epochs->size(), L.id, L.tverts->size(),
                    L.tidx->size(), L.buckets);
        std::fflush(stdout);
    }
    if (!have) return;
    if (L.loads != g_tex.loads) {  // (the level's textures: GX2CopySurface changes the GPU's state, the context set again after)
        std::printf("[wiiu] mrp_world_gx2: level %d's textures: %zu arrays\n", L.id, L.buckets);
        std::fflush(stdout);
        tex_sync(L);
        if (ctx != nullptr) GX2SetContextState(ctx);
    }
    if (stage < 3) return;
    geo_sync(L);
    cols_sync();
    if (g_geo.vb == nullptr || g_geo.ib == nullptr || g_cols.tex.surface.image == nullptr || F.batches->empty()) return;
    ++g_calls;
    if (F.serial != g_cache.serial || g_ring_wraps != g_cache.wraps || g_cache.draws.size() != F.draws->size()) cache_reset(F.serial, F.draws->size());

    const std::vector<mrpwb::Batch>& batches = *F.batches;
    const std::vector<mrpwb::DrawCmd>& draws = *F.draws;
    const std::vector<uint32_t>& cmds = *F.cmds;
    const std::vector<mrpwb::InstGpu>& insts = *F.insts;
    const std::vector<std::vector<std::pair<int, uint32_t>>>& epochs = *F.epochs;

    // the frame's plain uniforms (mrp_world_renderer.cpp's set_frame_uniforms)
    auto frame_block = [&] {
        FrameBlock fb{};
        float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
        if (fb_w <= 0.0f) fb_w = 512.0f;
        if (fb_h <= 0.0f) fb_h = 416.0f;
        fb.frame[0] = fb_w;
        fb.frame[1] = fb_h;
        fb.frame[2] = vp.xscale_3d;
        fb.frame[3] = 0.0f;
        double da = 0.0, dbb = 0.0;
        const bool has = render_gs_depth(da, dbb);
        float xo = static_cast<float>(F.env_xyoff & 0xffff), yo = static_cast<float>((F.env_xyoff >> 32) & 0xffff);
        if (F.env_xyoff == 0) {
            xo = (2048.0f - fb_w * 0.5f) * 16.0f;
            yo = (2048.0f - fb_h * 0.5f) * 16.0f;
        }
        fb.xyoff[0] = xo;
        fb.xyoff[1] = yo;
        fb.xyoff[2] = has ? static_cast<float>(da) : 0.5f;
        fb.xyoff[3] = has ? static_cast<float>(dbb) : 0.0f;
        fb.mode[0] = (g_debug & 4) != 0 ? 0 : F.mode;
        fb.mode[1] = (g_debug & 16) != 0 ? 1 : 0;
        fb.mode[2] = static_cast<int32_t>(kColsLog2);
        return block_of(&fb, sizeof fb, g_frame_loc.size);
    };
    if (g_cache.frame == nullptr) g_cache.frame = frame_block();
    if (g_cache.frame == nullptr) return;
    const int mode = (g_debug & 4) != 0 ? 0 : F.mode;

    // GX2's state for the whole call
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetShaderMode(g_sh.vertexShader->mode);
    GX2SetFetchShader(&g_sh.fetchShader);
    GX2SetVertexShader(g_sh.vertexShader);
    GX2SetPixelShader(g_sh.pixelShader);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetRasterizerClipControlEx(TRUE, mode == 1 ? FALSE : TRUE, FALSE);  // (the VU projection: the GL backend's depth clamp)
    // (testing, debug bit 256: the template's first vertex given by the attribute buffer's address instead of the draw's offset)
    const bool by_address = (g_debug & 256) != 0;
    const uint32_t vbytes = static_cast<uint32_t>(g_geo.v_count * sizeof(mrpwb::TVert));
    if (!by_address) GX2SetAttribBuffer(0, vbytes, sizeof(mrpwb::TVert), g_geo.vb);
    if (g_frame_loc.found) GX2SetVertexUniformBlock(g_frame_loc.loc, g_frame_loc.size, g_cache.frame);
    if (g_cols_ok) {
        GX2SetVertexTexture(&g_cols.tex, g_cols_loc);
        GX2SetVertexSampler(&g_cols_sampler, g_cols_loc);
    }
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);

    PixelBlock last_px{};
    const void* last_px_blk = nullptr;
    uint64_t last_px_wraps = 0;
    auto set_pixel = [&](const PixelBlock& pb) {
        if (last_px_blk == nullptr || last_px_wraps != g_ring_wraps || !(last_px == pb)) {
            last_px_blk = block_of(&pb, sizeof pb, g_pixel_loc.size);
            last_px_wraps = g_ring_wraps;
            last_px = pb;
        }
        if (last_px_blk != nullptr && g_pixel_loc.found) GX2SetPixelUniformBlock(g_pixel_loc.loc, g_pixel_loc.size, last_px_blk);
    };
    int cur_bucket = -2;
    uint32_t cur_sampler = ~0u;
    // one draw command: its texture (the unit its record names in the batch's epoch), its instance and record, the indexed draw
    auto draw_one = [&](const mrpwb::Batch& b, uint32_t i) {
        if (g_ring_wraps != g_cache.wraps) {  // (the ring wrapped during this call: what it held is written again as needed)
            const uint64_t serial = g_cache.serial;
            cache_reset(serial, draws.size());
            g_cache.frame = frame_block();
            if (g_cache.frame != nullptr && g_frame_loc.found) GX2SetVertexUniformBlock(g_frame_loc.loc, g_frame_loc.size, g_cache.frame);
            last_px_blk = nullptr;
            set_pixel(last_px);
        }
        const mrpwb::DrawCmd& c = draws[i];
        const uint32_t* rec = &cmds[8 * static_cast<size_t>(c.base_instance)];
        const mrpwb::InstGpu& I = insts[rec[0]];
        const uint32_t kind = I.info[3] & 1u;
        if (((g_debug & 64) != 0 && kind != 0) || ((g_debug & 8) != 0 && kind != 1)) return;  // (testing: the ties / shrubs alone)
        int bucket = -1;
        uint32_t sampler = 0;
        const uint32_t unit = (rec[3] >> 16) & 15u;
        if (unit != 0 && b.epoch >= 0 && static_cast<size_t>(b.epoch) < epochs.size() && unit - 1 < epochs[static_cast<size_t>(b.epoch)].size()) {
            bucket = epochs[static_cast<size_t>(b.epoch)][unit - 1].first;
            sampler = epochs[static_cast<size_t>(b.epoch)][unit - 1].second & 15u;
        }
        if (bucket != cur_bucket) {
            const GX2Texture* t = bucket >= 0 && static_cast<size_t>(bucket) < g_tex.arrays.size() && g_tex.arrays[static_cast<size_t>(bucket)].surface.image != nullptr
                                      ? &g_tex.arrays[static_cast<size_t>(bucket)]
                                      : &g_white;
            if (g_tex_ok) GX2SetPixelTexture(t, g_tex_loc);
            cur_bucket = bucket;
        }
        if (sampler != cur_sampler) {
            if (g_tex_ok) GX2SetPixelSampler(&g_samplers[sampler], g_tex_loc);
            cur_sampler = sampler;
        }
        void*& blk = g_cache.draws[i];
        if (blk == nullptr) {
            DrawBlock d;
            d.inst = I;
            std::memcpy(d.cmd, rec, sizeof d.cmd);
            blk = block_of(&d, sizeof d, g_draw_loc.size);
        }
        if (blk == nullptr) return;
        if (g_draw_loc.found) GX2SetVertexUniformBlock(g_draw_loc.loc, g_draw_loc.size, blk);
        if (stage < 4) return;
        if ((g_debug & 2048) != 0 && g_draws_this_call++ > 0) return;  // (testing: one draw a call)
        static bool dumped = false;
        if (!dumped && c.count >= 3) {  // (the first draw's data, on the log: what the GPU reads)
            dumped = true;
            const uint32_t* ix = static_cast<const uint32_t*>(g_geo.ib) + c.first_index;
            const uint32_t v0 = static_cast<uint32_t>(c.base_vertex) + ix[0];
            const uint8_t* v = static_cast<const uint8_t*>(g_geo.vb) + static_cast<size_t>(v0) * sizeof(mrpwb::TVert);
            std::printf("[wiiu] mrp_world_gx2: first draw: %u indices from %u (%u %u %u) + base %d, record %08x %08x %08x %08x %08x, instance rows T %g %g %g %g, "
                        "morph %g %g %g %g, info %u %u %u %u; vertex %u:",
                        c.count, c.first_index, ix[0], ix[1], ix[2], c.base_vertex, rec[0], rec[1], rec[2], rec[3], rec[4], I.rows[12], I.rows[13], I.rows[14], I.rows[15],
                        I.morph[0], I.morph[1], I.morph[2], I.morph[3], I.info[0], I.info[1], I.info[2], I.info[3], v0);
            for (size_t k = 0; k < sizeof(mrpwb::TVert); ++k) std::printf(" %02x", v[k]);
            const uint8_t ci = v[24];
            const size_t at = static_cast<size_t>(I.info[0]) + ci;
            const uint32_t* colw = static_cast<const uint32_t*>(g_cols.tex.surface.image);
            const uint32_t pitch = g_cols.tex.surface.pitch;
            std::printf("; its colour %zu: texel %08x\n", at, at < g_cols.words ? colw[(at >> kColsLog2) * pitch + (at & ((1u << kColsLog2) - 1))] : 0u);
            std::fflush(stdout);
        }
        if (by_address) {
            GX2SetAttribBuffer(0, vbytes - static_cast<uint32_t>(c.base_vertex) * sizeof(mrpwb::TVert), sizeof(mrpwb::TVert),
                               static_cast<uint8_t*>(g_geo.vb) + static_cast<size_t>(c.base_vertex) * sizeof(mrpwb::TVert));
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + c.first_index, 0, 1);
        } else if ((g_debug & 32) != 0) {
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, static_cast<uint32_t>(c.base_vertex), 1);
        } else {
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + c.first_index,
                             static_cast<uint32_t>(c.base_vertex), 1);
        }
        ++g_draws;
        g_tris += c.count / 3;
    };

    g_draws_this_call = 0;
    const bool grey = (g_debug & 2) != 0;
    const int shade = grey ? 3 : (g_debug & 4096) != 0 ? 1 : (g_debug & 8192) != 0 ? 2 : g_render_shade;
    static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;  // (testing: the failing pixels not drawn, as the GL backend's)
    for (const mrpwb::Batch& b : batches) {
        if (!b.gpu || b.tfrag || b.sky || b.rk.shadow) {  // (the CPU path's, the tfrags', the casters': not drawn here yet)
            ++g_skipped;
            continue;
        }
        {  // the batch's draw commands checked against what they read (the GL backend's batch_refused): one outside is refused
            bool ok = static_cast<uint64_t>(b.first) + b.count <= draws.size();
            for (uint32_t i = b.first; ok && i < b.first + b.count; ++i) {
                const mrpwb::DrawCmd& c = draws[i];
                ok = static_cast<uint64_t>(c.first_index) + c.count <= g_geo.i_count && c.base_vertex >= 0 && static_cast<size_t>(c.base_vertex) <= g_geo.v_count &&
                     c.instances == 1 && (static_cast<uint64_t>(c.base_instance) + 1) * 8 <= cmds.size() && cmds[8 * static_cast<size_t>(c.base_instance)] < insts.size();
            }
            if (!ok) {
                if (g_refused++ < 8)
                    std::printf("[wiiu] mrp_world_gx2: batch refused: first %u count %u (%zu draw commands, %zu records, %zu indices)\n", b.first, b.count, draws.size(),
                                cmds.size() / 8, g_geo.i_count);
                continue;
            }
        }
        ++g_batches;
        const mrpwb::Key& k = b.key;
        const mrpwb::RKey& rk = b.rk;
        if (grey) GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        else gs::set_blend_gx2(k.prim, k.alpha);
        // the depth: the GS's GEQUAL / GREATER on the host's depth as LEQUAL / LESS; no test: no writes (as GL)
        const uint32_t ztst = fld(k.test, 17, 2);
        const bool ztest = db != nullptr && fld(k.test, 16, 1) != 0 && ztst != 1 && (g_debug & 1) == 0;
        const bool zwrite = rk.zwrite != 0;
        const GX2CompareFunction zf = ztst == 0 ? GX2_COMPARE_FUNC_NEVER : ztst == 2 ? GX2_COMPARE_FUNC_LEQUAL : GX2_COMPARE_FUNC_LESS;
        PixelBlock pb{};
        pb.fog_col[0] = static_cast<float>(rk.fogcol & 0xff) / 255.0f;
        pb.fog_col[1] = static_cast<float>((rk.fogcol >> 8) & 0xff) / 255.0f;
        pb.fog_col[2] = static_cast<float>((rk.fogcol >> 16) & 0xff) / 255.0f;
        pb.a[1] = shade;
        pb.a[2] = (rk.fogcol >> 24) & 1 ? 1 : 0;
        pb.a[3] = grey ? 1 : 0;
        // pass 1: the passing pixels of the draws whose alpha test is on (their mode in draw_flags), every pixel of the others
        GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && zwrite ? TRUE : FALSE, zf);
        pb.a[0] = 1;
        set_pixel(pb);
        for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(b, i);
        // pass 2: the failing ones as AFAIL says (RKey::pass2: 1 FB_ONLY the colour with alpha, 2 ZB_ONLY the depth, 3 RGB_ONLY the
        // colour without alpha)
        if (rk.pass2 != 0 && !one_pass && !grey) {
            const uint32_t afail = rk.pass2;
            GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && afail == 2 && zwrite ? TRUE : FALSE, zf);
            set_mask(afail == 2 ? none : afail == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA);
            pb.a[0] = 2;
            set_pixel(pb);
            for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(b, i);
            set_mask(GX2_CHANNEL_MASK_RGBA);
        }
    }

    // the state left as the header says
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    set_mask(GX2_CHANNEL_MASK_RGBA);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetRasterizerClipControlEx(TRUE, TRUE, FALSE);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

    g_cpu_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (g_calls % 120 == 0) {  // (the log: what was drawn, the CPU's time in these calls)
        std::printf("[wiiu] mrp_world_gx2: last 120 calls: %.1f draws, %.1f triangles, %.1f batches, %.2f ms CPU a call; %llu batches refused, %llu skipped (not "
                    "the GPU path's); %zu instances\n",
                    static_cast<double>(g_draws) / 120.0, static_cast<double>(g_tris) / 120.0, static_cast<double>(g_batches) / 120.0, g_cpu_ms / 120.0,
                    static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_skipped), insts.size());
        std::fflush(stdout);
        g_draws = g_batches = g_tris = 0;
        g_cpu_ms = 0.0;
    }
}

}  // namespace rac1
