#ifndef RAC1_FORMATS_SCENE_WAD_H
#define RAC1_FORMATS_SCENE_WAD_H

#include <io/byte_reader.h>
#include <vector>
#include <string>
#include <span>
#include <array>
#include <cstdint>

namespace rac1 {

// Decompresses an LZ-compressed WAD sub-asset.
// Returns true on success, false if the header magic is invalid or decompression fails.
bool decompress_wad(std::span<const std::byte> src, std::vector<std::uint8_t>& dest);

} // namespace rac1

#endif // RAC1_FORMATS_SCENE_WAD_H
