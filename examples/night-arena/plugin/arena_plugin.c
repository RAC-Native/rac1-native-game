/* The arena's plugin (docs/sdk.md): our own code, nothing of the game's.
 *
 * - The arena's menu: on the hub's top (the trigger "center") Triangle opens the challenges' list. Up / Down choose,
 *   Cross starts one, Square opens the vendor, Circle or Triangle close it. Ratchet is held still while it is open.
 * - The challenges: waves of Kalebo III's enemies (level.txt's spawn lines): the robots 1445, the bomb bots 52, the
 *   Gadgetron robots 541 (bomb launcher, club or flamethrower), the jetpack troopers 638, the dropships 1356 bringing
 *   troopers (or one staying to fight: the gunship). They come in at the gates and the drop zones (the spawn points'
 *   objects; the dropships land on the drop zones). A wave is over when all its enemies are dead; the last one wins the
 *   challenge (bolts, the challenge marked done in the save). Ratchet dying, or walking off to the ship's plaza (the
 *   trigger "pad"), loses it. Some limit the weapons (weapon_allow): the wrench only, a random weapon each wave
 *   (Roulette), a new one every 20 seconds (Caos). The championships (Campionato, 15 rounds; Impossibile, 50, in the
 *   menu once every other challenge is won) make their rounds at the start, the modes taking turns at random.
 * - The walkers find their way round the walls and up the ramps (the navigation below: a grid, a flow field); the
 *   troopers fly on their own (the game keeps them out of the walls: they slide along them or over them, straight).
 * - The flame vents light the floor round them while they warn and burn (light_add).
 * (The lava's rules are the game's: on a level mod Ratchet sinks in its collision's lava, jumps out, and is lost at the
 * third jump in three seconds. Every moby is updated however far: level.txt's update_all line.)
 * It needs the SDK's ABI 5 (Kalebo III's enemies, the weapons; ABI 4: vendor_open, input_hold, the HUD's panels,
 * base_hunt, light_add).
 * - The supplies: four places on the hub's top; during a challenge, every so often (the challenge's `supplies`
 *   seconds, give or take a quarter), two nanotech or two ammo crates come into the free ones (a place is free once its
 *   crate is broken: never one crate into another). Two nanotech crates there at the level's start.
 * - flame (the level's class): a flame vent in the floor: rests, warns (smoke, a hiss), bursts (a roar), hurts
 *   Ratchet standing in it; the vents take turns ("delay=" in frames).
 * - spawn (the level's class): the spawn points, hidden at their spawn.
 * - The rails (rails.txt, the classes rail_<name>): a rail challenge (for those who have the Grind Scarponi) puts
 *   Ratchet on the station outside the walls and its rails into the game's rail table: "Test Grind Scarponi" one rail
 *   down into the arena, "Grind Scarponi: Discesa" a course of three lanes with mines, barriers and drones (the classes
 *   mine, barrier, drone), then a last wave in the arena (the grind section below).
 *
 * RAC1_MOD_TEST=arena (headless): Ratchet put on the hub at frame 100, the first challenge started at frame 160, a
 * line every second. The other tests: before test() below.
 */
#include <rac1_mod.h>

#include <math.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

RAC1_MOD_DECLARE_ABI()

static const rac1_api* api;

/* ---- the game's (the same address on every level) ---- */
#define HERO_STATE 0x001413d4u  /* the hero's state: the player block 0x0013f350 + 0x2084 (the tests' lines) */
#define HERO_ITEM 0x00140408u   /* the item in his hand: P + 0x10b8 (the tests' lines) */
#define ROBOT 1445              /* Kalebo III's robots */
#define WRENCH 8                /* the wrench's item */

#define PAD_TRIANGLE 0x10u
#define PAD_CIRCLE 0x20u
#define PAD_CROSS 0x40u
#define PAD_SQUARE 0x80u
#define PAD_UP 0x1000u
#define PAD_DOWN 0x4000u

#define WHITE 0x80ffffffu  /* 0xAABBGGRR, alpha 0x80 opaque */
#define GREY 0x80a0a0a0u
#define YELLOW 0x8020d0ffu
#define GREEN 0x8050ff60u
#define RED 0x804050ffu
#define CYAN 0x80ffe060u
#define PANEL 0x5a28140au   /* a dark blue panel, three quarters opaque */
#define BAR 0x8020c0ffu     /* its yellow top edge */
#define PICK 0x26ffffffu    /* the menu's chosen line, a faint white band */
#define RULE 0x30ffffffu    /* a thin faint line between a panel's parts */
#define BOX_EMPTY 0x20ffffffu  /* the menu's mark of a challenge not won yet (won: GREEN) */

/* ---- the enemies (Kalebo III's, SDK ABI 5) ---- */
enum { K_ROBOT, K_BOMBER, K_LAUNCHER, K_CLUB, K_FLAMER, K_JET, K_SHIP, K_GUNSHIP, N_KINDS };

static const struct kind_def {
    char letter;   /* in a wave's mix */
    int32_t cls;   /* the class (level.txt's spawn line) */
    int weapon;    /* a Gadgetron robot's (base_option RAC1_BASE_WEAPON), -1 none */
    int moves;     /* 0 walks (the paths), 1 flies (its own flight: the game keeps it out of the walls), 2 a dropship */
    float step;    /* units a frame along a path (measured: their own pace) */
} KIND[N_KINDS] = {
    {'R', 1445, -1, 0, 0.07f},  /* the robots */
    {'B', 52, -1, 0, 0.12f},    /* the bomb bots: roll at Ratchet and blow up */
    {'L', 541, 0, 0, 0.08f},    /* a Gadgetron robot with the bomb launcher (keeps 9 units off) */
    {'C', 541, 1, 0, 0.10f},    /* with the club */
    {'F', 541, 2, 0, 0.085f},   /* with the flamethrower */
    {'J', 638, -1, 1, 0.0f},    /* a jetpack trooper, starting on a drop zone */
    {'S', 1356, -1, 2, 0.0f},   /* a dropship: lands on a drop zone, drops 3 troopers, flies off */
    {'G', 1356, -1, 2, 0.0f},   /* the gunship: drops 2 troopers and stays to fight (counted, as they are) */
};

/* ---- the challenges ---- */
typedef struct wave_def {
    int count;        /* enemies in the wave (a dropship's troopers each one, the gunship one) */
    int at_once;      /* alive at once, at most */
    int every;        /* frames between two arrivals */
    const char* mix;  /* the kinds' letters, one taken at random for each arrival ("RRB": two robots to a bomb bot);
                         a G first: the gunship opens the wave (once) */
} wave_def;

enum { WAVES, SURVIVE, TIMED, CUP, RAIL };
enum { W_ANY, W_WRENCH, W_ROULETTE, W_CHAOS };  /* the weapons allowed */

typedef struct challenge_def {
    const char* name;
    const char* text;  /* the menu's line about it */
    int kind;          /* WAVES: every wave beaten; SURVIVE: alive when the time runs out (wave[0] without end);
                          TIMED: every wave beaten within the time; CUP: `waves` rounds made at its start, the modes
                          (MODES) taking turns at random; RAIL: the rail ridden to its end within the time (a test of
                          the arena's own rails; not needed for Impossibile) */
    int time;          /* seconds (SURVIVE, TIMED) */
    int waves;
    wave_def wave[8];
    int bombs;         /* frames between two bombs from the sky, about (0: none) */
    int supplies;      /* seconds between two deliveries of crates on the hub (0: none; the wrench only: nanotech only) */
    int vents;         /* 1: the flame vents burn three times as often */
    int nohit;         /* 1: one hit loses it */
    int weapons;       /* W_ANY; W_WRENCH the wrench only; W_ROULETTE a random weapon each wave; W_CHAOS a new one
                          every CHAOS_FRAMES */
    int reward;        /* bolts */
    int hidden;        /* 1: in the menu once every other challenge is won */
    const char* rails; /* RAIL: its rails (rails.txt's names, the first the one whose end finishes it) */
    const char* start; /* RAIL: the spawn point Ratchet starts from */
    int obstacles;     /* RAIL: the course's obstacles (mines, barriers, drones) on */
} challenge_def;

static const challenge_def CHALLENGES[] = {
    /* name, line, kind, time, waves {count, at once, every, mix}, bombs, supplies, vents, nohit, weapons, reward */
    {"Primo sangue", "5 ondate di robot", WAVES, 0, 5,
     {{6, 4, 45, "R"}, {8, 5, 40, "R"}, {10, 6, 35, "RRB"}, {12, 7, 30, "RRB"}, {14, 8, 30, "RRRB"}}, 0, 40, 0, 0, W_ANY, 1500},
    {"Manichini", "i robot Gadgetron armati", WAVES, 0, 5,
     {{6, 4, 40, "RRF"}, {8, 5, 35, "RFC"}, {10, 6, 30, "RFCB"}, {12, 7, 28, "RFCL"}, {14, 8, 25, "RBFCL"}}, 0, 35, 0, 0, W_ANY, 2500},
    {"Sbarco", "navi e soldati col jetpack", WAVES, 0, 5,
     {{6, 6, 60, "S"}, {8, 7, 45, "SR"}, {10, 8, 40, "SJR"}, {12, 9, 35, "SJRB"}, {14, 10, 30, "SJRBF"}}, 0, 30, 0, 0, W_ANY, 3000},
    {"Assedio", "5 ondate, grate impazzite", WAVES, 0, 5,
     {{10, 8, 30, "RRB"}, {12, 9, 28, "RRBF"}, {14, 10, 25, "RBFC"}, {18, 12, 20, "RBFCJ"}, {22, 14, 15, "RBFCJL"}}, 0, 30, 1, 0,
     W_ANY, 3500},
    {"Pioggia di fuoco", "5 ondate sotto le bombe", WAVES, 0, 5,
     {{8, 6, 35, "RL"}, {10, 7, 32, "RBL"}, {12, 8, 30, "RBL"}, {14, 9, 28, "RBLJ"}, {16, 10, 25, "RBLJF"}}, 150, 30, 0, 0, W_ANY,
     4000},
    {"Solo chiave", "5 ondate, solo la chiave inglese", WAVES, 0, 5,
     {{6, 4, 40, "R"}, {7, 5, 38, "RB"}, {8, 5, 35, "RB"}, {9, 6, 32, "RRC"}, {10, 6, 30, "RRCB"}}, 0, 30, 0, 0, W_WRENCH, 4500},
    {"Roulette", "un'arma a caso a ogni ondata", WAVES, 0, 5,
     {{8, 6, 35, "RB"}, {10, 7, 32, "RBF"}, {12, 8, 30, "RBF"}, {14, 10, 25, "RBFJ"}, {16, 10, 20, "RBFLS"}}, 0, 30, 0, 0,
     W_ROULETTE, 5000},
    {"Contro il tempo", "5 ondate in 3 minuti", TIMED, 180, 5,
     {{10, 8, 22, "RB"}, {12, 10, 18, "RB"}, {14, 12, 15, "RBF"}, {16, 12, 14, "RBFJ"}, {18, 14, 12, "RBFJ"}}, 0, 25, 0, 0, W_ANY,
     5500},
    {"Intoccabile", "3 ondate senza un graffio", WAVES, 0, 3, {{6, 5, 40, "R"}, {8, 6, 35, "R"}, {10, 7, 30, "RRB"}}, 0, 0, 0, 1,
     W_ANY, 6000},
    {"Caos", "4 minuti, l'arma cambia ogni 20 s", SURVIVE, 240, 1, {{9999, 12, 25, "RBFJS"}}, 0, 20, 0, 0, W_CHAOS, 7000},
    {"Ultimo in piedi", "resisti 2 minuti e mezzo", SURVIVE, 150, 1, {{9999, 16, 15, "RRBFLCJS"}}, 170, 20, 1, 0, W_ANY, 7500},
    {"Inferno", "8 ondate, bombe, grate, cannoniera", WAVES, 0, 8,
     {{12, 10, 20, "RB"}, {14, 11, 18, "RBF"}, {16, 12, 15, "RBFL"}, {18, 14, 12, "RBFLJ"}, {20, 14, 10, "RBFCJS"},
      {22, 16, 10, "RBFLCJ"}, {24, 16, 9, "RBFLCJS"}, {20, 16, 8, "GRBFJ"}},
     120, 25, 1, 0, W_ANY, 12000},
    {"Campionato", "15 round, le modalita' a caso", CUP, 0, 15, {{0}}, 0, 25, 0, 0, W_ANY, 20000},
    {"Test Grind Scarponi", "prova: giu' per la rotaia fino all'arena", RAIL, 45, 1, {{0}}, 0, 0, 0, 0, W_ANY, 1000, 0,
     "test", "rail_start", 0},
    {"Grind Scarponi: Discesa", "3 corsie, ostacoli e droni, poi l'ultima ondata", RAIL, 90, 1, {{8, 6, 25, "RRB"}}, 0, 20,
     0, 0, W_ANY, 3000, 0, "lane_c lane_l lane_r", "rail_course", 1},
    {"Impossibile", "il campionato, 50 round", CUP, 0, 50, {{0}}, 0, 20, 0, 0, W_ANY, 100000, 1},
};
#define N_CHALLENGES ((int)(sizeof CHALLENGES / sizeof CHALLENGES[0]))

#define CHAOS_FRAMES 1200  /* Caos: a new weapon every 20 seconds */

/* ---- the championships (CUP): their rounds, made at the start; the modes take turns, a few rounds each ---- */
enum { M_FREE, M_WRENCH, M_ROULETTE, M_CHAOS, M_BOMBS, M_VENTS, M_TIMED, N_MODES };

static const struct {
    const char* name;
    int min, max;  /* rounds in a row */
} MODES[N_MODES] = {
    {"Libero", 1, 3}, {"Solo chiave", 1, 2}, {"Roulette", 2, 4}, {"Caos", 4, 5}, {"Pioggia di fuoco", 1, 3},
    {"Grate impazzite", 1, 3}, {"Contro il tempo", 1, 2},
};

#define MAX_ROUNDS 50
#define CUP_BOMBS 150  /* a bombs round's frames between two bombs */

static struct {
    wave_def w;
    char mix[16];
    int mode;
} cup[MAX_ROUNDS];

/* the rules of a wave (a round): the weapons, the bombs, the vents, a round's time (seconds, 0 none) */
typedef struct rules {
    int weapons, bombs, vents, time;
} rules;

/* the weapons (weapons_owned's items), their names; the Taunter and the Decoy Glove kill nothing: never drawn */
static const struct {
    int32_t item;
    const char* name;
} WEAPONS[] = {{0x09, "Suck Cannon"}, {0x0a, "Bomb Glove"}, {0x0b, "Devastator"}, {0x0d, "Visibomb Gun"}, {0x0e, NULL},
               {0x0f, "Blaster"}, {0x10, "Pyrocitor"}, {0x11, "Mine Glove"}, {0x12, "Walloper"}, {0x13, "Tesla Claw"},
               {0x14, "Glove of Doom"}, {0x15, "Morph-o-Ray"}, {0x17, "R.Y.N.O."}, {0x18, "Drone Device"}, {0x19, NULL}};
#define N_WEAPONS ((int)(sizeof WEAPONS / sizeof WEAPONS[0]))

/* where the enemies come in: the ground spawn points (level.txt's spawn objects, by name); the dropships land on the
   drop zones */
static const char* const POINTS[] = {"gate_n", "gate_e", "gate_w", "drop_n", "drop_e", "drop_w"};
#define N_POINTS ((int)(sizeof POINTS / sizeof POINTS[0]))
#define MAX_EN 40  /* enemies at once (level.txt's spawn lines: 24 + 8 + 6 + 8 + 2) */

typedef struct enemy {
    uint32_t h;      /* base_spawn's handle */
    int kind;
    uint32_t ship;   /* a trooper: the dropship that brought it (0 none) */
    float to[2];     /* where it walks: a point along the path, or Ratchet */
    int direct;      /* Ratchet in sight: straight at him (its own code) */
    float prev[3];   /* where it stood after the last frame's steering */
    int has_prev;
    float vz;        /* falling: the plugin's gravity (their own code does not drop them off a ledge) */
} enemy;

enum { FREE, MENU, INTRO, FIGHT, BREAK, RESULT };

#define INTRO_FRAMES 180
#define BREAK_FRAMES 150
#define RESULT_FRAMES 300

