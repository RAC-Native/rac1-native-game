// The GS replica's primitives (runtime/gs/gs_emu.h) drawn with GX2: the game's 2D on the Wii U - the HUD, the text, the
// menus, the fades. A port of the GL backend, rac1::gs::draw() in runtime/gs/gs_draw.cpp: the logic that does not depend
// on the API (the vertices, a sprite as two triangles, the batching by state, the frame -> window mapping with the
// viewport's xscale / crop / hud_shift / sprite_clamp / hide_edge_slivers, the scissor from SCISSOR, the colour mask from
// FBMSK, the texture's filter / wrap from TEX1 / CLAMP, the texture functions, TCC, the alpha test with AFAIL, FGE's fog,
// the blending from ALPHA with FIX as a constant, the render targets: the GS's drawing into pages the frame then samples)
// mirrors it; the places are marked "(gs_draw.cpp)". Host code.
//
// How a caller uses it (ports/wiiu/game/wiiu_gfx.cpp):
//
//     WHBGfxInit();
//     rac1::gs::draw_gx2_init();                       // shaders/gs_draw.gsh, beside the game's data
//     ...each frame:
//     WHBGfxBeginRender();
//     WHBGfxBeginRenderTV();
//     WHBGfxClearColor(0, 0, 0, 1);
//     const GX2ColorBuffer* cb = WHBGfxGetTVColourBuffer();          // 1280 x 720 by default
//     rac1::gs::Viewport vp;  // where the 512 x 416 (or so) frame goes: x / y from the bottom left, as the GL backend's
//     vp.x = ...; vp.y = ...; vp.w = ...; vp.h = ...; vp.screen_fbp = <FRAME's FBP of the shown frame>;
//     rac1::gs::draw_gx2(gs, cb, fb_w, fb_h, &vp, true);   // the frame's render targets drawn by this call
//     WHBGfxFinishRenderTV();
//     WHBGfxBeginRenderDRC();  ... the same with WHBGfxGetDRCColourBuffer() (854 x 480) and `targets` false: the GamePad's
//     picture samples the targets the TV's call drew (drawing them again would blend the game's drawing into them twice) ...
//     WHBGfxFinishRenderDRC();
//     WHBGfxFinishRender();
//     ...at the end:
//     rac1::gs::draw_gx2_shutdown();
//     WHBGfxShutdown();
//
// The Gs (its primitives and their textures) must not change between the calls of one frame (draw after chain_join()).
// draw_gx2 sets its own state on every call and leaves: `screen` bound as colour buffer 0 (it binds its render targets in
// between), the shader mode back at GX2_SHADER_MODE_UNIFORM_REGISTER, blending off, every channel written, the scissor and
// the viewport at the whole buffer, no depth test, no culling. The depth buffer the caller bound stays bound while a render
// target is (depth test and writes off: not touched).
//
// Not drawn yet (TODO, as gs_draw.cpp does them):
//  - the projected 3D (Primitive::vu): it needs the host's depth (DepthMap) and the 3D renderer under it;
//  - a render target's depth buffer (gs_draw.cpp's render_target_depth): a target's primitives drawn without their Z test
//    (ZTST NEVER: not drawn);
//  - the primitives that read the screen they draw into: DATE (the destination alpha test), the frame sampled as a texture;
//  - the per-pixel mip level: the base level (Primitive::tex, level mip_lo) only;
//  - types 7 (the frame's picture put back, Viewport::screen_picture) and 8 (the host's 3D into a render target);
//  - the depth test, and so the AFAIL split's Z pass (gs_draw.cpp draws two passes only with Z writes on);
//  - the dual-source blend factor (gs_draw.cpp's SRC1_ALPHA in render targets): a target's batch whose blend reads the source
//    alpha is drawn twice instead, the colour then the raw alpha (see flush in draw_gx2): the same result.
#include "gs_draw_gx2.h"

#include <coreinit/memdefaultheap.h>
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
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <vector>

namespace rac1::gs {

namespace {

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

// The vertex as the shader reads it (ports/wiiu/shaders/gs_draw.vert): x, y in clip space at the batch's append (frame
// pixels before), colour as bytes / 255, s t q, the fog byte, the glyph's sampling clamp (gs_draw.cpp's V without its depth)
struct V {
    float x, y;
    float r, g, b, a;
    float s, t, q;
    float f;
    float c0 = 1.0f, c1 = 1.0f, c2 = 0.0f, c3 = 0.0f;  // s0, t0, s1, t1 (s0 > s1: none)
};
static_assert(sizeof(V) == 56);

// The pixel shader's uniform block (gs_draw.frag's GsDraw, std140): GX2 reads it as 32-bit big-endian words, the PPC's own
struct alignas(16) Block {
    float fog_col[4];
    int32_t a[4];  // TME, TFX, TCC, ATE
    int32_t b[4];  // ATST, AREF, AFAIL, FGE
    int32_t c[4];  // the raw GS alpha written (a render target's), 0, 0, 0
    bool operator==(const Block& o) const { return std::memcmp(this, &o, sizeof *this) == 0; }
};
static_assert(sizeof(Block) == 64);

bool g_ready = false;
WHBGfxShaderGroup g_sh{};
uint32_t g_block_loc = 0, g_block_size = sizeof(Block);
uint32_t g_sampler_loc = 0;

// The vertices and uniform blocks of the draws, appended to a ring in MEM2. GX2 reads them when it runs the commands, later:
// before the ring wraps over what it may still read the CPU waits for the GPU (GX2DrawDone; a ring this size wraps every
// few frames of 2D at most)
constexpr uint32_t kRingBytes = 8u << 20;
uint8_t* g_ring = nullptr;
uint32_t g_ring_off = 0;
uint64_t g_ring_wraps = 0;  // (what was allocated before a wrap may be overwritten after it)

void* ring_alloc(uint32_t bytes, uint32_t align) {
    uint32_t off = (g_ring_off + align - 1) & ~(align - 1);
    if (bytes > kRingBytes) return nullptr;
    if (off + bytes > kRingBytes) {
        GX2DrawDone();
        off = 0;
        ++g_ring_wraps;
    }
    g_ring_off = off + bytes;
    return g_ring + off;
}

// The textures, one GX2 texture per decoded image (TexImage::id: an image never changes under its id); a primitive without an
// image (none taken at its kick) is decoded from the local memory as it is now, keyed by TEX0 / TEXA / the memory's generation
// (gs_draw.cpp's texture_for). Evicted least recently used past kTexCap, never one this draw call uses (a batch waiting for
// its draw may name it); the GPU waited for before their memory goes.
constexpr size_t kTexCap = 64u << 20;
struct TexEntry {
    GX2Texture tex{};
    uint32_t bytes = 0;
    uint64_t last_use = 0;
};
using TexKey = std::tuple<uint64_t, uint64_t, uint64_t>;
std::map<TexKey, TexEntry> g_tex;
size_t g_tex_bytes = 0;
uint64_t g_call = 0;
// a white texel bound while a batch has no texture (the shader's sampling may run whatever TME says)
GX2Texture g_white{};

// The image's uint32 texels are r | g << 8 | b << 16 | a << 24: on the big-endian PPC their bytes in memory are a, b, g, r,
// which the GPU's R8G8B8A8 reads as R = a ... A = r. The texture's component map puts them back (no conversion); with
// kSwapOnCpu the texels are byte-swapped as they are copied instead and the map is the identity.
constexpr bool kSwapOnCpu = false;

// a w x h texture of the image's texels (rows of w) in MEM2; false: no memory for it
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
    s.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;  // (no tiling: the rows copied as they are, at the surface's pitch)
    s.swizzle = 0;
    t.viewFirstMip = 0;
    t.viewNumMips = 1;
    t.viewFirstSlice = 0;
    t.viewNumSlices = 1;
    t.compMap = kSwapOnCpu ? GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A)
                           : GX2_COMP_MAP(GX2_SQ_SEL_A, GX2_SQ_SEL_B, GX2_SQ_SEL_G, GX2_SQ_SEL_R);
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&t);
    s.image = MEMAllocFromDefaultHeapEx(s.imageSize, static_cast<int32_t>(s.alignment));
    if (s.image == nullptr) return false;
    auto* dst = static_cast<uint32_t*>(s.image);
    for (int y = 0; y < h; ++y) {
        uint32_t* row = dst + static_cast<size_t>(y) * s.pitch;
        const uint32_t* in = src + static_cast<size_t>(y) * w;
        if (kSwapOnCpu)
            for (int x = 0; x < w; ++x) row[x] = __builtin_bswap32(in[x]);
        else
            std::memcpy(row, in, static_cast<size_t>(w) * 4);
    }
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, s.image, s.imageSize);
    return true;
}

