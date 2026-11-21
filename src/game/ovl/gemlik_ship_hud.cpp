// Gemlik Base's (level 13) space fight HUD (Qwark's fight), ported by hand from the level's own code: the draw-list callback of
// Ratchet's ship (class 69, L13:0x2b97f8, queued by its update every frame: gemlik_ship.cpp) with its three helpers: the lock-on
// (0x2b95b8), the radar's blips and the targets' reticles over the drawn mobys (0x2b8be0) and one blip (0x2b89f8). Level 13 only:
// `orig: L13:`, the level's own data L13_ADDR, the base ELF bare. The original's order of float operations kept; its EE frames of
// its own sizes and offsets (their vectors are handed to the callees by address).
//
// It carries the 18 HUD-width hooks the generator placed in its code (tools/codegen/hooks.glue before task H2, the space fights' HUD,
// #82): render_view_hud_begin(side) before each call that draws a side element (the radar's two sprites and each blip: side 2, the
// missiles: side 1), render_view_hud_end() at the call's return address. The original's code reaches one of those return addresses,
// 0x2b9560 (the blip of a class 0x4bf), also on other paths: after the blip of the classes 0x184 / 0x3eb / 0x504, and for the classes
// 389 .. 400 and from 1285, which draw nothing: the port calls render_view_hud_end() there too (no range is pending then: nothing is
// noted). Batch 220's p13h_cb_2b97f8 checks the hooks' sequence against the original's (docs/oracle.md, "The HUD-width hooks").
//
// Not the same code as Drek's fleet's HUD (veldin_orbit_hud.cpp, L17:0x2eb9a8): here the radar is drawn last, the ring's times are
// frame_scale_i(500) / (75), the lock's class is +0x124 and a new lock's timer +0x138, a blip has no x flip, a candidate's flags are
// 0x1000 without 1 / 2, and a class 0x184 / 0x3eb / 0x504 off screen gets an arrow toward itself (no big ship's case).
//
// The ship's pvars (gemlik_ship.cpp's list): +0x34 / +0x38 its pitch / heading, +0x84 the missiles' lock held, +0x88 the target, +0x8c
// the lock's timer, +0xe0 / +0xe4 the crosshair, +0xe8 the reticled targets counted, +0xec the target locked (for the missiles), +0xf0 /
// +0xf4 the health bar's value and index, +0x104 the radar's textures' base, +0x114 the arrows' phase, +0x118 the frames off target,
// +0x124 the target's class, +0x138 a new lock's timer.
#include <game/ovl/gemlik_mobys.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/save_menus.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/variants_l13.h>
#include <game/render/render_view.h>

