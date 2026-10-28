// The MRP's mobys drawn with GX2 on the Wii U (MRP_LITE.md phase 2; how a caller uses it: mrp_moby_gx2.h). A port of
// mrp_moby_renderer.cpp's GL draw (draw_batches) over the same CPU preparation, read through mrp_moby_backend.inc:
//
// - the level: its GPU vertices (40 bytes, built by the PPC: big-endian) and u32 indices (big-endian: GX2_INDEX_TYPE_U32, not
//   _LE) copied into MEM2 buffers, appended as the frames meet new classes; its texture buckets as GX2 2D-array textures with
//   their mips (each layer's level written into a linear surface and copied into the tiled array by GX2CopySurface; RGBA bytes
//   in memory order: the identity component map);
// - per draw (no storage buffers, no indirect draws, no gl_BaseInstance): a uniform block with the draw's part (PartGpu, 18
//   vec4) and its record (part, texture word, layer), one GX2DrawIndexedEx per draw command; per moby a uniform block of its
//   joints (at most kMaxJoints: four vec4 each, 8 KB); per call the frame's plain uniforms (the GL backend's, as MobyVkPush) and
//   the normal table (128 vec4); per batch the pixel shader's block (TME, the alpha test, AFAIL, FGE, FOGCOL, the mip mode). All
//   written into a MEM2 ring as 32-bit big-endian words (what the PPC writes is what GX2 reads), aligned to 0x100;
// - the GPU state per batch as the GL backend sets it: the texture array and the sampler (TEX1's filters, CLAMP's wraps), the
//   GS's blending (gs::set_blend_gx2, as the 2D's), the depth test (the GS's GEQUAL / GREATER as LEQUAL / LESS on the host's
//   depth, Z written unless ZMSK; no test: no writes, as GL), the alpha test's fail modes with the GL backend's split (a Z pass,
//   colour masked, then the colour with LEQUAL and no Z writes);
// - the world's projection (g_mrp_moby_projection 0, the PC's default: the parts' clip matrices), the world's ties / shrubs' too
//   (mrp_world_gx2.cpp, drawn before into the same depth). (testing: RAC1_WIIU_MRP_PROJ=vu the game's own VU1 projection onto its
//   4:3 frame, the depth a + b Z the host's GS depth mapping gives, the GL backend's depth clamp as the z clipping off)
//
// Not drawn (phase 2): the drop shadows, the modern lighting (none of its hooks), the mobys drawn into render targets (the menus'
// item models: Key::rt), the casters' cache; the deferred mobys are drawn in the frame's order (after the others, no shadows
// between). Host code.
#include "mrp_moby_gx2.h"

#include "gs_draw_gx2.h"

#include <game/ovl/ovl_common.h>
#include <game/render/mrp_chain.h>
#include <game/render/mrp_moby_backend.inc>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_emu.h>

#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
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
#include <unordered_map>
#include <vector>

