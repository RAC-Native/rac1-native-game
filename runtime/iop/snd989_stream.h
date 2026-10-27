// The IOP side of 989snd's VAG streams on the host (the music and the dialog): the stream
// commands of the EE library (0x2a init, 0x2c play, 0x2d pause, 0x2e continue, 0x2f stop, 0x30 /
// 0x31 / 0x32 elapsed / length / time left, 0x34 stop all, 0x36 / 0x37 the reader, 0x38 a disc
// read for the game, 0x4f started; 0x15 / 0x19 / 0x21 on a stream handle), the stream read from
// the ISO into an SPU double buffer played by an SPU voice of OpenGOAL's 989snd synth. The
// protocol and what is taken from the IRX:
#ifndef RAC1_RUNTIME_IOP_SND989_STREAM_H
#define RAC1_RUNTIME_IOP_SND989_STREAM_H

#include <cstdint>
#include <string>

namespace snd {
class Player;
}

namespace rac1 {

// One IOP's streams: the game's (`snd989_streams(false)`, the one the EE talks to), or a host mirror's
// (`snd989_streams(true)`, a new one: no writes to the EE, no log; ADR 0013).
struct Snd989Streams;
Snd989Streams* snd989_streams(bool mirror);

// A stream handle (type 4 in bits 24..28, as the IRX's handles)?
bool snd989_is_stream_handle(uint32_t h);
// The command `id` with its argument words `args` (copied from the EE's RPC buffer, at least 8) if it is a stream
// command (or one on a stream handle): true and its result in `result`. The caller holds the render lock.
bool snd989_stream_command(Snd989Streams* io, snd::Player& p, uint32_t id, const uint32_t* args, int32_t& result);
// The master volume of a group changed (command 0x09): the streams' voices follow.
void snd989_stream_master_volume(Snd989Streams* io, uint32_t group, int32_t vol);
// Pause (command 0x16) or continue (0x17) all streams whose volume group is in `mask` (bit g = group g).
void snd989_stream_group_pause(Snd989Streams* io, uint32_t mask, bool pause);
// The gains of the movie's sound (command 0x3b with volume 0x400, pan 0, group 5, mode 3; the IRX's 0xa2c0 starts the
// voices): the left channel's voice and the right channel's, as factors of the ADPCM samples (the SPU2's voice volume
// / 0x4000), by the stream volume formula with the group 5 master volume now (the game's IOP).
void snd989_movie_gain(float& left, float& right);
// The reader: the SPU halves the voices have left refilled from the disc, ended streams freed.
// Called after each command batch (every frame), with the render lock held.
void snd989_stream_update(Snd989Streams* io);
void snd989_stream_reset(Snd989Streams* io);
// (testing) the live streams as text, "group:sector" each ("p" when paused), in slot order
std::string snd989_stream_describe(Snd989Streams* io);

}  // namespace rac1

#endif