namespace rac1 {

namespace {
// an EE stack frame of the original's size (its vectors are passed to the callees by address)
struct Frame {
    uint32_t n, sp;
    explicit Frame(uint32_t size) : n(size), sp(ee_stack_alloc(size)) {}
    ~Frame() { ee_stack_free(n); }
    Frame(const Frame&) = delete;
    Frame& operator=(const Frame&) = delete;
};

constexpr uint32_t P = 0x0013f350;  // the hero (+0x80 his place, +0x15f6 / +0x15f7 the missiles / their room, +0x15fc / +0x1600 the ship's health / its full)
constexpr float kHalfPi = 1.5707963705062866f;     // 0x3fc90fdb
constexpr float kQuarterPi = 0.7853981852531433f;  // 0x3f490fdb
constexpr float kPi = 3.1415927410125732f;         // 0x40490fdb

// a 32-bit word as the EE holds it in a 64-bit register (lui / ori, sll: sign-extended)
uint64_t sx64(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }

// a reticle (the line strip L13:0x1cbd40, 30 points) at the projected point (at + 0, at + 4) less the screen's centre (0x13e510 /
// 0x13e514), over 16, turned by the frame counter over 20 (or against it)
void reticle(uint32_t at, uint64_t rgba, float scale, bool negate) {
    const float x = fpu_mul(fpu_sub(lwc1(at + 0x0), int_to_float(lw(0x0013e510u))), 0.0625f);
    const float y = fpu_mul(fpu_sub(lwc1(at + 0x4), int_to_float(lw(0x0013e514u))), 0.0625f);
    const float a = angle_wrap_mod(fpu_div(int_to_float(lw(L13_ADDR(0x0015f5ccu))), 20.0f));
    hud_line_strip_turned(L13_ADDR(0x001cbd40u), 0x1e, 0x00fffff3u, rgba, x, y, scale, negate ? -a : a);
}

// the red pair of reticles (0x500000ff, 1 and 1.3, turning both ways) at the projected point `at`
void red_reticles(uint32_t at) {
    reticle(at, 0x500000ffu, 1.0f, false);
    reticle(at, 0x500000ffu, 1.2999999523162842f, true);
}
}  // namespace

// A blip of the radar: the moby `target` seen from `cam_pos` in the turn `rot` (+8 its heading), scaled by 1/14, drawn when nearer
// than 46 (fading out from 38) with the icon `icon` (records L13:0x1cbdb8, 16 bytes) of the textures from `tex_base`.
// orig: L13:0x002b89f8
void gml_2b89f8_l13(uint32_t rot, uint32_t cam_pos, uint32_t target, uint32_t icon, uint32_t color, uint32_t tex_base) {
    const Frame f(0x110);
    const uint32_t sp = f.sp;
    for (uint32_t k = 0; k < 16; k += 4) sw(sp + 0x10 + k, 0);
    swc1(sp + 0x18, lwc1(rot + 0x8));
    swc1(sp + 0x18, angle_sub(kHalfPi, lwc1(rot + 0x8)));
    euler_to_mat4_vu(M(sp + 0x20), V(sp + 0x10));
    vec3_sub(V(sp + 0x60), V(target + 0x10), V(cam_pos));
    sw(sp + 0x68, 0);
    mat4_transform(V(sp + 0x70), V(sp + 0x60), M(sp + 0x20));
    vec3_scale(V(sp + 0x70), V(sp + 0x70), u2f(0x3d924925u));
    const float dist = vec3_length(V(sp + 0x70));
    const int32_t rx = float_to_int(lwc1(sp + 0x70));
    const int32_t ry = float_to_int(-lwc1(sp + 0x74));
    if (!(dist < 46.0f)) return;
    uint64_t c = sx64(color);
    if (38.0f < dist) {  // (fading: the alpha times the distance left to 46, over 8)
        const int32_t a = static_cast<int32_t>((c >> 24) & 0xffu);
        const float fade = fpu_mul(fpu_mul(int_to_float(a), fpu_sub(46.0f, dist)), 0.125f);
        c = sx64(static_cast<uint32_t>(float_to_int(fade)) << 24) | (c & 0x00ffffffu);
    }
    const uint32_t rec = L13_ADDR(0x001cbdb8u) + (icon << 4);
    const uint64_t tex = effect_tex0(static_cast<int32_t>(tex_base + static_cast<uint32_t>(lh(rec + 0x0) + 0x28)));
    const int32_t w = lh(rec + 0x6), h = lh(rec + 0x8);
    const int32_t x = (rx + 0x1b0) - lh(rec + 0xc);
    const int32_t y = (ry - 0x50 + lw(0x0013e504u)) - lh(rec + 0xe);
    sd(sp + 0x0, c);
    sd(sp + 0x8, tex);
    glyph_sprite(x, y, w, h, lh(rec + 0x2), lh(rec + 0x4), w, h, c, tex);
}

// The radar's blips and the targets' reticles over the chain of the drawn mobys (L13:0x15ffe4, by +0x28), and the lock's next target:
// the current one `cur` while it stays inside its lock radius on screen, else (none) the nearest to the crosshair of the candidates
// (flags 0x1000 without 1 / 2, drawn, class record +0x46 == 5) but `keep` (the one the pad just let go). The classes 0x184 / 0x3eb /
// 0x504 get red reticles (not 0x184) and are counted (+0xe8); off screen, an arrow toward them, or the reticles when the ship aims
// within a quarter turn of them. 0x191 gets the reticles alone. Returns the target.
// orig: L13:0x002b8be0
uint32_t gml_2b8be0_l13(uint32_t ship, uint32_t pv, uint32_t cam_pos, uint32_t rot, uint32_t cur, uint32_t keep) {
    const Frame f(0x160);
    const uint32_t sp = f.sp;
    uint32_t target = 0;
    int32_t best = 0x7fffffff;  // (the candidates' distance to the crosshair, squared)
    sw(pv + 0xe8, 0);
    // the blip of `m` (icon, colour) between the hooks of its call
    auto blip = [&](uint32_t m, uint32_t icon, uint32_t color) {
        render_view_hud_begin(2);  // enhancement: the HUD's width (a blip)
        gml_2b89f8_l13(rot, cam_pos, m, icon, color, lwu(pv + 0x104));
        render_view_hud_end();
    };
    // the green reticle (0xff00ff00, 1, turning back) of a drawn moby at its projection, then its blip (icon 1)
    auto green = [&](uint32_t m) {
        screen_project(sp + 0x0, m + 0x10);
        if (lbu(m + 0x31) != 0) reticle(sp, 0xff00ff00u, 1.0f, true);
        blip(m, 1, 0x75808080u);
    };
    for (uint32_t m = lwu(L13_ADDR(0x0015ffe4u)); m != 0; m = lwu(m + 0x28)) {
        // a target candidate: flags 0x1000 (without 1 / 2), drawn, an enemy, inside its lock radius (its size +0xc over 1280 in the
        // camera's row L13:0x1672e0, projected) about the crosshair
        const uint32_t cls_rec = lwu(m + 0x24);
        if ((lhu(m + 0x34) & 0x1003) == 0x1000 && lbu(m + 0x31) != 0 && cls_rec != 0 && lh(cls_rec + 0x46) == 5) {
            hud_screen_point(m + 0x10, sp + 0x60, sp + 0x64, 0);
            int32_t dx = static_cast<int32_t>(lwu(sp + 0x60) - lwu(pv + 0xe0));
            int32_t dy = static_cast<int32_t>(lwu(sp + 0x64) - lwu(pv + 0xe4));
            if (!(-1 < dx)) dx = static_cast<int32_t>(0u - static_cast<uint32_t>(dx));
            if (!(-1 < dy)) dy = static_cast<int32_t>(0u - static_cast<uint32_t>(dy));
            lq_sq(sp + 0x0, L13_ADDR(0x001672e0u));
            vec3_scale(V(sp + 0x0), V(L13_ADDR(0x001672e0u)), fpu_div(lwc1(m + 0xc), 1280.0f));
            vec3_add(V(sp + 0x0), V(sp + 0x0), V(m + 0x10));
            hud_screen_point(sp + 0x0, sp + 0x68, sp + 0x6c, 0);
            const uint32_t rx = lwu(sp + 0x68) - lwu(sp + 0x60), ry = lwu(sp + 0x6c) - lwu(sp + 0x64);
            sw(sp + 0x6c, ry);
            sw(sp + 0x68, rx);
            const int32_t lim = float_to_int(sqrt_f(int_to_float(static_cast<int32_t>(rx * rx + ry * ry)))) + 0x1c;
            if (dx < lim && dy < lim) {
                if (cur != 0) {
                    if (m == cur) {
                        best = 0;
                        target = m;
                    }
                } else if (m != keep) {
                    const int32_t d = static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) +
                                                           static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy));
                    if (d < best) {
                        target = m;
                        best = d;
                    }
                }
            }
        }
        const int32_t cls = lh(m + 0xa6);
        switch (cls) {
            case 0x52: blip(m, 1, 0x75004080u); break;  // (the hooks at 0x2b8f48 / 0x2b8f50)
            case 0x53: blip(m, 1, 0x75007080u); break;  // (0x2b8f70 / 0x2b8f78)
            case 0xe0: case 0xe4: case 0x4c2: case 0x4c4: blip(m, 2, 0x75808080u); break;  // (0x2b948c / 0x2b9494)
            case 0x6f: case 0x4be:  // a green reticle when drawn: not hidden (+0x34 & 1)
                if ((lhu(m + 0x34) & 1) != 0) break;
                green(m);  // (0x2b948c / 0x2b9494)
                break;
            case 0x4bf:  // the same from its state 2
                if ((lhu(m + 0x34) & 1) != 0 || lbu(m + 0x20) < 2) break;
                green(m);  // (0x2b9558 / 0x2b9560)
                break;
            case 0x184: case 0x3eb: case 0x504: case 0x191: {
                screen_project(sp + 0x0, m + 0x10);
                if (cls != 0x184 && lbu(m + 0x31) != 0) red_reticles(sp);
                if (cls == 0x191) break;  // (the reticles alone: no blip, not counted)
                if (lbu(m + 0x31) == 0) {
                    // off screen: the arrow toward it in the camera's turn (L13:0x1672d0, transposed) from the hero, 160 from the
                    // screen's centre (256, 208), its alpha pulsing (+0x114) faster the nearer the ship's aim is to it; the red
                    // reticles instead when the ship aims within a quarter turn of it both ways
                    mat3_transpose(M(sp + 0x10), M(L13_ADDR(0x001672d0u)));
                    vec3_sub(V(sp + 0x50), V(m + 0x10), V(P + 0x80));
                    mat4_transform(V(sp + 0x50), V(sp + 0x50), M(sp + 0x10));
                    const float ax = -lwc1(sp + 0x54), ay = -lwc1(sp + 0x58);
                    sw(sp + 0x58, 0);
                    swc1(sp + 0x50, ax);
                    swc1(sp + 0x54, ay);
                    vec3_set_length(V(sp + 0x50), V(sp + 0x50), 160.0f);
                    const float turn = angle_add(atan2_f(lwc1(sp + 0x50), lwc1(sp + 0x54)), kHalfPi);
                    swc1(sp + 0x50, fpu_add(lwc1(sp + 0x50), 256.0f));
                    swc1(sp + 0x54, fpu_add(lwc1(sp + 0x54), 208.0f));
                    const float dist = vec3_distance(V(ship + 0x10), V(m + 0x10));
                    const float yaw = abs_f(angle_sub(atan2_f(fpu_sub(lwc1(m + 0x10), lwc1(ship + 0x10)), fpu_sub(lwc1(m + 0x14), lwc1(ship + 0x14))),
                                                      lwc1(pv + 0x38)));
                    const float flat = vec2_distance(V(ship + 0x10), V(m + 0x10));
                    const float pitch = abs_f(angle_sub(-atan2_f(flat, fpu_sub(lwc1(m + 0x18), lwc1(ship + 0x18))), lwc1(pv + 0x34)));
                    const float off = fpu_div(sqrt_f(fpu_add(fpu_mul(yaw, yaw), fpu_mul(pitch, pitch))), kPi);
                    const float phase = angle_add(lwc1(pv + 0x114), fpu_div(0.10000000149011612f, off));
                    swc1(pv + 0x114, phase);
                    const int32_t alpha = float_to_int(fpu_mul(fpu_add(sin_vu(phase), 1.399999976158142f), 53.0f));
                    if (yaw < kQuarterPi && pitch < kQuarterPi) {
                        red_reticles(sp);
                    } else {
                        const float scale = fpu_add(fpu_div(240.0f, dist), 1.5f);
                        hud_line_strip_turned(L13_ADDR(0x001613c8u), 3, 0x00fffff3u, sx64((static_cast<uint32_t>(alpha) << 24) | 0x8000u),
                                              lwc1(sp + 0x50), lwc1(sp + 0x54), scale, turn);
                    }
                }
                blip(m, 0, 0x75808080u);  // (0x2b93b0 / 0x2b93b8)
                sw(pv + 0xe8, lw(pv + 0xe8) + 1);
                render_view_hud_end();  // (the original's code goes on through 0x2b9560, the end of the 0x4bf's blip: none pending)
                break;
            }
            default:
                // (the classes 389 .. 400 and from 1285 go through 0x2b9560, the end of the 0x4bf's blip: none pending)
                if ((cls >= 389 && cls <= 400) || cls >= 1285) render_view_hud_end();
                break;
        }
    }
    return target;
}

