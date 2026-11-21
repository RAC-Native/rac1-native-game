// See gemlik_hooks.h.
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/sound_emitter.h>

#include <game/ovl/blarg_own.h>
#include <game/ovl/carry_hooks.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/gaspar_mobys.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/shared_cam19.h>
#include <game/ovl/shared_helpers.h>
namespace rac1 {

// Class 63's hook +8 (the trooper let go by what carried it): its carry record first (the class's hook +0x10, 0x2b7b00: pvars + 0x60),
// then Veldin's carry hook +8; carried: the state 7, let go from 7: the state 1; the record's +0x68 at 6: the sound 2 on the moby.
// orig: L13:0x002b7a20
int32_t gml_hook_2b7a20_l13(uint32_t m, uint32_t a1, uint32_t a2, float f12) {
    const uint32_t rec = class_hook_10(m);
    const int32_t r = carry_hook8(m, a1, a2, f12);
    if (r != 0) sb(m + 0x20, 7);
    else if (lbu(m + 0x20) == 7) sb(m + 0x20, 1);
    if (rec != 0 && lh(rec + 0x68) == 6) moby_sound_start(2, 0, m);
    return r;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_camera_type_call(uint32_t fn, uint32_t cam, uint32_t other, int32_t* ret) {
    *ret = 0;
    switch (lvl_key(fn)) {
    // camera type 19: the shared port (shared_cam19.cpp)
    case L13_CASE(0x00317b00): *ret = cam_type19_accept(cam, other); return true;
    case L13_CASE(0x00317060): cam_type19_init(cam); return true;
    case L13_CASE(0x00317420): cam_type19_update(cam); return true;
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
    cam0_level_hook(0);
    return true;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_camera_callback_call(uint32_t fn) {
    if (fn != L13_ADDR(0x00316ba8)) return false;
    cam0_level_callback(0);
    return true;
}

// orig: (host) see gemlik_hooks.h
bool gemlik_class_hook_call(uint32_t fn, uint32_t m, uint32_t a1, uint32_t a2, float f12, int32_t& ret) {
    switch (lvl_key(fn)) {
    // (+0 / +4 / +0xc / +0x10: word for word Gaspar's class 193's hooks L9:0x2e2680 / 0x2e26d0 / 0x2e2770 / 0x2e27a0 and Kalebo III's,
    // masked for relocations: their shared ports; +0x10 is Blarg's L6:0x2ea800 too)
    case L13_CASE(0x002b7980): ret = carry_state_hook(m, a1, a2, f12, 0, 7, 1); return true;  // (shared_helpers.cpp)
    case L13_CASE(0x002b79d0): ret = carry_state_hook(m, a1, a2, f12, 4, 7, 1); return true;
    case L13_CASE(0x002b7a20): ret = gml_hook_2b7a20_l13(m, a1, a2, f12); return true;
    case L13_CASE(0x002b7ad0): ret = gas_hook_2e2770_l9(m, a1, a2, f12); return true;  // (gaspar_carry.cpp)
    case L13_CASE(0x002b7b00): ret = static_cast<int32_t>(class_hook_pvars60_l6(m)); return true;  // (blarg_own.cpp)
    default: return false;
    }
}

}  // namespace rac1
