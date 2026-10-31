// Game mode 2: the level's cutscenes. Started by a talk script (talk.cpp) with the scene's number:
// the scene's first blocks read from the disc (the level header's table 0x137b80 + 0x2afc, PAL
// + 0x2c18: 0x250 bytes per scene, a sector per block), its sound (the dialog stream, line =
// scene) awaited then continued; each frame the camera and the scene's mobys follow the block's
// records, the next block read every 0x60 frames (0x50 PAL); at its end (or skipped with Start)
// the hero is given back and the music's pause lifted after 30 frames (0x265880).

#include <game/ovl/cutscene.h>
#include <game/ovl/ovl_reloc.h>
#include <game/lib/libvu0.h>
#include <game/ovl/arrival.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/shared_fx_tr.h>
#include <game/ovl/umbris_call.h>
#include <game/ovl/oltanis_mobys.h>
#include <game/ovl/particle_16_spawn.h>
#include <game/ovl/variants_l14.h>
#include <game/ovl/quartu_call.h>
#include <game/enhance/advanced.h>
#include <game/port_stubs.h>

namespace rac1 {

namespace {
constexpr OvConst<0x0016c860u> SC{};   // the scene
constexpr uint32_t S = 0x001516d0;    // the streams
constexpr uint32_t P = 0x0013f350;    // the hero
constexpr OvConst<0x0016c058u> C{};    // the frame control (+8 the update bits)
constexpr OvConst<0x00179100u> TK{};   // the talk state
constexpr uint32_t PAD = 0x0013c940;
constexpr OvConst<0x00166dc0u> CAM{};

// orig: (helper) repeated in 0x00298840 / 0x00298b18: the mobys past *(0x15ffdc) of class 0x4a /
// 0xcb (but those flagged 0x80 in +0x20) hidden (+0x34 | 0x80) or shown again
void markers_hide(bool hide) {
    uint32_t m = lwu(OV(0x0015ffdc));
    while (lbu(m + 0x20) != 0xff) {
        if ((lbu(m + 0x20) & 0x80) == 0) {
            const int16_t cls = lh(m + 0xa6);
            if (cls == 0x4a || cls == 0xcb) sh(m + 0x34, hide ? (lhu(m + 0x34) | 0x80u) : (lhu(m + 0x34) & 0xff7fu));
        }
        m += 0x100;
    }
}

// orig: (helper) repeated in 0x00298f90: an update bit of the frame control (C+8), or with bit 0x10
// (single step) the pad's pressed buttons `mask`
bool update_on(uint32_t bit, uint32_t mask) {
    const uint32_t f = lwu(C + 8);
    if ((f & bit) != 0) return true;
    return (f & 0x10) != 0 && (lwu(PAD + 0x1a4) & mask) != 0;
}
}  // namespace

// Scene `scene` started: game mode 2 (unless the hero is gone: 0x211250), the music paused, the
// scene state cleared, its buffers 0x3c000 below *(0x1611cc), the hero in state 0x64 (mode 2),
// the talking moby and the menu's moby hidden, block 0 read (a fade of 6 frames) and decoded,
// the markers hidden, its sound (dialog line `scene`) awaited and continued, block 1 asked for.
// orig: 0x00298840
void cutscene_start(int32_t scene) {
    if (scene < 0) return;
    if (lw(P + 0x22a8) == 0) {
        hero_lost();
        return;
    }
    sub_00265828(0);
    stream_reads_wait(0);
    sw(OV(0x0015f5d8), 1);
    mem_fill32(SC, 0, 0x1c0);
    mem_fill32(OV(0x0017c440), 0, 0x40);
    mem_fill32(OV(0x0017c340), 0, 0x40);
    mem_fill32(OV(0x0017c380), 0, 0x40);
    mem_fill32(OV(0x0017c400), 0, 0x40);
    mem_fill32(OV(0x0017c3c0), 0, 0x40);
    const uint32_t base = lwu(OV(0x001611cc)) + 0xfffc4000u;
    sw(SC + 0x30, static_cast<uint32_t>(scene));
    sw(SC + 0x5c, lwu(OV(0x00173e08)) + base);
    sw(SC + 0x58, lwu(OV(0x00173e04)) + base);
    swc1(OV(0x0015f59c), 1.0f);
    sw(OV(0x0015f5a0), 0);
    sw(SC + 0x34, 0);
    sw(SC + 0x3c, 0);
    sw(OV(0x001611cc), base);
    sub_002039a0();
    swc1(OV(0x0015f3fc), 1.0f);
    sw(OV(0x0015f5c4), 2);
    sw(OV(0x0015f400), 0);
    hero_set_state(0x64, 2);
    sb(P + 0x20a5, 1);
    sub_00232fe8();
    if (const uint32_t m = lwu(TK + 8); m != 0) sh(m + 0x34, lhu(m + 0x34) | 1u);
    if (const uint32_t m = lwu(OV(0x001b9ddc)); m != 0) sh(m + 0x34, lhu(m + 0x34) | 1u);
    cutscene_block_read(0);
    screen_fade(frame_scale_i(6));
    stream_reads_wait(1);
    scene_block_decode();
    markers_hide(true);
    sw(S + 0x1c, lwu(SC + 0x30));
    while (lh(S + 0x5a) != 3) {
        sound_frame();
        TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    }
    sh(SC + 0x48, 0xfffdu);
    dialog_stream_stop();
    cutscene_block_read(1);
    if (lh(SC + 0x48) < 0) {
        int32_t s1 = 0;
        do {
            --s1;
            TODO_PORT(0x00122298, "kernel: vsync wait")(0);
        } while (lh(SC + 0x48) < s1);
    }
}

// Block `block` of the scene *(0x16c890) read from the disc into the read buffer *(0x16c8bc) (its
// sectors: the scene's row of the level header's table, this entry to the next), not awaited.
// orig: 0x002451b8
int32_t cutscene_block_read(int32_t block) {
    const uint32_t buf = lwu(SC + 0x5c);
    const uint32_t tab = 0x00137b80u + (lw(0x0015ed80) != 0 ? 0x2c18u : 0x2afcu);
    const uint32_t row = tab + lwu(SC + 0x30) * 0x250u;
    const uint32_t lo = lwu(row + static_cast<uint32_t>(block) * 4);
    const uint32_t hi = lwu(row + static_cast<uint32_t>(block + 1) * 4);
    const int32_t n = static_cast<int32_t>(hi - lo);
    if (0 < n) {
        stream_disc_read(buf, lo, static_cast<uint32_t>(n));
        stream_reads_wait(0);
    }
    return 1;
}

// The camera of the scene's frame (its record in the block +0x54: position, flag +0xc, angles
// x / y / z +0x10.., field of view +0x1c, scaled by 0x15f59c when 0x15edb0), the view's rows,
// pulled back along the view by *(0x15f5a0) when 0x15edb0; the record's flag (odd frames not
// blended) returned.
// orig: 0x00298de8
int32_t cutscene_camera() {
    const uint32_t fr = lwu(SC + 0x54) + static_cast<uint32_t>(lw(SC + 0x38)) * 0x20;
    const uint32_t s1 = fr + 0x10;
    const int32_t flag = static_cast<int32_t>(lbu(fr + 0xc));
    const float fov = lwc1(s1 + 0xc);
    swc1(OV(0x0016caf0), fov);
    if (lbu(0x0015edb0) != 0) swc1(OV(0x0016caf0), fov * lwc1(OV(0x0015f59c)));
    projection_setup();
    lq_sq(CAM, fr);
    Mat4 r;
    vu0_unit_matrix(r);
    vu0_rot_matrix_x(r, r, lwc1(fr + 0x10));
    vu0_rot_matrix_y(r, r, lwc1(s1 + 4));
    vu0_rot_matrix_z(r, r, lwc1(s1 + 8));
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
    const float back = lwc1(OV(0x0015f5a0));
    if (lbu(0x0015edb0) == 0) return flag;
    if (back == 0.0f) return flag;
    Vec4 d = V(CAM + 0x210);
    vec3_set_length(d, d, back);
    vec3_sub(V(CAM), V(CAM), d);
    return flag;
}

// One frame of game mode 2: the update passes its bits of C+8 allow (the draw lists, the level's
// scene extras, the mobys and the sound emitters, the hero, the particles), the fade, the dialog
// continued at its frame (+0x48), the end (the last frame, or Start once 18 frames are past and
// the fade is done: on Veldin, or with 0x15eea0 / 0x15ee20 / 0x15eed8 set, Start alone; elsewhere
// L1 + L2 + R1 + R2 held and Start; the PC's Skip Cutscenes = Always: Start alone everywhere, advanced.h), the
// next block every 0x60 frames (0x50 PAL), the camera,
// the scene's mobys placed between their two positions of the frame pair (the hero's and
// Clank's own updates), the sound, the lights, the shadows, the frame counters.
// orig: 0x00298f90
void cutscene_update() {
    if (update_on(2, 0x200)) draw_lists_reset();
    if ((lwu(C + 8) & 0x10) == 0 || (lwu(PAD + 0x1a4) & 0x220) != 0) cutscene_level_extras();
    if (update_on(2, 0x200)) {
        moby_update_visible();
        sound_emitters_update();
    }
    prof_mark(OV(0x00160ff8), 3);
    if (update_on(1, 0x200)) hero_frame_update();
    prof_mark(OV(0x00161008), 7);
    if (update_on(4, 0x200)) particles_update();
    const float f20 = 0.0f;
    prof_mark(OV(0x00161018), 5);
    const float fade = lwc1(OV(0x0015f3fc)) - u2f(0x3eae147bu);
    const int32_t frame = lw(SC + 0x34) + 1;
    sw(SC + 0x34, static_cast<uint32_t>(frame));
    sw(SC + 0x38, static_cast<uint32_t>(lw(SC + 0x38) + 1));
    swc1(OV(0x0015f3fc), fade);
    if (fade < f20) swc1(OV(0x0015f3fc), f20);
    if (!(frame < lh(SC + 0x48))) dialog_stream_stop();
    const int32_t frames = lh(SC + 0x40);
    int32_t end = !(lw(SC + 0x34) < frames) ? 1 : 0;
    const int32_t t18 = frame_scale_i(0x12);
    if (!(lw(SC + 0x34) < t18) && lwc1(OV(0x0015f3fc)) == f20) {
        bool start_alone = lw(0x0015eea0) != 0 || lw(0x0015ee20) != 0 || lw(0x0015eed8) != 0 || !(0 < lw(0x0015ed84)) ||
                           advanced_skip_cutscenes();  // (the PC's Always: the condition passes, the skip is the game's)
        if (start_alone && (lwu(PAD + 0x1a4) & 0x800) != 0) {
            end = 1;
        } else {
            constexpr uint64_t keys = (uint64_t{0x8000} << 28) | 0xf;
            if (((ld(PAD + 0x1a0) & keys) ^ keys) == 0) end = 1;
        }
    }
    if (end != 0) {
        cutscene_end();
    } else {
        if (!(lw(SC + 0x38) < (lw(0x0015ed80) != 0 ? 0x50 : 0x60))) {
            stream_reads_wait(1);
            sw(SC + 0x3c, static_cast<uint32_t>(lw(SC + 0x3c) + 1));
            scene_block_decode();
            cutscene_block_read(lw(SC + 0x3c) + 1);
        }
        const int32_t flag = cutscene_camera();
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            const int32_t half = lw(SC + 0x38) >> 1;
            sb(m + 0x51, static_cast<uint32_t>(half + 1));
            sb(m + 0x50, static_cast<uint32_t>(half));
            sub_0024ebc0(m);
            swc1(m + 0x54, int_to_float(lw(SC + 0x38) & 1) * 0.5f);
            if (flag != 0 && (lw(SC + 0x38) & 1) != 0) swc1(m + 0x54, 1.0f);
            Mat4 tmp;
            const uint32_t pos = lwu(m + 0x78);
            vec3_scale(tmp.r[0], V(pos + static_cast<uint32_t>(lbu(m + 0x50)) * 16), 1.0f - lwc1(m + 0x54));
            vec3_scale(tmp.r[1], V(pos + static_cast<uint32_t>(lbu(m + 0x51)) * 16), lwc1(m + 0x54));
            vec3_add(V(m + 0x10), tmp.r[0], tmp.r[1]);
            sb(m + 0x71, 0xff);
            moby_update_transform(m);
            if (lbu(m + 0x7f) != 0) moby_shadow_probe(m);
            if (lh(m + 0xa6) == 0) hero_ship_update(m);
            const int32_t cls = lh(m + 0xa6);
            if (cls == 10 || cls == 0x1a3 || cls == 0x555) clank_ship_update(m);
        }
    }
    prof_mark(OV(0x00161028), 8);
    sound_frame();
    prof_mark(OV(0x00161038), 6);
    level_lights_update();
    shadow_direction_update();
    if (update_on(2, 0x200)) frame_counters_advance();
}

