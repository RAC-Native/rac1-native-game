// Ported overlay code: the overlay's entry 0x245c28 (the game's start on a level and its main
// loop), the per-frame helpers it calls and the render view setup 0x1f6e48.
// Callees not ported (or not yet proven with the oracle) are TODO_PORT. The hardware the loop
// waits on (the RCNT1 counter, vsync, DMA kicks) is the host's: see host_rcnt1().
#include <chrono>
#include <utility>
#include <vector>
#include <game/ovl/credits_tr.h>
#include <game/ovl/screen_flash.h>
#include <game/ovl/memory_card.h>
#include <game/ovl/ovl_reloc.h>
#include <game/input/pad_build.h>
#include <game/math/vecmath.h>
#include <game/lib/snd989.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/ovl.h>
#include <game/ovl/music_streams.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/planet_map.h>
#include <game/ovl/movie_mode.h>
#include <game/render/mrp_fx.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_world_scene.h>
#if defined(__WIIU__)
#include <game/render/mrp_world_renderer.h>
#endif
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/pause_menu_pages.h>
#include <game/ovl/particles_draw.h>
#include <game/ovl/shadows.h>
#include <game/ovl/sky_levels.h>
#include <game/ovl/world_cull.h>
#include <game/ovl/world_lighting.h>
#include <game/ovl/sparkles_draw.h>
#include <game/ovl/quad_draw.h>
#include <game/ovl/screen_effects.h>
#include <game/ovl/vendor_tr.h>
#include <game/enhance/vendor_buy_all.h>
#include <game/port_stubs.h>
#include <runtime/kernel/bootstrap.h>
#include <runtime/ee/ee_thread.h>
#include <runtime/ee/ee_timer.h>
#include <profiler/host_profiler.h>

namespace rac1 {

uint8_t g_host_pad_packet[6] = {0xff, 0xff, 0x7f, 0x7f, 0x7f, 0x7f};

// The EE timer T1 the loop reads (BUSCLK / 256 since the frame's start: 0x2580 an NTSC frame): the host's
// model of an ideal PS2 whose EE takes no time (runtime/ee/ee_timer.h): it moves 0x2580 at each
// vsync only. (A constant 0x2580 would read every frame's work as a whole frame, load 1.0, and the
// game would shed particles, lights and explosion effects as a PS2 at its limit.)
// orig: (host) REG_T1_COUNT (0x10000800)
static uint32_t host_rcnt1() { return ee_t1_count(); }

// orig: 0x001f6da0 — profiler marker (empty in this build).
void prof_mark_end(uint32_t, int32_t) {}
// orig: 0x001f6d90 — debug hook (empty in this build).
void debug_hook_1f6d90() {}

// orig: 0x001f6d70 — the per-frame counters: last frame's 0x169fe4 kept in 0x169fe8.
void frame_counters_reset() {
    sw(OV(0x00169fe8), lwu(OV(0x00169fe4)));
    sw(OV(0x00169fe4), 0);
    sw(OV(0x00169e30), 0);
}

// orig: 0x00265860 — the music volumes 0x15172c / 0x151710 / 0x151748 to 4.
void music_volumes_default() {
    sh(0x0015172c, 4);
    sh(0x00151710, 4);
    sh(0x00151748, 4);
}

// orig: 0x00284c50 — the autosave tick 0x15ee24 (every 50 frames on PAL it skips ahead by 10).
void autosave_tick() {
    if (lw(OV(0x0015f5ac)) != 0) return;
    const int32_t t = lw(0x0015ee24);
    sw(0x0015ee24, static_cast<uint32_t>(t + 1));
    if (ee_rem(t + 1, 0x32) == 0 && lw(0x0015ed80) != 0) sw(0x0015ee24, static_cast<uint32_t>(t + 0xb));
}

// orig: 0x001f3638 — the 20 words 0x169ff0..0x16a03c to 1.
void render_enables_init() {
    for (uint32_t a = OV(0x0016a03c); a >= OV(0x00169ff0); a -= 4) sw(a, 1);
}

// orig: 0x001f36f8 — the frame's debug strings (0x16a840, 0x10 each, count 0x15efc4) drawn and
// the list emptied.
void debug_strings_flush() {
    for (int32_t i = 0; i < lw(OV(0x0015efc4)); ++i) {
        const uint32_t s = OV(0x0016a840) + static_cast<uint32_t>(i) * 0x10;
        TODO_PORT(0x001f3770, "debug string draw (GS packets)")(lwu(s), lwu(s + 4), lwu(s + 8), lwu(s + 0xc));
    }
    sw(OV(0x0015efc4), 0);
    sw(OV(0x0015efc0), OV(0x0016a040));
}

// orig: 0x00201350 — a DMA tag in the packet *(0x1611c0): call 0x151b60, 0x29 quadwords.
void packet_call_151b60() {
    const uint32_t p = lwu(OV(0x001611c0));
    sw(p, 0x30000029);
    sw(p + 4, 0x00151b60);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000029);
    sw(OV(0x001611c0), p + 0x10);
}

// orig: 0x00200f40 — a DMA tag in the packet *(0x1611c0) (or, without a packet, a kernel send):
// call *(0x15eeb8) + 0x30, 9 quadwords.
void packet_call_display() {
    const uint32_t p = lwu(OV(0x001611c0));
    if (p == 0) {
        TODO_PORT(0x001221b0, "kernel DMA send")(lwu(0x0015eeb8) + 0x30);
        return;
    }
    sw(p, 0x30000009);
    sw(p + 4, (lwu(0x0015eeb8) + 0x30) & 0x0fffffffu);
    sw(p + 8, 0);
    sw(p + 0xc, 0x50000009);
    sw(OV(0x001611c0), p + 0x10);
}