// The lock-on: the timer +0x8c run (but in the view modes 3 / 4), the target dropped when it is gone, dead (0xfe / 0xfd), of another
// class than locked (+0x124), off screen or the missiles are out, or let go by the pad (0x2000: kept from the next choice); the next
// target chosen (0x2b8be0; a new one locked with the timer +0x138 while missiles are left, but not over a locked class 0x3eb by another
// class); the target dropped after frame_scale_i(60) frames off it, or when it died or changed class.
// orig: L13:0x002b95b8
void gml_2b95b8_l13(uint32_t ship, uint32_t pv) {
    const Frame f(0x50);
    const uint32_t sp = f.sp;
    auto drop = [pv] {
        sw(pv + 0xec, 0);
        sw(pv + 0x124, 0xffffffffu);
        sw(pv + 0x88, 0);
    };
    if (!(lwu(L13_ADDR(0x0015f5c4u)) - 3u < 2u)) timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x8c)));
    uint32_t keep = 0;
    bool held = false;
    if (lw(pv + 0x8c) != 0) {
        const uint32_t t = lwu(pv + 0x88);
        if (t != 0 && lh(t + 0xa6) == lw(pv + 0x124) && lbu(t + 0x20) != 0xfe && lbu(t + 0x20) != 0xfd) {
            if (lbu(t + 0x31) != 0) {
                if ((lw(0x0013caf4u) & 0x2000) == 0 && lbu(0x00140946u) != 0) held = true;
            }
        }
    }
    if (!held) {
        if ((lw(0x0013caf4u) & 0x2000) != 0) keep = lwu(pv + 0x88);
        drop();
    }
    lq_sq(sp + 0x0, L13_ADDR(0x001670d0u));
    // (the original also passes f12 = f13 = 0x3e490fdb, f14 = 255: 0x2b8be0 reads none of them)
    const uint32_t next = gml_2b8be0_l13(ship, pv, L13_ADDR(0x001670c0u), sp, lwu(pv + 0x88), keep);
    if (!((next == lwu(pv + 0x88) || lw(pv + 0x8c) < 501) && lbu(0x00140946u) != 0)) drop();
    if (next != 0) {
        const uint32_t t = lwu(pv + 0x88);
        const bool over_3eb = t != 0 && (lh(t + 0xa6) == 0x3eb || lh(next + 0xa6) != 0x3eb);
        if (!over_3eb && lbu(0x00140946u) != 0) {
            sw(pv + 0x88, next);
            sw(pv + 0x8c, lwu(pv + 0x138));
            sw(pv + 0x124, static_cast<uint32_t>(lh(next + 0xa6)));
        }
    }
    if (next == lwu(pv + 0x88)) {
        sw(pv + 0x118, 0);
    } else {
        sw(pv + 0x118, lwu(pv + 0x118) + 1);
        if (frame_scale_i(0x3c) < lw(pv + 0x118)) drop();
    }
    const uint32_t t = lwu(pv + 0x88);
    if (t == 0 || lw(pv + 0x84) != 0) return;
    if (lh(t + 0xa6) != lw(pv + 0x124) || lbu(t + 0x20) == 0xfe || lbu(t + 0x20) == 0xfd) {
        sw(pv + 0x88, 0);
        sw(pv + 0x124, 0xffffffffu);
    }
}

