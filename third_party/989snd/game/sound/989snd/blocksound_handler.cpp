#include "blocksound_handler.h"

#include <random>
#include <stdexcept>

#include "util.h"

#include "common/log/log.h"

namespace snd {
std::array<s8, 32> g_block_reg{};

BlockSoundHandler::BlockSoundHandler(SoundBank& bank,
                                     SFXBlock::SFX& sfx,
                                     VoiceManager& vm,
                                     s32 sfx_vol,
                                     s32 sfx_pan,
                                     SndPlayParams& params,
                                     u32 sound_id,
                                     s32 start_tick,
                                     u32 owner)
    : m_group(sfx.VolGroup),
      m_sfx(&sfx),
      m_orig_sfx(&sfx),
      m_vm(vm),
      m_bank(bank),
      m_sound_id(sound_id),
      m_start_tick(start_tick) {
  m_sound_handle = owner;

  s32 vol, pan, pitch_mod, pitch_bend;
  if (sfx_vol == -1) {
    sfx_vol = sfx.Vol;
  }
  if (sfx_pan == -1) {
    sfx_pan = sfx.Pan;
  }

  if (params.vol.has_value()) {
    vol = params.vol.value();
  } else {
    vol = 1024;
  }

  if (params.pan.has_value()) {
    pan = params.pan.value();
  } else {
    pan = -1;
  }

  if (params.pitch_mod.has_value()) {
    pitch_mod = params.pitch_mod.value();
  } else {
    pitch_mod = 0;
  }

  if (params.pitch_bend.has_value()) {
    pitch_bend = params.pitch_bend.value();
  } else {
    pitch_bend = 0;
  }

  if (vol == VOLUME_DONT_CHANGE) {
    vol = 1024;
  }
  s32 play_vol = (sfx_vol * vol) >> 10;
  if (play_vol >= 128) {
    play_vol = 127;
  }

  if (pan == PAN_RESET || pan == PAN_DONT_CHANGE) {
    pan = sfx_pan;
  }

  m_orig_volume = sfx_vol;
  m_orig_pan = sfx_pan;

  m_cur_volume = play_vol;
  m_cur_pan = pan;
  m_positional = pan != 0;
  m_cur_pb = pitch_bend;
  m_cur_pm = pitch_mod;

  m_app_volume = vol;
  m_app_pan = pan;
  m_app_pb = pitch_bend;
  m_app_pm = pitch_mod;
  m_autopan_target = pan;
  m_autopan_pos = pan << 16;
  m_autopan_step = 0;
  m_autopan_ticks = 0;

  m_lfo_volume = 0;
  m_lfo_pan = 0;
  m_lfo_pb = 0;
  m_lfo_pm = 0;

  if (params.registers.has_value()) {
    m_registers = params.registers.value();
  }

  // Figure this stuff out properly someday
  // if (m_sfx.d.Flags & 2) {
  //   fmt::print("solo flag\n");
  //   m_done = true;
  //   return;
  // }

  m_next_grain = 0;
  m_countdown = m_sfx->Grains[0].Delay;
  while (m_countdown <= 0 && !m_done) {
    DoGrain();
  }
}

BlockSoundHandler::~BlockSoundHandler() {
  for (auto& p : m_voices) {
    auto v = p.lock();
    if (v != nullptr) {
      v->Stop();
    }
  }
}

bool BlockSoundHandler::Tick() {
  m_voices.remove_if([](std::weak_ptr<BlockSoundVoice>& p) { return p.expired(); });

  if (!m_paused) {
    if (m_autopan_ticks > 0) {
      m_autopan_ticks--;
      s32 next_pan = m_autopan_target;
      if (m_autopan_ticks > 0) {
        m_autopan_pos += m_autopan_step;
        next_pan = m_autopan_pos >> 16;
        while (next_pan >= 360)
          next_pan -= 360;
        while (next_pan < 0)
          next_pan += 360;
      }
      if (next_pan != m_app_pan) {
        ApplyVolPan(VOLUME_DONT_CHANGE, next_pan, false);
      }
    }
  }

  for (auto& lfo : m_lfo) {
    lfo.Tick();
  }

  for (auto it = m_children.begin(); it != m_children.end();) {
    bool done = it->get()->Tick();
    if (done) {
      it = m_children.erase(it);
    } else {
      ++it;
    }
  }

  if (m_done && m_children.empty()) {
    if (m_voices.empty()) {
      return m_done;
    } else {
      return false;
    }
  }

  if (m_paused)
    return false;

  m_countdown--;
  while (m_countdown <= 0 && !m_done) {
    DoGrain();
  }

  return false;
};

void BlockSoundHandler::Pause() {
  m_paused = true;

  for (auto& c : m_children) {
    c->Pause();
  }

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    m_vm.Pause(voice);
  }
}

