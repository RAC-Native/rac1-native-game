// The level-data facade (stage 5, task L1): typed accessors over the engine's level-data objects - the paths, the volumes
// and the three lists of 0x80-byte shapes, the level cameras, the grind rails, the moby groups, and the camera-volume and zone
// grids. For now each accessor reads the same EE words the original code reads, through OV() (the played level's own
// overlay, the region tables, a session's view), with the original's widths: what a reader gets is bit for bit what it got
// from its own `lwu(OV(...))`. Later (task L3a) the same calls read the native level registry instead, and no reader changes
// again.
//
// Rules:
// - Nothing is cached here: the open engine's ld_enter / ld_leave (enhance/leveldata.cpp) swap these EE words around every
//   update of a moby of an imported class, and the world sections swap them per section, so every call reads them again.
// - Every accessor is inline (one or two reads): the cameras, the rails and the volumes are read every frame.
// - Each address literal sits inside its own OV() (region rule 1: region_maps.py check-levels sees it); a list chosen by
//   an argument is a switch of three literals, folded away when the argument is a constant.
// - The values are EE addresses (an entry, a record) and counts: the records' fields are read by the callers through the
//   usual accessors (lw, lwc1, V() ...), never through a host pointer with an assumed byte order.
// - Reading only: the writers (gameplay_load, the open engine's registries, the set pieces, the sections) stay as they are.
//
// The objects (Veldin's addresses, the same as enhance/leveldata.h's LD_*):
//   paths      a table of 176 path addresses at 0x1b04b0, the count at 0x160104, the paths' data block at 0x160108; a path
//              is its point count (a word), 12 bytes, then 16-byte points (x y z, the w lane a per-point value)
//   volumes    the 0x90-byte records (pointer 0x1600e4, count 0x1600e8: a matrix +0, the centre +0x30, the matrix to unit
//              space +0x40, the end caps' radius +0x80) and three lists of 0x80-byte shapes (the same layout without the
//              radius): boxes (0x1600ec / 0x1600f0), spheres (0x1600f4 / 0x1600f8), cylinders (0x1600fc / 0x160100)
//   cameras    the level cameras, 0x20 each (pointer 0x15ef50, count 0x15ef54); +0x1c the camera's data block
//   rails      the grind rails, 0x20 each (pointer 0x15f70c, count 0x15f710): centre +0, radius +0xc, path +0x10, loop +0x14
//   groups     112 words at 0x1ab840 (a group's list of moby indices), the count (one past the last) at 0x15fff4
//   grids      the camera-volume grid's address (*0x15ef80) and the zone grid's (*0x15f600)
#ifndef RAC1_GAME_LEVEL_LEVEL_DATA_H
#define RAC1_GAME_LEVEL_LEVEL_DATA_H

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

#include <cstdint>

// `lvd`, not `ld`: rac1::ld() is the 64-bit load accessor (ovl_common.h).
namespace rac1::lvd {

// ---- paths

// orig: (host) the number of paths (0x160104)
inline int32_t path_count() { return lw(OV(0x00160104)); }
// orig: (host) the EE address of the paths' data block (0x160108)
inline uint32_t path_data() { return lwu(OV(0x00160108)); }
// orig: (host) the EE address of path `i` (the table 0x1b04b0; 0: none)
inline uint32_t path(int32_t i) { return lwu(OV(0x001b04b0) + static_cast<uint32_t>(i) * 4); }

// ---- volumes

// orig: (host) the number of 0x90-byte volume records (0x1600e8)
inline int32_t volume_count() { return lw(OV(0x001600e8)); }
// orig: (host) the EE address of the 0x90-byte volume record `i` (*0x1600e4)
inline uint32_t volume(int32_t i) { return lwu(OV(0x001600e4)) + static_cast<uint32_t>(i) * 0x90; }

// The three lists of 0x80-byte shapes
enum BoxList : int { BOXES = 0, SPHERES = 1, CYLINDERS = 2 };
// orig: (host) the number of shapes of list `list` (0x1600f0 / 0x1600f8 / 0x160100)
inline int32_t box_count(BoxList list) {
    switch (list) {
    case BOXES: return lw(OV(0x001600f0));
    case SPHERES: return lw(OV(0x001600f8));
    default: return lw(OV(0x00160100));
    }
}
// orig: (host) the EE address of the 0x80-byte shape `i` of list `list` (*0x1600ec / *0x1600f4 / *0x1600fc)
inline uint32_t box(BoxList list, int32_t i) {
    uint32_t base;
    switch (list) {
    case BOXES: base = lwu(OV(0x001600ec)); break;
    case SPHERES: base = lwu(OV(0x001600f4)); break;
    default: base = lwu(OV(0x001600fc)); break;
    }
    return base + static_cast<uint32_t>(i) * 0x80;
}

// ---- level cameras

// orig: (host) the number of level cameras (0x15ef54)
inline int32_t camera_count() { return lw(OV(0x0015ef54)); }
// orig: (host) the EE address of level camera `i`'s 0x20-byte entry (*0x15ef50)
inline uint32_t camera(int32_t i) { return lwu(OV(0x0015ef50)) + static_cast<uint32_t>(i) * 0x20; }
// orig: (host) the EE address of level camera `i`'s data block (its entry's +0x1c)
inline uint32_t camera_data(int32_t i) { return lwu(camera(i) + 0x1c); }

// ---- grind rails

// orig: (host) the number of grind rails (0x15f710)
inline int32_t rail_count() { return lw(OV(0x0015f710)); }
// orig: (host) the EE address of rail `i`'s 0x20-byte record (*0x15f70c)
inline uint32_t rail(int32_t i) { return lwu(OV(0x0015f70c)) + static_cast<uint32_t>(i) * 0x20; }

// ---- moby groups (EE-backed until stage 7)

// orig: (host) the number of groups (0x15fff4: one past the last)
inline int32_t group_count() { return lw(OV(0x0015fff4)); }
// orig: (host) the EE address of group `i`'s list of moby indices (the table 0x1ab840; 0: none)
inline uint32_t group(int32_t i) { return lwu(OV(0x001ab840) + static_cast<uint32_t>(i) * 4); }

// ---- grids

// orig: (host) the EE address of the camera-volume grid (*0x15ef80: 64 x 64 cell offsets, then the cells' lists)
inline uint32_t cam_volume_grid() { return lwu(OV(0x0015ef80)); }
// orig: (host) the EE address of the zone grid (*0x15f600; 0: none)
inline uint32_t zone_grid() { return lwu(OV(0x0015f600)); }

}  // namespace rac1::lvd

#endif
