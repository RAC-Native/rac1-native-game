// The MRP world's terrain (tfrag) on the GPU (ADR 0010). Host code: none of it is
// the game's.
//
// The tfrag A / B programs (1.3) are not static per packet like the tie / shrub ones: per frame
// they transform the entry's records through the view, morph the finer LODs' vertices toward their parents by the view depth
// (`p' = (P1 + P2) f.x + p f.y`), substitute a parent's vertex info for a vertex past the band, fix up the lists, and send the strips
// with ADC near the guard band. The CPU reference (src/formats/world_mrp.cpp tfrag_kick / tfrag_b_kick) runs them on a copy of
// VU1 memory for every visible entry every frame. Here:
//
//  - Load once (a level): every entry's three images (far / mid / near: the VU1 memory its lists leave, derived from the entry's data
//    as EE memory holds it after the level load - the texture prims' CLAMP / TEX0 as the load rewrote them), and per image and start
//    its *plan*: the program's steps (transform, morph, morph with substitutions, fix-up: their lists, counts, bands; whether a step
//    must run in order) and the emission's static walk (per sent vertex its vertex info address, its strip position, whether its
//    clip flags count for ADC; the strips' triangles, the texture switches). The lit colours (entry +0x1e, relit by 0x295350) are
//    kept resident as the ties' / shrubs' are (a mirror compared after each update, the changed entries uploaded).
//  - Per frame the CPU sends only the pass's decisions: per visible entry its image / start (the LOD code), the view (VU1 5..8), the
//    pass's 14 constants (0x290..0x29d); a compute shader runs the steps on a copy of the image in shared memory (the same float
//    operations in the same order: `precise`, the division correctly rounded) and writes the vertices the CPU reference's kicks give
//    the renderer (mrp_world_renderer.cpp GpuV) and the strips' triangles (a triangle whose closing vertex has ADC: degenerate).
//  - At LOD > 0 / in widescreen the pass's per-entry decisions (0x294368: the zones, the sphere tests, the clip matrix's box test,
//    the draw distance, the LOD bands) are made here for every entry with the scaled distances / widened frustum: no tfrag shadow run.
#ifndef RAC1_GAME_RENDER_MRP_TFRAG_GPU_H
#define RAC1_GAME_RENDER_MRP_TFRAG_GPU_H

#include <formats/world_mrp.h>

#include <array>
#include <cstdint>
#include <utility>
#include <vector>

