// The modern GPU renderer for mobys (ADR 0009).
// orig: (host) GPU renderer consuming unpacked VU1 moby DMA chains and rendering in native depth space.
#include <game/render/moby_gpu_renderer.h>
#include <profiler/host_profiler.h>

#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/vu/vu1.h>
#include <runtime/kernel/ee_memory.h>

#include <glad/glad.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>
#include <memory>

namespace rac1 {

namespace {

struct MobyGpuVertex {
    float pos[3];  // x (px), y (py), z_win (native [0, 1])
    uint8_t col[4];
    float tex[3];  // s, t, q
};

struct MobySubmeshDraw {
    uint32_t index_offset = 0;
    uint32_t index_count = 0;
    float proj_scale = 0.0f;
    gs::DrawState st{};
    std::shared_ptr<const gs::TexImage> tex;
    int mip_lo = 0;
    int mip_n = 0;
    uint64_t mip_tex0[3] = {};
    std::shared_ptr<const gs::TexImage> mip_tex[3];
};

static std::vector<MobyGpuVertex> s_frame_verts;
static std::vector<uint32_t> s_frame_indices;
static std::vector<MobySubmeshDraw> s_frame_submeshes;

static MobySubmeshDraw s_cur_sub;
static std::vector<uint32_t> s_cur_strip_queue;
static bool s_in_submesh = false;

static GLuint s_vao = 0;
static GLuint s_vbo = 0;
static GLuint s_ebo = 0;
static GLuint s_prog = 0;

static GLint s_loc_uFb = -1;
static GLint s_loc_uScaleX = -1;
static GLint s_loc_uTexture = -1;
static GLint s_loc_uTme = -1;
static GLint s_loc_uTfx = -1;
static GLint s_loc_uTcc = -1;
static GLint s_loc_uAte = -1;
static GLint s_loc_uAtst = -1;
static GLint s_loc_uAref = -1;
static GLint s_loc_uAfail = -1;
static GLint s_loc_uMip1 = -1;
static GLint s_loc_uMip2 = -1;
static GLint s_loc_uMip3 = -1;
static GLint s_loc_uMipN = -1;
static GLint s_loc_uMipLo = -1;
static GLint s_loc_uMipLcm = -1;
static GLint s_loc_uMipMxl = -1;
static GLint s_loc_uMipK = -1;
static GLint s_loc_uMipL = -1;
static GLint s_loc_uDebugShade = -1;

static uint64_t s_diff_submeshes = 0;
static uint64_t s_diff_verts = 0;
static uint64_t s_diff_mismatches = 0;

static const char* MOBY_VS = R"(
#version 330 core
layout (location = 0) in vec3 aPos;
layout (location = 1) in vec4 aColor;
layout (location = 2) in vec3 aTex;

uniform vec2 uFb;
uniform float uScaleX;

out vec4 vColor;
out vec3 vTex;

void main() {
    vColor = aColor;
    vTex = aTex;
    float x = (aPos.x / uFb.x * 2.0 - 1.0) * uScaleX;
    float y = 1.0 - aPos.y / uFb.y * 2.0;
    float z = aPos.z * 2.0 - 1.0;
    gl_Position = vec4(x, y, z, 1.0);
}
)";

static const char* MOBY_FS = R"(
#version 330 core
in vec4 vColor;
in vec3 vTex;

uniform sampler2D uTexture;
uniform int uTme, uTfx, uTcc, uAte, uAtst, uAref, uAfail;
uniform sampler2D uMip1, uMip2, uMip3;
uniform int uMipN, uMipLo, uMipLcm, uMipMxl;
uniform float uMipK, uMipL;
uniform int uDebugShade;

out vec4 FragColor;

void main() {
    vec4 c = vColor;
    vec4 o = c;
    if (uTme != 0) {
        vec2 st = vTex.xy / (vTex.z != 0.0 ? vTex.z : 1.0);
        vec4 t;
        int idx = 0;
        if (uMipN > 0) {
            float lod = uMipLcm != 0 ? uMipK : log2(1.0 / max(abs(vTex.z), 1e-20)) * uMipL + uMipK;
            int level = lod <= 0.0 ? 0 : min(int(floor(lod + 0.5)), uMipMxl);
            idx = clamp(level - uMipLo, 0, uMipN);
        }
        if (idx == 0) t = texture(uTexture, st);
        else if (idx == 1) t = texture(uMip1, st);
        else if (idx == 2) t = texture(uMip2, st);
        else t = texture(uMip3, st);

        if (uTfx == 0) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0); o.a = uTcc != 0 ? t.a * c.a * (255.0 / 128.0) : c.a; }
        else if (uTfx == 1) { o.rgb = t.rgb; o.a = uTcc != 0 ? t.a : c.a; }
        else if (uTfx == 2) { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a + c.a : c.a; }
        else { o.rgb = t.rgb * c.rgb * (255.0 / 128.0) + c.a; o.a = uTcc != 0 ? t.a : c.a; }
    }
    o = clamp(o, 0.0, 1.0);
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
        if (!pass && (uAfail == 0 || uAfail == 2)) discard;
    }
    if (uDebugShade == 1) o = vec4(c.rgb, o.a);
    else if (uDebugShade == 2 && uTme != 0) {
        vec2 st = vTex.xy / (vTex.z != 0.0 ? vTex.z : 1.0);
        o = vec4(texture(uTexture, st).rgb, o.a);
    }
    else if (uDebugShade == 3) o = vec4(vec3(0.8), o.a);

    FragColor = vec4(o.rgb, min(o.a * (255.0 / 128.0), 1.0));
}
)";

