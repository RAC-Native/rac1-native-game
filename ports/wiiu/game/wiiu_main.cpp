// The Wii U's entry: the game's host (src/host/orig_main.cpp, its main renamed rac1_orig_main) run with the settings of
// rac1-native/run.cfg on the SD card (KEY=VALUE lines, the RAC1_* variables of docs/env-vars.md; RAC1_HEADLESS and
// RAC1_NO_GRAPHICS always on), its output in rac1-native/run.log (stderr in run.err.log), the game's data beside it (rac1-native/assets/: the
// disc image, the boot executable and the level maps the setup made on the PC from the player's own disc). Then back
// to the Wii U Menu.
#include <coreinit/debug.h>
#include <coreinit/foreground.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <proc_ui/procui.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

#include <game/render/render_view.h>
#include <runtime/ee/game_thread.h>

#include <exception>
#include <typeinfo>

int rac1_orig_main(int argc, char** argv);
void wiiu_sampler_start();  // wiiu_sampler.cpp
void wiiu_sampler_stop();
bool wiiu_gfx_wanted();  // wiiu_gfx.cpp: the picture (RAC1_WIIU_GFX=1)
bool wiiu_gfx_start();
void wiiu_gfx_stop();
void wiiu_gfx_frame();
void wiiu_gfx_game_ended();
void wiiu_gfx_foreground(bool on);
void rac1_wiiu_debug_tools_gate(int argc, char** argv);  // (src/host/orig_main.cpp)

// ---- What goes wrong before main or outside it, on the UDP log (port 4405) ----
// The return addresses up the stack (the PowerPC back chain: r1 points at the caller's frame, its LR saved at +4), with
// this function's own address beside them: the loader moves the code, `addr - here + here's ELF address` finds it in
// the ELF.
// (OSReport does not reach the UDP log once the game runs: the lines go to WHBLogPrintf too, which has no handler yet
// before main)
static void log_backtrace(const char* why) {
    OSReport("rac1-wiiu: %s; log_backtrace at %p\n", why, reinterpret_cast<void*>(&log_backtrace));
    WHBLogPrintf("rac1-wiiu: %s; log_backtrace at %p", why, reinterpret_cast<void*>(&log_backtrace));
    uintptr_t* fp = static_cast<uintptr_t*>(__builtin_frame_address(0));
    for (int i = 0; i < 24 && fp != nullptr && (reinterpret_cast<uintptr_t>(fp) & 3) == 0; ++i) {
        const uintptr_t lr = fp[1];
        OSReport("rac1-wiiu:   #%d %08x\n", i, static_cast<unsigned>(lr));
        WHBLogPrintf("rac1-wiiu:   #%d %08x", i, static_cast<unsigned>(lr));
        fp = reinterpret_cast<uintptr_t*>(fp[0]);
    }
}
// After a failure we can catch (an exception, an abort of our own code): back to the Wii U Menu instead of a stopped
// console. (A crash of the processor itself, a DSI or an OS error, still shows Aroma's screen: power-cycle then.)
static bool g_proc_up = false;  // (WHBProcInit done: the menu reachable)
// ProcUI over: its exit seen (HOME's "close software"). WHBProcIsRunning then shuts ProcUI down (ProcUIShutdown) - and on
// every later call processes its messages again, after that shutdown: ProcUIProcessMessages(TRUE) waits for a message that
// never comes. The console hung there on its way back to the menu (2026-10-28: the log's last line "the game's main
// returned", the next one never written). So no WHBProcIsRunning once ProcUI is over: proc_running.
static volatile bool g_proc_exit = false;
static uint32_t on_proc_exit(void*) {
    g_proc_exit = true;
    return 0;
}
// the picture's foreground (ProcUI's release / acquire: the HOME menu, the exit): no frame drawn without it (wiiu_gfx_frame)
static uint32_t on_proc_release(void*) {
    wiiu_gfx_foreground(false);
    return 0;
}
static uint32_t on_proc_acquire(void*) {
    wiiu_gfx_foreground(true);
    return 0;
}
static bool proc_running() { return g_proc_up && !g_proc_exit && ProcUIIsRunning() && WHBProcIsRunning(); }
[[noreturn]] static void back_to_menu(const char* why) {
    OSReport("rac1-wiiu: %s: back to the menu\n", why);
    std::fflush(stdout);
    static bool once = false;
    if (!once && g_proc_up) {
        once = true;
        if (proc_running()) {
            SYSLaunchMenu();
            while (proc_running()) OSSleepTicks(OSMillisecondsToTicks(16));
        }
        WHBProcShutdown();
    }
    std::_Exit(1);
}
static void on_terminate() {
    if (std::exception_ptr e = std::current_exception()) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            OSReport("rac1-wiiu: uncaught exception %s: %s\n", typeid(x).name(), x.what());
        } catch (...) {
            OSReport("rac1-wiiu: uncaught exception (not a std::exception)\n");
        }
    }
    log_backtrace("terminate");
    back_to_menu("terminate");
}
extern "C" void __real_abort();
extern "C" void __wrap_abort() {  // (-Wl,--wrap=abort)
    log_backtrace("abort");
    back_to_menu("abort");
}
// Every C++ throw, with where it came from, on the log (-Wl,--wrap=__cxa_throw): the game's errors are exceptions, and
// most of the Wii U's are a field read in the wrong byte order somewhere
extern "C" [[noreturn]] void __real___cxa_throw(void*, void*, void (*)(void*));
extern "C" [[noreturn]] void __wrap___cxa_throw(void* obj, void* tinfo, void (*dest)(void*)) {
    log_backtrace("throw");
    __real___cxa_throw(obj, tinfo, dest);
}

