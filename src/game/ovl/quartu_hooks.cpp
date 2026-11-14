// See quartu_hooks.h.
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

// orig: (host) see quartu_hooks.h
bool quartu_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L15_CASE(0x002f84d8): *ret = static_cast<int32_t>(qua_cam_type19_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L15_CASE(0x002f7a38): qua_cam_type19_init_l15(cam); return true;
    case L15_CASE(0x002f7df8): qua_cam_type19_update_l15(cam); return true;
    case L15_CASE(0x002f85a0): empty_update(cam); return true;
    case L15_CASE(0x002f8ba8): *ret = static_cast<int32_t>(qua_cam_type22_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L15_CASE(0x002f8c10): empty_update(cam); return true;
    case L15_CASE(0x002f8c18): empty_update(cam); return true;
    case L15_CASE(0x002f8c20): empty_update(cam); return true;
    default: return false;
    }
}

// orig: (host) see quartu_hooks.h
bool quartu_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L15_CASE(0x002f9230): emitter_reverb_box_update(em); return true;
    case L15_CASE(0x002f9390): emitter_camera_bed_update(em); return true;
    default: return false;
    }
}

// orig: (host) see quartu_hooks.h
bool quartu_camera_hook_call(uint32_t fn) {
    if (fn != L15_ADDR(0x002f6980)) return false;
    qua_cam0_hook_l15(0);
    return true;
}

// orig: (host) see quartu_hooks.h
bool quartu_camera_callback_call(uint32_t fn) {
    if (fn != L15_ADDR(0x002f69e0)) return false;
    qua_cam_callback_l15(0);
    return true;
}

}  // namespace rac1
