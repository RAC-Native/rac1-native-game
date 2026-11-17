#include "android_host.h"

#include <runtime/vk/vk_context.h>

#include <GLFW/glfw3.h>
#include <backends/imgui_impl_android.h>
#include <imgui.h>

#include <android_native_app_glue.h>
#include <android/configuration.h>
#include <android/input.h>
#include <android/keycodes.h>
#include <android/log.h>
#include <android/looper.h>
#include <android/native_activity.h>
#include <android/native_window.h>
#include <jni.h>

#include <pthread.h>
#include <unistd.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <chrono>
#include <cmath>
#include <condition_variable>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <mutex>
#include <string>
#include <thread>
#include <unordered_map>
#include <vector>

// Entry point renamed from main() in src/host/orig_main.cpp on __ANDROID__.
int rac1_orig_main(int argc, char** argv);

namespace rac1::android_host {

namespace {

constexpr const char* kLogTag = "rac1-native";

// Android axis constants (guaranteed same across API levels; keep explicit fallbacks if headers omit any).
constexpr int32_t kAxisX = AMOTION_EVENT_AXIS_X;
constexpr int32_t kAxisY = AMOTION_EVENT_AXIS_Y;
constexpr int32_t kAxisZ = AMOTION_EVENT_AXIS_Z;
constexpr int32_t kAxisRx = AMOTION_EVENT_AXIS_RX;
constexpr int32_t kAxisRy = AMOTION_EVENT_AXIS_RY;
constexpr int32_t kAxisRz = AMOTION_EVENT_AXIS_RZ;
constexpr int32_t kAxisHatX = AMOTION_EVENT_AXIS_HAT_X;
constexpr int32_t kAxisHatY = AMOTION_EVENT_AXIS_HAT_Y;
constexpr int32_t kAxisLTrigger = AMOTION_EVENT_AXIS_LTRIGGER;
constexpr int32_t kAxisRTrigger = AMOTION_EVENT_AXIS_RTRIGGER;
constexpr int32_t kAxisGas = AMOTION_EVENT_AXIS_GAS;
constexpr int32_t kAxisBrake = AMOTION_EVENT_AXIS_BRAKE;

// Source bitmasks from android.view.InputDevice.
constexpr int32_t kSourceJoystick = 0x01000010;
constexpr int32_t kSourceGamepad = 0x00000401;
constexpr int32_t kSourceDpad = 0x00000201;

struct JniScope {
    JavaVM* vm = nullptr;
    JNIEnv* env = nullptr;
    bool attached = false;

    explicit JniScope(JavaVM* jvm) : vm(jvm) {
        if (vm == nullptr) return;
        const jint r = vm->GetEnv(reinterpret_cast<void**>(&env), JNI_VERSION_1_6);
        if (r == JNI_OK) return;
        if (r == JNI_EDETACHED) {
            if (vm->AttachCurrentThread(&env, nullptr) == JNI_OK) {
                attached = true;
            } else {
                env = nullptr;
            }
        } else {
            env = nullptr;
        }
    }

    ~JniScope() {
        if (attached && vm != nullptr) vm->DetachCurrentThread();
    }
};

struct ImguiEvent {
    enum class Type : uint8_t {
        kKey,
        kMouseSource,
        kMousePos,
        kMouseButton,
        kMouseWheel,
    };
    Type type = Type::kMousePos;
    int32_t a = 0;
    int32_t b = 0;
    float fx = 0.0f;
    float fy = 0.0f;
    bool down = false;
};

struct PadDevice {
    int32_t id = -1;
    std::string name;
    std::string guid;
    int vendor_id = 0;
    int product_id = 0;
    bool is_sony = false;
    bool has_z = false;
    bool has_rz = false;
    bool has_rx = false;
    bool has_ry = false;
    bool has_ltrigger = false;
    bool has_rtrigger = false;
    bool has_gas = false;
    bool has_brake = false;
    bool has_hat_x = false;
    bool has_hat_y = false;

