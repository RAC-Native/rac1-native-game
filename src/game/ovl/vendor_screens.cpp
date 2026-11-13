// The Vendor's screens (game mode 5): their start, their ways out (back to the game, or into a weapon's demo and out of
// it), the Vendor's head and its remarks, the sign's text and the selected item's hologram. See vendor_screens.h for
// the screens' block and globals.
#include <game/ovl/vendor_screens.h>

#include <game/enhance/vendor_buy_all.h>
#include <game/lib/libc.h>
#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/hydro.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/vendor_frame.h>
#include <game/ovl/vendor_tr.h>

namespace rac1 {

using namespace vendor;

namespace {
constexpr float PI = std::bit_cast<float>(0x40490fdbu);
constexpr float HALF_PI_NEG = std::bit_cast<float>(0xbfc90fdbu);
constexpr uint32_t GREY = NOT_ADDR(0x202020u);  // the item mobys' colour (moby_set_color_packed)

// The calls the Vendor's "All Ammo" entry steps in at (vendor_buy_all.h), by the original call's return address
constexpr uint32_t SITE_START_CAMERA_RESET = NOT_ADDR(0x0029a838u);

// orig: (helper) a moby's +0x20 state byte set, with its animation `set` started over `frames` frames unless it plays
// already (+0x53)
void head_anim_to(uint32_t m, int32_t set, int32_t frames, uint32_t state) {
    if (static_cast<int32_t>(lbu(m + 0x53)) != set) moby_anim_set(m, static_cast<uint32_t>(set), 0, frame_scale_i(frames));
    sb(m + 0x20, state);
}

// orig: (helper) the dialog stream's state 6 / 7 kept, any other set to 5 (0x15172a)
void dialog_release() {
    if (static_cast<uint32_t>(lhu(DIALOG_STATE)) - 6u >= 2u) sh(DIALOG_STATE, 5);
}

// orig: (helper) the remark `line` of the language's voice set queued (0x1516ec: 0x2710 + line * 6 + the set)
void remark_queue(int32_t line) { sw(DIALOG_LINE, static_cast<uint32_t>(line * 6 + lw(VS + VOICE) + 0x2710)); }

// orig: (helper) the HUD's ammo counter of the item `item` (slot 0x30, key 60000 + the item: its ammo, its most)
int32_t ammo_counter_request(int32_t item) {
    return camera_request(0x30, static_cast<uint32_t>(item + 60000), OVF(0x0023a760), OVF(0x00235ea0), OVF(0x0023a7a8),
                          AMMO + static_cast<uint32_t>(item) * 4, lhu(ITEMS + static_cast<uint32_t>(item) * 0x18 + 0xe));
}

// orig: (helper) the screens' two ways out: game mode 0, the hero back in the game
void hero_back() {
    sw(SKIP_DRAW, 1);
    sw(GAME_MODE, 0);
    hero_set_state(0, 1);
    sb(HERO + 0x20a5, 0);
    sub_002330d0();
}

// orig: (helper) the hero placed at the offset `ofs` of the moby whose pointer is at `slot` (its matrix +0xc0, position
// +0x10), facing its angle (+0x48) + `turn`; `cam` hero_place's last argument. The vectors are the original's frame (EE
// stack: hero_place reads them by address)
void hero_place_by(uint32_t slot, uint32_t ofs, float turn, int32_t cam, bool hydro) {
    const uint32_t f = ee_stack_alloc(0x20);
    const uint32_t pos = f, rot = f + 0x10;
    mat3_transform(V(pos), V(ofs), M(lwu(slot) + 0xc0));
    vec3_add(V(pos), V(pos), V(lwu(slot) + 0x10));
    sw(rot + 0x0, 0);
    sw(rot + 0x4, 0);
    swc1(rot + 0x8, angle_add(lwc1(lwu(slot) + 0x48), turn));
    sw(rot + 0xc, 0);  // (not written by the original: its stack's word)
    hero_place(pos, rot, 0, cam);
    if (hydro) hydro_2eac18(2);
    ee_stack_free(0x20);
}
}  // namespace

namespace vendor {

// The Vendor's animations unpacked (the WAD the draw read 0x80000 bytes past VS+0x18, into VS+0x18) and each of the 16
// sequences of the Vendor's class relocated (its class: 0x197300[0x197bcc], its sequence table +0x48).
// orig: 0x0029a330
void vendor_seqs_unpack() {
    const uint32_t packed = lwu(VS + ANIMS) + 0x80000u;
    // (FlushCache before and after: no effect on the host)
    wad_decompress(packed, lwu(VS + ANIMS));
    const uint32_t cls = lwu(OV(0x00197300) + static_cast<uint32_t>(lbu(OV(0x00197bcc))) * 4);
    const uint32_t offsets = lwu(VS + ANIMS);
    for (uint32_t i = 0; i < 16; ++i) {
        const uint32_t ofs = lwu(offsets + i * 4);
        if (ofs == 0) continue;
        sw(cls + 0x48 + i * 4, lwu(VS + ANIMS) + ofs);
        moby_seq_relocate(cls, i);
    }
}

// The screens left for the game: the HUD's counters released, game mode 0, the hero put in front of the Vendor (unless
// opened by the PDA; `demo` 1 after a weapon's demo, the camera then follows at once), the Vendor idle, the PDA's
// Vendor retired, the music back.
// orig: 0x0029ab70
void vendor_screens_exit(int32_t demo) {
    if (lw(VS + BOLTS_HUD) != -1) sub_002359f8(lwu(VS + BOLTS_HUD), 0);
    if (lw(VS + AMMO_HUD) != -1) sub_002359f8(lwu(VS + AMMO_HUD), 0);
    sw(VS + AMMO_HUD, 0xffffffffu);
    sw(VS + BOLTS_HUD, 0xffffffffu);
    hud_weapon_element_release();
    hero_back();
    if (lw(VS + PDA) == 0) {
        if (demo == 0) hero_place_by(VS + VENDOR, OV(0x00161070), PI, 0, true);
        else hero_place_by(VS + VENDOR, OV(0x00161070), PI, 1, false);
    }
    sb(lwu(VS + VENDOR) + 0x20, 1);
    dialog_release();
    if (lw(VS + PDA) != 0) moby_retire(lwu(VS + VENDOR));
    snd_cmd_17(0x1d);
    music_volumes_default();
    snd_flush();
}

// A weapon's demo (after a purchase, the weapon's scene from the disc): the screens' state 3, the demo's place before the
// Vendor (VS+0xc0, on the floor) and its matrix, the scene buffer cleared, the weapon in hand put away, the weapon's class
// loaded, its scene read (0x1ca020[the weapon]) and its first block chosen; the Vendor's line 0x2734 played once the dialog
// stream is ready.
// orig: 0x0029ad08
void vendor_demo_start() {
    hydro_2eac18(2);
    dialog_release();
    sw(SKIP_DRAW, 1);
    sb(lwu(VS + VENDOR) + 0x20, 1);
    sw(VS + STATE, 3);
    sw(VS + FRAMES, 0);
    mat3_transform(V(VS + DEMO_POS), V(OV(0x001610f0)), M(lwu(VS + VENDOR) + 0xc0));
    vec3_add(V(VS + DEMO_POS), V(VS + DEMO_POS), V(lwu(VS + VENDOR) + 0x10));
    swc1(VS + DEMO_POS + 8, fpu_add(lwc1(VS + DEMO_POS + 8), 2.0f));
    swc1(VS + DEMO_POS + 8, floor_probe_v(0.5f, V(VS + DEMO_POS), 0));
    vec_clear(V(VS + DEMO_ANGLES));
    swc1(VS + DEMO_ANGLES + 8, angle_add(lwc1(lwu(VS + VENDOR) + 0x48), PI));
    euler_to_mat3_vu(M(VS + DEMO_MATRIX), V(VS + DEMO_ANGLES));
    mem_fill32(SCENE, 0, 0x1c0);
    mem_fill32(OV(0x0017c340), 0, 0x40);
    dma_wait(1);
    sub_002a07c8();
    const int32_t weapon = lw(VS + BOUGHT);
    const uint32_t buf = lwu(OV(0x001611cc)) - 0x40000u;
    const uint32_t scene = lwu(OV(0x001ca020) + static_cast<uint32_t>(weapon) * 4);
    const uint32_t in_hand = lwu(HERO_ITEM);
    sw(OV(0x0016c8bc), lwu(OV(0x00173e08)) + buf);
    sw(OV(0x0016c8b8), lwu(OV(0x00173e04)) + buf);
    sw(OV(0x001611cc), buf);
    swc1(FADE, 0.0f);
    sw(OV(0x0015f400), 0);
    if (in_hand != 0 && static_cast<uint32_t>(weapon) != in_hand) hero_slot_close(0, 0);
    const int32_t bought = lw(VS + BOUGHT);
    sw(HERO + 0x20b8, bought == ITEM_DRONE_DEVICE ? 0u : static_cast<uint32_t>(bought));
    item_class_load(lwu(HUD_ITEMS + static_cast<uint32_t>(lw(VS + BOUGHT)) * 0x4c + 0x10), -1);
    scene_stream_load(static_cast<int32_t>(scene));
    sw(DIALOG_LINE, 0x2734);
    scene_block_select(0);
    while (lh(DIALOG_STATE) != 3) {
        sound_frame();
        vendor_vsync_wait(NOT_ADDR(0x0029af48u));  // (the vsync wait of this loop: not ported, vendor_frame.h)
    }
    dialog_stream_stop();
}

// The demo left: game mode 0, the hero put beside the moby VS+0x28 (turned a quarter left of it), the HUD's ammo element
// back, the music back.
// orig: 0x0029af80
void vendor_demo_exit() {
    hero_back();
    hero_place_by(VS + PLACE_MOBY, OV(0x00161100), HALF_PI_NEG, 1, false);
    hud_weapon_element_release();
    dialog_release();
    snd_cmd_17(0x1d);
    music_volumes_default();
    snd_flush();
}

// The sign's text: `text` after 18 spaces (the marquee's run-in) into the buffer VS+0x2c, its scroll from the start.
// orig: 0x0029b070
void vendor_sign_text(uint32_t text) {
    libc_sprintf(lwu(VS + SIGN), OV(0x001e94b0), {static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(text)))});
    sw(VS + SIGN_SCROLL, 0);
}

