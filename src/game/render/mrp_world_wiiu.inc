// The Wii U's world preparation on its worker (ports/wiiu/PERFORMANCE.md, "The world's whole preparation on core 0", 2026-10-28): the
// parts of the MRP-lite's frame that read EE memory, taken apart from those that only compute, so that the computing ones can run on the
// preparation's worker beside the game thread's update - which writes EE memory meanwhile - while the main thread takes the few EE reads
// they need at the prepare. Host code: none of it is the game's.
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_world_sky.inc.)
#ifndef RAC1_GAME_RENDER_MRP_WORLD_WIIU_INC
#define RAC1_GAME_RENDER_MRP_WORLD_WIIU_INC

#include <game/render/mrp_tfrag_gpu.h>
#include <game/render/mrp_world_scene.h>
#include <game/render/mrp_world_static.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/vu/vu1.h>

#include <array>
#include <cstdint>
#include <cstring>
#include <utility>
#include <vector>

namespace rac1::mrpw {

// Host copies of EE memory's 4 KB pages: those the ties' / shrubs' / tfrags' culling reads - the class records, the instances' bounds and
// data, the tfrag table and its boxes - noted once a level (static_pages_note, tfrag_pages_note), copied then and refreshed by the main
// thread between two of the worker's runs (the class records' flags - hot words - every prepare, the rest a few pages a prepare). The level's
// own tables: nothing of them changes in play but the colours (their mirrors, mrp_world_static.h) and the relighting's flags, which the
// culling does not read.
class EePages {
public:
    static constexpr uint32_t kShift = 12, kSize = 1u << kShift;
    void clear();
    // the pages covering [addr, addr + bytes) kept (`hot`: those bytes' words copied by refresh_hot too); not copied yet
    void note(uint32_t addr, uint32_t bytes, bool hot = false);
    void refresh_all();
    void refresh_hot();
    // the next `n` pages (after the last call's), round the whole set
    void refresh_rolling(uint32_t n);
    // the copy of EE address `a` (nullptr: a page not kept - counted in `misses`, but for the first page: a null pointer's)
    const void* ptr(uint32_t a) const {
        const uint32_t p = a >> kShift;
        const int32_t i = p < index_.size() ? index_[p] : -1;
        if (i < 0) {
            if (p != 0) ++misses;
            return nullptr;
        }
        return reinterpret_cast<const uint8_t*>(data_.data()) + (static_cast<size_t>(i) << kShift) + (a & (kSize - 1));
    }
    size_t pages() const { return ee_page_.size(); }
    size_t hot_words() const { return hot_.size(); }
    mutable uint32_t misses = 0;  // (written by the one thread reading the copies; read by the main thread once that one is joined)

private:
    std::vector<int32_t> index_;     // per EE page: its copy's index, -1 none
    std::vector<uint32_t> ee_page_;  // per copy: its EE page
    std::vector<uint32_t> hot_;      // the hot words' EE addresses
    std::vector<uint32_t> data_;     // the copies, EE memory's layout (host-order words)
    uint32_t roll_ = 0;
    void copy(uint32_t i);
};

// EE memory as the culling reads it: the memory itself (`pages` null: the game's thread, the PC) or the copies
struct EeRead {
    const EePages* pages = nullptr;
    const void* at(uint32_t a) const { return pages != nullptr ? pages->ptr(a) : ee_ram_ptr_read(a); }
    uint32_t u(uint32_t a) const {
        const void* p = at(a);
        if (p == nullptr) return 0;
        uint32_t v;
        std::memcpy(&v, p, 4);
        return v;
    }
    float f(uint32_t a) const {
        const void* p = at(a);
        if (p == nullptr) return 0.0f;
        float v;
        std::memcpy(&v, p, 4);
        return v;
    }
    uint32_t h(uint32_t a) const { return u(a & ~3u) >> (8 * (a & 2)) & 0xffff; }
    uint32_t b(uint32_t a) const { return u(a & ~3u) >> (8 * (a & 3)) & 0xff; }
    std::array<float, 4> q(uint32_t a) const { return {f(a), f(a + 4), f(a + 8), f(a + 12)}; }
};

// orig: (host) the snapshot static_note_pass took (for a copy: the game thread takes the next one during the update)
const StaticSnapshot& static_snapshot();
// orig: (host) static_frame_build from the snapshot `S` and EE memory as `pages` hold it (null: the memory itself)
void static_frame_build_at(const StaticLevel& L, const ResidentColours& rc, StaticFrame& f, bool check, const StaticSnapshot& S, const EePages* pages);
// orig: (host) the pages static_frame_build reads for level `L` noted in `pg` (main thread, EE memory; the class records hot)
void static_pages_note(const StaticLevel& L, EePages& pg);

// orig: (host) tfrag_decide with the level's table read from `pages` (null: the memory itself)
void tfrag_decide_at(const TfragLevel& L, const TfragSnapshot& S, bool check, std::vector<std::pair<uint32_t, uint8_t>>& out, const EePages* pages);
// orig: (host) the pages tfrag_decide reads for level `L` (the table, each entry's box) noted in `pg`
void tfrag_pages_note(const TfragLevel& L, EePages& pg);
// orig: (host) the Wii U port: a tfrag entry (the EE table's index) the skipped tfrag pass's relighting relit (overlay_entry.cpp
// world_relight, game thread), for tfrag_colours_update_touched; past 4096 notes the next update compares every entry
void tfrag_colours_touch(uint32_t index);
// orig: (host) the noted entries' colours compared alone (tfrag_colours_update's; a new level or too many notes: all of them), the
// notes cleared (`drop`: only cleared - every entry was just compared)
void tfrag_colours_update_touched(TfragColours& c, const TfragLevel& L, bool drop);

// What scene_from_static reads of EE memory besides its arguments: the tfrag pass's view and VU1 constants (tfrag_pass_vu), the tie A
// alpha-test state packet's qwords (0x1c2a30 / 0x1c2a40) and the shrub A fog constants (0x1c2ad8)
struct SceneStaticConsts {
    bool tfrag_vu = false;
    std::array<float, 16> tview{};
    std::array<uint32_t, 4 * 14> tconsts{};
    std::array<uint32_t, 4> tie_tag{}, tie_test{};
    float shrub_fog[4] = {};
};
// orig: (host) the constants read (main thread, after the update)
void scene_static_consts(SceneStaticConsts& k);
// orig: (host) scene_from_static with the constants `k` (no EE memory read)
bool scene_from_static_k(const StaticFrame& sf, const StaticLevel& sl, const std::array<float, 16>& a, const std::array<float, 3>& camera, const TfragLevel* tl,
                         const std::vector<std::pair<uint32_t, uint8_t>>* tdraws, const SceneStaticConsts& k, SceneFrame& out, const SceneFrame* sky = nullptr);

// The sky pass's range copied out of EE memory (sky_capture_copy): each tag's stream - its upper 64 bits, then its data - in the walk's
// order, and what sky_capture takes besides (the asset, its texture entries, the projection A); sky_capture_run makes the capture from it
// ((2026-10-28) the host sky, mrp_sky_host.inc: a stream made by the host, its shells' places `shells` - each a packet of its own before
// the tag `tag` - in place of their clusters' DIRECTs)
struct SkyHostMark {
    uint32_t tag = 0;
    uint8_t shell = 0;
    std::array<float, 16> m{};  // the shell's matrix as the variant left it at 0x1bd7e0 (four quadwords: the columns x, y, z, w)
};
struct SkyStream {
    bool ok = false;
    std::vector<uint8_t> bytes;
    std::vector<std::pair<uint32_t, uint32_t>> tags;  // (each tag's stream: offset, bytes)
    uint32_t outside = 0;
    uint32_t sky_asset = 0;
    std::vector<std::pair<uint64_t, uint32_t>> sky_tex;
    std::array<float, 16> sky_a{};
    std::vector<SkyHostMark> shells;  // (the host sky's; none in a capture)
};
// orig: (host) the walk of sky_capture without its codes run (main thread: EE memory); false: nothing walked
bool sky_capture_copy(uint32_t first, uint32_t end, const SceneAddrs& ad, SkyStream& s);
// orig: (host) the codes of a copied range run as sky_capture runs them (its private VU1: never while another thread runs VU1 programs).
// `vu`: the private VU1 to run them on (null: the capture's own)
bool sky_capture_run(const SkyStream& s, SceneFrame& out, bool reset, vu1::Vu1** vu = nullptr);

// ---- the host sky (2026-10-28, ports/wiiu/MRP_LITE.md "The host sky"): the sky drawn from the level's sky asset without the game's sky pass ----
// The pass's work a frame is the sky's chain: its shells' clusters culled, their vertices transformed and their triangles written as GS
// packets (0x28b8a0 / 0x28bae0), the stars as the particle program's data (0x28bd58). Here the shells are the asset's, decoded once a level
// (SkyHostModel) and drawn whole by the GPU, turned by the matrix the level's variant gives each (0x1bd7e0, in its order); the variant's
// game state runs on the game's thread in place of the pass (sky_host_frame: its RNG calls, the stars' movement); the stars' data are made
// by the game's own 0x28bd58 once a host frame, its writes put back (sky_host_stream). The stream then goes the capture's way:
// sky_capture_run (the stars on the private VU1), sky_prepend, build_triangles (the shells' places: SkyWrite mode 4, a shell; mode 5, its
// matrix's quadwords) - the GS's state along it as the capture's.

// A corner of a shell's triangle (three a triangle, in the pass's order): the vertex as the asset holds it (x, y, z: the shell's
// coordinates, a direction once turned), its S / T (x 1/4096, the GS's ST) and Q (1; the colours' shells: the RGBAQ's word, unused), RGBA
struct SkyHostCorner {
    float p[3] = {};
    float st[2] = {};
    uint32_t q = 0x3f800000u;
    uint32_t rgba = 0;
};
// a run of a shell's triangles of one texture entry (`tex`, -1: the colours' shells): corners [first, first + count)
struct SkyHostRun {
    uint32_t first = 0, count = 0;
    int32_t tex = -1;
};
struct SkyHostShell {
    bool colours = false;  // (header + 4 not 0: 0x28bae0 - RGBA from the asset, no texture)
    uint32_t clusters = 0, first = 0, count = 0;
    std::vector<SkyHostRun> runs;
};
struct SkyHostModel {
    int level = -1;
    uint32_t asset = 0;
    uint64_t serial = 0;  // (a new number for each model made)
    std::vector<SkyHostCorner> corners;
    std::vector<SkyHostShell> shells;
    std::vector<uint64_t> tex0;  // per texture entry: a TEX0 of its format (TBP / CBP the entry's own, CLD 0: never one the game makes)
};
// orig: (host) the model of the stream last made (null: none); valid until the next sky_host_stream (the prepare's worker joined first)
const SkyHostModel* sky_host_model();
// orig: (host) the frame's host sky stream (main thread, the game's update done, the chain's reader joined: EE memory this frame's): the
// ops the last sky_host_frame noted, the stars' data made by 0x28bd58 (in the widened view `widen` > 1, mrp_lod_sky_widen_real; its EE
// writes put back), the model made again at a new level. false: no ops this frame, or a level without a host variant
bool sky_host_stream(SkyStream& s, float widen);
// orig: (host) sky_host_frame noted ops not yet taken by sky_host_stream (this host frame's: the pass was not run); sky_host_forget
// drops them (no stream made this host frame)
bool sky_host_pending();
void sky_host_forget();
// orig: (host) the frame's ops noted after the pass ran (the PC's check, mrp_world_renderer.cpp sky_host_check: the pass's game state
// left as it is) - false: no host variant
bool sky_host_ops_after_pass();
// orig: (host) testing (the check): render_frame runs the pass now (g_pass_noted: not the flight's sky, the title's)
void sky_host_note_pass();
extern bool g_pass_noted;

// orig: (host) tfrag_exec_cpu with an explicit scratch slot (0 or 1, for Core 0 worker + Core 1 main thread) and optional xyzf[0..2]
bool tfrag_exec_cpu_fast(const TfragEntryGpu& e, const TfragPlan& p, int lod, const uint32_t* colours, const float view[16], const uint32_t consts[4 * 14],
                         std::vector<TfragCpuVertex>& out, std::vector<uint8_t>* adc_out, bool need_xyz, int slot);

}  // namespace rac1::mrpw

namespace rac1 {
// orig: (host) the chain's reader waited for, its error left to mrp_chain_join (any thread: mrp_chain.cpp)
void mrp_chain_wait_idle();
bool mrp_chain_is_idle();
}  // namespace rac1

#endif

