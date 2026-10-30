// wiiu: release previous level GX2 buffers before alloc
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
#include <game/render/mrp_fx.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_shadows.h>
#include <game/render/mrp_world_backend.inc>
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_world_sky.inc>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_emu.h>

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <gx2/draw.h>
#include <gx2/enum.h>
#include <gx2/event.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/state.h>
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
#include <unordered_map>
#include <vector>

#include "gx2_counters.h"  // (after every GX2 header: the frame's GX2 work counted)

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
// one instanced draw, 4 (2026-10-28) as 3 split by texture unit (one sampler: the pixel shader's unit tree not run)
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
// the sky's shaders (shaders/mrp_sky.gsh = mrp_sky.vert + mrp_world.frag: MRP_LITE.md phase 5), their fetch shaders ([0] swapped, [1] as
// they are: sky debug bit 1024), blocks and sampler
bool g_sready = false;
WHBGfxShaderGroup g_ssh{};
GX2FetchShader g_sfetch[2]{};
void* g_sfetch_prog[2] = {};
BlockLoc g_sframe_loc, g_spixel_loc;
uint32_t g_stex_loc = 0;
bool g_stex_ok = false;
// ((2026-10-28) Gx2Perf::nokill) its pixel shader's build without the alpha test (shaders/mrp_sky_noat.gsh: mrp_sky.vert + mrp_world.frag
// RAC1_NO_ALPHA_TEST), for the sky batches none of whose draws tests (Batch::ate): ~14 of the shader's ~66 ALU groups fewer a pixel
WHBGfxShaderGroup g_sshv_noat{};
const GX2PixelShader* g_sps_noat = nullptr;
// ((2026-10-28) the host sky, mrp_sky_host.inc: its shells drawn from static corners) the pairs whose vertex shader is mrp_sky.vert built
// RAC1_SKY_HOST (the corners' STQ multiplied by the clip w there, not on the CPU): [0] with mrp_world.frag, [1] without the alpha test
// (RAC1_NO_ALPHA_TEST), [2] / [3] the fast pixel shader's (RAC1_FAST) - each a pair compiled together, bound together (gx2_pair_variant);
// g_hok false: not there. Without [0] the host sky does not stand in for the pass (mrp_world_wiiu_sky_host_wanted)
WHBGfxShaderGroup g_hsh[4]{};
bool g_hok[4] = {};
// (2026-10-28, the GPU's time) the pixel shaders' other builds (Gx2Perf: nokill, lean), [path: 0 the ties' / shrubs', 1 the tfrags'][bits: 1
// no alpha test (RAC1_NO_ALPHA_TEST), 2 lean (RAC1_LEAN)] - [k][0] the shaders' own; nullptr a build not there (or refused: gx2_ps_variant)
WHBGfxShaderGroup g_shv[2][4]{};
const GX2PixelShader* g_ps[2][4] = {};
void variants_load(int path, const WHBGfxShaderGroup& main) {
    static const char* const kFile[2][4] = {{nullptr, "shaders/mrp_world_noat.gsh", "shaders/mrp_world_lean.gsh", "shaders/mrp_world_lean_noat.gsh"},
                                            {nullptr, "shaders/mrp_tfrag_noat.gsh", "shaders/mrp_tfrag_lean.gsh", "shaders/mrp_tfrag_lean_noat.gsh"}};
    g_ps[path][0] = main.pixelShader;
    for (int k = 1; k < 4; ++k) g_ps[path][k] = gx2_ps_variant(kFile[path][k], main, g_shv[path][k]);
}
void variants_free(int path) {
    for (int k = 1; k < 4; ++k) {
        if (g_ps[path][k] != nullptr) WHBGfxFreeShaderGroup(&g_shv[path][k]);
        g_shv[path][k] = WHBGfxShaderGroup{};
    }
    for (int k = 0; k < 4; ++k) g_ps[path][k] = nullptr;
}
// the pixel shader for `path` with the alpha test (`at`) or not, as the switches say: the nearest build there (lean before no-kill)
const GX2PixelShader* variant_ps(int path, bool at) {
    const int k = (g_gx2_perf.lean ? 2 : 0) | (!at && g_gx2_perf.nokill ? 1 : 0);
    for (const int t : {k, k & 2, k & 1, 0})
        if (g_ps[path][t] != nullptr) return g_ps[path][t];
    return nullptr;
}
// ((2026-10-28, the world's pixels) Gx2Perf::fast) the fast pixel shader's pairs (mrp_world_fast.frag with the path's vertex shader built
// RAC1_FAST: ports/wiiu/game/CMakeLists.txt), [path: 0 the ties' / shrubs', 1 the tfrags', 2 the sky's][0 with the alpha test, 1 without
// (RAC1_NO_ALPHA_TEST)]; g_fok false: not there (or refused: gx2_pair_variant). Each pair's vertex shader is bound with its pixel shader
WHBGfxShaderGroup g_fsh[3][2]{};
bool g_fok[3][2] = {};
void fast_load(int path, const WHBGfxShaderGroup& main) {
    static const char* const kFile[3][2] = {{"shaders/mrp_world_fast.gsh", "shaders/mrp_world_fast_noat.gsh"},
                                            {"shaders/mrp_tfrag_fast.gsh", "shaders/mrp_tfrag_fast_noat.gsh"},
                                            {"shaders/mrp_sky_fast.gsh", "shaders/mrp_sky_fast_noat.gsh"}};
    for (int k = 0; k < 2; ++k) {
        g_fok[path][k] = gx2_pair_variant(kFile[path][k], main, g_fsh[path][k]);
        if (g_fok[path][k]) gx2_ps_early_z(g_fsh[path][k].pixelShader, kFile[path][k]);
    }
}
void fast_free(int path) {
    for (int k = 0; k < 2; ++k) {
        if (g_fok[path][k]) WHBGfxFreeShaderGroup(&g_fsh[path][k]);
        g_fsh[path][k] = WHBGfxShaderGroup{};
        g_fok[path][k] = false;
    }
}
// the fast pair for `path` with the alpha test (`at`) or not (Gx2Perf::nokill: the build without discard), when Gx2Perf::fast says so;
// nullptr: the path's own shaders (mrp_world.frag's builds, variant_ps)
const WHBGfxShaderGroup* fast_pair(int path, bool at) {
    if (!g_gx2_perf.fast) return nullptr;
    if (!at && g_gx2_perf.nokill && g_fok[path][1]) return &g_fsh[path][1];
    return g_fok[path][0] ? &g_fsh[path][0] : nullptr;
}

// The uniform blocks, appended to a ring in MEM2 (mrp_moby_gx2.cpp's): before the ring wraps over what the GPU may still read the
// CPU waits for it (GX2DrawDone)
constexpr uint32_t kRingBytes = 16u << 20;
uint8_t* g_ring = nullptr;
uint32_t g_ring_off = 0;
uint64_t g_ring_wraps = 0;
bool g_ring_stale = false;  // (Gx2Perf::ub_bulk: the ring wrapped since its GPU caches were last invalidated)
void* ring_alloc(uint32_t bytes) {
    uint32_t off = (g_ring_off + GX2_UNIFORM_BLOCK_ALIGNMENT - 1) & ~(GX2_UNIFORM_BLOCK_ALIGNMENT - 1);
    if (bytes > kRingBytes) return nullptr;
    if (off + bytes > kRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_ring_wraps;
        g_ring_stale = true;
    }
    g_ring_off = off + bytes;
    return g_ring + off;
}
// ((2026-10-28) Gx2Perf::ub_bulk) the whole ring's GPU caches invalidated: at a call's start (before its first draw) and after a wrap. A
// block is read by the draws after its writing only, at an address no draw read since the ring last came round: the caches emptied once
// a call hold no stale line of it (the blocks start 256 bytes apart: no line shared)
void ring_gpu_invalidate() {
    if (!g_gx2_perf.ub_bulk || g_ring == nullptr) return;
    GX2Invalidate(GX2_INVALIDATE_MODE_UNIFORM_BLOCK, g_ring, kRingBytes);
    g_ring_stale = false;
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
    if (g_gx2_perf.ub_bulk) {  // (out of the CPU's cache alone; the GPU's emptied by the call: ring_gpu_invalidate)
        DCFlushRange(p, size);
        if (g_ring_stale) ring_gpu_invalidate();
    } else {
        GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_UNIFORM_BLOCK), p, size);
    }
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

