// src/host/host_input.cpp — the host's controls (see host_input.h). Host code, not a port.

#include "host_input.h"

#include <GLFW/glfw3.h>  // (the key / button numbers only: nothing here calls GLFW)

#include <platform/platform.h>

#include <algorithm>
#include <cctype>
#include <cmath>
#include <cstdio>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>

namespace rac1::host_input {

namespace {

// orig: (host) the PS2 pad's controls; the actions are the ones the RE notes name (
// Cross jumps; Square swings the wrench; Start opens the pause menu; L3 shows the HUD)
constexpr ControlInfo CONTROLS[kControlCount] = {
    {"select", "Select", "", 0x0100},
    {"l3", "L3", "Show the HUD", 0x0200},
    {"r3", "R3", "", 0x0400},
    {"start", "Start", "Pause menu", 0x0800},
    {"up", "D-pad up", "", 0x1000},
    {"right", "D-pad right", "", 0x2000},
    {"down", "D-pad down", "", 0x4000},
    {"left", "D-pad left", "", 0x8000},
    {"l2", "L2", "", 0x0001},
    {"r2", "R2", "", 0x0002},
    {"l1", "L1", "", 0x0004},
    {"r1", "R1", "", 0x0008},
    {"triangle", "Triangle", "", 0x0010},
    {"circle", "Circle", "", 0x0020},
    {"cross", "Cross", "Jump", 0x0040},
    {"square", "Square", "Wrench", 0x0080},
    {"left_stick_left", "Left stick left", "Move", 0},
    {"left_stick_right", "Left stick right", "Move", 0},
    {"left_stick_up", "Left stick up", "Move", 0},
    {"left_stick_down", "Left stick down", "Move", 0},
    {"right_stick_left", "Right stick left", "", 0},
    {"right_stick_right", "Right stick right", "", 0},
    {"right_stick_up", "Right stick up", "", 0},
    {"right_stick_down", "Right stick down", "", 0},
};

struct Name { const char* name; int code; };
// the pad's buttons and axes by their gamecontrollerdb (SDL) names
constexpr Name PAD_BUTTONS[] = {
    {"a", GLFW_GAMEPAD_BUTTON_A}, {"b", GLFW_GAMEPAD_BUTTON_B}, {"x", GLFW_GAMEPAD_BUTTON_X}, {"y", GLFW_GAMEPAD_BUTTON_Y},
    {"leftshoulder", GLFW_GAMEPAD_BUTTON_LEFT_BUMPER}, {"rightshoulder", GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER},
    {"back", GLFW_GAMEPAD_BUTTON_BACK}, {"start", GLFW_GAMEPAD_BUTTON_START}, {"guide", GLFW_GAMEPAD_BUTTON_GUIDE},
    {"leftstick", GLFW_GAMEPAD_BUTTON_LEFT_THUMB}, {"rightstick", GLFW_GAMEPAD_BUTTON_RIGHT_THUMB},
    {"dpup", GLFW_GAMEPAD_BUTTON_DPAD_UP}, {"dpright", GLFW_GAMEPAD_BUTTON_DPAD_RIGHT},
    {"dpdown", GLFW_GAMEPAD_BUTTON_DPAD_DOWN}, {"dpleft", GLFW_GAMEPAD_BUTTON_DPAD_LEFT},
};
constexpr Name PAD_AXES[] = {
    {"leftx", GLFW_GAMEPAD_AXIS_LEFT_X}, {"lefty", GLFW_GAMEPAD_AXIS_LEFT_Y}, {"rightx", GLFW_GAMEPAD_AXIS_RIGHT_X},
    {"righty", GLFW_GAMEPAD_AXIS_RIGHT_Y}, {"lefttrigger", GLFW_GAMEPAD_AXIS_LEFT_TRIGGER},
    {"righttrigger", GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER},
};
constexpr Name MOUSE_BUTTONS[] = {
    {"left", GLFW_MOUSE_BUTTON_LEFT}, {"right", GLFW_MOUSE_BUTTON_RIGHT}, {"middle", GLFW_MOUSE_BUTTON_MIDDLE},
    {"button4", GLFW_MOUSE_BUTTON_4}, {"button5", GLFW_MOUSE_BUTTON_5}, {"button6", GLFW_MOUSE_BUTTON_6},
    {"button7", GLFW_MOUSE_BUTTON_7}, {"button8", GLFW_MOUSE_BUTTON_8},
};
// keys other than letters, digits and F1..F25 (made by key_name / key_code)
constexpr Name KEYS[] = {
    {"space", GLFW_KEY_SPACE}, {"apostrophe", GLFW_KEY_APOSTROPHE}, {"comma", GLFW_KEY_COMMA}, {"minus", GLFW_KEY_MINUS},
    {"period", GLFW_KEY_PERIOD}, {"slash", GLFW_KEY_SLASH}, {"semicolon", GLFW_KEY_SEMICOLON}, {"equal", GLFW_KEY_EQUAL},
    {"left_bracket", GLFW_KEY_LEFT_BRACKET}, {"backslash", GLFW_KEY_BACKSLASH}, {"right_bracket", GLFW_KEY_RIGHT_BRACKET},
    {"grave_accent", GLFW_KEY_GRAVE_ACCENT}, {"escape", GLFW_KEY_ESCAPE}, {"enter", GLFW_KEY_ENTER}, {"tab", GLFW_KEY_TAB},
    {"backspace", GLFW_KEY_BACKSPACE}, {"insert", GLFW_KEY_INSERT}, {"delete", GLFW_KEY_DELETE}, {"right", GLFW_KEY_RIGHT},
    {"left", GLFW_KEY_LEFT}, {"down", GLFW_KEY_DOWN}, {"up", GLFW_KEY_UP}, {"page_up", GLFW_KEY_PAGE_UP},
    {"page_down", GLFW_KEY_PAGE_DOWN}, {"home", GLFW_KEY_HOME}, {"end", GLFW_KEY_END}, {"caps_lock", GLFW_KEY_CAPS_LOCK},
    {"kp_0", GLFW_KEY_KP_0}, {"kp_1", GLFW_KEY_KP_1}, {"kp_2", GLFW_KEY_KP_2}, {"kp_3", GLFW_KEY_KP_3}, {"kp_4", GLFW_KEY_KP_4},
    {"kp_5", GLFW_KEY_KP_5}, {"kp_6", GLFW_KEY_KP_6}, {"kp_7", GLFW_KEY_KP_7}, {"kp_8", GLFW_KEY_KP_8}, {"kp_9", GLFW_KEY_KP_9},
    {"kp_decimal", GLFW_KEY_KP_DECIMAL}, {"kp_divide", GLFW_KEY_KP_DIVIDE}, {"kp_multiply", GLFW_KEY_KP_MULTIPLY},
    {"kp_subtract", GLFW_KEY_KP_SUBTRACT}, {"kp_add", GLFW_KEY_KP_ADD}, {"kp_enter", GLFW_KEY_KP_ENTER},
    {"left_shift", GLFW_KEY_LEFT_SHIFT}, {"left_control", GLFW_KEY_LEFT_CONTROL}, {"left_alt", GLFW_KEY_LEFT_ALT},
    {"right_shift", GLFW_KEY_RIGHT_SHIFT}, {"right_control", GLFW_KEY_RIGHT_CONTROL}, {"right_alt", GLFW_KEY_RIGHT_ALT},
};

template <size_t N>
const char* name_of(const Name (&t)[N], int code) {
    for (const Name& n : t) if (n.code == code) return n.name;
    return nullptr;
}
template <size_t N>
bool code_of(const Name (&t)[N], std::string_view s, int& code) {
    for (const Name& n : t) if (s == n.name) { code = n.code; return true; }
    return false;
}

std::string key_name(int code) {
    if (code >= GLFW_KEY_A && code <= GLFW_KEY_Z) return std::string(1, static_cast<char>('a' + (code - GLFW_KEY_A)));
    if (code >= GLFW_KEY_0 && code <= GLFW_KEY_9) return std::string(1, static_cast<char>('0' + (code - GLFW_KEY_0)));
    if (code >= GLFW_KEY_F1 && code <= GLFW_KEY_F25) return "f" + std::to_string(code - GLFW_KEY_F1 + 1);
    if (const char* n = name_of(KEYS, code)) return n;
    return std::to_string(code);  // (any other GLFW key by its number)
}
bool key_code(std::string_view s, int& code) {
    if (s.size() == 1 && s[0] >= 'a' && s[0] <= 'z') { code = GLFW_KEY_A + (s[0] - 'a'); return true; }
    if (s.size() == 1 && s[0] >= '0' && s[0] <= '9') { code = GLFW_KEY_0 + (s[0] - '0'); return true; }
    if (s.size() >= 2 && s[0] == 'f' && std::all_of(s.begin() + 1, s.end(), ::isdigit)) {
        const int n = std::atoi(std::string(s.substr(1)).c_str());
        if (n >= 1 && n <= 25) { code = GLFW_KEY_F1 + n - 1; return true; }
        return false;
    }
    if (code_of(KEYS, s, code)) return true;
    if (!s.empty() && std::all_of(s.begin(), s.end(), ::isdigit)) {
        code = std::atoi(std::string(s).c_str());
        return code >= GLFW_KEY_SPACE && code <= GLFW_KEY_LAST;
    }
    return false;
}

std::string trim(std::string_view s) {
    size_t a = 0, b = s.size();
    while (a < b && std::isspace(static_cast<unsigned char>(s[a]))) ++a;
    while (b > a && std::isspace(static_cast<unsigned char>(s[b - 1]))) --b;
    return std::string(s.substr(a, b - a));
}

HostInput key(int c) { return {InputKind::kKey, c, 1}; }
HostInput mouse(int c) { return {InputKind::kMouseButton, c, 1}; }
HostInput mouse_axis(int c, int sign) { return {InputKind::kMouseAxis, c, sign}; }
HostInput pad(int c) { return {InputKind::kPadButton, c, 1}; }
HostInput pad_axis(int c, int sign) { return {InputKind::kPadAxis, c, sign}; }

constexpr float MOUSE_PIXELS_FULL = 20.0f;  // pixels in a game frame that tilt a stick fully at sensitivity 1
constexpr float TRIGGER_PRESSED = 0.1f;      // a trigger as a button (what the DualSense code did before)
constexpr float STICK_PRESSED = 0.5f;        // a stick half axis as a button
constexpr float MOUSE_PRESSED_PIXELS = 2.0f; // a mouse movement as a button

}  // namespace

const ControlInfo& control_info(PadControl c) { return CONTROLS[static_cast<int>(c)]; }

std::string input_to_string(const HostInput& in) {
    const char* sign = in.sign < 0 ? "-" : "+";
    switch (in.kind) {
        case InputKind::kKey: return "key:" + key_name(in.code);
        case InputKind::kMouseButton: { const char* n = name_of(MOUSE_BUTTONS, in.code); return std::string("mouse:") + (n ? n : "?"); }
        case InputKind::kMouseAxis: return std::string("mouse:") + sign + (in.code == 0 ? "x" : "y");
        case InputKind::kPadButton: { const char* n = name_of(PAD_BUTTONS, in.code); return std::string("pad:") + (n ? n : "?"); }
        case InputKind::kPadAxis: { const char* n = name_of(PAD_AXES, in.code); return std::string("pad:") + sign + (n ? n : "?"); }
        default: return "none";
    }
}

bool input_from_string(std::string_view s, HostInput& out) {
    const size_t colon = s.find(':');
    if (colon == std::string_view::npos) return false;
    const std::string_view dev = s.substr(0, colon);
    std::string_view rest = s.substr(colon + 1);
    int sign = 0;
    if (!rest.empty() && (rest[0] == '+' || rest[0] == '-')) { sign = rest[0] == '-' ? -1 : 1; rest.remove_prefix(1); }
    int code = 0;
    if (dev == "key") {
        if (sign != 0 || !key_code(rest, code)) return false;
        out = key(code);
        return true;
    }
    if (dev == "mouse") {
        if (sign != 0 && (rest == "x" || rest == "y")) { out = mouse_axis(rest == "x" ? 0 : 1, sign); return true; }
        if (sign == 0 && code_of(MOUSE_BUTTONS, rest, code)) { out = mouse(code); return true; }
        return false;
    }
    if (dev == "pad") {
        if (sign == 0 && code_of(PAD_BUTTONS, rest, code)) { out = pad(code); return true; }
        if (code_of(PAD_AXES, rest, code)) { out = pad_axis(code, sign == 0 ? 1 : sign); return true; }  // ("pad:lefttrigger" = "+")
        return false;
    }
    return false;
}

Bindings default_bindings() {
    using P = PadControl;
    Bindings b;
    auto set = [&b](P c, std::initializer_list<HostInput> in) { b.map[static_cast<int>(c)] = in; };
    // the pad: as the earlier DualSense code (the pad's position names: a = bottom face button = Cross)
    // the keyboard and mouse: WASD moves, the mouse is the right stick (a click in the window captures it)
    set(P::kSelect, {pad(GLFW_GAMEPAD_BUTTON_BACK), key(GLFW_KEY_TAB), key(GLFW_KEY_BACKSPACE)});
    set(P::kL3, {pad(GLFW_GAMEPAD_BUTTON_LEFT_THUMB), key(GLFW_KEY_V)});
    set(P::kR3, {pad(GLFW_GAMEPAD_BUTTON_RIGHT_THUMB), mouse(GLFW_MOUSE_BUTTON_MIDDLE), key(GLFW_KEY_B)});
    set(P::kStart, {pad(GLFW_GAMEPAD_BUTTON_START), key(GLFW_KEY_ESCAPE), key(GLFW_KEY_ENTER)});
    set(P::kUp, {pad(GLFW_GAMEPAD_BUTTON_DPAD_UP), key(GLFW_KEY_UP)});
    set(P::kRight, {pad(GLFW_GAMEPAD_BUTTON_DPAD_RIGHT), key(GLFW_KEY_RIGHT)});
    set(P::kDown, {pad(GLFW_GAMEPAD_BUTTON_DPAD_DOWN), key(GLFW_KEY_DOWN)});
    set(P::kLeft, {pad(GLFW_GAMEPAD_BUTTON_DPAD_LEFT), key(GLFW_KEY_LEFT)});
    set(P::kL2, {pad_axis(GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, 1), key(GLFW_KEY_Z)});
    set(P::kR2, {pad_axis(GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER, 1), key(GLFW_KEY_C)});
    set(P::kL1, {pad(GLFW_GAMEPAD_BUTTON_LEFT_BUMPER), key(GLFW_KEY_Q)});
    set(P::kR1, {pad(GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER), key(GLFW_KEY_LEFT_SHIFT)});
    set(P::kTriangle, {pad(GLFW_GAMEPAD_BUTTON_Y), key(GLFW_KEY_E)});
    set(P::kCircle, {pad(GLFW_GAMEPAD_BUTTON_B), mouse(GLFW_MOUSE_BUTTON_LEFT)});
    set(P::kCross, {pad(GLFW_GAMEPAD_BUTTON_A), key(GLFW_KEY_SPACE)});
    set(P::kSquare, {pad(GLFW_GAMEPAD_BUTTON_X), mouse(GLFW_MOUSE_BUTTON_RIGHT), key(GLFW_KEY_F)});
    set(P::kLeftStickLeft, {pad_axis(GLFW_GAMEPAD_AXIS_LEFT_X, -1), key(GLFW_KEY_A)});
    set(P::kLeftStickRight, {pad_axis(GLFW_GAMEPAD_AXIS_LEFT_X, 1), key(GLFW_KEY_D)});
    set(P::kLeftStickUp, {pad_axis(GLFW_GAMEPAD_AXIS_LEFT_Y, -1), key(GLFW_KEY_W)});
    set(P::kLeftStickDown, {pad_axis(GLFW_GAMEPAD_AXIS_LEFT_Y, 1), key(GLFW_KEY_S)});
    set(P::kRightStickLeft, {pad_axis(GLFW_GAMEPAD_AXIS_RIGHT_X, -1), mouse_axis(0, -1), key(GLFW_KEY_J)});
    set(P::kRightStickRight, {pad_axis(GLFW_GAMEPAD_AXIS_RIGHT_X, 1), mouse_axis(0, 1), key(GLFW_KEY_L)});
    set(P::kRightStickUp, {pad_axis(GLFW_GAMEPAD_AXIS_RIGHT_Y, -1), mouse_axis(1, -1), key(GLFW_KEY_I)});
    set(P::kRightStickDown, {pad_axis(GLFW_GAMEPAD_AXIS_RIGHT_Y, 1), mouse_axis(1, 1), key(GLFW_KEY_K)});
    return b;
}

std::string bindings_to_text(const Bindings& b) {
    std::ostringstream o;
    o << "# rac1-native controls. One line per PS2 pad control: <control> = <input>, <input>, ...\n"
         "# Inputs: key:<name> (a..z, 0..9, f1.., space, enter, escape, tab, left_shift, up, kp_0, ...),\n"
         "#         mouse:left / right / middle / button4..8, mouse:+x / -x / +y / -y (movement, needs the captured cursor),\n"
         "#         pad:a / b / x / y / back / start / guide / leftshoulder / rightshoulder / leftstick / rightstick /\n"
         "#             dpup / dpdown / dpleft / dpright, pad:+leftx / -leftx / +lefty / -lefty / ... / +lefttrigger / +righttrigger\n"
         "# The pad names are positions (gamecontrollerdb): a = the bottom face button (Cross on a PlayStation pad).\n"
         "# A control left out keeps its default; '<control> =' with nothing leaves it unbound. Delete the file for the defaults.\n"
         "# The game's Options > Controls page edits this file.\n";
    char buf[64];
    std::snprintf(buf, sizeof(buf), "%g", b.mouse_sensitivity);
    o << "mouse_sensitivity = " << buf << "\n";
    o << "mouse_look = " << (b.mouse_look ? 1 : 0) << "\n";
    o << "invert_look_y = " << (b.invert_look_y ? 1 : 0) << "\n";
    for (int i = 0; i < kControlCount; ++i) {
        const ControlInfo& ci = control_info(i);
        o << ci.name << " =";
        for (size_t k = 0; k < b.map[i].size(); ++k) o << (k == 0 ? " " : ", ") << input_to_string(b.map[i][k]);
        if (ci.action[0] != 0) o << "  # " << ci.action;
        o << "\n";
    }
    return o.str();
}

Bindings bindings_from_text(std::string_view text, std::vector<std::string>* warnings) {
    Bindings b = default_bindings();
    auto warn = [warnings](const std::string& m) { if (warnings != nullptr) warnings->push_back(m); };
    size_t pos = 0;
    int line_no = 0;
    while (pos <= text.size()) {
        size_t end = text.find('\n', pos);
        if (end == std::string_view::npos) end = text.size();
        std::string_view line = text.substr(pos, end - pos);
        pos = end + 1;
        ++line_no;
        if (const size_t hash = line.find('#'); hash != std::string_view::npos) line = line.substr(0, hash);
        const std::string l = trim(line);
        if (l.empty()) continue;
        const size_t eq = l.find('=');
        if (eq == std::string::npos) { warn("line " + std::to_string(line_no) + ": no '='"); continue; }
        const std::string name = trim(std::string_view(l).substr(0, eq));
        const std::string value = trim(std::string_view(l).substr(eq + 1));
        if (name == "mouse_sensitivity") {
            const float v = std::strtof(value.c_str(), nullptr);
            if (v > 0.0f && v <= 100.0f) b.mouse_sensitivity = v;
            else warn("line " + std::to_string(line_no) + ": bad mouse_sensitivity");
            continue;
        }
        if (name == "mouse_look") { b.mouse_look = std::atoi(value.c_str()) != 0; continue; }
        if (name == "invert_look_y") { b.invert_look_y = std::atoi(value.c_str()) != 0; continue; }
        int ctl = -1;
        for (int i = 0; i < kControlCount; ++i) if (name == control_info(i).name) ctl = i;
        if (ctl < 0) { warn("line " + std::to_string(line_no) + ": unknown control '" + name + "'"); continue; }
        std::vector<HostInput> ins;
        size_t p = 0;
        while (p <= value.size()) {
            size_t c = value.find(',', p);
            if (c == std::string::npos) c = value.size();
            const std::string tok = trim(std::string_view(value).substr(p, c - p));
            p = c + 1;
            if (tok.empty()) continue;
            HostInput in;
            if (input_from_string(tok, in)) { if (std::find(ins.begin(), ins.end(), in) == ins.end()) ins.push_back(in); }
            else warn("line " + std::to_string(line_no) + ": unknown input '" + tok + "'");
        }
        b.map[ctl] = std::move(ins);
    }
    return b;
}

std::string controls_path() {
    if (const char* f = std::getenv("RAC1_CONTROLS_FILE"); f != nullptr && *f != 0) return f;
    if (const std::filesystem::path d = rac1::platform::config_dir(); !d.empty()) return (d / "controls.txt").string();
    return "controls.txt";
}

bool save(const Bindings& b, const std::string& path) {
    std::error_code ec;
    const std::filesystem::path p(path);
    if (p.has_parent_path()) std::filesystem::create_directories(p.parent_path(), ec);
    return rac1::platform::write_file_replace(p, bindings_to_text(b));  // (through a temporary file: never half written)
}

Bindings load_or_create(const std::string& path) {
    std::ifstream f(path);
    if (!f) {
        const Bindings d = default_bindings();
        if (save(d, path)) std::printf("[controls] wrote the default controls to %s\n", path.c_str());
        return d;
    }
    std::stringstream ss;
    ss << f.rdbuf();
    std::vector<std::string> warnings;
    Bindings b = bindings_from_text(ss.str(), &warnings);
    for (const std::string& w : warnings) std::fprintf(stderr, "[controls] %s: %s\n", path.c_str(), w.c_str());
    std::printf("[controls] %s\n", path.c_str());
    return b;
}

float input_value(const HostInput& in, const HostState& s, const Bindings& b) {
    switch (in.kind) {
        case InputKind::kKey: return in.code >= 0 && in.code < static_cast<int>(s.keys.size()) && s.keys[in.code] ? 1.0f : 0.0f;
        case InputKind::kMouseButton: return in.code >= 0 && in.code < static_cast<int>(s.mouse_buttons.size()) && s.mouse_buttons[in.code] ? 1.0f : 0.0f;
        case InputKind::kMouseAxis: {
            const float d = (in.code == 0 ? s.mouse_dx : s.mouse_dy) * static_cast<float>(in.sign);
            return std::clamp(d * b.mouse_sensitivity / MOUSE_PIXELS_FULL, 0.0f, 1.0f);
        }
        case InputKind::kPadButton:
            return s.pad_present && in.code >= 0 && in.code < static_cast<int>(s.pad_buttons.size()) && s.pad_buttons[in.code] ? 1.0f : 0.0f;
        case InputKind::kPadAxis: {
            if (!s.pad_present || in.code < 0 || in.code >= static_cast<int>(s.pad_axes.size())) return 0.0f;
            const float v = s.pad_axes[in.code];
            const bool trigger = in.code == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || in.code == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
            if (trigger) return in.sign > 0 ? std::clamp((v + 1.0f) * 0.5f, 0.0f, 1.0f) : std::clamp((1.0f - v) * 0.5f, 0.0f, 1.0f);
            return std::clamp(v * static_cast<float>(in.sign), 0.0f, 1.0f);
        }
        default: return 0.0f;
    }
}

namespace {
// whether an input holds a button (the thresholds of the analogue inputs)
bool input_pressed(const HostInput& in, const HostState& s, const Bindings& b) {
    switch (in.kind) {
        case InputKind::kPadAxis: {
            if (!s.pad_present || in.code < 0 || in.code >= static_cast<int>(s.pad_axes.size())) return false;
            const float v = s.pad_axes[in.code] * static_cast<float>(in.sign);
            const bool trigger = in.code == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || in.code == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER;
            return v > (trigger ? TRIGGER_PRESSED : STICK_PRESSED);
        }
        case InputKind::kMouseAxis:
            return (in.code == 0 ? s.mouse_dx : s.mouse_dy) * static_cast<float>(in.sign) > MOUSE_PRESSED_PIXELS;
        default: return input_value(in, s, b) > 0.0f;
    }
}
}  // namespace

PadOutput compute(const Bindings& b, const HostState& s) {
    PadOutput o;
    for (int i = 0; i < kButtonCount; ++i)
        for (const HostInput& in : b.map[i])
            if (input_pressed(in, s, b)) { o.held |= control_info(i).bit; break; }
    auto half = [&](PadControl c) {
        float v = 0.0f;
        for (const HostInput& in : b.map[static_cast<int>(c)]) v = std::max(v, input_value(in, s, b));
        return v;
    };
    using P = PadControl;
    o.lx = half(P::kLeftStickRight) - half(P::kLeftStickLeft);
    o.ly = half(P::kLeftStickDown) - half(P::kLeftStickUp);
    o.rx = half(P::kRightStickRight) - half(P::kRightStickLeft);
    o.ry = half(P::kRightStickDown) - half(P::kRightStickUp);
    if (b.invert_look_y) o.ry = -o.ry;
    stick_to_square(o.lx, o.ly);
    stick_to_square(o.rx, o.ry);
    return o;
}

void stick_to_square(float& x, float& y) {
    const float mag = std::sqrt(x * x + y * y);
    const float big = std::max(std::fabs(x), std::fabs(y));
    if (big <= 0.0f) return;
    const float k = std::min(mag, 1.0f) / big;
    x = std::clamp(x * k, -1.0f, 1.0f);
    y = std::clamp(y * k, -1.0f, 1.0f);
}

namespace {
uint8_t axis_byte(float v) {
    const float m = (v + 1.0f) * 127.5f;
    return m < 0.0f ? 0 : (m > 255.0f ? 255 : static_cast<uint8_t>(m));
}
}  // namespace

void to_packet(const PadOutput& o, uint8_t packet[6]) {
    const uint16_t low = static_cast<uint16_t>(~o.held);
    packet[0] = static_cast<uint8_t>(low >> 8);
    packet[1] = static_cast<uint8_t>(low & 0xff);
    packet[2] = axis_byte(o.rx);
    packet[3] = axis_byte(o.ry);
    packet[4] = axis_byte(o.lx);
    packet[5] = axis_byte(o.ly);
}


// --- The remapping page ---------------------------------------------------------------------------------

namespace {
using P = PadControl;
// orig: (host) the page's rows: the PS2 pad's buttons, then the D-pad and the sticks
constexpr RemapRow ROWS[kRemapRowCount] = {
    {"Cross", {P::kCross}, 1},   {"Circle", {P::kCircle}, 1}, {"Square", {P::kSquare}, 1}, {"Triangle", {P::kTriangle}, 1},
    {"L1", {P::kL1}, 1},         {"R1", {P::kR1}, 1},         {"L2", {P::kL2}, 1},         {"R2", {P::kR2}, 1},
    {"L3", {P::kL3}, 1},         {"R3", {P::kR3}, 1},         {"Start", {P::kStart}, 1},   {"Select", {P::kSelect}, 1},
    {"D-pad", {P::kUp, P::kDown, P::kLeft, P::kRight}, 4},
    {"Left Stick", {P::kLeftStickUp, P::kLeftStickDown, P::kLeftStickLeft, P::kLeftStickRight}, 4},
    {"Right Stick", {P::kRightStickUp, P::kRightStickDown, P::kRightStickLeft, P::kRightStickRight}, 4},
};

bool contains(const std::vector<HostInput>& v, const HostInput& in) { return std::find(v.begin(), v.end(), in) != v.end(); }

// the groups of four (up, down, left, right) the page knows by one name
constexpr HostInput LEFT_STICK[4] = {{InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_LEFT_Y, -1}, {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_LEFT_Y, 1},
                                     {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_LEFT_X, -1}, {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_LEFT_X, 1}};
constexpr HostInput RIGHT_STICK[4] = {{InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_RIGHT_Y, -1}, {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_RIGHT_Y, 1},
                                      {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_RIGHT_X, -1}, {InputKind::kPadAxis, GLFW_GAMEPAD_AXIS_RIGHT_X, 1}};
constexpr HostInput DPAD[4] = {{InputKind::kPadButton, GLFW_GAMEPAD_BUTTON_DPAD_UP, 1}, {InputKind::kPadButton, GLFW_GAMEPAD_BUTTON_DPAD_DOWN, 1},
                               {InputKind::kPadButton, GLFW_GAMEPAD_BUTTON_DPAD_LEFT, 1}, {InputKind::kPadButton, GLFW_GAMEPAD_BUTTON_DPAD_RIGHT, 1}};
constexpr HostInput MOUSE[4] = {{InputKind::kMouseAxis, 1, -1}, {InputKind::kMouseAxis, 1, 1}, {InputKind::kMouseAxis, 0, -1}, {InputKind::kMouseAxis, 0, 1}};
constexpr HostInput ARROWS[4] = {{InputKind::kKey, GLFW_KEY_UP, 1}, {InputKind::kKey, GLFW_KEY_DOWN, 1}, {InputKind::kKey, GLFW_KEY_LEFT, 1},
                                 {InputKind::kKey, GLFW_KEY_RIGHT, 1}};

bool is_trigger(int axis) { return axis == GLFW_GAMEPAD_AXIS_LEFT_TRIGGER || axis == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER; }

std::string title_case(std::string s) {  // "page_up" -> "Page Up"
    bool start = true;
    for (char& c : s) {
        if (c == '_') {
            c = ' ';
            start = true;
            continue;
        }
        if (start) c = static_cast<char>(std::toupper(static_cast<unsigned char>(c)));
        start = false;
    }
    return s;
}

std::string key_label(int code) {
    if (code >= GLFW_KEY_KP_0 && code <= GLFW_KEY_KP_9) return "Num " + std::to_string(code - GLFW_KEY_KP_0);
    switch (code) {
        case GLFW_KEY_LEFT_SHIFT: return "L Shift";
        case GLFW_KEY_RIGHT_SHIFT: return "R Shift";
        case GLFW_KEY_LEFT_CONTROL: return "L Ctrl";
        case GLFW_KEY_RIGHT_CONTROL: return "R Ctrl";
        case GLFW_KEY_LEFT_ALT: return "L Alt";
        case GLFW_KEY_RIGHT_ALT: return "R Alt";
        case GLFW_KEY_GRAVE_ACCENT: return "`";
        case GLFW_KEY_APOSTROPHE: return "'";
        case GLFW_KEY_COMMA: return "Comma";
        case GLFW_KEY_MINUS: return "-";
        case GLFW_KEY_PERIOD: return ".";
        case GLFW_KEY_SLASH: return "/";
        case GLFW_KEY_SEMICOLON: return ";";
        case GLFW_KEY_EQUAL: return "=";
        case GLFW_KEY_LEFT_BRACKET: return "[";
        case GLFW_KEY_RIGHT_BRACKET: return "]";
        case GLFW_KEY_BACKSLASH: return "\\";
        default: break;
    }
    return title_case(key_name(code));
}

// the pad's buttons as each style names them (by GLFW_GAMEPAD_BUTTON_*: a b x y lb rb back start guide l3 r3 up right down left)
constexpr const char* PAD_NAMES[4][15] = {
    {"\x10", "\x11", "\x13", "\x12", "\x14", "\x15", "Share", "Options", "PS", "L3", "R3", "D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"},
    {"A", "B", "X", "Y", "LB", "RB", "View", "Menu", "Guide", "LS", "RS", "D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"},
    {"B", "A", "Y", "X", "L", "R", "-", "+", "Home", "LS", "RS", "D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"},
    {"A", "B", "X", "Y", "LB", "RB", "Back", "Start", "Guide", "LS", "RS", "D-pad Up", "D-pad Right", "D-pad Down", "D-pad Left"},
};
constexpr const char* TRIGGER_NAMES[4][2] = {{"\x16", "\x17"}, {"LT", "RT"}, {"ZL", "ZR"}, {"LT", "RT"}};
std::string stick_name(bool left, PadStyle st) {
    if (st == PadStyle::kPlayStation) return left ? "\x19" : "\x18";
    return left ? "Left Stick" : "Right Stick";
}

// a layer of a group (one input per direction: up, down, left, right) as one name, "" when it is no known set
std::string group_name(const HostInput (&in)[4], PadStyle st) {
    auto same = [&in](const HostInput (&g)[4]) { return std::equal(std::begin(in), std::end(in), std::begin(g)); };
    if (same(LEFT_STICK)) return stick_name(true, st);
    if (same(RIGHT_STICK)) return stick_name(false, st);
    if (same(DPAD)) return "D-pad";
    if (same(MOUSE)) return "Mouse";
    if (same(ARROWS)) return "Arrow Keys";
    return "";
}

constexpr float CAPTURE_STICK = 0.6f;          // a stick past this picks it
constexpr float CAPTURE_MOUSE_PIXELS = 60.0f;  // a mouse movement this long picks the mouse
}  // namespace

const RemapRow& remap_row(int i) { return ROWS[std::clamp(i, 0, kRemapRowCount - 1)]; }

bool on_device(const HostInput& in, Device d) {
    const bool pad_in = in.kind == InputKind::kPadButton || in.kind == InputKind::kPadAxis;
    const bool kbm = in.kind == InputKind::kKey || in.kind == InputKind::kMouseButton || in.kind == InputKind::kMouseAxis;
    return d == Device::kPad ? pad_in : kbm;
}

std::vector<HostInput> device_inputs(const Bindings& b, PadControl c, Device d) {
    std::vector<HostInput> v;
    for (const HostInput& in : b.map[static_cast<int>(c)])
        if (on_device(in, d)) v.push_back(in);
    return v;
}

PadStyle pad_style(std::string_view name) {
    std::string n(name);
    for (char& c : n) c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    auto has = [&n](const char* w) { return n.find(w) != std::string::npos; };
    if (has("ps3") || has("ps4") || has("ps5") || has("playstation") || has("dualshock") || has("dualsense") || has("sony")) return PadStyle::kPlayStation;
    if (has("xbox") || has("xinput") || has("x-box")) return PadStyle::kXbox;
    if (has("nintendo") || has("switch") || has("joy-con")) return PadStyle::kNintendo;
    return PadStyle::kGeneric;
}

std::string input_label(const HostInput& in, PadStyle st) {
    const int s = static_cast<int>(st);
    switch (in.kind) {
        case InputKind::kKey: return key_label(in.code);
        case InputKind::kMouseButton:
            switch (in.code) {
                case GLFW_MOUSE_BUTTON_LEFT: return "Left Click";
                case GLFW_MOUSE_BUTTON_RIGHT: return "Right Click";
                case GLFW_MOUSE_BUTTON_MIDDLE: return "Middle Click";
                default: return "Mouse " + std::to_string(in.code + 1);
            }
        case InputKind::kMouseAxis:
            return in.code == 0 ? (in.sign < 0 ? "Mouse Left" : "Mouse Right") : (in.sign < 0 ? "Mouse Up" : "Mouse Down");
        case InputKind::kPadButton: return in.code >= 0 && in.code < 15 ? PAD_NAMES[s][in.code] : "?";
        case InputKind::kPadAxis: {
            if (is_trigger(in.code)) return TRIGGER_NAMES[s][in.code == GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER ? 1 : 0];
            const bool left = in.code == GLFW_GAMEPAD_AXIS_LEFT_X || in.code == GLFW_GAMEPAD_AXIS_LEFT_Y;
            const bool x = in.code == GLFW_GAMEPAD_AXIS_LEFT_X || in.code == GLFW_GAMEPAD_AXIS_RIGHT_X;
            return stick_name(left, st) + " " + (x ? (in.sign < 0 ? "Left" : "Right") : (in.sign < 0 ? "Up" : "Down"));
        }
        default: return "-";
    }
}

std::string row_label(const Bindings& b, int row, Device d, PadStyle st) {
    const RemapRow& r = remap_row(row);
    std::string out;
    auto add = [&out](const std::string& s) { out += (out.empty() ? "" : ", ") + s; };
    if (r.count == 1) {
        for (const HostInput& in : device_inputs(b, r.controls[0], d)) add(input_label(in, st));
        return out.empty() ? "-" : out;
    }
    std::vector<HostInput> v[4];
    size_t layers = 0;
    for (int k = 0; k < 4; ++k) {
        v[k] = device_inputs(b, r.controls[k], d);
        layers = std::max(layers, v[k].size());
    }
    for (size_t l = 0; l < layers; ++l) {  // the n-th input of each direction together: "Mouse", "I J K L"
        HostInput layer[4];
        bool whole = true;
        for (int k = 0; k < 4; ++k) {
            if (l < v[k].size()) layer[k] = v[k][l];
            else whole = false;
        }
        const std::string g = whole ? group_name(layer, st) : "";
        if (!g.empty()) {
            add(g);
            continue;
        }
        // up, left, down, right: "WASD" (single letters together), else "Up Left Down Right"
        std::string labels[4];
        bool letters = true;
        for (int k = 0; k < 4; ++k) {
            labels[k] = l < v[k].size() ? input_label(v[k][l], st) : std::string("-");
            letters &= labels[k].size() == 1;
        }
        add(letters ? labels[0] + labels[2] + labels[1] + labels[3] : labels[0] + " " + labels[2] + " " + labels[1] + " " + labels[3]);
    }
    return out.empty() ? "-" : out;
}

void rebind(Bindings& b, PadControl c, Device d, const HostInput& in, std::vector<int>* swapped) {
    const int ci = static_cast<int>(c);
    const std::vector<HostInput> old = device_inputs(b, c, d);
    for (int x = 0; x < kControlCount; ++x) {
        if (x == ci || !contains(b.map[x], in)) continue;
        std::vector<HostInput>& v = b.map[x];
        v.erase(std::remove(v.begin(), v.end(), in), v.end());
        size_t at = 0;  // (the pad's inputs first, as in the defaults)
        for (const HostInput& o : old)
            if (!contains(v, o)) {
                if (d == Device::kPad) v.insert(v.begin() + static_cast<std::ptrdiff_t>(at++), o);
                else v.push_back(o);
            }
        if (swapped != nullptr && std::find(swapped->begin(), swapped->end(), x) == swapped->end()) swapped->push_back(x);
    }
    std::vector<HostInput>& v = b.map[ci];
    v.erase(std::remove_if(v.begin(), v.end(), [d](const HostInput& h) { return on_device(h, d); }), v.end());
    if (d == Device::kPad) v.insert(v.begin(), in);  // (the pad's inputs first, as in the defaults)
    else v.push_back(in);
}

void reset_device(Bindings& b, Device d) {
    const Bindings def = default_bindings();
    for (int i = 0; i < kControlCount; ++i) {
        std::vector<HostInput> v;
        for (const Device k : {Device::kPad, Device::kKeyboardMouse})
            for (const HostInput& in : (k == d ? def : b).map[i])
                if (on_device(in, k)) v.push_back(in);
        b.map[i] = std::move(v);
    }
    if (d == Device::kKeyboardMouse) b.mouse_sensitivity = def.mouse_sensitivity;
    b.invert_look_y = def.invert_look_y;
}

void merge_pad(HostState& s, const bool (&buttons)[15], const float (&axes)[6]) {
    s.pad_present = true;
    for (int b = 0; b < 15; ++b) s.pad_buttons[b] = s.pad_buttons[b] || buttons[b];
    for (const int x : {GLFW_GAMEPAD_AXIS_LEFT_X, GLFW_GAMEPAD_AXIS_RIGHT_X}) {  // (the stick's y axis follows its x)
        const float had = s.pad_axes[x] * s.pad_axes[x] + s.pad_axes[x + 1] * s.pad_axes[x + 1];
        if (axes[x] * axes[x] + axes[x + 1] * axes[x + 1] > had) {
            s.pad_axes[x] = axes[x];
            s.pad_axes[x + 1] = axes[x + 1];
        }
    }
    for (const int t : {GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER}) s.pad_axes[t] = std::max(s.pad_axes[t], axes[t]);
}

bool pad_held(const bool (&buttons)[15], const float (&axes)[6]) {
    for (const bool p : buttons)
        if (p) return true;
    for (int a = 0; a < 6; ++a)
        if (is_trigger(a) ? axes[a] > -0.4f : std::fabs(axes[a]) > 0.3f) return true;
    return false;
}

bool any_held(const HostState& s) {
    for (const bool k : s.keys)
        if (k) return true;
    for (const bool m : s.mouse_buttons)
        if (m) return true;
    if (!s.pad_present) return false;
    for (const bool p : s.pad_buttons)
        if (p) return true;
    for (int a = 0; a < static_cast<int>(s.pad_axes.size()); ++a)
        if (is_trigger(a) ? s.pad_axes[a] > -0.4f : std::fabs(s.pad_axes[a]) > 0.3f) return true;  // (a trigger rests at -1)
    return false;
}

void Remapper::begin(int row, Device d) {
    row_ = std::clamp(row, 0, kRemapRowCount - 1);
    device_ = d;
    step_ = frames_ = 0;
    armed_ = false;
    releasing_ = false;
    mouse_x_ = mouse_y_ = 0.0f;
    result_ = Result::kNone;
    swapped_.clear();
}

void Remapper::cancel() {
    if (row_ < 0) return;
    row_ = -1;
    releasing_ = true;
    result_ = Result::kCancelled;
}

Remapper::Result Remapper::take_result(std::vector<int>* swapped) {
    const Result r = result_;
    result_ = Result::kNone;
    if (swapped != nullptr) *swapped = swapped_;
    return r;
}

bool Remapper::frame(Bindings& b, const HostState& s) {
    if (releasing_) {  // (the frame everything is let go is still idle: the game sees no press of the new input)
        releasing_ = any_held(s);
        return true;
    }
    if (row_ < 0) return false;
    if (!armed_) {  // everything let go first (the button that started it, the last direction's key)
        if (!any_held(s)) {
            armed_ = true;
            frames_ = 0;
            mouse_x_ = mouse_y_ = 0.0f;
        }
        return true;
    }
    if (++frames_ > kTimeoutFrames) {
        cancel();
        return true;
    }
    const RemapRow& r = remap_row(row_);
    const bool group = r.count == 4;
    auto done = [&]() {
        std::vector<int> keep;  // (a group's own directions swapping among themselves are not reported)
        for (const int x : swapped_) {
            bool own = false;
            for (int k = 0; k < r.count; ++k) own |= static_cast<int>(r.controls[k]) == x;
            if (!own) keep.push_back(x);
        }
        swapped_ = keep;
        row_ = -1;
        releasing_ = true;
        result_ = Result::kDone;
    };
    auto bind_group = [&](const HostInput (&g)[4]) {
        for (int k = 0; k < 4; ++k) rebind(b, r.controls[k], device_, g[k], &swapped_);
        done();
    };
    if (device_ == Device::kPad) {
        if (!s.pad_present) return true;
        if (group) {  // a stick or the D-pad: the whole group
            for (int a = GLFW_GAMEPAD_AXIS_LEFT_X; a <= GLFW_GAMEPAD_AXIS_RIGHT_Y; ++a)
                if (std::fabs(s.pad_axes[a]) > CAPTURE_STICK) {
                    bind_group(a == GLFW_GAMEPAD_AXIS_LEFT_X || a == GLFW_GAMEPAD_AXIS_LEFT_Y ? LEFT_STICK : RIGHT_STICK);
                    return true;
                }
            for (const HostInput& in : DPAD)
                if (s.pad_buttons[in.code]) {
                    bind_group(DPAD);
                    return true;
                }
            return true;
        }
        for (int k = 0; k < static_cast<int>(s.pad_buttons.size()); ++k)
            if (s.pad_buttons[k] && k != GLFW_GAMEPAD_BUTTON_GUIDE) {
                rebind(b, r.controls[0], device_, pad(k), &swapped_);
                done();
                return true;
            }
        for (const int a : {GLFW_GAMEPAD_AXIS_LEFT_TRIGGER, GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER})
            if (s.pad_axes[a] > 0.0f) {  // (more than half pressed)
                rebind(b, r.controls[0], device_, pad_axis(a, 1), &swapped_);
                done();
                return true;
            }
        return true;
    }
    // the keyboard and mouse: the first key or mouse button held
    HostInput in;
    bool found = false;
    for (int k = GLFW_KEY_SPACE; k < static_cast<int>(s.keys.size()) && !found; ++k)
        if (s.keys[k]) {
            in = key(k);
            found = true;
        }
    for (int m = 0; m < static_cast<int>(s.mouse_buttons.size()) && !found; ++m)
        if (s.mouse_buttons[m]) {
            in = mouse(m);
            found = true;
        }
    // a mouse movement before the first key: the mouse as the stick (the sticks only, as the page says: the D-pad bound to the
    // mouse loses the arrow keys the menus are moved with, #61)
    if (group && step_ == 0 && !found && r.controls[0] != PadControl::kUp) {
        mouse_x_ += s.mouse_dx;
        mouse_y_ += s.mouse_dy;
        if (std::fabs(mouse_x_) > CAPTURE_MOUSE_PIXELS || std::fabs(mouse_y_) > CAPTURE_MOUSE_PIXELS) {
            bind_group(MOUSE);
            return true;
        }
    }
    if (!found) return true;
    if (!group) {
        rebind(b, r.controls[0], device_, in, &swapped_);
        done();
        return true;
    }
    rebind(b, r.controls[step_], device_, in, &swapped_);  // a group's directions in turn: up, down, left, right
    if (++step_ == 4) {
        done();
        return true;
    }
    armed_ = false;
    frames_ = 0;
    return true;
}

}  // namespace rac1::host_input
