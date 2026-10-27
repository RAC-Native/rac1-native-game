#ifndef RAC1_RUNTIME_LEVEL_LOADER_H
#define RAC1_RUNTIME_LEVEL_LOADER_H

#include <filesystem>
#include <runtime/level/host_layout.h>
#include <cstdint>

#include <span>

#include "io/host_endian.h"

namespace rac1 {

// The legacy host writes its own state into the game's memory while it loads a level (asset
// anchors 0x1940cc..0x1940d4, its moby pool, trigger shapes); the original runtime clears this:
// the game's own level setup owns that memory.
inline bool g_legacy_host_writes = true;

typedef void (*SkyInitCallback)(std::uint32_t sky_offset, std::uint32_t tfrag_textures_offset);
extern SkyInitCallback g_sky_init_callback;

typedef void (*TfragInitCallback)(std::span<const std::byte> level_wad_data);
extern TfragInitCallback g_tfrag_init_callback;

typedef void (*TieInitCallback)(std::span<const std::byte> level_wad_data);
extern TieInitCallback g_tie_init_callback;

typedef void (*ShrubInitCallback)(std::span<const std::byte> level_wad_data);
extern ShrubInitCallback g_shrub_init_callback;

typedef void (*MobyInitCallback)(std::span<const std::byte> level_wad_data);
extern MobyInitCallback g_moby_init_callback;

extern std::uint32_t g_particle_texture_pixel_vaddrs[256];

// whether a legacy host renderer is in use (LevelLoader::legacy_level_ensure); set by the host
inline bool (*g_legacy_level_needed)() = nullptr;

#pragma pack(push, 1)
struct TextureHeader {
    std::uint32_t palette_addr;
    std::uint16_t width;
    std::uint16_t height;
    std::uint16_t type;
    std::uint16_t pad;
};
#pragma pack(pop)
static_assert(sizeof(TextureHeader) == 12);
inline void le_fix(TextureHeader& h) {
    RAC1_LE_FIX(h, palette_addr);
    RAC1_LE_FIX(h, width);
    RAC1_LE_FIX(h, height);
    RAC1_LE_FIX(h, type);
    RAC1_LE_FIX(h, pad);
}

class LevelLoader {
public:
    // Global EE RAM Virtual Addresses for Level Transitions
    static constexpr std::uint32_t ADDR_NEXT_LEVEL_ID      = 0x0015f600;
    static constexpr std::uint32_t ADDR_ACTIVE_LEVEL_ID    = 0x0015ed84;
    static constexpr std::uint32_t ADDR_LOADING_COMPLETE   = 0x0015f618;
    static constexpr std::uint32_t ADDR_LEVEL_WAD_LBA      = 0x00139068;
    static constexpr std::uint32_t ADDR_LEVEL_WAD_SECTORS  = 0x0013906c;
    static constexpr std::uint32_t ADDR_LEVEL_LOADER_BASE  = 0x001940cc;
    static constexpr std::uint32_t ADDR_CORE_TEMP_BUFFER   = 0x001940d0;
    static constexpr std::uint32_t ADDR_LEVEL_WAD_BASE     = 0x001940d4;

    static constexpr std::uint32_t VIRT_ADDR_STAGING       = host_layout::STAGING;
    static constexpr std::uint32_t VIRT_ADDR_DECOMPRESSED  = host_layout::CORE;

    static void load_level_wad(int level);
    // The legacy host renderers' copy of the level (the textures for the GS renderer, the host world's tfrag /
    // tie / shrub meshes and their GL caches, the GLB mobys' models): ~150..250 ms of a level's start, used only by the host
    // world draw (tfrag_render_update: RAC1_MRP=0 / RAC1_WORLD_MRP=0), the GLB moby drawer (RAC1_MOBY_VU1=0) and the title's
    // host scene (RAC1_TITLE_HOST). Built at the level's load when `g_legacy_level_needed` says one of them is in use
    // (nullptr: always), else left pending and built by legacy_level_ensure when one is chosen later (the debug menu).
    static void legacy_level_ensure();
    // The level's sectors read from the disc and its core decompressed ahead on a worker thread (the level exit's start, when the
    // next level is known): load_level_wad of that level then takes them instead of reading and decompressing (~40..70 ms,
    // and a cold disk's read, ~300 ms) on the game's thread. Only the host's copy: nothing of the game's state.
    static void prefetch(int level);
    static void register_level_textures(std::span<const std::byte> level_wad_data);
    static std::uint32_t load_level_overlay(int level);
    static std::uint32_t load_level_overlay(std::span<const std::byte> level_wad_data);
};

} // namespace rac1

#endif // RAC1_RUNTIME_LEVEL_LOADER_H
