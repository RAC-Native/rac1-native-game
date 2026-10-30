// src/profiler/host_profiler.h — the host's frame profiler (openly invented tooling, ADR 0003; not a port): where a host
// frame's time goes, stage by stage, on the CPU (the main thread's wall time) and on the GPU (timestamp queries around
// each stage, read back frames later: never a stall), the worker threads' busy time, the threads' CPU time, the process's
// memory and the GPU's memory. Its view is the debug menu's Profiler tab (src/_devtools/dev_profiler.cpp); RAC1_PROFILER_DUMP,
// RAC1_PROFILER_CSV and RAC1_PROFILER_TRACE (docs/env-vars.md) use it without the menu.
//
// Off (the tab closed, no overlay, no RAC1_PROFILER_*): every zone is one relaxed atomic load, nothing is queried.
// The zones and frame_mark are for the main (GL) thread, the zones also for the game thread (game_thread_attach: its own stack,
// its stages' times added to the frame's while the main thread waits for it); Job is for the worker threads.
#pragma once

#include <atomic>
#include <cstdint>
#include <cstdio>
#include <string>
#include <vector>

namespace rac1::prof {

// The stages of a host frame, in the order they usually run. A zone inside another (the GS 2D inside the effects)
// counts in its own stage only: each stage's time is its "self" time, so the stages add up to the frame.
enum Stage : int {
    kEvents,            // the window's events, the input, the debug menu's poll, the settings applied (the loop's top)
    kGameFrames,        // the game frames' host side: the pad, the MRP's per-frame captures, the RAM hash ...
    kGameCode,          // the game's own update: the EE code on its coroutine (what its sub-stages below leave: the frame's set-up, the
                        // other game modes, the frame's end)
    // the game code's sub-stages (StageInfo::game; zones inside the EE coroutine, nested in kGameCode): where the game's own
    // update goes. The game logic (0x297f78's passes), the rendering preparation done by game code (0x1f85b8's passes: the
    // packets, the VU0 work), and the host's re-runs of game passes for the renderer (the LOD slider's / widened view's)
    kGameLogic,         // the gameplay frame's own work and its passes without a sub-stage (sound, lights, HUD, menus' checks)
    kGameMobyUpdates,   // the mobys' updates (0x2648e0), the sound emitters, the wind
    kGameHero,          // the hero's frame (0x2070d0)
    kGameParticles,     // the particles' update (0x267c08), the spin timers
    kGameCamera,        // the camera (0x1ed0a8)
    kGameRender,        // the frame's drawing (0x1f85b8) without a sub-stage: the view, the zones, the static packets, the drop shadows
    kGameWorldPasses,   // the world passes: sky, tfrag, tie, shrub (visibility, LOD, the VU1 chains)
    kGameMobyPass,      // the moby pass (0x250038): the draw loop, the mobys' packets, the textures
    kGameMobyJoints,    // the mobys' joints and skinning (0x251490's VU0 work), in the real pass and the re-run
    kGameMobyShadows,   // the mobys' drop shadows (0x24fc38: their points, poses and volumes, the VU0 program, the packets)
    kGameEffects,       // the 3D effects' drawing (particles, glows ...) and the draw lists
    kGameWorldEnd,      // the world passes' lighting and ends after the DMA waits, the moby texture slots
    kGameLodMobys,      // host: the moby pass re-run for the LOD slider / the widened view (journaled, the memory put back)
    kGameLodOther,      // host: the world passes', effects' and draw lists' re-runs for the LOD slider / the widened view
    kAudioStep,         // the IOP's mirror and the audio queue stepped (main thread)
    kGameThreadWait,    // the main thread waiting for the game thread's update (its drawing done first, runtime/ee/game_thread.h)
    kGameDrawWait,      // the game thread waiting for the main thread's drawing (the GS's local memory, a GL call asked)
    kPrepare,           // the frame's preparation: the view, what the draw reads of the game's state
    kChainCapture,      // the frame's chain captured (its reading started on the chain worker), the MRP world's scene started
    kSceneCaptureWait,  // waiting for the MRP world's worker to capture the scene's inputs
    kMobyLists,         // the MRP mobys' draw lists
    kDraw,              // the draw's own work: the clears, the state, what is not in a stage below
    kSceneWait,         // waiting for the MRP world's scene build (worker thread)
    kShadowCascades,    // the modern lighting's sun cascades (and the frame's shadow set-up)
    kPointCubes,        // the point lights' shadow cubes
    kWorld,             // the MRP world: ties, tfrags, shrubs, sky
    kMobysLegacy,       // the legacy / GLB moby drawer
    kChainJoinWait,     // waiting for the chain worker (the MRP's reading of the frame's chain, mrp_chain.h; the GS replica's run of it on the faithful path)
    kMobys,             // the MRP mobys
    kAo,                // the ambient occlusion (and the sun's cover probe)
    kSunRays,           // the sun's rays
    kParticles,         // the ambient particles (our own: dust, embers, rain, snow ...)
    kScaleResolve,      // the render scale's buffer put on the window
    kEffects,           // the MRP's 3D effects (particles ...), before and after the mobys
    kGs2d,              // the 2D: the HUD, text, menus, fades (mrp_2d.h; the GS replica's drawing on the faithful path)
    kFrameCapture,      // the frame kept (the pause menu's background, the screen fades')
    kDebugMenu,         // the debug menu (ImGui) and this profiler's own view
    kProfiler,          // the profiler's own work: the GPU's results read back, the threads and memory sampled
    kScreenshots,       // testing: screenshots
    kPacing,            // the frame cap's wait
    kPresent,           // the swap (the present: the v-sync's wait, the driver's queue full)
    kStageCount
};

struct StageInfo {
    const char* name;
    bool wait;      // the main thread waits there (for another thread, the display or the cap): not its own work
    bool gpu;       // timed on the GPU too
    uint32_t color; // ImGui's ABGR
    bool game = false;  // a part of the game code (kGameCode and its sub-stages): Summary::game_code is their sum
};
const StageInfo& stage_info(int s);

// The worker threads whose jobs are timed (their busy wall time and their CPU time, per frame).
// (kWorkerJobs: the job pool's workers, src/jobs, their times summed; kWorkerGame: the game thread's jobs, the update beside the drawing)
// (kWorkerChain: the thread reading the frame's chain, the MRP's chain reader or, on the faithful path, the GS replica's)
enum Worker : int { kWorkerChain, kWorkerScene, kWorkerJobs, kWorkerGame, kWorkerCount };
const char* worker_name(int w);

namespace detail {
extern std::atomic<bool> g_on;
extern uint32_t g_zone_gen;  // (the frame's zones' generation: a zone left open across frames, the EE coroutine suspended in it, is not closed twice)
int zone_begin(Stage s);
void zone_end(int z);
void job_begin(int w, double* t0, double* c0);
void job_end(int w, double t0, double c0, int lane);
}  // namespace detail

// Whether the profiler records this frame.
inline bool on() { return detail::g_on.load(std::memory_order_relaxed); }

// A stage's zone on the main thread (RAII): CPU wall time, and a GPU timestamp pair when the stage has one.
struct Zone {
    int z;
    uint32_t gen;
    explicit Zone(Stage s) : z(on() ? detail::zone_begin(s) : -1), gen(detail::g_zone_gen) {}
    ~Zone() {
        if (z >= 0 && gen == detail::g_zone_gen) detail::zone_end(z);
    }
    Zone(const Zone&) = delete;
    Zone& operator=(const Zone&) = delete;
};
// A worker's job (RAII), on that worker's thread (`lane`: which of the job pool's workers, its own track in a trace).
struct Job {
    int w, lane;
    double t0 = 0, c0 = 0;
    explicit Job(Worker k, int lane_ = 0) : w(on() ? static_cast<int>(k) : -1), lane(lane_) {
        if (w >= 0) detail::job_begin(w, &t0, &c0);
    }
    ~Job() {
        if (w >= 0) detail::job_end(w, t0, c0, lane);
    }
    Job(const Job&) = delete;
    Job& operator=(const Job&) = delete;
};

// A zone opened and closed by hand (-1: none), where a scope does not fit.
inline int begin(Stage s) { return on() ? detail::zone_begin(s) : -1; }
inline void end(int z) {
    if (z >= 0) detail::zone_end(z);
}

#define RAC1_PROF_CAT2(a, b) a##b
#define RAC1_PROF_CAT(a, b) RAC1_PROF_CAT2(a, b)
#define RAC1_PROF_ZONE(stage) ::rac1::prof::Zone RAC1_PROF_CAT(rac1_prof_zone_, __LINE__)(::rac1::prof::stage)

// The calling thread's name in the Threads list (the main thread, the workers, the audio's): once per thread, any time.
void thread_name(const char* name);
// The calling thread is the game thread (runtime/ee/game_thread.h): its zones recorded on a stack of their own. Its jobs run
// only between the main thread's frame_mark calls (the main thread waits for each before the next frame).
void game_thread_attach();

// The host frame's start (the main loop's top): the frame before ends there. Reads back the GPU's results of the frames
// before (when ready), samples the threads and the memory now and then, and decides whether this frame is recorded.
void frame_mark();
// The game frames this host frame runs (its first zone's context).
void frame_steps(int steps);

// The view asks for the profiler this frame (the Profiler tab drawn); `pinned`: the overlay kept on screen.
void request();
void set_pinned(bool on);
bool pinned();
// Recording held (the numbers kept as they are), or going on.
void set_frozen(bool on);
bool frozen();

// ---- What it measured ----

// One host frame, in milliseconds.
struct FrameRec {
    uint64_t index = 0;          // the host frame's number
    double period = 0;           // its start to the next one's
    double cpu[kStageCount] = {};// the main thread's self time per stage
    double cpu_other = 0;        // the main thread's time in no stage
    double gpu[kStageCount] = {};// the GPU's time per stage (self)
    double gpu_other = 0;        // the GPU's time in no GPU-timed stage: idle, or the few commands outside the stages
    double gpu_busy = 0;         // the GPU's time in the stages (with its idle waits inside them, see gpu_boundary)
    double gpu_span = 0;         // the GPU's first to last timestamp of the frame (its idle gaps included)
    bool gpu_ok = false;         // the GPU's results came back
    double main_cpu = 0;         // the main thread's CPU time (the system's account: its waits left out)
    double worker_busy[kWorkerCount] = {}, worker_cpu[kWorkerCount] = {};
    int steps = 0;               // the game frames it ran
};
// The frames recorded, the oldest first (up to history_capacity()).
int history_size();
int history_capacity();
const FrameRec& history_at(int i);  // 0 = the oldest kept

enum class Bound { None, Gpu, MainThread, SceneWorker, ChainWorker, GameThread, Cap };
struct Summary {
    int frames = 0;
    double fps = 0, period_avg = 0, period_min = 0, period_max = 0, period_p99 = 0, low1_fps = 0;  // low1: the worst 1 % of the frames' mean, as fps
    double cpu[kStageCount] = {}, cpu_max[kStageCount] = {}, cpu_other = 0;
    double gpu[kStageCount] = {}, gpu_max[kStageCount] = {}, gpu_other = 0;
    double gpu_busy = 0, gpu_span = 0;
    int gpu_frames = 0;
    double gpu_kernel = -1;  // the GPU's time a frame by the kernel's account (Linux DRM; -1: none): the check on gpu_busy
    double main_work = 0;   // the main thread's wall time minus its waits
    double waits = 0;       // the main thread's waits (the stages marked wait)
    double game_code = 0, host_work = 0;
    double main_cpu = 0;
    double worker_busy[kWorkerCount] = {}, worker_cpu[kWorkerCount] = {};
    double steps = 0;
    Bound bound = Bound::None;
    std::string bound_text;
};
// The last `frames` frames' statistics.
Summary summarize(int frames);

// The threads, sampled about twice a second while the profiler is on: CPU % of one core (100 = one core busy).
struct ThreadRow {
    uint64_t tid = 0;
    std::string name;  // ours ("main", "MRP chain worker" ...) or the system's
    bool ours = false;
    double cpu_pct = 0;
};
struct Resources {
    double sampled_at = 0;            // (steady clock, s; 0 = not yet)
    std::vector<ThreadRow> threads;   // busiest first
    double process_cpu_pct = 0;       // of one core
    int cpus = 1;
    uint64_t rss = 0, rss_peak = 0, virt = 0;
    // the GPU: the kernel's account (Linux DRM) and the GL driver's (NVX / ATI meminfo), bytes
    bool drm = false;
    std::string drm_driver;
    uint64_t drm_vram = 0, drm_gtt = 0, dev_vram_total = 0, dev_vram_used = 0, dev_gtt_total = 0, dev_gtt_used = 0;
    double gpu_busy_pct = -1;         // the kernel's GPU time of this process over the last sample (-1: not given)
    bool nvx = false, ati = false;
    uint64_t nvx_total = 0, nvx_dedicated = 0, nvx_available = 0, nvx_evicted = 0;
    uint64_t ati_tex_free = 0, ati_vbo_free = 0, ati_rb_free = 0;
    std::string gl_renderer, gl_vendor;  // (the renderer's device and driver: OpenGL's strings, or Vulkan's - api)
    std::string api = "OpenGL";
    // Vulkan (VK_EXT_memory_budget): the device-local heaps' size, the driver's budget for this process and its use of them
    bool vk_budget = false;
    uint64_t vk_heap_size = 0, vk_heap_budget = 0, vk_heap_usage = 0;
};
const Resources& resources();

// The GL objects counted (a scan of the texture, buffer and renderbuffer names: main thread, a few ms), per owner.
struct GlOwnerRow {
    std::string owner;
    int textures = 0, buffers = 0, renderbuffers = 0;
    uint64_t tex_bytes = 0, buf_bytes = 0, rb_bytes = 0;
};
void gl_scan(std::vector<GlOwnerRow>* out);
// An allocation's owner for gl_scan (kind: kGlTexture / kGlBuffer / kGlRenderbuffer; any thread with the context: the main).
enum GlKind : int { kGlTexture, kGlBuffer, kGlRenderbuffer };
void gl_track(GlKind kind, unsigned name, const char* owner);
void gl_track_n(GlKind kind, int n, const unsigned* names, const char* owner);

// The GPU side when the game draws with Vulkan (runtime/vk sets it when it starts, docs/vulkan.md); none: OpenGL's queries.
// Its timestamps go in the frame's command buffer (a slot's `per_slot` of them a frame, kept until read); gl_scan then counts
// the renderer's own allocations (images and buffers by use) instead of the GL objects.
struct GpuApi {
    const char* name;                                // ("Vulkan")
    bool (*alloc)(int slots, int per_slot);          // the timestamps' pool (false: none, no GPU timings)
    void (*slot_reset)(int slot);                    // the slot's timestamps made writable again (before its first write)
    void (*write)(int slot, int q);                  // a timestamp at this point of the GPU's stream
    bool (*read)(int slot, int n, uint64_t* ns);     // the slot's n timestamps (ns), false while the last is not there; never waits
    double (*now_ms)();                              // the GPU's clock now, ms (the trace's calibration; may be null)
    void (*resources)(Resources* r);                 // the device's names and memory figures
    void (*scan)(std::vector<GlOwnerRow>* out);      // the allocations, by use
};
void set_gpu_api(const GpuApi* api);

// ---- Exports ----
// The last `frames` frames as CSV (a row a frame, the stages' CPU and GPU ms). False when the file cannot be written.
bool export_csv(const char* path, int frames);
// A Chrome trace (chrome://tracing, ui.perfetto.dev) of the next `frames` frames: the main thread's zones, the GPU's,
// the workers' jobs; written when they are done (trace_status says where).
bool trace_start(const char* path, int frames);
bool trace_active();
std::string trace_status();

// The text summary of the last `frames` frames (RAC1_PROFILER_DUMP), with the threads and the memory.
void dump(std::FILE* f, int frames, bool with_gl_scan);
// At exit: a pending trace written, the CSV closed.
void shutdown();

}  // namespace rac1::prof
