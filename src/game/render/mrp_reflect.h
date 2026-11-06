// Options > Graphics > Water Reflections (enhancement; docs/materials.md#water-reflections): what Surface Detail's water mirrors, in
// place of the screen's last picture (screen-space reflections broke as soon as the camera moved: a hole under Ratchet and under
// every object in front of the water, flashing - Daino at Rilgar's harbour, 2026-11-02).
//  - Low: cube maps from fixed probes - one per zone of the level's ground plan (128 units square) and body of water, over the water
//    at the zone's middle - of the sky and the scenery past some tens of units of the probe (nothing near: posts, crates, the dock),
//    made the first time the camera is in that zone, filled in where they hold nothing yet as the view turns, never drawn over; read
//    along the reflected ray met with a sphere around the probe. Nothing that moves; the reflection never moves on its own (a cube
//    map from the camera put near objects far out on the water and shifted at every new drawing - Daino at Rilgar's dock, 2026-11-03).
//  - Medium: the planar view - the frame's world, sky and mobys drawn again mirrored about the water's plane (the flat sea covering
//    the most of the view, at the height Surface Detail holds it still), cut at the water, at half the 3D's resolution, unlit, over the
//    rectangle the water covers alone, the ties and shrubs outside it or under the water left out - read at the
//    water's own pixel, moved a little by its ripples. Where the water does not lie at that plane (another body, a sloping stream),
//    or the planar view drew nothing there, the cube map.
//  - Medium (RT): the reflection ray traced through the level's static world and shaded from the hit's own triangle (mrp_rt.h,
//    docs/rendering.md#ray-tracing-phase-2), shown only where the renderer can trace (Vulkan with ray queries).
//  - High (RT): the same with the frame's mobys in the trace - Ratchet, the ships - (docs/rendering.md#ray-tracing-phase-3-the-mobys).
// Host code: it draws what the frame holds again, the game's state is not read for it
// beyond the frame's own drawing and never written.
#ifndef RAC1_GAME_RENDER_MRP_REFLECT_H
#define RAC1_GAME_RENDER_MRP_REFLECT_H

namespace rac1 {
namespace vk {
struct GpuImage;
}

// orig: (host) 0 none (Graphics: Original), 1 Low (the cube map), 2 Medium (the planar view and the cube map), 3 Medium (RT) (the
// reflection ray traced through the static world, mrp_rt.h: Vulkan with ray queries only, else played as Medium), 4 High (RT) (the
// mobys in the trace too): the Graphics page's row (RAC1_WATER_REFLECTIONS=1..4 overrides it, testing). Only with Surface Detail on
// (its water material draws them)
extern int g_water_reflections;
// orig: (host) the planar view drawn at 1 / this of the 3D's size (2, half: the default; RAC1_REFLECT_SCALE, the debug menu's
// Enhancements tab - not saved: 3 looked the same at Rilgar's dock and harbour for 40 % less)
extern int g_reflect_scale;
// orig: (host) build_vu7 (mrp_fx.cpp): a batch of a sea tagged `water` drawn still - its height (a flat one's; else its mean), its
// share of the view (the clip square's area its triangles cover, 0..4), whether it is flat, the clip square's rectangle they cover
// (x0, y0, x1, y1; empty: x0 > x1). The next frame's planar view takes the flat height with the largest share, and draws only the
// rectangle its batches cover
void mrp_reflect_note_water(float h, float area, bool flat, const float box[4]);
// orig: (host) after the world is drawn (the 3D's target bound, `vp` its viewport: x, y, w, h): the cube map made if it is due, the
// planar view drawn (Medium); the target and the viewport put back
void mrp_reflect_pass(const int vp[4]);
// orig: (host) the materials' uniforms (uMat2[253], [254], [252], [251]; mrp_materials_glsl.inc sm_water_refl) for draws into the
// viewport `vp`: c the probe in use (x, y, z, the radius of the sphere its reflection is met with; 0: none), d.x its first layer, d.y
// the sky's (-1: none)
void mrp_reflect_uniforms(const int vp[4], float a[4], float b[4], float c[4], float d[4]);
// orig: (host) Vulkan: the planar view and the cube map's faces for the lighting's set (bindings 8, 9; nullptr: not made yet)
const vk::GpuImage* mrp_reflect_vk_planar();
const vk::GpuImage* mrp_reflect_vk_cube();
// orig: (host) a new level (mrp_fx_clear_caches): the cube map made again, the notes dropped
void mrp_reflect_level_reset();
// orig: (host) the report at the end of a run (RAC1_REFLECT_STATS=1): planar views and cube maps drawn, the planes chosen
void mrp_reflect_report();

}  // namespace rac1

#endif  // RAC1_GAME_RENDER_MRP_REFLECT_H
