// Oracle batch 126 (task 0364): the cameras that did not hand back. Run with ORACLE_LEVEL=1 (and 3) build/tests/ovl_oracle
// --batch 126 (needs the ISO); ORACLE_LEVEL=0 / 2 run the shared check on Veldin's / Aridia's copy.
// - Novalis's camera type 3 (the slide / rail path camera, row 1: init L1:0x315de0, update L1:0x315358, exit L1:0x316030,
//   transliterated: novalis_camera_tr.cpp) on Novalis's own type-3 camera: made the active camera as camera_switch does,
//   the hero put along its path (or away from it), the hero's camera request 0x1415d4, the hand-back kind (data +0x30)
//   varied (the rail pair data +0x24 is -1 on Novalis: that branch of the exit is not reached here); the update after 0..20 earlier frames.
// - the scripted camera's end (Veldin's 0x2eac18 = hydro_2eac18; Novalis's copy L1:0x317070, Aridia's L2:0x2f8c48, which
//   the levels' classes called as TODO_PORT stubs): the active camera any live camera (the scripted one, uid 5, most often).
// - Kerwan's camera types 1 and 14 (accept / init / update / exit) and type 18's accept (ORACLE_LEVEL=3; kerwan_camera_tr.cpp) on its
//   live cameras of those types, the hero in or near their boxes.

#include "ovl_oracle.h"

#include <game/ovl/hydro.h>
#include <game/ovl/kerwan_camera.h>
#include <game/ovl/novalis_camera.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l1.h>
#include <game/ovl/variants_l3.h>

#include <runtime/kernel/disc.h>

#include <cstring>