// The sky's vertices / indices (the CPU path's: mrpwb::FrameView::cpu_verts / cpu_idx), a frame's appended to a ring in MEM2 of its own, as
// the tfrags'; a frame's on Veldin ~900 vertices (64 KB)
constexpr uint32_t kSkyRingBytes = 2u << 20;
uint8_t* g_sring = nullptr;
uint32_t g_sring_off = 0;
uint64_t g_sring_wraps = 0;
void* sky_alloc(uint32_t bytes) {
    uint32_t off = (g_sring_off + GX2_VERTEX_BUFFER_ALIGNMENT - 1) & ~(GX2_VERTEX_BUFFER_ALIGNMENT - 1);
    if (bytes > kSkyRingBytes) return nullptr;
    if (off + bytes > kSkyRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_sring_wraps;
    }
    g_sring_off = off + bytes;
    return g_sring + off;
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
int g_sdebug = 0;  // (RAC1_WIIU_SKY_DEBUG's)

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

// orig: (host) see mrp_world_gx2.h
Gx2Perf g_gx2_perf;
Gx2DrawView g_gx2_view;

// orig: (host) see mrp_world_gx2.h
void gx2_draw_view_take() {
    Gx2DrawView& v = g_gx2_view;
    mrp_world_view_proj(v.vp);
    v.depth_ok = render_gs_depth(v.da, v.db);
    v.fb_w = static_cast<float>(lhu(0x001518d0));
    v.fb_h = static_cast<float>(lhu(0x001518d2));
    if (v.fb_w <= 0.0f) v.fb_w = 512.0f;
    if (v.fb_h <= 0.0f) v.fb_h = 416.0f;
    v.moby_mip = g_mrp_moby_mip;
}

// orig: (host) see mrp_world_gx2.h
void gx2_perf_init() {
    static bool done = false;
    if (done) return;
    done = true;
    auto env = [](const char* name, bool def) {
        const char* v = std::getenv(name);
        return v != nullptr && v[0] != 0 ? std::atoi(v) != 0 : def;
    };
    // (hierarchical Z on by default: Veldin 101 -> 70 ms a host frame on the console, the GPU no longer waited for, 2026-10-28)
    g_gx2_perf.hiz = env("RAC1_WIIU_HIZ", true);
    // (the builds without discard on by default with the early depth test, 2026-10-28: their batches' depth then tested and written before
    // the shader - the same pixels: no draw of such a batch discards)
    g_gx2_perf.nokill = env("RAC1_WIIU_NOKILL", true);
    g_gx2_perf.lean = env("RAC1_WIIU_LEAN", false);
    g_gx2_perf.pixels_off = env("RAC1_WIIU_PIXELS_OFF", false);
    g_gx2_perf.zclear2 = env("RAC1_WIIU_ZCLEAR", true);
    g_gx2_perf.earlyz = env("RAC1_WIIU_EARLYZ", true);
    g_gx2_perf.ub_bulk = env("RAC1_WIIU_UB_BULK", true);
    if (const char* v = std::getenv("RAC1_WIIU_FLUSH"); v != nullptr && v[0] != 0) g_gx2_perf.flush = std::clamp(std::atoi(v), 0, 2);
    if (const char* v = std::getenv("RAC1_WIIU_FLUSH_DRAWS"); v != nullptr && v[0] != 0) g_gx2_perf.flush_draws = static_cast<uint32_t>(std::max(1, std::atoi(v)));
    g_gx2_perf.fast = env("RAC1_WIIU_FASTPS", true);
    g_gx2_perf.p2_less = env("RAC1_WIIU_P2_LESS", true);
    g_gx2_perf.p2_cull = env("RAC1_WIIU_P2_CULL", true);
    std::printf("[wiiu] gx2 perf: hierarchical Z %d, no-kill shaders %d, lean world shader %d, pixels off %d, second depth clear %d, early depth test %d, "
                "flushes %d (every %u world draws), uniform blocks' caches once a call %d, fast world pixel shaders %d, pass2 less %d, pass2 cull %d\n",
                g_gx2_perf.hiz, g_gx2_perf.nokill, g_gx2_perf.lean, g_gx2_perf.pixels_off, g_gx2_perf.zclear2, g_gx2_perf.earlyz, g_gx2_perf.flush,
                g_gx2_perf.flush_draws, g_gx2_perf.ub_bulk, g_gx2_perf.fast, g_gx2_perf.p2_less, g_gx2_perf.p2_cull);
}

// orig: (host) see mrp_world_gx2.h
void gx2_flush(int level) {
    if (g_gx2_perf.flush >= level) GX2Flush();
}

// orig: (host) see mrp_world_gx2.h. DB_SHADER_CONTROL's Z_ORDER (bits 4..5): 0 EARLY_Z_THEN_LATE_Z, 1 LATE_Z (CafeGLSL's for every
// shader, seen in the .gsh files' registers 2026-10-28: 0x50 with discard, 0x10 without), 2 / 3 the re-Z orders (left alone: Mesa's r600
// driver keeps clear of RE_Z on R6xx / R7xx - lockups - and uses EARLY_Z_THEN_LATE_Z with a discarding shader too, LATE_Z only for the
// fixed-function alpha test, which is off here: GX2SetAlphaTest FALSE). GX2SetPixelShader writes the register from the struct
void gx2_ps_early_z(const GX2PixelShader* ps, const char* who) {
    if (ps == nullptr || !g_gx2_perf.earlyz) return;
    auto* p = const_cast<GX2PixelShader*>(ps);
    const uint32_t was = p->regs.db_shader_control;
    if (((was >> 4) & 3u) != 1u) return;
    p->regs.db_shader_control = was & ~0x30u;
    std::printf("[wiiu] gx2 perf: %s: db_shader_control %08x -> %08x (early depth test)\n", who, was, p->regs.db_shader_control);
}

// orig: (host) see mrp_world_gx2.h
const GX2PixelShader* gx2_ps_variant(const char* path, const WHBGfxShaderGroup& main, WHBGfxShaderGroup& keep) {
    keep = WHBGfxShaderGroup{};
    if (main.vertexShader == nullptr || main.pixelShader == nullptr) return nullptr;
    std::vector<uint8_t> gsh;
    if (std::FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&keep, 0, gsh.data())) {
        std::printf("[wiiu] gx2 perf: %s not loaded\n", path);
        keep = WHBGfxShaderGroup{};
        return nullptr;
    }
    const GX2VertexShader *a = main.vertexShader, *b = keep.vertexShader;
    const GX2PixelShader *p = main.pixelShader, *q = keep.pixelShader;
    const char* why = nullptr;
    if (b == nullptr || q == nullptr) {
        why = "no vertex or pixel shader in it";
    } else if (a->size != b->size || std::memcmp(a->program, b->program, a->size) != 0 || std::memcmp(&a->regs, &b->regs, sizeof a->regs) != 0) {
        why = "another vertex shader (its outputs may differ)";
    } else if (p->mode != q->mode) {
        why = "another uniform mode";
    } else {
        for (uint32_t i = 0; why == nullptr && i < q->uniformBlockCount; ++i) {
            bool same = false;
            for (uint32_t j = 0; j < p->uniformBlockCount; ++j)
                if (std::strcmp(q->uniformBlocks[i].name, p->uniformBlocks[j].name) == 0) same = q->uniformBlocks[i].offset == p->uniformBlocks[j].offset;
            if (!same) why = "a uniform block not where the shaders' own is";
        }
        for (uint32_t i = 0; why == nullptr && i < q->samplerVarCount; ++i) {
            bool same = false;
            for (uint32_t j = 0; j < p->samplerVarCount; ++j)
                if (std::strcmp(q->samplerVars[i].name, p->samplerVars[j].name) == 0) same = q->samplerVars[i].location == p->samplerVars[j].location;
            if (!same) why = "a sampler not where the shaders' own is";
        }
    }
    if (why != nullptr) {
        std::printf("[wiiu] gx2 perf: %s refused: %s\n", path, why);
        WHBGfxFreeShaderGroup(&keep);
        keep = WHBGfxShaderGroup{};
        return nullptr;
    }
    std::printf("[wiiu] gx2 perf: %s: its pixel shader (%u bytes, db_shader_control %08x) can stand in\n", path, q->size, q->regs.db_shader_control);
    return q;
}

// orig: (host) see mrp_world_gx2.h. The pair's own vertex shader feeds its own pixel shader (compiled together: the outputs' semantics
// match - 2026-10-28, a vertex output only some pixel shaders read drew the world untextured when another build's pixel shader ran with
// the main vertex shader); what must match `main`'s is what the caller sets once for both: the fetch shader (the attributes' locations),
// the blocks' and samplers' slots, the uniform mode
bool gx2_pair_variant(const char* path, const WHBGfxShaderGroup& main, WHBGfxShaderGroup& keep) {
    keep = WHBGfxShaderGroup{};
    if (main.vertexShader == nullptr || main.pixelShader == nullptr) return false;
    std::vector<uint8_t> gsh;
    if (std::FILE* f = std::fopen(path, "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&keep, 0, gsh.data())) {
        std::printf("[wiiu] gx2 perf: %s not loaded\n", path);
        keep = WHBGfxShaderGroup{};
        return false;
    }
    const GX2VertexShader *a = main.vertexShader, *b = keep.vertexShader;
    const GX2PixelShader *p = main.pixelShader, *q = keep.pixelShader;
    const char* why = nullptr;
    // (each of the pair's blocks / samplers where `main`'s of its name is: the caller binds them by `main`'s locations and sizes)
    auto blocks = [&](const GX2UniformBlock* v, uint32_t nv, const GX2UniformBlock* m, uint32_t nm) {
        for (uint32_t i = 0; why == nullptr && i < nv; ++i) {
            bool same = false;
            for (uint32_t j = 0; j < nm; ++j)
                if (std::strcmp(v[i].name, m[j].name) == 0) same = v[i].offset == m[j].offset && v[i].size <= m[j].size;
            if (!same) why = "a uniform block not where (or larger than) the main shaders' own";
        }
    };
    auto samplers = [&](const GX2SamplerVar* v, uint32_t nv, const GX2SamplerVar* m, uint32_t nm) {
        for (uint32_t i = 0; why == nullptr && i < nv; ++i) {
            bool same = false;
            for (uint32_t j = 0; j < nm; ++j)
                if (std::strcmp(v[i].name, m[j].name) == 0) same = v[i].location == m[j].location;
            if (!same) why = "a sampler not where the main shaders' own is";
        }
    };
    if (b == nullptr || q == nullptr) {
        why = "no vertex or pixel shader in it";
    } else if (b->mode != a->mode || q->mode != p->mode) {
        why = "another uniform mode";
    } else {
        for (uint32_t i = 0; why == nullptr && i < b->attribVarCount; ++i) {
            bool same = false;
            for (uint32_t j = 0; j < a->attribVarCount; ++j)
                if (std::strcmp(b->attribVars[i].name, a->attribVars[j].name) == 0) same = b->attribVars[i].location == a->attribVars[j].location;
            if (!same) why = "an attribute not at the main vertex shader's location (its fetch shader)";
        }
        blocks(b->uniformBlocks, b->uniformBlockCount, a->uniformBlocks, a->uniformBlockCount);
        blocks(q->uniformBlocks, q->uniformBlockCount, p->uniformBlocks, p->uniformBlockCount);
        samplers(b->samplerVars, b->samplerVarCount, a->samplerVars, a->samplerVarCount);
        samplers(q->samplerVars, q->samplerVarCount, p->samplerVars, p->samplerVarCount);
    }
    if (why != nullptr) {
        std::printf("[wiiu] gx2 perf: %s refused: %s\n", path, why);
        WHBGfxFreeShaderGroup(&keep);
        keep = WHBGfxShaderGroup{};
        return false;
    }
    std::printf("[wiiu] gx2 perf: %s: its vertex (%u bytes, %u attributes) and pixel shader (%u bytes, db_shader_control %08x) can take the main ones' place\n", path,
                b->size, b->attribVarCount, q->size, q->regs.db_shader_control);
    return true;
}

int& mrp_world_gx2_merge_mode() { return g_merge; }

// orig: (host) see mrp_world_backend.inc
bool mrp_world_gx2_tfrag_ready() { return g_ready && g_tready; }

// orig: (host) see mrp_world_gx2.h
bool g_wiiu_sky = false;

// orig: (host) see mrp_world_sky.inc
bool mrp_world_wiiu_sky_pass_wanted() { return !g_ready || g_wiiu_sky; }

// orig: (host) see mrp_world_sky.inc: the world up; with its sky drawn, the host sky's shaders there (else the pass and its capture)
bool mrp_world_wiiu_sky_host_wanted() { return g_ready && (!(g_sready && g_wiiu_sky) || g_hok[0]); }

// orig: (host) see mrp_world_backend.inc
bool mrp_world_gx2_sky_ready() { return g_ready && g_sready && g_wiiu_sky; }

// orig: (host) see mrp_world_gx2.h
bool mrp_world_gx2_sky_loaded() { return g_ready && g_sready; }

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
    static const char* slist = std::getenv("RAC1_WIIU_SKY_DEBUG");
    static const uint64_t sat = std::getenv("RAC1_WIIU_SKY_DEBUG_AT") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_SKY_DEBUG_AT"), nullptr, 10) : 0;
    const int smode = debug_mode(slist, sat, game_frame);
    if (smode != g_sdebug) {
        std::printf("[wiiu] mrp_world_gx2: sky debug mode %d from game frame %llu\n", smode, static_cast<unsigned long long>(game_frame));
        std::fflush(stdout);
    }
    g_sdebug = smode;
}

