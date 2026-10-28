// The MRP's reader of the frame's DMA chain (host code, none of it the game's). The game still builds a frame's chain - its VIF
// codes, its GIF packets, its texture uploads - and the MRP takes from it what it draws itself:
//  - the 2D (the HUD, text, menus, fades, tints, the menus' pictures drawn into the GS's memory): the GIF packets read directly,
//    each primitive with the GS state it was kicked with and its texture (decoded once from the GS's local memory, gs_memory.h),
//    drawn by mrp_2d.h;
//  - the 3D effects (mrp_fx.h): the ranges the producers noted, their VU1 programs (the particles', program 7's) run on the
//    reader's own VU1, the kicked primitives kept for mrp_fx, program 7's batches taken as the program finds them;
//  - the texture uploads written into the GS's local memory, which the game reads back (libgraph's store, gs_memory.h).
// Nothing else: the world's and the mobys' ranges (the MRP draws them from their own decoding) are walked over, their VU1 program
// starts only flip the VU1's double buffer; the VU1 programs outside the effects' ranges (the moby program's set-up) run for the
// VU1's state the next ones read, their kicks dropped; no primitive is drawn by GS emulation. The GS replica
// (runtime/gs/gs_emu.h, gs_draw.h) stays the faithful reference (RAC1_MRP=0, the debug menu's "VU emulation").
//
// The chain is captured on the game's thread (its tags and data copied) and read on a worker thread of its own ("MRP chain
// worker"), joined before the drawing, the next chain and the game's own GS transfers (libgraph: gs::memory_join).
#ifndef RAC1_GAME_RENDER_MRP_CHAIN_H
#define RAC1_GAME_RENDER_MRP_CHAIN_H

#include <runtime/gs/gs_types.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

