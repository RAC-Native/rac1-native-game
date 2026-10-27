// The EE side of 989snd (see snd989.h). Globals ($gp-relative in the original, gp 0x166c00):
// 0x15eca0[2] the two command buffers (word 0 the count, commands from +4: u16 id, u16 size,
// the data padded to words), 0x15eca8[2] their free bytes (0xffc empty), 0x15ecb0[2] their
// callback tables (16 bytes a command: function, 64-bit data), 0x15ecb8[2] their reply
// buffers, 0x15ecc0 the current one, 0x15ecc4 hold (no sending), 0x15ec80 / 0x15ec84 the reply
// awaited (buffer, count: complete when its first and last words read -1), 0x15ec88 the last
// error, 0x15ec8c the stream system up; 0x15ecc8.. a bank load in progress (callback 0x15ecd0,
// data 0x15ecd8, result 0x15ed00). Clients: 0x15ebc0 (commands, server 0x123456), 0x15ebe8
// (loader, 0x123457). Synchronous calls send from 0x133140 and get 3 words at 0x133100.
#include <game/lib/snd989.h>
#include <game/ovl/ovl_reloc.h>

#include <game/lib/libcdvd.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/port_stubs.h>
#include <runtime/kernel/bootstrap.h>
#include <runtime/sif/sif_rpc.h>

#include <cstddef>
#include <vector>

