// The Wii U's picture (step 3 of ports/wiiu/PLAN.md): GX2 through the WHB helpers, the TV and the GamePad, a frame drawn
// after each host frame of the game: the MRP's world and mobys, then its 2D (the MRP chain reader's primitives, mrp_2d_gx2.cpp; with
// RAC1_WIIU_REPLICA=1 the GS replica's, gs_draw_gx2.cpp) in a 4:3 area at the centre;
// before the game's first frame, or without the 2D's shaders, a test triangle whose colours follow the frames. RAC1_WIIU_GFX=1 turns it on (the log's screen, WHBLogConsole, is then left out: the log
// goes to the UDP log alone); the shaders are .gsh files compiled on the PC (ports/wiiu/shaders, CafeGLSL) and read
// from shaders/ beside the game's data. Host code.
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_chain.h>
#include <runtime/gs/gs_emu.h>
#include "gs_draw_gx2.h"
#include "mrp_2d_gx2.h"
#include "mrp_moby_gx2.h"
#include "mrp_world_gx2.h"

#include <coreinit/cache.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/surface.h>
#include <gx2/mem.h>
#include <gx2/state.h>
#include <coreinit/debug.h>
#include <whb/log.h>
#include <gx2/display.h>
#include <gx2/event.h>
#include <gx2/swap.h>
#include <gx2/enum.h>
#include <gx2/shaders.h>
#include <gx2r/buffer.h>
#include <gx2r/draw.h>
#include <whb/gfx.h>

#include <cmath>
#include <cstdint>
#include <cstdio>
#include <algorithm>
#include <chrono>
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

// the frame's 2D (orig_main's no-graphics branch, after the frame's chain was read - by the MRP's chain reader, or the GS replica):
// drawn by the next frame
struct Frame2d {
    bool have = false;                     // (a frame given)
    const rac1::gs::Gs* replica = nullptr;  // (the GS replica walked it: RAC1_WIIU_REPLICA=1; else mrp_chain_frame)
    float fb_w = 0, fb_h = 0;
    int screen_fbp = -1, skip_fbp = -1;
    uint64_t game_frame = 0;
    // the frame's render targets still to draw (gs_draw_gx2: once per frame of the GS replica, by the first picture drawn
    // of it; a picture of the same frame again only samples them)
    bool targets_due = false;
};
Frame2d g_2d;
bool g_2d_ready = false;  // (the GS replica's GX2 backend up: RAC1_WIIU_REPLICA=1)
bool g_m2d_ready = false;  // (the MRP 2D's GX2 backend up)
bool g_mobys_ready = false;  // (the MRP's mobys' GX2 backend up)
bool g_world_ready = false;  // (the MRP world's ties / shrubs' GX2 backend up)
}  // namespace

bool rac1_wiiu_picture() { return g_on; }
void rac1_wiiu_picture_frame(const rac1::gs::Gs* replica, float fb_w, float fb_h, int screen_fbp, int skip_fbp, uint64_t game_frame) {
    g_2d.game_frame = game_frame;
    g_2d.have = true;
    g_2d.replica = replica;
    g_2d.fb_w = fb_w;
    g_2d.fb_h = fb_h;
    g_2d.screen_fbp = screen_fbp;
    g_2d.skip_fbp = skip_fbp;
    g_2d.targets_due = true;
}

// the game over (its main returned): its 2D no longer drawn, the frames on the way back to the menu the test's
namespace {
bool g_ending = false;  // (the game ended: wiiu_gfx_game_ended)
OSTime g_frame_ts = 0;  // (the last frame's commands, GX2GetLastSubmittedTimeStamp: waited for before the next frame's writes)
}  // namespace

