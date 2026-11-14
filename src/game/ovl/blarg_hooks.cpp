// See blarg_hooks.h.
#include <game/ovl/blarg_hooks.h>

#include <game/ovl/blarg_mobys.h>
#include <game/ovl/empty_class_updates.h>
#include <game/ovl/kerwan_camera.h>

namespace rac1 {

// orig: (host) see blarg_hooks.h
bool blarg_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L6_CASE(0x00316368): *ret = cam_type18_accept(cam, other); return true;
    case L6_CASE(0x003163d0): empty_update(cam); return true;
    case L6_CASE(0x003163d8): empty_update(cam); return true;
    case L6_CASE(0x003163e0): empty_update(cam); return true;
    default: return false;
    }
}

// orig: (host) see blarg_hooks.h
bool blarg_camera_hook_call(uint32_t fn) {
    if (fn != L6_ADDR(0x003147b0)) return false;
    cam0_level_hook(0);
    return true;
}

// orig: (host) see blarg_hooks.h
bool blarg_camera_callback_call(uint32_t fn) {
    if (fn != L6_ADDR(0x00314810)) return false;
    cam0_level_callback(0);
    return true;
}

}  // namespace rac1
