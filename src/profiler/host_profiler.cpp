// src/profiler/host_profiler.cpp — the host's frame profiler (openly invented tooling, ADR 0003): see host_profiler.h.
#include "profiler/host_profiler.h"

#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include "platform/platform.h"

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <cstring>
#include <map>
#include <mutex>
#include <unordered_map>

namespace rac1::prof {

namespace {

constexpr uint32_t rgb(uint32_t r, uint32_t g, uint32_t b) { return 0xff000000u | (b << 16) | (g << 8) | r; }  // (ImGui's ABGR)

const StageInfo kStages[kStageCount] = {
    {"Events & input", false, false, rgb(120, 120, 120)},
    {"Game frames (host side)", false, false, rgb(95, 140, 200)},
    {"Game code (EE update, other)", false, false, rgb(60, 110, 230), true},
    {"Game: logic (other)", false, false, rgb(70, 90, 200), true},
    {"Game: moby updates", false, false, rgb(90, 130, 255), true},
    {"Game: hero", false, false, rgb(40, 80, 180), true},
    {"Game: particles update", false, false, rgb(120, 160, 255), true},
    {"Game: camera", false, false, rgb(50, 60, 150), true},
    {"Game: render prep (other)", false, false, rgb(30, 140, 200), true},
    {"Game: world passes", false, false, rgb(40, 170, 220), true},
    {"Game: moby pass (draw loop)", false, false, rgb(20, 120, 170), true},
    {"Game: moby joints / skinning (VU0)", false, false, rgb(80, 200, 240), true},
    {"Game: effects draw, draw lists", false, false, rgb(30, 100, 140), true},
    {"Game: world lighting / pass ends", false, false, rgb(100, 180, 210), true},
    {"Host: LOD / wide view moby re-run", false, false, rgb(140, 100, 240), true},
    {"Host: LOD / wide view other re-runs", false, false, rgb(170, 140, 250), true},
    {"Audio step (IOP mirror)", false, false, rgb(150, 110, 220)},
    {"Wait: game thread", true, false, rgb(120, 170, 230)},
    {"Game thread: wait for the draw", true, false, rgb(100, 100, 160)},
    {"Prepare", false, false, rgb(70, 180, 190)},
    {"Chain capture / scene start", false, true, rgb(40, 150, 160)},
    {"Wait: scene capture", true, false, rgb(200, 200, 90)},
    {"Moby draw lists", false, true, rgb(110, 200, 170)},
    {"Draw (other)", false, false, rgb(160, 160, 160)},
    {"Wait: MRP scene build", true, false, rgb(230, 210, 60)},
    {"Shadows: sun cascades", false, true, rgb(140, 90, 60)},
    {"Shadows: point cubes", false, true, rgb(190, 120, 70)},
    {"World (ties, tfrags, shrubs, sky)", false, true, rgb(80, 190, 80)},
    {"Mobys (legacy drawer)", false, true, rgb(100, 120, 60)},
    {"Wait: chain worker", true, false, rgb(240, 170, 40)},
    {"Mobys (MRP)", false, true, rgb(60, 220, 140)},
    {"Ambient occlusion", false, true, rgb(200, 80, 160)},
    {"Sun rays", false, true, rgb(250, 230, 120)},
    {"Ambient particles", false, true, rgb(170, 220, 250)},
    {"Render scale resolve", false, true, rgb(130, 130, 200)},
    {"Effects (MRP fx)", false, true, rgb(240, 110, 60)},
    {"2D, HUD", false, true, rgb(80, 120, 250)},
    {"Frame capture", false, true, rgb(170, 140, 120)},
    {"Debug menu / profiler view", false, true, rgb(200, 200, 200)},
    {"Profiler (readback, sampling)", false, false, rgb(255, 0, 255)},
    {"Screenshots", false, true, rgb(110, 110, 110)},
    {"Wait: frame cap", true, false, rgb(70, 70, 70)},
    {"Present (swap, v-sync)", true, true, rgb(230, 60, 60)},
};
const char* const kWorkerNames[kWorkerCount] = {"Chain worker", "MRP scene worker", "Job workers (summed)", "Game thread"};
constexpr int kJobTrack = 10;  // (a trace's track of the job pool's worker k: kJobTrack + k)

double now_ms() { return std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now().time_since_epoch()).count(); }

struct ZoneRec {
    int16_t stage = 0, parent = -1;
    double t0 = 0, t1 = 0;
};
// The GPU's time line cut at each boundary of a GPU-timed zone into segments, each given to the innermost GPU-timed
// zone open over it (-1: none, the GPU's "no stage": idle, or the few commands outside the stages).
struct Seg {  // (consecutive segments share their boundary's timestamp)
    int16_t stage = -1;
    int q0 = -1, q1 = -1;
};

constexpr int kSlots = 32;     // the frames whose GPU results may be in flight (a hidden or uncapped window runs far ahead of the GPU)
constexpr int kQueries = 256;  // the timestamps one frame may take (one a GPU stage boundary)
struct Slot {
    bool pending = false;
    uint64_t frame = 0;
    int nq = 0;
    bool done = false;  // (the frame's last segment closed)
    GLuint q[kQueries] = {};
    std::vector<Seg> segs;
    double calib = 0;  // (tracing: the CPU's ms minus the GPU's at the frame's start)
    bool trace = false;
};

struct TraceEv {
    const char* name;
    int track;  // 0 the main thread, 1 the GPU, 2 + w the workers (the job pool's: kJobTrack + its lane), 9 the frames
    double ts, dur;
};

constexpr int kHistory = 1200;

struct State {
    bool init = false;
    int dump_every = 0;  // RAC1_PROFILER_DUMP
    std::FILE* csv = nullptr;  // RAC1_PROFILER_CSV
    // RAC1_PROFILER_TRACE=<file>[,<from>[,<frames>]]
    std::string env_trace_path;
    uint64_t env_trace_from = 0;
    int env_trace_frames = 0;
    bool requested = false, pinned = false, frozen = false;
    bool want_prev = false;
    uint64_t frame_index = 0;
    bool rec = false;
    double t_frame0 = 0, main_cpu0 = 0;
    int steps = 0;
    double cpu[kStageCount] = {};
    std::vector<ZoneRec> zones;
    int stack[64] = {};
    int depth = 0;
    // the game thread's zones (game_thread_attach), their stages' self times; indices from kGameZone0
    double gcpu[kStageCount] = {};
    std::vector<ZoneRec> gzones;
    int gstack[64] = {};
    int gdepth = 0;
    bool gl_ready = false;
    Slot slots[kSlots];
    int cur = -1;
    std::vector<FrameRec> hist;
    int hist_head = 0, hist_n = 0;
    // tracing
    std::string trace_path, trace_msg;
    uint64_t trace_from = 0, trace_to = 0;
    bool tracing = false, trace_frame = false;
    double trace_t0 = 0;
    std::vector<TraceEv> trace;
    // resources
    Resources res;
    double res_t = 0, drm_t = 0;
    std::unordered_map<uint64_t, double> thread_prev;
    double proc_cpu_prev = -1, drm_engine_prev = -1;
    bool gl_ext_checked = false;
    // the GPU's results: read back, dropped (not back after kSlots frames), and their age when read (frames)
    uint64_t gpu_read = 0, gpu_dropped = 0, gpu_age_sum = 0;
};
State& S() {
    static State s;
    return s;
}

std::mutex g_jobs_mutex;  // (the trace's events from the workers; the thread names)
std::atomic<int64_t> g_wbusy_us[kWorkerCount], g_wcpu_us[kWorkerCount];
std::atomic<bool> g_trace_on{false};
std::vector<TraceEv> g_worker_trace;
std::unordered_map<uint64_t, std::string> g_thread_names;
thread_local bool t_main = false;
thread_local bool t_game = false;
constexpr int kGameZone0 = 1 << 20;  // (a game thread zone's index: kGameZone0 + its place in State::gzones)

std::mutex g_gl_mutex;
std::map<std::pair<int, unsigned>, const char*> g_gl_owner;
const GpuApi* g_api = nullptr;  // (set_gpu_api: Vulkan's timestamps and memory; none: OpenGL's)
bool g_api_ok = false;

// the CSV's first row (the stages' names quoted: some hold commas)
void csv_header(std::FILE* f) {
    std::fprintf(f, "frame,period_ms,steps,main_cpu_ms,cpu_other_ms,gpu_ok,gpu_busy_ms,gpu_span_ms,gpu_other_ms");
    for (int i = 0; i < kStageCount; ++i) std::fprintf(f, ",\"cpu: %s\"", kStages[i].name);
    for (int i = 0; i < kStageCount; ++i)
        if (kStages[i].gpu) std::fprintf(f, ",\"gpu: %s\"", kStages[i].name);
    for (int w = 0; w < kWorkerCount; ++w) std::fprintf(f, ",\"busy: %s\",\"cpu: %s\"", kWorkerNames[w], kWorkerNames[w]);
    std::fprintf(f, "\n");
}

void env_init(State& s) {
    if (s.init) return;
    s.init = true;
    s.hist.resize(kHistory);
    if (const char* d = std::getenv("RAC1_PROFILER_DUMP")) s.dump_every = std::max(1, std::atoi(d) > 0 ? std::atoi(d) : 120);
    if (const char* c = std::getenv("RAC1_PROFILER_CSV")) {
        s.csv = std::fopen(c, "w");
        if (s.csv != nullptr) {
            csv_header(s.csv);
        }
    }
    if (const char* t = std::getenv("RAC1_PROFILER_TRACE")) {
        const char* comma = std::strchr(t, ',');
        s.env_trace_path = comma != nullptr ? std::string(t, comma) : std::string(t);
        unsigned long long from = 120;
        int frames = 300;
        if (comma != nullptr) std::sscanf(comma + 1, "%llu,%d", &from, &frames);
        s.env_trace_from = from;
        s.env_trace_frames = std::max(1, frames);
    }
}

FrameRec* hist_find(State& s, uint64_t index) {
    for (int k = 0; k < std::min(s.hist_n, kSlots + 4); ++k) {
        FrameRec& r = s.hist[static_cast<size_t>((s.hist_head - 1 - k + kHistory) % kHistory)];
        if (r.index == index) return &r;
    }
    return nullptr;
}

void csv_row(std::FILE* f, const FrameRec& r) {
    std::fprintf(f, "%llu,%.4f,%d,%.4f,%.4f,%d,%.4f,%.4f,%.4f", static_cast<unsigned long long>(r.index), r.period, r.steps, r.main_cpu, r.cpu_other, r.gpu_ok ? 1 : 0, r.gpu_busy, r.gpu_span,
                 r.gpu_other);
    for (int i = 0; i < kStageCount; ++i) std::fprintf(f, ",%.4f", r.cpu[i]);
    for (int i = 0; i < kStageCount; ++i)
        if (kStages[i].gpu) std::fprintf(f, ",%.4f", r.gpu[i]);
    for (int w = 0; w < kWorkerCount; ++w) std::fprintf(f, ",%.4f,%.4f", r.worker_busy[w], r.worker_cpu[w]);
    std::fprintf(f, "\n");
}

// a frame's slot read back when its last timestamp is there (never waited for); false: not yet
bool slot_read(State& s, Slot& sl, bool drop_if_not_ready) {
    if (!sl.pending) return true;
    GLint avail = 0;
    std::vector<GLuint64> ts(static_cast<size_t>(sl.nq), 0);
    if (g_api != nullptr) {  // (Vulkan: the slot's timestamps all at once, when the last is there)
        if (sl.nq > 0 && g_api->read(static_cast<int>(&sl - s.slots), sl.nq, ts.data())) avail = 1;
    } else if (sl.nq > 0) {
        glGetQueryObjectiv(sl.q[sl.nq - 1], GL_QUERY_RESULT_AVAILABLE, &avail);
    }
    if (avail == 0 && !drop_if_not_ready) return false;
    sl.pending = false;
    FrameRec* r = hist_find(s, sl.frame);
    if (avail == 0) {  // (dropped: its results never waited for)
        ++s.gpu_dropped;
        if (r != nullptr && s.csv != nullptr) csv_row(s.csv, *r);
        return true;
    }
    ++s.gpu_read;
    s.gpu_age_sum += s.frame_index - sl.frame;
    if (g_api == nullptr)
        for (int i = 0; i < sl.nq; ++i) glGetQueryObjectui64v(sl.q[i], GL_QUERY_RESULT, &ts[static_cast<size_t>(i)]);
    auto ms = [&ts](int a, int b) { return static_cast<double>(static_cast<int64_t>(ts[static_cast<size_t>(b)] - ts[static_cast<size_t>(a)])) * 1e-6; };
    double gpu[kStageCount] = {};
    double busy = 0;
    int first = -1, last = -1;
    for (const Seg& g : sl.segs) {
        if (g.q0 < 0 || g.q1 < 0) continue;
        const double d = std::max(0.0, ms(g.q0, g.q1));
        if (g.stage >= 0) {
            gpu[g.stage] += d;
            busy += d;
        }
        if (first < 0) first = g.q0;
        last = g.q1;
        if (sl.trace && d > 0.0005)
            s.trace.push_back({g.stage >= 0 ? kStages[g.stage].name : "(no stage)", 1, static_cast<double>(ts[static_cast<size_t>(g.q0)]) * 1e-6 + sl.calib, d});
    }
    if (r != nullptr) {
        for (int i = 0; i < kStageCount; ++i) r->gpu[i] = gpu[i];
        r->gpu_busy = busy;
        r->gpu_span = first >= 0 ? ms(first, last) : 0.0;
        r->gpu_other = std::max(0.0, r->gpu_span - busy);
        r->gpu_ok = true;
        if (s.csv != nullptr) csv_row(s.csv, *r);
    }
    return true;
}

void trace_write(State& s) {
    s.tracing = false;
    g_trace_on.store(false);
    {
        std::lock_guard<std::mutex> lk(g_jobs_mutex);
        s.trace.insert(s.trace.end(), g_worker_trace.begin(), g_worker_trace.end());
        g_worker_trace.clear();
    }
    std::FILE* f = std::fopen(s.trace_path.c_str(), "w");
    if (f == nullptr) {
        s.trace_msg = "could not write " + s.trace_path;
        s.trace.clear();
        return;
    }
    std::fprintf(f, "{\"displayTimeUnit\":\"ms\",\"traceEvents\":[\n");
    const char* tracks[] = {"main thread (CPU)", "GPU", "Chain worker", "MRP scene worker"};
    for (int t = 0; t < 4; ++t) std::fprintf(f, "{\"ph\":\"M\",\"pid\":1,\"tid\":%d,\"name\":\"thread_name\",\"args\":{\"name\":\"%s\"}},\n", t, tracks[t]);
    std::fprintf(f, "{\"ph\":\"M\",\"pid\":1,\"tid\":9,\"name\":\"thread_name\",\"args\":{\"name\":\"host frames\"}},\n");
    for (int k = 0; k < 16; ++k)
        std::fprintf(f, "{\"ph\":\"M\",\"pid\":1,\"tid\":%d,\"name\":\"thread_name\",\"args\":{\"name\":\"job worker %d\"}},\n", kJobTrack + k, k + 1);
    bool first = true;
    for (const TraceEv& e : s.trace) {
        std::fprintf(f, "%s{\"ph\":\"X\",\"pid\":1,\"tid\":%d,\"name\":\"%s\",\"ts\":%.3f,\"dur\":%.3f}", first ? "" : ",\n", e.track, e.name, (e.ts - s.trace_t0) * 1000.0,
                     std::max(0.0, e.dur) * 1000.0);
        first = false;
    }
    std::fprintf(f, "\n]}\n");
    std::fclose(f);
    char m[512];
    std::snprintf(m, sizeof m, "wrote %s (%zu events, frames %llu-%llu)", s.trace_path.c_str(), s.trace.size(), static_cast<unsigned long long>(s.trace_from),
                  static_cast<unsigned long long>(s.trace_to - 1));
    s.trace_msg = m;
    std::fprintf(stderr, "[profiler] trace: %s\n", m);
    s.trace.clear();
}

void gl_ext_check(State& s) {
    if (s.gl_ext_checked) return;
    s.gl_ext_checked = true;
    if (g_api != nullptr) return;  // (Vulkan: its own names, GpuApi::resources)
    GLint n = 0;
    glGetIntegerv(GL_NUM_EXTENSIONS, &n);
    for (GLint i = 0; i < n; ++i) {
        const char* e = reinterpret_cast<const char*>(glGetStringi(GL_EXTENSIONS, static_cast<GLuint>(i)));
        if (e == nullptr) continue;
        if (std::strcmp(e, "GL_NVX_gpu_memory_info") == 0) s.res.nvx = true;
        if (std::strcmp(e, "GL_ATI_meminfo") == 0) s.res.ati = true;
    }
    if (const GLubyte* r = glGetString(GL_RENDERER)) s.res.gl_renderer = reinterpret_cast<const char*>(r);
    if (const GLubyte* v = glGetString(GL_VENDOR)) s.res.gl_vendor = reinterpret_cast<const char*>(v);
}

void resources_sample(State& s, bool force) {
    const double t = now_ms();
    if (!force && s.res_t != 0 && t - s.res_t < 500.0) return;
    const double dt = s.res_t != 0 ? t - s.res_t : 0.0;
    s.res_t = t;
    Resources& R = s.res;
    R.sampled_at = t * 1e-3;
    R.cpus = platform::cpu_count();
    std::vector<platform::ThreadTime> th;
    if (platform::process_threads(&th)) {
        std::unordered_map<uint64_t, double> now_map;
        std::vector<ThreadRow> rows;
        std::lock_guard<std::mutex> lk(g_jobs_mutex);
        for (const platform::ThreadTime& x : th) {
            now_map[x.tid] = x.cpu_ms;
            ThreadRow row;
            row.tid = x.tid;
            const auto it = g_thread_names.find(x.tid);
            row.ours = it != g_thread_names.end();
            row.name = row.ours ? it->second : (x.name.empty() ? "thread " + std::to_string(x.tid) : x.name);
            const auto pv = s.thread_prev.find(x.tid);
            row.cpu_pct = (pv != s.thread_prev.end() && dt > 0) ? std::max(0.0, x.cpu_ms - pv->second) / dt * 100.0 : 0.0;
            rows.push_back(std::move(row));
        }
        std::sort(rows.begin(), rows.end(), [](const ThreadRow& a, const ThreadRow& b) { return a.cpu_pct > b.cpu_pct; });
        R.threads = std::move(rows);
        s.thread_prev = std::move(now_map);
    }
    const double pc = platform::process_cpu_ms();
    R.process_cpu_pct = (s.proc_cpu_prev >= 0 && dt > 0) ? (pc - s.proc_cpu_prev) / dt * 100.0 : 0.0;
    s.proc_cpu_prev = pc;
    platform::ProcessMemory pm;
    if (platform::process_memory(&pm)) {
        R.rss = pm.resident;
        R.rss_peak = pm.resident_peak;
        R.virt = pm.virtual_size;
    }
    if (s.drm_t == 0 || t - s.drm_t >= 1000.0 || force) {  // (the DRM's file descriptors read once a second)
        platform::GpuClientInfo g;
        R.drm = platform::gpu_client_info(&g);
        if (R.drm) {
            R.drm_driver = g.driver;
            R.drm_vram = g.vram;
            R.drm_gtt = g.gtt;
            R.dev_vram_total = g.vram_total;
            R.dev_vram_used = g.vram_used;
            R.dev_gtt_total = g.gtt_total;
            R.dev_gtt_used = g.gtt_used;
            if (g.engine_ms >= 0 && s.drm_engine_prev >= 0 && s.drm_t != 0) R.gpu_busy_pct = (g.engine_ms - s.drm_engine_prev) / (t - s.drm_t) * 100.0;
            s.drm_engine_prev = g.engine_ms;
        }
        s.drm_t = t;
    }
    gl_ext_check(s);
    if (g_api != nullptr && g_api->resources != nullptr) g_api->resources(&R);
    if (R.nvx) {
        GLint v = 0;
        glGetIntegerv(0x9047, &v);  // GPU_MEMORY_INFO_DEDICATED_VIDMEM_NVX (KiB)
        R.nvx_dedicated = static_cast<uint64_t>(v) * 1024;
        glGetIntegerv(0x9048, &v);  // GPU_MEMORY_INFO_TOTAL_AVAILABLE_MEMORY_NVX
        R.nvx_total = static_cast<uint64_t>(v) * 1024;
        glGetIntegerv(0x9049, &v);  // GPU_MEMORY_INFO_CURRENT_AVAILABLE_VIDMEM_NVX
        R.nvx_available = static_cast<uint64_t>(v) * 1024;
        glGetIntegerv(0x904B, &v);  // GPU_MEMORY_INFO_EVICTED_MEMORY_NVX
        R.nvx_evicted = static_cast<uint64_t>(v) * 1024;
    }
    if (R.ati) {
        GLint v[4] = {};
        glGetIntegerv(0x87FC, v);  // TEXTURE_FREE_MEMORY_ATI: the pool's free KiB first
        R.ati_tex_free = static_cast<uint64_t>(v[0]) * 1024;
        glGetIntegerv(0x87FB, v);  // VBO_FREE_MEMORY_ATI
        R.ati_vbo_free = static_cast<uint64_t>(v[0]) * 1024;
        glGetIntegerv(0x87FD, v);  // RENDERBUFFER_FREE_MEMORY_ATI
        R.ati_rb_free = static_cast<uint64_t>(v[0]) * 1024;
    }
}

// a boundary of the GPU's segments: one timestamp, the open segment's end and the next one's start (given to `stage`;
// -2: the frame's end). No flush: a flush per boundary made the driver submit (and the GPU drain) dozens of times a frame,
// a run 50 % slower. So a segment's time is the GPU's from its first command to its last, and holds the GPU's idle wait
// for the CPU to record and submit them when the GPU is ahead (CPU-bound frames); GPU-bound, the GPU's queue is never
// empty and the segments are its work
void gpu_boundary(State& s, int stage) {
    if (s.cur < 0) return;
    Slot& sl = s.slots[s.cur];
    if (sl.done) return;
    if (sl.nq >= kQueries) {
        sl.done = true;
        return;
    }
    const int q = sl.nq++;
    if (g_api != nullptr) g_api->write(s.cur, q);
    else glQueryCounter(sl.q[q], GL_TIMESTAMP);
    if (!sl.segs.empty() && sl.segs.back().q1 < 0) sl.segs.back().q1 = q;
    if (stage == -2 || sl.nq + 2 >= kQueries) {  // (the frame's end, or no queries left: the rest not timed)
        sl.done = true;
        return;
    }
    Seg g;
    g.stage = static_cast<int16_t>(stage);
    g.q0 = q;
    sl.segs.push_back(g);
}
// the innermost GPU-timed zone open (the stack's top down), -1 none
int gpu_stage_open(const State& s) {
    for (int d = s.depth - 1; d >= 0; --d) {
        const int st = s.zones[static_cast<size_t>(s.stack[d])].stage;
        if (kStages[st].gpu) return st;
    }
    return -1;
}

}  // namespace