// The Vendor's head (VS+0x24): its animation by its state (+0x20); with the cheat 0x15edb0 (big heads) its scale node. 0 idle (a remark after
// 600 frames of nothing), 2 / 3 the idles, 4 / 5 / 6 talking (the remark's stream stopped at its line's frame, then idle),
// 10 / 11 / 12 a remark starts once the dialog is ready.
// orig: 0x0029b0b0
void vendor_head_anim(uint32_t head) {
    const uint32_t em = BIG_HEAD_NODE;
    if (lbu(0x0015edb0) != 0) {
        if (lbu(em + 1) == 0) sub_0024f590(head, 0, em);
        const float big = std::bit_cast<float>(0x4017ae14u);
        swc1(em + 0x28, big);
        swc1(em + 0x20, big);
        swc1(em + 0x24, big);
    } else if (lbu(em + 1) != 0) {
        sub_0024f608(head, em);
    }
    // (the original jumps through its table 0x1e94d0; the states' NTSC-U cases are in the comments)
    switch (lbu(head + 0x20)) {
    case 0:  // (0x0029b158) idle: after 600 frames, a remark
        if (frame_scale_i(600) < lw(VS + IDLE)) {
            const int32_t r = rand_mod(2);
            sw(DIALOG_QUEUED, 1);
            sw(VS + REMARK, static_cast<uint32_t>(r));
            sw(DIALOG_PLAYING, 0);
            remark_queue(r * 3 + 1);
            head_anim_to(head, 2, 0x12, 0xc);
            sw(VS + IDLE, 0);
        }
        sw(VS + IDLE, static_cast<uint32_t>(lw(VS + IDLE) + 1));
        break;
    case 2:  // (0x0029b1f8)
        if (frame_scale_i(600) < lw(VS + IDLE)) {
            head_anim_to(head, 3, 0x12, 3);
            sw(VS + IDLE, 0);
        }
        sw(VS + IDLE, static_cast<uint32_t>(lw(VS + IDLE) + 1));
        break;
    case 3:  // (0x0029b260)
        if ((lbu(head + 0x70) & 2) == 0) break;
        head_anim_to(head, 0, 0x12, 0);
        break;
    case 4:  // (0x0029b2a0)
    case 5:  // (0x0029b2ac)
    case 6: {  // (0x0029b2b8)
        // the frame of the remark's animation where its line ends (three tables, by the state; the remark +0x3c)
        const uint32_t st = lbu(head + 0x20);
        const uint32_t ends = st == 4 ? OV(0x00161110) : st == 5 ? OV(0x00161118) : OV(0x00161120);
        const float end = static_cast<float>(lw(ends + static_cast<uint32_t>(lw(VS + REMARK)) * 4));
        if (anim_crossed(head, end) != 0 && lw(DIALOG_PLAYING) != 0) {
            sw(DIALOG_PLAYING, 0);
            dialog_stream_stop();
        }
        if ((lbu(head + 0x70) & 2) == 0) break;
        head_anim_to(head, 2, 0x12, 2);
        sw(VS + IDLE, 0);
        break;
    }
    case 10:  // (0x0029b344)
        if (lh(DIALOG_STATE) == 3 && lw(DIALOG_QUEUED) != 0) {
            sw(DIALOG_QUEUED, 0);
            sw(DIALOG_PLAYING, 1);
        }
        if ((lbu(head + 0x70) & 2) == 0) break;
        head_anim_to(head, lw(VS + REMARK) * 3 + 4, 0xc, 4);
        break;
    case 11:  // (0x0029b440)
    case 12: {  // (0x0029b3c8)
        if (lh(DIALOG_STATE) != 3 || lw(DIALOG_QUEUED) == 0) break;
        sw(DIALOG_PLAYING, 1);
        sw(DIALOG_QUEUED, 0);
        const int32_t k = lbu(head + 0x20) == 12 ? 5 : 6;
        head_anim_to(head, lw(VS + REMARK) * 3 + k, 0x12, static_cast<uint32_t>(k));
        break;
    }
    default:  // 1, 7, 8, 9 (0x0029b4b8): nothing
        break;
    }
}

