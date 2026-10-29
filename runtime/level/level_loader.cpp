#include <runtime/level/level_loader.h>
#include <runtime/vk/vk_context.h>
#include <runtime/level/host_layout.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/load_prof.h>
#include <formats/level_core.h>
#include <formats/level_textures.h>
#include <formats/ratchet_executable.h>
#include <formats/sky_model.h>
#include <runtime/gs/static_geom_cache.h>

// Hoisted from _invented/level/collision_query.cpp per ADR 0003 — runtime
// must not depend on src/_invented/.
namespace rac1 {
std::uint32_t g_core_data_vaddr = host_layout::CORE;
std::vector<DecodedTexture> g_level_textures;
DecodedSky g_decoded_sky;
std::uint32_t g_particle_texture_pixel_vaddrs[256] = {0};
}

#include <cstdio>
#include <iostream>
#include <string>
#include <vector>
#include <stdexcept>
#include <span>
#include <cstring>
#include <bit>
#include <optional>
#include <future>
#include <memory>
#include <mutex>


namespace rac1 {

SkyInitCallback g_sky_init_callback = nullptr;
TfragInitCallback g_tfrag_init_callback = nullptr;
TieInitCallback g_tie_init_callback = nullptr;
ShrubInitCallback g_shrub_init_callback = nullptr;
MobyInitCallback g_moby_init_callback = nullptr;

int g_pending_level_id = -1;

namespace {
// orig: (host) an address as 0x%08x, for the error messages
std::string hex32(std::uint32_t v) {
    char b[16];
    std::snprintf(b, sizeof b, "0x%08x", v);
    return b;
}

int g_legacy_pending_level = -1;  // the level whose legacy copy is not built yet

// a level's sectors and its core, read ahead by LevelLoader::prefetch
struct Prefetched {
    std::vector<std::byte> file;
    std::vector<std::uint8_t> core_index, core_data;
    bool ok = false;
};
std::mutex g_pf_lock;
int g_pf_level = -1;
std::shared_future<std::shared_ptr<Prefetched>> g_pf;

std::shared_ptr<Prefetched> take_prefetched_level(int level) {
    std::shared_future<std::shared_ptr<Prefetched>> f;
    {
        std::scoped_lock lock(g_pf_lock);
        if (!g_pf.valid() || g_pf_level != level) return nullptr;
        f = g_pf;
        g_pf = {};
        g_pf_level = -1;
    }
    LoadProf prof("  the prefetch waited for");
    return f.get();
}
}  // namespace

// orig: (host) see level_loader.h
void LevelLoader::prefetch(int level) {
#if defined(__WIIU__)
    // On Wii U there is no legacy OpenGL host renderer; engine_image_load loads the level overlay directly
    // via disc_read_level_overlay, so reading the 25-35 MB WAD and decompressing its core into a 40-55 MB
    // MEM2 buffer during the flight scene is pure waste and causes SD I/O contention and MEM2 pressure.
    (void)level;
    return;
#else
    std::scoped_lock lock(g_pf_lock);
    if (g_pf.valid() && g_pf_level == level) return;
    g_pf_level = level;
    g_pf = std::async(std::launch::async, [level] {
               auto p = std::make_shared<Prefetched>();
               LoadProf prof("(worker) the level's sectors read from the disc and its core decompressed ahead");
               if (disc_read_level_wad(level, p->file))
                   p->ok = extract_core_from_level_wad(std::span<const std::byte>(p->file.data(), p->file.size()), p->core_index, p->core_data);
               return p;
           }).share();
#endif
}

void build_legacy(std::span<const std::byte> level_wad_span);

namespace {
void finish_level_wad_load(std::span<const std::byte> level_wad_span,
                           const std::vector<std::uint8_t>& core_index,
                           const std::vector<std::uint8_t>& core_data_decompressed,
                           std::optional<LoadProf>& prof,
                           int level) {
    // 3. Check memory boundary constraints
    const u32 core_base_addr = host_layout::CORE;
    const std::size_t total_payload_size = core_index.size() + core_data_decompressed.size();
    if (!ee_is_valid_addr(core_base_addr, static_cast<u32>(total_payload_size))) {
        throw std::runtime_error("LevelLoader: level core payload exceeds virtual EE RAM bounds");
    }

    // 4. Copy decompressed payload into EE virtual RAM at host_layout::CORE
    void* dest = ee_ram_ptr(core_base_addr);
    if (!dest) {
        throw std::runtime_error("LevelLoader: failed to map virtual RAM at address host_layout::CORE");
    }

    ee_copy_in(core_base_addr, core_index.data(), core_index.size());
    ee_copy_in(core_base_addr + static_cast<u32>(core_index.size()), core_data_decompressed.data(), core_data_decompressed.size());

    prof.emplace("  the core copied, the overlay");
    // 5. Load level overlay
    LevelLoader::load_level_overlay(level_wad_span);

    // 6. Write hardcoded layout pointers to virtual RAM
    if (g_legacy_host_writes) {  // the legacy host only: the game sets these itself
        ee_write<u32>(0x001940cc, host_layout::LOADER_ANCHOR); // Base Level Loader Anchor
        ee_write<u32>(0x001940d0, host_layout::CORE_TEMP); // Core asset temporary buffer
        ee_write<u32>(0x001940d4, host_layout::CORE); // Level core WAD base
    }

    prof.emplace("  register_level_textures");
    // 7. Register level textures
    LevelLoader::register_level_textures(level_wad_span);

    prof.emplace("  sky_init");
    // 8. Initialize sky asset
    if (g_sky_init_callback) {
        AssetIndexHeader core_header = ee_read<AssetIndexHeader>(host_layout::CORE);
        g_sky_init_callback(core_header.sky_block, core_header.tfrag_textures.offset);
    }
    prof.reset();

    // 9. The legacy host renderers' copy: now if one of them is in use, else when one is chosen
    g_legacy_pending_level = -1;
    if (g_legacy_level_needed == nullptr || g_legacy_level_needed()) {
        build_legacy(level_wad_span);
    } else {
        g_legacy_pending_level = level;
        if (load_prof_on()) std::printf("[load]   the legacy host renderers' copy of the level: not in use, left\n");
    }
}
}  // namespace

void LevelLoader::load_level_wad(int level) {
#if defined(__WIIU__)
    // On Wii U, host_layout::CORE, host_layout::TEX_HEAP, and g_decoded_sky are unused (legacy OpenGL host
    // renderers are not built on Wii U, and engine_image_load already calls LevelLoader::load_level_overlay).
    (void)level;
    return;
#else
    LoadProf prof_all("load_level_wad");
    std::vector<std::byte> file_data;
    std::vector<std::uint8_t> core_index;
    std::vector<std::uint8_t> core_data_decompressed;
    std::optional<LoadProf> prof;
    if (std::shared_ptr<Prefetched> pf = take_prefetched_level(level); pf != nullptr && pf->ok) {
        file_data = std::move(pf->file);
        core_index = std::move(pf->core_index);
        core_data_decompressed = std::move(pf->core_data);
    } else {
        prof.emplace("  the WAD read");
        if (!disc_read_level_wad(level, file_data)) {
            throw std::runtime_error("LevelLoader: failed to read level " + std::to_string(level) + " from disc image");
        }
        prof.emplace("  the core decompressed");
        if (!extract_core_from_level_wad(std::span<const std::byte>(file_data.data(), file_data.size()), core_index, core_data_decompressed)) {
            throw std::runtime_error("LevelLoader: failed to extract/decompress level core WAD for level " + std::to_string(level));
        }
    }
    finish_level_wad_load(std::span<const std::byte>(file_data.data(), file_data.size()),
                          core_index, core_data_decompressed, prof, level);
#endif
}

// orig: (host) the legacy host renderers' copy of a level (see level_loader.h)
void build_legacy(std::span<const std::byte> level_wad_span) {
    LoadProf prof_all("  the legacy host renderers' copy");
    std::optional<LoadProf> prof("    decode_level_textures");
    // Decode textures for GsRenderer
    g_level_textures = decode_level_textures(level_wad_span);
    std::cout << "[LevelLoader] Decoded " << g_level_textures.size() << " level textures for GsRenderer." << std::endl;

    std::cout << "[LevelLoader] g_tfrag_init_callback = " << (void*)g_tfrag_init_callback
              << ", g_tie_init_callback = " << (void*)g_tie_init_callback
              << ", g_shrub_init_callback = " << (void*)g_shrub_init_callback
              << ", g_moby_init_callback = " << (void*)g_moby_init_callback << std::endl;

    prof.emplace("    tfrag_init");
    if (g_tfrag_init_callback) {
        g_tfrag_init_callback(level_wad_span);
    }

    prof.emplace("    tie_init");
    if (g_tie_init_callback) {
        g_tie_init_callback(level_wad_span);
    }
    prof.emplace("    shrub_init");
    if (g_shrub_init_callback) {
        g_shrub_init_callback(level_wad_span);
    }
    prof.emplace("    moby_init");
    if (g_moby_init_callback) {
        g_moby_init_callback(level_wad_span);
    }

    prof.emplace("    static_geom_cache_init");
    if (g_use_gpu_rendering && !vk::is_active()) {  // (the legacy renderers' OpenGL buffers: none with Vulkan, which draws no legacy renderer)
        static_geom_cache_init();
    }
}

// orig: (host) the legacy copy built when a legacy renderer is chosen after the level's load (the debug menu): from the
// level's sectors on the disc and the EE's memory as they are now (the texture tables and the core the legacy decoders read)
void LevelLoader::legacy_level_ensure() {
    if (g_legacy_pending_level < 0 || (g_legacy_level_needed != nullptr && !g_legacy_level_needed())) return;
    const int level = g_legacy_pending_level;
    g_legacy_pending_level = -1;
    std::vector<std::byte> file_data;
    if (!disc_read_level_wad(level, file_data)) return;
    build_legacy(std::span<const std::byte>(file_data.data(), file_data.size()));
}

void LevelLoader::register_level_textures(std::span<const std::byte> file_data) {
    // 2. Locate the inner WAD block (level_data)
    std::span<const std::byte> level_data(file_data.data(), file_data.size());
    if (file_data.size() >= 48) {
        std::int32_t header_size = le32s(file_data.data());
        if (header_size == 48) {
            std::int32_t data_sector_offset = le32s(file_data.data() + 0x10);
            std::int32_t data_sector_size = le32s(file_data.data() + 0x14);
            std::size_t byte_offset = static_cast<std::size_t>(data_sector_offset) * 2048;
            std::size_t byte_size = static_cast<std::size_t>(data_sector_size) * 2048;
            if (byte_offset + byte_size <= file_data.size()) {
                level_data = std::span<const std::byte>(file_data.data() + byte_offset, byte_size);
            }
        }
    }

    if (level_data.size() < sizeof(LevelFileHeader)) {
        throw std::runtime_error("LevelLoader: level WAD data too small to contain header");
    }

    // 3. Parse LevelFileHeader
    LevelFileHeader wad_header;
    std::memcpy(&wad_header, level_data.data(), sizeof(LevelFileHeader));
    le_fix(wad_header);

    // Check bounds for gs_ram segment
    if (wad_header.vram_image.offset < 0 || wad_header.vram_image.size < 0 ||
        static_cast<std::size_t>(wad_header.vram_image.offset + wad_header.vram_image.size) > level_data.size()) {
        throw std::runtime_error("LevelLoader: gs_ram segment out of bounds in level WAD");
    }

    // 4. Read AssetIndexHeader from virtual EE RAM at host_layout::CORE
    AssetIndexHeader core_header = ee_read<AssetIndexHeader>(host_layout::CORE);

    // 5. Initialize the heap pointer starting at host_layout::TEX_HEAP
    std::uint32_t heap_ptr = host_layout::TEX_HEAP;
    std::uint32_t core_data_vaddr = host_layout::CORE + wad_header.asset_index.size;
    g_core_data_vaddr = core_data_vaddr;

    auto register_texture = [&](const std::string& category, int index, int width, int height, int type, int pad,
                                const std::uint8_t* pixel_src, const std::uint8_t* palette_src,
                                std::uint32_t table_base_vaddr) -> std::uint32_t {
        // Align heap pointer to 16 bytes
        heap_ptr = (heap_ptr + 15) & ~15;

        std::uint32_t header_vaddr = heap_ptr;
        std::uint32_t palette_vaddr = header_vaddr + sizeof(TextureHeader);
        std::uint32_t pixel_vaddr = palette_vaddr + 1024;
        std::size_t num_pixels = static_cast<std::size_t>(width) * height;

        // Check if layout exceeds virtual RAM bounds
        if (!ee_is_valid_addr(header_vaddr, sizeof(TextureHeader) + 1024 + num_pixels)) {
            throw std::runtime_error("LevelLoader: texture allocation exceeds virtual RAM bounds during registration of " + category + " index " + std::to_string(index));
        }

        // TextureHeader
        TextureHeader tex_hdr;
        tex_hdr.palette_addr = palette_vaddr;
        tex_hdr.width = static_cast<std::uint16_t>(width);
        tex_hdr.height = static_cast<std::uint16_t>(height);
        tex_hdr.type = static_cast<std::uint16_t>(type);
        tex_hdr.pad = static_cast<std::uint16_t>(pad);

        // Write to EE RAM
        ee_write<TextureHeader>(header_vaddr, tex_hdr);

        // Copy palette (1024 bytes)
        // (the sources: the PS2's bytes, from the level file or ee_bytes_view)
        if (palette_src) {
            ee_copy_in(palette_vaddr, palette_src, 1024);
        } else {
            ee_zero(palette_vaddr, 1024);
        }

        // Copy pixels
        if (pixel_src) {
            ee_copy_in(pixel_vaddr, pixel_src, num_pixels);
        } else {
            ee_zero(pixel_vaddr, num_pixels);
        }

        // Advance heap_ptr
        heap_ptr = pixel_vaddr + num_pixels;

        // Compute packed pointer
        std::uint32_t log2_val = 0;
        if (width > 0) {
            log2_val = 0x1e - std::countl_zero(static_cast<std::uint32_t>(width));
        }
        std::uint32_t packed_ptr = (log2_val << 28) | pixel_vaddr;

        // Register in global table
        if (table_base_vaddr != 0) {
            ee_write<std::uint32_t>(table_base_vaddr + index * 4, packed_ptr);
        }
        if (category == "particle" && index >= 0 && index < 256) {
            g_particle_texture_pixel_vaddrs[index] = pixel_vaddr;
        }

        return packed_ptr;
    };

    // --- Tfrag Textures ---
    if (core_header.tfrag_textures.count > 0 && core_header.tfrag_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.tfrag_textures.offset;
        for (int i = 0; i < core_header.tfrag_textures.count; ++i) {
            TextureRecord entry = ee_read<TextureRecord>(array_vaddr + i * sizeof(TextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.texel_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.width * entry.height)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.width * entry.height), pixel_src_bytes);
            }
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            std::size_t pal_offset = static_cast<std::size_t>(entry.palette) * 256;
            if (pal_offset + 1024 <= static_cast<std::size_t>(wad_header.vram_image.size)) {
                palette_src = reinterpret_cast<const std::uint8_t*>(level_data.data()) + wad_header.vram_image.offset + pal_offset;
            }
            register_texture("tfrag", i, entry.width, entry.height, entry.mip_count, entry.mip2_page, pixel_src, palette_src, host_layout::TEX_TABLE_TFRAG);
        }
    }

    // --- Moby Textures ---
    if (core_header.moby_textures.count > 0 && core_header.moby_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.moby_textures.offset;
        for (int i = 0; i < core_header.moby_textures.count; ++i) {
            TextureRecord entry = ee_read<TextureRecord>(array_vaddr + i * sizeof(TextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.texel_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.width * entry.height)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.width * entry.height), pixel_src_bytes);
            }
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            std::size_t pal_offset = static_cast<std::size_t>(entry.palette) * 256;
            if (pal_offset + 1024 <= static_cast<std::size_t>(wad_header.vram_image.size)) {
                palette_src = reinterpret_cast<const std::uint8_t*>(level_data.data()) + wad_header.vram_image.offset + pal_offset;
            }
            register_texture("moby", i, entry.width, entry.height, entry.mip_count, entry.mip2_page, pixel_src, palette_src, host_layout::TEX_TABLE_MOBY);
        }
    }

    // --- TIE Textures ---
    if (core_header.tie_textures.count > 0 && core_header.tie_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.tie_textures.offset;
        for (int i = 0; i < core_header.tie_textures.count; ++i) {
            TextureRecord entry = ee_read<TextureRecord>(array_vaddr + i * sizeof(TextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.texel_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.width * entry.height)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.width * entry.height), pixel_src_bytes);
            }
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            std::size_t pal_offset = static_cast<std::size_t>(entry.palette) * 256;
            if (pal_offset + 1024 <= static_cast<std::size_t>(wad_header.vram_image.size)) {
                palette_src = reinterpret_cast<const std::uint8_t*>(level_data.data()) + wad_header.vram_image.offset + pal_offset;
            }
            register_texture("tie", i, entry.width, entry.height, entry.mip_count, entry.mip2_page, pixel_src, palette_src, host_layout::TEX_TABLE_TIE);
        }
    }

    // --- Shrub Textures ---
    if (core_header.shrub_textures.count > 0 && core_header.shrub_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.shrub_textures.offset;
        for (int i = 0; i < core_header.shrub_textures.count; ++i) {
            TextureRecord entry = ee_read<TextureRecord>(array_vaddr + i * sizeof(TextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.texel_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.width * entry.height)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.width * entry.height), pixel_src_bytes);
            }
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            std::size_t pal_offset = static_cast<std::size_t>(entry.palette) * 256;
            if (pal_offset + 1024 <= static_cast<std::size_t>(wad_header.vram_image.size)) {
                palette_src = reinterpret_cast<const std::uint8_t*>(level_data.data()) + wad_header.vram_image.offset + pal_offset;
            }
            register_texture("shrub", i, entry.width, entry.height, entry.mip_count, entry.mip2_page, pixel_src, palette_src, host_layout::TEX_TABLE_SHRUB);
        }
    }

    // --- Particle Textures ---
    if (core_header.particle_textures.count > 0 && core_header.particle_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.particle_textures.offset;
        for (int i = 0; i < core_header.particle_textures.count; ++i) {
            ParticleTextureRecord entry = ee_read<ParticleTextureRecord>(array_vaddr + i * sizeof(ParticleTextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.particle_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.side * entry.side)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.side * entry.side), pixel_src_bytes);
            }
            std::uint32_t palette_src_vaddr = core_data_vaddr + core_header.particle_block + entry.palette;
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            if (ee_is_valid_addr(palette_src_vaddr, 1024)) {
                palette_src = ee_bytes_view(palette_src_vaddr, static_cast<std::size_t>(1024), palette_src_bytes);
            }
            register_texture("particle", i, entry.side, entry.side, 0, 0, pixel_src, palette_src, 0);
        }
    }

    // --- FX Textures ---
    if (core_header.effect_textures.count > 0 && core_header.effect_textures.offset > 0) {
        std::uint32_t array_vaddr = host_layout::CORE + core_header.effect_textures.offset;
        for (int i = 0; i < core_header.effect_textures.count; ++i) {
            EffectTextureRecord entry = ee_read<EffectTextureRecord>(array_vaddr + i * sizeof(EffectTextureRecord));
            std::uint32_t pixel_src_vaddr = core_data_vaddr + core_header.effect_block + entry.texels;
            std::vector<std::uint8_t> pixel_src_bytes;
            const std::uint8_t* pixel_src = nullptr;
            if (ee_is_valid_addr(pixel_src_vaddr, entry.width * entry.height)) {
                pixel_src = ee_bytes_view(pixel_src_vaddr, static_cast<std::size_t>(entry.width * entry.height), pixel_src_bytes);
            }
            std::uint32_t palette_src_vaddr = core_data_vaddr + core_header.effect_block + entry.palette;
            std::vector<std::uint8_t> palette_src_bytes;
            const std::uint8_t* palette_src = nullptr;
            if (ee_is_valid_addr(palette_src_vaddr, 1024)) {
                palette_src = ee_bytes_view(palette_src_vaddr, static_cast<std::size_t>(1024), palette_src_bytes);
            }
            register_texture("fx", i, entry.width, entry.height, 0, 0, pixel_src, palette_src, 0);
        }
    }
}