    // Separate sources combined into standard GLFW-style 15 buttons + 6 axes.
    bool raw_buttons[15]{};
    bool dpad_keys[4]{};  // up, right, down, left
    float hat_x = 0.0f;
    float hat_y = 0.0f;
    float lx = 0.0f;
    float ly = 0.0f;
    float rx = 0.0f;
    float ry = 0.0f;
    float l2_axis = 0.0f;  // 0..1 on Android
    float r2_axis = 0.0f;  // 0..1 on Android
    bool l2_key = false;
    bool r2_key = false;
    uint64_t last_active_seq = 0;
    bool has_received_input = false;
};

std::mutex g_lifecycle_mu;
std::condition_variable g_lifecycle_cv;
std::recursive_mutex g_render_mu;
std::mutex g_input_mu;

struct android_app* g_app = nullptr;
ANativeWindow* g_window = nullptr;
ANativeWindow* g_imgui_window = nullptr;
int g_win_w = 1280;
int g_win_h = 720;
std::atomic<int> g_refresh_hz{60};
std::atomic<bool> g_vulkan_active{false};
std::atomic<bool> g_should_close{false};
std::atomic<bool> g_game_exited{false};
bool g_surface_suspended = false;
bool g_app_paused = false;
bool g_need_resume_surface = false;
bool g_finish_requested = false;

// Input state (protected by g_input_mu).
std::array<bool, 349> g_keys{};
std::array<bool, 8> g_mouse_buttons{};
float g_mouse_dx = 0.0f;
float g_mouse_dy = 0.0f;
float g_last_mouse_x = 0.0f;
float g_last_mouse_y = 0.0f;
bool g_has_last_mouse = false;
float g_wheel_accum = 0.0f;
uint64_t g_input_seq = 0;
std::unordered_map<int32_t, PadDevice> g_pads;
std::vector<ImguiEvent> g_imgui_queue;

// Logcat + file tee state.
std::string g_log_file_path;

void start_stdio_redirect(const std::string& log_path) {
    g_log_file_path = log_path;
    int pfd[2]{-1, -1};
    if (pipe(pfd) != 0) return;

    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    std::setvbuf(stderr, nullptr, _IONBF, 0);
    dup2(pfd[1], STDOUT_FILENO);
    dup2(pfd[1], STDERR_FILENO);
    if (pfd[1] > STDERR_FILENO) close(pfd[1]);

    const int read_fd = pfd[0];
    std::thread([read_fd]() {
        std::FILE* logf = !g_log_file_path.empty() ? std::fopen(g_log_file_path.c_str(), "w") : nullptr;
        char buf[1024];
        std::string line;
        for (;;) {
            const ssize_t n = read(read_fd, buf, sizeof(buf));
            if (n <= 0) break;
            if (logf != nullptr) {
                std::fwrite(buf, 1, static_cast<size_t>(n), logf);
                std::fflush(logf);
            }
            for (ssize_t i = 0; i < n; ++i) {
                const char c = buf[i];
                if (c == '\n') {
                    __android_log_write(ANDROID_LOG_INFO, kLogTag, line.c_str());
                    line.clear();
                } else if (c != '\r') {
                    line.push_back(c);
                    if (line.size() >= 900) {
                        __android_log_write(ANDROID_LOG_INFO, kLogTag, line.c_str());
                        line.clear();
                    }
                }
            }
        }
        if (!line.empty()) __android_log_write(ANDROID_LOG_INFO, kLogTag, line.c_str());
        if (logf != nullptr) std::fclose(logf);
        close(read_fd);
    }).detach();
}

std::string jstring_to_utf8(JNIEnv* env, jstring js) {
    if (env == nullptr || js == nullptr) return {};
    const char* chars = env->GetStringUTFChars(js, nullptr);
    std::string out = chars != nullptr ? chars : "";
    if (chars != nullptr) env->ReleaseStringUTFChars(js, chars);
    return out;
}

std::string resolve_external_files_dir(ANativeActivity* activity) {
    if (activity == nullptr) return {};
    if (activity->externalDataPath != nullptr && activity->externalDataPath[0] != '\0') {
        return activity->externalDataPath;
    }
    JniScope jni(activity->vm);
    if (jni.env == nullptr || activity->clazz == nullptr) {
        return activity->internalDataPath != nullptr ? activity->internalDataPath : "";
    }
    JNIEnv* env = jni.env;
    jclass act_cls = env->GetObjectClass(activity->clazz);
    jmethodID get_ext = env->GetMethodID(act_cls, "getExternalFilesDir", "(Ljava/lang/String;)Ljava/io/File;");
    std::string out;
    if (get_ext != nullptr) {
        jobject file_obj = env->CallObjectMethod(activity->clazz, get_ext, nullptr);
        if (!env->ExceptionCheck() && file_obj != nullptr) {
            jclass file_cls = env->GetObjectClass(file_obj);
            jmethodID get_abs = env->GetMethodID(file_cls, "getAbsolutePath", "()Ljava/lang/String;");
            if (get_abs != nullptr) {
                jstring path_js = static_cast<jstring>(env->CallObjectMethod(file_obj, get_abs));
                if (!env->ExceptionCheck()) out = jstring_to_utf8(env, path_js);
                if (path_js != nullptr) env->DeleteLocalRef(path_js);
            }
            env->DeleteLocalRef(file_cls);
            env->DeleteLocalRef(file_obj);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(act_cls);
    if (!out.empty()) return out;
    return activity->internalDataPath != nullptr ? activity->internalDataPath : "";
}

int query_display_refresh_hz(ANativeActivity* activity) {
    if (activity == nullptr) return 60;
    JniScope jni(activity->vm);
    if (jni.env == nullptr || activity->clazz == nullptr) return 60;
    JNIEnv* env = jni.env;
    int hz = 60;

    jclass act_cls = env->GetObjectClass(activity->clazz);
    jmethodID get_sys = env->GetMethodID(act_cls, "getSystemService", "(Ljava/lang/String;)Ljava/lang/Object;");
    if (get_sys != nullptr) {
        jstring svc = env->NewStringUTF("window");
        jobject wm = env->CallObjectMethod(activity->clazz, get_sys, svc);
        env->DeleteLocalRef(svc);
        if (!env->ExceptionCheck() && wm != nullptr) {
            jclass wm_cls = env->GetObjectClass(wm);
            jmethodID get_disp = env->GetMethodID(wm_cls, "getDefaultDisplay", "()Landroid/view/Display;");
            if (get_disp != nullptr) {
                jobject disp = env->CallObjectMethod(wm, get_disp);
                if (!env->ExceptionCheck() && disp != nullptr) {
                    jclass disp_cls = env->GetObjectClass(disp);
                    jmethodID get_hz = env->GetMethodID(disp_cls, "getRefreshRate", "()F");
                    if (get_hz != nullptr) {
                        const float f = env->CallFloatMethod(disp, get_hz);
                        if (!env->ExceptionCheck() && f >= 24.0f && f <= 240.0f) {
                            hz = static_cast<int>(std::lround(f));
                        }
                    }
                    env->DeleteLocalRef(disp_cls);
                    env->DeleteLocalRef(disp);
                }
            }
            env->DeleteLocalRef(wm_cls);
            env->DeleteLocalRef(wm);
        }
    }
    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(act_cls);
    return std::clamp(hz, 30, 240);
}

void configure_environment(struct android_app* app) {
    const std::string ext_dir = resolve_external_files_dir(app->activity);
    const std::string base_dir = !ext_dir.empty() ? ext_dir
        : (app->activity != nullptr && app->activity->internalDataPath != nullptr ? app->activity->internalDataPath : "/data/local/tmp");

    std::error_code ec;
    const std::filesystem::path root(base_dir);
    const std::filesystem::path userdata = root / "userdata";
    const std::filesystem::path memcards = userdata / "memcards";
    std::filesystem::create_directories(userdata / "rac1-native", ec);
    std::filesystem::create_directories(memcards, ec);

    start_stdio_redirect((root / "game.log").string());

    if (chdir(base_dir.c_str()) != 0) {
        __android_log_print(ANDROID_LOG_WARN, kLogTag, "chdir(%s) failed", base_dir.c_str());
    }

    setenv("HOME", base_dir.c_str(), 1);
    setenv("XDG_CONFIG_HOME", userdata.string().c_str(), 1);
    setenv("XDG_DATA_HOME", userdata.string().c_str(), 1);
    setenv("RAC1_MEMCARD_DIR", memcards.string().c_str(), 1);
    setenv("RAC1_RENDERER", "vulkan", 0);

    char lang[4]{};
    char country[4]{};
    if (app->config != nullptr) {
        AConfiguration_getLanguage(app->config, lang);
        AConfiguration_getCountry(app->config, country);
    }
    if (lang[0] != '\0') {
        std::string locale(lang, strnlen(lang, 2));
        if (country[0] != '\0') {
            locale.push_back('_');
            locale.append(country, strnlen(country, 2));
        }
        locale.append(".UTF-8");
        setenv("LANG", locale.c_str(), 0);
    } else {
        setenv("LANG", "en_US.UTF-8", 0);
    }

    g_refresh_hz.store(query_display_refresh_hz(app->activity), std::memory_order_relaxed);
    std::printf("[android] cwd=%s refresh=%dHz\n", base_dir.c_str(), g_refresh_hz.load(std::memory_order_relaxed));
}

bool contains_icase(const std::string& hay, const char* needle) {
    if (needle == nullptr || *needle == '\0') return false;
    const size_t nlen = std::strlen(needle);
    if (hay.size() < nlen) return false;
    for (size_t i = 0; i + nlen <= hay.size(); ++i) {
        bool eq = true;
        for (size_t j = 0; j < nlen; ++j) {
            const char a = hay[i + j];
            const char b = needle[j];
            const char la = (a >= 'A' && a <= 'Z') ? static_cast<char>(a + ('a' - 'A')) : a;
            const char lb = (b >= 'A' && b <= 'Z') ? static_cast<char>(b + ('a' - 'A')) : b;
            if (la != lb) {
                eq = false;
                break;
            }
        }
        if (eq) return true;
    }
    return false;
}

std::string make_sdl_guid(uint16_t bustype, uint16_t vendor, uint16_t product, uint16_t version) {
    char buf[33];
    std::snprintf(buf, sizeof(buf),
                  "%02x%02x0000%02x%02x0000%02x%02x0000%02x%02x0000",
                  bustype & 0xff, bustype >> 8,
                  vendor & 0xff, vendor >> 8,
                  product & 0xff, product >> 8,
                  version & 0xff, version >> 8);
    return buf;
}

PadDevice query_pad_device(int32_t dev_id) {
    PadDevice d;
    d.id = dev_id;
    d.name = "Android Controller";
    d.guid = make_sdl_guid(0x0005, 0x0000, 0x0000, 0x0001);

    if (g_app == nullptr || g_app->activity == nullptr || dev_id < 0) return d;
    JniScope jni(g_app->activity->vm);
    if (jni.env == nullptr) return d;
    JNIEnv* env = jni.env;

    jclass dev_cls = env->FindClass("android/view/InputDevice");
    if (dev_cls == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return d;
    }
    jmethodID get_dev = env->GetStaticMethodID(dev_cls, "getDevice", "(I)Landroid/view/InputDevice;");
    jobject dev_obj = get_dev != nullptr ? env->CallStaticObjectMethod(dev_cls, get_dev, static_cast<jint>(dev_id)) : nullptr;
    if (env->ExceptionCheck() || dev_obj == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(dev_cls);
        return d;
    }

    if (jmethodID m = env->GetMethodID(dev_cls, "getName", "()Ljava/lang/String;")) {
        jstring js = static_cast<jstring>(env->CallObjectMethod(dev_obj, m));
        if (!env->ExceptionCheck() && js != nullptr) {
            std::string nm = jstring_to_utf8(env, js);
            if (!nm.empty()) d.name = std::move(nm);
        }
        if (js != nullptr) env->DeleteLocalRef(js);
    }
    if (jmethodID m = env->GetMethodID(dev_cls, "getVendorId", "()I")) {
        const jint v = env->CallIntMethod(dev_obj, m);
        if (!env->ExceptionCheck()) d.vendor_id = v;
    }
    if (jmethodID m = env->GetMethodID(dev_cls, "getProductId", "()I")) {
        const jint p = env->CallIntMethod(dev_obj, m);
        if (!env->ExceptionCheck()) d.product_id = p;
    }

    if (d.vendor_id == 0x054c) {
        d.is_sony = true;
        if (!contains_icase(d.name, "sony") && !contains_icase(d.name, "dualsense") &&
            !contains_icase(d.name, "dualshock") && !contains_icase(d.name, "playstation")) {
            d.name += " (Sony)";
        }
    }
    d.guid = make_sdl_guid(0x0005,
                           static_cast<uint16_t>(d.vendor_id),
                           static_cast<uint16_t>(d.product_id),
                           0x0001);

    if (jmethodID has_range = env->GetMethodID(dev_cls, "getMotionRange", "(I)Landroid/view/InputDevice$MotionRange;")) {
        auto check = [&](int32_t axis) {
            jobject mr = env->CallObjectMethod(dev_obj, has_range, static_cast<jint>(axis));
            const bool ok = !env->ExceptionCheck() && mr != nullptr;
            if (env->ExceptionCheck()) env->ExceptionClear();
            if (mr != nullptr) env->DeleteLocalRef(mr);
            return ok;
        };
        d.has_z = check(kAxisZ);
        d.has_rz = check(kAxisRz);
        d.has_rx = check(kAxisRx);
        d.has_ry = check(kAxisRy);
        d.has_ltrigger = check(kAxisLTrigger);
        d.has_rtrigger = check(kAxisRTrigger);
        d.has_gas = check(kAxisGas);
        d.has_brake = check(kAxisBrake);
        d.has_hat_x = check(kAxisHatX);
        d.has_hat_y = check(kAxisHatY);
    }

    if (env->ExceptionCheck()) env->ExceptionClear();
    env->DeleteLocalRef(dev_obj);
    env->DeleteLocalRef(dev_cls);
    return d;
}

PadDevice& get_or_register_pad_locked(int32_t dev_id) {
    auto it = g_pads.find(dev_id);
    if (it != g_pads.end()) return it->second;
    PadDevice info = query_pad_device(dev_id);
    std::printf("[controls] Android pad %d connected: %s (vid=%04x pid=%04x z=%d rz=%d rx=%d ry=%d lt=%d rt=%d)\n",
                dev_id, info.name.c_str(), info.vendor_id, info.product_id,
                info.has_z ? 1 : 0, info.has_rz ? 1 : 0, info.has_rx ? 1 : 0, info.has_ry ? 1 : 0,
                info.has_ltrigger ? 1 : 0, info.has_rtrigger ? 1 : 0);
    return g_pads.emplace(dev_id, std::move(info)).first->second;
}

void scan_connected_gamepads() {
    if (g_app == nullptr || g_app->activity == nullptr) return;
    JniScope jni(g_app->activity->vm);
    if (jni.env == nullptr) return;
    JNIEnv* env = jni.env;

    jclass dev_cls = env->FindClass("android/view/InputDevice");
    if (dev_cls == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        return;
    }
    jmethodID get_ids = env->GetStaticMethodID(dev_cls, "getDeviceIds", "()[I");
    jmethodID get_dev = env->GetStaticMethodID(dev_cls, "getDevice", "(I)Landroid/view/InputDevice;");
    jmethodID get_srcs = env->GetMethodID(dev_cls, "getSources", "()I");
    if (get_ids == nullptr || get_dev == nullptr || get_srcs == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(dev_cls);
        return;
    }

    jintArray ids_arr = static_cast<jintArray>(env->CallStaticObjectMethod(dev_cls, get_ids));
    if (env->ExceptionCheck() || ids_arr == nullptr) {
        if (env->ExceptionCheck()) env->ExceptionClear();
        env->DeleteLocalRef(dev_cls);
        return;
    }

    const jsize len = env->GetArrayLength(ids_arr);
    std::vector<jint> ids(static_cast<size_t>(len));
    if (len > 0) env->GetIntArrayRegion(ids_arr, 0, len, ids.data());
    env->DeleteLocalRef(ids_arr);

    std::vector<int32_t> live_gamepad_ids;
    for (jint id : ids) {
        jobject dev = env->CallStaticObjectMethod(dev_cls, get_dev, id);
        if (env->ExceptionCheck() || dev == nullptr) {
            if (env->ExceptionCheck()) env->ExceptionClear();
            continue;
        }
        const jint srcs = env->CallIntMethod(dev, get_srcs);
        env->DeleteLocalRef(dev);
        if (env->ExceptionCheck()) {
            env->ExceptionClear();
            continue;
        }
        const bool is_pad = ((srcs & kSourceGamepad) == kSourceGamepad) || ((srcs & kSourceJoystick) == kSourceJoystick);
        if (is_pad) live_gamepad_ids.push_back(static_cast<int32_t>(id));
    }
    env->DeleteLocalRef(dev_cls);

    std::lock_guard<std::mutex> lk(g_input_mu);
    // Drop disconnected device IDs.
    for (auto it = g_pads.begin(); it != g_pads.end();) {
        if (std::find(live_gamepad_ids.begin(), live_gamepad_ids.end(), it->first) == live_gamepad_ids.end()) {
            it = g_pads.erase(it);
        } else {
            ++it;
        }
    }
    for (int32_t id : live_gamepad_ids) {
        (void)get_or_register_pad_locked(id);
    }
}

int android_key_to_glfw(int32_t code) {
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) return GLFW_KEY_A + (code - AKEYCODE_A);
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) return GLFW_KEY_0 + (code - AKEYCODE_0);
    if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) return GLFW_KEY_F1 + (code - AKEYCODE_F1);
    if (code >= AKEYCODE_NUMPAD_0 && code <= AKEYCODE_NUMPAD_9) return GLFW_KEY_KP_0 + (code - AKEYCODE_NUMPAD_0);
    switch (code) {
        case AKEYCODE_SPACE: return GLFW_KEY_SPACE;
        case AKEYCODE_APOSTROPHE: return GLFW_KEY_APOSTROPHE;
        case AKEYCODE_COMMA: return GLFW_KEY_COMMA;
        case AKEYCODE_MINUS: return GLFW_KEY_MINUS;
        case AKEYCODE_PERIOD: return GLFW_KEY_PERIOD;
        case AKEYCODE_SLASH: return GLFW_KEY_SLASH;
        case AKEYCODE_SEMICOLON: return GLFW_KEY_SEMICOLON;
        case AKEYCODE_EQUALS: return GLFW_KEY_EQUAL;
        case AKEYCODE_LEFT_BRACKET: return GLFW_KEY_LEFT_BRACKET;
        case AKEYCODE_BACKSLASH: return GLFW_KEY_BACKSLASH;
        case AKEYCODE_RIGHT_BRACKET: return GLFW_KEY_RIGHT_BRACKET;
        case AKEYCODE_GRAVE: return GLFW_KEY_GRAVE_ACCENT;
        case AKEYCODE_ESCAPE: return GLFW_KEY_ESCAPE;
        case AKEYCODE_ENTER: return GLFW_KEY_ENTER;
        case AKEYCODE_TAB: return GLFW_KEY_TAB;
        case AKEYCODE_DEL: return GLFW_KEY_BACKSPACE;
        case AKEYCODE_INSERT: return GLFW_KEY_INSERT;
        case AKEYCODE_FORWARD_DEL: return GLFW_KEY_DELETE;
        case AKEYCODE_DPAD_RIGHT: return GLFW_KEY_RIGHT;
        case AKEYCODE_DPAD_LEFT: return GLFW_KEY_LEFT;
        case AKEYCODE_DPAD_DOWN: return GLFW_KEY_DOWN;
        case AKEYCODE_DPAD_UP: return GLFW_KEY_UP;
        case AKEYCODE_PAGE_UP: return GLFW_KEY_PAGE_UP;
        case AKEYCODE_PAGE_DOWN: return GLFW_KEY_PAGE_DOWN;
        case AKEYCODE_MOVE_HOME: return GLFW_KEY_HOME;
        case AKEYCODE_MOVE_END: return GLFW_KEY_END;
        case AKEYCODE_CAPS_LOCK: return GLFW_KEY_CAPS_LOCK;
        case AKEYCODE_SCROLL_LOCK: return GLFW_KEY_SCROLL_LOCK;
        case AKEYCODE_NUM_LOCK: return GLFW_KEY_NUM_LOCK;
        case AKEYCODE_SYSRQ: return GLFW_KEY_PRINT_SCREEN;
        case AKEYCODE_BREAK: return GLFW_KEY_PAUSE;
        case AKEYCODE_NUMPAD_DOT: return GLFW_KEY_KP_DECIMAL;
        case AKEYCODE_NUMPAD_DIVIDE: return GLFW_KEY_KP_DIVIDE;
        case AKEYCODE_NUMPAD_MULTIPLY: return GLFW_KEY_KP_MULTIPLY;
        case AKEYCODE_NUMPAD_SUBTRACT: return GLFW_KEY_KP_SUBTRACT;
        case AKEYCODE_NUMPAD_ADD: return GLFW_KEY_KP_ADD;
        case AKEYCODE_NUMPAD_ENTER: return GLFW_KEY_KP_ENTER;
        case AKEYCODE_NUMPAD_EQUALS: return GLFW_KEY_KP_EQUAL;
        case AKEYCODE_SHIFT_LEFT: return GLFW_KEY_LEFT_SHIFT;
        case AKEYCODE_CTRL_LEFT: return GLFW_KEY_LEFT_CONTROL;
        case AKEYCODE_ALT_LEFT: return GLFW_KEY_LEFT_ALT;
        case AKEYCODE_META_LEFT: return GLFW_KEY_LEFT_SUPER;
        case AKEYCODE_SHIFT_RIGHT: return GLFW_KEY_RIGHT_SHIFT;
        case AKEYCODE_CTRL_RIGHT: return GLFW_KEY_RIGHT_CONTROL;
        case AKEYCODE_ALT_RIGHT: return GLFW_KEY_RIGHT_ALT;
        case AKEYCODE_META_RIGHT: return GLFW_KEY_RIGHT_SUPER;
        case AKEYCODE_MENU: return GLFW_KEY_MENU;
        default: return GLFW_KEY_UNKNOWN;
    }
}

int android_key_to_imgui(int32_t code) {
    if (code >= AKEYCODE_A && code <= AKEYCODE_Z) return ImGuiKey_A + (code - AKEYCODE_A);
    if (code >= AKEYCODE_0 && code <= AKEYCODE_9) return ImGuiKey_0 + (code - AKEYCODE_0);
    if (code >= AKEYCODE_F1 && code <= AKEYCODE_F12) return ImGuiKey_F1 + (code - AKEYCODE_F1);
    switch (code) {
        case AKEYCODE_TAB: return ImGuiKey_Tab;
        case AKEYCODE_DPAD_LEFT: return ImGuiKey_LeftArrow;
        case AKEYCODE_DPAD_RIGHT: return ImGuiKey_RightArrow;
        case AKEYCODE_DPAD_UP: return ImGuiKey_UpArrow;
        case AKEYCODE_DPAD_DOWN: return ImGuiKey_DownArrow;
        case AKEYCODE_PAGE_UP: return ImGuiKey_PageUp;
        case AKEYCODE_PAGE_DOWN: return ImGuiKey_PageDown;
        case AKEYCODE_MOVE_HOME: return ImGuiKey_Home;
        case AKEYCODE_MOVE_END: return ImGuiKey_End;
        case AKEYCODE_INSERT: return ImGuiKey_Insert;
        case AKEYCODE_FORWARD_DEL: return ImGuiKey_Delete;
        case AKEYCODE_DEL: return ImGuiKey_Backspace;
        case AKEYCODE_SPACE: return ImGuiKey_Space;
        case AKEYCODE_ENTER: return ImGuiKey_Enter;
        case AKEYCODE_ESCAPE: return ImGuiKey_Escape;
        default: return ImGuiKey_None;
    }
}

bool is_gamepad_keycode(int32_t code) {
    switch (code) {
        case AKEYCODE_BUTTON_A:
        case AKEYCODE_BUTTON_B:
        case AKEYCODE_BUTTON_X:
        case AKEYCODE_BUTTON_Y:
        case AKEYCODE_BUTTON_L1:
        case AKEYCODE_BUTTON_R1:
        case AKEYCODE_BUTTON_L2:
        case AKEYCODE_BUTTON_R2:
        case AKEYCODE_BUTTON_THUMBL:
        case AKEYCODE_BUTTON_THUMBR:
        case AKEYCODE_BUTTON_START:
        case AKEYCODE_BUTTON_SELECT:
        case AKEYCODE_BUTTON_MODE:
            return true;
        default:
            return false;
    }
}

bool is_dpad_keycode(int32_t code) {
    return code == AKEYCODE_DPAD_UP || code == AKEYCODE_DPAD_RIGHT ||
           code == AKEYCODE_DPAD_DOWN || code == AKEYCODE_DPAD_LEFT;
}

void build_pad_arrays(const PadDevice& d, bool (&buttons)[15], float (&axes)[6]) {
    for (int i = 0; i < 15; ++i) buttons[i] = d.raw_buttons[i];
    if (d.dpad_keys[0] || d.hat_y < -0.5f) buttons[GLFW_GAMEPAD_BUTTON_DPAD_UP] = true;
    if (d.dpad_keys[1] || d.hat_x > 0.5f) buttons[GLFW_GAMEPAD_BUTTON_DPAD_RIGHT] = true;
    if (d.dpad_keys[2] || d.hat_y > 0.5f) buttons[GLFW_GAMEPAD_BUTTON_DPAD_DOWN] = true;
    if (d.dpad_keys[3] || d.hat_x < -0.5f) buttons[GLFW_GAMEPAD_BUTTON_DPAD_LEFT] = true;

    axes[GLFW_GAMEPAD_AXIS_LEFT_X] = d.lx;
    axes[GLFW_GAMEPAD_AXIS_LEFT_Y] = d.ly;
    axes[GLFW_GAMEPAD_AXIS_RIGHT_X] = d.rx;
    axes[GLFW_GAMEPAD_AXIS_RIGHT_Y] = d.ry;
    axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = std::max(d.l2_axis * 2.0f - 1.0f, d.l2_key ? 1.0f : -1.0f);
    axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = std::max(d.r2_axis * 2.0f - 1.0f, d.r2_key ? 1.0f : -1.0f);
}

int32_t handle_key_event(const AInputEvent* ev) {
    const int32_t key_code = AKeyEvent_getKeyCode(ev);
    const int32_t src = AInputEvent_getSource(ev);
    const int32_t action = AKeyEvent_getAction(ev);

    // Let Android handle hardware volume / camera / power buttons normally.
    if (key_code == AKEYCODE_VOLUME_UP || key_code == AKEYCODE_VOLUME_DOWN ||
        key_code == AKEYCODE_VOLUME_MUTE || key_code == AKEYCODE_CAMERA) {
        return 0;
    }

    const bool down = (action == AKEY_EVENT_ACTION_DOWN);
    const bool valid_action = (action == AKEY_EVENT_ACTION_DOWN || action == AKEY_EVENT_ACTION_UP);
    const bool from_gamepad_src = ((src & kSourceGamepad) == kSourceGamepad) ||
                                  ((src & kSourceJoystick) == kSourceJoystick) ||
                                  ((src & kSourceDpad) == kSourceDpad);

    // Prevent Circle / controller Back from dismissing the activity unexpectedly.
    if (key_code == AKEYCODE_BACK && from_gamepad_src) {
        return 1;
    }

    std::lock_guard<std::mutex> lk(g_input_mu);

    // Queue keyboard events for Dear ImGui when menu is active.
    if (valid_action) {
        const int ik = android_key_to_imgui(key_code);
        if (ik != ImGuiKey_None) {
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kKey, ik, 0, 0.0f, 0.0f, down});
        }
    }

    if (from_gamepad_src || is_gamepad_keycode(key_code)) {
        const int32_t dev_id = AInputEvent_getDeviceId(ev);
        if (dev_id >= 0 && valid_action) {
            PadDevice& pad = get_or_register_pad_locked(dev_id);
            bool consumed = true;
            switch (key_code) {
                case AKEYCODE_BUTTON_A: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_A] = down; break;
                case AKEYCODE_BUTTON_B: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_B] = down; break;
                case AKEYCODE_BUTTON_X: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_X] = down; break;
                case AKEYCODE_BUTTON_Y: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_Y] = down; break;
                case AKEYCODE_BUTTON_L1: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_LEFT_BUMPER] = down; break;
                case AKEYCODE_BUTTON_R1: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_RIGHT_BUMPER] = down; break;
                case AKEYCODE_BUTTON_L2: pad.l2_key = down; break;
                case AKEYCODE_BUTTON_R2: pad.r2_key = down; break;
                case AKEYCODE_BUTTON_THUMBL: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_LEFT_THUMB] = down; break;
                case AKEYCODE_BUTTON_THUMBR: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_RIGHT_THUMB] = down; break;
                case AKEYCODE_BUTTON_START: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_START] = down; break;
                case AKEYCODE_BUTTON_SELECT: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_BACK] = down; break;
                case AKEYCODE_BUTTON_MODE: pad.raw_buttons[GLFW_GAMEPAD_BUTTON_GUIDE] = down; break;
                case AKEYCODE_DPAD_UP: pad.dpad_keys[0] = down; break;
                case AKEYCODE_DPAD_RIGHT: pad.dpad_keys[1] = down; break;
                case AKEYCODE_DPAD_DOWN: pad.dpad_keys[2] = down; break;
                case AKEYCODE_DPAD_LEFT: pad.dpad_keys[3] = down; break;
                default: consumed = false; break;
            }
            if (consumed) {
                pad.has_received_input = true;
                pad.last_active_seq = ++g_input_seq;
                return 1;
            }
        }
    }

    if (valid_action && !is_gamepad_keycode(key_code)) {
        const int gk = android_key_to_glfw(key_code);
        if (gk >= GLFW_KEY_SPACE && gk <= GLFW_KEY_LAST) {
            g_keys[static_cast<size_t>(gk)] = down;
            if (!is_dpad_keycode(key_code)) return 1;
        }
    }
    return 0;
}

