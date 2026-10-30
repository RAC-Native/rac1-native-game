// Veldin's remaining moby callbacks: the class-1545 update, the class-1440 muzzle
// flash's billboard, the ship's update and the ships' canopy reflection, with their helpers.
// Header veldin_mobys.h.
#include <game/ovl/veldin_mobys.h>

#include <game/ovl/ovl_reloc.h>
#include <game/lib/libgraph.h>
#include <game/math/vecmath.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/screen_fade.h>
#include <game/port_stubs.h>
#include <runtime/kernel/bootstrap.h>

namespace rac1 {

// A class-750 moby of the scene smoking (the cutscene's crash site): at its chain 0's joint,
// moved back along the joint's z axis, two dark smoke puffs (kind 0x17, colour 0x503030ff) and
// five white ones (0x7fffffff, random frame), sized by the joint's scale (its x row's length
// / 1.026, at most 1).
// orig: 0x002637f8
void scene_moby_smoke(uint32_t m) {
    if (lh(m + 0xa6) != 0x2ee) return;
    if (m == 0) return;
    Mat4 w;
    moby_chain_world_one_v(m, 0, w);
    float s = fpu_div(vec3_length(w.r[0]), u2f(0x3f8353f8u));
    if (1.0f < s) s = 1.0f;
    const float k = u2f(0xbccccccdu);
    float len = s * k + k;
    float size = s * 44000.0f + 6000.0f;
    float jitter = s * u2f(0x3ca3d70au) + u2f(0x3c23d70au);
    Vec4 off, pos;
    vec3_set_length(off, w.r[2], len);
    vec3_add(pos, w.r[3], off);
    for (int32_t i = 1; i >= 0; --i) {
        const int32_t r = rand_mod(0x10);
        const int32_t spin = rand_mod(2) == 0 ? r : -r;
        const uint32_t p = spawn_particle_17_v(pos, spin, V(OV(0x0015f580)), 0x503030ffu, jitter, 1.0f, 0.75f, size);
        if (p == 0) continue;
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(6)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x50);
        sb(p + 0x2b, lbu(p + 0xa));
    }
    len = s * u2f(0xbc75c290u) + u2f(0xbc23d70au);
    size = s * 25000.0f + 5000.0f;
    jitter = s * u2f(0x3c75c290u) + u2f(0x3c23d70au);
    vec3_set_length(off, w.r[2], len);
    vec3_add(pos, w.r[3], off);
    for (int32_t i = 4; i >= 0; --i) {
        const uint32_t p = spawn_particle_17_v(pos, 0x10, V(OV(0x0015f580)), 0x7fffffffu, jitter, 1.0f, 1.0f, size);
        if (p == 0) continue;
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(2)));
        sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x7f);
        sb(p + 0x2b, lbu(p + 0xa));
    }
}

// A burst of smoke: two red-brown puffs (0x7f204080, 30 frames) at `a` of size `size_a` and three
// white ones (0x7fffffff, 6 frames, random frame) at `b` of size `size_b`, spinning one way
// then the other, drifting along `vel`.
// orig: 0x00263b70
void smoke_burst_v(const Vec4& a, const Vec4& b, const Vec4& vel, float size_a, float size_b) {
    for (int32_t i = 1; i >= 0; --i) {
        const int32_t r = rand_mod(0x10);
        const int32_t spin = rand_mod(2) == 0 ? r : -r;
        const uint32_t p = spawn_particle_17_v(a, spin, vel, 0x7f204080u, u2f(0x3dcccccdu), 1.0f, u2f(0x3f666666u), size_a);
        if (p == 0) continue;
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x1e)));
        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x7f);
        sb(p + 0x2b, lbu(p + 0xa));
    }
    int32_t spin = 0x10;
    for (int32_t i = 2; i >= 0; --i) {
        const uint32_t p = spawn_particle_17_v(b, spin, vel, 0x7fffffffu, u2f(0x3d4ccccdu), 1.0f, u2f(0x3f7851ecu), size_b);
        spin = -spin;
        if (p == 0) continue;
        sb(p + 9, static_cast<uint32_t>(float_to_int(4.0f) + 0x40));
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(6)));
        sb(p + 8, static_cast<uint32_t>(rand_mod(0xff)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, 0x7f);
        sb(p + 0x2b, lbu(p + 0xa));
    }
}
// orig: 0x00263b70 (EE-address form)
void smoke_burst(uint32_t a, uint32_t b, uint32_t vel, float size_a, float size_b) { smoke_burst_v(V(a), V(b), V(vel), size_a, size_b); }

