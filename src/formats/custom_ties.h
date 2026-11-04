// A custom level's ties (testing, local experiment: RAC1_CUSTOM_LEVEL, runtime/kernel/disc.h): ties from another game's level
// (tools/custom_level/maktar.py: RAC2's arena of Maktar Nebula) that RAC1's tie pass does not draw - no EE data stands for
// them - kept in <dir>/ties.mrpc, our own container. `rac1-native --setup --custom-level <dir>` turns its classes into the MRP
// world's tie classes (world.mrpw: a kick program of our own per packet, for the GPU path's templates) and its textures into
// textures_tie.mrpt; the MRP world renderer draws the placements (World::custom_ties) itself, at the end of the world passes.
//
// ties.mrpc (little-endian):
//   u32 magic "MRPC", u32 version 1
//   u32 textures; each: u32 width, height; 1024 bytes the palette (CT32, the GS's CSM1 order, as GS RAM holds it); u32 mips,
//       each u32 bytes + the T8 texels (linear)
//   u32 classes; each: i32 id; f32 scale; f32 sphere x y z r (s16 units / 1024); u32 local textures, each {u32 the texture,
//       u32 CLAMP's wrap bits (WMS / WMT), u64 ALPHA_1}; u32 packets, each: u8 lod, u8 index in its lod, u16 0, u32 dinky,
//       u32 fat vertices; the dinky ones s16 x y z 0 s t q 0 (s t q / 4096), the fat ones s16 delta x y z 0, base x y z 0,
//       s t q 0; u32 strips, each {u32 local texture, u32 n, n u16 vertices (dinky 0.., then fat), padded to 4 bytes}
//   u32 instances; each: u32 the class, f32 the rotation's three rows, f32 the position, u8 RGBA
// A vertex's world position: position + (rotation rows) x (s16 x scale / 1024), as RAC1's tie instances (data +0x30 w the scale).
#ifndef RAC1_FORMATS_CUSTOM_TIES_H
#define RAC1_FORMATS_CUSTOM_TIES_H

#include <formats/moby_mrp.h>
#include <formats/world_mrp.h>

#include <string>
#include <vector>

namespace rac1::mrpw {

struct CustomTies {
    std::vector<mrp::Texture> textures;     // (the level's tie textures: textures_tie.mrpt)
    std::vector<TieClass> classes;          // as the MRP world's tie classes
    std::vector<CustomTieInstance> instances;
};

// orig: (host) reads `path`; false (with `error`) when it is not one. The classes' packets get the kick program of RAC1's tie A
// (micro 0x000) with our own GIF stream: per strip an A+D tag and texture slot when its texture changes, then a strip tag (PRIM
// 0x7c: a triangle strip, Gouraud, textured, fogged, blended; STQ, RGBAQ, XYZF2) and its vertices, colour 0 each (the
// instance's single colour).
bool read_custom_ties(const std::string& path, CustomTies& out, std::string& error);

}  // namespace rac1::mrpw

#endif