namespace detail {
std::atomic<bool> g_on{false};
uint32_t g_zone_gen = 0;

int zone_begin(Stage st) {
    State& s = S();
    if (t_game) {  // (the game thread's own stack; its stages are the CPU's only)
        if (!s.rec || s.gdepth >= 64) return -1;
        ZoneRec z;
        z.stage = static_cast<int16_t>(st);
        z.parent = static_cast<int16_t>(s.gdepth > 0 ? s.gstack[s.gdepth - 1] : -1);
        z.t0 = now_ms();
        const int idx = static_cast<int>(s.gzones.size());
        s.gzones.push_back(z);
        s.gstack[s.gdepth++] = idx;
        return kGameZone0 + idx;
    }
    if (!t_main || !s.rec || s.depth >= 64) return -1;
    ZoneRec z;
    z.stage = static_cast<int16_t>(st);
    z.parent = static_cast<int16_t>(s.depth > 0 ? s.stack[s.depth - 1] : -1);
    if (kStages[st].gpu && gpu_stage_open(s) != st) gpu_boundary(s, st);
    z.t0 = now_ms();
    const int idx = static_cast<int>(s.zones.size());
    s.zones.push_back(z);
    s.stack[s.depth++] = idx;
    return idx;
}

void zone_end(int idx) {
    State& s = S();
    if (idx >= kGameZone0) {
        const int gi = idx - kGameZone0;
        if (!t_game || gi >= static_cast<int>(s.gzones.size())) return;
        ZoneRec& z = s.gzones[static_cast<size_t>(gi)];
        z.t1 = now_ms();
        const double incl = z.t1 - z.t0;
        s.gcpu[z.stage] += incl;
        if (z.parent >= 0) s.gcpu[s.gzones[static_cast<size_t>(z.parent)].stage] -= incl;
        for (int d = s.gdepth - 1; d >= 0; --d)
            if (s.gstack[d] == gi) {
                s.gdepth = d;
                break;
            }
        if (s.trace_frame) {
            std::lock_guard<std::mutex> lk(g_jobs_mutex);
            g_worker_trace.push_back({kStages[z.stage].name, 2 + kWorkerGame, z.t0, incl});
        }
        return;
    }
    if (!t_main || idx >= static_cast<int>(s.zones.size())) return;  // (a main thread's zone ended on the game thread: left)  // (a frame_mark inside the zone: it belongs to no frame now)
    ZoneRec& z = s.zones[static_cast<size_t>(idx)];
    z.t1 = now_ms();
    const double incl = z.t1 - z.t0;
    s.cpu[z.stage] += incl;
    if (z.parent >= 0) s.cpu[s.zones[static_cast<size_t>(z.parent)].stage] -= incl;
    const int16_t st = z.stage;
    for (int d = s.depth - 1; d >= 0; --d)  // (its own place in the stack: a zone left open inside it is closed with it)
        if (s.stack[d] == idx) {
            s.depth = d;
            break;
        }
    if (kStages[st].gpu) {
        const int open = gpu_stage_open(s);
        if (open != st) gpu_boundary(s, open);
    }
    if (s.trace_frame) s.trace.push_back({kStages[z.stage].name, 0, z.t0, incl});
}

void job_begin(int, double* t0, double* c0) {
    *t0 = now_ms();
    *c0 = platform::thread_cpu_ms();
}

void job_end(int w, double t0, double c0, int lane) {
    const double t1 = now_ms();
    g_wbusy_us[w].fetch_add(static_cast<int64_t>((t1 - t0) * 1000.0), std::memory_order_relaxed);
    g_wcpu_us[w].fetch_add(static_cast<int64_t>((platform::thread_cpu_ms() - c0) * 1000.0), std::memory_order_relaxed);
    if (g_trace_on.load(std::memory_order_relaxed)) {
        std::lock_guard<std::mutex> lk(g_jobs_mutex);
        g_worker_trace.push_back({w == kWorkerJobs ? "job" : kWorkerNames[w], w == kWorkerJobs ? kJobTrack + lane : 2 + w, t0, t1 - t0});
    }
}
}  // namespace detail

