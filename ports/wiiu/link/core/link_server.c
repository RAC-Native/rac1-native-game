// rac1-link: the receiver. See link_proto.h for the wire format and ../README.md for the design.
//
// Threads: discovery (UDP 47198), net (TCP 47199: accept, one session at a time, receive), writer (one per session:
// writes the chunks the net thread received, fsyncs every 16 MiB). The net thread computes CRC and SHA-1 while the
// writer writes, so the network and the card overlap and the slower of the two sets the speed.
//
// On the card: `path.part` (written at offsets, 64-byte-aligned 1 MiB buffers), fsync, SHA-1 check, rename to `path`
// (an existing `path` goes to `path.old` first and is removed after). rac1-native/.link/manifest.txt lists committed
// files ("F") and large files in progress ("P", for resume). Writes only under rac1-native/ (not .link/) and
// wiiu/apps/rac1-native/.
#define _GNU_SOURCE
#include "link_server.h"

#include "link_hash.h"
#include "link_proto.h"

#include <arpa/inet.h>
#include <errno.h>
#include <fcntl.h>
#include <malloc.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <pthread.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/select.h>
#include <sys/socket.h>
#include <sys/stat.h>
#include <sys/statvfs.h>
#include <sys/types.h>
#include <unistd.h>

#ifndef MSG_NOSIGNAL
#define MSG_NOSIGNAL 0
#endif
#ifndef O_BINARY
#define O_BINARY 0
#endif

#define NSLOTS 6
#define ALIGN 64
#define SMALL_FILE (8u << 20)  // smaller files get no "P" manifest entry (resending them is cheaper)

// ---------------------------------------------------------------------------------------------------------------
// Paths and peers

static int ieq_prefix(const char *s, const char *p)
{
    for (; *p; s++, p++) {
        char a = *s, b = *p;
        if (a >= 'A' && a <= 'Z')
            a += 32;
        if (a != b)
            return 0;
    }
    return 1;
}

// A relative path under one of the two folders: "rac1-native/..." (except ".link", our own state) or
// "wiiu/apps/rac1-native/...". Components: no "", ".", "..", no leading/trailing space or trailing dot (FAT would
// alias them), no FAT-invalid or control characters, no ".part"/".old" suffix (ours). At most 240 bytes.
int link_path_ok(const char *rel)
{
    size_t n = strlen(rel);
    if (n == 0 || n > 240 || rel[0] == '/')
        return 0;
    for (size_t i = 0; i < n; i++) {
        unsigned char c = (unsigned char)rel[i];
        if (c < 0x20 || c >= 0x7f || strchr("\\:*?\"<>|", c))
            return 0;
    }
    const char *rest;
    if (strncmp(rel, "rac1-native/", 12) == 0)
        rest = rel + 12;
    else if (strncmp(rel, "wiiu/apps/rac1-native/", 22) == 0)
        rest = rel + 22;
    else
        return 0;
    if (!*rest || (ieq_prefix(rest, ".link") && (rest[5] == '/' || rest[5] == 0)))
        return 0;
    const char *p = rel;
    while (*p) {
        const char *e = strchr(p, '/');
        size_t len = e ? (size_t)(e - p) : strlen(p);
        if (len == 0 || (len == 1 && p[0] == '.') || (len == 2 && p[0] == '.' && p[1] == '.'))
            return 0;
        if (p[0] == ' ' || p[len - 1] == ' ' || p[len - 1] == '.')
            return 0;
        if (!e)
            break;
        p = e + 1;
    }
    if (rel[n - 1] == '/')
        return 0;
    if ((n >= 5 && ieq_prefix(rel + n - 5, ".part")) || (n >= 4 && ieq_prefix(rel + n - 4, ".old")))
        return 0;
    return 1;
}

int link_peer_ok(uint32_t ip)
{
    return (ip >> 24) == 127 || (ip >> 24) == 10 || (ip >> 20) == (172u << 4 | 1) || (ip >> 16) == (192u << 8 | 168) ||
           (ip >> 16) == (169u << 8 | 254);
}

// ---------------------------------------------------------------------------------------------------------------
// Small helpers

static int kv_get(const char *text, const char *key, char *out, size_t outsz)
{
    size_t kl = strlen(key);
    const char *p = text;
    while (p && *p) {
        if (strncmp(p, key, kl) == 0 && p[kl] == '=') {
            const char *v = p + kl + 1, *e = strchr(v, '\n');
            size_t len = e ? (size_t)(e - v) : strlen(v);
            if (len >= outsz)
                return -1;
            memcpy(out, v, len);
            out[len] = 0;
            return 0;
        }
        p = strchr(p, '\n');
        if (p)
            p++;
    }
    return -1;
}

static long long kv_int(const char *text, const char *key, long long def)
{
    char v[32];
    if (kv_get(text, key, v, sizeof v) != 0)
        return def;
    return strtoll(v, NULL, 10);
}

static void put_be32(uint8_t *p, uint32_t v)
{
    p[0] = (uint8_t)(v >> 24);
    p[1] = (uint8_t)(v >> 16);
    p[2] = (uint8_t)(v >> 8);
    p[3] = (uint8_t)v;
}

static uint32_t get_be32(const uint8_t *p)
{
    return ((uint32_t)p[0] << 24) | ((uint32_t)p[1] << 16) | ((uint32_t)p[2] << 8) | p[3];
}

static int mkdirs_for(const char *full, size_t root_len)
{
    char tmp[640];
    snprintf(tmp, sizeof tmp, "%s", full);
    for (char *p = tmp + root_len + 1; *p; p++) {
        if (*p == '/') {
            *p = 0;
            if (mkdir(tmp, 0777) != 0 && errno != EEXIST) {
                struct stat st;
                if (stat(tmp, &st) != 0 || !S_ISDIR(st.st_mode))
                    return -1;
            }
            *p = '/';
        }
    }
    return 0;
}

static uint64_t free_bytes(const char *root)
{
    struct statvfs v;
    char path[300];
    snprintf(path, sizeof path, "%s/", root);
    if (statvfs(path, &v) != 0)
        return 0;
    uint64_t bs = v.f_frsize ? v.f_frsize : v.f_bsize;
    return (uint64_t)v.f_bavail * bs;
}