namespace {
// orig: (host) the fetch shaders of a vertex shader reading mrpwb::GpuV (72 bytes, written by the PPC: the tfrags' and the sky's vertices):
// the floats and the 32-bit integers big-endian, swapped within each 32 bits (8_IN_32; `out[1]`: not swapped), the RGBA bytes not swapped
// (byte 0 is R). Fewer components than four: the rest 0, w 1 (as WHB's). false: not made (`prog` freed)
bool gpuv_fetch(const GX2VertexShader* vs, GX2FetchShader out[2], void* prog[2], const char* who) {
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
    std::printf("[wiiu] mrp_world_gx2: %s: %u attributes:", who, n);
    for (uint32_t i = 0; i < n; ++i) std::printf(" loc %u off %u fmt %#x", streams[0][i].location, streams[0][i].offset, streams[0][i].format);
    std::printf("\n");
    const uint32_t size = GX2CalcFetchShaderSizeEx(n, GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
    for (int k = 0; k < 2; ++k) {
        prog[k] = mem_alloc(size, GX2_SHADER_PROGRAM_ALIGNMENT);
        if (prog[k] == nullptr) {
            std::printf("[wiiu] mrp_world_gx2: %s: fetch shader not made\n", who);
            for (int j = 0; j < 2; ++j) mem_free(prog[j]);
            return false;
        }
        GX2InitFetchShaderEx(&out[k], static_cast<uint8_t*>(prog[k]), n, streams[k], GX2_FETCH_SHADER_TESSELLATION_NONE, GX2_TESSELLATION_MODE_DISCRETE);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_SHADER, prog[k], size);
    }
    return true;
}

// orig: (host) the tfrags' shaders, fetch shaders and geometry ring freed (the GPU done with them: the caller waited)
void tfrag_free() {
    for (int k = 0; k < 2; ++k) {
        if (g_tfetch_prog[k] != nullptr) MEMFreeToDefaultHeap(g_tfetch_prog[k]);
        g_tfetch_prog[k] = nullptr;
        g_tfetch[k] = GX2FetchShader{};
    }
    if (g_gring != nullptr) MEMFreeToDefaultHeap(g_gring);
    g_gring = nullptr;
    variants_free(1);
    fast_free(1);
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
    const GX2VertexShader* vs = g_tsh.vertexShader;
    if (!gpuv_fetch(vs, g_tfetch, g_tfetch_prog, "tfrag")) {
        tfrag_free();
        return;
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
    if (!variant) variants_load(1, g_tsh);  // (its pixel shader's other builds, Gx2Perf; not for another build of the shaders)
    if (!variant) fast_load(1, g_tsh);      // (the fast pixel shader's pairs, Gx2Perf::fast)
    gx2_ps_early_z(g_tsh.pixelShader, "mrp_tfrag");
    for (int k = 1; k < 4; ++k) gx2_ps_early_z(g_ps[1][k], "mrp_tfrag (another build)");
    std::printf("[wiiu] mrp_world_gx2: tfrag up (%s; blocks: frame %u pixel %u bytes; geometry ring %u KB)\n", variant ? gsh_env : "shaders/mrp_tfrag.gsh", g_tframe_loc.size,
                g_tpixel_loc.size, kGeoRingBytes / 1024);
}

// the sky's textures: per bucket a frame's build made (mrpwb::built_bucket - the sky asset's, decoded at their first use), a 1 x 1 layer
// array of one level, linear (written by the CPU in place: one texture a few times a level, no GPU copy), made at the bucket's first draw;
// all dropped when the level changes
struct SkyTextures {
    uint64_t loads = ~0ull;
    std::vector<GX2Texture> tex;
    std::vector<uint8_t> tried;
    size_t made = 0, bytes = 0;
} g_stex;
void sky_tex_free() {
    for (GX2Texture& t : g_stex.tex) tex_free_one(t);
    g_stex = SkyTextures{};
}
// the texture of a sky batch's bucket (the white one: none, or not made)
const GX2Texture* sky_texture(const mrpwb::LevelView& L, int bucket) {
    if (g_stex.loads != L.loads) {
        GX2DrawDone();  // (the old level's may still be sampled)
        sky_tex_free();
        g_stex.loads = L.loads;
    }
    if (bucket < 0) return &g_white;
    const size_t b = static_cast<size_t>(bucket);
    if (b >= g_stex.tex.size()) {
        g_stex.tex.resize(b + 1);
        g_stex.tried.resize(b + 1, 0);
    }
    GX2Texture& t = g_stex.tex[b];
    if (t.surface.image != nullptr) return &t;
    if (g_stex.tried[b] != 0) return &g_white;
    g_stex.tried[b] = 1;
    uint32_t w = 0, h = 0;
    const std::vector<uint32_t>* px = nullptr;
    if (!mrpwb::built_bucket(b, w, h, px) || px == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: sky: bucket %d has no texels: drawn white\n", bucket);
        return &g_white;
    }
    if (!tex_make(t, w, h, 1, 1, GX2_TILE_MODE_LINEAR_ALIGNED)) {
        tex_free_one(t);
        std::printf("[wiiu] mrp_world_gx2: sky: no memory for a %u x %u texture\n", w, h);
        return &g_white;
    }
    auto* dst = static_cast<uint32_t*>(t.surface.image);
    for (uint32_t y = 0; y < h; ++y) std::memcpy(dst + static_cast<size_t>(y) * t.surface.pitch, px->data() + static_cast<size_t>(y) * w, w * 4u);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, t.surface.image, t.surface.imageSize);
    ++g_stex.made;
    g_stex.bytes += t.surface.imageSize;
    std::printf("[wiiu] mrp_world_gx2: sky: level %d bucket %d: a %u x %u texture (%zu made, %zu KB)\n", L.id, bucket, w, h, g_stex.made, g_stex.bytes / 1024);
    return &t;
}

// the sky's data a frame's first call wrote (its vertices / indices in the sky's ring, its frame block in the blocks' ring), reused by the
// frame's next call while the frame and both rings' wraps are the same
struct SkyCache {
    uint64_t serial = ~0ull, swraps = ~0ull, wraps = ~0ull;
    void* vb = nullptr;
    void* ib = nullptr;
    uint32_t nverts = 0, nidx = 0;
    void* frame = nullptr;
} g_scache;
// (the log per 120 calls) GX2 draws, triangles, batches, batches not drawn (their depth test fails at the far plane), frames; the CPU's
// time: the capture and build (the frame's, mrpwb::FrameView::sky_ms), this backend's calls
uint64_t g_scalls = 0, g_sdraws = 0, g_stris = 0, g_sbatches = 0, g_snever = 0, g_sframes = 0, g_srefused = 0, g_slast_serial = ~0ull;
uint64_t g_shost_draws = 0;  // ((2026-10-28) of g_sdraws, the host sky's shells')
double g_sprep_ms = 0.0, g_scpu_ms = 0.0;

// ((2026-10-28) the host sky) the model's corners (mrpwb::FrameView::sky_host_verts: mrpwb::GpuV, written by the PPC as the CPU path's)
// in MEM2, copied once a model (a level); a draw's flat values (its triangles' closing corners: the texture word, layer, draw_flags,
// FOGCOL) and fog (every corner) written into its run when they differ from what the run holds - its first frame, or a GS state that
// changed since (none on Veldin: the same every frame)
struct SkyHostGpu {
    uint64_t serial = 0;
    void* vb = nullptr;
    uint32_t n = 0;
    uint64_t rebakes = 0;
} g_shost;
void sky_host_free() {
    mem_free(g_shost.vb);
    g_shost = SkyHostGpu{};
}
bool sky_host_vb(const mrpwb::FrameView& F) {
    if (F.sky_host_verts == nullptr || F.sky_host_verts->empty()) return false;
    if (g_shost.vb != nullptr && g_shost.serial == F.sky_host_serial) return true;
    if (g_shost.vb != nullptr) GX2DrawDone();  // (the old model's may still be read)
    sky_host_free();
    const uint32_t n = static_cast<uint32_t>(F.sky_host_verts->size());
    void* vb = mem_alloc(n * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), GX2_VERTEX_BUFFER_ALIGNMENT);
    if (vb == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: sky host: no memory for %u corners\n", n);
        return false;
    }
    std::memcpy(vb, F.sky_host_verts->data(), n * sizeof(mrpwb::GpuV));
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vb, n * static_cast<uint32_t>(sizeof(mrpwb::GpuV)));
    g_shost.vb = vb;
    g_shost.n = n;
    g_shost.serial = F.sky_host_serial;
    std::printf("[wiiu] mrp_world_gx2: sky host: model %llu, %u corners (%u KB) in MEM2\n", static_cast<unsigned long long>(F.sky_host_serial), n,
                n * static_cast<uint32_t>(sizeof(mrpwb::GpuV)) / 1024);
    return true;
}
// a draw's flat values and fog into its run (the CPU's copy is the GPU's: MEM2, invalidated after)
void sky_host_bake(const mrpwb::SkyHostDraw& d) {
    if (d.count < 3 || static_cast<uint64_t>(d.first) + d.count > g_shost.n) return;
    auto* v = static_cast<mrpwb::GpuV*>(g_shost.vb) + d.first;
    const mrpwb::GpuV& c = v[2];
    if (c.word == d.word && c.layer == d.layer && c.state[0] == d.state[0] && c.state[1] == d.state[1] && v[0].fog == d.fog) return;
    for (uint32_t i = 0; i < d.count; ++i) {
        v[i].word = d.word;
        v[i].layer = d.layer;
        v[i].state[0] = d.state[0];
        v[i].state[1] = d.state[1];
        v[i].fog = d.fog;
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, v, d.count * static_cast<uint32_t>(sizeof(mrpwb::GpuV)));
    ++g_shost.rebakes;
}

