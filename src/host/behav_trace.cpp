// See behav_trace.h.
#include <host/behav_trace.h>

#include <game/lib/snd989.h>
#include <game/ovl/globals.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <runtime/kernel/ee_memory.h>

#include <algorithm>
#include <bit>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <vector>

namespace rac1 {

namespace {

constexpr uint8_t kMagic[4] = {'R', '1', 'B', 'T'};
constexpr uint32_t kVersion = 1;

struct AudioEvent {
    uint16_t kind = 0;   // 1 = sfx play, 2 = stream play, 3 = stop all streams
    uint16_t flags = 0;  // stream flags (kind 2), 0 otherwise
    uint32_t id = 0;     // sound index in bank (kind 1) or stream group (kind 2)
    int16_t vol = 0;
    int16_t pan = 0;
    int16_t pm = 0;      // pitch mod (kind 1) or queued-flag (kind 2)
    int16_t pb = 0;      // pitch bend (kind 1), 0 otherwise
};
static_assert(sizeof(AudioEvent) == 16);

struct MobySnap {
    int16_t class_id = 0;
    int16_t uid = -1;
    uint16_t slot_idx = 0;
    uint16_t mode_bits = 0;
    int8_t state = 0;
    int8_t group_id = -1;
    uint8_t anim_cur = 0;
    uint8_t anim_next = 0;
    uint8_t anim_frame = 0;
    uint8_t drawn = 0;
    uint16_t pad = 0;
    float pos[3] = {};
    float rot[3] = {};
    float scale = 0.0f;
    float anim_pos = 0.0f;
};
static_assert(sizeof(MobySnap) == 48);

FILE* g_file = nullptr;
bool g_active = false;
std::vector<AudioEvent> g_audio_events;
std::vector<MobySnap> g_mobys;
std::vector<uint8_t> g_out_buf;

inline void put_u8(std::vector<uint8_t>& b, uint8_t v) { b.push_back(v); }
inline void put_i8(std::vector<uint8_t>& b, int8_t v) { b.push_back(static_cast<uint8_t>(v)); }
inline void put_u16_le(std::vector<uint8_t>& b, uint16_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xffu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xffu));
}
inline void put_i16_le(std::vector<uint8_t>& b, int16_t v) { put_u16_le(b, static_cast<uint16_t>(v)); }
inline void put_u32_le(std::vector<uint8_t>& b, uint32_t v) {
    b.push_back(static_cast<uint8_t>(v & 0xffu));
    b.push_back(static_cast<uint8_t>((v >> 8) & 0xffu));
    b.push_back(static_cast<uint8_t>((v >> 16) & 0xffu));
    b.push_back(static_cast<uint8_t>((v >> 24) & 0xffu));
}
inline void put_i32_le(std::vector<uint8_t>& b, int32_t v) { put_u32_le(b, static_cast<uint32_t>(v)); }
inline void put_u64_le(std::vector<uint8_t>& b, uint64_t v) {
    put_u32_le(b, static_cast<uint32_t>(v & 0xffffffffull));
    put_u32_le(b, static_cast<uint32_t>(v >> 32));
}
inline void put_f32_le(std::vector<uint8_t>& b, float v) { put_u32_le(b, std::bit_cast<uint32_t>(v)); }

inline int16_t clamp_i16(int32_t v) {
    if (v > 32767) return 32767;
    if (v < -32768) return -32768;
    return static_cast<int16_t>(v);
}

inline bool valid_ram_range(uint32_t addr, uint32_t bytes) {
    const uint32_t end = ee_game_ram_end();
    return addr >= 0x00100000u && addr < end && bytes <= end - addr;
}

}  // namespace

