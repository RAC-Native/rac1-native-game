/* The example level's plugin (docs/custom-levels.md, "Code"; the reference: docs/sdk.md): our own code, nothing of the
 * game's.
 *
 * ABI 1's services:
 * - A moving platform: the level's 6th placed moby (level.txt's 6th moby line, a crate) glides back and forth between
 *   two points; its own update (a crate's: breaking when hit) does not run, so it cannot be broken.
 * - A sound where it turns round (one of the base level's sounds).
 * - A HUD line: how many times the platform has turned, how many times you came to the level, the gems taken.
 * - A counter in the save (save<N>.pc): the visits, kept by saving, reset by a new game.
 *
 * ABI 2's (the level's own classes, objects, sounds and trigger: level.txt's class / object / sound / trigger lines):
 * - sentry: a ball that hovers and turns. When Ratchet is within 12 units and nothing stands between them (a ray), it
 *   beeps, turns to him and after a second zaps him (one health point). It hums while it is there (a looping sound).
 *   Hit by his attacks three times it breaks: smoke, a sound, 25 bolts, and a gem where it was.
 * - gem: spins and bobs; Ratchet takes it by touching it (a sound, the HUD's count).
 * - the trigger "ramp": a chime and a HUD line while Ratchet is on the ramp.
 */
#include <rac1_mod.h>

#include <math.h>
#include <stdio.h>
#include <string.h>

RAC1_MOD_DECLARE_ABI()

#define PLATFORM_UID 6         /* the 6th moby line (uids 1, 2, ... in the lines' order) */
#define SOUND_TURN 3           /* the base level's sound played where it turns round */
#define SPEED 3.0f             /* units a second */

#define SENTRY_SIGHT 12.0f     /* how far a sentry sees */
#define SENTRY_AIM 60          /* frames it watches Ratchet before it zaps */
#define SENTRY_REST 90         /* frames after a zap before it watches again */
#define SENTRY_HP 3            /* hits it takes */

static const rac1_api* api;

static struct {
    int mine;                  /* on our level */
    float a[3], b[3];          /* the platform's two ends */
    float t;                   /* 0 .. 1 along a -> b */
    int dir;                   /* +1 towards b, -1 towards a */
    int turns;                 /* this visit */
    uint32_t visits;           /* kept in the save */
    int gems;                  /* gems taken this visit */
    int on_ramp;               /* Ratchet in the ramp's trigger */
    int32_t gem_class;         /* the level's class "gem" */
    int32_t snd_beep, snd_zap, snd_hit, snd_break, snd_pickup, snd_chime, snd_hum;
    char line[64], line2[64];  /* the HUD lines, made in frame (hud may run twice a frame: it only draws) */
} s;

/* a sentry's state (its class's state_size: the game keeps one per object, zeroed at its spawn) */
typedef struct sentry {
    float home[3];
    float t;
    float yaw;
    int hp;
    int watch;                 /* frames it has watched Ratchet */
    int rest;                  /* frames before it watches again */
    int32_t hum;               /* its looping hum's sound slot (-1 none) */
} sentry;

/* a gem's state */
typedef struct gem {
    float home[3];
    float t;
} gem;

/* the level's classes and sounds by name (the placed objects spawn before level_start: their spawn asks too) */
static void look_up(void) {
    s.gem_class = api->class_find("gem");
    s.snd_beep = api->sfx_find("beep");
    s.snd_zap = api->sfx_find("zap");
    s.snd_hit = api->sfx_find("hit");
    s.snd_break = api->sfx_find("break");
    s.snd_pickup = api->sfx_find("pickup");
    s.snd_chime = api->sfx_find("chime");
    s.snd_hum = api->sfx_find("hum");
}

static void make_lines(void) {
    snprintf(s.line, sizeof s.line, "Platform turns: %d   Visits: %u   Gems: %d", s.turns, (unsigned)s.visits, s.gems);
    snprintf(s.line2, sizeof s.line2, "%s", s.on_ramp ? "On the ramp" : "");
}

static void pos_of(uint32_t moby, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = rac1_lwf(api, moby + 0x10 + 4 * (uint32_t)i);
}

static float dist(const float a[3], const float b[3]) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}

