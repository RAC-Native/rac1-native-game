// Novalis on its own overlay by default: see novalis_level_fns.h. Hand ports of the short functions
// (camera type, class 775) and the dispatch of Novalis's sound emitters (sound_emitter_types.cpp) and of
// the shared effects (shared_fx_tr.cpp).
#include <game/ovl/novalis_level_fns.h>

#include <game/ovl/help_panel_tr.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/novalis_mobys.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_fx_tr.h>
#include <game/ovl/swingshot_rope_tr.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/vendor_tr.h>

namespace rac1 {

// The accept function of Novalis's camera type 3 (row 1: accept 0x315dd8, init 0x315de0, update 0x315358, exit 0x316030;
// the other three are in novalis_camera.cpp): `jr ra; move v0,zero`. The camera is taken
// by its trigger kind 7 (camera_accept: the hero's camera request 0x1415d4 = 3) and handed back by its exit.
// orig: L1:0x00315dd8
int32_t camera_type_315dd8_l1() { return 0; }

// orig: L1:0x002ff810
void class775_update_l1(uint32_t m) {
    const float s = lwc1(m + 0x2c) * u2f(0x3f833333u);  // 1.025
    const uint32_t a = lbu(m + 0x23) - 3;
    sb(m + 0x23, a);
    swc1(m + 0x2c, s);
    if ((a & 0xffu) < 4) moby_retire(m);
}

// orig: (host) see novalis_level_fns.h
bool level_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t& ret) {
    if (on_kerwan()) {  // Kerwan's own camera types 1 and 14 and type 18's accept (kerwan_camera.cpp)
        switch (lvl_key(fn)) {
        case L3_CASE(0x002e8870): ret = cam_type1_accept(cam, other); return true;
        case L3_CASE(0x002e8940): cam_type1_init(cam); ret = 0; return true;
        case L3_CASE(0x002e8960): cam_type1_update(cam); ret = 0; return true;
        case L3_CASE(0x002e8980): cam_type1_exit(cam); ret = 0; return true;
        case L3_CASE(0x002ebd70): ret = ker_cam_type14_accept_l3(cam, other); return true;
        case L3_CASE(0x002eb978): ker_cam_type14_init_l3(cam); ret = 0; return true;
        case L3_CASE(0x002ebdb0): ker_cam_type14_update_l3(cam); ret = 0; return true;
        case L3_CASE(0x002ebde8): ker_cam_type14_exit_l3(cam); ret = 0; return true;
        case L3_CASE(0x002ed5f8): ret = cam_type18_accept(cam, other); return true;
        default: return false;
        }
    }
    if (!on_novalis()) return false;
    switch (lvl_key(fn)) {
    case L1_CASE(0x00315dd8): ret = camera_type_315dd8_l1(); return true;
    case L1_CASE(0x00315de0): cam_type3_init(cam); ret = 0; return true;
    case L1_CASE(0x00315358): cam_type3_update(cam); ret = 0; return true;
    case L1_CASE(0x00316030): cam_type3_exit(cam); ret = 0; return true;
    default: return false;
    }
}

// orig: (host) see novalis_level_fns.h
bool level_sound_emitter_call(uint32_t fn, uint32_t em) {
    if (!on_novalis()) return false;
    switch (lvl_key(fn)) {
    case L1_CASE(0x00319928): emitter_fade_box_update(em); return true;
    case L1_CASE(0x00319f18): emitter_reverb_box_update(em); return true;
    case L1_CASE(0x0031a078): emitter_camera_bed_update(em); return true;
    case L1_CASE(0x0031a128): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (none) -- see novalis_level_fns.h
void novalis_level_fns_register() { ovl_register_level_moby_fn(NOVALIS, L1_ADDR(0x002ff810), class775_update_l1); }

// orig: (none) -- see novalis_level_fns.h
void shared_fx_tr_register() {
    ovl_register_moby_fn(0x00269d20, particle_06_update);
    ovl_register_moby_fn(0x002a5040, vendor_draw_tr);
    ovl_register_moby_fn(0x002c5dc8, swingshot_rope_draw_tr);  // (the swingshot's rope)
    // the vendor screens' draw callback, queued by their update in the opening / closing (`draw_callback_add(0x29f990,
    // *(0x1ca4dc))` at 0x29c9ec) and called by the draw list's `jalr` with a0 = that argument
    ovl_register_moby_fn(0x0029f990, vendor_screens_glass_cb);
    ovl_register_moby_fn(0x00237190, help_panel_init_tr);  // (the help message panel: help_hint_update's request id 0xc)
    ovl_register_moby_fn(0x002371e0, help_panel_update_tr);
    ovl_register_moby_fn(0x00237200, help_panel_draw_tr);
    ovl_register_moby_fn(0x00272838, particle_38_update);
    ovl_register_moby_fn(0x002728e8, particle_39_update);
    ovl_register_moby_fn(0x0023db30, cutscene_l14_effect_draw_tr);  // (queued by cutscene_level_extras's level 14 branch)
}

}  // namespace rac1
