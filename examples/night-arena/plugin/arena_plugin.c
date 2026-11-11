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
 * base_hunt, light_add); the rail challenges ABI 11 (the rails).
 * - The supplies: four places on the hub's top; during a challenge, every so often (the challenge's `supplies`
 *   seconds, give or take a quarter), two nanotech or two ammo crates come into the free ones (a place is free once its
 *   crate is broken: never one crate into another). Two nanotech crates there at the level's start.
 * - flame (the level's class): a flame vent in the floor: rests, warns (smoke, a hiss), bursts (a roar), hurts
 *   Ratchet standing in it; the vents take turns ("delay=" in frames).
 * - spawn (the level's class): the spawn points, hidden at their spawn.
 * - The rails (level.txt's rail lines, the classes rail_<name>): a rail challenge (for those who have the Grindscarponi)
 *   puts Ratchet on the station outside the walls and turns its rails on: "Grindscarponi: Discesa" a course of three
 *   lanes with mines (ours, the class mine, to jump; Kalebo III's rail mines, class 933, to break), then a last wave in
 *   the arena (the grind section below); "La grande fuga" the same course, two rounds in the arena, the lava flooding
 *   it, an escape rail through a portal into game planets' worlds (sections): Batalia's rail to its jump, then the
 *   Snagglebeast's cave on Umbris (its boss beaten wins).
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
enum { K_ROBOT, K_BOMBER, K_LAUNCHER, K_CLUB, K_FLAMER, K_JET, K_SHIP, K_GUNSHIP, K_GEMLIK, K_GASPAR, K_AMOEBOID, K_BLARG, K_TURRET,
       K_SPORE, N_KINDS };

static const struct kind_def {
    char letter;   /* in a wave's mix */
    int32_t cls;   /* the class (level.txt's spawn line) */
    int weapon;    /* a Gadgetron robot's (base_option RAC1_BASE_WEAPON), -1 none */
    int moves;     /* 0 walks (the paths), 1 flies (its own flight: the game keeps it out of the walls), 2 a dropship,
                      3 stays where it is put (a turret on a tower), 4 a hazard, not an enemy (a plant by a gate: it cannot
                      be killed; put at the wave's start, not counted) */
    float step;    /* units a frame along a path (measured: their own pace) */
    const char* import;  /* another planet's (import_spawn; its class loaded for the waves that have it), NULL Kalebo III's */
} KIND[N_KINDS] = {
    {'R', 1445, -1, 0, 0.07f},  /* the robots */
    {'B', 52, -1, 0, 0.12f},    /* the bomb bots: roll at Ratchet and blow up */
    {'L', 541, 0, 0, 0.08f},    /* a Gadgetron robot with the bomb launcher (keeps 9 units off) */
    {'C', 541, 1, 0, 0.10f},    /* with the club */
    {'F', 541, 2, 0, 0.085f},   /* with the flamethrower */
    {'J', 638, -1, 1, 0.0f},    /* a jetpack trooper, starting on a drop zone */
    {'S', 1356, -1, 2, 0.0f},   /* a dropship: lands on a drop zone, drops 3 troopers, flies off */
    {'G', 1356, -1, 2, 0.0f},   /* the gunship: drops 2 troopers and stays to fight (counted, as they are) */
    /* other planets' (data/pool/names.txt): they walk or fly straight at Ratchet (no navigation of ours); their areas the
       whole arena (path_create) */
    {'Y', 63, -1, 1, 0.0f, "gemlik.flying_robot"},     /* Gemlik's flying robot */
    {'W', 193, -1, 0, 0.0f, "gaspar.walking_robot"},   /* Gaspar's walking robot */
    {'A', 572, -1, 0, 0.0f, "pokitaru.amoeboid"},      /* Pokitaru's amoeboid */
    {'K', 1382, -1, 0, 0.0f, "fleet.blarg_trooper"},   /* Drek's fleet's Blarg trooper */
    {'T', 1041, -1, 4, 0.0f, "umbris.turret"},         /* Umbris's turret: on a tower's top or a pillar (a hazard: it can be
                                                          destroyed, but the round does not wait for it) */
    {'P', 939, -1, 4, 0.0f, "orxon.spore_plant"},      /* Orxon's spore plant: by a gate, by the hub (it cannot be killed) */
};
/* where the ones that stay are put (from the arena's centre; z over its floor): the towers' tops and the pillars' (the
   turrets), by the gates and the hub's corners (the plants) */
static const float TURRET_SPOTS[][3] = {{42, 42, 6}, {-42, 42, 6}, {42, -42, 6}, {-42, -42, 6},
                                        {29, 29, 5}, {-29, 29, 5}, {29, -29, 5}, {-29, -29, 5}};
static const float SPORE_SPOTS[][3] = {{0, 46, 0}, {46, 0, 0}, {-46, 0, 0}, {11, 11, 0}, {-11, 11, 0}, {11, -11, 0}, {-11, -11, 0}};

/* ---- the challenges ---- */
typedef struct wave_def {
    int count;        /* enemies in the wave (a dropship's troopers each one, the gunship one) */
    int at_once;      /* alive at once, at most */
    int every;        /* frames between two arrivals */
    const char* mix;  /* the kinds' letters, one taken at random for each arrival ("RRB": two robots to a bomb bot);
                         a G first: the gunship opens the wave (once) */
} wave_def;

enum { WAVES, SURVIVE, TIMED, CUP, RAIL, BUMP };
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
    wave_def wave[10];
    int bombs;         /* frames between two bombs from the sky, about (0: none) */
    int supplies;      /* seconds between two deliveries of crates on the hub (0: none; the wrench only: nanotech only) */
    int vents;         /* 1: the flame vents burn three times as often */
    int nohit;         /* 1: one hit loses it */
    int weapons;       /* W_ANY; W_WRENCH the wrench only; W_ROULETTE a random weapon each wave; W_CHAOS a new one
                          every CHAOS_FRAMES */
    int reward;        /* bolts */
    int hidden;        /* 1: in the menu once every other challenge is won */
    const char* rails; /* RAIL: its rails (level.txt's names, the first the one whose end finishes it) */
    const char* start; /* RAIL: the spawn point Ratchet starts from */
    int obstacles;     /* RAIL: the course's obstacles (our mines, Kalebo III's rail mines) on */
    const char* escape; /* RAIL: after the arena's rounds (its waves) the lava floods the arena and this rail, from the
                           hub's top, leads to a portal into the sections of `section` ("La grande fuga") */
    const char* section; /* RAIL with an escape: the sections (level.txt's: game planets' worlds) beyond its portal, in
                            order, each reached through a flash of white; what each asks is CHAIN's */
    int escape_after;    /* WAVES with an escape: the lava after this many waves; the last world's portal leads back to the
                            arena (no lava), where the waves go on ("Assalto Finale"). 0: the escape after the last wave (RAIL) */
    int target;          /* BUMP: the enemies to knock down (on Kalebo III's hoverboard, ramming them) within the time */
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
    {"Grindscarponi: Discesa", "3 corsie di mine, poi l'ultima ondata", RAIL, 90, 1, {{8, 6, 25, "RRB"}}, 0, 20,
     0, 0, W_ANY, 3000, 0, "lane_c lane_l lane_r", "rail_course", 1},
    {"La grande fuga", "lava, Quartu, Batalia, Rilgar, Snagglebeast", RAIL, 90, 2, {{10, 7, 30, "RRB"}, {12, 8, 28, "RRBF"}},
     0, 25, 0, 0, W_ANY, 10000, 0, "lane_c lane_l lane_r", "rail_course", 1, "escape", "quartu batalia rilgar umbris"},
    /* Assalto Finale: other planets' enemies with Kalebo III's, more each round; after the fifth the lava, the escape, Kalebo
       III's own long rail in its world, back to the arena; the tenth an invasion, as many as the game takes */
    {"Assalto Finale", "10 round, lava, Kalebo III, invasione", WAVES, 0, 10,
     {{14, 10, 30, "RRBWA"}, {18, 12, 26, "RBWAY"}, {22, 14, 24, "RBFWAYP"}, {26, 16, 22, "RBFLWAYP"}, {30, 18, 20, "RBFLCJWAYP"},
      {32, 20, 20, "RBKTFJ"}, {36, 22, 18, "RBKTFLJS"}, {40, 24, 16, "RBKTFLCJS"}, {44, 26, 14, "GRBKTFLCJS"},
      {200, 70, 4, "GRBFLCJSKYWA"}},  /* (110 at once crashed the game once in headless: richiesta_invasione.md) */
     0, 20, 1, 0, W_ANY, 30000, 0, NULL, NULL, 0, "escape", "batalia", 5},  /* ("kalebo" once the composer takes the base's
                                                                                own world as a section: CHAIN has it) */
    /* Autoscontro: on Kalebo III's hoverboard (free, ABI 16) round the arena, the enemies knocked down by ramming them (its
       wave without end: walkers and bomb bots), 60 within 2 minutes; rings to ride through, 5 seconds more each */
    {"Autoscontro", "in tavola: investi 60 nemici in 2 minuti", BUMP, 120, 1, {{9999, 16, 30, "RRRWAB"}}, 0, 0, 0, 0, W_ANY, 15000, 0,
     NULL, NULL, 0, NULL, NULL, 0, 60},
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
#define MAX_EN 128  /* enemies at once (level.txt's spawn lines: 24 + 8 + 6 + 8 + 2; and other planets', no pool) */

typedef struct enemy {
    uint32_t h;      /* base_spawn's handle (0: another planet's, `m`) */
    uint32_t m;      /* (another planet's) its moby, while it is of its class and in play */
    int spot;        /* (one that stays) its spot's index */
    int kind;
    uint32_t ship;   /* a trooper: the dropship that brought it (0 none) */
    float to[2];     /* where it walks: a point along the path, or Ratchet */
    int direct;      /* Ratchet in sight: straight at him (its own code) */
    float prev[3];   /* where it stood after the last frame's steering */
    int has_prev;
    float vz;        /* falling: the plugin's gravity (their own code does not drop them off a ledge) */
} enemy;

/* an enemy's moby while it is in play, else 0 (Kalebo III's: base_moby; another planet's: its moby still of its class and in
   play - gone once killed, its moby given to something else later) */
static uint32_t en_moby(const enemy* e);

enum { FREE, MENU, INTRO, FIGHT, BREAK, RESULT };

#define INTRO_FRAMES 180
#define BREAK_FRAMES 150
#define RESULT_FRAMES 300

static struct {
    int mine;
    int level;    /* the game's level number now (a level mod's: its base level's) */
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
    int save_ask;     /* a challenge won: frames left to save the game in (ABI 15's game_save, once back in the arena) */
    int saving;       /* the save being written (game_saving) */
    int lost_reload;  /* a challenge on when the level ended (Ratchet lost in the lava: the game loads it again) */
    char why[32];  /* the lost banner's second line */
    float last_h[3];  /* Ratchet's place the frame before: a jump of it is a death (he comes back at his start) */
    int have_last;
    int placed;       /* frames the plugin's own move of him (hero_move) is not taken for a death */
    int test_log;     /* (a test running: the obstacles' events logged) */
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
    int banner_y;  /* this frame's banner's top (the toast goes over it), 0 none */
    int box_rect;  /* this frame's challenge box (its panel's index), -1 none */
    int boss_hud;  /* BOSS_BAR_H while a boss's bar is at the bottom (the HUD's row over it), else 0 */
    int in_msg;    /* the panels and texts made now are a message of the bottom row (moved off the box) */
    /* the HUD's texts and panels, made in frame (hud may run twice a frame: it only draws them) */
    int nlines, nrects;
    struct {
        int16_t x, y;
        uint32_t colour;
        float scale;
        uint8_t anchor, centred;  /* RAC1_HUD_*; centred on x */
        uint8_t msg;              /* a message's (in_msg) */
        int16_t maxw;  /* its width at most (made smaller to fit), 0 any */
        char text[96];
    } line[15];  /* (the game draws 15 texts a frame at most) */
    struct {
        int16_t x, y, w, h;
        uint32_t colour;
        uint8_t anchor, msg;
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
    s.line[s.nlines].msg = (uint8_t)s.in_msg;
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
    s.rect[s.nrects].msg = (uint8_t)s.in_msg;
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

/* the HUD's bottom row (Daino: nothing in the middle of the screen, nor over the quick select's ring at the left): the
   challenge's box at the left, the banners and the toasts in the middle, all ending this far down */
#define BOSS_BAR_H 46  /* the game's own row at the bottom (the Snagglebeast's health bar, Rilgar's race: lap, time, place),
                          under the box and the messages */
#define HUD_BOTTOM (RAC1_HUD_HEIGHT - 10 - s.boss_hud)
#define TOAST_H 26
#define MSG_W 440  /* the banners' and toasts' texts at most this wide (smaller past it; hud moves them off the box) */

/* a banner at the bottom of the screen, in the middle: a panel, a large line, a smaller one under it (or none) */
static void banner(uint32_t colour, const char* big, const char* small) {
    const int h = small ? 66 : 44, y = HUD_BOTTOM - h;
    s.in_msg = 1;
    hud_panel(116, y, 280, h, PANEL, RAC1_HUD_CENTRE);
    hud_panel(116, y, 280, 3, BAR, RAC1_HUD_CENTRE);
    hud_text_at(256, y + 10, colour, 1.6f, RAC1_HUD_CENTRE, 1, "%s", big);
    hud_maxw(MSG_W);
    if (small) hud_text_at(256, y + 38, WHITE, 1.2f, RAC1_HUD_CENTRE, 1, "%s", small);
    if (small) hud_maxw(MSG_W);
    hud_fit(2, small ? 2 : 1);
    s.in_msg = 0;
    s.banner_y = y;
}

static void toast(const char* text) {
    snprintf(s.toast, sizeof s.toast, "%s", text);
    s.toast_t = 180;
}

/* ---- the texts in the game's language (its text language 0x15ed88: 0 English, 1 Japanese, 2 French, 3 German,
   4 Spanish, 5 Italian; Japanese gets the English ones). The plugin's texts are written in Italian (the challenges'
   names are their keys in the save too); tr() gives one in the game's language, the Italian one when the table has
   none. Plain ASCII, as the HUD's font takes it (no accents: e', ae / oe / ue). A row: Italian, English, French,
   German, Spanish, the same % conversions in each (texts_check logs a row that has not). The weapons', the wrench's
   and the Grindscarponi's names are the game's own (item_name, below), and so are the words for bolts. ---- */
#define GAME_LANGUAGE 0x0015ed88u
enum { L_IT, L_EN, L_FR, L_DE, L_ES, N_LANGS };

static const char* const TEXTS[][N_LANGS] = {
    /* the challenges, their lines */
    {"Primo sangue", "First Blood", "Premier sang", "Erstes Blut", "Primera sangre"},
    {"5 ondate di robot", "5 waves of robots", "5 vagues de robots", "5 Wellen Roboter", "5 oleadas de robots"},
    {"Manichini", "Dummies", "Mannequins", "Dummys", "Maniquies"},
    {"i robot Gadgetron armati", "Gadgetron's armed robots", "les robots Gadgetron armes", "Gadgetrons bewaffnete Roboter",
     "los robots Gadgetron armados"},
    {"Sbarco", "Landing", "Debarquement", "Landung", "Desembarco"},
    {"navi e soldati col jetpack", "dropships and jetpack troopers", "vaisseaux et soldats a jetpack",
     "Landungsschiffe und Jetpack-Soldaten", "naves y soldados con jetpack"},
    {"Assedio", "Siege", "Siege", "Belagerung", "Asedio"},
    {"5 ondate, grate impazzite", "5 waves, the vents gone wild", "5 vagues, grilles dechainees", "5 Wellen, wilde Gitter",
     "5 oleadas, rejillas enloquecidas"},
    {"Pioggia di fuoco", "Rain of Fire", "Pluie de feu", "Feuerregen", "Lluvia de fuego"},
    {"5 ondate sotto le bombe", "5 waves under the bombs", "5 vagues sous les bombes", "5 Wellen unter Bomben",
     "5 oleadas bajo las bombas"},
    {"Solo chiave", "Wrench Only", "Cle seule", "Nur Schluessel", "Solo llave"},
    {"5 ondate, solo la chiave inglese", "5 waves, the wrench only", "5 vagues, la cle seulement",
     "5 Wellen, nur der Schraubenschluessel", "5 oleadas, solo la llave inglesa"},
    {"Roulette", "Roulette", "Roulette", "Roulette", "Ruleta"},
    {"un'arma a caso a ogni ondata", "a random weapon each wave", "une arme au hasard a chaque vague",
     "jede Welle eine Zufallswaffe", "un arma al azar en cada oleada"},
    {"Contro il tempo", "Against the Clock", "Contre la montre", "Gegen die Zeit", "Contrarreloj"},
    {"5 ondate in 3 minuti", "5 waves in 3 minutes", "5 vagues en 3 minutes", "5 Wellen in 3 Minuten", "5 oleadas en 3 minutos"},
    {"Intoccabile", "Untouchable", "Intouchable", "Unberuehrbar", "Intocable"},
    {"3 ondate senza un graffio", "3 waves without a scratch", "3 vagues sans une egratignure", "3 Wellen ohne einen Kratzer",
     "3 oleadas sin un rasguno"},
    {"Caos", "Chaos", "Chaos", "Chaos", "Caos"},
    {"4 minuti, l'arma cambia ogni 20 s", "4 minutes, a new weapon every 20 s", "4 minutes, l'arme change toutes les 20 s",
     "4 Minuten, alle 20 s eine neue Waffe", "4 minutos, el arma cambia cada 20 s"},
    {"Ultimo in piedi", "Last One Standing", "Le dernier debout", "Der Letzte steht", "El ultimo en pie"},
    {"resisti 2 minuti e mezzo", "hold out for two and a half minutes", "tiens deux minutes et demie",
     "halte zweieinhalb Minuten durch", "resiste dos minutos y medio"},
    {"Inferno", "Inferno", "Enfer", "Inferno", "Infierno"},
    {"8 ondate, bombe, grate, cannoniera", "8 waves, bombs, vents, a gunship", "8 vagues, bombes, grilles, canonniere",
     "8 Wellen, Bomben, Gitter, Kanonenboot", "8 oleadas, bombas, rejillas, canonera"},
    {"Campionato", "Championship", "Championnat", "Meisterschaft", "Campeonato"},
    {"15 round, le modalita' a caso", "15 rounds, the modes at random", "15 manches, les modes au hasard",
     "15 Runden, die Modi per Zufall", "15 rondas, los modos al azar"},
    {"Grindscarponi: Discesa", "%s: Descent", "%s: Descente", "%s: Abfahrt", "%s: Descenso"},
    {"3 corsie di mine, poi l'ultima ondata", "3 lanes of mines, then a last wave", "3 voies de mines, puis une derniere vague",
     "3 Spuren voller Minen, dann eine letzte Welle", "3 carriles de minas, y una ultima oleada"},
    {"Impossibile", "Impossible", "Impossible", "Unmoeglich", "Imposible"},
    {"il campionato, 50 round", "the championship, 50 rounds", "le championnat, 50 manches", "die Meisterschaft, 50 Runden",
     "el campeonato, 50 rondas"},
    /* the championships' modes (the ones not above) */
    {"Libero", "Free", "Libre", "Frei", "Libre"},
    {"Grate impazzite", "Wild Vents", "Grilles dechainees", "Wilde Gitter", "Rejillas locas"},
    /* the menu */
    {"ARENA - SFIDE", "ARENA - CHALLENGES", "ARENE - DEFIS", "ARENA - HERAUSFORDERUNGEN", "ARENA - DESAFIOS"},
    {"Servono i Grindscarponi", "You need the %s", "Il faut les %s", "Du brauchst die %s", "Necesitas las %s"},
    {"Premio %d bolts   Record %u:%02u", "Prize %d bolts   Record %u:%02u", "Prix %d boulons   Record %u:%02u",
     "Preis %d Bolts   Rekord %u:%02u", "Premio %d guitones   Record %u:%02u"},
    {"Premio %d bolts", "Prize %d bolts", "Prix %d boulons", "Preis %d Bolts", "Premio %d guitones"},
    {"Croce: inizia      Cerchio: esci", "Cross: start      Circle: leave", "Croix: commencer      Rond: quitter",
     "Kreuz: starten      Kreis: verlassen", "Equis: empezar      Circulo: salir"},
    {"Quadrato: rivenditore", "Square: vendor", "Carre: vendeur", "Quadrat: Haendler", "Cuadrado: vendedor"},
    {"Triangolo: menu arena", "Triangle: arena menu", "Triangle: menu de l'arene", "Dreieck: Arena-Menue",
     "Triangulo: menu de la arena"},
    {"Il rivenditore non si apre ora", "The vendor won't open now", "Le vendeur ne s'ouvre pas maintenant",
     "Der Haendler oeffnet jetzt nicht", "El vendedor no se abre ahora"},
    {"Servono almeno 2 armi", "You need at least 2 weapons", "Il faut au moins 2 armes", "Du brauchst mindestens 2 Waffen",
     "Necesitas al menos 2 armas"},
    /* a challenge running */
    {"Si comincia tra %d", "Starting in %d", "Debut dans %d", "Start in %d", "Empieza en %d"},
    {"Prossimo: %s", "Next: %s", "Ensuite: %s", "Als Naechstes: %s", "Siguiente: %s"},
    {"Round superato!", "Round cleared!", "Manche reussie!", "Runde geschafft!", "Ronda superada!"},
    {"Ondata superata!", "Wave cleared!", "Vague repoussee!", "Welle geschafft!", "Oleada superada!"},
    {"Arma: %s", "Weapon: %s", "Arme: %s", "Waffe: %s", "Arma: %s"},
    {"Nessuna arma: solo la chiave", "No weapons: the wrench only", "Aucune arme: la cle seulement",
     "Keine Waffen: nur der Schluessel", "Sin armas: solo la llave"},
    {"Resisti %d:%02d   Abbattuti %d", "Hold out %d:%02d   Downed %d", "Tiens %d:%02d   Abattus %d",
     "Durchhalten %d:%02d   Erledigt %d", "Resiste %d:%02d   Abatidos %d"},
    {"Restano %d:%02d", "Time left %d:%02d", "Reste %d:%02d", "Noch %d:%02d", "Quedan %d:%02d"},
    {"Nemici %d   %d:%02d", "Enemies %d   %d:%02d", "Ennemis %d   %d:%02d", "Gegner %d   %d:%02d", "Enemigos %d   %d:%02d"},
    {"Nemici %d   restano %d:%02d", "Enemies %d   left %d:%02d", "Ennemis %d   reste %d:%02d", "Gegner %d   noch %d:%02d",
     "Enemigos %d   quedan %d:%02d"},
    {"Mine %d/%d   Cadute %d", "Mines %d/%d   Falls %d", "Mines %d/%d   Chutes %d", "Minen %d/%d   Stuerze %d",
     "Minas %d/%d   Caidas %d"},
    {"Cadute: %d", "Falls: %d", "Chutes: %d", "Stuerze: %d", "Caidas: %d"},
    {"Zone elettrificate: stai lontano dai cerchi!", "Electrified zones: keep off the rings!",
     "Zones electrifiees: evite les cercles!", "Landezonen unter Strom: weg von den Ringen!",
     "Zonas electrificadas: alejate de los circulos!"},
    /* the rails */
    {"Nessuna rotaia", "No rails", "Aucun rail", "Keine Schienen", "Sin railes"},
    {"Salta sulla rotaia!", "Jump on the rail!", "Saute sur le rail!", "Spring auf die Schiene!", "Salta al rail!"},
    {"La grande fuga", "The Great Escape", "La Grande Evasion", "Die grosse Flucht", "La gran fuga"},
    {"lava, Quartu, Batalia, Rilgar, Snagglebeast", "lava, Quartu, Batalia, Rilgar, Snagglebeast",
     "lave, Quartu, Batalia, Rilgar, Snagglebeast", "Lava, Quartu, Batalia, Rilgar, Snagglebeast",
     "lava, Quartu, Batalia, Rilgar, Snagglebeast"},
    {"Rilgar: salta dalla rampa grande sulla piattaforma", "Rilgar: big ramp, jump onto the platform",
     "Rilgar: grand tremplin, saute sur la plateforme", "Rilgar: grosse Rampe, Sprung auf die Plattform",
     "Rilgar: rampa grande, salta a la plataforma"},
    {"La lava sale! Sali al centro e prendi la rotaia", "The lava is rising! Up the centre, onto the rail",
     "La lave monte! Monte au centre, prends le rail", "Die Lava steigt! Hoch zur Mitte, auf die Schiene",
     "La lava sube! Sube al centro y toma el rail"},
    {"Fuga: restano %d:%02d", "Escape: %d:%02d left", "Fuite: encore %d:%02d", "Flucht: noch %d:%02d", "Huida: quedan %d:%02d"},
    {"Caduto! Di nuovo dal centro", "You fell! Back to the centre", "Tombe! Retour au centre", "Gefallen! Zurueck zur Mitte",
     "Caiste! De vuelta al centro"},
    {"Mondo %d/%d   %d:%02d", "World %d/%d   %d:%02d", "Monde %d/%d   %d:%02d", "Welt %d/%d   %d:%02d",
     "Mundo %d/%d   %d:%02d"},
    {"Batalia: fino in fondo alla rotaia", "Batalia: ride the rail to its end", "Batalia: le rail jusqu'au bout",
     "Batalia: die Schiene bis ans Ende", "Batalia: el rail hasta el final"},
    {"Umbris: sconfiggi lo Snagglebeast", "Umbris: defeat the Snagglebeast", "Umbris: bats le Snagglebeast",
     "Umbris: besiege das Snagglebeast", "Umbris: derrota al Snagglebeast"},
    {"Quartu: in tavola fino al centro dell'arena", "Quartu: board to the arena's middle", "Quartu: en planche au centre de l'arene",
     "Quartu: mit dem Board zur Arenamitte", "Quartu: en tabla al centro de la arena"},
    {"Caduto! Di nuovo dalla stazione", "You fell! Back to the station", "Tombe! Retour a la station",
     "Gestuerzt! Zurueck zur Station", "Caiste! De vuelta a la estacion"},
    /* the result */
    {"SFIDA COMPLETATA!", "CHALLENGE COMPLETE!", "DEFI REUSSI!", "HERAUSFORDERUNG GESCHAFFT!", "DESAFIO COMPLETADO!"},
    {"SFIDA FALLITA", "CHALLENGE FAILED", "DEFI RATE", "HERAUSFORDERUNG VERLOREN", "DESAFIO FALLIDO"},
    {"+%d bolts   %d abbattuti", "+%d bolts   %d downed", "+%d boulons   %d abattus", "+%d Bolts   %d erledigt",
     "+%d guitones   %d abatidos"},
    {"+%d bolts   %d:%02d", "+%d bolts   %d:%02d", "+%d boulons   %d:%02d", "+%d Bolts   %d:%02d", "+%d guitones   %d:%02d"},
    {"Ti hanno colpito", "You were hit", "Tu as ete touche", "Du wurdest getroffen", "Te han dado"},
    {"Tempo scaduto", "Time's up", "Temps ecoule", "Zeit abgelaufen", "Se acabo el tiempo"},
    {"Ratchet e' caduto", "Ratchet fell", "Ratchet est tombe", "Ratchet ist gefallen", "Ratchet ha caido"},
    {"Sbloccata: %s", "Unlocked: %s", "Debloque: %s", "Freigeschaltet: %s", "Desbloqueado: %s"},
    {"Assalto Finale", "Final Assault", "Assaut final", "Letzter Ansturm", "Asalto final"},
    {"10 round, lava, Kalebo III, invasione", "10 rounds, lava, Kalebo III, invasion", "10 manches, lave, Kalebo III, invasion",
     "10 Runden, Lava, Kalebo III, Invasion", "10 rondas, lava, Kalebo III, invasion"},
    {"Kalebo III: fino al portale in fondo alla rotaia", "Kalebo III: ride to the portal at the rail's end",
     "Kalebo III: jusqu'au portail au bout du rail", "Kalebo III: bis zum Portal am Schienenende",
     "Kalebo III: hasta el portal al final del rail"},
    {"Di nuovo nell'arena: si continua!", "Back in the arena: keep going!", "De retour dans l'arene: on continue!",
     "Zurueck in der Arena: weiter!", "De vuelta en la arena: sigue!"},
    {"INVASIONE TOTALE!", "TOTAL INVASION!", "INVASION TOTALE!", "TOTALE INVASION!", "INVASION TOTAL!"},
    {"Autoscontro", "Bumper Boards", "Auto-tamponneuses", "Autoscooter", "Autos de choque"},
    {"in tavola: investi 60 nemici in 2 minuti", "on the board: ram 60 enemies in 2 minutes",
     "en planche: percute 60 ennemis en 2 minutes", "auf dem Board: ramm 60 Gegner in 2 Minuten",
     "en tabla: embiste a 60 enemigos en 2 minutos"},
    {"Investiti %d/%d   %d:%02d", "Rammed %d/%d   %d:%02d", "Percutes %d/%d   %d:%02d", "Gerammt %d/%d   %d:%02d",
     "Embestidos %d/%d   %d:%02d"},
    {"Investili con la tavola! Gli anelli danno tempo", "Ram them with the board! The rings give time",
     "Percute-les avec la planche! Les anneaux donnent du temps", "Ramm sie mit dem Board! Die Ringe geben Zeit",
     "Embistelos con la tabla! Los anillos dan tiempo"},
    {"+5 secondi", "+5 seconds", "+5 secondes", "+5 Sekunden", "+5 segundos"},
    {"Salvataggio...", "Saving...", "Sauvegarde...", "Speichern...", "Guardando..."},
};
#define N_TEXTS ((int)(sizeof TEXTS / sizeof TEXTS[0]))

/* The game's own names (its texts, in its language): the level's text table (at Veldin's overlay address 0x15f660, its
   count at 0x17943c; 16 bytes a text: +0 the string, +4 its id, as the game's lookup 0x203ad8 reads it), an item's name
   in the item table (0x179ac0, 0x4c bytes an item: +0x46 its name's id, +0x48 its gold version's, the gold one owned
   when 0x13e520 + the item is set). In plain ASCII for the HUD: the font's accented vowels (0x80..0xa7, four a vowel:
   A a E e I i O o U u, each acute, grave, circumflex, diaeresis) as the vowel (in German a diaeresis as ae, oe, ue),
   0xa8 / 0xa9 / 0xaa / 0xab / 0xac (N n C c with their marks, the sharp s) as N n C c ss, its colour and button codes
   left out, and the space after a hyphen (where the quick select breaks a name in two lines) too. */
#define TEXT_LIST 0x0015f660u
#define TEXT_COUNT 0x0017943cu
#define ITEM_TABLE 0x00179ac0u
#define GOLD_ITEMS 0x0013e520u
#define BOOTS_ITEM 0x1d  /* the Grindscarponi's item (owned: GRIND_BOOTS, 0x13d4c0 + it) */

/* the game's text `id` in ASCII into out (empty when the level has none) */
static int lang_now(void);

static const char* game_text(int32_t id, char* out, size_t cap) {
    out[0] = 0;
    const int german = lang_now() == L_DE;
    const uint32_t list = rac1_lw(api, api->ov(TEXT_LIST));
    const int32_t n = (int32_t)rac1_lw(api, api->ov(TEXT_COUNT));
    uint32_t str = 0;
    for (int32_t i = 0; i < n && list != 0 && str == 0; ++i)
        if ((int32_t)rac1_lw(api, list + 16u * (uint32_t)i + 4) == id) str = rac1_lw(api, list + 16u * (uint32_t)i);
    size_t k = 0;
    for (uint32_t j = 0; str != 0 && j < 96; ++j) {
        const uint8_t c = rac1_lb(api, str + j);
        if (c == 0) break;
        const char* add = NULL;
        char one[3] = {0, 0, 0};
        if (c == ' ' && k > 0 && out[k - 1] == '-') continue;
        if (c >= 0x20 && c < 0x7f) one[0] = (char)c;
        else if (c >= 0x80 && c <= 0xa7) {
            one[0] = "AaEeIiOoUu"[(c - 0x80) / 4];
            if (german && (c - 0x80) % 4 == 3 && strchr("AaOoUu", one[0])) one[1] = 'e';
        }
        else if (c >= 0xa8 && c <= 0xab) one[0] = "NnCc"[c - 0xa8];
        else if (c == 0xac) add = "ss";
        if (one[0]) add = one;
        for (; add != NULL && *add && k + 1 < cap; ++add) out[k++] = *add;
    }
    out[k] = 0;
    return out;
}

/* an item's name as the game shows it (its gold version's when that is owned), "" when the level's texts have none */
static const char* item_name(int32_t item, char* out, size_t cap) {
    const uint32_t rec = api->ov(ITEM_TABLE) + 0x4cu * (uint32_t)item;
    const int gold = item > 0 && item < 0x40 && rac1_lb(api, GOLD_ITEMS + (uint32_t)item) != 0;
    return game_text((int16_t)rac1_lh(api, rec + (gold ? 0x48 : 0x46)), out, cap);
}

static const char* tr(const char* it);

/* an Italian text naming the Grindscarponi, in the game's language: the other languages' rows hold a %s for the game's
   own name of them */
static const char* with_boots(const char* it) {
    static char buf[2][96];
    static int k;
    char* out = buf[k++ & 1];
    const char* t = tr(it);
    if (strstr(t, "%s") == NULL) return t;
    char name[48];
    if (!item_name(BOOTS_ITEM, name, sizeof name)[0]) snprintf(name, sizeof name, "Grindboots");
    snprintf(out, sizeof buf[0], t, name);
    return out;
}

/* a challenge's name in the game's language */
static const char* ch_title(int ch) {
    return with_boots(CHALLENGES[ch].name);
}

/* the game's text language as a column of TEXTS */
static int lang_now(void) {
    switch (rac1_lw(api, GAME_LANGUAGE)) {
    case 2: return L_FR;
    case 3: return L_DE;
    case 4: return L_ES;
    case 5: return L_IT;
    default: return L_EN;
    }
}

/* an Italian text in the game's language */
static const char* tr(const char* it) {
    const int l = lang_now();
    if (l == L_IT) return it;
    for (int i = 0; i < N_TEXTS; ++i)
        if (strcmp(TEXTS[i][L_IT], it) == 0) return TEXTS[i][l];
    return it;
}

/* (at the start) a row whose languages' % conversions differ is logged: printf would read the wrong arguments */
static void texts_check(void) {
    for (int i = 0; i < N_TEXTS; ++i) {
        char want[16] = "", got[16];
        for (int l = 0; l < N_LANGS; ++l) {
            int n = 0;
            for (const char* c = TEXTS[i][l]; *c && n < 15; ++c) {
                if (*c != '%') continue;
                ++c;
                if (*c == '%') continue;
                while (*c && strchr("0123456789.-+ #", *c)) ++c;
                if (*c) got[n++] = *c;
            }
            got[n] = 0;
            if (l == L_IT) snprintf(want, sizeof want, "%s", got);
            else if (strcmp(want, got) != 0 && !(strstr(TEXTS[i][L_IT], "Grindscarponi") && strcmp(got, "s") == 0 && !want[0])) {
                /* (the rows naming the Grindscarponi: the other languages' %s is the game's own name of them) */
                char msg[160];
                snprintf(msg, sizeof msg, "texts: \"%s\" has other conversions in language %d", TEXTS[i][L_IT], l);
                api->log(msg);
            }
        }
    }
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
#define MOBY_HIDDEN 0x1u  /* the moby's flags (+0x34): not drawn */

/* an enemy the Suck Cannon took: hidden and held at Ratchet (in the cannon: its ammo until fired, when it dies). It
   counts as felled at once (else a wave would wait for it while it is never fired: the weapon changed, Caos, Roulette);
   its moby is left to the cannon */
static uint32_t en_moby(const enemy* e) {
    if (e->h != 0) return api->base_moby(e->h);
    const int32_t c = KIND[e->kind].import ? api->import_class(KIND[e->kind].import) : -1;
    if (e->m == 0 || c < 0 || (int32_t)rac1_lh(api, e->m + 0xa6) != c || (int8_t)rac1_lb(api, e->m + 0x20) < 0) return 0;
    return e->m;
}

static int sucked(const enemy* e, uint32_t m) {
    if (e->kind == K_JET && rac1_lb(api, m + 0x20) == JET_CARRIED) return 0;  /* (a trooper still in its dropship) */
    if ((rac1_lh(api, m + 0x34) & MOBY_HIDDEN) == 0) return 0;
    float r[3], h[3];
    pos_of(m, r);
    pos_of(api->hero(), h);
    const float dx = r[0] - h[0], dy = r[1] - h[1], dz = r[2] - h[2];
    return dx * dx + dy * dy + dz * dz < 9.0f;
}

static int enemies_alive(void) {
    int n = 0;
    for (int i = 0; i < s.nen;) {
        const enemy* e = &s.en[i];
        uint32_t m = en_moby(e);
        if (m != 0 && e->kind == K_JET && e->ship != 0 && rac1_lb(api, m + 0x20) == JET_CARRIED && api->base_moby(e->ship) == 0) {
            api->moby_remove(m);
            m = 0;
        }
        if (m != 0 && api->hero() != 0 && sucked(e, m)) m = 0;  /* (felled: the cannon keeps it) */
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
        const uint32_t m = en_moby(&s.en[i]);
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
    const int was = s.rule.weapons, was_vents = s.rule.vents;
    s.rule = nr;
    s.frenzy = nr.vents;
    if (nr.vents && !was_vents) toast(tr("Zone elettrificate: stai lontano dai cerchi!"));
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

static void portal_reset(void);
static void board_off(void);
static void guests_sync(const char* mix);
static void escape_prepare(int ch);

static void hazards_place(const char* mix);

static void bump_start(void);
static void bump_update(const float h[3]);
static void bump_end(void);

static void wave_start(void) {
    guests_sync(cur_wave()->mix);  /* (other planets' classes for this wave's mix) */
    hazards_place(cur_wave()->mix);
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
    if (cur_wave()->count >= 100 && CHALLENGES[s.ch].kind != BUMP) toast(tr("INVASIONE TOTALE!"));  /* (Assalto Finale's last) */
    if (CHALLENGES[s.ch].kind == BUMP) bump_start();
    if (CHALLENGES[s.ch].kind == RAIL) rail_go();  /* (the countdown's end: the rails on; not the last wave's start) */
}

/* the level is a session (format 3, ABI 9): a death a soft reset at the current checkpoint, no level load */
static int session(void) {
    return RAC1_API_HAS(api, session_on) && api->session_on();
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
    else if (CHALLENGES[ch].escape != NULL) escape_prepare(ch);  /* (Assalto Finale: the escape mid-challenge) */
}

static int unlocked(int ch);

#define SAVE_TRY 900  /* frames a won challenge's save is tried in (game_save says 0 while it cannot: a switch, ...) */

/* a won challenge saved with the game (ABI 15: the planet's save into the player's slot, the plugin's bytes with it), once
   Ratchet is back in the arena and not moved; "Salvataggio..." while it is written */
static int in_main(void);

static void save_update(void) {
    if (s.saving) {
        if (api->game_saving()) {
            if (s.toast_t < 2) toast(tr("Salvataggio..."));
            if (strcmp(s.toast, tr("Salvataggio...")) == 0) s.toast_t = 30;
        } else {
            s.saving = 0;
        }
    }
    if (s.save_ask <= 0 || api->abi < 15) return;
    --s.save_ask;
    if (s.save_ask % 30 != 0 || !in_main() || s.placed > 0 || s.mode == INTRO || s.mode == FIGHT || s.mode == BREAK) return;
    if (session()) api->checkpoint_set("arena");  /* (a load of it: back south of the hub) */
    if (api->game_save()) {
        s.save_ask = 0;
        s.saving = 1;
        toast(tr("Salvataggio..."));
        api->log("the game saved (a challenge won)");
    }
}

static void challenge_end(int won, const char* why) {
    enemies_clear();
    guests_sync(NULL);  /* (other planets' classes released) */
    bombs_clear();
    if (CHALLENGES[s.ch].kind == RAIL || CHALLENGES[s.ch].escape != NULL) rail_stop();
    board_off();     /* (a board world's: Ratchet himself again) */
    if (CHALLENGES[s.ch].kind == BUMP) bump_end();
    portal_reset();  /* (a section's challenge: its spawn points no longer the next's; the result takes him back) */
    const rules none = {W_ANY, 0, 0, 0};
    rules_set(none);
    s.won = won;
    snprintf(s.why, sizeof s.why, "%s", won ? "" : !strcmp(why, "colpito") ? tr("Ti hanno colpito") : !strcmp(why, "tempo scaduto") ? tr("Tempo scaduto")
                                                 : !strcmp(why, "Ratchet e' caduto") ? tr("Ratchet e' caduto") : "");
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
        s.save_ask = SAVE_TRY;  /* (the won challenge and its record in the player's save: frame(), back in the arena) */
        play(s.snd_win);
        if (locked >= 0 && unlocked(locked)) {
            char t[64];
            snprintf(t, sizeof t, tr("Sbloccata: %s"), ch_title(locked));
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

/* a weapon's name as the game shows it (the table's English one when the level's texts have none) */
static const char* weapon_name(int32_t item) {
    static char buf[2][48];
    static int k;
    char* out = buf[k++ & 1];
    if (item_name(item, out, sizeof buf[0])[0]) return out;
    if (item == WRENCH) return "OmniWrench";
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
        toast(tr("Nessuna arma: solo la chiave"));
        return;
    }
    int i = (int)(rnd() * (float)n) % n;
    if (n > 1 && w[i] == s.wpn) i = (i + 1 + (int)(rnd() * (float)(n - 1)) % (n - 1)) % n;
    s.wpn = w[i];
    api->weapon_allow(&s.wpn, 1);
    api->ammo_fill(s.wpn);
    s.wpn_ask = 1;
    char msg[64];
    snprintf(msg, sizeof msg, tr("Arma: %s"), weapon_name(s.wpn));
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

static int static_spot(int k);

static int kind_ready(int k) {
    if (KIND[k].import != NULL)  /* (its class loaded now; one that stays: a free spot) */
        return api->import_class(KIND[k].import) >= 0 && (KIND[k].moves != 3 || static_spot(k) >= 0);
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

/* ---- other planets' enemies (import_spawn): their classes loaded for the waves that have them (the game keeps 224 classes
   at once: La grande fuga's worlds take most; import_load / import_release, about a millisecond each) ---- */
static uint32_t guests_loaded;  /* the kinds whose class the plugin loaded (bits) */
static uint32_t haz_m[16];      /* the hazards in play (the spore plants, the turrets: not enemies), their kinds and spots */
static int haz_kind[16], haz_spot[16], nhaz;
static int32_t arena_path;      /* the whole arena's floor as a path (path_create): their areas */

static int static_spot(int k) {
    const int n = k == K_TURRET ? (int)(sizeof TURRET_SPOTS / sizeof TURRET_SPOTS[0]) : (int)(sizeof SPORE_SPOTS / sizeof SPORE_SPOTS[0]);
    for (int i = 0; i < n; ++i) {
        int used = 0;
        for (int j = 0; j < s.nen && !used; ++j) used = s.en[j].kind == k && s.en[j].spot == i && en_moby(&s.en[j]) != 0;
        if (!used) return i;
    }
    return -1;
}

/* the classes the wave's mix needs loaded, the others released (their enemies gone first): released before loaded */
static void guests_sync(const char* mix) {
    if (!RAC1_API_HAS(api, import_load)) return;
    for (int pass = 0; pass < 2; ++pass)
        for (int k = 0; k < N_KINDS; ++k) {
            if (KIND[k].import == NULL) continue;
            const int need = mix != NULL && strchr(mix, KIND[k].letter) != NULL, have = (guests_loaded >> k) & 1u;
            if (pass == 0 && have && !need) {
                if (KIND[k].moves == 4)  /* (its hazards: import_release takes them out) */
                    for (int i = 0; i < nhaz;) {
                        if (haz_kind[i] == k) {
                            haz_m[i] = haz_m[--nhaz];
                            haz_kind[i] = haz_kind[nhaz];
                            haz_spot[i] = haz_spot[nhaz];
                        } else {
                            ++i;
                        }
                    }
                for (int i = 0; i < s.nen;) {
                    if (s.en[i].kind == k) {
                        const uint32_t m = en_moby(&s.en[i]);
                        if (m != 0) api->moby_remove(m);
                        s.en[i] = s.en[--s.nen];
                    } else {
                        ++i;
                    }
                }
                api->import_release(KIND[k].import);
                guests_loaded &= ~(1u << k);
            } else if (pass == 1 && need && !have) {
                const int32_t c = api->import_load(KIND[k].import);
                if (c >= 0) guests_loaded |= 1u << k;
                char b[96];
                snprintf(b, sizeof b, "guests: %s %s", KIND[k].import, c >= 0 ? "loaded" : "NOT loaded (the class table?)");
                api->log(b);
            }
        }
}

static float floor_under(uint32_t self, const float p[3]);
static float dist3(const float a[3], const float b[3]);

/* a place on the arena's floor 14 to 24 units from Ratchet, in his sight (another planet's enemies wake for him only near
   them: from the gates they stood idle), 0 none found */
static int guest_near(float out[3]) {
    float h[3];
    pos_of(api->hero(), h);
    for (int t = 0; t < 10; ++t) {
        const float a = rnd() * 6.2832f, d = 14.0f + 10.0f * rnd();
        const float p[3] = {h[0] + d * cosf(a), h[1] + d * sinf(a), nav.floor_z + 1.0f};
        if (fabsf(p[0] - s.centre[0]) > 44.0f || fabsf(p[1] - s.centre[1]) > 44.0f) continue;
        const float z = floor_under(0, p);
        if (isnan(z) || fabsf(z - nav.floor_z) > 0.6f) continue;  /* (the arena's floor, not the hub, a tower, a cover) */
        const float eye[3] = {h[0], h[1], h[2] + 1.0f}, to[3] = {p[0], p[1], z + 1.0f};
        rac1_ray_hit r = {sizeof r};
        if (api->collide_ray(eye, to, api->hero(), 0, &r)) continue;  /* (a wall between) */
        out[0] = p[0], out[1] = p[1], out[2] = z;
        return 1;
    }
    return 0;
}

/* other planets' movers that strayed far from Ratchet (more than 40 units, every 5 s) put back near him */
static void guests_nudge(void) {
    if (s.clock % 300 != 0) return;
    float h[3];
    pos_of(api->hero(), h);
    for (int i = 0; i < s.nen; ++i) {
        const enemy* e = &s.en[i];
        if (KIND[e->kind].import == NULL || KIND[e->kind].moves == 3) continue;
        const uint32_t m = en_moby(e);
        float r[3], p[3];
        if (m == 0) continue;
        pos_of(m, r);
        if (dist3(r, h) < 40.0f || !guest_near(p)) continue;
        if (KIND[e->kind].moves == 1) p[2] += 3.0f;
        else p[2] += 0.5f;
        api->swf(m + 0x10, p[0]);
        api->swf(m + 0x14, p[1]);
        api->swf(m + 0x18, p[2]);
        api->moby_moved(m);
    }
}

/* one of another planet's: a walker or a flyer near Ratchet (else at a gate, a drop zone), a turret on a tower or a pillar,
   a plant by a gate or the hub; its area the whole arena */
static void guest_spawn(int k) {
    const int32_t c = api->import_class(KIND[k].import);
    if (c < 0 || s.nen >= MAX_EN) return;
    float at[3];
    int spot = -1;
    if (KIND[k].moves == 3) {
        spot = static_spot(k);
        if (spot < 0) return;
        const float* o = k == K_TURRET ? TURRET_SPOTS[spot] : SPORE_SPOTS[spot];
        at[0] = s.centre[0] + o[0], at[1] = s.centre[1] + o[1], at[2] = nav.floor_z + o[2];
    } else if (guest_near(at)) {
        at[2] += KIND[k].moves == 1 ? 3.0f : 0.5f;
    } else {
        const float* p = s.points[s.next_point++ % s.npoints];
        if (KIND[k].moves == 1)
            for (int i = 0; i < s.npoints; ++i)
                if (s.is_drop[(s.next_point + i) % s.npoints]) {
                    p = s.points[(s.next_point + i) % s.npoints];
                    break;
                }
        at[0] = p[0] + 3.0f * (rnd() - 0.5f), at[1] = p[1] + 3.0f * (rnd() - 0.5f), at[2] = p[2] + (KIND[k].moves == 1 ? 3.0f : 0.5f);
    }
    const float yaw = atan2f(s.centre[1] - at[1], s.centre[0] - at[0]);
    const uint32_t m = api->import_spawn(c, at, yaw);
    if (m == 0) return;
    if (arena_path != 0 && RAC1_API_HAS(api, object_bind_path)) {  /* (their areas: the arena; slots their profiles name) */
        api->object_bind_path(m, "area", arena_path);
        if (k == K_AMOEBOID) api->object_bind_path(m, "home", arena_path);
    }
    enemy_add(0, k, 0, at);
    s.en[s.nen - 1].m = m;
    s.en[s.nen - 1].spot = spot;
    ++s.spawned;
}

/* the hazards of the wave's mix, placed at its start where none is: the spore plants (P, by the gates and the hub) and the
   turrets (T, on the towers' tops and the pillars), 2 of each and one more every second wave. Not enemies: not counted (the
   plants cannot be killed; the turrets can, the round does not wait for them); they stay until their class is released */
static void hazards_place(const char* mix) {
    for (int k = 0; k < N_KINDS; ++k) {
        if (KIND[k].moves != 4 || KIND[k].import == NULL || mix == NULL || strchr(mix, KIND[k].letter) == NULL) continue;
        const int32_t c = api->import_class(KIND[k].import);
        if (c < 0) continue;
        const float(*spots)[3] = k == K_TURRET ? TURRET_SPOTS : SPORE_SPOTS;
        const int n = k == K_TURRET ? (int)(sizeof TURRET_SPOTS / sizeof TURRET_SPOTS[0]) : (int)(sizeof SPORE_SPOTS / sizeof SPORE_SPOTS[0]);
        int have = 0;
        for (int j = 0; j < nhaz; ++j)  /* (still in play: a turret destroyed leaves its spot free) */
            if (haz_kind[j] == k && (int32_t)rac1_lh(api, haz_m[j] + 0xa6) == c && (int8_t)rac1_lb(api, haz_m[j] + 0x20) >= 0) ++have;
        const int want = 2 + s.wave / 2;
        for (int i = 0; i < n && have < want && nhaz < 16; ++i) {
            int used = 0;
            for (int j = 0; j < nhaz && !used; ++j)
                used = haz_kind[j] == k && haz_spot[j] == i && (int32_t)rac1_lh(api, haz_m[j] + 0xa6) == c &&
                       (int8_t)rac1_lb(api, haz_m[j] + 0x20) >= 0;
            if (used) continue;
            const float at[3] = {s.centre[0] + spots[i][0], s.centre[1] + spots[i][1], nav.floor_z + spots[i][2]};
            const uint32_t m = api->import_spawn(c, at, atan2f(s.centre[1] - at[1], s.centre[0] - at[0]));
            if (m == 0) continue;
            for (int j = 0; j < nhaz; ++j)  /* (a spot's old entry, its turret gone: replaced) */
                if (haz_kind[j] == k && haz_spot[j] == i) {
                    haz_m[j] = haz_m[--nhaz];
                    haz_kind[j] = haz_kind[nhaz];
                    haz_spot[j] = haz_spot[nhaz];
                    break;
                }
            haz_m[nhaz] = m;
            haz_kind[nhaz] = k;
            haz_spot[nhaz++] = i;
            ++have;
        }
    }
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
        if (kk >= 0 && kk != K_GUNSHIP && KIND[kk].moves != 4 && kind_ready(kk)) k = kk;
    }
    if (k < 0) return;
    if (KIND[k].moves == 2) {
        ship_arrive(k, h);
        return;
    }
    if (KIND[k].import != NULL) {
        guest_spawn(k);
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
        const uint32_t m = en_moby(e);
        if (m == 0 || k->moves != 0 || k->import != NULL || rac1_lw(api, m + 0x78) == 0) continue;  /* (the flyers: their own flight;
                                                                                     other planets': straight at him) */
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
    /* a nanotech crate now and then (one delivery in five; the wrench only, needing no ammo: one in three), else two
       ammo crates */
    const float r = rnd();
    if (s.rule.weapons == W_WRENCH) {
        if (r < 0.33f) supplies_drop(CRATE_NANO, 1);
    } else {
        supplies_drop(r < 0.2f ? CRATE_NANO : CRATE_AMMO, r < 0.2f ? 1 : 2);
    }
}

/* ---- the electrified drop zones (the rules' vents: Grate impazzite) ----
   While the vents burn faster the three drop zones' grated circles take turns at being electrified: a warning (their
   ring, the class zap's object, blinking over the texture's ring, a crackle), then live (the ring lit, a cyan light, a
   buzz): Ratchet standing in one is hurt by one every ZAP_HURT frames, the enemies in one are hit (the zones are a
   trap for them too). One at a time, a few seconds apart. */
#define ZAP_R 5.4f       /* the ring of the drop zone's texture: 5 to 5.6 from its middle */
#define ZAP_WARN 90
#define ZAP_LIVE 150
#define ZAP_HURT 40
#define ZAP_HIT 30       /* frames between two hits on an enemy in it */
enum { Z_REST, Z_WARN, Z_LIVE };

static struct {
    int32_t cls, snd_crackle, snd_buzz;
    int n;
    uint32_t obj[N_POINTS];
    float p[N_POINTS][3];
    int phase[N_POINTS], t[N_POINTS];
    int32_t buzz[N_POINTS];  /* its buzz's sound slot, -1 none */
    int wait, hurt_t;
} zap;

/* at the level's start: a hidden ring on each drop zone */
static void zap_start(void) {
    zap.cls = api->class_find("zap");
    zap.snd_crackle = api->sfx_find("zap_crackle");
    zap.snd_buzz = api->sfx_find("zap_buzz");
    zap.n = 0;
    zap.wait = 180;
    for (int i = 0; i < s.npoints && zap.cls >= 0; ++i) {
        if (!s.is_drop[i]) continue;
        memcpy(zap.p[zap.n], s.points[i], sizeof zap.p[0]);
        const float ring[3] = {zap.p[zap.n][0], zap.p[zap.n][1], zap.p[zap.n][2] + 0.12f};  /* (just over the floor) */
        zap.obj[zap.n] = api->object_spawn(zap.cls, ring, 0.0f);
        if (zap.obj[zap.n] != 0) api->object_show(zap.obj[zap.n], 0);
        zap.phase[zap.n] = Z_REST;
        zap.buzz[zap.n] = -1;
        ++zap.n;
    }
}

/* its ring shown or not (placed again when shown: its matrix kept up, as the bombs' rings) */
static void zap_ring(int i, int on) {
    if (zap.obj[i] == 0) return;
    api->object_show(zap.obj[i], on);
    if (on) {
        const float rot[3] = {0.0f, 0.0f, 0.0f}, at[3] = {zap.p[i][0], zap.p[i][1], zap.p[i][2] + 0.12f};
        api->object_place(zap.obj[i], at, rot, 1.0f);
    }
}

static void zap_rest(int i) {
    zap.phase[i] = Z_REST;
    if (zap.obj[i] != 0) api->object_show(zap.obj[i], 0);
    if (zap.buzz[i] >= 0 && api->sfx_active(zap.buzz[i], zap.snd_buzz)) api->sfx_stop(zap.buzz[i]);
    zap.buzz[i] = -1;
}

/* each frame: the zones' turns while the rules have the vents on (a challenge running), else all at rest */
static void zap_update(const float h[3]) {
    const int on = s.rule.vents && (s.mode == FIGHT || s.mode == BREAK);
    if (zap.hurt_t > 0) --zap.hurt_t;
    for (int i = 0; i < zap.n; ++i)
        if (!on && zap.phase[i] != Z_REST) zap_rest(i);
    if (!on || zap.n == 0) return;
    if (--zap.wait <= 0) {
        const int i = (int)(rnd() * (float)zap.n) % zap.n;
        if (zap.phase[i] == Z_REST) {
            zap.phase[i] = Z_WARN;
            zap.t[i] = 0;
            if (zap.snd_crackle >= 0) api->sfx_play(zap.snd_crackle, 0, zap.p[i]);
        }
        zap.wait = ZAP_WARN + ZAP_LIVE + 90 + (int)(rnd() * 180.0f);
    }
    static const float cyan[3] = {0.45f, 0.85f, 1.0f};
    for (int i = 0; i < zap.n; ++i) {
        const float up[3] = {zap.p[i][0], zap.p[i][1], zap.p[i][2] + 1.0f};
        if (zap.phase[i] == Z_WARN) {
            zap_ring(i, (zap.t[i] / 6) % 2 == 0);
            api->light_add(up, cyan, 0.6f + 0.8f * rnd(), 9.0f);
            if (++zap.t[i] >= ZAP_WARN) {
                zap.phase[i] = Z_LIVE;
                zap.t[i] = 0;
                char msg[64];
                snprintf(msg, sizeof msg, "zap: drop zone %d live", i);
                api->log(msg);
                if (zap.snd_buzz >= 0) zap.buzz[i] = api->sfx_play(zap.snd_buzz, 0, zap.p[i]);
            }
        } else if (zap.phase[i] == Z_LIVE) {
            zap_ring(i, zap.t[i] % 3 != 0);
            api->light_add(up, cyan, 2.5f + 2.0f * rnd(), 11.0f);
            const float dx = h[0] - zap.p[i][0], dy = h[1] - zap.p[i][1];
            if (dx * dx + dy * dy < ZAP_R * ZAP_R && h[2] < zap.p[i][2] + 1.5f && zap.hurt_t == 0) {
                api->hero_damage(1, 0);
                zap.hurt_t = ZAP_HURT;
            }
            if (zap.t[i] % ZAP_HIT == 0)
                for (int k = 0; k < s.nen; ++k) {
                    const uint32_t m = en_moby(&s.en[k]);
                    if (m == 0) continue;
                    float r[3];
                    pos_of(m, r);
                    const float ex = r[0] - zap.p[i][0], ey = r[1] - zap.p[i][1];
                    if (ex * ex + ey * ey < ZAP_R * ZAP_R && r[2] < zap.p[i][2] + 2.0f) api->hit_send(m, RAC1_HIT_ENEMY, 3.0f, api->hero());
                }
            if (++zap.t[i] >= ZAP_LIVE) zap_rest(i);
        }
    }
}

/* ---- the grind rails (RAIL; level.txt's rail lines: export_arena.py's, from build_rails.py's curves) ----
   The composer puts the level's rails into the game's rail table, a collision strip under each (the game's grind lets go
   of a rail with nothing under Ratchet's feet); on a level mod with rails of its own the hero code takes one anywhere.
   They are all out of the game's search from the level's start (rail_enable), their models hidden: a rail challenge
   turns its own on once its countdown is over and off again when it ends. Once on one, the game's jumps and the side
   jumps to a rail beside it (2..4.5 away: the lanes) do the rest. The challenges need the Grindscarponi (0x13d4dd, the
   grind boots) and the SDK's ABI 11: without them the menu does not start them.
   The course ("Grindscarponi: Discesa") has obstacles on its lanes: our mines (the class mine's objects) are jumped;
   Kalebo III's rail mines (class 933, started by base_spawn where the spawn points with kind=kmine are) are broken with
   the wrench (Square on the rail) or a weapon from afar, or passed on another lane; each hurts him by one. They blow up
   with their own code (its explosion): near him the plugin decides (and sends the hit), farther their own code takes
   his attacks. Its end leads to a last wave in the arena. */
#define GRIND_BOOTS 0x0013d4ddu
#define MAX_RAILS 6
#define N_WMOBYS 16  /* a world's objects the plugin starts there (La grande fuga's worlds) */
/* Kalebo III's rail mines on its long rail (its own places), started by the plugin when Ratchet comes into its world */
static const float KALEBO_MINES[][3] = {
    {84.8f, 200.2f, 121.8f}, {134.3f, 200.8f, 106.1f}, {135.8f, 201.6f, 106.5f}, {115.0f, 149.9f, 127.6f}, {165.1f, 100.5f, 118.9f},
    {292.4f, 130.6f, 129.3f}, {336.7f, 90.1f, 140.7f}, {357.8f, 193.2f, 140.1f}, {343.2f, 204.2f, 134.7f}, {340.9f, 204.6f, 134.1f},
};
#define RAIL_ATTACK 0x2b  /* his wrench swing on a rail */
#define MAX_OBST 48
#define MINE_BOLTS 50      /* a rail mine broken with the wrench */
#define RAIL_MINE 933      /* Kalebo III's rail mine (level.txt's spawn 933 14): it floats 0.7 over the rail */
#define MINE_SWING 2.6f    /* the wrench's swing on the rail breaks one this near his middle */
#define MINE_TOUCH 2.0f    /* nearer, and not swinging, it blows up on him (before he bumps into it, at about 1.4) */
#define MINE_HELD 4        /* the rail mine's state the plugin keeps it in while he is near: its own code off. In its own
                              state 1 it blows up when he passes within 3 or 4 (also on the next lane), hurting him unless
                              he swings, or when an attack hits it (a weapon, the wrench): its explosion, then hidden in
                              its state 2 until off screen */
#define MINE_GONE 5        /* blown up for good: hidden, its own code off (taken away at the next try) */
#define MINE_NEAR 6.0f     /* held this near him; farther, its own code (his weapons break it, as on Kalebo III) */

enum { O_MINE, O_KMINE };
enum { BLOW_NONE, BLOW_BROKEN, BLOW_TOUCH };  /* a rail mine blown up by its own code on the plugin's hit: broken, or on him */

static struct {
    int32_t set[MAX_RAILS];    /* the challenge's rails (rail_find's handles; set[0]: the one whose end finishes it) */
    char name[MAX_RAILS][24];  /* their names (their models: the objects rail_<name>) */
    int nset;
    int on;                    /* the challenge's rails in the game's search now */
    const char* start;         /* the spawn point Ratchet starts from */
    int phase;                 /* 0 the countdown on the station, 1 riding, 2 the last wave in the arena */
    int ride;                  /* frames riding (the time limit) */
    int falls;
    int reached;  /* the finishing rail's end reached (the rail callback's END): done once he is off it */
    struct {
        uint32_t obj;  /* O_MINE: our object; O_KMINE: its spawn point's (kind=kmine) */
        uint32_t h;    /* O_KMINE: base_spawn's handle while it is out */
        int kind, alive;
        int blow;      /* O_KMINE: BLOW_*, the plugin's hit sent (its own code blows it up) */
        int course;    /* 0 the course's, 1 the escape's (named mine_e<n>, kmine_e<n>) */
        float p[3];
    } obs[MAX_OBST];
    int nobs, kmines, kmines_e, kmines_hit;  /* (the course's rail mines, the escape's, those broken) */
    int use_obs;  /* the obstacles on: bit 0 the course's, bit 1 the escape's */
    /* La grande fuga: phase 3 the lava rising (Ratchet to the hub's top), 4 riding the escape, 5 the worlds beyond its
       portal (the challenge's sections, one after the other: CHAIN) */
    int32_t escape;      /* the escape's rail (rail_find's), 0 none */
    uint32_t flood;      /* the flood's object (the lava rising over the arena's floor) */
    float flood_z0;      /* its place under the lava, where it rests */
    int flood_t, hurt_t, esc_ride;
    int sec_in;          /* phase 5: Ratchet is in the section (its switch made: section_entered) */
    int sec_asked;       /* phase 5: the switch asked (the flash's white held) */
    char stage_name[4][16];  /* the challenge's sections, in order */
    int nstages, stage;      /* phase 5: how many, the one now */
    float goal[3];       /* reaching it (the world's ring) leads on to the next one */
    uint32_t ring;       /* the portal's ring there (the class portal's object: drawn in a world's section since composer 7) */
    int has_goal, off_rail;  /* (a rail world: frames off its rail before its ring, a fall) */
    int32_t stage_rail;  /* the world's rail of ours <section>_in (Ratchet put on it as he comes in), 0 none */
    int stage_step;      /* (a board world) 0 down its rail, 1 on the board */
    int stage_t;         /* frames in the world */
    int rail_done;       /* its rail's end reached (the rail callback) */
    uint32_t board;      /* the board (base_spawn's handle) */
    int rammed;          /* foes knocked down by the board */
    uint32_t wbase[N_WMOBYS];  /* Kalebo III's rail mines the plugin started in a world (base_spawn handles) */
    int nwbase;
    int going_home;      /* the last world's portal: back to the arena, the waves going on (escape_after) */
} rails;

/* the game has the rail services (ABI 11) */
static int rails_sdk(void) {
    return api->abi >= 11;
}

/* a waves challenge with an escape (Assalto Finale): its escape's rail, no course */
static void escape_prepare(int ch) {
    rails.nset = 0;
    rails.phase = 0;
    rails.falls = 0;
    rails.use_obs = 0;
    rails.going_home = 0;
    rails.escape = rails_sdk() ? api->rail_find(CHALLENGES[ch].escape) : 0;
}

/* his states on a rail: 0x28 grinding, 0x29 / 0x2a jumping, 0x2b attacking, 0x42 hopping off (hero_transitions) */
static int on_rail(void) {
    const uint32_t st = rac1_lw(api, HERO_STATE);
    return (st >= 0x28 && st <= 0x2b) || st == 0x42;
}

/* a rail's model (the class rail_<name>'s object) shown or not */
static void rail_show(const char* name, int on) {
    char n[32];
    snprintf(n, sizeof n, "rail_%s", name);
    const uint32_t o = api->object_named(n);
    if (o != 0) api->object_show(o, on);
}

/* a rail's collision strips (export_arena.py's objects strip_<rail>_<k>: the grind lets go of a rail with nothing under
   Ratchet's feet): solid only while the rail is on (else a ledge in the air: he walked on it over the hub), never drawn */
static void rail_strips(const char* name, int on) {
    for (int k = 0; k < 8; ++k) {
        char n[40];
        snprintf(n, sizeof n, "strip_%s_%d", name, k);
        const uint32_t o = api->object_named(n);
        if (o == 0) break;
        api->object_show(o, 0);
        api->object_solid(o, on);
    }
}

static float dist3(const float a[3], const float b[3]) {
    const float x = a[0] - b[0], y = a[1] - b[1], z = a[2] - b[2];
    return sqrtf(x * x + y * y + z * z);
}

/* the level's start: every rail out of the game's search, its model hidden */
static void rails_all_off(void) {
    if (!rails_sdk()) return;
    char name[24];
    int n = 0;
    for (int32_t r = 1; api->rail_info(r, name, sizeof name, NULL, NULL); ++r, ++n) {
        api->rail_enable(r, 0);
        rail_show(name, 0);
        rail_strips(name, 0);
    }
    char msg[64];
    snprintf(msg, sizeof msg, "rails: %d, all off", n);
    api->log(msg);
}

/* the challenge's rails in the game's search (on) or out of it */
static void rails_switch(int on) {
    for (int j = 0; j < rails.nset; ++j) {
        api->rail_enable(rails.set[j], on);
        rail_strips(rails.name[j], on);
    }
    rails.on = on;
    if (on) {
        char msg[96];
        snprintf(msg, sizeof msg, "rails on: %d (the first: %s)", rails.nset, rails.name[0]);
        api->log(msg);
    }
}

/* the rail callback (ABI 11): the finishing rail ridden to its end */
static void portal_go(void);

static void rail_event(void* user, int32_t rail, const char* name, int32_t event) {
    (void)user, (void)name;
    if (rails.phase == 5 && rails.stage_rail != 0 && rail == rails.stage_rail && event == RAC1_RAIL_END) rails.rail_done = 1;
    if (rails.on && rails.phase == 1 && rail == rails.set[0] && event == RAC1_RAIL_END) rails.reached = 1;
    if ((rails.phase == 3 || rails.phase == 4) && rail != 0 && rail == rails.escape && event == RAC1_RAIL_END) portal_go();
}

/* Ratchet has the Grindscarponi (the rail challenges need them) */
static int has_boots(void) {
    return rac1_lb(api, GRIND_BOOTS) != 0;
}

/* Ratchet put there by the plugin (not taken for a death) */
static void hero_move(const float p[3], float yaw) {
    api->hero_place(p, yaw);
    s.placed = 3;
    s.have_last = 0;  /* (also when moved from outside frame: the test callback) */
}

/* the course's obstacles, found at the level's start: our mines (the class mine's objects) and the places of Kalebo
   III's rail mines (the spawn points with kind=kmine) */
static void obstacles_find(void) {
    const int32_t mine = api->class_find("mine"), spawn = api->class_find("spawn");
    rails.nobs = rails.kmines = rails.kmines_e = 0;
    for (int32_t i = 0; i < api->object_count() && rails.nobs < MAX_OBST; ++i) {
        const uint32_t o = api->object_at(i);
        const int32_t c = api->object_class(o);
        char kind[16] = "";
        if (c < 0 || (c != mine && c != spawn)) continue;
        if (c == spawn && (!api->object_param(o, "kind", kind, sizeof kind) || strcmp(kind, "kmine") != 0)) continue;
        char name[24] = "";
        api->object_param(o, "name", name, sizeof name);
        rails.obs[rails.nobs].obj = o;
        rails.obs[rails.nobs].h = 0;
        rails.obs[rails.nobs].kind = c == mine ? O_MINE : O_KMINE;
        rails.obs[rails.nobs].course = strstr(name, "_e") != NULL;
        rails.obs[rails.nobs].alive = 0;
        pos_of(api->object_moby(o), rails.obs[rails.nobs].p);
        if (c == mine) api->object_show(o, 0);
        if (c != mine && rails.obs[rails.nobs].course) ++rails.kmines_e;
        else if (c != mine) ++rails.kmines;
        ++rails.nobs;
    }
}

/* a rail mine's moby while it is out, else 0 */
static uint32_t kmine_moby(int i) {
    return rails.obs[i].h != 0 ? api->base_moby(rails.obs[i].h) : 0;
}

/* those of the courses in `mask` back (a new try: the rail mines started again where they were), the others gone */
static void obstacles_reset(int mask) {
    for (int i = 0; i < rails.nobs; ++i) {
        const int on = (mask >> rails.obs[i].course) & 1;
        if (rails.obs[i].kind == O_KMINE) {
            const uint32_t m = kmine_moby(i);
            if (m != 0) api->moby_remove(m);
            rails.obs[i].h = on ? api->base_spawn(RAIL_MINE, rails.obs[i].p, 0.0f) : 0;
            rails.obs[i].alive = rails.obs[i].h != 0;
            rails.obs[i].blow = BLOW_NONE;
        } else {
            rails.obs[i].alive = on;
            api->object_show(rails.obs[i].obj, on);
        }
    }
    rails.kmines_hit = 0;
}

/* our mine blown up: smoke, a flash, the boom; the object hidden */
static void obstacle_blast(int i, const float at[3]) {
    const float up[3] = {0.0f, 0.0f, 0.04f}, fire[3] = {1.0f, 0.55f, 0.2f};
    for (int k = 0; k < 4; ++k) api->smoke(at, up, 1.2f + 0.4f * (float)k);
    api->light_add(at, fire, 3.0f, 8.0f);
    if (snd_boom >= 0) api->sfx_play(snd_boom, 0, at);
    rails.obs[i].alive = 0;
    api->object_show(rails.obs[i].obj, 0);
}

/* a rail mine blown up by its own code (its explosion, its sound): back in its state 1 with a hit of the player's, which
   it takes at its next update */
static void kmine_blow(int i, uint32_t m, int how) {
    rails.obs[i].blow = how;
    api->sb(m + 0x20, 1);
    api->hit_send(m, RAC1_HIT_PLAYER, 1.0f, api->hero());
}

/* riding past them: our mine under his feet hurts him (jump it); a rail mine breaks under his wrench's swing (bolts)
   or blows up on him when he comes too near without one (he jumps over it: his middle stays away) */
static void obstacles_update(const float h[3]) {
    const int swing = rac1_lw(api, HERO_STATE) == RAIL_ATTACK;
    const float mid[3] = {h[0], h[1], h[2] + 0.9f};
    for (int i = 0; i < rails.nobs; ++i) {
        if (!rails.obs[i].alive) continue;
        if (rails.obs[i].kind == O_MINE) {
            const float* p = rails.obs[i].p;
            const float dxy = sqrtf((h[0] - p[0]) * (h[0] - p[0]) + (h[1] - p[1]) * (h[1] - p[1])), dz = h[2] - p[2];
            if (dxy < 0.8f && dz > -0.6f && dz < 1.1f) {
                obstacle_blast(i, p);
                api->hero_damage(1, 0);
            }
            continue;
        }
        const uint32_t m = kmine_moby(i);
        if (m == 0) {  /* (gone by itself) */
            rails.obs[i].alive = 0;
            continue;
        }
        float q[3];
        pos_of(m, q);
        const float d = dist3(mid, q);
        const int st = (int8_t)rac1_lb(api, m + 0x20);
        /* blown up by its own code (in its state 2, hidden): by a hit (his weapon or wrench from afar, the plugin's below)
           or on him. Gone for good, its explosion playing on (its own code would put it back once off screen; the next try
           takes it away); broken unless on him */
        if (st == 2 || (rac1_lh(api, m + 0x34) & MOBY_HIDDEN) != 0) {
            api->sb(m + 0x20, MINE_GONE);
            rails.obs[i].alive = 0;
            const int broken = rails.obs[i].blow != BLOW_TOUCH && (rails.obs[i].blow == BLOW_BROKEN || swing || d >= MINE_NEAR);
            if (broken) {
                ++rails.kmines_hit;
                api->bolts_add(MINE_BOLTS);
            }
            if (s.test_log) {
                char b[96];
                snprintf(b, sizeof b, "rail mine %d blown up at %.2f (%s)", i, d, broken ? "broken" : "on him");
                api->log(b);
            }
            continue;
        }
        if (rails.obs[i].blow != BLOW_NONE || st == 0) continue;  /* (the plugin's hit sent; still starting) */
        rac1_hit hit = {sizeof hit};
        const int shot = st == MINE_HELD && api->hit_take(m, RAC1_HIT_PLAYER, &hit);  /* (held: an attack of his kept) */
        if (shot || (swing && d < MINE_SWING)) {
            kmine_blow(i, m, BLOW_BROKEN);
            if (s.test_log) {
                char b[96];
                snprintf(b, sizeof b, "rail mine %d broken at %.2f (%s)", i, d, shot ? "hit" : "the swing");
                api->log(b);
            }
        } else if (!swing && d < MINE_TOUCH) {
            kmine_blow(i, m, BLOW_TOUCH);
            api->hero_damage(1, m);
            if (s.test_log) {
                char b[96];
                snprintf(b, sizeof b, "rail mine %d hit him at %.2f", i, d);
                api->log(b);
            }
        } else if (d < MINE_NEAR) {
            if (st == 1) api->sb(m + 0x20, MINE_HELD);  /* (near him: its own code off, the next lane safe) */
        } else if (st == MINE_HELD) {
            api->sb(m + 0x20, 1);  /* (away from him: its own code again) */
        }
    }
}

/* on the station, facing the first rail's start; the obstacles back */
static void rail_to_start(void) {
    const uint32_t o = rails.start ? api->object_named(rails.start) : 0;
    float p[3], a[3];
    if (o == 0 || rails.nset == 0 || !api->rail_point(rails.set[0], 0.0f, a, NULL)) return;
    pos_of(api->object_moby(o), p);
    hero_move(p, atan2f(a[1] - p[1], a[0] - p[0]));
    rails.reached = 0;
    if (rails.use_obs) obstacles_reset(rails.use_obs);
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
    rails.escape = c->escape && rails_sdk() ? api->rail_find(c->escape) : 0;
    char names[64];
    snprintf(names, sizeof names, "%s", c->rails && rails_sdk() ? c->rails : "");
    for (char* t = strtok(names, " "); t != NULL && rails.nset < MAX_RAILS; t = strtok(NULL, " ")) {
        const int32_t r = api->rail_find(t);
        if (r == 0) continue;
        snprintf(rails.name[rails.nset], sizeof rails.name[0], "%s", t);
        rails.set[rails.nset++] = r;
    }
    if (rails.nset == 0) {
        toast(tr("Nessuna rotaia"));
        return;
    }
    for (int j = 0; j < rails.nset; ++j) rail_show(rails.name[j], 1);
    rail_to_start();
    api->input_hold(1);
}

/* the countdown over: the rails in the game's search, Ratchet free */
static void rail_go(void) {
    if (rails.phase != 0) return;
    api->input_hold(0);
    if (rails.nset == 0) {
        challenge_end(0, "nessuna rotaia");
        return;
    }
    rails_switch(1);
    rails.phase = 1;
    toast(tr("Salta sulla rotaia!"));
}

/* the rails and their models gone (and the obstacles) */
static void rails_away(void) {
    if (rails.on) rails_switch(0);
    for (int j = 0; j < rails.nset; ++j) rail_show(rails.name[j], 0);
    if (rails.use_obs) obstacles_reset(0);
}

/* the end: all gone, Ratchet back in the arena if he is not */
static void flood_reset(void);
static int in_main(void);

static void rail_stop(void) {
    api->input_hold(0);
    rails_away();
    if (rails.escape != 0) {
        api->rail_enable(rails.escape, 0);
        rail_show("escape", 0);
        rail_strips("escape", 0);
    }
    flood_reset();
    rails.phase = 0;
    const uint32_t hero = api->hero();
    if (hero == 0 || !in_main()) return;  /* (in the section beyond the portal: the result takes him back) */
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
    if (rails.reached && !on_rail()) {
        if (c->wave[0].count > 0) {  /* (down in the arena: the last wave) */
            rails_away();
            rails.phase = 2;
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
        toast(tr("Caduto! Di nuovo dalla stazione"));
    }
    if (rails.ride >= c->time * 60) challenge_end(0, "tempo scaduto");
}

/* ---- La grande fuga: the lava floods the arena, the escape's rail from the hub's top, its portal, the section ---- */
#define FLOOD_RISE 600     /* frames the lava takes to flood the arena */
#define FLOOD_TOP 1.8f     /* its top over the floor at last (the hub's top is at 3: Ratchet waits there) */
#define FLOOD_HURT 45      /* frames between two hurts while Ratchet stands in it */
#define ESCAPE_TIME 120    /* seconds from the lava's start to the portal */
#define SECTION_ENTRY "arrivo"  /* the entry of the challenge's section (challenge_def.section), main's entry back */
#define MAIN_ENTRY "ritorno"
#define PORTAL_BACK 6.0f   /* the portal's ring on the escape, this far before its end (build_rails.py's) */
#define FLASH 0xffffffu    /* the portal's flash (a white screen: out, held, back) */
#define FLASH_OUT 2        /* (frames: up to white, held while the switch is made, back) */
#define FLASH_HOLD 3
#define FLASH_IN 12
#define RESULT_BACK 150    /* frames of the result's banner in the section before the way back to the arena */

/* Ratchet is in main (the arena's part of the session), not in a section */
static int in_main(void) {
    char n[24];
    return !RAC1_API_HAS(api, section_current) || api->section_current(n, sizeof n) <= 0;
}

/* the worlds beyond the escape's portal (challenge_def.section's sections, level.txt's): what each asks. On a rail world
   Ratchet comes in just over the planet's own rail (its entry) and lands on it; a portal's ring on it (ours, near its end)
   leads on through another flash, and off the rail he has fallen. A boss (level.txt's boss line) beaten wins the challenge */
static const struct chain_def {
    const char* name;
    const char* goal;  /* the HUD's line (TEXTS) */
    int rail;          /* 1: ridden on the planet's rail to the ring (off it: a fall); 0 none */
    float ring[3];     /* the ring's place (the planet's coordinates), its yaw (the way through it) */
    float ring_yaw;
    int board;         /* 1: down our rail <section>_in, then Kalebo III's hoverboard to the ring (ABI 16), ramming foes */
    float start[3];    /* (board) where a fall puts him back on it, turned start_yaw; under fall_z he falls */
    float start_yaw, fall_z;
    const char* foes;  /* (board) the foes on the way (KIND letters, one each), between start and ring */
    float reach;       /* > 0: the ring reached within this (across; 4 up or down) leads on, however he gets there */
} CHAIN[] = {
    /* Quartu: our rail quartu_in from high south of its Giant Clank's pad down by it, then the board east across the
       Giant Clank's round arena to its middle (195 235: the ring), Kalebo III's robots and bomb bots in the way */
    {"quartu", "Quartu: in tavola fino al centro dell'arena", 0, {195.0f, 235.0f, 26.0f}, 0.0f, 1, {133.0f, 222.0f, 26.0f}, 0.0f, 24.0f,
     "RBRRBRBRRB", 3.0f},
    /* Batalia's long rail (its rail 2: 359 units, from 289.5 86.4 31.9 to 328.9 104.2 49.2, where it leaps), the ring 6
       before its end */
    /* Kalebo III: its own long rail (its rail 1: 1109 units, from its station by the arena's, 114 264 133, to its leap at 317
       206 130; about a minute), the ring 6 before its end; its rail mines (Kalebo III's 933, ours: KALEBO_MINES) */
    {"kalebo", "Kalebo III: fino al portale in fondo alla rotaia", 1, {323.36f, 206.0f, 130.19f}, 3.1416f, 0, {0}, 0.0f, 0.0f, NULL,
     0.0f},
    {"batalia", "Batalia: fino in fondo alla rotaia", 1, {334.2f, 106.7f, 46.3f}, -2.69f, 0, {0}, 0.0f, 0.0f, NULL, 0.0f},
    /* Rilgar: its hoverboard race (its own: hero rilgar board, its HUD, back on the track after a fall) from its finish
       line, round two thirds of its lap to the big ramp, whose jump lands on the small platform of bolt crates: the ring */
    {"rilgar", "Rilgar: salta dalla rampa grande sulla piattaforma", 0, {264.0f, 440.0f, 80.0f}, 0.24f, 0, {0}, 0.0f, 0.0f, NULL, 7.0f},
    {"umbris", "Umbris: sconfiggi lo Snagglebeast", 0, {0.0f, 0.0f, 0.0f}, 0.0f, 0, {0}, 0.0f, 0.0f, NULL, 0.0f},
};
#define BOARD_CLASS 439    /* Kalebo III's hoverboard (level.txt's spawn 439 1) */


#define N_CHAIN ((int)(sizeof CHAIN / sizeof CHAIN[0]))
#define GOAL_NEAR 2.5f     /* the section's goal (its portal) reached this near */
#define OFF_RAIL 120       /* frames off a world's rail before its ring (or before he first lands on it): lost, he fell */

static const struct chain_def* chain_of(const char* name) {
    for (int i = 0; i < N_CHAIN; ++i)
        if (strcmp(CHAIN[i].name, name) == 0) return &CHAIN[i];
    return NULL;
}

/* the challenge's end: no longer beyond the portal (Ratchet may still be in a section: the result takes him back) */
static void portal_reset(void) {
    if (rails.phase == 5) rails.phase = 0;
    rails.sec_in = rails.sec_asked = 0;
    rails.has_goal = 0;
}

/* the hub's top, by the escape's start (Ratchet put back there after a fall from it), facing it */
static void hub_top(float p[3]) {
    p[0] = s.centre[0] + 7.0f, p[1] = s.centre[1], p[2] = nav.floor_z + 3.1f;
}

/* the flood down under the lava, hidden */
static void flood_reset(void) {
    rails.flood_t = 0;
    if (rails.flood == 0) return;
    const float p[3] = {s.centre[0], s.centre[1], rails.flood_z0};
    api->object_place(rails.flood, p, NULL, 0.0f);
    api->object_show(rails.flood, 0);
}

/* the arena's last round won: the lava rises, the escape's rail and its obstacles appear */
static void lava_start(void) {
    rails.phase = 3;
    rails.flood_t = rails.hurt_t = rails.esc_ride = 0;
    if (rails.flood != 0) api->object_show(rails.flood, 1);
    if (rails.escape != 0) {
        api->rail_enable(rails.escape, 1);
        rail_show("escape", 1);
        rail_strips("escape", 1);
    }
    rails.use_obs = 2;
    obstacles_reset(2);
    play(s.snd_wave);
    toast(tr("La lava sale! Sali al centro e prendi la rotaia"));
    api->log("the lava floods the arena: the escape's rail on");
}

/* through a portal (the escape's ring, grinding; a section's goal): a white flash; while it covers the screen the switch to
   the next section (portal_switch), where the challenge goes on (section_entered) */
static void portal_go(void) {
    if (rails.phase == 5 && !rails.sec_in) return;  /* (a switch under way) */
    const int next = rails.phase == 5 ? rails.stage + 1 : 0;
    if (rails.phase != 5) {
        obstacles_reset(0);
        rails.nstages = 0;  /* (the challenge's sections, in order) */
        char names[64];
        snprintf(names, sizeof names, "%s", CHALLENGES[s.ch].section ? CHALLENGES[s.ch].section : "");
        for (char* t = strtok(names, " "); t != NULL && rails.nstages < 4; t = strtok(NULL, " "))
            snprintf(rails.stage_name[rails.nstages++], sizeof rails.stage_name[0], "%s", t);
    }
    rails.phase = 5;
    rails.stage = next;
    rails.sec_in = rails.sec_asked = 0;
    rails.has_goal = 0;
    if (!RAC1_API_HAS(api, section_enter) || next >= rails.nstages) {
        challenge_end(0, "nessun portale");
        return;
    }
    api->fade(FLASH_OUT, FLASH_HOLD, FLASH_IN, FLASH);
    if (snd_boom >= 0) api->sfx_play(snd_boom, 0, NULL);
    api->log("the portal: the flash");
}

/* the flash's white held: the switch (no fade of its own) */
static void board_off(void);

static void portal_switch(void) {
    if (rails.sec_asked || api->fade_active() != 2) return;
    rails.sec_asked = 1;
    board_off();  /* (the world left: its board and its foes with it) */
    enemies_clear();
    if (rails.going_home) {  /* (the last world's portal: the arena again, where the waves go on) */
        if (!api->section_enter("main", MAIN_ENTRY, 0)) challenge_end(0, "nessun portale");
        api->log("the portal: back into the arena");
        return;
    }
    if (!api->section_enter(rails.stage_name[rails.stage], SECTION_ENTRY, 0)) {
        challenge_end(0, "nessun portale");
        return;
    }
    char b[64];
    snprintf(b, sizeof b, "the portal: into the section %s", rails.stage_name[rails.stage]);
    api->log(b);
}

/* the lava rising (hurting Ratchet in it), the escape's ride (its obstacles; a fall from it: back on the hub's top),
   its time */
static void escape_update(const float h[3]) {
    ++rails.esc_ride;
    if (rails.flood_t < FLOOD_RISE) ++rails.flood_t;
    const float z = rails.flood_z0 + (nav.floor_z + FLOOD_TOP - rails.flood_z0) * (float)rails.flood_t / (float)FLOOD_RISE;
    if (rails.flood != 0) {
        const float p[3] = {s.centre[0], s.centre[1], z};
        api->object_place(rails.flood, p, NULL, 0.0f);
    }
    const float lx = h[0] - s.centre[0], ly = h[1] - s.centre[1];
    const int riding = on_rail();
    if (rails.hurt_t > 0) --rails.hurt_t;
    if (!riding && fabsf(lx) < 48.0f && fabsf(ly) < 48.0f && h[2] < z + 0.05f && rails.hurt_t == 0) {
        api->hero_damage(1, 0);
        rails.hurt_t = FLOOD_HURT;
    }
    if (riding) rails.phase = 4;
    rac1_rail_info ri = {sizeof ri};
    if (riding && api->hero_rail(&ri) && ri.rail == rails.escape && ri.distance >= ri.length - PORTAL_BACK) {
        portal_go();  /* (through the ring) */
        return;
    }
    obstacles_update(h);
    const int outside = fabsf(lx) > 48.0f || fabsf(ly) > 48.0f;
    if (rails.phase == 4 && !riding && s.placed == 0 && (h[2] < nav.floor_z - 1.2f || (outside && h[2] < nav.floor_z + 5.0f))) {
        float p[3];
        hub_top(p);
        hero_move(p, 3.1416f);
        ++rails.falls;
        rails.phase = 3;
        obstacles_reset(2);
        toast(tr("Caduto! Di nuovo dal centro"));
    }
    if (rails.esc_ride >= ESCAPE_TIME * 60) challenge_end(0, "tempo scaduto");
}

static void dump_level(void);

/* the section_enter callback (ABI 10): beyond the portal, the world's task begins (its rail, its boss); back in main, the
   arena's rails all out of the search again (a switch puts a section's rails back) */
static void section_entered(void* user, const char* name, const char* entry) {
    (void)user;
    if (!s.mine) return;
    s.placed = 3;
    s.have_last = 0;
    if (strcmp(name, "main") == 0) {
        rails_all_off();
        if (rails.going_home && s.mode == FIGHT) {  /* (back from the worlds: no lava, the next wave after a break) */
            rails.going_home = 0;
            rails.phase = 0;
            rails.sec_in = 0;
            flood_reset();
            if (rails.escape != 0) {
                api->rail_enable(rails.escape, 0);
                rail_show("escape", 0);
                rail_strips("escape", 0);
            }
            obstacles_reset(0);
            rails.use_obs = 0;
            if (session()) api->checkpoint_set("arena");
            ++s.wave;
            s.mode = BREAK;
            s.t = 0;
            toast(tr("Di nuovo nell'arena: si continua!"));
        }
        if (getenv("ARENA_DUMP_MAIN")) dump_level();  /* (a test's: what is in play back in main) */
        return;
    }
    if (s.mode != FIGHT || rails.phase != 5 || rails.stage >= rails.nstages || strcmp(name, rails.stage_name[rails.stage]) != 0 ||
        entry[0] == 0)
        return;
    const struct chain_def* d = chain_of(name);
    rails.stage_step = rails.stage_t = rails.rail_done = rails.rammed = 0;
    char rn[40];
    snprintf(rn, sizeof rn, "%s_in", name);  /* (its rail of ours: Ratchet goes on grinding on it) */
    rails.stage_rail = api->rail_find(rn);
    if (rails.stage_rail != 0 && !api->hero_rail_attach(rails.stage_rail, 0.0f, 1)) api->log("the section: its rail did not take Ratchet");
    rails.has_goal = d != NULL && (d->rail || d->board || d->reach > 0.0f);
    if (rails.has_goal) {  /* (the portal's ring on its rail: through it, the next world) */
        memcpy(rails.goal, d->ring, sizeof rails.goal);
        const float at[3] = {d->ring[0], d->ring[1], d->ring[2] + 0.9f};
        const int32_t pc = api->class_find("portal");
        rails.ring = pc >= 0 ? api->object_spawn(pc, at, d->ring_yaw) : 0;
        if (getenv("ARENA_RINGCAM") && rails.ring != 0) {  /* (a test's: the camera on the ring) */
            rac1_camera cam = {sizeof cam};
            cam.mode = RAC1_CAMERA_LOOK;
            const float c = cosf(d->ring_yaw), sn = sinf(d->ring_yaw);
            cam.pos[0] = at[0] - 9.0f * c + 3.0f * sn, cam.pos[1] = at[1] - 9.0f * sn - 3.0f * c, cam.pos[2] = at[2] + 2.5f;
            cam.moby = api->object_moby(rails.ring);
            api->camera_set(&cam);
        }
    }
    rails.off_rail = 0;
    rails.nwbase = 0;
    if (strcmp(name, "kalebo") == 0)  /* (its rail mines: ours, at each try) */
        for (int i = 0; i < (int)(sizeof KALEBO_MINES / sizeof KALEBO_MINES[0]) && rails.nwbase < N_WMOBYS; ++i) {
            const uint32_t hm = api->base_spawn(RAIL_MINE, KALEBO_MINES[i], 0.0f);
            if (hm != 0) rails.wbase[rails.nwbase++] = hm;
        }
    /* (Batalia's mines, Umbris's Snagglebeast: the worlds' own, back at every entry: their sections are `fresh`) */
    rails.sec_in = 1;
    if (d != NULL) toast(tr(d->goal));
    char b[128];
    snprintf(b, sizeof b, "the section %s (%d of %d): its goal %s (%.1f %.1f %.1f), its ring %u, %d objects of ours", name, rails.stage + 1,
             rails.nstages, rails.has_goal ? "its ring" : "none", rails.goal[0], rails.goal[1], rails.goal[2], (unsigned)rails.ring, rails.nwbase);
    api->log(b);
}

/* the world's objects the plugin started there taken away (the mines a try left) */
static void world_mobys_clear(void) {
    for (int i = 0; i < rails.nwbase; ++i) {  /* (Kalebo III's rail mines the plugin started there) */
        const uint32_t m = api->base_moby(rails.wbase[i]);
        if (m != 0) api->moby_remove(m);
    }
    rails.nwbase = 0;
}

/* the section_leave callback (ABI 10): leaving a world of the chain, its ring and its mines gone */
static void section_left(void* user, const char* name) {
    (void)user;
    if (!s.mine || chain_of(name) == NULL) return;
    if (rails.ring != 0) api->object_remove(rails.ring);
    rails.ring = 0;
    world_mobys_clear();
}

/* the boss callback (ABI 13): the last world's boss beaten wins the challenge */
static void boss_event(void* user, const char* section, int32_t cls, uint32_t obj, int32_t event) {
    (void)user, (void)cls, (void)obj;
    if (!s.mine || event != RAC1_BOSS_BEATEN || s.mode != FIGHT || rails.phase != 5 || !rails.sec_in) return;
    if (strcmp(section, rails.stage_name[rails.stage]) == 0) challenge_end(1, NULL);
}

/* in a world beyond the portal: its goal reached leads on (the last one's wins); off its rail too long, a fall */
/* a board world's board taken away (Ratchet himself again, where he is) */
static void board_off(void) {
    if (rails.board == 0) return;
    if (api->abi >= 16) {  /* (the board's rules back to the game's: they last until the level ends, Rilgar's race too) */
        api->board_rule(RAC1_BOARD_AUTO);
        api->board_return(NULL, 0.0f);
        api->board_fall(0, 0.0f);
    }
    if (RAC1_API_HAS(api, hero_mode) && api->hero_mode_now() == RAC1_HERO_BOARD) api->hero_mode(RAC1_HERO_NONE, 0);
    const uint32_t m = api->base_moby(rails.board);
    if (m != 0) api->moby_remove(m);
    rails.board = 0;
    if (in_main()) api->hero_build(RAC1_HERO_BUILD_GENERIC);  /* (main's own hero code again: level.txt's hero generic) */
}

/* ---- Autoscontro (BUMP): Kalebo III's hoverboard in the arena, the enemies knocked down by ramming them, rings for time ---- */
#define BUMP_RINGS 4
#define BUMP_BONUS 300  /* frames a ring gives */
static const float BUMP_SPOTS[][2] = {{0, -30}, {30, 0}, {-30, 0}, {0, 30}, {21, 21}, {-21, 21}, {21, -21}, {-21, -21},
                                      {38, -15}, {-38, 15}, {15, 38}, {-15, -38}};
#define N_BUMP_SPOTS ((int)(sizeof BUMP_SPOTS / sizeof BUMP_SPOTS[0]))
static struct {
    uint32_t ring[BUMP_RINGS];
    int spot[BUMP_RINGS];
    int bonus;   /* frames won by the rings */
    int rammed;
} bump;

/* a ring put on a free spot (not one of the others', not where it was) */
static void bump_ring_move(int i) {
    int spot = -1;
    for (int t = 0; t < 20 && spot < 0; ++t) {
        const int c = (int)(rnd() * (float)N_BUMP_SPOTS) % N_BUMP_SPOTS;
        int used = c == bump.spot[i];
        for (int j = 0; j < BUMP_RINGS && !used; ++j) used = j != i && bump.spot[j] == c;
        if (!used) spot = c;
    }
    if (spot < 0) return;
    bump.spot[i] = spot;
    const float at[3] = {s.centre[0] + BUMP_SPOTS[spot][0], s.centre[1] + BUMP_SPOTS[spot][1], nav.floor_z + 1.6f};
    const float rot[3] = {0.0f, 0.0f, atan2f(BUMP_SPOTS[spot][1], BUMP_SPOTS[spot][0]) + 1.5708f};  /* (across the way round) */
    if (bump.ring[i] == 0) {
        const int32_t pc = api->class_find("portal");
        bump.ring[i] = pc >= 0 ? api->object_spawn(pc, at, rot[2]) : 0;
    } else {
        api->object_place(bump.ring[i], at, rot, 0.0f);
    }
}

/* the countdown over: Ratchet on the board where he stands, the rings out */
static void bump_start(void) {
    float h[3];
    pos_of(api->hero(), h);
    memset(&bump, 0, sizeof bump);
    for (int i = 0; i < BUMP_RINGS; ++i) bump.spot[i] = -1;
    if (api->abi < 16) {
        challenge_end(0, "nessuna tavola");
        return;
    }
    const float yaw = rac1_lwf(api, api->hero() + 0x48);
    rails.board = api->base_spawn(BOARD_CLASS, h, yaw);
    const uint32_t bm = rails.board ? api->base_moby(rails.board) : 0;
    api->hero_build(16);  /* (Kalebo III's hero code rides it) */
    const int on = bm != 0 && api->hero_mode(RAC1_HERO_BOARD, bm);
    api->board_rule(RAC1_BOARD_FREE);
    const float back[3] = {s.centre[0], s.centre[1] - 16.0f, nav.floor_z + 0.5f};  /* (a fall into the lava: south of the hub) */
    api->board_return(back, 1.5708f);
    api->board_fall(1, nav.floor_z - 1.0f);
    for (int i = 0; i < BUMP_RINGS; ++i) bump_ring_move(i);
    toast(tr("Investili con la tavola! Gli anelli danno tempo"));
    char b[64];
    snprintf(b, sizeof b, "autoscontro: the board %s", on ? "on" : "NOT on");
    api->log(b);
}

/* riding: the enemies the board touches knocked down; a ring ridden through: time; the target felled wins, the time out loses */
static void bump_update(const float h[3]) {
    const challenge_def* c = &CHALLENGES[s.ch];
    uint32_t m[8];
    const int n = rails.board != 0 ? api->board_touching(m, 8) : 0;
    for (int k = 0; k < n; ++k)
        for (int i = 0; i < s.nen; ++i)
            if (en_moby(&s.en[i]) == m[k]) {
                api->hit_send(m[k], RAC1_HIT_PLAYER, 10.0f, api->hero());
                ++bump.rammed;
            }
    for (int i = 0; i < BUMP_RINGS; ++i) {
        if (bump.ring[i] == 0 || bump.spot[i] < 0) continue;
        const float dx = h[0] - (s.centre[0] + BUMP_SPOTS[bump.spot[i]][0]), dy = h[1] - (s.centre[1] + BUMP_SPOTS[bump.spot[i]][1]);
        const float dz = h[2] - nav.floor_z;
        if (dx * dx + dy * dy < 2.6f * 2.6f && dz > -1.0f && dz < 3.5f) {
            bump.bonus += BUMP_BONUS;
            play(s.snd_wave);
            toast(tr("+5 secondi"));
            bump_ring_move(i);
        }
    }
    if (s.felled >= c->target) {
        challenge_end(1, NULL);
        return;
    }
    if (s.clock >= c->time * 60 + bump.bonus) challenge_end(0, "tempo scaduto");
}

/* the end: off the board, the rings gone */
static void bump_end(void) {
    board_off();
    for (int i = 0; i < BUMP_RINGS; ++i)
        if (bump.ring[i] != 0) api->object_remove(bump.ring[i]);
    memset(&bump, 0, sizeof bump);
}

/* a board world: off its rail, Ratchet on Kalebo III's hoverboard (ABI 16: free, a fall back at its start), the foes in
   the way between there and the ring, hunting him */
static void board_start(const struct chain_def* d, const float h[3]) {
    rails.stage_step = 1;
    if (api->abi < 16) {
        challenge_end(0, "nessuna tavola");
        return;
    }
    const float yaw = atan2f(d->ring[1] - h[1], d->ring[0] - h[0]);
    rails.board = api->base_spawn(BOARD_CLASS, h, yaw);
    const uint32_t bm = rails.board ? api->base_moby(rails.board) : 0;
    api->hero_build(16);  /* (Kalebo III's hero code rides it) */
    const int on = bm != 0 && api->hero_mode(RAC1_HERO_BOARD, bm);
    api->board_rule(RAC1_BOARD_FREE);
    api->board_return(d->start, d->start_yaw);
    api->board_fall(1, d->fall_z);
    const int n = d->foes ? (int)strlen(d->foes) : 0;
    for (int i = 0; i < n && s.nen < MAX_EN; ++i) {  /* (along the way, from a third of it to near the ring, either side) */
        const int k = kind_of(d->foes[i]);
        if (k < 0 || !kind_ready(k)) continue;
        const float t = 0.3f + 0.6f * (float)i / (float)(n > 1 ? n - 1 : 1), side = (i & 1) ? 5.0f : -5.0f;
        const float dx = d->ring[0] - d->start[0], dy = d->ring[1] - d->start[1], len = sqrtf(dx * dx + dy * dy);
        const float at[3] = {d->start[0] + dx * t - side * dy / len, d->start[1] + dy * t + side * dx / len, d->ring[2] + 0.5f};
        const uint32_t e = api->base_spawn(KIND[k].cls, at, yaw + 3.1416f);
        if (e == 0) continue;
        enemy_add(e, k, 0, at);
        const uint32_t m = api->base_moby(e);
        if (m != 0) api->base_hunt(m, 1);
    }
    char b[96];
    snprintf(b, sizeof b, "the board: %s (moby %08x), %d foes", on ? "on it" : "NOT on it", (unsigned)bm, s.nen);
    api->log(b);
}

/* in a world beyond the portal: its goal reached leads on (the last one's wins); off its rail too long, a fall */
/* the last world's goal reached: the challenge won, or (escape_after) back to the arena through a last flash */
static void stage_last_done(void) {
    if (CHALLENGES[s.ch].escape_after <= 0) {
        challenge_end(1, NULL);
        return;
    }
    rails.going_home = 1;
    rails.sec_in = rails.sec_asked = 0;
    rails.has_goal = 0;
    api->fade(FLASH_OUT, FLASH_HOLD, FLASH_IN, FLASH);
    if (snd_boom >= 0) api->sfx_play(snd_boom, 0, NULL);
    api->log("the portal home: the flash");
}

static void stage_update(const float h[3]) {
    const struct chain_def* d = chain_of(rails.stage_name[rails.stage]);
    ++rails.stage_t;
    if (d != NULL && d->board) {
        if (rails.stage_step == 0) {  /* (down its rail: at its end, or off it, the board) */
            if (rails.rail_done || (!on_rail() && rails.stage_t > 90) || rails.stage_rail == 0) board_start(d, h);
            return;
        }
        uint32_t m[8];
        const int n = api->board_touching(m, 8);
        for (int k = 0; k < n; ++k)
            for (int i = 0; i < s.nen; ++i)
                if (en_moby(&s.en[i]) == m[k]) {  /* (one of the foes: knocked down) */
                    api->hit_send(m[k], RAC1_HIT_PLAYER, 10.0f, api->hero());
                    ++rails.rammed;
                }
    }
    if (d != NULL && d->reach > 0.0f) {  /* (the ring's place reached: on the board, on foot, landing from a jump) */
        const float dx = h[0] - rails.goal[0], dy = h[1] - rails.goal[1];
        if (dx * dx + dy * dy < d->reach * d->reach && fabsf(h[2] - rails.goal[2]) < 4.0f) {
            if (rails.stage + 1 < rails.nstages) portal_go();
            else stage_last_done();
        }
        return;
    }
    if (rails.has_goal) {
        if (dist3(h, rails.goal) < GOAL_NEAR && on_rail()) {
            if (rails.stage + 1 < rails.nstages) portal_go();
            else stage_last_done();
            return;
        }
        rails.off_rail = on_rail() ? 0 : rails.off_rail + 1;
        if (rails.off_rail > OFF_RAIL) challenge_end(0, "Ratchet e' caduto");
    }
}

static void fight_update(const float h[3]) {
    const challenge_def* c = &CHALLENGES[s.ch];
    if (c->kind == RAIL && rails.phase < 2) {
        rail_update(h);
        return;
    }
    if (rails.phase == 3 || rails.phase == 4) {
        escape_update(h);
        return;
    }
    if (rails.phase == 5) {  /* (through the portal: the switch to the section, then its task) */
        if (!rails.sec_in) portal_switch();
        else stage_update(h);
        return;
    }
    nav_track(h);
    enemies_follow(h);
    guests_nudge();
    if (c->kind == BUMP) {
        bump_update(h);
        if (s.mode != FIGHT) return;
    }
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
    if (c->kind == BUMP) return;  /* (bump_update: the target, the time) */
    if ((c->kind == TIMED && s.clock >= c->time * 60) || (s.rule.time > 0 && ++s.round_clock >= s.rule.time * 60)) {
        challenge_end(0, "tempo scaduto");
        return;
    }
    if (s.spawned >= w->count && enemies_alive() == 0) {
        if (c->escape != NULL && ((c->escape_after == 0 && rails.phase == 2 && s.wave + 1 >= c->waves) ||
                                  (c->escape_after > 0 && rails.phase == 0 && s.wave + 1 == c->escape_after))) {  /* (the lava) */
            lava_start();
            return;
        }
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
    hud_text_at(256, top + 11, YELLOW, 1.5f, C, 1, "%s", tr("ARENA - SFIDE"));
    hud_maxw(w - 32);
    for (int r = 0; r < rows; ++r) {
        const int i = list[s.scroll + r], y = list_y + step * r, chosen = s.scroll + r == s.sel;
        if (chosen) {
            hud_panel(x + 8, y - 3, w - 30, step - 2, PICK, C);
            hud_panel(x + 8, y - 3, 3, step - 2, BAR, C);
        }
        hud_panel(x + 20, y + 4, 7, 7, (s.done >> i) & 1 ? GREEN : BOX_EMPTY, C);
        hud_text_at(x + 36, y, CHALLENGES[i].kind == RAIL && !has_boots() ? GREY : chosen ? WHITE : CYAN, 1.1f, C, 0, "%s", ch_title(i));
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
    else if ((c->kind == RAIL || c->escape != NULL) && !has_boots())
        hud_text_at(256, info_y, GREY, 1.05f, C, 1, "%s", with_boots("Servono i Grindscarponi"));
    else hud_text_at(256, info_y, WHITE, 1.05f, C, 1, "%s", tr(c->text));
    hud_maxw(w - 32);
    if (s.best[ch])
        hud_text_at(256, info_y + 19, YELLOW, 1.0f, C, 1, tr("Premio %d bolts   Record %u:%02u"), c->reward, (unsigned)(s.best[ch] / 3600),
                    (unsigned)(s.best[ch] / 60 % 60));
    else
        hud_text_at(256, info_y + 19, YELLOW, 1.0f, C, 1, tr("Premio %d bolts"), c->reward);
    hud_maxw(w - 32);
    hud_text_at(256, info_y + 46, GREY, 0.95f, C, 1, "%s", tr("Croce: inizia      Cerchio: esci"));
    hud_maxw(w - 32);
    hud_text_at(256, info_y + 64, GREY, 0.95f, C, 1, "%s", tr("Quadrato: rivenditore"));
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
    s.banner_y = 0;
    s.box_rect = -1;
    s.boss_hud = 0;
    if (!s.mine) return;
    if (s.mode == FIGHT && rails.phase == 5 && rails.sec_in && rails.stage < rails.nstages &&
        (strcmp(rails.stage_name[rails.stage], "umbris") == 0 || strcmp(rails.stage_name[rails.stage], "rilgar") == 0))
        s.boss_hud = BOSS_BAR_H;  /* (the Snagglebeast's bar, Rilgar's race's row at the bottom: the HUD's row over them) */
    const uint32_t hero = api->hero();
    if (hero == 0) return;
    float h[3];
    pos_of(hero, h);
    const uint32_t pressed = api->pad_pressed();
    nav_build();
    flames_light();
    bombs_update();
    supplies_update();
    zap_update(h);
    save_update();
    ++s.t;
    switch (s.mode) {
    case FREE:
        if (s.t_center >= 0 && api->trigger_inside(s.t_center)) {
            hud_panel(150, HUD_BOTTOM - TOAST_H, 212, TOAST_H, PANEL, RAC1_HUD_CENTRE);
            hud_text_at(256, HUD_BOTTOM - TOAST_H + 5, WHITE, 1.2f, RAC1_HUD_CENTRE, 1, "%s", tr("Triangolo: menu arena"));
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
                if (!api->vendor_open()) toast(tr("Il rivenditore non si apre ora"));
            } else if (pressed & PAD_CROSS) {
                int32_t w[16];
                const challenge_def* c = &CHALLENGES[list[s.sel]];
                const int need = c->weapons == W_ROULETTE || c->weapons == W_CHAOS || c->kind == CUP ? 2 : 0;
                if ((c->kind == RAIL || c->escape != NULL) && !has_boots()) {
                    toast(with_boots("Servono i Grindscarponi"));
                } else if (need > 0 && weapons_usable(w) < need) {
                    toast(tr("Servono almeno 2 armi"));
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
        snprintf(cd, sizeof cd, tr("Si comincia tra %d"), 3 - s.t / 60);
        banner(YELLOW, ch_title(s.ch), cd);
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
            if (isc && cup[s.wave].mode != cup[s.wave - 1].mode) snprintf(next, sizeof next, tr("Prossimo: %s"), tr(MODES[cup[s.wave].mode].name));
            banner(GREEN, isc ? tr("Round superato!") : tr("Ondata superata!"), next[0] ? next : NULL);
        }
        if (s.mode != RESULT) {  /* the challenge's box, at the screen's bottom left (under the quick select's ring) */
            const challenge_def* c = &CHALLENGES[s.ch];
            const int isc = c->kind == CUP;
            char third[64] = "";  /* the mode (a championship's round), the weapon (a limited one) */
            char wl[40] = "";
            if (s.rule.weapons == W_CHAOS) snprintf(wl, sizeof wl, "%s  %d s", weapon_name(s.wpn), (s.wpn_t + 59) / 60);
            else if (s.rule.weapons == W_ROULETTE || (s.rule.weapons == W_WRENCH && !isc)) snprintf(wl, sizeof wl, "%s", weapon_name(s.wpn));
            if (isc) snprintf(third, sizeof third, "%s%s%s", tr(MODES[cup[s.wave < c->waves ? s.wave : c->waves - 1].mode].name), wl[0] ? " - " : "", wl);
            else if (wl[0]) snprintf(third, sizeof third, tr("Arma: %s"), wl);
            else if (c->kind == RAIL && rails.phase < 2 && (rails.falls > 0 || rails.kmines > 0)) {
                if (rails.use_obs && rails.kmines > 0) snprintf(third, sizeof third, tr("Mine %d/%d   Cadute %d"), rails.kmines_hit, rails.kmines, rails.falls);
                else snprintf(third, sizeof third, tr("Cadute: %d"), rails.falls);
            } else if ((rails.phase == 3 || rails.phase == 4 || (rails.phase == 5 && !rails.sec_in))) {
                snprintf(third, sizeof third, tr("Mine %d/%d   Cadute %d"), rails.kmines_hit, rails.kmines + rails.kmines_e, rails.falls);
            } else if (rails.phase == 5) {
                const struct chain_def* d = chain_of(rails.stage_name[rails.stage]);
                if (d != NULL) snprintf(third, sizeof third, "%s", tr(d->goal));
            }
            const int bh = third[0] ? 72 : 50, y = HUD_BOTTOM - bh, box = s.nrects, first = s.nlines;
            s.box_rect = box;
            hud_panel(12, y, 230, bh, PANEL, RAC1_HUD_LEFT);
            hud_panel(12, y, 3, bh, BAR, RAC1_HUD_LEFT);
            if (third[0]) hud_text_at(22, y + 50, CYAN, 1.1f, RAC1_HUD_LEFT, 0, "%s", third);
            if (c->kind == BUMP) {
                const int left = c->time * 60 + bump.bonus - s.clock;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", ch_title(s.ch));
                hud_text_at(22, y + 28, left < 600 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0, tr("Investiti %d/%d   %d:%02d"), s.felled, c->target,
                            (left > 0 ? left : 0) / 3600, (left > 0 ? left : 0) / 60 % 60);
            } else if (c->kind == SURVIVE) {
                const int left = c->time * 60 - s.clock;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", ch_title(s.ch));
                hud_text_at(22, y + 28, WHITE, 1.1f, RAC1_HUD_LEFT, 0, tr("Resisti %d:%02d   Abbattuti %d"), left / 3600, left / 60 % 60, s.felled);
            } else if (c->kind == RAIL && rails.phase < 2) {
                const int left = c->time * 60 - rails.ride;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", ch_title(s.ch));
                hud_text_at(22, y + 28, left < 600 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0, tr("Restano %d:%02d"), left / 3600, left / 60 % 60);
            } else if ((rails.phase == 3 || rails.phase == 4 || (rails.phase == 5 && !rails.sec_in))) {
                const int left = ESCAPE_TIME * 60 - rails.esc_ride;
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", ch_title(s.ch));
                hud_text_at(22, y + 28, left < 1200 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0, tr("Fuga: restano %d:%02d"), left / 3600, left / 60 % 60);
            } else if (rails.phase == 5) {  /* (the worlds beyond the portal) */
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s", ch_title(s.ch));
                hud_text_at(22, y + 28, WHITE, 1.1f, RAC1_HUD_LEFT, 0, tr("Mondo %d/%d   %d:%02d"), rails.stage + 1, rails.nstages,
                            s.clock / 3600, s.clock / 60 % 60);
            } else {
                hud_text_at(22, y + 6, YELLOW, 1.2f, RAC1_HUD_LEFT, 0, "%s  %d/%d", ch_title(s.ch), s.wave + 1, c->waves);
                const int timed = c->kind == TIMED || s.rule.time > 0;
                const int t = c->kind == TIMED ? c->time * 60 - s.clock : s.rule.time > 0 ? s.rule.time * 60 - s.round_clock : s.clock;
                hud_text_at(22, y + 28, timed && t < 600 ? RED : WHITE, 1.1f, RAC1_HUD_LEFT, 0,
                            timed ? tr("Nemici %d   restano %d:%02d") : tr("Nemici %d   %d:%02d"),
                            s.mode == FIGHT ? cur_wave()->count - s.killed : 0, t / 3600, t / 60 % 60);
            }
            hud_grow(box, first, 300);  /* (wider round long texts; smaller texts past 300) */
        }
        const float jx = h[0] - s.last_h[0], jy = h[1] - s.last_h[1], jz = h[2] - s.last_h[2];
        const int boarding = (rails.board != 0 || (RAC1_API_HAS(api, hero_mode_now) &&
                                                  api->hero_mode_now() == RAC1_HERO_BOARD));  /* (the board's own returns: a
                                                                                                 wipe-out, a fall, the race's) */
        if (s.mode != RESULT && (api->health() <= 0 || (!boarding && s.have_last && jx * jx + jy * jy + jz * jz > 64.0f)))
            challenge_end(0, "Ratchet e' caduto");  /* (dead: his health gone, or back at his start - the lava, a fall) */
        else if (s.mode != RESULT && s.t_pad >= 0 && api->trigger_inside(s.t_pad)) challenge_end(0, "abbandonata");
        break;
    case RESULT:
        if (s.won) {
            char line[48];
            if (CHALLENGES[s.ch].kind == SURVIVE)
                snprintf(line, sizeof line, tr("+%d bolts   %d abbattuti"), CHALLENGES[s.ch].reward, s.felled);
            else
                snprintf(line, sizeof line, tr("+%d bolts   %d:%02d"), CHALLENGES[s.ch].reward, s.clock / 3600, s.clock / 60 % 60);
            banner(GREEN, tr("SFIDA COMPLETATA!"), line);
        } else {
            banner(RED, tr("SFIDA FALLITA"), s.why[0] ? s.why : NULL);
        }
        if (s.t == RESULT_BACK && !in_main()) api->section_enter("main", MAIN_ENTRY, 20);  /* (beyond the portal: back) */
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
        if (s.mode != MENU) {  /* (at the bottom, over this frame's banner if there is one) */
            const int y = (s.banner_y > 0 ? s.banner_y - 6 : HUD_BOTTOM) - TOAST_H;
            s.in_msg = 1;
            hud_panel(126, y, 260, TOAST_H, PANEL, RAC1_HUD_CENTRE);
            hud_text_at(256, y + 5, YELLOW, 1.2f, RAC1_HUD_CENTRE, 1, "%s", s.toast);
            hud_maxw(MSG_W);
            hud_fit(1, 1);
            s.in_msg = 0;
        }
    }
}

static void hud(void* user) {
    (void)user;
    if (!s.mine) return;
    if (rails.phase == 5 && (api->fade_active() == 1 || api->fade_active() == 2)) return;  /* (the portal's flash: white alone) */
    float scale[15];  /* each text's scale: smaller when wider than its maxw */
    for (int i = 0; i < s.nlines; ++i) {
        scale[i] = s.line[i].scale;
        const int tw = s.line[i].maxw > 0 ? api->hud_text_width(s.line[i].text, scale[i]) : 0;
        if (tw > s.line[i].maxw) scale[i] *= (float)s.line[i].maxw / (float)tw;
    }
    int rx[32], rw[32];
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
        rx[i] = x, rw[i] = w;
    }
    /* the bottom row's messages (centred) off the challenge's box (at the left): moved right just past it, made smaller
       when they do not fit there (the HUD's width: 512 at 4:3, the LEFT elements moved (width - 512) / 2 to the left) */
    float f = 1.0f;
    int left = 9999, right = -9999, to = 0;
    for (int i = 0; i < s.nrects; ++i)
        if (s.rect[i].msg) {
            if (rx[i] < left) left = rx[i];
            if (rx[i] + rw[i] > right) right = rx[i] + rw[i];
        }
    if (s.box_rect >= 0 && left < right) {
        const int side = ((RAC1_API_HAS(api, hud_width) ? (int)api->hud_width() : 512) - 512) / 2;
        to = rx[s.box_rect] + rw[s.box_rect] - side + 8;  /* (the box's right edge, in the centre's units) */
        const int end = 504;  /* (a centred text is cut past the 4:3 frame's right edge, 512) */
        if (left >= to) to = left;
        else if (to + (right - left) > end) f = (float)(end - to) / (float)(right - left);
    }
    for (int i = 0; i < s.nrects; ++i) {
        int x = rx[i], w = rw[i];
        if (s.rect[i].msg && to != left) x = to + (int)((float)(x - left) * f), w = (int)((float)w * f);
        api->hud_rect(x, s.rect[i].y, w, s.rect[i].h, s.rect[i].colour, s.rect[i].anchor);
    }
    for (int i = 0; i < s.nlines; ++i) {
        int x = s.line[i].x;
        if (s.line[i].msg && to != left) {
            scale[i] *= f;
            x = to + (int)((float)(x - left) * f);
        }
        if (s.line[i].centred) x -= api->hud_text_width(s.line[i].text, scale[i]) / 2;
        api->hud_text_ex(x, s.line[i].y, s.line[i].colour, scale[i], s.line[i].anchor, s.line[i].text);
    }
}

/* ---- the level ---- */

static void look_up_sounds(void);

static void level_start(void* user, const rac1_level_info* level) {
    (void)user;
    s.mine = level->mine;
    s.level = level->level;
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
    guests_loaded = 0;  /* (the level's load: the imports of the challenges' waves gone) */
    nhaz = 0;
    arena_path = 0;
    if (RAC1_API_HAS(api, path_create)) {  /* (other planets' enemies' areas: the arena's floor) */
        const float c[12] = {s.centre[0] - 47, s.centre[1] - 47, nav.floor_z, s.centre[0] + 47, s.centre[1] - 47, nav.floor_z,
                             s.centre[0] + 47, s.centre[1] + 47, nav.floor_z, s.centre[0] - 47, s.centre[1] + 47, nav.floor_z};
        arena_path = api->path_create(c, 4);
    }
    cls_bomb = api->class_find("bomb");
    cls_mark = api->class_find("mark");
    for (int i = 0; i < MAX_BOMBS; ++i) bombs[i].bomb = bombs[i].mark = 0, bombs[i].flash = 0;
    for (int i = 0; i < N_SLOTS; ++i) slot_crate[i] = 0, slot_flash[i] = 0;
    zap_start();
    if (session()) api->checkpoint_set("arena");  /* (a death: back south of the hub, not at the landing) */
    rails.on = 0;  /* (the level's rails are all in the game's search again) */
    rails.phase = 0;
    rails.ring = 0;  /* (a world's ring: gone with the level's objects) */
    rails.board = 0;

    rails_all_off();
    rails.flood = api->object_named("flood");  /* (La grande fuga's lava: down under the lava, hidden) */
    if (rails.flood != 0) {
        float p[3];
        pos_of(api->object_moby(rails.flood), p);
        rails.flood_z0 = p[2];
        api->object_show(rails.flood, 0);
    }
    obstacles_find();
    if (s.lost_reload) {  /* (the challenge he died in: lost, its banner now) */
        s.lost_reload = 0;
        s.won = 0;
        snprintf(s.why, sizeof s.why, "%s", tr("Ratchet e' caduto"));
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

/* a session's soft reset (ABI 9): Ratchet back at the checkpoint with his health. The challenge he died in is lost
   (frame() has usually seen it first: his health gone) */
static void respawn(void* user, const rac1_respawn* r) {
    (void)user, (void)r;
    if (!s.mine) return;
    if (!getenv("ARENA_DOOM_KEEP")) {  /* (the Glove of Doom's agents (230) and theirs (186) the soft reset leaves in play, frozen
                                           where he died: taken out, as a level's load would; with them in play his glove fired no
                                           more and Daino's game crashed. ARENA_DOOM_KEEP: a test's, left) */
        float h[3];
        static uint32_t m[128];
        pos_of(api->hero(), h);
        static const int32_t AG[2] = {230, 186};
        int n = 0;
        for (int c = 0; c < 2; ++c) {
            const int32_t k = api->mobys_near(h, 100000.0f, AG[c], m, 128);
            for (int32_t i = 0; i < k; ++i, ++n) api->moby_remove(m[i]);
        }
        char b[64];
        snprintf(b, sizeof b, "respawn: %d of the Glove of Doom's taken out", n);
        api->log(b);
    }
    s.placed = 3;
    s.have_last = 0;
    if (s.mode == INTRO || s.mode == FIGHT || s.mode == BREAK) challenge_end(0, "Ratchet e' caduto");
    if (s.mode == RESULT && !in_main()) {  /* (dead in a world: back to the arena at once, not left in it to fall again; the world
                                               put back at its next entry: fresh) */
        api->section_enter("main", MAIN_ENTRY, 15);
        api->log("dead in a world: back to the arena");
    }
}

static void level_end(void* user) {
    (void)user;
    s.lost_reload = s.mine && (s.mode == INTRO || s.mode == FIGHT || s.mode == BREAK);
    if (s.mine) rails.on = 0;
    zap.n = 0;  /* (its rings gone with the level) */
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

/* the save: "ARN3" (ARN2 the same, but its "La grande fuga" an older one: dropped), then each challenge by its name's hash: won (0/1), its best time (frames, 0 none). By name, so a
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
    memcpy(buf, "ARN3", 4);
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
    const int v2 = memcmp(data, "ARN2", 4) == 0;  /* (ARN2: La grande fuga was another challenge, ending in Quartu) */
    if (!v2 && memcmp(data, "ARN3", 4) != 0) return;
    uint32_t count;
    memcpy(&count, data + 4, 4);
    for (uint32_t k = 0; k < count && 8 + 12 * k + 12 <= size; ++k) {
        uint32_t rec[3];
        memcpy(rec, data + 8 + 12 * k, 12);
        if (v2 && rec[0] == name_hash("La grande fuga")) continue;
        for (int i = 0; i < N_CHALLENGES; ++i)
            if (name_hash(CHALLENGES[i].name) == rec[0]) {
                if (rec[1]) s.done |= 1u << i;
                s.best[i] = rec[2];
            }
    }
    char b[64];
    int won = 0;
    for (int i = 0; i < N_CHALLENGES; ++i) won += (int)((s.done >> i) & 1u);
    snprintf(b, sizeof b, "the records loaded: %d challenges won", won);
    api->log(b);
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
   challenge N from 420, the enemies by kind, the dropships, the weapon in his hand. And: arena_rail*, arena_course*, arena_fuga* (the
   rail challenges; arena_fuga: La grande fuga, Ratchet put on the escape when the lava rises, then Batalia's rail and
   Umbris's Snagglebeast, taken to its last hit; arena_fuga_noboss: left to fight), arena_go (a world section looked round:
   ARENA_SEC, ARENA_AT...), arena_dump (a level's rails, paths and mobys in the log; ARENA_DUMP_AT with any test),
   arena_mine* (a rail mine on its own code before him: _send the plugin's hit, _bomb the Bomb Glove with RAC1_SIM_INPUT=shoot, _held held), arena_suck (the Suck Cannon's beam on the enemies, with RAC1_SIM_INPUT=hold), arena_zap (an
   electrified drop zone, Ratchet in it), arena_texts (the game's text table and the items' names) */
/* RAC1_MOD_TEST=arena_dump (any level: RAC1_GOTO=<level>): at frame 300 the level's grind rails, paths and mobys in the
   log (where a world section's rails, race track and enemies are) */
static void dump_level(void) {
    char b[256];
    float h[3];
    pos_of(api->hero(), h);
    snprintf(b, sizeof b, "dump: level %d, Ratchet at %.2f %.2f %.2f", (int)rac1_lw(api, 0x0015ed84u), h[0], h[1], h[2]);
    api->log(b);
    const uint32_t rails_at = rac1_lw(api, api->ov(0x0015f70cu));
    const int32_t nrails = (int32_t)rac1_lw(api, api->ov(0x0015f710u));
    for (int32_t i = 0; i < nrails && i < 64; ++i) {
        const uint32_t e = rails_at + 0x20u * (uint32_t)i, path = rac1_lw(api, e + 0x10);
        const int32_t n = path ? (int32_t)rac1_lw(api, path) : 0;
        float len = 0.0f;
        for (int32_t k = 0; k + 1 < n; ++k) len += rac1_lwf(api, path + 0x10 + 16u * (uint32_t)k + 12);
        snprintf(b, sizeof b, "dump rail %d: centre %.1f %.1f %.1f reach %.1f, %d points, length %.1f, loop %d", (int)i,
                 rac1_lwf(api, e), rac1_lwf(api, e + 4), rac1_lwf(api, e + 8), rac1_lwf(api, e + 12), (int)n, len,
                 (int)rac1_lw(api, e + 0x14));
        api->log(b);
        for (int32_t k = 0; k < n; ++k) {
            if (k % 4 != 0 && k != n - 1) continue;
            const uint32_t p = path + 0x10 + 16u * (uint32_t)k;
            snprintf(b, sizeof b, "dump rail %d pt %d: %.2f %.2f %.2f w %.2f", (int)i, (int)k, rac1_lwf(api, p), rac1_lwf(api, p + 4),
                     rac1_lwf(api, p + 8), rac1_lwf(api, p + 12));
            api->log(b);
        }
    }
    const uint32_t table = api->ov(0x001b04b0u);
    const int32_t npaths = (int32_t)rac1_lw(api, api->ov(0x00160104u));
    for (int32_t i = 0; i < npaths && i < 176; ++i) {
        const uint32_t path = rac1_lw(api, table + 4u * (uint32_t)i);
        const int32_t n = path ? (int32_t)rac1_lw(api, path) : 0;
        snprintf(b, sizeof b, "dump path %d: %d points", (int)i, (int)n);
        api->log(b);
        for (int32_t k = 0; k < n && k < 400; ++k) {
            const uint32_t p = path + 0x10 + 16u * (uint32_t)k;
            snprintf(b, sizeof b, "dump path %d pt %d: %.2f %.2f %.2f", (int)i, (int)k, rac1_lwf(api, p), rac1_lwf(api, p + 4),
                     rac1_lwf(api, p + 8));
            api->log(b);
        }
    }
    float g[7];  /* ARENA_PROBE=x0,y0,x1,y1,step,ztop,zbottom: the ground's height on a grid (rays down; '.' none) */
    if (getenv("ARENA_PROBE") && sscanf(getenv("ARENA_PROBE"), "%f,%f,%f,%f,%f,%f,%f", &g[0], &g[1], &g[2], &g[3], &g[4], &g[5], &g[6]) == 7)
        for (float y = g[3]; y >= g[1]; y -= g[4]) {
            static char row[4096];
            int k = snprintf(row, sizeof row, "probe y %6.1f:", y);
            for (float x = g[0]; x <= g[2] && k < (int)sizeof row - 6; x += g[4]) {
                const float from[3] = {x, y, g[5]}, to[3] = {x, y, g[6]};
                rac1_ray_hit r = {sizeof r};
                k += api->collide_ray(from, to, 0, getenv("ARENA_PROBE_MOBYS") ? RAC1_RAY_NO_LEVEL : 0u, &r)
                         ? snprintf(row + k, sizeof row - (size_t)k, " %3d", (int)lroundf(r.point[2]))
                         : snprintf(row + k, sizeof row - (size_t)k, "   .");
            }
            api->log(row);
        }
    static uint32_t m[2048];
    const int32_t nm = api->mobys_near(h, 100000.0f, -1, m, 2048);
    for (int32_t i = 0; i < nm; ++i) {
        float p[3];
        pos_of(m[i], p);
        snprintf(b, sizeof b, "dump moby %d: class %d uid %d at %.2f %.2f %.2f state %d (%08x)", (int)i, (int)rac1_lh(api, m[i] + 0xa6),
                 (int)(int16_t)rac1_lh(api, m[i] + 0xb2), p[0], p[1], p[2], (int)(int8_t)rac1_lb(api, m[i] + 0x20), (unsigned)m[i]);
        api->log(b);
    }
}

static void test(void* user, const char* text, uint64_t frame) {
    (void)user;
    {  /* (ARENA_DUMP_AT=<frame>: the dump with any test too) */
        static int dumped;
        const char* at = getenv("ARENA_DUMP_AT");
        if (!dumped && frame >= (uint64_t)(at ? atoll(at) : 300) && (at || strcmp(text, "arena_dump") == 0)) dump_level(), dumped = 1;
        if (strcmp(text, "arena_dump") == 0) return;
    }
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
    if (s.mine && strcmp(text, "arena_texts") == 0 && frame == 300) {  /* the game's text table, every record, then the
                                                                          items' names as the plugin reads them */
        for (int32_t item = 0x08; item <= 0x1d; ++item) {
            char name[48], b[96];
            snprintf(b, sizeof b, "item 0x%x: \"%s\"", (unsigned)item, item_name(item, name, sizeof name));
            api->log(b);
        }
        const uint32_t list = rac1_lw(api, api->ov(0x0015f660u));
        const int32_t n = (int32_t)rac1_lw(api, api->ov(0x0017943cu));
        for (int32_t i = 0; i < n; ++i) {
            const uint32_t str = rac1_lw(api, list + 16u * (uint32_t)i), id = rac1_lw(api, list + 16u * (uint32_t)i + 4);
            char b[200];
            int k = snprintf(b, sizeof b, "text 0x%x: ", (unsigned)id);
            for (uint32_t j = 0; str != 0 && j < 120 && k < (int)sizeof b - 5; ++j) {
                const uint8_t ch = rac1_lb(api, str + j);
                if (ch == 0) break;
                k += snprintf(b + k, sizeof b - (size_t)k, ch >= 0x20 && ch < 0x7f ? "%c" : "<%02x>", ch);
            }
            api->log(b);
        }
        return;
    }
    if (s.mine && strcmp(text, "arena_zap") == 0) {
        /* (Assedio from 420, Ratchet 9 units south of the north drop zone facing it; that zone electrified at 700 and at
           1000, Ratchet put in its middle at 1050) the zone, his health */
        if (frame == 420) challenge_start(3);
        if ((frame == 430 || frame == 1050) && zap.n > 0) {
            const float at[3] = {zap.p[0][0], zap.p[0][1] - (frame == 430 ? 9.0f : 0.0f), zap.p[0][2] + 0.5f};
            hero_move(at, 1.5708f);
        }
        if (frame == 450 || frame == 760 || frame == 830) {
            char b[160];
            float q[3] = {0, 0, 0};
            const uint32_t m = zap.n > 0 && zap.obj[0] ? api->object_moby(zap.obj[0]) : 0;
            if (m) pos_of(m, q);
            snprintf(b, sizeof b, "zap test %llu: class %d, %d zones, ring 0x%x moby 0x%x at %.2f %.2f %.2f flags 0x%x, zone at %.2f %.2f %.2f",
                     (unsigned long long)frame, (int)zap.cls, zap.n, zap.n ? (unsigned)zap.obj[0] : 0, (unsigned)m, q[0], q[1], q[2],
                     m ? (unsigned)rac1_lh(api, m + 0x34) : 0, zap.n ? zap.p[0][0] : 0, zap.n ? zap.p[0][1] : 0, zap.n ? zap.p[0][2] : 0);
            api->log(b);
        }
        if ((frame == 700 || frame == 1000) && zap.n > 0) {
            zap.phase[0] = Z_WARN;
            zap.t[0] = 0;
            zap.wait = 2000;
        }
        if (frame >= 1000 && frame <= 1300 && frame % 20 == 0) {
            char b[96];
            snprintf(b, sizeof b, "zap test %llu: zone 0 phase %d t %d, health %d, mode %d vents %d", (unsigned long long)frame, zap.phase[0], zap.t[0], (int)api->health(), s.mode, s.rule.vents);
            api->log(b);
        }
        return;
    }
    if (s.mine && strcmp(text, "arena_suck") == 0) {
        /* (Primo sangue from 420 with the Suck Cannon in hand: RAC1_SIM_INPUT=hold keeps its beam on) the enemies as the
           plugin sees them: their mobys' state, class, pvars, place */
        if (frame == 100) {
            const float at[3] = {s.centre[0], s.centre[1] - 20.0f, nav.floor_z + 0.5f};
            hero_move(at, 1.5708f);
            api->sb(0x0013d4c0u + 0x09, 1);
        }
        if (frame >= 300 && frame < 2000 && frame % 10 == 0 && rac1_lw(api, HERO_ITEM) != 0x09) api->weapon_equip(0x09);
        if (frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
        if (frame == 420) challenge_start(0);
        if (frame >= 600 && frame % 30 == 0) {
            float h[3];
            pos_of(api->hero(), h);
            char b[160];
            snprintf(b, sizeof b, "suck %llu: in hand 0x%x, killed %d felled %d, %d enemies, hero %.1f %.1f", (unsigned long long)frame,
                     (unsigned)rac1_lw(api, HERO_ITEM), s.killed, s.felled, s.nen, h[0], h[1]);
            api->log(b);
            for (int i = 0; i < s.nen; ++i) {
                const uint32_t m = en_moby(&s.en[i]);
                float r[3] = {0, 0, 0};
                if (m) pos_of(m, r);
                snprintf(b, sizeof b, "  %c %d: moby 0x%x state %d class %d update 0x%x +0x34 0x%x +0x38 0x%x at %.1f %.1f %.1f (%.1f away)",
                         KIND[s.en[i].kind].letter, i, (unsigned)m, m ? (int)(int8_t)rac1_lb(api, m + 0x20) : -99,
                         m ? (int)(int16_t)rac1_lh(api, m + 0xa6) : 0, m ? (unsigned)rac1_lw(api, m + 0x74) : 0,
                         m ? (unsigned)rac1_lw(api, m + 0x34) : 0, m ? (unsigned)rac1_lw(api, m + 0x38) : 0, r[0], r[1], r[2],
                         m ? dist3(r, h) : 0.0f);
                api->log(b);
            }
        }
        return;
    }
    if (s.mine && strcmp(text, "arena_hubwalk") == 0) {
        /* (on the hub's top by the escape's start, facing west along it, at 200: with RAC1_SIM_INPUT=forward he walks over
           where the escape's rail runs; his height every 10 frames: on the floor 3 over the arena's, not on its strip) */
        if (frame == 200) {
            float p[3];
            hub_top(p);
            hero_move(p, 3.1416f);
        }
        if (frame > 200 && frame < 420 && frame % 10 == 0) {
            float h[3];
            pos_of(api->hero(), h);
            char b[96];
            snprintf(b, sizeof b, "hubwalk %llu: hero %.2f %.2f, %.2f over the arena's floor", (unsigned long long)frame, h[0] - s.centre[0],
                     h[1] - s.centre[1], h[2] - nav.floor_z);
            api->log(b);
        }
        return;
    }
    if (s.mine && strncmp(text, "arena_mine", 10) == 0) {
        /* (a rail mine 8 ahead of Ratchet on the arena's floor, the Blaster in his hand, its state every 10 frames:
           arena_mine its own code (shot at with RAC1_SIM_INPUT=shoot), arena_mine_send the plugin's hit at 300,
           arena_mine_held held in MINE_HELD, a player's hit taken with hit_take) */
        static uint32_t mh;
        const int held = strcmp(text, "arena_mine_held") == 0;
        const int32_t gun = strcmp(text, "arena_mine_bomb") == 0 ? 0x0a : 0x0f;  /* (arena_mine_bomb: the Bomb Glove) */
        if (frame == 100) {
            const float at[3] = {s.centre[0], s.centre[1] - 20.0f, nav.floor_z + 0.5f};
            hero_move(at, 1.5708f);
            api->sb(0x0013d4c0u + (uint32_t)gun, 1);
            const float p[3] = {at[0], at[1] + 8.0f, nav.floor_z + 0.5f};
            mh = api->base_spawn(RAIL_MINE, p, 0.0f);
        }
        if (frame >= 110 && frame < 1500 && frame % 10 == 0 && rac1_lw(api, HERO_ITEM) != (uint32_t)gun) api->weapon_equip(gun);
        const uint32_t m = mh ? api->base_moby(mh) : 0;
        if (held && m != 0 && (int8_t)rac1_lb(api, m + 0x20) == 1) api->sb(m + 0x20, MINE_HELD);
        rac1_hit hit = {sizeof hit};
        if (held && m != 0 && api->hit_take(m, RAC1_HIT_PLAYER, &hit)) {
            char b[120];
            snprintf(b, sizeof b, "mine %llu: a hit taken, kinds %#x strength %.2f from %08x", (unsigned long long)frame, (unsigned)hit.kinds,
                     (double)hit.strength, (unsigned)hit.from);
            api->log(b);
        }
        if (frame == 300 && m != 0 && strcmp(text, "arena_mine_send") == 0) {
            api->hit_send(m, RAC1_HIT_PLAYER, 1.0f, api->hero());
            api->log("mine 300: the plugin's hit sent");
        }
        if (frame >= 100 && frame % 10 == 0) {
            float q[3] = {0, 0, 0};
            if (m != 0) pos_of(m, q);
            char b[160];
            snprintf(b, sizeof b, "mine %llu: moby %08x state %d flags %#x at %.2f %.2f %.2f, health %d, in hand %#x", (unsigned long long)frame, (unsigned)m,
                     m ? (int)(int8_t)rac1_lb(api, m + 0x20) : -1, m ? (unsigned)rac1_lh(api, m + 0x34) : 0u, (double)q[0], (double)q[1], (double)q[2],
                     (int)api->health(), (unsigned)rac1_lw(api, HERO_ITEM));
            api->log(b);
        }
        return;
    }
    if (strcmp(text, "arena_go") == 0) {  /* (on any level: RAC1_GOTO=<level> for a planet itself, ARENA_AT, ARENA_EVERY) */
        /* (a look round a section: ARENA_SEC=<section> ARENA_ENTRY=<entry> entered at 600; ARENA_AT="<frame>:<x>,<y>,<z>,<yaw>;..."
           Ratchet put there at those frames; ARENA_BOOTS=1 the Grindscarponi; ARENA_RAIL=<n>@<frame>:<distance> him on
           level.txt's rail <n>; where he is every second) */
        const char* sec = getenv("ARENA_SEC");
        const char* entry = getenv("ARENA_ENTRY");
        if (frame == 100 && getenv("ARENA_BOOTS")) api->sb(GRIND_BOOTS, 1);
        if (getenv("ARENA_BACK") && frame == (uint64_t)atoll(getenv("ARENA_BACK"))) {  /* (back to main) */
            char b[64];
            snprintf(b, sizeof b, "test: back to main: %d", api->section_enter("main", MAIN_ENTRY, 0));
            api->log(b);
        }
        if (frame == 600 && sec) {
            char b[96];
            snprintf(b, sizeof b, "test: into %s at %s: %d", sec, entry ? entry : "", api->section_enter(sec, entry ? entry : "", 0));
            api->log(b);
        }
        for (const char* a = getenv("ARENA_AT"); a && *a;) {
            unsigned long long f = 0;
            float p[3], yaw = 0.0f;
            if (sscanf(a, "%llu:%f,%f,%f,%f", &f, &p[0], &p[1], &p[2], &yaw) == 5 && f == frame) {
                hero_move(p, yaw);
                api->log("test: Ratchet put");
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        for (const char* a = getenv("ARENA_CAM"); a && *a;) {  /* ("<frame>:<x>,<y>,<z>,<yaw>,<pitch>;...": a fixed camera) */
            unsigned long long f = 0;
            rac1_camera c = {sizeof c};
            if (sscanf(a, "%llu:%f,%f,%f,%f,%f", &f, &c.pos[0], &c.pos[1], &c.pos[2], &c.yaw, &c.pitch) == 6 && f == frame) {
                c.mode = RAC1_CAMERA_FIXED;
                api->camera_set(&c);
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        for (const char* a = getenv("ARENA_LOOK"); a && *a;) {  /* ("<frame>:<class>;...": the camera on a moby of that class) */
            unsigned long long f = 0;
            int cls = 0;
            if (sscanf(a, "%llu:%d", &f, &cls) == 2 && f == frame) {
                float h[3];
                pos_of(api->hero(), h);
                uint32_t m = 0;
                if (api->mobys_near(h, 400.0f, cls, &m, 1) == 1) {
                    rac1_camera c = {sizeof c};
                    float p[3];
                    pos_of(m, p);
                    c.mode = RAC1_CAMERA_LOOK;
                    c.pos[0] = p[0] - 5.0f, c.pos[1] = p[1] - 5.0f, c.pos[2] = p[2] + 3.5f;
                    c.moby = m;
                    api->camera_set(&c);
                    char b[96];
                    snprintf(b, sizeof b, "test: the camera on class %d at %.1f %.1f %.1f", cls, p[0], p[1], p[2]);
                    api->log(b);
                }
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        for (const char* a = getenv("ARENA_HIT"); a && *a;) {  /* ("<frame>:<class>;...": the player's hit on the nearest of a class) */
            unsigned long long f = 0;
            int cls = 0;
            if (sscanf(a, "%llu:%d", &f, &cls) == 2 && f == frame) {
                float h[3];
                uint32_t m = 0;
                pos_of(api->hero(), h);
                if (api->mobys_near(h, 200.0f, cls, &m, 1) == 1) api->hit_send(m, RAC1_HIT_PLAYER, 10.0f, api->hero());
                char b[96];
                snprintf(b, sizeof b, "test: hit on class %d (moby %08x), state %d", cls, (unsigned)m, m ? (int)(int8_t)rac1_lb(api, m + 0x20) : -9);
                api->log(b);
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        for (const char* a = getenv("ARENA_IMPORT"); a && *a;) {  /* ("<frame>:<name>;...": import_load, "-<name>" import_release) */
            unsigned long long f = 0;
            char n[48];
            if (sscanf(a, "%llu:%47[^;]", &f, n) == 2 && f == frame) {
                char b[96];
                if (n[0] == '-') snprintf(b, sizeof b, "test: released %s: %d", n + 1, (int)api->import_release(n + 1));
                else snprintf(b, sizeof b, "test: loaded %s: %d", n, (int)api->import_load(n));
                api->log(b);
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        for (const char* a = getenv("ARENA_SPAWN"); a && *a;) {  /* ("<frame>:<level>:<class>:<x>,<y>,<z>;...": an import) */
            unsigned long long f = 0;
            int lv = 0, cl = 0;
            float p[3];
            if (sscanf(a, "%llu:%d:%d:%f,%f,%f", &f, &lv, &cl, &p[0], &p[1], &p[2]) == 6 && f == frame) {
                char n[16], b[96];
                snprintf(n, sizeof n, "%d:%d", lv, cl);
                const int32_t c = api->import_class(n);
                const uint32_t m = c >= 0 ? api->import_spawn(c, p, 0.0f) : 0;
                snprintf(b, sizeof b, "test: %s spawned: class %d, moby %08x", n, (int)c, (unsigned)m);
                api->log(b);
            }
            a = strchr(a, ';');
            if (a) ++a;
        }
        if (getenv("ARENA_VENDOR") && frame == (uint64_t)atoll(getenv("ARENA_VENDOR"))) {  /* (the vendor's screens) */
            char b[48];
            snprintf(b, sizeof b, "test: the vendor: %d", api->vendor_open());
            api->log(b);
        }
        if (getenv("ARENA_GIANT") && frame == (uint64_t)atoll(getenv("ARENA_GIANT"))) {  /* (Giant Clank: Quartu's, 15:419) */
            float h[3];
            pos_of(api->hero(), h);
            const int32_t c = api->import_load("15:419");
            const uint32_t m = c >= 0 ? api->import_spawn(c, h, rac1_lwf(api, api->hero() + 0x48)) : 0;
            const int b = api->hero_build(15);
            const int on = m ? api->hero_mode(RAC1_HERO_GIANT_CLANK, m) : -1;
            char b2[96];
            snprintf(b2, sizeof b2, "test: Giant Clank: class %d moby %08x build %d mode %d", (int)c, (unsigned)m, b, on);
            api->log(b2);
        }
        if (getenv("ARENA_BOARD") && frame == (uint64_t)atoll(getenv("ARENA_BOARD"))) {  /* (the hoverboard: Kalebo III's) */
            float h[3];
            pos_of(api->hero(), h);
            const int b16 = api->hero_build(16);
            const int32_t c = api->import_class("16:439");
            uint32_t m = c >= 0 ? api->import_spawn(c, h, rac1_lwf(api, api->hero() + 0x48)) : 0;
            if (m == 0) {  /* (the base's own class: level.txt's spawn 439 1) */
                const uint32_t bh = api->base_spawn(439, h, rac1_lwf(api, api->hero() + 0x48));
                m = bh ? api->base_moby(bh) : 0;
            }
            const int on = m ? api->hero_mode(RAC1_HERO_BOARD, m) : -1;
            char b[128];
            snprintf(b, sizeof b, "test: the board: hero_build %d, class %d, moby %08x, hero_mode %d", b16, (int)c, (unsigned)m, on);
            api->log(b);
        }
        if (getenv("ARENA_RAIL")) {
            char name[32];
            unsigned long long f = 0;
            float d = 0.0f;
            if (sscanf(getenv("ARENA_RAIL"), "%31[^@]@%llu:%f", name, &f, &d) == 3 && f == frame) {
                char b[96];
                snprintf(b, sizeof b, "test: on the rail %s at %.1f: %d", name, d, api->hero_rail_attach(api->rail_find(name), d, 1));
                api->log(b);
            }
        }
        const int every = getenv("ARENA_EVERY") ? atoi(getenv("ARENA_EVERY")) : 60;  /* (the line's frames apart) */
        if (frame % (uint64_t)(every > 0 ? every : 60) == 0) {
            float h[3], cp[3];
            pos_of(api->hero(), h);
            api->camera_position(cp);
            rac1_rail_info ri = {sizeof ri};
            const int g = api->hero_rail(&ri);
            char b[240];
            snprintf(b, sizeof b, "go %llu: hero %.2f %.2f %.2f state 0x%x health %d mode %d%s rail %d seg %d, camera %.2f %.2f %.2f",
                     (unsigned long long)frame, h[0], h[1], h[2], (unsigned)rac1_lw(api, HERO_STATE), (int)api->health(),
                     (int)api->hero_mode_now(), g ? " grinding" : "", g ? (int)ri.rail : -1, g ? (int)ri.segment : -1, cp[0], cp[1], cp[2]);
            api->log(b);
        }
        return;
    }
    const int course_t = strncmp(text, "arena_course", 12) == 0, fuga_t = strncmp(text, "arena_fuga", 10) == 0;
    if (s.mine && (strncmp(text, "arena_rail", 10) == 0 || course_t || fuga_t)) {
        /* a rail challenge from 420 (arena_course* / arena_rail*: the course, arena_fuga*: La grande fuga), the
           Grindscarponi given; Ratchet dropped onto the first rail's third point at 640 (arena_coursewalk: left on the
           station, for a RAC1_SIM_INPUT script); his health kept up; the arena's waves: an enemy taken away every 1.5 s.
           arena_fuga: in Umbris the Snagglebeast taken to its last hit 300 frames after he comes in (arena_fuga_noboss:
           not) */
        const char* want = fuga_t ? "La grande fuga" : "Grindscarponi: Discesa";
        int ch = -1;
        for (int i = 0; i < N_CHALLENGES; ++i)
            if (strcmp(CHALLENGES[i].name, want) == 0) ch = i;
        if (frame == 100) api->sb(GRIND_BOOTS, 1);  /* (a test's game has no Grindscarponi) */
        if (frame == 420 && ch >= 0) challenge_start(ch);
        /* (arena_fuga_twice: the challenge again 300 frames after its first end, back in the arena: what a try leaves behind
           must not stop the next one) */
        static int tries = 1;
        static uint64_t again_at, q_at, ril_at, bat_at, boss_at, esc_at;
        if (strstr(text, "twice") != NULL && tries == 1 && s.mode == FREE && frame > 1000 && in_main() && again_at == 0) again_at = frame + 300;
        if (again_at != 0 && frame == again_at && ch >= 0) {
            tries = 2;
            q_at = ril_at = bat_at = boss_at = esc_at = 0;
            api->log("test: the challenge again");
            challenge_start(ch);
        }
        float a[3], d[3];
        if ((frame == 640 || (again_at != 0 && frame == again_at + 220)) && strstr(text, "walk") == NULL && rails.nset > 0 &&
            api->rail_point(rails.set[0], 3.0f, a, d)) {
            const float at[3] = {a[0], a[1], a[2] + 1.5f};
            hero_move(at, atan2f(d[1], d[0]));
            api->log("test: Ratchet dropped over the first rail, 3 units along it");
        }
        s.test_log = 1;
        static int32_t hp_was = -1;  /* (arena_course_nh: his health not kept up; each change logged) */
        if (strstr(text, "_nh") == NULL && frame % 30 == 0 && api->health() > 0 && api->health() < 4) api->sw(0x001415f8u, 4);
        if (api->health() != hp_was) {
            char b[96];
            snprintf(b, sizeof b, "test %llu: health %d (was %d), state 0x%x", (unsigned long long)frame, (int)api->health(), (int)hp_was,
                     (unsigned)rac1_lw(api, HERO_STATE));
            if (hp_was >= 0) api->log(b);
            hp_was = api->health();
        }
        /* (arena_fuga: on Quartu's board 360 frames, then put by the ring - it does not steer) */
        if (rails.phase == 5 && rails.sec_in && strcmp(rails.stage_name[rails.stage], "quartu") == 0 && rails.stage_step == 1 && q_at == 0)
            q_at = frame + 360;
        if (fuga_t && strstr(text, "ride") == NULL && frame == q_at) {
            const float at[3] = {192.0f, 235.0f, 26.5f};
            hero_move(at, 0.0f);
            api->log("test: Ratchet put by Quartu's ring");
        }
        /* (arena_fuga: on Rilgar's board 300 frames, then put on the crates' platform) */
        if (rails.phase == 5 && rails.sec_in && strcmp(rails.stage_name[rails.stage], "rilgar") == 0 && ril_at == 0) ril_at = frame + 300;
        if (fuga_t && strstr(text, "ride") == NULL && frame == ril_at) {
            const float at[3] = {264.0f, 438.0f, 81.0f};
            hero_move(at, 0.24f);
            api->log("test: Ratchet put on Rilgar's platform");
        }
        /* (arena_fuga: on Batalia's rail, Ratchet put past its tower at its middle - he does not jump
                                    its obstacles; arena_fuga_ride: left to ride it all) */
        if (rails.phase == 5 && rails.sec_in && strcmp(rails.stage_name[rails.stage], "batalia") == 0 && bat_at == 0) {
            bat_at = frame + 60;
            float h[3];
            uint32_t mm[32];
            pos_of(api->hero(), h);
            const int32_t mc = api->import_class("8:621");
            char b[80];
            snprintf(b, sizeof b, "test: Batalia's mines in play: %d", mc >= 0 ? (int)api->mobys_near(h, 400.0f, mc, mm, 32) : -1);
            api->log(b);
        }
        if (fuga_t && strstr(text, "ride") == NULL && frame == bat_at) {
            const float at[3] = {378.34f, 170.07f, 59.8f};
            hero_move(at, 0.27f);
            api->log("test: Ratchet put on Batalia's rail past its tower");
        }
        /* (arena_fuga: in Umbris, the boss to its last hit) */
        if (rails.phase == 5 && rails.sec_in && strcmp(rails.stage_name[rails.stage], "umbris") == 0 && boss_at == 0) boss_at = frame + 300;
        if (fuga_t && strstr(text, "die") != NULL && frame == boss_at) {  /* (arena_fuga_die: Ratchet dies in Umbris) */
            api->sw(0x001415f8u, 1);
            api->hero_damage(1, 0);
            api->log("test: Ratchet dies in Umbris");
        }
        if (fuga_t && strstr(text, "noboss") == NULL && strstr(text, "die") == NULL && frame == boss_at &&
            !(tries == 2 && strstr(text, "watch") != NULL)) {  /* (arena_fuga_twice_watch: the second boss left alone) */
            float h[3];
            uint32_t m = 0;
            pos_of(api->hero(), h);
            const int32_t c = api->import_class("umbris.snagglebeast");
            const uint32_t pv = c >= 0 && api->mobys_near(h, 200.0f, c, &m, 1) == 1 ? rac1_lw(api, m + 0x78) : 0;
            if (pv != 0) {  /* (as the open engine's s10_kill: its state 14, its health 0.5, one hit pending) */
                api->sb(m + 0x20, 14);
                api->swf(pv + 0x20, 0.5f);
                api->sw(pv + 0x164, 1);
                api->sw(pv + 0x160, 5);
            }
            char b[96];
            snprintf(b, sizeof b, "test: the Snagglebeast %08x taken to its last hit", (unsigned)m);
            api->log(b);
        }
        if (rails.phase == 2 && s.mode == FIGHT && frame % 90 == 5 && s.nen > 0) {
            const uint32_t m = en_moby(&s.en[0]);
            if (m != 0) api->moby_remove(m);
        }
        /* (arena_fuga: the lava's start; two seconds later Ratchet put on the escape) */
        if (rails.phase == 3 && esc_at == 0) esc_at = frame + 120;
        if (fuga_t && strstr(text, "walk") == NULL && frame == esc_at && rails.escape != 0) {
            char b[64];
            s.placed = 3;  /* (moved: not a death) */
            s.have_last = 0;
            snprintf(b, sizeof b, "test: Ratchet put on the escape: %d", api->hero_rail_attach(rails.escape, 3.0f, 1));
            api->log(b);
        }
        const int riding = s.mode == FIGHT && (rails.phase == 1 || rails.phase == 4);
        if (frame >= 420 && frame <= 9000 && api->hero() != 0 && rails.nset > 0 && (riding ? frame % 6 == 0 : frame % 60 == 0)) {
            float h[3];
            pos_of(api->hero(), h);
            rac1_rail_info ri = {sizeof ri};
            char on[24] = "-";
            if (api->hero_rail(&ri) && !api->rail_info(ri.rail, on, sizeof on, NULL, NULL)) snprintf(on, sizeof on, "?");
            int alive = 0;
            for (int i = 0; i < rails.nobs; ++i) alive += rails.obs[i].alive;
            char b[240];
            snprintf(b, sizeof b, "test %llu: mode %d phase %d, hero %.2f %.2f %.2f, state 0x%x, rail %s at %.1f of %.1f, health %d, obstacles %d, rail mines %d/%d, falls %d, on %d, alive %d",
                     (unsigned long long)frame, s.mode, rails.phase, h[0], h[1], h[2], (unsigned)rac1_lw(api, HERO_STATE), on,
                     on[0] == '-' ? 0.0f : ri.distance, on[0] == '-' ? 0.0f : ri.length, (int)api->health(), alive, rails.kmines_hit,
                     rails.kmines, rails.falls, rails.on, s.nen);
            api->log(b);
            float near_d = 1e9f, near_q[3] = {0, 0, 0};  /* (the nearest rail mine out: where, how far from his middle) */
            for (int i = 0; i < rails.nobs; ++i) {
                const uint32_t km = rails.obs[i].alive && rails.obs[i].kind == O_KMINE ? kmine_moby(i) : 0;
                if (km == 0) continue;
                float q[3];
                pos_of(km, q);
                const float mid[3] = {h[0], h[1], h[2] + 0.9f}, dm = dist3(mid, q);
                if (dm < near_d) near_d = dm, memcpy(near_q, q, sizeof q);
            }
            if (near_d < 6.0f) {
                snprintf(b, sizeof b, "  rail mine at %.2f %.2f %.2f, %.2f from his middle", near_q[0], near_q[1], near_q[2], near_d);
                api->log(b);
            }
            if (rails.phase == 5) {  /* (beyond the portal: the world, its goal) */
                snprintf(b, sizeof b, "  world %d/%d %s, in %d, its goal %.1f away, off its rail %d, step %d, rammed %d, foes %d, mode %d",
                         rails.stage + 1, rails.nstages, rails.stage < rails.nstages ? rails.stage_name[rails.stage] : "-", rails.sec_in,
                         rails.has_goal ? dist3(h, rails.goal) : -1.0f, rails.off_rail, rails.stage_step, rails.rammed, s.nen,
                         (int)api->hero_mode_now());
                api->log(b);
            }
        }
        return;
    }
    if (s.mine && strcmp(text, "arena_doom") == 0) {
        /* (the Glove of Doom's agents and a death in the session: given and in hand from 100, RAC1_SIM_INPUT=shoot; at 700
           his health to 1 and a hit: the soft reset; what is in play near him every second) */
        if (frame == 100) {
            api->sb(0x0013d4c0u + 0x14, 1);
            api->ammo_fill(0x14);
        }
        if (frame >= 110 && frame % 10 == 0 && rac1_lw(api, HERO_ITEM) != 0x14) api->weapon_equip(0x14);
        if (frame == 700) {
            api->sw(0x001415f8u, 1);
            api->hero_damage(1, 0);
            api->log("test: Ratchet killed");
        }
        if (frame == 800) api->ammo_fill(0x14);
        if (frame >= 300 && frame % 60 == 0) {
            float h[3];
            uint32_t m[64];
            pos_of(api->hero(), h);
            const int32_t n = api->mobys_near(h, 80.0f, -1, m, 64);
            char b[400];
            int k = snprintf(b, sizeof b, "doom %llu: state 0x%x item 0x%x health %d, near:", (unsigned long long)frame, (unsigned)rac1_lw(api, HERO_STATE),
                             (unsigned)rac1_lw(api, HERO_ITEM), (int)api->health());
            for (int32_t i = 0; i < n && k < (int)sizeof b - 16; ++i)
                k += snprintf(b + k, sizeof b - (size_t)k, " %d/%d", (int)rac1_lh(api, m[i] + 0xa6), (int)(int8_t)rac1_lb(api, m[i] + 0x20));
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
        if (frame == 100 && ch >= 0 && ch < N_CHALLENGES && CHALLENGES[ch].escape != NULL) api->sb(GRIND_BOOTS, 1);
        if (frame == 420 && ch >= 0 && ch < N_CHALLENGES) challenge_start(ch);
        static uint64_t esc2_at;  /* (a challenge with an escape: Ratchet put on it two seconds into the lava) */
        if (rails.phase == 3 && esc2_at == 0) esc2_at = frame + 120;
        if (rails.phase != 3 && rails.phase != 4) esc2_at = 0;
        if (esc2_at != 0 && frame == esc2_at && rails.escape != 0) {
            s.placed = 3;
            s.have_last = 0;
            char b[64];
            snprintf(b, sizeof b, "test: Ratchet put on the escape: %d", api->hero_rail_attach(rails.escape, 3.0f, 1));
            api->log(b);
        }
        static uint64_t bat2_at;  /* (on Batalia's rail: put past its tower, as arena_fuga does) */
        if (rails.phase == 5 && rails.sec_in && strcmp(rails.stage_name[rails.stage], "batalia") == 0 && bat2_at == 0) bat2_at = frame + 60;
        if (rails.phase != 5) bat2_at = 0;
        if (bat2_at != 0 && frame == bat2_at) {
            const float at[3] = {378.34f, 170.07f, 59.8f};
            hero_move(at, 0.27f);
            api->log("test: Ratchet put on Batalia's rail past its tower");
        }
        if (frame % 60 == 0 && rails.phase == 5) {
            float h[3];
            pos_of(api->hero(), h);
            char b[200];
            snprintf(b, sizeof b, "test %llu: world %s, in %d, hero %.1f %.1f %.1f state 0x%x, its goal %.1f away, off its rail %d, health %d",
                     (unsigned long long)frame, rails.stage < rails.nstages ? rails.stage_name[rails.stage] : "-", rails.sec_in, h[0], h[1], h[2],
                     (unsigned)rac1_lw(api, HERO_STATE), rails.has_goal ? dist3(h, rails.goal) : -1.0f, rails.off_rail, (int)api->health());
            api->log(b);
        }
        if (kill && frame % (uint64_t)kill == 5 && s.mode == FIGHT && s.nen > 0) {
            const uint32_t m = en_moby(&s.en[0]);
            if (m != 0) api->moby_remove(m);
        }
        if (frame % 120 == 0 || (frame >= 840 && frame <= 1260 && frame % 30 == 0)) {  /* (often round a quick select test's) */
            int by[N_KINDS] = {0};
            for (int i = 0; i < s.nen; ++i) ++by[s.en[i].kind];
            float h[3];
            pos_of(api->hero(), h);
            char b[480];
            float e0[3] = {0.0f, 0.0f, 0.0f};
            if (s.nen > 0 && en_moby(&s.en[0]) != 0) pos_of(en_moby(&s.en[0]), e0);
            snprintf(b, sizeof b, "test %llu: clock %d, enemy 0 at %.2f %.2f, mode %d wave %d phase %d spawned %d killed %d felled %d alive %d: R%d B%d L%d C%d F%d J%d G%d Y%d W%d A%d K%d T%d P%d, ships %d %d, in hand 0x%x (asked 0x%x), health %d, hero %.1f %.1f %.1f",
                     (unsigned long long)frame, s.clock, e0[0], e0[1], s.mode, s.wave, rails.phase, s.spawned, s.killed, s.felled, s.nen, by[K_ROBOT], by[K_BOMBER], by[K_LAUNCHER], by[K_CLUB], by[K_FLAMER],
                     by[K_JET], by[K_GUNSHIP], by[K_GEMLIK], by[K_GASPAR], by[K_AMOEBOID], by[K_BLARG], by[K_TURRET], by[K_SPORE],
                     s.ships[0] && api->base_moby(s.ships[0]) ? 1 : 0, s.ships[1] && api->base_moby(s.ships[1]) ? 1 : 0,
                     (unsigned)rac1_lw(api, HERO_ITEM), (unsigned)s.wpn, (int)api->health(), h[0], h[1], h[2]);
            api->log(b);
        }
        if (frame % 240 == 0)
            for (int i = 0; i < s.nen; ++i) {
                const uint32_t m = en_moby(&s.en[i]);
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
        if (frame == 100) api->sb(GRIND_BOOTS, 1);  /* (the rail challenges in the list as for one who has them) */
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
        const uint32_t m = en_moby(&s.en[0]);
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
            const uint32_t m = en_moby(e);
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
    if (RAC1_API_HAS(api, session_on)) mod->respawn = respawn;
    if (RAC1_API_HAS(api, section_enter)) mod->section_enter = section_entered, mod->section_leave = section_left;
    if (RAC1_API_HAS(api, boss_set)) mod->boss = boss_event;
    if (rails_sdk()) mod->rail = rail_event;
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
    texts_check();
    api->log("the arena's plugin is on: the menu, the challenges, Kalebo III's enemies and their paths, the weapons, the flame vents");
    return 0;
}
