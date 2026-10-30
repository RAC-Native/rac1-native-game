// orig: (host) The MRP's drop shadows (see mrp_shadows.h).
#include <game/render/mrp_shadows.h>
#include <profiler/host_profiler.h>

#include <game/ovl/moby_shadows.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/render/mrp_scene.h>
#include <game/render/mrp_shadows_gpu.inc>
#include <game/render/render_toggles.h>
#include <runtime/gs/gs_blend.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>

#include <algorithm>
#include <bit>
#include <chrono>
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

void ds_time_end();
void obs_chain(uint32_t first, uint32_t last) {
    ds_time_end();
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

// ---- RAC1_WIIU_SHADOWS_GPU: the shapes taken by the host (mrp_shadows_gpu.inc) -------------------------------------------------------

DropShadowFrame g_ds;       // (this frame's capture)
DropShadowFrame g_ds_draw;  // (the copy the renderer draws: mrp_shadows_snapshot)
bool g_ds_check = false;    // (testing, RAC1_WIIU_SHADOWS_GPU_CHECK=1: the game's shadow_points run too, its points against the capture's)
struct DsCheck {
    uint64_t frames = 0, mobys = 0, points = 0, differ = 0, scene = 0, game = 0;
    double max_d = 0.0, max_r = 0.0;
} g_dsc;
bool g_ds_log = false;  // (testing, RAC1_WIIU_SHADOWS_GPU_LOG=1: each frame's casters on the log)

// orig: (helper) the inverse of the 3 x 3 rows r (p' = p.x r0 + p.y r1 + p.z r2), row-major into m; false: singular
bool inv3(const MrpVec4* r, float m[9]) {
    const float a = r[0].x, b = r[0].y, c = r[0].z, d = r[1].x, e = r[1].y, f = r[1].z, g = r[2].x, h = r[2].y, i = r[2].z;
    const float A = e * i - f * h, B = -(d * i - f * g), C = d * h - e * g;
    const float det = a * A + b * B + c * C;
    if (!(std::fabs(det) > 1e-20f)) return false;
    const float k = 1.0f / det;
    m[0] = A * k;
    m[1] = -(b * i - c * h) * k;
    m[2] = (b * f - c * e) * k;
    m[3] = B * k;
    m[4] = (a * i - c * g) * k;
    m[5] = -(a * f - c * d) * k;
    m[6] = C * k;
    m[7] = -(a * h - b * g) * k;
    m[8] = (a * e - b * d) * k;
    return true;
}
MrpVec4 row_at(uint32_t a) { return MrpVec4{lwc1(a), lwc1(a + 4), lwc1(a + 8), lwc1(a + 12)}; }
// orig: (helper) p through four rows (x r0 + y r1 + z r2 + r3), xyz
void xform(const MrpVec4* r, const float p[3], float o[3]) {
    o[0] = p[0] * r[0].x + p[1] * r[1].x + p[2] * r[2].x + r[3].x;
    o[1] = p[0] * r[0].y + p[1] * r[1].y + p[2] * r[2].y + r[3].y;
    o[2] = p[0] * r[0].z + p[1] * r[1].z + p[2] * r[2].z + r[3].z;
}

// The joint matrices a deferred moby's shadow points go through (shadow_points 0x285a68: moby_pose_matrices 0x2531e0 onto the
// scratchpad, the joints' own matrices J). The MRP's scene already holds the pose the skinning consumed this frame (mrp_scene.h: per
// joint S = B J, B the record's bind matrix +0x1c); J p = (p B^-1) S. A moby the scene has no pose of (not drawn: a shadow seen
// under a moby out of the view; a pose of another joint count) gets the game's routine, as the original.
struct PoseSource {
    const MrpVec4* skin = nullptr;  // the scene's S rows (4 per joint), or nullptr: the scratchpad's J
    uint32_t bind = 0;              // the record's bind matrices (0x40 per joint)
    uint32_t joints = 0;
};
bool pose_point(const PoseSource& ps, uint32_t j, const float p[3], float o[3]) {
    if (ps.skin == nullptr) {
        MrpVec4 J[4];
        for (int r = 0; r < 4; ++r) J[r] = row_at(0x70000000u + 0x40u * j + 0x10u * static_cast<uint32_t>(r));
        xform(J, p, o);
        return true;
    }
    if (j >= ps.joints) return false;
    MrpVec4 B[4];
    for (int r = 0; r < 4; ++r) B[r] = row_at(ps.bind + 0x40u * j + 0x10u * static_cast<uint32_t>(r));
    float inv[9];
    if (!inv3(B, inv)) return false;
    const float q[3] = {p[0] - B[3].x, p[1] - B[3].y, p[2] - B[3].z};
    const float u[3] = {q[0] * inv[0] + q[1] * inv[3] + q[2] * inv[6], q[0] * inv[1] + q[1] * inv[4] + q[2] * inv[7], q[0] * inv[2] + q[1] * inv[5] + q[2] * inv[8]};
    xform(ps.skin + 4u * j, u, o);
    return true;
}

// orig: (host) moby_shadows' hook (moby_shadows.cpp): the shapes of the deferred mobys 0x1ac600 (count 0x15fffc) as shadow_points
// would write them into 0x1ac780, kept on the host; true: the game's shadow_points / drop_shadows_draw not run
// (testing, RAC1_WIIU_SHADOWS_GPU_TIME=1, any host: the drop shadows' time on the game's thread - from this hook, at moby_shadows' start,
// to its chain observer, at its end: the game's shadow_points / drop_shadows_draw, or the capture -, per frame every 120 frames that drew any)
bool g_ds_time = false;
std::chrono::steady_clock::time_point g_ds_t0;
double g_ds_ms = 0.0;
uint64_t g_ds_frames = 0;
bool ds_capture_impl();
bool ds_capture() {
    if (g_ds_time) g_ds_t0 = std::chrono::steady_clock::now();
    return ds_capture_impl();
}
void ds_time_end() {
    if (!g_ds_time || g_ds_t0.time_since_epoch().count() == 0) return;
    g_ds_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - g_ds_t0).count();
    g_ds_t0 = {};
    if (++g_ds_frames % 120 == 0) {
        std::printf("[shadows-gpu] the drop shadows on the game's thread (%s): %.4f ms a frame (120 frames)\n", g_drop_shadows_gpu != 0 ? "the host's capture" : "the game's code",
                    g_ds_ms / 120.0);
        g_ds_ms = 0.0;
    }
}
bool ds_capture_impl() {
    if (g_drop_shadows_gpu == 0) return false;
    DropShadowFrame& F = g_ds;
    F.casters.clear();
    F.prims.clear();
    F.scene_poses = F.game_poses = 0;
    ++F.serial;
    const int32_t n = lw(OV(0x0015fffc));
    const MrpFrame& sc = mrp_scene();
    uint32_t bytes = 0;  // (shadow_points' room: 0x1f80 bytes of entries at 0x1ac780, checked before each moby)
    for (int32_t i = 0; i < n && bytes <= 0x1f80u; ++i) {
        const uint32_t e = OV(0x001ac600u) + 8u * static_cast<uint32_t>(i);
        const uint32_t m = lwu(e);
        const uint32_t cls = lwu(m + 0x24);
        const uint32_t nj = lbu(cls + 8);
        const float s = lwc1(m + 0x2c) * (1.0f / 1024.0f) * (static_cast<float>(lhu(e + 4)) * (1.0f / 4096.0f));
        MrpVec4 M[4];
        for (int r = 0; r < 3; ++r) M[r] = row_at(m + 0xc0u + 0x10u * static_cast<uint32_t>(r));
        M[3] = row_at(m + 0x10);
        PoseSource ps;
        for (size_t k = sc.mobys.size(); k-- > 0;) {
            const MrpMoby& mm = sc.mobys[k];
            if (mm.moby != m) continue;
            if (mm.joint_count == nj && nj > 0 && mm.ring != 0 && static_cast<uint32_t>(lh(mm.ring + 0x10)) == nj && mm.joint_first + nj <= sc.joints.size()) {
                ps.skin = &sc.joints[mm.joint_first].r[0];
                ps.bind = lwu(mm.ring + 0x1c);
                ps.joints = nj;
            }
            break;
        }
        if (g_ds_check) ps.skin = nullptr;  // (the check: the game's own routine's matrices; the scene's compared below)
        PoseSource scene_ps;
        if (g_ds_check)
            for (size_t k = sc.mobys.size(); k-- > 0;) {
                const MrpMoby& mm = sc.mobys[k];
                if (mm.moby != m) continue;
                if (mm.joint_count == nj && nj > 0 && mm.ring != 0 && static_cast<uint32_t>(lh(mm.ring + 0x10)) == nj && mm.joint_first + nj <= sc.joints.size()) {
                    scene_ps.skin = &sc.joints[mm.joint_first].r[0];
                    scene_ps.bind = lwu(mm.ring + 0x1c);
                    scene_ps.joints = nj;
                }
                break;
            }
        if (g_ds_check && scene_ps.skin != nullptr) ++g_dsc.scene;
        if (ps.skin == nullptr) {
            moby_pose_matrices(m, static_cast<int32_t>(nj), -1, 0, 0);
            ++F.game_poses;
        } else {
            ++F.scene_poses;
        }
        DropShadowCaster c;
        c.moby = m;
        {
            const uint32_t da = OV(0x001aeb80u) + (static_cast<uint32_t>(lbu(m + 0xbd)) << 4);
            float d[3] = {lwc1(da), lwc1(da + 4), lwc1(da + 8)};
            const float l = std::sqrt(d[0] * d[0] + d[1] * d[1] + d[2] * d[2]);
            if (l > 0.0f)
                for (float& x : d) x /= l;
            std::memcpy(c.dir, d, sizeof d);
        }
        c.z_near = lwc1(m + 0x88);
        c.z_far = lwc1(m + 0x84);
        c.first = static_cast<uint32_t>(F.prims.size());
        bytes += 0x20;
        // the class's shadow list: 0x20-byte one-joint entries (kind byte 0) and 0x30-byte two-joint ones; byte +1 ends the list
        uint32_t t = lwu(cls + 0x14) - (static_cast<uint32_t>(lbu(cls + 0xf)) << 4);
        bool last = false;
        auto world = [&](const PoseSource& src, uint32_t j, uint32_t pa, float o[3], float& r) {
            const float p[3] = {lwc1(pa), lwc1(pa + 4), lwc1(pa + 8)};
            float q[3];
            if (!pose_point(src, j, p, q)) {  // (a joint past the pose's, a singular bind matrix: the shape left out)
                o[0] = o[1] = o[2] = 0.0f;
                r = 0.0f;
                return;
            }
            const float w[3] = {q[0] * s, q[1] * s, q[2] * s};
            xform(M, w, o);
            r = lwc1(pa + 12) * s;
        };
        for (int guard = 0; !last && guard < 64; ++guard) {
            const uint32_t kind = lbu(t);
            last = lbu(t + 1) != 0;
            DropShadowPrim p;
            if (kind == 0) {
                world(ps, lwu(t + 4), t + 0x10, p.c0, p.r0);
                t += 0x20;
                bytes += 0x20;
            } else {
                world(ps, lhu(t + 4), t + 0x10, p.c0, p.r0);
                world(ps, lhu(t + 6), t + 0x20, p.c1, p.r1);
                p.capsule = true;
                // a negative end word: that end moved along the segment by -w x 16 / 4096 of it (its ring then left out: the end flat)
                const int32_t w0 = lw(t + 8), w1 = lw(t + 0xc);
                if (w0 < 0) {
                    const float f = static_cast<float>(-w0 * 16) * (1.0f / 4096.0f);
                    for (int k = 0; k < 3; ++k) p.c0[k] += (p.c0[k] - p.c1[k]) * f;
                }
                if (w1 < 0) {
                    const float f = static_cast<float>(-w1 * 16) * (1.0f / 4096.0f);
                    for (int k = 0; k < 3; ++k) p.c1[k] += (p.c1[k] - p.c0[k]) * f;
                }
                t += 0x30;
                bytes += 0x30;
            }
            if (g_ds_check && scene_ps.skin != nullptr) {  // (the scene's pose against the game's: the same points?)
                const uint32_t back = t - (p.capsule ? 0x30u : 0x20u);
                DropShadowPrim q;
                if (!p.capsule) {
                    world(scene_ps, lwu(back + 4), back + 0x10, q.c0, q.r0);
                } else {
                    world(scene_ps, lhu(back + 4), back + 0x10, q.c0, q.r0);
                    world(scene_ps, lhu(back + 6), back + 0x20, q.c1, q.r1);
                    const int32_t w0 = lw(back + 8), w1 = lw(back + 0xc);
                    if (w0 < 0)
                        for (int k = 0; k < 3; ++k) q.c0[k] += (q.c0[k] - q.c1[k]) * (static_cast<float>(-w0 * 16) * (1.0f / 4096.0f));
                    if (w1 < 0)
                        for (int k = 0; k < 3; ++k) q.c1[k] += (q.c1[k] - q.c0[k]) * (static_cast<float>(-w1 * 16) * (1.0f / 4096.0f));
                }
                double dmax = 0.0;
                for (int k = 0; k < 3; ++k) {
                    dmax = std::max(dmax, static_cast<double>(std::fabs(q.c0[k] - p.c0[k])));
                    if (p.capsule) dmax = std::max(dmax, static_cast<double>(std::fabs(q.c1[k] - p.c1[k])));
                }
                g_dsc.max_d = std::max(g_dsc.max_d, dmax);
                g_dsc.max_r = std::max(g_dsc.max_r, static_cast<double>(std::fabs(q.r0 - p.r0)));
                ++g_dsc.points;
                if (dmax > 1e-3) {
                    ++g_dsc.differ;
                    static int shown = 0;
                    if (shown++ < 12)
                        std::printf("[shadows-gpu-check] moby %08x class %d entry %u: scene's point %.4f %.4f %.4f, the game's %.4f %.4f %.4f\n", m, lh(m + 0xa6),
                                    (back - (lwu(cls + 0x14) - (static_cast<uint32_t>(lbu(cls + 0xf)) << 4))), q.c0[0], q.c0[1], q.c0[2], p.c0[0], p.c0[1], p.c0[2]);
                }
            }
            if (p.r0 > 0.0f || (p.capsule && p.r1 > 0.0f)) F.prims.push_back(p);
        }
        c.count = static_cast<uint32_t>(F.prims.size()) - c.first;
        if (c.count != 0) F.casters.push_back(c);
    }
    if (g_ds_check) {
        ++g_dsc.frames;
        g_dsc.mobys += static_cast<uint64_t>(std::max(n, 0));
        g_dsc.game += F.game_poses;
        if (g_dsc.frames % 300 == 0)
            std::printf("[shadows-gpu-check] %llu frames, %llu deferred mobys (the scene's pose for %llu), %llu shapes compared, %llu differ by more than 0.001, "
                        "the largest difference %.6f (radius %.6f)\n",
                        static_cast<unsigned long long>(g_dsc.frames), static_cast<unsigned long long>(g_dsc.mobys), static_cast<unsigned long long>(g_dsc.scene),
                        static_cast<unsigned long long>(g_dsc.points), static_cast<unsigned long long>(g_dsc.differ), g_dsc.max_d, g_dsc.max_r);
        return false;  // (the game's code runs as well: the check's own run, its points written to 0x1ac780 as ever)
    }
    if (g_ds_log) {
        std::printf("[shadows-gpu] serial %llu: %zu casters, %zu shapes (poses: %u the scene's, %u the game's)", static_cast<unsigned long long>(F.serial), F.casters.size(),
                    F.prims.size(), F.scene_poses, F.game_poses);
        for (const DropShadowCaster& c : F.casters) {
            const DropShadowPrim& p = F.prims[c.first];
            std::printf(" | moby %08x z %.2f..%.2f (moby %.2f) dir %.2f %.2f %.2f, %u: %s r %.2f at %.2f %.2f %.2f", c.moby, c.z_far, c.z_near, lwc1(c.moby + 0x18), c.dir[0], c.dir[1],
                        c.dir[2], c.count, p.capsule ? "capsule" : "sphere", p.r0, p.c0[0], p.c0[1], p.c0[2]);
        }
        std::printf("\n");
    }
    return true;
}

}  // namespace

