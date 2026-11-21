// The run-time data's root folder (asset_paths.h).
#include "asset_paths.h"

#include <platform/platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <mutex>

namespace rac1::io {

namespace {

namespace fs = std::filesystem;

struct Root {
    std::string path = "assets";  // (the default: relative to the current directory, as before)
    std::string source = "default";
};
Root g_root;
std::once_flag g_decided;

// orig: (host) a folder as written (an environment variable, a line of the pointer file): the end of line, blanks and
// trailing separators dropped ("/" and a drive's root "C:\" kept)
std::string trimmed(std::string s) {
    while (!s.empty() && (s.back() == '\r' || s.back() == '\n' || s.back() == ' ' || s.back() == '\t')) s.pop_back();
    size_t b = 0;
    while (b < s.size() && (s[b] == ' ' || s[b] == '\t')) ++b;
    s.erase(0, b);
    while (s.size() > 1 && (s.back() == '/' || s.back() == '\\') && s[s.size() - 2] != ':') s.pop_back();
    return s;
}

// orig: (host) `p` absolute: as it is when it already is, else against `base` (the current directory when empty)
std::string absolute_of(const std::string& p, const fs::path& base) {
    const fs::path q(p);
    if (q.is_absolute()) return trimmed(p);
    std::error_code ec;
    const fs::path a = base.empty() ? fs::absolute(q, ec) : base / q;
    return ec ? trimmed(p) : trimmed(a.lexically_normal().string());
}

// orig: (host) the root, once: RAC1_ASSETS, else the pointer file, else "assets" (asset_paths.h)
void decide() {
    if (const char* e = std::getenv("RAC1_ASSETS"); e != nullptr && trimmed(e) != "") {
        g_root = {absolute_of(trimmed(e), {}), "RAC1_ASSETS"};
    } else if (const std::string pf = asset_root_pointer_file(); !pf.empty()) {
        std::ifstream f(pf, std::ios::binary);
        std::string line;
        for (bool first = true; f && std::getline(f, line); first = false) {
            if (first && line.rfind("\xef\xbb\xbf", 0) == 0) line.erase(0, 3);  // (a UTF-8 mark, from a Windows editor)
            line = trimmed(line);
            if (line.empty() || line[0] == '#') continue;
            g_root = {absolute_of(line, fs::path(pf).parent_path()), pf};
            break;
        }
    }
    if (g_root.source != "default") std::printf("[assets] the game's data: %s (%s)\n", g_root.path.c_str(), g_root.source.c_str());
}

}  // namespace

const std::string& asset_root() {
    std::call_once(g_decided, decide);
    return g_root.path;
}

const std::string& asset_root_source() {
    std::call_once(g_decided, decide);
    return g_root.source;
}

std::string asset_path(const char* rel) {
    const std::string& root = asset_root();
    std::string out;
    out.reserve(root.size() + 1 + std::strlen(rel));
    out += root;
    out += '/';
    out += rel;
    return out;
}

std::string asset_path(const std::string& rel) { return asset_path(rel.c_str()); }

bool asset_root_set(const std::string& dir, const char* source) {
    const std::string want = absolute_of(trimmed(dir), {});
    bool applied = false;
    std::call_once(g_decided, [&] {
        g_root = {want, source};
        applied = true;
        std::printf("[assets] the game's data: %s (%s)\n", g_root.path.c_str(), g_root.source.c_str());
    });
    if (applied || g_root.path == want) return true;
    std::fprintf(stderr, "[assets] %s %s comes too late: the data's folder is already %s (%s)\n", source, want.c_str(),
                 g_root.path.c_str(), g_root.source.c_str());
    return false;
}

std::string asset_root_pointer_file() {
    const fs::path exe = platform::executable_path();
    return exe.empty() ? std::string() : (exe.parent_path() / "assets.txt").string();
}

bool asset_root_remember(const std::string& dir, std::string& why) {
    const std::string pf = asset_root_pointer_file();
    if (pf.empty()) {
        why = "the executable's folder cannot be found";
        return false;
    }
    const std::string tmp = pf + ".tmp";
    {
        std::ofstream f(tmp, std::ios::binary | std::ios::trunc);
        f << "# the game's data (rac1-native --setup --output-dir <dir> --remember; RAC1_ASSETS overrides it)\n"
          << absolute_of(trimmed(dir), {}) << "\n";
        if (!f.flush()) {
            why = "cannot write " + tmp;
            return false;
        }
    }
    std::error_code ec;
    fs::rename(tmp, pf, ec);
    if (ec) {
        why = "cannot write " + pf + ": " + ec.message();
        fs::remove(tmp, ec);
        return false;
    }
    return true;
}

bool asset_root_forget(std::string& why) {
    const std::string pf = asset_root_pointer_file();
    if (pf.empty()) return true;
    std::error_code ec;
    fs::remove(pf, ec);
    if (ec) why = "cannot remove " + pf + ": " + ec.message();
    return !ec;
}

}  // namespace rac1::io