void tex_free(TexEntry& e) {
    if (e.tex.surface.image != nullptr) MEMFreeToDefaultHeap(e.tex.surface.image);
    e.tex.surface.image = nullptr;
    g_tex_bytes -= e.bytes;
    e.bytes = 0;
}

void tex_evict() {
    if (g_tex_bytes <= kTexCap) return;
    std::vector<std::pair<uint64_t, TexKey>> order;
    for (const auto& [k, e] : g_tex)
        if (e.last_use < g_call) order.emplace_back(e.last_use, k);
    std::sort(order.begin(), order.end());
    bool waited = false;
    for (const auto& [use, k] : order) {
        if (g_tex_bytes <= kTexCap * 3 / 4) break;
        if (!waited) {
            GX2DrawDone();  // (the commands already given may still sample it)
            waited = true;
        }
        auto it = g_tex.find(k);
        tex_free(it->second);
        g_tex.erase(it);
    }
}

// the GX2 texture of the primitive's TEX0 (its image when it has one), made now if it is not there
const GX2Texture* texture_of(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img) {
    const TexKey key = img != nullptr ? TexKey{img->id, 0, 0} : TexKey{gs.generation(), tex0 & 0x1fffffffffffffffull, texa | (1ull << 63)};
    auto it = g_tex.find(key);
    if (it != g_tex.end()) {
        it->second.last_use = g_call;
        return &it->second.tex;
    }
    const int tw = 1 << std::min(10u, fld(tex0, 26, 4)), th = 1 << std::min(10u, fld(tex0, 30, 4));
    int w = tw, h = th;
    std::vector<uint32_t> px;
    const uint32_t* src = nullptr;
    if (img != nullptr && img->w > 0 && img->h > 0 && img->px.size() >= static_cast<size_t>(img->w) * img->h) {
        w = img->w;
        h = img->h;
        src = img->px.data();
    } else {
        px.resize(static_cast<size_t>(tw) * th);
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x) px[static_cast<size_t>(y) * tw + x] = gs.texel(tex0, texa, x, y);
        src = px.data();
    }
    TexEntry e;
    if (!make_texture(e.tex, w, h, src)) {
        tex_evict();
        if (!make_texture(e.tex, w, h, src)) return nullptr;
    }
    e.bytes = e.tex.surface.imageSize;
    e.last_use = g_call;
    g_tex_bytes += e.bytes;
    auto& slot = g_tex[key];
    slot = e;
    tex_evict();
    return &g_tex.find(key)->second.tex;
}

