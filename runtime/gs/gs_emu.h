// A subset of the PS2's Graphics Synthesizer for the game's own 2D packets (the HUD, text, the
// texture uploads into the GS memory): the VIF1 DMA chain the game builds in EE memory is walked,
// its DIRECT data (GIF packets, path 2) runs on the GS registers, IMAGE transfers fill the 4 MB
// local memory (with its swizzled page / block / column layouts), and drawing kicks are kept as
// primitives (with the drawing state of their kick) for the host to draw. VU1 programs and their
// data (the 3D, drawn by the host from the game's structures) are skipped.
#ifndef RAC1_RUNTIME_GS_GS_EMU_H
#define RAC1_RUNTIME_GS_GS_EMU_H

#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <memory>
#include <tuple>
#include <utility>
#include <vector>

namespace rac1::gs {

struct Vertex {
    float x = 0, y = 0;   // pixels, XYOFFSET removed
    uint32_t z = 0;
    uint8_t r = 0, g = 0, b = 0, a = 0;
    float q = 1.0f, s = 0, t = 0;  // ST / Q
    float u = 0, v = 0;            // UV, texels
    uint8_t fog = 0;
};

// The drawing state a primitive was kicked with (raw GS register values, the context's).
struct DrawState {
    uint64_t prim = 0, tex0 = 0, tex1 = 0, clamp = 0, alpha = 0, test = 0, scissor = 0;
    uint64_t texa = 0, fba = 0, frame = 0, zbuf = 0, colclamp = 0, pabe = 0, fogcol = 0, dthe = 0;
    uint64_t miptbp1 = 0;  // MIPTBP1: the mip levels 1..3's bases / widths
    uint64_t xyoffset = 0;  // (host) XYOFFSET of the context: the kicked X / Y are the vertices' x / y x 16 + its x / y
};

// A texture as the local memory held it when a primitive sampled it (TEX0's size, CLUT and
// TEXA applied, RGBA with the raw alpha): the memory is rewritten during a frame (the same
// blocks reused by later uploads), so a texture is read at its draw, as the GS does.
struct TexImage {
    uint64_t id = 0;  // unique per decode
    int w = 0, h = 0;
    std::vector<uint32_t> px;
};

struct Primitive {
    // 0 point, 1 line, 2 line strip, 3 triangle, 4 strip, 5 fan, 6 sprite (assembled: 1 line, 3 triangle, 6 sprite);
    // (host) 7: IMAGE transfers into the frame (Gs::set_screen_area): `tex` the frame's picture after them
    // (fbw x 64 by the frame's height, CT32 as the memory holds it), two vertices at its corners;
    // (host) 8: the host's 3D drawn here (a skip range's host_3d in fx_id), no vertices
    int type = 0;
    DrawState st;
    std::vector<Vertex> v;  // 2 (line, sprite) or 3 (triangle)
    std::shared_ptr<const TexImage> tex;  // textured: its texture at the kick (mipmapped: level mip_lo)
    // mipmapped (TEX1 MXL > 0): the levels mip_lo + 1 .. mip_lo + mip_n its pixels may sample (the
    // GS picks the level per pixel from Q), their TEX0 (base / width / size of the level)
    int mip_lo = 0, mip_n = 0;
    uint64_t mip_tex0[3] = {};
    std::shared_ptr<const TexImage> mip_tex[3];
    bool vu = false;    // kicked by a VU1 program (XGKICK, path 1): the game's projected 3D
    bool moby = false;  // kicked specifically by the moby VU1 program
    bool wide = false;  // (host) from a substituted chain (Gs::add_chain_substitute): drawn past the frame's sides
    // (host) 1: from a substitute, 2: from the real range it stands for. The drawing takes the projected 3D from the
    // substitute and the 2D (the HUD, the text: the substitute's may use VRAM its texture uploads did not reach) from the real range.
    uint8_t sub = 0;
    uint32_t vu_start = 0;  // the micro address of the VU1 program that kicked it (testing: RAC1_GS_PRIMDBG)
    // (host) kicked inside an effects range (Gs::set_chain_fx): the MRP draws it (mrp_fx.h); its value the range's kind
    // (the producer, whose projection takes the vertices back to the world), fx_id the range's id
    uint8_t fx = 0;
    uint32_t fx_id = 0;
    // (host) kicked inside a screen layer's range (Gs::set_chain_layers): a full-screen 2D layer the game draws over its
    // 4:3 frame, stretched across a widened view (gs_draw.cpp)
    bool layer = false;
    // (host) kicked inside a HUD element's range (Gs::set_chain_hud): 1 an element at the frame's left, 2 at its right; moved
    // toward the edges of a HUD wider than 4:3 (Viewport::hud_shift, gs_draw.cpp). 0: none (centred, or not the HUD)
    uint8_t hud = 0;
    // (host) the hash of the ported VU1 program that kicked it (vu1::g_last_hash; 0: none), and the label of the chain range its tag
    // was in (Gs::set_chain_labels; 0: none): the replica's inventory (testing)
    uint64_t vu_hash = 0;
    uint32_t src = 0;
};

// (host) one batch of VU1 program 7 (the shadow-mesh program, shadows_vu1.cpp) as the program found it in VU1 memory, before it
// transformed it in place and clipped it at the guard band (Gs::set_chain_vu7): the MRP draws these itself, in the world.
struct Vu7Vertex {
    float xyz[3];    // the input point (w 1), the space the program's screen matrix takes
    float st[3];     // S, T, Q as the batch holds them (Q 1)
    uint8_t rgba[4];
};
struct Vu7Batch {
    DrawState st;                          // the GS state its kick would have (PRIM from the batch's GIF tag)
    std::shared_ptr<const TexImage> tex;   // textured: its texture (base level) at the start
    bool mip = false;                      // (TEX1 asked for mip levels: only the base level is kept)
    float m[16] = {};                      // the screen matrix vf18..vf21 (column c = vf[18 + c])
    float off[4] = {};                     // vf30: the screen offset (w: the fog's offset)
    float k = 0;                           // vf31.w: the Q numerator
    float fog[2] = {};                     // vf7.x / .y: the fog's clamp
    uint32_t first = 0, n = 0;             // its vertices in Gs::vu7_vertices()
    uint32_t start = 0;                    // 0xc (no clipping) or 0xe (the clipping path)
    uint32_t src = 0;                      // the label of its range (set_chain_labels)
    uint32_t tag = 0;                      // its vu7 range's (Gs::Vu7Range::tag)
    uint8_t sub = 0;                       // as Primitive::sub
};

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
    // Host (testing: the replica's inventory): the next chain's tags at addresses in [first, last] (inclusive; the real chain's)
    // carry `label` into their primitives (Primitive::src) and into the programs' times. Any order, not overlapping. Cleared by the
    // next chain.
    struct LabelRange {
        uint32_t first = 0, last = 0;
        uint32_t label = 0;
    };
    void set_chain_labels(std::vector<LabelRange> ranges);
    // Host (testing): the VU1 programs' run times per (label, hash, start) kept from now on
    struct ProgTime {
        uint64_t runs = 0, ns = 0;
    };
    void set_inventory(bool on) { inventory_ = on; }
    const std::map<std::tuple<uint32_t, uint64_t, uint32_t>, ProgTime>& prog_times() const { return prog_times_; }
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
    // A texel of the texture TEX0 `tex0` (CLUT applied, TEXA for 16/24 bit): RGBA, alpha raw (0x80 = 1).
    uint32_t texel(uint64_t tex0, uint64_t texa, int x, int y) const;
    uint8_t* vram() { return vram_.data(); }
    const uint8_t* vram() const { return vram_.data(); }
    uint64_t reg(uint32_t r) const { return regs_[r & 0x7f]; }
    uint64_t generation() const { return gen_; }  // bumped by every upload into the local memory

