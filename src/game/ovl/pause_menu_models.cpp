// Ported overlay code: the 3D item panels of the Weapons / Gadgets pages (see pause_menu_pages.h
//): the focused item's model (and its ammo's) spawned as menu
// mobys turning in front of the menu camera, the ammo count text; the hero panel's draw.
#include <game/enhance/fixes102.h>
#include <game/lib/libc.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001b9cf0u> MS{};    // the menu's state
constexpr OvConst<0x00166dc0u> CAM{};   // the camera's position (the menu's: (256, 256, 64))
constexpr OvConst<0x00179ac0u> ITEMS{};  // the item table (0x4c bytes per item)
constexpr OvConst<0x001c4508u> PLACE{};  // per item: its model's offsets (0x20 bytes)
constexpr float PI_F = 3.14159274f;     // 0x40490fdb
// orig: (helper) the item of the focused grid's selected cell
int32_t focused_item() {
    const uint32_t fw = lwu(lwu(MS + 4) + 0x40);
    return lh(lwu(fw + 0x48) + static_cast<uint32_t>(lw(fw + 0x3c) * 10) + 6);
}
// orig: (helper) repeated in 0x0027be78: animation `set` of a menu moby, at most its class's last
void anim_clamped(uint32_t m, int32_t set) {
    const int32_t last = static_cast<int32_t>(lbu(lwu(m + 0x24) + 0xc)) - 1;
    moby_set_anim(m, static_cast<uint32_t>(set < last ? set : last), 0);
}
// orig: (helper) `jal 0x0024ff08` with a1 = 1 (0x27bb3c, 0x27bdb8, 0x27bdcc, 0x27c2b0, 0x27c2c4): one moby
// drawn by the moby pass (moby_draw_list) into the widget's render target
void moby_draw_vu(uint32_t m) { moby_draw_list(m, 1); }
}  // namespace

// A text box filling widget w's box, centred (flags 0x10).
// orig: 0x002845c0
void widget_text_box(uint32_t b, uint32_t w) {
    sh(b, 0);
    sh(b + 2, lhu(w + 0x24));
    sh(b + 4, 0);
    sh(b + 6, lhu(w + 0x20));
    sh(b + 8, static_cast<uint32_t>(lw(w + 0x20) >> 1));
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 0);
    sh(b + 0xa, static_cast<uint32_t>(lw(w + 0x24) >> 1));
}

// An item panel's focus in: nothing shown yet (+0x34 state, +0x44 / +0x48 its mobys), the
// turn angle +0x38 = pi.
// orig: 0x0027bb78
int32_t menu_model_focus_in(uint32_t w) {
    sw(w + 0x48, 0);
    swc1(w + 0x38, PI_F);
    sw(w + 0x34, 0);
    sw(w + 0x44, 0);
    return 0;
}

// An item panel's focus out: its mobys freed (the original frees the first one twice: the second, +0x48, stays until
// the menu frees its mobys).
// orig: 0x0027bba0
int32_t menu_model_focus_out(uint32_t w) {
    const uint32_t a = static_cast<uint32_t>(menu_moby_free(lwu(w + 0x44)));
    // (1.02 Fixes, fix 30: the 1.02 build, 2.00 0x0027cb80, frees the second moby +0x48, not the first call's result)
    const uint32_t second = fixes102_on(30) ? lwu(w + 0x48) : a;
    sw(w + 0x44, a);
    sw(w + 0x48, static_cast<uint32_t>(menu_moby_free(second)));
    return 0;
}

