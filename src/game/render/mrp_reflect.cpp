// orig: (host) Options > Graphics > Water Reflections (see mrp_reflect.h; docs/materials.md#water-reflections)
#include <game/render/mrp_reflect.h>
#include <game/render/mrp_rt.h>

#include <game/render/mrp_lighting.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/render_view.h>
#include <profiler/host_profiler.h>
#include <runtime/vk/vk_context.h>

#include <glad/glad.h>

#include <glm/glm.hpp>
#include <glm/gtc/matrix_transform.hpp>
#include <glm/gtc/type_ptr.hpp>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <vector>

namespace rac1 {

// orig: (host) see mrp_reflect.h
int g_water_reflections = [] {
    const char* e = std::getenv("RAC1_WATER_REFLECTIONS");
    return e != nullptr ? std::clamp(std::atoi(e), 0, 3) : 2;
}();

// orig: (host) see mrp_reflect.h
int g_reflect_scale = [] {
    const char* e = std::getenv("RAC1_REFLECT_SCALE");
    return e != nullptr ? std::clamp(std::atoi(e), 1, 8) : 2;
}();

namespace {
constexpr int kPlanarUnit = 22, kCubeUnit = 23;  // (OpenGL: the materials' texture units, mrp_materials_glsl.inc)
constexpr int kCubeRes = 256;                    // (a face's size)
constexpr float kCutBelow = 0.02f;               // (the planar view keeps what lies above the water less this: the waterline closed)
constexpr float kBoxMargin = 0.12f;              // (the planar view's rectangle around the water's, in the clip square's units)
// The cube maps (Low; Medium's fallback): fixed probes, one per zone of the level's ground plan (a square of kZone units, the world's own
// grid) and body of water (its held-still height), kProbeAbove over the water at the zone's middle - where the camera happens to be
// plays no part in where a probe is. The zone the camera is in (left kZoneHyst units past its edges before another is taken) picks
// the probe; kSlots probes are kept (the least recently used one made anew), so walking back into a zone makes nothing again.
// A probe holds the sky and the scenery past kNear units of it alone (a cube of that half-size around it left out: the near plane of
// each face) - posts, crates, the dock, Ratchet's surroundings never in it - and the water reads it along its reflected ray met with
// a sphere of kProxy units around it (the scenery stays put on the water as the camera moves).
constexpr int kSlots = 8;
constexpr int kSkyLayer = 6 * kSlots, kLayers = 6 * kSlots + 6;  // (the sky's own six faces after the probes': the sky lies at no distance)
constexpr float kZone = 128.0f, kZoneHyst = 16.0f, kProbeAbove = 6.0f, kNear = 48.0f, kProxy = 160.0f;
constexpr float kBodyApart = 3.0f;  // (another body of water: its height this far from the probe's)
// (the frame holds what the game's view sees alone: a probe made at a zone's first frame is filled in, never drawn over - only where it
// holds nothing yet, its depth kept - when the view has turned this far, in radians, or moved this far since, kFillGap passes apart at
// least)
constexpr float kFillTurn = 0.7f, kFillMove = 25.0f;
constexpr int kFillGap = 20;

struct Note {
    float h, area;
    bool flat;
    float box[4];
};

struct State {
    std::mutex m;
    std::vector<Note> notes;  // (this frame's, build_vu7: read by the next frame's pass)
    uint64_t frame = 0;       // (the passes: one a frame the world is drawn)
    uint64_t water_frame = 0;  // (the last frame with water seen)
    // the planar view
    bool planar_on = false;  // (drawn this frame)
    float plane_h = 0.0f;
    int pw = 0, ph = 0;
    GLuint fbo = 0, tex = 0, depth = 0;
    vk::GpuImage vk_c{}, vk_d{};
    // the cube maps: the probes (slot k: layers 6 k .. 6 k + 5 of the colour and the depth)
    struct Probe {
        bool used = false;
        int ix = 0, iy = 0;
        float h = 0.0f;
        glm::vec3 at{0.0f};
        uint64_t last = 0, fill_frame = 0;  // (the pass it was last read / drawn into)
        int fills = 0;
        glm::vec3 fill_cam{0.0f}, fill_fwd{0.0f, 1.0f, 0.0f};  // (the view of its last drawing)
    } probes[kSlots];
    int active = -1;  // (this frame's probe)
    bool cube_ok = false;
    GLuint cfbo = 0, ctex = 0, cdepth = 0;
    vk::GpuImage vk_cube{}, vk_cdepth{};
    VkImageView vk_faces[kLayers] = {}, vk_dfaces[kLayers] = {};
    bool sky_made = false;  // (the sky's faces drawn once since the level began)
    // the report
    uint64_t n_planar = 0, n_cube = 0, n_fill = 0, n_none = 0;
    double area = 0.0;  // (the planar views' drawn share of their target, summed)
    float last_box[4] = {2.0f, 2.0f, -2.0f, -2.0f};  // (the plane's water's rectangle the frame before)
    std::map<int, uint64_t> planes;  // (height x 4: frames)
    bool reg = false;
} g_r;

// orig: (host) Vulkan: the images freed with the device
void vk_shutdown() {
    const VkDevice dev = vk::device();
    for (VkImageView* vs : {g_r.vk_faces, g_r.vk_dfaces})
        for (int i = 0; i < kLayers; ++i) {
            if (vs[i] != VK_NULL_HANDLE && dev != VK_NULL_HANDLE) vkDestroyImageView(dev, vs[i], nullptr);
            vs[i] = VK_NULL_HANDLE;
        }
    vk::destroy_image_now(g_r.vk_c);
    vk::destroy_image_now(g_r.vk_d);
    vk::destroy_image_now(g_r.vk_cube);
    vk::destroy_image_now(g_r.vk_cdepth);
}

// orig: (host) the planar view's target (its colour and a depth-and-stencil as the 3D's, for the same pipelines), `w` x `h`
bool planar_target(int w, int h) {
    if (vk::is_active()) {
        if (!g_r.reg) {
            g_r.reg = true;
            vk::register_shutdown_callback(vk_shutdown);
        }
        if (g_r.vk_c.image != VK_NULL_HANDLE && g_r.pw == w && g_r.ph == h) return true;
        if (g_r.vk_c.image != VK_NULL_HANDLE) vk::wait_idle();
        vk::destroy_image_now(g_r.vk_c);
        vk::destroy_image_now(g_r.vk_d);
        vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), vk::color_format(),
                         VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT, g_r.vk_c);
        vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), vk::depth_stencil_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g_r.vk_d);
        g_r.pw = w;
        g_r.ph = h;
        return g_r.vk_c.image != VK_NULL_HANDLE && g_r.vk_d.image != VK_NULL_HANDLE;
    }
    if (g_r.fbo != 0 && g_r.pw == w && g_r.ph == h) return true;
    if (g_r.fbo == 0) {
        glGenFramebuffers(1, &g_r.fbo);
        glGenTextures(1, &g_r.tex);
        glGenRenderbuffers(1, &g_r.depth);
        prof::gl_track_n(prof::kGlTexture, 1, &g_r.tex, "Water reflections: the planar view");
        prof::gl_track_n(prof::kGlRenderbuffer, 1, &g_r.depth, "Water reflections: the planar view");
    }
    GLint prev_t = 0, prev_rb = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev_t);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_rb);
    glBindTexture(GL_TEXTURE_2D, g_r.tex);
    glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev_t));
    glBindRenderbuffer(GL_RENDERBUFFER, g_r.depth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(prev_rb));
    GLint prev_fb = 0;
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &prev_fb);
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_r.fbo);
    glFramebufferTexture2D(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, g_r.tex, 0);
    glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_r.depth);
    const bool ok = glCheckFramebufferStatus(GL_DRAW_FRAMEBUFFER) == GL_FRAMEBUFFER_COMPLETE;
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(prev_fb));
    g_r.pw = w;
    g_r.ph = h;
    if (!ok) std::fprintf(stderr, "[reflect] the planar view's framebuffer is not complete\n");
    return ok;
}

