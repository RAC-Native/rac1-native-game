#ifndef RAC1_RUNTIME_EE_MEMORY_H
#define RAC1_RUNTIME_EE_MEMORY_H

#include <runtime/kernel/ee_types.h>
#include <cstring>
#include <stdexcept>
#include <type_traits>
#include <vector>

namespace rac1 {

// --- SPR constants ---
inline constexpr u32 SPR_BASE = 0x70000000u;
inline constexpr size_t SPR_SIZE = 0x4000;

// SPR frame layout used by moby_build_visible_queue (orig: 0x00250768).
//
// Layout:
//   [0x0000 .. 0x01FF]  uint16_t class_heads[256]   per-class visible list head slot indices
//   [0x0200 .. 0x027D]  (unused in native port; next_visible lives in Moby+0xb4)
//   [0x027E .. 0x3FFF]  free
struct ScratchpadFrame {
    // class_heads[class_byte] = first slot index visible for that class.
    // MOBY_CHAIN_END (0xFFFF) means no visible Moby of that class.
    uint16_t class_heads[256];  // 512 bytes
    uint8_t  _pad[SPR_SIZE - 512];
};
static_assert(sizeof(ScratchpadFrame) == SPR_SIZE);

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
const void* ee_ram_ptr_const(u32 vaddr);

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

// The byte order of the emulated memories (EE RAM, the scratchpad; the VUs' memories are the same already): host-native
// 32-bit words. On a little-endian host (every PC) that is the PS2's own layout, byte for byte. On a big-endian host (the
// Wii U) a word, a float, a Vec4 / Mat4 (four-word groups) read and written in place are right as they are; a byte of
// the PS2's address a lives at a ^ 3, a halfword at a ^ 2, and a doubleword's low word comes first (at a) as on the PS2.
// Code that reaches EE memory through bytes (a byte or halfword pointer, a memcpy of a size not a multiple of four, a
// copy from a file) goes through the helpers below; ee_read / ee_write do it themselves.
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
#define RAC1_EE_WORDS_BE 1
inline constexpr u32 EE_BYTE_SWIZZLE = 3u;  // a byte's host offset: a ^ 3
inline constexpr u32 EE_HALF_SWIZZLE = 2u;  // a halfword's: a ^ 2
#else
inline constexpr u32 EE_BYTE_SWIZZLE = 0u;
inline constexpr u32 EE_HALF_SWIZZLE = 0u;
#endif
inline constexpr u32 ee_byte_addr(u32 a) { return a ^ EE_BYTE_SWIZZLE; }
inline constexpr u32 ee_half_addr(u32 a) { return a ^ EE_HALF_SWIZZLE; }

// A record read or written whole with ee_read / ee_write (more than eight bytes). On a big-endian host:
// - made of 32-bit fields alone (u32, s32, float: a quadword of floats, a VU vector), it is laid out like EE memory: declare
//   it RAC1_EE_WORDS_ONLY and it is copied as it is;
// - any other record is copied as the PS2's bytes and its fields put in the host's order by an le_fix(T&) found by ADL
//   (src/io/host_endian.h), the one a file's parser uses too. A record with neither does not compile there.
// On a little-endian host both are a plain copy.
template <typename T>
struct ee_words_only : std::false_type {};
#define RAC1_EE_WORDS_ONLY(T) \
    template <>               \
    struct ee_words_only<T> : std::true_type {}

// A quadword copied between two places of EE memory as it is (the same layout at both ends)
struct alignas(8) EeQuad {
    u32 w[4];
};
RAC1_EE_WORDS_ONLY(EeQuad);

namespace ee_detail {
// the host address of the access: a byte's or a halfword's moved within its word
template <typename T>
inline constexpr u32 swizzle(u32 vaddr) {
    if constexpr (sizeof(T) == 1) return ee_byte_addr(vaddr);
    else if constexpr (sizeof(T) == 2) return ee_half_addr(vaddr);
    else return vaddr;
}
}  // namespace ee_detail

// A reference to a scalar of EE memory (a counter a ported routine updates in place): a byte's or a halfword's host place
// is its swizzled address (above), a word's its own. Not for arrays: a pointer to bytes or halfwords cannot be indexed on a
// big-endian host.
template <typename T>
inline T& ee_ref(u32 vaddr) {
    static_assert(sizeof(T) <= 4, "a scalar of at most a word");
    return *static_cast<T*>(ee_ram_ptr(ee_detail::swizzle<T>(vaddr)));
}

// Bytes in the PS2's order (a file's, a disc sector's, a DMA packet's) to / from EE memory at vaddr: a memcpy on a
// little-endian host, each byte at its swizzled place on a big-endian one (whole words reversed at once).
void ee_copy_in(u32 vaddr, const void* src, size_t n);
void ee_copy_out(void* dst, u32 vaddr, size_t n);
// The same on a host buffer laid out as EE memory (the RAM or scratchpad block, a copy of it): `base` is address 0 of
// that layout's words (4-byte aligned on a big-endian host).
void ee_bytes_to_words(u8* base, size_t off, const void* src, size_t n);
void ee_words_to_bytes(void* dst, const u8* base, size_t off, size_t n);
// n bytes of EE memory set to zero (whole words in place, the edges' bytes at their swizzled places)
void ee_zero(u32 vaddr, size_t n);
// n bytes of EE memory at vaddr in the PS2's order, for code that reads them as bytes: the memory itself on a little-endian
// host; on a big-endian one a copy, in `tmp` (valid while `tmp` lives and is not changed)
const u8* ee_bytes_view(u32 vaddr, size_t n, std::vector<u8>& tmp);

// Safe templated memory read.
template <typename T>
T ee_read(u32 vaddr) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    const void* ptr = ee_ram_ptr_read(ee_detail::swizzle<T>(vaddr));
    if (!ptr) {
        ee_debug_backtrace();
        throw std::runtime_error("Attempted read from null pointer");
    }
    T val;
#if defined(RAC1_EE_WORDS_BE)
    if constexpr (sizeof(T) == 8) {  // (the low word at the lower address)
        u32 w[2];
        std::memcpy(w, ptr, 8);
        const uint64_t v = (static_cast<uint64_t>(w[1]) << 32) | w[0];
        std::memcpy(&val, &v, 8);
        return val;
    } else if constexpr (sizeof(T) > 4 && !ee_words_only<T>::value) {
        ee_copy_out(&val, vaddr, sizeof(T));
        le_fix(val);
        return val;
    }
#endif
    std::memcpy(&val, ptr, sizeof(T));
    return val;
}

