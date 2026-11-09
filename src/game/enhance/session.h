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

}  // namespace rac1

#endif
