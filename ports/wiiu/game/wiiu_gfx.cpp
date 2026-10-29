// The Wii U's picture (step 3 of ports/wiiu/PLAN.md): GX2 through the WHB helpers, the TV and the GamePad, a frame drawn
// after each host frame of the game: the MRP's world and mobys, then its 2D (the MRP chain reader's primitives, mrp_2d_gx2.cpp; with
// RAC1_WIIU_REPLICA=1 the GS replica's, gs_draw_gx2.cpp) in a 4:3 area at the centre;
// before the game's first frame, or without the 2D's shaders, a test triangle whose colours follow the frames. RAC1_WIIU_GFX=1 turns it on (the log's screen, WHBLogConsole, is then left out: the log
// goes to the UDP log alone); the shaders are .gsh files compiled on the PC (ports/wiiu/shaders, CafeGLSL) and read
// from shaders/ beside the game's data. Host code.
#include <game/enhance/advanced.h>
#include <game/enhance/graphics.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_chain.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/ipu/movie_hw.h>
#include <profiler/host_profiler.h>
#include "gs_draw_gx2.h"
#include "mrp_2d_gx2.h"
#include "debug_menu.h"
#include "mrp_moby_gx2.h"
#include "mrp_world_gx2.h"

#include <malloc.h>

#include <coreinit/cache.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/time.h>
#include <coreinit/memexpheap.h>
#include <coreinit/memheap.h>
#include <gx2/clear.h>
#include <gx2/context.h>
#include <gx2/draw.h>
#include <gx2/surface.h>
#include <gx2/mem.h>
#include <gx2/registers.h>
#include <gx2/sampler.h>
#include <gx2/state.h>
#include <gx2/texture.h>
#include <gx2/utils.h>
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
    // ((2026-10-28) what of the game's state the backends' draws read, taken now: the game thread may run the next steps beside the
    // drawing - orig_main.cpp's Wii U branch)
    if (g_on) rac1::gx2_draw_view_take();
}

// the game over (its main returned): its 2D no longer drawn, the frames on the way back to the menu the test's
namespace {
bool g_ending = false;  // (the game ended: wiiu_gfx_game_ended)
OSTime g_frame_ts = 0;  // (the last frame's commands, GX2GetLastSubmittedTimeStamp: waited for before the next frame's writes)
// the foreground ours (ProcUI's release / acquire callbacks, wiiu_main.cpp). Released - the HOME menu, the exit - WHB's own
// release callback has waited for the GPU (GX2DrawDone) and freed the TV's and the GamePad's buffers (MEM1, the foreground's):
// no frame drawn into them until the acquire. (The game's host frame after the release still came here - WHBProcIsRunning
// returns true from the release itself -, and the frames on the way back to the menu, 2026-10-28)
bool g_foreground = true;
}  // namespace

void wiiu_gfx_foreground(bool on) {
    g_foreground = on;
    if (!on) g_frame_ts = 0;  // (the GPU idle: WHB's release callback's GX2DrawDone)
}

void wiiu_gfx_game_ended() {
    // (the GPU may still be drawing the last frame - the frames overlap the next one's work, wiiu_gfx_frame -: waited for before the
    // backends free what it reads)
    // (each step on the log: the way back to the menu hung - 2026-10-28 - and the log says where)
    auto step = [](const char* m) {
        WHBLogPrintf("rac1-wiiu: ending: %s", m);
        OSReport("rac1-wiiu: ending: %s\n", m);
    };
    step("the GPU's last frame");
    if (g_on && g_foreground && g_frame_ts != 0) GX2WaitTimeStamp(g_frame_ts);  // (released: the GPU idle already)
    step("waited");
    g_frame_ts = 0;
    g_ending = true;  // (the frames from here to ProcUI's release: no timestamp waited for, WHBGfxFinishRender's own wait - the
                      // application leaving the foreground, a frame's commands may never retire: waiting for them hung the console)
    g_2d.have = false;
    g_2d.replica = nullptr;
    // (the backends' memory not freed: the system takes back all of the application's memory when it ends, and freeing it here -
    // the 2D's hundreds of textures after a long game, a full heap - took tens of seconds while ProcUI waited for the foreground:
    // the console seemed hung on its way back, 2026-10-28. Only the world's worker is stopped, before the process goes)
    if (g_world_ready) rac1::mrp_world_wiiu_join();
    g_2d_ready = g_m2d_ready = g_mobys_ready = g_world_ready = false;
    step("backends left (their memory the system's)");
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
    if (debug_menu_wanted()) debug_menu_init();  // (the GamePad's debug menu, RAC1_WIIU_MENU=1)
    std::printf("[wiiu] gfx: GX2 up; the game's 2D %s\n", g_m2d_ready ? "drawn by the MRP's 2D (shaders/mrp_2d.gsh)"
                                                           : g_2d_ready ? "drawn by the GS replica's backend (RAC1_WIIU_REPLICA=1, shaders/gs_draw.gsh)"
                                                                        : "not drawn (no shaders/mrp_2d.gsh, or gs_draw.gsh with RAC1_WIIU_REPLICA=1): the test triangle");
    return true;
}

// (testing, RAC1_WIIU_TIMING=1: a host frame's time by stage, averaged over 60 host frames on the log - the game's thread's
// clock: the stages orig_main.cpp times (rac1_wiiu_time), the picture's draws by backend, the GX2 waits, and the rest of the frame
// - the game's steps)
namespace {
// (slots 0..3 orig_main.cpp's: rac1_wiiu_time(0 / 1 / 2 / 3); the sky's two, 2026-10-28: its capture on the game's thread, its draws.
// (2026-10-28) the game thread's, orig_main.cpp's too: kTmJoin (6) also the chain's join before the game thread starts, kTmGameWait (15)
// the main thread's wait for the game thread after its drawing, kTmGameSteps (16) the steps the game thread ran beside the drawing - not
// the main thread's time: out of the sum, its own figure on the line)
enum TimeSlot { kTmChain, kTmMobyPrep, kTmWorldPrep, kTmSkyCapture, kTmWorldJoin, kTmGpuWait, kTmJoin, kTmBegin, kTmSky, kTmWorld, kTmMobys, kTm2d, kTmFinishTv, kTmDrc, kTmFinish,
                kTmGameWait, kTmGameSteps, kTm2dBuild, kTmN };
const char* const kTmName[kTmN] = {"chain start", "mobys' prepare", "world's prepare", "sky's capture", "world's worker join", "GPU wait (the frame before)", "chain join",
                                   "begin (swap wait)", "sky draws", "world draws", "moby draws", "2D draws", "TV finish", "GamePad", "finish", "game thread join",
                                   "game steps beside the drawing", "2D build (before the GPU wait)"};
uint64_t g_tm_gt_frames = 0;  // (the window's host frames whose steps ran on the game thread: kTmGameSteps reported)
double g_tm[kTmN] = {}, g_tm_total = 0.0;
double g_tm_mark[kTmN] = {};  // (the stages' sums at the host frame's start: a hitch's own share)
std::vector<std::string> g_menu_stats;  // (the GamePad's debug menu: the last window's times)
uint64_t g_tm_steps_mark = 0;  // (the game frame at the window's start)
uint64_t g_tm_frames = 0;
std::chrono::steady_clock::time_point g_tm_last{};
bool tm_on() {
    static const bool on = (std::getenv("RAC1_WIIU_TIMING") != nullptr && std::atoi(std::getenv("RAC1_WIIU_TIMING")) != 0) || debug_menu_wanted();
    return on;
}
double ms_since(std::chrono::steady_clock::time_point t) { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t).count(); }
}  // namespace

// orig: (host) a stage's time of this host frame (orig_main.cpp's Wii U branch: kTmChain, kTmMobyPrep, kTmWorldPrep)
void rac1_wiiu_time(int slot, double ms) {
    if (tm_on() && slot >= 0 && slot < kTmN) g_tm[slot] += ms;
    if (tm_on() && slot == kTmGameSteps) ++g_tm_gt_frames;
}

