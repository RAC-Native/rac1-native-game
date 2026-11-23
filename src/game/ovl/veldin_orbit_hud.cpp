// Drek's fleet's (level 17) space fight HUD, ported by hand from the level's own code: the draw-list callback of the hero's
// ship (class 1379, L17:0x2eb9a8, queued by its update every frame) with its three helpers: the lock-on (0x2eb768), the radar's
// blips and the targets' reticles over the drawn mobys (0x2eace8) and one blip (0x2eaae0). Level 17 only: `orig: L17:`, the
// level's own data L17_ADDR, the base ELF bare. The original's order of float operations kept; its EE frames of its own sizes.
//
// It carries the 18 HUD-width hooks the generator placed in its code (once tools/codegen/hooks.glue's, the space fights' HUD, #82):
// render_view_hud_begin(side) before each call that draws a side element (the radar's two sprites and each blip: side 2, the
// missiles: side 1), render_view_hud_end() at the call's return address. The original's code reaches one of those return
// addresses, 0x2eb4e4, also on two other paths (after the blip of a class 0x15b, and for the classes 399 / 400, which draw
// nothing): the port calls render_view_hud_end() there too (no range is pending then: nothing is noted). Batch 260's
// p17_cb_2eb9a8 checks the hooks' sequence against the original's (docs/oracle.md, "The HUD-width hooks").
//
// The ship's pvars: +0x34 / +0x38 its pitch / heading, +0x84 the missiles' lock held (the target kept), +0x88 the target, +0x8c
// the lock's timer, +0xe0 / +0xe4 the crosshair, +0xe8 the blips of enemies counted, +0xec the target locked (for the missiles),
// +0xf0 / +0xf4 the health bar's value and index, +0x104 the radar's textures' base, +0x114 the arrow's phase, +0x118 the frames
// off target, +0x128 the target's class.
#include <game/ovl/veldin_orbit_mobys.h>

#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/enemy_749.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/save_menus.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/shared_hud_shapes.h>
#include <game/ovl/variants_l17.h>
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
constexpr float kHalfPi = 1.5707963705062866f;  // 0x3fc90fdb

// a 32-bit word as the EE holds it in a 64-bit register (lui / ori, sll: sign-extended)
uint64_t sx64(uint32_t v) { return static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(v))); }

// the reticles' angle of the frame: the frame counter over 20, wrapped
float reticle_angle() { return angle_wrap_mod(fpu_div(int_to_float(lw(L17_ADDR(0x0015f5ccu))), 20.0f)); }

// a reticle (the line strip L17:0x1d9ac0, 30 points) at the projected point (sp + 0, sp + 4) less the screen's centre
// (0x13e510 / 0x13e514), over 16
void reticle(uint32_t at, uint64_t rgba, float scale, bool negate) {
    const float x = fpu_mul(fpu_sub(lwc1(at + 0x0), int_to_float(lw(0x0013e510u))), 0.0625f);
    const float y = fpu_mul(fpu_sub(lwc1(at + 0x4), int_to_float(lw(0x0013e514u))), 0.0625f);
    const float a = reticle_angle();
    hud_line_strip_turned(L17_ADDR(0x001d9ac0u), 0x1e, 0x00fffff3u, rgba, x, y, scale, negate ? -a : a);
}
}  // namespace

