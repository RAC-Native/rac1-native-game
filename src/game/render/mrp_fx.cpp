// orig: (host) The MRP's particles (see mrp_fx.h).
#include <game/render/mrp_fx.h>
#include <game/render/mrp_particles.h>
#include <profiler/host_profiler.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>
#include <runtime/vu/vu1.h>

#include <glad/glad.h>

#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>

#include <algorithm>
#include <array>
#include <map>
#include <set>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace rac1 {

bool g_mrp_fx = [] {
    const char* e = std::getenv("RAC1_MRP_FX");
    return e == nullptr || std::atoi(e) != 0;
}();

bool g_mrp_seas = [] {
    const char* e = std::getenv("RAC1_MRP_SEAS");
    return e == nullptr || std::atoi(e) != 0;
}();

namespace {

const bool g_env_debug = std::getenv("RAC1_MRP_FX_DEBUG") != nullptr;
// (testing) every drawn vertex taken back to the GS's frame through the particle program's projection and compared with what it kicked
const bool g_env_verify = std::getenv("RAC1_MRP_FX_VERIFY") != nullptr;
const bool g_env_verify_log = std::getenv("RAC1_MRP_FX_VERIFY_LOG") != nullptr;
const bool g_env_hide = std::getenv("RAC1_MRP_FX_HIDE") != nullptr;
const bool g_env_states = std::getenv("RAC1_MRP_FX_STATES") != nullptr;  // (testing: the effects' GS states counted, reported)
std::map<uint64_t, uint64_t> g_states;  // (testing: the MRP's effects not drawn: what they change in the image)

uint32_t fld(uint64_t v, int lo, int n) { return static_cast<uint32_t>((v >> lo) & ((1ull << n) - 1)); }

// ---- the producers' ranges (game thread) ------------------------------------------------------------

// RAC1_MRP_FX_KINDS (testing): the producers the MRP takes (bit 1 << kind)
const uint32_t g_kinds_mask = [] {
    const char* e = std::getenv("RAC1_MRP_FX_KINDS");
    return e != nullptr ? static_cast<uint32_t>(std::strtoul(e, nullptr, 0)) : 0xffffffffu;
}();
const bool g_env_inventory = std::getenv("RAC1_MRP_FX_INVENTORY") != nullptr;

// VU1 program 7's constants as 0x1fc740 put them in the chain: the matrix (4 qwords, the point's world xyz 1 -> clip), the screen offset
// vf30, the Q numerator (the fans' tag's w)
struct Vu7 {
    float m[16] = {};
    float off[3] = {};
    float k = 0;
};
// A producer's range; `vu7` (kMrpFxPrimList) the constants it ran with; `vin` / `vn` the producer's world points (the vertex check)
struct Note {
    uint8_t kind = 0;
    uint32_t first = 0, end = 0;
    int32_t vu7 = -1;
    uint32_t vin = 0, vn = 0;
};
struct Notes {
    std::vector<Note> list;
    std::vector<Vu7> vu7;
    std::vector<glm::dvec3> vin;
    std::vector<std::array<uint32_t, 3>> lists;  // the draw lists' callbacks' ranges [first, end) and their moby class (kMrpFxVu7)
    int32_t vu7_cur = -1;
    void clear() {
        list.clear();
        vu7.clear();
        vin.clear();
        lists.clear();
        vu7_cur = -1;
    }
};
Notes g_real, g_shadow[kMrpLodSlots];  // (the shadow blocks' by slot, mrp_lod_shadow_slot)
bool g_window = false;
bool g_patched = false;
float g_saved[8] = {};
uint64_t g_covered = 0;  // (the inventory: bytes the notes covered so far)

Notes& notes_now() {
    const int k = mrp_lod_shadow_slot();
    return k >= 0 && k < kMrpLodSlots ? g_shadow[k] : g_real;
}

// A producer's projection of the frame drawn: the GS's (X, Y, Z) = off + (P (pos - base, 1)).xyz x k / w with Z's part x zscale; base the
// camera 0x166dc0 (or 0: VU1 program 7 takes world points)
struct View {
    bool ok = false;
    glm::dmat4 inv{1.0}, fwd{1.0};
    glm::dvec3 off{0.0}, base{0.0};
    double k = 1.0, zscale = 1.0;
};
// the frame's projections by kind (kMrpFxPrimList: per range, g_frame.views[kMrpFxKinds + vu7])
struct FrameNote {
    uint8_t kind = 0;
    uint32_t view = 0;
    uint32_t vin = 0, vn = 0;
};
struct Frame {
    std::vector<View> views;
    std::vector<FrameNote> notes;  // by range id
    std::vector<glm::dvec3> vin;
    std::array<glm::dvec4, 16> slots{};  // (the sparkles' check: the live slots' points, w 1; w 0 none)
    glm::dvec3 cam{0.0};                 // (the check: the camera 0x166dc0)
    // (kMrpFxVu7) the world -> screen matrix 0x1fc740 builds from the frame's view (0x166cc0 x 1024 on the point less the camera,
    // without its Z bias) and its inverse: a batch whose screen matrix is this one takes world points
    glm::dmat4 vu7_g{1.0}, vu7_ginv{1.0};
    glm::dmat4 vu7_ge{1.0}, vu7_geinv{1.0};  // (its x, y, z rows times its Q numerator 0x16cc50: the screen's (X, Y, Z) less the offset, times w)
    bool vu7 = false;
} g_frame;
std::array<glm::dvec4, 16> g_slots_real{}, g_slots_shadow{};

// ---- statistics -----------------------------------------------------------------------------------

struct Stats {
    uint64_t frames = 0, prims = 0, tris = 0, lines = 0, sprites = 0, batches = 0, draws = 0, verts = 0, gs_runs = 0;
    uint64_t by_kind[kMrpFxKinds] = {}, notes[kMrpFxKinds] = {}, no_view = 0;
    // (the producers' check, RAC1_MRP_FX_VERIFY) by kind: vertices checked, at a projected input point, on the input's surface only (the
    // clippers' new vertices), neither (verify_vertex); the largest Z difference of those passed
    uint64_t pv_verts[kMrpFxKinds] = {}, pv_at[kMrpFxKinds] = {}, pv_on[kMrpFxKinds] = {}, pv_bad[kMrpFxKinds] = {};
    double pv_max_plane[kMrpFxKinds] = {}, pv_max_rel[kMrpFxKinds] = {};
    uint64_t pv_world[kMrpFxKinds] = {};
    uint64_t tex_new = 0, tex_big = 0, tex_mip = 0, region = 0, fst = 0, fge = 0;
    uint64_t ver_verts = 0, ver_xy_bad = 0, ver_z_bad = 0;
    uint64_t kc_starts = 0, kc_kicks = 0, kc_qw = 0, kc_bad_starts = 0, kc_bad_qw = 0;
    double ver_max_xy = 0, ver_max_z = 0;
    double ms_build = 0, ms_gl = 0;
} g_st;

// ---- GL ---------------------------------------------------------------------------------------------

// One vertex: its world point, the STQ x its GL clip w (the GS's affine S / Q once divided), RGBA, F; flat: the atlas layer, the
// texture's size, the GS state word, FOGCOL
struct FV {
    float wp[4];
    float vu[3];  // the GS's X, Y (pixels), Z as kicked (the VU projection, g_mrp_world_projection 1: the image diff's reference mode)
    float stq[3];
    float fog;
    uint8_t rgba[4];
    uint32_t tex[4];  // layer, w | h << 16, flags, FOGCOL
};
static_assert(sizeof(FV) == 64);

const char* kVs = R"(#version 460 core
layout(location = 0) in vec4 aWorld;
layout(location = 1) in vec3 aStq;
layout(location = 2) in float aFog;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in uvec4 aTex;
layout(location = 5) in vec3 aVu;
uniform mat4 uVP;
uniform int uMode;    // 0 the world's GL projection (uVP), 1 the GS's frame (the replica's mapping)
uniform vec4 uFrame;  // fb width, fb height, x scale, 0
uniform vec4 uXyOff;  // the frame's centre X, Y (pixels), depth a, depth b
smooth out vec3 vStq;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFog;
flat out uvec4 vTex;
void main() {
    if (uMode == 0) {
        gl_Position = uVP * aWorld;
    } else {
        float sx = uFrame.z / (uFrame.x * 0.5), sy = 1.0 / (uFrame.y * 0.5);
        gl_Position = vec4((aVu.x - uXyOff.x) * sx, -(aVu.y - uXyOff.y) * sy, 2.0 * (uXyOff.z + uXyOff.w * aVu.z) - 1.0, 1.0);
    }
    vStq = aStq;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFog = aFog;
    vTex = aTex;
}
)";

// the GS's texture lookup (nearest / bilinear on the texel grid, REPEAT or CLAMP per axis, from the atlas layer), texture function,
// fog and alpha test, as the GS replica's fragment shader (gs_draw.cpp) does them
const char* kFs = R"(#version 460 core
smooth in vec3 vStq;
noperspective in vec4 vColor;
flat in vec4 vFlat;
noperspective in float vFog;
flat in uvec4 vTex;
uniform sampler2DArray uAtlas;
uniform int uShade;
uniform int uPass;  // 0 the colour, 1 the Z of a batch whose alpha test's fail mode writes one of them only (BKey::split)
// (the blending as dual-source factors, so that one draw holds the additive, the alpha-blended and the opaque primitives: blend kind
// 0 Cs, 1 Cs As + Cd, 2 Cs As + Cd (1 - As); 3 the batch's own GL blend state, gs::blend_state)
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 FragFactor;
ivec2 wrapc(ivec2 c, ivec2 sz, bool rs, bool rt) {
    c.x = rs ? ((c.x % sz.x) + sz.x) % sz.x : clamp(c.x, 0, sz.x - 1);
    c.y = rt ? ((c.y % sz.y) + sz.y) % sz.y : clamp(c.y, 0, sz.y - 1);
    return c;
}
void main() {
    uint fl = vTex.z;
    bool tme = (fl & 1u) != 0u, iip = ((fl >> 4) & 1u) != 0u, fge = ((fl >> 5) & 1u) != 0u, ate = ((fl >> 6) & 1u) != 0u;
    uint tfx = (fl >> 1) & 3u, tcc = (fl >> 3) & 1u, atst = (fl >> 7) & 7u, aref = (fl >> 10) & 0xffu, afail = (fl >> 22) & 3u;
    bool lin = ((fl >> 18) & 1u) != 0u, rs = ((fl >> 19) & 1u) != 0u, rt = ((fl >> 20) & 1u) != 0u;
    vec4 c = iip ? vColor : vFlat;
    vec4 o = c;
    if (tme) {
        ivec2 sz = ivec2(int(vTex.y & 0xffffu), int(vTex.y >> 16));
        vec2 uv = vStq.xy / vStq.z * vec2(sz);
        int layer = int(vTex.x);
        vec4 t;
        if (lin) {
            vec2 p = uv - 0.5;
            ivec2 i0 = ivec2(floor(p));
            vec2 f = p - vec2(i0);
            vec4 t00 = texelFetch(uAtlas, ivec3(wrapc(i0, sz, rs, rt), layer), 0);
            vec4 t10 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 0), sz, rs, rt), layer), 0);
            vec4 t01 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(0, 1), sz, rs, rt), layer), 0);
            vec4 t11 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 1), sz, rs, rt), layer), 0);
            t = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);
        } else {
            t = texelFetch(uAtlas, ivec3(wrapc(ivec2(floor(uv)), sz, rs, rt), layer), 0);
        }
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    o = clamp(o, 0.0, 1.0);
    if (fge) {
        uint fc = vTex.w;
        vec3 fogcol = vec3(float(fc & 0xffu), float((fc >> 8) & 0xffu), float((fc >> 16) & 0xffu)) / 255.0;
        float f = vFog / 255.0;
        o.rgb = o.rgb * f + fogcol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (ate) {
        bool pass = true;
        if (atst == 0u) pass = false;
        else if (atst == 2u) pass = a < float(aref);
        else if (atst == 3u) pass = a <= float(aref);
        else if (atst == 4u) pass = abs(a - float(aref)) < 0.5;
        else if (atst == 5u) pass = a >= float(aref);
        else if (atst == 6u) pass = a > float(aref);
        else if (atst == 7u) pass = abs(a - float(aref)) >= 0.5;
        // (a failed pixel: AFAIL KEEP / ZB_ONLY no colour, FB_ONLY / RGB_ONLY drawn; only ZB_ONLY writes its Z)
        if (!pass && uPass == 0 && (afail == 0u || afail == 2u)) discard;
        if (!pass && uPass == 1 && afail != 2u) discard;
    }
    float as = min(o.a * (255.0 / 128.0), 1.0);
    uint bk = (fl >> 24) & 3u;
    if (bk == 1u || bk == 2u) {
        FragColor = vec4(o.rgb * as, as);
        FragFactor = vec4(bk == 1u ? 1.0 : 1.0 - as);
    } else {
        FragColor = vec4(o.rgb, as);
        FragFactor = vec4(0.0);
    }
}
)";

GLuint g_prog = 0, g_vao = 0, g_vbo = 0;
GLint g_loc_vp = -1, g_loc_atlas = -1, g_loc_shade = -1, g_loc_mode = -1, g_loc_frame = -1, g_loc_xyoff = -1, g_loc_pass = -1;

struct FxVkPush {
    float uVP[16];
    float uFrame[4];
    float uXyOff[4];
    int32_t uMode;
    int32_t uShade;
    int32_t uPass;
    int32_t _pad = 0;
};
static_assert(sizeof(FxVkPush) == 112);

VkShaderModule g_vk_vs = VK_NULL_HANDLE, g_vk_fs = VK_NULL_HANDLE;
VkDescriptorSetLayout g_vk_set_layout = VK_NULL_HANDLE;
VkPipelineLayout g_vk_pipe_layout = VK_NULL_HANDLE;
VkSampler g_vk_sampler = VK_NULL_HANDLE;

GLuint compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp-fx] shader: %s\n", log);
    }
    return s;
}

// ---- the resident textures: an atlas of kLayer x kLayer layers, a texture in a layer's corner, by its pixels ------------------------

constexpr int kLayer = 256;
// The atlas is bounded: an effect whose texture the game writes anew every frame (Aridia's sand sea, L2:0x2a4818's texture upload: a
// 64 x 64 texture of new pixels each frame) took a new layer every frame, the atlas doubled without end (2048 layers after ~34 s,
// 4096 after ~68 s), and where the GL's GL_MAX_ARRAY_TEXTURE_LAYERS is 2048 (NVIDIA) the growth to 4096 failed: the array was left
// without storage and every effect drew untextured or black from then on - the glows, trails and smoke, the bolts, the enemies'
// shots, the sand sea itself (issue #19), until the game was restarted. Now the atlas grows up to kAtlasCap layers (or the GL's
// maximum, if lower), then reuses the layer least recently drawn with (never one drawn with this frame: its batches are pending).
constexpr int kAtlasCap = 1024;  // (256 MB at most)
constexpr size_t kIdsPerLayer = 64;  // (the decodes remembered per layer; past it they are found by their pixels' hash again)
struct Atlas {
    GLuint tex = 0;
    int layers = 0, used = 0, cap = 0;
    std::unordered_map<uint64_t, int> by_hash;  // the pixels' hash (and size) -> layer
    std::unordered_map<uint64_t, int> by_id;    // TexImage::id -> layer (the hash not taken again)
    std::vector<uint64_t> hash;                 // per layer: its texture's hash
    std::vector<std::vector<uint64_t>> ids;     // per layer: the decodes in by_id that point to it
    std::vector<uint64_t> last;                 // per layer: the frame it was last drawn with (g_st.frames)
    uint64_t reused = 0, full = 0, grow_failed = 0;
} g_atlas, g_vk_atlas;
vk::GpuImage g_vk_atlas_img;

