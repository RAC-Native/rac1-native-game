// The MRP moby renderer's boundary between its CPU preparation and a GPU backend. Host code.
//
// mrp_moby_renderer.cpp prepares, with no GPU: the level's classes as GPU vertices (GpuVertex) and triangle indices, its textures
// as arrays per size (the buckets), and per frame the parts drawn (PartGpu), one draw record and one draw command per texture run
// (DrawCmd: the record's index in `base_instance`), the runs of draws sharing one GPU state (Batch / Key), in the chain's order.
// Its GL and Vulkan backends are inline in that file; the Wii U's GX2 one (ports/wiiu/game/mrp_moby_gx2.cpp) reads the same
// data through the accessors below.
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_materials_glsl.inc.)
#ifndef RAC1_GAME_RENDER_MRP_MOBY_BACKEND_INC
#define RAC1_GAME_RENDER_MRP_MOBY_BACKEND_INC

#include <game/render/mrp_scene.h>

#include <cstddef>
#include <cstdint>
#include <vector>

namespace rac1::mrpm {

// One GPU vertex: a VU1 slot of a packet as the kick program sends it (its position, colour, UV are
// the slot's: the asset's kick ops never mix slots but for STQ's second Q factor, kept as q2).
// The slot's skinning record: bind position, normal angles, vertex colour, the VU0 matrix it is
// skinned with as <= 3 weighted joints (n 1: the joint's matrix itself; 2 / 3: the VU0 blend
// M[j0] w0 + M[j1] w1 (+ M[j2] w2), in that order). (Host byte order: big-endian on the Wii U.)
struct GpuVertex {
    int16_t pos[3];
    uint16_t normal;  // the angle bytes n0 | n1 << 8
    uint8_t joint[3];
    uint8_t n;
    uint8_t weight[3];
    uint8_t flags;  // 1: STQ x the Q of the q2 slot
    uint8_t rgba[4];
    int16_t uv[2];
    int16_t q2pos[3];
    int16_t pad0;
    uint8_t q2joint[3];
    uint8_t q2n;
    uint8_t q2weight[3];
    uint8_t pad1;
};
static_assert(sizeof(GpuVertex) == 40);

// One drawn part (a VU1 program start of a moby's chain): what the vertex shader reads of it
struct PartGpu {
    float clip[16];   // the world's projection: model -> GL clip (columns)
    float rows[16];   // the VU1 rows R0, R1, R2, T
    float light[32];  // the skinning record's rows (+0x40..+0xbf)
    float sub[4];     // the sub-mesh colour (bytes)
    int32_t info[4];  // the moby's first joint (in the frame's joints), its joint count, flags (1: the sub-mesh colour), 1: the clipping program
};
static_assert(sizeof(PartGpu) == 4 * (16 + 16 + 32 + 4 + 4));

struct DrawCmd {  // DrawElementsIndirectCommand
    uint32_t count, instances, first_index;
    int32_t base_vertex;
    uint32_t base_instance;  // the draw's record: the frame's cmds[4 * base_instance ..] (part, texture word, layer, 0)
};

// the GL state of a run of draws
struct Key {
    int bucket = -1;
    uint32_t sampler = 0;
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    bool deferred = false;  // the draw loop's second pass (the mobys whose drop shadow is seen): drawn after the shadows
    bool caster_only = false;  // a moby outside the view (the casters' cache): drawn into the shadow maps only
    uint32_t rt = 0;  // a moby drawn into a render target (MrpMoby::rt): its address; drawn there by mrp_moby_draw_target only
    // the boot's title main menu's boxes (MrpMoby::other_view, Frame::title_menu): drawn in the game's own 4:3 projection (mode 1)
    // over the scene's mobys in the world's, cut at the 4:3 frame; never casters
    bool own_view = false;
    bool operator==(const Key&) const = default;
};
struct Batch {
    Key key;
    uint32_t first = 0, count = 0;  // into the frame's draw commands
};

// The frame mrp_moby_prepare built (valid until the next mrp_moby_begin_frame / mrp_moby_prepare)
struct FrameView {
    uint64_t serial = 0;  // a new number for each frame built
    int mode = 0;         // 0 the world's projection, 1 the game's own VU1 projection onto its frame (the menu: always)
    bool menu = false, title_menu = false;
    const std::vector<PartGpu>* parts = nullptr;
    const std::vector<uint32_t>* cmds = nullptr;  // 4 words a draw record: part, texture word, layer, 0
    const std::vector<DrawCmd>* draws = nullptr;
    const std::vector<Batch>* batches = nullptr;
    const std::vector<MrpMat4>* joints = nullptr;  // PartGpu::info[0] / [1] index them
    const float* normals = nullptr;                // the normal table: 0x200 floats
    float proj = 0, offset[4] = {}, fog_min = 0;   // B+292 x, B+296, B+291 x
    uint64_t env_xyoff = 0;                        // the frame's XYOFFSET_1 (0: none found)
};
// false: no frame built (the MRP not drawing this frame, or mrp_moby_prepare not called since mrp_moby_begin_frame)
bool frame_view(FrameView& out);

// The level's GPU data
struct LevelView {
    int id = -1;
    uint64_t loads = 0;       // a new number for each level loaded
    uint64_t table_hash = 0;  // the class table's: a new value when the vertices and indices were dropped and built again
    // the classes built so far (appended as the frames meet them; dropped when `loads` or `table_hash` changes)
    const std::vector<GpuVertex>* verts = nullptr;
    const std::vector<uint32_t>* indices = nullptr;  // absolute vertex indices
    size_t buckets = 0;                              // the texture arrays (Key::bucket; -1 none: the white texture)
};
bool level_view(LevelView& out);  // false: no level loaded
struct BucketInfo {
    uint32_t w = 0, h = 0, levels = 0, layers = 0;
};
BucketInfo bucket_info(size_t bucket);
// the texels of one layer's level of a texture array, as the GL backend uploads them: RGBA bytes in memory order (any host),
// (max(1, w >> level)) x (max(1, h >> level)); a level the texture lacks: transparent black
void bucket_texels(size_t bucket, uint32_t layer, uint32_t level, std::vector<uint32_t>& px);

}  // namespace rac1::mrpm

#if defined(__WIIU__)
namespace rac1 {
// ports/wiiu/game/mrp_moby_gx2.cpp: its shaders loaded (mrp_moby_gx2_init); what mrp_moby_begin_frame / mrp_moby_ready ask
// on the Wii U in place of the GL program
bool mrp_moby_gx2_ready();
}  // namespace rac1
#endif

#endif