int32_t handle_motion_event(const AInputEvent* ev) {
    const int32_t src = AInputEvent_getSource(ev);
    if ((src & AINPUT_SOURCE_CLASS_JOYSTICK) != 0) {
        const int32_t dev_id = AInputEvent_getDeviceId(ev);
        if (dev_id < 0) return 0;

        std::lock_guard<std::mutex> lk(g_input_mu);
        PadDevice& pad = get_or_register_pad_locked(dev_id);

        pad.lx = AMotionEvent_getAxisValue(ev, kAxisX, 0);
        pad.ly = AMotionEvent_getAxisValue(ev, kAxisY, 0);

        // Standard Android gamepad maps Right Stick to Z/RZ. Fall back to RX/RY only when Z/RZ don't exist
        // (and never misinterpret DualSense RX/RY trigger axes as the right stick).
        if (pad.has_z || pad.has_rz) {
            pad.rx = AMotionEvent_getAxisValue(ev, kAxisZ, 0);
            pad.ry = AMotionEvent_getAxisValue(ev, kAxisRz, 0);
        } else if ((pad.has_rx || pad.has_ry) && !pad.is_sony) {
            pad.rx = AMotionEvent_getAxisValue(ev, kAxisRx, 0);
            pad.ry = AMotionEvent_getAxisValue(ev, kAxisRy, 0);
        }

        if (pad.has_hat_x) pad.hat_x = AMotionEvent_getAxisValue(ev, kAxisHatX, 0);
        if (pad.has_hat_y) pad.hat_y = AMotionEvent_getAxisValue(ev, kAxisHatY, 0);

        float lt = 0.0f;
        if (pad.has_ltrigger) lt = std::max(lt, AMotionEvent_getAxisValue(ev, kAxisLTrigger, 0));
        if (pad.has_brake) lt = std::max(lt, AMotionEvent_getAxisValue(ev, kAxisBrake, 0));
        pad.l2_axis = std::clamp(lt, 0.0f, 1.0f);

        float rt = 0.0f;
        if (pad.has_rtrigger) rt = std::max(rt, AMotionEvent_getAxisValue(ev, kAxisRTrigger, 0));
        if (pad.has_gas) rt = std::max(rt, AMotionEvent_getAxisValue(ev, kAxisGas, 0));
        pad.r2_axis = std::clamp(rt, 0.0f, 1.0f);

        if (std::fabs(pad.lx) > 0.25f || std::fabs(pad.ly) > 0.25f ||
            std::fabs(pad.rx) > 0.25f || std::fabs(pad.ry) > 0.25f ||
            pad.l2_axis > 0.2f || pad.r2_axis > 0.2f ||
            std::fabs(pad.hat_x) > 0.5f || std::fabs(pad.hat_y) > 0.5f) {
            pad.has_received_input = true;
            pad.last_active_seq = ++g_input_seq;
        }
        return 1;
    }

    // Touch / physical mouse events (forwarded safely to ImGui on the render thread).
    const int32_t full_action = AMotionEvent_getAction(ev);
    const int32_t ptr_idx = (full_action & AMOTION_EVENT_ACTION_POINTER_INDEX_MASK) >> AMOTION_EVENT_ACTION_POINTER_INDEX_SHIFT;
    const int32_t action = full_action & AMOTION_EVENT_ACTION_MASK;
    const int32_t tool = AMotionEvent_getToolType(ev, ptr_idx);
    const float px = AMotionEvent_getX(ev, ptr_idx);
    const float py = AMotionEvent_getY(ev, ptr_idx);

    std::lock_guard<std::mutex> lk(g_input_mu);

    const int32_t source_kind = (tool == AMOTION_EVENT_TOOL_TYPE_MOUSE)
        ? static_cast<int32_t>(ImGuiMouseSource_Mouse)
        : ((tool == AMOTION_EVENT_TOOL_TYPE_STYLUS || tool == AMOTION_EVENT_TOOL_TYPE_ERASER)
            ? static_cast<int32_t>(ImGuiMouseSource_Pen)
            : static_cast<int32_t>(ImGuiMouseSource_TouchScreen));
    g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseSource, source_kind, 0, 0.0f, 0.0f, false});

    switch (action) {
        case AMOTION_EVENT_ACTION_DOWN:
        case AMOTION_EVENT_ACTION_UP: {
            if (tool == AMOTION_EVENT_TOOL_TYPE_FINGER || tool == AMOTION_EVENT_TOOL_TYPE_UNKNOWN) {
                g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMousePos, 0, 0, px, py, false});
                g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseButton, 0, 0, 0.0f, 0.0f, action == AMOTION_EVENT_ACTION_DOWN});
            }
            break;
        }
        case AMOTION_EVENT_ACTION_BUTTON_PRESS:
        case AMOTION_EVENT_ACTION_BUTTON_RELEASE: {
            const int32_t bstate = AMotionEvent_getButtonState(ev);
            g_mouse_buttons[0] = (bstate & AMOTION_EVENT_BUTTON_PRIMARY) != 0;
            g_mouse_buttons[1] = (bstate & AMOTION_EVENT_BUTTON_SECONDARY) != 0;
            g_mouse_buttons[2] = (bstate & AMOTION_EVENT_BUTTON_TERTIARY) != 0;
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseButton, 0, 0, 0.0f, 0.0f, g_mouse_buttons[0]});
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseButton, 1, 0, 0.0f, 0.0f, g_mouse_buttons[1]});
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseButton, 2, 0, 0.0f, 0.0f, g_mouse_buttons[2]});
            break;
        }
        case AMOTION_EVENT_ACTION_HOVER_MOVE:
        case AMOTION_EVENT_ACTION_MOVE: {
            if (tool == AMOTION_EVENT_TOOL_TYPE_MOUSE) {
                if (g_has_last_mouse) {
                    g_mouse_dx += (px - g_last_mouse_x);
                    g_mouse_dy += (py - g_last_mouse_y);
                }
                g_last_mouse_x = px;
                g_last_mouse_y = py;
                g_has_last_mouse = true;
            }
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMousePos, 0, 0, px, py, false});
            break;
        }
        case AMOTION_EVENT_ACTION_SCROLL: {
            const float hs = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_HSCROLL, ptr_idx);
            const float vs = AMotionEvent_getAxisValue(ev, AMOTION_EVENT_AXIS_VSCROLL, ptr_idx);
            g_wheel_accum += vs;
            g_imgui_queue.push_back(ImguiEvent{ImguiEvent::Type::kMouseWheel, 0, 0, hs, vs, false});
            break;
        }
        default:
            break;
    }
    return 1;
}

