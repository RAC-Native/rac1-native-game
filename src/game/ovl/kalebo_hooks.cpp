// See kalebo_hooks.h.
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see kalebo_hooks.h
bool kalebo_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L16_CASE(0x002f0d88): *ret = static_cast<int32_t>(kal_cam_type3_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L16_CASE(0x002f0d90): kal_cam_type3_init_l16(cam); return true;
    case L16_CASE(0x002f0308): kal_cam_type3_update_l16(cam); return true;
    case L16_CASE(0x002f0fe0): kal_cam_type3_exit_l16(cam); return true;
    case L16_CASE(0x002f51e0): *ret = static_cast<int32_t>(kal_cam_type8_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L16_CASE(0x002f51e8): kal_cam_type8_init_l16(cam); return true;
    case L16_CASE(0x002f5260): kal_cam_type8_update_l16(cam); return true;
    default: return false;
    }
}

// orig: (host) see kalebo_hooks.h
bool kalebo_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L16_CASE(0x002f58a0): emitter_fade_box_update(em); return true;
    case L16_CASE(0x002f5c40): emitter_reverb_box_update(em); return true;
    case L16_CASE(0x002f5da0): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see kalebo_hooks.h
bool kalebo_camera_hook_call(uint32_t fn) {
    if (fn != L16_ADDR(0x002f3a88)) return false;
    kal_cam0_hook_l16(0);
    return true;
}

// orig: (host) see kalebo_hooks.h
bool kalebo_camera_callback_call(uint32_t fn) {
    if (fn != L16_ADDR(0x002f3ae8)) return false;
    kal_cam_callback_l16(0);
    return true;
}

}  // namespace rac1