// (before every other static constructor: the terminate handler. The static constructors run before wut sets up newlib,
// the files and the sockets, so these first lines go through OSReport, the system's log: Aroma's LoggingModule sends it
// on UDP 4405 with its own socket)
__attribute__((constructor(101))) static void early_init() {
    OSReport("rac1-wiiu: static initialisation\n");
    std::set_terminate(on_terminate);
}
__attribute__((constructor(65535))) static void late_init() { OSReport("rac1-wiiu: static initialisation done\n"); }

// stdout / stderr: a newlib device that writes each line to the system console (OSConsoleWrite: Cemu's log.txt) and to
// the UDP log (WHBLogWrite, port 4405: the console's own); wut's default stdout reaches neither
#include <sys/iosupport.h>
#include <mutex>
namespace {
// ((2026-10-28) one line buffer for stdout and stderr, whose FILE locks are two: the writers serialised here - the game thread's
// lines, the workers', the main thread's at once)
std::mutex g_line_m;
char g_line[1024];
size_t g_line_n = 0;
void line_out() {
    g_line[g_line_n] = 0;
    OSConsoleWrite(g_line, static_cast<uint32_t>(g_line_n));
    WHBLogWrite(g_line);
    g_line_n = 0;
}
ssize_t console_write(struct _reent*, void*, const char* p, size_t n) {
    std::lock_guard<std::mutex> lk(g_line_m);
    for (size_t i = 0; i < n; ++i) {
        g_line[g_line_n++] = p[i];
        if (p[i] == '\n' || g_line_n == sizeof g_line - 1) line_out();
    }
    return static_cast<ssize_t>(n);
}
devoptab_t g_console_dot = {};
void console_stdio() {
    g_console_dot.name = "console";
    g_console_dot.write_r = &console_write;
    devoptab_list[STD_OUT] = &g_console_dot;
    devoptab_list[STD_ERR] = &g_console_dot;
}
}  // namespace

