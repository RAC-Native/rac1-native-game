// The movie hardware the game's player drives: the PS2 decodes a movie's MPEG-2
// video on its IPU (VLC / IDCT / colour conversion, with libmpeg / libipu doing the motion
// compensation on the EE) and plays its sound on the SPU2 (ADPCM streamed by the IOP). The host
// stands in for both, at the level the game's player uses them: a PSS stream (an MPEG-2 program
// stream: video 0xe0, the sound tracks in private stream 1 with Sony's "SShd" / "SSbd" headers)
// read from the user's disc, its pictures decoded to RGBA (the IPU's colour-converted output) and
// its sound track decoded to 48 kHz stereo for the host's audio device.
//
// The MPEG-2 decoding uses FFmpeg's libavcodec when CMake found it (optional, system library:
// RAC1_HAVE_AVCODEC); without it movie_hw_open() fails and the game's player sees a movie that
// cannot be opened (its "Can't Open movie" path). The demux, the sound decoding (SPU2 ADPCM) and
// the colour conversion are this file's own code.
#pragma once

#include <cstdint>
#include <vector>

namespace rac1 {

// True when the host can decode MPEG-2 video (built with libavcodec) and movies are on.
bool movie_hw_available();
// Movies on / off (RAC1_NO_MOVIES, the oracle's tests: a movie then cannot be opened).
void movie_hw_enable(bool on);
bool movie_hw_enabled();

// Opens a movie: `bytes` bytes of PSS from sector `lba` of the disc; `track` the sound track to
// play (the PSS of R&C1 carry one per language: 0 English, 2 French, 3 German, 4 Spanish, 5
// Italian; a missing track falls back to the first). False if it cannot be read or decoded.
bool movie_hw_open(uint32_t lba, uint32_t bytes, int track);
bool movie_hw_is_open();
// The pictures' size and rate (frames per second).
int movie_hw_width();
int movie_hw_height();
double movie_hw_fps();

// Decodes the next picture into `rgba` (width * height * 4, row 0 at the top). False at the end of
// the stream.
bool movie_hw_next_picture(std::vector<uint8_t>& rgba);
// True when every picture has been taken.
bool movie_hw_video_ended();

// A host setting, not the original's: a factor on the movie's sound (1 = as the IRX's voices make it, the default;
// the debug menu's Audio tab, RAC1_MOVIE_VOLUME). The PSS soundtracks are mastered louder than the game's own sounds.
extern float g_movie_volume;

// The sound, interleaved 48 kHz stereo s16, in two halves (ADR 0013: also for the host's IOP mirror): the next `frames`
// raw samples of the track taken (the frames there were, 0 when no movie), then added to `out` (added, clamped), scaled
// by the SPU2 voices' volumes (`gain_left` / `gain_right`: the left and right channels' voices, snd989_movie_gain, as the
// IRX sets them) times g_movie_volume.
int movie_hw_audio_take(int16_t* dst, int frames);
void movie_hw_audio_add(int16_t* out, const int16_t* src, int frames, float gain_left, float gain_right);

void movie_hw_close();

// The picture on screen (the player's display: the PS2 uploads it to the GS frame buffer at a
// vblank): set by the player, drawn by the host over the frame while a movie runs. Null when
// no movie picture is shown.
void movie_hw_show(const std::vector<uint8_t>& rgba, int w, int h);
const std::vector<uint8_t>* movie_hw_shown(int& w, int& h);
void movie_hw_show_clear();

// The SPU2's ADPCM: one 16-byte block (shift / filter, flags, 14 bytes of 4-bit samples) to 28
// samples, with the channel's two previous samples.
void spu2_adpcm_block(const uint8_t* block, int16_t* out, int32_t& h1, int32_t& h2);

}  // namespace rac1
