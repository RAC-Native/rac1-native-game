// The collision's results as native state (collision_state.h): the mode, the check mode, the import, the flush and the writers.
#include <game/collision/collision_state.h>
#include <game/bridge/guard.h>

#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace rac1 {

namespace {

CollStateMode env_mode() {  // (native unless RAC1_COLL_STATE is mirror or ee)
    const char* v = std::getenv("RAC1_COLL_STATE");
    if (v != nullptr && std::strcmp(v, "ee") == 0) return CollStateMode::Ee;
    if (v != nullptr && std::strcmp(v, "mirror") == 0) return CollStateMode::Mirror;
    return CollStateMode::Native;
}
bool env_flag(const char* name) {
    const char* v = std::getenv(name);
    return v != nullptr && std::atoi(v) != 0;
}

uint64_t g_differences = 0;
bool g_quiet = false;
uint64_t g_overflows = 0;
CollStateMode g_mode = env_mode();
const bool g_no_flush = env_flag("RAC1_COLL_STATE_NO_FLUSH");
constexpr uint64_t kReportLines = 50;  // (the differences after these are counted, not printed)

uint32_t vec_word(const Vec4& v, uint32_t lane) {
    switch (lane) {
    case 0: return f2u(v.x);
    case 1: return f2u(v.y);
    case 2: return f2u(v.z);
    default: return f2u(v.w);
    }
}
void set_vec_word(Vec4& v, uint32_t lane, uint32_t word) {
    switch (lane) {
    case 0: v.x = u2f(word); break;
    case 1: v.y = u2f(word); break;
    case 2: v.z = u2f(word); break;
    default: v.w = u2f(word); break;
    }
}
void put_vec(uint32_t a, const Vec4& v) {
    for (uint32_t k = 0; k < 4; ++k) sw(a + k * 4, vec_word(v, k));
}
Vec4 ee_vec(uint32_t a) { return Vec4{u2f(lwu(a)), u2f(lwu(a + 4)), u2f(lwu(a + 8)), u2f(lwu(a + 12))}; }
bool same_vec(const Vec4& a, const Vec4& b) {
    for (uint32_t k = 0; k < 4; ++k)
        if (vec_word(a, k) != vec_word(b, k)) return false;
    return true;
}
// the state's vector at block offset `off` (0x20, 0x30, 0x40)
Vec4& hit_vec_at(uint32_t off) {
    CollisionHit& h = g_coll_state.hit;
    return off == 0x20 ? h.point : off == 0x30 ? h.pushed : h.normal;
}

void report(const char* accessor, uint32_t ee, uint32_t native, uint32_t ee_word) {
    ++g_differences;
    if (g_quiet || g_differences > kReportLines) return;
    std::printf("[coll-state] differs: %s at 0x%08x: state 0x%08x, EE 0x%08x (difference %llu)%s\n", accessor, ee, native, ee_word,
                static_cast<unsigned long long>(g_differences), g_differences == kReportLines ? " (later ones counted only)" : "");
    std::fflush(stdout);
}

// the queries whose hit list ran past its room, printed at exit when there were any
struct OverflowReport {
    ~OverflowReport() {
        if (g_overflows == 0) return;
        std::printf("[coll-state] overflow: %llu queries listed 0x80 mobys or more (the entries past the list's room stored into the contact ring)\n",
                    static_cast<unsigned long long>(g_overflows));
        std::fflush(stdout);
    }
} g_overflow_report;

}  // namespace

CollState g_coll_state{};
bool g_coll_state_check = env_flag("RAC1_COLL_STATE_CHECK");
bool g_coll_state_native = g_mode != CollStateMode::Ee;
bool g_coll_state_project = g_mode != CollStateMode::Native || g_coll_state_check;

// orig: (host) see collision_state.h
CollStateMode coll_state_mode() { return g_mode; }
void coll_state_set_mode(CollStateMode m) {
    g_mode = m;
    g_coll_state_native = m != CollStateMode::Ee;
    g_coll_state_project = m != CollStateMode::Native || g_coll_state_check;
}
void coll_state_set_check(bool on) {
    g_coll_state_check = on;
    g_coll_state_project = g_mode != CollStateMode::Native || on;
}
uint64_t coll_state_differences() { return g_differences; }
void coll_state_set_check_quiet(bool quiet) { g_quiet = quiet; }
uint64_t coll_state_overflows() { return g_overflows; }

// orig: (host) see collision_state.h. Reads EE memory as the bridge (inside a BridgeScope: the guard counts it apart from the
// game's readers).
void coll_state_decode_ee(CollState& out) {
    const BridgeScope bridge;
    const uint32_t b = OV(0x00173e40);
    out.hit.moby = lwu(b + 0x18);
    out.hit.flags = lwu(b + 0x1c);
    out.hit.point = ee_vec(b + 0x20);
    out.hit.pushed = ee_vec(b + 0x30);
    out.hit.normal = ee_vec(b + 0x40);
    for (uint32_t k = 0; k < 3; ++k) out.hit.tri[k] = ee_vec(b + 0x50 + k * 0x10);
    const uint32_t l = OV(0x00177f00);
    for (uint32_t i = 0; i < kCollHitListWords; ++i) out.list[i] = lwu(l + i * 4);
}