// orig: 0x0028eed0 — the start mode: gameplay (0) on Veldin, the title levels and a first visit
// of Novalis; otherwise the planet arrival (6) with the level's GS memory set up.
void start_mode_select() {
    sh(0x0013e05c, 1);
    if (lh(0x0013e05a) != 0) sh(0x0013e05a, 0);
    const int32_t lvl = lw(0x0015ed84);
    if (lvl == 1 && lbu(0x0013dd43) == 0) {
        sw(OV(0x0015f5c4), 0);
    } else if (lvl == 0 || (lvl == 0xe && lbu(0x0013d3f0) == 0) || 0x13 < lvl) {
        sw(OV(0x0015f5c4), 0);
    } else {
        sh(0x0013e054, 0xffff);
        sw(OV(0x0015f5c4), 6);
        sw(OV(0x0015f3fc), 0x3f800000);
        sw(0x0013e050, 0);
        sub_002a07c8();
        const uint32_t base = lwu(OV(0x001611cc)) - 0x60000u;
        sw(OV(0x001611cc), base);
        sw(OV(0x0016c8bc), lwu(OV(0x00173e08)) + base);
        sw(OV(0x0016c8b8), lwu(OV(0x00173e04)) + base);
        ship_sequence_start(8);
    }
}

// The view matrices for the frame (the debug view 0x16c06c builds the rows from the Euler angles
// 0x166dd0 instead of the camera's rows 0x166fd0..): 0x166c80 the world-to-view rotation (rows
// -left, -up, forward as its columns; no translation), 0x166cc0 / 0x166d00 / 0x166d40 /
// 0x166d80 that rotation times the projection / clip matrices 0x16cb00.. (0x166d00 with the
// offset 0x16cbe0 times its w column), 0x166d80..0x166db0 the scaled 0x16cb80 / 0x16cb90 rows,
// 0x16ccc0.. the rotation's three rows, 0x16ccf0 the camera position 0x166dc0 times 1024.
// orig: 0x001f6e48
void render_view_setup() {
    const MrpLodSetupScope shadow_patch(true);  // (host: inside a widened shadow run, the game's own rows A, mrp_lod.h)
    Vec4 r0{}, r1{}, r2{};
    if (lw(OV(0x0016c06c)) == 0) {
        r0 = V(OV(0x00166fd0));
        r1 = V(OV(0x00166fe0));
        r2 = V(OV(0x00166ff0));
    } else {
        TODO_PORT(0x00125218, "debug view rows (libvu0 unit / rotate x y z)")(lwc1(OV(0x00166dd0)), lwc1(OV(0x00166dd4)), lwc1(OV(0x00166dd8)));
    }
    const uint32_t m = OV(0x00166c80);
    swc1(m, -r1.x);
    swc1(m + 0x3c, 1.0f);
    sw(m + 0x30, 0);
    swc1(m + 0x10, -r1.y);
    swc1(m + 0x20, -r1.z);
    swc1(m + 0x04, -r2.x);
    swc1(m + 0x14, -r2.y);
    swc1(m + 0x24, -r2.z);
    swc1(m + 0x08, r0.x);
    swc1(m + 0x18, r0.y);
    swc1(m + 0x28, r0.z);
    sw(m + 0x34, 0);
    sw(m + 0x38, 0);
    sw(m + 0x0c, 0);
    sw(m + 0x1c, 0);
    sw(m + 0x2c, 0);
    mat4_mul(M(m + 0x40), M(OV(0x0016cb00)), M(m));
    mat4_mul(M(m + 0x80), M(OV(0x0016cb40)), M(m));
    const float ox = lwc1(OV(0x0016cbe0)), oy = lwc1(OV(0x0016cbe4)), oz = lwc1(OV(0x0016cbe8));
    for (uint32_t r = 0; r < 4; ++r) {
        const uint32_t row = m + 0x80 + r * 0x10;
        const float w = lwc1(row + 0xc);
        swc1(row, fpu_add(lwc1(row), fpu_mul(w, ox)));
        swc1(row + 4, fpu_add(lwc1(row + 4), fpu_mul(w, oy)));
        swc1(row + 8, fpu_add(lwc1(row + 8), fpu_mul(w, oz)));
    }
    mat4_mul(M(m + 0xc0), M(OV(0x0016cb80)), M(m));
    vec4_scale(V(m + 0x100), V(OV(0x0016cb80)), lwc1(OV(0x0016cc00)));
    vec4_scale(V(m + 0x110), V(OV(0x0016cb90)), lwc1(OV(0x0016cc00)));
    lq_sq(m + 0x120, OV(0x0016cba0));
    lq_sq(m + 0x130, OV(0x0016cbb0));
    mat4_mul(M(m + 0x100), M(m + 0x100), M(m));
    lq_sq(OV(0x0016ccc0), m);
    lq_sq(OV(0x0016ccd0), m + 0x10);
    lq_sq(OV(0x0016cce0), m + 0x20);
    vec3_scale(V(OV(0x0016ccf0)), V(OV(0x00166dc0)), 1024.0f);
    swc1(OV(0x0016ccfc), 1024.0f);
}

// The visible zone list of a grid cell (x, y, z in 4-unit cells) from the level's zone grid
// *(0x15f600): z rows, then y rows, then x cells (each row: first index, count, then halfword
// offsets / 4); the cell's entry 0xffff is none, else the 0x80-byte zone mask at +*(grid) +
// entry * 0x80. 0 when the cell is outside the grid.
// orig: 0x001f7278
uint32_t zone_grid_cell(int32_t x, int32_t y, int32_t z) {
    const uint32_t g = lwu(OV(0x0015f600));
    const uint32_t masks = g + lwu(g);
    z -= static_cast<int32_t>(lhu(g + 4));
    if (z < 0 || !(z < static_cast<int32_t>(lhu(g + 6)))) return 0;
    const uint32_t zr = lhu(g + 8 + static_cast<uint32_t>(z) * 2);
    if (zr == 0) return 0;
    const uint32_t yr = g + zr * 4;
    y -= static_cast<int32_t>(lhu(yr));
    if (y < 0 || !(y < static_cast<int32_t>(lhu(yr + 2)))) return 0;
    const uint32_t yo = lhu(yr + 4 + static_cast<uint32_t>(y) * 2);
    if (yo == 0) return 0;
    const uint32_t xr = g + yo * 4;
    x -= static_cast<int32_t>(lhu(xr));
    if (x < 0 || !(x < static_cast<int32_t>(lhu(xr + 2)))) return 0;
    const uint32_t e = lhu(xr + 4 + static_cast<uint32_t>(x) * 2);
    return e == 0xffff ? 0u : masks + e * 0x80;
}

