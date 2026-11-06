# 989snd (OpenGOAL)

The IOP sound driver 989snd reimplemented by the OpenGOAL project
(<https://github.com/open-goal/jak-project>, `game/sound/989snd` and `game/sound/common`,
commit efb21c3e8c5a40a74f2e86510f915da55eaeba34), ISC license (see `LICENSE`). Ratchet &
Clank's banks are the same 989snd format (SBlk version 1 sound blocks, the samples in SPU2
ADPCM). The host's 989snd servers (`runtime/iop/snd989_iop.cpp`) run on it.

Changes from upstream:

- `player.h` / `player.cpp`: the cubeb output stream removed; `Tick` is public, the host's
  audio device pulls the output. `SetSoundPBend` added (the handler's pitch bend), `ActiveSounds` (a debug count).
- `sfxblock.h` / `sfxblock.cpp`, `sfxgrain.cpp`, `musicbank.cpp`: a sound id past the bank's
  sounds plays nothing (reported once) instead of indexing past `Sounds`, as the IRX's
  `snd_PlaySFX` does (`sound < 0 || sound >= count`: handle 0). The game plays such ids: on
  Quartu the hero's low-air beep is sound -1.
- `common/common_types.h`: only the integer types kept.
- `common/synth.h` / `common/synth.cpp`: the master volume read as the SPU2 does (the register's level,
  `0x3fff` = 1.0; upstream multiplied by the raw register, -6 dB); the voices summed in 32 bits and clamped
  once at the end, as the SPU2 mixes (upstream saturated after each voice); an SPU2 reverb (the effect area's
  algorithm at 24 kHz, the SDK's ten presets, ECHO / DELAY's delay and feedback) fed by the voices whose tone
  has the reverb flags, with `SetReverbMode` / `SetReverbDepth` / `AutoReverb`.
- `common/voice.h` / `common/voice.cpp`: the SPU2 noise generator for noise tones (upstream skipped them),
  reverb / reverb-only flags, the ADPCM decoder refilled until its FIFO holds 16 samples (high pitches no
  longer starve it), invalid shift (13..15 as 9) and filter values handled, the interpolation summed in 32
  bits, the loop address reset to the start address at key on.
- `common/envelope.h` / `common/envelope.cpp`: a fixed (non-sweep) volume change ramped over 400 samples
  (host smoothing), snapped at key on.
- `989snd/vagvoice.cpp`: reverb-only tones (flag 0x10) played instead of throwing; the voice's reverb flags
  set from the tone; a group's master volume re-applied only when it changes.
- `989snd/sfxgrain.cpp`: noise tones (flag 8) start a noise voice (the clock from the tone's centre note).
- `989snd/blocksound_handler.*`, `sound_handler.h`, `player.*`: `AutoPan` (the IRX's `snd_AutoPan`: the
  pan moved over handler ticks, the shortest way or in a direction), the voices' base volume kept current
  for the group volumes; `Player::AutoPan`, `SetReverbMode`, `SetReverbDepth`, `AutoReverb`; the master
  volume of group 16 scaled to `0x3fff` (upstream `0x3ffff` overflowed the register).
- `common/synth.*`, `common/voice.*`, `989snd/vagvoice.cpp`, `player.h`, (rac1) the SPU2's cores: `Synth::AllocVoice`
  gives each voice the SPU2 voice the IRX's allocator would (the group's range from `SetMixerMode` / `SetGroupVoiceRange`,
  the lowest free voice; nothing stolen), so its core; with the mixer mode, core 0's output (clamped to 16 bits) feeds
  core 1's reverb. (host) `Synth::Tick` / `Player::Tick` also render the voices on 5.1 / 7.1 speakers (`multi`): a
  voice's `Place` (front left / right, positional by its pan, or centre) set by `blocksound_handler.*` / `sfxgrain.cpp`
  (a sound played or moved with a pan is positional) and the host's streams; the stereo output unchanged.
- `common/voice.*`, `common/synth.*`, `989snd/player.*`, (rac1, host) the high-precision mix: `Voice::Run`'s `hp`, `Synth::Tick`'s
  and `Player::Tick`'s `hp` / `hpMulti` give the same output in float (the voice's products exact, nothing clamped, the
  reverb a second time in float on its own effect area: `StepReverb`'s `hpIn`); the voices' state and the s16 output
  are the same with or without them.
- Not taken: `fakeplayer`, `sndplay` (a command-line player), `common/flava.h` (Jak 3).
- Shims for what the code used from the rest of OpenGOAL: `common/util/Assert.h`,
  `common/log/log.h`, `fmt/format.h` (the few `fmt::format` / `fmt::print` calls),
  `third-party/magic_enum.hpp` (`enum_name` in a debug print). `common/util/BinaryReader.h` is
  OpenGOAL's.
