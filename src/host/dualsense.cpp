// src/host/dualsense.cpp — the DualSense's output report (see dualsense.h). Host code, not a port.
#include "dualsense.h"

#include <cstdio>
#include <cstdlib>
#include <cstring>

#if defined(__linux__)
#include <cerrno>
#include <dirent.h>
#include <fcntl.h>
#include <linux/hidraw.h>
#include <sys/ioctl.h>
#include <unistd.h>
#elif defined(_WIN32)
#define WIN32_LEAN_AND_MEAN
#include <windows.h>
#include <setupapi.h>
#include <vector>
#endif

namespace rac1::dualsense {

namespace {
constexpr size_t kCommon = 47;
constexpr uint16_t kSony = 0x054c, kDualSense = 0x0ce6, kDualSenseEdge = 0x0df2;

struct Ids {
    uint16_t bus = 0, vendor = 0, product = 0;
};

// orig: (host) the bus, vendor and product of a GLFW GUID (little-endian 16-bit words at bytes 0, 4 and 8: Linux's
// evdev GUIDs and Windows' DirectInput ones alike)
bool guid_ids(const std::string& guid, Ids& ids) {
    if (guid.size() < 20) return false;
    auto byte = [&guid](int i) { return static_cast<uint16_t>(std::strtoul(guid.substr(static_cast<size_t>(i) * 2, 2).c_str(), nullptr, 16)); };
    ids.bus = static_cast<uint16_t>(byte(0) | (byte(1) << 8));
    ids.vendor = static_cast<uint16_t>(byte(4) | (byte(5) << 8));
    ids.product = static_cast<uint16_t>(byte(8) | (byte(9) << 8));
    return true;
}

struct Device {
    std::string guid;               // the pad opened (or tried)
    bool bt = false;                // Bluetooth: report 0x31
    uint8_t seq = 0;                // its sequence number
    bool written = false;           // `last` holds the last common part written
    uint8_t last[kCommon] = {};
    bool v2 = true;                 // the firmware's rumble (dualsense.h)
    uint8_t neutral[3] = {0, 0, 128};  // the light bar before the game took it (hid-playstation's default: blue)
    std::FILE* fake = nullptr;      // RAC1_DUALSENSE_OUT
#if defined(__linux__)
    int fd = -1;
#elif defined(_WIN32)
    HANDLE h = INVALID_HANDLE_VALUE;
    size_t out_len = 0;             // the device's output report length (WriteFile takes exactly that many bytes)
    OVERLAPPED ov{};
    bool pending = false;           // a write not finished yet
    std::vector<uint8_t> buf;
#endif
} g_dev;

// orig: (host) the version of the firmware's update (feature report 0x20, its bytes 44..45, as hid-playstation's
// dualsense_get_firmware_info) chooses the rumble: from 2.21 on (0x0215), and always on the Edge, the newer one
void firmware(const uint8_t* buf, size_t n, uint16_t product) {
    if (product == kDualSenseEdge) {
        g_dev.v2 = true;
    } else if (n >= 46 && buf[0] == 0x20) {
        const unsigned update = buf[44] | (buf[45] << 8);
        g_dev.v2 = update >= 0x0215;
    }
}
}  // namespace

// orig: (host) (dualsense.h)
Trigger trigger(int kind, uint8_t a, uint8_t b, uint8_t c) {
    Trigger t;
    switch (kind) {
        case 1:  // resistance from a position
            t.mode = 0x01;
            t.p[0] = a;
            t.p[1] = b;
            break;
        case 2:  // a section: stiff from `a` to `b`, then giving way
            t.mode = 0x02;
            t.p[0] = a;
            t.p[1] = b;
            t.p[2] = c;
            break;
        case 3:  // vibration past a position
            t.mode = 0x06;
            t.p[0] = a;
            t.p[1] = b;
            t.p[2] = c;
            break;
        default:
            break;  // off (0x05)
    }
    return t;
}

// orig: (host) (dualsense.h)
uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n) {
    crc = ~crc;
    for (size_t i = 0; i < n; ++i) {
        crc ^= p[i];
        for (int k = 0; k < 8; ++k) crc = (crc >> 1) ^ (0xedb88320u & (0u - (crc & 1u)));
    }
    return ~crc;
}

