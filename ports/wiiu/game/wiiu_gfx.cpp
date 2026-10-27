// The Wii U's picture (step 3 of ports/wiiu/PLAN.md): GX2 through the WHB helpers, the TV and the GamePad, a frame drawn
// after each host frame of the game: its 2D (the GS replica's primitives, gs_draw_gx2.cpp) in a 4:3 area at the centre;
// before the game's first frame, or without the 2D's shaders, a test triangle whose colours follow the frames. RAC1_WIIU_GFX=1 turns it on (the log's screen, WHBLogConsole, is then left out: the log
// goes to the UDP log alone); the shaders are .gsh files compiled on the PC (ports/wiiu/shaders, CafeGLSL) and read
// from shaders/ beside the game's data. Host code.
#include <runtime/gs/gs_emu.h>
#include "gs_draw_gx2.h"

#include <coreinit/cache.h>
#include <gx2/draw.h>
#include <gx2/surface.h>
#include <gx2/mem.h>
#include <gx2/event.h>
#include <gx2/enum.h>
#include <gx2/shaders.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <whb/gfx.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace {
bool g_on = false;
WHBGfxShaderGroup g_test{};
GX2RBuffer g_pos{}, g_col{};
uint64_t g_frames = 0;

std::vector<uint8_t> read_file(const char* path) {
    std::vector<uint8_t> d;
    std::FILE* f = std::fopen(path, "rb");
    if (f == nullptr) return d;
    std::fseek(f, 0, SEEK_END);
    d.resize(static_cast<size_t>(std::ftell(f)));
    std::fseek(f, 0, SEEK_SET);
    if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
    std::fclose(f);
    return d;
}

bool make_buffer(GX2RBuffer& b, const float* data, uint32_t count) {
    b.flags = static_cast<GX2RResourceFlags>(GX2R_RESOURCE_BIND_VERTEX_BUFFER | GX2R_RESOURCE_USAGE_CPU_READ | GX2R_RESOURCE_USAGE_CPU_WRITE |
                                             GX2R_RESOURCE_USAGE_GPU_READ);
    b.elemSize = 4 * 4;
    b.elemCount = count;
    if (!GX2RCreateBuffer(&b)) return false;
    void* p = GX2RLockBufferEx(&b, GX2R_RESOURCE_BIND_NONE);
    std::memcpy(p, data, b.elemSize * count);
    GX2RUnlockBufferEx(&b, GX2R_RESOURCE_BIND_NONE);
    return true;
}

void draw_test() {
    // (the corners' colours turn with the game's frames: a frame drawn per host frame of the game)
    const float t = static_cast<float>(g_frames) / 60.0f;
    float col[12];
    for (int v = 0; v < 3; ++v) {
        col[4 * v + 0] = 0.5f + 0.5f * std::sin(t + 2.094f * static_cast<float>(v));
        col[4 * v + 1] = 0.5f + 0.5f * std::sin(t * 1.3f + 2.094f * static_cast<float>(v) + 1.0f);
        col[4 * v + 2] = 0.5f + 0.5f * std::sin(t * 0.7f + 2.094f * static_cast<float>(v) + 2.0f);
        col[4 * v + 3] = 1.0f;
    }
    void* p = GX2RLockBufferEx(&g_col, GX2R_RESOURCE_BIND_NONE);
    std::memcpy(p, col, sizeof col);
    GX2RUnlockBufferEx(&g_col, GX2R_RESOURCE_BIND_NONE);
    GX2SetFetchShader(&g_test.fetchShader);
    GX2SetVertexShader(g_test.vertexShader);
    GX2SetPixelShader(g_test.pixelShader);
    GX2RSetAttributeBuffer(&g_pos, 0, g_pos.elemSize, 0);
    GX2RSetAttributeBuffer(&g_col, 1, g_col.elemSize, 0);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLES, 3, 0, 1);
}

