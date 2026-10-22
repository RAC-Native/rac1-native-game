// The host's operating system on Windows (Win32, built with clang or GCC for MinGW-w64): see
// platform.h. Host code. Not yet run on a Windows machine (docs/platforms.md).
#ifndef NOMINMAX
#define NOMINMAX
#endif
#ifndef WIN32_LEAN_AND_MEAN
#define WIN32_LEAN_AND_MEAN
#endif
#include <windows.h>
#include <mmsystem.h>
#include <psapi.h>
#include <tlhelp32.h>

#include "platform.h"

#include <io.h>
#include <process.h>
#include <xmmintrin.h>

#include <cfenv>
#include <cstdlib>
#include <cstring>
#include <cwchar>

namespace rac1::platform {

namespace {
// UTF-8 <-> UTF-16 (the Win32 wide calls)
std::wstring widen(const std::string& s) {
    if (s.empty()) return {};
    const int n = MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), nullptr, 0);
    std::wstring w(static_cast<std::size_t>(n), L'\0');
    MultiByteToWideChar(CP_UTF8, 0, s.data(), static_cast<int>(s.size()), w.data(), n);
    return w;
}
}  // namespace

// ---- The coroutine: a fiber ----
// The host thread becomes a fiber the first time; the game's fiber runs a loop: each start runs the
// entry once, then switches back to the host and waits there for the next start. Both fibers are
// created with FIBER_FLAG_FLOAT_SWITCH, so each keeps its own FPU state (MXCSR, the x87 control word)
// across switches, as each ucontext does on Linux; a start sets the fiber's FPU state to the host
// thread's (getcontext's copy on Linux): the toward-zero rounding with FTZ / DAZ of decisions 11.
namespace {
struct Coroutine {
    void* host = nullptr;
    void* game = nullptr;
    void (*entry)() = nullptr;
    std::fenv_t env{};
    unsigned int mxcsr = 0;
};
Coroutine g_co;

void WINAPI fiber_main(void*) {
    for (;;) {
        std::fesetenv(&g_co.env);
        _mm_setcsr(g_co.mxcsr);  // (FTZ and DAZ too, whatever fesetenv keeps)
        g_co.entry();
        SwitchToFiber(g_co.host);  // (entry returned: the host goes on; the next start resumes here)
    }
}
}  // namespace

bool coroutine_create(std::size_t bytes) {
    if (g_co.game != nullptr) return true;
    if (IsThreadAFiber()) {
        g_co.host = GetCurrentFiber();
    } else {
        g_co.host = ConvertThreadToFiberEx(nullptr, FIBER_FLAG_FLOAT_SWITCH);
        if (g_co.host == nullptr) return false;
    }
    // (the reserve is address space only; Windows commits the stack as it grows, a guard page below it)
    g_co.game = CreateFiberEx(0, bytes, FIBER_FLAG_FLOAT_SWITCH, fiber_main, nullptr);
    return g_co.game != nullptr;
}

void coroutine_start(void (*entry)()) {
    g_co.entry = entry;
    std::fegetenv(&g_co.env);
    g_co.mxcsr = _mm_getcsr();
}

void coroutine_resume() { SwitchToFiber(g_co.game); }

void coroutine_yield() { SwitchToFiber(g_co.host); }

// ---- Files ----

std::FILE* open_file(const std::filesystem::path& path, const char* mode) {
    return _wfopen(path.c_str(), widen(mode).c_str());
}

std::int64_t read_at(std::FILE* f, std::uint64_t offset, void* dst, std::size_t n) {
    const HANDLE h = reinterpret_cast<HANDLE>(_get_osfhandle(_fileno(f)));
    if (h == INVALID_HANDLE_VALUE) return -1;
    OVERLAPPED ov{};  // (the offset of this read only: no shared position between threads)
    ov.Offset = static_cast<DWORD>(offset & 0xffffffffu);
    ov.OffsetHigh = static_cast<DWORD>(offset >> 32);
    DWORD got = 0;
    const DWORD want = static_cast<DWORD>(n < (1u << 30) ? n : (1u << 30));
    if (!ReadFile(h, dst, want, &got, &ov)) return GetLastError() == ERROR_HANDLE_EOF ? 0 : -1;
    return static_cast<std::int64_t>(got);
}

