// Oracle batch 151 (task 0415): the Gadgebot command ring (Veldin's 0x238b18 / 0x238b80 / 0x238f88, the HUD element the hero's
// input requests with triangle while the hero is Clank) and the Gadgebots' answer to a command, on RAM dumps of Blarg's Clank
// section (tools/oracle/blarg_tour.sh clankdumps: assets/dumps/oracle/clank_0415/ring_<frame>.bin, skipped without them; ORACLE_LEVEL=6):
//   ring_3015.bin  the ring open by the snake robot 1048, Attack chosen, the four Gadgebots in range (0x17e504 = 4);
//   ring_3815.bin  the ring open by the Gadgebot Entrance 1302, Enter chosen.
// Checked: the ring's start / frame / draw on HUD slot 3 (the pad varied: triangle held or let go, the d-pad, the stick; the slot
// shown, the timers, the fades), and Blarg's class 857 (the Gadgebot, L6:0x2f0040) with each command 0..4 in 0x141610.
// The random-state checks of the three ring functions are in batch 67 (level 0; also run with ORACLE_LEVEL=6 / 10).
#include "ovl_oracle.h"

#include <game/ovl/shared_class857.h>
#include <game/ovl/blarg_mobys.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/variants_l6.h>
#include <runtime/ee/ee_interpreter.h>

#include <optional>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <map>
#include <string>
#include <vector>

namespace rac1::oracle {

namespace {
// The checks named here reach segment_collide, whose crossing lists, chunk buffers and vertices the port keeps on
// the host stack (the original: 0x70003000 .. 0x70003fff, 0x70000000 .., 0x70002000): A2's collision scratch ranges
// are left out of these checks, and only of them (stage 5, task C2c).
bool c2c_spr_check(const char* name) {
    static const char* const kNames[] = {"gadgebot_857_command"};
    for (const char* n : kNames)
        if (std::strcmp(n, name) == 0) return true;
    return false;
}

const std::vector<uint8_t>& cl_dump(const std::string& path) {
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
void cl_load(const std::vector<uint8_t>& d) {
    std::memcpy(ee_get_ram_base(), d.data(), 0x02000000);
    std::memcpy(ee_ram_ptr(0x70000000u), d.data() + 0x02000000, 0x4000);
}
// a check of the level's own function at `at` (not a Veldin address to translate)
void lvcheck(const char* name, uint32_t at, int n, Ret ret, const std::function<Args()>& gen, const std::function<uint64_t(const Args&)>& port) {
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s L%d:0x%08x\n", name, g_oracle_level, at);
    std::optional<CollisionScratchIgnored> spr_ignored;
    if (c2c_spr_check(name)) spr_ignored.emplace();
    if (!check_fn(name, at, n, ret, gen, port, Scope::FullRam, 0, 0)) ++g_fail;
}
constexpr uint32_t PAD = 0x0013c940;
}  // namespace

void batch_151() {
    if (g_only_batch != 151) return;
    g_batch = 151;
    const char* names[] = {"gadgebot_cmd_start", "gadgebot_cmd_update", "gadgebot_cmd_draw", "gadgebot_857_command"};
    if (g_oracle_level != 6) {
        for (const char* n : names) skip_check(n, 0x00238b80, "Blarg's Clank section: ORACLE_LEVEL=6");
        return;
    }
    const std::vector<const std::vector<uint8_t>*> dumps = {&cl_dump(dump_path("clank_0415/ring_3015.bin")), &cl_dump(dump_path("clank_0415/ring_3815.bin"))};
    if (dumps[0]->empty() || dumps[1]->empty()) {
        for (const char* n : names) skip_check(n, 0x00238b80, "no RAM dump (tools/oracle/blarg_tour.sh clankdumps: assets/dumps/oracle/clank_0415)");
        return;
    }
    const std::vector<uint32_t> saved_ignored = g_ignored_words;
    g_ignored_words = vcross_lanes();
    const bool null_ram = g_ee_null_is_ram;
    g_ee_null_is_ram = true;  // (as in the runtime)
    const uint32_t e = OV(0x0017dc50) + 3 * 0x90;  // (the ring's HUD slot)

    // the ring on a dump, the pad varied
    auto ring = [&dumps, e] {
        cl_load(*dumps[static_cast<size_t>(ri(0, 1))]);
        const float r = ri(0, 2) == 0 ? rf(0, 0.6f) : rf(0.85f, 1.0f), t = rf(-3.2f, 3.2f);
        wf(PAD + 0x148, r * std::cos(t));
        wf(PAD + 0x14c, r * std::sin(t));
        w32(PAD + 0x1c0, ri(0, 2) != 0 ? 0x10u : 0u);  // (triangle held, or let go: the command given)
        w32(PAD + 0x1c4, ri(0, 2) == 0 ? (R() & 0xf000) : 0u);
        if (coin()) w32(e + 0x74, u(ri(-2, 3)));
        if (ri(0, 3) == 0) w32(e + 0x78, coin() ? 0x010000ffu : u(ri(0, 3)));
        if (coin()) w8(e + 0x70, u(ri(0, 8)));
        if (coin()) w8(e + 0x71, u(ri(0, 8)));
        if (ri(0, 4) == 0) w32(OV(0x0017e504), 0);  // (no Gadgebot in range: the icons greyed, the stick locked)
        w32(OV(0x001611c0), ee_read<uint32_t>(OV(0x001611c0)) + u(ri(0, 0x40)) * 0x10);  // (the frame packet's cursor on a little)
        return Args{{e}, {}};
    };
    check("gadgebot_cmd_start", 0x00238b18, 10, Ret::None, ring, [](A a) { gadgebot_cmd_start(a.i[0]); return uint64_t{0}; });
    check("gadgebot_cmd_update", 0x00238b80, 80, Ret::None, ring, [](A a) { gadgebot_cmd_update(a.i[0]); return uint64_t{0}; });
    check("gadgebot_cmd_draw", 0x00238f88, 60, Ret::Int32, ring, [](A a) { return ri32(gadgebot_cmd_draw(a.i[0])); });

    // a Gadgebot given each command (the hero by it on the dumps: in range)
    lvcheck("gadgebot_857_command", L6_ADDR(0x002f0040u), 60, Ret::None, [&dumps] {
        cl_load(*dumps[static_cast<size_t>(ri(0, 1))]);
        std::vector<uint32_t> bots;
        const uint32_t first = ee_read<uint32_t>(OV(0x0015ffd8));
        for (uint32_t m = first; m < first + 0x100 * 3000; m += 0x100)
            if (ee_read<int16_t>(m + 0xa6) == 857 && ee_read<uint8_t>(m + 0x20) < 0xfd) bots.push_back(m);
        const uint32_t m = bots.empty() ? 0u : bots[static_cast<size_t>(ri(0, static_cast<int>(bots.size()) - 1))];
        w32(0x00141610, u(ri(0, 4)));
        return Args{{m}, {}};
    }, [](A a) { if (a.i[0] != 0) class857_update(a.i[0]); return uint64_t{0}; });
    g_ignored_words = saved_ignored;
    g_ee_null_is_ram = null_ram;
}

}  // namespace rac1::oracle