static GLuint compile_shader(GLenum type, const char* src) {
    GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[512];
        glGetShaderInfoLog(s, 512, nullptr, log);
        std::fprintf(stderr, "[moby_gpu] shader compilation error: %s\n", log);
    }
    return s;
}

}  // namespace

// orig: (host) initialize shaders and vertex buffers
void moby_gpu_init() {
    if (s_prog != 0) return;

    GLuint vs = compile_shader(GL_VERTEX_SHADER, MOBY_VS);
    GLuint fs = compile_shader(GL_FRAGMENT_SHADER, MOBY_FS);
    s_prog = glCreateProgram();
    glAttachShader(s_prog, vs);
    glAttachShader(s_prog, fs);
    glLinkProgram(s_prog);
    glDeleteShader(vs);
    glDeleteShader(fs);

    s_loc_uFb = glGetUniformLocation(s_prog, "uFb");
    s_loc_uScaleX = glGetUniformLocation(s_prog, "uScaleX");
    s_loc_uTexture = glGetUniformLocation(s_prog, "uTexture");
    s_loc_uTme = glGetUniformLocation(s_prog, "uTme");
    s_loc_uTfx = glGetUniformLocation(s_prog, "uTfx");
    s_loc_uTcc = glGetUniformLocation(s_prog, "uTcc");
    s_loc_uAte = glGetUniformLocation(s_prog, "uAte");
    s_loc_uAtst = glGetUniformLocation(s_prog, "uAtst");
    s_loc_uAref = glGetUniformLocation(s_prog, "uAref");
    s_loc_uAfail = glGetUniformLocation(s_prog, "uAfail");
    s_loc_uMip1 = glGetUniformLocation(s_prog, "uMip1");
    s_loc_uMip2 = glGetUniformLocation(s_prog, "uMip2");
    s_loc_uMip3 = glGetUniformLocation(s_prog, "uMip3");
    s_loc_uMipN = glGetUniformLocation(s_prog, "uMipN");
    s_loc_uMipLo = glGetUniformLocation(s_prog, "uMipLo");
    s_loc_uMipLcm = glGetUniformLocation(s_prog, "uMipLcm");
    s_loc_uMipMxl = glGetUniformLocation(s_prog, "uMipMxl");
    s_loc_uMipK = glGetUniformLocation(s_prog, "uMipK");
    s_loc_uMipL = glGetUniformLocation(s_prog, "uMipL");
    s_loc_uDebugShade = glGetUniformLocation(s_prog, "uDebugShade");

    glGenVertexArrays(1, &s_vao);
    glGenBuffers(1, &s_vbo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &s_vbo, "Mobys (GPU drawer, testing)");
    glGenBuffers(1, &s_ebo);
    rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &s_ebo, "Mobys (GPU drawer, testing)");

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);

    glVertexAttribPointer(0, 3, GL_FLOAT, GL_FALSE, sizeof(MobyGpuVertex), reinterpret_cast<void*>(0));
    glEnableVertexAttribArray(0);

    glVertexAttribPointer(1, 4, GL_UNSIGNED_BYTE, GL_TRUE, sizeof(MobyGpuVertex), reinterpret_cast<void*>(3 * sizeof(float)));
    glEnableVertexAttribArray(1);

    glVertexAttribPointer(2, 3, GL_FLOAT, GL_FALSE, sizeof(MobyGpuVertex), reinterpret_cast<void*>(3 * sizeof(float) + 4));
    glEnableVertexAttribArray(2);

    glBindVertexArray(0);
}

// orig: (host) free GPU resources
void moby_gpu_shutdown() {
    if (s_vao != 0) { glDeleteVertexArrays(1, &s_vao); s_vao = 0; }
    if (s_vbo != 0) { glDeleteBuffers(1, &s_vbo); s_vbo = 0; }
    if (s_ebo != 0) { glDeleteBuffers(1, &s_ebo); s_ebo = 0; }
    if (s_prog != 0) { glDeleteProgram(s_prog); s_prog = 0; }
}

// orig: (host) reset per-frame vertex and draw collections
void moby_gpu_begin_frame() {
    s_frame_verts.clear();
    s_frame_indices.clear();
    s_frame_submeshes.clear();
    s_in_submesh = false;
}

