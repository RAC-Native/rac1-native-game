// `rac1-native --setup`: generates the game's data into assets/ from the player's own disc image.
// ./setup.sh (tools/setup.py) drives it one step at a time and decides which steps to redo; it can
// also be run on its own.
#include "setup_manager.h"
#include "iso_extract.h"
#include "mrp_export.h"
#include "scene_export.h"

#include <io/mapped_file.h>
#include <platform/platform.h>
#include <runtime/kernel/disc.h>

#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <string>
#include <vector>

namespace rac1::setup {

namespace {

namespace fs = std::filesystem;

// where the setup finds the disc image again (runtime/kernel/disc.cpp; the game reads the disc's files, assets/disc/, and
// the image only with RAC1_READ_ISO=1), relative to assets/: one link per region
constexpr const char* GAME_ISO = "iso/rac1-ntsc-u.iso";
constexpr const char* GAME_ISO_PAL = "iso/rac1-pal.iso";

const char* game_iso(const KnownDisc* disc) {
    return disc != nullptr && std::string(disc->region) == "PAL" ? GAME_ISO_PAL : GAME_ISO;
}

void print_usage(const char* prog) {
    std::cout << "Usage: " << prog << " --setup [options]\n\n"
              << "Generates the game's data into assets/ from your Ratchet & Clank disc image (NTSC-U SCUS-97199,\n"
              << "or PAL SCES-50916).\n"
              << "Run it from the repository's root. ./setup.sh runs it for you.\n\n"
              << "Options:\n"
              << "  --iso <path>       your disc image (default: $RAC1_ISO, then assets/iso/rac1-ntsc-u.iso or rac1-pal.iso)\n"
              << "  --assets <dir>     where to write the data (default: assets; the game reads assets/)\n"
              << "  --force            regenerate files that already exist\n"
              << "  --step <step>      run one step only: elf, moby, world, flight, overlay (default: all)\n"
              << "  --check-iso        only check the disc image, and make the game find it\n"
              << "  --compare-disc     read every sector through the image and through the disc's files (assets/disc/),\n"
              << "                     and compare\n"
              << "  --status           show which parts of the data are present\n"
              << "  --help, -h         show this help\n";
}

void print_status(const fs::path& root) {
    std::cout << "\nGame data in " << root.string() << ":\n\n";

    auto check_file = [&](const std::string& desc, const fs::path& p) {
        const bool ok = fs::exists(p);
        std::cout << "  [" << (ok ? "OK" : "MISSING") << "] " << desc << " (" << p.string() << ")\n";
    };
    auto count_dir = [&](const std::string& desc, const fs::path& p, size_t min_count) {
        size_t count = 0;
        if (fs::exists(p))
            for (auto const& e : fs::recursive_directory_iterator(p))
                if (e.is_regular_file()) ++count;
        const bool ok = count >= min_count;
        std::cout << "  [" << (ok ? "OK" : "MISSING") << "] " << desc << " (" << count << " files in " << p.string() << ")\n";
    };

    const bool pal = !fs::exists(root / GAME_ISO) && fs::exists(root / GAME_ISO_PAL);
    const fs::path link = root / (pal ? GAME_ISO_PAL : GAME_ISO);
    check_file("Disc image", fs::exists(link) || !fs::exists(root / "iso/disc-path.txt") ? link : fs::path(disc_path()));
    check_file("The disc's files", root / "disc/layout.txt");
    check_file("Boot executable", root / (pal ? "dumps/SCES_509.16" : "dumps/SCUS_971.99"));
    count_dir("Moby models and textures", root / "generated/mrp/moby", 20);
    count_dir("World geometry and textures", root / "generated/mrp/world", 20);
    check_file("The flight between planets", root / "generated/mrp/moby/flight/textures.mrpt");
    count_dir("Overlay address maps", root / "generated/overlay_map", 20);
    std::cout << "\n";
}

// Makes the setup find the disc again: assets/iso/rac1-ntsc-u.iso (rac1-pal.iso for the PAL disc), a link to the
// image when it is elsewhere (the game reads the disc's files; the image only with RAC1_READ_ISO=1). A file the
// player put there is never replaced. The other region's link is removed: assets/ holds one disc's data. Where a
// link cannot be made (Windows, without Developer Mode or the administrator's right), the image's path goes into
// assets/iso/disc-path.txt instead, which the setup and RAC1_READ_ISO read (runtime/kernel/disc.cpp).
bool link_iso(const fs::path& assets, const fs::path& iso, const KnownDisc* disc) {
    const fs::path dest = assets / game_iso(disc);
    std::error_code ec;
    const fs::path other = assets / (game_iso(disc) == GAME_ISO ? GAME_ISO_PAL : GAME_ISO);
    if (fs::is_symlink(fs::symlink_status(other, ec))) fs::remove(other, ec);
    const fs::path target = fs::absolute(iso);
    if (fs::exists(dest, ec) && fs::equivalent(dest, target, ec)) return true;
    if (fs::is_symlink(fs::symlink_status(dest, ec))) {
        fs::remove(dest, ec);  // (a link to another image, or a broken one)
    } else if (fs::exists(fs::symlink_status(dest, ec))) {
        std::cerr << "[setup] Note: " << dest.string() << " is another file, and the game reads that one.\n"
                  << "        To play from " << target.string() << ", move that file away and run the setup again.\n";
        return true;
    }
    fs::create_directories(dest.parent_path(), ec);
    const fs::path path_file = dest.parent_path() / "disc-path.txt";
    fs::create_symlink(target, dest, ec);
    if (ec) {
#if defined(_WIN32)
        // no link: the path in a text file (UTF-8, the program's code page: its manifest)
        std::ofstream f(path_file, std::ios::binary | std::ios::trunc);
        f << target.string() << "\n";
        if (f.flush()) {
            std::cout << "[setup] Your disc image: " << target.string() << " (noted in " << path_file.string() << ")\n";
            return true;
        }
#endif
        std::cerr << "[setup] Error: cannot create " << dest.string() << ": " << ec.message() << "\n";
        return false;
    }
    fs::remove(path_file, ec);  // (an older note of the path: the link is used now)
    std::cout << "[setup] Your disc image: " << dest.string() << "\n";
    return true;
}

// The repository's root: the directory above build/ holding this executable, else the current directory.
fs::path repo_root() {
    const fs::path exe = platform::executable_path();
    if (!exe.empty()) {
        const fs::path root = exe.parent_path().parent_path();
        if (fs::exists(root / "tools/overlay_match/overlay_match.py")) return root;
    }
    return fs::current_path();
}

// Runs a program (looked up in PATH) with its standard output discarded; returns its exit code (-1: not started).
int run_quiet(const std::vector<std::string>& args) { return platform::run_quiet(args); }

bool overlay_maps(const fs::path& assets, const std::string& iso_path, const KnownDisc* disc) {
    const fs::path tools = repo_root() / "tools/overlay_match";
    const fs::path out = assets / "generated/overlay_map";
    if (!fs::exists(tools / "overlay_match.py")) {
        std::cerr << "\n[setup] Error: " << (tools / "overlay_match.py").string() << " not found. Run the setup from the repository.\n";
        return false;
    }
    fs::create_directories(out);
    const fs::path elf = assets / "dumps" / disc->boot_file;
    const std::string py = platform::python_program();
    int r = 0;
    if (std::string(disc->region) != "NTSC-U") {
        // another region's disc: through its region table (data/regions/<serial>.txt): the game's maps, then the code
        // generators' (tools/setup.py does the same)
        const fs::path table = repo_root() / "data/regions" / (std::string(disc->serial) + ".txt");
        std::cout << " (region table)" << std::flush;
        r = run_quiet({py, (tools / "region_maps.py").string(), "apply", "--iso", iso_path, "--elf", elf.string(),
                       "--table", table.string(), "-o", out.string()});
        if (r == 0)
            r = run_quiet({py, (tools / "region_view.py").string(), "maps", "--iso", iso_path, "--table", table.string(),
                           "-o", (out / "codegen").string()});
    } else {
        // the boot module (the title and its menu) first, then each level against Veldin's (level 0)
        std::cout << " title" << std::flush;
        r = run_quiet({py, (tools / "front_match.py").string(), "--elf", elf.string(), "--iso", iso_path, "--out",
                       out.string()});
    }
    for (int lv = 1; lv <= 18 && r == 0 && std::string(disc->region) == "NTSC-U"; ++lv) {
        std::cout << " " << lv << std::flush;
        r = run_quiet({platform::python_program(), (tools / "overlay_match.py").string(), "--iso", iso_path, "--out", out.string(),
                       "--levels", std::to_string(lv)});
    }
    if (r != 0) {
        std::cerr << "\n[setup] Error: the overlay matcher failed (" << (r < 0 ? platform::python_program() + " not found"
                                                                             : "exit code " + std::to_string(r)) << ").\n";
        return false;
    }
    return true;
}

// Every sector of the disc read through the image and through the extracted files (assets/disc/, tools/disc_extract.py),
// the two compared: reads of 1 to 67 sectors one after the other, so that they start and end everywhere (across two
// files, inside one, where no file is), then reads past the end, which both refuse. 0 when all are the same.
int compare_disc() {
    const uint64_t bytes = disc_size(DiscSource::Image), files_bytes = disc_size(DiscSource::Files);
    if (files_bytes == 0) {
        std::cerr << "[setup] Error: cannot read the disc's files (" << disc_files_dir() << "): run the setup to extract them\n";
        return 1;
    }
    if (bytes == 0 || bytes != files_bytes || bytes % 2048 != 0) {
        std::cerr << "[setup] Error: the image is " << bytes << " bytes, the disc's files " << files_bytes << ": extract them again\n";
        return 1;
    }
    const uint32_t sectors = static_cast<uint32_t>(bytes / 2048);
    std::vector<char> a(67 * 2048), b(67 * 2048);
    uint32_t lba = 0, reads = 0, step = 0, last_pct = 0;
    std::cout << "[setup] Comparing the disc's " << sectors << " sectors: 0%" << std::flush;
    while (lba < sectors) {
        const uint32_t n = std::min<uint32_t>(1 + (step++ * 37) % 67, sectors - lba);
        const bool ra = disc_read_from(DiscSource::Image, lba, n, a.data()), rb = disc_read_from(DiscSource::Files, lba, n, b.data());
        if (!ra || !rb || std::memcmp(a.data(), b.data(), size_t{n} * 2048) != 0) {
            std::cerr << "\n[setup] Error: sectors " << lba << " to " << lba + n - 1 << " differ (" << (ra ? "" : "the image cannot be read, ")
                      << (rb ? "" : "the files cannot be read, ") << "extract the disc again)\n";
            return 1;
        }
        lba += n;
        ++reads;
        const uint32_t pct = static_cast<uint32_t>(uint64_t{lba} * 100 / sectors);
        if (pct / 10 != last_pct / 10) std::cout << " " << pct << "%" << std::flush;
        last_pct = pct;
    }
    for (const uint32_t at : {sectors - 1, sectors, sectors + 1}) {  // (past the end: refused by both)
        if (disc_read_from(DiscSource::Image, at, 2, a.data()) || disc_read_from(DiscSource::Files, at, 2, b.data())) {
            std::cerr << "\n[setup] Error: a read past the disc's end (sector " << at << ") was not refused\n";
            return 1;
        }
    }
    std::cout << "\n[setup] The disc's files read the same as the image: " << sectors << " sectors in " << reads << " reads\n";
    return 0;
}

} // namespace

int run_setup(int argc, char** argv) {
    std::string iso_path;
    std::string assets_dir = "assets";
    std::string step = "all";
    bool force = false;
    bool status_only = false;
    bool check_only = false;
    bool compare = false;

    if (const char* env_iso = std::getenv("RAC1_ISO")) iso_path = env_iso;

    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (a == "--setup" || a == "-s") {
            continue;
        } else if ((a == "--iso" || a == "-i") && i + 1 < argc) {
            iso_path = argv[++i];
        } else if ((a == "--assets" || a == "-a") && i + 1 < argc) {
            assets_dir = argv[++i];
        } else if (a == "--force" || a == "-f") {
            force = true;
        } else if (a == "--step" && i + 1 < argc) {
            step = argv[++i];
        } else if (a == "--status") {
            status_only = true;
        } else if (a == "--check-iso") {
            check_only = true;
        } else if (a == "--compare-disc") {
            compare = true;
        } else if (a == "--help" || a == "-h") {
            print_usage(argv[0]);
            return 0;
        } else {
            std::cerr << "Unknown option: " << a << "\n";
            print_usage(argv[0]);
            return 1;
        }
    }
    if (step != "all" && step != "elf" && step != "moby" && step != "world" && step != "flight" &&
        step != "overlay") {
        std::cerr << "Unknown step: " << step << " (elf, moby, world, flight, overlay or all)\n";
        return 1;
    }

