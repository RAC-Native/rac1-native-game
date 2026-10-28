// The GamePad's debug menu (debug_menu.h): text drawn as quads from a font atlas made at build time (ports/wiiu/tools/font_atlas.py),
// the items' boxes plain quads, a tap on the touch screen flipping the item under it. Host code.
#include "debug_menu.h"

#include "mrp_world_gx2.h"

#include <game/ovl/game_skips.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/render_toggles.h>

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

void rac1_wiiu_quit();  // wiiu_input.cpp: the game asked to end by itself

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
    // (2026-10-28, the GPU's time: rac1::Gx2Perf, mrp_world_gx2.h - three rows, the page's room)
    {"GPU: hierarchical Z", [] { return on_off(rac1::g_gx2_perf.hiz); }, [] { rac1::g_gx2_perf.hiz = !rac1::g_gx2_perf.hiz; }},
    {"GPU: pixel shaders", [] {
         const rac1::Gx2Perf& g = rac1::g_gx2_perf;
         return std::string(g.lean && g.nokill ? "lean+nokill" : g.lean ? "lean" : g.nokill ? "nokill" : "off");
     },
     [] {  // (off -> nokill -> lean -> both -> off)
         rac1::Gx2Perf& g = rac1::g_gx2_perf;
         const int k = ((g.lean ? 2 : 0) | (g.nokill ? 1 : 0)) + 1;
         g.nokill = (k & 1) != 0;
         g.lean = (k & 2) != 0;
     }},
    {"GPU test", [] {
         const rac1::Gx2Perf& g = rac1::g_gx2_perf;
         return std::string(g.pixels_off ? "no pixels" : !g.zclear2 ? "1 Z clear" : "off");
     },
     [] {  // (off -> the pixels off (8 x 8 scissor) -> one depth clear (WHB's) -> off)
         rac1::Gx2Perf& g = rac1::g_gx2_perf;
         if (!g.pixels_off && g.zclear2) {
             g.pixels_off = true;
         } else if (g.pixels_off) {
             g.pixels_off = false;
             g.zclear2 = false;
         } else {
             g.zclear2 = true;
         }
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
    RAC1_SKIP_ITEM("Mobys' poses (joints)", g_skip_joints),
    {"Catch-up steps drawn", [] { return on_off(!rac1::g_draw_last_step_only); }, [] { rac1::g_draw_last_step_only = !rac1::g_draw_last_step_only; }},
    {"Exit game (back to the Wii U Menu)", [] { return std::string("tap"); }, [] { rac1_wiiu_quit(); }},
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
    // (VPAD's own sample buffer: the game's VPADRead takes the latest too - reading it here takes nothing from the game's buttons)
    if (VPADRead(VPAD_CHAN_0, &v, 1, &e) != 1 || e != VPAD_READ_SUCCESS) return;
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
                    items[i].flip();
                    std::printf("[wiiu] menu: %s -> %s\n", items[i].label, items[i].state().c_str());
                }
        }
    }
    g_was_touched = touched;
}

void debug_menu_draw(const GX2ColorBuffer* cb, const std::vector<std::string>& stats) {
    if (!g_up || cb == nullptr) return;
    g_v.clear();
    static const float white[4] = {1, 1, 1, 1}, grey[4] = {0.6f, 0.6f, 0.6f, 1}, on[4] = {0.10f, 0.45f, 0.15f, 1}, off[4] = {0.45f, 0.12f, 0.10f, 1},
                       head[4] = {1.0f, 0.85f, 0.3f, 1}, tab[4] = {0.15f, 0.2f, 0.35f, 1};
    text(kItemX, 6, "rac1-native debug menu (tap an item)", head);
    size_t n = 0;
    const Item* items = page_items(&n);
    box(kItemX, kItemY, kItemW, kItemH - 4, tab);
    text(kItemX + 6, kItemY + 4, g_page == 0 ? "Page: [Picture]  Game   (tap)" : "Page:  Picture  [Game]  (tap)", head);
    for (size_t i = 0; i < n; ++i) {
        const int y = kItemY + static_cast<int>(i + 1) * kItemH;
        const std::string st = items[i].state();
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
    char b[64];
    std::snprintf(b, sizeof b, "touch %d,%d  taps %d", g_touch_x, g_touch_y, g_taps);
    text(kItemX, kScreenH - kFontCellH - 4, b, grey);
    if (g_v.empty()) return;
    void* p = GX2RLockBufferEx(&g_vb, GX2R_RESOURCE_BIND_NONE);
    std::memcpy(p, g_v.data(), g_v.size() * sizeof(V));
    GX2RUnlockBufferEx(&g_vb, GX2R_RESOURCE_BIND_NONE);
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
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, static_cast<uint32_t>(g_v.size()), 0, 1);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0x00, FALSE, TRUE);
}
