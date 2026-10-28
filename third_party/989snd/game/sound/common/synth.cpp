// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#include "synth.h"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdlib>
#include <stdexcept>

namespace snd {

namespace {
struct ReverbPreset {
  u16 dAPF1, dAPF2;
  s16 vIIR, vCOMB1, vCOMB2, vCOMB3, vCOMB4, vWALL, vAPF1, vAPF2;
  u16 mLSAME, mRSAME, mLCOMB1, mRCOMB1, mLCOMB2, mRCOMB2, dLSAME, dRSAME;
  u16 mLDIFF, mRDIFF, mLCOMB3, mRCOMB3, mLCOMB4, mRCOMB4, dLDIFF, dRDIFF;
  u16 mLAPF1, mRAPF1, mLAPF2, mRAPF2;
  s16 vLIN, vRIN;
};

// rac1: the SDK's presets (libsd's sceSdSetEffectAttr modes) are the PS1's (psx-spx "Reverb Examples"): studio A / B /
// C the PS1's studio small / medium / large, space its space echo, pipe its half echo. Pipe was a made-up table that
// fed back on itself (a constant loud static on Novalis' sewer slide); studio A / C's vWALL had a typo. Still not
// libsd's: room's different-side and comb 3 / 4 addresses (zero here), echo / delay.
constexpr std::array<ReverbPreset, 10> kReverbPresets = {{
    // 0: OFF
    {},
    // 1: ROOM
    {0x007D, 0x005B, 0x6D80, 0x54B8, -0x4130, 0x0000, 0x0000, -0x4580, 0x5800, 0x5300, 0x04D6,
     0x0333, 0x03F0, 0x0227, 0x0374, 0x01EF, 0x0334, 0x01B5, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x01B4, 0x0136, 0x00B8, 0x005C, -0x8000, -0x8000},
    // 2: STUDIO_A
    {0x0033, 0x0025, 0x70F0, 0x4FA8, -0x4320, 0x4410, -0x3F10, -0x6400, 0x5280, 0x4EC0, 0x03E4,
     0x031B, 0x03A4, 0x02AF, 0x0372, 0x0266, 0x031C, 0x025D, 0x025C, 0x018E, 0x022F, 0x0135,
     0x01D2, 0x00B7, 0x018F, 0x00B5, 0x00B4, 0x0080, 0x004C, 0x0026, -0x8000, -0x8000},
    // 3: STUDIO_B
    {0x00B1, 0x007F, 0x70F0, 0x4FA8, -0x4320, 0x4510, -0x4110, -0x4B40, 0x5280, 0x4EC0, 0x0904,
     0x076B, 0x0824, 0x065F, 0x07A2, 0x0616, 0x076C, 0x05ED, 0x05EC, 0x042E, 0x050F, 0x0305,
     0x0462, 0x02B7, 0x042F, 0x0265, 0x0264, 0x01B2, 0x0100, 0x0080, -0x8000, -0x8000},
    // 4: STUDIO_C
    {0x00E3, 0x00A9, 0x6F60, 0x4FA8, -0x4320, 0x4510, -0x4110, -0x5980, 0x5680, 0x52C0, 0x0DFB,
     0x0B58, 0x0D09, 0x0A3C, 0x0BD9, 0x0973, 0x0B59, 0x08DA, 0x08D9, 0x05E9, 0x07EC, 0x04B0,
     0x06EF, 0x03D2, 0x05EA, 0x031D, 0x031C, 0x0238, 0x0154, 0x00AA, -0x8000, -0x8000},
    // 5: HALL
    {0x01A5, 0x0139, 0x6000, 0x5000, 0x4C00, -0x4800, -0x4400, -0x4000, 0x6000, 0x5C00, 0x15BA,
     0x11BB, 0x14C2, 0x10BD, 0x11BC, 0x0DC1, 0x11C0, 0x0DC3, 0x0DC0, 0x09C1, 0x0BC4, 0x07C1,
     0x0A00, 0x06CD, 0x09C2, 0x05C1, 0x05C0, 0x041A, 0x0274, 0x013A, -0x8000, -0x8000},
    // 6: SPACE
    {0x033D, 0x0231, 0x7E00, 0x5000, -0x4C00, -0x5000, 0x4C00, -0x5000, 0x6000, 0x5400, 0x1ED6,
     0x1A31, 0x1D14, 0x183B, 0x1BC2, 0x16B2, 0x1A32, 0x15EF, 0x15EE, 0x1055, 0x1334, 0x0F2D,
     0x11F6, 0x0C5D, 0x1056, 0x0AE1, 0x0AE0, 0x07A2, 0x0464, 0x0232, -0x8000, -0x8000},
    // 7: ECHO
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, -0x7F00, 0x0000, 0x0000, 0x1FFF,
     0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, -0x8000, -0x8000},
    // 8: DELAY
    {0x0001, 0x0001, 0x7FFF, 0x7FFF, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x0000, 0x1FFF,
     0x0FFF, 0x1005, 0x0005, 0x0000, 0x0000, 0x1005, 0x0005, 0x0000, 0x0000, 0x0000, 0x0000,
     0x0000, 0x0000, 0x0000, 0x0000, 0x1004, 0x1002, 0x0004, 0x0002, -0x8000, -0x8000},
    // 9: PIPE (the PS1's "half echo")
    {0x0017, 0x0013, 0x70F0, 0x4FA8, -0x4320, 0x4510, -0x4110, -0x7B00, 0x5F80, 0x54C0, 0x0371,
     0x02AF, 0x02E5, 0x01DF, 0x02B0, 0x01D7, 0x0358, 0x026A, 0x01D6, 0x011E, 0x012D, 0x00B1,
     0x011F, 0x0059, 0x01A0, 0x00E3, 0x0058, 0x0040, 0x0028, 0x0014, -0x8000, -0x8000},
}};

s16 Clamp16(s64 v) {
  return static_cast<s16>(std::clamp<s64>(v, INT16_MIN, INT16_MAX));
}
}  // namespace

