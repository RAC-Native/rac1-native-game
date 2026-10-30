// The MRP world renderer's boundary between its CPU preparation and a GPU backend. Host code.
//
// mrp_world_renderer.cpp prepares, with no GPU: the level's tfrag / tie / shrub textures as arrays per size (the buckets), the
// GPU path's packet templates (one TVert per kicked vertex of every tie / shrub packet, the triangles' indices relative to the
// template's first vertex), and per frame the GPU path's instances (InstGpu), one draw record (8 words) and one draw command per
// template run (DrawCmd: the record's index in `base_instance`, the template's first vertex in `base_vertex`), the runs of draws
// sharing one GPU state (Batch / RKey, the GS state of its first draw in Key), in the chain's order. Its GL and Vulkan backends
// are inline in that file; the Wii U's GX2 one (ports/wiiu/game/mrp_world_gx2.cpp) reads the same data through the accessors
// below (the tie / shrub GPU path, the tfrag GPU path's triangles as the CPU makes them - tfrag_frame_cpu -, the CPU path's vertices: on
// the Wii U the sky's, mrp_world_sky.inc).
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_moby_backend.inc.)
#ifndef RAC1_GAME_RENDER_MRP_WORLD_BACKEND_INC
#define RAC1_GAME_RENDER_MRP_WORLD_BACKEND_INC

#include <array>
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

// One GPU vertex (an XYZF2 the programs kick; the CPU path's, the tfrag compute program's): its world point (homogeneous), its point on
// the GS's frame (x w), the STQ undivided ((S, T, Q) x w: S / Q is the texture coordinate), the GS's Q (the mip level), RGBA, F. (Host
// byte order but `rgba`: its bytes R, G, B, A in memory on any host.)
struct GpuV {
    float wp[4];
    float vu[4];
    float stq[3];
    float lodq;
    uint8_t rgba[4];
    float fog;
    uint32_t word, layer;  // the texture (the triangle this vertex closes: flat, the last vertex provokes)
    uint32_t state[2];     // its GS state as data (draw_flags, FOGCOL), flat as the texture
};
static_assert(sizeof(GpuV) == 72);

