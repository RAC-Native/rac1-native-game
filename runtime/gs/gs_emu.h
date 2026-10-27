// A subset of the PS2's Graphics Synthesizer for the game's own 2D packets (the HUD, text, the
// texture uploads into the GS memory): the VIF1 DMA chain the game builds in EE memory is walked,
// its DIRECT data (GIF packets, path 2) runs on the GS registers, IMAGE transfers fill the 4 MB
// local memory (with its swizzled page / block / column layouts), and drawing kicks are kept as
// primitives (with the drawing state of their kick) for the host to draw. VU1 programs and their
// data (the 3D, drawn by the host from the game's structures) are skipped.
#ifndef RAC1_RUNTIME_GS_GS_EMU_H
#define RAC1_RUNTIME_GS_GS_EMU_H

#include <array>
#include <runtime/gs/gs_memory.h>
#include <runtime/gs/gs_types.h>

#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <utility>
#include <vector>

namespace rac1::gs {

class Gs {
public:
    Gs();
    void reset_frame() {
        prims_.clear();
        tag_left_ = 0;
        fx_mark_ = SIZE_MAX;
        moby_mark_ = SIZE_MAX;
        fx_external_ = false;
        scr_prim_ = -1;
        vu7_batches_.clear();
        vu7_verts_.clear();
    }
    // Host: the frame the game draws into (block `bp`, `bw` x 64 pixels wide, `h` rows, CT32; bw 0: none). The GS
    // replica draws no pixels into its memory (the host draws its primitives), so an IMAGE transfer into the frame - the game
    // putting a stored picture back as the frame, the Vendor's screens (0x2a0fc8) - is kept as a primitive of type 7 in the
    // primitives' order: the picture of the frame's memory once the transfers in a row are done (before the next primitive, or
    // at the chain's end). Kept until set again.
    void set_screen_area(uint32_t bp, uint32_t bw, uint32_t h) { scr_bp_ = bp; scr_bw_ = bw; scr_h_ = h; }
    // A GS register write (A+D / REGLIST / the PACKED forms' results).
    void write_reg(uint32_t reg, uint64_t value);
    // GIF packets (path 2 / 3) of `qwc` quadwords at `p` (EE memory, host pointer).
    void gif(const uint8_t* p, uint32_t qwc);
    // The VIF1 DMA chain (source chain mode, tags with VIF codes) from `tag` (EE address), up
    // to `end` (its last tag + 1) when the chain has no END.
    void vif1_chain(uint32_t tag, uint32_t end);
    // the same in two steps: the tags and data it reads captured (on the game's thread, after the skips are set), then the
    // chain run on the capture (any thread; the skips set before the capture apply)
    struct ChainCapture {
        uint32_t tag = 0, end = 0;
        std::unique_ptr<uint8_t[]> bytes;  // (grown without zeroing: copied over at once)
        size_t size = 0, cap = 0;
        std::vector<std::array<uint32_t, 3>> spans;  // (EE address, offset in bytes, length; 0xffffffff: a null pointer)
        std::vector<std::array<uint32_t, 3>> subs;  // the substitutes the capture was made with (add_chain_substitute): first, end, size
        float side_widen[2] = {1.0f, 1.0f};         // set_side_widen's as the capture was made
    };
    void capture_chain(uint32_t tag, uint32_t end, ChainCapture& c) const;
    void vif1_chain(const ChainCapture& c);
    // Host (the MRP skipping the GS / VU1 work of the mobys): the DMA tags of the next
    // vif1_chain at an address in one of the ranges [first, last] (tag addresses, inclusive) are walked but
    // their VIF codes and data are not processed (the skipped skinning leaves the codes the VU0 wrote
    // there unwritten). The range's `mscals` (the VU1 program starts the range's chain makes, in order) are
    // run when its first tag is met, so that the VU1's double buffer flips as it does with the data.
    // Cleared by the next vif1_chain.
    struct SkipRange {
        uint32_t first = 0, last = 0;
        std::vector<uint16_t> mscals;
        // (host) non-zero: the host draws this range's 3D itself (the MRP's moby drawn into a render target, the menus'
        // item models): at the range's first tag a type-8 primitive goes into the primitives' order, with the drawing state of
        // context 1 then (its FRAME the target) and fx_id = host_3d (the MRP: the moby's address)
        uint32_t host_3d = 0;
        // (host) a range of the moby pass's draw loop (the MRP's mobys): moby_mark() is the primitives' count when the first of
        // these is met, where the MRP's mobys go in the primitives' order
        bool moby = false;
    };
    void set_chain_skips(std::vector<SkipRange> ranges);
    // (host) the primitives' count when the chain met its first SkipRange::moby range (SIZE_MAX: none): what the GS replica
    // draws before it (the draw lists that run before the moby pass, 0x1f85b8) belongs under the MRP's mobys
    size_t moby_mark() const { return moby_mark_; }
    // Host (the widescreen's sides): the next chain's range [first, end) (its tags from the tag at `first` up to the
    // tag at `end`) is preceded by `chain`, the chain a shadow run of the same code wrote at `first` (mrp_lod.h): that chain is
    // walked first (its tags and data at [first, first + size) read from `chain`, the rest from EE memory, the skip ranges not
    // applied), its primitives marked sub 1 / `wide`; then the VU1, VIF, GIF and GS register state and the local memory it changed
    // are put back as they were before it and the real range is walked, its primitives marked sub 2 (the drawing picks: Primitive
    // ::sub). Several (kMaxSubs at most), one per range, the ranges disjoint; substitute k's effects ranges are FxRange::sub k + 1.
    // Cleared by the next chain.
    static constexpr int kMaxSubs = 4;
    void add_chain_substitute(uint32_t first, uint32_t end, std::vector<uint8_t> chain);
    // Host (the widescreen's sides): the view the next chains' 3D is drawn into is `wx` x `wy` times as wide as the game's 4:3
    // frustum (1 1: the game's own). While a chain is walked the VU1 programs see it (vu1::g_side_widen): a primitive past the
    // 4:3 sides that the wider view shows is not dropped (the shadow program's side test, shadows_vu1.cpp). Kept until set again.
    void set_side_widen(float wx, float wy) { side_widen_[0] = wx; side_widen_[1] = wy; }
    uint64_t skipped_tags() const { return skipped_tags_; }
    // Host (the MRP's particles): the next chain's tags at addresses in [first, last] (inclusive; `sub`: the substitute's
    // tags, add_chain_substitute, else the real chain's) are processed as usual - VIF codes, the VU1 programs, the GS registers -,
    // their primitives marked `fx`. `external`: the MRP draws them, draw() leaves them. fx_mark(): the primitives' count when the
    // first range was met (SIZE_MAX: none), where the MRP's draw goes in the primitives' order. Cleared by the next chain.
    // `kind` (non-zero) and `id` are copied to the primitives (Primitive::fx / fx_id); the ranges may come in any order
    // and must not overlap.
    struct FxRange {
        uint32_t first = 0, last = 0;
        uint8_t sub = 0;  // 0: the real chain's tags, k: substitute k - 1's (in the order add_chain_substitute was called)
        uint8_t kind = 1;
        uint32_t id = 0;
    };
    void set_chain_fx(std::vector<FxRange> ranges, bool external);
    // Host (a widened view): the next chain's tags at addresses in [first, last] (inclusive; the real chain's, not a
    // substitute's) belong to a full-screen 2D layer the game draws over its whole 4:3 frame in pieces (the Visibomb's
    // out-of-range static: a grid of 32 x 32 sprites); their primitives marked `layer`. Any order, not overlapping. Cleared by
    // the next chain.
    void set_chain_layers(std::vector<std::pair<uint32_t, uint32_t>> ranges);
    // Host (a HUD wider than 4:3): the next chain's tags at addresses in [first, last] (inclusive; the real chain's) belong to a
    // HUD element anchored at the frame's left (side 1) or right (side 2) edge; their primitives marked `hud`. Any order, not
    // overlapping. Cleared by the next chain.
    struct HudRange {
        uint32_t first = 0, last = 0;
        uint8_t side = 0;
    };
    void set_chain_hud(std::vector<HudRange> ranges);
    // Host (the MRP's VU1 program 7 surfaces): the next chain's tags at addresses in [first, last] (inclusive; `sub` as FxRange's):
    // each start of the program whose code hashes to `hash` at 0xc / 0xe there has its batch kept (vu7_batches) as the program
    // found it, and a primitive of type 9 (fx `kind`, fx_id the batch's index) put in the primitives' order where it starts; the
    // primitives it kicks are marked fx `kind`, fx_id kVu7Kicked (the MRP draws the batch instead: draw() leaves them). Sets
    // fx_external(). Any order, not overlapping. Cleared by the next chain (the batches by reset_frame).
    struct Vu7Range {
        uint32_t first = 0, last = 0;
        uint8_t sub = 0;
        uint32_t tag = 0;  // copied to the batches (Vu7Batch::tag: the MRP's emissive surfaces' class)
    };
    static constexpr uint32_t kVu7Kicked = 0xffffffffu;
    void set_chain_vu7(std::vector<Vu7Range> ranges, uint64_t hash, uint8_t kind);
    const std::vector<Vu7Batch>& vu7_batches() const { return vu7_batches_; }
    const std::vector<Vu7Vertex>& vu7_vertices() const { return vu7_verts_; }
    // (host) the program starts on the walk's VU1 (vu1::StartObserver)
    void vu1_before(void* vu, uint32_t start, uint64_t hash);
    void vu1_after(void* vu, uint32_t start, uint64_t hash);
    size_t fx_mark() const { return fx_mark_; }
    bool fx_external() const { return fx_external_; }

