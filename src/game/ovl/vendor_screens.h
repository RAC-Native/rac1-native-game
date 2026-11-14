// The Vendor's screens (game mode 5), shared by their start / update (vendor_screens.cpp, vendor_update.cpp) and their
// draw (vendor_draw.cpp). Veldin's addresses: every level runs its own copy of this code, relinked; the overlay
// addresses below go through OV / OVF (ovl_reloc.h), the boot ELF's (below 0x15ef00) stay as they are.
//
// The screens' block VS = 0x1ca4c0 (0x220 bytes, cleared by the start):
//   +0x00 the screens' state: 0 opening, 1 the item list, 2 closing, 3 a weapon's demo (the scene played after a purchase)
//   +0x04 the frames in the state, +0x08 the draw's mode (0: the frame rendered and its picture stored, 1: the picture put
//         back), +0x0c 1 once opened, +0x10 / +0x14 two blocks of the scene buffer (0x173e00's +8 / +4, + 0x60000): the
//         stored picture and the Vendor's head moby, +0x18 the Vendor's animations (read as a WAD 0x80000 further, unpacked here)
//   +0x1c the Vendor moby, +0x20 the item mobys' block (0x100 bytes a moby: +0x100 the item, +0x200 / +0x300 its two
//         hologram parts, +0x400 the quantity box, +0x500 the second ring), +0x24 the Vendor's head (class 0xc),
//         +0x28 a moby the hero is put next to after a demo (0: the Vendor), +0x2c the sign's text buffer
//   +0x30 the time of the last remark, +0x34 the idle frames, +0x38 the language's voice set (0x15ed88 - 1, at least 0),
//         +0x3c the remark (0 / 1), +0x40 opened by the PDA (the hero's item 0x20) instead of a Vendor, +0x44 the sign's scroll
//   +0x48 the weapon bought last (-1: none), +0x4c the wheel's scroll, +0x50 / +0x54 the HUD's ammo / bolts counters
//         (-1: none), +0x58 the cursor, +0x5c the purchase's step (0 none, 1 the box opens, 2 the box, 3 / 4 it closes)
//   +0x60 / +0x70 the screens' camera position / target, +0x78 its angle, +0x80 the demo's matrix, +0xb0 its angles,
//         +0xc0 its position
//   +0xd0 the item list: 16 entries of 0x14 bytes (+0 the item, +4 1 for its ammo, +8 0 when it can be bought, +0x10 the
//         item's word), built by item_list_build (0x29a400); +0x210 their count
#ifndef RAC1_GAME_OVL_VENDOR_SCREENS_H
#define RAC1_GAME_OVL_VENDOR_SCREENS_H

#include <cstdint>

#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/vu0.h>  // rac1::Vec4

