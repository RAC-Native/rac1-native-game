// See front_image.h.
#include <game/boot/front_image.h>

#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <io/host_endian.h>

#include <cstring>
#include <fstream>
#include <string>
#include <vector>

namespace rac1 {

// orig: (host) the ELF loader's part for the boot module's sections (the base ELF's PT_LOAD
// covers 0x100080..0x23d360; .bss is cleared here as the boot module's main clears it too)
bool front_image_load() {
    std::ifstream f(disc_boot_elf_path(), std::ios::binary);  // (the disc's own, SCES_509.16 on the PAL disc)
    if (!f) return false;
    std::vector<uint8_t> elf((std::istreambuf_iterator<char>(f)), std::istreambuf_iterator<char>());
    auto u16 = [&elf](size_t o) { return le16(&elf[o]); };
    auto u32 = [&elf](size_t o) { return le32(&elf[o]); };
    if (elf.size() < 0x34) return false;
    const uint32_t shoff = u32(0x20), shnum = u16(0x30), shstrndx = u16(0x32);
    if (shoff + shnum * 40u > elf.size() || shstrndx >= shnum) return false;
    const uint32_t stroff = u32(shoff + shstrndx * 40u + 16);
    const char* const want[] = {".lit", ".bss", ".data", "lvl.vtbl", "lvl.camvtbl", "lvl.sndvtbl", ".text"};
    int found = 0;
    for (uint32_t i = 0; i < shnum; ++i) {
        const size_t h = shoff + i * 40u;
        const std::string name(reinterpret_cast<const char*>(&elf[stroff + u32(h)]));
        bool wanted = false;
        for (const char* w : want) wanted = wanted || name == w;
        if (!wanted) continue;
        const uint32_t type = u32(h + 4), addr = u32(h + 12), off = u32(h + 16), size = u32(h + 20);
        if (!ee_is_valid_addr(addr, size)) return false;
        if (type == 8) {
            ee_zero(addr, size);
        } else {
            if (static_cast<size_t>(off) + size > elf.size()) return false;
            ee_copy_in(addr, &elf[off], size);
        }
        ++found;
    }
    return found == 7;
}

}  // namespace rac1