// Class 1545 (the crash site): started (state 1, +0x30 = 0xff); in game mode 2 (a cutscene) on
// scene 2 from frame 0x618 (scene moby 3) and on scene 3 (scene moby 2) that moby smokes
// (0x2637f8); on scene 4 between frames 0x96 and 0xdc a burst of smoke and ten dark puffs at
// scene moby 9's chain 0 point. Scene state 0x16c860: +0x30 the scene, +0x34 the frame,
// +0x178.. its mobys.
// orig: 0x002e3800
void moby_1545_update(uint32_t m) {
    const uint32_t st = lbu(m + 0x20);
    if (st == 0) {
        sb(m + 0x20, 1);
        sb(m + 0x30, 0xff);
        return;
    }
    if (st != 1) return;
    if (lw(OV(0x0015f5c4)) != 2) return;
    constexpr OvConst<0x0016c860u> SC{};
    bool smoke = false;
    if (lw(SC + 0x30) == 2 && !(lw(SC + 0x34) < frame_scale_i(0x618))) {
        smoke = true;
    } else {
        if (lw(OV(0x0015f5c4)) != 2) return;
        smoke = lw(SC + 0x30) == 3;
    }
    if (smoke) {
        const int32_t scene = lw(SC + 0x30);
        int32_t idx;
        if (scene == 2) {
            idx = 3;
        } else {
            idx = -1;
            if (scene == 3) idx = 2;
        }
        scene_moby_smoke(lwu(SC + 0x178 + static_cast<uint32_t>(idx) * 4));
    }
    if (lw(OV(0x0015f5c4)) != 2) return;
    if (lw(SC + 0x30) != 4) return;
    const uint32_t src = lwu(SC + 0x184);
    if (src == 0) return;
    if (lw(SC + 0x34) < frame_scale_i(0x96)) return;
    if (frame_scale_i(0xdc) < lw(SC + 0x34)) return;
    Vec4 at;
    moby_chain_point_v(src, 0, at);
    smoke_burst_v(at, at, V(OV(0x0015f580)), u2f(0x47ea6000u), u2f(0x47435000u));
    for (int32_t i = 9; i >= 0; --i) {
        const uint32_t c = (static_cast<uint32_t>(rand_range(0x40, 0x80)) << 24) | 0x00787878u;
        const int32_t r = rand_range(0, 4);
        const int32_t spin = rand_mod(2) == 0 ? r : -r;
        const uint32_t p = spawn_particle_17_v(at, spin, V(OV(0x0015f580)), c, 0.75f, 1.0f, u2f(0x3f83d70au), u2f(0x47ea6000u));
        if (p == 0) continue;
        sb(p + 3, 0x44);
        sb(p + 8, static_cast<uint32_t>(rand_range(0, 0xff)));
        sh(p + 0xa, static_cast<uint32_t>(frame_scale_i(0x3c)));
        sw(p + 0x24, 2);
        sb(p + 0x2a, static_cast<uint32_t>(static_cast<int32_t>(c) >> 24));
        sb(p + 0x2b, lbu(p + 0xa));
    }
}