static s16 ApplyVolume(s16 sample, s32 volume) {
  return static_cast<s16>((sample * volume) >> 15);
}

void Synth::StepReverb(s32 revInL, s32 revInR) {
  if (mReverbMode <= 0 || mReverbMode >= static_cast<s32>(kReverbPresets.size())) {
    mReverbOutL = 0;
    mReverbOutR = 0;
    return;
  }
  if (mReverbBuf.size() < 0x10000) {
    mReverbBuf.assign(0x10000, 0);
  }

  mReverbPhase ^= 1;
  if (mReverbPhase == 0) {
    return;
  }

  ReverbPreset r = kReverbPresets[mReverbMode];
  if (mReverbMode == 7 || mReverbMode == 8) {
    u16 d = static_cast<u16>(std::clamp<s32>(mReverbDelay, 0, 127) << 4);
    r.mLSAME = static_cast<u16>(0x1004 + d);
    r.mRSAME = static_cast<u16>(0x0004 + d);
    r.vWALL = static_cast<s16>(std::clamp<s32>((mReverbFeedback * 0x8000) / 128, -0x8000, 0x7FFF));
  }

  auto rd = [&](u32 off) -> s64 { return mReverbBuf[(mReverbPos + (off << 2)) & 0xFFFF]; };
  auto rd1 = [&](u32 off) -> s64 { return mReverbBuf[(mReverbPos + (off << 2) - 1) & 0xFFFF]; };
  auto wr = [&](u32 off, s16 val) { mReverbBuf[(mReverbPos + (off << 2)) & 0xFFFF] = val; };

  s32 Lin = (Clamp16(revInL) * static_cast<s32>(r.vLIN)) >> 15;
  s32 Rin = (Clamp16(revInR) * static_cast<s32>(r.vRIN)) >> 15;

  wr(r.mLSAME,
     Clamp16((((Lin + ((rd(r.dLSAME) * r.vWALL) >> 15) - rd1(r.mLSAME)) * r.vIIR) >> 15) +
             rd1(r.mLSAME)));
  wr(r.mRSAME,
     Clamp16((((Rin + ((rd(r.dRSAME) * r.vWALL) >> 15) - rd1(r.mRSAME)) * r.vIIR) >> 15) +
             rd1(r.mRSAME)));

  wr(r.mLDIFF,
     Clamp16((((Lin + ((rd(r.dRDIFF) * r.vWALL) >> 15) - rd1(r.mLDIFF)) * r.vIIR) >> 15) +
             rd1(r.mLDIFF)));
  wr(r.mRDIFF,
     Clamp16((((Rin + ((rd(r.dLDIFF) * r.vWALL) >> 15) - rd1(r.mRDIFF)) * r.vIIR) >> 15) +
             rd1(r.mRDIFF)));

  s32 Lout = Clamp16((r.vCOMB1 * rd(r.mLCOMB1) + r.vCOMB2 * rd(r.mLCOMB2) +
                      r.vCOMB3 * rd(r.mLCOMB3) + r.vCOMB4 * rd(r.mLCOMB4)) >>
                     15);
  s32 Rout = Clamp16((r.vCOMB1 * rd(r.mRCOMB1) + r.vCOMB2 * rd(r.mRCOMB2) +
                      r.vCOMB3 * rd(r.mRCOMB3) + r.vCOMB4 * rd(r.mRCOMB4)) >>
                     15);

  s64 lapf1 = rd(r.mLAPF1 - r.dAPF1);
  Lout = Clamp16(Lout - ((r.vAPF1 * lapf1) >> 15));
  wr(r.mLAPF1, static_cast<s16>(Lout));
  Lout = Clamp16(((Lout * r.vAPF1) >> 15) + lapf1);

  s64 rapf1 = rd(r.mRAPF1 - r.dAPF1);
  Rout = Clamp16(Rout - ((r.vAPF1 * rapf1) >> 15));
  wr(r.mRAPF1, static_cast<s16>(Rout));
  Rout = Clamp16(((Rout * r.vAPF1) >> 15) + rapf1);

  s64 lapf2 = rd(r.mLAPF2 - r.dAPF2);
  Lout = Clamp16(Lout - ((r.vAPF2 * lapf2) >> 15));
  wr(r.mLAPF2, static_cast<s16>(Lout));
  Lout = Clamp16(((Lout * r.vAPF2) >> 15) + lapf2);

  s64 rapf2 = rd(r.mRAPF2 - r.dAPF2);
  Rout = Clamp16(Rout - ((r.vAPF2 * rapf2) >> 15));
  wr(r.mRAPF2, static_cast<s16>(Rout));
  Rout = Clamp16(((Rout * r.vAPF2) >> 15) + rapf2);

  mReverbOutL = static_cast<s16>(Lout);
  mReverbOutR = static_cast<s16>(Rout);
  mReverbPos = (mReverbPos + 1) & 0xFFFF;
}

