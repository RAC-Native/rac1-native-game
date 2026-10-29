// Game mode 6: the ship sequences (the landing on a planet, the take-off from it, the flight to
// another one): their start 0x28e9c8, their frame 0x290590 and drawing 0x291d00, the take-off
// from the ship 0x28ecd8 and the planet chosen on the ship's map 0x28ed58.

#include <game/enhance/advanced.h>
#include <game/ovl/arrival.h>
#include <game/ovl/flight_draw.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/level_change.h>
#include <game/lib/libvu0.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/game_skips.h>
#include <game/render/mrp_lod.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x0016c860u> SC{};  // the scene
constexpr uint32_t G = 0x0013e030;   // the ship sequence
constexpr uint32_t STREAMS = 0x001516d0;

// orig: (helper) repeated in 0x0028e9c8 / 0x0028ecd8 / 0x00290590: the dialog stream (the
// sequence's sound) stopped (5) unless it ends already (6, 7)
void dialog_slot_stop() {
    if (!(static_cast<uint32_t>(lhu(STREAMS + 0x5a) - 6u) < 2)) sh(STREAMS + 0x5a, 5);
}

// orig: (helper) repeated in 0x00290590: a ship's grey level `v` as its colour (+0x90), the red
// halved on the ship of class 0x215
uint32_t ship_grey(int32_t cls, int32_t v) {
    const uint32_t a = static_cast<uint32_t>(v);
    if (advanced_ship_look_class(cls) == 0x215) return (static_cast<uint32_t>(v >> 1) | (a << 8)) | (a << 16);  // (enhancement, #94: the shown ship's)
    return (a | (a << 8)) | (a << 16);
}

// orig: (helper) repeated in 0x00290590: the grey level pulsing with the frame (64 frames a
// period): 0x78 + 80 cos((frame & 0x3f) - 0x20) pi/32)
int32_t ship_pulse(int32_t frame, float f21, float f20) {
    float f0 = int_to_float((frame & 0x3f) - 0x20);
    f0 = cos_vu(f0 * f21);
    f0 = f0 * f20;
    return float_to_int(f0) + 0x78;
}

// orig: (helper) repeated in 0x0028e9c8 / 0x00290590: the mobys past *(0x15ffdc) of class 0x4a
// / 0xcb (not dying, +0x20 bit 7) given or cleared the flag 0x80 of +0x34
void markers_flag(bool set) {
    uint32_t m = lwu(OV(0x0015ffdc));
    while (lbu(m + 0x20) != 0xff) {
        if ((lbu(m + 0x20) & 0x80) == 0) {
            const int16_t cls = lh(m + 0xa6);
            if (cls == 0x4a || cls == 0xcb) sh(m + 0x34, set ? (lhu(m + 0x34) | 0x80u) : (lhu(m + 0x34) & 0xff7fu));
        }
        m += 0x100;
    }
}
}  // namespace

// A ship sequence started at step `step` (8 the landing, 0 the take-off): game mode 6, the scene
// cleared, the buffers 0x60000 below *(0x1611cc), the hero's state 0x64 (mode 2), the ship
// hidden, the scene read (take-off: the ship's variant + 1, landing + 5 (+ 3 when 0x13e05c);
// level 10 / 14 special cases) with a fade of 6 frames, its sound (dialog 40000 + variant + 6 /
// + 3 / ...) awaited and continued.
// orig: 0x0028e9c8
void ship_sequence_start(int32_t step) {
    sh(G + 0x24, 0xffff);
    sw(G + 8, 0);
    sw(OV(0x0015f5d8), 1);
    sw(G + 0x20, static_cast<uint32_t>(step));
    mem_fill32(SC, 0, 0x1c0);
    mem_fill32(OV(0x0017c340), 0, 0x40);
    mem_fill32(OV(0x0017c380), 0, 0x40);
    sub_002a07c8();
    const uint32_t base = lwu(OV(0x001611cc)) + 0xfffa0000u;
    sw(SC + 0x5c, lwu(OV(0x00173e08)) + base);
    sw(SC + 0x58, lwu(OV(0x00173e04)) + base);
    sw(OV(0x0015f5c4), 6);
    swc1(OV(0x0015f3fc), 1.0f);
    sw(OV(0x001611cc), base);
    sw(OV(0x0015f400), 0);
    hero_set_state(0x64, 2);
    sb(0x001413f5, 1);
    sub_00232fe8();
    const uint32_t ship = lwu(G);
    sh(ship + 0x34, lhu(ship + 0x34) | 1u);
    markers_flag(true);
    const int32_t level = lw(0x0015ed84);
    int32_t snd;
    if (step == 0) {
        const int32_t v = lh(G + 0x26);
        int32_t scene = v + 1;
        snd = v + 6;
        if (level == 0xa && v == 1 && lbu(0x0013d4c6) == 0) {
            scene = 0;
            snd = 0xb;
        } else if (level == 0xe && lh(G + 0x26) == 2) {
            scene = 8;
            snd = 0xe;
        }
        scene_read(scene, frame_scale_i(6));
    } else {
        const int32_t v = lh(G + 0x26);
        const int32_t opt = lh(G + 0x2c);
        snd = opt != 0 ? v : v + 3;
        int32_t scene = v + 5;
        if (level == 0xa && v == 1 && lbu(0x0013d4c6) == 0) {
            snd = opt != 0 ? 9 : 0xa;
            scene = 4;
        } else if (level == 0xe && lh(G + 0x26) == 2) {
            snd = lh(G + 0x2c) != 0 ? 0xc : 0xd;
            scene = 9;
        }
        scene_read(scene, lh(G + 0x2c) != 0 ? 0 : frame_scale_i(6));
    }
    sw(STREAMS + 0x1c, static_cast<uint32_t>(snd + 0x9c40));
    scene_block_load(0);
    while (lh(STREAMS + 0x5a) != 3) {
        sound_frame();
        TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    }
    dialog_stream_stop();
}