// orig: (host) the cube maps' target: kSlots probes of six faces (layers) of kCubeRes and the sky's six, colour and depth-and-stencil
// (kept between drawings: a probe filled in where it holds nothing yet)
bool cube_target() {
    if (vk::is_active()) {
        if (!g_r.reg) {
            g_r.reg = true;
            vk::register_shutdown_callback(vk_shutdown);
        }
        if (g_r.vk_cube.image != VK_NULL_HANDLE) return g_r.vk_dfaces[kLayers - 1] != VK_NULL_HANDLE;
        vk::create_image(kCubeRes, kCubeRes, vk::color_format(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                         g_r.vk_cube, kLayers, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        vk::create_image(kCubeRes, kCubeRes, vk::depth_stencil_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g_r.vk_cdepth, kLayers, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        for (uint32_t f = 0; f < kLayers && g_r.vk_cube.image != VK_NULL_HANDLE && g_r.vk_cdepth.image != VK_NULL_HANDLE; ++f) {
            VkImageViewCreateInfo vci{};
            vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vci.image = g_r.vk_cube.image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = g_r.vk_cube.format;
            vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, f, 1};
            vkCreateImageView(vk::device(), &vci, nullptr, &g_r.vk_faces[f]);
            vci.image = g_r.vk_cdepth.image;
            vci.format = g_r.vk_cdepth.format;
            vci.subresourceRange = {VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, 0, 1, f, 1};
            vkCreateImageView(vk::device(), &vci, nullptr, &g_r.vk_dfaces[f]);
        }
        return g_r.vk_cube.image != VK_NULL_HANDLE && g_r.vk_cdepth.image != VK_NULL_HANDLE && g_r.vk_dfaces[kLayers - 1] != VK_NULL_HANDLE;
    }
    if (g_r.cfbo != 0) return true;
    glGenFramebuffers(1, &g_r.cfbo);
    glGenTextures(1, &g_r.ctex);
    glGenTextures(1, &g_r.cdepth);
    prof::gl_track_n(prof::kGlTexture, 1, &g_r.ctex, "Water reflections: the cube maps");
    prof::gl_track_n(prof::kGlTexture, 1, &g_r.cdepth, "Water reflections: the cube maps' depth");
    GLint prev_t = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev_t);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_r.ctex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kCubeRes, kCubeRes, kLayers, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_r.cdepth);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_DEPTH24_STENCIL8, kCubeRes, kCubeRes, kLayers, 0, GL_DEPTH_STENCIL, GL_UNSIGNED_INT_24_8, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_NEAREST);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_NEAREST);
    glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(prev_t));
    return true;
}

