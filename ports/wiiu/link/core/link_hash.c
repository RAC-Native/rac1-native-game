// rac1-link: CRC-32, SHA-1, SHA-256, HMAC-SHA256. Plain C from the specifications (RFC 3174, FIPS 180-4,
// RFC 2104); checked against Python's zlib / hashlib / hmac by the host stand-in's --selftest.
#include "link_hash.h"

#include <string.h>

// --- CRC-32 (IEEE 802.3, reflected, as zlib), slicing by 4 ---
static uint32_t crc_tab[4][256];
static int crc_ready;

static void crc_init(void)
{
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = i;
        for (int k = 0; k < 8; k++)
            c = (c & 1) ? 0xEDB88320u ^ (c >> 1) : c >> 1;
        crc_tab[0][i] = c;
    }
    for (uint32_t i = 0; i < 256; i++) {
        uint32_t c = crc_tab[0][i];
        for (int t = 1; t < 4; t++) {
            c = crc_tab[0][c & 0xff] ^ (c >> 8);
            crc_tab[t][i] = c;
        }
    }
    crc_ready = 1;
}

uint32_t link_crc32(uint32_t crc, const void *data, size_t len)
{
    if (!crc_ready)
        crc_init();
    const uint8_t *p = (const uint8_t *)data;
    uint32_t c = ~crc;
    while (len >= 4) {
        c ^= (uint32_t)p[0] | ((uint32_t)p[1] << 8) | ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        c = crc_tab[3][c & 0xff] ^ crc_tab[2][(c >> 8) & 0xff] ^ crc_tab[1][(c >> 16) & 0xff] ^ crc_tab[0][c >> 24];
        p += 4;
        len -= 4;
    }
    while (len--)
        c = crc_tab[0][(c ^ *p++) & 0xff] ^ (c >> 8);
    return ~c;
}

#define ROL(x, n) (((x) << (n)) | ((x) >> (32 - (n))))
#define ROR(x, n) (((x) >> (n)) | ((x) << (32 - (n))))

static uint32_t be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

// --- SHA-1 ---
// Four branch-free rounds of 20 on a rolling 16-word schedule (an in-order core such as Espresso prefers this to an
// 80-word array and a branch per step).
#define SHA1_W(i) (w[(i)&15] = ROL(w[((i) + 13) & 15] ^ w[((i) + 8) & 15] ^ w[((i) + 2) & 15] ^ w[(i)&15], 1))
#define SHA1_STEP(f, k, wi)                                                                                            \
    do {                                                                                                               \
        uint32_t t = ROL(a, 5) + (f) + e + (k) + (wi);                                                               \
        e = d;                                                                                                         \
        d = c;                                                                                                         \
        c = ROL(bb, 30);                                                                                               \
        bb = a;                                                                                                        \
        a = t;                                                                                                         \
    } while (0)

static void sha1_block(uint32_t h[5], const uint8_t *b)
{
    uint32_t w[16];
    for (int i = 0; i < 16; i++)
        w[i] = be32(b + 4 * i);
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4];
    int i = 0;
    for (; i < 16; i++)
        SHA1_STEP(d ^ (bb & (c ^ d)), 0x5A827999, w[i]);
    for (; i < 20; i++)
        SHA1_STEP(d ^ (bb & (c ^ d)), 0x5A827999, SHA1_W(i));
    for (; i < 40; i++)
        SHA1_STEP(bb ^ c ^ d, 0x6ED9EBA1, SHA1_W(i));
    for (; i < 60; i++)
        SHA1_STEP((bb & c) | (d & (bb | c)), 0x8F1BBCDC, SHA1_W(i));
    for (; i < 80; i++)
        SHA1_STEP(bb ^ c ^ d, 0xCA62C1D6, SHA1_W(i));
    h[0] += a;
    h[1] += bb;
    h[2] += c;
    h[3] += d;
    h[4] += e;
}

void link_sha1_init(link_sha1 *s)
{
    static const uint32_t iv[5] = {0x67452301, 0xEFCDAB89, 0x98BADCFE, 0x10325476, 0xC3D2E1F0};
    memcpy(s->h, iv, sizeof iv);
    s->len = 0;
    s->used = 0;
}