namespace rac1 {

namespace {

constexpr uint32_t kMaxJoints = 128;  // (mrp_moby.vert's MobyJoints: J[512])

// The vertex shader's blocks (mrp_moby.vert, std140)
struct alignas(16) FrameBlock {
    float offset[4];
    float frame[4];     // fb width, fb height, x scale, 1 a render target
    float xyoff[4];     // XYOFFSET x, y (1/16 px), depth a, depth b
    float proj_fog[4];  // B+292 x, B+291 x
    int32_t mode[4];
};
static_assert(sizeof(FrameBlock) == 80);
struct alignas(16) DrawBlock {
    mrpm::PartGpu part;
    uint32_t cmd[4];
};
static_assert(sizeof(DrawBlock) == 304);
// The pixel shader's (mrp_moby.frag's MobyPixel)
struct alignas(16) PixelBlock {
    float fog_col[4];
    int32_t a[4];  // TME, ATE, ATST, AREF
    int32_t b[4];  // AFAIL, the mip mode, the shade debug, FGE
    int32_t c[4];  // the pass
    bool operator==(const PixelBlock& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};
static_assert(sizeof(PixelBlock) == 64);

bool g_ready = false;
WHBGfxShaderGroup g_sh{};
// (testing, RAC1_WIIU_MRP_DEBUG bit 128: the attributes fetched as SCALED - the integers converted to floats - instead of
// INTEGER; its fetch shader only: the shaders are g_sh's)
WHBGfxShaderGroup g_sh_scaled{};  // (its fetch program kept until the app ends)
bool g_scaled_ok = false;
// the blocks' locations and sizes (the compiler's: at least the structs')
struct BlockLoc {
    uint32_t loc = 0, size = 0;
    bool found = false;
};
BlockLoc g_frame_loc, g_normals_loc, g_joints_loc, g_draw_loc, g_pixel_loc;
uint32_t g_sampler_loc = 0;
bool g_sampler_ok = false;
GX2Sampler g_samplers[2][16];  // [mip mode][TEX1 / CLAMP bits: 0 MMAG linear, 1 MMIN linear, 2 wrap s, 3 wrap t]

// The uniform blocks, appended to a ring in MEM2 (gs_draw_gx2.cpp's): before the ring wraps over what the GPU may still read
// the CPU waits for it (GX2DrawDone)
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
// a block of `size` bytes holding `data` (`bytes` of it; the rest left as it is: never read), written out of the CPU's cache.
// The shaders' blocks are read by the GPU as little-endian words: written with each 32-bit word swapped (`vertex`: any block; testing:
// RAC1_WIIU_MRP_PSWAP=0 the pixel shader's block as the PPC writes it)
void* block_of(const void* data, uint32_t bytes, uint32_t size, bool vertex) {
    void* p = ring_alloc(size);
    if (p == nullptr) return nullptr;
    const uint32_t n = std::min(bytes, size);
    if (vertex) {
        const auto* src = static_cast<const uint8_t*>(data);
        auto* dst = static_cast<uint32_t*>(p);
        for (uint32_t i = 0; i + 4 <= n; i += 4) {
            uint32_t w;
            std::memcpy(&w, src + i, 4);
            dst[i / 4] = __builtin_bswap32(w);
        }
    } else {
        std::memcpy(p, data, n);
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

// its vertices and indices: what was copied (a prefix of the level's, which the frames append to), the buffers' room
struct Geometry {
    uint64_t loads = ~0ull, table_hash = 0;
    void* vb = nullptr;
    void* ib = nullptr;
    size_t v_cap = 0, i_cap = 0;  // (elements)
    size_t v_count = 0, i_count = 0;
} g_geo;

// the level's vertices / indices from `from` on copied (the buffers made larger first when they must: the GPU waited for)
bool geo_copy(void*& buf, size_t& cap, size_t from, size_t count, const void* src, size_t elem, uint32_t align) {
    if (count > cap) {
        size_t ncap = std::max<size_t>(count + count / 2, 4096);
        void* nb = mem_alloc(static_cast<uint32_t>(ncap * elem), align);
        if (nb == nullptr) return false;
        GX2DrawDone();  // (the old buffer may still be read)
        if (buf != nullptr && from > 0) std::memcpy(nb, buf, from * elem);
        mem_free(buf);
        buf = nb;
        cap = ncap;
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, buf, static_cast<uint32_t>(from * elem));
    }
    if (count > from) {
        uint8_t* dst = static_cast<uint8_t*>(buf) + from * elem;
        std::memcpy(dst, static_cast<const uint8_t*>(src) + from * elem, (count - from) * elem);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, dst, static_cast<uint32_t>((count - from) * elem));
    }
    return true;
}
void geo_sync(const mrpm::LevelView& L) {
    const std::vector<mrpm::GpuVertex>& v = *L.verts;
    const std::vector<uint32_t>& ix = *L.indices;
    bool fresh = L.loads != g_geo.loads || L.table_hash != g_geo.table_hash || v.size() < g_geo.v_count || ix.size() < g_geo.i_count;
    if (fresh) {  // (dropped and built again: everything copied anew, once the GPU is done with the frames drawn from it)
        GX2DrawDone();
        g_geo.loads = L.loads;
        g_geo.table_hash = L.table_hash;
        g_geo.v_count = g_geo.i_count = 0;
    }
    if (v.size() == g_geo.v_count && ix.size() == g_geo.i_count) return;
    // (indices first refer to vertices appended with them: both copied before a draw reads them)
    if (!geo_copy(g_geo.vb, g_geo.v_cap, g_geo.v_count, v.size(), v.data(), sizeof(mrpm::GpuVertex), GX2_VERTEX_BUFFER_ALIGNMENT) ||
        !geo_copy(g_geo.ib, g_geo.i_cap, g_geo.i_count, ix.size(), ix.data(), sizeof(uint32_t), GX2_INDEX_BUFFER_ALIGNMENT)) {
        std::printf("[wiiu] mrp_moby_gx2: no memory for the level's %zu vertices / %zu indices\n", v.size(), ix.size());
        g_geo.v_count = g_geo.i_count = 0;
        g_geo.loads = ~0ull;
        return;
    }
    g_geo.v_count = v.size();
    g_geo.i_count = ix.size();
}
void geo_free() {
    mem_free(g_geo.vb);
    mem_free(g_geo.ib);
    g_geo = Geometry{};
}

// its texture arrays (one per bucket), made at the level's first draw
struct Textures {
    uint64_t loads = ~0ull;
    std::vector<GX2Texture> arrays;
} g_tex;
GX2Texture g_white{};  // a 1 x 1 array of one white layer (a batch without an array: the shader's white texture or none)

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
    // (the texels' bytes R, G, B, A in memory: what the GPU's R8_G8_B8_A8 reads as R ... A; RAC1_WIIU_MRP_TEXSWAP=1 (testing) reverses
    // them, should GX2CopySurface or the format read the words the other way)
    static const bool swap = std::getenv("RAC1_WIIU_MRP_TEXSWAP") != nullptr && std::getenv("RAC1_WIIU_MRP_TEXSWAP")[0] == '1';
    t.compMap = swap ? GX2_COMP_MAP(GX2_SQ_SEL_A, GX2_SQ_SEL_B, GX2_SQ_SEL_G, GX2_SQ_SEL_R) : GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
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

// the level's buckets made: each layer's level decoded (mrpm::bucket_texels) into a linear surface, copied into the array's
// level and slice by the GPU (GX2CopySurface: the tiling and the mips' places are GX2's), the GPU waited for after each array
// (the linear surfaces freed). Changes the GPU's state (the caller sets its context again).
void tex_sync(const mrpm::LevelView& L) {
    if (L.loads == g_tex.loads) return;
    GX2DrawDone();  // (the old arrays may still be sampled)
    tex_free();
    g_tex.loads = L.loads;
    const auto t0 = std::chrono::steady_clock::now();
    size_t bytes = 0, failed = 0;
    std::vector<uint32_t> px;
    std::vector<void*> temps;
    g_tex.arrays.resize(L.buckets);
    for (size_t bi = 0; bi < L.buckets; ++bi) {
        const mrpm::BucketInfo bk = mrpm::bucket_info(bi);
        GX2Texture& t = g_tex.arrays[bi];
        if (bk.layers == 0 || bk.levels == 0 || !tex_make(t, bk.w, bk.h, bk.layers, bk.levels, GX2_TILE_MODE_DEFAULT)) {
            tex_free_one(t);
            ++failed;
            continue;
        }
        bytes += t.surface.imageSize + t.surface.mipmapSize;
        std::printf("[wiiu] mrp_moby_gx2: array %zu: %ux%u, %u layers, %u levels, %u + %u bytes\n", bi, bk.w, bk.h, bk.layers, bk.levels, t.surface.imageSize,
                    t.surface.mipmapSize);
        std::fflush(stdout);
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
                mrpm::bucket_texels(bi, layer, l, px);
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
    std::printf("[wiiu] mrp_moby_gx2: level %d: %zu texture arrays, %zu KB (%zu not made) in %.0f ms\n", L.id, g_tex.arrays.size(), bytes / 1024, failed,
                std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count());
    std::fflush(stdout);
}

// ---- the per-call caches ----------------------------------------------------------------------------------------------
// The blocks written for a frame (the TV's call) reused by its next call (the GamePad's): valid while the frame and the ring's
// wraps are the same (a wrap: the GPU waited for and the ring written again from its start)
struct Cache {
    uint64_t serial = ~0ull, wraps = ~0ull;
    std::vector<void*> draws;                      // per draw command: its DrawBlock
    std::unordered_map<uint64_t, void*> joints;    // per (first joint, count): the moby's joints block
    void* frame = nullptr;
    void* frame_own = nullptr;                     // (the title menu's boxes: the 4:3 projection whatever the frame's)
    void* normals = nullptr;
} g_cache;
void cache_reset(uint64_t serial, size_t draws) {
    g_cache.serial = serial;
    g_cache.wraps = g_ring_wraps;
    g_cache.draws.assign(draws, nullptr);
    g_cache.joints.clear();
    g_cache.frame = g_cache.frame_own = g_cache.normals = nullptr;
}

int g_debug = 0;  // (RAC1_WIIU_MRP_DEBUG's mode for this frame: mrp_moby_gx2.h)

uint64_t g_tris = 0;
uint32_t g_draws_this_call = 0;
uint64_t g_calls = 0, g_draws = 0, g_batches = 0, g_refused = 0, g_skipped_rt = 0, g_joint_clamps = 0;
double g_cpu_ms = 0.0;

}  // namespace

bool mrp_moby_gx2_ready() { return g_ready; }

void mrp_moby_gx2_debug_frame(uint64_t game_frame) {
    static const char* list = std::getenv("RAC1_WIIU_MRP_DEBUG");
    static const uint64_t at = std::getenv("RAC1_WIIU_MRP_DEBUG_AT") != nullptr ? std::strtoull(std::getenv("RAC1_WIIU_MRP_DEBUG_AT"), nullptr, 10) : 125;
    int mode = 0;
    if (list != nullptr && game_frame >= at) {
        uint64_t band = (game_frame - at) / 50;
        const char* q = list;  // (past the list's end its last mode stays)
        for (const char* n = std::strchr(q, ','); band > 0 && n != nullptr; --band, n = std::strchr(q, ',')) q = n + 1;
        mode = std::atoi(q);
    }
    if (mode != g_debug) {
        std::printf("[wiiu] mrp_moby_gx2: debug mode %d from game frame %llu\n", mode, static_cast<unsigned long long>(game_frame));
        std::fflush(stdout);
        g_cache.serial = ~0ull;  // (the frame's blocks written again: the mode is in them)
    }
    g_debug = mode;
}

bool mrp_moby_gx2_init() {
    if (g_ready) return true;
    std::vector<uint8_t> gsh;
    // (testing, RAC1_WIIU_MRP_GSH=<file>: another build of the shaders - a reduced one: the blocks and the sampler it lacks
    // are not bound)
    const char* gsh_env = std::getenv("RAC1_WIIU_MRP_GSH");
    const bool variant = gsh_env != nullptr && gsh_env[0] != 0;
    if (std::FILE* f = std::fopen(variant ? gsh_env : "shaders/mrp_moby.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_sh, 0, gsh.data())) {
        std::printf("[wiiu] mrp_moby_gx2: shaders/mrp_moby.gsh not loaded\n");
        return false;
    }
    // The vertex's attributes (mrpm::GpuVertex, 40 bytes, written by the PPC: big-endian). The shader reads them as integers
    // (ivec4 / uvec4): the fetch shader's integer formats (GX2_ATTRIB_FLAG_INTEGER: the values as integers in the registers,
    // not converted to floats; _SIGNED: sign-extended), the 16-bit ones swapped within each 16 bits (8_IN_16: the PPC's
    // big-endian s16 / u16 read as their values, the normal's u16 included), the 8-bit ones not swapped (byte 0 is X)
    const auto s16x4 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_SIGNED | GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_16_16_16_16);
    const auto s16x2 = static_cast<GX2AttribFormat>(GX2_ATTRIB_FLAG_SIGNED | GX2_ATTRIB_FLAG_INTEGER | GX2_ATTRIB_TYPE_16_16);
    const auto u8x4 = GX2_ATTRIB_FORMAT_UINT_8_8_8_8;
    struct A {
        const char* name;
        uint32_t offset;
        GX2AttribFormat format;
        bool two;  // two components (z 0, w 1)
    };
    const A attrs[] = {
        {"aPosN", offsetof(mrpm::GpuVertex, pos), s16x4, false},        {"aJoint", offsetof(mrpm::GpuVertex, joint), u8x4, false},
        {"aWeight", offsetof(mrpm::GpuVertex, weight), u8x4, false},    {"aRgba", offsetof(mrpm::GpuVertex, rgba), u8x4, false},
        {"aUv", offsetof(mrpm::GpuVertex, uv), s16x2, true},            {"aQ2Pos", offsetof(mrpm::GpuVertex, q2pos), s16x4, false},
        {"aQ2Joint", offsetof(mrpm::GpuVertex, q2joint), u8x4, false}, {"aQ2Weight", offsetof(mrpm::GpuVertex, q2weight), u8x4, false},
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
        s.mask = a.two ? GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_0, GX2_SQ_SEL_1) : GX2_SEL_MASK(GX2_SQ_SEL_X, GX2_SQ_SEL_Y, GX2_SQ_SEL_Z, GX2_SQ_SEL_W);
        s.endianSwap = a.format == u8x4 ? GX2_ENDIAN_SWAP_NONE : GX2_ENDIAN_SWAP_8_IN_16;
    }
    g_sh_scaled = g_sh;
    for (uint32_t i = 0; i < g_sh_scaled.numAttributes; ++i) {
        GX2AttribStream& a = g_sh_scaled.attributes[i];
        a.format = static_cast<GX2AttribFormat>((static_cast<uint32_t>(a.format) & ~static_cast<uint32_t>(GX2_ATTRIB_FLAG_INTEGER)) | GX2_ATTRIB_FLAG_SCALED);
    }
    g_sh_scaled.fetchShader = GX2FetchShader{};
    g_sh_scaled.fetchShaderProgram = nullptr;
    g_scaled_ok = WHBGfxInitFetchShader(&g_sh_scaled);
    std::printf("[wiiu] mrp_moby_gx2: %u attributes:", g_sh.numAttributes);
    for (uint32_t i = 0; i < g_sh.numAttributes; ++i) std::printf(" loc %u off %u fmt %#x", g_sh.attributes[i].location, g_sh.attributes[i].offset, g_sh.attributes[i].format);
    std::printf("\n");
    if (!WHBGfxInitFetchShader(&g_sh)) {
        std::printf("[wiiu] mrp_moby_gx2: fetch shader not made\n");
        WHBGfxFreeShaderGroup(&g_sh);
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
    find(vs->uniformBlocks, vs->uniformBlockCount, "MobyFrame", sizeof(FrameBlock), g_frame_loc);
    find(vs->uniformBlocks, vs->uniformBlockCount, "MobyNormals", 0x200 * 4, g_normals_loc);
    find(vs->uniformBlocks, vs->uniformBlockCount, "MobyJoints", kMaxJoints * 64, g_joints_loc);
    find(vs->uniformBlocks, vs->uniformBlockCount, "MobyDraw", sizeof(DrawBlock), g_draw_loc);
    const GX2PixelShader* ps = g_sh.pixelShader;
    find(ps->uniformBlocks, ps->uniformBlockCount, "MobyPixel", sizeof(PixelBlock), g_pixel_loc);
    bool sampler = false;
    for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
        if (std::strcmp(ps->samplerVars[i].name, "uTex") == 0) {
            g_sampler_loc = ps->samplerVars[i].location;
            sampler = true;
        }
    g_sampler_ok = sampler;
    g_ring = static_cast<uint8_t*>(mem_alloc(kRingBytes, GX2_UNIFORM_BLOCK_ALIGNMENT));
    const bool white = g_ring != nullptr && tex_make(g_white, 1, 1, 1, 1, GX2_TILE_MODE_LINEAR_ALIGNED);
    if (variant)
        std::printf("[wiiu] mrp_moby_gx2: %s: blocks frame %d normals %d joints %d draw %d pixel %d, sampler %d\n", gsh_env, g_frame_loc.found, g_normals_loc.found,
                    g_joints_loc.found, g_draw_loc.found, g_pixel_loc.found, sampler);
    if ((!variant && (!g_frame_loc.found || !g_normals_loc.found || !g_joints_loc.found || !g_draw_loc.found || !g_pixel_loc.found || !sampler)) || !white) {
        std::printf("[wiiu] mrp_moby_gx2: blocks frame %d normals %d joints %d draw %d pixel %d, sampler %d, memory %d\n", g_frame_loc.found,
                    g_normals_loc.found, g_joints_loc.found, g_draw_loc.found, g_pixel_loc.found, sampler, white);
        tex_free_one(g_white);
        if (g_ring != nullptr) MEMFreeToDefaultHeap(g_ring);
        g_ring = nullptr;
        WHBGfxFreeShaderGroup(&g_sh);
        g_sh = WHBGfxShaderGroup{};
        return false;
    }
    std::memset(g_white.surface.image, 0xff, g_white.surface.imageSize);  // (white in any tiling)
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, g_white.surface.image, g_white.surface.imageSize);
    // the samplers (mrp_moby_renderer.cpp's init_gl): mip mode 0 (the GS's level, chosen in the shader: textureLod) nearest
    // between levels, mode 1 linear; the filters from TEX1's MMAG / MMIN, the wraps from CLAMP's WMS / WMT; no anisotropy
    for (int mm = 0; mm < 2; ++mm)
        for (uint32_t s = 0; s < 16; ++s) {
            GX2Sampler& smp = g_samplers[mm][s];
            GX2InitSampler(&smp, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_POINT);
            const GX2TexXYFilterMode mag = (s & 1) != 0 ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT;
            const GX2TexXYFilterMode mn = mm != 0 || (s & 2) != 0 ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT;
            GX2InitSamplerXYFilter(&smp, mag, mn, GX2_TEX_ANISO_RATIO_NONE);
            GX2InitSamplerZMFilter(&smp, GX2_TEX_Z_FILTER_MODE_POINT, mm == 0 ? GX2_TEX_MIP_FILTER_MODE_POINT : GX2_TEX_MIP_FILTER_MODE_LINEAR);
            GX2InitSamplerClamping(&smp, (s & 4) != 0 ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP, (s & 8) != 0 ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                                   GX2_TEX_CLAMP_MODE_CLAMP);
            GX2InitSamplerLOD(&smp, 0.0f, 13.0f, 0.0f);
        }
    g_ring_off = 0;
    // the world's projection (the world's ties / shrubs are drawn in it too, mrp_world_gx2.cpp). (testing: RAC1_WIIU_MRP_PROJ=vu the
    // game's own VU1 projection onto its 4:3 frame)
    const char* pj = std::getenv("RAC1_WIIU_MRP_PROJ");
    g_mrp_moby_projection = pj != nullptr && std::strcmp(pj, "vu") == 0 ? 1 : 0;
    g_ready = true;
    std::printf("[wiiu] mrp_moby_gx2: up (blocks: frame %u normals %u joints %u draw %u pixel %u bytes)\n", g_frame_loc.size, g_normals_loc.size,
                g_joints_loc.size, g_draw_loc.size, g_pixel_loc.size);
    return true;
}

void mrp_moby_gx2_shutdown() {
    if (!g_ready) return;
    GX2DrawDone();
    tex_free();
    geo_free();
    tex_free_one(g_white);
    MEMFreeToDefaultHeap(g_ring);
    g_ring = nullptr;
    g_cache = Cache{};
    WHBGfxFreeShaderGroup(&g_sh);
    g_sh = WHBGfxShaderGroup{};
    g_ready = false;
}

void mrp_moby_gx2_draw(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const gs::Viewport& vp) {
    if (!g_ready || cb == nullptr) return;
    // (testing, RAC1_WIIU_MRP_STAGE=<n>: how far a call goes - 0 / 1 nothing (the depth's clear is wiiu_gfx.cpp's now, before the
    // world), 2 the level's textures made, 3 its geometry copied and the state set, no draws, 4 (the default) everything)
    static const int stage = std::getenv("RAC1_WIIU_MRP_STAGE") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_MRP_STAGE")) : 4;
    if (stage <= 0) return;
    const auto t0 = std::chrono::steady_clock::now();
    const int win_w = static_cast<int>(cb->surface.width), win_h = static_cast<int>(cb->surface.height);
    GX2ContextState* ctx = cb == WHBGfxGetTVColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();
    mrpm::FrameView F;
    mrpm::LevelView L;
    const bool have = mrpm::frame_view(F) && mrpm::level_view(L);
    // the level's textures (GX2CopySurface) change the GPU's state: the context set again after them
    static uint64_t logged = 0;
    if (logged < 3 && have && !F.batches->empty()) {  // (the first frames' size, on the log)
        ++logged;
        std::printf("[wiiu] mrp_moby_gx2: frame %llu: %zu batches, %zu draws, %zu parts, %zu joints; level %d: %zu vertices, %zu indices, %zu buckets\n",
                    static_cast<unsigned long long>(F.serial), F.batches->size(), F.draws->size(), F.parts->size(), F.joints->size(), L.id, L.verts->size(),
                    L.indices->size(), L.buckets);
        std::fflush(stdout);
    }
    bool restore = false;
    if (stage >= 2 && have && L.loads != g_tex.loads) {
        std::printf("[wiiu] mrp_moby_gx2: level %d's textures: %zu arrays\n", L.id, L.buckets);
        std::fflush(stdout);
        tex_sync(L);
        restore = true;
    }
    static bool ctx_logged = false;
    if (!ctx_logged) {
        ctx_logged = true;
        std::printf("[wiiu] mrp_moby_gx2: context %p (TV %p, DRC %p)\n", static_cast<void*>(ctx), static_cast<void*>(WHBGfxGetTVContextState()),
                    static_cast<void*>(WHBGfxGetDRCContextState()));
    }
    if (restore && ctx != nullptr) GX2SetContextState(ctx);
    if (!have || F.batches->empty() || stage < 2) return;  // (stage 1: as 0 since the depth's clear left this call)
    geo_sync(L);
    if (g_geo.vb == nullptr || g_geo.ib == nullptr) return;
    static bool geo_logged = false;
    if (!geo_logged) {
        geo_logged = true;
        std::printf("[wiiu] mrp_moby_gx2: geometry on the GPU (%zu vertices, %zu indices)\n", g_geo.v_count, g_geo.i_count);
        std::fflush(stdout);
    }
    ++g_calls;
    if (F.serial != g_cache.serial || g_ring_wraps != g_cache.wraps || g_cache.draws.size() != F.draws->size()) cache_reset(F.serial, F.draws->size());

    const std::vector<mrpm::Batch>& batches = *F.batches;
    const std::vector<mrpm::DrawCmd>& draws = *F.draws;
    const std::vector<uint32_t>& cmds = *F.cmds;
    const std::vector<mrpm::PartGpu>& parts = *F.parts;
    const std::vector<MrpMat4>& joints = *F.joints;

    // the frame's plain uniforms (mrp_moby_renderer.cpp's set_common_uniforms / fill_common_vk_push)
    auto frame_block = [&](bool own) {
        FrameBlock fb{};
        std::memcpy(fb.offset, F.offset, sizeof fb.offset);
        float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
        if (fb_w <= 0.0f) fb_w = 512.0f;
        if (fb_h <= 0.0f) fb_h = 416.0f;
        fb.frame[0] = fb_w;
        fb.frame[1] = fb_h;
        fb.frame[2] = own || F.menu ? vp.xscale : vp.xscale_3d;
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
        fb.proj_fog[0] = F.proj;
        fb.proj_fog[1] = F.fog_min;
        fb.mode[0] = own ? 1 : (g_debug & 4) != 0 ? 0 : F.mode;
        fb.mode[1] = (g_debug & 256) != 0 ? 2 : (g_debug & 16) != 0 ? 1 : 0;
        return block_of(&fb, sizeof fb, g_frame_loc.size, true);
    };
    if (g_cache.frame == nullptr) g_cache.frame = frame_block(false);
    if (g_cache.normals == nullptr) g_cache.normals = block_of(F.normals, 0x200 * 4, g_normals_loc.size, true);
    if (g_cache.frame == nullptr || g_cache.normals == nullptr) return;
    float fog_col[4];
    {  // FOGCOL (the GS register the game's packets set: R, G, B in the low 24 bits; as the frame's chain left it - the MRP's reader, or
       // the GS replica's walk: mrp_chain_gs_reg, as the PC's mobys)
        const uint64_t fc = mrp_chain_gs_reg(0x3d);
        fog_col[0] = static_cast<float>(fc & 0xff) / 255.0f;
        fog_col[1] = static_cast<float>((fc >> 8) & 0xff) / 255.0f;
        fog_col[2] = static_cast<float>((fc >> 16) & 0xff) / 255.0f;
        fog_col[3] = 0.0f;
    }

    // GX2's state for the whole call
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetShaderMode(g_sh.vertexShader->mode);  // (uniform blocks: a reduced build without any, registers)
    GX2SetFetchShader((g_debug & 128) != 0 && g_scaled_ok ? &g_sh_scaled.fetchShader : &g_sh.fetchShader);
    GX2SetVertexShader(g_sh.vertexShader);
    GX2SetPixelShader(g_sh.pixelShader);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetViewport(static_cast<float>(vp.x), static_cast<float>(win_h - vp.y - vp.h), static_cast<float>(vp.w), static_cast<float>(vp.h), 0.0f, 1.0f);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetAttribBuffer(0, static_cast<uint32_t>(g_geo.v_count * sizeof(mrpm::GpuVertex)), sizeof(mrpm::GpuVertex), g_geo.vb);
    if (g_normals_loc.found) GX2SetVertexUniformBlock(g_normals_loc.loc, g_normals_loc.size, g_cache.normals);
    const auto none = static_cast<GX2ChannelMask>(0);
    auto set_mask = [&](GX2ChannelMask m) { GX2SetTargetChannelMasks(m, none, none, none, none, none, none, none); };
    set_mask(GX2_CHANNEL_MASK_RGBA);
    // (testing: RAC1_WIIU_MRP_ZCLIP=1 keeps the z clipping in the game's projection, where the GL backend clamps the depth)
    static const bool keep_zclip = std::getenv("RAC1_WIIU_MRP_ZCLIP") != nullptr && std::getenv("RAC1_WIIU_MRP_ZCLIP")[0] == '1';
    const int mm = g_mrp_moby_mip != 0 ? 1 : 0;

    bool own_on = false, frame_set = false;
    auto own_view = [&](bool on) {  // (the boot's title menu's boxes: the game's 4:3 projection, whatever the frame's)
        if (frame_set && on == own_on) return;
        frame_set = true;
        own_on = on;
        if (on && g_cache.frame_own == nullptr) g_cache.frame_own = frame_block(true);
        void* fb = on ? g_cache.frame_own : g_cache.frame;
        if (fb != nullptr && g_frame_loc.found) GX2SetVertexUniformBlock(g_frame_loc.loc, g_frame_loc.size, fb);
        GX2SetRasterizerClipControlEx(TRUE, (on || F.mode == 1) && !keep_zclip ? FALSE : TRUE, FALSE);  // (z in [-w, w]: GL's clip space)
    };
    const void* bound_joints = nullptr;
    PixelBlock last_px{};
    const void* last_px_blk = nullptr;
    uint64_t last_px_wraps = 0;
    auto set_pixel = [&](const PixelBlock& pb) {
        if (last_px_blk == nullptr || last_px_wraps != g_ring_wraps || !(last_px == pb)) {
            static const bool pswap = std::getenv("RAC1_WIIU_MRP_PSWAP") == nullptr || std::atoi(std::getenv("RAC1_WIIU_MRP_PSWAP")) != 0;
            last_px_blk = block_of(&pb, sizeof pb, g_pixel_loc.size, pswap);
            last_px_wraps = g_ring_wraps;
            last_px = pb;
        }
        if (last_px_blk != nullptr && g_pixel_loc.found) GX2SetPixelUniformBlock(g_pixel_loc.loc, g_pixel_loc.size, last_px_blk);
    };
    // one draw command: its joints' block (the moby's, relative: first joint 0), its part and record, the indexed draw
    auto draw_one = [&](uint32_t i) {
        if (g_ring_wraps != g_cache.wraps) {  // (the ring wrapped during this call: what it held is written again as needed)
            const uint64_t serial = g_cache.serial;
            cache_reset(serial, draws.size());
            bound_joints = nullptr;
            g_cache.frame = frame_block(false);
            g_cache.normals = block_of(F.normals, 0x200 * 4, g_normals_loc.size, true);
            if (g_cache.normals != nullptr && g_normals_loc.found) GX2SetVertexUniformBlock(g_normals_loc.loc, g_normals_loc.size, g_cache.normals);
            frame_set = false;
            own_view(own_on);
        }
        const mrpm::DrawCmd& c = draws[i];
        const uint32_t ci = c.base_instance;
        const uint32_t pi = cmds[4 * ci];
        const mrpm::PartGpu& P = parts[pi];
        const uint32_t jf = static_cast<uint32_t>(std::max(0, P.info[0]));
        uint32_t jc = static_cast<uint32_t>(std::max(0, P.info[1]));
        if (jf >= joints.size()) jc = 0;
        jc = std::min<uint32_t>(jc, static_cast<uint32_t>(joints.size() - std::min<size_t>(jf, joints.size())));
        if (jc > kMaxJoints) {
            if (g_joint_clamps++ < 4) std::printf("[wiiu] mrp_moby_gx2: a moby of %u joints drawn with its first %u\n", jc, kMaxJoints);
            jc = kMaxJoints;
        }
        const uint64_t jkey = static_cast<uint64_t>(jf) | (static_cast<uint64_t>(jc) << 32);
        void*& jb = g_cache.joints[jkey];
        if (jb == nullptr) jb = block_of(jc > 0 ? static_cast<const void*>(&joints[jf]) : static_cast<const void*>(F.normals), jc * 64u, g_joints_loc.size, true);
        void*& db_ = g_cache.draws[i];
        if (db_ == nullptr) {
            DrawBlock d;
            d.part = P;
            d.part.info[0] = 0;
            d.part.info[1] = static_cast<int32_t>(jc);
            std::memcpy(d.cmd, &cmds[4 * ci], sizeof d.cmd);
            db_ = block_of(&d, sizeof d, g_draw_loc.size, true);
        }
        if (jb == nullptr || db_ == nullptr) return;
        if (jb != bound_joints && g_joints_loc.found) {
            GX2SetVertexUniformBlock(g_joints_loc.loc, g_joints_loc.size, jb);
            bound_joints = jb;
        }
        if (g_draw_loc.found) GX2SetVertexUniformBlock(g_draw_loc.loc, g_draw_loc.size, db_);
        if (stage < 4) return;
        if ((g_debug & 2048) != 0 && g_draws_this_call++ > 0) return;  // (testing: one draw a call - the screen triangles drawn once)
        static bool dumped = false;
        if (!dumped && c.count >= 3) {  // (the first draw's data, on the log: what the GPU reads)
            dumped = true;
            const uint32_t* ix = static_cast<const uint32_t*>(g_geo.ib) + c.first_index;
            const uint8_t* v = static_cast<const uint8_t*>(g_geo.vb) + static_cast<size_t>(ix[0]) * sizeof(mrpm::GpuVertex);
            std::printf("[wiiu] mrp_moby_gx2: first draw: %u indices from %u (%u %u %u), part %u, joints %u; vertex %u:", c.count, c.first_index, ix[0], ix[1], ix[2], pi, jc,
                        ix[0]);
            for (size_t b = 0; b < sizeof(mrpm::GpuVertex); ++b) std::printf(" %02x", v[b]);
            const float* r = P.rows;
            std::printf("; rows T %g %g %g %g\n", r[12], r[13], r[14], r[15]);
            std::fflush(stdout);
        }
        if ((g_debug & 32) != 0) GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, 0, 1);
        else GX2DrawIndexedEx(GX2_PRIMITIVE_MODE_TRIANGLES, c.count, GX2_INDEX_TYPE_U32, static_cast<const uint32_t*>(g_geo.ib) + c.first_index, 0, 1);
        ++g_draws;
        g_tris += c.count / 3;
    };

