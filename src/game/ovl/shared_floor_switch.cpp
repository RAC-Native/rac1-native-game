// Class 1209 of Quartu (level 15) and Drek's fleet (17): a floor switch of a group. One port of both levels' copies, the same
// code relinked: its addresses are the engine's (OV: the levels' switch bytes 0x1bb230 + 0x454, the flag words 0x1ba4d0)
// or the base ELF's (bare: the hero 0x13f3d0, the level number 0x15ed84, the per-level flag words 0x14c190); the group's
// state goes through moby_group_set_state (the shared port of the levels' copies). Header shared_floor_switch.h.
//
// The pvars: +0 the lead's time left (frames, i16), +2 the frames until its tick sounds again (i16), +4 lit (i16), +6 the
// frames until the done switch blinks again (i16), +8 the blink's phase, +9 led by another switch, +0xa the switches lit
// (the lead's), +0xb the switches of the group (the lead's), +0xc the time the group has (frames), +0x10 the lead, +0x14 the
// switch's radius, +0x18 the done state's time, +0x1c already done. The moby: its group +0x21, its flag index +0xb2 (s16).
// States: 0 start, 1 waiting, 2 the group running (the lead's), 3 back to waiting, 4 done.
#include <game/ovl/shared_floor_switch.h>

#include <game/math/vecmath.h>
#include <game/ovl/novalis_mobys2.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {
constexpr int32_t SWITCH_CLASS = 0x4b9;  // (1209)
constexpr uint32_t DONE = 4;             // (the group's state when all switches are lit)
constexpr uint32_t SOUND_FLAGS = 0x31;

int32_t flag_index(uint32_t m) { return static_cast<int16_t>(lhu(m + 0xb2)); }
uint32_t flag_bit(uint32_t m) { return 1u << (lhu(m + 0xb2) & 0x1f); }
// the moby's flag word in the level's flag words (0x14c190, 0x100 bytes a level) and in the words 0x1ba4d0
uint32_t level_flag_word(uint32_t m) {
    return 0x0014c190 + (static_cast<uint32_t>(flag_index(m) >> 5) << 2) + (lwu(0x0015ed84) << 8);
}
uint32_t flag_word(uint32_t m) { return OV(0x001ba4d0) + (static_cast<uint32_t>(flag_index(m) >> 5) << 2); }

// the switch done: already set in the switch bytes (0x1bb230 + 0x454) or the level's flag words, and marked done
bool already_done(uint32_t m, uint32_t pv) {
    if (lbu(OV(0x001bb230) + static_cast<uint32_t>(flag_index(m)) + 0x454) == 0 && ((lwu(level_flag_word(m)) >> (lhu(m + 0xb2) & 0x1f)) & 1u) == 0)
        return false;
    return lwu(pv + 0x1c) != 0;
}

void finish(uint32_t m, uint32_t pv) {
    sh(pv, lhu(pv + 0x18));
    sb(m + 0x20, DONE);
}
}  // namespace

