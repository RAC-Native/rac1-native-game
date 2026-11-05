// See vk_safety.h.
#include "vk_safety.h"

#include <platform/platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace rac1::vk_safety {
namespace {

constexpr unsigned kSafeFrames = 600;  // (10 seconds at 60: past the pipelines' making and the first level's load)

bool g_on = false;
bool g_pending = false;  // (the start marker is ours, still there)
unsigned g_frames = 0;

// orig: (none) — host: a marker's path ("" when there is no settings folder)
std::filesystem::path marker(const char* name) {
    const std::filesystem::path d = platform::config_dir();
    return d.empty() ? std::filesystem::path() : d / name;
}

// orig: (none) — host: a marker's first line ("" when there is none)
bool read_marker(const char* name, std::string* text) {
    const std::filesystem::path p = marker(name);
    if (p.empty()) return false;
    std::FILE* f = platform::open_file(p, "r");
    if (f == nullptr) return false;
    char line[256] = {};
    if (std::fgets(line, sizeof line, f) != nullptr) {
        line[std::strcspn(line, "\r\n")] = 0;
        *text = line;
    }
    std::fclose(f);
    return true;
}

// orig: (none) — host
void write_marker(const char* name, const std::string& text) {
    const std::filesystem::path p = marker(name);
    if (p.empty()) return;
    std::error_code ec;
    std::filesystem::create_directories(p.parent_path(), ec);
    if (std::FILE* f = platform::open_file(p, "w"); f != nullptr) {
        std::fprintf(f, "%s\n", text.c_str());
        std::fflush(f);
        std::fclose(f);
    }
}

// orig: (none) — host
void remove_marker(const char* name) {
    const std::filesystem::path p = marker(name);
    if (p.empty()) return;
    std::error_code ec;
    std::filesystem::remove(p, ec);
}

}  // namespace

// orig: (none) — host
void start(bool headless) {
#if defined(__APPLE__)
    (void)headless;
    g_on = false;
#else
    const char* e = std::getenv("RAC1_VK_MARKERS");
    g_on = !headless || (e != nullptr && std::atoi(e) != 0);
#endif
}

// orig: (none) — host
Marker found(std::string* why) {
    if (!g_on) return Marker::kNone;
    std::string text;
    if (read_marker("vk_failed", &text)) {
        *why = "the graphics driver stopped the GPU during the last Vulkan run (VK_ERROR_DEVICE_LOST" + (text.empty() ? std::string() : ", at " + text) + ")";
        return Marker::kDeviceLost;
    }
    if (read_marker("vk_start_pending", &text)) {
        *why = "the last Vulkan start did not finish (a crash in its first frames)";
        return Marker::kStartFailed;
    }
    return Marker::kNone;
}

// orig: (none) — host
void starting() {
    if (!g_on) return;
    write_marker("vk_start_pending", "a Vulkan start: removed after its first 600 frames or when the game closes");
    g_pending = true;
    g_frames = 0;
}

// orig: (none) — host
void frame_shown() {
    if (!g_pending) return;
    ++g_frames;
    if (g_frames == 120) {
        const char* e = std::getenv("RAC1_VK_FAIL");
        if (e != nullptr && std::strstr(e, "crash") != nullptr) {
            std::fprintf(stderr, "[vk] RAC1_VK_FAIL=crash: the game ended at the 120th Vulkan frame, as by a crash (a test)\n");
            std::fflush(stdout);
            std::_Exit(70);
        }
    }
    if (g_frames >= kSafeFrames) start_over();
}

// orig: (none) — host
void start_over() {
    if (!g_pending) return;
    remove_marker("vk_start_pending");
    g_pending = false;
}

// orig: (none) — host
void device_lost(const char* where) {
    if (!g_on) return;
    write_marker("vk_failed", where != nullptr ? where : "");
}

// orig: (none) — host
void cleared() {
    if (!g_on) return;
    remove_marker("vk_failed");
    if (!g_pending) remove_marker("vk_start_pending");  // (this start's own stays until its frames are shown)
}

}  // namespace rac1::vk_safety