static struct {
    int mine;
    int mode, t;  /* the mode, frames in it */
    int look;     /* (RAC1_MOD_TEST=arena_look: the flames kept burning) */
    int sel;      /* the menu's line (in the list it shows) */
    int scroll;   /* its first row shown */
    int ch, wave; /* the challenge running, its wave */
    int spawned, killed, next_point, spawn_wait;
    int felled;   /* enemies down in the whole challenge */
    int health0;  /* (nohit) Ratchet's health: lower loses */
    int frenzy;   /* the vents burn three times as often */
    int bomb_wait, supply_wait;
    int clock;    /* the challenge's frames */
    enemy en[MAX_EN];
    int nen;
    uint32_t ships[2];   /* the dropships that leave (not counted: their troopers are) */
    int ship_drop[2];    /* the drop zone each landed on (one ship a zone) */
    rules rule;          /* the wave's (the round's) */
    int round_clock;     /* the wave's frames (a championship's timed round) */
    int32_t wpn;         /* the weapon a challenge put in his hand (WRENCH: the wrench only), 0 none */
    int wpn_t;           /* Caos: frames to the next one */
    int wpn_ask;         /* weapon_equip to ask again (it says 0 while he swings, switches...) */
    int won;
    int lost_reload;  /* a challenge on when the level ended (Ratchet lost in the lava: the game loads it again) */
    char why[32];  /* the lost banner's second line */
    float last_h[3];  /* Ratchet's place the frame before: a jump of it is a death (he comes back at his start) */
    int have_last;
    int placed;       /* frames the plugin's own move of him (hero_move) is not taken for a death */
    int32_t t_center, t_pad;
    float points[N_POINTS][3];
    int is_drop[N_POINTS];  /* a drop zone's (a dropship lands there) */
    const char* point_name[N_POINTS];
    int npoints;
    float centre[3];
    float lava_z;  /* (the navigation's floor: 1.5 over it) */
    int lava_ok;
    uint32_t done;                 /* the challenges won (saved) */
    uint32_t best[N_CHALLENGES];   /* their best times, frames (saved; 0 none) */
    int32_t snd_wave, snd_win, snd_fail;
    char toast[80];
    int toast_t;
    /* the HUD's texts and panels, made in frame (hud may run twice a frame: it only draws them) */
    int nlines, nrects;
    struct {
        int16_t x, y;
        uint32_t colour;
        float scale;
        uint8_t anchor, centred;  /* RAC1_HUD_*; centred on x */
        int16_t maxw;  /* its width at most (made smaller to fit), 0 any */
        char text[96];
    } line[15];  /* (the game draws 15 texts a frame at most) */
    struct {
        int16_t x, y, w, h;
        uint32_t colour;
        uint8_t anchor;
        int8_t fit_first, fit_n;  /* widened round these texts (20 units each side) when they reach past it */
        int16_t grow;             /* (a box at the screen's left) widened to the right instead, to this width at most */
    } rect[32];
} s;

/* a text of the HUD (512 x 448 units): at `scale` times the font, toward `anchor`'s edge, its left at x or its middle */
static void hud_text_at(int x, int y, uint32_t colour, float scale, int anchor, int centred, const char* fmt, ...)
    __attribute__((format(printf, 7, 8)));

static void hud_text_at(int x, int y, uint32_t colour, float scale, int anchor, int centred, const char* fmt, ...) {
    if (s.nlines >= 15) return;
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(s.line[s.nlines].text, sizeof s.line[s.nlines].text, fmt, ap);
    va_end(ap);
    s.line[s.nlines].x = (int16_t)x;
    s.line[s.nlines].y = (int16_t)y;
    s.line[s.nlines].colour = colour;
    s.line[s.nlines].scale = scale;
    s.line[s.nlines].anchor = (uint8_t)anchor;
    s.line[s.nlines].centred = (uint8_t)centred;
    s.line[s.nlines].maxw = 0;
    ++s.nlines;
}

/* the last text at most `w` units wide (smaller when it is wider) */
static void hud_maxw(int w) {
    if (s.nlines > 0) s.line[s.nlines - 1].maxw = (int16_t)w;
}

/* a panel behind texts */
static void hud_panel(int x, int y, int w, int h, uint32_t colour, int anchor) {
    if (s.nrects >= 32) return;
    s.rect[s.nrects].x = (int16_t)x;
    s.rect[s.nrects].y = (int16_t)y;
    s.rect[s.nrects].w = (int16_t)w;
    s.rect[s.nrects].h = (int16_t)h;
    s.rect[s.nrects].colour = colour;
    s.rect[s.nrects].anchor = (uint8_t)anchor;
    s.rect[s.nrects].fit_n = 0;
    s.rect[s.nrects].grow = 0;
    ++s.nrects;
}

/* the last `nrects` panels fitted round the last `nlines` texts */
static void hud_fit(int nrects, int nlines) {
    for (int i = s.nrects - nrects; i < s.nrects; ++i) {
        s.rect[i].fit_first = (int8_t)(s.nlines - nlines);
        s.rect[i].fit_n = (int8_t)nlines;
    }
}

/* panel `rect` (a box with its left edge kept) widened to the right round the texts from `first` on, `wmax` wide at
   most; those texts made smaller past that */
static void hud_grow(int rect, int first, int wmax) {
    if (rect < 0 || rect >= s.nrects) return;
    s.rect[rect].fit_first = (int8_t)first;
    s.rect[rect].fit_n = (int8_t)(s.nlines - first);
    s.rect[rect].grow = (int16_t)wmax;
    for (int k = first; k < s.nlines; ++k)
        if (s.line[k].maxw == 0 || s.line[k].maxw > wmax - (s.line[k].x - s.rect[rect].x) - 10)
            s.line[k].maxw = (int16_t)(wmax - (s.line[k].x - s.rect[rect].x) - 10);
}

/* a banner in the middle of the screen: a panel, a large line, a smaller one under it (or none) */
static void banner(uint32_t colour, const char* big, const char* small) {
    hud_panel(116, 136, 280, small ? 66 : 44, PANEL, RAC1_HUD_CENTRE);
    hud_panel(116, 136, 280, 3, BAR, RAC1_HUD_CENTRE);
    hud_text_at(256, 146, colour, 1.6f, RAC1_HUD_CENTRE, 1, "%s", big);
    hud_maxw(440);
    if (small) hud_text_at(256, 174, WHITE, 1.2f, RAC1_HUD_CENTRE, 1, "%s", small);
    if (small) hud_maxw(440);
    hud_fit(2, small ? 2 : 1);
}

static void toast(const char* text) {
    snprintf(s.toast, sizeof s.toast, "%s", text);
    s.toast_t = 180;
}

static void pos_of(uint32_t moby, float out[3]) {
    for (int i = 0; i < 3; ++i) out[i] = rac1_lwf(api, moby + 0x10 + 4 * (uint32_t)i);
}

static float rnd(void) {
    return (float)(api->random() & 0xffffu) / 65535.0f;
}

/* the flame vents (their objects), for their lights */
static uint32_t flames[8];
static int nflames;

static void play(int32_t snd) {
    if (snd >= 0) api->sfx_play(snd, 0, NULL);
}

/* ---- the navigation (pathfinding) ----
 *
 * The game's enemies have none: they walk straight at their target. Here a grid of 1-unit cells over the arena
 * (from the hub's point: x -66 .. 66, y -92 .. 66, the alcoves and the ship's plaza in) is probed with the level's
 * collision when the level starts, a few rows a frame (nav_build):
 * - each cell's walkable heights, rays straight down: the topmost surface, and one more under it when there is
 *   headroom (a gate's lintel over the floor); the lava's level and below is not walkable;
 * - the links between neighbouring cells (8 ways): a walker climbs at most NAV_STEP between two cells (the ramps' slope
 *   is a third of a unit per unit: they link; a low wall, a tower's side do not), and a ray at knee height between
 *   them must hit nothing of the level (walls, cover, pillars, posts).
 * A node is a cell's height (two at most). Every half second (nav_field) a Dijkstra from Ratchet's node gives every
 * node the step toward him (its next node) - a flow field: one search serves every robot. Cells by a wall cost more,
 * so the paths keep off the walls; a diagonal step needs both straight ones beside it (no corner cut).
 * A robot (nav_steer) walks straight at Ratchet when a knee-high ray from it to him hits nothing of the level and
 * they stand at about one height; else at the node NAV_AHEAD steps along its path from where it is.
 * The jetpack troopers fly on their own: the game keeps them out of the walls (they slide along them or over them).
 */
#define NAV_W 132
#define NAV_H 158
#define NAV_X0 (-66.0f)
#define NAV_Y0 (-92.0f)
#define NAV_CELLS (NAV_W * NAV_H)
#define NAV_NODES (2 * NAV_CELLS)
#define NAV_STEP 0.75f       /* the height climbed between two neighbouring cells */
#define NAV_HEADROOM 1.8f    /* under a surface: room for a robot */
#define NAV_KNEE 0.7f        /* the links' rays, over the ground */
#define NAV_BUDGET 1500      /* rays a frame while the grid is built */
#define NAV_AHEAD 4          /* steps along the path a robot walks at */
#define NAV_NONE 0xffffu

static const int NAV_DX[8] = {1, 1, 0, -1, -1, -1, 0, 1}, NAV_DY[8] = {0, 1, 1, 1, 0, -1, -1, -1};

static struct {
    int state;  /* 0 none, 1 heights, 2 links, 3 ready */
    int row, rays, frames;
    float ox, oy, floor_z;
    float h[NAV_NODES];        /* a node's height (level 0: the cell's top surface, 1: one under it); NAN none */
    uint8_t link[NAV_NODES];   /* bit d: linked to the neighbour cell d */
    uint8_t lvl[NAV_NODES];    /* bit d: that link's node is the neighbour's level 1 */
    uint8_t edge_cost[NAV_NODES];  /* more by a wall */
    uint16_t dist[NAV_NODES];
    int32_t next[NAV_NODES];   /* the step toward the target; -1 none */
    int32_t heap[8 * NAV_NODES];
    int target, field_t;
    int nodes, links;
} nav;

static int nav_hit(const float a[3], const float b[3], float* z, int level_only) {
    /* the level's collision along a -> b (Ratchet left out; a moby hit is passed through once: a crate is no wall);
       1 and the hit's height in *z */
    rac1_ray_hit hit = {sizeof hit};
    const uint32_t hero = api->hero();
    ++nav.rays;
    if (!api->collide_ray(a, b, hero, 0, &hit)) return 0;
    if (hit.moby != 0) {
        if (!level_only) return 0;
        const float a2[3] = {a[0], a[1], hit.point[2] - 0.05f};
        ++nav.rays;
        if (a[2] == b[2] || !api->collide_ray(a2, b, hit.moby, 0, &hit) || hit.moby != 0) return 0;
    }
    if (z) *z = hit.point[2];
    return 1;
}

static void nav_reset(void) {
    nav.state = 1;
    nav.row = nav.rays = nav.frames = 0;
    nav.ox = s.centre[0] + NAV_X0;
    nav.oy = s.centre[1] + NAV_Y0;
    nav.floor_z = s.lava_ok ? s.lava_z + 1.5f : s.centre[2] - 3.0f;
    nav.target = -1;
    nav.nodes = nav.links = 0;
    for (int i = 0; i < NAV_NODES; ++i) {
        nav.h[i] = NAN;
        nav.link[i] = nav.lvl[i] = 0;
        nav.edge_cost[i] = 0;
    }
}

static void nav_heights_row(int r) {
    for (int c = 0; c < NAV_W; ++c) {
        const int cell = r * NAV_W + c;
        const float x = nav.ox + (float)c + 0.5f, y = nav.oy + (float)r + 0.5f, bottom = nav.floor_z - 2.5f;
        const float top[3] = {x, y, nav.floor_z + 14.0f}, down[3] = {x, y, bottom};
        float z1, z2;
        if (!nav_hit(top, down, &z1, 1) || z1 < nav.floor_z - 0.8f) continue;  /* nothing, or the lava */
        nav.h[cell] = z1;
        ++nav.nodes;
        const float under[3] = {x, y, z1 - 0.05f};
        if (nav_hit(under, down, &z2, 1) && z1 - z2 >= NAV_HEADROOM && z2 >= nav.floor_z - 0.8f) {
            nav.h[NAV_CELLS + cell] = z2;
            ++nav.nodes;
        }
    }
}

static void nav_links_row(int r) {
    for (int c = 0; c < NAV_W; ++c)
        for (int l = 0; l < 2; ++l) {
            const int n = l * NAV_CELLS + r * NAV_W + c;
            if (isnan(nav.h[n])) continue;
            for (int d = 0; d < 4; ++d) {  /* east, north-east, north, north-west: the other four are theirs */
                const int c2 = c + NAV_DX[d], r2 = r + NAV_DY[d];
                if (c2 < 0 || c2 >= NAV_W || r2 < 0 || r2 >= NAV_H) continue;
                const float climb = NAV_STEP * ((d & 1) ? 1.42f : 1.0f);
                for (int l2 = 0; l2 < 2; ++l2) {
                    const int n2 = l2 * NAV_CELLS + r2 * NAV_W + c2;
                    if (isnan(nav.h[n2]) || fabsf(nav.h[n2] - nav.h[n]) > climb) continue;
                    const float a[3] = {nav.ox + (float)c + 0.5f, nav.oy + (float)r + 0.5f, nav.h[n] + NAV_KNEE};
                    const float b[3] = {nav.ox + (float)c2 + 0.5f, nav.oy + (float)r2 + 0.5f, nav.h[n2] + NAV_KNEE};
                    if (nav_hit(a, b, NULL, 0)) continue;
                    nav.link[n] |= (uint8_t)(1u << d);
                    nav.link[n2] |= (uint8_t)(1u << ((d + 4) & 7));
                    if (l2) nav.lvl[n] |= (uint8_t)(1u << d);
                    if (l) nav.lvl[n2] |= (uint8_t)(1u << ((d + 4) & 7));
                    ++nav.links;
                    break;
                }
            }
        }
}

/* the grid, a few rows a frame until NAV_BUDGET rays are spent */
static void nav_build(void) {
    if (nav.state == 0 || nav.state == 3) return;
    ++nav.frames;
    const int start = nav.rays;
    while (nav.rays - start < NAV_BUDGET && nav.state < 3) {
        if (nav.state == 1) nav_heights_row(nav.row);
        else nav_links_row(nav.row);
        if (++nav.row >= NAV_H) {
            nav.row = 0;
            ++nav.state;
        }
    }
    if (nav.state == 3) {
        for (int n = 0; n < NAV_NODES; ++n)  /* by a wall (a link missing): dearer, the paths keep off */
            if (!isnan(nav.h[n]) && nav.link[n] != 0xff) nav.edge_cost[n] = 8;
        char msg[160];
        snprintf(msg, sizeof msg, "navigation: %d nodes, %d links, %d rays in %d frames", nav.nodes, nav.links, nav.rays, nav.frames);
        api->log(msg);
    }
}

static int nav_node_at(const float p[3]) {
    const int c = (int)floorf(p[0] - nav.ox), r = (int)floorf(p[1] - nav.oy);
    if (c < 0 || c >= NAV_W || r < 0 || r >= NAV_H) return -1;
    int best = -1;
    float bd = 3.0f;
    for (int l = 0; l < 2; ++l) {
        const int n = l * NAV_CELLS + r * NAV_W + c;
        if (isnan(nav.h[n])) continue;
        const float d = fabsf(p[2] - nav.h[n]);
        if (d < bd) {
            bd = d;
            best = n;
        }
    }
    return best;
}

static int nav_neighbour(int n, int d) {
    const int cell = n % NAV_CELLS;
    const int c = cell % NAV_W + NAV_DX[d], r = cell / NAV_W + NAV_DY[d];
    return ((nav.lvl[n] >> d) & 1) * NAV_CELLS + r * NAV_W + c;
}

static void heap_push(int* n, int32_t node) {
    int i = (*n)++;
    nav.heap[i] = node;
    while (i > 0) {
        const int p = (i - 1) / 2;
        if (nav.dist[nav.heap[p]] <= nav.dist[nav.heap[i]]) break;
        const int32_t t = nav.heap[p];
        nav.heap[p] = nav.heap[i];
        nav.heap[i] = t;
        i = p;
    }
}

static int32_t heap_pop(int* n, uint16_t* key) {
    const int32_t top = nav.heap[0];
    *key = nav.dist[top];
    nav.heap[0] = nav.heap[--(*n)];
    for (int i = 0;;) {
        const int a = 2 * i + 1, b = a + 1;
        int m = i;
        if (a < *n && nav.dist[nav.heap[a]] < nav.dist[nav.heap[m]]) m = a;
        if (b < *n && nav.dist[nav.heap[b]] < nav.dist[nav.heap[m]]) m = b;
        if (m == i) break;
        const int32_t t = nav.heap[m];
        nav.heap[m] = nav.heap[i];
        nav.heap[i] = t;
        i = m;
    }
    return top;
}

/* the flow field toward the node `target` (a Dijkstra over the links: 10 a straight step, 14 a diagonal one, more by
   a wall); the heap keeps a node's older entries and skips them when popped */
