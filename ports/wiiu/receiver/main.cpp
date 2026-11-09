// rac1-native's Wii U receiver: our own small homebrew (no code of the game in it) that takes the files a PC sends - the Wii U
// build of the game made on that PC from the player's own disc, and the disc's data laid out for the SD card - and writes them
// to the SD card, then starts the game. The PC side: ports/wiiu/tools/wiiu_send.py (later the launcher).
//
// On the TV and the GamePad: the console's address, the port and a pairing code the PC must give before it can write anything.
// A: start the game (when it is on the SD card). HOME: back to the menu.
//
// The protocol (TCP port 47821, one client at a time; text lines ending in \n, a file's bytes raw after its PUT):
//   HELLO 1 <code>                -> OK rac1-receiver <version> | ERR <why>          (nothing else is answered before it)
//   STAT <path>                   -> SIZE <bytes> <crc32 hex> | PART <bytes> | NONE   (a file this receiver wrote whole, one
//                                    half-written, or none)
//   PUT <path> <bytes> <crc32 hex> -> READY <offset>, then <bytes - offset> bytes, -> OK | ERR <why>
//                                    (offset: what a .part of the same file kept from a transfer cut short; 0 otherwise)
//   MANIFEST                      -> N <count>, then <count> lines "<path> <bytes> <crc32 hex>": the files this receiver wrote
//                                    whole, as its manifest records them (not checked on the card; STAT checks one)
//   PUTS <path> <bytes> <crc32 hex> + the bytes at once, no READY -> OK | ERR <why>  (a small file, written straight to its
//                                    name; the PC may send several before reading their answers)
//   DEL <path>                    -> OK
//   FREE                          -> FREE <bytes free on the SD card>
//   LAUNCH                        -> OK, and the game starts
//   BYE                           -> OK
//   QUIT                          -> OK, and back to the Wii U Menu
// <path>: relative to the SD card's root, inside rac1-native/ or wiiu/apps/rac1-native/, no "..". A file is written to
// <path>.part and renamed when its CRC-32 matches; its size and CRC go into rac1-native/.receiver/manifest.txt (STAT's answer).
#include <coreinit/memory.h>
#include <coreinit/thread.h>
#include <coreinit/time.h>
#include <nn/ac.h>
#include <nn/nets2/somemopt.h>
#include <rpxloader/rpxloader.h>
#include <sysapp/launch.h>
#include <sys/statvfs.h>
#include <vpad/input.h>
#include <whb/log.h>
#include <whb/log_console.h>
#include <whb/log_udp.h>
#include <whb/proc.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <unistd.h>

#include <cerrno>
#include <cstdarg>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <malloc.h>
#include <memory>
#include <thread>
#include <string>
#include <utility>
#include <vector>

