// orig: (host) The MRP's drop shadows (see mrp_shadows.h).
#include <game/render/mrp_shadows.h>
#include <profiler/host_profiler.h>

#include <game/ovl/moby_shadows.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/render_toggles.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>

#include <algorithm>
#include <bit>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace rac1 {

int g_mrp_shadows = 1;

namespace {

constexpr uint32_t SPR = 0x70000000u;     // the scratchpad: the point pairs at +0 (0x20 bytes each), the faces' bytes at +0x3000 / +0x3800
constexpr int kStencilStart = 0x80;        // the counter's start: the frame's alpha 0x7f is "below 0x80" as this is "not above the start"
constexpr uint32_t kMaxRecords = 0x180;    // the scratchpad's 0x3000 bytes of pairs

struct State {
    bool on = false;        // the MRP draws this frame's mobys: the shadows are captured
    bool captured = false;  // drop_shadows_draw ran
    uint32_t clear_rgba = 0;
    uint64_t apply_alpha = 0;
    uint32_t apply_rgba = 0;
    bool have_apply = false;
    uint32_t chain_first = 0, chain_last = 0;
    bool have_chain = false;
    // the tube in hand (before shadow_quads): its point pairs, world space
    uint32_t n = 0;
    std::vector<float> near_pts, far_pts;
    // the faces: triangles (3 x xyz) that mark / unmark
    std::vector<float> mark, unmark;
    // RAC1_MRP_SHADOW_CHECK=1: every face in the order the original draws it (9 floats and the alpha byte), the pass's result against a CPU run of
    // the original's integer alpha arithmetic on the same faces and depth buffer
    bool check = false;
    struct Face {
        float v[9];
        uint8_t a;
    };
    std::vector<Face> ordered;
    uint64_t chk_frames = 0, chk_pixels = 0, chk_marked = 0, chk_mismatch = 0, chk_border = 0, chk_skipped_tris = 0, chk_tris = 0;
    // totals
    uint64_t frames = 0, tubes = 0, mark_tris = 0, unmark_tris = 0, drawn_frames = 0, odd_alpha = 0;
    bool warned = false;
};
State S;
State D;  // (the faces and the darkening captured for a pipelined frame: mrp_shadows_snapshot)
struct CheckGs {
    uint64_t frames = 0, count_differs = 0, faces = 0, alpha_differs = 0, pos_differs = 0;
    double max_px = 0, max_dz = 0;
} g_chk_gs;

void obs_begin(uint32_t clear_rgba) {
    if (!S.on) return;
    S.captured = true;
    S.clear_rgba = clear_rgba;
    ++S.frames;
}

void obs_tube_points(uint32_t count, uint32_t) {
    if (!S.on) return;
    S.n = std::min(count, kMaxRecords);
    S.near_pts.resize(S.n * 3);
    S.far_pts.resize(S.n * 3);
    for (uint32_t i = 0; i < S.n; ++i)
        for (uint32_t k = 0; k < 3; ++k) {
            S.near_pts[i * 3 + k] = lwc1(SPR + 0x20 * i + 4 * k);
            S.far_pts[i * 3 + k] = lwc1(SPR + 0x20 * i + 0x10 + 4 * k);
        }
    ++S.tubes;
}

void tri(std::vector<float>& out, const float* a, const float* b, const float* c) {
    out.insert(out.end(), a, a + 3);
    out.insert(out.end(), b, b + 3);
    out.insert(out.end(), c, c + 3);
}

// orig: (host) shadow_packets 0x002868c0's selection of what a layer draws, on the scratchpad as shadow_quads 0x00285d30 left it:
// the quads between the point pairs i and i + 1 whose orientation byte (0x70003000 + i) is the layer, the near ring's cap when the
// layer's sign times the first near orientation (0x160468) is not negative, the far ring's cap when the one of the far ring (0x16046c)
// is not positive: a cap is a fan of the records whose clip byte (0x70003800 + 2 i, + 1 for the far ring) is zero
void obs_layer(uint32_t layer, uint32_t word) {
    if (!S.on || S.n <= 1) return;
    const uint32_t a = word >> 24;
    if (a == 0x80) return;  // (alpha x 0x80 / 128: nothing changes)
    std::vector<float>& out = a > 0x80 ? S.mark : S.unmark;
    uint64_t& count = a > 0x80 ? S.mark_tris : S.unmark_tris;
    const size_t before = out.size();
    for (uint32_t i = 0; i + 1 < S.n; ++i) {
        if (static_cast<int8_t>(lbu(SPR + 0x3000 + i)) != static_cast<int32_t>(layer)) continue;
        const float* n0 = &S.near_pts[i * 3];
        const float* f0 = &S.far_pts[i * 3];
        const float* n1 = &S.near_pts[(i + 1) * 3];
        const float* f1 = &S.far_pts[(i + 1) * 3];
        tri(out, n0, f0, n1);  // (the strip n0 f0 n1 f1)
        tri(out, f0, n1, f1);
    }
    const float sign = layer == 2 ? -1.0f : 1.0f;
    const uint32_t near_bits = std::bit_cast<uint32_t>(sign * lwc1(OV(0x00160468)));
    const uint32_t far_bits = std::bit_cast<uint32_t>(sign * lwc1(OV(0x00160468) + 4));
    for (int ring = 0; ring < 2; ++ring) {
        const bool draw = ring == 0 ? (near_bits >> 31) == 0 : (static_cast<int32_t>(far_bits) <= 0);
        if (!draw) continue;
        const std::vector<float>& pts = ring == 0 ? S.near_pts : S.far_pts;
        std::vector<const float*> v;
        for (uint32_t i = 0; i < S.n; ++i)
            if (static_cast<int8_t>(lbu(SPR + 0x3800 + 2 * i + static_cast<uint32_t>(ring))) == 0) v.push_back(&pts[i * 3]);
        for (size_t k = 2; k < v.size(); ++k) tri(out, v[0], v[k - 1], v[k]);
    }
    count += (out.size() - before) / 9;
    if (S.check)
        for (size_t o = before; o + 9 <= out.size(); o += 9) {
            State::Face f;
            std::copy(out.begin() + static_cast<std::ptrdiff_t>(o), out.begin() + static_cast<std::ptrdiff_t>(o) + 9, f.v);
            f.a = static_cast<uint8_t>(a);
            S.ordered.push_back(f);
        }
}

void obs_apply(uint64_t alpha_reg, uint32_t rgba) {
    if (!S.on) return;
    S.apply_alpha = alpha_reg;
    S.apply_rgba = rgba;
    S.have_apply = true;
}

void obs_chain(uint32_t first, uint32_t last) {
    if (!S.on) return;
    S.chain_first = first;
    S.chain_last = last;
    S.have_chain = true;
}

// ---- GL ----------------------------------------------------------------------------------------

GLuint g_face_prog = 0, g_dark_prog = 0, g_vao = 0, g_vbo = 0, g_dark_vao = 0;
GLint g_loc_vp = -1, g_loc_color = -1;
bool g_gl_failed = false;

GLuint compile(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "[mrp-shadows] shader: %s\n", log);
        g_gl_failed = true;
    }
    return s;
}

