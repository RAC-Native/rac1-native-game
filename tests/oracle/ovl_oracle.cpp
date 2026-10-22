// Differential test: the overlay functions ported in src/game/ovl/*.cpp and
// src/game/lib/libc.cpp vs the original code executed by the EE interpreter
// (ADR 0006). Baseline RAM = boot ELF + Veldin overlay as loaded from the
// user's ISO; each case randomizes the globals the function reads, then the
// whole of EE RAM and the scratchpad must end up identical, as must the
// return value.


#include <game/enhance/advanced.h>
#include <runtime/ee/ps2_fpu.h>
#include "ovl_oracle.h"

#include <game/ovl/ovl_reloc.h>
using namespace rac1;
using namespace rac1::oracle;

namespace rac1::oracle {

std::vector<uint32_t> vcross_lanes() {
    std::vector<uint32_t> v = {OV(0x00166fec), OV(0x00166ffc), OV(0x00168f6c), OV(0x00168f7c)};
    for (uint32_t c = OV(0x00167150); c < OV(0x00167150) + 48 * 0xa0; c += 0xa0) { v.push_back(c + 0x1c); v.push_back(c + 0x2c); }
    return v;
}

void veldin_boot() {
    static bool read_ok = false;
    if (!read_ok) read_ok = toc_read() && level_header_read(g_oracle_level);  // (--level N: that level's files)
    w32(0x0015ed84, u(g_oracle_level)); w32(0x0015ed80, 0); w16(0x0013e056, 0);
    level_files_read();
    if (g_oracle_level == 0) w32(OV(0x001611cc), NOT_ADDR(0x160000));  // Veldin's (0x1c20b8[1])
    else sub_002a07c8();  // the level's row of 0x1c20b8, as its overlay's start sets it
    memory_layout();
    mem_fill32(OV(0x00197bc0), 0xffffffffu, 0x800); mem_fill32(OV(0x0019a980), 0xffffffffu, 0xe00); mem_fill32(OV(0x0019a280), 0, 0xe0);
    mem_fill32(OV(0x001c5b00), 0xffffffffu, 0xc00); mem_fill32(OV(0x001c7700), 0xffffffffu, 0x800); mem_fill32(OV(0x001c6f00), 0, 0x80);
    mem_fill32(OV(0x001bc1b0), 0xffffffffu, 0x400); mem_fill32(OV(0x001bd3b0), 0xffffffffu, 0x400); mem_fill32(OV(0x001bcdb0), 0, 0x40);
    w32(OV(0x0015ffc0), 0); w32(OV(0x00160fbc), 0); w32(OV(0x0016048c), 0); w32(OV(0x0015ffc8), 0); w32(0x0015ee8c, 0x100000);
    sif_rpc_reset();
    snd_init();  // the base ELF's start: 989snd's EE side up
    stash_init();  // the overlay's start: the IOP texture stash bound
}

}  // namespace rac1::oracle

