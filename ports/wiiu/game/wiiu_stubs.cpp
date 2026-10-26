// What the Wii U build leaves out, as functions that do nothing: the audio device (miniaudio; the game's sound is
// still made by 989snd every vsync, the IOP's state the game reads back included, only not played yet). Host code.
#include <cstdint>

bool audio_out_start(int) { return false; }
void audio_out_push(const int16_t*, const int16_t*, int) {}
void audio_out_set_channels(int) {}
void audio_out_set_alone(bool) {}
void audio_out_step(bool) {}
void audio_out_stop() {}
int audio_out_channels() { return 2; }
uint64_t audio_out_alone_total() { return 0; }

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
