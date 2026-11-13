// See veldin_orbit_hooks.h.
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L17_CASE(0x002fd208): *ret = static_cast<int32_t>(vo_cam_type3_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L17_CASE(0x002fd210): vo_cam_type3_init_l17(cam); return true;
    case L17_CASE(0x002fc788): vo_cam_type3_update_l17(cam); return true;
    case L17_CASE(0x002fd460): vo_cam_type3_exit_l17(cam); return true;
    default: return false;
    }
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L17_CASE(0x00300e70): emitter_camera_bed_update(em); return true;
    default: return false;
    }
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_hook_call(uint32_t fn) {
    if (fn != L17_ADDR(0x002fff38)) return false;
    vo_cam0_hook_l17(0);
    return true;
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_callback_call(uint32_t fn) {
    if (fn != L17_ADDR(0x002fff98)) return false;
    vo_cam_callback_l17(0);
    return true;
}

}  // namespace rac1
