// The Vendor's screens' update (game mode 5): the opening and the closing, the item list's cursor (a strip up to 7
// entries, a wheel from 8), the purchase box (Cross on an entry: a weapon's Yes / No, an ammo's quantity) and the
// purchase, the Vendor's head and the holograms, the weapon's demo after a purchase. See vendor_screens.h.
#include <game/ovl/vendor_screens.h>

#include <game/enhance/vendor_buy_all.h>
#include <game/lib/libvu0.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/vendor_tr.h>

namespace rac1 {

using namespace vendor;

namespace {
// The calls the Vendor's "All Ammo" entry steps in at (vendor_buy_all.h): the text lookups, by the original call's return
// address
constexpr uint32_t SITE_BOUGHT_AMMO = NOT_ADDR(0x0029c428u), SITE_BOUGHT_WEAPON = NOT_ADDR(0x0029c5b8u),
                   SITE_NO_BOLTS = NOT_ADDR(0x0029c5d4u), SITE_STRIP_RIGHT = NOT_ADDR(0x0029d080u),
                   SITE_STRIP_LEFT = NOT_ADDR(0x0029d1a4u), SITE_WHEEL_RIGHT = NOT_ADDR(0x0029d290u),
                   SITE_WHEEL_LEFT = NOT_ADDR(0x0029d3d0u);
constexpr uint32_t GREY = NOT_ADDR(0x202020u);
// The Vendor's sounds (moby_sound_start's index on the Vendor moby)
constexpr int32_t SND_BOX = 0, SND_MOVE = 1, SND_CANT = 2, SND_OPEN = 4, SND_CLOSE = 5, SND_GOODBYE = 6, SND_BOUGHT = 7;

// orig: (helper) the item table's record of the item `item`
uint32_t item_rec(int32_t item) { return ITEMS + static_cast<uint32_t>(item) * 0x18; }
// orig: (helper) a round's price of the item: the PDA's or the Vendor's
int32_t round_price(int32_t item) { return lhu(item_rec(item) + (lw(VS + PDA) != 0 ? 0xau : 0x8u)); }
// orig: (helper) a weapon's price: the second with 0x13d4e3 set
int32_t weapon_price(int32_t item) { return lw(item_rec(item) + (lbu(SECOND_PRICES) != 0 ? 4u : 0u)); }
// orig: (helper) the item's ammo is short of its most
bool ammo_short(int32_t item) { return lw(AMMO + static_cast<uint32_t>(item) * 4) < static_cast<int32_t>(lhu(item_rec(item) + 0xe)); }
// orig: (helper) the entry at the cursor
uint32_t cursor_entry() { return entry(lw(VS + CURSOR)); }
int32_t cursor_item() { return lw(cursor_entry() + E_ITEM); }
// orig: (helper) a sound of the Vendor moby
void vendor_sound(int32_t k) { moby_sound_start(k, 0, lwu(VS + VENDOR)); }
// orig: (helper) a button pressed this frame, not the frame before
bool pressed(uint32_t b) { return (lwu(PAD_NOW) & b) != 0 && (lwu(PAD_BEFORE) & b) == 0; }

// orig: (helper) the HUD's ammo counter of the item (slot 0x30, key 60000 + the item: its ammo, its most)
int32_t ammo_counter_request(int32_t item) {
    return camera_request(0x30, static_cast<uint32_t>(item + 60000), OVF(0x0023a760), OVF(0x00235ea0), OVF(0x0023a7a8),
                          AMMO + static_cast<uint32_t>(item) * 4, lhu(item_rec(item) + 0xe));
}

// orig: (helper) the cursor moved (0x29c648's four moves, after the cursor is stored): the sign shows the item's name, the
// HUD's ammo counter follows the entry (released, made again on an ammo entry)
void cursor_moved(uint32_t site) {
    vendor_sign_text(vendor_buy_all_text(site, lw(ITEM_TEXTS + static_cast<uint32_t>(cursor_item()) * 0x14 + 0x10)));
    if (lw(VS + AMMO_HUD) != -1) sub_002359f8(lwu(VS + AMMO_HUD), 0);
    sw(VS + AMMO_HUD, 0xffffffffu);
    if (lw(cursor_entry() + E_AMMO) == 1) sw(VS + AMMO_HUD, static_cast<uint32_t>(ammo_counter_request(cursor_item())));
}

// orig: (helper) the quantity box's repeat: a button held 16 frames steps every 8 frames, from 48 every frame
bool held_repeat(int32_t held) { return held >= 16 && ((held & 7) == 0 || held >= 48); }
// orig: (helper) the quantity's step sound: every 4th frame of a repeat (or the press itself)
bool held_sound(int32_t held) { return (held & 3) == 0 || held < 16; }

// orig: (helper) the quantity box's moby (+0x400): its animation run backwards to its first frame over `frames` (closing)
void box_close_anim(int32_t frames) {
    const uint32_t box = lwu(VS + MOBYS) + 0x400;
    moby_anim_set(box, 0, static_cast<int32_t>(lbu(lwu(lwu(box + 0x24) + 0x48) + 0x10)) - 1, frames);
}

// orig: (helper) the update's world passes, each run when its bit of 0x16c060 is set, or all of them with the bit 0x10 and
// the debug button (L3 on the second pad word)
bool world_pass(uint32_t bit) {
    const uint32_t flags = lwu(OV(0x0016c060));
    if ((flags & bit) != 0) return true;
    return (flags & 0x10) != 0 && (lwu(PAD_DEBUG) & 0x200) != 0;
}

// The purchase, step by step (VS+0x5c): 1 the box opens (an ammo's quantity box when the bolts pay a round and the ammo
// is short, else the weapon's Yes / No, or an error sound), 2 the box (Left / Right the quantity, Cross / Triangle close
// it), 3 its closing animation, 4 the purchase on Cross: the ammo or the weapon bought, the list rebuilt.
// orig: 0x0029ba78 (vendor_purchase_update)
void purchase_box_open() {
    const uint32_t box = lwu(VS + MOBYS) + 0x400;
    sw(QUANTITY, 1);
    sw(CONFIRMED, 0);
    moby_init(box, 0x471);
    sh(lwu(VS + MOBYS) + 0x432, 0x40);
    const Vec4 zero{};
    vec3_add(V(lwu(VS + MOBYS) + 0x410), zero, V(lwu(VS + VENDOR) + 0x10));
    lq_sq(lwu(VS + MOBYS) + 0x440, lwu(VS + VENDOR) + 0x40);
    moby_set_anim(lwu(VS + MOBYS) + 0x400, 0, 0);
    const uint32_t mobys = lwu(VS + MOBYS);
    if (lbu(lwu(lwu(mobys + 0x424) + 0x48) + 0x10) < 2) {
        sw(mobys + 0x458, 0);
        sb(lwu(VS + MOBYS) + 0x451, 0);
        sub_0024ebc0(lwu(VS + MOBYS) + 0x400);
    }
    moby_update_transform(lwu(VS + MOBYS) + 0x400);
    moby_set_color_packed(lwu(VS + MOBYS) + 0x400, NOT_ADDR(0x101010), 0xe, 0xe, 0);
    sw(QUANTITY_BOX, 0);
    const int32_t item = cursor_item();
    if (lw(cursor_entry() + E_AMMO) == 1 && !ammo_short(item)) {
        vendor_sound(SND_CANT);
    } else if (lw(cursor_entry() + E_AMMO) == 1) {
        if (lw(BOLTS) < round_price(cursor_item())) vendor_sound(SND_CANT);
        else sw(QUANTITY_BOX, 1);
    } else if (lw(BOLTS) < weapon_price(cursor_item())) {
        vendor_sound(SND_CANT);
    }
    sw(VS + STEP, 2);
}

// orig: (helper) the quantity box's Left / Right (step 2, the box asks for a quantity)
void purchase_quantity() {
    sw(HELD_LEFT, (lwu(PAD_HELD) & BTN_LEFT) != 0 ? static_cast<uint32_t>(lw(HELD_LEFT) + 1) : 0u);
    sw(HELD_RIGHT, (lwu(PAD_HELD) & BTN_RIGHT) != 0 ? static_cast<uint32_t>(lw(HELD_RIGHT) + 1) : 0u);
    // Left: one less, down to 1
    if ((lwu(PAD_NOW) & BTN_LEFT) != 0 || held_repeat(lw(HELD_LEFT))) {
        const int32_t q = lw(QUANTITY);
        if (q >= 2) {
            sw(QUANTITY, static_cast<uint32_t>(q - 1));
            if (held_sound(lw(HELD_LEFT))) vendor_sound(SND_MOVE);
        }
    }
    // Right: one more, up to what the bolts pay and the ammo takes
    if ((lwu(PAD_NOW) & BTN_RIGHT) != 0 || held_repeat(lw(HELD_RIGHT))) {
        if (round_price(cursor_item()) <= 0) return;
        const int32_t price = round_price(cursor_item());
        int32_t most = ee_div(lw(BOLTS), price);
        const int32_t item = cursor_item();
        const int32_t room = static_cast<int32_t>(lhu(item_rec(item) + 0xe)) - lw(AMMO + static_cast<uint32_t>(item) * 4);
        if (room < most) most = room;
        const int32_t q = lw(QUANTITY);
        if (q < most) {
            sw(QUANTITY, static_cast<uint32_t>(q + 1));
            if (held_sound(lw(HELD_RIGHT))) vendor_sound(SND_MOVE);
        }
    }
}

// orig: (helper) step 4 on Cross: the purchase
void purchase_buy() {
    const bool ammo = lw(cursor_entry() + E_AMMO) == 1;
    const int32_t cost = ammo ? round_price(cursor_item()) * lw(QUANTITY) : weapon_price(cursor_item());
    const int32_t bolts = lw(BOLTS);
    // (the original tests the entry's item's ammo when the bolts fall short, for a weapon too: a full one, or one with no
    // ammo at all, only plays the sound)
    if (bolts < cost && !ammo_short(cursor_item())) {
        vendor_sound(SND_BOX);
        return;
    }
    if (lw(cursor_entry() + E_AMMO) == 1) {
        if (!ammo_short(cursor_item())) {
            vendor_sound(SND_BOX);
            return;
        }
        const int32_t total = round_price(cursor_item()) * lw(QUANTITY);
        if (bolts < total) {
            vendor_sign_text(vendor_buy_all_text(SITE_NO_BOLTS, TXT(0x4f60)));
            vendor_sound(SND_BOX);
            return;
        }
        const int32_t item = cursor_item();
        const uint32_t have = AMMO + static_cast<uint32_t>(item) * 4;
        const int32_t most = lhu(item_rec(item) + 0xe);
        if (most < lw(have)) sw(have, static_cast<uint32_t>(most));
        const int32_t asked = lw(QUANTITY);
        const int32_t got = asked - ammo_add(lwu(cursor_entry() + E_ITEM), asked);
        const int32_t paid = round_price(cursor_item()) * got;
        const uint32_t bought = AMMO_BOUGHT + static_cast<uint32_t>(cursor_item()) * 4;
        sw(bought, static_cast<uint32_t>(lw(bought) + got));
        vendor_remark(3);
        sw(BOLTS, static_cast<uint32_t>(lw(BOLTS) - paid));
        item_list_build(lw(VS + PDA) > 0 ? 1 : 0);
        vendor_sign_text(vendor_buy_all_text(SITE_BOUGHT_AMMO, TXT(0x4f5f)));
        vendor_sound(SND_BOUGHT);
        return;
    }
    const int32_t price = weapon_price(cursor_item());
    if (bolts < price) {
        vendor_sign_text(vendor_buy_all_text(SITE_NO_BOLTS, TXT(0x4f60)));
        vendor_sound(SND_BOX);
        return;
    }
    vendor_remark(2);
    item_give(cursor_item(), 1);
    const int32_t weapon = cursor_item();
    sw(VS + BOUGHT, static_cast<uint32_t>(weapon));
    if (weapon == ITEM_DRONE_DEVICE) sb(HERO_DRONES, 1);
    sw(BOLTS, static_cast<uint32_t>(lw(BOLTS) - price));
    item_list_build(lw(VS + PDA) > 0 ? 1 : 0);
    if (lw(VS + AMMO_HUD) != -1) sub_002359f8(lwu(VS + AMMO_HUD), 0);
    sw(VS + AMMO_HUD, 0xffffffffu);
    if (lw(cursor_entry() + E_AMMO) == 1) {
        // the weapon's ammo now in its place
        sw(VS + AMMO_HUD, static_cast<uint32_t>(ammo_counter_request(cursor_item())));
    } else {
        const int32_t last = lw(VS + COUNT) - 1;
        if (last < lw(VS + CURSOR)) sw(VS + CURSOR, static_cast<uint32_t>(last));
        vendor_hologram_make();
    }
    vendor_sign_text(vendor_buy_all_text(SITE_BOUGHT_WEAPON, TXT(0x4f5f)));
    vendor_sound(SND_BOUGHT);
}
}  // namespace

namespace vendor {

// orig: 0x0029ba78
void vendor_purchase_update() {
    const int32_t step = lw(VS + STEP);
    if (step == 0) return;
    const auto box = [] { return lwu(VS + MOBYS) + 0x400; };
    if (step == 1) {
        purchase_box_open();
        return;
    }
    if (step == 2) {
        if (lw(QUANTITY_BOX) == 1) purchase_quantity();
        if (pressed(BTN_TRIANGLE)) {
            sw(VS + STEP, 3);
            sw(CONFIRMED, 0);
            box_close_anim(1);
        } else if (pressed(BTN_CROSS)) {
            sw(VS + STEP, 3);
            sw(CONFIRMED, 1);
            box_close_anim(8);
        } else {
            moby_anim_advance(box());
            const uint32_t mobys = lwu(VS + MOBYS);
            if (lbu(mobys + 0x453) == 0 && (lbu(mobys + 0x470) & 2) != 0) moby_anim_set(mobys + 0x400, 1, 0, 1);
        }
        moby_update_transform(box());
        return;
    }
    if (step == 3) {
        moby_anim_advance(box());
        const uint32_t mobys = lwu(VS + MOBYS);
        if (lbu(mobys + 0x453) == 0 && (lbu(mobys + 0x470) & 2) != 0) {
            swc1(mobys + 0x458, -1.0f);
            sw(VS + STEP, 4);
        }
        moby_update_transform(box());
        return;
    }
    if (step != 4) return;
    moby_anim_advance(box());
    if ((lbu(lwu(VS + MOBYS) + 0x470) & 2) != 0) {
        if (lw(CONFIRMED) != 1) vendor_sound(SND_BOX);
        else purchase_buy();
        sw(VS + STEP, 0);
    }
    moby_update_transform(box());
}

}  // namespace vendor

namespace {
// orig: (helper) the item list's update (state 1, no purchase and no closing under way): Left / Right move the cursor
// (a strip up to 7 entries, stopping at its ends; a wheel from 8, scrolling by 0x38 and wrapping), Cross starts a purchase
// on an entry that can be bought, Triangle closes the screens (not while the Vendor's animations are read)
void list_input() {
    if (lw(VS + COUNT) < 8) {
        if (pressed(BTN_RIGHT) && lw(VS + CURSOR) < lw(VS + COUNT) - 1) {
            sw(CURSOR_MOVED, 1);
            vendor_remark(0);
            sw(VS + CURSOR, static_cast<uint32_t>(lw(VS + CURSOR) + 1));
            vendor_sound(SND_MOVE);
            cursor_moved(SITE_STRIP_RIGHT);
        }
        if (pressed(BTN_LEFT) && lw(VS + CURSOR) > 0) {
            sw(CURSOR_MOVED, 1);
            vendor_remark(0);
            sw(VS + CURSOR, static_cast<uint32_t>(lw(VS + CURSOR) - 1));
            vendor_sound(SND_MOVE);
            cursor_moved(SITE_STRIP_LEFT);
        }
    } else {
        if (pressed(BTN_RIGHT) && lw(VS + SCROLL) < 57) {
            sw(VS + SCROLL, static_cast<uint32_t>(lw(VS + SCROLL) + 0x38));
            sw(CURSOR_MOVED, 1);
            vendor_sound(SND_MOVE);
            vendor_remark(0);
            const int32_t c = lw(VS + CURSOR);
            sw(VS + CURSOR, c < lw(VS + COUNT) - 1 ? static_cast<uint32_t>(c + 1) : 0u);
            cursor_moved(SITE_WHEEL_RIGHT);
        }
        if (pressed(BTN_LEFT) && lw(VS + SCROLL) >= -56) {
            sw(VS + SCROLL, static_cast<uint32_t>(lw(VS + SCROLL) - 0x38));
            sw(CURSOR_MOVED, 1);
            vendor_sound(SND_MOVE);
            vendor_remark(0);
            const int32_t c = lw(VS + CURSOR);
            sw(VS + CURSOR, static_cast<uint32_t>((c <= 0 ? lw(VS + COUNT) : c) - 1));
            cursor_moved(SITE_WHEEL_LEFT);
        }
    }
    if (pressed(BTN_CROSS) && lw(cursor_entry() + E_LOCKED) == 0) {
        sw(VS + STEP, 1);
        vendor_sound(SND_BOX);
    }
    if ((lwu(PAD_NOW) & BTN_TRIANGLE) != 0 && lw(ANIMS_READING) == 0) {
        sw(CLOSING, 1);
        sw(CLOSE_TIMER, 8);
        vendor_sound(SND_CLOSE);
    }
}

// orig: (helper) a hologram part (+0x200 / +0x300) placed on the stand: the stand's offset 0x1610a0 bobbing with the spin
// (z + sin(spin) / 20) plus the item's offset `ofs`, in the Vendor's frame
void hologram_part_place(uint32_t part, uint32_t ofs) {
    Vec4 p{};
    p.z = fpu_div(sin_vu(lwc1(SPIN)), 20.0f);
    vec3_add(p, V(OV(0x001610a0)), p);
    vec3_add(p, p, V(ofs));
    mat3_transform(V(part + 0x10), p, M(lwu(VS + VENDOR) + 0xc0));
    vec3_add(V(part + 0x10), V(part + 0x10), V(lwu(VS + VENDOR) + 0x10));
}

// orig: (helper) a ring moby (`ring` 0 / 0x500) of the screens: put at its offset in the Vendor's frame, turned
void ring_update(uint32_t ring, uint32_t ofs) {
    const uint32_t m = lwu(VS + MOBYS) + ring;
    mat3_transform(V(m + 0x10), V(ofs), M(lwu(VS + VENDOR) + 0xc0));
    vec3_add(V(lwu(VS + MOBYS) + ring + 0x10), V(lwu(VS + MOBYS) + ring + 0x10), V(lwu(VS + VENDOR) + 0x10));
    const float dx = frame_scale_f(std::bit_cast<float>(0xbd4ccccdu));  // -0.05
    swc1(lwu(VS + MOBYS) + ring + 0x40, angle_add(lwc1(lwu(VS + MOBYS) + ring + 0x40), dx));
    swc1(lwu(VS + MOBYS) + ring + 0x44, std::bit_cast<float>(0xbeb851ecu));  // -0.36
    const float dz = frame_scale_f(std::bit_cast<float>(0x3d4ccccdu));  // 0.05
    swc1(lwu(VS + MOBYS) + ring + 0x48, angle_add(lwc1(lwu(VS + MOBYS) + ring + 0x48), dz));
    moby_update_transform(lwu(VS + MOBYS) + ring);
}

// orig: (helper) a moby's animation advanced, set `set` over 8 frames when its current one ended
void anim_advance_loop(uint32_t m) {
    moby_anim_advance(m);
    if (lbu(m + 0x53) == 0 && (lbu(m + 0x70) & 2) != 0) moby_anim_set(m, 1, 0, 8);
}

// orig: (helper) state 1, the item list: the Vendor's head made once its animations are read (and unpacked),
// the Vendor and its head animated, the rings, the hologram, the list's input, the purchase, the closing's countdown
void list_update() {
    if (lw(HEAD_UP) == 0 && lh(STREAM_BUSY) == 0 && lw(ANIMS_READING) != 0) {
        vendor_seqs_unpack();
        const uint32_t head = lwu(VS + SCENE_B);
        sw(VS + HEAD, head);
        moby_init(head, 0xc);
        sh(lwu(VS + HEAD) + 0x32, 0x40);
        swc1(lwu(VS + HEAD) + 0x48, lwc1(lwu(VS + VENDOR) + 0x48));
        moby_update_transform(lwu(VS + HEAD));
        moby_set_color_packed(lwu(VS + HEAD), GREY, 0xe, 0xe, 0);
        sw(HEAD_UP, 1);
        sw(ANIMS_READING, 0);
        vendor_remark(1);
    }
    moby_anim_advance(lwu(VS + VENDOR));
    moby_update_transform(lwu(VS + VENDOR));
    if (lw(HEAD_UP) == 1) {
        mat3_transform(V(lwu(VS + HEAD) + 0x10), V(OV(0x00161080)), M(lwu(VS + VENDOR) + 0xc0));
        vec3_add(V(lwu(VS + HEAD) + 0x10), V(lwu(VS + HEAD) + 0x10), V(lwu(VS + VENDOR) + 0x10));
        moby_anim_advance(lwu(VS + HEAD));
        moby_update_transform(lwu(VS + HEAD));
        vendor_head_anim(lwu(VS + HEAD));
    }
    ring_update(0, OV(0x001610b0));
    ring_update(0x500, OV(0x001610c0));
    swc1(SPIN, angle_add(lwc1(SPIN), frame_scale_f(std::bit_cast<float>(0x3d4ccccdu))));

    // the hologram: the item (+0x100), its two parts (+0x200 / +0x300) bobbing and turning with the spin
    anim_advance_loop(lwu(VS + MOBYS) + 0x100);
    moby_update_transform(lwu(VS + MOBYS) + 0x100);
    const auto item = [] { return static_cast<uint32_t>(cursor_item()); };
    hologram_part_place(lwu(VS + MOBYS) + 0x200, OV(0x001c9010) + (item() << 6));
    {
        const float spin = lwc1(SPIN);
        const uint32_t m = lwu(VS + MOBYS);
        swc1(m + 0x240, lwc1(OV(0x001c8ff0) + (item() << 6) + 0x10));
        swc1(lwu(VS + MOBYS) + 0x244, lwc1(OV(0x001c8ff0) + (item() << 6) + 0x14));
        swc1(lwu(VS + MOBYS) + 0x248, spin);
        moby_update_transform(lwu(VS + MOBYS) + 0x200);
    }
    hologram_part_place(lwu(VS + MOBYS) + 0x300, OV(0x001c9950) + item() * 0x30);
    {
        const float spin = lwc1(SPIN);
        const uint32_t m = lwu(VS + MOBYS);
        swc1(m + 0x340, lwc1(OV(0x001c9930) + item() * 0x30 + 0x10));
        swc1(lwu(VS + MOBYS) + 0x344, lwc1(OV(0x001c9930) + item() * 0x30 + 0x14));
        swc1(lwu(VS + MOBYS) + 0x348, spin);
        anim_advance_loop(lwu(VS + MOBYS) + 0x300);
        moby_update_transform(lwu(VS + MOBYS) + 0x300);
        // the part's own offset turned by its angles
        const uint32_t rec = OV(0x001c9930) + item() * 0x30;
        Vec4 ofs{};
        ofs.x = lwc1(rec + 0);
        ofs.y = lwc1(rec + 4);
        ofs.z = lwc1(rec + 8);
        euler_to_mat3_vu(M(lwu(VS + MOBYS) + 0x3c0), V(lwu(VS + MOBYS) + 0x340));
        mat3_transform(ofs, ofs, M(lwu(VS + MOBYS) + 0x3c0));
        vec3_add(V(lwu(VS + MOBYS) + 0x310), V(lwu(VS + MOBYS) + 0x310), ofs);
    }

    sw(VS + FRAMES, static_cast<uint32_t>(lw(VS + FRAMES) + 1));
    if (lw(VS + STEP) == 0 && lw(CLOSING) == 0) list_input();
    vendor_purchase_update();
    sw(PAD_BEFORE, lwu(PAD_NOW));
    if (lw(OPENING) == 1) {
        const int32_t t = lw(OPEN_TIMER);
        if (t == 0) sw(OPENING, 0);
        else sw(OPEN_TIMER, static_cast<uint32_t>(t - 1));
    }
    if (lw(CLOSING) == 1) {
        const int32_t t = lw(CLOSE_TIMER) - 1;
        sw(CLOSE_TIMER, static_cast<uint32_t>(t));
        if (t == 0) {
            // the screens close: the Vendor's goodbye animation, the camera's rows, the moby of the hero's item in hand (if
            // any) given its class back
            sw(SKIP_DRAW, 1);
            sub_002a07c8();
            moby_anim_set(lwu(VS + VENDOR), 4, 9, 8);
            swc1(lwu(VS + VENDOR) + 0x58, fpu_mul(lwc1(FRAME_TIME), -0.5f));
            if (lw(VS + PDA) != 0) lq_sq(OV(0x00166dd0), VS + CAM_TARGET);
            const uint32_t held = lwu(HERO + 0x1090);
            const uint32_t r2 = OV(0x00165fa0), r0 = OV(0x00165f80), r1 = OV(0x00165f90);
            lq_sq(OV(0x00166ff0), r2);
            sw(VS + STATE, 2);
            lq_sq(OV(0x00166fd0), r0);
            lq_sq(OV(0x00166fe0), r1);
            sw(VS + FRAMES, 0);
            sw(VS + DRAW_MODE, 0);
            if (held != 0) {
                item_class_load(static_cast<uint32_t>(lh(held + 0xa6)), -1);
                const uint32_t m = lwu(HERO + 0x1090);
                sw(m + 0x24, lwu(OV(0x00197300) + static_cast<uint32_t>(lbu(m + 0x22)) * 4));
                sub_0024ebc0(lwu(HERO + 0x1090));
            }
        }
    } else if (lw(CURSOR_MOVED) != 0) {
        vendor_hologram_make();
    }
    hud_update(1);
}

// orig: (helper) state 3, a weapon's demo: the world's passes, the fade in (out near the scene's end), the scene's frame
// (its camera from the frame's record, in the demo's place; its mobys between two key positions), and at the scene's end
// its mobys retired and the screens left
void demo_update() {
    draw_lists_reset();
    moby_update_visible();
    sound_emitters_update();
    hero_frame_update();
    particles_update();
    const uint32_t sc = SCENE;
    sw(sc + 0x38, static_cast<uint32_t>(lw(sc + 0x38) + 1));
    const int32_t frame = lw(sc + 0x34) + 1;
    sw(sc + 0x34, static_cast<uint32_t>(frame));
    if (lh(sc + 0x40) - 12 < frame) {
        const float f = fpu_add(lwc1(FADE), std::bit_cast<float>(0x3dcccccdu));  // + 0.1
        swc1(FADE, f);
        if (1.0f < f) swc1(FADE, 1.0f);
    } else {
        const float f = fpu_sub(lwc1(FADE), 0.125f);
        swc1(FADE, f);
        if (f < 0.0f) swc1(FADE, 0.0f);
    }
    if (!(lw(sc + 0x34) < lh(sc + 0x40))) {
        // the scene's end
        swc1(FADE, 0.0f);
        sub_002a07c8();
        swc1(OV(0x0016caf0), std::bit_cast<float>(0x3f2147aeu));  // (the view's field: 0.63)
        projection_setup();
        for (int32_t i = 0; i < lh(sc + 0x44); ++i) {
            const uint32_t m = lwu(sc + 0x178 + static_cast<uint32_t>(i) * 4);
            if (m == 0) continue;
            const uint32_t cls = lwu(m + 0x24);
            sb(cls + 0xc, lbu(cls + 0xc) - 1u);
            const uint32_t cls2 = lwu(m + 0x24);
            sw(cls2 + static_cast<uint32_t>(lbu(cls2 + 0xc)) * 4 + 0x48, 0);
            moby_retire(m);
        }
        if (lw(VS + PLACE_MOBY) != 0) vendor_demo_exit();
        else vendor_screens_exit(1);
        return;
    }
    // the scene's blocks: a new one every 0x60 frames (0x50 at 50 Hz: the PAL build's video mode)
    const int32_t per_block = lw(VIDEO_50HZ) != 0 ? 0x50 : 0x60;
    if (!(lw(sc + 0x38) < per_block)) {
        const int32_t b = lw(sc + 0x3c) + 1;
        sw(sc + 0x3c, static_cast<uint32_t>(b));
        scene_block_select(b);
    }
    // the frame's camera record (0x20 bytes: +0 the position, +0xc the cut flag, +0x10 / +0x14 / +0x18 the angles, +0x1c
    // the field)
    const uint32_t rec = lwu(sc + 0x54) + static_cast<uint32_t>(lw(sc + 0x38)) * 32;
    const bool cut = lbu(rec + 0xc) != 0;
    swc1(OV(0x0016caf0), lwc1(rec + 0x1c));
    projection_setup();
    lq_sq(OV(0x00166dc0), rec);
    mat3_transform(V(OV(0x00166dc0)), V(OV(0x00166dc0)), M(VS + DEMO_MATRIX));
    vec3_add(V(OV(0x00166dc0)), V(OV(0x00166dc0)), V(VS + DEMO_POS));
    const float yaw = angle_add(lwc1(rec + 0x18), lwc1(VS + DEMO_ANGLES + 8));
    Mat4 rot;
    vu0_unit_matrix(rot);
    vu0_rot_matrix_x(rot, rot, lwc1(rec + 0x10));
    vu0_rot_matrix_y(rot, rot, lwc1(rec + 0x14));
    vu0_rot_matrix_z(rot, rot, yaw);
    // the camera's matrix rows (0x166fd0..): -row 2, -row 0, row 1
    swc1(OV(0x00166ff8), rot.r[1].z);
    swc1(OV(0x00166fd0), -rot.r[2].x);
    swc1(OV(0x00166fe0), -rot.r[0].x);
    swc1(OV(0x00166ff0), rot.r[1].x);
    swc1(OV(0x00166fd4), -rot.r[2].y);
    swc1(OV(0x00166fe4), -rot.r[0].y);
    swc1(OV(0x00166ff4), rot.r[1].y);
    swc1(OV(0x00166fd8), -rot.r[2].z);
    swc1(OV(0x00166fe8), -rot.r[0].z);
    // the scene's mobys: between their key positions (+0x78, 16 bytes each) k / 2 and k / 2 + 1, halfway on odd frames
    // (at the second with a cut)
    for (int32_t i = 0; i < lh(sc + 0x44); ++i) {
        const uint32_t m = lwu(sc + 0x178 + static_cast<uint32_t>(i) * 4);
        const int32_t k = lw(sc + 0x38) >> 1;
        sb(m + 0x51, static_cast<uint32_t>(k + 1));
        sb(m + 0x50, static_cast<uint32_t>(k));
        sub_0024ebc0(m);
        swc1(m + 0x54, fpu_mul(int_to_float(lw(sc + 0x38) & 1), 0.5f));
        if (cut && (lw(sc + 0x38) & 1) != 0) swc1(m + 0x54, 1.0f);
        const uint32_t keys = lwu(m + 0x78);
        Vec4 a, b;
        vec3_scale(a, V(keys + static_cast<uint32_t>(lbu(m + 0x50)) * 16), fpu_sub(1.0f, lwc1(m + 0x54)));
        vec3_scale(b, V(keys + static_cast<uint32_t>(lbu(m + 0x51)) * 16), lwc1(m + 0x54));
        vec3_add(V(m + 0x10), a, b);
        mat3_transform(V(m + 0x10), V(m + 0x10), M(VS + DEMO_MATRIX));
        vec3_add(V(m + 0x10), V(m + 0x10), V(VS + DEMO_POS));
        sb(m + 0x71, 0xff);
        swc1(m + 0x48, lwc1(VS + DEMO_ANGLES + 8));
        moby_update_transform(m);
        if (lbu(m + 0x7f) != 0) moby_shadow_probe(m);
        if (lh(m + 0xa6) == 0) hero_ship_update(m);
    }
    level_lights_update();
    frame_counters_advance();
}

// orig: (helper) states 0 and 2, the opening and the closing (the world's passes run unless the PDA opened the screens and
// they open): the fade out and the Vendor's greeting after 40 frames (at once for the PDA), or after 40 frames the Vendor
// idle, its arms' nodes released and the screens left (into the bought weapon's demo when it has one)
void open_close_update() {
    const int32_t state = lw(VS + STATE);
    if (lw(VS + PDA) == 0 || state == 2) {
        if (world_pass(2)) draw_lists_reset();
        if (world_pass(2)) {
            moby_update_visible();
            sound_emitters_update();
        }
        prof_mark(OV(0x00161128), 3);
        if (world_pass(1)) hero_frame_update();
        prof_mark(OV(0x00161138), 7);
        if (world_pass(4)) particles_update();
        prof_mark(OV(0x00161148), 5);
    }
    if (lw(VS + STATE) == 0) {
        const float f = fpu_sub(lwc1(FADE), std::bit_cast<float>(0x3eae147bu));  // - 0.34
        swc1(FADE, f);
        if (f < 0.0f) swc1(FADE, 0.0f);
        sw(VS + FRAMES, static_cast<uint32_t>(lw(VS + FRAMES) + 1));
        if (!(lw(VS + FRAMES) < frame_scale_i(0x28)) || lw(VS + PDA) != 0) {
            swc1(FADE, 0.0f);
            sw(VS + OPENED, 1);
            sw(VS + STATE, 1);
            sw(VS + FRAMES, 0);
            moby_anim_set(lwu(VS + VENDOR), 3, 0, 8);
            swc1(lwu(VS + VENDOR) + 0x58, 1.0f);
            sw(OPENING, 1);
            sw(OPEN_TIMER, 8);
            vendor_sound(SND_OPEN);
        }
    } else if (lw(VS + STATE) == 2) {
        bool demo = false;
        if (lw(VS + PDA) == 0 && lw(VS + BOUGHT) >= 0) demo = lw(OV(0x001ca020) + lwu(VS + BOUGHT) * 4) >= 0;
        sw(VS + FRAMES, static_cast<uint32_t>(lw(VS + FRAMES) + 1));
        if (!(lw(VS + FRAMES) < frame_scale_i(0x28)) || lw(VS + PDA) != 0) {
            moby_anim_set(lwu(VS + VENDOR), 1, 0, 8);
            for (uint32_t i = 0; i < 4; ++i) {
                const uint32_t node = JOINT_NODES + i * 0x40;
                if (lbu(node + 1) == 1) sub_0024f608(lwu(VS + VENDOR), node);
            }
            if (demo) {
                vendor_demo_start();
            } else {
                vendor_screens_exit(0);
                vendor_sound(SND_GOODBYE);
            }
        }
        hud_update(1);
    }
    if (!(frame_scale_i(0x24) < lw(VS + FRAMES))) draw_callback_add(OVF(0x0029f990), lwu(VS + VENDOR));
    if (lw(VS + PDA) == 0) {
        uid5_set_pvar_80(VS + CAM_POS);
        uid5_set_pvar_90(VS + CAM_TARGET);
    }
    level_lights_update();
    if (world_pass(2)) frame_counters_advance();
}
}  // namespace

// The screens' update, by their state: 0 / 2 the opening / the closing, 1 the item list, 3 a weapon's demo; then the
// sound's frame.
// orig: 0x0029c648
void vendor_mode_update_tr(uint32_t) {
    switch (lw(VS + STATE)) {
    case 0:
    case 2: open_close_update(); break;
    case 1: list_update(); break;
    case 3: demo_update(); break;
    default: break;
    }
    sound_frame();
}

}  // namespace rac1
