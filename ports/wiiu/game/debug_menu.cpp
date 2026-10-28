// The GamePad's debug menu (debug_menu.h): text drawn as quads from a font atlas made at build time (ports/wiiu/tools/font_atlas.py),
// the items' boxes plain quads, a tap on the touch screen flipping the item under it. Host code.
#include "debug_menu.h"

#include "mrp_world_gx2.h"

#include <game/ovl/game_skips.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/world_lighting.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/ee/game_thread.h>

#include "font_atlas.h"  // (generated into the build folder)

#include <coreinit/memdefaultheap.h>
#include <gx2/clear.h>
#include <gx2/draw.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/shaders.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <vpad/input.h>
#include <whb/gfx.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <cstring>

bool g_menu_draw_mobys = true, g_menu_draw_2d = true, g_menu_overlap = true;
float g_menu_vpscale = 1.0f;

// ((2026-10-28) the game's update on the game thread beside the drawing - orig_main.cpp's Wii U branch, runtime/ee/game_thread.h:
// RAC1_WIIU_GAME_THREAD=0 the host frame in series as before; read at the first ask, run.cfg's then set; the Game page flips it)
namespace {
int g_game_thread = -1;  // (-1: not read yet)
}  // namespace
bool rac1_wiiu_game_thread() {
    if (g_game_thread < 0) {
        const char* e = std::getenv("RAC1_WIIU_GAME_THREAD");
        g_game_thread = e == nullptr || std::atoi(e) != 0 ? 1 : 0;
    }
    return g_game_thread != 0;
}

void rac1_wiiu_quit();  // wiiu_input.cpp: the game asked to end by itself
bool rac1_wiiu_last_vpad(VPADStatus* out);  // wiiu_input.cpp: the GamePad's last sample the game's pads read took