void shutdown_vk_fx() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        if (g_vk_pipe_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g_vk_pipe_layout, nullptr);
        if (g_vk_set_layout != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(dev, g_vk_set_layout, nullptr);
        if (g_vk_vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_vs, nullptr);
        if (g_vk_fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_fs, nullptr);
    }
    g_vk_pipe_layout = VK_NULL_HANDLE;
    g_vk_set_layout = VK_NULL_HANDLE;
    g_vk_vs = VK_NULL_HANDLE;
    g_vk_fs = VK_NULL_HANDLE;
    g_vk_sampler = VK_NULL_HANDLE;
    vk::destroy_image_now(g_vk_atlas_img);
    g_vk_atlas = Atlas{};
}

bool init_vk() {
    if (g_vk_pipe_layout != VK_NULL_HANDLE) return true;
    VkDevice dev = vk::device();
    if (dev == VK_NULL_HANDLE) return false;
    g_vk_vs = vk::create_shader_module(vk::shaders::kMrpFxVs, sizeof(vk::shaders::kMrpFxVs));
    g_vk_fs = vk::create_shader_module(vk::shaders::kMrpFxFs, sizeof(vk::shaders::kMrpFxFs));
    if (g_vk_vs == VK_NULL_HANDLE || g_vk_fs == VK_NULL_HANDLE) return false;

    VkDescriptorSetLayoutBinding bind{};
    bind.binding = 0;
    bind.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    bind.descriptorCount = 1;
    bind.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;

    VkDescriptorSetLayoutCreateInfo slci{};
    slci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    slci.bindingCount = 1;
    slci.pBindings = &bind;
    if (vkCreateDescriptorSetLayout(dev, &slci, nullptr, &g_vk_set_layout) != VK_SUCCESS) return false;

    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(FxVkPush);

    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 1;
    plci.pSetLayouts = &g_vk_set_layout;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_pipe_layout) != VK_SUCCESS) return false;

    g_vk_sampler = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST,
                                   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    vk::register_shutdown_callback(shutdown_vk_fx);
    return true;
}

bool init_gl() {
    if (vk::is_active()) return init_vk();
    if (g_prog != 0) return true;
    const GLuint vs = compile(GL_VERTEX_SHADER, kVs), fs = compile(GL_FRAGMENT_SHADER, kFs);
    g_prog = glCreateProgram();
    glAttachShader(g_prog, vs);
    glAttachShader(g_prog, fs);
    glLinkProgram(g_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(g_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(g_prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp-fx] link: %s\n", log);
        glDeleteProgram(g_prog);
        g_prog = 0;
        return false;
    }
    g_loc_vp = glGetUniformLocation(g_prog, "uVP");
    g_loc_atlas = glGetUniformLocation(g_prog, "uAtlas");
    g_loc_shade = glGetUniformLocation(g_prog, "uShade");
    g_loc_mode = glGetUniformLocation(g_prog, "uMode");
    g_loc_pass = glGetUniformLocation(g_prog, "uPass");
    g_loc_frame = glGetUniformLocation(g_prog, "uFrame");
    g_loc_xyoff = glGetUniformLocation(g_prog, "uXyOff");
    glGenVertexArrays(1, &g_vao);
    glGenBuffers(1, &g_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_vbo, "MRP effects");
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    const GLsizei S = sizeof(FV);
    glVertexAttribPointer(0, 4, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(FV, wp)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(FV, stq)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(FV, fog)));
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(FV, rgba)));
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(FV, tex)));
    glVertexAttribPointer(5, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(FV, vu)));
    for (GLuint a = 0; a < 6; ++a) glEnableVertexAttribArray(a);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

uint64_t tex_hash(const gs::TexImage& t) {
    uint64_t h = 1469598103934665603ull ^ (static_cast<uint64_t>(t.w) << 32 | static_cast<uint32_t>(t.h));
    for (uint32_t p : t.px) h = (h ^ p) * 1099511628211ull;
    return h;
}

// orig: (host) the array grown to `n` layers, the used ones copied; false: the GL refused it (the old array kept)
bool atlas_grow(Atlas& A, int n) {
    if (vk::is_active()) {
        vk::GpuImage nt{};
        if (!vk::create_image(kLayer, kLayer, VK_FORMAT_R8G8B8A8_UNORM,
                              VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                              VK_IMAGE_ASPECT_COLOR_BIT, nt, static_cast<uint32_t>(n), 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY, 1)) {
            if (A.grow_failed++ == 0) std::fprintf(stderr, "[mrp-fx] the effects' texture atlas could not grow to %d layers: kept at %d\n", n, A.layers);
            return false;
        }
        auto copy_layers = [&](VkCommandBuffer cb) {
            vk::transition_image(cb, nt, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
            if (g_vk_atlas_img.image != VK_NULL_HANDLE && A.used > 0) {
                vk::transition_image(cb, g_vk_atlas_img, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                VkImageCopy region{};
                region.srcSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, static_cast<uint32_t>(A.used)};
                region.dstSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, static_cast<uint32_t>(A.used)};
                region.extent = {static_cast<uint32_t>(kLayer), static_cast<uint32_t>(kLayer), 1};
                vkCmdCopyImage(cb, g_vk_atlas_img.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                               nt.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, 1, &region);
                vk::transition_image(cb, g_vk_atlas_img, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
            }
            vk::transition_image(cb, nt, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        };
        if (vk::cmd() != VK_NULL_HANDLE) {
            vk::end_rendering();
            copy_layers(vk::cmd());
        } else {
            vk::submit_immediate(copy_layers);
        }
        vk::destroy_image(g_vk_atlas_img);
        g_vk_atlas_img = nt;
        A.tex = 1;
        A.layers = n;
        return true;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    GLuint nt = 0;
    glGenTextures(1, &nt);
    rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &nt, "MRP effects");
    glBindTexture(GL_TEXTURE_2D_ARRAY, nt);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kLayer, kLayer, n, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    if (glGetError() != GL_NO_ERROR) {
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glDeleteTextures(1, &nt);
        if (A.grow_failed++ == 0) std::fprintf(stderr, "[mrp-fx] the effects' texture atlas could not grow to %d layers: kept at %d\n", n, A.layers);
        return false;
    }
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_BASE_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAX_LEVEL, 0);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    if (A.tex != 0) {
        using PfnCopy = void(APIENTRYP)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
        static const PfnCopy copy = reinterpret_cast<PfnCopy>(glfwGetProcAddress("glCopyImageSubData"));
        if (copy != nullptr && A.used > 0) {
            copy(A.tex, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, nt, GL_TEXTURE_2D_ARRAY, 0, 0, 0, 0, kLayer, kLayer, A.used);
        } else if (copy == nullptr) {  // (no copy: the textures uploaded again as they come)
            A.by_hash.clear();
            A.by_id.clear();
            A.hash.clear();
            A.ids.clear();
            A.last.clear();
            A.used = 0;
        }
        glDeleteTextures(1, &A.tex);
    }
    A.tex = nt;
    A.layers = n;
    return true;
}

// orig: (host) layer `l` emptied for another texture: its hash and decodes forgotten
void atlas_evict(Atlas& A, int l) {
    if (auto it = A.by_hash.find(A.hash[static_cast<size_t>(l)]); it != A.by_hash.end() && it->second == l) A.by_hash.erase(it);
    for (uint64_t id : A.ids[static_cast<size_t>(l)]) A.by_id.erase(id);
    A.ids[static_cast<size_t>(l)].clear();
    ++A.reused;
}

// orig: (host) decode `id` of layer `l` remembered (a few per layer: a texture decoded again every frame finds its layer by its hash)
void atlas_note_id(Atlas& A, int l, uint64_t id) {
    std::vector<uint64_t>& ids = A.ids[static_cast<size_t>(l)];
    if (ids.size() >= kIdsPerLayer) {
        for (uint64_t old : ids) A.by_id.erase(old);
        ids.clear();
    }
    ids.push_back(id);
    A.by_id[id] = l;
}

// orig: (host) the layer holding texture `t` (uploaded at its first use; the atlas grown, its layers copied, up to its cap; then the
// least recently drawn layer reused); -1: too big, or every layer drawn with this frame
int atlas_layer(const gs::TexImage& t) {
    const bool use_vk = vk::is_active();
    Atlas& A = use_vk ? g_vk_atlas : g_atlas;
    const uint64_t now = g_st.frames;
    if (auto it = A.by_id.find(t.id); it != A.by_id.end()) {
        A.last[static_cast<size_t>(it->second)] = now;
        return it->second;
    }
    if (t.w > kLayer || t.h > kLayer || t.w <= 0 || t.h <= 0 || t.px.size() < static_cast<size_t>(t.w) * static_cast<size_t>(t.h)) {
        ++g_st.tex_big;
        return -1;
    }
    const uint64_t h = tex_hash(t);
    if (auto it = A.by_hash.find(h); it != A.by_hash.end()) {
        atlas_note_id(A, it->second, t.id);
        A.last[static_cast<size_t>(it->second)] = now;
        return it->second;
    }
    if (A.cap == 0) {
        if (use_vk) {
            A.cap = kAtlasCap;
        } else {
            GLint max_layers = 0;
            glGetIntegerv(GL_MAX_ARRAY_TEXTURE_LAYERS, &max_layers);
            A.cap = std::max(32, std::min(kAtlasCap, static_cast<int>(max_layers)));
        }
        if (const char* e = std::getenv("RAC1_MRP_FX_ATLAS_CAP"); e != nullptr && std::atoi(e) > 0) A.cap = std::atoi(e);  // (testing: a smaller cap)
    }
    int layer = -1;
    if (A.used == A.layers && A.layers < A.cap && !atlas_grow(A, std::min(A.cap, std::max(32, A.layers * 2)))) A.cap = A.layers;  // (refused: kept)
    if (A.used < A.layers) {
        layer = A.used++;
        A.hash.resize(static_cast<size_t>(A.used), 0);
        A.ids.resize(static_cast<size_t>(A.used));
        A.last.resize(static_cast<size_t>(A.used), 0);
    } else {  // full: the layer least recently drawn with, not this frame
        uint64_t oldest = now;
        for (int l = 0; l < A.used; ++l)
            if (A.last[static_cast<size_t>(l)] < oldest) {
                oldest = A.last[static_cast<size_t>(l)];
                layer = l;
            }
        if (layer < 0) {
            ++A.full;
            return -1;
        }
        atlas_evict(A, layer);
    }
    if (use_vk) {
        vk::upload_image_2d(g_vk_atlas_img, t.px.data(), static_cast<size_t>(t.w) * t.h * 4, 0, 0, t.w, t.h, 0,
                            static_cast<uint32_t>(layer), 0);
    } else {
        glBindTexture(GL_TEXTURE_2D_ARRAY, A.tex);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glTexSubImage3D(GL_TEXTURE_2D_ARRAY, 0, 0, 0, layer, t.w, t.h, 1, GL_RGBA, GL_UNSIGNED_BYTE, t.px.data());
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    }
    A.by_hash[h] = layer;
    A.hash[static_cast<size_t>(layer)] = h;
    atlas_note_id(A, layer, t.id);
    A.last[static_cast<size_t>(layer)] = now;
    ++g_st.tex_new;
    return layer;
}

// ---- the frame's primitives to vertices / batches --------------------------------------------------

// the GL state of a run of primitives
struct BKey {
    bool lines = false, blend = false, ztest = false, zwrite = false;
    bool dual = false;  // (the blend kinds 0..2 of the fragment shader: one GL state)
    // Z written and the alpha test's fail mode FB_ONLY / RGB_ONLY / ZB_ONLY: the GS writes the colour and the Z of different
    // pixels (every effect seen: TEST 0x5380b, alpha GEQUAL 0x80 failing RGB_ONLY, ZMSK 0: only the opaque texels write Z); drawn in two
    // passes, the colour without Z writes, then the Z alone (uPass)
    bool split = false;
    uint32_t ztst = 0;
    uint64_t alpha = 0;
    bool operator==(const BKey&) const = default;
};
struct Batch {
    BKey key;
    uint32_t first = 0, count = 0;
};
std::vector<FV> g_v;
std::vector<Batch> g_batches;

// orig: (host) the world point a producer's projection `V` puts at the GS's (X, Y, Z) (pixels, Z units)
glm::dvec4 world_of(const View& V, double X, double Y, double Z) {
    const double zs = V.off.z + (Z - V.off.z) / V.zscale;
    const glm::dvec4 h = V.inv * glm::dvec4(X - V.off.x, Y - V.off.y, zs - V.off.z, V.k);
    if (!(std::fabs(h.w) > 1e-30)) return glm::dvec4(V.base, 1.0);
    return glm::dvec4(glm::dvec3(h) / h.w + V.base, 1.0);
}
// orig: (host) a world point on the GS's frame by a producer's projection (its formula, in double)
glm::dvec3 screen_of(const View& V, const glm::dvec3& wp) {
    const glm::dvec4 hp = V.fwd * glm::dvec4(wp - V.base, 1.0);
    glm::dvec3 s = glm::dvec3(hp) * (V.k / hp.w);
    s.z *= V.zscale;
    return s + V.off;
}

// orig: (host) a View from its forward matrix
View make_view(const glm::dmat4& P, const glm::dvec3& off, double k, double zscale, const glm::dvec3& base) {
    View V;
    if (!(std::fabs(glm::determinant(P)) > 1e-30) || k == 0.0) return V;
    V.fwd = P;
    V.inv = glm::inverse(P);
    V.off = off;
    V.k = k;
    V.zscale = zscale;
    V.base = base;
    V.ok = true;
    return V;
}

// orig: (host) the view rows 0x166cc0 as a matrix on the camera-relative point x `s` (rows 0..2 x s, the last as is)
glm::dmat4 rows_matrix(double s) {
    glm::dmat4 P;
    for (int c = 0; c < 4; ++c) {
        const double k = c < 3 ? s : 1.0;
        const uint32_t a = OV(0x00166cc0u) + 0x10 * static_cast<uint32_t>(c);
        P[c] = glm::dvec4(lwc1(a) * k, lwc1(a + 4) * k, lwc1(a + 8) * k, lwc1(a + 12) * k);
    }
    return P;
}

// orig: (host) the producer's input as world points, for the vertex check (RAC1_MRP_FX_VERIFY)
void note_inputs(Notes& N, Note& n, uint32_t a, uint32_t b) {
    n.vin = static_cast<uint32_t>(N.vin.size());
    if (n.kind == kMrpFxQuad) {  // 0x1fd228: the descriptor's four points (through the matrix a1 when given)
        for (uint32_t c = 0; c < 4; ++c) {
            const glm::dvec4 p(lwc1(a + 0x10 * c), lwc1(a + 0x10 * c + 4), lwc1(a + 0x10 * c + 8), lwc1(a + 0x10 * c + 12));
            glm::dvec3 w(p);
            if (b != 0) {
                glm::dvec4 o(0.0);
                for (uint32_t r = 0; r < 4; ++r) o += glm::dvec4(lwc1(b + 0x10 * r), lwc1(b + 0x10 * r + 4), lwc1(b + 0x10 * r + 8), lwc1(b + 0x10 * r + 12)) * p[static_cast<int>(r)];
                w = glm::dvec3(o);
            }
            N.vin.push_back(w);
        }
    } else if (n.kind == kMrpFxPrimList) {  // 0x1fde98: the n positions (12 bytes each)
        for (uint32_t i = 0; i < b && i < 4096; ++i) N.vin.push_back(glm::dvec3(lwc1(a + 12 * i), lwc1(a + 12 * i + 4), lwc1(a + 12 * i + 8)));
    }
    n.vn = static_cast<uint32_t>(N.vin.size()) - n.vin;
}

}  // namespace

