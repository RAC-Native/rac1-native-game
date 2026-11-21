// Blarg's Nebula G34 (level 6): the level's own builds of two small engine functions, hand-ported (`orig: L6:`). Header
// blarg_own.h; level_sound_play_l6 is what sound.cpp's level_sound_play runs on Blarg, class_hook_pvars60_l6 the class
// hook carry_hooks_levels.cpp reaches.
#include <game/ovl/blarg_own.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>

namespace rac1 {

// The level's sound `idx` (from the level's first, 0x15f574; none past the count 0x15f5f0) started on the moby `moby`
// with the flags `flags`, its record from the level's sound table (0x15f5f4, 0x20 each) and the volume 0x400; the sound
// slot it took (0x13e550, 0x70 each) keeps the sound's number (+0x7e) and its moby (+0x88). The slot, -1 for none.
// orig: L6:0x0029aec0
int32_t level_sound_play_l6(int32_t idx, uint32_t flags, uint32_t moby) {
    const int32_t snd = idx + lw(L6_ADDR(0x0015f574));
    if (!(snd < lw(L6_ADDR(0x0015f5f0)))) return -1;
    const int32_t slot = sound_start(lwu(L6_ADDR(0x0015f5f4)) + (static_cast<uint32_t>(snd) << 5), flags, moby, 0, 0x400);
    if (slot >= 0) {
        const uint32_t e = 0x0013e550u + static_cast<uint32_t>(slot * 0x70);
        sh(e + 0x7e, static_cast<uint32_t>(snd));
        sw(e + 0x88, moby);
    }
    return slot;
}

// A class hook (+0x10 of the class table of level 6's class-827 copies): the moby's pvars + 0x60, 0 without a moby or
// pvars.
// orig: L6:0x002ea800
uint32_t class_hook_pvars60_l6(uint32_t m) {
    if (m == 0) return 0;
    const uint32_t pv = lwu(m + 0x78);
    return pv != 0 ? pv + 0x60 : 0;
}

}  // namespace rac1
