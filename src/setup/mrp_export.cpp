#include "mrp_export.h"
#include "kick_trace.h"
#include "world_trace.h"

#include <formats/iso_toc.h>
#include <formats/level_core.h>
#include <formats/level_textures.h>
#include <formats/moby_mrp.h>
#include <formats/scene_wad.h>
#include <formats/world_mrp.h>
#include <game/render/mrp_world_assets.h>

#include <algorithm>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iterator>
#include <iostream>
#include <span>
#include <string>
#include <vector>

namespace rac1::setup {

namespace {

constexpr std::size_t SECTOR = 0x800;

template <typename T>
std::vector<T> table(const std::vector<uint8_t>& idx, rac1::TableRef r) {
    std::vector<T> v;
    if (r.count <= 0 || r.offset < 0) return v;
    const size_t end = static_cast<size_t>(r.offset) + static_cast<size_t>(r.count) * sizeof(T);
    if (end > idx.size()) return v;
    v.resize(static_cast<size_t>(r.count));
    std::memcpy(v.data(), idx.data() + r.offset, static_cast<size_t>(r.count) * sizeof(T));
    return v;
}

int export_moby_title(std::span<const std::byte> iso, const std::string& out_dir, bool force) {
    const std::filesystem::path dir = std::filesystem::path(out_dir) / "title";
    if (!force && std::filesystem::exists(dir / "textures.mrpt")) {
        return 0;
    }

    uint32_t lba = 0, sectors = 0;
    if (1500 * SECTOR + 0x14f0 > iso.size()) return 1;
    std::memcpy(&lba, iso.data() + 1500 * SECTOR + 0x14e8, 4);
    std::memcpy(&sectors, iso.data() + 1500 * SECTOR + 0x14ec, 4);
    std::vector<uint8_t> H;
    if (sectors == 0 || static_cast<size_t>(lba + sectors) * SECTOR > iso.size() ||
        !rac1::decompress_wad(std::span<const std::byte>(iso.data() + static_cast<size_t>(lba) * SECTOR, static_cast<size_t>(sectors) * SECTOR), H) || H.size() < 0x90) {
        std::cerr << "title: cannot read its level\n";
        return 1;
    }
    auto rd = [&H](size_t at) {
        uint32_t v = 0;
        if (at + 4 <= H.size()) std::memcpy(&v, H.data() + at, 4);
        return v;
    };
    const size_t s5 = rd(4), s4 = s5 + rd(0x60), gs = rd(0);
    const uint32_t ntex = rd(0x38), texl = rd(0x3c);
    std::span<const uint8_t> tex_table;
    if (static_cast<size_t>(texl) + static_cast<size_t>(ntex) * 16 <= H.size())
        tex_table = std::span<const uint8_t>(H.data() + texl, static_cast<size_t>(ntex) * 16);
    std::filesystem::create_directories(dir);
    int failed = 0, classes = 0;
    for (uint32_t i = 0; i < rd(0x18); ++i) {
        const size_t r = rd(0x1c) + static_cast<size_t>(i) * 0x20;
        const uint32_t off = rd(r);
        const int32_t o_class = static_cast<int32_t>(rd(r + 4));
        if (off == 0 || s5 + off >= H.size()) continue;
        rac1::mrp::MobyClass c;
        c.level = -1;
        c.o_class = o_class;
        if (r + 0x20 <= H.size()) std::memcpy(c.texture_remap.data(), H.data() + r + 0x10, 16);
        std::string err;
        if (!rac1::mrp::decode_class(std::span<const uint8_t>(H).subspan(s5 + off), 0, c, err, tex_table)) {
            std::cerr << "title class " << o_class << ": " << err << "\n";
            ++failed;
            continue;
        }
        for (auto& pk : c.packets) {
            std::string note;
            if (pk.lod != 2 && !rac1::mrp::trace_kicks(pk, note)) {
                // non-fatal trace note
            }
        }
        if (!rac1::mrp::write_class((dir / (std::to_string(o_class) + ".mrpm")).string(), c)) ++failed;
        ++classes;
    }
    std::vector<rac1::mrp::Texture> out;
    for (uint32_t i = 0; i < ntex && !tex_table.empty(); ++i) {
        rac1::TextureRecord te{};
        std::memcpy(&te, tex_table.data() + static_cast<size_t>(i) * 16, 16);
        rac1::mrp::Texture t;
        t.index = i;
        t.width = static_cast<uint32_t>(te.width);
        t.height = static_cast<uint32_t>(te.height);
        const size_t pal = gs + static_cast<size_t>(te.palette) * 256;
        if (pal + 1024 <= H.size()) std::memcpy(t.palette.data(), H.data() + pal, 1024);
        size_t px = s4 + static_cast<size_t>(te.texels);
        uint32_t w = t.width, h = t.height;
        for (int m = 0; m < 2 && w > 0 && h > 0; ++m) {
            if (px + static_cast<size_t>(w) * h > H.size()) break;
            t.mips.emplace_back(H.begin() + static_cast<long>(px), H.begin() + static_cast<long>(px + static_cast<size_t>(w) * h));
            px += static_cast<size_t>(w) * h;
            w /= 2;
            h /= 2;
        }
        const int16_t at2[2] = {te.mip1_page, te.mip2_page};
        for (int m = 0; m < 2 && t.mips.size() == static_cast<size_t>(2 + m) && w > 0 && h > 0 && at2[m] > 0; ++m) {
            const size_t o = gs + static_cast<size_t>(at2[m]) * 256;
            if (o + static_cast<size_t>(w) * h > H.size()) break;
            t.mips.emplace_back(H.begin() + static_cast<long>(o), H.begin() + static_cast<long>(o + static_cast<size_t>(w) * h));
            w /= 2;
            h /= 2;
        }
        out.push_back(std::move(t));
    }
    if (!rac1::mrp::write_textures((dir / "textures.mrpt").string(), out)) ++failed;
    return failed == 0 ? 0 : 1;
}

int export_world_title(std::span<const std::byte> iso, const std::string& out_dir, bool force) {
    const std::filesystem::path dir = std::filesystem::path(out_dir) / rac1::mrpw::level_dir_name(rac1::mrpw::kTitleLevel);
    if (!force && std::filesystem::exists(dir / "world.mrpw")) {
        return 0;
    }

    const size_t toc = 1500 * SECTOR;
    if (toc + 0x14f0 > iso.size()) return 1;
    uint32_t lba = 0, sectors = 0;
    std::memcpy(&lba, iso.data() + toc + 0x14e8, 4);
    std::memcpy(&sectors, iso.data() + toc + 0x14ec, 4);
    if (static_cast<size_t>(lba + sectors) * SECTOR > iso.size() || sectors == 0) {
        std::cerr << "title: no level in the table of contents\n";
        return 1;
    }
    std::vector<uint8_t> H;
    if (!rac1::decompress_wad(std::span<const std::byte>(iso.data() + static_cast<size_t>(lba) * SECTOR, static_cast<size_t>(sectors) * SECTOR), H) || H.size() < 0x90) {
        std::cerr << "title: cannot decompress its level\n";
        return 1;
    }
    auto rd = [&H](size_t at) {
        uint32_t v = 0;
        if (at + 4 <= H.size()) std::memcpy(&v, H.data() + at, 4);
        return v;
    };
    rac1::mrpw::World w;
    std::string err;
    if (!rac1::mrpw::export_title(H, w, err, false)) {
        std::cerr << "title: " << err << "\n";
        return 1;
    }
    std::filesystem::create_directories(dir);
    if (!rac1::mrpw::write_world((dir / "world.mrpw").string(), w)) {
        std::cerr << "title: cannot write\n";
        return 1;
    }
    const size_t s5 = rd(4), s4 = s5 + rd(0x60), gs = rd(0);
    const struct {
        const char* name;
        uint32_t count_at, list_at;
    } tabs[3] = {{"tfrag", 0x30, 0x34}, {"tie", 0x40, 0x44}, {"shrub", 0x48, 0x4c}};
    int failed = 0;
    for (const auto& tb : tabs) {
        std::vector<rac1::mrp::Texture> out;
        const uint32_t n = tb.count_at != 0 ? rd(tb.count_at) : 0, list = tb.list_at != 0 ? rd(tb.list_at) : 0;
        for (uint32_t i = 0; i < n; ++i) {
            rac1::TextureRecord te{};
            if (list + (i + 1) * sizeof te > H.size()) break;
            std::memcpy(&te, H.data() + list + i * sizeof te, sizeof te);
            rac1::mrp::Texture t;
            t.index = i;
            t.width = static_cast<uint32_t>(te.width);
            t.height = static_cast<uint32_t>(te.height);
            const size_t pal = gs + static_cast<size_t>(te.palette) * 256;
            if (pal + 1024 <= H.size()) std::memcpy(t.palette.data(), H.data() + pal, 1024);
            size_t px = s4 + static_cast<size_t>(te.texels);
            uint32_t tw = t.width, th = t.height;
            for (int m = 0; m < 2 && tw > 0 && th > 0; ++m) {
                if (px + static_cast<size_t>(tw) * th > H.size()) break;
                t.mips.emplace_back(H.begin() + static_cast<long>(px), H.begin() + static_cast<long>(px + static_cast<size_t>(tw) * th));
                px += static_cast<size_t>(tw) * th;
                tw /= 2;
                th /= 2;
            }
            const int16_t at2[2] = {te.mip1_page, te.mip2_page};
            for (int m = 0; m < 2 && t.mips.size() == static_cast<size_t>(2 + m) && tw > 0 && th > 0 && at2[m] > 0; ++m) {
                const size_t o = gs + static_cast<size_t>(at2[m]) * 256;
                if (o + static_cast<size_t>(tw) * th > H.size()) break;
                t.mips.emplace_back(H.begin() + static_cast<long>(o), H.begin() + static_cast<long>(o + static_cast<size_t>(tw) * th));
                tw /= 2;
                th /= 2;
            }
            out.push_back(std::move(t));
        }
        if (!rac1::mrp::write_textures((dir / (std::string("textures_") + tb.name + ".mrpt")).string(), out)) ++failed;
    }
    return failed == 0 ? 0 : 1;
}

} // namespace

bool export_mrp_mobys(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg, bool force) {
    if (level_arg == "title") {
        return export_moby_title(iso, out_dir, force) == 0;
    }

    const auto levels = rac1::read_rac1_iso_toc(iso);
    int failed = 0;
    for (const auto& lv : levels) {
        if (level_arg != "all" && lv.id != std::stoi(level_arg)) continue;
        const std::filesystem::path dir = std::filesystem::path(out_dir) / ("level" + std::to_string(lv.id));
        if (!force && std::filesystem::exists(dir / "textures.mrpt")) {
            continue;
        }
        std::cout << " " << lv.id << std::flush;  // (progress: the level being exported)

        const size_t at = static_cast<size_t>(lv.data_lba) * SECTOR, size = static_cast<size_t>(lv.data_sectors) * SECTOR;
        if (at + size > iso.size()) continue;
        const std::span<const std::byte> wad(iso.data() + at, size);
        std::vector<uint8_t> core_index, core_data;
        if (!rac1::extract_core_from_level_wad(wad, core_index, core_data)) {
            std::cerr << "level " << lv.id << ": no core\n";
            ++failed;
            continue;
        }
        const auto decoded = rac1::decode_level_core_index(
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(core_index.data()), core_index.size()),
            std::span<const std::byte>(reinterpret_cast<const std::byte*>(core_data.data()), core_data.size()));
        std::filesystem::create_directories(dir);
        const auto entries = table<rac1::ClassRecord>(core_index, decoded.header.moby_classes);
        for (const auto& asset : decoded.sub_assets) {
            if (asset.kind != "moby_class" && asset.kind != "gadget") continue;
            const bool gadget = asset.kind == "gadget";
            const rac1::ClassRecord* entry = nullptr;
            for (const auto& e : entries)
                if (gadget ? e.class_id == asset.id : static_cast<uint32_t>(e.data) == asset.offset) {
                    entry = &e;
                    break;
                }
            std::vector<uint8_t> bytes(core_data.begin() + asset.offset, core_data.begin() + asset.offset + asset.size);
            if (asset.packed) {
                std::vector<uint8_t> un;
                if (!rac1::decompress_wad(std::span<const std::byte>(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size()), un)) {
                    std::cerr << "level " << lv.id << " class " << asset.id << ": cannot decompress\n";
                    ++failed;
                    continue;
                }
                bytes = std::move(un);
            }
            rac1::mrp::MobyClass c;
            c.level = lv.id;
            c.o_class = asset.id;
            c.gadget = gadget;
            if (entry != nullptr) c.texture_remap = entry->texture_remap;
            std::span<const uint8_t> tex_table;
            if (!gadget && decoded.header.moby_textures.count > 0)
                tex_table = std::span<const uint8_t>(core_index.data() + decoded.header.moby_textures.offset, static_cast<size_t>(decoded.header.moby_textures.count) * 16);
            std::string err;
            if (!rac1::mrp::decode_class(bytes, 0, c, err, tex_table)) {
                std::cerr << "level " << lv.id << " class " << asset.id << ": " << err << "\n";
                ++failed;
                continue;
            }
            for (auto& pk : c.packets) {
                std::string note;
                if (pk.lod != 2 && !rac1::mrp::trace_kicks(pk, note)) {
                    // trace note
                }
            }
            if (!rac1::mrp::write_class((dir / (std::to_string(asset.id) + ".mrpm")).string(), c)) ++failed;
        }

        // moby textures
        rac1::LevelFileHeader wh{};
        std::memcpy(&wh, wad.data(), sizeof wh);
        const auto gs = std::span<const std::byte>(wad.data() + wh.vram_image.offset, static_cast<size_t>(wh.vram_image.size));
        const auto texs = table<rac1::TextureRecord>(core_index, decoded.header.moby_textures);
        std::vector<rac1::mrp::Texture> out;
        for (size_t i = 0; i < texs.size(); ++i) {
            const auto& te = texs[i];
            rac1::mrp::Texture t;
            t.index = static_cast<uint32_t>(i);
            t.width = static_cast<uint32_t>(te.width);
            t.height = static_cast<uint32_t>(te.height);
            const size_t pal = static_cast<size_t>(te.palette) * 256;
            if (pal + 1024 <= gs.size()) std::memcpy(t.palette.data(), gs.data() + pal, 1024);
            size_t px = static_cast<size_t>(decoded.header.texel_block) + static_cast<size_t>(te.texels);
            uint32_t w = t.width, h = t.height;
            for (int m = 0; m < 2 && w > 0 && h > 0; ++m) {
                if (px + static_cast<size_t>(w) * h > core_data.size()) break;
                t.mips.emplace_back(core_data.begin() + static_cast<long>(px), core_data.begin() + static_cast<long>(px + static_cast<size_t>(w) * h));
                px += static_cast<size_t>(w) * h;
                w /= 2;
                h /= 2;
            }
            const int16_t at2[2] = {te.mip1_page, te.mip2_page};
            for (int m = 0; m < 2 && t.mips.size() == static_cast<size_t>(2 + m) && w > 0 && h > 0 && at2[m] > 0; ++m) {
                const size_t o = static_cast<size_t>(at2[m]) * 256;
                if (o + static_cast<size_t>(w) * h > gs.size()) break;
                const auto* g = reinterpret_cast<const uint8_t*>(gs.data()) + o;
                t.mips.emplace_back(g, g + static_cast<size_t>(w) * h);
                w /= 2;
                h /= 2;
            }
            out.push_back(std::move(t));
        }
        if (!rac1::mrp::write_textures((dir / "textures.mrpt").string(), out)) ++failed;
    }

