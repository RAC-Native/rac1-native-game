// The IOP side of 989snd, on the host: the command server (0x123456) and the bank loader
// (0x123457) the EE library (src/game/lib/snd989) calls by SIF RPC, run on OpenGOAL's 989snd
// reimplementation (third_party/989snd).
//
// Two copies of it run (ADR 0013):
// - the game's IOP: the one the EE talks to. Its replies and its state (a sound still playing, a stream's time left)
// are what the game reads; it advances 1/60 s at each of the game's vsyncs, on the game's thread.
// - the host's mirror: a second 989snd fed with the same commands and bank loads, in the same order, at the same
//   vsyncs, on the host's audio thread. Its output is what is heard. When the game's thread stalls (a load), the mirror
//   goes on alone, as the PS2's IOP, a processor of its own, played the music and the sounds already started while the
//   EE was busy: the game's IOP, and what the game reads, stays on the game's vsyncs. The mirror writes nothing the game
//   reads.
#ifndef RAC1_RUNTIME_IOP_SND989_IOP_H
#define RAC1_RUNTIME_IOP_SND989_IOP_H

#include <cstdint>
#include <string>

namespace rac1 {

void snd989_iop_start();  // the servers registered with the SIF RPC layer
// The calling thread runs the game's IOP (the main thread; the game thread, runtime/ee/game_thread.h): its sound's random
// numbers are the game's one sequence, whatever thread runs a game frame (the mirror's thread keeps its own).
void snd989_rand_use_game();
void snd989_iop_stop();   // every sound stopped and bank unloaded
// The game's IOP's output (stereo, 48 kHz, 16 bit), `frames` frames; silence before the start. `multi` (host): also the
// output on 5.1 / 7.1 speakers (`channels` 6 / 8 per frame, the WAVE order FL FR FC LFE, then SL SR / BL BR SL SR; the
// voices placed by Synth::Tick), else nullptr.
void snd989_iop_render(int16_t* out, int frames, int16_t* multi = nullptr, int channels = 2);
// The movie's samples (as movie_hw_audio_add) added to a 5.1 / 7.1 output's front left / right.
void snd989_movie_add_multi(int16_t* multi, int channels, const int16_t* movie, int movie_frames, float gl, float gr);

// Host knobs (debug menu): the output's gain (0 mutes), the command trace (RAC1_SND_TRACE),
// and the voices playing.
extern float g_snd989_gain;
extern bool g_snd989_trace;
int snd989_iop_active_sounds();

// The host's mirror (see above). Enabled before the first command (snd989_iop_start), when an audio device plays it.
void snd989_mirror_enable();
void snd989_mirror_disable();  // (no device after all: nothing recorded any more, the queue dropped)
bool snd989_mirror_enabled();
// The game's thread, at each vsync after the game's IOP rendered its 800 frames: the commands recorded since the last
// vsync closed by this one. `movie`: the movie's raw samples of these frames (`movie_frames` stereo frames, or none), with
// the gains the game's mix used (movie_hw_audio_add); `game_pcm`: the game's IOP's mix (800 frames), kept only for
// RAC1_IOP_VERIFY.
void snd989_mirror_vsync(const int16_t* movie, int movie_frames, float movie_gl, float movie_gr, const int16_t* game_pcm);
// The audio thread: the vsyncs queued.
int snd989_mirror_queued();
// The audio thread: wait (at most `ms`) for a vsync to be queued; true when one is.
bool snd989_mirror_wait(double ms);
// The audio thread: the commands up to the next queued vsync applied and its 800 frames rendered into `out`; false
// (nothing done) when no vsync is queued.
// `multi`: also the 5.1 / 7.1 output (snd989_iop_render), else nullptr.
bool snd989_mirror_next(int16_t* out, int16_t* multi = nullptr, int channels = 2);
// The audio thread: the game's thread is stalled. The commands it already sent applied, then 800 frames rendered by the
// mirror alone, its streams' reader run after them (the IRX's reader reads on whatever the EE does).
void snd989_mirror_alone(int16_t* out, int16_t* multi = nullptr, int channels = 2);
// The mirror's counters, printed at exit: vsyncs rendered, frames rendered alone, (RAC1_IOP_VERIFY) vsyncs equal / not
// equal to the game's mix before the first frame alone.
void snd989_mirror_report();
// (testing) the game's IOP's live streams, or the mirror's, as text (snd989_stream_describe)
std::string snd989_streams_describe(bool of_mirror);

}  // namespace rac1

#endif