const StageInfo& stage_info(int s) { return kStages[std::clamp(s, 0, kStageCount - 1)]; }
const char* worker_name(int w) { return kWorkerNames[std::clamp(w, 0, kWorkerCount - 1)]; }

void game_thread_attach() { t_game = true; }

void thread_name(const char* name) {
    std::lock_guard<std::mutex> lk(g_jobs_mutex);
    g_thread_names[platform::thread_id()] = name;
}

void request() { S().requested = true; }
void set_pinned(bool on) { S().pinned = on; }
bool pinned() { return S().pinned; }
void set_frozen(bool on) { S().frozen = on; }
bool frozen() { return S().frozen; }
void frame_steps(int steps) { S().steps = steps; }

void frame_mark() {
    State& s = S();
    env_init(s);
    if (!t_main) {
        t_main = true;
        thread_name("main (game, GL)");
    }
    const double now = now_ms();
    // the frame that ends here
    if (s.rec) {
        if (s.cur >= 0) {
            gpu_boundary(s, -2);
            Slot& sl = s.slots[s.cur];
            sl.pending = sl.nq > 0;
            sl.frame = s.frame_index;
            sl.trace = s.trace_frame;
        }
        FrameRec r;
        r.index = s.frame_index;
        r.period = now - s.t_frame0;
        double sum = 0;
        for (int i = 0; i < kStageCount; ++i) {
            r.cpu[i] = std::max(0.0, s.cpu[i]);
            sum += r.cpu[i];
        }
        r.cpu_other = std::max(0.0, r.period - sum);
        // (the game thread's stages: its update beside the main thread's drawing - counted in the stages, not in the main
        // thread's frame; its busy time in worker_busy[kWorkerGame])
        for (int i = 0; i < kStageCount; ++i) r.cpu[i] += std::max(0.0, s.gcpu[i]);
        r.main_cpu = platform::thread_cpu_ms() - s.main_cpu0;
        for (int w = 0; w < kWorkerCount; ++w) {
            r.worker_busy[w] = static_cast<double>(g_wbusy_us[w].exchange(0)) * 1e-3;
            r.worker_cpu[w] = static_cast<double>(g_wcpu_us[w].exchange(0)) * 1e-3;
        }
        r.steps = s.steps;
        if (s.trace_frame) s.trace.push_back({"host frame", 9, s.t_frame0, r.period});
        const bool keep = !s.frozen || s.dump_every > 0;
        if (keep) {
            s.hist[static_cast<size_t>(s.hist_head)] = r;
            s.hist_head = (s.hist_head + 1) % kHistory;
            s.hist_n = std::min(s.hist_n + 1, kHistory);
        }
        if (s.cur < 0 && s.csv != nullptr) csv_row(s.csv, r);
        s.zones.clear();
        s.depth = 0;
        s.gzones.clear();
        s.gdepth = 0;
        std::fill(std::begin(s.gcpu), std::end(s.gcpu), 0.0);
        s.rec = false;
        ++detail::g_zone_gen;
    }
    // RAC1_PROFILER_DUMP: the summary every n frames (below, in the new frame's "Profiler" stage)
    const bool dump_due = s.dump_every > 0 && s.frame_index > 0 && s.frame_index % static_cast<uint64_t>(s.dump_every) == 0;
    ++s.frame_index;
    // a trace asked for by RAC1_PROFILER_TRACE
    if (!s.env_trace_path.empty() && s.frame_index == s.env_trace_from) {
        trace_start(s.env_trace_path.c_str(), s.env_trace_frames);
        s.env_trace_path.clear();
    }
    if (s.tracing) {
        s.trace_frame = s.frame_index >= s.trace_from && s.frame_index < s.trace_to;
        if (s.frame_index == s.trace_from) {
            s.trace_t0 = now;
            g_trace_on.store(true);
        }
        if (s.frame_index >= s.trace_to + kSlots + 1) trace_write(s);
    }
    // this frame: recorded or not
    const bool want = s.dump_every > 0 || s.csv != nullptr || s.tracing || ((s.pinned || s.requested) && !s.frozen);
    s.requested = false;
    detail::g_on.store(want, std::memory_order_relaxed);
    if (!want) {
        if (s.want_prev)
            for (Slot& sl : s.slots) sl.pending = false;  // (their results left: the next start begins anew)
        s.want_prev = false;
        s.cur = -1;
        for (int w = 0; w < kWorkerCount; ++w) g_wbusy_us[w].store(0), g_wcpu_us[w].store(0);
        return;
    }
    if (!s.want_prev) {
        s.thread_prev.clear();
        s.proc_cpu_prev = -1;
        s.res_t = 0;
    }
    s.want_prev = true;
    if (!s.gl_ready) {
        if (g_api != nullptr) g_api_ok = g_api->alloc(kSlots, kQueries);
        else
            for (Slot& sl : s.slots) glGenQueries(kQueries, sl.q);
        s.gl_ready = true;
    }
    s.rec = true;
    s.t_frame0 = now;
    s.main_cpu0 = platform::thread_cpu_ms();
    std::fill(std::begin(s.cpu), std::end(s.cpu), 0.0);
    s.zones.clear();
    s.depth = 0;
    std::fill(std::begin(s.gcpu), std::end(s.gcpu), 0.0);
    s.gzones.clear();
    s.gdepth = 0;
    ++detail::g_zone_gen;
    // the profiler's own work in a stage of its own: the GPU's results read back (with Mesa's GL thread, a query's
    // result is a sync with it: the driver's back-pressure, met there instead of at the next GL call that syncs), the
    // threads and the memory sampled, the dump
    const int self = detail::zone_begin(kProfiler);
    s.cur = static_cast<int>(s.frame_index % kSlots);
    if (s.gl_ready)  // (the GPU's results of the frames before, those ready, oldest first)
        for (int k = 1; k <= kSlots; ++k) {
            Slot& old = s.slots[(s.cur + k) % kSlots];
            if (old.pending && !slot_read(s, old, false)) break;
        }
    Slot& sl = s.slots[s.cur];
    if (sl.pending) slot_read(s, sl, true);  // (its results not back after kSlots frames: dropped, never waited for)
    sl.nq = 0;
    sl.segs.clear();
    sl.done = g_api != nullptr && !g_api_ok;  // (no timestamps: no GPU timings)
    if (g_api != nullptr && g_api_ok) g_api->slot_reset(s.cur);
    gpu_boundary(s, -1);  // (the frame's first segment: no stage yet)
    if (s.trace_frame) {  // (the GPU's clock against the CPU's: its timestamps placed on the trace's time line)
        if (g_api != nullptr) {
            sl.calib = g_api->now_ms != nullptr ? now_ms() - g_api->now_ms() : 0.0;
        } else {
            GLint64 g = 0;
            glGetInteger64v(GL_TIMESTAMP, &g);
            sl.calib = now_ms() - static_cast<double>(g) * 1e-6;
        }
    }
    resources_sample(s, false);
    if (dump_due) dump(stdout, s.dump_every, true);
    detail::zone_end(self);
}