void wiiu_gfx_game_ended() {
    // (the GPU may still be drawing the last frame - the frames overlap the next one's work, wiiu_gfx_frame -: waited for before the
    // backends free what it reads)
    // (each step on the log: the way back to the menu hung - 2026-10-28 - and the log says where)
    auto step = [](const char* m) {
        WHBLogPrintf("rac1-wiiu: ending: %s", m);
        OSReport("rac1-wiiu: ending: %s\n", m);
    };
    step("the GPU's last frame");
    if (g_on && g_frame_ts != 0) GX2WaitTimeStamp(g_frame_ts);
    step("waited");
    g_frame_ts = 0;
    g_ending = true;  // (the frames from here to ProcUI's release: no timestamp waited for, WHBGfxFinishRender's own wait - the
                      // application leaving the foreground, a frame's commands may never retire: waiting for them hung the console)
    g_2d.have = false;
    g_2d.replica = nullptr;
    // (the 2D's backend let go before the menu takes over: with it up, the console hung on its way back)
    if (g_2d_ready) rac1::gs::draw_gx2_shutdown();
    g_2d_ready = false;
    step("2D (replica) down");
    if (g_m2d_ready) rac1::mrp_2d_gx2_shutdown();
    g_m2d_ready = false;
    step("2D down");
    if (g_mobys_ready) rac1::mrp_moby_gx2_shutdown();
    g_mobys_ready = false;
    step("mobys down");
    if (g_world_ready) {
        rac1::mrp_world_wiiu_join();  // (the preparation's worker idle before the world's data goes)
        rac1::mrp_world_gx2_shutdown();
    }
    g_world_ready = false;
    step("world down");
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
    // the 2D: the MRP's (shaders/mrp_2d.gsh); the GS replica's backend only when the replica walks the chain (RAC1_WIIU_REPLICA=1, orig_main)
    const char* rep = std::getenv("RAC1_WIIU_REPLICA");
    if (rep != nullptr && std::atoi(rep) != 0) g_2d_ready = rac1::gs::draw_gx2_init();
    else g_m2d_ready = rac1::mrp_2d_gx2_init();
    g_mobys_ready = rac1::mrp_moby_gx2_init();
    std::printf("[wiiu] gfx: the MRP's mobys %s\n", g_mobys_ready ? "drawn (shaders/mrp_moby.gsh)" : "not drawn (no shaders/mrp_moby.gsh)");
    // the MRP world's ties / shrubs (RAC1_WIIU_WORLD=0: not drawn, the static path's snapshot not taken)
    const char* world = std::getenv("RAC1_WIIU_WORLD");
    g_world_ready = (world == nullptr || std::atoi(world) != 0) && rac1::mrp_world_gx2_init();
    std::printf("[wiiu] gfx: the MRP world's ties / shrubs %s\n", g_world_ready ? "drawn (shaders/mrp_world.gsh)" : "not drawn (RAC1_WIIU_WORLD=0 or no shaders/mrp_world.gsh)");
    g_on = true;
    std::printf("[wiiu] gfx: GX2 up; the game's 2D %s\n", g_m2d_ready ? "drawn by the MRP's 2D (shaders/mrp_2d.gsh)"
                                                           : g_2d_ready ? "drawn by the GS replica's backend (RAC1_WIIU_REPLICA=1, shaders/gs_draw.gsh)"
                                                                        : "not drawn (no shaders/mrp_2d.gsh, or gs_draw.gsh with RAC1_WIIU_REPLICA=1): the test triangle");
    return true;
}

// (testing, RAC1_WIIU_TIMING=1: a host frame's time by stage, averaged over 60 host frames on the log - the game's thread's
// clock: the stages orig_main.cpp times (rac1_wiiu_time), the picture's draws by backend, the GX2 waits, and the rest of the frame
// - the game's steps)
namespace {
enum TimeSlot { kTmChain, kTmMobyPrep, kTmWorldPrep, kTmWorldJoin, kTmGpuWait, kTmJoin, kTmBegin, kTmWorld, kTmMobys, kTm2d, kTmFinishTv, kTmDrc, kTmFinish, kTmN };
const char* const kTmName[kTmN] = {"chain start", "mobys' prepare", "world's prepare", "world's worker join", "GPU wait (the frame before)", "chain join", "begin (swap wait)",
                                   "world draws", "moby draws", "2D draws", "TV finish", "GamePad", "finish"};
double g_tm[kTmN] = {}, g_tm_total = 0.0;
uint64_t g_tm_frames = 0;
std::chrono::steady_clock::time_point g_tm_last{};
bool tm_on() {
    static const bool on = std::getenv("RAC1_WIIU_TIMING") != nullptr && std::atoi(std::getenv("RAC1_WIIU_TIMING")) != 0;
    return on;
}
double ms_since(std::chrono::steady_clock::time_point t) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); }
}  // namespace

// orig: (host) a stage's time of this host frame (orig_main.cpp's Wii U branch: kTmChain, kTmMobyPrep, kTmWorldPrep)
void rac1_wiiu_time(int slot, double ms) {
    if (tm_on() && slot >= 0 && slot < kTmN) g_tm[slot] += ms;
}


