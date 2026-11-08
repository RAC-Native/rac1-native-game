/* The example level's plugin (docs/custom-levels.md, "Code"): our own code, nothing of the game's.
 *
 * - A moving platform: the level's 6th placed object (level.txt's 6th moby line, a crate) glides back and forth
 *   between two points; its own update (a crate's: breaking when hit) does not run, so it cannot be broken.
 * - A sound where it turns round (one of the base level's sounds).
 * - A HUD line: how many times the platform has turned and how many times you came to the level.
 * - A counter in the save (save<N>.pc): the visits, kept by saving, reset by a new game.
 */
#include <rac1_mod.h>

#include <math.h>
#include <stdio.h>

RAC1_MOD_DECLARE_ABI()

#define PLATFORM_UID 6         /* the 6th moby line (uids 1, 2, ... in the lines' order) */
#define SOUND_TURN 3           /* the base level's sound played where it turns round */
#define SPEED 3.0f             /* units a second */

static const rac1_api* api;

static struct {
    int mine;                  /* on our level */
    float a[3], b[3];          /* the platform's two ends */
    float t;                   /* 0 .. 1 along a -> b */
    int dir;                   /* +1 towards b, -1 towards a */
    int turns;                 /* this visit */
    uint32_t visits;           /* kept in the save */
    char line[64];             /* the HUD line, made in frame (hud may run twice a frame: it only draws) */
} s;

static void make_line(void) {
    snprintf(s.line, sizeof s.line, "Platform turns: %d   Visits: %u", s.turns, (unsigned)s.visits);
}

static void level_start(void* user, const rac1_level_info* level) {
    (void)user;
    s.mine = level->mine;
    if (!s.mine) return;
    uint32_t m = api->moby_by_uid(PLATFORM_UID);
    if (m == 0) {
        api->log("no platform (the 6th moby line) on the level");
        s.mine = 0;
        return;
    }
    /* its ends: where level.txt put it, and 10 units along x, 2 units up */
    for (int i = 0; i < 3; ++i) s.a[i] = rac1_lwf(api, m + 0x10 + 4 * (uint32_t)i) + (i == 2 ? 2.0f : 0.0f);
    api->swf(m + 0x2c, rac1_lwf(api, m + 0x2c) * 2.0f);  /* twice a crate's size */
    s.b[0] = s.a[0] + 10.0f;
    s.b[1] = s.a[1];
    s.b[2] = s.a[2];
    s.t = 0.0f;
    s.dir = 1;
    s.turns = 0;
    ++s.visits;
    make_line();
    char msg[96];
    snprintf(msg, sizeof msg, "the platform is moby %08x, visit %u", (unsigned)m, (unsigned)s.visits);
    api->log(msg);
}

static void level_end(void* user) {
    (void)user;
    s.mine = 0;
}

/* the platform's update, instead of its class's */
static int object_update(void* user, uint32_t m, int32_t uid, int32_t cls) {
    (void)user;
    (void)cls;
    if (uid != PLATFORM_UID) return 0;  /* the other objects: their own updates */
    const float len = 10.0f;
    s.t += s.dir * SPEED / 60.0f / len;
    if (s.t >= 1.0f || s.t <= 0.0f) {
        s.t = s.t >= 1.0f ? 1.0f : 0.0f;
        s.dir = -s.dir;
        ++s.turns;
        api->sound(SOUND_TURN, m);
        make_line();
    }
    /* eased: slow at the ends */
    const float e = 0.5f - 0.5f * cosf(s.t * 3.14159265f);
    for (int i = 0; i < 3; ++i) api->swf(m + 0x10 + 4 * (uint32_t)i, s.a[i] + (s.b[i] - s.a[i]) * e);
    api->moby_moved(m);
    return 1;
}

static void hud(void* user) {
    (void)user;
    if (!s.mine) return;
    api->hud_text(24, 40, 0x80f0f0f0u, s.line);
}

static void new_game(void* user) {
    (void)user;
    s.visits = 0;
}

static uint32_t save(void* user, uint8_t* buf, uint32_t cap) {
    (void)user;
    if (cap < 4) return 0;
    for (int i = 0; i < 4; ++i) buf[i] = (uint8_t)(s.visits >> (8 * i));
    return 4;
}

static void load(void* user, const uint8_t* data, uint32_t size) {
    (void)user;
    s.visits = 0;
    if (data != NULL && size >= 4) s.visits = (uint32_t)data[0] | (uint32_t)data[1] << 8 | (uint32_t)data[2] << 16 | (uint32_t)data[3] << 24;
}

/* testing: RAC1_MOD_TEST=log the platform's and the hero's places every 60 frames; RAC1_MOD_TEST=ride also Ratchet put on
   the platform at the 200th frame */
static void test(void* user, const char* text, uint64_t frame) {
    (void)user;
    if (!s.mine) return;
    uint32_t m = api->moby_by_uid(PLATFORM_UID);
    if (m == 0) return;
    if (text[0] == 'r' && frame == 200) {
        const float at[3] = {rac1_lwf(api, m + 0x10), rac1_lwf(api, m + 0x14), rac1_lwf(api, m + 0x18) + 1.5f};
        api->hero_place(at, 0.0f);
        api->log("Ratchet put on the platform");
    }
    if (frame % 60 != 0) return;
    char msg[128];
    uint32_t hero = api->hero();
    snprintf(msg, sizeof msg, "frame %llu: platform at %.2f %.2f %.2f, t %.2f, turns %d; hero at %.2f %.2f %.2f, bolts %d", (unsigned long long)frame,
             rac1_lwf(api, m + 0x10), rac1_lwf(api, m + 0x14), rac1_lwf(api, m + 0x18), s.t, s.turns, rac1_lwf(api, hero + 0x10),
             rac1_lwf(api, hero + 0x14), rac1_lwf(api, hero + 0x18), api->bolts());
    api->log(msg);
}

RAC1_MOD_EXPORT int rac1_mod_main(const rac1_api* game, rac1_mod* mod) {
    if (game->abi < 1) return 1;
    api = game;
    mod->size = sizeof(rac1_mod);
    mod->level_start = level_start;
    mod->level_end = level_end;
    mod->object_update = object_update;
    mod->hud = hud;
    mod->new_game = new_game;
    mod->save = save;
    mod->load = load;
    mod->test = test;
    api->log("the example plugin is on");
    return 0;
}