// A frame of game mode 6, by the step 0x13e050 (the frame count 0x13e054 + 1):
// 0 / 8: the scene's next frame: the camera from its frame record (position, angles, field of
//   view) relative to the ship, its mobys at their positions (half frames blended), shadows,
//   Clank, the hero with his backpack (levels 10 / 13), the ships drawn with their engines and
//   their colour by the frame; X / circle (0x13cae4 & 0x50) skips once 30 frames are past; at
//   the end the scene's mobys gone, then the take-off opens the planet map (0x276190(0)) and
//   the landing gives the hero back (game mode 0, the music on 0x265860, 0x216f90 / 0x234d40);
// 3: the flight along the path 0x1b04b0[*(0x13e060)] (set up the first frame: the sound, Ratchet
//   and Clank on the ship, the path's turn rates) until its end or the fade (then 0x15f570: the
//   next level); 4: the title screen's (not ported).
// orig: 0x00290590
void ship_sequence_update() {
    sh(G + 0x24, lhu(G + 0x24) + 1u);
    const uint32_t step = lwu(G + 0x20);
    if (!(step < 9)) return;
    if (step == 0 || step == 8) {
        draw_lists_reset();
        float f20 = 0.0f;
        moby_update_visible();
        sound_emitters_update();
        hero_frame_update();
        particles_update();
        const float fade0 = lwc1(OV(0x0015f3fc)) - 0.125f;
        sw(SC + 0x38, static_cast<uint32_t>(lw(SC + 0x38) + 1));
        sw(SC + 0x34, static_cast<uint32_t>(lw(SC + 0x34) + 1));
        swc1(OV(0x0015f3fc), fade0);
        if (fade0 < f20) swc1(OV(0x0015f3fc), f20);
        const int32_t s2 = lw(0x0015ed80) != 0 ? 0x50 : 0x60;
        int32_t s0 = lh(G + 0x28);
        if ((lwu(0x0013cae4) & 0x50) != 0 && frame_scale_i(0x1e) < lw(SC + 0x34) && lwc1(OV(0x0015f3fc)) == f20) {
            if (lw(G + 0x20) == 0) {
                const int32_t t = frame_scale_i(lw(OV(0x001605d0) + static_cast<uint32_t>(lh(G + 0x26)) * 4) - 0x1e);
                if (lw(SC + 0x34) < t) s0 = 1;
            }
            if (lw(G + 0x20) == 8) {
                const int32_t t = frame_scale_i(0x1e);
                if (lw(SC + 0x34) < lh(SC + 0x40) - t) s0 = 1;
            }
        }
        if (s0 != 0) {  // skipped (or 0x13e058 set): straight to the end
            dialog_slot_stop();
            if (lw(G + 0x20) == 0) {
                const int32_t v = frame_scale_i(lw(OV(0x001605d0) + static_cast<uint32_t>(lh(G + 0x26)) * 4));
                sw(SC + 0x34, static_cast<uint32_t>(v));
                const int32_t blk = ee_div(v, s2);
                sw(SC + 0x3c, static_cast<uint32_t>(blk));
                scene_block_load(blk);
                const int32_t a0 = lh(G + 0x28);
                sw(SC + 0x38, static_cast<uint32_t>(ee_rem(lw(SC + 0x34), s2)));
                if (a0 == 0) screen_fade(4);
                else sh(G + 0x28, 0);
                swc1(OV(0x0015f3fc), 1.0f);
            } else {
                sw(SC + 0x34, static_cast<uint32_t>(lh(SC + 0x40)));
            }
        }
        if (!(lw(SC + 0x34) < lh(SC + 0x40))) {  // the end of the scene
            dialog_slot_stop();
            sub_002a07c8();
            swc1(OV(0x0016caf0), u2f(0x3f2147aeu));
            projection_setup();
            for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
                const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
                if (m != 0) {
                    const uint32_t cd = lwu(m + 0x24);
                    sb(cd + 0xc, lbu(cd + 0xc) - 1u);
                    const uint32_t cd2 = lwu(m + 0x24);
                    sw(cd2 + static_cast<uint32_t>(lbu(cd2 + 0xc)) * 4 + 0x48, 0);
                    moby_retire(m);
                }
                const uint32_t pack = lwu(G + 8);
                if (pack != 0) moby_retire(pack);
            }
            const uint32_t ship = lwu(G);
            sh(ship + 0x34, lhu(ship + 0x34) & 0xfffeu);
            markers_flag(false);
            if (lw(G + 0x20) == 0) {  // the take-off: the planet map
                menu_open(0);
                sw(OV(0x001b9cf0), 0xe);
                sw(OV(0x0015f5d8), 1);
                return;
            }
            sw(OV(0x0015f5c4), 0);
            music_volumes_default();
            sw(OV(0x0015f5d8), 1);
            sb(0x001413f5, 0);
            sub_002330d0();
            if (lh(G + 0x2c) != 0) hero_ship_floor_fix();
            else hero_place(G + 0x60, G + 0x70, 0, 1);
            return;
        }
        // the camera: the frame record (position, angles, field of view +0x1c, flag +0xc)
        if (!(lw(SC + 0x38) < (lw(0x0015ed80) != 0 ? 0x50 : 0x60))) {
            const int32_t blk = lw(SC + 0x3c) + 1;
            sw(SC + 0x3c, static_cast<uint32_t>(blk));
            scene_block_load(blk);
        }
        const uint32_t fr = lwu(SC + 0x54) + static_cast<uint32_t>(lw(SC + 0x38)) * 0x20;
        const uint32_t flag = lbu(fr + 0xc);
        const uint32_t s1 = fr + 0x10;
        swc1(OV(0x0016caf0), lwc1(s1 + 0xc));
        projection_setup();
        constexpr OvConst<0x00166dc0u> CAM{};
        lq_sq(CAM, fr);
        mat3_transform(V(CAM), V(CAM), M(lwu(G) + 0xc0));
        vec3_add(V(CAM), V(CAM), V(lwu(G) + 0x10));
        f20 = angle_add(lwc1(s1 + 8), lwc1(lwu(G) + 0x48));
        Mat4 r;
        vu0_unit_matrix(r);
        vu0_rot_matrix_x(r, r, lwc1(fr + 0x10));
        vu0_rot_matrix_y(r, r, lwc1(s1 + 4));
        vu0_rot_matrix_z(r, r, f20);
        const uint32_t V0 = CAM - 0x140;
        const uint8_t cross = lbu(0x0015edb4);
        swc1(V0 + 0x378, r.r[1].z);
        swc1(V0 + 0x350, -r.r[2].x);
        swc1(V0 + 0x360, -r.r[0].x);
        swc1(V0 + 0x370, r.r[1].x);
        swc1(V0 + 0x354, -r.r[2].y);
        swc1(V0 + 0x364, -r.r[0].y);
        swc1(V0 + 0x374, r.r[1].y);
        swc1(V0 + 0x358, -r.r[2].z);
        swc1(V0 + 0x368, -r.r[0].z);
        if (cross != 0) vec3_cross_ba(V(CAM + 0x220), V(CAM + 0x230), V(CAM + 0x210));
        // the scene's mobys
        const float f21 = u2f(0x3dc90fdbu);
        f20 = 80.0f;
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            const int32_t half = lw(SC + 0x38) >> 1;
            sb(m + 0x50, static_cast<uint32_t>(half));
            sb(m + 0x51, static_cast<uint32_t>(half + 1));
            sub_0024ebc0(m);
            swc1(m + 0x54, int_to_float(lw(SC + 0x38) & 1) * 0.5f);
            if (flag != 0 && (lw(SC + 0x38) & 1) != 0) swc1(m + 0x54, 1.0f);
            const float t = lwc1(m + 0x54);
            Mat4 tmp;
            const uint32_t pos = lwu(m + 0x78);
            vec3_scale(tmp.r[0], V(pos + static_cast<uint32_t>(lbu(m + 0x50)) * 16), 1.0f - t);
            vec3_scale(tmp.r[1], V(pos + static_cast<uint32_t>(lbu(m + 0x51)) * 16), lwc1(m + 0x54));
            vec3_add(V(m + 0x10), tmp.r[0], tmp.r[1]);
            mat3_transform(V(m + 0x10), V(m + 0x10), M(lwu(G) + 0xc0));
            vec3_add(V(m + 0x10), V(m + 0x10), V(lwu(G) + 0x10));
            sb(m + 0x71, 0xff);
            swc1(m + 0x48, lwc1(lwu(G) + 0x48));
            moby_update_transform(m);
            const int32_t cls = lh(m + 0xa6);
            if (cls == 0) {  // the hero: his shadow
                if (lw(G + 0x20) == 0) {
                    if (lw(0x0015ed84) == 0xa && lbu(0x0013d4c6) == 0) sb(m + 0x7f, 0);
                    else sb(m + 0x7f, frame_scale_i(0x3e) < lw(SC + 0x34) ? 0u : 0x18u);
                } else if (!(frame_scale_i(0x168) < lw(SC + 0x34))) {
                    sb(m + 0x7f, 0);
                } else {
                    sb(m + 0x7f, lw(0x0015ed84) != 0xa || lbu(0x0013d4c6) != 0 ? 0x18u : 0u);
                }
            } else if (cls == 10) {  // Clank: his shadow
                if (lw(G + 0x20) == 0) sb(m + 0x7f, frame_scale_i(0x15e) < lw(SC + 0x34) ? 0u : 0x18u);
                else sb(m + 0x7f, frame_scale_i(0x20c) < lw(SC + 0x34) ? 0x18u : 0u);
            }
            if (lbu(m + 0x7f) != 0) moby_shadow_probe(m);
            if (lh(m + 0xa6) == 10) clank_ship_update(m);
            if (lh(m + 0xa6) == 0) {
                hero_ship_update(m);
                const int32_t lv = lw(0x0015ed84);
                if ((lv == 0xa && lbu(0x0013d4c6) != 0) || lv == 0xd) {
                    if (lw(G + 8) == 0) {  // the backpack
                        const uint32_t b = moby_spawn(0x509);
                        sw(G + 8, b);
                        sh(b + 0x32, 0x40);
                        const uint32_t b2 = lwu(G + 8);
                        sh(b2 + 0x34, lhu(b2 + 0x34) | 0x806u);
                        sd(lwu(G + 8) + 0x38, ld(m + 0x38));
                        const uint32_t b3 = lwu(G + 8);
                        if (lbu(lwu(b3 + 0x24) + 6) != 0) sb(b3 + 0x73, 0x18);
                    }
                    hero_backpack_attach(m, lwu(G + 8));
                }
            }
            if (!(static_cast<uint32_t>(lhu(m + 0xa6)) - 0x213u < 3)) continue;
            // a ship: drawn with its engines, its colour by the frame
            draw_callback_add(OVF(0x002935b8), m);
            sub_001f93e0(OVF(0x0028e670), m);
            const int32_t cls_s = lh(m + 0xa6);
            if (lw(G + 0x20) == 0) {  // the take-off
                const int32_t a0 = lw(SC + 0x34);
                if (a0 < frame_scale_i(0x168)) {
                    sw(m + 0x90, ship_grey(cls_s, ship_pulse(a0, f21, f20)));
                    continue;
                }
                if (lw(SC + 0x34) < frame_scale_i(0x1f8)) {
                    const float b0 = frame_scale_f(360.0f);
                    float f1 = int_to_float(lw(SC + 0x34));
                    const float f2 = lwc1(0x0015ed60) * 1.25f;
                    f1 = (f1 - b0) * f2;
                    int32_t a1 = float_to_int(f1);
                    if (!(a1 < 0x100)) a1 = 0xff;
                    const uint32_t u = static_cast<uint32_t>(a1);
                    sw(m + 0x90, cls_s == 0x215 ? ((u << 8) | (u << 16)) : ((u | (u << 8)) | (u << 16)));
                    if (frame_scale_i(0x1a4) < lw(SC + 0x34)) {
                        const float c0 = frame_scale_f(420.0f);
                        float g1 = int_to_float(lw(SC + 0x34));
                        const float g2 = lwc1(0x0015ed60) * u2f(0x3f99999au);
                        g1 = (g1 - c0) * g2;
                        sb(lwu(G) + 0xbc, static_cast<uint32_t>(float_to_int(g1)));
                        sh(lwu(G) + 0xb2, 0);
                        sub_001f93e0(OVF(0x0028efc8), lwu(G));
                    }
                    if (frame_scale_i(0x1d0) < lw(SC + 0x34)) {
                        const float f0 = lwc1(OV(0x0015f400)) + u2f(0x3ccccccdu);
                        swc1(OV(0x0015f400), f0);
                        if (1.0f < f0) swc1(OV(0x0015f400), 1.0f);
                    }
                } else {
                    const float f0 = lwc1(OV(0x0015f400)) - u2f(0x3ccccccdu);
                    sw(m + 0x90, 0x00a0a0a0u);
                    swc1(OV(0x0015f400), f0);
                    if (f0 < 0.0f) swc1(OV(0x0015f400), 0.0f);
                }
                continue;
            }
            // the landing
            if (lw(SC + 0x34) < frame_scale_i(0xf0)) {
                if (lw(SC + 0x34) < frame_scale_i(0xc8)) {
                    ship_thrusters(m, lwc1(0x0015ed6c) * -4.0f);
                    ship_thrusters(m, lwc1(0x0015ed6c) * -3.0f);
                }
                sw(m + 0x90, ship_grey(lh(m + 0xa6), ship_pulse(lw(SC + 0x34), f21, f20)));
                sb(m + 0xbc, 0x32);
                sh(m + 0xb2, 0xa);
                sub_001f93e0(OVF(0x0028efc8), m);
                continue;
            }
            const int32_t a0 = lw(SC + 0x34);
            if (frame_scale_i(0x168) < a0) {
                sw(m + 0x90, ship_grey(lh(m + 0xa6), ship_pulse(a0, f21, f20)));
                continue;
            }
            if (lw(SC + 0x34) < frame_scale_i(0x12c)) {
                float f0 = frame_scale_f(300.0f);
                const float f3 = int_to_float(lw(SC + 0x34));
                const float f1 = lwc1(0x0015ed60) * 1.5f;
                f0 = (f0 - f3) * f1;
                sb(lwu(G) + 0xbc, static_cast<uint32_t>(float_to_int(f0)));
                sh(lwu(G) + 0xb2, 0);
                sub_001f93e0(OVF(0x0028efc8), lwu(G));
            }
            float f0 = frame_scale_f(360.0f);
            const float f3 = int_to_float(lw(SC + 0x34));
            const float f1 = lwc1(0x0015ed60) * 1.25f;
            f0 = (f0 - f3) * f1;
            const int32_t g = float_to_int(f0);
            const uint32_t u = static_cast<uint32_t>(g);
            sw(m + 0x90, lh(m + 0xa6) == 0x215 ? ((u << 8) | (u << 16)) : ((u | (u << 8)) | (u << 16)));
        }
    } else if (step == 3) {
        draw_lists_reset();
        moby_update_visible();
        sound_emitters_update();
        hero_frame_update();
        particles_update();
        if (lh(G + 0x24) == 0) {  // the first frame: the sound, the riders, the path
            sub_00265828(0);
            stream_reads_wait(0);
            sw(STREAMS + 0x1c, static_cast<uint32_t>(lh(G + 0x26) + 0x9c4f));
            screen_fade(frame_scale_i(0xc));
            while (lh(STREAMS + 0x5a) != 3) {
                sound_frame();
                TODO_PORT(0x00122298, "kernel: vsync wait")(0);
            }
            dialog_stream_stop();
            const uint32_t ship = lwu(G);
            sb(ship + 0x52, 1);
            sb(lwu(G) + 0x53, 2);
            sw(lwu(G) + 0x54, 0);
            sub_0024ebc0(lwu(G));
            sb(lwu(G) + 0x72, 0xff);
            sw(lwu(G) + 0x94, 0);
            if (lw(G + 0x30) >= 0) {
                for (uint32_t k = 0; k < 2; ++k) {
                    const uint32_t m = moby_spawn(k == 0 ? 0u : 10u);
                    const uint32_t slot = G + 0x10 + k * 4;
                    sw(slot, m);
                    sh(m + 0x32, 0x1ff);
                    sb(lwu(slot) + 0x72, 0xff);
                    sw(lwu(slot) + 0x94, 0);
                    sh(lwu(slot) + 0x34, lhu(lwu(slot) + 0x34) | 6u);
                    sd(lwu(slot) + 0x38, ld(lwu(0x0013f350 + 0x2080) + 0x38));
                }
            }
            camera_scripted_set(OV(0x00166dc0), OV(0x00166dd0), 1, 0, 0);
            const int32_t pi = lw(G + 0x30);
            sw(G + 0x48, 0);
            sw(G + 0x4c, 0);
            if (pi >= 0) {
                const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(pi) * 4);
                float f21 = lwc1(G + 0x48);
                swc1(path + 0x1c, f21);
                for (int32_t k = 1; k < lw(path) - 1;) {  // the turn at each point
                    const uint32_t p0 = path + static_cast<uint32_t>(k) * 0x10;
                    const uint32_t pm = path + static_cast<uint32_t>(k - 1) * 0x10;
                    const float a = atan2_f(lwc1(p0 + 0x10) - lwc1(pm + 0x10), lwc1(p0 + 0x14) - lwc1(pm + 0x14));
                    ++k;
                    const uint32_t pn = path + static_cast<uint32_t>(k) * 0x10;
                    const float b = atan2_f(lwc1(pn + 0x10) - lwc1(p0 + 0x10), lwc1(pn + 0x14) - lwc1(p0 + 0x14));
                    const float d = angle_sub(b, a);
                    swc1(p0 + 0x1c, d);
                    if (f21 < abs_f(d)) f21 = abs_f(lwc1(p0 + 0x1c));
                }
                {  // the last point: the turn back to the first
                    const int32_t n = lw(path);
                    const uint32_t pa = path + static_cast<uint32_t>(n - 2) * 0x10, pb = path + static_cast<uint32_t>(n - 1) * 0x10;
                    const float a = atan2_f(lwc1(pb + 0x10) - lwc1(pa + 0x10), lwc1(pb + 0x14) - lwc1(pa + 0x14));
                    const uint32_t pl = path + static_cast<uint32_t>(lw(path) - 1) * 0x10;
                    const float b = atan2_f(lwc1(path + 0x10) - lwc1(pl + 0x10), lwc1(path + 0x14) - lwc1(pl + 0x14));
                    const float d = angle_sub(b, a);
                    swc1(path + static_cast<uint32_t>(lw(path) - 1) * 0x10 + 0x1c, d);
                }
                const float scale = fpu_div(u2f(0x3eb2b8c2u), f21);
                for (int32_t k = 0; k < lw(path); ++k) {
                    const uint32_t p = path + static_cast<uint32_t>(k) * 0x10;
                    swc1(p + 0x1c, lwc1(p + 0x1c) * scale);
                }
                swc1(G + 0x44, vec3_distance(V(path + 0x10), V(path + 0x20)));
                const uint32_t ship2 = lwu(G);
                sw(G + 0x3c, 0);
                sw(G + 0x40, 0);
                sw(G + 0x50, 0);
                sw(G + 0x54, 0);
                lq_sq(G + 0x80, ship2 + 0x10);
                lq_sq(G + 0x90, path + 0x10);
                lq_sq(G + 0xa0, ship2 + 0x40);
                const float h = vec2_distance(V(path + 0x20), V(path + 0x10));
                swc1(G + 0xb4, atan2_f(h, lwc1(path + 0x18) - lwc1(path + 0x28)));
                swc1(G + 0xb8, atan2_f(lwc1(path + 0x20) - lwc1(path + 0x10), lwc1(path + 0x24) - lwc1(path + 0x14)));
            }
        }
        if (lw(G + 0x34) >= 0 && lw(G + 0x38) >= 0) {  // the camera between two level cameras
            const float f13 = lwc1(0x0015ed70) * u2f(0x3f2a7efau);
            seek_1d(G + 0x48, G + 0x4c, 1.0f, f13, f13, lwc1(0x0015ed6c) * 0.5f);
            const uint32_t sp = ee_stack_alloc(0x20);
            const uint32_t cams = lwu(OV(0x001600ec));
            vec3_lerp(V(sp), V(cams + static_cast<uint32_t>(lw(G + 0x34)) * 0x80 + 0x30), V(cams + static_cast<uint32_t>(lw(G + 0x38)) * 0x80 + 0x30), lwc1(G + 0x48));
            vec_clear(V(sp + 0x10));
            auto cam = [](uint32_t k) { return lwu(OV(0x001600ec)) + static_cast<uint32_t>(lw(G + k)) * 0x80; };
            float d = angle_sub(lwc1(cam(0x38) + 0x74), lwc1(cam(0x34) + 0x74));
            float f = d * lwc1(G + 0x48);
            swc1(sp + 0x14, f);
            swc1(sp + 0x14, angle_add(lwc1(cam(0x34) + 0x74), f));
            d = angle_sub(lwc1(cam(0x38) + 0x78), lwc1(cam(0x34) + 0x78));
            f = d * lwc1(G + 0x48);
            swc1(sp + 0x18, f);
            swc1(sp + 0x18, angle_add(lwc1(cam(0x34) + 0x78), f));
            uid5_set_pvar_80(sp);
            uid5_set_pvar_90(sp + 0x10);
            ee_stack_free(0x20);
        }
        if (lw(G + 0x30) >= 0) {
            const uint32_t ship = lwu(G);
            if (lh(G + 0x24) < frame_scale_i(0x96)) {  // the take-off from the planet
                const float f20a = int_to_float(lh(G + 0x24));
                const float f0 = int_to_float(frame_scale_i(0x78));
                const float c = cos_vu(f20a * fpu_div(u2f(0x40490fdbu), f0));
                float f20 = (1.0f - c) * 0.5f;
                if (!(lh(G + 0x24) < frame_scale_i(0x78))) f20 = 1.0f;
                swc1(ship + 0x54, f20);
                vec3_lerp(V(ship + 0x10), V(G + 0x80), V(G + 0x90), f20);
                sw(ship + 0x40, 0);
                float dd = angle_sub(lwc1(G + 0xb4), lwc1(G + 0xa4));
                swc1(lwu(G) + 0x44, angle_add(lwc1(G + 0xa4), dd * f20));
                dd = angle_sub(lwc1(G + 0xb8), lwc1(G + 0xa8));
                const float rz = angle_add(lwc1(G + 0xa8), dd * f20);
                const float fade = lwc1(OV(0x0015f3fc));
                swc1(lwu(G) + 0x48, rz);
                if (0.0f < fade) {
                    const float f0b = fade - 0.125f;
                    swc1(OV(0x0015f3fc), f0b);
                    if (f0b < 0.0f) swc1(OV(0x0015f3fc), 0.0f);
                }
                if (f20 < 1.0f) ship_thrusters(lwu(G), lwc1(0x0015ed6c) * -3.75f);
            } else {  // along the path
                const uint32_t path = lwu(OV(0x001b04b0) + static_cast<uint32_t>(lw(G + 0x30)) * 4);
                const float sp0 = lwc1(G + 0x40) + u2f(0x3f4ccccdu);
                swc1(G + 0x40, sp0);
                if (100.0f < sp0) swc1(G + 0x40, 100.0f);
                if ((lwu(0x0013cae4) & 0x50) != 0 && lwc1(OV(0x0015f3fc)) < 0.0625f) swc1(OV(0x0015f3fc), 0.0625f);
                if (int_to_float(lw(path) - 6) < lwc1(G + 0x3c) && lwc1(OV(0x0015f3fc)) < 0.0625f) swc1(OV(0x0015f3fc), 0.0625f);
                if (advanced_ship_look(lh(G + 0x26)) < 2) {  // (enhancement, #94: the shown ship's flames)
                    sb(lwu(G) + 0xbc, 0x32);
                    sh(lwu(G) + 0xb2, 0xa);
                    sub_001f93e0(OVF(0x0028efc8), lwu(G));
                }
                const float t = lwc1(G + 0x3c) + fpu_div(lwc1(G + 0x40) * lwc1(0x0015ed6c), lwc1(G + 0x44));
                swc1(G + 0x3c, t);
                if (int_to_float(lw(path) - 1) < t || 1.0f <= lwc1(OV(0x0015f3fc))) {  // the end: the next level
                    sw(OV(0x0015f5d8), 1);
                    sw(OV(0x0015f570), 1);
                } else {
                    const uint32_t sh_ = lwu(G);
                    path_point(t, path, 1, sh_ + 0x10, sh_ + 0x40, 0);
                    swc1(lwu(G) + 0x40, lwc1(lwu(G) + 0x4c));
                    sw(G + 0x50, static_cast<uint32_t>(lw(G + 0x50) + 1) & 0x1fu);
                    if (lw(G + 0x54) < 0x20) sw(G + 0x54, static_cast<uint32_t>(lw(G + 0x54) + 1));
                    for (uint32_t k = 0; k < 2; ++k) {  // the trails: two points of the ship's
                        const uint32_t o = G + 0xc0 + k * 0x200 + static_cast<uint32_t>(lw(G + 0x50)) * 16;
                        mat3_transform(V(o), V(OV(0x001bdab0) + k * 0x10 + static_cast<uint32_t>(advanced_ship_look(lh(G + 0x26))) * 32), M(lwu(G) + 0xc0));
                        const uint32_t o2 = G + 0xc0 + k * 0x200 + static_cast<uint32_t>(lw(G + 0x50)) * 16;
                        vec3_add(V(o2), V(o2), V(lwu(G) + 0x10));
                        swc1(G + k * 0x200 + static_cast<uint32_t>(lw(G + 0x50)) * 16 + 0xcc, 1.0f);
                    }
                    sub_001f93e0(OVF(0x0028f238), lwu(G));
                    const float fade = lwc1(OV(0x0015f3fc));
                    if (0.0f < fade) {
                        const float f0 = fade + 0.0625f;
                        swc1(OV(0x0015f3fc), f0);
                        if (1.0f < f0) swc1(OV(0x0015f3fc), 1.0f);
                    }
                }
            }
            for (uint32_t k = 0; k < 2; ++k) {  // Ratchet and Clank on the ship
                const uint32_t m = lwu(G + 0x10 + k * 4);
                lq_sq(m + 0x40, lwu(G) + 0x40);
                mat3_transform(V(m + 0x10), V((k == 0 ? OV(0x001bdd80u) : OV(0x001bddb0u)) + static_cast<uint32_t>(advanced_ship_look(lh(G + 0x26))) * 16), M(lwu(G) + 0xc0));
                const uint32_t m2 = lwu(G + 0x10 + k * 4);
                vec3_add(V(m2 + 0x10), V(m2 + 0x10), V(lwu(G) + 0x10));
                moby_update_transform(lwu(G + 0x10 + k * 4));
            }
        }
        camera_update();
    } else if (step == 4) {
        flight_sequence_update();
        return;
    } else {
        return;
    }
    sound_frame();
    level_lights_update();
    shadow_direction_update();
    frame_counters_advance();
}

