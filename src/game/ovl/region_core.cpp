// See region_core.h.
#include <game/ovl/region_core.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/level/level_loader.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <io/host_endian.h>
#include <cstring>
#include <fstream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

// orig: (host) the table's core unit (region_core.txt): the runs NTSC-U -> the disc's, where each core ends, the core's
// own pointers into itself
struct CoreRuns {
    using Run = std::array<uint32_t, 3>;  // lo, hi, displacement
    std::vector<Run> code;                // NTSC-U functions lo..hi (end exclusive) at +d
    std::vector<Run> data;                // NTSC-U data lo..hi (both included) at +d: the core's data without gaps
    uint32_t end_n = 0x0015ef00u, end_p = 0x0015ef00u;  // the NTSC-U core's end, the disc's
    std::vector<uint32_t> ptr;                          // the core data's words that point into the core (the disc's addresses)
    // orig: (host) the disc's core address p as the NTSC-U build's (0: the disc's own code, or bytes it inserted)
    uint32_t back(uint32_t p) const {
        for (const Run& r : code)
            if (p - r[2] >= r[0] && p - r[2] < r[1]) return p - r[2];
        uint32_t found = 0;
        for (const Run& r : data)  // (two runs claiming p: the later one, the object it starts)
            if (p - r[2] >= r[0] && p - r[2] <= r[1]) found = p - r[2];
        return found;
    }
};

// orig: (host) a file's `ptr` lines, sorted (and its other lines to `other`)
template <class F>
std::vector<uint32_t> read_ptrs(const std::string& path, F other) {
    std::vector<uint32_t> ptr;
    std::ifstream in(path);
    std::string line;
    while (std::getline(in, line)) {
        std::istringstream s(line);
        std::string kind, d;
        uint32_t lo = 0, hi = 0;
        s >> kind >> std::hex >> lo >> hi >> d;
        if (kind == "ptr") ptr.push_back(lo);
        else other(kind, lo, hi, d);
    }
    std::sort(ptr.begin(), ptr.end());
    return ptr;
}

// orig: (host) read once: empty (no runs) on the NTSC-U disc, PAL 1.00, or without the file
const CoreRuns& core_runs() {
    static const CoreRuns runs = [] {
        CoreRuns c;
        if (!disc_other_region()) return c;
        c.ptr = read_ptrs("assets/generated/overlay_map/region_core.txt", [&c](const std::string& kind, uint32_t lo, uint32_t hi, const std::string& d) {
            const uint32_t disp = d.empty() ? 0 : static_cast<uint32_t>(std::stoll(d, nullptr, 16));
            if (kind == "code") c.code.push_back({lo, hi, disp});
            else if (kind == "data") c.data.push_back({lo, hi, disp});
            else if (kind == "end") {
                c.end_n = lo;
                c.end_p = hi;
            }
        });
        return c;
    }();
    return runs;
}

// orig: (host) one unit's `ptr` words (region_levelN.txt / region_boot.txt: the disc's addresses), read once
const std::vector<uint32_t>& unit_ptrs(int unit) {
    static std::map<int, std::vector<uint32_t>> all;
    const auto it = all.find(unit);
    if (it != all.end()) return it->second;
    const std::string path = unit == OVL_FRONT ? std::string("assets/generated/overlay_map/region_boot.txt")
                                               : "assets/generated/overlay_map/region_level" + std::to_string(unit) + ".txt";
    return all[unit] = read_ptrs(path, [](const std::string&, uint32_t, uint32_t, const std::string&) {});
}

// orig: (host) the listed words of [addr, addr + size) of `mem` (the image of the disc's addresses from `base`) taken
// back to the NTSC-U addresses of what they point to
void fix_words(uint8_t* mem, uint32_t base, uint32_t addr, uint32_t size, const std::vector<uint32_t>& ptr) {
    const CoreRuns& c = core_runs();
    for (auto it = std::lower_bound(ptr.begin(), ptr.end(), addr); it != ptr.end() && *it + 4 <= addr + size; ++it) {
        uint32_t v = 0;
        std::memcpy(&v, mem + (*it - base), 4);
        const uint32_t b = c.back(v);
        if (b != 0) std::memcpy(mem + (*it - base), &b, 4);
    }
}