// orig: (host) the projection of the frame's view (world_view_proj's) with another field, a square of 90 degrees, and the near plane `n`
glm::mat4 face_proj(float n) {
    const float f = kRenderFar;
    glm::mat4 p(0.0f);
    p[0][0] = 1.0f;
    p[1][1] = 1.0f;
    p[2][2] = -(f + n) / (f - n);
    p[3][2] = -(2.0f * f * n) / (f - n);
    p[2][3] = -1.0f;
    return p;
}
// orig: (host) the cube map's face `f`: its axis and up (as sm_refl_cube reads them)
void face_axes(int f, glm::vec3& F, glm::vec3& U) {
    static const glm::vec3 kF[6] = {{1, 0, 0}, {-1, 0, 0}, {0, 1, 0}, {0, -1, 0}, {0, 0, 1}, {0, 0, -1}};
    F = kF[f];
    U = f < 4 ? glm::vec3(0, 0, 1) : glm::vec3(0, 1, 0);
}

// The target the passes draw into, and back
struct Saved {
    GLint fb = 0, vp[4] = {};
    GLboolean scissor = GL_FALSE, clip0 = GL_FALSE;
    int vk_vp[4] = {};
};
Saved save_gl() {
    Saved s;
    if (vk::is_active()) {
        vk::get_viewport(s.vk_vp);
        return s;
    }
    glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &s.fb);
    glGetIntegerv(GL_VIEWPORT, s.vp);
    s.scissor = glIsEnabled(GL_SCISSOR_TEST);
    s.clip0 = glIsEnabled(GL_CLIP_DISTANCE0);
    return s;
}
void restore(const Saved& s) {
    if (vk::is_active()) {
        vk::set_viewport(s.vk_vp[0], s.vk_vp[1], s.vk_vp[2], s.vk_vp[3]);
        return;
    }
    glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(s.fb));
    glViewport(s.vp[0], s.vp[1], s.vp[2], s.vp[3]);
    if (s.scissor) glEnable(GL_SCISSOR_TEST);
    else glDisable(GL_SCISSOR_TEST);
    if (s.clip0) glEnable(GL_CLIP_DISTANCE0);
    else glDisable(GL_CLIP_DISTANCE0);
}
// orig: (host) OpenGL: the reflections' textures on the materials' units (0: the one being drawn into left unbound)
void gl_units(bool planar, bool cube) {
    GLint prev = 0;
    glGetIntegerv(GL_ACTIVE_TEXTURE, &prev);
    glActiveTexture(GL_TEXTURE0 + kPlanarUnit);
    glBindTexture(GL_TEXTURE_2D, planar ? g_r.tex : 0);
    glActiveTexture(GL_TEXTURE0 + kCubeUnit);
    glBindTexture(GL_TEXTURE_2D_ARRAY, cube && g_r.cube_ok ? g_r.ctex : 0);
    glActiveTexture(static_cast<GLenum>(prev));
}

