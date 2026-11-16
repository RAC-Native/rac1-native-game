// Blarg's Nebula G34 (level 6) own moby classes, hand-ported: class 1348, an unseen moby that plays the help voices of a
// part of the station (sub_00203908 with the voice key and its help record) when Ratchet comes to its boxes or its mobys
// are dealt with, keeping the help records 0x141968.. (+0 a count, +2 the game's clock 0x15eea4 in 600ths, +4 the
// levels' bits). Header blarg_own.h; registered in blarg_mobys.cpp. Addresses are level 6's (`orig: L6:`); its own data
// is L6_ADDR, the base ELF's are bare.
#include <game/ovl/blarg_own.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO_POS = 0x0013f3d0;  // the hero's position (0x13f350 + 0x80)

// the help record `k` (8 bytes)
uint32_t help_record(uint32_t k) { return 0x00141968 + k * 8; }

// the moby of the level's moby table at index `i` (-1: the 256 bytes below the table's first)
uint32_t table_moby(int32_t i) { return lwu(L6_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// a moby gone: none, or in its state 0xfe / 0xfd
bool gone(uint32_t m) { return m == 0 || lbu(m + 0x20) == 0xfe || lbu(m + 0x20) == 0xfd; }

// the game's clock 0x15eea4 (frame-scaled) in 600ths
int32_t clock_600ths() { return ee_div(frame_scale_i(lw(0x0015eea4)), 0x258); }
}  // namespace

// Class 1348, the help voices. pvars: six mobys +0 .. +0x14 (-1: none), a group +0x18, the boxes +0x20 and +0x24, the
// mobys +0x28 and +0x2c. Each frame:
// - the voice 0x1773 (record 0x2a) while it was never heard and Ratchet is in the box +0x24;
// - while the voice 0x1772 (record 0x29) was never heard, it for each of the six mobys not in its state 1;
// - once the moby +0x28 is gone (until both records 0x2e and 0x2f count): with the Taunter (item 0xe) the voice 0x1777
//   (0x2e) and the record 0x2f counted, else the voice 0x1778 (0x2f) and the record 0x2e counted;
// - with Ratchet in the box +0x20 and the group's mobys all gone, for each of the six mobys still there and not in its
//   state 1: the first time the record 0x28 counted (no voice), then the voice 0x1771 (0x28) when the record's time is
//   0 or more than 18 seconds old (in 60 Hz frames), else the record's time brought up to the clock; each moby gone (and
//   the slot of a -1) sets the record's count to 0xffff;
// - the game's flag 0x13d3ad set once the moby +0x2c (if any) has its +0xbc set.
// orig: L6:0x003083b0
void blg_class1348_update_l6(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lhu(help_record(0x2a)) == 0 && point_in_box(HERO_POS, lw(pv + 0x24)) != 0) sub_00203908(0x1773, 0x2a);
    if (lhu(help_record(0x29)) == 0) {
        for (uint32_t i = 0; i < 6; ++i) {
            const int32_t idx = lw(pv + i * 4);
            if (idx == -1) continue;
            if (lbu(table_moby(idx) + 0x20) != 1) sub_00203908(0x1772, 0x29);
        }
    }
    if ((lhu(help_record(0x2e)) == 0 || lhu(help_record(0x2f)) == 0) && gone(table_moby(lw(pv + 0x28)))) {
        if (lbu(0x0013d4c0 + 0xe) != 0) {
            sub_00203908(0x1777, 0x2e);
            hero_stat_use(help_record(0x2f));
        } else {
            sub_00203908(0x1778, 0x2f);
            hero_stat_use(help_record(0x2e));
        }
    }
    if (point_in_box(HERO_POS, lw(pv + 0x20)) != 0 && moby_group_count_l1(lw(pv + 0x18), -1) == 0) {
        const uint32_t rec = help_record(0x28);
        for (uint32_t i = 0; i < 6; ++i) {
            const int32_t idx = lw(pv + i * 4);
            const uint32_t s = table_moby(idx);
            if (idx != -1 && !gone(s) && lbu(s + 0x20) != 1) {
                if (lhu(rec) == 0) {
                    hero_stat_use(rec);
                } else {
                    const int32_t since = static_cast<int32_t>(static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4))) -
                                                               lhu(rec + 2) * 0x258u);
                    const int32_t wait = float_to_int(fpu_mul(int_to_float(frame_scale_i(0x12)), 60.0f));
                    if (wait < since || lhu(rec + 2) * 0x258u == 0) {
                        sub_00203908(0x1771, 0x28);
                    } else if (static_cast<int32_t>(lhu(rec + 2)) < clock_600ths()) {
                        sh(rec + 2, static_cast<uint32_t>(clock_600ths()));
                    }
                }
            }
            if (gone(s)) sh(rec, 0xffff);
        }
    }
    if (lbu(0x0013d3ad) == 0) {
        const int32_t idx = lw(pv + 0x2c);
        if (idx != -1 && lbu(table_moby(idx) + 0xbc) != 0) sb(0x0013d3ad, 1);
    }
}

}  // namespace rac1
