// Orxon (level 10): the small pieces the level's own class code repeats (orxon_*.cpp). Engine data through OV() (the
// map pairs it with Veldin's), the level's own data through L10_ADDR(); base-ELF addresses bare.
#ifndef RAC1_GAME_OVL_ORXON_COMMON_H
#define RAC1_GAME_OVL_ORXON_COMMON_H

#include <cstdint>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l10.h>

namespace rac1::orxon {

constexpr uint32_t HERO = 0x0013f350;       // the hero (+0x80 his position, +0x20a4 his form: 1 Clank)
constexpr uint32_t HERO_POS = 0x0013f3d0;   // (HERO + 0x80)
constexpr uint32_t HERO_FORM = 0x001413f4;  // (HERO + 0x20a4: 0 Ratchet, 1 Clank, 2 Giant Clank)
constexpr uint32_t SOUND_SLOTS = 0x0013e550;  // the sound slots, 0x70 bytes each (+0x74 playing, +0x88 the owner, +0x90 the place)

// the frame's time scale and its square
inline float dt() { return lwc1(0x0015ed6c); }
inline float dt2() { return lwc1(0x0015ed70); }
// the level number
inline uint32_t level_number() { return lwu(0x0015ed84); }

// the moby at index `i` of the level's moby table
inline uint32_t moby_at(int32_t i) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(i) << 8); }

// the timers (1 idle, 0 running, 2 expired)
inline int32_t tick32(uint32_t a) { return timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(a))); }
inline int32_t tick16(uint32_t a) { return timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(a))); }

// the moby's byte of the level's row of the "gone for good" table (0x14c050 + the level x 16, at its +0xb0): 0xff gone
inline uint32_t gone_byte(uint32_t m) { return lbu(0x0014c050 + (level_number() << 4) + lbu(m + 0xb0)); }

// whether the moby's one-shot flag (its index +0xb2) is set: the level's byte (+0x454 of the table at Veldin 0x1bb230)
// or the bit of the level's words 0x14c190
inline bool flag_set(uint32_t m) {
    const int32_t idx = lh(m + 0xb2);
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(idx) + 0x454) != 0) return true;
    const uint32_t word = lwu(0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (level_number() << 8));
    return ((static_cast<int32_t>(word) >> (lhu(m + 0xb2) & 0x1f)) & 1) != 0;
}
// the moby's one-shot flag set: its bit in the level's words 0x14c190 and in the bits at Veldin 0x1ba4d0
inline void flag_mark(uint32_t m) {
    const int32_t idx = lh(m + 0xb2);
    const uint32_t bit = 1u << (lhu(m + 0xb2) & 0x1f);
    const uint32_t word = 0x0014c190 + (static_cast<uint32_t>(idx >> 5) << 2) + (level_number() << 8);
    sw(word, lwu(word) | bit);
    const uint32_t mine = OV(0x001ba4d0) + (static_cast<uint32_t>(idx >> 5) << 2);
    sw(mine, lwu(mine) | bit);
}

// the moby's sound in `slot` stopped while it still plays for the moby (-1: none)
inline void sound_stop_owned(uint32_t m, int32_t slot) {
    if (slot == -1) return;
    const uint32_t e = SOUND_SLOTS + static_cast<uint32_t>(slot) * 0x70;
    if (lwu(e + 0x88) == m && lbu(e + 0x74) != 0) sub_0028d918(slot);
}

}  // namespace rac1::orxon

#endif