namespace rac1::vendor {

constexpr OvConst<0x001ca4c0u> VS{};
// VS's fields
constexpr uint32_t STATE = 0x00, FRAMES = 0x04, DRAW_MODE = 0x08, OPENED = 0x0c, SCENE_A = 0x10, SCENE_B = 0x14, ANIMS = 0x18,
                   VENDOR = 0x1c, MOBYS = 0x20, HEAD = 0x24, PLACE_MOBY = 0x28, SIGN = 0x2c, REMARK_TIME = 0x30, IDLE = 0x34,
                   VOICE = 0x38, REMARK = 0x3c, PDA = 0x40, SIGN_SCROLL = 0x44, BOUGHT = 0x48, SCROLL = 0x4c, AMMO_HUD = 0x50, BOLTS_HUD = 0x54,
                   CURSOR = 0x58, STEP = 0x5c, CAM_POS = 0x60, CAM_TARGET = 0x70, CAM_ANGLE = 0x78, DEMO_MATRIX = 0x80,
                   DEMO_ANGLES = 0xb0, DEMO_POS = 0xc0, LIST = 0xd0, COUNT = 0x210;
constexpr uint32_t ENTRY_SIZE = 0x14;
// An entry's fields
constexpr uint32_t E_ITEM = 0x0, E_AMMO = 0x4, E_LOCKED = 0x8;

// The screens' globals (overlay data)
constexpr OvConst<0x00161188u> HEAD_UP{};         // the Vendor's head made (it talks)
constexpr OvConst<0x0016118cu> ANIMS_READING{};   // the Vendor's animations are being read (the head is made once read)
constexpr OvConst<0x00161190u> DIALOG_QUEUED{};   // a remark queued (its stream starts when the dialog is ready)
constexpr OvConst<0x00161194u> DIALOG_PLAYING{};  // a remark plays (stopped when the head's mouth closes)
constexpr OvConst<0x00161e0cu> CONFIRMED{};       // the purchase box closed with Cross (1) or Triangle (0)
constexpr OvConst<0x00161e10u> QUANTITY_BOX{};    // the box asks for a quantity (an ammo purchase the bolts pay for)
constexpr OvConst<0x00161e14u> CLOSE_TIMER{};     // the frames before the screens close
constexpr OvConst<0x00161e18u> CLOSING{};         // Triangle pressed: the screens close
constexpr OvConst<0x00161e1cu> OPEN_TIMER{};      // the frames of the opening's pause
constexpr OvConst<0x00161e20u> OPENING{};
constexpr OvConst<0x00161e24u> SPIN{};            // the hologram's spin angle
constexpr OvConst<0x00161e28u> HELD_LEFT{};       // the frames Left / Right have been held (the quantity's repeat)
constexpr OvConst<0x00161e2cu> HELD_RIGHT{};
constexpr OvConst<0x00161e30u> PAD_BEFORE{};      // pre-1.02 only: 1.02 removed this word
constexpr OvConst<0x00161e34u> QUANTITY{};
constexpr OvConst<0x00161e38u> CURSOR_MOVED{};    // the hologram is made again
constexpr OvConst<0x001ca6e0u> BIG_HEAD_NODE{};   // the head's scale node (0x24f590's, 0x40 bytes) of the cheat 0x15edb0
constexpr OvConst<0x00165e80u> JOINT_NODES{};     // the Vendor's four joint nodes (joints 0x14..0x17: the screens' arms)
constexpr OvConst<0x0016c860u> SCENE{};           // the demo's scene stream block (0x1c0 bytes)
constexpr OvConst<0x0015f5c4u> GAME_MODE{};
constexpr OvConst<0x0015f5d8u> SKIP_DRAW{};
constexpr OvConst<0x0015f3fcu> FADE{};            // the screen's fade level

// The game's tables (overlay data)
constexpr OvConst<0x001c40b0u> ITEMS{};      // 0x18 bytes an item: +0 / +4 the price (the second with 0x13d4e3), +8 a round's
                                             // price, +0xa at the PDA, +0xe the most ammo
constexpr OvConst<0x00179ac0u> HUD_ITEMS{};  // 0x4c bytes an item: +0x10 its class
constexpr OvConst<0x001c2108u> ITEM_TEXTS{}; // 0x14 bytes an item: +0x10 the name's text id

// The boot ELF's data
constexpr uint32_t PAD = 0x0013c940u;      // the pad block: +0x1c0 the buttons held, +0x1c4 the buttons of this frame
constexpr uint32_t PAD_HELD = PAD + 0x1c0, PAD_NOW = PAD + 0x1c4, PAD_DEBUG = PAD + 0x1a4;
constexpr uint32_t BTN_TRIANGLE = 0x10, BTN_CROSS = 0x40, BTN_RIGHT = 0x2000, BTN_LEFT = 0x8000;
constexpr uint32_t AMMO = 0x0013d428u;          // the ammo of each item (words)
constexpr uint32_t AMMO_BOUGHT = 0x0013dd70u;   // the ammo bought, of each item (words)
constexpr uint32_t BOLTS = 0x0015ed98u;
constexpr uint32_t LANGUAGE = 0x0015ed88u;
constexpr uint32_t VIDEO_50HZ = 0x0015ed80u;     // the video mode is 50 Hz (the PAL build): other scene sectors and timings
constexpr uint32_t FRAME_TIME = 0x0015ed60u;     // the frame's length in 60ths (1.2 at 50 Hz)
constexpr uint32_t SECOND_PRICES = 0x0013d4e3u; // a byte (0x13d4c0's 0x23rd flag): the items' second price, the hologram lifted
constexpr uint32_t DIALOG = 0x001516d0u;        // the dialog stream: +0x1c the line to play, +0x5a its state (3: ready)
constexpr uint32_t DIALOG_LINE = DIALOG + 0x1c, DIALOG_STATE = DIALOG + 0x5a;
constexpr uint32_t STREAM_BUSY = DIALOG + 8;      // a halfword: the stream's reads under way
constexpr uint32_t HERO = 0x0013f350u;           // the hero block P: +0x10b8 the item in hand, +0x1ff5 the Drone Device bought
constexpr uint32_t HERO_ITEM = HERO + 0x10b8, HERO_DRONES = HERO + 0x1ff5;
constexpr int32_t ITEM_DRONE_DEVICE = 0x18;      // (its hologram is the class 0x1df, not its HUD class)

// orig: (helper) the list's entry `i`
inline uint32_t entry(int32_t i) { return VS + LIST + static_cast<uint32_t>(i) * ENTRY_SIZE; }

// --- vendor_screens.cpp ------------------------------------------------------------------------
bool button_pressed(uint32_t button);
void pad_history_set(uint32_t buttons);
void vendor_seqs_unpack();                 // orig: 0x0029a330
void vendor_screens_exit(int32_t demo);    // orig: 0x0029ab70
void vendor_demo_start();                  // orig: 0x0029ad08
void vendor_demo_exit();                   // orig: 0x0029af80
void vendor_sign_text(uint32_t text);      // orig: 0x0029b070
void vendor_head_anim(uint32_t head);      // orig: 0x0029b0b0
void vendor_remark(int32_t kind);          // orig: 0x0029b4d8
void vendor_hologram_make();               // orig: 0x0029b680
uint32_t scene_stream_load(int32_t scene); // orig: 0x002454c8
uint32_t scene_block_select(int32_t i);    // orig: 0x002455c0
// --- vendor_update.cpp -------------------------------------------------------------------------
void vendor_purchase_update();             // orig: 0x0029ba78

// --- vendor_draw.cpp ---------------------------------------------------------------------------
struct Box {  // a screen box: its size and its corner, pixels
    float w, h, x, y;
};
void gs_target_rect(int32_t x0, int32_t y0, int32_t x1, int32_t y1, int32_t tw, int32_t th, uint32_t rgba);  // orig: 0x00201560
void gs_target_clear(int32_t w, int32_t h);                                                         // orig: 0x002013b0
void gs_textured_quad(int32_t u, int32_t v, int32_t du, int32_t dv, uint64_t rgba, uint64_t tex0, float x, float y, float w,
                      float h);                                                                     // orig: 0x001fa418
void text_right(int32_t x, int32_t y, uint64_t colour, uint32_t s, int32_t n);                      // orig: 0x001fb888
Box box_project(const Vec4& p0, const Vec4& p1);                                                    // orig: 0x0029a148
void target_begin(int32_t lw, int32_t lh, float aspect);                                            // orig: 0x0029f2d0
void target_end();                                                                                  // orig: 0x0029f390
void sign_glyphs(uint32_t s, int32_t x, int32_t y, float scale);                                    // orig: 0x0029dac8
void panel_sign();                                                                                  // orig: 0x0029dcd8
void panel_head();                                                                                  // orig: 0x0029dde8
void price_format(uint32_t dst, int32_t v);                                                         // orig: 0x0029de40
void panel_list();                                                                                  // orig: 0x0029dea0
void panel_monitor();                                                                               // orig: 0x0029e198
void panel_message(int32_t w, int32_t h);                                                           // orig: 0x0029e6c0
void panel_prompt();                                                                                // orig: 0x0029e918
void panel_quantity(int32_t w, int32_t h);                                                          // orig: 0x0029ead8
void screen_static(int32_t k, float w, float h);                                                    // orig: 0x0029ef68
void screens_draw(uint32_t vendor);                                                                 // orig: 0x0029f3c0
void screens_glass(uint32_t vendor);                                                                // orig: 0x0029f990
void quantity_screen(uint32_t box);                                                                 // orig: 0x0029fcd0
void screens_frames();                                                                              // orig: 0x0029ff58
void frame_picture_store(uint32_t dst);                                                             // orig: 0x001f99c0

}  // namespace rac1::vendor

#endif