// The nearer of two neighbour cells first (the fraction of the camera in its cell, under 0.5:
// the lower one), then the other.
// orig: 0x001f7350
uint32_t zone_grid_neighbour(float frac, int32_t x0, int32_t y0, int32_t z0, int32_t x1, int32_t y1, int32_t z1) {
    uint32_t r;
    if (frac < 0.5f) {
        r = zone_grid_cell(x0, y0, z0);
        if (r == 0) r = zone_grid_cell(x1, y1, z1);
    } else {
        r = zone_grid_cell(x1, y1, z1);
        if (r == 0) r = zone_grid_cell(x0, y0, z0);
    }
    return r;
}

// The visible zones 0x173d00 (0x80 bytes of bits; the draw loops keep a moby or a shrub when its
// zone bits meet them) for the camera position 0x166dc0: its grid cell's list, else the union
// of the neighbour cells' lists (0x173d80) unless the mode 0x15f608 is set, else by the mode:
// all zones (1), the last list 0x15f610 (0; all zones with the debug view or without a list),
// the octant of the box *(0x15f604) the camera is in (2). Bit 0x80 of the last byte is always
// set.
// orig: 0x001f7408
void zones_from_camera() {
    const float cx = lwc1(OV(0x00166dc0)) * 0.25f, cy = lwc1(OV(0x00166dc4)) * 0.25f, cz = lwc1(OV(0x00166dc8)) * 0.25f;
    const int32_t x = float_to_int(cx), y = float_to_int(cy), z = float_to_int(cz);
    uint32_t list = zone_grid_cell(x, y, z);
    if (list != 0) {
        sw(OV(0x0015f60c), 0);
        mem_copy128(OV(0x00173d00), list, 0x80);
        sw(OV(0x0015f610), list);
    } else {
        sw(OV(0x0015f60c), 1);
        if (lw(OV(0x0015f608)) == 0) {
            const uint32_t nx = zone_grid_neighbour(lwc1(OV(0x00166dc0)) * 0.25f - int_to_float(x), x - 1, y, z, x + 1, y, z);
            const uint32_t ny = zone_grid_neighbour(lwc1(OV(0x00166dc4)) * 0.25f - int_to_float(y), x, y - 1, z, x, y + 1, z);
            const uint32_t nz = zone_grid_neighbour(lwc1(OV(0x00166dc8)) * 0.25f - int_to_float(z), x, y, z - 1, x, y, z + 1);
            if (nx != 0 || ny != 0 || nz != 0) {
                mem_clear128(OV(0x00173d80), 0x80);
                if (nx != 0) mem_or128(OV(0x00173d80), OV(0x00173d80), nx, 0x80);
                if (ny != 0) mem_or128(OV(0x00173d80), OV(0x00173d80), ny, 0x80);
                if (nz != 0) mem_or128(OV(0x00173d80), OV(0x00173d80), nz, 0x80);
                list = OV(0x00173d80);
                sw(OV(0x0015f610), list);
                mem_copy128(OV(0x00173d00), list, 0x80);
            }
        }
        if (list == 0) {
            const int32_t mode = lw(OV(0x0015f608));
            auto last_or_all = [] {
                const uint32_t last = lwu(OV(0x0015f610));
                if (lw(OV(0x0016c06c)) == 0 && last != 0) {
                    mem_copy128(OV(0x00173d00), last, 0x80);
                } else {
                    mem_fill32(OV(0x00173d00), 0xffffffffu, 0x80);
                }
            };
            if (mode == 1) {
                mem_fill32(OV(0x00173d00), 0xffffffffu, 0x80);
            } else if (mode == 0) {
                last_or_all();
            } else if (mode == 2) {
                const uint32_t box = lwu(OV(0x0015f604));
                if (box != 0) {
                    const uint32_t i = (0.0f < lwc1(OV(0x00166dc0)) - lwc1(box) ? 4u : 0u) + (0.0f < lwc1(OV(0x00166dc4)) - lwc1(box + 4) ? 2u : 0u) +
                                       (0.0f < lwc1(OV(0x00166dc8)) - lwc1(box + 8) ? 1u : 0u);
                    mem_copy128(OV(0x00173d00), box + 0x10 + i * 0x80, 0x80);
                } else {
                    last_or_all();
                }
            }
        }
    }
    sb(OV(0x00173d7f), lbu(OV(0x00173d7f)) | 0x80u);
}

// orig: 0x001f77f8
void zones_update() {
    const int32_t m = lw(OV(0x0016c074));
    if (m == 0) {
        mem_fill32(OV(0x00173d00), 0xffffffffu, 0x80);
    } else if (m == 2) {
        zones_from_camera();
    }
}

// orig: (host) a world pass (tfrag / tie / shrub: 0x294240 / 0x295c80 / 0x286e60) run for what it decides (the visibility lists, the lighting
// lists): the VU1 chain it builds in the frame packet (the world's REF tags, the textures' uploads) is taken back unless RAC1_WORLD_CHAIN=1,
// the host's world renderers draw the world (and the VU1 world programs are not run). With the MRP world the chain is kept
// and its range noted: the MRP decodes it (mrp_world_scene.h), the GS replica skips it. `kind`: 0 tfrag, 1 tie, 2 shrub, -1 the sky.
// orig: (host) testing (the Wii U port): RAC1_WORLD_PASS_SKIP, a mask of the kinds whose culling, and then relighting, is not run (2 tie,
// 4 shrub: the game's own render enable off while they run; not the tfrag, whose packets read the culling's lists). The game state stays
// (600 Veldin frames: hero and camera identical); the instances keep the colours of the level's load (a renderer relights them itself)
static bool world_pass_skipped(int kind) {
    static const char* wp_skip = std::getenv("RAC1_WORLD_PASS_SKIP");
    return wp_skip != nullptr && kind >= 0 && ((std::strtoul(wp_skip, nullptr, 16) >> kind) & 1) != 0;
}

