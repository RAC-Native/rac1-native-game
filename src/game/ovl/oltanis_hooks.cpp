// See oltanis_hooks.h.
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/empty_class_updates.h>

namespace rac1 {

namespace {
using UFn = TrRet (*)(uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, uint32_t, float, float, float, float, float, float,
                      float, float);
// a transliterated function in the uniform ABI (TrRet: every argument register) called with a0 / a1 / a2 and f12
TrRet u_call3(UFn f, uint32_t a0, uint32_t a1, uint32_t a2, float f12) { return f(a0, a1, a2, 0, 0, 0, 0, 0, f12, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f); }
}  // namespace

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    case L14_CASE(0x00314d48): *ret = static_cast<int32_t>(u_call3(olt_cam_type19_accept_u, cam, other, 0, 0.0f).v0); return true;
    case L14_CASE(0x003142a8): olt_cam_type19_init_l14(cam); return true;
    case L14_CASE(0x00314668): olt_cam_type19_update_l14(cam); return true;
    case L14_CASE(0x00314e10): empty_update(cam); return true;
    case L14_CASE(0x00315920): *ret = static_cast<int32_t>(u_call3(olt_cam_type20_accept_u, cam, other, 0, 0.0f).v0); return true;
    case L14_CASE(0x00314f00): olt_cam_type20_init_l14(cam); return true;
    case L14_CASE(0x00315290): olt_cam_type20_update_l14(cam); return true;
    case L14_CASE(0x003159d0): empty_update(cam); return true;
    case L14_CASE(0x00316748): *ret = static_cast<int32_t>(u_call3(olt_cam_type21_accept_u, cam, other, 0, 0.0f).v0); return true;
    case L14_CASE(0x003159d8): olt_cam_type21_init_l14(cam); return true;
    case L14_CASE(0x00315d48): olt_cam_type21_update_l14(cam); return true;
    case L14_CASE(0x003167e0): olt_cam_type21_exit_l14(cam); return true;
    default: return false;
    }
}

// orig: (host) see oltanis_hooks.h
bool oltanis_sound_emitter_call(uint32_t fn, uint32_t em) {
    switch (lvl_key(fn)) {
    case L14_CASE(0x00316990): emitter_fade_box_update(em); return true;
    case L14_CASE(0x00316d30): emitter_reverb_box_update(em); return true;
    case L14_CASE(0x00316e90): emitter_open_bed_update(em); return true;
    case L14_CASE(0x00317118): emitter_music_box_update(em); return true;
    default: return false;
    }
}

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_hook_call(uint32_t fn) {
    if (fn != L14_ADDR(0x003131e0)) return false;
    olt_cam0_hook_l14(0);
    return true;
}

// orig: (host) see oltanis_hooks.h
bool oltanis_camera_callback_call(uint32_t fn) {
    if (fn != L14_ADDR(0x00313240)) return false;
    olt_cam_callback_l14(0);
    return true;
}

// orig: (host) see oltanis_hooks.h
bool oltanis_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    UFn f = nullptr;
    switch (lvl_key(fn)) {
    case L14_CASE(0x002dad60): ret = enemy10e_hook0(m, a1, a2, f12); return true;  // (Veldin's 0x2cadc0, the same code)
    case L14_CASE(0x002dadb0): ret = enemy10e_hook4(m, a1, a2, f12); return true;  // (Veldin's 0x2cae10, the same code)
    case L14_CASE(0x002dae00): f = olt_hook_2dae00_u; break;
    case L14_CASE(0x002dae50): f = olt_hook_2dae50_u; break;
    case L14_CASE(0x002daed8): f = olt_hook_2daed8_u; break;
    case L14_CASE(0x002dd1a0): ret = enemy10e_hookc(m, a1, a2, f12); return true;  // (Veldin's 0x2caf10, the same code)
    default: return false;
    }
    ret = static_cast<int32_t>(u_call3(f, m, a1, a2, f12).v0);
    return true;
}

}  // namespace rac1
