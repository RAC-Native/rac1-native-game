// Aridia's own moby classes: class 1324, the level's hint giver (it starts the help dialogues 0x7d2 .. 0x7dc and the
// level's 0x4e22 / 0x4e25 / 0x4e2c as the hero comes into its boxes), hand-ported from Aridia's overlay (`orig: L2:`).
// The base ELF's (the hero 0x13f350, the game record 0x141968, the items owned 0x13d4c0, the switches 0x14c050, the
// difficulty rows 0x14ee90, the play time 0x15eea4) are bare. Declared in aridia_enemies.h, registered in aridia_mobys.cpp.
// The dialogue ids are the records sub_00203908 starts (as the engine's own hint code passes them), not text ids.
#include <game/math/vecmath.h>
#include <game/ovl/aridia_enemies.h>
#include <game/ovl/ee_regs.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/variants_l2.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;     // the hero (+0x80 his position; his state 0x1413d4, the one before 0x1413e0)
constexpr uint32_t RECORD = 0x00141968;   // the game record (the hints' counters and times)
constexpr uint32_t OWNED = 0x0013d4c0;    // the items owned (a byte each)

bool hero_in(int32_t box) { return point_in_box(HERO + 0x80, box) != 0; }
}  // namespace

// The play time in steps of 0x258 frames (10 s at 60 Hz).
// orig: (helper) the hint code's frame_scale_i(*0x15eea4) / 0x258, inlined in the original
int32_t hint_play_steps() { return frame_scale_i(lw(0x0015eea4)) / 0x258; }

// A hint whose last time (in play steps) is the half at `last`: due again once more than 18 s (scaled) have passed, or
// when it was never given.
// orig: (helper) inlined in the original (class 1324, Skid's agent)
bool hint_due(uint32_t last) {
    const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(last)) * 0x258;
    const int32_t limit = static_cast<int32_t>(f2u(eer::cvt_w(fpu_mul(static_cast<float>(frame_scale_i(0x12)), 60.0f))));
    return limit < since || static_cast<int32_t>(lhu(last)) * 0x258 == 0;
}

// The hint's last time moved up to now (when now is later).
// orig: (helper) inlined in the original (class 1324, Skid's agent)
void hint_seen(uint32_t last) {
    if (static_cast<int32_t>(lhu(last)) < hint_play_steps()) sh(last, static_cast<uint32_t>(hint_play_steps()));
}

// A visited place's hint (its record: the visits +0, the last time +2, the frame modes it was seen in +4): the first visit
// counts it (and marks the frame's mode 0x15ed84 with the top bit); later visits start the dialogue `text` (slot `slot`)
// when it is due, else note the time.
// orig: (helper) inlined in the original (class 1324's 0x141968 + 0x90, Skid's agent's + 0x70)
void hint_visit(uint32_t rec, int32_t text, int32_t slot) {
    if (lhu(rec) != 0) {
        if (hint_due(rec + 2)) sub_00203908(text, slot);
        else hint_seen(rec + 2);
        return;
    }
    sh(rec, lhu(rec) + 1);
    hint_seen(rec + 2);
    sw(rec + 4, lwu(rec + 4) | (1u << (lwu(0x0015ed84) & 31)) | 0x80000000u);
}

