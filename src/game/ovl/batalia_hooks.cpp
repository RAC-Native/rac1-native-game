// See batalia_hooks.h.
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see batalia_hooks.h
bool batalia_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L8_CASE(0x003127d0): *ret = static_cast<int32_t>(bat_cam_type3_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L8_CASE(0x003127d8): bat_cam_type3_init_l8(cam); return true;
    case L8_CASE(0x00311d50): bat_cam_type3_update_l8(cam); return true;
    case L8_CASE(0x00312a28): bat_cam_type3_exit_l8(cam); return true;
    case L8_CASE(0x00316298): *ret = static_cast<int32_t>(bat_cam_type18_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L8_CASE(0x00316300): bat_cam_type18_init_l8(cam); return true;
    case L8_CASE(0x00316308): bat_cam_type18_update_l8(cam); return true;
    case L8_CASE(0x00316310): bat_cam_type18_exit_l8(cam); return true;
    default: return false;
    }
}

// orig: (host) see batalia_hooks.h
bool batalia_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L8_CASE(0x00316aa8): emitter_fade_box_update(em); return true;
    case L8_CASE(0x00316e48): emitter_reverb_box_update(em); return true;
    case L8_CASE(0x00316fa8): emitter_open_bed_update(em); return true;
    case L8_CASE(0x00317230): emitter_music_box_update(em); return true;
    default: return false;
    }
}

}  // namespace rac1
