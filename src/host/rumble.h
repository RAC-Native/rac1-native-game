#pragma once
// src/host/rumble.h — the gamepad's vibration motors (#10). Host code, not a port: the game's side is
// src/game/enhance/vibration.h (the events, the motors' strengths); this drives a pad's motors with them.
//
// GLFW reads gamepads but has no force feedback, so the motors are driven directly, with what each system has, no new
// library:
// - Linux: the kernel's force feedback on the pad's event device (/dev/input/event*, FF_RUMBLE: xpad for Xbox pads,
//   hid-playstation for the DualShock 4 and DualSense, hid-nintendo, ...), the device found by GLFW's GUID for the pad
//   (bus, vendor, product, version: the same numbers) among those with FF_RUMBLE; the desktop's rules give the seat's user
//   read and write access to a pad's event device (as Steam and SDL use it);
// - Windows: XInput's XInputSetState (xinput1_4.dll, else xinput9_1_0.dll, loaded when first needed) on every connected
//   XInput pad. A DualShock 4 or DualSense on Windows speaks HID, not XInput: it vibrates through Steam Input or DS4Windows
//   (which present it as an XInput pad), not by itself;
// - elsewhere (macOS): nothing.
// The motors are set when their strength changes (in steps of 1/64) and again every half second while they run, each
// setting lasting one second at most: a game that stops calling leaves the pad still within a second.

#include <string>

namespace rac1::rumble {

// orig: (host) the motors of the pad with GLFW GUID `guid` ("" none: every motor stopped), `low` the big slow one, `high`
// the small fast one, 0..1. Once a host frame, the main thread.
void set(const std::string& guid, float low, float high);

// orig: (host) every motor stopped and the devices closed (the game's end)
void shutdown();

}  // namespace rac1::rumble
