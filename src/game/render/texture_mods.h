// Texture replacement by content hash (docs/mods.md), the first kind of mod beyond the disc's files: a mod's
// textures/<hash>.png (any size) is drawn instead of the game's texture whose decoded pixels have that hash, by the MRP's
// textures (the mobys', the world's tfrag / tie / shrub, the sky's) and its 2D (the HUD, the menus, the fonts: the GS's
// textures as the frame samples them). The faithful renderer (RAC1_MRP=0) draws the original.
//
// The hash: FNV-1a 64 over 32-bit words - the width, the height, then the pixels, RGBA (R in the low byte) as the game's
// palette gives them, the GS's alpha (0..0x80) - written as 16 lower-case hex digits. A PNG's alpha 0..255 is taken as the
// GS's 0..0x80 (255: 0x80, opaque), as the dump writes it.
//
// The dump (a debug tool: the debug menu's Mods > Dump textures, or RAC1_TEX_DUMP=1 / =<folder>): every texture drawn
// written once as <hash>.png into mods/_dump/textures/ (or that folder), with a line in its index.txt (hash, kind, size):
// the start of a pack. The world's and the mobys' textures are dumped at the level's load, the 2D's as they are drawn.
#pragma once

#include <cstdint>
#include <filesystem>
#include <memory>
#include <vector>

namespace rac1::texture_mods {

// A replacement: RGBA with the GS's alpha, its mip chain down to 1 x 1 (each level a box filter of the one above)
struct Image {
    uint32_t w = 0, h = 0;
    std::vector<std::vector<uint32_t>> mips;
    bool opaque = true;  // every texel's alpha at 0x80
};

// orig: (none) — enhancement: textures replaced (a mod on has some) or dumped: the drawers look textures up (else nothing
// here runs: vanilla)
bool on();
// orig: (none) — enhancement: the content hash of a w x h texture (see above)
uint64_t hash(uint32_t w, uint32_t h, const uint32_t* px);
// orig: (none) — enhancement: a texture the drawer is about to make: dumped when the dump is on (`kind`: "moby", "tfrag",
// "tie", "shrub", "sky", "2d"; null: never dumped), and its replacement (null: none). Thread-safe.
std::shared_ptr<const Image> find(uint32_t w, uint32_t h, const uint32_t* px, const char* kind);
// orig: (none) — enhancement: the dump on / off (the debug menu); its folder
void dump_set(bool on);
bool dump_on();
std::filesystem::path dump_dir();
// orig: (none) — enhancement: the textures replaced so far in this run (the debug menu, the log)
size_t replaced_count();

}  // namespace rac1::texture_mods
