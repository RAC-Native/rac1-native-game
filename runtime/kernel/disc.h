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
// the image (assets/iso/rac1-ntsc-u.iso, or assets/iso/rac1-pal.iso for the PAL disc), else the path written in
// disc_path_file() (where the setup could not make the link: Windows).
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

}  // namespace rac1
