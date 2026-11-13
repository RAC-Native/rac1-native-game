// See hoven_hooks.h.
#include <game/ovl/hoven_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see hoven_hooks.h
bool hoven_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L12_CASE(0x00316680): emitter_fade_box_update(em); return true;
    case L12_CASE(0x00316a20): emitter_reverb_box_update(em); return true;
    case L12_CASE(0x00316b80): emitter_camera_bed_update(em); return true;
    case L12_CASE(0x00316c30): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see hoven_hooks.h
bool hoven_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    if (fn != L12_ADDR(0x00316478)) return false;
    *ret = static_cast<int32_t>(hov_cam_type18_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0);
    return true;
}

// orig: (host) see hoven_hooks.h
bool hoven_camera_hook_call(uint32_t fn) {
    if (fn != L12_ADDR(0x003148f0)) return false;
    hov_cam0_hook_l12(0);
    return true;
}

// orig: (host) see hoven_hooks.h
bool hoven_camera_callback_call(uint32_t fn) {
    if (fn != L12_ADDR(0x00314950)) return false;
    hov_cam_callback_l12(0);
    return true;
}

}  // namespace rac1
