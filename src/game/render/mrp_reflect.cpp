// orig: (host) Options > Graphics > Water Reflections (see mrp_reflect.h; docs/materials.md#water-reflections)
#include <game/render/mrp_reflect.h>

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
    return e != nullptr ? std::clamp(std::atoi(e), 0, 2) : 2;
}();

namespace {
constexpr int kPlanarUnit = 22, kCubeUnit = 23;  // (OpenGL: the materials' texture units, mrp_materials_glsl.inc)
constexpr int kCubeRes = 256;                    // (a face's size)
constexpr float kCutBelow = 0.02f;               // (the planar view keeps what lies above the water less this: the waterline closed)
constexpr float kCubeAbove = 1.0f;               // (the cube map seen from the eye, at least this far above the water)
// (drawn again over itself - what the frame holds now over what earlier views left - when the camera has turned this far, in radians, or
// moved this far since; started anew for another body of water or level)
constexpr float kCubeTurn = 0.7f, kCubeMove = 25.0f, kCubeRise = 4.0f;

struct Note {
    float h, area;
    bool flat;
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
    // the cube map
    bool cube_ok = false;
    glm::vec3 cube_at{0.0f}, cube_fwd{0.0f, 1.0f, 0.0f};
    float cube_h = 0.0f;
    GLuint cfbo = 0, ctex = 0, cdepth = 0;
    vk::GpuImage vk_cube{}, vk_cdepth{};
    VkImageView vk_faces[6] = {};
    // the report
    uint64_t n_planar = 0, n_cube = 0, n_none = 0;
    std::map<int, uint64_t> planes;  // (height x 4: frames)
    bool reg = false;
} g_r;

// orig: (host) Vulkan: the images freed with the device
void vk_shutdown() {
    const VkDevice dev = vk::device();
    for (VkImageView& v : g_r.vk_faces) {
        if (v != VK_NULL_HANDLE && dev != VK_NULL_HANDLE) vkDestroyImageView(dev, v, nullptr);
        v = VK_NULL_HANDLE;
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

// orig: (host) the cube map's target: six faces (layers) of kCubeRes, a depth-and-stencil for one face at a time
bool cube_target() {
    if (vk::is_active()) {
        if (!g_r.reg) {
            g_r.reg = true;
            vk::register_shutdown_callback(vk_shutdown);
        }
        if (g_r.vk_cube.image != VK_NULL_HANDLE) return true;
        vk::create_image(kCubeRes, kCubeRes, vk::color_format(), VK_IMAGE_USAGE_COLOR_ATTACHMENT_BIT | VK_IMAGE_USAGE_SAMPLED_BIT, VK_IMAGE_ASPECT_COLOR_BIT,
                         g_r.vk_cube, 6, 0, VK_IMAGE_VIEW_TYPE_2D_ARRAY);
        vk::create_image(kCubeRes, kCubeRes, vk::depth_stencil_format(), VK_IMAGE_USAGE_DEPTH_STENCIL_ATTACHMENT_BIT,
                         VK_IMAGE_ASPECT_DEPTH_BIT | VK_IMAGE_ASPECT_STENCIL_BIT, g_r.vk_cdepth);
        for (uint32_t f = 0; f < 6 && g_r.vk_cube.image != VK_NULL_HANDLE; ++f) {
            VkImageViewCreateInfo vci{};
            vci.sType = VK_STRUCTURE_TYPE_IMAGE_VIEW_CREATE_INFO;
            vci.image = g_r.vk_cube.image;
            vci.viewType = VK_IMAGE_VIEW_TYPE_2D;
            vci.format = g_r.vk_cube.format;
            vci.subresourceRange = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 1, f, 1};
            vkCreateImageView(vk::device(), &vci, nullptr, &g_r.vk_faces[f]);
        }
        return g_r.vk_cube.image != VK_NULL_HANDLE && g_r.vk_cdepth.image != VK_NULL_HANDLE && g_r.vk_faces[5] != VK_NULL_HANDLE;
    }
    if (g_r.cfbo != 0) return true;
    glGenFramebuffers(1, &g_r.cfbo);
    glGenTextures(1, &g_r.ctex);
    glGenRenderbuffers(1, &g_r.cdepth);
    prof::gl_track_n(prof::kGlTexture, 1, &g_r.ctex, "Water reflections: the cube map");
    prof::gl_track_n(prof::kGlRenderbuffer, 1, &g_r.cdepth, "Water reflections: the cube map");
    GLint prev_t = 0, prev_rb = 0;
    glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev_t);
    glGetIntegerv(GL_RENDERBUFFER_BINDING, &prev_rb);
    glBindTexture(GL_TEXTURE_2D_ARRAY, g_r.ctex);
    glTexImage3D(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA8, kCubeRes, kCubeRes, 6, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MIN_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_MAG_FILTER, GL_LINEAR);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_S, GL_CLAMP_TO_EDGE);
    glTexParameteri(GL_TEXTURE_2D_ARRAY, GL_TEXTURE_WRAP_T, GL_CLAMP_TO_EDGE);
    glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(prev_t));
    glBindRenderbuffer(GL_RENDERBUFFER, g_r.cdepth);
    glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, kCubeRes, kCubeRes);
    glBindRenderbuffer(GL_RENDERBUFFER, static_cast<GLuint>(prev_rb));
    return true;
}

