// See pokitaru_hooks.h.
#include <game/ovl/pokitaru_hooks.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see pokitaru_hooks.h
bool pokitaru_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L11_CASE(0x00328b50): emitter_fade_box_update(em); return true;
    case L11_CASE(0x00328ef0): emitter_camera_bed_update(em); return true;
    default: return false;
    }
}

// orig: (host) see pokitaru_hooks.h
bool pokitaru_camera_hook_call(uint32_t fn) {
    if (fn != L11_ADDR(0x00327a90)) return false;
    pok_cam0_hook_l11(0);
    return true;
}

// orig: (host) see pokitaru_hooks.h
bool pokitaru_camera_callback_call(uint32_t fn) {
    if (fn != L11_ADDR(0x00327af0)) return false;
    pok_cam_callback_l11(0);
    return true;
}

}  // namespace rac1