// The class-1440 enemy's muzzle flash (queued by 0x2e1aa8): a billboard at its pvars +0x1c0
// facing the camera (rows: towards the camera, its cross with the up axis 0x13f5e0 negated,
// their cross, the point), the template quad 0x1e6ca0 scaled by 0.2, effect texture 0xb,
// colour 0x80ffffff, additive.
// orig: 0x002e1c78
void e1440_flash_draw(uint32_t m) {
    const uint32_t f = ee_stack_alloc(0xd0);  // the quad +0x00..+0x90, its matrix +0x90
    const uint32_t mat = f + 0x90;
    lq_sq(mat + 0x30, lwu(m + 0x78) + 0x1c0);
    swc1(mat + 0x3c, 1.0f);
    vec3_sub(V(mat), V(OV(0x00166dc0)), V(mat + 0x30));
    vec3_set_length(V(mat), V(mat), 1.0f);
    vec3_cross_ba(V(mat + 0x10), V(mat), V(0x0013f5e0));
    vec3_set_length(V(mat + 0x10), V(mat + 0x10), -1.0f);
    vec3_cross_ba(V(mat + 0x20), V(mat + 0x10), V(mat));
    sd(f + 0x78, effect_tex0(0xb));
    sd(f + 0x88, 0x8000000048ull);
    sd(f + 0x80, 0x0000ff9000000260ull);
    sd(f + 0x70, 5);
    sw(f + 0x40, 0x80ffffffu);
    swc1(f + 0x6c, 1.0f);
    sw(f + 0x4c, 0x80ffffffu);
    sw(f + 0x48, 0x80ffffffu);
    sw(f + 0x44, 0x80ffffffu);
    sw(f + 0x50, 0);
    sw(f + 0x54, 0);
    sw(f + 0x58, 0);
    swc1(f + 0x5c, 1.0f);
    swc1(f + 0x60, 1.0f);
    sw(f + 0x64, 0);
    swc1(f + 0x68, 1.0f);
    for (uint32_t k = 0; k < 4; ++k) vec3_scale(V(f + k * 0x10), V(OV(0x001e6ca0) + k * 0x10), u2f(0x3e4ccccdu));
    quad_draw(f, mat);
    ee_stack_free(0xd0);
}

// The HUD message of `who` taken down: when 0x15f594 is `who` and a request is up (0x161e04),
// it is cleared (0x235908) and 0x15f590 / 0x15f594 reset. 1 when one was.
// orig: 0x002644e0
int32_t hud_message_release(int32_t who) {
    if (lw(OV(0x0015f594)) != who) return 0;
    const int32_t serial = lw(OV(0x00161e04));
    if (serial == -1) return 0;
    sw(OV(0x0015f590), 0);
    sw(OV(0x0015f594), 0);
    hud_request_clear(serial);
    sw(OV(0x00161e04), 0xffffffffu);
    return 1;
}

// Game mode 1 started (the movie): the sounds and the music stopped, the movie's sector / size /
// `arg` kept at 0x161048 / 0x16104c / 0x161050, the frame cut short, the screen faded (0x1f9838,
// not ported), game mode 1, 0x161054 / 0x161058 the buffers; then the 0xd (PAL 0xe) 128 x 128
// PSMCT24 images and 0x11 (PAL 0xe) 64 x 64 PSMCT32 ones at *(0x15f40c) uploaded to the GS at
// *(0x15ee88) / *(0x15ee78) (a debug trap past 4 MB of VRAM).
// orig: 0x002995d0
void movie_start(uint32_t sector, uint32_t size, uint32_t arg) {
    sb(0x0013e5bb, lbu(0x0013e5bb) | 8u);
    FlushCache(0);
    sound_stop_all();
    music_stop_all();
    sw(OV(0x00161048), sector);
    sw(OV(0x00161050), arg);
    sw(OV(0x0016104c), size);
    sw(OV(0x0015f5d8), 1);
    screen_fade(4);
    sub_002039a0();
    uint32_t src = lwu(OV(0x0015f40c));
    const bool pal = lw(0x0015ed80) != 0;
    sw(OV(0x0015f5c4), 1);
    sw(OV(0x00161054), lwu(OV(0x00173e04)));
    sw(OV(0x00161058), src);
    const int32_t big = pal ? 0xe : 0xd;
    const int32_t small = pal ? 0xe : 0x11;
    uint32_t vram = lwu(0x0015ee88);
    const uint32_t lp = ee_stack_alloc(0x60);
    for (int32_t n = big; n != 0; --n) {
        gs_set_def_load_image(lp, static_cast<int32_t>(vram << 8) >> 16, 2, 1, 0, 0, 0x80, 0x80);
        vram += 0x10000;
        FlushCache(0);
        gs_exec_load_image(lp, src);
        src += 0xc000;
        gs_sync_path(0, 0);
    }
    vram = lwu(0x0015ee78);
    for (int32_t n = small; n != 0; --n) {
        gs_set_def_load_image(lp, static_cast<int32_t>(vram << 8) >> 16, 1, 0, 0, 0, 0x40, 0x40);
        vram += 0x4000;
        FlushCache(0);
        gs_exec_load_image(lp, src);
        src += 0x4000;
        gs_sync_path(0, 0);
    }
    ee_stack_free(0x60);
    if (0x400000 < static_cast<int32_t>(vram)) {
        debug_printf_stub();  // (0x1e9480, vram - 0x400000)
        debug_trap();
    }
}