// A blip of the radar: the moby `target` seen from `cam_pos` in the turn `rot` (+8 its heading), scaled by 1/14, drawn when
// nearer than 46 (fading out from 38) with the icon `icon` (records L17:0x1d9b38, 16 bytes) of the textures from `tex_base`.
// orig: L17:0x002eaae0
void vo_2eaae0_l17(uint32_t rot, uint32_t cam_pos, uint32_t target, uint32_t icon, uint32_t color, uint32_t tex_base) {
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
    const float sign_x = lbu(0x0015edb4u) != 0 ? -1.0f : 1.0f;
    const int32_t rx = float_to_int(fpu_mul(sign_x, lwc1(sp + 0x70)));
    const int32_t ry = float_to_int(-lwc1(sp + 0x74));
    if (!(dist < 46.0f)) return;
    uint64_t c = sx64(color);
    if (38.0f < dist) {
        const int32_t a = static_cast<int32_t>((c >> 24) & 0xffu);
        const float fade = fpu_mul(fpu_mul(int_to_float(a), fpu_sub(46.0f, dist)), 0.125f);
        c = sx64(static_cast<uint32_t>(float_to_int(fade)) << 24) | (c & 0x00ffffffu);
    }
    const uint32_t rec = L17_ADDR(0x001d9b38u) + (icon << 4);
    const uint64_t tex = effect_tex0(static_cast<int32_t>(tex_base + static_cast<uint32_t>(lh(rec + 0x0) + 0x28)));
    const int32_t w = lh(rec + 0x6), h = lh(rec + 0x8);
    const int32_t x = (rx + 0x1b0) - lh(rec + 0xc);
    const int32_t y = (ry - 0x50 + lw(0x0013e504u)) - lh(rec + 0xe);
    sd(sp + 0x0, c);
    sd(sp + 0x8, tex);
    glyph_sprite(x, y, w, h, lh(rec + 0x2), lh(rec + 0x4), w, h, c, tex);
}