int history_size() { return S().hist_n; }
int history_capacity() { return kHistory; }
const FrameRec& history_at(int i) {
    const State& s = S();
    return s.hist[static_cast<size_t>((s.hist_head - s.hist_n + i + kHistory * 2) % kHistory)];
}

Summary summarize(int frames) {
    Summary m;
    const int n = std::min(frames, history_size());
    if (n <= 0) return m;
    std::vector<double> periods;
    periods.reserve(static_cast<size_t>(n));
    m.period_min = 1e30;
    for (int k = history_size() - n; k < history_size(); ++k) {
        const FrameRec& r = history_at(k);
        periods.push_back(r.period);
        m.period_avg += r.period;
        m.period_min = std::min(m.period_min, r.period);
        m.period_max = std::max(m.period_max, r.period);
        for (int i = 0; i < kStageCount; ++i) {
            m.cpu[i] += r.cpu[i];
            m.cpu_max[i] = std::max(m.cpu_max[i], r.cpu[i]);
            if (kStages[i].wait) m.waits += r.cpu[i];
        }
        m.cpu_other += r.cpu_other;
        m.main_cpu += r.main_cpu;
        m.steps += r.steps;
        for (int w = 0; w < kWorkerCount; ++w) {
            m.worker_busy[w] += r.worker_busy[w];
            m.worker_cpu[w] += r.worker_cpu[w];
        }
        if (r.gpu_ok) {
            ++m.gpu_frames;
            for (int i = 0; i < kStageCount; ++i) {
                m.gpu[i] += r.gpu[i];
                m.gpu_max[i] = std::max(m.gpu_max[i], r.gpu[i]);
            }
            m.gpu_other += r.gpu_other;
            m.gpu_busy += r.gpu_busy;
            m.gpu_span += r.gpu_span;
        }
    }
    m.frames = n;
    const double inv = 1.0 / n;
    m.period_avg *= inv;
    for (int i = 0; i < kStageCount; ++i) m.cpu[i] *= inv;
    m.cpu_other *= inv;
    m.main_cpu *= inv;
    m.steps *= inv;
    m.waits *= inv;
    for (int w = 0; w < kWorkerCount; ++w) {
        m.worker_busy[w] *= inv;
        m.worker_cpu[w] *= inv;
    }
    if (m.gpu_frames > 0) {
        const double g = 1.0 / m.gpu_frames;
        for (int i = 0; i < kStageCount; ++i) m.gpu[i] *= g;
        m.gpu_other *= g;
        m.gpu_busy *= g;
        m.gpu_span *= g;
    }
    m.fps = m.period_avg > 0 ? 1000.0 / m.period_avg : 0.0;
    std::sort(periods.begin(), periods.end());
    m.period_p99 = periods[std::min(periods.size() - 1, periods.size() * 99 / 100)];
    {  // the 1 % lows: the mean of the slowest 1 % of the frames, as frames a second
        const size_t worst = std::max<size_t>(1, periods.size() / 100);
        double sw = 0;
        for (size_t i = periods.size() - worst; i < periods.size(); ++i) sw += periods[i];
        m.low1_fps = sw > 0 ? 1000.0 * static_cast<double>(worst) / sw : 0.0;
    }
    m.main_work = std::max(0.0, m.period_avg - m.waits);
    m.game_code = 0;
    for (int i = 0; i < kStageCount; ++i)
        if (kStages[i].game) m.game_code += m.cpu[i];
    // (the game thread's game code is not the main thread's work)
    m.host_work = m.worker_busy[kWorkerGame] > 0.0 ? m.main_work : std::max(0.0, m.main_work - m.game_code);
    if (S().res.gpu_busy_pct >= 0) m.gpu_kernel = S().res.gpu_busy_pct * 0.01 * m.period_avg;
    // the bottleneck: what the frame waits on. The GPU's load: the kernel's account where there is one (the timestamps'
    // can hold the GPU's waits inside a stage: an upload, a sync)
    const double P = m.period_avg;
    const double gpu_load = m.gpu_kernel >= 0 ? m.gpu_kernel : m.gpu_busy;
    const double scene_wait = m.cpu[kSceneWait] + m.cpu[kSceneCaptureWait], chain_wait = m.cpu[kChainJoinWait];
    const double cap = m.cpu[kPacing] + m.cpu[kPresent];
    char t[256];
    if ((m.gpu_frames > 0 || m.gpu_kernel >= 0) && gpu_load >= 0.85 * P) {
        m.bound = Bound::Gpu;
        std::snprintf(t, sizeof t, "GPU-bound: the GPU is busy %.2f of the frame's %.2f ms (%.0f %%)", gpu_load, P, 100.0 * gpu_load / P);
    } else if (scene_wait >= 0.15 * P && scene_wait >= chain_wait) {
        m.bound = Bound::SceneWorker;
        std::snprintf(t, sizeof t, "Bound by the MRP scene worker: the main thread waits %.2f ms a frame for its scene build (%.2f ms busy)", scene_wait,
                      m.worker_busy[kWorkerScene]);
    } else if (chain_wait >= 0.15 * P) {
        m.bound = Bound::ChainWorker;
        std::snprintf(t, sizeof t, "Bound by the chain worker: the main thread waits %.2f ms a frame for it (%.2f ms busy)", chain_wait, m.worker_busy[kWorkerChain]);
    } else if (m.cpu[kGameThreadWait] >= 0.15 * P) {
        m.bound = Bound::GameThread;
        std::snprintf(t, sizeof t, "Bound by the game thread: the main thread waits %.2f ms a frame for the game's update (%.2f ms busy)", m.cpu[kGameThreadWait],
                      m.worker_busy[kWorkerGame]);
    } else if (m.main_work >= 0.85 * P) {
        m.bound = Bound::MainThread;
        std::snprintf(t, sizeof t, "CPU-bound on the main thread: %.2f of %.2f ms is its own work (game code %.2f, host %.2f)", m.main_work, P, m.game_code, m.host_work);
    } else if (cap >= 0.2 * P) {
        m.bound = Bound::Cap;
        std::snprintf(t, sizeof t, "Held by the frame cap / v-sync: %.2f ms a frame waiting (present %.2f, cap %.2f); the work fits in %.2f ms", cap, m.cpu[kPresent],
                      m.cpu[kPacing], P - cap);
    } else {
        std::snprintf(t, sizeof t, "No single bottleneck (main thread work %.2f, GPU %.2f, waits %.2f of %.2f ms)", m.main_work, gpu_load, m.waits, P);
    }
    m.bound_text = t;
    return m;
}

