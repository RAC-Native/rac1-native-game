// The MRP's ambient particles (see mrp_particles.h; docs/particles.md). Host code, an enhancement: nothing here is the game's.
#include <game/render/mrp_particles.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_fx.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_view.h>
#include <profiler/host_profiler.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>
#include <glm/glm.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <vector>

namespace rac1 {

namespace {
// orig: (host) an integer environment variable (`def` when unset)
int env_int(const char* name, int def) {
    const char* e = std::getenv(name);
    return e != nullptr ? std::atoi(e) : def;
}
}  // namespace

int g_ambient_particles = std::clamp(env_int("RAC1_PARTICLES", 0), 0, 2);
int g_particles_show_game = env_int("RAC1_PARTICLES_GAME", 0) != 0 ? 1 : 0;

namespace {

// ---- the kinds ---------------------------------------------------------------------------------------------------------------
// Our art choices. The counts are High's (Low: half), times the line's density; sizes in world units (Ratchet is about 1.2 tall)
struct Kind {
    int count;            // particles in the volume (High, density 1)
    glm::vec3 box;        // the camera's volume (x, y, z units)
    float fall;           // vertical speed, units a second (negative: falling)
    float drift;          // the slow wander's amplitude (units)
    float size, spread;   // the size, its spread (0..1)
    glm::vec3 col;        // the colour (the line may give its own)
    float alpha;
    bool additive;        // glows: added to the picture (embers, fireflies, motes)
    bool emissive;        // their own light: not dimmed by the zone's (embers, fireflies)
    bool outdoors;        // not drawn inside the level's interior boxes (what falls from the sky)
    float streak;         // rain: the streak's length in seconds of its motion
    float min_px;         // the smallest drawn size, pixels (smaller: the alpha lowered instead)
    float soft;           // the distance over which a particle fades into the surface behind it (units)
};
constexpr int kMaxCount = 24000;
const Kind kKinds[kPartKinds] = {
    // count  box                      fall    drift size   spread colour                      alpha add    emit   out    streak  min_px soft
    {2600, {30.0f, 30.0f, 16.0f}, -0.04f, 0.70f, 0.022f, 0.50f, {0.86f, 0.80f, 0.68f}, 0.70f, false, false, false, 0.0f, 1.8f, 0.5f},   // dust
    {2400, {32.0f, 32.0f, 20.0f}, -0.45f, 0.90f, 0.030f, 0.50f, {0.58f, 0.56f, 0.54f}, 0.80f, false, false, true, 0.0f, 1.6f, 0.5f},    // ash
    {2000, {30.0f, 30.0f, 16.0f}, 1.60f, 0.80f, 0.040f, 0.40f, {1.00f, 0.42f, 0.10f}, 1.00f, true, true, false, 0.0f, 2.0f, 0.4f},      // embers
    {400, {32.0f, 32.0f, 8.0f}, 0.00f, 1.60f, 0.060f, 0.30f, {0.72f, 1.00f, 0.32f}, 1.00f, true, true, false, 0.0f, 2.5f, 0.4f},        // fireflies
    {3000, {24.0f, 24.0f, 12.0f}, -0.02f, 0.45f, 0.020f, 0.50f, {1.00f, 0.95f, 0.82f}, 0.90f, true, false, false, 0.0f, 2.0f, 0.4f},    // motes
    {9000, {32.0f, 32.0f, 22.0f}, -22.0f, 0.00f, 0.010f, 0.30f, {0.72f, 0.77f, 0.86f}, 0.30f, false, false, true, 0.028f, 1.1f, 0.6f},  // rain
    {9000, {30.0f, 30.0f, 20.0f}, -1.30f, 0.70f, 0.028f, 0.45f, {1.00f, 1.00f, 1.00f}, 0.85f, false, false, true, 0.0f, 1.2f, 0.5f},    // snow
};
constexpr double kTimePeriod = 1200.0;  // (the particles' time wraps every 20 minutes: the floats stay exact)
constexpr float kSplashRange = 18.0f;    // (rain's splashes: up to this far from the camera)

// The uniforms of one draw (std140: vec4s and mat4s; the shaders' PartUbo)
struct Ubo {
    float vp[16];
    float inv_vp[16];
    float cam[4];      // xyz the camera, w the time (s)
    float box[4];      // xyz the volume, w the count
    float vel[4];      // xyz the base velocity, w the kind
    float col[4];      // rgb the colour (lit), a the alpha
    float size[4];     // x size, y spread, z streak (s), w the soft distance
    float cam_vel[4];  // xyz the camera's velocity (units / s)
    float sun[4];      // xyz toward the sun, w the forward scatter's weight
    float vpr[4];      // the viewport (pixels)
    float proj[4];     // P22, P32, P00, P11
    float lim_lo[4];   // xyz, w 1 with a box
    float lim_hi[4];
    float misc[4];     // x drift, yz the depth texture's offset (pixels), w the seed
    float misc2[4];    // x interiors, y outdoors only, z the smallest size (pixels), w the splashes' share
    float int_lo[4][4];
    float int_hi[4][4];
};
static_assert(sizeof(Ubo) == 2 * 64 + 13 * 16 + 8 * 16, "PartUbo's std140 layout");

// ---- the shaders (OpenGL; the Vulkan ones are the same bodies in tools/gen_vk_shaders.py, MRP_PART_*: keep them in step) ----
#define RAC1_PART_UBO                                                                                                       \
    "layout(std140, binding = 7) uniform PartUbo {\n"                                                                      \
    "    mat4 uVP; mat4 uInvVP; vec4 uCam; vec4 uBox; vec4 uVel; vec4 uCol; vec4 uSize; vec4 uCamVel; vec4 uSun;\n"        \
    "    vec4 uVp; vec4 uProj; vec4 uLimLo; vec4 uLimHi; vec4 uMisc; vec4 uMisc2; vec4 uIntLo[4]; vec4 uIntHi[4];\n"         \
    "};\n"                                                                                                                 \
    "layout(binding = 29) uniform sampler2D uDepth;\n"
const char* kPartVs = "#version 460 core\n" RAC1_PART_UBO R"(
#define VID gl_VertexID
out vec2 vQ;
out float vA;
out float vZ;
uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) {
    s = pcg(s);
    return float(s) * (1.0 / 4294967296.0);
}
void main() {
    int id = VID;
    int pi = id / 6, c = id - pi * 6;
    vec2 q = vec2((c == 1 || c == 2 || c == 4) ? 1.0 : -1.0, (c == 2 || c == 4 || c == 5) ? 1.0 : -1.0);
    vQ = q;
    vA = 0.0;
    vZ = 1.0;
    gl_Position = vec4(2.0, 2.0, 2.0, 1.0);  // (culled)
    int kind = int(uVel.w + 0.5);
    uint s = uint(pi) * 9781u + uint(uMisc.w);
    vec3 r0 = vec3(rnd(s), rnd(s), rnd(s));
    float r3 = rnd(s), r4 = rnd(s), r5 = rnd(s);
    float t = uCam.w;
    vec3 vel = uVel.xyz * (0.75 + 0.5 * r3);
    float ph = r4 * 6.2832;
    vec3 drift = uMisc.x * vec3(sin(t * (0.31 + 0.4 * r5) + ph), cos(t * (0.27 + 0.35 * r3) + ph * 1.7), 0.5 * sin(t * (0.23 + 0.3 * r4) + ph * 2.3));
    // the particle's place in the world, wrapped into the volume around the camera
    vec3 rel = r0 * uBox.xyz + vel * t + drift - uCam.xyz;
    rel = (fract(rel / uBox.xyz) - 0.5) * uBox.xyz;
    vec3 wp = uCam.xyz + rel;
    vec3 e = abs(rel) / (0.5 * uBox.xyz);
    float a = uCol.a * smoothstep(1.0, 0.75, max(max(e.x, e.y), e.z));  // (faded toward the volume's faces: no popping where it wraps)
    if (uLimLo.w > 0.5) {  // the line's box
        vec3 d = min(wp - uLimLo.xyz, uLimHi.xyz - wp);
        a *= clamp(min(min(d.x, d.y), d.z), 0.0, 1.0);
    }
    if (uMisc2.y > 0.5)  // (from the sky: none inside the level's interiors)
        for (int i = 0; i < int(uMisc2.x); ++i)
            if (all(greaterThan(wp, uIntLo[i].xyz)) && all(lessThan(wp, uIntHi[i].xyz))) a = 0.0;
    if (kind == 2) {  // embers: a life (rising, fading), flickering
        float life = fract(t * (0.18 + 0.2 * r5) + r4);
        a *= sin(life * 3.14159) * (0.6 + 0.4 * sin(t * (9.0 + 6.0 * r3) + ph));
    } else if (kind == 3) {  // fireflies: blinking
        a *= smoothstep(0.35, 1.0, 0.5 + 0.5 * sin(t * (1.1 + 1.4 * r5) + ph * 3.0));
    } else if (kind == 4) {  // motes: bright looking toward the sun (forward scatter), twinkling
        vec3 vd = normalize(rel);
        float sc = pow(max(dot(vd, uSun.xyz), 0.0), 3.0);
        a *= mix(1.0, 0.6 + 2.4 * sc, uSun.w) * (0.7 + 0.3 * sin(t * (2.0 + 3.0 * r5) + ph));
    }
    if (a <= 0.003) return;
    vec4 c0 = uVP * vec4(wp, 1.0);
    if (c0.w < 0.3) return;
    a *= smoothstep(0.4, kind == 5 ? 1.0 : 2.0, c0.w);  // (none right at the lens: a speck there would be a blot)
    float size = uSize.x * (1.0 + uSize.y * (r5 * 2.0 - 1.0));
    float half_px = size * uProj.z * uVp.z * 0.5 / c0.w;  // (half the size in pixels)
    float k = max(uMisc2.z * 0.5 / max(half_px, 1e-4), 1.0);  // (at least the smallest size: its alpha lowered instead)
    if (kind == 5) {  // rain: a streak along its motion as the camera sees it (the camera's own taken off)
        vec3 v = vel - uCamVel.xyz;
        vec4 c1 = uVP * vec4(wp - v * uSize.z, 1.0);
        if (c1.w < 0.3) return;
        vec2 px = uVp.zw * 0.5;
        vec2 dir = c1.xy / c1.w * px - c0.xy / c0.w * px;
        float len = length(dir);
        dir = len > 1e-3 ? dir / len : vec2(0.0, 1.0);
        vec2 nrm = vec2(-dir.y, dir.x);
        float along = q.y * 0.5 + 0.5;
        vec4 cc = mix(c0, c1, along);
        gl_Position = vec4(cc.xy + nrm * (q.x * half_px * k) / px * cc.w, cc.zw);
        vZ = cc.w;
        a /= k;
        a *= clamp(len / 3.0, 0.25, 1.0) * 1.0;  // (a short streak - seen along its fall - lighter)
    } else {
        gl_Position = vec4(c0.xy + q * vec2(uProj.z, uProj.w) * size * k, c0.zw);
        vZ = c0.w;
        a /= k;  // (far: drawn at the smallest size, fainter - between the area's k * k and nothing, so that they still read)
    }
    vA = a;
}
)";
const char* kPartFs = "#version 460 core\n" RAC1_PART_UBO R"(
in vec2 vQ;
in float vA;
in float vZ;
out vec4 FragColor;
void main() {
    int kind = int(uVel.w + 0.5);
    float r2 = dot(vQ, vQ);
    float shape;
    if (kind == 5) {
        shape = (1.0 - abs(vQ.x)) * smoothstep(-1.0, -0.4, vQ.y) * (0.45 + 0.55 * (vQ.y * 0.5 + 0.5));  // (a streak: its head brighter)
    } else {
        if (r2 >= 1.0) discard;
        shape = (kind == 2 || kind == 3 || kind == 4) ? exp(-r2 * 3.5) : smoothstep(1.0, 0.25, r2);  // (glows; soft discs)
    }
    float a = vA * shape;
    // soft against the scene: faded in front of the surface behind it, hidden behind it
    float d = texelFetch(uDepth, ivec2(gl_FragCoord.xy - uMisc.yz), 0).r;
    float zs = uProj.y / ((d * 2.0 - 1.0) + uProj.x);
    a *= clamp((zs - vZ) / uSize.w, 0.0, 1.0);
    if (a <= 0.002) discard;
    FragColor = vec4(uCol.rgb, min(a, 1.0));
}
)";
// rain's splashes: over the scene's upward surfaces near the camera, a ring now and then in each cell of a grid on the ground
const char* kSplashVs = "#version 460 core\n" R"(
void main() {
    vec2 p = vec2((gl_VertexID << 1) & 2, gl_VertexID & 2);
    gl_Position = vec4(p * 2.0 - 1.0, 0.0, 1.0);
}
)";
const char* kSplashFs = "#version 460 core\n" RAC1_PART_UBO R"(
out vec4 FragColor;
uint pcg(uint v) {
    uint s = v * 747796405u + 2891336453u;
    uint w = ((s >> ((s >> 28u) + 4u)) ^ s) * 277803737u;
    return (w >> 22u) ^ w;
}
float rnd(inout uint s) {
    s = pcg(s);
    return float(s) * (1.0 / 4294967296.0);
}
void main() {
    float d = texelFetch(uDepth, ivec2(gl_FragCoord.xy - uMisc.yz), 0).r;
    vec2 ndc = (gl_FragCoord.xy - uVp.xy) / uVp.zw * 2.0 - 1.0;
    vec4 w = uInvVP * vec4(ndc, d * 2.0 - 1.0, 1.0);
    vec3 p = w.xyz / w.w;
    vec3 n = cross(dFdx(p), dFdy(p));
    float nl = length(n);
    if (d >= 0.99999 || nl <= 0.0) discard;
    if (dot(n, uCam.xyz - p) < 0.0) n = -n;  // (the side the camera sees: a roof's underside is no ground)
    float up = n.z / nl;
    float dist = length(p - uCam.xyz);
    if (up < 0.85 || dist > uSize.w) discard;
    if (uLimLo.w > 0.5 && (any(lessThan(p, uLimLo.xyz)) || any(greaterThan(p, uLimHi.xyz)))) discard;
    for (int i = 0; i < int(uMisc2.x); ++i)
        if (all(greaterThan(p, uIntLo[i].xyz - vec3(0.0, 0.0, 0.5))) && all(lessThan(p, uIntHi[i].xyz))) discard;
    const float cell = 0.55;
    vec2 gi = floor(p.xy / cell);
    uint s = uint(int(gi.x) * 73856093) ^ uint(int(gi.y) * 19349663);
    float rate = 1.4 + rnd(s) * 1.2;
    float tt = uCam.w * rate + rnd(s);
    float cyc = floor(tt), ph = fract(tt);
    uint s2 = s ^ (uint(int(cyc)) * 83492791u);
    if (rnd(s2) > uMisc2.w) discard;
    vec2 ctr = (gi + 0.25 + 0.5 * vec2(rnd(s2), rnd(s2))) * cell;
    float r = length(p.xy - ctr), rad = 0.02 + ph * 0.12;
    float ring = smoothstep(0.014, 0.0, abs(r - rad)) * (1.0 - ph) + smoothstep(0.025, 0.0, r) * max(1.0 - ph * 5.0, 0.0);
    float a = ring * uCol.a * smoothstep(uSize.w, uSize.w * 0.5, dist) * up;
    if (a <= 0.003) discard;
    FragColor = vec4(uCol.rgb, min(a, 1.0));
}
)";

