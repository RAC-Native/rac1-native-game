// The level composer (docs/mods.md, "Level mods"): a level mod's level folder (format 1: our own text and OBJ files, no
// game data) composed with the player's own disc files into the level's build, which the game reads on a host level
// slot (src/host/level_slots.h). `rac1-native --setup --level-mods` composes every level mod in mods/ whose build is
// missing or old (the setup's last step), `--level-mod <folder>` one, always.
//
// The build, assets/generated/mods/<folder>/ (made from the player's disc: never in the repository, never in the mod):
//   data.wad          the level's data file: the base level's code, sound bank, HUD and VRAM image, a new asset index,
//                     and new asset data: no tfrags, no ties, no shrubs, no occlusion; the base level's sky, moby
//                     classes, hero sequences, gadgets and effects; the collision made from the mod's collision mesh
//   gameplay.bin      the level's gameplay file: the base level's settings, lights, generic cameras and help texts; the
//                     hero at the mod's start; the ship (or none); its take-off path; no other moby
//   host_meshes.mrpc  the mod's meshes, which the host draws (src/formats/host_meshes.h)
//   mrp/world/level<base>/   the MRP world of the composed data file, the host meshes' classes and textures added
//   build.json        the stamp: the composer's version, the mod's files' stamp (runtime/kernel/mods.h)
//   compose.log       what the composer did
//
// The level folder (mods/<folder>/level/, format 1):
//   level.txt         one setting a line ('#' comments): see level_compose.cpp, kLevelTxtHelp
//   collision.obj     the collision: its triangles (OBJ: v, f; `usemtl type<N>` gives the faces after it surface type N)
//   meshes.obj (+ .mtl, PNG textures)   what is drawn: each object (`o`) a host mesh; each material a texture (its
//                     map_Kd PNG, else its Kd colour)
//   lights.txt        optional: the modern lighting's file for the level (data/lights/README.md's format)
#ifndef RAC1_SETUP_LEVEL_COMPOSE_H
#define RAC1_SETUP_LEVEL_COMPOSE_H

#include <string>

namespace rac1::setup {

// orig: (host) the level mod mods/<folder> composed into its build; `force`: even when its build is current. False (said
// on stderr) when it cannot be.
bool compose_level_mod(const std::string& folder, bool force);

// orig: (host) every level mod in mods/ composed whose build is missing or old (`force`: all). False when one failed.
bool compose_level_mods(bool force);

}  // namespace rac1::setup

#endif