const Resources& resources() { return S().res; }

// ---- The GL objects ----

void gl_track(GlKind kind, unsigned name, const char* owner) {
    if (name == 0) return;
    std::lock_guard<std::mutex> lk(g_gl_mutex);
    g_gl_owner[{static_cast<int>(kind), name}] = owner;
}
void gl_track_n(GlKind kind, int n, const unsigned* names, const char* owner) {
    for (int i = 0; i < n; ++i) gl_track(kind, names[i], owner);
}

void set_gpu_api(const GpuApi* api) { g_api = api; }

void gl_scan(std::vector<GlOwnerRow>* out) {
    out->clear();
    if (g_api != nullptr) {  // (Vulkan: the renderer's own account)
        if (g_api->scan != nullptr) g_api->scan(out);
        return;
    }
    // (GL 4.5's direct-state queries: not in the 3.3 loader, looked up)
    using PfnTexParam = void(APIENTRYP)(GLuint, GLenum, GLint*);
    using PfnTexLevelParam = void(APIENTRYP)(GLuint, GLint, GLenum, GLint*);
    using PfnBufParam64 = void(APIENTRYP)(GLuint, GLenum, GLint64*);
    using PfnRbParam = void(APIENTRYP)(GLuint, GLenum, GLint*);
    static const auto tex_param = reinterpret_cast<PfnTexParam>(glfwGetProcAddress("glGetTextureParameteriv"));
    static const auto tex_level = reinterpret_cast<PfnTexLevelParam>(glfwGetProcAddress("glGetTextureLevelParameteriv"));
    static const auto buf_param = reinterpret_cast<PfnBufParam64>(glfwGetProcAddress("glGetNamedBufferParameteri64v"));
    static const auto rb_param = reinterpret_cast<PfnRbParam>(glfwGetProcAddress("glGetNamedRenderbufferParameteriv"));
    if (tex_param == nullptr || tex_level == nullptr || buf_param == nullptr || rb_param == nullptr) return;
    while (glGetError() != GL_NO_ERROR) {
    }
    std::map<std::string, GlOwnerRow> rows;
    std::lock_guard<std::mutex> lk(g_gl_mutex);
    auto owner_of = [](int kind, GLuint n) -> std::string {
        const auto it = g_gl_owner.find({kind, n});
        return it != g_gl_owner.end() ? it->second : "(untracked)";
    };
    auto bits_of = [&](GLuint t, GLint lvl) {
        GLint b = 0, sum = 0;
        const GLenum keys[] = {GL_TEXTURE_RED_SIZE, GL_TEXTURE_GREEN_SIZE, GL_TEXTURE_BLUE_SIZE, GL_TEXTURE_ALPHA_SIZE, GL_TEXTURE_DEPTH_SIZE, GL_TEXTURE_STENCIL_SIZE, GL_TEXTURE_SHARED_SIZE};
        for (GLenum k : keys) {
            b = 0;
            tex_level(t, lvl, k, &b);
            sum += b;
        }
        return sum;
    };
    // the names are small integers given in order: scanned until a long run of unused ones
    constexpr GLuint kRun = 4096;
    GLuint miss = 0;
    for (GLuint n = 1; miss < kRun && n < (1u << 22); ++n) {
        if (!glIsTexture(n)) {
            ++miss;
            continue;
        }
        miss = 0;
        GLint target = 0;
        tex_param(n, 0x1006 /* GL_TEXTURE_TARGET */, &target);
        if (target == GL_TEXTURE_BUFFER) continue;  // (its buffer counted)
        uint64_t bytes = 0;
        const uint64_t faces = target == GL_TEXTURE_CUBE_MAP ? 6 : 1;
        for (GLint lvl = 0; lvl < 16; ++lvl) {
            GLint w = 0, h = 0, d = 0, samples = 0, compressed = 0;
            tex_level(n, lvl, GL_TEXTURE_WIDTH, &w);
            if (w <= 0) break;
            tex_level(n, lvl, GL_TEXTURE_HEIGHT, &h);
            tex_level(n, lvl, GL_TEXTURE_DEPTH, &d);
            tex_level(n, lvl, GL_TEXTURE_SAMPLES, &samples);
            tex_level(n, lvl, GL_TEXTURE_COMPRESSED, &compressed);
            if (compressed) {
                GLint sz = 0;
                tex_level(n, lvl, GL_TEXTURE_COMPRESSED_IMAGE_SIZE, &sz);
                bytes += static_cast<uint64_t>(sz) * faces;
            } else {
                bytes += static_cast<uint64_t>(w) * static_cast<uint64_t>(std::max(h, 1)) * static_cast<uint64_t>(std::max(d, 1)) * static_cast<uint64_t>(std::max(samples, 1)) *
                         static_cast<uint64_t>(bits_of(n, lvl)) / 8 * faces;
            }
            if (target == GL_TEXTURE_2D_MULTISAMPLE || target == GL_TEXTURE_2D_MULTISAMPLE_ARRAY || target == GL_TEXTURE_RECTANGLE) break;
        }
        GlOwnerRow& r = rows[owner_of(kGlTexture, n)];
        ++r.textures;
        r.tex_bytes += bytes;
    }
    miss = 0;
    for (GLuint n = 1; miss < kRun && n < (1u << 22); ++n) {
        if (!glIsBuffer(n)) {
            ++miss;
            continue;
        }
        miss = 0;
        GLint64 sz = 0;
        buf_param(n, GL_BUFFER_SIZE, &sz);
        GlOwnerRow& r = rows[owner_of(kGlBuffer, n)];
        ++r.buffers;
        r.buf_bytes += static_cast<uint64_t>(std::max<GLint64>(sz, 0));
    }
    miss = 0;
    for (GLuint n = 1; miss < 1024 && n < (1u << 20); ++n) {
        if (!glIsRenderbuffer(n)) {
            ++miss;
            continue;
        }
        miss = 0;
        GLint w = 0, h = 0, samples = 0, sum = 0, b = 0;
        rb_param(n, GL_RENDERBUFFER_WIDTH, &w);
        rb_param(n, GL_RENDERBUFFER_HEIGHT, &h);
        rb_param(n, GL_RENDERBUFFER_SAMPLES, &samples);
        const GLenum keys[] = {GL_RENDERBUFFER_RED_SIZE, GL_RENDERBUFFER_GREEN_SIZE, GL_RENDERBUFFER_BLUE_SIZE, GL_RENDERBUFFER_ALPHA_SIZE, GL_RENDERBUFFER_DEPTH_SIZE, GL_RENDERBUFFER_STENCIL_SIZE};
        for (GLenum k : keys) {
            b = 0;
            rb_param(n, k, &b);
            sum += b;
        }
        GlOwnerRow& r = rows[owner_of(kGlRenderbuffer, n)];
        ++r.renderbuffers;
        r.rb_bytes += static_cast<uint64_t>(w) * static_cast<uint64_t>(h) * static_cast<uint64_t>(std::max(samples, 1)) * static_cast<uint64_t>(sum) / 8;
    }
    while (glGetError() != GL_NO_ERROR) {
    }
    for (auto& [name, r] : rows) {
        r.owner = name;
        out->push_back(r);
    }
    std::sort(out->begin(), out->end(), [](const GlOwnerRow& a, const GlOwnerRow& b) { return a.tex_bytes + a.buf_bytes + a.rb_bytes > b.tex_bytes + b.buf_bytes + b.rb_bytes; });
}