// The ship's HUD (its draw-list callback, queued each frame by its update): the lock-on (0x2b95b8) with the radar's blips (0x2b8be0),
// the lock's ring over the target (growing and fading in the timer's first frame_scale_i(75) frames past frame_scale_i(500), then
// blinking red / yellow), the missiles at the frame's left (+0x15f6 loaded of +0x15f7, two columns), the crosshair, the low-health
// warning (blinking, under a tenth), the health bar's colour strip (*L13:0x1613c0 from L13:0x1cbe70) eased toward the health, and the
// radar's two sprites (the textures +0x104 + 0x28 / + 0x29) at the frame's right.
// orig: L13:0x002b97f8
void gml_cb_2b97f8_l13(uint32_t m) {
    const Frame f(0xd0);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    gml_2b95b8_l13(m, pv);

    if (const uint32_t tgt = lwu(pv + 0x88); tgt != 0) {
        hud_screen_point(tgt + 0x10, sp + 0x10, sp + 0x14, 0);
        if (frame_scale_i(0x1f4) < lw(pv + 0x8c)) {
            const int32_t past = lw(pv + 0x8c) - frame_scale_i(0x1f4);
            float frac = fpu_div(int_to_float(past), int_to_float(frame_scale_i(0x4b)));
            float a = fpu_sub(1.0f, frac);
            frac = fpu_mul(frac, 5.0f);
            a = fpu_add(a, a);
            const float size = fpu_add(frac, 1.0f);
            if (1.0f < a) a = 1.0f;
            const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(a, 96.0f))) & 0xffu;
            const float x = int_to_float(lw(sp + 0x10)), y = int_to_float(lw(sp + 0x14));
            const float ang = angle_wrap_mod(fpu_div(int_to_float(lw(L13_ADDR(0x0015f5ccu))), 30.0f));
            hud_ring(0, 0xff, 0, alpha, x, y, size, ang);
            sw(pv + 0xec, 0);
        } else {
            const int32_t period = frame_scale_i(0x14);
            const uint32_t g = (ee_div(lw(pv + 0x8c), period) & 1) != 0 ? 0u : 0xffu;
            hud_ring(0xff, g, 0, 0x60, int_to_float(lw(sp + 0x10)), int_to_float(lw(sp + 0x14)), 1.0f, 0.0f);
            sw(pv + 0xec, lwu(pv + 0x88));
        }
    }

    if (lbu(P + 0x15f7) != 0) {
        int32_t x = 0x18, y = 0x40, i = 0;
        for (;;) {
            const bool loaded = i < static_cast<int32_t>(lbu(P + 0x15f6));
            render_view_hud_begin(1);  // enhancement: the HUD's width (a missile: 0x2b9a18 loaded, 0x2b9a58 not)
            hud_line_strip(L13_ADDR(0x001cbc60u), 0x19, 0x00fffff3u, loaded ? 0x50008f00u : 0x20004f00u, int_to_float(x), int_to_float(y), 0.5f);
            render_view_hud_end();
            const int32_t cap = static_cast<int32_t>(lbu(P + 0x15f7));
            if (i == (cap >> 1)) {
                y = 0x2e;
                x += 0x1e;
            }
            ++i;
            if (!(i < cap)) break;
            y += 0x12;
        }
    }

    const float cx = int_to_float(lw(pv + 0xe0));
    const float cy = int_to_float(lw(pv + 0xe4));
    screen_sprite(0x3f, 0x3f, effect_tex0(0x11), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x3f, 0x3f, effect_tex0(0x12), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x1f, 0x1f, effect_tex0(0x08), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 10.0f, 10.0f, 0.0f, 0.5f, 0.5f);

    if (lwc1(P + 0x15fc) < fpu_div(lwc1(P + 0x1600), 10.0f) && (ee_div(lw(L13_ADDR(0x0015f5ccu)), 0x5a) & 1) != 0)
        text_centered_med(0x100, 0x186, 0x80000080u, sub_00203ad8(static_cast<int32_t>(TXT(0x5269))), 0x64);

    // the health bar's value eased toward the ship's health, its colour strip's entries between the old and new index rewritten
    float delta = fpu_mul(fpu_sub(lwc1(P + 0x15fc), lwc1(pv + 0xf0)), u2f(0x3e4ccccdu));
    const float abs_d = abs_f(delta);
    if (1.0f < abs_d) delta = fpu_div(delta, abs_d);
    const float cur = fpu_add(lwc1(pv + 0xf0), delta);
    swc1(pv + 0xf0, cur);
    int32_t bar = float_to_int(fpu_div(fpu_mul(cur, 251.0f), lwc1(P + 0x1600))) + 2;
    if (!(bar < 254)) bar = 0xfd;
    if (!(1 < bar)) bar = 2;
    const int32_t prev = lw(pv + 0xf4);
    const int32_t lo_i = bar < prev ? bar : prev;
    const int32_t hi_i = prev < bar ? bar : prev;
    for (int32_t k = lo_i; k <= hi_i; ++k) {
        const uint32_t uk = static_cast<uint32_t>(k);
        const uint32_t idx = (uk & 0xe7u) | ((uk & 0x10u) >> 1) | ((uk & 0x08u) << 1);
        const uint32_t dst = lwu(L13_ADDR(0x001613c0u)) + (idx << 2);
        sw(dst, k < bar ? lwu(L13_ADDR(0x001cbe70u) + (idx << 2)) : 0x80000000u);
    }

    // the radar's two sprites at the frame's right
    const uint64_t radar_col = 0x70808080u;
    const uint64_t tex_a = effect_tex0(static_cast<int32_t>(lwu(pv + 0x104) + 0x28));
    sd(sp + 0x0, radar_col);
    sd(sp + 0x8, tex_a);
    render_view_hud_begin(2);  // enhancement: the HUD's width (the radar)
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_a);
    render_view_hud_end();
    const uint64_t tex_b = effect_tex0(static_cast<int32_t>(lwu(pv + 0x104) + 0x29));
    sd(sp + 0x0, radar_col);
    sd(sp + 0x8, tex_b);
    render_view_hud_begin(2);
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_b);
    render_view_hud_end();
    sw(pv + 0xf4, static_cast<uint32_t>(bar));
}

}  // namespace rac1