// orig: (host) testing (the Wii U port): RAC1_TFRAG_PASS_OFF=1, the game's tfrag pass not run at all, nor its relighting at the passes'
// end (which reads the pass's lists) - the pass's work is the chain's tfrag packets, which a renderer making the tfrags from the host's
// decisions does not read (the tfrag snapshot is taken all the same); the tfrags keep the colours of the level's load
static bool tfrag_pass_off() {
    static const bool off = std::getenv("RAC1_TFRAG_PASS_OFF") != nullptr && std::atoi(std::getenv("RAC1_TFRAG_PASS_OFF")) != 0;
    return off;
}

// orig: (host) the world passes' chain ranges of the frame (first tag, end), noted while on: for a renderer that draws the
// world itself and has the GS replica skip them (the Wii U's 2D path; the MRP has its own, mrp_world_skip_ranges). The
// taker clears them.
bool g_world_pass_ranges_on = false;
std::vector<std::pair<uint32_t, uint32_t>> g_world_pass_ranges;

void world_pass(void (*pass)(), int kind) {
    RAC1_PROF_ZONE(kGameWorldPasses);
    static const bool keep_env = std::getenv("RAC1_WORLD_CHAIN") != nullptr;
    const bool keep = keep_env || g_world_faithful || g_world_mrp || g_world_chain_forced;  // (RAC1_WORLD_FAITHFUL / the debug menu: the chain is the world's drawing)
    const uint32_t cur = lwu(OV(0x001611c0));
    if (g_world_mrp) mrp_lod_world_shadow(kind >= 0 ? kind : 3, pass, cur);  // (the LOD slider's shadow run first, the memory put back)
#if defined(__WIIU__)
    // (the Wii U's MRP-lite: its ties / shrubs always made by the host from the pass's inputs - mrp_world_static.h, MRP_LITE.md phase 3 -,
    // whose snapshot is taken here, as mrp_lod_world_shadow does on the PC: no LOD slider, no widening; its tfrags' decisions too -
    // mrp_tfrag_gpu.h tfrag_decide, phase 4 -: the game's tfrag pass still runs, its packets read the culling's lists)
    if (kind == 1 || kind == 2) mrpw::static_note_pass(kind, mrp_world_ready(), 1.0f, 1.0f, 1.0f);
    if (kind == 0) mrpw::tfrag_note_pass(mrp_world_ready(), 1.0f, 1.0f);
#endif
    // (testing, the Wii U port: RAC1_WORLD_PASS_TIME, each world pass's own time every 60 frames)
    static const bool wp_time = std::getenv("RAC1_WORLD_PASS_TIME") != nullptr;
    static double wp_ms[4] = {};
    static int wp_n = 0;
    const auto wp_t0 = wp_time ? std::chrono::steady_clock::now() : std::chrono::steady_clock::time_point();
    if (kind == 0 && tfrag_pass_off()) {
    } else if (world_pass_skipped(kind)) {
        // the pass with its culling off (its render enable 0 while it runs: EN + 0x10 / 0x18 / 0x20), then put back
        const uint32_t en = OV(0x00169ff0u) + 0x10 + 8 * static_cast<uint32_t>(kind), was = lwu(en);
        sw(en, 0);
        pass();
        sw(en, was);
    } else {
        pass();
    }
    if (wp_time) {
        wp_ms[kind >= 0 ? kind : 3] += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - wp_t0).count();
        if (kind == 0 && ++wp_n == 60) {
            std::printf("[world-pass] ms/frame: tfrag %.2f tie %.2f shrub %.2f sky %.2f\n", wp_ms[0] / 60, wp_ms[1] / 60, wp_ms[2] / 60, wp_ms[3] / 60);
            wp_n = 0;
            for (double& v : wp_ms) v = 0;
        }
    }
    static const bool sky_only = std::getenv("RAC1_SKY_ONLY") != nullptr;  // (testing: the world's chain taken back: the sky alone)
    if (sky_only && kind >= 0) {
        sw(OV(0x001611c0), cur);
        return;
    }
    if (g_world_mrp) mrpw::scene_note_pass(kind >= 0 ? kind : 3, cur, lwu(OV(0x001611c0)));  // (the sky: 3)
    if (g_world_pass_ranges_on && lwu(OV(0x001611c0)) > cur) g_world_pass_ranges.emplace_back(cur, lwu(OV(0x001611c0)));
    if (g_world_mrp) mrp_lod_world_after(kind >= 0 ? kind : 3, cur, lwu(OV(0x001611c0)));  // (the shadow chain in place of this range)
    static const bool trace = std::getenv("RAC1_VU1_TRACE") != nullptr;  // (testing)
    if (trace) std::fprintf(stderr, "[VU1] world pass chain %08x..%08x\n", cur, lwu(OV(0x001611c0)));
    if (!keep) sw(OV(0x001611c0), cur);
}

// orig: (host) the sky (0x23d968: the level's variant) as a pass for world_pass. Run only when its chain is drawn (RAC1_WORLD_FAITHFUL / the
// debug menu; RAC1_SKY_CHAIN=1 / RAC1_WORLD_CHAIN=1 for tests): its work is the chain (the sky's GIF packets and the texture uploads in it), and with
// the host's sky drawing instead (the default) running it changes the frame (it updates state the mobys' pass reads: the default image
// differs from the one without it by a sub-pixel shift of one prop)
static void sky_pass() { sky_render(); }
bool sky_wanted() {
    static const bool env = std::getenv("RAC1_SKY_CHAIN") != nullptr || std::getenv("RAC1_WORLD_CHAIN") != nullptr;
    return env || ((g_world_faithful || g_world_mrp || g_world_chain_forced) && g_render_sky);  // (the debug menu's Sky checkbox gates the faithful / MRP sky too)
}

