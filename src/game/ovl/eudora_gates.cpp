// Eudora's classes 1101, 1102, 1531 and 1532 (one update): gates that open by moving up or down for a second once their
// trigger is done: the moby whose index their pvars +0 hold, a Trespasser's pad (class 615) in its state 4 or a class
// 1190 in its state 2, or, from an earlier visit, their flag (their index +0xb2: the byte at L4:0x1bb3b0 + index + 0x454,
// or its bit in the level's words 0x14c190). Classes 1101 and 1532 rise 1.3 a frame-time unit (both with a sound as they
// start, 1101 also as it ends), 1531 sinks 2.55, 1102 2.05; done, their flag bits are set (the level's words 0x14c190 and
// L4:0x1ba650), so they stay open. Header eudora_mobys.h; registered in eudora_mobys.cpp. Addresses are Eudora's
// (`orig: L4:`); Eudora-only data is L4_ADDR, the base ELF's are bare.
#include <game/math/vecmath.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l4.h>

namespace rac1 {

namespace {
constexpr int32_t RISER = 0x44d;     // class 1101
constexpr int32_t SINKER = 0x5fb;    // class 1531
constexpr int32_t RISER_2 = 0x5fc;   // class 1532
constexpr int32_t PAD_CLASS = 0x267;  // class 615, the Trespasser's pad (solved: state 4)
constexpr int32_t SWITCH_CLASS = 0x4a6;  // class 1190 (done: state 2)

float dt() { return lwc1(0x0015ed6c); }  // the frame's time scale

// the level's moby of index `idx` (the moby table L4:0x15ffd8, 0x100 bytes each)
uint32_t moby_at(int32_t idx) { return lwu(L4_ADDR(0x0015ffd8)) + (static_cast<uint32_t>(idx) << 8); }

// the level's flag word holding the bit of the moby index `idx` (+0xb2): 32 words a level, the level number 0x15ed84
uint32_t level_flag_word(uint32_t idx) {
    return 0x0014c190 + (static_cast<uint32_t>(static_cast<int16_t>(idx) >> 5) << 2) + (lwu(0x0015ed84) << 8);
}

// whether the gate was opened before: its flag byte (L4:0x1bb3b0 + index + 0x454) or its bit of the level's words
bool opened_before(uint32_t m) {
    const uint32_t idx = lhu(m + 0xb2);
    if (lbu(L4_ADDR(0x001bb3b0) + static_cast<uint32_t>(static_cast<int16_t>(idx)) + 0x454) != 0) return true;
    return ((static_cast<int32_t>(lwu(level_flag_word(idx))) >> (idx & 0x1f)) & 1) != 0;
}

// whether the trigger (the moby of the index in pvars +0; -1 none) is done: a pad in state 4 or a class 1190 in state 2
bool trigger_done(uint32_t pv) {
    const int32_t link = lw(pv);
    if (link == -1) return false;
    const uint32_t t = moby_at(link);
    const int32_t cls = lh(t + 0xa6);
    return (cls == PAD_CLASS && lbu(t + 0x20) == 4) || (cls == SWITCH_CLASS && lbu(t + 0x20) == 2);
}
}  // namespace

// The gate's speed (a frame-time unit's move up) by its class: 1531 -2.55, 1101 and 1532 1.3, the others -2.05.
// orig: L4:0x002e1768
float gate_speed_l4(uint32_t m) {
    const int32_t cls = lh(m + 0xa6);
    if (cls == SINKER) return u2f(0xc0233333u);
    if (cls == RISER || cls == RISER_2) return u2f(0x3fa66666u);
    return u2f(0xc0033333u);
}

// pvars: the trigger's moby index +0 (-1: none), its height when set up +4, the move's timer +8. States: 0 set up,
// 1 shut (waiting for its trigger), 2 moving, 3 open.
// orig: L4:0x002e17d8
void eud_class1101_update_l4(uint32_t m) {
    ee_stack_alloc(0x40);  // (the original's frame; it keeps no locals there)
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0:  // (0x002e182c)
        swc1(pv + 4, lwc1(m + 0x18));
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x002e183c) shut: opened before, or its trigger done: moving for a second
        if (!opened_before(m) && !trigger_done(pv)) break;
        if (const int32_t cls = lh(m + 0xa6); cls == RISER || cls == RISER_2) moby_sound_play(0, 0, m, RISER);
        sb(m + 0x20, 2);
        sw(pv + 8, static_cast<uint32_t>(frame_scale_i(0x3c)));
        break;
    case 2: {  // (0x002e1930) moving; at the timer's end its flags set, open
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), fpu_mul(gate_speed_l4(m), dt())));
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 8))) == 0) break;
        const uint32_t idx = lhu(m + 0xb2);
        const uint32_t word = level_flag_word(idx);
        sw(word, lwu(word) | (1u << (idx & 0x1f)));
        const uint32_t idx2 = lhu(m + 0xb2);
        const uint32_t bits = L4_ADDR(0x001ba650) + (static_cast<uint32_t>(static_cast<int16_t>(idx2) >> 5) << 2);
        sw(bits, lwu(bits) | (1u << (idx2 & 0x1f)));
        if (lh(m + 0xa6) == RISER) moby_sound_start(1, 0, m);
        sb(m + 0x20, 3);
        break;
    }
    default:
        break;
    }
    ee_stack_free(0x40);
}

}  // namespace rac1