void BlockSoundHandler::Unpause() {
  m_paused = false;

  for (auto& c : m_children) {
    c->Unpause();
  }

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    m_vm.Unpause(voice);
  }
}

void BlockSoundHandler::Stop() {
  m_done = true;

  for (auto& c : m_children) {
    c->Stop();
  }

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    voice->KeyOff();
  }
}

void BlockSoundHandler::ApplyVolPan(s32 vol, s32 pan, bool cancel_autopan) {
  if (vol != VOLUME_DONT_CHANGE) {
    m_app_volume = (vol >= 0) ? vol : (-1024 * vol / 127);
  }

  if (pan == PAN_RESET) {
    m_app_pan = m_sfx->Pan;
    if (cancel_autopan)
      m_autopan_ticks = 0;
  } else if (pan != PAN_DONT_CHANGE) {
    m_app_pan = pan;
    if (cancel_autopan)
      m_autopan_ticks = 0;
  }

  s32 new_vol = ((m_app_volume * m_orig_volume) >> 10) + m_lfo_volume;
  new_vol = std::clamp(new_vol, 0, 127);

  s32 new_pan = m_app_pan + m_lfo_pan;
  while (new_pan >= 360) {
    new_pan -= 360;
  }
  while (new_pan < 0) {
    new_pan += 360;
  }

  if (new_pan != m_cur_pan || new_vol != m_cur_volume) {
    m_cur_volume = new_vol;
    m_cur_pan = new_pan;
    if (new_pan != 0) {
      m_positional = true;
    }

    if (cancel_autopan) {
      for (auto& c : m_children) {
        c->SetVolPan(m_app_volume * m_orig_volume / 127, pan);
      }
    }

    for (auto& p : m_voices) {
      auto voice = p.lock();
      if (voice == nullptr) {
        continue;
      }

      auto volume = m_vm.MakeVolume(127, 0, m_cur_volume, m_cur_pan, voice->g_vol, voice->g_pan);
      voice->basevol = volume;
      auto left = m_vm.AdjustVolToGroup(volume.left, m_group);
      auto right = m_vm.AdjustVolToGroup(volume.right, m_group);

      voice->SetVolume(left >> 1, right >> 1);
      Place(*voice);
    }
  }
}

// rac1 (host): a voice's place in 5.1 / 7.1: a positional sound's voices by their pan (the sound's and the tone's),
// the others on the front left / right as in stereo
void BlockSoundHandler::Place(BlockSoundVoice& voice) {
  if (m_host_place == 1) {
    voice.SetPlace(Voice::Place::Positional, m_host_pan + voice.g_pan);
  } else if (m_host_place == 2) {
    voice.SetPlace(Voice::Place::Diffuse, 0);
  } else {
    voice.SetPlace(m_positional ? Voice::Place::Positional : Voice::Place::Front, m_cur_pan + voice.g_pan);
  }
}

// rac1 (host): see blocksound_handler.h; the voices playing placed again
void BlockSoundHandler::SetHostPlace(u8 mode, s32 pan) {
  if (mode == m_host_place && pan == m_host_pan) {
    return;
  }
  m_host_place = mode;
  m_host_pan = pan;
  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice != nullptr) {
      Place(*voice);
    }
  }
  for (auto& c : m_children) {
    c->SetHostPlace(mode, pan);
  }
}

void BlockSoundHandler::SetVolPan(s32 vol, s32 pan) {
  ApplyVolPan(vol, pan, true);
}

