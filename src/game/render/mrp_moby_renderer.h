// The Modern Rendering Pipeline's moby renderer (ADR 0010 step 1: "the MRP draw with
// original lighting"). Host code: none of it is the game's.
//
// Inputs, nothing else:
// - load time: the level's MRP moby assets (
//    assets/generated/mrp/moby/level<N>/<o_class>.mrpm + textures.mrpt, the armor's) decoded into
//    GPU buffers once: per class and LOD the VU1 vertex slots with their skinning records (the
//    effective VU0 matrix of each slot resolved to <= 3 weighted joints), the kick programs'
//    triangles as index runs per texture prim, the textures in arrays with every mip;
// - per frame: the MRP scene state (mrp_scene.h): the mobys the original drew, their
//    parts, model-to-view rows, joint matrices, light rows, tints, the projection block. One upload
//    (joints, parts, draw records), the draws in the chain's order with consecutive draws of the
//    same GL state merged into one glMultiDrawElementsIndirect.
// Shaders: the vertex shader is mrp::skin_packet (the VU0 skinning + lighting, the EE's fixed-point
// colour step included) followed by the VU1 program's transform (mrp::vu1_vertex / kick_triangles);
// the fragment shader applies the draw state the game set (the GS's texture function and alpha test,
// the full texture chain or, with Texture Detail Original, the GS's per-pixel mip level as gs_draw.cpp has it);
// the GPU rasterises.
#ifndef RAC1_GAME_RENDER_MRP_MOBY_RENDERER_H
#define RAC1_GAME_RENDER_MRP_MOBY_RENDERER_H

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rac1 {

namespace gs {
class Gs;
struct DepthMap;
struct Viewport;
}  // namespace gs

// Options (debug menu, env): the projection the mobys are drawn with (0 the world renderers' GL
// projection: one depth space with tfrag / tie / shrub; 1 the game's own VU1 projection onto its 4:3
// frame, as the faithful path draws), the texture level rule (0 the GS's per-pixel level from Q with
// TEX1's K / L / MXL, "faithful mip"; 1 every mip of the asset with the GPU's trilinear choice, "full
// texture").
extern int g_mrp_moby_projection;
// mrp_moby_draw keeps the scissor the host set (the full-screen menu's mobys cut at its 4:3 frame)
extern bool g_mrp_moby_keep_scissor;
extern int g_mrp_moby_mip;
// The GS's fog on the mobys: 1 (default) the fogging the GS does with the prim's FGE and the FOGCOL register from the vertices' F (the VU1
// programs' fog: the moby's, or per vertex in the clipping program), 0 none (RAC1_MRP_FOG=0, the debug menu)
extern int g_mrp_moby_fog;

// orig: (host) true when the MRP draws this frame's mobys (the renderer chosen and the level's assets
// loaded): the faithful VU1 moby programs are then not run
// The import list (the open engine, docs/open-engine.md): another level's classes a session brought in, drawn from their
// own level's class files (mrp/moby/levelNN/<class>.mrpm) with their textures appended to the level's. A moby of one is
// matched as any class (its header's class id), its source found by the session (session_class_source). The list is
// read at the next frame (the level's moby assets loaded again with them).
// orig: (host) class `cls` of level `level` added to the import list
void mrp_moby_import(int level, uint32_t cls);
// orig: (host) the import list emptied
void mrp_moby_imports_clear();
// orig: (host) class `cls` of level `level` taken off the import list (a session's class unloaded: its file and textures
// dropped at the next frame, with the level's moby assets loaded again)
void mrp_moby_import_remove(int level, uint32_t cls);
// orig: (host) the import list's length
int mrp_moby_imports_count();
bool mrp_moby_active();
// orig: (host) true when the renderer is the MRP and its assets for the level now running are loaded (GL ready):
// the frame's mobys will be drawn by it (asked before the game's update, to skip what only the GS /
// VU1 path needs)
bool mrp_moby_ready();
// orig: (host) the world renderers' view-projection (column-major, 16 floats): what the MRP draws the mobys and the shadows with
void mrp_world_view_proj(float out[16]);
// orig: (host) per host frame, before the GS chain: the level's assets (re)loaded when the level
// changed, the frame's activity decided (`title`: the boot's title scene, drawn by the faithful pass; `menu_mode`: the full-screen
// menu's frame, every moby in the game's own 4:3 projection; `title_menu`: the boot's title main menu, the mobys the game drew from
// another camera than the scene's (MrpMoby::other_view: the menu's boxes) in the game's own 4:3 projection cut at its 4:3 frame,
// the scene's mobys in the world's)
void mrp_moby_begin_frame(bool title, bool menu_mode, bool title_menu = false);
// orig: (host) draws the scene's mobys (mrp_scene()) into the bound framebuffer, viewport `vp`
// orig: (host) the frame's draw lists built and uploaded now (from the scene and the game's state), for a frame drawn after
// the next update (mrp_moby_draw then only draws)
void mrp_moby_prepare(const gs::Viewport* vp);
void mrp_moby_draw(int win_w, int win_h, const gs::Viewport* vp);
// orig: (host) the moby at `moby` that the game drew into a render target (MrpMoby::rt: the menus' item models, 0x24ff08) drawn
// into the bound framebuffer, the target (w x h, one pixel per GS pixel, its depth buffer attached), with the view the draw loop sent at its
// call, the target's XYOFFSET and ZBUF (the GS's state at the moby's chain, Gs::SkipRange::host_3d); mrp_moby_draw leaves these mobys out
void mrp_moby_draw_target(uint32_t moby, uint64_t xyoffset, uint64_t zbuf, int w, int h);
// orig: (host) the differential image check (RAC1_MRP_DIFF=1): the faithful pass's moby primitives
// (gs::draw, the moby filter) and the MRP's, each alone in an offscreen buffer, compared per pixel
void mrp_moby_diff(int win_w, int win_h, const gs::Viewport* vp, const gs::Gs& gs, float fb_w, float fb_h, uint64_t frame, const gs::DepthMap* depth);
// orig: (host) the differential check's bookkeeping: called before and after each faithful moby program
// start with the GS's primitive count, so that each scene part's GS triangles can be compared with the
// MRP's kick program (RAC1_MRP_DIFF with RAC1_MRP_CHECK)
void mrp_moby_diff_program(size_t primitives);
// orig: (host) the totals of the checks (diff, shader check) at the run's end
void mrp_moby_report();

// The frame's mobys for the ray tracing's moby pass (Options > Graphics > Water Reflections High (RT): mrp_rt.cpp; Vulkan): the GPU
// buffers the moby draws read (the level's vertices and indices; the frame's joints, parts, draw commands, normal table: their
// VkBuffer, offset, size) and the draws of the mobys drawn in the world's view (on screen, and the shadows' off-screen casters; not
// the menus' render targets nor the title menu's boxes), in the frame's order: a moby's parts follow one another
struct MrpMobyRtBuf {
    uint64_t buffer = 0, offset = 0, size = 0;
};
struct MrpMobyRtFrame {
    MrpMobyRtBuf verts, indices, joints, parts, cmds, normals;
    std::vector<uint32_t> runs;        // 4 words a draw: its first index, its index count, its draw command, its part
    std::vector<int32_t> run_bucket;   // per draw its texture array (-1: none)
    std::vector<float> part_world;     // 16 a part: its model -> world matrix (column-major)
    std::vector<uint32_t> part_moby;   // per part the moby it is of
    std::vector<uint64_t> part_pose;   // per part a hash of its pose (its world matrix, its joints): the same when its triangles are
    std::vector<uint64_t> buckets;     // the texture arrays' VkImageView (sampler2DArray, every mip)
};
// orig: (host) the frame's mobys as above (the frame's buffers uploaded if they are not yet); false when the MRP draws no mobys in the
// world's view this frame (OpenGL, a menu, the title)
bool mrp_moby_rt_frame(MrpMobyRtFrame& out);

}  // namespace rac1

#endif
