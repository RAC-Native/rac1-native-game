// See umbris_hooks.h.
#include <game/ovl/umbris_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/umbris_mobys.h>

namespace rac1 {

// orig: (host) see umbris_hooks.h
bool umbris_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L7_CASE(0x003290b8): *ret = static_cast<int32_t>(umb_cam_type18_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L7_CASE(0x00329120): umb_cam_type18_init_l7(cam); return true;
    case L7_CASE(0x00329128): umb_cam_type18_update_l7(cam); return true;
    case L7_CASE(0x00329130): umb_cam_type18_exit_l7(cam); return true;
    default: return false;
    }
}

// orig: (host) see umbris_hooks.h
bool umbris_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L7_CASE(0x00329740): emitter_reverb_box_update(em); return true;
    case L7_CASE(0x003298a0): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see umbris_hooks.h
bool umbris_camera_hook_call(uint32_t fn) {
    if (fn != L7_ADDR(0x00328090)) return false;
    umb_cam0_hook_l7(0);
    return true;
}

// orig: (host) see umbris_hooks.h
bool umbris_camera_callback_call(uint32_t fn) {
    if (fn != L7_ADDR(0x003280f0)) return false;
    umb_cam_callback_l7(0);
    return true;
}

}  // namespace rac1
