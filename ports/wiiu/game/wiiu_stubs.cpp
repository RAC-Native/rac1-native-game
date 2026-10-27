// What the Wii U build leaves out, as functions that do nothing (the audio device is wiiu_audio.cpp's). Host code.
#include <cstdint>

// The threads' priorities (rtkit on Linux): the console's scheduler as it is
namespace rac1 {
bool host_priority_enabled() { return false; }
void host_priority_set_enabled(bool) {}
bool host_priority_realtime(std::uint64_t, int) { return false; }
int host_priority_raise_all(int) { return 0; }
}  // namespace rac1

// 64-bit atomics: the PowerPC 750 has none (32-bit lwarx / stwcx. only) and devkitPPC ships no libatomic, so GCC's
// calls for std::atomic<uint64_t> land here, under one spinlock that also masks interrupts on its core.
#include <coreinit/spinlock.h>

namespace {
OSSpinLock g_atomic64_lock = {};
struct Atomic64Lock {
    Atomic64Lock() { OSUninterruptibleSpinLock_Acquire(&g_atomic64_lock); }
    ~Atomic64Lock() { OSUninterruptibleSpinLock_Release(&g_atomic64_lock); }
};
}  // namespace

extern "C" {
uint64_t __atomic_load_8(const volatile void* p, int) {
    Atomic64Lock l;
    return *static_cast<const volatile uint64_t*>(p);
}
void __atomic_store_8(volatile void* p, uint64_t v, int) {
    Atomic64Lock l;
    *static_cast<volatile uint64_t*>(p) = v;
}
uint64_t __atomic_exchange_8(volatile void* p, uint64_t v, int) {
    Atomic64Lock l;
    const uint64_t old = *static_cast<volatile uint64_t*>(p);
    *static_cast<volatile uint64_t*>(p) = v;
    return old;
}
bool __atomic_compare_exchange_8(volatile void* p, void* expected, uint64_t desired, bool, int, int) {
    Atomic64Lock l;
    volatile uint64_t* q = static_cast<volatile uint64_t*>(p);
    uint64_t* e = static_cast<uint64_t*>(expected);
    if (*q == *e) {
        *q = desired;
        return true;
    }
    *e = *q;
    return false;
}
uint64_t __atomic_fetch_add_8(volatile void* p, uint64_t v, int) {
    Atomic64Lock l;
    const uint64_t old = *static_cast<volatile uint64_t*>(p);
    *static_cast<volatile uint64_t*>(p) = old + v;
    return old;
}
}

// GLFW: no window system on the console. RAC1_NO_GRAPHICS never opens a window; these answer as GLFW does with none.
#include <GLFW/glfw3.h>

extern "C" {
int glfwInit(void) { return GLFW_FALSE; }
void glfwTerminate(void) {}
int glfwGetPlatform(void) { return GLFW_PLATFORM_NULL; }
void glfwWindowHint(int, int) {}
GLFWwindow* glfwCreateWindow(int, int, const char*, GLFWmonitor*, GLFWwindow*) { return nullptr; }
void glfwDestroyWindow(GLFWwindow*) {}
int glfwWindowShouldClose(GLFWwindow*) { return GLFW_TRUE; }
void glfwPollEvents(void) {}
void glfwSwapBuffers(GLFWwindow*) {}
void glfwSwapInterval(int) {}
void glfwMakeContextCurrent(GLFWwindow*) {}
GLFWglproc glfwGetProcAddress(const char*) { return nullptr; }
int glfwExtensionSupported(const char*) { return GLFW_FALSE; }
GLFWmonitor** glfwGetMonitors(int* count) {
    if (count != nullptr) *count = 0;
    return nullptr;
}
GLFWmonitor* glfwGetPrimaryMonitor(void) { return nullptr; }
const char* glfwGetMonitorName(GLFWmonitor*) { return ""; }
const GLFWvidmode* glfwGetVideoMode(GLFWmonitor*) { return nullptr; }
void glfwGetMonitorPos(GLFWmonitor*, int* x, int* y) {
    if (x != nullptr) *x = 0;
    if (y != nullptr) *y = 0;
}
GLFWmonitor* glfwGetWindowMonitor(GLFWwindow*) { return nullptr; }
void glfwSetWindowMonitor(GLFWwindow*, GLFWmonitor*, int, int, int, int, int) {}
void glfwGetWindowSize(GLFWwindow*, int* w, int* h) {
    if (w != nullptr) *w = 0;
    if (h != nullptr) *h = 0;
}
void glfwSetWindowSize(GLFWwindow*, int, int) {}
void glfwGetWindowPos(GLFWwindow*, int* x, int* y) {
    if (x != nullptr) *x = 0;
    if (y != nullptr) *y = 0;
}
void glfwSetWindowPos(GLFWwindow*, int, int) {}
void glfwGetFramebufferSize(GLFWwindow*, int* w, int* h) {
    if (w != nullptr) *w = 0;
    if (h != nullptr) *h = 0;
}
int glfwGetWindowAttrib(GLFWwindow*, int) { return 0; }
void glfwSetInputMode(GLFWwindow*, int, int) {}
int glfwRawMouseMotionSupported(void) { return GLFW_FALSE; }
int glfwGetKey(GLFWwindow*, int) { return GLFW_RELEASE; }
int glfwGetMouseButton(GLFWwindow*, int) { return GLFW_RELEASE; }
void glfwGetCursorPos(GLFWwindow*, double* x, double* y) {
    if (x != nullptr) *x = 0.0;
    if (y != nullptr) *y = 0.0;
}
int glfwJoystickPresent(int) { return GLFW_FALSE; }
int glfwJoystickIsGamepad(int) { return GLFW_FALSE; }
const char* glfwGetJoystickName(int) { return nullptr; }
const char* glfwGetJoystickGUID(int) { return nullptr; }
const char* glfwGetGamepadName(int) { return nullptr; }
int glfwGetGamepadState(int, GLFWgamepadstate*) { return GLFW_FALSE; }
int glfwUpdateGamepadMappings(const char*) { return GLFW_TRUE; }
const float* glfwGetJoystickAxes(int, int* count) {
    if (count != nullptr) *count = 0;
    return nullptr;
}
const unsigned char* glfwGetJoystickButtons(int, int* count) {
    if (count != nullptr) *count = 0;
    return nullptr;
}
const unsigned char* glfwGetJoystickHats(int, int* count) {
    if (count != nullptr) *count = 0;
    return nullptr;
}
}