// ---- the state ---------------------------------------------------------------------------------------------------------------
struct Gl {
    bool tried = false, ok = false;
    GLuint prog = 0, splash = 0, vao = 0, ubo = 0;
    GLuint dfbo = 0, dtex = 0;
    int dw = 0, dh = 0;
    GLenum dfmt = 0;
    GLint last_fb = -1;
} g_gl;
struct Vk {
    bool tried = false, ok = false;
    VkDescriptorSetLayout dsl = VK_NULL_HANDLE;
    VkPipelineLayout layout = VK_NULL_HANDLE;
    VkShaderModule vs = VK_NULL_HANDLE, fs = VK_NULL_HANDLE, svs = VK_NULL_HANDLE, sfs = VK_NULL_HANDLE;
    VkFormat fmt = VK_FORMAT_UNDEFINED;
    VkPipeline blend = VK_NULL_HANDLE, add = VK_NULL_HANDLE, splash = VK_NULL_HANDLE;
} g_vk;

struct Frame {
    uint64_t game_frame = ~0ull;
    glm::vec3 cam{0.0f}, cam_vel{0.0f};
    int drawn = 0;  // particles drawn last frame
} g_fr;
struct Stats {
    double cpu_acc = 0.0;
    int n = 0;
    float cpu_ms = 0.0f;
    int count = 0;
} g_st;
// the game's rain / snow replaced this frame (set by the draw: the lines' kinds and "keep")
bool g_hide_game = false;

