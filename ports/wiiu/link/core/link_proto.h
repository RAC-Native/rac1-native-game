// rac1-link protocol v1 (prototype): the numbers both sides share. pc/rac1_link.py mirrors them; keep the two in step.
//
// One TCP connection per session, PC -> console. Every message is a 12-byte header in network byte order
//   'R' '1' type:u8 flags:u8 length:u32 seq:u32
// followed by `length` bytes. Control payloads are "key=value\n" lines (UTF-8, no '\n' in values).
// DATA's payload is binary: offset:u64, crc32:u32 (zlib's, of the bytes), then at most 1 MiB of bytes.
#pragma once

#define LINK_PROTO_VERSION 1
#define LINK_TCP_PORT 47199
#define LINK_UDP_PORT 47198
#define LINK_HDR_SIZE 12
#define LINK_DATA_HDR 12
#define LINK_CHUNK (1u << 20)              // the largest DATA payload after its 12-byte header
#define LINK_CTRL_MAX (16u << 20)          // the largest control payload (a manifest)
#define LINK_FILE_MAX 0xFFFFFFFFull        // FAT32: 4 GiB - 1
#define LINK_FLUSH_EVERY (16u << 20)       // fsync + ACK every this many bytes written
#define LINK_IDLE_TIMEOUT_S 30             // no byte for this long drops the session (it can be resumed)
#define LINK_DISCOVER_QUERY "RAC1LINK?"    // UDP, PC -> broadcast:47198; answer "RAC1LINK! key=value ..."
#define LINK_DISCOVER_ANSWER "RAC1LINK!"

enum link_msg {
    LINK_HELLO = 1,         // C->PC  proto, name, version, serial, nonce, free, chunk
    LINK_AUTH = 2,          // PC->C  nonce, mac = hex HMAC-SHA256(key, "pc" + nonce_c + nonce_p), client
    LINK_AUTH_OK = 3,       // C->PC  mac = hex HMAC-SHA256(key, "console" + nonce_p + nonce_c)
    LINK_ERROR = 4,         // C->PC  code, msg
    LINK_GET_MANIFEST = 5,  // PC->C
    LINK_MANIFEST = 6,      // C->PC  lines "F\tsize\tsha1\tpath" (committed) / "P\tsize\tsha1\tpath\toffset" (.part)
    LINK_BEGIN_FILE = 7,    // PC->C  path, size, sha1; flags LINK_F_*
    LINK_READY = 8,         // C->PC  offset (where the PC starts: > 0 resumes a .part)
    LINK_DATA = 9,          // PC->C  binary, see above
    LINK_ACK = 10,          // C->PC  offset (contiguous bytes written and fsync'd)
    LINK_RESEND = 11,       // C->PC  offset (a chunk failed its CRC: send again from here)
    LINK_END_FILE = 12,     // PC->C
    LINK_FILE_OK = 13,      // C->PC  sha1, bytes, wall_ms, net_ms, hash_ms, sd_ms, wait_ms, rehash_ms
    LINK_FILE_BAD = 14,     // C->PC  reason
    LINK_BENCH_SD = 15,     // PC->C  mb, chunk_kib, files, fwrite
    LINK_BENCH_RESULT = 16, // C->PC  bytes, write_ms, rename_ms, read_ms, delete_ms
    LINK_CONFIG = 17,       // PC->C  rcvbuf, winscale, sack, noslowstart, rusrbuf (next connection); somemopt (next start)
    LINK_CONFIG_OK = 18,    // C->PC  the values now in force
    LINK_PROGRESS = 19,     // PC->C  text (shown on the TV)
    LINK_PING = 20,
    LINK_PONG = 21,
    LINK_CANCEL = 22,       // PC->C  inside a file: stop, keep the .part (answered with ACK)
    LINK_BYE = 23,
    LINK_QUIT = 24,         // PC->C  close rac1-link and go back to the Wii U Menu (remote tests)
    LINK_LAUNCH = 25,       // PC->C  start the game (sd:/wiiu/apps/rac1-native/rac1-native.rpx); the session ends
};

// BEGIN_FILE flags
#define LINK_F_DISCARD 1  // receive and drop: the network alone (no file, no manifest)
#define LINK_F_NOHASH 2   // skip CRC and SHA-1 (with DISCARD: the network with no CPU work at all)
#define LINK_F_BENCH 4    // write to rac1-native/.link/bench/xfer.bin.part, verify, delete (network + SD)