bool init_gl() {
    if (g_face_prog != 0) return !g_gl_failed;
    const char* face_vs = R"(#version 330 core
layout (location = 0) in vec3 aPos;
uniform mat4 uVP;
void main() { gl_Position = uVP * vec4(aPos, 1.0); }
)";
    const char* face_fs = R"(#version 330 core
void main() {}
)";
    const char* dark_vs = R"(#version 330 core
void main() {
    vec2 p = vec2(float((gl_VertexID << 1) & 2), float(gl_VertexID & 2));  // one triangle over the viewport
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
    const char* dark_fs = R"(#version 330 core
uniform vec4 uColor;
out vec4 FragColor;
void main() { FragColor = uColor; }
)";
    g_face_prog = glCreateProgram();
    glAttachShader(g_face_prog, compile(GL_VERTEX_SHADER, face_vs));
    glAttachShader(g_face_prog, compile(GL_FRAGMENT_SHADER, face_fs));
    glLinkProgram(g_face_prog);
    g_dark_prog = glCreateProgram();
    glAttachShader(g_dark_prog, compile(GL_VERTEX_SHADER, dark_vs));
    glAttachShader(g_dark_prog, compile(GL_FRAGMENT_SHADER, dark_fs));
    glLinkProgram(g_dark_prog);
    g_loc_vp = glGetUniformLocation(g_face_prog, "uVP");
    g_loc_color = glGetUniformLocation(g_dark_prog, "uColor");
    glGenVertexArrays(1, &g_vao);
    glGenVertexArrays(1, &g_dark_vao);
    glGenBuffers(1, &g_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &g_vbo, "MRP drop shadows");
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, 3 * sizeof(float), nullptr);
    glEnableVertexAttribArray(0);
    glBindVertexArray(0);
    return !g_gl_failed;
}

// ---- VK ----------------------------------------------------------------------------------------