// orig: L15:0x002e5958 L17:0x002e5f50
void floor_switch_update(uint32_t m) {
    const uint32_t sp = ee_stack_alloc(0xc0);
    const uint32_t rel = sp, rot = sp + 0x10, group = sp + 0x50;
    const uint32_t pv = lwu(m + 0x78);
    const uint32_t state = lbu(m + 0x20);
    // the hero on the switch: within its radius, and less than 0.2 along its local y
    bool stood_on = false;
    if (state == 1 || state == 2) {
        vec3_sub(V(rel), V(m + 0x10), V(0x0013f3d0));
        if (vec3_length(V(rel)) < lwc1(pv + 0x14)) {
            mat3_transpose(M(rot), M(m + 0xc0));
            mat4_transform(V(rel), V(rel), M(rot));
            if (lwc1(rel + 4) < 0.2f) stood_on = true;
        }
    }
    switch (state) {  // (the table L15:0x1e34c0)
    case 0: {  // (0x002e5a28)
        if (lbu(pv + 9) == 0) {  // the lead: its radius from its chain point 0, the group's switches told and counted
            moby_chain_point_v(m, 0, V(rel));
            swc1(pv + 0x14, vec3_distance(V(m + 0x10), V(rel)));
            sb(pv + 0xb, 0);
            sw(pv + 0x10, m);
            group_first(group, lbu(m + 0x21), 0, 0);
            while (lwu(group) != 0) {
                const uint32_t o = lwu(group);
                if (o != m && lh(o + 0xa6) == SWITCH_CLASS) {
                    const uint32_t opv = lwu(o + 0x78);
                    sw(opv + 0x10, m);
                    sb(opv + 9, 1);
                    swc1(opv + 0x14, lwc1(pv + 0x14));
                }
                sb(pv + 0xb, lbu(pv + 0xb) + 1);
                group_next(group, lwu(group), 0, 0);
            }
        }
        sh(pv + 6, 0);
        sh(pv, 0);
        sh(pv + 2, 0);
        sb(pv + 0xa, 0);
        sh(pv + 4, 0);
        moby_set_color_hi(m, 0, 0, 0);
        sb(m + 0x20, 1);
        break;
    }
    case 1:
    case 2: {  // (0x002e5b14)
        if (already_done(m, pv)) {
            moby_group_set_state(lbu(m + 0x21), DONE);
            finish(m, pv);
            break;
        }
        if (lh(pv + 4) == 0 && stood_on) {  // lit: the lead starts the group's time when it waits
            const uint32_t lead = lwu(pv + 0x10);
            const uint32_t lpv = lwu(lead + 0x78);
            if (lbu(lead + 0x20) == 1) {
                sb(pv + 0xa, 0);
                sb(lead + 0x20, 2);
                // (the lead's time tested, the switch's own time taken)
                sh(lpv, lw(lpv + 0xc) <= 0 ? 0x384u : static_cast<uint32_t>(frame_scale_i(lw(pv + 0xc))));
            }
            sb(lpv + 0xa, lbu(lpv + 0xa) + 1);
            moby_sound_start(3, SOUND_FLAGS, m);
            moby_set_color_hi(m, 0, 0xff, 0);
            sh(pv + 4, 1);
        }
        if (lbu(m + 0x20) != 2) break;
        // the lead, running
        if (!(lbu(pv + 0xa) < lbu(pv + 0xb))) {  // all lit
            moby_group_set_state(lbu(m + 0x21), DONE);
            moby_sound_start(1, SOUND_FLAGS, m);
            finish(m, pv);
            break;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv))) != 0) {  // the time is out: the group back
            sh(pv + 4, 0);
            sb(pv + 0xa, 0);
            moby_sound_start(2, SOUND_FLAGS, m);
            moby_group_set_state(lbu(m + 0x21), 3);
            break;
        }
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 2))) == 0) break;
        // the tick, again sooner as the time runs out
        moby_sound_start(0, SOUND_FLAGS, m);
        const int32_t longest = frame_scale_i(0x2d);
        const int32_t sooner = frame_scale_i(5) + ee_div(lh(pv), frame_scale_i(0x28));
        if (longest < sooner) {
            sh(pv + 2, static_cast<uint32_t>(frame_scale_i(0x2d)));
        } else {
            const int32_t base = frame_scale_i(5);
            sh(pv + 2, static_cast<uint32_t>(base + ee_div(lh(pv), frame_scale_i(0x28))));
        }
        break;
    }
    case 3:  // (0x002e5d54)
        moby_set_color_hi(m, 0, 0, 0);
        sh(pv + 4, 0);
        sb(pv + 0xa, 0);
        sb(m + 0x20, 1);
        break;
    case 4: {  // (0x002e5d7c) done: blinks, its flag set
        if (timer_tick_i16(*static_cast<int16_t*>(ee_ram_ptr(pv + 6))) != 0) {
            sh(pv + 6, static_cast<uint32_t>(frame_scale_i(0x14)));
            if (lbu(pv + 8) == 0) {
                moby_set_color_hi(m, 0xff, 0xff, 0xff);
                sb(pv + 8, 1);
            } else {
                moby_set_color_hi(m, 0, 0xff, 0);
                sb(pv + 8, 0);
            }
        }
        const uint32_t level_word = level_flag_word(m);
        sw(level_word, lwu(level_word) | flag_bit(m));
        const uint32_t word = flag_word(m);
        sw(word, lwu(word) | flag_bit(m));
        break;
    }
    default:
        break;
    }
    ee_stack_free(0xc0);
}

}  // namespace rac1
