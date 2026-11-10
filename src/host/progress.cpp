// See progress.h.
#include "progress.h"

#include <platform/platform.h>

#include <algorithm>
#include <chrono>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <ctime>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <vector>

namespace rac1::progress {
namespace {

// Each level's planet (the disc's level numbers): {id, name, counted in the percentage}. The story's planets
// count, Veldin once for its two levels.
struct Planet {
    const char* id;
    const char* name;
};
constexpr Planet kLevels[19] = {
    {"veldin", "Veldin"},     {"novalis", "Novalis"},   {"aridia", "Aridia"},      {"kerwan", "Kerwan"},
    {"eudora", "Eudora"},     {"rilgar", "Rilgar"},     {"blarg", "Blarg Station"}, {"umbris", "Umbris"},
    {"batalia", "Batalia"},   {"gaspar", "Gaspar"},     {"orxon", "Orxon"},        {"pokitaru", "Pokitaru"},
    {"hoven", "Hoven"},       {"gemlik", "Gemlik Base"}, {"oltanis", "Oltanis"},    {"quartu", "Quartu"},
    {"kalebo", "Kalebo III"}, {"drek_fleet", "Drek's Fleet"}, {"veldin", "Veldin"},
};
constexpr int kPlanets = 18;  // the distinct ids above
constexpr double kWriteEvery = 60.0;  // seconds

using Clock = std::chrono::steady_clock;

struct State {
    bool on = false;
    std::string path;
    bool title = true;  // the title ran last
    int level = -1;
    std::vector<std::string> visited;
    double played = 0.0;  // seconds in levels
    double unwritten = 0.0;
    Clock::time_point last{};
};
State g;

bool has(const std::string& id) {
    return std::find(g.visited.begin(), g.visited.end(), id) != g.visited.end();
}

// The two lists kept from the last run: "visited" and "playtime_s" (the file is the one written below).
void load() {
    std::FILE* f = std::fopen(g.path.c_str(), "rb");
    if (f == nullptr) return;
    std::string s;
    char buf[4096];
    for (std::size_t n; (n = std::fread(buf, 1, sizeof buf, f)) > 0;) s.append(buf, n);
    std::fclose(f);
    if (std::size_t v = s.find("\"visited\""); v != std::string::npos) {
        const std::size_t open = s.find('[', v), close = s.find(']', v);
        for (std::size_t i = open; open != std::string::npos && close != std::string::npos && i < close;) {
            const std::size_t a = s.find('"', i + 1);
            if (a == std::string::npos || a > close) break;
            const std::size_t b = s.find('"', a + 1);
            if (b == std::string::npos || b > close) break;
            const std::string id = s.substr(a + 1, b - a - 1);
            if (!id.empty() && !has(id)) g.visited.push_back(id);
            i = b;
        }
    }
    if (std::size_t t = s.find("\"playtime_s\""); t != std::string::npos)
        if (std::size_t c = s.find(':', t); c != std::string::npos) g.played = std::strtod(s.c_str() + c + 1, nullptr);
}

int percent() {
    int n = 0;
    for (int i = 0; i < 19; ++i) {
        bool first = true;  // (Veldin's second level is the same planet)
        for (int j = 0; j < i; ++j) first = first && std::strcmp(kLevels[j].id, kLevels[i].id) != 0;
        if (first && has(kLevels[i].id)) ++n;
    }
    return n * 100 / kPlanets;
}

// Written to a temporary file and renamed over the old one: the launcher never reads half a file.
void write_file(const std::string& path, const std::string& text) {
    std::error_code ec;
    std::filesystem::create_directories(std::filesystem::path(path).parent_path(), ec);
    const std::string tmp = path + ".tmp";
    std::FILE* f = std::fopen(tmp.c_str(), "wb");
    if (f == nullptr) return;
    const bool ok = std::fwrite(text.data(), 1, text.size(), f) == text.size();
    if (std::fclose(f) != 0 || !ok) return;
    std::filesystem::rename(tmp, path, ec);
}

// The file's writer: one thread, started at the first write, that writes the last text handed to it (a text not yet
// written when the next comes is dropped: the file holds the latest).
struct Writer {
    std::mutex m;
    std::condition_variable cv;
    std::string path, text;
    bool pending = false, busy = false, started = false;
} g_writer;
void writer_post(const std::string& path, std::string&& text) {
    Writer& w = g_writer;
    std::lock_guard<std::mutex> lk(w.m);
    w.path = path;
    w.text = std::move(text);
    w.pending = true;
    if (!w.started) {
        w.started = true;
        std::thread([&w] {
            std::unique_lock<std::mutex> lk(w.m);
            for (;;) {
                w.cv.wait(lk, [&w] { return w.pending; });
                const std::string path = std::move(w.path), text = std::move(w.text);
                w.pending = false;
                w.busy = true;
                lk.unlock();
                write_file(path, text);
                lk.lock();
                w.busy = false;
                w.cv.notify_all();
            }
        }).detach();  // (the process ends with std::_Exit: finish() waits for the last write)
    }
    w.cv.notify_all();
}
// the writer's last text on the disc
void writer_wait() {
    Writer& w = g_writer;
    std::unique_lock<std::mutex> lk(w.m);
    w.cv.wait(lk, [&w] { return !w.pending && !w.busy; });
}

void write(bool now_sync = false) {
    g.unwritten = 0.0;
    std::string s = "{";
    if (g.level >= 0)
        s += "\"planet\": \"" + std::string(kLevels[g.level].id) + "\", \"planet_name\": \"" + kLevels[g.level].name +
             "\", \"level\": " + std::to_string(g.level) + ", ";
    s += "\"visited\": [";
    for (std::size_t i = 0; i < g.visited.size(); ++i) s += (i ? ", \"" : "\"") + g.visited[i] + "\"";
    s += "], \"percent\": " + std::to_string(percent()) + ", \"playtime_s\": " + std::to_string(static_cast<long long>(g.played));
    char when[32];
    const std::time_t now = std::time(nullptr);
    std::tm utc{};
#ifdef _WIN32
    gmtime_s(&utc, &now);
#else
    gmtime_r(&now, &utc);
#endif
    std::strftime(when, sizeof when, "%Y-%m-%dT%H:%M:%SZ", &utc);
    s += ", \"updated\": \"" + std::string(when) + "\"}\n";
    // (the file's write off the caller's thread - the game's, once a minute and at each planet: a slow disc or a scanner made
    // that frame late. RAC1_PERF_PROGRESS_THREAD=0: written here, as before)
    static const bool threaded = [] {
        const char* e = std::getenv("RAC1_PERF_PROGRESS_THREAD");
        return e == nullptr || std::atoi(e) != 0;
    }();
    if (now_sync || !threaded) {
        writer_wait();
        write_file(g.path, s);
    } else {
        writer_post(g.path, std::move(s));
    }
}

}  // namespace

void start(bool headless) {
    std::string path;
    if (const char* e = std::getenv("RAC1_PROGRESS"); e != nullptr) {
        if (std::strcmp(e, "0") != 0) path = e;
    } else if (!headless) {
        if (const std::filesystem::path d = platform::data_dir(); !d.empty()) path = (d / "progress.json").string();
    }
    if (path.empty()) return;
    g = State{};
    g.on = true;
    g.path = path;
    load();
}

void finish() {
    if (g.on && g.level >= 0 && g.unwritten > 0.0) write(true);
    else if (g.on) writer_wait();
}

void frame(int level, bool title, bool exiting) {
    if (!g.on) return;
    const Clock::time_point now = Clock::now();
    const double dt = g.last == Clock::time_point{} ? 0.0 : std::chrono::duration<double>(now - g.last).count();
    g.last = now;
    if (title) {
        g.title = true;
        return;
    }
    if (exiting || level < 0 || level >= 19) return;
    if (level != g.level || g.title) {
        if (g.title && level == 0) {  // a new game
            g.visited.clear();
            g.played = 0.0;
        }
        const bool changed = level != g.level || g.title;
        g.title = false;
        g.level = level;
        if (!has(kLevels[level].id)) g.visited.emplace_back(kLevels[level].id);
        if (changed) write();
    }
    // (a frame held long - a load, the window dragged, the computer asleep - counts a quarter second at most)
    const double step = std::min(dt, 0.25);
    g.played += step;
    g.unwritten += step;
    if (g.unwritten >= kWriteEvery) write();
}

}  // namespace rac1::progress
