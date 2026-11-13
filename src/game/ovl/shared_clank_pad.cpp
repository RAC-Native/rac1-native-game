// Giant Clank's pad on Quartu (level 15, class 1451) and on Veldin's finale (18, class 1899): Ratchet standing on it is told
// to press (text 0x3aa4; as Giant Clank on Quartu, 0x3aa5), and the press turns him into Giant Clank (the moby at pvar +0x60
// shown and taken as the hero's mode 2, a cutscene and the music of the fight) or, on Quartu, back. The two levels' copies
// are the same code, relinked: no level word; the moby list 0x15ffd8, the game mode 0x15f5c4 and the press 0x15f594 go
// through OV(), the base ELF's are bare (the hero 0x13f350, the pad's buttons 0x13cae4, the broken mobys 0x14c050, the
// game's level number 0x15ed84). Header shared_clank_pad.h.
//
// The 1.02 build (the PAL disc 2.00: L15:0x2ee458, L18:0x2fccc8) changed the press that boards: a moby still linked at pvar
// +0x70 is retired first (its index cleared), and on Quartu the pad's place is saved as the checkpoint (checkpoint_save,
// 0x284e50) before Ratchet is put on it. That is 1.02 Gameplay fix 23 (fixes102.h), off by default.
#include <game/ovl/shared_clank_pad.h>

#include <game/enhance/fixes102.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

namespace {
constexpr uint32_t HERO = 0x0013f350;
int32_t game_level() { return lw(0x0015ed84); }
uint32_t moby_at(int32_t index) { return lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(index) << 8); }

// the hero's frame kept for the cutscene (+0x80 / +0x90 into +0x1d00 / +0x1d10)
void keep_hero_frame() {
    V(HERO + 0x1d00) = V(HERO + 0x80);
    V(HERO + 0x1d10) = V(HERO + 0x90);
}
}  // namespace

// pvars: Giant Clank's moby index +0x60, +0x68 (-1 while Ratchet is not on the pad), a moby left linked +0x70 (1.02).
// States: 0 Giant Clank hidden (on the finale, Giant Clank already broken: 3), 1 -> 2, 2 the pad.
// orig: L15:0x002ed068 L18:0x002fb868
void clank_pad_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    switch (lbu(m + 0x20)) {
    case 0: {
        const int32_t index = lw(pv + 0x60);
        if (index == -1) {
            moby_retire(m);
            return;
        }
        const uint32_t clank = moby_at(index);
        sb(clank + 0x31, 0);
        sw(clank + 0x94, 0);
        sh(clank + 0x34, lhu(clank + 0x34) | 1u);
        const bool broken = game_level() == 0x12 && lbu(0x0014c050 + lbu(m + 0xb0) + 0x120) == 0xff;
        sb(m + 0x20, broken ? 3u : 1u);
        sw(pv + 0x68, 0xffffffffu);
        return;
    }
    case 1:
        sb(m + 0x20, 2);
        return;
    case 2:
        break;
    default:
        return;
    }
    // Ratchet on the pad, free (not in a cutscene, not in state 0x1d)
    if (lwu(HERO + 0x2fc) != m || lh(HERO + 0x30e) != 0 || lw(OV(0x0015f5c4)) == 2 || lw(HERO + 0x2084) == 0x1d) {
        sw(pv + 0x68, 0xffffffffu);
        return;
    }
    const uint32_t mode = lbu(HERO + 0x20a4);
    if (mode == 0) hud_message_request(4, TXT(0x3aa4));
    else if (mode == 2 && game_level() != 0x12) hud_message_request(4, TXT(0x3aa5));
    if ((lwu(0x0013cae4) & 0x10) == 0 || lw(OV(0x0015f594)) != 4) return;  // (the press)
    if (lbu(HERO + 0x20a4) == 2) {
        if (game_level() != 0xf) return;  // (Giant Clank on the finale: no way back)
        const uint32_t clank = moby_at(lw(pv + 0x60));  // (Quartu: back to Ratchet, Giant Clank hidden)
        sw(clank + 0x94, 0);
        sh(clank + 0x34, lhu(clank + 0x34) | 1u);
        sb(clank + 0x31, 0);
        hero_mode_exit();
        moby_set_anim(clank, 0, 0);
        if (game_level() != 0xf) return;
        cutscene_start(9);
        music_track_request(0, 5);
        return;
    }
    if (lbu(HERO + 0x20a4) != 0) return;
    const uint32_t clank = moby_at(lw(pv + 0x60));
    if (gameplay102_on(23)) {  // enhancement: 1.02 Gameplay, fix 23 (fixes102.h): a moby left linked is retired
        const int32_t left = lw(pv + 0x70);
        if (left != -1) {
            const uint32_t other = moby_at(left);
            if (other != 0 && lbu(other + 0x20) != 0xfe && lbu(other + 0x20) != 0xfd) {
                moby_retire(other);
                sw(pv + 0x70, 0xffffffffu);
            }
        }
    }
    const uint32_t sp = ee_stack_alloc(0x10);
    V(sp) = V(m + 0x10);
    swc1(sp + 8, fpu_add(floor_probe_v(0.5f, V(sp), 0), u2f(0x3dcccccdu)));  // (0.1 over the floor)
    if (gameplay102_on(23) && game_level() == 0xf) checkpoint_save(sp, m + 0x40);  // enhancement: 1.02 Gameplay, fix 23: Quartu's checkpoint at the pad
    hero_place(sp, m + 0x40, 0, 1);
    ee_stack_free(0x10);
    sb(clank + 0x31, 1);
    sw(clank + 0x94, lwu(lwu(clank + 0x24) + 0x10));
    sh(clank + 0x34, lhu(clank + 0x34) & 0xfffeu);
    hero_mode_enter(2, 0x5a, clank);
    if (game_level() == 0xf) {
        cutscene_start(8);
        keep_hero_frame();
        music_track_request(2, 4);
    } else if (game_level() == 0x12) {
        cutscene_start(6);
        keep_hero_frame();
        sb(m + 0x20, 3);
    }
}

}  // namespace rac1