void behav_trace_init() {
    if (g_file != nullptr) return;
    const char* path = std::getenv("RAC1_BEHAV_TRACE");
    if (path == nullptr || *path == '\0') return;
    g_file = std::fopen(path, "wb");
    if (g_file == nullptr) {
        std::fprintf(stderr, "[behav_trace] failed to open '%s' for writing\n", path);
        return;
    }
    std::vector<uint8_t> hdr;
    hdr.reserve(16);
    hdr.insert(hdr.end(), std::begin(kMagic), std::end(kMagic));
    put_u32_le(hdr, kVersion);
    put_u32_le(hdr, 0u);  // flags
    put_u32_le(hdr, 0u);  // reserved
    std::fwrite(hdr.data(), 1, hdr.size(), g_file);
    g_audio_events.clear();
    g_audio_events.reserve(64);
    g_mobys.reserve(512);
    g_out_buf.reserve(32768);
    g_active = true;
    SndTraceHooks hooks{};
    hooks.sfx = &behav_trace_audio_sfx;
    hooks.stream = &behav_trace_audio_stream;
    hooks.stop_streams = &behav_trace_audio_stop_streams;
    snd_set_trace_hooks(hooks);
    std::printf("[behav_trace] recording R1BT v%u trace to %s\n", kVersion, path);
}

void behav_trace_shutdown() {
    snd_set_trace_hooks(SndTraceHooks{});
    if (g_file != nullptr) {
        std::fflush(g_file);
        std::fclose(g_file);
        g_file = nullptr;
    }
    g_active = false;
    g_audio_events.clear();
}

bool behav_trace_active() { return g_active; }

void behav_trace_audio_sfx(uint32_t snd_index, int32_t vol, int32_t pan, int32_t pm, int32_t pb) {
    if (!g_active || ee_journal_active()) return;
    AudioEvent ev{};
    ev.kind = 1;
    ev.flags = 0;
    ev.id = snd_index;
    ev.vol = clamp_i16(vol);
    ev.pan = clamp_i16(pan);
    ev.pm = clamp_i16(pm);
    ev.pb = clamp_i16(pb);
    g_audio_events.push_back(ev);
}

void behav_trace_audio_stream(uint32_t group, int32_t vol, int32_t pan, uint32_t queue, uint32_t flags) {
    if (!g_active || ee_journal_active()) return;
    AudioEvent ev{};
    ev.kind = 2;
    ev.flags = static_cast<uint16_t>(flags & 0xffffu);
    ev.id = group;
    ev.vol = clamp_i16(vol);
    ev.pan = clamp_i16(pan);
    ev.pm = queue != 0 ? 1 : 0;
    ev.pb = 0;
    g_audio_events.push_back(ev);
}

void behav_trace_audio_stop_streams() {
    if (!g_active || ee_journal_active()) return;
    AudioEvent ev{};
    ev.kind = 3;
    g_audio_events.push_back(ev);
}