VkPipelineLayout g_vk_face_layout = VK_NULL_HANDLE, g_vk_dark_layout = VK_NULL_HANDLE;
VkShaderModule g_vk_face_vs = VK_NULL_HANDLE, g_vk_face_fs = VK_NULL_HANDLE, g_vk_dark_vs = VK_NULL_HANDLE, g_vk_dark_fs = VK_NULL_HANDLE;
VkPipeline g_vk_mark_pipe = VK_NULL_HANDLE, g_vk_unmark_pipe = VK_NULL_HANDLE;
std::map<uint64_t, VkPipeline> g_vk_dark_pipes;
bool g_vk_inited = false;

void shutdown_vk() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        for (auto& [_, p] : g_vk_dark_pipes)
            if (p != VK_NULL_HANDLE) vkDestroyPipeline(dev, p, nullptr);
        g_vk_dark_pipes.clear();
        if (g_vk_mark_pipe != VK_NULL_HANDLE) vkDestroyPipeline(dev, g_vk_mark_pipe, nullptr);
        if (g_vk_unmark_pipe != VK_NULL_HANDLE) vkDestroyPipeline(dev, g_vk_unmark_pipe, nullptr);
        if (g_vk_face_vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_face_vs, nullptr);
        if (g_vk_face_fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_face_fs, nullptr);
        if (g_vk_dark_vs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_dark_vs, nullptr);
        if (g_vk_dark_fs != VK_NULL_HANDLE) vkDestroyShaderModule(dev, g_vk_dark_fs, nullptr);
        if (g_vk_face_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g_vk_face_layout, nullptr);
        if (g_vk_dark_layout != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, g_vk_dark_layout, nullptr);
    }
    g_vk_mark_pipe = g_vk_unmark_pipe = VK_NULL_HANDLE;
    g_vk_face_vs = g_vk_face_fs = g_vk_dark_vs = g_vk_dark_fs = VK_NULL_HANDLE;
    g_vk_face_layout = g_vk_dark_layout = VK_NULL_HANDLE;
    g_vk_inited = false;
}

bool init_vk() {
    if (g_vk_inited) return g_vk_mark_pipe != VK_NULL_HANDLE;
    VkDevice dev = vk::device();
    if (dev == VK_NULL_HANDLE) return false;
    g_vk_inited = true;
    vk::register_shutdown_callback(shutdown_vk);
    {
        VkPushConstantRange pcr{VK_SHADER_STAGE_VERTEX_BIT, 0, 64};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_face_layout);
    }
    {
        VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0, 16};
        VkPipelineLayoutCreateInfo plci{VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO};
        plci.pushConstantRangeCount = 1;
        plci.pPushConstantRanges = &pcr;
        vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk_dark_layout);
    }
    g_vk_face_vs = vk::create_shader_module(vk::shaders::kMrpShadowsFaceVs, sizeof(vk::shaders::kMrpShadowsFaceVs));
    g_vk_face_fs = vk::create_shader_module(vk::shaders::kMrpShadowsFaceFs, sizeof(vk::shaders::kMrpShadowsFaceFs));
    g_vk_dark_vs = vk::create_shader_module(vk::shaders::kMrpShadowsDarkVs, sizeof(vk::shaders::kMrpShadowsDarkVs));
    g_vk_dark_fs = vk::create_shader_module(vk::shaders::kMrpShadowsDarkFs, sizeof(vk::shaders::kMrpShadowsDarkFs));

    vk::GraphicsPipelineDesc desc{};
    desc.layout = g_vk_face_layout;
    desc.vs = g_vk_face_vs;
    desc.fs = g_vk_face_fs;
    desc.vertex_bindings = {{0, 3 * sizeof(float), VK_VERTEX_INPUT_RATE_VERTEX}};
    desc.vertex_attribs = {{0, 0, VK_FORMAT_R32G32B32_SFLOAT, 0}};
    desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
    desc.cull_mode = VK_CULL_MODE_NONE;
    desc.depth_test = true;
    desc.depth_write = false;
    desc.depth_compare = VK_COMPARE_OP_LESS_OR_EQUAL;
    desc.stencil_test = true;
    desc.color_write_mask = 0;

    VkStencilOpState st_mark{};
    st_mark.failOp = VK_STENCIL_OP_KEEP;
    st_mark.passOp = VK_STENCIL_OP_INCREMENT_AND_WRAP;
    st_mark.depthFailOp = VK_STENCIL_OP_KEEP;
    st_mark.compareOp = VK_COMPARE_OP_ALWAYS;
    st_mark.compareMask = 0xff;
    st_mark.writeMask = 0xff;
    st_mark.reference = static_cast<uint32_t>(kStencilStart);
    desc.stencil_front = desc.stencil_back = st_mark;
    g_vk_mark_pipe = vk::create_graphics_pipeline(desc);

    VkStencilOpState st_unmark = st_mark;
    st_unmark.passOp = VK_STENCIL_OP_DECREMENT_AND_WRAP;
    desc.stencil_front = desc.stencil_back = st_unmark;
    g_vk_unmark_pipe = vk::create_graphics_pipeline(desc);
    return g_vk_mark_pipe != VK_NULL_HANDLE && g_vk_unmark_pipe != VK_NULL_HANDLE;
}

