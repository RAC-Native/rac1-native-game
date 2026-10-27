// The MRP world's scene (ADR 0010 step 2): what the frame's world passes (tfrag 0x294240, tie 0x295c80, shrub
// 0x286e60) asked VU1 to draw, decoded from the VU1 chain they built - so the visibility, the LODs, the instances' rows, lit
// colours, fades and fog are exactly the passes' decisions - without running the world's draw programs. Host code.
//
// The walker runs the frame's chain (its tags as the DMA would, runtime/gs's vif1_chain) on a private VU1 (runtime/vu: the
// same VIF unpacking): inside the world passes' chain ranges every VIF code is processed and the programs' set-up / constant
// starts run on the ported programs; at a draw start (shrub 0x063, tie 0x000, the tfrag LOD starts) the packet in
// VU1 memory is found in the assets (mrp_world_check.h), the CPU reference (src/formats/world_mrp.h, proved against the
// original) evaluates it with the instance data / constants the memory holds, and the draw's own state left for the next
// starts
// (shrub: the instance block toggle at qword 1; tie: the block queue vf22, the alpha-test state at 0x3cb) is applied as the
// program does. The GS registers the chain's DIRECT A+D packets set are kept in chain order.
#ifndef RAC1_GAME_RENDER_MRP_WORLD_SCENE_H
#define RAC1_GAME_RENDER_MRP_WORLD_SCENE_H

#include <formats/world_mrp.h>
#include <game/render/mrp_world_check.h>
#include <game/render/mrp_world_static.h>
#include <game/render/mrp_tfrag_gpu.h>

#include <array>
#include <cstdint>
#include <memory>
#include <string>
#include <utility>
#include <vector>

