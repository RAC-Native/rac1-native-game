// See batalia_hooks.h.
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/sound_emitter.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/shared_helpers.h>

namespace rac1 {

// orig: (host) see batalia_hooks.h
bool batalia_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L8_CASE(0x003127d0): *ret = zero_stub(); return true;
    case L8_CASE(0x003127d8): cam_type3_init(cam); return true;
    case L8_CASE(0x00311d50): cam_type3_update(cam); return true;
    case L8_CASE(0x00312a28): cam_type3_exit(cam); return true;
    case L8_CASE(0x00316298): *ret = cam_type18_accept(cam, other); return true;
    case L8_CASE(0x00316300): empty_update(cam); return true;
    case L8_CASE(0x00316308): empty_update(cam); return true;
    case L8_CASE(0x00316310): empty_update(cam); return true;
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
