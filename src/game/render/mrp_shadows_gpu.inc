// The drop shadows made by the host from the game's own shadow shapes (RAC1_WIIU_SHADOWS_GPU, 2026-10-30: the Wii U's default, off
// elsewhere): the game's shadow code (moby_shadows 0x24fc38's shadow_points 0x285a68 and drop_shadows_draw 0x285870: each deferred
// moby's pose again, VU0 program 0x190, the tubes' quads and their GS packets) skipped, and in its place the same shapes - per moby of
// the deferred list 0x1ac600 the spheres / two-joint capsules of its class's shadow list (class +0x14), through its pose, scaled by
// moby +0x2c and the list's size word, the slab between moby +0x84 and +0x88 and the shadow direction 0x1aeb80 + 16 x moby +0xbd - kept
// as host data (DropShadowFrame). The renderer extrudes each shape along the direction into a closed prism between the slab's two
// planes - the original's tube, a ring of points cut by those planes (shadow_points_clip) - and darkens the surfaces inside: on the PC
// through mrp_shadows' stencil counter (a check of the shapes), on the Wii U through a counter in a mask of its own and a soft
// darkening pass (ports/wiiu/game/mrp_world_gx2.cpp, mrp_world_gx2_draw_shadows). What the skipped code wrote and nothing else reads:
// 0x1ac780 (the shadow entries), 0x160420..0x16046c, the scratchpad, the VU0's registers, the packet buffer's shadow packets (the
// packet pointer 0x1611c0 then stands lower by their size). A .inc, not a header: the generated code's fingerprint.
#pragma once

#include <cstdint>
#include <vector>

namespace rac1 {

// one shape: a sphere (c1 unused) or a capsule between c0 and c1, the radii in world units
struct DropShadowPrim {
    float c0[3] = {}, r0 = 0.0f;
    float c1[3] = {}, r1 = 0.0f;
    bool capsule = false;
};
// a deferred moby's shapes: [first, first + count) of DropShadowFrame::prims, the direction (unit, toward the ground) and the slab
struct DropShadowCaster {
    uint32_t moby = 0;
    float dir[3] = {0.0f, 0.0f, -1.0f};
    float z_near = 0.0f, z_far = 0.0f;  // the planes the near and the far ends of the tube are cut to (moby +0x88, +0x84)
    uint32_t first = 0, count = 0;
};
struct DropShadowFrame {
    std::vector<DropShadowCaster> casters;
    std::vector<DropShadowPrim> prims;
    uint64_t serial = 0;
    // (the capture's numbers: the poses taken from the MRP's scene, those built again by the game's routine)
    uint32_t scene_poses = 0, game_poses = 0;
};

// the prisms of a frame as triangles (9 floats each, world space), split by the side they show the camera at `cam` (the faces toward
// it: front, else back); `inside`: the casters whose prism holds the camera (their back faces alone are seen)
struct DropShadowVolumes {
    std::vector<float> front, back;
    uint32_t inside = 0;
    float lo[3] = {}, hi[3] = {};  // (their bounds)
    std::vector<float> boxes;      // (each caster's: lo x, y, z, hi x, y, z - the renderer's rectangles)
    uint32_t prisms = 0;
};

// orig: (host) 1 the game's shadow code skipped and the shapes captured (RAC1_WIIU_SHADOWS_GPU: 1 on the Wii U, 0 elsewhere); the
// Wii U's debug menu flips it (the game's code runs again the next frame)
extern int g_drop_shadows_gpu;
// orig: (host) moby_shadows' hook: when it returns true the game's shadow_points / drop_shadows_draw are not run (moby_shadows.cpp)
extern bool (*g_drop_shadows_native)();
// orig: (host) the frame the renderer draws (mrp_shadows_snapshot's copy; nullptr: none this frame)
const DropShadowFrame* mrp_drop_shadows_view();
// orig: (host) the prisms of `f` for the camera at `cam` (ring of `ring` points per sphere, rounder for larger ones)
void mrp_drop_shadows_volumes(const DropShadowFrame& f, const float cam[3], int ring, DropShadowVolumes& out);
// orig: (host) the camera's world point of a view-projection (column-major, clip = M p): where clip x, y and w are 0. False: none
bool mrp_drop_shadows_camera(const float* view_proj, float cam[3]);

}  // namespace rac1