namespace {

bool g_up = false;
WHBGfxShaderGroup g_sh{};
GX2RBuffer g_vb{};
GX2Texture g_font{};
GX2Sampler g_sampler{};
uint32_t g_sampler_loc = 0;
constexpr uint32_t kMaxQuads = 6000;
constexpr int kScreenW = 854, kScreenH = 480;

// one quad's six vertices: x, y (clip space), u, v (u < 0: a plain box), then r, g, b, a
struct V {
    float p[4];
    float c[4];
};
std::vector<V> g_v;

void quad(float x0, float y0, float x1, float y1, float u0, float v0, float u1, float v1, const float* col) {
    if (g_v.size() + 6 > kMaxQuads * 6) return;
    auto cx = [](float x) { return x / kScreenW * 2.0f - 1.0f; };
    auto cy = [](float y) { return 1.0f - y / kScreenH * 2.0f; };
    const V a{{cx(x0), cy(y0), u0, v0}, {col[0], col[1], col[2], col[3]}}, b{{cx(x1), cy(y0), u1, v0}, {col[0], col[1], col[2], col[3]}},
        c{{cx(x1), cy(y1), u1, v1}, {col[0], col[1], col[2], col[3]}}, d{{cx(x0), cy(y1), u0, v1}, {col[0], col[1], col[2], col[3]}};
    g_v.insert(g_v.end(), {a, b, c, a, c, d});
}
void box(float x, float y, float w, float h, const float* col) { quad(x, y, x + w, y + h, -1.0f, 0.0f, -1.0f, 0.0f, col); }
// text at (x, y) (the top left), one atlas cell a character
void text(float x, float y, const std::string& s, const float* col) {
    for (char ch : s) {
        const int c = static_cast<unsigned char>(ch);
        if (c > 32 && c < 128) {
            const int i = c - 32;
            const float u0 = static_cast<float>((i % kFontCols) * kFontCellW) / kFontW, v0 = static_cast<float>((i / kFontCols) * kFontCellH) / kFontH;
            quad(x, y, x + kFontCellW, y + kFontCellH, u0, v0, u0 + static_cast<float>(kFontCellW) / kFontW, v0 + static_cast<float>(kFontCellH) / kFontH, col);
        }
        x += kFontCellW;
    }
}

// The items: a label and its state's text; a tap calls `flip`
struct Item {
    const char* label;
    std::string (*state)();
    void (*flip)();
};
std::string on_off(bool b) { return b ? "ON" : "off"; }
int g_page = 0;  // 0 the picture's pieces, 1 the game's (game_skips.h)
const Item kRender[] = {
    {"World: terrain (tfrag)", [] { return on_off(rac1::g_render_tfrag); }, [] { rac1::g_render_tfrag = !rac1::g_render_tfrag; }},
    {"World: ties", [] { return on_off(rac1::g_render_tie); }, [] { rac1::g_render_tie = !rac1::g_render_tie; }},
    {"World: shrubs", [] { return on_off(rac1::g_render_shrub); }, [] { rac1::g_render_shrub = !rac1::g_render_shrub; }},
    // (2026-10-28: the MRP sky, mrp_world_gx2.h g_wiiu_sky - its capture and its draws; the game's sky pass runs either way)
    {"Sky", [] { return rac1::mrp_world_gx2_sky_loaded() ? on_off(rac1::g_wiiu_sky) : std::string("no shaders"); }, [] { rac1::g_wiiu_sky = !rac1::g_wiiu_sky; }},
    // (2026-10-28: the skipped passes' relighting made from the entries' flags - the point lights' light on the world -, world_lighting.h
    // g_world_relight; the page's 14th row, its last at y 454)
    {"World relit (point lights)", [] { return on_off(rac1::g_world_relight); }, [] { rac1::g_world_relight = !rac1::g_world_relight; }},
    {"Mobys drawn", [] { return on_off(g_menu_draw_mobys); }, [] { g_menu_draw_mobys = !g_menu_draw_mobys; }},
    {"2D (HUD, text, menus)", [] { return on_off(g_menu_draw_2d); }, [] { g_menu_draw_2d = !g_menu_draw_2d; }},
    {"CPU / GPU overlap", [] { return on_off(g_menu_overlap); }, [] { g_menu_overlap = !g_menu_overlap; }},
    {"World prepared on a worker", [] { return on_off(rac1::g_wiiu_world_async); }, [] { rac1::g_wiiu_world_async = !rac1::g_wiiu_world_async; }},
    {"World draws merged", [] { return std::string("mode ") + std::to_string(rac1::mrp_world_gx2_merge_mode()); },
     [] {
         int& m = rac1::mrp_world_gx2_merge_mode();
         m = m == 0 ? 3 : 0;
     }},
    {"Resolution (the frame's share)", [] {
         char b[16];
         std::snprintf(b, sizeof b, "%.2f", g_menu_vpscale);
         return std::string(b);
     },
     [] { g_menu_vpscale = g_menu_vpscale > 0.9f ? 0.75f : g_menu_vpscale > 0.6f ? 0.5f : 1.0f; }},
    // (2026-10-28, the GPU's time: rac1::Gx2Perf, mrp_world_gx2.h - two rows, the page's room: the pixel shaders' builds and the tests in
    // one row since the sky's row came)
    {"GPU: hierarchical Z", [] { return on_off(rac1::g_gx2_perf.hiz); }, [] { rac1::g_gx2_perf.hiz = !rac1::g_gx2_perf.hiz; }},
    {"GPU: shaders / test", [] {
         const rac1::Gx2Perf& g = rac1::g_gx2_perf;
         if (g.pixels_off) return std::string("test: no pixels");
         if (!g.zclear2) return std::string("test: 1 Z clear");
         return std::string(g.lean && g.nokill ? "lean+nokill" : g.lean ? "lean" : g.nokill ? "nokill" : "off");
     },
     [] {  // (off -> nokill -> lean -> both -> the test: the pixels off (8 x 8 scissor) -> the test: one depth clear (WHB's) -> off; a test
           // with the shaders' builds off)
         rac1::Gx2Perf& g = rac1::g_gx2_perf;
         const int k = g.pixels_off ? 4 : !g.zclear2 ? 5 : (g.lean ? 2 : 0) | (g.nokill ? 1 : 0);
         const int n = (k + 1) % 6;
         g.nokill = n == 1 || n == 3;
         g.lean = n == 2 || n == 3;
         g.pixels_off = n == 4;
         g.zclear2 = n != 5;
     }},
    {"Exit game (back to the Wii U Menu)", [] { return std::string("tap"); }, [] { rac1_wiiu_quit(); }},
};
// (ON: the part runs; off: skipped - the game goes on without it)
#define RAC1_SKIP_ITEM(label, var) {label, [] { return on_off(!rac1::var); }, [] { rac1::var = !rac1::var; }}
const Item kGame[] = {
    RAC1_SKIP_ITEM("Mobys' updates (enemies ...)", g_skip_mobys),
    RAC1_SKIP_ITEM("Ratchet (the hero's frame)", g_skip_hero),
    RAC1_SKIP_ITEM("Particles, spin timers", g_skip_particles),
    RAC1_SKIP_ITEM("Camera", g_skip_camera),
    RAC1_SKIP_ITEM("Game sound (requests)", g_skip_sound),
    RAC1_SKIP_ITEM("HUD, lights, help", g_skip_hud),
    RAC1_SKIP_ITEM("3D effects, draw lists", g_skip_effects),
    RAC1_SKIP_ITEM("Shadows", g_skip_shadows),
    RAC1_SKIP_ITEM("Effects' draw lists", g_skip_draw_lists),
    RAC1_SKIP_ITEM("Mobys' poses (joints)", g_skip_joints),
    // (2026-10-28: the poses' hot loops in plain floats, moby_joints.cpp: fast -> exact (rounded as the VU0) -> off (the VU0's ops))
    {"Fast poses", [] { return std::string(rac1::g_pose_fast == 1 ? "fast" : rac1::g_pose_fast == 2 ? "exact" : "off"); },
     [] { rac1::g_pose_fast = rac1::g_pose_fast == 1 ? 2 : rac1::g_pose_fast == 2 ? 0 : 1; }},
    // ((2026-10-28) the widescreen, render_view.h g_view_draw_widen: 16:9 -> 16:9 with the mobys culled 4:3 -> the game's 4:3 -> 16:9)
    {"Widescreen", [] { return std::string(!(rac1::g_view_draw_widen > 1.0f) ? "off (4:3)" : rac1::g_view_draw_widen_mobys ? "16:9" : "16:9, mobys 4:3"); },
     [] {
         if (!(rac1::g_view_draw_widen > 1.0f)) {
             rac1::g_view_draw_widen = 4.0f / 3.0f;
             rac1::g_view_draw_widen_mobys = true;
         } else if (rac1::g_view_draw_widen_mobys) {
             rac1::g_view_draw_widen_mobys = false;
         } else {
             rac1::g_view_draw_widen = 1.0f;
         }
     }},
    {"Catch-up steps drawn", [] { return on_off(!rac1::g_draw_last_step_only); }, [] { rac1::g_draw_last_step_only = !rac1::g_draw_last_step_only; }},
    // ((2026-10-28) the game's steps on the game thread while the frame before is drawn; the page's 14th row - "Exit game" is the
    // Picture page's)
    {"Game thread (beside the drawing)", [] { return on_off(rac1_wiiu_game_thread()); }, [] {
         rac1_wiiu_game_thread();
         g_game_thread = g_game_thread != 0 ? 0 : 1;
     }},
};
#undef RAC1_SKIP_ITEM
// the page's items (row 0, above them, switches the page)
const Item* page_items(size_t* n) {
    if (g_page == 0) {
        *n = sizeof kRender / sizeof kRender[0];
        return kRender;
    }
    *n = sizeof kGame / sizeof kGame[0];
    return kGame;
}
constexpr int kItemY = 34, kItemH = 30, kItemX = 8, kItemW = 420;

bool g_was_touched = false;
int g_touch_x = -1, g_touch_y = -1, g_taps = 0;
// ((2026-10-28) the taps while the game thread runs the game's steps - its update reads many of the switches -: flipped once it is
// joined, debug_menu_flush)
std::vector<const Item*> g_held;
void flip(const Item& it) {
    it.flip();
    std::printf("[wiiu] menu: %s -> %s\n", it.label, it.state().c_str());
}

}  // namespace