// orig: (host) the world (`world`; `sky`: its sky) and the mobys drawn through `light` (cut at the plane, `eye` its eye; unlit) into the
// bound target, the ties' and shrubs' instances outside it or under the water left out
void draw_view(const glm::mat4& light, const glm::vec4& cut, const glm::vec3& eye, bool mobys, bool sky = true, bool world = true) {
    mrp_lighting_reflect_begin(glm::value_ptr(light), glm::value_ptr(cut), glm::value_ptr(eye));
    // (testing, RAC1_REFLECT_CASTERS=1: the world the shadow maps alone draw - outside the view - drawn too; their fog is not made for
    // them: far ones come out in the fog's colour)
    static const bool casters = std::getenv("RAC1_REFLECT_CASTERS") != nullptr && std::atoi(std::getenv("RAC1_REFLECT_CASTERS")) != 0;
    // (testing, RAC1_REFLECT_CULL=0: the instances outside the view not left out)
    static const bool cull = std::getenv("RAC1_REFLECT_CULL") == nullptr || std::atoi(std::getenv("RAC1_REFLECT_CULL")) != 0;
    if (world || sky) mrp_world_draw_reflect(casters, sky, world, cull ? glm::value_ptr(light) : nullptr, -cut.w);
    if (mobys) mrp_moby_draw_reflect();
    mrp_lighting_reflect_end();
}

// orig: (host) probe slot `k` drawn: each face the world above its water past kNear units of it, as the frame holds it (`fresh`: over
// nothing - alpha 0, the sky's faces or the materials' sky colour; else filled in only where nothing nearer is held - its depth kept,
// the new one pushed a few hundredths further back, so the same scenery at another LOD does not draw over itself); then the sky's faces
// (`k` -1 alone): the frame's sky over them
void cube_capture(int k, bool fresh) {
    if (!cube_target()) return;
    State::Probe sky_probe;
    if (k < 0) sky_probe.at = g_r.probes[g_r.active >= 0 ? g_r.active : 0].at;
    State::Probe& pr = k >= 0 ? g_r.probes[k] : sky_probe;
    const bool sky = k < 0;
    if (sky) fresh = !g_r.sky_made;
    const Saved sv = save_gl();
    const glm::mat4 P = face_proj(fresh || sky ? kNear : kNear * 0.97f);
    const glm::vec4 cut(0.0f, 0.0f, -1.0f, -(pr.h - kCutBelow));
    if (vk::is_active()) {
        vk::transition_image(vk::cmd(), g_r.vk_cube, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk::transition_image(vk::cmd(), g_r.vk_cdepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    } else {
        gl_units(g_r.planar_on, false);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_r.cfbo);
        glEnable(GL_CLIP_DISTANCE0);
        glDisable(GL_SCISSOR_TEST);
    }
    for (int f = 0; f < 6; ++f) {
        if (!fresh && f == 5) continue;  // (straight down: the water's cut leaves nothing there)
        glm::vec3 F, U;
        face_axes(f, F, U);
        const glm::mat4 light = P * glm::lookAt(pr.at, pr.at + F, U);
        const int layer = (sky ? kSkyLayer : 6 * k) + f;
        if (vk::is_active()) {
            vk::RenderTarget rt{};
            rt.color_view = g_r.vk_faces[layer];
            rt.color_format = g_r.vk_cube.format;
            rt.depth_view = g_r.vk_dfaces[layer];
            rt.depth_format = g_r.vk_cdepth.format;
            rt.stencil_view = g_r.vk_dfaces[layer];
            rt.width = kCubeRes;
            rt.height = kCubeRes;
            const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            vk::set_viewport(0, 0, kCubeRes, kCubeRes);
            vk::push_target(rt, fresh, clear, fresh || sky, 1.0f, true, 0);
            draw_view(light, cut, pr.at, false, sky, !sky);
            vk::pop_target();
        } else {
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_r.ctex, 0, layer);
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, g_r.cdepth, 0, layer);
            glViewport(0, 0, kCubeRes, kCubeRes);
            glDepthMask(GL_TRUE);
            glStencilMask(0xff);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearDepth(1.0);
            glClearStencil(0);
            glClear((fresh ? GL_COLOR_BUFFER_BIT : 0) | (fresh || sky ? GL_DEPTH_BUFFER_BIT : 0) | GL_STENCIL_BUFFER_BIT);
            draw_view(light, cut, pr.at, false, sky, !sky);
        }
    }
    if (vk::is_active()) vk::transition_image(vk::cmd(), g_r.vk_cube, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    restore(sv);
    if (sky) {
        g_r.sky_made = true;
        return;
    }
    if (fresh) {
        pr.fills = 0;
        ++g_r.n_cube;
    } else {
        ++pr.fills;
        ++g_r.n_fill;
    }
    static const bool trace = std::getenv("RAC1_REFLECT_STATS") != nullptr;
    if (trace)
        std::printf("[reflect] probe %d (zone %d %d) %s at %.1f %.1f %.1f (water %.2f), pass %llu\n", k, pr.ix, pr.iy, fresh ? "made" : "filled in", pr.at.x,
                    pr.at.y, pr.at.z, pr.h, static_cast<unsigned long long>(g_r.frame));
}

