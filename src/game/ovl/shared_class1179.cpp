// Class 1179, a pad the hero uses once: one port of the copies Pokitaru and Quartu carry. The copies are the same code
// relinked (compared with the relocated fields masked, and normalised: no other difference); what they name is the
// engine's (OV: the level state 0x1ba5d0 and its checkpoint copy 0x1bb230, the level's moby-uid table 0x15fc88) or the base
// ELF's (bare: the hero 0x13f350.., the help records 0x141968, the levels' tables 0x14c050 / 0x14c190, the level 0x15ed84,
// the game time 0x15eea4). Both copies test for level 11 (Pokitaru) and read Pokitaru's rows of the levels' tables, also
// on Quartu. Header shared_class1179.h.
#include <game/ovl/shared_class1179.h>

#include <game/math/vecmath.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

namespace {

constexpr uint32_t HELP = 0x00141968;  // the help messages' records (8 bytes each: +0 times shown, +2 when, +4 levels)
constexpr int32_t POKITARU = 11;

int32_t trunc_w(float f) { return static_cast<int32_t>(f2u(eer::cvt_w(f))); }

// The game time in units of 600 frames (10 s).
int32_t time_tens() { return ee_div(frame_scale_i(lw(0x0015eea4)), 600); }

// Help message `msg` (record `opt`), shown again when its last showing is more than 18 s ago or it was never shown;
// otherwise the record's time is kept at the latest.
void help_again(int32_t msg, int32_t opt) {
    const uint32_t rec = HELP + static_cast<uint32_t>(opt) * 8;
    const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(rec + 2)) * 600;
    const int32_t wait = trunc_w(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f));
    if (wait < since || static_cast<int32_t>(lhu(rec + 2)) * 600 == 0) {
        sub_00203908(msg, opt);
        return;
    }
    if (static_cast<int32_t>(lhu(rec + 2)) < time_tens()) sh(rec + 2, static_cast<uint32_t>(time_tens()));
}

// It was used: it sinks by 1.5 and plays its sound.
void use(uint32_t m) {
    sb(m + 0x20, 2);
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), 1.5f));
    moby_sound_start(0, 0, m);
}

}  // namespace

// State 0 (start): the record of its help message 0x2b01 cleared, the flag 8 of its pvars' +0x3e set. On Pokitaru, a pad with a
// uid (+0xb0) that was used before (its byte in the level checkpoint's state, or its bit in Pokitaru's row of the table
// 0x14c190) starts used; otherwise it waits (state 1).
// State 1: used when the hero stands on it in state 0x22 with the flag 0x13d4c3 set and no 0x13f65e; on Pokitaru its uid
// is marked used in the level state and, when the uid is a level-wide one (0x15fc88) not in Pokitaru's row of 0x14c050,
// in the checkpoint copy too. While it has a uid, the hero within 4 (flat) and in mode 0 or 1: help message 0x2b02
// (record 0x71) without the flag 0x13d4c3, else 0x2b01 (record 0x6c), shown the first time at once (and the record marked
// for the level).
// orig: L11:0x0030ee00 L15:0x002d9100
void class1179_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        const uint32_t pv = lwu(m + 0x78);
        sh(HELP + 0x6c * 8 + 2, 0);
        sh(HELP + 0x6c * 8, 0);
        sh(pv + 0x3e, lhu(pv + 0x3e) | 8u);
        if (lw(0x0015ed84) != POKITARU || lbu(m + 0xb0) == 0xff) {
            sb(m + 0x20, 1);
            return;
        }
        const uint32_t idx = lhu(m + 0xb2);
        const int32_t sidx = static_cast<int16_t>(idx);
        bool used = lbu(OV(0x001bb230) + static_cast<uint32_t>(sidx) + 0x454) != 0;
        if (!used) {
            const uint32_t word = lwu(0x0014c190 + (POKITARU << 8) + (static_cast<uint32_t>(sidx >> 5) << 2));
            used = ((static_cast<int32_t>(word) >> (idx & 0x1f)) & 1) != 0;
        }
        if (used) use(m);
        else sb(m + 0x20, 1);
        return;
    }
    if (st != 1) return;

    if (lwu(0x0013f64c) == m && lbu(0x0013d4c3) != 0 && lh(0x0013f65e) == 0 && lw(0x001413d4) == 0x22) {
        if (lw(0x0015ed84) == POKITARU && lbu(m + 0xb0) != 0xff) {
            sb(OV(0x001ba5d0) + static_cast<uint32_t>(lh(m + 0xb2)) + 0x454, lbu(m + 0xb0) + 2u);
            const uint32_t uid = lbu(m + 0xb0);
            bool keep = uid == 0xff;
            if (!keep && lbu(OV(0x0015fc88) + uid) != 0xff) keep = lbu(0x0014c050 + uid + (POKITARU << 4)) == 0xff;
            if (keep) sb(OV(0x001bb230) + static_cast<uint32_t>(lh(m + 0xb2)) + 0x454, uid + 2u);
        }
        use(m);
    }

    if (lbu(m + 0xb0) == 0xff) return;
    if (!(vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 4.0f)) return;
    if (!(lwu(0x001413dc) < 2u)) return;
    if (lbu(0x0013d4c3) == 0) {
        help_again(0x2b02, 0x71);
        return;
    }
    const uint32_t rec = HELP + 0x6c * 8;
    if (lhu(rec) != 0) {
        help_again(0x2b01, 0x6c);
        return;
    }
    sh(rec, 1);
    if (static_cast<int32_t>(lhu(rec + 2)) < time_tens()) sh(rec + 2, static_cast<uint32_t>(time_tens()));
    sw(rec + 4, lwu(rec + 4) | (1u << (lwu(0x0015ed84) & 0x1f)) | 0x80000000u);
}

}  // namespace rac1
