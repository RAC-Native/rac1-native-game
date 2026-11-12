// The game's window as a desktop's window, and the process's end on a fatal error: see host_window.h. Host code.
#include "host_window.h"

#include <platform/platform.h>

#include <GLFW/glfw3.h>

#include <atomic>
#include <csignal>
#include <cstdio>
#include <cstdlib>
#include <exception>
#include <typeinfo>

#ifdef _WIN32
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#define GLFW_EXPOSE_NATIVE_WIN32
#include <GLFW/glfw3native.h>
#endif

namespace rac1 {

namespace {

#ifdef _WIN32
std::atomic<bool> g_dying{false};
std::atomic<HWND> g_hwnd{nullptr};

// orig: (host) the end of a fatal error: the log flushed, the monitor's desktop mode back, the window out of the way, the
// process gone with exit status 3 (no error report kept waiting, no dialog)
[[noreturn]] void die(const char* what) {
    if (g_dying.exchange(true)) {  // (a second fatal error on another thread while the first ends the process)
        TerminateProcess(GetCurrentProcess(), 3);
        std::_Exit(3);
    }
    std::fprintf(stderr, "[fatal] %s: the game stopped\n", what);
    platform::print_backtrace(32);
    std::fflush(stdout);
    std::fflush(stderr);
    // (from any thread: the window's own thread may be the one stuck. ShowWindowAsync and the display settings call
    // never wait for it; the process's end destroys the window anyway)
    if (HWND h = g_hwnd.load(); h != nullptr) ShowWindowAsync(h, SW_HIDE);
    ChangeDisplaySettingsExW(nullptr, nullptr, nullptr, 0, nullptr);  // (a fullscreen mode of its own: the desktop's back)
    TerminateProcess(GetCurrentProcess(), 3);
    std::_Exit(3);
}
#endif

// orig: (host) std::terminate's handler: an exception nobody caught (the message of a std::exception), or terminate called
[[noreturn]] void on_terminate() {
    char msg[512];
    if (std::exception_ptr e = std::current_exception(); e != nullptr) {
        try {
            std::rethrow_exception(e);
        } catch (const std::exception& x) {
            std::snprintf(msg, sizeof msg, "an error not handled (%s: %s)", typeid(x).name(), x.what());
        } catch (...) {
            std::snprintf(msg, sizeof msg, "an error not handled (not a std::exception)");
        }
    } else {
        std::snprintf(msg, sizeof msg, "std::terminate called");
    }
#ifdef _WIN32
    die(msg);
#else
    std::fprintf(stderr, "[fatal] %s: the game stopped\n", msg);
    platform::print_backtrace(32);
    std::fflush(stdout);
    std::fflush(stderr);
    std::abort();  // (Linux: a core dump as before, if the system keeps them)
#endif
}

#ifdef _WIN32
// orig: (host) abort() (SIGABRT): the C runtime's would ask Windows' error reporting first
void on_abort(int) { die("abort() called"); }

// orig: (host) a crash (an access violation, ...) that no handler took
LONG WINAPI on_crash(EXCEPTION_POINTERS* ep) {
    char msg[160];
    const EXCEPTION_RECORD* r = ep != nullptr ? ep->ExceptionRecord : nullptr;
    const unsigned long code = r != nullptr ? static_cast<unsigned long>(r->ExceptionCode) : 0ul;
    // (0x20474343, "GCC": a C++ exception nobody caught, as the clang / MinGW runtime raises it - std::terminate not reached)
    std::snprintf(msg, sizeof msg, "%s (exception 0x%08lx at %p)", code == 0x20474343ul ? "an error not handled (a C++ exception)" : "a crash", code,
                  r != nullptr ? r->ExceptionAddress : nullptr);
    die(msg);
}
#endif

}  // namespace

// orig: (host) see host_window.h
void host_fatal_handlers_install() {
    std::set_terminate(on_terminate);
#ifdef _WIN32
    // (no "has stopped working" dialog, no critical-error box for a missing drive: the handlers below end the process)
    SetErrorMode(GetErrorMode() | SEM_FAILCRITICALERRORS | SEM_NOGPFAULTERRORBOX);
    // (the C runtime's abort: no message box, no report to Windows; SIGABRT's handler ends the process)
    _set_abort_behavior(0, _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
    std::signal(SIGABRT, on_abort);
    SetUnhandledExceptionFilter(on_crash);
#endif
}

// orig: (host) see host_window.h
void host_window_created(GLFWwindow* w) {
#ifdef _WIN32
    HWND h = w != nullptr ? glfwGetWin32Window(w) : nullptr;
    g_hwnd.store(h);
#endif
    host_window_frame(w);
#ifdef _WIN32
    // (no longer topmost, a window that did not get the focus at its start would open behind the others: asked to the front
    // once - Windows allows it to a game started by the player, from the launcher or a click - else its taskbar button
    // flashes until it is clicked)
    if (h != nullptr && GetForegroundWindow() != h && !SetForegroundWindow(h)) {
        FLASHWINFO fi{sizeof fi, h, FLASHW_TRAY | FLASHW_TIMERNOFG, 0, 0};
        FlashWindowEx(&fi);
    }
#endif
}

// orig: (host) see host_window.h
void host_window_frame(GLFWwindow* w) {
#ifdef _WIN32
    if (w == nullptr) return;
    HWND h = glfwGetWin32Window(w);
    if (h == nullptr) return;
    if ((GetWindowLongPtrW(h, GWL_EXSTYLE) & WS_EX_TOPMOST) == 0) return;
    // (its place and size, its focus kept: only out of the topmost band)
    SetWindowPos(h, HWND_NOTOPMOST, 0, 0, 0, 0, SWP_NOMOVE | SWP_NOSIZE | SWP_NOACTIVATE | SWP_NOOWNERZORDER);
    static int said = 0;
    if (said++ < 3) std::printf("[window] the fullscreen window made an ordinary one (not always on top)\n");
#else
    (void)w;
#endif
}

}  // namespace rac1
