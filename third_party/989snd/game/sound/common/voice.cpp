// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#include "voice.h"

#include <array>

namespace snd {
#include "interp_table.inc"

// Integer math version of ps-adpcm coefs
static constexpr std::array<std::array<s16, 2>, 5> adpcm_coefs = {{
    {0, 0},
    {60, 0},
    {115, -52},
    {98, -55},
    {122, -60},
}};

void Voice::DecodeSamples() {
  if (mSample == nullptr) {
    while (mDecodeBuf.Size() < 16) {
      for (int i = 0; i < 4; i++)
        mDecodeBuf.Push(0);
    }
    return;
  }

  while (mDecodeBuf.Size() < 16) {
    // Skip decoding for stopped voices.
    if (mADSR.GetPhase() == ADSR::Phase::Stopped) {
      for (int i = 0; i < 4; i++)
        mDecodeBuf.Push(0);
    } else {
      u32 data = mSample[mNAX];
      u8 shift = mCurHeader.Shift.get();
      if (shift > 12)
        shift = 9;
      u8 filter = std::min<u8>(mCurHeader.Filter.get(), 4);

      for (int i = 0; i < 4; i++) {
        s32 sample = (s16)((data & 0xF) << 12);
        sample >>= shift;

        sample += (adpcm_coefs[filter][0] * mDecodeHist1) >> 6;
        sample += (adpcm_coefs[filter][1] * mDecodeHist2) >> 6;

        sample = std::clamp<s32>(sample, INT16_MIN, INT16_MAX);

        mDecodeHist2 = mDecodeHist1;
        mDecodeHist1 = static_cast<s16>(sample);

        mDecodeBuf.Push(static_cast<s16>(sample));
        data >>= 4;
      }
    }

    mNAX++;

    if ((mNAX & 0x7) == 0) {
      if (mCurHeader.LoopEnd.get()) {
        mNAX = mLSA;
        mENDX = true;

        if (!mCurHeader.LoopRepeat.get()) {
          if (!mNoise)
            mADSR.Stop();
        }
      }

      UpdateBlockHeader();

      mNAX++;
    }
  }
}

// rac1 (host): DecodeSamples without the samples: the same pushes (their values not made), the same position, loop and
// end. (The ADPCM history is left as it was: only the samples read it.)
void Voice::DecodeState() {
  if (mSample == nullptr) {
    while (mDecodeBuf.Size() < 16) {
      mDecodeBuf.PushSkip(4);
    }
    return;
  }

  while (mDecodeBuf.Size() < 16) {
    mDecodeBuf.PushSkip(4);
    mNAX++;

    if ((mNAX & 0x7) == 0) {
      if (mCurHeader.LoopEnd.get()) {
        mNAX = mLSA;
        mENDX = true;

        if (!mCurHeader.LoopRepeat.get()) {
          if (!mNoise)
            mADSR.Stop();
        }
      }

      UpdateBlockHeader();

      mNAX++;
    }
  }
}

void Voice::UpdateBlockHeader() {
  if (mSample == nullptr)
    return;
  mCurHeader.bits = mSample[mNAX & ~0x7];
  if (mCurHeader.LoopStart.get() && !mCustomLoop)
    mLSA = mNAX & ~0x7;
}

s16 Voice::StepNoise() {
  static constexpr std::array<u8, 64> noise_add = {
      1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0,
      1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0,
      0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
      0, 1, 1, 0, 1, 0, 0, 1, 0, 1, 1, 0, 1, 0, 0, 1,
  };
  static constexpr std::array<u8, 5> noise_freq_add = {0, 84, 140, 180, 210};

  u32 step = std::min<u32>(mNoiseClock >> 2, 15);
  u32 shift = mNoiseClock & 3;
  u32 level = std::max<u32>(0x8000u >> step, 1u);

  mNoiseCount += 0x10000;
  mNoiseCount += noise_freq_add[shift];
  if ((mNoiseCount & 0xFFFF) >= noise_freq_add[4]) {
    mNoiseCount += 0x10000;
    mNoiseCount -= noise_freq_add[shift];
  }
  if ((static_cast<u32>(mNoiseCount) >> 16) >= level) {
    mNoiseCount &= 0xFFFF;
    u32 bit = (mNoiseLevel & 1) ^ noise_add[(mNoiseLevel >> 10) & 0x3F];
    mNoiseLevel = static_cast<u16>((mNoiseLevel << 1) | bit);
  }
  return static_cast<s16>(mNoiseLevel);
}

static s16 ApplyVolume(s16 sample, s32 volume) {
  return (sample * volume) >> 15;
}

void Voice::KeyOn() {
  if (!mCustomLoop) {
    mLSA = mSSA;
  }
  mNAX = mSSA;
  mNAX++;

  UpdateBlockHeader();

  mENDX = false;
  mADSR.Attack();
  mVolume.Snap();
  mCounter = 0;
  mDecodeHist1 = 0;
  mDecodeHist2 = 0;
  mDecodeBuf.Reset();
  mCustomLoop = false;
  // Console.WriteLn("SPU[%d]:VOICE[%d] Key On, SSA %08x", m_SPU.m_Id, m_Id, m_SSA);
}

void Voice::KeyOff() {
  mADSR.Release();
  // fmt::print("Key Off\n");
}

s16Output Voice::Run() {
  s16 sample = 0;
  if (mNoise) {
    sample = StepNoise();
  } else {
    DecodeSamples();

    u32 index = (mCounter & 0x0FF0) >> 4;

    s32 acc = 0;
    acc += (mDecodeBuf.Peek(0) * interp_table[index][0]) >> 15;
    acc += (mDecodeBuf.Peek(1) * interp_table[index][1]) >> 15;
    acc += (mDecodeBuf.Peek(2) * interp_table[index][2]) >> 15;
    acc += (mDecodeBuf.Peek(3) * interp_table[index][3]) >> 15;
    sample = static_cast<s16>(std::clamp<s32>(acc, INT16_MIN, INT16_MAX));

    s32 step = mPitch;
    step = std::min(step, 0x3FFF);
    mCounter += step;

    auto steps = mCounter >> 12;
    mCounter &= 0xFFF;

    while (steps > 0) {
      steps--;
      mDecodeBuf.Pop();
    }
  }

  sample = ApplyVolume(sample, mADSR.Level());
  mLastSample = sample;
  s16 left = ApplyVolume(sample, mVolume.left.GetCurrent());
  s16 right = ApplyVolume(sample, mVolume.right.GetCurrent());

  mADSR.Run();
  mVolume.Run();

  return s16Output{left, right};
}
// rac1 (host): Run's state for `n` samples (Synth::TickState): the noise's and the samples' values not made, nor the
// last sample (the 5.1 / 7.1 placement's), nor the volumes' ramps (nothing but the samples reads them)
bool Voice::RunState(int n) {
  for (int i = 0; i < n; i++) {
    if (Dead()) {
      return false;
    }
    if (!mNoise) {
      DecodeState();
      s32 step = mPitch;
      step = std::min(step, 0x3FFF);
      mCounter += step;
      mDecodeBuf.PopSkip(mCounter >> 12);
      mCounter &= 0xFFF;
    }
    mADSR.Run();
  }
  return true;
}
}  // namespace snd