static void nav_field(int target) {
    for (int i = 0; i < NAV_NODES; ++i) {
        nav.dist[i] = NAV_NONE;
        nav.next[i] = -1;
    }
    nav.target = target;
    if (target < 0) return;
    int n = 0;
    nav.dist[target] = 0;
    heap_push(&n, target);
    while (n > 0) {
        uint16_t key;
        const int32_t u = heap_pop(&n, &key);
        if (key != nav.dist[u]) continue;  /* (an older entry) */
        for (int d = 0; d < 8; ++d) {
            if (!((nav.link[u] >> d) & 1)) continue;
            if ((d & 1) && !(((nav.link[u] >> ((d + 7) & 7)) & 1) && ((nav.link[u] >> ((d + 1) & 7)) & 1))) continue;
            const int32_t v = nav_neighbour(u, d);
            const uint32_t w = (uint32_t)nav.dist[u] + ((d & 1) ? 14u : 10u) + nav.edge_cost[v];
            if (w < nav.dist[v] && n < 8 * NAV_NODES - 1) {
                nav.dist[v] = (uint16_t)(w < NAV_NONE ? w : NAV_NONE - 1);
                nav.next[v] = u;
                heap_push(&n, v);
            }
        }
    }
}

/* the point NAV_AHEAD steps along the flow field's path from `r` toward Ratchet: 1 and `to`, 0 none (off the grid, no
   path) */
static int nav_toward(const float r[3], float to[2]) {
    if (nav.state != 3 || nav.target < 0) return 0;
    int n = nav_node_at(r);
    if (n < 0 || nav.dist[n] == NAV_NONE) return 0;
    const float h0 = nav.h[n];
    for (int k = 0; k < NAV_AHEAD && nav.next[n] >= 0; ++k) {  /* (on a slope only a step or two: no cutting off a ramp's side) */
        if (k > 0 && fabsf(nav.h[nav.next[n]] - h0) > 0.4f) break;
        n = nav.next[n];
    }
    const int cell = n % NAV_CELLS;
    to[0] = nav.ox + (float)(cell % NAV_W) + 0.5f;
    to[1] = nav.oy + (float)(cell / NAV_W) + 0.5f;
    return 1;
}

/* where a walker at `r` should walk to reach Ratchet at `h`: *direct 1 (straight at him) or `to` */
static void nav_steer(uint32_t robot, const float r[3], const float h[3], float to[2], int* direct) {
    *direct = 1;
    if (nav.state != 3 || nav.target < 0) return;
    if (fabsf(r[2] - h[2]) < 1.5f) {  /* in sight at about one height: straight at him */
        const float a[3] = {r[0], r[1], r[2] + NAV_KNEE}, b[3] = {h[0], h[1], h[2] + NAV_KNEE};
        rac1_ray_hit hit = {sizeof hit};
        if (!api->collide_ray(a, b, robot, 0, &hit) || hit.moby != 0) return;
    }
    if (nav_toward(r, to)) *direct = 0;
}

/* the field again every half second while robots are out, when Ratchet stands on another node */
static void nav_track(const float h[3]) {
    if (nav.state != 3) return;
    const int t = nav_node_at(h);
    if (++nav.field_t >= 30 || (t >= 0 && t != nav.target && nav.field_t >= 10)) {
        nav.field_t = 0;
        if (t >= 0) nav_field(t);
    }
}

/* ---- the challenges ---- */

static void bombs_clear(void);
static void bomb_drop(const float h[3]);
static void weapon_pick(void);
static void rail_begin(void);
static void rail_go(void);
static void rail_stop(void);

#define JET_CARRIED 10  /* a trooper's state in its dropship (hidden, carried) */
#define GADGET_HIT 8    /* a Gadgetron robot's state while a hit throws it */

/* the enemies still alive (the dead ones counted: killed, felled); a trooper still in a dropship shot down goes with it */
static int enemies_alive(void) {
    int n = 0;
    for (int i = 0; i < s.nen;) {
        const enemy* e = &s.en[i];
        uint32_t m = api->base_moby(e->h);
        if (m != 0 && e->kind == K_JET && e->ship != 0 && rac1_lb(api, m + 0x20) == JET_CARRIED && api->base_moby(e->ship) == 0) {
            api->moby_remove(m);
            m = 0;
        }
        if (m != 0) {
            ++n;
            ++i;
        } else {  /* gone: killed */
            s.en[i] = s.en[--s.nen];
            ++s.killed;
            ++s.felled;
        }
    }
    return n;
}

static void enemies_clear(void) {
    for (int i = 0; i < s.nen; ++i) {
        const uint32_t m = api->base_moby(s.en[i].h);
        if (m != 0) api->moby_remove(m);
    }
    s.nen = 0;
    for (int k = 0; k < 2; ++k) {
        const uint32_t m = s.ships[k] ? api->base_moby(s.ships[k]) : 0;
        if (m != 0) api->moby_remove(m);
        s.ships[k] = 0;
    }
}

/* the wave (the round) being fought */
static const wave_def* cur_wave(void) {
    return CHALLENGES[s.ch].kind == CUP ? &cup[s.wave].w : &CHALLENGES[s.ch].wave[s.wave];
}

/* a championship's rounds: the modes in turns of MODES[m].min..max rounds (Libero first, never one mode twice in a
   row, Caos only with its four rounds left); the enemies more, more at once, sooner and of more kinds round after
   round (`d` 0 .. 1 over 15 rounds; 50 rounds: 0.2 .. 1.6, past anything else); a gunship opens every fifth round
   from d 0.4 on; a wrench round has fewer and weaker ones */
static void cup_make(int rounds) {
    int r = 0, prev = -1;
    while (r < rounds) {
        int m = M_FREE;
        if (r > 0)
            do m = (int)(rnd() * (float)N_MODES) % N_MODES;
            while (m == prev || (m == M_CHAOS && rounds - r < MODES[M_CHAOS].min));
        int len = MODES[m].min + (int)(rnd() * (float)(MODES[m].max - MODES[m].min + 1)) % (MODES[m].max - MODES[m].min + 1);
        if (len > rounds - r) len = rounds - r;
        for (int k = 0; k < len; ++k, ++r) cup[r].mode = m;
        prev = m;
    }
    const float d0 = rounds > 20 ? 0.2f : 0.0f, d1 = rounds > 20 ? 1.6f : 1.0f;
    for (r = 0; r < rounds; ++r) {
        const float d = d0 + (d1 - d0) * (float)r / (float)(rounds > 1 ? rounds - 1 : 1);
        const int wrench = cup[r].mode == M_WRENCH;
        char* x = cup[r].mix;
        int n = 0;
        if ((r + 1) % 5 == 0 && d >= 0.4f && !wrench) x[n++] = 'G';
        x[n++] = 'R';
        x[n++] = 'R';
        if (d >= 0.1f) x[n++] = 'B';
        if (d >= 0.2f && !wrench) x[n++] = 'F';
        if (d >= 0.3f && !wrench) x[n++] = 'J';
        if (d >= 0.4f) x[n++] = 'C';
        if (d >= 0.5f && !wrench) x[n++] = 'L';
        if (d >= 0.6f && !wrench) x[n++] = 'S';
        if (d >= 1.0f) x[n++] = 'B', x[n++] = wrench ? 'R' : 'J';
        x[n] = 0;
        wave_def* w = &cup[r].w;
        w->count = (int)((8.0f + 14.0f * d) * (wrench ? 0.7f : 1.0f));
        w->at_once = (int)(5.0f + 9.0f * d);
        w->every = (int)fmaxf(8.0f, 40.0f - 24.0f * d);
        w->mix = x;
    }
    char msg[160];
    int at = snprintf(msg, sizeof msg, "championship: %d rounds:", rounds);
    for (r = 0; r < rounds && at < (int)sizeof msg - 4; ++r) at += snprintf(msg + at, sizeof msg - (size_t)at, " %d", cup[r].mode);
    api->log(msg);
}

/* a wave's rules: the challenge's, or its round's mode in a championship */
static rules rules_of(int ch, int wave) {
    const challenge_def* c = &CHALLENGES[ch];
    rules r = {c->weapons, c->bombs, c->vents, 0};
    if (c->kind != CUP) return r;
    r.weapons = W_ANY;
    r.bombs = r.vents = 0;
    switch (cup[wave].mode) {
    case M_WRENCH: r.weapons = W_WRENCH; break;
    case M_ROULETTE: r.weapons = W_ROULETTE; break;
    case M_CHAOS: r.weapons = W_CHAOS; break;
    case M_BOMBS: r.bombs = CUP_BOMBS; break;
    case M_VENTS: r.vents = 1; break;
    case M_TIMED: r.time = cup[wave].w.count * 3 + 25; break;
    default: break;
    }
    return r;
}

/* the rules on: the weapons allowed (Roulette: a new one each time; Caos: one now, then its turns), the vents */
static void rules_set(rules nr) {
    const int was = s.rule.weapons;
    s.rule = nr;
    s.frenzy = nr.vents;
    if (nr.weapons == W_ANY) {
        if (s.wpn != 0) api->weapon_allow(NULL, 0);
        s.wpn = 0;
        s.wpn_ask = 0;
    } else if (nr.weapons == W_WRENCH) {
        static const int32_t wrench[1] = {WRENCH};
        api->weapon_allow(wrench, 1);  /* (no weapon allowed: the one in his hand is put back for the wrench) */
        s.wpn = WRENCH;
        s.wpn_ask = 0;
    } else if (nr.weapons == W_ROULETTE || was != W_CHAOS) {
        weapon_pick();
        s.wpn_t = CHAOS_FRAMES;
    }
}

static void wave_start(void) {
    s.mode = FIGHT;
    s.t = 0;
    s.spawned = s.killed = 0;
    s.spawn_wait = 30;
    s.round_clock = 0;
    if (s.wave > 0) {  /* (the first wave's rules: at the start, the weapon in his hand during the countdown) */
        if (CHALLENGES[s.ch].kind == CUP) rules_set(rules_of(s.ch, s.wave));
        else if (s.rule.weapons == W_ROULETTE) weapon_pick();
    }
    play(s.snd_wave);
    if (CHALLENGES[s.ch].kind == RAIL) rail_go();  /* (the countdown's end: the rails on; not the last wave's start) */
}

static void challenge_start(int ch) {
    s.ch = ch;
    s.wave = 0;
    s.clock = 0;
    s.felled = 0;
    s.mode = INTRO;
    s.t = 0;
    s.health0 = (int)api->health();
    s.bomb_wait = 120;
    s.supply_wait = CHALLENGES[ch].supplies * 60;
    enemies_clear();
    bombs_clear();
    s.wpn = 0;
    s.wpn_ask = 0;
    s.rule.weapons = W_ANY;
    if (CHALLENGES[ch].kind == CUP) cup_make(CHALLENGES[ch].waves);
    rules_set(rules_of(ch, 0));
    if (CHALLENGES[ch].kind == RAIL) rail_begin();
}

static int unlocked(int ch);

static void challenge_end(int won, const char* why) {
    enemies_clear();
    bombs_clear();
    if (CHALLENGES[s.ch].kind == RAIL) rail_stop();
    const rules none = {W_ANY, 0, 0, 0};
    rules_set(none);
    s.won = won;
    snprintf(s.why, sizeof s.why, "%s", won ? "" : !strcmp(why, "colpito") ? "Ti hanno colpito" : !strcmp(why, "tempo scaduto") ? "Tempo scaduto"
                                                 : !strcmp(why, "Ratchet e' caduto") ? "Ratchet e' caduto" : "");
    s.mode = RESULT;
    s.t = 0;
    if (won) {
        const challenge_def* c = &CHALLENGES[s.ch];
        int locked = -1;  /* (a hidden challenge this win shows) */
        for (int i = 0; i < N_CHALLENGES; ++i)
            if (CHALLENGES[i].hidden && !unlocked(i)) locked = i;
        api->bolts_add(c->reward);
        s.done |= 1u << s.ch;
        if (s.best[s.ch] == 0 || (uint32_t)s.clock < s.best[s.ch]) s.best[s.ch] = (uint32_t)s.clock;
        play(s.snd_win);
        if (locked >= 0 && unlocked(locked)) {
            char t[64];
            snprintf(t, sizeof t, "Sbloccata: %s", CHALLENGES[locked].name);
            toast(t);
        }
    } else {
        play(s.snd_fail);
    }
    char msg[96];
    snprintf(msg, sizeof msg, "challenge %s: %s (%d s)", CHALLENGES[s.ch].name, won ? "won" : why, s.clock / 60);
    api->log(msg);
}

/* ---- the weapons (Roulette, Caos) ---- */

static const char* weapon_name(int32_t item) {
    if (item == WRENCH) return "chiave inglese";
    for (int i = 0; i < N_WEAPONS; ++i)
        if (WEAPONS[i].item == item && WEAPONS[i].name) return WEAPONS[i].name;
    return "?";
}

/* the weapons owned that kill (the drawn ones): into out, their count */
static int weapons_usable(int32_t out[16]) {
    int32_t own[32];
    const int32_t n = api->weapons_owned(own, 32);
    int k = 0;
    for (int32_t i = 0; i < n && i < 32; ++i)
        for (int j = 0; j < N_WEAPONS; ++j)
            if (WEAPONS[j].item == own[i] && WEAPONS[j].name && k < 16) out[k++] = own[i];
    return k;
}

/* a weapon at random (another than the one now) the only one allowed, its ammo full, in his hand as soon as he can
   take it (weapon_update); none owned: the wrench */
static void weapon_pick(void) {
    int32_t w[16];
    const int n = weapons_usable(w);
    if (n == 0) {
        static const int32_t wrench[1] = {WRENCH};
        api->weapon_allow(wrench, 1);
        s.wpn = WRENCH;
        toast("Nessuna arma: solo la chiave");
        return;
    }
    int i = (int)(rnd() * (float)n) % n;
    if (n > 1 && w[i] == s.wpn) i = (i + 1 + (int)(rnd() * (float)(n - 1)) % (n - 1)) % n;
    s.wpn = w[i];
    api->weapon_allow(&s.wpn, 1);
    api->ammo_fill(s.wpn);
    s.wpn_ask = 1;
    char msg[64];
    snprintf(msg, sizeof msg, "Arma: %s", weapon_name(s.wpn));
    toast(msg);
    snprintf(msg, sizeof msg, "weapons: %s (item 0x%x) of %d", weapon_name(s.wpn), (unsigned)s.wpn, n);
    api->log(msg);
}

/* each frame of a challenge: the weapon asked into his hand until weapon_equip takes it (it says 0 while he swings
   the wrench, switches, rides); Caos: a new one when its time is up */
static void weapon_update(void) {
    if (s.wpn_ask && s.wpn != WRENCH && api->weapon_equip(s.wpn)) s.wpn_ask = 0;
    if (s.rule.weapons == W_CHAOS && (s.mode == FIGHT || s.mode == BREAK) && --s.wpn_t <= 0) {
        s.wpn_t = CHAOS_FRAMES;
        weapon_pick();
    }
}

/* ---- the arrivals ---- */

static int kind_of(char c) {
    for (int k = 0; k < N_KINDS; ++k)
        if (KIND[k].letter == c) return k;
    return -1;
}

static int kind_ready(int k) {
    if (api->base_free(KIND[k].cls) <= 0) return 0;
    if (KIND[k].moves == 2) {  /* a dropship: a place for it (two at once), troopers to bring */
        if (api->base_free(638) <= 0) return 0;
        if (s.ships[0] && api->base_moby(s.ships[0]) && s.ships[1] && api->base_moby(s.ships[1])) return 0;
    }
    return 1;
}

static void enemy_add(uint32_t h, int kind, uint32_t ship, const float at[3]) {
    enemy* e = &s.en[s.nen++];
    memset(e, 0, sizeof *e);
    e->h = h;
    e->kind = kind;
    e->ship = ship;
    e->direct = 1;
    e->to[0] = at[0];
    e->to[1] = at[1];
}

/* a drop zone for a dropship: free (no dropship on it), away from Ratchet, at random among them; -1 none */
static int drop_zone(const float h[3]) {
    int best = -1;
    float bs = 0.0f;
    for (int i = 0; i < s.npoints; ++i) {
        if (!s.is_drop[i]) continue;
        int used = 0;
        for (int k = 0; k < 2; ++k)
            if (s.ships[k] && api->base_moby(s.ships[k]) && s.ship_drop[k] == i) used = 1;
        const float dx = s.points[i][0] - h[0], dy = s.points[i][1] - h[1], d = dx * dx + dy * dy;
        if (used || d < 12.0f * 12.0f) continue;
        const float score = d * (0.5f + rnd());
        if (score > bs) bs = score, best = i;
    }
    return best;
}

/* the troopers a dropship carries (started at its landing point by base_option): counted, hunting */
static int troopers_adopt(const float at[3], uint32_t ship) {
    uint32_t near[8];
    const int32_t n = api->mobys_near(at, 1.0f, 638, near, 8);
    int got = 0;
    for (int32_t j = 0; j < n && j < 8 && s.nen < MAX_EN; ++j) {
        const uint32_t h = api->base_handle(near[j]);
        int known = h == 0;
        for (int i = 0; i < s.nen && !known; ++i) known = s.en[i].h == h;
        if (known) continue;
        enemy_add(h, K_JET, ship, at);
        api->base_hunt(near[j], 1);
        ++s.spawned;
        ++got;
    }
    return got;
}

