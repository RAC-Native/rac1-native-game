// Ported overlay code: the camera system (0x1eb188..0x1ee6b8).
//
// 48 cameras at 0x167150 (0xa0 each, live flags 0x169890): rows +0x00/+0x10/+0x20, position
// +0x30, the drawn position +0x64, the work block +0x70, the switch priority +0x7c, the
// switch state +0x7d/+0x7e, the level camera index +0x84, the type id +0x86, the row of the
// type table 0x1ea880 +0x8c (0x14 each: id, then the accept / init / update / exit functions),
// the active flag +0x8e. The active camera *(0x166e00), the previous one *(0x166e04); the view
// the renderer reads: position 0x166dc0, Euler angles 0x166dd0, rows 0x166fd0..0x167000.
#include <game/ovl/camera_types.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/variants_l3.h>
#include <game/ovl/eudora_mobys.h>
#include <game/ovl/rilgar_hooks.h>
#include <game/ovl/blarg_hooks.h>
#include <game/ovl/umbris_hooks.h>
#include <game/ovl/gaspar_hooks.h>
#include <game/ovl/batalia_hooks.h>
#include <game/ovl/orxon_hooks.h>
#include <game/ovl/pokitaru_hooks.h>
#include <game/ovl/hoven_hooks.h>
#include <game/ovl/gemlik_hooks.h>
#include <game/ovl/oltanis_hooks.h>
#include <game/ovl/quartu_hooks.h>
#include <game/ovl/kalebo_hooks.h>
#include <game/ovl/veldin_orbit_hooks.h>
#include <game/ovl/veldin_finale_hooks.h>
#include <game/ovl/variants_l2.h>
#include <game/ovl/novalis_level_fns.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <cstdio>
#include <cstdlib>