    // Hero armor classes
    if (level_arg == "all") {
        const std::filesystem::path dir = std::filesystem::path(out_dir) / "armor";
        if (force || !std::filesystem::exists(dir / "3.mrpt")) {
            std::filesystem::create_directories(dir);
            for (uint32_t k = 0; k < 4; ++k) {
                uint32_t lba = 0, n = 0;
                std::memcpy(&lba, iso.data() + 1500 * SECTOR + 0x12c8 + k * 8, 4);
                std::memcpy(&n, iso.data() + 1500 * SECTOR + 0x12cc + k * 8, 4);
                if (static_cast<size_t>(lba + n) * SECTOR > iso.size()) continue;
                const auto* f = reinterpret_cast<const uint8_t*>(iso.data()) + static_cast<size_t>(lba) * SECTOR;
                const std::vector<uint8_t> file(f, f + static_cast<size_t>(n) * SECTOR);
                uint32_t w[4];
                std::memcpy(w, file.data(), 16);
                std::vector<rac1::mrp::Texture> texs;
                for (uint32_t s = 0; s < 2; ++s) {
                    const uint32_t at = w[s * 2], tex = w[s * 2 + 1];
                    if (at >= file.size() || tex >= file.size()) continue;
                    rac1::mrp::MobyClass c;
                    c.level = -1;
                    c.o_class = -1;
                    std::string err;
                    if (!rac1::mrp::decode_class(std::span<const uint8_t>(file).subspan(at), 0, c, err)) {
                        std::cerr << "armor " << k << " class " << s << ": " << err << "\n";
                        ++failed;
                        continue;
                    }
                    for (auto& pk : c.packets) {
                        std::string note;
                        if (pk.lod != 2 && !rac1::mrp::trace_kicks(pk, note)) {
                            // trace note
                        }
                    }
                    if (!rac1::mrp::write_class((dir / (std::to_string(k) + "_" + std::to_string(s) + ".mrpm")).string(), c)) ++failed;
                    rac1::mrp::Texture t;
                    t.index = s;
                    const size_t len = s == 0 ? 0x14000 : 0x4000;
                    if (tex + 0x430 + len <= file.size()) t.mips.emplace_back(file.begin() + tex, file.begin() + tex + 0x430 + static_cast<long>(len));
                    texs.push_back(std::move(t));
                }
                if (!rac1::mrp::write_textures((dir / (std::to_string(k) + ".mrpt")).string(), texs)) ++failed;
            }
        }
        std::cout << " title" << std::flush;
        if (export_moby_title(iso, out_dir, force) != 0) failed++;
    }