// orig: (host) see mrp_fx.h
void mrp_fx_frame_begin() {
    g_real.clear();
    for (Notes& N : g_shadow) N.clear();
    g_window = false;
}

// orig: (host) see mrp_fx.h
void mrp_fx_window(bool on) { g_window = on; }

// orig: (host) see mrp_fx.h
void mrp_fx_note(uint8_t kind, uint32_t first, uint32_t end) {
    if (!g_window || end <= first || kind == 0 || kind >= kMrpFxKinds) return;
    Notes& N = notes_now();
    if (kind == kMrpFxPrimList && N.vu7_cur < 0) return;  // (no 0x1fc740 seen: its projection unknown, the replica draws it)
    g_covered += end - first;
    // (the quads of one effect follow each other in the chain: one range)
    if (!g_env_verify && !N.list.empty() && N.list.back().kind == kind && N.list.back().end == first && (kind != kMrpFxPrimList || N.list.back().vu7 == N.vu7_cur)) {
        N.list.back().end = end;
        return;
    }
    Note n;
    n.kind = kind;
    n.first = first;
    n.end = end;
    n.vu7 = kind == kMrpFxPrimList ? N.vu7_cur : -1;
    N.list.push_back(n);
}

// orig: (host) see mrp_fx.h
void mrp_fx_note_list(uint32_t first, uint32_t end, uint32_t cls) {
    if (!g_window || end <= first || !g_mrp_seas) return;
    Notes& N = notes_now();
    if (!N.lists.empty() && N.lists.back()[1] == first && N.lists.back()[2] == cls) N.lists.back()[1] = end;
    else N.lists.push_back({first, end, cls});
}

// orig: (host) see mrp_fx.h
void mrp_fx_note_vu7(uint32_t qw) {
    if (!g_window) return;
    Notes& N = notes_now();
    Vu7 v;
    for (uint32_t i = 0; i < 16; ++i) v.m[i] = lwc1(qw + 4 * i);
    for (uint32_t i = 0; i < 3; ++i) v.off[i] = lwc1(qw + 0xa0 + 4 * i);  // vf30 (0x16cbe0's copy)
    v.k = lwc1(qw + 0x8c);                                                  // the fans' tag's w (0x16cc50.x's copy)
    N.vu7.push_back(v);
    N.vu7_cur = static_cast<int32_t>(N.vu7.size()) - 1;
}

// orig: (host) see mrp_fx.h
MrpFxScope::MrpFxScope(uint8_t kind, uint32_t a, uint32_t b) : kind_(kind), first_(0), on_(g_window) {
    if (!on_) return;
    first_ = lwu(OV(0x001611c0u));
    if (g_env_verify) {  // (the producer's input, read before it runs)
        Notes& N = notes_now();
        if (kind == kMrpFxSparkles) {
            auto& slots = mrp_lod_in_effects_shadow() ? g_slots_shadow : g_slots_real;
            for (uint32_t i = 0; i < 16; ++i) {
                const uint32_t e = OV(0x0016ea40u) + i * 0x20;
                slots[i] = lh(e + 0x10) > 0 ? glm::dvec4(lwc1(e), lwc1(e + 4), lwc1(e + 8), 1.0) : glm::dvec4(0.0);
            }
        } else {
            Note n;
            n.kind = kind;
            note_inputs(N, n, a, b);
            pending_vin_ = n.vin;
            pending_vn_ = n.vn;
        }
    }
}
// orig: (host) see mrp_fx.h
MrpFxScope::~MrpFxScope() {
    if (!on_) return;
    const uint32_t end = lwu(OV(0x001611c0u));
    Notes& N = notes_now();
    const size_t before = N.list.size();
    mrp_fx_note(kind_, first_, end);
    if (g_env_verify && N.list.size() > before) {
        N.list.back().vin = pending_vin_;
        N.list.back().vn = pending_vn_;
    }
}

namespace {
struct Inventory {
    uint64_t calls = 0, bytes = 0, uncovered = 0;
};
std::map<uint32_t, Inventory> g_inventory;
}  // namespace

// orig: (host) see mrp_fx.h
void mrp_fx_list_call(uint32_t fn, uint32_t first, uint32_t end) {
    static uint64_t covered_at = 0;
    if (!g_env_inventory || mrp_lod_shadow_slot() >= 0) return;
    if (first == 0 && end == 0) {  // (a call's start)
        covered_at = g_covered;
        return;
    }
    Inventory& I = g_inventory[fn];
    ++I.calls;
    I.bytes += end - first;
    const uint64_t cov = g_covered - covered_at;
    I.uncovered += (end - first) > cov ? (end - first) - cov : 0;
}

// orig: (host) see mrp_fx.h. The tests as 0x267c98 makes them (particles_draw.cpp), in double, each with a margin `m` (2 % of the
// compared terms): a test is "sure" when it passes by more than the margin, "possible" when it fails by less. A particle needs the
// widened run when the widened test is possible and the game's own is not sure.
bool mrp_fx_particles_widen_needed(float widen) {
    const uint32_t pool = lwu(OV(0x0016016cu));
    const int32_t last = lw(OV(0x00160174u));
    if (pool == 0 || last < 0) return false;
    const glm::dvec3 cam(lwc1(OV(0x00166dc0u)), lwc1(OV(0x00166dc4u)), lwc1(OV(0x00166dc8u)));
    glm::dvec3 rot[3];
    for (uint32_t r = 0; r < 3; ++r) rot[r] = glm::dvec3(lwc1(OV(0x00166c80u) + 0x10 * r), lwc1(OV(0x00166c84u) + 0x10 * r), lwc1(OV(0x00166c88u) + 0x10 * r));
    glm::dvec4 proj[4];
    for (uint32_t r = 0; r < 4; ++r) {
        const double k = r < 3 ? 1024.0 : 1.0;
        const uint32_t a = OV(0x00166d40u) + 0x10 * r;
        proj[r] = glm::dvec4(lwc1(a) * k, lwc1(a + 4) * k, lwc1(a + 8) * k, lwc1(a + 12) * k);
    }
    const double tx = lwc1(OV(0x0016caf0u)), ty = lwc1(OV(0x0016caf4u)), rx = lwc1(OV(0x0016cc20u)), ry = lwc1(OV(0x0016cc24u));
    const double twx = tx * widen, rwx = std::sqrt(1.0 + twx * twx);  // (patch_cull)
    const double scx = lwc1(OV(0x0016cc00u)) * 0.9375, scy = lwc1(OV(0x0016cc04u)) * 0.9375;
    const double szk = lwc1(OV(0x001607e0u));
    constexpr double m = 0.02;
    for (int32_t i = 0; i <= last && i < 0x4000; ++i) {
        const uint32_t rec = pool + static_cast<uint32_t>(i) * 0x40;
        const int8_t fl = static_cast<int8_t>(lbu(rec + 1));
        if (fl < 0) continue;
        const glm::dvec3 d = glm::dvec3(lwc1(rec + 0x10), lwc1(rec + 0x14), lwc1(rec + 0x18)) - cam;
        if ((fl & 2) == 0) {  // a sprite / line: the centre against the sides, widened by its size
            const glm::dvec3 v = rot[0] * d.x + rot[1] * d.y + rot[2] * d.z;
            const double sx = szk * static_cast<double>(lwc1(rec + 0xc));
            const double ey = ty * v.z - (std::fabs(v.y) - ry * sx);
            if (ey < -m * (std::fabs(ty * v.z) + std::fabs(v.y) + 1e-3)) continue;  // (the y test fails either way)
            const double en = tx * v.z - (std::fabs(v.x) - rx * sx), ew = twx * v.z - (std::fabs(v.x) - rwx * sx);
            const double mx = m * (std::fabs(twx * v.z) + std::fabs(v.x) + 1e-3);
            if (ew >= -mx && !(en > mx)) return true;
        } else {  // a streak: both ends through the clip rows, inside |x|, |y|, |z| <= w; then not both past one side of the screen band
            const glm::dvec3 e = glm::dvec3(lwc1(rec + 0x20), lwc1(rec + 0x24), lwc1(rec + 0x28)) - cam;
            const glm::dvec4 c1 = proj[0] * d.x + proj[1] * d.y + proj[2] * d.z + proj[3], c2 = proj[0] * e.x + proj[1] * e.y + proj[2] * e.z + proj[3];
            bool possible = true, sure = true;
            for (const glm::dvec4& c : {c1, c2}) {
                const double w = c.w, mw = m * (std::fabs(w) + 1e-3);
                if (!(w > -mw)) {  // (behind the eye: out in both)
                    possible = sure = false;
                    break;
                }
                if (std::fabs(c.y) > w + mw || std::fabs(c.z) > w + mw || std::fabs(c.x) > w * widen + mw * widen) possible = false;
                if (std::fabs(c.y) > w - mw || std::fabs(c.z) > w - mw || std::fabs(c.x) > w - mw) sure = false;
                if (std::fabs(c.x * scx) > w - mw || std::fabs(c.y * scy) > w - mw) sure = false;  // (the screen band: kept sure only well inside)
            }
            if (possible && !sure) return true;
        }
    }
    return false;
}

// orig: (host) see mrp_fx.h. The streak test of 0x267c98: both ends through the clip rows 0x166d40 (x 1024) inside |x|, |y| <= w, then
// both ends past one side of the screen (x, y x 0x16cc00 x 0.9375) culls it. In the effects shadow (the sphere test widened by w,
// patch_cull) the clip rows' and the screen scale's x shrunk by w: the same tests over the widened frustum.
void mrp_fx_particles_patch(bool begin) {
    static const bool off = std::getenv("RAC1_MRP_FX_STREAKS") != nullptr && std::atoi(std::getenv("RAC1_MRP_FX_STREAKS")) == 0;  // (testing: the earlier streak-free output)
    if (off || !mrp_lod_in_effects_shadow()) return;
    const float w = mrp_lod_effects_widen();
    if (begin) {
        if (!(w > 1.0f)) return;
        for (uint32_t r = 0; r < 4; ++r) g_saved[r] = lwc1(OV(0x00166d40u) + 0x10 * r);
        g_saved[4] = lwc1(OV(0x0016cc00u));
        for (uint32_t r = 0; r < 4; ++r) swc1(OV(0x00166d40u) + 0x10 * r, g_saved[r] / w);
        swc1(OV(0x0016cc00u), g_saved[4] / w);
        g_patched = true;
    } else if (g_patched) {  // (the shadow's journal puts them back too; the rest of the block sees them as before)
        for (uint32_t r = 0; r < 4; ++r) swc1(OV(0x00166d40u) + 0x10 * r, g_saved[r]);
        swc1(OV(0x0016cc00u), g_saved[4]);
        g_patched = false;
    }
}

// orig: (host) see mrp_fx.h
bool mrp_fx_chain_ranges(bool active, const std::vector<int>& subs, std::vector<gs::Gs::FxRange>& ranges, std::vector<gs::Gs::Vu7Range>& vu7) {
    mrp_particles_fx_capture();  // (the game's rain / snow textures this frame, for the drawing: nothing unless the ambient particles are on)
    ranges.clear();
    vu7.clear();
    Frame& F = g_frame;
    F.notes.clear();
    F.vin.clear();
    F.vu7 = false;
    bool any = !g_real.list.empty() || !g_real.lists.empty();
    for (int k : subs) any = any || (k >= 0 && k < kMrpLodSlots && (!g_shadow[k].list.empty() || !g_shadow[k].lists.empty()));
    if (!g_mrp_fx || !g_world_mrp || !active || !any) return false;
    if (g_mrp_seas && (g_kinds_mask >> kMrpFxVu7 & 1u) != 0) {  // (VU1 program 7's batches in the draw lists' callbacks)
        auto add_lists = [&](const Notes& N, uint8_t sub) {
            for (const auto& l : N.lists) vu7.push_back(gs::Gs::Vu7Range{l[0], l[1] - 1, sub, l[2]});
        };
        add_lists(g_real, 0);
        for (size_t i = 0; i < subs.size(); ++i)
            if (subs[i] >= 0 && subs[i] < kMrpLodSlots) add_lists(g_shadow[subs[i]], static_cast<uint8_t>(i + 1));
        // (the ranges must not overlap: a substitute's are its own, sub k)
        std::sort(vu7.begin(), vu7.end(), [](const gs::Gs::Vu7Range& a, const gs::Gs::Vu7Range& b) { return a.sub != b.sub ? a.sub < b.sub : a.first < b.first; });
        std::vector<gs::Gs::Vu7Range> merged;
        for (const auto& r : vu7) {
            if (!merged.empty() && merged.back().sub == r.sub && r.first <= merged.back().last + 1 && r.tag == merged.back().tag)
                merged.back().last = std::max(merged.back().last, r.last);
            else if (!merged.empty() && merged.back().sub == r.sub && r.first <= merged.back().last)  // (overlapping: never seen; the first kept)
                continue;
            else merged.push_back(r);
        }
        vu7.swap(merged);
        const glm::dvec3 cam(lwc1(OV(0x00166dc0u)), lwc1(OV(0x00166dc4u)), lwc1(OV(0x00166dc8u)));
        F.vu7_g = rows_matrix(1024.0) * glm::translate(glm::dmat4(1.0), -cam);
        F.vu7_ginv = glm::inverse(F.vu7_g);
        F.vu7_ge = F.vu7_g;
        const double kf = lwc1(OV(0x0016cc50u));
        for (int c = 0; c < 4; ++c)
            for (int r = 0; r < 3; ++r) F.vu7_ge[c][r] *= kf;
        F.vu7_geinv = glm::inverse(F.vu7_ge);
        F.vu7 = !vu7.empty();
    }
    // the frame's projections
    F.views.assign(kMrpFxKinds, View{});
    const glm::dvec3 cam(lwc1(OV(0x00166dc0u)), lwc1(OV(0x00166dc4u)), lwc1(OV(0x00166dc8u)));
    const glm::dvec3 off(lwc1(OV(0x0016cbe0u)), lwc1(OV(0x0016cbe4u)), lwc1(OV(0x0016cbe8u)));
    F.views[kMrpFxParticles] = make_view(rows_matrix(1024.0), off, lwc1(OV(0x0016cc50u)), 1.0, cam);
    F.views[kMrpFxQuad] = make_view(rows_matrix(lwc1(OV(0x00160be8u))), off, lwc1(OV(0x00160bf0u)), 1.0, cam);  // (vf30.z, vf31.x)
    // (0x1fead8: X / Y + 0x8000 / 16, Z x 0x3f7fec57 + 0x16cbe8)
    F.views[kMrpFxSparkles] = make_view(rows_matrix(1024.0), glm::dvec3(2048.0, 2048.0, off.z), lwc1(OV(0x0016cc50u)), static_cast<double>(u2f(0x3f7fec57u)), cam);
    auto add = [&](const Notes& N, uint8_t sub) {
        const uint32_t vu7_base = static_cast<uint32_t>(F.views.size());
        for (const Vu7& v : N.vu7) {
            glm::dmat4 P;
            for (int c = 0; c < 4; ++c) P[c] = glm::dvec4(v.m[4 * c], v.m[4 * c + 1], v.m[4 * c + 2], v.m[4 * c + 3]);
            F.views.push_back(make_view(P, glm::dvec3(v.off[0], v.off[1], v.off[2]), v.k, 1.0, glm::dvec3(0.0)));
        }
        const uint32_t vin_base = static_cast<uint32_t>(F.vin.size());
        F.vin.insert(F.vin.end(), N.vin.begin(), N.vin.end());
        for (const Note& n : N.list) {
            if ((g_kinds_mask >> n.kind & 1u) == 0) continue;
            const uint32_t view = n.kind == kMrpFxPrimList ? vu7_base + static_cast<uint32_t>(n.vu7) : n.kind;
            if (!F.views[view].ok) {
                ++g_st.no_view;
                continue;
            }
            gs::Gs::FxRange r;
            r.first = n.first;
            r.last = n.end - 1;
            r.sub = sub;
            r.kind = n.kind;
            r.id = static_cast<uint32_t>(F.notes.size());
            ranges.push_back(r);
            F.notes.push_back(FrameNote{n.kind, view, vin_base + n.vin, n.vn});
            ++g_st.notes[n.kind];
        }
    };
    add(g_real, 0);
    bool fx_sub = false;
    for (size_t i = 0; i < subs.size(); ++i) {
        if (subs[i] < 0 || subs[i] >= kMrpLodSlots) continue;
        add(g_shadow[subs[i]], static_cast<uint8_t>(i + 1));
        fx_sub = fx_sub || subs[i] == kMrpLodSlotEffects;
    }
    F.slots = fx_sub ? g_slots_shadow : g_slots_real;
    F.cam = cam;
    return !ranges.empty() || !vu7.empty();
}

