// The MRP world's ties and shrubs drawn with GX2 on the Wii U (MRP_LITE.md phase 3; how a caller uses it: mrp_world_gx2.h). A port
// of mrp_world_renderer.cpp's GL draw (draw_frame, its GPU path batches) over the same CPU preparation, read through
// mrp_world_backend.inc:
//
// - the level: its packet templates (TVert, 28 bytes, built by the PPC: big-endian; each with its template run, mrpwm::VertR) and
//   their u32 indices (relative to the template's first vertex, and absolute; big-endian: GX2_INDEX_TYPE_U32) copied once into MEM2
//   buffers; its texture buckets as GX2 2D-array textures with
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
//   MEM2 ring with each 32-bit word byte-swapped (GX2 reads the uniform blocks as little-endian words: mrp_moby_gx2.cpp's block_of).
//   That is the per-draw path (RAC1_WIIU_WORLD_MERGE=0); by default the draw commands are merged (mrp_world_merge.inc: an instance's
//   consecutive commands one draw, consecutive instances drawing the same ones one instanced draw, in the same order): one block slot
//   per instance per frame, the records in a run table (R32UI, the vertex's run), the batch's texture units bound at once (uT1..uT15);
// - the GPU state per batch as the GL backend sets it: the GS's blending (gs::set_blend_gx2), the depth test (GEQUAL / GREATER as
//   LEQUAL / LESS on the host's depth, written when ZBUF's ZMSK is off and the test on), the alpha test's two passes (pass 1 the passing
//   pixels of the draws whose test is on, every pixel of the others; pass 2, RKey::pass2, the failing ones as AFAIL says: FB_ONLY the
//   colour, ZB_ONLY the depth alone, RGB_ONLY the colour without alpha);
// - the world's projection (g_mrp_world_projection 0, the PC's default: the instances' clip matrices), as the mobys' (mrp_moby_gx2_init),
//   whose depth the world shares. The game's own VU1 projection (mode 1, RAC1_WIIU_WORLD_PROJ=vu) repeats the GS's 16-bit screen
//   coordinates: an instance crossing the camera's plane wraps into planes over the screen (the VU's clipping programs keep those
//   from the PS2's GS; the static path draws its instances whole) - seen on the console.
//
// - the tfrags (MRP_LITE.md phase 4): the GPU path's tfrag batches over the compute program's vertices / triangles, made by the CPU
//   (mrp_world_renderer.cpp tfrag_frame_cpu: GpuV, 72 bytes, big-endian; u32 indices, absolute) and copied per frame into a MEM2 ring
//   of their own (GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER), drawn with shaders/mrp_tfrag.gsh (mrp_tfrag.vert: the PC's kVs in the
//   world's projection, the frame's view-projection a uniform block; mrp_world.frag) before the ties / shrubs, in the frame's batch
//   order (the tfrag pass's range comes first in the chain); a batch's triangles sample by texture unit (each vertex's draw_flags) and
//   GX2 binds one texture: one GX2DrawIndexedEx per run of one unit (mrpwb::TfragRange), its texture bound before it. The 32-bit
//   attributes swapped by the fetch shader (GX2_ENDIAN_SWAP_8_IN_32; tfrag debug bit 1024 not), the RGBA bytes as they are.
//
// Not drawn: the CPU path's batches (the static path makes none; the tie A state packet has no triangles), the sky, the shrub far
// variants, the casters, the modern lighting. Host code.
#include "mrp_world_gx2.h"

#include "gs_draw_gx2.h"
#include "mrp_world_merge.inc"

#include <game/ovl/ovl_common.h>
#include <game/render/mrp_moby_renderer.h>
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
    int32_t mode2[4];  // 1 the instance's slot by gl_InstanceID (the instanced draws), the run table's row's log2, 0, 0
};
static_assert(sizeof(FrameBlock) == 64);
// a slot of the vertex shader's WorldDraw block (256 bytes: a block may start at any slot, GX2_UNIFORM_BLOCK_ALIGNMENT): the instance
// and a record (cmd[5] 0: the record is this one, the per-draw path's; 1: the vertex's run's in the run table, the merged draws')
struct alignas(16) DrawBlock {
    mrpwb::InstGpu inst;
    uint32_t cmd[8];
};
static_assert(sizeof(DrawBlock) == 192);
constexpr uint32_t kSlot = 256;
// The pixel shader's (mrp_world.frag's WorldPixel)
struct alignas(16) PixelBlock {
    float fog_col[4];
    int32_t a[4];  // the alpha test's pass, the shade debug, FGE, 1 the alpha test off (testing)
    int32_t b[4];  // 1 the texture by the triangle's unit (the merged draws: the batch's units bound), 0, 0, 0
    bool operator==(const PixelBlock& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};
static_assert(sizeof(PixelBlock) == 48);
// The tfrag vertex shader's (mrp_tfrag.vert's TfragFrame)
struct alignas(16) TfragFrameBlock {
    float vp[16];  // the world renderers' view-projection, column-major
};
static_assert(sizeof(TfragFrameBlock) == 64);

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

bool g_ready = false;
WHBGfxShaderGroup g_sh{};
struct BlockLoc {
    uint32_t loc = 0, size = 0;
    bool found = false;
};
BlockLoc g_frame_loc, g_draw_loc, g_pixel_loc;
uint32_t g_tex_loc = 0, g_cols_loc = 0, g_runs_loc = 0;
bool g_tex_ok = false, g_cols_ok = false, g_runs_ok = false;
uint32_t g_unit_loc[16] = {};  // (the merged draws' texture units: uTex, uT1..uT15)
bool g_units_ok = false;
// RAC1_WIIU_WORLD_MERGE (mrpwm::Mode): 0 one GX2 draw per draw command (as before), 1 an instance's consecutive commands of one texture
// unit one draw, 2 of any unit (the batch's units bound at once), 3 (the default) and consecutive instances drawing the same commands
// one instanced draw
int g_merge = mrpwm::kInstanced;
GX2Sampler g_samplers[16];  // [TEX1 / CLAMP bits: 0 MMAG linear, 1 MMIN linear, 2 wrap s, 3 wrap t] (mip mode 0: the GS's level, textureLod)
GX2Sampler g_cols_sampler;  // (the colours' texture: point, clamped)
// the tfrags' shaders (shaders/mrp_tfrag.gsh): their fetch shaders by the 32-bit attributes' byte order ([0] swapped, the default; [1] as
// they are: tfrag debug bit 1024), their blocks and sampler
bool g_tready = false;
WHBGfxShaderGroup g_tsh{};
GX2FetchShader g_tfetch[2]{};
void* g_tfetch_prog[2] = {};
BlockLoc g_tframe_loc, g_tpixel_loc;
uint32_t g_ttex_loc = 0;
bool g_ttex_ok = false;

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
// `bytes` of `data` written at `p`, each 32-bit word swapped (GX2 reads the shaders' blocks as little-endian words)
void words_swapped(void* p, const void* data, uint32_t bytes) {
    const auto* src = static_cast<const uint8_t*>(data);
    auto* dst = static_cast<uint32_t*>(p);
    for (uint32_t i = 0; i + 4 <= bytes; i += 4) {
        uint32_t w;
        std::memcpy(&w, src + i, 4);
        dst[i / 4] = __builtin_bswap32(w);
    }
}
// a block of `size` bytes holding `data` (`bytes` of it; the rest left as it is: never read), each 32-bit word swapped (GX2 reads the
// shaders' blocks as little-endian words), written out of the CPU's cache
void* block_of(const void* data, uint32_t bytes, uint32_t size) {
    void* p = ring_alloc(size);
    if (p == nullptr) return nullptr;
    words_swapped(p, data, std::min(bytes, size));
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), p, size);
    return p;
}