// (testing, the timing on - RAC1_WIIU_TIMING=1 or the debug menu -, 2026-10-28: the GPU's time) the TV frame's passes timed by the GPU
// itself: at each mark two samples of the GPU's clock, a bottom-of-pipe one (GX2SampleBottomGPUCycle: written once everything before it
// has been drawn) and a top-of-pipe one (GX2SampleTopGPUCycle: written when the command processor reaches it), into a ring of four
// frames read two frames later (by then the frame before was waited for: no stall). A pass's time is the bottoms' difference; the CP's
// lead (bottom - top at a mark) says how far the commands ran ahead of the drawing - near 0 the GPU waited for the CPU's commands, and
// the pass's time is then the CPU's pace, not the GPU's. Averaged per timing window: the log's "[gpu]" line, the menu's timing page.
extern "C" void GX2SampleTopGPUCycle(uint64_t* result);  // (gx2.rpl: wut has the imports, not the declarations)
extern "C" void GX2SampleBottomGPUCycle(uint64_t* result);
namespace {
enum GpuMark { kGmStart, kGmClear, kGmZclear, kGmSky, kGmWorld, kGmMobys, kGmUpscale, kGm2d, kGmTvCopy, kGmDrc, kGmSwap, kGmN };
const char* const kGmName[kGmN] = {"start", "clear (WHB)", "depth clear", "sky", "world", "mobys", "3D upscale", "2D", "TV copy", "GamePad", "swap"};
struct alignas(64) GpuSlot {  // (a cache line each: the GPU writes it, the CPU only reads it - no CPU line written back over it)
    uint64_t v;
    uint8_t pad[56];
};
constexpr int kGpuRing = 4;
GpuSlot* g_gslots = nullptr;       // [kGpuRing][2: top, bottom][kGmN], MEM2
uint64_t g_gframe[kGpuRing] = {};  // (the host frame whose marks a set holds; 0 none)
uint32_t g_gmarked[kGpuRing] = {}; // (its marks made, bit per GpuMark)
int g_gcur = -1;                   // (the set this host frame marks; -1 none)
double g_gsum[kGmN] = {}, g_glead[kGmN] = {}, g_gbusy = 0.0, g_ggap = 0.0;
uint64_t g_gn = 0, g_glate = 0, g_gbad = 0, g_gswapped = 0, g_gnperiod = 0;
uint64_t g_gprev_end = 0, g_gprev_frame = 0;  // (the last set read: its swap's bottom, its host frame)
GpuSlot& gpu_slot(int set, int bottom, int mark) { return g_gslots[(set * 2 + bottom) * kGmN + mark]; }
// (the samples count the GPU's 27 MHz clock - its raw values on the console, 2026-10-28: a frame's passes 0.8-1 M ticks apart, as 30-36 ms;
// GX2GPUTimeToCPUTime gave nonsense, 27 M ticks as 3.4e11 ms)
double gpu_ms(uint64_t d) { return static_cast<double>(d) / 27000.0; }
void gpu_mark(int mark) {
    if (g_gcur < 0) return;
    GpuSlot& t = gpu_slot(g_gcur, 0, mark);
    GpuSlot& b = gpu_slot(g_gcur, 1, mark);
    t.v = ~0ull;  // (not there yet)
    b.v = ~0ull;
    DCFlushRange(&t, sizeof t);
    DCFlushRange(&b, sizeof b);
    GX2SampleTopGPUCycle(&t.v);
    GX2SampleBottomGPUCycle(&b.v);
    DCFlushRange(&t, sizeof t);  // (whatever GX2 wrote there itself out before the GPU writes it)
    DCFlushRange(&b, sizeof b);
    g_gmarked[g_gcur] |= 1u << mark;
}
// ((2026-10-28, the world's GPU time) RAC1_WIIU_GPU_TIME=2: inside the sky's and the world's passes a bottom and a top sample at the end of
// each pass of each batch (rac1::wiiu_gpu_submark, its kind: mrp_world_gx2.h Gx2SubMark), read with the frame's marks. Per kind the
// GPU's time since the sample before (the bottoms') and the command processor's gap: the top minus the bottom before when positive - the
// GPU had drawn everything before and the CP had not reached the mark yet (its commands not there: the CPU's pace, or not yet sent);
// near 0 the GPU drew those passes with their commands waiting. The log's "[gpu-world]" line)
constexpr int kSubMax = 256;
constexpr int kSubKinds = rac1::kSmN + 1;  // (the last: from the last mark to the world's end - the state's reset, the passes past kSubMax)
const char* const kSubName[kSubKinds] = {"sky pass 1", "sky pass 2", "tfrag pass 1", "tfrag pass 2", "ties/shrubs pass 1", "ties/shrubs pass 2", "the rest"};
bool g_gsub_on = false;
GpuSlot* g_gsub = nullptr;  // [kGpuRing][2: top, bottom][kSubMax], MEM2
uint8_t g_gsub_kind[kGpuRing][kSubMax] = {};
int g_gsub_n[kGpuRing] = {};
uint64_t g_gsub_over = 0;  // (frames whose marks went past kSubMax)
double g_gsub_ms[kSubKinds] = {}, g_gsub_gap[kSubKinds] = {};
uint64_t g_gsub_cnt[kSubKinds] = {};
GpuSlot& gsub_slot(int set, int bottom, int i) { return g_gsub[(set * 2 + bottom) * kSubMax + i]; }
// a set's sub-marks read (its marks' samples `bot`, swapped as `swap` says): between the depth clear's mark and the world's
void gsub_read(int set, const uint64_t* bot, bool swap) {
    const int n = std::min(g_gsub_n[set], kSubMax);
    g_gsub_n[set] = 0;
    if (g_gsub == nullptr || n == 0 || (g_gmarked[set] & (1u << kGmZclear)) == 0 || (g_gmarked[set] & (1u << kGmWorld)) == 0) return;
    DCInvalidateRange(&gsub_slot(set, 0, 0), static_cast<uint32_t>(2 * kSubMax * sizeof(GpuSlot)));
    const uint64_t lo = bot[kGmZclear], hi = bot[kGmWorld];
    uint64_t prev = lo;
    for (int i = 0; i < n; ++i) {
        uint64_t t = gsub_slot(set, 0, i).v, b = gsub_slot(set, 1, i).v;
        if (t == ~0ull || b == ~0ull) return;  // (not written: the frame's sub-marks dropped from here)
        if (swap) {
            t = __builtin_bswap64(t);
            b = __builtin_bswap64(b);
        }
        if (b < prev || b > hi) return;
        const int k = g_gsub_kind[set][i] < rac1::kSmN ? g_gsub_kind[set][i] : rac1::kSmN;
        g_gsub_ms[k] += gpu_ms(b - prev);
        g_gsub_gap[k] += t > prev ? gpu_ms(t - prev) : 0.0;
        ++g_gsub_cnt[k];
        prev = b;
    }
    g_gsub_ms[rac1::kSmN] += gpu_ms(hi - prev);
    ++g_gsub_cnt[rac1::kSmN];
}
// a set two frames old read into the window's sums
void gpu_read(int set) {
    if (g_gframe[set] == 0 || (g_gmarked[set] & 1u) == 0) return;
    uint64_t top[kGmN], bot[kGmN];
    DCInvalidateRange(&gpu_slot(set, 0, 0), static_cast<uint32_t>(2 * kGmN * sizeof(GpuSlot)));
    for (int k = 0; k < kGmN; ++k) {
        top[k] = gpu_slot(set, 0, k).v;
        bot[k] = gpu_slot(set, 1, k).v;
        if ((g_gmarked[set] & (1u << k)) != 0 && (top[k] == ~0ull || bot[k] == ~0ull)) {
            ++g_glate;  // (not written yet: dropped)
            g_gframe[set] = 0;
            return;
        }
    }
    // the samples' byte order checked (the bottoms in mark order, the frame under a second; else byte-swapped, else unreadable)
    auto sane = [&](const uint64_t* b) {
        uint64_t last = b[0];
        for (int k = 1; k < kGmN; ++k) {
            if ((g_gmarked[set] & (1u << k)) == 0) continue;
            if (b[k] < last) return false;
            last = b[k];
        }
        return gpu_ms(last - b[0]) < 1000.0;
    };
    bool swapped = false;
    if (!sane(bot)) {
        swapped = true;
        uint64_t t2[kGmN], b2[kGmN];
        for (int k = 0; k < kGmN; ++k) {
            t2[k] = __builtin_bswap64(top[k]);
            b2[k] = __builtin_bswap64(bot[k]);
        }
        if (!sane(b2)) {
            if (g_gbad++ < 3) {
                std::printf("[gpu] host frame %llu: samples unreadable (marks %#x):", static_cast<unsigned long long>(g_gframe[set]), g_gmarked[set]);
                for (int k = 0; k < kGmN; ++k) std::printf(" %s %016llx/%016llx", kGmName[k], static_cast<unsigned long long>(top[k]), static_cast<unsigned long long>(bot[k]));
                std::printf("\n");
            }
            g_gframe[set] = 0;
            return;
        }
        std::memcpy(top, t2, sizeof top);
        std::memcpy(bot, b2, sizeof bot);
        ++g_gswapped;
    }
    uint64_t prev = bot[0], end = bot[0];
    for (int k = 1; k < kGmN; ++k) {
        if ((g_gmarked[set] & (1u << k)) == 0) continue;  // (a pass not drawn: its time the next mark's)
        g_gsum[k] += gpu_ms(bot[k] - prev);
        g_glead[k] += bot[k] > top[k] ? gpu_ms(bot[k] - top[k]) : 0.0;
        prev = end = bot[k];
    }
    g_gbusy += gpu_ms(end - bot[0]);
    gsub_read(set, bot, swapped);
    if (g_gprev_end != 0 && g_gprev_frame + 1 == g_gframe[set] && bot[0] >= g_gprev_end) {  // (from the frame before's end: the GPU idle)
        g_ggap += gpu_ms(bot[0] - g_gprev_end);
        ++g_gnperiod;
    }
    g_gprev_end = end;
    g_gprev_frame = g_gframe[set];
    ++g_gn;
    g_gframe[set] = 0;
}
// a host frame's start (the frame before waited for): the set of two frames ago read, this frame's set begun (`on`: the timing's)
void gpu_frame_begin(bool on, uint64_t frame) {
    g_gcur = -1;
    if (!on) return;
    if (g_gslots == nullptr) {
        const uint32_t bytes = static_cast<uint32_t>(kGpuRing * 2 * kGmN * sizeof(GpuSlot));
        g_gslots = static_cast<GpuSlot*>(MEMAllocFromDefaultHeapEx(bytes, 64));
        if (g_gslots == nullptr) return;
        std::memset(g_gslots, 0xff, bytes);
        DCFlushRange(g_gslots, bytes);
        std::printf("[gpu] the GPU's clock: 27,000,000 of its ticks = %.1f ms (27 MHz)\n", gpu_ms(27000000ull));
    }
    if (g_gsub_on && g_gsub == nullptr) {
        const uint32_t bytes = static_cast<uint32_t>(kGpuRing * 2 * kSubMax * sizeof(GpuSlot));
        g_gsub = static_cast<GpuSlot*>(MEMAllocFromDefaultHeapEx(bytes, 64));
        if (g_gsub != nullptr) {
            std::memset(g_gsub, 0xff, bytes);
            DCFlushRange(g_gsub, bytes);
        }
        std::printf("[gpu] the sky's / world's passes timed by kind (RAC1_WIIU_GPU_TIME=2): %s\n", g_gsub != nullptr ? "on" : "no memory");
    }
    gpu_read(static_cast<int>((frame + kGpuRing - 2) % kGpuRing));
    g_gcur = static_cast<int>(frame % kGpuRing);
    g_gframe[g_gcur] = frame;
    g_gmarked[g_gcur] = 0;
    g_gsub_n[g_gcur] = 0;
}
// the window's averages on the log and the menu's lines (with the [timing] line)
void gpu_window(std::vector<std::string>& menu) {
    if (g_gn == 0) {
        if (g_gslots != nullptr) std::printf("[gpu] no frame read (%llu late, %llu unreadable)\n", static_cast<unsigned long long>(g_glate), static_cast<unsigned long long>(g_gbad));
        return;
    }
    const double n = static_cast<double>(g_gn);
    char b[96];
    std::printf("[gpu] %llu TV frames: busy %.1f ms (pass ms / the CP's lead at its end ms):", static_cast<unsigned long long>(g_gn), g_gbusy / n);
    std::snprintf(b, sizeof b, "GPU %.1f ms a frame, idle between %.1f", g_gbusy / n, g_gnperiod != 0 ? g_ggap / static_cast<double>(g_gnperiod) : 0.0);
    menu.emplace_back(b);
    for (int k = 1; k < kGmN; ++k) {
        std::printf(" %s %.2f/%.1f,", kGmName[k], g_gsum[k] / n, g_glead[k] / n);
        if (g_gsum[k] / n >= 0.3) {
            std::snprintf(b, sizeof b, "  gpu %-20.20s %6.1f", kGmName[k], g_gsum[k] / n);
            menu.emplace_back(b);
        }
        g_gsum[k] = g_glead[k] = 0.0;
    }
    std::printf(" idle between frames %.1f; %llu late, %llu unreadable, %llu byte-swapped\n", g_gnperiod != 0 ? g_ggap / static_cast<double>(g_gnperiod) : 0.0,
                static_cast<unsigned long long>(g_glate), static_cast<unsigned long long>(g_gbad), static_cast<unsigned long long>(g_gswapped));
    if (g_gsub != nullptr) {  // (RAC1_WIIU_GPU_TIME=2)
        std::printf("[gpu-world] ms a TV frame (passes a frame, the CP's gap ms):");
        for (int k = 0; k < kSubKinds; ++k) {
            std::printf(" %s %.2f (%.1f, gap %.2f),", kSubName[k], g_gsub_ms[k] / n, static_cast<double>(g_gsub_cnt[k]) / n, g_gsub_gap[k] / n);
            if (g_gsub_ms[k] / n >= 0.3) {
                std::snprintf(b, sizeof b, "   %-19.19s %6.1f", kSubName[k], g_gsub_ms[k] / n);
                menu.emplace_back(b);
            }
            g_gsub_ms[k] = g_gsub_gap[k] = 0.0;
            g_gsub_cnt[k] = 0;
        }
        std::printf(" %llu frames past %d marks\n", static_cast<unsigned long long>(g_gsub_over), kSubMax);
        g_gsub_over = 0;
    }
    g_gbusy = g_ggap = 0.0;
    g_gn = g_gnperiod = g_glate = g_gswapped = 0;
}

// (Gx2Perf::hiz, 2026-10-28) a depth buffer's hierarchical Z: WHB makes the TV's and the GamePad's without its buffer (HTILE: no
// hierarchical culling, no compressed depth, no fast clear); its memory (MEM2) made at the first frame on and kept, the buffer's
// registers set each frame as the switch says (WHB's acquire may make them again), before the frame's clears
struct HizMem {
    const GX2DepthBuffer* db = nullptr;
    void* mem = nullptr;
    uint32_t size = 0;
};
HizMem g_hiz[3];  // (the TV's, the GamePad's, the scaled 3D's: RAC1_WIIU_3D_SCALE)
void hiz_apply(GX2DepthBuffer* db) {
    if (db == nullptr) return;
    HizMem* h = nullptr;
    for (HizMem& m : g_hiz)
        if (m.db == db || (h == nullptr && m.db == nullptr)) h = &m;
    if (h == nullptr) return;
    h->db = db;
    if (rac1::g_gx2_perf.hiz && (db->hiZPtr == nullptr || db->hiZPtr != h->mem)) {
        uint32_t size = 0, align = 0;
        GX2CalcDepthBufferHiZInfo(db, &size, &align);
        if (h->mem == nullptr || h->size < size) {
            if (h->mem != nullptr) MEMFreeToDefaultHeap(h->mem);
            h->mem = size != 0 ? MEMAllocFromDefaultHeapEx(size, std::max<uint32_t>(align, 64)) : nullptr;
            h->size = h->mem != nullptr ? size : 0;
            if (h->mem != nullptr) DCFlushRange(h->mem, size);  // (no CPU line of it written back over the GPU's)
            std::printf("[gpu] hierarchical Z for the %u x %u depth buffer: %u KB (align %u)%s\n", db->surface.width, db->surface.height, size / 1024, align,
                        h->mem != nullptr ? "" : ": no memory, off");
        }
        if (h->mem == nullptr) {
            rac1::g_gx2_perf.hiz = false;
            return;
        }
        db->hiZPtr = h->mem;
        db->hiZSize = h->size;
    }
    if (db->hiZPtr != nullptr && db->hiZPtr == h->mem) GX2InitDepthBufferHiZEnable(db, rac1::g_gx2_perf.hiz ? TRUE : FALSE);
}
}  // namespace

