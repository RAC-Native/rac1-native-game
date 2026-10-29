// The MRP 2D's boundary between its CPU build and a GPU backend. Host code.
//
// mrp_2d.cpp builds, with no GPU, a frame's 2D from the chain reader's primitives (mrp_chain.h): the vertices in frame pixels (or a
// render target's GS pixels) and the runs of primitives drawn with one state, in the chain's order (Built: its Items). Its OpenGL
// and Vulkan submits are inline in that file; the Wii U's GX2 one (ports/wiiu/game/mrp_2d_gx2.cpp) takes the same build through
// mrp2d::build below.
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_moby_backend.inc.)
#ifndef RAC1_GAME_RENDER_MRP_2D_BACKEND_INC
#define RAC1_GAME_RENDER_MRP_2D_BACKEND_INC

#include <game/render/mrp_chain.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_types.h>

#include <cstddef>
#include <cstdint>
#include <memory>
#include <vector>

namespace rac1::mrp2d {

// A vertex as the build makes it: x, y in the frame's pixels (a render target's: its GS pixels), colour as bytes / 255, s t q, the
// window depth (0.5 without depth), the fog byte, a glyph's sampling clamp
struct V {
    float x, y;
    float r, g, b, a;
    float s, t, q;
    float d;  // window depth (0.5 without depth)
    float f;  // the vertex's fog byte
    float c0 = 1.0f, c1 = 1.0f, c2 = 0.0f, c3 = 0.0f;  // a glyph's sampling clamp (s0, t0, s1, t1; s0 > s1: none)
};
static_assert(sizeof(V) == 15 * sizeof(float));

// A texture a run samples: a decoded image (its id the GPU texture's key; `own` the primitive's hold on it, for the GPU texture to
// know when the image is gone) or a render target (its frame page)
struct TexRef {
    const gs::TexImage* img = nullptr;
    const std::shared_ptr<const gs::TexImage>* own = nullptr;
    int rt = -1;
    bool operator==(const TexRef& o) const { return img == o.img && rt == o.rt; }
};

// Everything a run's draw sets
struct Key {
    int want = -1;  // the target: -1 the screen, else a render target's frame page
    bool to_rt = false;
    float sx = 1.0f;
    bool lines = false;
    int fge = 0;
    uint32_t fogcol = 0;
    int tme = 0, tfx = 0, tcc = 0, ate = 0, atst = 0, aref = 0, afail = 0, datm = 0, scr = 0, dt = 0;
    uint32_t fbmsk = 0;
    TexRef tex;
    int lin = 0, wrap_s = 0, wrap_t = 0;
    int mip_n = 0, mip_lo = 0, lcm = 0, mxl = 0, lin_min = 0;
    float mip_k = 0.0f, mip_l = 0.0f;
    TexRef mip[3];
    bool blend = false;
    uint64_t alpha = 0;
    bool ztest = false, zwrite = false;
    int ztst = 0;
    int sc[4] = {0, 0, 0, 0};  // the scissor: a render target's in its GS pixels (x, y from its row 0, w, h), the screen's in window pixels from the bottom left
    bool operator==(const Key&) const = default;
};

// A step of the frame's 2D in its order
struct Item {
    enum Kind { kDraw, kPicture, kHost3d } kind = kDraw;
    Key key;
    uint32_t first = 0, count = 0;  // its vertices
    bool alone = false;             // it reads the screen under it (DATE / the frame sampled): drawn alone, after a copy of its box
    int copy[4] = {0, 0, 0, 0};     // that box (window pixels x0, y0, x1, y1, from the bottom left)
    // (the window position its texture coordinates name: x = [0] + [1] s, y from the bottom = [2] + [3] t)
    float scr_map[4] = {0, 0, 0, 0};
    // a render target the draw needs (created / resized - which clears it - before it), its size, its depth buffer
    int rt_w = 0, rt_h = 0;
    bool rt_depth = false;
    const gs::Primitive* p = nullptr;  // kPicture / kHost3d: the primitive
    bool host_first = false;           // kHost3d: the first of a run into its target (its depth cleared)
};

// Why the build left a primitive out (the backends' logs)
enum Skip {
    kSkFx,        // an effect's (mrp_fx draws it)
    kSkPicture,   // type 7 without a Viewport::screen_picture
    kSkHost3d,    // type 8 without a Viewport::host_3d, or into a page not sampled
    kSkType,      // a point, or a type not drawn
    kSkSkipFbp,   // Viewport::skip_fbp's page
    kSkSub,       // the other half of a substituted range
    kSkNoSample,  // covers none of the GS's sample points
    kSkSliver,    // an edge sliver (Viewport::hide_edge_slivers)
    kSkCrop,      // the overscan crop
    kSkGif3d,     // the game's projected 3D on the GIF path (BuildOptions::drop_gif_3d)
    kSkN
};

struct Built {
    std::vector<V> verts;
    std::vector<Item> items;
    uint32_t prims = 0;           // the primitives [from, to) looked at
    uint32_t drawn = 0;           // those in an Item
    uint32_t skipped[kSkN] = {};  // those left out, by reason
};

struct BuildOptions {
    // the game's projected 3D the GIF path draws itself, Z-tested at a depth inside the view (with no host depth to test it
    // against - the Wii U's, whose 3D effects are not drawn yet -: left out, as the GS replica's GX2 backend does; else drawn as 2D)
    bool drop_gif_3d = false;
};

// orig: (host) the primitives [from, to) of `f` that are not the effects' as one frame's vertices and runs (mrp_2d_draw's CPU half),
// for a backend that draws `f` onto the viewport `v` of the win_w x win_h window (depth: as mrp_2d_draw's)
void build(const MrpChainFrame& f, int win_w, int win_h, float fb_w, float fb_h, const gs::DepthMap* depth, const gs::Viewport& v, size_t from, size_t to, Built& out,
           const BuildOptions& opt = BuildOptions{});
// orig: (host) the render targets' sizes as the backend holds them (the build follows the frame's draws into them in order: a
// target sampled before this frame draws into it is sampled at the size it has): each target the backend makes or resizes noted;
// cleared when the backend lets its targets go
void rt_size_note(uint32_t fbp, int w, int h);
void rt_sizes_clear();

}  // namespace rac1::mrp2d

#if defined(__WIIU__)
namespace rac1 {
// ports/wiiu/game/mrp_2d_gx2.cpp: what mrp_2d_clear_caches / mrp_2d_texture_count do on the Wii U (its GX2 textures)
size_t mrp_2d_gx2_clear_caches();
size_t mrp_2d_gx2_texture_count();
}  // namespace rac1
#endif

#endif