// orig: (host) see collision_state.h
void coll_state_import() { coll_state_decode_ee(g_coll_state); }

// orig: (host) see collision_state.h. Writes EE memory as the bridge (inside a BridgeScope), through sw only (the bytes as the
// original's stores leave them on any host).
void coll_state_flush() {
    const BridgeScope bridge;
    const CollisionHit& h = g_coll_state.hit;
    const uint32_t b = OV(0x00173e40);
    sw(b + 0x18, h.moby);
    sw(b + 0x1c, h.flags);
    put_vec(b + 0x20, h.point);
    put_vec(b + 0x30, h.pushed);
    put_vec(b + 0x40, h.normal);
    for (uint32_t k = 0; k < 3; ++k) put_vec(b + 0x50 + k * 0x10, h.tri[k]);
    const uint32_t l = OV(0x00177f00);
    for (uint32_t i = 0; i < kCollHitListWords; ++i) sw(l + i * 4, g_coll_state.list[i]);
}

// orig: (host) see collision_state.h
void coll_state_ee_sync() {
    if (!g_coll_state_project && !g_no_flush) coll_state_flush();
}

// orig: (host) see collision_state.h
bool coll_state_same(const CollState& a, const CollState& b, const char** field) {
    const char* f = nullptr;
    if (a.hit.moby != b.hit.moby) f = "moby";
    else if (a.hit.flags != b.hit.flags) f = "flags";
    else if (!same_vec(a.hit.point, b.hit.point)) f = "point";
    else if (!same_vec(a.hit.pushed, b.hit.pushed)) f = "pushed";
    else if (!same_vec(a.hit.normal, b.hit.normal)) f = "normal";
    else if (!same_vec(a.hit.tri[0], b.hit.tri[0])) f = "tri0";
    else if (!same_vec(a.hit.tri[1], b.hit.tri[1])) f = "tri1";
    else if (!same_vec(a.hit.tri[2], b.hit.tri[2])) f = "tri2";
    else if (std::memcmp(a.list, b.list, sizeof a.list) != 0) f = "list";
    if (field != nullptr) *field = f;
    return f == nullptr;
}

// orig: (host) see collision_state.h
void coll_state_check_word(const char* accessor, uint32_t ee, uint32_t native) {
    uint32_t w;
    {
        const BridgeScope bridge;
        w = lwu(ee);
    }
    if (w != native) report(accessor, ee, native, w);
}

// orig: (host) see collision_state.h
void coll_state_check_vec(const char* accessor, uint32_t ee, const Vec4& native) {
    for (uint32_t k = 0; k < 4; ++k) {
        uint32_t w;
        {
            const BridgeScope bridge;
            w = lwu(ee + k * 4);
        }
        if (w != vec_word(native, k)) {
            report(accessor, ee + k * 4, vec_word(native, k), w);
            return;
        }
    }
}

// orig: (host) see collision_state.h. The EE stores (with the projection) are the original's (sw: the bytes as the original's
// on any host), inside a BridgeScope (the projection of a native object, for the guard).
void coll_state_put_moby(uint32_t moby) {
    g_coll_state.hit.moby = moby;
    if (!g_coll_state_project) return;
    const BridgeScope bridge;
    sw(OV(0x00173e58), moby);
}

// orig: (host) see collision_state.h
void coll_state_put_vec(uint32_t off, const Vec4& v) {
    hit_vec_at(off) = v;
    if (!g_coll_state_project) return;
    const BridgeScope bridge;
    put_vec(OV(0x00173e40) + off, v);
}

// orig: (host) see collision_state.h
void coll_state_put_lane(uint32_t off, uint32_t lane, uint32_t word) {
    set_vec_word(hit_vec_at(off), lane, word);
    if (!g_coll_state_project) return;
    const BridgeScope bridge;
    sw(OV(0x00173e40) + off + lane * 4, word);
}

// orig: (host) see collision_state.h
void coll_state_list_put(uint32_t i, uint32_t moby) {
    if (i >= kCollHitListWords) {  // (past the list's room: the contact ring's words, EE memory only, in every mode)
        if (moby == 0) ++g_overflows;  // (the terminator: one per such query)
        sw(OV(0x00177f00) + i * 4, moby);
        return;
    }
    g_coll_state.list[i] = moby;
    if (!g_coll_state_project) return;
    const BridgeScope bridge;
    sw(OV(0x00177f00) + i * 4, moby);
}

}  // namespace rac1
