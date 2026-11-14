// See kalebo_hooks.h.
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/shared_race_camera.h>
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see kalebo_hooks.h
bool kalebo_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    (void)other;  // (the level's type 3 / 8 accepts are bare `return 0`: zero_stub)
    *ret = 0;
    switch (lvl_key(fn)) {
    case L16_CASE(0x002f0d88): *ret = static_cast<int32_t>(zero_stub()); return true;
    case L16_CASE(0x002f0d90): cam_type3_init(cam); return true;
    case L16_CASE(0x002f0308): cam_type3_update(cam); return true;
    case L16_CASE(0x002f0fe0): cam_type3_exit(cam); return true;
    case L16_CASE(0x002f51e0): *ret = static_cast<int32_t>(zero_stub()); return true;
    case L16_CASE(0x002f51e8): race_cam_init(cam); return true;
    case L16_CASE(0x002f5260): race_cam_update(cam); return true;
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
    cam0_level_hook(0);
    return true;
}

// orig: (host) see kalebo_hooks.h
bool kalebo_camera_callback_call(uint32_t fn) {
    if (fn != L16_ADDR(0x002f3ae8)) return false;
    cam0_level_callback(0);
    return true;
}

}  // namespace rac1