namespace rac1 {

namespace {
constexpr uint32_t BUFS = 0x0015eca0, LEFT = 0x0015eca8, CBS = 0x0015ecb0, REPLIES = 0x0015ecb8;
constexpr uint32_t CUR = 0x0015ecc0, HOLD = 0x0015ecc4, PEND = 0x0015ec80, PEND_N = 0x0015ec84, ERR = 0x0015ec88;
constexpr uint32_t STREAM = 0x0015ec8c, LOAD_BUSY = 0x0015ecc8, LOAD_CB = 0x0015ecd0, LOAD_DATA = 0x0015ecd8, LOAD_RET = 0x0015ed00;
constexpr uint32_t CLIENT = 0x0015ebc0, LOADER = 0x0015ebe8, SYNC_RECV = 0x00133100, SYNC_SEND = 0x00133140;

struct CbEntry {
    uint32_t addr;
    SndCallback fn;
};
std::vector<CbEntry>& callbacks() {
    static std::vector<CbEntry> t;
    return t;
}
// orig: (helper) the jalr to a reply's callback in 0x0012dc80
void call_callback(uint32_t fn, int32_t reply, uint64_t data) {
    const uint32_t v = ovl_code_to_veldin(fn);  // (the registry holds Veldin's addresses)
    for (const CbEntry& e : callbacks()) {
        if (e.addr == v) {
            e.fn(reply, data);
            return;
        }
    }
    TODO_PORT(fn, "989snd callback")(reply, data);
}
}  // namespace

// orig: (none) — registers the port of an EE callback the replies are handed to.
void snd_register_callback(uint32_t addr, SndCallback fn) {
    for (CbEntry& e : callbacks()) {
        if (e.addr == addr) {
            e.fn = fn;
            return;
        }
    }
    callbacks().push_back(CbEntry{addr, fn});
}

// The command buffers set up, both servers bound (waiting until the IOP has them), the stream
// status word 0x137b00 handed to the IOP (command 0).
// orig: 0x0012da28
void snd_init() {
    sw(BUFS, 0x00133280);
    sw(BUFS + 4, 0x00134280);
    sw(REPLIES, 0x00137280);
    sw(REPLIES + 4, 0x001376c0);
    sw(CBS, 0x00135280);
    sw(CBS + 4, 0x00136280);
    sif_init_rpc(0);
    do {
        sif_bind_rpc(CLIENT, 0x123456, 0);
    } while (lw(CLIENT + 0x24) == 0);
    sw(LOAD_BUSY, 0);
    sw(LOAD_CB, 0);
    sd(LOAD_DATA, 0);
    sw(LOAD_RET, 0);
    do {
        sif_bind_rpc(LOADER, 0x123457, 0);
    } while (lw(LOADER + 0x24) == 0);
    sw(0x00133280, 0);
    sw(0x00137b00, 0);
    sw(0x00134280, 0);
    sw(LEFT + 4, 0xffc);
    sw(0x00137b10, 0);
    sw(LEFT, 0xffc);
    const uint32_t status = 0x00137b00;  // where the IOP reports the stream requests done
    snd_command_sync_bytes(0, 4, reinterpret_cast<const uint8_t*>(&status));
}

// Hands finished replies to their callbacks (the last batch's, or a synchronous call's), ends a
// finished bank load (its callback), sends the current batch when nothing is awaited and it is
// not held, and polls a stream status request. 1 while something is still awaited.
// orig: 0x0012dc80
int32_t snd_flush() {
    if (lw(PEND) != 0 && snd_reply_ready() != 0) {
        if (lw(0x0015ec9c) != 0) {
            const uint32_t cb = lwu(0x0015ece0);
            if (cb != 0) call_callback(cb, lw(0x00133104), ld(0x0015ece8));
            sw(0x0015ece0, 0);
            sw(0x0015ec9c, 0);
        } else {
            const uint32_t other = lw(CUR) != 1 ? 1u : 0u;
            const uint32_t bp = BUFS + other * 4;
            for (int32_t i = 0; i < lw(lwu(bp)); ++i) {
                const uint32_t e = lwu(CBS + other * 4) + static_cast<uint32_t>(i) * 0x10;
                const uint32_t cb = lwu(e);
                if (cb != 0) call_callback(cb, lw(lwu(REPLIES + other * 4) + static_cast<uint32_t>(i) * 4 + 4), ld(e + 8));
            }
        }
    }
    if (lw(LOAD_BUSY) != 0) {
        FlushCache(0);
        const int32_t r = lw(LOAD_RET);
        if (r != -1) {
            const uint32_t cb = lwu(LOAD_CB);
            if (cb == 0) {
                sw(LOAD_RET, 0);
            } else {
                const uint64_t d = ld(LOAD_DATA);
                sw(LOAD_CB, 0);
                sd(LOAD_DATA, 0);
                call_callback(cb, r, d);
                sw(LOAD_RET, 0);
            }
            sw(LOAD_BUSY, 0);
        }
    }
    if (lw(PEND) == 0 && lw(lwu(BUFS + lwu(CUR) * 4)) != 0 && lw(HOLD) == 0) snd_batch_send();
    if (lw(0x0015ec94) != 0) {
        snd_cd_status(1);
        if (lw(0x0015ec98) != 0) {
            sw(0x0015ec94, 0);
            sw(0x0015ec98, 0);
            if (lwu(0x0015ec90) != 0) call_callback(lwu(0x0015ec90), 1, 0);
        }
    }
    return (lw(PEND) != 0 || lw(LOAD_BUSY) != 0) ? 1 : 0;
}

// Whether the awaited reply has arrived (its first and last words -1); 1 when none is awaited.
// orig: 0x0012de70
int32_t snd_reply_ready() {
    FlushCache(0);
    const uint32_t p = lwu(PEND);
    if (p == 0) return 1;
    if (sif_check_stat_rpc(CLIENT) != 0) return 0;
    if (lw(p) == -1 && lw(p + static_cast<uint32_t>(lw(PEND_N)) * 4 + 4) == -1) {
        sw(PEND, 0);
        return 1;
    }
    debug_printf_stub();  // the reply came back incomplete
    return 0;
}

// Awaits a reply of n results in buf (its first and last words cleared).
// orig: 0x0012def8
void snd_reply_wait_set(uint32_t buf, int32_t n) {
    sw(PEND_N, static_cast<uint32_t>(n));
    sw(PEND, buf);
    sw(buf + static_cast<uint32_t>(n) * 4 + 4, 0);
    sw(buf, 0);
}

namespace {
// waits until the client's last call is complete
// orig: (helper) repeated in 0x0012df20
void client_wait(uint32_t client) {
    while (sif_check_stat_rpc(client) != 0) {
        debug_printf_stub();
        snd_flush();
        FlushCache(0);
    }
}
}  // namespace

// Loads a sound bank from the disc (sector `loc`, `offset`) and waits for its handle (0 when a
// load or the CD is busy, error 0x15ec88).
// orig: 0x0012df20
int32_t snd_bank_load_by_loc(uint32_t loc, uint32_t offset) {
    sw(ERR, 0);
    if (lw(LOAD_BUSY) != 0 || snd_cd_status(1) == 1) {
        debug_printf_stub();
        return 0;
    }
    sw(0x00137b44, offset);
    sw(LOAD_RET, 0xffffffffu);
    sw(0x00137b40, loc);
    client_wait(LOADER);
    if (sif_call_rpc(LOADER, 3, 1, 0x00137b40, 8, LOAD_RET, 4, 0, 0) < 0) {
        debug_printf_stub();
        sw(ERR, 0x106);
        return 0;
    }
    while (lw(LOAD_RET) == -1) FlushCache(0);
    return lw(LOAD_RET);
}

// snd_BankLoadFromEE_CB: starts loading the bank at EE address `addr` without waiting; the
// loader's reply (the bank handle) goes to `cb` with `data` from snd_flush.
// orig: 0x0012e088
void snd_bank_load_from_ee_cb(uint32_t addr, uint32_t cb, uint64_t data) {
    sw(ERR, 0);
    if (lw(LOAD_BUSY) != 0) {
        debug_printf_stub();
        return;
    }
    if (snd_cd_status(1) == 1) {
        debug_printf_stub();
        return;
    }
    sw(0x00137b40, addr);
    sw(LOAD_RET, 0xffffffffu);
    sw(LOAD_CB, cb);
    sd(LOAD_DATA, data);
    while (sif_check_stat_rpc(LOADER) != 0) {
        debug_printf_stub();
        snd_flush();
        FlushCache(0);
    }
    sw(LOAD_BUSY, 1);
    sif_call_rpc(LOADER, 0x57, 1, 0x00137b40, 4, LOAD_RET, 4, 0, 0);
}

namespace {
// the stubs' argument words (on the EE stack in the original)
template <size_t N>
void send(uint32_t cmd, const uint32_t (&w)[N], uint32_t cb = 0, uint64_t data = 0) {
    snd_command_bytes(cmd, static_cast<int32_t>(N * 4), reinterpret_cast<const uint8_t*>(w), cb, data);
}
}  // namespace

// orig: 0x0012e1a8
void snd_cmd_08() { snd_command_bytes(8, 0, nullptr, 0, 0); }
// orig: 0x0012e1d8
void snd_cmd_06(uint32_t a) { send(6, {a}); }
// orig: 0x0012e208
void snd_set_master_volume(uint32_t group, int32_t vol) { send(9, {group, static_cast<uint32_t>(vol)}); }
// orig: 0x0012e240
void snd_cmd_0b(uint32_t a) { send(0xb, {a}); }
// orig: 0x0012e280
void snd_cmd_0d(uint32_t a, uint32_t b) { send(0xd, {a, b}); }
// orig: 0x0012e2b8
void snd_cmd_4e(uint32_t a, uint32_t b, uint32_t c) { send(0x4e, {a, b, c}); }

// Plays sound `snd` of `bank`: the voice handle goes to `cb` with `data`.
// orig: 0x0012e308
void snd_play_sound(uint32_t bank, uint32_t snd, int32_t vol, int32_t pan, int32_t pm, int32_t pb, uint32_t cb, uint64_t data) {
    send(0x11, {bank, snd, static_cast<uint32_t>(vol), static_cast<uint32_t>(pan), static_cast<uint32_t>(pm), static_cast<uint32_t>(pb)}, cb, data);
}
// orig: 0x0012e368
void snd_stop_sound(uint32_t handle) { send(0x15, {handle}); }
// orig: 0x0012e3b8
void snd_cmd_18() { snd_command_bytes(0x18, 0, nullptr, 0, 0); }
// orig: 0x0012e3e8
void snd_cmd_16(uint32_t a) { send(0x16, {a}); }
// orig: 0x0012e418
void snd_cmd_17(uint32_t a) { send(0x17, {a}); }
// orig: 0x0012e448
void snd_sound_is_playing(uint32_t handle, uint32_t cb, uint64_t data) { send(0x19, {handle}, cb, data); }
// orig: 0x0012e4c0
void snd_set_sound(uint32_t handle, uint32_t flags, int32_t vol, int32_t pan, int32_t pm, int32_t pb, uint32_t cb, uint64_t data) {
    send(0x21, {handle, flags, static_cast<uint32_t>(vol), static_cast<uint32_t>(pan), static_cast<uint32_t>(pm), static_cast<uint32_t>(pb)}, cb, data);
}

// A command answered at once (the batch flushed first): its single result.
// orig: 0x0012e548
// (`data` is the command's record laid out as EE memory: the ports build it as 32-bit words, so its byte i is at
// ee_byte_addr(i) on a big-endian host)
int32_t snd_command_sync_bytes(uint32_t cmd, int32_t size, const uint8_t* data) {
    for (int32_t i = 0; i < size; ++i) sb(SYNC_SEND + static_cast<uint32_t>(i), data[ee_byte_addr(static_cast<uint32_t>(i))]);
    while (lw(PEND) != 0) {
        snd_flush();
        FlushCache(0);
    }
    snd_reply_wait_set(SYNC_RECV, 1);
    while (sif_check_stat_rpc(CLIENT) != 0) {
        debug_printf_stub();
        snd_flush();
        FlushCache(0);
    }
    if (size != 0) {
        sif_call_rpc(CLIENT, cmd, 1, SYNC_SEND, static_cast<uint32_t>(size), SYNC_RECV, 0xc, 0, 0);
    } else {
        sif_call_rpc(CLIENT, cmd, 1, 0, 0, SYNC_RECV, 0xc, 0, 0);
    }
    while (snd_reply_ready() == 0) {
    }
    const int32_t ret = lw(SYNC_RECV + 4);
    if (lw(lwu(BUFS + lwu(CUR) * 4)) != 0 && lw(HOLD) == 0) snd_batch_send();
    return ret;
}
// orig: 0x0012e548 (the data read from EE memory)
int32_t snd_command_sync(uint32_t cmd, int32_t size, uint32_t data) {
    std::vector<uint8_t> b((static_cast<size_t>(size > 0 ? size : 0) + 3) & ~size_t{3});  // (laid out as EE memory: whole words)
    for (int32_t i = 0; i < size; ++i) b[ee_byte_addr(static_cast<uint32_t>(i))] = lbu(data + static_cast<uint32_t>(i));
    return snd_command_sync_bytes(cmd, size, b.data());
}

// Queues a command in the current batch (sent at once, synchronously, when nothing is batched
// and it has no data nor callback); a full batch (0x100 commands or no room) is flushed first.
// orig: 0x0012e6e0
void snd_command_bytes(uint32_t cmd, int32_t size, const uint8_t* data, uint32_t cb, uint64_t cbdata) {
    if (lw(HOLD) == 0 && lw(PEND) == 0 && size == 0 && cb == 0) {
        snd_reply_wait_set(SYNC_RECV, 1);
        while (sif_check_stat_rpc(CLIENT) != 0) {
            debug_printf_stub();
            snd_flush();
            FlushCache(0);
        }
        sif_call_rpc(CLIENT, cmd, 1, 0, 0, SYNC_RECV, 0xc, 0, 0);
        return;
    }
    int32_t s = size + 4;
    if ((s & 3) != 0) s = s + 4 - (s % 4);
    int32_t tries = 0;
    bool held = false;
    while (lw(lwu(BUFS + lwu(CUR) * 4)) == 0x100 || lw(LEFT + lwu(CUR) * 4) < s) {
        if (lw(HOLD) != 0) {
            sw(HOLD, 0);
            held = true;
        }
        snd_flush();
        if (tries == 1) debug_printf_stub();
        ++tries;
    }
    if (tries != 0) debug_printf_stub();
    if (held) sw(HOLD, 1);
    const uint32_t cur = lwu(CUR);
    uint32_t p = lwu(BUFS + cur * 4) + 0x1000 - lwu(LEFT + cur * 4);
    sh(p, static_cast<uint16_t>(cmd));
    sh(p + 2, static_cast<uint16_t>(size));
    p += 4;
    for (int32_t i = 0; i < size; ++i) sb(p + static_cast<uint32_t>(i), data[ee_byte_addr(static_cast<uint32_t>(i))]);  // (laid out as EE memory)
    sw(LEFT + cur * 4, static_cast<uint32_t>(lw(LEFT + cur * 4) - s));
    const uint32_t e = lwu(CBS + cur * 4) + lwu(lwu(BUFS + cur * 4)) * 0x10;
    sw(e, cb);
    sd(e + 8, cbdata);
    snd_command_added();
}
// orig: 0x0012e6e0 (the data read from EE memory)
void snd_command(uint32_t cmd, int32_t size, uint32_t data, uint32_t cb, uint64_t cbdata) {
    std::vector<uint8_t> b((static_cast<size_t>(size > 0 ? size : 0) + 3) & ~size_t{3});  // (laid out as EE memory: whole words)
    for (int32_t i = 0; i < size; ++i) b[ee_byte_addr(static_cast<uint32_t>(i))] = lbu(data + static_cast<uint32_t>(i));
    snd_command_bytes(cmd, size, b.data(), cb, cbdata);
}

// orig: 0x0012e9a0
void snd_command_added() {
    const uint32_t b = lwu(BUFS + lwu(CUR) * 4);
    sw(b, lwu(b) + 1);
    snd_flush();
}

// Sends the current batch (call 0x4d, replies into its reply buffer) and switches buffers.
// orig: 0x0012e9d8
void snd_batch_send() {
    const uint32_t cur = lwu(CUR);
    snd_reply_wait_set(lwu(REPLIES + cur * 4), lw(lwu(BUFS + cur * 4)));
    while (sif_check_stat_rpc(CLIENT) != 0) {
        debug_printf_stub();
        FlushCache(0);
    }
    const uint32_t c = lwu(CUR);
    const uint32_t buf = lwu(BUFS + c * 4);
    sif_call_rpc(CLIENT, 0x4d, 1, buf, 0x1000 - lwu(LEFT + c * 4), lwu(REPLIES + c * 4), lwu(buf) * 4 + 8, 0, 0);
    const uint32_t n = lwu(CUR) != 1 ? 1u : 0u;
    sw(CUR, n);
    sw(lwu(BUFS + n * 4), 0);
    sw(LEFT + n * 4, 0xffc);
}

// orig: 0x0012eaf0
void snd_hold() { sw(HOLD, 1); }
// orig: 0x0012eb00
void snd_release() {
    sw(HOLD, 0);
    snd_flush();
}

// The disc's state for loads: sceCdSync without streaming; with it, whether the stream system's
// last request finished (0x137b00 clear; 0x15ec98), waited for unless `mode` is 1 (then 1 busy).
// orig: 0x0012ee08
int32_t snd_cd_status(int32_t mode) {
    if (lw(STREAM) == 0) return cd_sync(mode);
    FlushCache(0);
    int32_t v = lw(0x00137b00) == 0 ? 1 : 0;
    sw(0x0015ec98, static_cast<uint32_t>(v));
    if (v == 1) return 0;
    if (mode == 1) return 1;
    while (v == 0) {
        snd_flush();
        FlushCache(0);
        v = lw(0x00137b00) == 0 ? 1 : 0;
        sw(0x0015ec98, static_cast<uint32_t>(v));
    }
    return 0;
}

// The callback of a finished stream request (snd_flush); without streaming the CD's callback.
// The previous one.
// orig: 0x0012ef28
uint32_t snd_set_stream_callback(uint32_t fn) {
    if (lw(STREAM) == 0) return TODO_PORT(0x00120678, "libcdvd: sceCdCallback")(fn);
    const uint32_t old = lwu(0x0015ec90);
    sw(0x0015ec90, fn);
    return old;
}

// orig: 0x0012ef68
void snd_cmd_ef68(uint32_t a, uint32_t b, uint32_t c, uint32_t d, uint32_t e) { send(0x50, {a, b, c, d, e}); }
// orig: 0x0012eca0
void snd_cmd_2e(uint32_t a) { send(0x2e, {a}); }
// orig: 0x0012efa8
void snd_cmd_51(uint32_t a, uint32_t b) { send(0x51, {a, b}); }

// The stream system started (once): any bank load finished, the disc idle, then the IOP's
// answer to command 0x2a (its arguments) 0x15ec8c, non-zero when streams are up.
// orig: 0x0012eb20
int32_t snd_stream_init(uint32_t a, uint32_t b, uint32_t c, uint32_t d) {
    if (lw(STREAM) == 1) return 0;
    if (lw(LOAD_BUSY) != 0) {
        while (snd_flush() != 0) {
        }
    }
    snd_cd_status(0);
    const uint32_t w[4] = {a, b, c, d};
    const int32_t r = snd_command_sync_bytes(0x2a, 0x10, reinterpret_cast<const uint8_t*>(w));
    sw(STREAM, static_cast<uint32_t>(r));
    return r;
}

// orig: 0x0012efe0
void snd_cmd_efe0(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { send(0x10, {a, b, c, d}); }

// orig: 0x0012f1a0
int32_t snd_pitch_from_cents(int32_t c) {
    return static_cast<int32_t>(static_cast<uint32_t>(c) * 0x5f4u) / 0x2e5;
}

// snd_StopAllStreams (command 0x34, no data: sent at once when nothing is batched).
// orig: 0x0012ebd0
void snd_stream_stop_all() { snd_command_bytes(0x34, 0, nullptr, 0, 0); }

// Plays the VAG at sector `loc1` (+ byte offset `off1`; a second channel `loc2` / `off2`),
// volume / pan, volume group, queued after the stream `queue` (0: a new one), flags (1 paused,
// 4 looped, 0x20 ...): command 0x2c, the handle (or 1 when queued) to `cb` with `data`.
// orig: 0x0012ec08
void snd_stream_play(uint32_t loc1, uint32_t loc2, uint32_t off1, uint32_t off2, int32_t vol, int32_t pan, uint32_t group,
                     uint32_t queue, uint32_t flags, uint32_t cb, uint64_t data) {
    const uint32_t w2 = (static_cast<uint32_t>(vol) << 16) | (off1 & 0xffffu);
    const uint32_t w3 = (static_cast<uint32_t>(pan) << 16) | (off2 & 0xffffu);
    send(0x2c, {loc1, loc2, w2, w3, group, queue, flags}, cb, data);
}
// snd_PauseVAGStream (command 0x2d)
// orig: 0x0012ec70
void snd_stream_pause(uint32_t handle) { send(0x2d, {handle}); }
// the stream's time left (1/1024 s, command 0x32) to `cb`
// orig: 0x0012ecd0
void snd_stream_remaining(uint32_t handle, uint32_t cb, uint64_t data) { send(0x32, {handle}, cb, data); }
// whether the stream's first buffer is in and it started (command 0x4f; -1 no such stream) to `cb`
// orig: 0x0012ed00
void snd_stream_started(uint32_t handle, uint32_t cb, uint64_t data) { send(0x4f, {handle}, cb, data); }
// waits until the stream system's disc reads are idle (command 0x36, answered at once)
// orig: 0x0012ed30
int32_t snd_stream_reads_wait(uint32_t a) {
    const uint32_t w[1] = {a};
    return snd_command_sync_bytes(0x36, 4, reinterpret_cast<const uint8_t*>(w));
}

// A disc read for the game (`n` sectors from `lsn` to EE `buf`): without streams sceCdRead;
// with them queued to the stream system's reader (command 0x38) when the disc is free, the
// status word 0x137b00 set until the IOP clears it, its error 0x137b10. 1 when started.
// orig: 0x0012ed58
int32_t snd_stream_cd_read(uint32_t lsn, uint32_t n, uint32_t buf, uint32_t mode) {
    if (lw(STREAM) == 0) return TODO_PORT(0x00121450, "libcdvd: sceCdRead")(lsn, n, buf, mode);
    if (snd_cd_status(1) == 1) return 0;
    sw(0x00137b00, 1);
    sw(0x00137b10, 0);
    const uint32_t w[3] = {lsn, n, buf};
    send(0x38, w);
    sw(0x0015ec94, 1);
    sw(0x0015ec98, 0);
    return 1;
}

// A disc read stopped: with streams the IOP's reader told to break (command 0x37), else
// sceCdBreak. 1 with streams.
// orig: 0x0012eea8
int32_t snd_cd_break() {
    if (lw(STREAM) != 0) {
        snd_command(0x37, 0, 0, 0, 0);
        return 1;
    }
    return TODO_PORT(0x001216c8, "libcdvd: sceCdBreak")();
}

// The last disc read's error: sceCdGetError without streams, else the IOP's word 0x137b10.
// orig: 0x0012eef0
int32_t snd_cd_error() {
    if (lw(STREAM) == 0) return TODO_PORT(0x00121630, "libcdvd: sceCdGetError")();
    return lw(0x00137b10);
}

}  // namespace rac1
