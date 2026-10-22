# Other platforms

rac1-native runs on Linux x86-64, and on Windows x86-64. This page is the state of the
Windows port, what remains, and a note on macOS.

The goal is one codebase that builds on Linux and Windows and behaves identically on both: same
game state, same oracle results, same reference images.

## What is already portable

- **The libraries.**
  - GLFW: window, input, gamepads.
  - glad and OpenGL 4.6.
  - Dear ImGui, the menus' widgets.
  - miniaudio: audio output.
  - OpenGOAL's 989snd.

  All of them support Windows.
- **The game's code** (`src/game/`) is standard C++20 with no operating-system calls.
- **OpenGL 4.6**, compute shaders included, is supported on Windows by the NVIDIA, AMD and Intel drivers.

## The host interface

Every operating-system call is behind one small interface, `src/platform/platform.h`, with one
implementation per system: `platform_posix.cpp` (Linux) and `platform_win32.cpp` (Windows). CMake
compiles one of them. `src/game/` and `runtime/` call the interface and stay standard C++.

| Area | Where it is used | Linux | Windows |
|---|---|---|---|
| The game frame's coroutine ([Decisions](decisions.md), 12) | `runtime/ee/ee_thread.cpp` | `ucontext`, a 64 MB `mmap` stack | a fiber (`CreateFiberEx`, 64 MB reserved) |
| The FPU mode in the coroutine ([Decisions](decisions.md), 11) | the coroutine's start | `getcontext` copies it | the fiber copies the host thread's (`fegetenv` and MXCSR), and each fiber keeps its own (`FIBER_FLAG_FLOAT_SWITCH`) |
| Float rounding toward zero ([Decisions](decisions.md), 11) | `runtime/ee/ps2_fpu.h` | `fesetround` and MXCSR | the same calls (MinGW-w64 has both) |
| Memory-mapped files | `src/io/mapped_file.h` (the setup) | `mmap` | `CreateFileMapping` / `MapViewOfFile` |
| Reads of the disc at an offset | `runtime/kernel/disc.cpp` | `pread`, `posix_fadvise` | `ReadFile` with an offset |
| Settings and saves folders | the options pages, the controls, the memory card | `~/.config/rac1-native/`, `~/.local/share/rac1-native/` (or `XDG_*`) | `%APPDATA%\rac1-native\` |
| Files by path | the settings files | `fopen` | `_wfopen` (any characters) |
| The local time | the memory card's dates, the CDVD clock | `localtime_r` | `localtime_s` |
| Thread ids, CPU time | the audio threads, the frame profiler | `gettid`, `clock_gettime` | `GetCurrentThreadId`, `GetThreadTimes` |
| The process's resources (`process_memory`, `process_cpu_ms`, `process_threads`, `gpu_client_info`) | the debug menu's profiler (`src/profiler/`) | `/proc/self/status`, `/proc/self/task/*/{comm,schedstat}`, `CLOCK_PROCESS_CPUTIME_ID`, the DRM's `/proc/self/fdinfo` and amdgpu's sysfs | `GetProcessMemoryInfo`, `GetProcessTimes`, a Toolhelp snapshot with `GetThreadTimes` / `GetThreadDescription`; no GPU account (the GL extensions instead) |
| The setup's helper programs | `src/setup/setup_manager.cpp` | `posix_spawnp` | `CreateProcessW` |
| The environment | the debug tools gate | `environ`, `unsetenv` | `_environ`, `_putenv_s` |
| Backtraces (`EE_BT`) | `runtime/kernel/ee_memory.cpp` | glibc's `backtrace` | the return addresses (`CaptureStackBackTrace`), for `addr2line` |
| The console and the timer at start (`process_begin` / `process_end`) | `src/host/orig_main.cpp`'s `main` | nothing | the console's QuickEdit mode off (a click in the console window paused the game) and `timeBeginPeriod(1)` (a sleep wakes up within ~1 ms, not 15.6 ms), both put back at the exit |
| Thread priority | `src/host/host_priority_{posix,win32}.cpp` | rtkit over D-Bus | `SetThreadPriority` |
| The disc image's link | the setup, `runtime/kernel/disc.cpp` | a symbolic link | a link if allowed, else the path in `assets/iso/disc-path.txt` |

The Linux side behaves as before: the three reference images are unchanged.

Left as they are, Linux only: the sampling profiler (`src/host/sample_prof.cpp`, `RAC1_SAMPLE_PROF`,
a no-op elsewhere) and the overlay matcher's `--ported` count (it calls `grep`; the setup does not
use it).

## The compiler

The port must stay faithful: the PS2's float rounding, and the same results as the original in
every oracle check. That depends on the compiler generating the same float operations on both
platforms. The build uses `-frounding-math` and `-ffp-contract=off` (no `a * b + c` fused into
one rounding) with both GCC and clang; adding `-ffp-contract=off` left the Linux reference images
unchanged.

- **clang** on Windows (MSYS2's CLANG64 environment), or GCC for MinGW-w64. Both have the GCC
  extensions the code uses, and the same float behaviour as the Linux build, so the game's state
  should match bit for bit.
- **MSVC** (Visual Studio's compiler) is not supported: CMake stops with a message. It would mean
  more work (GCC extensions; the very large generated sources) and more risk to float exactness;
  it is a later step if it is ever needed.

Windows-specific build settings (`CMakeLists.txt`): a 64 MB main-thread stack (Windows gives
1 MB; the game's code also runs on the main thread outside the coroutine), the C++ runtime
linked in when FFmpeg is not, a manifest (`src/platform/win32/`) that makes the program's file
names UTF-8, and GCC's large object format for the biggest generated sources.

Cross-compiling from Linux (Debian / Ubuntu: `sudo apt install clang lld mingw-w64`):

```bash
cmake -S . -B build-win -DCMAKE_TOOLCHAIN_FILE=cmake/toolchains/clang-x86_64-w64-mingw32.cmake
cmake --build build-win -j 4
```

(`cmake/toolchains/mingw-w64-x86_64.cmake` does the same with GCC.) This needs the generated
sources, which `./setup.sh` makes on Linux: they are the same on Windows.

## Verification

- **Build:** a CI workflow (`.github/workflows/build.yml`) builds on Linux, on a Windows runner
  (MSYS2, clang) and cross-compiled from Linux, and on a Linux ARM64 runner (not blocking yet: the ARM64
  port is in progress, #78). A public CI has no disc, so it cannot generate the
  game's code: it builds with `-DRAC1_NO_GENERATED=ON`, which compiles everything else (the
  hardware layer, the formats, the platform layer, the setup mode, the debug menu, the host, the
  game's hand-written code) and links nothing.
- **Behaviour:** the headless reference runs on Windows (`RAC1_HEADLESS`) should give the same
  image hashes as on Linux, and the oracle checks must pass. These need the player's own disc, so
  they run on a developer's machine, not in public CI. An image can differ for a reason other than
  the game's state: the pictures come from the GPU and its driver, and the host renderer uses the
  C library's `sin` / `cos`, which differ between glibc and Windows. The same GPU on both systems
  is the fair comparison; the game's own code does not call them.
- **A human playtest** on a Windows PC.

## State

1. Done: the host interface on Linux, with no change in behaviour (same reference hashes).
2. Done: the Windows implementations, the clang / MinGW-w64 build, the toolchain files. The full
   game also cross-compiles and links from Linux with clang and with GCC.
3. Done: the setup (`setup.cmd`, or `./setup.sh` in MSYS2's shell; both run `tools/setup.py`), and
   the Windows instructions in [Setup](setup.md#on-windows).
4. Done: the CI workflow (Linux, Windows, and the Windows cross-build). Run and played on one Windows
   PC. To do: more Windows machines, the reference runs and the oracle on Windows, a longer
   playtest.
5. To do: a release with a Windows build. The player still runs the setup with their own disc.

What has been tried least: everything that only a Windows machine shows, on more than one PC. The
fiber and its FPU mode, the paths, the disc's reads, the setup's helper programs, the audio
(miniaudio's WASAPI), the gamepads, the window. The cross-builds use MinGW-w64's older C runtime (`msvcrt.dll`) and GCC's
C++ library; MSYS2's CLANG64 uses the newer one (UCRT) and LLVM's C++ library, which the CI's
Windows job compiles against.

## macOS

macOS stops at OpenGL 4.1 (deprecated, no compute shaders), so the current renderer can't run
there. macOS comes after a Vulkan renderer, through MoltenVK, or a Metal one (see
[Roadmap](roadmap.md)). The host-interface work above would carry over.

## Other devices

The code is portable C++, but a device needs more than a renderer:
- a graphics API it supports (OpenGL ES or a console's own);
- a little-endian processor, because the game's memory is little-endian like the PS2's;
- enough memory and speed.

Each such port is its own project. The first, to ARM64 and Android, is in progress on the
`port/android` branch (#78); the CI's Linux ARM64 build does not pass yet (parts of the PS2 layer
still use x86 instructions), so it does not block.
