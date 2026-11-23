// See oltanis_hooks.h.
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/blarg_own.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_helpers.h>

namespace rac1 {

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L14_CASE(0x00314d48): *ret = cam_type19_accept(cam, other); return true;
    case L14_CASE(0x003142a8): cam_type19_init(cam); return true;
    case L14_CASE(0x00314668): cam_type19_update(cam); return true;
    case L14_CASE(0x00314e10): empty_update(cam); return true;
    case L14_CASE(0x00315920): *ret = olt_cam_type20_accept_l14(cam, other); return true;
    case L14_CASE(0x00314f00): olt_cam_type20_init_l14(cam); return true;
    case L14_CASE(0x00315290): olt_cam_type20_update_l14(cam); return true;
    case L14_CASE(0x003159d0): empty_update(cam); return true;
    case L14_CASE(0x00316748): *ret = olt_cam_type21_accept_l14(cam, other); return true;
    case L14_CASE(0x003159d8): olt_cam_type21_init_l14(cam); return true;
    case L14_CASE(0x00315d48): olt_cam_type21_update_l14(cam); return true;
    case L14_CASE(0x003167e0): olt_cam_type21_exit_l14(cam); return true;
    default: return false;
    }
}

// orig: (host) see oltanis_hooks.h
bool oltanis_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L14_CASE(0x00316990): emitter_fade_box_update(em); return true;
    case L14_CASE(0x00316d30): emitter_reverb_box_update(em); return true;
    case L14_CASE(0x00316e90): emitter_open_bed_update(em); return true;
    case L14_CASE(0x00317118): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_hook_call(uint32_t fn) {
    if (fn != L14_ADDR(0x003131e0)) return false;
    cam0_level_hook(0);
    return true;
}

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_callback_call(uint32_t fn) {
    if (fn != L14_ADDR(0x00313240)) return false;
    cam0_level_callback(0);
    return true;
}

// orig: (host) see oltanis_hooks.h
bool oltanis_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    switch (lvl_key(fn)) {
    case L14_CASE(0x002dad60): ret = enemy10e_hook0(m, a1, a2, f12); return true;  // (Veldin's 0x2cadc0, the same code)
    case L14_CASE(0x002dadb0): ret = enemy10e_hook4(m, a1, a2, f12); return true;  // (Veldin's 0x2cae10, the same code)
    case L14_CASE(0x002dae00): ret = carry_state_hook(m, a1, a2, f12, 8, 5, 1); return true;  // (shared_helpers.cpp)
    case L14_CASE(0x002dae50): ret = class252_hook_c(m, a1, a2, f12); return true;  // (shared_helpers.cpp)
    case L14_CASE(0x002daed8): ret = static_cast<int32_t>(class_hook_pvars60_l6(m)); return true;  // (L6:0x2ea800's code, not the map's 0x2d5800)
    case L14_CASE(0x002dd1a0): ret = enemy10e_hookc(m, a1, a2, f12); return true;  // (Veldin's 0x2caf10, the same code)
    default: return false;
    }
}

}  // namespace rac1