namespace {

constexpr const char* kVersion = "0.1.0";
constexpr int kPort = 47821;
constexpr const char* kSd = "fs:/vol/external01/";
constexpr const char* kGame = "wiiu/apps/rac1-native/rac1-native.rpx";
constexpr const char* kManifest = "fs:/vol/external01/rac1-native/.receiver/manifest.txt";

// ---- the screen: a few fixed lines and the last messages ----
std::string g_head[4];
std::vector<std::string> g_lines;
OSTime g_drawn = 0;

void draw(bool now = false) {
    if (!now && OSTicksToMilliseconds(OSGetTime() - g_drawn) < 250) return;
    g_drawn = OSGetTime();
    WHBLogConsoleSetColor(0x10203000);
    // (the console redraws everything given since its last draw: the head and the last lines again)
    for (int i = 0; i < 24; ++i) WHBLogPrint("");
    for (const std::string& h : g_head) WHBLogPrint(h.c_str());
    WHBLogPrint("");
    for (const std::string& l : g_lines) WHBLogPrint(l.c_str());
    WHBLogConsoleDraw();
}

void say(const char* fmt, ...) __attribute__((format(printf, 1, 2)));
void say(const char* fmt, ...) {
    char s[200];
    va_list a;
    va_start(a, fmt);
    std::vsnprintf(s, sizeof s, fmt, a);
    va_end(a);
    g_lines.emplace_back(s);
    if (g_lines.size() > 10) g_lines.erase(g_lines.begin());
    draw(true);
}

// ---- CRC-32 (the zlib one) ----
uint32_t g_crc_table[256];
void crc_init() {
    for (uint32_t i = 0; i < 256; ++i) {
        uint32_t c = i;
        for (int k = 0; k < 8; ++k) c = (c & 1) != 0 ? 0xedb88320u ^ (c >> 1) : c >> 1;
        g_crc_table[i] = c;
    }
}
uint32_t crc_add(uint32_t crc, const uint8_t* p, size_t n) {
    crc = ~crc;
    while (n-- != 0) crc = g_crc_table[(crc ^ *p++) & 0xff] ^ (crc >> 8);
    return ~crc;
}

// ---- the files ----
std::map<std::string, std::pair<uint64_t, uint32_t>> g_manifest;  // (a path: its size and CRC as written)

void mkdirs(const std::string& path) {  // (the folders up to the file's)
    for (size_t at = std::strlen(kSd); (at = path.find('/', at + 1)) != std::string::npos;) mkdir(path.substr(0, at).c_str(), 0777);
}
void manifest_load() {
    if (std::FILE* f = std::fopen(kManifest, "r")) {
        char path[512];
        unsigned long long size;
        unsigned crc;
        while (std::fscanf(f, "%511s %llu %x", path, &size, &crc) == 3) g_manifest[path] = {size, crc};
        std::fclose(f);
    }
}
void manifest_save() {
    const std::string tmp = std::string(kManifest) + ".tmp";
    mkdirs(tmp);
    if (std::FILE* f = std::fopen(tmp.c_str(), "w")) {
        for (const auto& [p, v] : g_manifest) std::fprintf(f, "%s %llu %08x\n", p.c_str(), static_cast<unsigned long long>(v.first), v.second);
        std::fclose(f);
        std::remove(kManifest);
        std::rename(tmp.c_str(), kManifest);
    }
}
// a path the PC may write: relative, inside our two folders, no ".."
bool allowed(const std::string& p) {
    if (p.empty() || p[0] == '/' || p.find("..") != std::string::npos || p.find('\\') != std::string::npos || p.find(':') != std::string::npos) return false;
    return p.rfind("rac1-native/", 0) == 0 || p.rfind("wiiu/apps/rac1-native/", 0) == 0;
}
uint64_t file_size(const std::string& p) {
    struct stat st;
    return stat(p.c_str(), &st) == 0 ? static_cast<uint64_t>(st.st_size) : ~0ull;
}

// ---- the connection ----
struct Conn {
    int s = -1;
    std::vector<uint8_t> in;  // (received, not yet used)
    bool ok = true;