    g_draws_this_call = 0;
    int cur_bucket = -2;
    uint32_t cur_sampler = ~0u;
    for (const mrpm::Batch& b : batches) {
        const mrpm::Key& k = b.key;
        if (k.caster_only) continue;  // (the casters' cache: the shadow maps' only)
        if (k.rt != 0) {              // TODO: the menus' item models (drawn into a render target the 2D samples)
            ++g_skipped_rt;
            continue;
        }
        {  // the batch's draw commands checked against what they read (the GL backend's check): one outside is refused
            bool ok = static_cast<uint64_t>(b.first) + b.count <= draws.size();
            for (uint32_t i = b.first; ok && i < b.first + b.count; ++i) {
                const mrpm::DrawCmd& c = draws[i];
                ok = static_cast<uint64_t>(c.first_index) + c.count <= g_geo.i_count && c.base_vertex == 0 && c.instances == 1 &&
                     (static_cast<uint64_t>(c.base_instance) + 1) * 4 <= cmds.size() && cmds[4 * c.base_instance] < parts.size();
            }
            if (!ok) {
                if (g_refused++ < 8)
                    std::printf("[wiiu] mrp_moby_gx2: batch refused: first %u count %u (%zu draw commands, %zu records, %zu indices)\n", b.first, b.count, draws.size(),
                                cmds.size(), g_geo.i_count);
                continue;
            }
        }
        ++g_batches;
        own_view(k.own_view);
        if (k.bucket != cur_bucket) {
            const GX2Texture* t = k.bucket >= 0 && static_cast<size_t>(k.bucket) < g_tex.arrays.size() && g_tex.arrays[static_cast<size_t>(k.bucket)].surface.image != nullptr
                                      ? &g_tex.arrays[static_cast<size_t>(k.bucket)]
                                      : &g_white;
            if (g_sampler_ok) GX2SetPixelTexture(t, g_sampler_loc);
            cur_bucket = k.bucket;
        }
        if ((k.sampler & 15u) != cur_sampler) {
            cur_sampler = k.sampler & 15u;
            if (g_sampler_ok) GX2SetPixelSampler(&g_samplers[mm][cur_sampler], g_sampler_loc);
        }
        PixelBlock pb{};
        std::memcpy(pb.fog_col, fog_col, sizeof pb.fog_col);
        pb.a[0] = gs::field(k.prim, 4, 1) != 0 ? 1 : 0;
        pb.a[1] = static_cast<int32_t>(gs::field(k.test, 0, 1));
        pb.a[2] = static_cast<int32_t>(gs::field(k.test, 1, 3));
        pb.a[3] = static_cast<int32_t>(gs::field(k.test, 4, 8));
        pb.b[0] = static_cast<int32_t>(gs::field(k.test, 12, 2));
        pb.b[1] = g_mrp_moby_mip;
        pb.b[2] = (g_debug & 2) != 0 ? 3 : (g_debug & 4096) != 0 ? 1 : (g_debug & 8192) != 0 ? 2 : g_render_shade;
        if ((g_debug & 2) != 0) pb.a[1] = 0;
        pb.b[3] = g_mrp_moby_fog != 0 && gs::field(k.prim, 5, 1) != 0 ? 1 : 0;
        if ((g_debug & 2) != 0) GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        else gs::set_blend_gx2(k.prim, k.alpha);
        // the depth: the GS's GEQUAL / GREATER on the host's depth as LEQUAL / LESS; ZMSK: no writes; no test: no writes (as GL)
        const uint32_t ztst = gs::field(k.test, 17, 2);
        const bool ztest = db != nullptr && gs::field(k.test, 16, 1) != 0 && ztst != 1 && (g_debug & 1) == 0;
        const bool zwrite = ztest && gs::field(k.zbuf, 32, 1) == 0;
        const GX2CompareFunction zf = ztst == 0 ? GX2_COMPARE_FUNC_NEVER : ztst == 2 ? GX2_COMPARE_FUNC_LEQUAL : GX2_COMPARE_FUNC_LESS;
        // the alpha test's fail modes (the GL backend's split): with Z writes and an alpha test whose failing pixels write one of
        // colour / Z (AFAIL FB_ONLY / ZB_ONLY / RGB_ONLY), the Z first (colour masked: the pixels the GS writes Z for), then the
        // colour against it with LEQUAL and no Z writes (the pixels it writes colour for)
        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;
        const bool split = !no_split && zwrite && pb.a[1] != 0 && pb.a[2] != 1 && pb.b[0] != 0;
        if (split) {
            GX2SetDepthOnlyControl(TRUE, TRUE, zf);
            set_mask(none);
            pb.c[0] = 1;
            set_pixel(pb);
            for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(i);
            set_mask(GX2_CHANNEL_MASK_RGBA);
            GX2SetDepthOnlyControl(TRUE, FALSE, ztst != 0 ? GX2_COMPARE_FUNC_LEQUAL : GX2_COMPARE_FUNC_NEVER);
        } else {
            GX2SetDepthOnlyControl(ztest ? TRUE : FALSE, zwrite ? TRUE : FALSE, zf);
        }
        pb.c[0] = 0;
        set_pixel(pb);
        for (uint32_t i = b.first; i < b.first + b.count; ++i) draw_one(i);
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
        std::printf("[wiiu] mrp_moby_gx2: last 120 calls: %.1f draws, %.1f triangles, %.1f batches, %.2f ms CPU a call; %llu batches refused, %llu render-target "
                    "batches skipped; level vertices %zu, indices %zu\n",
                    static_cast<double>(g_draws) / 120.0, static_cast<double>(g_tris) / 120.0, static_cast<double>(g_batches) / 120.0, g_cpu_ms / 120.0,
                    static_cast<unsigned long long>(g_refused), static_cast<unsigned long long>(g_skipped_rt), g_geo.v_count, g_geo.i_count);
        std::fflush(stdout);
        g_draws = g_batches = g_tris = 0;
        g_cpu_ms = 0.0;
    }
}

}  // namespace rac1