// The radar's blips and the targets' reticles over the chain of the drawn mobys (L17:0x15ffe4, by +0x28), and the lock's next
// target: the current one `cur` while it stays inside its lock radius on screen, else (none) the nearest to the crosshair of
// the enemies (class record +0x46 == 5) but `keep` (the one the pad just let go). The class 0x15b (the big ship) gets two
// reticles and, when none of them is on screen, an arrow toward the nearest. Returns the target.
// orig: L17:0x002eace8
uint32_t vo_2eace8_l17(uint32_t ship, uint32_t pv, uint32_t cam_pos, uint32_t rot, uint32_t cur, uint32_t keep) {
    const Frame f(0x170);
    const uint32_t sp = f.sp;
    sw(sp + 0x80, ship);
    sw(sp + 0x84, cur);
    sw(sp + 0x88, keep);
    sw(sp + 0x8c, 0);
    sw(pv + 0xe8, 0);
    sw(sp + 0x90, 0);
    sw(sp + 0x94, 0);
    float nearest = 100000.0f;  // (the class 0x15b nearest to the hero)
    int32_t best = 0x7fffffff;  // (the candidates' distance to the crosshair, squared)
    // the blip of `m` (icon, colour) between the hooks of its call
    auto blip = [&](uint32_t from, uint32_t m, uint32_t icon, uint32_t color) {
        render_view_hud_begin(2);  // enhancement: the HUD's width (a blip)
        vo_2eaae0_l17(rot, from, m, icon, color, lwu(pv + 0x104));
        render_view_hud_end();
    };
    auto enemy = [](uint32_t m) {
        const uint32_t cls = lwu(m + 0x24);
        return cls != 0 && lh(cls + 0x46) == 5;
    };
    for (uint32_t m = lwu(L17_ADDR(0x0015ffe4u)); m != 0;) {
        // a target candidate: drawn, on screen, an enemy, inside its lock radius (its size +0xc over 1280 in the camera's row
        // 0x1678e0, projected) about the crosshair
        if ((lhu(m + 0x34) & 0x1000) != 0 && lbu(m + 0x31) != 0 && enemy(m)) {
            hud_screen_point(m + 0x10, sp + 0x70, sp + 0x74, 0);
            int32_t dx = static_cast<int32_t>(lwu(sp + 0x70) - lwu(pv + 0xe0));
            int32_t dy = static_cast<int32_t>(lwu(sp + 0x74) - lwu(pv + 0xe4));
            if (!(-1 < dx)) dx = static_cast<int32_t>(0u - static_cast<uint32_t>(dx));
            if (!(-1 < dy)) dy = static_cast<int32_t>(0u - static_cast<uint32_t>(dy));
            lq_sq(sp + 0x0, L17_ADDR(0x001678e0u));
            vec3_scale(V(sp + 0x0), V(L17_ADDR(0x001678e0u)), fpu_div(lwc1(m + 0xc), 1280.0f));
            vec3_add(V(sp + 0x0), V(sp + 0x0), V(m + 0x10));
            hud_screen_point(sp + 0x0, sp + 0x78, sp + 0x7c, 0);
            const uint32_t rx = lwu(sp + 0x78) - lwu(sp + 0x70), ry = lwu(sp + 0x7c) - lwu(sp + 0x74);
            sw(sp + 0x7c, ry);
            sw(sp + 0x78, rx);
            const int32_t lim = float_to_int(sqrt_f(int_to_float(static_cast<int32_t>(rx * rx + ry * ry)))) + 0x1c;
            if (dx < lim && dy < lim) {
                if (cur != 0) {
                    if (m == cur) {
                        best = 0;
                        sw(sp + 0x8c, m);
                    }
                } else if (m != keep) {
                    const int32_t d = static_cast<int32_t>(static_cast<uint32_t>(dx) * static_cast<uint32_t>(dx) +
                                                           static_cast<uint32_t>(dy) * static_cast<uint32_t>(dy));
                    if (d < best) {
                        sw(sp + 0x8c, m);
                        best = d;
                    }
                }
            }
        }
        const int32_t cls = static_cast<int16_t>(lhu(m + 0xa6));
        const uint32_t next_of = m;
        switch (cls) {
            case 0x52: blip(cam_pos, m, 1, 0x70004080u); break;  // (the hooks at 0x2eb074 / 0x2eb07c)
            case 0x53: blip(cam_pos, m, 1, 0x70007080u); break;  // (0x2eb09c / 0x2eb0a4)
            case 0xe0: case 0xe4: case 0x4c2: case 0x4c4: blip(cam_pos, m, 2, 0x70808080u); break;  // (0x2eb490 / 0x2eb498)
            case 0x184: case 0x3eb: case 0x504: case 0x191: {  // two reticles when drawn (not 0x184), an enemy counted (not 0x191)
                screen_project(sp + 0x0, m + 0x10);
                if (cls != 0x184 && lbu(m + 0x31) != 0) {
                    reticle(sp, 0x500000ffu, 1.0f, false);
                    reticle(sp, 0x500000ffu, 1.2999999523162842f, true);
                }
                if (cls != 0x191) sw(pv + 0xe8, lw(pv + 0xe8) + 1);
                blip(cam_pos, m, 0, 0x70808080u);  // (0x2eb490 / 0x2eb498)
                break;
            }
            case 0x6f: case 0x4be: case 0x4bf: {  // a green reticle when drawn: not hidden (+0x34 & 1), 0x4bf from its state 2
                if ((lhu(m + 0x34) & 1) != 0) break;
                if (cls == 0x4bf && lbu(m + 0x20) < 2) break;
                screen_project(sp + 0x0, m + 0x10);
                if (lbu(m + 0x31) != 0) reticle(sp, 0xff00ff00u, 1.0f, true);
                blip(cam_pos, m, 1, 0x70808080u);  // (0x2eb490 / 0x2eb498)
                break;
            }
            case 0x15b: {  // the big ship: two reticles 4 above it when drawn, the nearest to the hero kept for the arrow
                const uint32_t s0 = sp + 0x10;
                lq_sq(s0, m + 0x10);
                swc1(sp + 0x18, fpu_add(lwc1(sp + 0x18), 4.0f));
                screen_project(sp + 0x0, s0);
                if (lbu(m + 0x31) != 0) {
                    sw(sp + 0x90, 1);
                    reticle(sp, 0x400000ffu, 2.0f, false);
                    reticle(sp, 0x400000ffu, 2.5f, true);
                }
                const float d = vec3_distance(V(m + 0x10), V(P + 0x80));
                if (d < nearest) {
                    sw(sp + 0x94, m);
                    nearest = d;
                }
                blip(s0, m, 0, 0x70808080u);  // (0x2eb344 / 0x2eb34c: seen from the point 4 above itself, as the original passes it)
                sw(pv + 0xe8, lw(pv + 0xe8) + 1);
                render_view_hud_end();  // (the original's code goes on through 0x2eb4e4, the end of the blip below)
                break;
            }
            case 399: case 400:
                render_view_hud_end();  // (the original's code goes on through 0x2eb4e4, the end of the blip below: none pending)
                break;
            default:
                if ((cls >= 389 && cls <= 396) || (cls >= 1332 && cls <= 1338)) break;  // (no blip)
                if (!enemy(m)) break;
                render_view_hud_begin(2);  // (0x2eb4dc)
                vo_2eaae0_l17(rot, cam_pos, m, 1, 0x70808080u, lwu(pv + 0x104));
                render_view_hud_end();  // (0x2eb4e4)
                break;
        }
        m = lwu(next_of + 0x28);
    }
    (void)best;
    // the arrow toward the nearest big ship when none was on screen: in the camera's turn (L17:0x1678d0, transposed) from the
    // hero, 160 from the screen's centre (256, 208), its alpha pulsing (+0x114) faster the nearer the ship's aim is to it
    if (lw(sp + 0x90) == 0 && lwu(sp + 0x94) != 0) {
        const uint32_t n = lwu(sp + 0x94);
        const uint32_t s1 = n + 0x10, s2 = ship + 0x10;
        lq_sq(sp + 0x60, s1);
        swc1(sp + 0x68, fpu_add(lwc1(sp + 0x68), 4.0f));
        screen_project(sp + 0x10, sp + 0x60);
        mat3_transpose(M(sp + 0x20), M(L17_ADDR(0x001678d0u)));
        vec3_sub(V(sp + 0x0), V(s1), V(P + 0x80));
        mat4_transform(V(sp + 0x0), V(sp + 0x0), M(sp + 0x20));
        const float ax = -lwc1(sp + 0x4), ay = -lwc1(sp + 0x8);
        sw(sp + 0x8, 0);
        swc1(sp + 0x0, ax);
        swc1(sp + 0x4, ay);
        vec3_set_length(V(sp + 0x0), V(sp + 0x0), 160.0f);
        const float turn = angle_add(atan2_f(lwc1(sp + 0x0), lwc1(sp + 0x4)), kHalfPi);
        swc1(sp + 0x0, fpu_add(lwc1(sp + 0x0), 256.0f));
        swc1(sp + 0x4, fpu_add(lwc1(sp + 0x4), 208.0f));
        const float dist = vec3_distance(V(s2), V(s1));
        float yaw = abs_f(angle_sub(atan2_f(fpu_sub(lwc1(n + 0x10), lwc1(ship + 0x10)), fpu_sub(lwc1(n + 0x14), lwc1(ship + 0x14))), lwc1(pv + 0x38)));
        const float flat = vec2_distance(V(s2), V(s1));
        float pitch = abs_f(angle_sub(-atan2_f(flat, fpu_sub(lwc1(n + 0x18), lwc1(ship + 0x18))), lwc1(pv + 0x34)));
        pitch = fpu_mul(pitch, pitch);
        yaw = fpu_mul(yaw, yaw);
        const float off = fpu_div(sqrt_f(fpu_add(yaw, pitch)), 3.1415927410125732f);
        const float phase = angle_add(lwc1(pv + 0x114), fpu_div(0.10000000149011612f, off));
        swc1(pv + 0x114, phase);
        const int32_t alpha = float_to_int(fpu_mul(fpu_add(sin_vu(phase), 1.399999976158142f), 53.0f));
        const float scale = fpu_add(fpu_div(240.0f, dist), 1.5f);
        hud_line_strip_turned(L17_ADDR(0x00162188u), 3, 0x00fffff3u, sx64((static_cast<uint32_t>(alpha) << 24) | 0x8000u), lwc1(sp + 0x0),
                              lwc1(sp + 0x4), scale, turn);
    }
    return lwu(sp + 0x8c);
}

