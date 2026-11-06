// Oracle batch 483: the sky's routines written by hand (src/game/ovl/sky_hand.cpp), against each level's own original (its overlay
// map), on the RAM dumps of batch 480 (world_cull/: levels 0, 3, 5, 6, 11, 12 and 18, see batch_480.cpp's header for the command
// lines), with the frame's matrices and tables: 0x28c14c (a sky part's vertices to the screen) on random vertices about the view,
// 0x28c3c0 / 0x28c298 (its triangles' packets, textured / coloured) on random projected vertices and triangles (some textures not
// yet made this frame), 0x28c680 (spheres against the view) on random spheres, 0x28bd58 (the sprites) on the frame's own, their
// flags, angles and textures varied. Skipped without the dumps.

#include "ovl_oracle.h"

#include <game/ovl/sky_hand.h>
#include <runtime/vu/vu0.h>

#include <cstdio>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
const std::vector<uint8_t>& dump483(const std::string& path) {
    static std::map<std::string, std::vector<uint8_t>> cache;
    std::vector<uint8_t>& d = cache[path];
    if (d.empty()) {
        if (FILE* f = std::fopen(path.c_str(), "rb")) {
            d.resize(0x02000000 + 0x4000);
            if (std::fread(d.data(), 1, d.size(), f) != d.size()) d.clear();
            std::fclose(f);
        }
    }
    return d;
}
void load_dump(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_get_scratchpad_base(), d.data() + 0x02000000, 0x4000);
}
// the interpreter's VU0 as the port's (the routines read lanes their callers left)
void vu0_to_interpreter(EeInterpreter& ee) {
    const VUState& p = vu0::global().st;
    ee.vu0().vf = p.vf;
    ee.vu0().vi = p.vi;
    ee.vu0().acc = p.acc;
    ee.vu0().q = p.q;
    ee.vu0().i = p.i;
    ee.vu0().mem = p.mem;
}
}  // namespace

