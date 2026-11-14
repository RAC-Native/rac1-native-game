// The level cache's tables chunk (`TABL`): the static tables of a level's overlay, as the overlay has them, cut out of the
// overlay image on the disc (docs/formats/README.md, "The level cache", "The tables chunk"). Stage 5 (K3) writes and checks
// them; no reader of the game uses them yet.
//
// A level's overlay (the level's own build of the engine: its code, data and tables, linked at its own addresses) is a
// Ratchet Executable inside the level's data file. The ports name its objects by Veldin's (level 0's, NTSC-U) addresses
// and translate them onto the level's (OV(), ovl_reloc.h). The tables here are those whose bytes the game never writes
// after the overlay load (each table's entry below says what the code shows; the stage-5 notes have the run-time census).
// For each table the cache holds: its id, Veldin's address, the level's address (the translation's answer when the file
// was made), its size, its row stride and the words of each row that are run-time state (left out of every comparison:
// a reader takes those from the game's memory), then its bytes, little-endian as on the disc.
//
// This file is pure: it parses bytes and lays out bytes. The disc reads, the translation (the overlay maps) and the files
// are its caller's (src/game/level/level_cache_load.cpp).
#ifndef RAC1_FORMATS_OVERLAY_TABLES_H
#define RAC1_FORMATS_OVERLAY_TABLES_H

#include <formats/level_cache.h>

#include <cstdint>
#include <functional>
#include <span>
#include <string>
#include <vector>

namespace rac1::overlay_tables {

constexpr uint32_t kChunkId = level_cache::fourcc("TABL");
constexpr uint16_t kChunkVersion = 1;  // (bump it when a table is added, dropped or measured differently)

// How a table's extent is found on a level
enum class Extent : uint8_t {
    Fixed,     // `size` bytes from the translated address
    RowsToZero,  // rows of `stride` bytes up to and with the first whose first word is 0
    Section,   // the whole overlay section that starts at the translated address (rows up to a -1 key in it)
};

// One table the cache holds (Veldin's NTSC-U address: every level and region reaches it through the translation)
struct TableDef {
    uint32_t id;       // four characters (level_cache::fourcc)
    const char* name;  // for the messages
    uint32_t veldin;   // Veldin's address
    Extent extent;
    uint32_t size;     // Fixed: the size; others: 0
    uint32_t stride;   // the row's size (0: not rows)
    uint32_t live;     // bit k: word k of each row is run-time state (not compared, not cached for readers)
};
// The tables, in the order the chunk lists them
std::span<const TableDef> table_defs();
// The definition of table `id` (null: none)
const TableDef* table_def(uint32_t id);

// One table as cached
struct Table {
    uint32_t id = 0;
    uint32_t veldin = 0;      // Veldin's address
    uint32_t level_addr = 0;  // the level's address (the translation's, when the file was made)
    uint32_t stride = 0;
    uint32_t live = 0;        // see TableDef
    std::vector<uint8_t> bytes;  // little-endian, as the overlay has them
};
// Whether byte `offset` of table `t` is in a word that is run-time state (its row's live mask)
bool is_live(const Table& t, uint32_t offset);

// ---- The overlay image ----

// One block of the overlay executable: where the overlay load copies it
struct Section {
    int index = 0;
    uint32_t addr = 0;
    uint32_t size = 0;
    uint32_t type = 0;           // ELF section type: 1 bytes (PROGBITS), 8 zeros (NOBITS)
    std::span<const uint8_t> bytes;  // (type 1: `size` bytes, inside the caller's buffer)
};
// The overlay's blocks from the level's data file as the disc reader gives it (disc_read_level_overlay: the data file's
// first sectors, the overlay's offset and size at +0 / +4, then the executable). All fields little-endian. False, with
// `why`, when the bytes are not an overlay.
bool parse_overlay(std::span<const uint8_t> data_file_head, std::vector<Section>& out, std::string& why);

// ---- Extraction ----

// The translation the caller gives: Veldin's data address on the level (ovl_xlate_data's answer for the overlay)
using Xlate = std::function<uint32_t(uint32_t veldin)>;
// A step before a table is kept (the region's fix of the words that point into the boot executable: the caller's)
using Fix = std::function<void(uint32_t level_addr, std::vector<uint8_t>& bytes)>;

// Each table of table_defs() found in `sections` through `xlate`, its bytes copied out (then `fix`ed when given). A table
// the translation cannot place as one piece (its last word does not translate to its first word's place plus the size), or
// that is not inside one section with bytes, or whose rows have no end: left out, a line in `notes` saying why.
void extract(std::span<const Section> sections, const Xlate& xlate, const Fix& fix, std::vector<Table>& out,
             std::vector<std::string>& notes);

// ---- The chunk ----
//
// TABL version 1, little-endian:
//   +0x00 u32  the level (0..18) whose overlay the tables come from
//   +0x04 u32  the table count n
//   +0x08 u32  the bytes' total
//   +0x0c u32  zero
//   +0x10      n entries of 32 bytes: u32 id, u32 Veldin's address, u32 the level's address, u32 size, u32 the bytes'
//              offset from the chunk's start (a multiple of 16), u32 row stride, u32 live-word mask, u32 zero
//   then the bytes, each table at a multiple of 16, zeros between
std::vector<uint8_t> encode(int level, std::span<const Table> tables);
// False, with `why`, when the bytes are not a version 1 tables chunk
bool decode(std::span<const uint8_t> chunk, int& level, std::vector<Table>& out, std::string& why);

}  // namespace rac1::overlay_tables

#endif
