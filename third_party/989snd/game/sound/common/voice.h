// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#pragma once
#include "bitfield.h"
#include "envelope.h"
#include "fifo.h"
#include "sound_types.h"

#include "common/common_types.h"

namespace snd {

class Voice {
 public:
  enum class AllocationType {
    Managed,
    Permanent,
  };

  Voice(AllocationType alloc = AllocationType::Managed) : mAlloc(alloc) {}
  s16Output Run();
  // rac1 (host): `n` samples of Run's state alone, no sample made (the game's IOP while the host's mirror plays the sound,
  // RAC1_IOP_STATE_ONLY): the decode position, the block headers' loop / end flags and the envelope as Run leaves them
  // (the volumes' ramps not run: only the samples read them); false when the voice is found dead before one of them
  // (Synth::Tick then drops it at that sample)
  bool RunState(int n);

  void KeyOn();

  void KeyOff();

  bool Dead() {
    if (mAlloc == AllocationType::Permanent) {
      return false;
    }

    return mADSR.GetPhase() == ADSR::Phase::Stopped;
  }

  void SetPitch(u16 reg) {
    // fmt::print("VOICE[{}] PITCH WRITE {:x}\n", m_channel, reg);
    mPitch = reg;
  }
  void SetAsdr1(u16 reg) {
    // fmt::print("VOICE[{}] ADSR1 WRITE {:x}\n", m_channel, reg);
    mADSR.m_Reg.lo.set(reg);
  }
  void SetAsdr2(u16 reg) {
    // fmt::print("VOICE[{}] ADSR2 WRITE {:x}\n", m_channel, reg);
    mADSR.m_Reg.hi.set(reg);
  }
  void SetVolume(u16 left, u16 right) {
    // fmt::print("VOICE[{}] VOLL WRITE {:x}\n", m_channel, left);
    // fmt::print("VOICE[{}] VOLR WRITE {:x}\n", m_channel, right);
    mVolume.left.Set(left);
    mVolume.right.Set(right);
  }

  void SetVolumeL(u16 vol) { mVolume.left.Set(vol); }

  void SetVolumeR(u16 vol) { mVolume.right.Set(vol); }

  void SetNoise(bool noise, u8 clock = 0) {
    mNoise = noise;
    mNoiseClock = clock & 0x3F;
  }

  void SetReverb(bool reverb, bool reverbOnly = false) {
    mReverb = reverb;
    mReverbOnly = reverbOnly;
  }

  [[nodiscard]] bool IsReverb() const { return mReverb; }
  [[nodiscard]] bool IsReverbOnly() const { return mReverbOnly; }

  // rac1: the SPU2 voice this voice would have (Synth::AllocVoice: the group's voice range, the lowest free one), and
  // so its core (voices 0..23 core 0, 24..47 core 1); -1 / core 1 until allocated
  void SetSlot(s8 slot) {
    mSlot = slot;
    mCore = slot >= 24 ? 1 : 0;
  }
  [[nodiscard]] s8 Slot() const { return mSlot; }
  [[nodiscard]] u8 Core() const { return mCore; }
  s8 mPriority{0};  // rac1: the tone's priority (the allocator's steal), and its start time
  u32 mStartTime{0};

  // rac1 (host): where the voice goes in a 5.1 / 7.1 output (Synth::Tick's speakers; nothing of the stereo mix):
  // Front its left / right on the front left / right (the stereo mix's), Positional its sound's direction (989snd's
  // pan in degrees: 0 ahead, 90 right, 180 behind) around the listener, Centre the centre speaker (the dialogue; a pan
  // spreads it towards the front left / right)
  enum class Place : u8 { Front, Positional, Centre };
  void SetPlace(Place p, s32 pan) {
    while (pan > 180) pan -= 360;
    while (pan <= -180) pan += 360;
    mPlace = p;
    mPan = static_cast<s16>(pan);
  }
  [[nodiscard]] Place GetPlace() const { return mPlace; }
  [[nodiscard]] s16 GetPan() const { return mPan; }
  [[nodiscard]] s16 LastSample() const { return mLastSample; }
  // the speaker gains for a pan, kept until the pan or the layout changes (Synth::Tick)
  float mSpk[8]{};
  s16 mSpkPan{0x7fff};
  u8 mSpkLayout{0};
  Place mSpkPlace{Place::Front};

  s16 GetEnvx() { return mADSR.Level(); }

  void SetSample(u16* sample) {
    mSample = sample;
    mSSA = 0;
  }

  u32 GetNax() { return mNAX; }

  void SetSsa(u32 addr) { mSSA = addr; }

  void SetLsa(u32 addr) {
    mLSA = addr;
    mCustomLoop = true;
  }

  void Stop() { mADSR.Stop(); }

 private:
  union ADPCMHeader {
    u16 bits;
    bitfield<u16, bool, 10, 1> LoopStart;
    bitfield<u16, bool, 9, 1> LoopRepeat;
    bitfield<u16, bool, 8, 1> LoopEnd;
    bitfield<u16, u8, 4, 3> Filter;
    bitfield<u16, u8, 0, 4> Shift;
  };

  AllocationType mAlloc;
  bool mNoise{false};
  u8 mNoiseClock{0};
  u16 mNoiseLevel{0x4D75};
  s32 mNoiseCount{0};
  bool mReverb{false};
  bool mReverbOnly{false};
  bool mENDX{false};
  s8 mSlot{-1};
  u8 mCore{1};
  Place mPlace{Place::Front};
  s16 mPan{0};
  s16 mLastSample{0};

  void DecodeSamples();
  void DecodeState();  // rac1: DecodeSamples' positions and flags alone
  void UpdateBlockHeader();
  s16 StepNoise();

  fifo<s16, 0x20> mDecodeBuf{};
  s16 mDecodeHist1{0};
  s16 mDecodeHist2{0};
  u32 mCounter{0};

  u16 mPitch{0};

  u16* mSample{nullptr};
  u32 mSSA{0};
  u32 mNAX{0};
  u32 mLSA{0};
  bool mCustomLoop{false};

  ADPCMHeader mCurHeader{};

  ADSR mADSR{};
  VolumePair mVolume{};
};
}  // namespace snd