// orig: (host) RAC1_MRP_SHADOW_CHECK: the stencil the faces left against the original's arithmetic run on the CPU: per pixel, the
// faces in the original's order, each one that covers the pixel's centre and is not farther than the depth buffer multiplying the
// frame's alpha by its alpha byte over 128 (the GS's texture function on the alpha it reads back, the result >> 7 clamped to 255),
// from the clear's alpha; the pixel is shadowed when the alpha has bit 7. Pixels where a face's depth is within 2e-6 of the buffer's
// or its edge function is within 1e-3 of zero (a rounding tie of the rasteriser) are counted apart and not compared.
void check_pass(const float* vp) {
    GLint view[4];
    glGetIntegerv(GL_VIEWPORT, view);
    const int W = view[2], H = view[3];
    if (W <= 0 || H <= 0) return;
    std::vector<float> depth(static_cast<size_t>(W) * H);
    std::vector<uint8_t> stencil(static_cast<size_t>(W) * H);
    glFinish();
    glReadPixels(view[0], view[1], W, H, GL_DEPTH_COMPONENT, GL_FLOAT, depth.data());
    glReadPixels(view[0], view[1], W, H, GL_STENCIL_INDEX, GL_UNSIGNED_BYTE, stencil.data());
    std::vector<int> alpha(static_cast<size_t>(W) * H, static_cast<int>(S.clear_rgba >> 24));
    std::vector<uint8_t> border(static_cast<size_t>(W) * H, 0);
    auto mul = [&](int i, int j) {
        return vp[i * 4 + j];
    };
    (void)mul;
    for (const State::Face& f : S.ordered) {
        ++S.chk_tris;
        double cx[3], cy[3], cz[3], cw[3];
        bool behind = false;
        for (int k = 0; k < 3; ++k) {
            const double x = f.v[k * 3], y = f.v[k * 3 + 1], z = f.v[k * 3 + 2];
            double c[4];
            for (int r = 0; r < 4; ++r) c[r] = static_cast<double>(vp[0 * 4 + r]) * x + static_cast<double>(vp[1 * 4 + r]) * y + static_cast<double>(vp[2 * 4 + r]) * z + static_cast<double>(vp[3 * 4 + r]);
            if (c[3] <= 1e-6) behind = true;
            cw[k] = c[3];
            cx[k] = c[0];
            cy[k] = c[1];
            cz[k] = c[2];
        }
        if (behind) {  // (the GPU clips at the near plane: not reproduced here)
            ++S.chk_skipped_tris;
            continue;
        }
        double sx[3], sy[3], sz[3];
        for (int k = 0; k < 3; ++k) {
            sx[k] = (cx[k] / cw[k] * 0.5 + 0.5) * W;
            sy[k] = (cy[k] / cw[k] * 0.5 + 0.5) * H;
            sz[k] = (cz[k] / cw[k]) * 0.5 + 0.5;
        }
        const double area = (sx[1] - sx[0]) * (sy[2] - sy[0]) - (sx[2] - sx[0]) * (sy[1] - sy[0]);
        if (std::fabs(area) < 1e-12) continue;
        const int x0 = std::max(0, static_cast<int>(std::floor(std::min({sx[0], sx[1], sx[2]})))), x1 = std::min(W - 1, static_cast<int>(std::ceil(std::max({sx[0], sx[1], sx[2]}))));
        const int y0 = std::max(0, static_cast<int>(std::floor(std::min({sy[0], sy[1], sy[2]})))), y1 = std::min(H - 1, static_cast<int>(std::ceil(std::max({sy[0], sy[1], sy[2]}))));
        for (int py = y0; py <= y1; ++py)
            for (int px = x0; px <= x1; ++px) {
                const double qx = px + 0.5, qy = py + 0.5;
                const double w0 = ((sx[1] - qx) * (sy[2] - qy) - (sx[2] - qx) * (sy[1] - qy)) / area;
                const double w1 = ((sx[2] - qx) * (sy[0] - qy) - (sx[0] - qx) * (sy[2] - qy)) / area;
                const double w2 = 1.0 - w0 - w1;
                const double eps = 1e-3;
                const bool in = w0 >= 0 && w1 >= 0 && w2 >= 0;
                const bool tie = (std::fabs(w0) < eps && w1 > -eps && w2 > -eps) || (std::fabs(w1) < eps && w0 > -eps && w2 > -eps) || (std::fabs(w2) < eps && w0 > -eps && w1 > -eps);
                const size_t idx = static_cast<size_t>(py) * W + px;
                if (!in) {
                    if (tie) border[idx] = 1;
                    continue;
                }
                if (tie) border[idx] = 1;
                const double z = w0 * sz[0] + w1 * sz[1] + w2 * sz[2];
                if (std::fabs(z - static_cast<double>(depth[idx])) < 2e-6) border[idx] = 1;
                if (z <= static_cast<double>(depth[idx])) alpha[idx] = std::min(255, (alpha[idx] * f.a) >> 7);
            }
    }
    ++S.chk_frames;
    for (size_t i = 0; i < alpha.size(); ++i) {
        const bool cpu = alpha[i] >= 0x80;
        const bool gpu = stencil[i] > kStencilStart;
        if (border[i] != 0) {
            ++S.chk_border;
            continue;
        }
        ++S.chk_pixels;
        if (cpu) ++S.chk_marked;
        if (cpu != gpu) {
            ++S.chk_mismatch;
            static int shown = 0;
            if (shown++ < 10) std::printf("[mrp-shadows-check] pixel %zu %zu: cpu alpha %d, stencil %d\n", i % static_cast<size_t>(W), i / static_cast<size_t>(W), alpha[i], stencil[i]);
        }
    }
}

}  // namespace

