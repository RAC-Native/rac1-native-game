// tests/rumble/dualsense_test.cpp — the DualSense output report's layout and its Bluetooth CRC (src/host/dualsense.h),
// checked against the kernel's hid-playstation / SDL layout and against zlib's CRC-32. Host code only, no pad needed:
//   cmake --build build --target dualsense_test && ./build/dualsense_test
#include <host/dualsense.h>

#include <cstdio>
#include <cstring>

namespace ds = rac1::dualsense;

namespace {
int g_failed = 0;

// orig: (none) — test: one check
void check(bool ok, const char* what) {
    std::printf("%s %s\n", ok ? "ok  " : "FAIL", what);
    if (!ok) ++g_failed;
}

// orig: (none) — test: a report's bytes, in hex (for the log, and for a cross-check with another tool)
void dump(const char* name, const uint8_t* p, size_t n) {
    std::printf("%s:", name);
    for (size_t i = 0; i < n; ++i) std::printf(" %02x", p[i]);
    std::printf("\n");
}
}  // namespace

// orig: (none) — test
int main() {
    // the CRC-32: zlib's check value, and the empty input
    const char* nine = "123456789";
    check(ds::crc32(0, reinterpret_cast<const uint8_t*>(nine), 9) == 0xcbf43926u, "crc32(\"123456789\") = 0xcbf43926 (zlib's)");
    check(ds::crc32(0, nullptr, 0) == 0, "crc32 of nothing = 0");
    // in two parts as one
    check(ds::crc32(ds::crc32(0, reinterpret_cast<const uint8_t*>(nine), 4), reinterpret_cast<const uint8_t*>(nine) + 4, 5) == 0xcbf43926u,
          "crc32 continued over two parts");

    ds::State s;
    s.left = 0x80;
    s.right = 0x40;
    s.triggers = true;
    s.r2 = ds::trigger(2, 0x40, 0x90, 0xa0);
    s.l2 = ds::trigger(1, 0x40, 0x50, 0);
    s.lightbar = true;
    s.red = 1;
    s.green = 2;
    s.blue = 3;
    s.vibration_v2 = true;

    uint8_t u[ds::kUsbSize];
    ds::report_usb(s, u);
    dump("usb", u, sizeof u);
    // (the USB report: id 0x02, then dualsense_output_report_common at +1)
    check(u[0] == 0x02, "usb: report id 0x02");
    check(u[1] == (0x02 | 0x04 | 0x08), "usb: valid_flag0 = haptics select | right trigger | left trigger (v2: no compatible vibration bit)");
    check(u[2] == 0x04, "usb: valid_flag1 = light bar");
    check(u[3] == 0x40 && u[4] == 0x80, "usb: motor_right at +3, motor_left at +4");
    check(u[11] == 0x02 && u[12] == 0x40 && u[13] == 0x90 && u[14] == 0xa0, "usb: R2 effect at +11 (section 0x02 start end force)");
    check(u[22] == 0x01 && u[23] == 0x40 && u[24] == 0x50, "usb: L2 effect at +22 (resistance 0x01 position force)");
    check(u[39] == 0x04, "usb: valid_flag2 = compatible vibration 2 (+39)");
    check(u[45] == 1 && u[46] == 2 && u[47] == 3, "usb: light bar red green blue at +45..+47");
    bool rest = true;
    for (size_t i = 48; i < sizeof u; ++i) rest = rest && u[i] == 0;
    check(rest, "usb: bytes 48..62 zero");

    ds::State old = s;
    old.vibration_v2 = false;
    old.triggers = false;
    old.lightbar = false;
    ds::report_usb(old, u);
    check(u[1] == (0x01 | 0x02) && u[2] == 0 && u[39] == 0, "usb, older firmware, no triggers or light bar: valid_flag0 = 0x03, flag1 = flag2 = 0");
    check(u[11] == 0 && u[22] == 0 && u[45] == 0, "usb, no triggers or light bar: their bytes 0");

    ds::State off = s;
    off.r2 = ds::trigger(0, 9, 9, 9);
    ds::report_usb(off, u);
    check(u[11] == 0x05 && u[12] == 0 && u[13] == 0, "usb: an effect off is mode 0x05, no parameters");
    const ds::Trigger vib = ds::trigger(3, 30, 0xa0, 0x30);
    check(vib.mode == 0x06 && vib.p[0] == 30 && vib.p[1] == 0xa0 && vib.p[2] == 0x30, "vibration effect: 0x06 frequency amplitude position");

    uint8_t b[ds::kBtSize];
    ds::report_bt(s, 5, b);
    dump("bt", b, sizeof b);
    check(b[0] == 0x31 && b[1] == 0x50 && b[2] == 0x10, "bt: report id 0x31, sequence 5 << 4, tag 0x10");
    check(b[3] == (0x02 | 0x04 | 0x08) && b[5] == 0x40 && b[6] == 0x80 && b[13] == 0x02 && b[41] == 0x04 && b[47] == 1,
          "bt: the common part at +3");
    uint8_t seeded[1 + 74];
    seeded[0] = 0xa2;
    std::memcpy(seeded + 1, b, 74);
    const uint32_t crc = ds::crc32(0, seeded, sizeof seeded);
    check((b[74] | (b[75] << 8) | (b[76] << 16) | (static_cast<uint32_t>(b[77]) << 24)) == crc, "bt: CRC-32 of 0xa2 + bytes 0..73, little-endian at +74");
    check(crc == 0x2ee8deaau, "bt: the CRC is zlib's (Python: zlib.crc32(b'\\xa2' + report[:74]) = 0x2ee8deaa)");

    check(ds::is_dualsense("030000004c050000e60c000000016800") && ds::is_dualsense("050000004c050000e60c000000810000") &&
              ds::is_dualsense("030000004c050000f20d000000006800") && !ds::is_dualsense("030000004c050000cc09000000016800") &&
              !ds::is_dualsense("78696e70757401000000000000000000") && !ds::is_dualsense(""),
          "is_dualsense: the DualSense and the Edge (USB, Bluetooth), not a DualShock 4 or an XInput pad");

    std::printf("%s\n", g_failed == 0 ? "all passed" : "FAILED");
    return g_failed == 0 ? 0 : 1;
}