    // Local memory addressing (bytes; PSMT4: half bytes) of pixel (x, y) in a buffer at block
    // `bp` of width `bw` (in 64 pixels).
    static uint32_t addr32(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
    static uint32_t addr16(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, bool s);
    static uint32_t addr8(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);
    static uint32_t addr4(uint32_t bp, uint32_t bw, uint32_t x, uint32_t y);  // half bytes
    uint32_t read_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y) const;
    void write_pixel(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v);
    void write_pixel_raw(uint32_t psm, uint32_t bp, uint32_t bw, uint32_t x, uint32_t y, uint32_t v);
    // (host) a w x h CT32 picture written at block bp (bw x 64 wide) at once: the memory's change noted once (a store
    // hook's frame: write_pixel's per-pixel bookkeeping cost ~150 ms for a 512 x 416 frame)
    void write_ct32_rect(uint32_t bp, uint32_t bw, uint32_t w, uint32_t h, const uint32_t* px);
    std::shared_ptr<const TexImage> texture_at_kick(uint64_t tex0, uint64_t texa);

private:
    bool tag_skipped(uint32_t addr, size_t* range) const;
    template <class Mem>
    void chain_walk(uint32_t tag, uint32_t end, Mem&& mem);
    void vertex_kick(const Vertex& v, bool draw);
    void packed(const uint8_t* q, uint32_t reg);
    void image_data(const uint8_t* p, uint32_t bytes);
    void vif_stream(const uint8_t* p, uint32_t bytes, bool tag_codes);