/* a dropship (S: three troopers, then off; G: two, it stays and fights) landing on a free drop zone */
static void ship_arrive(int k, const float h[3]) {
    const wave_def* w = cur_wave();
    const int z = drop_zone(h);
    const int slot = !(s.ships[0] && api->base_moby(s.ships[0])) ? 0 : !(s.ships[1] && api->base_moby(s.ships[1])) ? 1 : -1;
    if (z < 0 || slot < 0) return;
    int troops = k == K_GUNSHIP ? 2 : 3;
    const int left = w->count - s.spawned - (k == K_GUNSHIP);
    if (troops > left) troops = left;
    if (troops > api->base_free(638)) troops = api->base_free(638);
    if (troops < 1 && k == K_SHIP) return;
    const float* p = s.points[z];
    const float yaw = atan2f(s.centre[1] - p[1], s.centre[0] - p[0]);  /* (it comes in from behind, over the wall) */
    const uint32_t ship = api->base_spawn(1356, p, yaw);
    if (ship == 0) return;
    if (troops > 0) troops = api->base_option(ship, RAC1_BASE_TROOPS, troops);
    s.ships[slot] = ship;
    s.ship_drop[slot] = z;
    if (k == K_GUNSHIP) {
        api->base_hunt(api->base_moby(ship), 1);
        enemy_add(ship, K_GUNSHIP, 0, p);
        ++s.spawned;
    }
    const int got = troopers_adopt(p, ship);
    char msg[96];
    snprintf(msg, sizeof msg, "%s on %s: %d troopers (%d counted)", k == K_GUNSHIP ? "the gunship" : "a dropship", s.point_name[z], troops, got);
    api->log(msg);
}

static void spawn_one(const float h[3]) {
    const wave_def* w = cur_wave();
    if (s.npoints == 0 || s.nen >= MAX_EN) return;
    int k = -1;
    if (s.spawned == 0 && w->mix[0] == 'G') {  /* the gunship opens the wave */
        if (kind_ready(K_GUNSHIP)) ship_arrive(K_GUNSHIP, h);
        return;
    }
    const int len = (int)strlen(w->mix), first = (int)(rnd() * (float)len) % len;
    for (int j = 0; j < len && k < 0; ++j) {  /* the one drawn, else the next one in the mix with room */
        const int kk = kind_of(w->mix[(first + j) % len]);
        if (kk >= 0 && kk != K_GUNSHIP && kind_ready(kk)) k = kk;
    }
    if (k < 0) return;
    if (KIND[k].moves == 2) {
        ship_arrive(k, h);
        return;
    }
    const float* p = s.points[s.next_point++ % s.npoints];
    if (KIND[k].moves == 1) {  /* a trooper on a drop zone (open floor): at a gate's alcove, under its lintel, it would stay
                                  stuck (they fly straight at Ratchet, and stop at a wall they cannot get past nor over) */
        int drops[N_POINTS], nd = 0;
        for (int i = 0; i < s.npoints; ++i)
            if (s.is_drop[i]) drops[nd++] = i;
        if (nd > 0) p = s.points[drops[(int)(rnd() * (float)nd) % nd]];
    }
    /* (a Gadgetron robot walks at the height it was put: on the floor; the others find the floor themselves) */
    const float at[3] = {p[0] + 3.0f * (rnd() - 0.5f), p[1] + 3.0f * (rnd() - 0.5f), p[2] + (KIND[k].cls == 541 ? 0.0f : 0.5f)};
    const float yaw = atan2f(s.centre[1] - at[1], s.centre[0] - at[0]);
    const uint32_t e = api->base_spawn(KIND[k].cls, at, yaw);
    if (e == 0) return;
    if (KIND[k].weapon >= 0) api->base_option(e, RAC1_BASE_WEAPON, KIND[k].weapon);
    enemy_add(e, k, 0, at);
    ++s.spawned;
    const uint32_t m = api->base_moby(e);
    if (m != 0) api->base_hunt(m, 1);  /* (it hunts Ratchet wherever he is, instead of guarding where it came in) */
}

/* Kalebo III's robots (class 1445, update 0x2e5e08): their target search (target_select, 0x25ff38) writes the
   target's place into pvars +0x180 (the target itself +0x1c0) and they walk (state 4) and swing (state 5) straight at
   it; base_hunt (at their spawn) keeps them on Ratchet wherever he is, but straight: a wall stops them. The other
   walkers (the bomb bots, the Gadgetron robots) are the same. So while one cannot see him (nav_steer: a wall between
   them, or another height - up on a tower, where the robots' own code gives up: states 1 and 6), its step is taken
   over: from where it stood, at its own pace, toward the point NAV_AHEAD cells along the navigation field's path,
   facing that way (a robot kept walking: state 4). The robots' and the bomb bots' own ground code keeps them on the
   floor and up the ramps; a Gadgetron robot walks at the height it was put: the plugin's (gadget_ground). */
#define ROBOT_WALK 0.07f  /* units a frame (measured: their chase across the floor) */
#define GRAVITY 0.025f    /* units a frame each frame, falling */
#define ROBOT_IDLE 1      /* their states (the update's jump table): waiting */
#define ROBOT_CHASE 4     /* walking at the target (5 within 1.25 units: swinging) */
#define ROBOT_GIVEUP 6    /* walking back to their guard point (+0x1d0), the target lost */

/* the level's floor under p (the robot itself and the mobys passed through), NAN none */
static float floor_under(uint32_t self, const float p[3]) {
    float a[3] = {p[0], p[1], p[2] + 0.6f};
    const float b[3] = {p[0], p[1], p[2] - 60.0f};
    uint32_t skip = self;
    for (int k = 0; k < 3; ++k) {
        rac1_ray_hit hit = {sizeof hit};
        if (!api->collide_ray(a, b, skip, 0, &hit)) return NAN;
        if (hit.moby == 0) return hit.point[2];
        skip = hit.moby;
        a[2] = hit.point[2] - 0.05f;
    }
    return NAN;
}

/* falling: faster each frame, down to the floor `g`; into the lava they burn (gone: counted as felled) */
static void fall_to(enemy* e, uint32_t m, float r[3], float g) {
    e->vz = fminf(e->vz + GRAVITY, 1.0f);
    r[2] -= e->vz;
    if (!isnan(g) && r[2] < g) r[2] = g;
    api->swf(m + 0x18, r[2]);
    api->moby_moved(m);
    if (s.lava_ok && r[2] < s.lava_z + 0.3f) api->moby_remove(m);  /* (enemies_alive counts it) */
}

/* the robots' own code climbs the floor but never drops them: off a ramp's side, a wall or the hub they would walk on
   in the air. Standing (waiting, walking, swinging, giving up: not while a hit throws them) over nothing within 0.05,
   they fall */
static void robot_fall(enemy* e, uint32_t m, uint32_t st, float r[3]) {
    if (st != ROBOT_IDLE && st != ROBOT_CHASE && st != 5 && st != ROBOT_GIVEUP) {
        e->vz = 0.0f;
        return;
    }
    const float g = floor_under(m, r);
    if (!isnan(g) && r[2] <= g + 0.05f) {
        e->vz = 0.0f;
        return;
    }
    fall_to(e, m, r, g);
}

/* a Gadgetron robot keeps the height it was put at (its patrol's points at its own height): on the floor under it -
   up a ramp's slope (within 0.6), down by falling */
static void gadget_ground(enemy* e, uint32_t m, float r[3]) {
    const float g = floor_under(m, r);
    if (isnan(g)) return;
    if (r[2] <= g + 0.05f) {
        e->vz = 0.0f;
        if (g - r[2] > 0.002f) {
            r[2] = g;
            api->swf(m + 0x18, r[2]);
            api->moby_moved(m);
        }
        return;
    }
    fall_to(e, m, r, g);
}

static void enemies_follow(const float h[3]) {
    for (int i = 0; i < s.nen; ++i) {
        enemy* e = &s.en[i];
        const struct kind_def* k = &KIND[e->kind];
        const uint32_t m = api->base_moby(e->h);
        if (m == 0 || k->moves != 0 || rac1_lw(api, m + 0x78) == 0) continue;  /* (the flyers: their own flight) */
        float r[3];
        pos_of(m, r);
        const uint32_t st = rac1_lb(api, m + 0x20);
        if ((s.clock + i) % 6 == 0) nav_steer(m, r, h, e->to, &e->direct);
        const int robot = e->kind == K_ROBOT, gadget = k->cls == 541;
        const int may = robot ? st == ROBOT_CHASE || st == ROBOT_IDLE || st == ROBOT_GIVEUP : gadget ? st != GADGET_HIT : 1;
        if (!e->direct && e->has_prev && may) {
            if (robot && st != ROBOT_CHASE) api->sb(m + 0x20, ROBOT_CHASE);
            const float dx = e->to[0] - e->prev[0], dy = e->to[1] - e->prev[1];
            const float d = sqrtf(dx * dx + dy * dy);
            if (d > 0.05f) {
                r[0] = e->prev[0] + dx / d * k->step;
                r[1] = e->prev[1] + dy / d * k->step;
                api->swf(m + 0x10, r[0]);
                api->swf(m + 0x14, r[1]);
                api->swf(m + 0x48, atan2f(dy, dx));
                api->moby_moved(m);
            }
        }
        if (robot) robot_fall(e, m, st, r);
        else if (gadget && st != GADGET_HIT) gadget_ground(e, m, r);
        memcpy(e->prev, r, sizeof r);
        e->has_prev = 1;
    }
}

/* a hit as from an explosion on Kalebo III's enemies within `reach` of p: how many */
static int hit_enemies(const float p[3], float reach, float strength, uint32_t from) {
    static const int32_t CLS[] = {1445, 52, 541, 638};
    int n = 0;
    for (int c = 0; c < 4; ++c) {
        uint32_t near[16];
        const int32_t k = api->mobys_near(p, reach, CLS[c], near, 16);
        for (int32_t j = 0; j < k && j < 16; ++j, ++n) api->hit_send(near[j], RAC1_HIT_ENEMY, strength, from);
    }
    return n;
}

/* ---- the bombardment ----
 * A red ring (the level's class "mark") marks where a bomb will land: near Ratchet, never right under him. It pulses
 * on the ground with a red light for BOMB_WARN frames; then the bomb (class "bomb") shows BOMB_HIGH units over it with
 * a whistle and falls, about another second. Landing: smoke, a boom, a flash of light; Ratchet within BOMB_REACH (the
 * ring's outer edge, as drawn) loses a health point, and the enemies within it take a hit as from an explosion. Two
 * seconds to step out of a ring.
 */
#define MAX_BOMBS 6
#define BOMB_WARN 60      /* frames the ring shows alone */
#define BOMB_HIGH 22.0f
#define MARK_SCALE 1.1f   /* the ring's: its outer edge 2.3 at 1 */
#define BOMB_REACH 2.5f   /* (that edge) */

static struct {
    uint32_t bomb, mark;  /* their objects (0: none) */
    float at[3];          /* where it lands */
    float z, vz;
    int wait;             /* frames before the bomb shows */
    int flash;            /* frames of its blast's light left */
} bombs[MAX_BOMBS];
static int32_t cls_bomb = -1, cls_mark = -1, snd_whistle = -1, snd_boom = -1;
static int bombs_landed, bombs_hurt, bombs_hit;  /* (the tests' counts) */

static void bombs_clear(void) {
    for (int i = 0; i < MAX_BOMBS; ++i) {
        if (bombs[i].bomb) api->object_remove(bombs[i].bomb);
        if (bombs[i].mark) api->object_remove(bombs[i].mark);
        bombs[i].bomb = bombs[i].mark = 0;
        bombs[i].flash = 0;
    }
}

static void bomb_drop(const float h[3]) {
    if (cls_bomb < 0 || cls_mark < 0) return;
    int i = 0;
    while (i < MAX_BOMBS && (bombs[i].mark || bombs[i].flash)) ++i;
    if (i == MAX_BOMBS) return;
    const float ang = rnd() * 6.2832f, d = 1.5f + rnd() * 5.0f;
    float p[3] = {h[0] + cosf(ang) * d, h[1] + sinf(ang) * d, h[2] + 4.0f};
    const float g = floor_under(api->hero(), p);
    if (isnan(g) || (s.lava_ok && g < s.lava_z + 0.3f)) return;  /* (over the lava: none) */
    p[2] = g;
    memcpy(bombs[i].at, p, sizeof p);
    bombs[i].mark = api->object_spawn(cls_mark, p, ang);
    bombs[i].bomb = 0;
    bombs[i].wait = BOMB_WARN;
    bombs[i].flash = 0;
}

static void bomb_blast(int i) {
    const float* p = bombs[i].at;
    for (int k = 0; k < 5; ++k) {
        const float at[3] = {p[0] + 2.0f * (rnd() - 0.5f), p[1] + 2.0f * (rnd() - 0.5f), p[2] + 0.3f};
        const float up[3] = {2.0f * (rnd() - 0.5f), 2.0f * (rnd() - 0.5f), 2.0f + 2.0f * rnd()};
        api->smoke(at, up, 2.5f);
    }
    if (snd_boom >= 0) api->sfx_play(snd_boom, 0, p);
    float h[3];
    pos_of(api->hero(), h);
    const float dx = h[0] - p[0], dy = h[1] - p[1];
    /* (the bomb's moby, on the ground now: the hit throws him away from where it landed) */
    if (dx * dx + dy * dy < BOMB_REACH * BOMB_REACH && fabsf(h[2] - p[2]) < 3.0f && api->hero_damage(1, api->object_moby(bombs[i].bomb)))
        ++bombs_hurt;
    bombs_hit += hit_enemies(p, BOMB_REACH, 40.0f, api->object_moby(bombs[i].bomb));
    api->object_remove(bombs[i].bomb);
    api->object_remove(bombs[i].mark);
    bombs[i].bomb = bombs[i].mark = 0;
    bombs[i].flash = 14;
    ++bombs_landed;
}

/* each frame (from frame: the lights) */
static void bombs_update(void) {
    static const float fire[3] = {1.0f, 0.55f, 0.2f}, red[3] = {1.0f, 0.12f, 0.06f};
    for (int i = 0; i < MAX_BOMBS; ++i) {
        const float* p = bombs[i].at;
        if (bombs[i].flash > 0) {
            const float at[3] = {p[0], p[1], p[2] + 1.5f};
            api->light_add(at, fire, 5.0f * (float)bombs[i].flash / 14.0f, 18.0f);
            --bombs[i].flash;
            continue;
        }
        if (!bombs[i].mark) continue;
        const float glow[3] = {p[0], p[1], p[2] + 0.6f};
        if (bombs[i].wait > 0) {  /* the ring alone, pulsing slowly */
            const float pulse = 0.5f + 0.5f * sinf((float)bombs[i].wait * 0.25f);
            const float spin[3] = {0.0f, 0.0f, (float)bombs[i].wait * 0.03f};
            api->object_place(bombs[i].mark, NULL, spin, MARK_SCALE);
            api->light_add(glow, red, 0.6f + 0.6f * pulse, 5.0f);
            if (--bombs[i].wait == 0) {  /* the bomb high over it, falling */
                const float top[3] = {p[0], p[1], p[2] + BOMB_HIGH};
                bombs[i].bomb = api->object_spawn(cls_bomb, top, 0.0f);
                bombs[i].z = top[2];
                bombs[i].vz = 0.08f;
                if (snd_whistle >= 0) api->sfx_play(snd_whistle, 0, p);
            }
            continue;
        }
        bombs[i].vz += 0.006f;
        bombs[i].z -= bombs[i].vz;
        const float left = (bombs[i].z - p[2]) / BOMB_HIGH;  /* 1 high up .. 0 landing */
        const float pulse = 0.5f + 0.5f * sinf((1.0f - left) * 30.0f);
        const float spin[3] = {0.0f, 0.0f, (1.0f - left) * 6.0f};
        api->object_place(bombs[i].mark, NULL, spin, MARK_SCALE * (1.0f + 0.05f * pulse));
        const float at[3] = {p[0], p[1], fmaxf(bombs[i].z, p[2])};
        if (bombs[i].bomb) api->object_place(bombs[i].bomb, at, NULL, 0.0f);
        api->light_add(glow, red, 0.8f + 1.4f * pulse * (1.0f - left), 5.0f);
        if (bombs[i].z <= p[2]) bomb_blast(i);
    }
}

/* ---- the supplies ---- */
#define CRATE_NANO 501  /* Kalebo III's nanotech crate (level.txt's spawn 501 4) */
#define CRATE_AMMO 511  /* its ammo crate (spawn 511 4) */
#define N_SLOTS 4

static const float SLOT[N_SLOTS][2] = {{-3.5f, -3.5f}, {3.5f, -3.5f}, {-3.5f, 3.5f}, {3.5f, 3.5f}};  /* from the hub's point */
static uint32_t slot_crate[N_SLOTS];  /* its crate (base_spawn's handle), 0 none */
static int slot_flash[N_SLOTS];       /* frames of its arrival's light left */
static int supplies_given;            /* (the tests' count) */