/* ---- the sentry ---- */
static void sentry_spawn(void* user, uint32_t obj, const rac1_spawn* sp) {
    (void)user;
    look_up();
    sentry* e = (sentry*)api->object_state(obj);
    memcpy(e->home, sp->pos, sizeof e->home);
    e->yaw = sp->yaw;
    e->hp = SENTRY_HP;
    e->hum = api->sfx_play(s.snd_hum, api->object_moby(obj), NULL);
    char name[32] = "";
    api->object_param(obj, "name", name, sizeof name);
    char msg[96];
    snprintf(msg, sizeof msg, "sentry \"%s\" at %.1f %.1f %.1f, its hum in slot %d", name, sp->pos[0], sp->pos[1], sp->pos[2], (int)e->hum);
    api->log(msg);
}

static void sentry_remove(void* user, uint32_t obj) {
    (void)user;
    sentry* e = (sentry*)api->object_state(obj);
    if (api->sfx_active(e->hum, s.snd_hum)) api->sfx_stop(e->hum);
    e->hum = -1;
}

/* Ratchet seen: near enough, and the ray from the sentry to his chest hits nothing but him */
static int sentry_sees(uint32_t moby, const float at[3]) {
    const uint32_t hero = api->hero();
    float h[3];
    pos_of(hero, h);
    h[2] += 1.0f;
    if (dist(at, h) > SENTRY_SIGHT) return 0;
    rac1_ray_hit hit = {sizeof hit};
    return !api->collide_ray(at, h, moby, 0, &hit) || hit.moby == hero;
}

static void sentry_update(void* user, uint32_t obj) {
    (void)user;
    sentry* e = (sentry*)api->object_state(obj);
    const uint32_t m = api->object_moby(obj);
    e->t += 1.0f / 60.0f;
    float at[3] = {e->home[0], e->home[1], e->home[2] + 0.3f * sinf(e->t * 2.0f)};
    /* its hum: started again when the game let it go (a looping sound too far to be heard is dropped) */
    if (!api->sfx_active(e->hum, s.snd_hum) && ((int)(e->t * 60.0f) % 30) == 0) e->hum = api->sfx_play(s.snd_hum, m, NULL);
    if (e->rest > 0) --e->rest;
    if (e->rest == 0 && sentry_sees(m, at)) {
        float h[3];
        pos_of(api->hero(), h);
        e->yaw = atan2f(h[1] - at[1], h[0] - at[0]);
        if (e->watch++ == 0) api->sfx_play(s.snd_beep, m, NULL);
        if (e->watch >= SENTRY_AIM) {
            api->sfx_play(s.snd_zap, m, NULL);
            api->hero_damage(1, m);
            api->log("a sentry zaps Ratchet");
            e->watch = 0;
            e->rest = SENTRY_REST;
        }
    } else {
        e->watch = 0;
        e->yaw += 0.01f;
    }
    const float rot[3] = {0.0f, 0.0f, e->yaw};
    api->object_place(obj, at, rot, 0.0f);
}

static void sentry_hit(void* user, uint32_t obj, const rac1_hit* hit) {
    (void)user;
    sentry* e = (sentry*)api->object_state(obj);
    const uint32_t m = api->object_moby(obj);
    char msg[96];
    snprintf(msg, sizeof msg, "a sentry hit: kinds %08x, strength %.1f, from %08x", (unsigned)hit->kinds, hit->strength, (unsigned)hit->from);
    api->log(msg);
    if (--e->hp > 0) {
        api->sfx_play(s.snd_hit, m, NULL);
        return;
    }
    float at[3];
    pos_of(m, at);
    const float up[3] = {0.0f, 0.0f, 2.0f};
    api->smoke(at, up, 2.0f);
    api->sfx_play(s.snd_break, 0, at);
    api->bolts_add(25);
    api->object_remove(obj);  /* (its remove stops its hum) */
    api->object_spawn(s.gem_class, at, 0.0f);
    api->log("a sentry broke: a gem in its place");
}

/* ---- the gem ---- */
static void gem_spawn(void* user, uint32_t obj, const rac1_spawn* sp) {
    (void)user;
    gem* g = (gem*)api->object_state(obj);
    memcpy(g->home, sp->pos, sizeof g->home);
}