namespace rac1::oracle {

namespace {
// A check whose original is Novalis's own function at `l1addr` (not a Veldin address to translate).
void l1check(const char* name, uint32_t l1addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != 1) {
        std::printf("%-26s L1:0x%08x  skipped (Novalis only: run with ORACLE_LEVEL=1)\n", name, l1addr);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L1:0x%08x\n", name, l1addr);
    if (!check_fn(name, l1addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
// The same for level `lv`'s own function at `addr`.
void lcheck(int lv, const char* name, uint32_t addr, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    if (g_oracle_level != lv) {
        std::printf("%-26s L%d:0x%08x  skipped (level %d only: run with ORACLE_LEVEL=%d)\n", name, lv, addr, lv, lv);
        ++g_skipped;
        return;
    }
    std::printf("%-26s L%d:0x%08x\n", name, lv, addr);
    if (!check_fn(name, addr, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
}  // namespace

void batch_126() {
    if (g_only_batch != 126) return;
    g_batch = 126;
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // as in the runtime

    // 989snd's IOP server (as batch 113's)
    sif_rpc_register(0x123456, [](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        if (fno == 0x4d) {
            const uint32_t n = ee_read<uint32_t>(send);
            uint32_t p = send + 4;
            for (uint32_t i = 0; i < n; ++i) {
                const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
                if (id == 0x38) {
                    disc_read_ee(ee_read<uint32_t>(p + 0xc), ee_read<uint32_t>(p + 4), ee_read<uint32_t>(p + 8));
                    ee_write<uint32_t>(0x00137b10, 0);
                    ee_write<uint32_t>(0x00137b00, 0);
                }
                p += 4 + ((size + 3) & ~3u);
            }
            ee_write<uint32_t>(recv, 0xffffffffu);
            ee_write<uint32_t>(recv + 4 + n * 4, 0xffffffffu);
            return;
        }
        ee_write<uint32_t>(recv + 4, result(fno, send != 0 ? ee_read<uint32_t>(send) : 0u));
        ee_write<uint32_t>(recv, 0xffffffffu);
        ee_write<uint32_t>(recv + 8, 0xffffffffu);
    });
    // the level after its first gameplay frame (the port's), kept as a snapshot
    static std::vector<uint8_t> snap;
    static int snap_level = -1;
    auto level = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty() || snap_level != g_oracle_level) {
            veldin_boot();
            level_setup_body(g_oracle_level == 0 ? 1 : g_oracle_level, 0);  // (level 0: Novalis's data on Veldin's image, as batch 113)
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            snap.assign(ram, ram + 0x02000000);
            snap_level = g_oracle_level;
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    constexpr uint32_t P = 0x0013f350;
    auto live_cam = [](auto pred) -> uint32_t {
        for (uint32_t i = 0; i < 48; ++i) {
            const uint32_t c = OV(0x00167150) + i * 0xa0;
            if (ee_read<uint32_t>(OV(0x00169890) + i * 4) != 0 && pred(c)) return c;
        }
        return 0;
    };

    // ---- Novalis's camera type 3
    {
        // the type-3 camera made the active one (as camera_switch: the work block 0x169110 at +0x70), the hero along its path
        // (or near its own place), the request / rail pair / hand-back kind varied; `steps` earlier inits + updates (the port's)
        auto cam3 = [level, live_cam](int steps) -> uint32_t {
            level();
            const uint32_t cam = live_cam([](uint32_t c) { return ee_read<int16_t>(c + 0x86) == 3; });
            if (cam == 0) return 0;
            const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
            const uint32_t d = ee_read<uint32_t>(e + 0x1c);
            w32(OV(0x00166e00), cam);
            w32(cam + 0x70, OV(0x00169110));
            w16(cam + 0x7e, 0);
            const int32_t pi = ee_read<int32_t>(d + 0x20);
            if (pi >= 0 && ri(0, 4) != 0) {
                const uint32_t path = ee_read<uint32_t>(L1_ADDR(0x001b0930u) + u(pi) * 4);
                const int32_t np = ee_read<int32_t>(path);
                if (np > 0) {
                    Vec4 q = ee_ref<Vec4>(path + 0x10 + u(ri(0, np - 1)) * 0x10);
                    q.x += rf(-2, 2); q.y += rf(-2, 2); q.z += rf(-0.5f, 2);
                    ee_ref<Vec4>(P + 0x80) = q;
                }
            } else {
                Vec4 q = ee_ref<Vec4>(P + 0x80);
                q.x += rf(-20, 20); q.y += rf(-20, 20); q.z += rf(-2, 5);
                ee_ref<Vec4>(P + 0x80) = q;
            }
            w32(P + 0x2284, ri(0, 3) != 0 ? 3u : u(ri(0, 0x50)));
            // (the rail pair data +0x24 stays the level's: -1 on Novalis, whose rail table *(0x15f70c) is not set up)
            // (data +0x30: 3 on Novalis; 2 would take the init through its paths +0x28 / +0x2c, -1 on Novalis: not a state of this level)
            if (ri(0, 3) == 0) w32(d + 0x30, coin() ? 3u : (coin() ? 0u : u(ri(4, 6))));
            if (ee_read<int32_t>(d + 0x24) >= 0) {
                const uint32_t t = ee_read<uint32_t>(OV(0x0015f70c)) + u(ee_read<int32_t>(d + 0x24)) * 0x20;
                w32(P + 0x560, coin() ? ee_read<uint32_t>(t + 0x10) : R());
                w32(P + 0x570, coin() ? 0u : 1u);
            }
            for (int s = 0; s < steps; ++s) {
                if (s == 0) cam_type3_init(cam);
                else cam_type3_update(cam);
                Vec4 q = ee_ref<Vec4>(P + 0x80);  // the hero moving on
                q.x += rf(-0.6f, 0.6f); q.y += rf(-0.6f, 0.6f); q.z += rf(-0.3f, 0.1f);
                ee_ref<Vec4>(P + 0x80) = q;
            }
            return cam;
        };
        if (g_oracle_level == 1) {
            level();
            if (live_cam([](uint32_t c) { return ee_read<int16_t>(c + 0x86) == 3; }) == 0) std::printf("cam_type3                  no live camera of type 3 on this level\n");
        }
        l1check("cam_type3_init", 0x00315de0, 150, Ret::None, [cam3] { return Args{{cam3(0)}, {}}; },
                [](A a) { if (a.i[0] != 0) cam_type3_init(a.i[0]); return uint64_t{0}; });
        l1check("cam_type3_update", 0x00315358, 300, Ret::None, [cam3] { return Args{{cam3(ri(1, 20))}, {}}; },  // (an update always follows the init)
                [](A a) { if (a.i[0] != 0) cam_type3_update(a.i[0]); return uint64_t{0}; });
        l1check("cam_type3_exit", 0x00316030, 200, Ret::None, [cam3] { return Args{{cam3(ri(1, 6))}, {}}; },
                [](A a) { if (a.i[0] != 0) cam_type3_exit(a.i[0]); return uint64_t{0}; });
    }

    // ---- Kerwan's camera types 1 and 14 (accept / init / update / exit) and type 18's accept (kerwan_camera_tr.cpp)
    {
        // a live camera of the type (as batch 119's): the hero in or near its box, priorities / trigger / the current camera's
        // request varied; for init / update / exit made the active camera (the work block 0x169110), after 0..10 earlier frames
        auto cam_setup = [level, live_cam](int type, uint32_t* other, int steps, void (*init)(uint32_t), void (*update)(uint32_t)) -> uint32_t {
            level();
            const uint32_t cam = live_cam([type](uint32_t c) { return ee_read<int16_t>(c + 0x86) == type; });
            if (cam == 0) return 0;
            const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + u(ee_read<int16_t>(cam + 0x84) * 0x20);
            const uint32_t d = ee_read<uint32_t>(e + 0x1c);
            Vec4 at = ee_ref<Vec4>(P + 0x80);
            if (ri(0, 3) != 0 && d != 0) {
                const int32_t idx = ee_read<int32_t>(d + 0xc);
                if (idx >= 0) at = ee_ref<Vec4>(ee_read<uint32_t>(OV(0x001600ec)) + u(idx) * 0x80 + 0x30);
            } else if (ri(0, 2) == 0) {
                at = ee_ref<Vec4>(e);
            }
            at.x += rf(-3, 3); at.y += rf(-3, 3); at.z += rf(-1, 1);
            ee_ref<Vec4>(P + 0x80) = at;
            if (ri(0, 2) == 0) ee_ref<Vec4>(OV(0x00166dc0)) = at;
            if (ri(0, 3) == 0) w8(cam + 0x7c, u(ri(0, 7)));
            if (other != nullptr) {
                *other = ri(0, 3) == 0 ? 0u : ee_read<uint32_t>(OV(0x00166e00));
                if (*other != 0) { w16(*other + 0x7e, u(ri(0, 2) == 0 ? ri(1, 5) : 0)); w8(*other + 0x7c, u(ri(0, 7))); }
                return cam;
            }
            w32(OV(0x00166e00), cam);
            w32(cam + 0x70, OV(0x00169110));
            w16(cam + 0x7e, 0);
            for (int s = 0; s < steps; ++s) {
                if (s == 0) init(cam);
                else update(cam);
                Vec4 q = ee_ref<Vec4>(P + 0x80);
                q.x += rf(-0.5f, 0.5f); q.y += rf(-0.5f, 0.5f); q.z += rf(-0.2f, 0.2f);
                ee_ref<Vec4>(P + 0x80) = q;
            }
            return cam;
        };
        using Acc = int32_t (*)(uint32_t, uint32_t);
        struct CamT { int type; uint32_t accept, init, update, exit; Acc acc; void (*fi)(uint32_t); void (*fu)(uint32_t); void (*fe)(uint32_t); };
        static const CamT cams[] = {
            {1, 0x002e8870, 0x002e8940, 0x002e8960, 0x002e8980, cam_type1_accept, cam_type1_init, cam_type1_update, cam_type1_exit},
            {14, 0x002ebd70, 0x002eb978, 0x002ebdb0, 0x002ebde8, ker_cam_type14_accept_l3, ker_cam_type14_init_l3, ker_cam_type14_update_l3, ker_cam_type14_exit_l3},
            {18, 0x002ed5f8, 0, 0, 0, ker_cam_type18_accept_l3, nullptr, nullptr, nullptr},
        };
        static char nm[16][48];
        int k = 0;
        for (const CamT& ct : cams) {
            auto name = [&](const char* what) { char* p = nm[k++ & 15]; std::snprintf(p, 48, "ker_cam_type%d_%s", ct.type, what); return static_cast<const char*>(p); };
            const CamT* c = &ct;
            lcheck(3, name("accept"), ct.accept, 300, Ret::Int32, [cam_setup, c] { uint32_t other = 0; const uint32_t cam = cam_setup(c->type, &other, 0, nullptr, nullptr); return Args{{cam, other}, {}}; },
                   [c](A a) { return r32(a.i[0] != 0 ? static_cast<uint32_t>(c->acc(a.i[0], a.i[1])) : 0u); });
            if (ct.init == 0) continue;
            lcheck(3, name("init"), ct.init, 100, Ret::None, [cam_setup, c] { return Args{{cam_setup(c->type, nullptr, 0, c->fi, c->fu)}, {}}; },
                   [c](A a) { if (a.i[0] != 0) c->fi(a.i[0]); return uint64_t{0}; });
            lcheck(3, name("update"), ct.update, 200, Ret::None, [cam_setup, c] { return Args{{cam_setup(c->type, nullptr, ri(1, 10), c->fi, c->fu)}, {}}; },
                   [c](A a) { if (a.i[0] != 0) c->fu(a.i[0]); return uint64_t{0}; });
            lcheck(3, name("exit"), ct.exit, 200, Ret::None, [cam_setup, c] { return Args{{cam_setup(c->type, nullptr, ri(1, 6), c->fi, c->fu)}, {}}; },
                   [c](A a) { if (a.i[0] != 0) c->fe(a.i[0]); return uint64_t{0}; });
        }
    }

    // ---- Kerwan's follow-camera hook L3:0x2eb408 (the look height from the float L3:0x16710c, then the callback L3:0x2eb468
    // queued) and that callback (the block L3:0x167100: +4 a flag, +8 set to 10000, +0xc counted down by the frame step)
    {
        auto prep = [level] {
            level();
            const uint32_t b = L3_ADDR(0x00167100u);
            wf(b + 0xc, ri(0, 3) == 0 ? 0.0f : rf(-1, 3));
            w32(b + 4, ri(0, 2) == 0 ? 0u : 1u);
            w32(b, R() & 1);
            if (ri(0, 2) == 0) w32(OV(0x0015ef4c), u(ri(0, 3)));  // (the callback queue's count)
        };
        lcheck(3, "ker_cam0_hook", 0x002eb408, 200, Ret::None, [prep] { prep(); return Args{{0u}, {}}; },
               [](A) { cam0_level_hook(0); return uint64_t{0}; });
        lcheck(3, "ker_cam_callback", 0x002eb468, 200, Ret::None, [prep] { prep(); return Args{{0u}, {}}; },
               [](A) { cam0_level_callback(0); return uint64_t{0}; });
    }

    // ---- the scripted camera's end (Veldin's 0x2eac18; the level's own copy on another level)
    check("scripted_camera_end", 0x002eac18, 200, Ret::None, [level, live_cam] {
        level();
        uint32_t cam = 0;
        if (ri(0, 3) != 0) cam = live_cam([](uint32_t c) { return ee_read<int16_t>(c + 0x86) == 5; });
        if (cam == 0) {
            const int k = ri(0, 6);
            int seen = 0;
            cam = live_cam([&seen, k](uint32_t) { return seen++ == k; });
        }
        if (cam != 0) {
            w32(OV(0x00166e00), cam);
            w32(cam + 0x70, OV(0x00169110));
            w16(cam + 0x7e, u(ri(0, 5)));
            w8(cam + 0x7d, R() & 1);
            wfrand(cam + 0x30, 3, 50, 300);
            wfrand(cam + 0x40, 3, -3, 3);
        }
        Vec4 q = ee_ref<Vec4>(P + 0x80);
        q.x += rf(-5, 5); q.y += rf(-5, 5); q.z += rf(-1, 3);
        ee_ref<Vec4>(P + 0x80) = q;
        wfrand(P + 0x90, 3, -3.1f, 3.1f);  // (the hero's Euler angles: the view put behind him)
        return Args{{ri(0, 3) == 0 ? 0u : (coin() ? 2u : u(ri(0, 4)))}, {}};
    }, [](A a) { hydro_2eac18(a.i[0]); return uint64_t{0}; });

    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
    g_extra_noop_hooks.clear();
}

}  // namespace rac1::oracle