namespace rac1::mrpw {

// A GIF packet a world program kicks, as the CPU reference makes it (Kicked::hp: the positions before the projection).
struct ScenePacket {
    Kicked k;
    Program prog = Program::None;
    uint16_t start = 0;
    uint32_t seq = 0;    // the draw start it belongs to (in the frame's order)
    int32_t view = -1;   // SceneFrame::views: the matrix from hp to the world (homogeneous)
    int32_t table = 0;   // the texture table its CLAMP's texture index names: 0 tfrag, 1 tie, 2 shrub
    bool state = false;  // a tie A alpha-test state packet (A+D only)
    // the program's screen formula as linear in hp (for a vertex behind the camera): X w = hp.x k scale.x + offset.x hp.w, k = 1
    // or (scr_k) the vertex's STQ q x hp.w (tfrag: C0.x; shrub B: h; tie B: q)
    float scr_scale[3] = {1, 1, 1}, scr_off[3] = {0, 0, 0};
    uint8_t scr_k = 0;
    // the GPU path (tie A; shrub A but the packets whose colours read the output buffers): `k` left empty (unless the check asked
    // for the kicks), the asset packet, the instance (SceneFrame::insts), its texture prims' A+D qwords as VU1 memory holds them
    // (SceneFrame::tex: 5 per tie slot, 4 per shrub prim)
    const TiePacket* tie = nullptr;
    const ShrubPacket* shrub = nullptr;
    uint32_t inst = 0, tex = 0, ntex = 0;
    // the sky: a GIF stream of the sky pass's chain (a DIRECT, or a kick of the particle program its sprites start), as
    // the GS register writes it makes (SceneFrame::sky [sky_first, + sky_count)); `table` 3
    bool sky = false;
    uint32_t sky_first = 0, sky_count = 0;
    // a GIF stream's vertices in a world pass's range (the shrub far variants' EE-built quads):
    // its register writes SceneFrame::sky [sky_first, + sky_count) as the sky's, its GS screen points taken back to the world
    // through `view`; `table` the pass's texture table
    bool gif = false;
    // the GPU path's tfrag draw (SceneFrame::tfrag.draws; -1: none): `k` left empty (unless the verification asked for
    // the reference's kicks: then all of the draw's kicks, one after the other)
    int32_t tfrag = -1;
    bool shadow = false;  // a caster outside the view: drawn into the modern lighting's shadow maps only
    uint16_t shadow_views = 0;  // its shadow views (bits 0..2 the cascades, 3 + i point light slot i)
};
// One GS register write of the sky's GIF streams, in stream order: `reg` the GS register, `v` its 64-bit value (the
// PACKED formats converted to the register's: RGBAQ, ST, XYZ(F)2 / 3), `mode` 0 a plain write (A+D, REGLIST; RGBAQ's Q in v's high
// word), 1 PACKED RGBA (Q unchanged), 2 PACKED STQ (Q in `q`), 3 a PACKED XYZ(F) with its ADC bit set
struct SkyWrite {
    uint64_t v = 0;
    uint32_t q = 0;
    uint8_t reg = 0, mode = 0;
};
// A GPU path instance: its rows (R0, R1, R2, T), the tie's morph block (+4), its lit colours (SceneFrame::colours: tie 64 as VU1
// memory holds them, 2^23 + byte floats; shrub 24 integers), the GS's F byte its packets carry, the shrub's alpha.
struct SceneInst {
    float rows[4][4] = {};
    float morph[4] = {};
    uint32_t colour_first = 0, fog = 0, alpha = 0x80;
    bool resident = false;  // colour_first is the instance's slot in the resident colours (mrp_world_static.h), not in SceneFrame::colours
};
// A GS register the chain's DIRECT A+D packets set, before packet `before`.
struct SceneReg {
    uint32_t before = 0;
    uint8_t reg = 0;
    uint64_t value = 0;
};
struct SceneFrame {
    std::vector<ScenePacket> packets;
    std::vector<SceneReg> regs;
    std::vector<std::array<double, 16>> views;  // column-major
    std::vector<SceneInst> insts;
    std::vector<std::array<uint32_t, 4>> colours, tex;
    // the sky: its streams' register writes; the sky asset's texture entries holding a TEX0 this frame (the per-frame
    // cache 0x28b680 clears and 0x28c3c0 / 0x28c07c fill: TEX0, the entry's EE address), the asset's address
    std::vector<SkyWrite> sky;
    std::vector<std::pair<uint64_t, uint32_t>> sky_tex;
    uint32_t sky_asset = 0, sky_packets = 0, sky_kicks = 0;
    uint32_t gif_packets = 0;  // the world ranges' GIF vertex packets (their writes in `sky` too)
    std::array<double, 16> sky_a{};  // the frame's projection A (0x166d00, its qwords the columns): the sky's screen points to directions
    bool has_sky = false;
    double ms_sky = 0;  // (RAC1_FRAME_PROF) the walker's time in the sky's range
    uint64_t draws[6] = {}, unknown[6] = {}, setups = 0, unknown_program = 0;
    uint64_t setup_starts[6 * 0x20] = {};      // (testing) per program and start (micro address & 0x1f), over the run
    double ms_setup[6] = {}, ms_draw[6] = {};  // (RAC1_FRAME_PROF) the frame's time in the set-up starts / the draws, per program
    uint32_t draw_starts = 0;
    // the GPU path's instances whose colours are the resident table's / copied (their source not found), and with
    // RAC1_MRPW_STATIC_VERIFY=1 the resident colours compared with VU1 memory's: instances checked, instances that differed
    uint32_t resident = 0, copied = 0, verified = 0, verify_bad = 0;
    // (stage 2) draws made by the host (ties, shrubs); with RAC1_MRPW_STATIC_CHECK the comparison: the walker's instance
    // draws (tie, shrub), those the host made too, those it missed, extra host draws, and the values that differed
    uint32_t stat_draws[2] = {}, chk_walker[2] = {}, chk_found[2] = {}, chk_missing[2] = {}, chk_extra[2] = {};
    uint32_t stat_variants = 0;                                  // (the host-made shrub far variants' quads drawn)
    uint32_t chk_gif_frames = 0, chk_gif_bad = 0, chk_gif_quads = 0;  // (the check: frames whose far variants' stream differs)
    uint32_t chk_morph_bad = 0, chk_fog_bad = 0, chk_alpha_bad = 0, chk_rows_bad = 0;
    double chk_rows_maxrel = 0;
    std::vector<std::string> chk_notes;
    // the GPU path's tfrag draws; RAC1_MRPW_TFRAG_SELFCHECK=1: each draw's vertices by the plan's CPU form against the
    // reference's on VU1 memory (vertices compared, differing); the draws made by the host (LOD > 0 / widened)
    TfragFrame tfrag;
    uint64_t tfrag_checked = 0, tfrag_bad = 0;
    uint32_t tfrag_host = 0, tfrag_cpu = 0;
    // (RAC1_MRPW_TFRAG_CHECK=1) the walker's (entry, code) sequence against the host's decisions: draws, missing / extra / code differing
    uint32_t tchk_walker = 0, tchk_found = 0, tchk_missing = 0, tchk_extra = 0, tchk_code = 0;
    bool ok = false;
};

// orig: (host) the world passes' chain ranges of the frame being updated: cleared before each game frame's update,
// noted by the passes (overlay_entry.cpp world_pass; `pass` 0 tfrag, 1 tie, 2 shrub, 3 the sky; [first, end): the
// tags it added)
void scene_begin_update();
void scene_note_pass(int pass, uint32_t first, uint32_t end);
struct PassRange {
    int pass = 0;
    uint32_t first = 0, end = 0;
};
const std::vector<PassRange>& scene_passes();
// orig: (host) (mrp_lod.h): the chain a pass's shadow run made (its EE addresses [base, base + bytes), copied out
// before the memory was put back), walked by scene_build in place of the pass's own range [first, end)
// (the sky's: `sky_tex` the texture entries' TEX0 its drawers cached, sky_texture_entries, taken before the memory was put back)
// (`sky_a`: the projection A the sky's shadow ran with, used for its directions)
void scene_note_shadow(int pass, uint32_t first, uint32_t end, uint32_t base, std::vector<uint8_t>&& bytes,
                       std::vector<std::pair<uint64_t, uint32_t>>&& sky_tex = {}, const std::array<float, 16>* sky_a = nullptr);
// orig: (host) the sky asset's texture entries holding a TEX0 now (the per-frame cache 0x28b680 clears and 0x28c3c0 / 0x28c07c
// fill): (TEX0, the entry's EE address)
void sky_texture_entries(std::vector<std::pair<uint64_t, uint32_t>>& out);
void sky_texture_entries_at(std::vector<std::pair<uint64_t, uint32_t>>& out, uint32_t asset_ptr);

// A pass's shadow chain (mrp_lod.h), by the real range it replaces
struct SceneShadow {
    int pass = 0;
    uint32_t first = 0, end = 0, base = 0;
    std::vector<uint8_t> bytes;
    std::vector<std::pair<uint64_t, uint32_t>> sky_tex;
    bool has_a = false;
    std::array<float, 16> sky_a{};
};
// A byte buffer whose growth does not zero the new bytes (they are copied over at once)
template <class T>
struct NoInitAlloc : std::allocator<T> {
    template <class U>
    struct rebind {
        using other = NoInitAlloc<U>;
    };
    NoInitAlloc() = default;
    template <class U>
    NoInitAlloc(const NoInitAlloc<U>&) noexcept {}
    template <class U>
    void construct(U* p) noexcept {
        ::new (static_cast<void*>(p)) U;
    }
    template <class U, class... A>
    void construct(U* p, A&&... a) {
        ::new (static_cast<void*>(p)) U(std::forward<A>(a)...);
    }
};
using CaptureBytes = std::vector<uint8_t, NoInitAlloc<uint8_t>>;

// What a scene build reads of the game's state, captured on the game's thread once its update has ended (the chain's
// tags and data in the walk's order, the shadow chains, the passes' ranges, the projections and the camera, the sky's entries): the
// build itself then reads no EE memory (it can run on another thread while the game goes on)
struct SceneInput {
    bool ok = false;
    uint32_t chain_start = 0, chain_end = 0;
    std::vector<PassRange> passes;
    std::vector<SceneShadow> shadows;
    CaptureBytes bytes;
    std::vector<std::array<uint32_t, 3>> spans;  // (EE address, offset in bytes, length; 0xffffffff: a null pointer)
    // (the same range read again - a class's packet for each of its instances - is copied once: the spans share the bytes)
    std::vector<std::array<uint32_t, 3>> seen;  // (a small hash table: address, length, offset + 1)
    std::array<float, 16> a{}, a_clip{};          // the projections A 0x166d00, 0x166d40
    std::array<float, 3> camera{};                // 0x166dc0
    bool has_sky = false;
    uint32_t sky_asset = 0;
    std::vector<std::pair<uint64_t, uint32_t>> sky_tex;
    std::array<double, 16> sky_a{};
    // the resident colours (mrp_world_static.h; updated before the build, not changed while it runs): the GPU path's
    // instances name their slot there when their colours' UNPACK came from the table
    const ResidentColours* colours = nullptr;
    // (stage 2) the frame's ties / shrubs made by the host (mrp_world_static.h): for a pass it covers (`stat->on`), the
    // chain's own draws in that pass's range are decoded (their effects on the VU state kept) but not drawn, and these are drawn at
    // the range's end. `stat_check` (RAC1_MRPW_STATIC_CHECK=1): nothing replaced; the walker's tie / shrub draws compared with them
    const StaticFrame* stat = nullptr;
    const StaticLevel* stat_level = nullptr;
    bool stat_check = false;
    // the GPU path's tfrag: its level data and resident colours (null: the CPU reference as before); `tfrag_kicks`: the
    // reference's kicks kept too (the GPU's vertices compared with them); `tfrag_host`: the frame's (entry, code) made by the host
    // (LOD > 0 / widened: the chain's tfrag draws decoded but not drawn, these drawn at the range's end); `tfrag_check`: compared only
    const TfragLevel* tfrag_level = nullptr;
    const TfragColours* tfrag_colours = nullptr;
    bool tfrag_kicks = false;
    const std::vector<std::pair<uint32_t, uint8_t>>* tfrag_host = nullptr;
    bool tfrag_check = false;
    // the modern lighting's tfrag casters outside the view (mrp_tfrag_gpu.h tfrag_shadow_decide): drawn at the tfrag range's end
    // as the host's, their packets ScenePacket::shadow
    const std::vector<std::pair<uint32_t, uint8_t>>* tfrag_shadow = nullptr;
    const std::vector<uint16_t>* tfrag_shadow_views = nullptr;  // (each one's shadow views)
};

// orig: (host) the frame's chain [chain_start, chain_end) walked to the end of its last world range: the world's packets
// in `out` (the sky's range: its DIRECT GIF streams and the particle program's kicks for its sprites, run on the private VU1) (the assets in `idx`; `reset`: the private VU1 started again, e.g. a new level)
// (`kicks`: the GPU path's packets get their CPU reference kicks as well, for the check)
bool scene_build(const WorldIndex& idx, uint32_t chain_start, uint32_t chain_end, SceneFrame& out, bool reset, bool kicks);
// orig: (host) the same in two steps: the capture (after the update, while the game's state does not change; the frame's shadow chains
// copied into the input) and the build from it (any thread)
void scene_capture(uint32_t chain_start, uint32_t chain_end, SceneInput& in);
// orig: (host) the overlay addresses the capture reads (translated on the game's thread: OV sites are not for other threads), and the
// capture from them (any thread, while the game's state does not change; the passes' notes and shadow chains are not touched meanwhile)
struct SceneAddrs {
    uint32_t a = 0, a_clip = 0, camera = 0, sky_asset = 0;
};
SceneAddrs scene_addrs();
void scene_capture(uint32_t chain_start, uint32_t chain_end, SceneInput& in, const SceneAddrs& ad);
bool scene_build(const WorldIndex& idx, const SceneInput& in, SceneFrame& out, bool reset, bool kicks);
// orig: (host) the frame's scene without the walker (the Wii U's MRP-lite: the world VU1 programs are not in its build): the host's
// ties / shrubs of `sf` as the GPU path's packets (as the walk emits them at its passes' ends, emit_static), in the view of the
// projection A `a` (0x166d00) and the camera; the walker's VU state those packets take - the tie A alpha-test state packet, the shrub
// A fog constants - read from the overlay's copies the passes' constants UNPACK (game thread: EE memory). Before them the host's tfrag
// draws `tdraws` (tfrag_decide's (entry, code) of level `tl`; null: none), as the walk emits them at the tfrag range's end
// (emit_tfrag_host), in the view and constants tfrag_pass_vu reads. The frame's GS registers before the world passes
// (SceneFrame::regs) are not made (none). No sky, far variants, casters. false: no level
bool scene_from_static(const StaticFrame& sf, const StaticLevel& sl, const std::array<float, 16>& a, const std::array<float, 3>& camera, const TfragLevel* tl,
                       const std::vector<std::pair<uint32_t, uint8_t>>* tdraws, SceneFrame& out);
// orig: (host) the view and the constants the frame's tfrag pass gives VU1 through its chain - what the walker's VU1 memory holds at
// the end of the tfrag range: 5..8 (R0, R1, R2, T) and 0x290..0x29d -, read from where the chain takes them (game thread, after the
// update: EE memory). false: not found (no tfrag pass this frame)
bool tfrag_pass_vu(std::array<float, 16>& view, std::array<uint32_t, 4 * 14>& consts);
// orig: (host) the build's EE reads the capture did not have (0: the walk and the capture agree)
uint64_t scene_capture_misses();

// orig: (host) the in-game check (RAC1_WORLD_MRP_CHECK=1, with the faithful world): each world draw start of the real VU1's
// chain run, its kicked packets, compared with the scene's at the frame's end (mrp_world_check.h's comparisons)
void scene_check_start(Program prog, uint32_t start);
void scene_check_kick(const uint8_t* data, uint32_t qwc);
void scene_check_frame(const SceneFrame& f, CheckStats& stats);

// The draw starts (the starts that kick): shrub 0x063, tie 0x000, tfrag A's LOD starts and B's 0x002.
bool is_draw_start(Program prog, uint32_t start);

}  // namespace rac1::mrpw

#endif
