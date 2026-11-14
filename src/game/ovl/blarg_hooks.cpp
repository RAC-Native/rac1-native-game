// See blarg_hooks.h.
#include <game/ovl/blarg_hooks.h>

#include <game/ovl/blarg_mobys.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

// orig: (host) see blarg_hooks.h
bool blarg_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L6_CASE(0x00316368): *ret = static_cast<int32_t>(blg_cam_type18_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0); return true;
    case L6_CASE(0x003163d0): empty_update(cam); return true;
    case L6_CASE(0x003163d8): empty_update(cam); return true;
    case L6_CASE(0x003163e0): empty_update(cam); return true;
    default: return false;
    }
}

// orig: (host) see blarg_hooks.h
bool blarg_camera_hook_call(uint32_t fn) {
    if (fn != L6_ADDR(0x003147b0)) return false;
    blg_cam0_hook_l6(0);
    return true;
}

// orig: (host) see blarg_hooks.h
bool blarg_camera_callback_call(uint32_t fn) {
    if (fn != L6_ADDR(0x00314810)) return false;
    blg_cam_callback_l6(0);
    return true;
}

}  // namespace rac1