// orig: (host) see mrp_shadows.h
void mrp_shadows_install() {
    if (const char* e = std::getenv("RAC1_MRP_SHADOWS")) g_mrp_shadows = std::atoi(e) != 0 ? 1 : 0;
    S.check = std::getenv("RAC1_MRP_SHADOW_CHECK") != nullptr;
    g_drop_shadow_observer.begin = obs_begin;
    g_drop_shadow_observer.tube_points = obs_tube_points;
    g_drop_shadow_observer.layer = obs_layer;
    g_drop_shadow_observer.apply = obs_apply;
    g_drop_shadow_observer.chain = obs_chain;
}

// orig: (host) see mrp_shadows.h
void mrp_shadows_begin_frame(bool on) {
    static const bool s_log = std::getenv("RAC1_MRP_SHADOW_LOG") != nullptr;
    static uint64_t s_frame = 0;
    if (s_log && S.captured) std::printf("[mrp-shadows] frame %llu: %zu marking, %zu unmarking triangles, %llu tubes so far\n", static_cast<unsigned long long>(s_frame), S.mark.size() / 9, S.unmark.size() / 9, static_cast<unsigned long long>(S.tubes));
    ++s_frame;
    S.on = on && g_mrp_shadows != 0;
    S.captured = false;
    S.have_apply = false;
    S.have_chain = false;
    S.n = 0;
    S.mark.clear();
    S.unmark.clear();
    S.ordered.clear();
}

// orig: (host) see mrp_shadows.h
bool mrp_shadows_check_enabled() { return S.check; }

// orig: (host) see mrp_shadows.h
bool mrp_shadows_chain(uint32_t& first, uint32_t& last) {
    if (S.check) return false;  // (the check wants the GS path's own shadow primitives)
    if (!S.on || !S.captured || !S.have_chain) return false;
    first = S.chain_first;
    last = S.chain_last;
    return true;
}

// orig: (host) see mrp_shadows.h
void mrp_shadows_snapshot() {
    D.on = S.on;
    D.captured = S.captured;
    D.have_apply = S.have_apply;
    D.clear_rgba = S.clear_rgba;
    D.apply_alpha = S.apply_alpha;
    D.apply_rgba = S.apply_rgba;
    D.mark = S.mark;
    D.unmark = S.unmark;
}

