// The GPU row (Options > Display > GPU, the debug menu's GPU section) and the restart it asks for: the names shown, and the
// calls between the host (orig_main.cpp), the menus (src/game/enhance/graphics.cpp) and the debug menu (src/_devtools).
// Not a port. Kept out of src/game/*.h on purpose: those headers are the generated code's fingerprint (tools/setup.py
// CODE_SOURCES), and a change to one makes every player's setup generate the code again.
#ifndef RAC1_PLATFORM_GPU_SELECT_H
#define RAC1_PLATFORM_GPU_SELECT_H

#include <cctype>
#include <cstdint>
#include <string>
#include <vector>

namespace rac1 {

// A Vulkan device as the host lists it (runtime/vk/vk_context.h GpuInfo): the GPU row's values
struct GraphicsGpu {
    std::string name;  // the driver's full name
    std::string uuid;  // VkPhysicalDeviceIDProperties::deviceUUID, "xxxxxxxx-xxxx-xxxx-xxxx-xxxxxxxxxxxx"
};

// orig: (none) — enhancement: a device's name for the row, by general rules (no table of models): what is in parentheses or brackets
// dropped ("(RADV GFX1150)", "(R)", "(TM)", "(LLVM 19.1.7, 256 bits)") - unless the group starts with a vendor word, as a layer's
// "Microsoft Direct3D12 (NVIDIA GeForce RTX 3060)": then the group is the name; "Corporation" dropped; vendor words dropped
// from the front while more text follows (NVIDIA, GeForce, AMD, ATI, Intel, Apple, Qualcomm, Microsoft); a trailing "Graphics"
// dropped when two words or more stay; the spaces collapsed; a result under 3 characters: the full name
inline std::string gpu_short_name(const std::string& full) {
    auto lower = [](std::string s) {
        for (char& c : s) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
        return s;
    };
    auto vendor = [&](const std::string& w) {
        const std::string l = lower(w);
        return l == "nvidia" || l == "geforce" || l == "amd" || l == "ati" || l == "intel" || l == "apple" || l == "qualcomm" || l == "microsoft";
    };
    auto words = [](const std::string& s) {
        std::vector<std::string> out;
        std::string cur;
        for (char c : s) {
            if (c == ' ' || c == '\t') {
                if (!cur.empty()) out.push_back(cur);
                cur.clear();
            } else {
                cur += c;
            }
        }
        if (!cur.empty()) out.push_back(cur);
        return out;
    };
    // 1. the groups: dropped, or (one starting with a vendor word) taken as the name
    std::string outside, inside_vendor;
    for (size_t i = 0; i < full.size();) {
        const char c = full[i];
        if (c == '(' || c == '[') {
            const char close = c == '(' ? ')' : ']';
            size_t j = i + 1;
            int depth = 1;
            while (j < full.size() && depth > 0) {
                if (full[j] == c) ++depth;
                else if (full[j] == close) --depth;
                ++j;
            }
            const std::string group = full.substr(i + 1, (depth == 0 ? j - 1 : j) - (i + 1));
            const std::vector<std::string> gw = words(group);
            if (inside_vendor.empty() && !gw.empty() && vendor(gw[0])) inside_vendor = group;
            i = j;
            continue;
        }
        outside += c;
        ++i;
    }
    std::vector<std::string> w = words(inside_vendor.empty() ? outside : inside_vendor);
    // 2. the words: "Corporation" dropped, the vendors in front, "Graphics" behind
    for (size_t i = 0; i < w.size();) {
        if (lower(w[i]) == "corporation" && w.size() > 1) w.erase(w.begin() + static_cast<long>(i));
        else ++i;
    }
    while (w.size() > 1 && vendor(w[0])) w.erase(w.begin());
    if (w.size() > 2 && lower(w.back()) == "graphics") w.pop_back();
    std::string out;
    for (const std::string& x : w) out += (out.empty() ? "" : " ") + x;
    return out.size() < 3 ? full : out;
}

// orig: (none) — enhancement: `s` cut to `max` characters with "..." (the font has no ellipsis character)
inline std::string gpu_clip(const std::string& s, size_t max) {
    if (s.size() <= max || max < 4) return s;
    std::string out = s.substr(0, max - 3);
    while (!out.empty() && out.back() == ' ') out.pop_back();
    return out + "...";
}

// --- the host's side (orig_main.cpp) ---
// orig: (none) — enhancement: the host's function that lists the machine's Vulkan devices, called once, when the GPU row (or the
// debug menu) first needs them: the OpenGL renderer may be the one running, so the host lists them with an instance of its own
void graphics_set_gpu_lister(std::vector<GraphicsGpu> (*fn)());
// orig: (none) — enhancement: the GPU the player chose, by name and UUID (both empty: Automatic), for the renderer's start
const std::string& graphics_gpu_name();
const std::string& graphics_gpu_uuid();
// orig: (none) — enhancement: the renderer running and its device's full name (the host, once, after the renderer's start;
// `name` empty with OpenGL: the GL_RENDERER string): the help's and the debug menu's "In use"
void graphics_set_gpu_running(const std::string& name, bool vulkan);
const std::string& graphics_gpu_running();
bool graphics_gpu_running_vulkan();

// --- the debug menu's side (src/_devtools/dev_menu.cpp) ---
// orig: (none) — enhancement: the Vulkan devices (listed once) and the GPU chosen among them (-1: Automatic, -2: a saved choice
// that is not connected); the choice made as the Options row does (kept in the preferences, from the next start)
std::vector<GraphicsGpu> graphics_gpu_list();
int graphics_gpu_selected();
void graphics_gpu_choose(int index);

// --- the restart ---
// orig: (none) — enhancement: the game restarted to use a change of the GPU or the renderer. The Options rows ask in a popup
// (Restart now / Later); Restart now (or the debug menu's button) asks the host, which closes the game as the window's close
// does and starts it again with the same arguments and environment (host_restart.cpp), never in a headless run
void graphics_request_restart();
// the host's loop: true once when the restart was asked
bool graphics_take_restart();
// the host: the restart could not be done (it is logged); the rows' help then says to restart by hand
void graphics_restart_failed();

}  // namespace rac1

#endif  // RAC1_PLATFORM_GPU_SELECT_H
