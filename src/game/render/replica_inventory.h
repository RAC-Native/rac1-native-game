// Testing (RAC1_REPLICA_INVENTORY=1): what projected 3D the GS replica still draws, by its source - the draw-list callback whose
// chain range kicked it, the VU1 program, the kind of primitive -, per level and mode, with the programs' run times on the
// replica's VU1 and the primitives' area on the 4:3 frame. Printed at the run's end. Host code: none of it is the game's.
#ifndef RAC1_GAME_RENDER_REPLICA_INVENTORY_H
#define RAC1_GAME_RENDER_REPLICA_INVENTORY_H

#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>

#include <cstdint>

namespace rac1 {

bool replica_inventory_on();
// orig: (host) a draw-list callback `fn` wrote [first, end) of the chain (the real block's)
void replica_inventory_note(uint32_t fn, uint32_t first, uint32_t end);
// orig: (host) the chain about to be walked: the update's notes handed to the replica as its labels
void replica_inventory_chain(gs::Gs& gs);
// orig: (host) the walked chain's primitives the replica will draw (as gs::draw filters them), counted under `mode`'s key
void replica_inventory_count(const gs::Gs& gs, int prim_filter, const gs::DepthMap* depth, float fb_w, float fb_h, int level, const char* mode);
void replica_inventory_report();

}  // namespace rac1

#endif
