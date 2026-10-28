// rac1-link-host: the console's receiver built for a PC, for loopback tests of the protocol and of pc/rac1_link.py.
// The same core (../core) as the Wii U build; a directory stands in for the SD card.
//
//   rac1-link-host --root DIR [--port 47199] [--udp-port 47198] [--code 123456]
//   rac1-link-host --selftest
// SIGUSR1 toggles the pause that HOME causes on the console.
#define _GNU_SOURCE
#include "../core/link_hash.h"
#include "../core/link_proto.h"
#include "../core/link_server.h"

#include <pthread.h>
#include <signal.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>
#include <unistd.h>

uint64_t lp_now_us(void)
{
    struct timespec t;
    clock_gettime(CLOCK_MONOTONIC, &t);
    return (uint64_t)t.tv_sec * 1000000u + (uint64_t)t.tv_nsec / 1000u;
}

void lp_sleep_ms(unsigned ms) { usleep(ms * 1000u); }

void lp_random(void *buf, size_t n)
{
    FILE *f = fopen("/dev/urandom", "rb");
    if (!f || fread(buf, 1, n, f) != n)
        abort();
    fclose(f);
}

static pthread_mutex_t log_lock = PTHREAD_MUTEX_INITIALIZER;
void lp_log(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    pthread_mutex_lock(&log_lock);
    fprintf(stderr, "[link] ");
    vfprintf(stderr, fmt, ap);
    fputc('\n', stderr);
    pthread_mutex_unlock(&log_lock);
    va_end(ap);
}

void lp_thread_role(int role) { (void)role; }

void lp_device_id(char *out, size_t n) { snprintf(out, n, "host-stand-in"); }

static int check(int cond, const char *what)
{
    if (!cond)
        fprintf(stderr, "selftest FAILED: %s\n", what);
    return cond ? 0 : 1;
}

static int selftest(void)
{
    int bad = 0;
    char hex[65];
    uint8_t d[32];
    // Known answers (FIPS / zlib)
    bad += check(link_crc32(0, "123456789", 9) == 0xCBF43926u, "crc32");
    link_sha1 s1;
    link_sha1_init(&s1);
    link_sha1_update(&s1, "abc", 3);
    link_sha1_final(&s1, d);
    link_hex(d, 20, hex);
    bad += check(strcmp(hex, "a9993e364706816aba3e25717850c26c9cd0d89d") == 0, "sha1 abc");
    link_sha256 s2;
    link_sha256_init(&s2);
    link_sha256_update(&s2, "abc", 3);
    link_sha256_final(&s2, d);
    link_hex(d, 32, hex);
    bad += check(strcmp(hex, "ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad") == 0, "sha256 abc");
    link_hmac_sha256("key", 3, "The quick brown fox jumps over the lazy dog", 43, d);
    link_hex(d, 32, hex);
    bad += check(strcmp(hex, "f7bc83f430538424b13298e6aa6fb143ef4d59a14946175997479dbc2d1a3cd8") == 0, "hmac");

    static const char *ok[] = {
        "rac1-native/run.cfg",
        "rac1-native/assets/disc/toc/a.vag",
        "rac1-native/shaders/x.gsh",
        "wiiu/apps/rac1-native/rac1-native.wuhb",
        "wiiu/apps/rac1-native/rac1-link.wuhb",
        "rac1-native/.links/x",
    };
    static const char *no[] = {
        "",
        "/rac1-native/x",
        "rac1-native",
        "rac1-native/",
        "rac1-native/../x",
        "rac1-native/a/../../x",
        "rac1-native/./x",
        "rac1-native//x",
        "rac1-native/x/",
        "rac1-native/.link/manifest.txt",
        "rac1-native/.LINK/pair.txt",
        "rac1-native/.link",
        "rac1-native/a\\b",
        "rac1-native/c:x",
        "rac1-native/x.part",
        "rac1-native/x.PART",
        "rac1-native/x.old",
        "rac1-native/x.",
        "rac1-native/ x",
        "rac1-native/x\ty",
        "RAC1-NATIVE/x",
        "wiiu/apps/other/x.wuhb",
        "wiiu/apps/rac1-native",
        "wiiu/apps/rac1-native/",
        "wiiu/apps/x",
        "x",
        "../rac1-native/x",
        "fs:/vol/external01/rac1-native/x",
    };
    for (size_t i = 0; i < sizeof ok / sizeof *ok; i++)
        bad += check(link_path_ok(ok[i]) == 1, ok[i]);
    for (size_t i = 0; i < sizeof no / sizeof *no; i++)
        bad += check(link_path_ok(no[i]) == 0, no[i]);
    char longp[300] = "rac1-native/";
    memset(longp + 12, 'a', 250);
    longp[262] = 0;
    bad += check(link_path_ok(longp) == 0, "long path");

    bad += check(link_peer_ok(0x7F000001u), "127.0.0.1");
    bad += check(link_peer_ok(0xC0A80010u), "192.168.0.16");
    bad += check(link_peer_ok(0x0A000001u), "10.0.0.1");
    bad += check(link_peer_ok(0xAC100001u), "172.16.0.1");
    bad += check(link_peer_ok(0xAC1F0001u), "172.31.0.1");
    bad += check(link_peer_ok(0xA9FE0101u), "169.254.1.1");
    bad += check(!link_peer_ok(0xAC200001u), "172.32.0.1 refused");
    bad += check(!link_peer_ok(0x08080808u), "8.8.8.8 refused");
    bad += check(!link_peer_ok(0xC0A90001u), "192.169.0.1 refused");
    printf("selftest: %s (%d failure(s))\n", bad ? "FAILED" : "ok", bad);
    return bad ? 1 : 0;
}