// dst replaced by src: dst -> dst.old, src -> dst, remove dst.old (AromaUpdater's order; FAT rename does not replace)
static int replace_file(const char *src, const char *dst)
{
    char old[520];
    snprintf(old, sizeof old, "%s.old", dst);
    struct stat st;
    int had = stat(dst, &st) == 0;
    if (had) {
        unlink(old);
        if (rename(dst, old) != 0)
            return -1;
    }
    if (rename(src, dst) != 0) {
        if (had)
            rename(old, dst);
        return -1;
    }
    if (had)
        unlink(old);
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Manifest

typedef struct mentry {
    char kind;  // 'F' committed, 'P' in progress
    uint64_t size;
    char sha1[41];
    char *path;
} mentry;

typedef struct manifest {
    mentry *e;
    size_t n, cap;
    int dirty;
    uint64_t saved_us;
} manifest;

static mentry *man_find(manifest *m, const char *path)
{
    for (size_t i = 0; i < m->n; i++)
        if (strcmp(m->e[i].path, path) == 0)
            return &m->e[i];
    return NULL;
}

static void man_set(manifest *m, char kind, const char *path, uint64_t size, const char *sha1)
{
    mentry *e = man_find(m, path);
    if (!e) {
        if (m->n == m->cap) {
            m->cap = m->cap ? m->cap * 2 : 256;
            m->e = (mentry *)realloc(m->e, m->cap * sizeof *m->e);
        }
        e = &m->e[m->n++];
        e->path = strdup(path);
    }
    e->kind = kind;
    e->size = size;
    snprintf(e->sha1, sizeof e->sha1, "%s", sha1);
    m->dirty = 1;
}

static void man_remove(manifest *m, const char *path)
{
    mentry *e = man_find(m, path);
    if (!e)
        return;
    free(e->path);
    *e = m->e[--m->n];
    m->dirty = 1;
}

static void man_free(manifest *m)
{
    for (size_t i = 0; i < m->n; i++)
        free(m->e[i].path);
    free(m->e);
    memset(m, 0, sizeof *m);
}

static void man_load(manifest *m, const char *root)
{
    char path[300], line[600];
    snprintf(path, sizeof path, "%s/rac1-native/.link/manifest.txt", root);
    FILE *f = fopen(path, "r");
    if (!f)
        return;
    while (fgets(line, sizeof line, f)) {
        char kind, sha1[41], p[512];
        unsigned long long size;
        line[strcspn(line, "\r\n")] = 0;
        if (sscanf(line, "%c\t%llu\t%40s\t%511[^\n]", &kind, &size, sha1, p) == 4 && (kind == 'F' || kind == 'P') &&
            link_path_ok(p))
            man_set(m, kind, p, size, sha1);
    }
    fclose(f);
    m->dirty = 0;
}

static int man_save(manifest *m, const char *root)
{
    char dir[300], tmp[320], path[320];
    snprintf(dir, sizeof dir, "%s/rac1-native/.link", root);
    snprintf(tmp, sizeof tmp, "%s/manifest.new", dir);
    snprintf(path, sizeof path, "%s/manifest.txt", dir);
    FILE *f = fopen(tmp, "w");
    if (!f)
        return -1;
    for (size_t i = 0; i < m->n; i++)
        fprintf(f, "%c\t%llu\t%s\t%s\n", m->e[i].kind, (unsigned long long)m->e[i].size, m->e[i].sha1, m->e[i].path);
    fflush(f);
    fsync(fileno(f));
    fclose(f);
    if (replace_file(tmp, path) != 0)
        return -1;
    m->dirty = 0;
    m->saved_us = lp_now_us();
    return 0;
}

// ---------------------------------------------------------------------------------------------------------------
// Server state

struct link_server {
    link_options o;
    char code[8];
    char serial[20];
    pthread_t net_thread, disc_thread;
    int started;
    volatile int quit, paused;
    pthread_mutex_t lock;  // status, socket options
    link_status st;
    // socket options for the next connection (CONFIG)
    int rcvbuf, winscale, sack, noslowstart, rusrbuf, cfg_gen;
    int auth_failures;
    uint64_t locked_until_us;
    manifest man;
};

static void status_set(link_server *s, int state, const char *file, uint64_t done, uint64_t total)
{
    pthread_mutex_lock(&s->lock);
    s->st.state = state;
    if (file) {
        snprintf(s->st.file, sizeof s->st.file, "%s", file);
        s->st.file_start_us = lp_now_us();
    }
    s->st.done = done;
    s->st.total = total;
    pthread_mutex_unlock(&s->lock);
}

void link_server_status(link_server *s, link_status *out)
{
    pthread_mutex_lock(&s->lock);
    *out = s->st;
    pthread_mutex_unlock(&s->lock);
}

const char *link_server_code(link_server *s) { return s->code; }

void link_server_pause(link_server *s, int on) { s->paused = on; }

int link_cfg_int(const char *root, const char *key, int def)
{
    char path[300], line[128];
    snprintf(path, sizeof path, "%s/rac1-native/.link/link.cfg", root);
    FILE *f = fopen(path, "r");
    if (!f)
        return def;
    int v = def;
    size_t kl = strlen(key);
    while (fgets(line, sizeof line, f))
        if (strncmp(line, key, kl) == 0 && line[kl] == '=')
            v = atoi(line + kl + 1);
    fclose(f);
    return v;
}

static void cfg_write(link_server *s, int somemopt, int somemopt_big)
{
    char path[300];
    snprintf(path, sizeof path, "%s/rac1-native/.link/link.cfg", s->o.sd_root);
    FILE *f = fopen(path, "w");
    if (!f)
        return;
    fprintf(f, "# rac1-link settings (written by CONFIG); read at start\nsomemopt=%d\nsomemopt_big=%d\n"
               "rcvbuf=%d\nwinscale=%d\nsack=%d\nnoslowstart=%d\n",
            somemopt, somemopt_big, s->rcvbuf, s->winscale, s->sack, s->noslowstart);
    fclose(f);
}

static void apply_sockopts(link_server *s, int fd)
{
    int one = 1, v;
    pthread_mutex_lock(&s->lock);
    int rcvbuf = s->rcvbuf, winscale = s->winscale, sack = s->sack, nss = s->noslowstart, rus = s->rusrbuf;
    pthread_mutex_unlock(&s->lock);
    if (rcvbuf > 0)
        setsockopt(fd, SOL_SOCKET, SO_RCVBUF, &rcvbuf, sizeof rcvbuf);
#ifdef SO_WINSCALE
    if (winscale) {
        v = 1;
        setsockopt(fd, SOL_SOCKET, SO_WINSCALE, &v, sizeof v);
    }
#endif
#ifdef SO_TCPSACK
    if (sack) {
        v = 1;
        setsockopt(fd, SOL_SOCKET, SO_TCPSACK, &v, sizeof v);
    }
#endif
#ifdef SO_NOSLOWSTART
    if (nss) {
        v = 1;
        setsockopt(fd, SOL_SOCKET, SO_NOSLOWSTART, &v, sizeof v);
    }
#endif
#ifdef SO_RUSRBUF
    if (rus) {
        v = 1;
        setsockopt(fd, SOL_SOCKET, SO_RUSRBUF, &v, sizeof v);
    }
#endif
    setsockopt(fd, IPPROTO_TCP, TCP_NODELAY, &one, sizeof one);  // our ACKs are small
    (void)winscale, (void)sack, (void)nss, (void)rus, (void)v;
}

// ---------------------------------------------------------------------------------------------------------------
// Session

typedef struct slot {
    uint8_t *buf;
    uint32_t len;
    int op;  // 0 write, 1 flush, 2 quit
    int fd;
    uint64_t end_off;
} slot;

typedef struct sess {
    link_server *srv;
    int fd;
    uint32_t seq;
    int authed;
    char nonce_c[33];
    char peer[48];
    uint8_t *scratch;  // aligned, LINK_CHUNK
    // writer ring
    slot ring[NSLOTS];
    int head, tail, count;
    pthread_mutex_t m;
    pthread_cond_t cv_free, cv_work;
    pthread_t writer;
    int writer_err;  // errno of a failed write
    uint64_t flushed;
    uint64_t sd_us;
} sess;

static void *writer_main(void *arg)
{
    sess *x = (sess *)arg;
    lp_thread_role(LP_ROLE_WRITER);
    for (;;) {
        pthread_mutex_lock(&x->m);
        while (x->count == 0)
            pthread_cond_wait(&x->cv_work, &x->m);
        slot sl = x->ring[x->tail];
        int err = x->writer_err;
        pthread_mutex_unlock(&x->m);
        if (sl.op == 2)
            break;
        uint64_t t0 = lp_now_us();
        if (!err) {
            if (sl.op == 0) {
                uint32_t done = 0;
                while (done < sl.len) {
                    ssize_t w = write(sl.fd, sl.buf + done, sl.len - done);
                    if (w <= 0) {
                        err = w < 0 ? errno : ENOSPC;
                        break;
                    }
                    done += (uint32_t)w;
                }
            } else if (fsync(sl.fd) != 0) {
                err = errno;
            }
        }
        uint64_t dt = lp_now_us() - t0;
        pthread_mutex_lock(&x->m);
        x->sd_us += dt;
        if (err && !x->writer_err)
            x->writer_err = err;
        if (sl.op == 1 && !err)
            x->flushed = sl.end_off;
        x->tail = (x->tail + 1) % NSLOTS;
        x->count--;
        pthread_cond_broadcast(&x->cv_free);
        pthread_mutex_unlock(&x->m);
    }
    return NULL;
}

static slot *ring_acquire(sess *x)
{
    pthread_mutex_lock(&x->m);
    while (x->count == NSLOTS)
        pthread_cond_wait(&x->cv_free, &x->m);
    slot *sl = &x->ring[x->head];
    pthread_mutex_unlock(&x->m);
    return sl;
}

static void ring_push(sess *x)
{
    pthread_mutex_lock(&x->m);
    x->head = (x->head + 1) % NSLOTS;
    x->count++;
    pthread_cond_signal(&x->cv_work);
    pthread_mutex_unlock(&x->m);
}

static void ring_op(sess *x, int op, int fd, uint64_t end_off)
{
    slot *sl = ring_acquire(x);
    sl->op = op;
    sl->fd = fd;
    sl->len = 0;
    sl->end_off = end_off;
    ring_push(x);
}

static void ring_drain(sess *x)
{
    pthread_mutex_lock(&x->m);
    while (x->count > 0)
        pthread_cond_wait(&x->cv_free, &x->m);
    pthread_mutex_unlock(&x->m);
}

// Wait for readability, 250 ms at a time, checking quit. 1 ready, 0 idle timeout, -1 quit/error.
static int wait_readable(sess *x, int timeout_s)
{
    uint64_t start = lp_now_us();
    for (;;) {
        if (x->srv->quit || x->srv->paused)  // HOME: drop the session (the .part is kept for resume)
            return -1;
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(x->fd, &rs);
        struct timeval tv = {0, 250000};
        int r = select(x->fd + 1, &rs, NULL, NULL, &tv);
        if (r > 0)
            return 1;
        if (r < 0 && errno != EINTR)
            return -1;
        if (lp_now_us() - start > (uint64_t)timeout_s * 1000000u)
            return 0;
    }
}

static int recv_all(sess *x, void *buf, size_t n)
{
    uint8_t *p = (uint8_t *)buf;
    while (n) {
        if (wait_readable(x, LINK_IDLE_TIMEOUT_S) <= 0)
            return -1;
        ssize_t r = recv(x->fd, p, n, 0);
        if (r <= 0) {
            if (r < 0 && (errno == EINTR || errno == EAGAIN))
                continue;
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

static int send_all(int fd, const void *buf, size_t n)
{
    const uint8_t *p = (const uint8_t *)buf;
    while (n) {
        ssize_t r = send(fd, p, n, MSG_NOSIGNAL);
        if (r <= 0) {
            if (r < 0 && errno == EINTR)
                continue;
            return -1;
        }
        p += r;
        n -= (size_t)r;
    }
    return 0;
}

static int send_msg(sess *x, int type, int flags, const void *payload, uint32_t len)
{
    uint8_t h[LINK_HDR_SIZE] = {'R', '1', (uint8_t)type, (uint8_t)flags};
    put_be32(h + 4, len);
    put_be32(h + 8, x->seq++);
    if (send_all(x->fd, h, sizeof h) != 0)
        return -1;
    return len ? send_all(x->fd, payload, len) : 0;
}

static int send_kv(sess *x, int type, const char *fmt, ...) __attribute__((format(printf, 3, 4)));
static int send_kv(sess *x, int type, const char *fmt, ...)
{
    char buf[1024];
    va_list ap;
    va_start(ap, fmt);
    int n = vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (n < 0 || n >= (int)sizeof buf)
        n = (int)sizeof buf - 1;
    return send_msg(x, type, 0, buf, (uint32_t)n);
}

typedef struct hdr {
    int type, flags;
    uint32_t len;
} hdr;

static int recv_hdr(sess *x, hdr *h)
{
    uint8_t b[LINK_HDR_SIZE];
    if (recv_all(x, b, sizeof b) != 0)
        return -1;
    if (b[0] != 'R' || b[1] != '1')
        return -1;
    h->type = b[2];
    h->flags = b[3];
    h->len = get_be32(b + 4);
    return 0;
}

// A control payload, NUL-terminated. Caller frees.
static char *recv_ctrl(sess *x, uint32_t len)
{
    if (len > 65536)
        return NULL;
    char *p = (char *)malloc(len + 1);
    if (!p || recv_all(x, p, len) != 0) {
        free(p);
        return NULL;
    }
    p[len] = 0;
    return p;
}

static int skip_bytes(sess *x, uint32_t len)
{
    while (len) {
        uint32_t n = len > LINK_CHUNK ? LINK_CHUNK : len;
        if (recv_all(x, x->scratch, n) != 0)
            return -1;
        len -= n;
    }
    return 0;
}

static void derive_key(const char *code, uint8_t key[32])
{
    char k[64];
    int n = snprintf(k, sizeof k, "rac1-link pairing v1:%s", code);
    link_sha256 s;
    link_sha256_init(&s);
    link_sha256_update(&s, k, (size_t)n);
    link_sha256_final(&s, key);
}

static int do_auth(sess *x, const char *body)
{
    link_server *s = x->srv;
    char nonce_p[40], mac[70], expect[65], msg[128];
    uint8_t key[32], h[32];
    if (lp_now_us() < s->locked_until_us) {
        send_kv(x, LINK_ERROR, "code=locked\nmsg=too many wrong pairing codes, wait a minute\n");
        return -1;
    }
    if (kv_get(body, "nonce", nonce_p, sizeof nonce_p) != 0 || strlen(nonce_p) != 32 ||
        kv_get(body, "mac", mac, sizeof mac) != 0) {
        send_kv(x, LINK_ERROR, "code=proto\nmsg=bad AUTH\n");
        return -1;
    }
    derive_key(s->code, key);
    int n = snprintf(msg, sizeof msg, "pc%s%s", x->nonce_c, nonce_p);
    link_hmac_sha256(key, 32, msg, (size_t)n, h);
    link_hex(h, 32, expect);
    int diff = 0;
    for (int i = 0; i < 64; i++)
        diff |= expect[i] ^ mac[i];
    if (diff || strlen(mac) != 64) {
        if (++s->auth_failures >= 5) {
            s->locked_until_us = lp_now_us() + 60000000u;
            s->auth_failures = 0;
        }
        lp_log("pairing refused from %s (wrong code)", x->peer);
        send_kv(x, LINK_ERROR, "code=auth\nmsg=wrong pairing code\n");
        lp_sleep_ms(1000);  // slow down guessing
        return -1;
    }
    s->auth_failures = 0;
    n = snprintf(msg, sizeof msg, "console%s%s", nonce_p, x->nonce_c);
    link_hmac_sha256(key, 32, msg, (size_t)n, h);
    link_hex(h, 32, expect);
    x->authed = 1;
    return send_kv(x, LINK_AUTH_OK, "mac=%s\n", expect);
}

static int do_manifest(sess *x)
{
    link_server *s = x->srv;
    size_t cap = 4096, len = 0;
    char *out = (char *)malloc(cap);
    for (size_t i = 0; i < s->man.n; i++) {
        mentry *e = &s->man.e[i];
        char line[700];
        int n;
        if (e->kind == 'P') {
            char full[600];
            struct stat st;
            snprintf(full, sizeof full, "%s/%s.part", s->o.sd_root, e->path);
            uint64_t have = stat(full, &st) == 0 ? (uint64_t)st.st_size : 0;
            have = have > e->size ? 0 : have & ~(uint64_t)(LINK_CHUNK - 1);
            n = snprintf(line, sizeof line, "P\t%llu\t%s\t%s\t%llu\n", (unsigned long long)e->size, e->sha1, e->path,
                         (unsigned long long)have);
        } else {
            // Installed only if it is in the manifest *and* on the card with that size (FAT has no journal).
            char full[600];
            struct stat st;
            snprintf(full, sizeof full, "%s/%s", s->o.sd_root, e->path);
            if (stat(full, &st) != 0 || (uint64_t)st.st_size != e->size)
                continue;
            n = snprintf(line, sizeof line, "F\t%llu\t%s\t%s\n", (unsigned long long)e->size, e->sha1, e->path);
        }
        if (len + (size_t)n + 1 > cap) {
            while (len + (size_t)n + 1 > cap)
                cap *= 2;
            out = (char *)realloc(out, cap);
        }
        memcpy(out + len, line, (size_t)n);
        len += (size_t)n;
    }
    int r = send_msg(x, LINK_MANIFEST, 0, out, (uint32_t)len);
    free(out);
    return r;
}

static int file_bad(sess *x, const char *reason)
{
    x->srv->st.files_bad++;
    return send_kv(x, LINK_FILE_BAD, "reason=%s\n", reason);
}

// BEGIN_FILE ... END_FILE. Returns 0 to keep the session, -1 to close it.
static int do_file(sess *x, const char *body, int flags)
{
    link_server *s = x->srv;
    const char *root = s->o.sd_root;
    char path[300], sha_hex[48];
    const int discard = flags & LINK_F_DISCARD, nohash = flags & LINK_F_NOHASH, bench = flags & LINK_F_BENCH;
    if (kv_get(body, "sha1", sha_hex, sizeof sha_hex) != 0)
        sha_hex[0] = 0;
    long long sizel = kv_int(body, "size", -1);
    if (sizel < 0 || (unsigned long long)sizel > LINK_FILE_MAX)
        return send_kv(x, LINK_ERROR, "code=size\nmsg=bad size (FAT32 limit 4 GiB - 1)\n");
    uint64_t size = (uint64_t)sizel;
    if (bench)
        snprintf(path, sizeof path, "rac1-native/.link/bench/xfer.bin");
    else if (discard)
        snprintf(path, sizeof path, "(discard)");
    else if (kv_get(body, "path", path, sizeof path) != 0 || !link_path_ok(path)) {
        lp_log("refused path: %.120s", kv_get(body, "path", path, sizeof path) == 0 ? path : "(none)");
        return send_kv(x, LINK_ERROR, "code=path\nmsg=path not allowed\n");
    }
    if (!nohash && strlen(sha_hex) != 40)
        return send_kv(x, LINK_ERROR, "code=proto\nmsg=missing sha1\n");

    char full[600], part[610];
    snprintf(full, sizeof full, "%s/%s", root, path);
    snprintf(part, sizeof part, "%s.part", full);
    uint64_t off = 0, rehash_us = 0;
    int fd = -1;
    link_sha1 sha;
    link_sha1_init(&sha);

    if (!discard) {
        mentry *e = bench ? NULL : man_find(&s->man, path);
        struct stat st;
        if (e && e->kind == 'P' && e->size == size && strcmp(e->sha1, sha_hex) == 0 && stat(part, &st) == 0) {
            off = (uint64_t)st.st_size;
            off = off > size ? 0 : off & ~(uint64_t)(LINK_CHUNK - 1);
        }
        uint64_t need = size - off, have = free_bytes(root);
        if (have && need + need / 20 + (1u << 20) > have) {
            char m[160];
            snprintf(m, sizeof m, "code=space\nmsg=card full: %llu MB needed, %llu MB free\n",
                     (unsigned long long)(need >> 20), (unsigned long long)(have >> 20));
            return send_msg(x, LINK_ERROR, 0, m, (uint32_t)strlen(m));
        }
        if (mkdirs_for(full, strlen(root)) != 0)
            return send_kv(x, LINK_ERROR, "code=io\nmsg=cannot create the folder\n");
        if (off) {
            // Rebuild the SHA-1 state from what is on the card (before opening it for writing: FSA may refuse a
            // second handle on a file open for writing).
            uint64_t t0 = lp_now_us();
            int rfd = open(part, O_RDONLY | O_BINARY);
            uint64_t left = off;
            while (rfd >= 0 && left) {
                size_t n = left > LINK_CHUNK ? LINK_CHUNK : (size_t)left;
                ssize_t r = read(rfd, x->scratch, n);
                if (r <= 0)
                    break;
                if (!nohash)
                    link_sha1_update(&sha, x->scratch, (size_t)r);
                left -= (uint64_t)r;
            }
            if (rfd >= 0)
                close(rfd);
            if (left) {
                off = 0;
                link_sha1_init(&sha);
            }
            rehash_us = lp_now_us() - t0;
        }
        fd = open(part, O_WRONLY | O_CREAT | O_BINARY | (off ? 0 : O_TRUNC), 0666);
        if (fd < 0)
            return send_kv(x, LINK_ERROR, "code=io\nmsg=cannot open the file (card write-protected?)\n");
        if (off) {
            if (ftruncate(fd, (off_t)off) != 0 || lseek(fd, (off_t)off, SEEK_SET) != (off_t)off) {
                close(fd);
                return send_kv(x, LINK_ERROR, "code=io\nmsg=cannot resume the .part\n");
            }
            lp_log("resume %s at %llu MiB (re-hash %llu ms)", path, (unsigned long long)(off >> 20),
                   (unsigned long long)(rehash_us / 1000));
        }
        if (!bench && size >= SMALL_FILE) {
            man_set(&s->man, 'P', path, size, sha_hex);
            man_save(&s->man, root);
        }
    }

    status_set(s, LINK_ST_FILE, path, off, size);
    if (send_kv(x, LINK_READY, "offset=%llu\n", (unsigned long long)off) != 0)
        goto drop;

    {
        uint64_t expected = off, acked = off, next_flush = off + LINK_FLUSH_EVERY;
        uint64_t t_start = lp_now_us(), net_us = 0, hash_us = 0, wait_us = 0, last_status = 0;
        int crc_fail = 0, failed = 0, resend_pending = 0;
        char why[96] = "";
        pthread_mutex_lock(&x->m);
        x->flushed = off;
        x->sd_us = 0;
        x->writer_err = 0;
        pthread_mutex_unlock(&x->m);

        for (;;) {
            hdr h;
            uint64_t t_hdr = lp_now_us();  // "net" = waiting for and receiving DATA frames, minus waits for the card
            if (recv_hdr(x, &h) != 0)
                goto drop;
            if (h.type == LINK_DATA) {
                uint8_t dh[LINK_DATA_HDR];
                if (h.len < LINK_DATA_HDR || h.len - LINK_DATA_HDR > LINK_CHUNK || recv_all(x, dh, sizeof dh) != 0)
                    goto drop;
                uint32_t n = h.len - LINK_DATA_HDR;
                uint64_t doff = ((uint64_t)get_be32(dh) << 32) | get_be32(dh + 4);
                uint32_t crc = get_be32(dh + 8);
                if (failed || doff != expected || expected + n > size) {
                    if (skip_bytes(x, n) != 0)
                        goto drop;
                    if (!failed && !resend_pending && doff != expected)
                        goto drop;  // out of order without a RESEND: a broken sender
                    continue;
                }
                resend_pending = 0;
                slot *sl = NULL;
                uint8_t *dst = x->scratch;
                uint64_t waited = 0;
                if (!discard) {
                    uint64_t t0 = lp_now_us();
                    sl = ring_acquire(x);
                    waited = lp_now_us() - t0;
                    wait_us += waited;
                    dst = sl->buf;
                }
                if (recv_all(x, dst, n) != 0)
                    goto drop;
                uint64_t t1 = lp_now_us();
                net_us += t1 - t_hdr - waited;
                if (!nohash) {
                    if (link_crc32(0, dst, n) != crc) {
                        lp_log("CRC mismatch at %llu (%s)", (unsigned long long)doff, path);
                        if (++crc_fail >= 3) {
                            failed = 1;
                            snprintf(why, sizeof why, "crc (3 bad chunks)");
                        } else {
                            resend_pending = 1;
                            if (send_kv(x, LINK_RESEND, "offset=%llu\n", (unsigned long long)expected) != 0)
                                goto drop;
                        }
                        continue;
                    }
                    link_sha1_update(&sha, dst, n);
                    hash_us += lp_now_us() - t1;
                }
                expected += n;
                if (!discard) {
                    sl->op = 0;
                    sl->fd = fd;
                    sl->len = n;
                    sl->end_off = expected;
                    ring_push(x);
                    if (expected >= next_flush || expected == size) {
                        ring_op(x, 1, fd, expected);
                        next_flush = expected + LINK_FLUSH_EVERY;
                    }
                    pthread_mutex_lock(&x->m);
                    uint64_t fl = x->flushed;
                    int werr = x->writer_err;
                    pthread_mutex_unlock(&x->m);
                    if (werr && !failed) {
                        failed = 1;
                        snprintf(why, sizeof why, "write error %d (card full or failing?)", werr);
                    }
                    if (fl > acked) {
                        acked = fl;
                        if (send_kv(x, LINK_ACK, "offset=%llu\n", (unsigned long long)acked) != 0)
                            goto drop;
                    }
                } else if (expected - acked >= LINK_FLUSH_EVERY || expected == size) {
                    acked = expected;
                    if (send_kv(x, LINK_ACK, "offset=%llu\n", (unsigned long long)acked) != 0)
                        goto drop;
                }
                uint64_t now = lp_now_us();
                if (now - last_status > 200000) {
                    status_set(s, LINK_ST_FILE, NULL, expected, size);
                    last_status = now;
                }
                if (s->paused) {
                    lp_log("paused (HOME): keeping %s.part at %llu MB", path, (unsigned long long)(expected >> 20));
                    goto drop;
                }
            } else if (h.type == LINK_END_FILE || h.type == LINK_CANCEL) {
                if (skip_bytes(x, h.len) != 0)
                    goto drop;
                if (h.type == LINK_END_FILE && resend_pending && !failed)
                    continue;  // sent before our RESEND reached the PC: the resent data and a new END_FILE follow
                if (!discard) {
                    ring_op(x, 1, fd, expected);
                    ring_drain(x);
                    if (x->writer_err && !failed) {
                        failed = 1;
                        snprintf(why, sizeof why, "write error %d", x->writer_err);
                    }
                    close(fd);
                    fd = -1;
                }
                if (h.type == LINK_CANCEL) {
                    status_set(s, LINK_ST_SESSION, NULL, 0, 0);
                    return send_kv(x, LINK_ACK, "offset=%llu\n", (unsigned long long)expected);
                }
                uint64_t wall = lp_now_us() - t_start;
                uint8_t dig[20];
                char got[41] = "-";
                if (!nohash) {
                    link_sha1_final(&sha, dig);
                    link_hex(dig, 20, got);
                }
                if (!failed && expected != size)
                    failed = 1, snprintf(why, sizeof why, "short: %llu of %llu bytes", (unsigned long long)expected,
                                         (unsigned long long)size);
                if (!failed && !nohash && strcmp(got, sha_hex) != 0)
                    failed = 1, snprintf(why, sizeof why, "sha1 mismatch");
                status_set(s, LINK_ST_SESSION, NULL, 0, 0);
                if (failed) {
                    lp_log("BAD %s: %s", path, why);
                    if (!discard) {
                        unlink(part);
                        man_remove(&s->man, path);
                        man_save(&s->man, root);
                    }
                    return file_bad(x, why) == 0 ? 0 : -1;
                }
                if (bench) {
                    unlink(part);
                } else if (!discard) {
                    if (replace_file(part, full) != 0) {
                        lp_log("BAD %s: rename failed", path);
                        return file_bad(x, "rename failed") == 0 ? 0 : -1;
                    }
                    man_set(&s->man, 'F', path, size, sha_hex);
                    if (size >= SMALL_FILE || lp_now_us() - s->man.saved_us > 5000000u)
                        man_save(&s->man, root);
                }
                s->st.files_ok++;
                uint64_t got_b = expected - off;
                double mb = got_b / 1048576.0;
                if (size >= (1u << 20) || discard || bench)
                    lp_log("OK %s %.1f MiB %.2f MiB/s (net %.2f, sd %.2f, hash %.1f)", path, mb,
                           wall ? mb / (wall / 1e6) : 0, net_us ? mb / (net_us / 1e6) : 0,
                           x->sd_us ? mb / (x->sd_us / 1e6) : 0, hash_us ? mb / (hash_us / 1e6) : 0);
                return send_kv(x, LINK_FILE_OK,
                               "sha1=%s\nbytes=%llu\nwall_ms=%llu\nnet_ms=%llu\nhash_ms=%llu\nsd_ms=%llu\nwait_ms=%llu\n"
                               "rehash_ms=%llu\n",
                               got, (unsigned long long)got_b, (unsigned long long)(wall / 1000),
                               (unsigned long long)(net_us / 1000), (unsigned long long)(hash_us / 1000),
                               (unsigned long long)(x->sd_us / 1000), (unsigned long long)(wait_us / 1000),
                               (unsigned long long)(rehash_us / 1000)) == 0
                           ? 0
                           : -1;
            } else if (h.type == LINK_PING) {
                if (skip_bytes(x, h.len) != 0 || send_msg(x, LINK_PONG, 0, NULL, 0) != 0)
                    goto drop;
            } else {
                goto drop;
            }
        }
    }

drop:
    // The connection broke (or HOME): flush what was received, keep the .part for resume.
    if (fd >= 0) {
        ring_op(x, 1, fd, 0);
        ring_drain(x);
        close(fd);
    }
    status_set(s, LINK_ST_SESSION, NULL, 0, 0);
    return -1;
}

// Write `mb` MB of generated bytes to the card: the SD card alone.
static int do_bench_sd(sess *x, const char *body)
{
    link_server *s = x->srv;
    long long mb = kv_int(body, "mb", 256), chunk_kib = kv_int(body, "chunk_kib", 1024), files = kv_int(body, "files", 1);
    int use_fwrite = (int)kv_int(body, "fwrite", 0);
    if (mb < 1 || mb > 4000 || chunk_kib < 1 || chunk_kib > 1024 || files < 1 || files > 10000)
        return send_kv(x, LINK_ERROR, "code=proto\nmsg=bad BENCH_SD\n");
    uint64_t total = (uint64_t)mb << 20, per = (total / (uint64_t)files) & ~(uint64_t)(ALIGN - 1);
    if (free_bytes(s->o.sd_root) && total + (64u << 20) > free_bytes(s->o.sd_root))
        return send_kv(x, LINK_ERROR, "code=space\nmsg=card full for the bench\n");
    uint32_t chunk = (uint32_t)chunk_kib << 10;
    uint32_t r = 0x12345678;
    for (uint32_t i = 0; i < LINK_CHUNK; i += 4) {
        r ^= r << 13, r ^= r >> 17, r ^= r << 5;
        memcpy(x->scratch + i, &r, 4);
    }
    char dir[300], fn[340], fn2[350];
    snprintf(dir, sizeof dir, "%s/rac1-native/.link/bench", s->o.sd_root);
    mkdir(dir, 0777);
    lp_log("bench SD: %lld MiB in %lld file(s), %lld KiB %s writes", mb, files, chunk_kib,
           use_fwrite ? "unaligned fwrite" : "aligned write()");
    status_set(s, LINK_ST_BENCH, "SD bench", 0, total);
    int err = 0;
    uint64_t t0 = lp_now_us(), written = 0;
    for (long long i = 0; i < files && !err && !s->quit; i++) {
        snprintf(fn, sizeof fn, "%s/b%05lld.bin", dir, i);
        uint64_t left = per;
        if (use_fwrite) {
            FILE *f = fopen(fn, "wb");
            if (!f) {
                err = errno ? errno : EIO;
                break;
            }
            while (left && !err) {
                size_t n = left > chunk ? chunk : (size_t)left;
                if (n > LINK_CHUNK - 1)
                    n = LINK_CHUNK - 1;
                if (fwrite(x->scratch + 1, 1, n, f) != n)  // +1: deliberately unaligned
                    err = errno ? errno : EIO;
                left -= n;
                written += n;
            }
            fflush(f);
            fsync(fileno(f));
            fclose(f);
        } else {
            int fd = open(fn, O_WRONLY | O_CREAT | O_TRUNC | O_BINARY, 0666);
            if (fd < 0) {
                err = errno ? errno : EIO;
                break;
            }
            while (left && !err) {
                size_t n = left > chunk ? chunk : (size_t)left;
                ssize_t w = write(fd, x->scratch, n);
                if (w != (ssize_t)n)
                    err = w < 0 ? errno : ENOSPC;
                left -= n;
                written += n;
            }
            fsync(fd);
            close(fd);
        }
        status_set(s, LINK_ST_BENCH, NULL, written, total);
    }
    uint64_t t1 = lp_now_us();
    for (long long i = 0; i < files && !err; i++) {
        snprintf(fn, sizeof fn, "%s/b%05lld.bin", dir, i);
        snprintf(fn2, sizeof fn2, "%s/r%05lld.bin", dir, i);
        rename(fn, fn2);
    }
    uint64_t t2 = lp_now_us(), readb = 0;
    for (long long i = 0; i < files && !err; i++) {
        snprintf(fn2, sizeof fn2, "%s/r%05lld.bin", dir, i);
        int fd = open(fn2, O_RDONLY | O_BINARY);
        if (fd < 0)
            continue;
        ssize_t n;
        while ((n = read(fd, x->scratch, LINK_CHUNK)) > 0)
            readb += (uint64_t)n;
        close(fd);
    }
    uint64_t t3 = lp_now_us();
    for (long long i = 0; i < files; i++) {
        snprintf(fn, sizeof fn, "%s/b%05lld.bin", dir, i);
        snprintf(fn2, sizeof fn2, "%s/r%05lld.bin", dir, i);
        unlink(fn);
        unlink(fn2);
    }
    uint64_t t4 = lp_now_us();
    rmdir(dir);
    status_set(s, LINK_ST_SESSION, NULL, 0, 0);
    if (err)
        return send_kv(x, LINK_ERROR, "code=io\nmsg=SD bench write error %d\n", err);
    lp_log("bench SD: write %.2f MiB/s, read %.2f MiB/s, %lld renames %llu ms, delete %llu ms",
           written / 1048576.0 / ((t1 - t0) / 1e6 + 1e-9), readb / 1048576.0 / ((t3 - t2) / 1e6 + 1e-9), files,
           (unsigned long long)((t2 - t1) / 1000), (unsigned long long)((t4 - t3) / 1000));
    return send_kv(x, LINK_BENCH_RESULT, "bytes=%llu\nwrite_ms=%llu\nrename_ms=%llu\nread_bytes=%llu\nread_ms=%llu\n"
                                         "delete_ms=%llu\nfiles=%lld\n",
                   (unsigned long long)written, (unsigned long long)((t1 - t0) / 1000),
                   (unsigned long long)((t2 - t1) / 1000), (unsigned long long)readb,
                   (unsigned long long)((t3 - t2) / 1000), (unsigned long long)((t4 - t3) / 1000), files);
}

static int do_config(sess *x, const char *body)
{
    link_server *s = x->srv;
    pthread_mutex_lock(&s->lock);
    s->rcvbuf = (int)kv_int(body, "rcvbuf", s->rcvbuf);
    s->winscale = (int)kv_int(body, "winscale", s->winscale);
    s->sack = (int)kv_int(body, "sack", s->sack);
    s->noslowstart = (int)kv_int(body, "noslowstart", s->noslowstart);
    s->rusrbuf = (int)kv_int(body, "rusrbuf", s->rusrbuf);
    s->cfg_gen++;
    pthread_mutex_unlock(&s->lock);
    int somemopt = (int)kv_int(body, "somemopt", link_cfg_int(s->o.sd_root, "somemopt", 0));
    int big = (int)kv_int(body, "somemopt_big", link_cfg_int(s->o.sd_root, "somemopt_big", 1));
    cfg_write(s, somemopt, big);
    lp_log("config: rcvbuf=%d winscale=%d sack=%d noslowstart=%d rusrbuf=%d (next connection); somemopt=%d big=%d "
           "(next start)",
           s->rcvbuf, s->winscale, s->sack, s->noslowstart, s->rusrbuf, somemopt, big);
    return send_kv(x, LINK_CONFIG_OK, "rcvbuf=%d\nwinscale=%d\nsack=%d\nnoslowstart=%d\nrusrbuf=%d\nsomemopt=%d\n"
                                      "somemopt_big=%d\n",
                   s->rcvbuf, s->winscale, s->sack, s->noslowstart, s->rusrbuf, somemopt, big);
}

static void session(link_server *s, int fd, const char *peer)
{
    sess *x = (sess *)calloc(1, sizeof *x);
    x->srv = s;
    x->fd = fd;
    snprintf(x->peer, sizeof x->peer, "%s", peer);
    x->scratch = (uint8_t *)memalign(ALIGN, LINK_CHUNK);
    for (int i = 0; i < NSLOTS; i++)
        x->ring[i].buf = (uint8_t *)memalign(ALIGN, LINK_CHUNK);
    pthread_mutex_init(&x->m, NULL);
    pthread_cond_init(&x->cv_free, NULL);
    pthread_cond_init(&x->cv_work, NULL);
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 64 * 1024);
    int writer_ok = pthread_create(&x->writer, &at, writer_main, x) == 0;
    pthread_attr_destroy(&at);
    int ok = x->scratch && writer_ok;
    for (int i = 0; i < NSLOTS; i++)
        ok = ok && x->ring[i].buf;

    uint8_t nonce[16];
    lp_random(nonce, sizeof nonce);
    link_hex(nonce, 16, x->nonce_c);
    pthread_mutex_lock(&s->lock);
    s->st.sessions++;
    snprintf(s->st.peer, sizeof s->st.peer, "%s", peer);
    pthread_mutex_unlock(&s->lock);
    status_set(s, LINK_ST_SESSION, "", 0, 0);
    lp_log("PC connected: %s", peer);

    if (ok)
        ok = send_kv(x, LINK_HELLO,
                     "proto=%d\nname=%s\nversion=%s\nserial=%s\nnonce=%s\nfree=%llu\nchunk=%u\n", LINK_PROTO_VERSION,
                     s->o.name, s->o.version, s->serial, x->nonce_c, (unsigned long long)free_bytes(s->o.sd_root),
                     LINK_CHUNK) == 0;
    while (ok && !s->quit && !s->paused) {
        hdr h;
        if (recv_hdr(x, &h) != 0)
            break;
        if (h.type == LINK_DATA) {  // stray data (e.g. after a refused BEGIN_FILE): drop it
            if (h.len > LINK_DATA_HDR + LINK_CHUNK || skip_bytes(x, h.len) != 0)
                break;
            continue;
        }
        char *body = recv_ctrl(x, h.len);
        if (!body)
            break;
        int r = 0;
        if (h.type == LINK_PING)
            r = send_msg(x, LINK_PONG, 0, NULL, 0);
        else if (h.type == LINK_BYE)
            r = -1;
        else if (h.type == LINK_AUTH)
            r = do_auth(x, body);
        else if (!x->authed)
            r = send_kv(x, LINK_ERROR, "code=auth\nmsg=not paired\n"), r = -1;
        else if (h.type == LINK_GET_MANIFEST)
            r = do_manifest(x);
        else if (h.type == LINK_BEGIN_FILE)
            r = do_file(x, body, h.flags);
        else if (h.type == LINK_BENCH_SD)
            r = do_bench_sd(x, body);
        else if (h.type == LINK_CONFIG)
            r = do_config(x, body);
        else if (h.type == LINK_QUIT) {
            lp_log("the PC asked rac1-link to close");
            pthread_mutex_lock(&s->lock);
            s->st.quit_requested = 1;
            pthread_mutex_unlock(&s->lock);
            r = -1;
        }
        else if (h.type == LINK_LAUNCH) {
            lp_log("the PC asked to start the game");
            pthread_mutex_lock(&s->lock);
            s->st.launch_requested = 1;
            pthread_mutex_unlock(&s->lock);
            r = -1;
        }
        else if (h.type == LINK_PROGRESS) {
            char t[120];
            if (kv_get(body, "text", t, sizeof t) == 0)
                lp_log("PC: %s", t);
        } else
            r = send_kv(x, LINK_ERROR, "code=proto\nmsg=unknown message %d\n", h.type);
        free(body);
        if (r != 0)
            break;
    }
    if (s->man.dirty)
        man_save(&s->man, s->o.sd_root);
    // stop the writer
    if (writer_ok) {
        ring_op(x, 2, -1, 0);
        pthread_join(x->writer, NULL);
    }
    for (int i = 0; i < NSLOTS; i++)
        free(x->ring[i].buf);
    free(x->scratch);
    pthread_mutex_destroy(&x->m);
    pthread_cond_destroy(&x->cv_free);
    pthread_cond_destroy(&x->cv_work);
    free(x);
    lp_log("PC disconnected: %s", peer);
    status_set(s, s->paused ? LINK_ST_PAUSED : LINK_ST_IDLE, "", 0, 0);
}

static int make_listener(link_server *s)
{
    int fd = socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0)
        return -1;
    int one = 1;
    setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
    apply_sockopts(s, fd);  // inherited by accepted sockets (window scale is negotiated at SYN)
    struct sockaddr_in a;
    memset(&a, 0, sizeof a);
    a.sin_family = AF_INET;
    a.sin_port = htons((uint16_t)s->o.tcp_port);
    a.sin_addr.s_addr = htonl(INADDR_ANY);
    if (bind(fd, (struct sockaddr *)&a, sizeof a) != 0 || listen(fd, 2) != 0) {
        close(fd);
        return -1;
    }
    return fd;
}

static void *net_main(void *arg)
{
    link_server *s = (link_server *)arg;
    lp_thread_role(LP_ROLE_NET);
    int lfd = -1, gen = -1;
    while (!s->quit) {
        if (s->paused) {
            if (lfd >= 0) {
                close(lfd);
                lfd = -1;
                status_set(s, LINK_ST_PAUSED, "", 0, 0);
            }
            lp_sleep_ms(100);
            continue;
        }
        if (lfd < 0 || gen != s->cfg_gen) {
            if (lfd >= 0)
                close(lfd);
            gen = s->cfg_gen;
            lfd = make_listener(s);
            if (lfd < 0) {
                lp_log("cannot listen on TCP %d (errno %d), retrying", s->o.tcp_port, errno);
                lp_sleep_ms(1000);
                continue;
            }
            status_set(s, LINK_ST_IDLE, "", 0, 0);
        }
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(lfd, &rs);
        struct timeval tv = {0, 250000};
        if (select(lfd + 1, &rs, NULL, NULL, &tv) <= 0)
            continue;
        struct sockaddr_in pa;
        socklen_t pl = sizeof pa;
        int fd = accept(lfd, (struct sockaddr *)&pa, &pl);
        if (fd < 0)
            continue;
        char peer[48];
        inet_ntop(AF_INET, &pa.sin_addr, peer, sizeof peer);
        if (!link_peer_ok(ntohl(pa.sin_addr.s_addr))) {
            lp_log("refused a connection from %s (not a private address)", peer);
            close(fd);
            continue;
        }
        apply_sockopts(s, fd);
        session(s, fd, peer);
        close(fd);
    }
    if (lfd >= 0)
        close(lfd);
    return NULL;
}

static void *disc_main(void *arg)
{
    link_server *s = (link_server *)arg;
    lp_thread_role(LP_ROLE_DISCOVERY);
    int fd = -1;
    while (!s->quit) {
        if (fd < 0) {
            fd = socket(AF_INET, SOCK_DGRAM, 0);
            int one = 1;
            setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &one, sizeof one);
            setsockopt(fd, SOL_SOCKET, SO_BROADCAST, &one, sizeof one);
            struct sockaddr_in a;
            memset(&a, 0, sizeof a);
            a.sin_family = AF_INET;
            a.sin_port = htons((uint16_t)s->o.udp_port);
            a.sin_addr.s_addr = htonl(INADDR_ANY);
            if (fd < 0 || bind(fd, (struct sockaddr *)&a, sizeof a) != 0) {
                lp_log("cannot bind UDP %d (errno %d), retrying", s->o.udp_port, errno);
                if (fd >= 0)
                    close(fd);
                fd = -1;
                lp_sleep_ms(2000);
                continue;
            }
        }
        fd_set rs;
        FD_ZERO(&rs);
        FD_SET(fd, &rs);
        struct timeval tv = {0, 250000};
        if (select(fd + 1, &rs, NULL, NULL, &tv) <= 0)
            continue;
        char buf[256];
        struct sockaddr_in pa;
        socklen_t pl = sizeof pa;
        ssize_t n = recvfrom(fd, buf, sizeof buf - 1, 0, (struct sockaddr *)&pa, &pl);
        if (n <= 0)
            continue;
        buf[n] = 0;
        if (strncmp(buf, LINK_DISCOVER_QUERY, strlen(LINK_DISCOVER_QUERY)) != 0 || s->paused ||
            !link_peer_ok(ntohl(pa.sin_addr.s_addr)))
            continue;
        char reply[256];
        int len = snprintf(reply, sizeof reply, "%s proto=%d name=%s version=%s port=%d serial=%s busy=%d",
                           LINK_DISCOVER_ANSWER, LINK_PROTO_VERSION, s->o.name, s->o.version, s->o.tcp_port,
                           s->serial, s->st.state >= LINK_ST_SESSION);
        sendto(fd, reply, (size_t)len, 0, (struct sockaddr *)&pa, pl);
    }
    if (fd >= 0)
        close(fd);
    return NULL;
}

link_server *link_server_new(const link_options *o)
{
    link_server *s = (link_server *)calloc(1, sizeof *s);
    s->o = *o;
    pthread_mutex_init(&s->lock, NULL);
    s->rcvbuf = link_cfg_int(o->sd_root, "rcvbuf", 0);
    s->winscale = link_cfg_int(o->sd_root, "winscale", 1);
    s->sack = link_cfg_int(o->sd_root, "sack", 0);
    s->noslowstart = link_cfg_int(o->sd_root, "noslowstart", 0);
    s->rusrbuf = link_cfg_int(o->sd_root, "somemopt", 0);

    char dir[300], path[320];
    snprintf(dir, sizeof dir, "%s/rac1-native", o->sd_root);
    mkdir(dir, 0777);
    snprintf(dir, sizeof dir, "%s/rac1-native/.link", o->sd_root);
    mkdir(dir, 0777);

    // The pairing code: fixed (tests), or kept on the card so that pairing is once.
    snprintf(path, sizeof path, "%s/pair.txt", dir);
    if (o->fixed_code[0]) {
        snprintf(s->code, sizeof s->code, "%.6s", o->fixed_code);
    } else {
        FILE *f = fopen(path, "r");
        if (!f || fscanf(f, "%6s", s->code) != 1 || strlen(s->code) != 6) {
            uint8_t r[4];
            lp_random(r, sizeof r);
            uint32_t v = ((uint32_t)r[0] << 24 | (uint32_t)r[1] << 16 | (uint32_t)r[2] << 8 | r[3]) % 1000000u;
            snprintf(s->code, sizeof s->code, "%06u", (unsigned)v);
            if (f)
                fclose(f);
            f = fopen(path, "w");
            if (f)
                fprintf(f, "%s\n", s->code);
        }
        if (f)
            fclose(f);
    }
    // The serial sent is a hash of the device id, never the id.
    char id[64];
    lp_device_id(id, sizeof id);
    uint8_t h[32];
    link_hmac_sha256("rac1-link serial", 16, id, strlen(id), h);
    link_hex(h, 8, s->serial);
    man_load(&s->man, o->sd_root);
    return s;
}

int link_server_start(link_server *s)
{
    pthread_attr_t at;
    pthread_attr_init(&at);
    pthread_attr_setstacksize(&at, 128 * 1024);
    if (pthread_create(&s->net_thread, &at, net_main, s) != 0)
        return -1;
    if (pthread_create(&s->disc_thread, &at, disc_main, s) != 0) {
        s->quit = 1;
        pthread_join(s->net_thread, NULL);
        return -1;
    }
    pthread_attr_destroy(&at);
    s->started = 1;
    return 0;
}

void link_server_stop(link_server *s)
{
    s->quit = 1;
    if (s->started) {
        pthread_join(s->net_thread, NULL);
        pthread_join(s->disc_thread, NULL);
    }
    if (s->man.dirty)
        man_save(&s->man, s->o.sd_root);
    man_free(&s->man);
    pthread_mutex_destroy(&s->lock);
    free(s);
}