    bool fill() {
        static uint8_t b[256 * 1024];
        const int n = recv(s, b, sizeof b, 0);
        if (n <= 0) return ok = false;
        in.insert(in.end(), b, b + n);
        return true;
    }
    bool line(std::string& out) {
        for (;;) {
            for (size_t i = 0; i < in.size(); ++i)
                if (in[i] == '\n') {
                    out.assign(reinterpret_cast<const char*>(in.data()), i);
                    in.erase(in.begin(), in.begin() + static_cast<long>(i) + 1);
                    return true;
                }
            if (in.size() > 4096 || !fill()) return false;
        }
    }
    void send_line(const char* fmt, ...) __attribute__((format(printf, 2, 3))) {
        char s2[600];
        va_list a;
        va_start(a, fmt);
        const int n = std::vsnprintf(s2, sizeof s2 - 1, fmt, a);
        va_end(a);
        s2[n] = '\n';
        for (int at = 0; at <= n;) {
            const int k = ::send(s, s2 + at, n + 1 - at, 0);
            if (k <= 0) {
                ok = false;
                return;
            }
            at += k;
        }
    }
};

struct Totals {
    uint64_t bytes = 0, files = 0;
    OSTime t0 = 0;
} g_tot;

// PUT: the file into <path>.part (resumed from what a cut transfer left when its size and CRC were the same), checked, renamed
void put(Conn& c, const std::string& path, uint64_t size, uint32_t crc) {
    const std::string full = kSd + path, part = full + ".part", info = full + ".part.info";
    uint64_t offset = 0;
    uint32_t have = 0;
    if (std::FILE* f = std::fopen(info.c_str(), "r")) {  // (a .part of this very file: its size and CRC as announced then)
        unsigned long long s0;
        unsigned c0;
        if (std::fscanf(f, "%llu %x", &s0, &c0) == 2 && s0 == size && c0 == crc) offset = file_size(part);
        std::fclose(f);
    }
    std::unique_ptr<uint8_t[]> buf(new uint8_t[1u << 20]);
    if (offset != ~0ull && offset > 0 && offset <= size) {  // (the CRC of what is there already)
        if (std::FILE* f = std::fopen(part.c_str(), "rb")) {
            for (size_t n; (n = std::fread(buf.get(), 1, 1u << 20, f)) > 0;) have = crc_add(have, buf.get(), n);
            std::fclose(f);
        }
    } else {
        offset = 0;
    }
    mkdirs(full);
    std::FILE* out = std::fopen(part.c_str(), offset > 0 ? "ab" : "wb");
    if (out == nullptr) {
        c.send_line("ERR cannot write %s", path.c_str());
        return;
    }
    if (offset == 0)
        if (std::FILE* f = std::fopen(info.c_str(), "w")) {
            std::fprintf(f, "%llu %08x\n", static_cast<unsigned long long>(size), crc);
            std::fclose(f);
        }
    setvbuf(out, nullptr, _IOFBF, 1u << 20);
    c.send_line("READY %llu", static_cast<unsigned long long>(offset));
    uint64_t left = size - offset;
    size_t fill = 0;
    bool ok = true;
    OSTime t_net = 0, t_write = 0, t_crc = 0;  // (the network log: where a file's time went)
    while (left > 0 && c.ok) {
        if (c.in.empty()) {
            const OSTime t = OSGetTime();
            if (!c.fill()) break;
            t_net += OSGetTime() - t;
        }
        const size_t k = static_cast<size_t>(std::min<uint64_t>(left, c.in.size()));
        size_t used = 0;
        while (used < k) {  // (into the 1 MB buffer, written when full)
            const size_t m = std::min(k - used, (1u << 20) - fill);
            std::memcpy(buf.get() + fill, c.in.data() + used, m);
            fill += m;
            used += m;
            if (fill == (1u << 20)) {
                OSTime t = OSGetTime();
                have = crc_add(have, buf.get(), fill);
                t_crc += OSGetTime() - t;
                t = OSGetTime();
                ok = ok && std::fwrite(buf.get(), 1, fill, out) == fill;
                t_write += OSGetTime() - t;
                fill = 0;
            }
        }
        c.in.erase(c.in.begin(), c.in.begin() + static_cast<long>(k));
        left -= k;
        g_tot.bytes += k;
        if (OSTicksToMilliseconds(OSGetTime() - g_drawn) > 500) {
            const double s = static_cast<double>(OSTicksToMilliseconds(OSGetTime() - g_tot.t0)) / 1000.0;
            g_head[3] = "Receiving: " + std::to_string(g_tot.files) + " files, " + std::to_string(g_tot.bytes >> 20) + " MB (" +
                        std::to_string(s > 0 ? static_cast<int>((g_tot.bytes >> 20) / s * 10) / 10.0 : 0.0).substr(0, 4) + " MB/s)";
            draw();
        }
    }
    if (fill > 0) {
        have = crc_add(have, buf.get(), fill);
        ok = ok && std::fwrite(buf.get(), 1, fill, out) == fill;
    }
    ok = std::fclose(out) == 0 && ok;
    WHBLogPrintf("[rx] %s: %llu bytes, network %llu ms, crc %llu ms, write %llu ms", path.c_str(), static_cast<unsigned long long>(size),
                 static_cast<unsigned long long>(OSTicksToMilliseconds(t_net)), static_cast<unsigned long long>(OSTicksToMilliseconds(t_crc)),
                 static_cast<unsigned long long>(OSTicksToMilliseconds(t_write)));
    if (left > 0) return;  // (the connection dropped: the .part kept for the next PUT)
    if (!ok || have != crc) {
        std::remove(part.c_str());
        std::remove(info.c_str());
        c.send_line("ERR %s %s", path.c_str(), ok ? "crc mismatch" : "write failed (SD card full?)");
        say("Failed: %s", path.c_str());
        return;
    }
    std::remove(full.c_str());
    if (std::rename(part.c_str(), full.c_str()) != 0) {
        c.send_line("ERR cannot rename %s", path.c_str());
        return;
    }
    std::remove(info.c_str());
    g_manifest[path] = {size, crc};
    ++g_tot.files;
    if (g_tot.files % 200 == 0) manifest_save();
    c.send_line("OK");
}

// PUTS: a small file (the PC sends it right after the line) straight to its name, no resume: one create on the card instead of
// a .part, its .info and a rename (~0.35 s a file on the SD card's FAT)
void puts_small(Conn& c, const std::string& path, uint64_t size, uint32_t crc) {
    const std::string full = kSd + path;
    mkdirs(full);
    std::FILE* out = std::fopen(full.c_str(), "wb");
    uint64_t left = size;
    uint32_t have = 0;
    bool ok = out != nullptr;
    while (left > 0 && c.ok) {
        if (c.in.empty() && !c.fill()) break;
        const size_t k = static_cast<size_t>(std::min<uint64_t>(left, c.in.size()));
        have = crc_add(have, c.in.data(), k);
        if (ok) ok = std::fwrite(c.in.data(), 1, k, out) == k;
        c.in.erase(c.in.begin(), c.in.begin() + static_cast<long>(k));
        left -= k;
        g_tot.bytes += k;
    }
    if (out != nullptr) ok = std::fclose(out) == 0 && ok;
    if (left > 0) {
        std::remove(full.c_str());
        return;
    }
    if (!ok || have != crc) {
        std::remove(full.c_str());
        g_manifest.erase(path);
        c.send_line("ERR %s %s", path.c_str(), ok ? "crc mismatch" : "write failed (SD card full?)");
        say("Failed: %s", path.c_str());
        return;
    }
    g_manifest[path] = {size, crc};
    ++g_tot.files;
    if (g_tot.files % 500 == 0) manifest_save();
    if (OSTicksToMilliseconds(OSGetTime() - g_drawn) > 500) {
        const double sec = static_cast<double>(OSTicksToMilliseconds(OSGetTime() - g_tot.t0)) / 1000.0;
        char line[160];
        std::snprintf(line, sizeof line, "Receiving: %llu files, %llu MB (%.1f MB/s)", static_cast<unsigned long long>(g_tot.files),
                      static_cast<unsigned long long>(g_tot.bytes >> 20), sec > 0 ? (g_tot.bytes >> 20) / sec : 0.0);
        g_head[3] = line;
        draw();
    }
    c.send_line("OK");
}

bool g_launch = false, g_quit = false;

void serve(Conn& c, uint32_t code) {
    std::string l;
    bool hello = false;
    while (c.ok && c.line(l)) {
        char cmd[16] = {}, path[512] = {};
        unsigned long long a = 0;
        unsigned b = 0;
        std::sscanf(l.c_str(), "%15s", cmd);
        if (!hello) {
            unsigned proto = 0, given = 0;
            if (std::sscanf(l.c_str(), "HELLO %u %u", &proto, &given) == 2 && proto == 1 && given == code) {
                hello = true;
                g_tot = Totals{};
                g_tot.t0 = OSGetTime();
                c.send_line("OK rac1-receiver %s", kVersion);
                say("PC connected.");
            } else {
                c.send_line("ERR wrong pairing code or protocol");
                say("A PC gave a wrong pairing code.");
                return;
            }
        } else if (std::strcmp(cmd, "STAT") == 0 && std::sscanf(l.c_str(), "STAT %511s", path) == 1 && allowed(path)) {
            const auto it = g_manifest.find(path);
            const uint64_t on_card = file_size(kSd + std::string(path));
            if (it != g_manifest.end() && on_card == it->second.first)
                c.send_line("SIZE %llu %08x", static_cast<unsigned long long>(it->second.first), it->second.second);
            else if (const uint64_t p = file_size(kSd + std::string(path) + ".part"); p != ~0ull)
                c.send_line("PART %llu", static_cast<unsigned long long>(p));
            else
                c.send_line("NONE");
        } else if (std::strcmp(cmd, "PUT") == 0 && std::sscanf(l.c_str(), "PUT %511s %llu %x", path, &a, &b) == 3 && allowed(path)) {
            put(c, path, a, b);
        } else if (std::strcmp(cmd, "PUTS") == 0 && std::sscanf(l.c_str(), "PUTS %511s %llu %x", path, &a, &b) == 3 && allowed(path)) {
            puts_small(c, path, a, b);
        } else if (std::strcmp(cmd, "MANIFEST") == 0) {
            std::string out;
            size_t n = 0;
            for (const auto& [p, v] : g_manifest) {  // (as recorded: a stat of each on the card would cost ~15 ms a file)
                char row[600];
                std::snprintf(row, sizeof row, "%s %llu %08x\n", p.c_str(), static_cast<unsigned long long>(v.first), v.second);
                out += row;
                ++n;
            }
            c.send_line("N %u", static_cast<unsigned>(n));
            for (size_t at = 0; at < out.size() && c.ok;) {
                const int k = ::send(c.s, out.data() + at, static_cast<int>(std::min<size_t>(out.size() - at, 65536)), 0);
                if (k <= 0) c.ok = false;
                else at += static_cast<size_t>(k);
            }
        } else if (std::strcmp(cmd, "DEL") == 0 && std::sscanf(l.c_str(), "DEL %511s", path) == 1 && allowed(path)) {
            std::remove((kSd + std::string(path)).c_str());
            g_manifest.erase(path);
            c.send_line("OK");
        } else if (std::strcmp(cmd, "FREE") == 0) {
            struct statvfs v;
            const unsigned long long free_b = statvfs(kSd, &v) == 0 ? static_cast<unsigned long long>(v.f_bavail) * v.f_frsize : 0ull;
            c.send_line("FREE %llu", free_b);
        } else if (std::strcmp(cmd, "LAUNCH") == 0) {
            c.send_line("OK");
            g_launch = true;
            return;
        } else if (std::strcmp(cmd, "QUIT") == 0) {
            c.send_line("OK");
            g_quit = true;
            return;
        } else if (std::strcmp(cmd, "BYE") == 0) {
            c.send_line("OK");
            return;
        } else {
            c.send_line("ERR bad command");
        }
    }
}

std::string ip_text() {
    uint32_t ip = 0;
    ACInitialize();
    if (NNResult_IsFailure(ACGetAssignedAddress(&ip)) || ip == 0) return "(no network)";
    char s[32];
    std::snprintf(s, sizeof s, "%u.%u.%u.%u", ip >> 24, (ip >> 16) & 0xff, (ip >> 8) & 0xff, ip & 0xff);
    return s;
}

bool launch_game() {
    if (file_size(kSd + std::string(kGame)) == ~0ull) {
        say("The game is not on the SD card yet: send it from the PC first.");
        return false;
    }
    if (RPXLoader_InitLibrary() != RPX_LOADER_RESULT_SUCCESS) {
        say("Cannot start the game: Aroma's RPX loader is missing.");
        return false;
    }
    say("Starting the game...");
    manifest_save();
    return RPXLoader_LaunchHomebrew(kGame) == RPX_LOADER_RESULT_SUCCESS;
}

// the network's receive memory for our sockets (somemopt: its own buffer, 3 MB, given to the system once - the call blocks
// for the app's life, so on a thread of its own; SO_RUSRBUF on a socket then uses it): the receive went at 0.74 MB/s without
void* g_somem = nullptr;
std::thread g_somem_th;
bool somem_start() {
    g_somem = memalign(0x40, 0x300000);
    if (g_somem == nullptr) return false;
    g_somem_th = std::thread([] { somemopt(SOMEMOPT_REQUEST_INIT, g_somem, 0x300000, SOMEMOPT_FLAGS_BIG_BUFFERS); });
    return somemopt(SOMEMOPT_REQUEST_WAIT_FOR_INIT, nullptr, 0, SOMEMOPT_FLAGS_NONE) != -1;
}
void somem_stop() {
    if (!g_somem_th.joinable()) return;
    somemopt(SOMEMOPT_REQUEST_CANCEL_WAIT, nullptr, 0, SOMEMOPT_FLAGS_NONE);
    g_somem_th.join();
    free(g_somem);
}

}  // namespace