// the frame's 2D (orig_main's no-graphics branch, after the frame's chain ran on the GS replica): drawn by the next frame
struct Frame2d {
    const rac1::gs::Gs* gs = nullptr;
    float fb_w = 0, fb_h = 0;
    int screen_fbp = -1, skip_fbp = -1;
    uint64_t game_frame = 0;
    // the frame's render targets still to draw (gs_draw_gx2: once per frame of the GS replica, by the first picture drawn
    // of it; a picture of the same frame again only samples them)
    bool targets_due = false;
};
Frame2d g_2d;
bool g_2d_ready = false;  // (the GS replica's GX2 backend up)
}  // namespace

bool rac1_wiiu_picture() { return g_on; }
void rac1_wiiu_picture_frame(const rac1::gs::Gs& gs, float fb_w, float fb_h, int screen_fbp, int skip_fbp, uint64_t game_frame) {
    g_2d.game_frame = game_frame;
    g_2d.gs = &gs;
    g_2d.fb_w = fb_w;
    g_2d.fb_h = fb_h;
    g_2d.screen_fbp = screen_fbp;
    g_2d.skip_fbp = skip_fbp;
    g_2d.targets_due = true;
}

// the game over (its main returned): its 2D no longer drawn, the frames on the way back to the menu the test's
void wiiu_gfx_game_ended() {
    g_2d.gs = nullptr;
    // (the 2D's backend let go before the menu takes over: with it up, the console hung on its way back)
    if (g_2d_ready) rac1::gs::draw_gx2_shutdown();
    g_2d_ready = false;
}

bool wiiu_gfx_wanted() {
    const char* g = std::getenv("RAC1_WIIU_GFX");
    return g != nullptr && g[0] == '1';
}

// GX2 up, the test's shaders and buffers made (false: no picture, the game runs on as without graphics)
bool wiiu_gfx_start() {
    if (!WHBGfxInit()) {
        std::printf("[wiiu] gfx: WHBGfxInit failed\n");
        return false;
    }
    const std::vector<uint8_t> gsh = read_file("shaders/test.gsh");
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_test, 0, gsh.data())) {
        std::printf("[wiiu] gfx: shaders/test.gsh not loaded\n");
        WHBGfxShutdown();
        return false;
    }
    WHBGfxInitShaderAttribute(&g_test, "a_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitShaderAttribute(&g_test, "a_col", 1, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32);
    WHBGfxInitFetchShader(&g_test);
    const float pos[12] = {0.0f, 0.7f, 0.0f, 1.0f, -0.7f, -0.6f, 0.0f, 1.0f, 0.7f, -0.6f, 0.0f, 1.0f};
    const float col[12] = {1, 0, 0, 1, 0, 1, 0, 1, 0, 0, 1, 1};
    if (!make_buffer(g_pos, pos, 3) || !make_buffer(g_col, col, 3)) {
        std::printf("[wiiu] gfx: buffers not made\n");
        WHBGfxShutdown();
        return false;
    }
    g_2d_ready = rac1::gs::draw_gx2_init();
    g_on = true;
    std::printf("[wiiu] gfx: GX2 up; the game's 2D %s\n", g_2d_ready ? "drawn (shaders/gs_draw.gsh)" : "not drawn (no shaders/gs_draw.gsh): the test triangle");
    return true;
}

namespace {
// the game's frame (4:3) centred on a colour buffer and its 2D drawn (the test triangle until the game's first frame)
void draw_frame(const GX2ColorBuffer* cb, bool targets) {
    if (g_2d.gs == nullptr || !g_2d_ready) {
        if (!g_2d_ready) draw_test();
        return;
    }
    const int w = static_cast<int>(cb->surface.width), h = static_cast<int>(cb->surface.height);
    rac1::gs::Viewport vp;
    vp.h = h;
    vp.w = h * 4 / 3;
    vp.x = (w - vp.w) / 2;
    vp.y = 0;
    vp.screen_fbp = g_2d.screen_fbp;
    vp.skip_fbp = g_2d.skip_fbp;
    vp.sprite_clamp = true;
    vp.hide_edge_slivers = true;
    rac1::gs::draw_gx2(*g_2d.gs, cb, g_2d.fb_w, g_2d.fb_h, &vp, targets);  // (the render targets: by the TV's first call per frame)
}
}  // namespace

