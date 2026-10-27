#include <formats/level_textures.h>
#include <formats/scene_wad.h>

#include <algorithm>
#include <array>
#include <cstring>
#include <iostream>

namespace rac1 {

namespace {

// The GS keeps an 8-bit palette (CLUT) in blocks of 32 entries whose middle two groups of 8 are
// swapped: entry i of the palette as the texels index it is stored at i with bits 3 and 4 swapped.
std::uint8_t clut_slot(std::uint8_t i) {
    const bool b3 = (i & 0x08) != 0, b4 = (i & 0x10) != 0;
    return b3 == b4 ? i : static_cast<std::uint8_t>(i ^ 0x18);
}

// GS alpha 0x00..0x80 (0x80 opaque) to 0..255.
std::uint32_t widen_alpha(std::uint32_t rgba) {
    std::uint32_t a = rgba >> 24;
    a = a < 0x80 ? a * 2 : 255;
    return (rgba & 0x00ffffffu) | a << 24;
}

// The 256 RGBA colours a texture's texels index, from the palette as stored (`stored`, up to 256
// words in CLUT order).
std::array<std::uint32_t, 256> texel_colours(std::span<const std::uint32_t> stored) {
    const std::size_t n = std::min<std::size_t>(stored.size(), 256);
    auto at = [&](std::size_t i) { return stored[clut_slot(static_cast<std::uint8_t>(i))]; };

    // orig: GS register TEXA / TEX0 alpha behaviour - 3.2.
    // Palette alpha is 0x00..0x80 and CLUT entries the level never wrote read back as 0. Many
    // tfrag palettes are only partly written (e.g. 56 of 256 entries): the colours past the last
    // written one repeat the written ones (entry 0 left out) and are opaque, and a palette with
    // fewer than 4 non-zero alphas is taken as opaque throughout.
    int used = 0;
    for (int i = static_cast<int>(n) - 1; i >= 0; --i) {
        if (at(static_cast<std::size_t>(i)) != 0) {
            used = i + 1;
            break;
        }
    }
    int alphas = 0;
    for (int i = 0; i < used; ++i) {
        if ((at(static_cast<std::size_t>(i)) >> 24) != 0) ++alphas;
    }
    const bool all_opaque = alphas < 4;

    std::array<std::uint32_t, 256> out{};
    for (std::size_t i = 0; i < 256; ++i) {
        const bool past_used = used > 0 && i >= static_cast<std::size_t>(used);
        std::size_t from = i;
        if (past_used) from = used > 1 ? 1 + i % static_cast<std::size_t>(used - 1) : 0;
        std::uint32_t c = from < n ? at(from) : 0;
        if (all_opaque || past_used) c = (c & 0x00ffffffu) | 0x80000000u;
        out[i] = widen_alpha(c);
    }
    return out;
}

// Appends the texture (8-bit texels `texels`, palette `stored`) to `out`; false (nothing added)
// when its size is not positive or the texels are short.
bool add_texture(std::vector<DecodedTexture>& out, std::string name, std::string category, int index, int width,
                 int height, std::span<const std::uint8_t> texels, std::span<const std::uint32_t> stored) {
    if (width <= 0 || height <= 0) return false;
    const std::size_t count = static_cast<std::size_t>(width) * height;
    if (texels.size() < count) {
        std::cerr << "Warning: Pixel index buffer too small for texture " << name << " (got " << texels.size()
                  << ", expected " << count << ")" << std::endl;
        return false;
    }
    const std::array<std::uint32_t, 256> colours = texel_colours(stored);
    std::vector<std::uint8_t> rgba(count * 4);
    for (std::size_t i = 0; i < count; ++i) {
        const std::uint32_t c = colours[texels[i]];
        std::memcpy(&rgba[i * 4], &c, 4);  // R, G, B, A in the word's byte order
    }
    out.push_back({std::move(name), std::move(category), index, width, height, std::move(rgba)});
    return true;
}

// The `ref.count` records of type T at `ref.offset` in `index` (none when the table is empty or
// does not fit).
template <class T>
std::vector<T> table_rows(std::span<const std::byte> index, TableRef ref) {
    if (ref.count <= 0 || ref.offset < 0) return {};
    const std::size_t first = static_cast<std::size_t>(ref.offset);
    const std::size_t bytes = static_cast<std::size_t>(ref.count) * sizeof(T);
    if (first + bytes > index.size()) {
        std::cerr << "Warning: table out of bounds: offset=" << ref.offset << ", count=" << ref.count
                  << ", size=" << sizeof(T) << ", stream_size=" << index.size() << std::endl;
        return {};
    }
    std::vector<T> rows(static_cast<std::size_t>(ref.count));
    std::memcpy(rows.data(), index.data() + first, bytes);
    return rows;
}

bool span_fits(FileSpan s, std::size_t total) {
    return s.offset >= 0 && s.size >= 0 && static_cast<std::size_t>(s.offset) + static_cast<std::size_t>(s.size) <= total;  // (no int32 sum: it can overflow)
}

} // namespace

std::vector<DecodedTexture> decode_level_textures(std::span<const std::byte> level_wad_data) {
    bool out_of_range = false;
    const std::span<const std::byte> file = level_file_body(level_wad_data, &out_of_range);
    if (out_of_range) std::cerr << "Warning: Level WAD data chunk out of file bounds." << std::endl;
    if (file.size() < sizeof(LevelFileHeader)) {
        std::cerr << "Error: level WAD data too small to contain header." << std::endl;
        return {};
    }
    LevelFileHeader fh;
    std::memcpy(&fh, file.data(), sizeof fh);

    std::cout << "Level WAD size: " << file.size() << "\n"
              << "  overlay:     offset=" << fh.overlay.offset << ", size=" << fh.overlay.size << "\n"
              << "  sound_bank:  offset=" << fh.sound_bank.offset << ", size=" << fh.sound_bank.size << "\n"
              << "  asset_index: offset=" << fh.asset_index.offset << ", size=" << fh.asset_index.size << "\n"
              << "  vram_image:  offset=" << fh.vram_image.offset << ", size=" << fh.vram_image.size << "\n"
              << "  asset_data:  offset=" << fh.asset_data.offset << ", size=" << fh.asset_data.size << "\n";

    for (const auto& [part, label] : {std::pair{fh.asset_index, "asset_index"}, std::pair{fh.vram_image, "vram_image"},
                                      std::pair{fh.asset_data, "asset_data"}}) {
        if (!span_fits(part, file.size())) {
            std::cerr << "Error: Level WAD range '" << label << "' is out of bounds (offset=" << part.offset
                      << ", size=" << part.size << ", total_size=" << file.size() << ")." << std::endl;
            return {};
        }
    }
    const std::span<const std::byte> index = file.subspan(fh.asset_index.offset, fh.asset_index.size);
    const std::span<const std::byte> vram = file.subspan(fh.vram_image.offset, fh.vram_image.size);

    std::vector<std::uint8_t> data;
    if (!decompress_wad(file.subspan(fh.asset_data.offset, fh.asset_data.size), data)) {
        std::cerr << "Error: failed to decompress the asset data." << std::endl;
        return {};
    }
    if (index.size() < sizeof(AssetIndexHeader)) {
        std::cerr << "Error: asset index too small for its header." << std::endl;
        return {};
    }
    AssetIndexHeader ih;
    std::memcpy(&ih, index.data(), sizeof ih);

    // A palette of the VRAM image (`unit` in 256-byte units); opaque black when out of range.
    auto vram_palette = [&](std::int16_t unit) {
        const std::size_t at = static_cast<std::size_t>(unit) * 256;
        std::vector<std::uint32_t> p(256, 0xff000000u);
        if (unit >= 0 && at + 1024 <= vram.size()) std::memcpy(p.data(), vram.data() + at, 1024);
        return p;
    };
    // A palette of the asset data (1 KB at `at`).
    auto data_palette = [&](std::size_t at) {
        std::vector<std::uint32_t> p(256);
        std::memcpy(p.data(), data.data() + at, 1024);
        return p;
    };
    auto data_texels = [&](std::size_t at, std::size_t n) { return std::span<const std::uint8_t>(data.data() + at, n); };
    auto size_name = [](const std::string& head, std::size_t i, int w, int h) {
        return head + "_" + std::to_string(i) + "_" + std::to_string(w) + "x" + std::to_string(h);
    };

    std::vector<DecodedTexture> out;

    // The four TextureRecord tables: texels in the asset data, palettes in the VRAM image.
    const std::pair<TableRef, const char*> world_tables[] = {
        {ih.tfrag_textures, "tfrag"}, {ih.moby_textures, "moby"}, {ih.tie_textures, "tie"}, {ih.shrub_textures, "shrub"}};
    for (const auto& [ref, category] : world_tables) {
        const std::vector<TextureRecord> rows = table_rows<TextureRecord>(index, ref);
        for (std::size_t i = 0; i < rows.size(); ++i) {
            const TextureRecord& t = rows[i];
            const std::size_t at = static_cast<std::size_t>(ih.texel_block) + t.texels;
            const std::size_t n = static_cast<std::size_t>(t.width) * t.height;
            if (at + n > data.size()) {
                std::cerr << "Warning: " << category << " texture " << i << " pixels out of bounds." << std::endl;
                continue;
            }
            add_texture(out, size_name(category, i, t.width, t.height), category, static_cast<int>(i), t.width, t.height,
                        data_texels(at, n), vram_palette(t.palette));
        }
    }

    // The shrub classes' billboards: texels and palette in the VRAM image.
    const std::vector<ShrubClassRecord> shrubs = table_rows<ShrubClassRecord>(index, ih.shrub_classes);
    for (std::size_t i = 0; i < shrubs.size(); ++i) {
        const BillboardRecord& b = shrubs[i].billboard;
        if (b.width <= 0 || b.height <= 0) continue;
        const std::size_t at = static_cast<std::size_t>(b.texels) * 256;
        const std::size_t n = static_cast<std::size_t>(b.width) * b.height;
        if (at + n > vram.size()) {
            std::cerr << "Warning: Shrub class " << shrubs[i].class_id << " billboard pixels out of bounds." << std::endl;
            continue;
        }
        const std::string name = "shrub_class_" + std::to_string(shrubs[i].class_id) + "_billboard_" + std::to_string(b.width) +
                                 "x" + std::to_string(b.height);
        add_texture(out, name, "billboard", static_cast<int>(i), b.width, b.height,
                    std::span<const std::uint8_t>(reinterpret_cast<const std::uint8_t*>(vram.data() + at), n),
                    vram_palette(b.palette));
    }

    // Particle and effect textures: palette and texels in their own bank of the asset data.
    auto bank_texture = [&](const char* head, const char* category, std::size_t i, std::int32_t bank, std::int32_t palette,
                            std::int32_t texels, std::int32_t w, std::int32_t h) {
        const std::size_t at = static_cast<std::size_t>(bank) + texels;
        const std::size_t n = static_cast<std::size_t>(w) * h;
        if (at + n > data.size()) {
            std::cerr << "Warning: " << category << " texture " << i << " pixels out of bounds." << std::endl;
            return;
        }
        const std::size_t pal = static_cast<std::size_t>(bank) + palette;
        if (pal + 1024 > data.size()) {
            std::cerr << "Warning: " << category << " texture " << i << " palette out of bounds." << std::endl;
            return;
        }
        add_texture(out, size_name(head, i, w, h), category, static_cast<int>(i), w, h, data_texels(at, n), data_palette(pal));
    };
    const std::vector<ParticleTextureRecord> particles = table_rows<ParticleTextureRecord>(index, ih.particle_textures);
    for (std::size_t i = 0; i < particles.size(); ++i) {
        const ParticleTextureRecord& p = particles[i];
        bank_texture("particle", "particle", i, ih.particle_block, p.palette, p.texels, p.side, p.side);
    }
    const std::vector<EffectTextureRecord> effects = table_rows<EffectTextureRecord>(index, ih.effect_textures);
    for (std::size_t i = 0; i < effects.size(); ++i) {
        const EffectTextureRecord& e = effects[i];
        bank_texture("fx", "fx", i, ih.effect_block, e.palette, e.texels, e.width, e.height);
    }
    return out;
}

} // namespace rac1
