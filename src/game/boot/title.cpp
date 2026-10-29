// The title screen's loop, update and drawing: the boot module's own code (its own
// image, see front_image.h) ported at its own addresses (`// orig: front:0x...`); the engine code
// it shares with the levels is the ports' (src/game/ovl) relocated onto the boot module. The
// addresses below are the boot module's.
#include <game/boot/title.h>

#include <cstdlib>
#include <game/boot/title_menu.h>
#include <game/boot/title_sky.h>
#include <game/lib/libc.h>
#include <game/lib/libvu0.h>
#include <game/lib/snd989.h>
#include <game/math/vecmath.h>
#include <game/ovl/arrival.h>
#include <game/ovl/level_change.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/particles_draw.h>
#include <game/ovl/screen_fade.h>
#include <game/ovl/sky_ee.h>
#include <game/ovl/world_cull.h>
#include <game/ovl/world_lighting.h>
#include <game/port_stubs.h>
#include <game/render/mrp_title_view.h>
#include <game/render/render_toggles.h>

#include <game/ovl/ovl_reloc.h>
namespace rac1 {

int32_t g_title_attract = 0;
int32_t g_title_idle = 0;

namespace {
constexpr uint32_t TOC = 0x00137b80;
constexpr FrConst<0x0018cb20u> SCENE{};   // the scene (cutscene) state (the overlays' 0x16c860)
constexpr FrConst<0x0015f604u> MODE{};    // the game mode (the overlays' 0x15f5c4)
constexpr uint32_t SLOTS = 0x0013e550;   // the sound slots (0x70 each)
}  // namespace

// Whether the sound slot `handle` still plays the title's sound `snd` (its id +0x7e is the sound
// index plus the base 0x15f5b4, its state +0x74 1 or 2).
// orig: front:0x001eb740
int32_t title_sound_alive(int32_t handle, int32_t snd) {
    if (handle < 0) return 0;
    const uint32_t r = static_cast<uint32_t>(handle) * 0x70 + SLOTS;
    if (lh(r + 0x7e) != snd + lw(FR(0x0015f5b4))) return 0;
    if (static_cast<uint32_t>(lbu(r + 0x74) - 1) < 2) return 1;
    return 0;
}

// The title's sound `idx` (table *(0x15f634), 0x20 each, *(0x15f630) of them) played (the
// overlays' 0x28d970); its slot remembers the sound and `a2`. The slot, -1 when none.
// orig: front:0x0022dba0
int32_t title_sound_play(int32_t idx, uint32_t a1, uint32_t a2) {
    const int32_t s0 = idx + lw(FR(0x0015f5b4));
    if (!(s0 < lw(FR(0x0015f630)))) return -1;
    const int32_t slot = sound_start(lwu(FR(0x0015f634)) + static_cast<uint32_t>(s0) * 32, a1, a2, 0, 0x400);
    if (slot >= 0) {
        const uint32_t r = static_cast<uint32_t>(slot) * 0x70 + SLOTS;
        sh(r + 0x7e, static_cast<uint32_t>(s0));
        sw(r + 0x88, a2);
    }
    return slot;
}

// The title's timed sounds (its music): the 16-byte rows of 0x1862b0 (the scene's first and last
// frame, the sound, its slot) until a first frame -1. A row with a last frame plays its sound
// again whenever it ended while the scene is in its range, and stops it outside; a row without
// (-1) plays once from its first frame.
// orig: front:0x001eb798 (its timed sounds, each pass of its loop)
void title_sounds_timed() {
    if (lw(FR(0x001862b0)) == -1) return;
    uint32_t s0 = FR(0x001862bc);  // the row's slot (+0xc)
    for (;;) {
        const int32_t end = lw(s0 - 8);
        bool play = false;
        if (end == -1) {
            if (lw(SCENE + 0x34) < lw(s0 - 0xc)) {
                sw(s0, 0xffffffffu);
            } else if (lw(s0) == -1) {
                play = true;
            }
        } else {
            const int32_t frame = lw(SCENE + 0x34);
            if (frame < lw(s0 - 0xc) || end < frame) {
                if (title_sound_alive(lw(s0), lw(s0 - 4)) == 0) {
                    sw(s0, 0xffffffffu);
                } else {
                    sub_0028d918(lw(s0));
                    sw(s0, 0xffffffffu);
                }
            } else if (title_sound_alive(lw(s0), lw(s0 - 4)) == 0) {
                play = true;
            }
        }
        if (play) sw(s0, static_cast<uint32_t>(title_sound_play(lw(s0 - 4), 0, 0)));
        s0 += 0x10;
        if (lw(s0 - 0xc) == -1) break;
    }
}

// The view from the scene's camera key of the frame (*(0x18cb74) + frame * 0x20: its position,
// the rotations about x, y, z; the field of view's tangent 0.63): the camera's position
// 0x187080 and the view matrix's rotation rows at 0x187290.. (negated).
// orig: front:0x001eaf88
void title_camera() {
    const uint32_t k = lwu(SCENE + 0x54) + static_cast<uint32_t>(lw(SCENE + 0x38)) * 0x20;
    swc1(FR(0x0018cdb0), u2f(0x3f2147aeu));
    projection_setup();
    lq_sq(FR(0x00187080), k);
    const uint32_t m = ee_stack_alloc(0x40);
    vu0_unit_matrix(M(m));
    vu0_rot_matrix_x(M(m), M(m), lwc1(k + 0x10));
    vu0_rot_matrix_y(M(m), M(m), lwc1(k + 0x14));
    vu0_rot_matrix_z(M(m), M(m), lwc1(k + 0x18));
    const uint32_t C = FR(0x00186f40u);
    swc1(C + 0x350, -lwc1(m + 0x20));
    swc1(C + 0x378, lwc1(m + 0x18));
    swc1(C + 0x360, -lwc1(m + 0x0));
    swc1(C + 0x370, lwc1(m + 0x10));
    swc1(C + 0x354, -lwc1(m + 0x24));
    swc1(C + 0x364, -lwc1(m + 0x4));
    swc1(C + 0x374, lwc1(m + 0x14));
    swc1(C + 0x358, -lwc1(m + 0x28));
    swc1(C + 0x368, -lwc1(m + 0x8));
    ee_stack_free(0x40);
}

// The title's sky: the sky's start (the overlays' 0x28b680: its packet tag, the per-frame
// texture cache cleared), the boot module's own sky routine (its shells and sprites into the
// frame's chain: title_sky.cpp), the sky's end (the overlays' 0x28b710), then the GS registers
// 0x47 / 0x4e of the next primitives. (The levels' 0x23d968 with the boot module's variant.)
// orig: front:0x001e9ab8
void title_sky() {
    ovl_28b680_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);       // (front: 0x22b4c8)
    front_sky_shells_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);  // (front: 0x22ae70)
    ovl_28b710_tr(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0);       // (front: 0x22b558)
    sub_002a0ba8(0x47, 0x5360b);
    // (or a1, v0, a1: the sign-extended arithmetic shift of the word, ORed with 0x01000000)
    sub_002a0ba8(0x4e, 0x01000000ull | static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lw(0x0015ee88)) >> 13)));
}