// orig: (host) a GL program from two sources (0 on an error: printed)
GLuint gl_link(const char* vs, const char* fs) {
    auto compile = [](GLenum type, const char* src) {
        const GLuint s = glCreateShader(type);
        glShaderSource(s, 1, &src, nullptr);
        glCompileShader(s);
        GLint ok = 0;
        glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
        if (!ok) {
            char log[2048];
            glGetShaderInfoLog(s, sizeof log, nullptr, log);
            std::fprintf(stderr, "[particles] shader error: %s\n", log);
            glDeleteShader(s);
            return 0u;
        }
        return s;
    };
    const GLuint v = compile(GL_VERTEX_SHADER, vs), f = compile(GL_FRAGMENT_SHADER, fs);
    if (v == 0 || f == 0) return 0;
    const GLuint p = glCreateProgram();
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetProgramInfoLog(p, sizeof log, nullptr, log);
        std::fprintf(stderr, "[particles] link error: %s\n", log);
        glDeleteProgram(p);
        return 0;
    }
    return p;
}
// orig: (host) the GL objects, made once
bool gl_init() {
    if (g_gl.tried) return g_gl.ok;
    g_gl.tried = true;
    g_gl.prog = gl_link(kPartVs, kPartFs);
    g_gl.splash = gl_link(kSplashVs, kSplashFs);
    if (g_gl.prog == 0 || g_gl.splash == 0) return false;
    glGenVertexArrays(1, &g_gl.vao);
    glGenBuffers(1, &g_gl.ubo);
    glBindBuffer(GL_UNIFORM_BUFFER, g_gl.ubo);
    glBufferData(GL_UNIFORM_BUFFER, sizeof(Ubo), nullptr, GL_STREAM_DRAW);
    glBindBuffer(GL_UNIFORM_BUFFER, 0);
    glGenFramebuffers(1, &g_gl.dfbo);
    g_gl.ok = true;
    return true;
}
// orig: (host) the scene's depth over the viewport copied into our texture (the framebuffer drawn into keeps its own; GL cannot
// sample a buffer it draws to); false: not possible
bool gl_copy_depth(GLint fb, const int vp[4]) {
    if (fb != g_gl.last_fb || g_gl.dfmt == 0) {  // the depth format of the framebuffer drawn into (the blit wants the same)
        GLint dbits = 24, sbits = 0, dtype = GL_UNSIGNED_NORMALIZED;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fb));
        const GLenum datt = fb == 0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT, satt = fb == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &dbits);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &dtype);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, satt, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &sbits);
        while (glGetError() != GL_NO_ERROR) {}
        const GLenum fmt = dtype == GL_FLOAT ? (sbits > 0 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F)
                                             : (sbits > 0 ? GL_DEPTH24_STENCIL8 : (dbits >= 32 ? GL_DEPTH_COMPONENT32 : dbits >= 24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16));
        g_gl.last_fb = fb;
        if (fmt != g_gl.dfmt) {
            g_gl.dfmt = fmt;
            g_gl.dw = 0;
        }
    }
    if (g_gl.dw != vp[2] || g_gl.dh != vp[3]) {
        g_gl.dw = vp[2];
        g_gl.dh = vp[3];
        if (g_gl.dtex != 0) glDeleteTextures(1, &g_gl.dtex);
        glGenTextures(1, &g_gl.dtex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_gl.dtex, "Ambient particles: depth");
        glBindTexture(GL_TEXTURE_2D, g_gl.dtex);
        const bool st = g_gl.dfmt == GL_DEPTH24_STENCIL8 || g_gl.dfmt == GL_DEPTH32F_STENCIL8;
        const GLenum ext = st ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT;
        const GLenum type = g_gl.dfmt == GL_DEPTH24_STENCIL8 ? GL_UNSIGNED_INT_24_8 : g_gl.dfmt == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV
                                                                                         : g_gl.dfmt == GL_DEPTH_COMPONENT32F ? GL_FLOAT : GL_UNSIGNED_INT;
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(g_gl.dfmt), vp[2], vp[3], 0, ext, type, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_COMPARE_MODE, GL_NONE);
        if (st) glTexParameteri(GL_TEXTURE_2D, 0x90ea /* GL_DEPTH_STENCIL_TEXTURE_MODE */, GL_DEPTH_COMPONENT);
        glBindFramebuffer(GL_FRAMEBUFFER, g_gl.dfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_gl.dtex, 0);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fb));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_gl.dfbo);
    const bool st = g_gl.dfmt == GL_DEPTH24_STENCIL8 || g_gl.dfmt == GL_DEPTH32F_STENCIL8;
    glBlitFramebuffer(vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], 0, 0, vp[2], vp[3], st ? (GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT) : GL_DEPTH_BUFFER_BIT,
                      GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
    return glGetError() == GL_NO_ERROR;
}

