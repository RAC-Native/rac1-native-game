// The MRP's modern lighting: the sun's cascaded shadow maps (see mrp_lighting.h). Host code, an enhancement: none of it is the game's.
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_fx.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_view.h>
#include <profiler/host_profiler.h>
#include <runtime/kernel/ee_copy.h>
#include <runtime/vk/vk_context.h>
#include <runtime/vk/vk_shaders.h>

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <atomic>
#include <chrono>
#include <vector>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <mutex>
#include <string>

namespace rac1 {

// (mrp_particles.cpp: the ambient particles' sky grid - docs/particles.md, "Under a roof" - which the materials' rain rings read too;
// declared here, not in mrp_particles.h: the headers of src/game are the generated code's fingerprint)
void mrp_particles_sky_frame();
const vk::GpuImage* mrp_particles_sky_vk();

namespace {
int env_int(const char* name, int def) {
    const char* e = std::getenv(name);
    return e != nullptr ? std::atoi(e) : def;
}
}  // namespace

int g_modern_shadows = env_int("RAC1_SHADOWS", 0);
int g_shadow_quality = std::clamp(env_int("RAC1_SHADOW_RES", 1), 0, 2);
int g_shadow_strength = std::clamp(env_int("RAC1_SHADOW_STRENGTH", 70), 0, 100);
int g_shadow_debug = env_int("RAC1_SHADOW_DEBUG", 0);
int g_modern_points = env_int("RAC1_POINTS", 1);
int g_point_shadows = env_int("RAC1_POINT_SHADOWS", 0);
int g_auto_lights = env_int("RAC1_AUTO_LIGHTS", 1);
int g_shadow_cache = env_int("RAC1_SHADOW_CACHE", 1);
int g_user_light_selected = -1;
int g_ao_intensity = std::clamp(env_int("RAC1_AO_INTENSITY", 75), 0, 200);
int g_ao = env_int("RAC1_AO", 0);
int g_sun_rays = env_int("RAC1_SUN_RAYS", 0);
int g_emissive = env_int("RAC1_EMISSIVE", 1);
int g_surface_detail = std::clamp(env_int("RAC1_SURFACE_DETAIL", 0), 0, 2);
float g_shadow_moby_dist = std::getenv("RAC1_SHADOW_MOBY_DIST") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_SHADOW_MOBY_DIST"))) : 0.0f;

namespace {
constexpr int kCascades = 3;
constexpr int kUnit = 24;  // the shadow map's texture unit (the world's samplers use 0..16)
constexpr GLenum kGlCubeMapArray = 0x9009;  // (GL_TEXTURE_CUBE_MAP_ARRAY, GL 4.0: not in the loader's header)
constexpr int kPtUnit = 25;  // the point lights' cube shadow maps' unit
constexpr int kTileUnit = 26;
constexpr int kSsrColUnit = 27, kSsrDepthUnit = 29;  // the materials' High step: the last frame's 3D (28: the cover probe's)
constexpr int kTilesX = 32, kTilesY = 18;  // (the view's clip square cut into tiles: per tile the lights whose sphere reaches it)
constexpr int kPtShadows = 4;  // the point lights with a shadow at most (the nearest)
constexpr int kPtRes = 512;    // their cube faces' resolution
constexpr float kPtNear = 0.3f;  // (an automatic emitter sits on its lamp's surface: the lamp itself not its occluder)
constexpr int kMaxPts = 96;  // the point lights lit per pixel at most (the shaders' arrays): the game's 8, then the automatic emitters
// how far toward the sun (from a cascade's centre) the casters reach, units: the light box's near plane, the same for the frame's casters
// and the added ones (no depth clamp: a far caster the camera sees would cast, one it does not would not - its shadow came and went)
constexpr float kBack = 400.0f;
// a cached cascade's box: its sphere this many times the view's slice's (the slice moves inside it this far before the
// static world is drawn again); the casters outside the view are looked for in the same larger sphere (mrp_lighting_cull_params)
constexpr float kCacheGrow = 1.25f;
std::atomic<bool> g_sun_known{false};
// the level's own sun direction (our data: "sun <azimuth> <elevation>" in data/lights/levelNN.txt, degrees; see mrp_lighting.h)
struct SunFile {
    bool on = false;
    float az = 0.0f, el = 45.0f;
} g_sun_file;
bool g_sun_rays_off = false;  // (the level's file says "sun_rays off": the Sun Rays pass off on this level)
bool g_sun_snap = false;  // (the editor changed it: the sun turned at once, not a degree a frame)
// the level's interiors (our data: "interior x0 y0 z0 x1 y1 z1" boxes and "interior_set <n>" light sets in its file): where the
// camera is in one (or the hero is lit with that set), the sun is off - the original's look -, faded over kInteriorFade frames
std::vector<SunInterior> g_interiors;
std::vector<int> g_interior_sets;
constexpr float kInteriorFade = 18.0f;  // (about 0.3 s)
float g_interior_k = 0.0f;              // 0 outdoors .. 1 inside (faded)
int g_interior_hero_set = -1;           // (the hero's light set, the debug menu's)
void user_ensure();  // (the level's file read, once per level: below)
void screen_point_read(const int vp[4]);  // (the debug menu's crosshair: below)
void lamp_scan(const int vp[4]);          // (testing, RAC1_LAMP_SCAN: below)
std::atomic<bool> g_has_sun{true};  // the level's key light comes from above: the sun's shadows (else none: Blarg)
// how much of the sun's weight the view keeps: 0 covered from the sun .. 1 open (see kProbeW)
float view_open();
// orig: (host) the covered views' probe: off (the level's interiors decide where the sun is off - a short covered passage
// faded the sun out and it came back slowly); RAC1_SUN_COVER=1 turns it on, for testing
bool cover_probe_on() {
    static const bool on = env_int("RAC1_SUN_COVER", 0) != 0;
    return on;
}
std::atomic<bool> g_casc_on{true};  // the cascades drawn: the zone has a sun (its shadows' weight may be faded to 0: the view covered)
float g_cull_sun[3] = {0.0f, 0.0f, 1.0f};  // (the sun for the casters' search on the scene's worker)
float g_cull_tan[2] = {0.0f, 0.0f};         // (the drawn view's half-angle tangents, 1 / P00 and 1 / P11)

struct Cascade {
    glm::mat4 vp{1.0f};   // world -> the light's clip space
    glm::mat4 tex{1.0f};  // world -> the map's [0, 1] coordinates and depth
    float texel = 0.0f;   // a texel's size in world units
    int res = 0;          // its square in the map: the map's size, or half of it (High: the far cascades at 2048)
    float part = 1.0f;    // (its square's share of the layer: the sampling's scale, uShPart)
    float range = 1.0f;   // its depth range, units (the box's near to far)
};

}  // namespace
// (mrp_moby_renderer.cpp: declared here, not in mrp_lighting.h - a game header is an input of the generated code's fingerprint, and
// changing one asks for ./setup.sh again) the light's clip-space rectangle of each moby part a caster pass draws, 8 floats each
// appended to `out` (x0, y0, x1, y1, its world centre, radius: the parts mrp_moby_caster_rect takes); the parts' count
int mrp_moby_caster_parts(const float* light_vp, std::vector<float>& out, const float* within = nullptr);
namespace {
// ---- the mobys over a cascade's static cache: their rectangles (RAC1_SHADOW_CLUSTERS) ---------------------------------------------
// Each frame a cached cascade's layer gets back the cache where last frame's mobys were drawn, then this frame's mobys are drawn,
// each inside a rectangle (the scissor) that the next frame puts back: nothing the mobys drew stays. One rectangle around every
// moby part was most of the near cascade's map once two mobys stood apart (Ratchet and a moby across the box: its 4096-texel map
// copied whole, 64 MB, every frame). The parts are grouped instead into at most kMaxClusters rectangles, each drawn with its own
// scissor and only the parts near its group (mrp_moby_draw_casters' sphere): the same depths, much less copied.
constexpr int kMaxClusters = 4;
struct MobyRects {
    int n = 0;
    int r[kMaxClusters][4] = {};    // texel rectangles (x0, y0, x1, y1; padded 2 texels)
    float s[kMaxClusters][4] = {};  // the world sphere around each rectangle's parts (x, y, z, r)
};
const int g_shadow_clusters = std::clamp(env_int("RAC1_SHADOW_CLUSTERS", kMaxClusters), 1, kMaxClusters);
// the far cascades' mobys redrawn every g_shadow_far_every frames (in turn: cascade 1 and 2 not on the same frame), the near one every
// frame; a cascade whose box moved (its cache drawn again: the camera's move, the sun's turn, a new level) is drawn whole that frame.
// 0: as before (cascade 2 every other frame, cascade 1 every frame)
const int g_shadow_far_every = std::clamp(env_int("RAC1_SHADOW_FAR_EVERY", 2), 0, 8);
// orig: (host) a cascade's moby casters as at most g_shadow_clusters rectangles of texels (false: none)
bool moby_rects(const float* light_vp, int cres, const float* within, MobyRects& out) {
    out.n = 0;
    static std::vector<float> parts;
    parts.clear();
    if (mrp_moby_caster_parts(light_vp, parts, within) == 0) return false;
    auto tx = [&](float v, bool up) {  // (as the one rectangle was: the texel's edge out, 2 texels more)
        const float t = (v * 0.5f + 0.5f) * static_cast<float>(cres);
        return std::clamp(static_cast<int>(up ? std::ceil(t) : std::floor(t)) + (up ? 2 : -2), 0, cres);
    };
    struct C {
        int r[4];
        glm::vec3 c;
        float rad;
    };
    auto merge = [](C& a, const C& b) {
        a.r[0] = std::min(a.r[0], b.r[0]);
        a.r[1] = std::min(a.r[1], b.r[1]);
        a.r[2] = std::max(a.r[2], b.r[2]);
        a.r[3] = std::max(a.r[3], b.r[3]);
        const float d = glm::length(b.c - a.c);  // (the sphere around both)
        if (d + b.rad <= a.rad) return;
        if (d + a.rad <= b.rad) {
            a.c = b.c;
            a.rad = b.rad;
            return;
        }
        const float r = 0.5f * (d + a.rad + b.rad);
        a.c += (b.c - a.c) * ((r - a.rad) / d);
        a.rad = r;
    };
    auto area = [](const int* r) { return static_cast<int64_t>(r[2] - r[0]) * (r[3] - r[1]); };
    // the parts' rectangles marked on a grid of kG x kG cells; the groups of touching cells, each with its parts' rectangle and sphere
    constexpr int kG = 64;
    static std::vector<int> cell, rects;
    cell.assign(static_cast<size_t>(kG) * kG, -1);  // (-1 empty, -2 a part's, then its group)
    const size_t np = parts.size() / 8;
    rects.resize(np * 4);
    const float cs = static_cast<float>(cres) / static_cast<float>(kG);
    auto cells = [&](const int* r, int c[4]) {
        c[0] = std::clamp(static_cast<int>(static_cast<float>(r[0]) / cs), 0, kG - 1);
        c[1] = std::clamp(static_cast<int>(static_cast<float>(r[1]) / cs), 0, kG - 1);
        c[2] = std::clamp(static_cast<int>(static_cast<float>(r[2] - 1) / cs), 0, kG - 1);
        c[3] = std::clamp(static_cast<int>(static_cast<float>(r[3] - 1) / cs), 0, kG - 1);
    };
    int all[4] = {cres, cres, 0, 0};
    size_t used = 0;
    for (size_t i = 0; i < np; ++i) {
        int* t = &rects[4 * i];
        t[0] = tx(parts[8 * i], false);
        t[1] = tx(parts[8 * i + 1], false);
        t[2] = tx(parts[8 * i + 2], true);
        t[3] = tx(parts[8 * i + 3], true);
        all[0] = std::min(all[0], t[0]);
        all[1] = std::min(all[1], t[1]);
        all[2] = std::max(all[2], t[2]);
        all[3] = std::max(all[3], t[3]);
        if (t[2] <= t[0] || t[3] <= t[1]) continue;
        ++used;
        int c[4];
        cells(t, c);
        for (int y = c[1]; y <= c[3]; ++y)
            for (int x = c[0]; x <= c[2]; ++x) cell[static_cast<size_t>(y) * kG + x] = -2;
    }
    if (used == 0 || g_shadow_clusters == 1) {  // (one rectangle: as before, the parts unfiltered)
        out.n = 1;
        std::memcpy(out.r[0], all, sizeof all);
        out.s[0][3] = -1.0f;
        return true;
    }
    int groups = 0;
    static std::vector<int> stack;
    for (int i0 = 0; i0 < kG * kG; ++i0) {  // (the groups: cells touching, corners too)
        if (cell[static_cast<size_t>(i0)] != -2) continue;
        stack.assign(1, i0);
        cell[static_cast<size_t>(i0)] = groups;
        while (!stack.empty()) {
            const int c = stack.back();
            stack.pop_back();
            const int cx = c % kG, cy = c / kG;
            for (int dy = -1; dy <= 1; ++dy)
                for (int dx = -1; dx <= 1; ++dx) {
                    const int x = cx + dx, y = cy + dy;
                    if (x < 0 || y < 0 || x >= kG || y >= kG) continue;
                    int& v = cell[static_cast<size_t>(y) * kG + x];
                    if (v == -2) {
                        v = groups;
                        stack.push_back(y * kG + x);
                    }
                }
        }
        ++groups;
    }
    static std::vector<C> cl;
    cl.assign(static_cast<size_t>(groups), C{{cres, cres, 0, 0}, glm::vec3(0.0f), -1.0f});
    for (size_t i = 0; i < np; ++i) {
        const int* r = &rects[4 * i];
        if (r[2] <= r[0] || r[3] <= r[1]) continue;
        int c[4];
        cells(r, c);
        C& g = cl[static_cast<size_t>(cell[static_cast<size_t>(c[1]) * kG + c[0]])];
        const C p{{r[0], r[1], r[2], r[3]}, glm::vec3(parts[8 * i + 4], parts[8 * i + 5], parts[8 * i + 6]), parts[8 * i + 7]};
        if (g.rad < 0.0f) g = p;
        else merge(g, p);
    }
    if (cl.size() > 48) {  // (a crowd of groups: one rectangle, as before)
        out.n = 1;
        std::memcpy(out.r[0], all, sizeof all);
        out.s[0][3] = -1.0f;
        return true;
    }
    for (;;) {
        for (bool merged = true; merged;) {  // the rectangles that overlap: one (a copy's regions may not overlap)
            merged = false;
            for (size_t i = 0; i < cl.size(); ++i)
                for (size_t j = i + 1; j < cl.size();) {
                    const int* a = cl[i].r;
                    const int* b = cl[j].r;
                    if (a[0] < b[2] && b[0] < a[2] && a[1] < b[3] && b[1] < a[3]) {
                        merge(cl[i], cl[j]);
                        cl[j] = cl.back();
                        cl.pop_back();
                        merged = true;
                    } else {
                        ++j;
                    }
                }
        }
        if (cl.size() <= static_cast<size_t>(g_shadow_clusters)) break;
        size_t bi = 0, bj = 1;  // too many: the pair whose union adds the least
        int64_t best = INT64_MAX;
        for (size_t i = 0; i < cl.size(); ++i)
            for (size_t j = i + 1; j < cl.size(); ++j) {
                const int u[4] = {std::min(cl[i].r[0], cl[j].r[0]), std::min(cl[i].r[1], cl[j].r[1]), std::max(cl[i].r[2], cl[j].r[2]), std::max(cl[i].r[3], cl[j].r[3])};
                const int64_t add = area(u) - area(cl[i].r) - area(cl[j].r);
                if (add < best) {
                    best = add;
                    bi = i;
                    bj = j;
                }
            }
        merge(cl[bi], cl[bj]);
        cl[bj] = cl.back();
        cl.pop_back();
    }
    out.n = static_cast<int>(cl.size());
    for (int i = 0; i < out.n; ++i) {
        std::memcpy(out.r[i], cl[static_cast<size_t>(i)].r, sizeof out.r[i]);
        const C& c = cl[static_cast<size_t>(i)];
        out.s[i][0] = c.c.x;
        out.s[i][1] = c.c.y;
        out.s[i][2] = c.c.z;
        out.s[i][3] = out.n > 1 ? c.rad : -1.0f;  // (one rectangle: every part, unfiltered, as before)
    }
    return true;
}

struct State {
    GLuint fbo = 0, tex = 0;
    int res = 0;
    bool on = false;  // this frame's receivers shade
    Cascade casc[kCascades];
    glm::vec3 sun{0.0f, 0.0f, 1.0f};  // toward the sun
    float sun_k = 1.0f;               // its shadows' weight (the zone's light's share; faded between zones)
    glm::vec4 key{0.0f};              // the key light's colour (as the set holds it) and back-face factor: what a shadow takes off the world's colours
    glm::vec3 key_dir{0.0f, 0.0f, 1.0f};  // toward the key light as the game lights with it (the sun's direction unless the level's file turns it)
    int npts = 0;                     // the frame's active point lights: position, radius; colour
    int ngame = 0;                    // (the list's first ngame: the game's own lights - the mobys have them per vertex already, #110)
    glm::vec4 pt_pos[kMaxPts], pt_col[kMaxPts];  // (pt_col.w: the light's cube in the shadow array, -1 none)
    float pt_down[kMaxPts] = {};                 // (the share of each light aimed down - a lamp per class's -, 0 every way alike)
    int pt_slot[kMaxPts] = {};                   // (the game's slot of each: its casters' view, 3 + slot; -1 an automatic emitter; -2 a moby class's lamp: no shadow cube)
    uint64_t frame = 0;              // the far cascade's turn
    // the static world's cache per cascade: the cascade's box held still while the view's slice stays inside it (its
    // sphere, kCacheGrow times the slice's), the world's casters drawn into `stex` only when it moves; each frame that map is copied
    // into the cascade's layer and the mobys drawn over it
    struct Cache {
        bool valid = false;
        glm::vec3 lc{0.0f};  // the box's centre in the light's frame (x, y snapped to its texels)
        float R = 0.0f, back = 0.0f;
        int res = 0;
        GLuint stex = 0;
        int stex_res = 0;
        bool incomplete = false;  // drawn without the casters outside the view: drawn again as soon as they are made
        glm::vec3 prev{0.0f};     // the slice's centre the frame before (its speed: when the box's edge is near)
        MobyRects dirty;  // the layer's rectangles the mobys were drawn into last frame (moby_rects)
    } cache[kCascades];
    std::atomic<bool> want{true};  // the next frame's casters outside the view wanted (the scene's worker reads it)
    bool refresh[kCascades] = {};
    uint64_t cache_key = 0;  // (the level, the sun, the LOD, the quality: any change empties the caches)
    uint64_t refreshes = 0, frames_cached = 0, built = 0;
    GLuint ptex = 0;                  // the point lights' cube shadow maps (a cube map array, kPtShadows cubes)
    glm::vec3 cam{0.0f};
    glm::mat4 view_proj{1.0f};  // the view's world -> clip (the point lights' tiles)
    GLuint tiles = 0;           // their texture (kTilesX x kTilesY, RGBA32UI: lights 0..31, 32..63, 64..95)
    glm::mat4 inv_view{1.0f};
    glm::vec2 proj{1.0f, 1.0f};
    float splits[kCascades] = {18.0f, 60.0f, 220.0f};
    bool logged = false;
} g_sh;
// orig: (host) a far cascade's layer kept as it is this frame (g_shadow_far_every; its box not moved: the caller checks)
bool far_skip(int k) {
    if (k == 0) return false;
    if (g_shadow_far_every == 0) return k == 2 && (g_sh.frame & 1u) != 0;
    return (g_sh.frame + static_cast<uint64_t>(k)) % static_cast<uint64_t>(g_shadow_far_every) != 0;
}

// orig: (host) a cascade's resolution - at the High setting (4096) the near cascade alone at full size, the others at half
// (drawn into a corner of their layer: the far ones' texels are coarse anyway, the cost of 4096 x 4096 three times is not)
int cascade_res(int k) { return g_shadow_quality >= 2 && k > 0 ? g_sh.res / 2 : g_sh.res; }

// orig: (host) one light set's sun - of its two directional lights (0x17fec0 + 0x40 x set: colour 1, direction 1, colour 2,
// direction 2) the brightest coming from above (at least ~11 degrees: the direction negated, the relighting
// routines take it as the light's travel), with its share of the set's brightest (the shadow's weight: a dim second light, weak
// shadows); none when both come from below or the horizon (Blarg's hangar's key light, Umbris' lava glow from beneath)
struct SetSun {
    glm::vec3 dir{0.0f, 0.0f, 1.0f};
    float weight = 0.0f;
    bool ok = false;
    glm::vec4 key{0.0f};  // that light's colour (rgb as the set holds it: x 128 in the vertex colours' bytes) and back-face factor (w: the set's k)
};
SetSun set_sun(uint32_t set) {
    SetSun r;
    if (set > 15) return r;
    const uint32_t base = OV(0x0017fec0u) + 0x40 * set;
    float lum[2], best = 0.0f;
    glm::vec3 dir[2];
    for (uint32_t l = 0; l < 2; ++l) {
        const uint32_t c = base + 0x20 * l, d = c + 0x10;
        lum[l] = std::max({draw_lwc1(c), draw_lwc1(c + 4), draw_lwc1(c + 8), 0.0f});
        dir[l] = glm::vec3(-draw_lwc1(d), -draw_lwc1(d + 4), -draw_lwc1(d + 8));
        best = std::max(best, lum[l]);
    }
    int pick = -1;
    for (int l = 0; l < 2; ++l) {
        const float n = glm::length(dir[l]);
        if (!(n > 1e-3f) || dir[l].z < 0.2f * n || !(lum[l] > 0.0f)) continue;
        if (pick < 0 || lum[l] > lum[pick]) pick = l;
    }
    if (pick < 0 || !(best > 0.0f)) return r;
    r.dir = glm::normalize(dir[pick]);
    r.weight = lum[pick] / best;
    r.ok = true;
    const uint32_t c = base + 0x20 * static_cast<uint32_t>(pick);
    r.key = glm::vec4(std::max(draw_lwc1(c), 0.0f), std::max(draw_lwc1(c + 4), 0.0f), std::max(draw_lwc1(c + 8), 0.0f), std::clamp(draw_lwc1(c + 0xc), 0.0f, 1.0f));
    return r;
}

// orig: (host) the game's key light this frame: the direction toward it, its shadows' weight (0..1; 0: no sun) - the light sets the game lights the hero with - moby +0x38 / +0x39, blended by +0x3a / 256: the zone's
// own key light, as Ratchet is lit (Veldin's outdoors set 0, its house set 1; Blarg's zones each their own) - each set's sun (set_sun),
// the two blended. On Veldin's outdoors (-0.397, 0.852, -0.342): the low warm sun the sky shows (checked against the screenshots)
glm::vec3 game_sun(float* weight, glm::vec4* key) {
    *weight = 1.0f;
    *key = glm::vec4(0.0f);
    if (std::getenv("RAC1_SHADOW_LIGHTS") != nullptr) {  // (testing: the level's light sets, once)
        static uint32_t shown = 0xffffffffu;
        if (shown != OV(0x0017fec0u)) {
            shown = OV(0x0017fec0u);
            for (uint32_t i = 0; i < 16; ++i) {
                const uint32_t b = shown + 0x40 * i;
                float v[16];
                bool any = false;
                for (uint32_t k = 0; k < 16; ++k) {
                    v[k] = draw_lwc1(b + 4 * k);
                    any = any || v[k] != 0.0f;
                }
                if (any)
                    std::printf("[shadows] light set %u @%08x: c1 %.2f %.2f %.2f k1 %.2f d1 %.3f %.3f %.3f | c2 %.2f %.2f %.2f k2 %.2f d2 %.3f %.3f %.3f\n", i, b, v[0], v[1], v[2],
                                v[3], v[4], v[5], v[6], v[8], v[9], v[10], v[11], v[12], v[13], v[14]);  // (as float)
            }
        }
    }
    if (std::getenv("RAC1_SUN_TRACE") != nullptr) {  // (testing: the light sets the hero is lit with)
        static uint32_t last = 0xffffffffu;
        const uint32_t hero = draw_lwu(OV(0x0015ffd8u));
        static int census = 0;
        if (hero != 0 && ++census == 400) {  // (the light sets the level's mobys use, once: count, an example position, the key light)
            std::map<uint32_t, std::pair<int, uint32_t>> sets;
            for (uint32_t m = hero; m < 0x01fe0000u && draw_lbu(m + 0x20) != 0xff; m += 0x100) {
                if (static_cast<int8_t>(draw_lbu(m + 0x20)) < 0) continue;
                const uint32_t k = draw_lbu(m + 0x38) | (draw_lbu(m + 0x39) << 8);
                auto& e = sets[k];
                if (e.first++ == 0) e.second = m;
            }
            for (const auto& [k, e] : sets) {
                const uint32_t da = OV(0x0017fec0u) + 0x40 * (k & 15) + 0x10;
                std::printf("[sun] sets %u / %u: %d mobys, e.g. at %.0f %.0f %.0f; set a's d1 (%.2f %.2f %.2f) colour %.2f %.2f %.2f\n", k & 0xff, k >> 8, e.first,
                            draw_lwc1(e.second + 0x10), draw_lwc1(e.second + 0x14), draw_lwc1(e.second + 0x18), draw_lwc1(da), draw_lwc1(da + 4), draw_lwc1(da + 8), draw_lwc1(da - 0x10), draw_lwc1(da - 0xc),
                            draw_lwc1(da - 8));
            }
        }
        if (hero != 0) {
            const uint32_t a = draw_lbu(hero + 0x38), b = draw_lbu(hero + 0x39), w = draw_lbu(hero + 0x3a), key = a | (b << 8) | (w << 16);
            if (key != last) {
                last = key;
                const uint32_t da = OV(0x0017fec0u) + 0x40 * (a & 15) + 0x10, db = OV(0x0017fec0u) + 0x40 * (b & 15) + 0x10;
                std::printf("[sun] hero %.1f %.1f %.1f: sets %u / %u blend %u: d1 a (%.2f %.2f %.2f) b (%.2f %.2f %.2f)\n", draw_lwc1(hero + 0x10), draw_lwc1(hero + 0x14), draw_lwc1(hero + 0x18), a, b, w,
                            draw_lwc1(da), draw_lwc1(da + 4), draw_lwc1(da + 8), draw_lwc1(db), draw_lwc1(db + 4), draw_lwc1(db + 8));
            }
        }
    }
    uint32_t a = 0, b = 0;
    float w = 0.0f;
    static const bool zone_dir = std::getenv("RAC1_SUN_ZONE") != nullptr;  // (testing: turn the sun to each zone's light set)
    if (const uint32_t hero = draw_lwu(OV(0x0015ffd8u)); hero != 0) {
        a = draw_lbu(hero + 0x38);
        b = draw_lbu(hero + 0x39);
        w = static_cast<float>(draw_lbu(hero + 0x3a)) / 256.0f;
        if (a > 15) a = 0;  // (15 / 15 or none: the level's own)
        if (b > 15) b = a;
        if (a == 15) a = b = 0;
    }
    const SetSun s0 = set_sun(0);
    const SetSun sa = set_sun(a), sb = set_sun(b);
    if (!zone_dir && s0.ok) {
        const float wa = sa.ok ? sa.weight : 0.0f;
        const float wb = sb.ok ? sb.weight : 0.0f;
        *weight = wa + (wb - wa) * w;
        *key = s0.key;
        return s0.dir;
    }
    if (sa.ok && sb.ok) {
        *weight = sa.weight + (sb.weight - sa.weight) * w;
        *key = sa.key + (sb.key - sa.key) * w;
        return glm::normalize(sa.dir + (sb.dir - sa.dir) * w);
    }
    if (sa.ok || sb.ok) {
        const SetSun& s1 = sa.ok ? sa : sb;
        *weight = s1.weight * (sa.ok ? 1.0f - w : w);
        *key = s1.key;
        return s1.dir;
    }
    *weight = 0.0f;
    return glm::normalize(glm::vec3(0.4f, -0.85f, 0.35f));
}

// orig: (host) the sun's shadows this frame: the direction toward the sun, their weight, the key light's colour and the game's own
// direction for it (what a shadow takes off the world's colours: the game's term, whatever way the shadows are cast). The game's key
// light (game_sun), turned where the level's file says so (g_sun_file: only where the game has a sun - its weight and colour kept);
// RAC1_SUN_DIR="x,y,z" (testing, world, z up) turns it everywhere, a sun of weight 1 where the game has none
glm::vec3 sun_direction(float* weight, glm::vec4* key, glm::vec3* key_dir) {
    const glm::vec3 g = game_sun(weight, key);
    *key_dir = g;
    if (const char* e = std::getenv("RAC1_SUN_DIR")) {
        float x = 0, y = 0, z = 0;
        if (std::sscanf(e, "%f,%f,%f", &x, &y, &z) == 3 && x * x + y * y + z * z > 1e-6f) {
            if (!(*weight > 0.0f)) {
                *weight = 1.0f;
                *key = glm::vec4(0.0f);
            }
            return glm::normalize(glm::vec3(x, y, z));
        }
    }
    user_ensure();
    if (g_sun_file.on && *weight > 0.0f) {
        const float az = glm::radians(g_sun_file.az), el = glm::radians(g_sun_file.el);
        return glm::vec3(std::cos(el) * std::cos(az), std::cos(el) * std::sin(az), std::sin(el));
    }
    return g;
}

// orig: (host) testing, RAC1_TFRAG_LIGHT_TRACE=<every>[,x,y,z,r]: the terrain's lit colours near the hero (or that point) taken
// apart as the game's relighting (world_tfrag_lighting 0x295350, world_render.md section 10) makes them - the vertex's base colour,
// its light set's two directional terms (colour x max(n.l, k n.l)) - against the colour EE memory holds; and the sun's set
void tfrag_light_trace() {
    const char* e = std::getenv("RAC1_TFRAG_LIGHT_TRACE");
    if (e == nullptr) return;
    static uint64_t n = 0;
    int every = 60;
    float px = 0, py = 0, pz = 0, pr = 12.0f;
    const int got = std::sscanf(e, "%d,%f,%f,%f,%f", &every, &px, &py, &pz, &pr);
    if (n++ % static_cast<uint64_t>(std::max(1, every)) != 0) return;
    if (got < 4) {
        const uint32_t hero = draw_lwu(OV(0x0015ffd8u));
        if (hero == 0) return;
        px = draw_lwc1(hero + 0x10);
        py = draw_lwc1(hero + 0x14);
        pz = draw_lwc1(hero + 0x18);
    }
    const uint32_t table = draw_lwu(OV(0x00160f4cu));
    const int32_t count = draw_lw(OV(0x00160f50u));
    if (table == 0 || count <= 0 || count > 0x10000) return;
    const uint32_t sets = OV(0x0017fec0u), norms = OV(0x00166080u);
    auto set_term = [&](uint32_t set, const glm::vec3& nrm, int l) {  // (one light's term, 0..255 units: the routine's colours are 65536 + byte / 128)
        const uint32_t c = sets + 0x40 * (set & 15) + 0x20 * static_cast<uint32_t>(l), d = c + 0x10;
        const float k = draw_lwc1(c + 0xc);
        const float nd = -glm::dot(nrm, glm::vec3(draw_lwc1(d), draw_lwc1(d + 4), draw_lwc1(d + 8)));  // (toward the light)
        return glm::vec3(draw_lwc1(c), draw_lwc1(c + 4), draw_lwc1(c + 8)) * (128.0f * std::max(nd, k * nd));
    };
    double sum_base = 0, sum_t[2] = {0, 0}, sum_lit = 0, sum_err = 0;
    int verts = 0, back = 0, neg_key = 0;
    std::map<uint32_t, int> set_use;
    for (int32_t i = 0; i < count; ++i) {
        const uint32_t ent = table + 0x40 * static_cast<uint32_t>(i);
        const uint32_t data = draw_lwu(ent + 0x10);
        if (data == 0 || data >= 0x02000000u) continue;
        const int8_t single = static_cast<int8_t>(draw_lbu(ent + 0x34));  // (>= 0: one set for the whole entry, its count +0x29 pairs)
        const uint32_t va = data + static_cast<uint32_t>(static_cast<int32_t>(draw_lh(ent + 0x30))) + 0x10;
        const uint32_t nv = single >= 0 ? 2u * draw_lbu(ent + 0x29) : draw_lbu(ent + 0x3c);
        const uint32_t lit = data + draw_lhu(ent + 0x1e);
        const glm::vec3 base_pos(static_cast<float>(draw_lw(va - 0x10)) / 1024.0f, static_cast<float>(draw_lw(va - 0xc)) / 1024.0f, static_cast<float>(draw_lw(va - 8)) / 1024.0f);
        for (uint32_t v = 0; v < nv; ++v) {
            const uint32_t vx = va + 8 * v;
            const uint32_t po = data + draw_lhu(vx);
            const glm::vec3 p = base_pos + glm::vec3(draw_lh(po), draw_lh(po + 2), draw_lh(po + 4)) / 1024.0f;
            if (glm::length(p - glm::vec3(px, py, pz)) > pr) continue;
            const uint32_t az = draw_lbu(vx + 2), el = draw_lbu(vx + 3);
            const float ca = draw_lwc1(norms + 8 * az), sa = draw_lwc1(norms + 8 * az + 4), ce = draw_lwc1(norms + 8 * el), se = draw_lwc1(norms + 8 * el + 4);
            const glm::vec3 nrm(ca * ce, sa * ce, se);
            const uint16_t b5 = draw_lhu(vx + 4), sh = draw_lhu(vx + 6);
            const glm::vec3 base(static_cast<float>((b5 & 31) << 3), static_cast<float>(((b5 >> 5) & 31) << 3), static_cast<float>(((b5 >> 10) & 31) << 3));
            const uint32_t sa_ = single >= 0 ? static_cast<uint32_t>(single) : (sh & 15u), sb_ = single >= 0 ? sa_ : ((sh >> 4) & 15u);
            const float wb = single >= 0 ? 0.0f : static_cast<float>(sh >> 8) / 256.0f;
            glm::vec3 t[2];
            for (int l = 0; l < 2; ++l) t[l] = set_term(sa_, nrm, l) * (1.0f - wb) + set_term(sb_, nrm, l) * wb;  // (approximate: the game blends the rows)
            const glm::vec3 sum = glm::min(base + t[0] + t[1], glm::vec3(255.0f));
            const uint32_t lc = draw_lwu(lit + 4 * v);
            const glm::vec3 got_c(static_cast<float>(lc & 0xff), static_cast<float>((lc >> 8) & 0xff), static_cast<float>((lc >> 16) & 0xff));
            auto lum = [](const glm::vec3& x) { return (x.r + x.g + x.b) / 3.0; };
            sum_base += lum(base);
            sum_t[0] += lum(t[0]);
            sum_t[1] += lum(t[1]);
            sum_lit += lum(got_c);
            sum_err += std::fabs(lum(sum) - lum(got_c));
            back += glm::dot(nrm, g_sh.sun) <= 0.0f ? 1 : 0;
            neg_key += lum(t[0]) < 0.0 ? 1 : 0;
            ++set_use[sa_ | (sb_ << 4)];
            ++verts;
        }
    }
    if (verts == 0) return;
    std::printf("[tfrag light] at %.1f %.1f %.1f r %.0f: %d vertices; mean (0..255) lit %.1f = base %.1f + light 1 %.1f + light 2 %.1f (recomposed off by %.1f); "
                "facing away from the sun %d, light 1 negative %d; sets", px, py, pz, pr, verts, sum_lit / verts, sum_base / verts, sum_t[0] / verts, sum_t[1] / verts,
                sum_err / verts, back, neg_key);
    for (const auto& [k, c] : set_use) std::printf(" %u/%u:%d", k & 15, k >> 4, c);
    std::printf("; sun (%.2f %.2f %.2f) weight %.2f\n", g_sh.sun.x, g_sh.sun.y, g_sh.sun.z, g_sh.sun_k);
}

void read_splits() {
    if (const char* e = std::getenv("RAC1_SHADOW_SPLITS")) {
        float a = 0, b = 0, c = 0;
        if (std::sscanf(e, "%f,%f,%f", &a, &b, &c) == 3 && a > 0 && b > a && c > b) {
            g_sh.splits[0] = a;
            g_sh.splits[1] = b;
            g_sh.splits[2] = c;
        }
    }
}

bool g_vk_ao_marking = false;  // (Vulkan: the MRP mobys drawn now mark their pixels in the stencil, bit 0, for the occlusion)
struct VkLighting {
    bool inited = false;
    VkDescriptorSetLayout set_layout = VK_NULL_HANDLE;
    vk::GpuImage tex{};
    VkImageView tex_layers[kCascades] = {};
    int res = 0;
    vk::GpuImage stex[kCascades]{};
    int stex_res[kCascades] = {};
    vk::GpuImage ptex{};
    VkImageView ptex_faces[6 * kPtShadows] = {};
    vk::GpuImage tiles{};
    uint64_t ubo_gen = 1;
    uint64_t off_gen = 0;
    VkDescriptorSet off_ds = VK_NULL_HANDLE;
    uint64_t rec_gen[2][2] = {};
    VkDescriptorSet rec_ds[2][2] = {};
    uint64_t cast_gen = 0;
    float cast_vp[16] = {};
    bool cast_opaque = false;
    VkDescriptorSet cast_ds = VK_NULL_HANDLE;

