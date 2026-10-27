#ifndef RAC1_RUNTIME_EE_MEMORY_H
#define RAC1_RUNTIME_EE_MEMORY_H

#include <runtime/kernel/ee_types.h>
#include <cstring>
#include <stdexcept>
#include <type_traits>

namespace rac1 {

// --- SPR constants ---
inline constexpr u32 SPR_BASE = 0x70000000u;
inline constexpr size_t SPR_SIZE = 0x4000;

// Initialize the virtual memory buffers (RAM and scratchpad).
void init_memory();

// Free virtual memory resources.
void shutdown_memory();

// Resolve a 32-bit virtual address to a host pointer.
// Returns nullptr if address is null, and throws if address is out of range.
// (the common cases - RAM below 64 MB outside a journal, the scratchpad - inline; every other case, the same
// rules as before, in ee_ram_ptr_slow)
namespace ee_detail {
extern u8* g_ram;             // the RAM block (nullptr before init_memory)
extern u8* g_spr;             // the scratchpad
extern u32 g_fast_last;       // the last RAM address reached inline: 64 MB - 1, or 0 (no inline RAM: none yet, a journal on)
extern u32 g_read_last;       // the same for reads (ee_read): a read is never journaled, so 64 MB - 1 once the RAM exists
extern const u8* g_jseen;     // while a journal is on: per 4 KB page, saved (one more entry, always set, past the last page)
}  // namespace ee_detail
// (`journal` false: an access that only reads, not saved by a journal; `size` != 0: an access of that many bytes, else one
// through a pointer that may run on into the next page)
void* ee_ram_ptr_slow(u32 vaddr, bool journal = true, u32 size = 0);
inline void* ee_ram_ptr(u32 vaddr) {
    if (vaddr - 1u < ee_detail::g_fast_last) return ee_detail::g_ram + vaddr;  // (1 .. 64 MB - 1)
    if (vaddr - SPR_BASE < static_cast<u32>(SPR_SIZE) && ee_detail::g_spr != nullptr) return ee_detail::g_spr + (vaddr - SPR_BASE);
    if (ee_detail::g_jseen != nullptr && vaddr - 1u < ee_detail::g_read_last) {  // (a journal on: its page and the next saved already)
        const u32 p = vaddr >> 12;
        if ((ee_detail::g_jseen[p] & ee_detail::g_jseen[p + 1]) != 0) return ee_detail::g_ram + vaddr;
    }
    return ee_ram_ptr_slow(vaddr);
}
// A pointer for a write of `size` bytes (ee_write): the journal saves the pages those bytes are on
inline void* ee_ram_ptr_write(u32 vaddr, u32 size) {
    if (vaddr - 1u < ee_detail::g_fast_last) return ee_detail::g_ram + vaddr;
    if (vaddr - SPR_BASE < static_cast<u32>(SPR_SIZE) && ee_detail::g_spr != nullptr) return ee_detail::g_spr + (vaddr - SPR_BASE);
    if (ee_detail::g_jseen != nullptr && vaddr - 1u < ee_detail::g_read_last) {
        if ((ee_detail::g_jseen[vaddr >> 12] & ee_detail::g_jseen[(vaddr + size - 1) >> 12]) != 0) return ee_detail::g_ram + vaddr;
    }
    return ee_ram_ptr_slow(vaddr, true, size);
}
// A pointer for a read only (ee_read): the same as ee_ram_ptr, but the journal does not save the page (reads
// change nothing to put back).
inline const void* ee_ram_ptr_read(u32 vaddr) {
    if (vaddr - 1u < ee_detail::g_read_last) return ee_detail::g_ram + vaddr;
    if (vaddr - SPR_BASE < static_cast<u32>(SPR_SIZE) && ee_detail::g_spr != nullptr) return ee_detail::g_spr + (vaddr - SPR_BASE);
    return ee_ram_ptr_slow(vaddr, false);
}

// On the EE address 0 is ordinary (kernel) memory: the original code reads it through null class
// slots and the like. The host treats 0 as a null pointer unless this is set (RAC1_REAL_HERO).
extern bool g_ee_null_is_ram;
// oracle tests only: an unmapped address resolves to a zeroed scratch (a read gives 0, a write is lost), as in the EE interpreter
extern bool g_ee_oob_is_zero;

// A journal of EE memory (the host runs ported code a second time for the renderer and puts the memory back): from
// begin, every RAM page reached through ee_ram_ptr is saved at its first access, the scratchpad whole; restore writes them all
// back (returns the number of pages). Pointers taken before begin (ee_get_ram_base ...) are not journaled: the code run in
// between must reach memory only through ee_ram_ptr (ee_read / ee_write and the views built on it). Single-threaded, not nested.
void ee_journal_begin();
size_t ee_journal_restore();
bool ee_journal_active();

// Debugging: with EE_BT set, prints a backtrace (where a bad EE address came from).
void ee_debug_backtrace();

// Check if a virtual address range is valid.
bool ee_is_valid_addr(u32 vaddr, u32 size);

// Safe templated memory read.
template <typename T>
T ee_read(u32 vaddr) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    const void* ptr = ee_ram_ptr_read(vaddr);
    if (!ptr) {
        ee_debug_backtrace();
        throw std::runtime_error("Attempted read from null pointer");
    }
    T val;
    std::memcpy(&val, ptr, sizeof(T));
    return val;
}

// Safe templated memory write.
template <typename T>
void ee_write(u32 vaddr, const T& val) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    void* ptr = ee_ram_ptr_write(vaddr, static_cast<u32>(sizeof(T)));
    if (!ptr) {
        throw std::runtime_error("Attempted write to null pointer");
    }
    std::memcpy(ptr, &val, sizeof(T));
}

// Get the base pointer of the RAM block (64 MB: the 32 MB of the PS2 and the host area above it) (useful for debug inspectors/tools).
u8* ee_get_ram_base();

// Get the base pointer of the 16KB Scratchpad block.
u8* ee_get_scratchpad_base();

} // namespace rac1

#endif // RAC1_RUNTIME_EE_MEMORY_H
