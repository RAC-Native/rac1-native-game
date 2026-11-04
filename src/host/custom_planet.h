// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/custom_planet.cpp.)
// A custom planet (local experiment, branch feat/custom-levels-experiment): Maktar's arena (or any planet folder the
// tools make) as an additional planet the ship flies to.
//
// TEMPORARY ROUTE, a "shadow slot": the planet runs in level 0's slot (Veldin's overlay, index, sectors, music table),
// which the ship never reaches otherwise (planet 0 is not in the ship's list). Everything that makes level 0 another
// planet is in this module and in runtime/kernel/disc.h (the file redirect), so a true level slot 19 can replace it later
// without touching the rest: the ship's row, the planet's own state and its progress in our .pc file.
//
// The planet's folder (disc.h: RAC1_CUSTOM_LEVEL, else assets/custom/<name>/) holds data.wad, gameplay.bin (the tools'
// output) and planet.txt, our own small text file, one setting a line ('#' comments):
//   region <text>            the ship's list row (as "Tobruk Crater" is Novalis's), the planet page's label
//                            (region.<en|fr|de|es|it> <text> per text language)
//   name <text>              the line under it ("Novalis"), the planet page's title (name.<lang> likewise)
//   death_height <z>         below it Ratchet dies (the gameplay file's +0x28)
//   ship <x> <y> <z> <yaw>   where the ship stands (the gameplay file's +0x2c; x <= 0: no ship, no way out but the
//                            pause menu)
//
// - The ship's planet list: a row after the unlocked planets, always (Daino's choice for now). Its entry points at the
//   list's end (planet 0): choosing it flies to level 0 with the planet on. Choosing it on level 0 itself loads it again
//   (level 0 has no landing scene).
// - The planet's own state: the save's level blocks are per level (the list 0x1848c0, 20 levels); while the planet is
//   played, level 0's records in EE memory hold the PLANET's (its kill / collected bits, its map...), Veldin's are kept
//   in host memory, and swapped back when another level loads. A memory card save always gets Veldin's real records
//   (save_block_write asks custom_planet_save_record): the card's saves are byte for byte the game's.
// - Its progress: in the save's save<N>.pc (advanced.h) as our item 0x200 (the planet's name, whether the save was made
//   on it, its level records); loading the save gives them back, and a save made on the planet loads it again.
// With no planet folder nothing here does anything: the game is the same.
#ifndef RAC1_HOST_CUSTOM_PLANET_H
#define RAC1_HOST_CUSTOM_PLANET_H

#include <cstdint>
#include <vector>

namespace rac1 {

// orig: (none) — whether a custom planet exists (a folder with planet.txt and data.wad)
bool custom_planet_exists();

// orig: (none) — the ship's planet list (galmap_list_focus_in): our row added at entry `n` of the list `list` (0xc bytes
// an entry); the count of entries after it (n or n + 1)
uint32_t custom_planet_list_row(uint32_t list, uint32_t n);

// orig: (none) — the planet chosen on the ship's map (planet_travel): true when it is the custom planet's row (planet 0)
// and the current level is level 0, so that the travel must load it (no landing again)
bool custom_planet_travel(int32_t planet);

// orig: (none) — testing: as the ship's row was chosen (RAC1_GOTO's "c", the debug menu)
void custom_planet_choose();

// orig: (none) — the text of our ids (0x7d00..), 0 for the others (the text lookup 0x203ad8)
uint32_t custom_planet_text(int32_t id);

// orig: (none) — a level's header read (level_header_read): the planet on (level 0 chosen) or off (another level)
void custom_planet_level_header(int32_t level);

// orig: (none) — a level's gameplay load starts (gameplay_load, before its mobys spawn): the per-level records swapped
// in (entering the planet) or back (leaving it)
void custom_planet_level_start(int32_t level);

// orig: (none) — the gameplay file's settings at `st` (gameplay_load): planet.txt's values over them on the planet
void custom_planet_settings(uint32_t st);

// orig: (none) — whether the level exit's story cards and movies of level 0 (the new game's Veldin, Veldin -> Novalis)
// are left out: the travel is to / from the custom planet
bool custom_planet_skip_story();

// orig: (none) — save_block_write's entry `e` of the list `list` for the level `index`: true when it wrote the record's
// `size` bytes at `out` itself (Veldin's real records while the planet's are in EE memory)
bool custom_planet_save_record(uint32_t list, int32_t index, uint32_t e, uint32_t out);

// orig: (none) — save_block_read read level `index`'s block of `list` into the game: EE memory holds the save's
// records again (the planet's state is the save's .pc's, custom_planet_save_loaded)
void custom_planet_block_read(uint32_t list, int32_t index);

// orig: (none) — a new game (the disc's default save): no progress on the planet, not on it
void custom_planet_new_game();

// orig: (none) — our items for save<N>.pc (advanced.cpp's save_file_make): appended to `d`, their count returned
uint32_t custom_planet_pc_items(std::vector<uint8_t>& d);

// orig: (none) — save<N>.pc's item `id` (`size` bytes at `p`) read: true when it is ours
bool custom_planet_pc_item(uint32_t id, const uint8_t* p, uint32_t size);

// orig: (none) — a save loaded (the card's blocks read, then its .pc): whether the planet's state came with it
void custom_planet_save_loaded(bool pc_read);

// ---- Sound and music (custom_planet_audio.cpp) ----
// From Ratchet & Clank 2's disc, a data disc the setup links (tools/setup.py: the North American, PAL and Japanese
// discs; region "RAC2-..."), read where they are. The disc chosen by the voice language (voice_language.h): Japanese
// from the Japanese disc, French / German / Spanish / Italian from the PAL disc, English from the North American one;
// else whichever is there. planet.txt says what to use: "rac2_level <n>", "rac2_banks <header offsets>" (LEVELn.WAD's
// sound banks, loaded beside the level's own while the planet is on), "music <intro> <loop>" (AUDIOn.WAD's entries,
// the left channels; the right ones follow: stereo, runtime/iop/snd989_stream.cpp).
// orig: (none) — level_header_read read a header: on the planet its music (level 0's tracks 0 and 1) made the planet's
void custom_planet_level_header_read(int32_t level);
// orig: (none) — the music streams (music_streams.cpp): the right channel's sector of the planet's music at `lsn`, else 0
uint32_t custom_planet_music_right(uint32_t lsn);
// orig: (none) — the level's sound bank loaded (the loader's step 7): the planet's banks loaded too
void custom_planet_banks_load();
// orig: (none) — a level's banks unloaded (the loader's step 3): the planet's too
void custom_planet_banks_unload();
// orig: (none) — the planet's banks' handles (empty: none loaded)
const std::vector<uint32_t>& custom_planet_banks();

// orig: (none) — testing: once per game frame (the host's frame loop): RAC1_PLANET_SAVE_IMAGE's save images
void custom_planet_test_frame(uint64_t frame);

}  // namespace rac1

#endif