// orig: (host) (dualsense.h)
void common(const State& s, uint8_t* out) {
    std::memset(out, 0, kCommon);
    out[0] = 0x02;  // haptics select: the actuators as rumble motors
    if (s.vibration_v2) out[38] |= 0x04;
    else out[0] |= 0x01;
    out[2] = s.right;
    out[3] = s.left;
    if (s.triggers) {
        out[0] |= 0x04 | 0x08;
        out[10] = s.r2.mode;
        std::memcpy(out + 11, s.r2.p, 10);
        out[21] = s.l2.mode;
        std::memcpy(out + 22, s.l2.p, 10);
    }
    if (s.lightbar) {
        out[1] |= 0x04;
        out[44] = s.red;
        out[45] = s.green;
        out[46] = s.blue;
    }
}

// orig: (host) (dualsense.h)
void report_usb(const State& s, uint8_t* out) {
    std::memset(out, 0, kUsbSize);
    out[0] = 0x02;
    common(s, out + 1);
}

// orig: (host) (dualsense.h)
void report_bt(const State& s, uint8_t seq, uint8_t* out) {
    std::memset(out, 0, kBtSize);
    out[0] = 0x31;
    out[1] = static_cast<uint8_t>((seq & 0xf) << 4);
    out[2] = 0x10;
    common(s, out + 3);
    const uint8_t seed = 0xa2;  // (the Bluetooth HID header of an output report)
    const uint32_t c = crc32(crc32(0, &seed, 1), out, kBtSize - 4);
    out[74] = static_cast<uint8_t>(c);
    out[75] = static_cast<uint8_t>(c >> 8);
    out[76] = static_cast<uint8_t>(c >> 16);
    out[77] = static_cast<uint8_t>(c >> 24);
}

// orig: (host) (dualsense.h)
bool is_dualsense(const std::string& guid) {
    if (std::getenv("RAC1_DUALSENSE_OUT") != nullptr) return !guid.empty();
    Ids ids;
    return guid_ids(guid, ids) && ids.vendor == kSony && (ids.product == kDualSense || ids.product == kDualSenseEdge);
}

// orig: (host) (dualsense.h)
void lightbar_neutral(uint8_t& r, uint8_t& g, uint8_t& b) {
    r = g_dev.neutral[0];
    g = g_dev.neutral[1];
    b = g_dev.neutral[2];
}

#if defined(__linux__)
namespace {
// orig: (host) a sysfs file's first line
std::string read_line(const std::string& path) {
    std::FILE* f = std::fopen(path.c_str(), "r");
    if (f == nullptr) return {};
    char buf[256] = {};
    const bool ok = std::fgets(buf, sizeof buf, f) != nullptr;
    std::fclose(f);
    if (!ok) return {};
    std::string s = buf;
    while (!s.empty() && (s.back() == '\n' || s.back() == '\r')) s.pop_back();
    return s;
}

// orig: (host) the light bar's colour the kernel set (its LED class device, "<input>:rgb:indicator": multi_intensity)
void read_neutral(const std::string& dev) {
    DIR* d = opendir((dev + "/leds").c_str());
    if (d == nullptr) return;
    while (const dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name.find(":rgb:indicator") == std::string::npos) continue;
        unsigned r = 0, g = 0, b = 0;
        if (std::sscanf(read_line(dev + "/leds/" + name + "/multi_intensity").c_str(), "%u %u %u", &r, &g, &b) == 3) {
            g_dev.neutral[0] = static_cast<uint8_t>(r > 255 ? 255 : r);
            g_dev.neutral[1] = static_cast<uint8_t>(g > 255 ? 255 : g);
            g_dev.neutral[2] = static_cast<uint8_t>(b > 255 ? 255 : b);
        }
        break;
    }
    closedir(d);
}
}  // namespace