namespace {
// rac1 (host): the speakers of a 5.1 / 7.1 output around the listener (degrees, 0 ahead, 90 right: 989snd's pan), in
// order of angle, with their channel in the WAVE order (FL FR FC LFE, then 5.1: SL SR; 7.1: BL BR SL SR). The centre
// is left out: the positional sounds are panned between the speakers of the ring; the centre is the dialogue's.
struct Speaker {
  int angle;
  int channel;
};
constexpr Speaker kRing51[] = {{-110, 4}, {-30, 0}, {30, 1}, {110, 5}};
constexpr Speaker kRing71[] = {{-150, 4}, {-90, 6}, {-30, 0}, {30, 1}, {90, 7}, {150, 5}};

// the gains of a sound at `pan` degrees: equal power between the two speakers either side of it
void RingGains(float* g, int pan, int channels) {
  const Speaker* ring = channels == 8 ? kRing71 : kRing51;
  const int n = channels == 8 ? 6 : 4;
  for (int c = 0; c < 8; ++c) {
    g[c] = 0.0f;
  }
  for (int i = 0; i < n; ++i) {
    const Speaker& a = ring[i];
    const Speaker& b = ring[(i + 1) % n];
    const int span = (b.angle - a.angle + 360) % 360;
    const int off = (pan - a.angle + 720) % 360;
    if (off <= span) {
      const float t = static_cast<float>(off) / static_cast<float>(span) * 1.5707963f;
      g[a.channel] = std::cos(t);
      g[b.channel] = std::sin(t);
      return;
    }
  }
}

// the dialogue's gains: the centre, a pan spreading it towards the front left / right (equal power; a pan behind folded
// to the front: the dialogue stays ahead)
void CentreGains(float* g, int pan) {
  for (int c = 0; c < 8; ++c) {
    g[c] = 0.0f;
  }
  int a = std::abs(pan);
  if (a > 90) {
    a = 180 - a;
  }
  const float t = static_cast<float>(a) / 90.0f * 1.5707963f;
  g[2] = std::cos(t);
  g[pan < 0 ? 0 : 1] = std::sin(t);
}

s16 Clamp16f(float v) {
  return static_cast<s16>(std::clamp(std::lround(v), static_cast<long>(INT16_MIN), static_cast<long>(INT16_MAX)));
}
}  // namespace

