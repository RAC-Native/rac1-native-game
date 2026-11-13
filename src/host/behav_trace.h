// Semantic per-frame behaviour trace recorder (RAC1_BEHAV_TRACE=<path>).
//
// Captures high-level gameplay, hero, moby, camera, UI, PRNG, and audio-event
// state once per completed game frame in a layout-independent binary format
// (R1BT v1). Used alongside tools/behav_trace/verify_trace.py to prove semantic
// equivalence across the 1.0 native-engine migration stages, including stages
// that retire EE memory addresses and overlay layouts.
#ifndef RAC1_HOST_BEHAV_TRACE_H
#define RAC1_HOST_BEHAV_TRACE_H

#include <cstdint>

namespace rac1 {

// Opens RAC1_BEHAV_TRACE if set in the environment and writes the R1BT v1 header.
// Safe to call unconditionally at startup; a no-op when RAC1_BEHAV_TRACE is unset.
void behav_trace_init();

// Flushes and closes the trace file at shutdown.
void behav_trace_shutdown();

// Returns true when RAC1_BEHAV_TRACE recording is active for this run.
bool behav_trace_active();

// Appends one completed game frame's semantic snapshot (and drains queued audio
// events emitted during the frame). Must be called at the game_frame_ended point.
void behav_trace_frame(uint64_t game_frame);

// Lightweight audio-event hooks invoked by the sound/stream dispatch layer.
// Ignored when tracing is inactive or while the renderer replays a frame.
void behav_trace_audio_sfx(uint32_t snd_index, int32_t vol, int32_t pan, int32_t pm, int32_t pb);
void behav_trace_audio_stream(uint32_t group, int32_t vol, int32_t pan, uint32_t queue, uint32_t flags);
void behav_trace_audio_stop_streams();

}  // namespace rac1

#endif  // RAC1_HOST_BEHAV_TRACE_H