// orig: (host) see mrp_shadows.h
bool mrp_shadows_draw(const float* view_proj) {
    // (a pipelined frame draws what was captured before the next update)
    const State& X = g_draw_from_snapshot ? D : S;
    if (!X.on || !X.captured || !X.have_apply) return false;
    if (X.mark.empty() && X.unmark.empty()) return false;
    if (vk::is_active()) {
        if (!init_vk()) return false;
        if (vk::current_target().stencil_view == VK_NULL_HANDLE) {
            if (!S.warned) std::fprintf(stderr, "[mrp-shadows] the framebuffer has no 8-bit stencil: the drop shadows are not drawn\n");
            S.warned = true;
            return false;
        }
        {
            const uint32_t a0 = X.clear_rgba >> 24;
            if (a0 != 0x7f && !S.warned) {
                std::fprintf(stderr, "[mrp-shadows] unexpected clear alpha 0x%02x\n", a0);
                S.warned = true;
                ++S.odd_alpha;
            }
        }
        vk::ensure_rendering();
        VkCommandBuffer cmd = vk::cmd();
        vk::clear_attachments(false, nullptr, false, 1.0f, true, static_cast<uint32_t>(kStencilStart));
        VkRect2D sc{{0, 0}, {static_cast<uint32_t>(vk::current_target_width()), static_cast<uint32_t>(vk::current_target_height())}};
        vkCmdSetScissor(cmd, 0, 1, &sc);
        auto vk_faces = [&](const std::vector<float>& v, VkPipeline pipe) {
            if (v.empty()) return;
            vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, pipe);
            vkCmdPushConstants(cmd, g_vk_face_layout, VK_SHADER_STAGE_VERTEX_BIT, 0, 64, view_proj);
            auto alloc = vk::alloc_ring(v.size() * sizeof(float), 16);
            std::memcpy(alloc.ptr, v.data(), v.size() * sizeof(float));
            VkDeviceSize off = alloc.offset;
            vkCmdBindVertexBuffers(cmd, 0, 1, &alloc.buffer, &off);
            vkCmdDraw(cmd, static_cast<uint32_t>(v.size() / 3), 1, 0, 0);
        };
        vk_faces(X.mark, g_vk_mark_pipe);
        vk_faces(X.unmark, g_vk_unmark_pipe);

        gs::DrawState ds{};
        ds.prim = 1ull << 6;
        ds.alpha = X.apply_alpha;
        float fix = 0.0f;
        VkPipeline& dark_pipe = g_vk_dark_pipes[X.apply_alpha];
        if (dark_pipe == VK_NULL_HANDLE) {
            vk::GraphicsPipelineDesc desc{};
            desc.layout = g_vk_dark_layout;
            desc.vs = g_vk_dark_vs;
            desc.fs = g_vk_dark_fs;
            desc.topology = VK_PRIMITIVE_TOPOLOGY_TRIANGLE_LIST;
            desc.cull_mode = VK_CULL_MODE_NONE;
            desc.depth_test = false;
            desc.depth_write = false;
            desc.stencil_test = true;
            VkStencilOpState st_dark{};
            st_dark.failOp = VK_STENCIL_OP_KEEP;
            st_dark.passOp = VK_STENCIL_OP_KEEP;
            st_dark.depthFailOp = VK_STENCIL_OP_KEEP;
            st_dark.compareOp = VK_COMPARE_OP_LESS;
            st_dark.compareMask = 0xff;
            st_dark.writeMask = 0;
            st_dark.reference = static_cast<uint32_t>(kStencilStart);
            desc.stencil_front = desc.stencil_back = st_dark;
            gs::vk_blend_state(ds, false, desc, fix);
            desc.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
            dark_pipe = vk::create_graphics_pipeline(desc);
        }
        vkCmdBindPipeline(cmd, VK_PIPELINE_BIND_POINT_GRAPHICS, dark_pipe);
        gs::blend_state(ds, false);
        const float col[4] = {
            static_cast<float>(X.apply_rgba & 0xff) / 255.0f,
            static_cast<float>((X.apply_rgba >> 8) & 0xff) / 255.0f,
            static_cast<float>((X.apply_rgba >> 16) & 0xff) / 255.0f,
            1.0f,
        };
        vkCmdPushConstants(cmd, g_vk_dark_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(col), col);
        vkCmdDraw(cmd, 3, 1, 0, 0);
        ++S.drawn_frames;
        return true;
    }
    if (!init_gl()) return false;
    GLint fbo = 0;
    glGetIntegerv(GL_FRAMEBUFFER_BINDING, &fbo);
    GLint stencil_bits = 0;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, fbo == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &stencil_bits);
    if (stencil_bits < 8) {
        if (!S.warned) std::fprintf(stderr, "[mrp-shadows] the framebuffer has no 8-bit stencil: the drop shadows are not drawn\n");
        S.warned = true;
        return false;
    }
    // the marking alpha of the frame's clear and of the two layers: the net of faces is the sign of the integer product's result
    // (checked over every order up to 12 faces for 0x7f / 0x82 / 0x7f); other values would need another predicate
    {
        const uint32_t a0 = X.clear_rgba >> 24;
        if (a0 != 0x7f && !S.warned) {
            std::fprintf(stderr, "[mrp-shadows] unexpected clear alpha 0x%02x\n", a0);
            S.warned = true;
            ++S.odd_alpha;
        }
    }
    // the GL state the caller (the MRP's draw) leaves: saved, then restored
    GLboolean depth_test = glIsEnabled(GL_DEPTH_TEST), blend = glIsEnabled(GL_BLEND), cull = glIsEnabled(GL_CULL_FACE), scissor = glIsEnabled(GL_SCISSOR_TEST);
    GLint depth_func = GL_LEQUAL, prog = 0, vao = 0, abuf = 0, ebuf = 0;
    GLboolean depth_mask = GL_TRUE, color_mask[4] = {GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE};
    glGetIntegerv(GL_DEPTH_FUNC, &depth_func);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &vao);
    glGetIntegerv(GL_ARRAY_BUFFER_BINDING, &abuf);
    glGetIntegerv(GL_ELEMENT_ARRAY_BUFFER_BINDING, &ebuf);
    glGetBooleanv(GL_DEPTH_WRITEMASK, &depth_mask);
    glGetBooleanv(GL_COLOR_WRITEMASK, color_mask);

    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    // the counter: the stencil cleared to its start
    glStencilMask(0xff);
    glClearStencil(kStencilStart);
    glClear(GL_STENCIL_BUFFER_BIT);
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 0, 0xff);
    // the faces: against the world's depth (the GS's GEQUAL: not farther than the surface), no colour, no depth write
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_FALSE);
    glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
    glUseProgram(g_face_prog);
    glUniformMatrix4fv(g_loc_vp, 1, GL_FALSE, view_proj);
    glBindVertexArray(g_vao);
    glBindBuffer(GL_ARRAY_BUFFER, g_vbo);
    auto faces = [&](const std::vector<float>& v, GLenum zpass) {
        if (v.empty()) return;
        glStencilOp(GL_KEEP, GL_KEEP, zpass);
        glBufferData(GL_ARRAY_BUFFER, static_cast<GLsizeiptr>(v.size() * sizeof(float)), v.data(), GL_STREAM_DRAW);
        glDrawArrays(GL_TRIANGLES, 0, static_cast<GLsizei>(v.size() / 3));
    };
    faces(X.mark, GL_INCR_WRAP);
    faces(X.unmark, GL_DECR_WRAP);
    if (S.check) check_pass(view_proj);
    // the darkening: where the counter is above its start (the frame's alpha has bit 7), the blend the original sets
    glDisable(GL_DEPTH_TEST);
    glStencilFunc(GL_LESS, kStencilStart, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_KEEP);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    gs::DrawState ds{};
    ds.prim = 1ull << 6;  // ABE
    ds.alpha = X.apply_alpha;
    gs::blend_state(ds);
    glUseProgram(g_dark_prog);
    glUniform4f(g_loc_color, static_cast<float>(X.apply_rgba & 0xff) / 255.0f, static_cast<float>((X.apply_rgba >> 8) & 0xff) / 255.0f,
                static_cast<float>((X.apply_rgba >> 16) & 0xff) / 255.0f, 1.0f);
    glBindVertexArray(g_dark_vao);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // restore
    glDisable(GL_STENCIL_TEST);
    glColorMask(color_mask[0], color_mask[1], color_mask[2], color_mask[3]);
    glDepthMask(depth_mask);
    glDepthFunc(static_cast<GLenum>(depth_func));
    if (depth_test) glEnable(GL_DEPTH_TEST); else glDisable(GL_DEPTH_TEST);
    if (blend) glEnable(GL_BLEND); else glDisable(GL_BLEND);
    if (cull) glEnable(GL_CULL_FACE); else glDisable(GL_CULL_FACE);
    if (scissor) glEnable(GL_SCISSOR_TEST); else glDisable(GL_SCISSOR_TEST);
    glUseProgram(static_cast<GLuint>(prog));
    glBindVertexArray(static_cast<GLuint>(vao));
    glBindBuffer(GL_ARRAY_BUFFER, static_cast<GLuint>(abuf));
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, static_cast<GLuint>(ebuf));
    ++S.drawn_frames;
    return true;
}

