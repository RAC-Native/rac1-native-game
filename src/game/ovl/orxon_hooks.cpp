// See orxon_hooks.h.
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see orxon_hooks.h
bool orxon_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L10_CASE(0x002f65f8): *ret = static_cast<int32_t>(orx_cam_type19_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L10_CASE(0x002f5b58): orx_cam_type19_init_l10(cam); return true;
    case L10_CASE(0x002f5f18): orx_cam_type19_update_l10(cam); return true;
    case L10_CASE(0x002f66c0): orx_cam_type19_exit_l10(cam); return true;
    default: return false;
    }
}

// orig: (host) see orxon_hooks.h
bool orxon_sound_emitter_call(uint32_t fn, uint32_t em) {
    if (fn != L10_ADDR(0x002f6e58)) return false;
    emitter_reverb_box_update(em);
    return true;
}

// orig: (host) see orxon_hooks.h
bool orxon_camera_hook_call(uint32_t fn) {
    if (fn != L10_ADDR(0x002f4b18)) return false;
    orx_cam0_hook_l10(0);
    return true;
}

// orig: (host) see orxon_hooks.h
bool orxon_camera_callback_call(uint32_t fn) {
    if (fn != L10_ADDR(0x002f4b78)) return false;
    orx_cam_callback_l10(0);
    return true;
}

}  // namespace rac1