// Movie `n` (the level header's table 0x137b80 + 0x1808, PAL + 0x18a0: sector, size): with
// 0x1415f8 clear the hero is lost (0x211250) instead; nothing while the respawn flag 0x141401 is
// set; else game mode 1 starts (0x2995d0) with *(0x15ed88).
// orig: 0x00299460
void movie_play(int32_t n) {
    if (n < 0) return;
    const uint32_t e = 0x00137b80 + static_cast<uint32_t>(n) * 8;
    uint32_t size, sector;
    if (lw(0x0015ed80) != 0) {
        size = lwu(e + 0x18a4);
        sector = lwu(e + 0x18a0);
    } else {
        size = lwu(e + 0x180c);
        sector = lwu(e + 0x1808);
    }
    if (lw(0x001415f8) == 0) {
        hero_lost();
        return;
    }
    if (lbu(0x00141401) != 0) return;
    movie_start(sector, size, lwu(0x0015ed88));
}

// The ship (classes 0x213..0x215, *(0x13e030), set up by 0x28e8a0). Shown: its engines' glow
// +0xbc steps by 2 (a grey 0x96 + 50 cos(pi/128 (glow - 0x80)) into +0x90, red halved on 0x215),
// its canopy drawn (0x2935b8) and within 32 of the camera (xy) 0x28e670 queued. The boarding
// prompt (pvars +0xc): on while the hero is within 4 of the hatch (the variant's point 0x1bd880
// through the ship's rotation), off past 4.1 or when the ship is hidden; while on, the HUD
// message 0x53e4 and triangle (0x13cae4 & 0x10): on level 10 with 0x1413f4 == 1 state 0x2a and
// scene 9, else the message taken down, 0x15f630 = 1 (the take-off) and 0x13e058 = 0.
// State 0x2a (outside game mode 2): the scenes 4 (with 0x13d4dc), 5, the movie 0xb and scene 6
// (with 0x13dd4b) once each (flags 0x13d3d0..0x13d3d3), then state 0, shown, 0x13e058 = 1 and
// the take-off.
// orig: 0x0028e180
void ship_update(uint32_t m) {
    const uint32_t pv = lwu(m + 0x78);
    if (lbu(m + 0x20) == 0x2a) {
        if (lw(OV(0x0015f5c4)) == 2) return;
        constexpr uint32_t F = 0x0013d388;
        if (lbu(F + 0x48) == 0 && lbu(0x0013d4dc) != 0) {
            cutscene_start(4);
            sb(F + 0x48, 1);
            return;
        }
        if (lbu(F + 0x49) == 0 && lbu(0x0013dd4b) != 0) {
            cutscene_start(5);
            sb(F + 0x49, 1);
            return;
        }
        if (lbu(F + 0x4a) == 0 && lbu(0x0013dd4b) != 0) {
            movie_play(0xb);
            sb(F + 0x4a, 1);
            return;
        }
        if (lbu(F + 0x4b) == 0 && lbu(0x0013dd4b) != 0) {
            cutscene_start(6);
            sb(F + 0x4b, 1);
            return;
        }
        sb(m + 0x20, 0);
        sh(m + 0x34, lhu(m + 0x34) & 0xffbeu);
        sh(0x0013e058, 1);
        sw(OV(0x0015f630), 1);
        return;
    }
    if (lbu(m + 0x31) != 0) {
        const uint32_t g = lbu(m + 0xbc) + 2u;
        sb(m + 0xbc, g);
        const float c = cos_vu(int_to_float(static_cast<int32_t>(g & 0xffu) - 0x80) * u2f(0x3cc90fdbu));
        const int32_t a = float_to_int(c * 50.0f) + 0x96;
        uint32_t col;
        if (lh(m + 0xa6) == 0x215) {
            col = static_cast<uint32_t>(a >> 1) | static_cast<uint32_t>(a << 8) | static_cast<uint32_t>(a << 16);
        } else {
            col = static_cast<uint32_t>(a) | static_cast<uint32_t>(a << 8) | static_cast<uint32_t>(a << 16);
        }
        sw(m + 0x90, col);
        draw_callback_add(OVF(0x002935b8), m);
        if (vec2_distance(V(m + 0x10), V(OV(0x00166dc0))) < 32.0f) sub_001f93e0(OVF(0x0028e670), m);
    }
    const uint32_t hatch = OV(0x001bd880) + static_cast<uint32_t>(lh(0x0013e056)) * 0x10;
    if (lh(pv + 0xc) != 0) {
        Vec4 at;
        mat3_transform(at, V(hatch), M(m + 0xc0));
        vec3_add(at, at, V(m + 0x10));
        if (u2f(0x40833333u) < vec3_distance(at, V(0x0013f3d0))) sh(pv + 0xc, 0);
    } else if (vec2_distance(V(m + 0x10), V(0x0013f3d0)) < 6.0f) {
        Vec4 at;
        mat3_transform(at, V(hatch), M(m + 0xc0));
        vec3_add(at, at, V(m + 0x10));
        if (vec3_distance(at, V(0x0013f3d0)) < 4.0f) sh(pv + 0xc, 1);
    }
    if ((lhu(m + 0x34) & 1u) != 0) sh(pv + 0xc, 0);
    if (lh(pv + 0xc) == 0) return;
    const bool shown = hud_message_request(2, 0x53e4) != 0;
    if ((lw(0x0013cae4) & 0x10) == 0) return;
    if (!shown) return;
    if (lw(0x0015ed84) == 10 && lbu(0x001413f4) == 1) {
        sb(m + 0x20, 0x2a);
        sh(m + 0x34, lhu(m + 0x34) | 0x41u);
        cutscene_start(9);
        return;
    }
    hud_message_release(2);
    sw(OV(0x0015f630), 1);
    sh(0x0013e058, 0);
}

