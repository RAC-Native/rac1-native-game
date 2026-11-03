#pragma once

#include <cstddef>
#include <span>
#include <string>

namespace rac1::setup {

bool export_mrp_mobys(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg = "all", bool force = false);
bool export_mrp_world(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg = "all", bool force = false);
// A custom level's world (testing, RAC1_CUSTOM_LEVEL): <dir>/data.wad's tfrags, ties and shrubs into <dir>/mrp/world/level0/.
bool export_mrp_world_custom(const std::string& custom_dir);

} // namespace rac1::setup