namespace {

// The producers' arithmetic is single precision: their GS Z is a sum at 2^23 (0x16cbe8 = 8388112 on Veldin) where a float's ulp is 1, of
// madd / FTOI4 steps each truncating (ADR 0011), so a kicked Z may stand a few units from the exact value; X / Y within a 1/16 px step.
// VU1 program 7 takes world points (~1e2) through a matrix holding -1024 x the camera (~1e5): its clip coordinates carry a relative error
// of a few 1e-6, i.e. Z up to ~5e-6 of Z - 0x16cbe8 (tens of units for far meshes), X / Y still within the 1/16 px step.
constexpr double kVerifyXy = 0.1;
double verify_ztol(const View& V, double Z) { return 4.0 + 5e-6 * std::fabs(Z - V.off.z); }

// orig: (host) testing (RAC1_MRP_FX_VERIFY): a drawn vertex against its producer - at one of its input points projected by the
// producer's formula (kVerifyXy px, verify_ztol), else on its input's surface (the clippers' new vertices: the screen ray through (X, Y) meets
// a triangle of consecutive inputs at the kicked Z), else counted bad
void verify_vertex(const FrameNote& fn, const View& V, double X, double Y, double Z) {
    const int k = fn.kind;
    ++g_st.pv_verts[k];
    const Frame& F = g_frame;
    if (k == kMrpFxSparkles) {  // a corner at its live slot's Z, within 64 px of its centre
        for (const glm::dvec4& s : F.slots) {
            if (s.w == 0.0) continue;
            const glm::dvec3 sc = screen_of(V, glm::dvec3(s));
            if (std::fabs(sc.z - Z) <= verify_ztol(V, Z) && std::fabs(sc.x - X) <= 64.0 && std::fabs(sc.y - Y) <= 64.0) {
                ++g_st.pv_at[k];
                g_st.pv_max_plane[k] = std::max(g_st.pv_max_plane[k], std::fabs(sc.z - Z));
                return;
            }
        }
        ++g_st.pv_bad[k];
        static int shown = 0;
        if (g_env_verify_log && shown++ < 10) {
            std::printf("[mrp-fx] verify sparkle bad: X %.2f Y %.2f Z %.0f;", X, Y, Z);
            for (const glm::dvec4& s : F.slots)
                if (s.w != 0.0) {
                    const glm::dvec3 sc = screen_of(V, glm::dvec3(s));
                    std::printf(" slot (%.2f %.2f %.0f)", sc.x, sc.y, sc.z);
                }
            std::printf("\n");
        }
        return;
    }
    if (fn.vn == 0) {
        ++g_st.pv_bad[k];
        return;
    }
    for (uint32_t i = 0; i < fn.vn; ++i) {
        const glm::dvec3 sc = screen_of(V, F.vin[fn.vin + i]);
        if (std::fabs(sc.x - X) <= kVerifyXy && std::fabs(sc.y - Y) <= kVerifyXy && std::fabs(sc.z - Z) <= verify_ztol(V, Z)) {
            ++g_st.pv_at[k];
            g_st.pv_max_plane[k] = std::max(g_st.pv_max_plane[k], std::fabs(sc.z - Z));
            return;
        }
    }
    // on the surface: the screen ray through (X, Y) against each triangle of consecutive inputs (a quad's two, a strip's)
    const glm::dvec3 p1(world_of(V, X, Y, Z)), p2(world_of(V, X, Y, Z + 4096.0));
    double best = 1e30;
    for (uint32_t i = 0; i + 2 < fn.vn; ++i) {
        const glm::dvec3 &a = F.vin[fn.vin + i], &b = F.vin[fn.vin + i + 1], &c = F.vin[fn.vin + i + 2];
        const glm::dvec3 n = glm::cross(b - a, c - a);
        const double den = glm::dot(p2 - p1, n);
        if (!(std::fabs(den) > 1e-30)) continue;
        const glm::dvec3 h = p1 + (p2 - p1) * (glm::dot(a - p1, n) / den);
        // inside the triangle (barycentric, a little slack for the clipper's rounding)
        const double nn = glm::dot(n, n);
        const double u = glm::dot(glm::cross(c - b, h - b), n) / nn, v = glm::dot(glm::cross(a - c, h - c), n) / nn, w = 1.0 - u - v;
        if (u < -1e-3 || v < -1e-3 || w < -1e-3) continue;
        best = std::min(best, std::fabs(screen_of(V, h).z - Z));
    }
    // (the clipper's vertices: its cut point t = (a - aw) / den and the lerps in float on clip coordinates of the size of Z - 0x16cbe8,
    // ~8M on Veldin: up to ~2e-6 of that more)
    if (best <= verify_ztol(V, Z)) {
        ++g_st.pv_on[k];
        g_st.pv_max_plane[k] = std::max(g_st.pv_max_plane[k], best);
        return;
    }
    // in world space, relative to the distance from the camera: VU1 program 7 multiplies world points (~1e2) by a matrix holding
    // -1024 x the camera (~1e5) in float, so its kicked vertices carry an error of ~1e-5..1e-4 of the distance, more than the screen
    // tolerances above for far, small meshes (the orbs at a distance); the kicked vertex within 2e-4 of the distance of the input's
    // surface (an input point or a triangle of consecutive inputs) passes, counted apart
    {
        const glm::dvec3 w(world_of(V, X, Y, Z));
        double rel = 1e30;
        auto seg = [](const glm::dvec3& p, const glm::dvec3& a, const glm::dvec3& b) {
            const glm::dvec3 d = b - a;
            const double l = glm::dot(d, d);
            const double t = l > 0.0 ? std::clamp(glm::dot(p - a, d) / l, 0.0, 1.0) : 0.0;
            return glm::length(p - (a + d * t));
        };
        for (uint32_t i = 0; i < fn.vn; ++i) {
            const glm::dvec3& a = F.vin[fn.vin + i];
            const double dist = std::max(1e-6, glm::length(a - F.cam));
            double e = glm::length(w - a);
            if (i + 1 < fn.vn) e = std::min(e, seg(w, a, F.vin[fn.vin + i + 1]));
            if (i + 2 < fn.vn) {  // the triangle's interior
                const glm::dvec3 &b = F.vin[fn.vin + i + 1], &c = F.vin[fn.vin + i + 2];
                const glm::dvec3 n = glm::cross(b - a, c - a);
                const double nn = glm::dot(n, n);
                if (nn > 0.0) {
                    const glm::dvec3 h = w - n * (glm::dot(w - a, n) / nn);
                    const double u = glm::dot(glm::cross(c - b, h - b), n) / nn, v = glm::dot(glm::cross(a - c, h - c), n) / nn;
                    if (u >= 0.0 && v >= 0.0 && u + v <= 1.0) e = std::min(e, glm::length(w - h));
                    e = std::min(e, seg(w, b, c));
                }
            }
            rel = std::min(rel, e / dist);
        }
        if (rel <= 2e-4) {
            ++g_st.pv_world[k];
            g_st.pv_max_rel[k] = std::max(g_st.pv_max_rel[k], rel);
            return;
        }
    }
    ++g_st.pv_bad[k];
    static int shown = 0;
    if (g_env_verify_log && shown++ < 20) {
        std::printf("[mrp-fx] verify kind %d bad: X %.3f Y %.3f Z %.0f (nearest surface Z off by %.1f); inputs:", k, X, Y, Z, best);
        for (uint32_t i = 0; i < fn.vn && i < 6; ++i) {
            const glm::dvec3 sc = screen_of(V, F.vin[fn.vin + i]);
            const glm::dvec3 sp = screen_of(F.views[kMrpFxParticles], F.vin[fn.vin + i]);
            std::printf(" (%.4f %.4f %.4f -> %.3f %.3f %.0f | %.3f %.3f %.0f)", F.vin[fn.vin + i].x, F.vin[fn.vin + i].y, F.vin[fn.vin + i].z, sc.x, sc.y, sc.z, sp.x, sp.y, sp.z);
        }
        // the nearest input on the screen
        double bd = 1e30;
        uint32_t bi = 0;
        for (uint32_t i = 0; i < fn.vn; ++i) {
            const glm::dvec3 sc = screen_of(V, F.vin[fn.vin + i]);
            const double d = std::hypot(sc.x - X, sc.y - Y);
            if (d < bd) {
                bd = d;
                bi = i;
            }
        }
        std::printf(" | nearest input %u of %u at %.3f px", bi, fn.vn, bd);
        std::printf("\n");
    }
}

// the frame's work in the primitives' order: a run of the GS replica's (gs_draw from..to), of the MRP's effects' batches, or of
// VU1 program 7's surfaces' batches (kMrpFxVu7)
enum : int { kStepGs = 0, kStepFx = 1, kStepVu7 = 2 };
struct Step {
    int kind = kStepGs;
    size_t from = 0, to = 0;  // primitives (gs) or batches (MRP)
};
std::vector<Step> g_steps;

// orig: (host) the GL state of a primitive's GS state (BKey) and its fragment shader's flags word
void key_and_flags(const gs::DrawState& st, bool lines, BKey& k, uint32_t& flags) {
    const bool tme = fld(st.prim, 4, 1) != 0, fge = fld(st.prim, 5, 1) != 0;
    const uint32_t ztst = fld(st.test, 17, 2);
    k = BKey{};
    k.lines = lines;
    k.blend = fld(st.prim, 6, 1) != 0;
    k.alpha = k.blend ? st.alpha : 0;
    // the GS's Cv = (A - B) C + D as dual-source factors: none (0), additive 0x48 (1), alpha 0x44 (2); else the batch's own state (3)
    uint32_t bkind = 3;
    if (!k.blend) bkind = 0;
    else if ((st.alpha & 0xff) == 0x48) bkind = 1;
    else if ((st.alpha & 0xff) == 0x44) bkind = 2;
    if (bkind != 3) {
        k.dual = true;
        k.blend = false;
        k.alpha = 0;
    }
    k.ztest = fld(st.test, 16, 1) != 0 && ztst != 1;
    k.ztst = ztst;
    k.zwrite = k.ztest && fld(st.zbuf, 32, 1) == 0;
    k.split = k.zwrite && fld(st.test, 0, 1) != 0 && fld(st.test, 1, 3) != 1 && fld(st.test, 12, 2) != 0;
    flags = (tme ? 1u : 0u) | (fld(st.tex0, 35, 2) << 1) | (fld(st.tex0, 34, 1) << 3) | (fld(st.prim, 3, 1) << 4) | ((fge ? 1u : 0u) << 5) |
            (fld(st.test, 0, 1) << 6) | (fld(st.test, 1, 3) << 7) | (fld(st.test, 4, 8) << 10) | (fld(st.tex1, 5, 1) << 18) |
            ((fld(st.clamp, 0, 2) == 0 ? 1u : 0u) << 19) | ((fld(st.clamp, 2, 2) == 0 ? 1u : 0u) << 20) | (fld(st.test, 12, 2) << 22) | (bkind << 24);
}

// ---- VU1 program 7's surfaces (kMrpFxVu7) ----------------------------------------------------------------------------

// One vertex of a batch: its world point, S T Q as the batch holds them, the fog's term before its clamp (the screen matrix's w plus
// vf30.w), RGBA; flat: the atlas layer, the texture's size, the flags word, FOGCOL; the fog's clamp
struct V7 {
    float wp[3];
    float stq[3];
    float fogw;
    uint8_t rgba[4];
    uint32_t tex[4];
    float fogc[2];
    uint32_t emis;
    float depth;  // the GS's Z as the replica maps it to the depth buffer, in NDC (x the clip w in the vertex shader)
};
static_assert(sizeof(V7) == 64);
std::vector<V7> g_v7;
std::vector<Batch> g_b7;

struct Vu7Stats {
    uint64_t batches = 0, tris = 0, world = 0, general = 0, no_tex = 0, mip = 0, bad_type = 0, clip_path = 0;
} g_s7;

// the emissive surfaces of kind 4 ("surface", mrp_lighting.h): (the callback's moby class, TEX0's TBP) -> the shader's slot
struct Vu7Emissive {
    uint32_t gen = 0;
    std::unordered_map<uint64_t, uint32_t> slot;
    std::set<uint64_t> seen;  // (RAC1_LIGHTS_TRACE: the surfaces drawn, listed once)
    bool on = false;
} g_e7;
uint64_t e7_key(uint32_t cls, uint32_t tbp) { return static_cast<uint64_t>(cls) << 32 | tbp; }
// orig: (host) the table brought up to date (once a frame)
void vu7_emissive_refresh() {
    std::vector<EmissiveSurface> list;
    EmissiveSurface pv;
    bool has_pv = false;
    const uint32_t gen = mrp_emissive_snapshot(list, &pv, &has_pv);
    g_e7.on = mrp_lighting_active() && g_emissive != 0;
    if (gen == g_e7.gen) return;
    g_e7.gen = gen;
    g_e7.slot.clear();
    for (size_t i = 0; i < list.size() && i < static_cast<size_t>(kEmissiveSlots); ++i)
        if (list[i].kind == 4) g_e7.slot[e7_key(static_cast<uint32_t>(list[i].o_class), static_cast<uint32_t>(list[i].tex))] = static_cast<uint32_t>(i + 1);
    if (has_pv && pv.kind == 4) g_e7.slot[e7_key(static_cast<uint32_t>(pv.o_class), static_cast<uint32_t>(pv.tex))] = static_cast<uint32_t>(kEmissivePreviewSlot);
}

// The program's own vertex (shadows_vu1.cpp, 0x010..): X Y Z = (M p).xyz x Q + vf30.xyz, Q = vf31.w / w, F = clamp(w + vf30.w,
// vf7.x, vf7.y) (FTOI4: its integer part), STQ = (S, T, Q') x Q, the colour as it is. The MRP draws the same strip from the points p
// themselves - in the world when M is the frame's 0x1fc740 matrix (less its Z bias), else taken there through it - with the world's
// projection: the GL clips what the program clipped at the guard band (and the triangles its side test dropped).
// orig: (host) the batch of type-9 primitive `p` into g_v7 / g_b7 (false: not drawn)
// the replica's depth map of the frame (render_gs_depth: window depth = a + b Z), for the surfaces' depth
bool g_v7_depth = false;
double g_v7_da = 0.0, g_v7_db = 0.0;
bool build_vu7(const gs::Gs& gs, const gs::Primitive& p) {
    const auto& bs = gs.vu7_batches();
    if (p.fx_id >= bs.size()) return false;
    const gs::Vu7Batch& b = bs[p.fx_id];
    const int type = static_cast<int>(fld(b.st.prim, 0, 3));
    if (type != 3 && type != 4 && type != 5) {
        ++g_s7.bad_type;
        return false;
    }
    // the two GS states a batch draws with: its own (the strip), and on the clipping path the fans' (the triangles it clips)
    struct State {
        const gs::DrawState* st = nullptr;
        bool ok = false, fst = false;
        int layer = 0, tw = 1, th = 1;
        BKey k;
        uint32_t flags = 0, emis = 0;
    };
    auto state = [&](const gs::DrawState& st, const std::shared_ptr<const gs::TexImage>& tex) {
        State S;
        S.st = &st;
        const bool tme = fld(st.prim, 4, 1) != 0;
        S.fst = fld(st.prim, 8, 1) != 0;
        if (tme) {
            if (tex == nullptr) {
                ++g_s7.no_tex;
                return S;
            }
            S.layer = atlas_layer(*tex);
            if (S.layer < 0) return S;
            S.tw = tex->w;
            S.th = tex->h;
            // (an emissive surface or a material: the callback's class, the texture's block)
            const uint64_t key = e7_key(b.tag, fld(st.tex0, 0, 14));
            if (g_e7.on)
                if (auto it = g_e7.slot.find(key); it != g_e7.slot.end()) S.emis = it->second;
            static const bool trace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
            if (trace && g_e7.seen.insert(key).second)
                std::printf("[lights] program 7 surface: class %u tex %u (TEX0 %016llx, %d x %d): \"emissive surface %u tex %u\"\n", b.tag, fld(st.tex0, 0, 14),
                            static_cast<unsigned long long>(st.tex0), S.tw, S.th, b.tag, fld(st.tex0, 0, 14));
        }
        key_and_flags(st, false, S.k, S.flags);
        S.ok = true;
        return S;
    };
    const State strip = state(b.st, b.tex);
    const bool clip_path = b.start == 0xe;
    const State fan = clip_path ? state(b.st_fan, b.tex_fan) : State{};
    if (b.mip) ++g_s7.mip;
    glm::dmat4 M, C;
    for (int c = 0; c < 4; ++c) {
        M[c] = glm::dvec4(b.m[4 * c], b.m[4 * c + 1], b.m[4 * c + 2], b.m[4 * c + 3]);
        C[c] = glm::dvec4(b.clip[4 * c], b.clip[4 * c + 1], b.clip[4 * c + 2], b.clip[4 * c + 3]);
    }
    // The point's place on the screen is the matrix's x, y, z times Q = k / w: a matrix and its k scaled together (the w row and k: a
    // callback's own fog density, Kerwan's class 994) draw the same. So the matrices are compared with their x, y, z rows times their k,
    // each scaled by its w row's size: the frame's 0x1fc740 matrix (element by element to the float's precision of its row: its
    // translation's z is ~1e11 on Gaspar, where the Z bias the level's shadow meshes add, 0x15f308 = -0.04, is below a float's step: no
    // bias reaches the GS there either) takes world points; any other is taken back to the world through the frame's.
    glm::dmat4 Me = M;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 3; ++r) Me[c][r] *= static_cast<double>(b.k);
    const glm::dmat4& Ge = g_frame.vu7_ge;
    auto wsize = [](const glm::dmat4& A) { return std::max({std::fabs(A[0][3]), std::fabs(A[1][3]), std::fabs(A[2][3]), std::fabs(A[3][3]), 1e-30}); };
    const double sm = wsize(Me), sg = wsize(Ge);
    bool same = std::isfinite(sm) && b.k != 0.0f;
    for (int r = 0; r < 4 && same; ++r) {
        const double row = std::max({std::fabs(Ge[0][r]), std::fabs(Ge[1][r]), std::fabs(Ge[2][r]), std::fabs(Ge[3][r])}) / sg;
        for (int c = 0; c < 4 && same; ++c)
            if (std::fabs(Me[c][r] / sm - Ge[c][r] / sg) > 1e-5 * std::max(row, 1e-12)) same = false;
    }
    ++(same ? g_s7.world : g_s7.general);
    ++g_s7.batches;
    ++g_st.by_kind[kMrpFxVu7];  // (the report's "by producer": batches)
    if (std::getenv("RAC1_VU7_WHERE") != nullptr && g_st.frames % 120 == 1 && b.n > 0) {  // (temporary)
        const auto& v0 = gs.vu7_vertices()[b.first];
        std::printf("[vu7where] class %u at %.1f %.1f %.1f cam %.1f %.1f %.1f\n", b.tag, v0.xyz[0], v0.xyz[1], v0.xyz[2], g_frame.cam.x, g_frame.cam.y, g_frame.cam.z);
    }
    if (clip_path) ++g_s7.clip_path;
    const auto& vin = gs.vu7_vertices();
    auto vert = [&](uint32_t i, const State& S) {
        const gs::Vu7Vertex& s = vin[b.first + i];
        const glm::dvec4 pt(s.xyz[0], s.xyz[1], s.xyz[2], 1.0);
        const glm::dvec4 clip = M * pt;
        glm::dvec3 w(pt);
        if (!same) {
            const glm::dvec4 h = g_frame.vu7_geinv * (Me * pt);
            w = std::fabs(h.w) > 1e-30 ? glm::dvec3(h) / h.w : glm::dvec3(pt);
        }
        V7 v{};
        v.wp[0] = static_cast<float>(w.x);
        v.wp[1] = static_cast<float>(w.y);
        v.wp[2] = static_cast<float>(w.z);
        if (S.fst) {  // (UV in texels: with FST the GS reads the UV register, not kicked here)
            v.stq[0] = s.st[0] / static_cast<float>(S.tw);
            v.stq[1] = s.st[1] / static_cast<float>(S.th);
            v.stq[2] = 1.0f;
        } else {
            v.stq[0] = s.st[0];
            v.stq[1] = s.st[1];
            v.stq[2] = s.st[2] != 0.0f ? s.st[2] : 1.0f;
        }
        // (the fog per vertex as the program makes it - clamped, its integer part - and interpolated over the screen as the GS does)
        const float fw = std::min(std::max(static_cast<float>(clip.w) + b.off[3], b.fog[0]), b.fog[1]);
        v.fogw = std::isfinite(fw) ? std::floor(fw) : 0.0f;
        for (int l = 0; l < 4; ++l) v.rgba[l] = s.rgba[l];
        v.tex[0] = static_cast<uint32_t>(S.layer);
        v.tex[1] = static_cast<uint32_t>(S.tw) | static_cast<uint32_t>(S.th) << 16;
        v.tex[2] = S.flags;
        v.tex[3] = static_cast<uint32_t>(S.st->fogcol & 0xffffff);
        v.fogc[0] = b.fog[0];
        v.fogc[1] = b.fog[1];
        v.emis = S.emis;
        {  // the depth: the program's Z (its integer part, FTOI4) through the replica's map (render_gs_depth)
            const double q = static_cast<double>(b.k) / clip.w;
            const double Z = std::floor(clip.z * q + static_cast<double>(b.off[2]));
            v.depth = g_v7_depth ? static_cast<float>(2.0 * (g_v7_da + g_v7_db * Z) - 1.0) : 2.0f;
        }
        g_v7.push_back(v);
    };
    std::vector<std::array<uint32_t, 3>> tris;
    if (type == 3) {
        for (uint32_t i = 0; i + 2 < b.n; i += 3) tris.push_back({i, i + 1, i + 2});
    } else {
        for (uint32_t i = 0; i + 2 < b.n; ++i) tris.push_back(type == 4 ? std::array<uint32_t, 3>{i, i + 1, i + 2} : std::array<uint32_t, 3>{0, i + 1, i + 2});
    }
    // Which triangles the GS drew, and how. The program's own decision is taken from its kick (the strip's ADC flags): a triangle it
    // kicked whole is the strip's - unless a point of it lies behind the camera (Q = k / w not of k's sign): the program's near test
    // (z < -w of the clip matrix, whose z row is its w row but for a hair) lets such points through by its rounding, and the GS
    // draws the triangle mirrored through the eye, off the screen - so the MRP leaves it out too (drawn rightly, Gaspar's lava glow
    // covered the lava far away where the PS2 shows none). A triangle it did not kick whole was clipped - drawn as a fan of the fans'
    // state - unless all three points lie past one plane of the guard band (not drawn).
    enum : uint8_t { kNone = 0, kStrip = 1, kFan = 2 };
    std::vector<uint8_t> how(tris.size(), kStrip);
    {
        std::vector<uint8_t> flags(b.n), behind(b.n);
        for (uint32_t i = 0; i < b.n; ++i) {
            const gs::Vu7Vertex& s = vin[b.first + i];
            const glm::dvec4 pt(s.xyz[0], s.xyz[1], s.xyz[2], 1.0);
            const glm::dvec4 c = C * pt;
            const double aw = std::fabs(c.w);
            flags[i] = static_cast<uint8_t>((c.x > aw ? 1 : 0) | (c.x < -aw ? 2 : 0) | (c.y > aw ? 4 : 0) | (c.y < -aw ? 8 : 0) | (c.z < -aw ? 16 : 0));
            // (behind the camera, or past the GS's coordinate range: X, Y are 12.4 fixed point in 16 bits, the program's FTOI4 keeps
            // their low bits, so a point beyond 0..4096 pixels lands elsewhere - a whole triangle of the unclipped path reaching that far,
            // a big one near the camera, is drawn torn across the screen or off it: Gaspar's lava glow near the camera)
            const glm::dvec4 h = M * pt;
            const double q = static_cast<double>(b.k) / h.w;
            const double X = h.x * q + b.off[0], Y = h.y * q + b.off[1], Z = h.z * q + b.off[2];
            behind[i] = !(q > 0.0) || !(X >= 0.0 && X < 4096.0 && Y >= 0.0 && Y < 4096.0) || !(Z >= 0.0 && Z < 16777216.0) ? 1 : 0;
        }
        const bool have = b.kicked.size() == b.n && type == 4;
        for (size_t t = 0; t < tris.size(); ++t) {
            const auto& T = tris[t];
            const bool any_out = (flags[T[0]] | flags[T[1]] | flags[T[2]]) != 0;
            const bool all_one = (flags[T[0]] & flags[T[1]] & flags[T[2]]) != 0;
            const bool whole = have ? b.kicked[T[2]] != 0 : !(clip_path && any_out);
            if (whole) how[t] = (behind[T[0]] | behind[T[1]] | behind[T[2]]) != 0 ? kNone : kStrip;
            else how[t] = all_one || !clip_path ? kNone : kFan;
        }
    }
    auto emit = [&](const State& S, uint8_t want) {  // (the program kicks the clipped fans as it goes, the strip at its end)
        if (!S.ok) return;
        const size_t at = g_v7.size();
        for (size_t t = 0; t < tris.size(); ++t)
            if (how[t] == want)
                for (uint32_t i : tris[t]) vert(i, S);
        const uint32_t n = static_cast<uint32_t>(g_v7.size() - at);
        g_s7.tris += n / 3;
        if (n == 0) return;
        if (g_b7.empty() || g_b7.back().count == 0) {
            if (g_b7.empty()) g_b7.push_back(Batch{});
            g_b7.back().key = S.k;
            g_b7.back().first = static_cast<uint32_t>(at);
        } else if (!(g_b7.back().key == S.k)) {
            g_b7.push_back(Batch{S.k, static_cast<uint32_t>(at), 0});
        }
        g_b7.back().count += n;
    };
    if (clip_path) emit(fan, kFan);
    emit(strip, kStrip);
    return true;
}

