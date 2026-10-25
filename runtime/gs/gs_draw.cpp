// See gs_draw.h.
#include <runtime/gs/gs_draw.h>
#include <profiler/host_profiler.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>

#include <array>

#include <algorithm>
#include <chrono>
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

// the GS's texture functions (colours as bytes / 255, 0x80 = 1.0) and alpha test
const char* FS = R"(
#version 330 core
in vec4 vColor;
in vec3 vTex;
in float vFog;
// a glyph's sampling kept inside its own cell (Viewport::sprite_clamp): s, t clamped to xy..zw (x > z: none)
flat in vec4 vClamp;
uniform sampler2D uTexture;
// the fogging (testing, RAC1_GS_FOG=1): PRIM FGE with FOGCOL and the vertices' F bytes
uniform int uFge;
uniform vec3 uFogCol;
uniform int uTme, uTfx, uTcc, uAte, uAtst, uAref, uAfail, uRt;
// a batch whose alpha test's fail mode writes one of colour / Z only (AFAIL FB_ONLY / RGB_ONLY / ZB_ONLY) with Z writes on is
// drawn twice: 0 the colour (Z not written), 1 the Z (colour masked): each pass keeps the pixels the GS writes that buffer for
uniform int uPass;
// the frame itself (a copy of the screen around the primitive): sampled as its texture (uScrTex,
// window position = uScrMap.xz + uScrMap.yw * texture coordinates) and for the destination alpha
// test (uDate, DATM uDatm: the pixel's alpha bit 7 = the screen's alpha 1.0)
uniform sampler2D uScreen;
uniform int uScrTex, uDate, uDatm;
// mipmapping: the level per pixel from the interpolated Q (TEX1 LCM / L / K, MXL), the texture of
// level uMipLo on uTexture, the next uMipN levels on uMip1..3
uniform sampler2D uMip1, uMip2, uMip3;
uniform int uMipN, uMipLo, uMipLcm, uMipMxl;
uniform float uMipK, uMipL;
uniform vec4 uScrMap;
layout(location = 0, index = 0) out vec4 FragColor;
// the blend's source alpha, the GS's 0x80 = 1.0 (a render target keeps the raw alpha in FragColor: its blend reads this one)
layout(location = 0, index = 1) out vec4 FragFactor;
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
        if (uScrTex != 0) {  // the screen's alpha back to the GS's (0x80 = 1.0 there)
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
    if (uFge != 0) {  // Cv F + FOGCOL (1 - F), F the byte / 255
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
        // a failed pixel: AFAIL 0 KEEP / 2 ZB_ONLY write no colour; 1 FB_ONLY / 3 RGB_ONLY do; only ZB_ONLY writes its Z
        if (!pass && uPass == 0 && (uAfail == 0 || uAfail == 2)) discard;
        if (!pass && uPass == 1 && uAfail != 2) discard;
    }
    // a render target keeps the GS's raw alpha (0x80 = 1), as the textures read from its memory
    FragColor = uRt != 0 ? o : vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
    FragFactor = vec4(min(o.a * (255.0 / 128.0), 1.0));
}
)";

GLuint g_prog = 0, g_vao = 0, g_vbo = 0;
// The GS's drawing into its own memory (a FRAME other than the screen's, sampled afterwards as
// a texture, e.g. the menus' render-to-texture): a GL framebuffer per frame base page.
struct RenderTarget {
    GLuint fbo = 0, tex = 0;
    int w = 0, h = 0;
    GLuint depth = 0;  // a depth buffer, attached when the host draws its 3D into the target (Viewport::host_3d)
    int depth_w = 0, depth_h = 0;
};
// the target's depth buffer, its size the target's (attached to its framebuffer; the framebuffer left bound)
void render_target_depth(RenderTarget& r) {
    if (r.depth == 0) glGenRenderbuffers(1, &r.depth);
    rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &r.depth, "GS replica: render targets");
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
std::map<uint32_t, RenderTarget> g_rt;
RenderTarget& render_target(uint32_t fbp, int w, int h) {
    RenderTarget& r = g_rt[fbp];
    if (r.fbo != 0 && r.w == w && r.h == h) return r;
    if (r.fbo == 0) {
        glGenFramebuffers(1, &r.fbo);
        glGenTextures(1, &r.tex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &r.tex, "GS replica: render targets");
    }
    r.w = w;
    r.h = h;
    glBindTexture(GL_TEXTURE_2D, r.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
    glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, r.tex, 0);
    if (r.depth != 0) render_target_depth(r);  // (the depth buffer resized with it, the framebuffer stays complete)
    glClearColor(0, 0, 0, 0);
    glClear(GL_COLOR_BUFFER_BIT);
    return r;
}
// A copy of the screen (the window) around the primitives that read the frame they draw into.
GLuint g_screen_copy = 0;
int g_screen_w = 0, g_screen_h = 0;
struct TexEntry {
    GLuint id = 0;
    uint64_t gen = ~0ull;
};
std::map<std::pair<uint64_t, uint64_t>, TexEntry> g_tex;

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::fprintf(stderr, "[gs_draw] shader: %s\n", log);
    }
    return s;
}

}  // namespace

uint32_t field(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

uint32_t texture_for(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img) {
    const uint64_t key0 = tex0 & 0x1fffffffffffffffull;  // CLD not part of the image
    TexEntry& e = g_tex[{key0, texa}];
    // the image the primitive sampled (its id), or the memory as it is now (its generation)
    const uint64_t ver = img != nullptr ? img->id : gs.generation() | (1ull << 63);
    if (e.id != 0 && e.gen == ver) return e.id;
    const int tw = 1 << std::min(10u, field(tex0, 26, 4)), th = 1 << std::min(10u, field(tex0, 30, 4));
    std::vector<uint32_t> px;
    if (img == nullptr) {
        px.resize(static_cast<size_t>(tw) * th);
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x) px[static_cast<size_t>(y) * tw + x] = gs.texel(tex0, texa, x, y);
    }
    if (e.id == 0) glGenTextures(1, &e.id);
    rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &e.id, "GS replica: textures");
    glBindTexture(GL_TEXTURE_2D, e.id);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, tw, th, 0, GL_RGBA, GL_UNSIGNED_BYTE, img != nullptr ? img->px.data() : px.data());
    e.gen = ver;
    return e.id;
}

namespace {
uint64_t g_stat_calls = 0, g_stat_prims = 0, g_stat_draws = 0, g_stat_stale = 0;
double g_stat_draw_ms = 0;  // RAC1_FRAME_PROF: the draw's primitives and GL draws
// true when texture_for() would upload (so it changes the texture the GL has under its id)
bool texture_stale(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img) {
    const uint64_t key0 = tex0 & 0x1fffffffffffffffull;
    const TexEntry& e = g_tex[{key0, texa}];
    const uint64_t ver = img != nullptr ? img->id : gs.generation() | (1ull << 63);
    return !(e.id != 0 && e.gen == ver);
}

struct V {
    float x, y;
    float r, g, b, a;
    float s, t, q;
    float d;  // window depth (0.5 without depth)
    float f;  // the vertex's fog byte (XYZF2's F)
    float c0 = 1.0f, c1 = 1.0f, c2 = 0.0f, c3 = 0.0f;  // a glyph's sampling clamp (s0, t0, s1, t1; s0 > s1: none)
};
struct DrawLocs {
    GLint fb = -1, rt = -1, scale_x = -1, fge = -1, fog_col = -1;
    GLint tme = -1, tfx = -1, tcc = -1, ate = -1, atst = -1, aref = -1, afail = -1, pass = -1;
    GLint scr_tex = -1, date = -1, datm = -1, scr_map = -1;
    GLint mip_n = -1, mip_lo = -1, mip_lcm = -1, mip_mxl = -1, mip_k = -1, mip_l = -1;
} g_loc;
}  // namespace

void blend_state(const DrawState& st, bool to_rt) {
    if (vk::is_active()) {
        if (field(st.prim, 6, 1)) {
            const float fix = std::min(static_cast<float>(field(st.alpha, 32, 8)) / 128.0f, 1.0f);
            const float bc[4] = {0.0f, 0.0f, 0.0f, fix};
            vkCmdSetBlendConstants(vk::cmd(), bc);
        }
        return;
    }
    if (!field(st.prim, 6, 1)) {
        glDisable(GL_BLEND);
        return;
    }
    glEnable(GL_BLEND);
    // ALPHA: Cv = (A - B) C + D, A / B / D one of Cs (0), Cd (1), 0 (2; 3 is reserved, read as 0), C one of As (0), Ad (1), FIX (2)
    const uint32_t A = field(st.alpha, 0, 2), B = field(st.alpha, 2, 2), C = field(st.alpha, 4, 2), D = field(st.alpha, 6, 2);
    const float fix = static_cast<float>(field(st.alpha, 32, 8)) / 128.0f;
    glBlendColor(0, 0, 0, std::min(fix, 1.0f));
    const GLenum cf = C == 2 ? GL_CONSTANT_ALPHA : (C == 1 ? GL_DST_ALPHA : GL_SRC_ALPHA);
    const GLenum cf1 = C == 2 ? GL_ONE_MINUS_CONSTANT_ALPHA : (C == 1 ? GL_ONE_MINUS_DST_ALPHA : GL_ONE_MINUS_SRC_ALPHA);
    // the factor of Cs and of Cd in that sum, k0 + k1 C (k0, k1 of -1, 0, 1), as a GL factor and its sign; every ALPHA but the two
    // that give a factor 1 + C (A = D, B = 0) maps exactly (those: 1). Before, the combinations without a case of their own were
    // drawn as (Cs - Cd) C + Cd: Orxon's O2 barriers' second pass (Cd - Cs) FIX, ALPHA 0xa1, was drawn as the source - opaque black
    auto factor = [&](uint32_t who, GLenum& f) {
        const int k0 = D == who ? 1 : 0, k1 = (A == who ? 1 : 0) - (B == who ? 1 : 0);
        if (k1 == 0) {
            f = k0 != 0 ? GL_ONE : GL_ZERO;
            return 1;
        }
        if (k0 == 0) {
            f = cf;
            return k1;
        }
        f = k1 < 0 ? cf1 : GL_ONE;  // (1 - C; 1 + C, not a GL factor: 1)
        return 1;
    };
    GLenum sf = GL_ZERO, df = GL_ZERO;
    const int ss = factor(0, sf), ds = factor(1, df);
    // (the GS blends the colour; the alpha written is the source's)
    if (ss >= 0 && ds >= 0) {
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
    } else if (ss < 0 && ds >= 0) {  // Cd df - Cs sf
        glBlendEquationSeparate(GL_FUNC_REVERSE_SUBTRACT, GL_FUNC_ADD);
    } else if (ss >= 0) {  // Cs sf - Cd df
        glBlendEquationSeparate(GL_FUNC_SUBTRACT, GL_FUNC_ADD);
    } else {  // (-(Cs + Cd) C: below 0, clamped to black)
        glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
        sf = df = GL_ZERO;
    }
    if (to_rt) {
        auto as_gs = [](GLenum f) -> GLenum {
            if (f == GL_SRC_ALPHA) return GL_SRC1_ALPHA;
            if (f == GL_ONE_MINUS_SRC_ALPHA) return GL_ONE_MINUS_SRC1_ALPHA;
            return f;
        };
        sf = as_gs(sf);
        df = as_gs(df);
    }
    glBlendFuncSeparate(sf, df, GL_ONE, GL_ZERO);
}