// The drawing of game mode 6: nothing on a frame cut short (0x15f5d8); steps 0 / 8 as game
// mode 2 (0x1f8e90), 3 / 7 the frame (0x15f3f4 = 0x7f), 4 the ship's flight 0x2900a0 (the loading screen; flight_draw.cpp).
// orig: 0x00291d00
void ship_sequence_draw() {
    if (lw(OV(0x0015f5d8)) != 0) return;
    const uint32_t step = lwu(G + 0x20);
    if (!(step < 9)) return;
    switch (step) {
        case 0:
        case 8: mode2_draw(); break;
        case 3:
        case 7:
            sw(OV(0x0015f3f4), 0x7f);
            if (!g_draw_last_step_only || g_mrp_lod_frame_drawn) render_frame();
            break;
        case 4: flight_draw(); break;
        default: break;
    }
}

// The drawing of game mode 2 (and of the ship's landing / take-off): unless the frame was cut
// short, 0x200fd8, then the frame with 0x15f3f4 = 0x7f.
// orig: 0x001f8e90
void mode2_draw() {
    if (lw(OV(0x0015f5d8)) != 0) return;
    sub_00200fd8();
    sw(OV(0x0015f3f4), 0x7f);
    if (!g_draw_last_step_only || g_mrp_lod_frame_drawn) render_frame();
}