// orig: (host) (dualsense.h)
bool open(const std::string& guid) {
    close();
    g_dev = Device{};
    g_dev.guid = guid;
    if (const char* path = std::getenv("RAC1_DUALSENSE_OUT")) {
        g_dev.fake = std::fopen(path, "ab");
        std::printf("[rumble] DualSense output reports to %s (testing)\n", path);
        std::fflush(stdout);
        return g_dev.fake != nullptr;
    }
    Ids ids;
    if (!guid_ids(guid, ids)) return false;
    DIR* d = opendir("/sys/class/hidraw");
    if (d == nullptr) return false;
    std::string node, why = "no hidraw node";
    while (const dirent* e = readdir(d)) {
        const std::string name = e->d_name;
        if (name.rfind("hidraw", 0) != 0) continue;
        const std::string dev = "/sys/class/hidraw/" + name + "/device";
        std::FILE* f = std::fopen((dev + "/uevent").c_str(), "r");
        if (f == nullptr) continue;
        unsigned bus = 0, vendor = 0, product = 0;
        char line[256];
        bool match = false;
        while (std::fgets(line, sizeof line, f) != nullptr)
            if (std::sscanf(line, "HID_ID=%x:%x:%x", &bus, &vendor, &product) == 3)
                match = vendor == ids.vendor && product == ids.product && (bus == ids.bus || ids.bus == 0);
        std::fclose(f);
        if (!match) continue;
        const std::string path = "/dev/" + name;
        const int fd = ::open(path.c_str(), O_RDWR | O_NONBLOCK | O_CLOEXEC);
        if (fd < 0) {
            why = path + ": " + std::strerror(errno) + (errno == EACCES ? " (no udev rule gives this user the pad's hidraw node)" : "");
            continue;
        }
        g_dev.fd = fd;
        g_dev.bt = bus == 0x05;
        node = path;
        uint8_t buf[64] = {0x20};
        const int n = ioctl(fd, HIDIOCGFEATURE(sizeof buf), buf);
        firmware(buf, n > 0 ? static_cast<size_t>(n) : 0, ids.product);
        read_neutral(dev);
        break;
    }
    closedir(d);
    if (g_dev.fd < 0) {
        std::printf("[rumble] DualSense: %s; the kernel's force feedback instead (no adaptive triggers)\n", why.c_str());
        std::fflush(stdout);
        return false;
    }
    std::printf("[rumble] %s: DualSense (%s, %s rumble), output reports\n", node.c_str(), g_dev.bt ? "Bluetooth" : "USB",
                g_dev.v2 ? "firmware 2.21+" : "older firmware");
    std::fflush(stdout);
    return true;
}

// orig: (host) (dualsense.h)
bool write(const State& s) {
    State st = s;
    st.vibration_v2 = g_dev.v2;
    uint8_t c[kCommon];
    common(st, c);
    if (g_dev.written && std::memcmp(c, g_dev.last, kCommon) == 0) return true;
    uint8_t buf[kBtSize];
    if (g_dev.fake != nullptr) {
        report_usb(st, buf);
        std::fwrite(buf, 1, kUsbSize, g_dev.fake);
        std::fflush(g_dev.fake);
    } else if (g_dev.fd >= 0) {
        size_t n = kUsbSize;
        if (g_dev.bt) {
            report_bt(st, g_dev.seq, buf);
            n = kBtSize;
        } else {
            report_usb(st, buf);
        }
        if (::write(g_dev.fd, buf, n) != static_cast<ssize_t>(n)) {
            if (errno == ENODEV) close();
            return false;  // (EAGAIN: tried again next frame)
        }
        g_dev.seq = static_cast<uint8_t>((g_dev.seq + 1) & 0xf);
    } else {
        return false;
    }
    std::memcpy(g_dev.last, c, kCommon);
    g_dev.written = true;
    return true;
}

// orig: (host) (dualsense.h)
void close() {
    if (g_dev.fd >= 0) ::close(g_dev.fd);
    g_dev.fd = -1;
    if (g_dev.fake != nullptr) std::fclose(g_dev.fake);
    g_dev.fake = nullptr;
    g_dev.written = false;
}