// A remark of the Vendor's head (once it is made): `kind` 0 a remark on a move of the cursor (one in four, at most every
// 360 frames, while idle), 1 the greeting, 2 / 3 after a purchase (the idle count restarted).
// orig: 0x0029b4d8
void vendor_remark(int32_t kind) {
    if (lw(HEAD_UP) == 0) return;
    const uint32_t head = lwu(VS + HEAD);
    if (kind == 1) {
        const int32_t r = rand_mod(2);
        sw(VS + REMARK, static_cast<uint32_t>(r));
        sw(DIALOG_QUEUED, 1);
        sw(DIALOG_PLAYING, 0);
        remark_queue(r * 3);
        if (lbu(head + 0x53) != 1) moby_anim_set(head, 1, 0, 10);
        sb(head + 0x20, 10);
        return;
    }
    if (kind == 0) {
        const uint32_t st = lbu(head + 0x20);
        if (st != 2 && st != 0) return;
        if (rand_mod(4) != 0) return;
        const int32_t gap = frame_scale_i(0x168);
        const int32_t now = lw(OV(0x0015f3f8));
        if (!(static_cast<int32_t>(lwu(VS + REMARK_TIME) + static_cast<uint32_t>(gap)) < now)) return;
        sw(VS + REMARK_TIME, static_cast<uint32_t>(now));
        const int32_t r = rand_mod(2);
        sw(DIALOG_QUEUED, 1);
        sw(VS + REMARK, static_cast<uint32_t>(r));
        sw(DIALOG_PLAYING, 0);
        remark_queue(r * 3 + 2);
        sb(head + 0x20, 0xb);
        return;
    }
    if (kind == 2 || kind == 3) {
        const uint32_t st = lbu(head + 0x20);
        if (st == 2 || st == 0) sw(VS + IDLE, 0);
    }
}