// orig: (host) begin a submesh before running VU1 microcode
void moby_gpu_begin_submesh(const VUState& st, uint32_t start_pc) {
    s_cur_sub = MobySubmeshDraw{};
    s_cur_strip_queue.clear();
    s_cur_sub.index_offset = static_cast<uint32_t>(s_frame_indices.size());

    uint32_t xtop = st.xtop & 0x3ff;
    bool is_buf1 = (start_pc == 0x008 || start_pc == 0x00c) ? true : (xtop != 0);
    uint32_t s_base = is_buf1 ? 0x287 : 0x123;
    const float* s_124 = reinterpret_cast<const float*>(&st.mem[((s_base + 1) & 0x3ff) << 4]);
    s_cur_sub.proj_scale = s_124[0];
    if (s_cur_sub.proj_scale == 0.0f || std::isnan(s_cur_sub.proj_scale)) {
        s_cur_sub.proj_scale = -0.0267885f;
    }

    // Default GS state from current GS registers
    rac1::gs::Gs& gs = rac1::gs::gs_global();
    uint32_t tag_hi = *reinterpret_cast<const uint32_t*>(&st.mem[(((s_base + 1) & 0x3ff) << 4) + 4]);
    uint64_t tag_lo = static_cast<uint64_t>(tag_hi) << 32;
    if ((tag_lo >> 46) & 1) {
        uint64_t prim = (tag_lo >> 47) & 0x7ff;
        gs.write_reg(0x00, prim);
    }
    uint64_t prim = (gs.reg(0x1a) & 1) ? gs.reg(0x00) : (gs.reg(0x1b) & ~7ull) | (gs.reg(0x00) & 7);
    int ctx = rac1::gs::field(prim, 9, 1);

    gs::DrawState& s = s_cur_sub.st;
    s.prim = prim;
    s.tex0 = gs.reg(0x06 + ctx);
    s.clamp = gs.reg(0x08 + ctx);
    s.tex1 = gs.reg(0x14 + ctx);
    s.miptbp1 = gs.reg(0x34 + ctx);
    s.alpha = gs.reg(0x42 + ctx);
    s.test = gs.reg(0x47 + ctx);
    s.scissor = gs.reg(0x40 + ctx);
    s.fba = gs.reg(0x4a + ctx);
    s.frame = gs.reg(0x4c + ctx);
    s.zbuf = gs.reg(0x4e + ctx);
    s.texa = gs.reg(0x3b);
    s.colclamp = gs.reg(0x46);
    s.pabe = gs.reg(0x49);
    s.fogcol = gs.reg(0x3d);
    s.dthe = gs.reg(0x45);

    s_in_submesh = true;
}

static void finalize_submesh(MobySubmeshDraw& sub) {
    sub.index_count = static_cast<uint32_t>(s_frame_indices.size()) - sub.index_offset;
    if (sub.index_count == 0) return;

    rac1::gs::Gs& gs = rac1::gs::gs_global();
    gs::DrawState& st = sub.st;

    static const bool s_moby_diff = std::getenv("RAC1_MOBY_DIFF") != nullptr;
    if (s_moby_diff) {
        int ctx = rac1::gs::field(st.prim, 9, 1);
        uint64_t gs_prim = (gs.reg(0x1a) & 1) ? gs.reg(0x00) : (gs.reg(0x1b) & ~7ull) | (gs.reg(0x00) & 7);
        bool match = true;
        if (st.prim != gs_prim) match = false;
        if (st.tex0 != gs.reg(0x06 + ctx)) match = false;
        if (st.clamp != gs.reg(0x08 + ctx)) match = false;
        if (st.tex1 != gs.reg(0x14 + ctx)) match = false;
        if (st.miptbp1 != gs.reg(0x34 + ctx)) match = false;
        if (st.alpha != gs.reg(0x42 + ctx)) match = false;
        if (st.test != gs.reg(0x47 + ctx)) match = false;
        if (!match) {
            ++s_diff_mismatches;
            static int s_logged = 0;
            if (s_logged < 10) {
                ++s_logged;
                std::fprintf(stderr, "[moby_diff_mismatch] submesh %zu: prim=%llx (gs=%llx), tex0=%016llx (gs=%016llx), clamp=%llx (gs=%llx), tex1=%llx (gs=%llx), miptbp1=%llx (gs=%llx), alpha=%llx (gs=%llx), test=%llx (gs=%llx)\n",
                    s_frame_submeshes.size(),
                    (unsigned long long)st.prim, (unsigned long long)gs_prim,
                    (unsigned long long)st.tex0, (unsigned long long)gs.reg(0x06 + ctx),
                    (unsigned long long)st.clamp, (unsigned long long)gs.reg(0x08 + ctx),
                    (unsigned long long)st.tex1, (unsigned long long)gs.reg(0x14 + ctx),
                    (unsigned long long)st.miptbp1, (unsigned long long)gs.reg(0x34 + ctx),
                    (unsigned long long)st.alpha, (unsigned long long)gs.reg(0x42 + ctx),
                    (unsigned long long)st.test, (unsigned long long)gs.reg(0x47 + ctx));
            }
        }
    }

    if (rac1::gs::field(st.prim, 4, 1) != 0) {
        const uint32_t mxl = std::min(rac1::gs::field(st.tex1, 2, 3), 3u);
        if (mxl != 0 && !rac1::gs::field(st.prim, 8, 1) && rac1::gs::field(st.tex1, 6, 3) >= 2) {
            const float k = static_cast<float>(static_cast<int32_t>(rac1::gs::field(st.tex1, 32, 12) << 20) >> 20) / 16.0f;
            auto level_of = [&](float q) {
                if (std::isnan(q)) return 0;
                const float lod = rac1::gs::field(st.tex1, 0, 1) != 0 ? k : std::log2(1.0f / std::max(std::fabs(q), 1e-20f)) * static_cast<float>(1u << rac1::gs::field(st.tex1, 19, 2)) + k;
                if (std::isnan(lod) || lod <= 0.0f) return 0;
                return std::min(static_cast<int>(std::floor(lod + 0.5f)), static_cast<int>(mxl));
            };
            int lo = 3, hi = 0;
            for (uint32_t idx = sub.index_offset; idx < sub.index_offset + sub.index_count; ++idx) {
                float q = s_frame_verts[s_frame_indices[idx]].tex[2];
                lo = std::min(lo, level_of(q));
                hi = std::max(hi, level_of(q));
            }
            lo = std::clamp(lo, 0, static_cast<int>(mxl));
            hi = std::clamp(hi, lo, static_cast<int>(mxl));
            auto level_tex0 = [&](int level) -> uint64_t {
                if (level == 0) return st.tex0;
                const uint32_t lv = static_cast<uint32_t>(level - 1);
                const uint64_t tbp = rac1::gs::field(st.miptbp1, static_cast<int>(20 * lv), 14), tbw = rac1::gs::field(st.miptbp1, static_cast<int>(20 * lv + 14), 6);
                const uint64_t tw = rac1::gs::field(st.tex0, 26, 4), th = rac1::gs::field(st.tex0, 30, 4);
                const uint64_t ntw = tw > lv + 1 ? tw - lv - 1 : 0, nth = th > lv + 1 ? th - lv - 1 : 0;
                return (st.tex0 & ~((0x3fffull) | (0x3full << 14) | (0xfull << 26) | (0xfull << 30))) | tbp | (tbw << 14) | (ntw << 26) | (nth << 30);
            };
            sub.mip_lo = lo;
            sub.mip_n = hi - lo;
            for (int l = lo + 1; l <= hi; ++l) {
                int m = l - lo - 1;
                if (m >= 0 && m < 3) {
                    sub.mip_tex0[m] = level_tex0(l);
                    sub.mip_tex[m] = gs.texture_at_kick(sub.mip_tex0[m], st.texa);
                }
            }
            st.tex0 = level_tex0(lo);
        }
        sub.tex = gs.texture_at_kick(st.tex0, st.texa);
    }

    s_frame_submeshes.push_back(sub);
}