#elif defined(_WIN32)
namespace {
// hid.dll's (hidsdi.h / hidpi.h, declared here: not every toolchain ships the headers)
struct HiddAttributes {
    ULONG Size;
    USHORT VendorID, ProductID, VersionNumber;
};
struct HidpCaps {
    USHORT Usage, UsagePage, InputReportByteLength, OutputReportByteLength, FeatureReportByteLength;
    USHORT Reserved[17];
    USHORT Counts[10];
};
struct Hid {
    void(WINAPI* get_guid)(GUID*) = nullptr;
    BOOLEAN(WINAPI* attributes)(HANDLE, HiddAttributes*) = nullptr;
    BOOLEAN(WINAPI* preparsed)(HANDLE, void**) = nullptr;
    BOOLEAN(WINAPI* free_preparsed)(void*) = nullptr;
    LONG(WINAPI* caps)(void*, HidpCaps*) = nullptr;
    BOOLEAN(WINAPI* feature)(HANDLE, void*, ULONG) = nullptr;
    decltype(&SetupDiGetClassDevsW) devs = nullptr;
    decltype(&SetupDiEnumDeviceInterfaces) enum_if = nullptr;
    decltype(&SetupDiGetDeviceInterfaceDetailW) detail = nullptr;
    decltype(&SetupDiDestroyDeviceInfoList) destroy = nullptr;
    bool ok = false;
};
// orig: (host) hid.dll and setupapi.dll, loaded when first needed
const Hid& hid() {
    static const Hid h = [] {
        Hid r;
        HMODULE m = LoadLibraryA("hid.dll"), s = LoadLibraryA("setupapi.dll");
        if (m == nullptr || s == nullptr) return r;
        auto get = [](HMODULE mod, const char* n) { return reinterpret_cast<void*>(GetProcAddress(mod, n)); };
        r.get_guid = reinterpret_cast<decltype(r.get_guid)>(get(m, "HidD_GetHidGuid"));
        r.attributes = reinterpret_cast<decltype(r.attributes)>(get(m, "HidD_GetAttributes"));
        r.preparsed = reinterpret_cast<decltype(r.preparsed)>(get(m, "HidD_GetPreparsedData"));
        r.free_preparsed = reinterpret_cast<decltype(r.free_preparsed)>(get(m, "HidD_FreePreparsedData"));
        r.caps = reinterpret_cast<decltype(r.caps)>(get(m, "HidP_GetCaps"));
        r.feature = reinterpret_cast<decltype(r.feature)>(get(m, "HidD_GetFeature"));
        r.devs = reinterpret_cast<decltype(r.devs)>(get(s, "SetupDiGetClassDevsW"));
        r.enum_if = reinterpret_cast<decltype(r.enum_if)>(get(s, "SetupDiEnumDeviceInterfaces"));
        r.detail = reinterpret_cast<decltype(r.detail)>(get(s, "SetupDiGetDeviceInterfaceDetailW"));
        r.destroy = reinterpret_cast<decltype(r.destroy)>(get(s, "SetupDiDestroyDeviceInfoList"));
        r.ok = r.get_guid && r.attributes && r.preparsed && r.free_preparsed && r.caps && r.feature && r.devs && r.enum_if && r.detail &&
               r.destroy;
        return r;
    }();
    return h;
}
}  // namespace

// orig: (host) (dualsense.h)
bool open(const std::string& guid) {
    close();
    g_dev = Device{};
    g_dev.guid = guid;
    if (const char* path = std::getenv("RAC1_DUALSENSE_OUT")) {
        g_dev.fake = std::fopen(path, "ab");
        std::printf("[rumble] DualSense output reports to %s (testing)\n", path);
        std::fflush(stdout);
        return g_dev.fake != nullptr;
    }
    Ids ids;
    const Hid& h = hid();
    if (!guid_ids(guid, ids) || !h.ok) return false;
    GUID hid_guid;
    h.get_guid(&hid_guid);
    HDEVINFO set = h.devs(&hid_guid, nullptr, nullptr, DIGCF_PRESENT | DIGCF_DEVICEINTERFACE);
    if (set == INVALID_HANDLE_VALUE) return false;
    SP_DEVICE_INTERFACE_DATA itf{};
    itf.cbSize = sizeof itf;
    for (DWORD i = 0; g_dev.h == INVALID_HANDLE_VALUE && h.enum_if(set, nullptr, &hid_guid, i, &itf); ++i) {
        DWORD size = 0;
        h.detail(set, &itf, nullptr, 0, &size, nullptr);
        if (size == 0) continue;
        std::vector<uint8_t> mem(size);
        auto* det = reinterpret_cast<SP_DEVICE_INTERFACE_DETAIL_DATA_W*>(mem.data());
        det->cbSize = sizeof(SP_DEVICE_INTERFACE_DETAIL_DATA_W);
        if (!h.detail(set, &itf, det, size, nullptr, nullptr)) continue;
        HANDLE dev = CreateFileW(det->DevicePath, GENERIC_READ | GENERIC_WRITE, FILE_SHARE_READ | FILE_SHARE_WRITE, nullptr, OPEN_EXISTING,
                                 FILE_FLAG_OVERLAPPED, nullptr);
        if (dev == INVALID_HANDLE_VALUE) continue;
        HiddAttributes a{};
        a.Size = sizeof a;
        HidpCaps caps{};
        void* pp = nullptr;
        bool match = h.attributes(dev, &a) && a.VendorID == ids.vendor && a.ProductID == ids.product && h.preparsed(dev, &pp);
        if (match) {
            match = h.caps(pp, &caps) >= 0 && caps.OutputReportByteLength >= 48;  // (HIDP_STATUS_SUCCESS is positive)
            h.free_preparsed(pp);
        }
        if (!match) {
            CloseHandle(dev);
            continue;
        }
        g_dev.h = dev;
        g_dev.out_len = caps.OutputReportByteLength;
        g_dev.bt = caps.OutputReportByteLength > 64;  // (USB: 48 bytes; Bluetooth: its biggest report's, past 64)
        g_dev.ov.hEvent = CreateEventW(nullptr, TRUE, TRUE, nullptr);
        std::vector<uint8_t> fbuf(caps.FeatureReportByteLength > 64 ? caps.FeatureReportByteLength : 64, 0);
        fbuf[0] = 0x20;
        firmware(fbuf.data(), h.feature(dev, fbuf.data(), static_cast<ULONG>(fbuf.size())) ? fbuf.size() : 0, ids.product);
    }
    h.destroy(set);
    if (g_dev.h == INVALID_HANDLE_VALUE) {
        std::printf("[rumble] DualSense: no HID device opened; XInput instead (no adaptive triggers)\n");
        std::fflush(stdout);
        return false;
    }
    std::printf("[rumble] DualSense (%s, %s rumble), output reports\n", g_dev.bt ? "Bluetooth" : "USB", g_dev.v2 ? "firmware 2.21+" : "older firmware");
    std::fflush(stdout);
    return true;
}