// (diagnostics) operator new that says why an allocation failed: the size asked, the default heap's free memory, the
// caller, before the bad_alloc (which itself needs memory)
#include <coreinit/debug.h>
#include <coreinit/memdefaultheap.h>
#include <coreinit/memexpheap.h>
#include <cstdlib>
#include <new>

void* operator new(std::size_t n) {
    if (void* p = std::malloc(n == 0 ? 1 : n)) return p;
    OSReport("rac1-wiiu: operator new(%u) failed; caller %p; MEM2 default heap free %u, largest %u\n",
             static_cast<unsigned>(n), __builtin_return_address(0),
             static_cast<unsigned>(MEMGetTotalFreeSizeForExpHeap(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2))),
             static_cast<unsigned>(MEMGetAllocatableSizeForExpHeapEx(MEMGetBaseHeapHandle(MEM_BASE_HEAP_MEM2), 4)));
    OSFatal("rac1-wiiu: out of memory (operator new)");
    return nullptr;
}
void* operator new[](std::size_t n) { return operator new(n); }
void operator delete(void* p) noexcept { std::free(p); }
void operator delete[](void* p) noexcept { std::free(p); }
void operator delete(void* p, std::size_t) noexcept { std::free(p); }
void operator delete[](void* p, std::size_t) noexcept { std::free(p); }

// The VU1's translated programs, left out of this build (CMakeLists.txt): without graphics none runs. Should one run,
// it says so and the game stops (its result would be wrong, not just missing).
#include "runtime/vu/vu_interpreter.h"

namespace rac1 {
struct VuPipe;
// (a start of one of them, the picture's chain walk reaching the world's or a moby's VU1 part: the program ends at once,
// nothing kicked; that 3D is the renderer's own. Said once per program)
static bool vu1_program_missing(const char* name, bool& said) {
    if (!said) OSReport("rac1-wiiu: the VU1 program %s started: not in this build, ended at once\n", name);
    said = true;
    return false;
}
#define RAC1_VU1_MISSING(fn) \
    bool fn(VUState&, VuPipe&) {        \
        static bool said = false;       \
        return vu1_program_missing(#fn, said); \
    }
RAC1_VU1_MISSING(moby_vu1_step)
RAC1_VU1_MISSING(moby_vu1_fast)
RAC1_VU1_MISSING(world_tfrag_a_step)
RAC1_VU1_MISSING(world_tfrag_a_fast)
RAC1_VU1_MISSING(world_tfrag_b_step)
RAC1_VU1_MISSING(world_tfrag_b_fast)
RAC1_VU1_MISSING(world_tie_a_step)
RAC1_VU1_MISSING(world_tie_a_fast)
RAC1_VU1_MISSING(world_tie_b_step)
RAC1_VU1_MISSING(world_tie_b_fast)
RAC1_VU1_MISSING(world_shrub_a_step)
RAC1_VU1_MISSING(world_shrub_a_fast)
RAC1_VU1_MISSING(world_shrub_b_step)
RAC1_VU1_MISSING(world_shrub_b_fast)
}  // namespace rac1