static volatile sig_atomic_t stop, pause_toggles;
static void on_signal(int sig) { (void)sig, stop = 1; }
static void on_usr1(int sig) { (void)sig, pause_toggles++; }  // stands in for HOME (ProcUI release / acquire)

int main(int argc, char **argv)
{
    link_options o;
    memset(&o, 0, sizeof o);
    o.tcp_port = LINK_TCP_PORT;
    o.udp_port = LINK_UDP_PORT;
    o.name = "rac1-link-host";
    o.version = "0.1";
    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "--selftest"))
            return selftest();
        else if (!strcmp(argv[i], "--root") && i + 1 < argc)
            snprintf(o.sd_root, sizeof o.sd_root, "%s", argv[++i]);
        else if (!strcmp(argv[i], "--port") && i + 1 < argc)
            o.tcp_port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--udp-port") && i + 1 < argc)
            o.udp_port = atoi(argv[++i]);
        else if (!strcmp(argv[i], "--code") && i + 1 < argc)
            snprintf(o.fixed_code, sizeof o.fixed_code, "%s", argv[++i]);
        else {
            fprintf(stderr, "usage: %s --root DIR [--port N] [--udp-port N] [--code NNNNNN] | --selftest\n", argv[0]);
            return 2;
        }
    }
    if (!o.sd_root[0]) {
        fprintf(stderr, "--root DIR is required (the folder that stands in for the SD card)\n");
        return 2;
    }
    size_t n = strlen(o.sd_root);
    while (n > 1 && o.sd_root[n - 1] == '/')
        o.sd_root[--n] = 0;
    signal(SIGINT, on_signal);
    signal(SIGTERM, on_signal);
    signal(SIGPIPE, SIG_IGN);
    signal(SIGUSR1, on_usr1);
    link_server *s = link_server_new(&o);
    if (link_server_start(s) != 0) {
        fprintf(stderr, "cannot start\n");
        return 1;
    }
    lp_log("rac1-link-host: root %s, TCP %d, UDP %d, pairing code %s", o.sd_root, o.tcp_port, o.udp_port,
           link_server_code(s));
    int paused = 0;
    while (!stop) {
        if ((pause_toggles & 1) != paused) {
            paused = pause_toggles & 1;
            lp_log("%s (SIGUSR1)", paused ? "paused, as on HOME" : "resumed");
            link_server_pause(s, paused);
        }
        link_status st;
        link_server_status(s, &st);
        if (st.quit_requested)
            break;
        usleep(100000);
    }
    link_server_stop(s);
    lp_log("rac1-link-host: exit");
    return 0;
}