void behav_trace_frame(uint64_t game_frame) {
    if (!g_active || g_file == nullptr) return;

    constexpr uint32_t P = g::PLAYER;        // 0x0013f350
    constexpr uint32_t W_AMMO = 0x0013d428u; // per-item ammo table

    const int32_t level_id = lw(0x0015ed84u);
    const int32_t ovl_unit = ovl_reloc_level();
    const int32_t game_mode = lw(OV(0x0015f5c4u));

    const uint32_t reent = lwu(0x0012f76cu);
    const uint32_t rng_seed = valid_ram_range(reent, 0x60u) ? lwu(reent + 0x58u) : 0u;
    const int32_t bolts = lw(0x0015ed98u);

    const uint32_t hero_moby = lwu(0x001413d0u);
    const bool has_hero_moby = valid_ram_range(hero_moby, 0x100u);
    const int32_t hero_state = lw(0x001413d4u);
    const int32_t hero_substate = lw(0x001413dcu);
    const int32_t hero_health = lw(0x001415f8u);
    const int32_t hero_item = lw(P + 0x10b8u);
    const int32_t hero_ammo = (hero_item >= 0 && hero_item < 0x25) ? lw(W_AMMO + static_cast<uint32_t>(hero_item) * 4u) : 0;

    float hero_pos[3] = {lwc1(P + 0x80u), lwc1(P + 0x84u), lwc1(P + 0x88u)};
    float hero_rot[3] = {lwc1(P + 0x90u), lwc1(P + 0x94u), lwc1(P + 0x98u)};
    float hero_vel[3] = {lwc1(P + 0xe0u), lwc1(P + 0xe4u), lwc1(P + 0xe8u)};
    const float hero_anim_pos = has_hero_moby ? lwc1(hero_moby + 0x54u) : 0.0f;
    const uint8_t hero_mode = lbu(P + 0x20a4u);
    const uint8_t hero_anim_cur = has_hero_moby ? lbu(hero_moby + 0x52u) : 0u;
    const uint8_t hero_anim_next = has_hero_moby ? lbu(hero_moby + 0x53u) : 0u;
    const uint8_t hero_anim_frame = has_hero_moby ? lbu(hero_moby + 0x50u) : 0u;

    const uint32_t active_cam = lwu(OV(0x00166e00u));
    const int32_t cam_type = valid_ram_range(active_cam, 0x90u) ? static_cast<int32_t>(lh(active_cam + 0x86u)) : -1;
    const uint32_t cam_pos_addr = OV(0x00166dc0u);
    const uint32_t cam_rot_addr = OV(0x00166dd0u);
    float cam_pos[3] = {lwc1(cam_pos_addr), lwc1(cam_pos_addr + 4u), lwc1(cam_pos_addr + 8u)};
    float cam_rot[3] = {lwc1(cam_rot_addr), lwc1(cam_rot_addr + 4u), lwc1(cam_rot_addr + 8u)};
    const float cam_fov = lwc1(OV(0x0016caf0u));

    const uint32_t ms_addr = OV(0x001b9cf0u);
    const int32_t ui_menu_state = (game_mode == 3) ? lw(ms_addr) : -1;
    const int32_t ui_menu_page = (game_mode == 3) ? lw(ms_addr + 4u) : -1;

    // Collect live mobys in canonical layout-independent order.
    g_mobys.clear();
    const uint32_t moby_base = lwu(OV(0x0015ffd8u));
    const uint32_t moby_end = lwu(OV(0x0015ffe0u));
    if (valid_ram_range(moby_base, 0x100u) && moby_end >= moby_base && valid_ram_range(moby_end, 0x100u)) {
        uint32_t slot = 0;
        for (uint32_t m = moby_base; m <= moby_end && slot < 4096u; m += 0x100u, ++slot) {
            const int8_t st = static_cast<int8_t>(lbu(m + 0x20u));
            if (st == -1) break;
            if (st < 0) continue;
            MobySnap s{};
            s.class_id = lh(m + 0xa6u);
            s.uid = lh(m + 0xb2u);
            s.slot_idx = static_cast<uint16_t>(slot);
            s.mode_bits = lhu(m + 0x34u);
            s.state = st;
            s.group_id = static_cast<int8_t>(lbu(m + 0x21u));
            s.anim_cur = lbu(m + 0x52u);
            s.anim_next = lbu(m + 0x53u);
            s.anim_frame = lbu(m + 0x50u);
            s.drawn = lbu(m + 0x31u);
            s.pad = 0;
            s.pos[0] = lwc1(m + 0x10u);
            s.pos[1] = lwc1(m + 0x14u);
            s.pos[2] = lwc1(m + 0x18u);
            s.rot[0] = lwc1(m + 0x40u);
            s.rot[1] = lwc1(m + 0x44u);
            s.rot[2] = lwc1(m + 0x48u);
            s.scale = lwc1(m + 0x2cu);
            s.anim_pos = lwc1(m + 0x54u);
            g_mobys.push_back(s);
        }
        std::sort(g_mobys.begin(), g_mobys.end(), [](const MobySnap& a, const MobySnap& b) {
            const int a_dyn = a.uid >= 0 ? 0 : 1;
            const int b_dyn = b.uid >= 0 ? 0 : 1;
            if (a_dyn != b_dyn) return a_dyn < b_dyn;
            if (a_dyn == 0 && a.uid != b.uid) return a.uid < b.uid;
            if (a.slot_idx != b.slot_idx) return a.slot_idx < b.slot_idx;
            return a.class_id < b.class_id;
        });
    }

    const uint32_t moby_count = static_cast<uint32_t>(g_mobys.size());
    const uint32_t audio_count = static_cast<uint32_t>(g_audio_events.size());

    g_out_buf.clear();
    g_out_buf.reserve(144u + moby_count * 48u + audio_count * 16u);

    // 144-byte frame header
    put_u64_le(g_out_buf, game_frame);
    put_i32_le(g_out_buf, level_id);
    put_i32_le(g_out_buf, ovl_unit);
    put_i32_le(g_out_buf, game_mode);
    put_u32_le(g_out_buf, rng_seed);
    put_i32_le(g_out_buf, bolts);
    put_i32_le(g_out_buf, hero_state);
    put_i32_le(g_out_buf, hero_substate);
    put_i32_le(g_out_buf, hero_health);
    put_i32_le(g_out_buf, hero_item);
    put_i32_le(g_out_buf, hero_ammo);
    for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, hero_pos[k]);
    for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, hero_rot[k]);
    for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, hero_vel[k]);
    put_f32_le(g_out_buf, hero_anim_pos);
    put_u8(g_out_buf, hero_mode);
    put_u8(g_out_buf, hero_anim_cur);
    put_u8(g_out_buf, hero_anim_next);
    put_u8(g_out_buf, hero_anim_frame);
    put_i32_le(g_out_buf, cam_type);
    for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, cam_pos[k]);
    for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, cam_rot[k]);
    put_f32_le(g_out_buf, cam_fov);
    put_i32_le(g_out_buf, ui_menu_state);
    put_i32_le(g_out_buf, ui_menu_page);
    put_u32_le(g_out_buf, moby_count);
    put_u32_le(g_out_buf, audio_count);
    put_u32_le(g_out_buf, 0u);  // pad_reserved

    // Moby records (48 bytes each)
    for (const MobySnap& s : g_mobys) {
        put_i16_le(g_out_buf, s.class_id);
        put_i16_le(g_out_buf, s.uid);
        put_u16_le(g_out_buf, s.slot_idx);
        put_u16_le(g_out_buf, s.mode_bits);
        put_i8(g_out_buf, s.state);
        put_i8(g_out_buf, s.group_id);
        put_u8(g_out_buf, s.anim_cur);
        put_u8(g_out_buf, s.anim_next);
        put_u8(g_out_buf, s.anim_frame);
        put_u8(g_out_buf, s.drawn);
        put_u16_le(g_out_buf, 0u);
        for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, s.pos[k]);
        for (int k = 0; k < 3; ++k) put_f32_le(g_out_buf, s.rot[k]);
        put_f32_le(g_out_buf, s.scale);
        put_f32_le(g_out_buf, s.anim_pos);
    }

    // Audio event records (16 bytes each)
    for (const AudioEvent& ev : g_audio_events) {
        put_u16_le(g_out_buf, ev.kind);
        put_u16_le(g_out_buf, ev.flags);
        put_u32_le(g_out_buf, ev.id);
        put_i16_le(g_out_buf, ev.vol);
        put_i16_le(g_out_buf, ev.pan);
        put_i16_le(g_out_buf, ev.pm);
        put_i16_le(g_out_buf, ev.pb);
    }

    std::fwrite(g_out_buf.data(), 1, g_out_buf.size(), g_file);
    g_audio_events.clear();
}

}  // namespace rac1