int g_drop_shadows_gpu = 0;

// orig: (host) see mrp_shadows_gpu.inc
const DropShadowFrame* mrp_drop_shadows_view() { return g_ds_draw.casters.empty() ? nullptr : &g_ds_draw; }

// orig: (host) see mrp_shadows_gpu.inc
bool mrp_drop_shadows_camera(const float* M, float cam[3]) {
    // rows x (0), y (1), w (3) of M: M[c * 4 + r]; solve r . (C, 1) = 0
    double a[3][4];
    const int rows[3] = {0, 1, 3};
    for (int i = 0; i < 3; ++i)
        for (int c = 0; c < 4; ++c) a[i][c] = M[c * 4 + rows[i]];
    for (int col = 0; col < 3; ++col) {  // (Gaussian elimination, partial pivoting)
        int piv = col;
        for (int i = col + 1; i < 3; ++i)
            if (std::fabs(a[i][col]) > std::fabs(a[piv][col])) piv = i;
        if (std::fabs(a[piv][col]) < 1e-12) return false;
        if (piv != col)
            for (int c = 0; c < 4; ++c) std::swap(a[piv][c], a[col][c]);
        for (int i = 0; i < 3; ++i) {
            if (i == col) continue;
            const double f = a[i][col] / a[col][col];
            for (int c = col; c < 4; ++c) a[i][c] -= f * a[col][c];
        }
    }
    for (int i = 0; i < 3; ++i) cam[i] = static_cast<float>(-a[i][3] / a[i][i]);
    return std::isfinite(cam[0]) && std::isfinite(cam[1]) && std::isfinite(cam[2]);
}

