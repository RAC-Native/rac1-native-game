#ifndef RAC1_GAME_RENDER_RENDER_TOGGLES_H
#define RAC1_GAME_RENDER_RENDER_TOGGLES_H

namespace rac1 {

extern bool g_render_tfrag;
extern bool g_render_tie;
extern bool g_render_shrub;
extern bool g_render_moby;
extern bool g_render_sky;
// the effects (particles) layer of the debug menu: the MRP effects not drawn, the particle program's kicks dropped
extern bool g_render_fx;
// (the host's pipelined frame: the frame drawn after the next game frame's update) true while the host draws a frame whose
// game state was captured before that update: the draw-time functions that read the game's state (the view, the depth mapping,
// the LOD widening, the drop shadows) use what was captured then (render_view_snapshot, mrp_lod_snapshot, mrp_shadows_snapshot)
extern bool g_draw_from_snapshot;
// The original moby pass (0x250038, its VU0 / VU1 programs ported) runs, and the mobys are drawn from it: by the MRP
// (mrp_scene.h observes it) or by the faithful path (on by default; off = the host's GLB model drawer, RAC1_MOBY_VU1=0).
extern bool g_moby_vu1_pass;
// The world (tfrag / tie / shrub) and the sky drawn only by the original passes' chain through the ported VU1 programs (the sky's
// chain has none: EE-built GIF packets) and the GS replica (RAC1_WORLD_FAITHFUL=1, the debug menu): the host's
// native world and sky renderers are not run and the passes keep the chain they build. The reference the MRP world renderer is
// compared with.
extern bool g_world_faithful;
// The world drawn by the Modern Rendering Pipeline (RAC1_WORLD_MRP=1, the debug menu; mrp_world_renderer.h): the
// world passes keep their chain, the MRP decodes it and draws the world from its assets, the GS replica skips it.
extern bool g_world_mrp;
// the boot's title scene: its passes' chain drawn (by the MRP world when it draws, else by the ported VU1 programs and
// the GS replica) whatever g_world_faithful says: set by the host while the boot runs, unless g_title_scene_host
extern bool g_world_chain_forced;
// the fallback: the title's ties, shrubs and sky by the old host renderers (title_scene_host.h, // RAC1_TITLE_HOST=1), its
// passes' chain taken back as the levels' with the host world
extern bool g_title_scene_host;
enum class MobyRenderer {
    kGlb = 0,       // legacy GLB drawer
    kFaithful = 1,  // faithful VU1 pass through CPU + GS (default)
    kGpu = 2,       // modern GPU moby renderer
    kMrp = 3,       // the Modern Rendering Pipeline (ADR 0010): mrp_moby_renderer.h
};
extern MobyRenderer g_moby_renderer;
// Debug views (the host's debug menu): wireframe, the shading (0 normal, 1 vertex colours only:
// the baked lighting, 2 textures only, 3 flat white), the aspect ratio of the projection.
extern bool g_render_gs2d;  // the game's 2D GS packets (HUD, text)
extern bool g_render_wireframe;
extern int g_render_shade;
extern float g_render_aspect;
// The host's view of the game's 4:3 frame in the window (debug menu): 0 stretched to the
// window (default), 1 4:3 with bars, 2 widescreen (the 3D widened to the window, the game's
// frame - HUD, particles - kept 4:3 at the centre).
extern int g_view_mode;
// (Options > Graphics) the frame's shape in view modes 0 / 1: 4:3 (the original's) or
// wider (16:9: the 3D widened to it, the HUD kept 4:3 at its centre), stretched to the window (0)
// or with bars (1).
extern float g_view_frame_aspect;

} // namespace rac1

#endif // RAC1_GAME_RENDER_RENDER_TOGGLES_H
