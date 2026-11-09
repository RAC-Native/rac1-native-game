// The game's data on the console's USB storage (its own folder, rac1-native/, at the root of the storage the Wii U formatted - beside
// usr/ and sys/, which it never touches): the SD card's FAT walks a big folder at every open (10-28 ms) and a file's cluster chain to
// seek back; the USB storage's file system (WFS) does neither. Reached through Aroma's Mocha (libmocha: the storage mounted for this
// app, as rac1usb:/).
//   RAC1_WIIU_USB_COPY=1   the SD card's rac1-native/assets/disc/ copied there (files already there at their size skipped), then
//                          .complete written beside them; on the log, its progress
//   RAC1_WIIU_USB_BENCH=1  opens and first reads of the same files on the SD card and on the USB storage, timed, on the log
//   RAC1_WIIU_USB_DELETE=1 the copy removed (rac1-native/ on the USB storage, nothing else)
//   RAC1_WIIU_USB=1        the disc's files read from a complete copy there (else, the default, from the SD card)
// Not faster, measured (2026-11-09, an SSD): the route Veldin -> Novalis -> Kerwan with its reads over 40 ms logged, the SD card 13,
// the USB storage 20; a level's sound pack read whole 3.0 / 2.76 s, Kerwan's data.wad 0.83 / 0.88 s; an open in toc/ 3-33 / 2-44 ms.
// The cost is the console's file system (IOSU, the storage's encryption, USB 2.0), not the card. With RAC1_WIIU_USB=1 and a complete
// copy the disc's files are read from it (RAC1_WIIU_DISC_DIR, runtime/kernel/disc.cpp's disc_files_dir); the rest
// (run.cfg, the shaders, the memory card, mods/) stays on the SD card.
#include <coreinit/time.h>
#include <mocha/mocha.h>
#include <whb/log.h>

#include <dirent.h>
#include <sys/stat.h>

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <memory>
#include <string>
#include <vector>

