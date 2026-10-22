// Shared helpers of the overlay oracle (ovl_oracle.cpp and its per-batch files batch_*.cpp):
// test areas, the random generator (one sequence for the whole run), RAM writers, check().
#ifndef RAC1_TESTS_OVL_ORACLE_H
#define RAC1_TESTS_OVL_ORACLE_H

#include "oracle_env.h"

#include <bit>
#include <game/ovl/globals.h>
#include <game/lib/libc.h>
#include <game/math/vecmath.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_reloc.h>
#include <game/ovl/enemy_749.h>
#include <game/input/pad_build.h>
#include <game/boot/level_read.h>
#include <game/lib/snd989.h>
#include <game/lib/libgraph.h>
#include <runtime/sif/sif_rpc.h>
#include <runtime/iop/stash_iop.h>

#include <cstdio>
#include <cstring>
#include <functional>
#include <array>
#include <cstdlib>
#include <random>
#include <string>
#include <vector>
#include <algorithm>

namespace rac1::oracle {


constexpr uint32_t M = TEST_AREA + 0x0000;   // a moby-like struct
constexpr uint32_t M2 = TEST_AREA + 0x0200;  // a second one
constexpr uint32_t T = TEST_AREA + 0x1000;   // tables / lists
constexpr uint32_t T2 = TEST_AREA + 0x2000;
constexpr uint32_t OUT = TEST_AREA + 0x3000;
constexpr uint32_t T3 = TEST_AREA + 0x4000;
constexpr uint32_t T4 = TEST_AREA + 0x5000;
constexpr uint32_t M3 = TEST_AREA + 0x0400;

// A level-only address where L<n>_ADDR cannot be used (a constant expression, or an address held in a
// variable: L<n>_ADDR translates a constant through the region maps): the NTSC-U address itself. The
// oracle runs on the NTSC-U disc only (setup_level), where those maps are the identity.
#define ORACLE_LADDR(n, a) (a)

template <typename T> inline T* hp(uint32_t a) { return static_cast<T*>(ee_ram_ptr(a)); }

inline std::mt19937 rng(20261001);
inline uint32_t R() { return static_cast<uint32_t>(rng()); }
inline int32_t ri(int32_t lo, int32_t hi) { return std::uniform_int_distribution<int32_t>(lo, hi)(rng); }
inline float rf(float lo, float hi) { return std::uniform_real_distribution<float>(lo, hi)(rng); }
inline bool coin() { return (R() & 1) != 0; }
inline uint32_t u(int32_t v) { return static_cast<uint32_t>(v); }
inline uint32_t sx16(uint32_t v) { return static_cast<uint32_t>(static_cast<int32_t>(static_cast<int16_t>(v))); }
inline int64_t sx(uint32_t v) { return static_cast<int64_t>(static_cast<int32_t>(v)); }

inline void w8(uint32_t a, uint32_t v) { ee_write<uint8_t>(a, static_cast<uint8_t>(v)); }
inline void w16(uint32_t a, uint32_t v) { ee_write<uint16_t>(a, static_cast<uint16_t>(v)); }
inline void w32(uint32_t a, uint32_t v) { ee_write<uint32_t>(a, v); }
inline void wf(uint32_t a, float v) { ee_write<float>(a, v); }
inline void wrand(uint32_t a, int words) { for (int i = 0; i < words; ++i) w32(a + 4 * i, R()); }
inline void wfrand(uint32_t a, int n, float lo, float hi) { for (int i = 0; i < n; ++i) wf(a + 4 * i, rf(lo, hi)); }

inline Vec4& VV(uint32_t a) { return *hp<Vec4>(a); }
inline Mat4& MM(uint32_t a) { return *hp<Mat4>(a); }
inline uint64_t r32(uint32_t v) { return v; }
inline uint64_t ri32(int32_t v) { return static_cast<uint32_t>(v); }
inline uint64_t rf32(float v) { return fbits(v); }

inline void seed_rand() { w32(ee_read<uint32_t>(0x0012f76c) + 0x58, R()); }

// The moby variables of the level's spawned mobys (debris, bolts), with memory_layout()'s blocks
// for Veldin (0x160000 each, 0x1c20b8[1]).
inline uint32_t pvars_lo() { return 0x017a0000u; }

inline int g_fail = 0;
inline int g_batch = 1, g_only_batch = 0;  // `ovl_oracle --batch N` runs only that batch
inline int g_anim_nmin = 1;  // fewest joints animenv gives the hero class (24 for the chain points 0x16 / 0x17)

inline int g_skipped = 0;  // checks whose function the level does not have (--level N)

// `orig` is Veldin's address of the function: on another level (--level N, task 0307) the
// original called is the level's equivalent (the overlay map's function, ovl_code_lookup); a
// function the level does not have is skipped and counted.
inline void check(const char* name, uint32_t orig, int n, Ret ret, const std::function<Args()>& gen,
           const std::function<uint64_t(const Args&)>& port, uint32_t ignore_word = 0, uint32_t ignore_word2 = 0) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;  // one function
    uint32_t at = orig;
    if (g_oracle_level != 0 && orig >= NOT_ADDR(0x001eaa00u) && orig < NOT_ADDR(0x002f0cd0u)) {  // (Veldin's overlay code)
        at = ovl_code_lookup(orig);
        if (at == 0) {
            std::printf("%-26s 0x%08x  skipped (no equivalent in level %d)\n", name, orig, g_oracle_level);
            ++g_skipped;
            return;
        }
        std::printf("%-26s 0x%08x -> level %d 0x%08x\n", name, orig, g_oracle_level, at);
    }
    if (!check_fn(name, at, n, ret, gen, port, Scope::FullRam, ignore_word, ignore_word2)) ++g_fail;
}