namespace {
// ((2026-10-28, the GPU's time: the world's pixels) RAC1_WIIU_3D_SCALE / the debug menu's "3D resolution": the TV's 3D - the sky, the world,
// the mobys - drawn into a colour and a depth buffer that much smaller (the TV's sides times the scale; its own hierarchical Z), then drawn
// over the TV's frame by shaders/wiiu_blit.gsh (a quad, bilinear) before the 2D, which stays at the TV's resolution; the GamePad's copy
// (RAC1_WIIU_GAMEPAD=1) and the shots copy the TV's buffer after it. Its memory: inside the TV's depth buffer (MEM1, WHB's - unused by the
// TV's 3D then: nothing else reads or writes it between the frame's clear and the upscale; the depth caches flushed around it) as far as it
// fits there (RAC1_WIIU_3D_MEM1, on: 1280 x 720 both buffers, 1440 x 810 the colour alone), the rest in MEM2. (g_menu_vpscale, the
// frame's share, is another thing: the frame drawn smaller inside the TV's buffer, not scaled up.) 1: the 3D drawn into the TV's buffer
struct Scaled3d {
    bool ok = false;
    float scale = 1.0f;
    const void* tv_depth = nullptr;  // (the TV's depth image the buffers were placed against: WHB's acquire may make it again)
    GX2ColorBuffer cb{};
    GX2DepthBuffer db{};
    GX2Texture tex{};                // (the colour buffer's image, sampled by the upscale)
    void* own_c = nullptr;           // (MEM2 allocations; nullptr: inside the TV's depth buffer)
    void* own_d = nullptr;
} g_s3;
WHBGfxShaderGroup g_blit{};
bool g_blit_ok = false, g_blit_tried = false;
uint32_t g_blit_loc = 0;
GX2Sampler g_blit_sampler{};
float* g_blit_vb = nullptr;  // [8][4 corners][x, y, u, v], MEM2 (cycled across blits and overlapped frames)
uint32_t g_blit_half = 0;
GX2Texture g_movie_tex{};
bool g_movie_active = false;
uint64_t g_movie_tex_serial = 0;
void movie_tex_free() {
    if (g_movie_tex.surface.image != nullptr) {
        GX2DrawDone();
        MEMFreeToDefaultHeap(g_movie_tex.surface.image);
    }
    g_movie_tex = GX2Texture{};
    g_movie_active = false;
    g_movie_tex_serial = 0;
}
void s3_free() {
    if (g_s3.own_c != nullptr) MEMFreeToDefaultHeap(g_s3.own_c);
    if (g_s3.own_d != nullptr) MEMFreeToDefaultHeap(g_s3.own_d);
    g_s3 = Scaled3d{};
}
// the upscale's shaders (shaders/wiiu_blit.gsh), sampler and corners, made at the first use; false: not there (the 3D drawn at the TV's size)
bool blit_ready() {
    if (g_blit_tried) return g_blit_ok;
    g_blit_tried = true;
    const std::vector<uint8_t> gsh = read_file("shaders/wiiu_blit.gsh");
    if (gsh.empty() || !WHBGfxLoadGFDShaderGroup(&g_blit, 0, gsh.data())) {
        std::printf("[gpu] shaders/wiiu_blit.gsh not loaded: the 3D drawn at the TV's resolution (RAC1_WIIU_3D_SCALE)\n");
        g_blit = WHBGfxShaderGroup{};
        return false;
    }
    bool loc = false;
    for (uint32_t i = 0; i < g_blit.pixelShader->samplerVarCount; ++i)
        if (std::strcmp(g_blit.pixelShader->samplerVars[i].name, "u_tex") == 0) {
            g_blit_loc = g_blit.pixelShader->samplerVars[i].location;
            loc = true;
        }
    g_blit_vb = static_cast<float*>(MEMAllocFromDefaultHeapEx(8 * 4 * 4 * sizeof(float), GX2_VERTEX_BUFFER_ALIGNMENT));
    if (!loc || g_blit_vb == nullptr || !WHBGfxInitShaderAttribute(&g_blit, "a_pos", 0, 0, GX2_ATTRIB_FORMAT_FLOAT_32_32_32_32) || !WHBGfxInitFetchShader(&g_blit)) {
        std::printf("[gpu] shaders/wiiu_blit.gsh: sampler %d, memory %d: the 3D drawn at the TV's resolution\n", loc, g_blit_vb != nullptr);
        return false;
    }
    GX2InitSampler(&g_blit_sampler, GX2_TEX_CLAMP_MODE_CLAMP, GX2_TEX_XY_FILTER_MODE_LINEAR);
    g_blit_ok = true;
    return true;
}
// the scaled 3D's buffers for `scale` beside the TV's (made again when the scale or the TV's depth buffer changes); nullptr: the 3D drawn
// into the TV's buffers (scale 1, no upscale shaders, no memory). Called after the frame before's GPU work was waited for: freeing is safe
Scaled3d* s3_get(float scale, const GX2ColorBuffer* tv, GX2DepthBuffer* tvdb) {
    if (scale > 0.99f || tv == nullptr || tvdb == nullptr || !blit_ready()) {
        if (g_s3.ok) s3_free();
        return nullptr;
    }
    if (g_s3.ok && g_s3.scale == scale && g_s3.tv_depth == tvdb->surface.image) return &g_s3;
    s3_free();
    static const bool mem1 = std::getenv("RAC1_WIIU_3D_MEM1") == nullptr || std::atoi(std::getenv("RAC1_WIIU_3D_MEM1")) != 0;
    const uint32_t w = std::max(64u, static_cast<uint32_t>(static_cast<float>(tv->surface.width) * scale + 0.5f) & ~1u);
    const uint32_t h = std::max(64u, static_cast<uint32_t>(static_cast<float>(tv->surface.height) * scale + 0.5f) & ~1u);
    auto surface = [&](GX2Surface& sf, GX2SurfaceFormat format, GX2SurfaceUse use) {
        sf = GX2Surface{};
        sf.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        sf.width = w;
        sf.height = h;
        sf.depth = 1;
        sf.mipLevels = 1;
        sf.format = format;
        sf.aa = GX2_AA_MODE1X;
        sf.use = use;
        sf.tileMode = GX2_TILE_MODE_DEFAULT;
        GX2CalcSurfaceSizeAndAlignment(&sf);
    };
    GX2Surface& cs = g_s3.cb.surface;
    GX2Surface& ds = g_s3.db.surface;
    surface(cs, tv->surface.format, static_cast<GX2SurfaceUse>(GX2_SURFACE_USE_TEXTURE | GX2_SURFACE_USE_COLOR_BUFFER));
    surface(ds, tvdb->surface.format, GX2_SURFACE_USE_DEPTH_BUFFER);
    // the images: inside the TV's depth buffer while they fit (aligned), else MEM2
    auto* base = static_cast<uint8_t*>(tvdb->surface.image);
    uintptr_t at = reinterpret_cast<uintptr_t>(base), end = at + tvdb->surface.imageSize;
    auto place = [&](GX2Surface& sf, void*& own) {
        const uintptr_t p = (at + sf.alignment - 1) & ~static_cast<uintptr_t>(sf.alignment - 1);
        if (mem1 && base != nullptr && p + sf.imageSize <= end) {
            sf.image = reinterpret_cast<void*>(p);
            at = p + sf.imageSize;
            return true;
        }
        own = MEMAllocFromDefaultHeapEx(sf.imageSize, static_cast<int>(sf.alignment));
        sf.image = own;
        return own != nullptr;
    };
    if (!place(cs, g_s3.own_c) || !place(ds, g_s3.own_d)) {
        std::printf("[gpu] the 3D at %u x %u: no memory, drawn at the TV's resolution\n", w, h);
        s3_free();
        return nullptr;
    }
    g_s3.cb.viewMip = 0;
    g_s3.cb.viewFirstSlice = 0;
    g_s3.cb.viewNumSlices = 1;
    GX2InitColorBufferRegs(&g_s3.cb);
    g_s3.db.viewMip = 0;
    g_s3.db.viewFirstSlice = 0;
    g_s3.db.viewNumSlices = 1;
    g_s3.db.depthClear = 1.0f;
    g_s3.db.stencilClear = 0;
    GX2InitDepthBufferRegs(&g_s3.db);
    GX2Texture& t = g_s3.tex;
    t = GX2Texture{};
    t.surface = cs;
    t.viewFirstMip = 0;
    t.viewNumMips = 1;
    t.viewFirstSlice = 0;
    t.viewNumSlices = 1;
    t.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_A);
    GX2InitTextureRegs(&t);
    g_s3.ok = true;
    g_s3.scale = scale;
    g_s3.tv_depth = tvdb->surface.image;
    std::printf("[gpu] the 3D at %u x %u (scale %.3f of %u x %u), upscaled: colour %u KB in %s, depth %u KB in %s\n", w, h, scale, tv->surface.width,
                tv->surface.height, cs.imageSize / 1024, g_s3.own_c == nullptr ? "MEM1 (the TV's depth buffer)" : "MEM2", ds.imageSize / 1024,
                g_s3.own_d == nullptr ? "MEM1 (the TV's depth buffer)" : "MEM2");
    return &g_s3;
}
// the scaled 3D's frame `src` (on its buffers) drawn over the TV's frame `dst` (on `tv`), bilinear; the TV's buffers bound after it
void s3_blit(const GX2ColorBuffer* tv, GX2DepthBuffer* tvdb, const rac1::gs::Viewport& src, const rac1::gs::Viewport& dst) {
    // (the 3D's pixels out of the colour cache, the texture cache emptied over them; its depth out of the depth cache - the TV's depth
    // buffer, whose memory it may be, is cleared by the next frame)
    GX2Invalidate(static_cast<GX2InvalidateMode>(GX2_INVALIDATE_MODE_COLOR_BUFFER | GX2_INVALIDATE_MODE_TEXTURE), g_s3.cb.surface.image, g_s3.cb.surface.imageSize);
    GX2Invalidate(GX2_INVALIDATE_MODE_DEPTH_BUFFER, g_s3.db.surface.image, g_s3.db.surface.imageSize);
    const float sw = static_cast<float>(g_s3.cb.surface.width), sh = static_cast<float>(g_s3.cb.surface.height);
    const float tw = static_cast<float>(tv->surface.width), th = static_cast<float>(tv->surface.height);
    // (the Viewports' y from the bottom; the buffers' rows and GX2's from the top)
    const float u0 = static_cast<float>(src.x) / sw, u1 = static_cast<float>(src.x + src.w) / sw;
    const float v0 = (sh - static_cast<float>(src.y + src.h)) / sh, v1 = (sh - static_cast<float>(src.y)) / sh;
    const float x0 = static_cast<float>(dst.x) / tw * 2.0f - 1.0f, x1 = static_cast<float>(dst.x + dst.w) / tw * 2.0f - 1.0f;
    const float y0 = 1.0f - (th - static_cast<float>(dst.y + dst.h)) / th * 2.0f, y1 = 1.0f - (th - static_cast<float>(dst.y)) / th * 2.0f;
    float* q = g_blit_vb + 16 * g_blit_half;
    g_blit_half = (g_blit_half + 1u) & 7u;
    const float c[16] = {x0, y0, u0, v0, x1, y0, u1, v0, x0, y1, u0, v1, x1, y1, u1, v1};  // (a strip: top left, top right, bottom left, bottom right)
    std::memcpy(q, c, sizeof c);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, q, sizeof c);
    GX2SetColorBuffer(tv, GX2_RENDER_TARGET_0);
    GX2SetDepthBuffer(tvdb);
    GX2SetViewport(0.0f, 0.0f, tw, th, 0.0f, 1.0f);
    GX2SetScissor(0, 0, tv->surface.width, tv->surface.height);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);  // (as the 3D's backends leave it)
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    const auto none = static_cast<GX2ChannelMask>(0);
    GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, none, none, none, none, none, none, none);
    GX2SetShaderMode(g_blit.vertexShader->mode);
    GX2SetFetchShader(&g_blit.fetchShader);
    GX2SetVertexShader(g_blit.vertexShader);
    GX2SetPixelShader(g_blit.pixelShader);
    GX2SetPixelTexture(&g_s3.tex, g_blit_loc);
    GX2SetPixelSampler(&g_blit_sampler, g_blit_loc);
    GX2SetAttribBuffer(0, sizeof c, 4 * sizeof(float), q);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);  // (as the 3D's backends leave it)
}
// .pss FMV picture uploaded to `g_movie_tex` once per host frame (after the previous frame's GPU commands finished)
void movie_sync() {
    int mw = 0, mh = 0;
    const std::vector<uint8_t>* movie_rgba = !g_ending ? rac1::movie_hw_shown(mw, mh) : nullptr;
    if (movie_rgba == nullptr || mw <= 0 || mh <= 0 || movie_rgba->size() < static_cast<size_t>(mw) * static_cast<size_t>(mh) * 4) {
        if (g_movie_tex.surface.image != nullptr) movie_tex_free();
        g_movie_active = false;
        return;
    }
    if (!blit_ready()) {
        g_movie_active = false;
        return;
    }
    if (g_movie_tex.surface.image == nullptr ||
        g_movie_tex.surface.width != static_cast<uint32_t>(mw) ||
        g_movie_tex.surface.height != static_cast<uint32_t>(mh)) {
        movie_tex_free();
        g_movie_tex = GX2Texture{};
        GX2Surface& sf = g_movie_tex.surface;
        sf.dim = GX2_SURFACE_DIM_TEXTURE_2D;
        sf.width = static_cast<uint32_t>(mw);
        sf.height = static_cast<uint32_t>(mh);
        sf.depth = 1;
        sf.mipLevels = 1;
        sf.format = GX2_SURFACE_FORMAT_UNORM_R8_G8_B8_A8;
        sf.aa = GX2_AA_MODE1X;
        sf.use = GX2_SURFACE_USE_TEXTURE;
        sf.tileMode = GX2_TILE_MODE_LINEAR_ALIGNED;
        GX2CalcSurfaceSizeAndAlignment(&sf);
        const uint32_t align = std::max<uint32_t>(sf.alignment, 64u);
        sf.image = MEMAllocFromDefaultHeapEx(sf.imageSize, static_cast<int>(align));
        if (sf.image == nullptr) {
            g_movie_active = false;
            return;
        }
        g_movie_tex.viewFirstMip = 0;
        g_movie_tex.viewNumMips = 1;
        g_movie_tex.viewFirstSlice = 0;
        g_movie_tex.viewNumSlices = 1;
        g_movie_tex.compMap = GX2_COMP_MAP(GX2_SQ_SEL_R, GX2_SQ_SEL_G, GX2_SQ_SEL_B, GX2_SQ_SEL_1);
        GX2InitTextureRegs(&g_movie_tex);
    }
    const uint64_t serial = rac1::movie_hw_shown_serial();
    if (serial != g_movie_tex_serial) {
        const uint8_t* src = movie_rgba->data();
        uint8_t* dst = static_cast<uint8_t*>(g_movie_tex.surface.image);
        const size_t src_row_bytes = static_cast<size_t>(mw) * 4;
        const size_t dst_row_bytes = static_cast<size_t>(g_movie_tex.surface.pitch) * 4;
        if (dst_row_bytes == src_row_bytes) {
            std::memcpy(dst, src, src_row_bytes * static_cast<size_t>(mh));
        } else {
            for (int y = 0; y < mh; ++y) {
                std::memcpy(dst + static_cast<size_t>(y) * dst_row_bytes, src + static_cast<size_t>(y) * src_row_bytes, src_row_bytes);
            }
        }
        GX2Invalidate(GX2_INVALIDATE_MODE_CPU_TEXTURE, g_movie_tex.surface.image, static_cast<uint32_t>(dst_row_bytes * static_cast<size_t>(mh)));
        g_movie_tex_serial = serial;
    }
    g_movie_active = true;
}
void movie_blit(const GX2ColorBuffer* cb, GX2DepthBuffer* db, const rac1::gs::Viewport& dst) {
    if (!g_movie_active || g_movie_tex.surface.image == nullptr || !blit_ready()) return;
    const float tw = static_cast<float>(cb->surface.width), th = static_cast<float>(cb->surface.height);
    const int cw = static_cast<int>(static_cast<float>(dst.w) * dst.xscale + 0.5f);
    const int cx = dst.x + (dst.w - cw) / 2;
    const float x0 = static_cast<float>(cx) / tw * 2.0f - 1.0f, x1 = static_cast<float>(cx + cw) / tw * 2.0f - 1.0f;
    const float y0 = 1.0f - (th - static_cast<float>(dst.y + dst.h)) / th * 2.0f, y1 = 1.0f - (th - static_cast<float>(dst.y)) / th * 2.0f;
    float* q = g_blit_vb + 16 * g_blit_half;
    g_blit_half = (g_blit_half + 1u) & 7u;
    const float c[16] = {x0, y0, 0.0f, 0.0f, x1, y0, 1.0f, 0.0f, x0, y1, 0.0f, 1.0f, x1, y1, 1.0f, 1.0f};
    std::memcpy(q, c, sizeof c);
    GX2Invalidate(GX2_INVALIDATE_MODE_CPU_ATTRIBUTE_BUFFER, q, sizeof c);
    GX2SetColorBuffer(cb, GX2_RENDER_TARGET_0);
    if (db != nullptr) GX2SetDepthBuffer(db);
    GX2SetViewport(0.0f, 0.0f, tw, th, 0.0f, 1.0f);
    GX2SetScissor(0, 0, cb->surface.width, cb->surface.height);
    GX2SetDepthOnlyControl(FALSE, FALSE, GX2_COMPARE_FUNC_ALWAYS);
    GX2SetColorControl(GX2_LOGIC_OP_COPY, 0, FALSE, TRUE);
    GX2SetCullOnlyControl(GX2_FRONT_FACE_CCW, FALSE, FALSE);
    GX2SetAlphaTest(FALSE, GX2_COMPARE_FUNC_ALWAYS, 0.0f);
    const auto none = static_cast<GX2ChannelMask>(0);
    GX2SetTargetChannelMasks(GX2_CHANNEL_MASK_RGBA, none, none, none, none, none, none, none);
    GX2SetShaderMode(g_blit.vertexShader->mode);
    GX2SetFetchShader(&g_blit.fetchShader);
    GX2SetVertexShader(g_blit.vertexShader);
    GX2SetPixelShader(g_blit.pixelShader);
    GX2SetPixelTexture(&g_movie_tex, g_blit_loc);
    GX2SetPixelSampler(&g_blit_sampler, g_blit_loc);
    GX2SetAttribBuffer(0, sizeof c, 4 * sizeof(float), q);
    GX2DrawEx(GX2_PRIMITIVE_MODE_TRIANGLE_STRIP, 4, 0, 1);
    GX2SetShaderMode(GX2_SHADER_MODE_UNIFORM_REGISTER);
}
}  // namespace


