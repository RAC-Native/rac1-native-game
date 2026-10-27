// The Wii U's picture (step 3 of ports/wiiu/PLAN.md, phase 0): GX2 through the WHB helpers, the TV and the GamePad, a
// frame drawn after each host frame of the game. For now a test: the screen cleared and one triangle whose colours
// follow the game's frame count. RAC1_WIIU_GFX=1 turns it on (the log's screen, WHBLogConsole, is then left out: the log
// goes to the UDP log alone); the shaders are .gsh files compiled on the PC (ports/wiiu/shaders, CafeGLSL) and read
// from shaders/ beside the game's data. Host code.
#include <gx2/draw.h>
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
}  // namespace

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
    g_on = true;
    std::printf("[wiiu] gfx: GX2 up, the test triangle\n");
    return true;
}

// a frame for the TV and the GamePad, after a host frame of the game
void wiiu_gfx_frame() {
    if (!g_on) return;
    ++g_frames;
    WHBGfxBeginRender();
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.05f, 0.08f, 0.15f, 1.0f);
    draw_test();
    WHBGfxFinishRenderTV();
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.05f, 0.08f, 0.15f, 1.0f);
    draw_test();
    WHBGfxFinishRenderDRC();
    WHBGfxFinishRender();
}

void wiiu_gfx_stop() {
    if (!g_on) return;
    GX2RDestroyBufferEx(&g_pos, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&g_col, GX2R_RESOURCE_BIND_NONE);
    WHBGfxFreeShaderGroup(&g_test);
    WHBGfxShutdown();
    g_on = false;
}