    std::vector<uint8_t> vram_;
    uint64_t regs_[0x80] = {};
    Vertex cur_;          // RGBAQ / ST / UV / FOG accumulated
    std::vector<Vertex> queue_;
    // the host -> local transfer in progress
    bool xfer_ = false;
    uint32_t xx_ = 0, xy_ = 0;
    std::vector<uint8_t> xpend_;  // partial pixels between packets (24 / 4 bit)
    std::vector<Primitive> prims_;
    // the GIF tag in progress (its data left: quadwords, or REGLIST double words)
    uint32_t tag_left_ = 0, tag_flg_ = 0, tag_nreg_ = 1, tag_reg_ = 0;
    uint64_t tag_regs_ = 0;
    // VIF state
    uint32_t vif_cl_ = 1, vif_wl_ = 1;
    bool vu_kick_ = false;    // the GIF data being read comes from the VU1 (path 1)
    bool moby_kick_ = false;  // the GIF data comes from the moby VU1 program
    uint64_t gen_ = 0;
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
    struct VUndo {
        uint32_t a, n, before;
    };
    std::vector<VUndo> vundo_;  // the local memory's bytes the substitute's walk changed, as they were
    bool vundo_on_ = false;
    uint64_t sub_dropped_ = 0;
    uint32_t sub_size_of(int k) const;
    const uint8_t* sub_mem(uint32_t a) const;
    uint64_t skipped_tags_ = 0;
    uint32_t scr_bp_ = 0, scr_bw_ = 0, scr_h_ = 0;  // set_screen_area
    long scr_prim_ = -1;                            // the type-7 primitive whose picture is still to read
    std::shared_ptr<const TexImage> scr_snap_;
    uint64_t scr_seq_ = 0;
    uint32_t scr_snap_bp_ = ~0u, scr_snap_bw_ = 0, scr_snap_h_ = 0;
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
    std::vector<LabelRange> label_ranges_;  // set_chain_labels (sorted by first)
    uint32_t label_now_ = 0;
    bool inventory_ = false;
    std::map<std::tuple<uint32_t, uint64_t, uint32_t>, ProgTime> prog_times_;
    uint64_t prog_t0_ = 0;
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
private:
    // the textures read since the memory last changed (TEX0 without CLD, TEXA) -> image
    // (valid while the local memory it read, [rd_lo, rd_hi), has not changed since `seq`: an upload that
    // writes the bytes already there changes nothing, so a texture the game loads again each frame is
    // decoded once)
    struct TexSnap {
        uint64_t gen = ~0ull;
        std::shared_ptr<const TexImage> img;
        uint64_t seq = 0;
        uint32_t rd_lo[2] = {0, 0}, rd_hi[2] = {0, 0};  // the texture's bytes, and its CLUT's
    };
    std::map<std::pair<uint64_t, uint64_t>, TexSnap> tex_snaps_;
    uint64_t tex_ids_ = 0;
    // the writes that changed the local memory's bytes: [lo, hi) per run of changes, numbered
    struct WrRange {
        uint64_t seq;
        uint32_t lo, hi;
    };
    std::vector<WrRange> wr_log_;
    uint64_t wr_seq_ = 0;    // the last number given
    uint64_t wr_floor_ = 0;  // the log is complete after this number
    bool wr_open_ = false;   // the last entry still takes changes
    void note_change(uint32_t a, uint32_t n);
    bool snap_stale(const TexSnap& s) const;
    mutable bool rd_track_ = false;  // a texture decode notes the bytes it reads
    mutable uint32_t rd_lo_[2] = {0xffffffffu, 0xffffffffu}, rd_hi_[2] = {0, 0};
    mutable int rd_slot_ = 0;  // 0: the texture's pixels, 1: its CLUT
};

// The one GS the game's packets and uploads (libgraph's image loads, the frame's chain) go to.
Gs& gs_global();
// gs_global().vif1_chain(capture) on a worker thread (the capture kept alive until chain_join); chain_join waits for it
// (before anything else uses gs_global(): the drawing, the next chain, libgraph's image transfers)
void chain_start_async(const Gs::ChainCapture& c);
void chain_join();
// Host: called by the game's GS -> EE transfers (libgraph's 0x1227d8) before they read the local memory at block `bp`
// (`bw` x 64 pixels wide, format `psm`): the host may put there what the GS would hold - the frame it drew, which the replica
// does not draw into its memory. nullptr: none.
using StoreImageHook = void (*)(uint32_t bp, uint32_t bw, uint32_t psm);
void set_store_image_hook(StoreImageHook hook);
StoreImageHook store_image_hook();

}  // namespace rac1::gs

#endif