// The title's update: the fade out of black, the scene's frame (and its next block every 0x60
// frames, the first again at its end), the camera, the scene's mobys posed (their keyframe pair
// of the frame, the transform), then by game mode: 0 the logo's alpha (up to 0x40 after a
// second) and the pulsing "press start" (0x60 + 32 cos, after two), Start or Cross the main menu;
// 3 the alphas down, the menu; 4 the message screens.
// orig: front:0x001eb0a8
void title_update() {
    // (0x1e9430: an empty function)
    float f = lwc1(FR(0x0015f43c)) - 0.0625f;
    const int32_t blk_frame = lw(SCENE + 0x38) + 1;
    const int32_t frame = lw(SCENE + 0x34) + 1;
    sw(SCENE + 0x38, static_cast<uint32_t>(blk_frame));
    sw(SCENE + 0x34, static_cast<uint32_t>(frame));
    swc1(FR(0x0015f43c), f);
    if (f < 0.0f) swc1(FR(0x0015f43c), 0.0f);
    if (!(frame < lh(SCENE + 0x40))) {
        sw(SCENE + 0x3c, 0);
        sw(SCENE + 0x34, 0);
        scene_block_load(0);
    } else if (!(blk_frame < 0x60)) {
        const int32_t b = lw(SCENE + 0x3c) + 1;
        sw(SCENE + 0x3c, static_cast<uint32_t>(b));
        scene_block_load(b);
    }
    title_camera();
    for (int32_t i = 0; i < lh(SCENE + 0x44); ++i) {
        const uint32_t m = lwu(SCENE + 0x178 + static_cast<uint32_t>(i) * 4);
        const int32_t h = lw(SCENE + 0x38) >> 1;
        sb(m + 0x50, static_cast<uint32_t>(h));
        sb(m + 0x51, static_cast<uint32_t>(h + 1));
        sub_0024ebc0(m);
        const float t = int_to_float(lw(SCENE + 0x38) & 1) * 0.5f;
        const uint32_t keys = lwu(m + 0x78);
        swc1(m + 0x54, t);
        const uint32_t a = ee_stack_alloc(0x20);
        vec3_scale(V(a), V(keys + lbu(m + 0x50) * 16u), 1.0f - t);
        vec3_scale(V(a + 0x10), V(keys + lbu(m + 0x51) * 16u), lwc1(m + 0x54));
        vec3_add(V(m + 0x10), V(a), V(a + 0x10));
        ee_stack_free(0x20);
        sb(m + 0x71, 0xff);
        moby_update_transform(m);
        sb(m + 0x7f, 0);
        // (lh(m + 0xa6) == 0: 0x1e9410, an empty function)
    }
    // (0x1e9428: an empty function)
    const int32_t mode = lw(MODE);
    if (mode == 0) {
        sw(FR(0x0015ef58), static_cast<uint32_t>(lw(FR(0x0015ef58)) + 1));
        if (frame_scale_i(0x3c) < lw(FR(0x0015ef58))) {
            int32_t al = lw(FR(0x0015ef50)) + 1;
            sw(FR(0x0015ef50), static_cast<uint32_t>(al));
            if (!(al < 0x41)) sw(FR(0x0015ef50), 0x40);
        }
        if (frame_scale_i(0x78) < lw(FR(0x0015ef58))) {
            const int32_t t0 = frame_scale_i(0x78);
            const int32_t k = ee_rem(lw(FR(0x0015ef58)) - t0, 0x3c);
            const float c = cos_vu(int_to_float(k) * u2f(0x3dd67750u) + u2f(0xc0490fdbu));
            sw(FR(0x0015ef54), static_cast<uint32_t>(float_to_int(c * 32.0f) + 0x60));
        }
        if ((lwu(0x0013c940 + 0x1a4) & 0x840) != 0) title_menu_open();
        sound_frame();
    } else if (mode == 3) {
        sw(FR(0x0015ef58), static_cast<uint32_t>(frame_scale_i(0x3c)));
        int32_t a = lw(FR(0x0015ef50)) - 0x10;
        sw(FR(0x0015ef50), static_cast<uint32_t>(a));
        if (a < 0) sw(FR(0x0015ef50), 0);
        a = lw(FR(0x0015ef54)) - 0x10;
        sw(FR(0x0015ef54), static_cast<uint32_t>(a));
        if (a < 0) sw(FR(0x0015ef54), 0);
        title_menu_update();
        sound_frame();
    } else if (mode == 4) {
        screen_update();
        sound_frame();
    }
}

