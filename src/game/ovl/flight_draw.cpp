// See flight_draw.h. Ported by hand from Veldin's listings.
#include <game/ovl/flight_draw.h>

#include <game/math/vecmath.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/ship_draw_cbs.h>
#include <game/ovl/sky_ee.h>
#include <game/ovl/veldin_mobys.h>
#include <game/ovl/world_cull.h>
#include <game/port_stubs.h>
#include <game/render/mrp_fx.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>

namespace rac1 {

namespace {
// orig: (helper) repeated in 0x0028b440 (as in 0x23d968): GS register 0x4e (ZBUF_1) = 0x01000000 | the word 0x15ee88 >> 13
// (sra of the sign-extended word, or'ed in 64 bits)
uint64_t zbuf_word() { return 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(0x0015ee88))) >> 13); }
// orig: (host) the flight's sky as a pass for the host's world_pass (render_frame's sky: its chain kept for the MRP / faithful sky)
void flight_sky_pass() { flight_sky(); }
}  // namespace

// orig: 0x0028b440
void flight_sky() {
    const uint32_t sp = ee_stack_alloc(0x10);  // (the original's frame: the angles at sp+0)
    ovl_28b680_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    int32_t i = 0;  // s1
    sh(lwu(OV(0x0016051c)) + 4, 0);
    mat4_identity(M(OV(0x001bd7e0)));
    vec_clear(V(sp));
    if (0 < lh(lwu(OV(0x0016051c)) + 6)) {
        do {
            float scale = 1.0f;  // f20
            if (static_cast<uint32_t>(i) < 6) {  // the jump table 0x1e93b0 (case 0 runs on into case 1)
                switch (ovl_code_to_veldin(lwu(OV(0x001e93b0) + static_cast<uint32_t>(i) * 4))) {
                    case 0x0028b4c4:
                        sw(sp + 4, 0);
                        swc1(sp + 8, lwc1(OV(0x00166c00u - 0x673c)));
                        [[fallthrough]];
                    case 0x0028b4d0:
                        sw(sp + 4, 0);
                        scale = 1.0f;
                        swc1(sp + 8, angle_add(lwc1(OV(0x00166c00u - 0x673c)), lwc1(sp + 4)));
                        break;
                    case 0x0028b4f0:
                        swc1(sp + 4, -0.075f);  // 0xbd99999a
                        swc1(sp + 8, angle_add(lwc1(OV(0x00166c00u - 0x673c)), -0.15f));  // 0xbe19999a
                        scale = 1.25f;
                        break;
                    case 0x0028b524:
                        swc1(sp + 4, 0.05f);  // 0x3d4ccccd
                        swc1(sp + 8, angle_add(lwc1(OV(0x00166c00u - 0x673c)), 0.125f));  // 0x3e000000
                        scale = 1.5f;
                        break;
                    case 0x0028b554:
                        swc1(sp + 4, 0.1f);  // 0x3dcccccd
                        swc1(sp + 8, angle_add(lwc1(OV(0x00166c00u - 0x673c)), -0.05f));  // 0xbd4ccccd
                        scale = 1.75f;
                        break;
                    case 0x0028b588:
                        swc1(sp + 4, -0.15f);  // 0xbe19999a
                        swc1(sp + 8, angle_add(lwc1(OV(0x00166c00u - 0x673c)), 0.1f));  // 0x3dcccccd
                        scale = 2.0f;
                        break;
                    default: break;
                }
            }
            const uint32_t m = OV(0x001bd7e0);
            euler_to_mat4_fpu(M(m), V(sp));
            vec4_scale(V(m), V(m), scale);
            vec4_scale(V(m + 0x10), V(m + 0x10), scale);
            vec4_scale(V(m + 0x20), V(m + 0x20), scale);
            lq_sq(m + 0x30, OV(0x00160520));  // the camera's position
            ovl_28b848_tr(static_cast<uint32_t>(i), 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
            ++i;
        } while (i < lh(lwu(OV(0x0016051c)) + 6));
    }
    ovl_28b710_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);
    sub_002a0ba8(0x47, 0x5360b);
    sub_002a0ba8(0x4e, zbuf_word());
    ee_stack_free(0x10);
}

// orig: 0x0028f6e0
void flight_planet_picture() {
    const uint32_t d = ee_stack_alloc(0x100);  // the quad's descriptor (quad_draw.h) at the original's sp
    sub_002a0ba8(0x47, 0x31801);
    float scale = 1.0f;
    sd(d + 0x88, 0x0000008000000044ull);  // ALPHA (0x8000 << 24 | 0x44)
    sd(d + 0x78, ld(OV(0x00160640)));     // TEX0
    sd(d + 0x80, 0x0000ff9000000260ull);  // TEX1 (0xff90 << 32 | 0x260)
    sd(d + 0x70, 0);                      // CLAMP
    mem_copy128(d + 0x50, OV(0x001bd8b0), 0x20);  // the corners' (s, t)
    const uint32_t level = lwu(0x0015ed84);
    if (level < 0x13) scale = lwc1(OV(0x001bdb90) + level * 4);
    uint32_t src = OV(0x001bdb50);
    for (uint32_t k = 0; k < 4; ++k) {
        sw(d + 0x40 + k * 4, 0x80808080u);
        vec3_scale(V(d + k * 16), V(src), scale);
        src += 16;
        vec3_add(V(d + k * 16), V(d + k * 16), V(OV(0x00166c00u - 0x6650)));
    }
    quad_draw(d, 0);
    sub_002a0ba8(0x47, 0x5360b);
    ee_stack_free(0x100);
}