// orig: (host) the projection of the frame's view (world_view_proj's) with another field: a square of 90 degrees
glm::mat4 face_proj() {
    const float n = kRenderNear, f = kRenderFar;
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

// orig: (host) the world (and the mobys) drawn through `light` (cut at the plane, lit as from `eye`) into the bound target
void draw_view(const glm::mat4& light, const glm::vec4& cut, const glm::vec3& eye, bool mobys) {
    mrp_lighting_reflect_begin(glm::value_ptr(light), glm::value_ptr(cut), glm::value_ptr(eye));
    // (testing, RAC1_REFLECT_CASTERS=1: the world the shadow maps alone draw - outside the view - drawn too; their fog is not made for
    // them: far ones come out in the fog's colour)
    static const bool casters = std::getenv("RAC1_REFLECT_CASTERS") != nullptr && std::atoi(std::getenv("RAC1_REFLECT_CASTERS")) != 0;
    mrp_world_draw_reflect(casters);
    if (mobys) mrp_moby_draw_reflect();
    mrp_lighting_reflect_end();
}

// orig: (host) the cube map drawn from `at` (the water at height `h` below it): each face the sky and the world above the water the
// frame holds, over what earlier views left there (`fresh`: over nothing - alpha 0, the materials' sky colour)
void cube_capture(const glm::vec3& at, float h, const glm::vec3& fwd, bool fresh) {
    if (!cube_target()) return;
    const Saved sv = save_gl();
    const glm::mat4 P = face_proj();
    const glm::vec4 cut(0.0f, 0.0f, -1.0f, -(h - kCutBelow));
    if (vk::is_active()) {
        vk::transition_image(vk::cmd(), g_r.vk_cube, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
        vk::transition_image(vk::cmd(), g_r.vk_cdepth, VK_IMAGE_LAYOUT_DEPTH_STENCIL_ATTACHMENT_OPTIMAL);
    } else {
        gl_units(g_r.planar_on, false);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, g_r.cfbo);
        glFramebufferRenderbuffer(GL_DRAW_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, g_r.cdepth);
        glEnable(GL_CLIP_DISTANCE0);
        glDisable(GL_SCISSOR_TEST);
    }
    for (int f = 0; f < 6; ++f) {
        glm::vec3 F, U;
        face_axes(f, F, U);
        const glm::mat4 light = P * glm::lookAt(at, at + F, U);
        if (vk::is_active()) {
            vk::RenderTarget rt{};
            rt.color_view = g_r.vk_faces[f];
            rt.color_format = g_r.vk_cube.format;
            rt.depth_view = g_r.vk_cdepth.view;
            rt.depth_format = g_r.vk_cdepth.format;
            rt.stencil_view = g_r.vk_cdepth.view;
            rt.width = kCubeRes;
            rt.height = kCubeRes;
            const float clear[4] = {0.0f, 0.0f, 0.0f, 0.0f};
            vk::set_viewport(0, 0, kCubeRes, kCubeRes);
            vk::push_target(rt, fresh, clear, true, 1.0f, true, 0);
            draw_view(light, cut, at, false);
            vk::pop_target();
        } else {
            glFramebufferTextureLayer(GL_DRAW_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, g_r.ctex, 0, f);
            glViewport(0, 0, kCubeRes, kCubeRes);
            glDepthMask(GL_TRUE);
            glStencilMask(0xff);
            glColorMask(GL_TRUE, GL_TRUE, GL_TRUE, GL_TRUE);
            glClearColor(0.0f, 0.0f, 0.0f, 0.0f);
            glClearDepth(1.0);
            glClearStencil(0);
            glClear((fresh ? GL_COLOR_BUFFER_BIT : 0) | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            draw_view(light, cut, at, false);
        }
    }
    if (vk::is_active()) vk::transition_image(vk::cmd(), g_r.vk_cube, VK_IMAGE_LAYOUT_SHADER_READ_ONLY_OPTIMAL);
    restore(sv);
    g_r.cube_ok = true;
    g_r.cube_at = at;
    g_r.cube_fwd = fwd;
    g_r.cube_h = h;
    ++g_r.n_cube;
    static const bool trace = std::getenv("RAC1_REFLECT_STATS") != nullptr;
    if (trace)
        std::printf("[reflect] cube map %s at %.1f %.1f %.1f (water %.2f), pass %llu\n", fresh ? "made" : "drawn again", at.x, at.y, at.z, h,
                    static_cast<unsigned long long>(g_r.frame));
}

// orig: (host) the planar view: the frame drawn mirrored about z = h into a target of half the 3D's viewport
void planar_draw(const int vp[4], float h, const glm::vec3& cam) {
    // (the planar view at 1 / RAC1_REFLECT_SCALE of the 3D's size: 2, half, by default; testing)
    static const int div = std::getenv("RAC1_REFLECT_SCALE") != nullptr ? std::clamp(std::atoi(std::getenv("RAC1_REFLECT_SCALE")), 1, 8) : 2;
    const int w = std::max(1, vp[2] / div), hh = std::max(1, vp[3] / div);
    if (!planar_target(w, hh)) return;
    const Saved sv = save_gl();
    float vpf[16];
    mrp_world_view_proj(vpf);
    glm::mat4 M(1.0f);  // (the mirror about the plane z = h)
    M[2][2] = -1.0f;
    M[3][2] = 2.0f * h;
    const glm::mat4 light = glm::make_mat4(vpf) * M;
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
        vk::set_viewport(0, 0, w, hh);
        vk::push_target(rt, true, clear, true, 1.0f, true, 0);
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
        draw_view(light, cut, eye, true);
    }
    restore(sv);
    g_r.planar_on = true;
    g_r.plane_h = h;
}
}  // namespace

