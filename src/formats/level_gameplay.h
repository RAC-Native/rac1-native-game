#ifndef RAC1_FORMATS_LEVEL_GAMEPLAY_H
#define RAC1_FORMATS_LEVEL_GAMEPLAY_H

// A level's gameplay file: its block table summarised, and the blocks
// the tools read (moby placements, pvars, paths, cameras, volumes) decoded.

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1 {

struct Vec3f {
    float x, y, z;
};

struct Vec4f {
    float x, y, z, w;
};

struct Rgb96 {
    std::int32_t r, g, b;
};

// A moby placed in the level (from its 0x78-byte record; the fields not known are named by their
// offset in it).
struct MobyPlacement {
    std::int32_t id;                 // its index in the block
    std::int32_t class_id;           // +0x18
    float scale;                     // +0x1c
    float draw_distance;             // +0x20
    std::int32_t update_distance;    // +0x24
    Vec3f position;                  // +0x30
    Vec3f rotation;                  // +0x3c
    std::int32_t group;              // +0x48
    std::int32_t rooted;             // +0x4c
    float rooted_distance;           // +0x50
    std::int32_t pvar_index;         // +0x58
    std::int32_t occlusion;          // +0x5c
    std::int32_t mode_bits;          // +0x60
    Rgb96 colour;                    // +0x64
    std::int32_t light;              // +0x70
    std::int32_t word_4;
    std::int32_t word_8;
    std::int32_t word_c;
    std::int32_t word_10;
    std::int32_t word_14;
    std::int32_t word_54;
    std::int32_t word_74;
};

// A moby's pvars: where in the pvar data they start and how many bytes they are.
struct PvarSpan {
    std::int32_t offset;
    std::int32_t size;
};

// A path: its points in world space (w is 1).
struct GameplayPath {
    std::vector<Vec4f> points;
};

// A camera placement (32 bytes as stored).
struct CameraPlacement {
    std::int32_t type;
    Vec3f position;
    Vec3f rotation;                  // Euler angles
    std::int32_t pvar_index;
};

// A volume (cuboid, sphere, cylinder or pill), 128 bytes as stored.
struct VolumeShape {
    float matrix[16];                // model to world
    float inverse_matrix[12];        // world to model, 3 x 4
    float rotation[3];               // Euler angles
    float word_7c;
};

struct GameplayBlockSummary {
    std::string name;
    bool present = false;
    std::uint32_t offset = 0;
    std::uint32_t size = 0;
    std::uint32_t row_count = 0;
};

struct DecodedGameplay {
    std::vector<MobyPlacement> mobys;
    std::vector<PvarSpan> pvar_spans;
    std::vector<std::uint8_t> pvar_data;
    std::vector<GameplayPath> paths;
    std::vector<CameraPlacement> cameras;
    std::vector<VolumeShape> cuboids;
    std::vector<VolumeShape> spheres;
    std::vector<VolumeShape> cylinders;
    std::vector<VolumeShape> pills;
    std::vector<GameplayBlockSummary> blocks;
};

// Decodes a level's gameplay file, WAD-compressed or not.
DecodedGameplay decode_level_gameplay(std::span<const std::byte> data);

// ---- The level-data objects as the level's load lays them out (stage 5: game/level/level_registry.h) ----
// The blocks the load (gameplay_load, game/ovl/level_load.cpp) copies into the engine's level-data tables, parsed from the
// unpacked file with explicit little-endian reads (no struct laid over the file's bytes: right on big-endian hosts too), each
// field where the load puts it. Offsets below are the file's block table words (H + 0x..).

// A path (+0x70's), also a grind rail's spine (+0x74's): its point count (a word), 12 bytes, then 16-byte points
struct LdPath {
    std::uint32_t offset = 0;       // where it starts in its block's data (the table's entry: the data's address + offset)
    std::int32_t count = 0;         // the count word as the file has it
    std::uint32_t head[3] = {};     // the 12 bytes after it
    std::vector<Vec4f> points;      // x y z, w a per-point value (count of them; none when the count does not fit the data)
};

// A shape of the three lists (+0x60 boxes, +0x64 spheres, +0x68 cylinders), 0x80 bytes as stored
struct LdShape {
    float matrix[16];               // +0x00: the shape's matrix, rows; its centre is the fourth row (+0x30)
    float to_unit[16];              // +0x40: world to the unit shape
};

// A volume (+0x6c's): 0x90 bytes copied from records 0x80 apart (the load reads them so: +0x80..+0x8f are the next
// record's first 16 bytes in the file, the last one's the 16 bytes after the block)
struct LdVolume {
    LdShape shape;
    float tail[4];                  // +0x80: the end caps' radius first
};

// A level camera (+0x08's), 32 bytes as stored; the load puts the type at +0xc of its 0x20-byte entry, the position at +0,
// the rotation at +0x10 and the camera's pvar block (moved out of the pvar data) at +0x1c
struct LdCamera {
    std::int32_t type = 0;          // file +0x00
    float position[3] = {};         // file +0x04
    float rotation[3] = {};         // file +0x10
    std::int32_t pvar_index = -1;   // file +0x1c (< 0: none)
};

// A grind rail (+0x74's): a 0x20-byte record (centre, reach, the loop word at +0x10), its spine a path in the block's data
struct LdRail {
    float centre[3] = {};           // +0x00 (the load's entry +0x00)
    float radius = 0.0f;            // +0x0c (its reach: find_rail_v skips the rail beyond it)
    std::int32_t loop = 0;          // file +0x10 (the entry's +0x14)
    LdPath spine;                   // the entry's +0x10 points at it
};

struct GameplayLevelData {
    bool has_paths = false, has_volumes = false, has_shapes[3] = {}, has_cameras = false, has_rails = false, has_cam_volumes = false;
    std::vector<LdPath> paths;
    std::uint32_t path_data_size = 0;  // the paths' data block, as copied (its size word)
    std::vector<LdVolume> volumes;
    std::vector<LdShape> shapes[3];
    std::vector<LdCamera> cameras;
    std::vector<LdRail> rails;
    std::uint32_t rail_data_size = 0;
    std::vector<std::uint8_t> cam_volumes;  // +0x84's block from +0x10, (its size word + 0x4000) bytes, as the load copies it
};

// Parses the level-data objects of an unpacked gameplay file. Reads never go past `file` (a field outside it reads 0, an
// object whose records do not fit is cut short and named in `*error`); false when the block table itself does not fit.
bool parse_gameplay_level_data(std::span<const std::uint8_t> file, GameplayLevelData& out, std::string* error = nullptr);

} // namespace rac1

#endif // RAC1_FORMATS_LEVEL_GAMEPLAY_H