void will_need(std::FILE*, std::uint64_t, std::uint64_t) {}  // (no such hint on a plain handle)

bool map_file(const std::filesystem::path& path, const std::byte** data, std::size_t* size) {
    const HANDLE file = CreateFileW(path.c_str(), GENERIC_READ, FILE_SHARE_READ, nullptr, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, nullptr);
    if (file == INVALID_HANDLE_VALUE) return false;
    LARGE_INTEGER len{};
    if (!GetFileSizeEx(file, &len) || len.QuadPart <= 0) {
        CloseHandle(file);
        return false;
    }
    const HANDLE map = CreateFileMappingW(file, nullptr, PAGE_READONLY, 0, 0, nullptr);
    CloseHandle(file);
    if (map == nullptr) return false;
    void* p = MapViewOfFile(map, FILE_MAP_READ, 0, 0, 0);
    CloseHandle(map);  // (the view keeps the mapping)
    if (p == nullptr) return false;
    *data = static_cast<const std::byte*>(p);
    *size = static_cast<std::size_t>(len.QuadPart);
    return true;
}

void unmap_file(const std::byte* data, std::size_t) {
    if (data != nullptr) UnmapViewOfFile(data);
}

// ---- The player's folders ----

std::filesystem::path config_dir() {
    if (const wchar_t* a = _wgetenv(L"APPDATA"); a != nullptr && *a != 0) return std::filesystem::path(a) / "rac1-native";
    if (const wchar_t* u = _wgetenv(L"USERPROFILE"); u != nullptr && *u != 0)
        return std::filesystem::path(u) / "AppData" / "Roaming" / "rac1-native";
    return {};
}

std::filesystem::path data_dir() { return config_dir(); }

std::string system_language() {
    wchar_t name[LOCALE_NAME_MAX_LENGTH] = {};
    if (GetUserDefaultLocaleName(name, LOCALE_NAME_MAX_LENGTH) == 0) return {};
    std::string l;
    for (const wchar_t* c = name; *c >= L'a' && *c <= L'z' && l.size() < 3; ++c) l += static_cast<char>(*c);  // ("it-IT" -> "it")
    return l.size() == 2 ? l : std::string();
}

std::filesystem::path executable_path() {
    std::wstring buf(MAX_PATH, L'\0');
    for (;;) {
        const DWORD n = GetModuleFileNameW(nullptr, buf.data(), static_cast<DWORD>(buf.size()));
        if (n == 0) return {};
        if (n < buf.size()) {
            buf.resize(n);
            return std::filesystem::path(buf);
        }
        buf.resize(buf.size() * 2);
    }
}

// ---- Processes and the environment ----

namespace {
// One argument quoted for CommandLineToArgvW's rules (the C runtime's parsing of a command line).
std::wstring quote_arg(const std::wstring& a) {
    if (!a.empty() && a.find_first_of(L" \t\n\v\"") == std::wstring::npos) return a;
    std::wstring out = L"\"";
    for (std::size_t i = 0;; ++i) {
        std::size_t backslashes = 0;
        while (i < a.size() && a[i] == L'\\') {
            ++i;
            ++backslashes;
        }
        if (i == a.size()) {
            out.append(backslashes * 2, L'\\');
            break;
        }
        if (a[i] == L'"') {
            out.append(backslashes * 2 + 1, L'\\');
        } else {
            out.append(backslashes, L'\\');
        }
        out.push_back(a[i]);
    }
    out.push_back(L'"');
    return out;
}
}  // namespace