// The end of a cutscene: its sound stopped (unless it ends already), a fade of 12 frames, game
// mode 0, the default field of view, the scene's mobys removed, the markers shown, the hero's
// state 0 (mode 1) and his height on the floor below (within 4.5), the scene's hero place
// (+0x46: +0x10 / +0x20, its height on the floor within 2) taken, the talking moby and the menu's
// moby shown, the talk script's next step, and the music's pause lifted after 30 frames.
// orig: 0x00298b18
void cutscene_end() {
    const uint32_t st = lhu(S + 0x5a);
    if (!(st - 6u < 2u)) sh(S + 0x5a, 5);
    sw(OV(0x0015f5d8), 1);
    sub_002a07c8();
    screen_fade(0xc);
    sw(OV(0x0015f5c4), 0);
    swc1(OV(0x0016caf0), u2f(0x3f2147aeu));
    sw(OV(0x0015f3fc), 0);
    projection_setup();
    for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
        const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
        if (m == 0) continue;
        const uint32_t cd = lwu(m + 0x24);
        sb(cd + 0xc, lbu(cd + 0xc) - 1u);
        const uint32_t cd2 = lwu(m + 0x24);
        sw(cd2 + static_cast<uint32_t>(lbu(cd2 + 0xc)) * 4 + 0x48, 0);
        moby_retire(m);
    }
    markers_hide(false);
    hero_set_state(0, 1);
    float f20 = floor_probe(0.5f, P + 0x80, 0);
    if (2.0f < f20) {
        if (abs_f(lwc1(P + 0x88) - f20) < 4.5f) swc1(P + 0x88, f20);
    }
    sb(P + 0x20a5, 0);
    sub_002330d0();
    if (lh(SC + 0x46) != 0) {
        f20 = floor_probe(0.5f, SC + 0x10, 0);
        if (2.0f < f20) {
            if (abs_f(lwc1(SC + 0x18) - f20) < 2.0f) swc1(SC + 0x18, f20);
        }
        hero_place(SC + 0x10, SC + 0x20, 0, 1);
    }
    if (const uint32_t m = lwu(TK + 8); m != 0) sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
    if (const uint32_t m = lwu(OV(0x001b9cf0) + 0xec); m != 0) {
        sh(m + 0x34, lhu(m + 0x34) & 0xfffeu);
        sw(OV(0x001b9cf0) + 0xec, 0);
    }
    if (const uint32_t m = lwu(TK + 8); m != 0) {
        const uint32_t t = lwu(TK + 0xc);
        sw(TK + 0xc, 0);
        sw(TK + 8, 0);
        talk_step(m, t, 1);
    }
    music_pause_timer(frame_scale_i(0x1e));
}

