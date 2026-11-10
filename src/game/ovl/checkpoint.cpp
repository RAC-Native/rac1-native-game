// The checkpoint's save side (its restore, 0x285008, is in level_setup.cpp) and the table of the
// level's collected things. Header checkpoint.h.
#include <game/ovl/checkpoint.h>

#include <game/enhance/session_runtime.h>

#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// Marks entry `idx` of the current level's row of the collected-things table (0x14c050, 16 bytes a
// level) as done; 0xff means none.
// orig: 0x002502a0
void checkpoint_mark_collected(uint32_t idx) {
    if (idx == 0xff) return;
    sb(0x0014c050 + idx + (static_cast<uint32_t>(lw(0x0015ed84)) << 4), 0xff);
}

// Takes the checkpoint at `pos` / `rot` (the position and angles quads): the level state 0x1ba620 (0x404
// bytes) copied to 0x1bb280, then for each of the 0x7ff moby slots with a state byte at 0x1ba624 + i: kept
// (copied to 0x1bb684 + i, its bit set in the bitmaps 0x1ba4d0 and 0x14c190's row of the level) when it is 1
// or the collected-things entry of (byte - 2) is 0xff; then 0x1bb230: 1, the position at +0x10, the angles
// at +0x20, the hero moby's +0x38 / +0x3c / +0x80 at +0x30 / +0x34 / +0x38, the vehicle mode (PLAYER+0x20a4)
// at +0x44, the hero moby at +0x48, PLAYER+0x22cc at +0x4c, the HUD's +0x64 / +0x68 / +0x69 / +0x6a at
// +0x3c / +0x40 / +0x41 / +0x42, and the music track (0x151708's byte +0x22, or its +0x38 half when -1) at +0xc54.
// orig: 0x00284e50
void checkpoint_save_v(const Vec4& pos, uint32_t rot) {
    mem_copy(OV(0x001bb280), OV(0x001ba620), 0x404);
    const uint32_t level = static_cast<uint32_t>(lw(0x0015ed84));
    for (int32_t i = 0; i < 0x7ff; ++i) {
        const uint32_t b = lbu(OV(0x001ba620) + 0x404 + static_cast<uint32_t>(i));
        if (b == 0) continue;
        if (b != 1 && lbu(0x0014c050 + (level << 4) + b - 2) != 0xff) continue;
        const uint32_t w = static_cast<uint32_t>(i >> 5) << 2, bit = 1u << (static_cast<uint32_t>(i) & 0x1f);
        sb(OV(0x001bb280) + 0x404 + static_cast<uint32_t>(i), b);
        sw(OV(0x001ba4d0) + w, lwu(OV(0x001ba4d0) + w) | bit);
        sw(0x0014c190 + (level << 8) + w, lwu(0x0014c190 + (level << 8) + w) | bit);
    }
    const uint32_t C = OV(0x001bb230), P = 0x0013f350, HUD = 0x0013e550;
    sw(C, 1);
    V(C + 0x10) = pos;
    lq_sq(C + 0x20, rot);
    const uint32_t hero = lwu(P + 0x2080);
    const int16_t t1 = lh(P + 0x22cc);
    const int32_t t5 = static_cast<int8_t>(lbu(0x001516d0 + 0x22));
    sw(C + 0x30, lwu(hero + 0x38));
    sw(C + 0x34, lwu(hero + 0x3c));
    sw(C + 0x44, lbu(P + 0x20a4));
    sw(C + 0x38, lwu(hero + 0x80));
    sw(C + 0x48, hero);
    sw(C + 0x4c, static_cast<uint32_t>(static_cast<int32_t>(t1)));
    sw(C + 0x3c, lwu(HUD + 0x64));
    sb(C + 0x40, lbu(HUD + 0x68));
    sb(C + 0x41, lbu(HUD + 0x69));
    sb(C + 0x42, lbu(HUD + 0x6a));
    sw(C + 0xc54, t5 == -1 ? static_cast<uint32_t>(static_cast<int32_t>(lh(0x001516d0 + 0x38))) : static_cast<uint32_t>(t5));
    if (g_session_rt) {  // (enhancement: a session's checkpoint, src/game/enhance/session_runtime.h)
        const float at[3] = {pos.x, pos.y, pos.z};
        session_checkpoint_game(at, lwc1(rot + 8));
    }
}

// orig: 0x00284e50
void checkpoint_save(uint32_t pos, uint32_t rot) { checkpoint_save_v(V(pos), rot); }

}  // namespace rac1
