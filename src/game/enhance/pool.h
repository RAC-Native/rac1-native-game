// The asset pool (docs/open-engine.md): every level's moby classes, made from the player's disc into
// assets/generated/pool/ (never in the repository, never shared), for a custom-level session to take any class of any
// level. Stage 1 of the open engine: the class blocks and their index; sounds, contexts and level data come with the
// later stages. Built only when something needs it (a session mod, `rac1-native --setup --pool`, a test), not at setup.
//
// The files:
//   classes.bin  the class blocks as the disc stores them (unrelocated: moby_class_register relocates a copy wherever it
//                is put), each once by content, 64-byte aligned
//   pool.idx     the index, text: a `pool` line, a `level` line per level, a `class` line per (level, class)
//   context/levelNN.img  each level's overlay data sections (its context image: the open engine's stage 2)
//   sounds/levelNN.bnk   each level's sound bank (a host bank for its classes in a session)
//   pool.json    the stamp: the pool's version, the disc it was made from, the counts; a pool from another disc or an
//                older version is made again
#ifndef RAC1_GAME_ENHANCE_POOL_H
#define RAC1_GAME_ENHANCE_POOL_H

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// The pool's format version (pool.json's "version"): a change of what is in it makes every player's pool again
// (2: the levels' context images)
// (3: each level's sound bank, each class's sounds; 4: as many sounds as the class block's definitions)
constexpr int POOL_VERSION = 4;
// The overlays' first address (their data section 0): a context image starts there
constexpr uint32_t CONTEXT_BASE = 0x0015ef00u;

// One (level, class) row of the index
struct PoolClass {
    int level = 0;
    uint32_t id = 0;              // the class id (the moby's +0xa6)
    uint32_t block_off = 0;       // its block in classes.bin (0 / 0: a class without data, as the level's own row says)
    uint32_t block_size = 0;      // (up to the next item of the level's asset data: the block and its padding)
    uint64_t hash = 0;            // the block's content hash (FNV-1a 64): equal blocks are one in classes.bin
    uint32_t update_fn = 0;       // its update function in the level's overlay (the level's class table, section 3)
    uint32_t word2c = 0;          // the class table row's third word (the header's +0x2c once registered)
    uint8_t remap[16] = {};       // the class row's texture remap (into the level's moby textures)
    std::vector<uint16_t> sounds; // its sound definitions' sounds in the level's bank (the asset index +0x70's list)
};

// One level's summary row
struct PoolLevel {
    int level = 0;
    uint32_t classes = 0;         // class rows (with data or not)
    uint32_t with_data = 0;
    uint32_t moby_textures = 0;   // the level's moby texture table
    uint32_t asset_bytes = 0;     // the level's asset data, unpacked
    uint32_t class_bytes = 0;     // its class blocks
    uint32_t collision_bytes = 0, sky_bytes = 0, bank_bytes = 0, vram_bytes = 0, overlay_bytes = 0;
};

// orig: (host) the pool's folder (assets/generated/pool)
const char* pool_dir();
// orig: (host) the pool made from the disc's levels into pool_dir() (every level the disc has; written whole, then
// renamed into place). Deterministic: the same disc gives the same bytes. False with `why` on failure
bool pool_build(std::string* why);
// orig: (host) the pool read (once; made first when it is missing or stale and `build` is set). False when there is none
bool pool_open(bool build);
// orig: (host) the pool's rows (after pool_open)
const std::vector<PoolClass>& pool_classes();
const std::vector<PoolLevel>& pool_levels();
// orig: (host) the row of class `id` of level `level`, nullptr when the level has no such class
const PoolClass* pool_find(int level, uint32_t id);
// orig: (host) the block's bytes in classes.bin (nullptr: a class without data)
const uint8_t* pool_block(const PoolClass& c);
// orig: (host) level `level`'s sound bank (sounds/levelNN.bnk: the level's 989snd bank as its data file holds it); false
// when none
bool pool_sound_bank(int level, std::vector<uint8_t>& bank);
// orig: (host) level `level`'s context image (context/levelNN.img: its overlay's data sections 0..5 as the overlay load
// puts them, from CONTEXT_BASE up to its code, a bss as zeros) and its code section [code_lo, code_hi); false when none
bool pool_context_image(int level, std::vector<uint8_t>& image, uint32_t* code_lo, uint32_t* code_hi);
// orig: (host) the pool's report (`rac1-native --setup --pool-report`): per level and in all, as docs/open-engine.md
void pool_report();

// The pool checked against the levels' native loads (testing, RAC1_POOL_CHECK=1): each class the level setup registers
// is compared with its row (the block's bytes before relocation, the texture remap, then the update function and the
// +0x2c word the registration gave). `[pool] check level N: ...` at the level's end of setup.
// orig: (host) the check is on (RAC1_POOL_CHECK=1)
bool pool_checking();
// orig: (host) level `level`'s class `id` about to be registered from `hdr` (0: no data) with its row's remap `remap`
void pool_check_class(int level, uint32_t id, uint32_t hdr, uint32_t remap);
// orig: (host) ... and registered at class index `idx`
void pool_check_registered(int level, uint32_t id, uint32_t idx);
// orig: (host) the level's classes' sound definitions set (their bank sounds): each class's compared with its row's sounds
void pool_check_sounds(int level);
// orig: (host) the level's classes all registered: the line logged
void pool_check_level_done(int level);

}  // namespace rac1

#endif