// The title's frame drawn: the sky's clear, the camera, the moby animation lerp slots, the
// static packets, the sky, the scene's passes (tfrag, ties, shrubs: the levels' passes), the
// menu (mode 3) or the scene's mobys, the particles, the logo (256 x 128 at 0xec, 0x10) and
// "press start" (the HUD's texture 4 + language - 1, 192 x 96), the fade, the message screens
// (mode 4), then the passes' lighting and ends, and the fog.
// orig: front:0x001eb410
void title_draw() {
    const uint32_t sky = lwu(FR(0x0016045c));
    if (sky == 0 || lh(sky + 4) != 0) sub_00200fd8();
    mem_fill32(FR(0x00193fc0), 0xffffffffu, 0x80);
    title_camera();
    render_view_setup();
    moby_anim_lerp_slots_validate();  // (front: 0x20cc60)
    render_static_packets();
    sw(FR(0x0015f620), 0xffffffffu);
    // the passes (the same code as the levels'; world_pass: their chain kept for the faithful / MRP world and noted for the MRP,
    // else taken back and the host's fallback draws the scene, title_scene_host; the sky only when its chain is drawn, as in
    // render_frame)
    if (lwu(FR(0x0016045c)) != 0) {
        if (sky_wanted()) world_pass(title_sky, -1);
        else TODO_PORT(0x001e9ab8, "front: the title's sky (its chain not drawn: the host's sky draws instead)")();
    }
    world_pass(world_tfrag_pass, 0);  // (front: 0x2333a8)
    packet_mark(0x02010000);
    world_pass(world_tie_pass, 1);  // (front: 0x2358c8)
    packet_mark(0x02020000);
    world_pass(world_shrub_pass, 2);  // (front: 0x228b38)
    packet_mark(0x02040000);
    mrp_title_view_note();  // (host: the passes' view, before the menu's draw sets its own)
    mrp_title_view_note_menu(lw(MODE) == 3);  // (host: the menu's mobys drawn in the game's 4:3 projection)
    if (lw(MODE) == 3) {
        title_menu_draw();
    } else if (g_moby_vu1_pass) {
        moby_pass();  // (the boot module's 0x20d460: the overlays' 0x250038)
    } else {
        TODO_PORT(0x0020d460, "front: mobys (VU1): the host's model drawer instead")();
    }
    packet_mark(0x02080000);
    texture_block_begin(0);
    packet_ref_1c24f0();
    if (lw(FR(0x0015f464)) != 0) draw_list_d880();
    packet_ref_1c24f0();
    if (lw(FR(0x0018a2e8)) != 0) {
        sub_002a0ba8(8, 5);
        packet_ref_1c24f0();
        // (FlushCache: no memory effect)
        particles_draw();
        sw(FR(0x0015f620), 8);
    }
    packet_ref_151900();
    render_static_packets();
    if (lw(FR(0x0015ef50)) != 0) {  // the logo
        const uint64_t colour = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lwu(FR(0x0015ef50)) << 24 | 0x00808080u)));
        glyph_sprite(0xec, 0x10, 0x100, 0x80, 0, 0, 0x100, 0x80, colour, ld(FR(0x0015ef48)));
    }
    if (lw(FR(0x0015ef54)) != 0) {  // "press start"
        int32_t lang = lw(0x0015ed88) - 1;
        if (!(-1 < lang)) lang = 0;
        const uint64_t tex = effect_tex0(lang + 4);
        const uint64_t colour = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>(lwu(FR(0x0015ef54)) << 24 | 0x00808080u)));
        glyph_sprite(0xa0, lw(0x0013e504) - 0x50, 0xc0, 0x60, 0, 0, 0x100, 0x80, colour, tex);
    }
    texture_block_end();
    if (0.0f < lwc1(FR(0x0015f43c))) {  // the fade
        if (1.0f < lwc1(FR(0x0015f43c))) swc1(FR(0x0015f43c), 1.0f);
        sub_001fa050(0, 0, 0, float_to_int(lwc1(FR(0x0015f43c)) * 128.0f));
    }
    vif0_send(0x00100ae0);  // the VU0 micro programs
    // (FlushCache: no memory effect)
    if (lw(MODE) == 4) screen_draw();
    dma_wait(2);