bool debug_menu_wanted() {
    static const bool on = std::getenv("RAC1_WIIU_MENU") != nullptr && std::atoi(std::getenv("RAC1_WIIU_MENU")) != 0;
    return on;
}

bool debug_menu_init() {
    if (g_up) return true;
    std::vector<uint8_t> gsh;
    if (std::FILE* f = std::fopen("shaders/debug_text.gsh", "rb")) {
        std::fseek(f, 0, SEEK_END);
        gsh.resize(static_cast<size_t>(std::ftell(f)));
        std::fseek(f, 0, SEEK_SET);
        if (std::fread(gsh.data(), 1, gsh.size(), f) != gsh.size()) gsh.clear();
        std::fclose(f);
    }
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_sh, 0, gsh.data())) {
        std::printf("[wiiu] menu: shaders/debug_text.gsh not loaded\n");
        return false;
    }
    WHBGfxInitShaderAttribute(&g_sh, "a_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&g_sh, "a_col", 0, 16, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&g_sh);
    for (uint32_t i = 0; i < g_sh.pixelShader->samplerVarCount; ++i)
        if (std::strcmp(g_sh.pixelShader->samplerVars[i].name, "u_font") == 0) g_sampler_loc = g_sh.pixelShader->samplerVars[i].location;
    g_vb.flags = static_cast<GX2RResourceFlags>(GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_WRITE | GX2R_RESOURCE_USAGE_GPU_READ);
    g_vb.elemSize = sizeof(V);
    g_vb.elemCount = kMaxQuads * 6;
    if (!GX2RCreateBuffer(&g_vb)) {
        WHBGfxFreeShaderGroup(&g_sh);
        return false;
    }
    // the font: one coverage byte a pixel, a linear R8 surface
    GX2Surface& s = g_font.surface;
    s.dim = GX2_SURFACE_DIM_TEXTURE_2D;
    s.width = kFontW;
    s.height = kFontH;
    s.depth = 1;
    s.mipLevels = 1;
    s.format = GX2_SURFACE_FORMAT_UNORM_R8;
    s.aa = GX2_AA_MODE1X;
    s.use = GX2_SURFACE_USE_TEXTURE;
    s.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    g_font.viewNumMips = 1;
    g_font.viewNumSlices = 1;
    g_font.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_R, GX2_SQ_SEL_R, GX2_SQ_SEL_R);
    GX2CalcSurfaceSizeAndAlignment(&s);
    GX2InitTextureRegs(&g_font);
    s.image = MEMAllocFromDefaultHeapEx(s.imageSize, static_cast<int32_t>(s.alignment));
    if (s.image == nullptr) return false;
    for (int y = 0; y < kFontH; ++y) std::memcpy(static_cast<uint8_t*>(s.image) + static_cast<size_t>(y) * s.pitch, kFontAtlas + static_cast<size_t>(y) * kFontW, kFontW);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, s.image, s.imageSize);
    GX2InitSampler(&g_sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
    g_v.reserve(kMaxQuads * 6);
    g_up = true;
    std::printf("[wiiu] menu: up on the GamePad (font %s)\n", kFontOk ? "in" : "missing: boxes only");
    return true;
}

