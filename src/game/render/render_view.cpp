// The host renderer's view (see render_view.h). Host code: the PS2 draws with the VU from the
// matrices of 0x1f6e48; the host builds the equivalent GL view from the same data.
#include <game/render/render_view.h>
#include <game/render/frame_extrap.inc>
#include <game/ovl/ovl_reloc.h>
#include <game/debug_hooks.h>
#include <game/ovl/ovl.h>
#include <game/render/mrp_lod.h>
#include <game/render/render_toggles.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/ee_journal.h>

#include <vector>

#include <glm/gtc/quaternion.hpp>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <cmath>

namespace rac1 {

bool g_view_from_game = false;
bool g_view_debug = false;
glm::vec3 g_view_debug_eye{0.0f}, g_view_debug_at{0.0f};

// The game's view rotation 0x166c80 maps a world offset d (as a row vector, d * M) to the view
// space x right, y down, z forward; GL wants x right, y up, z backward, so its rows are M's
// columns 0, -1, -2, with the translation of the camera position 0x166dc0.
// The view's game inputs captured for a pipelined frame (render_view_snapshot), used while g_draw_from_snapshot
namespace {
struct ViewSnap {
    glm::mat4 view{1.0f};
    float tx = 0, ty = 0;
    bool depth_ok = false;
    double da = 0, db = 0;
    glm::vec3 cam{0.0f};
} g_vs;
glm::mat4 view_matrix_live();
glm::vec3 camera_live();
bool gs_depth_live(double& a, double& b);
}  // namespace

// orig: (host) see render_view.h
void render_view_snapshot() {
    g_vs.view = view_matrix_live();
    g_vs.tx = ee_read<float>(OV(0x0016caf0));
    g_vs.ty = ee_read<float>(OV(0x0016caf4));
    g_vs.depth_ok = gs_depth_live(g_vs.da, g_vs.db);
    g_vs.cam = camera_live();
}

// orig: (host) the GL view from the matrices of 0x001f6e48
namespace {
// the frame extrapolation's state (render_view.h): the views of the last two pipelined steps, the view in force while a begin
struct ExtrapView {
    uint64_t step = 0;  // (the frame being prepared)
    struct Step {
        bool ok = false;
        uint64_t frame = 0;
        glm::mat4 view{1.0f};
    } prev, cur;
    bool on = false;
    uint64_t frame = 0;
    double fraction = 0.0;
    glm::mat4 view{1.0f};
} g_ex;
}  // namespace

glm::mat4 render_view_matrix() {
    if (g_draw_from_snapshot && g_ex.on) return g_ex.view;
    return g_draw_from_snapshot ? g_vs.view : view_matrix_live();
}

bool g_frame_extrap = false;
// orig: (host) see render_view.h
void render_view_extrap_step(uint64_t frame) { g_ex.step = frame; }
uint64_t render_view_extrap_step_frame() { return g_ex.step; }
void render_view_extrap_note() {
    if (g_ex.cur.ok && g_ex.cur.frame == g_ex.step) {  // (the same step again: its view replaced)
        g_ex.cur.view = g_vs.view;
        return;
    }
    g_ex.prev = g_ex.cur;
    g_ex.cur.ok = true;
    g_ex.cur.frame = g_ex.step;
    g_ex.cur.view = g_vs.view;
}
bool render_view_extrap_affine(const glm::dmat4& prev, const glm::dmat4& cur, double gap, double ahead, double max_move, double max_turn_deg, glm::dmat4& out) {
    out = cur;
    if (!(gap > 0.0) || std::fabs(prev[3].w) < 1e-12 || std::fabs(cur[3].w) < 1e-12) return false;
    const glm::dvec3 tp = glm::dvec3(prev[3]) / prev[3].w, tc = glm::dvec3(cur[3]) / cur[3].w;
    const glm::dvec3 v = (tc - tp) / gap;  // (a step's motion)
    if (!(glm::length(v) <= max_move)) return false;
    // the turn from prev to cur: their rotations (the axes made orthonormal, the handedness kept), cur's times prev's inverse
    auto ortho = [](const glm::dmat4& m, glm::dmat3& r) {
        glm::dvec3 a(m[0]), b(m[1]), c(m[2]);
        const double la = glm::length(a);
        if (!(la > 1e-12)) return false;
        a /= la;
        b -= a * glm::dot(a, b);
        const double lb = glm::length(b);
        if (!(lb > 1e-12)) return false;
        b /= lb;
        const glm::dvec3 n = glm::cross(a, b);
        r = glm::dmat3(a, b, glm::dot(n, c) < 0.0 ? -n : n);
        return true;
    };
    glm::dmat3 rp, rc;
    if (!ortho(prev, rp) || !ortho(cur, rc)) return false;
    const glm::dmat3 d = rc * glm::transpose(rp);
    if (!(glm::determinant(d) > 0.5)) return false;
    const glm::dquat q = glm::normalize(glm::quat_cast(d));
    double angle = 2.0 * std::acos(std::min(1.0, std::fabs(q.w)));  // (the shorter way round)
    glm::dvec3 axis(q.x, q.y, q.z);
    if (q.w < 0.0) axis = -axis;
    const double al = glm::length(axis);
    const double k = ahead / gap;  // (steps ahead in the history's steps)
    glm::dmat3 turn(1.0);
    if (al > 1e-12 && angle > 1e-9) {
        if (!(angle / gap <= glm::radians(max_turn_deg))) return false;
        turn = glm::mat3_cast(glm::angleAxis(angle * k, axis / al));
    }
    const glm::dmat3 m = turn * glm::dmat3(cur);
    out = glm::dmat4(m);
    out[3] = glm::dvec4(tc + v * ahead, 1.0);
    return true;
}
bool render_view_extrap_begin(uint64_t frame, double fraction) {
    g_ex.on = false;
    if (!g_ex.cur.ok || !g_ex.prev.ok || g_ex.cur.frame != frame || g_ex.prev.frame >= frame || frame - g_ex.prev.frame > 2) return false;
    // the camera's world transform (the view's inverse) carried forward, the view its inverse again. A cut: more than 4 units
    // or 30 degrees a step (the follow camera turns a few degrees a step at most; Ratchet runs ~0.2 units a step)
    const glm::dmat4 cp = glm::inverse(glm::dmat4(g_ex.prev.view)), cc = glm::inverse(glm::dmat4(g_ex.cur.view));
    glm::dmat4 ce;
    if (!render_view_extrap_affine(cp, cc, static_cast<double>(frame - g_ex.prev.frame), fraction, 4.0, 30.0, ce)) return false;
    g_ex.view = glm::mat4(glm::inverse(ce));
    g_ex.frame = frame;
    g_ex.fraction = fraction;
    g_ex.on = true;
    return true;
}
void render_view_extrap_end() { g_ex.on = false; }
bool render_view_extrap_active(uint64_t* frame, double* fraction) {
    if (frame != nullptr) *frame = g_ex.frame;
    if (fraction != nullptr) *fraction = g_ex.fraction;
    return g_ex.on;
}

namespace {
// orig: (host) render_view_matrix from the game's state now
glm::mat4 view_matrix_live() {
    glm::mat4 view(1.0f);
    if (g_view_debug) {  // gl lookAt, z up
        const glm::vec3 f = glm::normalize(g_view_debug_at - g_view_debug_eye);
        const glm::vec3 r = glm::normalize(glm::cross(f, glm::vec3(0.0f, 0.0f, 1.0f)));
        const glm::vec3 up = glm::cross(r, f);
        for (int i = 0; i < 3; ++i) {
            view[i][0] = r[i];
            view[i][1] = up[i];
            view[i][2] = -f[i];
        }
        view[3] = glm::vec4(-glm::dot(r, g_view_debug_eye), -glm::dot(up, g_view_debug_eye), glm::dot(f, g_view_debug_eye), 1.0f);
        return view;
    }
    if (!g_view_from_game) {
        for (int row = 0; row < 3; ++row) {
            for (int col = 0; col < 4; ++col) view[col][row] = ee_read<float>(OV(0x166c80) + row * 16 + col * 4);
        }
        return view;
    }
    const glm::vec3 cam = render_camera_position();
    const float sign[3] = {1.0f, -1.0f, -1.0f};
    for (int r = 0; r < 3; ++r) {
        glm::vec3 axis;
        for (int i = 0; i < 3; ++i) axis[i] = sign[r] * ee_read<float>(OV(0x166c80) + i * 16 + r * 4);
        view[0][r] = axis.x;
        view[1][r] = axis.y;
        view[2][r] = axis.z;
        view[3][r] = -glm::dot(axis, cam);
    }
    return view;
}
}  // namespace

// orig: (host) the projection scales of 0x001f7980's fov tangents
float render_proj_x(float aspect, float fov) {
    const float t = g_draw_from_snapshot ? g_vs.tx : ee_read<float>(OV(0x0016caf0));
    if (g_view_from_game && !g_view_debug && t > 0.0f) return 1.0f / t;  // widened in the game's own (render_view_projection)
    return 1.0f / (aspect * std::tan(fov * 0.5f));
}
// orig: (host) the projection scales of 0x001f7980's fov tangents
float render_proj_y(float fov) {
    const float t = g_draw_from_snapshot ? g_vs.ty : ee_read<float>(OV(0x0016caf4));
    if (g_view_from_game && !g_view_debug && t > 0.0f) return 1.0f / t;
    return 1.0f / std::tan(fov * 0.5f);
}

// orig: (host) the GL depth of the Z the VU code computes from 0x001f7980's projection
bool render_gs_depth(double& a, double& b) {
    if (g_draw_from_snapshot) {
        a = g_vs.da;
        b = g_vs.db;
        return g_vs.depth_ok;
    }
    return gs_depth_live(a, b);
}
namespace {
// orig: (host) render_gs_depth from the game's state now
bool gs_depth_live(double& a, double& b) {
    const double p22 = ee_read<float>(OV(0x0016cb28)), p23 = ee_read<float>(OV(0x0016cb2c)), p32 = ee_read<float>(OV(0x0016cb38));
    const double k = ee_read<float>(OV(0x0016cc50)), z0 = ee_read<float>(OV(0x0016cbe8));
    if (p23 == 0.0 || p32 == 0.0 || k == 0.0) return false;
    const double A = z0 + k * p22 / p23, B = k * p32 / (1024.0 * p23);  // Z = A + B / z (the VU1 rows x 1024, row 3 not)
    const double n = kRenderNear, f = kRenderFar;
    const double g22 = -(f + n) / (f - n), g32 = -2.0 * f * n / (f - n);  // window depth = 0.5 - g22 / 2 + g32 / 2 / z
    a = 0.5 - 0.5 * g22 - 0.5 * g32 * A / B;
    b = 0.5 * g32 / B;
    return true;
}
}  // namespace

float g_view_widen = 1.0f;
float g_view_fov_scale = 1.0f;

float g_hud_aspect = 0.0f;

namespace {
std::vector<std::pair<uint32_t, uint32_t>> g_layers;  // render_view_layer_note's ranges (the tags' addresses, inclusive)
std::vector<HudNote> g_hud_notes;                      // render_view_hud_note's ranges (inclusive)
std::vector<ProjNote> g_proj_notes;                    // render_view_proj_note's ranges (inclusive)
struct HudOpen {
    bool on = false;
    uint32_t at = 0;
    uint8_t side = 0;
    int8_t slot = -1;
} g_hud_open;  // render_view_hud_begin's pending range
// (the HUD's note, merged with the last one when it continues it on the same side, in the same run)
void hud_note_add(uint32_t first, uint32_t end, uint8_t side, int8_t slot) {
    if (end <= first || side == 0) return;
    if (!g_hud_notes.empty() && g_hud_notes.back().last + 1 == first && g_hud_notes.back().side == side && g_hud_notes.back().slot == slot) {
        g_hud_notes.back().last = end - 1;
        return;
    }
    g_hud_notes.push_back({first, end - 1, side, slot});
}
}  // namespace

// orig: (host) see render_view.h
bool render_view_hud_wide() { return g_hud_aspect > 4.0f / 3.0f && g_view_widen > 1.0f; }
float render_view_hud_shift(float fb_w) {
    if (!render_view_hud_wide()) return 0.0f;
    const float widen = std::min(g_hud_aspect * 0.75f, g_view_widen);  // (the HUD's width over the 4:3 frame's)
    return widen > 1.0f ? fb_w * 0.5f * (widen - 1.0f) : 0.0f;
}
void render_view_hud_note(uint32_t first, uint32_t end, uint8_t side) { hud_note_add(first, end, side, -1); }
std::vector<HudNote> render_view_hud_notes() { return g_hud_notes; }

// orig: (host) see render_view.h (the packet cursor 0x1611c0)
void render_view_hud_begin(uint8_t side) {
    g_hud_open.on = false;
    if (!render_view_hud_wide() || ee_read<int32_t>(OV(0x0015f5c4u)) == 5) return;
    g_hud_open.on = true;
    g_hud_open.at = ee_read<uint32_t>(OV(0x001611c0u));
    g_hud_open.side = side;
    g_hud_open.slot = static_cast<int8_t>(mrp_lod_shadow_slot());
}
void render_view_hud_end() {
    if (!g_hud_open.on) return;
    g_hud_open.on = false;
    if (static_cast<int8_t>(mrp_lod_shadow_slot()) != g_hud_open.slot) return;  // (a begin of another run: never seen)
    hud_note_add(g_hud_open.at, ee_read<uint32_t>(OV(0x001611c0u)), g_hud_open.side, g_hud_open.slot);
}

// orig: (host) see render_view.h
void render_view_proj_note(uint32_t first, uint32_t end) {
    if (end <= first) return;
    g_proj_notes.push_back({first, end - 1, static_cast<int8_t>(mrp_lod_shadow_slot())});
}
std::vector<ProjNote> render_view_proj_notes() { return g_proj_notes; }

void render_view_layers_begin() {
    g_layers.clear();
    g_hud_notes.clear();
    g_proj_notes.clear();
    g_hud_open.on = false;
}
void render_view_layer_note(uint32_t first, uint32_t end) {
    if (end <= first) return;
    if (!g_layers.empty() && g_layers.back().second + 1 == first) {  // (one layer's pieces in a row: one range)
        g_layers.back().second = end - 1;
        return;
    }
    g_layers.emplace_back(first, end - 1);
}
std::vector<std::pair<uint32_t, uint32_t>> render_view_layers() { return g_layers; }

namespace {
constexpr float kAspect43 = 4.0f / 3.0f;
// the frame's shape in the window: the window's own in widescreen (view mode 2), else
// g_view_frame_aspect (4:3, or 16:9 from Options > Graphics)
float frame_aspect(int win_w, int win_h) {
    if (g_view_mode == 2) return static_cast<float>(win_w) / static_cast<float>(win_h);
    return g_view_frame_aspect > kAspect43 ? g_view_frame_aspect : kAspect43;
}
// the window's widening of the 4:3 frame in the view mode (1 but in widescreen or a wider frame)
float view_widen(int win_w, int win_h) {
    if (win_w <= 0 || win_h <= 0) return 1.0f;
    return std::max(1.0f, frame_aspect(win_w, win_h) / kAspect43);
}
// the field of view's scale applied now: 1 (the game's own) on the Vendor's screens (game mode 5): there the Vendor's monitors are
// the scene's mobys, in its view, and the screens' 2D is placed on them in the game's own projection (their boxes projected by the
// game, 4:3): another field of view put the monitors elsewhere than their 2D (issue #68). The widescreen's widening keeps them
// together (the 2D centred on the 4:3 frame, the 3D only wider around it), so it stays
float fov_scale_now() { return ee_read<int32_t>(OV(0x0015f5c4)) == 5 ? 1.0f : g_view_fov_scale; }
}  // namespace

// orig: (host) the frame's rectangle in the window by the view mode (debug menu, Options > Graphics)
void render_view_rect(int win_w, int win_h, int& x, int& y, int& w, int& h) {
    x = 0;
    y = 0;
    w = win_w;
    h = win_h;
    g_view_widen = view_widen(win_w, win_h);
    if (win_w <= 0 || win_h <= 0) return;
    const float a = static_cast<float>(win_w) / static_cast<float>(win_h), fa = frame_aspect(win_w, win_h);
    if (g_view_mode == 1) {  // the frame's shape with bars
        if (a > fa) {
            w = static_cast<int>(static_cast<float>(win_h) * fa);
            x = (win_w - w) / 2;
        } else {
            h = static_cast<int>(static_cast<float>(win_w) / fa);
            y = (win_h - h) / 2;
        }
    }
}

// orig: (host) see render_view.h
void render_overscan_crop(float fb_w, float fb_h, int crop[4]) {
    crop[0] = crop[1] = crop[2] = crop[3] = 0;
    static const char* s_env = std::getenv("RAC1_OVERSCAN");
    if (s_env == nullptr) return;
    int m[4] = {0, 0, 0, 0};
    if (std::sscanf(s_env, "%d,%d,%d,%d", &m[0], &m[1], &m[2], &m[3]) == 4) {
        for (int i = 0; i < 4; ++i) crop[i] = std::max(0, m[i]);
        return;
    }
    if (std::atoi(s_env) == 0 || fb_w <= 0.0f || fb_h <= 0.0f) return;
    // the TV's overscan: the game shows its 512 x 416 frame at the top of the NTSC
    // display window DISPLAY DX 636 / DW 2560 VCK, DY 50 / DH 448 lines (0x151798, written by 0x1f3958), the nominal PS2
    // picture, about 90 % of the TV raster across: a TV's overscan reached its left, right and top edges, never its bottom
    // (32 lines under the drawn frame). 4 frame pixels on those three sides: the 2D the game parks there (the Suck Cannon's
    // gauge, 3 columns at x 509..511) hidden; every 2D element meant to be seen measured at least ~8 pixels in (the Vendor's
    // screens), the HUD ~15..20
    constexpr float kSide = 4.0f / 512.0f, kTop = 4.0f / 416.0f;
    crop[0] = crop[1] = static_cast<int>(fb_w * kSide + 0.5f);
    crop[2] = static_cast<int>(fb_h * kTop + 0.5f);
    crop[3] = 0;
}

// The values this host wrote to the tangent, the game's ones they came from and their y base
// (without the widening): the game keeps copies of it (the pause menu saves 0x16caf0 when it
// opens, 0x276368, and puts it back when it closes, 0x276bd0), and a copy put back must not be
// widened twice.
namespace {
struct FovWrite {
    float written, base, y_base;
};
FovWrite g_fov_writes[8] = {};
int g_fov_next = 0;

const FovWrite* fov_write_of(float tangent) {
    for (const FovWrite& f : g_fov_writes)
        if (f.base > 0.0f && f.written == tangent) return &f;
    return nullptr;
}

// the projection setup's y tangent made from the x one without the widescreen widening
float fov_y_base(float x_tangent) {
    const FovWrite* f = fov_write_of(x_tangent);
    return f != nullptr ? f->y_base : x_tangent;
}
}  // namespace

namespace {
int g_last_w = 0, g_last_h = 0;  // the window render_view_projection last saw
std::vector<EeByteRun> g_proj_runs;  // the game's bytes render_view_projection changed, put back before the next update
}  // namespace

// orig: (host) see render_view.h
float render_view_cull_widen() {
    const float cur = ee_read<float>(OV(0x0016caf0));
    if (!(cur > 0.0f) || g_last_w <= 0 || g_last_h <= 0) return 1.0f;
    if (fov_write_of(cur) != nullptr) return 1.0f;  // (the host's widened value is in place)
    return std::max(1.0f, fov_scale_now() * view_widen(g_last_w, g_last_h));
}

// orig: (host) see render_view.h
float render_view_cull_widen_y() {
    const float cur = ee_read<float>(OV(0x0016caf0));
    if (!(cur > 0.0f) || g_last_w <= 0 || g_last_h <= 0) return 1.0f;
    if (fov_write_of(cur) != nullptr) return 1.0f;  // (the host's widened value is in place)
    return std::max(1.0f, fov_scale_now());
}

// orig: (host) see render_view.h
void render_view_note_write(float written, float base) {
    g_fov_writes[g_fov_next] = {written, base, base * fov_scale_now()};
    g_fov_next = (g_fov_next + 1) % 8;
    g_debug_fov_y_base = &fov_y_base;
}

// orig: (host) the game's fov tangent 0x0016caf0 scaled, its projection rebuilt (0x001f7980)
void render_view_projection(int win_w, int win_h) {
    g_last_w = win_w;
    g_last_h = win_h;
    const float cur = ee_read<float>(OV(0x0016caf0));
    if (!(cur > 0.0f)) return;
    const FovWrite* f = fov_write_of(cur);
    const float base = f != nullptr ? f->base : cur;
    const float y_base = base * fov_scale_now();
    const float want = y_base * view_widen(win_w, win_h);
    if (want == cur && (f != nullptr || want == base)) return;
    g_fov_writes[g_fov_next] = {want, base, y_base};
    g_fov_next = (g_fov_next + 1) % 8;
    g_debug_fov_y_base = &fov_y_base;
    // journaled: the bytes it changes noted, put back before the next update (render_view_projection_undo)
    const bool journal = !ee_journal_active();
    if (journal) ee_journal_begin();
    ee_write<float>(OV(0x0016caf0), want);
    projection_setup();
    if (journal) ee_journal_commit(g_proj_runs);
}

// orig: (host) see render_view.h
size_t render_view_projection_undo() {
    size_t n = 0;
    for (auto it = g_proj_runs.rbegin(); it != g_proj_runs.rend(); ++it) {
        uint8_t* p = static_cast<uint8_t*>(ee_ram_ptr(it->addr));
        if (p == nullptr || std::memcmp(p, it->after.data(), it->after.size()) != 0) continue;  // (written since: left)
        std::memcpy(p, it->before.data(), it->before.size());
        ++n;
    }
    g_proj_runs.clear();
    return n;
}

// orig: (host) the camera position for culling
namespace {
// orig: (host) render_camera_position from the game's state now
glm::vec3 camera_live() {
    if (g_view_debug) return g_view_debug_eye;
    if (g_view_from_game) return {ee_read<float>(OV(0x166dc0)), ee_read<float>(OV(0x166dc4)), ee_read<float>(OV(0x166dc8))};
    const uint32_t active_cam = ee_read<uint32_t>(0x13f350 + 0x2080);
    const uint32_t a = active_cam != 0 ? active_cam + 0x10 : 0x01c00010u;
    return {ee_read<float>(a), ee_read<float>(a + 4), ee_read<float>(a + 8)};
}
}  // namespace
// orig: (host) see render_view.h
glm::vec3 render_camera_position() { return g_draw_from_snapshot ? g_vs.cam : camera_live(); }

}  // namespace rac1