#if defined(__WIIU__)
    static const bool tfrag_off = std::getenv("RAC1_TFRAG_PASS_OFF") != nullptr && std::atoi(std::getenv("RAC1_TFRAG_PASS_OFF")) != 0;
    static const unsigned long wp_skip = std::getenv("RAC1_WORLD_PASS_SKIP") != nullptr ? std::strtoul(std::getenv("RAC1_WORLD_PASS_SKIP"), nullptr, 16) : 0ul;
    if (!tfrag_off) {
        world_tfrag_lighting(FR(0x001e1300u));  // (front: 0x234f98; the tfrag pass's list)
        world_tfrag_end();                      // (front: 0x233308)
    }
    dma_wait(4);
    if ((wp_skip & 2ul) == 0) {
        world_tie_lighting(FR(0x001e3200u));  // (front: 0x237370; the tie pass's list)
        world_tie_end();                      // (front: 0x235780)
    }
    dma_wait(8);
    if ((wp_skip & 4ul) == 0) {
        world_shrub_lighting(FR(0x001d8eb0u));  // (front: 0x22a5e0, the overlays' 0x288908; the shrub pass's list)
        world_shrub_end();                      // (front: 0x228a30)
    }
#else
    world_tfrag_lighting(FR(0x001e1300u));  // (front: 0x234f98; the tfrag pass's list)
    world_tfrag_end();                  // (front: 0x233308)
    dma_wait(4);
    world_tie_lighting(FR(0x001e3200u));  // (front: 0x237370; the tie pass's list)
    world_tie_end();                  // (front: 0x235780)
    dma_wait(8);
    world_shrub_lighting(FR(0x001d8eb0u));  // (front: 0x22a5e0, the overlays' 0x288908; the shrub pass's list)
    world_shrub_end();                  // (front: 0x228a30)
