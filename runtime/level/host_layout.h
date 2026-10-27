// Where the host keeps its own copy of the level's render assets in EE memory (the static
// renderers and texture tables of the host GS stand-in read them there). EE RAM is 64 MB on the
// host; the game uses the low 32 MB, so these live above 0x02000000 and never overlap the
// game's own level layout.
#pragma once

#include <cstdint>

namespace rac1::host_layout {

inline constexpr std::uint32_t EE_END = 0x04000000;         // the end of the host's EE memory (64 MB)
// the debug menu's free hoverboard (src/_devtools/dev_hoverboard.cpp): the hero's board sequences of another level, 256 KB
inline constexpr std::uint32_t DEV_BOARD_SEQS = 0x02300000;
inline constexpr std::uint32_t CORE = 0x024d4000;           // the level core (index + decompressed data)
inline constexpr std::uint32_t LOADER_ANCHOR = 0x02440000;  // written to 0x1940cc by the host loader
inline constexpr std::uint32_t CORE_TEMP = 0x024a4000;      // written to 0x1940d0 by the host loader
inline constexpr std::uint32_t STAGING = 0x028d4000;        // the level WAD read for a transition
inline constexpr std::uint32_t TEX_HEAP = 0x03500000;       // decoded texture headers / pixels
// 0x03c00000..0x03f00000: the moby pass's shadow run's packets and work area (the legacy host's GS packet ring before;
// src/game/render/mrp_lod.cpp shadow_packet_room)
inline constexpr std::uint32_t TEX_TABLE_TFRAG = 0x03f00000;
inline constexpr std::uint32_t TEX_TABLE_MOBY = 0x03f10000;
inline constexpr std::uint32_t TEX_TABLE_TIE = 0x03f20000;
inline constexpr std::uint32_t TEX_TABLE_SHRUB = 0x03f30000;
// 0x03f40000..0x04000000: the moby pass's shadow run's skinning records (mrp_lod.cpp shadow_packet_room)

}  // namespace rac1::host_layout