void debug_menu_shutdown() {
    if (!g_up) return;
    GX2RDestroyBufferEx(&g_vb, GX2R_RESOURCE_BIND_NONE);
    if (g_font.surface.image != nullptr) MEMFreeToDefaultHeap(g_font.surface.image);
    g_font = GX2Texture{};
    WHBGfxFreeShaderGroup(&g_sh);
    g_sh = WHBGfxShaderGroup{};
    g_up = false;
}

void debug_menu_input() {
    if (!g_up) return;
    VPADStatus v{};
    VPADReadError e = VPAD_READ_SUCCESS;
    // (VPAD's own sample buffer: the game's VPADRead takes the latest too - reading it here takes nothing from the game's buttons.
    // The game thread reading the pads meanwhile: its last sample, VPAD read by one thread at a time)
    const bool game_running = rac1::game_thread::running();
    if (game_running) {
        if (!rac1_wiiu_last_vpad(&v)) return;
    } else if (VPADRead(VPAD_CHAN_0, &v, 1, &e) != 1 || e != VPAD_READ_SUCCESS) {
        return;
    }
    VPADTouchData t{};
    VPADGetTPCalibratedPoint(VPAD_CHAN_0, &t, &v.tpNormal);
    const bool touched = v.tpNormal.touched != 0 && v.tpNormal.validity == 0;
    if (touched) {
        // (the calibrated point is in the 1280 x 720 frame: the menu's 854 x 480)
        g_touch_x = t.x * kScreenW / 1280;
        g_touch_y = t.y * kScreenH / 720;
    }
    if (touched && !g_was_touched) {  // a new tap: the item under it flipped
        ++g_taps;
        size_t n = 0;
        const Item* items = page_items(&n);
        auto hit = [](int y) { return g_touch_x >= kItemX && g_touch_x < kItemX + kItemW && g_touch_y >= y && g_touch_y < y + kItemH - 2; };
        if (hit(kItemY)) {
            g_page ^= 1;
        } else {
            for (size_t i = 0; i < n; ++i)
                if (hit(kItemY + static_cast<int>(i + 1) * kItemH)) {
                    if (game_running) g_held.push_back(&items[i]);
                    else flip(items[i]);
                }
        }
    }
    g_was_touched = touched;
}