static void gem_update(void* user, uint32_t obj) {
    (void)user;
    gem* g = (gem*)api->object_state(obj);
    g->t += 1.0f / 60.0f;
    const float at[3] = {g->home[0], g->home[1], g->home[2] + 0.15f * sinf(g->t * 3.0f)};
    const float rot[3] = {0.0f, 0.0f, g->t * 2.0f};
    api->object_place(obj, at, rot, 0.0f);
    float h[3];
    pos_of(api->hero(), h);
    h[2] += 0.7f;
    if (dist(at, h) < 1.2f) {
        api->sfx_play(s.snd_pickup, 0, NULL);
        ++s.gems;
        make_lines();
        api->object_remove(obj);
        api->log("a gem taken");
    }
}

/* ---- the level ---- */
static void level_start(void* user, const rac1_level_info* level) {
    (void)user;
    s.mine = level->mine;
    if (!s.mine) return;
    look_up();
    s.gems = 0;
    s.on_ramp = 0;
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
    make_lines();
    char msg[128];
    snprintf(msg, sizeof msg, "the platform is moby %08x, visit %u; %d objects of the level's own", (unsigned)m, (unsigned)s.visits,
             RAC1_API_HAS(api, object_count) ? (int)api->object_count() : 0);
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
        make_lines();
    }
    /* eased: slow at the ends */
    const float e = 0.5f - 0.5f * cosf(s.t * 3.14159265f);
    for (int i = 0; i < 3; ++i) api->swf(m + 0x10 + 4 * (uint32_t)i, s.a[i] + (s.b[i] - s.a[i]) * e);
    api->moby_moved(m);
    return 1;
}

static void trigger(void* user, int32_t index, const char* name, int32_t inside) {
    (void)user;
    (void)index;
    if (strcmp(name, "ramp") != 0) return;
    s.on_ramp = inside;
    if (inside) api->sfx_play(s.snd_chime, 0, NULL);
    make_lines();
    api->log(inside ? "Ratchet is on the ramp" : "Ratchet left the ramp");
}

