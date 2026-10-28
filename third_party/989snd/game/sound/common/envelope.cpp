// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC

#include "envelope.h"

#include <algorithm>
#include <array>

namespace snd {
void ADSR::UpdateSettings() {
  switch (m_Phase) {
    case Phase::Attack:
      m_Exp = m_Reg.AttackExp.get();
      m_Decrease = false;
      m_Shift = m_Reg.AttackShift.get();
      m_Step = static_cast<s8>(7 - m_Reg.AttackStep.get());
      m_Target = 0x7FFF;
      break;
    case Phase::Decay:
      m_Exp = true;
      m_Decrease = true;
      m_Shift = m_Reg.DecayShift.get();
      m_Step = -8;
      m_Target = (m_Reg.SustainLevel.get() + 1) << 11;
      break;
    case Phase::Sustain:
      m_Exp = m_Reg.SustainExp.get();
      m_Decrease = m_Reg.SustainDecr.get();
      m_Shift = m_Reg.SustainShift.get();
      m_Step = m_Decrease ? static_cast<s8>(-8 + m_Reg.SustainStep.get())
                          : static_cast<s8>(7 - m_Reg.SustainStep.get());
      m_Target = 0;  // unused for sustain
      break;
    case Phase::Release:
      m_Exp = m_Reg.ReleaseExp.get();
      m_Decrease = true;
      m_Shift = m_Reg.ReleaseShift.get();
      m_Step = -8;
      m_Target = 0;
      break;
    default:
      break;
  }
}

void ADSR::Attack() {
  m_Phase = Phase::Attack;
  m_Level = 0;
  m_Counter = 0;
  UpdateSettings();
}

void ADSR::Release() {
  m_Phase = Phase::Release;
  m_Counter = 0;
  UpdateSettings();
}

void ADSR::Stop() {
  m_Phase = Phase::Stopped;
  m_Level = 0;
}

s16 ADSR::Level() const {
  return static_cast<u16>(m_Level);
}

void Volume::Run() {
  if (!m_Sweep.EnableSweep.get()) {
    if (m_SmoothLeft > 0) {
      m_SmoothFixed += m_SmoothStep;
      if (--m_SmoothLeft <= 0) {
        m_Level = m_TargetLevel;
        m_SmoothFixed = m_TargetLevel << 10;
      } else {
        m_Level = std::clamp<s32>(m_SmoothFixed >> 10, INT16_MIN, INT16_MAX);
      }
    }
    return;
  }

  Step();
}

void Volume::Snap() {
  if (!m_Sweep.EnableSweep.get()) {
    m_Level = m_TargetLevel;
    m_SmoothFixed = m_TargetLevel << 10;
    m_SmoothStep = 0;
    m_SmoothLeft = 0;
  }
}

void Volume::Set(u16 volume) {
  m_Sweep.bits = volume;

  if (!m_Sweep.EnableSweep.get()) {
    const s32 target = static_cast<s16>(m_Sweep.bits << 1);
    if (!m_Primed) {
      m_Primed = true;
      m_TargetLevel = target;
      m_Level = target;
      m_SmoothFixed = target << 10;
      m_SmoothStep = 0;
      m_SmoothLeft = 0;
    } else if (target == m_TargetLevel) {
      return;
    } else if (target == m_Level) {
      m_TargetLevel = target;
      m_SmoothFixed = target << 10;
      m_SmoothStep = 0;
      m_SmoothLeft = 0;
    } else {
      m_TargetLevel = target;
      constexpr s32 kSmoothSamples = 400;  // 1/120 s (~8.3 ms) at 48 kHz
      m_SmoothFixed = m_Level << 10;
      m_SmoothLeft = kSmoothSamples;
      m_SmoothStep = ((target << 10) - m_SmoothFixed) / kSmoothSamples;
    }
    return;
  }

  m_SmoothLeft = 0;
  m_Exp = m_Sweep.SweepExp.get();
  m_Decrease = m_Sweep.SweepDecrease.get();
  m_Shift = m_Sweep.SweepShift.get();
  m_Step = m_Decrease ? static_cast<s8>(-8 + m_Sweep.SweepStep.get())
                      : static_cast<s8>(7 - m_Sweep.SweepStep.get());

  if (m_Exp && m_Decrease) {
    // if (m_Sweep.NegativePhase)
    //	Console.WriteLn("Disqualified from inv");
    m_Inv = false;
  } else {
    m_Inv = m_Sweep.NegativePhase.get();
  }

  // Console.WriteLn(Color_Red, "start sweep, e:%d d:%d sh:%d st:%d inv:%d", m_Exp, m_Decrease,
  // m_Shift, m_Step, m_Inv); Console.WriteLn(Color_Red, "Current level %08x", m_Level);
}

u16 Volume::Get() const {
  return m_Sweep.bits;
}

s16 Volume::GetCurrent() const {
  return static_cast<s16>(m_Level);
}
}  // namespace snd
