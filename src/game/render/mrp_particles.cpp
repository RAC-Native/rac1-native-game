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
#include <atomic>
#include <chrono>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <memory>
#include <mutex>
#include <thread>
#include <unordered_map>
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
int g_particles_sky = env_int("RAC1_PARTICLES_SKY", 1) != 0 ? 1 : 0;

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
    bool outdoors;        // what falls from the sky: not drawn under a roof (the sky grid) nor inside the level's interior boxes
    float streak;         // rain: the streak's length in seconds of its motion
    float min_px;         // the smallest drawn size, pixels (smaller: the alpha lowered instead)
    float soft;           // the distance over which a particle fades into the surface behind it (units)
    float wander;         // the drift's pace (1 the usual; less: a calmer drift)
    float lava_rise;      // from the lava ("source lava"): the rise, units a second
};
constexpr int kMaxCount = 24000;
const Kind kKinds[kPartKinds] = {
    // count  box                      fall    drift size   spread colour                      alpha add    emit   out    streak  min_px soft  wander rise
    {2600, {30.0f, 30.0f, 16.0f}, -0.04f, 0.70f, 0.022f, 0.50f, {0.86f, 0.80f, 0.68f}, 0.70f, false, false, false, 0.0f, 1.8f, 0.5f, 1.0f, 0.15f},   // dust
    {2400, {32.0f, 32.0f, 20.0f}, -0.45f, 0.90f, 0.030f, 0.50f, {0.58f, 0.56f, 0.54f}, 0.80f, false, false, true, 0.0f, 1.6f, 0.5f, 1.0f, 0.50f},    // ash
    {2000, {30.0f, 30.0f, 16.0f}, 1.60f, 0.80f, 0.040f, 0.40f, {1.00f, 0.42f, 0.10f}, 1.00f, true, true, false, 0.0f, 2.0f, 0.4f, 1.0f, 0.90f},      // embers
    {400, {32.0f, 32.0f, 8.0f}, 0.00f, 1.60f, 0.060f, 0.30f, {0.72f, 1.00f, 0.32f}, 1.00f, true, true, false, 0.0f, 2.5f, 0.4f, 1.0f, 0.10f},        // fireflies
    {3000, {24.0f, 24.0f, 12.0f}, -0.02f, 0.45f, 0.020f, 0.50f, {1.00f, 0.95f, 0.82f}, 0.90f, true, false, false, 0.0f, 2.0f, 0.4f, 1.0f, 0.10f},    // motes
    {9000, {32.0f, 32.0f, 22.0f}, -22.0f, 0.00f, 0.010f, 0.30f, {0.72f, 0.77f, 0.86f}, 0.30f, false, false, true, 0.028f, 1.1f, 0.6f, 1.0f, 0.0f},  // rain
    {9000, {30.0f, 30.0f, 20.0f}, -0.75f, 0.30f, 0.026f, 0.45f, {1.00f, 1.00f, 1.00f}, 0.85f, false, false, true, 0.0f, 1.2f, 0.5f, 0.45f, 0.0f},   // snow
};
// from the lava ("source lava"): the emitter slots the shaders read (uEmit), the air's currents (units a second), the slots' range
constexpr int kLavaSlots = 512;
constexpr float kLavaCurrent = 0.16f;
constexpr float kLavaRange = 60.0f;
constexpr float kLavaSpacing = 1.5f;  // (square units of lava an emitter point stands for)
constexpr double kTimePeriod = 1200.0;  // (the particles' time wraps every 20 minutes: the floats stay exact)
constexpr float kSplashRange = 18.0f;    // (rain's splashes: up to this far from the camera)
// the sky grid (below, "the sky"): its squares (units), its window's squares a side (128 units: the particles' volumes and the
// splashes well inside), how far the camera goes before the window is taken again, the height of "no face there"
constexpr float kSkyCell = 0.5f;
constexpr int kSkyWin = 256;         // (128 units at kSkyCell: the particles' volumes and the splashes well inside)
constexpr float kSkyMove = 24.0f;    // (the window taken again when the camera is this far from its centre)
constexpr float kSkyNone = -1.0e6f;  // (no face over that square: the open sky)
constexpr int kSkyUnit = 30;         // (OpenGL: its texture unit, the particles' and the materials' - mrp_materials_glsl.inc)

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
    float src[4];      // x 1 from the lava, z the distance it fades out at, w the spread around a point (units)
    float move[4];     // x the drift's pace, y the air currents' speed (from the lava)
    float sky[4];      // the sky grid's window: xy its corner, z its squares' size, w 1 when tested (what falls from the sky)
    float emit[kLavaSlots][4];  // from the lava: xyz a point, w the time its slot got it (< 0: none)
};
static_assert(sizeof(Ubo) == 2 * 64 + 16 * 16 + 8 * 16 + kLavaSlots * 16, "PartUbo's std140 layout");

// ---- the shaders (OpenGL; the Vulkan ones are the same bodies in tools/gen_vk_shaders.py, MRP_PART_*: keep them in step) ----
#define RAC1_PART_UBO                                                                                                       \
    "layout(std140, binding = 7) uniform PartUbo {\n"                                                                      \
    "    mat4 uVP; mat4 uInvVP; vec4 uCam; vec4 uBox; vec4 uVel; vec4 uCol; vec4 uSize; vec4 uCamVel; vec4 uSun;\n"        \
    "    vec4 uVp; vec4 uProj; vec4 uLimLo; vec4 uLimHi; vec4 uMisc; vec4 uMisc2; vec4 uIntLo[4]; vec4 uIntHi[4];\n"         \
    "    vec4 uSrc; vec4 uMove; vec4 uSky; vec4 uEmit[512];\n"                                                             \
    "};\n"                                                                                                                 \
    "layout(binding = 29) uniform sampler2D uDepth;\n"                                                                    \
    "layout(binding = 30) uniform sampler2D uSkyMap;\n"