const char* kV7Vs = R"(#version 460 core
layout(location = 0) in vec3 aWorld;
layout(location = 1) in vec3 aStq;
layout(location = 2) in float aFogW;
layout(location = 3) in uvec4 aRgba;
layout(location = 4) in uvec4 aTex;
layout(location = 5) in vec2 aFogC;
layout(location = 6) in uint aEmis;
layout(location = 7) in float aDepth;
uniform mat4 uVP;
)" RAC1_SHADOW_VS R"(
smooth out vec3 vStq;
noperspective out vec4 vColor;
flat out vec4 vFlat;
noperspective out float vFogW;
flat out vec2 vFogC;
flat out uvec4 vTex;
flat out uint vEmis;
void main() {
    gl_Position = sh_clip(uVP * vec4(aWorld, 1.0));
    // the depth the GS gave it (its Z through the replica's map, linear on the screen as the GS's): the surfaces meet the world as
    // in the original (a lava glow lying on the ground hidden by it far away, as the GS's Z steps decided)
    if (aDepth < 1.5) gl_Position.z = aDepth * gl_Position.w;
    vStq = aStq;
    vColor = vec4(aRgba) / 255.0;
    vFlat = vColor;
    vFogW = aFogW;
    vFogC = aFogC;
    vTex = aTex;
    vEmis = aEmis;
}
)";