#endif
    dma_wait(0x10);
    if (g_moby_vu1_pass) moby_texture_slots();  // (its group list is the moby pass's: with the pass only)
    else TODO_PORT(0x0020cef8, "front: moby end: with the original moby pass only")();
    fog_setup();
}

// The main menu opened: the full-screen menu's block (0x1d5bf0) on the root page 0x2d, game
// mode 3.
// orig: front:0x00218f68
void title_menu_open() {
    sw(FR(0x001d5bf0), 0x2d);
    sw(FR(0x001d5bf0) + 0x110, 0);
    sw(MODE, 3);
    sw(FR(0x001d5bf0) + 0xc, 0);
    sw(FR(0x001d5bf0) + 0x10, 0);
}

// The title loop's start: the level 0 asked, the timer, a new game's state, the title's level,
// six files after the level data (+0x1500.. +0x1524, +0x14f8: their offsets and sizes in the
// block *(0x1940c8)), the HUD and its data, then a wait of three seconds less the time the start
// took (timer 0 in H-blanks, 0x109 a frame). The frames to wait returned.
// orig: front:0x001eb798 (its start)
int32_t title_loop_start() {
    sw(0x0015ed84, 0);
    TODO_PORT(0x10000010, "front: timer 0 started (T0_MODE 0x83, T0_COUNT 0: H-blanks)")();
    new_game_state();
    title_load();
    const uint32_t s1 = lwu(FR(0x001940c8));
    uint32_t s0 = s1 + 0x60;
    sw(0x0015ee4c, s1);
    const struct { uint32_t row, at; } files[5] = {{0x1500, 0x28}, {0x1508, 0x30}, {0x1510, 0x38}, {0x1518, 0x40}, {0x1520, 0x48}};
    for (const auto& f : files) {
        const int32_t n = disc_read_wait(s0, lwu(TOC + f.row), lwu(TOC + f.row + 4));
        sw(s1 + f.at + 4, static_cast<uint32_t>(n));
        sw(s1 + f.at, s0 - s1);
        s0 += (static_cast<uint32_t>(n) + 0xf) & 0xfffffff0u;
    }
    sw(s1 + 0x24, static_cast<uint32_t>(disc_read_wait(s0, lwu(TOC + 0x14f8), lwu(TOC + 0x14fc))));
    sw(s1 + 0x20, s0 - s1);
    hud_alloc_reset();
    TODO_PORT(0x00232ce0, "front: the overlays' 0x293af8")();
    stash_init();
    hud_init();
    hud_data_load();
    const int32_t elapsed = 0;  // (timer 0 / 0x109: the host's loads take no time)
    return frame_scale_i(0xb4) - elapsed;
}

