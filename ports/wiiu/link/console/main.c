// rac1-link (prototype): the Wii U side. A text UI (WHB's log console, TV + GamePad) around the receiver in ../core:
// it shows the IP and the pairing code, answers the PC's discovery broadcast, receives files into
// sd:/rac1-native/ and sd:/wiiu/apps/rac1-native/, and runs the transfer benchmarks rac1_link.py asks for.
// Everything shown is also broadcast on UDP 4405 (WHBLogUdp), so a PC can log a run without looking at the TV.
//
// HOME works (ProcUI): on release the session is closed cleanly (the .part is kept for resume) and the listener
// closed; on return it listens again. The PC's `quit` command, or HOME then "close", goes back to the Wii U Menu.
//
// rac1-native/.link/link.cfg (optional, written by `rac1_link.py config`):
//   somemopt=1     lend nsysnet a 3 MiB socket buffer (somemopt, + SO_RUSRBUF), next start; experimental
//   somemopt_big=1 SOMEMOPT_FLAGS_BIG_BUFFERS (50/50 big/small buffers instead of 80/20)
//   affinity=1     net thread on core 2, writer on core 0 (default: wut's default affinity)
//   rcvbuf=N winscale=0|1 sack=0|1 noslowstart=0|1   socket options of the listener
#include "../core/link_hash.h"
#include "../core/link_proto.h"
#include "../core/link_server.h"

#include <coreinit/energysaver.h>
#include <coreinit/mcp.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <malloc.h>
#include <nn/ac/ac_c.h>
#include <nn/nets2/somemopt.h>
#include <nsysnet/_socket.h>
#include <proc_ui/procui.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <sysapp/launch.h>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#define SD_ROOT "fs:/vol/external01"
#define VERSION "0.1"
#define SOMEMOPT_SIZE (3u << 20)

// --- platform hooks for the core ---

uint64_t lp_now_us(void)
{
    uint64_t t = (uint64_t)OSGetSystemTime(), c = (uint64_t)OSTimerClockSpeed;
    return t / c * 1000000ull + t % c * 1000000ull / c;
}

void lp_sleep_ms(unsigned ms) { OSSleepTicks(OSMillisecondsToTicks(ms)); }

// Not a CSPRNG: the time base and the tick counter, mixed by SHA-256 with a counter. Good enough for nonces and a
// pairing code on a LAN (see README, known limits).
void lp_random(void *buf, size_t n)
{
    static uint32_t counter;
    static pthread_mutex_t m = PTHREAD_MUTEX_INITIALIZER;
    uint8_t *p = (uint8_t *)buf;
    pthread_mutex_lock(&m);
    while (n) {
        struct {
            uint64_t t, st;
            uint32_t tick, c;
        } seed = {(uint64_t)OSGetTime(), (uint64_t)OSGetSystemTime(), (uint32_t)OSGetTick(), ++counter};
        uint8_t h[32];
        link_sha256 s;
        link_sha256_init(&s);
        link_sha256_update(&s, &seed, sizeof seed);
        link_sha256_final(&s, h);
        size_t k = n < 32 ? n : 32;
        memcpy(p, h, k);
        p += k;
        n -= k;
    }
    pthread_mutex_unlock(&m);
}

// Threads log through a queue; the main thread prints (WHB's console is not thread-safe).
#define LOGQ 64
static char logq[LOGQ][160];
static int logq_head, logq_count;
static pthread_mutex_t logq_m = PTHREAD_MUTEX_INITIALIZER;

void lp_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pthread_mutex_lock(&logq_m);
    if (logq_count == LOGQ) {  // full: drop the oldest
        logq_head = (logq_head + 1) % LOGQ;
        logq_count--;
    }
    vsnprintf(logq[(logq_head + logq_count) % LOGQ], sizeof logq[0], fmt, ap);
    logq_count++;
    pthread_mutex_unlock(&logq_m);
    va_end(ap);
}

static void log_drain(void)
{
    char line[160];
    for (;;) {
        pthread_mutex_lock(&logq_m);
        if (!logq_count) {
            pthread_mutex_unlock(&logq_m);
            return;
        }
        memcpy(line, logq[logq_head], sizeof line);
        logq_head = (logq_head + 1) % LOGQ;
        logq_count--;
        pthread_mutex_unlock(&logq_m);
        WHBLogPrint(line);
    }
}

static int g_affinity;

void lp_thread_role(int role)
{
    if (!g_affinity)
        return;
    uint32_t a = role == LP_ROLE_WRITER ? OS_THREAD_ATTRIB_AFFINITY_CPU0 : OS_THREAD_ATTRIB_AFFINITY_CPU2;
    OSSetThreadAffinity(OSGetCurrentThread(), a);
}

void lp_device_id(char *out, size_t n)
{
    static MCPSysProdSettings sps __attribute__((aligned(0x40)));
    snprintf(out, n, "unknown");
    int32_t h = MCP_Open();
    if (h < 0)
        return;
    if (MCP_GetSysProdSettings(h, &sps) == 0)
        snprintf(out, n, "%.8s%.12s", sps.code_id, sps.serial_id);
    MCP_Close(h);
}

// --- somemopt (optional) ---

static pthread_t so_thread;
static int so_started;
static void *so_buf;
static SOMemOptFlags so_flags;

static void *so_main(void *arg)
{
    (void)arg;
    somemopt(SOMEMOPT_REQUEST_INIT, so_buf, SOMEMOPT_SIZE, so_flags);  // returns when nsysnet shuts down
    return NULL;
}

// --- ProcUI ---

static link_server *g_server;