static int slot_free(int i) {
    return slot_crate[i] == 0 || api->base_moby(slot_crate[i]) == 0;
}

/* `count` crates of `cls` into the free places, from a random one on: a puff of smoke, a light */
static int supplies_drop(int32_t cls, int count) {
    int n = 0;
    const int first = (int)(rnd() * N_SLOTS) % N_SLOTS;
    for (int k = 0; k < N_SLOTS && n < count; ++k) {
        const int i = (first + k) % N_SLOTS;
        if (!slot_free(i)) continue;
        const float at[3] = {s.centre[0] + SLOT[i][0], s.centre[1] + SLOT[i][1], s.centre[2] + 0.5f};
        const uint32_t h = api->base_spawn(cls, at, 0.0f);
        if (h == 0) continue;
        slot_crate[i] = h;
        slot_flash[i] = 30;
        const float up[3] = {0.0f, 0.0f, 1.5f};
        api->smoke(at, up, 1.5f);
        ++n;
    }
    if (n > 0) {
        supplies_given += n;
        char msg[80];
        snprintf(msg, sizeof msg, "supplies: %d %s crate%s on the hub", n, cls == CRATE_NANO ? "nanotech" : "ammo", n > 1 ? "s" : "");
        api->log(msg);
    }
    return n;
}

/* each frame (from frame: the lights) */
static void supplies_update(void) {
    static const float nano[3] = {0.3f, 1.0f, 0.5f};
    for (int i = 0; i < N_SLOTS; ++i) {
        if (slot_flash[i] <= 0) continue;
        const float at[3] = {s.centre[0] + SLOT[i][0], s.centre[1] + SLOT[i][1], s.centre[2] + 1.5f};
        api->light_add(at, nano, 2.0f * (float)slot_flash[i] / 30.0f, 6.0f);
        --slot_flash[i];
    }
    if (s.mode != FIGHT && s.mode != BREAK) return;
    const challenge_def* c = &CHALLENGES[s.ch];
    if (c->supplies <= 0 || --s.supply_wait > 0) return;
    s.supply_wait = (int)((float)(c->supplies * 60) * (0.75f + 0.5f * rnd()));
    supplies_drop(rnd() < 0.5f || s.rule.weapons == W_WRENCH ? CRATE_NANO : CRATE_AMMO, 2);  /* (the wrench only: no ammo) */
}

/* ---- the grind rails (RAIL; rails.txt: export_arena.py's, from build_rails.py's curves) ----
   The game's rail table (at Veldin's overlay address 0x15f70c, its count at 0x15f710; 0x20 bytes a rail: its centre,
   its reach +0xc, its path +0x10, a loop +0x14; a path: its count, 12 bytes, then 16-byte points, w the length to the
   next one) is empty on a level mod: a rail challenge points it at the plugin's own (ee_alloc) and back when it ends,
   so its rails are there only then (their models too). Kalebo III's hero code takes a rail only within 12 units of
   its own level's two rail spots (hero_rail_land_check 0x20cf58): the rails start by one, at the station. Once on one,
   its jumps and the side jumps to a rail beside it (2..4.5 away: the lanes) work anywhere. The challenges need the Grind
   Scarponi (0x13d4dd, the grind boots): without them the menu does not start them.
   The course ("Grind Scarponi: Discesa") has obstacles on its lanes (the level's objects of the classes mine, barrier,
   drone): a mine is jumped, a barrier is passed on another lane, a drone is broken with the wrench (Square on the rail)
   or zaps him as he passes; each hurts him by one. Its end leads to a last wave in the arena. */
#define RAIL_TABLE 0x0015f70cu
#define RAIL_COUNT 0x0015f710u
#define GRIND_BOOTS 0x0013d4ddu
#define MAX_RAILS 6
#define RAIL_PTS 200
#define RAIL_END 2.5f  /* the rail ridden to its end: this near its last point */
#define RAIL_ATTACK 0x2b  /* his wrench swing on a rail */
#define MAX_OBST 48
#define DRONE_BOLTS 50

enum { O_MINE, O_BARRIER, O_DRONE };

static struct {
    int n;
    struct {
        char name[24];
        int npts;
        float pt[RAIL_PTS][3];
        uint32_t path;  /* its path in the game's memory (made at the first challenge), 0 not yet */
    } r[MAX_RAILS];
    int loaded;
    uint32_t table;                /* (ee_alloc, once) the table's entries, then the paths */
    uint32_t old_table, old_count; /* the level's own, put back */
    int on;                        /* rails in the game's table now */
    int set[MAX_RAILS], nset;      /* the challenge's rails (set[0]: the one whose end finishes it), its start's name */
    const char* start;
    int phase;                     /* 0 the countdown on the station, 1 riding, 2 the last wave in the arena */
    int ride;                      /* frames riding (the time limit) */
    int falls;
    int reached;  /* the finishing rail's end reached: done once he is off it */
    struct {
        uint32_t obj;
        int kind, alive;
        float p[3];
    } obs[MAX_OBST];
    int nobs, drones, drones_hit, use_obs;
    int warned;  /* the lanes whose end was told (bits by their place in set) */
} rails;

/* his states on a rail: 0x28 grinding, 0x29 / 0x2a jumping, 0x2b attacking, 0x42 hopping off (hero_transitions) */
static int on_rail(void) {
    const uint32_t st = rac1_lw(api, HERO_STATE);
    return (st >= 0x28 && st <= 0x2b) || st == 0x42;
}

/* rails.txt: "rail <name> <points>", then a point a line (the game's coordinates) */
static void rails_load(void) {
    rails.loaded = 1;
    char path[512], line[128];
    if (!api->mod_path("level/rails.txt", path, sizeof path)) return;
    FILE* f = fopen(path, "r");
    if (f == NULL) return;
    int at = -1, want = 0;
    while (fgets(line, sizeof line, f)) {
        char name[24];
        int n;
        float x, y, z;
        if (sscanf(line, "rail %23s %d", name, &n) == 2) {
            at = rails.n < MAX_RAILS ? rails.n++ : -1;
            if (at >= 0) snprintf(rails.r[at].name, sizeof rails.r[at].name, "%s", name), rails.r[at].npts = 0;
            want = n;
        } else if (at >= 0 && sscanf(line, "%f %f %f", &x, &y, &z) == 3 && rails.r[at].npts < RAIL_PTS && rails.r[at].npts < want) {
            float* p = rails.r[at].pt[rails.r[at].npts++];
            p[0] = x, p[1] = y, p[2] = z;
        }
    }
    fclose(f);
    char msg[160];
    int len = snprintf(msg, sizeof msg, "rails: %d", rails.n);
    for (int i = 0; i < rails.n && len < (int)sizeof msg - 32; ++i)
        len += snprintf(msg + len, sizeof msg - (size_t)len, "%s %s (%d points)", i ? "," : ":", rails.r[i].name, rails.r[i].npts);
    api->log(msg);
}

static int rail_find(const char* name) {
    for (int i = 0; i < rails.n; ++i)
        if (strcmp(rails.r[i].name, name) == 0) return i;
    return -1;
}

/* its model (the class rail_<name>'s object) shown or not */
static void rail_show(int i, int on) {
    char n[32];
    snprintf(n, sizeof n, "rail_%s", rails.r[i].name);
    const uint32_t o = api->object_named(n);
    if (o != 0) api->object_show(o, on);
}

static float dist3(const float a[3], const float b[3]) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}

/* every rail's path into the game's memory, once (they stay: the table points at them while a challenge runs) */
static int rails_make(void) {
    if (rails.table != 0) return 1;
    uint32_t size = MAX_RAILS * 0x20u;
    for (int i = 0; i < rails.n; ++i) size += 0x10u + 16u * (uint32_t)rails.r[i].npts;
    rails.table = api->ee_alloc(size);
    if (rails.table == 0) return 0;
    uint32_t at = rails.table + MAX_RAILS * 0x20u;
    for (int i = 0; i < rails.n; ++i) {
        const int n = rails.r[i].npts;
        rails.r[i].path = at;
        api->sw(at, (uint32_t)n);
        for (int k = 0; k < n; ++k) {
            const float* p = rails.r[i].pt[k];
            const uint32_t q = at + 0x10 + 16u * (uint32_t)k;
            api->swf(q, p[0]);
            api->swf(q + 4, p[1]);
            api->swf(q + 8, p[2]);
            api->swf(q + 12, k + 1 < n ? dist3(p, rails.r[i].pt[k + 1]) : 0.0f);
        }
        at += 0x10u + 16u * (uint32_t)n;
    }
    char msg[96];
    snprintf(msg, sizeof msg, "rails: their paths in the game's memory at 0x%x (%u bytes)", (unsigned)rails.table, (unsigned)size);
    api->log(msg);
    return 1;
}

/* the challenge's rails into the game's table */
static int rails_install(void) {
    if (rails.nset == 0 || !rails_make()) return 0;
    for (int j = 0; j < rails.nset; ++j) {
        const int i = rails.set[j], n = rails.r[i].npts;
        float lo[3], hi[3], c[3], reach = 0.0f;
        for (int a = 0; a < 3; ++a) lo[a] = hi[a] = rails.r[i].pt[0][a];
        for (int k = 0; k < n; ++k)
            for (int a = 0; a < 3; ++a) {
                if (rails.r[i].pt[k][a] < lo[a]) lo[a] = rails.r[i].pt[k][a];
                if (rails.r[i].pt[k][a] > hi[a]) hi[a] = rails.r[i].pt[k][a];
            }
        for (int a = 0; a < 3; ++a) c[a] = (lo[a] + hi[a]) * 0.5f;
        for (int k = 0; k < n; ++k) {
            const float d = dist3(c, rails.r[i].pt[k]);
            if (d > reach) reach = d;
        }
        const uint32_t e = rails.table + 0x20u * (uint32_t)j;
        api->swf(e, c[0]);
        api->swf(e + 4, c[1]);
        api->swf(e + 8, c[2]);
        api->swf(e + 12, reach + 2.0f);
        api->sw(e + 0x10, rails.r[i].path);
        api->sw(e + 0x14, 0);
    }
    if (!rails.on) {
        rails.old_table = rac1_lw(api, api->ov(RAIL_TABLE));
        rails.old_count = rac1_lw(api, api->ov(RAIL_COUNT));
    }
    api->sw(api->ov(RAIL_TABLE), rails.table);
    api->sw(api->ov(RAIL_COUNT), (uint32_t)rails.nset);
    rails.on = rails.nset;
    char msg[96];
    snprintf(msg, sizeof msg, "rails on: %d (the first: %s)", rails.nset, rails.r[rails.set[0]].name);
    api->log(msg);
    return 1;
}

static void rails_uninstall(void) {
    if (!rails.on) return;
    if (rac1_lw(api, api->ov(RAIL_TABLE)) == rails.table) {  /* (still ours: not the level loaded again) */
        api->sw(api->ov(RAIL_COUNT), rails.old_count);
        api->sw(api->ov(RAIL_TABLE), rails.old_table);
    }
    rails.on = 0;
}

/* Ratchet has the Grind Scarponi (the rail challenges need them) */
static int has_boots(void) {
    return rac1_lb(api, GRIND_BOOTS) != 0;
}

/* Ratchet put there by the plugin (not taken for a death) */
static void hero_move(const float p[3], float yaw) {
    api->hero_place(p, yaw);
    s.placed = 3;
}

/* the course's obstacles (the level's objects of the classes mine, barrier, drone), found at the level's start */
static void obstacles_find(void) {
    const int32_t cls[3] = {api->class_find("mine"), api->class_find("barrier"), api->class_find("drone")};
    rails.nobs = rails.drones = 0;
    for (int32_t i = 0; i < api->object_count() && rails.nobs < MAX_OBST; ++i) {
        const uint32_t o = api->object_at(i);
        const int32_t c = api->object_class(o);
        for (int k = 0; k < 3; ++k) {
            if (c < 0 || c != cls[k]) continue;
            rails.obs[rails.nobs].obj = o;
            rails.obs[rails.nobs].kind = k;
            rails.obs[rails.nobs].alive = 0;
            pos_of(api->object_moby(o), rails.obs[rails.nobs].p);
            api->object_show(o, 0);
            rails.drones += k == O_DRONE;
            ++rails.nobs;
        }
    }
}

/* all of them back (a new try) or all gone */
static void obstacles_reset(int on) {
    for (int i = 0; i < rails.nobs; ++i) {
        rails.obs[i].alive = on;
        api->object_show(rails.obs[i].obj, on);
    }
    rails.drones_hit = 0;
}

/* one blown up: smoke, a flash, the boom */
static void obstacle_blast(int i) {
    float* p = rails.obs[i].p;
    const float up[3] = {0.0f, 0.0f, 0.04f}, fire[3] = {1.0f, 0.55f, 0.2f};
    for (int k = 0; k < 4; ++k) api->smoke(p, up, 1.2f + 0.4f * (float)k);
    api->light_add(p, fire, 3.0f, 8.0f);
    if (snd_boom >= 0) api->sfx_play(snd_boom, 0, p);
    rails.obs[i].alive = 0;
    api->object_show(rails.obs[i].obj, 0);
}

/* riding past them: a mine under his feet, a barrier in his way, a drone at his height hurt him (one each); his wrench
   swing on the rail (Square) breaks a drone within 2.4 of his middle */
static void obstacles_update(const float h[3]) {
    const int swing = rac1_lw(api, HERO_STATE) == RAIL_ATTACK;
    const float mid[3] = {h[0], h[1], h[2] + 0.9f};
    for (int i = 0; i < rails.nobs; ++i) {
        if (!rails.obs[i].alive) continue;
        const float* p = rails.obs[i].p;
        const float dxy = sqrtf((h[0] - p[0]) * (h[0] - p[0]) + (h[1] - p[1]) * (h[1] - p[1])), dz = h[2] - p[2];
        int hurt = 0;
        if (rails.obs[i].kind == O_MINE) {
            hurt = dxy < 0.8f && dz > -0.6f && dz < 1.1f;
        } else if (rails.obs[i].kind == O_BARRIER) {
            hurt = dxy < 0.9f && dz > -0.6f && dz < 2.3f;
        } else if (swing && dist3(mid, p) < 2.4f) {
            obstacle_blast(i);
            ++rails.drones_hit;
            api->bolts_add(DRONE_BOLTS);
            continue;
        } else {
            hurt = dxy < 1.0f && fabsf(mid[2] - p[2]) < 1.3f;
        }
        if (hurt) {
            obstacle_blast(i);
            api->hero_damage(1, 0);
        }
    }
}

/* on the station, facing the first rail's start; the obstacles back */
static void rail_to_start(void) {
    const uint32_t o = rails.start ? api->object_named(rails.start) : 0;
    if (o == 0 || rails.nset == 0) return;
    float p[3];
    pos_of(api->object_moby(o), p);
    const float* a = rails.r[rails.set[0]].pt[0];
    hero_move(p, atan2f(a[1] - p[1], a[0] - p[0]));
    rails.reached = 0;
    rails.warned = 0;
    if (rails.use_obs) obstacles_reset(1);
}

/* the challenge's start: its rails shown, Ratchet on the station, held still through the countdown (the rails are not
   on yet: a jump onto one would only land on its strip) */
static void rail_begin(void) {
    const challenge_def* c = &CHALLENGES[s.ch];
    rails.nset = 0;
    rails.phase = 0;
    rails.ride = 0;
    rails.falls = 0;
    rails.start = c->start;
    rails.use_obs = c->obstacles;
    char names[64];
    snprintf(names, sizeof names, "%s", c->rails ? c->rails : "");
    for (char* t = strtok(names, " "); t != NULL && rails.nset < MAX_RAILS; t = strtok(NULL, " ")) {
        const int i = rail_find(t);
        if (i >= 0) rails.set[rails.nset++] = i;
    }
    if (rails.nset == 0) {
        toast("Nessuna rotaia (rails.txt)");
        return;
    }
    for (int j = 0; j < rails.nset; ++j) rail_show(rails.set[j], 1);
    rail_to_start();
    api->input_hold(1);
}

/* the countdown over: the rails in the game's table, Ratchet free */
static void rail_go(void) {
    if (rails.phase != 0) return;
    api->input_hold(0);
    if (!rails_install()) {
        challenge_end(0, "nessuna rotaia");
        return;
    }
    rails.phase = 1;
    toast("Salta sulla rotaia!");
}

/* the rails and their models gone (and the obstacles) */
static void rails_away(void) {
    rails_uninstall();
    for (int j = 0; j < rails.nset; ++j) rail_show(rails.set[j], 0);
    if (rails.use_obs) obstacles_reset(0);
}