// Class 1324 (pvars: its boxes +0x10 .. +0x5c, the switch column +0, the mode +0x54, counters +0x30 / +0x50, a flag
// +0x58), every frame, by where the hero is:
// - in box +0x5c, early in the game (0x1413dc below 2, no other hint on): two or more of the 37 items (bar 8 and 24)
//   owned and not yet placed (0x179ac0 records): dialogue 0x4e25;
// - the mode: 0 waits for him in box +0x48 in state 0x2c (then 1); 1 (left again by 0x13f650): in box +0x4c the first
//   time (0x13d409), a sound and the HUD message 0x53d6;
// - in box +0x10 with its switch on (0x14c050) and item 0x10 owned, at difficulty 6+ and more than 0x4650 frames since
//   0x1417a2: dialogue 0x7d2; without item 0x10, at difficulty 3+: 0x7d3 (once each, record +0x60 / +0x68);
// - box +0x40 sets +0x58: then dialogues 0x4e22 (fewer than 3 of 0x141888 and the mode not marked) and 0x4e2c (the
//   flags 0x15eddc / 0x15ede0 on), else the record's +0x3b0 cleared;
// - box +0x18 marks the record's +0x80; a jump landed from state 0x2c on the ground counts +0x30: from 2, in box +0x1c,
//   dialogue 0x7d6;
// - item 0xc not owned: in box +0x14 dialogue 0x7d7 when due (+0x8a); owned: box +0x14 is a visited place (+0x90,
//   dialogue 0x7d8), else box +0x1c clears its count;
// - the hero in state 0x72 at a class-0x267 moby: dialogue 0x7db without the record's +0x230, else (no +0x238) 0x7dc after
//   0x258 frames of it (+0x50);
// - box +0x44 sets the story byte 0x13d39c; the hero's place is kept at +0x20.
// orig: L2:0x002ee890
void class1324_update_l2(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    sb(m + 0x30, 0xff);
    if (hero_in(lw(pv + 0x5c)) && lwu(0x001413dc) < 2 && lhu(0x00141be0) == 0 && lhu(0x001418e8) == 0) {
        int32_t owned = 0;
        for (int32_t i = 0; i < 37; ++i) {
            if (lw(OV(0x00179ac0) + 8 + static_cast<uint32_t>(i * 0x4c)) != 0) continue;
            if (i == 8 || i == 24) continue;
            if (lbu(OWNED + static_cast<uint32_t>(i)) != 0) ++owned;
        }
        if (owned >= 2) sub_00203908(0x4e25, 0x4f);
    }
    const int32_t mode = lw(pv + 0x54);
    if (mode == 0) {
        if (hero_in(lw(pv + 0x48)) && lw(HERO + 0x2084) == 0x2c) sw(pv + 0x54, 1);
    } else if (mode == 1) {
        if (lw(HERO + 0x300) != 0) {
            sw(pv + 0x54, 0);
        } else if (hero_in(lw(pv + 0x4c)) && lbu(0x0013d409) == 0) {
            sb(0x0013d409, 1);
            sound_play_indexed(1, 0, 0);
            sub_00263d40(TXT(0x53d6), -1);
        }
    }
    const uint32_t switch_on = 0x0014c050u + lwu(pv) + (lwu(0x0015ed84) << 4);  // (its switch, the frame mode's row)
    const uint32_t difficulty = 0x0014ee90u + (lwu(pv) << 2);
    if (lhu(RECORD + 0x60) == 0 && hero_in(lw(pv + 0x10)) && lbu(switch_on) != 0xff && lbu(OWNED + 0x10) != 0 &&
        !(lw(difficulty) < 6)) {
        const int32_t since = frame_scale_i(lw(0x0015eea4)) - static_cast<int32_t>(lhu(0x001417a2)) * 0x258;
        if (frame_scale_i(0x4650) < since) sub_00203908(0x7d2, 0xc);
    }
    if (hero_in(lw(pv + 0x40))) sw(pv + 0x58, 1);
    if (lw(pv + 0x58) != 0) {
        const uint32_t mode_bit = 1u << (lwu(0x0015ed84) & 31);
        if (lhu(0x00141888) < 3 && (lwu(RECORD + 0x264) & mode_bit) == 0) sub_00203908(0x4e22, 0x4c);
        if (lbu(0x0015eddc) != 0 && lbu(0x0015ede0) != 0) {
            if ((lwu(RECORD + 0x3b4) & (1u << (lwu(0x0015ed84) & 31))) == 0) sub_00203908(0x4e2c, 0x76);
        } else {
            sh(RECORD + 0x3b0, 0xffff);
        }
    }
    if (lhu(RECORD + 0x68) == 0 && hero_in(lw(pv + 0x10))) {
        if (lbu(0x0014c050u + lwu(pv) + (lwu(0x0015ed84) << 4)) != 0xff && lbu(OWNED + 0x10) == 0 &&
            3.0f <= static_cast<float>(lw(0x0014ee90u + (lwu(pv) << 2))))
            sub_00203908(0x7d3, 0xd);
    }
    if (lhu(RECORD + 0x80) == 0) {
        if (hero_in(lw(pv + 0x18))) sh(RECORD + 0x80, 0xffff);
        if (lw(HERO + 0x2084) != 0x2c && lw(HERO + 0x2090) == 0x2c && lw(HERO + 0x198) == 1) sw(pv + 0x30, lwu(pv + 0x30) + 1);
        if (!(lw(pv + 0x30) < 2) && hero_in(lw(pv + 0x1c))) sub_00203908(0x7d6, 0x10);
    }
    bool visit_box = true;  // (with item 0xc owned, box +0x14 is a visited place)
    if (lbu(OWNED + 0xc) == 0) {
        if (hero_in(lw(pv + 0x14))) {
            if (hint_due(RECORD + 0x8a)) sub_00203908(0x7d7, 0x11);
            else hint_seen(RECORD + 0x8a);
        }
        visit_box = lbu(OWNED + 0xc) != 0;
    }
    if (visit_box && hero_in(lw(pv + 0x14))) hint_visit(RECORD + 0x90, 0x7d8, 0x12);
    else if (hero_in(lw(pv + 0x1c))) sh(RECORD + 0x90, 0xffff);
    const uint32_t key = lhu(RECORD + 0x230);
    if ((key == 0 || lhu(RECORD + 0x238) == 0) && lw(0x001413d4) == 0x72) {
        const uint32_t at = lwu(HERO + 0x2fc);
        if (at != 0 && lh(at + 0xa6) == 0x267) {
            if (key == 0) {
                sub_00203908(0x7db, 0x46);
            } else if (lhu(RECORD + 0x238) == 0) {
                sw(pv + 0x50, lwu(pv + 0x50) + 1);
                if (frame_scale_i(0x258) < lw(pv + 0x50)) sub_00203908(0x7dc, 0x47);
            }
        }
    }
    if (lbu(0x0013d39c) == 0 && hero_in(lw(pv + 0x44))) sb(0x0013d39c, 1);
    lq_sq(pv + 0x20, HERO + 0x80);
}

}  // namespace rac1