// ALPHA as a GX2 blend: mirrors gs_draw.cpp's blend_state (Cv = (A - B) C + D; the factor of Cs and of Cd in that sum,
// k0 + k1 C), FIX as the constant colour (BLEND_FACTOR), the alpha written the source's
struct Blend {
    bool on = false;
    GX2BlendMode sf = GX2_BLEND_MODE_ONE, df = GX2_BLEND_MODE_ZERO;
    GX2BlendCombineMode op = GX2_BLEND_COMBINE_MODE_ADD;
    float fix = 0.0f;
    // a factor is the source alpha (As: in a render target the GS's 0x80 = 1.0, not the raw alpha the target keeps)
    bool src_alpha() const {
        auto sa = [](GX2BlendMode m) { return m == GX2_BLEND_MODE_SRC_ALPHA || m == GX2_BLEND_MODE_INV_SRC_ALPHA; };
        return on && (sa(sf) || sa(df));
    }
};
Blend blend_of(const DrawState& st) {
    Blend b;
    if (!fld(st.prim, 6, 1)) return b;
    b.on = true;
    const uint32_t A = fld(st.alpha, 0, 2), B = fld(st.alpha, 2, 2), C = fld(st.alpha, 4, 2), D = fld(st.alpha, 6, 2);
    b.fix = std::min(static_cast<float>(fld(st.alpha, 32, 8)) / 128.0f, 1.0f);
    const GX2BlendMode cf = C == 2 ? GX2_BLEND_MODE_BLEND_FACTOR : (C == 1 ? GX2_BLEND_MODE_DST_ALPHA : GX2_BLEND_MODE_SRC_ALPHA);
    const GX2BlendMode cf1 = C == 2 ? GX2_BLEND_MODE_INV_BLEND_FACTOR : (C == 1 ? GX2_BLEND_MODE_INV_DST_ALPHA : GX2_BLEND_MODE_INV_SRC_ALPHA);
    auto factor = [&](uint32_t who, GX2BlendMode& f) {
        const int k0 = D == who ? 1 : 0, k1 = (A == who ? 1 : 0) - (B == who ? 1 : 0);
        if (k1 == 0) {
            f = k0 != 0 ? GX2_BLEND_MODE_ONE : GX2_BLEND_MODE_ZERO;
            return 1;
        }
        if (k0 == 0) {
            f = cf;
            return k1;
        }
        f = k1 < 0 ? cf1 : GX2_BLEND_MODE_ONE;  // (1 - C; 1 + C, not a factor: 1)
        return 1;
    };
    const int ss = factor(0, b.sf), ds = factor(1, b.df);
    if (ss >= 0 && ds >= 0) {
        b.op = GX2_BLEND_COMBINE_MODE_ADD;
    } else if (ss < 0 && ds >= 0) {  // Cd df - Cs sf
        b.op = GX2_BLEND_COMBINE_MODE_REV_SUB;
    } else if (ss >= 0) {  // Cs sf - Cd df
        b.op = GX2_BLEND_COMBINE_MODE_SUB;
    } else {  // (-(Cs + Cd) C: below 0, clamped to black)
        b.op = GX2_BLEND_COMBINE_MODE_ADD;
        b.sf = b.df = GX2_BLEND_MODE_ZERO;
    }
    return b;
}
void set_blend(const Blend& b) {
    if (!b.on) {
        GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
        return;
    }
    GX2SetBlendConstantColor(b.fix, b.fix, b.fix, b.fix);
    GX2SetBlendControl(GX2_RENDER_TARGET_0, b.sf, b.df, b.op, TRUE, GX2_BLEND_MODE_ONE, GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 1, FALSE, TRUE);
}

// The GS's drawing into its own memory (a FRAME other than the screen's, sampled afterwards as a texture: the menus' text,
// their render-to-texture), as gs_draw.cpp's RenderTarget / render_target: one colour buffer per frame base page, FBW x 64
// pixels wide (64 at least) and as tall as wide, 512 at least, one GS pixel a pixel, row 0 the GS's row 0 (GX2's top);
// cleared when made, kept across frames (the game may draw into it once and sample it for many). The texture is a view of
// the same surface (the GPU's own byte order: the identity component map).
struct Rt {
    GX2ColorBuffer cb{};
    GX2Texture tex{};
    int w = 0, h = 0;
};
std::map<uint32_t, Rt> g_rt;

void rt_free(Rt& r) {
    if (r.cb.surface.image != nullptr) MEMFreeToDefaultHeap(r.cb.surface.image);
    r = Rt{};
}

// the target of page `fbp`, made (or remade at a new size: the GPU waited for first) when it is not that size; nullptr: no memory
Rt* render_target(uint32_t fbp, int w, int h) {
    Rt& r = g_rt[fbp];
    if (r.cb.surface.image != nullptr && r.w == w && r.h == h) return &r;
    if (r.cb.surface.image != nullptr) {
        GX2DrawDone();  // (the commands given may still draw into it or sample it)
        rt_free(r);
    }
    GX2Surface& s = r.cb.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = static_cast<uint32_t>(w);
    s.height = static_cast<uint32_t>(h);
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
        g_rt.erase(fbp);
        return nullptr;
    }
    // cleared to 0 (transparent black, as gs_draw.cpp's glClear): zero bytes are zero in any tiling
    std::memset(s.image, 0, s.imageSize);
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), s.image,
                  s.imageSize);
    r.tex.surface = s;
    r.tex.viewFirstMip = 0;
    r.tex.viewNumMips = 1;
    r.tex.viewFirstSlice = 0;
    r.tex.viewNumSlices = 1;
    r.tex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2InitTextureRegs(&r.tex);
    r.w = w;
    r.h = h;
    return &r;
}

// (testing, RAC1_GX2_SKIPLOG=1: every 300 calls, why the primitives were not drawn, counted by reason)
enum Skip { kSkFx, kSkType78, kSkFilter, kSkVu, kSkSkipFbp, kSkRtOff, kSkRtMem, kSkZNever, kSkScreenRead, kSkType, kSkSub, kSkNoSample,
            kSkSliver, kSkCrop, kSkScissor, kSkNoTex, kSkN };
const char* const kSkipName[kSkN] = {"fx (MRP's)", "type 7/8", "prim_filter", "vu (3D)", "skip_fbp", "rt (targets off)", "rt (no memory)",
                                     "rt ZTST never", "screen read (DATE / frame as texture)", "type", "sub", "no sample point", "edge sliver",
                                     "crop", "empty scissor", "no texture"};
uint64_t g_skips[kSkN] = {};
uint64_t g_drawn = 0, g_drawn_rt = 0;

}  // namespace