void vk_blend_state(const DrawState& st, bool to_rt, vk::GraphicsPipelineDesc& desc, float& out_fix) {
    out_fix = 0.0f;
    desc.src_alpha_blend = VK_BLEND_FACTOR_ONE;
    desc.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
    desc.alpha_blend_op = VK_BLEND_OP_ADD;
    if (!field(st.prim, 6, 1)) {
        desc.blend_enable = false;
        desc.src_color_blend = VK_BLEND_FACTOR_ONE;
        desc.dst_color_blend = VK_BLEND_FACTOR_ZERO;
        desc.color_blend_op = VK_BLEND_OP_ADD;
        return;
    }
    desc.blend_enable = true;
    const uint32_t A = field(st.alpha, 0, 2), B = field(st.alpha, 2, 2), C = field(st.alpha, 4, 2), D = field(st.alpha, 6, 2);
    out_fix = std::min(static_cast<float>(field(st.alpha, 32, 8)) / 128.0f, 1.0f);
    const VkBlendFactor cf = C == 2 ? VK_BLEND_FACTOR_CONSTANT_ALPHA : (C == 1 ? VK_BLEND_FACTOR_DST_ALPHA : VK_BLEND_FACTOR_SRC_ALPHA);
    const VkBlendFactor cf1 = C == 2 ? VK_BLEND_FACTOR_ONE_MINUS_CONSTANT_ALPHA : (C == 1 ? VK_BLEND_FACTOR_ONE_MINUS_DST_ALPHA : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA);
    auto factor = [&](uint32_t who, VkBlendFactor& f) {
        const int k0 = D == who ? 1 : 0, k1 = (A == who ? 1 : 0) - (B == who ? 1 : 0);
        if (k1 == 0) {
            f = k0 != 0 ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ZERO;
            return 1;
        }
        if (k0 == 0) {
            f = cf;
            return k1;
        }
        f = k1 < 0 ? cf1 : VK_BLEND_FACTOR_ONE;
        return 1;
    };
    VkBlendFactor sf = VK_BLEND_FACTOR_ZERO, df = VK_BLEND_FACTOR_ZERO;
    const int ss = factor(0, sf), ds = factor(1, df);
    if (ss >= 0 && ds >= 0) {
        desc.color_blend_op = VK_BLEND_OP_ADD;
    } else if (ss < 0 && ds >= 0) {
        desc.color_blend_op = VK_BLEND_OP_REVERSE_SUBTRACT;
    } else if (ss >= 0) {
        desc.color_blend_op = VK_BLEND_OP_SUBTRACT;
    } else {
        desc.color_blend_op = VK_BLEND_OP_ADD;
        sf = df = VK_BLEND_FACTOR_ZERO;
    }
    if (to_rt) {
        auto as_gs = [](VkBlendFactor f) -> VkBlendFactor {
            if (f == VK_BLEND_FACTOR_SRC_ALPHA) return VK_BLEND_FACTOR_SRC1_ALPHA;
            if (f == VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA) return VK_BLEND_FACTOR_ONE_MINUS_SRC1_ALPHA;
            return f;
        };
        sf = as_gs(sf);
        df = as_gs(df);
    }
    desc.src_color_blend = sf;
    desc.dst_color_blend = df;
}

