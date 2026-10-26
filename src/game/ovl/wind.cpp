// Ported overlay code: the level's wind (0x1cb7c0: +0 its base direction, +4 the current one and
// +8 its step, +0xc the mode (2 gusting, 3 storms), +0xe the phase, +0x10 the base speed, +0x14
// the current one and +0x18 its step, +0x1c the period, +0x1e the phase timer).
#include <game/math/vecmath.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x001cb7c0u> W{};
int16_t& h16(uint32_t a) { return ee_ref<int16_t>(a); }

}  // namespace

// The wind's frame: in its phases it picks a new direction and speed around the base ones and
// moves toward them (mode 2: gusts over half to one and a half periods with rests of two to
// three; mode 3: a gust of 45 to 60 frames, a hold, a calmer turn, a rest of three to five
// periods); the speed stays within 9.6 a frame.
// orig: 0x002a4890
void wind_update() {
    const int32_t mode = lh(W + 0xc);
    const int32_t ph = lh(W + 0xe);
    auto step = [] {
        swc1(W + 4, angle_add(lwc1(W + 4), lwc1(W + 8)));
        const float s = lwc1(W + 0x14) + lwc1(W + 0x18);
        swc1(W + 0x14, s);
        float lim = lwc1(0x0015ed6c) * u2f(0x4119999au);
        if (lim < abs_f(s)) {
            if (lwc1(W + 0x14) < 0.0f) lim = -lim;
            swc1(W + 0x14, lim);
        }
    };
    auto rest = [](float lo, float hi, uint32_t next) {
        const float p = int_to_float(lh(W + 0x1c));
        const float r = rand_range_f(lo, hi);
        sh(W + 0x1e, static_cast<uint32_t>(float_to_int(p * r)));
        sh(W + 0xe, next);
    };
    if (mode == 2) {
        if (ph == 0) {
            if (timer_tick_i16(h16(W + 0x1e)) == 0) return;
            float frames = int_to_float(lh(W + 0x1c));
            frames = frames * rand_range_f(0.5f, 1.5f);
            const float turn = rand_range_f(u2f(0xbf860a92u), u2f(0x3f860a92u));
            const float base = lwc1(W + 0x10);
            const float dir = angle_add(lwc1(W), turn);
            const float speed = rand_range_f(base * 0.5f, base * 1.5f);
            swc1(W + 8, fpu_div(angle_sub(dir, lwc1(W + 4)), frames));
            swc1(W + 0x18, fpu_div(speed - lwc1(W + 0x14), frames));
            sh(W + 0x1e, static_cast<uint32_t>(float_to_int(frames)));
            sh(W + 0xe, 1);
        } else if (ph == 1) {
            if (timer_tick_i16(h16(W + 0x1e)) == 0) {
                step();
                return;
            }
            rest(2.0f, 3.0f, 0);
        }
        return;
    }
    if (mode != 3) return;
    if (ph == 0 || ph == 2) {
        if (timer_tick_i16(h16(W + 0x1e)) == 0) return;
        const float frames = frame_scale_f(rand_range_f(45.0f, 60.0f));
        const float base = lwc1(W + 0x10);
        if (ph == 0) {
            const float turn = rand_range_f(u2f(0xbfc90fdbu), u2f(0x3fc90fdbu));
            const float dir = angle_add(lwc1(W), turn);
            const float speed = rand_range_f(base * 1.5f, base + base);
            swc1(W + 8, fpu_div(angle_sub(dir, lwc1(W + 4)), frames));
            swc1(W + 0x18, fpu_div(speed - lwc1(W + 0x14), frames));
            sh(W + 0xe, 1);
        } else {
            const float turn = rand_range_f(u2f(0xbf490fd8u), u2f(0x3f490fd8u));
            const float dir = angle_add(lwc1(W), turn);
            const float speed = rand_range_f(base * u2f(0x3f4ccccdu), base * u2f(0x3f99999au));
            swc1(W + 8, fpu_div(angle_sub(dir, lwc1(W + 4)), frames));
            swc1(W + 0x18, fpu_div(speed - lwc1(W + 0x14), frames));
            sh(W + 0xe, 3);
        }
        sh(W + 0x1e, static_cast<uint32_t>(float_to_int(frames)));
        return;
    }
    if (ph == 1 || ph == 3) {
        if (timer_tick_i16(h16(W + 0x1e)) == 0) {
            step();
            return;
        }
        if (ph == 1) rest(1.0f, 1.5f, 2);
        else rest(3.0f, 5.0f, 0);
    }
}

}  // namespace rac1