// orig: (host) this frame's probe: the zone the camera is in (the one in use kept while the camera is within kZoneHyst of it) and the
// water's held-still height `h`; made the first time (in the slot least recently read), filled in as the view turns; -1 none
int probe_for(const glm::vec3& cam, const glm::vec3& fwd, float h) {
    auto in_zone = [&](const State::Probe& p, float margin) {
        return std::fabs(p.h - h) < kBodyApart && cam.x >= p.ix * kZone - margin && cam.x < (p.ix + 1) * kZone + margin &&
               cam.y >= p.iy * kZone - margin && cam.y < (p.iy + 1) * kZone + margin;
    };
    int k = g_r.active >= 0 && g_r.probes[g_r.active].used && in_zone(g_r.probes[g_r.active], kZoneHyst) ? g_r.active : -1;
    for (int i = 0; i < kSlots && k < 0; ++i)
        if (g_r.probes[i].used && in_zone(g_r.probes[i], 0.0f)) k = i;
    bool fresh = false;
    if (k < 0) {
        k = 0;
        for (int i = 0; i < kSlots; ++i) {
            if (!g_r.probes[i].used) {
                k = i;
                break;
            }
            if (g_r.probes[i].last < g_r.probes[k].last) k = i;
        }
        State::Probe& p = g_r.probes[k];
        p = State::Probe{};
        p.used = true;
        p.ix = static_cast<int>(std::floor(cam.x / kZone));
        p.iy = static_cast<int>(std::floor(cam.y / kZone));
        p.h = h;
        p.at = glm::vec3((static_cast<float>(p.ix) + 0.5f) * kZone, (static_cast<float>(p.iy) + 0.5f) * kZone, h + kProbeAbove);
        fresh = true;
    }
    State::Probe& p = g_r.probes[k];
    p.last = g_r.frame;
    const bool fill = !fresh && g_r.frame >= p.fill_frame + kFillGap &&
                      (std::acos(std::clamp(glm::dot(fwd, p.fill_fwd), -1.0f, 1.0f)) > kFillTurn || glm::length(cam - p.fill_cam) > kFillMove);
    if (fresh || fill) {
        p.fill_frame = g_r.frame;
        p.fill_cam = cam;
        p.fill_fwd = fwd;
        cube_capture(k, fresh);
        cube_capture(-1, false);  // (the sky's faces: what the frame draws of it over what they hold)
    }
    return k;
}