// the effects' fragment shader (the GS's texture lookup, texture function, fog, alpha test, blending) with the program's fog - its
// clamp per pixel - and, with the modern lighting, the sun's shadows, the point lights and the emissive surfaces
const char* kV7Fs = R"(#version 460 core
#define SH_SPLIT 1
smooth in vec3 vStq;
noperspective in vec4 vColor;
flat in vec4 vFlat;
noperspective in float vFogW;
flat in vec2 vFogC;
flat in uvec4 vTex;
flat in uint vEmis;
uniform sampler2DArray uAtlas;
uniform int uShade;
uniform int uPass;
)" RAC1_SHADOW_FS R"(
layout(location = 0, index = 0) out vec4 FragColor;
layout(location = 0, index = 1) out vec4 FragFactor;
ivec2 wrapc(ivec2 c, ivec2 sz, bool rs, bool rt) {
    c.x = rs ? ((c.x % sz.x) + sz.x) % sz.x : clamp(c.x, 0, sz.x - 1);
    c.y = rt ? ((c.y % sz.y) + sz.y) % sz.y : clamp(c.y, 0, sz.y - 1);
    return c;
}
void main() {
    sh_prepare();
    uint fl = vTex.z;
    bool tme = (fl & 1u) != 0u, iip = ((fl >> 4) & 1u) != 0u, fge = ((fl >> 5) & 1u) != 0u, ate = ((fl >> 6) & 1u) != 0u;
    uint tfx = (fl >> 1) & 3u, tcc = (fl >> 3) & 1u, atst = (fl >> 7) & 7u, aref = (fl >> 10) & 0xffu, afail = (fl >> 22) & 3u;
    bool lin = ((fl >> 18) & 1u) != 0u, rs = ((fl >> 19) & 1u) != 0u, rt = ((fl >> 20) & 1u) != 0u;
    vec4 c = iip ? vColor : vFlat;
    vec3 c_game = c.rgb;
    // (the modern lighting: the sun's share of the surface's colour - taken as 55 %, our choice: the game's colour holds no light term
    // to take apart - hidden where the shadow maps say so, and the point lights added; nothing with the original lighting)
    c.rgb = sh_points(sh_apply_split(c.rgb, c.rgb * 0.45));
    vec4 o = c;
    if (tme) {
        ivec2 sz = ivec2(int(vTex.y & 0xffffu), int(vTex.y >> 16));
        vec2 uv = sh_mat_uv(vStq.xy / vStq.z, vEmis) * vec2(sz);  // (lava, mud, quicksand: the materials' slow flow)
        int layer = int(vTex.x);
        vec4 t;
        if (lin) {
            vec2 p = uv - 0.5;
            ivec2 i0 = ivec2(floor(p));
            vec2 f = p - vec2(i0);
            vec4 t00 = texelFetch(uAtlas, ivec3(wrapc(i0, sz, rs, rt), layer), 0);
            vec4 t10 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 0), sz, rs, rt), layer), 0);
            vec4 t01 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(0, 1), sz, rs, rt), layer), 0);
            vec4 t11 = texelFetch(uAtlas, ivec3(wrapc(i0 + ivec2(1, 1), sz, rs, rt), layer), 0);
            t = mix(mix(t00, t10, f.x), mix(t01, t11, f.x), f.y);
        } else {
            t = texelFetch(uAtlas, ivec3(wrapc(ivec2(floor(uv)), sz, rs, rt), layer), 0);
        }
        if (tfx == 0u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = tcc != 0u ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (tfx == 1u) { o.rgb = t.rgb; o.a = tcc != 0u ? t.a : c.a; }
        else if (tfx == 2u) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = tcc != 0u ? t.a : c.a; }
        o.rgb = sh_emissive(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, vEmis);
        o.rgb = sh_material(o.rgb, tfx == 1u ? t.rgb : t.rgb * c_game * (255.0 / 128.0), t.rgb, tfx == 1u ? vec3(1.0) : c.rgb * (255.0 / 128.0), vEmis);
        if (uShade == 2) o.rgb = t.rgb;
    }
    if (uShade == 1) o.rgb = c.rgb;
    o = clamp(o, 0.0, 1.0);
    if (fge) {  // F: the integer part of clamp(w + vf30.w, vf7.x, vf7.y), its low byte (FTOI4, XYZF2's F)
        uint fc = vTex.w;
        vec3 fogcol = vec3(float(fc & 0xffu), float((fc >> 8) & 0xffu), float((fc >> 16) & 0xffu)) / 255.0;
        float fw = min(max(vFogW, vFogC.x), vFogC.y);
        float f = float(uint(max(fw, 0.0)) & 0xffu) / 255.0;
        o.rgb = o.rgb * f + fogcol * (1.0 - f);
    }
    float a = o.a * 255.0;
    if (ate) {
        bool pass = true;
        if (atst == 0u) pass = false;
        else if (atst == 2u) pass = a < float(aref);
        else if (atst == 3u) pass = a <= float(aref);
        else if (atst == 4u) pass = abs(a - float(aref)) < 0.5;
        else if (atst == 5u) pass = a >= float(aref);
        else if (atst == 6u) pass = a > float(aref);
        else if (atst == 7u) pass = abs(a - float(aref)) >= 0.5;
        if (!pass && uPass == 0 && (afail == 0u || afail == 2u)) discard;
        if (!pass && uPass == 1 && afail != 2u) discard;
    }
    float as = min(o.a * (255.0 / 128.0), 1.0);
    uint bk = (fl >> 24) & 3u;
    if (bk == 1u || bk == 2u) {
        FragColor = vec4(o.rgb * as, as);
        FragFactor = vec4(bk == 1u ? 1.0 : 1.0 - as);
    } else {
        FragColor = vec4(o.rgb, as);
        FragFactor = vec4(0.0);
    }
}
)";

GLuint g7_prog = 0, g7_vao = 0, g7_vbo = 0;
GLint g7_loc_vp = -1, g7_loc_atlas = -1, g7_loc_shade = -1, g7_loc_pass = -1;
ShadowLocs g7_sh;
bool g7_failed = false;

// orig: (host) the surfaces' GL program and vertex array (false: not made)
bool init_gl_vu7() {
    if (g7_prog != 0) return true;
    if (g7_failed) return false;
    const GLuint vs = compile(GL_VERTEX_SHADER, kV7Vs), fs = compile(GL_FRAGMENT_SHADER, kV7Fs);
    g7_prog = glCreateProgram();
    glAttachShader(g7_prog, vs);
    glAttachShader(g7_prog, fs);
    glLinkProgram(g7_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);
    GLint ok = 0;
    glGetProgramiv(g7_prog, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(g7_prog, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp-fx] program 7 surfaces: link: %s\n", log);
        glDeleteProgram(g7_prog);
        g7_prog = 0;
        g7_failed = true;
        return false;
    }
    g7_loc_vp = glGetUniformLocation(g7_prog, "uVP");
    g7_loc_atlas = glGetUniformLocation(g7_prog, "uAtlas");
    g7_loc_shade = glGetUniformLocation(g7_prog, "uShade");
    g7_loc_pass = glGetUniformLocation(g7_prog, "uPass");
    g7_sh = mrp_lighting_locs(g7_prog);
    glGenVertexArrays(1, &g7_vao);
    glGenBuffers(1, &g7_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g7_vbo, "MRP program 7 surfaces");
    glBindVertexArray(g7_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g7_vbo);
    const GLsizei S = sizeof(V7);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(V7, wp)));
    glVertexAttribPointer(1, 3, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(V7, stq)));
    glVertexAttribPointer(2, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(V7, fogw)));
    glVertexAttribIPointer(3, 4, GL_UNSIGNED_BYTE, S, reinterpret_cast<void*>(offsetof(V7, rgba)));
    glVertexAttribIPointer(4, 4, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(V7, tex)));
    glVertexAttribPointer(5, 2, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(V7, fogc)));
    glVertexAttribIPointer(6, 1, GL_UNSIGNED_INT, S, reinterpret_cast<void*>(offsetof(V7, emis)));
    glVertexAttribPointer(7, 1, GL_FLOAT, GL_FALSE, S, reinterpret_cast<void*>(offsetof(V7, depth)));
    for (GLuint a = 0; a < 8; ++a) glEnableVertexAttribArray(a);
    glBindVertexArray(0);
    glBindBuffer(GL_ARRAY_BUFFER, 0);
    return true;
}

struct V7VkPush {
    float uVP[16];
    int32_t uShade;
    int32_t uPass;
    int32_t _pad[2];
};
static_assert(sizeof(V7VkPush) == 80);
VkShaderModule g7_vk_vs = VK_NULL_HANDLE, g7_vk_fs = VK_NULL_HANDLE;
VkPipelineLayout g7_vk_layout = VK_NULL_HANDLE;

void shutdown_vk_vu7() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        if (g7_vk_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g7_vk_layout, nullptr);
        if (g7_vk_vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g7_vk_vs, nullptr);
        if (g7_vk_fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g7_vk_fs, nullptr);
    }
    g7_vk_layout = VK_NULL_HANDLE;
    g7_vk_vs = g7_vk_fs = VK_NULL_HANDLE;
}

// orig: (host) the surfaces' Vulkan shaders and pipeline layout (set 0 the effects' atlas, set 1 the lighting's)
bool init_vk_vu7() {
    if (g7_vk_layout != VK_NULL_HANDLE) return true;
    if (!init_vk()) return false;
    VkDevice dev = vk::device();
    g7_vk_vs = vk::create_shader_module(vk::shaders::kMrpVu7Vs, sizeof(vk::shaders::kMrpVu7Vs));
    g7_vk_fs = vk::create_shader_module(vk::shaders::kMrpVu7Fs, sizeof(vk::shaders::kMrpVu7Fs));
    if (g7_vk_vs == VK_NULL_HANDLE || g7_vk_fs == VK_NULL_HANDLE) return false;
    VkDescriptorSetLayout sets[2] = {g_vk_set_layout, reinterpret_cast<VkDescriptorSetLayout>(mrp_lighting_vk_set_layout())};
    VkPushConstantRange pcr{};
    pcr.stageFlags = VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT;
    pcr.offset = 0;
    pcr.size = sizeof(V7VkPush);
    VkPipelineLayoutCreateInfo plci{};
    plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
    plci.setLayoutCount = 2;
    plci.pSetLayouts = sets;
    plci.pushConstantRangeCount = 1;
    plci.pPushConstantRanges = &pcr;
    if (vkCreatePipelineLayout(dev, &plci, nullptr, &g7_vk_layout) != VK_SUCCESS) return false;
    vk::register_shutdown_callback(shutdown_vk_vu7);
    return true;
}

// orig: (host) the batches [from, to) of `bs` drawn with the bound GL program and vertex array (the GS state as GL state: blending,
// the depth test and writes, the alpha test's split passes; `loc_pass` the program's uPass)
void gl_draw_batches(const std::vector<Batch>& bs, size_t from, size_t to, GLint loc_pass) {
    bool have = false;
    BKey cur;
    for (size_t bi = from; bi < to; ++bi) {
        const Batch& b = bs[bi];
        if (b.count == 0) continue;
        const BKey& k = b.key;
        if (!have || k.blend != cur.blend || k.alpha != cur.alpha || k.dual != cur.dual) {
            if (k.dual) {
                glEnable(GL_BLEND);
                glBlendEquationSeparate(GL_FUNC_ADD, GL_FUNC_ADD);
                glBlendFuncSeparate(GL_ONE, GL_SRC1_COLOR, GL_ONE, GL_ZERO);  // (the alpha written is the source's, as gs::blend_state)
            } else {
                gs::DrawState ds{};
                ds.prim = k.blend ? (1ull << 6) : 0;
                ds.alpha = k.alpha;
                gs::blend_state(ds);
            }
        }
        if (!have || k.ztest != cur.ztest || k.ztst != cur.ztst) {
            if (k.ztest) {  // the GS's GEQUAL / GREATER on the host's depth: LEQUAL / LESS
                glEnable(GL_DEPTH_TEST);
                glDepthFunc(k.ztst == 0 ? GL_NEVER : k.ztst == 2 ? GL_LEQUAL : GL_LESS);
            } else {
                glDisable(GL_DEPTH_TEST);
            }
        }
        glDepthMask(k.zwrite ? GL_TRUE : GL_FALSE);
        if (k.split) {  // the Z of the pixels the GS writes it for first, then the colour against it with LEQUAL
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
            glUniform1i(loc_pass, 1);
            glDrawArrays(k.lines ? GL_LINES : GL_TRIANGLES, static_cast<GLint>(b.first), static_cast<GLsizei>(b.count));
            glUniform1i(loc_pass, 0);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glDepthMask(GL_FALSE);
            if (k.ztst != 0) glDepthFunc(GL_LEQUAL);
            ++g_st.draws;
        }
        glDrawArrays(k.lines ? GL_LINES : GL_TRIANGLES, static_cast<GLint>(b.first), static_cast<GLsizei>(b.count));
        if (k.split) {
            glDepthMask(GL_TRUE);
            glDepthFunc(k.ztst == 0 ? GL_NEVER : k.ztst == 2 ? GL_LEQUAL : GL_LESS);
        }
        ++g_st.draws;
        ++g_st.batches;
        cur = k;
        have = true;
    }
}

// orig: (host) the batches [from, to) of `bs` drawn on Vulkan: `base` the pipeline (shaders, layout, vertex input), `push` the push
// constants of `push_size` bytes whose int at `pass_at` is uPass
void vk_draw_batches(const std::vector<Batch>& bs, size_t from, size_t to, const vk::GraphicsPipelineDesc& base, VkPipelineLayout layout, void* push,
                     uint32_t push_size, size_t pass_at) {
    VkCommandBuffer cb = vk::cmd();
    auto set_pass = [&](int32_t v) {
        std::memcpy(static_cast<uint8_t*>(push) + pass_at, &v, 4);
        vkCmdPushConstants(cb, layout, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, 0, push_size, push);
    };
    for (size_t bi = from; bi < to; ++bi) {
        const Batch& b = bs[bi];
        if (b.count == 0) continue;
        const BKey& k = b.key;
        vk::GraphicsPipelineDesc desc = base;
        desc.topology = k.lines ? VK_PRIMITIVE_TOPOLOGY_LINE_LIST : VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
        float fix = 0.0f;
        if (k.dual) {
            desc.blend_enable = true;
            desc.src_color_blend = VK_BLEND_FACTOR_ONE;
            desc.dst_color_blend = VK_BLEND_FACTOR_SRC1_COLOR;
            desc.color_blend_op = VK_BLEND_OP_ADD;
            desc.src_alpha_blend = VK_BLEND_FACTOR_ONE;
            desc.dst_alpha_blend = VK_BLEND_FACTOR_ZERO;
            desc.alpha_blend_op = VK_BLEND_OP_ADD;
        } else {
            gs::DrawState ds_blend{};
            ds_blend.prim = k.blend ? (1ull << 6) : 0;
            ds_blend.alpha = k.alpha;
            gs::vk_blend_state(ds_blend, false, desc, fix);
        }
        const float bc[4] = {0.0f, 0.0f, 0.0f, fix};
        vkCmdSetBlendConstants(cb, bc);
        desc.depth_test = k.ztest;
        desc.depth_compare = k.ztst == 0 ? VK_COMPARE_OP_NEVER : k.ztst == 2 ? VK_COMPARE_OP_LESS_OR_EQUAL : VK_COMPARE_OP_LESS;
        if (k.split) {
            desc.depth_write = true;
            desc.color_write_mask = 0;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vk::create_graphics_pipeline(desc));
            set_pass(1);
            vkCmdDraw(cb, b.count, 1, b.first, 0);
            ++g_st.draws;
            desc.depth_write = false;
            desc.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            if (k.ztst != 0) desc.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vk::create_graphics_pipeline(desc));
            set_pass(0);
            vkCmdDraw(cb, b.count, 1, b.first, 0);
        } else {
            desc.depth_write = k.zwrite;
            desc.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT | VK_COLOR_COMPONENT_A_BIT;
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, vk::create_graphics_pipeline(desc));
            set_pass(0);
            vkCmdDraw(cb, b.count, 1, b.first, 0);
        }
        ++g_st.draws;
        ++g_st.batches;
    }
}

}  // namespace