// (the sky: 1 under the open sky, 0 under the topmost face of the level's collision at the place - a roof -, blended over the
// grid's squares; the same in tools/gen_vk_shaders.py)
#define RAC1_PART_SKY                                                                                                      \
    "float sky_open(vec3 p) {\n"                                                                                           \
    "    if (uSky.w < 0.5) return 1.0;\n"                                                                                  \
    "    vec2 g = (p.xy - uSky.xy) / uSky.z - 0.5;\n"                                                                      \
    "    ivec2 i0 = ivec2(floor(g));\n"                                                                                    \
    "    vec2 f = g - vec2(i0);\n"                                                                                         \
    "    ivec2 n = textureSize(uSkyMap, 0) - ivec2(0, 1);\n" /* (the last row: the window's place, for the materials) */      \
    "    vec4 o;\n"                                                                                                        \
    "    for (int k = 0; k < 4; ++k) {\n"                                                                                  \
    "        ivec2 c = i0 + ivec2(k & 1, k >> 1);\n"                                                                       \
    "        float h = (any(lessThan(c, ivec2(0))) || any(greaterThanEqual(c, n))) ? -1.0e6 : texelFetch(uSkyMap, c, 0).r;\n" \
    "        o[k] = smoothstep(h - 0.25, h + 0.25, p.z);\n"                                                                \
    "    }\n"                                                                                                              \
    "    return mix(mix(o.x, o.y, f.x), mix(o.z, o.w, f.x), f.y);\n"                                                       \
    "}\n"
