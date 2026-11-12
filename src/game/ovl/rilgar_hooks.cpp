// See rilgar_hooks.h.
#include <game/ovl/rilgar_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/rilgar_mobys.h>

namespace rac1 {

// orig: (host) see rilgar_hooks.h
bool rilgar_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L5_CASE(0x00326280): *ret = static_cast<int32_t>(ril_cam_type3_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L5_CASE(0x00326288): ril_cam_type3_init_l5(cam); return true;
    case L5_CASE(0x00325800): ril_cam_type3_update_l5(cam); return true;
    case L5_CASE(0x003264d8): ril_cam_type3_exit_l5(cam); return true;
    case L5_CASE(0x0032a6d8): *ret = static_cast<int32_t>(ril_cam_type8_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L5_CASE(0x0032a6e0): ril_cam_type8_init_l5(cam); return true;
    case L5_CASE(0x0032a758): ril_cam_type8_update_l5(cam); return true;
    case L5_CASE(0x0032a788): ril_cam_type8_exit_l5(cam); return true;
    default: return false;
    }
}

// orig: (host) see rilgar_hooks.h
bool rilgar_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L5_CASE(0x0032b478): emitter_fade_box_update(em); return true;
    case L5_CASE(0x0032ba68): emitter_reverb_box_update(em); return true;
    case L5_CASE(0x0032bbc8): emitter_camera_bed_update(em); return true;
    case L5_CASE(0x0032bc78): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see rilgar_hooks.h
bool rilgar_camera_hook_call(uint32_t fn) {
    if (fn != L5_ADDR(0x00328f80)) return false;
    ril_cam0_hook_l5(0);
    return true;
}

// orig: (host) see rilgar_hooks.h
bool rilgar_camera_callback_call(uint32_t fn) {
    if (fn != L5_ADDR(0x00328fe0)) return false;
    ril_cam_callback_l5(0);
    return true;
}

}  // namespace rac1