// (RAC1_WIIU_WATCHDOG=<seconds>) a thread on core 2 that every so often stops the game's thread for an instant and logs
// where it is: its pc, lr and the return addresses up its stack (copied while it is stopped, printed after it runs again,
// so a lock it holds cannot stop the watchdog too). For a game that hangs without a crash.
namespace {
OSThread* g_game_thread = nullptr;
OSThread g_watchdog;
alignas(16) unsigned char g_watchdog_stack[16 * 1024];
int watchdog_main(int seconds, const char**) {
    for (int tick = 1;; ++tick) {
        OSSleepTicks(OSSecondsToTicks(seconds));
        uint32_t pc = 0, lr = 0, sp = 0, chain[16] = {};
        OSSuspendThread(g_game_thread);
        while (!OSIsThreadSuspended(g_game_thread)) OSSleepTicks(OSMillisecondsToTicks(1));
        pc = g_game_thread->context.srr0;
        lr = g_game_thread->context.lr;
        sp = g_game_thread->context.gpr[1];
        uintptr_t fp = sp;
        for (int i = 0; i < 16 && fp != 0 && (fp & 3) == 0 && fp >= 0x01000000u; ++i) {
            const uint32_t* f = reinterpret_cast<const uint32_t*>(fp);
            chain[i] = f[1];
            fp = f[0];
        }
        OSResumeThread(g_game_thread);
        // (the UDP log too: OSReport alone did not reach it)
        OSReport("rac1-wiiu: watchdog %d s: pc %08x lr %08x sp %08x\n", tick * seconds, pc, lr, sp);
        WHBLogPrintf("rac1-wiiu: watchdog %d s: pc %08x lr %08x sp %08x", tick * seconds, pc, lr, sp);
        for (int i = 0; i < 16 && chain[i] != 0; ++i) {
            OSReport("rac1-wiiu:   ^%d %08x\n", i, chain[i]);
            WHBLogPrintf("rac1-wiiu:   ^%d %08x", i, chain[i]);
        }
    }
    return 0;
}
void watchdog_start(int seconds) {
    g_game_thread = OSGetCurrentThread();
    if (!OSCreateThread(&g_watchdog, watchdog_main, seconds, nullptr, g_watchdog_stack + sizeof g_watchdog_stack,
                        sizeof g_watchdog_stack, 10, OS_THREAD_ATTRIB_AFFINITY_CPU2))
        return;
    OSSetThreadName(&g_watchdog, "rac1 watchdog");
    OSResumeThread(&g_watchdog);
}
}  // namespace

