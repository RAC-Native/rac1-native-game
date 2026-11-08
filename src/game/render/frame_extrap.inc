// Frame extrapolation (prototype; docs/env-vars.md RAC1_EXTRAP). Host code: none of it is the game's.
//
// More pictures a second than the game's 60 steps, with no latency added and no interpolation: in a host frame that draws its
// pipelined frame beside the game thread, the host presents that frame at its slot, then draws it again with its camera and its
// mobys' places carried forward by a fraction of a step along the motion of the two steps before it, presented between it and the
// next step's picture. Nothing of the game's state is read or written: the view's history is the snapshots the pipelined frames
// took (render_view_snapshot), the mobys' the draw lists the MRP built from them; the drawing reads them while
// g_draw_from_snapshot, as the frame's first picture does. (An .inc, not a .h, and kept out of render_view.h and the renderers'
// headers: every src/game/**/*.h is an input of the generated code's fingerprint, tools/setup.py CODE_SOURCES, and a change
// there asks for ./setup.sh again; no generated code includes this.)
#pragma once

#include <glm/glm.hpp>

#include <cstdint>

namespace rac1 {

// the extrapolation asked for this frame (the host: the debug menu's Display tab, RAC1_EXTRAP): the renderers keep the history it
// needs only then (nothing at all while it is off)
extern bool g_frame_extrap;

// ---- the view (render_view.cpp) ----
// orig: (host) the frame `frame` being prepared (every prepare, before its view snapshot and moby lists): the step the next
// notes and builds belong to
void render_view_extrap_step(uint64_t frame);
uint64_t render_view_extrap_step_frame();
// orig: (host) after render_view_snapshot: the snapshot's view kept as the newest step of the history (the one before it
// becomes the previous)
void render_view_extrap_note();
// orig: (host) the draws that follow (g_draw_from_snapshot) show the snapshot of step `frame` carried `fraction` (0..1) of a step
// forward: render_view_matrix the view extrapolated from the previous noted step. kExtrapOk, else why not (nothing changed): no
// step before `frame` in the history, a gap of more than 4 steps, or a camera cut - a jump in place or in direction (more than 12
// units or 75 degrees a step, or 4 times the step before's motion and a unit / 15 degrees past it)
enum { kExtrapOk = 0, kExtrapNoHistory = 1, kExtrapGap = 2, kExtrapMove = 3, kExtrapTurn = 4 };
int render_view_extrap_begin(uint64_t frame, double fraction);
void render_view_extrap_end();
// orig: (host) whether a begin is in force, its step and fraction
bool render_view_extrap_active(uint64_t* frame = nullptr, double* fraction = nullptr);
// orig: (host) an affine transform (column-major, translation in [3]) carried forward: from `prev` (`gap` steps before `cur`)
// through `cur` by `ahead` steps, turning about the same axis at the same rate and moving at the same velocity, the rest of
// `cur`'s matrix (its scale) kept. False (out = cur) when it moved more than `max_move` units or turned more than `max_turn_deg`
// degrees a step, or a matrix is degenerate
bool render_view_extrap_affine(const glm::dmat4& prev, const glm::dmat4& cur, double gap, double ahead, double max_move, double max_turn_deg,
                               glm::dmat4& out);

// ---- the renderers ----
// orig: (host) mrp_world_renderer.cpp: the built frame drawn again with the view in force - the ties' and shrubs' instance clip
// matrices moved to it ((vp now) (vp built)^-1; the rest of the world takes the view-projection at its draw), with none in force
// the build's own put back; uploaded again at the next draw (Vulkan's frame rings are the frame's own)
void mrp_world_extrap_apply();
// orig: (host) mrp_moby_renderer.cpp: the built frame's parts' clip matrices made again for the view in force and, while an
// extrapolation is in force, each moby's place carried forward from the step before (its first part's world matrix's motion; the
// pose stays the step's; a moby new this step or that jumped stays); with none in force the frame's own clips put back. Uploaded
// again (OpenGL: now; Vulkan: at the next draw)
void mrp_moby_extrap_apply();

}  // namespace rac1