// orig: (host) see mrp_shadows_gpu.inc. Per shape its outline across the direction (a circle of `ring` points, a capsule the hull of
// its two circles), each outline point's line along the direction cut by the slab's two planes (the original's tube: a ring of points
// and its far points, shadow_points_clip), the sides and the two caps as triangles; a face goes to `front` when its outward side
// looks at the camera
void mrp_drop_shadows_volumes(const DropShadowFrame& f, const float cam[3], int ring, DropShadowVolumes& out) {
    out.front.clear();
    out.back.clear();
    out.inside = 0;
    out.prisms = 0;
    out.boxes.clear();
    for (int k = 0; k < 3; ++k) {
        out.lo[k] = 1e30f;
        out.hi[k] = -1e30f;
    }
    ring = std::clamp(ring, 6, 32);
    std::vector<float> o2;
    std::vector<float> top, bot;
    auto put = [](std::vector<float>& v, const float* a, const float* b, const float* c) {
        v.insert(v.end(), a, a + 3);
        v.insert(v.end(), b, b + 3);
        v.insert(v.end(), c, c + 3);
    };
    for (const DropShadowCaster& c : f.casters) {
        const float* d = c.dir;
        if (!(std::fabs(d[2]) > 0.05f) || c.z_near == c.z_far) continue;  // (a light along the ground; an empty slab)
        // U, V across the direction
        float U[3], V[3];
        {
            const float ax[3] = {std::fabs(d[0]) < 0.9f ? 1.0f : 0.0f, std::fabs(d[0]) < 0.9f ? 0.0f : 1.0f, 0.0f};
            U[0] = d[1] * ax[2] - d[2] * ax[1];
            U[1] = d[2] * ax[0] - d[0] * ax[2];
            U[2] = d[0] * ax[1] - d[1] * ax[0];
            const float l = std::sqrt(U[0] * U[0] + U[1] * U[1] + U[2] * U[2]);
            for (float& x : U) x /= l;
            V[0] = d[1] * U[2] - d[2] * U[1];
            V[1] = d[2] * U[0] - d[0] * U[2];
            V[2] = d[0] * U[1] - d[1] * U[0];
        }
        const float zlo = std::min(c.z_near, c.z_far), zhi = std::max(c.z_near, c.z_far);
        const size_t box = out.boxes.size();
        out.boxes.insert(out.boxes.end(), {1e30f, 1e30f, 1e30f, -1e30f, -1e30f, -1e30f});
        const float cu = cam[0] * U[0] + cam[1] * U[1] + cam[2] * U[2], cv = cam[0] * V[0] + cam[1] * V[1] + cam[2] * V[2];
        for (uint32_t i = c.first; i < c.first + c.count && i < f.prims.size(); ++i) {
            const DropShadowPrim& p = f.prims[i];
            o2.clear();
            // the outline, counter-clockwise in (U, V): a circle's polygon (around it, not inside it), or a capsule's - the far arc of one
            // end's circle and the near arc of the other's between their outer tangents (cos a = (r0 - r1) / length from the axis)
            const float grow = 1.0f / std::cos(3.14159265f / static_cast<float>(ring));
            const float a0u = p.c0[0] * U[0] + p.c0[1] * U[1] + p.c0[2] * U[2], a0v = p.c0[0] * V[0] + p.c0[1] * V[1] + p.c0[2] * V[2];
            auto arc = [&](float cu0, float cv0, float r, float from, float to, int steps) {
                for (int k = 0; k <= steps; ++k) {
                    const float a = from + (to - from) * static_cast<float>(k) / static_cast<float>(steps);
                    o2.push_back(cu0 + r * grow * std::cos(a));
                    o2.push_back(cv0 + r * grow * std::sin(a));
                }
            };
            float r0 = std::max(p.r0, 0.0f), r1 = std::max(p.r1, 0.0f);
            bool one = !p.capsule;
            float b0u = 0.0f, b0v = 0.0f, L = 0.0f;
            if (p.capsule) {
                b0u = p.c1[0] * U[0] + p.c1[1] * U[1] + p.c1[2] * U[2];
                b0v = p.c1[0] * V[0] + p.c1[1] * V[1] + p.c1[2] * V[2];
                L = std::hypot(b0u - a0u, b0v - a0v);
                if (!(L > std::fabs(r0 - r1) + 1e-4f)) one = true;  // (one circle holds the other: the larger alone)
            }
            if (one) {
                float cu0 = a0u, cv0 = a0v, r = r0;
                if (p.capsule && r1 > r0) {
                    cu0 = b0u;
                    cv0 = b0v;
                    r = r1;
                }
                if (!(r > 0.0f)) continue;
                arc(cu0, cv0, r, 0.0f, 6.2831853f * static_cast<float>(ring - 1) / static_cast<float>(ring), ring - 1);
            } else {
                const float e = std::atan2(b0v - a0v, b0u - a0u);
                const float al = std::acos(std::clamp((r0 - r1) / L, -1.0f, 1.0f));
                const int nb = std::max(1, static_cast<int>(std::lround(static_cast<float>(ring) * al / 6.2831853f)));
                const int na = std::max(1, static_cast<int>(std::lround(static_cast<float>(ring) * (6.2831853f - 2.0f * al) / 6.2831853f)));
                arc(b0u, b0v, r1, e - al, e + al, nb);
                arc(a0u, a0v, r0, e + al, e + 6.2831853f - al, na);
            }
            const size_t n = o2.size() / 2;
            if (n < 3) continue;
            // each outline point's line: the point u U + v V + t d, at z = zlo and z = zhi
            top.resize(3 * n);
            bot.resize(3 * n);
            bool inside = cam[2] >= zlo && cam[2] <= zhi;
            for (size_t k = 0; k < n; ++k) {
                const float u = o2[2 * k], v = o2[2 * k + 1];
                const float P[3] = {u * U[0] + v * V[0], u * U[1] + v * V[1], u * U[2] + v * V[2]};
                const float th = (zhi - P[2]) / d[2], tl = (zlo - P[2]) / d[2];
                for (int a = 0; a < 3; ++a) {
                    top[3 * k + a] = P[a] + th * d[a];
                    bot[3 * k + a] = P[a] + tl * d[a];
                    out.boxes[box + a] = std::min({out.boxes[box + a], top[3 * k + a], bot[3 * k + a]});
                    out.boxes[box + 3 + a] = std::max({out.boxes[box + 3 + a], top[3 * k + a], bot[3 * k + a]});
                }
                top[3 * k + 2] = zhi;
                bot[3 * k + 2] = zlo;
                // (the camera inside the outline: left of every edge of the counter-clockwise hull)
                const size_t k1 = (k + 1) % n;
                const float ex = o2[2 * k1] - u, ey = o2[2 * k1 + 1] - v;
                if (ex * (cv - v) - ey * (cu - u) < 0.0f) inside = false;
            }
            ++out.prisms;
            if (inside) ++out.inside;
            for (int a = 0; a < 3; ++a) {
                out.lo[a] = std::min(out.lo[a], out.boxes[box + a]);
                out.hi[a] = std::max(out.hi[a], out.boxes[box + 3 + a]);
            }
            // the sides: outward normal (ey, -ex) in (U, V) - across the direction
            for (size_t k = 0; k < n; ++k) {
                const size_t k1 = (k + 1) % n;
                const float ex = o2[2 * k1] - o2[2 * k], ey = o2[2 * k1 + 1] - o2[2 * k + 1];
                const float N[3] = {ey * U[0] - ex * V[0], ey * U[1] - ex * V[1], ey * U[2] - ex * V[2]};
                const float* a = &top[3 * k];
                const bool fr = N[0] * (cam[0] - a[0]) + N[1] * (cam[1] - a[1]) + N[2] * (cam[2] - a[2]) > 0.0f;
                std::vector<float>& dst = fr ? out.front : out.back;
                put(dst, &top[3 * k], &bot[3 * k], &top[3 * k1]);
                put(dst, &bot[3 * k], &bot[3 * k1], &top[3 * k1]);
            }
            // the caps: the top's outward side up, the bottom's down
            std::vector<float>& tdst = cam[2] > zhi ? out.front : out.back;
            std::vector<float>& bdst = cam[2] < zlo ? out.front : out.back;
            for (size_t k = 2; k < n; ++k) {
                put(tdst, &top[0], &top[3 * (k - 1)], &top[3 * k]);
                put(bdst, &bot[0], &bot[3 * k], &bot[3 * (k - 1)]);
            }
        }
    }
}
namespace {
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
    // (2026-10-30, RAC1_WIIU_SHADOWS_GPU: the game's shadow code skipped, the shapes taken by the host - mrp_shadows_gpu.inc; the Wii U's
    // default, off elsewhere: the PC's game unchanged)
#if defined(__WIIU__)
    g_drop_shadows_gpu = 1;
#endif
    if (const char* e = std::getenv("RAC1_WIIU_SHADOWS_GPU")) g_drop_shadows_gpu = std::atoi(e) != 0 ? 1 : 0;
    g_ds_check = std::getenv("RAC1_WIIU_SHADOWS_GPU_CHECK") != nullptr && std::atoi(std::getenv("RAC1_WIIU_SHADOWS_GPU_CHECK")) != 0;
    g_ds_log = std::getenv("RAC1_WIIU_SHADOWS_GPU_LOG") != nullptr && std::atoi(std::getenv("RAC1_WIIU_SHADOWS_GPU_LOG")) != 0;
    g_ds_time = std::getenv("RAC1_WIIU_SHADOWS_GPU_TIME") != nullptr && std::atoi(std::getenv("RAC1_WIIU_SHADOWS_GPU_TIME")) != 0;
    g_drop_shadows_native = ds_capture;
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
    g_ds.casters.clear();
    g_ds.prims.clear();
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
    g_ds_draw = g_ds;
}