// The item panel: the focused item's model (its class +0x10 of the item table, 0x1df for the
// item 0x18) turning (+0x38, 0.01 a frame). State 0 / 1 wait for a class, 2 spawns it (the
// held items, slots other than 1..3, load their class into the hero's item buffers 0x245770,
// gold ones recoloured 0x207330; slot 3 adds the ammo class of the item 0x17 +0x5c), placed by
// the table 0x1c4508, updated by 0x27c2e0; 3 shows it until the item changes (back to 2).
// orig: 0x0027be78
int32_t menu_model_update(uint32_t w) {
    const int32_t item = focused_item();
    const uint32_t cur = lwu(w + 0x44);
    const int32_t cur_cls = cur != 0 ? lh(cur + 0xa6) : -1;
    const uint32_t it = ITEMS + static_cast<uint32_t>(item * 0x4c);
    uint32_t cls = lwu(it + 0x10);
    if ((item ^ 0x18) == 0) cls = 0x1df;
    swc1(w + 0x38, angle_add(lwc1(w + 0x38), u2f(0x3c23d70a)));
    const uint32_t slot = lwu(it + 8);
    const bool ammo = slot == 3;
    bool held = slot != 2 && !ammo && slot != 1;
    if ((item ^ 0x18) == 0) held = false;
    const bool alt = (lwu(w + 0x30) & 1) != 0;
    const int32_t st = lw(w + 0x34);
    if (st == 1) {
        if (cls != 0xffffffffu) sw(w + 0x34, 2);
        return 0;
    }
    if (st < 2) {
        if (st == 0) sw(w + 0x34, 1);
        return 0;
    }
    if (st == 3) {
        if (static_cast<uint32_t>(cur_cls) == cls) return 0;
        const uint32_t a = static_cast<uint32_t>(menu_moby_free(lwu(w + 0x44)));
        const uint32_t b = static_cast<uint32_t>(menu_moby_free(lwu(w + 0x48)));
        sw(w + 0x44, a);
        sw(w + 0x34, 2);
        sw(w + 0x48, b);
        return 0;
    }
    if (st != 2) return 0;
    if (cls == 0xffffffffu) {
        sw(w + 0x34, 1);
        return 0;
    }
    if (held) {
        const uint32_t h = lwu(0x00140408);
        if (h != 0 && cls != h) hero_slot_close(0, 0);
        if (cls != lwu(MS + 0x11c)) {
            item_class_load(cls, lw(MS + 0x118) == 0 ? 1 : 0);
            sw(MS + 0x144, lwu(OV(0x00160010)));
            sw(MS + 0x120, cls);
            sw(MS + 0x140, cls);
            sb(lwu(OV(0x00197300) + static_cast<uint32_t>(lbu(OV(0x00197bc0) + cls)) * 4) + 0xd, 0);
        }
    }
    const uint32_t m = menu_moby_spawn(cls);
    const int32_t set = (item ^ 2) == 0 ? 6 : 1;
    if (m != 0) {
        if (held && lbu(0x0013e520 + static_cast<uint32_t>(item)) != 0) sub_00207330(m, lwu(w + 0x14));
        sw(w + 0x44, m);
        sh(m + 0x34, 0);
        const uint32_t t = PLACE + static_cast<uint32_t>(item) * 0x20;
        swc1(m + 0x10, fpu_add(lwc1(CAM), alt ? lwc1(t) : lwc1(t + 4)));
        swc1(m + 0x14, fpu_add(lwc1(CAM + 4), lwc1(t + 8)));
        swc1(m + 0x18, fpu_add(lwc1(CAM + 8), lwc1(t + 0xc)));
        swc1(m + 0x40, lwc1(t + 0x10));
        sw(m + 0x74, OVF(0x0027c2e0));
        swc1(m + 0x48, PI_F);
        swc1(m + 0x44, lwc1(t + 0x14));
        const uint32_t d = lwu(m + 0x78);
        sw(d + 0xc, static_cast<uint32_t>(item));
        sw(d, w);
        anim_clamped(m, set);
    }
    if (ammo && m != 0) {
        const uint32_t m2 = menu_moby_spawn(lwu(ITEMS + 0x5c));
        if (m2 != 0) {
            sh(m2 + 0x34, 0);
            const uint32_t m1 = lwu(w + 0x44);
            lq_sq(m2 + 0x10, m1 + 0x10);
            lq_sq(m2 + 0x40, m1 + 0x40);
            sw(w + 0x48, m2);
            sw(m2 + 0x74, OVF(0x0027c2e0));
            const uint32_t d = lwu(m2 + 0x78);
            sw(d + 0xc, static_cast<uint32_t>(item));
            sw(d, w);
            anim_clamped(m2, set);
        }
    }
    sw(w + 0x34, 3);
    return 0;
}