    return failed == 0;
}

namespace {

// One level's world (world.mrpw and its textures_{tfrag,tie,shrub}.mrpt) from its data file `wad`, into `dir`.
// orig: (host)
bool export_world_level(std::span<const std::byte> wad, int level, const std::filesystem::path& dir) {
    rac1::mrpw::LevelSource src;
    src.level = level;
    if (!rac1::extract_core_from_level_wad(wad, src.core_index, src.core_data)) {
        std::cerr << "level " << level << ": no core\n";
        return false;
    }
    src.decoded = rac1::decode_level_core_index(
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(src.core_index.data()), src.core_index.size()),
        std::span<const std::byte>(reinterpret_cast<const std::byte*>(src.core_data.data()), src.core_data.size()));
    rac1::mrpw::World w;
    std::string err;
    if (!rac1::mrpw::export_level(src, w, err, false)) {
        std::cerr << "level " << level << ": " << err << "\n";
        return false;
    }
    std::filesystem::create_directories(dir);
    if (!rac1::mrpw::write_world((dir / "world.mrpw").string(), w)) {
        std::cerr << "level " << level << ": cannot write\n";
        return false;
    }

    // world textures
    int failed = 0;
    rac1::LevelFileHeader wh{};
    std::memcpy(&wh, wad.data(), sizeof wh);
    const auto gs = std::span<const std::byte>(wad.data() + wh.vram_image.offset, static_cast<size_t>(wh.vram_image.size));
    const rac1::TableRef ranges[3] = {src.decoded.header.tfrag_textures, src.decoded.header.tie_textures, src.decoded.header.shrub_textures};
    const char* names[3] = {"tfrag", "tie", "shrub"};
    for (int k = 0; k < 3; ++k) {
        std::vector<rac1::mrp::Texture> out;
        const rac1::TableRef r = ranges[k];
        for (int32_t i = 0; i < r.count; ++i) {
            rac1::TextureRecord te;
            std::memcpy(&te, src.core_index.data() + r.offset + static_cast<size_t>(i) * sizeof te, sizeof te);
            rac1::mrp::Texture t;
            t.index = static_cast<uint32_t>(i);
            t.width = static_cast<uint32_t>(te.width);
            t.height = static_cast<uint32_t>(te.height);
            const size_t pal = static_cast<size_t>(te.palette) * 256;
            if (pal + 1024 <= gs.size()) std::memcpy(t.palette.data(), gs.data() + pal, 1024);
            size_t px = static_cast<size_t>(src.decoded.header.texel_block) + static_cast<size_t>(te.texels);
            uint32_t tw = t.width, th = t.height;
            for (int m = 0; m < 2 && tw > 0 && th > 0; ++m) {
                if (px + static_cast<size_t>(tw) * th > src.core_data.size()) break;
                t.mips.emplace_back(src.core_data.begin() + static_cast<long>(px), src.core_data.begin() + static_cast<long>(px + static_cast<size_t>(tw) * th));
                px += static_cast<size_t>(tw) * th;
                tw /= 2;
                th /= 2;
            }
            const int16_t at2[2] = {te.mip1_page, te.mip2_page};
            for (int m = 0; m < 2 && t.mips.size() == static_cast<size_t>(2 + m) && tw > 0 && th > 0 && at2[m] > 0; ++m) {
                const size_t o = static_cast<size_t>(at2[m]) * 256;
                if (o + static_cast<size_t>(tw) * th > gs.size()) break;
                const auto* g = reinterpret_cast<const uint8_t*>(gs.data()) + o;
                t.mips.emplace_back(g, g + static_cast<size_t>(tw) * th);
                tw /= 2;
                th /= 2;
            }
            out.push_back(std::move(t));
        }
        if (!rac1::mrp::write_textures((dir / (std::string("textures_") + names[k] + ".mrpt")).string(), out)) ++failed;
    }
    return failed == 0;
}

} // namespace