s16Output Synth::Tick(s16* multi, int channels) {
  s32 dryL = 0;
  s32 dryR = 0;
  s32 core0L = 0;  // rac1: core 0's voices (its output: into core 1's input, and its reverb with 0x0d)
  s32 core0R = 0;
  s32 revInL = 0;
  s32 revInR = 0;
  const bool spatial = multi != nullptr && (channels == 6 || channels == 8);
  float spk[8] = {};

  mVoices.remove_if([](std::shared_ptr<Voice>& v) { return v->Dead(); });
  for (auto& v : mVoices) {
    s16Output vo = v->Run();
    // rac1: the SPU2's routing. A core-1 voice reaches the reverb when its tone has the reverb flag (VMIXEL / R, set
    // from the tone's flag 1: 0x151cc). A core-0 voice's VMIXE feeds core 0's reverb, which the game never turns on
    // (0x50 / 0x10 on core mask 2), but core 0's whole output goes into core 1's reverb when the game set the mixer
    // mode (0x0d (0, 1) at its start, 0x28ca88): then every core-0 voice is reverberated. (A reverb-only tone, 0x10,
    // on core 0 is kept feeding the reverb, as on core 1.)
    if (v->Core() == 0 && mCore0Reverb) {
      if (!v->IsReverbOnly()) {
        core0L += vo.left;
        core0R += vo.right;
      } else {
        revInL += vo.left;
        revInR += vo.right;
      }
    } else {
      if (!v->IsReverbOnly()) {
        dryL += vo.left;
        dryR += vo.right;
      }
      if (v->IsReverb()) {
        revInL += vo.left;
        revInR += vo.right;
      }
    }
    if (spatial && !v->IsReverbOnly()) {
      const Voice::Place place = mMono ? Voice::Place::Front : v->GetPlace();
      if (place == Voice::Place::Front) {
        spk[0] += vo.left;
        spk[1] += vo.right;
      } else {
        if (v->mSpkPan != v->GetPan() || v->mSpkLayout != channels || v->mSpkPlace != place) {
          if (place == Voice::Place::Positional) {
            RingGains(v->mSpk, v->GetPan(), channels);
          } else {
            CentreGains(v->mSpk, v->GetPan());
          }
          v->mSpkPan = v->GetPan();
          v->mSpkLayout = static_cast<u8>(channels);
          v->mSpkPlace = place;
        }
        // the voice's level, its left and right's power, on its speakers
        const float l = vo.left, r = vo.right;
        float m = std::sqrt(l * l + r * r);
        if (v->LastSample() < 0) {
          m = -m;
        }
        for (int c = 0; c < channels; ++c) {
          spk[c] += m * v->mSpk[c];
        }
      }
    }
  }
  if (mCore0Reverb) {  // core 0's output (16 bits, as the SPU2's cores give) into core 1's dry and reverb inputs
    const s16 c0L = Clamp16(core0L), c0R = Clamp16(core0R);
    dryL += c0L;
    dryR += c0R;
    revInL += c0L;
    revInR += c0R;
  }

  s32 wetL = 0;
  s32 wetR = 0;
  if (mReverbMode > 0) {
    StepReverb(revInL, revInR);
    wetL = ApplyVolume(mReverbOutL, mReverbDepth.left.GetCurrent());
    wetR = ApplyVolume(mReverbOutR, mReverbDepth.right.GetCurrent());
    dryL += wetL;
    dryR += wetR;
    mReverbDepth.Run();
  }

  s16Output out{Clamp16(dryL), Clamp16(dryR)};
  out.left = ApplyVolume(out.left, mVolume.left.GetCurrent());
  out.right = ApplyVolume(out.right, mVolume.right.GetCurrent());

  if (spatial) {
    // rac1 (host): the reverb, a diffuse sound, spread over its side's speakers at equal power (front and surround
    // 1/sqrt(2) in 5.1; front, side and back 1/sqrt(3) in 7.1); the LFE silent (the original has none: a receiver's
    // bass management sends the speakers' lows to the subwoofer)
    if (wetL != 0 || wetR != 0) {
      const float k = channels == 8 ? 0.57735027f : 0.70710678f;
      const float wl = static_cast<float>(wetL) * k, wr = static_cast<float>(wetR) * k;
      spk[0] += wl;
      spk[1] += wr;
      spk[4] += wl;
      spk[5] += wr;
      if (channels == 8) {
        spk[6] += wl;
        spk[7] += wr;
      }
    }
    spk[3] = 0.0f;
    const s32 vl = mVolume.left.GetCurrent(), vr = mVolume.right.GetCurrent();
    for (int c = 0; c < channels; ++c) {
      const bool right = c == 1 || c == 5 || c == 7;
      multi[c] = ApplyVolume(Clamp16f(spk[c]), right ? vr : vl);
    }
  }

  mVolume.Run();

  return out;
}

