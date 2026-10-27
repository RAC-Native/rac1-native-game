// See mrp_2d.h.
#include <game/render/mrp_2d.h>

#include <profiler/host_profiler.h>
#include <runtime/gs/gs_blend.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <set>
#include <tuple>
#include <unordered_map>
#include <vector>

namespace rac1 {

namespace {

using gs::DepthMap;
using gs::DrawState;
using gs::Primitive;
using gs::TexImage;
using gs::Vertex;
using gs::Viewport;

uint32_t field(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

// ---- the shaders (OpenGL; Vulkan's are the same, made by tools/gen_vk_shaders.py: kMrp2dVs / kMrp2dFs) ----

const char* VS = R"(
#version 330 core
layout (location = 0) in vec2 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec3 aTex;
layout (location = 3) in float aDepth;
layout (location = 4) in float aFog;
layout (location = 5) in vec4 aClamp;
uniform vec2 uFb;
uniform int uRt;
uniform float uScaleX;
out vec4 vColor;
out vec3 vTex;
out float vFog;
flat out vec4 vClamp;
// orig: (host)
void main() {
    vColor = aColor;
    vFog = aFog;
    vClamp = aClamp;
    vTex = aTex;
    // a render target: its row 0 at the texture's row 0 (sampled later)
    float y = uRt != 0 ? aPos.y / uFb.y * 2.0 - 1.0 : 1.0 - aPos.y / uFb.y * 2.0;
    gl_Position = vec4((aPos.x / uFb.x * 2.0 - 1.0) * uScaleX, y, aDepth * 2.0 - 1.0, 1.0);
}
)";

// the GS's texture functions (colours as bytes / 255, 0x80 = 1.0), alpha test, fogging
const char* FS = R"(
#version 330 core
in vec4 vColor;
in vec3 vTex;
in float vFog;
// a glyph's sampling kept inside its own cell: s, t clamped to xy..zw (x > z: none)
flat in vec4 vClamp;
uniform sampler2D uTexture;
uniform int uFge;
uniform vec3 uFogCol;
uniform int uTme, uTfx, uTcc, uAte, uAtst, uAref, uAfail, uRt;
// a run whose alpha test's fail mode writes one of colour / Z only with Z writes on is drawn twice: 0 the colour, 1 the Z
uniform int uPass;
// the frame itself (a copy of the screen around the primitive): sampled as its texture (uScrTex, window position = uScrMap.xz +
// uScrMap.yw * texture coordinates) and for the destination alpha test (uDate, DATM uDatm)
uniform sampler2D uScreen;
uniform int uScrTex, uDate, uDatm;
// mipmapping: the level per pixel from the interpolated Q (TEX1 LCM / L / K, MXL), level uMipLo on uTexture, the next uMipN levels
// on uMip1..3
uniform sampler2D uMip1, uMip2, uMip3;
uniform int uMipN, uMipLo, uMipLcm, uMipMxl;
uniform float uMipK, uMipL;
uniform vec4 uScrMap;
layout(location = 0, index = 0) out vec4 FragColor;
// the blend's source alpha, the GS's 0x80 = 1.0 (a render target keeps the raw alpha in FragColor: its blend reads this one)
layout(location = 0, index = 1) out vec4 FragFactor;
// orig: (host)
void main() {
    if (uDate != 0) {
        bool b7 = texelFetch(uScreen, ivec2(gl_FragCoord.xy), 0).a > 254.0 / 255.0;
        if ((uDatm == 0) == b7) discard;
    }
    vec4 c = vColor;
    vec4 o = c;
    if (uTme != 0) {
        vec2 st = vTex.xy / vTex.z;
        vec4 t;
        if (uScrTex != 0) {
            t = texelFetch(uScreen, ivec2(uScrMap.x + uScrMap.y * st.x, uScrMap.z + uScrMap.w * st.y), 0);
            t.a = t.a * (128.0 / 255.0);
        } else {
            int idx = 0;
            if (uMipN > 0) {
                float lod = uMipLcm != 0 ? uMipK : log2(1.0 / max(abs(vTex.z), 1e-20)) * uMipL + uMipK;
                int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), uMipMxl);
                idx = clamp(level - uMipLo, 0, uMipN);
            }
            if (vClamp.x <= vClamp.z) st = clamp(st, vClamp.xy, vClamp.zw);
            if (idx == 0) t = texture(uTexture, st);
            else if (idx == 1) t = texture(uMip1, st);
            else if (idx == 2) t = texture(uMip2, st);
            else t = texture(uMip3, st);
        }
        if (uTfx == 0) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = uTcc != 0 ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (uTfx == 1) { o.rgb = t.rgb; o.a = uTcc != 0 ? t.a : c.a; }
        else if (uTfx == 2) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a : c.a; }
    }
    o = clamp(o, 0.0, 1.0);
    if (uFge != 0) {
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + uFogCol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (uAte != 0) {
        bool pass = true;
        if (uAtst == 0) pass = false;
        else if (uAtst == 2) pass = a < float(uAref);
        else if (uAtst == 3) pass = a <= float(uAref);
        else if (uAtst == 4) pass = abs(a - float(uAref)) < 0.5;
        else if (uAtst == 5) pass = a >= float(uAref);
        else if (uAtst == 6) pass = a > float(uAref);
        else if (uAtst == 7) pass = abs(a - float(uAref)) >= 0.5;
        if (!pass && uPass == 0 && (uAfail == 0 || uAfail == 2)) discard;
        if (!pass && uPass == 1 && uAfail != 2) discard;
    }
    FragColor = uRt != 0 ? o : vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
    FragFactor = vec4(min(o.a * (255.0 / 128.0), 1.0));
}
)";

// ---- the frame's vertices and runs, built once for both backends ----

struct V {
    float x, y;
    float r, g, b, a;
    float s, t, q;
    float d;  // window depth (0.5 without depth)
    float f;  // the vertex's fog byte
    float c0 = 1.0f, c1 = 1.0f, c2 = 0.0f, c3 = 0.0f;  // a glyph's sampling clamp (s0, t0, s1, t1; s0 > s1: none)
};
static_assert(sizeof(V) == 15 * sizeof(float));

// A texture a run samples: a decoded image (its id the GPU texture's key) or a render target (its frame page)
struct TexRef {
    const TexImage* img = nullptr;
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
    int sc[4] = {0, 0, 0, 0};
    bool operator==(const Key&) const = default;
};

// A step of the frame's 2D in its order
struct Item {
    enum Kind { kDraw, kPicture, kHost3d } kind = kDraw;
    Key key;
    uint32_t first = 0, count = 0;  // its vertices
    bool alone = false;             // it reads the screen under it (DATE / the frame sampled): drawn alone, after a copy of its box
    int copy[4] = {0, 0, 0, 0};     // that box (window pixels x0, y0, x1, y1)
    float scr_map[4] = {0, 0, 0, 0};
    // a render target the draw needs (created / resized - which clears it - before it), its size, its depth buffer
    int rt_w = 0, rt_h = 0;
    bool rt_depth = false;
    const Primitive* p = nullptr;  // kPicture / kHost3d: the primitive
    bool host_first = false;       // kHost3d: the first of a run into its target (its depth cleared)
};

struct Built {
    std::vector<V> verts;
    std::vector<Item> items;
};

// The render targets' sizes as the backend holds them (the builder follows the frame's draws into them in order)
std::map<uint32_t, std::pair<int, int>> g_rt_size;