    // AO & Sun Rays Vulkan state
    VkShaderModule vs_mod = VK_NULL_HANDLE;
    VkShaderModule ao_fs_mod = VK_NULL_HANDLE;
    VkShaderModule blur_fs_mod = VK_NULL_HANDLE;
    VkShaderModule blur_sep_fs_mod = VK_NULL_HANDLE;
    VkShaderModule rays_mask_fs_mod = VK_NULL_HANDLE;
    VkShaderModule rays_blur_fs_mod = VK_NULL_HANDLE;
    VkShaderModule rays_split_fs_mod = VK_NULL_HANDLE;
    VkShaderModule rays_add_fs_mod = VK_NULL_HANDLE;
    VkDescriptorSetLayout samp1_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout samp2_layout = VK_NULL_HANDLE;
    VkDescriptorSetLayout samp3_layout = VK_NULL_HANDLE;  // (the occlusion's blurs: the occlusion, the depth, its stencil)
    VkImageView ao_stencil_view = VK_NULL_HANDLE;           // (ao_dtex's stencil: the mobys' pixels)
    VkPipelineLayout ao_layout = VK_NULL_HANDLE;
    VkPipelineLayout blur_layout = VK_NULL_HANDLE;
    VkPipelineLayout rays_mask_layout = VK_NULL_HANDLE;
    VkPipelineLayout rays_blur_layout = VK_NULL_HANDLE;
    VkPipelineLayout rays_split_layout = VK_NULL_HANDLE;
    VkPipelineLayout rays_add_layout = VK_NULL_HANDLE;
    VkPipeline ao_pipe = VK_NULL_HANDLE;
    VkPipeline blur_pipe = VK_NULL_HANDLE;
    VkPipeline blur_sep_h_pipe = VK_NULL_HANDLE;
    VkPipeline blur_sep_v_pipe = VK_NULL_HANDLE;
    VkPipeline rays_mask_pipe = VK_NULL_HANDLE;
    VkPipeline rays_blur_pipe = VK_NULL_HANDLE;
    VkPipeline rays_split_pipe = VK_NULL_HANDLE;
    VkPipeline rays_add_pipe = VK_NULL_HANDLE;
    vk::GpuImage ao_dtex{}, ao_atex{}, ao_btex{};
    int ao_w = 0, ao_h = 0, ao_aw = 0, ao_ah = 0;
    // (the 3D's own depth sampled, no copy: RAC1_AO_DEPTH_COPY=0; the stencil's view of that image, made when it changes, and the
    // final blur's programs without a depth attachment)
    VkImage ao_sdepth_img = VK_NULL_HANDLE;
    VkImageView ao_sdepth_stencil = VK_NULL_HANDLE;
    VkPipeline blur_nd_pipe = VK_NULL_HANDLE, blur_sep_v_nd_pipe = VK_NULL_HANDLE;
    vk::GpuImage rays_dtex{}, rays_ctex{}, rays_mtex{}, rays_rtex{};
    int rays_w = 0, rays_h = 0, rays_div = 0;
} g_vk_sh;

void vk_lighting_shutdown() {
    VkDevice dev = vk::device();
    if (dev != VK_NULL_HANDLE) {
        for (int k = 0; k < kCascades; ++k) {
            if (g_vk_sh.tex_layers[k] != VK_NULL_HANDLE) vkDestroyImageView(dev, g_vk_sh.tex_layers[k], nullptr);
        }
        for (int i = 0; i < 6 * kPtShadows; ++i) {
            if (g_vk_sh.ptex_faces[i] != VK_NULL_HANDLE) vkDestroyImageView(dev, g_vk_sh.ptex_faces[i], nullptr);
        }
        for (VkPipeline p : {g_vk_sh.ao_pipe, g_vk_sh.blur_pipe, g_vk_sh.blur_sep_h_pipe, g_vk_sh.blur_sep_v_pipe, g_vk_sh.blur_nd_pipe, g_vk_sh.blur_sep_v_nd_pipe,
                             g_vk_sh.rays_mask_pipe, g_vk_sh.rays_blur_pipe, g_vk_sh.rays_split_pipe, g_vk_sh.rays_add_pipe}) {
            if (p != VK_NULL_HANDLE) vkDestroyPipeline(dev, p, nullptr);
        }
        for (VkPipelineLayout pl : {g_vk_sh.ao_layout, g_vk_sh.blur_layout, g_vk_sh.rays_mask_layout,
                                    g_vk_sh.rays_blur_layout, g_vk_sh.rays_split_layout, g_vk_sh.rays_add_layout}) {
            if (pl != VK_NULL_HANDLE) vkDestroyPipelineLayout(dev, pl, nullptr);
        }
        if (g_vk_sh.ao_stencil_view != VK_NULL_HANDLE) vkDestroyImageView(dev, g_vk_sh.ao_stencil_view, nullptr);
        g_vk_sh.ao_stencil_view = VK_NULL_HANDLE;
        if (g_vk_sh.ao_sdepth_stencil != VK_NULL_HANDLE) vkDestroyImageView(dev, g_vk_sh.ao_sdepth_stencil, nullptr);
        g_vk_sh.ao_sdepth_stencil = VK_NULL_HANDLE;
        for (VkDescriptorSetLayout dsl : {g_vk_sh.set_layout, g_vk_sh.samp1_layout, g_vk_sh.samp2_layout, g_vk_sh.samp3_layout}) {
            if (dsl != VK_NULL_HANDLE) vkDestroyDescriptorSetLayout(dev, dsl, nullptr);
        }
        for (VkShaderModule sm : {g_vk_sh.vs_mod, g_vk_sh.ao_fs_mod, g_vk_sh.blur_fs_mod, g_vk_sh.blur_sep_fs_mod,
                                  g_vk_sh.rays_mask_fs_mod, g_vk_sh.rays_blur_fs_mod, g_vk_sh.rays_split_fs_mod, g_vk_sh.rays_add_fs_mod}) {
            if (sm != VK_NULL_HANDLE) vkDestroyShaderModule(dev, sm, nullptr);
        }
    }
    vk::destroy_image_now(g_vk_sh.tex);
    for (int k = 0; k < kCascades; ++k) vk::destroy_image_now(g_vk_sh.stex[k]);
    vk::destroy_image_now(g_vk_sh.ptex);
    vk::destroy_image_now(g_vk_sh.tiles);
    vk::destroy_image_now(g_vk_sh.ao_dtex);
    vk::destroy_image_now(g_vk_sh.ao_atex);
    vk::destroy_image_now(g_vk_sh.ao_btex);
    vk::destroy_image_now(g_vk_sh.rays_dtex);
    vk::destroy_image_now(g_vk_sh.rays_ctex);
    vk::destroy_image_now(g_vk_sh.rays_mtex);
    vk::destroy_image_now(g_vk_sh.rays_rtex);
    g_vk_sh = VkLighting{};
}

void ensure_vk_lighting_init() {
    if (g_vk_sh.inited || !vk::is_active()) return;
    g_vk_sh.inited = true;
    vk::register_shutdown_callback(vk_lighting_shutdown);
}

bool ensure_resources() {
    static const int sizes[3] = {1024, 2048, 4096};
    const int want = sizes[std::clamp(g_shadow_quality, 0, 2)];
    if (vk::is_active()) {
        ensure_vk_lighting_init();
        if (g_vk_sh.tex.image != VK_NULL_HANDLE && g_vk_sh.res == want) {
            g_sh.res = want;
            return true;
        }
        VkDevice dev = vk::device();
        // (a quality change: the frames in flight may still draw into the old cascades' views, destroyed at once below)
        if (g_vk_sh.tex_layers[0] != VK_NULL_HANDLE) vk::wait_idle();
        for (int k = 0; k < kCascades; ++k) {
            if (g_vk_sh.tex_layers[k] != VK_NULL_HANDLE) {
                vkDestroyImageView(dev, g_vk_sh.tex_layers[k], nullptr);
                g_vk_sh.tex_layers[k] = VK_NULL_HANDLE;
            }
        }
        vk::destroy_image(g_vk_sh.tex);
        vk::create_image(static_cast<uint32_t>(want), static_cast<uint32_t>(want), VK_FORMAT_D32_SFLOAT,
                         VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT |
                             VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT, g_vk_sh.tex, kCascades, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        for (int k = 0; k < kCascades; ++k) {
            VkImageViewCreateInfo vci{};
            vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vci.image = g_vk_sh.tex.image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = VK_FORMAT_D32_SFLOAT;
            vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
            vci.subresourceRange.baseMipLevel = 0;
            vci.subresourceRange.levelCount = 1;
            vci.subresourceRange.baseArrayLayer = static_cast<uint32_t>(k);
            vci.subresourceRange.layerCount = 1;
            vkCreateImageView(dev, &vci, nullptr, &g_vk_sh.tex_layers[k]);
        }
        vk::transition_image(vk::cmd(), g_vk_sh.tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        g_vk_sh.res = want;
        g_sh.res = want;
        return true;
    }
    if (g_sh.fbo != 0 && g_sh.res == want) return true;
    if (g_sh.fbo == 0) {
        glGenFramebuffers(1, &g_sh.fbo);
        glGenTextures(1, &g_sh.tex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_sh.tex, "Shadows: sun cascades");
    }
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_sh.tex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT32F, want, want, kCascades, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
    glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
    g_sh.res = want;
    return true;
}

// orig: (host) the cascades fitted to the view's slices: per slice its bounding sphere (stable under the camera's rotation), the
// light's orthographic box around it reaching `back` units toward the sun for the casters, its centre snapped to the map's texels
// (no shimmering when the camera moves)
void fit_cascades() {
    const glm::mat4 view = render_view_matrix();
    float vpf[16];
    mrp_world_view_proj(vpf);
    const glm::dmat4 vp = glm::make_mat4(vpf);
    const glm::dmat4 proj = vp * glm::inverse(glm::dmat4(view));
    g_sh.proj = glm::vec2(static_cast<float>(proj[0][0]), static_cast<float>(proj[1][1]));
    g_sh.view_proj = glm::mat4(vp);
    g_sh.inv_view = glm::mat4(glm::inverse(glm::dmat4(view)));
    g_sh.cam = glm::vec3(g_sh.inv_view[3]);
    {  // the zone's sun: turned toward a new zone's at most 1 degree a frame, its shadows' weight faded (1 / 30 a frame)
        float tw = 0.0f;
        glm::vec4 tkey(0.0f);
        glm::vec3 tkey_dir(0.0f, 0.0f, 1.0f);
        const glm::vec3 target = sun_direction(&tw, &tkey, &tkey_dir);
        const float zone_w = tw;
        tw *= view_open();  // (testing, RAC1_SUN_COVER=1: a view covered from the sun - its shadows faded out, the cascades still drawn)
        {  // the level's interiors: the camera in one of its boxes, or the hero lit with one of its light sets
            user_ensure();
            bool inside = false;
            for (const SunInterior& b : g_interiors)
                inside = inside || (g_sh.cam.x >= b.lo[0] && g_sh.cam.y >= b.lo[1] && g_sh.cam.z >= b.lo[2] && g_sh.cam.x <= b.hi[0] && g_sh.cam.y <= b.hi[1] &&
                                    g_sh.cam.z <= b.hi[2]);
            g_interior_hero_set = -1;
            if (const uint32_t hero = draw_lwu(OV(0x0015ffd8u)); hero != 0) g_interior_hero_set = draw_lbu(hero + 0x38);
            for (int n : g_interior_sets) inside = inside || n == g_interior_hero_set;
            static int32_t ilvl = -1;
            const float want = inside ? 1.0f : 0.0f;
            if (draw_lw(0x0015ed84) != ilvl) {
                ilvl = draw_lw(0x0015ed84);
                g_interior_k = want;
            } else {
                g_interior_k += std::clamp(want - g_interior_k, -1.0f / kInteriorFade, 1.0f / kInteriorFade);
            }
        }
        static glm::vec3 cur{0.0f, 0.0f, 1.0f};
        static float curw = 0.0f;
        static int32_t lvl = -1;
        const int32_t level = draw_lw(0x0015ed84);
        static glm::vec4 curkey{0.0f};
        static glm::vec3 curkey_dir{0.0f, 0.0f, 1.0f};
        if (level != lvl) {
            lvl = level;
            cur = target;
            curw = tw;
            curkey = tkey;
            curkey_dir = tkey_dir;
        } else if (g_sun_snap) {  // (the debug menu's sun editor: at once)
            cur = target;
            curw = tw;
            g_sun_snap = false;
        } else {
            curkey += (tkey - curkey) * 0.1f;  // (a new zone's key light's colour and direction: over about half a second)
            const glm::vec3 kd = curkey_dir + (tkey_dir - curkey_dir) * 0.1f;
            if (glm::dot(kd, kd) > 1e-6f) curkey_dir = glm::normalize(kd);
            curw += std::clamp(tw - curw, -1.0f / 30.0f, 1.0f / 30.0f);
            if (curw <= 0.02f) {
                cur = target;  // (unseen: at once)
            } else {
                const float c = std::clamp(glm::dot(cur, target), -1.0f, 1.0f), ang = std::acos(c), step = glm::radians(1.0f);
                if (ang <= step) cur = target;
                else cur = glm::normalize(cur * std::sin(ang - step) / std::sin(ang) + target * std::sin(step) / std::sin(ang));
            }
        }
        g_sh.sun = cur;
        g_sh.sun_k = curw * (1.0f - g_interior_k);  // (an interior: faded out apart from the zone's own fade)
        g_sh.key = curkey;
        g_sh.key_dir = curkey_dir;
        g_has_sun.store(g_sh.sun_k > 0.01f);
        g_casc_on.store(g_sh.sun_k > 0.01f || (cover_probe_on() && zone_w > 0.01f));
    }
    for (int i = 0; i < 3; ++i) g_cull_sun[i] = g_sh.sun[i];  // (the casters' search of the next frames: the level's sun is fixed)
    g_cull_tan[0] = 1.0f / g_sh.proj.x;
    g_cull_tan[1] = 1.0f / g_sh.proj.y;
    g_sun_known.store(true);
    tfrag_light_trace();
    const glm::vec3 up = std::fabs(g_sh.sun.z) > 0.95f ? glm::vec3(1.0f, 0.0f, 0.0f) : glm::vec3(0.0f, 0.0f, 1.0f);
    const float tx = 1.0f / g_sh.proj.x, ty = 1.0f / g_sh.proj.y;
    float near_d = 0.1f;
    // the caches emptied when what they hold may have changed
    uint64_t key = static_cast<uint64_t>(draw_lw(0x0015ed84)) * 1000003u + static_cast<uint64_t>(g_mrp_lod) * 7919u + static_cast<uint64_t>(g_sh.res);
    for (int i = 0; i < 3; ++i) {
        uint32_t u;
        std::memcpy(&u, &g_sh.sun[i], 4);
        key = key * 31u + u;
    }
    if (key != g_sh.cache_key || g_shadow_cache == 0) {
        g_sh.cache_key = key;
        for (auto& ca : g_sh.cache) ca.valid = false;
    }
    bool want_next = g_shadow_cache == 0;
    for (int k = 0; k < kCascades; ++k) {
        const float far_d = g_sh.splits[k];
        // the slice's corners in view space (z = -d), to the world
        glm::vec3 c(0.0f);
        glm::vec3 pts[8];
        int n = 0;
        for (float d : {near_d, far_d})
            for (int sy = -1; sy <= 1; sy += 2)
                for (int sx = -1; sx <= 1; sx += 2) {
                    pts[n] = glm::vec3(g_sh.inv_view * glm::vec4(sx * d * tx, sy * d * ty, -d, 1.0f));
                    c += pts[n++];
                }
        c /= 8.0f;
        float r = 0.0f;
        for (const glm::vec3& p : pts) r = std::max(r, glm::length(p - c));
        r = std::ceil(r * 1.05f);  // (whole units: the box's size does not change with the rotation)
        const glm::mat4 lv0 = glm::lookAt(glm::vec3(0.0f), -g_sh.sun, up);
        const int cres = cascade_res(k);
        const glm::vec3 lc0 = glm::vec3(lv0 * glm::vec4(c, 1.0f));
        float R = r, back = 0.0f;
        glm::vec3 lc = lc0;
        g_sh.refresh[k] = true;
        if (g_shadow_cache != 0) {  // the box kept while the slice's sphere stays inside its own
            State::Cache& ca = g_sh.cache[k];
            R = std::ceil(r * kCacheGrow);
            const float slack = R - (glm::length(lc0 - ca.lc) + r);
            if (ca.valid && ca.res == cres && ca.R == R && slack >= 0.0f && !(ca.incomplete && mrp_world_casters_built())) {
                g_sh.refresh[k] = false;
                lc = ca.lc;
                back = ca.back;
            }
            // (the casters wanted next frame when the box may be left by then: within a few frames' moves of its edge)
            const float speed = ca.valid ? glm::length(lc0 - ca.prev) : R;
            ca.prev = lc0;
            want_next = want_next || !ca.valid || ca.incomplete || slack < 4.0f * speed + 0.02f * R;
        }
        const float texel = 2.0f * R / static_cast<float>(cres);
        if (g_sh.refresh[k]) {
            // the casters toward the sun: up to the cut plane (kBack from the camera along the sun, uShCut), which does not move when
            // the camera turns - the box reaches past it (a cached box: past it wherever the camera goes while the box is kept)
            back = R + kBack + glm::length(c - g_sh.cam) + (g_shadow_cache != 0 ? R : 0.0f);
            // the centre snapped to the texels, in the light's frame
            lc.x = std::floor(lc.x / texel) * texel;
            lc.y = std::floor(lc.y / texel) * texel;
            if (g_shadow_cache != 0) {
                State::Cache& ca = g_sh.cache[k];
                ca.valid = true;
                ca.lc = lc;
                ca.R = R;
                ca.back = back;
                ca.res = cres;
            }
        }
        const glm::mat4 lv = glm::translate(glm::mat4(1.0f), -lc) * lv0;  // (the light's frame centred on the slice)
        const glm::mat4 lp = glm::ortho(-R, R, -R, R, -back, R);
        Cascade& C = g_sh.casc[k];
        C.vp = lp * lv;
        const glm::mat4 bias = glm::translate(glm::mat4(1.0f), glm::vec3(0.5f)) * glm::scale(glm::mat4(1.0f), glm::vec3(0.5f));
        const float part = static_cast<float>(cres) / static_cast<float>(g_sh.res);  // (the corner [0, part] of the layer)
        C.tex = bias * C.vp;
        C.part = part;
        C.res = cres;
        C.texel = texel;
        C.range = back + R;
        near_d = far_d * 0.9f;  // (the slices overlap a little: the blend band)
    }
    g_sh.want = want_next;
    if (!g_sh.logged) {
        g_sh.logged = true;
        if (!g_has_sun.load()) std::printf("[shadows] on: no sun (the level's key light comes from below or the horizon): no sun's shadows\n");
        else std::printf("[shadows] on: sun (%.3f %.3f %.3f), cascades %.0f / %.0f / %.0f units at %d px (texel %.3f / %.3f / %.3f)\n", g_sh.sun.x, g_sh.sun.y, g_sh.sun.z, g_sh.splits[0],
                    g_sh.splits[1], g_sh.splits[2], g_sh.res, g_sh.casc[0].texel, g_sh.casc[1].texel, g_sh.casc[2].texel);
    }
}
}  // namespace

namespace {
// orig: (host) the point lights per screen tile: each light's sphere (its bounding box's corners) projected by the view, the
// tiles of its screen rectangle given its bit (the whole screen when a corner is behind the camera); the fragment shader goes through
// its tile's lights alone (sh_points)
void point_tiles_update() {
    static std::vector<uint32_t> m(static_cast<size_t>(kTilesX) * kTilesY * 4);
    std::fill(m.begin(), m.end(), 0u);
    static const bool all = std::getenv("RAC1_POINT_TILES") != nullptr && std::atoi(std::getenv("RAC1_POINT_TILES")) == 0;  // (testing: every light everywhere)
    for (int i = 0; i < g_sh.npts && i < kMaxPts; ++i) {
        const glm::vec3 p(g_sh.pt_pos[i]);
        const float r = g_sh.pt_pos[i].w;
        const float wc = (g_sh.view_proj * glm::vec4(p, 1.0f)).w;
        if (!all && wc + r < 0.05f) continue;
        const glm::vec3 dcam = p - g_sh.cam;
        const bool inside_cam = all || glm::dot(dcam, dcam) <= (r + 0.15f) * (r + 0.15f);
        int tx0 = 0, tx1 = kTilesX - 1, ty0 = 0, ty1 = kTilesY - 1;
        if (!inside_cam) {
            constexpr float kNearW = 0.05f;
            glm::vec4 q[8];
            int nfront = 0;
            float x0 = 1e30f, y0 = 1e30f, x1 = -1e30f, y1 = -1e30f;
            for (int c = 0; c < 8; ++c) {
                q[c] = g_sh.view_proj * glm::vec4(p + glm::vec3((c & 1) ? r : -r, (c & 2) ? r : -r, (c & 4) ? r : -r), 1.0f);
                if (q[c].w >= kNearW) {
                    ++nfront;
                    const float inv_w = 1.0f / q[c].w;
                    x0 = std::min(x0, q[c].x * inv_w);
                    x1 = std::max(x1, q[c].x * inv_w);
                    y0 = std::min(y0, q[c].y * inv_w);
                    y1 = std::max(y1, q[c].y * inv_w);
                }
            }
            if (nfront == 0) continue;
            if (nfront < 8) {
                // Clip the 12 AABB edges that straddle the w = kNearW plane
                for (int a = 0; a < 8; ++a) {
                    for (int bit = 1; bit <= 4; bit <<= 1) {
                        const int b = a ^ bit;
                        if (a < b && (q[a].w >= kNearW) != (q[b].w >= kNearW)) {
                            const float t = (kNearW - q[a].w) / (q[b].w - q[a].w);
                            const float cx = (q[a].x + t * (q[b].x - q[a].x)) * (1.0f / kNearW);
                            const float cy = (q[a].y + t * (q[b].y - q[a].y)) * (1.0f / kNearW);
                            x0 = std::min(x0, cx);
                            x1 = std::max(x1, cx);
                            y0 = std::min(y0, cy);
                            y1 = std::max(y1, cy);
                        }
                    }
                }
            }
            if (x1 < -1.0f || x0 > 1.0f || y1 < -1.0f || y0 > 1.0f) continue;  // (off the screen)
            tx0 = std::clamp(static_cast<int>(std::floor((x0 * 0.5f + 0.5f) * kTilesX)), 0, kTilesX - 1);
            tx1 = std::clamp(static_cast<int>(std::floor((x1 * 0.5f + 0.5f) * kTilesX)), 0, kTilesX - 1);
            ty0 = std::clamp(static_cast<int>(std::floor((y0 * 0.5f + 0.5f) * kTilesY)), 0, kTilesY - 1);
            ty1 = std::clamp(static_cast<int>(std::floor((y1 * 0.5f + 0.5f) * kTilesY)), 0, kTilesY - 1);
        }
        for (int ty = ty0; ty <= ty1; ++ty)
            for (int tx = tx0; tx <= tx1; ++tx) m[static_cast<size_t>(ty * kTilesX + tx) * 4 + (i >> 5)] |= 1u << (i & 31);
    }
    if (vk::is_active()) {
        if (g_vk_sh.tiles.image == VK_NULL_HANDLE) {
            vk::create_image(kTilesX, kTilesY, VK_FORMAT_R32G32B32A32_UINT,
                             VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, g_vk_sh.tiles);
        }
        vk::upload_image_2d(g_vk_sh.tiles, m.data(), m.size() * 4);
        return;
    }
    if (g_sh.tiles == 0) {
        glGenTextures(1, &g_sh.tiles);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_sh.tiles, "Point lights: tiles");
        glBindTexture(GL_TEXTURE_2D, g_sh.tiles);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA32UI, kTilesX, kTilesY, 0, GL_RGBA_INTEGER, GL_UNSIGNED_INT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    }
    glBindTexture(GL_TEXTURE_2D, g_sh.tiles);
    glBindBuffer(GL_PIXEL_UNPACK_BUFFER, 0);
    glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
    glPixelStorei(GL_UNPACK_ROW_LENGTH, 0);
    glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, kTilesX, kTilesY, GL_RGBA_INTEGER, GL_UNSIGNED_INT, m.data());
}

// orig: (host) the point lights' shadows: the nearest lit ones (up to kPtShadows, within the cascades' reach) each drawn into a
// cube of depth maps (6 views of 90 degrees from the light, near kPtNear, far its radius) with the frame's casters, world and mobys;
// the light's pt_col.w names its cube (the fragment shader's sh_points compares against it). The shadow pass's framebuffer bound.
void point_shadows_pass() {
    if (g_point_shadows == 0 || g_modern_points == 0 || g_sh.npts == 0) return;
    if (vk::is_active()) {
        if (g_vk_sh.ptex.image == VK_NULL_HANDLE) {
            vk::create_image(kPtRes, kPtRes, VK_FORMAT_D32_SFLOAT,
                             VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_DEPTH_BIT, g_vk_sh.ptex, 6 * kPtShadows,
                             VK_IMAGE_CREATE_CUBE_COMPATIBLE_BIT, VK_IMAGE_VIEW_TYPE_CUBE_ARRAY);
            VkDevice dev = vk::device();
            for (int l = 0; l < 6 * kPtShadows; ++l) {
                VkImageViewCreateInfo vci{};
                vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                vci.image = g_vk_sh.ptex.image;
                vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
                vci.format = VK_FORMAT_D32_SFLOAT;
                vci.subresourceRange.aspectMask = VK_IMAGE_ASPECT_DEPTH_BIT;
                vci.subresourceRange.baseMipLevel = 0;
                vci.subresourceRange.levelCount = 1;
                vci.subresourceRange.baseArrayLayer = static_cast<uint32_t>(l);
                vci.subresourceRange.layerCount = 1;
                vkCreateImageView(dev, &vci, nullptr, &g_vk_sh.ptex_faces[l]);
            }
            vk::transition_image(vk::cmd(), g_vk_sh.ptex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        }
    } else if (g_sh.ptex == 0) {
        glGenTextures(1, &g_sh.ptex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_sh.ptex, "Shadows: point cubes");
        glBindTexture(kGlCubeMapArray, g_sh.ptex);
        glTexImage3D(kGlCubeMapArray, 0, GL_DEPTH_COMPONENT32F, kPtRes, kPtRes, 6 * kPtShadows, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_COMPARE_MODE, GL_COMPARE_REF_TO_TEXTURE);
        glTexParameteri(kGlCubeMapArray, GL_TEXTURE_COMPARE_FUNC, GL_LEQUAL);
        glBindTexture(kGlCubeMapArray, 0);
    }
    // the nearest to the camera first
    int order[kMaxPts];
    for (int i = 0; i < g_sh.npts; ++i) order[i] = i;
    std::sort(order, order + g_sh.npts, [](int a, int b) {
        return glm::length(glm::vec3(g_sh.pt_pos[a]) - g_sh.cam) - g_sh.pt_pos[a].w < glm::length(glm::vec3(g_sh.pt_pos[b]) - g_sh.cam) - g_sh.pt_pos[b].w;
    });
    // the faces as the cube map's sampling orders them (+x, -x, +y, -y, +z, -z: OpenGL's targets and up vectors)
    static const glm::vec3 dirs[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    static const glm::vec3 ups[6] = {{0, -1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}, {0, -1, 0}, {0, -1, 0}};
    // the cubes: the game's lights first (the frame's casters sorted for their slots), then the automatic emitters with a moby inside
    // their reach, the nearest to one (relative to the reach) first - their cube the mobys' alone: a merged emitter can sit inside its
    // lamp, the lamp's own shape would put it out -. An emitter's shadow fades in when it gets a cube and out when it loses it (it
    // keeps the cube while fading), so no shadow appears or goes at once.
    int pick[kPtShadows], npick = 0;
    for (int oi = 0; oi < g_sh.npts && npick < kPtShadows; ++oi)
        if (g_sh.pt_slot[order[oi]] >= 0 && glm::length(glm::vec3(g_sh.pt_pos[order[oi]]) - g_sh.cam) - g_sh.pt_pos[order[oi]].w <= g_sh.splits[1]) pick[npick++] = order[oi];
    const int game_cubes = npick;
    struct Fading {
        glm::vec3 pos{0.0f};
        float k = 0.0f;
    };
    static Fading fade[kPtShadows];
    static int nfade = 0;
    constexpr float kStep = 0.125f;  // (8 frames in, 8 out)
    if (g_auto_lights != 0) {
        // the wanted: the emitters with a moby part within their reach, by the part's distance over the reach
        std::pair<float, int> want[kMaxPts];
        int nwant = 0;
        for (int i = 0; i < g_sh.npts; ++i) {
            if (g_sh.pt_slot[i] >= 0 || g_sh.pt_slot[i] == -2) continue;  // (-2: a moby's own lamp - its mesh would hide it)
            const float p[3] = {g_sh.pt_pos[i].x, g_sh.pt_pos[i].y, g_sh.pt_pos[i].z};
            const float d = mrp_moby_nearest_part(p) / g_sh.pt_pos[i].w;
            if (d < 1.0f) want[nwant++] = {d, i};
        }
        std::sort(want, want + nwant);
        const int room = kPtShadows - game_cubes;
        nwant = std::min(nwant, room);
        auto find = [](const glm::vec3& q) {
            for (int i = 0; i < g_sh.npts; ++i)
                if (g_sh.pt_slot[i] < 0 && glm::vec3(g_sh.pt_pos[i]) == q) return i;
            return -1;
        };
        // the ones fading: up if still wanted, down if not (gone at 0, or when the light left the frame's list)
        int kept = 0;
        for (int j = 0; j < nfade; ++j) {
            bool wanted = false;
            for (int w = 0; w < nwant; ++w) wanted = wanted || glm::vec3(g_sh.pt_pos[want[w].second]) == fade[j].pos;
            fade[j].k = std::clamp(fade[j].k + (wanted ? kStep : -kStep), 0.0f, 1.0f);
            if (fade[j].k > 0.0f && find(fade[j].pos) >= 0) fade[kept++] = fade[j];
        }
        nfade = std::min(kept, room);  // (a game light taking a cube: the weakest dropped)
        for (int w = 0; w < nwant && nfade < room; ++w) {
            const glm::vec3 q(g_sh.pt_pos[want[w].second]);
            bool in = false;
            for (int j = 0; j < nfade; ++j) in = in || fade[j].pos == q;
            if (!in) fade[nfade++] = Fading{q, kStep};
        }
        for (int j = 0; j < nfade && npick < kPtShadows; ++j) pick[npick++] = find(fade[j].pos);
    } else {
        nfade = 0;
    }
    int cube = 0;
    if (!vk::is_active()) {
        glViewport(0, 0, kPtRes, kPtRes);
        glDepthMask(GL_TRUE);
        glEnable(GL_DEPTH_TEST);
        glDepthFunc(GL_LESS);
    } else {
        vk::transition_image(vk::cmd(), g_vk_sh.ptex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    }
    for (int pi = 0; pi < npick; ++pi) {
        const int i = pick[pi];
        const glm::vec3 lp(g_sh.pt_pos[i]);
        const float r = g_sh.pt_pos[i].w;
        if (glm::length(lp - g_sh.cam) - r > g_sh.splits[1]) continue;  // (beyond the middle cascade: not worth a cube)
        const glm::mat4 proj = glm::perspective(glm::radians(90.0f), 1.0f, kPtNear, r);
        for (int f = 0; f < 6; ++f) {
            if (vk::is_active()) {
                vk::RenderTarget rt{};
                rt.depth_view = g_vk_sh.ptex_faces[6 * cube + f];
                rt.depth_format = VK_FORMAT_D32_SFLOAT;
                rt.width = kPtRes;
                rt.height = kPtRes;
                vk::set_viewport(0, 0, kPtRes, kPtRes);
                vk::set_scissor(false);
                vk::push_target(rt, false, nullptr, true, 1.0f, false, 0);
            } else {
                glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_sh.ptex, 0, 6 * cube + f);
                glClear(GL_DEPTH_BUFFER_BIT);
            }
            // cull faces whose 90-degree pyramid (apex lp, base corners at distance r) lies outside a camera frustum plane
            const glm::vec3 right = glm::cross(dirs[f], ups[f]);
            glm::vec4 c[5];
            c[0] = g_sh.view_proj * glm::vec4(lp, 1.0f);
            c[1] = g_sh.view_proj * glm::vec4(lp + r * (dirs[f] - right - ups[f]), 1.0f);
            c[2] = g_sh.view_proj * glm::vec4(lp + r * (dirs[f] + right - ups[f]), 1.0f);
            c[3] = g_sh.view_proj * glm::vec4(lp + r * (dirs[f] - right + ups[f]), 1.0f);
            c[4] = g_sh.view_proj * glm::vec4(lp + r * (dirs[f] + right + ups[f]), 1.0f);
            int out_l = 0, out_r = 0, out_b = 0, out_t = 0, out_n = 0, out_f = 0;
            for (int v = 0; v < 5; ++v) {
                out_l += c[v].x < -c[v].w ? 1 : 0;
                out_r += c[v].x >  c[v].w ? 1 : 0;
                out_b += c[v].y < -c[v].w ? 1 : 0;
                out_t += c[v].y >  c[v].w ? 1 : 0;
                out_n += c[v].z < -c[v].w ? 1 : 0;
                out_f += c[v].z >  c[v].w ? 1 : 0;
            }
            if (!(out_l == 5 || out_r == 5 || out_b == 5 || out_t == 5 || out_n == 5 || out_f == 5)) {
                const glm::mat4 vp = proj * glm::lookAt(lp, lp + dirs[f], ups[f]);
                if (g_sh.pt_slot[i] >= 0) mrp_world_draw_casters(glm::value_ptr(vp), 3 + g_sh.pt_slot[i]);
                mrp_moby_draw_casters(glm::value_ptr(vp), glm::value_ptr(g_sh.pt_pos[i]));
            }
            if (vk::is_active()) vk::pop_target();
        }
        float k = 1.0f;  // (an emitter's shadow fading: its weight with the cube's index, pt_col.w = cube + 0.05 + 0.9 k)
        for (int j = 0; j < nfade; ++j)
            if (g_sh.pt_slot[i] < 0 && fade[j].pos == lp) k = fade[j].k;
        g_sh.pt_col[i].w = static_cast<float>(cube) + 0.05f + 0.9f * k;
        ++cube;
    }
    if (vk::is_active() && g_vk_sh.ptex.image != VK_NULL_HANDLE) {
        vk::transition_image(vk::cmd(), g_vk_sh.ptex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    }
}
}  // namespace

// orig: (host) see mrp_lighting.h
bool mrp_lighting_active() { return g_modern_shadows != 0 && g_sh.on; }

// orig: (host) see mrp_lighting.h
namespace {
// the game's point lights' ages: a light lit for more than kOldLight frames in a row (Blarg's purple lamp, lit from the
// level's start) is the level's own - the game's vertex colours carry it, as on the PS2: it is neither lit per pixel nor holds the
// colours; a short one (an explosion, a shot) is lit per pixel, the colours held meanwhile
constexpr uint64_t kOldLight = 90;
std::atomic<uint32_t> g_slot_old{0};  // bit i: slot i's light is the level's own
uint32_t g_slot_id[8] = {};           // its use record's +0x10 (0: free)
uint64_t g_slot_since[8] = {}, g_freeze_calls = 0;
}  // namespace

bool mrp_lighting_points_freeze() {
    if (g_modern_shadows == 0 || g_modern_points == 0) return false;
    ++g_freeze_calls;
    bool young = false;
    uint32_t old = 0;
    for (uint32_t i = 0; i < 8; ++i) {
        const uint32_t id = draw_lwu(OV(0x001804c0u) + i * 0x30 + 0x10);
        if (id == 0) {
            g_slot_id[i] = 0;
            continue;
        }
        if (id != g_slot_id[i]) {
            g_slot_id[i] = id;
            g_slot_since[i] = g_freeze_calls;
        }
        if (g_freeze_calls - g_slot_since[i] >= kOldLight) old |= 1u << i;
        else young = true;
    }
    g_slot_old.store(old);
    return young;
}

// ---- ambient occlusion -----------------------------------------------------------------------------------------------
//
// Screen space, from the 3D's depth alone: the framebuffer's depth copied (a blit into a texture of its format), per pixel its world
// position (the world's view-projection inverted), its face normal (the positions' derivatives), 12 points of a hemisphere around it
// (0.9 units, rotated per pixel) projected: each one behind the depth stored there (within reach) occludes. The result (R8) blurred
// within the surface (5 x 5, the depths compared) and multiplied into the colour (RGB; the sky - depth 1 - untouched).
namespace {
const char* kAoVs = R"(#version 460 core
out vec2 vUv;
void main() {
    vec2 p = vec2((gl_VertexID & 1) * 4.0 - 1.0, (gl_VertexID & 2) * 2.0 - 1.0);
    vUv = p * 0.5 + 0.5;
    gl_Position = vec4(p, 0.0, 1.0);
}
)";
const char* kAoFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uDepth;
uniform mat4 uInvVP, uVP;
uniform vec3 uCam;
uniform vec4 uParam;  // radius (units), strength, bias, mode (1=SSAO, 2=SSAO+, 3=HBAO, 4=GTAO)
out float FragAo;
vec3 world(vec2 uv, float d) {
    vec4 p = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return p.xyz / p.w;
}
void main() {
    float d = texture(uDepth, vUv).r;
    vec3 P = world(vUv, d);
    vec3 dPdx = dFdx(P), dPdy = dFdy(P);  // (before any return: derivatives in non-uniform control flow are undefined)
    if (d >= 1.0) { FragAo = 1.0; return; }
    int mode = int(uParam.w + 0.5);
    vec3 N;
    if (mode <= 1) {
        N = cross(dPdx, dPdy);
    } else {
        vec2 texel = 1.0 / vec2(textureSize(uDepth, 0));
        float dr = texture(uDepth, vUv + vec2(texel.x, 0.0)).r;
        float dl = texture(uDepth, vUv - vec2(texel.x, 0.0)).r;
        float du = texture(uDepth, vUv + vec2(0.0, texel.y)).r;
        float dd = texture(uDepth, vUv - vec2(0.0, texel.y)).r;
        vec3 dPx = (abs(dr - d) < abs(d - dl) && dr < 1.0) ? (world(vUv + vec2(texel.x, 0.0), dr) - P)
                                                            : (dl < 1.0 ? (P - world(vUv - vec2(texel.x, 0.0), dl)) : dPdx);
        vec3 dPy = (abs(du - d) < abs(d - dd) && du < 1.0) ? (world(vUv + vec2(0.0, texel.y), du) - P)
                                                            : (dd < 1.0 ? (P - world(vUv - vec2(0.0, texel.y), dd)) : dPdy);
        N = cross(dPx, dPy);
    }
    if (dot(N, N) < 1e-12) { FragAo = 1.0; return; }
    N = normalize(N);
    vec3 V = uCam - P;
    float vlen = length(V);
    V /= max(vlen, 1e-5);
    if (dot(N, V) < 0.0) N = -N;
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    float noise2 = fract(52.9829189 * fract(dot(gl_FragCoord.xy + vec2(17.0, 31.0), vec2(0.06711056, 0.00583715))));
    vec3 t = normalize(abs(N.z) < 0.9 ? cross(N, vec3(0.0, 0.0, 1.0)) : cross(N, vec3(1.0, 0.0, 0.0)));
    vec3 b = cross(N, t);
    float R = uParam.x;
    {  // (the reach capped on the screen: a surface near the camera looks no further than 4 % of the view's height - the taps
       // far apart on the screen were most of the cost, for an occlusion that hardly changes)
        vec4 c0 = uVP * vec4(P, 1.0), c1 = uVP * vec4(P + b * R, 1.0);
        if (c0.w > 0.0 && c1.w > 0.0) {
            float px = length(c1.xy / c1.w - c0.xy / c0.w) * 0.5;
            if (px > 0.04) R *= 0.04 / px;
        }
    }
    float invR2 = 1.0 / max(R * R, 1e-4);

    if (mode <= 2) {
        float a = 6.2831853 * noise;
        vec3 T = t * cos(a) + b * sin(a), B = cross(N, T);
        float occ = 0.0;
        const int K = 12;
        for (int i = 0; i < K; ++i) {
            float fi = (float(i) + 0.5) / float(K);
            float z = sqrt(1.0 - fi);
            float r = sqrt(fi), ph = float(i) * 2.3999632;
            vec3 h = T * (r * cos(ph)) + B * (r * sin(ph)) + N * z;
            float s = mix(0.15, 1.0, fi * fi) * R;
            vec3 S = P + N * uParam.z + h * s;
            vec4 c = uVP * vec4(S, 1.0);
            if (c.w <= 0.0) continue;
            vec2 suv = c.xy / c.w * 0.5 + 0.5;
            if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) continue;
            float sd = texture(uDepth, suv).r;
            float zs = c.z / c.w * 0.5 + 0.5;
            if (sd < zs - 0.00002) {
                float dist = length(world(suv, sd) - S);
                occ += smoothstep(0.0, 1.0, R * 1.5 / max(dist, 1e-3) - 0.5);
            }
        }
        FragAo = 1.0 - uParam.y * occ / float(K);
        return;
    }

    if (mode == 3) {
        // HBAO: 4 directions x 3 steps (12 taps), horizon angle sin(h) with quadratic distance attenuation
        const int D = 4, S = 3;
        float occ = 0.0;
        for (int d_i = 0; d_i < D; ++d_i) {
            float phi = (float(d_i) + noise) * (6.2831853 / float(D));
            vec3 dir = t * cos(phi) + b * sin(phi);
            float max_sin_h = 0.12;
            float best_att = 0.0;
            for (int s_i = 1; s_i <= S; ++s_i) {
                float frac_r = (float(s_i) - 0.5 + 0.5 * noise2) / float(S);
                vec3 Q = P + N * uParam.z + dir * (frac_r * R);
                vec4 c = uVP * vec4(Q, 1.0);
                if (c.w <= 0.0) continue;
                vec2 suv = c.xy / c.w * 0.5 + 0.5;
                if (any(lessThan(suv, vec2(0.0))) || any(greaterThan(suv, vec2(1.0)))) continue;
                float sd = texture(uDepth, suv).r;
                if (sd >= 1.0) continue;
                vec3 H = world(suv, sd) - P;
                float d2 = dot(H, H);
                if (d2 < 1e-5 || d2 > R * R * 1.44) continue;
                float sin_h = dot(H, N) * inversesqrt(d2);
                if (sin_h > max_sin_h) {
                    float att = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                    best_att += (sin_h - max_sin_h) * att;
                    max_sin_h = sin_h;
                }
            }
            occ += clamp(best_att, 0.0, 1.0);
        }
        FragAo = clamp(1.0 - uParam.y * 1.25 * (occ / float(D)), 0.0, 1.0);
        return;
    }

    // GTAO (mode 4 & 5): 3 slice directions x 2 sides x 3 steps (18 taps), cosine-weighted horizon integral
    const int D = 3, S = 3;
    float vis = 0.0;
    for (int d_i = 0; d_i < D; ++d_i) {
        float phi = (float(d_i) + noise) * (3.14159265 / float(D));
        vec3 dir = t * cos(phi) + b * sin(phi);
        float h1 = -1.0, h2 = -1.0;
        for (int s_i = 1; s_i <= S; ++s_i) {
            float step_r = ((float(s_i) - 0.5 + 0.5 * noise2) / float(S)) * R;
            vec4 cp = uVP * vec4(P + N * uParam.z + dir * step_r, 1.0);
            if (cp.w > 0.0) {
                vec2 uvp = cp.xy / cp.w * 0.5 + 0.5;
                if (all(greaterThanEqual(uvp, vec2(0.0))) && all(lessThanEqual(uvp, vec2(1.0)))) {
                    float sdp = texture(uDepth, uvp).r;
                    if (sdp < 1.0) {
                        vec3 Hp = world(uvp, sdp) - P;
                        float d2 = dot(Hp, Hp);
                        if (d2 > 1e-5) {
                            float c_h = dot(Hp, N) * inversesqrt(d2);
                            float falloff = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                            h1 = max(h1, mix(-1.0, c_h, falloff));
                        }
                    }
                }
            }
            vec4 cn = uVP * vec4(P + N * uParam.z - dir * step_r, 1.0);
            if (cn.w > 0.0) {
                vec2 uvn = cn.xy / cn.w * 0.5 + 0.5;
                if (all(greaterThanEqual(uvn, vec2(0.0))) && all(lessThanEqual(uvn, vec2(1.0)))) {
                    float sdn = texture(uDepth, uvn).r;
                    if (sdn < 1.0) {
                        vec3 Hn = world(uvn, sdn) - P;
                        float d2 = dot(Hn, Hn);
                        if (d2 > 1e-5) {
                            float c_h = dot(Hn, N) * inversesqrt(d2);
                            float falloff = clamp(1.0 - d2 * invR2, 0.0, 1.0);
                            h2 = max(h2, mix(-1.0, c_h, falloff));
                        }
                    }
                }
            }
        }
        // (h: the sine of each side's horizon's elevation over the surface's plane; the cosine-weighted share of a side's quarter
        // circle above its horizon is 1 - sin(elevation): a free side 1, a wall up to the normal 0)
        float slice_vis = 0.5 * ((1.0 - max(h1, 0.0)) + (1.0 - max(h2, 0.0)));
        vis += clamp(slice_vis, 0.0, 1.0);
    }
    vis /= float(D);
    // Multi-bounce approximation (Jimenez et al. 2016)
    float mb = vis / (vis * 0.6 + 0.4);
    FragAo = clamp(mix(1.0, mix(vis, mb, 0.35), uParam.y * 1.15), 0.0, 1.0);
}
)";
const char* kAoBlurFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uAo, uDepth;
uniform vec2 uTexel;
uniform usampler2D uStencil;  // (the depth's stencil: bit 0 the mobys' pixels, mrp_lighting_ao_moby_mark)
uniform float uMoby;          // (the share of the occlusion the mobys' pixels keep; 1: all, the stencil not read)
out vec4 FragColor;
void main() {
    float d0 = texture(uDepth, vUv).r;
    if (d0 >= 1.0) discard;
    float sum = 0.0, w = 0.0;
    for (int y = -2; y <= 2; ++y)
        for (int x = -2; x <= 2; ++x) {
            vec2 uv = vUv + vec2(x, y) * uTexel;
            float d = texture(uDepth, uv).r;
            float k = abs(d - d0) < 0.0005 ? 1.0 : 0.0;
            sum += texture(uAo, uv).r * k;
            w += k;
        }
    float ao = w > 0.0 ? sum / w : 1.0;
    float k = 1.0;
    if (uMoby < 1.0 && (texture(uStencil, vUv).r & 1u) != 0u) k = uMoby;
    FragColor = vec4(vec3(mix(1.0, ao, k)), 1.0);
}
)";
const char* kAoBlurSepFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uAo, uDepth;
uniform vec2 uTexel;
uniform usampler2D uStencil;  // (the depth's stencil: bit 0 the mobys' pixels, mrp_lighting_ao_moby_mark)
uniform float uMoby;          // (the share of the occlusion the mobys' pixels keep; 1: all, the stencil not read)
out vec4 FragColor;
void main() {
    float d0 = texture(uDepth, vUv).r;
    if (d0 >= 1.0) { FragColor = vec4(1.0); return; }
    const float gw[5] = float[5](0.227027, 0.1945946, 0.1216216, 0.054054, 0.016216);
    float sum = texture(uAo, vUv).r * gw[0], w = gw[0];
    for (int i = 1; i <= 4; ++i) {
        vec2 off = uTexel * float(i);
        vec2 uv1 = vUv + off, uv2 = vUv - off;
        float d1 = texture(uDepth, uv1).r, d2 = texture(uDepth, uv2).r;
        float k1 = gw[i] * exp(-abs(d1 - d0) * 3500.0);
        float k2 = gw[i] * exp(-abs(d2 - d0) * 3500.0);
        sum += texture(uAo, uv1).r * k1 + texture(uAo, uv2).r * k2;
        w += k1 + k2;
    }
    float ao = w > 1e-5 ? sum / w : 1.0;
    float k = 1.0;
    if (uMoby < 1.0 && (texture(uStencil, vUv).r & 1u) != 0u) k = uMoby;
    FragColor = vec4(vec3(mix(1.0, ao, k)), 1.0);
}
)";
GLuint ao_compile(GLenum type, const char* src) {
    const GLuint s = glCreateShader(type);
    glShaderSource(s, 1, &src, nullptr);
    glCompileShader(s);
    GLint ok = 0;
    glGetShaderiv(s, GL_COMPILE_STATUS, &ok);
    if (!ok) {
        char log[2048];
        glGetShaderInfoLog(s, sizeof log, nullptr, log);
        std::fprintf(stderr, "[ao] shader: %s\n", log);
    }
    return s;
}
GLuint ao_link(const char* fs) {
    const GLuint p = glCreateProgram();
    const GLuint v = ao_compile(GL_VERTEX_SHADER, kAoVs), f = ao_compile(GL_FRAGMENT_SHADER, fs);
    glAttachShader(p, v);
    glAttachShader(p, f);
    glLinkProgram(p);
    glDeleteShader(v);
    glDeleteShader(f);
    GLint ok = 0;
    glGetProgramiv(p, GL_LINK_STATUS, &ok);
    if (!ok) std::fprintf(stderr, "[ao] link failed\n");
    return p;
}
struct Ao {
    GLuint prog = 0, blur = 0, blur_sep = 0, vao = 0;
    GLuint dfbo = 0, dtex = 0, afbo = 0, atex = 0, bfbo = 0, btex = 0;
    GLuint stex = 0;  // (a stencil view of dtex, when the depth has a stencil)
    int w = 0, h = 0, aw = 0, ah = 0;
    GLint last_fb = -1;
    GLenum dfmt = 0;
    GLint u_depth = -1, u_inv_vp = -1, u_vp = -1, u_cam = -1, u_param = -1;
    GLint u_b_depth = -1, u_b_ao = -1, u_b_texel = -1, u_b_st = -1, u_b_moby = -1;
    GLint u_bs_depth = -1, u_bs_ao = -1, u_bs_texel = -1, u_bs_st = -1, u_bs_moby = -1;
} g_ao_s;
// The mobys' pixels (mrp_lighting_ao_moby_mark): stencil bit 0 set where the MRP drew a moby this frame. Screen-space occlusion
// sees a character's own arms, head and body overlapping as creases (Big Al's face and arms in his shop): on those pixels only a
// share of the occlusion is kept (RAC1_AO_MOBY, percent, default 40), the world's pixels around them keep all of it (the contact
// darkening on the ground under a crate or a character). The stencil is free while the modern lighting draws (the original's drop
// shadows, its only other user, are never drawn then)
struct AoMark {
    bool active = false;  // marking (between begin and end)
    bool done = false;    // this frame's mobys marked, into `fb`
    GLint fb = -1;
    GLint stencil_fb = -1;  // (the framebuffer whose stencil was looked at, and whether it has one)
    bool stencil = false;
} g_ao_mark;

