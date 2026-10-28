// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#pragma once
#include <forward_list>
#include <array>
#include <memory>
#include <unordered_map>
#include <vector>

#include "envelope.h"
#include "sound_types.h"
#include "voice.h"

#include "common/common_types.h"

namespace snd {
struct SpuVolume {
  /*   0 */ s16 left;
  /*   2 */ s16 right;
};

class Synth {
 public:
  Synth() {
    mGroupRange.fill({0, 47});
    mVolume.left.Set(0x3FFF);
    mVolume.right.Set(0x3FFF);
    mVolume.Snap();
  }

  // rac1: `multi` (host): also the voices placed on `channels` speakers (6: 5.1, 8: 7.1, the WAVE order FL FR FC LFE
  // then 5.1's SL SR / 7.1's BL BR SL SR), one frame of s16; the stereo result unchanged
  s16Output Tick(s16* multi = nullptr, int channels = 2);
  // rac1 (host): `samples` Ticks' state alone, no output (RAC1_IOP_STATE_ONLY: the game's IOP while the host's mirror
  // plays the sound): the voices' positions, flags and envelopes, each dead voice dropped at the sample Tick drops it
  // (the handlers see a voice end by its weak_ptr expiring). Nothing may start a voice in between: Player::TickState
  // calls it between the handlers' ticks.
  void TickState(int samples);
  void AddVoice(std::shared_ptr<Voice> voice);
  // rac1: the IRX's voice allocation (0x14d60) for the cores: the lowest free voice of the group's range (0x4e
  // snd_SetGroupVoiceRange, 0x0d snd_SetMixerMode), when none the one it would steal (the lowest priority, the
  // oldest); the voice's core from it. Nothing is stolen here (the host has no voice limit).
  void AllocVoice(Voice& voice, int group, int priority);
  void SetGroupVoiceRange(int group, int lo, int hi);
  void SetMixerMode(int channel_mode, int reverb_mode);
  void SetMono(bool mono) { mMono = mono; }
  void SetMasterVol(u32 volume);
  void SetReverbMode(s32 mode, s32 delay = 0, s32 feedback = 0);
  void SetReverbDepth(s16 left, s16 right);
  void AutoReverb(s16 depth, s32 delta_time, s32 channel_flags);

 private:
  void StepReverb(s32 revInL, s32 revInR);

  std::forward_list<std::shared_ptr<Voice>> mVoices;

  // rac1: the SPU2's 48 voices (the allocator's view), the groups' voice ranges (0x19848), core 0's output into core
  // 1's reverb (0x0d's second argument: core 1's MMIX SINEL / SINER, 0x149e8)
  std::array<std::weak_ptr<Voice>, 48> mSlots{};
  std::array<std::pair<s8, s8>, 8> mGroupRange{};
  bool mCore0Reverb{false};
  u32 mAllocTime{0};
  bool mMono{false};  // the game's mono (0x0b): every voice on the front left / right

  VolumePair mVolume{};
  VolumePair mReverbDepth{};
  s32 mReverbMode{0};
  s32 mReverbDelay{0};
  s32 mReverbFeedback{0};
  u32 mReverbPos{0};
  u8 mReverbPhase{0};
  s16 mReverbOutL{0};
  s16 mReverbOutR{0};
  std::vector<s16> mReverbBuf{};
};
}  // namespace snd