    const fs::path assets_path = fs::absolute(assets_dir);
    if (status_only) {
        print_status(assets_path);
        return 0;
    }

    if (iso_path.empty()) {
        fs::path default_iso = assets_path / GAME_ISO;
        if (!fs::exists(default_iso)) default_iso = assets_path / GAME_ISO_PAL;
        if (!fs::exists(default_iso)) {
            std::cerr << "[setup] Error: no disc image given.\n"
                      << "  Run ./setup.sh, or give the path to your Ratchet & Clank ISO (NTSC-U or PAL):\n"
                      << "    " << argv[0] << " --setup --iso <path/to/game.iso>\n";
            return 1;
        }
        iso_path = default_iso.string();
    }
    iso_path = fs::absolute(iso_path).string();

    const auto val = validate_iso(iso_path);
    if (!val.valid) {
        std::cerr << "[setup] Error: " << iso_path << "\n  " << val.message << "\n";
        return 2;  // (tools/setup.py: a disc that cannot be used)
    }
    // (the setup's steps read the image they were given, not the disc's files)
    if (!disc_open(iso_path)) {
        std::cerr << "[setup] Error: cannot read " << iso_path << "\n";
        return 1;
    }
    if (compare) return compare_disc();
    const bool verbose = step == "all" || check_only;  // (tools/setup.py checks the disc once, then runs the steps one by one)
    if (verbose) {
        std::cout << "[setup] Disc image: Ratchet & Clank, " << val.disc->region << " (" << val.disc->serial << ")\n";
        if (!val.message.empty()) std::cout << "[setup] Warning: " << val.message << "\n";
    }
    if (!link_iso(assets_path, iso_path, val.disc)) return 1;
    {
        std::error_code ec;
        const fs::path old_wads = assets_path / "extracted/level_wads";
        if (fs::exists(old_wads, ec)) {
            fs::remove_all(old_wads, ec);
            fs::remove(assets_path / "extracted", ec);
        }
    }
    if (check_only) return 0;