// orig: (host) the Vulkan objects (the pipelines for the target's colour format), made once
bool vk_init() {
    const VkFormat fmt = vk::active_color_image().format != VK_FORMAT_UNDEFINED ? vk::active_color_image().format : VK_FORMAT_R8G8B8A8_UNORM;
    if (g_vk.tried && g_vk.fmt == fmt) return g_vk.ok;
    VkDevice dev = vk::device();
    if (!g_vk.tried) {
        g_vk.tried = true;
        if (!vk::shaders::kAvailable) return false;
        VkDescriptorSetLayoutBinding b[2]{};
        b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dci.bindingCount = 2;
        dci.pBindings = b;
        vkCreateDescriptorSetLayout(dev, &dci, nullptr, &g_vk.dsl);
        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &g_vk.dsl;
        vkCreatePipelineLayout(dev, &plci, nullptr, &g_vk.layout);
        g_vk.vs = vk::create_shader_module(vk::shaders::kMrpPartVs, sizeof(vk::shaders::kMrpPartVs));
        g_vk.fs = vk::create_shader_module(vk::shaders::kMrpPartFs, sizeof(vk::shaders::kMrpPartFs));
        g_vk.svs = vk::create_shader_module(vk::shaders::kMrpPartSplashVs, sizeof(vk::shaders::kMrpPartSplashVs));
        g_vk.sfs = vk::create_shader_module(vk::shaders::kMrpPartSplashFs, sizeof(vk::shaders::kMrpPartSplashFs));
    }
    g_vk.fmt = fmt;
    for (VkPipeline* p : {&g_vk.blend, &g_vk.add, &g_vk.splash})
        if (*p != VK_NULL_HANDLE) {
            vk::wait_idle();
            vkDestroyPipeline(dev, *p, nullptr);
            *p = VK_NULL_HANDLE;
        }
    auto make = [&](VkShaderModule vs, VkShaderModule fs, bool additive) {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk.layout;
        d.vs = vs;
        d.fs = fs;
        d.depth_test = false;  // (the scene's depth sampled: the soft test in the shader)
        d.depth_write = false;
        d.blend_enable = true;
        d.src_color_blend = VK_BLEND_FACTOR_SRC_ALPHA;
        d.dst_color_blend = additive ? VK_BLEND_FACTOR_ONE : VK_BLEND_FACTOR_ONE_MINUS_SRC_ALPHA;
        d.src_alpha_blend = VK_BLEND_FACTOR_ZERO;
        d.dst_alpha_blend = VK_BLEND_FACTOR_ONE;
        d.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
        d.color_format = fmt;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        return vk::create_graphics_pipeline(d);
    };
    g_vk.blend = make(g_vk.vs, g_vk.fs, false);
    g_vk.add = make(g_vk.vs, g_vk.fs, true);
    g_vk.splash = make(g_vk.svs, g_vk.sfs, true);
    g_vk.ok = g_vk.blend != VK_NULL_HANDLE && g_vk.add != VK_NULL_HANDLE && g_vk.splash != VK_NULL_HANDLE;
    return g_vk.ok;
}

