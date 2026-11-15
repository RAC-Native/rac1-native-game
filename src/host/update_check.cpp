// See update_check.h. The check itself is tools/update_check.py (Python's standard library: the game gains
// no HTTP or TLS library), run with platform::run_quiet in a thread of its own so that start-up never
// waits for the network. The helper writes its answer to <config dir>/update_check.txt and keeps it for
// a few hours (a start within them asks GitHub nothing); any failure (no network, an error, the rate
// limit, no Python) leaves nothing to show, and nothing is shown.
#include <host/update_check.h>

#include <game/enhance/advanced.h>
#include <game/enhance/update_notice.h>
#include <platform/platform.h>

#include <rac1_version.h>  // (made at each build: cmake/version.cmake)

#include <atomic>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <map>
#include <memory>
#include <string>
#include <thread>
#include <vector>

namespace rac1 {
bool update_notice_update_now(bool* development = nullptr);
bool update_notice_take_auto_check();
bool update_notice_take_manual_check();
void update_notice_set_status(int status, const std::string& installed = "");
void update_notice_offer_manual(const UpdateOffer& offer);
}

namespace rac1::update_check {

namespace {
namespace fs = std::filesystem;

// The check's state, shared with its thread (which may outlive main: detached)
struct Check {
    std::atomic<bool> done{false};
    int exit_code = -1;
    fs::path result;
    bool development = false;
    bool manual = false;
    std::string installed;        // (stable) the version, (development) the commit or v<version>
    std::string installed_short;  // for display in the Updates page
};
std::shared_ptr<Check> g_check;
bool g_handled = false;
bool g_headless = false;
AdvancedUpdatePrefs g_last_prefs;
std::string g_skipped;  // "Skip Version"'s choice, as at start

// orig: (none) — host: true when the build is not on a developer branch other than main and dev (main, dev, a release
// tag's detached HEAD, or a zip download can all check and switch to the development channel: the dev branch, #141)
bool can_check_dev() {
    return *RAC1_GIT_BRANCH == 0 || std::strcmp(RAC1_GIT_BRANCH, "main") == 0 || std::strcmp(RAC1_GIT_BRANCH, "dev") == 0 ||
           std::strcmp(RAC1_GIT_BRANCH, "HEAD") == 0;
}

// orig: (none) — host: short installed version/commit string for the Updates page help text
std::string installed_display(bool dev) {
    if (dev) return std::strlen(RAC1_GIT_COMMIT) >= 7 ? std::string(RAC1_GIT_COMMIT).substr(0, 7) : std::string(RAC1_VERSION);
    if (*RAC1_GIT_DESCRIBE != 0) return std::string(RAC1_GIT_DESCRIBE);
    return std::string(RAC1_VERSION);
}

// orig: (none) — host: the repository's root: above build/ holding this program, else the working directory
fs::path repo_root() {
    const fs::path exe = platform::executable_path();
    if (!exe.empty()) {
        const fs::path r = exe.parent_path().parent_path();
        if (fs::exists(r / "tools/setup.py")) return r;
    }
    return fs::current_path();
}

// orig: (none) — host: the helper's path (tools/update_check.py)
fs::path helper_path() { return repo_root() / "tools/update_check.py"; }

// orig: (none) — host: the Python to run it with: the build's own (the setup's, on Windows), else the platform's
std::string python() {
    std::error_code ec;
    if (const char* p = RAC1_BUILD_PYTHON; *p != 0 && fs::exists(p, ec)) return p;
    return platform::python_program();
}

// orig: (none) — host: a result file's "key value" lines
std::map<std::string, std::string> read_result(const fs::path& path) {
    std::map<std::string, std::string> out;
    FILE* f = platform::open_file(path, "r");
    if (f == nullptr) return out;
    char buf[1024];
    while (std::fgets(buf, sizeof buf, f) != nullptr) {
        std::string line = buf;
        while (!line.empty() && (line.back() == '\n' || line.back() == '\r')) line.pop_back();
        const size_t sp = line.find(' ');
        if (sp != std::string::npos) out[line.substr(0, sp)] = line.substr(sp + 1);
    }
    std::fclose(f);
    return out;
}

// orig: (none) — host: the offer's common part
UpdateOffer offer_base() {
    UpdateOffer o;
    o.git = std::strlen(RAC1_GIT_COMMIT) == 40;
#ifdef _WIN32
    o.windows = true;
#endif
    return o;
}

// orig: (none) — testing: RAC1_UPDATE_FAKE=<x.y.z> (a release) or dev:<n> (n commits on the dev branch): a made-up offer
bool fake(const char* v, bool manual) {
    UpdateOffer o = offer_base();
    if (std::strncmp(v, "dev:", 4) == 0) {
        o.development = true;
        o.commits = std::atoi(v + 4) > 0 ? std::atoi(v + 4) : 1;
        o.id = "fake-dev-" + std::to_string(o.commits);
        o.installed = std::strlen(RAC1_GIT_COMMIT) >= 7 ? std::string(RAC1_GIT_COMMIT).substr(0, 7) : std::string(RAC1_VERSION);
    } else {
        o.version = v;
        o.id = "v" + o.version;
        o.installed = RAC1_VERSION;
        o.notes = "A test of the update notice (RAC1_UPDATE_FAKE): here would be the release's notes, in short.";
    }
    if (!manual && !g_skipped.empty() && g_skipped == o.id) {
        std::printf("[update] RAC1_UPDATE_FAKE: %s was skipped (update_skip in advanced_options.txt)\n", o.id.c_str());
        return true;
    }
    std::printf("[update] RAC1_UPDATE_FAKE: %s offered (no network)\n", v);
    if (manual) update_notice_offer_manual(o);
    else update_notice_offer(o);
    return true;
}

// orig: (none) — host: starts a background check for `prefs` (`force`: ask GitHub even when update_check.txt is recent)
void start_check(const AdvancedUpdatePrefs& prefs, bool force, bool manual) {
    auto c = std::make_shared<Check>();
    c->development = prefs.development && can_check_dev();
    c->manual = manual;
    if (prefs.development && !c->development)
        std::printf("[update] on branch %s: the releases are checked\n", RAC1_GIT_BRANCH);
    if (c->development) {
        c->installed = std::strlen(RAC1_GIT_COMMIT) == 40 ? std::string(RAC1_GIT_COMMIT) : (std::string("v") + RAC1_VERSION);
    } else {
        c->installed = std::string(RAC1_VERSION);
    }
    c->installed_short = installed_display(c->development);
    const fs::path dir = platform::config_dir();
    c->result = (dir.empty() ? fs::path(".") : dir) / "update_check.txt";
    std::vector<std::string> args = {python(), helper_path().string(), "--out", c->result.string(), "--channel",
                                     c->development ? "development" : "stable"};
    if (c->development) {
        args.push_back("--commit");
        args.push_back(c->installed);
    } else {
        args.push_back("--installed");
        args.push_back(c->installed);
        if (*RAC1_GIT_DESCRIBE != 0) {
            args.push_back("--describe");
            args.push_back(RAC1_GIT_DESCRIBE);
        }
        if (*RAC1_GIT_BRANCH != 0) {
            args.push_back("--branch");
            args.push_back(RAC1_GIT_BRANCH);
        }
    }
    if (force) args.push_back("--force");
    g_check = c;
    g_handled = false;
    update_notice_set_status(1, c->installed_short);
    std::printf("[update] checking for a newer %s in the background (Updates > Update Check)\n",
                c->development ? "commit on the dev branch" : "release");
    std::thread([c, args] {
        c->exit_code = platform::run_quiet(args);
        c->done.store(true, std::memory_order_release);
    }).detach();
}
}  // namespace

// orig: (none) — host (update_check.h)
std::string version_text() {
    std::string s = RAC1_VERSION;
    if (*RAC1_GIT_DESCRIBE != 0) {
        s += std::string(" (") + RAC1_GIT_DESCRIBE;
        if (*RAC1_GIT_BRANCH != 0) s += std::string(", ") + RAC1_GIT_BRANCH;
        s += ")";
    }
    return s;
}

// orig: (none) — host (update_check.h)
void start(bool headless) {
    g_headless = headless;
    const AdvancedUpdatePrefs prefs = advanced_update_prefs();  // (headless: off, unless RAC1_ADVANCED)
    g_last_prefs = prefs;
    g_skipped = prefs.skipped;
    update_notice_set_status(0, installed_display(prefs.development && can_check_dev()));
    // The automatic check itself runs when the player opens the main menu on the title screen
    // (update_notice_take_auto_check in poll); for headless tests with RAC1_UPDATE_FAKE, offer immediately.
    if (headless) {
        if (const char* v = std::getenv("RAC1_UPDATE_FAKE"); v != nullptr && *v != 0) g_handled = fake(v, false);
    }
}

// orig: (none) — host (update_check.h)
void poll() {
    const bool auto_req = update_notice_take_auto_check();
    const bool manual_req = update_notice_take_manual_check();
    if (!g_headless) {
        const AdvancedUpdatePrefs prefs = advanced_update_prefs();
        const bool channel_changed = prefs.development != g_last_prefs.development;
        const bool check_changed = prefs.check != g_last_prefs.check;
        if (check_changed || channel_changed) {
            g_last_prefs = prefs;
            g_skipped = prefs.skipped;
            update_notice_set_status(0, installed_display(prefs.development && can_check_dev()));
            if (prefs.check && !manual_req) start_check(prefs, true, false);
        }
        if (manual_req) {
            if (const char* v = std::getenv("RAC1_UPDATE_FAKE"); v != nullptr && *v != 0) {
                g_handled = fake(v, true);
                return;
            }
            start_check(prefs, true, true);
        } else if (auto_req) {
            if (const char* v = std::getenv("RAC1_UPDATE_FAKE"); v != nullptr && *v != 0) {
                g_handled = fake(v, false);
                return;
            }
            if (prefs.check) start_check(prefs, false, false);
        }
    }
    if (g_handled || g_check == nullptr || !g_check->done.load(std::memory_order_acquire)) return;
    g_handled = true;
    const Check& c = *g_check;
    if (c.exit_code != 0) {  // (no network, an error, the rate limit, no Python: nothing shown)
        update_notice_set_status(3, c.installed_short);
        std::printf("[update] no answer (offline, or GitHub did not answer): nothing to show\n");
        return;
    }
    auto r = read_result(c.result);
    const char* channel = c.development ? "development" : "stable";
    if (r["status"] != "ok" || r["channel"] != channel || r["installed"] != c.installed) {
        update_notice_set_status(3, c.installed_short);
        return;
    }
    if (r["newer"] != "1") {
        update_notice_set_status(2, c.installed_short);
        std::printf("[update] this build is the latest%s\n", c.development ? " on the dev branch" : " release");
        return;
    }
    UpdateOffer o = offer_base();
    o.development = c.development;
    o.id = r["id"];
    if (!c.manual && !g_skipped.empty() && g_skipped == o.id) {
        update_notice_set_status(0, c.installed_short);
        std::printf("[update] %s is out, skipped (Skip Version)\n", o.id.c_str());
        return;
    }
    if (c.development) {
        o.commits = std::atoi(r["commits"].c_str());
        o.installed = std::strlen(RAC1_GIT_COMMIT) >= 7 ? std::string(RAC1_GIT_COMMIT).substr(0, 7) : std::string(RAC1_VERSION);
        std::printf("[update] %d new commit(s) on the dev branch\n", o.commits);
    } else {
        o.version = r["version"];
        const std::string tag = "v" + o.version;
        const bool on_dev_main = (std::strcmp(RAC1_GIT_BRANCH, "main") == 0 || std::strcmp(RAC1_GIT_BRANCH, "dev") == 0) &&
                                 *RAC1_GIT_DESCRIBE != 0 &&
                                 std::string(RAC1_GIT_DESCRIBE) != tag && std::string(RAC1_GIT_DESCRIBE) != tag + "-dirty";
        o.installed = on_dev_main ? std::string(RAC1_GIT_DESCRIBE) : c.installed;
        o.notes = r["notes"];
        std::printf("[update] version %s is out (installed: %s)\n", o.version.c_str(), o.installed.c_str());
    }
    update_notice_set_status(0, c.installed_short);
    if (c.manual) update_notice_offer_manual(o);
    else update_notice_offer(o);
}

// orig: (none) — host (update_check.h)
bool update_requested() { return update_notice_update_now(); }

// orig: (none) — host (update_check.h)
void run_update() {
    bool dev = false;
    if (!update_notice_update_now(&dev)) return;
    const fs::path root = repo_root();
    const char* ch = dev ? "development" : "stable";
    std::printf("[update] starting the setup in update mode (--update --channel %s --play)\n", ch);
    std::fflush(stdout);
#ifdef _WIN32
    const fs::path cmd = root / "setup.cmd";
    if (fs::exists(cmd)) {
        platform::launch_updater({"cmd.exe", "/c", cmd.string(), "--update", "--channel", ch, "--play"}, root);
        return;
    }
#endif
    platform::launch_updater({python(), (root / "tools/setup.py").string(), "--update", "--channel", ch, "--play"}, root);
}

}  // namespace rac1::update_check