namespace {

constexpr const char* kUsbDisc = "rac1usb:/rac1-native/assets/disc";
constexpr const char* kSdDisc = "assets/disc";  // (the SD card's, from rac1-native/)

void log(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void log(const char* fmt, ...) {
    char s[256];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    std::printf("[usb] %s\n", s);
    WHBLogPrintf("[usb] %s", s);
}

double ms_since(OSTime t0) { return static_cast<double>(OSTicksToMicroseconds(OSGetTime() - t0)) / 1000.0; }

bool exists(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0;
}

struct Copy {
    std::vector<char> buf = std::vector<char>(2u << 20);
    unsigned long long bytes = 0, files = 0, skipped = 0, failed = 0;
    OSTime t0 = OSGetTime();
    OSTime shown = 0;

    bool file(const std::string& from, const std::string& to, off_t size) {
        struct stat st;
        if (stat(to.c_str(), &st) == 0 && st.st_size == size) {
            ++skipped;
            return true;
        }
        std::FILE* in = std::fopen(from.c_str(), "rb");
        if (in == nullptr) return false;
        const std::string tmp = to + ".part";
        std::FILE* out = std::fopen(tmp.c_str(), "wb");
        if (out == nullptr) {
            std::fclose(in);
            return false;
        }
        bool ok = true;
        for (;;) {
            const size_t n = std::fread(buf.data(), 1, buf.size(), in);
            if (n == 0) break;
            if (std::fwrite(buf.data(), 1, n, out) != n) {
                ok = false;
                break;
            }
            bytes += n;
        }
        ok = ok && std::ferror(in) == 0;
        std::fclose(in);
        ok = std::fclose(out) == 0 && ok;
        if (ok) {
            std::remove(to.c_str());
            ok = std::rename(tmp.c_str(), to.c_str()) == 0;
        } else {
            std::remove(tmp.c_str());
        }
        ++files;
        if (OSTicksToMilliseconds(OSGetTime() - shown) > 5000) {
            shown = OSGetTime();
            const double s = ms_since(t0) / 1000.0;
            log("copying: %llu files (%llu skipped), %llu MB in %.0f s (%.1f MB/s)", files, skipped, bytes >> 20, s, s > 0 ? (bytes >> 20) / s : 0.0);
        }
        return ok;
    }

    void tree(const std::string& from, const std::string& to) {
        mkdir(to.c_str(), 0777);
        DIR* d = opendir(from.c_str());
        if (d == nullptr) {
            ++failed;
            log("cannot open %s", from.c_str());
            return;
        }
        std::vector<std::pair<std::string, bool>> entries;
        while (dirent* e = readdir(d)) {
            if (std::strcmp(e->d_name, ".") == 0 || std::strcmp(e->d_name, "..") == 0) continue;
            entries.emplace_back(e->d_name, e->d_type == DT_DIR);
        }
        closedir(d);
        for (const auto& [name, dir] : entries) {
            const std::string f = from + "/" + name, t = to + "/" + name;
            if (dir) {
                tree(f, t);
                continue;
            }
            struct stat st;
            if (stat(f.c_str(), &st) != 0 || !file(f, t, st.st_size)) {
                ++failed;
                log("failed: %s", f.c_str());
            }
        }
    }
};

void copy_to_usb() {
    mkdir("rac1usb:/rac1-native", 0777);
    mkdir("rac1usb:/rac1-native/assets", 0777);
    std::remove((std::string(kUsbDisc) + "/.complete").c_str());
    Copy c;
    c.tree(kSdDisc, kUsbDisc);
    const double s = ms_since(c.t0) / 1000.0;
    log("copy done: %llu files copied, %llu already there, %llu failed; %llu MB in %.0f s", c.files, c.skipped, c.failed, c.bytes >> 20, s);
    if (c.failed == 0)
        if (std::FILE* f = std::fopen((std::string(kUsbDisc) + "/.complete").c_str(), "w")) {
            std::fputs("rac1-native: the disc's files copied from the SD card\n", f);
            std::fclose(f);
        }
}

// orig: (host) a folder of ours and everything in it removed
void remove_tree(const std::string& dir, unsigned long long& files) {
    std::vector<std::pair<std::string, bool>> entries;
    if (DIR* d = opendir(dir.c_str())) {
        while (dirent* e = readdir(d))
            if (std::strcmp(e->d_name, ".") != 0 && std::strcmp(e->d_name, "..") != 0) entries.emplace_back(e->d_name, e->d_type == DT_DIR);
        closedir(d);
    }
    for (const auto& [name, sub] : entries) {
        const std::string p = dir + "/" + name;
        if (sub) {
            remove_tree(p, files);
        } else if (std::remove(p.c_str()) == 0) {
            ++files;
        }
    }
    rmdir(dir.c_str());
}

// the same files opened and their first 64 KB read on both (each set once: the second medium's reads never the first's cache)
void bench() {
    static const char* const kFiles[] = {"toc/13/row_13b8.bin", "toc/19/row_1960.pss", "levels/level03/hdr.pack", "levels/level01/hdr.pack",
                                         "levels/level05/hdr.pack", "toc/1a/row_1ac4.vag", "layout.txt"};
    std::vector<char> buf(64 << 10);
    for (const char* root : {kSdDisc, kUsbDisc}) {
        double open_ms = 0, read_ms = 0, far_ms = 0;
        int n = 0;
        for (const char* name : kFiles) {
            const std::string p = std::string(root) + "/" + name;
            OSTime t = OSGetTime();
            std::FILE* f = std::fopen(p.c_str(), "rb");
            const double o = ms_since(t);
            if (f == nullptr) continue;
            t = OSGetTime();
            std::fread(buf.data(), 1, buf.size(), f);
            const double r = ms_since(t);
            std::fseek(f, 0, SEEK_END);
            const long size = std::ftell(f);
            t = OSGetTime();  // (a read far in, then back near the start: a seek back)
            std::fseek(f, size > (1 << 20) ? size - (1 << 20) : 0, SEEK_SET);
            std::fread(buf.data(), 1, buf.size(), f);
            std::fseek(f, 64 << 10, SEEK_SET);
            std::fread(buf.data(), 1, buf.size(), f);
            const double fr = ms_since(t);
            std::fclose(f);
            open_ms += o;
            read_ms += r;
            far_ms += fr;
            ++n;
            log("%s %s: open %.1f ms, 64 KB %.1f ms, far and back %.1f ms", root == kSdDisc ? "SD " : "USB", name, o, r, fr);
        }
        if (n > 0) log("%s: %d files, open %.1f ms, first read %.1f ms, far and back %.1f ms on average", root == kSdDisc ? "SD " : "USB", n, open_ms / n, read_ms / n, far_ms / n);
    }
}

}  // namespace

void wiiu_usb_setup() {
    const char* on = std::getenv("RAC1_WIIU_USB");
    const bool use = on != nullptr && std::atoi(on) != 0;
    const char* copy = std::getenv("RAC1_WIIU_USB_COPY");
    const char* bench_on = std::getenv("RAC1_WIIU_USB_BENCH");
    const char* del = std::getenv("RAC1_WIIU_USB_DELETE");
    const bool del_on = del != nullptr && std::atoi(del) != 0;
    if (!use && !del_on && (copy == nullptr || std::atoi(copy) == 0) && (bench_on == nullptr || std::atoi(bench_on) == 0)) return;
    if (Mocha_InitLibrary() != MOCHA_RESULT_SUCCESS) {
        log("no Mocha: the data from the SD card");
        return;
    }
    const MochaUtilsStatus m = Mocha_MountFS("rac1usb", nullptr, "/vol/storage_usb01");
    if (m != MOCHA_RESULT_SUCCESS && m != MOCHA_RESULT_ALREADY_EXISTS) {
        log("the USB storage not mounted (%s): the data from the SD card", Mocha_GetStatusStr(m));
        return;
    }
    if (del_on) {
        unsigned long long files = 0;
        const OSTime t0 = OSGetTime();
        remove_tree("rac1usb:/rac1-native", files);
        log("the copy removed: %llu files in %.0f ms; rac1-native/ %s", files, ms_since(t0), exists("rac1usb:/rac1-native") ? "still there" : "gone");
        return;
    }
    if (copy != nullptr && std::atoi(copy) != 0) copy_to_usb();
    if (bench_on != nullptr && std::atoi(bench_on) != 0) bench();
    if (!use) return;
    if (exists(std::string(kUsbDisc) + "/.complete")) {
        setenv("RAC1_WIIU_DISC_DIR", kUsbDisc, 1);
        log("the disc's files from the USB storage (%s)", kUsbDisc);
    } else {
        log("no complete copy on the USB storage: the data from the SD card");
    }
}
