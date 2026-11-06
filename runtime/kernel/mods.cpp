// See mods.h.
#include "mods.h"

#include <platform/platform.h>

#include <json.hpp>  // (nlohmann::json, bundled with TinyGLTF)

#include <algorithm>
#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <map>
#include <mutex>
#include <sstream>

namespace rac1 {

namespace {
std::vector<ModInfo> g_mods;  // found, in load order
bool g_started = false;
bool g_headless = false;
bool g_read_prefs = false;  // the preferences are the player's (a window, or RAC1_MODS_FILE)
std::mutex g_lock;          // (the disc's files: the game's thread and the audio thread)

// orig: (none) — enhancement: the preferences file: $RAC1_MODS_FILE, else <config>/mods.txt
std::filesystem::path prefs_path() {
    if (const char* f = std::getenv("RAC1_MODS_FILE")) return f;
    if (const std::filesystem::path d = platform::config_dir(); !d.empty()) return d / "mods.txt";
    return "mods.txt";
}

// orig: (none) — enhancement: the player's choices ("<folder> on|off" per line; '#' a comment)
std::map<std::string, bool> prefs_read() {
    std::map<std::string, bool> out;
    std::ifstream f(prefs_path());
    std::string line;
    while (std::getline(f, line)) {
        if (line.empty() || line[0] == '#') continue;
        const size_t sp = line.find_last_of(' ');
        if (sp == std::string::npos || sp == 0) continue;
        std::string v = line.substr(sp + 1);
        while (!v.empty() && (v.back() == '\r' || v.back() == ' ')) v.pop_back();
        if (v == "on" || v == "off") out[line.substr(0, sp)] = v == "on";
    }
    return out;
}

// orig: (none) — enhancement: the choices written (every mod found; the lines of mods not found kept)
void prefs_write() {
    std::map<std::string, bool> all = prefs_read();
    for (const ModInfo& m : g_mods) all[m.folder] = m.enabled;
    std::string text = "# rac1-native: the mods on and off (main menu > Options > Mods); a mod not listed is on\n";
    for (const auto& [k, v] : all) text += k + (v ? " on\n" : " off\n");
    const std::filesystem::path path = prefs_path();
    std::error_code ec;
    if (path.has_parent_path()) std::filesystem::create_directories(path.parent_path(), ec);
    if (!platform::write_file_replace(path, text)) std::fprintf(stderr, "[mods] cannot write %s\n", path.string().c_str());
}

// orig: (none) — enhancement: a manifest's text field (absent or not a string: `def`), cut to `max` characters
std::string field(const nlohmann::json& j, const char* key, const std::string& def, size_t max) {
    const auto it = j.find(key);
    if (it == j.end() || !it->is_string()) return def;
    std::string s = it->get<std::string>();
    if (s.size() > max) s.resize(max);
    return s;
}

// orig: (none) — enhancement: the files under `dir` (recursively), by their path relative to it ('/' separators)
std::vector<std::pair<std::string, std::filesystem::path>> files_under(const std::filesystem::path& dir) {
    std::vector<std::pair<std::string, std::filesystem::path>> out;
    std::error_code ec;
    if (!std::filesystem::is_directory(dir, ec)) return out;
    for (auto it = std::filesystem::recursive_directory_iterator(dir, std::filesystem::directory_options::skip_permission_denied, ec);
         !ec && it != std::filesystem::recursive_directory_iterator(); it.increment(ec)) {
        if (!it->is_regular_file(ec)) continue;
        out.emplace_back(std::filesystem::relative(it->path(), dir, ec).generic_string(), it->path());
    }
    std::sort(out.begin(), out.end());
    return out;
}

// orig: (none) — enhancement: mods/<folder>/mod.json read into `m`; false (m.error said) when it cannot be
bool manifest_read(ModInfo& m) {
    std::ifstream f(m.dir / "mod.json", std::ios::binary);
    if (!f) {
        m.error = "no mod.json";
        return false;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    const nlohmann::json j = nlohmann::json::parse(ss.str(), nullptr, false);
    if (j.is_discarded() || !j.is_object()) {
        m.error = "mod.json is not a JSON object";
        return false;
    }
    m.name = field(j, "name", m.folder, 40);
    m.author = field(j, "author", "", 40);
    m.version = field(j, "version", "", 16);
    m.description = field(j, "description", "", 200);
    const auto p = j.find("priority");
    if (p != j.end() && p->is_number_integer()) m.priority = static_cast<int>(std::clamp<int64_t>(p->get<int64_t>(), -1000000, 1000000));
    return true;
}

// The disc files the mods on give: the layout's path -> the winner's file and the others' (for the size check)
struct DiscEntry {
    std::vector<std::pair<const ModInfo*, std::filesystem::path>> by_order;  // the load order: the first wins
};
std::map<std::string, DiscEntry>* g_disc = nullptr;

// orig: (none) — enhancement: the mods' disc files indexed (once)
std::map<std::string, DiscEntry>& disc_index() {
    if (g_disc != nullptr) return *g_disc;
    g_disc = new std::map<std::string, DiscEntry>();
    for (const ModInfo& m : g_mods) {
        if (!m.active) continue;
        for (auto& [rel, path] : files_under(m.dir / "disc")) (*g_disc)[rel].by_order.emplace_back(&m, path);
    }
    return *g_disc;
}
}  // namespace

// orig: (none) — enhancement (mods.h)
std::filesystem::path mods_dir() {
    if (const char* d = std::getenv("RAC1_MODS_DIR"); d != nullptr && *d != 0) return d;
    return "mods";
}

// orig: (none) — enhancement (mods.h)
void mods_start(bool headless) {
    std::scoped_lock lock(g_lock);
    if (g_started) return;
    g_started = true;
    g_headless = headless;
    const char* env = std::getenv("RAC1_MODS");
    if (env != nullptr && std::strcmp(env, "0") == 0) {
        std::printf("[mods] RAC1_MODS=0: every mod off\n");
        return;
    }
    if (headless && (env == nullptr || std::strcmp(env, "1") != 0)) return;  // (a test run: vanilla)
    g_read_prefs = !headless || std::getenv("RAC1_MODS_FILE") != nullptr;
    const std::filesystem::path root = mods_dir();
    std::error_code ec;
    if (!std::filesystem::is_directory(root, ec)) return;
    const std::map<std::string, bool> prefs = g_read_prefs ? prefs_read() : std::map<std::string, bool>{};
    for (const auto& e : std::filesystem::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        ModInfo m;
        m.folder = e.path().filename().string();
        if (m.folder.empty() || m.folder[0] == '.' || m.folder[0] == '_') continue;  // (hidden, or ours: _dump)
        m.dir = e.path();
        if (!manifest_read(m)) {
            std::fprintf(stderr, "[mods] %s: %s, not loaded\n", m.folder.c_str(), m.error.c_str());
            m.name = m.folder;
            m.enabled = false;
        } else {
            const auto it = prefs.find(m.folder);
            m.enabled = it == prefs.end() || it->second;
        }
        m.active = m.enabled && m.error.empty();
        m.disc_files = files_under(m.dir / "disc").size();
        m.textures = 0;
        for (const auto& [rel, path] : files_under(m.dir / "textures")) {
            std::string ext = path.extension().string();
            for (char& c : ext) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
            if (ext == ".png") ++m.textures;
        }
        g_mods.push_back(std::move(m));
    }
    std::sort(g_mods.begin(), g_mods.end(), [](const ModInfo& a, const ModInfo& b) {
        if (a.priority != b.priority) return a.priority > b.priority;
        return a.folder < b.folder;
    });
    for (const ModInfo& m : g_mods)
        std::printf("[mods] %s: \"%s\" %s by %s, priority %d, %zu disc files, %zu textures: %s\n", m.folder.c_str(), m.name.c_str(),
                    m.version.empty() ? "-" : m.version.c_str(), m.author.empty() ? "-" : m.author.c_str(), m.priority, m.disc_files, m.textures,
                    m.active ? "on" : (m.error.empty() ? "off" : "not loaded"));
}

// orig: (none) — enhancement (mods.h)
bool mods_on() {
    for (const ModInfo& m : g_mods)
        if (m.active) return true;
    return false;
}

// orig: (none) — enhancement (mods.h)
const std::vector<ModInfo>& mods_list() { return g_mods; }

// orig: (none) — enhancement (mods.h)
std::vector<const ModInfo*> mods_active() {
    std::vector<const ModInfo*> out;
    for (const ModInfo& m : g_mods)
        if (m.active) out.push_back(&m);
    return out;
}

// orig: (none) — enhancement (mods.h)
void mods_set_enabled(size_t i, bool on) {
    if (i >= g_mods.size() || !g_mods[i].error.empty()) return;
    g_mods[i].enabled = on;
    std::printf("[mods] %s: %s at the next start\n", g_mods[i].folder.c_str(), on ? "on" : "off");
    if (g_read_prefs) prefs_write();
}

// orig: (none) — enhancement (mods.h)
bool mods_restart_pending() {
    for (const ModInfo& m : g_mods)
        if (m.error.empty() && m.enabled != m.active) return true;
    return false;
}

// orig: (none) — enhancement (mods.h)
std::filesystem::path mods_disc_file(const std::string& rel, uint64_t size) {
    std::scoped_lock lock(g_lock);
    if (!mods_on()) return {};
    const auto& index = disc_index();
    const auto it = index.find(rel);
    if (it == index.end()) return {};
    std::filesystem::path chosen;
    const ModInfo* winner = nullptr;
    for (const auto& [m, path] : it->second.by_order) {
        std::error_code ec;
        const uint64_t have = std::filesystem::file_size(path, ec);
        if (ec || have != size) {
            std::fprintf(stderr, "[mods] %s: %s's file is %llu bytes, the disc's %llu: not used (a mod's disc file keeps the original's size)\n",
                         rel.c_str(), m->folder.c_str(), static_cast<unsigned long long>(ec ? 0 : have), static_cast<unsigned long long>(size));
            continue;
        }
        if (winner == nullptr) {
            winner = m;
            chosen = path;
        } else {
            std::fprintf(stderr, "[mods] conflict: %s from %s (priority %d) over %s (priority %d)\n", rel.c_str(), winner->folder.c_str(), winner->priority,
                         m->folder.c_str(), m->priority);
        }
    }
    if (winner != nullptr) std::printf("[mods] disc file %s from %s\n", rel.c_str(), winner->folder.c_str());
    return chosen;
}

// orig: (none) — enhancement (mods.h)
std::vector<std::pair<std::string, std::filesystem::path>> mods_files(const char* sub) {
    std::vector<std::pair<std::string, std::filesystem::path>> out;
    std::map<std::string, const ModInfo*> winner;
    for (const ModInfo* m : mods_active())
        for (auto& [rel, path] : files_under(m->dir / sub)) {
            const auto [it, fresh] = winner.emplace(rel, m);
            if (!fresh) {
                std::fprintf(stderr, "[mods] conflict: %s/%s from %s (priority %d) over %s (priority %d)\n", sub, rel.c_str(), it->second->folder.c_str(),
                             it->second->priority, m->folder.c_str(), m->priority);
                continue;
            }
            out.emplace_back(rel, path);
        }
    return out;
}

}  // namespace rac1
