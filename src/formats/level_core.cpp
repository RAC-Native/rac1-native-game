#include <formats/level_core.h>
#include <formats/scene_wad.h>

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rac1 {

std::span<const std::byte> level_file_body(std::span<const std::byte> file, bool* out_of_range) {
    if (out_of_range) *out_of_range = false;
    if (file.size() < 48) return file;
    std::int32_t head[6];
    std::memcpy(head, file.data(), sizeof head);
    if (head[0] != 48) return file;
    const std::size_t at = static_cast<std::size_t>(head[4]) * 0x800;
    const std::size_t n = static_cast<std::size_t>(head[5]) * 0x800;
    if (head[4] < 0 || head[5] < 0 || n > file.size() || at > file.size() - n) {
        if (out_of_range) *out_of_range = true;
        return file;
    }
    return file.subspan(at, n);
}

namespace {

bool part_fits(FileSpan s, std::size_t total) {
    return s.offset >= 0 && s.size > 0 && static_cast<std::size_t>(s.offset) + static_cast<std::size_t>(s.size) <= total;  // (no int32 sum: it can overflow)
}

// The `ref.count` rows of type T at `ref.offset` of the index; none when they do not fit.
template <class T>
std::vector<T> rows_of(std::span<const std::byte> index, TableRef ref) {
    if (ref.count <= 0 || ref.offset < 0) return {};
    const std::size_t first = static_cast<std::size_t>(ref.offset);
    if (first + static_cast<std::size_t>(ref.count) * sizeof(T) > index.size()) return {};
    std::vector<T> rows(static_cast<std::size_t>(ref.count));
    std::memcpy(rows.data(), index.data() + first, rows.size() * sizeof(T));
    return rows;
}

} // namespace

bool extract_core_from_level_wad(
    std::span<const std::byte> level_wad_data,
    std::vector<std::uint8_t>& out_core_index,
    std::vector<std::uint8_t>& out_core_data_decompressed
) {
    const std::span<const std::byte> file = level_file_body(level_wad_data);
    if (file.size() < sizeof(LevelFileHeader)) return false;
    LevelFileHeader fh;
    std::memcpy(&fh, file.data(), sizeof fh);
    if (!part_fits(fh.asset_index, file.size()) || !part_fits(fh.asset_data, file.size())) return false;

    const auto* index = reinterpret_cast<const std::uint8_t*>(file.data() + fh.asset_index.offset);
    out_core_index.assign(index, index + fh.asset_index.size);
    return decompress_wad(file.subspan(fh.asset_data.offset, fh.asset_data.size), out_core_data_decompressed);
}

DecodedLevelCoreIndex decode_level_core_index(
    std::span<const std::byte> core_index_bytes,
    std::span<const std::byte> core_data_decompressed_bytes
) {
    const std::span<const std::byte> index = core_index_bytes;
    const std::span<const std::byte> data = core_data_decompressed_bytes;
    if (index.size() < sizeof(AssetIndexHeader)) {
        throw std::runtime_error("core_index is too small to contain its header");
    }
    DecodedLevelCoreIndex out;
    std::memcpy(&out.header, index.data(), sizeof(AssetIndexHeader));
    const AssetIndexHeader& h = out.header;

    const std::vector<ClassRecord> mobys = rows_of<ClassRecord>(index, h.moby_classes);
    const std::vector<ClassRecord> ties = rows_of<ClassRecord>(index, h.tie_classes);
    const std::vector<ShrubClassRecord> shrubs = rows_of<ShrubClassRecord>(index, h.shrub_classes);

    // The hero's 256 sequence offsets, when the table fits in the index.
    std::vector<std::int32_t> sequences;
    if (h.hero_sequences > 0 && h.hero_sequences + 256 * 4 <= static_cast<std::int32_t>(index.size())) {
        sequences.resize(256);
        std::memcpy(sequences.data(), index.data() + h.hero_sequences, 256 * 4);
    }
    std::vector<GadgetRecord> gadgets;
    if (h.gadget_table > 0 && h.gadget_count > 0 &&
        h.gadget_table + h.gadget_count * static_cast<std::int32_t>(sizeof(GadgetRecord)) <= static_cast<std::int32_t>(index.size())) {
        gadgets.resize(static_cast<std::size_t>(h.gadget_count));
        std::memcpy(gadgets.data(), index.data() + h.gadget_table, gadgets.size() * sizeof(GadgetRecord));
    }

    // A part's size is the distance to the next part start the index names (the particle and
    // effect banks not counted), or to the end of the data.
    std::vector<std::int64_t> starts = {h.tfrag_block};
    if (h.occlusion_block > 0) starts.push_back(h.occlusion_block);
    if (h.sky_block > 0) starts.push_back(h.sky_block);
    starts.push_back(h.collision_block);
    starts.push_back(h.texel_block);
    starts.push_back(static_cast<std::int64_t>(data.size()));
    for (const ClassRecord& c : mobys) if (c.data > 0) starts.push_back(c.data);
    for (const ClassRecord& c : ties) if (c.data > 0) starts.push_back(c.data);
    for (const ShrubClassRecord& c : shrubs) if (c.data > 0) starts.push_back(c.data);
    for (std::int32_t s : sequences) if (s > 0) starts.push_back(s);
    for (const GadgetRecord& g : gadgets) if (g.data > 0) starts.push_back(g.data);
    std::sort(starts.begin(), starts.end());
    starts.erase(std::unique(starts.begin(), starts.end()), starts.end());

    auto extent = [&](std::uint32_t at) -> std::uint32_t {
        if (at == 0) return 0;
        const auto next = std::upper_bound(starts.begin(), starts.end(), at);
        return next != starts.end() && *next > at ? static_cast<std::uint32_t>(*next - at) : 0;
    };
    auto add = [&](const char* kind, std::int32_t id, std::int32_t at, std::int64_t fixed_size = -1) {
        LevelCoreSubAsset a;
        a.kind = kind;
        a.id = id;
        a.offset = static_cast<std::uint32_t>(at);
        a.size = fixed_size >= 0 ? static_cast<std::uint32_t>(fixed_size) : extent(a.offset);
        a.packed = a.offset + 3 <= data.size() && a.size >= 3 && std::memcmp(data.data() + a.offset, "WAD", 3) == 0;
        out.sub_assets.push_back(std::move(a));
    };

    if (h.tfrag_block > 0) add("tfrags", -1, h.tfrag_block);
    if (h.occlusion_block > 0) add("occlusion", -1, h.occlusion_block);
    if (h.sky_block > 0) add("sky", -1, h.sky_block);
    if (h.collision_block > 0) add("collision", -1, h.collision_block);
    for (const ClassRecord& c : mobys) if (c.data > 0) add("moby_class", c.class_id, c.data);
    for (const ClassRecord& c : ties) if (c.data > 0) add("tie_class", c.class_id, c.data);
    for (const ShrubClassRecord& c : shrubs) if (c.data > 0) add("shrub_class", c.class_id, c.data);
    for (std::size_t i = 0; i < sequences.size(); ++i) {
        if (sequences[i] > 0) add("sequence", static_cast<std::int32_t>(i), sequences[i]);
    }
    for (const GadgetRecord& g : gadgets) if (g.data > 0) add("gadget", g.class_id, g.data, static_cast<std::uint32_t>(g.packed_size));
    if (h.texel_block > 0) add("textures", -1, h.texel_block);
    if (h.particle_block > 0) add("part_bank", -1, h.particle_block);
    if (h.effect_block > 0) add("fx_bank", -1, h.effect_block);

    std::sort(out.sub_assets.begin(), out.sub_assets.end(),
              [](const LevelCoreSubAsset& a, const LevelCoreSubAsset& b) { return a.offset < b.offset; });
    return out;
}

} // namespace rac1