namespace {
// (testing, RAC1_WIIU_SHOT=<n>[,<n>...]: the TV's picture of the first host frame at or past those game frames written to
// shot_<n>.ppm on the SD card, copied by the GPU into a linear surface first)
// the shot due at game frame `n` (0: none); each one taken once
uint64_t shot_wanted(uint64_t n) {
    static const char* list = std::getenv("RAC1_WIIU_SHOT");
    static uint64_t taken = 0;  // (the last one taken)
    if (list == nullptr) return 0;
    uint64_t due = 0;
    for (const char* q = list; *q != 0;) {
        char* e = nullptr;
        const unsigned long long v = std::strtoull(q, &e, 10);
        if (e == q) break;
        if (v <= n && v > taken && (due == 0 || v < due)) due = v;
        q = *e == ',' ? e + 1 : e;
    }
    if (due != 0) taken = due;
    return due;
}
void shot(const GX2ColorBuffer* cb, uint64_t n) {
    GX2Surface lin = cb->surface;
    lin.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
    lin.aa = GX2_AA_MODE1X;
    lin.mipLevels = 1;
    lin.mipmaps = nullptr;
    lin.mipmapSize = 0;
    lin.swizzle = 0;
    GX2CalcSurfaceSizeAndAlignment(&lin);
    lin.image = std::aligned_alloc(lin.alignment, (lin.imageSize + lin.alignment - 1) / lin.alignment * lin.alignment);
    if (lin.image == nullptr) return;
    DCFlushRange(lin.image, lin.imageSize);
    GX2CopySurface(&cb->surface, 0, 0, &lin, 0, 0);
    GX2DrawDone();
    DCInvalidateRange(lin.image, lin.imageSize);
    char path[64];
    std::snprintf(path, sizeof path, "shot_%llu.ppm", static_cast<unsigned long long>(n));
    if (std::FILE* f = std::fopen(path, "wb")) {
        std::fprintf(f, "P6\n%u %u\n255\n", lin.width, lin.height);
        std::vector<uint8_t> row(lin.width * 3);
        for (uint32_t y = 0; y < lin.height; ++y) {
            const uint8_t* src = static_cast<const uint8_t*>(lin.image) + static_cast<size_t>(y) * lin.pitch * 4;
            for (uint32_t x = 0; x < lin.width; ++x) {
                row[3 * x] = src[4 * x];
                row[3 * x + 1] = src[4 * x + 1];
                row[3 * x + 2] = src[4 * x + 2];
            }
            std::fwrite(row.data(), 1, row.size(), f);
        }
        std::fclose(f);
        std::printf("[wiiu] gfx: %s written (%ux%u)\n", path, lin.width, lin.height);
    }
    std::free(lin.image);
}
}  // namespace

// a frame for the TV and the GamePad, after a host frame of the game
void wiiu_gfx_frame() {
    if (!g_on) return;
    ++g_frames;
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    draw_frame(WHBGfxGetTVColourBuffer(), g_2d.targets_due);
    g_2d.targets_due = false;
    WHBGfxFinishRenderTV();
    if (const uint64_t due = shot_wanted(g_2d.game_frame)) shot(WHBGfxGetTVColourBuffer(), due);
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    draw_frame(WHBGfxGetDRCColourBuffer(), false);
    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();
}

void wiiu_gfx_stop() {
    if (!g_on) return;
    if (g_2d_ready) rac1::gs::draw_gx2_shutdown();
    GX2RDestroyBufferEx(&g_pos, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&g_col, GX2R_RESOURCE_BIND_NONE);
    WHBGfxFreeShaderGroup(&g_test);
    WHBGfxShutdown();
    g_on = false;
}
