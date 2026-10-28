// The moby pass around its draw loop (ADR 0007): the pass's start (VU1 program reference, chain
// slots), the textures the drawn mobys need uploaded to the GS ahead of their chain, the end.
#ifndef RAC1_GAME_OVL_MOBY_PASS_H
#define RAC1_GAME_OVL_MOBY_PASS_H

#include <cstdint>

namespace rac1 {

// VIF0 DMA of the source chain at `chain` (VU0 micro programs).
// orig: 0x002a0700
void vif0_send(uint32_t chain);
// orig: 0x0024fe50
void moby_pass_start();
// orig: 0x00250038
void moby_pass();
// orig: 0x0024ff88
void moby_pass_end();
// A moby list drawn on its own (the full-screen menu's mobys): `count` mobys from `m`.
// orig: 0x0024ff08
void moby_draw_list(uint32_t m, int32_t count);
// orig: 0x0024f9c8
void moby_textures_chain();
// The class textures the drawn mobys' groups need (scratchpad 0x3a00 depths against the mip
// distances 0x19a280), uploaded at the GS block `vram` on: returns the next free block << 8.
// orig: 0x002555c0
uint32_t moby_textures(uint32_t vram);
// orig: 0x0024fad0 -- the drawn groups' texture slots into their packets' TEX0 (TBP)
void moby_texture_slots();
// orig: 0x00255870
void moby_submesh_fill();

// orig: (host), ADR 0010 step 3: with the MRP drawing the frame's mobys (set by the host per game
// frame, never in the default modes) the work only the GS / VU1 path reads is skipped: the VU0 skinning
// 0x1ee858 (moby_skin_all), the sub-mesh colour fill 0x255870, the texture slots 0x24fad0. Their outputs
// (the skinned vertices in the moby chains, the chain's colours, the TBP of the class packets' TEX0) are
// read by the GS path only:
extern bool g_moby_mrp_skip;
// orig: (host) with the skip on, RAC1_MRP_SKIP=verify: the skinning still runs and the record the skip
// would have continued with is compared with the one it returns (counted, reported by the host)
extern bool g_moby_mrp_skip_verify;
// orig: (host) testing: which skips are on (bit 0 the skinning, 1 the sub-mesh fill, 2 the texture slots, 3 the host's chain
// processing: RAC1_MRP_SKIP_PARTS, hex, default all)
extern unsigned g_moby_mrp_skip_parts;
extern uint64_t g_moby_mrp_skip_next_bad, g_moby_mrp_skip_next_ok;
// orig: (host) per-stage wall time of the moby pass's EE work (ms, summed; RAC1_FRAME_PROF): the skinning
// (moby_skin_all, incl. the poses 0x250fe0), the sub-mesh fill, the texture groups 0x2555c0, the slots 0x24fad0
extern double g_prof_moby_skin_ms, g_prof_moby_fill_ms, g_prof_moby_tex_ms, g_prof_moby_slots_ms;
// orig: (host) the MRP-only moby pass (RAC1_MOBY_PASS_MRP=1, a Wii U default; set by the host per game frame, only with all the
// skips above and the host's chain processing skipping the mobys' ranges): the draw loop and the texture uploads compute
// everything as before - the culling, the drawn flags, the LOD, the lights, the skinning records, the depths, the texture
// groups, the packet buffer's positions (0x1611c0, 0x160000, the records' chain addresses) - but do not write the mobys'
// chain parts nor the uploads' packets, read by the GS / VU1 path only: each moby's range is one NEXT over it, its parts
// given to the draw observer (moby_draw.h). With RAC1_MOBY_PASS_MRP=verify the chain is written too and the scene state
// compares the parts it is given with the ones it decodes (g_moby_pass_mrp_verify).
extern bool g_moby_pass_mrp;
extern bool g_moby_pass_mrp_verify;
// orig: (host) the DMA chain's range of the moby texture uploads of this frame's pass (tags from `first`
// to `last` inclusive: the NEXT out of the pass's chain to the NEXT back); 0 when none
extern uint32_t g_moby_upload_chain_first, g_moby_upload_chain_last;
// orig: (host) set while the LOD slider's shadow run of the pass runs (src/game/render/mrp_lod.cpp): its drop shadows
// (0x2860a8..) are not made - the MRP draws the real pass's, and the memory is put back after the run
extern bool g_moby_pass_shadow_run;

}  // namespace rac1

#endif
