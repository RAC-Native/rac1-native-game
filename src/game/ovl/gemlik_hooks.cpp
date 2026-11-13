// See gemlik_hooks.h.
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

namespace {
// a transliterated function in the uniform ABI (TrRet: every argument register) called with a0 / a1 / a2 and f12
TrRet u_call3(TrRet (*f)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float,
                         float, float, float),
              uint32_t a0, uint32_t a1, uint32_t a2, float f12) {
    return f(a0, a1, a2, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f);
}
}  // namespace

// orig: (host) see gemlik_hooks.h
bool gemlik_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L13_CASE(0x00317b00): *ret = static_cast<int32_t>(u_call3(gml_cam_type19_accept_u, cam, other, 0, 0.0f).v0); return true;
    case L13_CASE(0x00317060): gml_cam_type19_init_l13(cam); return true;
    case L13_CASE(0x00317420): gml_cam_type19_update_l13(cam); return true;
    default: return false;
    }
}

// orig: (host) see gemlik_hooks.h
bool gemlik_sound_emitter_call(uint32_t fn, uint32_t em) {
    if (fn != L13_ADDR(0x00318360)) return false;
    emitter_fade_box_update(em);
    return true;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_camera_hook_call(uint32_t fn) {
    if (fn != L13_ADDR(0x00316b48)) return false;
    gml_cam0_hook_l13(0);
    return true;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_camera_callback_call(uint32_t fn) {
    if (fn != L13_ADDR(0x00316ba8)) return false;
    gml_cam_callback_l13(0);
    return true;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    TrRet (*f)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float,
               float, float) = nullptr;
    switch (lvl_key(fn)) {
    case L13_CASE(0x002b7980): f = gml_hook_2b7980_u; break;
    case L13_CASE(0x002b79d0): f = gml_hook_2b79d0_u; break;
    case L13_CASE(0x002b7a20): f = gml_hook_2b7a20_u; break;
    case L13_CASE(0x002b7ad0): f = gml_hook_2b7ad0_u; break;
    case L13_CASE(0x002b7b00): f = gml_hook_2b7b00_u; break;
    default: return false;
    }
    ret = static_cast<int32_t>(u_call3(f, m, a1, a2, f12).v0);
    return true;
}

}  // namespace rac1
