// orig: (host) The pose memo of the second moby pass (see mrp_pose_memo.h).
#include <game/render/mrp_pose_memo.h>

#include <game/ovl/moby_joints.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/moby_skin.h>
#include <game/ovl/ovl_common.h>
#include <game/render/mrp_scene.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <unordered_map>
#include <vector>

namespace rac1 {

namespace {

constexpr uint32_t SPR = 0x70000000u;

// RAC1_PERF_POSE_MEMO: 0 off, 1 on, 2 verify
const int g_mode = [] {
    const char* v = std::getenv("RAC1_PERF_POSE_MEMO");
    return v == nullptr ? 1 : std::atoi(v);
}();

struct Entry {
    std::vector<uint8_t> key;
    std::vector<uint8_t> joints;
};
std::unordered_map<uint64_t, Entry> g_memo;

// the pose asked now: its slot and its key, kept from lookup to store
bool g_pending = false;
uint64_t g_slot = 0;
std::vector<uint8_t> g_key;
// (verify) the matrices a hit would have given
bool g_expect = false;
std::vector<uint8_t> g_expected;

struct Stats {
    uint64_t asked = 0, no_joints = 0, hits = 0, misses = 0, bypass = 0, verified = 0, differing = 0, joints_hit = 0, joints_all = 0;
} g_st;

// orig: (host) `n` bytes at the EE address `a` (RAM below 64 MB or the scratchpad) appended to the key; false: not plain memory
bool add(uint32_t a, uint32_t n) {
    if (n == 0) return true;
    const bool spr = a >= SPR && a - SPR <= 0x4000u - n;
    const bool ram = a != 0 && a < 0x04000000u && n <= 0x04000000u - a;
    if (!spr && !ram) return false;
    const uint8_t* p = static_cast<const uint8_t*>(ee_ram_ptr_read(a));
    if (p == nullptr) return false;
    g_key.insert(g_key.end(), p, p + n);
    return true;
}

// orig: (host) the bytes the pose of `job` reads (moby_joints.cpp moby_joints_pose), in g_key; false: a case left to the pose
bool build_key(uint32_t job, int32_t n) {
    g_key.clear();
    if (!add(job, 0x40)) return false;  // the job (+0 .. +0x3e)
    // the toSPR chain the pose builds at work + 0x1fc0 (spr_dma_to_chain): three tags from the job's words
    const uint32_t key2 = lwu(job + 4);
    const uint32_t lo[3] = {0x30000000u | static_cast<uint32_t>(n), (0x30000000u | lwu(job + 0x24)) & (key2 == 0 ? 0xffffu : 0xffffffffu), lwu(job + 0x34)};
    const uint32_t hi[3] = {lwu(job + 0x18), lwu(job + 0x20), lwu(job + 0x30)};
    bool ended = false;
    for (int t = 0; t < 3 && !ended; ++t) {
        const uint32_t qwc = lo[t] & 0xffffu, id = (lo[t] >> 28) & 7u, addr = hi[t] & 0x7ffffff0u;
        if (id == 0) ended = true;  // refe
        else if (id == 3 || id == 4) {
        }  // ref / refs: on
        else if (id == 5 || id == 6) return false;  // (the walk stops without a transfer: not seen; left to the pose)
        else return false;  // cnt / next / end: data from the work buffer's chain itself
        if (qwc * 16u > 0x2000u) return false;
        if (!add(addr, qwc * 16u)) return false;
    }
    if (!ended) return false;  // (a fourth tag would come from the work buffer)
    // the blend channels: +2 the joints (x 3 quadwords of keys at +0x10), +8 the weight, +0x1c the next
    uint32_t t9 = lwu(job + 8);
    for (int g = 0; t9 != 0; ++g) {
        if (g >= 64 || !add(t9, 0x20)) return false;
        const uint32_t qwc = static_cast<uint32_t>(lhu(t9 + 2)) * 3u;
        if (qwc == 0 || qwc * 16u > 0x2000u) return false;
        if (!add(lwu(t9 + 0x10) & ~15u, qwc * 16u)) return false;
        t9 = lwu(t9 + 0x1c);
    }
    // the additive layers (0x40 each, +8 the next)
    uint32_t t8 = lwu(job + 0xc);
    for (int g = 0; t8 != 0; ++g) {
        if (g >= 256 || !add(t8, 0x40)) return false;
        t8 = lwu(t8 + 8);
    }
    // the bind matrices
    const uint32_t bq = (static_cast<uint32_t>(n) << 2) & 0xffffu;
    if (!add(lwu(job + 0x1c) & ~15u, bq * 16u)) return false;
    return true;
}

// orig: (host) see moby_joints.h: before the pose
bool lookup(uint32_t job) {
    g_pending = false;
    g_expect = false;
    if (!g_moby_pass_shadow_run || g_mode == 0) return false;
    ++g_st.asked;
    const int32_t n = lh(job + 0x10);
    if (n <= 0) {  // (no joints: the identity, nothing to keep)
        ++g_st.no_joints;
        return false;
    }
    uint32_t moby = 0;
    bool far = false;
    // the skinning of this record skipped (moby_skin_all: g_moby_skip_next non-zero for it), else it reads the pose's whole state
    if (n > 0x70 || !g_moby_mrp_skip || (g_moby_mrp_skip_parts & 1) == 0 || g_moby_mrp_skip_verify || g_moby_skip_next == nullptr ||
        !mrp_scene_pose_slot(job, moby, far) || !build_key(job, n)) {
        ++g_st.bypass;
        return false;
    }
    g_st.joints_all += static_cast<uint32_t>(n);
    g_slot = static_cast<uint64_t>(moby) << 1 | (far ? 1u : 0u);
    g_pending = true;
    const auto it = g_memo.find(g_slot);
    if (it == g_memo.end() || it->second.key.size() != g_key.size() || std::memcmp(it->second.key.data(), g_key.data(), g_key.size()) != 0) {
        ++g_st.misses;
        return false;
    }
    ++g_st.hits;
    g_st.joints_hit += static_cast<uint32_t>(n);
    const uint32_t bytes = static_cast<uint32_t>(n) * 0x40u;
    if (g_mode == 2) {  // (verify: the pose runs, store compares)
        g_expect = true;
        g_expected = it->second.joints;
        return false;
    }
    std::memcpy(ee_ram_ptr_write(SPR, bytes), it->second.joints.data(), bytes);
    g_pending = false;
    return true;
}

// orig: (host) see moby_joints.h: after the pose ran
void store(uint32_t job) {
    if (!g_pending) return;
    g_pending = false;
    const uint32_t bytes = static_cast<uint32_t>(lh(job + 0x10)) * 0x40u;
    const uint8_t* p = static_cast<const uint8_t*>(ee_ram_ptr_read(SPR));
    if (g_expect) {
        g_expect = false;
        ++g_st.verified;
        if (g_expected.size() != bytes || std::memcmp(g_expected.data(), p, bytes) != 0) {
            ++g_st.differing;
            if (g_st.differing <= 10) std::fprintf(stderr, "[pose-memo] VERIFY: job %08x (%u joints): the memo's matrices differ from the pose's\n", job, bytes / 0x40u);
        }
    }
    if (g_memo.size() > 16384) g_memo.clear();  // (bounded: the slots are moby addresses)
    Entry& e = g_memo[g_slot];
    e.key.swap(g_key);
    e.joints.assign(p, p + bytes);
}

}  // namespace

// orig: (host) see mrp_pose_memo.h
void mrp_pose_memo_install() {
    if (g_mode == 0) return;
    g_moby_pose_memo.lookup = &lookup;
    g_moby_pose_memo.store = &store;
}

// orig: (host) see mrp_pose_memo.h
void mrp_pose_memo_report() {
    if (g_st.asked == 0) return;
    std::printf("[lod] pose memo (RAC1_PERF_POSE_MEMO=%d): %llu poses asked (%llu without joints), %llu hits (%.1f %% of the memo's joints), %llu misses, %llu not memoised; verified %llu "
                "(differing %llu)\n",
                g_mode, static_cast<unsigned long long>(g_st.asked), static_cast<unsigned long long>(g_st.no_joints), static_cast<unsigned long long>(g_st.hits),
                g_st.joints_all != 0 ? 100.0 * static_cast<double>(g_st.joints_hit) / static_cast<double>(g_st.joints_all) : 0.0,
                static_cast<unsigned long long>(g_st.misses), static_cast<unsigned long long>(g_st.bypass), static_cast<unsigned long long>(g_st.verified),
                static_cast<unsigned long long>(g_st.differing));
}

}  // namespace rac1
