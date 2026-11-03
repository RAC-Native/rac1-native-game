// Ray tracing, phases 0 and 2 (testing: RAC1_RT=1, Vulkan only; docs/rendering.md#ray-tracing-phase-2): the water's reflection found by a
// ray query against the level's static world (ties, shrubs, tfrags) instead of the cube map / planar view. Phase 2: the hit shaded
// from its own triangle (its texture, its vertices' lit colours as the world's draws combine them, the world's fog along the
// reflection's path; alpha-tested textures' texels looked at), traced at half resolution and upsampled in the sea's shader; the
// frame's picture where the hit lies in it (phase 0's colour) is RAC1_RT_SHADE=1 / 0. No mobys in it (phase 3). Host code: it reads
// the level's static data, the zones' mask, the lit colours and the fog's constants, never writes the game's state.
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

namespace vk {
struct GpuImage;
}

// A triangle's shading (phase 2: the hit shaded from it), 12 words in the shader's order (RtTri, sm_water_rt): its three vertices'
// texture coordinates (s, t as the GS divides them), the texture (bucket | layer << 8; 0xff in the bucket: none), log2 of
// sqrt(its texture area / its area in its cluster's space) (the texture's level at a hit: a ray cone), its three vertices' colour
// indices (ties / shrubs: in their instance's resident colours, tfrags: in the level's tfrag colours), the texture word as the
// world's draws have it (bit 0 no texture, 1..2 TFX, 3 TCC, 22..23 the mip levels - 1; bit 4 here: every texel opaque)
struct RtTri {
    float uv[6] = {};
    uint32_t tex = 0xffu;
    float density = 0.0f;
    uint32_t col[3] = {};
    uint32_t word = 1;
};
static_assert(sizeof(RtTri) == 48);
// A cluster: triangles (float3 positions, 32-bit indices) in its own space, and per triangle its shading.
struct RtCluster {
    std::vector<float> pos;
    std::vector<uint32_t> idx;
    std::vector<RtTri> tri;  // (one per triangle of idx; phase 2)
    uint8_t kind = 0;  // 0 tie packet, 1 shrub packet, 2 tfrag entry (or one of the merged forms: a class, a zone cell's entries)
    bool opaque = true;  // (no triangle whose texture has a texel the alpha test can fail: a candidate's alpha is not looked at)
};
// An instance of a cluster: its 3x4 row-major transform (world = M (x, y, z, 1)), its partition, its zone (`zmask` & the zones' byte
// at `zcell`: visible; zcell >= 0x80: always).
struct RtInstance {
    uint32_t cluster = 0;
    float xform[12] = {};
    uint16_t partition = 0;
    uint8_t zmask = 0xff, zcell = 0xff;
    uint32_t colour = 0;  // (its first colour in the resident tie / shrub colours; tfrags: 0, their triangles' indices are the level's)
};
struct RtLevelGeometry {
    int level = -1;
    std::vector<RtCluster> clusters;
    std::vector<RtInstance> instances;
    uint32_t partitions = 0;  // (the static ones: the zone cells' and the unzoned one; the dynamic one comes after)
    std::string source;       // (what went in: the report)
    std::vector<uint64_t> textures;  // (the world's texture arrays, RtTri::tex's buckets: their VkImageView)
};

// orig: (host) RAC1_RT=1 and the device has ray queries (Vulkan): the water's ray-traced reflection on
bool mrp_rt_active();
// orig: (host) RAC1_RT_CLUSTERS: 0 a cluster per packet / tfrag entry, 1 merged per class (ties, shrubs) and zone cell (tfrags; default)
int mrp_rt_cluster_mode();
// orig: (host) at a new level (the world renderer's load, the host's thread): the clusters built and compacted, the partitions written,
// the first top level built; the counts, the memory and the times in the log
void mrp_rt_level(RtLevelGeometry&& g);
// orig: (host) the frame's zones' mask (0x173d00, 0x80 bytes: read on the host's thread while the game's state is frozen)
void mrp_rt_note_zones(const uint8_t* zone);
// orig: (host) the lit colours the world renderer keeps (the resident tie / shrub ones, the tfrags'; their counts of changes: copied
// again when they differ) - the host's thread, before the frame's scene is captured
void mrp_rt_note_colours(const std::vector<uint32_t>& resident, uint64_t resident_changes, const std::vector<uint32_t>& tfrag, uint64_t tfrag_changes);
// orig: (host) the world's fog this frame (the ties' pass: F = clamp(fog[1] + depth x fog[0], fog[3], fog[2]), 0x16cc60; FOGCOL; whether
// the world draws its fog): the hit's colour fogged by the reflection's path as the world's draws fog theirs
void mrp_rt_note_fog(const float fog[4], uint32_t fogcol, bool on);
// orig: (host) once a frame before the seas are drawn, outside a rendering pass: the changed partitions written, the top level updated
void mrp_rt_frame();
// orig: (host) Vulkan: the descriptor set (set 2: binding 0 the top level) of this frame, its layout (0: none)
uint64_t mrp_rt_vk_set();
uint64_t mrp_rt_vk_set_layout();
// orig: (host) the push constant's mode for the seas' ray-traced variant: 1 on, + RAC1_RT_DEBUG x 2, + RAC1_RT_SHADE x 16, + the pass x 64
// (0 the trace in the sea's own shader, 1 the half-resolution trace into its image, 2 the sea reading that image); the shading rate (RAC1_RT_RES)
int mrp_rt_shader_mode(int pass = 0);
uint32_t mrp_rt_shading_rate();
// orig: (host) RAC1_RT_RES: 0 the trace at half resolution, upsampled in the sea's shader (default), 1 in every pixel, 2 the whole sea
// shaded once per 2 x 2 pixels (VK_KHR_fragment_shading_rate)
int mrp_rt_res();
// orig: (host) Vulkan, the half-resolution trace: its target for a `w` x `h` 3D (half each way; made, or made again at a new size before
// the frame binds a set - mrp_fx's first seas of a frame): the colour (RGBA16F: the reflection, its distance) and the depth; null when
// it cannot be made
vk::GpuImage* mrp_rt_half_target(int w, int h, vk::GpuImage** depth);
// orig: (host) Vulkan: this frame's set for the half-resolution trace's own pass (its binding 6 a stand-in: the half image is its target)
uint64_t mrp_rt_vk_set_trace();

}  // namespace rac1

#endif  // RAC1_GAME_RENDER_MRP_RT_H