// The scenes' level-specific extras each frame: Veldin, the ships (class 0x212) drawn with their
// engines (0x2935b8); level 2, the class 0x1b1 mobys flagged 0x800; level 3 scene 5 frames
// 900..1100, four sparks (particle_16_spawn 0x26ced0) from the scene's fourth moby's (+0x184) chain points; level 7
// scene 4, the ship (0x214, second moby) with its engines; level 14 scene 0 frames 0x1c..0x21,
// the first moby's effect 0x23e008 and its draw 0x23db30 (shared_fx_tr.cpp; Oltanis runs its own
// build of this function, so only a level 14 without it would reach them); level 15 scenes 3 / 4,
// the mobys' lights. Aridia's and Kerwan's overlays carry their own builds with only their level's branch (L2:0x002400c8,
// L3:0x0022c728: the same instructions as this one's level 2 / level 3 branch; the others are keyed on other levels): this
// port runs there, proved against both builds by batch 410.
// orig: 0x0023e268 L2:0x002400c8 L3:0x0022c728
void cutscene_level_extras() {
    if (on_umbris()) { umb_call(umb_cutscene_level_extras_u); return; }  // (level 7's own: L7:0x00265bd8)
    if (on_oltanis()) { olt_cutscene_level_extras_l14(0); return; }  // (level 14's own: L14:0x002460f8, its scene 0 effect and draw)
    if (on_quartu()) { qua_call(qua_cutscene_level_extras_u); return; }  // (level 15's own: L15:0x0022db50)
    if (lw(0x0015ed84) == 0) {
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            if (lh(m + 0xa6) == 0x212) sub_001f93e0(OVF(0x002935b8), m);
        }
    }
    if (lw(0x0015ed84) == 2) {
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) {
            const uint32_t m = lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4);
            if (lh(m + 0xa6) == 0x1b1) sh(m + 0x34, lhu(m + 0x34) | 0x800u);
        }
    }
    if (lw(0x0015ed84) == 3 && lw(SC + 0x30) == 5 && frame_scale_i(0x384) < lw(SC + 0x34) && lw(SC + 0x34) < frame_scale_i(0x44c)) {
        const uint32_t m = lwu(SC + 0x184);
        if (m != 0) {
            const uint32_t f = ee_stack_alloc(0x20);  // (the frame's two vectors handed to the spawner: the point +0, the velocity +0x10)
            for (int32_t s1 = 0; s1 < 4; ++s1) {
                Vec4& at = V(f);
                Vec4& dir = V(f + 0x10);
                vec_clear(dir);
                moby_chain_point_v(m, static_cast<uint32_t>(s1 % 2), at);
                const float f21 = rand_angle();
                const float f20 = rand_angle();
                const float k = lwc1(0x0015ed6c);
                const float len = rand_range_f(k * u2f(0x3f333333u), k * u2f(0x400ccccdu));
                vec_from_spherical_v(dir, len, f21, f20);
                const int32_t lo = frame_scale_i(0xc);
                const int32_t hi = frame_scale_i(0x23);
                const int32_t life = rand_range(lo, hi);
                particle_16_spawn(f, f + 0x10, 0x80808080u, 0x00808080u, life, u2f(0x480f8e00u));
            }
            ee_stack_free(0x20);
        }
    }
    if (lw(0x0015ed84) == 7 && lw(SC + 0x30) == 4) {
        const uint32_t m = lwu(SC + 0x188);
        if (m != 0 && lh(m + 0xa6) == 0x214) sub_001f93e0(OVF(0x002935b8), m);
    }
    if (lw(0x0015ed84) == 0xe && lw(SC + 0x30) == 0) {
        const uint32_t m = lwu(SC + 0x17c);
        if (m != 0 && static_cast<uint32_t>(lw(SC + 0x34) - 0x1c) < 6u) {
            cutscene_l14_effect_tr(m);
            sub_001f93e0(OVF(0x0023db30), m);
        }
    }
    if (lw(0x0015ed84) == 0xf && static_cast<uint32_t>(lw(SC + 0x30) - 3) < 2u) {
        for (int32_t i = 0; i < lh(SC + 0x44); ++i) moby_update_light(lwu(SC + 0x178 + static_cast<uint32_t>(i) * 4));
    }
}

