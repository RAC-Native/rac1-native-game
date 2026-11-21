// The panels' rows of lights: one port of the copies Aridia, Blarg, Quartu and Drek's fleet carry, and of a second build
// Orxon and Quartu carry (the same code relinked, compared with the relocated fields masked and normalised; the second
// build's registers differ and its fields sit 0x10 further in the pvars: the parameter). They name the engine's camera
// (OV: 0x166dc0). Header shared_panel_lights.h.
//
// The moby's pvars (from `base`): four lights' spins +0..+0xc and their rates +0x10..+0x1c, their pulses' timers
// +0x20..+0x2c, their sizes +0x30..+0x3c.
#include <game/ovl/shared_panel_lights.h>

#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>

namespace rac1 {

// From 0.3 behind the moby's point (half a unit up, seen from the camera), a light every 0.1 away from the camera: each
// one's spin turned by its rate (kept within 0..255), its pulse's timer restarted (255 frames) when out, its colour
// between 0x4040ffff and 0x1040ffff by how far the pulse is from its middle.
// orig: L2:0x002ea8b0 L6:0x002f4cd8 L15:0x002ebb50 L17:0x002f1568 L10:0x00298940 L15:0x002ea9b0
void panel_lights(uint32_t m, uint32_t base) {
    const uint32_t pv = lwu(m + 0x78) + base;
    const uint32_t sp = ee_stack_alloc(0x80);
    const uint32_t at = sp, step = sp + 0x10;
    const float half = 0.5f, full = 255.0f;
    swc1(m + 0x18, fpu_add(lwc1(m + 0x18), half));
    vec3_sub(V(at), V(OV(0x00166dc0)), V(m + 0x10));
    vec3_set_length(V(at), V(at), u2f(0xbe99999au));
    vec3_set_length(V(step), V(at), u2f(0x3dcccccdu));
    vec3_add(V(at), V(at), V(m + 0x10));
    swc1(m + 0x18, fpu_sub(lwc1(m + 0x18), half));
    for (uint32_t k = 0; k < 0x10; k += 4) {
        const float spin = fpu_add(lwc1(pv + k), lwc1(pv + 0x10 + k));
        if (full <= spin) swc1(pv + k, fpu_sub(spin, full));
        else if (spin <= 0.0f) swc1(pv + k, fpu_add(spin, full));
        else swc1(pv + k, spin);
        const uint32_t timer = pv + 0x20 + k;
        if (timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(timer))) != 0) sw(timer, static_cast<uint32_t>(frame_scale_i(0xff)));
        const float gone = int_to_float(frame_scale_i(0xff) - lw(timer));
        const float pulse = fpu_div(gone, static_cast<float>(frame_scale_i(0xff)));
        const uint32_t colour = rgba_lerp(0x4040ffffu, 0x1040ffffu, abs_f(fpu_sub(half, pulse)));
        const uint32_t frame = static_cast<uint32_t>(float_to_int(lwc1(pv + k))) & 0xffu;
        spawn_particle_3b(at, colour, frame, 0x35, 1, 2, 0, lwc1(pv + 0x30 + k));
        vec3_add(V(at), V(at), V(step));
    }
    ee_stack_free(0x80);
}

}  // namespace rac1