// orig: (host)
void build(const MrpChainFrame& f, int win_w, int win_h, float FB_W, float FB_H, const DepthMap* depth, const Viewport& v, size_t from, size_t to, Built& out) {
    out.verts.clear();
    out.items.clear();
    const std::vector<Primitive>& prims = f.prims;
    // the frame pages drawn into and then sampled (TEX0's base = FRAME's page * 32): render targets
    std::set<uint32_t> sampled;
    for (const Primitive& p : prims)
        if (field(p.st.prim, 4, 1) != 0 && field(p.st.tex0, 20, 6) == 0) sampled.insert(field(p.st.tex0, 0, 14));
    std::map<uint32_t, std::pair<int, int>> rt = g_rt_size;  // (the sizes as the draws so far leave them)
    std::vector<V> verts;
    Key cur;
    bool have = false;
    int host_run_fbp = -1;
    const size_t prim_end = std::min(to, prims.size());
    for (size_t pi = from; pi < prim_end; ++pi) {
        const Primitive& p = prims[pi];
        if (p.fx != 0) continue;  // (the MRP's effects: mrp_fx)
        if (p.type == 7) {  // the frame's picture put back: drawn by the host
            if (v.screen_picture == nullptr || p.tex == nullptr) continue;
            Item it;
            it.kind = Item::kPicture;
            it.p = &p;
            out.items.push_back(it);
            have = false;
            continue;
        }
        if (p.type == 8) {  // the host's 3D here (the MRP's mobys drawn into a render target: the menus' item models)
            const uint32_t tfbp = field(p.st.frame, 0, 9);
            if (v.host_3d == nullptr || static_cast<int>(tfbp) == v.screen_fbp || sampled.count(tfbp * 32) == 0) {
                host_run_fbp = -1;
                continue;
            }
            const int rw = std::max(64, static_cast<int>(field(p.st.frame, 16, 6)) * 64), rh = std::max(rw, 512);
            rt[tfbp] = {rw, rh};
            Item it;
            it.kind = Item::kHost3d;
            it.p = &p;
            it.rt_w = rw;
            it.rt_h = rh;
            it.rt_depth = true;
            it.key.want = static_cast<int>(tfbp);
            it.host_first = host_run_fbp != static_cast<int>(tfbp);
            host_run_fbp = static_cast<int>(tfbp);
            out.items.push_back(it);
            have = false;
            continue;
        }
        host_run_fbp = -1;
        if (p.type != 1 && p.type != 3 && p.type != 6) continue;
        const DrawState& st = p.st;
        const bool tme = field(st.prim, 4, 1) != 0, fst = field(st.prim, 8, 1) != 0;
        const uint32_t ztst = field(st.test, 17, 2);
        const uint32_t fbp = field(st.frame, 0, 9);
        if (v.skip_fbp >= 0 && static_cast<int>(fbp) == v.skip_fbp) continue;
        // (the frame's own page is the screen even when sampled; a page at or above Viewport::rt_min_fbp that this frame does not
        // sample is a target too: the GS draws there, unseen)
        const bool to_rt = static_cast<int>(fbp) != v.screen_fbp && (sampled.count(fbp * 32) != 0 || (v.rt_min_fbp >= 0 && static_cast<int>(fbp) >= v.rt_min_fbp));
        const bool scr_tex = tme && !to_rt && v.screen_fbp >= 0 && field(st.tex0, 0, 14) == static_cast<uint32_t>(v.screen_fbp) * 32 && field(st.tex0, 20, 6) == 0;
        const bool date = !to_rt && field(st.test, 14, 1) != 0;
        // a render target's Z test on its own depth buffer (window depth = 1 - Z / 2^bits of ZBUF's format)
        const bool rt_z = to_rt && field(st.test, 16, 1) != 0;
        const double rt_zs = field(st.zbuf, 24, 4) == 0 ? 1.0 / 4294967296.0 : field(st.zbuf, 24, 4) == 1 ? 1.0 / 16777216.0 : 1.0 / 65536.0;
        const bool ztest = rt_z || (depth != nullptr && !to_rt && field(st.test, 16, 1) != 0 && ztst != 1);
        const float tw = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
        const auto src_rt = tme ? rt.find(field(st.tex0, 0, 14) / 32) : rt.end();
        const bool from_rt = tme && !scr_tex && field(st.tex0, 0, 14) % 32 == 0 && src_rt != rt.end() && sampled.count(field(st.tex0, 0, 14)) != 0;
        // texel units of the sampled texture: a render target is larger than the TEX0 size
        const float sw = from_rt ? static_cast<float>(src_rt->second.first) : tw, sh = from_rt ? static_cast<float>(src_rt->second.second) : th;
        auto mk = [&](const Vertex& q) {
            V o{q.x, q.y, q.r / 255.0f, q.g / 255.0f, q.b / 255.0f, q.a / 255.0f, 0, 0, 1, 0.5f, static_cast<float>(q.fog)};
            if (rt_z) o.d = static_cast<float>(std::clamp(1.0 - rt_zs * static_cast<double>(q.z), 0.0, 1.0));
            else if (ztest) o.d = static_cast<float>(std::clamp(depth->a + depth->b * static_cast<double>(q.z), 0.0, 1.0));
            if (fst) { o.s = q.u / sw; o.t = q.v / sh; o.q = 1.0f; }
            else { o.s = q.s * tw / sw; o.t = q.t * th / sh; o.q = q.q != 0.0f ? q.q : 1.0f; }
            return o;
        };
        const int want = to_rt ? static_cast<int>(fbp) : -1;
        int rw = 0, rh = 0;
        if (to_rt) {
            rw = std::max(64, static_cast<int>(field(st.frame, 16, 6)) * 64);
            rh = std::max(rw, 512);
            rt[fbp] = {rw, rh};
        }
        // the widened view: the game's projected 3D (a VU1 kick, or Z-tested at a depth inside the view) over the window, the 2D
        // at the frame's 4:3; the overscan crop leaves the projected 3D alone
        const bool crop_on = !to_rt && (v.crop[0] | v.crop[1] | v.crop[2] | v.crop[3]) != 0;
        bool proj3d = false;
        if (!to_rt && (v.xscale_3d != v.xscale || v.xscale < 1.0f || crop_on || v.hide_edge_slivers)) {
            proj3d = p.vu;
            if (!proj3d && depth != nullptr && field(st.test, 16, 1) != 0 && ztst >= 2)
                for (const Vertex& q : p.v) {
                    const double d = depth->a + depth->b * static_cast<double>(q.z);
                    if (q.z != 0 && d > 0.0 && d < 1.0) proj3d = true;
                }
        }
        const bool crop_3d = proj3d;
        if (!(v.xscale_3d != v.xscale || v.xscale < 1.0f)) proj3d = false;
        if (p.sub != 0) {  // (a substituted range: the substitute's projected 3D, the real range's 2D; the 2D drawn Z-tested sits at
                           // Z 0xfffff0, in front of everything)
            bool sub3d = p.vu;
            if (!sub3d && !to_rt && field(st.test, 16, 1) != 0 && ztst >= 2)
                for (const Vertex& q : p.v) sub3d = sub3d || (q.z != 0 && q.z < 0xff0000u);
            if ((p.sub == 1) != sub3d) continue;
        }
        const float sx = to_rt ? 1.0f : proj3d ? v.xscale_3d : v.xscale;
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
        // (the modern view, Viewport::sprite_clamp) a font glyph's bilinear sampling kept inside its own cell of the font's texture
        // (issue #76): a 16 x 16 or 24 x 16 texel cell at whole texels, drawn as a sprite or as a triangle whose texture coordinates
        // follow x and y apart; scaled up, its outer half texel no longer blends in the neighbouring glyphs' edges
        if (v.sprite_clamp && tme && !scr_tex && p.mip_n == 0 && !p.vu && (p.type == 6 || (p.type == 3 && fst))) {
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
            if (rect) {  // the clamp at the centres of the cell's outer texels
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
        // a 2D sprite / triangle that covers none of the GS's sample points draws nothing on the GS (pixel x covered when
        // x0 <= x < x1; GL samples at x + 0.5): not drawn (the oxygen indicator's frame parked at x = 512 lit the last column, #5)
        if (!p.vu && !p.wide && !crop_3d && (p.type == 6 || p.type == 3)) {
            float bx0 = p.v[0].x, bx1 = bx0, by0 = p.v[0].y, by1 = by0;
            for (size_t i = 1; i < (p.type == 6 ? 2u : 3u); ++i) {
                bx0 = std::min(bx0, p.v[i].x);
                bx1 = std::max(bx1, p.v[i].x);
                by0 = std::min(by0, p.v[i].y);
                by1 = std::max(by1, p.v[i].y);
            }
            bx0 = std::max(bx0, static_cast<float>(field(st.scissor, 0, 11)));
            bx1 = std::min(bx1, static_cast<float>(field(st.scissor, 16, 11) + 1));
            by0 = std::max(by0, static_cast<float>(field(st.scissor, 32, 11)));
            by1 = std::min(by1, static_cast<float>(field(st.scissor, 48, 11) + 1));
            if (std::ceil(bx0) >= bx1 || std::ceil(by0) >= by1) continue;
        }
        // the widened view's full-span 2D (a fade, a darkening in strips: its vertices at the frame's edges moved to the window's; a
        // flat fill covering every column / row as the GS fills it, a textured one only when it reaches the edges themselves) and
        // screen layers (a full-screen layer drawn in pieces: stretched about the frame's centre)
        bool to_left = false, to_right = false, full_span = false, edge_fill = false;
        if (!to_rt && p.layer && !proj3d && !p.vu && sx < 1.0f) {
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
            if (full_span && sx < 1.0f && !proj3d) {
                to_left = x0 <= 0.0f;
                to_right = x1 >= far_x;
                for (V& q : verts) {
                    if (q.x <= 0.0f) q.x = FB_W * 0.5f * (1.0f - 1.0f / sx);
                    else if (q.x >= far_x) q.x = FB_W * 0.5f * (1.0f + 1.0f / sx);
                }
            }
        }
        Key k;
        k.want = want;
        k.to_rt = to_rt;
        k.sx = sx;
        k.lines = p.type == 1;
        static const bool s_fog = std::getenv("RAC1_GS_FOG") != nullptr;  // (testing: the GS's fogging of the 2D, off as it always was)
        k.fge = s_fog && field(st.prim, 5, 1) != 0 ? 1 : 0;
        k.fogcol = k.fge != 0 ? static_cast<uint32_t>(st.fogcol & 0xffffffu) : 0;
        k.tme = tme ? 1 : 0;
        k.tfx = static_cast<int>(field(st.tex0, 35, 2));
        k.tcc = static_cast<int>(field(st.tex0, 34, 1));
        k.ate = static_cast<int>(field(st.test, 0, 1));
        k.atst = static_cast<int>(field(st.test, 1, 3));
        k.aref = static_cast<int>(field(st.test, 4, 8));
        k.afail = static_cast<int>(field(st.test, 12, 2));
        k.datm = static_cast<int>(field(st.test, 15, 1));
        k.scr = scr_tex ? 1 : 0;
        k.dt = date ? 1 : 0;
        k.fbmsk = field(st.frame, 32, 32);
        if (tme) {
            if (from_rt) k.tex.rt = static_cast<int>(src_rt->first);
            else k.tex.img = p.tex.get();
            k.lin = field(st.tex1, 5, 1) != 0 ? 1 : 0;
            k.wrap_s = field(st.clamp, 0, 2) == 0 ? 1 : 0;
            k.wrap_t = field(st.clamp, 2, 2) == 0 ? 1 : 0;
            k.lin_min = field(st.tex1, 6, 3) >= 4 ? 1 : 0;
            k.mip_n = p.mip_n;
            for (int m = 0; m < p.mip_n && m < 3; ++m) k.mip[m].img = p.mip_tex[m].get();
        }
        k.mip_lo = p.mip_lo;
        k.lcm = static_cast<int>(field(st.tex1, 0, 1));
        k.mxl = static_cast<int>(std::min(field(st.tex1, 2, 3), 3u));
        k.mip_k = static_cast<float>(static_cast<int32_t>(field(st.tex1, 32, 12) << 20) >> 20) / 16.0f;
        k.mip_l = static_cast<float>(1u << field(st.tex1, 19, 2));
        k.blend = field(st.prim, 6, 1) != 0;
        k.alpha = k.blend ? st.alpha : 0;
        k.ztest = ztest;
        k.ztst = static_cast<int>(ztst);
        k.zwrite = ztest && field(st.zbuf, 32, 1) == 0;
        int x0 = static_cast<int>(field(st.scissor, 0, 11)), x1 = static_cast<int>(field(st.scissor, 16, 11));
        int y0 = static_cast<int>(field(st.scissor, 32, 11)), y1 = static_cast<int>(field(st.scissor, 48, 11));
        // the 2D slivers at the frame's edges (Viewport::hide_edge_slivers): at most 4 frame pixels inside, the rest outside
        if (v.hide_edge_slivers && !to_rt && tme && !crop_3d && !p.vu && !p.wide && !(p.layer && full_span)) {
            float bx0 = verts[0].x, bx1 = bx0, by0 = verts[0].y, by1 = by0;
            for (const V& q : verts) {
                bx0 = std::min(bx0, q.x);
                bx1 = std::max(bx1, q.x);
                by0 = std::min(by0, q.y);
                by1 = std::max(by1, q.y);
            }
            constexpr float kSliver = 4.0f;
            if ((bx1 > FB_W + 0.5f && bx0 >= FB_W - kSliver) || (bx0 < -0.5f && bx1 <= kSliver) || (by0 < -0.5f && by1 <= kSliver)) continue;
        }
        // the overscan crop (testing: RAC1_OVERSCAN): the 2D's scissor narrowed to the area a TV showed
        if (crop_on && !crop_3d && !p.vu && !p.wide && !full_span && !edge_fill) {
            const int cw = static_cast<int>(FB_W), ch = static_cast<int>(FB_H);
            x0 = std::max(x0, v.crop[0]);
            x1 = std::min(x1, cw - 1 - v.crop[1]);
            y0 = std::max(y0, v.crop[2]);
            y1 = std::min(y1, ch - 1 - v.crop[3]);
            if (x1 < x0 || y1 < y0) continue;
        }
        // a HUD wider than 4:3 (Viewport::hud_shift): an element at the frame's left / right side moved toward the window's edges in
        // whole window pixels, its scissor with it
        float hud_dx = 0.0f;
        if (v.hud_shift != 0.0f && p.hud != 0 && !to_rt && !proj3d && !p.vu && !p.wide && !p.layer && !full_span) {
            const float kx = sx * static_cast<float>(v.w) / FB_W;
            hud_dx = std::round(v.hud_shift * kx) / kx * (p.hud == 1 ? -1.0f : 1.0f);
            for (V& q : verts) q.x += hud_dx;
        }
        if (to_rt) {
            k.sc[0] = x0; k.sc[1] = y0; k.sc[2] = x1 - x0 + 1; k.sc[3] = y1 - y0 + 1;
        } else if (hud_dx != 0.0f) {
            auto wxu = [&](float x) { return static_cast<float>(v.x) + ((x / FB_W * 2.0f - 1.0f) * sx + 1.0f) * 0.5f * static_cast<float>(v.w); };
            const float sy = static_cast<float>(v.h) / FB_H;
            const int sx0 = std::clamp(static_cast<int>(wxu(static_cast<float>(x0) + hud_dx)), v.x, v.x + v.w);
            const int sx1 = std::clamp(static_cast<int>(wxu(static_cast<float>(x1 + 1) + hud_dx)), v.x, v.x + v.w);
            k.sc[0] = sx0;
            k.sc[1] = static_cast<int>(static_cast<float>(v.y) + static_cast<float>(v.h) - static_cast<float>(y1 + 1) * sy);
            k.sc[2] = std::max(0, sx1 - sx0);
            k.sc[3] = static_cast<int>(static_cast<float>(y1 - y0 + 1) * sy);
        } else {
            // the frame's edges cut what is beyond them (a 4:3 frame narrower than the window); a substituted range's primitive and
            // any projected 3D reach the window's sides
            auto wx = [&](float x) { return static_cast<float>(v.x) + ((std::clamp(x, 0.0f, FB_W) / FB_W * 2.0f - 1.0f) * sx + 1.0f) * 0.5f * static_cast<float>(v.w); };
            const float sy = static_cast<float>(v.h) / FB_H;
            const bool wide = (p.wide || proj3d) && sx < 1.0f && !v.cut_3d;
            const int sx0 = (to_left || wide) && x0 <= 0 ? v.x : static_cast<int>(wx(static_cast<float>(x0)));
            const int sx1 = (to_right || wide) && static_cast<float>(x1 + 1) >= FB_W ? v.x + v.w : static_cast<int>(wx(static_cast<float>(x1 + 1)));
            k.sc[0] = sx0;
            k.sc[1] = static_cast<int>(static_cast<float>(v.y) + static_cast<float>(v.h) - static_cast<float>(y1 + 1) * sy);
            k.sc[2] = sx1 - sx0;
            k.sc[3] = static_cast<int>(static_cast<float>(y1 - y0 + 1) * sy);
        }
        const bool alone = scr_tex || date;  // (the screen copy it reads is of this primitive's box)
        if (alone || !have || !(k == cur) || out.items.empty() || out.items.back().kind != Item::kDraw) {
            Item it;
            it.kind = Item::kDraw;
            it.key = k;
            it.first = static_cast<uint32_t>(out.verts.size());
            it.alone = alone;
            if (to_rt) {
                it.rt_w = rw;
                it.rt_h = rh;
                it.rt_depth = rt_z;
            }
            if (alone) {  // the screen under the primitive (its window box), and how its texture coordinates name window pixels
                const float bx = static_cast<float>(v.w) * 0.5f * sx * 2.0f / FB_W, ax = static_cast<float>(v.x) + 0.5f * static_cast<float>(v.w) * (1.0f - sx);
                const float by = -static_cast<float>(v.h) / FB_H, ay = static_cast<float>(v.y + v.h);
                float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
                for (const V& q : verts) {
                    bx0 = std::min(bx0, ax + bx * q.x);
                    bx1 = std::max(bx1, ax + bx * q.x);
                    by0 = std::min(by0, ay + by * q.y);
                    by1 = std::max(by1, ay + by * q.y);
                }
                it.copy[0] = std::clamp(static_cast<int>(bx0) - 1, 0, win_w);
                it.copy[2] = std::clamp(static_cast<int>(bx1) + 2, 0, win_w);
                it.copy[1] = std::clamp(static_cast<int>(by0) - 1, 0, win_h);
                it.copy[3] = std::clamp(static_cast<int>(by1) + 2, 0, win_h);
                const float tw0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
                it.scr_map[0] = ax;
                it.scr_map[1] = bx * tw0;
                it.scr_map[2] = ay;
                it.scr_map[3] = by * th0;
            }
            out.items.push_back(it);
            cur = k;
            have = !alone;
        }
        out.verts.insert(out.verts.end(), verts.begin(), verts.end());
        out.items.back().count += static_cast<uint32_t>(verts.size());
        if (alone) have = false;
    }
}

Built g_built;
uint64_t g_frame_no = 0;  // (the calls' count: the textures' last use)

// ---- OpenGL ----

GLuint g_prog = 0, g_vao = 0, g_vbo = 0;
size_t g_vbo_size = 0;
struct GlLocs {
    GLint fb = -1, rt = -1, scale_x = -1, fge = -1, fog_col = -1;
    GLint tme = -1, tfx = -1, tcc = -1, ate = -1, atst = -1, aref = -1, afail = -1, pass = -1;
    GLint scr_tex = -1, date = -1, datm = -1, scr_map = -1;
    GLint mip_n = -1, mip_lo = -1, mip_lcm = -1, mip_mxl = -1, mip_k = -1, mip_l = -1;
} g_loc;
struct GlRt {
    GLuint fbo = 0, tex = 0, depth = 0;
    int w = 0, h = 0, depth_w = 0, depth_h = 0;
};
std::map<uint32_t, GlRt> g_gl_rt;
struct GlTex {
    GLuint id = 0;
    uint64_t used = 0;
};
std::unordered_map<uint64_t, GlTex> g_gl_tex;  // by the image's id
GLuint g_gl_screen = 0;
int g_gl_screen_w = 0, g_gl_screen_h = 0;
std::map<int, GLuint> g_gl_samplers;  // (lin, wrap_s, wrap_t) -> sampler

// orig: (host)
GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::fprintf(stderr, "[mrp-2d] shader: %s\n", log);
    }
    return s;
}