// The selected item's hologram: the item's model (+0x100: its HUD class, the Drone Device's 0x1df) on the stand before the
// Vendor (its offset 0x1c8920[item], a weapon's lifted by its height, twice with 0x13d4e3), its turn (0x1c8900[item]),
// and the two parts of its stand's display (+0x200: the class 0x1c8ff0[item] +0x30 if any, +0x300: the item's class).
// orig: 0x0029b680
void vendor_hologram_make() {
    const auto item_at_cursor = [] { return lw(entry(lw(VS + CURSOR)) + E_ITEM); };
    const int32_t item = item_at_cursor();
    uint32_t cls = lwu(HUD_ITEMS + static_cast<uint32_t>(item) * 0x4c + 0x10);
    if (item == ITEM_DRONE_DEVICE) cls = 0x1df;
    else item_class_load(cls, -1);
    const auto mob = [](uint32_t o) { return lwu(VS + MOBYS) + o; };

    moby_init(mob(0x100), cls);
    sh(mob(0x132), 0x40);
    sw(mob(0x194), 0);
    Vec4 ofs{};
    vec3_add(ofs, V(OV(0x00161090)), V(OV(0x001c8920) + static_cast<uint32_t>(item_at_cursor()) * 0x30));
    if (lw(entry(lw(VS + CURSOR)) + E_AMMO) == 0) {
        const float h = lwc1(OV(0x001c8920) + static_cast<uint32_t>(item_at_cursor()) * 0x30 + 0xc);
        ofs.z = fpu_add(ofs.z, h);
        if (lbu(SECOND_PRICES) != 0) ofs.z = fpu_add(ofs.z, h);
    }
    mat3_transform(V(mob(0x110)), ofs, M(lwu(VS + VENDOR) + 0xc0));
    vec3_add(V(mob(0x110)), V(mob(0x110)), V(lwu(VS + VENDOR) + 0x10));
    const uint32_t turn = OV(0x001c8900) + static_cast<uint32_t>(item_at_cursor()) * 0x30;
    swc1(mob(0x140), lwc1(turn + 0x10));
    swc1(mob(0x144), lwc1(turn + 0x14));
    swc1(mob(0x148), fpu_add(lwc1(turn + 0x18), lwc1(lwu(VS + VENDOR) + 0x48)));
    moby_update_transform(mob(0x100));
    moby_set_anim(mob(0x100), 0, 0);
    if (lbu(lwu(mob(0x124)) + 0xc) >= 2) moby_set_anim(mob(0x100), 1, 0);
    sw(mob(0x158), 0);
    sb(mob(0x151), 0);
    sub_0024ebc0(mob(0x100));
    moby_set_color_packed(mob(0x100), GREY, 0xe, 0xe, 0);

    const uint32_t part = lwu(OV(0x001c8ff0) + (static_cast<uint32_t>(item_at_cursor()) << 6) + 0x30);
    if (part != 0) {
        moby_init(mob(0x200), part);
        if (lbu(lwu(mob(0x224)) + 6) != 0) sb(mob(0x273), 0x18);
        sh(mob(0x232), 0x40);
        moby_update_transform(mob(0x200));
        moby_set_color_packed(mob(0x200), GREY, 0xe, 0xe, 0);
    }
    moby_init(mob(0x300), cls);
    // (the original tests the +0x200 moby's class again here, and sets that moby's +0x73)
    if (lbu(lwu(mob(0x224)) + 6) != 0) sb(mob(0x273), 0x18);
    sh(mob(0x332), 0x40);
    sw(mob(0x394), 0);
    moby_update_transform(mob(0x300));
    moby_set_anim(mob(0x300), 0, 0);
    if (lbu(lwu(mob(0x324)) + 0xc) >= 2) moby_set_anim(mob(0x300), 1, 0);
    sw(mob(0x358), 0);
    sb(mob(0x351), 0);
    sub_0024ebc0(mob(0x300));
    sh(mob(0x334), lhu(mob(0x334)) | 0x100u);
    moby_set_color_packed(mob(0x300), GREY, 0xe, 0xe, 0);
    sw(CURSOR_MOVED, 0);
}

