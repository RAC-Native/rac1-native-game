#include <runtime/kernel/ee_memory.h>
#include <runtime/kernel/ee_journal.h>
#include <runtime/kernel/disc.h>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <platform/platform.h>
#include <algorithm>
#include <vector>
#include <stdexcept>
#include <string>
#include <fstream>
#include <iostream>

namespace rac1 {

static std::vector<u8> g_scratchpad_buffer;

// The RAM: one reserved block, as large as a custom-level session may grow it (ee_session_ram_max), of which the first
// g_ram_size bytes are usable: 64 MB (the PS2's 32 MB and the host's areas above them, runtime/level/host_layout.h), more
// while a session's memory is on (ee_session_memory). Its pages cost memory only once touched (platform::memory_reserve).
constexpr u32 RAM_BASE_SIZE = 64 * 1024 * 1024;  // 64MB
static u8* g_ram_block = nullptr;
static size_t g_ram_reserved = 0;
static u32 RAM_SIZE = 0;  // (the usable size: 0 before init_memory)
constexpr u32 SCRATCHPAD_SIZE = 16 * 1024;  // 16KB
constexpr u32 SCRATCHPAD_START = 0x70000000;
constexpr u32 SCRATCHPAD_END = 0x70004000;

namespace ee_detail {
u8* g_ram = nullptr;
u8* g_spr = nullptr;
u32 g_fast_last = 0;
u32 g_read_last = 0;
const u8* g_jseen = nullptr;
u32 g_game_end = 0x02000000u;
u32 g_mirror_mask = 0x01FFFFFFu;
}  // namespace ee_detail
#if defined(RAC1_INLINE_HELPERS) && RAC1_INLINE_HELPERS
// (ee_read / ee_write's error for a null address: the same as their inline code without the switch)
void ee_read_null() {
    ee_debug_backtrace();
    throw std::runtime_error("Attempted read from null pointer");
}
void ee_write_null() { throw std::runtime_error("Attempted write to null pointer"); }
#endif
namespace {
// the inline path's view of the buffers, after each change of them. (Each word written only when it changes: a journal's begin and
// end change g_fast_last alone, on the game's thread, while the host's other threads - the drawing beside the game thread, the
// scene's worker - read memory through the others; ee_copy.h)
void fast_path_update(bool journal_on) {
    auto set = [](auto& word, auto value) {
        if (word != value) word = value;
    };
    set(ee_detail::g_ram, RAM_SIZE == 0 ? nullptr : g_ram_block);
    set(ee_detail::g_spr, g_scratchpad_buffer.empty() ? nullptr : g_scratchpad_buffer.data());
    set(ee_detail::g_fast_last, RAM_SIZE == 0 || journal_on ? 0u : RAM_SIZE - 1u);
    set(ee_detail::g_read_last, RAM_SIZE == 0 ? 0u : RAM_SIZE - 1u);
    const bool session = RAM_SIZE > RAM_BASE_SIZE;
    set(ee_detail::g_game_end, session ? RAM_SIZE : 0x02000000u);
    set(ee_detail::g_mirror_mask, session ? RAM_SIZE - 1u : 0x01FFFFFFu);
}
}  // namespace

// orig: (host) see ee_memory.h
u32 ee_session_ram_max() {
    static const u32 max = [] {
        // (testing: RAC1_SESSION_RAM_MB, a power of two from 64 to 512; 256 MB otherwise)
        u32 mb = 256;
        if (const char* e = std::getenv("RAC1_SESSION_RAM_MB"); e != nullptr && *e != 0) mb = static_cast<u32>(std::atoi(e));
        if (mb < 64 || mb > 512 || (mb & (mb - 1)) != 0) {
            std::fprintf(stderr, "[EE Memory] RAC1_SESSION_RAM_MB=%u is not a power of two from 64 to 512: 256 used\n", mb);
            mb = 256;
        }
        return mb << 20;
    }();
    return max;
}

// orig: (host) see ee_memory.h
u32 ee_ram_size() { return RAM_SIZE; }

void init_memory() {
    if (RAM_SIZE == 0) {
        g_ram_reserved = ee_session_ram_max();
        g_ram_block = static_cast<u8*>(platform::memory_reserve(g_ram_reserved));
        if (g_ram_block == nullptr || !platform::memory_commit(g_ram_block, RAM_BASE_SIZE))
            throw std::runtime_error("EE memory: the address space could not be reserved");
        RAM_SIZE = RAM_BASE_SIZE;
        
        // Load the base ELF PT_LOAD segment if available
        // the disc's own boot ELF (SCUS_971.99, or the PAL disc's SCES_509.16), its PT_LOAD as its program
        // header gives it (SCUS_971.99: offset 0x1000, 0x13d2e0 bytes at 0x100080)
        const std::string elf_path = disc_boot_elf_path();
        std::ifstream file(elf_path, std::ios::binary);
        if (file.is_open()) {
            u32 ph[8] = {};
            u32 phoff = 0;
            file.seekg(0x1c, std::ios::beg);
            file.read(reinterpret_cast<char*>(&phoff), 4);
            file.seekg(phoff, std::ios::beg);
            file.read(reinterpret_cast<char*>(ph), sizeof ph);
            file.seekg(ph[1], std::ios::beg);
            const u32 segment_vaddr = ph[2];
            const u32 segment_size = ph[4];
            if (segment_vaddr + segment_size <= RAM_SIZE) {
                file.read(reinterpret_cast<char*>(g_ram_block + segment_vaddr), segment_size);
                std::cout << "[EE Memory] Loaded base ELF segment (size: " << segment_size << " bytes) to 0x" << std::hex << segment_vaddr << std::dec << std::endl;
            }
        } else {
            std::cerr << "[EE Memory] WARNING: Could not open base ELF dump: " << elf_path << std::endl;
        }
    }
    if (g_scratchpad_buffer.empty()) {
        g_scratchpad_buffer.resize(SCRATCHPAD_SIZE, 0);
    }
    fast_path_update(false);
}

void shutdown_memory() {
    platform::memory_release(g_ram_block, g_ram_reserved);
    g_ram_block = nullptr;
    g_ram_reserved = 0;
    RAM_SIZE = 0;
    g_scratchpad_buffer.clear();
    g_scratchpad_buffer.shrink_to_fit();
    fast_path_update(false);
}

bool g_ee_null_is_ram = false;
bool g_ee_oob_is_zero = false;

// The journal: while on, the first access to each 4 KB page of RAM (and the page after it: an access through a
// pointer may run past the page's end) saves the page; the scratchpad is saved whole when it starts.
namespace {
constexpr u32 JPAGE = 4096;
bool g_journal_on = false;
std::vector<u8> g_journal_seen;    // per page: saved
std::vector<u32> g_journal_pages;  // the pages saved, in order
std::vector<u8> g_journal_data;    // their bytes (JPAGE each, in g_journal_pages' order)
std::vector<u8> g_journal_spr;
// (`size` != 0: the pages of [phys, phys + size) only; 0: the page and the next one)
void journal_touch(u32 phys, u32 size) {
    const u32 last = size != 0 ? (phys + size - 1) / JPAGE : phys / JPAGE + 1;
    for (u32 p = phys / JPAGE, e = std::min<u32>(last + 1, RAM_SIZE / JPAGE); p < e; ++p) {
        if (g_journal_seen[p] != 0) continue;
        g_journal_seen[p] = 1;
        g_journal_pages.push_back(p);
        const u8* src = g_ram_block + static_cast<size_t>(p) * JPAGE;
        // (host, RAC1_PERF_COPIES) the page appended as it is, not zero-filled first and copied over (=0: as before)
        static const bool append = std::getenv("RAC1_PERF_COPIES") == nullptr || std::atoi(std::getenv("RAC1_PERF_COPIES")) != 0;
        if (append) {
            g_journal_data.insert(g_journal_data.end(), src, src + JPAGE);
        } else {
            const size_t at = g_journal_data.size();
            g_journal_data.resize(at + JPAGE);
            std::memcpy(g_journal_data.data() + at, src, JPAGE);
        }
    }
}
}  // namespace

u32 g_unsaved_lo = 0, g_unsaved_hi = 0;  // (ee_journal_unsaved: pages marked saved for good, never copied)
void ee_journal_unsaved(u32 lo, u32 hi) {
    g_unsaved_lo = std::min<u32>(lo / JPAGE, RAM_SIZE / JPAGE);
    g_unsaved_hi = std::min<u32>(hi / JPAGE, RAM_SIZE / JPAGE);
    if (!g_journal_seen.empty())
        for (u32 p = 0; p < RAM_SIZE / JPAGE; ++p) g_journal_seen[p] = p >= g_unsaved_lo && p < g_unsaved_hi ? 1 : 0;
}

void ee_journal_begin() {
    if (g_journal_on || RAM_SIZE == 0) return;
    if (g_journal_seen.empty()) {
        g_journal_seen.assign(RAM_SIZE / JPAGE + 1, 0);
        g_journal_seen.back() = 1;  // (past the last page: "saved", for the inline check of a page and the next)
        for (u32 p = g_unsaved_lo; p < g_unsaved_hi; ++p) g_journal_seen[p] = 1;
    }
    g_journal_pages.clear();
    g_journal_data.clear();
    g_journal_spr = g_scratchpad_buffer;
    g_journal_on = true;
    fast_path_update(true);
    ee_detail::g_jseen = g_journal_seen.data();
}

size_t ee_journal_restore() {
    if (!g_journal_on) return 0;
    g_journal_on = false;
    ee_detail::g_jseen = nullptr;
    fast_path_update(false);
    for (size_t i = 0; i < g_journal_pages.size(); ++i) {
        const u32 p = g_journal_pages[i];
        std::memcpy(g_ram_block + static_cast<size_t>(p) * JPAGE, g_journal_data.data() + i * JPAGE, JPAGE);
        g_journal_seen[p] = 0;
    }
    std::memcpy(g_scratchpad_buffer.data(), g_journal_spr.data(), g_scratchpad_buffer.size());  // (same buffer: the inline path's pointer stays)
    const size_t n = g_journal_pages.size();
    g_journal_pages.clear();
    return n;
}

namespace {
// the runs of bytes where `a` (before) and `b` (after) differ, at EE address `base`
void byte_runs(const u8* a, const u8* b, size_t n, u32 base, std::vector<EeByteRun>& out) {
    for (size_t i = 0; i < n;) {
        if (a[i] == b[i]) {
            ++i;
            // the equal stretches skipped 8 bytes at a time (the same runs: a run still starts at its first differing byte)
            while (i + 8 <= n) {
                u64 x, y;
                std::memcpy(&x, a + i, 8);
                std::memcpy(&y, b + i, 8);
                if (x != y) break;
                i += 8;
            }
            continue;
        }
        size_t e = i;
        while (e < n && a[e] != b[e]) ++e;
        out.push_back({base + static_cast<u32>(i), std::vector<u8>(a + i, a + e), std::vector<u8>(b + i, b + e)});
        i = e;
    }
}
}  // namespace

size_t ee_journal_commit(std::vector<EeByteRun>& out) {
    if (!g_journal_on) return 0;
    g_journal_on = false;
    ee_detail::g_jseen = nullptr;
    fast_path_update(false);
    const size_t n0 = out.size();
    for (size_t i = 0; i < g_journal_pages.size(); ++i) {
        const u32 p = g_journal_pages[i];
        byte_runs(g_journal_data.data() + i * JPAGE, g_ram_block + static_cast<size_t>(p) * JPAGE, JPAGE, p * JPAGE, out);
        g_journal_seen[p] = 0;
    }
    byte_runs(g_journal_spr.data(), g_scratchpad_buffer.data(), g_scratchpad_buffer.size(), 0x70000000u, out);
    g_journal_pages.clear();
    return out.size() - n0;
}

bool ee_journal_active() { return g_journal_on; }

void* ee_ram_ptr_slow(u32 vaddr, bool journal, u32 size) {
    if (vaddr == 0 && !g_ee_null_is_ram) {
        return nullptr;
    }
    
    // RAM region
    if (vaddr < RAM_SIZE) {
        if (g_journal_on && journal) journal_touch(vaddr, size);
        return g_ram_block + vaddr;
    }
    if (RAM_SIZE == 0) throw std::runtime_error("Virtual memory not initialized. Call init_memory() first.");
    
    // Scratchpad region
    if (vaddr >= SCRATCHPAD_START && vaddr < SCRATCHPAD_END) {
        if (g_scratchpad_buffer.empty()) {
            throw std::runtime_error("Virtual memory not initialized. Call init_memory() first.");
        }
        return g_scratchpad_buffer.data() + (vaddr - SCRATCHPAD_START);
    }

    // kseg0 / kseg1 and the uncached RAM mirrors (the game runs in kernel mode: a stray pointer
    // such as 0x8xxxxxxx reads the 32 MB of RAM it mirrors, as on the EE and in the interpreter; while a session's
    // memory is on, the session's RAM: ee_mirror_phys)
    if ((vaddr >> 28) != 0x7) {
        const u32 phys = ee_mirror_phys(vaddr);
        if (phys != 0xffffffffu) {
            if (g_journal_on && journal) journal_touch(phys, size);
            return g_ram_block + phys;
        }
    }

    if (g_ee_oob_is_zero) {  // (the EE interpreter's way: an unmapped read gives 0, a write goes nowhere -- the oracle's random pvars hold floats where the code expects pointers)
        alignas(64) static u8 dummy[0x10000];
        std::memset(dummy + 0x7000, 0, 0x2000);
        return dummy + 0x8000;
    }
    ee_debug_backtrace();
    char hex[16];
    std::snprintf(hex, sizeof(hex), "%08x", vaddr);
    throw std::out_of_range(std::string("Pointer resolution out of bounds: 0x") + hex);
}

bool ee_is_valid_addr(u32 vaddr, u32 size) {
    if (vaddr == 0) {
        return false;
    }
    
    if (vaddr < RAM_SIZE) {
        return size <= RAM_SIZE - vaddr;  // (not vaddr + size: it wraps in 32 bits)
    }
    
    if (vaddr >= SCRATCHPAD_START && vaddr < SCRATCHPAD_END) {
        return size <= SCRATCHPAD_SIZE - (vaddr - SCRATCHPAD_START);
    }
    
    return false;
}

u8* ee_get_ram_base() {
    return RAM_SIZE == 0 ? nullptr : g_ram_block;
}

// orig: (host) see ee_memory.h
u32 ee_mirror_phys(u32 vaddr) {
    const u32 phys = vaddr & 0x1FFFFFFFu;
    if (RAM_SIZE == 0) return 0xffffffffu;
    if (RAM_SIZE <= RAM_BASE_SIZE) return phys < 0x10000000u ? phys & 0x01FFFFFFu : 0xffffffffu;  // (the PS2's 32 MB)
    return phys < std::max<u32>(0x10000000u, RAM_SIZE) ? phys & (RAM_SIZE - 1u) : 0xffffffffu;
}

// orig: (host) see ee_memory.h
bool ee_session_memory(u32 bytes) {
    if (bytes == 0) bytes = RAM_BASE_SIZE;
    if (RAM_SIZE == 0 || g_journal_on || bytes < RAM_BASE_SIZE || bytes > g_ram_reserved || (bytes & (bytes - 1)) != 0) return false;
    if (bytes == RAM_SIZE) return true;
    if (bytes > RAM_SIZE) {
        if (!platform::memory_commit(g_ram_block, bytes)) return false;
    } else {
        platform::memory_discard(g_ram_block, bytes, RAM_SIZE);  // (zeros when a session's memory is on again)
    }
    std::fprintf(stderr, "[EE Memory] %u MB (%s)\n", bytes >> 20, bytes > RAM_BASE_SIZE ? "a session's memory" : "as the original");
    RAM_SIZE = bytes;
    if (!g_journal_seen.empty()) {  // (the journal's page table: one entry a page, and the one past the last set)
        g_journal_seen.assign(RAM_SIZE / JPAGE + 1, 0);
        g_journal_seen.back() = 1;
        for (u32 p = g_unsaved_lo; p < g_unsaved_hi; ++p) g_journal_seen[p] = 1;
    }
    fast_path_update(false);
    return true;
}

u8* ee_get_scratchpad_base() {
    return g_scratchpad_buffer.empty() ? nullptr : g_scratchpad_buffer.data();
}

void ee_debug_backtrace() {
    if (std::getenv("EE_BT") != nullptr) platform::print_backtrace(32);
}

} // namespace rac1
