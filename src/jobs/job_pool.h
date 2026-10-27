// src/jobs/job_pool.h — the host's job pool: work spread over the CPU's cores (openly invented tooling, ADR 0003; not a port).
//
// Only for the host's own work whose results nothing of the game reads back: the renderer's scene building from what the game
// left in memory (the moby chains' decoding, the draw lists), never the game's code, which keeps its own order on its own
// thread (docs/performance.md). Deterministic: a parallel_for's chunks depend on its size and grain only, never on the number
// of threads, so a job that writes its results per chunk and merges them in chunk order gives the same output, bit for bit,
// on one thread or on many (RAC1_JOBS=1 runs every chunk on the calling thread, for comparison).
//
// The workers run with the calling thread's float environment (its rounding mode, flush-to-zero and denormals-are-zero: the
// game's thread rounds toward zero, ps2_fpu.h), so a float computation gives the same bits on a worker as on the caller.
#pragma once

#include <cstddef>
#include <functional>

namespace rac1::jobs {

// The threads a parallel_for spreads over, the calling thread included (1: all on the caller). RAC1_JOBS=<n> sets it
// (testing); by default about half the logical CPUs (their physical cores), at most 8, less the two workers the renderer
// already has (the GS chain's, the MRP scene's).
int threads();

// Runs fn(begin, end) over [0, n) cut in chunks of `grain` items (the last one shorter), chunk c = [c * grain, ...), on the
// pool's workers and the calling thread, and returns when all are done. A chunk's index is begin / grain. An exception
// thrown by a chunk is rethrown here (the first one). Two threads calling it at once run one after the other; a chunk
// must not call parallel_for itself. With n <= grain, or one thread, fn runs on the caller alone.
void parallel_for(size_t n, size_t grain, const std::function<void(size_t, size_t)>& fn);

// The number of chunks parallel_for cuts [0, n) into.
inline size_t chunks(size_t n, size_t grain) { return grain == 0 ? 0 : (n + grain - 1) / grain; }

}  // namespace rac1::jobs