static void hud(void* user) {
    (void)user;
    if (!s.mine) return;
    api->hud_text(24, 40, 0x80f0f0f0u, s.line);
    if (s.line2[0] != 0) api->hud_text(24, 56, 0x8060f0ffu, s.line2);
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

/* testing (RAC1_MOD_TEST=<text>):
 *   log   the platform's and the hero's places every 60 frames
 *   ride  also Ratchet put on the platform at the 200th frame
 *   sdk   the level's own objects, step by step: Ratchet put by the east sentry (it sees him, zaps him), three hits sent
 *         to it (as his attacks would: it breaks, a gem in its place), Ratchet put on that gem (taken), then on the ramp
 *         (the trigger), a ray down onto the platform sentry; the objects and his health logged every 60 frames */
static void test_sdk(uint64_t frame) {
    const uint32_t hero = api->hero();
    char msg[192];
    const uint32_t east = api->object_named("east");
    if (frame == 100 && east != 0) {
        float at[3];
        pos_of(api->object_moby(east), at);
        at[0] -= 5.0f;
        at[2] = 20.5f;  /* (the floor: the level's offset is 20 up) */
        api->hero_place(at, 0.0f);
        api->log("test: Ratchet put by the east sentry");
    }
    if (frame >= 300 && frame < 303 && east != 0) {
        api->hit_send(api->object_moby(east), RAC1_HIT_PROP, 1.0f, hero);
        snprintf(msg, sizeof msg, "test: a hit sent to the east sentry (frame %llu)", (unsigned long long)frame);
        api->log(msg);
    }
    if (frame == 400) {
        uint32_t near[4];
        float h[3];
        pos_of(hero, h);
        const int32_t n = api->objects_near(h, 30.0f, near, 4);
        for (int32_t i = 0; i < n; ++i)
            if (api->object_class(near[i]) == s.gem_class) {
                float at[3];
                pos_of(api->object_moby(near[i]), at);
                at[2] -= 0.5f;
                api->hero_place(at, 0.0f);
                api->log("test: Ratchet put on the nearest gem");
                break;
            }
    }
    if (frame == 500) {
        const float at[3] = {200.0f - 18.0f, 200.0f + 0.0f, 20.0f + 4.0f};
        api->hero_place(at, 1.5708f);
        api->log("test: Ratchet put on the ramp");
    }
    if (frame == 560) {
        const uint32_t plat = api->object_named("platform");
        float from[3], to[3];
        pos_of(api->object_moby(plat), from);
        memcpy(to, from, sizeof to);
        from[2] += 10.0f;
        to[2] -= 10.0f;
        rac1_ray_hit hit = {sizeof hit};
        const int r = api->collide_ray(from, to, 0, 0, &hit);
        snprintf(msg, sizeof msg, "test: a ray down onto the platform sentry: %d, at %.2f %.2f %.2f, normal %.2f %.2f %.2f, moby %08x, object %08x (%s)", r,
                 hit.point[0], hit.point[1], hit.point[2], hit.normal[0], hit.normal[1], hit.normal[2], (unsigned)hit.moby, (unsigned)hit.object,
                 hit.object == plat ? "the sentry" : "not the sentry");
        api->log(msg);
        const float up[3] = {182.0f, 200.0f, 30.0f}, down[3] = {182.0f, 200.0f, 15.0f};
        const int r2 = api->collide_ray(up, down, hero, 0, &hit);
        snprintf(msg, sizeof msg, "test: a ray down onto the ramp: %d, at %.2f %.2f %.2f, normal %.2f %.2f %.2f, moby %08x", r2, hit.point[0], hit.point[1],
                 hit.point[2], hit.normal[0], hit.normal[1], hit.normal[2], (unsigned)hit.moby);
        api->log(msg);
    }
    if (frame % 60 != 0) return;
    float h[3];
    pos_of(hero, h);
    snprintf(msg, sizeof msg, "frame %llu: %d objects, hero at %.2f %.2f %.2f, health %d, bolts %d, gems %d, ramp %d", (unsigned long long)frame,
             (int)api->object_count(), h[0], h[1], h[2], (int)api->health(), (int)api->bolts(), s.gems, api->trigger_inside(api->trigger_find("ramp")));
    api->log(msg);
    for (int32_t i = 0; i < api->object_count(); ++i) {
        const uint32_t o = api->object_at(i);
        float at[3];
        pos_of(api->object_moby(o), at);
        snprintf(msg, sizeof msg, "  object %08x class %d at %.2f %.2f %.2f", (unsigned)o, (int)api->object_class(o), at[0], at[1], at[2]);
        api->log(msg);
    }
}

static void test(void* user, const char* text, uint64_t frame) {
    (void)user;
    if (!s.mine) return;
    if (strcmp(text, "sdk") == 0) {
        if (RAC1_API_HAS(api, class_define)) test_sdk(frame);
        return;
    }
    const uint32_t hero = api->hero();
    uint32_t m = api->moby_by_uid(PLATFORM_UID);
    if (m == 0) return;
    if (text[0] == 'r' && frame == 200) {
        const float at[3] = {rac1_lwf(api, m + 0x10), rac1_lwf(api, m + 0x14), rac1_lwf(api, m + 0x18) + 1.5f};
        api->hero_place(at, 0.0f);
        api->log("Ratchet put on the platform");
    }
    if (frame % 60 != 0) return;
    char msg[192];
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
    mod->trigger = trigger;
    /* the level's own classes (ABI 2; on an ABI 1 game the objects are not there) */
    if (RAC1_API_HAS(api, class_define)) {
        rac1_class_def d = {sizeof d};
        d.name = "sentry";
        d.state_size = sizeof(sentry);
        d.spawn = sentry_spawn;
        d.update = sentry_update;
        d.hit = sentry_hit;
        d.remove = sentry_remove;
        api->class_define(&d);
        memset(&d, 0, sizeof d);
        d.size = sizeof d;
        d.name = "gem";
        d.state_size = sizeof(gem);
        d.spawn = gem_spawn;
        d.update = gem_update;
        api->class_define(&d);
    }
    api->log("the example plugin is on");
    return 0;
}
