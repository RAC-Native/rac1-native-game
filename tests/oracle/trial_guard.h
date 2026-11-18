#pragma once

#include <cstdint>

namespace rac1::oracle {

// Reject a randomly generated state that throws std::exception or faults. The
// oracle runs trials serially; this helper is not reentrant. With a time limit,
// POSIX rejects the timed-out state; Windows exits the oracle with status 124
// rather than attempting to resume a thread interrupted in arbitrary C++ code.
bool run_trial(void (*port)(uint32_t), uint32_t moby, unsigned timeout_seconds = 0);

}  // namespace rac1::oracle
