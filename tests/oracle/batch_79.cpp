// Oracle batch 79: the moby pass (task 0294) on Veldin's real level data (needs the ISO): the
// draw loop 0x2559c0.

#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
#include <game/ovl/moby_draw.h>
#include <game/ovl/moby_joints.h>
#include <game/ovl/moby_skin.h>
#include <game/ovl/moby_vu0.h>
#include <game/ovl/moby_vu1.h>
#include <runtime/vu/vu1.h>
#include <game/ovl/moby_shadows.h>
#include <runtime/vu/vu0.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/pause_menu.h>
#include <game/ovl/vu0_macro.h>
#include <game/math/vecmath.h>
#include <runtime/ee/ee_interpreter.h>

#include <algorithm>
#include <cmath>
#include <cstring>
#include <filesystem>
#include <memory>
#include <string>
#include <vector>

namespace rac1::oracle {

void batch_79() {
    if (g_only_batch != 79) return;
    g_batch = 79;
    g_ignored_words = vcross_lanes();
    // the draw loop's register save area on the scratchpad (the caller's registers); 0x2531e0's
    // spill of the caller's t8 / t9 (the drop shadows' joints)
    for (uint32_t a = 0x70003fa8; a < 0x70004000; a += 4) g_ignored_words.push_back(a);
    for (uint32_t a = 0x70003700; a < 0x70003720; a += 4) g_ignored_words.push_back(a);
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;
    static std::vector<uint8_t> snap;
    auto level1 = [] {
        uint8_t* ram = ee_get_ram_base();
        if (snap.empty()) {
            veldin_boot();
            particle_table_init();
            level_setup_body(1, 0);
            w32(OV(0x0016c060), 0xf);
            gameplay_frame();
            w16(0x001518d0, 512);
            w16(0x001518d2, 416);
            screen_setup();
            projection_setup();
            render_view_setup();
            snap.assign(ram, ram + 0x02000000);
        } else {
            std::memcpy(ram, snap.data(), snap.size());
        }
    };
    // The level's mobys, some moved in front of the camera (near, far, beside, behind), their
    // flags, fades, shadows, LOD distances and light sets varied; the light block random.
    auto mobys = [level1] {
        level1();
        w32(OV(0x0015f5f8), 0x01e80000u);  // (the skinning ring the checks hand the loop starts here, above the packet buffer at 0x01e10000 as in the game: the port bounds both, packet_rings.h)
        const float cx = ee_read<float>(OV(0x00166dc0)), cy = ee_read<float>(OV(0x00166dc4)), cz = ee_read<float>(OV(0x00166dc8));
        float fw[3], rt[3], up[3];
        for (int k = 0; k < 3; ++k) {
            rt[k] = ee_read<float>(OV(0x00166c80) + 0x10 * u(k));
            up[k] = ee_read<float>(OV(0x00166c84) + 0x10 * u(k));
            fw[k] = ee_read<float>(OV(0x00166c88) + 0x10 * u(k));
        }
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        uint32_t n = 0;
        while (n < 2000 && ee_read<uint8_t>(first + n * 0x100 + 0x20) != 0xff) ++n;
        for (uint32_t i = 0; i < n; ++i) {
            const uint32_t m = first + i * 0x100;
            const uint32_t cls = ee_read<uint32_t>(m + 0x24);
            if (ee_read<int8_t>(m + 0x20) < 0 || cls == 0) continue;
            // (a class without packets for a level of detail / a shadow mesh never gets drawn at
            // it: the original's packet loops would not end)
            if (ee_read<uint8_t>(cls + 4) == 0 || ee_read<uint8_t>(cls + 5) == 0) continue;
            if (ri(0, 2) != 0) {
                const int where = ri(0, 9);
                const float d = where == 0 ? rf(-30, 0) : where == 1 ? rf(0, 3) : where == 2 ? rf(100, 400) : rf(3, 80);
                const float side = where == 3 ? rf(20, 120) : rf(0.0f, 0.6f) * std::fabs(d);
                const float x = rf(-side, side), y = rf(-side, side);
                for (uint32_t k = 0; k < 3; ++k) wf(m + 4 * k, (k == 0 ? cx : k == 1 ? cy : cz) + fw[k] * d + rt[k] * x + up[k] * y);
            }
            uint32_t fl = ee_read<uint16_t>(m + 0x34) & ~0x0e19u;
            if (ri(0, 4) == 0) fl |= 0x10;
            // (a drop shadow only on classes with one: a shadow point list and joints)
            const bool can_shadow = ee_read<uint8_t>(cls + 0xf) != 0 && ee_read<uint8_t>(cls + 8) != 0 && ee_read<uint32_t>(cls + 0x14) != 0;
            if (can_shadow && ri(0, 1) == 0) fl |= ri(0, 3) == 0 ? 0x800 : (coin() ? 0x400 : 0xc00);
            if (ri(0, 4) == 0) fl |= coin() ? 0x200 : 0x8;
            if (ri(0, 20) == 0) fl |= 1;
            w16(m + 0x34, fl);
            w8(m + 0x31, u(ri(0, 1)));
            if (coin()) w16(m + 0x32, u(ri(1, 200)));
            w8(m + 0x72, u(ri(0, 60)));
            w8(m + 0x73, ri(0, 2) == 0 && ee_read<uint8_t>(cls + 6) != 0 ? u(ri(1, 80)) : 0u);
            w8(m + 0x7f, u(ri(0, 80)));
            w8(m + 0x23, u(ri(0, 255)));
            w8(m + 0xbd, u(ri(0, 3)));
            wf(m + 0x84, rf(-10, 40));
            wf(m + 0x88, ri(0, 4) == 0 ? rf(-5, 0) : rf(0, 60));
            if (coin()) wf(m + 0x2c, rf(0.2f, 3.0f));
            w8(m + 0x38, u(ri(0, 15)));
            w8(m + 0x39, u(ri(0, 15)));
            w8(m + 0x3a, coin() ? 0u : u(ri(0, 255)));
            if (coin()) w32(m + 0x3c, R());
            // the animation: a second key (+0x54 the blend factor), keys lerped only (+0x50 frame
            // bytes), blend channels (+0x60) and additive layers (+0x64) over the joints
            const uint32_t nj = std::min(ee_read<uint8_t>(cls + 8), ee_read<uint8_t>(cls + 9));
            if (nj != 0) {
                if (ee_read<uint32_t>(m + 0x6c) != 0 && coin()) wf(m + 0x54, rf(0.01f, 0.99f));
                if (coin()) {
                    const uint32_t b = u(ri(0, 200));
                    const uint32_t f = u(ri(0, 200));
                    w32(m + 0x50, b | ((b + 1) << 8) | (f << 16) | (f << 24));
                }
                const uint32_t area = 0x01f00000u + (i & 0x1ff) * 0x400;
                auto joint = [nj] { return 0x70000000u + u(ri(0, static_cast<int32_t>(nj) - 1)) * 0x40; };
                auto quat = [](uint32_t a) {
                    float q[4], l = 0;
                    for (float& x : q) { x = rf(-1, 1); l += x * x; }
                    l = std::sqrt(l) + 1e-3f;
                    for (uint32_t k = 0; k < 4; ++k) wf(a + 4 * k, q[k] / l);
                };
                w32(m + 0x60, 0);
                w32(m + 0x64, 0);
                if (ri(0, 2) == 0) {  // channels
                    uint32_t prev = m + 0x60 - 0x1c;
                    for (uint32_t c = 0, nc = u(ri(1, 2)); c < nc; ++c) {
                        const uint32_t node = area + c * 0x100, data = node + 0x20;
                        const uint32_t cnt = u(ri(1, 4));
                        w32(prev + 0x1c, node);
                        w16(node + 2, cnt);
                        wf(node + 8, rf(0, 1));
                        w32(node + 0x10, data);
                        w32(node + 0x1c, 0);
                        for (uint32_t e = 0; e < cnt; ++e) {
                            quat(data + e * 0x30);
                            wfrand(data + e * 0x30 + 0x10, 4, 0.5f, 1.5f);
                            wfrand(data + e * 0x30 + 0x20, 3, -2, 2);
                            w32(data + e * 0x30 + 0x2c, joint());
                        }
                        prev = node;
                    }
                }
                if (ri(0, 2) == 0) {  // layers
                    uint32_t prev = m + 0x64 - 8;
                    for (uint32_t c = 0, nc = u(ri(1, 3)); c < nc; ++c) {
                        const uint32_t node = area + 0x200 + c * 0x40;
                        w32(prev + 8, node);
                        w8(node + 3, coin() ? 0u : 1u);
                        w32(node + 4, joint());
                        w32(node + 8, 0);
                        wf(node + 0xc, rf(0, 1));
                        quat(node + 0x10);
                        wfrand(node + 0x20, 4, 0.5f, 1.5f);
                        wfrand(node + 0x30, 4, -2, 2);
                        prev = node;
                    }
                }
            }
        }
        // the light block: 16 directional sets, 8 point lights (some unset)
        for (uint32_t s = 0; s < 16; ++s) {
            const uint32_t a = OV(0x0017fec0) + s * 0x40;
            wfrand(a, 16, -1.0f, 1.0f);
            wfrand(a + 0x10, 3, -1.0f, 1.0f);
            wfrand(a + 0x30, 3, -1.0f, 1.0f);
        }
        for (uint32_t p = 0; p < 8; ++p) {
            const uint32_t a = OV(0x0017fec0) + 0x400 + p * 0x20;
            wfrand(a, 4, 0.0f, 1.0f);
            if (ri(0, 2) == 0) {
                w32(a + 0x10, 0);
                w32(a + 0x14, 0);
            } else {
                const float d = rf(0, 60);
                for (uint32_t k = 0; k < 3; ++k) wf(a + 0x10 + 4 * k, (k == 0 ? cx : k == 1 ? cy : cz) + fw[k] * d + rf(-15, 15));
                wf(a + 0x1c, rf(0.5f, 30.0f));
            }
        }
        const bool all_seen = ri(0, 3) != 0;  // the visibility masks: mostly every group seen
        for (uint32_t i = 0; i < 0x80; ++i) w8(OV(0x00173d00) + i, all_seen ? (ri(0, 20) == 0 ? R() : 0xffu) : R());
        w32(OV(0x001611c0), 0x01e10000u + 0x10 * u(ri(0, 0x100)));
        w32(OV(0x001611c8), ri(0, 5) == 0 ? 0x01e10000u + u(ri(0, 0x4000)) : 0x01f00000u);
    };
    static vu0m::Regs vu0_in;  // the VU0 registers the original started from
    g_pre_call = [](EeInterpreter& ee) {
        for (int i = 0; i < 32; ++i)
            for (int k = 0; k < 4; ++k) vu0_in.vf[i].v[k] = ee.vu0().vf[static_cast<size_t>(i)].f[k];
        for (int k = 0; k < 4; ++k) vu0_in.acc.v[k] = ee.vu0().acc.f[k];
        vu0_in.q = ee.vu0().q;
    };
    if (std::getenv("ORACLE_MOBY_STATS") != nullptr) {  // coverage: records and chain bytes the cases write
        for (int c = 0; c < 20; ++c) {
            mobys();
            const uint32_t ring = 0x01e80000u, sp0 = ee_read<uint32_t>(OV(0x001611c0));
            const uint32_t end = moby_draw_loop(ee_read<uint32_t>(OV(0x0015ffd8)), ring, -1, c & 1);
            std::printf("case %d: ring %u bytes, chain %u bytes, sub-meshes %u, deferred %u\n", c, end - ring, ee_read<uint32_t>(OV(0x001611c0)) - sp0,
                        ee_read<uint32_t>(OV(0x0015fff8)), ee_read<uint32_t>(OV(0x0015fffc)));
        }
    }
    check("moby_draw_loop", 0x002559c0, 150, Ret::Int32, [mobys] {
        mobys();
        return Args{{ee_read<uint32_t>(OV(0x0015ffd8)), 0x01e80000u + 0x10 * u(ri(0, 0x100)), 0xffffffffu, u(ri(0, 1))}, {}};
    }, [](A a) {
        vu0m::regs() = vu0_in;
        return static_cast<uint64_t>(moby_draw_loop(a.i[0], a.i[1], static_cast<int32_t>(a.i[2]), a.i[3]));
    });
    // The rest of the pass on the state the (proved) draw loop leaves.
    auto drawn = [mobys] {
        mobys();
        spr_moby_fill();
        vu0m::regs() = vu0_in;
        const uint32_t r = moby_draw_loop(ee_read<uint32_t>(OV(0x0015ffd8)), 0x01e80000u, -1, u(ri(0, 1)));
        w32(OV(0x0015ffd4), r);
    };
    check("moby_textures", 0x002555c0, 150, Ret::Int32, [drawn] {
        drawn();
        if (coin()) wfrand(OV(0x0019a280), 0x40, 0.0f, 0.0f);
        for (uint32_t i = 0; i < 0x40; ++i)
            if (ri(0, 3) == 0) w32(OV(0x0019a280) + 4 * i, u(ri(0, 0x30000)));
        return Args{{ri(0, 4) == 0 ? u(ri(0x3e0000, 0x3ff000)) : u(ri(0x40000, 0x300000)) & ~0xffu}, {}};
    }, [](A a) { return static_cast<uint64_t>(moby_textures(a.i[0])); });
    check("moby_textures_chain", 0x0024f9c8, 100, Ret::None, [drawn] {
        drawn();
        w32(OV(0x0015ffcc), 0x01e0fff0u);
        w32(OV(0x0016a018), coin() ? 1u : 0u);
        w32(OV(0x0016a014), coin() ? 1u : 0u);
        w32(0x0015ee74, u(ri(0x40000, 0x300000)) & ~0xffu);
        return Args{};
    }, [](A) { moby_textures_chain(); return uint64_t{0}; });
    check("moby_submesh_fill", 0x00255870, 100, Ret::None, [drawn] {
        do drawn();
        while (ee_read<uint32_t>(OV(0x0015fff8)) == 0);
        return Args{};
    }, [](A) { moby_submesh_fill(); return uint64_t{0}; });
    check("gs_ram_upload", 0x00240498, 100, Ret::None, [] {
        // the level's texture blocks: PSMT8 images (w, h powers of two, some tiny), the two
        // palette formats, an unknown format (the previous packet sent again)
        const uint32_t list = 0x01c00000u, data = 0x01c20000u;
        const int n = ri(0, 6);
        for (int k = 0; k < n; ++k) {
            const int pick = ri(0, 9);
            const uint32_t fmt = pick < 6 ? 0x13u : pick < 8 ? (pick == 6 ? 2u : 0u) : 7u;
            const uint32_t w = 1u << ri(2, 7), h = 1u << ri(2, 7);
            w32(list + 0x10u * u(k), fmt);
            w32(list + 0x10u * u(k) + 4, w | (h << 16));
            w32(list + 0x10u * u(k) + 0xc, 0x100u * u(ri(0, 0x40)));
        }
        w32(0x0015ee8c, NOT_ADDR(0x00280000u) + 0x100u * u(ri(0, 0x100)));
        return Args{{data, u(n), list}, {}};
    }, [](A a) { gs_ram_upload(static_cast<uint32_t>(a.i[0]), static_cast<int32_t>(a.i[1]), static_cast<uint32_t>(a.i[2])); return uint64_t{0}; });
    check("moby_texture_slots", 0x0024fad0, 200, Ret::None, [] {
        // the drawn groups (-1 ended), their classes' texture entries (ids ended by 0xff, the
        // packet, sign bit on the last), the slots (some 0) and the packets
        uint32_t list = OV(0x0019a600);
        const int groups = ri(0, 5);
        for (int g = 0; g < groups; ++g) {
            const uint32_t cls = u(ri(0, 15));
            w32(list, cls);
            list += 4;
            const uint32_t cs = 0x01c00000u + cls * 0x100u, ents = 0x01c10000u + cls * 0x400u;
            w32(OV(0x00197300) + cls * 4, cs);
            w32(cs + 0x20, ents);
            const int n = ri(1, 3);
            for (int e = 0; e < n; ++e) {
                const uint32_t en = ents + 0x10u * u(e);
                const int ids = ri(0, 4);
                for (int k = 0; k < ids; ++k) w8(en + u(k), u(ri(0, 0x3f)));
                w8(en + u(ids), 0xff);
                const uint32_t pk = 0x01c80000u + cls * 0x1000u + u(e) * 0x400u;
                w32(en + 0xc, pk | (e == n - 1 ? 0x80000000u : 0u));
                for (uint32_t q = 0; q < 0x140; q += 4) w32(pk + q, R());
            }
        }
        w32(list, ri(0, 1) == 0 ? 0xffffffffu : 0x80000000u + u(ri(0, 9)));
        for (uint32_t k = 0; k < 0x40; ++k) {
            w16(OV(0x00199e80) + k * 4, ri(0, 2) == 0 ? 0u : u(ri(0x2800, 0x3fff)));
            w16(OV(0x00199e82) + k * 4, ri(0, 2) == 0 ? 0u : (ri(0, 9) == 0 ? 0x8000u + u(ri(0, 0x7fff)) : u(ri(0x2800, 0x3fff))));
        }
        return Args{};
    }, [](A) { moby_texture_slots(); return uint64_t{0}; });
    check("moby_pass_start", 0x0024fe50, 50, Ret::None, [mobys] {
        mobys();
        w32(OV(0x0015f5f8), 0x01e80000u + 0x10 * u(ri(0, 0x100)));
        w32(OV(0x0015f5fc), 0x01e80000u);
        return Args{};
    }, [](A) { moby_pass_start(); return uint64_t{0}; });
    {
        const std::vector<uint32_t> hooks = g_extra_noop_hooks;
        g_extra_noop_hooks = {};
        const std::vector<uint32_t> ign = g_ignored_words;
        for (uint32_t a = 0x70001ff0; a < 0x70002000; a += 4) g_ignored_words.push_back(a);  // 0x1ee858's saved return address
        for (uint32_t a = OV(0x0019b780); a < OV(0x0019b7e0); a += 4) g_ignored_words.push_back(a);  // 0x2558e0's saved registers
        g_ignored_words.push_back(OV(0x0016044c));  // 0x1ff3c0's w lane (the drop shadows): a stale VU0 lane
        const auto pre = g_pre_call;
        g_pre_call = [](EeInterpreter& ee) {  // the whole VU0 as the original found it; its programs beside the EE
            vu0_in.vf = ee.vu0().vf;
            vu0_in.vi = ee.vu0().vi;
            vu0_in.acc = ee.vu0().acc;
            vu0_in.q = ee.vu0().q;
            vu0_in.i = ee.vu0().i;
            vu0_in.mem = ee.vu0().mem;
            ee.set_vu0_coexec(true, 1, 1);
        };
        if (std::getenv("ORACLE_SHADOW_STATS") != nullptr) {
            int with = 0;
            for (int c = 0; c < 40; ++c) {
                mobys();
                w32(OV(0x0015f5f8), 0x01e80000u);
                w32(OV(0x0015f5fc), 0x01f80000u);
                w32(OV(0x0016a018), 1);
                vu0::vif0_send_chain(0x0010e4c0, true);
                moby_pass();
                if (ee_read<uint32_t>(OV(0x0015fffc)) != 0) ++with;
                std::printf("case %d: deferred chain %08x, shadowed %u\n", c, ee_read<uint32_t>(OV(0x00160000)), ee_read<uint32_t>(OV(0x0015fffc)));
            }
            std::printf("cases with shadows: %d / 40\n", with);
        }
        check("moby_pass", 0x00250038, 100, Ret::None, [mobys] {
            mobys();
            vu0::vif0_send_chain(0x0010e4c0, true);
            w32(OV(0x0015f5f8), 0x01e80000u + 0x10 * u(ri(0, 0x100)));
            // (the packet buffer's end, the skinning's work buffer above it: apart from the chain,
            // as in the game -- a full buffer is the draw loop check's)
            w32(OV(0x0015f5fc), 0x01f80000u);
            w32(OV(0x0016a018), ri(0, 5) == 0 ? 0u : 1u);
            w32(OV(0x0016a014), coin() ? 1u : 0u);
            return Args{};
        }, [](A) {
            vu0m::regs() = vu0_in;
            moby_pass();
            return uint64_t{0};
        });
        g_extra_noop_hooks = hooks;
        g_ignored_words = ign;
        g_pre_call = pre;
    }
    // The joints of one of the ring's records (the k-th: header 0xc0, its 8-byte packet entries
    // ended by a zero doubleword; a shadow mesh's (+0x14) 0x40 bytes of matrices and entries after)
    auto record = [](uint32_t ring, int k) {
        uint32_t p = ring;
        for (; k > 0 && ee_read<uint32_t>(p) != 0; --k) {
            uint32_t e = p + 0xc0;
            while (ee_read<uint64_t>(e) != 0) e += 8;
            if (ee_read<uint16_t>(p + 0x14) != 0) {
                e = ((e + 0x10) & ~0xfu) + 0x40;
                while (ee_read<uint64_t>(e) != 0) e += 8;
            }
            p += (e + 0x10 - p) & ~0xfu;
        }
        return p;
    };
    if (std::getenv("ORACLE_JOINTS_STATS") != nullptr) {
        for (int c = 0; c < 10; ++c) {
            drawn();
            int recs = 0, jointed = 0, keys2 = 0, chans = 0, layers = 0;
            for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && recs < 500; p = record(p, 1), ++recs) {
                if (ee_read<int16_t>(p + 0x10) != 0) ++jointed;
                if (ee_read<uint32_t>(p + 4) != 0) ++keys2;
                if (ee_read<uint32_t>(p + 8) != 0) ++chans;
                if (ee_read<uint32_t>(p + 0xc) != 0) ++layers;
            }
            std::printf("case %d: records %d, with joints %d, second keys %d, channels %d, layers %d\n", c, recs, jointed, keys2, chans, layers);
        }
    }
    {
        uint32_t job = 0;
        check("moby_joints_pose", 0x00250fe0, 150, Ret::None, [drawn, record, &job] {
            int recs = 0;
            do {
                drawn();
                recs = 0;
                for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && recs < 500; p = record(p, 1)) ++recs;
            } while (recs == 0);
            // a record of a kind drawn at random when the frame has one: a second key, a second
            // key lerped only (0x400), blend channels, additive layers
            const int kind = ri(0, 4);
            std::vector<uint32_t> fit;
            for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && fit.size() < 500; p = record(p, 1)) {
                const bool k2 = ee_read<uint32_t>(p + 4) != 0;
                if ((kind == 1 && k2) || (kind == 2 && k2 && (ee_read<uint16_t>(p + 0x12) & 0x400) != 0) ||
                    (kind == 3 && ee_read<uint32_t>(p + 8) != 0) || (kind == 4 && ee_read<uint32_t>(p + 0xc) != 0))
                    fit.push_back(p);
            }
            job = !fit.empty() ? fit[u(ri(0, static_cast<int32_t>(fit.size()) - 1))] : record(0x01e80000u, ri(0, recs - 1));
            return Args{{job, 0x01f80000u}, {}};
        }, [](A a) {
            vu0m::regs() = vu0_in;
            moby_joints_pose(a.i[0], a.i[1]);
            return uint64_t{0};
        });
    }
    {
        // the skinning after its record's pose, the VU0 programs uploaded (both sides), the VU0
        // state the same on both sides; the original beside its VU0 program at pace 1 / 1
        const std::vector<uint32_t> ign = g_ignored_words;
        for (uint32_t a = 0x70001ff0; a < 0x70002000; a += 4) g_ignored_words.push_back(a);  // its saved return address
        uint32_t job = 0;
        check("moby_skin", 0x001ee858, 100, Ret::Int32, [drawn, record, &job] {
            int recs = 0;
            do {
                drawn();
                recs = 0;
                for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && recs < 500; p = record(p, 1)) ++recs;
            } while (recs == 0);
            job = record(0x01e80000u, ri(0, recs - 1));
            ee_vif0_chain(0x00100080, true);
            vu0::vif0_send_chain(0x00100080, true);
            mem_copy128(0x70003800, OV(0x00166080), 0x800);
            vu0m::regs() = vu0_in;
            vu0m::regs().vf[0].v[3] = 1.0f;
            moby_joints_pose(job, 0x01f80000u);
            g_pre_call = [](EeInterpreter& ee) {
                const VUState& p = vu0::global().st;
                ee.vu0().vf = p.vf;
                ee.vu0().vi = p.vi;
                ee.vu0().acc = p.acc;
                ee.vu0().q = p.q;
                ee.vu0().i = p.i;
                ee.vu0().mem = p.mem;
                ee.set_vu0_coexec(true, 1, 1);
            };
            return Args{{job, 0x01f80000u}, {}};
        }, [](A a) { return static_cast<uint64_t>(moby_skin(a.i[0], a.i[1])); });
        g_ignored_words = ign;
        g_pre_call = nullptr;
    }
    {
        // all the ring's records: their poses and skinning in turn (0x2558e0)
        const std::vector<uint32_t> ign = g_ignored_words;
        for (uint32_t a = 0x70001ff0; a < 0x70002000; a += 4) g_ignored_words.push_back(a);
        for (uint32_t a = OV(0x0019b780); a < OV(0x0019b7e0); a += 4) g_ignored_words.push_back(a);
        check("moby_skin_all", 0x002558e0, 60, Ret::None, [drawn, record] {
            drawn();
            ee_vif0_chain(0x00100080, true);
            vu0::vif0_send_chain(0x00100080, true);
            mem_copy128(0x70003800, OV(0x00166080), 0x800);
            vu0m::regs() = vu0_in;
            vu0m::regs().vf[0].v[3] = 1.0f;
            if (std::getenv("ORACLE_SKIN_LIST") != nullptr) {
                int k = 0;
                for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && k < 500; p = record(p, 1), ++k)
                    std::printf("    rec %d %08x: +0 %08x +4 %08x +8 %08x +c %08x +10 %08x +14 %08x\n", k, p, ee_read<uint32_t>(p), ee_read<uint32_t>(p + 4),
                                ee_read<uint32_t>(p + 8), ee_read<uint32_t>(p + 12), ee_read<uint32_t>(p + 16), ee_read<uint32_t>(p + 20));
            }
            g_pre_call = [](EeInterpreter& ee) {
                const VUState& p = vu0::global().st;
                ee.vu0().vf = p.vf;
                ee.vu0().vi = p.vi;
                ee.vu0().acc = p.acc;
                ee.vu0().q = p.q;
                ee.vu0().i = p.i;
                ee.vu0().mem = p.mem;
                ee.set_vu0_coexec(true, 1, 1);
            };
            return Args{{0x01e80000u, 0x01f80000u}, {}};
        }, [](A a) {
            moby_skin_all(a.i[0], a.i[1]);
            return uint64_t{0};
        });
        g_ignored_words = ign;
        g_pre_call = nullptr;
    }
    {
        // the drop shadows on the state the (proved) draw loop leaves, deferral on
        auto deferred = [mobys] {
            do {
                mobys();
                spr_moby_fill();
                moby_draw_loop(ee_read<uint32_t>(OV(0x0015ffd8)), 0x01e80000u, -1, 1);
            } while (ee_read<uint32_t>(OV(0x0015fffc)) == 0);
            vu0::vif0_send_chain(0x0010e4c0, true);
        };
        check("shadow_points", 0x00285a68, 40, Ret::None, [deferred] {
            deferred();
            return Args{};
        }, [](A) { shadow_points(); return uint64_t{0}; });
        g_ignored_words.push_back(OV(0x0016044c));  // 0x1ff3c0's w lane: a stale VU0 lane (as vcross_lanes())
        check("drop_shadows_draw", 0x00285870, 40, Ret::None, [deferred] {
            deferred();
            shadow_points();
            g_pre_call = [](EeInterpreter& ee) {
                const VUState& p = vu0::global().st;
                ee.vu0().vf = p.vf;
                ee.vu0().vi = p.vi;
                ee.vu0().acc = p.acc;
                ee.vu0().q = p.q;
                ee.vu0().i = p.i;
                ee.vu0().mem = p.mem;
            };
            return Args{{OV(0x001ac780u)}, {}};
        }, [](A a) { drop_shadows_draw(a.i[0]); return uint64_t{0}; });
        g_pre_call = nullptr;
    }
    // The ported VU0 programs against the interpreter on the uploaded code, in lock step from
    // random states at every entry: after each instruction the registers (vf, vi, ACC, Q, I),
    // the MAC / status / clip flags, the data memory and the pc compared.
    if (std::getenv("ORACLE_ONLY") == nullptr || std::string(std::getenv("ORACLE_ONLY")) == "moby_vu0") {
        vu0::vif0_send_chain(0x00100080, true);
        vu0::vif0_send_chain(0x0010e4c0, true);
        const auto& micro = vu0::global().micro;
        struct Entry { uint32_t pc; VuNativeStep step; };
        const Entry entries[] = {{0x000, &moby_vu0_skin_step}, {0x111, &moby_vu0_skin_step}, {0x121, &moby_vu0_skin_step},
                                 {0x12d, &moby_vu0_skin_step}, {0x190, &moby_vu0_shadow_step}, {0x192, &moby_vu0_shadow_step}};
        const int cases = 300;
        int ok = 0, shown = 0;
        uint64_t steps_total = 0;
        for (int c = 0; c < cases; ++c) {
            const Entry& e = entries[c % 6];
            VUState a{};
            for (int r = 1; r < 32; ++r)
                for (int l = 0; l < 4; ++l) a.vf[static_cast<size_t>(r)].f[l] = ri(0, 9) == 0 ? u2f(R()) : rf(-50, 50);
            a.vf[0].f[3] = 1.0f;
            for (int l = 0; l < 4; ++l) a.acc.f[l] = rf(-5, 5);
            for (int r = 1; r < 16; ++r) a.vi[static_cast<size_t>(r)] = static_cast<int16_t>(ri(0, 3) == 0 ? ri(-3, 0x3ff) : ri(0, 8));
            for (size_t k = 0; k < a.mem.size(); k += 4) {
                const float f = ri(0, 9) == 0 ? u2f(R()) : rf(-20, 20);
                std::memcpy(&a.mem[k], &f, 4);
            }
            a.q = rf(-2, 2);
            a.i = rf(-2, 2);
            a.macflag = R() & 0xffff;
            a.statusflag = R() & 0xfff;
            a.clipflag = R() & 0xffffff;
            VUState b = a;
            VuMicroRun ra, rb;
            ra.start(a, micro.data(), micro.size(), e.pc);
            rb.start(b, micro.data(), micro.size(), e.pc, e.step);
            bool same = true;
            int n = 0;
            for (; n < 4000; ++n) {
                if (n % 37 == 36) {  // the EE's CTC2: the program's waits on vi1 / vi2 released
                    a.vi[1] = b.vi[1] = 0;
                    a.vi[2] = b.vi[2] = 0;
                }
                const bool ga = ra.step(), gb = rb.step();
                auto eqv = [](const VUVector& x, const VUVector& y) { return std::memcmp(&x, &y, 16) == 0; };
                bool d = ga != gb || ra.pc() != rb.pc() || a.vi != b.vi || !eqv(a.acc, b.acc) || f2u(a.q) != f2u(b.q) || f2u(a.i) != f2u(b.i) ||
                         a.macflag != b.macflag || a.statusflag != b.statusflag || a.clipflag != b.clipflag || a.mem != b.mem;
                for (int r = 0; r < 32 && !d; ++r) d = !eqv(a.vf[static_cast<size_t>(r)], b.vf[static_cast<size_t>(r)]);
                if (d) {
                    same = false;
                    if (shown++ < 4) {
                        std::printf("  moby_vu0: case %d entry %03x: differs after %d steps (pc %03x / %03x)\n", c, e.pc, n + 1, ra.pc(), rb.pc());
                        for (int r = 0; r < 32; ++r)
                            if (!eqv(a.vf[static_cast<size_t>(r)], b.vf[static_cast<size_t>(r)]))
                                std::printf("    vf%d interp %08x %08x %08x %08x port %08x %08x %08x %08x\n", r, a.vf[r].u[0], a.vf[r].u[1], a.vf[r].u[2], a.vf[r].u[3], b.vf[r].u[0], b.vf[r].u[1], b.vf[r].u[2], b.vf[r].u[3]);
                        for (int r = 0; r < 16; ++r)
                            if (a.vi[r] != b.vi[r]) std::printf("    vi%d interp %d port %d\n", r, a.vi[r], b.vi[r]);
                        if (a.macflag != b.macflag) std::printf("    mac %04x / %04x\n", a.macflag, b.macflag);
                        if (a.statusflag != b.statusflag) std::printf("    status %03x / %03x\n", a.statusflag, b.statusflag);
                        if (f2u(a.q) != f2u(b.q)) std::printf("    q %08x / %08x\n", f2u(a.q), f2u(b.q));
                        if (!eqv(a.acc, b.acc)) std::printf("    acc differs\n");
                        if (a.mem != b.mem) std::printf("    mem differs\n");
                    }
                    break;
                }
                if (!ga) break;
            }
            steps_total += static_cast<uint64_t>(n);
            if (same) ++ok;
        }
        std::printf("%-26s %s  %s (%d/%d, %llu steps)\n", "moby_vu0", "0x00100090", ok == cases ? "ok" : "FAIL", ok, cases, static_cast<unsigned long long>(steps_total));
        if (ok != cases) ++g_fail;
    }
    if (std::getenv("ORACLE_ONLY") == nullptr || std::string(std::getenv("ORACLE_ONLY")) == "moby_vu1") {
        // The VU1 moby programs: the states they start on in the running game (RAC1_VU1_CAPTURE
        // of rac1-native into assets/dumps/oracle/vu1_moby, generated: not in the repo), each as captured
        // and with its registers and floats perturbed (other clipping / lighting paths), run by
        // the interpreter on the uploaded original and by the port in lockstep.
        const char* dir_env = std::getenv("ORACLE_VU1_DIR");
        const std::string dir = dir_env != nullptr ? std::string(dir_env) : dump_path("vu1_moby");
        std::vector<std::string> files;
        if (std::filesystem::is_directory(dir))
            for (const auto& de : std::filesystem::recursive_directory_iterator(dir))
                if (de.path().extension() == ".bin") files.push_back(de.path().string());
        std::sort(files.begin(), files.end());
        int cases = 0, ok = 0, shown = 0, kicks_total = 0;
        uint64_t steps_total = 0;
        auto micro = std::make_unique<std::array<uint64_t, 2048>>();
        for (const std::string& path : files) {
            FILE* fp = std::fopen(path.c_str(), "rb");
            if (fp == nullptr) continue;
            uint32_t hdr[4];
            VUState base{};
            float fr[4];
            uint32_t fl[4];
            bool rd = std::fread(hdr, 4, 4, fp) == 4 && std::fread(base.vf.data(), sizeof(VUVector), 32, fp) == 32 &&
                      std::fread(base.vi.data(), 2, 16, fp) == 16 && std::fread(&base.acc, sizeof(VUVector), 1, fp) == 1 &&
                      std::fread(fr, 4, 4, fp) == 4 && std::fread(fl, 4, 4, fp) == 4 &&
                      std::fread(base.mem.data(), 1, base.mem.size(), fp) == base.mem.size() && std::fread(micro->data(), 8, 2048, fp) == 2048;
            std::fclose(fp);
            if (!rd || hdr[0] != 0x31555676u) continue;
            if (vu1::code_hash(micro->data(), 0x508) != 0x1585563a86921802ull) continue;  // (another pass's programs)
            base.q = fr[0], base.i = fr[1], base.r = fr[2], base.p = fr[3];
            base.macflag = fl[0], base.statusflag = fl[1], base.clipflag = fl[2];
            base.xtop = hdr[2];
            for (int variant = 0; variant < 4; ++variant) {
                VUState a = base;
                if (variant != 0) {
                    for (int r = 1; r < 32; ++r)
                        for (int l = 0; l < 4; ++l)
                            if (ri(0, 3) == 0) a.vf[static_cast<size_t>(r)].f[l] = rf(-50, 50);
                    for (size_t k = 0; k < a.mem.size(); k += 4) {
                        uint32_t w;
                        std::memcpy(&w, &a.mem[k], 4);
                        const uint32_t e = (w >> 23) & 0xff;
                        if (e > 0x60 && e < 0x9f && ri(0, 2) == 0) {  // a float: moved by up to +-25% (x2 for the clip tests)
                            float f = u2f(w) * rf(variant == 3 ? -1.0f : 0.75f, variant == 3 ? 2.0f : 1.25f);
                            std::memcpy(&a.mem[k], &f, 4);
                        }
                    }
                }
                VUState b = a;
                std::vector<std::vector<uint8_t>> ka, kb;
                auto tap = [](VUState& st, std::vector<std::vector<uint8_t>>& out) {
                    return [&st, &out](uint32_t addr) {
                        const uint32_t qwc = vu1::gif_packet_qwc(st, addr);
                        std::vector<uint8_t> pkt(static_cast<size_t>(qwc) * 16 + 4);
                        std::memcpy(pkt.data(), &addr, 4);
                        for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&pkt[4 + q * 16], &st.mem[((addr + q) & 0x3ff) << 4], 16);
                        out.push_back(std::move(pkt));
                    };
                };
                a.xgkick_hook = tap(a, ka);
                b.xgkick_hook = tap(b, kb);
                VuMicroRun ra, rb;
                ra.start(a, micro->data(), micro->size(), hdr[1]);
                rb.start(b, micro->data(), micro->size(), hdr[1], &moby_vu1_step);
                bool same = true;
                int n = 0;
                for (; n < 400000; ++n) {
                    const bool ga = ra.step(), gb = rb.step();
                    auto eqv = [](const VUVector& x, const VUVector& y) { return std::memcmp(&x, &y, 16) == 0; };
                    const bool last = !ga || !gb;
                    bool d = ga != gb || ra.pc() != rb.pc() || a.vi != b.vi || !eqv(a.acc, b.acc) || f2u(a.q) != f2u(b.q) || f2u(a.i) != f2u(b.i) ||
                             a.macflag != b.macflag || a.statusflag != b.statusflag || a.clipflag != b.clipflag || ka.size() != kb.size() ||
                             ((n % 64 == 63 || last) && a.mem != b.mem);
                    for (int r = 0; r < 32 && !d; ++r) d = !eqv(a.vf[static_cast<size_t>(r)], b.vf[static_cast<size_t>(r)]);
                    if (last && !d) {
                        ra.finish();
                        rb.finish();
                        d = ka != kb;
                    }
                    if (d) {
                        same = false;
                        if (shown++ < 4) {
                            std::printf("  moby_vu1: %s variant %d (start %03x): differs after %d steps (pc %03x / %03x)\n", path.c_str(), variant, hdr[1], n + 1, ra.pc(), rb.pc());
                            for (int r = 0; r < 32; ++r)
                                if (!eqv(a.vf[static_cast<size_t>(r)], b.vf[static_cast<size_t>(r)]))
                                    std::printf("    vf%d interp %08x %08x %08x %08x port %08x %08x %08x %08x\n", r, a.vf[r].u[0], a.vf[r].u[1], a.vf[r].u[2], a.vf[r].u[3], b.vf[r].u[0], b.vf[r].u[1], b.vf[r].u[2], b.vf[r].u[3]);
                            for (int r = 0; r < 16; ++r)
                                if (a.vi[r] != b.vi[r]) std::printf("    vi%d interp %d port %d\n", r, a.vi[r], b.vi[r]);
                            if (a.macflag != b.macflag) std::printf("    mac %04x / %04x\n", a.macflag, b.macflag);
                            if (a.statusflag != b.statusflag) std::printf("    status %03x / %03x\n", a.statusflag, b.statusflag);
                            if (a.clipflag != b.clipflag) std::printf("    clip %06x / %06x\n", a.clipflag, b.clipflag);
                            if (f2u(a.q) != f2u(b.q)) std::printf("    q %08x / %08x\n", f2u(a.q), f2u(b.q));
                            if (!eqv(a.acc, b.acc)) std::printf("    acc differs\n");
                            if (a.mem != b.mem) std::printf("    mem differs\n");
                            if (ka != kb) std::printf("    kicks differ (%zu / %zu)\n", ka.size(), kb.size());
                        }
                        break;
                    }
                    if (last) break;
                }
                steps_total += static_cast<uint64_t>(n);
                kicks_total += static_cast<int>(ka.size());
                ++cases;
                if (same) ++ok;
            }
        }
        if (cases == 0 && files.empty()) {  // no captures recorded on this machine: skipped, not failed
            std::printf("%-26s %s  skipped (no captured states in %s: run rac1-native with RAC1_VU1_CAPTURE=%s)\n", "moby_vu1", "0x0010faa8", dir.c_str(), dir.c_str());
            ++g_skipped;
        } else if (cases == 0) {
            std::printf("%-26s %s  FAIL (no captured states in %s: run rac1-native with RAC1_VU1_CAPTURE=%s)\n", "moby_vu1", "0x0010faa8", dir.c_str(), dir.c_str());
            ++g_fail;
        } else {
            std::printf("%-26s %s  %s (%d/%d, %llu steps, %d kicks)\n", "moby_vu1", "0x0010faa8", ok == cases ? "ok" : "FAIL", ok, cases, static_cast<unsigned long long>(steps_total), kicks_total);
            if (ok != cases) ++g_fail;
        }
    }
    if (std::getenv("ORACLE_SKIN_RATIO") != nullptr) {  // the original against itself at another VU0 / EE pace
        // ORACLE_SKIN_RATIO=a/b,c/d: a VU0 instructions per b EE instructions, then c per d
        static int r[4] = {1, 1, 1, 1};
        std::sscanf(std::getenv("ORACLE_SKIN_RATIO"), "%d/%d,%d/%d", &r[0], &r[1], &r[2], &r[3]);
        static int which = 0;
        static vu0m::Regs pose_regs;
        uint32_t job = 0;
        check("moby_skin_ratio", 0x001ee858, 30, Ret::Int32, [drawn, record, &job] {
            int recs = 0;
            do {
                drawn();
                recs = 0;
                for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && recs < 500; p = record(p, 1)) ++recs;
            } while (recs == 0);
            job = record(0x01e80000u, ri(0, recs - 1));
            ee_vif0_chain(0x00100080, true);
            mem_copy128(0x70003800, OV(0x00166080), 0x800);
            vu0m::regs() = vu0_in;
            moby_joints_pose(job, 0x01f80000u);
            pose_regs = vu0m::regs();
            which = 0;
            g_pre_call = [](EeInterpreter& ee) {
                static VUState saved;  // the VU0 as the first run found it (integer registers, data memory)
                if (which == 0) saved = ee.vu0();
                else {
                    ee.vu0().vi = saved.vi;
                    ee.vu0().mem = saved.mem;
                    ee.vu0().acc = saved.acc;
                    ee.vu0().q = saved.q;
                    ee.vu0().i = saved.i;
                }
                for (int i = 0; i < 32; ++i)
                    for (int k = 0; k < 4; ++k) ee.vu0().vf[static_cast<size_t>(i)].f[k] = pose_regs.vf[i].v[k];
                ee.set_vu0_coexec(true, r[2 * which], r[2 * which + 1]);
                which = 1;
            };
            return Args{{job, 0x01f80000u}, {}};
        }, [](A a) {
            const OrigResult o = call_original(OVF(0x001ee858), a.i, a.f);
            return static_cast<uint64_t>(o.v0);
        });
    }
    if (std::getenv("ORACLE_SKIN_PROBE") != nullptr) {  // the original 0x1ee858 beside its VU0 program (no port yet)
        const int num = std::atoi(std::getenv("ORACLE_SKIN_PROBE"));
        uint32_t job = 0;
        check("moby_skin_probe", 0x001ee858, 20, Ret::Int32, [drawn, record, &job, num] {
            int recs = 0;
            do {
                drawn();
                recs = 0;
                for (uint32_t p = 0x01e80000u; ee_read<uint32_t>(p) != 0 && recs < 500; p = record(p, 1)) ++recs;
            } while (recs == 0);
            job = record(0x01e80000u, ri(0, recs - 1));
            ee_vif0_chain(0x00100080, true);
            mem_copy128(0x70003800, OV(0x00166080), 0x800);
            vu0m::regs() = vu0_in;
            moby_joints_pose(job, 0x01f80000u);
            g_pre_call = [num](EeInterpreter& ee) {
                for (int i = 0; i < 32; ++i)
                    for (int k = 0; k < 4; ++k) ee.vu0().vf[static_cast<size_t>(i)].f[k] = vu0m::regs().vf[i].v[k];
                ee.set_vu0_coexec(true, num, 1);
            };
            return Args{{job, 0x01f80000u}, {}};
        }, [](A) { return uint64_t{0}; });
    }
    g_pre_call = nullptr;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