// orig: (host) the sky's shaders, fetch shaders, ring and textures freed (the GPU done with them: the caller waited)
void sky_free() {
    for (int k = 0; k < 4; ++k) {
        if (g_hok[k]) WHBGfxFreeShaderGroup(&g_hsh[k]);
        g_hsh[k] = WHBGfxShaderGroup{};
        g_hok[k] = false;
    }
    sky_host_free();
    for (int k = 0; k < 2; ++k) {
        mem_free(g_sfetch_prog[k]);
        g_sfetch[k] = GX2FetchShader{};
    }
    if (g_sring != nullptr) MEMFreeToDefaultHeap(g_sring);
    g_sring = nullptr;
    sky_tex_free();
    if (g_sps_noat != nullptr) WHBGfxFreeShaderGroup(&g_sshv_noat);
    g_sshv_noat = WHBGfxShaderGroup{};
    g_sps_noat = nullptr;
    fast_free(2);
    if (g_ssh.vertexShader != nullptr || g_ssh.pixelShader != nullptr) WHBGfxFreeShaderGroup(&g_ssh);
    g_ssh = WHBGfxShaderGroup{};
    g_scache = SkyCache{};
    g_sready = false;
}

// orig: (host) the sky's shaders (shaders/mrp_sky.gsh, or RAC1_WIIU_SKY_GSH's) and its ring; the sky on at the start unless
// RAC1_WIIU_SKY=0 (the debug menu's "Sky" turns it on and off: the shaders loaded either way). (2026-10-28: on - seen right on the console
// by Daino; its capture costs ~2-3 ms a host frame there, the game's sky pass more: a cheaper sky from the decoded asset is to do)
void sky_init() {
    const char* on = std::getenv("RAC1_WIIU_SKY");
    g_wiiu_sky = on == nullptr || on[0] == 0 || std::atoi(on) != 0;
    std::vector<uint8_t> gsh;
    const char* gsh_env = std::getenv("RAC1_WIIU_SKY_GSH");
    const bool variant = gsh_env != nullptr && gsh_env[0] != 0;
    if (std::FILE* f = std::fopen(variant ? gsh_env : "shaders/mrp_sky.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_ssh, 0, gsh.data())) {
        std::printf("[wiiu] mrp_world_gx2: shaders/mrp_sky.gsh not loaded: no sky\n");
        g_ssh = WHBGfxShaderGroup{};
        return;
    }
    const GX2VertexShader* vs = g_ssh.vertexShader;
    if (!gpuv_fetch(vs, g_sfetch, g_sfetch_prog, "sky")) {
        sky_free();
        return;
    }
    auto find = [](const GX2UniformBlock* blocks, uint32_t nb, const char* name, uint32_t min_size, BlockLoc& out) {
        for (uint32_t i = 0; i < nb; ++i)
            if (std::strcmp(blocks[i].name, name) == 0) {
                out.loc = blocks[i].offset;
                out.size = (std::max(blocks[i].size, min_size) + 15u) & ~15u;
                out.found = true;
            }
    };
    find(vs->uniformBlocks, vs->uniformBlockCount, "SkyFrame", sizeof(TfragFrameBlock), g_sframe_loc);
    const GX2PixelShader* ps = g_ssh.pixelShader;
    find(ps->uniformBlocks, ps->uniformBlockCount, "WorldPixel", sizeof(PixelBlock), g_spixel_loc);
    for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
        if (std::strcmp(ps->samplerVars[i].name, "uTex") == 0) {
            g_stex_loc = ps->samplerVars[i].location;
            g_stex_ok = true;
        }
    g_sring = static_cast<uint8_t*>(mem_alloc(kSkyRingBytes, GX2_VERTEX_BUFFER_ALIGNMENT));
    g_sring_off = 0;
    if (!g_sframe_loc.found || !g_spixel_loc.found || !g_stex_ok || g_sring == nullptr) {
        std::printf("[wiiu] mrp_world_gx2: sky: blocks frame %d pixel %d, sampler %d, memory %d: no sky\n", g_sframe_loc.found, g_spixel_loc.found, g_stex_ok,
                    g_sring != nullptr);
        sky_free();
        return;
    }
    g_sready = true;
    if (!variant) g_sps_noat = gx2_ps_variant("shaders/mrp_sky_noat.gsh", g_ssh, g_sshv_noat);
    if (!variant) fast_load(2, g_ssh);  // (the fast pixel shader's pairs, Gx2Perf::fast)
    {  // ((2026-10-28) the host sky's pairs)
        static const char* const kHost[4] = {"shaders/mrp_sky_host.gsh", "shaders/mrp_sky_host_noat.gsh", "shaders/mrp_sky_host_fast.gsh",
                                             "shaders/mrp_sky_host_fast_noat.gsh"};
        for (int k = 0; k < 4; ++k) {
            g_hok[k] = gx2_pair_variant(kHost[k], g_ssh, g_hsh[k]);
            if (g_hok[k] && k >= 2) gx2_ps_early_z(g_hsh[k].pixelShader, kHost[k]);
        }
        std::printf("[wiiu] mrp_world_gx2: sky host: shaders %s (RAC1_WIIU_SKY_HOST %s)\n", g_hok[0] ? "loaded" : "NOT loaded: the pass and its capture",
                    mrpw::sky_host_on() ? "on" : "off");
    }
    std::printf("[wiiu] mrp_world_gx2: sky up (%s; blocks: frame %u pixel %u bytes; ring %u KB); %s (RAC1_WIIU_SKY)\n", variant ? gsh_env : "shaders/mrp_sky.gsh",
                g_sframe_loc.size, g_spixel_loc.size, kSkyRingBytes / 1024, g_wiiu_sky ? "drawn" : "off");
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
    if (const char* e = std::getenv("RAC1_WIIU_WORLD_MERGE")) g_merge = std::clamp(std::atoi(e), 0, 4);
    if (g_merge > 0 && (!g_runs_ok || g_draw_loc.size < 2 * kSlot)) {
        std::printf("[wiiu] mrp_world_gx2: the merged draws off: no run table / instance slots in the shaders\n");
        g_merge = 0;
    }
    if (mrpwm::mode_units(g_merge) && !g_units_ok) {
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
    gx2_perf_init();
    if (!variant) variants_load(0, g_sh);  // (its pixel shader's other builds, Gx2Perf; not for another build of the shaders)
    if (!variant) fast_load(0, g_sh);      // (the fast pixel shader's pairs, Gx2Perf::fast)
    gx2_ps_early_z(g_sh.pixelShader, "mrp_world");
    for (int k = 1; k < 4; ++k) gx2_ps_early_z(g_ps[0][k], "mrp_world (another build)");
    std::printf("[wiiu] mrp_world_gx2: up (blocks: frame %u draw %u pixel %u bytes; merged draws: mode %d)\n", g_frame_loc.size, g_draw_loc.size, g_pixel_loc.size, g_merge);
    tfrag_init();
    sky_init();
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
    sky_free();
    variants_free(0);
    fast_free(0);
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
    // (the TV's context for the TV's buffer and the scaled 3D's, wiiu_gfx.cpp RAC1_WIIU_3D_SCALE)
    GX2ContextState* ctx = cb != WHBGfxGetDRCColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();
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
    if (L.loads != g_tex.loads || L.loads != g_geo.loads) {
        // Free the previous level's GPU/MEM2 buffers (textures, template geometry, merged plan, resident colours)
        // upfront after a single GX2DrawDone() so the old level's geometry and the new level's textures never
        // coexist in MEM2 during level transitions (level0 -> flight -> level1 or level0 -> level1).
        GX2DrawDone();
        if (L.loads != g_tex.loads) tex_free();
        if (L.loads != g_geo.loads) {
            geo_free();
            cols_free();
            g_mlevel = mrpwm::Level{};
            g_mg = Merged{};
        }
    }
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
    ring_gpu_invalidate();  // (Gx2Perf::ub_bulk: the blocks' GPU caches once for the call, before its first draw)
    if (F.serial != g_cache.serial || g_ring_wraps != g_cache.wraps || g_cache.draws.size() != F.draws->size()) cache_reset(F.serial, F.draws->size());

    const std::vector<mrpwb::Batch>& batches = *F.batches;
    const std::vector<mrpwb::DrawCmd>& draws = *F.draws;
    const std::vector<uint32_t>& cmds = *F.cmds;
    const std::vector<mrpwb::InstGpu>& insts = *F.insts;
    const std::vector<std::vector<std::pair<int, uint32_t>>>& epochs = *F.epochs;

    // the frame's plain uniforms (mrp_world_renderer.cpp's set_frame_uniforms)
    auto frame_block = [&] {
        FrameBlock fb{};
        const float fb_w = g_gx2_view.fb_w, fb_h = g_gx2_view.fb_h;  // (taken at the prepare: gx2_draw_view_take)
        fb.frame[0] = fb_w;
        fb.frame[1] = fb_h;
        fb.frame[2] = vp.xscale_3d;
        fb.frame[3] = 0.0f;
        const double da = g_gx2_view.da, dbb = g_gx2_view.db;
        const bool has = g_gx2_view.depth_ok;
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
        fb.mode2[0] = mrpwm::mode_instanced(g_merge) ? 1 : 0;  // (a draw of one instance: gl_InstanceID 0, its first slot either way)
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
    mrp_world_wiiu_tfrag_join(&F.tf_ms);
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
        std::memcpy(fb.vp, g_gx2_view.vp, sizeof fb.vp);  // (mrp_world_view_proj at the prepare: gx2_draw_view_take)
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
    bool cpu_ready = false;
    if (g_tready && g_sring != nullptr && F.cpu_verts != nullptr && F.cpu_idx != nullptr && !F.cpu_verts->empty() && !F.cpu_idx->empty()) {
        if (F.serial != g_scache.serial || g_sring_wraps != g_scache.swraps || g_scache.vb == nullptr) {
            const auto& sv = *F.cpu_verts;
            const auto& si = *F.cpu_idx;
            const uint32_t vbytes = static_cast<uint32_t>(sv.size() * sizeof(mrpwb::GpuV));
            const uint32_t at_i = (vbytes + GX2_VERTEX_BUFFER_ALIGNMENT - 1) & ~(GX2_VERTEX_BUFFER_ALIGNMENT - 1);
            const uint32_t ibytes = static_cast<uint32_t>(si.size() * sizeof(uint32_t));
            g_scache = SkyCache{};
            g_scache.serial = F.serial;
            if (auto* p = static_cast<uint8_t*>(sky_alloc(at_i + ibytes))) {
                std::memcpy(p, sv.data(), vbytes);
                std::memcpy(p + at_i, si.data(), ibytes);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, p, at_i + ibytes);
                g_scache.vb = p;
                g_scache.ib = p + at_i;
                g_scache.nverts = static_cast<uint32_t>(sv.size());
                g_scache.nidx = static_cast<uint32_t>(si.size());
            }
            g_scache.swraps = g_sring_wraps;
        }
        if (g_tcache.frame == nullptr || g_tcache.wraps != g_ring_wraps) {
            g_tcache.frame = tfrag_frame();
            g_tcache.wraps = g_ring_wraps;
        }
        cpu_ready = g_scache.vb != nullptr && g_scache.ib != nullptr && g_tcache.frame != nullptr;
    }

    // GX2's state for the whole call
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    // (testing, Gx2Perf::pixels_off: 8 x 8 pixels at the frame's centre - the draws' GPU time without their pixels')
    if (g_gx2_perf.pixels_off) GX2SetScissor(static_cast<uint32_t>(win_w / 2), static_cast<uint32_t>(win_h / 2), 8, 8);
    else GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetRasterizerClipControlEx(TRUE, mode == 1 ? FALSE : TRUE, FALSE);  // (the VU projection: the GL backend's depth clamp)
    // (testing, debug bit 256: the template's first vertex given by the attribute buffer's address instead of the draw's offset)
    const bool by_address = (g_debug & 256) != 0;
    const uint32_t vbytes = static_cast<uint32_t>(g_geo.v_count * sizeof(mrpwm::VertR));
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);

    // ((2026-10-28) Gx2Perf::flush 2: the commands so far sent to the GPU every flush_draws GX2 draws - it draws while the CPU makes the rest)
    uint32_t since_flush = 0;
    auto drawn = [&] {
        if (g_gx2_perf.flush >= 2 && ++since_flush >= g_gx2_perf.flush_draws) {
            GX2Flush();
            since_flush = 0;
        }
    };
    // the shaders of a batch: 0 the ties / shrubs' (mrp_world.gsh: the level's templates, the frame's plain uniforms, the resident colours),
    // 1 the tfrags' (mrp_tfrag.gsh: the frame's tfrag vertices, its frame block); set when the batches' path changes - the shader mode
    // when it differs (the two paths' the same: uniform blocks)
    int path = -1;
    int shader_mode = -1;
    auto set_mode = [&](GX2ShaderMode m) {
        if (static_cast<int>(m) == shader_mode) return;
        GX2SetShaderMode(m);
        shader_mode = static_cast<int>(m);
    };
    int cur_bucket = -2;
    uint32_t cur_sampler = ~0u;
    int units_epoch = -3;  // (the merged draws' units bound: the batch epoch's; -3 none)
    const GX2PixelShader* cur_ps = nullptr;  // (the pixel shader set: the path's own or another build of it, Gx2Perf)
    const GX2VertexShader* cur_vs = nullptr;  // (the vertex shader set: the path's own or a fast pair's, Gx2Perf::fast)
    auto use_ps = [&](const GX2PixelShader* ps) {
        if (ps == nullptr || ps == cur_ps) return;
        GX2SetPixelShader(ps);
        cur_ps = ps;
    };
    auto use_vs = [&](const GX2VertexShader* vs) {
        if (vs == nullptr || vs == cur_vs) return;
        GX2SetVertexShader(vs);
        cur_vs = vs;
    };
    PixelBlock last_px{};
    const void* last_px_blk = nullptr;
    uint64_t last_px_wraps = 0;
    auto set_pixel = [&](const PixelBlock& pb) {
        if (last_px_blk == nullptr || last_px_wraps != g_ring_wraps || !(last_px == pb)) {
            last_px_blk = block_of(&pb, sizeof pb, path != 0 ? g_tpixel_loc.size : g_pixel_loc.size);
            last_px_wraps = g_ring_wraps;
            last_px = pb;
        }
        const BlockLoc& pl = path != 0 ? g_tpixel_loc : g_pixel_loc;
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
            set_mode(g_sh.vertexShader->mode);
            GX2SetFetchShader(&g_sh.fetchShader);
            GX2SetVertexShader(g_sh.vertexShader);
            GX2SetPixelShader(g_sh.pixelShader);
            cur_vs = g_sh.vertexShader;
            cur_ps = g_sh.pixelShader;
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
            set_mode(g_tsh.vertexShader->mode);
            GX2SetFetchShader(&g_tfetch[(g_tdebug & 1024) != 0 ? 1 : 0]);
            GX2SetVertexShader(g_tsh.vertexShader);
            GX2SetPixelShader(g_tsh.pixelShader);
            cur_vs = g_tsh.vertexShader;
            cur_ps = g_tsh.pixelShader;
            if (want == 2) GX2SetAttribBuffer(0, g_scache.nverts * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), sizeof(mrpwb::GpuV), g_scache.vb);
            else GX2SetAttribBuffer(0, g_tcache.nverts * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), sizeof(mrpwb::GpuV), g_tcache.vb);
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
    auto draw_one = [&](const mrpwb::Batch& b, uint32_t i, bool pass2_only, auto&& set_p1_at) {
        const mrpwb::DrawCmd& c = draws[i];
        const uint32_t* rec = &cmds[8 * static_cast<size_t>(c.base_instance)];
        const bool need_p2 = rec[6] != 0;
        if (pass2_only && g_gx2_perf.p2_cull && !need_p2) return;
        if (!pass2_only) set_p1_at(need_p2);
        if (g_ring_wraps != g_cache.wraps) {  // (the ring wrapped during this call: what it held is written again as needed)
            const uint64_t serial = g_cache.serial;
            cache_reset(serial, draws.size());
            g_cache.frame = frame_block();
            if (g_cache.frame != nullptr && g_frame_loc.found) GX2SetVertexUniformBlock(g_frame_loc.loc, g_frame_loc.size, g_cache.frame);
            last_px_blk = nullptr;
            set_pixel(last_px);
        }
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
        drawn();
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
    // a batch's merged items (mrpwm::Plan): per item its block (its instances' slots; a lone command's own), its texture (mrpwm::kByUnit,
    // kInstancedByUnit: the item's unit bound), one GX2DrawIndexedEx - absolute indices (the level's or the frame's own) and its
    // instances, or a lone command's relative ones from its template's first vertex
    auto draw_items = [&](const mrpwb::Batch& b, size_t bi, bool pass2_only, auto&& set_p1_at) {
        const mrpwm::Plan& P = g_mg.plan;
        if (bi + 1 >= P.batch_items.size()) return;
        for (uint32_t k = P.batch_items[bi]; k < P.batch_items[bi + 1]; ++k) {
            const mrpwm::Item& it = P.items[k];
            if (pass2_only && g_gx2_perf.p2_cull && !it.pass2) continue;
            if (!pass2_only) set_p1_at(it.pass2);
            if (mrpwm::mode_by_unit(g_merge)) bind_unit(b, it.unit);
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
            drawn();
        }
    };
    // a tfrag batch's triangles: one indexed draw per run of one texture unit (mrpwb::TfragRange, from `r0`: the batch's first), its
    // texture bound first; the frame's tfrag vertices absolute (base vertex 0). (testing, tfrag debug bit 4: the batch in one draw, its
    // first range's texture)
    uint32_t tdraws_this_call = 0;
    auto draw_tfrag = [&](const mrpwb::Batch& b, size_t bi, size_t r0, bool pass2_only, auto&& set_p1_at) {
        const bool whole = (g_tdebug & 4) != 0;
        for (size_t r = r0; r < tranges.size() && tranges[r].batch == bi; ++r) {
            const mrpwb::TfragRange& R = tranges[r];
            if (!whole && pass2_only && g_gx2_perf.p2_cull && !R.pass2) continue;
            if (!whole && !pass2_only) set_p1_at(R.pass2);
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
            drawn();
            if (whole) return;
        }
    };

    g_draws_this_call = 0;
    static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;  // (testing: the failing pixels not drawn, as the GL backend's)
    size_t tr = 0;  // (the tfrag ranges' cursor: made in the batches' order)
    for (size_t bi = 0; bi < batches.size(); ++bi) {
        const mrpwb::Batch& b = batches[bi];
        if (b.sky || b.rk.shadow) {
            ++g_skipped;
            continue;
        }
        const bool cpu_batch = !b.gpu && !b.tfrag;
        if (cpu_batch && !cpu_ready) {
            ++g_skipped;
            continue;
        }
        if (b.tfrag && !tfrag) continue;
        if (!b.tfrag && (g_tdebug & 8) != 0) continue;  // (testing, tfrag debug bit 8: the tfrags alone)
        size_t r0 = 0;
        if (cpu_batch) {
            if (static_cast<uint64_t>(b.first) + b.count > g_scache.nidx) continue;
        } else if (b.tfrag) {  // the batch's triangles and ranges checked against the frame's tfrag indices
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
        bind_path(cpu_batch ? 2 : b.tfrag ? 1 : 0);
        if (b.tfrag || cpu_batch) ++g_tbatches;
        else ++g_batches;
        const int dbg = (b.tfrag || cpu_batch) ? g_tdebug : g_debug;  // (the tfrags' debug bits: RAC1_WIIU_TFRAG_DEBUG)
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
        const bool items = merged && !b.tfrag && !cpu_batch;
        pb.b[0] = items && mrpwm::mode_units(g_merge) ? 1 : 0;
        if (items && mrpwm::mode_units(g_merge)) bind_units(b);
        if (cpu_batch) {
            const GX2Texture* ct = sky_texture(L, k.bucket);
            if (g_ttex_ok) {
                GX2SetPixelTexture(ct, g_ttex_loc);
                GX2SetPixelSampler(&g_samplers[k.sampler & 15u], g_ttex_loc);
            }
            cur_bucket = -2;
            cur_sampler = ~0u;
        }
        // the batch's shaders (a pass's): the fast pair (Gx2Perf::fast; not with the shade debug nor the grey test - mrp_world.frag's), else
        // the path's own vertex shader and pixel shader build
        const int sp = (b.tfrag || cpu_batch) ? 1 : 0;
        const GX2VertexShader* own_vs = (b.tfrag || cpu_batch) ? g_tsh.vertexShader : g_sh.vertexShader;
        auto use_shaders = [&](bool at) {
            if (const WHBGfxShaderGroup* fp = shade == 0 && !grey ? fast_pair(sp, at) : nullptr) {
                use_vs(fp->vertexShader);
                use_ps(fp->pixelShader);
            } else {
                use_vs(own_vs);
                use_ps(variant_ps(sp, at));
            }
        };
        auto set_p1_at = [&](bool at) {
            if (g_gx2_perf.p2_cull && b.ate && !grey) use_shaders(at);
        };
        // pass 1: the passing pixels of the draws whose alpha test is on (their mode in draw_flags), every pixel of the others - by the
        // build without the alpha test when none of the batch's draws tests (Batch::ate; grey: the test off) and Gx2Perf says so
        use_shaders(b.ate && !grey);
        GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && zwrite ? TRUE : FALSE, zf);
        pb.a[0] = 1;
        set_pixel(pb);
        if (cpu_batch) {
            GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, b.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_scache.ib) + b.first, 0, 1);
            ++g_tdraws;
            g_ttris += b.count / 3;
            drawn();
        } else if (b.tfrag) draw_tfrag(b, bi, r0, false, set_p1_at);
        else if (items) draw_items(b, bi, false, set_p1_at);
        else
            for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(b, i, false, set_p1_at);
        wiiu_gpu_submark((b.tfrag || cpu_batch) ? kSmTfrag1 : kSmTies1);  // (RAC1_WIIU_GPU_TIME=2: the GPU's time by pass, wiiu_gfx.cpp)
        // pass 2: the failing ones as AFAIL says (RKey::pass2: 1 FB_ONLY the colour with alpha, 2 ZB_ONLY the depth, 3 RGB_ONLY the
        // colour without alpha)
        bool has_p2 = rk.pass2 != 0 && !one_pass && !grey;
        if (has_p2 && g_gx2_perf.p2_cull && !cpu_batch) {
            has_p2 = false;
            if (b.tfrag) {
                const bool whole = (g_tdebug & 4) != 0;
                if (whole) has_p2 = true;
                else
                    for (size_t r = r0; r < tranges.size() && tranges[r].batch == bi; ++r)
                        if (tranges[r].pass2) {
                            has_p2 = true;
                            break;
                        }
            } else if (items) {
                const mrpwm::Plan& P = g_mg.plan;
                if (bi + 1 < P.batch_items.size())
                    for (uint32_t q = P.batch_items[bi]; q < P.batch_items[bi + 1]; ++q)
                        if (P.items[q].pass2) {
                            has_p2 = true;
                            break;
                        }
            } else {
                for (uint32_t i = b.first; i < b.first + b.count; ++i)
                    if (cmds[8 * static_cast<size_t>(draws[i].base_instance) + 6] != 0) {
                        has_p2 = true;
                        break;
                    }
            }
        }
        if (has_p2) {
            const uint32_t afail = rk.pass2;
            const GX2CompareFunction zf2 = g_gx2_perf.p2_less && !(afail == 2 && zwrite) && zf == GX2_COMPARE_FUNC_LEQUAL ? GX2_COMPARE_FUNC_LESS : zf;
            use_shaders(true);
            GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && afail == 2 && zwrite ? TRUE : FALSE, zf2);
            set_mask(afail == 2 ? none : afail == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA);
            pb.a[0] = 2;
            set_pixel(pb);
            if (cpu_batch) {
                GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, b.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_scache.ib) + b.first, 0, 1);
                ++g_tdraws;
                g_ttris += b.count / 3;
                drawn();
            } else if (b.tfrag) draw_tfrag(b, bi, r0, true, set_p1_at);
            else if (items) draw_items(b, bi, true, set_p1_at);
            else
                for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(b, i, true, set_p1_at);
            set_mask(GX2_CHANNEL_MASK_RGBA);
            wiiu_gpu_submark((b.tfrag || cpu_batch) ? kSmTfrag2 : kSmTies2);
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