// ---- the sun's rays (Options > Graphics > Sun Rays) ------------------------------------------------------------------------
//
// Our choice, not the game's: light shafts from the sun where the sky shows through the scene (a tunnel's mouth, the gaps between
// trees, the sky around a skybox planet). Screen space, at a fraction of the resolution: (1) the sky's texels (depth 1) near the
// sun's place on the screen, their colour tinted by the key light, the rest black; (2) a radial blur of that toward the sun (each
// tap further one weaker), and how open the sky around the sun is (0..1, the blur's texel (0, 0)); (3) the rays taken apart, at
// the same size: what the rays across a texel (to its sides, square to the way toward the sun) share is the sky's broad light,
// what the texel has above them a shaft; (4) added to the picture, faded when the sun leaves the screen or is behind the camera,
// and with the sun's weight (no rays where the zone has no sun, in the level's interiors, in a zone fading), with a soft limit a
// texel (kRaysSteps), blended as a screen (what is bright already gains less). In front of an open sky the broad light is a
// glare (it washed the whole picture out): it is cut to kRaysOpenNear near the sun and kRaysOpenFar far from it; in a covered
// view (a tunnel, a roof of leaves: the sky around the sun mostly hidden) it all stays, and the shafts stay everywhere.
// The three steps (kRaysSteps): the resolution of the mask and the blur, the blur's taps (its reach and its fall-off the same: the
// decay a tap is 0.945 to the 40 / taps) and the strength: Low 0.4.x Low's, High 0.4.x High's, Medium between them.
struct RaysStep {
    int div;      // the resolution: the view's / div
    int taps;     // the radial blur's
    float gain;   // the added light's gain
    float cap;    // and the most a texel gets (a soft limit)
};
constexpr RaysStep kRaysSteps[3] = {{8, 24, 3.5f, 0.28f}, {4, 40, 5.0f, 0.36f}, {3, 56, 7.0f, 0.45f}};
// the glare in front of an open sky (see kRaysAddFs): the added light where the sky around the sun is all open, near the sun and far
// from it (of the shafts' own), and how fast it falls from the one to the other (exp(-d^2 x), d in the view's heights from the sun)
constexpr float kRaysOpenNear = 0.7f, kRaysOpenFar = 0.12f, kRaysOpenFall = 5.0f;
const char* kRaysMaskFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uDepth, uColor;
uniform vec2 uSun;      // the sun's place, 0..1 of the view
uniform vec2 uAspect;   // (the view's width / height, 1: distances in the screen's proportions)
uniform vec3 uTint;     // the key light's colour, its largest channel 1
out vec4 FragColor;
void main() {
    float d = texture(uDepth, vUv).r;
    if (d < 1.0) { FragColor = vec4(0.0); return; }
    vec3 c = texture(uColor, vUv).rgb;
    vec2 v = (vUv - uSun) * uAspect;
    float k = exp(-dot(v, v) * 6.0);  // (the sky near the sun)
    float lum = dot(c, vec3(0.299, 0.587, 0.114));
    FragColor = vec4(mix(c, uTint * lum, 0.5) * k, 1.0);
}
)";
const char* kRaysBlurFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uMask;
uniform vec2 uSun;
uniform float uAspect;  // (the view's width / height)
uniform vec4 uParam;  // x density (how far toward the sun the taps reach), y decay, z weight, w exposure
uniform int uTaps;
out vec4 FragColor;
void main() {
    int N = uTaps;
    vec2 delta = (vUv - uSun) * (uParam.x / float(N));
    float noise = fract(52.9829189 * fract(dot(gl_FragCoord.xy, vec2(0.06711056, 0.00583715))));
    vec2 uv = vUv - delta * noise;  // (dithered start: no rings)
    vec3 sum = vec3(0.0);
    float decay = 1.0;
    for (int i = 0; i < N; ++i) {
        uv -= delta;
        sum += texture(uMask, uv).rgb * decay * uParam.z;
        decay *= uParam.y;
    }
    // how open the sky around the sun is, 0..1: the mask's sky on a 6 x 6 grid around the sun's place (the view's heights), weighted
    // as the mask weights the sky; in the texel (0, 0) alone, where the add reads it
    float os = 0.0, ow = 1e-6;
    if (gl_FragCoord.x < 1.0 && gl_FragCoord.y < 1.0) {
        for (int j = 0; j < 6; ++j) {
            for (int i = 0; i < 6; ++i) {
                vec2 o = (vec2(float(i), float(j)) - 2.5) * 0.12;
                vec2 q = clamp(uSun + vec2(o.x / uAspect, o.y), 0.0, 1.0);  // (a sun off the view: the sky at the view's edge)
                float w = exp(-dot(o, o) * 6.0);
                os += texture(uMask, q).a * w;
                ow += w;
            }
        }
    }
    FragColor = vec4(sum * uParam.w, os / ow);
}
)";
const char* kRaysSplitFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uRays;
uniform vec4 uA;  // xy the sun's place (0..1 of the view)
uniform vec4 uB;  // x the view's width / height; the open sky's glare: y near the sun, z far from it, w its fall-off
out vec4 FragColor;
void main() {
    // the rays here and across them (to the sides, square to the way toward the sun): what they share is the broad light of the
    // sky, what this texel has above it a shaft
    vec2 asp = vec2(uB.x, 1.0);
    vec2 v = (vUv - uA.xy) * asp;
    vec2 side = normalize(vec2(-v.y, v.x) + vec2(1e-6, 0.0)) / asp;
    vec3 r = texture(uRays, vUv).rgb;
    vec3 across = 0.25 * (texture(uRays, vUv + side * 0.05).rgb + texture(uRays, vUv - side * 0.05).rgb +
                          texture(uRays, vUv + side * 0.11).rgb + texture(uRays, vUv - side * 0.11).rgb);
    vec3 broad = min(r, across), shaft = r - broad;
    // an open sky around the sun (the blur's texel (0, 0)'s alpha toward 1): its broad light is a glare, kept mostly near the sun; in
    // a covered view (toward 0: a tunnel's mouth, the gaps in a roof of leaves) all of it stays
    float glare = uB.y * mix(uB.z, 1.0, exp(-dot(v, v) * uB.w));
    float open = smoothstep(0.2, 0.6, texelFetch(uRays, ivec2(0), 0).a);
    FragColor = vec4(shaft + broad * mix(1.0, glare, open), 1.0);
}
)";
const char* kRaysAddFs = R"(#version 460 core
in vec2 vUv;
uniform sampler2D uRays;
uniform float uK, uCap;  // (the gain, and the most a texel gets: a soft limit)
out vec4 FragColor;
void main() { FragColor = vec4(uCap * (1.0 - exp(-texture(uRays, vUv).rgb * uK / uCap)), 0.0); }  // (blended as a screen)
)";
// orig: (host) kRaysOpenNear / Far / Fall; RAC1_SUN_RAYS_SHAPE="near,far,fall" (testing) overrides them
glm::vec3 rays_open_shape() {
    static const glm::vec3 v = [] {
        glm::vec3 r(kRaysOpenNear, kRaysOpenFar, kRaysOpenFall);
        if (const char* e = std::getenv("RAC1_SUN_RAYS_SHAPE")) std::sscanf(e, "%f,%f,%f", &r.x, &r.y, &r.z);
        return r;
    }();
    return v;
}
struct Rays {
    GLuint mask = 0, blur = 0, split = 0, add = 0, vao = 0;
    GLuint dfbo = 0, dtex = 0, cfbo = 0, ctex = 0, mfbo = 0, mtex = 0, rfbo = 0, rtex = 0;
    int w = 0, h = 0, div = 0;
    GLenum dfmt = 0;
    GLint last_fb = -1;
} g_rays;
// orig: (host) the depth format of a framebuffer (a blit of its depth wants the same)
GLenum depth_format(GLint fb) {
    GLint dbits = 24, sbits = 8, dtype = GL_UNSIGNED_NORMALIZED;
    glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fb));
    const GLenum datt = fb == 0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT, satt = fb == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &dbits);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &dtype);
    glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, satt, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &sbits);
    while (glGetError() != GL_NO_ERROR) {}
    return dtype == GL_FLOAT ? (sbits > 0 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F)
                             : (sbits > 0 ? GL_DEPTH24_STENCIL8 : (dbits >= 32 ? GL_DEPTH_COMPONENT32 : dbits >= 24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16));
}
// orig: (host) the sun's rays drawn this frame: on, the zone with a sun, and not in the ship's flights (game mode 6 but its take-off
// and landing scenes, steps 0 / 8 at 0x13e050: the flight away from the planet, 3, and the loading screen's flight between planets,
// 4, level_exit.cpp / flight_draw.cpp), where the sun is still the level's left behind (GitHub issue #66)
bool sun_rays_now() {
    if (g_sun_rays <= 0 || g_sun_rays_off || !(g_sh.sun_k > 0.01f)) return false;  // (or off on this level: "sun_rays off" in its file)
    const int32_t step = draw_lw(0x0013e050u);
    return draw_lw(OV(0x0015f5c4u)) != 6 || step == 0 || step == 8;
}
// orig: (host) the sun's rays added to the bound framebuffer's 3D (see kRaysMaskFs); the caller saves and restores the GL state
void sun_rays_pass(GLint fb, const int vp[4]) {
    if (!sun_rays_now()) return;
    float vpf[16];
    mrp_world_view_proj(vpf);
    const glm::mat4 VP = glm::make_mat4(vpf);
    const glm::vec4 c = VP * glm::vec4(g_sh.cam + g_sh.sun * 2000.0f, 1.0f);
    if (!(c.w > 0.0f)) return;  // (behind the camera)
    const glm::vec2 su = glm::vec2(c.x, c.y) / c.w * 0.5f + 0.5f;
    // (on screen: full; fading out to 0.6 of the view beyond its edge)
    const float out = std::max({-su.x, su.x - 1.0f, -su.y, su.y - 1.0f, 0.0f});
    const float fade = std::clamp(1.0f - out / 0.6f, 0.0f, 1.0f);
    if (!(fade > 0.0f)) return;
    Rays& R = g_rays;
    const RaysStep& step = kRaysSteps[std::clamp(g_sun_rays, 1, 3) - 1];
    const int w = vp[2], h = vp[3], hw = std::max(1, w / step.div), hh = std::max(1, h / step.div);  // (a fraction: the shafts are soft)
    if (R.mask == 0) {
        R.mask = ao_link(kRaysMaskFs);
        R.blur = ao_link(kRaysBlurFs);
        R.split = ao_link(kRaysSplitFs);
        R.add = ao_link(kRaysAddFs);
        glGenVertexArrays(1, &R.vao);
        GLuint f[4], t[4];
        glGenFramebuffers(4, f);
        glGenTextures(4, t);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 4, t, "Sun rays");
        R.dfbo = f[0], R.cfbo = f[1], R.mfbo = f[2], R.rfbo = f[3];
        R.dtex = t[0], R.ctex = t[1], R.mtex = t[2], R.rtex = t[3];
    }
    GLenum fmt = R.dfmt;
    if (fb != R.last_fb || fmt == 0) {
        fmt = depth_format(fb);
        R.last_fb = fb;
    }
    const bool st = fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8;
    if (R.w != w || R.h != h || R.dfmt != fmt || R.div != step.div) {
        R.w = w;
        R.h = h;
        R.dfmt = fmt;
        R.div = step.div;
        auto tex = [](GLuint t, GLint ifmt, int tw, int th, GLenum f2, GLenum ty, GLint filter) {
            glBindTexture(GL_TEXTURE_2D, t);
            glTexImage2D(GL_TEXTURE_2D, 0, ifmt, tw, th, 0, f2, ty, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
        };
        tex(R.dtex, static_cast<GLint>(fmt), hw, hh, st ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
            fmt == GL_DEPTH24_STENCIL8 ? GL_UNSIGNED_INT_24_8 : fmt == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_FLOAT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, R.dfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, R.dtex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
        tex(R.ctex, GL_RGBA8, hw, hh, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, R.cfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.ctex, 0);
        tex(R.mtex, GL_RGBA16F, hw, hh, GL_RGBA, GL_FLOAT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, R.mfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.mtex, 0);
        tex(R.rtex, GL_RGBA16F, hw, hh, GL_RGBA, GL_FLOAT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, R.rfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, R.rtex, 0);
    }
    // the 3D's depth (nearest) and colour (linear) at the step's size
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fb));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.dfbo);
    glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, hw, hh, GL_DEPTH_BUFFER_BIT | (st ? GL_STENCIL_BUFFER_BIT : 0), GL_NEAREST);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.cfbo);
    glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, hw, hh, GL_COLOR_BUFFER_BIT, GL_LINEAR);
    glDisable(GL_BLEND);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glBindVertexArray(R.vao);
    glViewport(0, 0, hw, hh);
    const float tmax = std::max({g_sh.key.x, g_sh.key.y, g_sh.key.z, 1e-3f});
    const glm::vec3 tint = g_sh.key.x + g_sh.key.y + g_sh.key.z > 0.0f ? glm::vec3(g_sh.key) / tmax : glm::vec3(1.0f, 0.95f, 0.85f);
    // (1) the mask
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.mfbo);
    glUseProgram(R.mask);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, R.dtex);
    glActiveTexture(GL_TEXTURE1);
    glBindTexture(GL_TEXTURE_2D, R.ctex);
    glUniform1i(glGetUniformLocation(R.mask, "uDepth"), 0);
    glUniform1i(glGetUniformLocation(R.mask, "uColor"), 1);
    glUniform2f(glGetUniformLocation(R.mask, "uSun"), su.x, su.y);
    glUniform2f(glGetUniformLocation(R.mask, "uAspect"), static_cast<float>(w) / static_cast<float>(std::max(h, 1)), 1.0f);
    glUniform3f(glGetUniformLocation(R.mask, "uTint"), tint.x, tint.y, tint.z);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // (2) the radial blur
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.rfbo);
    glUseProgram(R.blur);
    glActiveTexture(GL_TEXTURE0);
    glBindTexture(GL_TEXTURE_2D, R.mtex);
    glUniform1i(glGetUniformLocation(R.blur, "uMask"), 0);
    glUniform2f(glGetUniformLocation(R.blur, "uSun"), su.x, su.y);
    glUniform1f(glGetUniformLocation(R.blur, "uAspect"), static_cast<float>(w) / static_cast<float>(std::max(h, 1)));
    // (the taps' decay and weight for the step's count: the reach and the fall-off along it as with 40)
    const float decay = step.taps == 40 ? 0.945f : static_cast<float>(std::pow(0.945, 40.0 / step.taps));
    glUniform4f(glGetUniformLocation(R.blur, "uParam"), 0.9f, decay, 1.0f / static_cast<float>(step.taps) * 4.0f, 1.0f);
    glUniform1i(glGetUniformLocation(R.blur, "uTaps"), step.taps);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // (3) the shafts and the broad light taken apart, into the mask's texture (done with)
    const glm::vec3 shape = rays_open_shape();
    const float aspect = static_cast<float>(w) / static_cast<float>(std::max(h, 1));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, R.mfbo);
    glUseProgram(R.split);
    glBindTexture(GL_TEXTURE_2D, R.rtex);
    glUniform1i(glGetUniformLocation(R.split, "uRays"), 0);
    glUniform4f(glGetUniformLocation(R.split, "uA"), su.x, su.y, 0.0f, 0.0f);
    glUniform4f(glGetUniformLocation(R.split, "uB"), aspect, shape.x, shape.y, shape.z);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    // (4) added to the picture
    static const float gain_env = std::getenv("RAC1_SUN_RAYS_GAIN") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_SUN_RAYS_GAIN"))) : 1.0f;  // (testing)
    const float k = step.gain * gain_env * fade * std::min(g_sh.sun_k, 1.0f);
    const float cap = step.cap * std::max(fade * std::min(g_sh.sun_k, 1.0f), 1e-3f);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
    glViewport(vp[0], vp[1], w, h);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
    glEnable(GL_BLEND);
    glBlendFunc(GL_ONE_MINUS_DST_COLOR, GL_ONE);  // (a screen: what is bright already gains less)
    glUseProgram(R.add);
    glBindTexture(GL_TEXTURE_2D, R.mtex);
    glUniform1i(glGetUniformLocation(R.add, "uRays"), 0);
    glUniform1f(glGetUniformLocation(R.add, "uK"), k);
    glUniform1f(glGetUniformLocation(R.add, "uCap"), cap);
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glDisable(GL_BLEND);
    if (std::getenv("RAC1_SUN_RAYS_TRACE") != nullptr) {
        static uint64_t n = 0;
        if (n++ % 60 == 0) std::printf("[rays] sun at %.2f %.2f of the view, fade %.2f, weight %.2f: %.2f\n", su.x, su.y, fade, g_sh.sun_k, k);
    }
}

// ---- the view covered from the sun -----------------------------------------------------------------------------------
//
// Our choice, not the game's: a room, a corridor, a ship's hold is lit by its zone's key light, which comes from above, but the
// roof stops it, so the sun's shadow covers the whole view and darkens it (Blarg's station, Orxon's base, Gaspar's halls). Each
// frame the 3D's depth is copied at 2 kProbeW x 2 kProbeH (a blit, nearest); per sample its world point and face normal (from its
// neighbours), tested against the cascades (one tap, a few texels toward the sun) when it faces the sun - a face turned away is
// not counted, as the receivers do not shadow it -, the sky as seeing it. Read back the next frame (a pixel buffer): the sun's
// weight is scaled by how much of the view sees the sun - none under kCoverLo, all over kCoverHi, the scale following it slowly
// (kCoverRate a frame, about a second), then faded as between zones. In a room, the original's look (the point lights and the
// occlusion on top); outdoors, where the ground in the sun or the sky shows, the shadows as before.
// Outdoors besides: where the camera is where the sky was in view (kSkySeen of the samples, any time since the level began),
// the sun is kept whatever the view shows. Outdoors a camera often looks at nothing but shaded ground (down at Novalis's
// start valley, all in the shade of its trees and of the plateau beside it): that view alone faded the sun out, the trees' and
// Ratchet's shadows went and came back as the camera turned. A room shows no sky. The places are the camera's positions, one
// every kSkyStep units, kSkyMax of them (the oldest dropped); within kSkyNear of one the sun is kept, beyond kSkyFar it is the
// view's measure alone.
constexpr int kProbeW = 64, kProbeH = 36;
constexpr float kCoverLo = 0.05f, kCoverHi = 0.25f, kCoverRate = 0.03f;
constexpr float kSkySeen = 0.05f, kSkyStep = 4.0f, kSkyNear = 12.0f, kSkyFar = 24.0f;
constexpr size_t kSkyMax = 1024;
const char* kProbeFs = R"(#version 460 core
uniform sampler2D uDepth;
uniform sampler2DArrayShadow uShMap;
uniform mat4 uShCasc[3];
uniform vec3 uShPart;
uniform vec3 uShTexel;
uniform vec3 uShSun;
uniform vec3 uCam;
uniform mat4 uInvVP;
out vec4 FragOut;  // r the sun seen, g counted, b the sky
vec3 world(ivec2 ip, float d) {
    vec2 uv = (vec2(ip) + 0.5) / vec2(textureSize(uDepth, 0));
    vec4 w = uInvVP * vec4(uv * 2.0 - 1.0, d * 2.0 - 1.0, 1.0);
    return w.xyz / w.w;
}
void main() {
    ivec2 ip = ivec2(gl_FragCoord.xy) * 2;
    float d = texelFetch(uDepth, ip, 0).r;
    if (d >= 1.0) { FragOut = vec4(1.0, 1.0, 1.0, 1.0); return; }
    float dx = texelFetch(uDepth, ip + ivec2(1, 0), 0).r, dy = texelFetch(uDepth, ip + ivec2(0, 1), 0).r;
    vec3 P = world(ip, d);
    if (dx >= 1.0 || dy >= 1.0) { FragOut = vec4(0.0); return; }
    vec3 N = cross(world(ip + ivec2(1, 0), dx) - P, world(ip + ivec2(0, 1), dy) - P);
    if (dot(N, N) < 1e-12) { FragOut = vec4(0.0); return; }
    N = normalize(N);
    if (dot(N, uCam - P) < 0.0) N = -N;
    if (dot(N, uShSun) < 0.1) { FragOut = vec4(0.0); return; }
    for (int k = 0; k < 3; ++k) {
        vec4 q = uShCasc[k] * vec4(P + N * (uShTexel[k] * 1.5) + uShSun * (uShTexel[k] * 2.0), 1.0);
        if (any(lessThan(q.xy, vec2(0.02))) || any(greaterThan(q.xy, vec2(0.98))) || q.z >= 1.0) continue;
        FragOut = vec4(texture(uShMap, vec4(q.xy * uShPart[k], float(k), q.z)), 1.0, 0.0, 1.0);
        return;
    }
    FragOut = vec4(1.0, 1.0, 0.0, 1.0);
}
)";
struct Probe {
    GLuint prog = 0, vao = 0, dfbo = 0, dtex = 0, ofbo = 0, otex = 0, pbo[2] = {0, 0};
    GLenum dfmt = 0;
    GLint last_fb = -1;
    bool filled[2] = {false, false};
    int next = 0;
    GLint u_depth = -1, u_map = -1, u_casc = -1, u_part = -1, u_texel = -1, u_sun = -1, u_cam = -1, u_inv_vp = -1;
    float open = 1.0f, sky = 1.0f;  // the last measure: the counted samples' share that see the sun, the samples' share of sky
    std::vector<glm::vec3> sky_at;  // where the camera was when the sky was in view (outdoors)
    float outdoors = 1.0f;          // the last measure: 1 near one of them .. 0 away from all
    float k = 1.0f;                 // the sun's weight's scale (smoothed)
    int32_t level = -1;
} g_probe;
}  // namespace