void link_sha1_update(link_sha1 *s, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    s->len += len;
    if (s->used) {
        size_t n = 64 - s->used < len ? 64 - s->used : len;
        memcpy(s->buf + s->used, p, n);
        s->used += n;
        p += n;
        len -= n;
        if (s->used == 64) {
            sha1_block(s->h, s->buf);
            s->used = 0;
        }
    }
    while (len >= 64) {
        sha1_block(s->h, p);
        p += 64;
        len -= 64;
    }
    if (len) {
        memcpy(s->buf, p, len);
        s->used = len;
    }
}

static void md_pad(uint8_t *buf, size_t *used, uint64_t bits, void (*block)(void *, const uint8_t *), void *st)
{
    buf[(*used)++] = 0x80;
    if (*used > 56) {
        memset(buf + *used, 0, 64 - *used);
        block(st, buf);
        *used = 0;
    }
    memset(buf + *used, 0, 56 - *used);
    put_be32(buf + 56, (uint32_t)(bits >> 32));
    put_be32(buf + 60, (uint32_t)bits);
    block(st, buf);
    *used = 0;
}

static void sha1_block_v(void *st, const uint8_t *b) { sha1_block((uint32_t *)st, b); }

void link_sha1_final(link_sha1 *s, uint8_t out[20])
{
    md_pad(s->buf, &s->used, s->len * 8, sha1_block_v, s->h);
    for (int i = 0; i < 5; i++)
        put_be32(out + 4 * i, s->h[i]);
}

// --- SHA-256 ---
static const uint32_t K256[64] = {
    0x428a2f98, 0x71374491, 0xb5c0fbcf, 0xe9b5dba5, 0x3956c25b, 0x59f111f1, 0x923f82a4, 0xab1c5ed5,
    0xd807aa98, 0x12835b01, 0x243185be, 0x550c7dc3, 0x72be5d74, 0x80deb1fe, 0x9bdc06a7, 0xc19bf174,
    0xe49b69c1, 0xefbe4786, 0x0fc19dc6, 0x240ca1cc, 0x2de92c6f, 0x4a7484aa, 0x5cb0a9dc, 0x76f988da,
    0x983e5152, 0xa831c66d, 0xb00327c8, 0xbf597fc7, 0xc6e00bf3, 0xd5a79147, 0x06ca6351, 0x14292967,
    0x27b70a85, 0x2e1b2138, 0x4d2c6dfc, 0x53380d13, 0x650a7354, 0x766a0abb, 0x81c2c92e, 0x92722c85,
    0xa2bfe8a1, 0xa81a664b, 0xc24b8b70, 0xc76c51a3, 0xd192e819, 0xd6990624, 0xf40e3585, 0x106aa070,
    0x19a4c116, 0x1e376c08, 0x2748774c, 0x34b0bcb5, 0x391c0cb3, 0x4ed8aa4a, 0x5b9cca4f, 0x682e6ff3,
    0x748f82ee, 0x78a5636f, 0x84c87814, 0x8cc70208, 0x90befffa, 0xa4506ceb, 0xbef9a3f7, 0xc67178f2,
};

static void sha256_block(uint32_t h[8], const uint8_t *b)
{
    uint32_t w[64];
    for (int i = 0; i < 16; i++)
        w[i] = be32(b + 4 * i);
    for (int i = 16; i < 64; i++) {
        uint32_t s0 = ROR(w[i - 15], 7) ^ ROR(w[i - 15], 18) ^ (w[i - 15] >> 3);
        uint32_t s1 = ROR(w[i - 2], 17) ^ ROR(w[i - 2], 19) ^ (w[i - 2] >> 10);
        w[i] = w[i - 16] + s0 + w[i - 7] + s1;
    }
    uint32_t a = h[0], bb = h[1], c = h[2], d = h[3], e = h[4], f = h[5], g = h[6], hh = h[7];
    for (int i = 0; i < 64; i++) {
        uint32_t S1 = ROR(e, 6) ^ ROR(e, 11) ^ ROR(e, 25);
        uint32_t ch = (e & f) ^ (~e & g);
        uint32_t t1 = hh + S1 + ch + K256[i] + w[i];
        uint32_t S0 = ROR(a, 2) ^ ROR(a, 13) ^ ROR(a, 22);
        uint32_t mj = (a & bb) ^ (a & c) ^ (bb & c);
        uint32_t t2 = S0 + mj;
        hh = g;
        g = f;
        f = e;
        e = d + t1;
        d = c;
        c = bb;
        bb = a;
        a = t1 + t2;
    }
    h[0] += a;
    h[1] += bb;
    h[2] += c;
    h[3] += d;
    h[4] += e;
    h[5] += f;
    h[6] += g;
    h[7] += hh;
}