// orig: (host) Tick's state for `samples` samples, voice by voice (they are independent without the mix): a voice dead at
// the start of one of them dropped (Tick's remove_if at that sample), one that dies at the last kept until the next
// call, as Tick keeps it until its next sample. The reverb's delay line and the master / reverb volumes' ramps are not
// run (only the output reads them).
void Synth::TickState(int samples) {
  mVoices.remove_if([samples](std::shared_ptr<Voice>& v) { return !v->RunState(samples); });
}

void Synth::AddVoice(std::shared_ptr<Voice> voice) {
  if (voice->Slot() >= 0) {
    mSlots[voice->Slot()] = voice;
  }
  mVoices.emplace_front(voice);
}

// orig: (host) the IRX's 0x14d60 (the core of the voice it would give)
void Synth::AllocVoice(Voice& voice, int group, int priority) {
  if (group < 0 || group >= 8) {
    group = 7;
  }
  const int lo = mGroupRange[group].first, hi = mGroupRange[group].second;
  voice.mPriority = static_cast<s8>(priority);
  voice.mStartTime = ++mAllocTime;
  int pick = -1;
  for (int i = lo; i <= hi; ++i) {
    auto o = mSlots[i].lock();
    if (o == nullptr || o->Dead()) {
      pick = i;
      break;
    }
  }
  if (pick < 0) {  // (the IRX steals the lowest priority, the oldest of equals: 0x14df0)
    std::shared_ptr<Voice> best;
    for (int i = lo; i <= hi; ++i) {
      auto o = mSlots[i].lock();
      if (pick < 0 || o->mPriority < best->mPriority ||
          (o->mPriority == best->mPriority && o->mStartTime < best->mStartTime)) {
        pick = i;
        best = o;
      }
    }
  }
  if (pick < 0) {
    return;
  }
  voice.SetSlot(static_cast<s8>(pick));  // (the slot taken by AddVoice)
}

// orig: (host) the IRX's 0x14ad4
void Synth::SetGroupVoiceRange(int group, int lo, int hi) {
  if (group < 0 || group >= 8) {
    return;
  }
  if (lo < 0) {
    lo = 0;
  }
  if (hi >= 48) {
    hi = 47;
  }
  mGroupRange[group] = {static_cast<s8>(lo), static_cast<s8>(hi)};
}

// orig: (host) the IRX's 0x149e8: core 1's MMIX bits 0..1 (core 0's output into its reverb) from the second argument;
// the groups' ranges from the first (1: core 1 for all but group 1; else every voice)
void Synth::SetMixerMode(int channel_mode, int reverb_mode) {
  mCore0Reverb = reverb_mode != 0;
  if (channel_mode == 1) {
    mGroupRange.fill({24, 47});
    mGroupRange[1] = {0, 23};
  } else {
    mGroupRange.fill({0, 47});
  }
}

void Synth::SetMasterVol(u32 volume) {
  mVolume.left.Set(volume);
  mVolume.right.Set(volume);
}

void Synth::SetReverbMode(s32 mode, s32 delay, s32 feedback) {
  s32 clamped = std::clamp<s32>(mode, 0, 9);
  if (clamped != mReverbMode) {
    mReverbMode = clamped;
    if (mReverbBuf.size() < 0x10000) {
      mReverbBuf.assign(0x10000, 0);
    } else {
      std::fill(mReverbBuf.begin(), mReverbBuf.end(), 0);
    }
    mReverbOutL = 0;
    mReverbOutR = 0;
  }
  mReverbDelay = delay;
  mReverbFeedback = feedback;
}

void Synth::SetReverbDepth(s16 left, s16 right) {
  u16 l = static_cast<u16>((left >> 1) & 0x7FFF);  // level = the depth (its low bit dropped)
  u16 r = static_cast<u16>((right >> 1) & 0x7FFF);
  mReverbDepth.left.Set(l);
  mReverbDepth.right.Set(r);
}

void Synth::AutoReverb(s16 depth, s32 /*delta_time*/, s32 channel_flags) {
  u16 d = static_cast<u16>((depth >> 1) & 0x7FFF);
  if (channel_flags & 1) {
    mReverbDepth.left.Set(d);
  }
  if (channel_flags & 2) {
    mReverbDepth.right.Set(d);
  }
}
}  // namespace snd