// A check whose generator needs what this level does not have (e.g. Veldin's class 1440 mobys
// as the subjects): skipped and counted, as one without an equivalent function.
inline void skip_check(const char* name, uint32_t orig, const char* why) {
    if (g_only_batch != 0 && g_only_batch != g_batch) return;
    if (const char* only = std::getenv("ORACLE_ONLY"); only != nullptr && std::strcmp(only, name) != 0) return;
    std::printf("%-26s 0x%08x  skipped (%s)\n", name, orig, why);
    ++g_skipped;
}

inline constexpr int N = 40;
using A = const Args&;

// The w lanes of rows built with VCROSS (0x1ff3c0): vf3.w is stale in the original.
std::vector<uint32_t> vcross_lanes();
// Veldin read from the disc and its level memory laid out, as a level start does (batches 59+).
void veldin_boot();
// The unit batches (1-58, 60, 62, 64): ovl_oracle_unit.cpp.
void batches_unit();

void batch_59();
void batch_63();
void batch_65();
void batch_66();
void batch_68();
void batch_70();
void batch_69();
void batch_67();
void batch_61();
void batch_71();
void batch_72();
void batch_73();
void batch_74();
void batch_75();
void batch_76();
void batch_78();
void batch_79();
void batch_80();
void batch_81();
void batch_82();
void batch_84();
void batch_86();
void batch_85();
void batch_90();
void batch_89();
void batch_95();
void batch_91();
void batch_93();
void batch_96();
void batch_100();
void batch_101();
void batch_103();
void batch_97();
void batch_102();
void batch_105();
void batch_98();
void batch_106();
void batch_104();
void batch_107();
void batch_110();
void batch_112();
void batch_108();
void batch_111();
void batch_114();
void batch_117();
void batch_119();
void batch_121();
void batch_120();
void batch_122();
void batch_127();
void batch_123();
void batch_124();
void batch_128();
void batch_129();
void batch_133();
void batch_131();
void batch_132();
void batch_116();
void batch_118();
void batch_109();
void batch_113();
void batch_115();
void batch_126();
void batch_125();
void batch_130();
void batch_135();
void batch_134();
void batch_136();
void batch_138();
void batch_139();
void batch_137();
void batch_140();
void batch_141();
void batch_142();
void batch_143();
void batch_144();
void batch_146();
void batch_147();
void batch_149();
void batch_150();
void batch_170();
void batch_171();
void batch_151();
void batch_153();
void batch_160();
void batch_180();
void batch_190();
void batch_191();
void batch_192();
void batch_200();  // Hoven (level 12): its own code (levels/pokitaru-hoven)
void batch_202();  // Pokitaru (11) and Hoven (12): their own classes on RAM dumps
void batch_203();  // the PDA (hero item 0x20) on levels 0, 8, 11, 12
void batch_220();  // Gemlik Base (level 13)
void batch_221();  // Oltanis (level 14)
void batch_222();  // Gemlik Base (level 13): RAM dumps
void batch_223();  // Oltanis (level 14): RAM dumps
void batch_240();
void batch_241();
void batch_242();
void batch_250();
void batch_260();
void batch_261();
void batch_270();
void batch_271();
void batch_310();
void batch_330();
void batch_340();
void batch_370();
void batch_371();
void batch_372();
void batch_400();
void batch_401();
void batch_410();
void batch_411();
void batch_440();
void batch_420();
void batch_470();

}  // namespace rac1::oracle

#endif
