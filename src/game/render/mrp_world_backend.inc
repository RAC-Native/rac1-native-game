// The MRP world renderer's boundary between its CPU preparation and a GPU backend. Host code.
//
// mrp_world_renderer.cpp prepares, with no GPU: the level's tfrag / tie / shrub textures as arrays per size (the buckets), the
// GPU path's packet templates (one TVert per kicked vertex of every tie / shrub packet, the triangles' indices relative to the
// template's first vertex), and per frame the GPU path's instances (InstGpu), one draw record (8 words) and one draw command per
// template run (DrawCmd: the record's index in `base_instance`, the template's first vertex in `base_vertex`), the runs of draws
// sharing one GPU state (Batch / RKey, the GS state of its first draw in Key), in the chain's order. Its GL and Vulkan backends
// are inline in that file; the Wii U's GX2 one (ports/wiiu/game/mrp_world_gx2.cpp) reads the same data through the accessors
// below (the tie / shrub GPU path only: the CPU path's vertices, the tfrag compute program and the sky are not in it yet).
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_moby_backend.inc.)
#ifndef RAC1_GAME_RENDER_MRP_WORLD_BACKEND_INC
#define RAC1_GAME_RENDER_MRP_WORLD_BACKEND_INC

#include <cstddef>
#include <cstdint>
#include <utility>
#include <vector>

namespace rac1::mrpwb {

// The GPU path's static data of a tie / shrub packet (its kick program evaluated once, by the GS's rules): one GPU vertex per
// kicked vertex (its sources), the triangles as runs of one texture slot and PRIM. (Host byte order: big-endian on the Wii U.)
struct TVert {
    int16_t pos[4];    // the position (tie fat: the base), flags (1: a tie fat vertex)
    int16_t delta[4];  // tie fat: the delta the morph factor scales
    int16_t uv[4];     // s, t, q (shrub h), x 4096
    uint8_t col[4];    // the colour indices (tie fat: c0, c1, c2)
};
static_assert(sizeof(TVert) == 28);

// the GL state of a run of triangles
struct Key {
    int bucket = -1;
    uint32_t layer = 0, word = 1, sampler = 0;
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    uint32_t fogcol = 0;
    bool operator==(const Key&) const = default;
};
// what a batch's draws share: the GL state the GS state maps to (blending: PRIM ABE + ALPHA; the depth test; the depth
// write; the alpha test's second pass, AFAIL), and for the CPU path its texture (bucket, sampler); the rest of the GS state (TME, IIP,
// FGE, the alpha test itself, FOGCOL; the GPU path's texture unit) is per draw data (draw_flags), so runs that differ only in it are
// one multi-draw
struct RKey {
    bool gpu = false, sky = false;
    bool shadow = false;  // the modern lighting's casters outside the view: drawn into the shadow maps only
    uint16_t views = 0;   // a caster's shadow views (bits 0..2 the cascades, 3 + i point light slot i)
    bool view_only = false;  // the frame's shrubs while the lighting casts its own (full models): not drawn into the shadow maps
    uint32_t abe = 0, ztest = 0, zwrite = 0, pass2 = 0, sampler = 0, fogcol = 0;
    uint64_t alpha = 0;
    int bucket = -1;
    bool operator==(const RKey&) const = default;
};
struct Batch {
    Key key;
    bool gpu = false;               // the GPU path's draws (word / layer in its draw records)
    bool sky = false;               // the sky's (in the world projection, directions: the depth clamped to the far plane)
    uint32_t first = 0, count = 0;  // indices (CPU path) / draw commands (GPU path)
    RKey rk;
    int epoch = -1;                 // the GPU path's texture units (Frame::epochs)
    bool tfrag = false;             // the GPU path's tfrag triangles (first / count: in the compute program's indices)
    bool ate = false;               // one of its draws has the alpha test on (the shadow pass's depth-only program not for it)
};
struct InstGpu {
    float clip[16];  // model -> GL clip (the world's projection)
    float rows[16];  // R0, R1, R2, T
    float morph[4];
    uint32_t info[4];  // colour first, F, alpha, kind (0 tie, 1 shrub) | resident << 1
};
static_assert(sizeof(InstGpu) == 160);
struct DrawCmd {  // DrawElementsIndirectCommand
    uint32_t count, instances, first_index;
    int32_t base_vertex;
    uint32_t base_instance;  // the draw's record: the frame's cmds[8 * base_instance ..]
};

// The frame the renderer built (valid until the next frame is prepared)
struct FrameView {
    uint64_t serial = 0;  // a new number for each frame built
    int mode = 0;         // 0 the world's projection (InstGpu::clip), 1 the game's own VU1 projection onto its frame (InstGpu::rows)
    const std::vector<InstGpu>* insts = nullptr;
    // 8 words a draw record: instance, texture word, layer, draw_flags (TME | IIP << 1 | FGE << 2 | the alpha test's mode << 3 | ATST << 4 |
    // AREF << 8 | texture unit << 16); FOGCOL, 0, 0, 0
    const std::vector<uint32_t>* cmds = nullptr;
    const std::vector<DrawCmd>* draws = nullptr;
    const std::vector<Batch>* batches = nullptr;
    // the GPU path's texture units: per epoch (Batch::epoch) the (bucket, sampler) of units 1.. (a draw's unit in its draw_flags; 0: none)
    const std::vector<std::vector<std::pair<int, uint32_t>>>* epochs = nullptr;
    uint64_t env_xyoff = 0;  // the frame's XYOFFSET_1 (0: none found)
};
// false: no frame built (the world not drawn this frame)
bool frame_view(FrameView& out);

// The level's GPU data
struct LevelView {
    int id = -1;
    uint64_t loads = 0;  // a new number for each level loaded
    const std::vector<TVert>* tverts = nullptr;
    const std::vector<uint32_t>* tidx = nullptr;  // relative to their template's first vertex (DrawCmd::base_vertex)
    size_t buckets = 0;                           // the texture arrays (Key::bucket, the epochs' buckets; -1 none: the white texture)
};
bool level_view(LevelView& out);  // false: no level loaded
struct BucketInfo {
    uint32_t w = 0, h = 0, levels = 0, layers = 0;
};
BucketInfo bucket_info(size_t bucket);
// the texels of one layer's level of a texture array, as the GL backend uploads them: RGBA bytes in memory order (any host),
// (max(1, w >> level)) x (max(1, h >> level)); a level the texture lacks: transparent black; a bucket made by a frame's build (the
// sky's, a GIF stream's: no layer of the level's tables) has none
void bucket_texels(size_t bucket, uint32_t layer, uint32_t level, std::vector<uint32_t>& px);

// The resident lit colours (mrp_world_static.h ResidentColours): the table (one u32 per colour, RGBA8 as the level's data holds
// them: R in the low byte, host order), whether all of it is to be sent (a new level), else the ranges (first u32, count) that
// changed since the last send. colours_taken() once sent (the ranges cleared, as the GL backend's upload does).
struct ColoursView {
    const std::vector<uint32_t>* words = nullptr;
    bool all = true;
    const std::vector<std::pair<uint32_t, uint32_t>>* dirty = nullptr;
};
void colours_view(ColoursView& out);
void colours_taken();

}  // namespace rac1::mrpwb

#if defined(__WIIU__)
namespace rac1 {
// ports/wiiu/game/mrp_world_gx2.cpp: its shaders loaded (mrp_world_gx2_init); what mrp_world_ready asks on the Wii U in place of
// the GL program
bool mrp_world_gx2_ready();
}  // namespace rac1
#endif

#endif