// orig: (host) process a GIF packet kicked by VU1 microcode
void moby_gpu_process_kick(const VUState& st, uint32_t /*start_pc*/, uint32_t kick_addr) {
    if (!s_in_submesh) return;

    auto split_submesh = [&]() {
        if (s_frame_indices.size() > s_cur_sub.index_offset) {
            finalize_submesh(s_cur_sub);
            MobySubmeshDraw next_sub{};
            next_sub.index_offset = static_cast<uint32_t>(s_frame_indices.size());
            next_sub.proj_scale = s_cur_sub.proj_scale;
            next_sub.st = s_cur_sub.st;
            s_cur_sub = next_sub;
        }
    };

    rac1::gs::Gs& gs = rac1::gs::gs_global();
    int ctx = rac1::gs::field(s_cur_sub.st.prim, 9, 1);
    uint64_t off_reg = gs.reg(0x18 + ctx);

    float fb_w = static_cast<float>(ee_read<uint16_t>(0x001518d0));
    float fb_h = static_cast<float>(ee_read<uint16_t>(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;

    float off_x = (off_reg != 0) ? static_cast<float>(off_reg & 0xffff) : (2048.0f - fb_w * 0.5f) * 16.0f;
    float off_y = (off_reg != 0) ? static_cast<float>((off_reg >> 32) & 0xffff) : (2048.0f - fb_h * 0.5f) * 16.0f;

    const float n = kRenderNear, f = kRenderFar;
    const float proj_scale = s_cur_sub.proj_scale;

    uint32_t qwc = vu1::gif_packet_qwc(st, kick_addr);
    uint32_t off = 0;
    while (off < qwc) {
        uint64_t tag_lo = 0, tag_hi = 0;
        std::memcpy(&tag_lo, &st.mem[((kick_addr + off) & 0x3ff) << 4], 8);
        std::memcpy(&tag_hi, &st.mem[(((kick_addr + off) & 0x3ff) << 4) + 8], 8);
        uint32_t nloop = tag_lo & 0x7fff;
        bool eop = (tag_lo >> 15) & 1;
        uint32_t flg = (tag_lo >> 58) & 3;
        uint32_t nreg = ((tag_lo >> 60) & 0xf) == 0 ? 16 : ((tag_lo >> 60) & 0xf);

        static const bool s_trace = std::getenv("RAC1_MOBY_TRACE") != nullptr;
        if (s_trace) {
            std::printf("[moby_kick] tag_lo=%016llx tag_hi=%016llx flg=%u nloop=%u nreg=%u cur_prim=%llx\n",
                (unsigned long long)tag_lo, (unsigned long long)tag_hi, flg, nloop, nreg, (unsigned long long)s_cur_sub.st.prim);
        }

        if (flg == 0 && ((tag_lo >> 46) & 1)) {
            uint64_t prim = (tag_lo >> 47) & 0x7ff;
            if (s_cur_sub.st.prim != prim) {
                split_submesh();
                s_cur_sub.st.prim = prim;
                gs.write_reg(0x00, prim);
                int new_ctx = rac1::gs::field(prim, 9, 1);
                s_cur_sub.st.tex0 = gs.reg(0x06 + new_ctx);
                s_cur_sub.st.clamp = gs.reg(0x08 + new_ctx);
                s_cur_sub.st.tex1 = gs.reg(0x14 + new_ctx);
                s_cur_sub.st.miptbp1 = gs.reg(0x34 + new_ctx);
                s_cur_sub.st.alpha = gs.reg(0x42 + new_ctx);
                s_cur_sub.st.test = gs.reg(0x47 + new_ctx);
                s_cur_sub.st.zbuf = gs.reg(0x4e + new_ctx);
            } else {
                gs.write_reg(0x00, prim);
            }
            s_cur_strip_queue.clear();
        }

        if (flg == 0 && (tag_hi & 0xf) == 0xe) {
            // A+D packet: update GS state
            for (uint32_t i = 0; i < nloop; ++i) {
                uint64_t val = 0;
                std::memcpy(&val, &st.mem[((kick_addr + off + 1 + i) & 0x3ff) << 4], 8);
                uint8_t reg = st.mem[(((kick_addr + off + 1 + i) & 0x3ff) << 4) + 8] & 0x7f;
                int ctx = rac1::gs::field(s_cur_sub.st.prim, 9, 1);
                if (reg == 0x06 + ctx) {
                    if (s_cur_sub.st.tex0 != val) { split_submesh(); s_cur_sub.st.tex0 = val; }
                } else if (reg == 0x08 + ctx) {
                    if (s_cur_sub.st.clamp != val) { split_submesh(); s_cur_sub.st.clamp = val; }
                } else if (reg == 0x14 + ctx) {
                    if (s_cur_sub.st.tex1 != val) { split_submesh(); s_cur_sub.st.tex1 = val; }
                } else if (reg == 0x34 + ctx) {
                    if (s_cur_sub.st.miptbp1 != val) { split_submesh(); s_cur_sub.st.miptbp1 = val; }
                } else if (reg == 0x42 + ctx) {
                    if (s_cur_sub.st.alpha != val) { split_submesh(); s_cur_sub.st.alpha = val; }
                } else if (reg == 0x47 + ctx) {
                    if (s_cur_sub.st.test != val) { split_submesh(); s_cur_sub.st.test = val; }
                } else if (reg == 0x4e + ctx) {
                    if (s_cur_sub.st.zbuf != val) { split_submesh(); s_cur_sub.st.zbuf = val; }
                } else if (reg == 0x00) {
                    if (s_cur_sub.st.prim != val) {
                        split_submesh();
                        s_cur_sub.st.prim = val;
                        int new_ctx = rac1::gs::field(val, 9, 1);
                        s_cur_sub.st.tex0 = gs.reg(0x06 + new_ctx);
                        s_cur_sub.st.clamp = gs.reg(0x08 + new_ctx);
                        s_cur_sub.st.tex1 = gs.reg(0x14 + new_ctx);
                        s_cur_sub.st.miptbp1 = gs.reg(0x34 + new_ctx);
                        s_cur_sub.st.alpha = gs.reg(0x42 + new_ctx);
                        s_cur_sub.st.test = gs.reg(0x47 + new_ctx);
                        s_cur_sub.st.zbuf = gs.reg(0x4e + new_ctx);
                    }
                    s_cur_strip_queue.clear();
                }
                gs.write_reg(reg, val);
            }
        } else if (flg == 0 && (tag_hi & 0xfff) == 0x412) {
            // Vertex packet: STQ, RGBAQ, XYZF2
            for (uint32_t v = 0; v < nloop; ++v) {
                const uint8_t* stq_p = &st.mem[((kick_addr + off + 1 + v * 3) & 0x3ff) << 4];
                const uint8_t* col_p = &st.mem[((kick_addr + off + 1 + v * 3 + 1) & 0x3ff) << 4];
                const uint8_t* pos_p = &st.mem[((kick_addr + off + 1 + v * 3 + 2) & 0x3ff) << 4];

                float s, t, q;
                std::memcpy(&s, stq_p, 4);
                std::memcpy(&t, stq_p + 4, 4);
                std::memcpy(&q, stq_p + 8, 4);

                uint32_t r, g, b, a;
                std::memcpy(&r, col_p, 4);
                std::memcpy(&g, col_p + 4, 4);
                std::memcpy(&b, col_p + 8, 4);
                std::memcpy(&a, col_p + 12, 4);

                uint32_t x_fix, y_fix, z_fix, w_flg;
                std::memcpy(&x_fix, pos_p, 4);
                std::memcpy(&y_fix, pos_p + 4, 4);
                std::memcpy(&z_fix, pos_p + 8, 4);
                std::memcpy(&w_flg, pos_p + 12, 4);

                float vx = (static_cast<float>(x_fix & 0xffff) - off_x) / 16.0f;
                float vy = (static_cast<float>(y_fix & 0xffff) - off_y) / 16.0f;

                // Native window depth in [0, 1]
                float z_win = 0.999f;
                if (!std::isnan(q) && q > 1e-8f && proj_scale < -1e-6f) {
                    z_win = (f / (f - n)) + ((f * n) / ((f - n) * proj_scale)) * q;
                    z_win = std::clamp(z_win, 0.0f, 1.0f);
                } else if (!std::isnan(q) && q < -1e-8f && proj_scale > 1e-6f) {
                    z_win = (f / (f - n)) + ((f * n) / ((f - n) * proj_scale)) * q;
                    z_win = std::clamp(z_win, 0.0f, 1.0f);
                }
                if (s_trace && v < 3) {
                    std::printf("[v_depth] v=%u q=%f proj_scale=%f -> z_win=%f\n", v, q, proj_scale, z_win);
                }

                MobyGpuVertex vert;
                vert.pos[0] = vx;
                vert.pos[1] = vy;
                vert.pos[2] = z_win;
                vert.col[0] = static_cast<uint8_t>(r);
                vert.col[1] = static_cast<uint8_t>(g);
                vert.col[2] = static_cast<uint8_t>(b);
                vert.col[3] = static_cast<uint8_t>(a);
                vert.tex[0] = s;
                vert.tex[1] = t;
                vert.tex[2] = q;
                s_frame_verts.push_back(vert);

                uint32_t curr_idx = static_cast<uint32_t>(s_frame_verts.size() - 1);
                bool adc = ((w_flg >> 15) & 1) != 0;
                bool draw = !adc;

                const int type = rac1::gs::field(s_cur_sub.st.prim, 0, 3);
                int need = 0;
                switch (type) {
                case 1: case 2: need = 2; break;
                case 3: case 4: case 5: need = 3; break;
                case 6: need = 2; break;
                default: s_cur_strip_queue.clear(); need = 0; break;
                }

                if (need > 0) {
                    s_cur_strip_queue.push_back(curr_idx);
                    if (static_cast<int>(s_cur_strip_queue.size()) >= need) {
                        if (draw) {
                            if (need == 3) {
                                s_frame_indices.push_back(s_cur_strip_queue[s_cur_strip_queue.size() - 3]);
                                s_frame_indices.push_back(s_cur_strip_queue[s_cur_strip_queue.size() - 2]);
                                s_frame_indices.push_back(s_cur_strip_queue[s_cur_strip_queue.size() - 1]);
                            }
                        }
                        switch (type) {
                        case 2: s_cur_strip_queue.erase(s_cur_strip_queue.begin(), s_cur_strip_queue.end() - 1); break;
                        case 4: s_cur_strip_queue.erase(s_cur_strip_queue.begin()); break;
                        case 5: s_cur_strip_queue.erase(s_cur_strip_queue.begin() + 1); break;
                        default: s_cur_strip_queue.clear(); break;
                        }
                    }
                }
            }
        }

        uint32_t body = flg == 0 ? nloop * nreg : flg == 1 ? (nloop * nreg + 1) / 2 : nloop;
        off += 1 + body;
        if (eop) break;
    }
}

// orig: (host) end current submesh after VU1 microcode finishes
void moby_gpu_end_submesh() {
    if (!s_in_submesh) return;
    s_in_submesh = false;
    finalize_submesh(s_cur_sub);
}

// orig: (host) legacy entry for standalone submesh extraction
void moby_gpu_submesh(const VUState& st, uint32_t start_pc) {
    (void)st;
    (void)start_pc;
}

// orig: (host) differential comparison between faithful kicked vertices and GPU twin
void moby_gpu_diff_compare(const VUState& /*st*/, uint32_t /*start_pc*/, const std::vector<MobyDiffVert>& faithful_verts) {
    if (faithful_verts.empty()) return;
    ++s_diff_submeshes;
    s_diff_verts += faithful_verts.size();
}

// orig: (host) return statistics of differential checks
bool moby_gpu_diff_stats(uint64_t& submeshes, uint64_t& vertices, uint64_t& mismatches) {
    submeshes = s_diff_submeshes;
    vertices = s_diff_verts;
    mismatches = s_diff_mismatches;
    return s_diff_submeshes > 0;
}

// orig: (host) render all collected moby submeshes with native OpenGL depth testing
void moby_gpu_draw(int win_w, int win_h, const gs::Viewport* vp) {
    static const bool s_trace = std::getenv("RAC1_MOBY_TRACE") != nullptr;
    if (s_trace) {
        std::printf("[moby_gpu_draw] submeshes=%zu verts=%zu indices=%zu\n",
            s_frame_submeshes.size(), s_frame_verts.size(), s_frame_indices.size());
        for (size_t i = 0; i < s_frame_submeshes.size(); ++i) {
            const auto& sub = s_frame_submeshes[i];
            std::printf("  sub %zu: off=%u count=%u tex0=%016llx prim=%llx test=%llx tex=%p proj_scale=%f\n",
                i, sub.index_offset, sub.index_count, (unsigned long long)sub.st.tex0,
                (unsigned long long)sub.st.prim, (unsigned long long)sub.st.test, static_cast<const void*>(sub.tex.get()), sub.proj_scale);
            if (sub.tex != nullptr && !sub.tex->px.empty()) {
                std::printf("    tex w=%d h=%d px[0..3]=%08x %08x %08x %08x\n",
                    sub.tex->w, sub.tex->h, sub.tex->px[0], sub.tex->px[1], sub.tex->px[2], sub.tex->px[3]);
            }
            if (i >= 71) {
                for (uint32_t k = 0; k < std::min(sub.index_count, 6u); ++k) {
                    uint32_t idx = s_frame_indices[sub.index_offset + k];
                    const auto& v = s_frame_verts[idx];
                    std::printf("    v %u: pos=(%.1f, %.1f, %.4f) col=(%u,%u,%u,%u) tex=(%.2f, %.2f, %.4f)\n",
                        idx, v.pos[0], v.pos[1], v.pos[2], v.col[0], v.col[1], v.col[2], v.col[3], v.tex[0], v.tex[1], v.tex[2]);
                }
            }
        }
    }
    if (s_frame_submeshes.empty() || s_frame_indices.empty()) return;
    auto t0 = std::chrono::steady_clock::now();
    moby_gpu_init();

    glBindVertexArray(s_vao);
    glBindBuffer(GL_ARRAY_BUFFER, s_vbo);
    glBufferData(GL_ARRAY_BUFFER, s_frame_verts.size() * sizeof(MobyGpuVertex), s_frame_verts.data(), GL_STREAM_DRAW);

    glBindBuffer(GL_ELEMENT_ARRAY_BUFFER, s_ebo);
    glBufferData(GL_ELEMENT_ARRAY_BUFFER, s_frame_indices.size() * sizeof(uint32_t), s_frame_indices.data(), GL_STREAM_DRAW);

    glUseProgram(s_prog);

    glUniform1i(s_loc_uTexture, 0);
    glUniform1i(s_loc_uMip1, 2);
    glUniform1i(s_loc_uMip2, 3);
    glUniform1i(s_loc_uMip3, 4);

    float scale_x = 1.0f;
    if (vp != nullptr) {
        glViewport(vp->x, vp->y, vp->w, vp->h);
        scale_x = vp->xscale_3d;
    } else {
        glViewport(0, 0, win_w, win_h);
    }

    float fb_w = static_cast<float>(ee_read<uint16_t>(0x001518d0));
    float fb_h = static_cast<float>(ee_read<uint16_t>(0x001518d2));
    if (fb_w <= 0.0f) fb_w = 512.0f;
    if (fb_h <= 0.0f) fb_h = 416.0f;
    glUniform2f(s_loc_uFb, fb_w, fb_h);
    glUniform1f(s_loc_uScaleX, scale_x);

    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glDepthMask(GL_TRUE);
    glDisable(GL_CULL_FACE);

    rac1::gs::Gs& gs = rac1::gs::gs_global();

    for (const auto& sub : s_frame_submeshes) {
        const gs::DrawState& st = sub.st;
        const bool tme = rac1::gs::field(st.prim, 4, 1) != 0;

        if (tme && sub.tex != nullptr) {
            glActiveTexture(GL_TEXTURE0);
            glBindTexture(GL_TEXTURE_2D, rac1::gs::texture_for(gs, st.tex0, st.texa, sub.tex.get()));
            const bool lin = rac1::gs::field(st.tex1, 5, 1) != 0;
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, lin ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lin ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rac1::gs::field(st.clamp, 0, 2) == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rac1::gs::field(st.clamp, 2, 2) == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);

            const bool lin_min = rac1::gs::field(st.tex1, 6, 3) >= 4;
            for (int m = 0; m < sub.mip_n; ++m) {
                glActiveTexture(GL_TEXTURE2 + m);
                glBindTexture(GL_TEXTURE_2D, rac1::gs::texture_for(gs, sub.mip_tex0[m], st.texa, sub.mip_tex[m].get()));
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, lin_min ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, lin_min ? GL_LINEAR : GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, rac1::gs::field(st.clamp, 0, 2) == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, rac1::gs::field(st.clamp, 2, 2) == 0 ? GL_REPEAT : GL_CLAMP_TO_EDGE);
            }
            glActiveTexture(GL_TEXTURE0);
        }

        glUniform1i(s_loc_uTme, tme ? 1 : 0);
        glUniform1i(s_loc_uTfx, static_cast<GLint>(rac1::gs::field(st.tex0, 35, 2)));
        glUniform1i(s_loc_uTcc, static_cast<GLint>(rac1::gs::field(st.tex0, 34, 1)));
        glUniform1i(s_loc_uAte, static_cast<GLint>(rac1::gs::field(st.test, 0, 1)));
        glUniform1i(s_loc_uAtst, static_cast<GLint>(rac1::gs::field(st.test, 1, 3)));
        glUniform1i(s_loc_uAref, static_cast<GLint>(rac1::gs::field(st.test, 4, 8)));
        glUniform1i(s_loc_uAfail, static_cast<GLint>(rac1::gs::field(st.test, 12, 2)));
        glUniform1i(s_loc_uDebugShade, g_render_shade);

        glUniform1i(s_loc_uMipN, tme ? sub.mip_n : 0);
        glUniform1i(s_loc_uMipLo, sub.mip_lo);
        glUniform1i(s_loc_uMipLcm, static_cast<GLint>(rac1::gs::field(st.tex1, 0, 1)));
        glUniform1i(s_loc_uMipMxl, static_cast<GLint>(std::min(rac1::gs::field(st.tex1, 2, 3), 3u)));
        glUniform1f(s_loc_uMipK, static_cast<float>(static_cast<int32_t>(rac1::gs::field(st.tex1, 32, 12) << 20) >> 20) / 16.0f);
        glUniform1f(s_loc_uMipL, static_cast<float>(1u << rac1::gs::field(st.tex1, 19, 2)));

        rac1::gs::blend_state(st);

        const uint32_t ztst = rac1::gs::field(st.test, 17, 2);
        const bool ztest = rac1::gs::field(st.test, 16, 1) != 0 && ztst != 1;
        if (ztest) {
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(ztst == 0 ? GL_NEVER : ztst == 2 ? GL_LEQUAL : GL_LESS);
        } else {
            glDisable(GL_DEPTH_TEST);
        }
        const bool zmsk = rac1::gs::field(st.zbuf, 32, 1) != 0;
        glDepthMask(zmsk ? GL_FALSE : GL_TRUE);

        glDrawElements(GL_TRIANGLES, sub.index_count, GL_UNSIGNED_INT,
                       reinterpret_cast<const void*>(static_cast<uintptr_t>(sub.index_offset * sizeof(uint32_t))));
    }

    glDisable(GL_BLEND);
    glDepthMask(GL_TRUE);
    glBindVertexArray(0);

    static const bool s_moby_bench = std::getenv("RAC1_MOBY_BENCH") != nullptr;
    if (s_moby_bench) {
        static auto s_last_report = std::chrono::steady_clock::now();
        static double s_gpu_ms = 0;
        static int s_gpu_frames = 0;
        s_gpu_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
        ++s_gpu_frames;
        if (std::chrono::duration<double>(std::chrono::steady_clock::now() - s_last_report).count() >= 1.0) {
            std::printf("[moby_bench] GPU moby pass: %.3f ms / frame (%d frames, %zu submeshes, %zu vertices)\n",
                s_gpu_ms / s_gpu_frames, s_gpu_frames, s_frame_submeshes.size(), s_frame_verts.size());
            s_gpu_ms = 0;
            s_gpu_frames = 0;
            s_last_report = std::chrono::steady_clock::now();
        }
    }
}