// A demo's scene read from the disc into the buffer 0x16c8bc (its sectors: 0x1390b0[scene], or 0x139120[scene] at 50 Hz,
// 8 bytes each), the screen faded meanwhile, and the scene's block table (0x16c860+0x5c, 8 bytes a block, up to 70:
// the blocks' addresses, 0x800 past each offset, from +0x60). Returns the last address written (0: none), as the original
// leaves it in v0.
// orig: 0x002454c8
uint32_t scene_stream_load(int32_t scene) {
    const uint32_t rec = 0x00137b80u + static_cast<uint32_t>(scene) * 8 + (lw(VIDEO_50HZ) != 0 ? 0x15a0u : 0x1530u);
    stream_disc_read(lwu(OV(0x0016c8bc)), lwu(rec), lwu(rec + 4));
    stream_reads_wait(0);
    screen_fade(4);
    stream_reads_wait(1);
    const uint32_t blk = SCENE;
    uint32_t e = lwu(blk + 0x5c);
    for (uint32_t i = 0; lwu(e + 4) != 0; ++i, e += 8) {
        const uint32_t ofs = lwu(e) + 0x800u;
        sw(blk + 0x60 + i * 4, lwu(blk + 0x5c) + ofs);
        if (i + 1 == 70) return ofs;
    }
    return 0;
}