// orig: (helper) part of 0x001f85b8 (render_frame): its texture block after the moby pass - the 3D effects (the draw lists, the lens
// flare, the decals, the particles, the sparkles, the screen effects), the HUD, the help text, the bars, then the block's texture
// uploads -; a function of its own so that the widened view's shadow run (mrp_lod.h mrp_lod_effects_shadow) runs the very
// same code and its chain is whole (the effect textures are given VRAM on demand during the block, uploaded at its end)
static void render_frame_overlay() {
    auto flags = [] { return lwu(OV(0x0015f3f4)); };
    constexpr OvConst<0x00169ff0u> EN{};  // the render enables (render_enables_init)
    texture_block_begin(0);
    if ((flags() & 0x20) != 0 && lw(EN + 0x30) != 0 && lw(OV(0x0015f5e0)) != 6) {
        packet_ref(0x0010faa0, lhu(0x0010fa90));
        sw(OV(0x0015f5e0), 6);
    }
    prof_mark_end(OV(0x0015f370), 4);
    prof_mark(OV(0x0015f370), 4);
    if ((flags() & 0x20) != 0) {
        mrp_fx_window(true);  // (host: the 3D effects' producers noted for the MRP, mrp_fx.h)
        packet_ref_1c24f0();
        if (lw(EN + 0x34) != 0) {
            if (lw(OV(0x0015f424)) != 0) draw_list_d880();
            sub_002a0ba8(0x42, 0x8000000048ull);
            lens_flare_draw();
            packet_ref_1c24f0();
            decals_draw();
        }
        prof_mark(OV(0x0015f380), 6);
        prof_mark_end(OV(0x0015f380), 6);
        if (lw(EN + 0x38) != 0) {
            const uint32_t pfirst = lwu(OV(0x001611c0));  // (host: the particles' chain range, the MRP's)
            sub_002a0ba8(8, 5);
            packet_ref_1c24f0();
            FlushCache(0);
            mrp_fx_particles_patch(true);  // (host: in the widened effects shadow only)
            particles_draw();
            mrp_fx_particles_patch(false);
            sw(OV(0x0015f5e0), 8);
            mrp_fx_note_particles(pfirst, lwu(OV(0x001611c0)));  // (host)
        }
        prof_mark(OV(0x0015f390), 8);
        prof_mark_end(OV(0x0015f390), 8);
        if (lw(EN + 0x3c) != 0) {
            int32_t m = lw(OV(0x0015f5c4));
            if (lw(OV(0x0015f428)) != 0) {
                packet_ref_1c24f0();
                draw_list_da80();
                m = lw(OV(0x0015f5c4));
            }
            if (m == 0) {
                const MrpFxScope fx(kMrpFxSparkles);  // (host)
                sparkles_draw();
            }
            sub_002a0ba8(0x42, 0x8000000044ull);
            screen_effects_draw();
        }
        prof_mark(OV(0x0015f3a0), 6);
        prof_mark_end(OV(0x0015f3a0), 6);
        mrp_fx_window(false);  // (host)
    }
    if (lw(EN + 0x48) != 0) packet_ref_151900();
    prof_mark_end(OV(0x0015f3b0), 0xf);
    packet_ref_1c24f0();
    if ((flags() & 0x10000) != 0) render_pass_297e28();
    if ((flags() & 0x80) != 0 && lw(EN + 0x40) != 0) {
        hud_draw();
        help_draw();
        screen_bars();
    }
    if (lw(OV(0x0015f5c4)) == 2 && lbu(0x0015ee40) != 0) cutscene_subtitle_draw();
    prof_mark_end(OV(0x0015f3b8), 0xe);
    prof_mark(OV(0x0015f3b8), 0xe);
    texture_block_end();
}

// orig: (helper) part of 0x001f85b8 (render_frame): the draw list 0x16dc80 (`list` 0, after the ties) or 0x16de80 (1, after the
// shrubs) in its texture block; a function of its own so that the widened view's shadow run (mrp_lod.h mrp_lod_list_shadow) runs the
// very same code
static void draw_list_block(int list) {
    packet_ref_1c24f0();
    texture_block_begin(1);
    mrp_fx_window(true);  // (host)
    if (list == 0) draw_list_dc80();
    else draw_list_de80();
    mrp_fx_window(false);
    texture_block_end();
    packet_ref_1c24c0();
}
static void draw_list_block_dc80() { draw_list_block(0); }
static void draw_list_block_de80() { draw_list_block(1); }
// (host) the block run as a shadow first in a widened view (the memory put back), then for real
static void draw_list_block_run(int list) {
    RAC1_PROF_ZONE(kGameEffects);
    const uint32_t cur = lwu(OV(0x001611c0));
    mrp_lod_list_shadow(list, list == 0 ? draw_list_block_dc80 : draw_list_block_de80, cur);
    draw_list_block(list);
    mrp_lod_list_after(list, cur, lwu(OV(0x001611c0)));
}

