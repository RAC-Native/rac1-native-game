// rac1-link: the receiver (the console's side of the protocol), portable C11 + POSIX sockets/files + pthreads.
// The same code runs on the Wii U (wut's newlib devoptab and nsysnet sockets) and on a PC (host/, for tests).
#pragma once
#include <stddef.h>
#include <stdint.h>

#ifdef __cplusplus
extern "C" {
#endif

// What each platform provides (console/main.c, host/main.c).
enum { LP_ROLE_NET, LP_ROLE_WRITER, LP_ROLE_DISCOVERY };
uint64_t lp_now_us(void);
void lp_sleep_ms(unsigned ms);
void lp_random(void *buf, size_t n);
void lp_log(const char *fmt, ...) __attribute__((format(printf, 1, 2)));
void lp_thread_role(int role);              // e.g. the core a thread runs on
void lp_device_id(char *out, size_t n);     // a stable id of the device; only its hash is ever sent

typedef struct link_options {
    char sd_root[256];  // the SD card's root, no trailing slash ("fs:/vol/external01" on the console)
    int tcp_port;
    int udp_port;
    char fixed_code[8]; // tests: a fixed pairing code; empty = generated once, kept in rac1-native/.link/pair.txt
    const char *name;
    const char *version;
} link_options;

enum { LINK_ST_IDLE, LINK_ST_PAUSED, LINK_ST_SESSION, LINK_ST_FILE, LINK_ST_BENCH };

typedef struct link_status {
    int state;
    char peer[48];
    char file[300];
    uint64_t done, total;
    uint64_t file_start_us;
    unsigned files_ok, files_bad, sessions;
    int quit_requested;  // the PC sent QUIT
} link_status;

typedef struct link_server link_server;

link_server *link_server_new(const link_options *o);
int link_server_start(link_server *s);           // starts the TCP and discovery threads; 0 on success
void link_server_pause(link_server *s, int on);  // ProcUI release/acquire: close the session cleanly, stop listening
void link_server_stop(link_server *s);           // quit, join, free
void link_server_status(link_server *s, link_status *out);
const char *link_server_code(link_server *s);

// link.cfg (rac1-native/.link/link.cfg, "key=value" lines): read one integer, default if missing.
int link_cfg_int(const char *sd_root, const char *key, int def);

// Exposed for the self-test.
int link_path_ok(const char *rel);   // 1 if `rel` may be written (see link_server.c)
int link_peer_ok(uint32_t ipv4);     // 1 for loopback / RFC 1918 / link-local (host byte order)

#ifdef __cplusplus
}
#endif
