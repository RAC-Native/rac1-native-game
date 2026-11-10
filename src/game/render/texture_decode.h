#ifndef RAC1_GAME_RENDER_TEXTURE_DECODE_H
#define RAC1_GAME_RENDER_TEXTURE_DECODE_H

#include <cstdint>
#include <runtime/level/level_loader.h>

namespace rac1 {

// Palette swizzle helper for PSMT8 (8-bit paletted textures).
// Swaps bit 3 and bit 4 of the palette index.
// legacy (not a port; once mislabelled 0x001f9298)
uint8_t swizzle_palette_index_psmt8(uint8_t index);

// Palette swizzle helper for PSMT4 (4-bit paletted textures).
// Uses a 16-entry mapping: 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15.
// orig: None
uint8_t swizzle_palette_index_psmt4(uint8_t index);

// Safely retrieves the TextureHeader from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the header address is invalid in EE RAM.
// orig: Inline address math
const TextureHeader* get_texture_header(uint32_t pixel_vaddr);

// Safely retrieves the raw palette pointer (1024 bytes) from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the palette address is invalid.
// orig: Inline address math
const uint32_t* get_texture_palette(uint32_t pixel_vaddr);

// Safely retrieves the raw pixel data pointer from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the pixel data is out of bounds.
// orig: Inline address math
const uint8_t* get_texture_pixels(uint32_t pixel_vaddr, uint32_t size);

// Decodes a PSMT8 (8-bit indexed) texture to linear RGBA8.
// If fix_alpha is true, multiplies alpha by 2 and clamps to 255 (PS2 alpha range is 0-128).
// orig: None (PC port helper)
void decode_psmt8(
    const uint8_t* src_pixels,
    const uint32_t* src_palette,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_rgba,
    bool fix_alpha = true);

// Decodes a PSMT4 (4-bit indexed) texture to linear RGBA8.
// If fix_alpha is true, multiplies alpha by 2 and clamps to 255.
// orig: None (PC port helper)
void decode_psmt4(
    const uint8_t* src_pixels,
    const uint32_t* src_palette,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_rgba,
    bool fix_alpha = true);

} // namespace rac1

#endif // RAC1_GAME_RENDER_TEXTURE_DECODE_H