// orig: 0x0028f820
void flight_banner(int32_t a) {
    sub_002a0ba8(0x47, 0x31801);
    sub_002a0ba8(0x42, 0x0000008000000044ull);
    // (sll a, 24: sign-extended; or 0x808080)
    const uint64_t colour = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(static_cast<uint32_t>(a) << 24))) | 0x00808080ull;
    glyph_sprite(0x20, lw(0x0013e504) - 0x58, 0x100, 0x20, 0, 0, 0x100, 0x20, colour, ld(OV(0x00160648)));
    sub_002a0ba8(0x47, 0x5360b);
}

// orig: 0x002900a0
void flight_draw() {
    mrp_fx_frame_begin();  // (host: as render_frame)
    render_view_layers_begin();  // (host: as render_frame)
    sub_00200fd8();
    render_view_setup();
    moby_anim_lerp_slots_validate();
    render_static_packets();
    const uint32_t cam = OV(0x0016ca40);
    const float fov = lwc1(cam + 0xb0);  // f20
    sw(OV(0x00166c00u - 0x7620), 0xffffffffu);
    if (fov < 0.63f) swc1(cam + 0xb0, 0.63f);  // 0x3f2147ae
    projection_setup();
    render_view_setup();
    // the sky (render_frame's way: a pass for the host, its chain kept when the sky is drawn from it)
    // (with RAC1_MRP=0 the host has no sky of the flight's own to draw instead, as render_frame's sky_draw: the chain is kept, the
    // ported VU1 programs and the GS replica draw it; off only from the debug menu's Sky checkbox)
    if (sky_wanted()) world_pass(flight_sky_pass, -1);
    else if (g_render_sky) flight_sky();
    else TODO_PORT(0x0028b440, "the flight's sky: off (debug menu)")();
    swc1(cam + 0xb0, fov);
    projection_setup();
    render_view_setup();
    if (lw(0x0013e088) == 4) {
        texture_block_begin(1);
        mrp_fx_window(true);  // (host: the picture's quad, 0x1fd228, noted for the MRP's effects)
        flight_planet_picture();
        mrp_fx_window(false);
        texture_block_end();
    }
    if (g_moby_vu1_pass) moby_pass();
    else TODO_PORT(0x00250038, "mobys (VU1): the host's model drawer instead")();
    packet_mark(0x02080000);
    texture_block_begin(1);
    mrp_fx_window(true);  // (host: the trails' / the canopy's effects noted for the MRP, as render_frame's draw lists)
    int32_t level = lw(0x0015ed84);
    if (level != 0 && (level != 1 || lbu(0x0013dd43) != 0)) ship_trails_draw(lwu(OV(0x0016c9d8)));
    if (lw(0x0013e088) == 4) {
        int32_t a = lw(OV(0x0016c894));
        if (!(a < 0x3d)) {
            a = (a - 0x3c) * 2;
            if (!(a < 0x81)) a = 0x80;
            flight_banner(a);
        }
    }
    level = lw(0x0015ed84);
    if (level != 0 && (level != 1 || lbu(0x0013dd43) != 0)) ship_canopy_draw(lwu(OV(0x0016c9d8)));
    mrp_fx_window(false);
    constexpr uint32_t SAVE = 0x0013d290;
    if (!(lw(SAVE + 0xd4) < 3) || !(lw(SAVE + 0xdc) < 0)) {  // a save running: the memory card's icon and the spinner
        sub_002a0ba8(0x47, 0x3004b);
        const uint64_t card = effect_tex0(2);
        glyph_sprite(0x2c, lw(0x0013e504) - 0x60, 0x40, 0x40, 0, 0, 0x40, 0x40, 0x0000000080808080ull, card);
        const int32_t t = lw(OV(0x0015f3f8));
        const float y = static_cast<float>((lw(0x0013e504) - 0x40) << 4);
        const float ang = fpu_div(static_cast<float>(t % 0x37) * -6.2831855f, 55.0f);  // 0xc0c90fdb, 0x425c0000
        const uint64_t spin = effect_tex0(3);
        sprite_rotated(0x40, 0x40, spin, 1216.0f, y, 272.0f, 272.0f, ang);  // 0x44980000, 0x43880000
    }
    texture_block_end();
    if (0.0f < lwc1(OV(0x0015f3fc))) {  // the fade
        if (1.0f < lwc1(OV(0x0015f3fc))) swc1(OV(0x00166c00u - 0x7804), 1.0f);
        sub_001fa050(0, 0, 0, float_to_int(lwc1(OV(0x0015f3fc)) * 128.0f));
    }
    dma_wait(0x10);
    if (g_moby_vu1_pass) moby_texture_slots();
    else TODO_PORT(0x0024fad0, "moby end: with the original moby pass only")();
}

}  // namespace rac1