// The merged draws' frame data (mrpwm::Plan: the instances' slots, the run table, the frame's own indices), written once per frame
// (the TV's call; the GamePad's reuses it) into a MEM2 ring of their own: nothing else allocates there between a frame's calls, so a
// wrap - the GPU waited for first - happens only while a frame's data is written
constexpr uint32_t kPlanRingBytes = 8u << 20;
uint8_t* g_pring = nullptr;
uint32_t g_pring_off = 0;
uint64_t g_pring_wraps = 0;
void* plan_alloc(uint32_t bytes, uint32_t align) {
    uint32_t off = (g_pring_off + align - 1) & ~(align - 1);
    if (bytes > kPlanRingBytes) return nullptr;
    if (off + bytes > kPlanRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_pring_wraps;
    }
    g_pring_off = off + bytes;
    return g_pring + off;
}

// The tfrags' vertices / indices, a frame's appended to a ring in MEM2 of their own (as the blocks' ring: before it wraps over what the
// GPU may still read the CPU waits for it); a frame's twice the size of Veldin's busiest (~2200 vertices: 160 KB) many times over
constexpr uint32_t kGeoRingBytes = 8u << 20;
uint8_t* g_gring = nullptr;
uint32_t g_gring_off = 0;
uint64_t g_gring_wraps = 0;
void* geo_alloc(uint32_t bytes) {
    uint32_t off = (g_gring_off + GX2_VERTEX_BUFFER_ALIGNMENT - 1) & ~(GX2_VERTEX_BUFFER_ALIGNMENT - 1);
    if (bytes > kGeoRingBytes) return nullptr;
    if (off + bytes > kGeoRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_gring_wraps;
    }
    g_gring_off = off + bytes;
    return g_gring + off;
}

void* mem_alloc(uint32_t bytes, uint32_t align) { return MEMAllocFromDefaultHeapEx(std::max<uint32_t>(bytes, 4), static_cast<int32_t>(align)); }
void mem_free(void*& p) {
    if (p != nullptr) MEMFreeToDefaultHeap(p);
    p = nullptr;
}

// ---- the level's data on the GPU ----------------------------------------------------------------------------------------

