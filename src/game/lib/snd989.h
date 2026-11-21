// The EE side of 989snd (base ELF 0x12da28..0x12f1a0): commands for the sound driver on the
// IOP, batched in two 4 KB buffers and sent by SIF RPC (server 0x123456, batch call 0x4d; the
// loader server 0x123457), replies handed to per-command callbacks.
#ifndef RAC1_GAME_LIB_SND989_H
#define RAC1_GAME_LIB_SND989_H

#include <cstdint>

namespace rac1 {

// EE callbacks the replies are handed to (function address -> port): (reply, data).
using SndCallback = void (*)(int32_t reply, uint64_t data);
void snd_register_callback(uint32_t addr, SndCallback fn);

void snd_init();                                                         // orig: 0x0012da28
int32_t snd_flush();                                                     // orig: 0x0012dc80
int32_t snd_reply_ready();                                               // orig: 0x0012de70
void snd_reply_wait_set(uint32_t buf, int32_t n);                        // orig: 0x0012def8
int32_t snd_bank_load_by_loc(uint32_t loc, uint32_t offset);             // orig: 0x0012df20
void snd_bank_load_from_ee_cb(uint32_t addr, uint32_t cb, uint64_t data);      // orig: 0x0012e088
void snd_cmd_08();                                                       // orig: 0x0012e1a8
void snd_cmd_06(uint32_t a);                                             // orig: 0x0012e1d8
void snd_set_master_volume(uint32_t group, int32_t vol);                 // orig: 0x0012e208
void snd_cmd_0b(uint32_t a);                                             // orig: 0x0012e240
void snd_cmd_0d(uint32_t a, uint32_t b);                                 // orig: 0x0012e280
void snd_cmd_4e(uint32_t a, uint32_t b, uint32_t c);                     // orig: 0x0012e2b8
void snd_play_sound(uint32_t bank, uint32_t snd, int32_t vol, int32_t pan, int32_t pm, int32_t pb, uint32_t cb, uint64_t data);  // orig: 0x0012e308
void snd_stop_sound(uint32_t handle);                                    // orig: 0x0012e368
void snd_cmd_18();                                                       // orig: 0x0012e3b8
void snd_cmd_16(uint32_t a);                                             // orig: 0x0012e3e8
void snd_cmd_17(uint32_t a);                                             // orig: 0x0012e418
void snd_sound_is_playing(uint32_t handle, uint32_t cb, uint64_t data);  // orig: 0x0012e448
void snd_set_sound(uint32_t handle, uint32_t flags, int32_t vol, int32_t pan, int32_t pm, int32_t pb, uint32_t cb, uint64_t data);  // orig: 0x0012e4c0
int32_t snd_command_sync(uint32_t cmd, int32_t size, uint32_t data);     // orig: 0x0012e548
int32_t snd_command_sync_bytes(uint32_t cmd, int32_t size, const uint8_t* data);  // orig: 0x0012e548
void snd_command_bytes(uint32_t cmd, int32_t size, const uint8_t* data, uint32_t cb, uint64_t cbdata);  // orig: 0x0012e6e0
int32_t snd_cd_status(int32_t mode);                                     // orig: 0x0012ee08
void snd_command(uint32_t cmd, int32_t size, uint32_t data, uint32_t cb, uint64_t cbdata);  // orig: 0x0012e6e0
void snd_command_added();                                                // orig: 0x0012e9a0
void snd_batch_send();                                                   // orig: 0x0012e9d8
void snd_hold();                                                         // orig: 0x0012eaf0
void snd_release();                                                      // orig: 0x0012eb00
uint32_t snd_set_stream_callback(uint32_t fn);                          // orig: 0x0012ef28
void snd_cmd_ef68(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e);  // orig: 0x0012ef68
void snd_cmd_2e(uint32_t a);                                                 // orig: 0x0012eca0
void snd_cmd_51(uint32_t a, uint32_t b);                                  // orig: 0x0012efa8
int32_t snd_stream_init(uint32_t a, uint32_t b, uint32_t c, uint32_t d);  // orig: 0x0012eb20
void snd_cmd_efe0(uint32_t a, uint32_t b, uint32_t c, uint32_t d);      // orig: 0x0012efe0
int32_t snd_pitch_from_cents(int32_t c);                                 // orig: 0x0012f1a0
// The VAG streams (the IOP's stream system, command 0x2a):
void snd_stream_stop_all();                                              // orig: 0x0012ebd0
void snd_stream_play(uint32_t loc1, uint32_t loc2, uint32_t off1, uint32_t off2, int32_t vol, int32_t pan, uint32_t group,
                     uint32_t queue, uint32_t flags, uint32_t cb, uint64_t data);  // orig: 0x0012ec08
void snd_stream_pause(uint32_t handle);                                  // orig: 0x0012ec70
void snd_stream_remaining(uint32_t handle, uint32_t cb, uint64_t data);  // orig: 0x0012ecd0
void snd_stream_started(uint32_t handle, uint32_t cb, uint64_t data);    // orig: 0x0012ed00
int32_t snd_stream_reads_wait(uint32_t a);                               // orig: 0x0012ed30
int32_t snd_stream_cd_read(uint32_t lsn, uint32_t n, uint32_t buf, uint32_t mode);  // orig: 0x0012ed58
int32_t snd_cd_break();                                                  // orig: 0x0012eea8
int32_t snd_cd_error();                                                  // orig: 0x0012eef0

// Optional host-registered callbacks for semantic behaviour tracing (RAC1_BEHAV_TRACE).
struct SndTraceHooks {
    void (*sfx)(uint32_t snd, int32_t vol, int32_t pan, int32_t pm, int32_t pb) = nullptr;
    void (*stream)(uint32_t group, int32_t vol, int32_t pan, uint32_t queue, uint32_t flags) = nullptr;
    void (*stop_streams)() = nullptr;
};
void snd_set_trace_hooks(const SndTraceHooks& hooks);

}  // namespace rac1

#endif