int32_t on_input_event(struct android_app* /*app*/, AInputEvent* ev) {
    if (ev == nullptr) return 0;
    const int32_t type = AInputEvent_getType(ev);
    if (type == AINPUT_EVENT_TYPE_KEY) return handle_key_event(ev);
    if (type == AINPUT_EVENT_TYPE_MOTION) return handle_motion_event(ev);
    return 0;
}

void on_app_cmd(struct android_app* app, int32_t cmd) {
    switch (cmd) {
        case APP_CMD_INIT_WINDOW: {
            if (app->window != nullptr) {
                const int w = std::max(1, ANativeWindow_getWidth(app->window));
                const int h = std::max(1, ANativeWindow_getHeight(app->window));
                std::lock_guard<std::mutex> lk(g_lifecycle_mu);
                g_window = app->window;
                g_win_w = w;
                g_win_h = h;
                if (g_vulkan_active.load(std::memory_order_acquire)) {
                    g_need_resume_surface = true;
                }
                g_surface_suspended = false;
                g_lifecycle_cv.notify_all();
                std::printf("[android] APP_CMD_INIT_WINDOW %dx%d\n", w, h);
            }
            break;
        }
        case APP_CMD_TERM_WINDOW: {
            std::printf("[android] APP_CMD_TERM_WINDOW\n");
            {
                std::lock_guard<std::mutex> lk(g_lifecycle_mu);
                g_surface_suspended = true;
                g_window = nullptr;
            }
            // Wait for any in-flight Vulkan frame on the game thread to finish, then destroy
            // the swapchain and VkSurfaceKHR BEFORE returning control to SurfaceFlinger.
            {
                std::lock_guard<std::recursive_mutex> rlk(g_render_mu);
                if (g_vulkan_active.load(std::memory_order_acquire)) {
                    rac1::vk::suspend_surface();
                }
            }
            g_lifecycle_cv.notify_all();
            break;
        }
        case APP_CMD_WINDOW_RESIZED:
        case APP_CMD_CONTENT_RECT_CHANGED: {
            if (app->window != nullptr) {
                const int w = std::max(1, ANativeWindow_getWidth(app->window));
                const int h = std::max(1, ANativeWindow_getHeight(app->window));
                std::lock_guard<std::mutex> lk(g_lifecycle_mu);
                g_win_w = w;
                g_win_h = h;
                g_lifecycle_cv.notify_all();
            }
            break;
        }
        case APP_CMD_PAUSE: {
            std::lock_guard<std::mutex> lk(g_lifecycle_mu);
            g_app_paused = true;
            g_lifecycle_cv.notify_all();
            break;
        }
        case APP_CMD_RESUME: {
            {
                std::lock_guard<std::mutex> lk(g_lifecycle_mu);
                g_app_paused = false;
                g_lifecycle_cv.notify_all();
            }
            scan_connected_gamepads();
            break;
        }
        case APP_CMD_GAINED_FOCUS: {
            scan_connected_gamepads();
            break;
        }
        case APP_CMD_DESTROY: {
            std::printf("[android] APP_CMD_DESTROY\n");
            g_should_close.store(true, std::memory_order_release);
            {
                std::lock_guard<std::mutex> lk(g_lifecycle_mu);
                g_surface_suspended = false;
                g_app_paused = false;
                g_window = nullptr;
            }
            g_lifecycle_cv.notify_all();
            break;
        }
        default:
            break;
    }
}