int main(int argc, char** argv) {
    rac1::ps2fpu::enter_ee_rounding();  // the EE's float rounding (toward zero), as the runtime runs
    rac1::g_advanced_disabled = true;  // the PC version's enhancements off: the faithful port alone (src/game/enhance/advanced.h)
    std::setvbuf(stdout, nullptr, _IONBF, 0);
    // --batch N: that batch only; --level N (or ORACLE_LEVEL=N): on level N's own overlay (task 0307)
    int level = std::getenv("ORACLE_LEVEL") != nullptr ? std::atoi(std::getenv("ORACLE_LEVEL")) : 0;
    for (int i = 1; i + 1 < argc; i += 2) {
        if (std::string(argv[i]) == "--batch") g_only_batch = std::atoi(argv[i + 1]);
        else if (std::string(argv[i]) == "--level") level = std::atoi(argv[i + 1]);
    }
    if (const char* sd = std::getenv("ORACLE_SEED")) rng.seed(static_cast<uint32_t>(std::strtoul(sd, nullptr, 0)));  // vary the random cases
    if (std::getenv("ORACLE_NULL_RAM") != nullptr) g_ee_null_is_ram = true;  // as in the runtime: a null pointer is RAM (the screen fade port writes its packet through a null pointer in the unit batches' states)
    if (!setup_level(level)) return SKIP_CODE;
    {  // 989snd's IOP servers for the tests: replies a function of the command alone (the same on
       // both sides), some zero (a sound that ended)
        auto result = [](uint32_t id, uint32_t a0) -> uint32_t {
            const uint32_t h = (id * 0x9e3779b1u) ^ (a0 * 0x85ebca6bu);
            return ((h >> 7) % 4 == 0) ? 0u : ((h >> 3) | 1u);
        };
        sif_rpc_register(0x123456, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            if (fno == 0x4d) {
                const uint32_t n = ee_read<uint32_t>(send);
                uint32_t p = send + 4;
                for (uint32_t i = 0; i < n; ++i) {
                    const uint32_t id = ee_read<uint16_t>(p), size = ee_read<uint16_t>(p + 2);
                    ee_write<uint32_t>(recv + 4 + i * 4, result(id, size != 0 ? ee_read<uint32_t>(p + 4) : 0u));
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
        sif_rpc_register(0x123457, [result](uint32_t fno, uint32_t send, uint32_t, uint32_t recv, uint32_t) {
            ee_write<uint32_t>(recv, result(0x1000 + fno, ee_read<uint32_t>(send)) | 0x100000u);
        });
        sound_callbacks_register();
        stash_iop_start();  // the stash daemon (deterministic: IOP RAM)
    }
    if (const char* d = std::getenv("ORACLE_DUMP")) {  // ORACLE_DUMP=addr,words: print loaded EE words
        const uint32_t a = static_cast<uint32_t>(std::strtoul(d, nullptr, 0));
        const char* c = std::strchr(d, ',');
        const int nw = c != nullptr ? std::atoi(c + 1) : 16;
        for (int i = 0; i < nw; ++i) std::printf("%08x %08x\n", a + 4 * u(i), ee_read<uint32_t>(a + 4 * u(i)));
        return 0;
    }
    if (std::getenv("ORACLE_BOOTREAD") != nullptr) {  // the disc reads of a level start (task 0293)
        const bool ok = toc_read() && level_header_read(0) && level_files_read();
        std::printf("boot read %s: data %08x gameplay %08x armor %08x, data header %08x %08x\n", ok ? "ok" : "FAILED", ee_read<uint32_t>(0x0015ee4c),
                    ee_read<uint32_t>(0x0015ee50), ee_read<uint32_t>(0x0015ee54), ee_read<uint32_t>(ee_read<uint32_t>(0x0015ee4c)),
                    ee_read<uint32_t>(ee_read<uint32_t>(0x0015ee4c) + 0x10));
        if (std::getenv("ORACLE_BOOTPORT") != nullptr) {  // the port's level setup on it
            ee_write<uint32_t>(OV(0x001611cc), NOT_ADDR(0x160000));  // Veldin's (0x1c20b8[1])
            memory_layout();
            mem_fill32(OV(0x00197bc0), 0xffffffffu, 0x800); mem_fill32(OV(0x001c5b00), 0xffffffffu, 0xc00); mem_fill32(OV(0x001bc1b0), 0xffffffffu, 0x400);
            ee_write<uint32_t>(OV(0x0015ffc0), 0); ee_write<uint32_t>(OV(0x00160fbc), 0); ee_write<uint32_t>(OV(0x0016048c), 0); ee_write<uint32_t>(OV(0x0015ffc8), 0);
            level_setup_body(1, 0);
            if (const char* d = std::getenv("ORACLE_BOOTDUMP")) {  // addr,words after the level setup
                const uint32_t a = static_cast<uint32_t>(std::strtoul(d, nullptr, 0));
                const char* c = std::strchr(d, ',');
                const int nw = c != nullptr ? std::atoi(c + 1) : 16;
                for (int i = 0; i < nw; ++i) std::printf("%08x %08x\n", a + 4 * u(i), ee_read<uint32_t>(a + 4 * u(i)));
            }
            for (uint32_t a : {OV(0x00173e1cu), OV(0x0015ffd8u), OV(0x0015ffdcu), OV(0x0015ffc0u), OV(0x00160fccu), OV(0x00160490u), OV(0x0015ef54u), OV(0x00160f50u), OV(0x0015f600u)})
                std::printf("%08x = %08x\n", a, ee_read<uint32_t>(a));
            for (int32_t i = 0; i < ee_read<int32_t>(OV(0x0015ef54)); ++i) {  // the level cameras: type, the class data
                const uint32_t e = ee_read<uint32_t>(OV(0x0015ef50)) + i * 0x20;
                std::printf("camera %d: type %d data %08x\n", i, ee_read<int16_t>(e + 0xc), ee_read<uint32_t>(e + 0x1c));
            }
            std::printf("moby array %08x..%08x list %08x..%08x hero %08x\n", ee_read<uint32_t>(OV(0x0015ff1c)), ee_read<uint32_t>(OV(0x0015ff20)),
                        ee_read<uint32_t>(OV(0x0015ffd8)), ee_read<uint32_t>(OV(0x0015ffdc)), ee_read<uint32_t>(0x001413d0));
            for (uint32_t m = ee_read<uint32_t>(OV(0x0015ffd8)), k = 0; k < 12; m += 0x100, ++k)
                std::printf("  moby %08x state %02x class %d pos %.1f %.1f %.1f flags %04x cls %08x\n", m, ee_read<uint8_t>(m + 0x20), ee_read<int16_t>(m + 0xa6),
                            ee_read<float>(m + 0x10), ee_read<float>(m + 0x14), ee_read<float>(m + 0x18), ee_read<uint16_t>(m + 0x34), ee_read<uint32_t>(m + 0x24));
        }
        return ok ? 0 : 1;
    }
    set_baseline_current();
    batches_unit();

    g_ignored_words.clear();
    batch_59();
    batch_63();
    batch_65();
    batch_66();
    batch_68();
    batch_70();
    batch_69();
    batch_67();
    batch_61();
    batch_71();
    batch_72();
    batch_73();
    batch_74();
    batch_75();
    batch_76();
    batch_78();
    batch_79();
    batch_80();
    batch_81();
    batch_82();
    batch_84();
    batch_86();
    batch_85();
    batch_90();
    batch_89();
    batch_95();
    batch_91();
    batch_93();
    batch_96();
    batch_100();
    batch_101();
    batch_103();
    batch_97();
    batch_102();
    batch_105();
    batch_98();
    batch_106();
    batch_104();
    batch_107();
    batch_110();
    batch_112();
    batch_108();
    batch_111();
    batch_114();
    batch_117();
    batch_119();
    batch_121();
    batch_120();
    batch_122();
    batch_127();
    batch_123();
    batch_124();
    batch_128();
    batch_129();
    batch_133();
    batch_131();
    batch_132();
    batch_116();
    batch_118();
    batch_109();
    batch_113();
    batch_115();
    batch_126();
    batch_125();
    batch_130();
    batch_135();
    batch_134();
    batch_136();
    batch_138();
    batch_139();
    batch_137();
    batch_140();
    batch_141();
    batch_142();
    batch_143();
    batch_144();
    batch_146();
    batch_147();
    batch_149();
    batch_150();
    batch_170();
    batch_171();
    batch_151();
    batch_153();
    batch_160();
    batch_180();
    batch_190();
    batch_191();
    batch_192();
    batch_200();
    batch_202();
    batch_203();
    batch_220();
    batch_221();
    batch_222();
    batch_223();
    batch_240();
    batch_241();
    batch_242();
    batch_250();
    batch_260();
    batch_261();
    batch_270();
    batch_271();
    batch_310();
    batch_330();
    batch_340();
    batch_370();
    batch_371();
    batch_372();
    batch_400();
    batch_401();
    batch_410();
    batch_411();
    batch_440();
    batch_420();
    batch_470();

    if (g_oracle_level != 0) std::printf("level %d: %d check(s) skipped (no equivalent function)\n", g_oracle_level, g_skipped);
    std::printf("%s: %d failing function(s)\n", g_fail ? "FAIL" : "PASS", g_fail);
    return g_fail ? 1 : 0;
}
