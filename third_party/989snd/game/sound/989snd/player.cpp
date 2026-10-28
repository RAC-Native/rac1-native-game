// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#include "player.h"

#include <algorithm>
#include <fstream>

#include "sfxblock.h"

#include "fmt/format.h"

#include "common/log/log.h"

namespace snd {

u8 g_global_excite = 0;

Player::Player() : mVmanager(mSynth) {}

Player::~Player() = default;

void Player::Tick(s16Output* stream, int samples, s16* multi, int channels) {
  std::scoped_lock lock(mTickLock);
  int& htick = mHTick;  // rac1: the player's own (two players: the game's IOP and the host's mirror)
  int& stick = mSTick;
  for (int i = 0; i < samples; i++) {
    // The handlers expect to tick at 240hz
    // 48000/240 = 200
    if (htick == 200) {
      mTick++;

      for (auto it = mHandlers.begin(); it != mHandlers.end();) {
        bool done = it->second->Tick();
        if (done) {
          // fmt::print("erasing handler\n");
          mHandleAllocator.FreeId(it->first);
          it = mHandlers.erase(it);
        } else {
          ++it;
        }
      }

      htick = 0;
    }

    if (stick == 48000) {
      // fmt::print("{} handlers active\n", m_handlers.size());
      stick = 0;
    }

    stick++;
    htick++;
    *stream++ = mSynth.Tick(multi, channels);
    if (multi != nullptr) {
      multi += channels;
    }
  }
}

// orig: (host) Tick's state alone: the handlers at the same samples (every 200th), the synth's voices run between them
void Player::TickState(int samples) {
  std::scoped_lock lock(mTickLock);
  int& htick = mHTick;
  int& stick = mSTick;
  for (int i = 0; i < samples;) {
    if (htick == 200) {
      mTick++;

      for (auto it = mHandlers.begin(); it != mHandlers.end();) {
        bool done = it->second->Tick();
        if (done) {
          mHandleAllocator.FreeId(it->first);
          it = mHandlers.erase(it);
        } else {
          ++it;
        }
      }

      htick = 0;
    }
    const int n = std::min(samples - i, 200 - htick);  // (to the next handlers' tick)
    stick = (stick - 1 + n) % 48000 + 1;                // (Tick's: 1..48000)
    htick += n;
    mSynth.TickState(n);
    i += n;
  }
}

u32 Player::PlaySound(BankHandle bank_id, u32 sound_id, s32 vol, s32 pan, s32 pm, s32 pb) {
  std::scoped_lock lock(mTickLock);
  auto bank = mLoader.GetBankByHandle(bank_id);
  if (bank == nullptr) {
    lg::error("play_sound: Bank {} does not exist", static_cast<void*>(bank_id));
    return 0;
  }

  u32 handle = mHandleAllocator.GetId();
  auto handler = bank->MakeHandler(mVmanager, sound_id, vol, pan, pm, pb, GetTick(), handle);
  if (!handler.has_value()) {
    return 0;
  }

  auto handler_to_stop = handler.value()->CheckInstanceLimit(mHandlers, vol, true);
  if (handler_to_stop) {
    handler_to_stop->Stop();
    if (handler_to_stop == handler.value().get()) {
      return 0;
    }
  }

  handler.value()->m_sound_handle = handle;
  mHandlers.emplace(handle, std::move(handler.value()));
  // fmt::print("play_sound {}:{} - {}\n", bank_id, sound_id, handle);

  return handle;
}

void Player::DebugPrintAllSoundsInBank(BankHandle bank_id) {
  std::scoped_lock lock(mTickLock);
  auto* bank = mLoader.GetBankByHandle(bank_id);
  if (!bank) {
    lg::error("DebugPrintAllSoundsInBank: invalid bank");
    return;
  }
  bank->DebugPrintAllSounds();
}

u32 Player::PlaySoundByName(BankHandle bank_id,
                            char* bank_name,
                            char* sound_name,
                            s32 vol,
                            s32 pan,
                            s32 pm,
                            s32 pb) {
  std::scoped_lock lock(mTickLock);
  SoundBank* bank = nullptr;
  if (bank_id == 0 && bank_name != nullptr) {
    bank = mLoader.GetBankByName(bank_name);
  } else if (bank_id != 0) {
    bank = mLoader.GetBankByHandle(bank_id);
  } else {
    bank = mLoader.GetBankWithSound(sound_name);
  }

  if (bank == nullptr) {
    // lg::error("play_sound_by_name: failed to find bank for sound {}", sound_name);
    return 0;
  }

  auto sound = bank->GetSoundByName(sound_name);
  if (sound.has_value()) {
    return PlaySound(bank, sound.value(), vol, pan, pm, pb);
  }

  // lg::error("play_sound_by_name: failed to find sound {}", sound_name);

  return 0;
}

void Player::StopSound(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Stop();

  // m_handle_allocator.free_id(sound_id);
  // m_handlers.erase(sound_id);
}

u32 Player::GetSoundID(u32 sound_handle) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return -1;
  return handler->second->SoundID();
}

void Player::SetSoundReg(u32 sound_id, u8 reg, u8 value) {
  std::scoped_lock lock(mTickLock);
  if (mHandlers.find(sound_id) == mHandlers.end()) {
    // fmt::print("set_midi_reg: Handler {} does not exist\n", sound_id);
    return;
  }

  auto* handler = mHandlers.at(sound_id).get();
  handler->SetRegister(reg, value);
}