struct ThreadLaunch {
    int rc = 0;
};

void* game_thread_entry(void* arg) {
    auto* launch = static_cast<ThreadLaunch*>(arg);
    const char* argv0 = "rac1-native";
    char* argv[] = {const_cast<char*>(argv0), nullptr};
    launch->rc = rac1_orig_main(1, argv);
    g_game_exited.store(true, std::memory_order_release);
    request_close();
    if (g_app != nullptr && g_app->looper != nullptr) ALooper_wake(g_app->looper);
    return nullptr;
}

}  // namespace

ANativeWindow* native_window() {
    std::lock_guard<std::mutex> lk(g_lifecycle_mu);
    return g_window;
}

void window_size(int* w, int* h) {
    std::lock_guard<std::mutex> lk(g_lifecycle_mu);
    if (g_window != nullptr) {
        const int nw = ANativeWindow_getWidth(g_window);
        const int nh = ANativeWindow_getHeight(g_window);
        if (nw > 0 && nh > 0) {
            g_win_w = nw;
            g_win_h = nh;
        }
    }
    if (w != nullptr) *w = g_win_w;
    if (h != nullptr) *h = g_win_h;
}

int refresh_hz() {
    return g_refresh_hz.load(std::memory_order_relaxed);
}

bool should_close() {
    if (g_should_close.load(std::memory_order_acquire)) return true;
    return g_app != nullptr && g_app->destroyRequested != 0;
}

