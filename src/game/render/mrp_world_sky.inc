// The MRP sky without the scene's walker (the Wii U's MRP-lite, ports/wiiu/MRP_LITE.md phase 5; 2026-10-28): the sky pass's range of the
// frame's chain walked in EE memory as mrp_world_scene.cpp's scene_build walks it there - its DIRECTs' GIF streams as the GS register
// writes they make, a packet each; its other VIF codes (the particle program's MPG, data and starts) on a private VU1 of its own, each
// kick a packet -, with the frame's sky asset, its texture entries and the projection A: what scene_build puts in a SceneFrame for the
// sky, so that build_triangles makes the sky's batches as on the PC (the GS's state along the streams, the points as directions, the
// sky asset's textures). Host code.
//
// (A .inc, not a .h: the generated code is fingerprinted with every header of src/game - tools/setup.py's CODE_SOURCES - and a
// renderer's change must not ask for ./setup.sh again; as mrp_world_backend.inc.)
#ifndef RAC1_GAME_RENDER_MRP_WORLD_SKY_INC
#define RAC1_GAME_RENDER_MRP_WORLD_SKY_INC

#include <game/render/mrp_world_scene.h>

#include <cstdint>

namespace rac1::mrpw {

// orig: (host) the sky's range [first, end) of the frame's chain (the sky pass's tags: scene_passes()'s pass 3) walked in EE memory, into
// `out` (emptied first): its packets (ScenePacket::sky, table 3) and their writes (SceneFrame::sky), the sky asset and its texture entries
// holding a TEX0 now, the projection A, has_sky. Game thread, after the update (the chain and the entries are this frame's), and while no
// other thread runs VU1 programs (the private VU1's program starts share the VU1 runtime's per-thread state: on the Wii U, one for all
// threads). `reset`: the private VU1 started again (a new level). Every tag reached from the range is the sky's (the walker takes a tag
// called out of the range for the frame environment's: counted in `outside`, none seen on Veldin). false: nothing walked
bool sky_capture(uint32_t first, uint32_t end, const SceneAddrs& ad, SceneFrame& out, bool reset, uint32_t* outside = nullptr);
// orig: (host) the sky of a capture put before the frame's own packets in `f` (the sky's range comes first in the chain): its packets
// first, their writes first in SceneFrame::sky (the GIF packets' ranges after them moved), the frame's register notes moved past them
// (none from mrpw::scene_from_static), the sky's asset, entries and projection taken
void sky_prepend(SceneFrame& f, const SceneFrame& sky);

}  // namespace rac1::mrpw

#if defined(__WIIU__)
namespace rac1 {
// orig: (host) the frame's sky captured for mrp_world_wiiu_prepare (mrpw::sky_capture of the sky pass's range, the MRP chain's reader
// joined first: its VU1 programs and the capture's never run at once), when the GX2 backend draws the sky (mrp_world_gx2_sky_ready) and
// `walk` (the frame's own chain, no fade). orig_main.cpp's walk branch, right before the chain's reading starts
void mrp_world_wiiu_sky_capture(bool walk);
// orig: (host) the game's sky pass run (overlay_entry.cpp sky_wanted): not while the GX2 world is up and the sky is off (ports/wiiu/game
// mrp_world_gx2.cpp: its work - ~7 ms a frame on the console - is the sky's chain alone, which nothing reads then; the pass also updates
// a little state the moby pass reads, so the game's frames then differ from the PC's by that much). Without the picture (the console's
// determinism runs) it runs, as on the PC
bool mrp_world_wiiu_sky_pass_wanted();
}  // namespace rac1
#endif

#endif