// The scene's block `i` (its address +0x60[i] into 0x16c8bc) decoded, the buffer's pointer then put back (0x173e08 +
// 0x1611cc). Returns that pointer.
// orig: 0x002455c0
uint32_t scene_block_select(int32_t i) {
    sw(OV(0x0016c8bc), lwu(SCENE + 0x60 + static_cast<uint32_t>(i) * 4));
    scene_block_decode();
    const uint32_t p = lwu(OV(0x00173e08)) + lwu(OV(0x001611cc));
    sw(OV(0x0016c8bc), p);
    return p;
}

}  // namespace vendor

// The screens opened, by a Vendor `vendor` (its moby) or by the PDA (0: a Vendor moby made in front of the hero): the
// block and the big-head node cleared, the sounds and the music for the screens, the item list built and the cursor put
// in its middle, the HUD's bolts counter (and the ammo counter when the cursor is on ammo), the scene buffer's two blocks,
// the Vendor's greeting sound and animation, game mode 5, the hero held, the screens' camera (a Vendor's), the moby pass's
// rows 0x180240..0x18027f, and the Vendor's arms (its joint nodes).
// orig: 0x0029a6b0
void vendor_screen_start_tr(uint32_t a0) {
    mem_fill32(VS, 0, 0x220);
    mem_fill32(BIG_HEAD_NODE, 0, 0x40);
    uint32_t v = a0;
    if (v == 0) {
        v = moby_spawn(0xb);
        sh(v + 0x32, 0x40);
        swc1(v + 0x10, fpu_add(lwc1(OV(0x00166dc0)), lwc1(OV(0x00161060))));
        swc1(v + 0x14, lwc1(OV(0x00166dc4)));
        swc1(v + 0x18, fpu_sub(lwc1(OV(0x00166dc8)), lwc1(OV(0x00161068))));
        vec_clear(V(v + 0x40));
        swc1(v + 0x48, PI);
        moby_update_transform(v);
        sb(v + 0x20, 3);
        sw(VS + PDA, 1);
    }
    const int32_t voice = lw(LANGUAGE) - 1;
    sw(VS + VOICE, voice < 0 ? 0u : static_cast<uint32_t>(voice));
    sw(VS + SIGN_SCROLL, 0);
    sw(VS + BOUGHT, 0xffffffffu);
    sw(VS + AMMO_HUD, 0xffffffffu);
    sw(VS + BOLTS_HUD, 0xffffffffu);
    sw(VS + CURSOR, 0);
    sw(CLOSING, 0);
    sw(OPENING, 0);
    sw(PAD_BEFORE, 0);
    sw(VS + STEP, 0);
    sw(QUANTITY, 0);
    sw(SPIN, 0);
    snd_cmd_16(0x1d);
    sub_00265828(0);
    snd_flush();
    item_list_build(lw(VS + PDA) > 0 ? 1 : 0);
    const int32_t n = lw(VS + COUNT);
    const int32_t middle = n / 2;
    sw(VS + CURSOR, static_cast<uint32_t>(middle));
    sw(VS + SCROLL, static_cast<uint32_t>(((n > 2 ? middle : 1) - middle) * 0x28));
    vendor_buy_all_camera_reset(SITE_START_CAMERA_RESET);
    sw(VS + BOLTS_HUD, static_cast<uint32_t>(camera_request(0x12, 0x754e, OVF(0x00239cc8), OVF(0x00239d00), OVF(0x00239df8), BOLTS, 9999999)));
    const uint32_t e = entry(lw(VS + CURSOR));
    if (lw(e + E_AMMO) == 1) sw(VS + AMMO_HUD, static_cast<uint32_t>(ammo_counter_request(lw(e + E_ITEM))));
    sw(SKIP_DRAW, 1);
    sw(VS + STATE, 0);
    sw(VS + SCENE_B, lwu(OV(0x00173e00) + 4) + 0x60000u);
    sw(VS + SCENE_A, lwu(OV(0x00173e00) + 8) + 0x60000u);
    sw(VS + DRAW_MODE, 0);
    sw(VS + VENDOR, v);
    moby_sound_start(3, 0, v);
    if (lw(VS + PDA) == 0) screen_fade(4);
    sub_002039a0();
    swc1(FADE, 1.0f);
    sw(GAME_MODE, 5);
    sw(OV(0x0015f400), 0);
    hero_set_state(0x64, 1);
    sb(HERO + 0x20a5, 1);
    sb(HERO + 0x20ac, 1);
    sub_00232fe8();
    moby_set_anim(v, lw(VS + PDA) != 0 ? 3u : 2u, 0);
    swc1(v + 0x58, fpu_mul(lwc1(FRAME_TIME), 0.5f));
    if (lw(VS + PDA) == 0) {
        mat3_transform(V(VS + CAM_POS), V(OV(0x00161060)), M(lwu(VS + VENDOR) + 0xc0));
        vec3_add(V(VS + CAM_POS), V(VS + CAM_POS), V(lwu(VS + VENDOR) + 0x10));
        swc1(VS + CAM_ANGLE, angle_add(lwc1(lwu(VS + VENDOR) + 0x48), PI));
        camera_scripted_set(VS + CAM_POS, VS + CAM_TARGET, 1, 0, 0);
        uid5_set_pvar_80(VS + CAM_POS);
        uid5_set_pvar_90(VS + CAM_TARGET);
    }
    // the moby pass's light rows (0x180240..0x180270): the screens' own
    mat3_transform(V(OV(0x00180250)), V(OV(0x001610d0)), M(lwu(VS + VENDOR) + 0xc0));
    lq_sq(OV(0x00180240), OV(0x001610e0));
    sd(OV(0x00180270), 0);
    sd(OV(0x00180278), 0);
    sd(OV(0x00180260), 0);
    sd(OV(0x00180268), 0);
    // the Vendor's four joint nodes (the screens' arms), folded in by the length of a short list (8 entries: open)
    for (uint32_t i = 0; i < 4; ++i) {
        const uint32_t node = JOINT_NODES + i * 0x40;
        if (lbu(node + 1) == 0) sub_0024f590(lwu(VS + VENDOR), i + 0x14, node);
    }
    const uint32_t nodes = JOINT_NODES;
    const int32_t count = lw(VS + COUNT);
    if (count < 8) {
        const int32_t d = 7 - count;
        const float in = static_cast<float>(d * -0x681);
        const float out = static_cast<float>(d * 0x681);
        swc1(nodes + 0xf4, in);
        swc1(nodes + 0xb4, out);
        swc1(nodes + 0x34, out);
        swc1(nodes + 0x74, in);
    } else {
        sw(nodes + 0xf4, 0);
        sw(nodes + 0x34, 0);
        sw(nodes + 0x74, 0);
        sw(nodes + 0xb4, 0);
    }
    sw(HEAD_UP, 0);
    sw(ANIMS_READING, 0);
}

// The adapters of the generated code's calls (the novalis_gold unit's glue, uniform ABI: v0 and f0 returned; f0 is 0).
// orig: 0x002454c8
TrRet ovl_2454c8_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    return TrRet{scene_stream_load(static_cast<int32_t>(a0)), 0.0f};
}
// orig: 0x002455c0
TrRet ovl_2455c0_tr(uint32_t a0, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float) {
    return TrRet{scene_block_select(static_cast<int32_t>(a0)), 0.0f};
}

}  // namespace rac1