namespace {
bool g_tm_draw = false;  // (RAC1_WIIU_TIMING: the TV's call timed by backend)
}  // namespace

// orig: (host) see mrp_world_gx2.h (RAC1_WIIU_GPU_TIME=2: the TV's call only)
void rac1::wiiu_gpu_submark(int kind) {
    if (!g_tm_draw || g_gcur < 0 || g_gsub == nullptr) return;
    const int i = g_gsub_n[g_gcur];
    if (i >= kSubMax) {
        if (i == kSubMax) ++g_gsub_over;
        g_gsub_n[g_gcur] = kSubMax + 1;  // (counted once; read as kSubMax)
        return;
    }
    GpuSlot& t = gsub_slot(g_gcur, 0, i);
    GpuSlot& b = gsub_slot(g_gcur, 1, i);
    t.v = ~0ull;
    b.v = ~0ull;
    DCFlushRange(&t, sizeof t);
    DCFlushRange(&b, sizeof b);
    GX2SampleTopGPUCycle(&t.v);
    GX2SampleBottomGPUCycle(&b.v);
    DCFlushRange(&t, sizeof t);
    DCFlushRange(&b, sizeof b);
    g_gsub_kind[g_gcur][i] = static_cast<uint8_t>(kind);
    g_gsub_n[g_gcur] = i + 1;
}