int run_quiet(const std::vector<std::string>& args) {
    if (args.empty()) return -1;
    std::wstring cmd;
    for (const auto& a : args) {
        if (!cmd.empty()) cmd.push_back(L' ');
        cmd += quote_arg(widen(a));
    }
    SECURITY_ATTRIBUTES sa{};
    sa.nLength = sizeof sa;
    sa.bInheritHandle = TRUE;
    const HANDLE nul = CreateFileW(L"NUL", GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, &sa, OPEN_EXISTING, 0, nullptr);
    STARTUPINFOW si{};
    si.cb = sizeof si;
    si.dwFlags = STARTF_USESTDHANDLES;
    si.hStdInput = GetStdHandle(STD_INPUT_HANDLE);
    si.hStdOutput = nul;
    si.hStdError = GetStdHandle(STD_ERROR_HANDLE);
    PROCESS_INFORMATION pi{};
    // (no application name: the program is looked up in PATH, as posix_spawnp does)
    const BOOL started = CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, TRUE, 0, nullptr, nullptr, &si, &pi);
    if (nul != INVALID_HANDLE_VALUE) CloseHandle(nul);
    if (!started) return -1;
    WaitForSingleObject(pi.hProcess, INFINITE);
    DWORD code = 0;
    const BOOL ok = GetExitCodeProcess(pi.hProcess, &code);
    CloseHandle(pi.hThread);
    CloseHandle(pi.hProcess);
    return ok ? static_cast<int>(code) : -1;
}

void launch_updater(const std::vector<std::string>& args, const std::filesystem::path& cwd) {
    if (args.empty()) return;
    std::wstring cmd;
    for (const auto& a : args) {
        if (!cmd.empty()) cmd.push_back(L' ');
        cmd += quote_arg(widen(a));
    }
    STARTUPINFOW si{};
    si.cb = sizeof si;
    PROCESS_INFORMATION pi{};
    const wchar_t* dir = cwd.empty() ? nullptr : cwd.c_str();
    if (CreateProcessW(nullptr, cmd.data(), nullptr, nullptr, FALSE, CREATE_NEW_CONSOLE, nullptr, dir, &si, &pi)) {
        CloseHandle(pi.hThread);
        CloseHandle(pi.hProcess);
    }
}

std::string python_program() {
    if (const char* p = std::getenv("RAC1_PYTHON"); p != nullptr && *p != 0) return p;
    return "python";
}

std::vector<std::string> environment_names(const char* prefix) {
    std::vector<std::string> out;
    for (char** e = _environ; e != nullptr && *e != nullptr; ++e) {
        const std::string kv = *e;
        if (kv.rfind(prefix, 0) != 0) continue;
        out.push_back(kv.substr(0, kv.find('=')));
    }
    return out;
}

void unset_environment(const char* name) {
    _putenv_s(name, "");  // (an empty value removes the C runtime's variable)
    SetEnvironmentVariableA(name, nullptr);
}

// ---- Threads, time ----

std::uint64_t thread_id() { return static_cast<std::uint64_t>(GetCurrentThreadId()); }

double thread_cpu_ms() {
    FILETIME created, exited, kernel, user;
    if (!GetThreadTimes(GetCurrentThread(), &created, &exited, &kernel, &user)) return 0.0;
    auto ticks = [](const FILETIME& f) { return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime; };
    return static_cast<double>(ticks(kernel) + ticks(user)) * 1e-4;  // (100 ns units)
}

bool local_time(std::time_t t, std::tm* out) { return localtime_s(out, &t) == 0; }

// ---- The process's resources ----

namespace {
std::uint64_t filetime_ticks(const FILETIME& f) { return (static_cast<std::uint64_t>(f.dwHighDateTime) << 32) | f.dwLowDateTime; }
std::string narrow(const wchar_t* w) {
    const int n = WideCharToMultiByte(CP_UTF8, 0, w, -1, nullptr, 0, nullptr, nullptr);
    if (n <= 1) return {};
    std::string s(static_cast<std::size_t>(n - 1), '\0');
    WideCharToMultiByte(CP_UTF8, 0, w, -1, s.data(), n, nullptr, nullptr);
    return s;
}
}  // namespace

bool process_memory(ProcessMemory* out) {
    PROCESS_MEMORY_COUNTERS_EX pmc{};
    pmc.cb = sizeof pmc;
    if (!K32GetProcessMemoryInfo(GetCurrentProcess(), reinterpret_cast<PROCESS_MEMORY_COUNTERS*>(&pmc), sizeof pmc)) return false;
    out->resident = pmc.WorkingSetSize;
    out->resident_peak = pmc.PeakWorkingSetSize;
    out->virtual_size = pmc.PrivateUsage;
    return true;
}

double process_cpu_ms() {
    FILETIME created, exited, kernel, user;
    if (!GetProcessTimes(GetCurrentProcess(), &created, &exited, &kernel, &user)) return 0.0;
    return static_cast<double>(filetime_ticks(kernel) + filetime_ticks(user)) * 1e-4;  // (100 ns units)
}

