// Options > Graphics > Water Reflections (enhancement; docs/materials.md#water-reflections): what Surface Detail's water mirrors, in
// place of the screen's last picture (screen-space reflections broke as soon as the camera moved: a hole under Ratchet and under
// every object in front of the water, flashing - Daino at Rilgar's harbour, 2026-11-02).
//  - Low: a cube map of the sky and the world around the water, drawn from the camera at the first frame the water is seen, and drawn
//    again over itself only when the view has turned or moved well away (the frame holds only what the game's view sees: the rest is
//    kept from the views before; started anew for another body of water): between those, still; nothing that moves.
//  - Medium: the planar view - the frame's world, sky and mobys drawn again mirrored about the water's plane (the flat sea covering
//    the most of the view, at the height Surface Detail holds it still), cut at the water, at half the 3D's resolution - read at the
//    water's own pixel, moved a little by its ripples. Where the water does not lie at that plane (another body, a sloping stream),
//    or the planar view drew nothing there, the cube map.
// High is left for a ray-traced mode (none yet). Host code: it draws what the frame holds again, the game's state is not read for it
// beyond the frame's own drawing and never written.
#ifndef RAC1_GAME_RENDER_MRP_REFLECT_H
#define RAC1_GAME_RENDER_MRP_REFLECT_H

namespace rac1 {
namespace vk {
struct GpuImage;
}

// orig: (host) 0 none (Graphics: Original), 1 Low (the cube map), 2 Medium (the planar view and the cube map): the Graphics page's
// row (RAC1_WATER_REFLECTIONS=1|2 overrides it, testing). Only with Surface Detail on (its water material draws them)
extern int g_water_reflections;
// orig: (host) build_vu7 (mrp_fx.cpp): a batch of a sea tagged `water` drawn still - its height (a flat one's; else its mean), its
// share of the view (the clip square's area its triangles cover, 0..4), whether it is flat. The next frame's planar view takes the
// flat height with the largest share
void mrp_reflect_note_water(float h, float area, bool flat);
// orig: (host) after the world is drawn (the 3D's target bound, `vp` its viewport: x, y, w, h): the cube map made if it is due, the
// planar view drawn (Medium); the target and the viewport put back
void mrp_reflect_pass(const int vp[4]);
// orig: (host) the materials' uniforms (uMat2[253], [254], mrp_materials_glsl.inc sm_water_refl) for draws into the viewport `vp`
void mrp_reflect_uniforms(const int vp[4], float a[4], float b[4]);
// orig: (host) Vulkan: the planar view and the cube map's faces for the lighting's set (bindings 8, 9; nullptr: not made yet)
const vk::GpuImage* mrp_reflect_vk_planar();
const vk::GpuImage* mrp_reflect_vk_cube();
// orig: (host) a new level (mrp_fx_clear_caches): the cube map made again, the notes dropped
void mrp_reflect_level_reset();
// orig: (host) the report at the end of a run (RAC1_REFLECT_STATS=1): planar views and cube maps drawn, the planes chosen
void mrp_reflect_report();

}  // namespace rac1

#endif  // RAC1_GAME_RENDER_MRP_REFLECT_H
