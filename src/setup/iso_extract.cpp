#include "iso_extract.h"

#include <io/mapped_file.h>
#include <platform/platform.h>

#include <algorithm>
#include <array>
#include <cstdio>
#include <cstring>
#include <filesystem>
#include <iostream>
#include <map>
#include <vector>

namespace rac1::setup {

namespace {

constexpr size_t SECTOR = 2048;

const std::map<int, std::string> LEVEL_DIRS = {
    {0, "0_veldin_tutorial"}, {1, "1_novalis"}, {2, "2_aridia"}, {3, "3_kerwan"},
    {4, "4_eudora"}, {5, "5_rilgar"}, {6, "6_nebula_g34"}, {7, "7_umbris"},
    {8, "8_batalia"}, {9, "9_gaspar"}, {10, "10_orxon"}, {11, "11_pokitaru"},
    {12, "12_hoven"}, {13, "13_oltanis_orbit"}, {14, "14_oltanis"}, {15, "15_quartu"},
    {16, "16_kalebo_iii"}, {17, "17_veldin_orbit"}, {18, "18_veldin_finale"}
};

bool iso9660_find_file(std::span<const std::byte> iso, const std::string& name, uint32_t& out_lba, uint32_t& out_size) {
    if (iso.size() < 17 * SECTOR) return false;
    const uint8_t* pvd = reinterpret_cast<const uint8_t*>(iso.data() + 16 * SECTOR);
    if (std::memcmp(pvd + 1, "CD001", 5) != 0) return false;

    const uint8_t* root = pvd + 156;
    uint32_t root_lba = 0, root_size = 0;
    std::memcpy(&root_lba, root + 2, 4);
    std::memcpy(&root_size, root + 10, 4);

    if (static_cast<size_t>(root_lba) * SECTOR + root_size > iso.size()) return false;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(iso.data() + static_cast<size_t>(root_lba) * SECTOR);

    std::string want = name;
    for (char& c : want) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));

    size_t off = 0;
    while (off < root_size) {
        uint8_t ln = data[off];
        if (ln == 0) {
            off = (off / SECTOR + 1) * SECTOR;
            continue;
        }
        if (off + ln > root_size) break;
        if (ln < 34) {  // (shorter than a record with a name: its name length byte, rec[32], not in it)
            off += ln;
            continue;
        }
        const uint8_t* rec = data + off;
        uint32_t ext = 0, sz = 0;
        std::memcpy(&ext, rec + 2, 4);
        std::memcpy(&sz, rec + 10, 4);
        uint8_t name_len = rec[32];
        if (33 + name_len <= ln) {
            std::string nm(reinterpret_cast<const char*>(rec + 33), name_len);
            size_t semi = nm.find(';');
            if (semi != std::string::npos) nm = nm.substr(0, semi);
            if (nm == want) {
                out_lba = ext;
                out_size = sz;
                return true;
            }
        }
        off += ln;
    }
    return false;
}

std::vector<std::string> iso9660_list_root(std::span<const std::byte> iso) {
    std::vector<std::string> names;
    if (iso.size() < 17 * SECTOR) return names;
    const uint8_t* pvd = reinterpret_cast<const uint8_t*>(iso.data() + 16 * SECTOR);
    if (std::memcmp(pvd + 1, "CD001", 5) != 0) return names;

    const uint8_t* root = pvd + 156;
    uint32_t root_lba = 0, root_size = 0;
    std::memcpy(&root_lba, root + 2, 4);
    std::memcpy(&root_size, root + 10, 4);

    if (static_cast<size_t>(root_lba) * SECTOR + root_size > iso.size()) return names;
    const uint8_t* data = reinterpret_cast<const uint8_t*>(iso.data() + static_cast<size_t>(root_lba) * SECTOR);

    size_t off = 0;
    while (off < root_size) {
        uint8_t ln = data[off];
        if (ln == 0) {
            off = (off / SECTOR + 1) * SECTOR;
            continue;
        }
        if (off + ln > root_size) break;
        if (ln < 34) {  // (shorter than a record with a name: its name length byte, rec[32], not in it)
            off += ln;
            continue;
        }
        const uint8_t* rec = data + off;
        uint8_t name_len = rec[32];
        if (33 + name_len <= ln) {
            std::string nm(reinterpret_cast<const char*>(rec + 33), name_len);
            size_t semi = nm.find(';');
            if (semi != std::string::npos) nm = nm.substr(0, semi);
            names.push_back(nm);
        }
        off += ln;
    }
    return names;
}

// Ratchet & Clank's releases. The port's addresses are the North American build's; the European PAL disc
// is supported through its region table (data/regions/SCES-50916.txt; see find_known_disc).
constexpr KnownDisc KNOWN_DISCS[] = {
    {"SCUS_971.99", "SCUS-97199", "NTSC-U", 4214095872ULL, true},
    {"SCES_509.16", "SCES-50916", "PAL", 4213997568ULL, true},
    {"SCPS_150.55", "SCPS-15055", "NTSC-J", 0, false},
};

struct LevelRange {
    int id;
    std::array<std::pair<uint32_t, uint32_t>, 4> ranges; // (lba, sectors)
};

