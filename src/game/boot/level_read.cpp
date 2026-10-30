// Ported base ELF code: the disc's table of contents and a level's files read into EE memory
// before the level's overlay runs. The reads themselves go through the host's disc (the CDVD
// driver: 0x121450 / 0x216728 and their waits).
#include <game/boot/level_read.h>

#include <game/lib/snd989.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>

#include <vector>

namespace rac1 {

namespace {
uint32_t rd(uint32_t a) { return ee_read<uint32_t>(a); }  // orig: (helper) lw
void wr(uint32_t a, uint32_t v) { ee_write<uint32_t>(a, v); }  // orig: (helper) sw
uint32_t sectors_bytes(uint32_t n) { return (n * 0x800 + 0xfff) & 0xfffff000u; }  // orig: (helper) a file's 4 KiB-rounded size
}  // namespace

// The table of contents (6 sectors at LBA 1500) read, its first 0x2960 bytes to 0x137b80.
// orig: 0x0012f2b8
bool toc_read() {
    std::vector<uint8_t> buf(6 * 2048);
    if (!disc_read(1500, 6, buf.data())) return false;
    for (uint32_t i = 0; i < 0x2960; ++i) ee_write<uint8_t>(0x00137b80 + i, buf[i]);
    return true;
}

// The level's header (5 sectors at the LBA in the table of contents 0x13a448[level * 2]), its
// first 0x2434 bytes to 0x13a4e0: the sector ranges of the level's data (0x13a4e8), gameplay
// NTSC (0x13a4f0) / PAL (0x13a4f8), occlusion, ...
// orig: 0x0012f368
bool level_header_read(int32_t level) {
    std::vector<uint8_t> buf(5 * 2048);
    if (!disc_read(rd(0x0013a448 + static_cast<uint32_t>(level) * 8), 5, buf.data())) return false;
    for (uint32_t i = 0; i < 0x2434; ++i) ee_write<uint8_t>(0x0013a4e0 + i, buf[i]);
    return true;
}

// The level's three files below 0x1ff8000, each 4 KiB aligned: the gameplay data (NTSC or PAL)
// at the top (0x15ee50), the armor (table of contents 0x138e48[(armor + 1) * 2], armor 0x13e056)
// below it (0x15ee54), the level data below that (0x15ee4c). The loader's steps 0..2.
// orig: 0x00204428 (steps 0, 1, 2)
bool level_files_read() {
    const bool pal = ee_read<int32_t>(0x0015ed80) != 0;
    const uint32_t armor = static_cast<uint32_t>(static_cast<int32_t>(ee_read<int16_t>(0x0013e056)) + 1);
    const uint32_t gp_lba = rd(pal ? 0x0013a4f8 : 0x0013a4f0), gp_n = rd(pal ? 0x0013a4fc : 0x0013a4f4);
    const uint32_t ar_lba = rd(0x00138e48 + armor * 8), ar_n = rd(0x00138e4c + armor * 8);
    const uint32_t dt_lba = rd(0x0013a4e8), dt_n = rd(0x0013a4ec);
    const uint32_t gp = 0x01ff8000u - sectors_bytes(gp_n);
    const uint32_t ar = gp - sectors_bytes(ar_n);
    const uint32_t dt = ar - sectors_bytes(dt_n);
    wr(0x0015ee54, ar);
    wr(0x0015ee50, gp);
    wr(0x0015ee4c, dt);
    return disc_read_ee(dt, dt_lba, dt_n) && disc_read_ee(ar, ar_lba, ar_n) && disc_read_ee(gp, gp_lba, gp_n);
}

// The reply of a load stored at `data` (the level's bank handle).
// orig: 0x0022dd78
void snd_reply_store(int32_t reply, uint64_t data) {
    const uint32_t p = static_cast<uint32_t>(data);
    if (p != 0) wr(p, static_cast<uint32_t>(reply));
}

// The loader's sound steps (step 0x15ee48): 3 every sound stopped, then the previous level's
// bank unloaded (4, 5) if there was one; 6 the level's bank (in the level data at its offset +8)
// loaded into 0x15ed5c, 7 waiting for it. 1 once the bank is in.
// orig: 0x00204428 (steps 3..7; the host's disc reads are done, sceCdSync / 0x121630 idle)
int32_t level_bank_step() {
    const uint16_t step = ee_read<uint16_t>(0x0015ee48);
    switch (step) {
    case 3:
        if (ee_read<int16_t>(0x0015ee4a) != 0) return 0;
        snd_cmd_18();
        if (rd(0x0015ed5c) == 0) {
            ee_write<uint16_t>(0x0015ee48, 6);
            return 0;
        }
        break;
    case 4:
        if (snd_flush() != 0) return 0;
        snd_cmd_06(rd(0x0015ed5c));
        wr(0x0015ed5c, 0);
        break;
    case 5:
        if (snd_flush() != 0) return 0;
        snd_cmd_08();
        break;
    case 6:
        if (snd_flush() != 0) return 0;
        wr(0x0015ed5c, 0xffffffffu);
        snd_bank_load_from_ee_cb(rd(rd(0x0015ee4c) + 8) + rd(0x0015ee4c), 0x0022dd78, 0x0015ed5c);
        break;
    case 7:
        if (snd_flush() != 0) return 0;
        if (rd(0x0015ed5c) != 0xffffffffu) {
            snd_cmd_08();
            return 1;
        }
        return 0;
    default:
        return 0;
    }
    ee_write<uint16_t>(0x0015ee48, static_cast<uint16_t>(ee_read<uint16_t>(0x0015ee48) + 1));
    return 0;
}

}  // namespace rac1