// orig: (host)
void gl_init() {
    if (g_prog != 0) return;
    const GLuint vs = compile(GL_VERTEX_SHADER, VS), fs = compile(GL_FRAGMENT_SHADER, FS);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    glUseProgram(g_prog);
    glUniform1i(glGetUniformLocation(g_prog, "uTexture"), 0);
    glUniform1i(glGetUniformLocation(g_prog, "uScreen"), 1);
    glUniform1i(glGetUniformLocation(g_prog, "uMip1"), 2);
    glUniform1i(glGetUniformLocation(g_prog, "uMip2"), 3);
    glUniform1i(glGetUniformLocation(g_prog, "uMip3"), 4);
    glUniform1i(glGetUniformLocation(g_prog, "uPass"), 0);
    g_loc.fb = glGetUniformLocation(g_prog, "uFb");
    g_loc.rt = glGetUniformLocation(g_prog, "uRt");
    g_loc.scale_x = glGetUniformLocation(g_prog, "uScaleX");
    g_loc.fge = glGetUniformLocation(g_prog, "uFge");
    g_loc.fog_col = glGetUniformLocation(g_prog, "uFogCol");
    g_loc.tme = glGetUniformLocation(g_prog, "uTme");
    g_loc.tfx = glGetUniformLocation(g_prog, "uTfx");
    g_loc.tcc = glGetUniformLocation(g_prog, "uTcc");
    g_loc.ate = glGetUniformLocation(g_prog, "uAte");
    g_loc.atst = glGetUniformLocation(g_prog, "uAtst");
    g_loc.aref = glGetUniformLocation(g_prog, "uAref");
    g_loc.afail = glGetUniformLocation(g_prog, "uAfail");
    g_loc.pass = glGetUniformLocation(g_prog, "uPass");
    g_loc.scr_tex = glGetUniformLocation(g_prog, "uScrTex");
    g_loc.date = glGetUniformLocation(g_prog, "uDate");
    g_loc.datm = glGetUniformLocation(g_prog, "uDatm");
    g_loc.scr_map = glGetUniformLocation(g_prog, "uScrMap");
    g_loc.mip_n = glGetUniformLocation(g_prog, "uMipN");
    g_loc.mip_lo = glGetUniformLocation(g_prog, "uMipLo");
    g_loc.mip_lcm = glGetUniformLocation(g_prog, "uMipLcm");
    g_loc.mip_mxl = glGetUniformLocation(g_prog, "uMipMxl");
    g_loc.mip_k = glGetUniformLocation(g_prog, "uMipK");
    g_loc.mip_l = glGetUniformLocation(g_prog, "uMipL");
    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    const GLsizei stride = sizeof(V);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(6 * sizeof(float)));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(9 * sizeof(float)));
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(10 * sizeof(float)));
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, stride, reinterpret_cast<void*>(11 * sizeof(float)));
    for (GLuint a = 0; a < 6; ++a) glEnableVertexAttribArray(a);
    glBindVertexArray(0);
}

