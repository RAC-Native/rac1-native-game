// Quartu (level 15): the level's two story triggers and the scene subtitle they draw, ported by hand from the level's own
// code.
//   class 1388  the item pickup of the level (item 0x22): turns, its panel lights blink; the hero within 3 of it starts
//               scene 5; once the scene is over a help message, the item given and a level change request; gone when the
//               item is already held (0x13d50a)
//   class 1419  the level's end: with its checkpoint not collected yet and the hero in its box while a linked moby (its
//               pvars +8) is in its state 3: scene 3 and the checkpoint marked, then movie 0x11, scene 4 (the hero placed at
//               its box +4), the level marked visited, the checkpoint saved and the level change to level 17. In a scene
//               (0x15f5c4 == 2) it queues the subtitle callback 0x2eb6b8
//   0x2eb6b8    the subtitle of the scene's shots 3 and 4: one of four texts typed out at a letter every four frames
//               inside its window of the scene's frames (0x16ce94), its cursor blinking once all is shown; a shadow then
//               the text (draw_detector_text)
// The original's order of calls is kept. Header quartu_mobys.h.
#include <game/ovl/quartu_mobys.h>

#include <game/lib/libc.h>
#include <game/lib/libgcc_fp.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/checkpoint.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/helmet_tr.h>
#include <game/ovl/hud_slot7_tr.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/quartu_own.h>
#include <game/ovl/shared_panel_lights.h>
#include <game/ovl/variants_l15.h>
#include <game/ovl/veldin_mobys.h>

namespace rac1 {

using namespace quartu;

namespace {
bool in_scene() { return lw(L15_ADDR(0x0015f5c4u)) == 2; }
uint64_t sx64(int32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(v)); }
}  // namespace

// orig: L15:0x002ea748
void qua_class1388_update_l15(uint32_t m) {
    swc1(m + 0x48, angle_add(lwc1(m + 0x48), fpu_mul(frame_time(), u2f(0x3fc90fdb))));  // (pi / 2 a second)
    swc1(m + 0x2c, fpu_mul(lwc1(lwu(m + 0x24) + 0x24), lwc1(L15_ADDR(0x00162078u))));
    switch (lbu(m + 0x20)) {
    case 0:  // set up
        helmet_init_fx(m);
        if (lbu(0x0013d50a) != 0) {  // (the item is held already)
            moby_retire(m);
            return;
        }
        sb(m + 0x20, 1);
        swc1(m + 0x18, fpu_add(lwc1(m + 0x18), 1.0f));
        return;
    case 1:  // waiting for the hero
        panel_lights(m, 0x10u);  // (the level's copy 0x2ea9b0: the lights' table at the pvars' +0x10)
        if (vec2_distance(V(m + 0x10), V(HERO_POS)) < 3.0f) {
            sh(m + 0x34, lhu(m + 0x34) | 0x41);
            cutscene_start(5);
            sb(m + 0x20, 2);
        }
        return;
    case 2:  // the scene: once over, the item
        if (in_scene()) return;
        sub_00263d40(TXT(0x3aa3), -1);
        sw(L15_ADDR(0x0015f640u), static_cast<uint32_t>(frame_scale_i(0xb4)));
        item_give(0x22, 1);
        level_change_request(0, -1);
        sb(m + 0x20, 3);
        return;
    case 3:
        moby_retire(m);
        return;
    default:
        return;
    }
}

// orig: L15:0x002eb4c0
void qua_class1419_update_l15(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);  // +0 the trigger box, +4 the box the hero is placed at, +8 the linked moby
    const uint32_t state = lbu(m + 0x20);
    sb(m + 0x30, 0x80);
    switch (state) {  // (the table L15:0x1e3580)
    case 0: {  // (0x002eb508) waiting: its checkpoint not collected, the hero in its box, the linked moby in its state 3
        const uint32_t idx = lbu(m + 0xb0) + (lwu(0x0015ed84) << 4);
        if (lbu(0x0014c050 + idx) == 0xff) break;
        if (lw(L15_ADDR(0x00161ac0u)) != 0) break;
        if (point_in_box(HERO_POS, lw(pv + 0x0)) == 0) break;
        if (lbu(lwu(OV(0x0015ffd8)) + (lwu(pv + 0x8) << 8) + 0x20) != 3) break;
        cutscene_start(3);
        sb(m + 0x20, 2);
        checkpoint_mark_collected(lbu(m + 0xb0));
        sub_00232fe8();
        break;
    }
    case 2:  // (0x002eb59c) the scene over: the movie
        if (in_scene()) break;
        movie_play(0x11);
        sb(m + 0x20, 1);
        break;
    case 1:  // (0x002eb5c4) the movie over: scene 4, the hero placed
        if (in_scene()) break;
        cutscene_start(4);
        sb(m + 0x20, 3);
        sub_002330d0();
        if (const int32_t box = lw(pv + 0x4); box != -1) {
            const uint32_t b = lvd::box(lvd::BOXES, box);
            hero_place(b + 0x30, b + 0x70, 0, 1);
        }
        break;
    case 3:  // (0x002eb620) the scene over: the level left
        if (in_scene()) break;
        level_mark_visited(0x11);
        if (const int32_t box = lw(pv + 0x4); box != -1) {
            const uint32_t b = lvd::box(lvd::BOXES, box);
            checkpoint_save(b + 0x30, b + 0x70);
        }
        level_arrival_message(0x11);
        level_change_request(0, -1);
        sb(m + 0x20, 4);
        break;
    default:  // (0x002eb67c: 4, and 5 and above)
        break;
    }
    if (in_scene()) sub_001f93e0(L15_ADDR(0x002eb6b8u), m);
}

// orig: L15:0x002eb6b8
void qua_cb_2eb6b8_l15(uint32_t) {
    const Frame frame(0xb0);
    const uint32_t text = frame.sp;  // the text, written by sprintf
    const int32_t shot = lw(L15_ADDR(0x0016ce90u));
    bool shown = false;
    int32_t typed = 0;
    // the text `id` while the scene's frame is inside (from, to): typed from `from`
    auto window = [&](int32_t from, int32_t to, int32_t id) {
        if (!(frame_scale_i(from) < lw(L15_ADDR(0x0016ce94u)))) return;
        if (!(lw(L15_ADDR(0x0016ce94u)) < frame_scale_i(to))) return;
        shown = true;
        libc_sprintf(text, L15_ADDR(0x00162190u), {static_cast<uint64_t>(sub_00203ad8(TXT(id)))});  // ("%s")
        typed = static_cast<int32_t>(lwu(L15_ADDR(0x0016ce94u)) - static_cast<uint32_t>(frame_scale_i(from))) >> 2;
    };
    if (shot == 3) {
        window(0xb5, 0x104, 0x526a);
        window(0x1c2, 0x280, 0x526b);
        window(0x2ee, 0x3a2, 0x526c);
    } else if (shot == 4) {
        window(0x8fc, 0x960, 0x526e);
    }
    if (!shown) return;
    int32_t n = libc_strlen(text);
    if (typed < n) n = typed;
    else if (n < typed && ((n - typed) & 4) != 0) n = n - 1;  // (all typed: the cursor blinks)
    draw_detector_text(lw(L15_ADDR(0x00162180u)) + 1, lw(L15_ADDR(0x00162184u)) + 1, sx64(lw(L15_ADDR(0x0016218cu))), text, n);
    draw_detector_text(lw(L15_ADDR(0x00162180u)), lw(L15_ADDR(0x00162184u)), sx64(lw(L15_ADDR(0x00162188u))), text, n);
}

}  // namespace rac1