void debug_menu_flush() {
    for (const Item* it : g_held) flip(*it);
    g_held.clear();
}

namespace {
void menu_submit(const GX2ColorBuffer* cb, size_t verts);
}  // namespace

void debug_menu_draw(const GX2ColorBuffer* cb, const std::vector<std::string>& stats) {
    if (!g_up || cb == nullptr) return;
    size_t n = 0;
    const Item* items = page_items(&n);
    std::vector<std::string> states(n);
    for (size_t i = 0; i < n; ++i) states[i] = items[i].state();
    char b[64];
    std::snprintf(b, sizeof b, "touch %d,%d  taps %d", g_touch_x, g_touch_y, g_taps);
    // ((2026-10-28) the quads made again only when what they show changes - the page, the items' states, the stats (a window's: every 30
    // host frames at most), the touch line -, and only their bytes flushed to the GPU (the buffer's whole 1.1 MB before): the menu cost ~2 ms
    // a host frame. RAC1_WIIU_MENU_CACHE=0: made every frame as before)
    static const bool cache = std::getenv("RAC1_WIIU_MENU_CACHE") == nullptr || std::atoi(std::getenv("RAC1_WIIU_MENU_CACHE")) != 0;
    static std::string last_key;
    static size_t last_n = 0;
    std::string key(1, static_cast<char>('0' + g_page));
    for (const std::string& st : states) key.append(st).push_back('\x1f');
    for (const std::string& l : stats) key.append(l).push_back('\x1e');
    key += b;
    if (cache && key == last_key) {
        if (last_n != 0) menu_submit(cb, last_n);
        return;
    }
    last_key = std::move(key);
    g_v.clear();
    static const float white[4] = {1, 1, 1, 1}, grey[4] = {0.6f, 0.6f, 0.6f, 1}, on[4] = {0.10f, 0.45f, 0.15f, 1}, off[4] = {0.45f, 0.12f, 0.10f, 1},
                       head[4] = {1.0f, 0.85f, 0.3f, 1}, tab[4] = {0.15f, 0.2f, 0.35f, 1};
    text(kItemX, 6, "rac1-native debug menu (tap an item)", head);
    box(kItemX, kItemY, kItemW, kItemH - 4, tab);
    text(kItemX + 6, kItemY + 4, g_page == 0 ? "Page: [Picture]  Game   (tap)" : "Page:  Picture  [Game]  (tap)", head);
    for (size_t i = 0; i < n; ++i) {
        const int y = kItemY + static_cast<int>(i + 1) * kItemH;
        const std::string& st = states[i];
        box(kItemX, static_cast<float>(y), kItemW, kItemH - 4, st == "off" ? off : on);
        text(kItemX + 6, static_cast<float>(y + 4), items[i].label, white);
        text(kItemX + kItemW - 6 - kFontCellW * static_cast<float>(st.size()), static_cast<float>(y + 4), st, white);
    }
    // the stats: their first two lines (the frame, the steps) on both pages, then the page's own section (the game's after "#game")
    float y = 6;
    bool game_part = false;
    int line = 0;
    for (const std::string& l : stats) {
        if (l == "#game") {
            game_part = true;
            continue;
        }
        if (line++ >= 2 && game_part != (g_page == 1)) continue;
        text(440, y, l, l.rfind("  ", 0) == 0 ? grey : white);
        y += kFontCellH + 2;
    }
    text(kItemX, kScreenH - kFontCellH - 4, b, grey);
    last_n = g_v.size();
    if (g_v.empty()) return;
    // (the GPU is done with the buffer: wiiu_gfx.cpp waits for the frame before's commands before this frame's draws)
    const auto lk = static_cast<GX2RResourceFlags>(GX2R_RESOURCE_DISABLE_CPU_INVALIDATE | GX2R_RESOURCE_DISABLE_GPU_INVALIDATE);
    void* p = GX2RLockBufferEx(&g_vb, lk);
    const uint32_t bytes = static_cast<uint32_t>(g_v.size() * sizeof(V));
    std::memcpy(p, g_v.data(), bytes);
    GX2RUnlockBufferEx(&g_vb, lk);
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_CPU | GX2_INVALIDATE_MODE_ATTRIBUTE_BUFFER), p, bytes);
    menu_submit(cb, last_n);
}