int cpu_count() {
    SYSTEM_INFO si{};
    GetSystemInfo(&si);
    return si.dwNumberOfProcessors > 0 ? static_cast<int>(si.dwNumberOfProcessors) : 1;
}

bool process_threads(std::vector<ThreadTime>* out) {
    out->clear();
    const HANDLE snap = CreateToolhelp32Snapshot(TH32CS_SNAPTHREAD, 0);
    if (snap == INVALID_HANDLE_VALUE) return false;
    // (GetThreadDescription: Windows 10 1607 and later, looked up)
    using GetDesc = HRESULT(WINAPI*)(HANDLE, PWSTR*);
    static const GetDesc get_desc = reinterpret_cast<GetDesc>(reinterpret_cast<void*>(GetProcAddress(GetModuleHandleW(L"kernel32.dll"), "GetThreadDescription")));
    const DWORD pid = GetCurrentProcessId();
    THREADENTRY32 te{};
    te.dwSize = sizeof te;
    for (BOOL ok = Thread32First(snap, &te); ok; ok = Thread32Next(snap, &te)) {
        if (te.th32OwnerProcessID != pid) continue;
        ThreadTime t;
        t.tid = te.th32ThreadID;
        const HANDLE h = OpenThread(THREAD_QUERY_LIMITED_INFORMATION, FALSE, te.th32ThreadID);
        if (h != nullptr) {
            FILETIME created, exited, kernel, user;
            if (GetThreadTimes(h, &created, &exited, &kernel, &user)) t.cpu_ms = static_cast<double>(filetime_ticks(kernel) + filetime_ticks(user)) * 1e-4;
            PWSTR desc = nullptr;
            if (get_desc != nullptr && SUCCEEDED(get_desc(h, &desc)) && desc != nullptr) {
                t.name = narrow(desc);
                LocalFree(desc);
            }
            CloseHandle(h);
        }
        out->push_back(std::move(t));
    }
    CloseHandle(snap);
    return true;
}

bool gpu_client_info(GpuClientInfo* out) {
    *out = GpuClientInfo{};
    return false;
}

// ---- The process's host settings ----

namespace {
DWORD g_console_mode = 0;  // (the console input's mode before process_begin)
bool g_console_mode_set = false;
bool g_timer_period = false;
}  // namespace

void process_begin() {
    // QuickEdit off: ENABLE_EXTENDED_FLAGS must be in the mode for ENABLE_QUICK_EDIT_MODE's bit to be taken
    // (a console the process does not have, or input redirected from a file: GetConsoleMode fails, nothing done)
    const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
    DWORD mode = 0;
    if (in != nullptr && in != INVALID_HANDLE_VALUE && GetConsoleMode(in, &mode) != 0) {
        const DWORD want = (mode | ENABLE_EXTENDED_FLAGS) & ~static_cast<DWORD>(ENABLE_QUICK_EDIT_MODE);
        if (want != mode && SetConsoleMode(in, want) != 0) {
            g_console_mode = mode;
            g_console_mode_set = true;
        }
    }
    g_timer_period = timeBeginPeriod(1) == TIMERR_NOERROR;
}

void process_end() {
    if (g_console_mode_set) {
        const HANDLE in = GetStdHandle(STD_INPUT_HANDLE);
        if (in != nullptr && in != INVALID_HANDLE_VALUE) SetConsoleMode(in, g_console_mode | ENABLE_EXTENDED_FLAGS);
        g_console_mode_set = false;
    }
    if (g_timer_period) timeEndPeriod(1);
    g_timer_period = false;
}

// ---- Debugging ----

void print_backtrace(int frames) {
    // the return addresses and the program's base, for addr2line / llvm-symbolizer (no DbgHelp needed)
    void* f[64];
    const USHORT n = CaptureStackBackTrace(0, static_cast<DWORD>(frames < 64 ? frames : 64), f, nullptr);
    std::fprintf(stderr, "[backtrace] base %p\n", static_cast<void*>(GetModuleHandleW(nullptr)));
    for (USHORT i = 0; i < n; ++i) std::fprintf(stderr, "  %p\n", f[i]);
}

}  // namespace rac1::platform