// The frame's drawing, its passes into the frame packet (the GS gets the 2D, the texture
// uploads and the fixed packets; the 3D passes, VU1 programs, are the host renderer's: TODO_PORT
// here), then the profiler marks of the DMA waits and the fog for the next frame.
// orig: 0x001f85b8
void render_frame() {
    RAC1_PROF_ZONE(kGameRender);  // (host: the profiler's sub-stages of the game code, nothing of the game's)
    auto flags = [] { return lwu(OV(0x0015f3f4)); };
    constexpr OvConst<0x00169ff0u> EN{};  // the render enables (render_enables_init)
    const uint32_t lv = lwu(OV(0x0016051c));
    if (lv == 0 || lh(lv + 4) != 0 || (flags() & 1) == 0 || lw(EN + 8) == 0 || lw(OV(0x0016c08c)) != 0) sub_00200fd8();
    mrp_lod_frame_begin();  // (host: testing only, RAC1_WIDEN_REF)
    mrp_fx_frame_begin();   // (host)
    render_view_layers_begin();  // (host)
    render_view_setup();
    moby_anim_lerp_slots_validate();
    zones_update();
    render_static_packets();
    sw(OV(0x0015f5e0), 0xffffffffu);
    prof_mark(OV(0x0015f340), 0xf);
    prof_mark_end(OV(0x0015f340), 0xf);
    if (lwu(OV(0x0016051c)) != 0 && (flags() & 1) != 0) {
        if (lw(EN + 8) != 0) {
            if (sky_wanted()) world_pass(sky_pass, -1);
            else TODO_PORT(0x0023d968, "sky (VU1): the host's sky draws instead")();
        }
        prof_mark_end(OV(0x0015f350), 0xe);
        prof_mark(OV(0x0015f350), 0xe);
    }
    if ((flags() & 2) != 0) world_pass(world_tfrag_pass, 0);
    packet_mark(0x02010000);
    if ((flags() & 4) != 0) {
        if (lw(0x0015ed80) != 0) TODO_PORT(0x00295d48, "ties, PAL (VU1)")();
        else world_pass(world_tie_pass, 1);
    }
    packet_mark(0x02020000);
    if (lw(EN + 0x34) != 0 && lw(OV(0x0015f42c)) != 0) draw_list_block_run(0);
    prof_mark(OV(0x0015f360), 6);
    prof_mark_end(OV(0x0015f360), 6);
    if ((flags() & 8) != 0) world_pass(world_shrub_pass, 2);
    packet_mark(0x02040000);
    if (lw(EN + 0x44) != 0 && lw(OV(0x0015f330)) != 0) TODO_PORT(0x001f9f78, "screen effects list")(OV(0x0015f330u));
    if ((flags() & 0x20) != 0) {
        texture_block_begin(1);
        // (host: the level's shadow meshes are VU1 program 7's batches: noted for the MRP, which draws them from their input, mrp_fx.h)
        const uint32_t scur = lwu(OV(0x001611c0u));
        mrp_fx_window(true);
        shadows_draw(0x00150000);  // $s6 as the main loop holds it (0x245c88): read by 0x1fe6d8
        mrp_fx_note_list(scur, lwu(OV(0x001611c0u)), 0);
        mrp_fx_window(false);
        texture_block_end();
    }
    if (lw(EN + 0x34) != 0 && lw(OV(0x0015f430)) != 0) draw_list_block_run(1);
    if ((flags() & 0x10) != 0) {
        if (g_moby_vu1_pass) {
            mrp_lod_moby_shadow(moby_pass);  // (the MRP's LOD slider / widened view, the memory put back)
            RAC1_PROF_ZONE(kGameMobyPass);
            moby_pass();
        }
        else TODO_PORT(0x00250038, "mobys (VU1): the host's model drawer instead")();
    }
    packet_mark(0x02080000);
    {  // (host: the widened view's 3D effects - particles, glows, the canopy shine, sparkles - run first as a shadow, the memory put back)
        RAC1_PROF_ZONE(kGameEffects);
        const uint32_t ecur = lwu(OV(0x001611c0));
        mrp_lod_effects_shadow(render_frame_overlay, ecur);
        render_frame_overlay();
        mrp_lod_effects_after(ecur, lwu(OV(0x001611c0)));
    }
    if ((flags() & 0x40) != 0) {
        if (lw(EN + 0x44) != 0) {
            sub_002a0ba8(0x42, 0x8000000044ull);
            if (lw(OV(0x00167014)) != 0) sub_001fa050(lbu(OV(0x00161200)), lbu(OV(0x00161201)), lbu(OV(0x00161202)), lbu(OV(0x00161203)));
            if (0.0f < lwc1(OV(0x0015f3fc))) {
                if (1.0f < lwc1(OV(0x0015f3fc))) swc1(OV(0x0015f3fc), 1.0f);
                sub_001fa050(0, 0, 0, float_to_int(lwc1(OV(0x0015f3fc)) * 128.0f));
            }
            if (0.0f < lwc1(OV(0x0015f400))) {
                if (1.0f < lwc1(OV(0x0015f400))) swc1(OV(0x0015f400), 1.0f);
                sub_001fa050(0xff, 0xff, 0xff, float_to_int(lwc1(OV(0x0015f400)) * 128.0f));
            }
            if (lw(OV(0x0015f30c)) != 0 && lw(OV(0x0015f310)) != 0) screen_flash();
        }
        prof_mark_end(OV(0x0015f3c0), 10);
    }
    const int prof_end = prof::begin(prof::kGameWorldEnd);
    vif0_send(0x00100ae0);  // the VU0 micro programs
    FlushCache(0);
    swc1(OV(0x0015f5d4), fpu_div(int_to_float(static_cast<int32_t>(host_rcnt1())), lw(0x0015ed80) != 0 ? 11520.0f : 9600.0f));
    dma_wait(2);
    prof_mark(OV(0x0015f3d0), 0x11);
    if ((flags() & 2) != 0) {
        if (lw(EN + 0x10) != 0 && !tfrag_pass_off()) {
            world_tfrag_lighting(OV(0x001c5400u));
            world_tfrag_end();
        }
        prof_mark(OV(0x001e7a00), 2);
    }
    dma_wait(4);
    prof_mark(OV(0x0015f3d0), 0x11);
    if ((flags() & 4) != 0) {
        if (lw(EN + 0x18) != 0 && !world_pass_skipped(1)) {
            if (lw(0x0015ed80) != 0) {
                TODO_PORT(0x00295c50, "tie lighting, PAL")();
                TODO_PORT(0x00295bf8, "tie end, PAL")();
            } else {
                world_tie_lighting(OV(0x001c7300u));
                world_tie_end();
            }
        }
        prof_mark(OV(0x0015f3d8), 5);
    }
    dma_wait(8);
    prof_mark(OV(0x0015f3d0), 0x11);
    if ((flags() & 8) != 0) {
        if (lw(EN + 0x20) != 0 && !world_pass_skipped(2)) {
            world_shrub_lighting(OV(0x001bcfb0u));
            world_shrub_end();
        }
        prof_mark(OV(0x001e7a18), 7);
    }
    dma_wait(0x10);
    prof_mark(OV(0x0015f3d0), 0x11);
    if ((flags() & 0x10) != 0) {
        if (lw(EN + 0x28) != 0) {  // (its group list is the moby pass's: with the pass only)
            if (g_moby_vu1_pass) moby_texture_slots();
            else TODO_PORT(0x0024fad0, "moby end: with the original moby pass only")();
        }
        prof_mark(OV(0x0015f3e8), 3);
    }
    prof::end(prof_end);
    fog_setup();
    sw(OV(0x0015f608), 0);
}