void request_close() {
    g_should_close.store(true, std::memory_order_release);
    {
        std::lock_guard<std::mutex> lk(g_lifecycle_mu);
        if (g_app != nullptr && g_app->activity != nullptr && !g_finish_requested) {
            g_finish_requested = true;
            ANativeActivity_finish(g_app->activity);
        }
    }
    g_lifecycle_cv.notify_all();
}

void set_vulkan_active(bool active) {
    g_vulkan_active.store(active, std::memory_order_release);
}

std::recursive_mutex& render_mutex() {
    return g_render_mu;
}

bool poll_events() {
    for (;;) {
        if (should_close()) return false;

        ANativeWindow* resume_win = nullptr;
        bool need_resume = false;
        {
            std::unique_lock<std::mutex> lk(g_lifecycle_mu);
            while ((g_surface_suspended || g_app_paused || g_window == nullptr) && !should_close()) {
                g_lifecycle_cv.wait_for(lk, std::chrono::milliseconds(50));
            }
            if (should_close()) return false;
            if (g_need_resume_surface && g_window != nullptr) {
                resume_win = g_window;
                need_resume = true;
                g_need_resume_surface = false;
            }
        }

        if (need_resume) {
            std::lock_guard<std::recursive_mutex> rlk(g_render_mu);
            if (g_vulkan_active.load(std::memory_order_acquire) && native_window() == resume_win) {
                rac1::vk::resume_surface(reinterpret_cast<GLFWwindow*>(resume_win));
            }
            continue;
        }
        return true;
    }
}