u8 Player::GetSoundGroup(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end()) {
    return 0;
  }
  return handler->second->Group();
}

bool Player::SoundStillActive(u32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return false;

  // fmt::print("sound_still_active {}\n", sound_id);
  return true;
}

void Player::SetMasterVolume(u32 group, s32 volume) {
  std::scoped_lock lock(mTickLock);
  if (volume > 0x400)
    volume = 0x400;

  if (volume < 0)
    volume = 0;

  if (group == 15)
    return;

  mVmanager.SetMasterVol(group, volume);

  // Master volume
  if (group == 16) {
    mSynth.SetMasterVol(0x3fff * volume / 0x400);
  }
}

BankHandle Player::LoadBank(std::span<u8> bank) {
  std::scoped_lock lock(mTickLock);
  return mLoader.BankLoad(bank);
}

void Player::UnloadBank(BankHandle bank_handle) {
  std::scoped_lock lock(mTickLock);
  auto* bank = mLoader.GetBankByHandle(bank_handle);
  if (bank == nullptr)
    return;

  for (auto it = mHandlers.begin(); it != mHandlers.end();) {
    if (&it->second->Bank() == bank_handle) {
      mHandleAllocator.FreeId(it->first);
      it = mHandlers.erase(it);
    } else {
      ++it;
    }
  }

  mLoader.UnloadBank(bank_handle);
}

void Player::SetPanTable(VolPair* pantable) {
  std::scoped_lock lock(mTickLock);
  mVmanager.SetPanTable(pantable);
}

void Player::SetPlaybackMode(s32 mode) {
  std::scoped_lock lock(mTickLock);
  mVmanager.SetPlaybackMode(mode);
}

void Player::PauseSound(s32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Pause();
}

void Player::ContinueSound(s32 sound_id) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->Unpause();
}

void Player::PauseAllSoundsInGroup(u8 group) {
  std::scoped_lock lock(mTickLock);

  for (auto& h : mHandlers) {
    if ((1 << h.second->Group()) & group) {
      h.second->Pause();
    }
  }
}

void Player::ContinueAllSoundsInGroup(u8 group) {
  std::scoped_lock lock(mTickLock);

  for (auto& h : mHandlers) {
    if ((1 << h.second->Group()) & group) {
      h.second->Unpause();
    }
  }
}

void Player::SetSoundVolPan(s32 sound_id, s32 vol, s32 pan) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_id);
  if (handler == mHandlers.end())
    return;

  handler->second->SetVolPan(vol, pan);
}

void Player::AutoPan(s32 sound_handle, s32 pan, s32 dir, s32 delta_time, s32 mode) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return;

  handler->second->AutoPan(pan, dir, delta_time, mode);
}

void Player::SetSoundPmod(s32 sound_handle, s32 mod) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return;

  handler->second->SetPMod(mod);
}

// rac1: the handler's pitch bend (the EE library's set command has one)
void Player::SetSoundPBend(s32 sound_handle, s32 bend) {
  std::scoped_lock lock(mTickLock);
  auto handler = mHandlers.find(sound_handle);
  if (handler == mHandlers.end())
    return;

  handler->second->SetPBend(bend);
}

void Player::SetReverbMode(s32 mode, s32 delay, s32 feedback) {
  std::scoped_lock lock(mTickLock);
  mSynth.SetReverbMode(mode, delay, feedback);
}

void Player::SetReverbDepth(s16 left, s16 right) {
  std::scoped_lock lock(mTickLock);
  mSynth.SetReverbDepth(left, right);
}

void Player::AutoReverb(s16 depth, s32 delta_time, s32 channel_flags) {
  std::scoped_lock lock(mTickLock);
  mSynth.AutoReverb(depth, delta_time, channel_flags);
}

void Player::StopAllSounds() {
  std::scoped_lock lock(mTickLock);
  for (auto it = mHandlers.begin(); it != mHandlers.end();) {
    mHandleAllocator.FreeId(it->first);
    it = mHandlers.erase(it);
  }
}

s32 Player::GetSoundUserData(BankHandle block_handle,
                             char* block_name,
                             s32 sound_id,
                             char* sound_name,
                             SFXUserData* dst) {
  std::scoped_lock lock(mTickLock);
  SoundBank* bank = nullptr;
  if (block_handle == nullptr && block_name != nullptr) {
    bank = mLoader.GetBankByName(block_name);
  } else if (block_handle != nullptr) {
    bank = mLoader.GetBankByHandle(block_handle);
  } else {
    bank = mLoader.GetBankWithSound(sound_name);
  }

  if (bank == nullptr) {
    return 0;
  }

  if (sound_id == -1) {
    auto sound = bank->GetSoundByName(sound_name);
    if (sound.has_value()) {
      sound_id = sound.value();
    } else {
      return 0;
    }
  }

  auto ud = bank->GetSoundUserData(sound_id);
  if (ud.has_value()) {
    dst->data[0] = ud.value()->data[0];
    dst->data[1] = ud.value()->data[1];
    dst->data[2] = ud.value()->data[2];
    dst->data[3] = ud.value()->data[3];
    return 1;
  } else {
    return 0;
  }

  return 0;
}

}  // namespace snd
