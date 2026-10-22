// The host renderer's view: the GL view matrix and camera position the static and moby renderers
// draw with. With g_view_from_game set (the original runtime) they come from the game's own view
// setup 0x1f6e48 (the rotation 0x166c80 and the camera position 0x166dc0); otherwise from the
// rows the legacy host camera writes at 0x166c80 (translation included).
#pragma once

#include <glm/glm.hpp>

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace rac1 {

extern bool g_view_from_game;
// A host debug view (a camera looking at a point) drawn instead of the game's, when set.
extern bool g_view_debug;
extern glm::vec3 g_view_debug_eye, g_view_debug_at;

glm::mat4 render_view_matrix();
// The GL projection's x / y scales: with the game's view, its fov tangents (0x16caf0 x,
// 0x16caf4 y: the screen's half width / height over the view distance, as its VU code projects);
// otherwise the host's `fov` (vertical) and aspect.
float render_proj_x(float aspect, float fov);
// The window rectangle the frame is drawn in (bottom-left origin) and the horizontal widening of
// the 3D (> 1 wider than 4:3) for the view mode g_view_mode; set each frame by the host.
void render_view_rect(int win_w, int win_h, int& x, int& y, int& w, int& h);
// the margins (left, right, top, bottom, in the game's frame pixels, fb_w x fb_h) the frame's 2D is cut at: none (the
// full frame, always shown since the Graphics page's Overscan row was removed). RAC1_OVERSCAN (testing): 0 none, 1 the
// overscan a TV hid, "l,r,t,b" those margins.
void render_overscan_crop(float fb_w, float fb_h, int crop[4]);
extern float g_view_widen;
// The field of view scale (debug menu, 1 the game's).
extern float g_view_fov_scale;
// The game's x fov tangent 0x16caf0 times the fov scale and, in widescreen, the window's widening,
// the game's projection rebuilt (0x1f7980): its culling, its VU-drawn 3D (particles, glass) and
// the native 3D are one projection over the whole window. Follows the game's own writes of the
// tangent. Nothing changes with the defaults (4:3, scale 1). Called by the host each frame.
void render_view_projection(int win_w, int win_h);
// orig: (host) the game's memory render_view_projection changed (the tangent, projection_setup's outputs) put back as the
// game left it, before the next game frame's update: the game never sees the host's widened projection (its state stays the
// one of a 4:3 run: before, a cutscene that reads the tangent before writing its own - Veldin's intro at frame 162 - kept the
// widened value and the game's state diverged). Returns the runs of bytes put back.
size_t render_view_projection_undo();
float render_proj_y(float fov);
// The host renderers' depth range (their GL projections' near / far).
constexpr float kRenderNear = 0.1f, kRenderFar = 4000.0f;
// The host window depth of a GS Z written by the game's VU code (Z = A + B / view depth, A and B
// from the projection 0x16cb00, the VU constants 0x16cc50.x / 0x16cbe0.z): depth = a + b * Z.
// False before the game's projection is set.
bool render_gs_depth(double& a, double& b);
glm::vec3 render_camera_position();

// Widescreen: how much wider the 3D is drawn than the game's own frustum test sees it: the x tangent 0x16caf0 the
// game's code reads now (the game writes its own 4:3 value back every update: the camera, the cutscenes) against the one
// render_view_projection makes of it for the drawing (fov scale x widening); >= 1.
float render_view_cull_widen();
// The same for the y tangent: the field of view's scale (render_view_projection widens the y tangent by it, not by the
// widescreen's widening); >= 1.
float render_view_cull_widen_y();

// orig: (host) (testing, RAC1_WIDEN_REF): a widened x tangent `written` the host wrote over the game's `base` elsewhere
// (mrp_lod_frame_begin), noted as render_view_projection's own writes are (not widened again, the y tangent the base's)
void render_view_note_write(float written, float base);

// orig: (host) the view's game inputs (the view matrix, the projection tangents, the depth mapping, the camera) captured
// now for a frame drawn later (render_toggles.h g_draw_from_snapshot)
void render_view_snapshot();

// orig: (host) the full-screen 2D layers the game draws in pieces over its whole 4:3 frame from a draw list's callback (the
// Visibomb's out-of-range static, class 832's callback on every level that has it: a grid of 32 x 32 sprites): in a widened
// view the GS replica stretches them across the window (gs::Primitive::layer). render_frame clears the notes
// (render_view_layers_begin); the draw lists' runner notes each such callback's chain range [first, end) of the real block
// (render_view_layer_note: the shadow runs' are not noted); the chain's stage takes them for the replica (the tag ranges,
// inclusive). Nothing of the game's state is read or written but the packet cursor's values the caller passes.
void render_view_layers_begin();
void render_view_layer_note(uint32_t first, uint32_t end);
std::vector<std::pair<uint32_t, uint32_t>> render_view_layers();

// orig: (host) the HUD's width (Options > Graphics > HUD): 0 the original's 4:3 frame (the default), else the aspect ratio the HUD
// spreads over (16:9, 21:9, 32:9; a very large value: the view's whole width), never wider than the view (the widened 3D's).
extern float g_hud_aspect;
// orig: (host) how far (in the game's frame pixels) the HUD's side elements move toward their edges in a frame `fb_w` pixels wide:
// 0 at 4:3 (g_hud_aspect 0, or a view not wider than 4:3), else half the frame's width times the HUD's widening (min(g_hud_aspect,
// the view's aspect) over 4:3) less one
float render_view_hud_shift(float fb_w);
// orig: (host) the HUD elements the frame drew at the frame's sides, for a HUD wider than 4:3 (gs::Primitive::hud): render_frame
// clears the notes (render_view_layers_begin), the HUD's drawing (hud_draw, ...) notes each side element's chain range
// [first, end) (side 1 left, 2 right; the packet cursor's values before and after its draw function); the chain's stage takes them
// for the replica (the tag ranges, inclusive). Nothing of the game's state is read or written but the cursor values the caller passes.
void render_view_hud_note(uint32_t first, uint32_t end, uint8_t side);
struct HudNote {
    uint32_t first, last;
    uint8_t side;
};
std::vector<HudNote> render_view_hud_notes();
// whether the HUD's notes are wanted now (a HUD wider than 4:3 over a widened view): the callers skip their bookkeeping otherwise
bool render_view_hud_wide();

}  // namespace rac1
