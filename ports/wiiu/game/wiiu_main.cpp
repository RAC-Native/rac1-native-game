// The Wii U's entry: the game's host (src/host/orig_main.cpp, its main renamed rac1_orig_main) run with the settings of
// rac1-native/run.cfg on the SD card (KEY=VALUE lines, the RAC1_* variables of docs/env-vars.md; RAC1_HEADLESS and
// RAC1_NO_GRAPHICS always on), its output in rac1-native/run.log, the game's data beside it (rac1-native/assets/: the
// disc image, the boot executable and the level maps the setup made on the PC from the player's own disc). Then back
// to the Wii U Menu.
#include <coreinit/foreground.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <sysapp/launch.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_module.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <string>
#include <unistd.h>

int rac1_orig_main(int argc, char** argv);
void rac1_wiiu_debug_tools_gate(int argc, char** argv);  // (src/host/orig_main.cpp)

namespace {
constexpr const char* kDir = "fs:/vol/external01/rac1-native";

void say(const char* s) {
    WHBLogPrintf("rac1-wiiu: %s", s);
    WHBLogConsoleDraw();
}

// (RAC1_WIIU_ISO_CHECK=1) the disc image's blocks of 1 MB at these offsets hashed (FNV-1a), to compare with the PC's
// copy: the SD card's copy read back as the game reads it (FTP cannot read past 2 GB here)
void iso_check() {
    static const unsigned long long offs[] = {0ull, 1073741824ull, 2911633408ull, 2912157696ull, 3500000000ull, 4213047296ull};
    std::FILE* f = std::fopen(std::getenv("RAC1_ISO") != nullptr ? std::getenv("RAC1_ISO") : "assets/iso/rac1-ntsc-u.iso", "rb");
    if (f == nullptr) {
        std::printf("[wiiu] iso check: no assets/iso/rac1-ntsc-u.iso\n");
        return;
    }
    static unsigned char buf[1 << 20];
    for (unsigned long long off : offs) {
        uint32_t h = 2166136261u;
        size_t n = 0;
        if (fseeko(f, static_cast<off_t>(off), SEEK_SET) == 0) n = std::fread(buf, 1, sizeof buf, f);
        for (size_t i = 0; i < n; ++i) {
            h ^= buf[i];
            h *= 16777619u;
        }
        std::printf("[wiiu] iso check @%llu: %zu bytes, fnv %08x\n", off, n, h);
    }
    std::fclose(f);
}

int read_settings() {
    setenv("RAC1_HEADLESS", "1", 1);
    setenv("RAC1_NO_GRAPHICS", "1", 1);
    std::FILE* f = std::fopen("run.cfg", "r");
    if (f == nullptr) return 0;
    int n = 0;
    char line[512];
    while (std::fgets(line, sizeof line, f) != nullptr) {
        std::string l = line;
        while (!l.empty() && (l.back() == '\n' || l.back() == '\r' || l.back() == ' ')) l.pop_back();
        if (l.empty() || l[0] == '#') continue;
        const size_t eq = l.find('=');
        if (eq == std::string::npos) continue;
        setenv(l.substr(0, eq).c_str(), l.substr(eq + 1).c_str(), 1);
        ++n;
    }
    std::fclose(f);
    return n;
}
}  // namespace

int main(int, char**) {
    WHBProcInit();
    WHBLogModuleInit();
    WHBLogUdpInit();
    WHBLogConsoleInit();
    say("start");
    if (chdir(kDir) != 0) say("cannot enter fs:/vol/external01/rac1-native");
    char msg[160];
    std::snprintf(msg, sizeof msg, "%d settings from run.cfg; the game's output goes to run.log", read_settings());
    say(msg);

    std::freopen("run.log", "w", stdout);
    std::freopen("run.log", "a", stderr);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    char arg0[] = "rac1-native";
    char* argv[] = {arg0, nullptr};
    if (const char* c = std::getenv("RAC1_WIIU_ISO_CHECK"); c != nullptr && c[0] == '1') iso_check();
    rac1_wiiu_debug_tools_gate(1, argv);
    const OSTime t0 = OSGetTime();
    const int rc = rac1_orig_main(1, argv);
    const unsigned ms = static_cast<unsigned>(OSTicksToMilliseconds(OSGetTime() - t0));
    std::printf("[wiiu] the game's main returned %d after %u ms\n", rc, ms);
    std::fflush(stdout);
    std::snprintf(msg, sizeof msg, "the game's main returned %d after %u ms", rc, ms);
    say(msg);

    // back to the menu (ProcUI's messages handled meanwhile; the screen given back on its release)
    SYSLaunchMenu();
    while (WHBProcIsRunning()) {
        WHBLogConsoleDraw();
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    say("exit");
    WHBLogConsoleFree();
    WHBLogUdpDeinit();
    WHBLogModuleDeinit();
    WHBProcShutdown();
    return 0;
}
