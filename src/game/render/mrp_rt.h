// Ray tracing, phase 0 (testing: RAC1_RT=1, Vulkan only; docs/rendering.md#ray-tracing-phase-0): the water's reflection found by a
// ray query against the level's static world (ties, shrubs, tfrags) instead of the cube map / planar view, its colour taken from the
// frame's own picture where the hit lies in it (its depth matching), else the cube map / the sky. No hit shading yet (phase 2), no
// mobys in it (phase 3). Host code: it reads the level's static data and the zones' mask, never writes the game's state.
//
// The acceleration structures are organised as DirectX's clustered geometry and partitioned TLAS (CLAS / PTLAS) are, with the plain
// KHR acceleration structures (the real cluster / partition extensions are NVIDIA's alone, VK_NV_cluster_acceleration_structure and
// VK_NV_partitioned_acceleration_structure; logged at the start when present, not used):
//  - clusters: the game's geometry is cluster-sized already: each tie / shrub packet template (its finest LOD) and each tfrag entry
//    (its near image) is a small bottom-level structure built once at the level's load and compacted; the instances reuse them.
//  - partitions: the static instances grouped by the game's zone cell (ties, tfrags; shrubs have none: a partition of their own), a
//    dynamic partition for the moving instances (empty: the mobys are phase 3). Per frame only the partitions whose zone visibility
//    changed (and the dynamic one) are written again, and the top level is updated (or rebuilt: RAC1_RT_TLAS).
//  - behind an interface (RtAccel, mrp_rt.cpp) so that a real cluster / partitioned extension can be added as another back end.
#ifndef RAC1_GAME_RENDER_MRP_RT_H
#define RAC1_GAME_RENDER_MRP_RT_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// A cluster: triangles (float3 positions, 32-bit indices) in its own space.
struct RtCluster {
    std::vector<float> pos;
    std::vector<uint32_t> idx;
    uint8_t kind = 0;  // 0 tie packet, 1 shrub packet, 2 tfrag entry (or one of the merged forms: a class, a zone cell's entries)
};
// An instance of a cluster: its 3x4 row-major transform (world = M (x, y, z, 1)), its partition, its zone (`zmask` & the zones' byte
// at `zcell`: visible; zcell >= 0x80: always).
struct RtInstance {
    uint32_t cluster = 0;
    float xform[12] = {};
    uint16_t partition = 0;
    uint8_t zmask = 0xff, zcell = 0xff;
    uint32_t custom = 0;  // (the instance's custom index: its kind << 22 | its number - the hit's shading, phase 2)
};
struct RtLevelGeometry {
    int level = -1;
    std::vector<RtCluster> clusters;
    std::vector<RtInstance> instances;
    uint32_t partitions = 0;  // (the static ones: the zone cells' and the unzoned one; the dynamic one comes after)
    std::string source;       // (what went in: the report)
};

// orig: (host) RAC1_RT=1 and the device has ray queries (Vulkan): the water's ray-traced reflection on
bool mrp_rt_active();
// orig: (host) RAC1_RT_CLUSTERS: 0 a cluster per packet / tfrag entry (default), 1 merged per class (ties, shrubs) and zone cell (tfrags)
int mrp_rt_cluster_mode();
// orig: (host) at a new level (the world renderer's load, the host's thread): the clusters built and compacted, the partitions written,
// the first top level built; the counts, the memory and the times in the log
void mrp_rt_level(RtLevelGeometry&& g);
// orig: (host) the frame's zones' mask (0x173d00, 0x80 bytes: read on the host's thread while the game's state is frozen)
void mrp_rt_note_zones(const uint8_t* zone);
// orig: (host) once a frame before the seas are drawn, outside a rendering pass: the changed partitions written, the top level updated
void mrp_rt_frame();
// orig: (host) Vulkan: the descriptor set (set 2: binding 0 the top level) of this frame, its layout (0: none)
uint64_t mrp_rt_vk_set();
uint64_t mrp_rt_vk_set_layout();
// orig: (host) the push constant's mode for the seas' ray-traced variant (1 on, + RAC1_RT_DEBUG x 2) and the shading rate (RAC1_RT_RES)
int mrp_rt_shader_mode();
uint32_t mrp_rt_shading_rate();

}  // namespace rac1

#endif  // RAC1_GAME_RENDER_MRP_RT_H