// The scene's subtitle: drawn by the frame's 2D (render_frame 0x1f85b8) in game mode 2 when the
// Options' Subtitles switch (0x15ee40) is on. The block's table *(0x16c8ac) (the block's +4, none
// below 0x400: scene_block_decode) holds records of 0x10 bytes: +0 the first frame, +2 the last
// (s16, the scene's frame +0x34), +4 the text's offset from the table in each language (s16:
// English, French, German, Spanish, Italian), ended by a negative first frame. The first record
// whose frames hold the scene's frame is drawn: its text measured in the box (flag 4), the box's
// line moved up when its lower half would pass the screen's height - 0x14, a grey rounded box
// (alpha 0x60) behind it, then the text drawn, centred, grey 0x80b0b0b0.
// orig: 0x001f9a20
void cutscene_subtitle_draw() {
    const uint32_t tbl = lwu(SC + 0x4c);
    if (tbl == 0) return;
    const int32_t lang = lw(0x0015ed88);
    // English, or French / German / Spanish / Italian (2..5) at 1..4 (sltu 3 < lang - 2: 0)
    const int32_t li = 3u < static_cast<uint32_t>(lang - 2) ? 0 : lang - 1;
    uint32_t r = tbl;
    if (lh(r) < 0) return;
    uint32_t txt = tbl + 4 + static_cast<uint32_t>(li) * 2;
    const uint32_t f = ee_stack_alloc(0x70);
    const uint32_t b = f + 0x10;  // the text box
    do {
        const int32_t frame = lw(SC + 0x34);
        if (!(frame < lh(r)) && !(lh(r + 2) < frame)) {
            sw(f, 7);  // (the stack argument)
            text_box_set(b, 0xc8, 0x208, 0x28, 0x1d8, 0x100, lw(0x0013e504) - 0x38, 0x12, 7);
            text_box_small(b, 0x80b0b0b0ull, lwu(SC + 0x4c) + static_cast<uint32_t>(lh(txt)), -1);
            const uint32_t h = lhu(0x0013e504);
            const uint32_t y = h - 0x3c;
            const int32_t hh = static_cast<int16_t>(lhu(b + 0xe)) >> 1;  // half the text's height
            const int32_t dy = hh + 5;
            const int32_t hw = static_cast<int16_t>(lhu(b + 0xc)) >> 1;  // half its width
            sh(b + 0xa, y);
            if (lw(0x0013e504) - 0x14 < static_cast<int16_t>(y) + dy) sh(b + 0xa, h - static_cast<uint32_t>(hh + 0x19));
            const int32_t cy = lh(b + 0xa);
            box_rounded(cy - dy, cy + dy, 0x100 - (hw + 0xa), hw + 0x10a, 0x60);
            sh(b + 0x12, lhu(b + 0x12) & 0xfffbu);
            text_box_small(b, 0x80b0b0b0ull, lwu(SC + 0x4c) + static_cast<uint32_t>(lh(txt)), -1);
            break;
        }
        r += 0x10;
        txt += 0x10;
    } while (!(lh(r) < 0));
    ee_stack_free(0x70);
}

}  // namespace rac1