namespace {

struct GsDrawPush {
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
static_assert(sizeof(GsDrawPush) == 128);

struct VkRenderTarget {
    vk::GpuImage color{};
    vk::GpuImage depth{};
    int w = 0, h = 0;
    int depth_w = 0, depth_h = 0;
};

struct VkTexEntry {
    vk::GpuImage img{};
    uint64_t gen = ~0ull;
};

struct VkPipeKey {
    VkPrimitiveTopology topology;
    bool depth_test;
    bool depth_write;
    VkCompareOp depth_compare;
    bool blend_enable;
    VkBlendFactor src_color;
    VkBlendFactor dst_color;
    VkBlendOp color_op;
    VkColorComponentFlags color_mask;
    bool has_depth;
    bool has_stencil;
    bool operator<(const VkPipeKey& o) const {
        return std::tie(topology, depth_test, depth_write, depth_compare, blend_enable, src_color, dst_color, color_op, color_mask, has_depth, has_stencil) <
               std::tie(o.topology, o.depth_test, o.depth_write, o.depth_compare, o.blend_enable, o.src_color, o.dst_color, o.color_op, o.color_mask, o.has_depth, o.has_stencil);
    }
};

VkShaderModule g_vk_vs = VK_NULL_HANDLE, g_vk_fs = VK_NULL_HANDLE;
VkDescriptorSetLayout g_vk_set_layout = VK_NULL_HANDLE;
VkPipelineLayout g_vk_pipe_layout = VK_NULL_HANDLE;
std::map<VkPipeKey, VkPipeline> g_vk_pipes;
std::map<uint32_t, VkRenderTarget> g_vk_rt;
std::map<std::pair<uint64_t, uint64_t>, VkTexEntry> g_vk_tex;
vk::GpuImage g_vk_screen_copy{};
int g_vk_screen_w = 0, g_vk_screen_h = 0;

void init_vk_draw() {
    if (g_vk_pipe_layout != VK_NULL_HANDLE) return;
    vk::register_shutdown_callback(vk_draw_shutdown);
    g_vk_vs = vk::create_shader_module(vk::shaders::kGsDrawVs, sizeof(vk::shaders::kGsDrawVs));
    g_vk_fs = vk::create_shader_module(vk::shaders::kGsDrawFs, sizeof(vk::shaders::kGsDrawFs));

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
    pcr.offset = 0;
    pcr.size = sizeof(GsDrawPush);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_vk_set_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    vkCreatePipelineLayout(vk::device(), &plci, nullptr, &g_vk_pipe_layout);
}

VkPipeline get_vk_pipeline(const VkPipeKey& key) {
    auto it = g_vk_pipes.find(key);
    if (it != g_vk_pipes.end()) return it->second;

    vk::GraphicsPipelineDesc desc{};
    desc.layout = g_vk_pipe_layout;
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

bool vk_texture_stale(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img) {
    const uint64_t key0 = tex0 & 0x1fffffffffffffffull;
    const VkTexEntry& e = g_vk_tex[{key0, texa}];
    const uint64_t ver = img != nullptr ? img->id : gs.generation() | (1ull << 63);
    return !(e.img.view != VK_NULL_HANDLE && e.gen == ver);
}

VkImageView vk_texture_for(const Gs& gs, uint64_t tex0, uint64_t texa, const TexImage* img) {
    const uint64_t key0 = tex0 & 0x1fffffffffffffffull;
    VkTexEntry& e = g_vk_tex[{key0, texa}];
    const uint64_t ver = img != nullptr ? img->id : gs.generation() | (1ull << 63);
    if (e.img.view != VK_NULL_HANDLE && e.gen == ver) return e.img.view;
    const int tw = 1 << std::min(10u, field(tex0, 26, 4)), th = 1 << std::min(10u, field(tex0, 30, 4));
    std::vector<uint32_t> px;
    if (img == nullptr) {
        px.resize(static_cast<size_t>(tw) * th);
        for (int y = 0; y < th; ++y)
            for (int x = 0; x < tw; ++x) px[static_cast<size_t>(y) * tw + x] = gs.texel(tex0, texa, x, y);
    }
    if (e.img.image == VK_NULL_HANDLE || e.img.width != static_cast<uint32_t>(tw) || e.img.height != static_cast<uint32_t>(th)) {
        vk::create_image(static_cast<uint32_t>(tw), static_cast<uint32_t>(th), VK_FORMAT_R8G8B8A8_UNORM,
                         VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, e.img);
    }
    const uint32_t* data = img != nullptr ? img->px.data() : px.data();
    vk::upload_image_2d(e.img, data, static_cast<size_t>(tw) * th * 4, 0, 0, tw, th);
    e.gen = ver;
    return e.img.view;
}

VkRenderTarget& vk_render_target(uint32_t fbp, int w, int h) {
    VkRenderTarget& r = g_vk_rt[fbp];
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
    return r;
}

void vk_render_target_depth(VkRenderTarget& r) {
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

void draw_vk(const Gs& gs, int win_w, int win_h, float FB_W, float FB_H, const DepthMap* depth, const Viewport* vp, int prim_filter, size_t from, size_t to) {
    init_vk_draw();
    Viewport v;
    if (vp != nullptr) v = *vp;
    else { v.w = win_w; v.h = win_h; }

    std::set<uint32_t> sampled;
    for (const Primitive& p : gs.primitives())
        if (field(p.st.prim, 4, 1) != 0 && field(p.st.tex0, 20, 6) == 0) sampled.insert(field(p.st.tex0, 0, 14));

    int cur_rt = -1;
    bool rt_pushed = false;
    auto bind_target = [&](int want, bool need_depth) {
        if (want == cur_rt && (!need_depth || want < 0 || g_vk_rt[static_cast<uint32_t>(want)].depth.image != VK_NULL_HANDLE)) return;
        if (rt_pushed) {
            vk::pop_target();
            if (cur_rt >= 0) {
                vk::transition_image(vk::cmd(), g_vk_rt[static_cast<uint32_t>(cur_rt)].color, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            rt_pushed = false;
        }
        if (want >= 0) {
            VkRenderTarget& r = g_vk_rt[static_cast<uint32_t>(want)];
            if (need_depth) vk_render_target_depth(r);
            vk::transition_image(vk::cmd(), r.color, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            if (r.depth.image != VK_NULL_HANDLE) {
                vk::transition_image(vk::cmd(), r.depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
            }
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

    struct VkKey {
        int want = -1;
        bool to_rt = false;
        float sx = 1.0f;
        bool lines = false;
        int fge = 0;
        std::uint32_t fogcol = 0;
        int tme = 0, tfx = 0, tcc = 0, ate = 0, atst = 0, aref = 0, afail = 0, datm = 0, scr = 0, dt = 0;
        std::uint32_t fbmsk = 0;
        VkImageView tex = VK_NULL_HANDLE;
        int lin = 0, wrap_s = 0, wrap_t = 0;
        int mip_n = 0, mip_lo = 0, lcm = 0, mxl = 0, lin_min = 0;
        float mip_k = 0.0f, mip_l = 0.0f;
        VkImageView mip_id[3] = {VK_NULL_HANDLE, VK_NULL_HANDLE, VK_NULL_HANDLE};
        bool blend = false;
        std::uint64_t alpha = 0;
        bool ztest = false, zwrite = false;
        int ztst = 0;
        int sc[4] = {0, 0, 0, 0};
        float scr_map[4] = {0, 0, 0, 0};
        bool operator==(const VkKey&) const = default;
    };

    std::vector<V> verts;
    std::vector<V> batch;
    VkKey cur{}, applied{};
    bool have = false;
    bool last_tex_valid = false;
    const TexImage* last_tex_img = nullptr;
    uint64_t last_tex0 = 0, last_texa = 0;
    VkImageView last_tex_id = VK_NULL_HANDLE;

    auto flush = [&] {
        if (batch.empty()) return;
        ++g_stat_draws;
        bind_target(applied.want, applied.ztest);
        vk::ensure_rendering();
        if (applied.to_rt) {
            const VkRenderTarget& r = g_vk_rt[static_cast<uint32_t>(applied.want)];
            vk::set_viewport(0, 0, r.w, r.h);
        } else {
            vk::set_viewport(v.x, v.y, v.w, v.h);
        }
        vk::set_scissor(true, applied.sc[0], applied.sc[1], applied.sc[2], applied.sc[3]);

        const size_t bytes = batch.size() * sizeof(V);
        vk::RingAlloc vbuf = vk::alloc_ring(bytes, 16);
        std::memcpy(vbuf.ptr, batch.data(), bytes);

        const VkFilter mag_f = applied.lin ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        const VkFilter mip_f = applied.lin_min ? VK_FILTER_LINEAR : VK_FILTER_NEAREST;
        const VkSamplerAddressMode ws = applied.wrap_s ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        const VkSamplerAddressMode wt = applied.wrap_t ? VK_SAMPLER_ADDRESS_MODE_REPEAT : VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE;
        VkSampler s_main = vk::get_sampler(mag_f, mag_f, ws, wt);
        VkSampler s_mip = vk::get_sampler(mip_f, mip_f, ws, wt);
        VkSampler s_near = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

        VkImageView v_main = (applied.tme && applied.tex != VK_NULL_HANDLE) ? applied.tex : vk::white_texture().view;
        VkImageView v_scr = ( (applied.scr || applied.dt) && g_vk_screen_copy.view != VK_NULL_HANDLE ) ? g_vk_screen_copy.view : vk::white_texture().view;
        VkDescriptorImageInfo ii[5]{};
        ii[0] = {s_main, v_main, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        ii[1] = {s_near, v_scr, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        for (int m = 0; m < 3; ++m) {
            VkImageView mv = (applied.tme && m < applied.mip_n && applied.mip_id[m] != VK_NULL_HANDLE) ? applied.mip_id[m] : vk::white_texture().view;
            ii[2 + m] = {s_mip, mv, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
        }
        // (the frame's sets kept by what they bind: a batch binding the same textures and samplers as one before reuses its set -
        // the transient pools are reset at each frame's start, the cache with them)
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

        GsDrawPush push{};
        if (applied.to_rt) {
            const VkRenderTarget& r = g_vk_rt[static_cast<uint32_t>(applied.want)];
            push.uFb[0] = static_cast<float>(r.w);
            push.uFb[1] = static_cast<float>(r.h);
            push.uRt = 1;
        } else {
            push.uFb[0] = FB_W;
            push.uFb[1] = FB_H;
            push.uRt = 0;
        }
        push.uScaleX = applied.sx;
        push.uFogCol[0] = static_cast<float>(applied.fogcol & 0xff) / 255.0f;
        push.uFogCol[1] = static_cast<float>((applied.fogcol >> 8) & 0xff) / 255.0f;
        push.uFogCol[2] = static_cast<float>((applied.fogcol >> 16) & 0xff) / 255.0f;
        push.uFge = applied.fge;
        std::memcpy(push.uScrMap, applied.scr_map, sizeof(push.uScrMap));
        push.uTme = applied.tme;
        push.uTfx = applied.tfx;
        push.uTcc = applied.tcc;
        push.uAte = applied.ate;
        push.uAtst = applied.atst;
        push.uAref = applied.aref;
        push.uAfail = applied.afail;
        push.uPass = 0;
        push.uScrTex = applied.scr;
        push.uDate = applied.dt;
        push.uDatm = applied.datm;
        push.uMipN = applied.mip_n;
        push.uMipLo = applied.mip_lo;
        push.uMipLcm = applied.lcm;
        push.uMipMxl = applied.mxl;
        push.uMipK = applied.mip_k;
        push.uMipL = applied.mip_l;

        DrawState dummy_st{};
        dummy_st.prim = applied.blend ? (1ull << 6) : 0ull;
        dummy_st.alpha = applied.alpha;
        vk::GraphicsPipelineDesc bdesc{};
        float fix = 0.0f;
        vk_blend_state(dummy_st, applied.to_rt, bdesc, fix);

        const uint32_t m = applied.fbmsk;
        const VkColorComponentFlags cmask =
            ((m & 0xffu) != 0xffu ? VK_COLOR_COMPONENT_R_BIT : 0) |
            ((m & 0xff00u) != 0xff00u ? VK_COLOR_COMPONENT_G_BIT : 0) |
            ((m & 0xff0000u) != 0xff0000u ? VK_COLOR_COMPONENT_B_BIT : 0) |
            ((m & 0xff000000u) != 0xff000000u ? VK_COLOR_COMPONENT_A_BIT : 0);

        const vk::RenderTarget& cur_target = vk::current_target();
        VkPipeKey pk{};
        pk.topology = applied.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        pk.depth_test = applied.ztest;
        pk.depth_write = applied.zwrite;
        pk.depth_compare = applied.ztst == 0 ? VK_COMPARE_OP_NEVER : applied.ztst == 1 ? VK_COMPARE_OP_ALWAYS : applied.ztst == 2 ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
        pk.blend_enable = bdesc.blend_enable;
        pk.src_color = bdesc.src_color_blend;
        pk.dst_color = bdesc.dst_color_blend;
        pk.color_op = bdesc.color_blend_op;
        pk.color_mask = cmask;
        pk.has_depth = cur_target.depth_view != VK_NULL_HANDLE;
        pk.has_stencil = cur_target.stencil_view != VK_NULL_HANDLE;

        VkCommandBuffer cb = vk::cmd();
        const float bc[4] = {0.0f, 0.0f, 0.0f, fix};
        vkCmdSetBlendConstants(cb, bc);
        vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_pipe_layout, 0, 1, &ds, 0, nullptr);
        vkCmdBindVertexBuffers(cb, 0, 1, &vbuf.buffer, &vbuf.offset);

        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;
        const bool split = !no_split && applied.ztest && applied.zwrite && applied.ate != 0 && applied.atst != 1 && applied.afail != 0;
        if (split) {
            VkPipeKey pk_z = pk;
            pk_z.color_mask = 0;
            push.uPass = 1;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_vk_pipeline(pk_z));
            vkCmdPushConstants(cb, g_vk_pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
            vkCmdDraw(cb, static_cast<uint32_t>(batch.size()), 1, 0, 0);

            pk.depth_write = false;
            if (applied.ztst != 0) pk.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
            push.uPass = 0;
        }
        vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, get_vk_pipeline(pk));
        vkCmdPushConstants(cb, g_vk_pipe_layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(push), &push);
        vkCmdDraw(cb, static_cast<uint32_t>(batch.size()), 1, 0, 0);
        batch.clear();
    };

    auto close_batch = [&] {
        flush();
        have = false;
    };

    const size_t prim_end = std::min(to, gs.primitives().size());
    int host_run_fbp = -1;
    for (size_t pi = from; pi < prim_end; ++pi) {
        const Primitive& p = gs.primitives()[pi];
        if (p.fx && gs.fx_external()) continue;
        if (p.type == 7) {
            if (prim_filter == 1 || v.screen_picture == nullptr || p.tex == nullptr) continue;
            close_batch();
            bind_target(-1, false);
            vk::set_scissor(false);
            v.screen_picture(*p.tex, v);
            cur_rt = -2;
            continue;
        }
        if (p.type == 8) {
            const uint32_t tfbp = field(p.st.frame, 0, 9);
            if (v.host_3d == nullptr || static_cast<int>(tfbp) == v.screen_fbp || sampled.count(tfbp * 32) == 0) {
                host_run_fbp = -1;
                continue;
            }
            close_batch();
            const int rw = std::max(64, static_cast<int>(field(p.st.frame, 16, 6)) * 64), rh = std::max(rw, 512);
            VkRenderTarget& r = vk_render_target(tfbp, rw, rh);
            vk_render_target_depth(r);
            bind_target(static_cast<int>(tfbp), true);
            vk::set_viewport(0, 0, r.w, r.h);
            vk::set_scissor(false);
            const bool first = host_run_fbp != static_cast<int>(tfbp);
            if (first) {
                vk::clear_attachments(false, nullptr, true, 1.0f, false, 0);
            }
            v.host_3d(p, r.w, r.h, first);
            host_run_fbp = static_cast<int>(tfbp);
            last_tex_valid = false;
            continue;
        }
        host_run_fbp = -1;
        if (prim_filter == 1 && !p.moby) continue;
        if (prim_filter == 2 && p.moby) continue;
        const DrawState& st = p.st;
        const bool tme = field(st.prim, 4, 1) != 0, fst = field(st.prim, 8, 1) != 0;
        const uint32_t ztst = field(st.test, 17, 2);
        const uint32_t fbp = field(st.frame, 0, 9);
        if (v.skip_fbp >= 0 && static_cast<int>(fbp) == v.skip_fbp) continue;
        const bool to_rt = static_cast<int>(fbp) != v.screen_fbp &&
                           (sampled.count(fbp * 32) != 0 || (v.rt_min_fbp >= 0 && static_cast<int>(fbp) >= v.rt_min_fbp));
        const bool scr_tex = tme && !to_rt && v.screen_fbp >= 0 && field(st.tex0, 0, 14) == static_cast<uint32_t>(v.screen_fbp) * 32 && field(st.tex0, 20, 6) == 0;
        const bool date = !to_rt && field(st.test, 14, 1) != 0;
        const bool rt_z = to_rt && field(st.test, 16, 1) != 0;
        const double rt_zs = field(st.zbuf, 24, 4) == 0 ? 1.0 / 4294967296.0 : field(st.zbuf, 24, 4) == 1 ? 1.0 / 16777216.0 : 1.0 / 65536.0;
        const bool ztest = rt_z || (depth != nullptr && !to_rt && field(st.test, 16, 1) != 0 && ztst != 1);
        const float tw = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
        const auto src_rt = tme ? g_vk_rt.find(field(st.tex0, 0, 14) / 32) : g_vk_rt.end();
        const bool from_rt = tme && !scr_tex && field(st.tex0, 0, 14) % 32 == 0 && src_rt != g_vk_rt.end() && sampled.count(field(st.tex0, 0, 14)) != 0;
        const float sw = from_rt ? static_cast<float>(src_rt->second.w) : tw, sh = from_rt ? static_cast<float>(src_rt->second.h) : th;
        auto mk = [&](const Vertex& vx) {
            V o{vx.x, vx.y, vx.r / 255.0f, vx.g / 255.0f, vx.b / 255.0f, vx.a / 255.0f, 0, 0, 1, 0.5f, static_cast<float>(vx.fog)};
            if (rt_z) o.d = static_cast<float>(std::clamp(1.0 - rt_zs * static_cast<double>(vx.z), 0.0, 1.0));
            else if (ztest) o.d = static_cast<float>(std::clamp(depth->a + depth->b * static_cast<double>(vx.z), 0.0, 1.0));
            if (fst) { o.s = vx.u / sw; o.t = vx.v / sh; o.q = 1.0f; }
            else { o.s = vx.s * tw / sw; o.t = vx.t * th / sh; o.q = vx.q != 0.0f ? vx.q : 1.0f; }
            return o;
        };
        const int want = to_rt ? static_cast<int>(fbp) : -1;
        if (to_rt) {
            const int rw = std::max(64, static_cast<int>(field(st.frame, 16, 6)) * 64);
            const int rh = std::max(rw, 512);
            const auto it = g_vk_rt.find(fbp);
            if (it == g_vk_rt.end() || it->second.color.image == VK_NULL_HANDLE || it->second.w != rw || it->second.h != rh) {
                close_batch();
                bind_target(-1, false);
            }
            vk_render_target(fbp, rw, rh);
            if (rt_z) {
                VkRenderTarget& r = g_vk_rt[fbp];
                if (r.depth.image == VK_NULL_HANDLE || r.depth_w != r.w || r.depth_h != r.h) {
                    close_batch();
                    bind_target(-1, false);
                    vk_render_target_depth(r);
                }
            }
        }
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
        if (p.sub != 0) {
            bool sub3d = p.vu;
            if (!sub3d && !to_rt && field(st.test, 16, 1) != 0 && ztst >= 2)
                for (const Vertex& q : p.v) sub3d = sub3d || (q.z != 0 && q.z < 0xff0000u);
            if ((p.sub == 1) != sub3d) continue;
        }
        const float sx = to_rt ? 1.0f : proj3d ? v.xscale_3d : v.xscale;
        verts.clear();
        if (p.type == 6) {
            const V a = mk(p.v[0]), b = mk(p.v[1]);
            V c0 = b, c1 = b, c2 = b, c3 = b;
            c0.x = a.x; c0.y = a.y; c0.s = a.s / a.q * b.q; c0.t = a.t / a.q * b.q;
            c1.y = a.y; c1.t = c0.t;
            c2.x = a.x; c2.s = c0.s;
            verts = {c0, c1, c2, c1, c3, c2};
        } else if (p.type == 3) {
            verts = {mk(p.v[0]), mk(p.v[1]), mk(p.v[2])};
        } else if (p.type == 1) {
            verts = {mk(p.v[0]), mk(p.v[1])};
        }
        if (verts.empty()) continue;

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
            if (rect) {
                const float sl = (std::round(s0) + 0.5f) / sw, sr = (std::round(s1) - 0.5f) / sw;
                const float tl = (std::round(t0) + 0.5f) / sh, tr = (std::round(t1) - 0.5f) / sh;
                for (V& q : verts) {
                    q.c0 = sl; q.c1 = tl; q.c2 = sr; q.c3 = tr;
                }
            }
        }

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

        bool to_left = false, to_right = false, full_span = false, edge_fill = false;
        if (!to_rt && p.layer && !proj3d && !p.vu && sx < 1.0f) {
            // (a full-screen 2D layer drawn in pieces: stretched across the window, as draw())
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
            const float far_x = tme ? FB_W : FB_W - 0.5f, far_y = tme ? FB_H : FB_H - 0.5f;  // (the far edge reached, as draw())
            full_span = (x0 <= 0.0f && x1 >= far_x) || (y0 <= 0.0f && y1 >= far_y);
            if (crop_on && !tme && (x0 <= 0.0f || x1 >= FB_W || y0 <= 0.0f || y1 >= FB_H)) edge_fill = true;
            if (full_span && sx < 1.0f && !proj3d) {  // (the 2D only, as draw())
                to_left = x0 <= 0.0f;
                to_right = x1 >= far_x;
                for (V& q : verts) {
                    if (q.x <= 0.0f) q.x = FB_W * 0.5f * (1.0f - 1.0f / sx);
                    else if (q.x >= far_x) q.x = FB_W * 0.5f * (1.0f + 1.0f / sx);
                }
            }
        }

        VkKey k{};
        k.want = want;
        k.to_rt = to_rt;
        k.sx = sx;
        k.lines = p.type == 1;
        static const bool s_fog = std::getenv("RAC1_GS_FOG") != nullptr;
        k.fge = s_fog && field(st.prim, 5, 1) != 0 ? 1 : 0;
        k.fogcol = k.fge != 0 ? static_cast<std::uint32_t>(st.fogcol & 0xffffffu) : 0;
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
            if (from_rt) {
                if (cur_rt == static_cast<int>(src_rt->first)) {
                    close_batch();
                    bind_target(-1, false);
                }
                k.tex = src_rt->second.color.view;
            } else if (last_tex_valid && p.tex.get() == last_tex_img && st.tex0 == last_tex0 && st.texa == last_texa) {
                k.tex = last_tex_id;
            } else {
                if (vk_texture_stale(gs, st.tex0, st.texa, p.tex.get())) {
                    ++g_stat_stale;
                    close_batch();
                }
                k.tex = vk_texture_for(gs, st.tex0, st.texa, p.tex.get());
                last_tex_valid = true;
                last_tex_img = p.tex.get();
                last_tex0 = st.tex0;
                last_texa = st.texa;
                last_tex_id = k.tex;
            }
            k.lin = field(st.tex1, 5, 1) != 0 ? 1 : 0;
            k.wrap_s = field(st.clamp, 0, 2) == 0 ? 1 : 0;
            k.wrap_t = field(st.clamp, 2, 2) == 0 ? 1 : 0;
            k.lin_min = field(st.tex1, 6, 3) >= 4 ? 1 : 0;
            k.mip_n = p.mip_n;
            for (int m = 0; m < p.mip_n; ++m) {
                if (vk_texture_stale(gs, p.mip_tex0[m], st.texa, p.mip_tex[m].get())) {
                    close_batch();
                }
                k.mip_id[m] = vk_texture_for(gs, p.mip_tex0[m], st.texa, p.mip_tex[m].get());
                last_tex_valid = false;
            }
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
        if (crop_on && !crop_3d && !p.vu && !p.wide && !full_span && !edge_fill) {
            const int cw = static_cast<int>(FB_W), ch = static_cast<int>(FB_H);
            x0 = std::max(x0, v.crop[0]);
            x1 = std::min(x1, cw - 1 - v.crop[1]);
            y0 = std::max(y0, v.crop[2]);
            y1 = std::min(y1, ch - 1 - v.crop[3]);
            if (x1 < x0 || y1 < y0) continue;
        }
        // (a HUD wider than 4:3, Viewport::hud_shift: as draw())
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

        const bool alone = scr_tex || date;
        if (alone || !have || !(k == cur)) {
            close_batch();
            if (scr_tex || date) {
                bind_target(-1, false);
                if (g_vk_screen_copy.image == VK_NULL_HANDLE || g_vk_screen_w != win_w || g_vk_screen_h != win_h) {
                    g_vk_screen_w = win_w;
                    g_vk_screen_h = win_h;
                    vk::create_image(static_cast<uint32_t>(win_w), static_cast<uint32_t>(win_h), VK_FORMAT_R8G8B8A8_UNORM,
                                     VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_vk_screen_copy);
                    vk::end_rendering();
                    vk::transition_image(vk::cmd(), g_vk_screen_copy, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
                const float bx = static_cast<float>(v.w) * 0.5f * sx * 2.0f / FB_W, ax = static_cast<float>(v.x) + 0.5f * static_cast<float>(v.w) * (1.0f - sx);
                const float by = -static_cast<float>(v.h) / FB_H, ay = static_cast<float>(v.y + v.h);
                float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
                for (const V& q : verts) {
                    bx0 = std::min(bx0, ax + bx * q.x);
                    bx1 = std::max(bx1, ax + bx * q.x);
                    by0 = std::min(by0, ay + by * q.y);
                    by1 = std::max(by1, ay + by * q.y);
                }
                const int cx0 = std::clamp(static_cast<int>(bx0) - 1, 0, win_w), cx1 = std::clamp(static_cast<int>(bx1) + 2, 0, win_w);
                const int cy0 = std::clamp(static_cast<int>(by0) - 1, 0, win_h), cy1 = std::clamp(static_cast<int>(by1) + 2, 0, win_h);
                if (cx1 > cx0 && cy1 > cy0) {
                    vk::blit_image(vk::active_color_image(), cx0, cy0, cx1, cy1, g_vk_screen_copy, cx0, cy0, cx1, cy1,
                                   VK_FILTER_NEAREST, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                }
                const float tw0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
                k.scr_map[0] = ax;
                k.scr_map[1] = bx * tw0;
                k.scr_map[2] = ay;
                k.scr_map[3] = by * th0;
            }
            cur = k;
            applied = k;
            have = !alone;
        }
        batch.insert(batch.end(), verts.begin(), verts.end());
        if (alone) close_batch();
    }
    close_batch();
    bind_target(-1, false);
    vk::set_viewport(v.x, v.y, v.w, v.h);
    vk::set_scissor(false);
}

}  // namespace

void vk_draw_shutdown() {
    for (auto& [k, p] : g_vk_pipes) {
        if (p != VK_NULL_HANDLE) vkDestroyPipeline(vk::device(), p, nullptr);
    }
    g_vk_pipes.clear();
    if (g_vk_pipe_layout != VK_NULL_HANDLE) {
        vkDestroyPipelineLayout(vk::device(), g_vk_pipe_layout, nullptr);
        g_vk_pipe_layout = VK_NULL_HANDLE;
    }
    if (g_vk_set_layout != VK_NULL_HANDLE) {
        vkDestroyDescriptorSetLayout(vk::device(), g_vk_set_layout, nullptr);
        g_vk_set_layout = VK_NULL_HANDLE;
    }
    if (g_vk_vs != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::device(), g_vk_vs, nullptr);
        g_vk_vs = VK_NULL_HANDLE;
    }
    if (g_vk_fs != VK_NULL_HANDLE) {
        vkDestroyShaderModule(vk::device(), g_vk_fs, nullptr);
        g_vk_fs = VK_NULL_HANDLE;
    }
    for (auto& [k, rt] : g_vk_rt) {
        vk::destroy_image(rt.color);
        vk::destroy_image(rt.depth);
    }
    g_vk_rt.clear();
    for (auto& [k, te] : g_vk_tex) {
        vk::destroy_image(te.img);
    }
    g_vk_tex.clear();
    vk::destroy_image(g_vk_screen_copy);
    g_vk_screen_w = g_vk_screen_h = 0;
}

void draw_init() {
    if (vk::is_active()) {
        init_vk_draw();
        return;
    }
    if (g_prog != 0) return;
    g_prog = glCreateProgram();
    glAttachShader(g_prog, compile(GL_VERTEX_SHADER, VS));
    glAttachShader(g_prog, compile(GL_FRAGMENT_SHADER, FS));
    glLinkProgram(g_prog);
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
    glUseProgram(g_prog);
    glUniform1i(glGetUniformLocation(g_prog, "uTexture"), 0);
    glUniform1i(glGetUniformLocation(g_prog, "uScreen"), 1);
    glUniform1i(glGetUniformLocation(g_prog, "uMip1"), 2);
    glUniform1i(glGetUniformLocation(g_prog, "uMip2"), 3);
    glUniform1i(glGetUniformLocation(g_prog, "uMip3"), 4);
    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_vbo, "GS replica: buffers");
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer(0, 2, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(0));
    glVertexAttribPointer(1, 4, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(2 * sizeof(float)));
    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(6 * sizeof(float)));
    glVertexAttribPointer(3, 1, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(9 * sizeof(float)));
    glVertexAttribPointer(4, 1, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(10 * sizeof(float)));
    glVertexAttribPointer(5, 4, GL_FLOAT, GL_FALSE, sizeof(V), reinterpret_cast<void*>(11 * sizeof(float)));
    glEnableVertexAttribArray(5);
    glEnableVertexAttribArray(3);
    glEnableVertexAttribArray(4);
    glEnableVertexAttribArray(0);
    glEnableVertexAttribArray(1);
    glEnableVertexAttribArray(2);
    glBindVertexArray(0);
}

void draw(const Gs& gs, int win_w, int win_h, float FB_W, float FB_H, const DepthMap* depth, const Viewport* vp, int prim_filter, size_t from, size_t to) {
    if (gs.primitives().empty() || from >= std::min(to, gs.primitives().size())) return;
    if (vk::is_active()) {
        draw_vk(gs, win_w, win_h, FB_W, FB_H, depth, vp, prim_filter, from, to);
        return;
    }
    const auto draw_t0 = std::chrono::steady_clock::now();
    draw_init();
    Viewport v;
    if (vp != nullptr) v = *vp;
    else { v.w = win_w; v.h = win_h; }
    GLint depth_func = GL_LESS;
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glUseProgram(g_prog);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glEnable(GL_SCISSOR_TEST);
    glActiveTexture(GL_TEXTURE0);
    const GLint lTme = g_loc.tme, lTfx = g_loc.tfx, lTcc = g_loc.tcc;
    const GLint lAte = g_loc.ate, lAtst = g_loc.atst, lAref = g_loc.aref;
    const GLint lAfail = g_loc.afail, lRt = g_loc.rt, lFb = g_loc.fb;
    const GLint lScaleX = g_loc.scale_x;
    const GLint lScrTex = g_loc.scr_tex, lDate = g_loc.date, lDatm = g_loc.datm;
    const GLint lScrMap = g_loc.scr_map;
    const GLint lMipN = g_loc.mip_n, lMipLo = g_loc.mip_lo;
    const GLint lMipLcm = g_loc.mip_lcm, lMipMxl = g_loc.mip_mxl;
    const GLint lMipK = g_loc.mip_k, lMipL = g_loc.mip_l;
    glUniform2f(lFb, FB_W, FB_H);
    glUniform1f(lScaleX, v.xscale);
    glViewport(v.x, v.y, v.w, v.h);
    GLint screen_fbo = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &screen_fbo);
    // the frame pages drawn into and then sampled (TEX0's base = FRAME's page * 32): render targets
    std::set<uint32_t> sampled;
    for (const Primitive& p : gs.primitives())
        if (field(p.st.prim, 4, 1) != 0 && field(p.st.tex0, 20, 6) == 0) sampled.insert(field(p.st.tex0, 0, 14));
    int cur_rt = -1;  // the target bound: -1 the screen (-2: not known, set again)
    std::vector<V> verts;
    // Consecutive primitives with the same GL state are one buffer upload and one draw: `key` holds
    // everything the loop sets before a primitive's draw, `batch` the vertices waiting for it. The
    // order of the primitives (and so of the blending) is the same; a primitive that copies the
    // screen (DATE / frame sampling) is drawn alone, a texture upload or a render target (re)creation
    // closes the batch before it happens (they would change what the waiting vertices draw with).
    struct Key {
        int want = -1;
        bool to_rt = false;
        float sx = 1.0f;
        bool lines = false;
        int fge = 0;
        std::uint32_t fogcol = 0;
        int tme = 0, tfx = 0, tcc = 0, ate = 0, atst = 0, aref = 0, afail = 0, datm = 0, scr = 0, dt = 0;
        std::uint32_t fbmsk = 0;
        GLuint tex = 0;
        int lin = 0, wrap_s = 0, wrap_t = 0;
        int mip_n = 0, mip_lo = 0, lcm = 0, mxl = 0, lin_min = 0;
        float mip_k = 0.0f, mip_l = 0.0f;
        GLuint mip_id[3] = {0, 0, 0};
        bool blend = false;
        std::uint64_t alpha = 0;
        bool ztest = false, zwrite = false;
        int ztst = 0;
        int sc[4] = {0, 0, 0, 0};
        bool operator==(const Key&) const = default;
    };
    std::vector<V> batch;
    Key cur, applied;  // the last primitive's key, and the one the GL state was last set to
    bool have = false;
    bool dirty = true;         // the GL state is not `applied`'s (set all of it)
    bool screen_bound = false;  // the screen copy is bound on texture unit 1
    // the last primitive's texture (its image, TEX0, TEXA) and the GL texture of it: the next primitive
    // with the same finds it without the lookups
    bool last_tex_valid = false;
    const TexImage* last_tex_img = nullptr;
    uint64_t last_tex0 = 0, last_texa = 0;
    GLuint last_tex_id = 0;
    GLenum batch_mode = GL_TRIANGLES;
    constexpr size_t kVboBytes = sizeof(V) * 131072;
    size_t vbo_off = kVboBytes;  // (a new buffer at the first draw)
    auto flush = [&] {
        if (batch.empty()) return;
        ++g_stat_draws;
        // the vertices appended to one big buffer (a new one when it is full), drawn from their offset
        const size_t bytes = batch.size() * sizeof(V);
        if (vbo_off + bytes > kVboBytes) {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(std::max(kVboBytes, bytes)), nullptr, GL_STREAM_DRAW);
            vbo_off = 0;
        }
        glBufferSubData(GL_ARRAY_BUFFER, static_cast<GLintptr>(vbo_off), static_cast<GLsizeiptr>(bytes), batch.data());
        // the alpha test's fail mode FB_ONLY / RGB_ONLY / ZB_ONLY with Z writes: the GS writes the colour and the Z of different
        // pixels (a failed pixel: the colour only, or the Z only), so the colour pass writes no Z and the Z pass no colour
        static const bool no_split = std::getenv("RAC1_GS_AFAIL_SPLIT") != nullptr && std::atoi(std::getenv("RAC1_GS_AFAIL_SPLIT")) == 0;  // (testing: the old way)
        const bool split = !no_split && applied.ztest && applied.zwrite && applied.ate != 0 && applied.atst != 1 && applied.afail != 0;
        // (the Z pass first, then the colour against it with LEQUAL: an opaque part's own back faces stay hidden as the GS's per-primitive Z
        // writes hide them; a pixel failing the test writes no Z, so what is drawn after it inside a translucent shell still shows)
        if (split) {
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glUniform1i(g_loc.pass, 1);
            glDrawArrays(batch_mode, static_cast<GLint>(vbo_off / sizeof(V)), static_cast<GLsizei>(batch.size()));
            glUniform1i(g_loc.pass, 0);
            const std::uint32_t m = applied.fbmsk;
            glColorMask((m & 0xffu) != 0xffu, (m & 0xff00u) != 0xff00u, (m & 0xff0000u) != 0xff0000u, (m & 0xff000000u) != 0xff000000u);
            glDepthMask(GL_FALSE);
            if (applied.ztst != 0) glDepthFunc(GL_LEQUAL);
        }
        glDrawArrays(batch_mode, static_cast<GLint>(vbo_off / sizeof(V)), static_cast<GLsizei>(batch.size()));
        if (split) {
            glDepthMask(GL_TRUE);
            glDepthFunc(applied.ztst == 0 ? GL_NEVER : applied.ztst == 1 ? GL_ALWAYS : applied.ztst == 2 ? GL_LEQUAL : GL_LESS);
        }
        vbo_off += bytes;
        batch.clear();
    };
    // the batch drawn and the state forgotten (what follows sets all of it again)
    auto close_batch = [&] {
        flush();
        have = false;
    };
    static const bool prim_dbg = std::getenv("RAC1_GS_PRIMDBG") != nullptr;  // testing: the frame's 3D primitives' extents
    if (prim_dbg) {
        static int frames = 0;
        if (++frames % 100 == 0) {
            int n3 = 0, huge = 0, shown = 0;
            for (const Primitive& p : gs.primitives()) {
                if (!p.vu || p.moby || p.type != 3) continue;
                ++n3;
                float x0 = 1e30f, x1 = -1e30f, y0 = 1e30f, y1 = -1e30f;
                for (const Vertex& q : p.v) { x0 = std::min(x0, q.x); x1 = std::max(x1, q.x); y0 = std::min(y0, q.y); y1 = std::max(y1, q.y); }
                if (x1 - x0 > 1500 || y1 - y0 > 1500) {
                    ++huge;
                    if (shown++ < 6)
                        std::fprintf(stderr, "[gs-dbg] huge tri (start %03x) x %.1f..%.1f y %.1f..%.1f z %u %u %u q %g %g %g\n", p.vu_start, x0, x1, y0, y1, p.v[0].z, p.v[1].z, p.v[2].z, p.v[0].q, p.v[1].q, p.v[2].q);
                }
            }
            std::fprintf(stderr, "[gs-dbg] frame %d: %d vu triangles, %d wider / taller than 1500 px\n", frames, n3, huge);
        }
    }
    const size_t prim_end = std::min(to, gs.primitives().size());
    int host_run_fbp = -1;  // the target of the run of type-8 primitives the last primitive belonged to (-1: none)
    for (size_t pi = from; pi < prim_end; ++pi) {
        const Primitive& p = gs.primitives()[pi];
        if (p.fx && gs.fx_external()) continue;  // (the MRP's)
        if (p.type == 7) {  // the frame's picture put back: drawn by the host, the GL state set again after it
            if (prim_filter == 1 || v.screen_picture == nullptr || p.tex == nullptr) continue;
            close_batch();
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            v.screen_picture(*p.tex, v);
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
            glEnable(GL_SCISSOR_TEST);
            glUseProgram(g_prog);
            cur_rt = -2;
            dirty = true;
            continue;
        }
        if (p.type == 8) {  // the host's 3D here (the MRP's mobys drawn into a render target: the menus' item models)
            const uint32_t tfbp = field(p.st.frame, 0, 9);
            if (v.host_3d == nullptr || static_cast<int>(tfbp) == v.screen_fbp || sampled.count(tfbp * 32) == 0) {
                host_run_fbp = -1;
                continue;
            }
            close_batch();
            const int rw = std::max(64, static_cast<int>(field(p.st.frame, 16, 6)) * 64), rh = std::max(rw, 512);
            RenderTarget& r = render_target(tfbp, rw, rh);
            render_target_depth(r);
            glViewport(0, 0, r.w, r.h);
            glDisable(GL_SCISSOR_TEST);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            const bool first = host_run_fbp != static_cast<int>(tfbp);
            if (first) {
                glDepthMask(GL_TRUE);
                glClearDepth(1.0);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            v.host_3d(p, r.w, r.h, first);
            host_run_fbp = static_cast<int>(tfbp);
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
            last_tex_valid = false;
            continue;
        }
        host_run_fbp = -1;
        if (prim_filter == 1 && !p.moby) continue;
        if (prim_filter == 2 && p.moby) continue;
        const DrawState& st = p.st;
        const bool tme = field(st.prim, 4, 1) != 0, fst = field(st.prim, 8, 1) != 0;
        // the Z test (TEST ZTE / ZTST, ZBUF ZMSK) on the host's depth
        const uint32_t ztst = field(st.test, 17, 2);
        const uint32_t fbp = field(st.frame, 0, 9);
        if (v.skip_fbp >= 0 && static_cast<int>(fbp) == v.skip_fbp) continue;
        // (the frame's own page is the screen even when it is sampled: the drop shadows' alpha)
        // also a page at or above Viewport::rt_min_fbp that this frame does not sample: the GS draws there, unseen (a menu
        // widget's texture whose draw shows nothing, e.g. the item panel of an item not owned: its clearing sprites went to the screen,
        // a dark box at its top left)
        const bool to_rt = static_cast<int>(fbp) != v.screen_fbp &&
                           (sampled.count(fbp * 32) != 0 || (v.rt_min_fbp >= 0 && static_cast<int>(fbp) >= v.rt_min_fbp));
        const bool scr_tex = tme && !to_rt && v.screen_fbp >= 0 && field(st.tex0, 0, 14) == static_cast<uint32_t>(v.screen_fbp) * 32 && field(st.tex0, 20, 6) == 0;
        const bool date = !to_rt && field(st.test, 14, 1) != 0;
        // a render target's Z test (ALWAYS too: the clear of its Z) on its own depth buffer: window depth = 1 - Z / 2^bits of ZBUF's
        // format (Z32 / Z24 / Z16), the GS's GEQUAL / GREATER as LEQUAL / LESS (the menus' item models drawn into their panels' textures)
        const bool rt_z = to_rt && field(st.test, 16, 1) != 0;
        const double rt_zs = field(st.zbuf, 24, 4) == 0 ? 1.0 / 4294967296.0 : field(st.zbuf, 24, 4) == 1 ? 1.0 / 16777216.0 : 1.0 / 65536.0;
        const bool ztest = rt_z || (depth != nullptr && !to_rt && field(st.test, 16, 1) != 0 && ztst != 1);
        const float tw = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
        const auto src_rt = tme ? g_rt.find(field(st.tex0, 0, 14) / 32) : g_rt.end();
        const bool from_rt = tme && !scr_tex && field(st.tex0, 0, 14) % 32 == 0 && src_rt != g_rt.end() && sampled.count(field(st.tex0, 0, 14)) != 0;
        // texel units of the sampled texture: a render target is larger than the TEX0 size
        const float sw = from_rt ? static_cast<float>(src_rt->second.w) : tw, sh = from_rt ? static_cast<float>(src_rt->second.h) : th;
        auto mk = [&](const Vertex& v) {
            V o{v.x, v.y, v.r / 255.0f, v.g / 255.0f, v.b / 255.0f, v.a / 255.0f, 0, 0, 1, 0.5f, static_cast<float>(v.fog)};
            if (rt_z) o.d = static_cast<float>(std::clamp(1.0 - rt_zs * static_cast<double>(v.z), 0.0, 1.0));
            else if (ztest) o.d = static_cast<float>(std::clamp(depth->a + depth->b * static_cast<double>(v.z), 0.0, 1.0));
            if (fst) { o.s = v.u / sw; o.t = v.v / sh; o.q = 1.0f; }
            else { o.s = v.s * tw / sw; o.t = v.t * th / sh; o.q = v.q != 0.0f ? v.q : 1.0f; }
            return o;
        };
        const int want = to_rt ? static_cast<int>(fbp) : -1;
        int rw = 0, rh = 0;
        if (to_rt) {
            rw = std::max(64, static_cast<int>(field(st.frame, 16, 6)) * 64);
            rh = std::max(rw, 512);
            const auto it = g_rt.find(fbp);
            if (it == g_rt.end() || it->second.fbo == 0 || it->second.w != rw || it->second.h != rh) {
                close_batch();  // (re)creating the target: it binds and clears it
                cur_rt = -2;
                dirty = true;
            }
            render_target(fbp, rw, rh);
            if (rt_z) {  // its depth buffer
                RenderTarget& r = g_rt[fbp];
                if (r.depth == 0 || r.depth_w != r.w || r.depth_h != r.h) {
                    close_batch();
                    render_target_depth(r);
                    cur_rt = -2;
                    dirty = true;
                }
            }
        }
        // widescreen: the game's projected 3D over the window, the 2D at the frame's 4:3
        // the overscan crop asked: the projected 3D found in 4:3 too (it is not cut, only the 2D is)
        const bool crop_on = !to_rt && (v.crop[0] | v.crop[1] | v.crop[2] | v.crop[3]) != 0;
        bool proj3d = false;
        if (!to_rt && (v.xscale_3d != v.xscale || v.xscale < 1.0f || crop_on || v.hide_edge_slivers)) {  // (also to let the 3D past the frame's sides)
            proj3d = p.vu;
            if (!proj3d && depth != nullptr && field(st.test, 16, 1) != 0 && ztst >= 2)
                for (const Vertex& q : p.v) {
                    const double d = depth->a + depth->b * static_cast<double>(q.z);
                    if (q.z != 0 && d > 0.0 && d < 1.0) proj3d = true;
                }
        }
        const bool crop_3d = proj3d;  // (the 3D the crop leaves alone)
        if (!(v.xscale_3d != v.xscale || v.xscale < 1.0f)) proj3d = false;  // (the widescreen's 3D scale: as before the crop)
        if (p.sub != 0) {  // (a substituted range, see Primitive::sub: the substitute's projected 3D, the real range's 2D)
            // (the 2D the game draws Z-tested sits at Z 0xfffff0, in front of everything: "projected" above, but 2D here)
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
        } else if (p.type == 1) {
            verts = {mk(p.v[0]), mk(p.v[1])};
        }
        if (verts.empty()) continue;
        // (the modern view, Viewport::sprite_clamp) a glyph's bilinear sampling kept inside its own cell of the font's texture. At the
        // GS's own resolution a glyph drawn 1:1 samples its texels' centres and never reads past its cell; scaled up, its outer half
        // texel blends with the texels beyond it, the neighbouring glyphs' edges in the atlas: thin lines under, over and through the
        // letters (issue #76; the CLAMP region 0x1fa648 sets includes the row and the column past the cell, and 0x1fa290 sets none).
        // The glyphs are triangle strips (two triangles, each with the cell's texture box) or sprites: a rectangle on the screen whose
        // texture coordinates follow x and y apart (u from x, v from y), so its texture box is the cell it shows, kept whole.
        if (v.sprite_clamp && tme && !scr_tex && p.mip_n == 0 && !p.vu && (p.type == 6 || (p.type == 3 && fst))) {
            bool rect = p.type == 6;
            if (!rect) {  // a triangle: its corners on its screen box's corners, u the same where x is, v where y is (half of a rectangle)
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
            // the box in texels (s / q of the sampled texture)
            float s0 = 1e30f, s1 = -1e30f, t0 = 1e30f, t1 = -1e30f;
            for (const V& q : verts) {
                s0 = std::min(s0, q.s / q.q * sw);
                s1 = std::max(s1, q.s / q.q * sw);
                t0 = std::min(t0, q.t / q.q * sh);
                t1 = std::max(t1, q.t / q.q * sh);
            }
            // only a font's glyph cell: 16 x 16 texels (24 x 16, the wide glyphs: the buttons' symbols) at whole texels, as both glyph
            // drawers make them (0x1fa648, 0x1fa290). The other 2D (the menus' panels and their highlights, pictures, the HUD) keeps the
            // GS's sampling: there the texels past a piece's edge are the rest of the same picture, and clamped, the panels' edges changed
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
        // a 2D sprite / triangle that covers none of the GS's sample points draws nothing on the GS: it samples each pixel at its
        // integer coordinates (pixel x covered when x0 <= x < x1, the "top-left" rule), GL at the pixel's centre (x + 0.5), so a
        // primitive whose part inside the scissor is within [511.5, 512) - the oxygen indicator's frame parked off the frame at x = 512,
        // less the HUD's half-pixel offset (issue #5) - lit the window's last column on every frame. Such a primitive is not drawn, as on
        // the GS.
        if (!p.vu && !p.wide && !crop_3d && (p.type == 6 || p.type == 3)) {
            float bx0 = p.v[0].x, bx1 = bx0, by0 = p.v[0].y, by1 = by0;
            for (size_t i = 1; i < (p.type == 6 ? 2u : 3u); ++i) {
                bx0 = std::min(bx0, p.v[i].x);
                bx1 = std::max(bx1, p.v[i].x);
                by0 = std::min(by0, p.v[i].y);
                by1 = std::max(by1, p.v[i].y);
            }
            // (inside the scissor: what lies past it is not drawn either)
            bx0 = std::max(bx0, static_cast<float>(field(st.scissor, 0, 11)));
            bx1 = std::min(bx1, static_cast<float>(field(st.scissor, 16, 11) + 1));
            by0 = std::max(by0, static_cast<float>(field(st.scissor, 32, 11)));
            by1 = std::min(by1, static_cast<float>(field(st.scissor, 48, 11) + 1));
            if (std::ceil(bx0) >= bx1 || std::ceil(by0) >= by1) continue;
        }
        // the 2D over the frame's whole width or height (a fade, a darkening in strips): its
        // vertices at the frame's edges moved to the window's. A flat (untextured) fill is "whole" as the GS fills it: every
        // column / row of the frame covered (pixel x drawn when x0 <= x < x1), so a fill drawn with the HUD's half-pixel offset
        // (quad_fill 0x1fa0e0: x -0.5 .. 511.5; the Visibomb's green screen and scan lines 0x1f9df8) is one too; a textured
        // one only when it reaches the edges themselves (a picture - the level exit's card banners at -0.5 .. 511.5, their text -
        // keeps its 4:3 shape)
        bool to_left = false, to_right = false, full_span = false, edge_fill = false;
        if (!to_rt && p.layer && !proj3d && !p.vu && sx < 1.0f) {
            // a full-screen 2D layer the game draws in pieces over its 4:3 frame (Primitive::layer: the Visibomb's
            // out-of-range static, a grid of sprites past the frame's edges): stretched about the frame's centre to the window's
            // width, its pieces past the frame's sides drawn there (the scissor to the window's edges)
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
            const float far_x = tme ? FB_W : FB_W - 0.5f, far_y = tme ? FB_H : FB_H - 0.5f;  // (the far edge reached)
            full_span = (x0 <= 0.0f && x1 >= far_x) || (y0 <= 0.0f && y1 >= far_y);
            // a flat (untextured) fill reaching the frame's edge: a part of a fill covering the screen in pieces (a
            // transition's wipe, Kerwan's talks), never cut by the crop (the band would show through)
            if (crop_on && !tme && (x0 <= 0.0f || x1 >= FB_W || y0 <= 0.0f || y1 >= FB_H)) edge_fill = true;
            // (not the projected 3D: its vertices are where the projection put them, past the frame's sides as far as the guard band;
            // moved to the window's edges, a clipped triangle crossing the whole frame lost a wedge - Rilgar's water near the camera,
            // its fans clipped at the guard band: a dark wedge across the water in a widened view, the sea bed seen through it)
            if (full_span && sx < 1.0f && !proj3d) {
                to_left = x0 <= 0.0f;
                to_right = x1 >= far_x;
                for (V& q : verts) {
                    if (q.x <= 0.0f) q.x = FB_W * 0.5f * (1.0f - 1.0f / sx);
                    else if (q.x >= far_x) q.x = FB_W * 0.5f * (1.0f + 1.0f / sx);
                }
            }
        }
        // the textures (a stale one is uploaded now: the waiting batch is drawn first)
        Key k;
        k.want = want;
        k.to_rt = to_rt;
        k.sx = sx;
        k.lines = p.type == 1;
        static const bool s_fog = std::getenv("RAC1_GS_FOG") != nullptr;  // (testing: the GS's fogging, off by default as it always was)
        k.fge = s_fog && field(st.prim, 5, 1) != 0 ? 1 : 0;
        k.fogcol = k.fge != 0 ? static_cast<std::uint32_t>(st.fogcol & 0xffffffu) : 0;
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
        k.fbmsk = field(st.frame, 32, 32);  // FBMSK: the channels kept (whole bytes)
        if (tme) {
            if (from_rt) {
                k.tex = src_rt->second.tex;
            } else if (last_tex_valid && p.tex.get() == last_tex_img && st.tex0 == last_tex0 && st.texa == last_texa) {
                k.tex = last_tex_id;  // (the previous primitive's texture: as it was, nothing changed it since)
            } else {
                if (texture_stale(gs, st.tex0, st.texa, p.tex.get())) {
                    ++g_stat_stale;
                    close_batch();
                    dirty = true;  // (the upload binds the texture on the unit)
                }
                k.tex = texture_for(gs, st.tex0, st.texa, p.tex.get());
                last_tex_valid = true;
                last_tex_img = p.tex.get();
                last_tex0 = st.tex0;
                last_texa = st.texa;
                last_tex_id = k.tex;
            }
            k.lin = field(st.tex1, 5, 1) != 0 ? 1 : 0;
            k.wrap_s = field(st.clamp, 0, 2) == 0 ? 1 : 0;
            k.wrap_t = field(st.clamp, 2, 2) == 0 ? 1 : 0;
            k.lin_min = field(st.tex1, 6, 3) >= 4 ? 1 : 0;
            k.mip_n = p.mip_n;
            for (int m = 0; m < p.mip_n; ++m) {
                if (texture_stale(gs, p.mip_tex0[m], st.texa, p.mip_tex[m].get())) {
                    close_batch();
                    dirty = true;
                }
                k.mip_id[m] = texture_for(gs, p.mip_tex0[m], st.texa, p.mip_tex[m].get());
                last_tex_valid = false;  // (this may have replaced an entry the cache stands for)
            }
            // (a texture bound by the uploads above is not the batch's: the state is set again)
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
        // the overscan crop: the 2D's scissor narrowed to the area a TV showed (nothing drawn when it is outside)
        if (crop_on && !crop_3d && !p.vu && !p.wide && !full_span && !edge_fill) {
            const int cw = static_cast<int>(FB_W), ch = static_cast<int>(FB_H);
            // (testing, RAC1_2D_EDGES=<px>: the 2D the crop applies to that comes within <px> of the frame's edges, its box inside the
            // scissor; how the crop's margins were chosen)
            static const int s_edges = std::getenv("RAC1_2D_EDGES") != nullptr ? std::atoi(std::getenv("RAC1_2D_EDGES")) : -1;
            if (s_edges >= 0) {
                float bx0 = verts[0].x, bx1 = bx0, by0 = verts[0].y, by1 = by0;
                for (const V& q : verts) {
                    bx0 = std::min(bx0, q.x);
                    bx1 = std::max(bx1, q.x);
                    by0 = std::min(by0, q.y);
                    by1 = std::max(by1, q.y);
                }
                bx0 = std::max(bx0, static_cast<float>(x0));
                bx1 = std::min(bx1, static_cast<float>(x1 + 1));
                by0 = std::max(by0, static_cast<float>(y0));
                by1 = std::min(by1, static_cast<float>(y1 + 1));
                if (bx1 > bx0 && by1 > by0 && (bx0 < s_edges || by0 < s_edges || bx1 > FB_W - s_edges || by1 > FB_H - s_edges))
                    std::printf("[2d-edge] type %d x %.1f..%.1f y %.1f..%.1f tex %d rgba %d %d %d %d\n", p.type, bx0, bx1, by0, by1, tme ? 1 : 0,
                                static_cast<int>(verts[0].r * 255), static_cast<int>(verts[0].g * 255), static_cast<int>(verts[0].b * 255),
                                static_cast<int>(verts[0].a * 255));
            }
            x0 = std::max(x0, v.crop[0]);
            x1 = std::min(x1, cw - 1 - v.crop[1]);
            y0 = std::max(y0, v.crop[2]);
            y1 = std::min(y1, ch - 1 - v.crop[3]);
            if (x1 < x0 || y1 < y0) continue;
        }
        // a HUD wider than 4:3 (Viewport::hud_shift): an element at the frame's left / right side moved toward the window's
        // edges, its scissor with it (the frame's edges become the wider HUD's: what slides out past them is cut there)
        float hud_dx = 0.0f;
        if (v.hud_shift != 0.0f && p.hud != 0 && !to_rt && !proj3d && !p.vu && !p.wide && !p.layer && !full_span) {
            const float k = sx * static_cast<float>(v.w) / FB_W;  // (window pixels a frame pixel: whole window pixels moved, the HUD as sharp)
            hud_dx = std::round(v.hud_shift * k) / k * (p.hud == 1 ? -1.0f : 1.0f);
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
            // the frame's edges cut what is beyond them (a 4:3 frame narrower than the window)
            auto wx = [&](float x) { return static_cast<float>(v.x) + ((std::clamp(x, 0.0f, FB_W) / FB_W * 2.0f - 1.0f) * sx + 1.0f) * 0.5f * static_cast<float>(v.w); };
            const float sy = static_cast<float>(v.h) / FB_H;
            // (a substituted chain's primitive, the widened view's particles: past the frame's sides to the window's)
            // (and any projected 3D: the GS 3D the game cut at the guard band, not at the 4:3 frame - glows, sparkles -)
            const bool wide = (p.wide || proj3d) && sx < 1.0f && !v.cut_3d;
            const int sx0 = (to_left || wide) && x0 <= 0 ? v.x : static_cast<int>(wx(static_cast<float>(x0)));
            const int sx1 = (to_right || wide) && static_cast<float>(x1 + 1) >= FB_W ? v.x + v.w : static_cast<int>(wx(static_cast<float>(x1 + 1)));
            k.sc[0] = sx0;
            k.sc[1] = static_cast<int>(static_cast<float>(v.y) + static_cast<float>(v.h) - static_cast<float>(y1 + 1) * sy);
            k.sc[2] = sx1 - sx0;
            k.sc[3] = static_cast<int>(static_cast<float>(y1 - y0 + 1) * sy);
        }
        const bool alone = scr_tex || date;  // the screen copy it reads is of this primitive's box
        if (alone || !have || !(k == cur)) {
            close_batch();
            // the GL state the key names: only what differs from the last primitive's is set (all of it
            // when `dirty`: the start, or after something outside it rebound or recreated a texture)
            auto chg = [&](auto m) { return dirty || k.*m != applied.*m; };            // the target
            if (want != cur_rt) {
                if (to_rt) {
                    const RenderTarget& r = g_rt[fbp];
                    glBindFramebuffer(GL_FRAMEBUFFER, r.fbo);
                    glViewport(0, 0, r.w, r.h);
                    glUniform2f(lFb, static_cast<float>(r.w), static_cast<float>(r.h));
                    glUniform1i(lRt, 1);
                } else {
                    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
                    glViewport(v.x, v.y, v.w, v.h);
                    glUniform2f(lFb, FB_W, FB_H);
                    glUniform1i(lRt, 0);
                }
                cur_rt = want;
            }
            if (chg(&Key::sx)) glUniform1f(lScaleX, sx);
            if (chg(&Key::tme)) glUniform1i(lTme, k.tme);
            if (chg(&Key::fge)) glUniform1i(g_loc.fge, k.fge);
            if (chg(&Key::fogcol)) glUniform3f(g_loc.fog_col, static_cast<float>(k.fogcol & 0xff) / 255.0f, static_cast<float>((k.fogcol >> 8) & 0xff) / 255.0f, static_cast<float>((k.fogcol >> 16) & 0xff) / 255.0f);
            if (chg(&Key::tfx)) glUniform1i(lTfx, k.tfx);
            if (chg(&Key::tcc)) glUniform1i(lTcc, k.tcc);
            if (chg(&Key::ate)) glUniform1i(lAte, k.ate);
            if (chg(&Key::atst)) glUniform1i(lAtst, k.atst);
            if (chg(&Key::aref)) glUniform1i(lAref, k.aref);
            if (chg(&Key::afail)) glUniform1i(lAfail, k.afail);
            if (chg(&Key::scr)) glUniform1i(lScrTex, k.scr);
            if (chg(&Key::dt)) glUniform1i(lDate, k.dt);
            if (chg(&Key::datm)) glUniform1i(lDatm, k.datm);
            if (scr_tex || date) {
                // the screen under the primitive copied (its window bounding box), sampled at the
                // window position its texture coordinates name (GS pixel -> window as the vertices)
                if (g_screen_copy == 0 || g_screen_w != win_w || g_screen_h != win_h) {
                    if (g_screen_copy == 0) glGenTextures(1, &g_screen_copy);
                    rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_screen_copy, "GS replica: screen copy");
                    g_screen_w = win_w;
                    g_screen_h = win_h;
                    glBindTexture(GL_TEXTURE_2D, g_screen_copy);
                    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, win_w, win_h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
                    // (testing, RAC1_GS_SCREEN_FILTER=1: the copy has no mip levels, so its default filter makes it an incomplete texture and the
                    // shader's texelFetch reads zeros: the drop shadows' alpha counter of the faithful path then never counts)
                    static const bool s_filter = std::getenv("RAC1_GS_SCREEN_FILTER") != nullptr;
                    if (s_filter) {
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                    }
                    dirty = true;  // (unit 0's texture is the copy now: the rest of the state is set again)
                    screen_bound = false;
                }
                const float bx = static_cast<float>(v.w) * 0.5f * sx * 2.0f / FB_W, ax = static_cast<float>(v.x) + 0.5f * static_cast<float>(v.w) * (1.0f - sx);
                const float by = -static_cast<float>(v.h) / FB_H, ay = static_cast<float>(v.y + v.h);
                float bx0 = 1e9f, bx1 = -1e9f, by0 = 1e9f, by1 = -1e9f;
                for (const V& q : verts) {
                    bx0 = std::min(bx0, ax + bx * q.x);
                    bx1 = std::max(bx1, ax + bx * q.x);
                    by0 = std::min(by0, ay + by * q.y);
                    by1 = std::max(by1, ay + by * q.y);
                }
                const int cx0 = std::clamp(static_cast<int>(bx0) - 1, 0, win_w), cx1 = std::clamp(static_cast<int>(bx1) + 2, 0, win_w);
                const int cy0 = std::clamp(static_cast<int>(by0) - 1, 0, win_h), cy1 = std::clamp(static_cast<int>(by1) + 2, 0, win_h);
                // (the copy stays bound on unit 1 between the primitives: nothing else uses that unit)
                glActiveTexture(GL_TEXTURE1);
                if (!screen_bound) {
                    glBindTexture(GL_TEXTURE_2D, g_screen_copy);
                    screen_bound = true;
                }
                if (cx1 > cx0 && cy1 > cy0) glCopyTexSubImage2D(GL_TEXTURE_2D, 0, cx0, cy0, cx0, cy0, cx1 - cx0, cy1 - cy0);
                glActiveTexture(GL_TEXTURE0);
                const float tw0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 26, 4))), th0 = static_cast<float>(1 << std::min(10u, field(st.tex0, 30, 4)));
                glUniform4f(lScrMap, ax, bx * tw0, ay, by * th0);
            }
            if (chg(&Key::fbmsk)) glColorMask((k.fbmsk & 0xffu) != 0xffu, (k.fbmsk & 0xff00u) != 0xff00u, (k.fbmsk & 0xff0000u) != 0xff0000u, (k.fbmsk & 0xff000000u) != 0xff000000u);
            if (tme) {
                if (chg(&Key::tex) || chg(&Key::lin) || chg(&Key::wrap_s) || chg(&Key::wrap_t)) {
                    glBindTexture(GL_TEXTURE_2D, k.tex);
                    const bool lin = k.lin != 0;
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lin ? GL_LINEAR : GL_NEAREST);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, k.wrap_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, k.wrap_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                }
                // the mip levels the pixels may pick (their own filter: MMIN's level filter)
                if (p.mip_n > 0 && (chg(&Key::mip_n) || chg(&Key::lin_min) || chg(&Key::wrap_s) || chg(&Key::wrap_t) || dirty || k.mip_id[0] != applied.mip_id[0] || k.mip_id[1] != applied.mip_id[1] || k.mip_id[2] != applied.mip_id[2])) {
                    const bool lin_min = k.lin_min != 0;
                    for (int m = 0; m < p.mip_n; ++m) {
                        glActiveTexture(GL_TEXTURE2 + m);
                        glBindTexture(GL_TEXTURE_2D, k.mip_id[m]);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, lin_min ? GL_LINEAR : GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lin_min ? GL_LINEAR : GL_NEAREST);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, k.wrap_s ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, k.wrap_t ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                    }
                    glActiveTexture(GL_TEXTURE0);
                }
            }
            if (chg(&Key::mip_n)) glUniform1i(lMipN, k.mip_n);
            if (chg(&Key::mip_lo)) glUniform1i(lMipLo, k.mip_lo);
            if (chg(&Key::lcm)) glUniform1i(lMipLcm, k.lcm);
            if (chg(&Key::mxl)) glUniform1i(lMipMxl, k.mxl);
            if (chg(&Key::mip_k)) glUniform1f(lMipK, k.mip_k);
            if (chg(&Key::mip_l)) glUniform1f(lMipL, k.mip_l);
            if (chg(&Key::blend) || chg(&Key::alpha) || chg(&Key::to_rt)) {
                blend_state(st, to_rt);
            }
            if (chg(&Key::ztest) || chg(&Key::ztst) || chg(&Key::zwrite)) {
                if (ztest) {
                    glEnable(GL_DEPTH_TEST);
                    glDepthFunc(ztst == 0 ? GL_NEVER : ztst == 1 ? GL_ALWAYS : ztst == 2 ? GL_LEQUAL : GL_LESS);  // (ALWAYS: a render target's)
                    glDepthMask(k.zwrite ? GL_TRUE : GL_FALSE);
                } else {
                    glDisable(GL_DEPTH_TEST);
                    glDepthMask(GL_FALSE);
                }
            }
            if (dirty || std::memcmp(k.sc, applied.sc, sizeof k.sc) != 0) glScissor(k.sc[0], k.sc[1], k.sc[2], k.sc[3]);
            batch_mode = k.lines ? GL_LINES : GL_TRIANGLES;
            cur = k;
            applied = k;
            dirty = false;
            have = !alone;
        }
        batch.insert(batch.end(), verts.begin(), verts.end());
        if (alone) close_batch();
    }
    close_batch();
    static const bool s_stats = std::getenv("RAC1_FRAME_PROF") != nullptr;
    if (s_stats) {
        g_stat_prims += gs.primitives().size();
        if (++g_stat_calls == 120) {
            std::printf("[prof] gs_draw per frame: %.0f primitives, %.0f draws, %.1f stale textures, whole draw() %.2f ms cpu\n", static_cast<double>(g_stat_prims) / 120.0, static_cast<double>(g_stat_draws) / 120.0, static_cast<double>(g_stat_stale) / 120.0, g_stat_draw_ms / 120.0);
            g_stat_stale = 0; g_stat_draw_ms = 0;
            g_stat_calls = g_stat_prims = g_stat_draws = 0;
        }
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    if (cur_rt != -1) glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen_fbo));
    glViewport(v.x, v.y, v.w, v.h);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glBlendEquation(GL_FUNC_ADD);
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(static_cast<GLenum>(depth_func));
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);
    g_stat_draw_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - draw_t0).count();
}

}  // namespace rac1::gs