// ---- Exports ----

bool export_csv(const char* path, int frames) {
    std::FILE* f = std::fopen(path, "w");
    if (f == nullptr) return false;
    csv_header(f);
    const int n = std::min(frames, history_size());
    for (int k = history_size() - n; k < history_size(); ++k) csv_row(f, history_at(k));
    std::fclose(f);
    return true;
}

bool trace_start(const char* path, int frames) {
    State& s = S();
    if (s.tracing || path == nullptr || *path == 0) return false;
    s.tracing = true;
    s.trace_path = path;
    s.trace_from = s.frame_index + 1;
    s.trace_to = s.trace_from + static_cast<uint64_t>(std::max(1, frames));
    s.trace.clear();
    s.trace.reserve(static_cast<size_t>(frames) * 64);
    s.trace_msg = "recording " + std::to_string(frames) + " frames to " + s.trace_path;
    return true;
}
bool trace_active() { return S().tracing; }
std::string trace_status() { return S().trace_msg; }

namespace {
double mb(uint64_t b) { return static_cast<double>(b) / (1024.0 * 1024.0); }
}  // namespace

void dump(std::FILE* f, int frames, bool with_gl_scan) {
    State& s = S();
    const Summary m = summarize(frames);
    if (m.frames == 0) return;
    std::fprintf(f, "[profiler] host frames %llu..%llu (%d): %.1f fps, frame %.2f ms (min %.2f, max %.2f, p99 %.2f; 1%% low %.1f fps), %.2f game frames each\n",
                 static_cast<unsigned long long>(history_at(history_size() - m.frames).index), static_cast<unsigned long long>(history_at(history_size() - 1).index), m.frames, m.fps,
                 m.period_avg, m.period_min, m.period_max, m.period_p99, m.low1_fps, m.steps);
    std::fprintf(f, "[profiler]   main thread: work %.2f ms (game code %.2f, host %.2f), waits %.2f, its CPU time %.2f; GPU busy %.2f ms (span %.2f, %d frames read back; the kernel's account %.2f)\n",
                 m.main_work, m.game_code, m.host_work, m.waits, m.main_cpu, m.gpu_busy, m.gpu_span, m.gpu_frames, m.gpu_kernel);
    std::fprintf(f, "[profiler]   %s\n", m.bound_text.c_str());
    std::fprintf(f, "[profiler]   GPU timestamps: %llu frames read back (%.1f frames after), %llu dropped (not back in %d frames)\n", static_cast<unsigned long long>(s.gpu_read),
                 s.gpu_read != 0 ? static_cast<double>(s.gpu_age_sum) / static_cast<double>(s.gpu_read) : 0.0, static_cast<unsigned long long>(s.gpu_dropped), kSlots);
    std::fprintf(f, "[profiler]   %-38s %8s %8s %8s %8s\n", "stage", "cpu avg", "cpu max", "gpu avg", "gpu max");
    for (int i = 0; i < kStageCount; ++i) {
        if (m.cpu_max[i] < 0.005 && m.gpu_max[i] < 0.005) continue;
        if (kStages[i].gpu)
            std::fprintf(f, "[profiler]   %-38s %8.3f %8.3f %8.3f %8.3f\n", kStages[i].name, m.cpu[i], m.cpu_max[i], m.gpu[i], m.gpu_max[i]);
        else
            std::fprintf(f, "[profiler]   %-38s %8.3f %8.3f %8s %8s\n", kStages[i].name, m.cpu[i], m.cpu_max[i], "-", "-");
    }
    std::fprintf(f, "[profiler]   %-38s %8.3f %8s %8.3f\n", "(no stage)", m.cpu_other, "", m.gpu_other);
    for (int w = 0; w < kWorkerCount; ++w)
        std::fprintf(f, "[profiler]   worker %-29s busy %.3f ms/frame, CPU %.3f ms/frame\n", kWorkerNames[w], m.worker_busy[w], m.worker_cpu[w]);
    resources_sample(s, true);
    const Resources& R = s.res;
    std::fprintf(f, "[profiler]   process CPU %.0f %% of a core (%d logical CPUs);", R.process_cpu_pct, R.cpus);
    int shown = 0;
    for (const ThreadRow& t : R.threads) {
        if (t.cpu_pct < 1.0 || shown >= 10) continue;
        std::fprintf(f, " %s %.0f%%%s", t.name.c_str(), t.cpu_pct, shown + 1 < 10 ? "," : "");
        ++shown;
    }
    std::fprintf(f, "\n");
    std::fprintf(f, "[profiler]   memory: resident %.0f MB (peak %.0f), address space %.0f MB; EE RAM 32 MB + scratchpad 16 KB (fixed)\n", mb(R.rss), mb(R.rss_peak), mb(R.virt));
    std::fprintf(f, "[profiler]   GPU %s (%s, %s)", R.gl_renderer.c_str(), R.gl_vendor.c_str(), R.api.c_str());
    if (R.vk_budget)
        std::fprintf(f, "; Vulkan device-local heaps %.0f MB, this process uses %.0f MB of a %.0f MB budget", mb(R.vk_heap_size), mb(R.vk_heap_usage), mb(R.vk_heap_budget));
    if (R.drm)
        std::fprintf(f, "; %s: this process %.0f MB VRAM + %.0f MB GTT; the device's VRAM %.0f / %.0f MB, GTT %.0f / %.0f MB; GPU busy %.0f %%", R.drm_driver.c_str(), mb(R.drm_vram), mb(R.drm_gtt),
                     mb(R.dev_vram_used), mb(R.dev_vram_total), mb(R.dev_gtt_used), mb(R.dev_gtt_total), R.gpu_busy_pct);
    if (R.nvx) std::fprintf(f, "; NVX: dedicated %.0f MB, available %.0f MB, evicted %.0f MB", mb(R.nvx_dedicated), mb(R.nvx_available), mb(R.nvx_evicted));
    if (R.ati) std::fprintf(f, "; ATI meminfo free: textures %.0f MB, buffers %.0f MB", mb(R.ati_tex_free), mb(R.ati_vbo_free));
    std::fprintf(f, "\n");
    if (with_gl_scan) {
        std::vector<GlOwnerRow> rows;
        gl_scan(&rows);
        uint64_t tot = 0;
        for (const GlOwnerRow& r : rows) tot += r.tex_bytes + r.buf_bytes + r.rb_bytes;
        std::fprintf(f, "[profiler]   %s: %.1f MB in all;", g_api != nullptr ? "Vulkan allocations" : "GL objects", mb(tot));
        for (const GlOwnerRow& r : rows)
            std::fprintf(f, " %s %.1f MB (%d tex %.1f, %d buf %.1f, %d rb %.1f);", r.owner.c_str(), mb(r.tex_bytes + r.buf_bytes + r.rb_bytes), r.textures, mb(r.tex_bytes), r.buffers,
                         mb(r.buf_bytes), r.renderbuffers, mb(r.rb_bytes));
        std::fprintf(f, "\n");
    }
    std::fflush(f);
}

void shutdown() {
    State& s = S();
    if (s.tracing && !s.trace.empty()) trace_write(s);
    if (s.csv != nullptr) {
        std::fclose(s.csv);
        s.csv = nullptr;
    }
}

}  // namespace rac1::prof
