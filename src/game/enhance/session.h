// A custom-level session's runtime (docs/open-engine.md). Stage 1 of the open engine: the session's memory (EE memory
// grown past the original's 64 MB, runtime/kernel/ee_memory.h) and the session heap above 64 MB, where the host places
// what a session brings in (a pool class's block, later a section's collision, a level's context). Nothing of the game
// ever allocates there by itself. Nothing here runs unless a session's memory is on: on the original's levels the EE
// memory, its accessors and every guard are as before.
#ifndef RAC1_GAME_ENHANCE_SESSION_H
#define RAC1_GAME_ENHANCE_SESSION_H

#include <cstddef>
#include <cstdint>
#include <string>

namespace rac1 {

// The session heap's start: the first address past the host's areas (runtime/level/host_layout.h's EE_END)
constexpr uint32_t SESSION_HEAP_BASE = 0x04000000u;
// Its blocks' alignment (a class block's quadwords, its packets' DMA tags: 16; a cache line for the host's copies)
constexpr uint32_t SESSION_HEAP_ALIGN = 64;

// orig: (host) a session's memory is on (the EE memory grown, the heap usable)
bool session_memory_on();
// orig: (host) the session's memory on: the EE memory grown to ee_session_ram_max (its pages committed as they are
// touched) with an empty heap. True when on (already, or now)
bool session_memory_begin();
// orig: (host) the session's memory off: the heap emptied, the EE memory back to the original's 64 MB
void session_memory_end();
// orig: (host) the testing switches read once at the start (after the EE memory exists): RAC1_SESSION_MEM=1 turns the
// session's memory on for the whole run (the original's levels must play the same with it: their low 32 MB are equal)
void session_start_tests();

// orig: (host) `size` bytes of the session heap (SESSION_HEAP_ALIGN aligned, zero-filled) for `owner` (a pool class
// "pool 0:500", a section, a context: what frees them together); 0 when the heap is full or no session's memory is on
uint32_t session_heap_alloc(uint32_t size, const std::string& owner);
// orig: (host) the block at `addr` given back (zeroed); false when it is not a block of the heap
bool session_heap_free(uint32_t addr);
// orig: (host) every block of `owner` given back; their count
size_t session_heap_free_owner(const std::string& owner);
// orig: (host) the heap's bytes in use (its blocks, aligned), and its blocks
uint32_t session_heap_used();
size_t session_heap_blocks();
// orig: (host) the heap's state logged: `[session] heap: <used> in <n> blocks, <free> free in <runs> runs` and each
// owner's share (RAC1_SESSION_TRACE=1 also logs every alloc and free)
void session_heap_log();
// orig: (host) a hash of the heap's blocks (their addresses, sizes and bytes): a session's own check beside RAMHASH,
// which stays the PS2's 32 MB
uint64_t session_heap_hash();

// ---- Classes from the pool in the session heap (stage 1: the test of RAC1_SESSION_HIGH_CLASS) ----
// RAC1_SESSION_HIGH_CLASS=<class> (testing): the session's memory on, and the level's own class `class` registered from a
// copy of its pool block in the session heap instead of from the level's asset data (the same class at an address above
// 64 MB: every picture must be the same). RAC1_SESSION_HIGH_CLASS=<class>:<id>: the copy registered besides the native
// class, under the session class id <id> (2048..7871: the id -> index map 0x197bc0 has room for them), after the level's
// own classes; the level's placed mobys of `class` are placed as <id> (its update function and +0x2c word are the pool
// row's, the modern renderer draws it with the class's file).
// (any session class: the hooks' one test on the original's levels)
extern bool g_session_classes;
// orig: (host) a level's full setup begins (its overlay loaded again: the class tables empty): the session classes of the
// level before dropped, their heap blocks freed
void session_level_begin();
// orig: (host) the level setup about to register class `cls` of level `level` from `native` (0: no data): the address to
// register it from (the pool block's copy in the session heap, for the test's class; `native` otherwise)
uint32_t session_class_block(int level, uint32_t cls, uint32_t native);
// orig: (host) the level's own classes registered (its moby textures at `tex`), before its gameplay: the session classes
// registered
void session_classes_register(int level, uint32_t tex);
// ---- Classes of other levels (the open engine's stage 2: context.h) ----
// RAC1_SESSION_IMPORT=<level>:<class>[,...] (testing): the session's memory on, and at each level setup the classes
// imported: each from its pool block in the session heap, its level's context made (context.h), registered under its own
// class id (a class the base has already: a session id, for a second copy), its update function and +0x2c word through
// the context (its level's own code: a tag), drawn by the modern renderer from its own level's class file (the import list)
// orig: (host) class `cls` of level `level` added to the session's imports (registered at the next level setup), as the
// testing switches' (RAC1_SESSION_IMPORT, the twin run)
void session_import_add(int level, uint32_t cls);
// ---- The pool complete (the open engine's stage 3) ----
// An import brings its closure: the classes it spawns (its own code's and the engine's for it: pool/deps, made by
// tools/overlay_match/deps.py), each from the import's level unless the base has it, their sounds in their level's bank
// (one host bank a level, shared, unloaded with the level's last class). A class can be unloaded during play (its mobys
// retired, its class table entries cleared and its index given back, its heap blocks, context and bank freed a few frames
// later) and loaded again.
// orig: (host) class `cls` of level `level` added to the session's imports, by `from` ("test", "mod", "plugin")
void session_import_request(int level, uint32_t cls, const char* from);
// orig: (host) a level's header read (level_slots.cpp): the level mod's imports (its build's imports.txt, the composer's
// closure of its level.txt's `import` lines) for this level, none for a game level ("": the mod's of the level before go);
// the session's memory on when it has some
void session_level_mod(const std::string& build_dir);
// orig: (host) the class id under which class `cls` of level `level` is in the session (an import's, or the base's own
// class), -1 when it is not
int32_t session_import_id(int level, uint32_t cls);
// orig: (host) class `cls` of level `level` imported now, during play (with its closure): its id, -1 when it cannot be (or
// before the level's setup: then registered at it)
int32_t session_import_load(int level, uint32_t cls, const char* from);
// orig: (host) the import of class `cls` of level `level` dropped: it and the dependencies no other import needs unloaded;
// how many classes went
size_t session_import_unload(int level, uint32_t cls);
// orig: (host) a moby of session class `id` spawned at `pos` turned `yaw`, from the class's first placement on its own level
// (the pool's placements: its scale, update distance, flags, colour, pvars); 0 when it cannot be (no such class, no room)
uint32_t session_import_spawn(uint32_t id, const float pos[3], float yaw);
// The spawn watch (a session with classes of other levels, or the probe): moby_spawn asks session_spawn_check first, the
// moby dispatch notes whose update runs
extern bool g_spawn_watch;
extern uint32_t g_spawn_updating;  // (the moby whose update runs, 0: none)
// orig: (host) moby_spawn about to spawn class `cls`: false when the session lacks the class (refused, as when the moby
// array is full; logged once per spawner and class: a dependency the closure missed)
bool session_spawn_check(uint32_t cls);
// The spawn probe (RAC1_DEPS_PROBE=1, testing, on the game's own levels): every class a moby's update spawns, by the
// spawner's class, logged per level (`[deps] probe level N: class C spawned D x n,...`): the check of deps.py's report
extern bool g_spawn_probe;
bool spawn_probe_on();
void spawn_probe_note(uint32_t cls);
void spawn_probe_end();
// orig: (host) a game frame begins (the host's loop, game thread): the unloads' frees when due, the spawns' pvars of
// mobys gone, the residency test (RAC1_SESSION_CYCLE)
void session_frame(uint64_t frame);
// orig: (host) the session's memory logged: the imported classes, the contexts, the other blocks, the heap, the sound
// banks, the class table, the renderer's import list (`[session] memory: ...`, after each change)
void session_memory_log();
// orig: (host) a placed moby's class id: the session class id it is placed as, else `cls`
uint32_t session_placed_class(uint32_t cls);
// orig: (host) session class `cls`'s class table row (its update function, its +0x2c word); false when it is not one
bool session_class_row(uint32_t cls, uint32_t* fn, uint32_t* word);
// orig: (host) session class `cls`'s source: its level and class; false when it is not one
bool session_class_source(uint32_t cls, int* level, uint32_t* src);

}  // namespace rac1

#endif