/* the end: all gone, Ratchet back in the arena if he is not */
static void rail_stop(void) {
    api->input_hold(0);
    rails_away();
    const uint32_t hero = api->hero();
    if (hero == 0) return;
    float h[3];
    pos_of(hero, h);
    const float lx = h[0] - s.centre[0], ly = h[1] - s.centre[1];
    if (lx < -48.0f || lx > 48.0f || ly > 48.0f || h[2] < nav.floor_z - 1.2f || h[2] > nav.floor_z + 15.0f) {
        const float at[3] = {s.centre[0], s.centre[1] - 12.0f, nav.floor_z + 0.5f};
        hero_move(at, 1.5708f);
    }
}

/* riding: the finishing rail's end reached wins, or leads to the last wave (once he is off it); a fall (into the lava,
   or outside the walls) puts him back on the station, the obstacles back too */
static void rail_update(const float h[3]) {
    const challenge_def* c = &CHALLENGES[s.ch];
    if (rails.nset == 0) return;
    ++rails.ride;
    if (rails.use_obs) obstacles_update(h);
    const int fin = rails.set[0];
    if (dist3(h, rails.r[fin].pt[rails.r[fin].npts - 1]) < RAIL_END) rails.reached = 1;
    if (s.t % 10 == 0)  /* on a lane that ends (not the finishing rail) within 20 units of its end: told once */
        for (int j = 1; j < rails.nset; ++j) {
            const int i = rails.set[j], n = rails.r[i].npts;
            if ((rails.warned >> j) & 1) continue;
            for (int k = n > 14 ? n - 14 : 0; k < n; ++k)
                if (dist3(h, rails.r[i].pt[k]) < 1.2f) {
                    rails.warned |= 1 << j;
                    toast("La corsia finisce! Cambia corsia");
                    break;
                }
        }
    if (rails.reached && !on_rail()) {
        if (c->wave[0].count > 0) {  /* (down in the arena: the last wave) */
            rails_away();
            rails.phase = 2;
            toast("Atterrato! Ora l'ultima ondata");
            wave_start();
        } else {
            challenge_end(1, NULL);
        }
        return;
    }
    const float lx = h[0] - s.centre[0], ly = h[1] - s.centre[1];
    const int outside = lx < -64.0f || lx > 64.0f || ly > 64.0f;
    if (s.placed == 0 && (h[2] < nav.floor_z - 1.2f || (outside && h[2] < nav.floor_z + 5.0f))) {
        ++rails.falls;
        rail_to_start();
        toast("Caduto! Di nuovo dalla stazione");
    }
    if (rails.ride >= c->time * 60) challenge_end(0, "tempo scaduto");
}

static void fight_update(const float h[3]) {
    const challenge_def* c = &CHALLENGES[s.ch];
    if (c->kind == RAIL && rails.phase < 2) {
        rail_update(h);
        return;
    }
    nav_track(h);
    enemies_follow(h);
    if (s.rule.bombs > 0 && --s.bomb_wait <= 0) {
        bomb_drop(h);
        s.bomb_wait = s.rule.bombs / 2 + (int)(rnd() * (float)s.rule.bombs);
    }
    const int hp = (int)api->health();
    if (hp > s.health0) s.health0 = hp;  /* (a nanotech crate) */
    if (c->nohit && hp < s.health0) {
        challenge_end(0, "colpito");
        return;
    }
    const wave_def* w = cur_wave();
    const int alive = enemies_alive();
    if (s.spawned < w->count && alive < w->at_once && --s.spawn_wait <= 0) {
        spawn_one(h);
        s.spawn_wait = w->every;
    }
    if (c->kind == SURVIVE) {
        if (s.clock >= c->time * 60) challenge_end(1, NULL);
        return;
    }
    if ((c->kind == TIMED && s.clock >= c->time * 60) || (s.rule.time > 0 && ++s.round_clock >= s.rule.time * 60)) {
        challenge_end(0, "tempo scaduto");
        return;
    }
    if (s.spawned >= w->count && enemies_alive() == 0) {
        if (++s.wave >= c->waves) {
            challenge_end(1, NULL);
            return;
        }
        s.mode = BREAK;
        s.t = 0;
    }
}

/* ---- the frame ---- */

static void flames_light(void);

/* a hidden challenge (Impossibile): shown once every other one is won */
static int unlocked(int ch) {
    if (!CHALLENGES[ch].hidden) return 1;
    for (int k = 0; k < N_CHALLENGES; ++k)
        if (k != ch && CHALLENGES[k].kind != RAIL && !((s.done >> k) & 1)) return 0;
    return 1;
}

/* the challenges the menu lists, into out: their count */
static int menu_list(int out[N_CHALLENGES]) {
    int n = 0;
    for (int i = 0; i < N_CHALLENGES; ++i)
        if (unlocked(i)) out[n++] = i;
    return n;
}

#define MENU_ROWS 10  /* the list's rows shown (it scrolls): with the title, the chosen one's two lines and the keys'
                         two, the 15 lines the game draws a frame */

/* the menu: a panel of a fixed width in the middle of the screen; the title; the list (a green mark: won; the chosen
   one on a band; a scroll bar when it is longer than MENU_ROWS); under a rule, the chosen one's line (or the toast)
   and its prize and best time; the keys */
static void menu_lines(void) {
    int list[N_CHALLENGES];
    const int n = menu_list(list);
    if (s.sel >= n) s.sel = n - 1;
    if (s.sel < s.scroll) s.scroll = s.sel;
    if (s.sel >= s.scroll + MENU_ROWS) s.scroll = s.sel - MENU_ROWS + 1;
    const int rows = n < MENU_ROWS ? n : MENU_ROWS;
    const int x = 96, w = 320, top = 36, step = 20, list_y = top + 48, info_y = list_y + step * rows + 12, h = info_y - top + 90;
    const int C = RAC1_HUD_CENTRE;
    hud_panel(x, top, w, h, PANEL, C);
    hud_panel(x, top, w, 3, BAR, C);
    hud_panel(x + 16, top + 38, w - 32, 1, RULE, C);
    hud_panel(x + 16, info_y - 7, w - 32, 1, RULE, C);
    hud_text_at(256, top + 11, YELLOW, 1.5f, C, 1, "ARENA - SFIDE");
    hud_maxw(w - 32);
    for (int r = 0; r < rows; ++r) {
        const int i = list[s.scroll + r], y = list_y + step * r, chosen = s.scroll + r == s.sel;
        if (chosen) {
            hud_panel(x + 8, y - 3, w - 30, step - 2, PICK, C);
            hud_panel(x + 8, y - 3, 3, step - 2, BAR, C);
        }
        hud_panel(x + 20, y + 4, 7, 7, (s.done >> i) & 1 ? GREEN : BOX_EMPTY, C);
        hud_text_at(x + 36, y, CHALLENGES[i].kind == RAIL && !has_boots() ? GREY : chosen ? WHITE : CYAN, 1.1f, C, 0, "%s", CHALLENGES[i].name);
        hud_maxw(w - 60);
    }
    if (n > MENU_ROWS) {  /* the scroll bar: its track, the part shown */
        const int track = step * MENU_ROWS - 2, thumb = track * MENU_ROWS / n;
        hud_panel(x + w - 14, list_y - 3, 4, track, RULE, C);
        hud_panel(x + w - 14, list_y - 3 + (track - thumb) * s.scroll / (n - MENU_ROWS), 4, thumb, BAR, C);
    }
    const int ch = list[s.sel];
    const challenge_def* c = &CHALLENGES[ch];
    if (s.toast_t > 0) hud_text_at(256, info_y, YELLOW, 1.05f, C, 1, "%s", s.toast);
    else if (c->kind == RAIL && !has_boots()) hud_text_at(256, info_y, GREY, 1.05f, C, 1, "Servono i Grind Scarponi");
    else hud_text_at(256, info_y, WHITE, 1.05f, C, 1, "%s", c->text);
    hud_maxw(w - 32);
    if (s.best[ch])
        hud_text_at(256, info_y + 19, YELLOW, 1.0f, C, 1, "Premio %d bulloni   Record %u:%02u", c->reward, (unsigned)(s.best[ch] / 3600),
                    (unsigned)(s.best[ch] / 60 % 60));
    else
        hud_text_at(256, info_y + 19, YELLOW, 1.0f, C, 1, "Premio %d bulloni", c->reward);
    hud_maxw(w - 32);
    hud_text_at(256, info_y + 46, GREY, 0.95f, C, 1, "Croce: inizia      Cerchio: esci");
    hud_maxw(w - 32);
    hud_text_at(256, info_y + 64, GREY, 0.95f, C, 1, "Quadrato: rivenditore");
    hud_maxw(w - 32);
}

static void menu_close(void) {
    s.mode = FREE;
    s.t = 0;
    api->input_hold(0);
}

static void frame(void* user) {
    (void)user;
    s.nlines = s.nrects = 0;
    if (!s.mine) return;
    const uint32_t hero = api->hero();
    if (hero == 0) return;
    float h[3];
    pos_of(hero, h);
    const uint32_t pressed = api->pad_pressed();
    nav_build();
    flames_light();
    bombs_update();
    supplies_update();
    ++s.t;
    switch (s.mode) {
    case FREE:
        if (s.t_center >= 0 && api->trigger_inside(s.t_center)) {
            hud_panel(150, 370, 212, 26, PANEL, RAC1_HUD_CENTRE);
            hud_text_at(256, 375, WHITE, 1.2f, RAC1_HUD_CENTRE, 1, "Triangolo: menu arena");
            hud_maxw(440);
            hud_fit(1, 1);
            if (pressed & PAD_TRIANGLE) {
                s.mode = MENU;
                s.t = 0;
                api->input_hold(1);  /* (Ratchet stands idle while it is open; the menu reads the real pad) */
            }
        }
        break;
    case MENU: {
        if (s.t > 1) {
            int list[N_CHALLENGES];
            const int n = menu_list(list);
            if (pressed & PAD_UP) s.sel = (s.sel + n - 1) % n;
            if (pressed & PAD_DOWN) s.sel = (s.sel + 1) % n;
            if (pressed & (PAD_CIRCLE | PAD_TRIANGLE)) {
                menu_close();
            } else if (pressed & PAD_SQUARE) {
                menu_close();
                if (!api->vendor_open()) toast("Il rivenditore non si apre ora");
            } else if (pressed & PAD_CROSS) {
                int32_t w[16];
                const challenge_def* c = &CHALLENGES[list[s.sel]];
                const int need = c->weapons == W_ROULETTE || c->weapons == W_CHAOS || c->kind == CUP ? 2 : 0;
                if (c->kind == RAIL && !has_boots()) {
                    toast("Servono i Grind Scarponi");
                } else if (need > 0 && weapons_usable(w) < need) {
                    toast("Servono almeno 2 armi");
                } else {
                    api->input_hold(0);
                    challenge_start(list[s.sel]);
                }
            }
        }
        if (s.mode == MENU) menu_lines();
        break;
    }
    case INTRO: {
        char cd[32];
        snprintf(cd, sizeof cd, "Si comincia tra %d", 3 - s.t / 60);
        banner(YELLOW, CHALLENGES[s.ch].name, cd);
        weapon_update();
        if (s.t >= INTRO_FRAMES) wave_start();
        break;
    }
    case FIGHT:
    case BREAK:
        ++s.clock;
        weapon_update();
        if (s.mode == FIGHT) fight_update(h);
        else if (s.t >= BREAK_FRAMES) wave_start();
        if (s.mode == BREAK) {
            const int isc = CHALLENGES[s.ch].kind == CUP;
            char next[48] = "";  /* (a championship: the next round's mode, when it changes) */
            if (isc && cup[s.wave].mode != cup[s.wave - 1].mode) snprintf(next, sizeof next, "Prossimo: %s", MODES[cup[s.wave].mode].name);
            banner(GREEN, isc ? "Round superato!" : "Ondata superata!", next[0] ? next : NULL);
        }
        if (s.mode != RESULT) {  /* the challenge's box, at the screen's left, under the game's weapon and ammo */
            const challenge_def* c = &CHALLENGES[s.ch];
            const int isc = c->kind == CUP;
            char third[64] = "";  /* the mode (a championship's round), the weapon (a limited one) */
            char wl[40] = "";
            if (s.rule.weapons == W_CHAOS) snprintf(wl, sizeof wl, "%s  %d s", weapon_name(s.wpn), (s.wpn_t + 59) / 60);
            else if (s.rule.weapons == W_ROULETTE || (s.rule.weapons == W_WRENCH && !isc)) snprintf(wl, sizeof wl, "%s", weapon_name(s.wpn));
            if (isc) snprintf(third, sizeof third, "%s%s%s", MODES[cup[s.wave < c->waves ? s.wave : c->waves - 1].mode].name, wl[0] ? " - " : "", wl);
            else if (wl[0]) snprintf(third, sizeof third, "Arma: %s", wl);
            else if (c->kind == RAIL && rails.phase < 2 && (rails.falls > 0 || rails.drones > 0)) {
                if (rails.use_obs && rails.drones > 0) snprintf(third, sizeof third, "Droni %d/%d   Cadute %d", rails.drones_hit, rails.drones, rails.falls);
                else snprintf(third, sizeof third, "Cadute: %d", rails.falls);
            }
            const int y = 84, bh = third[0] ? 72 : 50, box = s.nrects, first = s.nlines;
            hud_panel(12, y, 230, bh, PANEL, RAC1_HUD_LEFT);
            hud_panel(12, y, 3, bh, BAR, RAC1_HUD_LEFT);
            if (third[0]) hud_text_at(22, y + 50, CYAN, 1.1f, RAC1_HUD_LEFT, 0, "%s", third);
            if (c->kind == SURVIVE) {
                const int left = c->time * 60 - s.clock;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", c->name);
                hud_text_at(22, y + 28, WHITE, 1.1f, RAC1_HUD_LEFT, 0, "Resisti %d:%02d   Abbattuti %d", left / 3600, left / 60 % 60, s.felled);
            } else if (c->kind == RAIL && rails.phase < 2) {
                const int left = c->time * 60 - rails.ride;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", c->name);
                hud_text_at(22, y + 28, left < 600 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0, "Restano %d:%02d", left / 3600, left / 60 % 60);
            } else {
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s  %d/%d", c->name, s.wave + 1, c->waves);
                const int timed = c->kind == TIMED || s.rule.time > 0;
                const int t = c->kind == TIMED ? c->time * 60 - s.clock : s.rule.time > 0 ? s.rule.time * 60 - s.round_clock : s.clock;
                hud_text_at(22, y + 28, timed && t < 600 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0, "Nemici %d   %s%d:%02d",
                            s.mode == FIGHT ? cur_wave()->count - s.killed : 0, timed ? "restano " : "", t / 3600, t / 60 % 60);
            }
            hud_grow(box, first, 300);  /* (wider round long texts; smaller texts past 300) */
        }
        const float jx = h[0] - s.last_h[0], jy = h[1] - s.last_h[1], jz = h[2] - s.last_h[2];
        if (s.mode != RESULT && (api->health() <= 0 || (s.have_last && jx * jx + jy * jy + jz * jz > 64.0f)))
            challenge_end(0, "Ratchet e' caduto");  /* (dead: his health gone, or back at his start - the lava, a fall) */
        else if (s.mode != RESULT && s.t_pad >= 0 && api->trigger_inside(s.t_pad)) challenge_end(0, "abbandonata");
        break;
    case RESULT:
        if (s.won) {
            char line[48];
            if (CHALLENGES[s.ch].kind == SURVIVE)
                snprintf(line, sizeof line, "+%d bulloni   %d abbattuti", CHALLENGES[s.ch].reward, s.felled);
            else
                snprintf(line, sizeof line, "+%d bulloni   %d:%02d", CHALLENGES[s.ch].reward, s.clock / 3600, s.clock / 60 % 60);
            banner(GREEN, "SFIDA COMPLETATA!", line);
        } else {
            banner(RED, "SFIDA FALLITA", s.why[0] ? s.why : NULL);
        }
        if (s.t >= RESULT_FRAMES) {
            s.mode = FREE;
            s.t = 0;
        }
        break;
    default:
        break;
    }
    if (s.placed > 0) {  /* (moved by the plugin: his place jumped, not a death) */
        --s.placed;
        s.have_last = 0;
    } else {
        memcpy(s.last_h, h, sizeof h);
        s.have_last = 1;
    }
    if (s.toast_t > 0) {
        --s.toast_t;
        if (s.mode != MENU) {
            hud_panel(126, 232, 260, 26, PANEL, RAC1_HUD_CENTRE);
            hud_text_at(256, 237, YELLOW, 1.2f, RAC1_HUD_CENTRE, 1, "%s", s.toast);
            hud_maxw(440);
            hud_fit(1, 1);
        }
    }
}