std::uint32_t LevelLoader::load_level_overlay(int level) {
    std::vector<std::byte> file_data;
    if (!disc_read_level_overlay(level, file_data)) {
        throw std::runtime_error("LevelLoader: failed to read level " + std::to_string(level) + " overlay from disc image");
    }
    return load_level_overlay(std::span<const std::byte>(file_data.data(), file_data.size()));
}

std::uint32_t LevelLoader::load_level_overlay(std::span<const std::byte> file_data) {
    // 2. Locate the inner WAD block (level_data)
    std::span<const std::byte> level_data(file_data.data(), file_data.size());
    if (file_data.size() >= 48) {
        std::int32_t header_size = le32s(file_data.data());
        if (header_size == 48) {
            std::int32_t data_sector_offset = le32s(file_data.data() + 0x10);
            std::int32_t data_sector_size = le32s(file_data.data() + 0x14);
            std::size_t byte_offset = static_cast<std::size_t>(data_sector_offset) * 2048;
            std::size_t byte_size = static_cast<std::size_t>(data_sector_size) * 2048;
            if (byte_offset + byte_size <= file_data.size()) {
                level_data = std::span<const std::byte>(file_data.data() + byte_offset, byte_size);
            }
        }
    }

    if (level_data.size() < sizeof(LevelFileHeader)) {
        throw std::runtime_error("LevelLoader: level WAD data too small to contain header");
    }

    // 3. Parse LevelFileHeader
    LevelFileHeader wad_header;
    std::memcpy(&wad_header, level_data.data(), sizeof(LevelFileHeader));
    le_fix(wad_header);

    // Check bounds for overlay segment
    if (wad_header.overlay.offset < 0 || wad_header.overlay.size <= 0 ||
        static_cast<std::size_t>(wad_header.overlay.offset + wad_header.overlay.size) > level_data.size()) {
        throw std::runtime_error("LevelLoader: overlay segment out of bounds in level WAD");
    }

    // 4. Extract the overlay byte range
    std::span<const std::byte> overlay_data(level_data.data() + wad_header.overlay.offset, wad_header.overlay.size);

    // 5. Parse the overlay byte range as a RatchetExecutable
    RatchetExecutable exe = parse_ratchet_executable(overlay_data);

    // 6. Map/copy each parsed section payload into the specified destination virtual EE RAM address
    for (const auto& section : exe.sections) {
        std::uint32_t dest_addr = static_cast<std::uint32_t>(section.header.load_address);
        std::uint32_t copy_size = static_cast<std::uint32_t>(section.header.byte_count);

        if (copy_size == 0) {
            continue;
        }

        // Verify boundary constraints
        if (!ee_is_valid_addr(dest_addr, copy_size)) {
            throw std::runtime_error("LevelLoader: overlay section " + std::to_string(section.index) +
                                     " destination address " + hex32(dest_addr) +
                                     " of size " + std::to_string(copy_size) +
                                     " exceeds virtual EE RAM bounds");
        }

        void* dest_ptr = ee_ram_ptr(dest_addr);
        if (!dest_ptr) {
            throw std::runtime_error("LevelLoader: failed to map virtual RAM at address " + hex32(dest_addr));
        }

        if (section.header.elf_type == 8) { // SHT_NOBITS
            ee_zero(dest_addr, copy_size);
        } else {
            ee_copy_in(dest_addr, section.data.data(), copy_size);
        }
    }

    return static_cast<std::uint32_t>(exe.entry_point);
}

} // namespace rac1

