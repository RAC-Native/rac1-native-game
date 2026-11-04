// See custom_waves.h.
#include <host/custom_waves.h>

#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/disc.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

namespace rac1 {

namespace {

constexpr uint64_t kIntro = 90, kBetween = 150, kEndShown = 600;  // (game frames: before the first wave, between two, the end's line)

struct Enemy {
    int32_t uid = 0;
    uint32_t slot = 0;  // its moby (0: not found)
    int32_t cls = -1;
    bool dead = false;
};
struct Waves {
    bool on = false;            // (the custom level's challenge read)
    uint32_t base = 0;          // the moby list it was found in (another: the level loaded again)
    uint32_t tick = 0;          // the level's frame count *(0x15f5cc) last seen (smaller: the level loaded again, e.g. after a death)
    std::vector<std::vector<Enemy>> waves;
    int wave = -1;              // the wave running (-1: not yet)
    uint64_t next_at = 0;       // the frame the next wave shows
    uint64_t end_at = 0;        // (the round over) the frame it ended
    int kills = 0;
    bool done = false;
    uint64_t started = 0;       // the frame the running wave showed
} g_w;

bool level_on() {
    return custom_level_active(0) && lw(0x0015ed84) == 0 && lwu(OV(0x0015ffd8)) != 0;
}

// orig: (none) — the custom level's waves.txt
bool waves_read(Waves& w) {
    std::ifstream f(custom_level_dir() + "/waves.txt");
    if (!f) return false;
    std::string line;
    while (std::getline(f, line)) {
        std::istringstream in(line);
        std::string word;
        if (!(in >> word) || word != "wave") continue;
        std::vector<Enemy> wave;
        for (int32_t uid; in >> uid;) wave.push_back(Enemy{uid});
        if (!wave.empty()) w.waves.push_back(std::move(wave));
    }
    return !w.waves.empty();
}

// orig: (none) — the moby with that uid (+0xb2), alive, in the list (to the end marker, state 0xff); 0 none
uint32_t moby_by_uid(int32_t uid) {
    for (uint32_t m = lwu(OV(0x0015ffd8)); lbu(m + 0x20) != 0xff; m += 0x100)
        if (static_cast<int8_t>(lbu(m + 0x20)) >= 0 && lh(m + 0xb2) == uid) return m;
    return 0;
}

int alive(const std::vector<Enemy>& wave) {
    int n = 0;
    for (const Enemy& e : wave) n += e.dead ? 0 : 1;
    return n;
}

// orig: (none) — wave k shown: its enemies found and their hidden flag (+0x34 bit 2) cleared
void wave_start(uint64_t frame, int k) {
    g_w.wave = k;
    g_w.started = frame;
    for (Enemy& e : g_w.waves[static_cast<size_t>(k)]) {
        e.slot = moby_by_uid(e.uid);
        e.dead = e.slot == 0;
        if (e.slot == 0) continue;
        e.cls = lh(e.slot + 0xa6);
        sh(e.slot + 0x34, lhu(e.slot + 0x34) & ~3u);  // (bit 1: not updated; bit 0: not drawn)
    }
    std::printf("[waves] frame %llu: wave %d of %zu, %d enemies\n", static_cast<unsigned long long>(frame), k + 1, g_w.waves.size(),
                alive(g_w.waves[static_cast<size_t>(k)]));
}

}  // namespace

// orig: (none) — see custom_waves.h
void custom_waves_frame(uint64_t frame) {
    if (!level_on()) {
        if (g_w.on || g_w.base != 0) g_w = Waves{};
        return;
    }
    if (lw(OV(0x0015f5c4)) != 0) return;  // (game mode 0 only: not in a menu, a scene, a loading screen)
    const uint32_t base = lwu(OV(0x0015ffd8)), tick = lwu(OV(0x0015f5cc));
    const bool reloaded = tick < g_w.tick;
    g_w.tick = tick;
    if (g_w.base != base || reloaded) {  // (the level loaded: the challenge from the start)
        g_w = Waves{};
        g_w.base = base;
        g_w.on = waves_read(g_w);
        g_w.next_at = frame + kIntro;
        g_w.tick = tick;
        if (g_w.on) std::printf("[waves] %zu waves (%s/waves.txt)\n", g_w.waves.size(), custom_level_dir().c_str());
    }
    if (!g_w.on) return;
    if (g_w.done) {
        if (g_w.end_at != 0 && frame > g_w.end_at + kEndShown) g_w.end_at = 0;  // (the end's line shown for a while)
        return;
    }
    if (g_w.wave < 0 || (alive(g_w.waves[static_cast<size_t>(g_w.wave)]) == 0 && g_w.next_at != 0)) {
        if (frame < g_w.next_at) return;
        wave_start(frame, g_w.wave + 1);
        g_w.next_at = 0;
    }
    std::vector<Enemy>& wave = g_w.waves[static_cast<size_t>(g_w.wave)];
    const float death = lwc1(OV(0x0015f638));
    // (testing) RAC1_WAVES_AUTOKILL=<frames>: each wave's enemies killed that many frames after it shows (state 0xfe), so a
    // headless run goes through the whole round
    static const uint64_t autokill = [] {
        const char* e = std::getenv("RAC1_WAVES_AUTOKILL");
        return e != nullptr ? std::strtoull(e, nullptr, 10) : 0ull;
    }();
    if (autokill != 0 && frame == g_w.started + autokill)
        for (const Enemy& e : wave)
            if (!e.dead) sb(e.slot + 0x20, 0xfe);
    for (Enemy& e : wave) {
        if (e.dead) continue;
        const uint32_t m = e.slot;
        bool gone = static_cast<int8_t>(lbu(m + 0x20)) < 0 || lh(m + 0xb2) != e.uid || lh(m + 0xa6) != e.cls;
        if (!gone && lwc1(m + 0x18) < death) {  // (fallen off the arena: killed, as RAC1_KILL_UIDS does)
            sb(m + 0x20, 0xfe);
            gone = true;
        }
        if (gone) {
            e.dead = true;
            ++g_w.kills;
            std::printf("[waves] frame %llu: uid %d (class %d) killed (state %02x, uid %d, class %d, z %.1f)\n", static_cast<unsigned long long>(frame),
                        e.uid, e.cls, lbu(m + 0x20), lh(m + 0xb2), lh(m + 0xa6), static_cast<double>(lwc1(m + 0x18)));
        }
    }
    if (alive(wave) == 0 && g_w.next_at == 0) {
        if (static_cast<size_t>(g_w.wave) + 1 < g_w.waves.size()) {
            g_w.next_at = frame + kBetween;
            // the reward: the nanotech filled (as nanotech.cpp hero_health_max: Clank's 4, else *(0x15eda0))
            sw(0x001415f8, lbu(0x0013f350 + 0x20a4) == 1 ? 4u : lwu(0x0015eda0));
            std::printf("[waves] frame %llu: wave %d cleared, %d kills\n", static_cast<unsigned long long>(frame), g_w.wave + 1, g_w.kills);
        } else {
            g_w.done = true;
            g_w.end_at = frame;
            std::printf("[waves] frame %llu: the round is over, %d kills\n", static_cast<unsigned long long>(frame), g_w.kills);
        }
    }
}

// orig: (none) — see custom_waves.h
void custom_waves_hud() {
    if (!g_w.on || !level_on() || lw(OV(0x0015f5c4)) != 0) return;
    char text[64];
    const size_t n = g_w.waves.size();
    if (g_w.done) {
        if (g_w.end_at == 0) return;
        std::snprintf(text, sizeof text, "ROUND COMPLETE - %d KILLS", g_w.kills);
    } else if (g_w.wave < 0) {
        std::snprintf(text, sizeof text, "GET READY - WAVE 1 OF %zu", n);
    } else if (g_w.next_at != 0) {
        std::snprintf(text, sizeof text, "WAVE %d CLEARED - NEXT: WAVE %d OF %zu", g_w.wave + 1, g_w.wave + 2, n);
    } else {
        std::snprintf(text, sizeof text, "WAVE %d OF %zu - ENEMIES %d - KILLS %d", g_w.wave + 1, n, alive(g_w.waves[static_cast<size_t>(g_w.wave)]),
                      g_w.kills);
    }
    const uint32_t len = static_cast<uint32_t>(std::strlen(text));
    const uint32_t s = ee_stack_alloc(len + 1);
    for (uint32_t i = 0; i <= len; ++i) sb(s + i, static_cast<uint8_t>(text[i]));
    const uint64_t col = static_cast<uint64_t>(static_cast<int64_t>(static_cast<int32_t>((0x80u << 24) + 0xf0f0f0u)));
    hud_message_box(0x100, 0x48, col, s);
    ee_stack_free(len + 1);
}

}  // namespace rac1