bool draw_gx2_init() {
    if (g_ready) return true;
    std::vector<uint8_t> gsh;
    if (std::FILE* f = std::fopen("shaders/gs_draw.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_sh, 0, gsh.data())) {
        std::printf("[wiiu] gs_draw_gx2: shaders/gs_draw.gsh not loaded\n");
        return false;
    }
    // (the attributes the compiler kept: one it dropped is not found, and is not needed)
    WHBGfxInitShaderAttribute(&g_sh, "aPos", 0, offsetof(V, x), GX2_ATTRIB_FORMAT_FLOAT_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aColor", 0, offsetof(V, r), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aTex", 0, offsetof(V, s), GX2_ATTRIB_FORMAT_FLOAT_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "aFog", 0, offsetof(V, f), GX2_ATTRIB_FORMAT_FLOAT_32);
    WHBGfxInitShaderAttribute(&g_sh, "aClamp", 0, offsetof(V, c0), GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    if (!WHBGfxInitFetchShader(&g_sh)) {
        std::printf("[wiiu] gs_draw_gx2: fetch shader not made\n");
        WHBGfxFreeShaderGroup(&g_sh);
        return false;
    }
    const GX2PixelShader* ps = g_sh.pixelShader;
    bool block = false, sampler = false;
    for (uint32_t i = 0; i < ps->uniformBlockCount; ++i)
        if (std::strcmp(ps->uniformBlocks[i].name, "GsDraw") == 0) {
            g_block_loc = ps->uniformBlocks[i].offset;
            g_block_size = std::max<uint32_t>(ps->uniformBlocks[i].size, sizeof(Block));
            block = true;
        }
    for (uint32_t i = 0; i < ps->samplerVarCount; ++i)
        if (std::strcmp(ps->samplerVars[i].name, "uTexture") == 0) {
            g_sampler_loc = ps->samplerVars[i].location;
            sampler = true;
        }
    g_ring = static_cast<uint8_t*>(MEMAllocFromDefaultHeapEx(kRingBytes, GX2_UNIFORM_BLOCK_ALIGNMENT));
    if (!block || !sampler || g_ring == nullptr) {
        std::printf("[wiiu] gs_draw_gx2: shader block %d sampler %d ring %d\n", block, sampler, g_ring != nullptr);
        if (g_ring != nullptr) MEMFreeToDefaultHeap(g_ring);
        g_ring = nullptr;
        WHBGfxFreeShaderGroup(&g_sh);
        return false;
    }
    const uint32_t white = 0xffffffffu;
    if (!make_texture(g_white, 1, 1, &white)) {
        std::printf("[wiiu] gs_draw_gx2: no memory\n");
        MEMFreeToDefaultHeap(g_ring);
        g_ring = nullptr;
        WHBGfxFreeShaderGroup(&g_sh);
        return false;
    }
    g_ring_off = 0;
    g_ready = true;
    return true;
}

void draw_gx2_shutdown() {
    if (!g_ready) return;
    GX2DrawDone();
    for (auto& [k, e] : g_tex) tex_free(e);
    g_tex.clear();
    for (auto& [k, r] : g_rt) rt_free(r);
    g_rt.clear();
    g_tex_bytes = 0;
    MEMFreeToDefaultHeap(g_white.surface.image);
    g_white = GX2Texture{};
    MEMFreeToDefaultHeap(g_ring);
    g_ring = nullptr;
    WHBGfxFreeShaderGroup(&g_sh);
    g_sh = WHBGfxShaderGroup{};
    g_ready = false;
}

void draw_gx2(const Gs& gs, const GX2ColorBuffer* screen, float FB_W, float FB_H, const Viewport* vp, bool targets, int prim_filter, size_t from,
              size_t to) {
    if (!g_ready || screen == nullptr) return;
    const int win_w = static_cast<int>(screen->surface.width), win_h = static_cast<int>(screen->surface.height);
    const auto& prims = gs.primitives();
    const size_t prim_end = std::min(to, prims.size());
    if (prims.empty() || from >= prim_end || win_w <= 0 || win_h <= 0) return;
    ++g_call;
    Viewport v;
    if (vp != nullptr) v = *vp;
    else { v.w = win_w; v.h = win_h; }

    // GX2's state for the whole call
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_BLOCK);
    GX2SetFetchShader(&g_sh.fetchShader);
    GX2SetVertexShader(g_sh.vertexShader);
    GX2SetPixelShader(g_sh.pixelShader);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    GX2SetLineWidth(1.0f);
    // the target bound: -1 the screen (`screen`, bound by the caller), else a render target's page
    int bound = -1;
    auto set_screen_viewport = [&] {  // (GX2's y from the top, the Viewport's from the bottom)
        GX2SetViewport(static_cast<float>(v.x), static_cast<float>(win_h - v.y - v.h), static_cast<float>(v.w), static_cast<float>(v.h), 0.0f, 1.0f);
    };
    set_screen_viewport();
    // leaving a render target: what was drawn into it written out of the colour cache, the texture cache's view of it dropped
    // (before anything samples it)
    auto leave_target = [&] {
        if (bound < 0) return;
        const auto it = g_rt.find(static_cast<uint32_t>(bound));
        if (it != g_rt.end())
            GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), it->second.cb.surface.image,
                          it->second.cb.surface.imageSize);
    };
    auto bind = [&](int want) {
        if (want == bound) return;
        leave_target();
        if (want >= 0) {
            const Rt& r = g_rt[static_cast<uint32_t>(want)];
            GX2SetColorBuffer(&r.cb, GX2_RENDER_TARGET_0);
            GX2SetViewport(0.0f, 0.0f, static_cast<float>(r.w), static_cast<float>(r.h), 0.0f, 1.0f);
        } else {
            GX2SetColorBuffer(screen, GX2_RENDER_TARGET_0);
            set_screen_viewport();
        }
        bound = want;
    };

    // (gs_draw.cpp) the frame pages drawn into and then sampled (TEX0's base = FRAME's page * 32): render targets; also the pages
    // at or above Viewport::rt_min_fbp. (The frame's own page is the screen even when sampled.)
    std::set<uint32_t> sampled;
    for (const Primitive& p : prims)
        if (fld(p.st.prim, 4, 1) != 0 && fld(p.st.tex0, 20, 6) == 0) sampled.insert(fld(p.st.tex0, 0, 14));

    // Consecutive primitives with the same state are one draw (gs_draw.cpp's Key, less what is not drawn here)
    struct Key {
        int want = -1;  // the target (-1 the screen)
        float sx = 1.0f;
        bool lines = false;
        Block blk{};
        const GX2Texture* tex = nullptr;
        int lin = 0, wrap_s = 0, wrap_t = 0;
        bool blend = false;
        uint64_t alpha = 0;
        uint32_t fbmsk = 0;
        int sc[4] = {0, 0, 0, 0};  // the scissor, GX2's (from the top left of the target)
        bool operator==(const Key& o) const {
            return want == o.want && sx == o.sx && lines == o.lines && blk == o.blk && tex == o.tex && lin == o.lin && wrap_s == o.wrap_s &&
                   wrap_t == o.wrap_t && blend == o.blend && alpha == o.alpha && fbmsk == o.fbmsk && std::memcmp(sc, o.sc, sizeof sc) == 0;
        }
    };
    std::vector<V> batch, verts;
    Key cur;
    DrawState cur_st;  // (the blend's: its ALPHA / PRIM)
    bool have = false;
    const void* last_blk = nullptr;  // the last uniform block written (reused while it holds the same, until the ring wraps)
    uint64_t last_blk_wraps = 0;
    Block last_blk_val{};
    auto set_block = [&](const Block& blk) {
        if (last_blk == nullptr || last_blk_wraps != g_ring_wraps || !(last_blk_val == blk)) {
            void* ub = ring_alloc(g_block_size, GX2_UNIFORM_BLOCK_ALIGNMENT);
            if (ub == nullptr) return false;
            std::memset(ub, 0, g_block_size);
            std::memcpy(ub, &blk, sizeof(Block));
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
    auto flush = [&] {
        if (batch.empty()) return;
        const uint32_t bytes = static_cast<uint32_t>(batch.size() * sizeof(V));
        void* vb = ring_alloc(bytes, GX2_VERTEX_BUFFER_ALIGNMENT);
        if (vb == nullptr) {
            batch.clear();
            return;
        }
        std::memcpy(vb, batch.data(), bytes);
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, vb, bytes);
        bind(cur.want);
        const uint32_t m = cur.fbmsk;
        const uint32_t cm = ((m & 0xffu) != 0xffu ? GX2_CHANNEL_MASK_R : 0) | ((m & 0xff00u) != 0xff00u ? GX2_CHANNEL_MASK_G : 0) |
                            ((m & 0xff0000u) != 0xff0000u ? GX2_CHANNEL_MASK_B : 0) | ((m & 0xff000000u) != 0xff000000u ? GX2_CHANNEL_MASK_A : 0);
        {
            GX2Sampler smp;
            const GX2TexXYFilterMode filt = cur.lin ? GX2_TEX_XY_FILTER_MODE_LINEAR : GX2_TEX_XY_FILTER_MODE_POINT;
            GX2InitSampler(&smp, GX2_TEX_CLAMP_MODE_CLAMP, filt);
            GX2InitSamplerClamping(&smp, cur.wrap_s ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP,
                                   cur.wrap_t ? GX2_TEX_CLAMP_MODE_WRAP : GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_CLAMP_MODE_CLAMP);
            GX2SetPixelTexture(cur.tex != nullptr ? cur.tex : &g_white, g_sampler_loc);
            GX2SetPixelSampler(&smp, g_sampler_loc);
        }
        GX2SetScissor(static_cast<uint32_t>(cur.sc[0]), static_cast<uint32_t>(cur.sc[1]), static_cast<uint32_t>(cur.sc[2]), static_cast<uint32_t>(cur.sc[3]));
        GX2SetAttribBuffer(0, bytes, sizeof(V), vb);
        const GX2PrimitiveMode mode = cur.lines ? GX2_PRIMITIVE_MODE_LINES : GX2_PRIMITIVE_MODE_TRIANGLES;
        const uint32_t count = static_cast<uint32_t>(batch.size());
        const Blend bl = blend_of(cur_st);
        Block blk = cur.blk;
        if (cur.want >= 0 && bl.src_alpha() && (cm & GX2_CHANNEL_MASK_A) != 0) {
            // A render target keeps the GS's raw alpha (0x80 = 128 / 255) while its blend reads the source alpha as the GS's
            // (0x80 = 1.0): gs_draw.cpp writes the raw alpha and blends with the second output (SRC1_ALPHA, dual-source), which
            // GX2 has no shader output for here. So two passes of the same pixels (the same alpha test and discards): the colour
            // channels blended with the source alpha as 0x80 = 1.0 (the alpha channel masked), then the alpha channel alone,
            // unblended, the raw alpha (gs_draw.cpp's alpha blend is ONE / ZERO: the source's alpha, the same result).
            blk.c[0] = 0;
            if (set_block(blk)) {
                set_mask(cm & ~static_cast<uint32_t>(GX2_CHANNEL_MASK_A));
                set_blend(bl);
                GX2DrawEx(mode, count, 0, 1);
            }
            blk.c[0] = 1;
            if (set_block(blk)) {
                set_mask(GX2_CHANNEL_MASK_A);
                set_blend(Blend{});
                GX2DrawEx(mode, count, 0, 1);
            }
        } else {
            // the screen: the alpha as 0x80 = 1.0; a render target: the raw alpha (no factor reads the source alpha, or the alpha
            // channel is not written: the source alpha as 0x80 = 1.0 then)
            blk.c[0] = cur.want >= 0 && !bl.src_alpha() ? 1 : 0;
            if (set_block(blk)) {
                set_mask(cm);
                set_blend(bl);
                GX2DrawEx(mode, count, 0, 1);
            }
        }
        batch.clear();
    };
    static const bool s_fog = std::getenv("RAC1_GS_FOG") != nullptr;  // (gs_draw.cpp: the GS's fogging, off by default)
    static const bool s_skiplog = std::getenv("RAC1_GX2_SKIPLOG") != nullptr && std::getenv("RAC1_GX2_SKIPLOG")[0] == '1';
    auto skip = [](Skip r) { ++g_skips[r]; };

    for (size_t pi = from; pi < prim_end; ++pi) {
        const Primitive& p = prims[pi];
        if (p.fx && gs.fx_external()) { skip(kSkFx); continue; }  // (the MRP's)
        if (p.type == 7 || p.type == 8) { skip(kSkType78); continue; }  // TODO: the frame's picture put back, the host's 3D into a target
        if ((prim_filter == 1 && !p.moby) || (prim_filter == 2 && p.moby)) { skip(kSkFilter); continue; }
        if (p.vu) { skip(kSkVu); continue; }  // TODO: the projected 3D (needs the host's depth)
        const DrawState& st = p.st;
        const bool tme = fld(st.prim, 4, 1) != 0, fst = fld(st.prim, 8, 1) != 0;
        const uint32_t ztst = fld(st.test, 17, 2);
        const uint32_t fbp = fld(st.frame, 0, 9);
        if (v.skip_fbp >= 0 && static_cast<int>(fbp) == v.skip_fbp) { skip(kSkSkipFbp); continue; }
        // (gs_draw.cpp) a render target: a page other than the frame's that this frame samples, or at / above rt_min_fbp
        const bool to_rt = static_cast<int>(fbp) != v.screen_fbp &&
                           (sampled.count(fbp * 32) != 0 || (v.rt_min_fbp >= 0 && static_cast<int>(fbp) >= v.rt_min_fbp));
        const uint32_t tbp = fld(st.tex0, 0, 14);
        const bool scr_tex = tme && !to_rt && v.screen_fbp >= 0 && tbp == static_cast<uint32_t>(v.screen_fbp) * 32 && fld(st.tex0, 20, 6) == 0;
        const bool date = !to_rt && fld(st.test, 14, 1) != 0;
        if (scr_tex || date) { skip(kSkScreenRead); continue; }  // TODO: the primitives reading the screen (a copy of it around them)
        // (gs_draw.cpp's rule for a substituted range, which needs no host depth) the game's projected 3D the GIF path draws
        // itself, Z-tested at a depth inside the view (effects, glass): not the 2D, which it draws Z-tested at Z 0xfffff0
        if (!to_rt && fld(st.test, 16, 1) != 0 && ztst >= 2) {
            bool p3d = false;
            for (const Vertex& q : p.v) p3d = p3d || (q.z != 0 && q.z < 0xff0000u);
            if (p3d) { skip(kSkVu); continue; }  // (counted with the VU's 3D)
        }
        if (p.type != 6 && p.type != 3 && p.type != 1) { skip(kSkType); continue; }
        if (p.v.size() < (p.type == 3 ? 3u : 2u)) { skip(kSkType); continue; }
        int rw = 0, rh = 0;
        if (to_rt) {
            // (drawn by the first call of a frame only: the second, the GamePad's, samples what the first drew)
            if (!targets) { skip(kSkRtOff); continue; }
            // gs_draw.cpp tests a target's Z on its own depth buffer; none here (TODO): NEVER draws nothing there, the rest all
            if (fld(st.test, 16, 1) != 0 && ztst == 0) { skip(kSkZNever); continue; }
            rw = std::max(64, static_cast<int>(fld(st.frame, 16, 6)) * 64);
            rh = std::max(rw, 512);
            const auto it = g_rt.find(fbp);
            if (it == g_rt.end() || it->second.w != rw || it->second.h != rh) {
                // (made or remade: what waits for its draw is drawn first, it may sample or draw into the old one)
                flush();
                have = false;
                if (bound == static_cast<int>(fbp)) bind(-1);
                if (render_target(fbp, rw, rh) == nullptr) { skip(kSkRtMem); continue; }
            }
        }
        const float tw = static_cast<float>(1 << std::min(10u, fld(st.tex0, 26, 4))), th = static_cast<float>(1 << std::min(10u, fld(st.tex0, 30, 4)));
        // (gs_draw.cpp) sampling a render target this frame samples (its page's base): its texture, in its own texel units
        const auto src_rt = tme ? g_rt.find(tbp / 32) : g_rt.end();
        const bool from_rt = tme && tbp % 32 == 0 && src_rt != g_rt.end() && sampled.count(tbp) != 0;
        const float sw = from_rt ? static_cast<float>(src_rt->second.w) : tw, sh = from_rt ? static_cast<float>(src_rt->second.h) : th;
        auto mk = [&](const Vertex& q) {
            V o{q.x, q.y, q.r / 255.0f, q.g / 255.0f, q.b / 255.0f, q.a / 255.0f, 0, 0, 1, static_cast<float>(q.fog)};
            if (fst) { o.s = q.u / sw; o.t = q.v / sh; o.q = 1.0f; }
            else { o.s = q.s * tw / sw; o.t = q.t * th / sh; o.q = q.q != 0.0f ? q.q : 1.0f; }
            return o;
        };
        // (gs_draw.cpp) the overscan crop; no projected 3D here (proj3d / crop_3d false: the Z-tested 2D is 2D without a depth map)
        const bool crop_on = !to_rt && (v.crop[0] | v.crop[1] | v.crop[2] | v.crop[3]) != 0;
        if (p.sub != 0) {  // (a substituted range, see Primitive::sub: the substitute's projected 3D, the real range's 2D)
            bool sub3d = false;
            if (!to_rt && fld(st.test, 16, 1) != 0 && ztst >= 2)
                for (const Vertex& q : p.v) sub3d = sub3d || (q.z != 0 && q.z < 0xff0000u);
            if ((p.sub == 1) != sub3d) { skip(kSkSub); continue; }
        }
        const float sx = to_rt ? 1.0f : v.xscale;
        verts.clear();
        if (p.type == 6) {
            const V a = mk(p.v[0]), b = mk(p.v[1]);
            V c0 = b, c1 = b, c2 = b, c3 = b;  // sprite: the second vertex's colour
            c0.x = a.x; c0.y = a.y; c0.s = a.s / a.q * b.q; c0.t = a.t / a.q * b.q;
            c1.y = a.y; c1.t = c0.t;
            c2.x = a.x; c2.s = c0.s;
            verts = {c0, c1, c2, c1, c3, c2};
        } else if (p.type == 3) {
            verts = {mk(p.v[0]), mk(p.v[1]), mk(p.v[2])};
        } else {
            verts = {mk(p.v[0]), mk(p.v[1])};
        }
        // (gs_draw.cpp, Viewport::sprite_clamp) a glyph's bilinear sampling kept inside its own cell of the font's texture
        if (v.sprite_clamp && tme && p.mip_n == 0 && (p.type == 6 || (p.type == 3 && fst))) {
            bool rect = p.type == 6;
            if (!rect) {
                const auto& q = p.v;
                float bx0 = q[0].x, bx1 = bx0, by0 = q[0].y, by1 = by0;
                for (int i = 1; i < 3; ++i) {
                    bx0 = std::min(bx0, q[i].x);
                    bx1 = std::max(bx1, q[i].x);
                    by0 = std::min(by0, q[i].y);
                    by1 = std::max(by1, q[i].y);
                }
                rect = bx1 > bx0 && by1 > by0;
                for (int i = 0; i < 3 && rect; ++i) {
                    rect = (q[i].x == bx0 || q[i].x == bx1) && (q[i].y == by0 || q[i].y == by1);
                    for (int j = i + 1; j < 3 && rect; ++j) rect = (q[i].x == q[j].x) == (q[i].u == q[j].u) && (q[i].y == q[j].y) == (q[i].v == q[j].v);
                }
            }
            float s0 = 1e30f, s1 = -1e30f, t0 = 1e30f, t1 = -1e30f;
            for (const V& q : verts) {
                s0 = std::min(s0, q.s / q.q * sw);
                s1 = std::max(s1, q.s / q.q * sw);
                t0 = std::min(t0, q.t / q.q * sh);
                t1 = std::max(t1, q.t / q.q * sh);
            }
            auto whole = [](float x) { return std::abs(x - std::round(x)) < 1.0f / 32.0f; };
            const float cw = s1 - s0, ch = t1 - t0;
            rect = rect && whole(s0) && whole(t0) && std::abs(ch - 16.0f) < 1.0f / 32.0f &&
                   (std::abs(cw - 16.0f) < 1.0f / 32.0f || std::abs(cw - 24.0f) < 1.0f / 32.0f);
            if (rect) {
                const float sl = (std::round(s0) + 0.5f) / sw, sr = (std::round(s1) - 0.5f) / sw;
                const float tl = (std::round(t0) + 0.5f) / sh, tr = (std::round(t1) - 0.5f) / sh;
                for (V& q : verts) {
                    q.c0 = sl;
                    q.c1 = tl;
                    q.c2 = sr;
                    q.c3 = tr;
                }
            }
        }
        // (gs_draw.cpp) a 2D sprite / triangle covering none of the GS's sample points draws nothing
        if (!p.wide && (p.type == 6 || p.type == 3)) {
            float bx0 = p.v[0].x, bx1 = bx0, by0 = p.v[0].y, by1 = by0;
            for (size_t i = 1; i < (p.type == 6 ? 2u : 3u); ++i) {
                bx0 = std::min(bx0, p.v[i].x);
                bx1 = std::max(bx1, p.v[i].x);
                by0 = std::min(by0, p.v[i].y);
                by1 = std::max(by1, p.v[i].y);
            }
            bx0 = std::max(bx0, static_cast<float>(fld(st.scissor, 0, 11)));
            bx1 = std::min(bx1, static_cast<float>(fld(st.scissor, 16, 11) + 1));
            by0 = std::max(by0, static_cast<float>(fld(st.scissor, 32, 11)));
            by1 = std::min(by1, static_cast<float>(fld(st.scissor, 48, 11) + 1));
            if (std::ceil(bx0) >= bx1 || std::ceil(by0) >= by1) { skip(kSkNoSample); continue; }
        }
        // (gs_draw.cpp) the 2D over the frame's whole width or height (a fade, a darkening): to the window's edges
        bool to_left = false, to_right = false, full_span = false, edge_fill = false;
        if (!to_rt && p.layer && sx < 1.0f) {
            for (V& q : verts) q.x = FB_W * 0.5f + (q.x - FB_W * 0.5f) / sx;
            to_left = to_right = full_span = true;
        } else if (!to_rt && (sx < 1.0f || crop_on)) {
            float x0 = verts[0].x, x1 = x0, y0 = verts[0].y, y1 = y0;
            for (const V& q : verts) {
                x0 = std::min(x0, q.x);
                x1 = std::max(x1, q.x);
                y0 = std::min(y0, q.y);
                y1 = std::max(y1, q.y);
            }
            const float far_x = tme ? FB_W : FB_W - 0.5f, far_y = tme ? FB_H : FB_H - 0.5f;
            full_span = (x0 <= 0.0f && x1 >= far_x) || (y0 <= 0.0f && y1 >= far_y);
            if (crop_on && !tme && (x0 <= 0.0f || x1 >= FB_W || y0 <= 0.0f || y1 >= FB_H)) edge_fill = true;
            if (full_span && sx < 1.0f) {
                to_left = x0 <= 0.0f;
                to_right = x1 >= far_x;
                for (V& q : verts) {
                    if (q.x <= 0.0f) q.x = FB_W * 0.5f * (1.0f - 1.0f / sx);
                    else if (q.x >= far_x) q.x = FB_W * 0.5f * (1.0f + 1.0f / sx);
                }
            }
        }
        Key k;
        k.want = to_rt ? static_cast<int>(fbp) : -1;
        k.sx = sx;
        k.lines = p.type == 1;
        const int fge = s_fog && fld(st.prim, 5, 1) != 0 ? 1 : 0;
        const uint32_t fogcol = fge != 0 ? static_cast<uint32_t>(st.fogcol & 0xffffffu) : 0;
        k.blk.fog_col[0] = static_cast<float>(fogcol & 0xff) / 255.0f;
        k.blk.fog_col[1] = static_cast<float>((fogcol >> 8) & 0xff) / 255.0f;
        k.blk.fog_col[2] = static_cast<float>((fogcol >> 16) & 0xff) / 255.0f;
        k.blk.fog_col[3] = 0.0f;
        k.blk.a[0] = tme ? 1 : 0;
        k.blk.a[1] = static_cast<int32_t>(fld(st.tex0, 35, 2));
        k.blk.a[2] = static_cast<int32_t>(fld(st.tex0, 34, 1));
        k.blk.a[3] = static_cast<int32_t>(fld(st.test, 0, 1));
        k.blk.b[0] = static_cast<int32_t>(fld(st.test, 1, 3));
        k.blk.b[1] = static_cast<int32_t>(fld(st.test, 4, 8));
        k.blk.b[2] = static_cast<int32_t>(fld(st.test, 12, 2));
        k.blk.b[3] = fge;
        k.fbmsk = fld(st.frame, 32, 32);
        if (tme) {
            if (from_rt) k.tex = &src_rt->second.tex;
            else k.tex = texture_of(gs, st.tex0, st.texa, p.tex.get());  // TODO: the per-pixel mip level (the base level here)
            if (k.tex == nullptr) { skip(kSkNoTex); continue; }
            k.lin = fld(st.tex1, 5, 1) != 0 ? 1 : 0;
            k.wrap_s = fld(st.clamp, 0, 2) == 0 ? 1 : 0;
            k.wrap_t = fld(st.clamp, 2, 2) == 0 ? 1 : 0;
        }
        k.blend = fld(st.prim, 6, 1) != 0;
        k.alpha = k.blend ? st.alpha : 0;
        int x0 = static_cast<int>(fld(st.scissor, 0, 11)), x1 = static_cast<int>(fld(st.scissor, 16, 11));
        int y0 = static_cast<int>(fld(st.scissor, 32, 11)), y1 = static_cast<int>(fld(st.scissor, 48, 11));
        // (gs_draw.cpp, Viewport::hide_edge_slivers) the 2D slivers at the frame's edges
        if (v.hide_edge_slivers && !to_rt && tme && !p.wide && !(p.layer && full_span)) {
            float bx0 = verts[0].x, bx1 = bx0, by0 = verts[0].y, by1 = by0;
            for (const V& q : verts) {
                bx0 = std::min(bx0, q.x);
                bx1 = std::max(bx1, q.x);
                by0 = std::min(by0, q.y);
                by1 = std::max(by1, q.y);
            }
            constexpr float kSliver = 4.0f;
            if ((bx1 > FB_W + 0.5f && bx0 >= FB_W - kSliver) || (bx0 < -0.5f && bx1 <= kSliver) || (by0 < -0.5f && by1 <= kSliver)) { skip(kSkSliver); continue; }
        }
        // (gs_draw.cpp) the overscan crop: the 2D's scissor narrowed to the area a TV showed
        if (crop_on && !p.wide && !full_span && !edge_fill) {
            const int cw = static_cast<int>(FB_W), ch = static_cast<int>(FB_H);
            x0 = std::max(x0, v.crop[0]);
            x1 = std::min(x1, cw - 1 - v.crop[1]);
            y0 = std::max(y0, v.crop[2]);
            y1 = std::min(y1, ch - 1 - v.crop[3]);
            if (x1 < x0 || y1 < y0) { skip(kSkCrop); continue; }
        }
        // (gs_draw.cpp, Viewport::hud_shift) a HUD element at the frame's left / right side moved toward the window's edges
        float hud_dx = 0.0f;
        if (!to_rt && v.hud_shift != 0.0f && p.hud != 0 && !p.wide && !p.layer && !full_span) {
            const float kk = sx * static_cast<float>(v.w) / FB_W;
            hud_dx = std::round(v.hud_shift * kk) / kk * (p.hud == 1 ? -1.0f : 1.0f);
            for (V& q : verts) q.x += hud_dx;
        }
        if (to_rt) {
            // a target's scissor: its GS pixels (row 0 GX2's top, as the target's row 0 is)
            const int gx0 = std::clamp(x0, 0, rw), gx1 = std::clamp(x1 + 1, 0, rw);
            const int gy0 = std::clamp(y0, 0, rh), gy1 = std::clamp(y1 + 1, 0, rh);
            if (gx1 <= gx0 || gy1 <= gy0) { skip(kSkScissor); continue; }
            k.sc[0] = gx0;
            k.sc[1] = gy0;
            k.sc[2] = gx1 - gx0;
            k.sc[3] = gy1 - gy0;
        } else {
            // the scissor in window pixels from the bottom left (gs_draw.cpp's), then GX2's from the top left inside the buffer
            int sc[4];
            const float sy = static_cast<float>(v.h) / FB_H;
            if (hud_dx != 0.0f) {
                auto wxu = [&](float x) { return static_cast<float>(v.x) + ((x / FB_W * 2.0f - 1.0f) * sx + 1.0f) * 0.5f * static_cast<float>(v.w); };
                const int sx0 = std::clamp(static_cast<int>(wxu(static_cast<float>(x0) + hud_dx)), v.x, v.x + v.w);
                const int sx1 = std::clamp(static_cast<int>(wxu(static_cast<float>(x1 + 1) + hud_dx)), v.x, v.x + v.w);
                sc[0] = sx0;
                sc[2] = std::max(0, sx1 - sx0);
            } else {
                auto wx = [&](float x) { return static_cast<float>(v.x) + ((std::clamp(x, 0.0f, FB_W) / FB_W * 2.0f - 1.0f) * sx + 1.0f) * 0.5f * static_cast<float>(v.w); };
                const bool wide = p.wide && sx < 1.0f && !v.cut_3d;
                const int sx0 = (to_left || wide) && x0 <= 0 ? v.x : static_cast<int>(wx(static_cast<float>(x0)));
                const int sx1 = (to_right || wide) && static_cast<float>(x1 + 1) >= FB_W ? v.x + v.w : static_cast<int>(wx(static_cast<float>(x1 + 1)));
                sc[0] = sx0;
                sc[2] = sx1 - sx0;
            }
            sc[1] = static_cast<int>(static_cast<float>(v.y) + static_cast<float>(v.h) - static_cast<float>(y1 + 1) * sy);
            sc[3] = static_cast<int>(static_cast<float>(y1 - y0 + 1) * sy);
            const int gx0 = std::clamp(sc[0], 0, win_w), gx1 = std::clamp(sc[0] + sc[2], 0, win_w);
            const int top = win_h - (sc[1] + sc[3]);
            const int gy0 = std::clamp(top, 0, win_h), gy1 = std::clamp(top + sc[3], 0, win_h);
            if (gx1 <= gx0 || gy1 <= gy0) { skip(kSkScissor); continue; }  // (an empty scissor: nothing drawn, as with GL's)
            k.sc[0] = gx0;
            k.sc[1] = gy0;
            k.sc[2] = gx1 - gx0;
            k.sc[3] = gy1 - gy0;
        }
        // the vertices to clip space (gs_draw.cpp's VS): the screen's x about the frame's centre by sx, y down; a target's one GS
        // pixel a pixel, its row 0 at the top (GX2's row 0 of the surface, the texture's row 0)
        const float fw = to_rt ? static_cast<float>(rw) : FB_W, fh = to_rt ? static_cast<float>(rh) : FB_H;
        for (V& q : verts) {
            q.x = (q.x / fw * 2.0f - 1.0f) * sx;
            q.y = 1.0f - q.y / fh * 2.0f;
        }
        if (!have || !(k == cur)) {
            flush();
            cur = k;
            cur_st = st;
            have = true;
        }
        batch.insert(batch.end(), verts.begin(), verts.end());
        ++(to_rt ? g_drawn_rt : g_drawn);
    }
    flush();
    if (bound != -1) bind(-1);  // (the target left: its drawing written out for the next call's sampling)

    if (s_skiplog && g_call % 300 == 0) {
        std::printf("[gx2-2d] last 300 calls: %llu drawn on the screen, %llu into targets (%zu targets); not drawn:",
                    static_cast<unsigned long long>(g_drawn), static_cast<unsigned long long>(g_drawn_rt), g_rt.size());
        for (int r = 0; r < kSkN; ++r)
            if (g_skips[r] != 0) std::printf(" %s %llu,", kSkipName[r], static_cast<unsigned long long>(g_skips[r]));
        std::printf("\n");
        std::memset(g_skips, 0, sizeof g_skips);
        g_drawn = g_drawn_rt = 0;
    }

    // the state left as the header says
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA,
                             GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA, GX2_CHANNEL_MASK_RGBA);
    GX2SetScissor(0, 0, static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h));
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(win_w), static_cast<float>(win_h), 0.0f, 1.0f);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
}

}  // namespace rac1::gs