bool export_mrp_world(std::span<const std::byte> iso, const std::string& out_dir, const std::string& level_arg, bool force) {
    if (level_arg == "title") {
        return export_world_title(iso, out_dir, force) == 0;
    }

    const auto levels = rac1::read_rac1_iso_toc(iso);
    int failed = 0;
    for (const auto& lv : levels) {
        if (level_arg != "all" && lv.id != std::stoi(level_arg)) continue;
        const std::filesystem::path dir = std::filesystem::path(out_dir) / ("level" + std::to_string(lv.id));
        if (!force && std::filesystem::exists(dir / "world.mrpw")) {
            continue;
        }
        std::cout << " " << lv.id << std::flush;  // (progress: the level being exported)

        const size_t at = static_cast<size_t>(lv.data_lba) * SECTOR, size = static_cast<size_t>(lv.data_sectors) * SECTOR;
        if (at + size > iso.size()) continue;
        if (!export_world_level(std::span<const std::byte>(iso.data() + at, size), lv.id, dir)) ++failed;
    }

    if (level_arg == "all") {
        std::cout << " title" << std::flush;
        if (export_world_title(iso, out_dir, force) != 0) failed++;
    }

    return failed == 0;
}

bool export_mrp_world_custom(const std::string& custom_dir) {
    const std::filesystem::path wad_path = std::filesystem::path(custom_dir) / "data.wad";
    std::ifstream f(wad_path, std::ios::binary);
    if (!f) {
        std::cerr << "[setup] " << wad_path.string() << " cannot be read\n";
        return false;
    }
    const std::vector<char> bytes((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    const std::span<const std::byte> wad(reinterpret_cast<const std::byte*>(bytes.data()), bytes.size());
    return export_world_level(wad, 0, std::filesystem::path(custom_dir) / "mrp" / "world" / "level0");
}

} // namespace rac1::setup