// orig: (host) the boot module's data sections as the ELF's section headers give them (.lit, .data, the level tables)
std::vector<std::array<uint32_t, 2>> boot_data_sections() {
    std::vector<std::array<uint32_t, 2>> out;
    std::ifstream f(disc_boot_elf_path(), std::ios::binary);
    std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    if (elf.size() < 0x34) return out;
    auto u16 = [&elf](size_t o) { uint16_t v; std::memcpy(&v, &elf[o], 2); le_fix_field(v); return v; };  // (the ELF's little-endian fields)
    auto u32 = [&elf](size_t o) { uint32_t v; std::memcpy(&v, &elf[o], 4); le_fix_field(v); return v; };
    const uint32_t shoff = u32(0x20), shnum = u16(0x30), shstrndx = u16(0x32);
    if (shoff + shnum * 40u > elf.size() || shstrndx >= shnum) return out;
    const uint32_t stroff = u32(shoff + shstrndx * 40u + 16);
    for (uint32_t i = 0; i < shnum; ++i) {
        const size_t h = shoff + i * 40u;
        const std::string name(reinterpret_cast<const char*>(&elf[stroff + u32(h)]));
        if (u32(h + 4) == 1 && (name == ".lit" || name == ".data" || name == "lvl.vtbl" || name == "lvl.camvtbl" || name == "lvl.sndvtbl"))
            out.push_back({u32(h + 12), u32(h + 20)});
    }
    return out;
}

}  // namespace

// orig: (host) see region_core.h
bool region_core_moved() { return !core_runs().data.empty(); }

// orig: (host) see ovl_reloc.h (the 1.02 build's text ids: its core moved, region_core_moved; the PAL disc 2.00 is the
// only 1.02 disc the game runs from)
int32_t region_text_id(int32_t id) {
    if (id < 20695 || !region_core_moved()) return id;
    return id < 20742 ? id + 1 : id + 5;
}

// orig: (host) see region_core.h
void region_core_fix(int unit, uint32_t addr, uint32_t size) {
    if (!region_core_moved()) return;
    auto* p = static_cast<uint8_t*>(ee_ram_ptr(addr));
    if (p != nullptr) fix_words(p, addr, addr, size, unit_ptrs(unit));
}

// orig: (host) see region_core.h
void region_core_load() {
    const CoreRuns& c = core_runs();
    if (c.data.empty()) return;
    // the disc's core as the ELF loaded it, its data's pointers into itself taken back, then copied run by run to the
    // NTSC-U addresses (tools/overlay_match/region_maps.py's normalise_core, checked against the NTSC-U core: check-core)
    const uint32_t lo = 0x00100000u, hi = c.end_p;
    std::vector<uint8_t> src(hi - lo);
    auto* ram = static_cast<uint8_t*>(ee_ram_ptr(lo));
    std::memcpy(src.data(), ram, src.size());
    fix_words(src.data(), lo, lo, hi - lo, c.ptr);
    for (const auto& r : c.code) std::memcpy(ram + (r[0] - lo), &src[r[0] + r[2] - lo], r[1] - r[0]);
    for (const auto& r : c.data) {
        const uint32_t n = std::min<uint32_t>(r[1] + 1 - r[0], hi - (r[0] + r[2]));  // (the last run: up to the disc's core end)
        std::memcpy(ram + (r[0] - lo), &src[r[0] + r[2] - lo], n);
    }
    // what is left of the disc's core above the NTSC-U one's end: nothing (the boot module starts at end_p)
    std::memset(ram + (c.end_n - lo), 0, c.end_p - c.end_n);
    // The card's slot state (the save block 0x13d290 + 0x14): the NTSC-U build starts it at -3; the 1.02 build at -4, a
    // state of its own card machine (0x24b690's 1.02 version), whose NTSC-U port stands in.
    const uint32_t card_state = 0xfffffffdu;
    std::memcpy(ram + (0x0013d2a4u - lo), &card_state, 4);
    // the boot module's data (the ELF's sections above the core)
    for (const auto& s : boot_data_sections()) region_core_fix(OVL_FRONT, s[0], s[1]);
    // each level overlay's data as it is loaded
    g_overlay_section_callback = [](int level, uint32_t addr, uint32_t size) { region_core_fix(level < 0 ? 0 : level, addr, size); };
    std::printf("[region] the disc's core loaded at the NTSC-U build's addresses (%zu code runs, %zu data runs)\n", c.code.size(),
                c.data.size());
}

}  // namespace rac1
