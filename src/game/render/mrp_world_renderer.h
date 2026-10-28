// The Modern Rendering Pipeline's world renderer (ADR 0010 step 2): tfrag, tie and shrub drawn from the MRP world
// assets (src/formats/world_mrp.h, assets/generated/mrp/world/level<N>/) with the frame's data. Host code: none of it is the
// game's.
//
// Inputs, nothing else:
//  - load time: the level's world asset (the packets, the tfrag images, their kick programs) and its tfrag / tie / shrub
//    textures (every mip) in texture arrays;
//  - per frame: the scene (mrp_world_scene.h): the world passes' chain decoded on a private VU1 - what they made visible,
// the instances' rows, lit colours, fog, the LOD morphs - evaluated by the CPU reference (proved against the original)
//    into the GIF packets the programs kick; their vertices with the positions before the projection taken back to the
//    world (the passes' projection inverted) and drawn in the world renderers' GL projection the MRP mobys share
//    (`g_mrp_world_projection` 0), or in the VU's projection onto the game's frame (1, the faithful path's own pixels: the
//    image diff's reference mode). The GS state (PRIM, TEX0 / TEX1 / CLAMP, TEST, ALPHA, ZBUF, FOGCOL) as the chain sets it,
//    in the fragment shader as for the MRP mobys (texture function, alpha test, the GS's mip level, fog).
// The world's chain is then not processed by the frame's chain reader (mrp_chain.h: its skip ranges), as the MRP mobys'.
#ifndef RAC1_GAME_RENDER_MRP_WORLD_RENDERER_H
#define RAC1_GAME_RENDER_MRP_WORLD_RENDERER_H

#include <cstdint>
#include <utility>
#include <vector>

namespace rac1 {

namespace gs {
class Gs;
struct DepthMap;
struct Viewport;
}  // namespace gs

// The switch (RAC1_WORLD_MRP=1, the debug menu): the world drawn by the MRP. Options: the projection (0 the world renderers'
// GL projection, shared with the MRP mobys; 1 the VU's onto the frame), the texture level rule (0 the GS's from Q, 1 every mip
// with the GPU's choice), the GS's fog (1 as the packets enable it, 0 none).
// (g_world_mrp: render_toggles.h)
extern int g_mrp_world_projection;
extern int g_mrp_world_mip;
extern int g_mrp_world_fog;

// orig: (host) true when the MRP draws this frame's world: the switch on and the level's assets loaded (GL ready)
bool mrp_world_ready();
// orig: (host) true when the MRP makes the ties / shrubs of a pass whose distances / frustum the LOD slider or the
// widening change itself (mrp_world_static.h), so that pass's shadow run is not needed (the world projection, RAC1_MRPW_STATIC on,
// not the testing modes)
bool mrp_world_static_lod();
// orig: (host) true when the MRP draws the tfrags on the GPU (mrp_tfrag_gpu.h) and makes the tfrag pass's decisions itself
// at LOD > 0 / widened, so its shadow run is not needed (the world projection, RAC1_MRPW_TFRAG_GPU on, not the testing modes)
bool mrp_world_tfrag_lod();
// orig: (host) per host frame, before the world is drawn: the level's assets (re)loaded when the level changed, the frame's
// scene built from the chain [chain_start, chain_end) (`active`: a frame whose world is drawn: not the title, a level exit,
// a movie, the full-screen menu)
void mrp_world_begin_frame(bool active, uint32_t chain_start, uint32_t chain_end);
#if defined(__WIIU__)
// orig: (host) the Wii U's frame (ports/wiiu/MRP_LITE.md phases 3 and 4; the walker's VU1 programs are not in its build): per host
// frame, after the game's frames ran and before the next update, on the game's thread: the level's assets (re)loaded when the level
// changed, the frame's ties / shrubs made by the host (mrp_world_static.h) and its tfrags decided by the host (mrp_tfrag_gpu.h
// tfrag_decide) into the scene (mrpw::scene_from_static), its draw lists built and the tfrags' vertices / triangles made by the CPU
// (the compute program's output, tfrag_frame_cpu), for ports/wiiu/game/mrp_world_gx2.cpp (`active`: as mrp_world_begin_frame's). No
// sky, shadows yet.
void mrp_world_wiiu_prepare(bool active);
// orig: (host) the Wii U: the preparation's second half (on its worker core) waited for - before the frame's world is read
void mrp_world_wiiu_join();
// (the Wii U: the preparation's second half on its worker - RAC1_WIIU_WORLD_ASYNC, the GamePad's debug menu flips it live)
extern bool g_wiiu_world_async;
// orig: (host) the Wii U: the frame's chain read (its reader joined, ports/wiiu/game/wiiu_gfx.cpp): its GS registers at the first world
// pass kept for the next frame's preparation, which then does not wait for the reader (RAC1_WIIU_REGS_LAST=0: it waits, as before)
void mrp_world_wiiu_chain_joined();
#endif
// orig: (host) the same in two halves: _async starts the scene's build (on a worker thread, RAC1_MRPW_THREAD=0 not),
// after it mrp_world_skip_ranges() is known; the host may then read the frame's chain (it only reads EE memory) and must
// call mrp_world_finish_frame() before anything else reads or writes EE memory, the scene, or the chain reader's GS registers'
// use by the world (the scene waited for, its triangles and draw lists built)
void mrp_world_begin_frame_async(bool active, uint32_t chain_start, uint32_t chain_end);
void mrp_world_finish_frame();
// orig: (host) true between _async and _finish when a scene is being built
bool mrp_world_pending();
// orig: (host) the scene's inputs are captured on the worker thread: the game's state must not change before this returns
void mrp_world_wait_captured();
// orig: (host) the pending frame's scene waited for and dropped (a pipelined frame not drawn)
void mrp_world_drop_frame();
// orig: (host) true when the frame's world is the MRP's (begin_frame built its scene)
bool mrp_world_active();
// orig: (host) draws the frame's world into the bound framebuffer, viewport `vp`
void mrp_world_draw(int win_w, int win_h, const gs::Viewport* vp);
// orig: (host) the viewport the last frame's world was drawn into (x, y, w, h: GL's, the render scale's buffer when scaled); false: none
bool mrp_world_last_viewport(int out[4]);
// orig: (host) the world passes' chain ranges (tags [first, last]) the chain's reader skips this frame
std::vector<std::pair<uint32_t, uint32_t>> mrp_world_skip_ranges();
// orig: (host) testing: the in-game check (RAC1_WORLD_MRP_CHECK=1) and image diff (RAC1_WORLD_MRP_DIFF=1) run the faithful
// world as well: true when the world programs' starts must be reported (mrp_world_program_start / _kick) and their GS
// primitives marked (the GS's moby flag: in these modes the mobys are the MRP's, none of the GS's)
bool mrp_world_testing();
bool mrp_world_program_start(uint64_t hash, uint32_t start);  // (true: a world program)
void mrp_world_program_kick(const uint8_t* data, uint32_t qwc);
// orig: (host) testing, after the GS chain: the check's comparison, the offscreen image diff (the faithful world's GS
// primitives against the MRP's, each alone)
void mrp_world_after_chain(int win_w, int win_h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth);
// orig: (host) the totals at the run's end
void mrp_world_report();

}  // namespace rac1

#endif