    const std::vector<Primitive>& primitives() const { return prims_; }
    uint64_t reg(uint32_t r) const { return regs_[r & 0x7f]; }
    // the local memory the walk uploads into (gs_memory.h; the one local_memory())
    LocalMemory& memory() { return mem_; }
    const LocalMemory& memory() const { return mem_; }
    uint32_t texel(uint64_t tex0, uint64_t texa, int x, int y) const { return mem_.texel(tex0, texa, x, y); }
    uint8_t* vram() { return mem_.vram(); }
    const uint8_t* vram() const { return mem_.vram(); }
    uint64_t generation() const { return mem_.generation(); }
    static uint32_t addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) { return LocalMemory::addr32(bp, bw, x, y); }
    uint32_t read_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) const { return mem_.read_pixel(psm, bp, bw, x, y); }
    void write_ct32_rect(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h, const uint32_t* px) { mem_.write_ct32_rect(bp, bw, w, h, px); }

private:
    bool tag_skipped(uint32_t addr, size_t* range) const;
    template <class Mem>
    void chain_walk(uint32_t tag, uint32_t end, Mem&& mem);
    void vertex_kick(const Vertex& v, bool draw);
    void packed(const uint8_t* q, uint32_t reg);
    void vif_stream(const uint8_t* p, uint32_t bytes, bool tag_codes);

    uint64_t regs_[0x80] = {};
    Vertex cur_;          // RGBAQ / ST / UV / FOG accumulated
    std::vector<Vertex> queue_;
    LocalMemory& mem_;
    std::vector<Primitive> prims_;
    // the GIF tag in progress (its data left: quadwords, or REGLIST double words)
    uint32_t tag_left_ = 0, tag_flg_ = 0, tag_nreg_ = 1, tag_reg_ = 0;
    uint64_t tag_regs_ = 0;
    // VIF state
    uint32_t vif_cl_ = 1, vif_wl_ = 1;
    bool vu_kick_ = false;    // the GIF data being read comes from the VU1 (path 1)
    bool moby_kick_ = false;  // the GIF data comes from the moby VU1 program
    std::vector<SkipRange> skip_ranges_;  // sorted by first
    std::vector<bool> skip_started_;
    bool dry_ = false;                                        // the tag in hand is in a skipped range
    // the substitutes (add_chain_substitute); while walking one, addresses in it carry kSubFlag and its index (bits 26..28)
    struct Sub {
        uint32_t first = 0, end = 0;
        std::vector<uint8_t> chain;
        uint32_t replay_size = 0xffffffffu;  // (a capture's replay: the substitute's size it was made with)
    };
    std::vector<Sub> subs_;
    float side_widen_[2] = {1.0f, 1.0f};  // set_side_widen
    bool sub_wide_ = false, sub_drop_ = false;
    static constexpr uint32_t kSubFlag = 0x80000000u;
    static constexpr int kSubShift = 26;
    static uint32_t sub_flagged(uint32_t a, int k) { return a | kSubFlag | (static_cast<uint32_t>(k) << kSubShift); }
    size_t sub_prims0_ = 0;
    uint64_t sub_dropped_ = 0;
    uint32_t sub_size_of(int k) const;
    const uint8_t* sub_mem(uint32_t a) const;
    uint64_t skipped_tags_ = 0;
    uint32_t scr_bp_ = 0, scr_bw_ = 0, scr_h_ = 0;  // set_screen_area
    long scr_prim_ = -1;                            // the type-7 primitive whose picture is still to read
    void screen_transfer(uint32_t dbp);
    void screen_picture();
    std::vector<FxRange> fx_ranges_;  // (sorted by sub then first)
    bool fx_external_ = false;
    uint8_t fx_now_ = 0;
    std::vector<std::pair<uint32_t, uint32_t>> layer_ranges_;  // set_chain_layers (sorted by first)
    bool layer_now_ = false;
    std::vector<HudRange> hud_ranges_;  // set_chain_hud (sorted by first)
    uint8_t hud_now_ = 0;
    uint32_t fx_id_ = 0;
    std::vector<Vu7Range> vu7_ranges_;  // set_chain_vu7 (sorted by sub then first)
    uint64_t vu7_hash_ = 0;
    uint8_t vu7_kind_ = 0;
    bool vu7_now_ = false;   // the tag in hand is in a vu7 range
    uint32_t vu7_tag_ = 0;   // (its Vu7Range::tag)
    bool vu7_drop_ = false;  // a kept batch's program runs: its kicks marked
    std::vector<Vu7Batch> vu7_batches_;
    std::vector<Vu7Vertex> vu7_verts_;
    size_t fx_mark_ = SIZE_MAX;
    size_t moby_mark_ = SIZE_MAX;
    const FxRange* fx_tag(uint32_t addr, uint8_t sub) const;
public:
    void set_moby_kick(bool mk) { moby_kick_ = mk; }
    bool moby_kick() const { return moby_kick_; }
};

// The one GS the game's packets and uploads (libgraph's image loads, the frame's chain) go to.
Gs& gs_global();
// gs_global().vif1_chain(capture) on a worker thread (the capture kept alive until chain_join); chain_join waits for it
// (before anything else uses gs_global(): the drawing, the next chain, libgraph's image transfers)
void chain_start_async(const Gs::ChainCapture& c);
void chain_join();

}  // namespace rac1::gs

#endif