// the ALPHA register's (A - B) C + D as the GPU's blending (gs_blend.h)
// orig: (host)
gs::DrawState blend_of(bool blend, uint64_t alpha) {
    gs::DrawState st;
    st.prim = blend ? (1ull << 6) : 0ull;
    st.alpha = alpha;
    return st;
}

// orig: (host)
GLuint gl_sampler(bool lin, bool wrap_s, bool wrap_t) {
    const int key = (lin ? 1 : 0) | (wrap_s ? 2 : 0) | (wrap_t ? 4 : 0);
    GLuint& s = g_gl_samplers[key];
    if (s == 0) {
        glGenSamplers(1, &s);
        glSamplerParameteri(s, GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(s, GL_TEXTURE_MIN_FILTER, lin ? GL_LINEAR : GL_NEAREST);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_S, wrap_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
        glSamplerParameteri(s, GL_TEXTURE_WRAP_T, wrap_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
    }
    return s;
}

// orig: (host)
GLuint gl_texture(const TexImage* img) {
    GlTex& e = g_gl_tex[img->id];
    e.used = g_frame_no;
    if (e.id != 0) return e.id;
    glGenTextures(1, &e.id);
    rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &e.id, "MRP 2D: textures");
    glBindTexture(GL_TEXTURE_2D, e.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, img->w, img->h, 0, GL_RGBA, GL_UNSIGNED_BYTE, img->px.data());
    return e.id;
}

// orig: (host)
GlRt& gl_rt(uint32_t fbp, int w, int h) {
    GlRt& r = g_gl_rt[fbp];
    if (r.fbo != 0 && r.w == w && r.h == h) return r;
    if (r.fbo == 0) {
        glGenFramebuffers(1, &r.fbo);
        glGenTextures(1, &r.tex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &r.tex, "MRP 2D: render targets");
    }
    r.w = w;
    r.h = h;
    glBindTexture(GL_TEXTURE_2D, r.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.tex, 0);
    if (r.depth != 0) r.depth_w = r.depth_h = 0;  // (resized with it below, when asked)
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    g_rt_size[fbp] = {w, h};
    return r;
}

// orig: (host)
void gl_rt_depth(GlRt& r) {
    if (r.depth == 0) {
        glGenRenderbuffers(1, &r.depth);
        rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &r.depth, "MRP 2D: render targets");
    }
    glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
    if (r.depth_w != r.w || r.depth_h != r.h) {
        glBindRenderbuffer(GL_RENDERBUFFER, r.depth);
        glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT32F, r.w, r.h);
        glBindRenderbuffer(GL_RENDERBUFFER, 0);
        glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, r.depth);
        r.depth_w = r.w;
        r.depth_h = r.h;
    }
}

