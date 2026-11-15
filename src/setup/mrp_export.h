#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace rac1::setup {

bool export_mrp_mobys(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg = "all", bool force = false);
bool export_mrp_world(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg = "all", bool force = false);
// One level's world from its data file `wad` (a level mod's composed one: src/setup/level_compose.h) into `dir`
// (world.mrpw and its textures)
bool export_mrp_world_wad(std::span<const std::byte> wad, int level, const std::string& dir);

} // namespace rac1::setup