// The lock-on: the timer +0x8c run (but in the view modes 3 / 4), the target dropped when it is gone, dead (0xfe / 0xfd), of
// another class than locked, off screen or the missiles are out, or let go by the pad (0x2000: kept from the next choice); the
// next target chosen (0x2eace8; a new one locked with the timer L17:0x16216c while missiles are left, but not over a locked
// class 0x3eb by another class); the target dropped after frame_scale_i(60) frames off it, or when it died or changed class.
// orig: L17:0x002eb768
void vo_2eb768_l17(uint32_t ship, uint32_t pv) {
    const Frame f(0x50);
    const uint32_t sp = f.sp;
    auto drop = [pv] {
        sw(pv + 0xec, 0);
        sw(pv + 0x128, 0xffffffffu);
        sw(pv + 0x88, 0);
    };
    if (!(lwu(L17_ADDR(0x0015f5c4u)) - 3u < 2u)) timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(pv + 0x8c)));
    uint32_t keep = 0;
    bool held = false;
    if (lw(pv + 0x8c) != 0) {
        const uint32_t t = lwu(pv + 0x88);
        if (t != 0 && static_cast<int16_t>(lhu(t + 0xa6)) == lw(pv + 0x128) && lbu(t + 0x20) != 0xfe && lbu(t + 0x20) != 0xfd) {
            if (lbu(t + 0x31) != 0) {
                if ((lw(0x0013caf4u) & 0x2000) == 0 && lbu(0x00140946u) != 0) held = true;
            }
        }
    }
    if (!held) {
        if ((lw(0x0013caf4u) & 0x2000) != 0) keep = lwu(pv + 0x88);
        drop();
    }
    lq_sq(sp + 0x0, L17_ADDR(0x001676d0u));
    const uint32_t next = vo_2eace8_l17(ship, pv, L17_ADDR(0x001676c0u), sp, lwu(pv + 0x88), keep);
    if (!((next == lwu(pv + 0x88) || lw(pv + 0x8c) < 501) && lbu(0x00140946u) != 0)) drop();
    if (next != 0) {
        const uint32_t t = lwu(pv + 0x88);
        const bool over_3eb = t != 0 && (static_cast<int16_t>(lhu(t + 0xa6)) == 0x3eb || static_cast<int16_t>(lhu(next + 0xa6)) != 0x3eb);
        if (!over_3eb && lbu(0x00140946u) != 0) {
            sw(pv + 0x88, next);
            sw(pv + 0x8c, lwu(L17_ADDR(0x0016216cu)));
            sw(pv + 0x128, static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(lhu(next + 0xa6)))));
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
    if (static_cast<int16_t>(lhu(t + 0xa6)) != lw(pv + 0x128) || lbu(t + 0x20) == 0xfe || lbu(t + 0x20) == 0xfd) {
        sw(pv + 0x88, 0);
        sw(pv + 0x128, 0xffffffffu);
    }
}