// The ships' canopy (classes 0x212..0x215, per class from 0x1605e0.. : colour, vertices,
// quads, vertex / normal / quad tables in the overlay): the vertices through the moby's chain 0
// matrix into 0x1c05e0, each one's sphere-map coordinates (the view reflected off its normal)
// into 0x1c0c40, blended over 60 frames (0x16056c) from the last ones kept at 0x1c0f70 after a
// camera cut (0x160564 / 0x160568); then the quads (four vertex indices each) drawn with effect
// texture 0x15 (1 in the ship sequence's step 4), additive. Live outside gameplay (game mode 0)
// only within 16 of the camera (x and y), never in step 4 of mode 6.
// orig: 0x002935b8
void ship_canopy_draw(uint32_t m) {
    const int32_t mode = lw(OV(0x0015f5c4));
    const uint32_t k = static_cast<uint32_t>(lh(m + 0xa6) - 0x212) * 4;
    const uint32_t quads = lwu(OV(0x00160630) + k);
    const uint32_t verts = lwu(OV(0x00160620) + k);
    uint32_t normals = lwu(OV(0x00160610) + k);
    const int32_t nq = lw(OV(0x00160600) + k);
    const int32_t nv = lw(OV(0x001605f0) + k);
    int32_t tex = 0x15;
    if (mode == 6 && lw(0x0013e050) == 4) tex = 1;
    const uint32_t f = ee_stack_alloc(0x90);  // the quad
    sd(f + 0x78, effect_tex0(tex));
    const uint32_t col = lwu(OV(0x001605e0) + k);
    sd(f + 0x88, 0x8000000044ull);
    sd(f + 0x80, 0x0000ff9000000260ull);
    sw(f + 0x40, col);
    sd(f + 0x70, 0);
    sw(f + 0x4c, col);
    sw(f + 0x48, col);
    Mat4 w;
    sw(f + 0x44, col);
    moby_chain_world_one_v(m, 0, w);
    bool live = false;
    if (lw(OV(0x0015f5c4)) != 0) {
        live = true;
    } else if (abs_f(lwc1(OV(0x00166dc0)) - lwc1(m + 0x10)) < 16.0f && abs_f(lwc1(OV(0x00166dc4)) - lwc1(m + 0x14)) < 16.0f) {
        live = true;
    }
    if (lw(OV(0x0015f5c4)) == 6 && lw(0x0013e050) == 4) live = false;
    const uint32_t ST = OV(0x00160564), CUT = OV(0x00160568), TM = OV(0x0016056c);
    const uint32_t XF = OV(0x001c05e0), UV = OV(0x001c0c40), UV0 = OV(0x001c0f70);
    if (live || lw(ST) == 1) {
        sw(CUT, 1);
        timer_tick_i32(*static_cast<int32_t*>(ee_ram_ptr(TM)));
        const float t = int_to_float(lw(TM));
        const float blend = fpu_div(t, int_to_float(frame_scale_i(0x3c)));
        uint32_t v = verts;
        for (int32_t i = 0; i < nv; ++i) {
            const uint32_t o = static_cast<uint32_t>(i);
            mat4_transform(V(XF + o * 0x10), V(v), w);
            Vec4 view, n, r;
            vec3_sub(view, V(XF + o * 0x10), V(OV(0x00166dc0)));
            vec3_set_length(view, view, 1.0f);
            mat4_transform(n, V(normals), w);
            vec3_set_length(n, n, u2f(0x3dcccccdu));
            const float d = vec3_dot(n, view);
            vec3_scale(r, n, d + d);
            vec3_sub(r, view, r);
            vec3_set_length(r, r, 1.0f);
            r.z = r.z + 1.0f;
            const float q = sqrt_f(r.z + r.z);
            const float q2 = q + q;
            const uint32_t uv = UV + o * 8, uv0 = UV0 + o * 8;
            if (lw(ST) == 1 || lw(TM) == 0) {
                swc1(uv, fpu_div(r.x, q2) + 0.5f);
                swc1(uv + 4, fpu_div(r.y, q2) + 0.5f);
            } else {
                const float s0 = fpu_div(r.x, q2) + 0.5f;
                const float t0 = fpu_div(r.y, q2) + 0.5f;
                swc1(uv, s0 + (lwc1(uv0) - s0) * blend);
                swc1(uv + 4, t0 + (lwc1(uv0 + 4) - t0) * blend);
            }
            normals += 0x10;
            v += 0x10;
        }
        if (lw(ST) == 1) sw(ST, 2);
    } else {
        if (lw(CUT) == 1) {
            sw(CUT, 0);
            for (int32_t i = 0; i < nv; ++i) {
                const uint32_t o = static_cast<uint32_t>(i);
                swc1(UV0 + o * 8, lwc1(UV + o * 8));
                swc1(UV0 + o * 8 + 4, lwc1(UV + o * 8 + 4));
                mat4_transform(V(XF + o * 0x10), V(verts + o * 0x10), w);
            }
        } else {
            for (int32_t i = 0; i < nv; ++i) {
                const uint32_t o = static_cast<uint32_t>(i);
                mat4_transform(V(XF + o * 0x10), V(verts + o * 0x10), w);
            }
        }
        sw(TM, static_cast<uint32_t>(frame_scale_i(0x3c)));
    }
    for (int32_t q = 0; q < nq; ++q) {
        const uint32_t e = quads + static_cast<uint32_t>(q) * 0x10;
        for (uint32_t c = 0; c < 4; ++c) {
            const uint32_t idx = static_cast<uint32_t>(static_cast<int32_t>(lh(e + c * 4)));
            lq_sq(f + c * 0x10, XF + idx * 0x10);
            swc1(f + 0x50 + c * 8, lwc1(UV + idx * 8));
            swc1(f + 0x54 + c * 8, lwc1(UV + 4 + idx * 8));
        }
        quad_draw(f, 0);
    }
    ee_stack_free(0x90);
}

// orig: (none) — the callbacks above for the indirect calls (moby +0x74, the draw lists)
void veldin_moby_fns_register() {
    ovl_register_moby_fn(0x002e3800, moby_1545_update);
    ovl_register_moby_fn(0x002e1c78, e1440_flash_draw);
    ovl_register_moby_fn(0x0028e180, ship_update);
    ovl_register_moby_fn(0x002935b8, ship_canopy_draw);
}

}  // namespace rac1