// The DMA completion flags 0x1611a0 waited for (the host's DMA is done at once: none set).
// orig: 0x002a09d8
void dma_wait(uint32_t mask) {
    for (int32_t i = 0; (lwu(OV(0x001611a0)) & mask) != 0; ++i) {
        if (!(i < 0x186a1)) {
            debug_printf_stub();
            TODO_PORT(0x001f6da8, "DMA timeout report")();
            return;
        }
    }
}

// orig: 0x001f8e30
void render_frame_gate() {
    if (lw(OV(0x0015f5d8)) != 0 || lbu(0x00141401) != 0) return;
    sw(OV(0x0015f3f4), 0x100ff);
    if (lw(OV(0x0015f5c4)) == 3) sw(OV(0x0015f3f4), 0x7f);
    render_frame();
}

// The game's start on the level: the overlay's globals cleared, the start-up tables, the level
// set up from the disc data read before (0x244110), the frame control, the start mode, the music
// and the level's visit stats.
// orig: 0x00245c28 (its start, up to the main loop)
void overlay_entry_start() {
    mem_clear128(OV(0x00161e00), OV(0x00165fb0) - OV(0x00161e00));
    stash_init();
    sw(OV(0x0015f5c8), 0);
    sw(OV(0x0015f5c4), 6);
    overlay_init_tables();
    sh(0x00151708, 0);
    level_setup(1, 0);
    sw(OV(0x0016c060), 0xf);
    sw(OV(0x0016c088), lwu(0x0015ed80));
    sw(OV(0x0016c090), 0xffffffffu);
    sw(OV(0x0016c14c), 1);
    sw(OV(0x0016c064), 1);
    render_enables_init();
    planet_map_init();
    hud_init();
    menu_sequences_stash();
    start_mode_select();
    sw(OV(0x0015f3f8), 0);
    ee_thread_vsync(EeVsyncFrame::Held, false);  // the vsync wait 0x122298: a vsync passes (the host's per-vsync work), nothing shown
    TODO_PORT(0x00200f18, "kernel: display packet send")();
    music_start(lh(0x00151708), 1, 0x400);
    const int32_t lvl = lw(0x0015ed84);
    if ((lvl == 1 && lbu(0x0013dd43) == 0) || (lvl == 0 && lbu(0x0013d390) != 0)) {
        music_volumes_default();
    } else {
        sub_00265828(0);
    }
    level_visit_stats();
    snd_hold();
}

// The level's visit stats: visited flag 0x13dd58[level], visit count 0x141680[level * 4] (up to
// 0xffff), the longest stay in 10 s units +2, the visited bits +4.
// orig: (helper) repeated in 0x00245c28
void level_visit_stats() {
    const uint32_t lv = lwu(0x0015ed84);
    if (lbu(0x0013dd58 + lv) == 0) sb(0x0013dd58 + lv, 1);
    const uint32_t row = 0x00141680 + lwu(0x0015ed84) * 8;
    const uint32_t visits = lhu(row);
    if (!(0xfffeu < visits)) sh(row, visits + 1);
    level_time_stat();
    const uint32_t l2 = lwu(0x0015ed84);
    const uint32_t bits = 0x00141684 + l2 * 8;
    sw(bits, lwu(bits) | (1u << (l2 & 0x1f)) | 0x80000000u);
}

// orig: (helper) repeated in 0x00245c28
void level_time_stat() {
    const int32_t t = frame_scale_i(lw(0x0015eea4)) / 0x258;
    const uint32_t row = 0x00141680 + lwu(0x0015ed84) * 8;
    if (static_cast<int32_t>(lhu(row + 2)) < t) sh(0x00141680 + lwu(0x0015ed84) * 8 + 2, static_cast<uint32_t>(frame_scale_i(lw(0x0015eea4)) / 0x258));
}

static bool overlay_frame_end(int32_t mode);
namespace {
// Game mode 1's movie still playing. The PS2 runs the whole movie inside the mode's update 0x2999a8 (the player's own
// loop waits for its vsyncs), in one pass of the main loop; the host runs one picture of it a host frame. The host
// frames after the pass's first are the rest of that one pass, inside 0x2999a8: nothing else of the pass runs again
// (its start, the frame's end: the frame counter 0x15f3f8, the save's time played 0x15ee24, the frames-in-mode
// count). Before, every host frame of a movie ran a whole pass around it: the time played counted the movie's
// length, 60 a second, where the PS2's does not count it at all (#24).
bool g_movie_pass = false;

// Whether game mode 1's update 0x2999a8 is over after a call of the host's split of it (movie_mode_update): its end
// 0x2997c8 cuts the frame short (0x15f5d8 set), as a movie started again by the talk's next record does (0x2995d0);
// the player's frames leave it as the pass's start did (0).
// orig: (host)
bool movie_update_over() { return lw(OV(0x0015f5d8)) != 0; }
}  // namespace

// One host frame of the main loop's pass in game mode 1, from its update's call on (0x245fc0): one picture of the
// movie (the pass's first frame: the movie's start too); once the update is over, the rest of the pass
// (overlay_frame_end) and `done` set. False when the game quits (0x15f570).
// orig: (host) a part of 0x00245c28's pass in game mode 1 (0x245fc0 .. its end)
bool overlay_movie_pass(bool& done) {
    movie_mode_update();
    done = movie_update_over();
    g_movie_pass = !done;
    if (!done) return lw(OV(0x0015f570)) == 0;
    return overlay_frame_end(1);
}

