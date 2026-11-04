#pragma once

#include <cstddef>
#include <cstdint>
#include <span>
#include <string>

namespace rac1::setup {

// The discs the setup knows. To support another release, add its row with supported = true once the
// port runs on it (a release other than the NTSC-U one also needs its region table,
// data/regions/<serial>.txt); the rest of the setup reads this table.
struct KnownDisc {
    const char* boot_file;     // the boot executable in the disc's root directory, e.g. "SCUS_971.99"
    const char* serial;        // as printed on the case, e.g. "SCUS-97199"
    const char* region;        // "NTSC-U", "PAL", "NTSC-J"
    const char* version;       // SYSTEM.CNF's VER ("1.00"; the PAL disc 2.00 boots the same executable as 1.00)
    uint64_t redump_size;      // a good dump's size in bytes (0: unknown)
    bool supported;            // as the game's disc (the others only as data discs, setup.py --data-iso)
    const char* gives;         // what it gives as a data disc, in words ("" nothing; setup.py's GIVES): the Japanese voices,
                               // the corrected texts (those only beside the North American disc)
};
// The release by its boot executable and SYSTEM.CNF's version; without a version, the first release with that boot
// executable (its serial and region are the same for each). Null when it is not one the setup knows.
const KnownDisc* find_known_disc(const std::string& boot_file, const std::string& version = "");

struct IsoValidation {
    bool valid = false;
    const KnownDisc* disc = nullptr;  // the release found (also set when it is not supported)
    std::string serial;               // the boot executable found in the root directory, if any
    uint64_t size = 0;
    std::string message;              // why it is not valid, or a warning when it is
};

IsoValidation validate_iso(const std::string& iso_path);
// the disc's boot executable (SCUS_971.99 on the NTSC-U disc, SCES_509.16 on the PAL one)
bool extract_boot_elf(std::span<const std::byte> iso, const std::string& out_path, bool force = false,
                      const std::string& boot_file = "SCUS_971.99");

} // namespace rac1::setup
