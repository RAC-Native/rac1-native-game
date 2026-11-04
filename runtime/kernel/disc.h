// The disc: the game's CD / DVD reads (the host stands in for the IOP's CDVD driver). Sectors are 2048 bytes.
// They are served from the disc's files the setup extracted into assets/disc/ (tools/disc_extract.py; its
// layout.txt maps every byte of the image to a file, or to zero), or, with RAC1_READ_ISO=1, from the image itself.
// Both give the same bytes for every sector.
#pragma once

#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// The image's path (the setup reads it; the game only with RAC1_READ_ISO=1): $RAC1_ISO, else the setup's link to
// the image (assets/iso/rac1-ntsc-u.iso, or assets/iso/rac1-pal.iso for the PAL disc; with both, the one the disc's
// files were extracted from), else the path written in disc_path_file() (where the setup could not make the link:
// Windows).
std::string disc_path();
// assets/iso/disc-path.txt: the image's path, one line, written by the setup when it cannot link it.
const char* disc_path_file();
// The disc's boot ELF as its SYSTEM.CNF names it (BOOT2 = cdrom0:\XXXX_NNN.NN): "SCUS_971.99" on the
// NTSC-U disc (SCUS-97199), "SCES_509.16" on the PAL one (SCES-50916); "SCUS_971.99" when the disc cannot be read.
// Read once, from the disc's ISO 9660 root directory.
const std::string& disc_boot_elf();
// "assets/dumps/" + disc_boot_elf(): the boot ELF setup.sh extracted from that disc.
std::string disc_boot_elf_path();
// The disc is not the NTSC-U build the ports are written against (the PAL disc): the ports' overlay
// addresses go through the region's maps on every level, Veldin's included (src/game/ovl/ovl_reloc.cpp).
bool disc_other_region();
// Where the reads come from: the extracted files (assets/disc/, the default) or the image (disc_path()).
enum class DiscSource { Files, Image };
// The folder the setup extracts the disc into: "assets/disc".
const char* disc_files_dir();
// The reads' source: the image with RAC1_READ_ISO=1 or after disc_open(), the files otherwise.
DiscSource disc_source();
// Whether the disc can be read from its source; when not, `why` tells the player what to do (run the setup again).
bool disc_ready(std::string& why);
// Opens the image (default disc_path()) and reads from it from now on (the setup's own steps: the image they were
// given). False if it cannot be read.
bool disc_open(const std::string& path = "");
bool disc_is_open();
// The size of the disc in bytes through `src` (0 when it cannot be read).
uint64_t disc_size(DiscSource src);
// Reads through `src` whatever the current source is (rac1-native --setup --compare-disc compares the two).
bool disc_read_from(DiscSource src, uint32_t lba, uint32_t sectors, void* dst);
// Reads `sectors` sectors from `lba` to the host buffer `dst`. False past the end of the disc.
bool disc_read(uint32_t lba, uint32_t sectors, void* dst);
// Reads to EE memory at `ee_dst`.
bool disc_read_ee(uint32_t ee_dst, uint32_t lba, uint32_t sectors);
// The host asks its OS to read these sectors ahead (into its page cache, in the background): a load about
// to read them (a level's files at the level exit's start). Nothing is read into the game's memory.
void disc_will_need(uint32_t lba, uint32_t sectors);
// Whether `level` (0..18) has a level header in the disc's table of contents (sector 1500).
bool disc_has_level(int level);
// Reads `level`'s sectors straight from the disc into `out`, with the 48-byte level header sector in front
// (data at +0x10/+0x14, gameplay NTSC at +0x18/+0x1c, PAL at +0x20/+0x24, occlusion at +0x28/+0x2c).
bool disc_read_level_wad(int level, std::vector<std::byte>& out);
// Reads only `level`'s data header and overlay executable from the disc into `out`.
bool disc_read_level_overlay(int level, std::vector<std::byte>& out);

// ---- A custom planet (local experiment, branch feat/custom-levels-experiment) ----
// A "shadow slot" (TEMPORARY: a true level slot 19 is to replace it, see src/host/custom_planet.h): the custom planet
// runs in level 0's slot (Veldin's overlay, index and sectors), and while it is on, level 0's data file and gameplay
// files (levels/level00/data.wad, gameplay_ntsc.bin, gameplay_pal.bin of the disc's files) are read from
// <dir>/data.wad and <dir>/gameplay.bin instead (each the size of the file it stands for). Off, level 0 is Veldin's.
// The host's per-level data for level 0 (lights, materials, water, ray tracing, MRP world assets) is Veldin's and is
// gated off with custom_level_active(). Not with RAC1_READ_ISO=1 (the image itself).
// The planet's folder ("" when none): RAC1_CUSTOM_LEVEL=<dir> (the developer's way: on for every load of level 0),
// else the first folder of assets/custom/ (RAC1_CUSTOM_PLANETS=<dir> instead) holding a planet.txt and a data.wad.
const std::string& custom_level_dir();
// Whether the folder came from RAC1_CUSTOM_LEVEL (then level 0 is always the custom planet).
bool custom_level_sticky();
// The custom planet on or off (src/host/custom_planet.h decides: the ship's list, the level loads, the saves).
void custom_level_set_on(bool on);
bool custom_level_on();
// Changes each time the planet goes on or off: the host's caches of level 0's assets compare it.
uint32_t custom_level_generation();
// Whether `level` is the custom planet (level 0 while it is on).
bool custom_level_active(int level);

// ---- Data discs ----
// Other releases' discs given to the setup with --data-iso (docs/setup.md, "Data discs"): sources of data only (another
// release's voices), never of code. The setup extracts each into its own folder (assets/disc-<serial>-<version>/, the
// same files and layout.txt as assets/disc/) and lists them in disc_data_discs_file(). The disc layer reads data disc k
// (0, 1, ...) at the sectors base + lba, base = (k + 1) * kDataDiscWindow: windows past the game's disc, which ends
// below the first (a DVD holds less than 0x400000 sectors), so a sector number names one disc's sector and the reads
// (disc_read, the IOP's streams, the movies) take a data disc's sectors as they take the game's.
constexpr uint32_t kDataDiscWindow = 0x00400000u;
struct DataDisc {
    std::string dir;                        // its files' folder ("assets/disc-SCPS-15037-1.02")
    std::string serial, region, version;    // "SCPS-15037", "NTSC-J", "1.02"
    std::string boot;                       // its boot executable ("SCPS_150.37")
    uint32_t base = 0;                      // its sector 0 in the disc layer's sectors
    bool ok = false;                        // its files are there and complete
};
// "assets/data-discs.txt": one line per data disc, "<folder> <serial> <region> <version> <boot>" (the setup writes it;
// missing: no data disc)
const char* disc_data_discs_file();
// The data discs listed (read once)
std::vector<DataDisc> disc_data_discs();
// The first readable data disc of `region` ("NTSC-J"), else null
const DataDisc* disc_data_disc(const std::string& region);

}  // namespace rac1
