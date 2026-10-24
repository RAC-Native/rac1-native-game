#pragma once
// src/host/host_input.h — the host's controls. Host code, not a port: what the PC's keyboard,
// mouse and gamepads press on the PS2 pad. The game still reads its DualShock 2 (g_pad_state 0x13c940, filled by the
// ported pad code from the 6-byte packet g_host_pad_packet); only how that packet is made changes here.
//
// The data model is a bindings table, PS2 control -> host inputs, stored as text in
// ~/.config/rac1-native/controls.txt. The game's actions are the PS2 buttons
// themselves (the game maps buttons to actions per context): the label and
// action strings of control_info() are for the remapping page.
//
// This header and host_input.cpp do not call GLFW (only its key / button numbers): the mapping is unit-tested
//. The GLFW side (polling, the controller database, the cursor) is in orig_main.cpp.

#include <array>
#include <cstdint>
#include <string>
#include <string_view>
#include <vector>

namespace rac1::host_input {

// The PS2 pad's controls: its 16 buttons, then the two sticks' half axes.
enum class PadControl : uint8_t {
    kSelect, kL3, kR3, kStart, kUp, kRight, kDown, kLeft,
    kL2, kR2, kL1, kR1, kTriangle, kCircle, kCross, kSquare,
    kLeftStickLeft, kLeftStickRight, kLeftStickUp, kLeftStickDown,
    kRightStickLeft, kRightStickRight, kRightStickUp, kRightStickDown,
    kCount
};
constexpr int kControlCount = static_cast<int>(PadControl::kCount);
constexpr int kButtonCount = 16;

struct ControlInfo {
    const char* name;    // the config file's name ("cross", "left_stick_up")
    const char* label;   // shown to the player ("Cross", "Left stick up")
    const char* action;  // what the game does with it, where known ("" otherwise)
    uint16_t bit;        // buttons: the bit of the host's held mask (pad_packet: high byte select..left, low byte L2..square)
};
const ControlInfo& control_info(PadControl c);
inline const ControlInfo& control_info(int i) { return control_info(static_cast<PadControl>(i)); }

// A host input. Codes are GLFW's: GLFW_KEY_*, GLFW_MOUSE_BUTTON_*, GLFW_GAMEPAD_BUTTON_*, GLFW_GAMEPAD_AXIS_*.
enum class InputKind : uint8_t { kNone, kKey, kMouseButton, kMouseAxis, kPadButton, kPadAxis };
struct HostInput {
    InputKind kind = InputKind::kNone;
    int code = 0;     // kMouseAxis: 0 x, 1 y
    int sign = 1;     // axes: +1 the positive half, -1 the negative
    bool operator==(const HostInput&) const = default;
};
// "key:space", "mouse:left", "mouse:+x", "pad:a", "pad:-lefty", "pad:+righttrigger" (the SDL names of the
// gamecontrollerdb format for the pad)
std::string input_to_string(const HostInput& in);
bool input_from_string(std::string_view s, HostInput& out);

struct Bindings {
    std::array<std::vector<HostInput>, kControlCount> map;
    float mouse_sensitivity = 1.0f;  // 1.0: 20 pixels in a game frame tilt the stick fully
    bool mouse_look = true;          // a click in the window captures the cursor for the mouse's stick bindings
    bool invert_look_y = false;      // the right stick's up / down swapped, whatever drives it (pad, mouse, keys)
};
Bindings default_bindings();
std::string bindings_to_text(const Bindings& b);
// Controls missing from the text keep their default bindings; "cross =" (nothing) leaves a control unbound.
// Unknown lines / inputs are skipped with a message in *warnings.
Bindings bindings_from_text(std::string_view text, std::vector<std::string>* warnings = nullptr);

// The controls file: $RAC1_CONTROLS_FILE, else $XDG_CONFIG_HOME/rac1-native/controls.txt, else
// ~/.config/rac1-native/controls.txt (Windows: %APPDATA%\rac1-native\controls.txt; platform::config_dir).
std::string controls_path();
// Reads the file; when it does not exist, writes the defaults there (the player edits it) and returns them.
Bindings load_or_create(const std::string& path);
bool save(const Bindings& b, const std::string& path);

// What the host's devices hold, sampled once per game frame.
struct HostState {
    std::array<bool, 349> keys{};          // by GLFW_KEY_* (GLFW_KEY_LAST 348)
    std::array<bool, 8> mouse_buttons{};
    float mouse_dx = 0.0f, mouse_dy = 0.0f;  // pixels since the last game frame (captured cursor only)
    bool pad_present = false;
    std::array<bool, 15> pad_buttons{};    // by GLFW_GAMEPAD_BUTTON_*
    std::array<float, 6> pad_axes{0, 0, 0, 0, -1.0f, -1.0f};  // by GLFW_GAMEPAD_AXIS_* (triggers rest at -1)
};
// One gamepad's buttons and axes (GLFW's GLFWgamepadstate) merged into s's pad (issue #98: every mapped gamepad is read,
// so a silent or virtual one listed first does not hide the one played with): the buttons OR'ed, each stick the one
// deflected furthest (both of its axes from the same pad), each trigger the one pressed furthest. Merged into the
// HostState's resting pad (no button, sticks centred, triggers at -1), one pad gives exactly its own state.
void merge_pad(HostState& s, const bool (&buttons)[15], const float (&axes)[6]);
// Whether a pad holds anything: a button, a stick past 0.3, a trigger past -0.4 (any_held's thresholds)
bool pad_held(const bool (&buttons)[15], const float (&axes)[6]);
struct PadOutput {
    uint16_t held = 0;  // the host's held mask (ControlInfo::bit)
    float lx = 0, ly = 0, rx = 0, ry = 0;  // -1..1, up negative
};
// How much an input is pressed, 0..1.
float input_value(const HostInput& in, const HostState& s, const Bindings& b);
PadOutput compute(const Bindings& b, const HostState& s);
// The round stick's travel taken to the DualShock's square one (as the pad reports its corners).
void stick_to_square(float& x, float& y);
// The pad's 6-byte packet: 2 button bytes active low, the right stick, the left stick (0..255, 0x7f centred).
void to_packet(const PadOutput& o, uint8_t packet[6]);

// --- The remapping page (src/game/enhance/controls.cpp shows it) ---
// A device's inputs: the gamepad's, or the keyboard's and the mouse's. The page shows and rebinds one device at a time;
// the other device's inputs of a control are left as they are.
enum class Device : uint8_t { kPad, kKeyboardMouse };
bool on_device(const HostInput& in, Device d);
std::vector<HostInput> device_inputs(const Bindings& b, PadControl c, Device d);

// The page's rows: a PS2 button (one control), or a group of four (the D-pad, a stick: up, down, left, right).
struct RemapRow {
    const char* name;        // shown ("Cross", "Left stick"); the game's button glyph is added by the page
    PadControl controls[4];  // count 1: [0]; count 4: up, down, left, right
    int count;
};
constexpr int kRemapRowCount = 15;
const RemapRow& remap_row(int i);

// The pad's face labels: the database's names are positions (a = bottom); shown as the pad's own names
enum class PadStyle : uint8_t { kPlayStation, kXbox, kNintendo, kGeneric };
PadStyle pad_style(std::string_view gamepad_name);  // from GLFW's gamepad name ("PS5 Controller", "Xbox Controller", ...)
// An input as the page shows it. kPlayStation uses the game font's button glyphs (0x10 Cross, 0x11 Circle, 0x12 Triangle,
// 0x13 Square, 0x14 L1, 0x15 R1, 0x16 L2, 0x17 R2, 0x18 right stick, 0x19 left stick)
std::string input_label(const HostInput& in, PadStyle style);
// A row's value: its device inputs ("Space", "Escape, Enter", "WASD", "Mouse", "Left Stick", "-" when unbound)
std::string row_label(const Bindings& b, int row, Device d, PadStyle style);

// Control `c`'s inputs on device `d` replaced by `in`. When `in` was bound to other controls, they get `c`'s old inputs
// on that device in its place (a swap); their indices are added to *swapped. The other device's inputs are kept.
void rebind(Bindings& b, PadControl c, Device d, const HostInput& in, std::vector<int>* swapped = nullptr);
// A device's inputs back to the defaults (the other device's kept); the keyboard / mouse: the mouse's sensitivity too
void reset_device(Bindings& b, Device d);

// Whether anything is pressed on any device (the stick and trigger dead zones taken out)
bool any_held(const HostState& s);

// A rebinding in progress: the page asks for a row on a device, the host feeds it the devices once a game frame (the
// game is given an idle pad meanwhile, and until everything is let go after it ends). The new input is the first one
// pressed on that device after everything was let go: a button or key; for a group row on the pad a stick or the
// D-pad (the whole group), on the keyboard / mouse four keys in turn (up, down, left, right) or a mouse movement (the
// mouse as that stick). Nothing pressed for kTimeoutFrames: cancelled.
class Remapper {
public:
    static constexpr int kTimeoutFrames = 300;  // 5 s of game frames
    enum class Result : uint8_t { kNone, kDone, kCancelled };
    void begin(int row, Device d);
    void cancel();
    // One game frame: true while the game's pad is to be held idle
    bool frame(Bindings& b, const HostState& s);
    bool waiting() const { return row_ >= 0; }
    bool active() const { return row_ >= 0 || releasing_; }  // waiting, or holding the game's pad until everything is let go
    int row() const { return row_; }
    int step() const { return step_; }  // a group on the keyboard: the direction awaited (0 up, 1 down, 2 left, 3 right)
    Device device() const { return device_; }
    // The last rebinding's end, once: kDone (the controls swapped with in *swapped) or kCancelled
    Result take_result(std::vector<int>* swapped);

private:
    int row_ = -1, step_ = 0, frames_ = 0;
    Device device_ = Device::kPad;
    bool armed_ = false;     // everything let go since the start (or the last step)
    bool releasing_ = false; // ended: the game's pad held idle until everything is let go
    float mouse_x_ = 0.0f, mouse_y_ = 0.0f;
    Result result_ = Result::kNone;
    std::vector<int> swapped_;
};

}  // namespace rac1::host_input
