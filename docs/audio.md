# Audio

The game's sound runs through Sony's 989snd sound system: a client library on the EE that sends
commands, and a driver on the IOP, the PS2's I/O processor, that mixes the voices. The port keeps the
game's client code (ported), runs the IOP side with OpenGOAL's reimplementation of the 989snd driver,
and plays the result with miniaudio.

```mermaid
flowchart LR
    subgraph GT["Game thread"]
        G["Game code: sounds, music,<br/>dialogue (ported)"] --> C["989snd EE client<br/>src/game/lib/snd989.cpp"]
        C -->|"SIF RPC 0x123456 / 0x123457"| IOP["The game's IOP<br/>runtime/iop/snd989_iop.cpp<br/>800 samples per vsync"]
        IOP -->|"replies, state"| C
    end
    IOP -->|"record: commands, banks, vsyncs"| MIR
    subgraph AT["Audio threads"]
        MIR["The mirror<br/>(a second 989snd player)"] --> Q["Queue<br/>(2, 6 or 8 channels)"] --> DEV["miniaudio device<br/>src/host/audio_out.cpp"]
    end
```

## The EE side

The game never touches sound hardware. Its code calls the 989snd client (`src/game/lib/snd989.cpp`):
`snd_play_sound` (0x12e308), `snd_stop_sound` (0x12e368), `snd_set_sound` (0x12e4c0),
`snd_set_master_volume` (0x12e208), `snd_stream_play` (0x12ec08) and others. These append commands to
one of two 4 KB command buffers at 0x15eca0, sent in batches (`snd_batch_send`, 0x12e9d8) through the
SIF, the EE-to-IOP remote procedure call mechanism. `snd_init` (0x12da28) binds the two servers: the
command server 0x123456 and the bank loader 0x123457.

The game's music and dialogue logic is ported too: `src/game/ovl/music_streams.cpp` changes music
tracks with fades (`music_streams_frame`, 0x265aa8), starts music (`music_start`) and plays dialogue
lines (`dialog_play`).

## The IOP side

`runtime/sif/sif_rpc.cpp` runs the SIF calls synchronously against servers written on the host side.
`runtime/iop/snd989_iop.cpp` registers the two sound servers and runs them on **OpenGOAL's 989snd**
(`third_party/989snd`, from the [jak-project](https://github.com/open-goal/jak-project), ISC licence,
adapted as listed in its README). It loads the level's sound banks into IOP memory, plays, stops and
changes sounds, and answers the game's queries ("is this sound still playing?"). As in the PS2's
driver, a sound id the bank does not have plays nothing (the game asks for some: on Quartu the
hero's low-air beep is sound -1); the first time an id is refused it is reported on the console.
`runtime/iop/snd989_stream.cpp` plays the streamed music and dialogue, read from the disc.

**Reverb.** Each level's light zones carry a reverb setting: when the hero enters one, the game's sound
frame (`sound_frame`, 0x28cc08) sends command 0x50 (`snd_SetReverbEx`: core, type, depth, delay, feedback)
or 0x10 (`snd_AutoReverb`: core, depth, time, channels), and 0x50 with type 0 to turn it off (Veldin).
The synth runs the SPU2's reverb (the effect area's same-side / different-side reflections, four combs and
two all-pass filters, at 24 kHz) with the SDK's presets (room, studio A / B / C, hall, space, echo, delay,
pipe); its output, scaled by the depth (the SPU2's effect volume), is added to the mix. The presets
are the PS1's (as psx-spx lists them); pipe is its "half echo": an earlier table of our own fed back
on itself, a static over the whole slide down Novalis's sewer pipe.

**Which voices feed the reverb: the SPU2's two cores.** The PS2's SPU2 has two cores of 24 voices each; the
game's reverb is core 1's (0x50 / 0x10 are sent with the core mask 2), core 0's is never turned on. At its start
(`sound_system_init`, 0x28ca88) the game sends `snd_SetMixerMode(0, 1)` (command 0x0d; IRX 0x149e8): every group may
use voices 0..47, and core 1's MMIX bits 0..1 are set, so core 0's whole output goes into core 1's reverb. Then
`snd_SetGroupVoiceRange` (0x4e; IRX 0x14ad4) pins groups 1, 2 and 4 to voices 24..47 (core 1). The IRX's allocator
(0x14d60) takes the lowest free voice of the group's range (when none is free, it steals the lowest priority, the
oldest), so the other groups land on core 0 first. A core-1 voice reaches the reverb when its tone has the reverb
flag 0x1 (VMIXEL / R, 0x151cc); a core-0 voice always does, through core 0's output, whatever its flag. The synth
models this (`Synth::AllocVoice`, `SetMixerMode`, `SetGroupVoiceRange`): each voice gets the SPU2 voice the IRX would
give it, so its core (nothing is stolen: the host has no voice limit), and core 0's sum is clamped to 16 bits and
added to core 1's dry and reverb inputs. The streams' voices are allocated the same way (0xe948: their group, priority
0x7f; the reverb flag from the play command's flag 2, which the game never sets). In practice this changes nothing
heard today: every tone of every level's bank in volume group 0 (the world's sounds) carries the flag 0x1, group 4's
sixteen tones carry none but are on core 1, and the music and dialogue streams (groups 1, 2) are on core 1 without the
flag (measured on all 19 levels' banks and on Orxon's reverb zone: the same energy into the reverb, group by group,
with and without the routing; the output byte-identical). It matters only for sounds a bank would play on core 0
without the flag.