// orig: (host) see mrp_reflect.h
void mrp_reflect_note_water(float h, float area, bool flat) {
    if (!std::isfinite(h) || !std::isfinite(area)) return;
    std::lock_guard<std::mutex> lg(g_r.m);
    if (g_r.notes.size() < 4096) g_r.notes.push_back({h, area, flat});
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
    const float water_h = have_plane ? plane : static_cast<float>(sum_h / sum_a);
    // the cube map: made at the first frame water is seen (from the eye: in the open air), drawn again over itself when the view has turned
    // or moved well away (the frame holds only what the game's view sees: the rest is left from the views before), anew for another body
    float vpf[16];
    mrp_world_view_proj(vpf);
    glm::vec3 fwd(vpf[3], vpf[7], vpf[11]);  // (the clip w's row: the view's direction)
    fwd = glm::length(fwd) > 0.0f ? glm::normalize(fwd) : glm::vec3(0.0f, 1.0f, 0.0f);
    const glm::vec3 at(cam.x, cam.y, std::max(cam.z, water_h + kCubeAbove));
    const bool fresh = !g_r.cube_ok || std::fabs(g_r.cube_h - water_h) > 2.0f;
    const bool again = !fresh && (std::acos(std::clamp(glm::dot(fwd, g_r.cube_fwd), -1.0f, 1.0f)) > kCubeTurn ||
                                  glm::length(glm::vec2(at.x - g_r.cube_at.x, at.y - g_r.cube_at.y)) > kCubeMove || std::fabs(at.z - g_r.cube_at.z) > kCubeRise);
    if (fresh || again) cube_capture(at, water_h, fwd, fresh);
    if (g_water_reflections >= 2 && have_plane && cam.z > plane + 0.05f) {
        planar_draw(vp, plane, cam);
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
            std::vector<uint8_t> px(static_cast<size_t>(kCubeRes) * kCubeRes * 4 * 6);
            glGetIntegerv(GL_TEXTURE_BINDING_2D_ARRAY, &prev);
            glBindTexture(GL_TEXTURE_2D_ARRAY, g_r.ctex);
            glGetTexImage(GL_TEXTURE_2D_ARRAY, 0, GL_RGBA, GL_UNSIGNED_BYTE, px.data());
            glBindTexture(GL_TEXTURE_2D_ARRAY, static_cast<GLuint>(prev));
            std::vector<uint8_t> strip(static_cast<size_t>(kCubeRes) * 6 * kCubeRes * 4);  // (the faces side by side)
            for (int f = 0; f < 6; ++f)
                for (int y = 0; y < kCubeRes; ++y)
                    std::memcpy(&strip[(static_cast<size_t>(y) * kCubeRes * 6 + static_cast<size_t>(f) * kCubeRes) * 4],
                                &px[((static_cast<size_t>(f) * kCubeRes + y) * kCubeRes) * 4], static_cast<size_t>(kCubeRes) * 4);
            save("reflect_cube.ppm", kCubeRes * 6, kCubeRes, strip);
        }
    }
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_uniforms(const int vp[4], float a[4], float b[4]) {
    const bool on = g_water_reflections > 0 && g_r.water_frame == g_r.frame && g_r.water_frame != 0;
    // (testing, RAC1_REFLECT_DEBUG: 1 the water shows where its reflection comes from, 2 the reflection alone)
    static const int debug = std::getenv("RAC1_REFLECT_DEBUG") != nullptr ? std::atoi(std::getenv("RAC1_REFLECT_DEBUG")) : 0;
    a[0] = on ? static_cast<float>(g_water_reflections) + (debug == 1 ? 8.0f : debug == 2 ? 16.0f : 0.0f) : 0.0f;
    a[1] = g_r.plane_h;
    a[2] = on && g_r.planar_on ? 1.0f : 0.0f;
    a[3] = on && g_r.cube_ok ? 1.0f : 0.0f;
    b[0] = static_cast<float>(vp[0]);
    b[1] = static_cast<float>(vp[1]);
    b[2] = vp[2] > 0 ? 1.0f / static_cast<float>(vp[2]) : 0.0f;
    b[3] = vp[3] > 0 ? 1.0f / static_cast<float>(vp[3]) : 0.0f;
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
    std::lock_guard<std::mutex> lg(g_r.m);
    g_r.notes.clear();
}

// orig: (host) see mrp_reflect.h
void mrp_reflect_report() {
    static const bool on = std::getenv("RAC1_REFLECT_STATS") != nullptr;
    if (!on) return;
    std::printf("[reflect] passes %llu: planar views %llu, cube maps %llu, no water %llu; planes:", static_cast<unsigned long long>(g_r.frame),
                static_cast<unsigned long long>(g_r.n_planar), static_cast<unsigned long long>(g_r.n_cube), static_cast<unsigned long long>(g_r.n_none));
    for (const auto& [k, n] : g_r.planes) std::printf(" %.2f (%llu)", static_cast<double>(k) / 4.0, static_cast<unsigned long long>(n));
    std::printf("\n");
}

}  // namespace rac1