int main() {
    WHBProcInit();
    WHBLogConsoleInit();
    WHBLogUdpInit();
    crc_init();
    manifest_load();
    uint32_t code = 100000u + static_cast<uint32_t>(OSGetTime() % 900000);
    // (testing: rac1-native/.receiver/test_code on the card - written by whoever can already write the card - fixes the code)
    if (std::FILE* f = std::fopen("fs:/vol/external01/rac1-native/.receiver/test_code", "r")) {
        unsigned t = 0;
        if (std::fscanf(f, "%u", &t) == 1 && t >= 100000 && t <= 999999) code = t;
        std::fclose(f);
    }
    g_head[0] = std::string("rac1-native - Wii U receiver ") + kVersion;
    g_head[1] = "On the PC, send to:  " + ip_text() + "   port " + std::to_string(kPort);
    g_head[2] = "Pairing code:  " + std::to_string(code);
    g_head[3] = "Waiting for the PC.   A: start the game   HOME: quit";
    draw(true);

    // (off unless rac1-native/.receiver/somemopt is on the card: with it on, the console hung once at the next launch - 2026-11-09)
    const bool somem = file_size("fs:/vol/external01/rac1-native/.receiver/somemopt") != ~0ull && somem_start();
    WHBLogPrintf("[rx] somemopt %s", somem ? "up" : "failed");
    const int ls = socket(AF_INET, SOCK_STREAM, 0);
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kPort);
    addr.sin_addr.s_addr = htonl(INADDR_ANY);
    const int one = 1;
    setsockopt(ls, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    if (ls < 0 || bind(ls, reinterpret_cast<sockaddr*>(&addr), sizeof addr) != 0 || listen(ls, 1) != 0) say("Cannot listen on port %d.", kPort);
    fcntl(ls, F_SETFL, fcntl(ls, F_GETFL, 0) | O_NONBLOCK);

    bool launched = false;
    while (WHBProcIsRunning()) {
        VPADStatus vs{};
        VPADReadError ve = VPAD_READ_SUCCESS;
        VPADRead(VPAD_CHAN_0, &vs, 1, &ve);
        if (ve == VPAD_READ_SUCCESS && (vs.trigger & VPAD_BUTTON_A) != 0 && !launched) launched = launch_game();
        if (g_quit && !launched) {
            launched = true;  // (nothing more taken: the menu is on its way)
            manifest_save();
            SYSLaunchMenu();
        }
        if (g_launch && !launched) {
            g_launch = false;
            launched = launch_game();
        }
        if (!launched) {
            const int s = accept(ls, nullptr, nullptr);
            if (s >= 0) {
                fcntl(s, F_SETFL, fcntl(s, F_GETFL, 0) & ~O_NONBLOCK);
                setsockopt(s, SOL_SOCKET, SO_WINSCALE, &one, sizeof one);
                setsockopt(s, SOL_SOCKET, SO_TCPSACK, &one, sizeof one);
                if (somem) setsockopt(s, SOL_SOCKET, SO_RUSRBUF, &one, sizeof one);
                const int rcv = somem ? 0x100000 : 256 * 1024;
                setsockopt(s, SOL_SOCKET, SO_RCVBUF, &rcv, sizeof rcv);
                Conn c;
                c.s = s;
                serve(c, code);
                close(s);
                manifest_save();
                const double secs = static_cast<double>(OSTicksToMilliseconds(OSGetTime() - g_tot.t0)) / 1000.0;
                if (g_tot.files > 0) say("Received %llu files, %llu MB in %.0f s.", static_cast<unsigned long long>(g_tot.files),
                                         static_cast<unsigned long long>(g_tot.bytes >> 20), secs);
                g_head[3] = "Waiting for the PC.   A: start the game   HOME: quit";
                draw(true);
            }
        }
        OSSleepTicks(OSMillisecondsToTicks(16));
    }
    if (ls >= 0) close(ls);
    somem_stop();
    manifest_save();
    WHBLogConsoleFree();
    WHBProcShutdown();
    return 0;
}