// orig: (host)
void gl_submit(const Built& b, int win_w, int win_h, float FB_W, float FB_H, const Viewport& v) {
    gl_init();
    GLint depth_func = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    GLint screen_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &screen_fbo);
    glUseProgram(g_prog);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    const size_t bytes = b.verts.size() * sizeof(V);
    if (bytes > 0) {  // (the call's vertices in one upload, the buffer orphaned)
        if (bytes > g_vbo_size) g_vbo_size = std::max(bytes, g_vbo_size * 2);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g_vbo_size), nullptr, GL_STREAM_DRAW);
        glBufferSubData(GL_ARRAY_BUFFER, 0, static_cast<GLsizeiptr>(bytes), b.verts.data());
    }
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    glViewport(v.x, v.y, v.w, v.h);
    int cur_rt = -2;  // the target bound (-1 the screen, -2 not known)
    bool dirty = true;
    Key applied;
    bool screen_bound = false;
    auto bind_target = [&](int want) {
        if (want == cur_rt) return;
        if (want >= 0) {
            const GlRt& r = g_gl_rt[static_cast<uint32_t>(want)];
            glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
            glViewport(0, 0, r.w, r.h);
            glUniform2f(g_loc.fb, static_cast<float>(r.w), static_cast<float>(r.h));
            glUniform1i(g_loc.rt, 1);
        } else {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
            glViewport(v.x, v.y, v.w, v.h);
            glUniform2f(g_loc.fb, FB_W, FB_H);
            glUniform1i(g_loc.rt, 0);
        }
        cur_rt = want;
    };
    auto restore = [&] {  // (after a host callback: the GL state set again)
        glUseProgram(g_prog);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        glActiveTexture(GL_TEXTURE0);
        glBindSampler(0, 0);
        glDisable(GL_DEPTH_TEST);
        glDepthMask(GL_TRUE);
        glEnable(GL_SCISSOR_TEST);
        cur_rt = -2;
        dirty = true;
        screen_bound = false;
    };
    for (const Item& it : b.items) {
        if (it.kind == Item::kPicture) {
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            v.screen_picture(*it.p->tex, v);
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
            restore();
            continue;
        }
        if (it.kind == Item::kHost3d) {
            GlRt& r = gl_rt(static_cast<uint32_t>(it.key.want), it.rt_w, it.rt_h);
            gl_rt_depth(r);
            glViewport(0, 0, r.w, r.h);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            if (it.host_first) {
                glDepthMask(GL_TRUE);
                glClearDepth(1.0);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            v.host_3d(*it.p, r.w, r.h, it.host_first);
            restore();
            continue;
        }
        const Key& k = it.key;
        if (k.to_rt) {
            const auto ex = g_gl_rt.find(static_cast<uint32_t>(k.want));
            if (ex == g_gl_rt.end() || ex->second.fbo == 0 || ex->second.w != it.rt_w || ex->second.h != it.rt_h) {
                gl_rt(static_cast<uint32_t>(k.want), it.rt_w, it.rt_h);  // (created or resized: it binds and clears it)
                cur_rt = -2;
                dirty = true;
            }
            if (it.rt_depth) {
                GlRt& r = g_gl_rt[static_cast<uint32_t>(k.want)];
                if (r.depth == 0 || r.depth_w != r.w || r.depth_h != r.h) {
                    gl_rt_depth(r);
                    cur_rt = -2;
                    dirty = true;
                }
            }
        }
        // the textures it samples (uploaded at their first use)
        GLuint tex = 0, mips[3] = {0, 0, 0};
        if (k.tme) {
            if (k.tex.rt >= 0) tex = g_gl_rt[static_cast<uint32_t>(k.tex.rt)].tex;
            else if (k.tex.img != nullptr) tex = gl_texture(k.tex.img);
            for (int m = 0; m < k.mip_n && m < 3; ++m)
                if (k.mip[m].img != nullptr) mips[m] = gl_texture(k.mip[m].img);
            dirty = true;  // (an upload binds its texture on unit 0)
        }
        auto chg = [&](auto m) { return dirty || k.*m != applied.*m; };
        bind_target(k.want);
        if (chg(&Key::sx)) glUniform1f(g_loc.scale_x, k.sx);
        if (chg(&Key::tme)) glUniform1i(g_loc.tme, k.tme);
        if (chg(&Key::fge)) glUniform1i(g_loc.fge, k.fge);
        if (chg(&Key::fogcol))
            glUniform3f(g_loc.fog_col, static_cast<float>(k.fogcol & 0xff) / 255.0f, static_cast<float>((k.fogcol >> 8) & 0xff) / 255.0f, static_cast<float>((k.fogcol >> 16) & 0xff) / 255.0f);
        if (chg(&Key::tfx)) glUniform1i(g_loc.tfx, k.tfx);
        if (chg(&Key::tcc)) glUniform1i(g_loc.tcc, k.tcc);
        if (chg(&Key::ate)) glUniform1i(g_loc.ate, k.ate);
        if (chg(&Key::atst)) glUniform1i(g_loc.atst, k.atst);
        if (chg(&Key::aref)) glUniform1i(g_loc.aref, k.aref);
        if (chg(&Key::afail)) glUniform1i(g_loc.afail, k.afail);
        if (chg(&Key::scr)) glUniform1i(g_loc.scr_tex, k.scr);
        if (chg(&Key::dt)) glUniform1i(g_loc.date, k.dt);
        if (chg(&Key::datm)) glUniform1i(g_loc.datm, k.datm);
        if (it.alone) {  // the screen under the primitive copied (its window box), sampled at the window position its texture coordinates name
            if (g_gl_screen == 0 || g_gl_screen_w != win_w || g_gl_screen_h != win_h) {
                if (g_gl_screen == 0) glGenTextures(1, &g_gl_screen);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_gl_screen, "MRP 2D: screen copy");
                g_gl_screen_w = win_w;
                g_gl_screen_h = win_h;
                glBindTexture(GL_TEXTURE_2D, g_gl_screen);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                // (as the GS replica's copy: its default filter, no mip levels - an incomplete texture, which texelFetch reads as zeros;
                // RAC1_GS_SCREEN_FILTER=1 makes it complete, testing)
                static const bool s_filter = std::getenv("RAC1_GS_SCREEN_FILTER") != nullptr;
                if (s_filter) {
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                }
                screen_bound = false;
            }
            glActiveTexture(GL_TEXTURE1);
            if (!screen_bound) {
                glBindTexture(GL_TEXTURE_2D, g_gl_screen);
                screen_bound = true;
            }
            if (it.copy[2] > it.copy[0] && it.copy[3] > it.copy[1])
                glCopyTexSubImage2D(GL_TEXTURE_2D, 0, it.copy[0], it.copy[1], it.copy[0], it.copy[1], it.copy[2] - it.copy[0], it.copy[3] - it.copy[1]);
            glActiveTexture(GL_TEXTURE0);
            glUniform4f(g_loc.scr_map, it.scr_map[0], it.scr_map[1], it.scr_map[2], it.scr_map[3]);
        }
        if (chg(&Key::fbmsk))
            glColorMask((k.fbmsk & 0xffu) != 0xffu, (k.fbmsk & 0xff00u) != 0xff00u, (k.fbmsk & 0xff0000u) != 0xff0000u, (k.fbmsk & 0xff000000u) != 0xff000000u);
        if (k.tme) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, tex);
            glBindSampler(0, gl_sampler(k.lin != 0, k.wrap_s != 0, k.wrap_t != 0));
            for (int m = 0; m < k.mip_n && m < 3; ++m) {
                glActiveTexture(GL_TEXTURE2 + m);
                glBindTexture(GL_TEXTURE_2D, mips[m]);
                glBindSampler(static_cast<GLuint>(2 + m), gl_sampler(k.lin_min != 0, k.wrap_s != 0, k.wrap_t != 0));
            }
            glActiveTexture(GL_TEXTURE0);
        }
        if (chg(&Key::mip_n)) glUniform1i(g_loc.mip_n, k.mip_n);
        if (chg(&Key::mip_lo)) glUniform1i(g_loc.mip_lo, k.mip_lo);
        if (chg(&Key::lcm)) glUniform1i(g_loc.mip_lcm, k.lcm);
        if (chg(&Key::mxl)) glUniform1i(g_loc.mip_mxl, k.mxl);
        if (chg(&Key::mip_k)) glUniform1f(g_loc.mip_k, k.mip_k);
        if (chg(&Key::mip_l)) glUniform1f(g_loc.mip_l, k.mip_l);
        if (chg(&Key::blend) || chg(&Key::alpha) || chg(&Key::to_rt)) gs::blend_state(blend_of(k.blend, k.alpha), k.to_rt);
        if (chg(&Key::ztest) || chg(&Key::ztst) || chg(&Key::zwrite)) {
            if (k.ztest) {
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(k.ztst == 0 ? GL_NEVER : k.ztst == 1 ? GL_ALWAYS : k.ztst == 2 ? GL_LEQUAL : GL_LESS);
                glDepthMask(k.zwrite ? GL_TRUE : GL_FALSE);
            } else {
                glDisable(GL_DEPTH_TEST);
                glDepthMask(GL_FALSE);
            }
        }
        if (dirty || std::memcmp(k.sc, applied.sc, sizeof k.sc) != 0) glScissor(k.sc[0], k.sc[1], k.sc[2], k.sc[3]);
        applied = k;
        dirty = false;
        const GLenum mode = k.lines ? GL_LINES : GL_TRIANGLES;
        // the alpha test's fail mode FB_ONLY / RGB_ONLY / ZB_ONLY with Z writes: the colour and the Z of different pixels (the Z pass
        // first, then the colour against it with LEQUAL)
        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;
        const bool split = !no_split && k.ztest && k.zwrite && k.ate != 0 && k.atst != 1 && k.afail != 0;
        if (split) {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glUniform1i(g_loc.pass, 1);
            glDrawArrays(mode, static_cast<GLint>(it.first), static_cast<GLsizei>(it.count));
            glUniform1i(g_loc.pass, 0);
            const uint32_t m = k.fbmsk;
            glColorMask((m & 0xffu) != 0xffu, (m & 0xff00u) != 0xff00u, (m & 0xff0000u) != 0xff0000u, (m & 0xff000000u) != 0xff000000u);
            glDepthMask(GL_FALSE);
            if (k.ztst != 0) glDepthFunc(GL_LEQUAL);
        }
        glDrawArrays(mode, static_cast<GLint>(it.first), static_cast<GLsizei>(it.count));
        if (split) {
            glDepthMask(GL_TRUE);
            glDepthFunc(k.ztst == 0 ? GL_NEVER : k.ztst == 1 ? GL_ALWAYS : k.ztst == 2 ? GL_LEQUAL : GL_LESS);
        }
    }
    (void)win_w;
    (void)win_h;
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
    glViewport(v.x, v.y, v.w, v.h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(static_cast<GLenum>(depth_func));
    glDepthMask(GL_TRUE);
    for (GLuint u = 0; u < 5; ++u) glBindSampler(u, 0);
    glBindVertexArray(0);
}

