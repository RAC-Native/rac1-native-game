// Copyright: 2021 - 2024, Ziemas
// SPDX-License-Identifier: ISC
#pragma once

#include <map>
#include <memory>
#include <mutex>
#include <span>
#include <vector>

#if defined(__WIIU__)
#include <coreinit/mutex.h>
#endif

#include "ame_handler.h"
#include "handle_allocator.h"
#include "loader.h"
#include "sound_handler.h"

#include "common/common_types.h"

#include "../common/synth.h"
#include "game/sound/989snd/vagvoice.h"


namespace snd {

class Player {
 public:
  Player();
  ~Player();
  Player(const Player&) = delete;
  Player operator=(const Player&) = delete;

  // player(player&& other) noexcept = default;
  // player& operator=(player&& other) noexcept = default;

  BankHandle LoadBank(std::span<u8> bank);

  u32 PlaySound(BankHandle bank, u32 sound, s32 vol, s32 pan, s32 pm, s32 pb);
  u32 PlaySoundByName(BankHandle bank,
                      char* bank_name,
                      char* sound_name,
                      s32 vol,
                      s32 pan,
                      s32 pm,
                      s32 pb);
  void DebugPrintAllSoundsInBank(BankHandle bank);
  void SetSoundReg(u32 sound_id, u8 reg, u8 value);
  u8 GetSoundGroup(u32 sound_id);
  void SetGlobalExcite(u8 value) { GlobalExcite = value; };
  bool SoundStillActive(u32 sound_id);
  void SetMasterVolume(u32 group, s32 volume);
  void UnloadBank(BankHandle bank_handle);
  void StopSound(u32 sound_handle);
  u32 GetSoundID(u32 sound_handle);
  void SetPanTable(VolPair* pantable);
  void SetPlaybackMode(s32 mode);
  void PauseSound(s32 sound_handle);
  void ContinueSound(s32 sound_handle);
  void PauseAllSoundsInGroup(u8 group);
  void ContinueAllSoundsInGroup(u8 group);
  void SetSoundVolPan(s32 sound_handle, s32 vol, s32 pan);
  void AutoPan(s32 sound_handle, s32 pan, s32 dir, s32 delta_time, s32 mode);
  void SubmitVoice(std::shared_ptr<Voice>& voice) { mSynth.AddVoice(voice); };
  // rac1: a voice of the group's range (its core, Synth::AllocVoice), before SubmitVoice; the IRX's 0x0d / 0x4e
  void AllocVoice(Voice& voice, int group, int priority) {
    std::scoped_lock lock(mTickLock);
    mSynth.AllocVoice(voice, group, priority);
  }
  void SetMixerMode(s32 channel_mode, s32 reverb_mode) {
    std::scoped_lock lock(mTickLock);
    mSynth.SetMixerMode(channel_mode, reverb_mode);
  }
  void SetGroupVoiceRange(s32 group, s32 lo, s32 hi) {
    std::scoped_lock lock(mTickLock);
    mSynth.SetGroupVoiceRange(group, lo, hi);
  }
  void SetSoundPmod(s32 sound_handle, s32 mod);
  void SetSoundPBend(s32 sound_handle, s32 bend);  // rac1: the handler's pitch bend
  void SetReverbMode(s32 mode, s32 delay = 0, s32 feedback = 0);
  void SetReverbDepth(s16 left, s16 right);
  void AutoReverb(s16 depth, s32 delta_time, s32 channel_flags);
  s32 GetTick() { return mTick; };
  void StopAllSounds();
  int ActiveSounds() { std::scoped_lock lock(mTickLock); return static_cast<int>(mHandlers.size()); }  // rac1: a debug count
  s32 GetSoundUserData(BankHandle block_handle,
                       char* block_name,
                       s32 sound_id,
                       char* sound_name,
                       SFXUserData* dst);

 // rac1: the host's audio device pulls the output (the cubeb stream removed)
  // `multi` (host): also `channels` (6 / 8) speakers' samples per frame (Synth::Tick)
  void Tick(s16Output* stream, int samples, s16* multi = nullptr, int channels = 2);

 private:
#if defined(__WIIU__)
  // rac1 (the Wii U port): wut's std::recursive_mutex locks again as a plain mutex (a deadlock); Cafe OS's own
  // mutex is recursive
  struct RecursiveMutex {
    OSMutex m;
    RecursiveMutex() { OSInitMutex(&m); }
    void lock() { OSLockMutex(&m); }
    void unlock() { OSUnlockMutex(&m); }
    bool try_lock() { return OSTryLockMutex(&m) != 0; }
  };
  RecursiveMutex mTickLock;
#else
  std::recursive_mutex mTickLock;  // TODO does not need to recursive with some light restructuring
#endif
  IdAllocator mHandleAllocator;
  std::map<u32, std::unique_ptr<SoundHandler>> mHandlers;



  Loader mLoader;
  Synth mSynth;
  VoiceManager mVmanager;
  s32 mTick{0};
  int mHTick{200}, mSTick{48000};  // rac1: Tick's counters, statics before (one per player)

};
}  // namespace snd