// After the wait: 989snd's batch sent and flushed, the first attract movie chosen at random.
// orig: front:0x001eb798 (after its wait)
void title_loop_ready() {
    snd_release();
    while (snd_flush() != 0) {
    }
    g_title_attract = libc_rand() % 4;
    g_title_idle = 0;
    snd_hold();
}

// One pass of the title loop: the frame's packets, the memory card's machines, the pad, the texts
// of the language, the timed sounds, the update and the drawing; in game mode 0 after 0x5dc
// frames without a menu an attract movie (1 returned: the host plays it, then
// title_attract_after()); a switch of the video mode (0x16034c against 0x15ed80); the DMA wait,
// the vsync, the frame counter. 2 once the loop is left (0x15f5b0), else 0.
// orig: front:0x001eb798 (one pass of its loop)
int32_t title_loop_frame() {
    if (lw(FR(0x0015f5b0)) != 0) return 2;
    TODO_PORT(0x002336a0, "front: DMA: packet kick (the overlays' 0x2a08c8)")();
    sub_002a0858();
    sub_00201040();
    packet_call_151b60();
    packet_call_display();
    card_machine();
    card_ui_machine();
    pad_update();
    title_texts(lw(0x0015ed88));
    sw(FR(0x0015f618), 0);
    title_sounds_timed();
    title_update();
    title_draw();
    if (lw(MODE) == 0) {
        ++g_title_idle;
        if (!(g_title_idle < frame_scale_i(0x5dc))) {
            g_title_idle = 0;
            return 1;
        }
    } else {
        g_title_idle = 0;
    }
    if (lbu(FR(0x0016034c)) != lwu(0x0015ed80)) {  // the video mode switched (the options' NTSC / PAL)
        const uint32_t pal = lwu(0x0015ed80);
        sw(0x0015ed80, pal == 0 ? 1u : 0u);
        screen_fade(4);
        TODO_PORT(0x001204b8, "libgraph: sceGsResetPath")();
        TODO_PORT(0x00121878, "libgraph: sceGsResetGraph")(0, 1, lwu(0x0015ed80) == 0 ? 2 : 3, 0);
        const uint32_t keep = lwu(0x0015ee78);
        swc1(FR(0x0015f43c), 1.0f);
        TODO_PORT(0x001f34e8, "front: the GS start (the overlays' 0x1f80d0)")();
        sw(0x0015ee78, keep);
    }
    dma_wait(1);
    TODO_PORT(0x00122298, "kernel: vsync wait")(0);
    sw(FR(0x0015f438), lwu(FR(0x0015f438)) + 1);
    return lw(FR(0x0015f5b0)) != 0 ? 2 : 0;
}

// After an attract movie (0x1e9488): the next one in turn, the fade, the scene from its start,
// the logo and "press start" hidden again.
// orig: front:0x001eb798 (after its attract movie)
void title_attract_after() {
    g_title_attract = (g_title_attract + 1) % 4;
    swc1(FR(0x0015f43c), 1.0f);
    sw(SCENE + 0x34, 0);
    sw(SCENE + 0x3c, 0);
    scene_block_load(0);
    sw(FR(0x0015ef50), 0);
    sw(FR(0x0015ef54), 0);
    sw(FR(0x0015ef58), 0);
    sub_002a07f8();
}

// The loop's end: NTSC's texture base 0x280000, the frame's uploads from it.
// orig: front:0x001eb798 (its end)
void title_loop_end() {
    if (lw(0x0015ed80) == 0) sw(0x0015ee8c, 0x00280000u);
    sw(0x0015ee74, lwu(0x0015ee8c));
    sw(0x0015ee78, lwu(0x0015ee8c));
}

}  // namespace rac1
