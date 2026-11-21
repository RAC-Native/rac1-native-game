// See orxon_hooks.h.
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

// orig: (host) see orxon_hooks.h
bool orxon_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L10_CASE(0x002f65f8): *ret = cam_type19_accept(cam, other); return true;
    case L10_CASE(0x002f5b58): cam_type19_init(cam); return true;
    case L10_CASE(0x002f5f18): cam_type19_update(cam); return true;
    case L10_CASE(0x002f66c0): empty_update(cam); return true;
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
    cam0_level_hook(0);  // (the port of every level's copy, kerwan_camera.cpp)
    return true;
}

// orig: (host) see orxon_hooks.h
bool orxon_camera_callback_call(uint32_t fn) {
    if (fn != L10_ADDR(0x002f4b78)) return false;
    cam0_level_callback(0);  // (the port of every level's copy, kerwan_camera.cpp)
    return true;
}

}  // namespace rac1
