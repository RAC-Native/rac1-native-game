// The EE's main thread as a coroutine of the host (ADR 0012): see ee_thread.h.
#include "ee_thread.h"

#include <platform/platform.h>

#include <cstdio>
#include <cstdlib>
#include <exception>

namespace rac1 {

namespace {
// 64 MB of address space (committed as it is touched), a guard page below it: the game's frames
// (the transliterations' register files, their callees) as deep as on the host's main stack.
// The stack and the switches are the platform's (src/platform: a hand-written switch on x86-64 and AArch64,
// ucontext elsewhere, a fiber on Windows).
constexpr size_t kStack = 64u << 20;

struct Coroutine {
    bool stack = false;      // the platform's coroutine created
    bool running = false;    // fn started, not returned
    bool inside = false;     // the coroutine is the current context
    bool suspended = false;  // waiting in ee_thread_vsync
    EeVsyncFrame frame = EeVsyncFrame::Kicked;
    std::function<void()> fn;
    std::exception_ptr error;
};
Coroutine g_co;
void (*g_vsync_hook)(EeVsyncFrame) = nullptr;  // the host's per-vsync work (ee_thread_set_vsync_hook)

void entry() {
    try {
        g_co.fn();
    } catch (...) {
        g_co.error = std::current_exception();
    }
    g_co.running = false;
    g_co.inside = false;
    // (returning from here resumes the host)
}
}  // namespace

// orig: (host) the EE thread's coroutine
bool ee_thread_run(const std::function<void()>& fn) {
    if (g_co.inside) {  // (already on the game's thread: a nested run is a plain call)
        fn();
        return true;
    }
    if (!g_co.suspended) {
        if (!g_co.stack) {
            if (!platform::coroutine_create(kStack)) {
                std::fprintf(stderr, "[ee-thread] no stack: the game runs on the host's thread\n");
                fn();
                return true;
            }
            g_co.stack = true;
        }
        g_co.fn = fn;
        g_co.running = true;
        platform::coroutine_start(entry);  // (the host thread's FPU modes, ADR 0011's rounding, carried into the coroutine)
    }
    g_co.suspended = false;
    g_co.inside = true;
    platform::coroutine_resume();
    g_co.inside = false;
    if (g_co.error) {
        std::exception_ptr e = g_co.error;
        g_co.error = nullptr;
        g_co.suspended = false;
        g_co.running = false;
        std::rethrow_exception(e);
    }
    if (!g_co.running) g_co.fn = nullptr;
    return !g_co.running;
}

// orig: (host) the EE thread's coroutine
bool ee_thread_suspended() { return g_co.suspended; }
// orig: (host) the EE thread's coroutine
EeVsyncFrame ee_thread_frame() { return g_co.frame; }
// orig: (host) the EE thread's coroutine
bool ee_thread_inside() { return g_co.inside; }

// orig: (host) the EE thread's coroutine
void ee_thread_set_vsync_hook(void (*hook)(EeVsyncFrame)) { g_vsync_hook = hook; }

// orig: (host) the vsync wait (the kernel's semaphore, 0x122298) inside a blocking loop
void ee_thread_vsync(EeVsyncFrame kind, bool shown) {
    if (g_vsync_hook != nullptr) g_vsync_hook(kind);
    if (!g_co.inside || !shown) return;
    g_co.frame = kind;
    g_co.suspended = true;
    g_co.inside = false;
    platform::coroutine_yield();
    g_co.inside = true;
}

}  // namespace rac1