void link_sha256_init(link_sha256 *s)
{
    static const uint32_t iv[8] = {0x6a09e667, 0xbb67ae85, 0x3c6ef372, 0xa54ff53a,
                                   0x510e527f, 0x9b05688c, 0x1f83d9ab, 0x5be0cd19};
    memcpy(s->h, iv, sizeof iv);
    s->len = 0;
    s->used = 0;
}

void link_sha256_update(link_sha256 *s, const void *data, size_t len)
{
    const uint8_t *p = (const uint8_t *)data;
    s->len += len;
    while (len) {
        size_t n = 64 - s->used < len ? 64 - s->used : len;
        memcpy(s->buf + s->used, p, n);
        s->used += n;
        p += n;
        len -= n;
        if (s->used == 64) {
            sha256_block(s->h, s->buf);
            s->used = 0;
        }
    }
}

static void sha256_block_v(void *st, const uint8_t *b) { sha256_block((uint32_t *)st, b); }

void link_sha256_final(link_sha256 *s, uint8_t out[32])
{
    md_pad(s->buf, &s->used, s->len * 8, sha256_block_v, s->h);
    for (int i = 0; i < 8; i++)
        put_be32(out + 4 * i, s->h[i]);
}

void link_hmac_sha256(const void *key, size_t keylen, const void *msg, size_t msglen, uint8_t out[32])
{
    uint8_t k[64] = {0}, pad[64], inner[32];
    if (keylen > 64) {
        link_sha256 s;
        link_sha256_init(&s);
        link_sha256_update(&s, key, keylen);
        link_sha256_final(&s, k);
    } else {
        memcpy(k, key, keylen);
    }
    link_sha256 s;
    for (int i = 0; i < 64; i++)
        pad[i] = k[i] ^ 0x36;
    link_sha256_init(&s);
    link_sha256_update(&s, pad, 64);
    link_sha256_update(&s, msg, msglen);
    link_sha256_final(&s, inner);
    for (int i = 0; i < 64; i++)
        pad[i] = k[i] ^ 0x5c;
    link_sha256_init(&s);
    link_sha256_update(&s, pad, 64);
    link_sha256_update(&s, inner, 32);
    link_sha256_final(&s, out);
}

void link_hex(const uint8_t *in, size_t n, char *out)
{
    static const char d[] = "0123456789abcdef";
    for (size_t i = 0; i < n; i++) {
        out[2 * i] = d[in[i] >> 4];
        out[2 * i + 1] = d[in[i] & 15];
    }
    out[2 * n] = 0;
}

static int hexval(char c)
{
    if (c >= '0' && c <= '9')
        return c - '0';
    if (c >= 'a' && c <= 'f')
        return c - 'a' + 10;
    if (c >= 'A' && c <= 'F')
        return c - 'A' + 10;
    return -1;
}

int link_unhex(const char *in, uint8_t *out, size_t n)
{
    for (size_t i = 0; i < n; i++) {
        int a = hexval(in[2 * i]), b = a < 0 ? -1 : hexval(in[2 * i + 1]);
        if (a < 0 || b < 0)
            return -1;
        out[i] = (uint8_t)(a << 4 | b);
    }
    return in[2 * n] == 0 || in[2 * n] == '\n' || in[2 * n] == ' ' ? 0 : -1;
}