    MappedFile iso_file(iso_path);
    if (!iso_file) {
        std::cerr << "[setup] Error: cannot read " << iso_path << "\n";
        return 1;
    }
    const auto iso_span = iso_file.span();
    fs::create_directories(assets_path);

    auto want = [&](const char* s) { return step == "all" || step == s; };
    auto begin = [](int n, const char* what) { std::cout << "[setup] [" << n << "/5] " << what << "..." << std::flush; };
    auto skip = [](int n, const char* what) { std::cout << "[setup] [" << n << "/5] " << what << ": already there\n"; };

    if (want("elf")) {
        const fs::path elf_dest = assets_path / "dumps" / val.disc->boot_file;
        if (!force && fs::exists(elf_dest)) {
            skip(1, "Boot executable");
        } else {
            begin(1, "Copying the boot executable");
            if (!extract_boot_elf(iso_span, elf_dest.string(), true, val.disc->boot_file)) {
                std::cerr << " failed\n";
                return 1;
            }
            std::cout << " done\n";
        }
    }

    // (the exporters skip, level by level, what already exists unless forced)
    if (want("moby")) {
        begin(2, "Converting the moby models and textures, level");
        if (!export_mrp_mobys(iso_span, (assets_path / "generated/mrp/moby").string(), "all", force)) {
            std::cerr << "\n[setup] Error: the moby export failed\n";
            return 1;
        }
        std::cout << " done\n";
    }