namespace {
// the game's frame on colour buffer `cb`: 4:3, or wider with the widescreen, centred
rac1::gs::Viewport frame_viewport(const GX2ColorBuffer* cb) {
    const int w = static_cast<int>(cb->surface.width), h = static_cast<int>(cb->surface.height);
    rac1::gs::Viewport vp;
    // (testing, RAC1_WIIU_VPSCALE=<0.1..1>: the game's frame drawn that much smaller, centred - the pixels' share of a frame's cost)
    static const float vps_env = std::getenv("RAC1_WIIU_VPSCALE") != nullptr ? std::clamp(static_cast<float>(std::atof(std::getenv("RAC1_WIIU_VPSCALE"))), 0.1f, 1.0f) : 1.0f;
    static bool vps_first = true;
    if (vps_first) g_menu_vpscale = vps_env;  // (the debug menu changes it live)
    vps_first = false;
    const float vps = g_menu_vpscale;
    vp.h = static_cast<int>(static_cast<float>(h) * vps);
    vp.w = vp.h * 4 / 3;
    {  // ((2026-10-28) the widescreen, RAC1_WIIU_WIDE / the debug menu: the 3D in a frame g_view_draw_widen wider than the game's 4:3 -
       // render_proj_x's projection, the host's culling widened the same -, the game's 2D and its projected 3D kept 4:3 in its centre)
        const float wd = rac1::g_view_draw_widen;
        if (wd > 1.0f) {
            const int w43 = vp.w;
            vp.w = std::min(static_cast<int>(static_cast<float>(w43) * wd + 0.5f), static_cast<int>(static_cast<float>(w) * vps + 0.5f));
            vp.xscale = vp.xscale_3d = static_cast<float>(w43) / static_cast<float>(vp.w);
        } else if (rac1::g_view_mode == 0) {
            vp.w = static_cast<int>(static_cast<float>(w) * vps + 0.5f);
        }
    }
    vp.hud_shift = rac1::render_view_hud_shift(g_2d.fb_w);
    vp.x = (w - vp.w) / 2;
    vp.y = (h - vp.h) / 2;
    vp.screen_fbp = g_2d.screen_fbp;
    vp.skip_fbp = g_2d.skip_fbp;
    vp.sprite_clamp = true;
    vp.hide_edge_slivers = true;
    return vp;
}

// the game's frame (4:3) centred on a colour buffer and its 2D drawn (the test triangle until the game's first frame)
void draw_frame(const GX2ColorBuffer* cb, GX2DepthBuffer* db, bool targets) {
    if (g_movie_active) {
        if (g_world_ready) rac1::mrp_world_wiiu_join();
        movie_blit(cb, db, frame_viewport(cb));
        if (g_tm_draw) gpu_mark(kGm2d);
        return;
    }
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
    const rac1::gs::Viewport vp = frame_viewport(cb);
    // ((2026-10-28) RAC1_WIIU_3D_SCALE, the debug menu's "3D resolution": the TV's 3D drawn smaller and upscaled - Scaled3d above; the
    // GamePad's own drawing never)
    static bool s3_first = true;
    static float s3_gfx_seen = 0.0f;
    if (s3_first) {
        s3_first = false;
        if (const char* e = std::getenv("RAC1_WIIU_3D_SCALE"); e != nullptr && e[0] != 0) g_menu_3d_scale = std::clamp(static_cast<float>(std::atof(e)), 0.25f, 1.0f);
    }
    if (!rac1::g_advanced_disabled) {
        const float gs = std::clamp(rac1::graphics_render_scale(), 0.25f, 1.0f);
        if (gs != s3_gfx_seen) g_menu_3d_scale = s3_gfx_seen = gs;
    }
    const bool tv = cb != WHBGfxGetDRCColourBuffer();
    Scaled3d* s3 = tv && (g_mobys_ready || g_world_ready) ? s3_get(g_menu_3d_scale, cb, db) : nullptr;
    const GX2ColorBuffer* cb3 = s3 != nullptr ? &s3->cb : cb;  // (the 3D's buffers and frame)
    GX2DepthBuffer* db3 = s3 != nullptr ? &s3->db : db;
    const rac1::gs::Viewport vp3 = s3 != nullptr ? frame_viewport(cb3) : vp;
    {  // (2026-10-28, the GPU's time: the buffers the picture draws into, once on the log - sizes, formats, AA, tiling, hierarchical Z)
        static bool told[2] = {false, false};
        const int which = cb == WHBGfxGetTVColourBuffer() ? 0 : 1;
        if (!told[which]) {
            told[which] = true;
            const GX2Surface& c = cb->surface;
            std::printf("[gpu] the %s's colour buffer %u x %u, format %#x, aa %d, tile mode %d, %u KB; ", which == 0 ? "TV" : "GamePad", c.width, c.height,
                        static_cast<unsigned>(c.format), static_cast<int>(c.aa), static_cast<int>(c.tileMode), c.imageSize / 1024);
            if (db != nullptr)
                std::printf("depth %u x %u, format %#x, aa %d, tile mode %d, %u KB, hierarchical Z %s; ", db->surface.width, db->surface.height,
                            static_cast<unsigned>(db->surface.format), static_cast<int>(db->surface.aa), static_cast<int>(db->surface.tileMode), db->surface.imageSize / 1024,
                            db->hiZPtr != nullptr ? "buffer there" : "none");
            std::printf("the game's frame %d x %d at %d, %d (%.0f%% of the buffer's pixels)\n", vp.w, vp.h, vp.x, vp.y,
                        100.0 * vp.w * vp.h / (static_cast<double>(w) * h));
        }
    }
    // the 3D under the 2D: the depth cleared once (the world and the mobys share it), the world's ties / shrubs, then the mobys.
    // (testing, RAC1_WIIU_ZCLEAR=0: the depth not cleared here - WHBGfxClearColor clears it too, when called before)
    static const bool zclear = std::getenv("RAC1_WIIU_ZCLEAR") == nullptr || std::atoi(std::getenv("RAC1_WIIU_ZCLEAR")) != 0;
    if (s3 != nullptr) {
        // (the scaled 3D's buffers cleared - black, as WHBGfxClearColor the TV's, and the far depth -, after the TV's depth buffer's lines
        // are out of the depth cache: its memory may hold them)
        if (g_s3.own_c == nullptr || g_s3.own_d == nullptr) GX2Invalidate(GX2_INVALIDATE_MODE_DEPTH_BUFFER, db->surface.image, db->surface.imageSize);
        hiz_apply(db3);
        GX2ClearColor(&s3->cb, 0.0f, 0.0f, 0.0f, 1.0f);
        GX2ClearDepthStencilEx(db3, 1.0f, 0, GX2_CLEAR_FLAGS_DEPTH);
        if (GX2ContextState* ctx = WHBGfxGetTVContextState()) GX2SetContextState(ctx);
    }
    // ((2026-10-28) Gx2Perf::zclear2 off: the debug menu's - WHBGfxClearColor's clear alone)
    else if (db != nullptr && zclear && rac1::g_gx2_perf.zclear2 && (g_mobys_ready || g_world_ready)) {
        GX2ClearDepthStencilEx(db, 1.0f, 0, GX2_CLEAR_FLAGS_DEPTH);
        // (the clear changes the GPU's state: the context of `cb` set again, the TV's or the GamePad's)
        GX2ContextState* ctx = cb == WHBGfxGetTVColourBuffer() ? WHBGfxGetTVContextState() : WHBGfxGetDRCContextState();
        if (ctx != nullptr) GX2SetContextState(ctx);
    }
    if (g_tm_draw) gpu_mark(kGmZclear);  // (the TV's call: the GPU's marks, gpu_mark)
    // ((2026-10-28) Gx2Perf::flush: the commands so far sent to the GPU at each pass's end - the clears before the world's worker's join, the
    // sky before the world's draws are made, ...: GX2 sent a frame's commands at its end, the GPU idle meanwhile ("[gpu]" idle between frames
    // 9-43 ms, the frame's making on the CPU))
    rac1::gx2_flush(1);
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
        // the sky first (MRP_LITE.md phase 5): no depth test nor writes, the depth's hierarchical Z untouched
        rac1::mrp_world_gx2_draw_sky(cb3, db3, vp3);
        if (g_tm_draw) gpu_mark(kGmSky);
        rac1::gx2_flush(1);
        dlap(kTmSky);
        rac1::mrp_world_gx2_draw(cb3, db3, vp3);
        if (g_tm_draw) gpu_mark(kGmWorld);
        rac1::gx2_flush(1);
        dlap(kTmWorld);
    }
    if (g_mobys_ready && g_menu_draw_mobys) {
        rac1::mrp_moby_gx2_debug_frame(g_2d.game_frame);
        rac1::mrp_moby_gx2_draw(cb3, db3, vp3);
        if (g_tm_draw) gpu_mark(kGmMobys);
        rac1::gx2_flush(1);
        dlap(kTmMobys);
    }
    if (s3 != nullptr) {  // (the scaled 3D over the TV's frame, the TV's buffers bound again for the 2D)
        s3_blit(cb, db, vp3, vp);
        if (g_tm_draw) gpu_mark(kGmUpscale);
    }
    if (s_2d == 3) {  // (testing, RAC1_WIIU_TEST_AFTER=1: the test triangle drawn over the mobys, the target checked)
        static const bool test_after = std::getenv("RAC1_WIIU_TEST_AFTER") != nullptr && std::atoi(std::getenv("RAC1_WIIU_TEST_AFTER")) != 0;
        if (test_after) draw_test();
        return;
    }
    // the 2D (the render targets: by the TV's first call per frame), after the world and the mobys (no depth: over them)
    if (!g_menu_draw_2d) return;  // (the GamePad's debug menu)
    if (g_2d.replica != nullptr) rac1::gs::draw_gx2(*g_2d.replica, cb, g_2d.fb_w, g_2d.fb_h, &vp, targets);
    else rac1::mrp_2d_gx2_draw(rac1::mrp_chain_frame(), cb, g_2d.fb_w, g_2d.fb_h, vp, targets);
    if (g_tm_draw) gpu_mark(kGm2d);
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
    if (!g_on || !g_foreground) return;  // (released: no buffers to draw into)
    ++g_frames;
    const bool tm = tm_on();
    if (debug_menu_wanted()) rac1::prof::request();  // (the game's stages recorded for the menu's game page: the next host frame)
    if (tm) {  // (the host frame: from the last one's start to this one's)
        const auto now = std::chrono::steady_clock::now();
        if (g_tm_last.time_since_epoch().count() != 0) {
            const double frame_ms = std::chrono::duration<double, std::milli>(now - g_tm_last).count();
            // a hitch (a host frame over RAC1_WIIU_TIMING_SPIKE ms, default 400): its own stages on the log, the game frame it reached
            static const double spike = std::getenv("RAC1_WIIU_TIMING_SPIKE") != nullptr ? std::atof(std::getenv("RAC1_WIIU_TIMING_SPIKE")) : 400.0;
            if (frame_ms > spike) {
                double known = 0.0;
                std::printf("[hitch] game frame %llu: %.0f ms:", static_cast<unsigned long long>(g_2d.game_frame), frame_ms);
                for (int i = 0; i < kTmN; ++i) {
                    const double v = g_tm[i] - g_tm_mark[i];
                    if (v >= 1.0) std::printf(" %s %.0f,", kTmName[i], v);
                    if (i != kTmGameSteps) known += v;
                }
                std::printf(" the rest (the game's steps) %.0f\n", frame_ms - known);
            }
            for (int i = 0; i < kTmN; ++i) g_tm_mark[i] = g_tm[i];
            g_tm_total += frame_ms;
            static const int window = debug_menu_wanted() ? 30 : 60;
            if (++g_tm_frames == window) {
                const double n = window;
                double known = 0.0;
                std::printf("[timing] %d host frames, %.1f ms each:", window, g_tm_total / n);
                g_menu_stats.clear();
                char b[96];
                const uint64_t steps = g_2d.game_frame - g_tm_steps_mark;
                g_tm_steps_mark = g_2d.game_frame;
                std::snprintf(b, sizeof b, "frame %.1f ms  %.1f fps", g_tm_total / n, 1000.0 * n / g_tm_total);
                g_menu_stats.emplace_back(b);
                std::snprintf(b, sizeof b, "game steps %.1f a second", 1000.0 * static_cast<double>(steps) / g_tm_total);
                g_menu_stats.emplace_back(b);
                const double gt_steps = g_tm[kTmGameSteps];
                for (int i = 0; i < kTmN; ++i) {
                    if (i == kTmGameSteps) continue;  // (the game thread's: below)
                    std::printf(" %s %.1f,", kTmName[i], g_tm[i] / n);
                    if (g_tm[i] / n >= 0.3) {
                        std::snprintf(b, sizeof b, "  %-24.24s %6.1f", kTmName[i], g_tm[i] / n);
                        g_menu_stats.emplace_back(b);
                    }
                    known += g_tm[i];
                    g_tm[i] = 0.0;
                    g_tm_mark[i] = 0.0;
                }
                g_tm[kTmGameSteps] = g_tm_mark[kTmGameSteps] = 0.0;
                // (the steps the main thread ran itself - in series -, then those the game thread ran beside the drawing: their host
                // frames of the window, their time a host frame of the window)
                std::printf(" the rest (the game's steps) %.1f; game thread: %llu of %d host frames, its steps %.1f\n", (g_tm_total - known) / n,
                            static_cast<unsigned long long>(g_tm_gt_frames), window, gt_steps / n);
                std::snprintf(b, sizeof b, "  %-24.24s %6.1f", "the game's steps", (g_tm_total - known) / n);
                g_menu_stats.emplace_back(b);
                if (g_tm_gt_frames != 0) {
                    std::snprintf(b, sizeof b, "  %-24.24s %6.1f", "steps on the game thread", gt_steps / n);
                    g_menu_stats.emplace_back(b);
                }
                g_tm_gt_frames = 0;
                gpu_window(g_menu_stats);  // (the GPU's own times of the TV frame's passes: "[gpu]")
                // the menu's game page: the game code's own stages (the host profiler's zones, ms a host frame), the biggest first
                if (debug_menu_wanted() && rac1::prof::history_size() > 0) {
                    const rac1::prof::Summary sm = rac1::prof::summarize(window);
                    g_menu_stats.emplace_back("#game");
                    std::vector<std::pair<double, int>> rows;
                    for (int i = 0; i < rac1::prof::kStageCount; ++i)
                        if ((rac1::prof::stage_info(i).game || i == rac1::prof::kAudioStep) && sm.cpu[i] >= 0.2) rows.emplace_back(sm.cpu[i], i);
                    std::sort(rows.begin(), rows.end(), [](const auto& x, const auto& y) { return x.first > y.first; });
                    std::snprintf(b, sizeof b, "game code %.1f ms, %.1f steps a frame", sm.game_code, sm.steps);
                    g_menu_stats.emplace_back(b);
                    std::printf("[game] code %.1f ms, %.1f steps:", sm.game_code, sm.steps);
                    for (const auto& r : rows) {
                        std::snprintf(b, sizeof b, "  %-26.26s %6.1f", rac1::prof::stage_info(r.second).name, r.first);
                        g_menu_stats.emplace_back(b);
                        std::printf(" %s %.1f,", rac1::prof::stage_info(r.second).name, r.first);
                    }
                    std::printf("\n");
                }
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
    const bool overlap = overlap_on && g_menu_overlap && !g_ending;
    // ((2026-10-28) the TV's 2D built before the wait for the GPU, which it then overlaps - mrp_2d_gx2_prebuild: CPU work alone, nothing
    // the GPU reads written; the chain's reader joined for it first. RAC1_WIIU_2D_PREBUILD=0: built at its draw as before)
    static const bool prebuild_on = std::getenv("RAC1_WIIU_2D_PREBUILD") == nullptr || std::atoi(std::getenv("RAC1_WIIU_2D_PREBUILD")) != 0;
    const bool chain_2d = g_2d.have && g_2d.replica == nullptr;
    int pre_mw = 0, pre_mh = 0;
    const bool has_movie = !g_ending && rac1::movie_hw_shown(pre_mw, pre_mh) != nullptr;
    if (prebuild_on && chain_2d) {
        rac1::mrp_chain_join();  // (the frame's chain read on the reader's worker)
        rac1::mrp_world_wiiu_chain_joined();  // (its registers kept for the next frame's world)
        lap(kTmJoin);
        static const int s_2d = std::getenv("RAC1_WIIU_2D") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_2D")) : 2;
        if (!has_movie && g_m2d_ready && g_menu_draw_2d && s_2d == 2) {
            const GX2ColorBuffer* tv = WHBGfxGetTVColourBuffer();
            rac1::mrp_2d_gx2_prebuild(rac1::mrp_chain_frame(), tv, g_2d.fb_w, g_2d.fb_h, frame_viewport(tv));
        }
        lap(kTm2dBuild);
    }
    if (overlap && g_frame_ts != 0) GX2WaitTimeStamp(g_frame_ts);
    lap(kTmGpuWait);
    movie_sync();
    rac1::gx2_perf_init();         // (the GPU switches' start, run.cfg's: once)
    // (RAC1_WIIU_GPU_TIME=1: the GPU's marks - off by default: the first run with them on hung the console, 2026-10-28, not yet known why)
    static const bool gpu_time = std::getenv("RAC1_WIIU_GPU_TIME") != nullptr && std::atoi(std::getenv("RAC1_WIIU_GPU_TIME")) != 0;
    static const bool gpu_sub = gpu_time && std::atoi(std::getenv("RAC1_WIIU_GPU_TIME")) >= 2;  // (the sky's / world's passes by kind too)
    g_gsub_on = gpu_sub;
    gpu_frame_begin(tm && gpu_time, g_frames);  // (the GPU's marks of two frames ago read, this frame's begun)
    if (g_frames % 300 == 0) {  // (the memory left, on the log: a leak shows)
        const struct mallinfo mi = mallinfo();
        const unsigned exp_free_kb = static_cast<unsigned>(MEMGetTotalFreeSizeForExpHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2)) / 1024);
        const unsigned mall_free_kb = static_cast<unsigned>(static_cast<uint32_t>(mi.fordblks) / 1024);
        const unsigned mall_used_kb = static_cast<unsigned>(static_cast<uint32_t>(mi.uordblks) / 1024);
        std::printf("[wiiu] host frame %llu: %u KB free in the default heap (MEM2 exp: %u KB free, malloc: %u KB free, %u KB used)\n",
                    static_cast<unsigned long long>(g_frames), exp_free_kb + mall_free_kb, exp_free_kb, mall_free_kb, mall_used_kb);
    }
    if (chain_2d && !prebuild_on) {
        rac1::mrp_chain_join();  // (the frame's chain read on the reader's worker)
        rac1::mrp_world_wiiu_chain_joined();  // (its registers kept for the next frame's world)
    }
    lap(kTmJoin);
    WHBGfxBeginRender();
    lap(kTmBegin);
    hiz_apply(WHBGfxGetTVDepthBuffer());  // (Gx2Perf::hiz: before the clears, which clear its hierarchical Z too)
    gpu_mark(kGmStart);
    WHBGfxBeginRenderTV();
    WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
    gpu_mark(kGmClear);
    g_tm_draw = tm;
    draw_frame(WHBGfxGetTVColourBuffer(), WHBGfxGetTVDepthBuffer(), g_2d.targets_due);
    g_tm_draw = false;
    if (g_m2d_ready) rac1::mrp_2d_gx2_prebuild_drop();
    t = std::chrono::steady_clock::now();
    g_2d.targets_due = false;
    WHBGfxFinishRenderTV();
    gpu_mark(kGmTvCopy);
    lap(kTmFinishTv);
    if (const uint64_t due = shot_wanted(g_2d.game_frame)) shot(WHBGfxGetTVColourBuffer(), due);
    // (testing, RAC1_WIIU_DRC=0: the GamePad's screen cleared only, the frame drawn for the TV alone)
    static const bool drc = std::getenv("RAC1_WIIU_DRC") == nullptr || std::atoi(std::getenv("RAC1_WIIU_DRC")) != 0;
    // ((2026-10-28) the game's picture on the GamePad, RAC1_WIIU_GAMEPAD: 1 (the default) the TV's colour buffer copied down to the
    // GamePad's screen - GX2CopyColorBufferToScanBuffer scales it, as WHBGfxFinishRenderTV's copy does the TV's -: no second drawing of
    // the frame, neither its CPU work (~2 ms a host frame) nor its GPU work (every pass again, at 854 x 480); 2 drawn for it every other
    // host frame (its colour buffer, which keeps the last one, copied again between); 0 drawn every host frame as before. The debug menu
    // is drawn as before)
    static const int pad_mode = std::getenv("RAC1_WIIU_GAMEPAD") != nullptr ? std::atoi(std::getenv("RAC1_WIIU_GAMEPAD")) : 1;
    const bool menu_on = debug_menu_wanted() && !g_ending;
    if (menu_on) debug_menu_input();
    const bool menu = menu_on && debug_menu_shown();
    if (!menu && drc && (pad_mode == 1 || (pad_mode == 2 && (g_frames & 1) != 0))) {
        GX2CopyColorBufferToScanBuffer(pad_mode == 1 ? WHBGfxGetTVColourBuffer() : WHBGfxGetDRCColourBuffer(), GX2_SCAN_TARGET_DRC);
        GX2SetContextState(WHBGfxGetDRCContextState());  // (as WHBGfxFinishRenderDRC after its copy)
    } else {
        hiz_apply(WHBGfxGetDRCDepthBuffer());
        WHBGfxBeginRenderDRC();
        WHBGfxClearColor(0.0f, 0.0f, 0.0f, 1.0f);
        if (menu) {  // (the GamePad's debug menu in place of the game's picture there)
            debug_menu_draw(WHBGfxGetDRCColourBuffer(), g_menu_stats);
        } else if (drc) {
            draw_frame(WHBGfxGetDRCColourBuffer(), WHBGfxGetDRCDepthBuffer(), false);
        }
        WHBGfxFinishRenderDRC();
    }
    gpu_mark(kGmDrc);
    lap(kTmDrc);
    if (overlap) {  // (WHBGfxFinishRender without its GX2DrawDone)
        GX2SwapScanBuffers();
        gpu_mark(kGmSwap);
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
    movie_tex_free();
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