// orig: (host) the planar view: the frame drawn mirrored about z = h into a target of half the 3D's viewport - only the rectangle `box`
// of the clip square (x0, y0, x1, y1: where the water is), the rest left empty (alpha 0)
void planar_draw(const int vp[4], float h, const glm::vec3& cam, const float box[4]) {
    const int div = std::clamp(g_reflect_scale, 1, 8);
    const int w = std::max(1, vp[2] / div), hh = std::max(1, vp[3] / div);
    if (!planar_target(w, hh)) return;
    // the rectangle in the target's texels, and the clip square's part it is exactly: the view stretched over it (a crop of the projection)
    const int tx0 = std::clamp(static_cast<int>(std::floor((box[0] + 1.0f) * 0.5f * static_cast<float>(w))), 0, w - 1);
    const int tx1 = std::clamp(static_cast<int>(std::ceil((box[2] + 1.0f) * 0.5f * static_cast<float>(w))), tx0 + 1, w);
    const int ty0 = std::clamp(static_cast<int>(std::floor((box[1] + 1.0f) * 0.5f * static_cast<float>(hh))), 0, hh - 1);
    const int ty1 = std::clamp(static_cast<int>(std::ceil((box[3] + 1.0f) * 0.5f * static_cast<float>(hh))), ty0 + 1, hh);
    const float nx0 = 2.0f * static_cast<float>(tx0) / static_cast<float>(w) - 1.0f, nx1 = 2.0f * static_cast<float>(tx1) / static_cast<float>(w) - 1.0f;
    const float ny0 = 2.0f * static_cast<float>(ty0) / static_cast<float>(hh) - 1.0f, ny1 = 2.0f * static_cast<float>(ty1) / static_cast<float>(hh) - 1.0f;
    glm::mat4 C(1.0f);
    C[0][0] = 2.0f / (nx1 - nx0);
    C[3][0] = -(nx1 + nx0) / (nx1 - nx0);
    C[1][1] = 2.0f / (ny1 - ny0);
    C[3][1] = -(ny1 + ny0) / (ny1 - ny0);
    g_r.area += static_cast<double>(tx1 - tx0) * (ty1 - ty0) / (static_cast<double>(w) * hh);
    const Saved sv = save_gl();
    float vpf[16];
    mrp_world_view_proj(vpf);
    glm::mat4 M(1.0f);  // (the mirror about the plane z = h)
    M[2][2] = -1.0f;
    M[3][2] = 2.0f * h;
    const glm::mat4 light = C * glm::make_mat4(vpf) * M;
    const glm::vec4 cut(0.0f, 0.0f, -1.0f, -(h - kCutBelow));
    const glm::vec3 eye(cam.x, cam.y, 2.0f * h - cam.z);
    if (vk::is_active()) {
        vk::transition_image(vk::cmd(), g_r.vk_c, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk::transition_image(vk::cmd(), g_r.vk_d, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
        vk::RenderTarget rt{};
        rt.color_view = g_r.vk_c.view;
        rt.color_format = g_r.vk_c.format;
        rt.depth_view = g_r.vk_d.view;
        rt.depth_format = g_r.vk_d.format;
        rt.stencil_view = g_r.vk_d.view;
        rt.width = static_cast<uint32_t>(w);
        rt.height = static_cast<uint32_t>(hh);
        const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
        vk::push_target(rt, true, clear, true, 1.0f, true, 0);
        vk::set_viewport(tx0, ty0, tx1 - tx0, ty1 - ty0);
        draw_view(light, cut, eye, true);
        vk::pop_target();
        vk::transition_image(vk::cmd(), g_r.vk_c, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    } else {
        gl_units(false, g_r.cube_ok);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_r.fbo);
        glViewport(0, 0, w, hh);
        glDisable(GL_SCISSOR_TEST);
        glEnable(GL_CLIP_DISTANCE0);
        glDepthMask(GL_TRUE);
        glStencilMask(0xff);
        glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
        glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
        glClearDepth(1.0);
        glClearStencil(0);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        glViewport(tx0, ty0, tx1 - tx0, ty1 - ty0);
        draw_view(light, cut, eye, true);
    }
    restore(sv);
    g_r.planar_on = true;
    g_r.plane_h = h;
}
}  // namespace

// orig: (host) see mrp_reflect.h
void mrp_reflect_note_water(float h, float area, bool flat, const float box[4]) {
    if (!std::isfinite(h) || !std::isfinite(area)) return;
    std::lock_guard<std::mutex> lg(g_r.m);
    if (g_r.notes.size() < 4096) g_r.notes.push_back({h, area, flat, {box[0], box[1], box[2], box[3]}});
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_pass(const int vp[4]) {
    ++g_r.frame;
    g_r.planar_on = false;
    std::vector<Note> notes;
    {
        std::lock_guard<std::mutex> lg(g_r.m);
        notes.swap(g_r.notes);
    }
    if (g_water_reflections <= 0 || g_surface_detail <= 0 || !mrp_lighting_active() || notes.empty()) {
        ++g_r.n_none;
        if (!vk::is_active() && g_r.fbo != 0) gl_units(false, g_r.cube_ok);
        return;
    }
    float cam_a[3];
    if (!mrp_lighting_camera(cam_a)) return;
    const glm::vec3 cam(cam_a[0], cam_a[1], cam_a[2]);
    g_r.water_frame = g_r.frame;
    RAC1_PROF_ZONE(kWaterRefl);
    // the plane: the flat height with the largest share of the view (a quarter of a unit apart: one body), the water's mean height
    std::map<int, float> by_h;
    double sum_h = 0.0, sum_a = 0.0;
    for (const Note& n : notes) {
        if (n.flat) by_h[static_cast<int>(std::lround(n.h * 4.0f))] += n.area;
        sum_h += static_cast<double>(n.h) * (n.area + 1e-6);
        sum_a += n.area + 1e-6;
    }
    int best = 0;
    float best_a = 0.0f;
    for (const auto& [k, a] : by_h)
        if (a > best_a) {
            best_a = a;
            best = k;
        }
    float plane = 0.0f;
    bool have_plane = false;
    if (best_a > 1e-3f) {  // (the height itself: the notes of that body averaged)
        double s = 0.0, n = 0.0;
        for (const Note& no : notes)
            if (no.flat && static_cast<int>(std::lround(no.h * 4.0f)) == best) {
                s += no.h;
                n += 1.0;
            }
        plane = static_cast<float>(s / n);
        have_plane = true;
    }
    // the rectangle the planar view draws: the plane's water's, this frame's notes and the frame before's (they are a frame late), widened
    // by kBoxMargin (the ripples move the read a little; the view turns)
    float box[4] = {2.0f, 2.0f, -2.0f, -2.0f};
    if (have_plane) {
        for (const Note& no : notes)
            if (no.flat && static_cast<int>(std::lround(no.h * 4.0f)) == best && no.box[0] <= no.box[2]) {
                box[0] = std::min(box[0], no.box[0]);
                box[1] = std::min(box[1], no.box[1]);
                box[2] = std::max(box[2], no.box[2]);
                box[3] = std::max(box[3], no.box[3]);
            }
        const float now[4] = {box[0], box[1], box[2], box[3]};
        if (g_r.last_box[0] <= g_r.last_box[2]) {
            box[0] = std::min(box[0], g_r.last_box[0]);
            box[1] = std::min(box[1], g_r.last_box[1]);
            box[2] = std::max(box[2], g_r.last_box[2]);
            box[3] = std::max(box[3], g_r.last_box[3]);
        }
        std::memcpy(g_r.last_box, now, sizeof now);
        // (testing, RAC1_REFLECT_CULL=0: the whole view drawn)
        static const bool full = std::getenv("RAC1_REFLECT_CULL") != nullptr && std::atoi(std::getenv("RAC1_REFLECT_CULL")) == 0;
        if (box[0] > box[2] || full) {  // (none noted: the whole view)
            box[0] = box[1] = -1.0f;
            box[2] = box[3] = 1.0f;
        }
        box[0] = std::max(-1.0f, box[0] - kBoxMargin);
        box[1] = std::max(-1.0f, box[1] - kBoxMargin);
        box[2] = std::min(1.0f, box[2] + kBoxMargin);
        box[3] = std::min(1.0f, box[3] + kBoxMargin);
    } else {
        g_r.last_box[0] = 2.0f;
        g_r.last_box[2] = -2.0f;
    }
    const float water_h = have_plane ? plane : static_cast<float>(sum_h / sum_a);
    // the cube map: the probe of the zone and body of water (made or filled in if due)
    float vpf[16];
    mrp_world_view_proj(vpf);
    glm::vec3 fwd(vpf[3], vpf[7], vpf[11]);  // (the clip w's row: the view's direction)
    fwd = glm::length(fwd) > 0.0f ? glm::normalize(fwd) : glm::vec3(0.0f, 1.0f, 0.0f);
    g_r.active = probe_for(cam, fwd, water_h);
    g_r.cube_ok = g_r.active >= 0;
    if (g_water_reflections >= 3 && !mrp_rt_active()) {  // (an RT value the renderer cannot trace: played as Medium, said once)
        static bool said = false;
        if (!said) {
            said = true;
            std::printf("[reflect] Water Reflections %s: %s - played as Medium\n", "Medium (RT)",
                        mrp_rt_supported() ? "RAC1_RT=0" : "no ray queries here (Vulkan with ray queries and non-uniform sampled-image indexing)");
        }
    }
    if (g_water_reflections >= 2 && have_plane && cam.z > plane + 0.05f && !mrp_rt_tracing()) {  // (Medium (RT): the reflection traced instead)
        planar_draw(vp, plane, cam, box);
        ++g_r.n_planar;
        ++g_r.planes[static_cast<int>(std::lround(plane * 4.0f))];
    }
    if (!vk::is_active()) gl_units(g_r.planar_on, g_r.cube_ok);
    static const long dump = std::getenv("RAC1_REFLECT_DUMP") != nullptr ? std::atol(std::getenv("RAC1_REFLECT_DUMP")) : -1;
    if (dump >= 0 && g_r.frame == static_cast<uint64_t>(dump) && !vk::is_active()) {  // (testing: the planar view and the cube map's faces as PPM files)
        auto save = [](const char* path, int w, int h, const std::vector<uint8_t>& px) {
            FILE* f = std::fopen(path, "wb");
            if (f == nullptr) return;
            std::fprintf(f, "P6\n%d %d\n255\n", w, h);
            for (int y = h - 1; y >= 0; --y)
                for (int x = 0; x < w; ++x) std::fwrite(&px[(static_cast<size_t>(y) * w + x) * 4], 1, 3, f);
            std::fclose(f);
        };
        GLint prev = 0;
        glGetIntegerv(GL_TEXTURE_BINDING_2D, &prev);
        if (g_r.planar_on) {
            std::vector<uint8_t> px(static_cast<size_t>(g_r.pw) * g_r.ph * 4);
            glBindTexture(GL_TEXTURE_2D, g_r.tex);
            glGetTexImage(GL_TEXTURE_2D, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            save("reflect_planar.ppm", g_r.pw, g_r.ph, px);
        }
        glBindTexture(GL_TEXTURE_2D, static_cast<GLuint>(prev));
        if (g_r.cube_ok) {
            std::vector<uint8_t> px(static_cast<size_t>(kCubeRes) * kCubeRes * 4 * kLayers);
            glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev);
            glBindTexture(GL_TEXTURE_2D_ARRAY, g_r.ctex);
            glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(prev));
            std::vector<uint8_t> strip(static_cast<size_t>(kCubeRes) * 6 * kCubeRes * 4 * 2);  // (the faces side by side: the probe's, the sky's)
            for (int r = 0; r < 2; ++r)
                for (int f = 0; f < 6; ++f)
                    for (int y = 0; y < kCubeRes; ++y)
                        std::memcpy(&strip[((static_cast<size_t>(r) * kCubeRes + y) * kCubeRes * 6 + static_cast<size_t>(f) * kCubeRes) * 4],
                                    &px[((static_cast<size_t>((r == 0 ? 6 * g_r.active : kSkyLayer) + f) * kCubeRes + y) * kCubeRes) * 4],
                                    static_cast<size_t>(kCubeRes) * 4);
            save("reflect_cube.ppm", kCubeRes * 6, kCubeRes * 2, strip);
        }
    }
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_uniforms(const int vp[4], float a[4], float b[4], float c[4], float d[4]) {
    const bool on = g_water_reflections > 0 && g_r.water_frame == g_r.frame && g_r.water_frame != 0;
    // (testing, RAC1_REFLECT_DEBUG: 1 the water shows where its reflection comes from, 2 the reflection alone)
    static const int debug = std::getenv("RAC1_REFLECT_DEBUG") != nullptr ? std::atoi(std::getenv("RAC1_REFLECT_DEBUG")) : 0;
    // (the shader's modes: 1 Low, 2 Medium - an RT value where the seas do not trace reads as Medium)
    a[0] = on ? static_cast<float>(std::min(g_water_reflections, 2)) + (debug == 1 ? 8.0f : debug == 2 ? 16.0f : 0.0f) : 0.0f;
    a[1] = g_r.plane_h;
    a[2] = on && g_r.planar_on ? 1.0f : 0.0f;
    a[3] = on && g_r.cube_ok ? 1.0f : 0.0f;
    b[0] = static_cast<float>(vp[0]);
    b[1] = static_cast<float>(vp[1]);
    b[2] = vp[2] > 0 ? 1.0f / static_cast<float>(vp[2]) : 0.0f;
    b[3] = vp[3] > 0 ? 1.0f / static_cast<float>(vp[3]) : 0.0f;
    const bool probe = g_r.cube_ok && g_r.active >= 0;
    const glm::vec3 at = probe ? g_r.probes[g_r.active].at : glm::vec3(0.0f);
    c[0] = at.x;
    c[1] = at.y;
    c[2] = at.z;
    c[3] = probe ? kProxy : 0.0f;
    d[0] = probe ? static_cast<float>(6 * g_r.active) : 0.0f;
    d[1] = probe && g_r.sky_made ? static_cast<float>(kSkyLayer) : -1.0f;
    d[2] = d[3] = 0.0f;
}

// orig: (host) see mrp_reflect.h
const vk::GpuImage* mrp_reflect_vk_planar() {
    return g_r.vk_c.image != VK_NULL_HANDLE && g_r.vk_c.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? &g_r.vk_c : nullptr;
}
// orig: (host) see mrp_reflect.h
const vk::GpuImage* mrp_reflect_vk_cube() {
    return g_r.cube_ok && g_r.vk_cube.image != VK_NULL_HANDLE && g_r.vk_cube.layout == VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL ? &g_r.vk_cube : nullptr;
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_level_reset() {
    g_r.cube_ok = false;
    g_r.active = -1;
    g_r.sky_made = false;
    for (State::Probe& p : g_r.probes) p.used = false;
    std::lock_guard<std::mutex> lg(g_r.m);
    g_r.notes.clear();
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_report() {
    static const bool on = std::getenv("RAC1_REFLECT_STATS") != nullptr;
    if (!on) return;
    std::printf("[reflect] passes %llu: planar views %llu, probes made %llu, filled in %llu, no water %llu; planes:", static_cast<unsigned long long>(g_r.frame),
                static_cast<unsigned long long>(g_r.n_planar), static_cast<unsigned long long>(g_r.n_cube), static_cast<unsigned long long>(g_r.n_fill),
                static_cast<unsigned long long>(g_r.n_none));
    for (const auto& [k, n] : g_r.planes) std::printf(" %.2f (%llu)", static_cast<double>(k) / 4.0, static_cast<unsigned long long>(n));
    uint64_t n[2];
    mrp_world_reflect_counts(n);
    std::printf("; the planar views' drawn share %.1f %%; ties' and shrubs' draws in the views %llu, left out %llu\n",
                g_r.n_planar != 0 ? 100.0 * g_r.area / static_cast<double>(g_r.n_planar) : 0.0, static_cast<unsigned long long>(n[0]),
                static_cast<unsigned long long>(n[1]));
}

}  // namespace rac1
