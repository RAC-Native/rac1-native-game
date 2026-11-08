// src/profiler/perf_switch.h — what the RAC1_PERF_* switches share (docs/performance.md, docs/env-vars.md): a switch's value, and the
// account of a dual mode (=2 by convention: both ways run, the faster one's result compared with the old one's field by field, the
// old one's used), printed at the end of a run. Host tooling, not a port.
#pragma once

#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <string>
#include <vector>

namespace rac1::perf {

// A switch's value: the environment variable's number, `def` when it is not set (read once by the caller: a function-local static).
inline int switch_value(const char* name, int def) {
    const char* e = std::getenv(name);
    return e == nullptr || *e == '\0' ? def : std::atoi(e);
}

// A dual mode's account: the comparisons made, those that found a difference, the first differences described. One thread at a
// time (the caller's).
struct DualCheck {
    uint64_t compared = 0, differing = 0;
    std::vector<std::string> notes;
    size_t max_notes = 10;
    // one comparison: true when `same`; else counted and `why` noted
    bool check(bool same, const std::string& why = {}) {
        ++compared;
        if (same) return true;
        ++differing;
        if (notes.size() < max_notes && !why.empty()) notes.push_back(why);
        return false;
    }
    // "[perf] <what> check: N <unit> compared, M differing" and the notes, one a line
    void report(const char* what, const char* unit) const {
        std::printf("[perf] %s check: %llu %s compared, %llu differing\n", what, static_cast<unsigned long long>(compared), unit, static_cast<unsigned long long>(differing));
        for (const std::string& n : notes) std::printf("[perf] %s check: %s\n", what, n.c_str());
    }
};

}  // namespace rac1::perf