// An item model's update (+0x74): its panel's turn angle, its place (the table 0x1c4508: +0 / +4
// x with / without the panel's flag 1, +8 y, +0xc z) with the turn's offset (+0x18 / +0x1c).
// orig: 0x0027c2e0
void menu_item_moby_update(uint32_t m) {
    const uint32_t d = lwu(m + 0x78);
    const uint32_t w = lwu(d);
    const int32_t item = lw(d + 0xc);
    swc1(m + 0x48, lwc1(w + 0x38));
    const uint32_t t = PLACE + static_cast<uint32_t>(item << 5);
    swc1(m + 0x10, fpu_add(lwc1(CAM), (lwu(w + 0x30) & 1) != 0 ? lwc1(t) : lwc1(t + 4)));
    swc1(m + 0x14, fpu_add(lwc1(CAM + 4), lwc1(t + 8)));
    swc1(m + 0x18, fpu_add(lwc1(CAM + 8), lwc1(t + 0xc)));
    float a = lwc1(t + 0x1c);
    float b = lwc1(t + 0x18);
    const float c = cos_vu(lwc1(m + 0x48));
    const float s = sin_vu(lwc1(m + 0x48));
    const float bs = fpu_mul(b, s);
    b = fpu_mul(b, c);
    a = fpu_mul(a, c);
    const float na = fpu_mul(-lwc1(t + 0x1c), s);
    a = fpu_add(a, bs);
    const float x = fpu_add(na, b);
    swc1(m + 0x14, fpu_add(lwc1(m + 0x14), a));
    swc1(m + 0x10, fpu_add(lwc1(m + 0x10), x));
}

// An item panel's draw: its mobys (the moby pass, 0x24ff08); nothing (0) for an item not owned.
// orig: 0x0027c250
int32_t menu_model_draw(uint32_t w) {
    if (lbu(static_cast<uint32_t>(focused_item()) + 0x0013d4c0) == 0) return 0;
    if (lwu(w + 0x44) != 0) moby_draw_vu(lwu(w + 0x44));
    if (lwu(w + 0x48) != 0) moby_draw_vu(lwu(w + 0x48));
    return 8;
}

// The ammo panel: the focused item's ammo model (class +0x3a of the item table) in front of the
// camera (x + 6, z - 0.3), turning (0x27be40); replaced when the class changes, freed with none.
// orig: 0x0027bbe0
int32_t menu_ammo_update(uint32_t w) {
    const int32_t item = focused_item();
    const uint32_t cur = lwu(w + 0x44);
    const int32_t cur_cls = cur != 0 ? lh(cur + 0xa6) : -1;
    const int32_t cls = lh(ITEMS + static_cast<uint32_t>(item * 0x4c) + 0x3a);
    if (cls == -1) {
        sw(w + 0x44, static_cast<uint32_t>(menu_moby_free(lwu(w + 0x44))));
        return 0;
    }
    if (cur_cls == -1) {
        const uint32_t m = menu_moby_spawn(static_cast<uint32_t>(cls));
        if (m == 0) return 0;
        sw(w + 0x44, m);
        sh(m + 0x34, 0);
        const uint32_t d = lwu(m + 0x78);
        swc1(m + 0x10, fpu_add(lwc1(CAM), 6.0f));
        swc1(m + 0x14, lwc1(CAM + 4));
        swc1(m + 0x48, PI_F);
        sw(m + 0x74, OVF(0x0027be40));
        swc1(m + 0x18, fpu_sub(lwc1(CAM + 8), u2f(0x3e99999a)));
        sw(d, w);
        return 0;
    }
    if (cur_cls == cls) return 0;
    const uint32_t m = menu_moby_spawn(static_cast<uint32_t>(cls));
    if (m != 0) {
        sh(m + 0x34, 0);
        const uint32_t old = lwu(w + 0x44);
        lq_sq(m + 0x10, old + 0x10);
        lq_sq(m + 0x40, old + 0x40);
        sw(m + 0x74, lwu(old + 0x74));
        sw(lwu(m + 0x78), w);
    }
    menu_moby_free(lwu(w + 0x44));
    sw(w + 0x44, m);
    return 0;
}

// The ammo model's update: turning 0.01 a frame.
// orig: 0x0027be40
void menu_ammo_moby_update(uint32_t m) { swc1(m + 0x48, angle_add(lwc1(m + 0x48), u2f(0x3c23d70a))); }

// The ammo panel's draw: its mobys (the moby pass, 0x24ff08), or with none a text (0x4f4d) in the box; nothing for an
// item not owned.
// orig: 0x0027bd58
int32_t menu_ammo_draw(uint32_t w) {
    if (lbu(static_cast<uint32_t>(focused_item()) + 0x0013d4c0) == 0) return 0;
    if (lwu(w + 0x44) != 0) {
        moby_draw_vu(lwu(w + 0x44));
        if (lwu(w + 0x48) == 0) return 8;
        moby_draw_vu(lwu(w + 0x48));
        return 8;
    }
    texture_block_begin(0);
    const uint32_t b = ee_stack_alloc(0x20);
    widget_text_box(b, w);
    sh(b + 0x10, 0x10);
    sh(b + 0x12, 3);
    text_box_small(b, 0x80ffa888ull, sub_00203ad8(0x4f4d), -1);
    ee_stack_free(0x20);
    texture_block_end();
    return 2;
}