// One draw of the frame: its uniforms, its pipeline, its vertex count
struct Draw {
    Ubo u;
    bool additive = false, splash = false;
    int verts = 0;
};

// orig: (host) the frame's draws from the level's lines (false: none)
bool make_draws(std::vector<Draw>& out, const int vp[4], uint64_t game_frame) {
    const std::vector<ParticleSpec>& lines = mrp_user_particles();
    if (lines.empty()) return false;
    float vpf[16];
    mrp_world_view_proj(vpf);
    const glm::mat4 VP = glm::make_mat4(vpf);
    const glm::mat4 view = render_view_matrix();
    const glm::mat4 iv = glm::inverse(view);
    const glm::vec3 cam(iv[3]);
    const glm::mat4 P = VP * iv;  // (the projection alone)
    // the camera's velocity per game frame (rain's streaks): a cut or a jump forgotten
    if (game_frame != g_fr.game_frame) {
        const glm::vec3 v = (cam - g_fr.cam) * 60.0f;
        g_fr.cam_vel = g_fr.game_frame + 1 == game_frame && glm::length(v) < 60.0f ? v : glm::vec3(0.0f);
        g_fr.cam = cam;
        g_fr.game_frame = game_frame;
    }
    const float t = static_cast<float>(std::fmod(static_cast<double>(game_frame) / 60.0, kTimePeriod));
    float light[3], sun[3];
    mrp_lighting_zone_light(light, sun);
    // the zone's brightness: the sum of its two lights (the game's colours, about 0.5 a light in daylight)
    const glm::vec3 L(light[0], light[1], light[2]);
    const glm::vec3 lit0 = glm::clamp(glm::vec3(0.22f) + L * 0.85f, glm::vec3(0.12f), glm::vec3(1.25f));
    const float lum = (lit0.r + lit0.g + lit0.b) / 3.0f;
    const glm::vec3 lit = glm::mix(glm::vec3(lum), lit0, 0.35f);  // (the zone's tint, softened: white rain under a yellow light stays rain)
    // the interiors nearest the camera (at most the shaders' 4)
    std::vector<std::pair<float, const SunInterior*>> near;
    for (const SunInterior& b : mrp_user_interiors()) {
        glm::vec3 d(0.0f);
        for (int i = 0; i < 3; ++i) d[i] = std::max({b.lo[i] - cam[i], 0.0f, cam[i] - b.hi[i]});
        near.emplace_back(glm::length(d), &b);
    }
    std::sort(near.begin(), near.end(), [](const auto& a, const auto& b) { return a.first < b.first; });
    const int n_int = static_cast<int>(std::min<size_t>(near.size(), 4));
    const float q = g_ambient_particles >= 2 ? 1.0f : 0.5f;
    int seed = 0;
    for (const ParticleSpec& ps : lines) {
        ++seed;
        if (!ps.on || ps.kind < 0 || ps.kind >= kPartKinds || !(ps.density > 0.0f)) continue;
        const Kind& K = kKinds[ps.kind];
        const int count = std::clamp(static_cast<int>(static_cast<float>(K.count) * ps.density * q), 0, kMaxCount);
        if (count <= 0) continue;
        Draw d{};
        Ubo& u = d.u;
        std::memcpy(u.vp, vpf, sizeof u.vp);
        std::memcpy(u.inv_vp, glm::value_ptr(glm::inverse(VP)), sizeof u.inv_vp);
        u.cam[0] = cam.x;
        u.cam[1] = cam.y;
        u.cam[2] = cam.z;
        u.cam[3] = t;
        u.box[0] = K.box.x;
        u.box[1] = K.box.y;
        u.box[2] = K.box.z;
        u.box[3] = static_cast<float>(count);
        // the wind: the line's (a gust now and then); the fall not blown, the wind's share carries the rest
        const float gust = 1.0f + 0.25f * std::sin(t * 0.37f) * std::sin(t * 0.13f + 1.0f);
        u.vel[0] = ps.wind[0] * gust;
        u.vel[1] = ps.wind[1] * gust;
        u.vel[2] = K.fall;
        u.vel[3] = static_cast<float>(ps.kind);
        const glm::vec3 base = ps.has_col ? glm::vec3(ps.col[0], ps.col[1], ps.col[2]) : K.col;
        glm::vec3 c = K.emissive ? base : base * lit;
        float alpha = K.alpha;
        if (ps.kind == kPartFireflies) alpha *= std::clamp(1.35f - lum, 0.25f, 1.0f);  // (in the dark: brighter)
        if (ps.kind == kPartMotes) c = base * glm::clamp(lit * 1.1f, glm::vec3(0.3f), glm::vec3(1.2f));
        u.col[0] = c.r;
        u.col[1] = c.g;
        u.col[2] = c.b;
        u.col[3] = alpha;
        u.size[0] = K.size;
        u.size[1] = K.spread;
        u.size[2] = K.streak;
        u.size[3] = K.soft;
        u.cam_vel[0] = g_fr.cam_vel.x;
        u.cam_vel[1] = g_fr.cam_vel.y;
        u.cam_vel[2] = g_fr.cam_vel.z;
        u.sun[0] = sun[0];
        u.sun[1] = sun[1];
        u.sun[2] = sun[2];
        u.sun[3] = 1.0f;
        for (int i = 0; i < 4; ++i) u.vpr[i] = static_cast<float>(vp[i]);
        u.proj[0] = P[2][2];
        u.proj[1] = P[3][2];
        u.proj[2] = P[0][0];
        u.proj[3] = P[1][1];
        if (ps.has_box) {
            for (int i = 0; i < 3; ++i) {
                u.lim_lo[i] = ps.lo[i];
                u.lim_hi[i] = ps.hi[i];
            }
            u.lim_lo[3] = 1.0f;
        }
        u.misc[0] = K.drift;
        u.misc[1] = vk::is_active() ? 0.0f : static_cast<float>(vp[0]);  // (Vulkan samples the scene's own depth; GL our copy of the view's)
        u.misc[2] = vk::is_active() ? 0.0f : static_cast<float>(vp[1]);
        u.misc[3] = static_cast<float>(seed * 7919);
        u.misc2[0] = static_cast<float>(n_int);
        u.misc2[1] = K.outdoors ? 1.0f : 0.0f;
        u.misc2[2] = K.min_px;
        u.misc2[3] = 0.0f;
        for (int i = 0; i < n_int; ++i)
            for (int k = 0; k < 3; ++k) {
                u.int_lo[i][k] = near[static_cast<size_t>(i)].second->lo[k];
                u.int_hi[i][k] = near[static_cast<size_t>(i)].second->hi[k];
            }
        d.additive = K.additive;
        d.verts = count * 6;
        out.push_back(d);
        if (ps.kind == kPartRain) {  // its splashes on the ground
            Draw s = d;
            s.splash = true;
            s.additive = true;
            s.verts = 3;
            s.u.col[0] = 0.75f * lit.r;
            s.u.col[1] = 0.8f * lit.g;
            s.u.col[2] = 0.9f * lit.b;
            s.u.col[3] = 0.45f;
            s.u.size[3] = kSplashRange;
            s.u.misc2[3] = std::clamp(0.35f * ps.density * q * 2.0f, 0.0f, 1.0f);  // (the cells splashing in a cycle)
            out.push_back(s);
        }
    }
    return !out.empty();
}

}  // namespace

