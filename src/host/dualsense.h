#pragma once
// src/host/dualsense.h — the DualSense's output report, written to the pad directly (#10). Host code, not a port.
//
// The kernel's force feedback (and XInput) give a pad two rumble strengths and nothing more. The DualSense takes one
// output report that also sets its adaptive triggers and its light bar, so the game writes that report itself, in
// the layout the kernel's hid-playstation (dualsense_output_report_common) and SDL (DS5EffectsState_t) use:
// - USB: report 0x02, 63 bytes: [0] 0x02, [1..47] the common part, the rest 0;
// - Bluetooth: report 0x31, 78 bytes: [0] 0x31, [1] the sequence number << 4, [2] the tag 0x10, [3..49] the common
//   part, [74..77] the CRC-32 (little-endian) of the byte 0xa2 followed by bytes 0..73.
// The common part (offsets from its start):
//   +0 valid_flag0: 0x01 compatible vibration (the old firmwares' rumble), 0x02 haptics select (the two actuators
//      driven as rumble motors, not by the audio haptics), 0x04 the right trigger's effect, 0x08 the left one's;
//   +1 valid_flag1: 0x04 the light bar's colour;
//   +2 the right actuator, +3 the left one (0..255; the kernel's weak and strong motors);
//   +10..+20 the right trigger's effect (R2), +21..+31 the left one's (L2): a mode and its parameters;
//   +38 valid_flag2: 0x04 compatible vibration 2 (the rumble of the firmwares from 2.21 on, and of the DualSense Edge);
//   +44..+46 the light bar's red, green, blue.
// The trigger effects used are the simple ones (as SDL's test program and the community's notes name them):
//   0x05 off; 0x01 resistance [position, force]; 0x02 a section [start, end, force] (the trigger stiff between the two
//   positions, then giving way: a weapon's click); 0x06 vibration [frequency in Hz, amplitude, position].
//
// Where the report goes:
// - Linux: the pad's hidraw node (/dev/hidraw*, found by the bus, vendor and product of GLFW's GUID in
//   /sys/class/hidraw/*/device/uevent). Steam's and the systems' udev rules (uaccess) usually give the seated user
//   access to a DualSense's node; without it the log says so and the kernel's force feedback is used instead;
// - Windows: the HID device (SetupAPI and hid.dll, loaded when first needed), WriteFile with the device's output
//   report length; XInput stays for the Xbox pads;
// - (testing) RAC1_DUALSENSE_OUT=<file>: the reports appended to that file instead (USB layout), and a DualSense taken
//   as connected, in headless runs too.

#include <cstddef>
#include <cstdint>
#include <string>

namespace rac1::dualsense {

struct Trigger {
    uint8_t mode = 0x05;           // the report's mode byte (0x05 off)
    uint8_t p[10] = {};            // its parameters
};

struct State {
    uint8_t left = 0, right = 0;   // the actuators
    Trigger r2, l2;                // the triggers' effects
    bool triggers = false;         // the triggers' effects sent (valid_flag0 0x04 | 0x08)
    bool lightbar = false;         // the light bar's colour sent (valid_flag1 0x04)
    uint8_t red = 0, green = 0, blue = 0;
    bool vibration_v2 = true;      // the firmware's rumble (valid_flag2 0x04), else the old one (valid_flag0 0x01)
};

constexpr size_t kUsbSize = 63, kBtSize = 78;

// orig: (host) a trigger effect: 0 off, 1 resistance (a position, b force), 2 section (a start, b end, c force), 3
// vibration (a frequency in Hz, b amplitude, c position)
Trigger trigger(int kind, uint8_t a, uint8_t b, uint8_t c);

// orig: (host) the CRC-32 (IEEE 802.3, reflected, as zlib's crc32: initial 0xffffffff, final xor) of `n` bytes,
// continuing `crc` (0 to start)
uint32_t crc32(uint32_t crc, const uint8_t* p, size_t n);

// orig: (host) the 47 bytes of the common part
void common(const State& s, uint8_t* out);
// orig: (host) the USB report (kUsbSize bytes)
void report_usb(const State& s, uint8_t* out);
// orig: (host) the Bluetooth report (kBtSize bytes), `seq` its sequence number (0..15)
void report_bt(const State& s, uint8_t seq, uint8_t* out);

// orig: (host) whether GLFW's GUID `guid` is a DualSense (Sony 054c, product 0ce6, or 0df2 the Edge)
bool is_dualsense(const std::string& guid);

// orig: (host) the DualSense whose GLFW GUID is `guid` opened: false when there is no such device or no access (the
// reason logged once a pad)
bool open(const std::string& guid);
// orig: (host) the report for `s` written to the device opened (only when it differs from the last one written)
bool write(const State& s);
// orig: (host) the device let go
void close();
// orig: (host) the light bar's colour before the game took it (the kernel's LED, or its default blue)
void lightbar_neutral(uint8_t& r, uint8_t& g, uint8_t& b);

}  // namespace rac1::dualsense