// its templates' vertices and indices (static for the level: copied once per level loaded). The vertices with their template run
// (mrpwm::VertR, 32 bytes: mrpwm::level_build - the vertices two runs share copied after the level's), the indices twice: relative to
// their template's first vertex (tidx: the per-draw path, a draw's offset its base vertex) and absolute (the merged draws')
mrpwm::Level g_mlevel;  // (the CPU's copy: the frame's own indices are copied from it)
struct Geometry {
    uint64_t loads = ~0ull;
    void* vb = nullptr;
    void* ib = nullptr;
    void* ib_abs = nullptr;
    size_t v_count = 0, i_count = 0;
} g_geo;
void geo_free() {
    mem_free(g_geo.vb);
    mem_free(g_geo.ib);
    mem_free(g_geo.ib_abs);
    g_geo = Geometry{};
}
void geo_sync(const mrpwb::LevelView& L) {
    if (L.loads == g_geo.loads) return;
    GX2DrawDone();  // (the old buffers may still be read)
    geo_free();
    g_geo.loads = L.loads;
    const auto t0 = std::chrono::steady_clock::now();
    std::vector<mrpwb::TplRunInfo> runs;
    mrpwb::level_runs(runs);
    mrpwm::level_build(L, std::move(runs), g_mlevel);
    const std::vector<mrpwm::VertR>& v = g_mlevel.verts;
    const std::vector<uint32_t>& ix = *L.tidx;
    const uint32_t vbytes = static_cast<uint32_t>(v.size() * sizeof(mrpwm::VertR)), ibytes = static_cast<uint32_t>(ix.size() * sizeof(uint32_t));
    g_geo.vb = mem_alloc(vbytes, GX2_VERTEX_BUFFER_ALIGNMENT);
    g_geo.ib = mem_alloc(ibytes, GX2_INDEX_BUFFER_ALIGNMENT);
    g_geo.ib_abs = mem_alloc(ibytes, GX2_INDEX_BUFFER_ALIGNMENT);
    if (g_geo.vb == nullptr || g_geo.ib == nullptr || g_geo.ib_abs == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: no memory for the level's %zu template vertices / %zu indices\n", v.size(), ix.size());
        geo_free();
        g_geo.loads = L.loads;  // (not tried again every frame)
        return;
    }
    if (vbytes > 0) std::memcpy(g_geo.vb, v.data(), vbytes);
    if (ibytes > 0) std::memcpy(g_geo.ib, ix.data(), ibytes);
    if (ibytes > 0) std::memcpy(g_geo.ib_abs, g_mlevel.idx.data(), ibytes);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g_geo.vb, std::max<uint32_t>(vbytes, 4));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g_geo.ib, std::max<uint32_t>(ibytes, 4));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, g_geo.ib_abs, std::max<uint32_t>(ibytes, 4));
    g_geo.v_count = v.size();
    g_geo.i_count = ix.size();
    std::printf("[wiiu] mrp_world_gx2: level %d: %zu template vertices (%zu copied for their runs; %u KB), %zu indices (%u KB, twice), %zu runs (%zu left out) "
                "in %.1f ms\n",
                L.id, v.size(), g_mlevel.copies, vbytes / 1024, ix.size(), ibytes / 1024, g_mlevel.first.size(), g_mlevel.bad,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
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

int g_debug = 0;   // (RAC1_WIIU_WORLD_DEBUG's mode for this frame: mrp_world_gx2.h)
int g_tdebug = 0;  // (RAC1_WIIU_TFRAG_DEBUG's)

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
// the R32UI texels' words byte-swapped (the resident colours', the run table's): RAC1_WIIU_WORLD_RCSWAP, debug bit 1024 the other order
bool cols_swap() {
    static const bool rcswap = std::getenv("RAC1_WIIU_WORLD_RCSWAP") == nullptr || std::atoi(std::getenv("RAC1_WIIU_WORLD_RCSWAP")) != 0;
    return rcswap != ((g_debug & 1024) != 0);
}
// the colours' changes since the last call written (the whole table: a new level, its size changed, the byte order changed)
void cols_sync() {
    mrpwb::ColoursView cv;
    mrpwb::colours_view(cv);
    const std::vector<uint32_t>& m = *cv.words;
    const bool swap = cols_swap();
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

// the tfrags' data a frame's first call wrote (its vertices / indices in the geometry ring, its frame block in the blocks' ring), reused
// by the frame's next call while the frame and both rings' wraps are the same
struct TfragCache {
    uint64_t serial = ~0ull, gwraps = ~0ull, wraps = ~0ull;
    void* vb = nullptr;
    void* ib = nullptr;
    uint32_t nverts = 0, nidx = 0;
    void* frame = nullptr;
} g_tcache;

uint64_t g_tris = 0;
uint32_t g_draws_this_call = 0;
uint64_t g_calls = 0, g_draws = 0, g_batches = 0, g_refused = 0, g_skipped = 0;
uint64_t g_cmds = 0, g_blocks = 0, g_merged_calls = 0;  // (the log's: the draw commands drawn, the uniform blocks written, the merged calls)
double g_cpu_ms = 0.0, g_plan_ms = 0.0;
// the tfrags' (the log per 120 calls): GX2 draws, triangles, batches, frames, the CPU route's time (ms: sum, most), vertices
uint64_t g_tdraws = 0, g_ttris = 0, g_tbatches = 0, g_tframes = 0, g_tverts = 0, g_trefused = 0;
double g_troute_ms = 0.0, g_troute_max = 0.0;
uint64_t g_tlast_serial = ~0ull;

// ---- the merged draws' frame (mrpwm::Plan) ------------------------------------------------------------------------------
constexpr uint32_t kRunsLog2 = 10;  // (the run table: 1 << kRunsLog2 texels a row, 4 a run)
struct Merged {
    uint64_t serial = ~0ull, pwraps = ~0ull, loads = ~0ull;
    int mode = -1, kinds = -1;
    bool swap = false, ok = false;
    mrpwm::Plan plan;
    GX2Texture runs{};          // the run table (R32UI, linear): its image in the plan ring, a frame's
    uint32_t runs_rows = 0;
    std::vector<void*> blk;     // per item: its block (its first instance's slot)
    std::vector<uint32_t> item_slot;
    std::vector<int32_t> inst_slot;
    std::vector<uint8_t> written;
    void* own = nullptr;        // the frame's own indices
} g_mg;

// orig: (host) the frame's merged draws made and written (once per frame: false, none - the per-draw path draws this call): the plan
// (mrpwm::plan_build), the run table (the runs drawn: their 4 words, swapped as the resident colours' - the same R32UI reads -), the
// frame's own indices, the uniform blocks in one allocation: a slot per instance drawn alone (its InstGpu, the record's flag 1: the run
// table's), per lone draw command (its record, flag 0: as the per-draw path), per instanced item its instances' slots in a row (the
// previous item's reused when its instances are the same)
bool merged_sync(const mrpwb::FrameView& F, int kinds, bool swap) {
    Merged& G = g_mg;
    if (G.ok && G.serial == F.serial && G.pwraps == g_pring_wraps && G.mode == g_merge && G.kinds == kinds && G.loads == g_geo.loads && G.swap == swap) return true;
    const auto t0 = std::chrono::steady_clock::now();
    G.ok = false;
    G.serial = F.serial;
    G.mode = g_merge;
    G.kinds = kinds;
    G.loads = g_geo.loads;
    G.swap = swap;
    const uint64_t wraps0 = g_pring_wraps;
    mrpwm::Plan& P = G.plan;
    mrpwm::plan_build(F, g_mlevel, kinds, g_merge, P);
    const std::vector<mrpwb::InstGpu>& insts = *F.insts;
    const std::vector<uint32_t>& cmds = *F.cmds;
    const std::vector<mrpwb::DrawCmd>& draws = *F.draws;
    // the run table
    const uint32_t rows = std::max<uint32_t>(1, static_cast<uint32_t>((g_mlevel.first.size() * 4 + (1u << kRunsLog2) - 1) >> kRunsLog2));
    GX2Texture& t = G.runs;
    if (G.runs_rows != rows) {
        t = GX2Texture{};
        GX2Surface& rs = t.surface;
        rs.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        rs.width = 1u << kRunsLog2;
        rs.height = rows;
        rs.depth = 1;
        rs.mipLevels = 1;
        rs.format = GX2_SURFACE_FORMAT_UINT_R32;
        rs.aa = GX2_AA_MODE1X;
        rs.use = GX2_SURFACE_USE_TEXTURE;
        rs.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        t.viewNumMips = 1;
        t.viewNumSlices = 1;
        t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
        GX2CalcSurfaceSizeAndAlignment(&rs);
        GX2InitTextureRegs(&t);
        G.runs_rows = rows;
    }
    GX2Surface& rs = t.surface;
    auto* img = static_cast<uint32_t*>(plan_alloc(rs.imageSize, std::max<uint32_t>(rs.alignment, GX2_UNIFORM_BLOCK_ALIGNMENT)));
    if (img == nullptr) return false;
    for (const uint32_t g : P.runs_used)
        for (uint32_t k = 0; k < 4; ++k) {
            const uint32_t i = 4 * g + k, w = P.table[i];
            img[static_cast<size_t>(i >> kRunsLog2) * rs.pitch + (i & ((1u << kRunsLog2) - 1))] = swap ? __builtin_bswap32(w) : w;
        }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, img, rs.imageSize);
    rs.image = img;
    GX2InitTextureRegs(&t);
    // the frame's own indices (big-endian u32, as the level's)
    G.own = nullptr;
    if (!P.idx.empty()) {
        const uint32_t bytes = static_cast<uint32_t>(P.idx.size() * sizeof(uint32_t));
        G.own = plan_alloc(bytes, GX2_INDEX_BUFFER_ALIGNMENT);
        if (G.own == nullptr) return false;
        std::memcpy(G.own, P.idx.data(), bytes);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, G.own, bytes);
    }
    // the blocks: the slots laid out, then written in one allocation
    const size_t n = P.items.size();
    G.item_slot.assign(n, 0);
    G.inst_slot.assign(insts.size(), -1);
    uint32_t nslots = 0;
    size_t prev = ~size_t{0};  // (the previous instance item)
    for (size_t k = 0; k < n; ++k) {
        const mrpwm::Item& it = P.items[k];
        if (it.lone) {
            G.item_slot[k] = nslots++;
            continue;
        }
        if (it.ninst == 1) {
            int32_t& is = G.inst_slot[P.ilist[it.inst]];
            if (is < 0) is = static_cast<int32_t>(nslots++);
            G.item_slot[k] = static_cast<uint32_t>(is);
        } else if (prev != ~size_t{0} && P.items[prev].ninst == it.ninst &&
                   std::equal(P.ilist.begin() + it.inst, P.ilist.begin() + it.inst + it.ninst, P.ilist.begin() + P.items[prev].inst)) {
            G.item_slot[k] = G.item_slot[prev];
        } else {
            G.item_slot[k] = nslots;
            nslots += it.ninst;
        }
        prev = k;
    }
    auto* blocks = static_cast<uint8_t*>(nslots > 0 ? plan_alloc(nslots * kSlot, GX2_UNIFORM_BLOCK_ALIGNMENT) : nullptr);
    if (nslots > 0 && blocks == nullptr) return false;
    G.written.assign(nslots, 0);
    auto put = [&](uint32_t slot, const mrpwb::InstGpu& inst, const uint32_t* rec) {
        if (G.written[slot]) return;
        G.written[slot] = 1;
        DrawBlock d;
        d.inst = inst;
        std::memcpy(d.cmd, rec, sizeof d.cmd);
        words_swapped(blocks + static_cast<size_t>(slot) * kSlot, &d, sizeof d);
    };
    G.blk.assign(n, nullptr);
    for (size_t k = 0; k < n; ++k) {
        const mrpwm::Item& it = P.items[k];
        const uint32_t slot = G.item_slot[k];
        G.blk[k] = blocks + static_cast<size_t>(slot) * kSlot;
        if (it.lone) {
            const uint32_t* rec = &cmds[8 * static_cast<size_t>(draws[it.inst].base_instance)];
            put(slot, insts[rec[0]], rec);
            continue;
        }
        for (uint32_t q = 0; q < it.ninst; ++q) {
            const uint32_t i = P.ilist[it.inst + q];
            const uint32_t rec[8] = {i, 0, 0, 0, 0, 1, 0, 0};
            put(slot + q, insts[i], rec);
        }
    }
    if (nslots > 0) GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), blocks, nslots * kSlot);
    if (g_pring_wraps > wraps0 + 1) return false;  // (more than the ring in one frame: its first data overwritten)
    G.pwraps = g_pring_wraps;
    G.ok = true;
    g_blocks += nslots;
    g_plan_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    return true;
}

}  // namespace

// orig: (host) see mrp_world_gx2.h
bool mrp_world_gx2_ready() { return g_ready; }

int& mrp_world_gx2_merge_mode() { return g_merge; }

// orig: (host) see mrp_world_backend.inc
bool mrp_world_gx2_tfrag_ready() { return g_ready && g_tready; }

namespace {
// a debug list's mode at `game_frame` (each mode for 50 game frames from `at`, the last one kept)
int debug_mode(const char* list, uint64_t at, uint64_t game_frame) {
    int mode = 0;
    if (list != nullptr && game_frame >= at) {
        uint64_t band = (game_frame - at) / 50;
        const char* q = list;  // (past the list's end its last mode stays)
        for (const char* n = std::strchr(q, ','); band > 0 && n != nullptr; --band, n = std::strchr(q, ',')) q = n + 1;
        mode = std::atoi(q);
    }
    return mode;
}
}  // namespace