namespace {
// the game's frame (4:3) centred on a colour buffer and its 2D drawn (the test triangle until the game's first frame)
bool g_tm_draw = false;  // (RAC1_WIIU_TIMING: the TV's call timed by backend)
void draw_frame(const GX2ColorBuffer* cb, GX2DepthBuffer* db, bool targets) {
    // (testing, RAC1_WIIU_2D: 0 the frame's chain read but nothing of it drawn, 1 the screen's primitives only: no target,
    // 3 the MRP's 3D alone - the world's ties / shrubs and the mobys -: no 2D; the same with the replica's backend)
    static const int s_2d = std::getenv("RAC1_WIIU_2D") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_2D")) : 2;
    if (s_2d == 0) return;
    if (s_2d == 1) targets = false;
    const bool ready = g_2d.replica != nullptr ? g_2d_ready : g_m2d_ready;
    if (!g_2d.have || !ready) {
        if (!g_2d_ready && !g_m2d_ready) draw_test();
        return;
    }
    const int w = static_cast<int>(cb->surface.width), h = static_cast<int>(cb->surface.height);
    rac1::gs::Viewport vp;
    // (testing, RAC1_WIIU_VPSCALE=<0.1..1>: the game's frame drawn that much smaller, centred - the pixels' share of a frame's cost)
    static const float vps = std::getenv("RAC1_WIIU_VPSCALE") != nullptr ? std::clamp(static_cast<float>(std::atof(std::getenv("RAC1_WIIU_VPSCALE"))), 0.1f, 1.0f) : 1.0f;
    vp.h = static_cast<int>(static_cast<float>(h) * vps);
    vp.w = vp.h * 4 / 3;
    vp.x = (w - vp.w) / 2;
    vp.y = (h - vp.h) / 2;
    vp.screen_fbp = g_2d.screen_fbp;
    vp.skip_fbp = g_2d.skip_fbp;
    vp.sprite_clamp = true;
    vp.hide_edge_slivers = true;
    // the 3D under the 2D: the depth cleared once (the world and the mobys share it), the world's ties / shrubs, then the mobys.
    // (testing, RAC1_WIIU_ZCLEAR=0: the depth not cleared here - WHBGfxClearColor clears it too, when called before)
    static const bool zclear = std::getenv("RAC1_WIIU_ZCLEAR") == nullptr || std::atoi(std::getenv("RAC1_WIIU_ZCLEAR")) != 0;
    if (db != nullptr && zclear && (g_mobys_ready || g_world_ready)) {
        GX2ClearDepthStencilEx(db, 1.0f, 0, GX2_CLEAR_FLAGS_DEPTH);
        // (the clear changes the GPU's state: the context of `cb` set again, the TV's or the GamePad's)
        GX2ContextState* ctx = cb == WHBGfxGetTVColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();
        if (ctx != nullptr) GX2SetContextState(ctx);
    }
    auto dt = std::chrono::steady_clock::now();
    auto dlap = [&](int slot) {
        if (!g_tm_draw) return;
        g_tm[slot] += ms_since(dt);
        dt = std::chrono::steady_clock::now();
    };
    if (g_world_ready) {
        rac1::mrp_world_wiiu_join();  // (the frame's preparation's second half, on its worker core)
        dlap(kTmWorldJoin);
        rac1::mrp_world_gx2_debug_frame(g_2d.game_frame);
        rac1::mrp_world_gx2_draw(cb, db, vp);
        dlap(kTmWorld);
    }
    if (g_mobys_ready) {
        rac1::mrp_moby_gx2_debug_frame(g_2d.game_frame);
        rac1::mrp_moby_gx2_draw(cb, db, vp);
        dlap(kTmMobys);
    }
    if (s_2d == 3) {  // (testing, RAC1_WIIU_TEST_AFTER=1: the test triangle drawn over the mobys, the target checked)
        static const bool test_after = std::getenv("RAC1_WIIU_TEST_AFTER") != nullptr && std::atoi(std::getenv("RAC1_WIIU_TEST_AFTER")) != 0;
        if (test_after) draw_test();
        return;
    }
    // the 2D (the render targets: by the TV's first call per frame), after the world and the mobys (no depth: over them)
    if (g_2d.replica != nullptr) rac1::gs::draw_gx2(*g_2d.replica, cb, g_2d.fb_w, g_2d.fb_h, &vp, targets);
    else rac1::mrp_2d_gx2_draw(rac1::mrp_chain_frame(), cb, g_2d.fb_w, g_2d.fb_h, vp, targets);
    dlap(kTm2d);
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
    const bool tm = tm_on();
    if (tm) {  // (the host frame: from the last one's start to this one's)
        const auto now = std::chrono::steady_clock::now();
        if (g_tm_last.time_since_epoch().count() != 0) {
            g_tm_total += std::chrono::duration<double, std::milli>(now - g_tm_last).count();
            if (++g_tm_frames == 60) {
                double known = 0.0;
                std::printf("[timing] 60 host frames, %.1f ms each:", g_tm_total / 60.0);
                for (int i = 0; i < kTmN; ++i) {
                    std::printf(" %s %.1f,", kTmName[i], g_tm[i] / 60.0);
                    known += g_tm[i];
                    g_tm[i] = 0.0;
                }
                std::printf(" the rest (the game's steps) %.1f\n", (g_tm_total - known) / 60.0);
                g_tm_total = 0.0;
                g_tm_frames = 0;
            }
        }
        g_tm_last = now;
    }
    auto t = std::chrono::steady_clock::now();
    auto lap = [&](int slot) {
        if (!tm) return;
        g_tm[slot] += ms_since(t);
        t = std::chrono::steady_clock::now();
    };
    // the GPU draws a frame while the game's thread runs the next steps: the frame before's commands waited for here, before
    // this frame writes the memory the GPU reads (the backends' rings, the per-frame buffers) - not at the frame's end as
    // WHBGfxFinishRender does (GX2DrawDone). (testing: RAC1_WIIU_OVERLAP=0 waits at the end as before)
    static const bool overlap_on = std::getenv("RAC1_WIIU_OVERLAP") == nullptr || std::atoi(std::getenv("RAC1_WIIU_OVERLAP")) != 0;
    const bool overlap = overlap_on && !g_ending;
    if (overlap && g_frame_ts != 0) GX2WaitTimeStamp(g_frame_ts);
    lap(kTmGpuWait);
    if (g_frames % 300 == 0)  // (the memory left, on the log: a leak shows)
        std::printf("[wiiu] host frame %llu: %u KB free in the default heap\n", static_cast<unsigned long long>(g_frames),
                    static_cast<unsigned>(MEMGetTotalFreeSizeForExpHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2)) / 1024));
    if (g_2d.have && g_2d.replica == nullptr) rac1::mrp_chain_join();  // (the frame's chain read on the reader's worker)
    lap(kTmJoin);
    WHBGfxBeginRender();
    lap(kTmBegin);
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    g_tm_draw = tm;
    draw_frame(WHBGfxGetTVColourBuffer(), WHBGfxGetTVDepthBuffer(), g_2d.targets_due);
    g_tm_draw = false;
    t = std::chrono::steady_clock::now();
    g_2d.targets_due = false;
    WHBGfxFinishRenderTV();
    lap(kTmFinishTv);
    if (const uint64_t due = shot_wanted(g_2d.game_frame)) shot(WHBGfxGetTVColourBuffer(), due);
    // (testing, RAC1_WIIU_DRC=0: the GamePad's screen cleared only, the frame drawn for the TV alone)
    static const bool drc = std::getenv("RAC1_WIIU_DRC") == nullptr || std::atoi(std::getenv("RAC1_WIIU_DRC")) != 0;
    WHBGfxBeginRenderDRC();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    if (drc) draw_frame(WHBGfxGetDRCColourBuffer(), WHBGfxGetDRCDepthBuffer(), false);
    WHBGfxFinishRenderDRC();
    lap(kTmDrc);
    if (overlap) {  // (WHBGfxFinishRender without its GX2DrawDone)
        GX2SwapScanBuffers();
        GX2Flush();
        GX2SetTVEnable(TRUE);
        GX2SetDRCEnable(TRUE);
        g_frame_ts = GX2GetLastSubmittedTimeStamp();
    } else {
        WHBGfxFinishRender();
    }
    lap(kTmFinish);
}

void wiiu_gfx_stop() {
    if (!g_on) return;
    if (g_2d_ready) rac1::gs::draw_gx2_shutdown();
    if (g_m2d_ready) rac1::mrp_2d_gx2_shutdown();
    if (g_mobys_ready) rac1::mrp_moby_gx2_shutdown();
    if (g_world_ready) rac1::mrp_world_gx2_shutdown();
    GX2RDestroyBufferEx(&g_pos, GX2R_RESOURCE_BIND_NONE);
    GX2RDestroyBufferEx(&g_col, GX2R_RESOURCE_BIND_NONE);
    WHBGfxFreeShaderGroup(&g_test);
    WHBGfxShutdown();
    g_on = false;
}
