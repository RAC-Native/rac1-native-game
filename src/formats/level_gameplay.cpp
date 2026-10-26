#include "io/host_endian.h"
#include <formats/level_gameplay.h>
#include <formats/scene_wad.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rac1 {

namespace {

template <class T>
T get(std::span<const std::byte> b, std::size_t at) {
    T v;
    std::memcpy(&v, b.data() + at, sizeof(T));
    le_fix_value(v);  // (numbers and records of 4-byte fields)
    return v;
}

// The file starts with 36 block offsets (0: absent). What this decoder does with each block:
enum class Kind {
    Counted,      // a row count at +0
    Cameras,      // a row count at +0, 32-byte rows from +0x10
    Volumes,      // a row count at +0, 128-byte rows from +0x10
    Paths,        // a path table (count, data offset, -, -), then the paths
    Mobys,        // (count, spawnable count, -, -), then 0x78-byte records
    PvarSpans,    // 8-byte rows filling the block
    PvarData,     // bytes
    PvarFixups,   // 8-byte rows (pvar index, offset) up to a negative index
    SharedPvars,  // (data size, pointer count, ...)
    Occlusion,    // three counts (tfrag, tie, moby)
    Opaque,       // not counted
};

struct BlockInfo {
    const char* name;
    Kind kind;
};

constexpr int kBlockCount = 36;

// Slot i is the block whose offset is the file's word i.
constexpr BlockInfo kBlocks[kBlockCount] = {
    {"level settings", Kind::Opaque},
    {"directional lights", Kind::Counted},
    {"cameras", Kind::Cameras},
    {"sound instances", Kind::Counted},
    {"us english help messages", Kind::Counted},
    {"uk english help messages", Kind::Counted},
    {"french help messages", Kind::Counted},
    {"german help messages", Kind::Counted},
    {"spanish help messages", Kind::Counted},
    {"italian help messages", Kind::Counted},
    {"japanese help messages", Kind::Counted},
    {"korean help messages", Kind::Counted},
    {"tie classes", Kind::Counted},
    {"tie instances", Kind::Counted},
    {"shrub classes", Kind::Counted},
    {"shrub instances", Kind::Counted},
    {"moby classes", Kind::Counted},
    {"moby instances", Kind::Mobys},
    {"moby groups", Kind::Counted},
    {"shared pvar data", Kind::SharedPvars},
    {"pvar moby links", Kind::PvarFixups},
    {"pvar table", Kind::PvarSpans},
    {"pvar data", Kind::PvarData},
    {"pvar pointers", Kind::PvarFixups},
    {"cuboids", Kind::Volumes},
    {"spheres", Kind::Volumes},
    {"cylinders", Kind::Volumes},
    {"pills", Kind::Volumes},
    {"paths", Kind::Paths},
    {"grind paths", Kind::Counted},
    {"light grid", Kind::Opaque},
    {"point lights", Kind::Counted},
    {"environment changes", Kind::Counted},
    {"camera collision grid", Kind::Opaque},
    {"environment samples", Kind::Counted},
    {"occlusion", Kind::Occlusion},
};

constexpr std::size_t kMobyRecordBytes = 0x78;

MobyPlacement moby_placement(std::span<const std::byte> f, std::size_t r, std::int32_t id) {
    MobyPlacement m;
    m.id = id;
    m.class_id = get<std::int32_t>(f, r + 0x18);
    m.scale = get<float>(f, r + 0x1c);
    m.draw_distance = get<float>(f, r + 0x20);
    m.update_distance = get<std::int32_t>(f, r + 0x24);
    m.position = get<Vec3f>(f, r + 0x30);
    m.rotation = get<Vec3f>(f, r + 0x3c);
    m.group = get<std::int32_t>(f, r + 0x48);
    m.rooted = get<std::int32_t>(f, r + 0x4c);
    m.rooted_distance = get<float>(f, r + 0x50);
    m.pvar_index = get<std::int32_t>(f, r + 0x58);
    m.occlusion = get<std::int32_t>(f, r + 0x5c);
    m.mode_bits = get<std::int32_t>(f, r + 0x60);
    m.colour = get<Rgb96>(f, r + 0x64);
    m.light = get<std::int32_t>(f, r + 0x70);
    m.word_4 = get<std::int32_t>(f, r + 0x04);
    m.word_8 = get<std::int32_t>(f, r + 0x08);
    m.word_c = get<std::int32_t>(f, r + 0x0c);
    m.word_10 = get<std::int32_t>(f, r + 0x10);
    m.word_14 = get<std::int32_t>(f, r + 0x14);
    m.word_54 = get<std::int32_t>(f, r + 0x54);
    m.word_74 = get<std::int32_t>(f, r + 0x74);
    return m;
}

// The paths block: (path count, the points' base offset from the block, -, -), the paths' offsets
// from that base (s32 each, from +0x10); a path: its point count at +0, the points from +0x10.
void read_paths(std::span<const std::byte> f, std::uint32_t block, GameplayBlockSummary& sum, DecodedGameplay& out) {
    if (block + 16 > f.size()) return;
    const std::int32_t count = get<std::int32_t>(f, block);
    const std::int32_t base = get<std::int32_t>(f, block + 4);
    sum.row_count = static_cast<std::uint32_t>(count);
    for (std::int32_t i = 0; i < count; ++i) {
        const std::uint32_t slot = block + 0x10 + i * 4;
        if (slot + 4 > f.size()) break;
        const std::uint32_t path = block + static_cast<std::uint32_t>(base) + get<std::int32_t>(f, slot);
        if (path + 4 > f.size()) continue;
        const std::int32_t points = get<std::int32_t>(f, path);
        if (points <= 0) continue;
        GameplayPath p;
        p.points.reserve(points);
        for (std::int32_t k = 0; k < points; ++k) {
            const std::uint32_t at = path + 0x10 + k * 16;
            if (at + 16 > f.size()) break;
            p.points.push_back(get<Vec4f>(f, at));
        }
        out.paths.push_back(std::move(p));
    }
}

} // namespace