// orig: (host) compare drawn mobys in offscreen buffers and diff GS state
void moby_gpu_diff_compare_drawn(int w, int h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth) {
    if (s_frame_submeshes.empty()) return;
    int vw = vp ? vp->w : w;
    int vh = vp ? vp->h : h;
    if (vw <= 0 || vh <= 0) return;

    static GLuint fbo_a = 0, tex_a = 0, rbo_a = 0;
    static GLuint fbo_b = 0, tex_b = 0, rbo_b = 0;
    static int fbo_w = 0, fbo_h = 0;

    if (fbo_a == 0 || fbo_w != vw || fbo_h != vh) {
        if (fbo_a != 0) {
            glDeleteFramebuffers(1, &fbo_a); glDeleteTextures(1, &tex_a); glDeleteRenderbuffers(1, &rbo_a);
            glDeleteFramebuffers(1, &fbo_b); glDeleteTextures(1, &tex_b); glDeleteRenderbuffers(1, &rbo_b);
        }
        fbo_w = vw; fbo_h = vh;
        auto init_fbo = [](GLuint& fbo, GLuint& tex, GLuint& rbo, int fw, int fh) {
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex, "Mobys (GPU drawer, testing)");
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, fw, fh, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);

            glGenRenderbuffers(1, &rbo);
            rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &rbo, "Mobys (GPU drawer, testing)");
            glBindRenderbuffer(GL_RENDERBUFFER, rbo);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH_COMPONENT24, fw, fh);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_RENDERBUFFER, rbo);
        };
        init_fbo(fbo_a, tex_a, rbo_a, vw, vh);
        init_fbo(fbo_b, tex_b, rbo_b, vw, vh);
    }

    gs::Viewport local_vp = vp ? *vp : gs::Viewport{0, 0, vw, vh, 1.0f, 1.0f, -1};
    local_vp.x = 0; local_vp.y = 0; local_vp.w = vw; local_vp.h = vh;

    // Render faithful path into FBO A
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_a);
    glViewport(0, 0, vw, vh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    rac1::gs::draw(gs, vw, vh, fb_w, fb_h, depth, &local_vp, 1);



    // Render GPU path into FBO B
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_b);
    glViewport(0, 0, vw, vh);
    glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    moby_gpu_draw(vw, vh, &local_vp);

    // Readback & compare
    std::vector<uint8_t> pix_a(static_cast<size_t>(vw) * vh * 4), pix_b(static_cast<size_t>(vw) * vh * 4);
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_a);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, pix_a.data());
    glBindFramebuffer(GL_FRAMEBUFFER, fbo_b);
    glReadPixels(0, 0, vw, vh, GL_RGBA, GL_UNSIGNED_BYTE, pix_b.data());

    glBindFramebuffer(GL_FRAMEBUFFER, 0);

    if (frame == 300 || frame == 466) {
        auto save_ppm = [](const char* fn, int fw, int fh, const uint8_t* px) {
            FILE* f = std::fopen(fn, "wb");
            if (!f) return;
            std::fprintf(f, "P6\n%d %d\n255\n", fw, fh);
            for (int y = fh - 1; y >= 0; --y) {
                for (int x = 0; x < fw; ++x) {
                    size_t o = (static_cast<size_t>(y) * fw + x) * 4;
                    std::fwrite(&px[o], 1, 3, f);
                }
            }
            std::fclose(f);
        };
        char fn_a[64], fn_b[64];
        std::snprintf(fn_a, sizeof(fn_a), "diff_fbo_a_%llu.ppm", (unsigned long long)frame);
        std::snprintf(fn_b, sizeof(fn_b), "diff_fbo_b_%llu.ppm", (unsigned long long)frame);
        save_ppm(fn_a, vw, vh, pix_a.data());
        save_ppm(fn_b, vw, vh, pix_b.data());
    }

    uint32_t diff_count = 0;
    uint32_t drawn_count = 0;
    for (int y = 0; y < vh; ++y) {
        for (int x = 0; x < vw; ++x) {
            size_t off = (static_cast<size_t>(y) * vw + x) * 4;
            bool drawn_a = pix_a[off + 3] > 0;
            bool drawn_b = pix_b[off + 3] > 0;
            if (drawn_a || drawn_b) {
                ++drawn_count;
                int dr = std::abs(int(pix_a[off + 0]) - int(pix_b[off + 0]));
                int dg = std::abs(int(pix_a[off + 1]) - int(pix_b[off + 1]));
                int db = std::abs(int(pix_a[off + 2]) - int(pix_b[off + 2]));
                int da = std::abs(int(pix_a[off + 3]) - int(pix_b[off + 3]));
                if (dr > 5 || dg > 5 || db > 5 || da > 5) {
                    ++diff_count;
                }
            }
        }
    }
    float match_pct = drawn_count > 0 ? (100.0f * (1.0f - float(diff_count) / float(drawn_count))) : 100.0f;
    if (frame % 60 == 0 || diff_count > drawn_count / 10) {
        std::printf("[moby_diff] Frame %llu: %zu submeshes, state mismatches: %llu, drawn pixels: %u, diff: %u (%.2f%% match)\n",
            (unsigned long long)frame, s_frame_submeshes.size(), (unsigned long long)s_diff_mismatches, drawn_count, diff_count, match_pct);
    }
}

}  // namespace rac1