// ---- Vulkan ----

struct Push {
    float uFb[2];
    float uScaleX;
    int32_t uRt;
    float uFogCol[3];
    int32_t uFge;
    float uScrMap[4];
    int32_t uTme;
    int32_t uTfx;
    int32_t uTcc;
    int32_t uAte;
    int32_t uAtst;
    int32_t uAref;
    int32_t uAfail;
    int32_t uPass;
    int32_t uScrTex;
    int32_t uDate;
    int32_t uDatm;
    int32_t uMipN;
    int32_t uMipLo;
    int32_t uMipLcm;
    int32_t uMipMxl;
    float uMipK;
    float uMipL;
    float _pad[3];
};
static_assert(sizeof(Push) == 128);

struct VkRt {
    vk::GpuImage color{};
    vk::GpuImage depth{};
    int w = 0, h = 0;
    int depth_w = 0, depth_h = 0;
};
struct VkTex {
    vk::GpuImage img{};
    uint64_t used = 0;
};
struct PipeKey {
    VkPrimitiveTopology topology;
    bool depth_test, depth_write;
    VkCompareOp depth_compare;
    bool blend_enable;
    VkBlendFactor src_color, dst_color;
    VkBlendOp color_op;
    VkColorComponentFlags color_mask;
    bool has_depth, has_stencil;
    bool operator<(const PipeKey& o) const {
        return std::tie(topology, depth_test, depth_write, depth_compare, blend_enable, src_color, dst_color, color_op, color_mask, has_depth, has_stencil) <
               std::tie(o.topology, o.depth_test, o.depth_write, o.depth_compare, o.blend_enable, o.src_color, o.dst_color, o.color_op, o.color_mask, o.has_depth,
                        o.has_stencil);
    }
};

VkShaderModule g_vk_vs = VK_NULL_HANDLE, g_vk_fs = VK_NULL_HANDLE;
VkDescriptorSetLayout g_vk_set_layout = VK_NULL_HANDLE;
VkPipelineLayout g_vk_layout = VK_NULL_HANDLE;
std::map<PipeKey, VkPipeline> g_vk_pipes;
std::map<uint32_t, VkRt> g_vk_rt;
std::unordered_map<uint64_t, VkTex> g_vk_tex;  // by the image's id
vk::GpuImage g_vk_screen{};
int g_vk_screen_w = 0, g_vk_screen_h = 0;