// orig: (host) see mrp_shadows.h
void mrp_shadows_check_gs(const void* gs_primitives, int win_w, int win_h, float fb_w, float fb_h, int vp_x, int vp_y, int vp_w, int vp_h, double depth_a, double depth_b,
                          const float* view_proj) {
    (void)win_w;
    (void)win_h;
    if (!S.on || !S.captured) return;
    const auto& prims = *static_cast<const std::vector<gs::Primitive>*>(gs_primitives);
    std::vector<const gs::Primitive*> seq;  // the alpha counter's faces in the order the GS path got them (fans and strips already assembled as triangles)
    for (const gs::Primitive& p : prims)
        if (p.st.test == 0x51001 && p.type == 3 && !p.moby) seq.push_back(&p);
    ++g_chk_gs.frames;
    if (seq.size() != S.ordered.size()) {
        ++g_chk_gs.count_differs;
        if (g_chk_gs.count_differs <= 5) std::printf("[mrp-shadows-check] frame: the GS path has %zu faces, the capture %zu\n", seq.size(), S.ordered.size());
        return;
    }
    for (size_t i = 0; i < seq.size(); ++i) {
        const gs::Primitive& p = *seq[i];
        const State::Face& f = S.ordered[i];
        ++g_chk_gs.faces;
        if (p.v[0].a != f.a || p.v[1].a != f.a || p.v[2].a != f.a) {
            ++g_chk_gs.alpha_differs;
            continue;
        }
        // the face's vertices through the world's projection into the window and the depth buffer's range
        double wx[3], wy[3], wz[3];
        bool ok = true;
        for (int k = 0; k < 3; ++k) {
            double c[4];
            for (int r = 0; r < 4; ++r)
                c[r] = static_cast<double>(view_proj[0 * 4 + r]) * f.v[k * 3] + static_cast<double>(view_proj[1 * 4 + r]) * f.v[k * 3 + 1] +
                       static_cast<double>(view_proj[2 * 4 + r]) * f.v[k * 3 + 2] + static_cast<double>(view_proj[3 * 4 + r]);
            if (c[3] <= 1e-6) {
                ok = false;
                break;
            }
            wx[k] = vp_x + (c[0] / c[3] * 0.5 + 0.5) * vp_w;
            wy[k] = vp_y + (c[1] / c[3] * 0.5 + 0.5) * vp_h;
            wz[k] = c[2] / c[3] * 0.5 + 0.5;
        }
        if (!ok) continue;
        // each captured vertex against the nearest GS vertex (the strips / fans may start at another corner)
        bool matched = true;
        for (int k = 0; k < 3; ++k) {
            double best = 1e30, bz = 0;
            for (int j = 0; j < 3; ++j) {
                const double gx = vp_x + (p.v[j].x / fb_w) * vp_w, gy = vp_y + (1.0 - p.v[j].y / fb_h) * vp_h;
                const double d = std::hypot(gx - wx[k], gy - wy[k]);
                if (d < best) {
                    best = d;
                    bz = depth_a + depth_b * static_cast<double>(p.v[j].z);
                }
            }
            g_chk_gs.max_px = std::max(g_chk_gs.max_px, best);
            g_chk_gs.max_dz = std::max(g_chk_gs.max_dz, std::fabs(bz - wz[k]));
            if (best > 2.0) matched = false;
        }
        if (!matched) ++g_chk_gs.pos_differs;
    }
}