// One pass of the main loop: the frame's counters and packets, the mode's update and drawing
// (0 gameplay: the pad, 0x297f78, 0x1f8e30), the frames-in-mode count, the end of the frame
// (debug strings, timing, an extra update when the frame ran long, the frame counter, the
// autosave tick) and the restart of the level when the hero died (0x141401). False when the
// game quits (0x15f570).
// orig: 0x00245c28 (one pass of its main loop)
bool overlay_frame() {
    bool movie_done = false;
    if (g_movie_pass) return overlay_movie_pass(movie_done);  // (the rest of the pass that started the movie)
    const uint32_t cnt = host_rcnt1();
    sd(0x0015ed40, ld(0x0015ed40) + cnt);
    ee_t1_reset();  // (sw zero, T1_COUNT)
    frame_counters_reset();
    sw(OV(0x0015f5d8), 0);
    TODO_PORT(0x002a08c8, "DMA: packet kick")();
    sub_002a0858();
    sub_00201040();
    packet_call_151b60();
    packet_call_display();
    snd_flush();
    const int32_t mode = lw(OV(0x0015f5c4));
    switch (mode) {
        case 0:
            pad_read_packet(0x0013c940, g_host_pad_packet, 6);
            gameplay_frame();
            render_frame_gate();
            break;
        case 1: return overlay_movie_pass(movie_done);  // (the movie goes on in the next host frames: g_movie_pass)
        case 2: pad_read_packet(0x0013c940, g_host_pad_packet, 6); cutscene_update(); mode2_draw(); break;
        case 3: pad_read_packet(0x0013c940, g_host_pad_packet, 6); menu_update(); menu_draw_mode3(); break;
        case 4: pad_read_packet(0x0013c940, g_host_pad_packet, 6); screen_update(); screen_draw_mode4(); break;
        case 5:  // (the vendor screens; enhancement: the Vendor's "All Ammo" entry's input, vendor_buy_all.h)
            pad_read_packet(0x0013c940, g_host_pad_packet, 6);
            vendor_buy_all_input();
            vendor_mode_update_tr(0);
            vendor_mode_draw_tr(0);
            break;
        case 6: pad_read_packet(0x0013c940, g_host_pad_packet, 6); ship_sequence_update(); ship_sequence_draw(); break;
        case 7: pad_read_packet(0x0013c940, g_host_pad_packet, 6); credits_update_tr(0); credits_draw_tr(0); break;  // (the credits)
        case -1: pad_read_packet(0x0013c940, g_host_pad_packet, 6); TODO_PORT(0x001f4598, "mode -1 update")(); TODO_PORT(0x001f58e0, "mode -1 draw")(); break;
        default: break;
    }
    return overlay_frame_end(mode);
}

// orig: (helper) the rest of 0x00245c28's pass after the game mode's update (`mode` the mode it ran): the
// frames-in-mode count, the frame's end, the restart of the level when the hero died. False when the game quits.
static bool overlay_frame_end(int32_t mode) {
    if (lw(OV(0x0015f5c4)) == mode) {
        sw(OV(0x0015f5c8), static_cast<uint32_t>(lw(OV(0x0015f5c8)) + 1));
    } else {
        sw(OV(0x0015f5c8), 0);
    }
    if (lw(OV(0x0015f5d8)) != 0) {
        dma_wait(1);
        sub_002a07f8();
        return lw(OV(0x0015f570)) == 0;
    }
    card_machine();
    card_ui_machine();
    const bool debug_text = (2 < lw(0x0013d364) || -1 < lw(0x0013d36c)) && lw(OV(0x001b9e2c)) == 0;
    if (debug_text) TODO_PORT(0x001f9060, "debug: memory text")();
    sw(OV(0x001b9e2c), 0);
    debug_strings_flush();
    prof_mark(OV(0x0015fcb8), 0x10);
    prof_mark_end(OV(0x0015fcb8), 0x10);
    dma_wait(1);
    swc1(OV(0x0015f5d0), fpu_div(int_to_float(static_cast<int32_t>(host_rcnt1())), lw(0x0015ed80) != 0 ? 11520.0f : 9600.0f));
    if (lw(OV(0x0015f5c4)) == 0 && lw(OV(0x0016c070)) != 0) debug_hook_1f6d90();
    if (lbu(0x00141401) == 0) {
        const int32_t m = lw(OV(0x0015f5c4));
        if (m == 0 || m == 2) {
            const int32_t budget = lw(0x0015ed80) != 0 ? 0x2d00 : 0x2580;
            if (budget < static_cast<int32_t>(host_rcnt1())) {
                if (m == 0) {
                    pad_read_packet(0x0013c940, g_host_pad_packet, 6);
                    gameplay_frame();
                } else if (lw(OV(0x0016c8b0)) == 0) {
                    pad_read_packet(0x0013c940, g_host_pad_packet, 6);
                    cutscene_update();
                }
            }
            TODO_PORT(0x00122298, "kernel: vsync wait")(0);
            sw(OV(0x0015f3f8), static_cast<uint32_t>(lw(OV(0x0015f3f8)) + 1));
            autosave_tick();
            sw(OV(0x0015f5ac), 0);
        } else {
            TODO_PORT(0x00122298, "kernel: vsync wait")(0);
            sw(OV(0x0015f3f8), static_cast<uint32_t>(lw(OV(0x0015f3f8)) + 1));
            autosave_tick();
            sw(OV(0x0015f5ac), 0);
        }
    }
    // 0x1f8540: the GS display registers (the host's window)
    if (lw(OV(0x0015f5c4)) == 0 && lbu(0x00141401) != 0) {  // the hero died: the level restarts
        sub_002a07f8();
        snd_release();
        snd_flush();
        sound_stop_all();
        music_stop_all();
        level_setup(0, 1);
        sw(OV(0x0015f5c4), 0);
        music_start(lh(0x00151708), 1, 0x400);
        hud_init();
    }
    level_time_stat();
    return lw(OV(0x0015f570)) == 0;
}

}  // namespace rac1