// orig: (host) (dualsense.h)
bool write(const State& s) {
    State st = s;
    st.vibration_v2 = g_dev.v2;
    uint8_t c[kCommon];
    common(st, c);
    if (g_dev.written && std::memcmp(c, g_dev.last, kCommon) == 0) return true;
    uint8_t buf[kBtSize];
    if (g_dev.fake != nullptr) {
        report_usb(st, buf);
        std::fwrite(buf, 1, kUsbSize, g_dev.fake);
        std::fflush(g_dev.fake);
    } else if (g_dev.h != INVALID_HANDLE_VALUE) {
        if (g_dev.pending) {  // the last write still going: this one waits for a later frame
            DWORD done = 0;
            if (!GetOverlappedResult(g_dev.h, &g_dev.ov, &done, FALSE)) {
                if (GetLastError() == ERROR_IO_INCOMPLETE) return false;
            }
            g_dev.pending = false;
        }
        size_t n = kUsbSize;
        if (g_dev.bt) {
            report_bt(st, g_dev.seq, buf);
            n = kBtSize;
        } else {
            report_usb(st, buf);
        }
        g_dev.buf.assign(g_dev.out_len, 0);
        std::memcpy(g_dev.buf.data(), buf, n < g_dev.out_len ? n : g_dev.out_len);
        ResetEvent(g_dev.ov.hEvent);
        if (!WriteFile(g_dev.h, g_dev.buf.data(), static_cast<DWORD>(g_dev.out_len), nullptr, &g_dev.ov)) {
            if (GetLastError() != ERROR_IO_PENDING) {
                if (GetLastError() == ERROR_DEVICE_NOT_CONNECTED) close();
                return false;
            }
            g_dev.pending = true;
        }
        g_dev.seq = static_cast<uint8_t>((g_dev.seq + 1) & 0xf);
    } else {
        return false;
    }
    std::memcpy(g_dev.last, c, kCommon);
    g_dev.written = true;
    return true;
}

// orig: (host) (dualsense.h)
void close() {
    if (g_dev.h != INVALID_HANDLE_VALUE) {
        if (g_dev.pending) {
            DWORD done = 0;
            if (!GetOverlappedResult(g_dev.h, &g_dev.ov, &done, FALSE) && GetLastError() == ERROR_IO_INCOMPLETE) CancelIo(g_dev.h);
            GetOverlappedResult(g_dev.h, &g_dev.ov, &done, TRUE);
        }
        CloseHandle(g_dev.h);
        if (g_dev.ov.hEvent != nullptr) CloseHandle(g_dev.ov.hEvent);
    }
    g_dev.h = INVALID_HANDLE_VALUE;
    g_dev.ov = OVERLAPPED{};
    g_dev.pending = false;
    if (g_dev.fake != nullptr) std::fclose(g_dev.fake);
    g_dev.fake = nullptr;
    g_dev.written = false;
}

#else
// orig: (host) (dualsense.h): none elsewhere
bool open(const std::string&) { return false; }
// orig: (host) (dualsense.h)
bool write(const State&) { return false; }
// orig: (host) (dualsense.h)
void close() {}
#endif

}  // namespace rac1::dualsense
