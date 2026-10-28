// The MRP's level of detail slider ("Level of detail slider").
// Host code: an enhancement, none of it is the game's.
//
// The world passes (tfrag 0x294240, tie 0x295c80, shrub 0x286e60) decide each frame, in EE code, which entries / instances are
// drawn and at which level of detail, from distances they read in EE memory. The game's own run
// keeps those decisions. With the slider off its original position, after each real pass the same ported pass is run a second
// time - the "shadow" run - on a journal of EE memory (ee_journal_begin / restore: every page it reaches is saved first and
// put back after, the scratchpad, the VU0 registers and the DMA registers too), with the distances it reads scaled, and its
// chain written to a host-only region above the game's 32 MB; that chain is copied out and the MRP's scene walker decodes it in
// place of the pass's own range (mrp_world_scene.h scene_note_shadow). The game's state after the frame is the original's
// (proved by RAC1_RAMHASH runs: the hashes of EE RAM and the scratchpad after every update, identical with the slider at max).
#ifndef RAC1_GAME_RENDER_MRP_LOD_H
#define RAC1_GAME_RENDER_MRP_LOD_H

#include <cstdint>
#include <vector>

namespace rac1 {

// The slider (RAC1_MRP_LOD=0..100, the debug menu's Rendering tab): 0 the original (no shadow run: today's image), 1..99 the
// distances scaled by mrp_lod_scale(), 100 no LOD at all (every entry / instance at its nearest level of detail, and drawn up
// to the renderer's far plane).
extern int g_mrp_lod;
// The distances' scale of a slider position: 1 at 0, 16^(v / 100) between, 0 (= unlimited) at 100.
float mrp_lod_scale(int v);
// The widescreen part: when the 3D is drawn wider than the game's own frustum test sees it (render_view_cull_widen() > 1:
// widescreen, a wider field of view), the shadow runs - also of the sky pass - widen the passes' frustum, whatever the slider
// (automatic). RAC1_MRP_WIDEN=0 / this flag: off (testing).
extern bool g_mrp_lod_widen_off;
// "Fill the widescreen sides" (debug menu, RAC1_WIDE_FILL=0/1, default on): the shadow runs' widening - the world (tfrag,
// tie, shrub, the sky), the mobys and the 3D effects (mrp_lod_effects_shadow) - drawn in the side bands a 4:3 game culls. Off: the
// sides show only what the 4:3 game drew (no CPU cost: ~11 ms a frame at 5120 x 1440 on Veldin); the LOD
// slider still works. The game's state is the 4:3 one either way (render_view_projection_undo is always on).
extern bool g_mrp_wide_fill;
// the hook for the MRP static world: set when the world (tfrag / tie / shrub, not the sky) is culled by
// something else than the game's passes (e.g. on the GPU): the world's shadow runs are then not made for the widening (the LOD
// slider's still are, unless that renderer takes it over too: then set g_mrp_world_lod_external as well).
extern bool g_mrp_world_wide_external;
extern bool g_mrp_world_lod_external;
// orig: (host) whether the world pass `kind` (0 tfrag, 1 tie, 2 shrub, 3 the sky) needs its shadow run this frame (the LOD slider off 0,
// or the widening with the sides filled), and the widening it gets (1: none)
bool mrp_lod_world_shadows_needed(int kind, float* widen = nullptr);

// false while the host runs a game frame whose picture it will not draw (several updates in one host frame: only
// the last one's scene is drawn): the shadow runs, which only make the drawn scene, are not run for it. The main loop sets it.
extern bool g_mrp_lod_frame_drawn;
// orig: (host) the widening measured at the last update's tfrag pass (1 without the MRP world): the game's projection is the
// 4:3 one during the update; the host's GS replica draws the game's projected 3D (the VU-kicked particles, glass) of that frame
// narrowed by it to sit in the MRP's widened 3D (orig_main.cpp, gs::Viewport::xscale_3d)
float mrp_lod_frame_widen();
// orig: (host) the frame's widening captured for a frame drawn after the next update (render_toggles.h g_draw_from_snapshot)
void mrp_lod_snapshot();

// orig: (host) before the real world pass `kind` (0 tfrag, 1 tie, 2 shrub, 3 the sky), the frame packet cursor at `cur`: when the slider
// (or the widening) asks for it and the MRP draws the world, the pass run first as its shadow from the very state the real pass
// will start from (the passes read scratchpad / RAM their previous runs left: a second run after the real one would not be the
// same pass), its chain kept; then mrp_lod_world_after once the real pass ran to [cur, real_end): the shadow chain handed to the
// scene in place of that range.
void mrp_lod_world_shadow(int kind, void (*pass)(), uint32_t cur);
void mrp_lod_world_after(int kind, uint32_t cur, uint32_t real_end);
// orig: (host) before the real moby pass 0x250038 (`pass`): when the MRP draws the mobys and the slider or the widening asks for
// it, the pass run first as its shadow (the same journal), its mobys' draw distances (+0x32, the loop's maximum 0x15fff0) and LOD
// distances (+0x72) scaled and its frustum widened; the MRP scene observed during that run (mrp_scene_shadow_begin / end) is
// what the MRP draws this frame (mrp_scene_draw()): its mobys at their near model, with the poses the pass computed for them.
// The drop shadows stay the real pass's.
void mrp_lod_moby_shadow(void (*pass)());
// orig: (host) the real moby pass `pass` run with its frustum widened by `w` (the Wii U's widescreen, render_view.h g_view_draw_widen: no
// shadow run - its journal of the whole pass costs too much there): patch_widen's tangent, projection_setup outputs and clip matrix
// (the rows A the pass projects with kept), put back after the pass where the pass left them. The game's state then differs from a 4:3
// run's in what the pass writes for the mobys in the side bands (their +0x31 "drawn" byte, their draw loop's work); w <= 1: the pass
// as it is
void mrp_lod_moby_widen_real(void (*pass)(), float w);
// orig: (host) the same for the real sky pass (0x23d968: it projects its shells itself, culling its clusters to its frustum): the whole
// projection widened (widen_projection: the rows A too, the sky's screen points in the widened view), the A it projected with kept for the
// sky's capture (mrpw::sky_capture: mrp_lod_sky_real_a, nullptr when the last sky pass was not widened)
void mrp_lod_sky_widen_real(void (*pass)(), float w);
const float* mrp_lod_sky_real_a();

// orig: (host) the 3D effects the GS replica draws (render_frame's block after the moby pass: the draw lists' callbacks - the
// glows of the lights, the ships' canopy shine, through quad_draw 0x1fd228 -, the lens flare, the decals, the particles 0x267c98,
// the sparkles) in a widened view. The particles are culled against the game's 4:3 frustum (the tangent 0x16caf0 / the radius
// factor 0x16cc20: vf26 / vf27); the lights' glows (light_group_glows_draw 0x2df3d8) are drawn only for the mobys the moby pass drew
// this frame (moby +0x31, set by the draw loop 0x2559c0 when the moby is on screen). Before the real block (packet cursor `cur`), when
// the view is wider than the game's frustum and the MRP draws the world, the block is run as a shadow (the journal, as the world's)
// with the tangent widened (patch_cull: the sphere test, not the projections A the block projects with: what it draws stays in the
// game's 4:3 frame, past its sides for what is beyond them), the draw lists' clip matrix widened (mrp_lod_draw_list_widen) and the
// mobys' +0x31 as the moby pass's shadow left them; mrp_lod_effects_after keeps its chain; mrp_lod_substitutes hands it to the GS
// replica (gs::Gs::add_chain_substitute), whose projected 3D the window's side bands then show (gs_draw.cpp: past the frame's sides).
void mrp_lod_effects_shadow(void (*block)(), uint32_t cur);
void mrp_lod_effects_after(uint32_t cur, uint32_t real_end);
// orig: (host) the same for the draw lists render_frame runs between the world passes (0x16dc80 after the ties, 0x16de80 after the
// shrubs; `list` 0 / 1): the levels' water and lava surfaces (Aridia's sand sea L2:0x2ea048, the water grids of Rilgar, Pokitaru,
// Hoven, Gemlik Base, Umbris...) are queued there, and their cells / tiles are culled against the 4:3 clip matrix 0x166d80 (VCLIP /
// clip_outcode of their corners). Before the real block (packet cursor `cur`) it is run as a shadow (the journal) with that matrix
// widened (mrp_lod_draw_list_widen) and the sphere test's tangent (patch_cull); its chain stands in for the block's in the GS replica
// as the effects shadow's does. Run whenever the list is not empty and the view is widened (nothing to predict it by).
void mrp_lod_list_shadow(int list, void (*block)(), uint32_t cur);
void mrp_lod_list_after(int list, uint32_t cur, uint32_t real_end);
// orig: (host) the last drawn update's shadow chains, in the chain's order (the lists' blocks, then the effects'): the real range
// [first, end), the shadow's bytes, and `slot` (0 / 1 the lists, 2 the effects: mrp_lod_shadow_slot). Moved out: once per frame.
struct MrpLodSubstitute {
    int slot = 0;
    uint32_t first = 0, end = 0;
    std::vector<uint8_t> bytes;
};
void mrp_lod_substitutes(std::vector<MrpLodSubstitute>& out);
// orig: (host) the widening of the view the GS replica's chain is drawn into, x / y (1 1: none; the shadow runs' fill_widen /
// fill_widen_y): handed to the replica (gs::Gs::set_side_widen) for the VU1 programs' tests that drop a primitive wholly past the
// game's 4:3 sides. VU1 program 7 (shadows_vu1.cpp: the drop shadows, the prim lists of the levels' water and sand sea) drops a
// clipped triangle past 4 / 3 of the frame's half width, which a 32:9 view shows (Aridia's sand sea near the camera at the far left).
void mrp_lod_side_widen(float out[2]);
// orig: (host) the shadow block running now: -1 none, 0 / 1 the draw lists' (mrp_lod_list_shadow), 2 the effects'
constexpr int kMrpLodSlotEffects = 2, kMrpLodSlots = 3;
int mrp_lod_shadow_slot();
// orig: (host) true while the effects shadow's block runs; the widening it was made with
bool mrp_lod_in_effects_shadow();
float mrp_lod_effects_widen();
// orig: (host) around the draw lists' callbacks (render_passes.cpp draw_list_run): inside a shadow block (the lists' or the effects'),
// the clip matrix 0x166d80 (render_view_setup 0x1f6e48: the view's frustum, |x| / |y| <= w; the callbacks' VCLIP / clip_outcode
// cull tests) with its x and y columns divided by the widening (fill_widen / fill_widen_y), put back after the list. The projections
// the callbacks draw with (0x166cc0..0x166d7f) are left alone. Outside a shadow: nothing.
void mrp_lod_draw_list_widen(bool begin);

// orig: (host) around the body of projection_setup 0x1f7980 (`view` false) and render_view_setup 0x1f6e48 (`view` true)
// when the game's code calls them inside a shadow run whose pass runs on a patched projection (the effects shadow's widened cull
// tangent, the world / moby shadows' widened projection): the game's call gets the game's own tangent, and the shadow's patch is put
// back after it, so what the rest of the pass projects stays in the game's own 4:3 projection (the rows A 0x166cc0..) as in the real
// pass. Outside a shadow run (the real passes, the game's update) it does nothing.
class MrpLodSetupScope {
public:
    explicit MrpLodSetupScope(bool view);
    ~MrpLodSetupScope();
    MrpLodSetupScope(const MrpLodSetupScope&) = delete;
    MrpLodSetupScope& operator=(const MrpLodSetupScope&) = delete;

private:
    bool view_;
    bool on_ = false;
};

// orig: (host), testing only (RAC1_WIDEN_REF=1): at render_frame's start the game's own projection widened by the view's
// widening (the frame then culled and projected by the game itself over the whole window, no shadow run): the reference image the
// shadow runs are compared with. It changes the game's state (its culling decides things the updates read): never on by default.
void mrp_lod_frame_begin();
bool mrp_lod_reference();

// orig: (host) the totals at the run's end (RAC1_FRAME_PROF / RAC1_MRP_LOD_VERIFY)
void mrp_lod_report();

}  // namespace rac1

#endif