// orig: (host) see mrp_fx.h
void mrp_fx_draw(const gs::Gs& gs, int win_w, int win_h, const gs::Viewport* vp, const std::function<void(size_t, size_t)>& gs_draw, size_t from,
                 size_t to) {
    const std::vector<gs::Primitive>& prims = gs.primitives();
    to = std::min(to, prims.size());
    if (from >= to) return;
    if (!gs.fx_external() || (g_frame.notes.empty() && gs.vu7_batches().empty()) || !init_gl()) {
        if (gs_draw) gs_draw(from, to);
        return;
    }
    using clock = std::chrono::steady_clock;
    const auto t0 = clock::now();
    if (from == 0) ++g_st.frames;
    g_v.clear();
    g_batches.clear();
    g_steps.clear();
    g_v7.clear();
    g_b7.clear();
    if (from == 0 || g_e7.gen == 0) vu7_emissive_refresh();
    float vpm[16];
    mrp_world_view_proj(vpm);
    glm::dmat4 VP;
    for (int c = 0; c < 4; ++c)
        for (int r = 0; r < 4; ++r) VP[c][r] = vpm[c * 4 + r];
    const bool vu_mode = g_mrp_world_projection != 0;
    g_v7_depth = render_gs_depth(g_v7_da, g_v7_db);
    uint64_t env_xyoff = 0;
    // the run in progress: kStepGs from gs_from, or an MRP step (its batches from g_steps.back().from)
    int run = kStepGs;
    size_t gs_from = from;
    auto switch_to = [&](int kind, size_t pi) {
        if (kind == run) return;
        if (run == kStepGs) {
            if (pi > gs_from) g_steps.push_back(Step{kStepGs, gs_from, pi});
        } else {
            g_steps.back().to = run == kStepFx ? g_batches.size() : g_b7.size();
        }
        run = kind;
        if (kind == kStepGs) {
            gs_from = pi;
        } else if (kind == kStepFx) {
            g_steps.push_back(Step{kStepFx, g_batches.size(), g_batches.size()});
            g_batches.push_back(Batch{});  // (a new step starts a new batch)
            g_batches.back().first = static_cast<uint32_t>(g_v.size());
            g_batches.back().count = 0;
        } else {
            g_steps.push_back(Step{kStepVu7, g_b7.size(), g_b7.size()});
            g_b7.push_back(Batch{});
            g_b7.back().first = static_cast<uint32_t>(g_v7.size());
            g_b7.back().count = 0;
        }
    };
    for (size_t pi = from; pi < to; ++pi) {
        const gs::Primitive& p = prims[pi];
        // the MRP's: fx, not the real block's while the effects shadow's stands in for it (as gs::draw: those are dropped)
        if (p.fx == 0) {
            switch_to(kStepGs, pi);
            continue;
        }
        if (p.sub == 2) continue;
        if (p.fx == kMrpFxVu7) {  // (program 7's batches: drawn from their input; their kicks are not drawn)
            if (p.type != 9) continue;
            if (g_render_fx) {
                switch_to(kStepVu7, pi);
                build_vu7(gs, p);
            }
            continue;
        }
        if (p.type != 3 && p.type != 6 && p.type != 1) continue;
        if (p.fx_id >= g_frame.notes.size()) continue;
        const FrameNote& fn = g_frame.notes[p.fx_id];
        const View& V = g_frame.views[fn.view];
        const gs::DrawState& st = p.st;
        // (the game's rain / snow, where our ambient particles replace it: not drawn - the game made it as ever)
        if (g_ambient_particles > 0 && mrp_particles_hide_fx(fn.kind, st.tex0, p.type)) continue;
        const bool tme = fld(st.prim, 4, 1) != 0, fst = fld(st.prim, 8, 1) != 0;
        int layer = 0, tw = 1, th = 1;
        if (tme) {
            if (p.tex == nullptr) continue;
            layer = atlas_layer(*p.tex);
            if (layer < 0) continue;
            tw = p.tex->w;
            th = p.tex->h;
            if (p.mip_n > 0) ++g_st.tex_mip;  // (the base level only: the effects' TEX1 have none)
        }
        switch_to(kStepFx, pi);
        if (fld(st.clamp, 0, 2) >= 2 || fld(st.clamp, 2, 2) >= 2) ++g_st.region;
        if (fst) ++g_st.fst;
        const bool fge = fld(st.prim, 5, 1) != 0;
        if (fge) ++g_st.fge;
        if (g_env_states) {  // (testing: the GS states the effects are drawn with)
            const uint64_t key = (static_cast<uint64_t>(fn.kind) << 60) ^ (st.test & 0xffffff) ^ (static_cast<uint64_t>(fld(st.zbuf, 32, 1)) << 24) ^
                                 (static_cast<uint64_t>(fld(st.frame, 0, 9)) << 25) ^ ((st.alpha & 0xff) << 34) ^ (static_cast<uint64_t>(fld(st.prim, 6, 1)) << 42) ^
                                 (static_cast<uint64_t>(fld(st.frame, 32, 32) != 0) << 43);
            ++g_states[key];
        }
        BKey k;
        uint32_t flags = 0;
        key_and_flags(st, p.type == 1, k, flags);
        const double ox = static_cast<double>(fld(st.xyoffset, 0, 16)) / 16.0, oy = static_cast<double>(fld(st.xyoffset, 32, 16)) / 16.0;
        env_xyoff = st.xyoffset;
        // a vertex of the GS's (its pixel x / y, Z, S, T, Q, colour, F) as the MRP's
        auto mk = [&](float x, float y, uint32_t z, float s, float t, float q, const gs::Vertex& col) {
            const double X = static_cast<double>(x) + ox, Y = static_cast<double>(y) + oy;
            const glm::dvec4 wp = world_of(V, X, Y, static_cast<double>(z));
            if (g_env_verify) {  // (testing: the producer's own formula back from the world point, and its input)
                const glm::dvec3 sc = screen_of(V, glm::dvec3(wp));
                const double exy = std::max(std::fabs(sc.x - X), std::fabs(sc.y - Y)), ez = std::fabs(sc.z - static_cast<double>(z));
                ++g_st.ver_verts;
                if (exy * 16.0 >= 0.5) ++g_st.ver_xy_bad;
                if (ez >= 0.5) ++g_st.ver_z_bad;
                g_st.ver_max_xy = std::max(g_st.ver_max_xy, exy * 16.0);
                g_st.ver_max_z = std::max(g_st.ver_max_z, ez);
                if (fn.kind != kMrpFxParticles) verify_vertex(fn, V, X, Y, static_cast<double>(z));
            }
            FV v{};
            for (int l = 0; l < 4; ++l) v.wp[l] = static_cast<float>(wp[l]);
            v.vu[0] = static_cast<float>(X);
            v.vu[1] = static_cast<float>(Y);
            v.vu[2] = static_cast<float>(z);
            double w = vu_mode ? 1.0 : (VP * wp).w;
            if (!(w > 1e-9)) w = 1e-9;
            if (fst) {  // (UV in texels)
                v.stq[0] = static_cast<float>(static_cast<double>(s) / tw * w);
                v.stq[1] = static_cast<float>(static_cast<double>(t) / th * w);
                v.stq[2] = static_cast<float>(w);
            } else {
                v.stq[0] = static_cast<float>(static_cast<double>(s) * w);
                v.stq[1] = static_cast<float>(static_cast<double>(t) * w);
                v.stq[2] = static_cast<float>(static_cast<double>(q != 0.0f ? q : 1.0f) * w);
            }
            v.fog = static_cast<float>(col.fog);
            v.rgba[0] = col.r;
            v.rgba[1] = col.g;
            v.rgba[2] = col.b;
            v.rgba[3] = col.a;
            v.tex[0] = static_cast<uint32_t>(layer);
            v.tex[1] = static_cast<uint32_t>(tw) | static_cast<uint32_t>(th) << 16;
            v.tex[2] = flags;
            v.tex[3] = static_cast<uint32_t>(st.fogcol & 0xffffff);
            return v;
        };
        auto st_of = [fst](const gs::Vertex& a) { return fst ? std::array<float, 2>{a.u, a.v} : std::array<float, 2>{a.s, a.t}; };
        const size_t at = g_v.size();
        if (p.type == 6) {  // a sprite: two triangles, the corners' S / T from the two vertices' (at the second's Q), its colour, Z, F
            const gs::Vertex &a = p.v[0], &b = p.v[1];
            const auto sa = st_of(a), sb = st_of(b);
            const float qa = a.q != 0.0f ? a.q : 1.0f;
            const float as = fst ? sa[0] : sa[0] / qa * b.q, at_ = fst ? sa[1] : sa[1] / qa * b.q;
            const FV c0 = mk(a.x, a.y, b.z, as, at_, b.q, b), c1 = mk(b.x, a.y, b.z, sb[0], at_, b.q, b), c2 = mk(a.x, b.y, b.z, as, sb[1], b.q, b),
                     c3 = mk(b.x, b.y, b.z, sb[0], sb[1], b.q, b);
            g_v.insert(g_v.end(), {c0, c1, c2, c1, c3, c2});
            ++g_st.sprites;
        } else {
            const int nv = p.type == 3 ? 3 : 2;
            for (int i = 0; i < nv; ++i) {
                const gs::Vertex& pv = p.v[static_cast<size_t>(i)];
                const auto s2 = st_of(pv);
                g_v.push_back(mk(pv.x, pv.y, pv.z, s2[0], s2[1], pv.q, pv));
            }
            ++(p.type == 3 ? g_st.tris : g_st.lines);
        }
        ++g_st.prims;
        ++g_st.by_kind[fn.kind];
        const uint32_t n = static_cast<uint32_t>(g_v.size() - at);
        Batch& last = g_batches.back();
        if (last.count != 0 && !(last.key == k)) g_batches.push_back(Batch{k, static_cast<uint32_t>(at), 0});
        else if (last.count == 0) last.key = k;
        g_batches.back().count += n;
    }
    switch_to(kStepGs, to);  // (the run in progress closed)
    if (gs_from < to) g_steps.push_back(Step{kStepGs, gs_from, to});
    const auto t1 = clock::now();
    g_st.ms_build += std::chrono::duration<double, std::milli>(t1 - t0).count();
    if (g_env_debug && g_st.frames % 60 == 1)
        std::printf("[mrp-fx] frame %llu: %zu vertices, %zu batches, %zu steps, atlas %d layers, %zu program 7 vertices\n", static_cast<unsigned long long>(g_st.frames),
                    g_v.size(), g_batches.size(), g_steps.size(), g_atlas.used, g_v7.size());
    g_st.verts += g_v.size();
    double ms_gs = 0.0;
    bool uploaded = false, uploaded7 = false;
    vk::RingAlloc vk_vbo_alloc{}, vk_vbo7_alloc{};
    VkDescriptorSet vk_ds = VK_NULL_HANDLE;
    float fb_w = static_cast<float>(lhu(0x001518d0)), fb_h = static_cast<float>(lhu(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;
    auto vk_atlas_set = [&] {  // (the atlas's descriptor set, once a frame)
        if (vk_ds != VK_NULL_HANDLE) return;
        if (g_vk_atlas_img.image == VK_NULL_HANDLE) atlas_grow(g_vk_atlas, 32);
        vk_ds = vk::alloc_descriptor_set(g_vk_set_layout);
        VkDescriptorImageInfo dii{};
        dii.sampler = g_vk_sampler;
        dii.imageView = g_vk_atlas_img.view != VK_NULL_HANDLE ? g_vk_atlas_img.view : vk::white_texture_array().view;
        dii.imageLayout = VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
        VkWriteDescriptorSet wds{};
        wds.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        wds.dstSet = vk_ds;
        wds.dstBinding = 0;
        wds.descriptorCount = 1;
        wds.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        wds.pImageInfo = &dii;
        vkUpdateDescriptorSets(vk::device(), 1, &wds, 0, nullptr);
    };
    for (const Step& sp : g_steps) {
        if (!g_render_fx) break;  // (the debug menu's effects layer hidden)
        if (sp.kind == kStepGs) {  // the GS replica's run (its own GL state)
            if (gs_draw) {
                const auto g0 = clock::now();
                gs_draw(sp.from, sp.to);
                ms_gs += std::chrono::duration<double, std::milli>(clock::now() - g0).count();
                ++g_st.gs_runs;
            }
            continue;
        }
        if (g_env_hide) continue;  // (testing)
        if (sp.kind == kStepVu7) {  // program 7's surfaces
            if (g_v7.empty() || sp.from >= sp.to) continue;
            if (vk::is_active()) {
                if (!init_vk_vu7()) continue;
                if (!uploaded7) {
                    vk_vbo7_alloc = vk::alloc_ring(g_v7.size() * sizeof(V7), 16);
                    std::memcpy(vk_vbo7_alloc.ptr, g_v7.data(), g_v7.size() * sizeof(V7));
                    uploaded7 = true;
                }
                vk_atlas_set();
                vk::ensure_rendering();
                if (vp != nullptr) vk::set_viewport(vp->x, vp->y, vp->w, vp->h);
                else vk::set_viewport(0, 0, win_w, win_h);
                vk::set_scissor(false);
                VkCommandBuffer cb = vk::cmd();
                vkCmdBindVertexBuffers(cb, 0, 1, &vk_vbo7_alloc.buffer, &vk_vbo7_alloc.offset);
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g7_vk_layout, 0, 1, &vk_ds, 0, nullptr);
                const uint64_t pl = reinterpret_cast<uint64_t>(g7_vk_layout);
                if (mrp_lighting_active()) mrp_lighting_vk_bind_receiver(pl, false, true);
                else mrp_lighting_vk_bind_off(pl);
                V7VkPush pc{};
                std::memcpy(pc.uVP, vpm, sizeof(pc.uVP));
                pc.uShade = g_render_shade;
                vk::GraphicsPipelineDesc base{};
                base.layout = g7_vk_layout;
                base.vs = g7_vk_vs;
                base.fs = g7_vk_fs;
                base.vertex_bindings = {{0, sizeof(V7), VK_VERTEX_INPUT_RATE_VERTEX}};
                base.vertex_attribs = {
                    {0, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(V7, wp))},
                    {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(V7, stq))},
                    {2, 0, VK_FORMAT_R32_SFLOAT, static_cast<uint32_t>(offsetof(V7, fogw))},
                    {3, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(V7, rgba))},
                    {4, 0, VK_FORMAT_R32G32B32A32_UINT, static_cast<uint32_t>(offsetof(V7, tex))},
                    {5, 0, VK_FORMAT_R32G32_SFLOAT, static_cast<uint32_t>(offsetof(V7, fogc))},
                    {6, 0, VK_FORMAT_R32_UINT, static_cast<uint32_t>(offsetof(V7, emis))},
                    {7, 0, VK_FORMAT_R32_SFLOAT, static_cast<uint32_t>(offsetof(V7, depth))},
                };
                base.cull_mode = VK_CULL_MODE_NONE;
                base.depth_clamp = true;  // (as the OpenGL path)
                vk_draw_batches(g_b7, sp.from, sp.to, base, g7_vk_layout, &pc, sizeof(pc), offsetof(V7VkPush, uPass));
                continue;
            }
            if (!init_gl_vu7()) continue;
            if (vp != nullptr) glViewport(vp->x, vp->y, vp->w, vp->h);
            else glViewport(0, 0, win_w, win_h);
            glUseProgram(g7_prog);
            glUniformMatrix4fv(g7_loc_vp, 1, GL_FALSE, vpm);
            glUniform1i(g7_loc_atlas, 0);
            glUniform1i(g7_loc_shade, g_render_shade);
            glUniform1i(g7_loc_pass, 0);
            if (mrp_lighting_active()) mrp_lighting_bind_receiver(g7_sh);
            else mrp_lighting_bind_off(g7_sh);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas.tex);
            glBindSampler(0, 0);
            glBindVertexArray(g7_vao);
            glBindBuffer(GL_ARRAY_BUFFER, g7_vbo);
            if (!uploaded7) {
                glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g_v7.size() * sizeof(V7)), g_v7.data(), GL_STREAM_DRAW);
                uploaded7 = true;
            }
            glDisable(GL_CULL_FACE);
            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_CLAMP);  // (the replica clamps the mapped depth to the buffer's range)
            gl_draw_batches(g_b7, sp.from, sp.to, g7_loc_pass);
            glDisable(GL_DEPTH_CLAMP);
            mrp_lighting_bind_off(g7_sh);
            glBindVertexArray(0);
            glBindBuffer(GL_ARRAY_BUFFER, 0);
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            glUseProgram(0);
            glDisable(GL_BLEND);
            glDepthMask(GL_TRUE);
            glDepthFunc(GL_LEQUAL);
            glDisable(GL_DEPTH_TEST);
            continue;
        }
        if (vk::is_active()) {
            if (!uploaded && !g_v.empty()) {
                vk_vbo_alloc = vk::alloc_ring(g_v.size() * sizeof(FV), 16);
                std::memcpy(vk_vbo_alloc.ptr, g_v.data(), g_v.size() * sizeof(FV));
                uploaded = true;
            }
            if (g_v.empty()) continue;
            vk_atlas_set();
            vk::ensure_rendering();
            if (vp != nullptr) vk::set_viewport(vp->x, vp->y, vp->w, vp->h);
            else vk::set_viewport(0, 0, win_w, win_h);
            vk::set_scissor(false);
            VkCommandBuffer cb = vk::cmd();
            vkCmdBindVertexBuffers(cb, 0, 1, &vk_vbo_alloc.buffer, &vk_vbo_alloc.offset);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_pipe_layout, 0, 1, &vk_ds, 0, nullptr);

            FxVkPush pc{};
            std::memcpy(pc.uVP, vpm, sizeof(pc.uVP));
            pc.uFrame[0] = fb_w;
            pc.uFrame[1] = fb_h;
            pc.uFrame[2] = vp != nullptr ? vp->xscale_3d : 1.0f;
            pc.uFrame[3] = 0.0f;
            double da = 0.0, db = 0.0;
            const bool has = render_gs_depth(da, db);
            pc.uXyOff[0] = static_cast<float>(fld(env_xyoff, 0, 16)) / 16.0f + fb_w * 0.5f;
            pc.uXyOff[1] = static_cast<float>(fld(env_xyoff, 32, 16)) / 16.0f + fb_h * 0.5f;
            pc.uXyOff[2] = has ? static_cast<float>(da) : 0.5f;
            pc.uXyOff[3] = has ? static_cast<float>(db) : 0.0f;
            pc.uMode = vu_mode ? 1 : 0;
            pc.uShade = g_render_shade;
            pc.uPass = 0;

            vk::GraphicsPipelineDesc base_desc{};
            base_desc.layout = g_vk_pipe_layout;
            base_desc.vs = g_vk_vs;
            base_desc.fs = g_vk_fs;
            base_desc.vertex_bindings = {{0, sizeof(FV), VK_VERTEX_INPUT_RATE_VERTEX}};
            base_desc.vertex_attribs = {
                {0, 0, VK_FORMAT_R32G32B32A32_SFLOAT, static_cast<uint32_t>(offsetof(FV, wp))},
                {1, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(FV, stq))},
                {2, 0, VK_FORMAT_R32_SFLOAT, static_cast<uint32_t>(offsetof(FV, fog))},
                {3, 0, VK_FORMAT_R8G8B8A8_UINT, static_cast<uint32_t>(offsetof(FV, rgba))},
                {4, 0, VK_FORMAT_R32G32B32A32_UINT, static_cast<uint32_t>(offsetof(FV, tex))},
                {5, 0, VK_FORMAT_R32G32B32_SFLOAT, static_cast<uint32_t>(offsetof(FV, vu))},
            };
            base_desc.cull_mode = VK_CULL_MODE_NONE;
            vk_draw_batches(g_batches, sp.from, sp.to, base_desc, g_vk_pipe_layout, &pc, sizeof(pc), offsetof(FxVkPush, uPass));
            continue;
        }
        // the GL state
        if (vp != nullptr) glViewport(vp->x, vp->y, vp->w, vp->h);
        else glViewport(0, 0, win_w, win_h);
        glUseProgram(g_prog);
        glUniformMatrix4fv(g_loc_vp, 1, GL_FALSE, vpm);
        glUniform1i(g_loc_atlas, 0);
        glUniform1i(g_loc_shade, g_render_shade);
        glUniform1i(g_loc_mode, vu_mode ? 1 : 0);
        glUniform1i(g_loc_pass, 0);
        {  // (the VU projection: the frame's centre in GS pixels - XYOFFSET plus half the frame -, the depth map of the replica)
            glUniform4f(g_loc_frame, fb_w, fb_h, vp != nullptr ? vp->xscale_3d : 1.0f, 0.0f);
            double da = 0.0, db = 0.0;
            const bool has = render_gs_depth(da, db);
            glUniform4f(g_loc_xyoff, static_cast<float>(fld(env_xyoff, 0, 16)) / 16.0f + fb_w * 0.5f, static_cast<float>(fld(env_xyoff, 32, 16)) / 16.0f + fb_h * 0.5f,
                        has ? static_cast<float>(da) : 0.5f, has ? static_cast<float>(db) : 0.0f);
        }
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, g_atlas.tex);
        glBindSampler(0, 0);
        glBindVertexArray(g_vao);
        glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
        if (!uploaded) {
            glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(g_v.size() * sizeof(FV)), g_v.data(), GL_STREAM_DRAW);
            uploaded = true;
        }
        glDisable(GL_CULL_FACE);
        glDisable(GL_SCISSOR_TEST);
        gl_draw_batches(g_batches, sp.from, sp.to, g_loc_pass);
        glBindVertexArray(0);
        glBindBuffer(GL_ARRAY_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
        glUseProgram(0);
        glDisable(GL_BLEND);
        glDepthMask(GL_TRUE);
        glDepthFunc(GL_LEQUAL);
        glDisable(GL_DEPTH_TEST);
    }
    g_st.ms_gl += std::chrono::duration<double, std::milli>(clock::now() - t1).count() - ms_gs;
}

