#pragma once

#include <host/host_input.h>

#include <mutex>
#include <string>

struct ANativeWindow;
struct GLFWgamepadstate;

namespace rac1::android_host {

// Current ANativeWindow (nullptr while suspended/destroyed).
ANativeWindow* native_window();

// Current drawable surface pixel dimensions (last valid size if paused).
void window_size(int* w, int* h);

// Display refresh rate in Hz (queried via JNI Display.getRefreshRate, fallback 60).
int refresh_hz();

// True once activity teardown was requested by Android or the game.
bool should_close();

// Asks NativeActivity to finish and exit back to Android.
void request_close();

// Tracks whether Vulkan has been initialized and owns a swapchain/surface.
void set_vulkan_active(bool active);

// Serializes Vulkan frame work against ANativeWindow teardown/resume on Android.
// Recursive: the game thread holds it for the whole frame and some frame work (e.g. the pause
// menu's copy of the last picture) takes it again.
std::recursive_mutex& render_mutex();

// Waits out background/surface loss, resumes Vulkan surface if recreated, and updates window dimensions.
bool poll_events();

// Fills host input state (controller + keyboard + mouse) and reports active gamepad metadata.
void fill_host_state(rac1::host_input::HostState& st, std::string& pad_name, std::string& pad_guid);

// Reads key and primary gamepad state for dev_menu.
bool is_key_down(int glfw_key);
bool get_primary_gamepad(GLFWgamepadstate& out);

// Drains pending touch/mouse/key events into Dear ImGui and refreshes ImGui_ImplAndroid's window pointer on resume.
void flush_imgui_frame();

// Ensures ImGui's Android backend tracks the current ANativeWindow before shutdown if already recreated.
void sync_imgui_window();

}  // namespace rac1::android_host