// orig: (host) see mrp_particles.h
void mrp_particles_draw(uint64_t game_frame) {
    g_hide_game = false;
    g_fr.drawn = 0;
    if (g_ambient_particles <= 0) return;
    int vp[4];
    if (!mrp_world_active() || !mrp_world_last_viewport(vp)) return;
    const auto t0 = std::chrono::steady_clock::now();
    mrp_user_lights_ensure();
    for (const ParticleSpec& ps : mrp_user_particles())
        if (ps.on && !ps.keep && (ps.kind == kPartRain || ps.kind == kPartSnow) && ps.density > 0.0f) g_hide_game = g_particles_show_game == 0;
    std::vector<Draw> draws;
    if (!make_draws(draws, vp, game_frame)) return;
    RAC1_PROF_ZONE(kParticles);
    int total = 0;
    if (vk::is_active()) {
        if (!vk_init()) return;
        int prev_vp[4] = {};
        vk::get_viewport(prev_vp);
        vk::GpuImage& depth = vk::active_depth_image();
        vk::GpuImage& color = vk::active_color_image();
        if (depth.image == VK_NULL_HANDLE || color.image == VK_NULL_HANDLE) return;
        vk::transition_image(vk::cmd(), depth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vk::RenderTarget rt{};
        rt.color_view = color.view;
        rt.color_format = color.format;
        rt.width = color.width;
        rt.height = color.height;
        vk::push_target(rt);
        vk::set_viewport(vp[0], vp[1], vp[2], vp[3]);
        vk::set_scissor(false);
        vk::ensure_rendering();
        const VkSampler samp = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
        VkCommandBuffer cb = vk::cmd();
        for (const Draw& d : draws) {
            const vk::RingAlloc ra = vk::alloc_ring(sizeof(Ubo), 256);
            std::memcpy(ra.ptr, &d.u, sizeof(Ubo));
            VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk.dsl);
            VkDescriptorBufferInfo dbi{ra.buffer, ra.offset, sizeof(Ubo)};
            VkDescriptorImageInfo dii{samp, depth.depth_view != VK_NULL_HANDLE ? depth.depth_view : depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[2]{};
            w[0].sType = w[1].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[0].dstSet = w[1].dstSet = ds;
            w[0].dstBinding = 0;
            w[0].descriptorCount = 1;
            w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[0].pBufferInfo = &dbi;
            w[1].dstBinding = 1;
            w[1].descriptorCount = 1;
            w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[1].pImageInfo = &dii;
            vkUpdateDescriptorSets(vk::device(), 2, w, 0, nullptr);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, d.splash ? g_vk.splash : d.additive ? g_vk.add : g_vk.blend);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk.layout, 0, 1, &ds, 0, nullptr);
            vkCmdDraw(cb, static_cast<uint32_t>(d.verts), 1, 0, 0);
            if (!d.splash) total += d.verts / 6;
        }
        vk::pop_target();
        vk::transition_image(vk::cmd(), depth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        vk::set_viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    } else {
        if (!gl_init()) return;
        GLint fb = 0, rfb = 0, prev_prog = 0, prev_vao = 0, prev_active = 0, prev_tex = 0, prev_ubo = 0;
        GLint bsrc = 0, bdst = 0, bsrca = 0, bdsta = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
        glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
        glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
        glGetIntegerv(GL_UNIFORM_BUFFER_BINDING, &prev_ubo);
        glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
        glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrca);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &bdsta);
        glActiveTexture(GL_TEXTURE0 + 29);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE);
        GLboolean dmask = GL_TRUE, cmask[4];
        glGetBooleanv(GL_DEPTH_WRITEMASK, &dmask);
        glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
        GLint prev_vp[4];
        glGetIntegerv(GL_VIEWPORT, prev_vp);
        if (gl_copy_depth(fb, vp)) {
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
            glViewport(vp[0], vp[1], vp[2], vp[3]);
            glDisable(GL_DEPTH_TEST);
            glDepthMask(GL_FALSE);
            glDisable(GL_SCISSOR_TEST);
            glDisable(GL_CULL_FACE);
            glEnable(GL_BLEND);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glActiveTexture(GL_TEXTURE0 + 29);
            glBindTexture(GL_TEXTURE_2D, g_gl.dtex);
            glBindVertexArray(g_gl.vao);
            glBindBufferBase(GL_UNIFORM_BUFFER, 7, g_gl.ubo);
            for (const Draw& d : draws) {
                glBindBuffer(GL_UNIFORM_BUFFER, g_gl.ubo);
                glBufferData(GL_UNIFORM_BUFFER, sizeof(Ubo), &d.u, GL_STREAM_DRAW);
                glUseProgram(d.splash ? g_gl.splash : g_gl.prog);
                glBlendFuncSeparate(GL_SRC_ALPHA, d.additive ? GL_ONE : GL_ONE_MINUS_SRC_ALPHA, GL_ZERO, GL_ONE);
                glDrawArrays(GL_TRIANGLES, 0, d.verts);
                if (!d.splash) total += d.verts / 6;
            }
            glBindBufferBase(GL_UNIFORM_BUFFER, 7, 0);
        }
        glBindBuffer(GL_UNIFORM_BUFFER, static_cast<GLuint>(prev_ubo));
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
        glActiveTexture(static_cast<GLenum>(prev_active));
        glBlendFuncSeparate(static_cast<GLenum>(bsrc), static_cast<GLenum>(bdst), static_cast<GLenum>(bsrca), static_cast<GLenum>(bdsta));
        if (blend) glEnable(GL_BLEND);
        else glDisable(GL_BLEND);
        if (depth) glEnable(GL_DEPTH_TEST);
        if (scissor) glEnable(GL_SCISSOR_TEST);
        if (cull) glEnable(GL_CULL_FACE);
        glDepthMask(dmask);
        glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
        glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
        glUseProgram(static_cast<GLuint>(prev_prog));
        glBindVertexArray(static_cast<GLuint>(prev_vao));
    }
    g_fr.drawn = total;
    g_st.cpu_acc += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
    if (++g_st.n >= 60) {
        g_st.cpu_ms = static_cast<float>(g_st.cpu_acc / g_st.n);
        g_st.cpu_acc = 0.0;
        g_st.n = 0;
    }
    g_st.count = total;
    static const bool trace = std::getenv("RAC1_PARTICLES_TRACE") != nullptr;  // (testing: what is drawn, every 120 frames)
    if (trace && game_frame % 120 == 0)
        std::printf("[particles] frame %llu level %d: %zu draws, %d particles, light %.2f %.2f %.2f, the game's rain / snow %s\n",
                    static_cast<unsigned long long>(game_frame), mrp_user_lights_level(), draws.size(), total, draws[0].u.col[0], draws[0].u.col[1], draws[0].u.col[2],
                    g_hide_game ? "hidden" : "drawn");
}