void batch_483() {
    if (g_only_batch != 483) return;
    g_batch = 483;
    std::vector<std::string> dumps;
    for (int k = 0; k < 4; ++k) {
        const std::string p = dump_path("world_cull/l" + std::to_string(g_oracle_level) + "_" + std::to_string(k) + ".bin");
        if (!dump483(p).empty()) dumps.push_back(p);
    }
    if (dumps.empty()) {
        const char* why = "no world_cull/ dumps for this level: see batch_480.cpp's header";
        skip_check("sky 0x28c14c", 0x0028c14c, why);
        skip_check("sky 0x28c3c0", 0x0028c3c0, why);
        skip_check("sky 0x28c298", 0x0028c298, why);
        skip_check("sky 0x28bd58", 0x0028bd58, why);
        skip_check("sky 0x28c680", 0x0028c680, why);
        return;
    }
    vu0::vif0_send_chain(0x0010e4c0, true);  // (the VU0 programs the frame has when the sky runs: 0x190 / 0x192, a cosine and a sine)
    {
        int n = 0;
        g_pre_call = vu0_to_interpreter;
        check("sky 0x28c14c", 0x0028c14c, 12 * static_cast<int>(dumps.size()), Ret::Int32, [&] {
            load_dump(dump483(dumps[static_cast<size_t>(n++) % dumps.size()]));
            const int count = ri(1, 60);
            const int spread = coin() ? 2000 : 30000;
            for (int i = 0; i < count + 1; ++i) {
                for (uint32_t k = 0; k < 3; ++k) w16(T + 8 * u(i) + 2 * k, u(ri(-spread, spread)));
                w8(T + 8 * u(i) + 6, R());
                w8(T + 8 * u(i) + 7, R());
            }
            return Args{{T, 0x70002000u, u(count)}, {}};
        }, [](A a) {
            uint32_t r = 0;
            if (!sky_hand::project(a.i[0], a.i[1], a.i[2], r)) std::printf("  [sky 0x28c14c] the hand-written routine did not run\n");
            return uint64_t{r};
        });
        n = 0;
        check("sky 0x28c3c0", 0x0028c3c0, 12 * static_cast<int>(dumps.size()), Ret::Int32, [&] {
            load_dump(dump483(dumps[static_cast<size_t>(n++) % dumps.size()]));
            // projected vertices (as 0x28c14c leaves them at SPR 0x2000: x, y, z words, a byte at +0xc, the clip bits at +0xf), their
            // texture coordinates, triangles with a few textures (some not yet made this frame: their TEX0 zeroed)
            const int verts = ri(3, 40), tris = ri(1, 40);
            for (int i = 0; i < verts; ++i) {
                const uint32_t v = 0x70002000u + 0x10 * u(i);
                w32(v, R()), w32(v + 4, R()), w32(v + 8, R()), w32(v + 0xc, R());
                w8(v + 0xf, ri(0, 2) == 0 ? R() & 0x2f : 0);
                w32(T2 + 4 * u(i), R());
            }
            const uint32_t textures = ee_read<uint32_t>(ee_read<uint32_t>(OV(0x0016051cu)) + 0x10);
            for (int k = 0; k < 4; ++k)
                if (coin()) w32(textures + 0x10 * u(k), 0), w32(textures + 0x10 * u(k) + 4, 0);
            for (int i = 0; i < tris; ++i) {
                for (uint32_t k = 0; k < 3; ++k) w8(T + 4 * u(i) + k, u(ri(0, verts - 1)));
                w8(T + 4 * u(i) + 3, u(ri(0, 3)));
            }
            return Args{{u(tris), T, T2, 0x70002000u}, {}};
        }, [](A a) {
            uint32_t r = 0;
            if (!sky_hand::triangles(a.i[0], a.i[1], a.i[2], a.i[3], r)) std::printf("  [sky 0x28c3c0] the hand-written routine did not run\n");
            return uint64_t{r};
        });
        n = 0;
        check("sky 0x28c298", 0x0028c298, 12 * static_cast<int>(dumps.size()), Ret::Int32, [&] {
            load_dump(dump483(dumps[static_cast<size_t>(n++) % dumps.size()]));
            const int verts = ri(3, 40), tris = ri(1, 40);
            for (int i = 0; i < verts; ++i) {
                const uint32_t v = 0x70002000u + 0x10 * u(i);
                w32(v, R()), w32(v + 4, R()), w32(v + 8, R()), w32(v + 0xc, R());
                w8(v + 0xf, ri(0, 2) == 0 ? R() & 0x2f : 0);
                w32(T2 + 4 * u(i), R());
            }
            for (int i = 0; i < tris; ++i)
                for (uint32_t k = 0; k < 4; ++k) w8(T + 4 * u(i) + k, k == 3 ? R() : u(ri(0, verts - 1)));
            return Args{{u(tris), T, T2, 0x70002000u}, {}};
        }, [](A a) { return uint64_t{sky_hand::colour_triangles(a.i[0], a.i[1], a.i[2], a.i[3])}; });
        n = 0;
        check("sky 0x28bd58", 0x0028bd58, 12 * static_cast<int>(dumps.size()), Ret::Int32, [&] {
            load_dump(dump483(dumps[static_cast<size_t>(n++) % dumps.size()]));
            // the frame's sprites, past the first round with their seen flags, angles and textures (some not yet made) varied
            const uint32_t sky = ee_read<uint32_t>(OV(0x0016051cu));
            if (n > static_cast<int>(dumps.size()) && sky != 0) {
                const uint32_t list = ee_read<uint32_t>(sky + 0x1c), textures = ee_read<uint32_t>(sky + 0x10);
                const uint32_t count = ee_read<uint16_t>(sky + 8);
                for (uint32_t i = 0; i < count && i < 200; ++i) {
                    const uint32_t e = list + 0x20 * i;
                    if (coin()) w8(e + 1, u(ri(0, 1)));
                    if (ri(0, 3) == 0) wf(e + 8, rf(-7, 7));
                    const int8_t tex = static_cast<int8_t>(ee_read<uint8_t>(e + 2));
                    if (tex >= 0 && ri(0, 2) == 0) w32(textures + 0x10 * u(tex), 0), w32(textures + 0x10 * u(tex) + 4, 0);
                }
            }
            return Args{};
        }, [](A) {
            uint32_t r = 0;
            if (!sky_hand::sprites(r)) std::printf("  [sky 0x28bd58] the hand-written routine did not run\n");
            return uint64_t{r};
        });
        n = 0;
        check("sky 0x28c680", 0x0028c680, 12 * static_cast<int>(dumps.size()), Ret::Int32, [&] {
            load_dump(dump483(dumps[static_cast<size_t>(n++) % dumps.size()]));
            const int count = ri(1, 40);
            const float spread = coin() ? 50.0f : 2000.0f;
            for (int i = 0; i < count; ++i) {
                wfrand(T + 0x20 * u(i), 3, -spread, spread);
                wf(T + 0x20 * u(i) + 0xc, rf(0.1f, spread / 4));
            }
            return Args{{T, u(count), OUT}, {}};
        }, [](A a) {
            if (!sky_hand::spheres(a.i[0], a.i[1], a.i[2])) std::printf("  [sky 0x28c680] the hand-written routine did not run\n");
            return uint64_t{0};
        });
    }
    g_pre_call = nullptr;
}

}  // namespace rac1::oracle