void mrp_shadows_clear_snapshot() {
    D.on = false;
    D.captured = false;
    D.have_apply = false;
    D.mark.clear();
    D.unmark.clear();
    g_ds_draw.casters.clear();
    g_ds_draw.prims.clear();
}

bool mrp_shadows_wiiu_view(MrpShadowsWiiuView& out) {
    const State& X = D;
    if (!X.on || !X.captured || !X.have_apply) return false;
    if (X.mark.empty() && X.unmark.empty()) return false;
    out.mark = X.mark.data();
    out.mark_floats = X.mark.size();
    out.unmark = X.unmark.data();
    out.unmark_floats = X.unmark.size();
    out.apply_alpha = X.apply_alpha;
    out.apply_rgba = X.apply_rgba;
    ++S.drawn_frames;
    return true;
}

// orig: (host) see mrp_shadows.h
bool mrp_shadows_draw(const float* view_proj) {
    // (a pipelined frame draws what was captured before the next update)
    const State& X0 = g_draw_from_snapshot ? D : S;
    // ((2026-10-30) RAC1_WIIU_SHADOWS_GPU on the PC: the host's shapes (mrp_shadows_gpu.inc) as the counter's faces - the prisms' faces
    // toward the camera marking, the others unmarking -, the original's darkening: the shapes and their place seen on the PC)
    static State N;
    const DropShadowFrame& nf = g_draw_from_snapshot ? g_ds_draw : g_ds;
    if (g_drop_shadows_gpu != 0 && !g_ds_check && X0.on && !X0.captured && !nf.casters.empty()) {
        float cam[3];
        static DropShadowVolumes vol;
        if (!mrp_drop_shadows_camera(view_proj, cam)) return false;
        const auto tv = std::chrono::steady_clock::now();
        mrp_drop_shadows_volumes(nf, cam, 12, vol);
        if (g_ds_time) {  // (the prisms' making: on the Wii U the draw's, mrp_world_gx2_draw_shadows)
            static double ms = 0.0;
            static uint64_t n = 0, faces = 0, prisms = 0;
            ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - tv).count();
            faces += (vol.front.size() + vol.back.size()) / 9;
            prisms += vol.prisms;
            if (++n % 120 == 0) {
                std::printf("[shadows-gpu] the prisms made for the draw: %.4f ms a frame, %.1f prisms, %.0f faces (120 frames)\n", ms / 120.0, prisms / 120.0, faces / 120.0);
                ms = 0.0;
                faces = prisms = 0;
            }
        }
        N.on = N.captured = N.have_apply = true;
        N.clear_rgba = 0x7f808080u;
        N.apply_alpha = (static_cast<uint64_t>(0x8000) << 22) | 0x64;
        N.apply_rgba = 0;
        N.mark.swap(vol.front);
        N.unmark.swap(vol.back);
    } else {
        N.on = false;
    }
    const State& X = N.on ? N : X0;
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
    if (g_ds_check)
        std::printf("[shadows-gpu-check] %llu frames, %llu deferred mobys (poses: %llu the scene's, %llu the game's), %llu shapes compared, %llu differ by more than "
                    "0.001, the largest difference %.6f (radius %.6f)\n",
                    static_cast<unsigned long long>(g_dsc.frames), static_cast<unsigned long long>(g_dsc.mobys), static_cast<unsigned long long>(g_dsc.scene),
                    static_cast<unsigned long long>(g_dsc.game), static_cast<unsigned long long>(g_dsc.points), static_cast<unsigned long long>(g_dsc.differ), g_dsc.max_d,
                    g_dsc.max_r);
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