namespace {
// orig: (host) whether the level's file replaces the game's own of that kind (rain / snow) now
bool mrp_particles_hide_game_kind(int kind) {
    if (g_ambient_particles <= 0 || g_particles_show_game != 0) return false;
    mrp_user_lights_ensure();
    for (const ParticleSpec& ps : mrp_user_particles())
        if (ps.on && !ps.keep && ps.kind == kind && ps.density > 0.0f) return true;
    return false;
}
}  // namespace

// orig: (host) see mrp_particles.h
bool mrp_particles_hide_game() { return mrp_particles_hide_game_kind(kPartRain) || mrp_particles_hide_game_kind(kPartSnow); }

namespace {
// the particle textures' GS addresses in the last run of the game's particle draw (particles_draw.cpp: the TEX0 each texture
// index got; -1 none), and the rain's, captured with the frame's chain (the drawing reads them)
int32_t g_tex_tbp[256];
bool g_tex_any = false;
std::vector<uint32_t> g_rain_tbp_cap, g_snow_tbp_cap;  // (the textures of the game's rain, of its snow)
}  // namespace

// orig: (host) see mrp_particles.h
void mrp_particles_note_begin() {
    if (!g_tex_any) return;
    std::fill(std::begin(g_tex_tbp), std::end(g_tex_tbp), -1);
    g_tex_any = false;
}
// orig: (host) see mrp_particles.h
void mrp_particles_note_tex(uint32_t tex, uint64_t tex0) {
    if (g_ambient_particles <= 0) return;
    if (!g_tex_any) std::fill(std::begin(g_tex_tbp), std::end(g_tex_tbp), -1);
    g_tex_any = true;
    g_tex_tbp[tex & 0xff] = static_cast<int32_t>(tex0 & 0x3fff);
}
// orig: (host) mrp_fx.cpp (mrp_fx_chain_ranges, the game's thread after its update): the textures of the live particles of the
// game's rain (types 0 and 1: the drops and their splashes, Batalia) and of its snow (type 0x49, Hoven) as the frame's particle
// draw placed them
void mrp_particles_fx_capture() {
    g_rain_tbp_cap.clear();
    g_snow_tbp_cap.clear();
    if (g_ambient_particles <= 0 || !g_tex_any) return;
    bool seen[256] = {};
    const bool rain_lvl = lw(0x0015ed84) != 12;  // (Hoven's type 1 particles are not rain: its snow alone there)
    const uint32_t pool = lwu(OV(0x0016016c));
    const int32_t hi = lw(OV(0x00160174));
    for (int32_t i = 0; pool != 0 && i <= hi && i < 4096; ++i) {
        const uint32_t r = pool + 0x40u * static_cast<uint32_t>(i);
        if (static_cast<int8_t>(lbu(r + 1)) < 0) continue;
        const uint32_t type = lbu(r);
        const bool rain = rain_lvl && (type == 0 || type == 1), snow = type == 0x49;
        if (!rain && !snow) continue;
        const uint32_t tex = lbu(r + 2);
        if (seen[tex] || g_tex_tbp[tex] < 0) continue;
        seen[tex] = true;
        (rain ? g_rain_tbp_cap : g_snow_tbp_cap).push_back(static_cast<uint32_t>(g_tex_tbp[tex]));
    }
}

