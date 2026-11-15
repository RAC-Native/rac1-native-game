// Gaspar's own moby class 1000, the level's controller — hand-ported from Gaspar's own overlay. Header gaspar_mobys.h,
// registered in gaspar_mobys.cpp. Addresses are Gaspar's (`orig: L9:`); Gaspar-only data is L9_ADDR, the base ELF's are bare
// (the story bytes 0x13d388.., the game's counters 0x141968.., the play time 0x15eea4, the level number 0x15ed84, the hero
// 0x13f3d0).
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l9.h>

namespace rac1 {

// Class 1000. Once a frame has run (the frame counter past 0): when no piece of the structures (classes 1182 .. 1189, which
// count themselves in 0x1613c8 each frame) was left and the story byte 0x13d416 not yet set, the story byte, the jingle and
// the message 0x53d6; the count back to 0. Then, unless the counter 0x141c40 runs: it starts with the story byte 0x13d4cb; the
// level's time record (0x141c42, in units of 600 frames) raised to the play time, the level's bit (and the top bit) in the
// visited levels 0x141c44, and the hero inside its box (pvars +0) starts the dialogue 0x2329.
// orig: L9:0x00300888
void gas_class1000_update_l9(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lw(L9_ADDR(0x0015f5cc)) > 0) {
        if (lw(L9_ADDR(0x001613c8)) == 0 && lbu(0x0013d416) == 0) {
            sb(0x0013d416, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
        sw(L9_ADDR(0x001613c8), 0);
    }
    const uint32_t c = 0x00141968;
    const uint32_t counter = lhu(0x00141c40);
    if (counter != 0) return;
    if (lbu(0x0013d4cb) != 0) sh(0x00141c40, counter + 1);
    const int32_t units = ee_div(frame_scale_i(lw(0x0015eea4)), 600);
    if (static_cast<int32_t>(lhu(c + 0x2da)) < units) sh(c + 0x2da, static_cast<uint32_t>(ee_div(frame_scale_i(lw(0x0015eea4)), 600)));
    sw(c + 0x2dc, lwu(c + 0x2dc) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
    if (point_in_box(0x0013f3d0, lw(pv)) != 0) sub_00203908(0x2329, 0x5b);
}

}  // namespace rac1
