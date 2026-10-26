#ifndef RAC1_FORMATS_LEVEL_FILE_H
#define RAC1_FORMATS_LEVEL_FILE_H

// The records of a level's data file (the "level data" range of a level's table-of-contents row)
// and of its asset index, as stored on disc (little-endian, packed).
// (the file) (the asset index) describe every field; the offsets in
// the comments are the fields' byte offsets in their record.

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <span>

#include "io/host_endian.h"

namespace rac1 {

#pragma pack(push, 1)

// A byte range: where a part starts and how long it is.
struct FileSpan {
    std::int32_t offset;
    std::int32_t size;
};
static_assert(sizeof(FileSpan) == 8);

// A table of records: how many there are and where the first one is.
struct TableRef {
    std::int32_t count;
    std::int32_t offset;
};
static_assert(sizeof(TableRef) == 8);

// The level data file's first 0x58 bytes: the eleven parts of the file, each relative to the
// file's start.
struct LevelFileHeader {
    FileSpan overlay;                  // +0x00 the level's code (a Ratchet Executable)
    FileSpan sound_bank;               // +0x08 the level's 989snd bank
    FileSpan asset_index;              // +0x10 the asset index (uncompressed)
    FileSpan vram_image;               // +0x18 palettes and billboard texels, uploaded to the GS as is
    FileSpan hud_index;                // +0x20
    std::array<FileSpan, 5> hud_parts; // +0x28
    FileSpan asset_data;               // +0x50 the asset data (one WAD-compressed block)
};
static_assert(sizeof(LevelFileHeader) == 0x58);

// The asset index's first 0xbc bytes. Offsets named "block" are into the decompressed asset data;
// TableRef offsets and the other offsets are into the asset index itself, unless noted.
struct AssetIndexHeader {
    TableRef vram_pages;               // +0x00
    std::int32_t tfrag_block;          // +0x08 the world's tfrags
    std::int32_t occlusion_block;      // +0x0c
    std::int32_t sky_block;            // +0x10
    std::int32_t collision_block;      // +0x14
    TableRef moby_classes;             // +0x18 ClassRecord rows
    TableRef tie_classes;              // +0x20 ClassRecord rows
    TableRef shrub_classes;            // +0x28 ShrubClassRecord rows
    TableRef tfrag_textures;           // +0x30 TextureRecord rows
    TableRef moby_textures;            // +0x38
    TableRef tie_textures;             // +0x40
    TableRef shrub_textures;           // +0x48
    TableRef particle_textures;        // +0x50 ParticleTextureRecord rows
    TableRef effect_textures;          // +0x58 EffectTextureRecord rows
    std::int32_t texel_block;          // +0x60 the texels of the four TextureRecord tables
    std::int32_t particle_block;       // +0x64 the particle textures' palettes and texels
    std::int32_t effect_block;         // +0x68 the effect textures' palettes and texels
    std::int32_t particle_defs;        // +0x6c
    std::int32_t sound_remap;          // +0x70
    std::int32_t word_74;              // +0x74
    std::int32_t hero_sequences;       // +0x78 256 offsets of the hero's animation sequences (0: none)
    std::int32_t word_7c;              // +0x7c
    std::int32_t gadget_count;         // +0x80 GadgetRecord rows
    std::int32_t gadget_table;         // +0x84
    std::int32_t packed_size;          // +0x88 the asset data's size, compressed
    std::int32_t unpacked_size;        // +0x8c ... and decompressed
    std::int32_t chrome_texture;       // +0x90
    std::int32_t chrome_palette;       // +0x94
    std::int32_t glass_texture;        // +0x98
    std::int32_t glass_palette;        // +0x9c
    std::int32_t word_a0;              // +0xa0
    std::int32_t heightmap;            // +0xa4
    std::int32_t occlusion_octants;    // +0xa8
    std::int32_t moby_vram_stash;      // +0xac
    std::int32_t occlusion_radius;     // +0xb0
    std::int32_t moby_sound_remap;     // +0xb4 into the asset data
    std::int32_t occlusion_radius2;    // +0xb8
};
static_assert(sizeof(AssetIndexHeader) == 0xbc);

// A texture of the tfrag / moby / tie / shrub tables (16 bytes). The game's level setup builds the
// texture's GS registers from it: width / height (TEX0 TW / TH, TBW), mip_count (TEX1 MXL + 1),
// palette (CBP) and the two mip pages (MIPTBP1).
struct TextureRecord {
    std::int32_t texels;               // +0x0 from AssetIndexHeader::texel_block
    std::int16_t width;                // +0x4
    std::int16_t height;               // +0x6
    std::int16_t mip_count;            // +0x8
    std::int16_t palette;              // +0xa in 256-byte units of the VRAM image
    std::int16_t mip1_page;            // +0xc
    std::int16_t mip2_page;            // +0xe
};
static_assert(sizeof(TextureRecord) == 16);

// A moby or tie class (32 bytes).
struct ClassRecord {
    std::int32_t data;                         // +0x00 the class's data, in the asset data
    std::int32_t class_id;                     // +0x04
    std::int32_t word_8;                       // +0x08
    std::int32_t word_c;                       // +0x0c
    std::array<std::uint8_t, 16> texture_remap; // +0x10 local texture -> level texture (0xff: none)
};
static_assert(sizeof(ClassRecord) == 0x20);

// A gadget's moby class, stored compressed in the asset data (16 bytes).
struct GadgetRecord {
    std::int32_t data;                 // +0x0
    std::int32_t class_id;             // +0x4
    std::int32_t packed_size;          // +0x8
    std::int32_t word_c;               // +0xc
};
static_assert(sizeof(GadgetRecord) == 0x10);

// The texture a shrub class is drawn with from far away (16 bytes); its texels and palette are in
// the VRAM image.
struct BillboardRecord {
    std::int16_t width;                // +0x0
    std::int16_t height;               // +0x2
    std::int16_t top_mip;              // +0x4
    std::int16_t palette;              // +0x6 in 256-byte units of the VRAM image
    std::int16_t texels;               // +0x8 in 256-byte units of the VRAM image
    std::int16_t mip_texels[3];        // +0xa
};
static_assert(sizeof(BillboardRecord) == 0x10);

// A shrub class (48 bytes): a ClassRecord and its billboard.
struct ShrubClassRecord {
    std::int32_t data;                         // +0x00
    std::int32_t class_id;                     // +0x04
    std::int32_t word_8;                       // +0x08
    std::int32_t word_c;                       // +0x0c
    std::array<std::uint8_t, 16> texture_remap; // +0x10
    BillboardRecord billboard;                  // +0x20
};
static_assert(sizeof(ShrubClassRecord) == 0x30);

// A particle texture (square, side x side); palette and texels from AssetIndexHeader::particle_block.
struct ParticleTextureRecord {
    std::int32_t palette;              // +0x0
    std::int32_t word_4;               // +0x4
    std::int32_t texels;               // +0x8
    std::int32_t side;                 // +0xc
};
static_assert(sizeof(ParticleTextureRecord) == 16);

// An effect texture; palette and texels from AssetIndexHeader::effect_block.
struct EffectTextureRecord {
    std::int32_t palette;              // +0x0
    std::int32_t texels;               // +0x4
    std::int32_t width;                // +0x8
    std::int32_t height;               // +0xc
};
static_assert(sizeof(EffectTextureRecord) == 16);

#pragma pack(pop)

// The records in the host's byte order (they are read as the PS2's bytes, from the file or from EE memory: io/host_endian.h)
inline void le_fix(TableRef& r) { le_fix_words32(r); }
inline void le_fix(AssetIndexHeader& r) { le_fix_words32(r); }
inline void le_fix(ParticleTextureRecord& r) { le_fix_words32(r); }
inline void le_fix(EffectTextureRecord& r) { le_fix_words32(r); }
inline void le_fix(TextureRecord& r) {
    RAC1_LE_FIX(r, texels);
    RAC1_LE_FIX(r, width);
    RAC1_LE_FIX(r, height);
    RAC1_LE_FIX(r, mip_count);
    RAC1_LE_FIX(r, palette);
    RAC1_LE_FIX(r, mip1_page);
    RAC1_LE_FIX(r, mip2_page);
}
inline void le_fix(ClassRecord& r) {  // (texture_remap: bytes)
    RAC1_LE_FIX(r, data);
    RAC1_LE_FIX(r, class_id);
    RAC1_LE_FIX(r, word_8);
    RAC1_LE_FIX(r, word_c);
}
inline void le_fix(BillboardRecord& r) {
    RAC1_LE_FIX(r, width);
    RAC1_LE_FIX(r, height);
    RAC1_LE_FIX(r, top_mip);
    RAC1_LE_FIX(r, palette);
    RAC1_LE_FIX(r, texels);
    RAC1_LE_FIX(r, mip_texels);
}
inline void le_fix(ShrubClassRecord& r) {
    RAC1_LE_FIX(r, data);
    RAC1_LE_FIX(r, class_id);
    RAC1_LE_FIX(r, word_8);
    RAC1_LE_FIX(r, word_c);
    BillboardRecord b;
    std::memcpy(&b, reinterpret_cast<unsigned char*>(&r) + offsetof(ShrubClassRecord, billboard), sizeof b);
    le_fix(b);
    std::memcpy(reinterpret_cast<unsigned char*>(&r) + offsetof(ShrubClassRecord, billboard), &b, sizeof b);
}

// The level data file inside `file`: `file` itself, or, when `file` starts with the 48-byte
// header of the extracted level files (assets/extracted/level_wads: first word 48, the level
// data's sector and sector count at +0x10 / +0x14), the range that header names when it lies
// inside `file`. `*out_of_range` (when given) tells that such a header named a range past the end.
std::span<const std::byte> level_file_body(std::span<const std::byte> file, bool* out_of_range = nullptr);

} // namespace rac1

#endif // RAC1_FORMATS_LEVEL_FILE_H
