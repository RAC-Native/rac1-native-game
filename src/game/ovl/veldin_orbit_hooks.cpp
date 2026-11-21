// See veldin_orbit_hooks.h.
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/shared_helpers.h>
#include <game/ovl/sound_emitter.h>

namespace rac1 {

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    (void)other;  // (the type's accept ignores the other camera)
    switch (lvl_key(fn)) {
    case L17_CASE(0x002fd208): *ret = zero_stub(); return true;  // (the type's accept: v0 = 0, shared_helpers.cpp)
    case L17_CASE(0x002fd210): cam_type3_init(cam); return true;  // (every copy's port: novalis_camera.cpp)
    case L17_CASE(0x002fc788): cam_type3_update(cam); return true;
    case L17_CASE(0x002fd460): cam_type3_exit(cam); return true;
    default: return false;
    }
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L17_CASE(0x00300e70): emitter_camera_bed_update(em); return true;
    default: return false;
    }
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_hook_call(uint32_t fn) {
    if (fn != L17_ADDR(0x002fff38)) return false;
    cam0_level_hook(0);  // (every level's copy: kerwan_camera.cpp)
    return true;
}

// orig: (host) see veldin_orbit_hooks.h
bool veldin_orbit_camera_callback_call(uint32_t fn) {
    if (fn != L17_ADDR(0x002fff98)) return false;
    cam0_level_callback(0);
    return true;
}

}  // namespace rac1