**Pan and pitch.** Command 0x21 (`snd_SetSoundParams`, 0xb428) with flag 4 moves a sound's pan with
`snd_AutoPan` (over handler ticks, the shortest way) instead of jumping, as the IRX does for the world's
positional sounds; flag 2 alone sets it at once. Command 0x51 (`snd_SetPanMode`) is accepted and does nothing.
The voices' fixed volume changes are ramped over 400 samples (8.3 ms) on the host, so the per-frame
updates do not click; pitch changes apply at once, as on the SPU2 (a ramp made sounds end a frame early,
which the game polls).

**The mix.** The voices are summed in 32 bits and the result clamped to 16 bits once, then the master
volume applied: the SPU2's own output is 16-bit, and sums past full scale clip there too.

**Timing.** Everything the game reads back from the sound must depend only on the game's own time,
not the PC's speed. So the IOP's driver advances exactly 800 samples (1/60 s at 48 kHz) each time a
vsync passes in the game (`vsync_passed` in `src/host/orig_main.cpp`), on the game's thread. The
game's state is then the same however fast or slow the host runs.

## The mirror: sound that keeps playing during loads

On a PS2 the IOP is a separate processor: when the EE is busy for a long frame (reloading a
checkpoint, starting a level, opening a movie), the music keeps playing. In the port the game's IOP
only advances when the game does, so a long stall on the game's thread would leave the audio device
with nothing to play.

The port therefore runs **two copies** of the driver
([Decisions](decisions.md#13-the-iops-sound-runs-twice), still a proposal, on by default):

- **The game's IOP** answers the game's calls and holds the state the game reads. Its output is not
  played.
- **The mirror** is a second 989snd player on the host's audio thread (`iop_loop` in
  `src/host/audio_out.cpp`). The game's IOP records every command with its reply, every bank loaded
  and every vsync; the mirror replays them in order, and what it renders is what you hear. While the
  game keeps up, the mirror produces the same samples as the game's IOP (`RAC1_IOP_VERIFY=1` compares
  them every vsync).
