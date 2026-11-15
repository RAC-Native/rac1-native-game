// See mod_plugins.h. Not a port: our own tool.
#include "mod_plugins.h"

#include <runtime/kernel/mods.h>

#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

#include "../../sdk/rac1_mod.h"

namespace rac1::setup {

namespace fs = std::filesystem;

namespace {

#ifndef RAC1_PLUGIN_CMAKE
#define RAC1_PLUGIN_CMAKE "cmake"
#endif
#ifndef RAC1_PLUGIN_GENERATOR
#define RAC1_PLUGIN_GENERATOR ""
#endif
#ifndef RAC1_PLUGIN_CC
#define RAC1_PLUGIN_CC ""
#endif
#ifndef RAC1_PLUGIN_CXX
#define RAC1_PLUGIN_CXX ""
#endif
#ifndef RAC1_PLUGIN_MAKE
#define RAC1_PLUGIN_MAKE ""
#endif

// a command line's argument, quoted for the shell
std::string quote(const std::string& a) {
#if defined(_WIN32)
    return "\"" + a + "\"";
#else
    std::string q = "'";
    for (char c : a) q += c == '\'' ? std::string("'\\''") : std::string(1, c);
    return q + "'";
#endif
}

// runs `args` (its output into `log`); its exit code
int run(const std::vector<std::string>& args, const fs::path& log) {
    std::string cmd;
    for (const std::string& a : args) cmd += (cmd.empty() ? "" : " ") + quote(a);
    cmd += " >> " + quote(log.string()) + " 2>&1";
#if defined(_WIN32)
    cmd = "\"" + cmd + "\"";  // (cmd.exe drops the first and last quote of a line that starts with one)
#endif
    std::cout << "[plugin]   " << args.front() << " " << (args.size() > 1 ? args[1] : "") << " ...\n";
    return std::system(cmd.c_str());
}

std::string stamp_hex(uint64_t v) {
    char b[24];
    std::snprintf(b, sizeof b, "%016llx", static_cast<unsigned long long>(v));
    return b;
}

}  // namespace

// orig: (host) see mod_plugins.h
bool build_mod_plugin(const std::string& folder, bool force) {
    ModInfo m;
    m.folder = folder;
    m.dir = mods_dir() / folder;
    auto fail = [&](const std::string& why) {
        std::cerr << "[plugin] " << folder << ": " << why << "\n";
        return false;
    };
    std::error_code ec;
    const fs::path src = m.dir / "plugin";
    if (!fs::is_directory(src, ec)) return fail("no plugin/ folder");
    if (!force && mods_plugin_build_check(m).empty()) {
        std::cout << "[plugin] " << folder << ": up to date\n";
        return true;
    }
    const fs::path out = fs::absolute(mods_level_build_dir(folder) / "plugin", ec);
    const fs::path work = fs::absolute(mods_level_build_dir(folder) / "plugin-build", ec);
    fs::remove_all(out, ec);
    fs::create_directories(out, ec);
    fs::create_directories(work, ec);
    const fs::path log = out / "build.log";
    { std::ofstream(log, std::ios::trunc) << "rac1-native: the plugin of mods/" << folder << "\n"; }
    std::cout << "[plugin] " << folder << ": building its code (from " << src.generic_string() << ")\n";
    if (fs::exists(src / "CMakeLists.txt", ec)) {
        const fs::path sdk = fs::absolute("sdk", ec);
        if (!fs::exists(sdk / "rac1_mod.h", ec)) return fail("the SDK (sdk/rac1_mod.h) is not there: start the setup from the repository's root");
        std::vector<std::string> conf = {RAC1_PLUGIN_CMAKE, "-S", fs::absolute(src, ec).string(), "-B", work.string(), "-DCMAKE_BUILD_TYPE=Release",
                                         "-DRAC1_SDK=" + sdk.generic_string(), "-DRAC1_PLUGIN_OUT=" + out.generic_string()};
        if (std::string(RAC1_PLUGIN_GENERATOR).size() > 0) conf.insert(conf.begin() + 1, {"-G", RAC1_PLUGIN_GENERATOR});
        if (std::string(RAC1_PLUGIN_CC).size() > 0) conf.push_back(std::string("-DCMAKE_C_COMPILER=") + RAC1_PLUGIN_CC);
        if (std::string(RAC1_PLUGIN_CXX).size() > 0) conf.push_back(std::string("-DCMAKE_CXX_COMPILER=") + RAC1_PLUGIN_CXX);
        if (std::string(RAC1_PLUGIN_MAKE).size() > 0) conf.push_back(std::string("-DCMAKE_MAKE_PROGRAM=") + RAC1_PLUGIN_MAKE);
        if (run(conf, log) != 0) return fail("CMake could not configure it: see " + log.generic_string());
        if (run({RAC1_PLUGIN_CMAKE, "--build", work.string(), "--config", "Release"}, log) != 0)
            return fail("it does not build: see " + log.generic_string());
    } else if (fs::exists(src / "prebuilt" / kPluginFile, ec)) {
        fs::copy_file(src / "prebuilt" / kPluginFile, out / kPluginFile, fs::copy_options::overwrite_existing, ec);
        if (ec) return fail("its prebuilt plugin cannot be copied");
        std::cout << "[plugin] " << folder << ": its prebuilt " << kPluginFile << " (not built from source)\n";
    } else {
        return fail(std::string("plugin/ has no CMakeLists.txt (nor a prebuilt/") + kPluginFile + ")");
    }
    if (!fs::exists(out / kPluginFile, ec)) return fail(std::string("the build made no ") + kPluginFile + " (sdk/rac1_mod.cmake's rac1_mod_plugin makes it)");
    std::ofstream pj(out / "plugin.json");
    pj << "{\n  \"abi\": " << RAC1_MOD_ABI << ",\n  \"inputs\": \"" << stamp_hex(mods_plugin_inputs_stamp(m.dir)) << "\"\n}\n";
    if (!pj) return fail("cannot write plugin.json");
    std::cout << "[plugin] " << folder << ": built into " << out.generic_string() << "\n";
    return true;
}

// orig: (host) see mod_plugins.h
bool build_mod_plugins(bool force) {
    std::error_code ec;
    const fs::path root = mods_dir();
    if (!fs::is_directory(root, ec)) return true;
    std::vector<std::string> folders;
    for (const auto& e : fs::directory_iterator(root, ec)) {
        if (!e.is_directory(ec)) continue;
        const std::string f = e.path().filename().string();
        if (f.empty() || f[0] == '.' || f[0] == '_') continue;
        if (fs::is_directory(e.path() / "plugin", ec)) folders.push_back(f);
    }
    std::sort(folders.begin(), folders.end());
    bool ok = true;
    for (const std::string& f : folders) ok = build_mod_plugin(f, force) && ok;
    return ok;
}

}  // namespace rac1::setup