// The ammo count: "n / max" of the focused weapon (0x13d428, the table 0x1c40b0 +0xe), with
// thousands (0x160380 / 0x160390), or a text (0x4f52) for a weapon without ammo (+8 0).
// orig: 0x0027cda0
int32_t menu_ammo_count_draw(uint32_t w) {
    const int32_t item = focused_item();
    if (lbu(static_cast<uint32_t>(item) + 0x0013d4c0) == 0) return 0;
    const uint32_t e = OV(0x001c40b0) + static_cast<uint32_t>(item * 0x18);
    const int32_t n = lw(0x0013d428 + static_cast<uint32_t>(item) * 4);
    const int32_t max = lhu(e + 0xe);
    sub_002a0ba8(0x42, 0x44);
    sub_002a0ba8(0x47, 0xb);
    const uint32_t buf = ee_stack_alloc(0x70);  // the text +0, the box +0x50
    if (lhu(e + 8) == 0) {
        libc_sprintf(buf, sub_00203ad8(0x4f52), std::vector<uint64_t>());
    } else {
        int32_t len;
        std::vector<uint64_t> a;
        if (n < 1000) {
            a.push_back(static_cast<uint64_t>(static_cast<int64_t>(n)));
            len = libc_sprintf(buf, OV(0x00160378), a);
        } else {
            a.push_back(static_cast<uint64_t>(static_cast<int64_t>(n / 1000)));
            a.push_back(static_cast<uint64_t>(static_cast<int64_t>(n % 1000)));
            len = libc_sprintf(buf, OV(0x00160380), a);
        }
        std::vector<uint64_t> b;
        if (max < 1000) {
            b.push_back(static_cast<uint64_t>(static_cast<int64_t>(max)));
            libc_sprintf(buf + static_cast<uint32_t>(len), OV(0x00160360), b);
        } else {
            b.push_back(static_cast<uint64_t>(static_cast<int64_t>(max / 1000)));
            b.push_back(static_cast<uint64_t>(static_cast<int64_t>(max % 1000)));
            libc_sprintf(buf + static_cast<uint32_t>(len), OV(0x00160390), b);
        }
    }
    texture_block_begin(0);
    widget_text_box(buf + 0x50, w);
    sh(buf + 0x60, 0x10);
    sh(buf + 0x62, 3);
    text_box_small(buf + 0x50, 0x80ffa888ull, buf, -1);
    ee_stack_free(0x70);
    texture_block_end();
    return 2;
}

// The hero panel's draw: its 24 mobys (+0x44..) by the moby pass (0x24ff08), those hidden
// (+0xa4 + i) or not active (0x1b9120) skipped, some of the weapon's parts (slots 1..12) only in
// a texel range of their +0x50 colour; 4.
// orig: 0x0027ba40
int32_t menu_hero_draw(uint32_t w) {
    static const bool range_case[12] = {true, true, true, false, true, true, false, false, false, true, true, true};  // the jump table 0x1e9240
    for (uint32_t i = 0; i < 0x18; ++i) {
        const uint32_t m = lwu(w + 0x44 + i * 4);
        if (m == 0) continue;
        if (lbu(w + i + 0xa4) != 0) continue;
        if (lwu(OV(0x001b9120) + i * 4) == 0) continue;
        if (i == 7 && lh(lwu(w + 0x60) + 0xa6) == 0x4a) {
            const uint32_t h = lwu(w + 0x44);
            if (lbu(h + 0x52) != lbu(h + 0x53)) continue;
        }
        const uint32_t k = i - 1;
        if (k < 12u && range_case[k]) {
            const uint32_t h = lwu(w + 0x44);
            if ((ld(h + 0x50) & 0xffff0000ull) == 0x99990000ull) {
                const uint32_t v = lbu(h + 0x50);
                if (!(v < 0x4d) && v < 0x92) continue;
            }
        }
        moby_draw_vu(lwu(w + 0x44 + i * 4));
    }
    return 4;
}

}  // namespace rac1