- When the game's thread stalls and the queue is running low, the mirror renders **alone**
  (`snd989_mirror_alone`), so the music and the sounds already playing go on. When the game resumes,
  the mirror is ahead by the stall's length, as a PS2's IOP would be after a long EE frame, and the
  host does not run those game frames again. A stall is the game's thread stuck inside one game frame
  for more than 20 ms after its last vsync (a load), or no vsync for longer than the host's usual gap
  between its frames (a long draw). A host that simply runs below 60 frames a second (the 30 FPS cap,
  VSync at 75 Hz, a heavy scene) is not stalled: it runs two or more game frames per host frame and the
  queue absorbs the gaps (its latency is not lowered below the longest recent gap). Before, any gap
  over 20 ms counted as a stall, and the game ran slower than real time at those rates (issues #4, #8,
  #22).

`RAC1_IOP_MIRROR=0` turns the mirror off and plays the game's IOP directly.

**The audio device** (`audio_out_start`): miniaudio, 16-bit stereo (or 5.1 / 7.1, below) at 48 kHz, three periods of 10 ms
by default (`RAC1_AUDIO_PERIOD_MS`, `RAC1_AUDIO_PERIODS`, `RAC1_AUDIO_DEVICE` to choose another).
The queue between the threads absorbs small differences in pace with a slight rate adjustment, and
ramps the sound in and out over 2.5 ms when it has to skip or runs dry. The device's thread and the
IOP thread ask the system for real-time priority through rtkit (`src/host/host_priority.cpp`;
`RAC1_PRIORITY=0` to not ask). `RAC1_NO_AUDIO` runs without sound.

## Surround: 5.1 and 7.1

Options > Sound's last row is the game's own Mono / Stereo switch; the port adds **5.1** and **7.1** after
Stereo (Mono, Stereo, 5.1, 7.1, then Mono again; see [Settings](settings.md#sound-51-and-71)). They are the host's:
the game's byte (0x15ede8, kept in its save, read by its code) stays at Stereo, the choice is kept in the
preferences, and nothing the game reads changes (`RAC1_RAMHASH` identical to Stereo's). The device is opened again
with 6 or 8 channels (the WAVE order: FL FR FC LFE SL SR, or FL FR FC LFE BL BR SL SR; up to 7.1 PCM, no
object audio); a device that cannot take them is opened in stereo, with an `[audio]` line saying so.

The IOP's mirror then also renders each voice on the speakers (`Synth::Tick`'s `multi`; the stereo mix is made as
before, sample for sample):

- **The world's sounds** (a sound played or moved with a pan: the game's 30 sound slots, whose pan `sound_pan`
  0x28c9e8 computes every frame from the source's position in the camera's view) are placed by that pan, which is
  an azimuth: 0 ahead of the camera, 90 right, 180 behind (the stereo mix folds behind onto ahead, the speakers do
  not). The voice's level (its left and right's power) is panned at equal power between the two speakers either side
  of its direction: front left / right at 30 degrees, in 5.1 the surrounds at 110, in 7.1 the sides at 90 and the
  backs at 150. The centre is left to the dialogue. A sound within a unit of the camera has a pan near 0 (the game
  scales it down), so it sits between the front left and right, as in stereo.
- **The other sounds** (the menus', any sound played with pan 0 and never moved), **the music**
  and **the movies' sound** stay on the front left and right, exactly their stereo mix.
- **The dialogue** (the streams of volume group 2) is on the centre speaker. A dialogue line with a pan would be
  spread from the centre towards the front left / right (equal power); the game plays every line with pan 0, in the
  levels and in the cutscenes alike (`dialog_start` and the cutscene lines of `music_streams.cpp`, flags 0x21, never a
  pan update), so the dialogue is always on the centre. The movies' dialogue is in their stereo mix (front).
- **The reverb**, a diffuse sound, is spread over its side's speakers at equal power (front and surround in 5.1;
  front, side and back in 7.1). **The LFE** is silent: the original has no LFE, and a receiver's bass management
  sends the speakers' lows to the subwoofer.
- In the game's Mono mode every voice stays on the front left / right.

The energy is kept: the channels' summed power equals the stereo mix's (measured within 0.2 % on Orxon, Batalia
and the title's movie). The cost is the IOP thread's: about 1 % of a core more with 32 voices at 7.1. With
`RAC1_AUDIO_CHANNELS=6` / `8` a headless run's `RAC1_AUDIO_DUMP` holds the 6 / 8 channels interleaved.

## Movies

The game's movies are PSS files: an MPEG-2 video stream with SPU2 ADPCM audio tracks, one per
language (English, French, German, Spanish, Italian). The game's movie player is replaced by host
code (`src/game/lib/movie_player.cpp`), which hands the file to the decoder in
`runtime/ipu/movie_hw.cpp`:

- it demuxes the PSS itself and decodes the ADPCM audio track of the game's language (falling back to
  the first track present), resampled to 48 kHz and mixed into the sound output;
- the MPEG-2 video is decoded with the system's FFmpeg (`libavcodec`).

FFmpeg is optional at build time (the `RAC1_MOVIES` CMake option, on by default, uses it when
`pkg-config` finds it). Without it, the game runs and skips the movies, taking the game's own "cannot
open movie" path. `RAC1_MOVIE_VOLUME` changes the movies' volume.