void fill_host_state(rac1::host_input::HostState& st, std::string& pad_name, std::string& pad_guid) {
    st = rac1::host_input::HostState{};
    pad_name.clear();
    pad_guid.clear();

    std::lock_guard<std::mutex> lk(g_input_mu);

    for (size_t k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST && k < st.keys.size(); ++k) {
        st.keys[k] = g_keys[k];
    }
    for (size_t m = 0; m < st.mouse_buttons.size() && m < g_mouse_buttons.size(); ++m) {
        st.mouse_buttons[m] = g_mouse_buttons[m];
    }
    st.mouse_dx = g_mouse_dx;
    st.mouse_dy = g_mouse_dy;
    g_mouse_dx = g_mouse_dy = 0.0f;

    if (g_wheel_accum > 0.5f) {
        st.wheel_up = true;
        g_wheel_accum -= 1.0f;
    } else if (g_wheel_accum < -0.5f) {
        st.wheel_down = true;
        g_wheel_accum += 1.0f;
    }

    const size_t n = g_pads.size();
    st.pad_present = n > 0;
    const PadDevice* best = nullptr;
    for (const auto& kv : g_pads) {
        const PadDevice& d = kv.second;
        bool buttons[15]{};
        float axes[6]{};
        build_pad_arrays(d, buttons, axes);
        if (n == 1 || d.has_received_input) {
            rac1::host_input::merge_pad(st, buttons, axes);
        }
        if (best == nullptr || d.last_active_seq > best->last_active_seq) {
            best = &d;
        }
    }
    if (best != nullptr) {
        pad_name = best->name;
        pad_guid = best->guid;
    }
}

