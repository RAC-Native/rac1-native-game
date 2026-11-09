// A level mod's music (docs/mods.md, "Level mods", Music): level.txt's `music <loop> [<intro>]` files, a VAG (mono, or
// stereo: ours), a WAV or an Ogg Vorbis file, made the VAG the game's music streams play
// (runtime/iop/snd989_stream.cpp). Not a port: our own tool.
//
// A stereo VAG: the 0x30-byte "VAGp" header (big endian: +0xc the size of ONE channel's data, +0x10 the rate), byte
// +0x1e the channels (2; the disc's VAGs all have 0 there), then the left and the right channel's PS-ADPCM interleaved
// in chunks of 0x800 bytes (the last chunk of each padded). The host's streams play it on two voices, the left one on
// the left and the right one on the right, in step; the stream's volume, pan, group, pause, queue are both's.
#ifndef RAC1_SETUP_LEVEL_MUSIC_H
#define RAC1_SETUP_LEVEL_MUSIC_H

#include <cstdint>
#include <string>
#include <vector>

namespace rac1::setup {

struct MusicVag {
    std::vector<uint8_t> file;  // the VAG (header and data)
    uint32_t rate = 0;
    int channels = 1;
    uint64_t samples = 0;       // a channel's
    bool encoded = false;       // made from a WAV or an Ogg (its rate can be chosen)
    std::string how;            // what was done (the composer's log): "a VAG", "a WAV (...), encoded: ...", ...
};

// `in` (a VAG, a WAV or an Ogg Vorbis file) as the music VAG; `loop`: it loops (a WAV's or an Ogg's encoding made
// seamless: its length a whole number of ADPCM blocks, its first block independent of what played before); `channels`
// 2: made stereo (a mono one on both channels), 0: as it is; `rate` (a WAV's, an Ogg's) the rate to encode at, 0 its
// own (8000 .. 48000 Hz: else the nearest of those)
bool music_vag_make(const std::vector<uint8_t>& in, bool loop, int channels, uint32_t rate, MusicVag& out, std::string& why);

// an Ogg Vorbis file (mono or stereo) decoded (third_party/stb_vorbis): its channels' samples, its rate. Every sample
// the stream's last granule position says it has, no more: a loop's length survives the round trip
bool ogg_decode(const std::vector<uint8_t>& in, std::vector<std::vector<int16_t>>& ch, uint32_t& rate, std::string& why);

}  // namespace rac1::setup

#endif