// The ship's HUD (its draw-list callback, queued each frame by its update): the radar's two sprites (the textures +0x104 + 0x28
// / + 0x29) at the frame's right, the lock-on (0x2eb768) with the radar's blips (0x2eace8), the lock's ring over the target
// (growing and fading in the timer's first L17:0x162174 frames from L17:0x162170, then blinking red / yellow), the missiles at
// the frame's left (+0x15f6 loaded of +0x15f7, two columns), the crosshair, the low-health warning (blinking, under a tenth),
// the text of the ship's state 8, and the health bar's colour strip (*L17:0x162180 from L17:0x1d9b68) eased toward the health.
// orig: L17:0x002eb9a8
void vo_cb_2eb9a8_l17(uint32_t m) {
    const Frame f(0xe0);
    const uint32_t sp = f.sp;
    const uint32_t pv = lwu(m + 0x78);
    const uint64_t radar_col = 0x70808080u;
    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x44u);
    const uint64_t tex_a = effect_tex0(static_cast<int32_t>(lwu(pv + 0x104) + 0x28));
    sd(sp + 0x8, tex_a);
    sd(sp + 0x0, radar_col);
    render_view_hud_begin(2);  // enhancement: the HUD's width (the radar)
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_a);
    render_view_hud_end();
    const uint64_t tex_b = effect_tex0(static_cast<int32_t>(lwu(pv + 0x104) + 0x29));
    sd(sp + 0x0, radar_col);
    sd(sp + 0x8, tex_b);
    render_view_hud_begin(2);
    glyph_sprite(0x170, lw(0x0013e504u) - 0x90, 0x80, 0x80, 0, 0, 0x80, 0x80, radar_col, tex_b);
    render_view_hud_end();
    sub_002a0ba8(0x42, (0x8000ULL << 24) | 0x48u);
    vo_2eb768_l17(m, pv);

    if (const uint32_t tgt = lwu(pv + 0x88); tgt != 0) {
        hud_screen_point(tgt + 0x10, sp + 0x10, sp + 0x14, 0);
        const int32_t t = lw(pv + 0x8c);
        const int32_t base = lw(L17_ADDR(0x00162170u));
        if (base < t) {
            float frac = fpu_div(int_to_float(t - base), int_to_float(lw(L17_ADDR(0x00162174u))));
            float a = fpu_sub(1.0f, frac);
            frac = fpu_mul(frac, 5.0f);
            a = fpu_add(a, a);
            const float size = fpu_add(frac, 1.0f);
            if (1.0f < a) a = 1.0f;
            const uint32_t alpha = static_cast<uint32_t>(float_to_int(fpu_mul(a, 96.0f))) & 0xffu;
            const float x = int_to_float(lw(sp + 0x10)), y = int_to_float(lw(sp + 0x14));
            const float ang = angle_wrap_mod(fpu_div(int_to_float(lw(L17_ADDR(0x0015f5ccu))), 30.0f));
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
            render_view_hud_begin(1);  // (a missile: 0x2ebc68 loaded, 0x2ebca8 not)
            hud_line_strip(L17_ADDR(0x001d99e0u), 0x19, 0x00fffff3u, loaded ? 0x50008f00u : 0x20004f00u, static_cast<float>(x),
                           static_cast<float>(y), 0.5f);
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

    const float cx = static_cast<float>(lw(pv + 0xe0));
    const float cy = static_cast<float>(lw(pv + 0xe4));
    screen_sprite(0x3f, 0x3f, effect_tex0(0x11), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x3f, 0x3f, effect_tex0(0x12), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 40.0f, 40.0f, 0.0f, 0.5f, 0.5f);
    screen_sprite(0x1f, 0x1f, effect_tex0(0x08), 0x00fffff3u, 0xff20ff20u, 0, 0, cx, cy, 10.0f, 10.0f, 0.0f, 0.5f, 0.5f);

    if (lwc1(P + 0x15fc) < fpu_div(lwc1(P + 0x1600), 10.0f) && (ee_div(lw(L17_ADDR(0x0015f5ccu)), 0x5a) & 1) != 0)
        text_centered_med(0x100, 0x186, 0x80000080u, sub_00203ad8(static_cast<int32_t>(TXT(0x5269))), 0x64);
    if (lbu(m + 0x20) == 8) text_centered_med(0x100, 0xc8, 0x80005080u, sub_00203ad8(static_cast<int32_t>(TXT(0x523e))), 0x64);

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
        const uint32_t dst = lwu(L17_ADDR(0x00162180u)) + (idx << 2);
        sw(dst, k < bar ? lwu(L17_ADDR(0x001d9b68u) + (idx << 2)) : 0x80000000u);
    }
    sw(pv + 0xf4, static_cast<uint32_t>(bar));
}

}  // namespace rac1
