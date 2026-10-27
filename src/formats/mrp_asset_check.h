// (testing) RAC1_MRP_ASSET_CHECK=<level>: the level's MRP asset files read with the format readers (read_class,
// read_textures, read_world: no GPU), one line per file with a hash of what they parsed, the same on any byte order
// (each field's value in a fixed order, a number as its little-endian bytes; blobs and the palette as their bytes).
#ifndef RAC1_FORMATS_MRP_ASSET_CHECK_H
#define RAC1_FORMATS_MRP_ASSET_CHECK_H

#include <string>

namespace rac1::mrp {

// Prints "[mrp-check] <file> <hash>" per file (moby_root/level<N>/*.mrpm by name, its textures.mrpt, then
// world_root/level<N>/world.mrpw and textures_{tfrag,tie,shrub}.mrpt; "FAILED" for one that does not read) and
// "[mrp-check] total <files> files <hash>". Returns the number of files that failed.
int asset_check(int level, const std::string& moby_root, const std::string& world_root);

}  // namespace rac1::mrp

#endif
