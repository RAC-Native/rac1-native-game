// See rilgar_hooks.h.
#include <game/ovl/rilgar_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/rilgar_mobys.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_race_camera.h>

namespace rac1 {

// orig: (host) see rilgar_hooks.h
bool rilgar_camera_type_call(uint32_t fn, uint32_t cam, uint32_t /*other*/, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    // camera type 3 (the path camera, novalis_camera.cpp) and type 8 (the races', shared_race_camera.cpp): the shared ports;
    // both accept functions are a bare `return 0`
    case L5_CASE(0x00326280): *ret = zero_stub(); return true;
    case L5_CASE(0x00326288): cam_type3_init(cam); return true;
    case L5_CASE(0x00325800): cam_type3_update(cam); return true;
    case L5_CASE(0x003264d8): cam_type3_exit(cam); return true;
    case L5_CASE(0x0032a6d8): *ret = zero_stub(); return true;
    case L5_CASE(0x0032a6e0): race_cam_init(cam); return true;
    case L5_CASE(0x0032a758): race_cam_update(cam); return true;
    case L5_CASE(0x0032a788): empty_update(cam); return true;
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
    cam0_level_hook(0);
    return true;
}

// orig: (host) see rilgar_hooks.h
bool rilgar_camera_callback_call(uint32_t fn) {
    if (fn != L5_ADDR(0x00328fe0)) return false;
    cam0_level_callback(0);
    return true;
}

}  // namespace rac1