std::vector<LevelRange> read_level_headers(std::span<const std::byte> iso) {
    std::vector<LevelRange> result;
    if (iso.size() < (1500 + 1) * SECTOR) return result;

    const uint8_t* toc_ptr = reinterpret_cast<const uint8_t*>(iso.data() + 1500 * SECTOR);
    int32_t magic = 0, toc_size = 0;
    std::memcpy(&magic, toc_ptr, 4);
    std::memcpy(&toc_size, toc_ptr + 4, 4);
    if (magic != 1 || toc_size <= 0 || 1500 * SECTOR + static_cast<size_t>(toc_size) > iso.size()) {
        return result;
    }

    for (size_t ofs = 8; ofs + 8 <= static_cast<size_t>(toc_size); ofs += 8) {
        uint32_t lsn = 0;
        std::memcpy(&lsn, toc_ptr + ofs, 4);
        if (!lsn || (static_cast<size_t>(lsn) + 1) * SECTOR > iso.size()) continue;

        const uint8_t* h = reinterpret_cast<const uint8_t*>(iso.data() + static_cast<size_t>(lsn) * SECTOR);
        int32_t lid = 0, hsize = 0;
        std::memcpy(&lid, h, 4);
        std::memcpy(&hsize, h + 4, 4);
        if (hsize != 0x2434) continue;

        LevelRange lr;
        lr.id = lid;
        for (int k = 0; k < 4; ++k) {
            uint32_t o = 0, n = 0;
            std::memcpy(&o, h + 8 + 8 * k, 4);
            std::memcpy(&n, h + 12 + 8 * k, 4);
            lr.ranges[k] = {o, n};
        }
        result.push_back(lr);
    }
    return result;
}

} // namespace

const KnownDisc* find_known_disc(const std::string& boot_file) {
    for (const auto& d : KNOWN_DISCS)
        if (boot_file == d.boot_file) return &d;
    return nullptr;
}

IsoValidation validate_iso(const std::string& iso_path) {
    IsoValidation res;
    std::error_code ec;
    if (!std::filesystem::is_regular_file(iso_path, ec)) {
        res.message = "File not found: " + iso_path;
        return res;
    }
    res.size = std::filesystem::file_size(iso_path, ec);

    MappedFile mf(iso_path);
    if (!mf) {
        res.message = "Cannot open the file: " + iso_path;
        return res;
    }
    auto iso = mf.span();
    if (iso.size() < 17 * SECTOR ||
        std::memcmp(reinterpret_cast<const uint8_t*>(iso.data() + 16 * SECTOR) + 1, "CD001", 5) != 0) {
        res.message = "This is not a disc image (ISO 9660). Use a plain .iso dump, not a compressed one "
                      "(.chd, .cso, .gz) or a .bin/.cue pair.";
        return res;
    }

    for (const auto& fn : iso9660_list_root(iso)) {
        if (fn.size() == 11 && fn[4] == '_' && fn[8] == '.') {  // a PS2 boot executable, "XXXX_NNN.NN"
            res.serial = fn;
            if (find_known_disc(fn) != nullptr) break;
        }
    }
    res.disc = find_known_disc(res.serial);
    if (res.disc == nullptr) {
        res.message = res.serial.empty() ? "This is not a PlayStation 2 game disc (no boot executable found)."
                                         : "This is another game (" + res.serial + "), not Ratchet & Clank.";
        return res;
    }
    if (!res.disc->supported) {
        res.message = std::string("This is Ratchet & Clank's ") + res.disc->region + " disc (" + res.disc->serial +
                      "). Only the North American disc, SCUS-97199, and the European PAL disc, SCES-50916, are supported for now.";
        return res;
    }

    // a damaged or incomplete dump: the boot executable, the table of contents and every level must be inside the file
    uint32_t lba = 0, size = 0;
    const auto levels = read_level_headers(iso);
    int levels_ok = 0;
    for (const auto& lv : levels) {
        if (LEVEL_DIRS.count(lv.id) == 0) continue;
        bool in = true;
        for (const auto& r : lv.ranges)
            if ((static_cast<size_t>(r.first) + r.second) * SECTOR > iso.size()) in = false;
        if (in) ++levels_ok;
    }
    if (!iso9660_find_file(iso, res.serial, lba, size) || static_cast<size_t>(lba) * SECTOR + size > iso.size() ||
        levels_ok != static_cast<int>(LEVEL_DIRS.size())) {
        res.message = "The disc image is damaged or incomplete (" + std::to_string(res.size) + " bytes; a good dump is " +
                      std::to_string(res.disc->redump_size) + "). Dump the disc again.";
        return res;
    }

    res.valid = true;
    if (res.disc->redump_size != 0 && res.size != res.disc->redump_size) {
        res.message = "Its size (" + std::to_string(res.size) + " bytes) differs from a good dump's (" +
                      std::to_string(res.disc->redump_size) + "). It may still work; if the game misbehaves, dump the disc again.";
    }
    return res;
}

bool extract_boot_elf(std::span<const std::byte> iso, const std::string& out_path, bool force, const std::string& boot_file) {
    if (!force && std::filesystem::exists(out_path)) {
        return true;
    }
    uint32_t lba = 0, size = 0;
    if (!iso9660_find_file(iso, boot_file, lba, size)) {
        std::cerr << "[setup] " << boot_file << " not found in ISO\n";
        return false;
    }
    if (static_cast<size_t>(lba) * SECTOR + size > iso.size()) {
        std::cerr << "[setup] " << boot_file << " sector range out of bounds\n";
        return false;
    }
    std::filesystem::path out(out_path);
    std::error_code ec;
    std::filesystem::create_directories(out.parent_path(), ec);
    // (a temporary file renamed over it: an interrupted setup never leaves a cut executable behind)
    return platform::write_file_replace(out, iso.data() + static_cast<size_t>(lba) * SECTOR, size);
}

} // namespace rac1::setup