// the GL state of a run of triangles
struct Key {
    int bucket = -1;
    uint32_t layer = 0, word = 1, sampler = 0;
    uint64_t prim = 0, test = 0, alpha = 0, zbuf = 0;
    uint32_t fogcol = 0;
    bool opaque = false;  // (host: the bound texture's sampled mip levels have alpha >= 0x80 everywhere; not part of the GS state comparison)
    bool operator==(const Key& o) const {
        return bucket == o.bucket && layer == o.layer && word == o.word && sampler == o.sampler && prim == o.prim && test == o.test &&
               alpha == o.alpha && zbuf == o.zbuf && fogcol == o.fogcol;
    }
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
    int32_t host = -1;              // ((2026-10-28) the host sky's: a shell's run, FrameView::sky_host[host]; first / count unused)
};
// (2026-10-28) a run of a host sky shell's triangles (mrp_sky_host.inc: the shell drawn whole from the level's model, no game pass) drawn
// by a Batch with `host` >= 0: its corners [first, first + count) in FrameView::sky_host_verts (three a triangle, in order: static for
// the model - wp the shell's own point, w 0; stq S, T, Q undivided, which the vertex shader multiplies by the clip w), turned by `m` (the
// shell's matrix: four columns, the world's axes = m x the shell's point - its fourth column unused: a direction) before the world's
// view-projection; the flat values its triangles' closing corners carry this frame (the GS state the build found: as the CPU path's)
struct SkyHostDraw {
    float m[16] = {};
    uint32_t first = 0, count = 0;
    uint32_t word = 0, layer = 0;
    uint32_t state[2] = {};
    float fog = 0.0f;
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

// A tfrag batch's triangles of one texture unit (the GPU path's tfrag draws sample by unit, the unit in each vertex's draw_flags; a
// backend without texture units binds the unit's texture per range): `batch` the frame's batch (Batch::tfrag), [first, first + count)
// in the frame's tfrag indices, `unit` 1.. in the batch's epoch (0: none), `pass2` whether any pixel in the range can fail the alpha test
struct TfragRange {
    uint32_t batch = 0, first = 0, count = 0, unit = 0;
    bool pass2 = true;
    // ((2026-10-30) the Wii U's late sky) every pixel its pass 1 draws of alpha >= 0x80 (the texture's sampled levels and the vertices'
    // alphas: no blend with what lies behind it)
    bool opaque = false;
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
    // the GPU path's tfrag triangles made on the CPU (the compute program's output: mrp_world_renderer.cpp tfrag_frame_cpu): the
    // vertices, the indices (absolute: base vertex 0; a triangle closed by an ADC vertex degenerate, (c, c, c)), the batches' ranges
    // by texture unit in batch order; empty: none
    const std::vector<GpuV>* tf_verts = nullptr;
    const std::vector<uint32_t>* tf_idx = nullptr;
    const std::vector<TfragRange>* tf_ranges = nullptr;
    uint32_t tf_draws = 0;  // (the frame's tfrag draws; the CPU route's time, ms)
    double tf_ms = 0;
    // the CPU path's vertices and triangles (GsFront: the indices absolute, base vertex 0) - on the Wii U the sky's alone: its batches
    // (Batch::sky, gpu and tfrag false) [first, first + count) in `cpu_idx`, their texture Key::bucket (a frame's: built_bucket) / sampler
    const std::vector<GpuV>* cpu_verts = nullptr;
    const std::vector<uint32_t>* cpu_idx = nullptr;
    double sky_ms = 0;  // (the sky's CPU time, ms: the Wii U's capture on the game's thread and its packets' build)
    // ((2026-10-28) the host sky: its shells' draws (Batch::host), the corners of the model they were built from - the same vector for one
    // `sky_host_serial`)
    const std::vector<SkyHostDraw>* sky_host = nullptr;
    const std::vector<GpuV>* sky_host_verts = nullptr;
    uint64_t sky_host_serial = 0;
    // ((2026-10-30) the tfrags' programs in the vertex shader, src/game/render/mrp_tfrag_gpu_vs.inc: `tvs` the frame's tfrags made by it -
    // tf_verts / tf_idx empty -, its block's words (host order: mrp_world_wiiu.inc kTvs*) and one u32 per triangle corner, the frame's
    // triangle slots' order: the tfrag ranges' first / count are the corners')
    bool tvs = false;
    const std::vector<uint32_t>* tvs_block = nullptr;
    const std::vector<uint32_t>* tvs_corners = nullptr;
    // ((2026-10-31) RAC1_WIIU_TFRAG_IDX) not empty: `tvs_corners` the attribute words (a sent vertex once, or a form 3 triangle's corners)
    // and this one index per triangle corner into them - the ranges' first / count are the indices'
    const std::vector<uint32_t>* tvs_index = nullptr;
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
// The level's template runs (one per DrawCmd a packet's template makes: its triangles of one texture slot and PRIM): its indices
// [first_index, first_index + count) in LevelView::tidx, relative to its template's first vertex `base_vertex`; in no order
struct TplRunInfo {
    uint32_t first_index = 0, count = 0, base_vertex = 0;
};
void level_runs(std::vector<TplRunInfo>& out);  // (empty: no level loaded)
struct BucketInfo {
    uint32_t w = 0, h = 0, levels = 0, layers = 0;
};
BucketInfo bucket_info(size_t bucket);
// the texels of one layer's level of a texture array, as the GL backend uploads them: RGBA bytes in memory order (any host),
// (max(1, w >> level)) x (max(1, h >> level)); a level the texture lacks: transparent black; a bucket made by a frame's build (the
// sky's, a GIF stream's: no layer of the level's tables) has none
void bucket_texels(size_t bucket, uint32_t layer, uint32_t level, std::vector<uint32_t>& px);
// a bucket a frame's build made (the sky's: one layer, one level - mrp_world_renderer.cpp sky_texture): its size and texels, RGBA bytes
// in memory order (any host), valid until the level changes (LevelView::loads); false: not such a bucket, or not kept (the Wii U alone
// keeps them: elsewhere they are GL / Vulkan images)
bool built_bucket(size_t bucket, uint32_t& w, uint32_t& h, const std::vector<uint32_t>*& px);

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

// ((2026-10-30) mrp_tfrag_gpu_vs.inc) the level's tables for the tfrags' vertex shader: the images (a qword a texel) and the plans'
// tables (RGBA32UI texels, host order), new when `generation` changes (0: none); the tfrags' lit colours (as the resident ones: one u32
// each), all of them to be sent or the ranges changed since tfrag_vs_colours_taken(). tfrag_vs_tables_taken(): the plans' tables sent
// (their host copy dropped: `plans` empty after)
struct TfragVsView {
    uint64_t generation = 0;
    const std::vector<std::array<uint32_t, 4>>* images = nullptr;
    const std::vector<std::array<uint32_t, 4>>* plans = nullptr;
    const std::vector<uint32_t>* colours = nullptr;
    bool colours_all = true;
    const std::vector<std::pair<uint32_t, uint32_t>>* colours_dirty = nullptr;
};
bool tfrag_vs_view(TfragVsView& out);  // false: no level
void tfrag_vs_tables_taken();
void tfrag_vs_colours_taken();

}  // namespace rac1::mrpwb

#if defined(__WIIU__)
namespace rac1 {
// ports/wiiu/game/mrp_world_gx2.cpp: its shaders loaded (mrp_world_gx2_init); what mrp_world_ready asks on the Wii U in place of
// the GL program
bool mrp_world_gx2_ready();
// ... and its tfrag shaders (shaders/mrp_tfrag.gsh) loaded, the tfrags not turned off (RAC1_WIIU_TFRAG=0): the frame's tfrags made
bool mrp_world_gx2_tfrag_ready();
// ... and its sky's shaders (shaders/mrp_sky.gsh) loaded, the sky on (RAC1_WIIU_SKY, the debug menu's "Sky"): the frame's sky captured
bool mrp_world_gx2_sky_ready();
// mrp_world_renderer.cpp: finishes any remaining tfrag_frame_cpu draws on Core 1 alongside Core 0's worker before mrp_world_gx2_draw reads tf_verts
void mrp_world_wiiu_tfrag_join(double* out_ms = nullptr);
// ((2026-10-30) mrp_world_gx2.cpp) the tfrags' vertex shader's pairs loaded (shaders/mrp_tfrag_vs_fast.gsh), RAC1_WIIU_TFRAG_VS on, and
// nothing that needs the other shaders (the fast pixel shaders off, the shade debug, the tfrag debug): the frame's tfrags may take it
bool mrp_world_gx2_tfrag_vs_ready();
}  // namespace rac1
#endif

#endif