namespace rac1 {

// (see ovl.h: none until a session's hero context makes one)
uint32_t g_camera_types_table = 0;

namespace {
constexpr OvConst<0x00167150u> CAMS{}; constexpr OvConst<0x00169890u> LIVE{}; constexpr OvConst<0x001ea880u> TYPES{};
constexpr OvConst<0x00166e00u> ACTIVE{}; constexpr OvConst<0x00166e04u> PREV{};
// orig: (helper) a 32-bit EE word as the int32_t& the timer helpers take
int32_t& ref32(uint32_t a) { return *static_cast<int32_t*>(ee_ram_ptr(a)); }
// orig: (helper) the type table: the overlay's, or (enhancement: a session's hero context, hero_modes.cpp) a larger copy in the
// session heap with that level's own camera types added
uint32_t types_base() { return g_camera_types_table != 0 ? g_camera_types_table : static_cast<uint32_t>(TYPES); }
// orig: (helper) the type-table function `col` (4 accept, 8 init, 0xc update, 0x10 exit) of a camera
uint32_t type_fn(uint32_t cam, uint32_t col) { return lwu(types_base() + static_cast<uint32_t>(lh(cam + 0x8c) * 0x14) + col); }
// orig: (helper) the level camera entry (0x20 each at *(0x15ef50)) of a camera
uint32_t level_entry(uint32_t cam) { return lwu(OV(0x0015ef50)) + static_cast<uint32_t>(lh(cam + 0x84) * 0x20); }
// orig: (helper) the three unit rows of the hero's matrix *(0x1413d0) + 0xc0
void hero_rows(Vec4& r0, Vec4& r1, Vec4& r2) {
    vec3_set_length(r0, V(lwu(0x001413d0) + 0xc0), 1.0f);
    vec3_set_length(r1, V(lwu(0x001413d0) + 0xd0), 1.0f);
    vec3_set_length(r2, V(lwu(0x001413d0) + 0xe0), 1.0f);
}
// orig: (host) RAC1_CAM_TRACE=1 (testing): every camera switch on stderr
void cam_trace(const char* what, uint32_t old, uint32_t cam) {
    static const bool on = std::getenv("RAC1_CAM_TRACE") != nullptr;
    if (!on) return;
    auto idx = [](uint32_t c) { return c == 0 ? -1 : static_cast<int>((c - CAMS) / 0xa0); };
    std::fprintf(stderr, "[cam] %s f%u: cam %d (type %d, trig %u, prio %u, req %d) -> cam %d (type %d, trig %u, prio %u, kind %d) hero %.2f %.2f %.2f\n", what,
                 lwu(OV(0x00167018)), idx(old), old ? lh(old + 0x86) : -1, old ? lwu(old + 0x74) : 0u, old ? lbu(old + 0x7c) : 0u,
                 old ? lh(old + 0x7e) : 0, idx(cam), lh(cam + 0x86), lwu(cam + 0x74), lbu(cam + 0x7c),
                 lwu(level_entry(cam) + 0x1c) ? static_cast<int>(lbu(lwu(level_entry(cam) + 0x1c) + 0x1d)) : -1,
                 static_cast<double>(lwc1(0x0013f3d0)), static_cast<double>(lwc1(0x0013f3d4)), static_cast<double>(lwc1(0x0013f3d8)));
}
// orig: (host) RAC1_CAM_TRACE=1: a change of the hero's camera request 0x1415d4 or of the active camera's request +0x7e
void cam_trace_frame(uint32_t cur) {
    static const bool on = std::getenv("RAC1_CAM_TRACE") != nullptr;
    if (!on) return;
    static int32_t last_req = -999, last_7e = -999;
    static uint32_t last_cur = 0;
    const int32_t req = lw(0x001415d4), r7e = cur ? lh(cur + 0x7e) : -1;
    if (req != last_req || r7e != last_7e || cur != last_cur || lwu(OV(0x00167018)) % 30 == 0)
        std::fprintf(stderr, "[cam] f%u: active cam %d (type %d) +0x7e %d, hero camera request 0x1415d4 = %d, hero %.2f %.2f %.2f\n", lwu(OV(0x00167018)),
                     cur ? static_cast<int>((cur - CAMS) / 0xa0) : -1, cur ? lh(cur + 0x86) : -1, r7e, req, static_cast<double>(lwc1(0x0013f3d0)),
                     static_cast<double>(lwc1(0x0013f3d4)), static_cast<double>(lwc1(0x0013f3d8)));
    last_req = req;
    last_7e = r7e;
    last_cur = cur;
}
// orig: (host) RAC1_CAM_TRACE=1: the level's cameras at the level start
void cam_trace_list() {
    static const bool on = std::getenv("RAC1_CAM_TRACE") != nullptr;
    if (!on) return;
    std::fprintf(stderr, "[cam] slots: follow-camera hook %08x, callback queue %08x\n", OV(0x00167010), OV(0x00168ff0));
    for (int32_t i = 0; i < lw(OV(0x0015ef54)); ++i) {
        const uint32_t cam = CAMS + static_cast<uint32_t>(i) * 0xa0;
        const uint32_t e = level_entry(cam), data = lwu(e + 0x1c);
        std::fprintf(stderr, "[cam] level cam %d: type %d row %d (fns %08x %08x %08x %08x) trig %u prio %u kind %u pos %.1f %.1f %.1f data %08x box %08x\n", i,
                     lh(cam + 0x86), lh(cam + 0x8c), type_fn(cam, 4), type_fn(cam, 8), type_fn(cam, 0xc), type_fn(cam, 0x10), lwu(cam + 0x74),
                     lbu(cam + 0x7c), lbu(data + 0x1d), static_cast<double>(lwc1(e)), static_cast<double>(lwc1(e + 4)), static_cast<double>(lwc1(e + 8)), data,
                     lwu(data + 0xc));
        if (lw(data + 0xc) >= 0) {  // (its box, 0x80 each at *(0x1600ec): the centre +0x30)
            const uint32_t box = lwu(OV(0x001600ec)) + static_cast<uint32_t>(lw(data + 0xc)) * 0x80;
            std::fprintf(stderr, "[cam]   box %d at %.1f %.1f %.1f\n", lw(data + 0xc), static_cast<double>(lwc1(box + 0x30)), static_cast<double>(lwc1(box + 0x34)),
                         static_cast<double>(lwc1(box + 0x38)));
        }
        std::fprintf(stderr, "[cam]   data:");
        for (uint32_t k = 0; k < 0x40; k += 4) std::fprintf(stderr, " %08x", lwu(data + k));
        std::fprintf(stderr, "\n");
        if (lh(cam + 0x86) == 3 && lw(data + 0x24) >= 0) {
            const uint32_t t = lwu(OV(0x0015f70c)) + static_cast<uint32_t>(lw(data + 0x24)) * 0x20;
            std::fprintf(stderr, "[cam]   0x15f70c[%d]:", lw(data + 0x24));
            for (uint32_t k = 0; k < 0x20; k += 4) std::fprintf(stderr, " %08x", lwu(t + k));
            std::fprintf(stderr, "\n");
        }
        if (lh(cam + 0x86) == 3 && on_novalis() && lw(data + 0x20) >= 0) {  // (Novalis's type 3: its path, *(0x1b0930)[data +0x20])
            const uint32_t path = lwu(L1_ADDR(0x001b0930u) + static_cast<uint32_t>(lw(data + 0x20)) * 4);
            std::fprintf(stderr, "[cam]   path %d (%d points):", lw(data + 0x20), lw(path));
            for (int32_t k = 0; k < lw(path) && k < 64; ++k)
                std::fprintf(stderr, " (%.1f %.1f %.1f)", static_cast<double>(lwc1(path + 0x10 + static_cast<uint32_t>(k) * 0x10)),
                             static_cast<double>(lwc1(path + 0x14 + static_cast<uint32_t>(k) * 0x10)), static_cast<double>(lwc1(path + 0x18 + static_cast<uint32_t>(k) * 0x10)));
            std::fprintf(stderr, "\n");
        }
    }
}
}  // namespace

// The camera type functions of the table at 0x1ea880, called through their EE addresses.
// orig: (helper) jalr to a camera type function: accept (cam, current) returns -1 / 0 / 1
int32_t camera_type_call(uint32_t fn, uint32_t cam, uint32_t other) {
    if (on_camera_level(BATALIA)) {  // level 8's own camera types (3 and 0x12: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (batalia_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(OLTANIS)) {  // level 14's own camera types (0x13, 0x14, 0x15: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (oltanis_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(GEMLIK)) {  // level 13's own camera type (0x13: accept / init / update at its own addresses)
        int32_t r = 0;
        if (gemlik_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(QUARTU)) {  // level 15's own camera types (0x13 and 0x16: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (quartu_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(KALEBO)) {  // level 16's own camera types (3 and 8: accept / init / update (/ exit) at its own addresses)
        int32_t r = 0;
        if (kalebo_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(ORXON)) {  // level 10's own camera type (0x13: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (orxon_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(VELDIN_ORBIT)) {  // level 17's own camera type (3: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (veldin_orbit_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(VELDIN_FINALE)) {  // level 18's own camera type 0x12's accept (its init / update / exit are Veldin's)
        int32_t r = 0;
        if (veldin_finale_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(UMBRIS)) {  // level 7's own camera type (0x12: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (umbris_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(BLARG)) {  // level 6's own camera type (0x12: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (blarg_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(RILGAR)) {  // Rilgar's own camera types (3 and 8: accept / init / update / exit at its own addresses)
        int32_t r = 0;
        if (rilgar_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(HOVEN)) {  // level 12's own camera type function (type 0x12's accept; its init / update / exit are Veldin's)
        int32_t r = 0;
        if (hoven_camera_type_call(fn, cam, other, &r)) return r;
    }
    if (on_camera_level(EUDORA)) {  // Eudora's own camera type (1: accept / init / update / exit at its own addresses, no Veldin equivalent)
        switch (lvl_key(fn)) {
        case L4_CASE(0x002ed850): return static_cast<int32_t>(eud_cam_type1_accept_u(cam, other, 0, 0, 0, 0, 0, 0, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f, 0.0f).v0);
        case L4_CASE(0x002ed920): eud_cam_type1_init_l4(cam); return 0;
        case L4_CASE(0x002ed940): eud_cam_type1_update_l4(cam); return 0;
        case L4_CASE(0x002ed960): eud_cam_type1_exit_l4(cam); return 0;
        default: break;
        }
    }
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
    case 0x002e9c20: return cam0_accept(cam, other);
    case 0x002e6d60: cam0_init(cam); return 0;
    case 0x002e9c28: cam0_update(cam); return 0;
    case 0x002e9cb8: cam0_exit(cam); return 0;
    case 0x002ea488: return cam_look_accept(cam, other);
    case 0x002ea7a0: cam_look_init(cam); return 0;
    case 0x002e9f38: cam_look_update(cam); return 0;
    case 0x002ea810: cam_look_exit(cam); return 0;
    case 0x002ead60: cam5_init(cam); return 0;
    case 0x002eb218: cam5_update(cam); return 0;
    case 0x002ebb00: cam6_init(cam); return 0;
    case 0x002ebbb0: cam6_update(cam); return 0;
    case 0x002ebbd8: cam7_init(cam); return 0;
    case 0x002ec6c0: cam7_update(cam); return 0;
    case 0x002ec7b0: cam7_exit(cam); return 0;
    case 0x002ed230: return cam_mod5_accept(cam);
    case 0x002ed8b0: return cam_mod6_accept(cam);
    case 0x002ed330: case 0x002ed338: case 0x002ed340:  // the modifiers' init / update / exit: empty
    case 0x002ed938: case 0x002ed940: case 0x002ed948: return 0;
    case 0x002eb210: case 0x002ebaf8: case 0x002ec6b8: return 0;  // types 2 / 3 / 4's accept: v0 = 0
    case 0x002eb270: case 0x002ebbd0: return 0;                   // types 2 / 3's exit: empty
    default:
        // Aridia's own type 0x12 (its accept function: the modifier 6's, forked): the port of every level's copy (kerwan_camera.cpp)
        if (on_camera_level(ARIDIA) && fn == L2_ADDR(0x002fc298)) return cam_type18_accept(cam, other);
        if (int32_t r = 0; level_camera_type_call(fn, cam, other, r)) return r;  // (a level's own camera type)
        TODO_PORT(fn, "camera type function")(cam, other);
        return 0;
    }
}

// orig: (helper) jalr to one of the end-of-update callbacks queued at 0x168ff0
void camera_callback_call(uint32_t fn) {
    // Aridia's and Eudora's own copies of the look-height callback (queued by their camera hooks 0x2fa710 / 0x2f0448): the
    // port of every level's copy (kerwan_camera.cpp)
    if (on_camera_level(ARIDIA) && fn == L2_ADDR(0x002fa770)) {
        cam0_level_callback(0);
        return;
    }
    if (on_camera_level(EUDORA) && fn == L4_ADDR(0x002f04a8)) {
        cam0_level_callback(0);
        return;
    }
    if (on_camera_level(ORXON) && orxon_camera_callback_call(fn)) return;  // (level 10's own: 0x2f4b78, queued by the camera hook 0x2f4b18)
    if (on_camera_level(POKITARU) && pokitaru_camera_callback_call(fn)) return;  // (level 11's own: 0x327af0, queued by the camera hook 0x327a90)
    if (on_camera_level(HOVEN) && hoven_camera_callback_call(fn)) return;  // (level 12's own: 0x314950, queued by the camera hook 0x3148f0)
    if (on_camera_level(GEMLIK) && gemlik_camera_callback_call(fn)) return;  // (level 13's own: 0x316ba8, queued by the camera hook 0x316b48)
    if (on_camera_level(OLTANIS) && oltanis_camera_callback_call(fn)) return;  // (level 14's own: 0x313240, queued by the camera hook 0x3131e0)
    if (on_camera_level(QUARTU) && quartu_camera_callback_call(fn)) return;  // (level 15's own: 0x2f69e0, queued by the camera hook 0x2f6980)
    if (on_camera_level(KALEBO) && kalebo_camera_callback_call(fn)) return;  // (level 16's own: 0x2f3ae8, queued by the camera hook 0x2f3a88)
    if (on_camera_level(VELDIN_ORBIT) && veldin_orbit_camera_callback_call(fn)) return;  // (level 17's own: 0x2fff98, queued by the camera hook 0x2fff38)
    if (on_camera_level(VELDIN_FINALE) && veldin_finale_camera_callback_call(fn)) return;  // (level 18's own: 0x305378, queued by the camera hook 0x305318)
    if (on_camera_level(GASPAR) && gaspar_camera_callback_call(fn)) return;  // (level 9's own: 0x314280, queued by the camera hook 0x314220)
    if (on_camera_level(UMBRIS) && umbris_camera_callback_call(fn)) return;  // (level 7's own: 0x3280f0, queued by the camera hook 0x328090)
    if (on_camera_level(BLARG) && blarg_camera_callback_call(fn)) return;  // (level 6's own: 0x314810, queued by the camera hook 0x3147b0)
    if (on_camera_level(RILGAR) && rilgar_camera_callback_call(fn)) return;  // (Rilgar's own: 0x328fe0, queued by the camera hook 0x328f80)
    if (on_camera_level(KERWAN) && fn == L3_ADDR(0x002eb468)) {  // (Kerwan's own, queued by the camera hook 0x2eb408: kerwan_camera.cpp)
        cam0_level_callback(0);
        return;
    }
    switch (ovl_code_to_veldin(fn)) {  // (the level's code address as Veldin's: the ports' cases)
    default:
        TODO_PORT(fn, "camera end-of-update callback")();
        return;
    }
}

// orig: 0x001eb240 — move `cur` toward `target` with the velocity *vel (spring k, damping d,
// |vel| clamped to max when max != 0, never past the target).
float camera_spring(float cur, float target, float k, float d, float max, uint32_t vel) {
    const float diff = target - cur;
    float v = lwc1(vel);
    v = v + (k * diff - d * v);
    swc1(vel, v);
    if (max != 0.0f) {
        if (max < v) swc1(vel, max);
        else if (v < -max) swc1(vel, -max);
    }
    if (abs_f(diff) < lwc1(vel)) {
        swc1(vel, abs_f(diff));
    } else {
        const float n = -abs_f(diff);
        if (lwc1(vel) < n) swc1(vel, -abs_f(diff));
    }
    return cur + lwc1(vel);
}

// orig: 0x001eb188 — run and clear the callbacks queued at 0x168ff0 (count 0x15ef4c).
void camera_callbacks_run() {
    for (int32_t i = 0; i < lw(OV(0x0015ef4c)); ++i) camera_callback_call(lwu(OV(0x00168ff0) + static_cast<uint32_t>(i) * 4));
    sw(OV(0x0015ef4c), 0);
}

// orig: 0x001eb468 — the camera marker moby (0x166ed4): spawned for the type-0 camera, retired otherwise.
void camera_marker_update(uint32_t cam) {
    if (lh(cam + 0x86) == 0) {
        if (lw(OV(0x00166ed4)) == 0) sw(OV(0x00166ed4), camera_marker_spawn(OV(0x00166dc0)));
    } else if (lw(OV(0x00166ed4)) != 0) {
        moby_retire(lwu(OV(0x00166ed4)));
        sw(OV(0x00166ed4), 0);
    }
}

// orig: 0x001eb4c8 — the type's init function.
void camera_type_init(uint32_t cam) {
    const uint32_t fn = type_fn(cam, 8);
    if (fn != 0) camera_type_call(fn, cam, 0);
}

// orig: 0x001eb9d8 — the type's exit / pre-update function.
void camera_type_exit(uint32_t cam) {
    const uint32_t fn = type_fn(cam, 0x10);
    if (fn != 0) camera_type_call(fn, cam, 0);
}

// orig: 0x001eb510 — make `cam` the active camera, with the transition the old one asks for.
void camera_switch(uint32_t cam) {
    constexpr OvConst<0x00166ef0u> T{};  // transition: mode (s16), +2 state, +3 kind
    int32_t kind = 0;
    const uint32_t old = lwu(ACTIVE);
    cam_trace("switch", old, cam);
    const uint32_t data = lwu(level_entry(cam) + 0x1c);
    const int32_t req = lh(old + 0x7e);
    if (data != 0) kind = lbu(data + 0x1d);
    auto speed = [&](bool pal_slow) {  // the blend speed of the new camera (+0x78, default 0.018)
        const float f = lwc1(cam + 0x78);
        sb(T + 3, 0);
        if (0.0f < f) {
            swc1(T + 0x18, f);
            swc1(T + 0x24, f);
        } else {
            swc1(T + 0x18, u2f(0x3c9374bcu));
            swc1(T + 0x24, u2f(0x3c9374bcu));
            if (pal_slow && lw(0x0015ed84) == 1) {
                swc1(T + 0x18, u2f(0x3c23d70au));
                swc1(T + 0x24, u2f(0x3c23d70au));
            }
        }
    };
    auto mode = [&] { sh(T, lh(T) == 0 ? 1u : 2u); };
    if (req == 4) {
        sh(cam + 0x8e, 1);
    } else if (req == 2 || kind == 1 || kind == 5) {
        if (kind == 1) {
            speed(false);
        } else if (kind == 5) {
            sb(T + 3, 2);
            const float f = lwc1(cam + 0x78);
            sw(T + 0x84, 0.0f < f ? static_cast<uint32_t>(float_to_int(f)) : 0x28u);
        }
        mode();
    } else if (req == 3 || req == 5 || kind == 3 || kind == 6) {
        lq_sq(cam + 0x30, old + 0x30);
        lq_sq(cam, old);
        lq_sq(cam + 0x10, old + 0x10);
        lq_sq(cam + 0x20, old + 0x20);
        sb(cam + 0x7d, 2);
        if (lh(old + 0x7e) == 5 || kind == 6) {
            speed(true);
            mode();
        }
    } else {
        sh(cam + 0x8e, 1);
    }
    sh(old + 0x7e, 0);
    sb(old + 0x7d, 0);
    sh(old + 0x8e, 0);
    sw(PREV, old);
    mem_copy128(OV(0x00169390), OV(0x00169110), 0x280);
    sw(lwu(PREV) + 0x70, OV(0x00169390));
    sw(ACTIVE, cam);
    sw(cam + 0x70, OV(0x00169110));
    sw(OV(0x00167018), 0);
    camera_type_init(cam);
    sub_001eb128();
    if (lw(OV(0x0016c06c)) == 0) lq_sq(OV(0x00166dc0), cam + 0x30);
    swc1(cam + 0x64, lwc1(cam + 0x30));
    swc1(cam + 0x68, lwc1(cam + 0x34));
    swc1(cam + 0x6c, lwc1(cam + 0x38));
}

// orig: 0x001eb810 — may camera `cam` take over from `cur`? (by its type's accept function,
// then by its trigger kind +0x74: 0 always, 1 / 2 when armed, 4 inside its box, 7 by the
// hero's state; a higher priority +0x7c than the current camera unless that one asks out)
int32_t camera_accept(uint32_t cam, uint32_t cur) {
    if (lbu(cam + 0x7c) == 0) return 0;
    const uint32_t fn = type_fn(cam, 4);
    if (fn != 0) {
        const int32_t r = camera_type_call(fn, cam, cur);
        if (r == -1) return 0;
        if (r == 1) return 1;
    }
    const uint32_t trig = lwu(cam + 0x74);
    auto higher = [&] { return lbu(cur + 0x7c) < lbu(cam + 0x7c); };
    switch (trig) {
    case 1:
    case 2:
        if (lbu(cam + 0x7d) == 0) return 0;
        [[fallthrough]];
    case 0:
        if (cur == 0) return 1;
        if (lh(cur + 0x7e) != 0) return 1;
        return higher() ? 1 : 0;
    case 4: {
        const uint32_t data = lwu(level_entry(cam) + 0x1c);
        if (cur != 0 && lh(cur + 0x7e) == 0 && !higher()) return 0;
        return point_in_box(0x0013f3d0, lw(data + 0xc)) != 0 ? 1 : 0;
    }
    case 7: {
        const int32_t id = lh(cam + 0x86);
        if (id != lw(0x001415d4)) return 0;
        if (lh(cur + 0x7e) == 0 && !higher()) return 0;
        if (id != 3) return 1;
        const int32_t k = lw(lwu(level_entry(cam) + 0x1c) + 0x24);
        if (k < 0) return 1;
        if (lw(0x0013f8b0) != lw(lwu(OV(0x0015f70c)) + static_cast<uint32_t>(k) * 0x20 + 0x10)) return 0;
        return lw(0x0013f8c0) == 0 ? 1 : 0;
    }
    default:
        return 0;
    }
}

// orig: 0x001eba20 — pick the camera for this frame (switching when a live one accepts) and
// run its type update; the drawn position +0x64 follows +0x30.
int32_t camera_select() {
    uint32_t cur = lwu(ACTIVE);
    bool switched = false;
    cam_trace_frame(cur);
    camera_type_exit(cur);
    uint32_t c = CAMS;
    for (int32_t i = 0; i < 48; ++i, c += 0xa0) {
        if (lw(LIVE + static_cast<uint32_t>(i) * 4) == 0 || c == cur) continue;
        if (camera_accept(c, cur) != 0) {
            cur = c;
            switched = true;
        }
    }
    if (switched) camera_switch(cur);
    const uint32_t fn = type_fn(cur, 0xc);
    camera_marker_update(cur);
    if (fn != 0) camera_type_call(fn, cur, 0);
    swc1(cur + 0x64, lwc1(cur + 0x30));
    swc1(cur + 0x68, lwc1(cur + 0x34));
    swc1(cur + 0x6c, lwc1(cur + 0x38));
    camera_callbacks_run();
    return -1;
}

// orig: 0x001ebb30 — the two angles and distance of a - b in the frame (x, y, z): out = {yaw
// about z from x (signed by y), pitch from the z axis plane, distance}.
void camera_angles_v(float* out, const Vec4& a, const Vec4& b, const Vec4& x, const Vec4& y, const Vec4& z) {
    Vec4 d, pz, flat, unit, rot;
    vec3_sub(d, a, b);
    vec3_set_length(pz, z, vec3_dot(d, z));
    vec3_sub(flat, d, pz);
    float dot = vec3_dot(x, flat);
    float len = vec3_length(flat);
    if (len == 0.0f) len = u2f(0x38d1b717u);
    float yaw = u2f(0x3fc90fdbu) - asin_f(fpu_div(dot, len));
    vec3_set_length(unit, flat, 1.0f);
    if (vec3_dot(y, unit) < 0.0f) yaw = -yaw;
    out[0] = yaw;
    vec_rotate_axis(rot, x, z, yaw);
    dot = vec3_dot(rot, d);
    len = vec3_length(d);
    if (len == 0.0f) len = u2f(0x38d1b717u);
    const float p = u2f(0x3fc90fdbu) - asin_f(fpu_div(dot, len));
    vec3_set_length(unit, d, 1.0f);
    out[1] = vec3_dot(z, unit) < 0.0f ? p : -p;
    out[2] = vec3_length(d);
}
// orig: 0x001ebb30 (EE-address form)
void camera_angles(uint32_t out, const Vec4& a, const Vec4& b, const Vec4& x, const Vec4& y, const Vec4& z) {
    camera_angles_v(static_cast<float*>(ee_ram_ptr(out)), a, b, x, y, z);
}

// orig: 0x001ebd10 — the hero-relative angles of the transition end (0x166fb0) into 0x166f60.
void camera_trans_hero_angles() {
    constexpr OvConst<0x00166ef0u> T{};
    Vec4 r0, r1, r2;
    hero_rows(r0, r1, r2);
    V(T + 0x90) = r0;
    V(T + 0xa0) = r2;
    camera_angles(T + 0x70, V(T + 0xc0), V(0x0013f3d0), r0, r1, r2);
    lq_sq(T + 0xb0, T + 0xd0);
}

// orig: 0x001ebdf0 — the transition end from the blend start (moved with the hero for kind 2).
void camera_trans_end_from_start() {
    constexpr OvConst<0x00166ef0u> T{};
    if (lbu(T + 2) != 0) return;
    lq_sq(T + 0xc0, T + 0x50);
    if (lbu(T + 3) == 2) vec3_add(V(T + 0xc0), V(0x0013f490), V(T + 0xc0));
    lq_sq(T + 0xd0, T + 0x60);
}

// orig: 0x001ebe68 — the blend start from the transition end (state 1 / 2 restarts).
void camera_trans_start_from_end() {
    constexpr OvConst<0x00166ef0u> T{};
    if (lbu(T + 2) == 0) return;
    lq_sq(T + 0x50, T + 0xc0);
    lq_sq(T + 0x60, T + 0xd0);
}

// orig: 0x001ebea0 — start a transition from camera `old` (mode 1: fresh, 2: interrupted).
void camera_trans_begin(uint32_t old) {
    constexpr OvConst<0x00166ef0u> T{};
    if (lh(T) == 1) {
        const int32_t kind = lbu(T + 3);
        if (kind == 0) {
            lq_sq(T + 0x50, old + 0x30);
            mat_to_quat_at(T + 0x60, old);
        } else if (kind == 2) {
            lq_sq(T + 0xc0, old + 0x30);
            mat_to_quat_at(T + 0xd0, old);
            camera_trans_hero_angles();
        } else {
            Vec4 r0, r1, r2;
            hero_rows(r0, r1, r2);
            camera_angles(T + 0x70, V(old + 0x30), V(lwu(ACTIVE) + 0x30), r0, r1, r2);
            mat_to_quat_at(T + 0xb0, old);
            lq_sq(T + 0xd0, T + 0xb0);
        }
    } else {
        const int32_t kind = lbu(T + 3);
        if (kind == 2) {
            camera_trans_end_from_start();
            camera_trans_hero_angles();
        } else if (kind == 1) {
            camera_trans_end_from_start();
            lq_sq(T + 0xb0, T + 0xd0);
        } else if (kind == 0) {
            camera_trans_start_from_end();
        }
    }
    const uint32_t kind = lbu(T + 3);
    sh(T, 3);
    sb(T + 2, kind);
    if (kind == 0) {
        sw(T + 0x1c, 0);
        swc1(T + 0x20, lwc1(T + 0x24));
        lq_sq(T + 0x40, T + 0x50);
        sw(T + 0x10, 0);
        swc1(T + 0x14, lwc1(T + 0x18));
        lq_sq(T + 0x30, T + 0x60);
        return;
    }
    const int32_t n = lw(T + 0x84) + 1;
    sw(T + 0x84, static_cast<uint32_t>(n));
    const int32_t frames = frame_scale_i(n);
    sw(T + 0x7c, static_cast<uint32_t>(frames));
    swc1(T + 0x80, fpu_div(1.0f, int_to_float(frames)));
}

// orig: 0x001ec0f8 — one frame of the position / rotation blend (kind 0): returns 1 once both
// parameters reached 1.
int32_t camera_blend(uint32_t cam, uint32_t t) {
    if (lwc1(t + 0xc) == 1.0f && lwc1(t) == 1.0f) return 1;
    const float k = cos_ease(0.0f, 1.0f, lwc1(t + 0xc));
    vec3_add(V(t + 0x30), V(0x0013f490), V(t + 0x30));
    for (uint32_t o = 0; o < 0xc; o += 4) {
        const float a = lwc1(t + 0x30 + o);
        swc1(t + 0x40 + o, a + (lwc1(cam + 0x30 + o) - a) * k);
    }
    if (lw(OV(0x0016c06c)) == 0) lq_sq(OV(0x00166dc0), t + 0x40);
    Vec4 q;
    Mat4 m;
    {
        Mat4 c;
        mat3_copy_affine(c, M(cam));
        mat_to_quat(q, c);
        mat3_copy(M(cam), c);
    }
    quat_nlerp(V(t + 0x50), V(t + 0x20), q, cos_ease(0.0f, 1.0f, lwc1(t)));
    quat_to_mat4(V(t + 0x50), m);
    if (lw(OV(0x0016c06c)) == 0) mat3_copy(M(OV(0x00166fd0)), m);
    float c = lwc1(t + 0xc) + lwc1(t + 0x10) * lwc1(0x0015ed60);
    swc1(t + 0xc, c);
    if (1.0f < c) swc1(t + 0xc, 1.0f);
    c = lwc1(t) + lwc1(t + 4) * lwc1(0x0015ed60);
    swc1(t, c);
    if (1.0f < c) swc1(t, 1.0f);
    return 0;
}

// orig: 0x001ec2d8 — one frame of the orbiting transition (kinds 1 / 2): yaw, pitch and
// distance around the target ease toward the new camera's, the view rows turn with them.
// Returns 1 when the frame count +0xc ran out.
int32_t camera_orbit(uint32_t cam, uint32_t t) {
    const int32_t n = lw(t + 0xc);
    if (n <= 0) return 1;
    const float e = cos_ease(1.0f, static_cast<float>(lw(t + 0x14)), static_cast<float>(n) * lwc1(t + 0x10));
    const float k = fpu_div(1.0f, e);
    alignas(16) float ang[4];
    Vec4 centre, r0, r2, r1;
    if (lbu(OV(0x00166ef2)) == 2) {
        centre = V(0x0013f3d0);
        r0 = V(t + 0x20);
        r2 = V(t + 0x30);
        vec3_cross_ba(r1, r0, r2);
        camera_angles_v(ang, V(cam + 0x30), centre, r0, r1, r2);
    } else {
        centre = V(cam + 0x30);
        vec3_set_length(r0, V(lwu(0x001413d0) + 0xc0), 1.0f);
        vec3_set_length(r2, V(lwu(0x001413d0) + 0xe0), 1.0f);
        ang[0] = u2f(0x40490fdbu);
        ang[1] = 0.0f;
        ang[2] = 0.0f;
    }
    const float dyaw = angle_sub(ang[0], lwc1(t));
    swc1(t, angle_add(lwc1(t), dyaw * k));
    swc1(t + 4, angle_add(lwc1(t + 4), angle_sub(ang[1], lwc1(t + 4)) * k));
    const float d = lwc1(t + 8);
    swc1(t + 8, d + (ang[2] - d) * k);
    Vec4 off, side;
    vec3_set_length(off, r0, lwc1(t + 8));
    vec_rotate_axis(off, off, r2, lwc1(t));
    vec3_cross_ba(side, off, r2);
    vec3_set_length(side, side, 1.0f);
    vec_rotate_axis(off, off, side, lwc1(t + 4));
    vec3_add(V(t + 0x50), centre, off);
    if (lw(OV(0x0016c06c)) == 0) lq_sq(OV(0x00166dc0), t + 0x50);
    // the current rotation's rows; how far the new camera's x row is turned about row 2
    Mat4 m;
    quat_to_mat3(V(t + 0x40), m);
    Vec4 proj, rest;
    vec3_scale(proj, m.r[2], vec3_dot(m.r[2], V(cam)));
    vec3_sub(rest, V(cam), proj);
    const float dt = vec3_dot(m.r[0], rest);
    float yaw = u2f(0x3fc90fdbu) - asin_f(fpu_div(dt, vec3_length(rest)));
    const float sgn = (0.0f <= vec3_dot(rest, m.r[1])) ? 1.0f : -1.0f;
    yaw = yaw * sgn;
    float step;
    const bool far = u2f(0x3fc90fdbu) < abs_f(dyaw);
    if (far && ((0.0f <= dyaw && sgn < 0.0f) || (dyaw < 0.0f && 0.0f <= sgn))) {
        if (yaw < 0.0f) yaw = yaw + u2f(0x40c90fdbu);
        else yaw = yaw - u2f(0x40c90fdbu);
    }
    step = yaw * k;
    Vec4 x, y, q, xr;
    if (abs_f(step) < u2f(0x3727c5acu)) {
        x = m.r[0];
        y = m.r[1];
    } else {
        quat_axis_angle(q, m.r[2], step);
        quat_rotate(x, m.r[0], q);
        quat_rotate(y, m.r[1], q);
    }
    if (abs_f(yaw) < u2f(0x3727c5acu)) {
        xr = m.r[0];
    } else {
        Vec4 q2;
        quat_axis_angle(q2, m.r[2], yaw);
        quat_rotate(xr, m.r[0], q2);
    }
    float tilt = u2f(0x3fc90fdbu) - asin_f(vec3_dot(xr, V(cam)));
    tilt = (0.0f <= vec3_dot(xr, V(cam + 0x20))) ? tilt * 1.0f : tilt * -1.0f;
    vec_rotate_axis(x, x, y, tilt * k);
    vec3_set_length(V(OV(0x00166fd0)), x, 1.0f);
    vec3_cross_ba(V(OV(0x00166fe0)), V(OV(0x00166fd0)), V(0x0013f5e0));
    vec3_set_length(V(OV(0x00166fe0)), V(OV(0x00166fe0)), -1.0f);
    vec3_cross_ba(V(OV(0x00166ff0)), V(OV(0x00166fe0)), V(OV(0x00166fd0)));
    mat_to_quat_at(t + 0x60, OV(0x00166fd0));
    mat_to_quat_at(t + 0x40, OV(0x00166fd0));
    timer_tick_i32(ref32(t + 0xc));
    return 0;
}

// orig: 0x001ec8b0 — run the transition (blend or orbit); when it ends the view takes the
// camera's rows and position and the transition mode returns to 0.
void camera_trans_step(uint32_t cam) {
    constexpr OvConst<0x00166ef0u> T{};
    const int32_t done = lbu(T + 2) == 0 ? camera_blend(cam, T + 0x10) : camera_orbit(cam, T + 0x70);
    if (done == 0) return;
    if (lw(OV(0x0016c06c)) == 0) {
        lq_sq(OV(0x00166fd0), cam);
        lq_sq(OV(0x00166fe0), cam + 0x10);
        lq_sq(OV(0x00166ff0), cam + 0x20);
        lq_sq(OV(0x00166dc0), cam + 0x30);
    }
    sb(T + 2, 0);
    sh(T, 0);
}

// orig: 0x001ec960 — one camera shake (amplitude +0, offset +4, frames left +8, of +0xc):
// a decaying cosine along the view's z row (axis 0) or x row (axis 1) added to 0x166dc0.
void camera_shake(uint32_t s, int32_t axis) {
    const uint32_t cam = lwu(ACTIVE);
    if (cam != 0 && lh(cam + 0x86) == 6) {
        sw(s + 0xc, 0);
        sw(s + 8, 0);
        return;
    }
    const int32_t left = lw(s + 8);
    if (left == 0) {
        sw(s + 0xc, 0);
        return;
    }
    if (lw(s + 0xc) < left) sw(s + 0xc, static_cast<uint32_t>(left));
    timer_tick_i32(ref32(s + 8));
    float f = int_to_float(lw(s + 8));
    f = fpu_div(f, int_to_float(lw(s + 0xc)));
    const float a = int_to_float(lw(s + 8));
    const float c = cos_vu(angle_wrap(a + a));
    const float off = ((lwc1(s) * c) * f) * f;
    swc1(s + 4, off);
    Vec4 v;
    vec3_set_length(v, V(axis == 0 ? OV(0x00166ff0u) : OV(0x00166fd0u)), off);
    vec3_add(V(OV(0x00166dc0)), V(OV(0x00166dc0)), v);
}

// orig: 0x001eca70 — the targets the camera types follow: the smoothed hero facing 0x166e30,
// the hero offset 0x166e80 (length 0x166eb0, along the facing 0x166eb8 / 0x166ea0, across
// 0x166e90 / 0x166eb4), the smoothed hero position 0x166e10, the last five ground heights
// 0x166ebc.., and the platform the hero stands on 0x166ee4 (its height change 0x166eec).
void camera_track() {
    constexpr OvConst<0x00166e10u> S{};
    alignas(16) Vec4 dir;
    vec3_set_length(dir, V(0x0013f5e0), -1.0f);
    lq_sq(S + 0x40, S + 0x30);
    V(S + 0x30) = dir;
    if (vec3_dot(V(S + 0x20), dir) < u2f(0xbf7ae148u)) {
        dir.x = dir.x + u2f(0x3e4ccccdu);
        dir.y = dir.y + u2f(0x3e4ccccdu);
        dir.z = dir.z + u2f(0x3e4ccccdu);
    }
    const float k = u2f(0x3c75c28fu), d = u2f(0x3e4ccccdu);
    swc1(S + 0x20, camera_spring(lwc1(S + 0x20), dir.x, k, d, 0.0f, S + 0x50));
    swc1(S + 0x24, camera_spring(lwc1(S + 0x24), dir.y, k, d, 0.0f, S + 0x54));
    swc1(S + 0x28, camera_spring(lwc1(S + 0x28), dir.z, k, d, 0.0f, S + 0x58));
    vec3_set_length(V(S + 0x20), V(S + 0x20), 1.0f);
    vec3_sub(V(S + 0x70), V(0x0013f3d0), V(S + 0x60));
    swc1(S + 0xa0, vec3_length(V(S + 0x70)));
    const float along = vec3_dot(V(S + 0x70), dir);
    swc1(S + 0xa8, along);
    Vec4 a;
    vec3_set_length(a, dir, along);
    V(S + 0x90) = a;
    vec3_sub(V(S + 0x80), V(S + 0x70), a);
    const float across = vec3_length(V(S + 0x80));
    swc1(S + 0xa4, across);
    vec3_scale(V(S + 0x80), V(S + 0x80), fpu_div(1.0f, across));
    lq_sq(S + 0x60, 0x0013f3d0);
    if (lw(0x001415d4) == 0x50 && lw(0x001413d4) != 0x11) {
        swc1(S, lwc1(0x0013f3d0));
        swc1(S + 4, lwc1(0x0013f3d4));
    } else {
        const float x = lwc1(0x0013f3d0), y = lwc1(0x0013f3d4);
        swc1(S, x);
        swc1(S + 4, y);
        swc1(S + 8, camera_spring(lwc1(S + 8), lwc1(0x0013f3d8), u2f(0x3bf5c28fu), u2f(0x3e333333u), 0.0f, S + 0x10));
        swc1(S + 0xc, lwc1(0x0013f3d8));
    }
    for (uint32_t i = 0; i < 4; ++i) sw(S + 0xac + i * 4, lwu(S + 0xb0 + i * 4));
    swc1(S + 0xbc, lwc1(0x0013f3e8));
    const uint32_t plat = lwu(0x0013f64c);
    if (plat == 0 || lh(plat + 0xa6) == 0x4ba || lh(plat + 0xa6) == 0x336) {
        sw(S + 0xdc, 0);
        sw(S + 0xd4, 0);
        swc1(S + 0xd8, lwc1(0x0013f3d8));
    } else if (plat != lwu(S + 0xd4)) {
        sw(S + 0xd4, plat);
        sw(S + 0xdc, 0);
        swc1(S + 0xd8, lwc1(plat + 0x18));
    } else {
        const float dz = lwc1(plat + 0x18) - lwc1(S + 0xd8);
        swc1(S + 0xdc, dz);
        if (abs_f(dz) < u2f(0x3a83126fu)) sw(S + 0xdc, 0);
        swc1(S + 0xd8, lwc1(lwu(S + 0xd4) + 0x18));
    }
}

// orig: 0x001ecdf0 — is the camera close above the ground under it? Up to six segment tests
// from 0.75 above to 0.75 below the view position; 0x167014 = 1 when the hit ground is
// within 0.04 under the camera.
void camera_ground_check() {
    const uint32_t cam = lwu(ACTIVE);
    if (lh(cam + 0x86) == 6 || lw(OV(0x0015f5c4)) != 0) {
        sw(OV(0x00167014), 0);
        return;
    }
    alignas(16) Vec4 a = V(OV(0x00166dc0)), b = V(OV(0x00166dc0));
    a.z = a.z + 0.75f;
    b.z = b.z - 0.75f;
    for (int32_t i = 0; i < 6; ++i) {
        if (segment_collide_v(a, b, 0x12, 0, nullptr) == 0) return;
        if (sub_001f35d8() == 0) {
            const float h = surface_height(OV(0x00173e60), 0);
            sw(OV(0x00167014), lwc1(OV(0x00166dc8)) < h + u2f(0x3d23d70au) ? 1u : 0u);
            return;
        }
        a = V(OV(0x00173e60));
        a.z = a.z - u2f(0x3c23d70au);
    }
}

// orig: 0x001ecf40 — the collision masks the camera uses this frame (0x15ef5c / 0x15ef60,
// 0x15ef58) by the hero's state and what the hero carries.
void camera_masks() {
    sw(OV(0x0015ef5c), 0x14);
    if (static_cast<uint32_t>(lw(0x001413dc) - 0x11) < 2u || lw(0x001413d4) == 0x73) sw(OV(0x0015ef5c), 0x34);
    if (lw(0x001413dc) != 0x11 && lwc1(0x0013f640) < lwc1(OV(0x00166dc8))) sw(OV(0x0015ef5c), 0x14);
    const uint32_t m = lwu(OV(0x0015ef5c));
    sw(OV(0x0015ef60), m);
    sw(OV(0x0015ef5c), m | 0x80);
    sw(OV(0x0015ef58), 0xb4);
    uint32_t v, w;
    if (lbu(0x00140635) != 0) { v = 0x100; w = 0x1b4; }
    else if (lbu(0x0014063b) != 0) { v = 0xb00; w = 0xbb4; }
    else if (lbu(0x00140636) != 0) { v = 0x300; w = 0x3b4; }
    else if (lbu(0x0014063c) != 0) { v = 0xd00; w = 0xdb4; }
    else {
        if (lbu(0x00140634) != 0) {
            sw(OV(0x00166ed0), 0);
            sw(OV(0x0015ef58), 0xb4);
        } else {
            sw(OV(0x0015ef58), lwu(OV(0x00166ed0)) | 0xb4);
        }
        return;
    }
    sw(OV(0x00166ed0), v);
    sw(OV(0x0015ef58), w);
}

// orig: 0x001ed060 — count down the timer 0x15f3fc by the step 0x166ed8 (both cleared at 0).
void camera_timer_tick() {
    const float step = lwc1(OV(0x00166ed8));
    if (step == 0.0f) return;
    const float t = lwc1(OV(0x0015f3fc)) - step;
    swc1(OV(0x0015f3fc), t);
    if (t <= 0.0f) {
        swc1(OV(0x00166ed8), 0.0f);
        swc1(OV(0x0015f3fc), 0.0f);
    }
}

// orig: 0x001ed0a8 — the camera update of a gameplay frame: targets, the camera choice and
// its type update, the transition, the view rows and angles, the shakes, the ground check
// and the fog region.
void camera_update() {
    constexpr OvConst<0x00166ef0u> T{};
    if (lw(OV(0x0015f5c4)) == 5) {
        if (lw(OV(0x001ca500)) != 0) return;
        sh(T, 0);
        sb(T + 2, 0);
    }
    sw(OV(0x00167018), lwu(OV(0x00167018)) + 1);
    camera_timer_tick();
    camera_masks();
    camera_track();
    camera_select();
    const uint32_t cam = lwu(ACTIVE);
    if (static_cast<uint32_t>(lhu(T) - 1) < 2u) camera_trans_begin(lwu(PREV));
    bool view = true;
    if (lh(T) == 3) {
        camera_trans_step(cam);
    } else if (lw(OV(0x0016c06c)) != 0) {
        view = false;
    } else {
        lq_sq(OV(0x00166dc0), cam + 0x30);
        lq_sq(OV(0x00166fd0), cam);
        lq_sq(OV(0x00166fe0), cam + 0x10);
        lq_sq(OV(0x00166ff0), cam + 0x20);
    }
    if (view && lw(OV(0x0016c06c)) == 0) {
        Mat4 m;
        mat3_copy_affine(m, M(OV(0x00166fd0)));
        mat_to_euler_r(m, V(OV(0x00166dd0)));
    }
    camera_shake(OV(0x00166de0), 0);
    camera_shake(OV(0x00166df0), 1);
    camera_ground_check();
    camera_fog(OV(0x00166dc0));
    if (lbu(0x0015edb4) != 0) vec3_cross_ba(V(OV(0x00166fe0)), V(OV(0x00166ff0)), V(OV(0x00166fd0)));
}

// orig: 0x001ed230 — the row of the type table 0x1ea880 with id `type` (the end row if none).
int32_t camera_type_row(int32_t type) {
    int32_t i = 0;
    int32_t v = lw(types_base());
    if (v == -1 || v == type) return i;
    uint32_t p = types_base() + 0x14;
    do {
        v = lw(p);
        ++i;
        if (v == -1) return i;
        p += 0x14;
    } while (v != type);
    return i;
}

// orig: 0x001ed280 — make the camera of type 0 the active one, with the view on it.
void camera_select_first() {
    const uint32_t old = lwu(ACTIVE);
    if (old != 0) {
        sh(old + 0x8e, 0);
        sb(lwu(ACTIVE) + 0x7d, 0);
        sh(lwu(ACTIVE) + 0x7e, 0);
    }
    const uint32_t cam = moby_table_find_by_uid(0);
    sw(ACTIVE, cam);
    sh(cam + 0x8e, 1);
    sw(lwu(ACTIVE) + 0x70, OV(0x00169110));
    sh(OV(0x00166ef0), 0);
    camera_track();
    camera_type_init(lwu(ACTIVE));
    sub_001eb128();
    const uint32_t c = lwu(ACTIVE);
    lq_sq(OV(0x00166dc0), c + 0x30);
    swc1(c + 0x64, lwc1(OV(0x00166dc0)));
    swc1(c + 0x68, lwc1(OV(0x00166dc4)));
    swc1(c + 0x6c, lwc1(OV(0x00166dc8)));
    swc1(OV(0x00166e10), lwc1(0x0013f3d0));
    swc1(OV(0x00166e18), lwc1(0x0013f3d8));
    swc1(OV(0x00166e14), lwc1(0x0013f3d4));
}

// orig: 0x001ed358 — the camera system at a level start: the targets on the hero, one camera
// per level camera entry (0x15ef50, count 0x15ef54), then the type-0 camera.
void camera_init() {
    sw(OV(0x00167018), 0);
    swc1(OV(0x0015ef40), 0.75f);
    sw(OV(0x00167014), 0);
    sw(OV(0x0015ef4c), 0);
    for (uint32_t i = 0; i < 48; ++i) {
        sh(CAMS + i * 0xa0 + 0x8e, 0);
        sw(LIVE + i * 4, 0);
    }
    sw(PREV, 0);
    sh(OV(0x00166ef0), 0);
    vec3_set_length(V(OV(0x00166e30)), V(lwu(0x001413d0) + 0xe0), 1.0f);
    lq_sq(OV(0x00166e40), OV(0x00166e30));
    lq_sq(OV(0x00166e50), OV(0x00166e30));
    vec_clear(V(OV(0x00166e60)));
    lq_sq(OV(0x00166e70), 0x0013f3d0);
    constexpr OvConst<0x00166e10u> S{};
    const float z = lwc1(0x0013f3d8), g = lwc1(0x0013f3e8);
    swc1(S + 0xd8, z);
    swc1(S + 0xac, g);
    sw(S + 0xc4, 0);
    sw(S + 0xcc, 0);
    sw(S + 0xc0, 0);
    sw(S + 0xc8, 0);
    sw(S + 0xd0, 0);
    swc1(S + 0xbc, g);
    swc1(S + 0xb8, g);
    swc1(S + 0xb4, g);
    swc1(S + 0xb0, g);
    sw(S + 0xd4, 0);
    sw(S + 0xdc, 0);
    for (int32_t i = 0; i < lw(OV(0x0015ef54)); ++i) {
        const uint32_t cam = CAMS + static_cast<uint32_t>(i) * 0xa0;
        const uint32_t e = lwu(OV(0x0015ef50)) + static_cast<uint32_t>(i) * 0x20;
        const uint32_t data = lwu(e + 0x1c);
        sh(cam + 0x84, static_cast<uint32_t>(i));
        sw(data + 4, cam);
        sh(cam + 0x8e, 0);
        sb(cam + 0x89, 0);
        sh(cam + 0x8a, 0);
        sh(cam + 0x82, 0xffff);
        sh(cam + 0x80, 0xffff);
        sb(cam + 0x7d, 0);
        sh(cam + 0x7e, 0);
        swc1(cam + 0x78, -1.0f);
        sb(cam + 0x7c, lbu(data + 0x1c));
        sw(cam + 0x74, lbu(data + 0x1f));
        const uint16_t type = lhu(e + 0xc);
        sw(LIVE + static_cast<uint32_t>(i) * 4, 1);
        sh(cam + 0x86, type);
        sh(cam + 0x8c, static_cast<uint32_t>(camera_type_row(static_cast<int16_t>(type))));
    }
    camera_select_first();
    cam_trace_list();
    sh(OV(0x00166ef0), 0);
    sw(OV(0x00167000), 0);
    sw(OV(0x00167010), 0);
    sw(OV(0x0016700c), 0);
    sw(OV(0x00167004), 0);
}

// orig: 0x001ee6b8 — the fog of the fog region around pos (0x17eec0, 0x80 each; flag 2): the
// colour 0x15f444.. and the near / far values 0x15f448.. blended by the region's weight.
void camera_fog(uint32_t pos) {
    float t;
    int32_t idx;
    if (sub_00256de0(pos, &t, &idx) == 0) return;
    const uint32_t r = OV(0x0017eec0) + static_cast<uint32_t>(idx) * 0x80;
    if ((lw(r + 0x50) & 2) == 0) return;
    const int32_t a = float_to_int(t * 255.0f);
    const int32_t b = 0xff - a;
    const float inv = 1.0f - t;
    const int32_t c1 = lw(r + 0x58), c0 = lw(r + 0x54);
    const float f2 = (lwc1(r + 0x78) * t + lwc1(r + 0x68) * inv) * 255.0f;
    const float f5 = (lwc1(r + 0x70) * t + lwc1(r + 0x60) * inv) * 255.0f;
    const float f6 = (lwc1(r + 0x6c) * t + lwc1(r + 0x5c) * inv) * 1024.0f;
    const float f3 = (lwc1(r + 0x74) * t + lwc1(r + 0x64) * inv) * 1024.0f;
    sb(OV(0x0015f446), static_cast<uint32_t>((((c1 >> 16) & 0xff) * a + ((c0 >> 16) & 0xff) * b) >> 8));
    sb(OV(0x0015f444), static_cast<uint32_t>(((c1 & 0xff) * a + (c0 & 0xff) * b) >> 8));
    sb(OV(0x0015f445), static_cast<uint32_t>((((c1 >> 8) & 0xff) * a + ((c0 >> 8) & 0xff) * b) >> 8));
    swc1(OV(0x0015f448), f6);
    swc1(OV(0x0015f44c), f3);
    swc1(OV(0x0015f450), 255.0f - f5);
    swc1(OV(0x0015f454), 255.0f - f2);
}

}  // namespace rac1