// Safe templated memory write.
template <typename T>
void ee_write(u32 vaddr, const T& val) {
    static_assert(std::is_trivially_copyable_v<T>, "T must be trivially copyable");
    void* ptr = ee_ram_ptr_write(ee_detail::swizzle<T>(vaddr), static_cast<u32>(sizeof(T)));
    if (!ptr) {
        throw std::runtime_error("Attempted write to null pointer");
    }
#if defined(RAC1_EE_WORDS_BE)
    if constexpr (sizeof(T) == 8) {
        uint64_t v;
        std::memcpy(&v, &val, 8);
        const u32 w[2] = {static_cast<u32>(v), static_cast<u32>(v >> 32)};
        std::memcpy(ptr, w, 8);
        return;
    } else if constexpr (sizeof(T) > 4 && !ee_words_only<T>::value) {
        T le = val;
        le_fix(le);  // (the same swap both ways)
        ee_copy_in(vaddr, &le, sizeof(T));
        return;
    }
#endif
    std::memcpy(ptr, &val, sizeof(T));
}



// Get the base pointer of the 32MB RAM block (useful for debug inspectors/tools).
u8* ee_get_ram_base();

// Get the base pointer of the 16KB Scratchpad block.
u8* ee_get_scratchpad_base();

// Return pointer to the ScratchpadFrame overlay on the scratchpad.
inline ScratchpadFrame* spr_frame() {
    return reinterpret_cast<ScratchpadFrame*>(ee_get_scratchpad_base());
}

} // namespace rac1

#endif // RAC1_RUNTIME_EE_MEMORY_H