namespace {
constexpr const char* kDir = "fs:/vol/external01/rac1-native";

bool g_console = false;  // (the log's screen: not with the picture, RAC1_WIIU_GFX)
void say(const char* s) {
    OSReport("rac1-wiiu: %s\n", s);
    WHBLogPrintf("rac1-wiiu: %s", s);
    if (g_console) WHBLogConsoleDraw();
}

// (RAC1_WIIU_ISO_CHECK=1) the disc image's blocks of 1 MB at these offsets hashed (FNV-1a), to compare with the PC's
// copy: the SD card's copy read back as the game reads it (FTP cannot read past 2 GB here)
void iso_check() {
    static const unsigned long long offs[] = {0ull, 1073741824ull, 2911633408ull, 2912157696ull, 3500000000ull, 4213047296ull};
    std::FILE* f = std::fopen(std::getenv("RAC1_ISO") != nullptr ? std::getenv("RAC1_ISO") : "assets/iso/rac1-ntsc-u.iso", "rb");
    if (f == nullptr) {
        std::printf("[wiiu] iso check: no assets/iso/rac1-ntsc-u.iso\n");
        return;
    }
    static unsigned char buf[1 << 20];
    for (unsigned long long off : offs) {
        uint32_t h = 2166136261u;
        size_t n = 0;
        if (fseeko(f, static_cast<off_t>(off), SEEK_SET) == 0) n = std::fread(buf, 1, sizeof buf, f);
        for (size_t i = 0; i < n; ++i) {
            h ^= buf[i];
            h *= 16777619u;
        }
        std::printf("[wiiu] iso check @%llu: %zu bytes, fnv %08x\n", off, n, h);
    }
    std::fclose(f);
}

// Patchable 1 KB config slot for bench.sh (so benchmark runs never need FTP or touch Daino's SD run.cfg).
// Empty in normal builds ("RAC1_EMBEDDED_CFG:\0").
alignas(16) volatile char g_embedded_cfg[1024] = "RAC1_EMBEDDED_CFG:";

bool embedded_has_no_cfg() {
    const char* p = const_cast<const char*>(g_embedded_cfg) + 18;
    return std::strncmp(p, "--no-cfg\n", 9) == 0;
}

int embedded_settings() {
    const char* p = const_cast<const char*>(g_embedded_cfg) + 18;
    if (*p == '\0') return 0;
    int n = 0;
    while (*p != '\0') {
        const char* nl = std::strchr(p, '\n');
        const size_t len = nl != nullptr ? static_cast<size_t>(nl - p) : std::strlen(p);
        std::string l(p, len);
        while (!l.empty() && (l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (!l.empty() && l[0] != '#' && l != "--no-cfg") {
            const size_t eq = l.find('=');
            if (eq != std::string::npos) {
                setenv(l.substr(0, eq).c_str(), l.substr(eq + 1).c_str(), 1);
                ++n;
            }
        }
        if (nl == nullptr) break;
        p = nl + 1;
    }
    return n;
}

int read_settings(bool read_file = true) {
    setenv("RAC1_HEADLESS", "1", 1);
    setenv("RAC1_AUDIO_DEVICE", "ax", 1); // (a headless run plays its sound only with a device named; RAC1_NO_AUDIO=1 turns it off)
    setenv("RAC1_READ_ISO", "0", 1);      // (assets/disc's extracted files on the SD card, as the PC reads them; run.cfg may say 1: the image)
    // the MRP-lite's settings (MRP_LITE.md; run.cfg may say otherwise): the MRP's mobys with the GS path's work they make needless
    // skipped (RAC1_NO_GRAPHICS=2), the ties / shrubs' culling and relighting off and the game's tfrag pass not run (their drawing is
    // the host's: RAC1_WORLD_PASS_SKIP=6, RAC1_TFRAG_PASS_OFF=1), the moby pass's chain parts and texture uploads not built (the MRP
    // gets the parts from the draw loop: RAC1_MOBY_PASS_MRP=1, moby_pass.h)
    setenv("RAC1_NO_GRAPHICS", "2", 1);
    setenv("RAC1_MOBY_MRP", "1", 1);
    setenv("RAC1_WORLD_PASS_SKIP", "6", 1);
    setenv("RAC1_TFRAG_PASS_OFF", "1", 1);
    setenv("RAC1_MOBY_PASS_MRP", "1", 1);
    setenv("RAC1_SKIP_SHADOWS", "0", 1);     // (drop shadows and draw lists enabled on Wii U: rendered via mrp_world_gx2_draw_fx)
    setenv("RAC1_SKIP_DRAW_LISTS", "0", 1);  // (run.cfg or the debug menu's Game page can still override them)
    setenv("RAC1_DRAW_LAST_STEP", "1", 1);  // (a catch-up game frame, not shown, not drawn: game_skips.h)
    setenv("RAC1_IOP_STATE_ONLY", "1", 1);  // (the mirror plays the sound: the game's IOP keeps its state, unmixed - about 10% of the game thread, 2026-10-28)
    setenv("RAC1_WIIU_GFX", "1", 1);
    setenv("RAC1_WIIU_3D_SCALE", "1.0", 1);  // (the 3D at the TV's native resolution: Daino saw no frame-rate difference against 0.75 on Veldin / Novalis, 2026-10-30 - the CPU holds the frame; run.cfg or the debug menu can lower it)
    setenv("RAC1_WIIU_PREP_SPLIT", "1", 1);  // (the world job's second half on core 0 once the chain reader is done - it ends at ~10 ms of the cycle since 622ff0e: Novalis, the main thread's world join 11.4 -> 6.5 ms, 2026-10-30)
    setenv("RAC1_WIIU_FRAMES_IN_FLIGHT", "2", 1);  // (the GPU draws frame N while the main thread makes N + 1: wiiu_gfx.cpp - Veldin open host 32.2 -> 29.5 ms, Novalis 31.4 -> 30.3, the GPU's wait 11 -> 2, 2026-11-01; =1 as before)
    setenv("RAC1_REALTIME", "1", 1);
    setenv("RAC1_WIIU_MENU", "1", 1);
    setenv("RAC1_BOOT", "1", 1);
    setenv("RAC1_MEMCARD", "1", 1);
    setenv("RAC1_MEMCARD_DIR", "memcard", 1);
    setenv("RAC1_QUIT_ENDS", "0", 1);
    setenv("RAC1_ADVANCED", "1", 1);
    setenv("RAC1_FRAMES", "0", 1);  // (no frame limit: the headless default is 300 frames - the game ended after the intro when run.cfg was empty)
    if (!read_file) return 0;
    std::FILE* f = std::fopen("run.cfg", "r");
    if (f == nullptr) return 0;
    int n = 0;
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        const size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        setenv(l.substr(0, eq).c_str(), l.substr(eq + 1).c_str(), 1);
        ++n;
    }
    std::fclose(f);
    return n;
}
}  // namespace

// the launch's arguments (wiiload's, after the program): RAC1_*=value settings over run.cfg's, `--no-cfg` run.cfg left out - a run
// without FTP (the console's FTP plugin hangs now and then). Returns the settings taken. (2026-10-28: Aroma's wiiload does not pass
// them on - argc 1 -: kept for a loader that does)
int argv_settings(int argc, char** argv) {
    int n = 0;
    for (int i = 1; i < argc; ++i) {
        const char* a = argv[i];
        const char* eq = a != nullptr ? std::strchr(a, '=') : nullptr;
        if (eq == nullptr || std::strncmp(a, "RAC1_", 5) != 0) continue;
        setenv(std::string(a, static_cast<size_t>(eq - a)).c_str(), eq + 1, 1);
        ++n;
    }
    return n;
}
bool argv_has(int argc, char** argv, const char* flag) {
    for (int i = 1; i < argc; ++i)
        if (argv[i] != nullptr && std::strcmp(argv[i], flag) == 0) return true;
    return false;
}

int main(int argc, char** argv_in) {
    OSReport("rac1-wiiu: main\n");
    WHBProcInit();
    g_proc_up = true;
    ProcUIRegisterCallback(PROCUI_CALLBACK_EXIT, &on_proc_exit, nullptr, 100);
    OSReport("rac1-wiiu: ProcUI up\n");
    WHBLogUdpInit();
    if (chdir(kDir) != 0) OSReport("rac1-wiiu: cannot enter fs:/vol/external01/rac1-native\n");
    const bool no_cfg = argv_has(argc, argv_in, "--no-cfg") || embedded_has_no_cfg();
    const int settings = read_settings(!no_cfg);
    const int from_args = embedded_settings() + argv_settings(argc, argv_in);
    {  // ((2026-10-28) the widescreen: RAC1_WIIU_WIDE=0 the game's 4:3 frame, 1 (the default) 16:9 - the 3D drawn and culled 4/3 wider,
       // render_view.h g_view_draw_widen -; RAC1_WIIU_WIDE_MOBYS=0 the mobys' culling kept 4:3 (the moby pass's frustum not widened); the
       // debug menu's Game page changes both)
        const char* wide = std::getenv("RAC1_WIIU_WIDE");
        rac1::g_view_draw_widen = wide == nullptr || std::atoi(wide) != 0 ? 4.0f / 3.0f : 1.0f;
        const char* wm = std::getenv("RAC1_WIIU_WIDE_MOBYS");
        rac1::g_view_draw_widen_mobys = wm == nullptr || std::atoi(wm) != 0;
    }
    const bool gfx = wiiu_gfx_wanted();
    if (!gfx) {
        WHBLogConsoleInit();
        g_console = true;
    }
    say("start");
    char msg[160];
    std::snprintf(msg, sizeof msg, "%d settings from run.cfg, %d from the arguments (%d of them); the game's output goes to the system console", settings,
                  from_args, argc);
    say(msg);

    // the game's output: the system console and the UDP log, or with RAC1_WIIU_LOG_FILE=1 run.log / run.err.log
    console_stdio();
    if (const char* lf = std::getenv("RAC1_WIIU_LOG_FILE"); lf != nullptr && lf[0] == '1') {
        std::freopen("run.log", "w", stdout);
        std::freopen("run.err.log", "w", stderr);  // (not the same file twice: the filesystem keeps one writer)
    }
    std::setvbuf(stdout, nullptr, _IOLBF, 1024);  // (a line at a time: the log is there whatever happens next)
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    char arg0[] = "rac1-native";
    char* argv[] = {arg0, nullptr};
    if (const char* c = std::getenv("RAC1_WIIU_ISO_CHECK"); c != nullptr && c[0] == '1') iso_check();
    if (const char* w = std::getenv("RAC1_WIIU_WATCHDOG"); w != nullptr && std::atoi(w) > 0) watchdog_start(std::atoi(w));
    rac1_wiiu_debug_tools_gate(1, argv);
    say("the game's main");
    const OSTime t0 = OSGetTime();
    int rc = -1;
    const bool gfx_up = gfx && wiiu_gfx_start();  // (the picture: GX2 on the TV and the GamePad)
    if (gfx_up) {
        ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, &on_proc_release, nullptr, 100);
        ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, &on_proc_acquire, nullptr, 100);
    }
    wiiu_sampler_start();  // (RAC1_WIIU_SAMPLE)
    try {
        rc = rac1_orig_main(1, argv);
    } catch (const std::exception& x) {
        std::printf("[wiiu] exception %s: %s\n", typeid(x).name(), x.what());
        std::fflush(stdout);
        WHBLogPrintf("rac1-wiiu: exception %s: %s", typeid(x).name(), x.what());
        wiiu_sampler_stop();  // (the samples kept up to the failure)
        back_to_menu("exception");
    }
    wiiu_sampler_stop();
    rac1::game_thread::stop();  // ((2026-10-28) the game thread ended before the way back: no job left running, its OS thread joined)
    const unsigned ms = static_cast<unsigned>(OSTicksToMilliseconds(OSGetTime() - t0));
    std::printf("[wiiu] the game's main returned %d after %u ms\n", rc, ms);
    std::fflush(stdout);
    std::snprintf(msg, sizeof msg, "the game's main returned %d after %u ms", rc, ms);
    say(msg);

    // back to the menu (ProcUI's messages handled meanwhile; the screen given back on its release)
    // (with the picture, GX2 stays up until ProcUI lets go: the WHB helpers' foreground callbacks still use it while the
    // menu takes over; shut down before, the console hung on its way back)
    // (SYSLaunchMenu only when the game ended by itself: after HOME's "close", ProcUI is already shut down - by the
    // WHBProcIsRunning that saw its exit - and neither SYSLaunchMenu nor WHBProcIsRunning may be called again: proc_running)
    const bool still_running = proc_running();
    say(still_running ? "ending: the game ended by itself (ProcUI running)" : "ending: closed from HOME (ProcUI over)");
    if (gfx_up) wiiu_gfx_game_ended();
    if (still_running) {
        say("ending: the menu asked for");
        SYSLaunchMenu();
    }
    while (proc_running()) {
        if (gfx_up) wiiu_gfx_frame();
        else if (g_console) WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    say("ending: released");
    if (gfx_up) wiiu_gfx_stop();
    say("exit");
    if (g_console) WHBLogConsoleFree();
    WHBLogUdpDeinit();
    WHBProcShutdown();
    return 0;
}
