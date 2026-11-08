// A level mod's host meshes (docs/mods.md, "Level mods"): meshes no EE data stands for, which the MRP world renderer draws
// itself as tie classes at their placements. The setup's level composer (src/setup/level_compose.h) writes them from the
// mod's OBJ meshes into the level's build (assets/generated/mods/<mod>/host_meshes.mrpc); the MRP world export of the
// composed level adds their classes to its world.mrpw and their textures to its textures_tie.mrpt.
//
// host_meshes.mrpc (little-endian):
//   u32 magic "MRPC", u32 version 2 (1 read too: no palettes)
//   u32 textures; each: u32 width, height; 1024 bytes the palette (CT32, the GS's CSM1 order, as GS RAM holds it, alpha
//       0..0x80); u32 mips, each u32 bytes + the T8 texels (linear)
//   u32 classes; each: i32 id; f32 scale; f32 sphere x y z r (vertex units / 1024); u32 local textures, each {u32 the
//       texture, u32 CLAMP's wrap bits (WMS / WMT), u64 ALPHA_1}; u32 packets, each: u8 lod, u8 index in its lod, u16 0,
//       u32 dinky, u32 fat vertices; the dinky ones s16 x y z c s t q 0 (s t q / 4096; c the vertex's colour, an index
//       into its instance's palette), the fat ones s16 delta x y z 0,
//       base x y z 0, s t q 0; u32 strips, each {u32 local texture, u32 n, n u16 vertices (dinky 0.., then fat), padded
//       to 4 bytes}
//   u32 instances; each: u32 the class, f32 the rotation's three rows, f32 the position, u8 RGBA; (version 2) u32 palette
//       colours (at most 64), each u8 RGBA (0x80 = 1.0): the vertices' colours (the indices past it: the RGBA)
// A vertex's world position: position + (rotation rows) x (s16 x scale / 1024), as RAC1's tie instances (data +0x30 w
// the scale).
#ifndef RAC1_FORMATS_HOST_MESHES_H
#define RAC1_FORMATS_HOST_MESHES_H

#include <formats/moby_mrp.h>
#include <formats/world_mrp.h>

#include <array>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1::mrpw {

// What the file holds, as written
struct HostMeshStrip {
    uint32_t local_texture = 0;
    std::vector<uint16_t> verts;
};
struct HostMeshPacket {
    uint8_t lod = 0, index_in_lod = 0;
    std::vector<std::array<int16_t, 8>> dinky;
    std::vector<std::array<int16_t, 12>> fat;
    std::vector<HostMeshStrip> strips;
};
struct HostMeshTexRef {
    uint32_t texture = 0;  // the file's texture
    uint32_t wrap = 0;     // CLAMP's WMS / WMT bits (0: repeat)
    uint64_t alpha = 0x44; // ALPHA_1 (0x44: (Cs - Cd) x As + Cd)
};
struct HostMeshClass {
    int32_t id = 0;
    float scale = 1.0f;
    float sphere[4] = {};
    std::vector<HostMeshTexRef> textures;
    std::vector<HostMeshPacket> packets;
};
struct HostMeshFile {
    std::vector<mrp::Texture> textures;
    std::vector<HostMeshClass> classes;
    std::vector<HostMeshInstance> instances;
};

// orig: (host) `f` written to `path`; false (with `error`) when it cannot be
bool write_host_meshes(const std::string& path, const HostMeshFile& f, std::string& error);

// The file read as the MRP world's tie classes
struct HostMeshes {
    std::vector<mrp::Texture> textures;  // (the level's tie textures: textures_tie.mrpt)
    std::vector<TieClass> classes;       // as the MRP world's tie classes
    std::vector<HostMeshInstance> instances;
};

// orig: (host) reads `path`; false (with `error`) when it is not one. The classes' packets get the kick program of RAC1's
// tie A (micro 0x000) with our own GIF stream: per strip an A+D tag and texture slot when its texture changes, then a
// strip tag (PRIM 0x7c: a triangle strip, Gouraud, textured, fogged, blended; STQ, RGBAQ, XYZF2) and its vertices,
// each its colour index (the instance's 64 colours: its palette, then its RGBA).
bool read_host_meshes(const std::string& path, HostMeshes& out, std::string& error);

// The world whose host meshes the MRP world's scene draws (mrp_world_renderer.cpp binds it with its index at a level's load;
// null: none), and the tie pass's fog constants the frame's scene capture read for them (scale, offset, ceiling, floor)
// orig: (host)
void host_meshes_bind(const void* index, const World* world);
// orig: (host) the world bound with `index`, else null
const World* host_meshes_bound(const void* index);
// orig: (host) a world is bound
bool host_meshes_bound_any();
// orig: (host)
void host_meshes_fog_set(const float fog[4]);
void host_meshes_fog(float fog[4]);

}  // namespace rac1::mrpw

#endif