static void hud(void* user) {
    (void)user;
    if (!s.mine) return;
    float scale[15];  /* each text's scale: smaller when wider than its maxw */
    for (int i = 0; i < s.nlines; ++i) {
        scale[i] = s.line[i].scale;
        const int tw = s.line[i].maxw > 0 ? api->hud_text_width(s.line[i].text, scale[i]) : 0;
        if (tw > s.line[i].maxw) scale[i] *= (float)s.line[i].maxw / (float)tw;
    }
    for (int i = 0; i < s.nrects; ++i) {
        int x = s.rect[i].x, w = s.rect[i].w;
        if (s.rect[i].fit_n > 0 && s.rect[i].grow > 0) {  /* (hud_grow) its left edge kept: 10 units past its texts' ends */
            for (int k = s.rect[i].fit_first; k < s.rect[i].fit_first + s.rect[i].fit_n && k < s.nlines; ++k) {
                const int right = s.line[k].x + api->hud_text_width(s.line[k].text, scale[k]) + 10;
                if (right - x > w) w = right - x;
            }
            if (w > s.rect[i].grow) w = s.rect[i].grow;
        } else if (s.rect[i].fit_n > 0) {  /* round its texts (centred or from their x), 20 units each side, kept centred on 256;
                                       480 at most (wider than the 4:3 frame, a panel would reach the screen's edges) */
            int half = w / 2;
            for (int k = s.rect[i].fit_first; k < s.rect[i].fit_first + s.rect[i].fit_n && k < s.nlines; ++k) {
                const int tw = api->hud_text_width(s.line[k].text, scale[k]);
                const int left = s.line[k].centred ? 256 - tw / 2 : s.line[k].x, right = left + tw;
                if (256 - left + 20 > half) half = 256 - left + 20;
                if (right - 256 + 20 > half) half = right - 256 + 20;
            }
            if (half > 240) half = 240;
            if (s.rect[i].w < 2 * half) {
                const int grow = 2 * half - s.rect[i].w;
                w = s.rect[i].w + grow;
                x = s.rect[i].x - grow / 2;
            }
        }
        api->hud_rect(x, s.rect[i].y, w, s.rect[i].h, s.rect[i].colour, s.rect[i].anchor);
    }
    for (int i = 0; i < s.nlines; ++i) {
        int x = s.line[i].x;
        if (s.line[i].centred) x -= api->hud_text_width(s.line[i].text, scale[i]) / 2;
        api->hud_text_ex(x, s.line[i].y, s.line[i].colour, scale[i], s.line[i].anchor, s.line[i].text);
    }
}

/* ---- the level ---- */

static void look_up_sounds(void);

static void level_start(void* user, const rac1_level_info* level) {
    (void)user;
    s.mine = level->mine;
    s.mode = FREE;
    s.t = 0;
    s.nen = 0;
    s.ships[0] = s.ships[1] = 0;
    s.wpn = 0;  /* (weapon_allow is off again at every level's start) */
    s.toast_t = 0;
    s.have_last = 0;
    if (!s.mine) {
        s.lost_reload = 0;  /* (another level: not the arena loaded again) */
        return;
    }
    look_up_sounds();
    s.t_center = api->trigger_find("center");
    s.t_pad = api->trigger_find("pad");
    s.npoints = 0;
    for (int i = 0; i < N_POINTS; ++i) {
        const uint32_t o = api->object_named(POINTS[i]);
        if (o == 0) continue;
        pos_of(api->object_moby(o), s.points[s.npoints]);
        s.is_drop[s.npoints] = strncmp(POINTS[i], "drop_", 5) == 0;
        s.point_name[s.npoints++] = POINTS[i];
    }
    const uint32_t hub = api->object_named("hub");
    if (hub != 0) pos_of(api->object_moby(hub), s.centre);
    const uint32_t lava = api->object_named("info_lava");
    s.lava_ok = lava != 0;
    if (s.lava_ok) s.lava_z = rac1_lwf(api, api->object_moby(lava) + 0x18);
    nav_reset();
    cls_bomb = api->class_find("bomb");
    cls_mark = api->class_find("mark");
    for (int i = 0; i < MAX_BOMBS; ++i) bombs[i].bomb = bombs[i].mark = 0, bombs[i].flash = 0;
    for (int i = 0; i < N_SLOTS; ++i) slot_crate[i] = 0, slot_flash[i] = 0;
    supplies_drop(CRATE_NANO, 2);  /* (two nanotech crates there from the start) */
    if (!rails.loaded) rails_load();
    rails.on = 0;  /* (the level's table is its own again) */
    rails.phase = 0;
    for (int i = 0; i < rails.n; ++i) rail_show(i, 0);
    obstacles_find();
    if (s.lost_reload) {  /* (the challenge he died in: lost, its banner now) */
        s.lost_reload = 0;
        s.won = 0;
        snprintf(s.why, sizeof s.why, "Ratchet e' caduto");
        s.mode = RESULT;
        play(s.snd_fail);
        char lost[96];
        snprintf(lost, sizeof lost, "challenge %s: Ratchet e' caduto (the level loaded again)", CHALLENGES[s.ch].name);
        api->log(lost);
    }
    char msg[128];
    snprintf(msg, sizeof msg, "the arena: %d spawn points; room for %d robots, %d bomb bots, %d Gadgetron, %d troopers, %d dropships; the lava at %.1f, triggers %d / %d",
             s.npoints, (int)api->base_free(ROBOT), (int)api->base_free(52), (int)api->base_free(541), (int)api->base_free(638),
             (int)api->base_free(1356), s.lava_z, (int)s.t_center, (int)s.t_pad);
    api->log(msg);
}

static void level_end(void* user) {
    (void)user;
    s.lost_reload = s.mine && (s.mode == INTRO || s.mode == FIGHT || s.mode == BREAK);
    if (s.mine) rails.on = 0;
    s.mine = 0;
    s.nen = 0;  /* (the level's objects are gone with it) */
    s.ships[0] = s.ships[1] = 0;
    s.wpn = 0;
    s.mode = FREE;
    s.frenzy = 0;
    memset(&s.rule, 0, sizeof s.rule);
    nflames = 0;
    for (int i = 0; i < MAX_BOMBS; ++i) bombs[i].bomb = bombs[i].mark = 0, bombs[i].flash = 0;
    for (int i = 0; i < N_SLOTS; ++i) slot_crate[i] = 0, slot_flash[i] = 0;
}

/* the save: "ARN2", then each challenge by its name's hash: won (0/1), its best time (frames, 0 none). By name, so a
   challenge added or moved keeps what was won; "ARN1" (the first seven, by their place) is still read */
static uint32_t name_hash(const char* n) {
    uint32_t h = 2166136261u;  /* (FNV-1a) */
    for (; *n; ++n) h = (h ^ (uint8_t)*n) * 16777619u;
    return h;
}

static void record_set(const char* name, uint32_t won, uint32_t best) {
    for (int i = 0; i < N_CHALLENGES; ++i)
        if (strcmp(CHALLENGES[i].name, name) == 0) {
            if (won) s.done |= 1u << i;
            s.best[i] = best;
        }
}

static uint32_t save(void* user, uint8_t* buf, uint32_t cap) {
    (void)user;
    const uint32_t n = 8 + 12 * N_CHALLENGES;
    if (cap < n) return 0;
    memcpy(buf, "ARN2", 4);
    const uint32_t count = N_CHALLENGES;
    memcpy(buf + 4, &count, 4);
    for (int i = 0; i < N_CHALLENGES; ++i) {
        const uint32_t rec[3] = {name_hash(CHALLENGES[i].name), (s.done >> i) & 1u, s.best[i]};
        memcpy(buf + 8 + 12 * i, rec, 12);
    }
    return n;
}

static void load(void* user, const uint8_t* data, uint32_t size) {
    (void)user;
    s.done = 0;
    memset(s.best, 0, sizeof s.best);
    if (data == NULL || size < 8) return;
    if (memcmp(data, "ARN1", 4) == 0) {
        static const char* const OLD[7] = {"Primo sangue", "Assedio", "Pioggia di fuoco", "Contro il tempo", "Ultimo in piedi", "Intoccabile", "Inferno"};
        uint32_t done, best;
        memcpy(&done, data + 4, 4);
        for (uint32_t i = 0; i < 7 && 8 + 4 * i + 4 <= size; ++i) {
            memcpy(&best, data + 8 + 4 * i, 4);
            record_set(OLD[i], (done >> i) & 1u, best);
        }
        return;
    }
    if (memcmp(data, "ARN2", 4) != 0) return;
    uint32_t count;
    memcpy(&count, data + 4, 4);
    for (uint32_t k = 0; k < count && 8 + 12 * k + 12 <= size; ++k) {
        uint32_t rec[3];
        memcpy(rec, data + 8 + 12 * k, 12);
        for (int i = 0; i < N_CHALLENGES; ++i)
            if (name_hash(CHALLENGES[i].name) == rec[0]) {
                if (rec[1]) s.done |= 1u << i;
                s.best[i] = rec[2];
            }
    }
}

static void new_game(void* user) {
    (void)user;
    s.done = 0;
    memset(s.best, 0, sizeof s.best);
}

/* RAC1_MOD_TEST=arena: Ratchet on the hub; arena_near: on the floor 8 units in front of the north drop zone's point;
   arena_win: on the hub, his health kept full, a robot taken away every 1.5 s (the waves' logic through to the win);
   arena_lava: dropped into the lava; arena_menu: the menu opened from frame 200 to 500 (with RAC1_SIM_INPUT=forward);
   arena_look: in front of the north-east vent, the flames kept burning (a screenshot's); arena_tower: on the north-east
   tower's top, his health kept full, the challenge from frame 420 (the robots' paths up the ramps); arena_fall: the same,
   then down to the floor at 1500 (the robots come down); arena_bombs: "Pioggia di fuoco" from 420, the bombs counted;
   arena_ch<N> (arena_ch<N>k: an enemy taken away every 1.5 s, f: every third of a second): Ratchet south of the hub,
   four weapons if he owns one,
   challenge N from 420, the enemies by kind, the dropships, the weapon in his hand */