// orig: (host) see mrp_world_gx2.h
void mrp_world_gx2_debug_frame(uint64_t game_frame) {
    static const char* list = std::getenv("RAC1_WIIU_WORLD_DEBUG");
    static const uint64_t at = std::getenv("RAC1_WIIU_WORLD_DEBUG_AT") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_WORLD_DEBUG_AT"), nullptr, 10) : 0;
    const int mode = debug_mode(list, at, game_frame);
    if (mode != g_debug) {
        std::printf("[wiiu] mrp_world_gx2: debug mode %d from game frame %llu\n", mode, static_cast<unsigned long long>(game_frame));
        std::fflush(stdout);
        g_cache.serial = ~0ull;  // (the frame's blocks written again: the mode is in them)
    }
    g_debug = mode;
    static const char* tlist = std::getenv("RAC1_WIIU_TFRAG_DEBUG");
    static const uint64_t tat = std::getenv("RAC1_WIIU_TFRAG_DEBUG_AT") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_TFRAG_DEBUG_AT"), nullptr, 10) : 0;
    const int tmode = debug_mode(tlist, tat, game_frame);
    if (tmode != g_tdebug) {
        std::printf("[wiiu] mrp_world_gx2: tfrag debug mode %d from game frame %llu\n", tmode, static_cast<unsigned long long>(game_frame));
        std::fflush(stdout);
    }
    g_tdebug = tmode;
}