namespace {
// orig: (host) the view covered from the sun (see kProbeW): this frame's samples drawn and read into a pixel buffer, the last
// frame's read (its fence long passed: the map does not wait in practice; always the last frame's - the same picture every run)
void cover_probe(GLint fb, const int vp[4]) {
    const bool on = cover_probe_on();
    static const bool trace = std::getenv("RAC1_SUN_COVER_TRACE") != nullptr;
    Probe& P = g_probe;
    const int32_t level = draw_lw(0x0015ed84);
    if (level != P.level) {  // (a new level: nothing measured yet)
        P.level = level;
        P.k = 1.0f;
        P.open = P.sky = 1.0f;
        P.filled[0] = P.filled[1] = false;
        P.sky_at.clear();
    }
    if (on == 0 || !g_casc_on.load()) {
        P.k = 1.0f;
        P.filled[0] = P.filled[1] = false;
        return;
    }
    if (P.prog == 0) {
        P.prog = ao_link(kProbeFs);
        P.u_depth = glGetUniformLocation(P.prog, "uDepth");
        P.u_map = glGetUniformLocation(P.prog, "uShMap");
        P.u_casc = glGetUniformLocation(P.prog, "uShCasc");
        P.u_part = glGetUniformLocation(P.prog, "uShPart");
        P.u_texel = glGetUniformLocation(P.prog, "uShTexel");
        P.u_sun = glGetUniformLocation(P.prog, "uShSun");
        P.u_cam = glGetUniformLocation(P.prog, "uCam");
        P.u_inv_vp = glGetUniformLocation(P.prog, "uInvVP");
        glGenVertexArrays(1, &P.vao);
        glGenFramebuffers(1, &P.dfbo);
        glGenFramebuffers(1, &P.ofbo);
        glGenTextures(1, &P.dtex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &P.dtex, "Sun cover probe");
        glGenTextures(1, &P.otex);
        rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &P.otex, "Sun cover probe");
        glGenBuffers(2, P.pbo);
        rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 2, P.pbo, "Sun cover probe");
        for (int i = 0; i < 2; ++i) {
            glBindBuffer(GL_PIXEL_PACK_BUFFER, P.pbo[i]);
            glBufferData(GL_PIXEL_PACK_BUFFER, 4 * kProbeW * kProbeH, nullptr, GL_STREAM_READ);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindTexture(GL_TEXTURE_2D, P.otex);
        glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, kProbeW, kProbeH, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, P.ofbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, P.otex, 0);
    }
    // the last frame's measure
    const int prev = P.next ^ 1;
    if (P.filled[prev]) {
        glBindBuffer(GL_PIXEL_PACK_BUFFER, P.pbo[prev]);
        if (const auto* px = static_cast<const uint8_t*>(glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, 4 * kProbeW * kProbeH, GL_MAP_READ_BIT))) {
            uint32_t lit = 0, counted = 0, sky = 0;
            for (int i = 0; i < kProbeW * kProbeH; ++i) {
                lit += px[4 * i];
                counted += px[4 * i + 1] >= 128 ? 1u : 0u;
                sky += px[4 * i + 2] >= 128 ? 1u : 0u;
            }
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
            P.open = counted > 0 ? static_cast<float>(lit) / (255.0f * static_cast<float>(counted)) : 1.0f;
            P.sky = static_cast<float>(sky) / static_cast<float>(kProbeW * kProbeH);
            static const int sky_on = env_int("RAC1_SUN_COVER_SKY", 1);  // (testing: 0 the view alone, as in 0.4.0)
            P.outdoors = 0.0f;
            if (sky_on != 0) {
                const glm::vec3 h = g_sh.cam;  // (the camera: Clank's or a vehicle's sections too)
                float dmin = 1e30f;
                for (const glm::vec3& q : P.sky_at) dmin = std::min(dmin, glm::length(q - h));
                if (P.sky >= kSkySeen && dmin > kSkyStep) {
                    if (P.sky_at.size() >= kSkyMax) P.sky_at.erase(P.sky_at.begin());
                    P.sky_at.push_back(h);
                    dmin = 0.0f;
                }
                P.outdoors = std::clamp((kSkyFar - dmin) / (kSkyFar - kSkyNear), 0.0f, 1.0f);
            }
            const float kv = std::clamp((P.open - kCoverLo) / (kCoverHi - kCoverLo), 0.0f, 1.0f);
            P.k += (std::max(kv, P.outdoors) - P.k) * kCoverRate;
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        P.filled[prev] = false;
    }
    if (trace) {
        static uint64_t n = 0;
        if (n++ % 60 == 0)
            std::printf("[sun] cover probe: %.0f %% sees the sun, %.0f %% sky, outdoors %.2f (%zu places): the sun's weight x %.2f; camera %.1f %.1f %.1f\n", 100.0f * P.open,
                        100.0f * P.sky, P.outdoors, P.sky_at.size(), P.k, g_sh.cam.x, g_sh.cam.y, g_sh.cam.z);
    }
    // this frame's: the depth at 2 kProbeW x 2 kProbeH (a blit of the same format, nearest)
    GLenum fmt = P.dfmt;
    if (fb != P.last_fb || fmt == 0) {
        GLint dbits = 24, sbits = 8, dtype = GL_UNSIGNED_NORMALIZED;
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fb));
        const GLenum datt = fb == 0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT, satt = fb == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &dbits);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &dtype);
        glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, satt, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &sbits);
        while (glGetError() != GL_NO_ERROR) {}
        fmt = dtype == GL_FLOAT ? (sbits > 0 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F)
                                : (sbits > 0 ? GL_DEPTH24_STENCIL8 : (dbits >= 32 ? GL_DEPTH_COMPONENT32 : dbits >= 24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16));
        P.last_fb = fb;
    }
    const bool st = fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8;
    if (P.dfmt != fmt) {
        P.dfmt = fmt;
        glBindTexture(GL_TEXTURE_2D, P.dtex);
        glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), 2 * kProbeW, 2 * kProbeH, 0, st ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
                     fmt == GL_DEPTH24_STENCIL8 ? GL_UNSIGNED_INT_24_8 : fmt == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_FLOAT, nullptr);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
        glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, P.dfbo);
        glFramebufferTexture2D(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, P.dtex, 0);
        glDrawBuffer(GL_NONE);
        glReadBuffer(GL_NONE);
    }
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fb));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, P.dfbo);
    glBlitFramebuffer(vp[0], vp[1], vp[0] + vp[2], vp[1] + vp[3], 0, 0, 2 * kProbeW, 2 * kProbeH, GL_DEPTH_BUFFER_BIT | (st ? GL_STENCIL_BUFFER_BIT : 0), GL_NEAREST);
    // the samples against the cascades (the caller's state saved and restored around)
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, P.ofbo);
    glViewport(0, 0, kProbeW, kProbeH);
    glUseProgram(P.prog);
    glBindVertexArray(P.vao);
    constexpr int kProbeUnit = 28;
    glActiveTexture(GL_TEXTURE0 + kProbeUnit);
    glBindTexture(GL_TEXTURE_2D, P.dtex);
    glActiveTexture(GL_TEXTURE0 + kUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_sh.tex);
    glUniform1i(P.u_depth, kProbeUnit);
    glUniform1i(P.u_map, kUnit);
    glm::mat4 m[kCascades];
    for (int k = 0; k < kCascades; ++k) m[k] = g_sh.casc[k].tex;
    glUniformMatrix4fv(P.u_casc, kCascades, GL_FALSE, glm::value_ptr(m[0]));
    glUniform3f(P.u_part, g_sh.casc[0].part, g_sh.casc[1].part, g_sh.casc[2].part);
    glUniform3f(P.u_texel, g_sh.casc[0].texel, g_sh.casc[1].texel, g_sh.casc[2].texel);
    glUniform3f(P.u_sun, g_sh.sun.x, g_sh.sun.y, g_sh.sun.z);
    glUniform3f(P.u_cam, g_sh.cam.x, g_sh.cam.y, g_sh.cam.z);
    float vpf[16];
    mrp_world_view_proj(vpf);
    const glm::mat4 IVP = glm::inverse(glm::make_mat4(vpf));
    glUniformMatrix4fv(P.u_inv_vp, 1, GL_FALSE, glm::value_ptr(IVP));
    glDrawArrays(GL_TRIANGLES, 0, 3);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, P.ofbo);
    glReadBuffer(GL_COLOR_ATTACHMENT0);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, P.pbo[P.next]);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);
    glReadPixels(0, 0, kProbeW, kProbeH, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glPixelStorei(GL_PACK_ALIGNMENT, 4);
    glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
    P.filled[P.next] = true;
    P.next ^= 1;
}
// orig: (host) how much of the sun's weight the view keeps (a room lit by a key light from above that its roof stops: no sky,
// nearly no sample sees the sun - the sun's shadows faded out there; our choice, see kProbeW)
float view_open() { return g_probe.k; }
void ensure_vk_ao_pipelines() {
    if (g_vk_sh.ao_pipe != VK_NULL_HANDLE || !vk::is_active()) return;
    ensure_vk_lighting_init();
    VkDevice dev = vk::device();

    VkDescriptorSetLayoutBinding b1{};
    b1.binding = 0;
    b1.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    b1.descriptorCount = 1;
    b1.stageFlags = VK_SHADER_STAGE_FRAGMENT_BIT;
    VkDescriptorSetLayoutCreateInfo dsl1{};
    dsl1.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl1.bindingCount = 1;
    dsl1.pBindings = &b1;
    vkCreateDescriptorSetLayout(dev, &dsl1, nullptr, &g_vk_sh.samp1_layout);

    VkDescriptorSetLayoutBinding b2[2]{};
    b2[0] = {0, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    b2[1] = {1, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dsl2{};
    dsl2.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl2.bindingCount = 2;
    dsl2.pBindings = b2;
    vkCreateDescriptorSetLayout(dev, &dsl2, nullptr, &g_vk_sh.samp2_layout);

    VkDescriptorSetLayoutBinding b3[3]{};
    for (uint32_t i = 0; i < 3; ++i) b3[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1, VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
    VkDescriptorSetLayoutCreateInfo dsl3{};
    dsl3.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
    dsl3.bindingCount = 3;
    dsl3.pBindings = b3;
    vkCreateDescriptorSetLayout(dev, &dsl3, nullptr, &g_vk_sh.samp3_layout);

    auto make_layout = [&](VkDescriptorSetLayout dsl, uint32_t push_bytes) {
        VkPushConstantRange pcr{VK_SHADER_STAGE_FRAGMENT_BIT, 0, push_bytes};
        VkPipelineLayoutCreateInfo plci{};
        plci.sType = VK_STRUCTURE_TYPE_PIPELINE_LAYOUT_CREATE_INFO;
        plci.setLayoutCount = 1;
        plci.pSetLayouts = &dsl;
        plci.pushConstantRangeCount = push_bytes > 0 ? 1u : 0u;
        plci.pPushConstantRanges = push_bytes > 0 ? &pcr : nullptr;
        VkPipelineLayout out = VK_NULL_HANDLE;
        vkCreatePipelineLayout(dev, &plci, nullptr, &out);
        return out;
    };

    g_vk_sh.ao_layout = make_layout(g_vk_sh.samp1_layout, 176);
    g_vk_sh.blur_layout = make_layout(g_vk_sh.samp3_layout, 48);
    g_vk_sh.rays_mask_layout = make_layout(g_vk_sh.samp2_layout, 48);
    g_vk_sh.rays_blur_layout = make_layout(g_vk_sh.samp1_layout, 32);
    g_vk_sh.rays_split_layout = make_layout(g_vk_sh.samp1_layout, 32);
    g_vk_sh.rays_add_layout = make_layout(g_vk_sh.samp1_layout, 8);

    g_vk_sh.vs_mod = vk::create_shader_module(vk::shaders::kMrpAoVs, sizeof(vk::shaders::kMrpAoVs));
    g_vk_sh.ao_fs_mod = vk::create_shader_module(vk::shaders::kMrpAoFs, sizeof(vk::shaders::kMrpAoFs));
    g_vk_sh.blur_fs_mod = vk::create_shader_module(vk::shaders::kMrpAoBlurFs, sizeof(vk::shaders::kMrpAoBlurFs));
    g_vk_sh.blur_sep_fs_mod = vk::create_shader_module(vk::shaders::kMrpAoBlurSepFs, sizeof(vk::shaders::kMrpAoBlurSepFs));
    g_vk_sh.rays_mask_fs_mod = vk::create_shader_module(vk::shaders::kMrpRaysMaskFs, sizeof(vk::shaders::kMrpRaysMaskFs));
    g_vk_sh.rays_blur_fs_mod = vk::create_shader_module(vk::shaders::kMrpRaysBlurFs, sizeof(vk::shaders::kMrpRaysBlurFs));
    g_vk_sh.rays_split_fs_mod = vk::create_shader_module(vk::shaders::kMrpRaysSplitFs, sizeof(vk::shaders::kMrpRaysSplitFs));
    g_vk_sh.rays_add_fs_mod = vk::create_shader_module(vk::shaders::kMrpRaysAddFs, sizeof(vk::shaders::kMrpRaysAddFs));

    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.ao_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.ao_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = false;
        d.color_format = VK_FORMAT_R8_UNORM;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.ao_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.blur_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.blur_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = true;
        d.src_color_blend = VK_BLEND_FACTOR_DST_COLOR;
        d.dst_color_blend = VK_BLEND_FACTOR_ZERO;
        d.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
        d.color_format = VK_FORMAT_R8G8B8A8_UNORM;
        d.has_depth_attachment = true;
        d.has_stencil_attachment = true;
        g_vk_sh.blur_pipe = vk::create_graphics_pipeline(d);
        d.has_depth_attachment = false;  // (into the colour alone: the depth sampled meanwhile)
        d.has_stencil_attachment = false;
        g_vk_sh.blur_nd_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.blur_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.blur_sep_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = false;
        d.color_write_mask = VK_COLOR_COMPONENT_R_BIT;
        d.color_format = VK_FORMAT_R8_UNORM;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.blur_sep_h_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.blur_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.blur_sep_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = true;
        d.src_color_blend = VK_BLEND_FACTOR_DST_COLOR;
        d.dst_color_blend = VK_BLEND_FACTOR_ZERO;
        d.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
        d.color_format = VK_FORMAT_R8G8B8A8_UNORM;
        d.has_depth_attachment = true;
        d.has_stencil_attachment = true;
        g_vk_sh.blur_sep_v_pipe = vk::create_graphics_pipeline(d);
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.blur_sep_v_nd_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.rays_mask_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.rays_mask_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = false;
        d.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.rays_mask_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.rays_blur_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.rays_blur_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = false;
        d.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.rays_blur_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.rays_split_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.rays_split_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = false;
        d.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
        d.has_depth_attachment = false;
        d.has_stencil_attachment = false;
        g_vk_sh.rays_split_pipe = vk::create_graphics_pipeline(d);
    }
    {
        vk::GraphicsPipelineDesc d{};
        d.layout = g_vk_sh.rays_add_layout;
        d.vs = g_vk_sh.vs_mod;
        d.fs = g_vk_sh.rays_add_fs_mod;
        d.depth_test = false;
        d.depth_write = false;
        d.blend_enable = true;
        d.src_color_blend = VK_BLEND_FACTOR_ONE_MINUS_DST_COLOR;  // (a screen, as sun_rays_pass)
        d.dst_color_blend = VK_BLEND_FACTOR_ONE;
        d.color_write_mask = VK_COLOR_COMPONENT_R_BIT | VK_COLOR_COMPONENT_G_BIT | VK_COLOR_COMPONENT_B_BIT;
        d.color_format = VK_FORMAT_R8G8B8A8_UNORM;
        d.has_depth_attachment = true;
        d.has_stencil_attachment = true;
        g_vk_sh.rays_add_pipe = vk::create_graphics_pipeline(d);
    }
}

VkDescriptorSet alloc_samp1_ds(VkImageView view, VkSampler sampler) {
    VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_sh.samp1_layout);
    VkDescriptorImageInfo dii{sampler, view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL};
    VkWriteDescriptorSet w{};
    w.sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    w.dstSet = ds;
    w.dstBinding = 0;
    w.descriptorCount = 1;
    w.descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
    w.pImageInfo = &dii;
    vkUpdateDescriptorSets(vk::device(), 1, &w, 0, nullptr);
    return ds;
}

VkDescriptorSet alloc_samp3_ds(VkImageView v0, VkSampler s0, VkImageView v1, VkSampler s1, VkImageView v2, VkSampler s2) {
    VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_sh.samp3_layout);
    const VkDescriptorImageInfo dii[3] = {
        {s0, v0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {s1, v1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {s2, v2, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };
    VkWriteDescriptorSet w[3]{};
    for (uint32_t i = 0; i < 3; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = ds;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].pImageInfo = &dii[i];
    }
    vkUpdateDescriptorSets(vk::device(), 3, w, 0, nullptr);
    return ds;
}

VkDescriptorSet alloc_samp2_ds(VkImageView v0, VkSampler s0, VkImageView v1, VkSampler s1) {
    VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_sh.samp2_layout);
    VkDescriptorImageInfo dii[2] = {
        {s0, v0, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {s1, v1, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };
    VkWriteDescriptorSet w[2]{};
    for (uint32_t i = 0; i < 2; ++i) {
        w[i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        w[i].dstSet = ds;
        w[i].dstBinding = i;
        w[i].descriptorCount = 1;
        w[i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        w[i].pImageInfo = &dii[i];
    }
    vkUpdateDescriptorSets(vk::device(), 2, w, 0, nullptr);
    return ds;
}

// orig: (host) the ambient occlusion's mode (g_ao) as the passes take it: 1 SSAO, 2 SSAO+, 3 HBAO, 4 GTAO; `half` at half
// resolution (5 GTAO 1/2, 6 SSAO 1/2, 7 SSAO+ 1/2, 8 HBAO 1/2: Options > Graphics > Ambient Occlusion), upsampled by the separable
// blur's first pass with the full resolution's depth
int ao_base_mode(bool* half) {
    *half = g_ao >= 5;
    return g_ao <= 4 ? std::max(g_ao, 1) : g_ao == 5 ? 4 : std::clamp(g_ao - 5, 1, 3);
}

void vk_ao_and_rays_pass(const int vp[4]) {
    if (g_ao == 0 && g_sun_rays <= 0) return;
    ensure_vk_ao_pipelines();
    int prev_vp[4] = {};
    vk::get_viewport(prev_vp);
    vk::set_scissor(false);
    const VkSampler samp_nearest = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST,
                                                   VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    const VkSampler samp_linear = vk::get_sampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                                                  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    int prof_zone = rac1::prof::begin(rac1::prof::kAo);  // (the debug menu's profiler: the occlusion, then the rays)
    if (g_ao != 0) {
        const int w = vp[2], h = vp[3];
        bool half_res = false;
        const int base = ao_base_mode(&half_res);
        const int aw = half_res ? std::max(1, w / 2) : w;
        const int ah = half_res ? std::max(1, h / 2) : h;
        const VkFormat dfmt = vk::depth_stencil_format();
        vk::GpuImage& sdepth = vk::active_depth_image();
        const VkImageAspectFlags daspect = sdepth.aspect;
        // the depth the passes read: the 3D's own (its view's rectangle, uDMap), or a copy of that rectangle. The copy of a
        // depth-and-stencil image cost about 1.1 of the occlusion's 4.0 ms (5053 x 1371, Radeon 890M)
        static const bool want_direct = env_int("RAC1_AO_DEPTH_COPY", 0) == 0;
        bool direct = want_direct && sdepth.depth_view != VK_NULL_HANDLE && sdepth.width > 0 && sdepth.height > 0 &&
                      vp[0] >= 0 && vp[1] >= 0 && vp[0] + w <= static_cast<int>(sdepth.width) && vp[1] + h <= static_cast<int>(sdepth.height);
        if (direct && (daspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0 && g_vk_sh.ao_sdepth_img != sdepth.image) {  // (its stencil's view)
            if (g_vk_sh.ao_sdepth_stencil != VK_NULL_HANDLE) {
                vk::wait_idle();
                vkDestroyImageView(vk::device(), g_vk_sh.ao_sdepth_stencil, nullptr);
                g_vk_sh.ao_sdepth_stencil = VK_NULL_HANDLE;
            }
            VkImageViewCreateInfo vci{};
            vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vci.image = sdepth.image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = sdepth.format;
            vci.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
            if (vkCreateImageView(vk::device(), &vci, nullptr, &g_vk_sh.ao_sdepth_stencil) != VK_SUCCESS) g_vk_sh.ao_sdepth_stencil = VK_NULL_HANDLE;
            g_vk_sh.ao_sdepth_img = sdepth.image;
        }
        if ((daspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0 && g_vk_sh.ao_sdepth_stencil == VK_NULL_HANDLE) direct = false;
        const glm::vec4 smap = direct ? glm::vec4(static_cast<float>(vp[0]) / static_cast<float>(sdepth.width), static_cast<float>(vp[1]) / static_cast<float>(sdepth.height),
                                                  static_cast<float>(w) / static_cast<float>(sdepth.width), static_cast<float>(h) / static_cast<float>(sdepth.height))
                                      : glm::vec4(0.0f, 0.0f, 1.0f, 1.0f);
        const glm::vec4 dmap = smap;  // (the depth and its stencil: the same image)
        if (g_vk_sh.ao_w != w || g_vk_sh.ao_h != h || g_vk_sh.ao_aw != aw || g_vk_sh.ao_ah != ah || g_vk_sh.ao_atex.image == VK_NULL_HANDLE ||
            (!direct && g_vk_sh.ao_dtex.image == VK_NULL_HANDLE)) {
            g_vk_sh.ao_w = w;
            g_vk_sh.ao_h = h;
            g_vk_sh.ao_aw = aw;
            g_vk_sh.ao_ah = ah;
            if (g_vk_sh.ao_stencil_view != VK_NULL_HANDLE) {  // (a new size: rare; the old view waited for)
                vk::wait_idle();
                vkDestroyImageView(vk::device(), g_vk_sh.ao_stencil_view, nullptr);
                g_vk_sh.ao_stencil_view = VK_NULL_HANDLE;
            }
            if (direct) vk::destroy_image(g_vk_sh.ao_dtex);  // (no copy kept)
            else vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), dfmt,
                                  VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, daspect, g_vk_sh.ao_dtex);
            if (!direct && (daspect & VK_IMAGE_ASPECT_STENCIL_BIT) != 0) {
                VkImageViewCreateInfo vci{};
                vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
                vci.image = g_vk_sh.ao_dtex.image;
                vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
                vci.format = dfmt;
                vci.subresourceRange = {VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, 0, 1};
                vkCreateImageView(vk::device(), &vci, nullptr, &g_vk_sh.ao_stencil_view);
            }
            vk::create_image(static_cast<uint32_t>(aw), static_cast<uint32_t>(ah), VK_FORMAT_R8_UNORM,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, g_vk_sh.ao_atex);
            vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8_UNORM,
                             VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, g_vk_sh.ao_btex);
        }
        if (direct) vk::transition_image(vk::cmd(), sdepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        else vk::copy_image(sdepth, vp[0], vp[1], g_vk_sh.ao_dtex, 0, 0, w, h, daspect,
                            VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        const VkImageView dview = direct ? sdepth.depth_view : g_vk_sh.ao_dtex.view;

        float vpf[16];
        mrp_world_view_proj(vpf);
        const glm::mat4 VP = glm::make_mat4(vpf), IVP = glm::inverse(VP);
        static const int env_r = env_int("RAC1_AO_RADIUS", -1), env_s = env_int("RAC1_AO_STRENGTH", -1);
        static const float kRadius[6] = {0.0f, 1.5f, 1.5f, 3.0f, 3.0f, 3.0f}, kStrength[6] = {0.0f, 1.0f, 1.2f, 2.0f, 2.5f, 2.5f};
        const int mi = base;
        const float radius = env_r >= 0 ? static_cast<float>(env_r) / 100.0f : kRadius[mi];
        // (Options > Graphics > AO Intensity, as mrp_lighting_ao_pass)
        const float strength = (env_s >= 0 ? static_cast<float>(env_s) / 100.0f : kStrength[mi]) * static_cast<float>(g_ao_intensity) / 100.0f;
        const int shader_mode = base;

        // Pass 1: raw AO into ao_atex
        vk::transition_image(vk::cmd(), g_vk_sh.ao_atex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk::RenderTarget rt_a{};
        rt_a.color_view = g_vk_sh.ao_atex.view;
        rt_a.color_format = VK_FORMAT_R8_UNORM;
        rt_a.width = static_cast<uint32_t>(aw);
        rt_a.height = static_cast<uint32_t>(ah);
        vk::set_viewport(0, 0, aw, ah);
        vk::push_target(rt_a);
        vk::ensure_rendering();
        {
            struct AoPush {
                glm::mat4 uInvVP;
                glm::mat4 uVP;
                glm::vec4 uCam;
                glm::vec4 uParam;
                glm::vec4 uDMap;
            } pc{IVP, VP, glm::vec4(g_sh.cam, 0.0f), glm::vec4(radius, strength, 0.02f, static_cast<float>(shader_mode)), dmap};
            static_assert(sizeof(AoPush) == 176);
            VkCommandBuffer cb = vk::cmd();
            VkDescriptorSet ds = alloc_samp1_ds(dview, samp_nearest);
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.ao_pipe);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.ao_layout, 0, 1, &ds, 0, nullptr);
            vkCmdPushConstants(cb, g_vk_sh.ao_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
            vkCmdDraw(cb, 3, 1, 0, 0);
        }
        vk::pop_target();
        vk::transition_image(vk::cmd(), g_vk_sh.ao_atex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

        // (the mobys' pixels, marked in the stencil, with their share of the occlusion: as mrp_lighting_ao_pass)
        static const float moby_k = static_cast<float>(std::clamp(env_int("RAC1_AO_MOBY", 40), 0, 100)) / 100.0f;
        const VkImageView sview = direct ? g_vk_sh.ao_sdepth_stencil : g_vk_sh.ao_stencil_view;
        const VkImageView stencil_view = sview != VK_NULL_HANDLE ? sview : vk::black_uint_texture().view;
        const float moby = g_ao_mark.done && sview != VK_NULL_HANDLE ? moby_k : 1.0f;
        // (the final pass into the 3D's colour: with the depth sampled, a target of that colour alone)
        auto final_target = [&] {
            if (!direct) return;
            vk::GpuImage& scolor = vk::active_color_image();
            vk::transition_image(vk::cmd(), scolor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vk::RenderTarget rt_c{};
            rt_c.color_view = scolor.view;
            rt_c.color_format = scolor.format;
            rt_c.width = scolor.width;
            rt_c.height = scolor.height;
            vk::push_target(rt_c);
        };
        if (base == 1 && !half_res) {  // (SSAO 1/2: the separable blur below, its first pass the upsample)
            final_target();
            vk::set_viewport(vp[0], vp[1], w, h);
            vk::ensure_rendering();
            VkCommandBuffer cb = vk::cmd();
            VkDescriptorSet ds = alloc_samp3_ds(g_vk_sh.ao_atex.view, samp_nearest, dview, samp_nearest, stencil_view, samp_nearest);
            const float texel[12] = {1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h), moby, 0.0f, dmap.x, dmap.y, dmap.z, dmap.w, smap.x, smap.y, smap.z, smap.w};
            vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, direct ? g_vk_sh.blur_nd_pipe : g_vk_sh.blur_pipe);
            vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.blur_layout, 0, 1, &ds, 0, nullptr);
            vkCmdPushConstants(cb, g_vk_sh.blur_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(texel), texel);
            vkCmdDraw(cb, 3, 1, 0, 0);
        } else {
            vk::transition_image(vk::cmd(), g_vk_sh.ao_btex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            vk::RenderTarget rt_b{};
            rt_b.color_view = g_vk_sh.ao_btex.view;
            rt_b.color_format = VK_FORMAT_R8_UNORM;
            rt_b.width = static_cast<uint32_t>(w);
            rt_b.height = static_cast<uint32_t>(h);
            vk::set_viewport(0, 0, w, h);
            vk::push_target(rt_b);
            vk::ensure_rendering();
            {
                VkCommandBuffer cb = vk::cmd();
                VkDescriptorSet ds = alloc_samp3_ds(g_vk_sh.ao_atex.view, half_res ? samp_linear : samp_nearest,
                                                    dview, samp_nearest, stencil_view, samp_nearest);
                const float texel_h[12] = {1.0f / static_cast<float>(aw), 0.0f, 1.0f, 0.0f, dmap.x, dmap.y, dmap.z, dmap.w, smap.x, smap.y, smap.z, smap.w};  // (the share taken at the end, as mrp_lighting_ao_pass)
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.blur_sep_h_pipe);
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.blur_layout, 0, 1, &ds, 0, nullptr);
                vkCmdPushConstants(cb, g_vk_sh.blur_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(texel_h), texel_h);
                vkCmdDraw(cb, 3, 1, 0, 0);
            }
            vk::pop_target();
            vk::transition_image(vk::cmd(), g_vk_sh.ao_btex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

            final_target();
            vk::set_viewport(vp[0], vp[1], w, h);
            vk::ensure_rendering();
            {
                VkCommandBuffer cb = vk::cmd();
                VkDescriptorSet ds = alloc_samp3_ds(g_vk_sh.ao_btex.view, samp_nearest, dview, samp_nearest, stencil_view, samp_nearest);
                const float texel_v[12] = {0.0f, 1.0f / static_cast<float>(h), moby, 0.0f, dmap.x, dmap.y, dmap.z, dmap.w, smap.x, smap.y, smap.z, smap.w};
                vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, direct ? g_vk_sh.blur_sep_v_nd_pipe : g_vk_sh.blur_sep_v_pipe);
                vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.blur_layout, 0, 1, &ds, 0, nullptr);
                vkCmdPushConstants(cb, g_vk_sh.blur_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(texel_v), texel_v);
                vkCmdDraw(cb, 3, 1, 0, 0);
            }
        }
        if (direct) {  // (the 3D's target again, its depth an attachment again)
            vk::pop_target();
            vk::transition_image(vk::cmd(), sdepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        }
    }

    rac1::prof::end(prof_zone);
    prof_zone = g_sun_rays > 0 ? rac1::prof::begin(rac1::prof::kSunRays) : -1;
    if (sun_rays_now()) {
        float vpf[16];
        mrp_world_view_proj(vpf);
        const glm::mat4 VP = glm::make_mat4(vpf);
        const glm::vec4 c = VP * glm::vec4(g_sh.cam + g_sh.sun * 2000.0f, 1.0f);
        if (c.w > 0.0f) {
            const glm::vec2 su = glm::vec2(c.x, c.y) / c.w * 0.5f + 0.5f;
            const float out = std::max({-su.x, su.x - 1.0f, -su.y, su.y - 1.0f, 0.0f});
            const float fade = std::clamp(1.0f - out / 0.6f, 0.0f, 1.0f);
            if (fade > 0.0f) {
                const RaysStep& step = kRaysSteps[std::clamp(g_sun_rays, 1, 3) - 1];  // (the three steps, as sun_rays_pass)
                const int w = vp[2], h = vp[3], hw = std::max(1, w / step.div), hh = std::max(1, h / step.div);
                const VkFormat dfmt = vk::depth_stencil_format();
                const VkImageAspectFlags daspect = vk::active_depth_image().aspect;
                (void)dfmt;
                (void)daspect;
                if (g_vk_sh.rays_w != w || g_vk_sh.rays_h != h || g_vk_sh.rays_div != step.div || g_vk_sh.rays_mtex.image == VK_NULL_HANDLE) {
                    g_vk_sh.rays_w = w;
                    g_vk_sh.rays_h = h;
                    g_vk_sh.rays_div = step.div;
                    vk::create_image(static_cast<uint32_t>(hw), static_cast<uint32_t>(hh), VK_FORMAT_R16G16B16A16_SFLOAT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_ASPECT_COLOR_BIT, g_vk_sh.rays_mtex);
                    vk::create_image(static_cast<uint32_t>(hw), static_cast<uint32_t>(hh), VK_FORMAT_R16G16B16A16_SFLOAT,
                                     VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_ASPECT_COLOR_BIT, g_vk_sh.rays_rtex);
                }
                // the 3D's depth (nearest) and colour (linear) sampled where the step's texels fall: what the GL path's blits to
                // the step's size give, without the copies (a depth blit is not portable in Vulkan either)
                vk::GpuImage& sdepth = vk::active_depth_image();
                vk::GpuImage& scolor = vk::active_color_image();
                vk::transition_image(vk::cmd(), sdepth, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                vk::transition_image(vk::cmd(), scolor, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                const glm::vec4 src_map(static_cast<float>(vp[0]) / static_cast<float>(scolor.width), static_cast<float>(vp[1]) / static_cast<float>(scolor.height),
                                        static_cast<float>(w) / static_cast<float>(scolor.width), static_cast<float>(h) / static_cast<float>(scolor.height));

                const float tmax = std::max({g_sh.key.x, g_sh.key.y, g_sh.key.z, 1e-3f});
                const glm::vec3 tint = g_sh.key.x + g_sh.key.y + g_sh.key.z > 0.0f ? glm::vec3(g_sh.key) / tmax : glm::vec3(1.0f, 0.95f, 0.85f);

                // (1) mask
                vk::transition_image(vk::cmd(), g_vk_sh.rays_mtex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                vk::RenderTarget rt_m{};
                rt_m.color_view = g_vk_sh.rays_mtex.view;
                rt_m.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
                rt_m.width = static_cast<uint32_t>(hw);
                rt_m.height = static_cast<uint32_t>(hh);
                vk::set_viewport(0, 0, hw, hh);
                vk::push_target(rt_m);
                vk::ensure_rendering();
                {
                    struct MaskPush {
                        glm::vec2 uSun;
                        glm::vec2 uAspect;
                        glm::vec4 uTint;
                        glm::vec4 uSrc;  // (the view's place in the 3D's buffers: offset, size, 0..1)
                    } pc{su, glm::vec2(static_cast<float>(w) / static_cast<float>(std::max(h, 1)), 1.0f), glm::vec4(tint, 1.0f), src_map};
                    VkCommandBuffer cb = vk::cmd();
                    VkDescriptorSet ds = alloc_samp2_ds(sdepth.depth_view != VK_NULL_HANDLE ? sdepth.depth_view : sdepth.view, samp_nearest, scolor.view, samp_linear);
                    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_mask_pipe);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_mask_layout, 0, 1, &ds, 0, nullptr);
                    vkCmdPushConstants(cb, g_vk_sh.rays_mask_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                    vkCmdDraw(cb, 3, 1, 0, 0);
                }
                vk::pop_target();
                vk::transition_image(vk::cmd(), g_vk_sh.rays_mtex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
                vk::transition_image(vk::cmd(), sdepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                vk::transition_image(vk::cmd(), scolor, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);

                // (2) radial blur
                vk::transition_image(vk::cmd(), g_vk_sh.rays_rtex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                vk::RenderTarget rt_r{};
                rt_r.color_view = g_vk_sh.rays_rtex.view;
                rt_r.color_format = VK_FORMAT_R16G16B16A16_SFLOAT;
                rt_r.width = static_cast<uint32_t>(hw);
                rt_r.height = static_cast<uint32_t>(hh);
                vk::set_viewport(0, 0, hw, hh);
                vk::push_target(rt_r);
                vk::ensure_rendering();
                {
                    struct BlurPush {
                        glm::vec2 uSun;
                        glm::vec2 uTaps;
                        glm::vec4 uParam;
                    };
                    const float decay = step.taps == 40 ? 0.945f : static_cast<float>(std::pow(0.945, 40.0 / step.taps));
                    const BlurPush pc{su, glm::vec2(static_cast<float>(step.taps), static_cast<float>(w) / static_cast<float>(std::max(h, 1))), glm::vec4(0.9f, decay, 1.0f / static_cast<float>(step.taps) * 4.0f, 1.0f)};
                    VkCommandBuffer cb = vk::cmd();
                    VkDescriptorSet ds = alloc_samp1_ds(g_vk_sh.rays_mtex.view, samp_linear);
                    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_blur_pipe);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_blur_layout, 0, 1, &ds, 0, nullptr);
                    vkCmdPushConstants(cb, g_vk_sh.rays_blur_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), &pc);
                    vkCmdDraw(cb, 3, 1, 0, 0);
                }
                vk::pop_target();
                vk::transition_image(vk::cmd(), g_vk_sh.rays_rtex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // (3) the shafts and the broad light taken apart, into the mask's image (done with)
                vk::transition_image(vk::cmd(), g_vk_sh.rays_mtex, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
                vk::set_viewport(0, 0, hw, hh);
                vk::push_target(rt_m);
                vk::ensure_rendering();
                {
                    const glm::vec3 shape = rays_open_shape();
                    const glm::vec4 pc[2] = {glm::vec4(su.x, su.y, 0.0f, 0.0f),
                                             glm::vec4(static_cast<float>(w) / static_cast<float>(std::max(h, 1)), shape.x, shape.y, shape.z)};
                    VkCommandBuffer cb = vk::cmd();
                    VkDescriptorSet ds = alloc_samp1_ds(g_vk_sh.rays_rtex.view, samp_linear);
                    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_split_pipe);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_split_layout, 0, 1, &ds, 0, nullptr);
                    vkCmdPushConstants(cb, g_vk_sh.rays_split_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
                    vkCmdDraw(cb, 3, 1, 0, 0);
                }
                vk::pop_target();
                vk::transition_image(vk::cmd(), g_vk_sh.rays_mtex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);

                // (4) add to scene
                static const float gain_env = std::getenv("RAC1_SUN_RAYS_GAIN") != nullptr ? static_cast<float>(std::atof(std::getenv("RAC1_SUN_RAYS_GAIN"))) : 1.0f;
                const float k = step.gain * gain_env * fade * std::min(g_sh.sun_k, 1.0f);
                const float cap = step.cap * std::max(fade * std::min(g_sh.sun_k, 1.0f), 1e-3f);
                if (std::getenv("RAC1_SUN_RAYS_TRACE") != nullptr) {
                    static uint64_t n = 0;
                    if (n++ % 60 == 0) std::printf("[rays] sun at %.2f %.2f of the view, fade %.2f, weight %.2f: %.2f\n", su.x, su.y, fade, g_sh.sun_k, k);
                }
                vk::set_viewport(vp[0], vp[1], w, h);
                vk::ensure_rendering();
                {
                    const float pc[2] = {k, cap};
                    VkCommandBuffer cb = vk::cmd();
                    VkDescriptorSet ds = alloc_samp1_ds(g_vk_sh.rays_mtex.view, samp_linear);
                    vkCmdBindPipeline(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_add_pipe);
                    vkCmdBindDescriptorSets(cb, VK_PIPELINE_BIND_POINT_GRAPHICS, g_vk_sh.rays_add_layout, 0, 1, &ds, 0, nullptr);
                    vkCmdPushConstants(cb, g_vk_sh.rays_add_layout, VK_SHADER_STAGE_FRAGMENT_BIT, 0, sizeof(pc), pc);
                    vkCmdDraw(cb, 3, 1, 0, 0);
                }
            }
        }
    }

    rac1::prof::end(prof_zone);
    vk::set_viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
}
}  // namespace

// orig: (host) see mrp_lighting.h
bool mrp_lighting_vk_ao_marking() { return g_vk_ao_marking; }

// orig: (host) see mrp_lighting.h
void mrp_lighting_ao_moby_mark(bool begin) {
    AoMark& M = g_ao_mark;
    if (vk::is_active()) {  // (Vulkan: the mobys' pipelines write the stencil while marking, mrp_lighting_vk_ao_marking)
        g_vk_ao_marking = begin && g_sh.on && g_ao != 0 && env_int("RAC1_AO_MOBY", 40) < 100;
        if (begin) {
            M.done = g_vk_ao_marking;
            M.active = false;
        }
        return;
    }
    if (!begin) {
        if (!M.active) return;
        M.active = false;
        glDisable(GL_STENCIL_TEST);
        glStencilMask(0xff);
        return;
    }
    M.active = false;
    M.done = false;
    static const bool all = env_int("RAC1_AO_MOBY", 40) >= 100;  // (testing: the mobys with all of the occlusion, as the world)
    if (!g_sh.on || g_ao == 0 || all) return;
    GLint fb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
    if (fb != M.stencil_fb) {  // (has the framebuffer drawn into a stencil? the scaled scene's buffer has; the window's, as it was made)
        GLint sbits = 0;
        const GLenum satt = fb == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
        glGetFramebufferAttachmentParameteriv(GL_DRAW_FRAMEBUFFER, satt, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &sbits);
        while (glGetError() != GL_NO_ERROR) {}
        M.stencil_fb = fb;
        M.stencil = sbits > 0;
    }
    if (!M.stencil) return;
    // (the stencil was cleared with the depth at the frame's start - orig_main -, nothing writes it before the mobys; written whole,
    // 1: a partial write mask on a packed depth-stencil buffer is a read-modify-write)
    glEnable(GL_STENCIL_TEST);
    glStencilFunc(GL_ALWAYS, 1, 0xff);
    glStencilOp(GL_KEEP, GL_KEEP, GL_REPLACE);
    glStencilMask(0xff);
    M.active = true;
    M.done = true;
    M.fb = fb;
}

namespace {
// ---- the materials' High step: the last frame's 3D (colour, depth) for the reflections and the lava's shimmer -----------------
struct MatSsr {
    GLuint fbo_c = 0, fbo_d = 0, tex_c = 0, tex_d = 0;  // (OpenGL)
    GLenum dfmt = 0;
    GLint last_fb = -1;
    vk::GpuImage vk_c{}, vk_d{};  // (Vulkan)
    int w = 0, h = 0;
    glm::mat4 vp{1.0f};    // the captured picture's view-projection
    uint64_t frame = ~0ull;  // the frame it was captured at (g_mat_frames): used by the next one alone
} g_mssr;
uint64_t g_mat_frames = 0;  // (the materials' clock: frames drawn with the lighting, 60 a second)
std::atomic<bool> g_mat_ssr_wanted{false};  // (this level has a water, puddle or lava surface: kept by the uniforms' fill)
// orig: (host) the High step's picture wanted now
bool mat_ssr_now() { return g_surface_detail >= 2 && g_mat_ssr_wanted.load() && g_sh.on; }
// orig: (host) the picture usable by this frame's draws (captured the frame before, at the same size)
bool mat_ssr_ready() { return mat_ssr_now() && g_mssr.frame + 1 == g_mat_frames && g_mssr.w > 0; }
// orig: (host) the Vulkan copies freed with the device
void vk_mat_shutdown() {
    vk::destroy_image_now(g_mssr.vk_c);
    vk::destroy_image_now(g_mssr.vk_d);
}
}  // namespace

// orig: (host) see mrp_lighting.h
void mrp_lighting_material_capture(const int vp[4]) {
    if (!mat_ssr_now()) return;
    const int w = vp[2], h = vp[3];
    if (w <= 0 || h <= 0) return;
    float vpf[16];
    mrp_world_view_proj(vpf);
    if (vk::is_active()) {
        static bool reg = false;
        if (!reg) {
            reg = true;
            vk::register_shutdown_callback(vk_mat_shutdown);
        }
        const VkFormat dfmt = vk::depth_stencil_format();
        const VkImageAspectFlags daspect = vk::active_depth_image().aspect;
        vk::GpuImage& scol = vk::active_color_image();
        if (g_mssr.w != w || g_mssr.h != h || g_mssr.vk_c.image == VK_NULL_HANDLE || g_mssr.vk_c.format != scol.format) {
            if (g_mssr.vk_c.image != VK_NULL_HANDLE) vk::wait_idle();
            vk::destroy_image_now(g_mssr.vk_c);
            vk::destroy_image_now(g_mssr.vk_d);
            vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), scol.format, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                             VK_IMAGE_ASPECT_COLOR_BIT, g_mssr.vk_c);
            vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), dfmt, VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, daspect,
                             g_mssr.vk_d);
            g_mssr.w = w;
            g_mssr.h = h;
        }
        vk::copy_image(scol, vp[0], vp[1], g_mssr.vk_c, 0, 0, w, h, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        vk::copy_image(vk::active_depth_image(), vp[0], vp[1], g_mssr.vk_d, 0, 0, w, h, daspect, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL,
                       VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        GLint fb = 0, rfb = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
        const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST);
        glDisable(GL_SCISSOR_TEST);
        GLenum fmt = g_mssr.dfmt;
        if (fb != g_mssr.last_fb || fmt == 0) {
            fmt = depth_format(fb);
            g_mssr.last_fb = fb;
        }
        const bool st = fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8;
        if (g_mssr.tex_c == 0) {
            glGenFramebuffers(1, &g_mssr.fbo_c);
            glGenFramebuffers(1, &g_mssr.fbo_d);
            glGenTextures(1, &g_mssr.tex_c);
            glGenTextures(1, &g_mssr.tex_d);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_mssr.tex_c, "Materials: the last frame's 3D");
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &g_mssr.tex_d, "Materials: the last frame's 3D");
        }
        if (g_mssr.w != w || g_mssr.h != h || g_mssr.dfmt != fmt) {
            GLint prev_t = 0;
            glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_t);
            auto tex = [](GLuint t, GLint ifmt, int tw, int th, GLenum f2, GLenum ty, GLint filter) {
                glBindTexture(GL_TEXTURE_2D, t);
                glTexImage2D(GL_TEXTURE_2D, 0, ifmt, tw, th, 0, f2, ty, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, filter);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            };
            tex(g_mssr.tex_c, GL_RGBA8, w, h, GL_RGBA, GL_UNSIGNED_BYTE, GL_LINEAR);
            tex(g_mssr.tex_d, static_cast<GLint>(fmt), w, h, st ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
                fmt == GL_DEPTH24_STENCIL8 ? GL_UNSIGNED_INT_24_8 : fmt == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_FLOAT, GL_NEAREST);
            glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_t));
            glBindFramebuffer(GL_FRAMEBUFFER, g_mssr.fbo_c);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_mssr.tex_c, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, g_mssr.fbo_d);
            glFramebufferTexture2D(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, g_mssr.tex_d, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            g_mssr.w = w;
            g_mssr.h = h;
            g_mssr.dfmt = fmt;
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fb));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_mssr.fbo_c);
        glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_mssr.fbo_d);
        glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT | (st ? GL_STENCIL_BUFFER_BIT : 0), GL_NEAREST);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
        if (scissor) glEnable(GL_SCISSOR_TEST);
    }
    g_mssr.vp = glm::make_mat4(vpf);
    g_mssr.frame = g_mat_frames;
}