// orig: (host)
void vk_shutdown() {
    for (auto& [k, p] : g_vk_pipes)
        if (p != VK_NULL_HANDLE) vkDestroyPipeline(vk::device(), p, nullptr);
    g_vk_pipes.clear();
    if (g_vk_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(vk::device(), g_vk_layout, nullptr);
    g_vk_layout = VK_NULL_HANDLE;
    if (g_vk_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(vk::device(), g_vk_set_layout, nullptr);
    g_vk_set_layout = VK_NULL_HANDLE;
    if (g_vk_vs != VK_NULL_HANDLE) vkDestroyShaderModule(vk::device(), g_vk_vs, nullptr);
    if (g_vk_fs != VK_NULL_HANDLE) vkDestroyShaderModule(vk::device(), g_vk_fs, nullptr);
    g_vk_vs = g_vk_fs = VK_NULL_HANDLE;
    for (auto& [k, r] : g_vk_rt) {
        vk::destroy_image(r.color);
        vk::destroy_image(r.depth);
    }
    g_vk_rt.clear();
    for (auto& [k, t] : g_vk_tex) vk::destroy_image(t.img);
    g_vk_tex.clear();
    vk::destroy_image(g_vk_screen);
    g_vk_screen_w = g_vk_screen_h = 0;
    g_rt_size.clear();
}

// orig: (host)
void vk_init() {
    if (g_vk_layout != VK_NULL_HANDLE) return;
    vk::register_shutdown_callback(vk_shutdown);
    g_vk_vs = vk::create_shader_module(vk::shaders::kMrp2dVs, sizeof(vk::shaders::kMrp2dVs));
    g_vk_fs = vk::create_shader_module(vk::shaders::kMrp2dFs, sizeof(vk::shaders::kMrp2dFs));
    VkDescriptorSetLayoutBinding bindings[5]{};
    for (uint32_t i = 0; i < 5; ++i) {
        bindings[i].binding = i;
        bindings[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        bindings[i].descriptorCount = 1;
        bindings[i].stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    }
    VkDescriptorSetLayoutCreateInfo slci{};
    slci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slci.bindingCount = 5;
    slci.pBindings = bindings;
    vkCreateDescriptorSetLayout(vk::device(), &slci, nullptr, &g_vk_set_layout);
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.size = sizeof(Push);
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_vk_set_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    vkCreatePipelineLayout(vk::device(), &plci, nullptr, &g_vk_layout);
}

// orig: (host)
VkPipeline vk_pipeline(const PipeKey& key) {
    auto it = g_vk_pipes.find(key);
    if (it != g_vk_pipes.end()) return it->second;
    vk::GraphicsPipelineDesc desc{};
    desc.layout = g_vk_layout;
    desc.vs = g_vk_vs;
    desc.fs = g_vk_fs;
    desc.topology = key.topology;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.depth_test = key.depth_test;
    desc.depth_write = key.depth_write;
    desc.depth_compare = key.depth_compare;
    desc.blend_enable = key.blend_enable;
    desc.src_color_blend = key.src_color;
    desc.dst_color_blend = key.dst_color;
    desc.color_blend_op = key.color_op;
    desc.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    desc.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
    desc.alpha_blend_op = VK_BLEND_OP_ADD;
    desc.color_write_mask = key.color_mask;
    desc.color_format = vk::color_format();
    desc.has_depth_attachment = key.has_depth;
    desc.has_stencil_attachment = key.has_stencil;
    desc.vertex_bindings = {{0, sizeof(V), VK_VERTEX_INPUT_RATE_VERTEX}};
    desc.vertex_attribs = {
        {0, 0, VK_FORMAT_R32G32_SFLOAT, 0},
        {1, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 2 * sizeof(float)},
        {2, 0, VK_FORMAT_R32G32B32_SFLOAT, 6 * sizeof(float)},
        {3, 0, VK_FORMAT_R32_SFLOAT, 9 * sizeof(float)},
        {4, 0, VK_FORMAT_R32_SFLOAT, 10 * sizeof(float)},
        {5, 0, VK_FORMAT_R32G32B32A32_SFLOAT, 11 * sizeof(float)},
    };
    VkPipeline p = vk::create_graphics_pipeline(desc);
    g_vk_pipes[key] = p;
    return p;
}

// orig: (host)
VkImageView vk_texture(const TexImage* img) {
    VkTex& e = g_vk_tex[img->id];
    e.used = g_frame_no;
    if (e.img.view != VK_NULL_HANDLE) return e.img.view;
    vk::create_image(static_cast<uint32_t>(img->w), static_cast<uint32_t>(img->h), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, e.img);
    vk::upload_image_2d(e.img, img->px.data(), img->px.size() * 4, 0, 0, img->w, img->h);
    return e.img.view;
}

// orig: (host)
VkRt& vk_rt(uint32_t fbp, int w, int h) {
    VkRt& r = g_vk_rt[fbp];
    if (r.color.image != VK_NULL_HANDLE && r.w == w && r.h == h) return r;
    r.w = w;
    r.h = h;
    vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                     VK_IMAGE_ASPECT_COLOR_BIT, r.color);
    vk::end_rendering();
    vk::transition_image(vk::cmd(), r.color, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearColorValue cc = {{0.0f, 0.0f, 0.0f, 0.0f}};
    VkImageSubresourceRange sr = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, 0, 1};
    vkCmdClearColorImage(vk::cmd(), r.color.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &cc, 1, &sr);
    vk::transition_image(vk::cmd(), r.color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    g_rt_size[fbp] = {w, h};
    return r;
}

// orig: (host)
void vk_rt_depth(VkRt& r) {
    if (r.depth.image != VK_NULL_HANDLE && r.depth_w == r.w && r.depth_h == r.h) return;
    r.depth_w = r.w;
    r.depth_h = r.h;
    vk::create_image(static_cast<uint32_t>(r.w), static_cast<uint32_t>(r.h), vk::depth_stencil_format(),
                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                     VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, r.depth);
    vk::end_rendering();
    vk::transition_image(vk::cmd(), r.depth, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
    VkClearDepthStencilValue dv = {1.0f, 0};
    VkImageSubresourceRange sr = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
    vkCmdClearDepthStencilImage(vk::cmd(), r.depth.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, &dv, 1, &sr);
    vk::transition_image(vk::cmd(), r.depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
}

// orig: (host)
void vk_submit(const Built& b, int win_w, int win_h, float FB_W, float FB_H, const Viewport& v) {
    vk_init();
    int cur_rt = -1;
    bool rt_pushed = false;
    auto bind_target = [&](int want, bool need_depth) {
        if (want == cur_rt && (!need_depth || want < 0 || g_vk_rt[static_cast<uint32_t>(want)].depth.image != VK_NULL_HANDLE)) return;
        if (rt_pushed) {
            vk::pop_target();
            if (cur_rt >= 0) vk::transition_image(vk::cmd(), g_vk_rt[static_cast<uint32_t>(cur_rt)].color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            rt_pushed = false;
        }
        if (want >= 0) {
            VkRt& r = g_vk_rt[static_cast<uint32_t>(want)];
            if (need_depth) vk_rt_depth(r);
            vk::transition_image(vk::cmd(), r.color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            if (r.depth.image != VK_NULL_HANDLE) vk::transition_image(vk::cmd(), r.depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            vk::RenderTarget rt{};
            rt.color_view = r.color.view;
            rt.color_format = r.color.format;
            rt.depth_view = r.depth.view;
            rt.depth_format = r.depth.format;
            rt.stencil_view = r.depth.view;
            rt.width = static_cast<uint32_t>(r.w);
            rt.height = static_cast<uint32_t>(r.h);
            vk::set_viewport(0, 0, r.w, r.h);
            vk::push_target(rt);
            rt_pushed = true;
        } else {
            vk::set_viewport(v.x, v.y, v.w, v.h);
        }
        cur_rt = want;
    };
    // the call's vertices in one allocation of the frame's ring
    vk::RingAlloc vbuf{};
    if (!b.verts.empty()) {
        const size_t bytes = b.verts.size() * sizeof(V);
        vbuf = vk::alloc_ring(bytes, 16);
        std::memcpy(vbuf.ptr, b.verts.data(), bytes);
    }
    for (const Item& it : b.items) {
        if (it.kind == Item::kPicture) {
            bind_target(-1, false);
            vk::set_scissor(false);
            v.screen_picture(*it.p->tex, v);
            cur_rt = -2;
            continue;
        }
        if (it.kind == Item::kHost3d) {
            VkRt& r = vk_rt(static_cast<uint32_t>(it.key.want), it.rt_w, it.rt_h);
            vk_rt_depth(r);
            bind_target(it.key.want, true);
            vk::set_viewport(0, 0, r.w, r.h);
            vk::set_scissor(false);
            if (it.host_first) vk::clear_attachments(false, nullptr, true, 1.0f, false, 0);
            v.host_3d(*it.p, r.w, r.h, it.host_first);
            continue;
        }
        const Key& k = it.key;
        if (k.to_rt) {
            const auto ex = g_vk_rt.find(static_cast<uint32_t>(k.want));
            if (ex == g_vk_rt.end() || ex->second.color.image == VK_NULL_HANDLE || ex->second.w != it.rt_w || ex->second.h != it.rt_h) {
                bind_target(-1, false);
                vk_rt(static_cast<uint32_t>(k.want), it.rt_w, it.rt_h);
            }
            if (it.rt_depth) {
                VkRt& r = g_vk_rt[static_cast<uint32_t>(k.want)];
                if (r.depth.image == VK_NULL_HANDLE || r.depth_w != r.w || r.depth_h != r.h) {
                    bind_target(-1, false);
                    vk_rt_depth(r);
                }
            }
        }
        VkImageView tex = VK_NULL_HANDLE, mips[3] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        if (k.tme) {
            if (k.tex.rt >= 0) {
                if (cur_rt == k.tex.rt) bind_target(-1, false);  // (sampled: not the target bound)
                tex = g_vk_rt[static_cast<uint32_t>(k.tex.rt)].color.view;
            } else if (k.tex.img != nullptr) {
                tex = vk_texture(k.tex.img);
            }
            for (int m = 0; m < k.mip_n && m < 3; ++m)
                if (k.mip[m].img != nullptr) mips[m] = vk_texture(k.mip[m].img);
        }
        float scr_map[4] = {0, 0, 0, 0};
        if (it.alone) {  // the screen under the primitive copied
            bind_target(-1, false);
            if (g_vk_screen.image == VK_NULL_HANDLE || g_vk_screen_w != win_w || g_vk_screen_h != win_h) {
                g_vk_screen_w = win_w;
                g_vk_screen_h = win_h;
                vk::create_image(static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h), VK_FORMAT_R8G8B8A8_UNORM, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                 VK_IMAGE_ASPECT_COLOR_BIT, g_vk_screen);
                vk::end_rendering();
                vk::transition_image(vk::cmd(), g_vk_screen, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            if (it.copy[2] > it.copy[0] && it.copy[3] > it.copy[1])
                vk::blit_image(vk::active_color_image(), it.copy[0], it.copy[1], it.copy[2], it.copy[3], g_vk_screen, it.copy[0], it.copy[1], it.copy[2], it.copy[3],
                               VK_FILTER_NEAREST, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            std::memcpy(scr_map, it.scr_map, sizeof scr_map);
        }
        bind_target(k.want, k.ztest);
        vk::ensure_rendering();
        if (k.to_rt) {
            const VkRt& r = g_vk_rt[static_cast<uint32_t>(k.want)];
            vk::set_viewport(0, 0, r.w, r.h);
        } else {
            vk::set_viewport(v.x, v.y, v.w, v.h);
        }
        vk::set_scissor(true, k.sc[0], k.sc[1], k.sc[2], k.sc[3]);
        const VkFilter mag_f = k.lin ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        const VkFilter mip_f = k.lin_min ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        const VkSamplerAddressMode ws = k.wrap_s ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        const VkSamplerAddressMode wt = k.wrap_t ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSampler s_main = vk::get_sampler(mag_f, mag_f, ws, wt);
        VkSampler s_mip = vk::get_sampler(mip_f, mip_f, ws, wt);
        VkSampler s_near = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        VkImageView v_main = (k.tme && tex != VK_NULL_HANDLE) ? tex : vk::white_texture().view;
        VkImageView v_scr = ((k.scr || k.dt) && g_vk_screen.view != VK_NULL_HANDLE) ? g_vk_screen.view : vk::white_texture().view;
        VkDescriptorImageInfo ii[5]{};
        ii[0] = {s_main, v_main, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        ii[1] = {s_near, v_scr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        for (int m = 0; m < 3; ++m) {
            VkImageView mv = (k.tme && m < k.mip_n && mips[m] != VK_NULL_HANDLE) ? mips[m] : vk::white_texture().view;
            ii[2 + m] = {s_mip, mv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        // (the frame's sets kept by what they bind; the transient pools are reset at each frame's start, the cache with them)
        static uint64_t ds_frame = ~0ull;
        static std::map<std::array<uint64_t, 10>, VkDescriptorSet> ds_cache;
        if (ds_frame != vk::frame_number()) {
            ds_frame = vk::frame_number();
            ds_cache.clear();
        }
        std::array<uint64_t, 10> ds_key{};
        for (int i = 0; i < 5; ++i) {
            ds_key[static_cast<size_t>(i) * 2] = reinterpret_cast<uint64_t>(ii[i].imageView);
            ds_key[static_cast<size_t>(i) * 2 + 1] = reinterpret_cast<uint64_t>(ii[i].sampler);
        }
        VkDescriptorSet& ds = ds_cache[ds_key];
        if (ds == VK_NULL_HANDLE) {
            ds = vk::alloc_descriptor_set(g_vk_set_layout);
            VkWriteDescriptorSet writes[5]{};
            for (uint32_t i = 0; i < 5; ++i) {
                writes[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
                writes[i].dstSet = ds;
                writes[i].dstBinding = i;
                writes[i].descriptorCount = 1;
                writes[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
                writes[i].pImageInfo = &ii[i];
            }
            vkUpdateDescriptorSets(vk::device(), 5, writes, 0, nullptr);
        }
        Push push{};
        if (k.to_rt) {
            const VkRt& r = g_vk_rt[static_cast<uint32_t>(k.want)];
            push.uFb[0] = static_cast<float>(r.w);
            push.uFb[1] = static_cast<float>(r.h);
            push.uRt = 1;
        } else {
            push.uFb[0] = FB_W;
            push.uFb[1] = FB_H;
            push.uRt = 0;
        }
        push.uScaleX = k.sx;
        push.uFogCol[0] = static_cast<float>(k.fogcol & 0xff) / 255.0f;
        push.uFogCol[1] = static_cast<float>((k.fogcol >> 8) & 0xff) / 255.0f;
        push.uFogCol[2] = static_cast<float>((k.fogcol >> 16) & 0xff) / 255.0f;
        push.uFge = k.fge;
        std::memcpy(push.uScrMap, scr_map, sizeof push.uScrMap);
        push.uTme = k.tme;
        push.uTfx = k.tfx;
        push.uTcc = k.tcc;
        push.uAte = k.ate;
        push.uAtst = k.atst;
        push.uAref = k.aref;
        push.uAfail = k.afail;
        push.uScrTex = k.scr;
        push.uDate = k.dt;
        push.uDatm = k.datm;
        push.uMipN = k.mip_n;
        push.uMipLo = k.mip_lo;
        push.uMipLcm = k.lcm;
        push.uMipMxl = k.mxl;
        push.uMipK = k.mip_k;
        push.uMipL = k.mip_l;
        const uint32_t m = k.fbmsk;
        const VkColorComponentFlags cmask = ((m & 0xffu) != 0xffu ? VK_COLOR_COMPONENT_R_BIT : 0) | ((m & 0xff00u) != 0xff00u ? VK_COLOR_COMPONENT_G_BIT : 0) |
                                            ((m & 0xff0000u) != 0xff0000u ? VK_COLOR_COMPONENT_B_BIT : 0) |
                                            ((m & 0xff000000u) != 0xff000000u ? VK_COLOR_COMPONENT_A_BIT : 0);
        const vk::RenderTarget& target = vk::current_target();
        PipeKey pk{};
        pk.topology = k.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pk.depth_test = k.ztest;
        pk.depth_write = k.zwrite;
        pk.depth_compare = k.ztst == 0 ? VK_COMPARE_OP_NEVER : k.ztst == 1 ? VK_COMPARE_OP_ALWAYS : k.ztst == 2 ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
        float fix = 0.0f;
        vk::GraphicsPipelineDesc bdesc{};
        gs::vk_blend_state(blend_of(k.blend, k.alpha), k.to_rt, bdesc, fix);
        pk.blend_enable = bdesc.blend_enable;
        pk.src_color = bdesc.src_color_blend;
        pk.dst_color = bdesc.dst_color_blend;
        pk.color_op = bdesc.color_blend_op;
        pk.color_mask = cmask;
        pk.has_depth = target.depth_view != VK_NULL_HANDLE;
        pk.has_stencil = target.stencil_view != VK_NULL_HANDLE;
        VkCommandBuffer cb = vk::cmd();
        const float bc[4] = {0.0f, 0.0f, 0.0f, fix};
        vkCmdSetBlendConstants(cb, bc);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_layout, 0, 1, &ds, 0, nullptr);
        vkCmdBindVertexBuffers(cb, 0, 1, &vbuf.buffer, &vbuf.offset);
        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;
        const bool split = !no_split && k.ztest && k.zwrite && k.ate != 0 && k.atst != 1 && k.afail != 0;
        if (split) {
            PipeKey pk_z = pk;
            pk_z.color_mask = 0;
            push.uPass = 1;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline(pk_z));
            vkCmdPushConstants(cb, g_vk_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            vkCmdDraw(cb, it.count, 1, it.first, 0);
            pk.depth_write = false;
            if (k.ztst != 0) pk.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
            push.uPass = 0;
        }
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vk_pipeline(pk));
        vkCmdPushConstants(cb, g_vk_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
        vkCmdDraw(cb, it.count, 1, it.first, 0);
    }
    bind_target(-1, false);
    vk::set_viewport(v.x, v.y, v.w, v.h);
    vk::set_scissor(false);
}

// the textures unused for this many calls forgotten (an image the game no longer draws: a texture whose memory it rewrote)
constexpr uint64_t kTexKeep = 600;

// orig: (host)
void evict() {
    if (g_frame_no % 64 != 0) return;
    for (auto it = g_gl_tex.begin(); it != g_gl_tex.end();) {
        if (g_frame_no - it->second.used > kTexKeep) {
            glDeleteTextures(1, &it->second.id);
            it = g_gl_tex.erase(it);
        } else {
            ++it;
        }
    }
    for (auto it = g_vk_tex.begin(); it != g_vk_tex.end();) {
        if (g_frame_no - it->second.used > kTexKeep) {
            vk::destroy_image(it->second.img);
            it = g_vk_tex.erase(it);
        } else {
            ++it;
        }
    }
}

}  // namespace

// orig: (host) see mrp_2d.h
void mrp_2d_draw(const MrpChainFrame& f, int win_w, int win_h, float fb_w, float fb_h, const gs::DepthMap* depth, const gs::Viewport* vp, size_t from, size_t to) {
    if (f.prims.empty() || from >= std::min(to, f.prims.size())) return;
    Viewport v;
    if (vp != nullptr) v = *vp;
    else { v.w = win_w; v.h = win_h; }
    ++g_frame_no;
    build(f, win_w, win_h, fb_w, fb_h, depth, v, from, to, g_built);
    if (g_built.items.empty()) return;
    if (vk::is_active()) vk_submit(g_built, win_w, win_h, fb_w, fb_h, v);
    else gl_submit(g_built, win_w, win_h, fb_w, fb_h, v);
    evict();
}

// orig: (host) see mrp_2d.h
size_t mrp_2d_clear_caches() {
    const size_t n = g_gl_tex.size() + g_vk_tex.size();
    for (auto& [id, t] : g_gl_tex) {
        glDeleteTextures(1, &t.id);
    }
    g_gl_tex.clear();
    for (auto& [id, t] : g_vk_tex) vk::destroy_image(t.img);
    g_vk_tex.clear();
    return n;
}

// orig: (host) see mrp_2d.h
size_t mrp_2d_texture_count() { return g_gl_tex.size() + g_vk_tex.size(); }

}  // namespace rac1