void BlockSoundHandler::AutoPan(s32 pan, s32 dir, s32 delta_time, s32 mode) {
  if (pan == PAN_RESET) {
    pan = m_sfx->Pan;
  } else if (pan == PAN_DONT_CHANGE) {
    return;
  }
  while (pan >= 360) {
    pan -= 360;
  }
  while (pan < 0) {
    pan += 360;
  }

  for (auto& c : m_children) {
    c->AutoPan(pan, dir, delta_time, mode);
  }

  s32 cur = m_app_pan;
  while (cur >= 360) {
    cur -= 360;
  }
  while (cur < 0) {
    cur += 360;
  }

  s32 diff = pan - cur;
  bool shortest = (dir == 0);
  if (shortest) {
    dir = 1;
  }
  if (dir > 0 && diff < 0) {
    diff += 360;
  }
  if (dir < 0 && diff > 0) {
    diff -= 360;
  }
  if (shortest && std::abs(diff) >= 181) {
    diff -= 360;
  }

  s32 ticks = delta_time;
  if (mode == 4) {
    ticks = (delta_time * std::abs(diff)) / 180;
  }
  if (diff == 0 || ticks <= 0) {
    m_autopan_ticks = 0;
    ApplyVolPan(VOLUME_DONT_CHANGE, pan, false);
    return;
  }

  m_autopan_target = pan;
  m_autopan_pos = cur << 16;
  m_autopan_step = (diff << 16) / ticks;
  m_autopan_ticks = ticks;
}

void BlockSoundHandler::UpdatePitch() {
  m_cur_pm = m_app_pm + m_lfo_pm;
  m_cur_pb = std::clamp<s32>(m_app_pb + m_lfo_pb, INT16_MIN, INT16_MAX);

  for (auto& p : m_voices) {
    auto voice = p.lock();
    if (voice == nullptr) {
      continue;
    }

    auto note = PitchBend(voice->tone, m_cur_pb, m_cur_pm, m_note, m_fine);
    auto pitch =
        PS1Note2Pitch(voice->tone.CenterNote, voice->tone.CenterFine, note.first, note.second);

    voice->SetPitch(pitch);
  }
}

void BlockSoundHandler::SetPMod(s32 mod) {
  for (auto& c : m_children) {
    c->SetPMod(mod);
  }
  m_app_pm = mod;
  UpdatePitch();
}

void BlockSoundHandler::SetPBend(s32 bend) {
  for (auto& c : m_children) {
    c->SetPBend(bend);
  }
  m_app_pb = bend;
  UpdatePitch();
}

void BlockSoundHandler::DoGrain() {
  auto& grain = m_sfx->Grains[m_next_grain];

  s32 ret = grain(*this);

  if (m_skip_grains) {
    m_grains_to_play--;
    if (m_grains_to_play == 0) {
      m_next_grain += m_grains_to_skip;
      m_skip_grains = false;
    }
  }

  m_next_grain++;
  if (m_next_grain >= m_sfx->Grains.size()) {
    m_done = true;
    return;
  }

  m_countdown = m_sfx->Grains[m_next_grain].Delay + ret;
}

SoundHandler* BlockSoundHandler::CheckInstanceLimit(
    const std::map<u32, std::unique_ptr<SoundHandler>>& handlers,
    s32 vol,
    bool parent) {
  if (!m_sfx->InstanceLimit) {
    return nullptr;
  }

  if (!m_sfx->Flags.has_instlimit()) {
    return nullptr;
  }

  BlockSoundHandler* weakest = nullptr;
  int inst = 0;

  for (const auto& [id, handler_ptr] : handlers) {
    // Only compare to BlockSoundHandlers
    auto* handler = dynamic_cast<BlockSoundHandler*>(handler_ptr.get());
    if (!handler) {
      continue;
    }

    // See if this is playing the same sound
    if ((parent && handler->m_orig_sfx == m_sfx) || handler->m_sfx == m_sfx) {
      inst++;
      if (!weakest ||                                                                         //
          (m_sfx->Flags.instlimit_vol() && handler->m_app_volume < weakest->m_app_volume) ||  //
          (m_sfx->Flags.instlimit_tick() && handler->m_start_tick < weakest->m_start_tick)) {
        weakest = handler;
      }
    }
  }

  // See if this handler would cause us to exceed the limit
  if (m_sfx->InstanceLimit - 1 < inst) {
    if (weakest && ((m_sfx->Flags.instlimit_vol() && weakest->m_app_volume < vol) ||
                    m_sfx->Flags.instlimit_tick())) {
      // existing weakest is worst
      return weakest;
    } else {
      // new sound is weakest
      return this;
    }
  } else {
    return nullptr;
  }
}

}  // namespace snd
