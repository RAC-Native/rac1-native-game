// (Here, not in src/game: the generated code is fingerprinted by src/game's headers; the code is src/game/enhance/level_slots.cpp.)
// Host level slots (docs/mods.md, "Level mods"): a level mod's level is a level of its own to the player and to the host,
// on host slot S (19, 20, ...: runtime/kernel/mods.h), and runs on a base level B of the game (its code, its address map:
// format 1, Veldin, level 0). While S is current the game's level number (0x15ed84) is B: the game's own code is B's,
// unchanged; everything of the host that is per level asks level_slot(), which is S.
//
// - Its files: the level's build (assets/generated/mods/<folder>/) through the disc layer's window for S
//   (runtime/kernel/disc.h): B's level header read, its data and gameplay ranges pointed into the window, its music
//   table the build's choice (another level's tracks, none, or the build's VAG files in the window).
// - The ship's planet list: a row per level mod on after the unlocked planets (their planet B, which the list never shows;
//   which row is selected is ours to know: level_slots_list_select). Choosing one flies to B with S chosen;
//   RAC1_GOTO=<level.id>@<frame> chooses it too (testing).
// - Its own state: the save's per-level records (the list 0x1848c0, per level) of B are S's while S is played; B's are
//   kept in host memory and swapped back when another level loads. A memory card save always gets B's real records:
//   the card's saves are byte for byte the game's (the current level in them B, as the game wrote it).
// - Its progress: in save<N>.pc (advanced.h) as our item 0x200, one per level: its level.id, whether the save was made on
//   it, the level flown from, its records. Items of levels whose mod is off or missing are carried over (written back
//   as they were read).
// - A save made on a level whose mod is off or missing: the player lands on the level flown from, and the ship's planet
//   map opens there (the save's unlocked planets, that one first): the planet prompt. Leaving the map for that level
//   (choosing it, or triangle / start) loads it again (Ratchet is not in his ship).
// With no level mod on (headless, RAC1_MODS=0, no mods: the reference checks) nothing here does anything, except the
// .pc items' carry-over and the prompt, which need a save that has our items.
#ifndef RAC1_HOST_LEVEL_SLOTS_H
#define RAC1_HOST_LEVEL_SLOTS_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// orig: (none) — enhancement: a level mod is on in this run (one test, every hook's gate)
bool level_slots_on();

// orig: (none) — enhancement: the level the host's per-level data is for: S while slot S's level is current, else the game's
// level 0x15ed84
int level_slot();

// orig: (none) — enhancement: level_slot() for the game's level `level` as a frame's copy of EE memory has it (the renderer's
// threads: draw_lw(0x15ed84))
int level_slot_of(int level);

// orig: (none) — enhancement: the slot whose level is current (-1: none)
int level_slot_current();

// orig: (none) — enhancement: slot `slot`'s base level (-1: no such slot)
int level_slot_base(int slot);

// orig: (none) — enhancement: slot `slot`'s build folder ("" when none) and its mod's folder (its level/ files)
std::string level_slot_build_dir(int slot);
std::string level_slot_mod_dir(int slot);

// orig: (none) — enhancement: the ship's planet list (galmap_list_focus_in): our row added at entry `n` of the list `list` (0xc
// bytes an entry); the count of entries after it (n or n + 1)
uint32_t level_slots_list_rows(uint32_t list, uint32_t n);

// orig: (none) — enhancement: the ship's planet list's selection when it opens (galmap_list_focus_in, `n` its rows): our row
// while its level is current (as the game selects the current planet's), the first in the planet prompt (the level flown
// from), else -1 (the game's choice)
int32_t level_slots_list_focus(uint32_t n);

// orig: (none) — enhancement: a list's selection moved (menu_button_update, `items` its entries, `sel` the row): on the ship's
// planet list, the planet the map shows (our rows' base level) and the row's slot
void level_slots_list_select(uint32_t items, int32_t sel);

// orig: (none) — enhancement: the ship's map left for the current planet (triangle, start: the landing there again)
void level_slots_list_back();

// orig: (none) — enhancement: the planet chosen on the ship's map (planet_travel): true when it is our row and the current level
// is its base, so that the travel must load it again (no landing)
bool level_slots_travel(int32_t planet);

// orig: (none) — enhancement: testing: as the row of the level `id` (level.id) was chosen (RAC1_GOTO=<id>@F); the base level to
// change to, -1 when no level mod on has that id
int32_t level_slots_choose_id(const std::string& id);

