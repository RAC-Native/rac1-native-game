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

static std::vector<u8> g_ram_buffer;
static std::vector<u8> g_scratchpad_buffer;

constexpr u32 RAM_SIZE = 64 * 1024 * 1024; // 64MB
constexpr u32 SCRATCHPAD_SIZE = 16 * 1024;  // 16KB
constexpr u32 SCRATCHPAD_START = 0x70000000;
constexpr u32 SCRATCHPAD_END = 0x70004000;

namespace ee_detail {
u8* g_ram = nullptr;
u8* g_spr = nullptr;
u32 g_fast_last = 0;
u32 g_read_last = 0;
const u8* g_jseen = nullptr;
}  // namespace ee_detail
namespace {
// the inline path's view of the buffers, after each change of them
void fast_path_update(bool journal_on) {
    ee_detail::g_ram = g_ram_buffer.empty() ? nullptr : g_ram_buffer.data();
    ee_detail::g_spr = g_scratchpad_buffer.empty() ? nullptr : g_scratchpad_buffer.data();
    ee_detail::g_fast_last = g_ram_buffer.empty() || journal_on ? 0u : RAM_SIZE - 1u;
    ee_detail::g_read_last = g_ram_buffer.empty() ? 0u : RAM_SIZE - 1u;
}
}  // namespace

void init_memory() {
    if (g_ram_buffer.empty()) {
        g_ram_buffer.resize(RAM_SIZE, 0);
        
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
                file.read(reinterpret_cast<char*>(g_ram_buffer.data() + segment_vaddr), segment_size);
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
    g_ram_buffer.clear();
    g_ram_buffer.shrink_to_fit();
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
        const size_t at = g_journal_data.size();
        g_journal_data.resize(at + JPAGE);
        std::memcpy(g_journal_data.data() + at, g_ram_buffer.data() + static_cast<size_t>(p) * JPAGE, JPAGE);
    }
}
}  // namespace

void ee_journal_begin() {
    if (g_journal_on || g_ram_buffer.empty()) return;
    if (g_journal_seen.empty()) {
        g_journal_seen.assign(RAM_SIZE / JPAGE + 1, 0);
        g_journal_seen.back() = 1;  // (past the last page: "saved", for the inline check of a page and the next)
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
        std::memcpy(g_ram_buffer.data() + static_cast<size_t>(p) * JPAGE, g_journal_data.data() + i * JPAGE, JPAGE);
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
        byte_runs(g_journal_data.data() + i * JPAGE, g_ram_buffer.data() + static_cast<size_t>(p) * JPAGE, JPAGE, p * JPAGE, out);
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
        if (g_ram_buffer.empty()) {
            throw std::runtime_error("Virtual memory not initialized. Call init_memory() first.");
        }
        if (g_journal_on && journal) journal_touch(vaddr, size);
        return g_ram_buffer.data() + vaddr;
    }
    
    // Scratchpad region
    if (vaddr >= SCRATCHPAD_START && vaddr < SCRATCHPAD_END) {
        if (g_scratchpad_buffer.empty()) {
            throw std::runtime_error("Virtual memory not initialized. Call init_memory() first.");
        }
        return g_scratchpad_buffer.data() + (vaddr - SCRATCHPAD_START);
    }

    // kseg0 / kseg1 and the uncached RAM mirrors (the game runs in kernel mode: a stray pointer
    // such as 0x8xxxxxxx reads the 32 MB of RAM it mirrors, as on the EE and in the interpreter)
    if ((vaddr >> 28) != 0x7) {
        const u32 phys = vaddr & 0x1FFFFFFFu;
        if (phys < 0x10000000u && !g_ram_buffer.empty()) {
            if (g_journal_on && journal) journal_touch(phys & 0x01FFFFFFu, size);
            return g_ram_buffer.data() + (phys & 0x01FFFFFFu);
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

const void* ee_ram_ptr_const(u32 vaddr) {
    return ee_ram_ptr(vaddr);
}

bool ee_is_valid_addr(u32 vaddr, u32 size) {
    if (vaddr == 0) {
        return false;
    }
    
    if (vaddr < RAM_SIZE) {
        return (vaddr + size <= RAM_SIZE);
    }
    
    if (vaddr >= SCRATCHPAD_START && vaddr < SCRATCHPAD_END) {
        return (vaddr - SCRATCHPAD_START + size <= SCRATCHPAD_SIZE);
    }
    
    return false;
}

u8* ee_get_ram_base() {
    return g_ram_buffer.empty() ? nullptr : g_ram_buffer.data();
}

u8* ee_get_scratchpad_base() {
    return g_scratchpad_buffer.empty() ? nullptr : g_scratchpad_buffer.data();
}

void ee_debug_backtrace() {
    if (std::getenv("EE_BT") != nullptr) platform::print_backtrace(32);
}

void ee_bytes_to_words(u8* base, size_t off, const void* src, size_t n) {
#if defined(RAC1_EE_WORDS_BE)
    const u8* s = static_cast<const u8*>(src);
    size_t i = 0;
    for (; i < n && ((off + i) & 3) != 0; i++) base[(off + i) ^ 3] = s[i];
    for (; i + 4 <= n; i += 4) {
        const u32 w = static_cast<u32>(s[i]) | static_cast<u32>(s[i + 1]) << 8 | static_cast<u32>(s[i + 2]) << 16 |
                      static_cast<u32>(s[i + 3]) << 24;
        std::memcpy(base + off + i, &w, 4);
    }
    for (; i < n; i++) base[(off + i) ^ 3] = s[i];
#else
    std::memcpy(base + off, src, n);
#endif
}

void ee_words_to_bytes(void* dst, const u8* base, size_t off, size_t n) {
#if defined(RAC1_EE_WORDS_BE)
    u8* d = static_cast<u8*>(dst);
    size_t i = 0;
    for (; i < n && ((off + i) & 3) != 0; i++) d[i] = base[(off + i) ^ 3];
    for (; i + 4 <= n; i += 4) {
        u32 w;
        std::memcpy(&w, base + off + i, 4);
        d[i] = static_cast<u8>(w);
        d[i + 1] = static_cast<u8>(w >> 8);
        d[i + 2] = static_cast<u8>(w >> 16);
        d[i + 3] = static_cast<u8>(w >> 24);
    }
    for (; i < n; i++) d[i] = base[(off + i) ^ 3];
#else
    std::memcpy(dst, base + off, n);
#endif
}

void ee_copy_in(u32 vaddr, const void* src, size_t n) {
    if (n == 0) return;
    u8* p = static_cast<u8*>(ee_ram_ptr_write(vaddr, static_cast<u32>(n)));
    if (p == nullptr) throw std::runtime_error("Attempted write to null pointer");
    ee_bytes_to_words(p - (vaddr & 3u), vaddr & 3u, src, n);
}

void ee_copy_out(void* dst, u32 vaddr, size_t n) {
    if (n == 0) return;
    const u8* p = static_cast<const u8*>(ee_ram_ptr_read(vaddr));
    if (p == nullptr) throw std::runtime_error("Attempted read from null pointer");
    ee_words_to_bytes(dst, p - (vaddr & 3u), vaddr & 3u, n);
}

} // namespace rac1
