// The level cache's tables (stage 5, K3; docs/formats/README.md, "The level cache"): made from the disc for each planet
// (the setup's `--step cache`, or at a level's first load when the file is missing or stale), checked against the game's
// memory with RAC1_CACHE_VERIFY=1. No reader of the game uses them yet: making, loading and checking them changes nothing
// in the game's memory or behaviour.
#ifndef RAC1_GAME_LEVEL_LEVEL_CACHE_LOAD_H
#define RAC1_GAME_LEVEL_LEVEL_CACHE_LOAD_H

#include <formats/level_cache.h>
#include <formats/overlay_tables.h>

#include <cstdint>
#include <span>
#include <string>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: the level has started (overlay_entry_start, after its setup): the overlay's tables cache
// loaded, made again when missing or stale (RAC1_LEVEL_CACHE=0: none of it), then compared with the game's memory
// (RAC1_CACHE_VERIFY=1)
void level_cache_level_started();

// orig: (host) the setup's step (`--setup --step cache`): every level's cache file made (`force`: also those that load
// fine), each level's translation set in turn (the setup's own process only: it changes the current translation). False
// when one could not be made; the lines it prints say why.
bool level_cache_setup_step(bool force);

// orig: (host) the cache's folder: <asset root>/generated/level_cache (decided once)
const std::string& level_cache_dir();

// orig: (host) the tables of `level`'s overlay read from the disc now, on the current translation (it must be that
// level's: ovl_reloc_set_level), with the region's fix of their pointers into the boot executable; `notes` the tables
// left out. False, with `why`, when the overlay cannot be read.
bool level_cache_extract(int level, std::vector<overlay_tables::Table>& out, std::vector<std::string>& notes, std::string& why);

// orig: (host) what a cache file of `level` must have been made from: the disc's serial and region, the fingerprint of its
// sources (docs/formats/README.md, "The tables chunk": the boot executable, the level's data file, from the disc
// folder's layout.txt). False, with `why`, when it cannot be told (no layout.txt: the image read directly).
bool level_cache_stamp(int level, level_cache::Stamp& out, std::string& why);

// orig: (host) the cache file's bytes for `level` from its tables
bool level_cache_file_bytes(int level, const level_cache::Stamp& stamp, const std::vector<overlay_tables::Table>& tables,
                            std::vector<uint8_t>& out, std::string& why);

// orig: (host) the tables of a cache file's bytes (or of the file `path` when `image` is empty), checked against `stamp`
// and the current translation (each table's level address); false, with `why`, when the file is to be made again
bool level_cache_read(const std::string& path, std::span<const uint8_t> image, int level, const level_cache::Stamp& stamp,
                      std::vector<overlay_tables::Table>& out, std::string& why);

// orig: (host) each table against the game's memory at its level address (the bytes its live mask names left out): the
// number of tables that differ; `first` the first difference in words (empty: none)
int level_cache_compare(const std::vector<overlay_tables::Table>& tables, std::string& first);

}  // namespace rac1

#endif