static void test(void* user, const char* text, uint64_t frame) {
    (void)user;
    const int near = strcmp(text, "arena_near") == 0, win = strcmp(text, "arena_win") == 0, lava = strcmp(text, "arena_lava") == 0;
    /* the menu left open (the run's last frame's screenshot); arena_menushot_all: every challenge won but the hidden
       one (shown now), the list's last chosen (scrolled) */
    const int shot = strncmp(text, "arena_menushot", 14) == 0, shot_all = strcmp(text, "arena_menushot_all") == 0;
    if (s.mine && strcmp(text, "arena_resultshot") == 0) {  /* the won banner held (the run's last frame's screenshot) */
        if (frame == 200) {
            s.ch = 0;
            s.clock = 5460;
            challenge_end(1, NULL);
        }
        if (frame > 200 && s.mode == RESULT) s.t = 0;
        return;
    }
    const int menu = strcmp(text, "arena_menu") == 0 || shot, look = strcmp(text, "arena_look") == 0;
    const int fall = strcmp(text, "arena_fall") == 0;    /* the tower, then down to the floor at frame 1500: they come down */
    const int tower = strcmp(text, "arena_tower") == 0 || fall;  /* on the north-east tower's top: up the ramps (the paths) */
    const int bombs_t = strcmp(text, "arena_bombs") == 0;  /* "Pioggia di fuoco", his health kept full */
    const int course_t = strncmp(text, "arena_course", 12) == 0;
    if (s.mine && (strncmp(text, "arena_rail", 10) == 0 || course_t)) {
        /* a rail challenge from 420 (arena_rail*: the test rail, arena_course*: the course), the Grind Scarponi given;
           arena_rail / arena_course: Ratchet dropped onto the first rail's third point at 640 (arena_railwalk /
           arena_coursewalk: left on the station, for a RAC1_SIM_INPUT script); his health kept up; the course's last
           wave: an enemy taken away every 1.5 s */
        const char* want = course_t ? "Grind Scarponi: Discesa" : "Test Grind Scarponi";
        int ch = -1;
        for (int i = 0; i < N_CHALLENGES; ++i)
            if (strcmp(CHALLENGES[i].name, want) == 0) ch = i;
        if (frame == 100) api->sb(GRIND_BOOTS, 1);  /* (a test's game has no Grind Scarponi) */
        if (frame == 420 && ch >= 0) challenge_start(ch);
        if (frame == 640 && (strcmp(text, "arena_rail") == 0 || strcmp(text, "arena_course") == 0) && rails.nset > 0) {
            const float *a = rails.r[rails.set[0]].pt[2], *b = rails.r[rails.set[0]].pt[4];
            const float at[3] = {a[0], a[1], a[2] + 1.5f};
            hero_move(at, atan2f(b[1] - a[1], b[0] - a[0]));
            api->log("test: Ratchet dropped over the first rail's third point");
        }
        if (frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
        if (rails.phase == 2 && s.mode == FIGHT && frame % 90 == 5 && s.nen > 0) {
            const uint32_t m = api->base_moby(s.en[0].h);
            if (m != 0) api->moby_remove(m);
        }
        const int riding = s.mode == FIGHT && rails.phase == 1;
        if (frame >= 420 && frame <= 4000 && api->hero() != 0 && rails.nset > 0 && (riding ? frame % 6 == 0 : frame % 60 == 0)) {
            float h[3];
            pos_of(api->hero(), h);
            int best = 0, lane = 0;
            float bd = 1e9f;
            for (int j = 0; j < rails.nset; ++j)
                for (int k = 0; k < rails.r[rails.set[j]].npts; ++k) {
                    const float d = dist3(h, rails.r[rails.set[j]].pt[k]);
                    if (d < bd) bd = d, best = k, lane = rails.set[j];
                }
            int alive = 0;
            for (int i = 0; i < rails.nobs; ++i) alive += rails.obs[i].alive;
            char b[240];
            snprintf(b, sizeof b, "test %llu: mode %d phase %d, hero %.2f %.2f %.2f, state 0x%x, rail %s point %d at %.2f, health %d, obstacles %d, drones %d/%d, falls %d, table %d, alive %d",
                     (unsigned long long)frame, s.mode, rails.phase, h[0], h[1], h[2], (unsigned)rac1_lw(api, HERO_STATE), rails.r[lane].name, best, bd,
                     (int)api->health(), alive, rails.drones_hit, rails.drones, rails.falls, (int)rac1_lw(api, api->ov(RAIL_COUNT)), s.nen);
            api->log(b);
        }
        return;
    }
    if (s.mine && strncmp(text, "arena_ch", 8) == 0) {  /* challenge N (arena_ch<N>, a k after it: an enemy taken away every 1.5 s) */
        const int ch = atoi(text + 8), kill = strchr(text + 8, 'k') != NULL ? 90 : strchr(text + 8, 'f') != NULL ? 20 : 0;
        if (frame == 100) {
            const float at[3] = {s.centre[0], s.centre[1] - 20.0f, nav.floor_z + 0.5f};
            api->hero_place(at, 1.5708f);
            static const int32_t GIVE[4] = {0x0a, 0x0b, 0x0f, 0x10};  /* (fewer than two owned in a test's game: four given) */
            if (api->weapons_owned(NULL, 0) < 2)
                for (int i = 0; i < 4; ++i) api->sb(0x0013d4c0u + (uint32_t)GIVE[i], 1);
            char b[96];
            snprintf(b, sizeof b, "test: Ratchet south of the hub, %d weapons owned", (int)api->weapons_owned(NULL, 0));
            api->log(b);
        }
        if (frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
        if (frame == 420 && ch >= 0 && ch < N_CHALLENGES) challenge_start(ch);
        if (kill && frame % (uint64_t)kill == 5 && s.mode == FIGHT && s.nen > 0) {
            const uint32_t m = api->base_moby(s.en[0].h);
            if (m != 0) api->moby_remove(m);
        }
        if (frame % 120 == 0 || (frame >= 840 && frame <= 1260 && frame % 30 == 0)) {  /* (often round a quick select test's) */
            int by[N_KINDS] = {0};
            for (int i = 0; i < s.nen; ++i) ++by[s.en[i].kind];
            float h[3];
            pos_of(api->hero(), h);
            char b[320];
            float e0[3] = {0.0f, 0.0f, 0.0f};
            if (s.nen > 0 && api->base_moby(s.en[0].h) != 0) pos_of(api->base_moby(s.en[0].h), e0);
            snprintf(b, sizeof b, "test %llu: clock %d, enemy 0 at %.2f %.2f, mode %d wave %d spawned %d killed %d felled %d alive R%d B%d L%d C%d F%d J%d G%d, ships %d %d, in hand 0x%x (asked 0x%x), health %d, hero %.1f %.1f %.1f",
                     (unsigned long long)frame, s.clock, e0[0], e0[1], s.mode, s.wave, s.spawned, s.killed, s.felled, by[K_ROBOT], by[K_BOMBER], by[K_LAUNCHER], by[K_CLUB], by[K_FLAMER],
                     by[K_JET], by[K_GUNSHIP], s.ships[0] && api->base_moby(s.ships[0]) ? 1 : 0, s.ships[1] && api->base_moby(s.ships[1]) ? 1 : 0,
                     (unsigned)rac1_lw(api, HERO_ITEM), (unsigned)s.wpn, (int)api->health(), h[0], h[1], h[2]);
            api->log(b);
        }
        if (frame % 240 == 0)
            for (int i = 0; i < s.nen; ++i) {
                const uint32_t m = api->base_moby(s.en[i].h);
                if (m == 0) continue;
                float r[3], h[3];
                pos_of(m, r);
                pos_of(api->hero(), h);
                char line[200];
                snprintf(line, sizeof line, "  %c %d: at %.1f %.1f %.1f over %.1f (%.1f from Ratchet), state %d, %s", KIND[s.en[i].kind].letter, i, r[0], r[1], r[2], floor_under(m, r),
                         sqrtf((r[0] - h[0]) * (r[0] - h[0]) + (r[1] - h[1]) * (r[1] - h[1]) + (r[2] - h[2]) * (r[2] - h[2])), (int)(int8_t)rac1_lb(api, m + 0x20),
                         KIND[s.en[i].kind].moves ? "" : s.en[i].direct ? "direct" : "path");
                api->log(line);
            }
        return;
    }
    if (!s.mine || (strcmp(text, "arena") != 0 && !near && !win && !lava && !menu && !look && !tower && !bombs_t)) return;
    if (bombs_t) {
        if (frame == 100) {
            const float at[3] = {s.centre[0], s.centre[1] - 20.0f, nav.floor_z + 0.5f};
            api->hero_place(at, 1.5708f);
        }
        if (frame == 420) challenge_start(4);  /* ("Pioggia di fuoco") */
        if (frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
        if (frame % 120 == 0) {
            char b[160];
            int full = 0;
            for (int i = 0; i < N_SLOTS; ++i) full += !slot_free(i);
            snprintf(b, sizeof b, "test %llu: mode %d wave %d, bombs landed %d (Ratchet hurt %d, robots hit %d), robots felled %d, alive %d, crates given %d, on the hub %d",
                     (unsigned long long)frame, s.mode, s.wave, bombs_landed, bombs_hurt, bombs_hit, s.felled, s.nen, supplies_given, full);
            api->log(b);
        }
        return;
    }
    if (fall && frame == 1500) {  /* down beside the tower: the robots up there must come down, not walk on the air */
        const float at[3] = {s.centre[0] + 28.0f, s.centre[1] + 32.0f, nav.floor_z + 0.5f};
        api->hero_place(at, 0.0f);
        s.have_last = 0;
        api->log("test: Ratchet down on the floor");
    }
    if (tower && frame % 30 == 0 && api->health() > 0 && api->health() < 4) {
        static int hurts;
        char b[64];
        snprintf(b, sizeof b, "test %llu: Ratchet hurt (%d so far)", (unsigned long long)frame, ++hurts);
        api->log(b);
        api->sw(0x001415f8u, 4);
    }
    if (look) {  /* Ratchet 6 units south of the north-east vent, facing it, the flames kept burning (for a screenshot) */
        s.look = 1;
        const uint32_t o = api->object_named("ne");
        if (frame == 100 && o != 0) {
            float at[3];
            pos_of(api->object_moby(o), at);
            at[1] -= 6.0f;
            at[2] += 1.2f;
            api->hero_place(at, 1.5708f);
            api->log("test: Ratchet in front of the north-east vent");
        }
        return;
    }
    if (menu) {  /* the menu opened at frame 200 and closed at 500 (with RAC1_SIM_INPUT=forward: he must not move in between) */
        if (frame == 200) {
            s.mode = MENU;
            s.t = 0;
            if (shot) s.sel = 1;
            if (shot_all) {
                for (int i = 0; i < N_CHALLENGES; ++i)
                    if (!CHALLENGES[i].hidden) s.done |= 1u << i, s.best[i] = 3600u + 60u * (uint32_t)i;
                s.sel = N_CHALLENGES - 1;
            }
            api->input_hold(1);
            api->log("test: the menu opened");
        }
        if (frame == 500 && !shot) {
            menu_close();
            api->log("test: the menu closed");
        }
        if (frame % 30 == 0 && frame >= 120 && frame <= 700) {
            float h[3];
            pos_of(api->hero(), h);
            char b[128];
            snprintf(b, sizeof b, "test %llu: mode %d, hero %.2f %.2f %.2f state 0x%x", (unsigned long long)frame, s.mode, h[0], h[1], h[2],
                     (unsigned)rac1_lw(api, HERO_STATE));
            api->log(b);
        }
        return;
    }
    if (lava) {  /* Ratchet dropped into the lava at frame 100, a line every 10 frames */
        const uint32_t o = api->object_named("info_lava");
        if (frame == 100 && o != 0) {
            float at[3];
            pos_of(api->object_moby(o), at);
            at[2] += 1.0f;
            api->hero_place(at, 0.0f);
            api->log("test: Ratchet dropped into the lava");
        }
        if (frame >= 100 && frame <= 400 && frame % 10 == 0) {
            float h[3];
            pos_of(api->hero(), h);
            char b[128];
            snprintf(b, sizeof b, "test %llu: hero %.1f %.1f %.1f health %d state 0x%x", (unsigned long long)frame, h[0], h[1], h[2],
                     (int)api->health(), (unsigned)rac1_lw(api, HERO_STATE));
            api->log(b);
        }
        return;
    }
    if (win && frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
    if (win && frame % 90 == 45 && s.mode == FIGHT && s.nen > 0) {  /* (the waves' logic, whatever the aim) */
        const uint32_t m = api->base_moby(s.en[0].h);
        if (m != 0) api->moby_remove(m);
    }
    if (win && frame % 60 == 0) {
        char b[64];
        snprintf(b, sizeof b, "test %llu: bolts %d, challenges done %u", (unsigned long long)frame, (int)api->bolts(), (unsigned)s.done);
        api->log(b);
    }
    if (frame == 100) {
        float at[3] = {s.centre[0], s.centre[1], s.centre[2] + 0.5f};
        const uint32_t o = api->object_named("drop_n");
        if (near && o != 0) {
            pos_of(api->object_moby(o), at);
            at[1] -= 8.0f;
            at[2] += 0.5f;
        }
        if (tower) {  /* the tower's top: 45, 45 from the hub's point (beside its ammo crate), 6 over the floor */
            at[0] = s.centre[0] + 45.0f;
            at[1] = s.centre[1] + 45.0f;
            at[2] = nav.floor_z + 6.5f;
        }
        api->hero_place(at, 0.0f);
        api->log(tower ? "test: Ratchet put on the north-east tower" : near ? "test: Ratchet put in front of the north drop zone" : "test: Ratchet put on the hub");
    }
    if (frame % 120 == 0)
        for (int i = 0; i < s.nen; ++i) {
            const enemy* e = &s.en[i];
            const uint32_t m = api->base_moby(e->h);
            if (m == 0) continue;
            float r[3], h[3];
            pos_of(m, r);
            pos_of(api->hero(), h);
            char line[160];
            const float g = floor_under(m, r);
            snprintf(line, sizeof line, "  %c %d: at %.1f %.1f %.1f over %.1f (%.1f from Ratchet), state %d, %s %.1f %.1f, yaw %.2f", KIND[e->kind].letter, i, r[0], r[1], r[2], g,
                     sqrtf((r[0] - h[0]) * (r[0] - h[0]) + (r[1] - h[1]) * (r[1] - h[1]) + (r[2] - h[2]) * (r[2] - h[2])), (int)(int8_t)rac1_lb(api, m + 0x20),
                     e->direct ? "direct, to" : "path, to", e->direct ? h[0] : e->to[0], e->direct ? h[1] : e->to[1], rac1_lwf(api, m + 0x48));
            api->log(line);
        }
    if (frame == (tower ? 420u : 160u)) challenge_start(tower ? 1 : 0);  /* (the tower: "Manichini", every walker) */
    if (frame % 60 != 0) return;
    float h[3];
    pos_of(api->hero(), h);
    char msg[160];
    snprintf(msg, sizeof msg, "test %llu: mode %d wave %d spawned %d killed %d alive %d room %d, hero %.1f %.1f %.1f health %d state 0x%x center %d",
             (unsigned long long)frame, s.mode, s.wave, s.spawned, s.killed, s.nen, (int)api->base_free(ROBOT), h[0], h[1], h[2],
             (int)api->health(), (unsigned)rac1_lw(api, HERO_STATE), s.t_center >= 0 ? api->trigger_inside(s.t_center) : -1);
    api->log(msg);
}

/* ---- the flame vents ---- */

#define REST_FRAMES 300  /* 5 s at rest */
#define WARN_FRAMES 60   /* 1 s of warning */
#define BURN_FRAMES 150  /* 2.5 s burning */
#define DOWN_FRAMES 12   /* dying down */
#define GROW_FRAMES 8    /* growing to full height */
#define REACH 1.8f       /* how far from the vent's middle the flame burns (horizontally) */
#define HEIGHT 3.5f      /* and how high */
#define HURT_EVERY 30    /* frames between two hurts */

static struct {
    int32_t roar, burst, hiss;
} snd = {-1, -1, -1};

static void look_up_sounds(void) {
    snd.roar = api->sfx_find("flame_roar");
    snd.burst = api->sfx_find("flame_burst");
    snd.hiss = api->sfx_find("flame_hiss");
    s.snd_wave = api->sfx_find("wave_start");
    s.snd_win = api->sfx_find("challenge_win");
    s.snd_fail = api->sfx_find("challenge_fail");
    snd_whistle = api->sfx_find("bomb_whistle");
    snd_boom = api->sfx_find("bomb_boom");
}

enum { REST, WARN, BURN, DOWN };


typedef struct flame {
    float home[3];
    int phase, t, hurt;
    int32_t roar;  /* its roar's sound slot (-1 none) */
    float spin;
} flame;

/* its model (crossed planes of fire) at `scale` (hidden below a sliver), turned to a new angle every few frames: the
   planes stand 60 degrees apart, so a jump of the angle reads as the tongues flickering */
static void flame_show(uint32_t obj, flame* f, float scale) {
    const float rot[3] = {0.0f, 0.0f, f->spin};
    if (scale < 0.03f) {
        api->object_show(obj, 0);
        api->object_place(obj, NULL, rot, 0.03f);
        return;
    }
    api->object_show(obj, 1);
    api->object_place(obj, NULL, rot, scale);
}

static void flame_quiet(flame* f) {
    if (f->roar >= 0 && api->sfx_active(f->roar, snd.roar)) api->sfx_stop(f->roar);
    f->roar = -1;
}

static void flame_spawn(void* user, uint32_t obj, const rac1_spawn* sp) {
    (void)user;
    look_up_sounds();
    flame* f = (flame*)api->object_state(obj);
    memcpy(f->home, sp->pos, sizeof f->home);
    f->phase = REST;
    f->roar = -1;
    char v[16] = "";
    f->t = api->object_param(obj, "delay", v, sizeof v) ? -atoi(v) : -90 * (sp->index > 0 ? sp->index : 0);
    flame_show(obj, f, 0.0f);
    if (nflames < 8) flames[nflames++] = obj;
}

static void flame_remove(void* user, uint32_t obj) {
    (void)user;
    flame_quiet((flame*)api->object_state(obj));
}

/* Ratchet in the flame: within REACH of its middle, his feet between just under the vent and HEIGHT over it */
static int hero_in(const flame* f) {
    const uint32_t hero = api->hero();
    if (hero == 0) return 0;
    const float dx = rac1_lwf(api, hero + 0x10) - f->home[0], dy = rac1_lwf(api, hero + 0x14) - f->home[1];
    const float dz = rac1_lwf(api, hero + 0x18) - f->home[2];
    return dx * dx + dy * dy < REACH * REACH && dz > -0.5f && dz < HEIGHT;
}

static void flame_update(void* user, uint32_t obj) {
    (void)user;
    flame* f = (flame*)api->object_state(obj);
    const uint32_t m = api->object_moby(obj);
    if (f->t % 3 == 0) f->spin = rnd() * 1.0472f;
    ++f->t;
    if (s.look && f->phase != BURN) {
        f->phase = BURN;
        f->t = GROW_FRAMES;
    }
    if (s.look && f->t >= BURN_FRAMES - 1) f->t = GROW_FRAMES;
    switch (f->phase) {
    case REST:
        flame_show(obj, f, 0.0f);
        if (f->t >= (s.frenzy ? REST_FRAMES / 3 : REST_FRAMES)) {
            f->phase = WARN;
            f->t = 0;
            api->sfx_play(snd.hiss, m, NULL);
        }
        break;
    case WARN:
        if (f->t % 12 == 1) {
            const float at[3] = {f->home[0] + (rnd() - 0.5f), f->home[1] + (rnd() - 0.5f), f->home[2] + 0.3f};
            const float up[3] = {0.0f, 0.0f, 1.2f + rnd()};
            api->smoke(at, up, 1.2f);
        }
        flame_show(obj, f, 0.18f + 0.12f * rnd());
        if (f->t >= WARN_FRAMES) {
            f->phase = BURN;
            f->t = 0;
            f->hurt = 0;
            api->sfx_play(snd.burst, m, NULL);
            f->roar = api->sfx_play(snd.roar, m, NULL);
        }
        break;
    case BURN: {
        const float grow = f->t < GROW_FRAMES ? (float)f->t / GROW_FRAMES : 1.0f;
        flame_show(obj, f, grow * (0.8f + 0.4f * rnd()));
        if (f->t % 10 == 5) {  /* smoke off its tip */
            const float at[3] = {f->home[0] + 0.6f * (rnd() - 0.5f), f->home[1] + 0.6f * (rnd() - 0.5f), f->home[2] + 3.2f * grow};
            const float up[3] = {0.0f, 0.0f, 2.0f + rnd()};
            api->smoke(at, up, 1.6f);
        }
        if (f->t % 30 == 0 && !api->sfx_active(f->roar, snd.roar)) f->roar = api->sfx_play(snd.roar, m, NULL);
        if (f->hurt > 0) --f->hurt;
        if (grow > 0.5f && f->hurt == 0) {
            int any = 0;
            if (hero_in(f)) {
                api->hero_damage(1, m);
                any = 1;
            }
            const int n = hit_enemies(f->home, REACH, 20.0f, m);  /* (the enemies in it burn too) */
            if (any || n > 0) f->hurt = HURT_EVERY;
        }
        if (f->t >= BURN_FRAMES) {
            f->phase = DOWN;
            f->t = 0;
            flame_quiet(f);
        }
        break;
    }
    default:
        flame_show(obj, f, 1.0f - (float)f->t / DOWN_FRAMES);
        if (f->t >= DOWN_FRAMES) {
            f->phase = REST;
            f->t = 0;
        }
        break;
    }
}

/* each frame (from frame: lights are added there), the vents' light: a small flicker while one warns, a strong one,
   flickering, while it burns (light_add: this frame only) */
static void flames_light(void) {
    static const float fire[3] = {1.0f, 0.55f, 0.18f};
    for (int i = 0; i < nflames; ++i) {
        const flame* f = (const flame*)api->object_state(flames[i]);
        if (f == NULL) continue;
        float at[3] = {f->home[0], f->home[1], f->home[2] + 1.0f};
        if (f->phase == WARN) {
            api->light_add(at, fire, 0.8f + 0.5f * rnd(), 5.0f);
        } else if (f->phase == BURN) {
            const float grow = f->t < GROW_FRAMES ? (float)f->t / GROW_FRAMES : 1.0f;
            at[2] += 1.0f;
            api->light_add(at, fire, grow * (2.6f + 1.4f * rnd()), 13.0f);
        } else if (f->phase == DOWN) {
            api->light_add(at, fire, 1.5f * (1.0f - (float)f->t / DOWN_FRAMES), 10.0f);
        }
    }
}

static void spawn_spawn(void* user, uint32_t obj, const rac1_spawn* sp) {
    (void)user, (void)sp;
    api->object_show(obj, 0);
}

RAC1_MOD_EXPORT int rac1_mod_main(const rac1_api* game, rac1_mod* mod) {
    api = game;
    mod->size = sizeof(rac1_mod);
    s.t_center = s.t_pad = -1;
    s.snd_wave = s.snd_win = s.snd_fail = -1;
    if (!RAC1_API_HAS(api, base_option)) {
        api->log("this game is older than the SDK's ABI 5: the arena's plugin needs a newer one");
        return 0;
    }
    mod->level_start = level_start;
    mod->level_end = level_end;
    mod->frame = frame;
    mod->hud = hud;
    mod->save = save;
    mod->load = load;
    mod->new_game = new_game;
    mod->test = test;
    rac1_class_def d = {sizeof d};
    d.name = "flame";
    d.state_size = sizeof(flame);
    d.spawn = flame_spawn;
    d.update = flame_update;
    d.remove = flame_remove;
    api->class_define(&d);
    memset(&d, 0, sizeof d);
    d.size = sizeof d;
    d.name = "spawn";
    d.spawn = spawn_spawn;
    api->class_define(&d);
    api->log("the arena's plugin is on: the menu, the challenges, Kalebo III's enemies and their paths, the weapons, the flame vents");
    return 0;
}