// orig: (host) see mrp_lighting.h
void mrp_lighting_ao_pass() {
    if (!g_sh.on) return;
    int vp[4];
    if (!mrp_world_last_viewport(vp)) return;
    if (vk::is_active()) {
        screen_point_read(vp);  // (the debug menu's crosshair)
        vk_ao_and_rays_pass(vp);
        mrp_lighting_material_capture(vp);  // (after the occlusion and the rays: the picture the reflections show)
        return;
    }
    screen_point_read(vp);
    lamp_scan(vp);
    const int prof_ao = rac1::prof::begin(rac1::prof::kAo);  // (the debug menu's profiler: the cover probe and the occlusion)
    GLint fb = 0, rfb = 0, prev_prog = 0, prev_vao = 0, prev_vp[4] = {}, prev_active = 0, prev_t0 = 0, prev_t1 = 0, prev_rb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &fb);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
    glGetIntegerv(GL_CURRENT_PROGRAM, &prev_prog);
    glGetIntegerv(GL_VERTEX_ARRAY_BINDING, &prev_vao);
    glGetIntegerv(GL_VIEWPORT, prev_vp);
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev_active);
    glGetIntegerv(GL_READ_BUFFER, &prev_rb);
    glActiveTexture(GL_TEXTURE0 + 28);
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_t0);
    glActiveTexture(GL_TEXTURE0 + kUnit);
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev_t1);
    const GLboolean blend = glIsEnabled(GL_BLEND), depth = glIsEnabled(GL_DEPTH_TEST), scissor = glIsEnabled(GL_SCISSOR_TEST), cull = glIsEnabled(GL_CULL_FACE);
    GLboolean cmask[4];
    glGetBooleanv(GL_COLOR_WRITEMASK, cmask);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_CULL_FACE);
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    cover_probe(fb, vp);
    glActiveTexture(GL_TEXTURE0 + 28);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_t0));
    glActiveTexture(GL_TEXTURE0 + kUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(prev_t1));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));  // (the read buffer is the bound read framebuffer's: rfb's, not the probe's)
    glReadBuffer(static_cast<GLenum>(prev_rb));

    static const bool ao_prof = std::getenv("RAC1_AO_PROF") != nullptr;  // (testing: the occlusion's GPU time)
    static GLuint ao_q = 0;
    static double ao_acc = 0.0;
    static int ao_n = 0;
    if (ao_prof && g_ao != 0) {
        if (ao_q == 0) glGenQueries(1, &ao_q);
        glBeginQuery(GL_TIME_ELAPSED, ao_q);
    }
    if (g_ao != 0) {
        Ao& A = g_ao_s;
        const int w = vp[2], h = vp[3];
        bool half_res = false;
        const int base = ao_base_mode(&half_res);
        const int aw = half_res ? std::max(1, w / 2) : w;
        const int ah = half_res ? std::max(1, h / 2) : h;
        if (A.prog == 0) {
            A.prog = ao_link(kAoFs);
            A.blur = ao_link(kAoBlurFs);
            A.blur_sep = ao_link(kAoBlurSepFs);
            A.u_depth = glGetUniformLocation(A.prog, "uDepth");
            A.u_inv_vp = glGetUniformLocation(A.prog, "uInvVP");
            A.u_vp = glGetUniformLocation(A.prog, "uVP");
            A.u_cam = glGetUniformLocation(A.prog, "uCam");
            A.u_param = glGetUniformLocation(A.prog, "uParam");
            A.u_b_depth = glGetUniformLocation(A.blur, "uDepth");
            A.u_b_ao = glGetUniformLocation(A.blur, "uAo");
            A.u_b_texel = glGetUniformLocation(A.blur, "uTexel");
            A.u_b_st = glGetUniformLocation(A.blur, "uStencil");
            A.u_b_moby = glGetUniformLocation(A.blur, "uMoby");
            A.u_bs_depth = glGetUniformLocation(A.blur_sep, "uDepth");
            A.u_bs_ao = glGetUniformLocation(A.blur_sep, "uAo");
            A.u_bs_texel = glGetUniformLocation(A.blur_sep, "uTexel");
            A.u_bs_st = glGetUniformLocation(A.blur_sep, "uStencil");
            A.u_bs_moby = glGetUniformLocation(A.blur_sep, "uMoby");
            glGenVertexArrays(1, &A.vao);
            glGenFramebuffers(1, &A.dfbo);
            glGenFramebuffers(1, &A.afbo);
            glGenFramebuffers(1, &A.bfbo);
            glGenTextures(1, &A.atex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &A.atex, "Ambient occlusion");
            glGenTextures(1, &A.btex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &A.btex, "Ambient occlusion");
        }
        GLenum fmt = A.dfmt;
        if (fb != A.last_fb || fmt == 0) {
            // the depth format of the framebuffer drawn into (the blit wants the same)
            GLint dbits = 24, sbits = 8, dtype = GL_UNSIGNED_NORMALIZED;
            glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(fb));
            const GLenum datt = fb == 0 ? GL_DEPTH : GL_DEPTH_ATTACHMENT, satt = fb == 0 ? GL_STENCIL : GL_STENCIL_ATTACHMENT;
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_DEPTH_SIZE, &dbits);
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, datt, GL_FRAMEBUFFER_ATTACHMENT_COMPONENT_TYPE, &dtype);
            glGetFramebufferAttachmentParameteriv(GL_FRAMEBUFFER, satt, GL_FRAMEBUFFER_ATTACHMENT_STENCIL_SIZE, &sbits);
            while (glGetError() != GL_NO_ERROR) {}
            fmt = dtype == GL_FLOAT ? (sbits > 0 ? GL_DEPTH32F_STENCIL8 : GL_DEPTH_COMPONENT32F)
                                    : (sbits > 0 ? GL_DEPTH24_STENCIL8 : (dbits >= 32 ? GL_DEPTH_COMPONENT32 : dbits >= 24 ? GL_DEPTH_COMPONENT24 : GL_DEPTH_COMPONENT16));
            A.last_fb = fb;
        }
        if (A.w != w || A.h != h || A.aw != aw || A.ah != ah || A.dfmt != fmt) {
            A.w = w;
            A.h = h;
            A.aw = aw;
            A.ah = ah;
            A.dfmt = fmt;
            // (immutable storage, made again at each size: the stencil's view needs it; GL 4.2 / 4.3, not in the loader's header)
            using PfnTexStorage2D = void(APIENTRYP)(GLenum, GLsizei, GLenum, GLsizei, GLsizei);
            using PfnTextureView = void(APIENTRYP)(GLuint, GLenum, GLuint, GLenum, GLuint, GLuint, GLuint, GLuint);
            static const PfnTexStorage2D tex_storage = reinterpret_cast<PfnTexStorage2D>(glfwGetProcAddress("glTexStorage2D"));
            static const PfnTextureView texture_view = reinterpret_cast<PfnTextureView>(glfwGetProcAddress("glTextureView"));
            constexpr GLenum kDepthStencilTextureMode = 0x90ea;  // (GL_DEPTH_STENCIL_TEXTURE_MODE, GL 4.3)
            if (A.stex != 0) glDeleteTextures(1, &A.stex);
            if (A.dtex != 0) glDeleteTextures(1, &A.dtex);
            A.stex = 0;
            glGenTextures(1, &A.dtex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &A.dtex, "Ambient occlusion");
            glBindTexture(GL_TEXTURE_2D, A.dtex);
            const bool st = fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8;
            if (tex_storage != nullptr && texture_view != nullptr) {
                tex_storage(GL_TEXTURE_2D, 1, fmt, w, h);
            } else {  // (no view: the mobys get all of the occlusion)
                glTexImage2D(GL_TEXTURE_2D, 0, static_cast<GLint>(fmt), w, h, 0, st ? GL_DEPTH_STENCIL : GL_DEPTH_COMPONENT,
                             fmt == GL_DEPTH24_STENCIL8 ? GL_UNSIGNED_INT_24_8 : fmt == GL_DEPTH32F_STENCIL8 ? GL_FLOAT_32_UNSIGNED_INT_24_8_REV : GL_FLOAT, nullptr);
            }
            if (st && tex_storage != nullptr && texture_view != nullptr) {
                glGenTextures(1, &A.stex);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &A.stex, "Ambient occlusion");
                texture_view(A.stex, GL_TEXTURE_2D, A.dtex, fmt, 0, 1, 0, 1);
                glBindTexture(GL_TEXTURE_2D, A.stex);
                glTexParameteri(GL_TEXTURE_2D, kDepthStencilTextureMode, GL_STENCIL_INDEX);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
                glBindTexture(GL_TEXTURE_2D, A.dtex);
            }
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, A.dfbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, st ? GL_DEPTH_STENCIL_ATTACHMENT : GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, A.dtex, 0);
            glDrawBuffer(GL_NONE);
            glReadBuffer(GL_NONE);
            glBindTexture(GL_TEXTURE_2D, A.atex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, aw, ah, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, half_res ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, half_res ? GL_LINEAR : GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, A.afbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, A.atex, 0);
            glBindTexture(GL_TEXTURE_2D, A.btex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_R8, w, h, 0, GL_RED, GL_UNSIGNED_BYTE, nullptr);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
            glBindFramebuffer(GL_FRAMEBUFFER, A.bfbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, A.btex, 0);
        }
        // the depth: the 3D's viewport of the framebuffer into the texture
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(fb));
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, A.dfbo);
        const bool st = fmt == GL_DEPTH24_STENCIL8 || fmt == GL_DEPTH32F_STENCIL8;
        glBlitFramebuffer(vp[0], vp[1], vp[0] + w, vp[1] + h, 0, 0, w, h, GL_DEPTH_BUFFER_BIT | (st ? GL_STENCIL_BUFFER_BIT : 0), GL_NEAREST);
        GLint bsrc = 0, bdst = 0, bsrca = 0, bdsta = 0, prev_tex = 0, prev_tex1 = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
        glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrca);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &bdsta);
        GLboolean dmask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &dmask);
        glDepthMask(GL_FALSE);
        glBindVertexArray(A.vao);
        // the occlusion
        float vpf[16];
        mrp_world_view_proj(vpf);
        const glm::mat4 VP = glm::make_mat4(vpf), IVP = glm::inverse(VP);
        // the radius (world units) and strength per mode. Until 0.4.x every mode used 0.9 units and 70 %: on these scenes (a crate
        // about 2 units, Ratchet about 1.5, buildings tens) that darkened 1 to 2 levels of 255 on average, so the five modes looked
        // the same (and none showed). The sphere-sampling modes 1.5 units, the horizon-based ones 3 (their falloff is smoother);
        // RAC1_AO_RADIUS / RAC1_AO_STRENGTH (percent, testing) override them
        static const int env_r = env_int("RAC1_AO_RADIUS", -1), env_s = env_int("RAC1_AO_STRENGTH", -1);
        static const float kRadius[6] = {0.0f, 1.5f, 1.5f, 3.0f, 3.0f, 3.0f}, kStrength[6] = {0.0f, 1.0f, 1.2f, 2.0f, 2.5f, 2.5f};
        const int mi = base;
        const float radius = env_r >= 0 ? static_cast<float>(env_r) / 100.0f : kRadius[mi];
        // (Options > Graphics > AO Intensity: the mode's strength scaled, 75 % by default)
        const float strength = (env_s >= 0 ? static_cast<float>(env_s) / 100.0f : kStrength[mi]) * static_cast<float>(g_ao_intensity) / 100.0f;
        const int shader_mode = base;
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, A.afbo);
        glViewport(0, 0, aw, ah);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glUseProgram(A.prog);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex);
        glBindTexture(GL_TEXTURE_2D, A.dtex);
        glUniform1i(A.u_depth, 0);
        glUniformMatrix4fv(A.u_inv_vp, 1, GL_FALSE, glm::value_ptr(IVP));
        glUniformMatrix4fv(A.u_vp, 1, GL_FALSE, glm::value_ptr(VP));
        glUniform3f(A.u_cam, g_sh.cam.x, g_sh.cam.y, g_sh.cam.z);
        glUniform4f(A.u_param, radius, strength, 0.02f, static_cast<float>(shader_mode));
        glDrawArrays(GL_TRIANGLES, 0, 3);

        if (std::getenv("RAC1_AO_DUMP") != nullptr) {  // (testing: the raw occlusion, ao_<mode>.pgm written every frame - the last one stays -, its stats every 60)
            static int n = 0;
            ++n;
            {
                std::vector<uint8_t> px(static_cast<size_t>(aw) * ah);
                glBindTexture(GL_TEXTURE_2D, A.atex);
                glPixelStorei(GL_PACK_ALIGNMENT, 1);
                glGetTexImage(GL_TEXTURE_2D, 0, GL_RED, GL_UNSIGNED_BYTE, px.data());
                glPixelStorei(GL_PACK_ALIGNMENT, 4);
                char fn[64];
                std::snprintf(fn, sizeof fn, "ao_%d.pgm", g_ao);
                if (FILE* f = std::fopen(fn, "wb")) {
                    std::fprintf(f, "P5 %d %d 255\n", aw, ah);
                    for (int y = ah - 1; y >= 0; --y) std::fwrite(px.data() + static_cast<size_t>(y) * aw, 1, static_cast<size_t>(aw), f);
                    std::fclose(f);
                }
                double sum = 0.0;
                int dark = 0;
                for (uint8_t v : px) {
                    sum += v;
                    dark += v < 200 ? 1 : 0;
                }
                if (n % 60 == 0) std::printf("[ao] mode %d (shader %d) %dx%d: mean %.1f / 255, %.1f %% under 200; radius %.2f strength %.2f\n", g_ao, shader_mode, aw, ah,
                            sum / static_cast<double>(px.size()), 100.0 * dark / static_cast<double>(px.size()), radius, strength);
            }
        }
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex1);
        // the multiply into the colour: the world's pixels with all of the occlusion, the mobys' (stencil bit 0, marked this
        // frame into this framebuffer, read through dtex's stencil view) with their share
        static const float moby_k = static_cast<float>(std::clamp(env_int("RAC1_AO_MOBY", 40), 0, 100)) / 100.0f;
        const float moby = g_ao_mark.done && g_ao_mark.fb == fb && A.stex != 0 ? moby_k : 1.0f;
        GLint prev_tex2 = 0;
        glActiveTexture(GL_TEXTURE2);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_tex2);
        glBindTexture(GL_TEXTURE_2D, A.stex);
        glActiveTexture(GL_TEXTURE1);
        if (base == 1 && !half_res) {  // (SSAO 1/2: the separable blur below, its first pass the upsample)
            // original SSAO: 5x5 box depth-threshold blur multiplied into the colour
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
            glViewport(vp[0], vp[1], w, h);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            glUseProgram(A.blur);
            glBindTexture(GL_TEXTURE_2D, A.atex);
            glUniform1i(A.u_b_depth, 0);
            glUniform1i(A.u_b_ao, 1);
            glUniform2f(A.u_b_texel, 1.0f / static_cast<float>(w), 1.0f / static_cast<float>(h));
            glUniform1i(A.u_b_st, 2);
            glUniform1f(A.u_b_moby, moby);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        } else {
            // SSAO+ / HBAO / GTAO / GTAO 1/2: 9-tap separable bilateral depth-weighted blur (horizontal -> btex, vertical -> fb)
            glUseProgram(A.blur_sep);
            glUniform1i(A.u_bs_depth, 0);
            glUniform1i(A.u_bs_ao, 1);
            glUniform1i(A.u_bs_st, 2);
            // Pass 1: horizontal blur (also upsamples half-res atex to full-res btex via full-res depth guidance)
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, A.bfbo);
            glViewport(0, 0, w, h);
            glColorMask(GL_TRUE, GL_FALSE, GL_FALSE, GL_FALSE);
            glDisable(GL_BLEND);
            glBindTexture(GL_TEXTURE_2D, A.atex);
            glUniform2f(A.u_bs_texel, 1.0f / static_cast<float>(aw), 0.0f);
            glUniform1f(A.u_bs_moby, 1.0f);  // (the share taken at the end: the blur mixes the mobys' edges with the world's)
            glDrawArrays(GL_TRIANGLES, 0, 3);
            // Pass 2: vertical blur multiplied into the colour
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
            glViewport(vp[0], vp[1], w, h);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_FALSE);
            glEnable(GL_BLEND);
            glBlendFunc(GL_DST_COLOR, GL_ZERO);
            glBindTexture(GL_TEXTURE_2D, A.btex);
            glUniform2f(A.u_bs_texel, 0.0f, 1.0f / static_cast<float>(h));
            glUniform1f(A.u_bs_moby, moby);
            glDrawArrays(GL_TRIANGLES, 0, 3);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex1));
        glActiveTexture(GL_TEXTURE2);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex2));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_tex));
        glDepthMask(dmask);
        glBlendFuncSeparate(static_cast<GLenum>(bsrc), static_cast<GLenum>(bdst), static_cast<GLenum>(bsrca), static_cast<GLenum>(bdsta));
    }
    g_ao_mark.done = false;
    if (ao_prof && g_ao != 0) {
        glEndQuery(GL_TIME_ELAPSED);
        GLuint64 ns = 0;
        glGetQueryObjectui64v(ao_q, GL_QUERY_RESULT, &ns);
        ao_acc += static_cast<double>(ns) * 1e-6;
        if (++ao_n == 60) {
            std::printf("[ao] mode %d: GPU %.3f ms a frame (%dx%d)\n", g_ao, ao_acc / ao_n, vp[2], vp[3]);
            ao_acc = 0.0;
            ao_n = 0;
        }
    }
    rac1::prof::end(prof_ao);
    if (g_sun_rays > 0) {  // the sun's rays (their own textures; the blending and the bindings put back)
        RAC1_PROF_ZONE(kSunRays);
        GLint bsrc = 0, bdst = 0, bsrca = 0, bdsta = 0, t0 = 0, t1 = 0;
        glGetIntegerv(GL_BLEND_SRC_RGB, &bsrc);
        glGetIntegerv(GL_BLEND_DST_RGB, &bdst);
        glGetIntegerv(GL_BLEND_SRC_ALPHA, &bsrca);
        glGetIntegerv(GL_BLEND_DST_ALPHA, &bdsta);
        GLboolean dmask = GL_TRUE;
        glGetBooleanv(GL_DEPTH_WRITEMASK, &dmask);
        glDepthMask(GL_FALSE);
        glActiveTexture(GL_TEXTURE1);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &t1);
        glActiveTexture(GL_TEXTURE0);
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &t0);
        static const bool trace = std::getenv("RAC1_SUN_RAYS_TRACE") != nullptr;  // (testing: the pass's GPU time)
        static GLuint q = 0;
        static double acc = 0.0;
        static int nq = 0;
        if (trace) {
            if (q == 0) glGenQueries(1, &q);
            glBeginQuery(GL_TIME_ELAPSED, q);
        }
        sun_rays_pass(fb, vp);
        if (trace) {
            glEndQuery(GL_TIME_ELAPSED);
            GLuint64 ns = 0;
            glGetQueryObjectui64v(q, GL_QUERY_RESULT, &ns);
            acc += static_cast<double>(ns) * 1e-6;
            if (++nq == 60) {
                float px[4] = {};  // (how open the sky around the sun is: the blur's texel (0, 0), read after the timing)
                if (g_rays.rfbo != 0) {
                    GLint rfb = 0;
                    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, g_rays.rfbo);
                    glReadPixels(0, 0, 1, 1, GL_RGBA, GL_FLOAT, px);
                    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
                }
                std::printf("[rays] GPU %.3f ms a frame (%dx%d), open sky %.2f\n", acc / nq, vp[2], vp[3], px[3]);
                acc = 0.0;
                nq = 0;
            }
        }
        glActiveTexture(GL_TEXTURE1);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(t1));
        glActiveTexture(GL_TEXTURE0);
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(t0));
        glDepthMask(dmask);
        glBlendFuncSeparate(static_cast<GLenum>(bsrc), static_cast<GLenum>(bdst), static_cast<GLenum>(bsrca), static_cast<GLenum>(bdsta));
    }

    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
    mrp_lighting_material_capture(vp);  // (after the occlusion and the rays: the picture the reflections show)
    glColorMask(cmask[0], cmask[1], cmask[2], cmask[3]);
    if (blend) glEnable(GL_BLEND);
    else glDisable(GL_BLEND);
    if (depth) glEnable(GL_DEPTH_TEST);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (cull) glEnable(GL_CULL_FACE);
    glActiveTexture(static_cast<GLenum>(prev_active));
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(fb));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    glUseProgram(static_cast<GLuint>(prev_prog));
    glBindVertexArray(static_cast<GLuint>(prev_vao));
}

// ---- (step 3) the lights placed by hand ------------------------------------------------------------------------------

namespace {
std::vector<UserLight> g_user;
std::vector<int> g_user_noauto;  // the tie classes whose automatic emitters are off
std::vector<ClassLight> g_class_lights;  // the lights per object class ("emit")
std::vector<EmissiveSurface> g_emissive_list;  // the emissive surfaces and the materials ("emissive", "material")
bool g_rain = false;                           // ("rain on": the puddles' and the water's rain ripples)
bool g_sky_file = false;                       // ("sky r g b": the reflections' sky colour; else kSkyDefault)
float g_sky[3] = {0.0f, 0.0f, 0.0f};
std::vector<ParticleSpec> g_particles, g_particles_read;  // the ambient particles ("particles"): now, and as the file was read
// the file's lines in their order, as the editor's Save writes them back: a comment (or a line not understood) by its text, an
// entry by its kind ('s' sun, 'r' sun_rays, 'S' interior_set, 'e' emit, 'm' emissive / material, 'R' rain, 'k' sky, 'i' interior, 'o' noauto,
// 'l' light, 'p' particles) - the next of
// that kind's current list written in its place, the ones added since after the last line
struct FileLine {
    char kind = '#';
    std::string text;
};
std::vector<FileLine> g_user_layout;
int g_user_level = -1;
std::string g_user_path;
// orig: (host) the level's lights file
std::string user_path(int level) {
    const char* dir = std::getenv("RAC1_LIGHTS_DIR");
    char name[32];
    std::snprintf(name, sizeof name, "level%02d.txt", level);
    return std::string(dir != nullptr ? dir : "data/lights") + "/" + name;
}
constexpr const char* kParticleKindNames[kPartKinds] = {"dust", "ash", "embers", "fireflies", "motes", "rain", "snow"};
// orig: (host) a "particles <kind> <density> [r g b] [wind <x> <y>] [gust <g>] [box x0 y0 z0 x1 y1 z1] [source lava] [rise <r>] [keep]" line
// read (false: not one)
bool particles_parse(const char* line, ParticleSpec& p) {
    char kind[16] = {};
    int at = 0;
    if (std::sscanf(line, "particles %15s %f%n", kind, &p.density, &at) != 2) return false;
    p.kind = -1;
    for (int k = 0; k < kPartKinds; ++k)
        if (std::strcmp(kind, kParticleKindNames[k]) == 0) p.kind = k;
    if (p.kind < 0) return false;
    p.density = std::clamp(p.density, 0.0f, 4.0f);
    const char* q = line + at;
    int n = 0;
    float c[3];
    if (std::sscanf(q, " %f %f %f%n", &c[0], &c[1], &c[2], &n) == 3) {
        p.has_col = true;
        for (int i = 0; i < 3; ++i) p.col[i] = c[i];
        q += n;
    }
    char word[16];
    while (std::sscanf(q, " %15s%n", word, &n) == 1) {
        q += n;
        if (std::strcmp(word, "wind") == 0 && std::sscanf(q, " %f %f%n", &p.wind[0], &p.wind[1], &n) == 2) {
            q += n;
        } else if (std::strcmp(word, "box") == 0 && std::sscanf(q, " %f %f %f %f %f %f%n", &p.lo[0], &p.lo[1], &p.lo[2], &p.hi[0], &p.hi[1], &p.hi[2], &n) == 6) {
            q += n;
            p.has_box = true;
            for (int i = 0; i < 3; ++i)
                if (p.lo[i] > p.hi[i]) std::swap(p.lo[i], p.hi[i]);
        } else if (std::strcmp(word, "gust") == 0 && std::sscanf(q, " %f%n", &p.gust, &n) == 1) {
            q += n;
            p.gust = std::clamp(p.gust, 0.0f, 4.0f);
        } else if (std::strcmp(word, "source") == 0 && std::sscanf(q, " %15s%n", word, &n) == 1) {
            q += n;
            p.src_lava = std::strcmp(word, "lava") == 0;
        } else if (std::strcmp(word, "rise") == 0 && std::sscanf(q, " %f%n", &p.rise, &n) == 1) {
            q += n;
            p.rise = std::clamp(p.rise, 0.0f, 10.0f);
        } else if (std::strcmp(word, "keep") == 0) {
            p.keep = true;
        } else if (word[0] == '#') {
            break;  // (a comment after the line's values)
        }
    }
    return true;
}
// orig: (host) a number as short as it reads back the same (%.3f's, its trailing zeros dropped)
std::string particles_num(float v) {
    char b[32];
    std::snprintf(b, sizeof b, "%.3f", v);
    std::string s(b);
    while (!s.empty() && s.back() == '0') s.pop_back();
    if (!s.empty() && s.back() == '.') s.pop_back();
    if (s == "-0") s = "0";
    return s;
}
// orig: (host) a particles line written from its values
std::string particles_line(const ParticleSpec& p) {
    std::string s = std::string("particles ") + kParticleKindNames[std::clamp(p.kind, 0, kPartKinds - 1)] + " " + particles_num(p.density);
    if (p.has_col) s += "  " + particles_num(p.col[0]) + " " + particles_num(p.col[1]) + " " + particles_num(p.col[2]);
    if (p.wind[0] != 0.0f || p.wind[1] != 0.0f) s += "  wind " + particles_num(p.wind[0]) + " " + particles_num(p.wind[1]);
    if (p.gust != 0.0f) s += "  gust " + particles_num(p.gust);
    if (p.has_box) {
        s += "  box";
        for (float v : {p.lo[0], p.lo[1], p.lo[2], p.hi[0], p.hi[1], p.hi[2]}) s += " " + particles_num(v);
    }
    if (p.src_lava) s += "  source lava";
    if (p.rise != 0.0f) s += "  rise " + particles_num(p.rise);
    if (p.keep) s += "  keep";
    return s;
}

// orig: (host) an "emit <tie|shrub|moby> <class> <ox oy oz> <r g b> <intensity> <radius> [flicker] [down] [joint <n>]" line read (false:
// not one; "joint" a moby's only)
bool emit_parse(const char* line, ClassLight& cl) {
    char kind[8] = {};
    int at = 0;
    const int got = std::sscanf(line, "emit %7s %d %f %f %f %f %f %f %f %f%n", kind, &cl.o_class, &cl.off[0], &cl.off[1], &cl.off[2], &cl.col[0], &cl.col[1],
                                &cl.col[2], &cl.intensity, &cl.radius, &at);
    if (got < 10 || !(std::strcmp(kind, "tie") == 0 || std::strcmp(kind, "moby") == 0 || std::strcmp(kind, "shrub") == 0)) return false;
    cl.moby = std::strcmp(kind, "moby") == 0;
    cl.shrub = std::strcmp(kind, "shrub") == 0;
    const char* q = line + at;
    int n = 0;
    if (std::sscanf(q, " %f%n", &cl.flicker, &n) == 1) {
        q += n;
        if (std::sscanf(q, " %f%n", &cl.down, &n) == 1) q += n;
    }
    cl.down = std::clamp(cl.down, 0.0f, 1.0f);
    if (int j = -1; cl.moby && std::sscanf(q, " joint %d", &j) == 1) cl.joint = std::max(j, -1);
    return true;
}
// orig: (host) an "emit" line written (the numbers as Save always wrote them)
std::string emit_line(const ClassLight& cl) {
    char b[256];
    std::snprintf(b, sizeof b, "emit %s %d  %.3f %.3f %.3f  %.3f %.3f %.3f  %.3f %.3f %.3f  %.2f", cl.moby ? "moby" : cl.shrub ? "shrub" : "tie", cl.o_class, cl.off[0],
                  cl.off[1], cl.off[2], cl.col[0], cl.col[1], cl.col[2], cl.intensity, cl.radius, cl.flicker, cl.down);
    std::string out(b);
    if (cl.moby && cl.joint >= 0) out += "  joint " + std::to_string(cl.joint);
    return out;
}
// orig: (host) an "emissive <tfrag|tie|shrub|moby|surface> <class> tex <n> [strength] [threshold]" or "material <kind> <class> tex <n>
// <type> [a] [b] [c] [glow <strength> [threshold]]" line read (false: neither)
bool emis_parse(const char* line, EmissiveSurface& out) {
    {
        EmissiveSurface es;
        char kind[8] = {};
        const int got = std::sscanf(line, "emissive %7s %d tex %d %f %f", kind, &es.o_class, &es.tex, &es.strength, &es.threshold);
        int k = -1;
        for (int i = 0; i < 5; ++i)
            if (std::strcmp(kind, mrp_emissive_kind_name(i)) == 0) k = i;
        if (got >= 3 && k >= 0 && es.tex >= 0) {
            es.kind = k;
            es.strength = std::clamp(es.strength, 0.0f, 4.0f);
            es.threshold = std::clamp(es.threshold, 0.0f, 1.0f);
            out = es;
            return true;
        }
    }
    EmissiveSurface es;
    char kind[8] = {}, type[16] = {};
    float a[3] = {0.0f, 0.0f, 0.0f};
    int at = 0;
    const int got = std::sscanf(line, "material %7s %d tex %d %15s%n %f %f %f", kind, &es.o_class, &es.tex, type, &at, &a[0], &a[1], &a[2]);
    int k = -1;
    for (int i = 0; i < 5; ++i)
        if (std::strcmp(kind, mrp_emissive_kind_name(i)) == 0) k = i;
    const int ty = got >= 4 ? mrp_material_type(type) : -1;
    if (k < 0 || es.tex < 0 || ty < 0) return false;
    es.kind = k;
    es.type = ty;
    const MatParam* mp = mrp_material_params(ty);
    for (int i = 0; i < 3; ++i) es.p[i] = mp[i].name == nullptr ? 0.0f : std::clamp(got >= 5 + i ? a[i] : mp[i].def, mp[i].lo, mp[i].hi);
    if (ty == MAT_EMISSIVE) {  // ("material ... emissive [strength] [threshold]": the emissive line's numbers)
        es.strength = es.p[0];
        es.threshold = es.p[1];
    } else if (const char* g = at > 0 ? std::strstr(line + at, " glow ") : nullptr; g != nullptr && ty != MAT_LAVA) {  // (its glow: lava glows by itself)
        float gs = 0.0f, gt = 0.0f;
        const int n = std::sscanf(g, " glow %f %f", &gs, &gt);
        if (n >= 1) {
            es.glow = std::clamp(gs, 0.0f, 4.0f);
            es.glow_threshold = n >= 2 ? std::clamp(gt, 0.0f, 1.0f) : 0.0f;
        }
    }
    out = es;
    return true;
}
// orig: (host) an emissive line and a material line for the same texture (a list read from one file): the emissive's strength and
// threshold become the material's glow, the emissive entry dropped (before batch 1 of #102 the later line's slot won and the other
// was not drawn: "a metal can't also be emissive"); `drop` says which entries go
std::vector<bool> emis_fold(std::vector<EmissiveSurface*> L) {
    std::vector<bool> drop(L.size(), false);
    for (size_t i = 0; i < L.size(); ++i) {
        const EmissiveSurface& e = *L[i];
        if (e.type != MAT_EMISSIVE) continue;
        for (size_t j = 0; j < L.size(); ++j) {
            EmissiveSurface& m = *L[j];
            if (j == i || drop[j] || m.type == MAT_EMISSIVE || m.type == MAT_LAVA || m.kind != e.kind || m.o_class != e.o_class || m.tex != e.tex) continue;
            if (m.glow <= 0.0f) {  // (a glow written on the material line itself wins)
                m.glow = e.strength;
                m.glow_threshold = e.threshold;
            }
            drop[i] = true;
            break;
        }
    }
    return drop;
}
// orig: (host) an "emissive" / "material" line written
std::string emis_line(const EmissiveSurface& es) {
    char b[256];
    if (es.type == MAT_EMISSIVE) {
        std::snprintf(b, sizeof b, "emissive %s %d tex %d  %.2f %.2f", mrp_emissive_kind_name(es.kind), es.o_class, es.tex, es.strength, es.threshold);
        return b;
    }
    std::snprintf(b, sizeof b, "material %s %d tex %d %s", mrp_emissive_kind_name(es.kind), es.o_class, es.tex, mrp_material_name(es.type));
    std::string out(b);
    const MatParam* mp = mrp_material_params(es.type);
    for (int i = 0; i < 3; ++i)
        if (mp[i].name != nullptr) {
            std::snprintf(b, sizeof b, " %g", static_cast<double>(es.p[i]));
            out += b;
        }
    if (es.glow > 0.0f && es.type != MAT_LAVA) {
        std::snprintf(b, sizeof b, "  glow %.2f %.2f", static_cast<double>(es.glow), static_cast<double>(es.glow_threshold));
        out += b;
    }
    return out;
}
bool same_emit(const ClassLight& a, const ClassLight& b) {
    return a.moby == b.moby && a.shrub == b.shrub && a.o_class == b.o_class && std::memcmp(a.off, b.off, sizeof a.off) == 0 && std::memcmp(a.col, b.col, sizeof a.col) == 0 &&
           a.intensity == b.intensity && a.radius == b.radius && a.flicker == b.flicker && a.down == b.down && a.on == b.on && a.joint == b.joint;
}
bool same_emis(const EmissiveSurface& a, const EmissiveSurface& b) {
    return a.kind == b.kind && a.o_class == b.o_class && a.tex == b.tex && a.strength == b.strength && a.threshold == b.threshold && a.type == b.type &&
           std::memcmp(a.p, b.p, sizeof a.p) == 0 && a.glow == b.glow && a.glow_threshold == b.glow_threshold;
}

// ---- the lines for every level: data/lights/classes.txt (moby classes: the same on every level) ----
struct GlobalLine {
    char kind = '#';    // '#' a comment / a line kept as it is, 'e' an emit line, 'm' an emissive / material line
    std::string text;   // (as read: written back while the entry is not edited)
    ClassLight cl;      // ('e')
    EmissiveSurface es; // ('m')
    bool shown = false; // (added to the current level's lists: not overridden by its file)
    bool gone = false;  // (deleted / moved to the level's file since: not written)
};
std::vector<GlobalLine> g_global;
std::string g_global_path;
// orig: (host) the file's path
std::string classes_path() {
    const char* dir = std::getenv("RAC1_LIGHTS_DIR");
    return std::string(dir != nullptr ? dir : "data/lights") + "/classes.txt";
}
// orig: (host) the file read (its moby lines; anything else kept as a comment)
void classes_load() {
    g_global.clear();
    g_global_path = classes_path();
    FILE* f = std::fopen(g_global_path.c_str(), "r");
    if (f == nullptr) return;
    const rac1::ps2fpu::NearestScope nearest;
    char line[256];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        GlobalLine g;
        g.text = line;
        while (!g.text.empty() && (g.text.back() == '\n' || g.text.back() == '\r')) g.text.pop_back();
        if (line[0] != '#') {
            if (emit_parse(line, g.cl) && g.cl.moby) g.kind = 'e';
            else if (emis_parse(line, g.es) && g.es.kind == 3) g.kind = 'm';
            else if (g.text.find_first_not_of(" \t") != std::string::npos)
                std::printf("[lights] %s: a line not used (only moby emit / emissive / material lines are), kept as it is: %s\n", g_global_path.c_str(), g.text.c_str());
        }
        g.cl.global = g.es.global = g.kind != '#';
        g_global.push_back(g);
    }
    std::fclose(f);
    std::vector<EmissiveSurface*> L;  // (an emissive and a material line for the same class and texture: one entry, the glowing material)
    std::vector<size_t> at;
    for (size_t i = 0; i < g_global.size(); ++i)
        if (g_global[i].kind == 'm') {
            L.push_back(&g_global[i].es);
            at.push_back(i);
        }
    const std::vector<bool> drop = emis_fold(L);
    for (size_t k = 0; k < drop.size(); ++k)
        if (drop[k]) g_global[at[k]].kind = '#';  // (its line kept as it is, not used: read again, folded again)
}
// orig: (host) the file's lines added to the level's lists (after its own), but where the level's file has its own line for that class
// (emit) or class and texture (emissive, material)
void classes_apply() {
    size_t ne = 0, nm = 0;
    for (size_t i = 0; i < g_global.size(); ++i) {
        GlobalLine& g = g_global[i];
        g.shown = false;
        if (g.kind == 'e') {
            const bool own = std::any_of(g_class_lights.begin(), g_class_lights.end(), [&](const ClassLight& c) { return !c.global && c.moby && c.o_class == g.cl.o_class; });
            if (own) continue;
            ClassLight c = g.cl;
            c.global = true;
            c.gidx = static_cast<int>(i);
            g_class_lights.push_back(c);
            g.shown = true;
            ++ne;
        } else if (g.kind == 'm') {
            const bool own = std::any_of(g_emissive_list.begin(), g_emissive_list.end(),
                                         [&](const EmissiveSurface& e) { return !e.global && e.kind == 3 && e.o_class == g.es.o_class && e.tex == g.es.tex; });
            if (own) continue;
            EmissiveSurface e = g.es;
            e.global = true;
            e.gidx = static_cast<int>(i);
            g_emissive_list.push_back(e);
            g.shown = true;
            ++nm;
        }
    }
    if (ne + nm > 0) std::printf("[lights] %s: %zu emit and %zu emissive / material lines for every level\n", g_global_path.c_str(), ne, nm);
}
// orig: (host) the file written when its lines changed: the level's global entries (edited, deleted, made global or made the level's
// own) folded into the file's list, each line in its place, a line not edited as it was read; true when written or nothing to write
bool classes_save() {
    std::vector<int> found(g_global.size(), -1);  // (the global line's entry in the level's lists)
    bool changed = false;
    for (size_t i = 0; i < g_class_lights.size(); ++i) {
        ClassLight& c = g_class_lights[i];
        if (c.gidx >= 0 && static_cast<size_t>(c.gidx) < g_global.size()) {
            GlobalLine& g = g_global[static_cast<size_t>(c.gidx)];
            if (!c.global || !c.on) {  // (made the level's own, or turned off: out of the file)
                if (!g.gone) changed = true;
                g.gone = true;
                if (!c.global) c.gidx = -1;
                continue;
            }
            found[static_cast<size_t>(c.gidx)] = 1;
            if (!same_emit(c, g.cl)) {
                ClassLight k = c;
                g.cl = k;
                g.text = emit_line(k);
                changed = true;
            }
        } else if (c.global && c.moby && c.on) {
            GlobalLine g;
            g.kind = 'e';
            g.cl = c;
            g.text = emit_line(c);
            g.shown = true;
            c.gidx = static_cast<int>(g_global.size());
            g_global.push_back(g);
            found.push_back(1);
            changed = true;
        }
    }
    for (size_t i = 0; i < g_emissive_list.size(); ++i) {
        EmissiveSurface& e = g_emissive_list[i];
        if (e.gidx >= 0 && static_cast<size_t>(e.gidx) < g_global.size()) {
            GlobalLine& g = g_global[static_cast<size_t>(e.gidx)];
            if (!e.global) {
                if (!g.gone) changed = true;
                g.gone = true;
                e.gidx = -1;
                continue;
            }
            found[static_cast<size_t>(e.gidx)] = 1;
            if (!same_emis(e, g.es)) {
                g.es = e;
                g.text = emis_line(e);
                changed = true;
            }
        } else if (e.global && e.kind == 3) {
            GlobalLine g;
            g.kind = 'm';
            g.es = e;
            g.text = emis_line(e);
            g.shown = true;
            e.gidx = static_cast<int>(g_global.size());
            g_global.push_back(g);
            found.push_back(1);
            changed = true;
        }
    }
    for (size_t i = 0; i < g_global.size(); ++i)  // (shown on this level and no longer in its lists: deleted)
        if (g_global[i].kind != '#' && g_global[i].shown && found[i] < 0 && !g_global[i].gone) {
            g_global[i].gone = true;
            changed = true;
        }
    if (!changed) return true;
    std::error_code ec;
    const std::filesystem::path p(g_global_path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    FILE* f = std::fopen(g_global_path.c_str(), "w");
    if (f == nullptr) return false;
    const rac1::ps2fpu::NearestScope nearest;
    if (g_global.empty() || g_global.front().kind != '#')
        std::fprintf(f, "# rac1-native: moby lights, emissive surfaces and materials for every level (the debug menu's \"for every level\")\n");
    for (const GlobalLine& g : g_global)
        if (!g.gone) std::fprintf(f, "%s\n", g.text.c_str());
    std::fclose(f);
    std::printf("[lights] %s written\n", g_global_path.c_str());
    return true;
}

// orig: (host) read: one light a line, "light x y z r g b intensity radius flicker on" ('#' comments)
void user_load(int level) {
    g_user.clear();
    g_user_noauto.clear();
    g_sun_file = SunFile{};
    g_interiors.clear();
    g_interior_sets.clear();
    g_class_lights.clear();
    g_emissive_list.clear();
    g_rain = false;
    g_sky_file = false;
    g_particles.clear();
    g_particles_read.clear();
    g_sun_snap = true;  // (a level's sun, or the file read again: at once)
    g_user_layout.clear();
    g_sun_rays_off = false;
    g_user_level = level;
    g_user_path = user_path(level);
    g_user_light_selected = -1;
    FILE* f = std::fopen(g_user_path.c_str(), "r");
    classes_load();
    if (f == nullptr) {
        classes_apply();
        mrp_user_emissive_changed();
        return;
    }
    const rac1::ps2fpu::NearestScope nearest;  // (the numbers read as written: this thread runs toward zero)
    char line[256];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        UserLight l;
        int on = 1, cls = 0;
        std::string text(line);
        while (!text.empty() && (text.back() == '\n' || text.back() == '\r')) text.pop_back();
        if (std::strncmp(line, "# rac1-native: lights placed by hand", 36) == 0 || std::strncmp(line, "# light x y z", 13) == 0) continue;  // (the header: Save's own)
        auto entry = [&](char kind) { g_user_layout.push_back(FileLine{kind, {}}); };
        if (line[0] == '#') {
            g_user_layout.push_back(FileLine{'#', text});
            continue;
        }
        if (char w[8] = {}; std::sscanf(line, "sun_rays %7s", w) == 1) {  // "sun_rays off": no Sun Rays on this level
            g_sun_rays_off = std::strcmp(w, "off") == 0 || std::strcmp(w, "0") == 0;
            entry('r');
            continue;
        }
        if (std::sscanf(line, "noauto %d", &cls) == 1) {
            g_user_noauto.push_back(cls);
            entry('o');
            continue;
        }
        if (ParticleSpec ps; particles_parse(line, ps)) {
            ps.text = text;
            g_particles.push_back(ps);
            g_particles_read.push_back(ps);
            entry('p');
            continue;
        }
        if (SunInterior b; std::sscanf(line, "interior %f %f %f %f %f %f", &b.lo[0], &b.lo[1], &b.lo[2], &b.hi[0], &b.hi[1], &b.hi[2]) == 6) {
            for (int i = 0; i < 3; ++i)
                if (b.lo[i] > b.hi[i]) std::swap(b.lo[i], b.hi[i]);
            g_interiors.push_back(b);
            entry('i');
            continue;
        }
        if (ClassLight cl; emit_parse(line, cl)) {  // "emit <tie|shrub|moby> <class> <ox oy oz> <r g b> <intensity> <radius> [flicker] [down] [joint n]"
            g_class_lights.push_back(cl);
            entry('e');
            continue;
        }
        if (EmissiveSurface es; emis_parse(line, es)) {  // "emissive ..." / "material ..."
            g_emissive_list.push_back(es);
            entry('m');
            continue;
        }
        if (char w[8] = {}; std::sscanf(line, "rain %7s", w) == 1) {  // "rain on": the materials' rain ripples
            g_rain = std::strcmp(w, "on") == 0 || std::strcmp(w, "1") == 0;
            entry('R');
            continue;
        }
        if (float r = 0, g = 0, b = 0; std::sscanf(line, "sky %f %f %f", &r, &g, &b) == 3) {  // the reflections' sky colour
            g_sky_file = true;
            g_sky[0] = std::clamp(r, 0.0f, 2.0f);
            g_sky[1] = std::clamp(g, 0.0f, 2.0f);
            g_sky[2] = std::clamp(b, 0.0f, 2.0f);
            entry('k');
            continue;
        }
        if (int n = 0; std::sscanf(line, "interior_set %d", &n) == 1) {
            g_interior_sets.push_back(n);
            entry('S');
            continue;
        }
        if (float az = 0, el = 0; std::sscanf(line, "sun %f %f", &az, &el) == 2) {
            if (!g_sun_file.on) entry('s');  // (a second sun line: the last one wins, written once)
            g_sun_file.on = true;
            g_sun_file.az = az;
            g_sun_file.el = std::clamp(el, 5.0f, 90.0f);
            continue;
        }
        if (std::sscanf(line, "light %f %f %f %f %f %f %f %f %f %d", &l.pos[0], &l.pos[1], &l.pos[2], &l.col[0], &l.col[1], &l.col[2], &l.intensity, &l.radius,
                        &l.flicker, &on) >= 9) {
            l.on = on != 0;
            g_user.push_back(l);
            entry('l');
            continue;
        }
        if (text.find_first_not_of(" \t") != std::string::npos)  // (a line not understood: said, so a typo does not go unseen)
            std::printf("[lights] %s: a line not understood, kept as it is: %s\n", g_user_path.c_str(), text.c_str());
        g_user_layout.push_back(FileLine{'#', text});  // (a blank line, or one not understood: kept as it is)
    }
    std::fclose(f);
    {  // (an emissive and a material line for the same texture: one entry, the glowing material - Save writes it as one line)
        std::vector<EmissiveSurface*> L;
        for (EmissiveSurface& e : g_emissive_list) L.push_back(&e);
        const std::vector<bool> drop = emis_fold(L);
        std::vector<EmissiveSurface> kept;
        for (size_t i = 0; i < g_emissive_list.size(); ++i)
            if (!drop[i]) kept.push_back(g_emissive_list[i]);
        if (kept.size() != g_emissive_list.size())
            std::printf("[lights] level %d: %zu emissive lines folded into the material of the same texture (its glow)\n", level, g_emissive_list.size() - kept.size());
        g_emissive_list.swap(kept);
    }
    std::printf("[lights] level %d: %zu lights placed by hand (%s)\n", level, g_user.size(), g_user_path.c_str());
    if (!g_emissive_list.empty()) {
        const size_t nm = static_cast<size_t>(std::count_if(g_emissive_list.begin(), g_emissive_list.end(), [](const EmissiveSurface& e) { return e.type != MAT_EMISSIVE; }));
        std::printf("[lights] level %d: %zu emissive surfaces, %zu materials%s\n", level, g_emissive_list.size() - nm, nm, g_rain ? " (rain)" : "");
    }
    if (g_sun_file.on) std::printf("[lights] level %d: the sun turned to azimuth %.1f, elevation %.1f degrees\n", level, g_sun_file.az, g_sun_file.el);
    classes_apply();
    mrp_user_emissive_changed();
}
// orig: (host) the current level's file read when the level changed (its sun is wanted at the shadow pass's start, whatever the
// secondary lights' setting)
void user_ensure() {
    const int level = static_cast<int>(draw_lw(0x0015ed84));
    if (level != g_user_level) user_load(level);
}
}  // namespace

