#include <formats/ratchet_executable.h>

#include "io/host_endian.h"

#include <algorithm>
#include <cstring>
#include <stdexcept>

namespace rac1 {

namespace {

// ELF32 little-endian constants from the ELF specification, repeated here so that the file does not
// depend on <elf.h> being installed.
constexpr std::uint16_t ET_EXEC      = 2;
constexpr std::uint16_t EM_MIPS      = 8;
constexpr std::uint32_t PT_LOAD      = 1;
constexpr std::uint32_t PF_X = 1, PF_W = 2, PF_R = 4;
constexpr std::uint32_t SHT_PROGBITS = 1;
constexpr std::uint32_t SHT_STRTAB   = 3;
constexpr std::uint32_t SHF_WRITE = 1, SHF_ALLOC = 2, SHF_EXECINSTR = 4;

// e_flags of a PS2 Emotion Engine (R5900) ELF, the same as the base ELF's (readelf: noreorder,
// 5900, eabi64, mips3). Ghidra's Emotion Engine plugin detects the language from these flags.
//   0x20000000 EF_MIPS_ARCH_3, 0x00920000 EF_MIPS_MACH_5900, 0x00004000 EF_MIPS_ABI_EABI64,
//   0x00000001 EF_MIPS_NOREORDER
constexpr std::uint32_t EE_R5900_EFLAGS = 0x20924001;

struct Elf32Header {
    std::uint8_t  ident[16];
    std::uint16_t type, machine;
    std::uint32_t version, entry, phoff, shoff, flags;
    std::uint16_t ehsize, phentsize, phnum, shentsize, shnum, shstrndx;
};
struct Elf32Segment {
    std::uint32_t type, offset, vaddr, paddr, filesz, memsz, flags, align;
};
struct Elf32Section {
    std::uint32_t name, type, flags, addr, offset, size, link, info, addralign, entsize;
};
static_assert(sizeof(Elf32Header) == 52);
static_assert(sizeof(Elf32Segment) == 32);
static_assert(sizeof(Elf32Section) == 40);

template <class T>
void store(std::vector<std::uint8_t>& file, std::size_t at, const T& value) {
    std::memcpy(file.data() + at, &value, sizeof(T));
}

} // namespace

RatchetExecutable parse_ratchet_executable(std::span<const std::byte> data) {
    RatchetExecutable exe;
    std::size_t pos = 0;

    // Anything shorter than a header at the end is padding, not a block.
    while (data.size() - pos >= sizeof(OverlayBlockHeader)) {
        const int index = static_cast<int>(exe.sections.size());
        OverlayBlockHeader hdr;
        std::memcpy(&hdr, data.data() + pos, sizeof(hdr));
        le_fix_words32(hdr);  // (the PS2's bytes: io/host_endian.h)

        if (index == 0) {
            exe.entry_point = hdr.entry;
        } else if (hdr.entry != exe.entry_point) {
            break;  // the first block of something else
        }
        pos += sizeof(hdr);

        if (hdr.byte_count < 0) {
            throw std::runtime_error("Ratchet section " + std::to_string(index) + " has negative copy_size");
        }
        const std::size_t left = data.size() - pos;
        const std::size_t count = static_cast<std::size_t>(hdr.byte_count);
        if (count > left) {
            throw std::runtime_error("Ratchet section " + std::to_string(index)
                + " copy_size " + std::to_string(hdr.byte_count)
                + " overruns input buffer (remaining " + std::to_string(left) + ")");
        }

        const auto* first = reinterpret_cast<const std::uint8_t*>(data.data() + pos);
        exe.sections.push_back(RatchetSection{index, hdr, std::vector<std::uint8_t>(first, first + count)});
        pos += count;
    }
    return exe;
}

std::vector<std::uint8_t> reconstruct_elf(const RatchetExecutable& exe) {
    // File layout:
    //   ELF header | one program header per block | the blocks' bytes | .shstrtab | (pad to 4) |
    //   section headers: [0] null, [1..N] one per block, [N+1] .shstrtab
    const std::size_t n = exe.sections.size();

    // The section name table: a leading NUL, ".rac1_overlay_<i>" for each block, then ".shstrtab".
    std::vector<std::uint8_t> names{0};
    std::vector<std::uint32_t> name_at;
    for (const RatchetSection& s : exe.sections) {
        name_at.push_back(static_cast<std::uint32_t>(names.size()));
        const std::string nm = ".rac1_overlay_" + std::to_string(s.index);
        names.insert(names.end(), nm.begin(), nm.end());
        names.push_back(0);
    }
    const std::uint32_t shstrtab_name = static_cast<std::uint32_t>(names.size());
    for (char ch : std::string(".shstrtab")) names.push_back(static_cast<std::uint8_t>(ch));
    names.push_back(0);

    // Lay the file out first, then fill it in.
    std::vector<std::uint32_t> data_at;
    std::size_t cursor = sizeof(Elf32Header) + n * sizeof(Elf32Segment);
    for (const RatchetSection& s : exe.sections) {
        data_at.push_back(static_cast<std::uint32_t>(cursor));
        cursor += s.data.size();
    }
    const std::size_t names_at = cursor;
    cursor += names.size();
    cursor = (cursor + 3) & ~static_cast<std::size_t>(3);
    const std::size_t shdrs_at = cursor;
    cursor += (n + 2) * sizeof(Elf32Section);

    std::vector<std::uint8_t> file(cursor, 0);
    for (std::size_t i = 0; i < n; ++i) {
        std::copy(exe.sections[i].data.begin(), exe.sections[i].data.end(), file.begin() + data_at[i]);
    }
    std::copy(names.begin(), names.end(), file.begin() + names_at);

    Elf32Header eh{};
    const std::uint8_t magic[7] = {0x7f, 'E', 'L', 'F', 1 /* 32-bit */, 1 /* little-endian */, 1 /* current */};
    std::memcpy(eh.ident, magic, sizeof(magic));
    eh.type = ET_EXEC;
    eh.machine = EM_MIPS;
    eh.version = 1;
    eh.entry = static_cast<std::uint32_t>(exe.entry_point);
    eh.phoff = sizeof(Elf32Header);
    eh.shoff = static_cast<std::uint32_t>(shdrs_at);
    eh.flags = EE_R5900_EFLAGS;
    eh.ehsize = sizeof(Elf32Header);
    eh.phentsize = sizeof(Elf32Segment);
    eh.phnum = static_cast<std::uint16_t>(n);
    eh.shentsize = sizeof(Elf32Section);
    eh.shnum = static_cast<std::uint16_t>(n + 2);
    eh.shstrndx = static_cast<std::uint16_t>(n + 1);
    store(file, 0, eh);

    for (std::size_t i = 0; i < n; ++i) {
        const OverlayBlockHeader& h = exe.sections[i].header;

        // Every block is a loadable segment (the container stores the bytes of NOBITS blocks too,
        // so file size and memory size agree).
        Elf32Segment seg{};
        seg.type = PT_LOAD;
        seg.offset = data_at[i];
        seg.vaddr = seg.paddr = static_cast<std::uint32_t>(h.load_address);
        seg.filesz = seg.memsz = static_cast<std::uint32_t>(h.byte_count);
        seg.flags = PF_R | PF_W | PF_X;
        seg.align = 4;
        store(file, sizeof(Elf32Header) + i * sizeof(Elf32Segment), seg);

        Elf32Section sec{};
        sec.name = name_at[i];
        sec.type = static_cast<std::uint32_t>(h.elf_type);
        sec.flags = SHF_ALLOC | (sec.type == SHT_PROGBITS ? SHF_EXECINSTR : SHF_WRITE);
        sec.addr = static_cast<std::uint32_t>(h.load_address);
        sec.offset = data_at[i];
        sec.size = static_cast<std::uint32_t>(h.byte_count);
        sec.addralign = 4;
        store(file, shdrs_at + (i + 1) * sizeof(Elf32Section), sec);
    }

    Elf32Section strtab{};
    strtab.name = shstrtab_name;
    strtab.type = SHT_STRTAB;
    strtab.offset = static_cast<std::uint32_t>(names_at);
    strtab.size = static_cast<std::uint32_t>(names.size());
    strtab.addralign = 1;
    store(file, shdrs_at + (n + 1) * sizeof(Elf32Section), strtab);

    return file;
}

} // namespace rac1