namespace {
// orig: (host) the tfrags' shaders, fetch shaders and geometry ring freed (the GPU done with them: the caller waited)
void tfrag_free() {
    for (int k = 0; k < 2; ++k) {
        if (g_tfetch_prog[k] != nullptr) MEMFreeToDefaultHeap(g_tfetch_prog[k]);
        g_tfetch_prog[k] = nullptr;
        g_tfetch[k] = GX2FetchShader{};
    }
    if (g_gring != nullptr) MEMFreeToDefaultHeap(g_gring);
    g_gring = nullptr;
    if (g_tsh.vertexShader != nullptr || g_tsh.pixelShader != nullptr) WHBGfxFreeShaderGroup(&g_tsh);
    g_tsh = WHBGfxShaderGroup{};
    g_tcache = TfragCache{};
    g_tready = false;
}

// orig: (host) the tfrags' shaders (shaders/mrp_tfrag.gsh, or RAC1_WIIU_TFRAG_GSH's) and their geometry ring; RAC1_WIIU_TFRAG=0: not
// (no tfrags made nor drawn)
void tfrag_init() {
    const char* on = std::getenv("RAC1_WIIU_TFRAG");
    if (on != nullptr && std::atoi(on) == 0) {
        std::printf("[wiiu] mrp_world_gx2: the tfrags off (RAC1_WIIU_TFRAG=0)\n");
        return;
    }
    std::vector<uint8_t> gsh;
    const char* gsh_env = std::getenv("RAC1_WIIU_TFRAG_GSH");
    const bool variant = gsh_env != nullptr && gsh_env[0] != 0;
    if (std::FILE* f = std::fopen(variant ? gsh_env : "shaders/mrp_tfrag.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_tsh, 0, gsh.data())) {
        std::printf("[wiiu] mrp_world_gx2: shaders/mrp_tfrag.gsh not loaded: no tfrags\n");
        g_tsh = WHBGfxShaderGroup{};
        return;
    }
    // The vertex's attributes (mrpwb::GpuV, 72 bytes, written by the PPC): the floats and the 32-bit integers big-endian, swapped
    // within each 32 bits (8_IN_32; the second fetch shader: not swapped), the RGBA bytes not swapped (byte 0 is R). Fewer components
    // than four: the rest 0, w 1 (as WHB's)
    const auto f32x4 = GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32, f32x3 = GX2_ATTRIB_FORMAT_FLOAT_32_32_32, f32x1 = GX2_ATTRIB_FORMAT_FLOAT_32;
    const auto u32x2 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_32_32);
    const auto u8x4 = GX2_ATTRIB_FORMAT_UINT_8_8_8_8;
    struct A {
        const char* name;
        uint32_t offset;
        GX2AttribFormat format;
        uint32_t comps;
    };
    const A attrs[] = {
        {"aWorld", offsetof(mrpwb::GpuV, wp), f32x4, 4}, {"aStq", offsetof(mrpwb::GpuV, stq), f32x3, 3},  {"aLodQ", offsetof(mrpwb::GpuV, lodq), f32x1, 1},
        {"aRgba", offsetof(mrpwb::GpuV, rgba), u8x4, 4}, {"aFog", offsetof(mrpwb::GpuV, fog), f32x1, 1},  {"aTex", offsetof(mrpwb::GpuV, word), u32x2, 2},
        {"aState", offsetof(mrpwb::GpuV, state), u32x2, 2},
    };
    GX2AttribStream streams[2][8]{};
    uint32_t n = 0;
    const GX2VertexShader* vs = g_tsh.vertexShader;
    for (const A& a : attrs) {
        const GX2AttribVar* var = nullptr;
        for (uint32_t i = 0; i < vs->attribVarCount; ++i)
            if (std::strcmp(vs->attribVars[i].name, a.name) == 0) var = &vs->attribVars[i];
        if (var == nullptr) continue;  // (one the compiler dropped: not read)
        for (int k = 0; k < 2; ++k) {
            GX2AttribStream& st = streams[k][n];
            st.location = var->location;
            st.buffer = 0;
            st.offset = a.offset;
            st.format = a.format;
            st.type = GX2_ATTRIB_INDEX_PER_VERTEX;
            st.aluDivisor = 0;
            st.mask = a.comps == 4   ? GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W)
                      : a.comps == 3 ? GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_1)
                      : a.comps == 2 ? GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1)
                                     : GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1);
            st.endianSwap = a.format == u8x4 || k == 1 ? GX2_ENDIAN_SWAP_NONE : GX2_ENDIAN_SWAP_8_IN_32;
        }
        ++n;
    }
    std::printf("[wiiu] mrp_world_gx2: tfrag: %u attributes:", n);
    for (uint32_t i = 0; i < n; ++i) std::printf(" loc %u off %u fmt %#x", streams[0][i].location, streams[0][i].offset, streams[0][i].format);
    std::printf("\n");
    const uint32_t size = GX2CalcFetchShaderSizeEx(n, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    for (int k = 0; k < 2; ++k) {
        g_tfetch_prog[k] = mem_alloc(size, GX2_SHADER_PROGRAM_ALIGNMENT);
        if (g_tfetch_prog[k] == nullptr) {
            std::printf("[wiiu] mrp_world_gx2: tfrag: fetch shader not made\n");
            tfrag_free();
            return;
        }
        GX2InitFetchShaderEx(&g_tfetch[k], static_cast<uint8_t*>(g_tfetch_prog[k]), n, streams[k], GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, g_tfetch_prog[k], size);
    }
    auto find = [](const GX2UniformBlock* blocks, uint32_t nb, const char* name, uint32_t min_size, BlockLoc& out) {
        for (uint32_t i = 0; i < nb; ++i)
            if (std::strcmp(blocks[i].name, name) == 0) {
                out.loc = blocks[i].offset;
                out.size = (std::max(blocks[i].size, min_size) + 15u) & ~15u;
                out.found = true;
            }
    };
    find(vs->uniformBlocks, vs->uniformBlockCount, "TfragFrame", sizeof(TfragFrameBlock), g_tframe_loc);
    const GX2PixelShader* ps = g_tsh.pixelShader;
    find(ps->uniformBlocks, ps->uniformBlockCount, "WorldPixel", sizeof(PixelBlock), g_tpixel_loc);
    for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
        if (std::strcmp(ps->samplerVars[i].name, "uTex") == 0) {
            g_ttex_loc = ps->samplerVars[i].location;
            g_ttex_ok = true;
        }
    g_gring = static_cast<uint8_t*>(mem_alloc(kGeoRingBytes, GX2_VERTEX_BUFFER_ALIGNMENT));
    g_gring_off = 0;
    if ((!variant && (!g_tframe_loc.found || !g_tpixel_loc.found || !g_ttex_ok)) || g_gring == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: tfrag: blocks frame %d pixel %d, sampler %d, memory %d: no tfrags\n", g_tframe_loc.found, g_tpixel_loc.found, g_ttex_ok,
                    g_gring != nullptr);
        tfrag_free();
        return;
    }
    g_tready = true;
    std::printf("[wiiu] mrp_world_gx2: tfrag up (%s; blocks: frame %u pixel %u bytes; geometry ring %u KB)\n", variant ? gsh_env : "shaders/mrp_tfrag.gsh", g_tframe_loc.size,
                g_tpixel_loc.size, kGeoRingBytes / 1024);
}
}  // namespace

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
    // (the vertex buffer holds mrpwm::VertR, 32 bytes: the TVert, then its run - a u32, swapped within its 32 bits, 8_IN_32)
    const auto s16x4 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_SIGNED | GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_16_16_16_16);
    const auto u8x4 = GX2_ATTRIB_FORMAT_UINT_8_8_8_8;
    const auto u32x1 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_32);
    struct A {
        const char* name;
        uint32_t offset;
        GX2AttribFormat format;
    };
    const A attrs[] = {
        {"aPos", offsetof(mrpwm::VertR, v) + offsetof(mrpwb::TVert, pos), s16x4},
        {"aDelta", offsetof(mrpwm::VertR, v) + offsetof(mrpwb::TVert, delta), s16x4},
        {"aUv", offsetof(mrpwm::VertR, v) + offsetof(mrpwb::TVert, uv), s16x4},
        {"aCol", offsetof(mrpwm::VertR, v) + offsetof(mrpwb::TVert, col), u8x4},
        {"aRun", offsetof(mrpwm::VertR, run), u32x1},
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
        s.mask = a.format == u32x1 ? GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_0, GX2_SQ_SEL_0, GX2_SQ_SEL_1) : GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
        s.endianSwap = a.format == u8x4 ? GX2_ENDIAN_SWAP_NONE : a.format == u32x1 ? GX2_ENDIAN_SWAP_8_IN_32 : GX2_ENDIAN_SWAP_8_IN_16;
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
    for (uint32_t i = 0; i < vs->samplerVarCount; ++i) {
        if (std::strcmp(vs->samplerVars[i].name, "uCols") == 0) {
            g_cols_loc = vs->samplerVars[i].location;
            g_cols_ok = true;
        }
        if (std::strcmp(vs->samplerVars[i].name, "uRuns") == 0) {
            g_runs_loc = vs->samplerVars[i].location;
            g_runs_ok = true;
        }
    }
    {  // the merged draws' texture units (uTex, uT1..uT15)
        int found = g_tex_ok ? 1 : 0;
        g_unit_loc[0] = g_tex_loc;
        for (uint32_t u = 1; u < 16; ++u) {
            char name[8];
            std::snprintf(name, sizeof name, "uT%u", u);
            for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
                if (std::strcmp(ps->samplerVars[i].name, name) == 0) {
                    g_unit_loc[u] = ps->samplerVars[i].location;
                    ++found;
                }
        }
        g_units_ok = found == 16;
    }
    // (testing, RAC1_WIIU_WORLD_MERGE=<n>: the ties' / shrubs' draws merged as mrpwm::Mode says - 0 one draw per draw command, as before)
    if (const char* e = std::getenv("RAC1_WIIU_WORLD_MERGE")) g_merge = std::clamp(std::atoi(e), 0, 3);
    if (g_merge > 0 && (!g_runs_ok || g_draw_loc.size < 2 * kSlot)) {
        std::printf("[wiiu] mrp_world_gx2: the merged draws off: no run table / instance slots in the shaders\n");
        g_merge = 0;
    }
    if (g_merge >= 2 && !g_units_ok) {
        std::printf("[wiiu] mrp_world_gx2: the merged draws by unit only: the shaders lack the units' samplers\n");
        g_merge = 1;
    }
    g_pring = g_merge > 0 ? static_cast<uint8_t*>(mem_alloc(kPlanRingBytes, GX2_UNIFORM_BLOCK_ALIGNMENT)) : nullptr;
    g_pring_off = 0;
    if (g_merge > 0 && g_pring == nullptr) g_merge = 0;
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
        if (g_pring != nullptr) MEMFreeToDefaultHeap(g_pring);
        g_pring = nullptr;
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
    std::printf("[wiiu] mrp_world_gx2: up (blocks: frame %u draw %u pixel %u bytes; merged draws: mode %d)\n", g_frame_loc.size, g_draw_loc.size, g_pixel_loc.size, g_merge);
    tfrag_init();
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
    if (g_pring != nullptr) MEMFreeToDefaultHeap(g_pring);
    g_pring = nullptr;
    g_mg = Merged{};
    g_mlevel = mrpwm::Level{};
    g_cache = Cache{};
    tfrag_free();
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
        fb.mode2[0] = g_merge >= mrpwm::kInstanced ? 1 : 0;  // (a draw of one instance: gl_InstanceID 0, its first slot either way)
        fb.mode2[1] = static_cast<int32_t>(kRunsLog2);
        return block_of(&fb, sizeof fb, g_frame_loc.size);
    };
    if (g_cache.frame == nullptr) g_cache.frame = frame_block();
    if (g_cache.frame == nullptr) return;
    const int mode = (g_debug & 4) != 0 ? 0 : F.mode;
    // the merged draws (mrp_world_merge.inc), made by the frame's first call; the per-draw path when they are off
    // (RAC1_WIIU_WORLD_MERGE=0), not made, or for the debug modes that draw by the template's base vertex (bits 32, 256)
    int kinds = 3;  // (the debug modes' ties / shrubs alone)
    if ((g_debug & 64) != 0) kinds &= 1;
    if ((g_debug & 8) != 0) kinds &= 2;
    const bool merged = g_merge > 0 && (g_debug & (256 | 32)) == 0 && merged_sync(F, kinds, cols_swap());
    if (merged) ++g_merged_calls;

    // the tfrags' data (mrpwb::FrameView::tf_*: the CPU route's output), sent by the frame's first call: its vertices / indices into the
    // geometry ring (one allocation), its frame block (the world renderers' view-projection, as the PC's set_frame_uniforms reads it).
    // (testing, RAC1_WIIU_TFRAG_STAGE=<n>: 1 the CPU route alone - nothing sent -, 2 its data sent and the batches' state set, no draws, 3
    // (the default) everything)
    static const int tstage = std::getenv("RAC1_WIIU_TFRAG_STAGE") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_TFRAG_STAGE")) : 3;
    const std::vector<mrpwb::GpuV>& tverts = *F.tf_verts;
    const std::vector<uint32_t>& tidx = *F.tf_idx;
    const std::vector<mrpwb::TfragRange>& tranges = *F.tf_ranges;
    if (g_tready && !tverts.empty() && F.serial != g_tlast_serial) {  // (the log's: per frame)
        g_tlast_serial = F.serial;
        ++g_tframes;
        g_tverts += tverts.size();
        g_troute_ms += F.tf_ms;
        g_troute_max = std::max(g_troute_max, F.tf_ms);
    }
    auto tfrag_frame = [&] {
        TfragFrameBlock fb{};
        mrp_world_view_proj(fb.vp);
        return block_of(&fb, sizeof fb, g_tframe_loc.size);
    };
    bool tfrag = g_tready && tstage >= 2 && !tverts.empty() && !tidx.empty();
    if (tfrag) {
        if (F.serial != g_tcache.serial || g_gring_wraps != g_tcache.gwraps || g_tcache.vb == nullptr) {
            const uint32_t vbytes = static_cast<uint32_t>(tverts.size() * sizeof(mrpwb::GpuV));
            const uint32_t at_i = (vbytes + GX2_VERTEX_BUFFER_ALIGNMENT - 1) & ~(GX2_VERTEX_BUFFER_ALIGNMENT - 1);
            const uint32_t ibytes = static_cast<uint32_t>(tidx.size() * sizeof(uint32_t));
            g_tcache = TfragCache{};
            g_tcache.serial = F.serial;
            auto* p = static_cast<uint8_t*>(geo_alloc(at_i + ibytes));
            if (p != nullptr) {
                std::memcpy(p, tverts.data(), vbytes);
                std::memcpy(p + at_i, tidx.data(), ibytes);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, p, at_i + ibytes);
                g_tcache.vb = p;
                g_tcache.ib = p + at_i;
                g_tcache.nverts = static_cast<uint32_t>(tverts.size());
                g_tcache.nidx = static_cast<uint32_t>(tidx.size());
            } else if (g_trefused++ < 4) {
                std::printf("[wiiu] mrp_world_gx2: tfrag: %u bytes of vertices / indices: more than the ring's %u\n", at_i + ibytes, kGeoRingBytes);
            }
            g_tcache.gwraps = g_gring_wraps;
        }
        if (g_tcache.frame == nullptr || g_tcache.wraps != g_ring_wraps) {
            g_tcache.frame = tfrag_frame();
            g_tcache.wraps = g_ring_wraps;
        }
        tfrag = g_tcache.vb != nullptr && g_tcache.frame != nullptr;
    }

    // GX2's state for the whole call
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetRasterizerClipControlEx(TRUE, mode == 1 ? FALSE : TRUE, FALSE);  // (the VU projection: the GL backend's depth clamp)
    // (testing, debug bit 256: the template's first vertex given by the attribute buffer's address instead of the draw's offset)
    const bool by_address = (g_debug & 256) != 0;
    const uint32_t vbytes = static_cast<uint32_t>(g_geo.v_count * sizeof(mrpwm::VertR));
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);

    // the shaders of a batch: 0 the ties / shrubs' (mrp_world.gsh: the level's templates, the frame's plain uniforms, the resident colours),
    // 1 the tfrags' (mrp_tfrag.gsh: the frame's tfrag vertices, its frame block); set when the batches' path changes
    int path = -1;
    int cur_bucket = -2;
    uint32_t cur_sampler = ~0u;
    int units_epoch = -3;  // (the merged draws' units bound: the batch epoch's; -3 none)
    PixelBlock last_px{};
    const void* last_px_blk = nullptr;
    uint64_t last_px_wraps = 0;
    auto set_pixel = [&](const PixelBlock& pb) {
        if (last_px_blk == nullptr || last_px_wraps != g_ring_wraps || !(last_px == pb)) {
            last_px_blk = block_of(&pb, sizeof pb, path == 1 ? g_tpixel_loc.size : g_pixel_loc.size);
            last_px_wraps = g_ring_wraps;
            last_px = pb;
        }
        const BlockLoc& pl = path == 1 ? g_tpixel_loc : g_pixel_loc;
        if (last_px_blk != nullptr && pl.found) GX2SetPixelUniformBlock(pl.loc, pl.size, last_px_blk);
    };
    auto bind_path = [&](int want) {
        if (want == path) return;
        path = want;
        cur_bucket = -2;  // (the texture, sampler and pixel block bound again: the other shader's locations, its block's size)
        cur_sampler = ~0u;
        units_epoch = -3;
        last_px_blk = nullptr;
        if (want == 0) {
            GX2SetShaderMode(g_sh.vertexShader->mode);
            GX2SetFetchShader(&g_sh.fetchShader);
            GX2SetVertexShader(g_sh.vertexShader);
            GX2SetPixelShader(g_sh.pixelShader);
            if (!by_address) GX2SetAttribBuffer(0, vbytes, sizeof(mrpwm::VertR), g_geo.vb);
            if (g_frame_loc.found) GX2SetVertexUniformBlock(g_frame_loc.loc, g_frame_loc.size, g_cache.frame);
            if (g_cols_ok) {
                GX2SetVertexTexture(&g_cols.tex, g_cols_loc);
                GX2SetVertexSampler(&g_cols_sampler, g_cols_loc);
            }
            if (g_runs_ok) {  // (the per-draw path never reads it: the colours' texture in its place)
                GX2SetVertexTexture(merged ? &g_mg.runs : &g_cols.tex, g_runs_loc);
                GX2SetVertexSampler(&g_cols_sampler, g_runs_loc);
            }
        } else {
            GX2SetShaderMode(g_tsh.vertexShader->mode);
            GX2SetFetchShader(&g_tfetch[(g_tdebug & 1024) != 0 ? 1 : 0]);
            GX2SetVertexShader(g_tsh.vertexShader);
            GX2SetPixelShader(g_tsh.pixelShader);
            GX2SetAttribBuffer(0, g_tcache.nverts * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), sizeof(mrpwb::GpuV), g_tcache.vb);
            if (g_tframe_loc.found) GX2SetVertexUniformBlock(g_tframe_loc.loc, g_tframe_loc.size, g_tcache.frame);
        }
    };
    // a texture unit of a batch's epoch (0: none) bound: its bucket's array (or the white texture) and sampler, when they change
    auto bind_unit = [&](const mrpwb::Batch& b, uint32_t unit) {
        int bucket = -1;
        uint32_t sampler = 0;
        if (unit != 0 && b.epoch >= 0 && static_cast<size_t>(b.epoch) < epochs.size() && unit - 1 < epochs[static_cast<size_t>(b.epoch)].size()) {
            bucket = epochs[static_cast<size_t>(b.epoch)][unit - 1].first;
            sampler = epochs[static_cast<size_t>(b.epoch)][unit - 1].second & 15u;
        }
        const bool ok = path == 1 ? g_ttex_ok : g_tex_ok;
        const uint32_t loc = path == 1 ? g_ttex_loc : g_tex_loc;
        if (bucket != cur_bucket) {
            const GX2Texture* t = bucket >= 0 && static_cast<size_t>(bucket) < g_tex.arrays.size() && g_tex.arrays[static_cast<size_t>(bucket)].surface.image != nullptr
                                      ? &g_tex.arrays[static_cast<size_t>(bucket)]
                                      : &g_white;
            if (ok) GX2SetPixelTexture(t, loc);
            cur_bucket = bucket;
        }
        if (sampler != cur_sampler) {
            if (ok) GX2SetPixelSampler(&g_samplers[sampler], loc);
            cur_sampler = sampler;
        }
        return bucket;
    };
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
        bind_unit(b, (rec[3] >> 16) & 15u);
        void*& blk = g_cache.draws[i];
        if (blk == nullptr) {
            DrawBlock d;
            d.inst = I;
            std::memcpy(d.cmd, rec, sizeof d.cmd);  // (cmd[5] 0: this record, not the run table's)
            blk = block_of(&d, sizeof d, kSlot);
            ++g_blocks;
        }
        if (blk == nullptr) return;
        if (g_draw_loc.found) GX2SetVertexUniformBlock(g_draw_loc.loc, kSlot, blk);
        if (stage < 4) return;
        if ((g_debug & 2048) != 0 && g_draws_this_call++ > 0) return;  // (testing: one draw a call)
        static bool dumped = false;
        if (!dumped && c.count >= 3) {  // (the first draw's data, on the log: what the GPU reads)
            dumped = true;
            const uint32_t* ix = static_cast<const uint32_t*>(g_geo.ib) + c.first_index;
            const uint32_t v0 = static_cast<uint32_t>(c.base_vertex) + ix[0];
            const uint8_t* v = static_cast<const uint8_t*>(g_geo.vb) + static_cast<size_t>(v0) * sizeof(mrpwm::VertR);
            std::printf("[wiiu] mrp_world_gx2: first draw: %u indices from %u (%u %u %u) + base %d, record %08x %08x %08x %08x %08x, instance rows T %g %g %g %g, "
                        "morph %g %g %g %g, info %u %u %u %u; vertex %u:",
                        c.count, c.first_index, ix[0], ix[1], ix[2], c.base_vertex, rec[0], rec[1], rec[2], rec[3], rec[4], I.rows[12], I.rows[13], I.rows[14], I.rows[15],
                        I.morph[0], I.morph[1], I.morph[2], I.morph[3], I.info[0], I.info[1], I.info[2], I.info[3], v0);
            for (size_t k = 0; k < sizeof(mrpwm::VertR); ++k) std::printf(" %02x", v[k]);
            const uint8_t ci = v[24];
            const size_t at = static_cast<size_t>(I.info[0]) + ci;
            const uint32_t* colw = static_cast<const uint32_t*>(g_cols.tex.surface.image);
            const uint32_t pitch = g_cols.tex.surface.pitch;
            std::printf("; its colour %zu: texel %08x\n", at, at < g_cols.words ? colw[(at >> kColsLog2) * pitch + (at & ((1u << kColsLog2) - 1))] : 0u);
            std::fflush(stdout);
        }
        if (by_address) {
            GX2SetAttribBuffer(0, vbytes - static_cast<uint32_t>(c.base_vertex) * sizeof(mrpwm::VertR), sizeof(mrpwm::VertR),
                               static_cast<uint8_t*>(g_geo.vb) + static_cast<size_t>(c.base_vertex) * sizeof(mrpwm::VertR));
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + c.first_index, 0, 1);
        } else if ((g_debug & 32) != 0) {
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, static_cast<uint32_t>(c.base_vertex), 1);
        } else {
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + c.first_index,
                             static_cast<uint32_t>(c.base_vertex), 1);
        }
        ++g_draws;
        ++g_cmds;
        g_tris += c.count / 3;
    };
    // the merged draws: a batch's texture units all bound (mrpwm::kUnits and up: the pixel shader samples the triangle's unit; unit 0 and
    // the units the epoch lacks the white texture), when the batch's epoch is not the one bound
    auto bind_units = [&](const mrpwb::Batch& b) {
        if (b.epoch == units_epoch) return;
        units_epoch = b.epoch;
        for (uint32_t u = 0; u < 16; ++u) {
            int bucket = -1;
            uint32_t sampler = 0;
            if (u != 0 && b.epoch >= 0 && static_cast<size_t>(b.epoch) < epochs.size() && u - 1 < epochs[static_cast<size_t>(b.epoch)].size()) {
                bucket = epochs[static_cast<size_t>(b.epoch)][u - 1].first;
                sampler = epochs[static_cast<size_t>(b.epoch)][u - 1].second & 15u;
            }
            const GX2Texture* t = bucket >= 0 && static_cast<size_t>(bucket) < g_tex.arrays.size() && g_tex.arrays[static_cast<size_t>(bucket)].surface.image != nullptr
                                      ? &g_tex.arrays[static_cast<size_t>(bucket)]
                                      : &g_white;
            GX2SetPixelTexture(t, g_unit_loc[u]);
            GX2SetPixelSampler(&g_samplers[sampler], g_unit_loc[u]);
        }
        cur_bucket = -2;  // (unit 0's slot is bind_unit's too)
        cur_sampler = ~0u;
    };
    // a batch's merged items (mrpwm::Plan): per item its block (its instances' slots; a lone command's own), its texture (mrpwm::kByUnit:
    // the item's unit bound), one GX2DrawIndexedEx - absolute indices (the level's or the frame's own) and its instances, or a lone
    // command's relative ones from its template's first vertex
    auto draw_items = [&](const mrpwb::Batch& b, size_t bi) {
        const mrpwm::Plan& P = g_mg.plan;
        if (bi + 1 >= P.batch_items.size()) return;
        for (uint32_t k = P.batch_items[bi]; k < P.batch_items[bi + 1]; ++k) {
            const mrpwm::Item& it = P.items[k];
            if (g_merge == mrpwm::kByUnit) bind_unit(b, it.unit);
            if (g_draw_loc.found) GX2SetVertexUniformBlock(g_draw_loc.loc, (it.lone ? 1u : it.ninst) * kSlot, g_mg.blk[k]);
            if (stage < 4) continue;
            if ((g_debug & 2048) != 0 && g_draws_this_call++ > 0) return;  // (testing: one draw a call)
            static bool dumped = false;
            if (!dumped && !it.lone && it.count >= 3) {  // (the first merged draw's data, on the log: what the GPU reads)
                dumped = true;
                const uint32_t* ix = (it.own ? P.idx.data() : g_mlevel.idx.data()) + it.first;
                const mrpwm::VertR& v = g_mlevel.verts[ix[0]];
                const uint32_t* t = &P.table[4 * static_cast<size_t>(v.run)];
                std::printf("[wiiu] mrp_world_gx2: first merged draw: %u indices from %u%s (%u %u %u), %u instances from %u, %u commands, unit %u; vertex %u: run %u, "
                            "its record %08x %08x %08x %08x\n",
                            it.count, it.first, it.own ? " (the frame's)" : "", ix[0], ix[1], ix[2], it.ninst, P.ilist[it.inst], it.cmds, it.unit, ix[0], v.run, t[0], t[1], t[2],
                            t[3]);
                std::fflush(stdout);
            }
            if (it.lone) {
                GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, it.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + it.first, static_cast<uint32_t>(it.base), 1);
                g_tris += it.count / 3;
            } else {
                const auto* ib = static_cast<const uint32_t*>(it.own ? g_mg.own : g_geo.ib_abs);
                GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, it.count, GX2_INDEX_TYPE_U32, ib + it.first, 0, it.ninst);
                g_tris += static_cast<uint64_t>(it.count / 3) * it.ninst;
            }
            ++g_draws;
            g_cmds += it.cmds;
        }
    };
    // a tfrag batch's triangles: one indexed draw per run of one texture unit (mrpwb::TfragRange, from `r0`: the batch's first), its
    // texture bound first; the frame's tfrag vertices absolute (base vertex 0). (testing, tfrag debug bit 4: the batch in one draw, its
    // first range's texture)
    uint32_t tdraws_this_call = 0;
    auto draw_tfrag = [&](const mrpwb::Batch& b, size_t bi, size_t r0) {
        const bool whole = (g_tdebug & 4) != 0;
        for (size_t r = r0; r < tranges.size() && tranges[r].batch == bi; ++r) {
            const mrpwb::TfragRange& R = tranges[r];
            if (g_ring_wraps != g_tcache.wraps) {  // (the blocks' ring wrapped during this call: the frame block written again)
                g_tcache.frame = tfrag_frame();
                g_tcache.wraps = g_ring_wraps;
                if (g_tcache.frame != nullptr && g_tframe_loc.found) GX2SetVertexUniformBlock(g_tframe_loc.loc, g_tframe_loc.size, g_tcache.frame);
                last_px_blk = nullptr;
                set_pixel(last_px);
            }
            const int bucket = bind_unit(b, R.unit);
            if (tstage < 3) return;
            if ((g_tdebug & 2048) != 0 && tdraws_this_call++ > 0) return;  // (testing: one draw a call)
            const uint32_t first = whole ? b.first : R.first, count = whole ? b.count : R.count;
            static bool dumped = false;
            if (!dumped && count >= 3) {  // (the first tfrag draw's data, on the log: what the GPU reads)
                dumped = true;
                const uint32_t* ix = static_cast<const uint32_t*>(g_tcache.ib) + first;
                const auto* v = static_cast<const uint8_t*>(g_tcache.vb) + static_cast<size_t>(ix[0]) * sizeof(mrpwb::GpuV);
                const mrpwb::GpuV& g = tverts[ix[0]];
                std::printf("[wiiu] mrp_world_gx2: first tfrag draw: batch %zu, %u indices from %u (%u %u %u), unit %u (bucket %d sampler %u); vertex %u: wp %g %g %g %g "
                            "stq %g %g %g lodq %g rgba %02x%02x%02x%02x fog %g word %08x layer %u state %08x %08x; its bytes:",
                            bi, count, first, ix[0], ix[1], ix[2], R.unit, bucket, cur_sampler, ix[0], g.wp[0], g.wp[1], g.wp[2], g.wp[3], g.stq[0], g.stq[1], g.stq[2], g.lodq,
                            g.rgba[0], g.rgba[1], g.rgba[2], g.rgba[3], g.fog, g.word, g.layer, g.state[0], g.state[1]);
                for (size_t k = 0; k < sizeof(mrpwb::GpuV); ++k) std::printf(" %02x", v[k]);
                std::printf("\n");
                std::fflush(stdout);
            }
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_tcache.ib) + first, 0, 1);
            ++g_tdraws;
            g_ttris += count / 3;
            if (whole) return;
        }
    };

    g_draws_this_call = 0;
    static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;  // (testing: the failing pixels not drawn, as the GL backend's)
    size_t tr = 0;  // (the tfrag ranges' cursor: made in the batches' order)
    for (size_t bi = 0; bi < batches.size(); ++bi) {
        const mrpwb::Batch& b = batches[bi];
        if (b.sky || b.rk.shadow || (!b.gpu && !b.tfrag)) {  // (the CPU path's, the sky's, the casters': not drawn here yet)
            ++g_skipped;
            continue;
        }
        if (b.tfrag && !tfrag) continue;
        if (!b.tfrag && (g_tdebug & 8) != 0) continue;  // (testing, tfrag debug bit 8: the tfrags alone)
        size_t r0 = 0;
        if (b.tfrag) {  // the batch's triangles and ranges checked against the frame's tfrag indices
            while (tr < tranges.size() && tranges[tr].batch < bi) ++tr;
            r0 = tr;
            bool ok = static_cast<uint64_t>(b.first) + b.count <= g_tcache.nidx;
            for (size_t r = r0; ok && r < tranges.size() && tranges[r].batch == bi; ++r) ok = static_cast<uint64_t>(tranges[r].first) + tranges[r].count <= g_tcache.nidx;
            if (!ok) {
                if (g_refused++ < 8)
                    std::printf("[wiiu] mrp_world_gx2: tfrag batch refused: first %u count %u (%u indices)\n", b.first, b.count, g_tcache.nidx);
                continue;
            }
        } else {  // the batch's draw commands checked against what they read (the GL backend's batch_refused): one outside is refused
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
        bind_path(b.tfrag ? 1 : 0);
        if (b.tfrag) ++g_tbatches;
        else ++g_batches;
        const int dbg = b.tfrag ? g_tdebug : g_debug;  // (the tfrags' debug bits: RAC1_WIIU_TFRAG_DEBUG)
        const bool grey = (dbg & 2) != 0;
        const int shade = grey ? 3 : (dbg & 4096) != 0 ? 1 : (dbg & 8192) != 0 ? 2 : g_render_shade;
        const mrpwb::Key& k = b.key;
        const mrpwb::RKey& rk = b.rk;
        if (grey) GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        else gs::set_blend_gx2(k.prim, k.alpha);
        // the depth: the GS's GEQUAL / GREATER on the host's depth as LEQUAL / LESS; no test: no writes (as GL)
        const uint32_t ztst = fld(k.test, 17, 2);
        const bool ztest = db != nullptr && fld(k.test, 16, 1) != 0 && ztst != 1 && (dbg & 1) == 0;
        const bool zwrite = rk.zwrite != 0;
        const GX2CompareFunction zf = ztst == 0 ? GX2_COMPARE_FUNC_NEVER : ztst == 2 ? GX2_COMPARE_FUNC_LEQUAL : GX2_COMPARE_FUNC_LESS;
        PixelBlock pb{};
        pb.fog_col[0] = static_cast<float>(rk.fogcol & 0xff) / 255.0f;
        pb.fog_col[1] = static_cast<float>((rk.fogcol >> 8) & 0xff) / 255.0f;
        pb.fog_col[2] = static_cast<float>((rk.fogcol >> 16) & 0xff) / 255.0f;
        pb.a[1] = shade;
        pb.a[2] = (rk.fogcol >> 24) & 1 ? 1 : 0;
        pb.a[3] = grey ? 1 : 0;
        const bool items = merged && !b.tfrag;
        pb.b[0] = items && g_merge >= mrpwm::kUnits ? 1 : 0;
        if (items && g_merge >= mrpwm::kUnits) bind_units(b);
        // pass 1: the passing pixels of the draws whose alpha test is on (their mode in draw_flags), every pixel of the others
        GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && zwrite ? TRUE : FALSE, zf);
        pb.a[0] = 1;
        set_pixel(pb);
        if (b.tfrag) draw_tfrag(b, bi, r0);
        else if (items) draw_items(b, bi);
        else
            for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(b, i);
        // pass 2: the failing ones as AFAIL says (RKey::pass2: 1 FB_ONLY the colour with alpha, 2 ZB_ONLY the depth, 3 RGB_ONLY the
        // colour without alpha)
        if (rk.pass2 != 0 && !one_pass && !grey) {
            const uint32_t afail = rk.pass2;
            GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && afail == 2 && zwrite ? TRUE : FALSE, zf);
            set_mask(afail == 2 ? none : afail == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA);
            pb.a[0] = 2;
            set_pixel(pb);
            if (b.tfrag) draw_tfrag(b, bi, r0);
            else if (items) draw_items(b, bi);
            else
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
        std::printf("[wiiu] mrp_world_gx2: last 120 calls: %.1f draw commands, %.1f GX2 draws (merged: mode %d, %llu calls), %.1f triangles, %.1f uniform blocks "
                    "written, %.1f batches, %.2f ms CPU a call (the merge's plan and writing %.2f); %llu batches refused, %llu skipped (not the GPU path's); %zu "
                    "instances; the last plan: %zu items, %zu instanced (%zu instances), %zu lone (%zu conflicts), %zu indices copied\n",
                    static_cast<double>(g_cmds) / 120.0, static_cast<double>(g_draws) / 120.0, g_merge, static_cast<unsigned long long>(g_merged_calls),
                    static_cast<double>(g_tris) / 120.0, static_cast<double>(g_blocks) / 120.0, static_cast<double>(g_batches) / 120.0, g_cpu_ms / 120.0,
                    g_plan_ms / 120.0, static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_skipped), insts.size(), g_mg.plan.items.size(),
                    g_mg.plan.instanced, g_mg.plan.instanced_insts, g_mg.plan.conflicts + g_mg.plan.unmatched, g_mg.plan.conflicts, g_mg.plan.copied);
        if (g_tready)
            std::printf("[wiiu] mrp_world_gx2: tfrag, last 120 calls: %.1f draws, %.1f triangles, %.1f batches; %llu frames: %.0f vertices, the CPU route %.2f ms "
                        "(the most %.2f ms); geometry ring wraps %llu\n",
                        static_cast<double>(g_tdraws) / 120.0, static_cast<double>(g_ttris) / 120.0, static_cast<double>(g_tbatches) / 120.0,
                        static_cast<unsigned long long>(g_tframes), g_tframes != 0 ? static_cast<double>(g_tverts) / static_cast<double>(g_tframes) : 0.0,
                        g_tframes != 0 ? g_troute_ms / static_cast<double>(g_tframes) : 0.0, g_troute_max, static_cast<unsigned long long>(g_gring_wraps));
        std::fflush(stdout);
        g_draws = g_batches = g_tris = g_cmds = g_blocks = g_merged_calls = 0;
        g_plan_ms = 0.0;
        g_tdraws = g_ttris = g_tbatches = g_tframes = g_tverts = 0;
        g_troute_ms = g_troute_max = 0.0;
        g_cpu_ms = 0.0;
    }
}

}  // namespace rac1