// orig: (host) see mrp_particles.h
bool mrp_particles_hide_fx(uint8_t kind, uint64_t tex0, int prim_type) {
    static const bool trace = std::getenv("RAC1_PARTICLES_FXTRACE") != nullptr;  // (testing: the effects' primitives by texture, and the pool's types)
    if (trace) {
        static std::map<uint64_t, int> hist;
        static uint64_t calls = 0;
        hist[(static_cast<uint64_t>(kind) << 56) | (static_cast<uint64_t>(prim_type) << 48) | (tex0 & 0x3fffffffffull)]++;
        if (++calls % 20000 == 0) {
            std::printf("[particles-fx] %llu primitives:", static_cast<unsigned long long>(calls));
            for (const auto& [k, n] : hist)
                std::printf(" k%d t%d tbp %04x psm %02x tw %d th %d: %d;", static_cast<int>(k >> 56), static_cast<int>((k >> 48) & 0xff), static_cast<unsigned>(k & 0x3fff),
                            static_cast<unsigned>((k >> 20) & 0x3f), static_cast<int>((k >> 26) & 0xf), static_cast<int>((k >> 30) & 0xf), n);
            std::printf("\n");
            std::map<int, int> types;  // the particle pool's live records by type
            const uint32_t pool = lwu(OV(0x0016016c));
            const int32_t hi = lw(OV(0x00160174));
            for (int32_t i = 0; pool != 0 && i <= hi && i < 4096; ++i) {
                const uint32_t r = pool + 0x40u * static_cast<uint32_t>(i);
                if (static_cast<int8_t>(lbu(r + 1)) < 0) continue;
                types[lbu(r) | (lbu(r + 2) << 8) | ((lbu(r + 1) & 3) << 16)]++;
            }
            std::printf("[particles-fx] SPR TEX0 tbp by texture:");
            for (uint32_t t = 0; t < 16; ++t) std::printf(" %u:%04x", t, lwu(0x70003000u + t * 8) & 0x3fff);
            std::printf(" | table:");
            for (uint32_t t = 0; t < 16; ++t) std::printf(" %u:%08x", t, lwu(OV(0x001b1a80u) + t * 8));
            std::printf("\n");
            std::printf("[particles-fx] pool (type / texture / shape):");
            for (const auto& [k, n] : types) std::printf(" %02x/%02x/%d: %d;", k & 0xff, (k >> 8) & 0xff, k >> 16, n);
            std::printf("\n");
            hist.clear();
        }
    }
    (void)prim_type;
    if (kind != kMrpFxParticles || (g_rain_tbp_cap.empty() && g_snow_tbp_cap.empty())) return false;
    const uint32_t tbp = static_cast<uint32_t>(tex0 & 0x3fff);
    auto in = [tbp](const std::vector<uint32_t>& v) { return std::find(v.begin(), v.end(), tbp) != v.end(); };
    return (in(g_rain_tbp_cap) && mrp_particles_hide_game_kind(kPartRain)) || (in(g_snow_tbp_cap) && mrp_particles_hide_game_kind(kPartSnow));
}

// orig: (host) see mrp_particles.h
void mrp_particles_stats(int* count, float* cpu_ms, float* gpu_ms) {
    *count = g_st.count;
    *cpu_ms = g_st.cpu_ms;
    *gpu_ms = -1.0f;  // (the Profiler tab's "Ambient particles" stage)
}

}  // namespace rac1