const char* kPartVs = "#version 460 core\n" RAC1_PART_UBO RAC1_PART_SKY R"(
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
    float tw = t * uMove.x;  // (the drift's pace: slower for a calm kind)
    vec3 drift = uMisc.x * vec3(sin(tw * (0.31 + 0.4 * r5) + ph), cos(tw * (0.27 + 0.35 * r3) + ph * 1.7), 0.5 * sin(tw * (0.23 + 0.3 * r4) + ph * 2.3));
    vec3 rel, wp;
    float a;
    bool src = uSrc.x > 0.5;
    if (src) {
        // from the lava: each life starts at one of the lava's points near the camera (uEmit: xyz the point, w the time its slot got
        // it, < 0 none) and rises slowly, carried by the air's slow currents, fading as it goes
        float rate = 0.07 + 0.06 * r5;  // (a life of 8 to 14 s)
        float lt = t * rate + r4;
        float life = fract(lt);
        uint s2 = pcg(uint(pi) * 2654435761u + uint(int(floor(lt))) * 40503u + uint(uMisc.w));
        vec4 em = uEmit[min(int(rnd(s2) * 512.0), 511)];
        if (em.w > -1.5 && em.w < 0.0) return;  // (an empty slot)
        float ang = rnd(s2) * 6.2832, rad = sqrt(rnd(s2)) * uSrc.w;
        float age = life / rate;  // (seconds since it left the lava)
        vec2 cur = uMove.y * vec2(sin(em.y * 0.07 + t * 0.031 + 1.3) + 0.6 * sin(em.x * 0.05 - t * 0.021),
                                  cos(em.x * 0.06 - t * 0.026) + 0.6 * cos(em.y * 0.045 + t * 0.017));
        vec3 wander = uMisc.x * min(age * 0.25, 1.0) * vec3(sin(age * (0.35 + 0.3 * r5) + ph), cos(age * (0.3 + 0.3 * r3) + ph * 1.7), 0.0);
        wp = em.xyz + vec3(cos(ang) * rad, sin(ang) * rad, 0.05) + vec3((cur + uVel.xy) * age, uVel.z * (0.7 + 0.6 * r3) * age) + wander;
        rel = wp - uCam.xyz;
        float since = t - (em.w >= 0.0 ? em.w : -2.0 - em.w);  // (the slot's point taken - or let go - this long ago)
        float slot_a = since < 0.0 ? 1.0 : (em.w >= 0.0 ? clamp(since / 1.5, 0.0, 1.0) : clamp(1.0 - since / 1.5, 0.0, 1.0));
        a = uCol.a * smoothstep(0.0, 0.06, life) * (1.0 - smoothstep(0.5, 1.0, life)) * smoothstep(uSrc.z, uSrc.z * 0.75, length(rel)) *
            slot_a;  // (born bright, fading as it rises; faded far off, and as its slot takes or lets go of its point)
    } else {
        // the particle's place in the world, wrapped into the volume around the camera
        rel = r0 * uBox.xyz + vel * t + drift - uCam.xyz;
        rel = (fract(rel / uBox.xyz) - 0.5) * uBox.xyz;
        wp = uCam.xyz + rel;
        vec3 e = abs(rel) / (0.5 * uBox.xyz);
        a = uCol.a * smoothstep(1.0, 0.75, max(max(e.x, e.y), e.z));  // (faded toward the volume's faces: no popping where it wraps)
    }
    if (uLimLo.w > 0.5) {  // the line's box
        vec3 d = min(wp - uLimLo.xyz, uLimHi.xyz - wp);
        a *= clamp(min(min(d.x, d.y), d.z), 0.0, 1.0);
    }
    if (uMisc2.y > 0.5) {  // (from the sky: none under a roof, none inside the level's interiors)
        a *= sky_open(wp);
        for (int i = 0; i < int(uMisc2.x); ++i)
            if (all(greaterThan(wp, uIntLo[i].xyz)) && all(lessThan(wp, uIntHi[i].xyz))) a = 0.0;
    }
    if (kind == 2) {  // embers: a life (rising, fading), flickering
        float life = fract(t * (0.18 + 0.2 * r5) + r4);
        a *= (src ? 1.0 : sin(life * 3.14159)) * (0.6 + 0.4 * sin(t * (9.0 + 6.0 * r3) + ph));  // (from the lava: its own life above)
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
const char* kSplashFs = "#version 460 core\n" RAC1_PART_UBO RAC1_PART_SKY R"(
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
    float open = sky_open(p + vec3(0.0, 0.0, 0.75));  // (the ground the topmost face there: under the sky)
    if (open <= 0.0) discard;
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
    float a = ring * uCol.a * smoothstep(uSize.w, uSize.w * 0.5, dist) * up * open;
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
        VkDescriptorSetLayoutBinding b[3]{};
        b[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        b[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        b[2] = {2, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        VkDescriptorSetLayoutCreateInfo dci{};
        dci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dci.bindingCount = 3;
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

// ---- the lava ("source lava") -------------------------------------------------------------------------------------------------
// The level's lava: points on the faces of its collision the game kills the hero in (surface kind 1, the deadly liquid: the lava
// of Umbris and Gaspar; hero_surface_update's +0x12e5), their tops, where no floor is above them (the lava's kill plane runs under
// a level's hills and platforms), area-weighted, one per kLavaSpacing square units, with a fixed seed. Read once per level from
// the collision tree in EE memory (*(0x173e40): the game's own, read only), on the game's thread (mrp_particles_fx_capture); the
// drawing takes points near the camera into its slots.
struct LavaSet {
    int level = -1;
    uint32_t tree = 0;
    std::vector<glm::vec3> pts;   // (in the order of the columns below)
    std::vector<uint32_t> col;    // the points of each 16-unit column of the world (64 x 64, x + 64 y): [col[c], col[c + 1])
    int faces = 0;
    float area = 0.0f, spacing = kLavaSpacing;
};
std::mutex g_lava_mu;
std::shared_ptr<const LavaSet> g_lava;  // (the game's thread makes it, the drawing reads it)
std::atomic<uint32_t> g_lava_fp{0};  // (the collision index's fingerprint last read)
std::atomic<bool> g_lava_busy{false};  // (a reading under way)
struct LavaSlots {
    std::shared_ptr<const LavaSet> set;  // (the points the slots index)
    int idx[kLavaSlots];
    float since[kLavaSlots], gone[kLavaSlots];  // (the time the slot got its point; let it go, -1 not)
    float a = 40.0f;  // (the chance's scale: a / d^2)
} g_slots;

int g_kind_hist[32];  // (testing, RAC1_PARTICLES_TRACE: the faces by surface kind)

// orig: (host) every face of the collision tree at `tree` in EE memory (the grid of 4-unit cells: src/formats/level_collision.cpp),
// read only: fn(a, b, c, the surface kind, the cell (x, y, z), a quad's second triangle) for each triangle, a quad's two in turn
// (a, c and its fourth point); `face` once per face before its triangles (its kind). False: no tree there
template <class Face, class Tri>
bool coll_faces(uint32_t tree, Face&& face, Tri&& fn) {
    auto ok = [](uint32_t a, uint32_t n) { return a >= 0x100000u && a + n < 0x2000000u; };
    if (!ok(tree, 8)) return false;
    const uint32_t z0 = lhu(tree), zn = lhu(tree + 2);
    for (uint32_t zi = 0; zi < zn && zi < 256; ++zi) {
        const uint32_t zrel = static_cast<uint32_t>(lhu(tree + 4 + zi * 2)) << 2;
        if (zrel == 0 || !ok(tree + zrel, 4)) continue;
        const uint32_t yrow = tree + zrel, y0 = lhu(yrow), yn = lhu(yrow + 2);
        for (uint32_t yi = 0; yi < yn && yi < 256; ++yi) {
            const uint32_t yrel = lwu(yrow + 4 + yi * 4);
            if (yrel == 0 || !ok(tree + yrel, 4)) continue;
            const uint32_t xrow = tree + yrel, x0 = lhu(xrow), xn = lhu(xrow + 2);
            for (uint32_t xi = 0; xi < xn && xi < 256; ++xi) {
                const uint32_t cell = lwu(xrow + 4 + xi * 4) >> 8;
                if (cell == 0 || !ok(tree + cell, 4)) continue;
                const uint32_t c = tree + cell;
                const uint32_t nf = lbu(c), np = lbu(c + 2), nq = lbu(c + 3);  // (the faces a byte, as the game reads them)
                if (!ok(c, 4 + np * 4 + nf * 4 + nq)) continue;
                const glm::ivec3 ci(static_cast<int>(x0 + xi), static_cast<int>(y0 + yi), static_cast<int>(z0 + zi));
                const glm::vec3 org(glm::vec3(ci) * 4.0f + 2.0f);
                auto vtx = [&](uint32_t i) {
                    const uint32_t w = lwu(c + 4 + i * 4);
                    return org + glm::vec3(static_cast<float>(static_cast<int32_t>(w << 22) >> 22) / 16.0f, static_cast<float>(static_cast<int32_t>(w << 12) >> 22) / 16.0f,
                                           static_cast<float>(static_cast<int32_t>(w) >> 20) / 64.0f);
                };
                const uint32_t fb = c + 4 + np * 4;
                for (uint32_t f = 0; f < nf; ++f) {
                    const int kind = static_cast<int>(lbu(fb + f * 4 + 3) & 0x1f);
                    face(kind);
                    const uint32_t i0 = lbu(fb + f * 4), i1 = lbu(fb + f * 4 + 1), i2 = lbu(fb + f * 4 + 2);
                    if (i0 >= np || i1 >= np || i2 >= np) continue;
                    fn(vtx(i0), vtx(i1), vtx(i2), kind, ci, false);
                    if (f < nq) {
                        const uint32_t i3 = lbu(fb + nf * 4 + f);
                        if (i3 < np) fn(vtx(i0), vtx(i2), vtx(i3), kind, ci, true);
                    }
                }
            }
        }
    }
    return true;
}

// orig: (host) the level's lava points from the collision tree at `tree`
std::shared_ptr<LavaSet> lava_scan(int level, uint32_t tree) {
    std::fill(std::begin(g_kind_hist), std::end(g_kind_hist), 0);
    auto out = std::make_shared<LavaSet>();
    out->level = level;
    out->tree = tree;
    struct Tri {
        glm::vec3 a, b, c;
        float area;
        glm::vec2 lo;  // (its cell's corner: a face crossing several cells is in each, its points kept in this cell's 4 x 4 only)
    };
    std::vector<Tri> tris;
    int up_votes = 0;  // (the lava's faces' winding: which way a floor's normal points, to tell a floor from a ceiling)
    // the other faces by cell (x, y, z packed): what covers the lava (a platform, a walkway over it)
    std::unordered_map<uint32_t, std::vector<glm::vec3>> roofs;
    auto cell_key = [](int x, int y, int z) { return static_cast<uint32_t>(x & 0x3ff) | static_cast<uint32_t>(y & 0x3ff) << 10 | static_cast<uint32_t>(z & 0x3ff) << 20; };
    int zmax = 0;  // (the tree's top cell)
    const bool any = coll_faces(
        tree,
        [&](int kind) { ++g_kind_hist[kind]; },
        [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& cc, int kind, const glm::ivec3& ci, bool second) {
            zmax = std::max(zmax, ci.z + 1);
            if (kind == 1 && !second) ++out->faces;
            if (kind != 1) {  // (surface kind 1: the deadly liquid - the lava; the others may cover it)
                std::vector<glm::vec3>& r = roofs[cell_key(ci.x, ci.y, ci.z)];
                for (const glm::vec3& v : {a, b, cc}) r.push_back(v);
                return;
            }
            const glm::vec3 n = glm::cross(b - a, cc - a);
            const float ar = 0.5f * glm::length(n);
            if (ar <= 1e-4f || std::abs(n.z) < 0.5f * glm::length(n)) return;  // (lava's top: not its sides)
            up_votes += n.z > 0.0f ? 1 : -1;
            tris.push_back({a, b, cc, ar, glm::vec2(static_cast<float>(ci.x) * 4.0f, static_cast<float>(ci.y) * 4.0f)});
        });
    if (!any || tris.empty()) return out;
    uint32_t rs = 0x9e3779b9u ^ static_cast<uint32_t>(level);
    auto rnd = [&rs]() {
        rs = rs * 1664525u + 1013904223u;
        return static_cast<float>(rs >> 8) * (1.0f / 16777216.0f);
    };
    // the lava hidden under the ground: the nearest face above a point is a floor (the ground, a platform: the lava's kill plane
    // runs under the level's hills) - no particles there; open lava has nothing above it, or a ceiling (a cave's roof)
    const float up_sign = up_votes >= 0 ? 1.0f : -1.0f;
    auto covered = [&](const glm::vec3& p) {
        const int cx = static_cast<int>(std::floor(p.x / 4.0f)), cy = static_cast<int>(std::floor(p.y / 4.0f));
        for (int cz = static_cast<int>(std::floor(p.z / 4.0f)); cz <= zmax; ++cz) {
            const auto it = roofs.find(cell_key(cx, cy, cz));
            if (it == roofs.end()) continue;
            const std::vector<glm::vec3>& r = it->second;
            float best = 1e9f;
            bool floor = false;
            for (size_t i = 0; i + 2 < r.size(); i += 3) {
                const glm::vec3 &a = r[i], &b = r[i + 1], &c = r[i + 2];
                const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
                if (std::abs(d) < 1e-6f) continue;  // (a wall: seen edge-on from above)
                const float l0 = ((b.y - c.y) * (p.x - c.x) + (c.x - b.x) * (p.y - c.y)) / d;
                const float l1 = ((c.y - a.y) * (p.x - c.x) + (a.x - c.x) * (p.y - c.y)) / d;
                const float l2 = 1.0f - l0 - l1;
                if (l0 < 0.0f || l1 < 0.0f || l2 < 0.0f) continue;
                const float z = l0 * a.z + l1 * b.z + l2 * c.z;
                if (z > p.z + 0.5f && z < best) {
                    best = z;
                    floor = glm::cross(b - a, c - a).z * up_sign > 0.0f;
                }
            }
            if (best < 1e9f) return floor;  // (the nearest above found in this cell)
        }
        return false;
    };
    float carry = rnd();  // (the area left over from the triangles before: every kLavaSpacing a point)
    for (const Tri& t : tris) {
        carry += t.area / kLavaSpacing;
        for (; carry >= 1.0f; carry -= 1.0f) {
            float u = rnd(), v = rnd();
            if (u + v > 1.0f) {
                u = 1.0f - u;
                v = 1.0f - v;
            }
            const glm::vec3 p = t.a + u * (t.b - t.a) + v * (t.c - t.a);
            if (p.x >= t.lo.x && p.x < t.lo.x + 4.0f && p.y >= t.lo.y && p.y < t.lo.y + 4.0f && !covered(p)) out->pts.push_back(p);
        }
    }
    out->area = static_cast<float>(out->pts.size()) * kLavaSpacing;  // (the lava's own area, each place once)
    out->spacing = kLavaSpacing;
    if (out->pts.size() > 262144) {  // (at most 262144 points: wider apart on a lava sea)
        const size_t step = (out->pts.size() + 262143) / 262144;
        size_t n = 0;
        for (size_t i = 0; i < out->pts.size(); i += step) out->pts[n++] = out->pts[i];
        out->pts.resize(n);
        out->spacing = kLavaSpacing * static_cast<float>(step);
    }
    auto column = [](const glm::vec3& p) { return std::clamp(static_cast<int>(p.x / 16.0f), 0, 63) + 64 * std::clamp(static_cast<int>(p.y / 16.0f), 0, 63); };
    std::stable_sort(out->pts.begin(), out->pts.end(), [&](const glm::vec3& a, const glm::vec3& b) { return column(a) < column(b); });
    out->col.assign(64 * 64 + 1, 0);
    for (const glm::vec3& p : out->pts) ++out->col[static_cast<size_t>(column(p)) + 1];
    for (size_t c = 1; c < out->col.size(); ++c) out->col[c] += out->col[c - 1];
    return out;
}

// ---- the sky ("rain shadow") ------------------------------------------------------------------------------------------------
// What falls from the sky (rain, snow, ash) stays out from under roofs: the level's collision seen from above, the height of its
// topmost face over each square of a grid (kSkyCell units) - a roof, a tunnel's ceiling, a room's, a ledge above -, read once per
// level on a thread of its own as the lava is (the same tree). A particle below that height at its place is under cover: not
// drawn (faded over the grid's square at the edges); a splash only where the ground itself is the topmost face. The drawing
// uploads a window of the grid around the camera (kSkyWin squares a side) as a texture, again when the camera nears its edge.
struct SkySet {
    int level = -1;
    uint32_t fp = 0;  // (the collision's fingerprint it was read from: used only while the game's is the same)
    float x0 = 0.0f, y0 = 0.0f, cell = kSkyCell;  // (the grid's corner and its squares' size)
    int w = 0, h = 0;
    std::vector<float> top;  // (w x h: the topmost face's height, kSkyNone none)
    int faces = 0;
};
std::shared_ptr<const SkySet> g_sky;  // (under g_lava_mu: the game's thread makes it, the drawing reads it)
std::atomic<uint32_t> g_sky_fp{0};
std::atomic<uint32_t> g_coll_fp{0};  // (the collision's fingerprint this frame: a grid of another - a level loading - is not used)
std::atomic<bool> g_sky_busy{false};

// orig: (host) the level's sky grid from the collision tree at `tree`: every face not a wall (seen from above: its normal at
// least 0.1 up or down) drawn into the grid at the squares' centres (and its corners' squares: a thin beam still counts), the
// highest height kept
std::shared_ptr<SkySet> sky_scan(int level, uint32_t tree) {
    auto out = std::make_shared<SkySet>();
    out->level = level;
    std::vector<glm::vec3> tris;
    glm::vec2 lo(1e9f), hi(-1e9f);
    coll_faces(
        tree, [](int) {},
        [&](const glm::vec3& a, const glm::vec3& b, const glm::vec3& c, int, const glm::ivec3&, bool) {
            const glm::vec3 n = glm::cross(b - a, c - a);
            const float nl = glm::length(n);
            if (nl <= 1e-6f || std::abs(n.z) < 0.1f * nl) return;  // (a wall, or nothing)
            for (const glm::vec3& v : {a, b, c}) {
                tris.push_back(v);
                lo = glm::min(lo, glm::vec2(v));
                hi = glm::max(hi, glm::vec2(v));
            }
        });
    out->faces = static_cast<int>(tris.size() / 3);
    if (tris.empty()) return out;
    float cell = kSkyCell;
    while ((hi.x - lo.x) / cell > 2047.0f || (hi.y - lo.y) / cell > 2047.0f) cell *= 2.0f;  // (at most 2048 squares a side: 16 MB)
    out->cell = cell;
    out->x0 = std::floor(lo.x / cell) * cell;
    out->y0 = std::floor(lo.y / cell) * cell;
    out->w = static_cast<int>(std::ceil((hi.x - out->x0) / cell)) + 1;
    out->h = static_cast<int>(std::ceil((hi.y - out->y0) / cell)) + 1;
    out->top.assign(static_cast<size_t>(out->w) * static_cast<size_t>(out->h), kSkyNone);
    const float inv = 1.0f / cell;
    auto put = [&](int x, int y, float z) {
        if (x < 0 || y < 0 || x >= out->w || y >= out->h) return;
        float& t = out->top[static_cast<size_t>(x) + static_cast<size_t>(y) * static_cast<size_t>(out->w)];
        t = std::max(t, z);
    };
    for (size_t i = 0; i + 2 < tris.size(); i += 3) {
        const glm::vec3 &a = tris[i], &b = tris[i + 1], &c = tris[i + 2];
        for (const glm::vec3& v : {a, b, c}) put(static_cast<int>((v.x - out->x0) * inv), static_cast<int>((v.y - out->y0) * inv), v.z);
        const float d = (b.y - c.y) * (a.x - c.x) + (c.x - b.x) * (a.y - c.y);
        if (std::abs(d) < 1e-8f) continue;
        const float zlo = std::min({a.z, b.z, c.z}), zhi = std::max({a.z, b.z, c.z});
        const int gx0 = static_cast<int>(std::floor((std::min({a.x, b.x, c.x}) - out->x0) * inv)), gx1 = static_cast<int>(std::floor((std::max({a.x, b.x, c.x}) - out->x0) * inv));
        const int gy0 = static_cast<int>(std::floor((std::min({a.y, b.y, c.y}) - out->y0) * inv)), gy1 = static_cast<int>(std::floor((std::max({a.y, b.y, c.y}) - out->y0) * inv));
        for (int gy = gy0; gy <= gy1; ++gy)
            for (int gx = gx0; gx <= gx1; ++gx) {
                const float px = out->x0 + (static_cast<float>(gx) + 0.5f) * cell, py = out->y0 + (static_cast<float>(gy) + 0.5f) * cell;
                const float l0 = ((b.y - c.y) * (px - c.x) + (c.x - b.x) * (py - c.y)) / d;
                const float l1 = ((c.y - a.y) * (px - c.x) + (a.x - c.x) * (py - c.y)) / d;
                const float l2 = 1.0f - l0 - l1;
                if (l0 < -1e-4f || l1 < -1e-4f || l2 < -1e-4f) continue;
                put(gx, gy, std::clamp(l0 * a.z + l1 * b.z + l2 * c.z, zlo, zhi));
            }
    }
    return out;
}

// The window of the sky grid the frame's draws read: the texture's contents (kSkyWin x kSkyWin heights, then a row whose first
// four texels are the corner's x and y, the squares' size and 1 when on - the materials read the window from it), its corner
struct SkyWin {
    std::shared_ptr<const SkySet> set;  // (the grid it was taken from)
    glm::vec2 centre{0.0f}, corner{0.0f};
    float cell = kSkyCell;
    bool fresh = false;  // (taken again: to upload)
    std::vector<float> texels;
    GLuint gl_tex = 0;
    vk::GpuImage vk_img;
} g_skywin;

// orig: (host) the window around the camera, taken again from the level's grid when that changed or the camera moved far enough;
// false: no grid for this level, or the test off (RAC1_PARTICLES_SKY=0: the texture's row then says off)
bool sky_window_update(const glm::vec3& cam) {
    std::shared_ptr<const SkySet> set;
    if (g_particles_sky != 0) {
        std::lock_guard<std::mutex> lk(g_lava_mu);
        set = g_sky;
    }
    const size_t row = static_cast<size_t>(kSkyWin) * kSkyWin;
    if (set == nullptr || set->level != mrp_user_lights_level() || set->fp != g_coll_fp.load() || set->top.empty()) {
        if (g_skywin.set != nullptr) {  // (the level's window let go: off)
            g_skywin.set = nullptr;
            g_skywin.texels.assign(row + kSkyWin, kSkyNone);
            g_skywin.texels[row + 3] = 0.0f;
            g_skywin.fresh = true;
        }
        return false;
    }
    if (g_skywin.set == set && glm::length(glm::vec2(cam) - g_skywin.centre) < kSkyMove) return true;
    g_skywin.set = set;
    g_skywin.centre = glm::vec2(cam);
    g_skywin.cell = set->cell;
    const int ix0 = static_cast<int>(std::floor((cam.x - set->x0) / set->cell)) - kSkyWin / 2;
    const int iy0 = static_cast<int>(std::floor((cam.y - set->y0) / set->cell)) - kSkyWin / 2;
    g_skywin.corner = glm::vec2(set->x0 + static_cast<float>(ix0) * set->cell, set->y0 + static_cast<float>(iy0) * set->cell);
    g_skywin.texels.assign(row + kSkyWin, kSkyNone);
    for (int y = 0; y < kSkyWin; ++y) {
        const int gy = iy0 + y;
        if (gy < 0 || gy >= set->h) continue;
        for (int x = 0; x < kSkyWin; ++x) {
            const int gx = ix0 + x;
            if (gx >= 0 && gx < set->w)
                g_skywin.texels[static_cast<size_t>(x) + static_cast<size_t>(y) * kSkyWin] = set->top[static_cast<size_t>(gx) + static_cast<size_t>(gy) * static_cast<size_t>(set->w)];
        }
    }
    g_skywin.texels[row] = g_skywin.corner.x;
    g_skywin.texels[row + 1] = g_skywin.corner.y;
    g_skywin.texels[row + 2] = g_skywin.cell;
    g_skywin.texels[row + 3] = 1.0f;
    g_skywin.fresh = true;
    return true;
}

// orig: (host) OpenGL: the window's texture (R32F, kSkyWin x kSkyWin + 1), made the first time and uploaded when taken again (it
// stays bound on the active unit)
GLuint sky_gl_texture() {
    if (g_skywin.gl_tex == 0) {
        glGenTextures(1, &g_skywin.gl_tex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_skywin.gl_tex, "Ambient particles: sky");
        glBindTexture(GL_TEXTURE_2D, g_skywin.gl_tex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_R32F, kSkyWin, kSkyWin + 1, 0, GL_RED, GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        if (g_skywin.texels.empty()) {  // (off until a window is taken)
            g_skywin.texels.assign(static_cast<size_t>(kSkyWin) * (kSkyWin + 1), kSkyNone);
            g_skywin.texels[static_cast<size_t>(kSkyWin) * kSkyWin + 3] = 0.0f;
        }
        g_skywin.fresh = true;
    }
    glBindTexture(GL_TEXTURE_2D, g_skywin.gl_tex);
    if (g_skywin.fresh) {
        GLint prev_unpack = 0, prev_pbo = 0, prev_row = 0;
        glGetIntegerv(GL_UNPACK_ALIGNMENT, &prev_unpack);
        glGetIntegerv(GL_UNPACK_ROW_LENGTH, &prev_row);
        glGetIntegerv(GL_PIXEL_UNPACK_BUFFER_BINDING, &prev_pbo);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
        glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kSkyWin, kSkyWin + 1, GL_RED, GL_FLOAT, g_skywin.texels.data());
        glPixelStorei(GL_UNPACK_ALIGNMENT, prev_unpack);
        glPixelStorei(GL_UNPACK_ROW_LENGTH, prev_row);
        glBindBuffer(GL_PIXEL_UNPACK_BUFFER, static_cast<GLuint>(prev_pbo));
        g_skywin.fresh = false;
    }
    return g_skywin.gl_tex;
}

// orig: (host) Vulkan: the window's image, made the first time and uploaded when taken again (outside a pass: the upload ends the
// one under way); null when it cannot be made
vk::GpuImage* sky_vk_image() {
    if (g_skywin.vk_img.image == VK_NULL_HANDLE) {
        if (!vk::create_image(kSkyWin, kSkyWin + 1, VK_FORMAT_R32_SFLOAT, VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                              g_skywin.vk_img))
            return nullptr;
        if (g_skywin.texels.empty()) {
            g_skywin.texels.assign(static_cast<size_t>(kSkyWin) * (kSkyWin + 1), kSkyNone);
            g_skywin.texels[static_cast<size_t>(kSkyWin) * kSkyWin + 3] = 0.0f;
        }
        g_skywin.fresh = true;
    }
    if (g_skywin.fresh) {
        vk::upload_image_2d(g_skywin.vk_img, g_skywin.texels.data(), g_skywin.texels.size() * sizeof(float));
        g_skywin.fresh = false;
    }
    return &g_skywin.vk_img;
}

// orig: (host) the slots the frame's "source lava" draws read (uEmit): some of the level's lava points within kLavaRange of the
// camera, the nearer the more (a point taken with the chance a / d^2, by its own fixed number: about as many on screen near as
// far; `a` follows the count so that some 460 of the 512 slots are used; points behind the camera count as twice as far). A slot
// keeps its point while that stays taken (a margin: no flicker at the edge); a point no longer taken lets its particles fade out
// over 1.5 s before the slot takes another, whose particles fade in: nothing jumps. uEmit's w: the time the slot got its point
// (>= 0), -2 - the time it was let go (fading out), -1 none
constexpr float kLavaFade = 1.5f;
void lava_slots_update(const glm::vec3& cam, const glm::vec3& fwd, float t, float out[kLavaSlots][4], float* spread) {
    std::shared_ptr<const LavaSet> set;
    {
        std::lock_guard<std::mutex> lk(g_lava_mu);
        set = g_lava;
    }
    if (set != nullptr && set->level != mrp_user_lights_level()) set = nullptr;
    if (set == nullptr || g_slots.set != set) {
        std::fill(std::begin(g_slots.idx), std::end(g_slots.idx), -1);
        std::fill(std::begin(g_slots.gone), std::end(g_slots.gone), -1.0f);
        g_slots.set = set;
        g_slots.a = 40.0f;
    }
    for (int i = 0; i < kLavaSlots; ++i) out[i][3] = -1.0f;
    if (set == nullptr || set->pts.empty()) return;
    *spread = 0.65f * std::sqrt(set->spacing);  // (around a point: about the lava it stands for)
    auto chance = [](uint32_t i) { return static_cast<float>((i * 2654435761u) >> 8) * (1.0f / 16777216.0f); };  // (the point's own number)
    static std::vector<std::pair<float, int>> near;  // (the points taken: their distance squared)
    near.clear();
    const float r2 = kLavaRange * kLavaRange, a = g_slots.a;
    float expect = 0.0f;  // (the count `a` gives: steered toward the slots' share)
    const int cx0 = std::clamp(static_cast<int>((cam.x - kLavaRange) / 16.0f), 0, 63), cx1 = std::clamp(static_cast<int>((cam.x + kLavaRange) / 16.0f), 0, 63);
    const int cy0 = std::clamp(static_cast<int>((cam.y - kLavaRange) / 16.0f), 0, 63), cy1 = std::clamp(static_cast<int>((cam.y + kLavaRange) / 16.0f), 0, 63);
    static std::vector<uint8_t> state;  // (by point: 1 taken now, 2 taken with the margin; cleared again below)
    if (state.size() != set->pts.size()) state.assign(set->pts.size(), 0);
    static std::vector<int> touched;
    touched.clear();
    for (int cy = cy0; cy <= cy1; ++cy)
        for (int cx = cx0; cx <= cx1; ++cx)
            for (uint32_t i = set->col[static_cast<size_t>(cx + 64 * cy)]; i < set->col[static_cast<size_t>(cx + 64 * cy) + 1]; ++i) {
                const glm::vec3 d = set->pts[i] - cam;
                float q = glm::dot(d, d);
                if (glm::dot(d, fwd) < -0.3f * std::sqrt(q)) q *= 4.0f;  // (behind the camera: as if twice as far)
                if (q >= r2) continue;
                const float k = a / std::max(q, 9.0f);
                expect += std::min(k, 1.0f);
                const float c = chance(i);
                if (c < k * 1.3f) {
                    state[i] = c < k ? 1 : 2;
                    touched.push_back(static_cast<int>(i));
                    if (c < k) near.emplace_back(q, static_cast<int>(i));
                }
            }
    g_slots.a = std::clamp(a * std::pow(460.0f / std::max(expect, 1.0f), 0.1f), 0.5f, 1e5f);
    // the slots: kept while taken (with the margin), else let go; let go long enough, free
    for (int k = 0; k < kLavaSlots; ++k) {
        const int i = g_slots.idx[k];
        if (i < 0) continue;
        if (g_slots.gone[k] >= 0.0f) {
            if (t - g_slots.gone[k] > kLavaFade || t < g_slots.gone[k]) g_slots.idx[k] = -1;
        } else if (state[static_cast<size_t>(i)] == 0) {
            g_slots.gone[k] = t;
        } else {
            state[static_cast<size_t>(i)] = 3;  // (in a slot already)
        }
    }
    std::sort(near.begin(), near.end());  // (the nearest first into the free slots)
    int k = 0;
    for (const auto& [q, i] : near) {
        if (state[static_cast<size_t>(i)] != 1) continue;
        while (k < kLavaSlots && g_slots.idx[k] >= 0) ++k;
        if (k >= kLavaSlots) break;
        g_slots.idx[k] = i;
        g_slots.since[k] = t;
        g_slots.gone[k] = -1.0f;
    }
    for (int i : touched) state[static_cast<size_t>(i)] = 0;
    for (int s = 0; s < kLavaSlots; ++s) {
        const int i = g_slots.idx[s];
        if (i < 0) continue;
        const glm::vec3& p = set->pts[static_cast<size_t>(i)];
        out[s][0] = p.x;
        out[s][1] = p.y;
        out[s][2] = p.z;
        out[s][3] = g_slots.gone[s] >= 0.0f ? -2.0f - g_slots.gone[s] : g_slots.since[s];
    }
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
    const bool sky = sky_window_update(cam);  // (what falls from the sky: not under a roof)
    int seed = 0;
    bool lava_done = false;  // (the lava's slots: once a frame, for every "source lava" line)
    static float lava_emit[kLavaSlots][4];
    float lava_spread = 1.1f;
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
        // the wind: the line's, steady (gusts only if the line asks); the fall not blown, the wind's share carries the rest
        const float gust = 1.0f + 0.25f * ps.gust * std::sin(t * 0.37f) * std::sin(t * 0.13f + 1.0f);
        u.vel[0] = ps.wind[0] * gust;
        u.vel[1] = ps.wind[1] * gust;
        u.vel[2] = ps.src_lava ? (ps.rise > 0.0f ? ps.rise : K.lava_rise) : K.fall;
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
        u.move[0] = K.wander;
        u.move[1] = kLavaCurrent;
        if (sky && K.outdoors && !ps.src_lava) {
            u.sky[0] = g_skywin.corner.x;
            u.sky[1] = g_skywin.corner.y;
            u.sky[2] = g_skywin.cell;
            u.sky[3] = 1.0f;
        }
        if (ps.src_lava) {  // from the lava near the camera
            if (!lava_done) {
                lava_slots_update(cam, -glm::vec3(iv[2]), t, lava_emit, &lava_spread);
                lava_done = true;
            }
            u.src[0] = 1.0f;
            u.src[2] = kLavaRange;
            u.src[3] = lava_spread;
            std::memcpy(u.emit, lava_emit, sizeof u.emit);
        }
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
    RAC1_PROF_ZONE(kParticles);  // (the lava's slots, made with the draws, included)
    std::vector<Draw> draws;
    if (!make_draws(draws, vp, game_frame)) return;
    int total = 0;
    if (vk::is_active()) {
        if (!vk_init()) return;
        int prev_vp[4] = {};
        vk::get_viewport(prev_vp);
        vk::GpuImage& depth = vk::active_depth_image();
        vk::GpuImage& color = vk::active_color_image();
        if (depth.image == VK_NULL_HANDLE || color.image == VK_NULL_HANDLE) return;
        const vk::GpuImage* sky = sky_vk_image();  // (the sky grid's window: off until a level's is read)
        if (sky == nullptr) return;
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
            VkDescriptorImageInfo sii{samp, sky->view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
            VkWriteDescriptorSet w[3]{};
            w[0].sType = w[1].sType = w[2].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
            w[0].dstSet = w[1].dstSet = w[2].dstSet = ds;
            w[0].dstBinding = 0;
            w[0].descriptorCount = 1;
            w[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
            w[0].pBufferInfo = &dbi;
            w[1].dstBinding = 1;
            w[1].descriptorCount = 1;
            w[1].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[1].pImageInfo = &dii;
            w[2].dstBinding = 2;
            w[2].descriptorCount = 1;
            w[2].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
            w[2].pImageInfo = &sii;
            vkUpdateDescriptorSets(vk::device(), 3, w, 0, nullptr);
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
        GLint prev_sky = 0;
        glActiveTexture(GL_TEXTURE0 + kSkyUnit);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_sky);
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
            glActiveTexture(GL_TEXTURE0 + kSkyUnit);
            sky_gl_texture();  // (the sky grid's window, uploaded when taken again)
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
        glActiveTexture(GL_TEXTURE0 + kSkyUnit);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_sky));
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
    if (trace && game_frame % 120 == 0) {
        int used = 0;
        for (int i : g_slots.idx) used += i >= 0 && g_slots.set != nullptr ? 1 : 0;
        std::printf("[particles] frame %llu level %d: %zu draws, %d particles, light %.2f %.2f %.2f, the game's rain / snow %s; the lava's slots %d (a %.1f)\n",
                    static_cast<unsigned long long>(game_frame), mrp_user_lights_level(), draws.size(), total, draws[0].u.col[0], draws[0].u.col[1], draws[0].u.col[2],
                    g_hide_game ? "hidden" : "drawn", used, static_cast<double>(g_slots.a));
        if (g_skywin.set != nullptr) {  // the sky grid at the camera: the topmost face above it (a roof: rain and snow kept out)
            const glm::vec3 c = g_fr.cam;
            const SkySet& k = *g_skywin.set;
            const int gx = static_cast<int>(std::floor((c.x - k.x0) / k.cell)), gy = static_cast<int>(std::floor((c.y - k.y0) / k.cell));
            const float top = gx >= 0 && gy >= 0 && gx < k.w && gy < k.h ? k.top[static_cast<size_t>(gx) + static_cast<size_t>(gy) * static_cast<size_t>(k.w)] : kSkyNone;
            std::printf("[particles]   the camera at %.1f %.1f %.1f, the topmost face there at %.1f: %s\n", static_cast<double>(c.x), static_cast<double>(c.y),
                        static_cast<double>(c.z), static_cast<double>(top), top > c.z ? "under a roof" : "under the sky");
        }
    }
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
    // (the materials' rain rings - Surface Detail, a level with "rain on" - want the sky grid too: none under a roof)
    const bool mat_rain = g_surface_detail > 0 && g_modern_shadows != 0 && mrp_user_rain();
    if (g_ambient_particles > 0 || mat_rain) {  // the level's lava ("source lava") and sky, once per level: its collision read here, on the game's thread
        const int level = lw(0x0015ed84);
        const uint32_t tree = lwu(OV(0x00173e40));
        // (read when the level, the tree's place or its index changed and then stayed so for 30 frames: not while a level loads)
        uint32_t fp = level * 2654435761u ^ tree;
        if (tree >= 0x100000u && tree + 0x100u < 0x2000000u)
            for (uint32_t i = 0; i < 0x100u; i += 4) fp = (fp ^ lwu(tree + i)) * 16777619u;
        static uint32_t last_fp = 0;
        static int stable = 0;
        stable = fp == last_fp ? stable + 1 : 0;
        last_fp = fp;
        g_coll_fp = fp;
        bool lava_line = false, sky_line = false;  // (only for a level whose file has a "source lava" line; one falling from the sky)
        for (const ParticleSpec& ps : mrp_user_particles()) {
            lava_line = lava_line || (g_ambient_particles > 0 && ps.on && ps.src_lava);
            sky_line = sky_line || (g_ambient_particles > 0 && ps.on && !ps.src_lava && ps.kind >= 0 && ps.kind < kPartKinds && kKinds[ps.kind].outdoors);
        }
        sky_line = sky_line || mat_rain;
        if (stable >= 30 && fp != g_sky_fp && sky_line && !g_sky_busy.exchange(true)) {
            g_sky_fp = fp;
            std::thread([level, tree, fp] {  // (the sky grid: on a thread of its own, as the lava)
                const auto t0 = std::chrono::steady_clock::now();
                std::shared_ptr<SkySet> set = sky_scan(level, tree);
                set->fp = fp;
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                static const bool trace = std::getenv("RAC1_PARTICLES_TRACE") != nullptr;
                if (trace) {
                    int covered = 0;
                    for (float z : set->top) covered += z > kSkyNone ? 1 : 0;
                    std::printf("[particles] level %d: the sky grid %d x %d squares of %.2f units from (%.0f, %.0f), %d faces, %d squares with a face (read in %.1f ms)\n",
                                level, set->w, set->h, static_cast<double>(set->cell), static_cast<double>(set->x0), static_cast<double>(set->y0), set->faces, covered, ms);
                }
                std::lock_guard<std::mutex> lk(g_lava_mu);
                if (fp == g_sky_fp) g_sky = std::move(set);
                g_sky_busy = false;
            }).detach();
        }
        if (stable >= 30 && fp != g_lava_fp && lava_line && !g_lava_busy.exchange(true)) {
            g_lava_fp = fp;
            // (read on a thread of its own: tens of ms on a lava sea; the collision stays as it is while the level runs, and a
            // reading that ends after the level changed is dropped)
            std::thread([level, tree, fp] {
                const auto t0 = std::chrono::steady_clock::now();
                std::shared_ptr<const LavaSet> set = lava_scan(level, tree);
                const double ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                static const bool trace = std::getenv("RAC1_PARTICLES_TRACE") != nullptr;
                if (trace) {
                    std::printf("[particles] level %d: lava %d faces, %.0f square units, %zu points (read in %.1f ms); the collision's surface kinds:", level,
                                set->faces, static_cast<double>(set->area), set->pts.size(), ms);
                    for (int k = 0; k < 32; ++k)
                        if (g_kind_hist[k] != 0) std::printf(" %d: %d", k, g_kind_hist[k]);
                    std::printf("\n");
                    std::map<int, std::pair<glm::vec3, glm::vec3>> by_z;  // (the lava's heights: each with its extent and its points)
                    std::map<int, int> nz;
                    for (const glm::vec3& p : set->pts) {
                        const int z = static_cast<int>(std::floor(p.z / 2.0f));
                        auto it = by_z.find(z);
                        if (it == by_z.end()) by_z[z] = {p, p};
                        else it->second = {glm::min(it->second.first, p), glm::max(it->second.second, p)};
                        ++nz[z];
                    }
                    for (const auto& [z, b] : by_z)
                        std::printf("[particles]   lava at z %d..%d: %d points, x %.0f..%.0f y %.0f..%.0f\n", z * 2, z * 2 + 2, nz[z], static_cast<double>(b.first.x), static_cast<double>(b.second.x),
                                    static_cast<double>(b.first.y), static_cast<double>(b.second.y));
                }
                std::lock_guard<std::mutex> lk(g_lava_mu);
                if (fp == g_lava_fp) g_lava = std::move(set);
                g_lava_busy = false;
            }).detach();
        }
    }
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

// orig: (host) mrp_lighting.cpp (declared there, not in mrp_particles.h: the headers of src/game are the generated code's
// fingerprint), its shadow pass on a level whose materials rain: the sky grid's window around the camera for the materials' rain
// rings (none under a roof) - taken again when needed, uploaded (Vulkan) or bound on kSkyUnit (OpenGL). Before the level's grid is
// read nothing is made, and the materials read no window (no texture: off)
void mrp_particles_sky_frame() {
    const glm::vec3 cam(glm::inverse(render_view_matrix())[3]);
    const bool on = sky_window_update(cam);
    if (!on && g_skywin.gl_tex == 0 && g_skywin.vk_img.image == VK_NULL_HANDLE) return;
    if (vk::is_active()) {
        sky_vk_image();
    } else {
        GLint prev = 0;
        glGetIntegerv(GL_ACTIVE_TEXTURE, &prev);
        glActiveTexture(GL_TEXTURE0 + kSkyUnit);
        sky_gl_texture();
        glActiveTexture(static_cast<GLenum>(prev));
    }
}
// orig: (host) mrp_lighting.cpp (declared there): Vulkan, the sky grid's window for the lighting's set (binding 6); null: none yet
const vk::GpuImage* mrp_particles_sky_vk() {
    return g_skywin.vk_img.image != VK_NULL_HANDLE && g_skywin.vk_img.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? &g_skywin.vk_img : nullptr;
}

// orig: (host) see mrp_particles.h
void mrp_particles_stats(int* count, float* cpu_ms, float* gpu_ms) {
    *count = g_st.count;
    *cpu_ms = g_st.cpu_ms;
    *gpu_ms = -1.0f;  // (the Profiler tab's "Ambient particles" stage)
}

}  // namespace rac1