static uint32_t on_release(void *arg)
{
    (void)arg;
    if (g_server)
        link_server_pause(g_server, 1);
    return 0;
}

static uint32_t on_acquire(void *arg)
{
    (void)arg;
    if (g_server)
        link_server_pause(g_server, 0);
    return 0;
}

static char g_ip[20] = "?";

static void banner(link_server *s)
{
    WHBLogPrintf("rac1-link %s (prototype): receives files from rac1_link.py", VERSION);
    WHBLogPrintf("IP %s   TCP %d   discovery UDP %d   log UDP 4405", g_ip, LINK_TCP_PORT, LINK_UDP_PORT);
    WHBLogPrintf("Pairing code: %s", link_server_code(s));
    WHBLogPrintf("Writes only to sd:/rac1-native/ and sd:/wiiu/apps/rac1-native/");
    WHBLogPrintf("HOME: exit   A: show this again");
}

int main(int argc, char **argv)
{
    (void)argc, (void)argv;
    WHBProcInit();
    WHBLogModuleInit();
    WHBLogUdpInit();
    WHBLogConsoleInit();
    WHBLogPrintf("rac1-link %s: start", VERSION);

    uint32_t apd = 0, dim = 0;
    IMIsAPDEnabled(&apd);
    IMIsDimEnabled(&dim);
    IMDisableAPD();  // a long transfer must not be cut by the auto power-down or the dimming
    IMDisableDim();

    g_affinity = link_cfg_int(SD_ROOT, "affinity", 0);
    if (link_cfg_int(SD_ROOT, "somemopt", 0)) {
        so_buf = memalign(0x40, SOMEMOPT_SIZE);
        so_flags = link_cfg_int(SD_ROOT, "somemopt_big", 1) ? SOMEMOPT_FLAGS_BIG_BUFFERS : SOMEMOPT_FLAGS_NONE;
        if (so_buf && pthread_create(&so_thread, NULL, so_main, NULL) == 0) {
            so_started = 1;
            somemopt(SOMEMOPT_REQUEST_WAIT_FOR_INIT, NULL, 0, SOMEMOPT_FLAGS_NONE);
            WHBLogPrintf("somemopt: %u KiB lent to nsysnet%s", SOMEMOPT_SIZE >> 10,
                         so_flags ? " (big buffers)" : "");
        }
    }

    // wut's socket init already called ACConnectAsync; wait for an address (the TV says so meanwhile).
    uint32_t ip = 0;
    int waited = 0;
    while (WHBProcIsRunning()) {
        ACGetAssignedAddress(&ip);
        if (ip)
            break;
        if (waited++ == 0)
            WHBLogPrintf("waiting for the network...");
        if (waited == 100)
            ACConnectAsync();
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(100));
    }
    snprintf(g_ip, sizeof g_ip, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 255, (ip >> 8) & 255, ip & 255);

    link_options o;
    memset(&o, 0, sizeof o);
    snprintf(o.sd_root, sizeof o.sd_root, "%s", SD_ROOT);
    o.tcp_port = LINK_TCP_PORT;
    o.udp_port = LINK_UDP_PORT;
    o.name = "rac1-link";
    o.version = VERSION;
    link_server *s = NULL;
    if (ip) {
        s = link_server_new(&o);
        g_server = s;
        ProcUIRegisterCallback(PROCUI_CALLBACK_RELEASE, on_release, NULL, 50);
        ProcUIRegisterCallback(PROCUI_CALLBACK_ACQUIRE, on_acquire, NULL, 50);
        if (link_server_start(s) != 0)
            WHBLogPrintf("cannot start the receiver threads");
        banner(s);
    }

    int last_state = -1, quitting = 0;
    uint64_t last_line = 0;
    while (WHBProcIsRunning()) {
        log_drain();
        if (s) {
            link_status st;
            link_server_status(s, &st);
            uint64_t now = lp_now_us();
            if ((st.state == LINK_ST_FILE || st.state == LINK_ST_BENCH) && st.total && now - last_line > 2000000) {
                double secs = (now - st.file_start_us) / 1e6;
                WHBLogPrintf("%.50s %.1f/%.1f MiB %.2f MiB/s", st.file, st.done / 1048576.0, st.total / 1048576.0,
                             secs > 0 ? st.done / 1048576.0 / secs : 0.0);
                last_line = now;
            }
            if (st.state == LINK_ST_IDLE && last_state >= LINK_ST_SESSION) {
                WHBLogPrintf("files: %u ok, %u failed, in %u session(s)", st.files_ok, st.files_bad, st.sessions);
                banner(s);
            }
            last_state = st.state;
            if (st.quit_requested && !quitting) {
                quitting = 1;
                WHBLogPrintf("rac1-link: auto-exit (asked by the PC)");
                log_drain();
                SYSLaunchMenu();
            }
            VPADStatus vs;
            VPADReadError err;
            if (VPADRead(VPAD_CHAN_0, &vs, 1, &err) == 1 && err == VPAD_READ_SUCCESS && (vs.trigger & VPAD_BUTTON_A))
                banner(s);
        }
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(16));
    }

    WHBLogPrintf("rac1-link: exit");
    if (s)
        link_server_stop(s);
    g_server = NULL;
    log_drain();
    if (so_started) {
        socket_lib_finish();  // ends somemopt's INIT request
        pthread_join(so_thread, NULL);
    }
    if (apd)
        IMEnableAPD();
    if (dim)
        IMEnableDim();
    WHBLogConsoleFree();
    WHBLogUdpDeinit();
    WHBLogModuleDeinit();
    WHBProcShutdown();
    return 0;
}