// orig: (host) see mrp_world_gx2.h: the frame's sky batches (Batch::sky, in their order: the CPU path's vertices / indices of the frame,
// mrpwb::FrameView::cpu_*) as the PC's draw_frame draws them - the GS's blending (PRIM ABE + ALPHA), the sky asset's texture (its bucket,
// Key::bucket / sampler), the alpha test's passes, the world's projection with the depth clamped to the far plane (mrp_sky.vert) - but the
// depth: the PC tests the sky at the far plane against the cleared depth (its GEQUAL / ALWAYS pass, GREATER / NEVER fail) and writes the
// value that is there; here the test and the writes are off, the batches whose test would fail not drawn. The depth function stays LEQUAL,
// the world's: no change of its direction, the depth buffer's hierarchical Z untouched (nothing read or written)
void mrp_world_gx2_draw_sky(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp) {
    if (!g_ready || !g_sready || !g_wiiu_sky || cb == nullptr) return;
    const auto t0 = std::chrono::steady_clock::now();
    mrpwb::FrameView F;
    mrpwb::LevelView L;
    if (!mrpwb::frame_view(F) || !mrpwb::level_view(L) || F.cpu_verts == nullptr || F.cpu_idx == nullptr) return;
    const std::vector<mrpwb::Batch>& batches = *F.batches;
    const std::vector<mrpwb::GpuV>& sv = *F.cpu_verts;
    const std::vector<uint32_t>& si = *F.cpu_idx;
    bool any_cpu = false, any_host = false;  // ((2026-10-28) the host sky's shells: Batch::host, drawn from the model's static corners)
    for (const mrpwb::Batch& b : batches)
        if (b.sky) (b.host >= 0 ? any_host : any_cpu) = true;
    if (sv.empty() || si.empty()) any_cpu = false;
    if (!any_cpu && !any_host) return;
    ++g_scalls;
    ring_gpu_invalidate();  // (Gx2Perf::ub_bulk: the blocks' GPU caches once for the call, before its first draw)
    if (F.serial != g_slast_serial) {  // (the log's: per frame)
        g_slast_serial = F.serial;
        ++g_sframes;
        g_sprep_ms += F.sky_ms;
    }
    // the frame's vertices / indices into the sky's ring (one allocation) and its frame block, by the frame's first call
    if (any_cpu && (F.serial != g_scache.serial || g_sring_wraps != g_scache.swraps || g_scache.vb == nullptr)) {
        const uint32_t vbytes = static_cast<uint32_t>(sv.size() * sizeof(mrpwb::GpuV));
        const uint32_t at_i = (vbytes + GX2_VERTEX_BUFFER_ALIGNMENT - 1) & ~(GX2_VERTEX_BUFFER_ALIGNMENT - 1);
        const uint32_t ibytes = static_cast<uint32_t>(si.size() * sizeof(uint32_t));
        g_scache = SkyCache{};
        g_scache.serial = F.serial;
        auto* p = static_cast<uint8_t*>(sky_alloc(at_i + ibytes));
        if (p != nullptr) {
            std::memcpy(p, sv.data(), vbytes);
            std::memcpy(p + at_i, si.data(), ibytes);
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, p, at_i + ibytes);
            g_scache.vb = p;
            g_scache.ib = p + at_i;
            g_scache.nverts = static_cast<uint32_t>(sv.size());
            g_scache.nidx = static_cast<uint32_t>(si.size());
        } else if (g_srefused++ < 4) {
            std::printf("[wiiu] mrp_world_gx2: sky: %u bytes of vertices / indices: more than the ring's %u\n", at_i + ibytes, kSkyRingBytes);
        }
        g_scache.swraps = g_sring_wraps;
    }
    auto frame_block = [&] {
        TfragFrameBlock fb{};
        std::memcpy(fb.vp, g_gx2_view.vp, sizeof fb.vp);  // (mrp_world_view_proj at the prepare: gx2_draw_view_take)
        return block_of(&fb, sizeof fb, g_sframe_loc.size);
    };
    if (g_scache.frame == nullptr || g_scache.wraps != g_ring_wraps) {
        g_scache.frame = frame_block();
        g_scache.wraps = g_ring_wraps;
    }
    if (g_scache.frame == nullptr || (any_cpu && g_scache.vb == nullptr && !any_host)) return;
    if (g_scache.vb == nullptr) any_cpu = false;
    const bool host_ok = any_host && g_hok[0] && F.sky_host != nullptr && sky_host_vb(F);

    // GX2's state for the call (the world's, mrp_world_gx2_draw: the same viewport, scissor, no culling, the GS's alpha test in the shader)
    const int win_w = static_cast<int>(cb->surface.width), win_h = static_cast<int>(cb->surface.height);
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    // (the scissor the game's frame: the clipper keeps a primitive whole inside its guard band, past the viewport - the sky's triangles,
    // as wide as the view, were drawn into the bars beside the 4:3 frame on the console, 2026-10-28)
    if (g_gx2_perf.pixels_off) GX2SetScissor(static_cast<uint32_t>(win_w / 2), static_cast<uint32_t>(win_h / 2), 8, 8);
    else GX2SetScissor(static_cast<uint32_t>(std::max(0, vp.x)), static_cast<uint32_t>(std::max(0, win_h - vp.y - vp.h)), static_cast<uint32_t>(vp.w), static_cast<uint32_t>(vp.h));
    GX2SetRasterizerClipControlEx(TRUE, TRUE, FALSE);  // (the shader puts the depth inside the far plane: nothing clipped there)
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_LEQUAL);
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);
    GX2SetShaderMode(g_ssh.vertexShader->mode);
    GX2SetFetchShader(&g_sfetch[(g_sdebug & 1024) != 0 ? 1 : 0]);
    GX2SetVertexShader(g_ssh.vertexShader);
    GX2SetPixelShader(g_ssh.pixelShader);
    const GX2PixelShader* cur_ps = g_ssh.pixelShader;  // (its own or the build without the alpha test, Gx2Perf::nokill)
    const GX2VertexShader* cur_vs = g_ssh.vertexShader;  // (its own or a fast pair's, Gx2Perf::fast)
    auto use_ps = [&](const GX2PixelShader* ps) {
        if (ps == cur_ps) return;
        GX2SetPixelShader(ps);
        cur_ps = ps;
    };
    // (2026-10-28) a pass's shaders: the fast pair (Gx2Perf::fast; not with the shade debug nor the grey test), else its own vertex shader
    // and its own pixel shader or the build without the alpha test (`at` false, Gx2Perf::nokill)
    auto use_shaders = [&](bool at, bool plain) {
        const WHBGfxShaderGroup* fp = plain ? fast_pair(2, at) : nullptr;
        const GX2VertexShader* vs = fp != nullptr ? fp->vertexShader : g_ssh.vertexShader;
        if (vs != cur_vs) {
            GX2SetVertexShader(vs);
            cur_vs = vs;
        }
        use_ps(fp != nullptr ? fp->pixelShader : g_gx2_perf.nokill && g_sps_noat != nullptr && !at ? g_sps_noat : g_ssh.pixelShader);
    };
    // (the CPU path's vertices and frame block, or a host shell's: its static corners, the world's view-projection x its matrix)
    int bound = -1;  // (0 the CPU path's, 1 a host shell's)
    auto bind_cpu = [&] {
        if (bound == 0) return;
        GX2SetAttribBuffer(0, g_scache.nverts * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), sizeof(mrpwb::GpuV), g_scache.vb);
        GX2SetVertexUniformBlock(g_sframe_loc.loc, g_sframe_loc.size, g_scache.frame);
        bound = 0;
    };
    auto bind_host = [&](const mrpwb::SkyHostDraw& d) -> bool {
        TfragFrameBlock fb{};
        const float* A = g_gx2_view.vp;  // (column-major; the shell's columns x, y, z - a direction, w 0 - and (0, 0, 0, 1))
        const float S[16] = {d.m[0], d.m[1], d.m[2], 0.0f, d.m[4], d.m[5], d.m[6], 0.0f, d.m[8], d.m[9], d.m[10], 0.0f, 0.0f, 0.0f, 0.0f, 1.0f};
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 4; ++r) {
                float acc = 0.0f;
                for (int k = 0; k < 4; ++k) acc += A[k * 4 + r] * S[c * 4 + k];
                fb.vp[c * 4 + r] = acc;
            }
        void* blk = block_of(&fb, sizeof fb, g_sframe_loc.size);
        if (blk == nullptr) return false;
        if (bound != 1) GX2SetAttribBuffer(0, g_shost.n * static_cast<uint32_t>(sizeof(mrpwb::GpuV)), sizeof(mrpwb::GpuV), g_shost.vb);
        GX2SetVertexUniformBlock(g_sframe_loc.loc, g_sframe_loc.size, blk);
        bound = 1;
        return true;
    };
    // the host pairs (g_hsh) as use_shaders picks the sky's own: the fast pair, else the main one or its build without the alpha test
    auto use_host_shaders = [&](bool at, bool plain) {
        int k = 0;
        if (plain && g_gx2_perf.fast) k = !at && g_gx2_perf.nokill && g_hok[3] ? 3 : g_hok[2] ? 2 : 0;
        if (k == 0 && !at && g_gx2_perf.nokill && g_hok[1]) k = 1;
        const WHBGfxShaderGroup& g = g_hsh[k];
        if (g.vertexShader != cur_vs) {
            GX2SetVertexShader(g.vertexShader);
            cur_vs = g.vertexShader;
        }
        use_ps(g.pixelShader);
    };

    static const bool one_pass = std::getenv("RAC1_MRPW_ONEPASS") != nullptr;  // (testing: the failing pixels not drawn, as the world's)
    const GX2Texture* cur_tex = nullptr;
    uint32_t cur_sampler = ~0u, draws_this_call = 0;
    auto draw = [&](const mrpwb::Batch& b) {
        if ((g_sdebug & 2048) != 0 && draws_this_call++ > 0) return;  // (testing: one draw a call)
        if (b.host >= 0) {  // (a host shell's run: its corners in order, three a triangle)
            const mrpwb::SkyHostDraw& d = (*F.sky_host)[static_cast<size_t>(b.host)];
            static bool told = false;
            if (!told) {
                told = true;
                std::printf("[wiiu] mrp_world_gx2: first sky host draw: corners %u from %u, bucket %d, prim %llx alpha %llx test %llx; matrix x %g %g %g y %g %g %g "
                            "z %g %g %g; word %08x state %08x %08x\n",
                            d.count, d.first, b.key.bucket, static_cast<unsigned long long>(b.key.prim), static_cast<unsigned long long>(b.key.alpha),
                            static_cast<unsigned long long>(b.key.test), d.m[0], d.m[1], d.m[2], d.m[4], d.m[5], d.m[6], d.m[8], d.m[9], d.m[10], d.word, d.state[0],
                            d.state[1]);
                std::fflush(stdout);
            }
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, d.count, d.first, 1);
            ++g_sdraws;
            ++g_shost_draws;
            g_stris += d.count / 3;
            return;
        }
        static bool dumped = false;
        if (!dumped && b.count >= 3) {  // (the first sky draw's data, on the log: what the GPU reads)
            dumped = true;
            const uint32_t* ix = static_cast<const uint32_t*>(g_scache.ib) + b.first;
            const mrpwb::GpuV& g = sv[ix[0]];
            std::printf("[wiiu] mrp_world_gx2: first sky draw: %u indices from %u (%u %u %u), bucket %d sampler %u, prim %llx alpha %llx test %llx; vertex %u: wp %g %g %g "
                        "%g stq %g %g %g rgba %02x%02x%02x%02x word %08x state %08x %08x\n",
                        b.count, b.first, ix[0], ix[1], ix[2], b.key.bucket, b.key.sampler, static_cast<unsigned long long>(b.key.prim),
                        static_cast<unsigned long long>(b.key.alpha), static_cast<unsigned long long>(b.key.test), ix[0], g.wp[0], g.wp[1], g.wp[2], g.wp[3], g.stq[0],
                        g.stq[1], g.stq[2], g.rgba[0], g.rgba[1], g.rgba[2], g.rgba[3], g.word, g.state[0], g.state[1]);
            std::fflush(stdout);
        }
        GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, b.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_scache.ib) + b.first, 0, 1);
        ++g_sdraws;
        g_stris += b.count / 3;
    };
    for (const mrpwb::Batch& b : batches) {
        if (!b.sky || b.gpu || b.tfrag) continue;
        const bool host = b.host >= 0;
        if (host) {
            if (!host_ok || static_cast<size_t>(b.host) >= F.sky_host->size()) continue;
            const mrpwb::SkyHostDraw& d = (*F.sky_host)[static_cast<size_t>(b.host)];
            if (static_cast<uint64_t>(d.first) + d.count > g_shost.n) {
                if (g_srefused++ < 8) std::printf("[wiiu] mrp_world_gx2: sky host draw refused: first %u count %u (%u corners)\n", d.first, d.count, g_shost.n);
                continue;
            }
        } else if (!any_cpu || static_cast<uint64_t>(b.first) + b.count > g_scache.nidx) {
            if (any_cpu && g_srefused++ < 8) std::printf("[wiiu] mrp_world_gx2: sky batch refused: first %u count %u (%u indices)\n", b.first, b.count, g_scache.nidx);
            continue;
        }
        const mrpwb::Key& k = b.key;
        const mrpwb::RKey& rk = b.rk;
        // (the PC's depth test at the far plane, against the cleared depth: NEVER and GREATER - the host's LESS - fail; testing, debug bit 16:
        // drawn anyway)
        const uint32_t ztst = fld(k.test, 17, 2);
        if (fld(k.test, 16, 1) != 0 && (ztst == 0 || ztst == 3) && (g_sdebug & 16) == 0) {
            ++g_snever;
            continue;
        }
        ++g_sbatches;
        const bool grey = (g_sdebug & 2) != 0;
        const int shade = grey ? 3 : (g_sdebug & 4096) != 0 ? 1 : (g_sdebug & 8192) != 0 ? 2 : g_render_shade;
        if (grey) GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        else gs::set_blend_gx2(k.prim, k.alpha);
        const GX2Texture* t = sky_texture(L, k.bucket);
        if (t != cur_tex) {
            GX2SetPixelTexture(t, g_stex_loc);
            cur_tex = t;
        }
        if ((k.sampler & 15u) != cur_sampler) {
            cur_sampler = k.sampler & 15u;
            GX2SetPixelSampler(&g_samplers[cur_sampler], g_stex_loc);
        }
        if (host) {
            const mrpwb::SkyHostDraw& d = (*F.sky_host)[static_cast<size_t>(b.host)];
            sky_host_bake(d);
            if (!bind_host(d)) continue;
        } else {
            bind_cpu();
        }
        PixelBlock pb{};
        pb.fog_col[0] = static_cast<float>(rk.fogcol & 0xff) / 255.0f;
        pb.fog_col[1] = static_cast<float>((rk.fogcol >> 8) & 0xff) / 255.0f;
        pb.fog_col[2] = static_cast<float>((rk.fogcol >> 16) & 0xff) / 255.0f;
        pb.a[1] = shade;
        pb.a[2] = (rk.fogcol >> 24) & 1 ? 1 : 0;
        pb.a[3] = grey ? 1 : 0;
        // pass 1: the passing pixels of the draws whose alpha test is on (their mode in draw_flags), every pixel of the others - by the
        // build without the alpha test when none of the batch's draws tests (Batch::ate; grey: the test off) and Gx2Perf says so
        if (host) use_host_shaders(b.ate && !grey, shade == 0 && !grey);
        else use_shaders(b.ate && !grey, shade == 0 && !grey);
        pb.a[0] = 1;
        if (void* blk = block_of(&pb, sizeof pb, g_spixel_loc.size)) GX2SetPixelUniformBlock(g_spixel_loc.loc, g_spixel_loc.size, blk);
        draw(b);
        wiiu_gpu_submark(kSmSky1);  // (RAC1_WIIU_GPU_TIME=2: the GPU's time by pass, wiiu_gfx.cpp)
        // pass 2: the failing ones as AFAIL says (1 FB_ONLY the colour with alpha, 3 RGB_ONLY the colour without alpha; 2 ZB_ONLY the depth
        // alone: nothing here, the depth not written)
        if (rk.pass2 != 0 && rk.pass2 != 2 && !one_pass && !grey) {
            set_mask(rk.pass2 == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA);
            if (host) use_host_shaders(true, shade == 0);
            else use_shaders(true, shade == 0);
            pb.a[0] = 2;
            if (void* blk = block_of(&pb, sizeof pb, g_spixel_loc.size)) GX2SetPixelUniformBlock(g_spixel_loc.loc, g_spixel_loc.size, blk);
            draw(b);
            set_mask(GX2_CHANNEL_MASK_RGBA);
            wiiu_gpu_submark(kSmSky2);
        }
    }

    // the state left as the header says (the world's)
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    set_mask(GX2_CHANNEL_MASK_RGBA);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetRasterizerClipControlEx(TRUE, TRUE, FALSE);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);

    g_scpu_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (g_scalls % 120 == 0) {
        std::printf("[wiiu] mrp_world_gx2: sky, last 120 calls: %.1f GX2 draws, %.1f triangles, %.1f batches (%.1f not drawn: their depth test fails at the far "
                    "plane); %llu frames: %zu vertices, the capture and build %.2f ms a frame (game thread and worker), the draws %.3f ms a call; %zu textures (%zu KB), "
                    "%llu refused, ring wraps %llu; host shells: %.1f draws a call (%s), %llu runs' values written again\n",
                    static_cast<double>(g_sdraws) / 120.0, static_cast<double>(g_stris) / 120.0, static_cast<double>(g_sbatches) / 120.0,
                    static_cast<double>(g_snever) / 120.0, static_cast<unsigned long long>(g_sframes), sv.size(),
                    g_sframes != 0 ? g_sprep_ms / static_cast<double>(g_sframes) : 0.0, g_scpu_ms / 120.0, g_stex.made, g_stex.bytes / 1024,
                    static_cast<unsigned long long>(g_srefused), static_cast<unsigned long long>(g_sring_wraps), static_cast<double>(g_shost_draws) / 120.0,
                    any_host ? (host_ok ? "drawn" : "NOT drawn: no shaders or memory") : "none", static_cast<unsigned long long>(g_shost.rebakes));
        std::fflush(stdout);
        g_sdraws = g_stris = g_sbatches = g_snever = g_sframes = g_shost_draws = 0;
        g_sprep_ms = g_scpu_ms = 0.0;
    }
}