DecodedGameplay decode_level_gameplay(std::span<const std::byte> data) {
    std::vector<std::uint8_t> unpacked;
    std::span<const std::byte> f = data;
    if (data.size() >= 3 && std::memcmp(data.data(), "WAD", 3) == 0) {
        if (!decompress_wad(data, unpacked)) throw std::runtime_error("Failed to decompress gameplay WAD");
        f = std::span<const std::byte>(reinterpret_cast<const std::byte*>(unpacked.data()), unpacked.size());
    }
    if (f.size() < kBlockCount * 4) throw std::runtime_error("Gameplay data too small for its block table");

    // A block's size: up to the next block's start, or the end of the file.
    std::int32_t start[kBlockCount];
    std::memcpy(start, f.data(), sizeof start);
    struct Start {
        std::size_t slot;
        std::uint32_t at;
        std::uint32_t size = 0;
    };
    std::vector<Start> by_offset;
    for (std::size_t i = 0; i < kBlockCount; ++i) {
        if (start[i] != 0) by_offset.push_back({i, static_cast<std::uint32_t>(start[i])});
    }
    std::sort(by_offset.begin(), by_offset.end(), [](const Start& a, const Start& b) { return a.at < b.at; });
    std::uint32_t size_of[kBlockCount] = {};
    for (std::size_t i = 0; i < by_offset.size(); ++i) {
        const std::uint32_t end = i + 1 < by_offset.size() ? by_offset[i + 1].at : static_cast<std::uint32_t>(f.size());
        if (end > by_offset[i].at) size_of[by_offset[i].slot] = end - by_offset[i].at;
    }

    DecodedGameplay out;
    std::vector<VolumeShape>* volumes[] = {&out.cuboids, &out.spheres, &out.cylinders, &out.pills};
    int volume_kind = 0;
    for (int slot = 0; slot < kBlockCount; ++slot) {
        const BlockInfo& info = kBlocks[slot];
        const std::uint32_t at = static_cast<std::uint32_t>(start[slot]), size = size_of[slot];
        const bool is_volume = info.kind == Kind::Volumes;
        std::vector<VolumeShape>* volume_list = is_volume ? volumes[volume_kind++] : nullptr;
        if (at == 0) continue;
        GameplayBlockSummary sum;
        sum.name = info.name;
        sum.present = true;
        sum.offset = at;
        sum.size = size;
        switch (info.kind) {
        case Kind::Counted:
            if (at + 4 <= f.size()) sum.row_count = static_cast<std::uint32_t>(get<std::int32_t>(f, at));
            break;
        case Kind::Cameras:
            if (at + 16 <= f.size()) {
                const std::int32_t n = get<std::int32_t>(f, at);
                sum.row_count = static_cast<std::uint32_t>(n);
                out.cameras.reserve(n);
                for (std::int32_t i = 0; i < n; ++i) {
                    const std::uint32_t r = at + 0x10 + i * static_cast<std::uint32_t>(sizeof(CameraPlacement));
                    if (r + sizeof(CameraPlacement) > f.size()) break;
                    out.cameras.push_back(get<CameraPlacement>(f, r));
                }
            }
            break;
        case Kind::Volumes:
            if (at + 16 <= f.size()) {
                const std::int32_t n = get<std::int32_t>(f, at);
                sum.row_count = static_cast<std::uint32_t>(n);
                std::vector<VolumeShape> rows;
                rows.reserve(n);
                for (std::int32_t i = 0; i < n; ++i) {
                    const std::uint32_t r = at + 0x10 + i * static_cast<std::uint32_t>(sizeof(VolumeShape));
                    if (r + sizeof(VolumeShape) > f.size()) break;
                    rows.push_back(get<VolumeShape>(f, r));
                }
                *volume_list = std::move(rows);
            }
            break;
        case Kind::Paths:
            read_paths(f, at, sum, out);
            break;
        case Kind::Mobys:
            if (at + 16 <= f.size()) {
                const std::int32_t n = get<std::int32_t>(f, at);  // the placed mobys (spawnable count at +4)
                sum.row_count = static_cast<std::uint32_t>(n);
                out.mobys.reserve(n);
                for (std::int32_t i = 0; i < n; ++i) {
                    const std::uint32_t r = at + 16 + i * static_cast<std::uint32_t>(kMobyRecordBytes);
                    if (r + kMobyRecordBytes > f.size()) continue;
                    const std::int32_t record_size = get<std::int32_t>(f, r);
                    if (record_size != 0x78) {
                        throw std::runtime_error("Invalid moby instance size field: expected 0x78, got " + std::to_string(record_size));
                    }
                    out.mobys.push_back(moby_placement(f, r, i));
                }
            }
            break;
        case Kind::PvarSpans:
            sum.row_count = size / 8;
            out.pvar_spans.reserve(sum.row_count);
            for (std::uint32_t i = 0; i < sum.row_count; ++i) {
                if (at + i * 8 + 8 <= f.size()) out.pvar_spans.push_back(get<PvarSpan>(f, at + i * 8));
            }
            break;
        case Kind::PvarData:
            sum.row_count = size;
            if (at + size <= f.size()) {
                const auto* b = reinterpret_cast<const std::uint8_t*>(f.data() + at);
                out.pvar_data.assign(b, b + size);
            }
            break;
        case Kind::PvarFixups: {
            std::uint32_t n = 0;
            for (std::uint32_t r = 0; r + 8 <= size; r += 8, ++n) {
                if (get<std::int32_t>(f, at + r) < 0) break;
            }
            sum.row_count = n;
            break;
        }
        case Kind::SharedPvars:
            if (at + 8 <= f.size()) sum.row_count = static_cast<std::uint32_t>(get<std::int32_t>(f, at + 4));
            break;
        case Kind::Occlusion:
            if (at + 12 <= f.size()) {
                sum.row_count = get<std::int32_t>(f, at) + get<std::int32_t>(f, at + 4) + get<std::int32_t>(f, at + 8);
            }
            break;
        case Kind::Opaque:
            break;
        }
        out.blocks.push_back(std::move(sum));
    }
    return out;
}

} // namespace rac1