namespace rac1 {

// What a frame's chain is read with (set on the game's thread, by the chain's stage in the host's prepare).
struct MrpChainInput {
    uint32_t start = 0, end = 0;  // the chain's first tag and its end (its last tag + 1, when it has no END)
    // tags at addresses in [first, last] walked over: their VIF codes and data not read (the MRP's world and mobys, the drop
    // shadows, the moby uploads). `mscals`: the VU1 program starts the range makes (its double buffer flipped that many times
    // when its first tag is met). `host_3d` non-zero: the MRP draws this range's 3D into a render target (a type-8 primitive at
    // the range's first tag: the menus' item models). `moby`: a range of the moby pass (moby_mark).
    // `dry` false: the range is read as any other (only its marks taken: a moby's when the MRP draws the mobys but the game did
    // not skip their GS work - the Vendor's screens)
    struct Skip {
        uint32_t first = 0, last = 0;
        uint32_t mscals = 0;
        uint32_t host_3d = 0;
        bool moby = false;
        bool dry = true;
    };
    std::vector<Skip> skips;
    // the widened view's shadow chains (mrp_lod.h), each read before the real range [first, end) it stands for: the effects
    // (their 3D) from the substitute, the 2D from the real range (gs::Primitive::sub 1 / 2); the uploads and the GS state the
    // substitute changed put back before the real range
    struct Sub {
        uint32_t first = 0, end = 0;
        std::vector<uint8_t> bytes;
    };
    std::vector<Sub> subs;
    float side_widen[2] = {1.0f, 1.0f};  // the VU1 programs' side tests (vu1::g_side_widen)
    // the effects' ranges (mrp_fx: their primitives marked fx `kind`, fx_id `id`; `sub` 0 the real chain's, k substitute k - 1's)
    struct Fx {
        uint32_t first = 0, last = 0;
        uint8_t sub = 0;
        uint8_t kind = 1;
        uint32_t id = 0;
    };
    std::vector<Fx> fx;
    // VU1 program 7's ranges (the draw lists' callbacks): each start of the program whose code hashes to vu7_hash at 0xc / 0xe
    // kept as a batch (gs::Vu7Batch), its kicks read for their clipping flags only
    struct Vu7 {
        uint32_t first = 0, last = 0;
        uint8_t sub = 0;
        uint32_t tag = 0;
    };
    std::vector<Vu7> vu7;
    uint64_t vu7_hash = 0;
    uint8_t vu7_kind = 0;
    std::vector<std::pair<uint32_t, uint32_t>> layers;  // a widened view's full-screen 2D layers (gs::Primitive::layer)
    struct Hud {
        uint32_t first = 0, last = 0;
        uint8_t side = 0;
    };
    std::vector<Hud> hud;  // a HUD wider than 4:3: its side elements (gs::Primitive::hud)
    // the frame (block, width in 64 pixels, rows; bw 0: none): an IMAGE transfer into it is a type-7 primitive (the Vendor's
    // screens putting their stored picture back)
    uint32_t scr_bp = 0, scr_bw = 0, scr_h = 0;
    // (host) the tag at which the GS registers are kept as the reading has them before it (MrpChainFrame::regs_at; 0: none): the
    // Wii U's first world pass, whose registers its world backend takes in place of the walker's (MRP_LITE.md, the 2D on mrp_2d)
    uint32_t regs_tag = 0;
};

// A frame's chain as read: its primitives in the chain's order (the 2D, the effects' (fx), type 7 / 8 / 9 markers), program 7's
// batches, and where the MRP's mobys (moby_mark) and effects (fx_mark) go in that order.
struct MrpChainFrame {
    std::vector<gs::Primitive> prims;
    std::vector<gs::Vu7Batch> vu7_batches;
    std::vector<gs::Vu7Vertex> vu7_verts;
    size_t fx_mark = SIZE_MAX, moby_mark = SIZE_MAX;
    bool fx_external = false;  // effects ranges were given: mrp_fx draws them
    // the primitives the VU1 programs outside the effects' ranges kicked (not kept): 3D nobody draws (the frame before's chain walked
    // again - its world's ranges unknown, the MRP's scene not of it - shows the picture kept instead)
    uint64_t dropped_3d = 0;
    uint64_t tags = 0, skipped_tags = 0, program_runs = 0, flips = 0;  // (statistics)
    // the GS registers before the tag MrpChainInput::regs_tag (regs_at_ok false: no such tag met)
    bool regs_at_ok = false;
    uint64_t regs_at[0x80] = {};
};

// orig: (host) the frame's chain captured with `in` and its reading started (on the worker; `async` false: here, now)
void mrp_chain_start(MrpChainInput&& in, bool async);
// orig: (host) the reading waited for
void mrp_chain_join();
// orig: (host) the last chain read (after mrp_chain_join); cleared by mrp_chain_clear
const MrpChainFrame& mrp_chain_frame();
void mrp_chain_clear();
// orig: (host) the frame read last is the reader's (true) or the GS replica's (false): which one the drawing takes
bool mrp_chain_used();
void mrp_chain_set_used(bool used);
// orig: (host) a GS register as the last chain read left it: the reader's, or the GS replica's when it read the chain (the moby
// renderer's FOGCOL, the world scene's registers before the frame's chain)
uint64_t mrp_chain_gs_reg(uint32_t r);
// orig: (host) whether the reader reads the frames' chains: the MRP draws the world, the mobys and the effects (RAC1_MRP_CHAIN=0,
// testing: the GS replica reads them, as before the reader)
bool mrp_chain_wanted();
// orig: (host) testing (RAC1_MRP_CHAIN_CHECK=1): the reader's last frame against the GS replica's walk of the same chain from the
// same local memory: their primitives (the replica's that the MRP never draws left out: program 7's kicks, the effects of a real
// range a substitute stood in for, the moby programs' kicks), program 7's batches, the marks; the differences printed
void mrp_chain_check(uint64_t frame, const std::vector<gs::Primitive>& prims, const std::vector<gs::Vu7Batch>& batches,
                     const std::vector<gs::Vu7Vertex>& verts, size_t fx_mark, size_t moby_mark);

}  // namespace rac1

#endif
