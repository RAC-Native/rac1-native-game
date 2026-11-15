// WAD LZ for the game's own decoder: an encoder whose output RAC1's wad_decompress (orig 0x24d8d0, ported in
// src/game/ovl/level_setup.cpp) accepts, and that decoder's rules step by step to check a stream (the tools' lenient
// decoder, scene_wad.h, accepts streams the game traps on).
//
// The decoder's rules (wad_decode_strict follows them):
// - "WAD", then the stream's total size (u32 at +3, the 16-byte header included), the codes from +0x10.
// - The codes are read through the scratchpad in 0x2000-byte halves (two loaded at the start, then one more each time a
//   half ends). A half ends with an end-of-half code (0x10..0x17 with distance 0 and a count other than 1, e.g.
//   `12 00 00`); the rest of that half is padding, and the codes go on at the start of the next one. A code never
//   straddles two halves. The last half has no end code: the stream stops where the total size says.
// - States: Top (a literal run or a match may come) and Next (only a match). A literal run (code < 0x10, 4..273 bytes)
//   leaves Next; a match whose last read byte has low bits k copies k (0..3) literals after it and leaves Next when
//   k > 0, Top when k = 0. A code below 0x10 in Next traps. `11 0k 00` is a match of nothing (count 1, distance 0):
//   k literals, back to Top when k = 0.
// - The first code may also be a run of (byte - 0x11) literals (byte > 0x11).
// The encoder: greedy LZ77 (3-byte hash chains, distances up to 16384 through the 0x20 and 0x40 codes, lengths
// 3..288) that tracks the decoder's state and the halves: no packet crosses a half's end, each half closes with
// `12 00 00` and padding.
#ifndef RAC1_FORMATS_WAD_ENCODE_H
#define RAC1_FORMATS_WAD_ENCODE_H

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1 {

// orig: (host) `data` as a WAD stream the game's decoder accepts; `lz` false: literal runs only
std::vector<std::uint8_t> wad_encode(std::span<const std::uint8_t> data, bool lz = true);

// orig: (host) the game's decoder (0x24d8d0) on `blob`, its scratchpad halves included: false (with `error`) where the
// game would trap or read past its two halves
bool wad_decode_strict(std::span<const std::uint8_t> blob, std::vector<std::uint8_t>& out, std::string& error);

}  // namespace rac1

#endif
