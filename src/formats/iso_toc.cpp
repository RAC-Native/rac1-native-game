#include "io/host_endian.h"
#include <formats/iso_toc.h>

#include <cstring>
#include <stdexcept>

namespace rac1 {

namespace {

constexpr std::size_t SECTOR_BYTES = 0x800;
constexpr std::size_t TOC_SECTOR = 1500;
constexpr std::int32_t LEVEL_HEADER_BYTES = 0x2434;

// The first 24 bytes of a level's header, as stored.
struct LevelHeaderHead {
    std::int32_t id;
    std::int32_t header_bytes;
    std::uint32_t data_lba, data_sectors;
    std::uint32_t gameplay_ntsc_lba, gameplay_ntsc_sectors;
};
static_assert(sizeof(LevelHeaderHead) == 24);

// Copies a T out of the image if all of it lies inside.
template <class T>
bool fetch(std::span<const std::byte> image, std::size_t at, T& out) {
    if (at > image.size() || image.size() - at < sizeof(T)) return false;
    std::memcpy(&out, image.data() + at, sizeof(T));
    le_fix_value(out);  // (numbers and records of 4-byte fields)
    return true;
}

} // namespace

std::vector<Rac1LevelTocEntry> read_rac1_iso_toc(std::span<const std::byte> iso_data) {
    const std::size_t toc_at = TOC_SECTOR * SECTOR_BYTES;

    std::int32_t preamble[2];  // magic, size in bytes
    if (!fetch(iso_data, toc_at, preamble)) {
        throw std::runtime_error("ISO too small to contain R&C1 TOC at LBA 1500");
    }
    if (preamble[0] != 1) {
        throw std::runtime_error("ISO TOC magic mismatch: expected 1, got " + std::to_string(preamble[0]));
    }
    const std::int64_t toc_bytes = preamble[1];
    if (toc_bytes < 8 || toc_at + static_cast<std::uint64_t>(toc_bytes) > iso_data.size()) {
        throw std::runtime_error("ISO TOC size unreasonable or truncated: " + std::to_string(preamble[1]));
    }

    std::vector<Rac1LevelTocEntry> levels;
    for (std::int64_t row = 8; row + 4 <= toc_bytes; row += 8) {
        std::uint32_t sector = 0;
        if (!fetch(iso_data, toc_at + static_cast<std::size_t>(row), sector) || sector == 0) continue;

        LevelHeaderHead head;
        if (!fetch(iso_data, static_cast<std::size_t>(sector) * SECTOR_BYTES, head)) continue;
        if (head.header_bytes != LEVEL_HEADER_BYTES) continue;  // not a level header

        levels.push_back({head.id, sector, head.data_lba, head.data_sectors,
                          head.gameplay_ntsc_lba, head.gameplay_ntsc_sectors});
    }
    return levels;
}

} // namespace rac1