// orig: (none) — enhancement: level_header_read wrote `level`'s header (0x13a4e0): a slot's, its ranges pointed into its window
void level_slots_header_read(int32_t level);

// orig: (none) — enhancement: `level`'s gameplay load starts (gameplay_load, before its mobys spawn): the per-level records swapped
// in (entering a slot's level) or back (leaving it)
void level_slots_level_start(int32_t level);

// orig: (none) — enhancement: whether the level exit's story cards and movies of the base level are left out: the travel is to /
// from a slot's level
bool level_slots_skip_story();

// orig: (none) — enhancement: the travel is to a slot's level whose base level is `next`: the base level's story cards and
// movies of the first flight there (Eudora, Gemlik Base, Kalebo III) are left out
bool level_slots_story_to(int32_t next);

// orig: (none) — enhancement: a slot's level is the level the game loads or plays (its row chosen, its save loaded): the
// game's tests of Veldin (level 0) as the first level (no landing, the loading screen's "to Veldin" flight, the flight's
// sound, the ship hidden, no trails) treat it as any planet. False with no level mod on
bool level_slots_planet_arrival();

// orig: (none) — enhancement: a slot's level is current: the base level's progress is left alone (its visits, its stats,
// its gadget in the quick select, its visited flag)
bool level_slots_keep_base_progress();

// orig: (none) — enhancement: the loading screen's pictures (loading_screen_setup, before their uploads: `pics` the
// transition file's picture table in EE memory, `lang` its language row): for a slot's level, the build's picture.img and
// banner[.<lang>].img (the mod's, or made from its name by the composer) written over the base level's entries
void level_slots_loading_pictures(uint32_t pics, int32_t lang);

// orig: (none) — enhancement: the ship's map shows one of our rows (its planet page: no missions, no gold bolts, no movie)
bool level_slots_map_row();

// orig: (none) — enhancement: the galaxy page's planet picture (mission_picture_update of the picture of the map's planet,
// its widget `w`): true when it is ours to show (one of our rows: the build's map.img uploaded instead of the disc's
// picture, or none)
bool level_slots_map_picture(uint32_t w);

// orig: (none) — enhancement: the planet page's planet (planet_map_update on the ship's map's planet page): for one of our
// rows the level's picture.img uploaded as the planet's picture (the map's +0x258; +0x228 0x100 + its slot), not the base
// planet's
void level_slots_map_planet_upload();

// orig: (none) — enhancement: the planet page's planet is our row's picture (planet_picture_draw draws it, still)
bool level_slots_map_planet_shown();

// orig: (none) — testing: the landing's end (ship_sequence_update, before the scene's mobys go; `hero` the scene's Ratchet):
// with RAC1_LEVEL_TRACE, where the scene leaves him and where the hero is put, in the ship's frame (any level)
void level_slots_landing_end(uint32_t hero);

// orig: (none) — enhancement: the text of our ids (0x7dc0..), 0 for the others (the text lookup 0x203ad8)
uint32_t level_slots_text(int32_t id);

// orig: (none) — enhancement: save_block_write's entry `e` of the list `list` for level `index`: true when it wrote the record's
// bytes at `out` itself (the base level's real records while a slot's are in EE memory)
bool level_slots_save_record(uint32_t list, int32_t index, uint32_t e, uint32_t out);

// orig: (none) — enhancement: save_block_read read level `index`'s block of `list` into the game: EE memory holds the save's records
void level_slots_block_read(uint32_t list, int32_t index);

// orig: (none) — enhancement: a new game (the disc's default save): no level's progress, none carried over
void level_slots_new_game();

// orig: (none) — enhancement: our items for save<N>.pc (advanced.cpp's save_file_make): appended to `d`, their count returned
uint32_t level_slots_pc_items(std::vector<uint8_t>& d);

// orig: (none) — enhancement: save<N>.pc's item `id` (`size` bytes at `p`) read: true when it is ours
bool level_slots_pc_item(uint32_t id, const uint8_t* p, uint32_t size);

// orig: (none) — enhancement: a save's .pc read (advanced_save_loaded; before: level_slots_pc_reset): back on its level, or the
// planet prompt when that level's mod is not on
void level_slots_pc_reset();
void level_slots_save_loaded();

// orig: (none) — enhancement: once a frame of play (game_frame): the planet prompt opened when it waits
void level_slots_frame();

// orig: (none) — enhancement: testing: once per game frame (the host's frame loop): RAC1_LEVEL_SAVE_IMAGE's save images,
// RAC1_LEVEL_MAP_AT's ship's map
void level_slots_test_frame(uint64_t frame);

}  // namespace rac1

#endif