// orig: (host) see mrp_lighting.h
bool mrp_user_sun(float* az, float* el) {
    *az = g_sun_file.az;
    *el = g_sun_file.el;
    return g_sun_file.on;
}
// orig: (host) see mrp_lighting.h
void mrp_user_sun_set(bool on, float az, float el) {
    g_sun_snap = true;
    g_sun_file.on = on;
    g_sun_file.az = az;
    g_sun_file.el = std::clamp(el, 5.0f, 90.0f);
}
// orig: (host) see mrp_lighting.h
bool mrp_user_sun_rays_off() { return g_sun_rays_off; }
// orig: (host) see mrp_lighting.h
void mrp_user_sun_rays_set_off(bool off) { g_sun_rays_off = off; }
// orig: (host) see mrp_lighting.h
const char* mrp_particle_kind_name(int kind) { return kind >= 0 && kind < kPartKinds ? kParticleKindNames[kind] : "?"; }
// orig: (host) see mrp_lighting.h
bool ParticleSpec::same_as(const ParticleSpec& o) const {
    auto eq3 = [](const float* a, const float* b) { return a[0] == b[0] && a[1] == b[1] && a[2] == b[2]; };
    return kind == o.kind && density == o.density && has_col == o.has_col && (!has_col || eq3(col, o.col)) && wind[0] == o.wind[0] && wind[1] == o.wind[1] &&
           gust == o.gust && src_lava == o.src_lava && rise == o.rise && has_box == o.has_box && (!has_box || (eq3(lo, o.lo) && eq3(hi, o.hi))) && keep == o.keep;
}
// orig: (host) see mrp_lighting.h
std::vector<ParticleSpec>& mrp_user_particles() { return g_particles; }
// orig: (host) see mrp_lighting.h
const std::vector<ParticleSpec>& mrp_user_particles_read() { return g_particles_read; }
// orig: (host) see mrp_lighting.h
void mrp_user_lights_ensure() { user_ensure(); }
// orig: (host) see mrp_lighting.h
void mrp_lighting_zone_light(float rgb[3], float sun[3]) {
    float w = 0.0f;
    glm::vec4 key(0.0f);
    glm::vec3 key_dir(0.0f, 0.0f, 1.0f);
    const glm::vec3 s = sun_direction(&w, &key, &key_dir);
    for (int i = 0; i < 3; ++i) sun[i] = s[i];
    // the hero's light sets (as game_sun picks them), both lights' colours summed, blended by the hero's weight
    uint32_t a = 0, b = 0;
    float t = 0.0f;
    if (const uint32_t hero = draw_lwu(OV(0x0015ffd8u)); hero != 0) {
        a = draw_lbu(hero + 0x38);
        b = draw_lbu(hero + 0x39);
        t = static_cast<float>(draw_lbu(hero + 0x3a)) / 256.0f;
        if (a > 15) a = 0;
        if (b > 15) b = a;
        if (a == 15) a = b = 0;
    }
    auto set_sum = [](uint32_t set) {
        const uint32_t base = OV(0x0017fec0u) + 0x40 * set;
        glm::vec3 c(0.0f);
        for (uint32_t l = 0; l < 2; ++l)
            for (uint32_t i = 0; i < 3; ++i) c[i] += std::max(draw_lwc1(base + 0x20 * l + 4 * i), 0.0f);
        return c;
    };
    const glm::vec3 c = glm::mix(set_sum(a), set_sum(b), t);
    for (int i = 0; i < 3; ++i) rgb[i] = std::isfinite(c[i]) ? std::min(c[i], 4.0f) : 0.0f;
}
// orig: (host) see mrp_lighting.h
std::vector<ClassLight>& mrp_user_class_lights() { return g_class_lights; }

