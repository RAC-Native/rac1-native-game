// rac1-link: CRC-32 (zlib's), SHA-1 and HMAC-SHA256, portable C (byte loads, any endianness).
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

uint32_t link_crc32(uint32_t crc, const void *data, size_t len);  // start with 0, as zlib.crc32

typedef struct {
    uint32_t h[5];
    uint64_t len;
    uint8_t buf[64];
    size_t used;
} link_sha1;
void link_sha1_init(link_sha1 *s);
void link_sha1_update(link_sha1 *s, const void *data, size_t len);
void link_sha1_final(link_sha1 *s, uint8_t out[20]);

typedef struct {
    uint32_t h[8];
    uint64_t len;
    uint8_t buf[64];
    size_t used;
} link_sha256;
void link_sha256_init(link_sha256 *s);
void link_sha256_update(link_sha256 *s, const void *data, size_t len);
void link_sha256_final(link_sha256 *s, uint8_t out[32]);
void link_hmac_sha256(const void *key, size_t keylen, const void *msg, size_t msglen, uint8_t out[32]);

void link_hex(const uint8_t *in, size_t n, char *out);  // out: 2n + 1 bytes
int link_unhex(const char *in, uint8_t *out, size_t n);  // 0 on success

#ifdef __cplusplus
}
#endif
