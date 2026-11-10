#include <game/render/texture_decode.h>
#include <runtime/kernel/ee_memory.h>

namespace rac1 {

// Palette swizzle helper for PSMT8 (8-bit paletted textures).
// Swaps bit 3 and bit 4 of the palette index.
// legacy (not a port; once mislabelled 0x001f9298)
uint8_t swizzle_palette_index_psmt8(uint8_t index) {
    bool bit4 = (index & 16) != 0;
    bool bit3 = (index & 8) != 0;
    if (bit4 != bit3) {
        return index ^ 0x18; // swap bits 3 and 4
    }
    return index;
}

// Palette swizzle helper for PSMT4 (4-bit paletted textures).
// Uses a 16-entry mapping: 0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15.
// orig: None
uint8_t swizzle_palette_index_psmt4(uint8_t index) {
    static const uint8_t map16[16] = {
        0, 1, 4, 5, 8, 9, 12, 13, 2, 3, 6, 7, 10, 11, 14, 15
    };
    if (index >= 16) {
        return index;
    }
    return map16[index];
}

// Safely retrieves the TextureHeader from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the header address is invalid in EE RAM.
// orig: Inline address math
const TextureHeader* get_texture_header(uint32_t pixel_vaddr) {
    if (pixel_vaddr == 0) {
        return nullptr;
    }
    uint32_t header_addr = pixel_vaddr - 1024 - sizeof(TextureHeader);
    if (!ee_is_valid_addr(header_addr, sizeof(TextureHeader))) {
        return nullptr;
    }
    return reinterpret_cast<const TextureHeader*>(ee_ram_ptr(header_addr));
}

// Safely retrieves the raw palette pointer (1024 bytes) from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the palette address is invalid.
// orig: Inline address math
const uint32_t* get_texture_palette(uint32_t pixel_vaddr) {
    if (pixel_vaddr == 0) {
        return nullptr;
    }
    uint32_t palette_addr = pixel_vaddr - 1024;
    // Standard palette is 256 colors (1024 bytes)
    if (!ee_is_valid_addr(palette_addr, 1024)) {
        return nullptr;
    }
    return reinterpret_cast<const uint32_t*>(ee_ram_ptr(palette_addr));
}

// Safely retrieves the raw pixel data pointer from EE RAM.
// Returns nullptr if pixel_vaddr is 0 or if the pixel data is out of bounds.
// orig: Inline address math
const uint8_t* get_texture_pixels(uint32_t pixel_vaddr, uint32_t size) {
    if (pixel_vaddr == 0) {
        return nullptr;
    }
    if (!ee_is_valid_addr(pixel_vaddr, size)) {
        return nullptr;
    }
    return reinterpret_cast<const uint8_t*>(ee_ram_ptr(pixel_vaddr));
}

// Decodes a PSMT8 (8-bit indexed) texture to linear RGBA8.
// If fix_alpha is true, multiplies alpha by 2 and clamps to 255 (PS2 alpha range is 0-128).
// orig: None (PC port helper)
void decode_psmt8(
    const uint8_t* src_pixels,
    const uint32_t* src_palette,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_rgba,
    bool fix_alpha) 
{
    if (!src_pixels || !src_palette || !dst_rgba || width == 0 || height == 0) {
        return;
    }

    uint32_t num_pixels = width * height;
    
    // We pre-decode and unswizzle the palette to make lookup fast
    uint32_t swizzled_palette[256];
    for (int i = 0; i < 256; ++i) {
        uint8_t swizzled_idx = swizzle_palette_index_psmt8(static_cast<uint8_t>(i));
        uint32_t color = src_palette[swizzled_idx];
        if (fix_alpha) {
            uint32_t alpha = (color >> 24) & 0xff;
            alpha = (alpha < 0x80) ? (alpha * 2) : 255;
            color = (color & 0x00ffffff) | (alpha << 24);
        }
        swizzled_palette[i] = color;
    }

    for (uint32_t i = 0; i < num_pixels; ++i) {
        uint8_t idx = src_pixels[i];
        uint32_t color = swizzled_palette[idx];
        dst_rgba[4 * i + 0] = color & 0xff;          // R
        dst_rgba[4 * i + 1] = (color >> 8) & 0xff;   // G
        dst_rgba[4 * i + 2] = (color >> 16) & 0xff;  // B
        dst_rgba[4 * i + 3] = (color >> 24) & 0xff;  // A
    }
}

// Decodes a PSMT4 (4-bit indexed) texture to linear RGBA8.
// If fix_alpha is true, multiplies alpha by 2 and clamps to 255.
// orig: None (PC port helper)
void decode_psmt4(
    const uint8_t* src_pixels,
    const uint32_t* src_palette,
    uint32_t width,
    uint32_t height,
    uint8_t* dst_rgba,
    bool fix_alpha) 
{
    if (!src_pixels || !src_palette || !dst_rgba || width == 0 || height == 0) {
        return;
    }

    uint32_t num_pixels = width * height;
    
    // We pre-decode and unswizzle the palette to make lookup fast
    uint32_t swizzled_palette[16];
    for (int i = 0; i < 16; ++i) {
        uint8_t swizzled_idx = swizzle_palette_index_psmt4(static_cast<uint8_t>(i));
        uint32_t color = src_palette[swizzled_idx];
        if (fix_alpha) {
            uint32_t alpha = (color >> 24) & 0xff;
            alpha = (alpha < 0x80) ? (alpha * 2) : 255;
            color = (color & 0x00ffffff) | (alpha << 24);
        }
        swizzled_palette[i] = color;
    }

    for (uint32_t i = 0; i < num_pixels; ++i) {
        uint32_t byte_idx = i / 2;
        uint8_t byte_val = src_pixels[byte_idx];
        uint8_t idx = ((i % 2) == 0) ? (byte_val & 0x0f) : ((byte_val >> 4) & 0x0f);
        
        uint32_t color = swizzled_palette[idx];
        dst_rgba[4 * i + 0] = color & 0xff;          // R
        dst_rgba[4 * i + 1] = (color >> 8) & 0xff;   // G
        dst_rgba[4 * i + 2] = (color >> 16) & 0xff;  // B
        dst_rgba[4 * i + 3] = (color >> 24) & 0xff;  // A
    }
}

} // namespace rac1