    if (want("world")) {
        begin(3, "Converting the world geometry and textures, level");
        if (!export_mrp_world(iso_span, (assets_path / "generated/mrp/world").string(), "all", force)) {
            std::cerr << "\n[setup] Error: the world export failed\n";
            return 1;
        }
        std::cout << " done\n";
    }

    if (want("flight")) {
        const fs::path moby = assets_path / "generated/mrp/moby", world = assets_path / "generated/mrp/world";
        if (!force && fs::exists(moby / "flight/textures.mrpt") && fs::exists(world / "flight/world.mrpw")) {
            skip(4, "The flight between planets");
        } else {
            begin(4, "Converting the flight between planets (the loading screen)");
            if (!export_flight(iso_span, moby.string(), world.string(), force)) {
                std::cerr << " failed\n";
                return 1;
            }
            std::cout << " done\n";
        }
    }

    if (want("overlay")) {
        const fs::path ovl = assets_path / "generated/overlay_map";
        bool present = fs::exists(ovl / "front_reloc.txt");
        const bool other = std::string(val.disc->region) != "NTSC-U";
        for (int lv = 1; lv <= 18; ++lv) {
            present = present && fs::exists(ovl / ("level" + std::to_string(lv) + ".txt"));
            if (other) present = present && fs::exists(ovl / ("codegen/level" + std::to_string(lv) + ".txt"));
        }
        if (!force && present) {
            skip(5, "Overlay address maps");
        } else {
            begin(5, "Matching each level's code to Veldin's, level");
            if (!overlay_maps(assets_path, iso_path, val.disc)) return 1;
            std::cout << " done\n";
        }
    }

    if (verbose) std::cout << "[setup] The game's data is ready in " << assets_path.string() << "\n";
    return 0;
}

} // namespace rac1::setup
