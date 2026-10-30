#pragma once
// src/host/rumble.h — the gamepad's vibration motors, and a DualSense's adaptive triggers and light bar (#10). Host
// code, not a port: the game's side is src/game/enhance/vibration.h (the events, the strengths); this drives the pad.
//
// GLFW reads gamepads but has no force feedback, so the pad is driven directly, with what each system has, no new
// library:
// - a DualSense (Linux and Windows): its own output report (src/host/dualsense.h), which sets the two actuators (as
//   left and right: a positional event is felt from its side), the adaptive triggers and the light bar. Without access
//   to the device (Linux: no udev rule for its hidraw node) the paths below are used, rumble only;
// - Linux: the kernel's force feedback on the pad's event device (/dev/input/event*, FF_RUMBLE: xpad for Xbox pads,
//   hid-playstation for the DualShock 4 and DualSense, hid-nintendo, ...), the device found by GLFW's GUID for the pad
//   (bus, vendor, product, version: the same numbers) among those with FF_RUMBLE; the desktop's rules give the seat's
//   user read and write access to a pad's event device (as Steam and SDL use it). One effect is uploaded, its strengths
//   updated in place and played again. The motors are stopped by uploading 0, playing it, then stopping it: the
//   kernel's ff-memless (behind most pad drivers) sends nothing to the pad for a stop that comes right after an upload,
//   and the pad then kept its last strength until the next event;
// - Windows: XInput's XInputSetState (xinput1_4.dll, else xinput9_1_0.dll, loaded when first needed) on every connected
//   XInput pad (and a DualShock 4 or a DualSense that Steam Input or DS4Windows presents as one);
// - elsewhere (macOS): nothing.
// The force feedback motors are set when their strength changes (in steps of 1/64) and again every half second while
// they run, each setting lasting one second at most: a game that stops calling leaves the pad still within a second.

#include <cstdint>
#include <string>

namespace rac1::rumble {

struct Trigger {
    uint8_t kind = 0;             // 0 off, 1 resistance (a position, b force), 2 section (a start, b end, c force),
    uint8_t a = 0, b = 0, c = 0;  // 3 vibration (a frequency in Hz, b amplitude, c position); 0..255 each
};

struct Output {
    float low = 0.0f, high = 0.0f;    // the classic pads' motors (the big slow one, the small fast one), 0..1
    float left = 0.0f, right = 0.0f;  // a DualSense's actuators, 0..1 (positional events panned between them)
    bool dualsense = false;           // the triggers and the light bar driven (Adaptive Triggers on)
    Trigger r2, l2;                   // their effects
    float health = -1.0f;             // the light bar: 0..1 the health's colour, below 0 the pad's own colour
};

// orig: (host) the pad with GLFW GUID `guid` ("" none: every motor stopped, the triggers and the light bar given back)
// driven with `out`. Once a host frame, the main thread (headless with RAC1_DUALSENSE_OUT: once a game frame).
void set(const std::string& guid, const Output& out);

// orig: (host) every motor stopped, the triggers and the light bar given back, the devices closed (the game's end)
void shutdown();

}  // namespace rac1::rumble
