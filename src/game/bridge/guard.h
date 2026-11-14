// The bridge guard (RAC1_BRIDGE_GUARD, stage 5 of the native 1.0 track; docs/env-vars.md): a run-time watch over ranges of
// the emulated memory, to prove that nothing reads or writes a range whose object moved to native storage any more, and to
// measure what a static scan of the sources cannot see (addresses computed at run time, a record's address passed as a
// parameter, data reached through pointers).
//
// Off (the variable unset) it costs nothing: ee_memory's inline fast paths are the same code as without it, and the slow
// path tests one flag. On, ee_memory turns its inline fast paths off (the same way a journal does: one owner of that switch,
// ee_memory.cpp's fast_path_update) so that every access through ee_ram_ptr / ee_read / ee_write reaches ee_ram_ptr_slow,
// which hands the accesses to the guard. The guard tests them against its ranges and then:
//   census[:<file>]  counts each access to a range by (range, read / write / pointer, calling function), never stopping. The
//                    counts live in a file mapped into memory (default bridge_census.bin in the current directory), so they
//                    are complete however the process ends; tools/bridge/guard_report.py symbolizes and sums them.
//   trap             the first access by code outside a BridgeScope prints the address, the range, the kind and a backtrace,
//                    and aborts.
// The ranges: RAC1_BRIDGE_RANGES=<file> or built-in sets by name (coll, ld, zones, tables, spr, all; comma-separated;
// default all). A range is given at Veldin's address and translated onto each level's copy of the object (ovl_xlate_data,
// again whenever the translation changes), or absolute (the scratchpad, the base ELF's data), or derived from the tables'
// pointers (the paths' points, the volume records ...): guard.cpp's header comment has the file's syntax.
//
// Not seen (said in docs/env-vars.md): accesses through a host pointer taken once (V(), M(), ee_get_ram_base() + a, the
// scratchpad's direct users world_cull_hand.h and moby_draw_hand.cpp) count once where the pointer is taken, or not at all;
// plugins' inline reads of api->ram; the EE interpreter (the oracle's original), which reaches memory through its own
// pointer helper.
#ifndef RAC1_GAME_BRIDGE_GUARD_H
#define RAC1_GAME_BRIDGE_GUARD_H

#include <cstdint>

namespace rac1 {

// orig: (host) the kind of an access the guard sees: a read (ee_read), a write (ee_write), or a host pointer (ee_ram_ptr: V(),
// M(), a buffer; read or written through it)
enum class BridgeAccess : uint8_t { Read = 1, Write = 2, Pointer = 3 };

// orig: (host) RAC1_BRIDGE_GUARD read and the guard set up (once, from init_memory): true when it is on
bool bridge_guard_init();
// orig: (host) the guard is on
bool bridge_guard_on();
// orig: (host) an access of `size` bytes (0: through a pointer) at the resolved address `addr` (RAM below the RAM's size, the
// mirrors folded; the scratchpad at 0x70000000), by the code that called ee_ram_ptr_slow from `caller` (its return address;
// `cfa`: ee_ram_ptr_slow's own call frame address, __builtin_dwarf_cfa(), for the walk past the memory accessors). Called by
// ee_ram_ptr_slow only, and only while the guard is on.
void bridge_guard_access(uint32_t addr, uint32_t size, BridgeAccess kind, const void* caller, const void* cfa);

// orig: (host) while one is alive on a thread, that thread's accesses to guarded ranges are not trapped and are counted
// apart (the census's lower-case kinds): the bridge's mirror writers, which keep the EE bytes of a migrated object for the
// readers left, run inside one. Nests.
class BridgeScope {
public:
    BridgeScope();
    ~BridgeScope();
    BridgeScope(const BridgeScope&) = delete;
    BridgeScope& operator=(const BridgeScope&) = delete;
};

}  // namespace rac1

#endif
