#ifndef RAC1_TESTS_ORACLE_ENV_H
#define RAC1_TESTS_ORACLE_ENV_H

// Differential-test environment (ADR 0006): runs original game functions with
// the EE interpreter and their C++ ports on the same EE RAM, then compares.
//
// Inputs come only from the user's ISO (./setup.sh): the boot ELF
// (assets/dumps/SCUS_971.99) and the level overlays read straight from the
// disc image. When they are missing the test exits with SKIP_CODE (77, the usual "skipped" code; the oracle is run by hand, not by CTest).

#include <runtime/ee/ee_interpreter.h>
#include <runtime/kernel/ee_memory.h>

#include <cstdint>
#include <cstdio>
#include <cstring>
#include <functional>
#include <initializer_list>
#include <string>
#include <vector>

namespace rac1::oracle {

constexpr int SKIP_CODE = 77;
constexpr uint32_t GP = 0x00166c00;          // $gp of the Veldin build
constexpr uint32_t TEST_AREA = 0x01E00000;   // scratch EE RAM used by tests
constexpr uint32_t TEST_AREA_SIZE = 0x10000;

// The directory of the recorded RAM dumps the batches read (docs/oracle.md): $RAC1_ORACLE_DUMPS, else
// assets/dumps/oracle. Never in the repository (game data). A batch whose dumps are missing skips its checks.
std::string dump_dir();
// `rel` under dump_dir() (e.g. "blarg_0390/c1.bin").
std::string dump_path(const std::string& rel);

// Loads the boot ELF and the Veldin overlay into EE RAM. False if assets are missing.
bool setup_veldin();
// The same with level N's own overlay (task 0307, ADR 0008 point 5): the ports' and the
// generators' overlay addresses translated to level N (ovl_reloc_set_level), the original
// functions called at level N's equivalents (check()). False if assets or the level's map are missing.
bool setup_level(int level);
extern int g_oracle_level;  // the level setup_level loaded (0: Veldin)

template <typename T> T& ee_ref(uint32_t addr) { return *static_cast<T*>(ee_ram_ptr(addr)); }

struct OrigResult {
    bool ok = false;
    uint64_t v0 = 0;
    uint32_t a3 = 0;  // $a3 at return (hand-written code that leaks it to its callers)
    float f0 = 0.0f;
    std::string error;
};

// Calls the original function: integer args in $a0..$a3, float args in $f12, $f13...
OrigResult call_original(uint32_t addr, std::initializer_list<uint32_t> args, std::initializer_list<float> fargs = {});
// Same, with up to 8 integer args ($a0..$a3, $t0..$t3; each sign-extended to 64 bits).
OrigResult call_original(uint32_t addr, const std::vector<uint32_t>& args, const std::vector<float>& fargs);

// Copy / restore a RAM range.
std::vector<uint8_t> save_range(uint32_t addr, uint32_t size);
void restore_range(uint32_t addr, const std::vector<uint8_t>& bytes);

// Reports the first differences between two saved copies of the same range.
// Returns the number of differing 32-bit words.
size_t diff_words(const char* what, uint32_t base, const std::vector<uint8_t>& a, const std::vector<uint8_t>& b,
                  int max_print = 8);

// --- generic differential check ---------------------------------------------

// EE words every full-RAM comparison skips (stale VU lanes the original leaves in its results).
extern std::vector<uint32_t> g_ignored_words;
// (task 0366) While set, a full-RAM / scratchpad comparison skips the w word (+0xc) of a quadword whose x, y, z words are equal on
// both sides: a VU0 register's stale w lane stored whole (sqc2) where the other lanes agree.
extern bool g_ignore_lone_w_lanes;
// Functions the interpreter skips (returning 0) while set: the TODO_PORT callees of a batch's checks.
extern std::vector<uint32_t> g_extra_noop_hooks;
// Functions the interpreter runs as host code while set: the host stand-ins the port calls in
// their place (e.g. the console's clock), the same code on both sides.
extern std::vector<std::pair<uint32_t, std::function<void(EeCpu&)>>> g_extra_hooks;
// Run on the interpreter just before each call of the original while set: registers a
// hand-written callee reads stale from its caller ($s6, VU0 lanes), set as the port receives them.
extern std::function<void(EeInterpreter&)> g_pre_call;
// The boot module's image in memory (task 0308, batch 89): the hooks of Veldin's overlay
// addresses (another function's code there) removed while set.
extern bool g_oracle_front;
// Clears the interpreter's vf25..vf29 before its next call: the collision routines' contact
// registers, which the port keeps in its own copy (collide_vu_reset() clears that one).
void oracle_vu_contact_reset();

enum class Ret { None, Int32, Int64, Float, A3 };
struct Args {
    std::vector<uint32_t> i;   // $a0..$a3, $t0..$t3 (up to 8)
    std::vector<float> f;      // $f12, $f13, $f14
};
enum class Scope { TestArea, FullRam };

// Runs `iters` cases: gen() prepares RAM/args, the original runs, RAM is
// restored, the port runs, then RAM (test area, or all of RAM + scratchpad
// except the interpreter stack) and the return value must be identical.
// ignore_word: one EE word excluded from the comparison (a lane the original leaves
// undefined, e.g. copied from an uninitialised stack slot). Returns true when all cases
// match. Prints one summary line.
bool check_fn(const char* name, uint32_t orig, int iters, Ret ret, const std::function<Args()>& gen,
              const std::function<uint64_t(const Args&)>& port, Scope scope = Scope::TestArea,
              uint32_t ignore_word = 0, uint32_t ignore_word2 = 0);

// Uses the current EE RAM (e.g. right after setup_veldin()) as the baseline,
// so FullRam checks need nothing beyond the user's ISO. check_fn restores it
// before every FullRam case.
void set_baseline_current();
void restore_baseline();

inline uint64_t fbits(float f) { uint32_t u; std::memcpy(&u, &f, 4); return u; }

inline bool same_bits(float a, float b) {
    uint32_t x, y;
    std::memcpy(&x, &a, 4);
    std::memcpy(&y, &b, 4);
    return x == y;
}

}  // namespace rac1::oracle

#endif
