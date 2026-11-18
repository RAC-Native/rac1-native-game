#include "trial_guard.h"

#include <exception>
#include <stdexcept>

#ifdef _WIN32
#include <windows.h>
#include <excpt.h>
#else
#include <csetjmp>
#include <csignal>
#include <sys/time.h>
#endif

namespace rac1::oracle {
#if defined(_WIN32) && !defined(__clang__) && !defined(_MSC_VER)
extern "C" long oracle_trial_access_violation(EXCEPTION_POINTERS*, void*);
#endif
namespace {

// Keep C++ exception handling in a separate frame from the Windows SEH scope.
#if defined(__GNUC__)
__attribute__((noinline))
#endif
bool invoke_port(void (*port)(uint32_t), uint32_t moby) {
    try { port(moby); } catch (const std::exception&) { return false; }
    return true;
}

#ifdef _WIN32
#if defined(__GNUC__)
__attribute__((noinline))
#endif
bool guarded_call(void (*port)(uint32_t), uint32_t moby) {
#if defined(__clang__) || defined(_MSC_VER)
    __try {
        return invoke_port(port, moby);
    } __except (GetExceptionCode() == EXCEPTION_ACCESS_VIOLATION
                    ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH) {
        return false;
    }
#elif defined(__MINGW32__) && defined(__x86_64__)
    // MinGW GCC provides the same SEH scope through excpt.h's primitives.
    // The filter has C linkage because __try1 embeds its name in unwind data.
    volatile bool ok = false;
    __try1(oracle_trial_access_violation)
    ok = invoke_port(port, moby);
    __except1
    return ok;
#else
#error The Windows oracle trial guard needs clang or x86-64 MinGW GCC
#endif
}

void CALLBACK trial_timeout(PTP_CALLBACK_INSTANCE, void*, PTP_TIMER) {
    constexpr char message[] = "[oracle] native trial timed out; stopping with status 124 (the state was not validated)\n";
    DWORD written;
    // Do not take a stdio lock: the interrupted trial might hold it.
    WriteFile(GetStdHandle(STD_ERROR_HANDLE), message, sizeof(message) - 1, &written, nullptr);
    TerminateProcess(GetCurrentProcess(), 124);
}

class TrialTimer {
public:
    explicit TrialTimer(unsigned seconds) {
        if (!seconds) return;
        timer_ = CreateThreadpoolTimer(trial_timeout, nullptr, nullptr);
        if (!timer_) throw std::runtime_error("oracle: could not create trial timeout");
        ULARGE_INTEGER due;
        due.QuadPart = static_cast<ULONGLONG>(-static_cast<LONGLONG>(seconds) * 10000000);
        FILETIME when{due.LowPart, due.HighPart};
        SetThreadpoolTimer(timer_, &when, 0, 0);
    }
    ~TrialTimer() {
        if (!timer_) return;
        SetThreadpoolTimer(timer_, nullptr, 0, 0);
        WaitForThreadpoolTimerCallbacks(timer_, TRUE);
        CloseThreadpoolTimer(timer_);
    }
    TrialTimer(const TrialTimer&) = delete;
    TrialTimer& operator=(const TrialTimer&) = delete;
private:
    PTP_TIMER timer_ = nullptr;
};
#else
sigjmp_buf trial_jump;
void trial_signal(int) { siglongjmp(trial_jump, 1); }
#endif

}  // namespace

#if defined(_WIN32) && !defined(__clang__) && !defined(_MSC_VER)
extern "C" long oracle_trial_access_violation(EXCEPTION_POINTERS* exception, void*) {
    return exception->ExceptionRecord->ExceptionCode == EXCEPTION_ACCESS_VIOLATION
        ? EXCEPTION_EXECUTE_HANDLER : EXCEPTION_CONTINUE_SEARCH;
}
#endif

bool run_trial(void (*port)(uint32_t), uint32_t moby, unsigned timeout_seconds) {
#ifdef _WIN32
    TrialTimer timer(timeout_seconds);
    return guarded_call(port, moby);
#else
    struct sigaction action {}, old_segv {}, old_alarm {};
    action.sa_handler = trial_signal;
    sigemptyset(&action.sa_mask);
    action.sa_flags = SA_NODEFER;
    sigaction(SIGSEGV, &action, &old_segv);
    if (timeout_seconds) sigaction(SIGALRM, &action, &old_alarm);
    bool ok = true;
    if (sigsetjmp(trial_jump, 1) != 0) ok = false;
    else {
        if (timeout_seconds) {
            itimerval timer {};
            timer.it_value.tv_sec = timeout_seconds;
            setitimer(ITIMER_REAL, &timer, nullptr);
        }
        ok = invoke_port(port, moby);
    }
    if (timeout_seconds) {
        itimerval off {};
        setitimer(ITIMER_REAL, &off, nullptr);
        sigaction(SIGALRM, &old_alarm, nullptr);
    }
    sigaction(SIGSEGV, &old_segv, nullptr);
    return ok;
#endif
}

}  // namespace rac1::oracle