namespace {
struct FxTextures {
    uint64_t loads = ~0ull;
    std::unordered_map<uint64_t, GX2Texture> map;
    size_t bytes = 0;
} g_fxtex;

void fx_tex_free() {
    for (auto& [id, t] : g_fxtex.map) tex_free_one(t);
    g_fxtex = FxTextures{};
}

const GX2Texture* fx_texture(const mrpwb::LevelView& L, const gs::TexImage* ti) {
    if (g_fxtex.loads != L.loads || g_fxtex.bytes > 8u * 1024u * 1024u) {
        GX2DrawDone();
        fx_tex_free();
        g_fxtex.loads = L.loads;
    }
    if (ti == nullptr || ti->w <= 0 || ti->h <= 0 || ti->px.empty()) return &g_white;
    auto it = g_fxtex.map.find(ti->id);
    if (it != g_fxtex.map.end()) return it->second.surface.image != nullptr ? &it->second : &g_white;
    GX2Texture& t = g_fxtex.map[ti->id];
    const uint32_t w = static_cast<uint32_t>(ti->w), h = static_cast<uint32_t>(ti->h);
    if (!tex_make(t, w, h, 1, 1, GX2_TILE_MODE_LINEAR_ALIGNED)) {
        tex_free_one(t);
        return &g_white;
    }
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_A, GX2_SQ_SEL_B, GX2_SQ_SEL_G, GX2_SQ_SEL_R);
    GX2InitTextureRegs(&t);
    auto* dst = static_cast<uint32_t*>(t.surface.image);
    for (uint32_t y = 0; y < h && static_cast<size_t>(y + 1) * w <= ti->px.size(); ++y)
        std::memcpy(dst + static_cast<size_t>(y) * t.surface.pitch, ti->px.data() + static_cast<size_t>(y) * w, w * 4u);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, t.surface.image, t.surface.imageSize);
    g_fxtex.bytes += t.surface.imageSize;
    return &t;
}