namespace {
// the renderers' copy of the emissive surfaces (they read it on the scene's worker), its generation, the preview
std::mutex g_emis_m;
std::vector<EmissiveSurface> g_emis_copy;
EmissiveSurface g_emis_preview;
bool g_emis_preview_on = false;
double g_emis_preview_t = 0.0;  // (when the debug menu last asked for it: a preview not asked for half a second ends - the menu closed)
double emis_now() { return std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count(); }
std::atomic<uint32_t> g_emis_gen{1};
}  // namespace
// orig: (host) see mrp_lighting.h
std::vector<EmissiveSurface>& mrp_user_emissive() { return g_emissive_list; }
// orig: (host) see mrp_lighting.h
const char* mrp_emissive_kind_name(int kind) {
    static const char* names[5] = {"tfrag", "tie", "shrub", "moby", "surface"};
    return kind >= 0 && kind < 5 ? names[kind] : "?";
}
namespace {
constexpr const char* kMatNames[MAT_COUNT] = {"emissive", "metal", "wet", "puddle", "lava", "water", "mud", "quicksand"};
// per type its parameters a, b, c: name, default, range (lava's c is the emissive threshold: the texels that glow)
constexpr MatParam kMatParams[MAT_COUNT][3] = {
    {{"strength", 1.0f, 0.0f, 4.0f}, {"threshold", 0.0f, 0.0f, 1.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
    {{"roughness", 0.35f, 0.03f, 1.0f}, {"strength", 1.0f, 0.0f, 2.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
    {{"wetness", 0.6f, 0.0f, 1.0f}, {"roughness", 0.45f, 0.05f, 1.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
    {{"coverage", 0.4f, 0.0f, 1.0f}, {"scale", 0.3f, 0.02f, 4.0f}, {"wetness", 0.6f, 0.0f, 1.0f}},
    {{"strength", 1.3f, 0.0f, 4.0f}, {"pulse", 0.15f, 0.0f, 1.0f}, {"threshold", 0.0f, 0.0f, 1.0f}},
    {{"strength", 1.0f, 0.0f, 2.0f}, {"ripples", 1.0f, 0.0f, 3.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
    {{"wetness", 0.5f, 0.0f, 1.0f}, {"drift", 1.0f, 0.0f, 3.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
    {{"drift", 1.0f, 0.0f, 3.0f}, {"gloss", 0.3f, 0.0f, 1.0f}, {nullptr, 0.0f, 0.0f, 0.0f}},
};
}  // namespace
// orig: (host) see mrp_lighting.h
const char* mrp_material_name(int type) { return type >= 0 && type < MAT_COUNT ? kMatNames[type] : "?"; }
// orig: (host) see mrp_lighting.h
int mrp_material_type(const char* name) {
    for (int i = 0; i < MAT_COUNT; ++i)
        if (std::strcmp(name, kMatNames[i]) == 0) return i;
    return -1;
}
// orig: (host) see mrp_lighting.h
const MatParam* mrp_material_params(int type) { return kMatParams[std::clamp(type, 0, MAT_COUNT - 1)]; }
// orig: (host) see mrp_lighting.h
bool mrp_user_rain() { return g_rain; }
// orig: (host) see mrp_lighting.h
void mrp_user_rain_set(bool on) { g_rain = on; }
// orig: (host) see mrp_lighting.h
void mrp_user_emissive_changed() {
    std::lock_guard<std::mutex> lg(g_emis_m);
    g_emis_copy.assign(g_emissive_list.begin(), g_emissive_list.begin() + std::min<ptrdiff_t>(static_cast<ptrdiff_t>(g_emissive_list.size()), kEmissiveSlots));
    if (g_emissive_list.size() > static_cast<size_t>(kEmissiveSlots))
        std::printf("[lights] level %d: %zu emissive surfaces, the first %d used\n", g_user_level, g_emissive_list.size(), kEmissiveSlots);
    ++g_emis_gen;
}
// orig: (host) see mrp_lighting.h
void mrp_emissive_preview(int kind, int o_class, int tex) {
    std::lock_guard<std::mutex> lg(g_emis_m);
    const bool on = kind >= 0;
    g_emis_preview_t = emis_now();
    if (on == g_emis_preview_on && (!on || (g_emis_preview.kind == kind && g_emis_preview.o_class == o_class && g_emis_preview.tex == tex))) return;
    g_emis_preview_on = on;
    g_emis_preview.kind = kind;
    g_emis_preview.o_class = o_class;
    g_emis_preview.tex = tex;
    ++g_emis_gen;
}
// orig: (host) see mrp_lighting.h
uint32_t mrp_emissive_snapshot(std::vector<EmissiveSurface>& out, EmissiveSurface* preview, bool* has_preview) {
    std::lock_guard<std::mutex> lg(g_emis_m);
    if (g_emis_preview_on && emis_now() - g_emis_preview_t > 0.5) {
        g_emis_preview_on = false;
        ++g_emis_gen;
    }
    static int detail = -1;  // (Surface Detail changed: the slots made again)
    const int d = g_surface_detail != 0 ? 1 : 0;
    if (d != detail) {
        detail = d;
        ++g_emis_gen;
    }
    out = g_emis_copy;
    if (d == 0)
        for (EmissiveSurface& e : out)
            if (e.type != MAT_EMISSIVE && !(e.glow > 0.0f && e.type != MAT_LAVA)) e.kind = -1;  // (a material while Surface Detail is Off: no slot, the original's draw - its glow alone if it has one)
    *preview = g_emis_preview;
    *has_preview = g_emis_preview_on;
    return g_emis_gen.load();
}
namespace {
// orig: (host) the shaders' emissive slots (uEmis): [0].x on (the option and the lighting's variant), slot i + 1 the list's surface
// i (strength, threshold; a material's glow and its type in z), slot 127 the preview (w 1, x its pulse)
void emissive_uniforms(glm::vec4 out[kEmissivePreviewSlot + 1]) {
    for (int i = 0; i <= kEmissivePreviewSlot; ++i) out[i] = glm::vec4(0.0f);
    out[0].x = g_emissive != 0 ? 1.0f : 0.0f;
    out[0].y = static_cast<float>(g_sh.ngame);  // (the mobys' point lights per pixel start after the game's own: sh_points_moby)
    const size_t n = std::min<size_t>(g_emissive_list.size(), kEmissiveSlots);
    for (size_t i = 0; i < n; ++i) {
        const EmissiveSurface& e = g_emissive_list[i];
        out[i + 1] = e.type == MAT_EMISSIVE ? glm::vec4(e.strength, e.threshold, 0.0f, 0.0f)
                                            : glm::vec4(e.type == MAT_LAVA ? 0.0f : e.glow, e.glow_threshold, static_cast<float>(e.type), 0.0f);  // (a material: its type and its glow, uMat the rest)
    }
    const double t = std::chrono::duration<double>(std::chrono::steady_clock::now().time_since_epoch()).count();
    out[kEmissivePreviewSlot] = glm::vec4(0.25f + 0.45f * static_cast<float>(0.5 + 0.5 * std::sin(t * 6.0)), 0.0f, 0.0f, 1.0f);
}
// orig: (host) the materials' uniforms (uMat, mrp_lighting.h): [0] Surface Detail (0 with no material), the clock (seconds, wrapped),
// the rain, the High step's picture ready; slot i + 1 the list's surface i's parameters. The reflections' view, depth and sky
struct MatUniforms {
    glm::vec4 mat[kEmissivePreviewSlot + 1];
    glm::mat4 ssr_vp{1.0f};
    glm::vec4 ssr_z{0.0f}, sky{0.0f};
};
constexpr float kSkyDefault[3] = {0.52f, 0.58f, 0.68f};  // (a level's file without "sky": a light, slightly blue grey)
void material_uniforms(MatUniforms& u) {
    for (int i = 0; i <= kEmissivePreviewSlot; ++i) u.mat[i] = glm::vec4(0.0f);
    const size_t n = std::min<size_t>(g_emissive_list.size(), kEmissiveSlots);
    bool any = false, ssr = false;
    for (size_t i = 0; i < n; ++i) {
        const EmissiveSurface& e = g_emissive_list[i];
        if (e.type == MAT_EMISSIVE) continue;
        any = true;
        ssr = ssr || e.type == MAT_PUDDLE || e.type == MAT_WATER || e.type == MAT_LAVA || (e.type == MAT_METAL && e.p[0] < 0.45f);  // (a polished metal mirrors too: roughness under 0.45, sh_material)
        u.mat[i + 1] = glm::vec4(e.p[0], e.p[1], e.p[2], 0.0f);
    }
    g_mat_ssr_wanted = ssr;
    const bool ready = mat_ssr_ready();
    u.mat[0] = glm::vec4(any ? static_cast<float>(g_surface_detail) : 0.0f, static_cast<float>(g_mat_frames % 60000u) / 60.0f, g_rain ? 1.0f : 0.0f, ready ? 1.0f : 0.0f);
    u.sky = g_sky_file ? glm::vec4(g_sky[0], g_sky[1], g_sky[2], 0.0f) : glm::vec4(kSkyDefault[0], kSkyDefault[1], kSkyDefault[2], 0.0f);
    if (ready) {
        // clip z = x clip w + y along any perspective view: two points of the captured view solve it
        const glm::mat4& V = g_mssr.vp;
        const glm::mat4 I = glm::inverse(V);
        glm::vec4 a = I * glm::vec4(0.0f, 0.0f, -0.5f, 1.0f), b = I * glm::vec4(0.0f, 0.0f, 0.5f, 1.0f);
        a /= a.w;
        b /= b.w;
        const glm::vec4 ca = V * a, cb = V * b;
        const float x = std::fabs(ca.w - cb.w) > 1e-6f ? (ca.z - cb.z) / (ca.w - cb.w) : 1.0f;
        u.ssr_vp = V;
        u.ssr_z = glm::vec4(x, ca.z - x * ca.w, 0.0f, 0.0f);
    }
}
}  // namespace
int g_class_light_selected = -1;
// orig: (host) see mrp_lighting.h
std::vector<SunInterior>& mrp_user_interiors() { return g_interiors; }
// orig: (host) see mrp_lighting.h
std::vector<int>& mrp_user_interior_sets() { return g_interior_sets; }
// orig: (host) see mrp_lighting.h
float mrp_lighting_interior(int* hero_set) {
    *hero_set = g_interior_hero_set;
    return g_interior_k;
}
// orig: (host) see mrp_lighting.h
bool mrp_lighting_sun_turn(float r[9]) {
    if (g_modern_shadows == 0 || !g_sun_known.load()) return false;
    const glm::vec3 a = glm::normalize(g_sh.key_dir), b = glm::normalize(g_sh.sun), ax = glm::cross(a, b);
    const float cs = glm::dot(a, b), sn = glm::length(ax);
    if (sn < 1e-4f && cs > 0.0f) return false;
    glm::mat3 m(1.0f);
    if (sn >= 1e-4f) m = glm::mat3(glm::rotate(glm::mat4(1.0f), std::atan2(sn, cs), ax / sn));
    else m = glm::mat3(glm::rotate(glm::mat4(1.0f), glm::pi<float>(), std::fabs(a.z) < 0.9f ? glm::vec3(0, 0, 1) : glm::vec3(1, 0, 0)));
    std::memcpy(r, glm::value_ptr(m), sizeof(float) * 9);
    return true;
}
// orig: (host) see mrp_lighting.h
void mrp_lighting_game_sun(float* az, float* el) {
    const glm::vec3 d = g_sh.key_dir;
    *az = glm::degrees(std::atan2(d.y, d.x));
    *el = glm::degrees(std::asin(std::clamp(d.z, -1.0f, 1.0f)));
}

// orig: (host) see mrp_lighting.h
std::vector<UserLight>& mrp_user_lights() { return g_user; }
// orig: (host) see mrp_lighting.h
bool mrp_user_lights_class_off(int o_class) { return std::find(g_user_noauto.begin(), g_user_noauto.end(), o_class) != g_user_noauto.end(); }
// orig: (host) see mrp_lighting.h
void mrp_user_lights_class_set_off(int o_class, bool off) {
    const auto it = std::find(g_user_noauto.begin(), g_user_noauto.end(), o_class);
    if (off && it == g_user_noauto.end()) g_user_noauto.push_back(o_class);
    if (!off && it != g_user_noauto.end()) g_user_noauto.erase(it);
}
// orig: (host) see mrp_lighting.h
int mrp_user_lights_level() { return g_user_level; }
// orig: (host) see mrp_lighting.h
const char* mrp_user_lights_path() { return g_user_path.c_str(); }
// orig: (host) see mrp_lighting.h
void mrp_user_lights_reload() {
    if (g_user_level >= 0) user_load(g_user_level);
}
// orig: (host) see mrp_lighting.h
bool mrp_user_lights_save() {
    if (g_user_level < 0) return false;
    std::error_code ec;
    const std::filesystem::path p(g_user_path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    FILE* f = std::fopen(g_user_path.c_str(), "w");
    if (f == nullptr) return false;
    const rac1::ps2fpu::NearestScope nearest;  // (the numbers written as they are: this thread runs toward zero)
    std::fprintf(f, "# rac1-native: lights placed by hand for level %d (the debug menu's Lights tab)\n", g_user_level);
    std::fprintf(f, "# light x y z  r g b  intensity radius flicker on\n");
    // each entry written in the place of the file's line of its kind (the next one of the list), the rest after the last line
    bool sun_done = false, rays_done = false, rain_done = false, sky_done = false;
    const bool has_rays = std::any_of(g_user_layout.begin(), g_user_layout.end(), [](const FileLine& ln) { return ln.kind == 'r'; });
    size_t n_set = 0, n_emit = 0, n_emis = 0, n_box = 0, n_off = 0, n_light = 0, n_part = 0;
    auto put = [&](char kind) {
        switch (kind) {
        case 's':
            if (!sun_done && g_sun_file.on) std::fprintf(f, "sun %.1f %.1f\n", g_sun_file.az, g_sun_file.el);
            sun_done = true;
            if (!has_rays && !rays_done && g_sun_rays_off) std::fprintf(f, "sun_rays off\n");  // (new to the file: under the sun)
            rays_done = rays_done || !has_rays;
            break;
        case 'r':
            if (!rays_done && g_sun_rays_off) std::fprintf(f, "sun_rays off\n");
            rays_done = true;
            break;
        case 'S':
            if (n_set < g_interior_sets.size()) std::fprintf(f, "interior_set %d\n", g_interior_sets[n_set++]);
            break;
        case 'e':
            // (one turned off: not written; one for every level: classes.txt's)
            while (n_emit < g_class_lights.size() && (!g_class_lights[n_emit].on || g_class_lights[n_emit].global)) ++n_emit;
            if (n_emit < g_class_lights.size()) std::fprintf(f, "%s\n", emit_line(g_class_lights[n_emit++]).c_str());
            break;
        case 'm':
            while (n_emis < g_emissive_list.size() && g_emissive_list[n_emis].global) ++n_emis;  // (one for every level: classes.txt's)
            if (n_emis < g_emissive_list.size()) std::fprintf(f, "%s\n", emis_line(g_emissive_list[n_emis++]).c_str());
            break;
        case 'R':
            if (!rain_done && g_rain) std::fprintf(f, "rain on\n");
            rain_done = true;
            break;
        case 'k':
            if (!sky_done && g_sky_file) std::fprintf(f, "sky %.2f %.2f %.2f\n", g_sky[0], g_sky[1], g_sky[2]);
            sky_done = true;
            break;
        case 'i':
            if (n_box < g_interiors.size()) {
                const SunInterior& b = g_interiors[n_box++];
                std::fprintf(f, "interior %.1f %.1f %.1f  %.1f %.1f %.1f\n", b.lo[0], b.lo[1], b.lo[2], b.hi[0], b.hi[1], b.hi[2]);
            }
            break;
        case 'o':
            if (n_off < g_user_noauto.size()) std::fprintf(f, "noauto %d\n", g_user_noauto[n_off++]);
            break;
        case 'p':
            while (n_part < g_particles.size() && !g_particles[n_part].on) ++n_part;  // (one turned off: not written)
            if (n_part < g_particles.size()) {
                ParticleSpec& p = g_particles[n_part];
                // (a line not edited: as it was read, its numbers and spacing kept)
                const bool same = !p.text.empty() && std::any_of(g_particles_read.begin(), g_particles_read.end(), [&](const ParticleSpec& r) { return r.text == p.text && p.same_as(r); });
                if (!same) p.text = particles_line(p);
                std::fprintf(f, "%s\n", p.text.c_str());
                ++n_part;
            }
            break;
        case 'l':
            if (n_light < g_user.size()) {
                const UserLight& l = g_user[n_light++];
                std::fprintf(f, "light %.3f %.3f %.3f  %.3f %.3f %.3f  %.3f %.3f %.3f %d\n", l.pos[0], l.pos[1], l.pos[2], l.col[0], l.col[1], l.col[2], l.intensity,
                             l.radius, l.flicker, l.on ? 1 : 0);
            }
            break;
        default:
            break;
        }
    };
    for (const FileLine& ln : g_user_layout) {
        if (ln.kind == '#') std::fprintf(f, "%s\n", ln.text.c_str());
        else put(ln.kind);
    }
    put('s');
    put('r');
    while (n_set < g_interior_sets.size()) put('S');
    while (n_emit < g_class_lights.size()) put('e');
    put('R');
    put('k');
    while (n_emis < g_emissive_list.size()) put('m');
    while (n_box < g_interiors.size()) put('i');
    while (n_off < g_user_noauto.size()) put('o');
    while (n_light < g_user.size()) put('l');
    while (n_part < g_particles.size()) put('p');
    std::fclose(f);
    g_particles_read.clear();  // (the lines written are the file's now: a later Save keeps them as they are)
    for (const ParticleSpec& p : g_particles)
        if (p.on) g_particles_read.push_back(p);
    return classes_save();  // (the lines for every level: their own file)
}
// orig: (host) see mrp_lighting.h
const char* mrp_user_classes_path() {
    if (g_global_path.empty()) g_global_path = classes_path();
    return g_global_path.c_str();
}

// orig: (host) see mrp_lighting.h
bool mrp_lighting_casters_wanted() { return g_casc_on.load() && (g_shadow_cache == 0 || g_sh.want.load()); }

// orig: (host) see mrp_lighting.h
float mrp_lighting_caster_grow() { return g_shadow_cache != 0 ? kCacheGrow : 1.0f; }

// orig: (host) see mrp_lighting.h
void mrp_lighting_point_casters(float pts[8][4]) {
    for (uint32_t i = 0; i < 8; ++i) {
        pts[i][0] = pts[i][1] = pts[i][2] = pts[i][3] = 0.0f;
        if (g_modern_shadows == 0 || g_modern_points == 0 || g_point_shadows == 0) continue;
        const uint32_t rec = OV(0x001802c0u) + i * 0x20, use = OV(0x001804c0u) + i * 0x30;
        if (draw_lw(use + 0x10) == 0 || !(draw_lwc1(rec + 0x1c) > 0.0f) || ((g_slot_old.load() >> i) & 1u) != 0) continue;
        pts[i][0] = draw_lwc1(rec + 0x10);
        pts[i][1] = draw_lwc1(rec + 0x14);
        pts[i][2] = draw_lwc1(rec + 0x18);
        pts[i][3] = draw_lwc1(rec + 0x1c);
    }
}

// orig: (host) see mrp_lighting.h
void mrp_lighting_frame_begin() {
    g_sh.on = false;
    ++g_mat_frames;  // (the materials' clock, and the High step's picture: the last frame's)
    if (std::getenv("RAC1_POINT_TRACE") != nullptr) {  // (testing: the game's 8 dynamic point lights, 0x1802c0 / 0x1804c0, every 20 draws)
        static uint64_t n = 0;
        if (n++ % 20 == 0) {
            for (uint32_t i = 0; i < 8; ++i) {
                const uint32_t rec = OV(0x001802c0u) + i * 0x20, use = OV(0x001804c0u) + i * 0x30;
                if (draw_lw(use + 0x10) == 0) continue;
                std::printf("[point] draw %llu slot %u colour %.2f %.2f %.2f k %.2f pos %.1f %.1f %.1f r %.1f\n", static_cast<unsigned long long>(n), i, draw_lwc1(rec), draw_lwc1(rec + 4),
                            draw_lwc1(rec + 8), draw_lwc1(rec + 0xc), draw_lwc1(rec + 0x10), draw_lwc1(rec + 0x14), draw_lwc1(rec + 0x18), draw_lwc1(rec + 0x1c));
            }
        }
    }
}


// orig: (host) see mrp_lighting.h
bool mrp_lighting_cull_params(float sun[3], float splits[3], float* back, float* tan_x, float* tan_y) {
    if (g_modern_shadows == 0 || !g_sun_known.load()) return false;
    if (tan_x != nullptr) *tan_x = g_cull_tan[0];  // (the last drawn frame's projection)
    if (tan_y != nullptr) *tan_y = g_cull_tan[1];
    for (int i = 0; i < 3; ++i) {
        sun[i] = g_cull_sun[i];
        splits[i] = g_sh.splits[i];
    }
    *back = kBack;
    return true;
}

namespace {
float g_pick_uv[2] = {-1.0f, -1.0f};  // (the point of the screen asked: 0..1 of the 3D's viewport, from the bottom left)
bool g_pick_ok = false;
float g_pick_world[3] = {0.0f, 0.0f, 0.0f};
// orig: (host) the asked screen point's world position: the 3D's depth there (after the world and the mobys, before the effects)
// through the world's view-projection inverted
// orig: (host) testing, RAC1_LAMP_SCAN=<every>: every that many frames the 3D picture (before the effects) is read, its bright
// texels (luma >= 0.8) not on the sky taken through the depth to the world and to the objects there (mrp_lighting_pick); per class
// how often, the mean colour and a place, printed at the level's end and every 600 frames: a survey of what glows on screen
float hue_deg(float r, float g, float b) {
    const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
    if (!(mx - mn > 1e-6f)) return 0.0f;
    const float h = mx == r ? std::fmod((g - b) / (mx - mn), 6.0f) : mx == g ? (b - r) / (mx - mn) + 2.0f : (r - g) / (mx - mn) + 4.0f;
    return h < 0.0f ? h * 60.0f + 360.0f : h * 60.0f;
}
void lamp_scan(const int vp[4]) {
    static int every = -1, from = 0;
    if (every < 0) {  // (RAC1_LAMP_SCAN=<every>[,<from frame>])
        every = 0;
        if (const char* e = std::getenv("RAC1_LAMP_SCAN")) {
            std::sscanf(e, "%d,%d", &every, &from);
            every = std::max(1, every);
        }
    }
    if (every == 0) return;
    static uint64_t n = 0;
    ++n;
    struct Acc {
        int hits = 0;
        double c[3] = {0, 0, 0};
        float at[3] = {0, 0, 0};
    };
    static std::map<std::pair<int, int>, Acc> acc;  // (kind 0 tie, 1 shrub, 2 moby, 3 none; class)
    if (n % 600 == 0 && !acc.empty()) {
        std::printf("[lamps] level %d, %llu frames:\n", static_cast<int>(draw_lw(0x0015ed84)), static_cast<unsigned long long>(n));
        for (const auto& [k, a] : acc)
            if (a.hits >= 3)
                std::printf("[lamps]   %s %d: %d hits, colour %.2f %.2f %.2f, e.g. at %.1f %.1f %.1f\n", k.first == 0 ? "tie" : k.first == 1 ? "shrub" : k.first == 2 ? "moby" : "world",
                            k.second, a.hits, a.c[0] / a.hits, a.c[1] / a.hits, a.c[2] / a.hits, a.at[0], a.at[1], a.at[2]);
    }
    if (n % static_cast<uint64_t>(every) != 0 || n < static_cast<uint64_t>(from)) return;
    const int w = vp[2], h = vp[3];
    std::vector<uint8_t> col(static_cast<size_t>(w) * h * 4);
    std::vector<float> dep(static_cast<size_t>(w) * h);
    GLint rfb = 0, dfb = 0;
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(dfb));
    glReadPixels(vp[0], vp[1], w, h, GL_RGBA, GL_UNSIGNED_BYTE, col.data());
    glReadPixels(vp[0], vp[1], w, h, GL_DEPTH_COMPONENT, GL_FLOAT, dep.data());
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
    float m[16];
    mrp_world_view_proj(m);
    const glm::mat4 inv = glm::inverse(glm::make_mat4(m));
    for (int y = 2; y < h; y += 6)
        for (int x = 2; x < w; x += 6) {
            const size_t i = static_cast<size_t>(y) * w + x;
            const float d = dep[i];
            if (!(d < 1.0f)) continue;
            const float r = col[4 * i] / 255.0f, g = col[4 * i + 1] / 255.0f, b = col[4 * i + 2] / 255.0f;
            const float mx = std::max({r, g, b}), mn = std::min({r, g, b});
            const float hue = hue_deg(r, g, b);
            // (bright, and white, cool or strongly coloured: a sunlit leaf or a sandy rock is a warm, half-saturated colour)
            if (0.299f * r + 0.587f * g + 0.114f * b < 0.75f || !(mx - mn <= 0.2f * mx || (hue >= 160.0f && hue <= 300.0f) || (mx - mn) >= 0.75f * mx)) continue;
            const glm::vec4 wp = inv * glm::vec4((x + 0.5f) / w * 2.0f - 1.0f, (y + 0.5f) / h * 2.0f - 1.0f, d * 2.0f - 1.0f, 1.0f);
            const float p[3] = {wp.x / wp.w, wp.y / wp.w, wp.z / wp.w};
            const LightPick pk = mrp_lighting_pick(p);
            const std::pair<int, int> key = pk.shrub ? std::make_pair(1, pk.shrub_class) : pk.tie ? std::make_pair(0, pk.tie_class)
                                          : pk.moby && pk.moby_dist < 3.0f ? std::make_pair(2, pk.moby_class) : std::make_pair(3, 0);
            Acc& a = acc[key];
            ++a.hits;
            a.c[0] += r;
            a.c[1] += g;
            a.c[2] += b;
            for (int k = 0; k < 3; ++k) a.at[k] = p[k];
        }
}
// orig: (host) Vulkan: the depth at (x, y) of the 3D's depth buffer, as the frame before last left it (no wait: a copy into
// this frame slot's buffer, read when the slot comes back - the frame's fence waited by then)
bool vk_depth_at(int x, int y, float* d) {
    static vk::GpuBuffer buf[vk::kFramesInFlight];
    static bool written[vk::kFramesInFlight] = {};
    const uint32_t fi = vk::frame_index();
    vk::GpuBuffer& b = buf[fi];
    if (b.buffer == VK_NULL_HANDLE &&
        !vk::create_buffer(16, VK_BUFFER_USAGE_TRANSFER_DST_BIT, VK_MEMORY_PROPERTY_HOST_VISIBLE_BIT | VK_MEMORY_PROPERTY_HOST_COHERENT_BIT, b, true))
        return false;
    bool ok = false;
    if (written[fi]) {
        uint32_t raw = 0;
        std::memcpy(&raw, b.mapped, 4);
        if (vk::depth_stencil_format() == VK_FORMAT_D24_UNORM_S8_UINT) *d = static_cast<float>(raw & 0xffffffu) / 16777215.0f;
        else std::memcpy(d, &raw, 4);
        ok = true;
    }
    vk::GpuImage& depth = vk::active_depth_image();
    if (x < 0 || y < 0 || x >= static_cast<int>(depth.width) || y >= static_cast<int>(depth.height)) return ok;
    VkCommandBuffer cb = vk::cmd();
    vk::end_rendering();
    const VkImageLayout was = depth.layout;
    vk::transition_image(cb, depth, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
    VkBufferImageCopy c{};
    c.imageSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
    c.imageOffset = {x, y, 0};
    c.imageExtent = {1, 1, 1};
    vkCmdCopyImageToBuffer(cb, depth.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, b.buffer, 1, &c);
    vk::transition_image(cb, depth, was);
    written[fi] = true;
    return ok;
}

void screen_point_read(const int vp[4]) {
    // (testing: RAC1_PICK_SCREEN=u,v[;u,v...] - each point printed with its objects every 120 frames, one a frame)
    static std::vector<std::pair<float, float>> env_uv;
    static bool env_read = false;
    static uint64_t n = 0;
    if (!env_read) {
        env_read = true;
        if (const char* e = std::getenv("RAC1_PICK_SCREEN"))
            for (const char* q = e; q != nullptr && *q != 0;) {
                float u = 0, v = 0;
                if (std::sscanf(q, "%f,%f", &u, &v) == 2 && env_uv.size() < 32) env_uv.emplace_back(u, v);
                q = std::strchr(q, ';');
                if (q != nullptr) ++q;
            }
    }
    const size_t at = env_uv.empty() ? 0 : static_cast<size_t>(n % 120);
    if (env_uv.size() == 1 && g_pick_ok) {  // (testing: the seas along the one point's ray, the last depth read's)
        float m[16];
        mrp_world_view_proj(m);
        const glm::vec4 e = glm::inverse(glm::make_mat4(m)) * glm::vec4(env_uv[0].first * 2.0f - 1.0f, env_uv[0].second * 2.0f - 1.0f, -1.0f, 1.0f);
        const glm::vec3 o = glm::vec3(e) / e.w, r = glm::vec3(g_pick_world[0], g_pick_world[1], g_pick_world[2]) - o;
        const float len = glm::length(r);
        if (len > 0.0f) {
            const float oo[3] = {o.x, o.y, o.z}, dd[3] = {r.x / len, r.y / len, r.z / len};
            mrp_fx_surface_pick(oo, dd, 4000.0f);
        }
    }
    if (!env_uv.empty()) {
        ++n;
        if (at >= env_uv.size()) return;
        g_pick_uv[0] = env_uv[at].first;
        g_pick_uv[1] = env_uv[at].second;
    }
    if (g_pick_uv[0] < 0.0f) return;
    const int x = vp[0] + static_cast<int>(g_pick_uv[0] * static_cast<float>(vp[2])), y = vp[1] + static_cast<int>(g_pick_uv[1] * static_cast<float>(vp[3]));
    float d = 1.0f;
    if (vk::is_active()) {
        if (!vk_depth_at(x, y, &d)) d = 1.0f;
    } else {
        GLint rfb = 0;
        glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
        GLint dfb = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(dfb));
        glReadPixels(x, y, 1, 1, GL_DEPTH_COMPONENT, GL_FLOAT, &d);
        glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
    }
    g_pick_ok = d < 1.0f;
    if (g_pick_ok) {
        float m[16];
        mrp_world_view_proj(m);
        const glm::vec4 w = glm::inverse(glm::make_mat4(m)) * glm::vec4(g_pick_uv[0] * 2.0f - 1.0f, g_pick_uv[1] * 2.0f - 1.0f, d * 2.0f - 1.0f, 1.0f);
        for (int k = 0; k < 3; ++k) g_pick_world[k] = w[k] / w.w;
    }
    if (!env_uv.empty()) {
        if (g_pick_ok) {
            const LightPick pk = mrp_lighting_pick(g_pick_world);
            std::printf("[lights] screen %.2f %.2f: %.2f %.2f %.2f; tie %d class %d offset %.3f %.3f %.3f; shrub %d class %d offset %.3f %.3f %.3f; moby %d class %d (%.2f away)\n",
                        g_pick_uv[0], g_pick_uv[1], g_pick_world[0], g_pick_world[1], g_pick_world[2], pk.tie ? 1 : 0, pk.tie_class, pk.tie_off[0], pk.tie_off[1], pk.tie_off[2],
                        pk.shrub ? 1 : 0, pk.shrub_class, pk.shrub_off[0], pk.shrub_off[1], pk.shrub_off[2], pk.moby ? 1 : 0, pk.moby_class, pk.moby_dist);
            if (pk.moby)
                std::printf("[lights] screen %.2f %.2f: moby %08x (%s), joint %d at %.3f %.3f %.3f in its frame\n", g_pick_uv[0], g_pick_uv[1], pk.moby_addr,
                            pk.moby_part ? "its parts hold the point" : "the nearest origin", pk.moby_joint, pk.moby_joint_off[0], pk.moby_joint_off[1], pk.moby_joint_off[2]);
            // (the surface there: an emissive line's kind, class, texture - within 2 % of the distance from the eye, as the debug menu)
            float m[16];
            mrp_world_view_proj(m);
            const glm::vec4 e = glm::inverse(glm::make_mat4(m)) * glm::vec4(g_pick_uv[0] * 2.0f - 1.0f, g_pick_uv[1] * 2.0f - 1.0f, -1.0f, 1.0f);
            const float dist = glm::length(glm::vec3(e) / e.w - glm::vec3(g_pick_world[0], g_pick_world[1], g_pick_world[2]));
            const SurfacePick sp = mrp_world_surface_at(g_pick_world, std::max(0.5f, 0.02f * dist));
            if (sp.kind >= 0)
                std::printf("[lights] screen %.2f %.2f: surface %s %d tex %d (level texture %d, %.3f away)\n", g_pick_uv[0], g_pick_uv[1], mrp_emissive_kind_name(sp.kind), sp.o_class,
                            sp.tex, sp.texture, sp.dist);
            if (env_uv.size() == 1) {  // (one point: the seas along its ray too, asked every frame - the answer comes from the frames after)
                const FxSurfacePick sea = mrp_fx_surface_pick_result();
                if (sea.hit) std::printf("[lights] screen %.2f %.2f: a sea, surface %d tex %d, %.2f along the ray\n", g_pick_uv[0], g_pick_uv[1], sea.o_class, sea.tex, sea.t);
            }
        }
    }
}
}  // namespace

// orig: (host) see mrp_lighting.h
bool mrp_lighting_screen_point(float u, float v, float out[3]) {
    g_pick_uv[0] = u;
    g_pick_uv[1] = v;
    for (int k = 0; k < 3; ++k) out[k] = g_pick_world[k];
    return g_pick_ok;
}

// orig: (host) see mrp_lighting.h
LightPick mrp_lighting_pick(const float p[3]) {
    LightPick r;
    const glm::vec3 P(p[0], p[1], p[2]);
    float best = 1e30f;
    float best_shrub = 1e30f;
    for (const TieSpot& t : mrp_world_tie_instances()) {
        const glm::vec3 d = P - glm::vec3(t.pos[0], t.pos[1], t.pos[2]);
        if (glm::dot(d, d) > 400.0f * 400.0f) continue;
        if (t.shrub) {  // (a shrub: its bounding sphere, the smallest holding the point)
            const float ds = glm::length(P - glm::vec3(t.sphere[0], t.sphere[1], t.sphere[2]));
            if (ds <= t.sphere[3] + 0.2f && t.sphere[3] < best_shrub) {
                best_shrub = t.sphere[3];
                r.shrub = true;
                r.shrub_class = t.o_class;
                const glm::mat3 M(glm::vec3(t.rows[0][0], t.rows[0][1], t.rows[0][2]), glm::vec3(t.rows[1][0], t.rows[1][1], t.rows[1][2]),
                                  glm::vec3(t.rows[2][0], t.rows[2][1], t.rows[2][2]));
                const glm::vec3 l = std::fabs(glm::determinant(M)) > 1e-12f ? glm::inverse(M) * d : glm::vec3(0.0f);
                for (int k = 0; k < 3; ++k) r.shrub_off[k] = l[k];
            }
            continue;
        }
        const glm::mat3 M(glm::vec3(t.rows[0][0], t.rows[0][1], t.rows[0][2]), glm::vec3(t.rows[1][0], t.rows[1][1], t.rows[1][2]),
                          glm::vec3(t.rows[2][0], t.rows[2][1], t.rows[2][2]));  // (columns: world = pos + M l)
        if (std::fabs(glm::determinant(M)) < 1e-12f) continue;
        const glm::vec3 l = glm::inverse(M) * d;
        bool in = true;
        float vol = 1.0f;
        for (int k = 0; k < 3; ++k) {
            const float m = 0.15f;
            in = in && l[k] >= t.lo[k] - m && l[k] <= t.hi[k] + m;
            vol *= std::max(t.hi[k] - t.lo[k], 0.05f);
        }
        if (in && vol < best) {
            best = vol;
            r.tie = true;
            r.tie_class = t.o_class;
            for (int k = 0; k < 3; ++k) r.tie_off[k] = l[k];
        }
    }
    const uint32_t first = draw_lwu(OV(0x0015ffd8u)), last = draw_lwu(OV(0x0015ffe0u));
    auto frame_off = [&](uint32_t m) {  // (the point in the moby's frame: its position, its rows normalised)
        const glm::vec3 mp(draw_lwc1(m + 0x10), draw_lwc1(m + 0x14), draw_lwc1(m + 0x18));
        for (uint32_t k = 0; k < 3; ++k) {
            glm::vec3 row(draw_lwc1(m + 0xc0 + 0x10 * k), draw_lwc1(m + 0xc4 + 0x10 * k), draw_lwc1(m + 0xc8 + 0x10 * k));
            const float len = glm::length(row);
            r.moby_off[k] = len > 0.0f ? glm::dot(P - mp, row / len) : 0.0f;
        }
    };
    // the drawn moby whose parts hold the point (a big one - a cannon, a train - whose origin is far from the part clicked), else
    // the live moby whose origin is nearest within 6 units (a moby whose origin is at its foot: a door frame, a lamp post)
    if (uint32_t ma = 0; mrp_moby_at(p, &ma, &r.moby_class, &r.moby_dist)) {
        r.moby = true;
        r.moby_part = true;
        r.moby_addr = ma;
        frame_off(ma);
    } else {
        r.moby_dist = 6.0f;
        for (uint32_t m = first; m != 0 && m < 0x01fe0000u && (last == 0 || m <= last) && draw_lbu(m + 0x20) != 0xff; m += 0x100) {
            if (static_cast<int8_t>(draw_lbu(m + 0x20)) < 0) continue;
            const glm::vec3 mp(draw_lwc1(m + 0x10), draw_lwc1(m + 0x14), draw_lwc1(m + 0x18));
            const float d = glm::length(P - mp);
            if (d >= r.moby_dist) continue;
            r.moby = true;
            r.moby_dist = d;
            r.moby_class = draw_lh(m + 0xa6);
            r.moby_addr = m;
            frame_off(m);
        }
    }
    if (r.moby) {  // its pose this frame: the joint nearest the point, the point in that joint's frame
        float jp[3], ax[9], best = 1e30f;
        int n = 0;
        if (mrp_moby_draw_frame(r.moby_addr, -1, jp, ax, &n))
            for (int j = 0; j < n && j < 128; ++j) {
                if (!mrp_moby_draw_frame(r.moby_addr, j, jp, ax)) continue;
                const glm::vec3 d = P - glm::vec3(jp[0], jp[1], jp[2]);
                if (glm::length(d) >= best) continue;
                best = glm::length(d);
                r.moby_joint = j;
                for (int k = 0; k < 3; ++k) r.moby_joint_off[k] = glm::dot(d, glm::vec3(ax[3 * k], ax[3 * k + 1], ax[3 * k + 2]));
            }
    }
    return r;
}

namespace {
// orig: (host) the lights per object class (g_class_lights) near the camera added to the frame's point lights after the ones placed
// by hand: the tie instances of each class (mrp_world_tie_instances) and the live mobys of each (the list from *(0x15ffd8), 0x100
// each, to the state 0xff), the offset turned with each; the nearest first, faded at the reach and at the first left out (no pop)
void class_lights_add(double t) {
    if (const char* e = std::getenv("RAC1_TIE_NEAR")) {  // (testing: the tie instances near a point, once: x,y,z,r)
        static bool shown = false;
        float x = 0, y = 0, z = 0, r = 6, at = 60;  // (at: the shadow pass's frame to print at, default 60)
        if (!shown && std::sscanf(e, "%f,%f,%f,%f,%f", &x, &y, &z, &r, &at) >= 3 && static_cast<float>(g_sh.frame) > at) {
            shown = true;
            for (const TieSpot& ti : mrp_world_tie_instances()) {
                const float d = glm::length(glm::vec3(ti.pos[0], ti.pos[1], ti.pos[2]) - glm::vec3(x, y, z));
                if (d <= r) {
                    const glm::mat3 M(glm::vec3(ti.rows[0][0], ti.rows[0][1], ti.rows[0][2]), glm::vec3(ti.rows[1][0], ti.rows[1][1], ti.rows[1][2]),
                                      glm::vec3(ti.rows[2][0], ti.rows[2][1], ti.rows[2][2]));
                    const glm::vec3 l = std::fabs(glm::determinant(M)) > 1e-12f ? glm::inverse(M) * (glm::vec3(x, y, z) - glm::vec3(ti.pos[0], ti.pos[1], ti.pos[2])) : glm::vec3(0.0f);
                    std::printf("[lights] %s class %d instance at %.2f %.2f %.2f (%.1f away); the point at %.2f %.2f %.2f, box %.2f %.2f %.2f .. %.2f %.2f %.2f, sphere %.1f %.1f %.1f r %.1f\n",
                                ti.shrub ? "shrub" : "tie", ti.o_class, ti.pos[0], ti.pos[1], ti.pos[2], d, l.x, l.y, l.z, ti.lo[0], ti.lo[1], ti.lo[2], ti.hi[0], ti.hi[1], ti.hi[2],
                                ti.sphere[0], ti.sphere[1], ti.sphere[2], ti.sphere[3]);
                }
            }
            const uint32_t first = draw_lwu(OV(0x0015ffd8u)), last = draw_lwu(OV(0x0015ffe0u));
            for (uint32_t m = first; m != 0 && m < 0x01fe0000u && (last == 0 || m <= last) && draw_lbu(m + 0x20) != 0xff; m += 0x100) {
                const glm::vec3 mp(draw_lwc1(m + 0x10), draw_lwc1(m + 0x14), draw_lwc1(m + 0x18));
                const float d = glm::length(mp - glm::vec3(x, y, z));
                if (d <= r) std::printf("[lights] moby class %d (state %d) at %.2f %.2f %.2f (%.1f away) +0x30 %02x +0x32 %02x +0x34 %04x\n", draw_lh(m + 0xa6), draw_lbu(m + 0x20), mp.x, mp.y, mp.z, d, draw_lbu(m + 0x30), draw_lbu(m + 0x32), draw_lhu(m + 0x34));
            }
        }
    }
    if (const char* e = std::getenv("RAC1_PICK")) {  // (testing: the objects at a point, once: x,y,z)
        static bool shown = false;
        float q[3] = {};
        if (!shown && g_sh.frame > 60 && std::sscanf(e, "%f,%f,%f", &q[0], &q[1], &q[2]) == 3) {
            shown = true;
            const LightPick pk = mrp_lighting_pick(q);
            std::printf("[lights] pick %.2f %.2f %.2f: tie %d class %d offset %.3f %.3f %.3f; shrub %d class %d offset %.3f %.3f %.3f; moby %d class %d (%.2f away) offset %.3f %.3f %.3f\n",
                        q[0], q[1], q[2], pk.tie ? 1 : 0, pk.tie_class, pk.tie_off[0], pk.tie_off[1], pk.tie_off[2], pk.shrub ? 1 : 0, pk.shrub_class, pk.shrub_off[0], pk.shrub_off[1],
                        pk.shrub_off[2], pk.moby ? 1 : 0, pk.moby_class, pk.moby_dist, pk.moby_off[0], pk.moby_off[1], pk.moby_off[2]);
        }
    }
    {  // the level's lights per class, once per level: how many instances each finds (a class with none: a wrong class or kind)
        static int shown_level = -1;
        const int level = static_cast<int>(draw_lw(0x0015ed84));
        if (level != shown_level && g_user_level == level && g_sh.frame > 30) {
            shown_level = level;
            if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr)  // (testing: the debug menu's emissive candidates)
                for (const EmissiveCandidate& c : mrp_world_emissive_candidates())
                    std::printf("[lights] emissive candidate: %s %d tex %d (level texture %d): %.1f %% glowing, %.1f %% near white\n", mrp_emissive_kind_name(c.kind), c.o_class,
                                c.tex, c.texture, 100.0f * c.glow, 100.0f * c.white);
            for (const ClassLight& cl : g_class_lights) {
                int n = 0;
                if (cl.moby) {
                    const uint32_t first = draw_lwu(OV(0x0015ffd8u)), last = draw_lwu(OV(0x0015ffe0u));
                    for (uint32_t m = first; m != 0 && m < 0x01fe0000u && (last == 0 || m <= last) && draw_lbu(m + 0x20) != 0xff; m += 0x100)
                        n += static_cast<int8_t>(draw_lbu(m + 0x20)) >= 0 && draw_lh(m + 0xa6) == cl.o_class ? 1 : 0;
                } else {
                    static const bool trace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
                    for (const TieSpot& t : mrp_world_tie_instances()) {
                        if (t.shrub != cl.shrub || t.o_class != cl.o_class) continue;
                        ++n;
                        if (trace) {  // (testing: where each instance's light sits)
                            glm::vec3 p(t.pos[0], t.pos[1], t.pos[2]);
                            for (int r = 0; r < 3; ++r) p += glm::vec3(t.rows[r][0], t.rows[r][1], t.rows[r][2]) * cl.off[r];
                            std::printf("[lights]   instance at %.2f %.2f %.2f: the light at %.2f %.2f %.2f\n", t.pos[0], t.pos[1], t.pos[2], p.x, p.y, p.z);
                        }
                    }
                }
                std::printf("[lights] level %d: emit %s %d: %d instances%s\n", level, cl.moby ? "moby" : cl.shrub ? "shrub" : "tie", cl.o_class, n,
                            cl.moby ? " (live now)" : "");
            }
        }
    }
    if (g_class_lights.empty() || g_sh.npts >= kMaxPts) return;
    struct Cand {
        float d;
        glm::vec3 p;
        size_t cl;
        uint32_t id;
    };
    static std::vector<Cand> cand;
    cand.clear();
    const float reach = g_sh.splits[1];
    auto add = [&](size_t i, const glm::vec3& p, uint32_t id) {
        const float d = glm::length(p - g_sh.cam) - g_class_lights[i].radius;
        if (d < reach) cand.push_back({d, p, i, id});
    };
    bool any_moby = false;
    for (size_t i = 0; i < g_class_lights.size(); ++i) {
        const ClassLight& cl = g_class_lights[i];
        if (!cl.on || !(cl.radius > 0.0f)) continue;
        if (cl.moby) {
            any_moby = true;
            continue;
        }
        const std::vector<TieSpot>& ties = mrp_world_tie_instances();
        for (size_t k = 0; k < ties.size(); ++k) {
            const TieSpot& ti = ties[k];
            if (ti.o_class != cl.o_class || ti.shrub != cl.shrub) continue;
            glm::vec3 p(ti.pos[0], ti.pos[1], ti.pos[2]);
            for (int r = 0; r < 3; ++r) p += glm::vec3(ti.rows[r][0], ti.rows[r][1], ti.rows[r][2]) * cl.off[r];
            add(i, p, static_cast<uint32_t>(k));
        }
    }
    if (any_moby) {  // (the whole moby array to its end *(0x15ffe0), not the level's placed ones alone (*(0x15ffdc)): the spawned mobys -
                     // a nanotech crate's orbs, bombs, enemies from a spawner - light too)
        const uint32_t first = draw_lwu(OV(0x0015ffd8u)), last = draw_lwu(OV(0x0015ffe0u));
        for (uint32_t m = first; m != 0 && m < 0x01fe0000u && (last == 0 || m <= last) && draw_lbu(m + 0x20) != 0xff; m += 0x100) {
            if (static_cast<int8_t>(draw_lbu(m + 0x20)) < 0) continue;  // (a dead slot)
            const int cls = draw_lh(m + 0xa6);
            for (size_t i = 0; i < g_class_lights.size(); ++i) {
                const ClassLight& cl = g_class_lights[i];
                if (!cl.on || !cl.moby || cl.o_class != cls) continue;
                static const bool jtrace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
                if (jtrace && g_sh.frame % 120 == 0) {  // (testing: the moby's position against its drawn model's origin and its first joints')
                    float o[3] = {0.0f, 0.0f, 0.0f}, a9[9];
                    int nj = 0;
                    std::string js;
                    const bool drawn = mrp_moby_draw_frame(m, -1, o, a9, &nj);
                    if (drawn)
                        for (int j = 0; j < nj; ++j)  // (each joint's height over the origin, its distance)
                            if (float q[3]; mrp_moby_draw_frame(m, j, q, a9)) {
                                char b[48];
                                std::snprintf(b, sizeof b, " j%d %.2f/%.2f", j, q[2] - o[2], std::sqrt((q[0] - o[0]) * (q[0] - o[0]) + (q[1] - o[1]) * (q[1] - o[1]) + (q[2] - o[2]) * (q[2] - o[2])));
                                js += b;
                            }
                    std::printf("[lights] moby %08x class %d at %.2f %.2f %.2f: %s %.2f %.2f %.2f (%d joints)%s\n", m, cls, draw_lwc1(m + 0x10), draw_lwc1(m + 0x14), draw_lwc1(m + 0x18),
                                drawn ? "drawn at" : "not drawn", o[0], o[1], o[2], nj, js.c_str());
                }
                if (float jp[3], ax[9]; cl.joint >= 0 && mrp_moby_draw_frame(m, cl.joint, jp, ax)) {  // ("joint <n>": that joint's frame as the pose has it)
                    glm::vec3 p(jp[0], jp[1], jp[2]);
                    for (int r = 0; r < 3; ++r) p += glm::vec3(ax[3 * r], ax[3 * r + 1], ax[3 * r + 2]) * cl.off[r];
                    add(i, p, m);
                    continue;
                }
                glm::vec3 p(draw_lwc1(m + 0x10), draw_lwc1(m + 0x14), draw_lwc1(m + 0x18));
                for (uint32_t r = 0; r < 3; ++r) {  // (the moby's rows, normalised: the offset in world units, turned with it)
                    glm::vec3 row(draw_lwc1(m + 0xc0 + 0x10 * r), draw_lwc1(m + 0xc4 + 0x10 * r), draw_lwc1(m + 0xc8 + 0x10 * r));
                    const float len = glm::length(row);
                    if (len > 0.0f) row /= len;
                    p += row * cl.off[r];
                }
                add(i, p, m);
            }
        }
    }
    std::sort(cand.begin(), cand.end(), [](const Cand& a, const Cand& b) { return a.d < b.d; });
    const int budget = kMaxPts - g_sh.npts;
    const int n = std::min(budget, static_cast<int>(cand.size()));
    constexpr float kFade = 6.0f;
    const float cut = static_cast<int>(cand.size()) > budget ? cand[static_cast<size_t>(budget)].d : reach;
    for (int k = 0; k < n; ++k) {
        const Cand& c = cand[static_cast<size_t>(k)];
        const ClassLight& cl = g_class_lights[c.cl];
        float f = cl.intensity * std::clamp((reach - c.d) / kFade, 0.0f, 1.0f) * std::clamp((cut - c.d) / kFade, 0.0f, 1.0f);
        if (cl.flicker > 0.0f) {  // (as the lights placed by hand: per instance)
            const double ph = static_cast<double>(c.id % 997) * 1.7;
            const double w = 0.5 * std::sin(t * 7.3 + ph) + 0.3 * std::sin(t * 13.1 + ph * 2.0) + 0.2 * std::sin(t * 29.7 + ph * 3.0);
            f *= 1.0f - cl.flicker * 0.35f * static_cast<float>(1.0 + w);
        }
        if (static_cast<int>(c.cl) == g_class_light_selected) f *= 0.6f + 0.4f * static_cast<float>(std::sin(t * 8.0));  // (the debug menu's: pulsing)
        g_sh.pt_pos[g_sh.npts] = glm::vec4(c.p, cl.radius);
        g_sh.pt_col[g_sh.npts] = glm::vec4(cl.col[0] * f, cl.col[1] * f, cl.col[2] * f, -1.0f);
        g_sh.pt_slot[g_sh.npts] = cl.moby ? -2 : -1;  // (a moby's lamp: no shadow cube - its own mesh around the light would hide it)
        g_sh.pt_down[g_sh.npts] = cl.down;
        ++g_sh.npts;
    }
    if (std::getenv("RAC1_LIGHTS_TRACE") != nullptr) {
        static uint64_t calls = 0;
        if (calls++ % 60 == 0) std::printf("[lights] per class: %zu near, %d taken\n", cand.size(), n);
    }
}

// orig: (host) our choice (#102): Veldin's plasma beams - the shooting robot's (class 1440) beam, drawn by the class-1471 effect
// manager - light the world while they are on: a violet-white light where the beam ends (its impact, 0x1e6de0[i]) and a weaker
// one at its muzzle (the matrix 0x1e6d20[i]'s position), for each of the manager's 3 slots (state 0x161bf8[i] 2 to 4, holding
// the robot 0x161c88[i]). Veldin's overlay only (level 0; the robot's own pointer checked); no shadow cube.
void beam_lights_add() {
    static const bool off = std::getenv("RAC1_BEAM_LIGHTS") != nullptr && std::atoi(std::getenv("RAC1_BEAM_LIGHTS")) == 0;  // (testing: 0 none)
    if (off || draw_lw(0x0015ed84) != 0) return;
    const uint32_t first = draw_lwu(OV(0x0015ffd8u)), last = draw_lwu(OV(0x0015ffe0u));
    for (uint32_t i = 0; i < 3 && g_sh.npts + 2 <= kMaxPts; ++i) {
        const int32_t st = draw_lw(OV(0x00161bf8u) + i * 4);
        const uint32_t who = draw_lwu(OV(0x00161c88u) + i * 4);
        if (st < 2 || st > 4 || who < first || who > last || ((who - first) & 0xffu) != 0 || draw_lh(who + 0xa6) != 1440) continue;
        const uint32_t e = OV(0x001e6de0u) + i * 0x10, s = OV(0x001e6d20u) + i * 0x40 + 0x30;
        const glm::vec3 end(draw_lwc1(e), draw_lwc1(e + 4), draw_lwc1(e + 8)), start(draw_lwc1(s), draw_lwc1(s + 4), draw_lwc1(s + 8));
        const glm::vec3 col(0.70f, 0.66f, 1.00f);
        static const bool trace = std::getenv("RAC1_LIGHTS_TRACE") != nullptr;
        if (trace) std::printf("[lights] beam %u (state %d): %.1f %.1f %.1f -> %.1f %.1f %.1f\n", i, st, start.x, start.y, start.z, end.x, end.y, end.z);
        const struct {
            glm::vec3 p;
            float k, r;
        } pts[2] = {{end, 2.4f, 6.0f}, {start, 1.2f, 4.0f}};
        for (const auto& q : pts) {
            if (glm::length(q.p - g_sh.cam) - q.r > g_sh.splits[1]) continue;
            g_sh.pt_pos[g_sh.npts] = glm::vec4(q.p, q.r);
            g_sh.pt_col[g_sh.npts] = glm::vec4(col * q.k, -1.0f);
            g_sh.pt_slot[g_sh.npts] = -2;  // (no shadow cube)
            g_sh.pt_down[g_sh.npts] = 0.0f;
            ++g_sh.npts;
        }
    }
}
}  // namespace

// orig: (host) see mrp_lighting.h
void mrp_lighting_shadow_pass() {
    g_sh.on = false;
    if (g_modern_shadows == 0 || !mrp_world_active() || !mrp_moby_active() || g_mrp_world_projection != 0) return;
    ++g_sh.frame;
    if (g_surface_detail > 0 && g_rain) mrp_particles_sky_frame();  // (the materials' rain rings: none under a roof)
    static bool splits_read = false;
    if (!splits_read) {
        read_splits();
        splits_read = true;
    }
    static const bool prof = std::getenv("RAC1_SHADOW_PROF") != nullptr;  // (testing: the pass's GPU time, cascades and point cubes)
    static GLuint q[2] = {};
    static double acc[2] = {};
    static int frames = 0, cubes_acc = 0;
    if (!vk::is_active() && prof && q[0] == 0) glGenQueries(2, q);
    if (!vk::is_active() && prof) glBeginQuery(GL_TIME_ELAPSED, q[0]);
    int prof_cascades = rac1::prof::begin(rac1::prof::kShadowCascades);  // (the debug menu's profiler: the frame's shadow set-up and cascades)
    ensure_resources();
    fit_cascades();
    // the game's dynamic point lights in use (0x1804c0 + 0x10) with a colour and a radius: lit per pixel this frame
    g_sh.npts = 0;
    std::fill(std::begin(g_sh.pt_down), std::end(g_sh.pt_down), 0.0f);  // (every way alike but a lamp per class aimed down)
    for (uint32_t i = 0; i < 8 && g_modern_points != 0; ++i) {
        const uint32_t rec = OV(0x001802c0u) + i * 0x20, use = OV(0x001804c0u) + i * 0x30;
        if (draw_lw(use + 0x10) == 0 || ((g_slot_old.load() >> i) & 1u) != 0) continue;  // (the level's own lights: the game's)
        const glm::vec3 col(draw_lwc1(rec), draw_lwc1(rec + 4), draw_lwc1(rec + 8));
        const float r = draw_lwc1(rec + 0x1c);
        if (!(r > 0.0f) || col.x + col.y + col.z <= 0.0f) continue;
        g_sh.pt_pos[g_sh.npts] = glm::vec4(draw_lwc1(rec + 0x10), draw_lwc1(rec + 0x14), draw_lwc1(rec + 0x18), r);
        g_sh.pt_col[g_sh.npts] = glm::vec4(col, -1.0f);
        g_sh.pt_slot[g_sh.npts] = static_cast<int>(i);
        ++g_sh.npts;
    }
    g_sh.ngame = g_sh.npts;
    if (g_modern_points != 0 && g_auto_lights != 0) {  // (step 3) the lights placed by hand, before the emitters
        const int level = static_cast<int>(draw_lw(0x0015ed84));
        if (level != g_user_level) user_load(level);
        const double t = static_cast<double>(g_sh.frame) / 60.0;
        for (size_t i = 0; i < g_user.size() && g_sh.npts < kMaxPts; ++i) {
            const UserLight& l = g_user[i];
            if (!l.on || !(l.radius > 0.0f)) continue;
            const glm::vec3 p(l.pos[0], l.pos[1], l.pos[2]);
            if (glm::length(p - g_sh.cam) - l.radius > g_sh.splits[1]) continue;
            float k = l.intensity;
            if (l.flicker > 0.0f) {  // (a flame's: two slow waves and a fast one, per light)
                const double ph = static_cast<double>(i) * 1.7;
                const double n = 0.5 * std::sin(t * 7.3 + ph) + 0.3 * std::sin(t * 13.1 + ph * 2.0) + 0.2 * std::sin(t * 29.7 + ph * 3.0);
                k *= 1.0f - l.flicker * 0.35f * static_cast<float>(1.0 + n);
            }
            if (static_cast<int>(i) == g_user_light_selected) k *= 0.6f + 0.4f * static_cast<float>(std::sin(t * 8.0));  // (the debug menu's: pulsing)
            g_sh.pt_pos[g_sh.npts] = glm::vec4(p, l.radius);
            g_sh.pt_col[g_sh.npts] = glm::vec4(l.col[0] * k, l.col[1] * k, l.col[2] * k, -1.0f);
            g_sh.pt_slot[g_sh.npts] = -1;
            ++g_sh.npts;
        }
        class_lights_add(t);
        beam_lights_add();
    }
    if (g_modern_points != 0 && g_auto_lights != 0) {  // the automatic emitters near the camera, the nearest first
        float pr[4 * kMaxPts], pc[4 * kMaxPts];
        const float cam[3] = {g_sh.cam.x, g_sh.cam.y, g_sh.cam.z};
        const int n = mrp_world_emitters_near(cam, g_sh.splits[1], kMaxPts - g_sh.npts, pr, pc);
        for (int k = 0; k < n; ++k, ++g_sh.npts) {
            g_sh.pt_pos[g_sh.npts] = glm::vec4(pr[4 * k], pr[4 * k + 1], pr[4 * k + 2], pr[4 * k + 3]);
            g_sh.pt_col[g_sh.npts] = glm::vec4(pc[4 * k], pc[4 * k + 1], pc[4 * k + 2], -1.0f);
            g_sh.pt_slot[g_sh.npts] = -1;
        }
    }
    if (vk::is_active()) {
        int prev_vp[4] = {};
        vk::get_viewport(prev_vp);
        if (mrp_world_casters_built()) ++g_sh.built;
        static const int skip = std::getenv("RAC1_SHADOW_SKIP") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_SKIP")) : 0;
        const float within_v[4] = {g_sh.cam.x, g_sh.cam.y, g_sh.cam.z, g_shadow_moby_dist};
        const float* within = g_shadow_moby_dist > 0.0f ? within_v : nullptr;
        for (int k = 0; k < kCascades; ++k) {
            if (!g_casc_on.load()) continue;
            const int cres = g_sh.casc[k].res;
            const MobyRects* moby_rects_now = nullptr;
            if (g_shadow_cache != 0) {
                State::Cache& ca = g_sh.cache[k];
                if (g_vk_sh.stex[k].image == VK_NULL_HANDLE || g_vk_sh.stex_res[k] != cres) {
                    vk::destroy_image(g_vk_sh.stex[k]);
                    vk::create_image(static_cast<uint32_t>(cres), static_cast<uint32_t>(cres), VK_FORMAT_D32_SFLOAT,
                                     VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT | VK_IMAGE_USAGE_TRANSFER_SRC_BIT |
                                         VK_IMAGE_USAGE_SAMPLED_BIT,
                                     VK_IMAGE_ASPECT_DEPTH_BIT, g_vk_sh.stex[k]);
                    g_vk_sh.stex_res[k] = cres;
                    ca.stex_res = cres;
                    g_sh.refresh[k] = true;
                }
                if (g_sh.refresh[k]) {
                    ca.incomplete = !mrp_world_casters_built();
                    if (ca.incomplete) g_sh.want = true;
                    vk::transition_image(vk::cmd(), g_vk_sh.stex[k], VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                    vk::RenderTarget rt_s{};
                    rt_s.depth_view = g_vk_sh.stex[k].view;
                    rt_s.depth_format = VK_FORMAT_D32_SFLOAT;
                    rt_s.width = static_cast<uint32_t>(cres);
                    rt_s.height = static_cast<uint32_t>(cres);
                    vk::set_viewport(0, 0, cres, cres);
                    vk::set_scissor(false);
                    vk::push_target(rt_s, false, nullptr, true, 1.0f, false, 0);
                    if ((skip & 1) == 0) mrp_world_draw_casters(glm::value_ptr(g_sh.casc[k].vp), k);
                    vk::pop_target();
                    ++g_sh.refreshes;
                } else if (far_skip(k)) {
                    ++g_sh.frames_cached;
                    continue;
                }
                ++g_sh.frames_cached;
                static const bool whole = std::getenv("RAC1_SHADOW_COPY_WHOLE") != nullptr;
                // the cache put back where last frame's mobys were (the whole map after a refresh), this frame's rectangles kept
                VkImageCopy ics[kMaxClusters];
                uint32_t nics = 0;
                auto region = [&](int x0, int y0, int x1, int y1) {
                    if (x1 <= x0 || y1 <= y0) return;
                    VkImageCopy& ic = ics[nics++];
                    ic = VkImageCopy{};
                    ic.srcSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, 0, 1};
                    ic.srcOffset = {x0, y0, 0};
                    ic.dstSubresource = {VK_IMAGE_ASPECT_DEPTH_BIT, 0, static_cast<uint32_t>(k), 1};
                    ic.dstOffset = {x0, y0, 0};
                    ic.extent = {static_cast<uint32_t>(x1 - x0), static_cast<uint32_t>(y1 - y0), 1};
                };
                if (g_sh.refresh[k] || whole) region(0, 0, cres, cres);
                else
                    for (int i = 0; i < ca.dirty.n; ++i) region(ca.dirty.r[i][0], ca.dirty.r[i][1], ca.dirty.r[i][2], ca.dirty.r[i][3]);
                MobyRects now;
                if ((skip & 2) != 0 || !moby_rects(glm::value_ptr(g_sh.casc[k].vp), cres, within, now)) now.n = 0;
                ca.dirty = now;
                if (!whole) moby_rects_now = &ca.dirty;
                if (nics > 0) {
                    vk::end_rendering();
                    vk::transition_image(vk::cmd(), g_vk_sh.stex[k], VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
                    vk::transition_image(vk::cmd(), g_vk_sh.tex, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL);
                    vkCmdCopyImage(vk::cmd(), g_vk_sh.stex[k].image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL,
                                   g_vk_sh.tex.image, VK_IMAGE_LAYOUT_TRANSFER_DST_OPTIMAL, nics, ics);
                }
                vk::transition_image(vk::cmd(), g_vk_sh.tex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                vk::RenderTarget rt_l{};
                rt_l.depth_view = g_vk_sh.tex_layers[k];
                rt_l.depth_format = VK_FORMAT_D32_SFLOAT;
                rt_l.width = static_cast<uint32_t>(cres);
                rt_l.height = static_cast<uint32_t>(cres);
                vk::set_viewport(0, 0, cres, cres);
                vk::set_scissor(false);
                vk::push_target(rt_l, false, nullptr, false, 1.0f, false, 0);
            } else {
                vk::transition_image(vk::cmd(), g_vk_sh.tex, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
                vk::RenderTarget rt_l{};
                rt_l.depth_view = g_vk_sh.tex_layers[k];
                rt_l.depth_format = VK_FORMAT_D32_SFLOAT;
                rt_l.width = static_cast<uint32_t>(cres);
                rt_l.height = static_cast<uint32_t>(cres);
                vk::set_viewport(0, 0, cres, cres);
                vk::set_scissor(false);
                vk::push_target(rt_l, false, nullptr, true, 1.0f, false, 0);
                if ((skip & 1) == 0) mrp_world_draw_casters(glm::value_ptr(g_sh.casc[k].vp), k);
            }
            if (moby_rects_now != nullptr) {  // (each rectangle's parts inside it: what the next frame puts back)
                for (int i = 0; i < moby_rects_now->n; ++i) {
                    const int* r = moby_rects_now->r[i];
                    if (r[2] <= r[0] || r[3] <= r[1]) continue;
                    vk::set_scissor(true, r[0], r[1], r[2] - r[0], r[3] - r[1]);
                    mrp_moby_draw_casters(glm::value_ptr(g_sh.casc[k].vp), moby_rects_now->s[i][3] >= 0.0f ? moby_rects_now->s[i] : nullptr, within);
                }
            } else if ((skip & 2) == 0) {
                mrp_moby_draw_casters(glm::value_ptr(g_sh.casc[k].vp), nullptr, within);
            }
            vk::set_scissor(false);
            vk::pop_target();
        }
        vk::transition_image(vk::cmd(), g_vk_sh.tex, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
        // (the profiler's stages as the OpenGL path's: the cascades' zone closed here - left open, the GPU's work after the world's
        // draw, outside a stage of its own, was counted as the cascades' -, the point lights' cubes their own)
        rac1::prof::end(prof_cascades);
        prof_cascades = -1;
        {
            RAC1_PROF_ZONE(kPointCubes);
            point_shadows_pass();
        }
        point_tiles_update();
        vk::set_viewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
        vk::set_scissor(false);
        ++g_vk_sh.ubo_gen;
        g_sh.on = true;
        return;
    }
    // (the read and draw framebuffers restored apart: the effects copy the screen from the read one - glCopyTexSubImage2D, gs_draw's
    // screen copies -; left at the draw one, an explosion's heat haze sampled a stale picture: a trail when the camera moved)
    GLint prev_fbo = 0, prev_read = 0, prev_vp[4] = {};
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fbo);
    glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &prev_read);
    if (std::getenv("RAC1_SHADOW_FBO_TRACE") != nullptr) {  // (testing)
        static int shown = 0;
        if (prev_read != prev_fbo && shown++ < 5) std::printf("[shadows] the read framebuffer %d is not the draw one %d\n", prev_read, prev_fbo);
    }
    glGetIntegerv(GL_VIEWPORT, prev_vp);
    const GLboolean scissor = glIsEnabled(GL_SCISSOR_TEST), clamp = glIsEnabled(GL_DEPTH_CLAMP);
    glBindFramebuffer(GL_FRAMEBUFFER, g_sh.fbo);
    glDrawBuffer(GL_NONE);
    glReadBuffer(GL_NONE);
    glDisable(GL_SCISSOR_TEST);
    glDisable(GL_BLEND);
    glDisable(GL_DEPTH_CLAMP);  // (the cut plane bounds every caster alike, see kBack)
    glEnable(GL_CLIP_DISTANCE0);
    glEnable(GL_POLYGON_OFFSET_FILL);
    glPolygonOffset(1.5f, 2.0f);
    static GLuint cq[kCascades] = {};
    static double cacc[kCascades] = {};
    static int cframes = 0;
    if (prof && cq[0] == 0) glGenQueries(kCascades, cq);
    if (mrp_world_casters_built()) ++g_sh.built;
    for (int k = 0; k < kCascades; ++k) {
        if (prof) {
            if (k == 0) glEndQuery(GL_TIME_ELAPSED);  // (the "cascades" query: up to here, the frame's world upload and dispatch)
            glBeginQuery(GL_TIME_ELAPSED, cq[k]);
        }
        if (!g_casc_on.load()) {  // (no sun, no cascades drawn - the receivers look at none)
            if (prof) glEndQuery(GL_TIME_ELAPSED);
            continue;
        }
        const int cres = g_sh.casc[k].res;
        const MobyRects* moby_rects_now = nullptr;
        const float within_v[4] = {g_sh.cam.x, g_sh.cam.y, g_sh.cam.z, g_shadow_moby_dist};  // (the shadows' distance)
        const float* within = g_shadow_moby_dist > 0.0f ? within_v : nullptr;
        static const int skip = std::getenv("RAC1_SHADOW_SKIP") != nullptr ? std::atoi(std::getenv("RAC1_SHADOW_SKIP")) : 0;  // (testing: 1 no world, 2 no mobys)
        auto begin_map = [&](bool layer) {  // (a depth target cleared: the cascade's layer, or its cache's map)
            if (layer) glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_sh.tex, 0, k);
            else glFramebufferTexture(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_sh.cache[k].stex, 0);
            glViewport(0, 0, cres, cres);
            glDepthMask(GL_TRUE);
            glClearDepth(1.0);
            glEnable(GL_SCISSOR_TEST);  // (its square alone)
            glScissor(0, 0, cres, cres);
            glClear(GL_DEPTH_BUFFER_BIT);
            glDisable(GL_SCISSOR_TEST);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        };
        if (g_shadow_cache != 0) {  // the static world from the cache (drawn into it when the box moved), the mobys over it
            State::Cache& ca = g_sh.cache[k];
            if (ca.stex == 0 || ca.stex_res != cres) {
                if (ca.stex == 0) glGenTextures(1, &ca.stex);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &ca.stex, "Shadows: static cache");
                glBindTexture(GL_TEXTURE_2D, ca.stex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, cres, cres, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
                glBindTexture(GL_TEXTURE_2D, 0);
                ca.stex_res = cres;
                g_sh.refresh[k] = true;  // (a new map: drawn this frame - the box is this frame's fit, see fit_cascades)
            }
            if (g_sh.refresh[k]) {
                ca.incomplete = !mrp_world_casters_built();
                if (ca.incomplete) g_sh.want = true;
                begin_map(false);
                if ((skip & 1) == 0) mrp_world_draw_casters(glm::value_ptr(g_sh.casc[k].vp), k);
                glDisable(GL_DEPTH_CLAMP);
                ++g_sh.refreshes;
            } else if (far_skip(k)) {
                // a far cascade: its layer (the cache and its mobys as last drawn) kept this frame - the box did not move
                ++g_sh.frames_cached;
                if (prof) glEndQuery(GL_TIME_ELAPSED);
                continue;
            }
            ++g_sh.frames_cached;
            glFramebufferTextureLayer(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, g_sh.tex, 0, k);
            // the layer: the cache copied whole when drawn, else only where last frame's mobys were (the rest is the cache still); the
            // mobys then drawn inside their own rectangle (their parts' spheres), which the next frame puts back
            static const bool whole = std::getenv("RAC1_SHADOW_COPY_WHOLE") != nullptr;  // (testing: the whole cache copied every frame)
            // (the regions put back: last frame's rectangles, or the whole map after a refresh)
            int regs[kMaxClusters][4] = {};
            int nregs = 0;
            if (g_sh.refresh[k] || whole) {
                regs[0][2] = regs[0][3] = cres;
                nregs = 1;
            } else {
                for (int i = 0; i < ca.dirty.n; ++i) std::memcpy(regs[nregs++], ca.dirty.r[i], sizeof regs[0]);
            }
            MobyRects now;
            if ((skip & 2) != 0 || !moby_rects(glm::value_ptr(g_sh.casc[k].vp), cres, within, now)) now.n = 0;
            ca.dirty = now;
            if (!whole) moby_rects_now = &ca.dirty;
            using PfnCopy = void(APIENTRYP)(GLuint, GLenum, GLint, GLint, GLint, GLint, GLuint, GLenum, GLint, GLint, GLint, GLint, GLsizei, GLsizei, GLsizei);
            static const PfnCopy copy = reinterpret_cast<PfnCopy>(glfwGetProcAddress("glCopyImageSubData"));  // (GL 4.3: not in the loader's header)
            if (copy != nullptr) {
                for (int i = 0; i < nregs; ++i) {
                    const int* r = regs[i];
                    if (r[2] > r[0] && r[3] > r[1]) copy(ca.stex, GL_TEXTURE_2D, 0, r[0], r[1], 0, g_sh.tex, GL_TEXTURE_2D_ARRAY, 0, r[0], r[1], k, r[2] - r[0], r[3] - r[1], 1);
                }
            } else {  // (no copy: the cache off from now on, this frame's world drawn as before)
                g_shadow_cache = 0;
                begin_map(true);
                if ((skip & 1) == 0) mrp_world_draw_casters(glm::value_ptr(g_sh.casc[k].vp), k);
                glDisable(GL_DEPTH_CLAMP);
            }
            glViewport(0, 0, cres, cres);
            glEnable(GL_DEPTH_TEST);
            glDepthFunc(GL_LESS);
            glDepthMask(GL_TRUE);
            glColorMask(GL_FALSE, GL_FALSE, GL_FALSE, GL_FALSE);
        } else {
            begin_map(true);
            if ((skip & 1) == 0) mrp_world_draw_casters(glm::value_ptr(g_sh.casc[k].vp), k);
            glDisable(GL_DEPTH_CLAMP);
        }
        if (moby_rects_now != nullptr) {  // (each rectangle's parts inside it: what the next frame puts back)
            glEnable(GL_SCISSOR_TEST);
            for (int i = 0; i < moby_rects_now->n; ++i) {
                const int* r = moby_rects_now->r[i];
                if (r[2] <= r[0] || r[3] <= r[1]) continue;
                glScissor(r[0], r[1], r[2] - r[0], r[3] - r[1]);
                mrp_moby_draw_casters(glm::value_ptr(g_sh.casc[k].vp), moby_rects_now->s[i][3] >= 0.0f ? moby_rects_now->s[i] : nullptr, within);
            }
        } else if ((skip & 2) == 0) {
            mrp_moby_draw_casters(glm::value_ptr(g_sh.casc[k].vp), nullptr, within);
        }
        glDisable(GL_DEPTH_CLAMP);
        glDisable(GL_SCISSOR_TEST);
        if (prof) glEndQuery(GL_TIME_ELAPSED);
    }
    if (prof) {
        for (int k = 0; k < kCascades; ++k) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(cq[k], GL_QUERY_RESULT, &ns);
            cacc[k] += static_cast<double>(ns) * 1e-6;
        }
        if (++cframes == 60) {
            std::printf("[shadows] GPU ms/frame per cascade: %.3f %.3f %.3f; the static cache drawn %llu times in %llu cascade frames, the casters outside the view "
                        "made in %llu of 60 frames\n", cacc[0] / 60, cacc[1] / 60, cacc[2] / 60, static_cast<unsigned long long>(g_sh.refreshes),
                        static_cast<unsigned long long>(g_sh.frames_cached), static_cast<unsigned long long>(g_sh.built));
            g_sh.refreshes = g_sh.frames_cached = g_sh.built = 0;
            cacc[0] = cacc[1] = cacc[2] = 0;
            cframes = 0;
        }
        glBeginQuery(GL_TIME_ELAPSED, q[0]);  // (closed below: the rest of the pass)
    }
    if (const char* ref = std::getenv("RAC1_SHADOW_REF")) {  // (testing: a world-fixed box's depth from the sun each frame, shadow_ref_<n>.pgm:
        // with the hero and the camera's target still, it changes only where the casters' set depends on the camera)
        static GLuint rtex = 0, rfbo = 0;
        static int n = 0;
        float x = 0, y = 0, z = 0, r = 25;
        int every = 1;
        if (std::sscanf(ref, "%f,%f,%f,%f,%d", &x, &y, &z, &r, &every) >= 3 && ++n % std::max(1, every) == 0) {
            constexpr int R = 1024;
            if (rtex == 0) {
                glGenTextures(1, &rtex);
                rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &rtex, "Shadows: reference (testing)");
                glBindTexture(GL_TEXTURE_2D, rtex);
                glTexImage2D(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT32F, R, R, 0, GL_DEPTH_COMPONENT, GL_FLOAT, nullptr);
                glGenFramebuffers(1, &rfbo);
                glBindFramebuffer(GL_FRAMEBUFFER, rfbo);
                glFramebufferTexture2D(GL_FRAMEBUFFER, GL_DEPTH_ATTACHMENT, GL_TEXTURE_2D, rtex, 0);
                glDrawBuffer(GL_NONE);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, rfbo);
            glViewport(0, 0, R, R);
            glClear(GL_DEPTH_BUFFER_BIT);
            const glm::vec3 c(x, y, z), up = std::fabs(g_sh.sun.z) > 0.95f ? glm::vec3(1, 0, 0) : glm::vec3(0, 0, 1);
            const glm::mat4 vp = glm::ortho(-r, r, -r, r, -(r + kBack + glm::length(c - g_sh.cam)), r) * glm::lookAt(c, c - g_sh.sun, up);
            mrp_world_draw_casters(glm::value_ptr(vp), 0);
            glDisable(GL_DEPTH_CLAMP);
            if (std::getenv("RAC1_SHADOW_REF_NOMOBY") == nullptr) mrp_moby_draw_casters(glm::value_ptr(vp));
            glDisable(GL_DEPTH_CLAMP);
            std::vector<float> px(static_cast<size_t>(R) * R);
            glBindTexture(GL_TEXTURE_2D, rtex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_DEPTH_COMPONENT, GL_FLOAT, px.data());
            glBindTexture(GL_TEXTURE_2D, 0);
            char fn[64];
            std::snprintf(fn, sizeof fn, "shadow_ref_%d.pgm", n);
            if (FILE* f = std::fopen(fn, "wb")) {
                std::fprintf(f, "P5 %d %d 255\n", R, R);
                for (int yy = R - 1; yy >= 0; --yy)
                    for (int xx = 0; xx < R; ++xx) std::fputc(static_cast<int>(std::clamp(px[static_cast<size_t>(yy) * R + xx], 0.0f, 1.0f) * 255.0f), f);
                std::fclose(f);
            }
            glBindFramebuffer(GL_FRAMEBUFFER, g_sh.fbo);
        }
    }
    glDisable(GL_CLIP_DISTANCE0);
    if (prof) {
        glEndQuery(GL_TIME_ELAPSED);
        glBeginQuery(GL_TIME_ELAPSED, q[1]);
    }
    rac1::prof::end(prof_cascades);
    prof_cascades = -1;
    {
        RAC1_PROF_ZONE(kPointCubes);
        point_shadows_pass();
    }
    if (prof) {
        glEndQuery(GL_TIME_ELAPSED);
        for (int k = 0; k < 2; ++k) {
            GLuint64 ns = 0;
            glGetQueryObjectui64v(q[k], GL_QUERY_RESULT, &ns);
            acc[k] += static_cast<double>(ns) * 1e-6;
        }
        for (int i = 0; i < g_sh.npts; ++i) cubes_acc += g_sh.pt_col[i].w >= 0.0f ? 1 : 0;
        if (++frames == 60) {
            std::printf("[shadows] GPU ms/frame: cascades %.3f, point cubes %.3f (%.2f cubes a frame)\n", acc[0] / frames, acc[1] / frames, static_cast<double>(cubes_acc) / frames);
            acc[0] = acc[1] = 0;
            frames = cubes_acc = 0;
        }
    }
    glDisable(GL_POLYGON_OFFSET_FILL);
    if (const char* dump = std::getenv("RAC1_SHADOW_DUMP")) {  // (testing: the cascades' depth at that draw, shadow_casc<k>.pgm)
        static int n = 0;
        if (++n % std::max(1, std::atoi(dump)) == 0) {
            std::vector<float> all(static_cast<size_t>(g_sh.res) * g_sh.res * kCascades);
            glBindTexture(GL_TEXTURE_2D_ARRAY, g_sh.tex);
            glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH_COMPONENT, GL_FLOAT, all.data());
            glBindTexture(GL_TEXTURE_2D_ARRAY, 0);
            {  // (testing: where cascade 0's nearest texels are in the world, and the camera)
                const float* px = all.data();
                const glm::mat4 inv = glm::inverse(g_sh.casc[0].vp);
                std::printf("[shadows] dump %d: camera %.1f %.1f %.1f\n", n, g_sh.cam.x, g_sh.cam.y, g_sh.cam.z);
                for (int gy = 0; gy < 8; ++gy)
                    for (int gx = 0; gx < 8; ++gx) {
                        float best = 2.0f;
                        int bx = 0, by = 0;
                        for (int y = gy * g_sh.res / 8; y < (gy + 1) * g_sh.res / 8; y += 4)
                            for (int x = gx * g_sh.res / 8; x < (gx + 1) * g_sh.res / 8; x += 4)
                                if (px[static_cast<size_t>(y) * g_sh.res + x] < best) { best = px[static_cast<size_t>(y) * g_sh.res + x]; bx = x; by = y; }
                        if (best >= 1.0f) continue;
                        const glm::vec4 w = inv * glm::vec4((bx + 0.5f) / g_sh.res * 2 - 1, (by + 0.5f) / g_sh.res * 2 - 1, best * 2 - 1, 1);
                        std::printf("[shadows]   cell %d,%d nearest depth %.3f world %.1f %.1f %.1f\n", gx, gy, best, w.x / w.w, w.y / w.w, w.z / w.w);
                    }
            }
            for (int k = 0; k < kCascades; ++k) {
                const float* px = all.data() + static_cast<size_t>(k) * g_sh.res * g_sh.res;
                char fn[64];
                std::snprintf(fn, sizeof fn, "shadow_%d_casc%d.pgm", n, k);
                if (FILE* f = std::fopen(fn, "wb")) {
                    std::fprintf(f, "P5 %d %d 255\n", g_sh.res, g_sh.res);
                    for (int y = g_sh.res - 1; y >= 0; --y)
                        for (int x = 0; x < g_sh.res; ++x) std::fputc(static_cast<int>(std::clamp(px[static_cast<size_t>(y) * g_sh.res + x], 0.0f, 1.0f) * 255.0f), f);
                    std::fclose(f);
                }
            }
        }
    }
    glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
    glDepthMask(GL_TRUE);
    glDepthFunc(GL_LEQUAL);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prev_fbo));
    glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(prev_read));
    glViewport(prev_vp[0], prev_vp[1], prev_vp[2], prev_vp[3]);
    if (scissor) glEnable(GL_SCISSOR_TEST);
    if (clamp) glEnable(GL_DEPTH_CLAMP);
    else glDisable(GL_DEPTH_CLAMP);
    glActiveTexture(GL_TEXTURE0 + kUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_sh.tex);
    if (g_sh.ptex != 0) {
        glActiveTexture(GL_TEXTURE0 + kPtUnit);
        glBindTexture(kGlCubeMapArray, g_sh.ptex);
    }
    point_tiles_update();
    glActiveTexture(GL_TEXTURE0 + kTileUnit);
    glBindTexture(GL_TEXTURE_2D, g_sh.tiles);
    glActiveTexture(GL_TEXTURE0 + kSsrColUnit);  // (the materials' last frame's 3D: read only when it is ready, uMat[0].w)
    glBindTexture(GL_TEXTURE_2D, g_mssr.tex_c);
    glActiveTexture(GL_TEXTURE0 + kSsrDepthUnit);
    glBindTexture(GL_TEXTURE_2D, g_mssr.tex_d);
    glActiveTexture(GL_TEXTURE0);
    g_sh.on = true;
}

// orig: (host) see mrp_lighting.h
ShadowLocs mrp_lighting_locs(unsigned prog) {
    const GLuint p = static_cast<GLuint>(prog);
    ShadowLocs l;
    l.pass = glGetUniformLocation(p, "uShPass");
    l.on = glGetUniformLocation(p, "uShOn");
    l.inv_view = glGetUniformLocation(p, "uShInvView");
    l.proj = glGetUniformLocation(p, "uShProj");
    l.light = glGetUniformLocation(p, "uShLight");
    l.cut = glGetUniformLocation(p, "uShCut");
    l.map = glGetUniformLocation(p, "uShMap");
    l.casc = glGetUniformLocation(p, "uShCasc");
    l.cam = glGetUniformLocation(p, "uShCam");
    l.sun = glGetUniformLocation(p, "uShSun");
    l.param = glGetUniformLocation(p, "uShParam");
    l.texel = glGetUniformLocation(p, "uShTexel");
    l.part = glGetUniformLocation(p, "uShPart");
    l.sun_k = glGetUniformLocation(p, "uShSunK");
    l.key = glGetUniformLocation(p, "uShKey");
    l.key_dir = glGetUniformLocation(p, "uShKeyDir");
    l.strength = glGetUniformLocation(p, "uShStrength");
    l.debug = glGetUniformLocation(p, "uShDebug");
    l.pt_n = glGetUniformLocation(p, "uPtN");
    l.pt_pos = glGetUniformLocation(p, "uPtPos");
    l.pt_col = glGetUniformLocation(p, "uPtCol");
    l.pt_down = glGetUniformLocation(p, "uPtDown");
    l.pt_map = glGetUniformLocation(p, "uPtMap");
    l.pt_near = glGetUniformLocation(p, "uPtNear");
    l.opaque = glGetUniformLocation(p, "uShOpaque");
    l.pt_tiles = glGetUniformLocation(p, "uPtTiles");
    l.pt_vp = glGetUniformLocation(p, "uPtVP");
    l.emis = glGetUniformLocation(p, "uEmis");
    l.mat = glGetUniformLocation(p, "uMat");
    l.ssr_vp = glGetUniformLocation(p, "uSsrVP");
    l.ssr_z = glGetUniformLocation(p, "uSsrZ");
    l.ssr_sky = glGetUniformLocation(p, "uSsrSky");
    l.ssr_col = glGetUniformLocation(p, "uSsrCol");
    l.ssr_depth = glGetUniformLocation(p, "uSsrDepth");
    return l;
}

namespace {
void bind_common(const ShadowLocs& l) {
    glUniformMatrix4fv(l.inv_view, 1, GL_FALSE, glm::value_ptr(g_sh.inv_view));
    glUniform2f(l.proj, g_sh.proj.x, g_sh.proj.y);
}
// the lighting's samplers on their own units, whatever the draw (a lit receiver, the sky, a caster, the lighting off): left at
// their default unit 0, the point lights' cube maps (samplerCubeArrayShadow) and the tiles (usampler2D) would share it with uTexs[0]
// (sampler2DArray) and every draw of the program would fail (GL_INVALID_OPERATION). A program's uniforms keep their values: once
// a lit receiver had set them, all was well - but the loading screen's flight from the title's Load Game, the first frames the lighting
// runs in, draws the sky alone (an empty world): its sky was never drawn
void bind_units(const ShadowLocs& l) {
    glUniform1i(l.map, kUnit);
    glUniform1i(l.pt_map, kPtUnit);
    glUniform1i(l.pt_tiles, kTileUnit);
    glUniform1i(l.ssr_col, kSsrColUnit);  // (the lighting's variants only: -1 elsewhere)
    glUniform1i(l.ssr_depth, kSsrDepthUnit);
}
}  // namespace

// orig: (host) see mrp_lighting.h
void mrp_lighting_bind_receiver(const ShadowLocs& l, bool sky) {
    const bool on = mrp_lighting_active() && !sky;
    glUniform1i(l.pass, 0);
    glUniform1i(l.on, on ? 1 : 0);
    bind_units(l);
    if (!on) return;
    bind_common(l);
    glm::mat4 m[kCascades];
    for (int k = 0; k < kCascades; ++k) m[k] = g_sh.casc[k].tex;
    glUniformMatrix4fv(l.casc, kCascades, GL_FALSE, glm::value_ptr(m[0]));
    glUniform3f(l.cam, g_sh.cam.x, g_sh.cam.y, g_sh.cam.z);
    glUniform3f(l.sun, g_sh.sun.x, g_sh.sun.y, g_sh.sun.z);
    // (x: the share fallback's strength - the lighting's variants apply the strength to the visibility, uShStrength)
    glUniform4f(l.param, l.strength >= 0 ? 1.0f : static_cast<float>(g_shadow_strength) / 100.0f, g_has_sun.load() ? static_cast<float>(kCascades) : 0.0f, 0.08f, 1.0f);
    glUniform3f(l.texel, g_sh.casc[0].texel, g_sh.casc[1].texel, g_sh.casc[2].texel);
    glUniform3f(l.part, g_sh.casc[0].part, g_sh.casc[1].part, g_sh.casc[2].part);
    if (l.sun_k >= 0) {  // (only the lighting's variants have them)
        glUniform1f(l.sun_k, g_sh.sun_k);
        glUniform1f(l.strength, static_cast<float>(g_shadow_strength) / 100.0f);
        glUniform3f(l.key_dir, g_sh.key_dir.x, g_sh.key_dir.y, g_sh.key_dir.z);
        if (l.key >= 0) {  // (RAC1_WORLD_SHADOW=share, testing / no key light known: the old estimate, the strength's share of the colour)
            static const bool share = std::getenv("RAC1_WORLD_SHADOW") != nullptr && std::strcmp(std::getenv("RAC1_WORLD_SHADOW"), "share") == 0;
            const glm::vec4& k = g_sh.key;
            if (share || !(k.x + k.y + k.z > 0.0f)) glUniform4f(l.key, 0.0f, 0.0f, 0.0f, -1.0f);
            else glUniform4f(l.key, k.x * (128.0f / 255.0f), k.y * (128.0f / 255.0f), k.z * (128.0f / 255.0f), k.w);
        }
    }
    glUniform1i(l.debug, g_shadow_debug);
    glUniform1i(l.pt_n, g_modern_points != 0 ? g_sh.npts : -1);
    glUniform1f(l.pt_near, kPtNear);
    glUniformMatrix4fv(l.pt_vp, 1, GL_FALSE, glm::value_ptr(g_sh.view_proj));
    if (g_sh.npts > 0) {
        glUniform4fv(l.pt_pos, g_sh.npts, glm::value_ptr(g_sh.pt_pos[0]));
        glUniform4fv(l.pt_col, g_sh.npts, glm::value_ptr(g_sh.pt_col[0]));
        glUniform4fv(l.pt_down, (g_sh.npts + 3) / 4, g_sh.pt_down);
    }
    if (l.emis >= 0) {  // (the lighting's variants only)
        glm::vec4 e[kEmissivePreviewSlot + 1];
        emissive_uniforms(e);
        glUniform4fv(l.emis, kEmissivePreviewSlot + 1, glm::value_ptr(e[0]));
    }
    if (l.mat >= 0) {
        MatUniforms u;
        material_uniforms(u);
        glUniform4fv(l.mat, kEmissivePreviewSlot + 1, glm::value_ptr(u.mat[0]));
        glUniformMatrix4fv(l.ssr_vp, 1, GL_FALSE, glm::value_ptr(u.ssr_vp));
        glUniform4fv(l.ssr_z, 1, glm::value_ptr(u.ssr_z));
        glUniform4fv(l.ssr_sky, 1, glm::value_ptr(u.sky));
    }
}

// orig: (host) see mrp_lighting.h
void mrp_lighting_bind_caster(const ShadowLocs& l, const float* light_vp) {
    glUniform1i(l.pass, 1);
    glUniform1i(l.on, 0);
    bind_units(l);
    bind_common(l);
    glUniformMatrix4fv(l.light, 1, GL_FALSE, light_vp);
    glUniform4f(l.cut, g_sh.sun.x, g_sh.sun.y, g_sh.sun.z, kBack + glm::dot(g_sh.cam, g_sh.sun));
}

// orig: (host) see mrp_lighting.h
void mrp_lighting_bind_off(const ShadowLocs& l) {
    glUniform1i(l.pass, 0);
    glUniform1i(l.on, 0);
    glUniform1i(l.pt_n, -1);
    bind_units(l);
}

namespace {
struct ShadowVkUbo {
    glm::mat4 uShInvView{1.0f};
    glm::mat4 uShLight{1.0f};
    glm::mat4 uShCasc[3]{glm::mat4(1.0f), glm::mat4(1.0f), glm::mat4(1.0f)};
    glm::mat4 uPtVP{1.0f};
    glm::vec4 uShCut{0.0f};
    glm::vec4 uShCam_Pass{0.0f};
    glm::vec4 uShSun_On{0.0f, 0.0f, 1.0f, 0.0f};
    glm::vec4 uShParam{0.0f};
    glm::vec4 uShTexel_SunK{0.0f, 0.0f, 0.0f, 1.0f};
    glm::vec4 uShKey{0.0f, 0.0f, 0.0f, -1.0f};
    glm::vec4 uShKeyDir_Str{0.0f, 0.0f, 1.0f, 0.0f};
    glm::vec4 uShPart_Near{1.0f, 1.0f, 1.0f, kPtNear};
    glm::vec2 uShProj{1.0f, 1.0f};
    int32_t uShDebug = 0;
    int32_t uPtN = -1;
    int32_t uShOpaque = 0;
    int32_t _shPad0 = 0, _shPad1 = 0, _shPad2 = 0;
    glm::vec4 uPtPos[96]{};
    glm::vec4 uPtCol[96]{};
    glm::vec4 uPtDown[24]{};
    glm::vec4 uEmis[kEmissivePreviewSlot + 1]{};
    glm::vec4 uMat[kEmissivePreviewSlot + 1]{};
    glm::mat4 uSsrVP{1.0f};
    glm::vec4 uSsrZ{0.0f};
    glm::vec4 uSsrSky{0.0f};
};
static_assert(sizeof(ShadowVkUbo) == 8192, "ShadowVkUbo must match SHADOW_UBO_DECL in gen_vk_shaders.py");

VkDescriptorSet build_lighting_vk_ds(const ShadowVkUbo& ubo, bool is_caster) {
    mrp_lighting_vk_set_layout();
    vk::RingAlloc r = vk::alloc_ring(sizeof(ShadowVkUbo), 256);
    if (r.ptr != nullptr) std::memcpy(r.ptr, &ubo, sizeof(ShadowVkUbo));

    VkDescriptorSet ds = vk::alloc_descriptor_set(g_vk_sh.set_layout);
    VkDescriptorBufferInfo dbi{r.buffer, r.offset, sizeof(ShadowVkUbo)};

    const VkSampler cmp_sampler = vk::get_sampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR,
                                                  VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE,
                                                  true, VK_COMPARE_OP_LESS_OR_EQUAL);
    const VkSampler nearest_sampler = vk::get_sampler(VK_FILTER_NEAREST, VK_FILTER_NEAREST,
                                                      VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);

    const vk::GpuImage& sh_map = (!is_caster && g_vk_sh.tex.image != VK_NULL_HANDLE &&
                                  g_vk_sh.tex.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                     ? g_vk_sh.tex
                                     : vk::dummy_depth_array();
    const bool ssr = !is_caster && g_mssr.vk_c.image != VK_NULL_HANDLE && g_mssr.vk_c.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL &&
                     g_mssr.vk_d.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL;
    const vk::GpuImage& ssr_col = ssr ? g_mssr.vk_c : vk::white_texture();  // (binding 2: the materials' last frame's 3D)
    const vk::GpuImage& ssr_depth = ssr ? g_mssr.vk_d : vk::white_texture();
    const VkSampler linear_sampler = vk::get_sampler(VK_FILTER_LINEAR, VK_FILTER_LINEAR, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE, VK_SAMPLER_ADDRESS_MODE_CLAMP_TO_EDGE);
    const vk::GpuImage& pt_map = (!is_caster && g_vk_sh.ptex.image != VK_NULL_HANDLE &&
                                  g_vk_sh.ptex.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                     ? g_vk_sh.ptex
                                     : vk::dummy_depth_cube_array();
    const vk::GpuImage& tiles_tex = (!is_caster && g_vk_sh.tiles.image != VK_NULL_HANDLE &&
                                     g_vk_sh.tiles.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL)
                                        ? g_vk_sh.tiles
                                        : vk::black_uint_texture();

    const vk::GpuImage* sky = is_caster ? nullptr : mrp_particles_sky_vk();  // (binding 6: the sky grid's window, the rain rings')
    const vk::GpuImage& sky_tex = sky != nullptr ? *sky : vk::white_texture();

    VkDescriptorImageInfo diis[6] = {
        {cmp_sampler, sh_map.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {linear_sampler, ssr_col.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {cmp_sampler, pt_map.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {nearest_sampler, tiles_tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {nearest_sampler, ssr_depth.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
        {nearest_sampler, sky_tex.view, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL},
    };

    VkWriteDescriptorSet writes[7]{};
    writes[0].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
    writes[0].dstSet = ds;
    writes[0].dstBinding = 0;
    writes[0].descriptorCount = 1;
    writes[0].descriptorType = VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER;
    writes[0].pBufferInfo = &dbi;
    for (uint32_t i = 0; i < 6; ++i) {
        writes[1 + i].sType = VK_STRUCTURE_TYPE_WRITE_DESCRIPTOR_SET;
        writes[1 + i].dstSet = ds;
        writes[1 + i].dstBinding = 1 + i;
        writes[1 + i].descriptorCount = 1;
        writes[1 + i].descriptorType = VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER;
        writes[1 + i].pImageInfo = &diis[i];
    }
    vkUpdateDescriptorSets(vk::device(), 7, writes, 0, nullptr);
    return ds;
}
}  // namespace

uint64_t mrp_lighting_vk_set_layout() {
    ensure_vk_lighting_init();
    if (g_vk_sh.set_layout == VK_NULL_HANDLE && vk::device() != VK_NULL_HANDLE) {
        VkDescriptorSetLayoutBinding bindings[7]{};
        bindings[0] = {0, VK_DESCRIPTOR_TYPE_UNIFORM_BUFFER, 1, VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        for (uint32_t i = 1; i <= 6; ++i) {  // (1 the sun's cascades, 2 the materials' last 3D, 3 the cubes, 4 the tiles, 5 its depth, 6 the sky grid)
            bindings[i] = {i, VK_DESCRIPTOR_TYPE_COMBINED_IMAGE_SAMPLER, 1,
                           VK_SHADER_STAGE_VERTEX_BIT | VK_SHADER_STAGE_FRAGMENT_BIT, nullptr};
        }
        VkDescriptorSetLayoutCreateInfo dslci{};
        dslci.sType = VK_STRUCTURE_TYPE_DESCRIPTOR_SET_LAYOUT_CREATE_INFO;
        dslci.bindingCount = 7;
        dslci.pBindings = bindings;
        vkCreateDescriptorSetLayout(vk::device(), &dslci, nullptr, &g_vk_sh.set_layout);
    }
    return reinterpret_cast<uint64_t>(g_vk_sh.set_layout);
}

void mrp_lighting_vk_bind_off(uint64_t pipe_layout) {
    const uint64_t gen = (vk::frame_number() << 32) | (g_vk_sh.ubo_gen & 0xffffffffu);
    if (g_vk_sh.off_gen != gen || g_vk_sh.off_ds == VK_NULL_HANDLE) {
        ShadowVkUbo ubo{};
        g_vk_sh.off_ds = build_lighting_vk_ds(ubo, false);
        g_vk_sh.off_gen = gen;
    }
    vkCmdBindDescriptorSets(vk::cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            reinterpret_cast<VkPipelineLayout>(pipe_layout), 1, 1, &g_vk_sh.off_ds, 0, nullptr);
}

void mrp_lighting_vk_bind_receiver(uint64_t pipe_layout, bool sky, bool lit_variant) {
    const bool on = mrp_lighting_active() && !sky;
    if (!on) {
        mrp_lighting_vk_bind_off(pipe_layout);
        return;
    }
    const uint64_t gen = (vk::frame_number() << 32) | (g_vk_sh.ubo_gen & 0xffffffffu);
    const int li = lit_variant ? 1 : 0;
    if (g_vk_sh.rec_gen[1][li] != gen || g_vk_sh.rec_ds[1][li] == VK_NULL_HANDLE) {
        ShadowVkUbo ubo{};
        ubo.uShInvView = g_sh.inv_view;
        ubo.uShProj = g_sh.proj;
        for (int k = 0; k < kCascades; ++k) ubo.uShCasc[k] = g_sh.casc[k].tex;
        ubo.uShCam_Pass = glm::vec4(g_sh.cam, 0.0f);
        ubo.uShSun_On = glm::vec4(g_sh.sun, 1.0f);
        ubo.uShParam = glm::vec4(lit_variant ? 1.0f : static_cast<float>(g_shadow_strength) / 100.0f,
                                 g_has_sun.load() ? static_cast<float>(kCascades) : 0.0f, 0.08f, 1.0f);
        ubo.uShTexel_SunK = glm::vec4(g_sh.casc[0].texel, g_sh.casc[1].texel, g_sh.casc[2].texel, g_sh.sun_k);
        ubo.uShPart_Near = glm::vec4(g_sh.casc[0].part, g_sh.casc[1].part, g_sh.casc[2].part, kPtNear);
        ubo.uShKeyDir_Str = glm::vec4(g_sh.key_dir, static_cast<float>(g_shadow_strength) / 100.0f);
        static const bool share = std::getenv("RAC1_WORLD_SHADOW") != nullptr &&
                                  std::strcmp(std::getenv("RAC1_WORLD_SHADOW"), "share") == 0;
        const glm::vec4& k = g_sh.key;
        if (share || !(k.x + k.y + k.z > 0.0f)) ubo.uShKey = glm::vec4(0.0f, 0.0f, 0.0f, -1.0f);
        else ubo.uShKey = glm::vec4(k.x * (128.0f / 255.0f), k.y * (128.0f / 255.0f), k.z * (128.0f / 255.0f), k.w);
        ubo.uShDebug = g_shadow_debug;
        ubo.uPtN = g_modern_points != 0 ? g_sh.npts : -1;
        ubo.uPtVP = g_sh.view_proj;
        if (g_sh.npts > 0) {
            std::memcpy(ubo.uPtPos, g_sh.pt_pos, static_cast<size_t>(g_sh.npts) * sizeof(glm::vec4));
            std::memcpy(ubo.uPtCol, g_sh.pt_col, static_cast<size_t>(g_sh.npts) * sizeof(glm::vec4));
            std::memcpy(ubo.uPtDown, g_sh.pt_down, static_cast<size_t>((g_sh.npts + 3) / 4) * sizeof(glm::vec4));
        }
        emissive_uniforms(ubo.uEmis);
        MatUniforms mu;
        material_uniforms(mu);
        std::memcpy(ubo.uMat, mu.mat, sizeof ubo.uMat);
        ubo.uSsrVP = mu.ssr_vp;
        ubo.uSsrZ = mu.ssr_z;
        ubo.uSsrSky = mu.sky;
        g_vk_sh.rec_ds[1][li] = build_lighting_vk_ds(ubo, false);
        g_vk_sh.rec_gen[1][li] = gen;
    }
    vkCmdBindDescriptorSets(vk::cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            reinterpret_cast<VkPipelineLayout>(pipe_layout), 1, 1, &g_vk_sh.rec_ds[1][li], 0, nullptr);
}

void mrp_lighting_vk_bind_caster(uint64_t pipe_layout, const float* light_vp, bool opaque) {
    const uint64_t gen = (vk::frame_number() << 32) | (g_vk_sh.ubo_gen & 0xffffffffu);
    if (g_vk_sh.cast_gen != gen || g_vk_sh.cast_ds == VK_NULL_HANDLE || g_vk_sh.cast_opaque != opaque ||
        std::memcmp(g_vk_sh.cast_vp, light_vp, 64) != 0) {
        ShadowVkUbo ubo{};
        ubo.uShInvView = g_sh.inv_view;
        ubo.uShProj = g_sh.proj;
        ubo.uShLight = glm::make_mat4(light_vp);
        ubo.uShCut = glm::vec4(g_sh.sun, kBack + glm::dot(g_sh.cam, g_sh.sun));
        ubo.uShCam_Pass = glm::vec4(g_sh.cam, 1.0f);
        ubo.uShSun_On = glm::vec4(g_sh.sun, 0.0f);
        ubo.uShOpaque = opaque ? 1 : 0;
        g_vk_sh.cast_ds = build_lighting_vk_ds(ubo, true);
        g_vk_sh.cast_gen = gen;
        g_vk_sh.cast_opaque = opaque;
        std::memcpy(g_vk_sh.cast_vp, light_vp, 64);
    }
    vkCmdBindDescriptorSets(vk::cmd(), VK_PIPELINE_BIND_POINT_GRAPHICS,
                            reinterpret_cast<VkPipelineLayout>(pipe_layout), 1, 1, &g_vk_sh.cast_ds, 0, nullptr);
}

}  // namespace rac1