// orig: (host) see mrp_fx.h
bool mrp_fx_vu1_check(void* vu_p, uint32_t start, uint64_t hash, void (*fn)(VUState&, uint32_t, const std::function<void(uint32_t)>&),
                      const std::function<void(uint32_t)>& kick) {
    static const bool on = std::getenv("RAC1_MRP_FX_KICKCHECK") != nullptr;
    if (!on || hash != 0x901435981696d0d3ull) return false;
    vu1::Vu1& vu = *static_cast<vu1::Vu1*>(vu_p);
    // the original microcode the chain uploaded, on the VU interpreter, from the same state; then the port, as always
    vu1::Vu1 ref = vu;
    const vu1::Vu1 pre = vu;
    std::vector<std::vector<uint8_t>> want, got;
    auto grab = [](const vu1::Vu1& v, uint32_t addr, std::vector<std::vector<uint8_t>>& out) {
        const uint32_t qwc = vu1::gif_packet_qwc(v.st, addr);
        std::vector<uint8_t> b(static_cast<size_t>(qwc) * 16);
        for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&b[q * 16], &v.st.mem[((addr + q) & 0x3ff) << 4], 16);
        out.push_back(std::move(b));
    };
    vu1::run_interpreter(ref, start, [&](uint32_t addr) { grab(ref, addr, want); });
    fn(vu.st, start, [&](uint32_t addr) {
        grab(vu, addr, got);
        kick(addr);
    });
    ++g_st.kc_starts;
    bool bad = want.size() != got.size();
    for (size_t i = 0; i < std::min(want.size(), got.size()); ++i) {
        ++g_st.kc_kicks;
        const size_t n = std::max(want[i].size(), got[i].size()) / 16;
        g_st.kc_qw += n;
        for (size_t q = 0; q < n; ++q)
            if (q * 16 + 16 > want[i].size() || q * 16 + 16 > got[i].size() || std::memcmp(&want[i][q * 16], &got[i][q * 16], 16) != 0) {
                ++g_st.kc_bad_qw;
                bad = true;
                static int shown = 0;
                if (shown++ < 12 && q * 16 + 16 <= want[i].size() && q * 16 + 16 <= got[i].size()) {
                    uint32_t a[4], b[4];
                    std::memcpy(a, &want[i][q * 16], 16);
                    std::memcpy(b, &got[i][q * 16], 16);
                    std::printf("[mrp-fx] kick check: start %03x kick %zu qword %zu: interpreter %08x %08x %08x %08x port %08x %08x %08x %08x\n", start, i, q, a[0], a[1], a[2], a[3],
                                b[0], b[1], b[2], b[3]);
                }
            }
    }
    if (ref.st.mem != vu.st.mem) bad = true;  // (the data memory after: the same)
    if (bad) {
        ++g_st.kc_bad_starts;
        static int dumped = 0;
        if (const char* dir = std::getenv("RAC1_MRP_FX_KICKCHECK_DUMP"); dir != nullptr && dumped++ < 4) {  // (testing: the start's state, vu1.cpp's capture format)
            char path[512];
            std::snprintf(path, sizeof path, "%s/fx_bad_%03x_%d.bin", dir, start, dumped);
            if (FILE* f = std::fopen(path, "wb")) {
                const VUState& st = pre.st;
                const uint32_t hdr[4] = {0x31555676u, start, st.xtop, 0};
                std::fwrite(hdr, 4, 4, f);
                std::fwrite(st.vf.data(), sizeof(VUVector), 32, f);
                std::fwrite(st.vi.data(), 2, 16, f);
                std::fwrite(&st.acc, sizeof(VUVector), 1, f);
                const float fr[4] = {st.q, st.i, st.r, st.p};
                std::fwrite(fr, 4, 4, f);
                const uint32_t fl[4] = {st.macflag, st.statusflag, st.clipflag, 0};
                std::fwrite(fl, 4, 4, f);
                std::fwrite(st.mem.data(), 1, st.mem.size(), f);
                std::fwrite(pre.micro.data(), 8, pre.micro.size(), f);
                std::fclose(f);
            }
        }
    }
    return true;
}

// orig: (host) see mrp_fx.h
void mrp_fx_report() {
    for (const auto& [k, n] : g_states)
        std::printf("[mrp-fx] state: kind %llu TEST %06llx (ZTE %llu ZTST %llu ATE %llu ATST %llu AREF %02llx AFAIL %llu) ZMSK %llu FBP %llu ALPHA %02llx ABE %llu FBMSK %llu: %llu primitives\n",
                    static_cast<unsigned long long>(k >> 60), static_cast<unsigned long long>(k & 0xffffff), static_cast<unsigned long long>((k >> 16) & 1),
                    static_cast<unsigned long long>((k >> 17) & 3), static_cast<unsigned long long>(k & 1), static_cast<unsigned long long>((k >> 1) & 7),
                    static_cast<unsigned long long>((k >> 4) & 0xff), static_cast<unsigned long long>((k >> 12) & 3), static_cast<unsigned long long>((k >> 24) & 1),
                    static_cast<unsigned long long>((k >> 25) & 0x1ff), static_cast<unsigned long long>((k >> 34) & 0xff), static_cast<unsigned long long>((k >> 42) & 1),
                    static_cast<unsigned long long>((k >> 43) & 1), static_cast<unsigned long long>(n));
    if (g_st.frames == 0 && g_inventory.empty()) return;
    if (g_st.frames == 0) g_st.frames = 1;
    const double f = static_cast<double>(g_st.frames);
    const Atlas& A = vk::is_active() ? g_vk_atlas : g_atlas;
    std::printf("[mrp-fx] %llu frames: %llu primitives (%llu sprites, %llu triangles, %llu lines), %.1f vertices / %.2f batches / %.2f draws a frame; "
                "atlas %d textures (%llu too big, %llu with mips, %llu region clamps, %llu UV, %llu fogged); build %.3f ms, GL %.3f ms a frame\n",
                static_cast<unsigned long long>(g_st.frames), static_cast<unsigned long long>(g_st.prims), static_cast<unsigned long long>(g_st.sprites),
                static_cast<unsigned long long>(g_st.tris), static_cast<unsigned long long>(g_st.lines), static_cast<double>(g_st.verts) / f,
                static_cast<double>(g_st.batches) / f, static_cast<double>(g_st.draws) / f, A.used, static_cast<unsigned long long>(g_st.tex_big),
                static_cast<unsigned long long>(g_st.tex_mip), static_cast<unsigned long long>(g_st.region), static_cast<unsigned long long>(g_st.fst),
                static_cast<unsigned long long>(g_st.fge), g_st.ms_build / f, g_st.ms_gl / f);
    std::printf("[mrp-fx] atlas: %d of %d layers (cap %d), %llu reused, %llu textures not drawn (every layer in use)\n", A.used, A.layers, A.cap,
                static_cast<unsigned long long>(A.reused), static_cast<unsigned long long>(A.full));
    if (g_s7.batches != 0)
        std::printf("[mrp-fx] program 7 surfaces: %llu batches (%llu on the clipping path), %llu triangles drawn from their input; %llu in the world as they are, "
                    "%llu through their own matrix; %llu without a texture, %llu with mip levels (the base drawn), %llu not triangles\n",
                    static_cast<unsigned long long>(g_s7.batches), static_cast<unsigned long long>(g_s7.clip_path), static_cast<unsigned long long>(g_s7.tris),
                    static_cast<unsigned long long>(g_s7.world), static_cast<unsigned long long>(g_s7.general), static_cast<unsigned long long>(g_s7.no_tex),
                    static_cast<unsigned long long>(g_s7.mip), static_cast<unsigned long long>(g_s7.bad_type));
    if (g_st.kc_starts != 0)
        std::printf("[mrp-fx] kick check: %llu particle program starts, %llu kicks, %llu quadwords against the VU interpreter on the uploaded microcode: "
                    "%llu starts and %llu quadwords differ\n",
                    static_cast<unsigned long long>(g_st.kc_starts), static_cast<unsigned long long>(g_st.kc_kicks), static_cast<unsigned long long>(g_st.kc_qw),
                    static_cast<unsigned long long>(g_st.kc_bad_starts), static_cast<unsigned long long>(g_st.kc_bad_qw));
    if (g_env_verify)
        std::printf("[mrp-fx] verify: %llu vertices back on the GS's frame: X / Y off by >= 1/32 px %llu (max %.4f / 16 px), Z off by >= 0.5 %llu (max %.4f)\n",
                    static_cast<unsigned long long>(g_st.ver_verts), static_cast<unsigned long long>(g_st.ver_xy_bad), g_st.ver_max_xy,
                    static_cast<unsigned long long>(g_st.ver_z_bad), g_st.ver_max_z);
    {  // by producer
        static const char* names[kMrpFxKinds] = {"", "particles", "quads", "sparkles", "prim lists", "program 7 surfaces"};
        std::printf("[mrp-fx] by producer (ranges / primitives drawn):");
        for (int k = 1; k < kMrpFxKinds; ++k)
            std::printf(" %s %llu / %llu", names[k], static_cast<unsigned long long>(g_st.notes[k]), static_cast<unsigned long long>(g_st.by_kind[k]));
        std::printf("; GS replica runs between them %.2f a frame; ranges without a projection %llu\n", static_cast<double>(g_st.gs_runs) / f,
                    static_cast<unsigned long long>(g_st.no_view));
        if (g_env_verify)
            for (int k = 2; k < kMrpFxKinds; ++k)
                if (g_st.pv_verts[k] != 0)
                    std::printf("[mrp-fx] verify %s: %llu vertices against the producer's input: %llu at an input point projected (0.1 px, Z 4 + 5e-6 of Z - 0x16cbe8), %llu on its surface "
                                "(the clipper's), %llu on it in world space only (within 2e-4 of the distance; largest %.1e), %llu neither; largest Z difference %.1f\n",
                                names[k], static_cast<unsigned long long>(g_st.pv_verts[k]), static_cast<unsigned long long>(g_st.pv_at[k]),
                                static_cast<unsigned long long>(g_st.pv_on[k]), static_cast<unsigned long long>(g_st.pv_world[k]), g_st.pv_max_rel[k],
                                static_cast<unsigned long long>(g_st.pv_bad[k]), g_st.pv_max_plane[k]);
        if (g_env_inventory)
            for (const auto& [fn, I] : g_inventory)
                std::printf("[mrp-fx] inventory: draw list callback %08x: %llu calls, %llu chain bytes, %llu not in a producer's range%s\n", fn,
                            static_cast<unsigned long long>(I.calls), static_cast<unsigned long long>(I.bytes), static_cast<unsigned long long>(I.uncovered),
                            I.uncovered != 0 ? " (the GS replica draws these)" : "");
    }
}

}  // namespace rac1
