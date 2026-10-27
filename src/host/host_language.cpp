// The console's language, kept by the host (host_language.h). Host code.
#include "host_language.h"

#include <platform/platform.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <string>

namespace rac1::host_language {
namespace {
bool g_keep = false;          // read and written (not a headless run)
uint32_t g_kept = 0;          // the language last kept (or read)
bool g_known = false;         // g_kept is set

// the game's language codes with texts on the disc (code 1, a UK English block, is empty)
bool valid(uint32_t code) { return code == 0 || (code >= 2 && code <= 5); }

std::filesystem::path file() {
    const std::filesystem::path d = platform::config_dir();
    return d.empty() ? std::filesystem::path("console.txt") : d / "console.txt";
}

// the system's language as the game's code, else English
uint32_t from_system() {
    const std::string l = platform::system_language();
    if (l == "fr") return 2;
    if (l == "de") return 3;
    if (l == "es") return 4;
    if (l == "it") return 5;
    return 0;
}
}  // namespace

uint32_t console_language(bool keep) {
    g_keep = keep;
    g_known = true;
    g_kept = 0;
    if (!keep) return g_kept;
    if (std::FILE* f = platform::open_file(file(), "r")) {
        char key[32];
        unsigned v = 0;
        bool read = false;
        while (std::fscanf(f, "%31s %u", key, &v) == 2)
            if (std::strcmp(key, "language") == 0 && valid(v)) g_kept = v, read = true;
        std::fclose(f);
        if (read) return g_kept;
    }
    g_kept = from_system();
    std::printf("[language] the console's language: %u (from the system's)\n", g_kept);
    return g_kept;
}

void note(uint32_t game_language) {
    if (!g_keep || !g_known || game_language == g_kept || !valid(game_language) || std::getenv("RAC1_LANGUAGE") != nullptr) return;
    g_kept = game_language;
    const std::filesystem::path p = file();
    std::error_code ec;
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    if (platform::write_file_replace(p, "language " + std::to_string(game_language) + "\n")) {
        std::printf("[language] the console's language kept: %u (%s)\n", game_language, p.string().c_str());
    }
}

}  // namespace rac1::host_language