// The planet `planet` chosen on the ship's map: game mode 6; the same planet: the landing again,
// from its frame 0x160570[variant] on; another one: its load asked for (0x24d430) and the flight
// there (step 3).
// orig: 0x0028ed58
void planet_travel(int32_t planet) {
    sw(OV(0x0015f5d8), 1);
    sw(OV(0x0015f5c4), 6);
    swc1(OV(0x0015f3fc), 1.0f);
    sw(OV(0x0015f400), 0);
    sw(G + 0x20, 0);
    sh(G + 0x24, 0xffff);
    sub_002a07c8();
    const uint32_t base = lwu(OV(0x001611cc)) + 0xfffa0000u;
    sw(SC + 0x5c, lwu(OV(0x00173e08)) + base);
    sw(SC + 0x58, lwu(OV(0x00173e04)) + base);
    sw(OV(0x001611cc), base);
    if (lw(0x0015ed84) == planet) {
        ship_sequence_start(8);
        const int32_t s0 = lw(0x0015ed80) != 0 ? 0x50 : 0x60;
        const int32_t v = frame_scale_i(lw(OV(0x00160570) + static_cast<uint32_t>(lh(G + 0x26)) * 4));
        sw(SC + 0x34, static_cast<uint32_t>(v));
        const int32_t blk = ee_div(v, s0);
        sw(SC + 0x3c, static_cast<uint32_t>(blk));
        scene_block_load(blk);
        sw(SC + 0x38, static_cast<uint32_t>(ee_rem(lw(SC + 0x34), s0)));
        return;
    }
    level_change_request(0, planet);
    sh(G + 0x24, 0xffff);
    sw(OV(0x0015f5c0), static_cast<uint32_t>(planet));
    sw(G + 0x20, 3);
}

// The hero boarded the ship: the frame cut short, 0x1415f8 at least 1, the music paused, the
// dialog stopped, a sound frame, 0x2039a0, the take-off (step 0); 0x13e05c = 0.
// orig: 0x0028ecd8
void ship_takeoff_start() {
    constexpr uint32_t P = 0x0013f350;
    if (!(0 < lw(P + 0x22a8))) sw(P + 0x22a8, 1);
    sw(OV(0x0015f5d8), 1);
    sub_00265828(0);
    dialog_slot_stop();
    sound_frame();
    sub_002039a0();
    ship_sequence_start(0);
    sh(G + 0x2c, 0);
}

}  // namespace rac1