// orig: (host) see mrp_shadows.h
void mrp_shadows_report() {
    if (S.check)
        std::printf("[mrp-shadows-check] vs the GS path: %llu frames (%llu with another face count), %llu faces compared, %llu with another alpha byte, %llu with a vertex more than 2 px "
                    "from the GS's, max vertex distance %.3f px, max depth difference %.6f\n",
                    static_cast<unsigned long long>(g_chk_gs.frames), static_cast<unsigned long long>(g_chk_gs.count_differs), static_cast<unsigned long long>(g_chk_gs.faces),
                    static_cast<unsigned long long>(g_chk_gs.alpha_differs), static_cast<unsigned long long>(g_chk_gs.pos_differs), g_chk_gs.max_px, g_chk_gs.max_dz);
    if (S.frames == 0) return;
    if (S.check)
        std::printf("[mrp-shadows-check] %llu frames, %llu faces (%llu skipped at the near plane), %llu pixels compared (%llu shadowed by the CPU), %llu differ, %llu border pixels not compared\n",
                    static_cast<unsigned long long>(S.chk_frames), static_cast<unsigned long long>(S.chk_tris), static_cast<unsigned long long>(S.chk_skipped_tris),
                    static_cast<unsigned long long>(S.chk_pixels), static_cast<unsigned long long>(S.chk_marked), static_cast<unsigned long long>(S.chk_mismatch),
                    static_cast<unsigned long long>(S.chk_border));
    std::printf("[mrp-shadows] frames with shadows %llu (drawn %llu), tubes %llu, marking triangles %llu, unmarking %llu\n", static_cast<unsigned long long>(S.frames),
                static_cast<unsigned long long>(S.drawn_frames), static_cast<unsigned long long>(S.tubes), static_cast<unsigned long long>(S.mark_tris),
                static_cast<unsigned long long>(S.unmark_tris));
}

}  // namespace rac1
