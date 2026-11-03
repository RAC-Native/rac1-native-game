// The disc's core loaded at the NTSC-U build's addresses (docs/architecture.md, "The region layer"). The ports read the
// base ELF's core (0x100080..0x15ef00: the libraries, the table of contents, the save block, the game's globals) at the
// NTSC-U build's addresses, never translated. The NTSC-U disc and the PAL disc 1.00 have that core at those addresses; a
// later build (1.02: the PAL disc 2.00, "Platinum") moved it (its data 0x100 bytes up, a block of it 0x108, its code by up
// to 0x300). On such a disc the region table has a `core` unit (tools/overlay_match/region_maps.py apply writes it for
// the runtime as assets/generated/overlay_map/region_core.txt): the core is copied back to the NTSC-U addresses, run by
// run, after the ELF's load, and every word of the disc's data that points into its core (the core's own data, the boot
// module's, each level overlay's as it is loaded: the table's `ptr` lines) is taken back the same way. The NTSC-U disc
// and PAL 1.00: nothing of this runs.
#ifndef RAC1_GAME_OVL_REGION_CORE_H
#define RAC1_GAME_OVL_REGION_CORE_H

#include <cstdint>

namespace rac1 {

// orig: (host) the disc's core is not at the NTSC-U addresses (region_core.txt, on another region's disc)
bool region_core_moved();
// orig: (host) after init_memory: the disc's core moved to the NTSC-U addresses, its data's and the boot module's
// pointers into it taken back; the level overlays' data from then on (LevelLoader's section callback)
void region_core_load();
// orig: (host) the words of [addr, addr + size) that point into the disc's core, taken back to the NTSC-U addresses
// (unit: the level whose overlay this is, or OVL_FRONT: the boot module; the words its table's `ptr` lines name)
void region_core_fix(int unit, uint32_t addr, uint32_t size);

}  // namespace rac1

#endif