namespace {
// orig: (host) the menu's `verts` vertices (in g_vb) drawn into `cb`
void menu_submit(const GX2ColorBuffer* cb, size_t verts) {
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    GX2SetViewport(0.0f, 0.0f, static_cast<float>(cb->surface.width), static_cast<float>(cb->surface.height), 0.0f, 1.0f);
    GX2SetScissor(0, 0, cb->surface.width, cb->surface.height);
    GX2SetShaderMode(g_sh.vertexShader->mode);
    GX2SetFetchShader(&g_sh.fetchShader);
    GX2SetVertexShader(g_sh.vertexShader);
    GX2SetPixelShader(g_sh.pixelShader);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x01, FALSE, TRUE);
    GX2SetBlendControl(GX2_RENDER_TARGET_0, GX2_BLEND_MODE_SRC_ALPHA, GX2_BLEND_MODE_INV_SRC_ALPHA, GX2_BLEND_COMBINE_MODE_ADD, FALSE, GX2_BLEND_MODE_ONE,
                       GX2_BLEND_MODE_ZERO, GX2_BLEND_COMBINE_MODE_ADD);
    GX2SetPixelTexture(&g_font, g_sampler_loc);
    GX2SetPixelSampler(&g_sampler, g_sampler_loc);
    GX2RSetAttributeBuffer(&g_vb, 0, sizeof(V), 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, static_cast<uint32_t>(verts), 0, 1);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
}
}  // namespace
