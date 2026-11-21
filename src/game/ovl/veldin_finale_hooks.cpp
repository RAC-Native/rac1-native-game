// See veldin_finale_hooks.h.
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see veldin_finale_hooks.h
bool veldin_finale_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L18_CASE(0x00306370): *ret = cam_type18_accept(cam, other); return true;  // (the port of every level's copy, kerwan_camera.cpp)
    default: return false;
    }
}

// orig: (host) see veldin_finale_hooks.h
bool veldin_finale_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L18_CASE(0x00306dd0): emitter_reverb_box_update(em); return true;
    case L18_CASE(0x00306f30): emitter_camera_bed_update(em); return true;
    case L18_CASE(0x00306fe0): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see veldin_finale_hooks.h
bool veldin_finale_camera_hook_call(uint32_t fn) {
    if (fn != L18_ADDR(0x00305318)) return false;
    cam0_level_hook(0);  // (the port of every level's copy, kerwan_camera.cpp)
    return true;
}

// orig: (host) see veldin_finale_hooks.h
bool veldin_finale_camera_callback_call(uint32_t fn) {
    if (fn != L18_ADDR(0x00305378)) return false;
    cam0_level_callback(0);  // (the port of every level's copy, kerwan_camera.cpp)
    return true;
}

}  // namespace rac1