struct FxCache {
    uint64_t serial = ~0ull, gwraps = ~0ull;
    void* vb = nullptr;
    uint32_t nverts = 0;
} g_fxcache;
}  // namespace

void mrp_world_gx2_draw_fx(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp) {
    if (!g_ready || !g_tready || cb == nullptr) return;
    mrpwb::LevelView L;
    if (!mrpwb::level_view(L)) return;
    MrpShadowsWiiuView sv;
    const bool have_sh = mrp_shadows_wiiu_view(sv) && sv.mark_floats >= 9;
    MrpFxWiiuView fv;
    const bool have_fx = mrp_fx_wiiu_view(fv) && fv.nverts >= 3;
    if (!have_sh && !have_fx) return;

    ring_gpu_invalidate();
    TfragFrameBlock fb{};
    std::memcpy(fb.vp, g_gx2_view.vp, sizeof fb.vp);
    void* frame_blk = block_of(&fb, sizeof fb, g_tframe_loc.size);
    if (frame_blk == nullptr) return;

    const int win_w = static_cast<int>(cb->surface.width), win_h = static_cast<int>(cb->surface.height);
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    if (g_gx2_perf.pixels_off) GX2SetScissor(static_cast<uint32_t>(win_w / 2), static_cast<uint32_t>(win_h / 2), 8, 8);
    else GX2SetScissor(static_cast<uint32_t>(std::max(0, vp.x)), static_cast<uint32_t>(std::max(0, win_h - vp.y - vp.h)), static_cast<uint32_t>(vp.w), static_cast<uint32_t>(vp.h));
    GX2SetRasterizerClipControlEx(TRUE, TRUE, FALSE);
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);

    GX2SetShaderMode(g_tsh.vertexShader->mode);
    GX2SetFetchShader(&g_tfetch[0]);
    const GX2VertexShader* cur_vs = g_tsh.vertexShader;
    const GX2PixelShader* cur_ps = g_tsh.pixelShader;
    GX2SetVertexShader(cur_vs);
    GX2SetPixelShader(cur_ps);
    if (g_tframe_loc.found) GX2SetVertexUniformBlock(g_tframe_loc.loc, g_tframe_loc.size, frame_blk);

    auto use_shaders = [&](bool at, bool plain) {
        const WHBGfxShaderGroup* fp = plain ? fast_pair(1, at) : nullptr;
        const GX2VertexShader* vs = fp != nullptr ? fp->vertexShader : g_tsh.vertexShader;
        const GX2PixelShader* ps = fp != nullptr ? fp->pixelShader : variant_ps(1, at);
        if (vs != cur_vs) {
            GX2SetVertexShader(vs);
            cur_vs = vs;
        }
        if (ps != cur_ps) {
            GX2SetPixelShader(ps);
            cur_ps = ps;
        }
    };

    if (have_sh) {
        const uint32_t sverts = static_cast<uint32_t>(sv.mark_floats / 3);
        const uint32_t sbytes = sverts * static_cast<uint32_t>(sizeof(mrpwb::GpuV));
        if (auto* svb = static_cast<mrpwb::GpuV*>(geo_alloc(sbytes))) {
            for (uint32_t i = 0; i < sverts; ++i) {
                mrpwb::GpuV& g = svb[i];
                g = mrpwb::GpuV{};
                g.wp[0] = sv.mark[3 * i + 0];
                g.wp[1] = sv.mark[3 * i + 1];
                g.wp[2] = sv.mark[3 * i + 2];
                g.wp[3] = 1.0f;
                g.stq[2] = 1.0f;
                g.lodq = 1.0f;
                for (int l = 0; l < 4; ++l) g.rgba[l] = static_cast<uint8_t>((sv.apply_rgba >> (8 * l)) & 0xffu);
                g.fog = 255.0f;
                g.word = 1u;
            }
            GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, svb, sbytes);
            GX2SetAttribBuffer(0, sbytes, sizeof(mrpwb::GpuV), svb);
            if (g_ttex_ok) {
                GX2SetPixelTexture(&g_white, g_ttex_loc);
                GX2SetPixelSampler(&g_samplers[0], g_ttex_loc);
            }
            gs::set_blend_gx2(1ull << 6, sv.apply_alpha);
            GX2SetDepthOnlyControl(db != nullptr ? TRUE : FALSE, FALSE, GX2_COMPARE_FUNC_LEQUAL);
            use_shaders(false, true);
            PixelBlock pb{};
            pb.a[0] = 1;
            if (void* blk = block_of(&pb, sizeof pb, g_tpixel_loc.size); blk != nullptr && g_tpixel_loc.found)
                GX2SetPixelUniformBlock(g_tpixel_loc.loc, g_tpixel_loc.size, blk);
            GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, sverts, 0, 1);
        }
    }

    if (have_fx) {
        const uint32_t vbytes = static_cast<uint32_t>(fv.nverts * sizeof(mrpwb::GpuV));
        if (fv.serial != g_fxcache.serial || g_gring_wraps != g_fxcache.gwraps || g_fxcache.vb == nullptr) {
            g_fxcache = FxCache{};
            g_fxcache.serial = fv.serial;
            if (void* p = geo_alloc(vbytes)) {
                std::memcpy(p, fv.verts, vbytes);
                GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, p, vbytes);
                g_fxcache.vb = p;
                g_fxcache.nverts = static_cast<uint32_t>(fv.nverts);
            }
            g_fxcache.gwraps = g_gring_wraps;
        }
        if (g_fxcache.vb != nullptr) {
            GX2SetAttribBuffer(0, vbytes, sizeof(mrpwb::GpuV), g_fxcache.vb);
            const GX2Texture* cur_tex = nullptr;
            uint32_t cur_sampler = ~0u;
            const int shade = g_render_shade;
            for (const MrpFxWiiuBatch& b : *fv.batches) {
                if (b.count == 0 || static_cast<uint64_t>(b.first) + b.count > g_fxcache.nverts) continue;
                gs::set_blend_gx2(b.prim, b.alpha);
                const bool ztest = db != nullptr && b.ztest;
                const bool zwrite = ztest && b.zwrite;
                const GX2CompareFunction zf = b.ztst == 0 ? GX2_COMPARE_FUNC_NEVER : b.ztst == 2 ? GX2_COMPARE_FUNC_LEQUAL : GX2_COMPARE_FUNC_LESS;
                const GX2Texture* t = fx_texture(L, b.tex.get());
                if (g_ttex_ok && t != cur_tex) {
                    GX2SetPixelTexture(t, g_ttex_loc);
                    cur_tex = t;
                }
                const uint32_t smp = b.sampler & 15u;
                if (g_ttex_ok && smp != cur_sampler) {
                    GX2SetPixelSampler(&g_samplers[smp], g_ttex_loc);
                    cur_sampler = smp;
                }
                PixelBlock pb{};
                pb.fog_col[0] = static_cast<float>(b.fogcol & 0xff) / 255.0f;
                pb.fog_col[1] = static_cast<float>((b.fogcol >> 8) & 0xff) / 255.0f;
                pb.fog_col[2] = static_cast<float>((b.fogcol >> 16) & 0xff) / 255.0f;
                pb.a[0] = 1;
                pb.a[1] = shade;
                pb.a[2] = b.fge ? 1 : 0;
                use_shaders(b.ate, shade == 0);
                GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, zwrite ? TRUE : FALSE, zf);
                if (void* blk = block_of(&pb, sizeof pb, g_tpixel_loc.size); blk != nullptr && g_tpixel_loc.found)
                    GX2SetPixelUniformBlock(g_tpixel_loc.loc, g_tpixel_loc.size, blk);
                GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, b.count, b.first, 1);
                if (b.pass2 != 0) {
                    const uint32_t afail = b.pass2;
                    use_shaders(true, shade == 0);
                    GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, ztest && afail == 2 && zwrite ? TRUE : FALSE, zf);
                    set_mask(afail == 2 ? none : afail == 3 ? GX2_CHANNEL_MASK_RGB : GX2_CHANNEL_MASK_RGBA);
                    pb.a[0] = 2;
                    if (void* blk = block_of(&pb, sizeof pb, g_tpixel_loc.size); blk != nullptr && g_tpixel_loc.found)
                        GX2SetPixelUniformBlock(g_tpixel_loc.loc, g_tpixel_loc.size, blk);
                    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, b.count, b.first, 1);
                    set_mask(GX2_CHANNEL_MASK_RGBA);
                }
            }
        }
    }

    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    set_mask(GX2_CHANNEL_MASK_RGBA);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetRasterizerClipControlEx(TRUE, TRUE, FALSE);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
}

}  // namespace rac1