namespace rac1::mrpw {

// ---- the static part ----------------------------------------------------------------------------------------------------------

// One image's start, as the program runs it (tfrag_kick / tfrag_b_kick). Steps: 0 T (records `a`, `n` of them), 1 M (vertex infos `a`,
// parent indices `b`, `n`, band `band`), 2 MS (the same with substitutions), 3 F (list `a`, second indices `b`, `n`, the band whose w
// is the threshold). `serial`: a step whose iterations read what another iteration writes (run in order).
struct TfragStep {
    uint8_t kind = 0;
    bool serial = false;
    uint16_t a = 0, b = 0, n = 0, band = 0;
};
// One sent vertex (the emission's walk, tf_emit): its vertex info (image-relative qword), its position in its strip (0, 1, 2 = 2 or
// more), whether its clip flags count for ADC (not the first vertex after a strip change 1.3's quirk), its run.
struct TfragPlanVertex {
    uint16_t via = 0;
    uint8_t pos = 0;
    bool counts = false;
    uint16_t run = 0;
};
// The GS state changes along the emission (for the renderer's replay of the GS registers, GsFront): a texture switch (the 5 A+D
// qwords at image qword `at`), a strip's GIF tag (the constant 0x292: PRIM, the queue restarted), or a run of strips' triangles.
struct TfragEvent {
    uint8_t kind = 0;  // 0 texture, 1 strip tag, 2 run
    uint16_t at = 0;   // texture: its first qword; run: the run's index
};
struct TfragRun {
    uint32_t first_tri = 0, ntri = 0;
};
struct TfragPlan {
    bool ok = false;
    int form = 0;  // the emission (tf_emit): 0 the morphed records, 1 the same with the guard band, 2 the raw records, 3 the raw records with the guard band, 4 tfrag B
    std::vector<TfragStep> steps;
    std::vector<TfragPlanVertex> verts;
    std::vector<TfragEvent> events;
    std::vector<TfragRun> runs;
    uint32_t ntris = 0;
    uint32_t gpu_first = 0;  // (the GPU copy: its first word)
};
// The plans of an entry: per image (0 far, 1 mid, 2 near) per start (the code; 2 on the near image: tfrag B)
struct TfragEntryGpu {
    bool ok = false;
    uint32_t ee = 0;                        // the entry in EE memory (*(0x160f4c) + 0x40 index)
    uint32_t colour_ee = 0, colour_n = 0;   // its lit colours (EE address: data + entry +0x1e; the near image's count)
    uint32_t colour_slot = 0;               // their first word in the resident table
    std::array<std::vector<uint8_t>, 3> image;  // the images from the entry's data in EE memory
    std::array<uint32_t, 3> gpu_image{};    // (the GPU copy: first qword)
    std::array<uint16_t, 3> colour_addr{}, colour_count{};
    TfragPlan far6, mid8, mida, neare, near10, near14, nearb;
    const TfragPlan* plan(int lod, uint32_t code, bool b) const;
};
struct TfragLevel {
    bool ok = false;
    std::vector<TfragEntryGpu> entries;  // by the asset's tfrag index
    std::vector<int32_t> by_ee;          // the EE table's entry -> its asset index (-1: none)
    uint32_t table = 0, count = 0;       // *(0x160f4c), *(0x160f50)
    uint32_t mismatched = 0, plans = 0, serial_steps = 0, plan_fail = 0;
    // the GPU copies (built at the level load, uploaded once by the renderer)
    std::vector<std::array<uint32_t, 4>> images;
    std::vector<uint32_t> plan_words;
    uint32_t colour_words = 0;
    uint64_t generation = 0;  // a new value at every build: the GPU copies are uploaded again when it changes
};
// orig: (host) at the level's load (the game's thread: EE memory, overlay addresses): `w` the level's MRP world asset
bool tfrag_level_build(TfragLevel& L, const World& w);

// ---- the resident lit colours --------------------------------------------------------------------------------------------------

struct TfragColours {
    std::vector<uint32_t> mirror;
    std::vector<std::pair<uint32_t, uint32_t>> dirty;  // (first word, count) changed since the last upload
    std::vector<uint32_t> dirty_words;
    bool all_dirty = true;
    const TfragLevel* level = nullptr;
    uint64_t updates = 0, entries_changed = 0, words_uploaded = 0;
};
// orig: (host) after the game's update (its state frozen; any thread): the mirror compared with EE memory
void tfrag_colours_update(TfragColours& c, const TfragLevel& L);

// ---- the frame ----------------------------------------------------------------------------------------------------------------

// One visible entry's draw (the walker's, or the host's at LOD > 0 / widened): its image / start, the frame's view and constants.
struct TfragDraw {
    uint32_t entry = 0;
    uint8_t lod = 0;
    uint8_t code = 0;
    bool b = false;          // the clipping program (tfrag B, start 0x002)
    uint32_t view = 0;       // TfragFrame::views
    uint32_t consts = 0;     // TfragFrame::consts
};
struct TfragFrame {
    std::vector<TfragDraw> draws;
    std::vector<std::array<float, 16>> views;            // the VU1 view (5..8: R0, R1, R2, T)
    std::vector<std::array<uint32_t, 4 * 14>> consts;    // VU1 0x290..0x29d
    void clear() {
        draws.clear();
        views.clear();
        consts.clear();
    }
};

// What the GPU program needs of the frame's draws (the renderer lays them out: vertices, triangle slots, runs).
struct TfragGpuDraw {  // (std430, 16 words)
    uint32_t image, nq, plan, colour;
    uint32_t col_addr, col_count, view, consts;
    uint32_t vbase, ibase, rbase, flags;
    uint32_t wview;  // the frame's world matrix (TfragGpuFrame::views) of the positions before the projection
    uint32_t pad[3];
};

// orig: (host) the CPU form of the GPU program (testing: RAC1_MRPW_TFRAG_SELFCHECK, the proof that the plan's semantics are the
// reference's): one draw's sent vertices - per vertex its position before the projection, the kicked STQ / RGBA / XYZF2 qwords
struct TfragCpuVertex {
    std::array<float, 4> hp{};
    std::array<uint32_t, 4> stq{}, rgba{}, xyzf{};
};
bool tfrag_exec_cpu(const TfragEntryGpu& e, const TfragPlan& p, int lod, const uint32_t* colours, const float view[16], const uint32_t consts[4 * 14],
                    std::vector<TfragCpuVertex>& out, std::vector<uint8_t>* adc = nullptr);
// orig: (host) the same draw by the reference (tfrag_kick / tfrag_b_kick) on VU1 memory - `vu_mem` (16 KB, the image at `base`: the
// walker's, as the frame's chain left it), or made from the same inputs when null - compared: the vertices differing (testing)
uint32_t tfrag_selfcheck(const TfragEntryGpu& e, const TfragPlan& p, int lod, uint32_t code, bool b, const uint32_t* colours, const float view[16],
                         const uint32_t consts[4 * 14], uint64_t& compared, const uint8_t* vu_mem = nullptr, uint32_t base = 0);

// ---- the host's decisions (LOD > 0 / widescreen) ------------------------------------------------------------------------------

// What the tfrag pass 0x294240 / 0x294368 reads, taken before the real pass (the game's thread): the camera, rotation, clip matrix
// (widened as the shadow run had it), the sphere tests' constants, the zones, the bands and the draw distance (scaled), the enable.
struct TfragSnapshot {
    bool valid = false, handle = false, enabled = false;
    float scale = 1.0f, widen = 1.0f;
    std::array<std::array<float, 4>, 3> rot{};   // 0x166c80
    std::array<float, 4> cam{};                  // 0x166dc0
    std::array<std::array<float, 4>, 4> clip{};  // 0x166d80 (vf12..vf15)
    std::array<float, 4> c23{}, c24{}, c26{}, c27{}, c22{};  // 0x16cae0, 0x16caf0, 0x16cc20, 0x16cc30, 0x160f20
    std::array<int32_t, 3> bands{};              // 0x160f70.. (scaled)
    uint32_t dist = 0;                           // 0x160f80 (float bits, scaled)
    std::array<uint8_t, 0x80> zone{};            // 0x173d00
    // the view the pass UNPACKs to VU1 5..8 (and 0x14d), as 0x294240 makes it on its stack: the identity with the camera 0x166dc0 x -1024
    // as its translation, times 0x166cc0 (mat4_mul). Taken here so that a frame whose tfrag pass is not run (the Wii U's
    // RAC1_TFRAG_PASS_OFF) has it: mrpw::tfrag_pass_vu
    std::array<float, 16> view{};
};
// orig: (host) before the real tfrag pass (mrp_lod_world_shadow, the game's thread): the snapshot of what the pass reads (inside the
// shadow's journal with the view widened when `widen` > 1: the widened values); `handle`: the host makes the frame's tfrag decisions,
// its distances scaled by `scale` (as mrp_lod.cpp patch_tfrag)
void tfrag_note_pass(bool handle, float scale, float widen);
// orig: (host) the snapshot's frustum widened by `w` after tfrag_note_pass took it from the game's own 4:3 memory (the Wii U's widescreen,
// render_view.h g_view_draw_widen: no shadow run, nothing of the game's memory written): the x tangent (vf24.x) times w, its radius factor
// (vf26.x) sqrt(1 + t^2), the clip matrix's x column (vf12..vf15) divided by w - as mrp_lod.cpp patch_widen's projection_setup and
// clip_widen_apply make them; the view (VU1 5..8, the game's projection A) kept
void tfrag_snapshot_widen(float w);
const TfragSnapshot& tfrag_snapshot();
// orig: (host) the frame's visible entries and their LOD codes from the snapshot (as 0x294368 decides them; `check`: at the game's own
// distances and frustum, RAC1_MRPW_TFRAG_CHECK): (entry index, code; 2 = the clipping program), in the chain's order
void tfrag_decide(const TfragLevel& L, const TfragSnapshot& S, bool check, std::vector<std::pair<uint32_t, uint8_t>>& out);
// orig: (host) the modern lighting's tfrag casters outside the view: the entries (in zone) whose sphere reaches `v` and that the
// frame's own decision (tfrag_decide at the snapshot's settings) does not draw, each with the LOD code its view depth gives (never the
// clipping program)
struct CasterVolume;
// (`views` each one's shadow views, CasterVolume::views; sorted by them)
void tfrag_shadow_decide(const TfragLevel& L, const TfragSnapshot& S, const CasterVolume& v, std::vector<std::pair<uint32_t, uint8_t>>& out,
                         std::vector<uint16_t>& views);

// ---- the GPU program ----------------------------------------------------------------------------------------------------------

// orig: (host) the GL objects (the renderer's thread): the static data uploaded at a new level; the frame's dispatch writing the
// vertices (GpuV layout, `vertex_bytes` each) and the triangles' indices into the buffers the renderer's VAO reads
struct TfragGpuFrame {
    std::vector<TfragGpuDraw> draws;
    std::vector<double> views;  // per view the 16 doubles of the matrix from the positions before the projection to the world (column-major)
    std::vector<float> vu_views;  // per view the 16 floats of the VU1 view
    std::vector<uint32_t> consts;  // per constants set 56 words
    std::vector<uint32_t> runs;    // per run: texture word, layer, draw flags, FOGCOL
    uint32_t nverts = 0, nidx = 0;
};
bool tfrag_gpu_init();
bool tfrag_gpu_ready();  // (the program compiled: no GL call, any thread)
void tfrag_gpu_level(const TfragLevel& L);
void tfrag_gpu_colours(TfragColours& c);
uint32_t tfrag_gpu_vbo();
uint32_t tfrag_gpu_ibo();
uint64_t tfrag_gpu_vk_vbo();
uint64_t tfrag_gpu_vk_ibo();
void tfrag_gpu_dispatch(const TfragGpuFrame& f, uint32_t vertex_bytes);
// orig: (host) testing: the vertices / indices the last dispatch wrote, read back
void tfrag_gpu_readback(const TfragGpuFrame& f, uint32_t vertex_bytes, std::vector<uint8_t>& verts, std::vector<uint32_t>& idx);

// Switches: RAC1_MRPW_TFRAG_GPU=0 the CPU reference as before (and the tfrag shadow run at LOD > 0 / widened); RAC1_MRPW_TFRAG_HOST=0
// the GPU path with the tfrag shadow run's chain at LOD > 0 / widened instead of the host's decisions (testing: with RAC1_MRPW_TFRAG_CHECK
// the host's decisions are compared with the shadow run's, with RAC1_MRPW_TFRAG_VERIFY the GPU with the reference on its chain).
bool tfrag_gpu_enabled();
bool tfrag_host_enabled();

}  // namespace rac1::mrpw

#endif