bool is_key_down(int glfw_key) {
    if (glfw_key < 0 || glfw_key > GLFW_KEY_LAST) return false;
    std::lock_guard<std::mutex> lk(g_input_mu);
    return g_keys[static_cast<size_t>(glfw_key)];
}

bool get_primary_gamepad(GLFWgamepadstate& out) {
    out = GLFWgamepadstate{};
    for (float& a : out.axes) a = 0.0f;
    out.axes[GLFW_GAMEPAD_AXIS_LEFT_TRIGGER] = -1.0f;
    out.axes[GLFW_GAMEPAD_AXIS_RIGHT_TRIGGER] = -1.0f;

    std::lock_guard<std::mutex> lk(g_input_mu);
    if (g_pads.empty()) return false;

    const PadDevice* best = nullptr;
    for (const auto& kv : g_pads) {
        if (best == nullptr || kv.second.last_active_seq > best->last_active_seq) best = &kv.second;
    }
    if (best == nullptr) return false;

    bool buttons[15]{};
    float axes[6]{};
    build_pad_arrays(*best, buttons, axes);
    for (int i = 0; i < 15; ++i) out.buttons[i] = buttons[i] ? GLFW_PRESS : GLFW_RELEASE;
    for (int i = 0; i < 6; ++i) out.axes[i] = axes[i];
    return true;
}

void sync_imgui_window() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    ANativeWindow* nw = native_window();
    if (nw != nullptr && nw != g_imgui_window) {
        ImGui_ImplAndroid_Shutdown();
        ImGui_ImplAndroid_Init(nw);
        g_imgui_window = nw;
    }
}

void flush_imgui_frame() {
    if (ImGui::GetCurrentContext() == nullptr) return;
    sync_imgui_window();

    std::vector<ImguiEvent> evs;
    {
        std::lock_guard<std::mutex> lk(g_input_mu);
        evs.swap(g_imgui_queue);
    }

    ImGuiIO& io = ImGui::GetIO();
    for (const ImguiEvent& e : evs) {
        switch (e.type) {
            case ImguiEvent::Type::kKey:
                io.AddKeyEvent(static_cast<ImGuiKey>(e.a), e.down);
                break;
            case ImguiEvent::Type::kMouseSource:
                io.AddMouseSourceEvent(static_cast<ImGuiMouseSource>(e.a));
                break;
            case ImguiEvent::Type::kMousePos:
                io.AddMousePosEvent(e.fx, e.fy);
                break;
            case ImguiEvent::Type::kMouseButton:
                io.AddMouseButtonEvent(e.a, e.down);
                break;
            case ImguiEvent::Type::kMouseWheel:
                io.AddMouseWheelEvent(e.fx, e.fy);
                break;
        }
    }

    if (native_window() != nullptr) {
        ImGui_ImplAndroid_NewFrame();
    } else {
        int w = 1280, h = 720;
        window_size(&w, &h);
        io.DisplaySize = ImVec2(static_cast<float>(w), static_cast<float>(h));
    }
}

}  // namespace rac1::android_host

extern "C" void android_main(struct android_app* app) {
    using namespace rac1::android_host;

    {
        std::lock_guard<std::mutex> lk(g_lifecycle_mu);
        g_app = app;
        g_window = nullptr;
        g_imgui_window = nullptr;
        g_surface_suspended = false;
        g_app_paused = false;
        g_need_resume_surface = false;
        g_finish_requested = false;
    }
    g_vulkan_active.store(false, std::memory_order_relaxed);
    g_should_close.store(false, std::memory_order_relaxed);
    g_game_exited.store(false, std::memory_order_relaxed);

    app->userData = nullptr;
    app->onAppCmd = on_app_cmd;
    app->onInputEvent = on_input_event;

    configure_environment(app);
    scan_connected_gamepads();

    auto pump_looper = [&](int timeout_ms) {
        int events = 0;
        struct android_poll_source* source = nullptr;
        const int ident = ALooper_pollOnce(timeout_ms, nullptr, &events, reinterpret_cast<void**>(&source));
        if (ident >= 0 && source != nullptr) {
            source->process(app, source);
        }
        // Drain any remaining ready sources without sleeping.
        while (!app->destroyRequested) {
            source = nullptr;
            const int next = ALooper_pollOnce(0, nullptr, &events, reinterpret_cast<void**>(&source));
            if (next < 0) break;
            if (source != nullptr) source->process(app, source);
        }
    };

    // Wait for Android to grant the initial ANativeWindow before booting rac1_orig_main.
    while (!app->destroyRequested && native_window() == nullptr) {
        pump_looper(16);
    }

    if (!app->destroyRequested && native_window() != nullptr) {
        ThreadLaunch launch{};
        pthread_attr_t attr;
        pthread_attr_init(&attr);
        pthread_attr_setstacksize(&attr, 32u << 20);  // Bionic default is 1 MB; desktop/game uses 32 MB headroom.
        pthread_t tid{};
        const int perr = pthread_create(&tid, &attr, game_thread_entry, &launch);
        pthread_attr_destroy(&attr);
        if (perr != 0) {
            __android_log_print(ANDROID_LOG_ERROR, kLogTag, "pthread_create failed: %d", perr);
            request_close();
        } else {
            while (!app->destroyRequested) {
                pump_looper(-1);
            }
            pthread_join(tid, nullptr);
        }
    }

    std::lock_guard<std::mutex> lk(g_lifecycle_mu);
    g_app = nullptr;
}
