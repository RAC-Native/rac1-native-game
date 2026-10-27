// The original runtime: the game's own start on a level and its main loop, ported
// (src/game/ovl), with the host standing in for the hardware: the disc (the user's ISO), the pad
// (keyboard / gamepad -> the controller packet), the vsync (60 Hz pacing) and the GS / VU (the
// host renderer draws the level's static geometry and the game's mobys from the game's own
// structures, with the game's view).
//
// rac1-native [--level N] (default 0, Veldin; needs assets from setup.sh)
// The game's own start (the intro movie, the title, its main menu; New Game then
//   plays the story's cards and movies before Veldin) runs in a window by default; a headless run,
//   --level / RAC1_LEVEL / RAC1_GOTO / RAC1_ARRIVE or RAC1_BOOT=0 (or the debug menu's "Start with the
//   game's boot" off) start directly on the level as before. RAC1_BOOT=1 forces it (also headless).
//   RAC1_NO_MOVIES=1 (testing: the movies skipped, as without a decoder)
//   RAC1_LEVEL=N (testing: start on level N)  RAC1_GOTO=N@F[,N2@F2..] (testing: the game's level change to N at frame F,
//   as the debug menu's Levels tab asks it; during the boot (RAC1_BOOT=1):
//   its title left for level N, the debug menu's skip)
//   RAC1_LANGUAGE=0|2|3|4|5 (testing: the language)  RAC1_HEADLESS=1  RAC1_FRAMES=N  RAC1_SIM_INPUT=forward|jump|dive|turn|attack|throw|hud|quick|fire|shoot|hold|holdwalk|look|start|menu:<keys>|title:<keys>|ship|shipskip|vendor|seq:<script> (sim_script.h)   (testing)
//   RAC1_AUDIO_DUMP=file.raw (the sound output, 48 kHz stereo s16; RAC1_AUDIO_CHANNELS=6 / 8: 5.1 / 7.1, interleaved)  RAC1_NO_AUDIO=1  RAC1_SND_TRACE=1
// RAC1_FADES=original|fast (the screen fade shown frame by frame as on the PS2, or run in one host frame: the default)
// RAC1_HOST_DELAY_MS=n, RAC1_STEPS=n, RAC1_AUDIO_DEVICE=1 (testing: each host frame n ms longer; n game frames
//     per host frame in a headless run (a window's catch-up); the audio device opened in a headless run: the game's state
// must not change with any of them)
// RAC1_PACING=0 (the old present: swap, then the cap's sleep),
//   RAC1_PACE_GAME=0 (host frames at the cap even above the game's 60), RAC1_PRIORITY=0|1 (rtkit's real-time audio thread and
//   nice -10 game threads; default on in a window), RAC1_AUDIO_QUEUE=0354 (the earlier queue policy, for comparison),
//   RAC1_AUDIO_LOG=1 (the queue's summary every 10 s), RAC1_AUDIO_PERIOD_MS / RAC1_AUDIO_PERIODS (the device's buffer),
//   RAC1_FRAME_PROF_LONG=ms (with RAC1_FRAME_PROF: the host frames longer than that, stage by stage; default 30), a headless
//   run with RAC1_FPS_CAP=n paced at n (testing the audio device at 60 fps), RAC1_REALTIME=1 (a headless run stepped on the
//   wall clock as a window; with RAC1_AUDIO_DEVICE=null, the null sound device; RAC1_DEVICE_DUMP=file what the device plays)
// ADR 0013: RAC1_IOP_MIRROR=0 (the game's IOP's output played, not
//   the host's mirror of the IOP), RAC1_IOP_VERIFY=1 (the mirror never alone, each of its vsyncs compared with the game's
//   IOP's mix: counted at exit), RAC1_LOAD_PROF=1 (each load's steps timed: "[load]" lines)
//   RAC1_MUSIC_ON=1 (testing: the level music's pause lifted at the start (0x265860), as the game does when a menu
//   closes or a landing ends; Veldin's first visit starts with it paused until its opening cutscene (scene 4,
//   game mode 2) ends: 0x298b18 -> 0x265880)
//   RAC1_ARRIVE=1 (testing: the ship's map choosing this planet at the start (0x28ed58): the landing, game mode 6)
//   RAC1_SHOTS=f1,f2,.. (testing: screenshots shot_<f>.ppm at those frames)
//   RAC1_CLOCK=<unix seconds> (testing: the console clock fixed: runs through a level change become deterministic;
// headless runs default to 1700000000 and ignore physical gamepads)
//   RAC1_BOLTS=n (testing: the bolt count set at RAC1_GIVE_FRAME, default 60)
// RAC1_UNLOCK_PLANETS=<l>,.. (testing: planets the ship can fly to); RAC1_HERO_AT=<class>@<frame>[,dx,dy,dz[,yaw]] (testing: the hero put next to the first moby of that class by the game's placement 0x216f90; 834 = Veldin's ship;
//   class -1: at x,y,z itself)
//   RAC1_VIEW_AT=ex,ey,ez,ax,ay,az (testing: the host view fixed at that eye looking at that point)
//   RAC1_MRP=1 (the Modern Rendering Pipeline for the mobys, the world and the sky: RAC1_MOBY_MRP=1 + RAC1_WORLD_MRP=1; either set
// explicitly decides for its part)
#include <runtime/ee/ps2_fpu.h>
#include <algorithm>
#include <array>
#include <cmath>
#include <glad/glad.h>
#include <GLFW/glfw3.h>

#include <chrono>
#include <ctime>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <map>
#include <thread>
#include <malloc.h>
#include <set>
#include <sstream>
#include <optional>
#include <vector>

#include <platform/platform.h>
#if defined(__SANITIZE_ADDRESS__)
#include <sanitizer/lsan_interface.h>
#endif

#include "host_language.h"

#include <game/enhance/advanced.h>
#include <game/enhance/controls.h>
#include <game/enhance/graphics.h>
#include <game/boot/front_end.h>
#include <game/boot/front_image.h>
#include <game/boot/level_read.h>
#include <game/lib/libcdvd.h>
#include <game/ovl/arrival.h>
#include <game/ovl/cutscene.h>
#include <game/ovl/level_change.h>
#include <game/ovl/ovl.h>
#include <game/ovl/ovl_common.h>
#include <game/ovl/particles_vu1.h>
#include <game/ovl/shadows_vu1.h>
#include <game/ovl/moby_pass.h>
#include <game/ovl/moby_vu0.h>
#include <game/ovl/world_light_vu0.h>
#include <game/ovl/moby_vu1.h>
#include <game/ovl/world_vu1.h>
#include <runtime/vu/vu_prof.h>
#include <runtime/vu/vu0.h>
#include <runtime/vu/vu1.h>
#include <game/ovl/ovl_reloc.h>
#include <game/render/moby_render.h>
#include <game/render/orig_moby_draw.h>
#include <game/render/moby_gpu_renderer.h>
#include <game/render/mrp_scene.h>
#include <game/render/mrp_lighting.h>
#include <game/render/mrp_particles.h>
#include <game/render/mrp_moby_renderer.h>
#include <game/render/mrp_shadows.h>
#include <game/render/mrp_2d.h>
#include <game/render/mrp_chain.h>
#include <game/render/mrp_fx.h>
#include <game/render/mrp_lod.h>
#include <game/render/mrp_world_renderer.h>
#include <game/render/mrp_world_scene.h>
#include <setup/setup_manager.h>
#include <formats/mrp_asset_check.h>
#include <game/render/render_toggles.h>
#include <game/render/render_view.h>
#include <game/render/shader_manager.h>
#include <game/render/sky_render.h>
#include <game/render/title_scene_host.h>
#include <game/render/mrp_title_view.h>
#include <game/render/tfrag_render.h>
#include <game/render/texture_loader.h>
#include <game/lib/libgraph.h>
#include <game/lib/snd989.h>
#include <game/lib/libmc.h>
#include <game/ovl/memory_card_ops.h>
#include <runtime/gs/gs_draw.h>
#include <runtime/gs/gs_emu.h>
#include <runtime/gs/static_geom_cache.h>
#include <runtime/vk/vk_context.h>
#include <runtime/iop/snd989_iop.h>
#include <runtime/iop/snd989_stream.h>
#include <runtime/iop/stash_iop.h>
#include <runtime/ipu/movie_hw.h>
#include <runtime/kernel/load_prof.h>
#include <runtime/ee/ee_thread.h>
#include <runtime/ee/game_thread.h>
#include <runtime/kernel/ee_copy.h>
#include <runtime/ee/ee_timer.h>
#include <game/ovl/vsync_callback.h>
#include <game/ovl/vendor_frame.h>
#include <runtime/kernel/disc.h>
#include <runtime/kernel/ee_memory.h>
#include <runtime/level/level_loader.h>
#include "host_priority.h"
#include "host_input.h"
#if defined(__WIIU__)
rac1::host_input::HostState rac1_wiiu_pad_state();  // ports/wiiu/game/wiiu_input.cpp: the GamePad and the Pro / Classic controllers
bool rac1_wiiu_frame();                             // (ProcUI each host frame: false when the application closes)
bool rac1_wiiu_picture();                           // ports/wiiu/game/wiiu_gfx.cpp: the picture is up (RAC1_WIIU_GFX)
// (the frame's 2D for the picture: the replica's primitives, the game's frame size, its page, the page not drawn in a fade)
void rac1_wiiu_picture_frame(const rac1::gs::Gs& gs, float fb_w, float fb_h, int screen_fbp, int skip_fbp, uint64_t game_frame);
#endif
namespace rac1 {
extern bool g_world_pass_ranges_on;  // src/game/ovl/overlay_entry.cpp: the world passes' chain ranges noted
extern std::vector<std::pair<uint32_t, uint32_t>> g_world_pass_ranges;
}  // namespace rac1
#include "sample_prof.h"
#include "profiler/host_profiler.h"
#include "sim_autopilot.h"
#include "sim_script.h"
#include "update_check.h"
#include "progress.h"
#include <_devtools/item_give_quiet.h>  // (header only: also without the dev tools)
#ifdef RAC1_DEVTOOLS
#include <_devtools/dev_menu.h>
#include <_devtools/dev_unlocks.h>
#endif

using namespace rac1;

bool audio_out_start(int channels);  // audio_out.cpp
void audio_out_push(const int16_t* pcm, const int16_t* multi, int frames);
int audio_out_channels();
namespace rac1 {
int advanced_audio_channels();  // advanced.cpp: Options > Sound's 5.1 / 7.1 (the device's channels)
extern void (*g_advanced_audio_reopen)(int channels);
}
void audio_out_set_channels(int channels);
void audio_out_set_alone(bool ok);
uint64_t audio_out_alone_total();
void audio_out_step(bool in_step);
namespace rac1 {
void gs_texture_cache_clear();  // runtime/gs/gs_renderer.cpp (its header clashes with gs_emu.h)
}
void audio_out_stop();

namespace {

// The debug tools gate. The debug menu, moving Ratchet by hand (F2 / L3),
// the renderer choice and the layer toggles, and the testing switches (the RAC1_* environment variables) are there only
// with --debug-tools or RAC1_DEBUG_TOOLS=1, or in a headless run (the tests). In a window without them the game runs
// as shipped to the player: the MRP with all its optimisations, the pad and the player's settings; every RAC1_*
// variable but the player-facing ones below is removed from the environment before anything reads it (this runs before
// the C++ static initialisers, which read some of them).
bool g_debug_tools = false;   // the debug tools asked for
bool g_test_switches = false; // the RAC1_* testing variables and --level honoured (headless, or the debug tools)
constexpr const char* PLAYER_ENV[] = {
    "RAC1_DEBUG_TOOLS", "RAC1_HEADLESS", "RAC1_NO_GRAPHICS", "RAC1_ISO", "RAC1_MEMCARD_DIR", "RAC1_ADVANCED_FILE", "RAC1_GRAPHICS_FILE", "RAC1_CONTROLS_FILE",
    "RAC1_RENDERER", "RAC1_WINDOW", "RAC1_FULLSCREEN", "RAC1_VSYNC", "RAC1_FPS_CAP", "RAC1_NO_AUDIO", "RAC1_AUDIO_DEVICE",
    "RAC1_AUDIO_PERIOD_MS", "RAC1_AUDIO_PERIODS", "RAC1_MOVIE_VOLUME", "RAC1_PRIORITY", "RAC1_IOP_MIRROR",
    "RAC1_PROGRESS",  // (where progress.json goes for the launcher, or 0: docs/launcher.md)
    "RAC1_PYTHON",  // (set by tools/setup.py for the setup mode: the Python it runs the overlay matcher with)
};

// orig: (host) the gate, from the ELF's .init_array before the static initialisers (glibc passes argc / argv; on Windows
// the C runtime's __argc / __argv, already parsed when the constructors run)
void debug_tools_gate_args(int argc, char** argv) {
    const char* env = std::getenv("RAC1_DEBUG_TOOLS");
    g_debug_tools = env != nullptr && std::atoi(env) != 0;
    for (int i = 1; argv != nullptr && i < argc; ++i)
        if (argv[i] != nullptr && std::strcmp(argv[i], "--debug-tools") == 0) g_debug_tools = true;
    // (RAC1_NO_GRAPHICS is a headless run: kept whatever the gate decides, never a window instead)
    g_test_switches = g_debug_tools || std::getenv("RAC1_HEADLESS") != nullptr || std::getenv("RAC1_NO_GRAPHICS") != nullptr;
    if (g_test_switches) return;
    std::vector<std::string> drop;
    for (const std::string& name : rac1::platform::environment_names("RAC1_"))
        if (std::none_of(std::begin(PLAYER_ENV), std::end(PLAYER_ENV), [&name](const char* n) { return name == n; })) drop.push_back(name);
    for (const std::string& n : drop) {
        std::fprintf(stderr, "[orig] %s ignored: testing switches need --debug-tools (or RAC1_DEBUG_TOOLS=1)\n", n.c_str());
        rac1::platform::unset_environment(n.c_str());
    }
}
#if defined(_WIN32)
__attribute__((constructor(101))) void debug_tools_gate() { debug_tools_gate_args(__argc, __argv); }
#elif defined(__WIIU__)
// (the Wii U: no arguments reach the constructors, and the settings come from a file read in main: ports/wiiu/game
// runs the gate itself once it has set them)
}  // namespace
void rac1_wiiu_debug_tools_gate(int argc, char** argv) { debug_tools_gate_args(argc, argv); }
namespace {
#else
__attribute__((constructor(101))) void debug_tools_gate(int argc, char** argv, char** /*envp*/) { debug_tools_gate_args(argc, argv); }
#endif

// Testing (RAC1_FRAME_PROF=1): the frame's host stages timed (each closed by glFinish, so the GPU's
// work counts in its stage), averaged and printed every 120 frames.
// orig: (host) a profiler of the host's frame
double g_prof_vu_moby_ms = 0;  // the moby VU1 programs' own time inside the GS chain stage (RAC1_FRAME_PROF)
static int s_mrp_skip_mode = 1;  // orig: (host) RAC1_MRP_SKIP, see main()

// A vsync passed: the main loop's vsync at the end of each game frame, and each vsync wait of a blocking
// loop (the screen fade's, shown or not; the start's: ee_thread_set_vsync_hook). What runs on the PS2's vsyncs runs
// here, on the game's time, never the host's:
// - the EE timer T1 + 0x2580 (runtime/ee/ee_timer.h: the host's EE takes no time),
// - the game's vsync callback 0x12f1c8 (the vsync count 0x15ed48, the time 0x15ed50),
// - the IOP's sound advanced 1/60 s (800 sample frames at 48 kHz), queued for the audio device and RAC1_AUDIO_DUMP:
//   what the game reads back from it (a sound still playing, a stream's time left) depends on its vsyncs only.
//   With the audio device open the game's IOP's output is not played: the host's mirror of the IOP replays the same
// commands on the audio thread and is what is heard (ADR 0013, it goes on alone while this thread stalls);
//   this vsync is recorded for it, with the movie's samples of these 800 frames.
FILE* g_audio_dump = nullptr;
int g_audio_dump_ch = 2;  // (the dump's channels: the 5.1 / 7.1 choice at the start, Options > Sound or RAC1_AUDIO_CHANNELS)
// orig: (host) a vsync: the timer, the vsync callback, the IOP's time (the SPU2's output)
uint64_t g_vsyncs = 0;  // (testing: the vsyncs passed, RAC1_STALL_LOG)
// (testing) RAC1_TEST_ZERO_FB=<first>,<last>[,raw]: the host frames of the frames first..last (exclusive) see a framebuffer
// of 0 x 0, as a minimised window's (Windows: a fullscreen window alt-tabbed out of); `raw`: given to the renderers as it
// is, without the minimised window's skip (the behaviour before it)
uint64_t g_test_zero_fb_from = 0, g_test_zero_fb_to = 0;
bool g_test_zero_fb_raw = false;
// orig: (host) testing: RAC1_TEST_ZERO_FB's size at this frame
static bool test_zero_fb(uint64_t frame) { return g_test_zero_fb_to != 0 && frame >= g_test_zero_fb_from && frame < g_test_zero_fb_to; }
// the longest game step of the last host frame; a step longer than kStallMs is a stall (a load) the host does
// not catch up
double g_last_steps_ms = 0.0;
constexpr double kStallMs = 50.0;
// (testing) RAC1_IOP_MIRROR_TEST=<vsync>:<n>,... (or 1): a headless run drives the host's mirror of the IOP itself, on
// the game's thread, with no audio device: each vsync replayed on it (snd989_mirror_next) and, at the vsyncs listed, n frames
// rendered by the mirror alone first (a stall of the game's thread, as the audio thread plays through a load). Each change of
// the live streams (group:sector) of the game's IOP or of the mirror is printed ([mirror-test]); RAC1_MIRROR_DUMP=file: the
// mirror's output (48 kHz stereo s16, the alone frames included); the mirror's report at the end. (Both IOPs run on this one
// thread here and so share its snd::Rand: the SFX of random pitch / volume differ between them, RAC1_IOP_VERIFY counts those
// vsyncs as different; the streams, the music, do not.)
bool g_mirror_test = false;
FILE* g_mirror_dump = nullptr;
std::map<uint64_t, int> g_mirror_stalls;
void mirror_test_vsync(const int16_t* movie, int movie_frames, float gl, float gr, const int16_t* pcm) {
    int16_t out[800 * 2];
    if (const auto it = g_mirror_stalls.find(g_vsyncs); it != g_mirror_stalls.end())
        for (int i = 0; i < it->second; ++i) {
            snd989_mirror_alone(out);
            if (g_mirror_dump != nullptr) std::fwrite(out, sizeof(out), 1, g_mirror_dump);
        }
    snd989_mirror_vsync(movie, movie_frames, gl, gr, pcm);
    while (snd989_mirror_next(out))
        if (g_mirror_dump != nullptr) std::fwrite(out, sizeof(out), 1, g_mirror_dump);
    static std::string last_g, last_m;
    std::string g = snd989_streams_describe(false), m = snd989_streams_describe(true);
    if (g != last_g || m != last_m) {
        std::printf("[mirror-test] vsync %llu game {%s} mirror {%s}%s\n", static_cast<unsigned long long>(g_vsyncs), g.c_str(), m.c_str(),
                    g == m ? "" : " DIFFER");
        last_g = std::move(g);
        last_m = std::move(m);
    }
}

void vsync_passed() {
    ++g_vsyncs;
    ee_t1_vsync();
    vsync_callback();
    int16_t pcm[800 * 2];
    // (host) the 5.1 / 7.1 output of the game's IOP: for the dump, or for the device without the mirror
    int16_t multi[800 * 8];
    const int dev_ch = snd989_mirror_enabled() ? 2 : audio_out_channels();
    const int multi_ch = g_audio_dump != nullptr && g_audio_dump_ch > 2 ? g_audio_dump_ch : dev_ch;
    int16_t* const mp = multi_ch > 2 ? multi : nullptr;
    snd989_iop_render(pcm, 800, mp, multi_ch);
    float movie_gl = 1.0f, movie_gr = 1.0f;
    snd989_movie_gain(movie_gl, movie_gr);  // (the IRX's volumes of the movie's voices, not unity)
    int16_t movie[800 * 2];
    const int movie_frames = movie_hw_audio_take(movie, 800);
    if (mp != nullptr) snd989_movie_add_multi(mp, multi_ch, movie, movie_frames, movie_gl, movie_gr);
    movie_hw_audio_add(pcm, movie, movie_frames, movie_gl, movie_gr);
    if (g_audio_dump != nullptr) {
        if (g_audio_dump_ch > 2) std::fwrite(multi, static_cast<size_t>(800 * g_audio_dump_ch) * 2, 1, g_audio_dump);
        else std::fwrite(pcm, sizeof(pcm), 1, g_audio_dump);
    }
    if (g_mirror_test) {
        mirror_test_vsync(movie, movie_frames, movie_gl, movie_gr, pcm);
        return;
    }
    if (snd989_mirror_enabled()) snd989_mirror_vsync(movie, movie_frames, movie_gl, movie_gr, pcm);
    else audio_out_push(pcm, multi_ch == dev_ch ? mp : nullptr, 800);
}

// The chain a blocking loop sent at its last Kicked vsync (the screen fade's frames: the kick 0x2a08c8 at the wait,
// then the flip 0x2a0858 keeps its end at 0x1611c4), shown or not. The chain stage's fallback for a frame that built no chain of
// its own (the previous buffer's chain, kicked by a loop that flips after its kick, e.g. the exit's cards) must not send it again:
// the PS2 sent it once, at that vsync. After a fade the game may write over that buffer before the frame ends (Kerwan's death:
// the checkpoint's reload fills it with other data), and the GS replica then walked the overwritten bytes as a chain - 15 s of
// garbage VIF codes and VU1 programs in one host frame with fast fades (the app "not responding").
struct KickedChain {
    uint32_t start = 0, end = 0;
};
KickedChain g_loop_kicked;

// The moby skip ranges of the last game frame whose chain the MRP's skinning skip left without vertex data (the mobys' tags, the
// texture uploads 0x2555c0 built for the GS path), with the chain's bounds and the hash of its bytes. A frame that builds no chain
// of its own (the chain stage's fallback: the previous buffer's chain, at 0x1611c4) has that chain walked by the GS replica, and
// its mobys' NEXT parts hold the buffer's older bytes, which the VIF takes for codes (STMOD 1 with a stale ROW: the particles'
// UNPACK broken, an endless VU1 loop, task 0358): their tags are skipped again. The ranges are noted for every game frame,
// also those a host frame runs without drawing them (several game frames in one host frame when the host is slow): the previous
// buffer's chain is then a frame never walked, and with no ranges for it the replica ran its garbage - VU1 programs that do not
// exist ("no port for the microprogram"), the GS memory and the VU1 state written with garbage, or the game hung for good in the
// particle program's endless loop on the chain worker (Aridia's arrival, with two or three game frames in its last host frames: #104).
struct MobySkipsMemo {
    uint32_t start = 0, end = 0;
    uint64_t hash = 0;
    std::vector<rac1::gs::Gs::SkipRange> ranges;
};
MobySkipsMemo g_moby_skips_memo;
// orig: (host) the hash of a chain's bytes (a, e)
static uint64_t chain_bytes_hash(uint32_t a, uint32_t e) {
    uint64_t h = 1469598103934665603ull;
    const uint8_t* p = static_cast<const uint8_t*>(ee_ram_ptr_read(a));
    if (p == nullptr) return h;
    for (uint32_t i = 0; i + 8 <= e - a; i += 8) {
        uint64_t w;
        std::memcpy(&w, p + i, 8);
        h = (h ^ w) * 1099511628211ull;
    }
    return h;
}
// orig: (host) the moby ranges the replica skips in the chain (start, end) of the game frame just updated (its MRP scene),
// noted in g_moby_skips_memo; appended to `out` when given
static void moby_skips_note(uint32_t start, uint32_t end, std::vector<rac1::gs::Gs::SkipRange>* out) {
    const MrpFrame& sf = mrp_scene();
    std::vector<rac1::gs::Gs::SkipRange> ranges;
    for (const MrpMoby& m : sf.mobys)
        if (m.chain_end > m.chain_begin) {
            rac1::gs::Gs::SkipRange r;
            r.first = m.chain_begin;
            r.last = m.chain_end - 1;
            for (uint32_t k = 0; k < m.part_count; ++k) r.mscals.push_back(sf.parts[m.part_first + k].mscal);
            if (m.rt) r.host_3d = m.moby;  // a moby drawn into a render target: the MRP draws it there, in the GS's order
            r.moby = true;  // (Gs::moby_mark: where the MRP's mobys go in the primitives' order)
            ranges.push_back(std::move(r));
        }
    if (g_moby_upload_chain_first != 0) {
        rac1::gs::Gs::SkipRange r;
        r.first = g_moby_upload_chain_first;
        r.last = g_moby_upload_chain_last;
        ranges.push_back(std::move(r));
    }
    g_moby_skips_memo.start = start;
    g_moby_skips_memo.end = end;
    g_moby_skips_memo.hash = chain_bytes_hash(start, end);
    if (out != nullptr) out->insert(out->end(), ranges.begin(), ranges.end());
    g_moby_skips_memo.ranges = std::move(ranges);
}
// orig: (host) testing (RAC1_GS_PRIMDUMP=<game frame>): the GS replica's primitives of that frame but the projected 3D, one line
// each (the type, the registers, the vertices, a hash of the texture's pixels), to compare a host's 2D with another's (the
// Wii U's with the PC's)
static void gs_prims_dump(const rac1::gs::Gs& gs, uint64_t game_frame) {
    static const char* e = std::getenv("RAC1_GS_PRIMDUMP");
    static bool done = false;  // (once, at the first frame drawn at or past it: a slow host runs several game frames in one)
    if (e == nullptr || done || game_frame < std::strtoull(e, nullptr, 10)) return;
    done = true;
    std::printf("[prim] game frame %llu: %zu primitives\n", static_cast<unsigned long long>(game_frame), gs.primitives().size());
    size_t i = 0;
    for (const rac1::gs::Primitive& p : gs.primitives()) {
        ++i;
        if (p.vu || p.fx != 0) continue;
        uint64_t th = 1469598103934665603ull;
        if (p.tex)
            for (uint32_t px : p.tex->px) th = (th ^ px) * 1099511628211ull;
        std::printf("[prim] %zu t%d prim %llx tex0 %llx alpha %llx test %llx frame %llx tex %dx%d %016llx", i, p.type, static_cast<unsigned long long>(p.st.prim),
                    static_cast<unsigned long long>(p.st.tex0), static_cast<unsigned long long>(p.st.alpha), static_cast<unsigned long long>(p.st.test),
                    static_cast<unsigned long long>(p.st.frame), p.tex ? p.tex->w : 0, p.tex ? p.tex->h : 0, static_cast<unsigned long long>(p.tex ? th : 0));
        for (const rac1::gs::Vertex& v : p.v) std::printf(" (%.1f %.1f rgba %02x%02x%02x%02x uv %.1f %.1f)", v.x, v.y, v.r, v.g, v.b, v.a, v.u, v.v);
        std::printf("\n");
    }
}
// orig: (host) a game frame a host frame ran without drawing it: its moby skip ranges noted (the next frame may walk its chain)
static void moby_skips_note_undrawn() {
    if (!g_moby_mrp_skip || (g_moby_mrp_skip_parts & 8) == 0 || g_moby_mrp_skip_verify) return;  // (the skinning not skipped)
    const uint32_t idx = lwu(OV(0x001611d0));
    const uint32_t start = lwu(OV(0x001611b8) + (idx & 1) * 4), end = lwu(OV(0x001611c0));
    if (start != 0 && end > start) moby_skips_note(start, end, nullptr);
}
// orig: (host) a vsync wait inside a blocking loop (ee_thread_vsync): the chain a Kicked wait sent, then the vsync's work
void blocking_loop_vsync(rac1::EeVsyncFrame kind) {
    if (kind == rac1::EeVsyncFrame::Kicked) {
        const uint32_t idx = lwu(OV(0x001611d0));
        g_loop_kicked = KickedChain{lwu(OV(0x001611b8) + (idx & 1) * 4), lwu(OV(0x001611c0))};
    }
    vsync_passed();
}

// orig: (host) testing: the page hashes of the game's RAM and scratchpad at the end of a frame's update
static void ram_hash_frame(FILE* f, uint64_t frame) {
    static std::vector<uint64_t> h;
    h.assign(8192 + 4, 0);
    auto hash_page = [](const uint8_t* p) {
        uint64_t a = 0xcbf29ce484222325ull;
        for (int i = 0; i < 4096; i += 8) {
            uint64_t w;
            std::memcpy(&w, p + i, 8);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
            w = w << 32 | w >> 32;  // (EE memory as host words: the doubleword's low word first, as the PS2 reads it)
#endif
            a = (a ^ w) * 0x100000001b3ull;
            a ^= a >> 29;
        }
        return a;
    };
    const uint8_t* ram = static_cast<const uint8_t*>(ee_ram_ptr(0x100000u)) - 0x100000;
    for (uint32_t pg = 0; pg < 8192; ++pg) h[pg] = hash_page(ram + static_cast<size_t>(pg) * 4096);
    const uint8_t* spr = static_cast<const uint8_t*>(ee_ram_ptr(0x70000000u));
    for (uint32_t pg = 0; pg < 4; ++pg) h[8192 + pg] = hash_page(spr + pg * 4096);
#if defined(__BYTE_ORDER__) && __BYTE_ORDER__ == __ORDER_BIG_ENDIAN__
    for (uint64_t& v : h) v = __builtin_bswap64(v);  // (the file little-endian, as the PC writes it: the runs compare)
#endif
    std::fwrite(h.data(), 8, h.size(), f);
    static const char* dump = std::getenv("RAC1_RAMDUMP");
    if (dump != nullptr) {
        unsigned long long want = 0;
        char path[512] = {};
        if (std::sscanf(dump, "%llu,%511s", &want, path) == 2 && want == frame) {
            std::printf("[ramdump] frame %llu ring0 %08x ring1 %08x work %08x records %08x chain_end %08x\n", static_cast<unsigned long long>(frame), lwu(OV(0x001611b8)), lwu(OV(0x001611bc)), lwu(OV(0x0015f5fc)),
                        lwu(OV(0x0015f5f8)), lwu(OV(0x001611c0)));
            if (FILE* d = std::fopen(path, "wb")) {
                std::fwrite(ram, 1, 32u << 20, d);
                std::fwrite(spr, 1, 0x4000, d);
                std::fclose(d);
            }
        }
    }
}

// The host frame's present on a precise timer. Before, the frame was presented as soon as its work ended and the
// cap's sleep came after the swap: the picture reached the screen at the end of a variable amount of work (an update, the
// GS chain, the MRP scene: 8..20 ms under load), so its interval jittered by that much and, on a 240 Hz display, flipped
// between 3 and 5 refreshes - a visible stutter even at a steady 60 fps. Now the work runs first, then the thread waits until
// the frame's slot (a sleep to 0.5 ms before it, the rest yielded on the CPU: a sleep's wake-up under load can be late),
// then presents: the swaps come 1/60 s apart to within ~0.1 ms whatever the work took, as long as it fit.
// - The slot: 1 / cap s; with the game running at speed 1 the host frames are paced at the game's 60 a second whatever the
//   cap above it (a host frame with no game frame shows the same game state again: it only cost CPU and broke the steps'
//   cadence: 1 0 1 1 0 ...); the cap 0 (Unlimited) too. The debug menu's pause / an open menu with the game held: the cap as
//   set, 0 unlimited.
// - A frame late by more than half a slot is presented at once and the slots restart from it (no burst of short frames
//   catching up); late by less: presented at once, the slots kept.
// - RAC1_PACING=0: the old way (the swap, then the cap's sleep), for comparison; RAC1_PACE_GAME=0: the cap as set.
// The swap interval for the VSync setting. On Wayland the compositor always presents whole frames (no tearing with
// an interval of 0), and an interval of 1 makes eglSwapBuffers wait for the compositor's frame callback, which it does not send
// to a window it does not show (another workspace, minimised, covered): the game's thread then stops in the swap - the game and
// its sound with it - until the window is seen again. So on Wayland the swap never waits and the frames are paced by
// HostPacer (the game running: its 60 a second; held by the debug menu with VSync on and no cap: the display's refresh);
// elsewhere VSync is the swap interval 1.
// orig: (host)
static int host_swap_interval(bool vsync) { return vsync && glfwGetPlatform() != GLFW_PLATFORM_WAYLAND ? 1 : 0; }
// orig: (host) the VSync setting applied: the swap interval set, and in the log what it became - the platform, the
// driver's swap control (an interval of 0 needs GLX_EXT / GLX_MESA_swap_control or WGL_EXT_swap_control; EGL's
// eglSwapInterval always takes it) and Mesa's vblank_mode when set (0 forces the interval 0, 3 forces 1). What
// the swaps then do is measured a few seconds later: the "[display] presents" line (HostPresents).
static void host_apply_swap_interval(bool vsync, const char* why) {
    const int interval = host_swap_interval(vsync);
    glfwSwapInterval(interval);
    const int plat = glfwGetPlatform();
    const char* name = "other";
    const char* control = "?";
    if (plat == GLFW_PLATFORM_WAYLAND) {
        name = "Wayland (EGL)";
        control = "eglSwapInterval";
    } else if (plat == GLFW_PLATFORM_X11) {
        name = "X11 (GLX)";
        control = glfwExtensionSupported("GLX_EXT_swap_control")    ? "GLX_EXT_swap_control"
                  : glfwExtensionSupported("GLX_MESA_swap_control") ? "GLX_MESA_swap_control"
                  : glfwExtensionSupported("GLX_SGI_swap_control")  ? "GLX_SGI_swap_control only (no interval 0)"
                                                                     : "none";
    } else if (plat == GLFW_PLATFORM_WIN32) {
        name = "Windows (WGL)";
        control = glfwExtensionSupported("WGL_EXT_swap_control") ? "WGL_EXT_swap_control" : "none";
    }
    const char* vblank = std::getenv("vblank_mode");
    std::printf("[display] %s: V-Sync %s, swap interval %d on %s (swap control: %s)%s%s%s\n", why, vsync ? "on" : "off", interval, name, control,
                vblank != nullptr ? ", vblank_mode=" : "", vblank != nullptr ? vblank : "",
                plat == GLFW_PLATFORM_WAYLAND ? "; on Wayland the compositor shows whole frames: no tearing either way" : "");
}
// orig: (host) the monitor the Graphics page's Monitor row chose (an index in GLFW's list), else the primary
// monitor, which on Wayland is only the first output the compositor advertises
static GLFWmonitor* host_fullscreen_monitor() {
    GLFWmonitor* primary = glfwGetPrimaryMonitor();
    int n = 0;
    GLFWmonitor** all = glfwGetMonitors(&n);
    const int row = rac1::graphics_display().monitor;
    if (all == nullptr || row < 0 || row >= n) return primary;
    const GLFWvidmode* mode = glfwGetVideoMode(all[row]);
    std::printf("[orig] the monitor: %s (%d), %s x %s at %d Hz\n", glfwGetMonitorName(all[row]), row, mode != nullptr ? std::to_string(mode->width).c_str() : "?", mode != nullptr ? std::to_string(mode->height).c_str() : "?",
                mode != nullptr ? mode->refreshRate : 0);
    return all[row];
}
// orig: (host) a windowed window centred on the monitor the Graphics page's Monitor row chose. GLFW cannot place a
// window on Wayland (the compositor does, and ignores the request), so this does nothing there; it is for X11.
static void host_window_on_monitor(GLFWwindow* w, int row) {
    if (glfwGetPlatform() == GLFW_PLATFORM_WAYLAND || row < 0) return;
    int n = 0;
    GLFWmonitor** all = glfwGetMonitors(&n);
    if (all == nullptr || row >= n) return;
    const GLFWvidmode* mode = glfwGetVideoMode(all[row]);
    if (mode == nullptr) return;
    int mx = 0, my = 0, ww = 0, wh = 0;
    glfwGetMonitorPos(all[row], &mx, &my);
    glfwGetWindowSize(w, &ww, &wh);
    glfwSetWindowPos(w, mx + std::max(0, (mode->width - ww) / 2), my + std::max(0, (mode->height - wh) / 2));
}
// orig: (host) the display's refresh rate (the window's monitor, else the first; 60 when unknown)
static double host_refresh_hz(GLFWwindow* w) {
    GLFWmonitor* m = glfwGetWindowMonitor(w);
    int n = 0;
    GLFWmonitor** all = m == nullptr ? glfwGetMonitors(&n) : nullptr;
    if (m == nullptr && all != nullptr && n > 0) m = all[0];
    const GLFWvidmode* v = m != nullptr ? glfwGetVideoMode(m) : nullptr;
    return v != nullptr && v->refreshRate > 0 ? static_cast<double>(v->refreshRate) : 60.0;
}

struct HostPacer {
    using clk = std::chrono::steady_clock;
    bool legacy = std::getenv("RAC1_PACING") != nullptr && std::atoi(std::getenv("RAC1_PACING")) == 0;
    clk::time_point next{};
    bool have = false;
    uint64_t late = 0, resync = 0;
    // orig: (host) wait for the frame's slot (before its present)
    void wait(double hz) {
        if (legacy) return;
        if (hz <= 0.0) {
            have = false;
            return;
        }
        const auto period = std::chrono::nanoseconds(static_cast<int64_t>(1e9 / hz));
        const auto now = clk::now();
        if (!have) {
            next = now;
            have = true;
            return;
        }
        next += period;
        if (now > next) {
            ++late;
            if (now > next + period / 2) {
                next = now;
                ++resync;
            }
            return;
        }
        if (next - now > std::chrono::microseconds(1000)) std::this_thread::sleep_until(next - std::chrono::microseconds(500));
        while (clk::now() < next) std::this_thread::yield();
    }
};

// The window's presents measured: the "[display] presents" line, from 1 s to 5 s after the start and after each change of
// the display settings - the pictures shown a second, the time between them (median, 5th and 95th percentile) and how long
// the swap waited (a swap that waits about a refresh each time: V-Sync in effect, the driver's or the compositor's, whatever
// the swap interval asked). Also the stats overlay's rate: present to present, not the host loop's turns (a turn that runs
// no new game frame presents nothing, and counted as a frame it doubled the rate shown: 120 for 60 pictures a second).
// orig: (host)
struct HostPresents {
    using clk = std::chrono::steady_clock;
    bool on = true;  // (a window's run, or a headless RAC1_REALTIME one)
    clk::time_point t0 = clk::now(), last{};
    bool have_last = false, reported = false;
    std::vector<double> gaps, waits;
    double ema_ms = 1000.0 / 60.0;
    void restart() {
        t0 = clk::now();
        gaps.clear();
        waits.clear();
        reported = false;
    }
    void swap(GLFWwindow* w) {
        const auto a = clk::now();
        if (rac1::vk::is_active()) rac1::vk::end_frame();  // (the Vulkan renderer: its present)
        else glfwSwapBuffers(w);
        const auto b = clk::now();
        if (have_last) ema_ms = ema_ms * 0.9 + std::chrono::duration<double, std::milli>(b - last).count() * 0.1;
        const double since = std::chrono::duration<double>(b - t0).count();
        if (on && !reported && since >= 1.0) {
            if (have_last && last >= t0 + std::chrono::seconds(1)) gaps.push_back(std::chrono::duration<double, std::milli>(b - last).count());
            waits.push_back(std::chrono::duration<double, std::milli>(b - a).count());
            if (since >= 5.0 && !gaps.empty()) {
                reported = true;
                std::vector<double> g = gaps;
                std::sort(g.begin(), g.end());
                double wsum = 0, wmax = 0;
                for (double x : waits) wsum += x, wmax = std::max(wmax, x);
                std::printf("[display] presents: %.1f a second; between them median %.2f ms, p5 %.2f, p95 %.2f; the swap waited %.2f ms mean, %.2f max\n",
                            static_cast<double>(waits.size()) / (since - 1.0), g[g.size() / 2], g[g.size() * 5 / 100], g[g.size() * 95 / 100], wsum / static_cast<double>(waits.size()), wmax);
            }
        }
        last = b;
        have_last = true;
    }
};

// RAC1_FRAME_PROF=2: no glFinish (the stages' CPU time as they run), each stage's GPU time from a time-elapsed
// query around it (its commands flushed at its end; read back R - 1 frames later), the host frame's period (mean, median, p95, the frames over 17.5 / 34 ms) and the
// game frames run per host frame.
struct FrameProf {
    static constexpr int N = 9;
    static constexpr int R = 4;  // the GPU queries' ring (frames in flight)
    const char* names[N] = {"update", "world", "glb_mobys", "capture", "gs_chain", "gpu_mobys", "gs_draw", "menu_shots", "swap"};
    int mode = std::getenv("RAC1_FRAME_PROF") != nullptr ? std::max(1, std::atoi(std::getenv("RAC1_FRAME_PROF"))) : 0;
    bool on = mode != 0;
    double acc[N] = {}, gpu[N] = {}, tcpu[N] = {};
    double tc = 0;  // the main thread's CPU time (not the wall's: the machine's other load left out)
    static double thread_ms() {
        return rac1::platform::thread_cpu_ms();
    }
    int frames = 0, gpu_frames = 0;
    // the whole run's host frames: their periods (present to present), the work before the present (the poll to
    // the swap, the pacing's wait left out) and the game frames each ran; printed at exit (report())
    std::vector<double> run_periods, run_work;
    std::chrono::steady_clock::time_point t_begin;
    int run_steps[5] = {};
    double cur[N] = {};  // (this host frame's stages: printed when the frame is long)
    int cur_steps = 0;
    uint64_t host_frames = 0;
    std::chrono::steady_clock::time_point t, frame_t0;
    bool have_frame_t0 = false;
    std::vector<double> periods;
    int steps_acc = 0;
    GLuint q[R][N + 1] = {};
    bool q_used[R] = {};
    int ring = 0;
    bool gpu_begun = false;
    void begin() {
        if (!on) return;
        t = std::chrono::steady_clock::now();
        t_begin = t;
        for (double& c : cur) c = 0;
        tc = thread_ms();
        if (mode == 2 && !gpu_begun && !rac1::vk::is_active()) {
            if (q[0][0] == 0) glGenQueries(R * (N + 1), &q[0][0]);
            glBeginQuery(GL_TIME_ELAPSED, q[ring][0]);
            gpu_begun = true;
        }
    }
    void steps(int n) {
        steps_acc += n;
        if (on) ++run_steps[std::clamp(n, 0, 4)];
        cur_steps = n;
    }
    void work_done() {  // (the host frame's work ended, its present next)
        if (on) run_work.push_back(std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t_begin).count());
    }
    // the whole run's distributions: the host frames' periods and work, the game frames per host frame
    void report() const {
        if (!on || run_periods.size() < 2) return;
        auto line = [](const char* what, std::vector<double> v) {
            std::sort(v.begin(), v.end());
            static const double edges[] = {8, 12, 15, 16.0, 17.4, 20, 25, 34, 50};
            int bins[10] = {};
            double sum = 0;
            for (double d : v) {
                int b = 0;
                while (b < 9 && d >= edges[b]) ++b;
                ++bins[b];
                sum += d;
            }
            const size_t n = v.size();
            std::printf("[prof] run %s: %zu frames, mean %.2f ms, median %.2f, p95 %.2f, p99 %.2f, max %.2f; ms <8 %d | 8-12 %d | 12-15 %d | 15-16 %d | 16-17.4 %d | 17.4-20 %d | 20-25 %d | "
                        "25-34 %d | 34-50 %d | >=50 %d\n",
                        what, n, sum / static_cast<double>(n), v[n / 2], v[n * 95 / 100], v[n * 99 / 100], v.back(), bins[0], bins[1], bins[2], bins[3], bins[4], bins[5], bins[6], bins[7],
                        bins[8], bins[9]);
        };
        line("host frame period", run_periods);
        if (!run_work.empty()) line("host frame work", run_work);
        std::printf("[prof] run game frames per host frame: 0: %d, 1: %d, 2: %d, 3: %d, 4+: %d\n", run_steps[0], run_steps[1], run_steps[2], run_steps[3], run_steps[4]);
    }
    void mark(int i) {
        if (!on) return;
        if (rac1::vk::is_active()) {
            if (mode == 1) rac1::vk::flush_cmd();
        } else if (mode == 1) {
            glFinish();
        } else if (gpu_begun) {  // (each stage's commands flushed with its query: the GPU's time on them, few idle gaps)
            glEndQuery(GL_TIME_ELAPSED);
            glFlush();
            if (i + 1 < N) glBeginQuery(GL_TIME_ELAPSED, q[ring][i + 1]);
        }
        const auto n = std::chrono::steady_clock::now();
        acc[i] += std::chrono::duration<double, std::milli>(n - t).count();
        cur[i] += std::chrono::duration<double, std::milli>(n - t).count();
        t = n;
        const double c = thread_ms();
        tcpu[i] += c - tc;
        tc = c;
    }
    void end_frame() {
        if (!on) return;
        const auto now = std::chrono::steady_clock::now();
        if (have_frame_t0) {
            periods.push_back(std::chrono::duration<double, std::milli>(now - frame_t0).count());
            run_periods.push_back(periods.back());
            // a long host frame: its stages, the time before its begin (the events' poll, the debug menu's) and the
            // game frames it ran
            static const double long_ms = std::getenv("RAC1_FRAME_PROF_LONG") != nullptr ? std::atof(std::getenv("RAC1_FRAME_PROF_LONG")) : 30.0;
            if (periods.back() > long_ms) {
                std::printf("[prof] long host frame %llu: %.1f ms, %d game frames; before begin %.1f;", static_cast<unsigned long long>(host_frames), periods.back(), cur_steps,
                            std::chrono::duration<double, std::milli>(t_begin - frame_t0).count());
                for (int i = 0; i < N; ++i)
                    if (cur[i] >= 0.5) std::printf(" %s %.1f", names[i], cur[i]);
                std::printf("\n");
            }
        }
        ++host_frames;
        frame_t0 = now;
        have_frame_t0 = true;
        if (mode == 2 && gpu_begun && !rac1::vk::is_active()) {
            gpu_begun = false;
            q_used[ring] = true;
            ring = (ring + 1) % R;
            if (q_used[ring]) {  // (the oldest frame's queries)
                for (int i = 0; i < N; ++i) {
                    GLuint64 ns = 0;
                    glGetQueryObjectui64v(q[ring][i], GL_QUERY_RESULT, &ns);
                    gpu[i] += static_cast<double>(ns) * 1e-6;
                }
                ++gpu_frames;
                q_used[ring] = false;
            }
        }
        if (++frames < 120) return;
        double total = 0;
        std::printf("[prof] ms/frame:");
        for (int i = 0; i < N; ++i) {
            std::printf(" %s %.2f", names[i], acc[i] / frames);
            total += acc[i];
            acc[i] = 0;
        }
        std::printf(" | total %.2f (moby VU1 programs, inside gs_chain: %.2f; inside update: moby skinning+poses %.2f, sub-mesh fill %.2f, texture groups %.2f, texture slots %.2f)\n", total / frames,
                    g_prof_vu_moby_ms / frames, g_prof_moby_skin_ms / frames, g_prof_moby_fill_ms / frames, g_prof_moby_tex_ms / frames, g_prof_moby_slots_ms / frames);
        if (mode == 2) {
            double gt = 0;
            std::printf("[prof] gpu ms/frame:");
            for (int i = 0; i < N; ++i) {
                std::printf(" %s %.2f", names[i], gpu_frames != 0 ? gpu[i] / gpu_frames : 0.0);
                gt += gpu[i];
                gpu[i] = 0;
            }
            std::printf(" | total %.2f\n", gpu_frames != 0 ? gt / gpu_frames : 0.0);
            gpu_frames = 0;
            double ct = 0;
            std::printf("[prof] thread cpu ms/frame:");
            for (int i = 0; i < N; ++i) {
                std::printf(" %s %.2f", names[i], tcpu[i] / frames);
                ct += tcpu[i];
                tcpu[i] = 0;
            }
            std::printf(" | total %.2f\n", ct / frames);
            if (!periods.empty()) {
                std::vector<double> p = periods;
                std::sort(p.begin(), p.end());
                double sum = 0;
                int over16 = 0, over33 = 0;
                for (double d : p) {
                    sum += d;
                    over16 += d > 17.5 ? 1 : 0;
                    over33 += d > 34.0 ? 1 : 0;
                }
                std::printf("[prof] host frame: mean %.2f ms (%.1f fps), median %.2f, p95 %.2f, max %.2f; over 17.5 ms %d / %zu, over 34 ms %d; game frames per host frame %.2f\n",
                            sum / static_cast<double>(p.size()), 1000.0 * static_cast<double>(p.size()) / sum, p[p.size() / 2], p[p.size() * 95 / 100], p.back(), over16, p.size(), over33,
                            static_cast<double>(steps_acc) / frames);
            }
        }
        periods.clear();
        steps_acc = 0;
        g_prof_vu_moby_ms = 0;
        g_prof_moby_skin_ms = g_prof_moby_fill_ms = g_prof_moby_tex_ms = g_prof_moby_slots_ms = 0;
        frames = 0;
    }
};

// The host's last world frame (the 3D, before the game's 2D), kept as the background of the
// full-screen menu (the PS2 captures its frame buffer when the menu opens, 0x2a0f18).
// orig: (host) the menu's captured frame
struct FrozenFrame {
    GLuint fbo = 0, tex = 0;
    rac1::vk::GpuImage vk_tex{};
    int w = 0, h = 0;
    bool valid() const { return rac1::vk::is_active() ? (vk_tex.image != VK_NULL_HANDLE) : (fbo != 0); }
    void vk_reset() {
        if (vk_tex.image != VK_NULL_HANDLE) rac1::vk::destroy_image_now(vk_tex);
        if (rac1::vk::is_active()) w = h = 0;
    }
    void capture(int cw, int ch) {
        if (rac1::vk::is_active()) {
            if (vk_tex.image == VK_NULL_HANDLE || cw != w || ch != h) {
                w = cw;
                h = ch;
                rac1::vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT, vk_tex);
            }
            rac1::vk::blit_image(rac1::vk::main_color_image(), 0, 0, w, h, vk_tex, 0, 0, w, h, VK_FILTER_NEAREST);
            return;
        }
        if (fbo == 0) {
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex, "Frame captures (menu, fades)");
        }
        if (cw != w || ch != h) {
            w = cw;
            h = ch;
            glBindTexture(GL_TEXTURE_2D, tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, 0);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, fbo);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
    void blit_to_screen() const {
        if (rac1::vk::is_active()) {
            if (vk_tex.image != VK_NULL_HANDLE) {
                auto& src = const_cast<rac1::vk::GpuImage&>(vk_tex);
                rac1::vk::blit_image(src, 0, 0, w, h, rac1::vk::main_color_image(), 0, 0, w, h, VK_FILTER_NEAREST,
                                     VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
            return;
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, w, h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// orig: (host) Vulkan: the last frame of the game shown kept where it was drawn (the 3D's colour image, untouched until the next
// frame's start clears it) instead of copied into `last_shown` every frame (a full-window copy: 0.6 ms of GPU at 5120 x 1440); the
// copy made only when something reads it - a frame that draws over it (a fade, the pause menu, a blocking loop), the pause menu's
// store - before that clear. Not with the debug menu or its overlays drawn over the frame after the capture (they would be in it).
// RAC1_LAZY_CAPTURE=0: copied every frame, as before
struct LazyShown {
    bool pending = false;  // `dst`'s picture is still the main colour image's (w x h)
    int w = 0, h = 0;
    FrozenFrame* dst = nullptr;
    void resolve() {
        if (!pending) return;
        pending = false;
        if (dst != nullptr && rac1::vk::is_active() && rac1::vk::main_color_image().width == static_cast<uint32_t>(w) &&
            rac1::vk::main_color_image().height == static_cast<uint32_t>(h))
            dst->capture(w, h);
    }
};
LazyShown g_lazy_shown;

// Options > Graphics > Render Resolution: the MRP's 3D (world, sky, mobys) drawn into a buffer of its own at a
// fraction of the window's resolution (0.5 .. 2), then put on the window - its colour scaled (linear), its depth (nearest: the
// GS replica's 3D effects are tested against it) - before the effects and the 2D (HUD, text, menus) are drawn at the window's
// resolution. Host code: the game's state is not touched.
// orig: (host) the render scale's buffer
struct ScaledScene {
    GLuint fbo = 0, col = 0, dep = 0;
    int w = 0, h = 0;      // the buffer (the window times the scale)
    float s = 1.0f;
    bool on = false;       // this frame's 3D is drawn into it
    bool warned = false;
    // the frame's 3D starts: the buffer bound and cleared
    void begin(int win_w, int win_h, float scale) {
        s = scale;
        const int bw = std::max(1, static_cast<int>(std::lround(win_w * scale))), bh = std::max(1, static_cast<int>(std::lround(win_h * scale)));
        if (rac1::vk::is_active()) {
            w = bw;
            h = bh;
            rac1::vk::scaled_scene_begin(win_w, win_h, bw, bh);
            on = true;
            return;
        }
        if (fbo == 0) {
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &col);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &col, "Render scale buffer");
            glGenRenderbuffers(1, &dep);
            rac1::prof::gl_track_n(rac1::prof::kGlRenderbuffer, 1, &dep, "Render scale buffer");
        }
        if (bw != w || bh != h) {
            w = bw;
            h = bh;
            glBindTexture(GL_TEXTURE_2D, col);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindRenderbuffer(GL_RENDERBUFFER, dep);
            glRenderbufferStorage(GL_RENDERBUFFER, GL_DEPTH24_STENCIL8, w, h);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, col, 0);
            glFramebufferRenderbuffer(GL_FRAMEBUFFER, GL_DEPTH_STENCIL_ATTACHMENT, GL_RENDERBUFFER, dep);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, fbo);
        glViewport(0, 0, w, h);
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);  // (the stencil: the ambient occlusion's moby mark starts at 0)
        on = true;
    }
    // a window viewport in the buffer
    rac1::gs::Viewport scaled(const rac1::gs::Viewport& v) const {
        rac1::gs::Viewport r = v;
        r.x = static_cast<int>(std::lround(v.x * s));
        r.y = static_cast<int>(std::lround(v.y * s));
        r.w = static_cast<int>(std::lround(v.w * s));
        r.h = static_cast<int>(std::lround(v.h * s));
        return r;
    }
    // the 3D put on the window (colour and depth), the window's framebuffer bound again
    void end(int win_w, int win_h) {
        if (!on) return;
        on = false;
        if (rac1::vk::is_active()) {
            rac1::vk::scaled_scene_end(win_w, win_h);
            return;
        }
        if (!warned)
            while (glGetError() != GL_NO_ERROR) {
            }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, 0, 0, win_w, win_h, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBlitFramebuffer(0, 0, w, h, 0, 0, win_w, win_h, GL_DEPTH_BUFFER_BIT, GL_NEAREST);
        if (!warned) {
            if (const GLenum e = glGetError(); e != GL_NO_ERROR) std::fprintf(stderr, "[graphics] render scale: the depth blit failed (GL error 0x%x)\n", e);
            warned = true;
        }
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        glViewport(0, 0, win_w, win_h);
    }
};
ScaledScene g_scaled_scene;

// The picture of a frame the game draws in order to store it. The Vendor's screens' draw (0x2a02b0) renders a
// frame of the game, shows it (a vsync wait the host makes a frame of its own: EeVsyncFrame::Scene) and stores the GS frame
// buffer's picture in EE memory (0x1f99c0 / 0x2a0f18); every later frame of the screens puts that picture back into the frame
// buffer first (0x2a0fc8) and draws over it. The GS replica draws no pixels into its memory, so the host keeps that frame's
// picture (the window's) and, when the game stores the frame buffer, writes it into the replica's memory at the frame, at the
// frame's resolution (store()); the game's transfer back into the frame is a primitive of type 7 (Gs::set_screen_area), drawn
// here: the kept picture at the window's resolution when the replica's frame holds what store() put there, else the replica's
// frame over the frame's 4:3 area.
// orig: (host) the frame buffer's picture a stored frame leaves
struct ScenePicture {
    FrozenFrame pic;                     // the window's picture of the frame
    int x0 = 0, y0 = 0, cw = 0, ch = 0;  // its 4:3 area (window pixels from the bottom left)
    int view[4] = {0, 0, 0, 0};          // the viewport it was drawn in (a window resized since: the frame's picture drawn instead)
    bool pending = false;                // kept, not yet written into the replica's memory
    // the last picture shown (rac1-native's `displayed`) and its viewport: what the frame buffer holds when a store
    // comes with no Scene frame kept (the pause menu's start 0x276368 stores the frame of the gameplay frame before it)
    const FrozenFrame* shown = nullptr;
    bool menu_pic = false;  // `pic` holds the frame the pause menu's start stored (its background, at the window's resolution)
    int shown_view[4] = {0, 0, 0, 0};
    uint64_t hash = 0;                   // the frame's pixels as store() wrote them (0: none)
    uint64_t match_id = 0;               // last TexImage::id confirmed to match `hash`
    GLuint rb_fbo = 0, rb_tex = 0;       // the frame's resolution: the picture read back / the replica's frame drawn
    rac1::vk::GpuImage vk_rb_tex{};
    int rb_w = 0, rb_h = 0;
    static uint64_t hash_of(const uint32_t* px, size_t n) {
        uint64_t h = 1469598103934665603ull;
        for (size_t i = 0; i < n; ++i) h = (h ^ px[i]) * 1099511628211ull;
        return h == 0 ? 1 : h;
    }
    void target(int w, int h) {  // the frame-sized texture and its framebuffer
        if (rac1::vk::is_active()) {
            if (vk_rb_tex.image == VK_NULL_HANDLE || rb_w != w || rb_h != h) {
                rb_w = w;
                rb_h = h;
                rac1::vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT, vk_rb_tex);
            }
            return;
        }
        if (rb_fbo == 0) {
            glGenFramebuffers(1, &rb_fbo);
            glGenTextures(1, &rb_tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &rb_tex, "Frame captures (menu, fades)");
        }
        if (rb_w != w || rb_h != h) {
            rb_w = w;
            rb_h = h;
            glBindTexture(GL_TEXTURE_2D, rb_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, rb_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, rb_tex, 0);
            glBindFramebuffer(GL_FRAMEBUFFER, 0);
        }
    }
    // after a Scene frame is drawn: the window's picture kept (the frame's 4:3 area: the viewport narrowed by the widescreen view)
    void keep(int w, int h, int vx, int vy, int vw, int vh) {
        pic.capture(w, h);
        cw = static_cast<int>(static_cast<float>(vw) / g_view_widen);
        ch = vh;
        x0 = vx + (vw - cw) / 2;
        y0 = vy;
        view[0] = vx;
        view[1] = vy;
        view[2] = vw;
        view[3] = vh;
        pending = true;
    }
    // the game reads the GS memory at block bp (libgraph's store): at the frame, the kept picture written there first, once
    // the Scene frame (or on-demand when the pause menu stores the last frame shown), read back at the frame's resolution
    GLuint pf_fbo = 0, pf_tex = 0, pf_pbo = 0;
    GLsync pf_fence = nullptr;
    rac1::vk::GpuImage vk_pf_tex{};
    rac1::vk::GpuBuffer vk_pf_buf{};
    int pf_w = 0, pf_h = 0;
    bool pf_valid = false;
    void vk_reset() {
        pic.vk_reset();
        if (vk_rb_tex.image != VK_NULL_HANDLE) rac1::vk::destroy_image_now(vk_rb_tex);
        if (vk_pf_tex.image != VK_NULL_HANDLE) rac1::vk::destroy_image_now(vk_pf_tex);
        rac1::vk::destroy_buffer_now(vk_pf_buf);
        pf_valid = false;
    }
    void prefetch(const FrozenFrame& src, int vx, int vy, int vw, int vh) {
        const uint32_t frame = lwu(0x0013cf20);
        const int w = static_cast<int>(((frame >> 16) & 0x3f) * 64), h = static_cast<int>(lhu(0x001518d2));
        if (w == 0 || h == 0 || !src.valid()) return;
        const int scw = static_cast<int>(static_cast<float>(vw) / g_view_widen), sx0 = vx + (vw - scw) / 2;
        if (rac1::vk::is_active()) {
            if (vk_pf_tex.image == VK_NULL_HANDLE || pf_w != w || pf_h != h) {
                pf_w = w;
                pf_h = h;
                rac1::vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT, vk_pf_tex);
                rac1::vk::create_buffer(static_cast<VkDeviceSize>(w) * h * 4, VK_BUFFER_USAGE_TRANSFER_DST_BIT,
                                        rac1::vk::readback_memory_flags(), vk_pf_buf, true);  // (cached: the store reads it ~50 times)
            }
            auto& src_img = const_cast<rac1::vk::GpuImage&>(src.vk_tex);
            rac1::vk::blit_image(src_img, sx0, vy, sx0 + scw, vy + vh, vk_pf_tex, 0, h, w, 0, VK_FILTER_LINEAR,
                                 VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL);
            rac1::vk::submit_immediate([&](VkCommandBuffer cb) {
                VkBufferImageCopy copy{};
                copy.imageSubresource = {VK_IMAGE_ASPECT_COLOR_BIT, 0, 0, 1};
                copy.imageExtent = {static_cast<uint32_t>(w), static_cast<uint32_t>(h), 1};
                vkCmdCopyImageToBuffer(cb, vk_pf_tex.image, VK_IMAGE_LAYOUT_TRANSFER_SRC_OPTIMAL, vk_pf_buf.buffer, 1, &copy);
            });
            pf_valid = true;
            return;
        }
        if (pf_fbo == 0) {
            glGenFramebuffers(1, &pf_fbo);
            glGenTextures(1, &pf_tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &pf_tex, "Frame captures (menu, fades)");
            glGenBuffers(1, &pf_pbo);
            rac1::prof::gl_track_n(rac1::prof::kGlBuffer, 1, &pf_pbo, "Frame captures (menu, fades)");
        }
        if (pf_w != w || pf_h != h) {
            pf_w = w;
            pf_h = h;
            glBindTexture(GL_TEXTURE_2D, pf_tex);
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, pf_fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, pf_tex, 0);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, pf_pbo);
            glBufferData(GL_PIXEL_PACK_BUFFER, static_cast<GLsizeiptr>(w) * h * 4, nullptr, GL_STREAM_READ);
            glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        }
        glBindFramebuffer(GL_READ_FRAMEBUFFER, src.fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pf_fbo);
        glBlitFramebuffer(sx0, vy, sx0 + scw, vy + vh, 0, h, w, 0, GL_COLOR_BUFFER_BIT, GL_LINEAR);  // (row 0 the frame's top)
        glBindFramebuffer(GL_READ_FRAMEBUFFER, pf_fbo);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pf_pbo);
        glPixelStorei(GL_PACK_ALIGNMENT, 4);
        glReadPixels(0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
        if (pf_fence != nullptr) glDeleteSync(pf_fence);
        pf_fence = glFenceSync(GL_SYNC_GPU_COMMANDS_COMPLETE, 0);
        pf_valid = true;
    }
    void store(uint32_t bp, uint32_t psm) {
        g_lazy_shown.resolve();  // (the last picture shown, still in the 3D's colour image: copied now)
        const bool from_shown = !pending && shown != nullptr && shown->valid();
        if ((!pending && !from_shown) || psm != 0) return;
        const uint32_t frame = lwu(0x0013cf20);  // FRAME_1 of the frame's draw environment 0x13cf10 (render_passes.cpp)
        const uint32_t fbp = frame & 0x1ff, fbw = (frame >> 16) & 0x3f, fh = lhu(0x001518d2);
        if (fbw == 0 || fh == 0 || bp < fbp * 32 || bp >= fbp * 32 + fbw * 64 * fh * 4 / 256) return;
        const int w = static_cast<int>(fbw * 64), h = static_cast<int>(fh);
        if (rac1::vk::is_active()) {
            // (the copy of the last picture shown made once for the store's 16 KB blocks, 0x2a0f18's loop: `shown` changes only with
            // a frame shown, which ends pf_valid; each copy outside a frame is a submit and a fence wait)
            if ((from_shown && (!pf_valid || !menu_pic)) || !pf_valid) {
                if (from_shown) {
                    if (!pic.valid() || pic.w != shown->w || pic.h != shown->h) pic.capture(shown->w, shown->h);
                    auto& src_img = const_cast<rac1::vk::GpuImage&>(shown->vk_tex);
                    rac1::vk::blit_image(src_img, 0, 0, shown->w, shown->h, pic.vk_tex, 0, 0, shown->w, shown->h, VK_FILTER_NEAREST);
                    for (int i = 0; i < 4; ++i) view[i] = shown_view[i];
                    menu_pic = true;
                } else {
                    menu_pic = false;
                }
                if (!pf_valid) prefetch(from_shown ? *shown : pic, view[0], view[1], view[2], view[3]);
                rac1::vk::flush_cmd();
            } else if (!from_shown) {
                menu_pic = false;
            }
            if (!pf_valid || w != pf_w || h != pf_h || vk_pf_buf.mapped == nullptr) return;
            pending = false;
            LoadProf prof_all("the stored picture (libgraph's store)");
            std::vector<uint32_t> px(static_cast<size_t>(w) * h);
            std::memcpy(px.data(), vk_pf_buf.mapped, px.size() * 4);
            for (uint32_t& c : px) c = (c & 0x00ffffffu) | 0x80000000u;
            rac1::gs::gs_global().write_ct32_rect(fbp * 32, fbw, static_cast<uint32_t>(w), static_cast<uint32_t>(h), px.data());
            if (!from_shown) {
                hash = hash_of(px.data(), px.size());
                match_id = 0;
            }
            return;
        }
        if (from_shown || !pf_valid) {
            GLint dfb = 0, rfb = 0;
            glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &dfb);
            glGetIntegerv(GL_READ_FRAMEBUFFER_BINDING, &rfb);
            if (from_shown) {  // (the last picture shown, kept whole as the picture the menu draws over: a GPU copy)
                if (pic.fbo == 0 || pic.w != shown->w || pic.h != shown->h) pic.capture(shown->w, shown->h);  // (its set-up)
                glBindFramebuffer(GL_READ_FRAMEBUFFER, shown->fbo);
                glBindFramebuffer(GL_DRAW_FRAMEBUFFER, pic.fbo);
                glBlitFramebuffer(0, 0, shown->w, shown->h, 0, 0, shown->w, shown->h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
                for (int i = 0; i < 4; ++i) view[i] = shown_view[i];
                menu_pic = true;
            } else {
                menu_pic = false;
            }
            if (!pf_valid) prefetch(from_shown ? *shown : pic, view[0], view[1], view[2], view[3]);
            glBindFramebuffer(GL_DRAW_FRAMEBUFFER, static_cast<GLuint>(dfb));
            glBindFramebuffer(GL_READ_FRAMEBUFFER, static_cast<GLuint>(rfb));
        } else {
            menu_pic = false;
        }
        if (!pf_valid || w != pf_w || h != pf_h) return;
        pending = false;  // (pf_valid kept: the store's next 16 KB blocks, 0x2a0f18's loop, read the same picture; the next frame shown ends it)
        LoadProf prof_all("the stored picture (libgraph's store)");
        // (the Scene frame is also the last frame shown: its read-back is the same picture)
        if (pf_fence != nullptr) glClientWaitSync(pf_fence, GL_SYNC_FLUSH_COMMANDS_BIT, 1000000000ull);
        std::vector<uint32_t> px(static_cast<size_t>(w) * h);
        glBindBuffer(GL_PIXEL_PACK_BUFFER, pf_pbo);
        if (const void* m = glMapBufferRange(GL_PIXEL_PACK_BUFFER, 0, static_cast<GLsizeiptr>(px.size()) * 4, GL_MAP_READ_BIT)) {
            std::memcpy(px.data(), m, px.size() * 4);
            glUnmapBuffer(GL_PIXEL_PACK_BUFFER);
        }
        glBindBuffer(GL_PIXEL_PACK_BUFFER, 0);
        for (uint32_t& c : px) c = (c & 0x00ffffffu) | 0x80000000u;  // (the alpha the frame's pixels are drawn with, 0x80)
        rac1::gs::gs_global().write_ct32_rect(fbp * 32, fbw, static_cast<uint32_t>(w), static_cast<uint32_t>(h), px.data());
        if (!from_shown) {
            hash = hash_of(px.data(), px.size());  // (the Vendor's copy back drawn at the window's resolution: its own picture only)
            match_id = 0;
        }
    }
    // the replica's frame after the game put a picture back (gs_draw's Viewport::screen_picture)
    void draw(const rac1::gs::TexImage& img, const rac1::gs::Viewport& vp) {
        const bool same_view = view[0] == vp.x && view[1] == vp.y && view[2] == vp.w && view[3] == vp.h;
        if (rac1::vk::is_active()) {
            int win[4] = {0, 0, 0, 0};
            rac1::vk::get_viewport(win);
            if (hash != 0 && pic.valid() && same_view && (img.id == match_id || hash_of(img.px.data(), img.px.size()) == hash)) {
                match_id = img.id;
                rac1::vk::blit_image(pic.vk_tex, 0, 0, pic.w, pic.h, rac1::vk::active_color_image(), 0, 0, pic.w, pic.h,
                                     VK_FILTER_NEAREST, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            } else {
                target(img.w, img.h);
                rac1::vk::upload_image_2d(vk_rb_tex, img.px.data(), img.px.size() * 4);
                const int fw = static_cast<int>(static_cast<float>(vp.w) * vp.xscale), fx = vp.x + (vp.w - fw) / 2;
                rac1::vk::blit_image(vk_rb_tex, 0, 0, img.w, img.h, rac1::vk::active_color_image(), fx, vp.y + vp.h, fx + fw, vp.y,
                                     VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            }
            rac1::vk::set_viewport(win[0], win[1], win[2], win[3]);
            return;
        }
        GLint screen = 0;
        glGetIntegerv(GL_DRAW_FRAMEBUFFER_BINDING, &screen);
        int win[4] = {0, 0, 0, 0};
        glGetIntegerv(GL_VIEWPORT, win);
        if (hash != 0 && pic.fbo != 0 && same_view && (img.id == match_id || hash_of(img.px.data(), img.px.size()) == hash)) {  // the picture kept, at the window's resolution
            match_id = img.id;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, pic.fbo);
            glBlitFramebuffer(0, 0, pic.w, pic.h, 0, 0, pic.w, pic.h, GL_COLOR_BUFFER_BIT, GL_NEAREST);
        } else {  // the replica's frame over the frame's 4:3 area
            target(img.w, img.h);
            glBindTexture(GL_TEXTURE_2D, rb_tex);
            glPixelStorei(GL_UNPACK_ALIGNMENT, 4);
            glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, img.w, img.h, GL_RGBA, GL_UNSIGNED_BYTE, img.px.data());
            const int fw = static_cast<int>(static_cast<float>(vp.w) * vp.xscale), fx = vp.x + (vp.w - fw) / 2;
            glBindFramebuffer(GL_READ_FRAMEBUFFER, rb_fbo);
            glBlitFramebuffer(0, 0, img.w, img.h, fx, vp.y + vp.h, fx + fw, vp.y, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        }
        glBindFramebuffer(GL_FRAMEBUFFER, static_cast<GLuint>(screen));
        glViewport(win[0], win[1], win[2], win[3]);
    }
};
ScenePicture g_scene_pic;
// the game mode (0x15f5c4) when the last game update began: the mode whose code built the frame's packets
int32_t g_mode_at_update = 0;

// The movie's picture on the screen (the PS2's player uploads each decoded picture to the GS
// frame buffer at a vblank): a texture blitted over the game's 4:3 area, row 0 at the top.
// orig: (host) the movie player's display
struct MoviePicture {
    GLuint fbo = 0, tex = 0;
    rac1::vk::GpuImage vk_tex{};
    int w = 0, h = 0;
    void vk_reset() {
        if (vk_tex.image != VK_NULL_HANDLE) rac1::vk::destroy_image_now(vk_tex);
        if (rac1::vk::is_active()) w = h = 0;
    }
    void draw(const std::vector<uint8_t>& rgba, int pw, int ph, int vx, int vy, int vw, int vh) {
        const int cw = static_cast<int>(static_cast<float>(vw) / g_view_widen);  // (4:3 in widescreen)
        const int x0 = vx + (vw - cw) / 2;
        if (rac1::vk::is_active()) {
            if (vk_tex.image == VK_NULL_HANDLE || pw != w || ph != h) {
                w = pw;
                h = ph;
                rac1::vk::create_image(static_cast<uint32_t>(w), static_cast<uint32_t>(h), VK_FORMAT_R8G8B8A8_UNORM,
                                       VK_IMAGE_USAGE_TRANSFER_SRC_BIT | VK_IMAGE_USAGE_TRANSFER_DST_BIT | VK_IMAGE_USAGE_SAMPLED_BIT,
                                       VK_IMAGE_ASPECT_COLOR_BIT, vk_tex);
            }
            rac1::vk::upload_image_2d(vk_tex, rgba.data(), rgba.size());
            rac1::vk::blit_image(vk_tex, 0, 0, w, h, rac1::vk::main_color_image(), x0, vy + vh, x0 + cw, vy,
                                 VK_FILTER_LINEAR, VK_IMAGE_ASPECT_COLOR_BIT, VK_IMAGE_LAYOUT_COLOR_ATTACHMENT_OPTIMAL);
            return;
        }
        if (fbo == 0) {
            glGenFramebuffers(1, &fbo);
            glGenTextures(1, &tex);
            rac1::prof::gl_track_n(rac1::prof::kGlTexture, 1, &tex, "Movie picture");
        }
        glBindTexture(GL_TEXTURE_2D, tex);
        if (pw != w || ph != h) {
            w = pw;
            h = ph;
            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA8, w, h, 0, GL_RGBA, GL_UNSIGNED_BYTE, nullptr);
            glBindFramebuffer(GL_FRAMEBUFFER, fbo);
            glFramebufferTexture2D(GL_FRAMEBUFFER, GL_COLOR_ATTACHMENT0, GL_TEXTURE_2D, tex, 0);
        }
        glTexSubImage2D(GL_TEXTURE_2D, 0, 0, 0, w, h, GL_RGBA, GL_UNSIGNED_BYTE, rgba.data());
        glBindFramebuffer(GL_READ_FRAMEBUFFER, fbo);
        glBindFramebuffer(GL_DRAW_FRAMEBUFFER, 0);
        glBlitFramebuffer(0, 0, w, h, x0, vy + vh, x0 + cw, vy, GL_COLOR_BUFFER_BIT, GL_LINEAR);
        glBindFramebuffer(GL_FRAMEBUFFER, 0);
    }
};

// The GS memory layout and the two draw environments the boot module's GS start (0x1f34e8 and
// 0x1fa978 in the base ELF, not ported) leaves, NTSC: the frame buffers at 0 / 0xe0000 bytes
// (0x15ee80 / 0x15ee84), the z buffer 0x1b0000 (0x15ee88), the textures from 0x280000
// (0x15ee8c) (the stores at 0x1f356c..0x1f359c); the libgraph block 0x151780 (*(0x15eeb8)):
// +0x150.. the sizes, PSMs, frame pages and z page (the stores at 0x1fa9ec..0x1faa64), the
// draw environments +0x40 (512 x 416, the frame 0xe0000, z test 3, z PSMZ24 0x31) and +0xd0
// (512 x 448, the frame 0, no z) after their A+D GIF tags +0x30 / +0xc0 (0x1faa84..0x1fac10).
// The game's frames CALL them (0x200f40 +0x30, 0x201040 +0xc0); a render to texture ends with
// the first, which puts the screen's frame back.
// orig: (host) a part of the boot module's GS start (0x001f34e8 / 0x001fa978, not ported)
void gs_memory_and_draw_envs() {
    sw(0x0015ee8c, NOT_ADDR(0x280000));
    sw(0x0015ee84, 0xe0000);
    sw(0x0015ee88, NOT_ADDR(0x1b0000));
    sw(0x0015ee80, 0);
    constexpr uint32_t G = 0x00151780;
    sw(0x0015eeb8, G);
    sh(G + 0x156, lwu(0x0015ee84) >> 13);
    sh(G + 0x16e, lwu(0x0015ee88) >> 13);
    sh(G + 0x150, 0x200);
    sh(G + 0x152, 0x1a0);
    sh(G + 0x16c, 0x31);
    sh(G + 0x15e, lwu(0x0015ee80) >> 13);
    sh(G + 0x158, 0x200);
    sh(G + 0x15a, 0x1c0);
    sh(G + 0x168, 0);
    sh(G + 0x16a, 0);
    sw(G + 0x170, 0);
    sh(G + 0x15c, 0);
    sh(G + 0x154, 0);
    sh(G + 0x164, 0);
    gs_set_def_draw_env(G + 0x40, lh(G + 0x154), lh(G + 0x150), lh(G + 0x152), 3, lh(G + 0x16c));
    sd(G + 0x40, (ld(G + 0x40) & ~0x1ffull) | static_cast<uint64_t>(lh(G + 0x156) & 0x1ff));
    sd(G + 0x50, static_cast<uint64_t>(static_cast<int64_t>(lh(G + 0x16e))) | (static_cast<uint64_t>(lhu(G + 0x16c) & 0xf) << 24));
    gs_set_def_draw_env(G + 0xd0, lh(G + 0x15c), lh(G + 0x158), lh(G + 0x15a), 0, 0);
    sd(G + 0xd0, (ld(G + 0xd0) & ~0x1ffull) | static_cast<uint64_t>(lh(G + 0x15e) & 0x1ff));
    for (uint32_t t : {G + 0x30, G + 0xc0}) {
        sd(t, 0x1000000000008008ull);
        sd(t + 8, 0xe);
    }
}

// The overlay image the ported code runs on. On the PS2 the base ELF's overlay load (0x12d8f8,
// after the level's files are read) copies the level's own overlay executable out of its level
// data: every level has its own build of the engine, linked at its own addresses
// (~80% of Veldin's functions are the same code in each level,
// but the overlay's globals, tables and code all move). The ports in src/game/ovl are Veldin's
// build, with Veldin's addresses, made relocatable (ADR 0008): the played level's own overlay is
// loaded (its code, data, its moby class / camera / sound tables) and the ports translate their
// overlay addresses onto it (OV / OVF); the level's own classes run their level-tagged ports.
// orig: (host) stands in for 0x0012d8f8 (the overlay load)
void engine_image_load(int played) {
    // ADR 0008: every level on its own overlay by default; RAC1_OWN_OVERLAY=0 runs
    // every level on Veldin's image instead (the stand-in of for comparison)
    static const bool own = [] {
        const char* e = std::getenv("RAC1_OWN_OVERLAY");
        return e == nullptr || std::strcmp(e, "0") != 0;
    }();
    const int level = own ? played : 0;
    LevelLoader::load_level_overlay(level);  // (after the level's files, as the PS2: their load writes into the overlay's data, which this copy restores)
    ovl_reloc_set_level(level);
}

// The boot module's loader (0x204428: the level's files and sound bank, its reply callback
// 0x22dd78 the boot module's own code) runs before the base ELF loads the overlay: no overlay
// is there, so no overlay translation while it runs (ADR 0008; identity on Veldin).
// orig: (host) the boot module's load before the overlay load 0x12d8f8
template <class F>
auto boot_loader_run(F f) {
    // (the identity, OVL_NONE; on the NTSC-U disc level 0 is that already, on another region's disc it is not)
    const int level = ovl_reloc_level();
    const bool was = !ovl_reloc_identity();
    if (was) ovl_reloc_set_level(OVL_NONE);
    const auto r = f();
    if (was) ovl_reloc_set_level(level);
    return r;
}

// The host's side of a level start: its renderers' copy of the level (geometry, textures, the
// class models), then the overlay image (the overlay load, 0x12d8f8, as above).
// orig: (host) the level's assets for the host renderers, the overlay load
bool level_host_load(int level) {
    if (!disc_has_level(level)) {
        std::fprintf(stderr, "rac1-native: level %d is not on the disc image\n", level);
        return false;
    }
    LoadProf prof_all("level_host_load");
    std::optional<LoadProf> prof("  gs_texture_cache_clear, moby_models_release");
    if (!rac1::vk::is_active() && glad_glDeleteTextures != nullptr) {
        gs_texture_cache_clear();
        moby_models_release();
    }
    prof.reset();
    LevelLoader::load_level_wad(level);
    prof.emplace("  engine_image_load");
    engine_image_load(level);
    g_moby_glb_dir = "assets/generated/moby_glb/level" + std::to_string(level);
    return true;
}

// The next level's files read ahead, on worker threads, from the level exit's start: the host's copy of its
// files (its core decompressed: LevelLoader::prefetch) and the disc's sectors the level exit's load will read (the level's
// header, its data and gameplay files: the OS reads them into its cache). Nothing of the game's state is touched: its own
// reads still happen at its load's steps, in its order; they find the disc's sectors in the host's cache.
// orig: (host) a read ahead of the level's files
void level_prefetch(int level) {
    if (!disc_has_level(level)) return;
    LevelLoader::prefetch(level);
    const uint32_t header_lba = lwu(0x0013a448 + static_cast<uint32_t>(level) * 8);  // (the table of contents, read at the start: toc_read)
    if (header_lba == 0) return;
    std::thread([header_lba] {
        std::vector<uint8_t> h(5 * 2048);
        if (!disc_read(header_lba, 5, h.data())) return;
        auto word = [&h](uint32_t o) { uint32_t v = 0; std::memcpy(&v, &h[o], 4); return v; };
        for (uint32_t o : {0x08u, 0x10u, 0x18u}) disc_will_need(word(o), word(o + 4));  // (0x13a4e8 the data, 0x13a4f0 / 0x13a4f8 the gameplay NTSC / PAL)
    }).detach();
}

// After the level exit (0x292e08) the overlay's main returns to the base ELF's main loop
// (0x12d9d8), whose overlay load (0x12d8f8) jumps to the new overlay's entry: the level starts
// as at boot (0x245c28). When the exit's last card did not finish loading the level (its loop
// left early), the host reads its files as the boot does (0x12f368, 0x204428).
// orig: (host) the base ELF's main loop between two overlays
bool level_enter(bool files_read) {
    const int level = lw(0x0015ed84);
    if (!files_read && !boot_loader_run([&] {
            if (!level_header_read(level) || !level_files_read()) return false;
            ee_write<uint16_t>(0x0015ee48, 3);
            while (level_bank_step() == 0) {
            }
            return true;
        }))
        return false;
    if (!level_host_load(level)) return false;
    {
        LoadProf prof("overlay_entry_start (the game's level setup)");
        overlay_entry_start();
    }
    std::printf("[orig] level %d started: mode %d, mobys %08x..%08x\n", level, lw(OV(0x0015f5c4)), lwu(OV(0x0015ffd8)), lwu(OV(0x0015ffdc)));
    return true;
}

// Quitting to the title (the pause menu's Quit): after the level exit (its read of the boot module,
// 0x243d38) the overlay's main returns to the base ELF's main loop, whose overlay load (0x12d8f8)
// copies the boot module read back over the level's overlay and jumps to its entry. The host loads
// the boot module's own image in its place (front_image_load: see boot_return_start), the ports
// relocated onto it, and starts it: the title, its main menu, a new game or a loaded one from
// there as at the boot.
// orig: (host) the base ELF's main loop between the overlay and the boot module read back
bool title_return() {
    if (!front_image_load()) {
        std::fprintf(stderr, "rac1-native: cannot read the boot module from %s\n", disc_boot_elf_path().c_str());
        return false;
    }
    ovl_reloc_set_level(OVL_FRONT);
    boot_return_start();
    std::printf("[orig] quit to the title: the boot module started again\n");
    return true;
}

void save_ppm(const char* path, int w, int h) {
    if (rac1::vk::is_active()) {
        rac1::vk::save_ppm(path, w, h);
        return;
    }
    std::vector<unsigned char> px(static_cast<size_t>(w) * h * 3);
    glPixelStorei(GL_PACK_ALIGNMENT, 1);  // rows packed (a window width * 3 not a multiple of 4 overran px)
    glReadPixels(0, 0, w, h, GL_RGB, GL_UNSIGNED_BYTE, px.data());
    std::ofstream out(path, std::ios::binary);
    out << "P6\n" << w << " " << h << "\n255\n";
    for (int y = h - 1; y >= 0; --y) out.write(reinterpret_cast<char*>(&px[static_cast<size_t>(y) * w * 3]), w * 3);
}

// A modern pad's stick has a round gate (a diagonal reads about 0.7 on each axis); the
// DualShock 2 the game reads has a square-ish one (a full diagonal reads near the corners).
// The host maps the round range onto the square: same direction, the larger axis at the
// stick's deflection.
// The host's controls (host_input.h): the bindings (~/.config/rac1-native/controls.txt), the standard
// controller database, the mouse captured for its stick bindings. Set up by host_input_init in a window.
namespace hi = rac1::host_input;
hi::Bindings g_bindings = hi::default_bindings();
bool g_input_ready = false;
bool g_cursor_captured = false;
bool g_capture_click_held = false;  // the click that captured the cursor, not given to the game until released
bool g_cursor_free = false;         // the debug menu is open: the cursor is the menu's
double g_cursor_x = 0.0, g_cursor_y = 0.0;
float g_mouse_dx = 0.0f, g_mouse_dy = 0.0f;  // the movement since the last game frame

// The standard game controller database (SDL's gamecontrollerdb.txt format, which GLFW reads): the copy bundled in
// third_party/gamecontrollerdb, then the player's own additions (~/.config/rac1-native/gamecontrollerdb.txt), then
// SDL_GAMECONTROLLERCONFIG (what Steam sets for its configured pads). GLFW's built-in mappings stay under them.
void load_controller_db() {
    auto load_file = [](const std::filesystem::path& p) {
        std::ifstream f(p);
        if (!f) return;
        std::stringstream ss;
        ss << f.rdbuf();
        const bool ok = glfwUpdateGamepadMappings(ss.str().c_str()) == GLFW_TRUE;
        std::printf("[controls] controller database %s%s\n", p.string().c_str(), ok ? "" : " (not read)");
    };
    load_file("third_party/gamecontrollerdb/gamecontrollerdb.txt");
    const std::filesystem::path cfg = std::filesystem::path(hi::controls_path()).parent_path();
    if (!cfg.empty() && std::filesystem::exists(cfg / "gamecontrollerdb.txt")) load_file(cfg / "gamecontrollerdb.txt");
    if (const char* env = std::getenv("SDL_GAMECONTROLLERCONFIG"); env != nullptr && *env != 0) glfwUpdateGamepadMappings(env);
}

// A joystick no mapping covers, on Linux, with the layout every controller of the kernel's Xbox driver (xpad) has: 11
// buttons, 6 axes and a hat, in the evdev order GLFW keeps (A B X Y, LB RB, Back Start Guide, the sticks' clicks; the
// left stick, LT, the right stick, RT). Many Xbox-compatible pads missing from the database are such (issue #1): one is
// given the Xbox layout as its mapping, under its own GUID. Any other unmapped joystick stays unused, and the log says
// how to add its mapping. RAC1_PAD_FALLBACK=force gives the layout to such a pad even when it is mapped (a test: a
// mapped Xbox pad then plays as an unknown one would); RAC1_PAD_FALLBACK=0 turns the fallback off.
std::array<std::string, GLFW_JOYSTICK_LAST + 1> g_joystick_seen;  // the GUID each slot was looked at with ("" none)

void joystick_check(int j) {
    if (!glfwJoystickPresent(j)) {
        g_joystick_seen[j].clear();
        return;
    }
    const char* guid = glfwGetJoystickGUID(j);
    if (guid == nullptr || g_joystick_seen[j] == guid) return;
    g_joystick_seen[j] = guid;
    const char* mode = std::getenv("RAC1_PAD_FALLBACK");
    const bool force = mode != nullptr && std::strcmp(mode, "force") == 0, off = mode != nullptr && std::strcmp(mode, "0") == 0;
    int buttons = 0, axes = 0, hats = 0;
    glfwGetJoystickButtons(j, &buttons);
    glfwGetJoystickAxes(j, &axes);
    glfwGetJoystickHats(j, &hats);
    buttons -= 4 * hats;  // (GLFW lists each hat's four directions after the buttons too: GLFW_JOYSTICK_HAT_BUTTONS)
    std::string name = glfwGetJoystickName(j) != nullptr ? glfwGetJoystickName(j) : "unknown controller";
#if defined(__linux__)
    const bool xpad_layout = (buttons == 11 || buttons == 12) && axes == 6 && hats == 1;  // (12: the Series pads' Share button, last)
#else
    const bool xpad_layout = false;  // (Windows: XInput pads have GLFW's own mappings)
#endif
    if (!off && xpad_layout && (force || !glfwJoystickIsGamepad(j))) {
        std::replace(name.begin(), name.end(), ',', ' ');  // (the mapping's separator)
        const std::string mapping = std::string(guid) + "," + name +
            ",a:b0,b:b1,x:b2,y:b3,leftshoulder:b4,rightshoulder:b5,back:b6,start:b7,guide:b8,leftstick:b9,rightstick:b10,"
            "leftx:a0,lefty:a1,lefttrigger:a2,rightx:a3,righty:a4,righttrigger:a5,dpup:h0.1,dpright:h0.2,dpdown:h0.4,dpleft:h0.8,"
            "platform:Linux,";
        glfwUpdateGamepadMappings(mapping.c_str());
        std::printf("[controls] joystick %d: %s (GUID %s)%s: given the Xbox layout (if its buttons are wrong, add its "
                    "mapping: docs/input.md)\n", j, name.c_str(), guid, force ? "" : ", not in the controller database");
    } else if (glfwJoystickIsGamepad(j)) {
        std::printf("[controls] joystick %d: %s as %s\n", j, name.c_str(), glfwGetGamepadName(j));
    } else {
        std::printf("[controls] joystick %d: %s (GUID %s, %d buttons, %d axes, %d hats): no mapping, not used (add its "
                    "mapping: docs/input.md)\n", j, name.c_str(), guid, buttons, axes, hats);
    }
    std::fflush(stdout);
}

void host_input_init(GLFWwindow* win) {
    load_controller_db();
    g_bindings = hi::load_or_create(hi::controls_path());
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) joystick_check(j);
    // every joystick GLFW sees, on one line (for bug reports: issue #98)
    std::string list;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        if (!glfwJoystickPresent(j)) continue;
        const char* name = glfwGetJoystickName(j);
        const char* guid = glfwGetJoystickGUID(j);
        list += (list.empty() ? "" : "; ") + std::to_string(j) + ": " + (name != nullptr ? name : "?") + " (GUID " +
                (guid != nullptr ? guid : "?") + ", gamepad " + (glfwJoystickIsGamepad(j) ? "yes" : "no") + ")";
    }
    std::printf("[controls] joysticks: %s\n", list.empty() ? "none" : list.c_str());
    std::fflush(stdout);
    if (glfwRawMouseMotionSupported()) glfwSetInputMode(win, GLFW_RAW_MOUSE_MOTION, GLFW_TRUE);
    g_input_ready = true;
}

void cursor_capture(GLFWwindow* win, bool on) {
    if (on == g_cursor_captured) return;
    glfwSetInputMode(win, GLFW_CURSOR, on ? GLFW_CURSOR_DISABLED : GLFW_CURSOR_NORMAL);
    glfwGetCursorPos(win, &g_cursor_x, &g_cursor_y);
    g_cursor_captured = on;
    g_mouse_dx = g_mouse_dy = 0.0f;
}

// Once per host frame: the cursor captured by a click in the window (when a mouse movement is bound and mouse_look is
// on), let go when the window loses the focus or the debug menu opens; its movement gathered for the next game frame.
void host_input_poll(GLFWwindow* win, bool cursor_free) {
    if (!g_input_ready) return;
    g_cursor_free = cursor_free;
    bool mouse_bound = false;
    for (const auto& v : g_bindings.map)
        for (const auto& in : v) mouse_bound |= in.kind == hi::InputKind::kMouseAxis;
    const bool focused = glfwGetWindowAttrib(win, GLFW_FOCUSED) != 0;
    if (g_cursor_captured && (!focused || cursor_free || !g_bindings.mouse_look)) cursor_capture(win, false);
    if (!g_cursor_captured && focused && !cursor_free && g_bindings.mouse_look && mouse_bound &&
        glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) == GLFW_PRESS) {
        cursor_capture(win, true);
        g_capture_click_held = true;
    }
    if (g_capture_click_held && glfwGetMouseButton(win, GLFW_MOUSE_BUTTON_LEFT) != GLFW_PRESS) g_capture_click_held = false;
    if (g_cursor_captured) {
        double x = 0.0, y = 0.0;
        glfwGetCursorPos(win, &x, &y);
        g_mouse_dx += static_cast<float>(x - g_cursor_x);
        g_mouse_dy += static_cast<float>(y - g_cursor_y);
        g_cursor_x = x;
        g_cursor_y = y;
    }
}

// What the window's keyboard, mouse and mapped gamepads hold now (host_input.h's HostState).
// `raw`: the mouse's buttons as they are (a rebinding on the Controls page takes any click)
// Every mapped gamepad is read and merged (hi::merge_pad), not only the first (issue #98: on Windows a virtual or
// silent device - a virtual pad's driver, a duplicate of the same pad - can come first and hide the real one). With
// more than one, a pad counts only once it has moved from what it read when first seen (a button, an axis by 0.3),
// so one stuck at a deflection (a wheel's pedal, a throttle) does not hold an input forever.
std::string g_pad_name;  // the name of the mapped gamepad last used ("" none), for the Controls page
struct PadSeen {
    std::string guid;  // ("" none yet)
    bool active = false;
    float first[6]{};
    uint64_t last_held = 0;  // (the poll it last held something at; 0 never)
};
std::array<PadSeen, GLFW_JOYSTICK_LAST + 1> g_pads_seen;
uint64_t g_pad_polls = 0;
hi::HostState host_state(GLFWwindow* win, bool raw = false) {
    hi::HostState st;
    for (int k = GLFW_KEY_SPACE; k <= GLFW_KEY_LAST; ++k) st.keys[k] = glfwGetKey(win, k) == GLFW_PRESS;
    if (!g_cursor_free) {  // (the debug menu's clicks are not the game's)
        for (int m = 0; m < static_cast<int>(st.mouse_buttons.size()); ++m) st.mouse_buttons[m] = glfwGetMouseButton(win, m) == GLFW_PRESS;
        // (the click that captured the cursor is not the game's; nor a left click while the cursor is not captured)
        if (!raw && (g_capture_click_held || (g_bindings.mouse_look && !g_cursor_captured))) st.mouse_buttons[GLFW_MOUSE_BUTTON_LEFT] = false;
    }
    st.mouse_dx = g_mouse_dx;
    st.mouse_dy = g_mouse_dy;
    g_mouse_dx = g_mouse_dy = 0.0f;
    ++g_pad_polls;
    struct Pad {
        int j;
        bool buttons[15];
        float axes[6];
    };
    std::array<Pad, GLFW_JOYSTICK_LAST + 1> pads;
    int n = 0;
    for (int j = GLFW_JOYSTICK_1; j <= GLFW_JOYSTICK_LAST; ++j) {
        if (g_input_ready) joystick_check(j);  // (a pad connected while the game runs)
        GLFWgamepadstate s;
        if (!glfwJoystickIsGamepad(j) || !glfwGetGamepadState(j, &s)) continue;
        Pad& p = pads[n++];
        p.j = j;
        for (int b = 0; b < 15; ++b) p.buttons[b] = s.buttons[b] == GLFW_PRESS;
        for (int a = 0; a < 6; ++a) p.axes[a] = s.axes[a];
        PadSeen& seen = g_pads_seen[j];
        const char* g = glfwGetJoystickGUID(j);
        const std::string guid = g != nullptr && *g != 0 ? g : "?";
        if (seen.guid != guid) {  // (a pad newly seen in this slot)
            seen = PadSeen{};
            seen.guid = guid;
            std::copy(std::begin(p.axes), std::end(p.axes), seen.first);
        }
        for (int a = 0; a < 6 && !seen.active; ++a) seen.active = std::fabs(p.axes[a] - seen.first[a]) > 0.3f;
        for (int b = 0; b < 15 && !seen.active; ++b) seen.active = p.buttons[b];
        if (hi::pad_held(p.buttons, p.axes)) seen.last_held = g_pad_polls;
    }
    int named = -1;  // (the pad last used; the first one when none has been)
    for (int i = 0; i < n; ++i) {
        const PadSeen& seen = g_pads_seen[pads[i].j];
        if (n == 1 || seen.active) hi::merge_pad(st, pads[i].buttons, pads[i].axes);
        if (named < 0 || seen.last_held > g_pads_seen[pads[named].j].last_held) named = i;
    }
    st.pad_present = n > 0;
    g_pad_name.clear();
    if (named >= 0)
        if (const char* nm = glfwGetGamepadName(pads[named].j)) g_pad_name = nm;
    return st;
}

// whether the debug menu shows the layer a VU1 program draws on the VU emulation path: the world's tfrag / tie /
// shrub programs (unless the MRP world draws), the mobys' (unless the MRP mobys draw), the particles' (unless the MRP effects draw)
bool vu1_layer_shown(uint64_t hash) {
    if (g_render_tfrag && g_render_tie && g_render_shrub && g_render_moby && g_render_fx) return true;
    if (hash == 0x1585563a86921802ull) return g_render_moby || g_moby_renderer == MobyRenderer::kMrp;  // (moby_vu1.cpp)
    if (hash == 0x901435981696d0d3ull) return g_render_fx || (g_mrp_fx && g_world_mrp);  // (particles_vu1.cpp)
    if (g_world_mrp) return true;
    int n = 0;
    const WorldVu1Program* progs = world_vu1_programs(n);
    for (int i = 0; i < n; ++i)
        if (progs[i].hash == hash) return i / 2 == 0 ? g_render_tfrag : i / 2 == 1 ? g_render_tie : g_render_shrub;  // (tfrag A / B, tie A / B, shrub A / B)
    return true;
}

// The Controls page's host side (src/game/enhance/controls.h): the bindings shown, rebound (the game's pad
// held idle meanwhile, pad_packet), saved to the controls file at each change. A headless run saves nothing unless
// RAC1_CONTROLS_FILE names a file (then it reads it at the start too).
hi::Remapper g_remap;
bool g_controls_save = false;
// (testing) RAC1_SIM_HOST=<input>,<input>,...: the host's devices as the `h` keys of a RAC1_SIM_INPUT menu: / title: script
// press them, one input of the list for each `h` in turn (the input names of controls.txt: key:k, pad:b, pad:-lefty, mouse:+x)
hi::HostState g_sim_host;

void controls_save() {
    if (!g_controls_save) return;
    if (!hi::save(g_bindings, hi::controls_path())) std::fprintf(stderr, "[controls] %s: not written\n", hi::controls_path().c_str());
}

class HostControls final : public rac1::ControlsHost {
public:
    std::string row_value(int row, int device) override { return hi::row_label(g_bindings, row, dev(device), style()); }
    std::string pad_name() override { return g_pad_name; }
    void rebind_begin(int row, int device) override { g_remap.begin(row, dev(device)); }
    bool rebind_waiting(int* step) override {
        if (step != nullptr) *step = g_remap.step();
        return g_remap.waiting();
    }
    int rebind_result(std::vector<int>* swapped) override {
        std::vector<int> controls;
        const hi::Remapper::Result r = g_remap.take_result(&controls);
        if (r == hi::Remapper::Result::kNone) return 0;
        if (r == hi::Remapper::Result::kCancelled) return 2;
        controls_save();
        if (swapped != nullptr) {  // (the controls as the page's rows)
            swapped->clear();
            for (const int c : controls)
                for (int row = 0; row < hi::kRemapRowCount; ++row) {
                    const hi::RemapRow& rr = hi::remap_row(row);
                    bool in = false;
                    for (int k = 0; k < rr.count; ++k) in |= static_cast<int>(rr.controls[k]) == c;
                    if (in && std::find(swapped->begin(), swapped->end(), row) == swapped->end()) swapped->push_back(row);
                }
        }
        return 1;
    }
    void reset(int device) override {
        hi::reset_device(g_bindings, dev(device));
        controls_save();
    }
    bool invert_look() override { return g_bindings.invert_look_y; }
    void set_invert_look(bool on) override {
        g_bindings.invert_look_y = on;
        controls_save();
    }
    float mouse_sensitivity() override { return g_bindings.mouse_sensitivity; }
    void set_mouse_sensitivity(float s) override {
        g_bindings.mouse_sensitivity = s;
        controls_save();
    }

private:
    static hi::Device dev(int d) { return d == 0 ? hi::Device::kPad : hi::Device::kKeyboardMouse; }
    // the pad's labels: its own (a PlayStation pad and no pad: the game's button glyphs)
    static hi::PadStyle style() { return g_pad_name.empty() ? hi::PadStyle::kPlayStation : hi::pad_style(g_pad_name); }
};
HostControls g_host_controls;

// (testing) the `h` key at `pos` of a script: the next RAC1_SIM_HOST input held this frame
void sim_host_press(const char* keys, size_t pos) {
    static std::vector<hi::HostInput> list = [] {
        std::vector<hi::HostInput> v;
        if (const char* e = std::getenv("RAC1_SIM_HOST")) {
            std::string s(e);
            size_t p = 0;
            while (p <= s.size()) {
                size_t c = s.find(',', p);
                if (c == std::string::npos) c = s.size();
                hi::HostInput in;
                if (hi::input_from_string(s.substr(p, c - p), in)) v.push_back(in);
                else std::fprintf(stderr, "RAC1_SIM_HOST: '%s' is no input\n", s.substr(p, c - p).c_str());
                p = c + 1;
            }
        }
        return v;
    }();
    size_t n = 0;
    for (size_t i = 0; i < pos; ++i) n += keys[i] == 'h';
    if (n >= list.size()) return;
    const hi::HostInput& in = list[n];
    switch (in.kind) {
        case hi::InputKind::kKey: g_sim_host.keys[in.code] = true; break;
        case hi::InputKind::kMouseButton: g_sim_host.mouse_buttons[in.code] = true; break;
        case hi::InputKind::kMouseAxis: (in.code == 0 ? g_sim_host.mouse_dx : g_sim_host.mouse_dy) = 100.0f * static_cast<float>(in.sign); break;
        case hi::InputKind::kPadButton: g_sim_host.pad_buttons[in.code] = true; break;
        case hi::InputKind::kPadAxis: g_sim_host.pad_axes[in.code] = static_cast<float>(in.sign); break;
        default: break;
    }
}

// The controller packet the pad returns (see g_host_pad_packet): 2 button bytes active low
// (high byte select L3 R3 start up right down left, low byte L2 R2 L1 R1 triangle circle cross
// square), then the right and left sticks (0..255, 0x7f centred, up = 0).
// orig: (host) the game's memory a pipelined frame's drawing reads (runtime/kernel/ee_copy.h), copied at its prepare: the base
// executable's data and every level overlay's data sections (the overlays' code starts at 0x1dd300 .. 0x21c200, their data
// below it: tools/overlay_match's maps) - the lights, the light sets, the hero, the camera, the level's state -, and the moby
// array (0x15ffd8 .. 0x15ffe0, a moby 0x100 bytes, the end's sentinel with it)
void draw_memory_take() {
    rac1::EeCopyRange r[2];
    r[0] = {0x00130000u, 0x00220000u};
    const uint32_t first = lwu(OV(0x0015ffd8u)), last = lwu(OV(0x0015ffe0u));
    if (first >= 0x00100000u && last >= first && last - first < (4u << 20) && last + 0x200u <= 0x02000000u) r[1] = {first, last + 0x200u};
    rac1::ee_copy_take(r, 2);
}
uint64_t g_overlapped = 0;  // (the host frames whose game frames ran on the game thread: reported at the end)
// The devices read for the game frames the game thread runs (pad_presample): GLFW's input is the main thread's. One state per
// game frame, read as pad_packet would have read them one after the other (the mouse's motion in the first).
std::vector<hi::HostState> g_pad_pre;
// orig: (host) the devices' states for the host frame's `steps` game frames, read now (main thread)
void pad_presample(GLFWwindow* win, int steps) {
    g_pad_pre.clear();
    if (!g_input_ready || std::getenv("RAC1_SIM_INPUT") != nullptr) return;
    for (int k = 0; k < steps; ++k) g_pad_pre.push_back(host_state(win, g_remap.active()));
}
// orig: (host) the pad packet of a game frame (`pre` >= 0: the devices read for it by pad_presample)
void pad_packet(GLFWwindow* win, uint64_t frame, int pre = -1) {
    uint16_t held = 0;
    float lx = 0, ly = 0, rx = 0, ry = 0;
    // (a headless run (win == nullptr) ignores the physical gamepads: tests are deterministic; so does
    // a simulated run, RAC1_SIM_INPUT, in a window)
    static const bool simulated = std::getenv("RAC1_SIM_INPUT") != nullptr;
    g_sim_host = hi::HostState{};
    g_sim_host.pad_present = true;
    hi::HostState devices;  // (what the Controls page's rebinding reads)
    bool have_devices = false;
    if (win != nullptr && !simulated && g_input_ready) {
        devices = pre >= 0 && static_cast<size_t>(pre) < g_pad_pre.size() ? g_pad_pre[static_cast<size_t>(pre)] : host_state(win, g_remap.active());
        have_devices = true;
        const hi::PadOutput o = hi::compute(g_bindings, devices);
        held = o.held;
        lx = o.lx;
        ly = o.ly;
        rx = o.rx;
        ry = o.ry;
    }
#if defined(__WIIU__)
    else if (!simulated) {  // (the Wii U: no window, its pads read through the port)
        devices = rac1_wiiu_pad_state();
        have_devices = true;
        const hi::PadOutput o = hi::compute(g_bindings, devices);
        held = o.held;
        lx = o.lx;
        ly = o.ly;
        rx = o.rx;
        ry = o.ry;
    }
#endif
    // (testing) RAC1_SIM_DELAY=N: the RAC1_SIM_INPUT script starts N frames late (after a level's arrival scene)
    static const uint64_t sim_delay = std::getenv("RAC1_SIM_DELAY") != nullptr ? std::strtoull(std::getenv("RAC1_SIM_DELAY"), nullptr, 0) : 0;
    if (const char* sim = frame >= sim_delay ? std::getenv("RAC1_SIM_INPUT") : nullptr) {
        frame -= sim_delay;
        if (std::strcmp(sim, "forward") == 0 && frame >= 30) ly = -1.0f;
        if (std::strcmp(sim, "jump") == 0 && (frame % 60) < 5) held |= 0x0040;
        if (std::strcmp(sim, "dive") == 0 && frame >= 30 && (frame % 40) < 4) held |= 0x0080;  // (testing) square: the dive from the water's surface
        if (std::strcmp(sim, "turn") == 0 && frame >= 30) { ly = -1.0f; lx = (frame / 90) % 2 ? 1.0f : -1.0f; }
        if (std::strcmp(sim, "attack") == 0 && frame >= 30) {  // walk and swing the wrench
            ly = frame % 240 < 120 ? -1.0f : 0.0f;
            lx = (frame / 240) % 2 ? 0.6f : -0.6f;
            if ((frame % 40) < 4) held |= 0x0080;
        }
        if (std::strcmp(sim, "quick") == 0 && frame >= 130 && frame < 200) {  // the quick select (triangle) and the stick
            held |= 0x0010;
            if (frame >= 150) { lx = 0.0f; ly = -1.0f; }
        }
        if (std::strcmp(sim, "fire") == 0) {  // the first quick-select item chosen, then fired (circle)
            if (frame >= 130 && frame < 170) { held |= 0x0010; if (frame >= 140) ly = -1.0f; }
            if (frame >= 200 && (frame % 40) < 4) held |= 0x0020;
        }
        if (std::strcmp(sim, "look") == 0 && frame >= 30 && frame < 300) {  // the first-person look (L1) and the right stick
            held |= 0x0004;
            if (frame >= 60 && frame < 150) rx = 1.0f;
            if (frame >= 150 && frame < 200) ry = -1.0f;
        }
        if (std::strcmp(sim, "start") == 0) {  // the pause menu: Start, down twice, up once, Start again to leave
            if ((frame >= 120 && frame < 124) || (frame >= 420 && frame < 424)) held |= 0x0800;
            if ((frame >= 200 && frame < 204) || (frame >= 240 && frame < 244)) held |= 0x4000;
            if (frame >= 280 && frame < 284) held |= 0x1000;
        }
        if (std::strncmp(sim, "ship", 4) == 0) {  // testing: Veldin's ending. "ship": Start skips the opening cutscene
            // (game mode 2 before frame 1000); "shipskip": Start also skips every later cutscene, the movies and the level exit's story
            // (every game mode but 0 and 3)
            const int32_t md = lw(OV(0x0015f5c4));
            const bool all = std::strcmp(sim, "shipskip") == 0;
            if (all) {
                if (md != 0 && md != 3 && frame % 20 == 0) held |= 0x0800;  // one frame a press, never in gameplay (the pause menu)
            } else if (md == 2 && frame < 1000 && frame % 20 < 4) {
                held |= 0x0800;
            }
        }
        if (std::strcmp(sim, "explore") == 0) {  // testing (the levels' checks): out of gameplay Start every 20 frames as "shipskip";
            // in gameplay forward, a turn of 40 frames every 240, a jump every 90, an attack every 150
            const int32_t md = lw(OV(0x0015f5c4));
            if (md != 0 && md != 3) {
                if (frame % 20 == 0) held |= 0x0800;
            } else if (md == 0) {
                ly = -1.0f;
                if (frame % 240 < 40) lx = 1.0f;
                if (frame % 90 < 4) held |= 0x0040;
                if (frame % 150 < 3) held |= 0x0080;
            }
        }
        if (std::strcmp(sim, "animsheet") == 0) {  // testing: Start at 120; from 200 the hero's update off (the frame's
            // enable bit 1) and his animation set n (RAC1_ANIM_FIRST.., 16 frames each) started at frame 0; shots via RAC1_SHOTS
            if (frame >= 120 && frame < 124) held |= 0x0800;
            constexpr OvConst<0x0016c058u> C{};
            if (frame == 200 && std::getenv("RAC1_ANIM_FREEZE") != nullptr) sw(C + 8, lwu(C + 8) & ~1u);
            static const int first = std::getenv("RAC1_ANIM_FIRST") ? std::atoi(std::getenv("RAC1_ANIM_FIRST")) : 0;
            if (frame >= 208 && (frame - 208) % 16 == 0) {
                const int n = first + static_cast<int>((frame - 208) / 16);
                if (n < 128) {
                    rac1::hero_anim_set(static_cast<uint32_t>(n), 0, -1.0f);
                    std::printf("[anim] frame %llu set %d\n", static_cast<unsigned long long>(frame), n);
                }
            }
        }
        if (std::strcmp(sim, "wrench") == 0) {  // testing: Start at 120; square at 200 / 216 / 232 (the combo), R1 +
            // square at 320 (the throw) with the stick forward from 330; each frame: state, combo step, animation time, speed
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if ((frame >= 200 && frame < 203) || (frame >= 216 && frame < 219) || (frame >= 232 && frame < 235)) held |= 0x0080;
            if (frame >= 318 && frame < 326) held |= 0x0008;
            if (frame >= 320 && frame < 323) held |= 0x0080;
            if (frame >= 330 && frame < 450) ly = -1.0f;
            if (frame >= 198 && frame < 450)
                std::printf("[wrench] frame %llu state %d step %d t %.1f speed %.2f thrown %d\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            lw(0x0013f350 + 0xa60), lwc1(0x0013f350 + 0xaa8), std::hypot(lwc1(0x0013f430), lwc1(0x0013f434)) * 60.0f, lbu(0x0013f350 + 0x10aa));
        }
        if (std::strcmp(sim, "djump") == 0) {  // testing: Start at 120; cross at 200 and again at 216 (the double
            // jump); RAC1_PACK=<item> asks that pack for slot 3 at frame 150 (0x14166c, owned 0x13d4c0+item); the height logged
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 150 && std::getenv("RAC1_PACK") != nullptr) {  // (the slot's record itself: the physics read its item)
                const uint32_t id = static_cast<uint32_t>(std::strtol(std::getenv("RAC1_PACK"), nullptr, 0));
                sb(0x0013d4c0 + id, 1);
                sw(0x0014166c, id);
                sw(0x0013f350 + 0x11a8, id);
            }
            static const uint64_t again = std::getenv("RAC1_DJ_AT") != nullptr ? std::strtoull(std::getenv("RAC1_DJ_AT"), nullptr, 10) : 216;
            if ((frame >= 200 && frame < 212) || (frame >= again && frame < again + 12)) held |= 0x0040;
            if (frame >= 195 && frame < 300)
                std::printf("[djump] frame %llu state %d z %.3f slot3 %d/%d\n", static_cast<unsigned long long>(frame), lw(0x001413d4), lwc1(lwu(0x001413d0) + 0x18),
                            lw(0x0013f350 + 0x11a4), lw(0x0013f350 + 0x11a8));
        }
        if (std::strcmp(sim, "move") == 0) {  // testing: Start at 120; the stick forward 200..300, back 300..360,
            // let go 360..430, forward again 430..520 with cross (jump) at 440; each frame: state, speed (u/s), facing, height
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 300) ly = -1.0f;
            if (frame >= 300 && frame < 360) ly = 1.0f;
            if (frame >= 430 && frame < 520) ly = -1.0f;
            if (frame >= 440 && frame < 444) held |= 0x0040;
            if (frame >= 195 && frame < 520)
                std::printf("[move] frame %llu state %d speed %.2f facing %.3f z %.3f\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            std::hypot(lwc1(0x0013f430), lwc1(0x0013f434)) * 60.0f, lwc1(0x0013f350 + 0x98), lwc1(lwu(0x001413d0) + 0x18));
        }
        if (std::strcmp(sim, "spin") == 0) {  // testing: Start at 120 (the arrival scene skipped), then the right stick held
            // right from 200: the hero still, the camera turning around him (the shadows' stability)
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200) rx = 1.0f;
        }
        if (std::strcmp(sim, "stick") == 0 || std::strcmp(sim, "stickaim") == 0) {  // testing: Start at 120; the right
            // stick right 200..240, up 260..300 (with L2 held 190..340 for "stickaim"); the camera's heading / pitch and options
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (sim[5] == 'a' && frame >= 190 && frame < 340) held |= 0x0001;
            if (frame >= 200 && frame < 240) rx = 1.0f;
            if (frame >= 260 && frame < 300) ry = -1.0f;
            if (frame >= 190 && frame < 360 && frame % 10 == 0) {
                const uint32_t c = lwu(OV(0x00166e00));
                std::printf("[stick] frame %llu opt_x %d opt_y %d heading %.3f pitch %.3f\n", static_cast<unsigned long long>(frame), lw(0x0015ede0), lw(0x0015eddc),
                            lwc1(OV(0x00166dd8)), std::atan2(lwc1(c + 8), std::hypot(lwc1(c), lwc1(c + 4))));
            }
        }
        if (std::strcmp(sim, "aimwrench") == 0) {  // testing: "aimfire", then square (the wrench) at 420 and 460;
            // the hero's held-item mobys logged
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 400) held |= 0x0001;
            if (frame >= 280 && frame < 360) held |= 0x0002;
            if ((frame >= 420 && frame < 424) || (frame >= 460 && frame < 464)) held |= 0x0080;
            if (frame % 20 == 0 && frame >= 140)
                std::printf("[wrench] frame %llu state %d item %u held-class %d 0x141660 %08x %08x %08x %08x\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            lwu(0x0013f350 + 0x10b8), lwu(0x0013f350 + 0x15f0) != 0 ? lh(lwu(0x0013f350 + 0x15f0) + 0xa6) : -1, lwu(0x00141660), lwu(0x00141664), lwu(0x00141668),
                            lwu(0x0014166c));
        }
        if (std::strcmp(sim, "aimfire") == 0) {  // testing: Start at 120, L2 held 200..400, the right stick up
            // 220..240 (the camera's pitch), R2 held 280..360 (with RAC1_GIVE_ITEMS=0xf RAC1_EQUIP=0xf: the blaster)
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 400) held |= 0x0001;
            if (frame >= 220 && frame < 240) ry = -1.0f;
            if (frame >= 280 && frame < 360) held |= 0x0002;
        }
        if (std::strcmp(sim, "aimback") == 0) {  // testing: Start at 120, L2 held 200..420: the right stick turns
            // the aim about 180 degrees (200..270), then the stick back (280..400): the backpedal
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 420) held |= 0x0001;
            if (frame >= 200 && frame < 270) rx = 1.0f;
            if (frame >= 280 && frame < 400) ly = 1.0f;
        }
        if (std::strcmp(sim, "flips") == 0) {  // testing: Start at 120; a strafe flip (L2 + stick right + cross at
            // 260), then the crouch flip (R1 held 380..430, the stick flicked left at 400, cross at 402); state and position logged
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 320) { held |= 0x0001; if (frame >= 240) lx = 1.0f; }
            if (frame >= 260 && frame < 264) held |= 0x0040;
            if (frame >= 380 && frame < 430) held |= 0x0008;
            if (frame >= 400 && frame < 420) lx = -1.0f;
            if (frame >= 402 && frame < 406) held |= 0x0040;
            if (frame >= 250 && frame < 480)
                std::printf("[flips] frame %llu state %d pos %.2f %.2f %.2f\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            lwc1(lwu(0x001413d0) + 0x10), lwc1(lwu(0x001413d0) + 0x14), lwc1(lwu(0x001413d0) + 0x18));
        }
        if (std::strcmp(sim, "aimjump") == 0) {  // testing: Start at 120, L2 held 200..420 with the stick right,
            // cross (jump) at 260 and 340; every 5 frames: state, facing, camera heading, position (x, y, z)
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 420) { held |= 0x0001; lx = 1.0f; }
            if ((frame >= 260 && frame < 264) || (frame >= 340 && frame < 344)) held |= 0x0040;
            if (frame >= 250 && frame < 400 && frame % 5 == 0)
                std::printf("[aimjump] frame %llu state %d facing %.3f camera %.3f pos %.2f %.2f %.2f\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            lwc1(0x0013f350 + 0x98), lwc1(OV(0x00166dd8)), lwc1(lwu(0x001413d0) + 0x10), lwc1(lwu(0x001413d0) + 0x14), lwc1(lwu(0x001413d0) + 0x18));
        }
        if (std::strcmp(sim, "aimstrafe") == 0) {  // testing: Start at 120, L2 held 200..500 with the stick right
            // (200..330), back (330..430), the right stick turning (430..480), R2 at 300; the hero's / camera's heading logged
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 500) held |= 0x0001;
            if (frame >= 200 && frame < 330) lx = 1.0f;
            if (frame >= 330 && frame < 430) ly = 1.0f;
            if (frame >= 430 && frame < 480) rx = 1.0f;
            if (frame >= 300 && frame < 304) held |= 0x0002;
            if (frame % 10 == 0 || (frame >= 496 && frame < 530))
                std::printf("[aim] frame %llu state %d facing %.3f camera %.3f pos %.2f %.2f speed %.2f cam-hero dist %.2f dz %.2f\n",
                            static_cast<unsigned long long>(frame), lw(0x001413d4), lwc1(0x0013f350 + 0x98), lwc1(OV(0x00166dd8)),
                            lwc1(lwu(0x001413d0) + 0x10), lwc1(lwu(0x001413d0) + 0x14), lwc1(0x0013f350 + 0x190),
                            std::hypot(lwc1(OV(0x00166dc0)) - lwc1(0x0013f3d0), lwc1(OV(0x00166dc4)) - lwc1(0x0013f3d4)),
                            lwc1(OV(0x00166dc8)) - lwc1(0x0013f3d8));
        }
        if (std::strcmp(sim, "strafecam") == 0 || std::strcmp(sim, "strafecam0") == 0) {  // testing (RAC1_ADVANCED=1, Movement
            // Strafing or RAC 3): Start at 120, L2 held 200..250 with the stick right, then the right stick right 255..340 (the
            // camera turned just after the strafe; not for "strafecam0": the view's release alone); the camera's heading logged 240..380
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 250) { held |= 0x0001; lx = 1.0f; }
            if (sim[9] == 0 && frame >= 255 && frame < 340) rx = 1.0f;
            if (frame >= 240 && frame < 380)
                std::printf("[strafecam] frame %llu state %d facing %.3f camera %.3f strafing %d\n", static_cast<unsigned long long>(frame), lw(0x001413d4),
                            lwc1(0x0013f350 + 0x98), lwc1(OV(0x00166dd8)), advanced_strafing() ? 1 : 0);
        }
        if (std::strcmp(sim, "qshold") == 0) {  // Start at 120 (the arrival scene skipped), the quick select held 200..360
            // with the stick up, the frame counters logged (Advanced > pause in the quick select: they stop)
            if (frame >= 120 && frame < 124) held |= 0x0800;
            if (frame >= 200 && frame < 360) { held |= 0x0010; if (frame >= 230) ly = -1.0f; }
            if (frame % 20 == 0) std::printf("[qshold] frame %llu 0x15f5cc %u 0x15eea4 %u\n", static_cast<unsigned long long>(frame), lwu(OV(0x0015f5cc)), lwu(0x0015eea4));
        }
        if (std::strcmp(sim, "shoot") == 0 && frame >= 100 && (frame % 40) < 4) held |= 0x0020;  // the item in hand used (circle)
        if (std::strcmp(sim, "shootwalk") == 0 && frame >= 100) {  // walking forward, the item used every 40 frames: the hero follows his bombs
            ly = -1.0f;
            if ((frame % 40) < 4) held |= 0x0020;
        }
        if (std::strcmp(sim, "hold") == 0 && frame >= 100) held |= 0x0020;  // the item in hand used and held (circle: the Suck Cannon's beam)
        if (std::strcmp(sim, "holdwalk") == 0 && frame >= 100) { held |= 0x0020; ly = (frame % 300) < 150 ? -1.0f : 0.0f; }  // held, walking
        if (std::strncmp(sim, "vendor:", 7) == 0) {  // testing: triangle every 90 frames until the vendor's screens are
            // up (game mode 5), then from 60 frames later one key every 30 frames (the keys of "menu:": u d l r x o t s e 1 2 .)
            static uint64_t vopen = 0;
            const char* keys = sim + 7;
            if (vopen == 0 && lw(OV(0x0015f5c4)) == 5) vopen = frame + 60;
            // (RAC1_VENDOR_OPEN=<buttons> the opening press, e.g. 0x20 circle: the item 0x20 in hand opens the screens
            // with no Vendor moby, 0x29a6b0 from the hero's item code)
            static const uint32_t open_buttons = std::getenv("RAC1_VENDOR_OPEN") != nullptr ? static_cast<uint32_t>(std::strtoul(std::getenv("RAC1_VENDOR_OPEN"), nullptr, 0)) : 0x0010u;
            if (vopen == 0 && frame >= 100 && (frame % 90) < 4) held |= open_buttons;
            const size_t n = std::strlen(keys);
            if (vopen != 0 && frame >= vopen && (frame - vopen) % 30 < 4 && (frame - vopen) / 30 < n) {
                switch (keys[(frame - vopen) / 30]) {
                    case 'u': held |= 0x1000; break;
                    case 'd': held |= 0x4000; break;
                    case 'l': held |= 0x8000; break;
                    case 'r': held |= 0x2000; break;
                    case 'x': held |= 0x0040; break;
                    case 'o': held |= 0x0020; break;
                    case 't': held |= 0x0010; break;
                    case 's': held |= 0x0800; break;
                    case 'e': held |= 0x0100; break;
                    case '1': held |= 0x0004; break;
                    case '2': held |= 0x0008; break;
                }
            }
        }
        if (std::strcmp(sim, "vendor") == 0 && frame >= 100) {  // testing: triangle every 90 frames (a Vendor opens), then
            // right on the d-pad and cross (a purchase) between the presses
            if ((frame % 90) < 4) held |= 0x0010;
            if ((frame % 90) >= 40 && (frame % 90) < 44) held |= 0x2000;
            if ((frame % 90) >= 60 && (frame % 90) < 64) held |= 0x0040;
        }
        if (std::strncmp(sim, "menu:", 5) == 0 || std::strncmp(sim, "map:", 4) == 0) {  // the pause menu opened (Start at 120;
            // map: Select), then one key every 30 frames from 60 frames after it is up (game mode 3, 0x15f5c4): u d l r up /
            // down / left / right, x cross, o circle, t triangle, s Start, e Select, 1 / 2 L1 / R1, . nothing
            static uint64_t open = 0;
            const bool map = sim[1] == 'a';
            const char* keys = sim + (map ? 4 : 5);
            // (RAC1_MENU_AT: at that frame instead, the arrival scene skipped by Start at 120)
            static const uint64_t at = std::getenv("RAC1_MENU_AT") != nullptr ? std::strtoull(std::getenv("RAC1_MENU_AT"), nullptr, 10) : 120;
            if (frame >= at && frame < at + 4) held |= map ? 0x0100 : 0x0800;
            if (at != 120 && frame >= 120 && frame < 124) held |= 0x0800;
            if (open == 0 && frame >= at + 4 && lw(OV(0x0015f5c4)) == 3) open = frame + 60;
            const size_t n = std::strlen(keys);
            if (open != 0 && frame >= open && (frame - open) % 30 < 4 && (frame - open) / 30 < n) {
                switch (keys[(frame - open) / 30]) {
                    case 'u': held |= 0x1000; break;
                    case 'd': held |= 0x4000; break;
                    case 'l': held |= 0x8000; break;
                    case 'r': held |= 0x2000; break;
                    case 'x': held |= 0x0040; break;
                    case 'o': held |= 0x0020; break;
                    case 't': held |= 0x0010; break;
                    case 's': held |= 0x0800; break;
                    case 'e': held |= 0x0100; break;
                    case '1': held |= 0x0004; break;
                    case '2': held |= 0x0008; break;
                    case 'h': sim_host_press(keys, (frame - open) / 30); break;  // (RAC1_SIM_HOST's next input)
                    default: break;
                }
            }
        }
        if (std::strncmp(sim, "title:", 6) == 0) {  // the boot's title: Start 60 frames into the title (game mode 0),
            // then one key every 30 frames from 60 frames after the main menu is up (mode 3), as menu:<keys>
            static uint64_t title_at = 0, open = 0;
            const char* keys = sim + 6;
            if (boot_in_card_check() && frame % 60 < 4) held |= 0x0800;  // (the memory card's message: any button)
            if (title_at == 0 && boot_in_title() && lw(OV(0x0015f5c4)) == 0) title_at = frame + (std::getenv("RAC1_TITLE_WAIT") != nullptr ? std::strtoull(std::getenv("RAC1_TITLE_WAIT"), nullptr, 10) : 60);  // (RAC1_TITLE_WAIT: other than 60 frames)
            if (title_at != 0 && frame >= title_at && frame < title_at + 4 && lw(OV(0x0015f5c4)) == 0) held |= 0x0800;
            if (open == 0 && title_at != 0 && lw(OV(0x0015f5c4)) == 3) open = frame + 60;
            const size_t n = std::strlen(keys);
            if (open != 0 && frame >= open && (frame - open) % 30 < 4 && (frame - open) / 30 < n) {
                switch (keys[(frame - open) / 30]) {
                    case 'u': held |= 0x1000; break;
                    case 'd': held |= 0x4000; break;
                    case 'l': held |= 0x8000; break;
                    case 'r': held |= 0x2000; break;
                    case 'x': held |= 0x0040; break;
                    case 'o': held |= 0x0020; break;
                    case 't': held |= 0x0010; break;
                    case 's': held |= 0x0800; break;
                    case 'h': sim_host_press(keys, (frame - open) / 30); break;  // (RAC1_SIM_HOST's next input)
                    default: break;
                }
            }
        }
        if (std::strncmp(sim, "at:", 3) == 0) {  // testing: at:<frame>:<keys>, one key every 30 frames from that
            // frame, as menu:<keys> (s Start, x cross, o circle, t triangle, u d l r, . nothing)
            char* rest = nullptr;
            const uint64_t from = std::strtoull(sim + 3, &rest, 10);
            const char* keys = rest != nullptr && *rest == ':' ? rest + 1 : "";
            const size_t n = std::strlen(keys);
            if (frame >= from && (frame - from) % 30 < 4 && (frame - from) / 30 < n) {
                switch (keys[(frame - from) / 30]) {
                    case 'u': held |= 0x1000; break;
                    case 'd': held |= 0x4000; break;
                    case 'l': held |= 0x8000; break;
                    case 'r': held |= 0x2000; break;
                    case 'x': held |= 0x0040; break;
                    case 'o': held |= 0x0020; break;
                    case 't': held |= 0x0010; break;
                    case 's': held |= 0x0800; break;
                    default: break;
                }
            }
        }
        if (std::strncmp(sim, "seq:", 4) == 0) sim_script(sim + 4, frame, held, lx, ly, rx, ry);  // (a timed script, sim_script.h)
        sim_autopilot(frame, held, lx, ly);  // (RAC1_AUTOPILOT, sim_autopilot.h)
        if (std::strcmp(sim, "hud") == 0 && frame >= 30 && (frame % 120) < 4) held |= 0x0200;  // the HUD shown (L3)
        if (std::strcmp(sim, "throw") == 0 && frame >= 30 && (frame % 120) < 30) held |= (frame % 120) < 4 ? 0x0088 : 0x0008;  // throw the wrench (R1 + square)
    }
    // the Controls page awaits a new input: the devices go to the rebinding, the game's pad is idle (until the
    // new input is let go: no press of it reaches the game)
    if (g_remap.active() && g_remap.frame(g_bindings, have_devices ? devices : g_sim_host)) {
        held = 0;
        lx = ly = rx = ry = 0.0f;
    }
    hi::to_packet(hi::PadOutput{held, lx, ly, rx, ry}, g_host_pad_packet);
}

// orig: (host) at start, with the modern renderer on: its assets for every level, the title and the flight are there (the
// file each of the setup's exporters writes last in a level's directory, src/setup/mrp_export.cpp), else one clear error
// naming the first one missing and the setup that makes them (assets/generated deleted, or part of it: without them the
// game draws its levels wrong or without their mobys). The same files tools/setup.py looks at (step_missing).
void check_mrp_assets(bool moby, bool world) {
    std::vector<std::string> dirs;
    for (int lv = 0; lv < 19; ++lv) dirs.push_back("level" + std::to_string(lv));
    dirs.push_back("title");
    dirs.push_back("flight");
    std::vector<std::filesystem::path> want;
    if (moby) {
        const char* r = std::getenv("RAC1_MRP_DIR");  // (mrp_moby_renderer.cpp's asset_root)
        const std::filesystem::path root = r != nullptr ? r : "assets/generated/mrp/moby";
        for (const std::string& d : dirs) want.push_back(root / d / "textures.mrpt");
        for (int k = 0; k < 4; ++k) want.push_back(root / "armor" / (std::to_string(k) + ".mrpt"));
    }
    if (world) {
        const char* r = std::getenv("RAC1_MRPW_DIR");  // (mrp_world_renderer.cpp's asset_root)
        const std::filesystem::path root = r != nullptr ? r : "assets/generated/mrp/world";
        for (const std::string& d : dirs) want.push_back(root / d / "world.mrpw");
    }
    size_t missing = 0;
    std::string first;
    std::error_code ec;
    for (const std::filesystem::path& p : want)
        if (!std::filesystem::is_regular_file(p, ec)) {
            if (missing++ == 0) first = p.string();
        }
    if (missing == 0) return;
#if defined(_WIN32)
    const char* setup = "setup.cmd";
#else
    const char* setup = "./setup.sh";
#endif
    std::fprintf(stderr,
                 "rac1-native: Error: the modern renderer's assets are missing (%zu of %zu files, e.g. %s).\n"
                 "             The levels will be drawn without them (wrong, or without their objects).\n"
                 "             Run %s to generate them again.\n",
                 missing, want.size(), first.c_str(), setup);
}

}  // namespace

#if defined(__WIIU__)
#define RAC1_WIIU_TRACE(s) std::fprintf(stderr, "[wiiu-trace] %s\n", s)  // (the Wii U's bring-up: where the start stops)
#else
#define RAC1_WIIU_TRACE(s) ((void)0)
#endif
int main(int argc, char** argv) {
    RAC1_WIIU_TRACE("main");
    // (Windows: the console's QuickEdit off - a click in the console window froze the game - and the timer at 1 ms;
    // put back at the exit: platform.h)
    rac1::platform::process_begin();
    std::atexit(rac1::platform::process_end);
    for (int i = 1; i < argc; ++i) {
        if (std::string(argv[i]) == "--setup" || std::string(argv[i]) == "-s") {
            return rac1::setup::run_setup(argc, argv);
        }
    }
    rac1::ps2fpu::enter_ee_rounding();  // the EE's float rounding (toward zero) for everything the ports run
    RAC1_WIIU_TRACE("rounding");
    std::setvbuf(stdout, nullptr, _IOLBF, 0);
    int level = std::getenv("RAC1_LEVEL") != nullptr ? std::atoi(std::getenv("RAC1_LEVEL")) : 0;  // testing: the level to start on
    bool level_asked = std::getenv("RAC1_LEVEL") != nullptr || std::getenv("RAC1_GOTO") != nullptr || std::getenv("RAC1_ARRIVE") != nullptr;
    for (int i = 1; i < argc; ++i) {
        const std::string a = argv[i];
        if (!g_test_switches) {  // (a level chosen at start is a debug tool)
            if (a.rfind("--level", 0) == 0) std::fprintf(stderr, "[orig] %s ignored: it needs --debug-tools\n", a.c_str());
            continue;
        }
        if (a.rfind("--level=", 0) == 0) level = std::atoi(a.c_str() + 8);
        else if (a == "--level" && i + 1 < argc) level = std::atoi(argv[++i]);
        else continue;
        level_asked = true;
    }
    if (level < 0 || level >= 19) {
        std::fprintf(stderr, "rac1-native: level %d has no host assets yet\n", level);
        return 1;
    }
    std::string disc_why;
    const bool have_disc = disc_ready(disc_why);
    RAC1_WIIU_TRACE(have_disc ? "disc found" : disc_why.c_str());
    if (!have_disc || !std::filesystem::exists(disc_boot_elf_path()) || !disc_has_level(level)) {
        std::fprintf(stderr, "\nrac1-native: %s\n"
                             "Run the setup again from the repository's root, with your disc image:\n"
                             "  ./setup.sh --iso <path-to-your-iso>   (setup.cmd on Windows)\n"
                             "and start the game from the repository's root.\n\n",
                     have_disc ? "the game's data is missing (assets/)." : disc_why.c_str());
        return 1;
    }
    // RAC1_NO_GRAPHICS=1: the game's update alone, no window, no GL / Vulkan context and nothing prepared or drawn, the
    // renderers' switches at the faithful path (the game's state is then that of RAC1_MRP=0 RAC1_WORLD_FAITHFUL=1); a
    // headless run. For machines with no renderer yet (the Wii U port, ports/wiiu) and to check such a build on the PC
    // (RAC1_RAMHASH against that run). The GS's stores of a drawn picture (the pause menu's, a Vendor's) and the movies'
    // reads of the GS memory have no picture to read: their frames differ there.
    const bool no_graphics = std::getenv("RAC1_NO_GRAPHICS") != nullptr && std::atoi(std::getenv("RAC1_NO_GRAPHICS")) != 0;
    // RAC1_NO_GRAPHICS=2: also the work only the GS / VU1 path reads skipped, as the MRP skips it when it draws (ADR 0010
    // step 3: the VU0 skinning, the sub-mesh colour fill, the texture slots; RAC1_MRP_SKIP / _PARTS apply): a renderer that
    // takes the mobys from the MRP scene state, as the Wii U's will (its game state is then that of a run with the MRP)
    const bool no_graphics_skip = no_graphics && std::atoi(std::getenv("RAC1_NO_GRAPHICS")) == 2;
    const bool headless = no_graphics || std::getenv("RAC1_HEADLESS") != nullptr;
    // the game's own start (the boot module): by default in a window when no level is
    // asked for; RAC1_BOOT=0 / 1 decides
    bool boot = !headless && !level_asked;
#ifdef RAC1_DEVTOOLS
    if (boot && g_debug_tools) boot = rac1::devtools::dev_boot_wanted();  // (the debug menu's choice)
#endif
    if (const char* b = std::getenv("RAC1_BOOT")) boot = std::atoi(b) != 0;
    if (boot) level = 0;  // (New Game's level; the boot module runs on its own image, see front_end.h)
    if (std::getenv("RAC1_NO_MOVIES") != nullptr) movie_hw_enable(false);
    // testing: RAC1_CLOCK=<unix seconds>: the console's clock (sceCdReadClock) fixed, so that runs through a
    // level change (RAC1_GOTO, the level exit) are deterministic (it is read from the host's wall clock otherwise)
    // (a headless run uses a fixed clock (1700000000) unless RAC1_CLOCK is given)
    if (headless) g_host_clock_fixed = 1700000000;
    // a headless run ignores the player's Advanced settings (~/.config/rac1-native/advanced_options.txt, save<N>.pc): tests are
    // deterministic, the enhancements off; RAC1_ADVANCED=1 keeps them (testing an enhancement)
    if (headless && std::getenv("RAC1_ADVANCED") == nullptr) rac1::g_advanced_disabled = true;
    std::printf("[orig] rac1-native %s\n", rac1::update_check::version_text().c_str());
    // the update check (Updates > Update Check): started when the main menu opens on the title screen (or on demand
    // via Check Now), never in a headless run; what it finds is shown as the update notice
    rac1::update_check::start(headless);
    rac1::progress::start(headless);  // (the launcher's progress.json: host/progress.h)
    if (const char* ck = std::getenv("RAC1_CLOCK")) g_host_clock_fixed = static_cast<std::time_t>(std::strtoll(ck, nullptr, 0));
    const uint64_t max_frames = std::getenv("RAC1_FRAMES") ? std::strtoull(std::getenv("RAC1_FRAMES"), nullptr, 10) : (headless ? 300 : 0);

    // EE memory with the base ELF image; the level overlay's image (what the base ELF's loader
    // reads from the disc).
    RAC1_WIIU_TRACE("init_memory");
    init_memory();
    RAC1_WIIU_TRACE("memory");
    g_ee_null_is_ram = true;
    g_view_from_game = true;
    g_legacy_host_writes = false;

    if (!no_graphics && !glfwInit()) { std::fprintf(stderr, "rac1-native: GLFW init failed\n"); return 1; }
    if (no_graphics) {  // (the frame profiler with no GPU to time: its timestamps' pool none, its stages the CPU's alone)
        static const rac1::prof::GpuApi k_no_gpu = {
            "none", [](int, int) { return false; }, [](int) {}, [](int, int) {}, [](int, int, uint64_t*) { return false; },
            nullptr, [](rac1::prof::Resources*) {}, [](std::vector<rac1::prof::GlOwnerRow>*) {}};
        rac1::prof::set_gpu_api(&k_no_gpu);
    }
    glfwWindowHint(GLFW_CONTEXT_VERSION_MAJOR, 4);
    glfwWindowHint(GLFW_CONTEXT_VERSION_MINOR, 6);
    glfwWindowHint(GLFW_OPENGL_PROFILE, GLFW_OPENGL_CORE_PROFILE);
    if (headless) glfwWindowHint(GLFW_VISIBLE, GLFW_FALSE);
    // Options > Graphics: the window, the view, the level of detail as the player left them (not read in a
    // headless run: g_advanced_disabled); the testing switches below override them
    rac1::graphics_load();
    if (!headless) {  // Options > Graphics's Monitor row: the host's monitors, for the row and for the window to go on
        std::vector<std::string> names;
        int n = 0;
        if (GLFWmonitor** all = glfwGetMonitors(&n); all != nullptr)
            for (int i = 0; i < n; ++i) names.emplace_back(glfwGetMonitorName(all[i]));
        rac1::graphics_set_monitors(std::move(names));
    }
    int win_w0 = 960, win_h0 = 720;  // testing: RAC1_WINDOW=WxH, RAC1_VIEW_MODE=0|1|2 (render_toggles.h)
    if (!headless) {
        win_w0 = rac1::graphics_display().window_w;
        win_h0 = rac1::graphics_display().window_h;
    }
    if (const char* ws = std::getenv("RAC1_WINDOW")) std::sscanf(ws, "%dx%d", &win_w0, &win_h0);
    if (const char* rs = std::getenv("RAC1_RENDER_SCALE")) rac1::graphics_set_render_scale_percent(std::atoi(rs));  // testing: the 3D's render scale, percent
    if (const char* vm = std::getenv("RAC1_VIEW_MODE")) g_view_mode = std::atoi(vm);
    // testing: RAC1_HUD=4:3|16:9|21:9|32:9|fit, the HUD's width (Options > Graphics > HUD)
    if (const char* hv = std::getenv("RAC1_HUD")) {
        const std::string h = hv;
        g_hud_aspect = h == "16:9" ? 16.0f / 9.0f : h == "21:9" ? 64.0f / 27.0f : h == "32:9" ? 32.0f / 9.0f : h == "fit" ? 1.0e6f : 0.0f;
    }
    // testing: RAC1_FULLSCREEN=1 a fullscreen window on the primary monitor at its own mode (as the debug menu's
    // Fullscreen does); RAC1_VSYNC=0|1 and RAC1_FPS_CAP=n (0 unlimited) override the debug menu's display settings
    GLFWmonitor* fs_monitor = nullptr;
    const char* fs_env = std::getenv("RAC1_FULLSCREEN");
    if (!headless && (fs_env != nullptr ? std::atoi(fs_env) != 0 : rac1::graphics_display().fullscreen)) {
        fs_monitor = host_fullscreen_monitor();
        if (const GLFWvidmode* mode = fs_monitor != nullptr ? glfwGetVideoMode(fs_monitor) : nullptr) {
            win_w0 = mode->width;
            win_h0 = mode->height;
            glfwWindowHint(GLFW_REFRESH_RATE, mode->refreshRate);
            std::printf("[orig] RAC1_FULLSCREEN: %d x %d at %d Hz\n", mode->width, mode->height, mode->refreshRate);
        }
    }
    const int env_vsync = std::getenv("RAC1_VSYNC") != nullptr ? std::atoi(std::getenv("RAC1_VSYNC")) : -1;
    const int env_fps_cap = std::getenv("RAC1_FPS_CAP") != nullptr ? std::atoi(std::getenv("RAC1_FPS_CAP")) : -1;
    GLFWwindow* window = nullptr;
    // Options > Graphics > Renderer (RAC1_RENDERER=vulkan): the Vulkan renderer (docs/vulkan.md), read once, here; OpenGL when
    // Vulkan cannot start (no driver, no Vulkan 1.3, the build without its shaders)
    if (!no_graphics && rac1::graphics_display().vulkan) {
        glfwWindowHint(GLFW_CLIENT_API, GLFW_NO_API);
        window = glfwCreateWindow(win_w0, win_h0, "Ratchet & Clank - original runtime", fs_monitor, nullptr);
        const bool vsync0 = !headless && (env_vsync >= 0 ? env_vsync != 0 : rac1::graphics_display().vsync);
        if (window != nullptr && rac1::vk::init(window, vsync0, headless)) {
            if (!headless && fs_monitor == nullptr) host_window_on_monitor(window, rac1::graphics_display().monitor);
        } else {
            std::fprintf(stderr, "rac1-native: Vulkan could not start, OpenGL instead\n");
            if (window != nullptr) glfwDestroyWindow(window);
            window = nullptr;
            rac1::graphics_set_vulkan(false);
            glfwWindowHint(GLFW_CLIENT_API, GLFW_OPENGL_API);
        }
    }
    const bool gl_host = !no_graphics && window == nullptr;  // (the OpenGL renderer: the default)
    if (gl_host) {
        window = glfwCreateWindow(win_w0, win_h0, "Ratchet & Clank - original runtime", fs_monitor, nullptr);
        if (window == nullptr) { std::fprintf(stderr, "rac1-native: no OpenGL 4.6 window\n"); return 1; }
        glfwMakeContextCurrent(window);
        if (!headless && fs_monitor == nullptr) host_window_on_monitor(window, rac1::graphics_display().monitor);
    }
    if (!headless) host_input_init(window);  // the controls: the controller database, the bindings
    // the Controls page's host side; its changes saved in a window, headless only to an explicit RAC1_CONTROLS_FILE
    g_controls_save = !headless || std::getenv("RAC1_CONTROLS_FILE") != nullptr;
    if (headless && g_controls_save) g_bindings = hi::load_or_create(hi::controls_path());
    rac1::controls_set_host(&g_host_controls);
    if (gl_host) {
        if (headless) glfwSwapInterval(0);
        else host_apply_swap_interval(env_vsync >= 0 ? env_vsync != 0 : rac1::graphics_display().vsync, env_vsync >= 0 ? "start (RAC1_VSYNC)" : "start");
        if (!gladLoadGLLoader(reinterpret_cast<GLADloadproc>(glfwGetProcAddress))) { std::fprintf(stderr, "rac1-native: GL load failed\n"); return 1; }
        if (!ShaderManager::get().init()) { std::fprintf(stderr, "rac1-native: shaders failed\n"); return 1; }
    }
    sample_prof_start();  // (testing: RAC1_SAMPLE_PROF=<file>, see sample_prof.h)
    g_use_gpu_rendering = !no_graphics;
    particles_vu1_register();
    moby_vu0_register();  // the ported VU0 / VU1 programs, run when the game uploads their code
    moby_vu1_register();
    world_light_vu0_register();  // the world's static lighting VU0 set
    if (const char* wf = std::getenv("RAC1_WORLD_FAITHFUL"); wf != nullptr && std::atoi(wf) != 0) g_world_faithful = true;
    if (no_graphics) g_world_faithful = true;
    // the whole Modern Rendering Pipeline, the default: the MRP mobys, world and sky with every
    // mip ("full texture"). RAC1_MRP=0 the faithful path (the reference); RAC1_WORLD_MRP / RAC1_MOBY_MRP, when set, decide for their part
    // (e.g. RAC1_MOBY_MRP=0: the MRP world and sky with the faithful mobys); RAC1_WORLD_FAITHFUL=1 / RAC1_MOBY_VU1 / RAC1_MOBY_GPU, when
    // set, turn their part's MRP off unless its own switch says otherwise
    const char* mrp_env = std::getenv("RAC1_MRP");
    const bool mrp_all = !no_graphics && (mrp_env == nullptr || std::atoi(mrp_env) != 0);
    auto mrp_part = [mrp_all](const char* name, bool other_chosen) {
        const char* v = std::getenv(name);
        return v != nullptr ? std::atoi(v) != 0 : (mrp_all && !other_chosen);
    };
    g_mrp_world_mip = 1;  // "full texture" by default (RAC1_MRPW_MIP / RAC1_MRP_MIP=faithful: the GS's mip)
    g_mrp_moby_mip = 1;
    if (mrp_part("RAC1_WORLD_MRP", g_world_faithful)) {  // the MRP world and sky
        g_world_mrp = true;
        if (const char* p = std::getenv("RAC1_MRPW_PROJ")) g_mrp_world_projection = std::string(p) == "vu" ? 1 : 0;
        if (const char* p = std::getenv("RAC1_MRPW_MIP")) g_mrp_world_mip = std::string(p) == "faithful" ? 0 : 1;
        if (const char* p = std::getenv("RAC1_MRPW_FOG")) g_mrp_world_fog = std::atoi(p) != 0 ? 1 : 0;
    }
    // (Vulkan draws no legacy host renderer: without the MRP world, the world of the frame's VU1 chain through the GS replica)
    if (rac1::vk::is_active() && !g_world_mrp) g_world_faithful = true;
    rac1::graphics_mips_apply();  // Options > Graphics > Texture Detail, over the defaults above (the variables win)
    if (const char* l = std::getenv("RAC1_MRP_LOD")) g_mrp_lod = std::clamp(std::atoi(l), 0, 100);  // the LOD slider
    if (const char* l = std::getenv("RAC1_MRP_WIDEN")) g_mrp_lod_widen_off = std::atoi(l) == 0;  // (testing: the widened culling off)
    if (const char* l = std::getenv("RAC1_WIDE_FILL")) g_mrp_wide_fill = std::atoi(l) != 0;  // (fill the widescreen sides, default on)
    world_vu1_register();  // the world's tfrag / tie / shrub programs
    if (gl_host) moby_gpu_init();
    mrp_scene_install();  // the MRP scene state's observers
    mrp_shadows_install();  // the MRP's drop shadows: the shadow pass's observer
    if (const char* g = std::getenv("RAC1_MOBY_GPU")) {
        if (std::atoi(g) != 0) {
            g_moby_renderer = MobyRenderer::kGpu;
            g_moby_vu1_pass = true;
        } else {
            g_moby_renderer = MobyRenderer::kFaithful;
            g_moby_vu1_pass = true;
        }
    } else if (mrp_part("RAC1_MOBY_MRP", std::getenv("RAC1_MOBY_VU1") != nullptr)) {  // the Modern Rendering Pipeline
        g_moby_renderer = MobyRenderer::kMrp;
        g_moby_vu1_pass = true;  // (the pass makes the scene state: mrp_scene.h)
        if (const char* p = std::getenv("RAC1_MRP_PROJ")) g_mrp_moby_projection = std::string(p) == "vu" ? 1 : 0;
        if (const char* p = std::getenv("RAC1_MRP_MIP")) g_mrp_moby_mip = std::string(p) == "faithful" ? 0 : 1;
        if (const char* p = std::getenv("RAC1_MRP_FOG")) g_mrp_moby_fog = std::atoi(p) != 0 ? 1 : 0;
    } else if (const char* m = std::getenv("RAC1_MOBY_VU1")) {
        if (std::atoi(m) == 0) {
            g_moby_renderer = MobyRenderer::kGlb;
            g_moby_vu1_pass = false;
        } else {
            g_moby_renderer = MobyRenderer::kFaithful;
            g_moby_vu1_pass = true;
        }
    }

    check_mrp_assets(g_moby_renderer == MobyRenderer::kMrp, g_world_mrp);
    // (testing) RAC1_MRP_ASSET_CHECK=<level>: that level's MRP asset files read by the format readers (no GPU: also with
    // RAC1_NO_GRAPHICS=1, and on the Wii U), one hashed line per file: the same on either byte order (mrp_asset_check.h)
    if (const char* lv = std::getenv("RAC1_MRP_ASSET_CHECK")) {
        const char* mr = std::getenv("RAC1_MRP_DIR");  // (the renderers' asset_root)
        const char* wr = std::getenv("RAC1_MRPW_DIR");
        mrp::asset_check(std::atoi(lv), mr != nullptr ? mr : "assets/generated/mrp/moby", wr != nullptr ? wr : "assets/generated/mrp/world");
    }
    // the legacy host renderers' copy of a level, built only while one of them is in use (level_loader.h)
    g_legacy_level_needed = [] { return (!g_world_mrp && !g_world_faithful) || !g_moby_vu1_pass || g_moby_renderer == MobyRenderer::kGlb || g_title_scene_host; };
    static const bool s_moby_diff = std::getenv("RAC1_MOBY_DIFF") != nullptr;
    // the MRP's skipping of the GS / VU1 path's work: RAC1_MRP_SKIP=1 (default with the MRP) / 0 (off) / verify
    // (on, but the skinning also runs, to compare what it returns with the skip's next record); off with the MRP's debug checks
    {
        const char* ms = std::getenv("RAC1_MRP_SKIP");
        s_mrp_skip_mode = ms == nullptr ? 1 : std::string(ms) == "verify" ? 2 : std::atoi(ms) != 0 ? 1 : 0;
        if (const char* pp = std::getenv("RAC1_MRP_SKIP_PARTS")) g_moby_mrp_skip_parts = static_cast<unsigned>(std::strtoul(pp, nullptr, 16));
        if (std::getenv("RAC1_MRP_DIFF") || std::getenv("RAC1_MRP_CHECK") || std::getenv("RAC1_MRP_SCENE_CHECK") || s_moby_diff) s_mrp_skip_mode = 0;
    }
    vu1::g_start_hook = [](vu1::Vu1& vu, uint32_t start, uint64_t hash, vu1::ProgramFn fn, const std::function<void(uint32_t)>& kick) -> bool {
        // the debug menu's layers on the VU emulation path (the MRP has its own): a hidden layer's program runs, its VU
        // state left as the game's, but kicks nothing to the GS replica
        if (!vu1_layer_shown(hash)) {
            fn(vu.st, start, [](uint32_t) {});
            return true;
        }
        // (testing: RAC1_WORLD_MRP_CHECK / _DIFF run the faithful world as well: its programs' starts and kicks reported,
        // their GS primitives marked with the GS's moby flag, the mobys being the MRP's)
        if (hash != 0x1585563a86921802ull && mrp_world_testing() && mrp_world_program_start(hash, start)) {
            rac1::gs::Gs& gs = rac1::gs::gs_global();
            gs.set_moby_kick(true);
            fn(vu.st, start, [&vu, &kick](uint32_t addr) {
                const uint32_t qwc = vu1::gif_packet_qwc(vu.st, addr);
                std::vector<uint8_t> buf(static_cast<size_t>(qwc) * 16);
                for (uint32_t q = 0; q < qwc; ++q) std::memcpy(&buf[q * 16], &vu.st.mem[((addr + q) & 0x3ff) << 4], 16);
                mrp_world_program_kick(buf.data(), qwc);
                kick(addr);
            });
            gs.set_moby_kick(false);
            return true;
        }
        if (mrp_fx_vu1_check(&vu, start, hash, fn, kick)) return true;  // (testing: RAC1_MRP_FX_KICKCHECK)
        if (hash != 0x1585563a86921802ull) return false;
        if (mrp_scene_check_enabled()) mrp_scene_check_start(vu.st, start);  // (RAC1_MRP_SCENE_CHECK: the MRP scene state against what VU1 got)
        // the MRP draws the frame's mobys from the scene state: the VU1 moby programs are not run
        // (with RAC1_MRP_DIFF they are, for the faithful image the MRP's is compared with)
        static const bool s_mrp_diff = std::getenv("RAC1_MRP_DIFF") != nullptr;
        rac1::gs::Gs& gs = rac1::gs::gs_global();
        if (mrp_moby_active() && !s_mrp_diff && (start == 0x008 || start == 0x00a || start == 0x00c || start == 0x00e)) {
            if (!boot_active()) return true;
            // the boot's title: the programs run for the GS state their kicks leave (the boot module's logo after the mobys
            // draws on it), their primitives flagged as the mobys' (the replica does not draw them: the MRP does)
            gs.set_moby_kick(true);
            fn(vu.st, start, [&kick](uint32_t addr) { kick(addr); });
            gs.set_moby_kick(false);
            return true;
        }
        if (s_moby_diff) {
            if (start != 0x008 && start != 0x00a && start != 0x00c && start != 0x00e) return false;
            moby_gpu_begin_submesh(vu.st, start);
            gs.set_moby_kick(true);
            fn(vu.st, start, [&vu, &kick, start](uint32_t addr) {
                moby_gpu_process_kick(vu.st, start, addr);
                kick(addr);
            });
            gs.set_moby_kick(false);
            moby_gpu_end_submesh();
            return true;
        }
        if (g_moby_renderer == MobyRenderer::kGpu) {
            if (start != 0x008 && start != 0x00a && start != 0x00c && start != 0x00e) return false;
            const auto t0 = std::chrono::steady_clock::now();
            moby_gpu_begin_submesh(vu.st, start);
            fn(vu.st, start, [&vu, start](uint32_t addr) {
                moby_gpu_process_kick(vu.st, start, addr);
            });
            moby_gpu_end_submesh();
            g_prof_vu_moby_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            return true;
        }
        if (start == 0x008 || start == 0x00a || start == 0x00c || start == 0x00e) {
            static const bool s_moby_bench = std::getenv("RAC1_MOBY_BENCH") != nullptr;
            if (s_moby_bench) {
                static auto s_last_report = std::chrono::steady_clock::now();
                static double s_vu1_ms = 0;
                static int s_vu1_submeshes = 0;
                auto t0 = std::chrono::steady_clock::now();
                gs.set_moby_kick(true);
                fn(vu.st, start, kick);
                gs.set_moby_kick(false);
                s_vu1_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                ++s_vu1_submeshes;
                if (std::chrono::duration<double>(std::chrono::steady_clock::now() - s_last_report).count() >= 1.0) {
                    std::printf("[moby_bench] Faithful CPU moby pass: %.2f ms / sec (%d submeshes)\n", s_vu1_ms, s_vu1_submeshes);
                    s_vu1_ms = 0;
                    s_vu1_submeshes = 0;
                    s_last_report = std::chrono::steady_clock::now();
                }
            } else {
                const auto t0 = std::chrono::steady_clock::now();
                if (s_mrp_diff && mrp_moby_active()) mrp_moby_diff_program(gs.primitives().size());  // (the MRP's diff: this start's GS triangles)
                gs.set_moby_kick(true);
                fn(vu.st, start, kick);
                gs.set_moby_kick(false);
                if (s_mrp_diff && mrp_moby_active()) mrp_moby_diff_program(gs.primitives().size());
                g_prof_vu_moby_ms += std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
            }
            return true;
        }
        return false;
    };
    shadows_vu1_register();

    // The host renderer's copy of the level's assets (above the game's 32 MB, see host_layout.h);
    // with the boot no level yet: the boot module runs on its own image (the image
    // the ELF loaded; its level, the title's, is its own data) until its level exit.
    if (!boot) level_host_load(level);

    // The base ELF's start (not ported): the level to play, NTSC; it reads the disc's table of
    // contents, the level's header and its files (0x204428) before it runs the overlay.
    sw(0x0015ed84, static_cast<uint32_t>(level));
    sw(0x0015ed80, 0);
    // the disc's region 0x15ee90 (1: PAL), which the overlay's main (0x23d598, not ported) sets from the
    // disc's SYSTEM.CNF (the Options' Video page, the save's settings size and folder): set on another region's disc
    // only. Its video mode stays 0: the PAL game's own 60 Hz mode (Daino's decision).
    if (disc_other_region()) sw(0x0015ee90, 1);
    // The language 0x15ed88: the overlay's main (0x23d598, not ported) sets it from the console's
    // language (0x12d240; 1 English → 0, 2 French → 2, 3 Spanish → 4, 4 German → 3, 5 Italian
    // → 5, others 0) before the level's load copies the texts of that language (0x241940). The
    // host stands in for the console's settings (host_language.h): the language the player last
    // chose in the game, else the system's; English in a headless run. Then the debug menu's saved
    // choice, then RAC1_LANGUAGE.
    sw(0x0015ed88, rac1::host_language::console_language(!headless));
#ifdef RAC1_DEVTOOLS
    if (!headless || std::getenv("RAC1_DEVMENU_SHOT") != nullptr) rac1::devtools::dev_language_boot();
#endif
    // (testing) RAC1_LANGUAGE=<0 English, 2 French, 3 German, 4 Spanish, 5 Italian>: the language, also headless
    if (const char* lang = std::getenv("RAC1_LANGUAGE")) sw(0x0015ed88, static_cast<uint32_t>(std::atoi(lang)));
    // The save's names: the overlay's main (0x23d598, not ported) reads SYSTEM.CNF (sector 0x121,
    // 0x12f208) and gives it to 0x24b410 (0x23d69c); the card library's start 0x24cf10
    // (0x23d818). The memory card (libmc's host stand-in): a virtual card backed by
    // the user's card directory (RAC1_MEMCARD_DIR, default ~/.local/share/rac1-native/memcard),
    // inserted in a window; headless runs have none unless RAC1_MEMCARD_DIR is set;
    // RAC1_MEMCARD=0 removes it.
    {
        const uint32_t cnf = ee_stack_alloc(0x800);
        if (disc_read_ee(cnf, 0x121, 1)) rac1::save_names_init(cnf);
        ee_stack_free(0x800);
        rac1::card_lib_init();
        const char* mc = std::getenv("RAC1_MEMCARD");
        const bool want = mc != nullptr ? std::atoi(mc) != 0 : (!headless || std::getenv("RAC1_MEMCARD_DIR") != nullptr);
        if (want) {
            rac1::mc_card_insert(rac1::mc_default_card_dir());
            std::printf("[orig] memory card: %s\n", rac1::mc_default_card_dir().c_str());
        }
    }
    // The IOP's 989snd (host) and the game's sound start (from its main, 0x23d598), the level's
    // sound bank loaded after its files (the loader's steps 3..7).
    // The VU0 programs the overlay's main (0x23d598, not ported) sends at its start (0x23d6dc): the
    // set at 0x190 the drop shadows (0x2860a8 / 0x2861f8) call. (The boot module's start does both.)
    if (!boot) vif0_send(0x0010e4c0);
    snd989_rand_use_game();  // (this thread runs the game's IOP; so will the game thread)
    snd989_iop_start();
    // (ADR 0013) the host's mirror of the IOP records from the first command when a device will play it
    const bool want_audio = (!headless || std::getenv("RAC1_AUDIO_DEVICE") != nullptr) && std::getenv("RAC1_NO_AUDIO") == nullptr;
    if (want_audio && (std::getenv("RAC1_IOP_MIRROR") == nullptr || std::atoi(std::getenv("RAC1_IOP_MIRROR")) != 0)) snd989_mirror_enable();
    if (const char* mt = std::getenv("RAC1_IOP_MIRROR_TEST"); mt != nullptr && headless && !want_audio) {  // (testing)
        g_mirror_test = true;
        snd989_mirror_enable();
        for (const char* q = mt; *q != 0;) {
            char* e = nullptr;
            const unsigned long long v = std::strtoull(q, &e, 10);
            if (*e == ':') g_mirror_stalls[v] = static_cast<int>(std::strtol(e + 1, &e, 10));
            if (*e != ',') break;
            q = e + 1;
        }
        if (const char* md = std::getenv("RAC1_MIRROR_DUMP")) g_mirror_dump = std::fopen(md, "wb");
    }
    stash_iop_start();
    sound_callbacks_register();
    if (!boot) sound_system_init();
    rac1::host_priority_set_enabled(!headless);  // (rtkit's priorities in a window; RAC1_PRIORITY=0 / 1 decides)
    if (want_audio && !audio_out_start(advanced_audio_channels())) snd989_mirror_disable();
    g_advanced_audio_reopen = audio_out_set_channels;  // (Options > Sound: 5.1 / 7.1 / stereo)
    if (!boot) {
        if (!boot_loader_run([&] {
                if (!toc_read() || !level_header_read(level) || !level_files_read()) return false;
                ee_write<uint16_t>(0x0015ee48, 3);
                while (level_bank_step() == 0) {
                }
                return true;
            })) {
            std::fprintf(stderr, "rac1-native: cannot read the level from the disc (%s)\n",
                         disc_source() == DiscSource::Image ? disc_path().c_str() : disc_files_dir());
            return 1;
        }
        std::printf("[orig] sound bank %08x\n", lwu(0x0015ed5c));
    }
    // The display size the boot module (0x1e9658, not ported) leaves at 0x1518d0 / 0x1518d2:
    // 512 x 416 (as the frame's own GS packet has it: XYOFFSET 0x7000 / 0x7300, scissor 511 / 415).
    ee_write<uint16_t>(0x001518d0, 512);
    ee_write<uint16_t>(0x001518d2, 416);
    gs_memory_and_draw_envs();
    g_audio_dump = std::getenv("RAC1_AUDIO_DUMP") ? std::fopen(std::getenv("RAC1_AUDIO_DUMP"), "wb") : nullptr;
    g_audio_dump_ch = advanced_audio_channels();
    ee_thread_set_vsync_hook(blocking_loop_vsync);  // (the vsync waits inside the game's code: the start's, a blocking loop's)
    // (on the game thread: by the main thread once its drawing is over - the frame's picture is in its GL / Vulkan context)
    if (!no_graphics) rac1::gs::set_store_image_hook([](uint32_t bp, uint32_t, uint32_t psm) { rac1::game_thread::on_main([&] { g_scene_pic.store(bp, psm); }); });
    if (boot) {  // the boot module: its own image, the ports relocated onto it
        if (!front_image_load()) {
            std::fprintf(stderr, "rac1-native: cannot read the boot module from %s\n", disc_boot_elf_path().c_str());
            return 1;
        }
        ovl_reloc_set_level(OVL_FRONT);
        // the title's scene: its own passes' chain (MRP / faithful); RAC1_TITLE_HOST=1 the old host renderers
        if (const char* th = std::getenv("RAC1_TITLE_HOST")) g_title_scene_host = std::atoi(th) != 0;
        if (g_title_scene_host) g_boot_title_loaded_hook = title_scene_host_load;
        boot_start();
    } else {
        overlay_entry_start();
    }
    if (std::getenv("RAC1_MOBY_LIST") != nullptr)
        for (uint32_t m = lwu(OV(0x0015ffd8)); m < lwu(OV(0x0015ffdc)); m += 0x100) {
            const uint32_t fn = lwu(m + 0x74);
            if (fn != 0 && !rac1::ovl_moby_fn_ported(fn))
                std::printf("[moby] %08x class %d fn %08x pos %.1f %.1f %.1f\n", m, lh(m + 0xa6), fn, lwc1(m + 0x10), lwc1(m + 0x14), lwc1(m + 0x18));
        }
    if (std::getenv("RAC1_MOBY_LIST") != nullptr)  // the particle types whose update (the table 0x1b1e80) has no port
        for (uint32_t t = 0; t < 0x60; ++t) {
            const uint32_t fn = lwu(OV(0x001b1e80) + t * 4);
            if (fn != 0 && fn < 0x02000000u && !rac1::ovl_moby_fn_ported(fn)) std::printf("[particle] type %02x update %08x not ported\n", t, fn);
        }
    if (std::getenv("RAC1_MOBY_LIST") != nullptr)  // the level's live cameras: type, trigger kind
        for (uint32_t i = 0; i < 48; ++i) {
            const uint32_t c = OV(0x00167150) + i * 0xa0;
            if (lw(OV(0x00169890) + i * 4) != 0)
                std::printf("[camera] %u type %d (id %d) trigger %u prio %u pos %.1f %.1f %.1f\n", i, lh(c + 0x8c), lh(c + 0x86), lwu(c + 0x74), lbu(c + 0x7c), lwc1(c + 0x30), lwc1(c + 0x34), lwc1(c + 0x38));
        }
    std::printf("[orig] level %d started: mode %d, mobys %08x..%08x\n", level, lw(OV(0x0015f5c4)), lwu(OV(0x0015ffd8)), lwu(OV(0x0015ffdc)));
    if (std::getenv("RAC1_MUSIC_ON") != nullptr) {  // testing: the music's pause lifted at once (0x265860), as a menu
        music_volumes_default();                      // closing or the end of a landing (0x290960) does
        std::printf("[orig] RAC1_MUSIC_ON: the music unpaused (0x265860)\n");
    }
    if (std::getenv("RAC1_ARRIVE") != nullptr) {  // testing: this planet chosen on the ship's map (the pause menu's
        rac1::planet_travel(lw(0x0015ed84));     // planet map, 0x276bd0 -> 0x28ed58): the landing again
        const uint32_t ship = lwu(0x0013e030);
        std::printf("[orig] RAC1_ARRIVE: the landing (game mode %d, step %d, scene frame %d of %d), the ship %08x class %d at %.1f %.1f %.1f\n", lw(OV(0x0015f5c4)), lw(0x0013e050),
                    lw(OV(0x0016c894)), lh(OV(0x0016c8a0)), ship, ship != 0 ? lh(ship + 0xa6) : -1, ship != 0 ? lwc1(ship + 0x10) : 0.0f, ship != 0 ? lwc1(ship + 0x14) : 0.0f,
                    ship != 0 ? lwc1(ship + 0x18) : 0.0f);
    }

#ifdef RAC1_DEVTOOLS
    namespace dt = rac1::devtools;
    const bool menu_shot = std::getenv("RAC1_DEVMENU_SHOT") != nullptr;  // testing: the menu open in a headless screenshot
    if (!headless || menu_shot) dt::dev_menu_init(window, g_debug_tools || menu_shot);  // (its saved settings always; the menu with the debug tools)
    if (g_debug_tools) std::printf("[orig] debug tools on: F1 / R3 the debug menu, F2 / L3 moves Ratchet by hand\n");
    if (env_vsync >= 0 && !headless) {
        if (rac1::vk::is_active()) rac1::vk::set_vsync(env_vsync != 0);
        else glfwSwapInterval(host_swap_interval(env_vsync != 0));  // (testing: over Options > Graphics)
    }
    if (const char* pe = std::getenv("RAC1_PIPELINE")) dt::dev_settings().pipeline = std::atoi(pe) != 0;  // (testing)
    if (menu_shot) {
        dt::dev_settings().open = true;
        dt::dev_settings().show_stats = true;
    }
#endif
    FrozenFrame displayed;  // the last picture drawn, the GS display buffer a blocking loop's frames draw over
    FrozenFrame last_shown;  // the last frame of the game drawn (not a menu's, a fade's, a movie's): the frame buffer a store reads
    bool displayed_is_last_shown = false;
    MoviePicture movie_picture;
    auto last = std::chrono::steady_clock::now();
    auto next = last;
    double game_time = 0.0;  // game frames owed (60 a second at speed 1)
    uint64_t frame = 0;       // game frame passes run (a blocking loop's frame shown with "Original fades" is one)
    uint64_t game_frame = 0;  // game frames ended: what the testing scripts (RAC1_SIM_INPUT, RAC1_GOTO, RAC1_RAMHASH) count
    double update_ms = 0.0, update_shown = 0.0, update_shown_draw = 0.0;
    bool running = true;
    bool exiting = false;  // the level exit (0x292e08) running its loops: one pass a game frame
    // testing: RAC1_GOTO=N@F: at frame F the game's level change to level N (as the debug menu's
    // Levels tab does: 0x24d430, 0x28eeb0)
    // a chain of them: RAC1_GOTO=N1@F1,N2@F2,.. (the levels a player's session goes through)
    int goto_level = -1;
    uint64_t goto_frame = 0;
    std::vector<std::pair<int, uint64_t>> goto_chain;
    if (const char* g = std::getenv("RAC1_GOTO")) {
        for (const char* q = g; *q != 0;) {
            int l = -1;
            unsigned long long f = 60;
            if (std::sscanf(q, "%d@%llu", &l, &f) < 1) break;
            goto_chain.emplace_back(l, f);
            const char* c = std::strchr(q, ',');
            if (c == nullptr) break;
            q = c + 1;
        }
        if (!goto_chain.empty()) goto_level = goto_chain[0].first, goto_frame = goto_chain[0].second;
    }
    // testing: RAC1_DIE_AT=<frame>: at that game frame Ratchet is killed through the game's own death state
    // (the debug menu's Kill Ratchet)
    const long long die_at = std::getenv("RAC1_DIE_AT") != nullptr ? std::atoll(std::getenv("RAC1_DIE_AT")) : -1;
    // testing: RAC1_HOVERBOARD_AT=<on frame>[,<off frame>,<on frame>...]: the debug menu's Hoverboard anywhere ticked /
    // unticked at those frames, in turn (at most 8)
    std::vector<long long> board_at;
    if (const char* b = std::getenv("RAC1_HOVERBOARD_AT"))
        for (const char* q = b; *q != 0 && board_at.size() < 8;) {
            char* e = nullptr;
            const long long v = std::strtoll(q, &e, 10);
            if (e == q) break;
            board_at.push_back(v);
            q = *e == ',' ? e + 1 : e;
        }
    if (const char* z = std::getenv("RAC1_TEST_ZERO_FB"); z != nullptr && g_test_switches) {
        unsigned long long a = 0, b = 0;
        char raw[8] = {};
        if (std::sscanf(z, "%llu,%llu,%7s", &a, &b, raw) >= 2) {
            g_test_zero_fb_from = a;
            g_test_zero_fb_to = b;
            g_test_zero_fb_raw = std::strcmp(raw, "raw") == 0;
        }
    }
    FrameProf fprof;
    HostPacer pacer;
    HostPresents presents;
    presents.on = !headless || (std::getenv("RAC1_REALTIME") != nullptr && std::atoi(std::getenv("RAC1_REALTIME")) != 0);
    // (testing) RAC1_RAMHASH=<file>: after each game frame's update the 4 KB pages' hashes of the game's EE RAM
    // (32 MB) and the scratchpad are appended to the file; RAC1_RAMDUMP=<frame>,<file>: that frame's RAM + scratchpad raw
    FILE* ramhash = std::getenv("RAC1_RAMHASH") ? std::fopen(std::getenv("RAC1_RAMHASH"), "wb") : nullptr;
    uint32_t display_serial = rac1::graphics_display_serial();
    int windowed_pos[2] = {100, 100};
    int display_settle = 0;  // (frames after a change before the window's size is reported back: the window manager answers late)
    bool unlock_done = false;  // RAC1_UNLOCK_PLANETS done
    int hero_at_done = 0;  // RAC1_HERO_AT: the entries placed (several, '/'-separated, in order)
    // (testing, RAC1_REALTIME) the game's speed over the run, 60 = real time
    const auto run_t0 = std::chrono::steady_clock::now();
    uint64_t run_host_frames = 0;
    bool pace_reported = false;
    auto pace_report = [&] {
        if (std::getenv("RAC1_REALTIME") == nullptr || pace_reported) return;
        pace_reported = true;
        const double secs = std::chrono::duration<double>(std::chrono::steady_clock::now() - run_t0).count();
        std::printf("[pace] %llu game steps in %llu host frames, %.2f s: %.2f game steps a second, %.2f host frames a second\n",
                    static_cast<unsigned long long>(frame), static_cast<unsigned long long>(run_host_frames), secs, static_cast<double>(frame) / secs,
                    static_cast<double>(run_host_frames) / secs);
    };
    while (running && (window == nullptr || !glfwWindowShouldClose(window))) {
        ++run_host_frames;
        rac1::prof::frame_mark();  // (the debug menu's profiler: the frame before ends, this one starts)
        int prof_events = rac1::prof::begin(rac1::prof::kEvents);
        if (window != nullptr) glfwPollEvents();
        if (!headless) {  // Options > Graphics: the window's settings changed there applied now; the window as it is reported
            const bool is_fs = glfwGetWindowMonitor(window) != nullptr;
            if (rac1::graphics_display_serial() != display_serial) {
                display_serial = rac1::graphics_display_serial();
                display_settle = 60;
                const rac1::GraphicsDisplay& gd = rac1::graphics_display();
                if (gd.fullscreen && is_fs) {  // (the Monitor row changed while fullscreen: the window goes to that screen)
                    if (GLFWmonitor* m = host_fullscreen_monitor(); m != nullptr && m != glfwGetWindowMonitor(window)) {
                        if (const GLFWvidmode* mode = glfwGetVideoMode(m); mode != nullptr) glfwSetWindowMonitor(window, m, 0, 0, mode->width, mode->height, mode->refreshRate);
                    }
                } else if (gd.fullscreen && !is_fs) {
                    glfwGetWindowPos(window, &windowed_pos[0], &windowed_pos[1]);
                    GLFWmonitor* m = host_fullscreen_monitor();
                    if (const GLFWvidmode* mode = m != nullptr ? glfwGetVideoMode(m) : nullptr) glfwSetWindowMonitor(window, m, 0, 0, mode->width, mode->height, mode->refreshRate);
                } else if (!gd.fullscreen && is_fs) {
                    glfwSetWindowMonitor(window, nullptr, windowed_pos[0], windowed_pos[1], gd.window_w, gd.window_h, 0);
                } else if (!gd.fullscreen) {
                    int cw = 0, ch = 0;
                    glfwGetWindowSize(window, &cw, &ch);
                    if (cw != gd.window_w || ch != gd.window_h) glfwSetWindowSize(window, gd.window_w, gd.window_h);
                    host_window_on_monitor(window, gd.monitor);  // (the Monitor row: the window over on that screen)
                }
                if (rac1::vk::is_active()) rac1::vk::set_vsync(env_vsync >= 0 ? env_vsync != 0 : gd.vsync);
                else host_apply_swap_interval(env_vsync >= 0 ? env_vsync != 0 : gd.vsync, "display settings changed");
                presents.restart();  // (what the swaps do now: measured again)
            } else if (display_settle > 0) {
                --display_settle;
            } else {
                int cw = 0, ch = 0;
                glfwGetWindowSize(window, &cw, &ch);
                rac1::graphics_host_window(cw, ch, is_fs);
            }
        }
        rac1::graphics_frame();
        rac1::update_check::poll();  // (the update check's answer, when it has come: to the main menu's notice)
        if (!headless) {
            bool cursor_free = false;  // (the debug menu has the cursor while it is open)
#ifdef RAC1_DEVTOOLS
            cursor_free = g_debug_tools && dt::dev_settings().open;
#endif
            host_input_poll(window, cursor_free);
        }
        const auto now0 = std::chrono::steady_clock::now();
        const double dt_s = std::chrono::duration<double>(now0 - last).count();
        last = now0;
        // the game frames of this host frame: one in headless runs (deterministic tests); else
        // 60 a second (times the debug menu's speed), paused or single-stepped from the menu
        int steps = 1;
        static const int test_steps = std::getenv("RAC1_STEPS") != nullptr ? std::max(1, std::atoi(std::getenv("RAC1_STEPS"))) : 1;
        if (headless) steps = test_steps;  // (testing: a window's catch-up, several game frames in one host frame)
        static const int host_delay_ms = std::getenv("RAC1_HOST_DELAY_MS") != nullptr ? std::atoi(std::getenv("RAC1_HOST_DELAY_MS")) : 0;
        if (host_delay_ms > 0) std::this_thread::sleep_for(std::chrono::milliseconds(host_delay_ms));  // (testing: a slow host frame)
        bool iop_alone = true;  // (the IOP's mirror may play alone while the game's thread stalls; not while the host holds the game)
        bool host_paused = false;  // (the debug menu holds the game: its frames still drawn, the menu over them)
        // (testing) RAC1_REALTIME=1: a headless run steps the game as a window does (60 game frames a second of the wall
        // clock, the stalls not caught up), with RAC1_FPS_CAP its host frames' rate and RAC1_AUDIO_DEVICE=null the sound's queue
        static const bool realtime_test = headless && std::getenv("RAC1_REALTIME") != nullptr && std::atoi(std::getenv("RAC1_REALTIME")) != 0;
        if (!headless || realtime_test) {
            // the debug menu's pause, single step and speed (a build without the debug tools: none, speed 1). The game's
            // frames come from the wall clock in every build: without the debug tools the window ran one game frame per host
            // frame, the game's speed tied to the display's refresh rate
            bool paused = false, step_once = false;
            double speed = 1.0;
#ifdef RAC1_DEVTOOLS
            if (!headless) dt::dev_menu_poll(window);
            const dt::DevSettings& ds = dt::dev_settings();
            paused = ds.paused || (ds.open && ds.pause_when_open);
            step_once = ds.step_once;
            speed = static_cast<double>(ds.speed);
            if (paused) dt::dev_settings().step_once = false;
#endif
            iop_alone = !paused && speed == 1.0;
            host_paused = paused;
            if (paused) {
                game_time = 0.0;
                steps = step_once ? 1 : 0;
            } else {
                // a game frame that took the host long (a load: a level's start, a checkpoint's reload, a movie's open)
                // is not caught up: the game goes on from where it was, as the PS2 after a long frame, instead of running the
                // owed frames at once (up to 6 game frames in one host frame: a jump in the picture, a burst of sound)
                const double owed_s = g_last_steps_ms > kStallMs ? std::min(dt_s, 1.0 / 60.0) : std::min(dt_s, 0.1);
                game_time += owed_s * 60.0 * speed;
                // nor the time the IOP's mirror already played alone while the game was late (ADR 0013): the PS2's
                // IOP played on and the EE did not run those frames again; caught up, their sound would come on top of what
                // was played (a surplus the queue skips). Only the frames owed beyond this host frame's own one are taken off:
                // the game never stops for it (a game frame not run would leave the IOP alone again)
                static uint64_t alone_seen = 0;
                const uint64_t alone_now = audio_out_alone_total();
                game_time = std::max(game_time - static_cast<double>(alone_now - alone_seen), std::min(game_time, 1.0));
                alone_seen = alone_now;
                // (the nearest whole number of game frames, the rest carried (-0.5 .. 0.5): host frames of 1/60 s give one
                // game frame each even when their measured length jitters around it - the floor gave 0 then 2, a frame shown twice)
                steps = static_cast<int>(std::floor(game_time + 0.5));
                game_time -= steps;
                steps = std::min(steps, 8);
            }
        }
        audio_out_set_alone(iop_alone);
        g_last_steps_ms = 0.0;
        fprof.begin();
        fprof.steps(steps);
        sample_prof_frame(frame);
        rac1::prof::end(prof_events);
        rac1::prof::frame_steps(steps);
        // The host's frame (in two halves). prepare: what reads the game's state after its update - the view, the GS replica's
        // run of the frame's chain, the MRP world's scene capture (its build started on the worker thread), the MRP mobys' draw lists;
        // draw: the GL drawing and the present. Pipelined (RAC1_PIPELINE, the debug menu; on by default in a window): a prepared frame is
        // drawn after the next game frame's update, the scene built while that update runs (one frame of latency); the draw-time
        // functions then use the state captured at its prepare (render_toggles.h g_draw_from_snapshot). Else (and for the frames not
        // pipelined: a movie, the boot's title, the full-screen menu, the level exit, the testing modes) prepare and draw in a row.
        struct FrameDraw {
            int w = 0, h = 0, vx = 0, vy = 0, vw = 0, vh = 0;
            bool menu_mode = false, title = false, story = false, exiting = false;
            // the boot's title with its main menu (game mode 3, mrp_title_view.h): the menu's boxes (its mobys seen from the menu's
            // camera) drawn by the MRP as the full-screen menu's mobys are - the game's own 4:3 projection, cut at its 4:3 frame -, the
            // title scene's own mobys under them in the scene's widened view (mrp_moby_begin_frame)
            bool title_menu = false;
            // the level exit's loading screen (game mode 6 step 4: the ship's flight, 0x2900a0): its 3D drawn by the MRP
            // from the flight's own assets (assets/generated/mrp/{moby,world}/flight/), as a level's
            bool flight = false;
            // a frame of a blocking loop the game waits in (the screen fade, ADR 0012): the picture shown kept and its packets
            // drawn over it (they darken the display buffer); `held`: no packets sent that frame (the picture unchanged)
            bool fade = false, fade_held = false;
            // a frame of the game a blocking loop renders and shows (EeVsyncFrame::Scene: the Vendor's screens before they
            // store its picture): drawn as a main loop's frame, its picture kept (g_scene_pic); `vendor_pic`: the screens draw over
            // the picture they stored (the GS's type-7 primitive drawn by g_scene_pic, the MRP's mobys left to the GS path)
            bool scene = false, vendor_pic = false;
            bool prev_chain = false;  // the chain walked is the frame before's (a frame that built none of its own)
            const std::vector<uint8_t>* movie_pic = nullptr;
            int mw = 0, mh = 0;
            uint64_t frame = 0;
            int32_t level = 0;
            uint32_t chain_start = 0, chain_end = 0;
            bool has_dm = false;
            rac1::gs::DepthMap dm;
            rac1::gs::Viewport gv;
            float fb_w = 0, fb_h = 0;
        };
        static FrameDraw pend;
        static bool have_pend = false;
        static const bool pipe_testing = s_moby_diff || std::getenv("RAC1_MRP_DIFF") != nullptr || mrp_world_testing() || mrp_scene_check_enabled() ||
                                         mrp_shadows_check_enabled() || g_moby_renderer == MobyRenderer::kGpu;
        bool pipeline = !headless;
        if (const char* pe = std::getenv("RAC1_PIPELINE")) pipeline = std::atoi(pe) != 0;
#ifdef RAC1_DEVTOOLS
        if (!headless) pipeline = dt::dev_settings().pipeline;
#endif
        pipeline = pipeline && !pipe_testing;
        // The game thread (runtime/ee/game_thread.h; RAC1_GAME_THREAD, the debug menu; on by default with the pipelined frames,
        // a window's default): a host frame that draws a pipelined frame runs its game frames on the game thread while this thread draws
        // that frame (its drawing reads only what its prepare took: render_toggles.h g_draw_from_snapshot, runtime/kernel/ee_copy.h), then
        // joins it before the next prepare. The same game frames, in the same order, with the same float environment: the game's
        // state is the same word for word. Never in a deterministic headless run (no pipelined frame there), nor for a frame the
        // game runs in a blocking loop, a level's exit or load, the boot, the debug menu open (its tabs write the game's memory)
        bool overlap = false;
        {
            static const int gt_env = std::getenv("RAC1_GAME_THREAD") != nullptr ? std::atoi(std::getenv("RAC1_GAME_THREAD")) : -1;
            bool gt = gt_env != 0;
            bool menu_open = false;
#ifdef RAC1_DEVTOOLS
            if (!headless && gt_env < 0) gt = dt::dev_settings().game_thread;
            menu_open = !headless && (dt::dev_settings().open || dt::dev_menu_owns_input());
#endif
            int fb_w = 1, fb_h = 1;
            if (!headless) glfwGetFramebufferSize(window, &fb_w, &fb_h);
            // (testing: RAC1_GAME_THREAD=2 also in a deterministic headless run with RAC1_PIPELINE=1 - its RAM hashes against =0's)
            overlap = gt && pipeline && have_pend && steps > 0 && running && (!headless || realtime_test || gt_env >= 2) && !host_paused && !menu_open && !exiting &&
                      !boot_active() && !ee_thread_suspended() && fb_w > 0 && fb_h > 0 && !(headless && test_zero_fb(frame)) && pend.level == lw(0x0015ed84);
        }
        // (the pads' devices read here, on the main thread, for each game frame the game thread runs: GLFW's input is the main thread's)
        if (overlap && !headless) pad_presample(window, steps);
        auto game_frames = [&]() {
            rac1::snd989_rand_use_game();  // (the game's sound's random numbers: the game's sequence on whatever thread runs it)
            int prof_game = rac1::prof::begin(rac1::prof::kGameFrames);
            for (int k = 0; k < steps && running; ++k) {
                g_mrp_lod_frame_drawn = k == steps - 1;  // (only the last game frame of the host frame is drawn)
                pad_packet(headless ? nullptr : window, game_frame, overlap ? k : -1);
    #ifdef RAC1_DEVTOOLS
                if (!headless && dt::dev_menu_owns_input()) {
                    static const uint8_t idle[6] = {0xff, 0xff, 0x7f, 0x7f, 0x7f, 0x7f};
                    std::memcpy(g_host_pad_packet, idle, 6);
                }
    #endif
                const auto t0 = std::chrono::steady_clock::now();
                {
                    RAC1_PROF_ZONE(kAudioStep);
                    audio_out_step(true);  // (the IOP's mirror plays alone only while the game's thread is stuck inside a step: a stall)
                }
                render_view_projection_undo();  // (the game's own projection back before its update: its state is a 4:3 run's)
                {  // the MRP skips what only the GS / VU1 path needs: asked
                   // before the update, for a frame the MRP will draw (not the title, the level exit, a movie, the debug diffs)
                    int mw0 = 0, mh0 = 0;
                    // (not on the Vendor's screens, game mode 5 once the picture is stored: their frames over it are drawn by the GS path)
                    g_moby_mrp_skip = s_mrp_skip_mode != 0 && (mrp_moby_ready() || no_graphics_skip) && !boot_active() && !exiting && movie_hw_shown(mw0, mh0) == nullptr &&
                                      (lw(OV(0x0015f5c4)) != 5 || (lw(OV(0x001ca4c8)) == 0 && !ee_thread_suspended()));
                    g_moby_mrp_skip_verify = g_moby_mrp_skip && s_mrp_skip_mode == 2;
                    // the MRP's drop shadows: the shadow pass's data captured on a frame the MRP draws (whatever the skipping; the boot's title loop too)
                    mrp_shadows_begin_frame(mrp_moby_ready() && (!boot_active() || boot_in_title()) && !exiting && movie_hw_shown(mw0, mh0) == nullptr);
                }
    #ifdef RAC1_DEVTOOLS
                if (die_at >= 0 && static_cast<long long>(game_frame) == die_at && !exiting && !ee_thread_suspended()) {
                    dt::dev_kill_hero();
                    std::printf("[orig] RAC1_DIE_AT: Ratchet killed at frame %llu\n", static_cast<unsigned long long>(game_frame));
                }
                for (size_t k = 0; k < board_at.size(); ++k)
                    if (static_cast<long long>(game_frame) == board_at[k] && !exiting && !ee_thread_suspended()) {
                        dt::dev_hoverboard_set(k % 2 == 0);
                        std::printf("[orig] RAC1_HOVERBOARD_AT: %s at frame %llu\n", k % 2 == 0 ? "on" : "off", static_cast<unsigned long long>(game_frame));
                    }
    #endif
                mrp_scene_begin_frame();
                rac1::mrpw::scene_begin_update();  // (the MRP world: the world passes' chain ranges of this update)
                g_world_chain_forced = boot_active() && !g_title_scene_host;  // (the title's passes keep their chain)
                for (const auto& [gl, gf] : goto_chain)  // (the chain's next change)
                    if (game_frame == gf) goto_level = gl, goto_frame = gf;
                if (goto_level >= 0 && game_frame == goto_frame && !exiting && !ee_thread_suspended()) {
                    if (boot_active()) {  // the boot's title left for the level (the debug menu's skip)
                        boot_skip_to(goto_level);
                    } else {
                        level_change_request(0, goto_level);
                        level_exit_to(goto_level);
                    }
                    std::printf("[orig] RAC1_GOTO: the level change to %d asked for at frame %llu\n", goto_level, static_cast<unsigned long long>(frame));
                }
                // the game's frame on the EE thread's coroutine (ADR 0012): a vsync wait inside a blocking loop (the screen fade
                // 0x1f9838) suspends it, this host frame shows that frame and the next one resumes it (the lambda not called again)
                if (!ee_thread_suspended()) g_mode_at_update = boot_active() ? -100 : lw(OV(0x0015f5c4));
                rac1::host_language::note(lwu(0x0015ed88));  // (a language chosen in the game: kept as the console's)
                const int prof_code = rac1::prof::begin(rac1::prof::kGameCode);
                ee_thread_run([&] {
                    if (exiting) {  // the level exit's loops (the loading screen, the load), then the new level
                        static uint64_t flight_frames = 0, flight_first = 0;  // (the frames the loading screen's flight was drawn)
                        if (level_exit_loading_screen() && lw(OV(0x0015f5d8)) == 0 && flight_frames++ == 0) flight_first = frame;
                        if (level_exit_frame()) {
                            if (flight_frames != 0)
                                std::printf("[orig] the loading screen's flight: %llu frames (from frame %llu)\n", static_cast<unsigned long long>(flight_frames),
                                            static_cast<unsigned long long>(flight_first));
                            flight_frames = 0;
                            exiting = false;
                            running = level_enter(level_exit_files_read());
                        }
                    } else if (boot_active() ? !boot_frame() : !overlay_frame()) {  // the main loop (or the boot's title loop) left (0x15f570): the level exit
                        switch (level_exit_start()) {
                            case LevelExit::Load:
                                exiting = true;
                                std::printf("[orig] level exit to %d: the loading screen\n", lw(0x0015ed84));
                                level_prefetch(lw(0x0015ed84));
                                break;
                            case LevelExit::Movie:
                                exiting = true;
                                std::printf("[orig] level exit to %d: the story's cards and movies\n", lw(OV(0x0015f5c0)));
                                level_prefetch(lw(OV(0x0015f5c0)));
                                break;
                            case LevelExit::Quit: running = title_return(); break;
                        }
                    }
                });
                rac1::prof::end(prof_code);
                if (rac1::update_check::update_requested()) running = false;
    #ifdef RAC1_DEVTOOLS
                if (!headless) {
                    dt::dev_menu_game_frame();
                } else {
                    dt::dev_kill_apply();
                    dt::dev_hoverboard_apply();
                }
    #endif
                mrp_scene_end_frame();
                const bool game_frame_ended = !ee_thread_suspended();
                if (game_frame_ended && k + 1 < steps) moby_skips_note_undrawn();  // (a game frame not drawn: the next may walk its chain)
                if (game_frame_ended) {  // the main loop's own vsync (overlay_frame's wait 0x122298)
                    vsync_passed();
                    // (a game frame's RAM, also with "Original fades": the hashes of a run compare to another's game frame by game frame)
                    if (ramhash != nullptr) ram_hash_frame(ramhash, game_frame);
                    ++game_frame;
                }
                {
                    RAC1_PROF_ZONE(kAudioStep);
                    audio_out_step(false);
                }
                const double step_ms = std::chrono::duration<double, std::milli>(std::chrono::steady_clock::now() - t0).count();
                update_ms += step_ms;
                g_last_steps_ms = std::max(g_last_steps_ms, step_ms);
                {  // testing: RAC1_STALL_LOG=ms: the steps (a game frame, or a blocking loop's frame) longer than that, and the vsyncs they passed
                    static const double stall_ms = std::getenv("RAC1_STALL_LOG") != nullptr ? std::atof(std::getenv("RAC1_STALL_LOG")) : -1.0;
                    static uint64_t vs0 = 0;
                    if (stall_ms >= 0.0 && step_ms > stall_ms)
                        std::printf("[stall] frame %llu game frame %llu step %.1f ms, %llu vsyncs, mode %d\n", static_cast<unsigned long long>(frame),
                                    static_cast<unsigned long long>(game_frame), step_ms, static_cast<unsigned long long>(g_vsyncs - vs0), lw(OV(0x0015f5c4)));
                    vs0 = g_vsyncs;
                }
                ++frame;
                rac1::progress::frame(lw(0x0015ed84), boot_active(), exiting);  // (the launcher's progress.json)
                if (std::getenv("RAC1_CARD_LOG") != nullptr) {  // testing: the memory card's machines and screens as they change
                    uint32_t owned = 0;  // the items owned (0x13d4c0, 0x25 bytes) as a bit mask
                    for (uint32_t i = 0; i < 0x20; ++i) owned |= ee_read<uint8_t>(0x0013d4c0 + i) != 0 ? 1u << i : 0u;
                    static uint32_t last[7] = {~0u, ~0u, ~0u, ~0u, ~0u, ~0u, ~0u};
                    const uint32_t now[7] = {lwu(0x0015eeb0), lwu(0x0015eeb4), lwu(0x0013d290 + 0xd4), lwu(0x0013d290 + 0xe4), lwu(OV(0x0015f5c4)),
                                             lwu(OV(0x001b9cf0) + 4), owned};
                    if (std::memcmp(now, last, sizeof now) != 0) {
                        std::printf("[card] frame %llu ui %u flags %x machine %u error %x mode %u page %08x level %d slot %d items %08x bolts %d\n",
                                    static_cast<unsigned long long>(frame), now[0], now[1], now[2], now[3], now[4], now[5], lw(0x0015ed84), lw(0x0013d290 + 0x14), owned,
                                    lw(0x0015ed98));
                        std::memcpy(last, now, sizeof now);
                    }
                }
                if (frame % 60 == 0) {
                    const uint32_t hero = lwu(OV(0x0015ffd8));
                    std::printf("[orig] frame %llu mode %d update %.2f ms hero %.2f %.2f %.2f camera %.2f %.2f %.2f state %d bolts %d hp %d\n",
                                static_cast<unsigned long long>(frame), lw(OV(0x0015f5c4)), update_ms / 60.0, hero ? lwc1(hero + 0x10) : 0.0f,
                                hero ? lwc1(hero + 0x14) : 0.0f, hero ? lwc1(hero + 0x18) : 0.0f, lwc1(OV(0x00166dc0)), lwc1(OV(0x00166dc4)), lwc1(OV(0x00166dc8)),
                                lw(0x001413d4), lw(0x0015ed98), lw(0x001415f8));
                    update_shown = update_ms / 60.0;
                    update_ms = 0.0;
                }
                static const bool fade_log = std::getenv("RAC1_FADE_LOG") != nullptr;  // testing: the frames of the blocking loops
                static bool was_suspended = false;
                if (fade_log && (ee_thread_suspended() || was_suspended))
                    std::printf("[fade] frame %llu %s frame counter 0x15f3f8 %u mode %d\n", static_cast<unsigned long long>(frame - 1),
                                !ee_thread_suspended() ? "resumed: the caller went on, the game frame ended" : ee_thread_frame() == rac1::EeVsyncFrame::Held ? "suspended (held)" : "suspended (kicked)",
                                lwu(OV(0x0015f3f8)), lw(OV(0x0015f5c4)));
                was_suspended = ee_thread_suspended();
                if (ee_thread_suspended()) break;  // (a blocking loop's frame: shown before the game goes on, ADR 0012)
            }
            rac1::prof::end(prof_game);
        };
        if (!overlap) game_frames();
        if (no_graphics) {  // (nothing prepared nor drawn: the next game frame)
            fprof.end_frame();
            if (max_frames != 0 && frame >= max_frames) break;
            // (testing, RAC1_NO_GRAPHICS_WALK=1: the walk below on any host, to try the Wii U's path on the PC)
            static const bool walk_test = std::getenv("RAC1_NO_GRAPHICS_WALK") != nullptr && std::atoi(std::getenv("RAC1_NO_GRAPHICS_WALK")) != 0;
#if defined(__WIIU__)
            const bool walk = walk_test || rac1_wiiu_picture();
#else
            const bool walk = walk_test;
#endif
            if (walk) {  // (the Wii U's picture: the frame's chain on the GS replica, its 2D drawn by the port)
                // (chain_stage's choice of the chain below, without the MRP's parts: the frame's own, else the one a loop kicked
                // before it flipped the buffers, never one already sent, nothing on a held fade frame)
                rac1::g_world_pass_ranges_on = true;  // (the next updates' world passes noted: their ranges skipped below)
                rac1::gs::Gs& gs = rac1::gs::gs_global();
                gs.reset_frame();
                const bool fade = ee_thread_suspended() && ee_thread_frame() != rac1::EeVsyncFrame::Scene;
                const bool fade_held = fade && ee_thread_frame() == rac1::EeVsyncFrame::Held;
                const uint32_t idx = lwu(OV(0x001611d0));
                uint32_t start = lwu(OV(0x001611b8) + (idx & 1) * 4), end = lwu(OV(0x001611c0));
                bool prev_chain = false;
                if (end <= start) {
                    prev_chain = true;
                    start = lwu(OV(0x001611b8) + ((idx & 1) ^ 1) * 4);
                    end = lwu(OV(0x001611c4));
                }
                const bool already_kicked = prev_chain && start == g_loop_kicked.start && end == g_loop_kicked.end;
                if (!prev_chain && !fade) g_loop_kicked = KickedChain{};
                if (!fade_held && !already_kicked && start != 0 && end > start) {
                    // (as chain_stage: the mobys whose skinning was skipped have no vertex data in the chain, their parts' bytes
                    // are older ones the VIF would take for codes - this frame's ranges, or the ones noted when the chain is walked again)
                    std::vector<rac1::gs::Gs::SkipRange> skips;
                    if (!prev_chain && !fade && mrp_moby_ready() && !(lw(OV(0x0015f5c4)) == 3 && g_mode_at_update == 3)) {
                        // (the MRP's mobys: the drop shadows' tags walked but not processed, as chain_stage does - the MRP-lite
                        // draws no drop shadows yet, MRP_LITE.md phase 5)
                        rac1::gs::Gs::SkipRange r;
                        if (mrp_shadows_chain(r.first, r.last)) skips.push_back(std::move(r));
                    }
                    if (!prev_chain && g_moby_mrp_skip && (g_moby_mrp_skip_parts & 8) && !g_moby_mrp_skip_verify) {
                        moby_skips_note(start, end, &skips);
                    } else if (g_moby_skips_memo.start == start && g_moby_skips_memo.end == end && !g_moby_skips_memo.ranges.empty() &&
                               g_moby_skips_memo.hash == chain_bytes_hash(start, end)) {
                        skips.insert(skips.end(), g_moby_skips_memo.ranges.begin(), g_moby_skips_memo.ranges.end());
                    }
                    // (the world: the passes' ranges skipped, their 3D and their textures' uploads, as with the MRP's world)
                    if (!prev_chain && !fade)
                        for (const auto& [first, last] : rac1::g_world_pass_ranges) {
                            rac1::gs::Gs::SkipRange r;
                            r.first = first;
                            r.last = last - 16;
                            skips.push_back(std::move(r));
                        }
                    if (!skips.empty()) gs.set_chain_skips(std::move(skips));
                    gs.set_screen_area(0, 0, 0);
                    gs.vif1_chain(start, end);
                }
                gs_prims_dump(gs, game_frame);  // (testing)
                if (walk_test && game_frame % 300 < 2) {  // (testing: the heap in use, a leak shows)
                    const struct mallinfo mi = mallinfo();
                    std::printf("[heap] game frame %llu: %d KB in use, %d KB in mapped blocks\n", static_cast<unsigned long long>(game_frame), mi.uordblks / 1024, mi.hblkhd / 1024);
                }
                rac1::g_world_pass_ranges.clear();
#if defined(__WIIU__)
                {  // the MRP's mobys (ports/wiiu/game/mrp_moby_gx2.h): chain_stage's begin_frame, then the frame prepared for the picture
                    const bool title = boot_active();
                    const bool flight = exiting && level_exit_loading_screen();
                    const bool scene = ee_thread_suspended() && ee_thread_frame() == rac1::EeVsyncFrame::Scene;
                    int mw = 0, mh = 0;
                    const bool movie = movie_hw_shown(mw, mh) != nullptr;
                    const bool vendor_pic = !title && !scene && vendor_picture_frame();
                    const bool menu_mode = lw(OV(0x0015f5c4)) == 3 && g_mode_at_update == 3;
                    mrp_moby_begin_frame((title && !boot_in_title() && !flight) || (exiting && !flight) || movie || fade || vendor_pic, menu_mode,
                                         title && mrp_title_menu_drawn());
                    rac1::gs::Viewport mgv;
                    if (mrp_moby_active()) mrp_moby_prepare(&mgv);
                    // the MRP world's ties / shrubs (ports/wiiu/game/mrp_world_gx2.h): the host-made static path's frame, on a frame
                    // whose world is drawn (mrp_world_begin_frame_async's condition on the PC)
                    mrp_world_wiiu_prepare(!movie && (flight || ((title ? boot_in_title() : !menu_mode) && !exiting)) && !fade && !vendor_pic);
                }
                const int screen_fbp = static_cast<int>(lwu(0x0013cf20) & 0x1ff);
                rac1_wiiu_picture_frame(gs, static_cast<float>(lhu(0x001518d0)), static_cast<float>(lhu(0x001518d2)), screen_fbp, fade ? screen_fbp : -1, game_frame);
#endif
            }
#if defined(__WIIU__)
            if (!rac1_wiiu_frame()) break;  // (the HOME menu's closing)
#endif
            if (realtime_test && steps == 0) std::this_thread::sleep_for(std::chrono::milliseconds(1));  // (ahead of the clock: no busy loop)
            continue;
        }
        // a host frame that ran no game frame (it came sooner than 1/60 s: a jitter in the host's timing, a faster display)
        // shows nothing new: not prepared nor drawn again (the picture on the screen stays), the host waits for the next game
        // frame instead. A frame prepared twice without the game's update between went wrong: the projection's widening
        // (undone before each update, render_view_projection_undo) applied twice - the title's scene drawn in a wrong view,
        // its menu's frames gone, for that one frame (a flicker, on Windows above all)
        if (steps == 0 && !host_paused && (!headless || realtime_test)) {  // (RAC1_REALTIME: as a window does)
            const double wait_s = std::max(0.0, 0.5 - game_time) / 60.0;  // (the next game frame is due when game_time reaches 0.5)
            std::this_thread::sleep_for(std::chrono::duration<double>(std::max(wait_s, 0.001)));
            fprof.end_frame();
            continue;
        }

        LevelLoader::legacy_level_ensure();  // (a legacy renderer chosen since the level's load: its copy of the level now)
        // the game's threads (this one, the GS chain's and the MRP world's workers, the GL driver's) given a higher
        // priority by rtkit (nice -10, host_priority.h) once they exist (the workers start with the first frames), off this thread
        static int prio_stage = 0;
        if (prio_stage < 2 && frame >= (prio_stage == 0 ? 120u : 600u) && rac1::host_priority_enabled() && ++prio_stage != 0)
            std::thread([] { std::fprintf(stderr, "[priority] %d threads at nice -10\n", rac1::host_priority_raise_all(-10)); }).detach();
        if (std::getenv("RAC1_VIEW_HERO_FRONT")) {  // debug: a close look at the hero's face
            const uint32_t hero = lwu(OV(0x0015ffd8));
            const glm::vec3 hp(lwc1(hero + 0x10), lwc1(hero + 0x14), lwc1(hero + 0x18) + 0.75f);
            const glm::vec3 fwd(lwc1(hero + 0xc0), lwc1(hero + 0xc4), 0.0f);
            g_view_debug = true;
            g_view_debug_at = hp;
            const float ang = std::getenv("RAC1_VIEW_ANGLE") ? std::atof(std::getenv("RAC1_VIEW_ANGLE")) : 0.0f;
            const glm::vec3 f = glm::normalize(fwd);
            const glm::vec3 side(-f.y, f.x, 0.0f);
            const float dist = std::getenv("RAC1_VIEW_DIST") ? std::atof(std::getenv("RAC1_VIEW_DIST")) : 1.3f;
            g_view_debug_eye = hp + (f * std::cos(ang) + side * std::sin(ang)) * dist + glm::vec3(0.0f, 0.0f, 0.15f);
        }
        if (const char* va = std::getenv("RAC1_VIEW_AT")) {  // testing: a fixed host view "ex,ey,ez,ax,ay,az" (eye, target)
            float e[6] = {};
            if (std::sscanf(va, "%f,%f,%f,%f,%f,%f", &e[0], &e[1], &e[2], &e[3], &e[4], &e[5]) == 6) {
                g_view_debug = true;
                g_view_debug_eye = glm::vec3(e[0], e[1], e[2]);
                g_view_debug_at = glm::vec3(e[3], e[4], e[5]);
            }
        }
        int w, h;
        fprof.mark(0);
        auto prepare = [&](FrameDraw& fd, bool snapshot) {
            RAC1_PROF_ZONE(kPrepare);
            {  // a level change: the textures decoded for the old level forgotten (the chain's walk and the drawing idle here)
                static int32_t s_cache_level = INT32_MIN;
                const int32_t lv = lw(0x0015ed84);
                if (lv != s_cache_level) {
                    if (s_cache_level != INT32_MIN) {
                        rac1::gs::memory_join();
                        const size_t decoded = rac1::gs::local_memory().clear_caches();
                        const size_t two_d = mrp_2d_clear_caches(), atlas = mrp_fx_clear_caches(), replica = rac1::gs::draw_clear_caches();
                        long rss_kb = 0;
                        if (FILE* st = std::fopen("/proc/self/statm", "r")) {
                            long pages = 0, res = 0;
                            if (std::fscanf(st, "%ld %ld", &pages, &res) == 2) rss_kb = res * 4;
                            std::fclose(st);
                        }
                        std::printf("[caches] level %d -> %d: dropped %zu decoded textures, %zu 2D textures, %zu effects atlas layers, %zu GS replica textures (RSS %ld MB)\n",
                                    s_cache_level, lv, decoded, two_d, atlas, replica, rss_kb / 1024);
                    }
                    s_cache_level = lv;
                }
            }
            glfwGetFramebufferSize(window, &fd.w, &fd.h);
            if (test_zero_fb(frame)) fd.w = fd.h = 0;  // (testing: RAC1_TEST_ZERO_FB with raw)
            w = fd.w;
            h = fd.h;
            fd.vx = 0, fd.vy = 0, fd.vw = fd.w, fd.vh = fd.h;
            render_view_rect(fd.w, fd.h, fd.vx, fd.vy, fd.vw, fd.vh);
            render_view_projection(fd.w, fd.h);  // the debug menu's fov / widescreen on the game's projection
            // the full-screen menu (mode 3) draws no world: its background is the frame captured
            // when it opened (0x2a0f18, uploaded back each frame by 0x2a0fc8), here the host's last
            // world frame; its mobys (the menu's moby list) are drawn over it
            // and the frame's packets are the menu's: the update began in mode 3 (the frame Start opens the menu
            // in is the gameplay frame's chain: drawn as one, not as a menu frame - that was a frame of partial pictures)
            fd.menu_mode = lw(OV(0x0015f5c4)) == 3 && g_mode_at_update == 3;
            fd.movie_pic = movie_hw_shown(fd.mw, fd.mh);
            // the boot (its own image): the host's level data is not loaded (the title's scene draws
            // its mobys through the original pass; its ties, shrubs and sky: see title_scene_draw)
            fd.title = boot_active();
            fd.title_menu = fd.title && mrp_title_menu_drawn();
            fd.exiting = exiting;
            fd.story = exiting && level_exit_in_story();
            fd.flight = exiting && level_exit_loading_screen();
            fd.frame = frame;
            fd.level = lw(0x0015ed84);
            fd.scene = ee_thread_suspended() && ee_thread_frame() == rac1::EeVsyncFrame::Scene;
            fd.fade = ee_thread_suspended() && !fd.scene;
            fd.fade_held = fd.fade && ee_thread_frame() == rac1::EeVsyncFrame::Held;
            fd.vendor_pic = !fd.title && !fd.scene && vendor_picture_frame();
            const bool fade = fd.fade;
            const bool menu_mode = fd.menu_mode, title = fd.title;
            const std::vector<uint8_t>* movie_pic = fd.movie_pic;
            uint32_t chain_start_r = 0, chain_end_r = 0;
            // the game's own GS packets of the frame (its DMA chain in the packet buffer: the 2D, the texture uploads) on the GS subset,
            // drawn over the 3D
            auto chain_stage = [&]() {
                RAC1_PROF_ZONE(kChainCapture);
                rac1::gs::memory_join();  // (the previous chain's run ended)
                rac1::gs::Gs& gs = rac1::gs::gs_global();
                gs.reset_frame();
                moby_gpu_begin_frame();
                // the MRP: the level's assets, whether it draws this frame's mobys (not the boot's
                // title before its loop (its scene's mobys are the MRP's), the level exit's loops, a movie: those keep the faithful pass)
                mrp_moby_begin_frame((title && !boot_in_title() && !fd.flight) || (exiting && !fd.flight) || movie_pic != nullptr || fade || (fd.vendor_pic && !mrp_chain_wanted()), menu_mode, fd.title_menu);
                const uint32_t idx = lwu(OV(0x001611d0));
                uint32_t start = lwu(OV(0x001611b8) + (idx & 1) * 4), end = lwu(OV(0x001611c0));
                // a loop that kicks its frame's chain (0x2a08c8, not ported) before it flips the buffers
                // (0x2a0858: the chain's end kept at 0x1611c4), e.g. the exit's cards: the chain kicked
                bool prev_chain = false;  // (the chain of the frame before: not this frame's scene)
                if (end <= start) {
                    prev_chain = true;
                    start = lwu(OV(0x001611b8) + ((idx & 1) ^ 1) * 4);
                    end = lwu(OV(0x001611c4));
                }
                // the previous chain is the one a blocking loop already sent at its vsync (the screen fade's last frame):
                // not sent again (the frame sends nothing: the screen keeps its picture)
                const bool already_kicked = prev_chain && start == g_loop_kicked.start && end == g_loop_kicked.end;
                if (!prev_chain && !fade) g_loop_kicked = KickedChain{};  // (a frame of the game's own: the loop's chain is behind it)
                // (a held fade frame: the packets so far are the caller's frame, never sent - the fade resets the buffers)
                const bool chain_runs = !fd.fade_held && !already_kicked && start != 0 && end > start;
                fd.prev_chain = chain_runs && prev_chain;
                if (mrp_scene_check_enabled()) mrp_scene_check_begin_chain(chain_runs, prev_chain);
                static const bool chain_log = std::getenv("RAC1_CHAIN_LOG") != nullptr;  // testing: the chain each host frame walks
                if (chain_log) std::printf("[chainlog] frame %llu chain %08x..%08x prev %d runs %d fade %d idx %u rings %08x %08x c4 %08x\n", static_cast<unsigned long long>(frame), start, end, prev_chain, chain_runs, fade, idx, lwu(OV(0x001611b8)), lwu(OV(0x001611bc)), lwu(OV(0x001611c4)));
                if (chain_runs && std::getenv("RAC1_VU1_TRACE") != nullptr) std::fprintf(stderr, "[VU1] ==== frame %llu chain %08x..%08x\n", static_cast<unsigned long long>(frame), start, end);
                // the MRP's own reader of the chain (mrp_chain.h: the 2D, the effects, the uploads; no GS emulation) with the MRP drawing
                // the world, the mobys and the effects; else the GS replica (the faithful reference: RAC1_MRP=0, the debug menu's
                // "VU emulation"). (testing: RAC1_MRP_CHAIN=0 the replica for the MRP too; RAC1_MRP_CHAIN_CHECK=1 both, compared, the
                // replica's drawn)
                static const bool s_chain_check = std::getenv("RAC1_MRP_CHAIN_CHECK") != nullptr;
                const bool use_reader = mrp_chain_wanted();
                const bool use_replica = !use_reader || s_chain_check;
                MrpChainInput ci;
                ci.start = start;
                ci.end = end;
                std::vector<rac1::gs::Gs::SkipRange> skips;
                if (chain_runs && !prev_chain && mrp_moby_active() && !menu_mode && g_mrp_moby_projection == 0) {
                    // the MRP draws the drop shadows (in the world's projection): the shadows' tags (the GS path's alpha counter and
                    // darkening) are walked but not processed
                    rac1::gs::Gs::SkipRange r;
                    if (mrp_shadows_chain(r.first, r.last)) skips.push_back(std::move(r));
                }
                if (chain_runs && !prev_chain && mrp_moby_active() && g_moby_mrp_skip && (g_moby_mrp_skip_parts & 8) && g_moby_mrp_skip_verify == false) {
                    // the MRP draws the mobys: their chain tags (UNPACK vertex data, DIRECT fade state) and the texture uploads
                    // 0x2555c0 built for the GS path are walked but not processed; the VU1's double buffer flips as before
                    moby_skips_note(start, end, &skips);  // (noted for the next frame, which may walk this chain again)
                    if (std::getenv("RAC1_SKIP_DBG")) { std::fprintf(stderr, "[skipdbg] frame %llu chain %08x..%08x ranges:", static_cast<unsigned long long>(frame), start, end); for (auto& r : skips) { std::fprintf(stderr, " %08x-%08x(%zu:", r.first, r.last, r.mscals.size()); for (auto m : r.mscals) std::fprintf(stderr, "%x,", m); std::fprintf(stderr, ")"); } std::fprintf(stderr, "\n"); }
                } else if (chain_runs && g_moby_skips_memo.start == start && g_moby_skips_memo.end == end && !g_moby_skips_memo.ranges.empty() &&
                           g_moby_skips_memo.hash == chain_bytes_hash(start, end)) {
                    // the chain of the frame before walked (kept at 0x1611c4; a fade's frame; a frame not drawn): the mobys whose
                    // skinning the MRP skipped have no vertex data in it (their NEXT parts' inline quads are the buffer's older bytes,
                    // which the VIF would take for codes: STMOD 1 with a stale ROW broke the particles' UNPACK, an endless VU1 loop),
                    // so their tags are skipped again as at its first walk
                    skips.insert(skips.end(), g_moby_skips_memo.ranges.begin(), g_moby_skips_memo.ranges.end());
                }
                if (chain_runs && !prev_chain && (!menu_mode || title) && !fade)  // the MRP world: the world passes' chain is not processed (its VU1 starts with it)
                    for (const auto& [first, last] : mrp_world_skip_ranges()) {
                        rac1::gs::Gs::SkipRange r;
                        r.first = first;
                        r.last = last;
                        skips.push_back(std::move(r));
                    }
                for (const rac1::gs::Gs::SkipRange& r : skips)
                    ci.skips.push_back(MrpChainInput::Skip{r.first, r.last, static_cast<uint32_t>(r.mscals.size()), r.host_3d, r.moby, true});
                if (use_reader && chain_runs && !prev_chain && mrp_moby_active() && !g_moby_mrp_skip && fd.vendor_pic) {
                    // the Vendor's screens over their stored picture: the game builds the mobys' GS work (no skip), the MRP draws them; their
                    // ranges only mark where they go in the chain's order (after the picture) and the render targets' models
                    const MrpFrame& sf = mrp_scene();
                    for (const MrpMoby& m : sf.mobys)
                        if (m.chain_end > m.chain_begin) ci.skips.push_back(MrpChainInput::Skip{m.chain_begin, m.chain_end - 1, 0, m.rt ? m.moby : 0u, true, false});
                }
                if (use_replica && !skips.empty()) gs.set_chain_skips(std::move(skips));
                std::vector<int> substituted;  // (the shadow blocks' slots, in the order given to the reader / the replica)
                {  // the widened view's draw lists and 3D effects: the shadows' chains walked in place of the real blocks'
                    std::vector<MrpLodSubstitute> subs;
                    mrp_lod_substitutes(subs);
                    if (chain_runs && !prev_chain && !menu_mode && !fade)
                        for (MrpLodSubstitute& u : subs) {
                            if (ci.subs.size() >= 4) break;  // (Gs::kMaxSubs)
                            if (use_replica) gs.add_chain_substitute(u.first, u.end, use_reader ? u.bytes : std::move(u.bytes));
                            if (use_reader) ci.subs.push_back(MrpChainInput::Sub{u.first, u.end, std::move(u.bytes)});
                            else ci.subs.push_back(MrpChainInput::Sub{u.first, u.end, {}});
                            substituted.push_back(u.slot);
                        }
                    // (the VU1 programs' side tests over the widened view's sides: the shadow program's clipped triangles, mrp_lod.h)
                    float sw[2] = {1.0f, 1.0f};
                    if (chain_runs && !prev_chain && !menu_mode && !fade) mrp_lod_side_widen(sw);
                    if (use_replica) gs.set_side_widen(sw[0], sw[1]);
                    ci.side_widen[0] = sw[0];
                    ci.side_widen[1] = sw[1];
                }
                {  // the MRP's 3D effects: their producers' ranges, their primitives kept for the MRP
                    const bool fx_active = chain_runs && !prev_chain && !menu_mode && !fade && !fd.vendor_pic && movie_pic == nullptr && ((!title && !fd.exiting) || fd.flight);
                    if (mrp_fx_chain_ranges(fx_active, substituted, ci.fx, ci.vu7)) {
                        ci.vu7_hash = kMrpVu7Hash;
                        ci.vu7_kind = kMrpFxVu7;
                        if (use_replica) {
                            std::vector<rac1::gs::Gs::FxRange> fr;
                            std::vector<rac1::gs::Gs::Vu7Range> v7;
                            for (const auto& r : ci.fx) fr.push_back(rac1::gs::Gs::FxRange{r.first, r.last, r.sub, r.kind, r.id});
                            for (const auto& r : ci.vu7) v7.push_back(rac1::gs::Gs::Vu7Range{r.first, r.last, r.sub, r.tag});
                            gs.set_chain_fx(std::move(fr), true);
                            if (!v7.empty()) gs.set_chain_vu7(std::move(v7), kMrpVu7Hash, kMrpFxVu7);
                        }
                    }
                }
                // the full-screen 2D layers the last update drew (render_view_layer_note: the Visibomb's static), stretched across a
                // widened view; not on a chain walked again (a fade's: the frame before's) or the menu's
                if (chain_runs && !prev_chain && !menu_mode)
                    if (auto layers = render_view_layers(); !layers.empty()) {
                        ci.layers = layers;
                        if (use_replica) gs.set_chain_layers(std::move(layers));
                    }
                // the HUD's side elements the last update drew (render_view_hud_note), for a HUD wider than 4:3; the same frames
                if (chain_runs && !prev_chain && !menu_mode)
                    if (const auto notes = render_view_hud_notes(); !notes.empty()) {
                        std::vector<rac1::gs::Gs::HudRange> hr;
                        hr.reserve(notes.size());
                        for (const HudNote& n : notes) {
                            hr.push_back({n.first, n.last, n.side});
                            ci.hud.push_back({n.first, n.last, n.side});
                        }
                        if (use_replica) gs.set_chain_hud(std::move(hr));
                    }
                {  // the frame: an IMAGE transfer into it a type-7 primitive, on the Vendor's screens' frames
                    const uint32_t fr = lwu(0x0013cf20);
                    if (fd.vendor_pic) {
                        ci.scr_bp = (fr & 0x1ff) * 32;
                        ci.scr_bw = (fr >> 16) & 0x3f;
                        ci.scr_h = lhu(0x001518d2);
                    }
                    if (use_replica) gs.set_screen_area(ci.scr_bp, ci.scr_bw, ci.scr_h);
                }
                mrp_chain_set_used(use_reader && !s_chain_check);
                if (!chain_runs) {
                    if (use_reader) mrp_chain_clear();
                } else if (use_reader && !s_chain_check) {
                    // (the chain's tags and data captured here, read on the MRP chain worker: joined before the drawing, the next chain
                    // and the game's own GS transfers; RAC1_GS_THREAD=0 or the testing modes: here)
                    static const bool chain_thread = [] {
                        const char* e = std::getenv("RAC1_GS_THREAD");
                        return e == nullptr || std::atoi(e) != 0;
                    }();
                    mrp_chain_start(std::move(ci), chain_thread && !pipe_testing);
                } else {
                    // (the chain's tags and data captured here, its run on a worker thread: joined before the drawing, the
                    // next chain and the game's own GS transfers (libgraph); RAC1_GS_THREAD=0 or the testing modes: here)
                    static const bool chain_thread = [] {
                        const char* e = std::getenv("RAC1_GS_THREAD");
                        return e == nullptr || std::atoi(e) != 0;
                    }();
                    static rac1::gs::Gs::ChainCapture chain_cap;
                    if (s_chain_check) {
                        // (testing: the replica's walk and the reader's from the same local memory, their primitives compared)
                        const rac1::gs::LocalMemory before = rac1::gs::local_memory();
                        gs.vif1_chain(start, end);
                        const rac1::gs::LocalMemory after = rac1::gs::local_memory();
                        rac1::gs::local_memory() = before;
                        mrp_chain_start(std::move(ci), false);
                        mrp_chain_check(frame, gs.primitives(), gs.vu7_batches(), gs.vu7_vertices(), gs.fx_mark(), gs.moby_mark());
                        rac1::gs::local_memory() = after;
                    } else if (chain_thread && !pipe_testing && std::getenv("RAC1_GS_TRACE") == nullptr && std::getenv("RAC1_GS_ZTRACE") == nullptr) {
                        gs.capture_chain(start, end, chain_cap);
                        rac1::gs::chain_start_async(chain_cap);
                    } else {
                        gs.vif1_chain(start, end);
                    }
                }
                if (mrp_scene_check_enabled()) mrp_scene_check_end_chain();
    chain_start_r = start;
                chain_end_r = end;
            };
            {  // the MRP world: the frame's chain decoded on a frame whose world is drawn (else nothing: no skipping)
                RAC1_PROF_ZONE(kChainCapture);
                const uint32_t ci = lwu(OV(0x001611d0));
                const uint32_t cs = lwu(OV(0x001611b8) + (ci & 1) * 4), ce = lwu(OV(0x001611c0));
                // (the scene's inputs captured, its build started on the worker thread while this thread runs the GS replica's chain)
                // (the boot's title scene too, once its level is loaded (the title loop), its main menu (game mode 3) included: the boot module draws its scene under it)
                // (not on the Vendor's screens' frames that put their stored picture back: no world pass runs on them, and the passes
                // noted last are an older frame's - their ranges skipped the screens' own packets at those addresses, issue #47 / #48)
                mrp_world_begin_frame_async(movie_pic == nullptr && (fd.flight || ((title ? boot_in_title() : !menu_mode) && !exiting)) && !fade &&
                                                !fd.vendor_pic && ce > cs,
                                            cs, ce);
            }
            chain_stage();
            fd.chain_start = chain_start_r;
            fd.chain_end = chain_end_r;
            rac1::gs::Gs& gs = rac1::gs::gs_global();
            const uint32_t start = fd.chain_start, end = fd.chain_end;
            (void)start;
            (void)end;
            fd.has_dm = render_gs_depth(fd.dm.a, fd.dm.b);  // the VU-drawn 3D (particles) against the host's depth
            fd.gv = rac1::gs::Viewport{};
            fd.gv.x = fd.vx; fd.gv.y = fd.vy; fd.gv.w = fd.vw; fd.gv.h = fd.vh;
            fd.gv.xscale = 1.0f / g_view_widen;
            fd.fb_w = static_cast<float>(lhu(0x001518d0));
            fd.fb_h = static_cast<float>(lhu(0x001518d2));
            fd.gv.hud_shift = fd.menu_mode ? 0.0f : render_view_hud_shift(fd.fb_w);  // (a HUD wider than 4:3: its side elements moved)
            render_overscan_crop(fd.fb_w, fd.fb_h, fd.gv.crop);  // the full frame (testing: RAC1_OVERSCAN cuts the 2D to a TV's area)
            // the glyphs' sampling inside their own cells (no neighbouring glyphs' lines around the letters, issue #76): the modern
            // view's; the faithful reference (RAC1_MRP=0) samples as the GS. (testing: RAC1_SPRITE_CLAMP=0 / 1 decides)
            {
                static const char* s_sc = std::getenv("RAC1_SPRITE_CLAMP");
                fd.gv.sprite_clamp = s_sc != nullptr ? std::atoi(s_sc) != 0 : g_moby_renderer == MobyRenderer::kMrp;
                // the 2D slivers the game parks at the frame's edges (a HUD element hiding: its 3 leftmost columns for 16 frames,
                // which a TV's overscan hid) not drawn, now that the full frame is always shown: the modern view's; the faithful
                // reference draws all the GS drew. (testing: RAC1_EDGE_SLIVERS=1 draws them, 0 hides them)
                static const char* s_es = std::getenv("RAC1_EDGE_SLIVERS");
                fd.gv.hide_edge_slivers = s_es != nullptr ? std::atoi(s_es) == 0 : g_moby_renderer == MobyRenderer::kMrp;
            }
            // the frame's page: FRAME_1 of the draw environment the frame's fixed packet 0x13cf10
            // sets (render_passes.cpp), the screen even when sampled (the drop shadows' alpha)
            fd.gv.screen_fbp = static_cast<int>(lwu(0x0013cf20) & 0x1ff);
            if (fade) fd.gv.skip_fbp = fd.gv.screen_fbp;
            fd.gv.cut_3d = fd.menu_mode;  // (the menu's mobys cut at its 4:3 frame, as the GLB drawer's menu_cut)
            // the menu's render targets: the pages from the z buffer's (0x15ee88) up, never the screen in the menu
            if (fd.menu_mode) fd.gv.rt_min_fbp = static_cast<int>((lwu(0x0015ee88) & 0x003fffffu) >> 13);
            // the mobys the game draws into a render target (the menus' item models, 0x24ff08): the MRP draws them into it
            if (mrp_moby_active()) fd.gv.host_3d = [](const rac1::gs::Primitive& p, int tw, int th, bool) { mrp_moby_draw_target(p.fx_id, p.st.xyoffset, p.st.zbuf, tw, th); };
            if (fd.vendor_pic) fd.gv.screen_picture = [](const rac1::gs::TexImage& img, const rac1::gs::Viewport& vp) { g_scene_pic.draw(img, vp); };  // (the fade's black sprite over the render buffer, not shown)
            // the game's projected 3D over the window (its projection widened), but in the menu:
            // there the game draws with the menu's own 4:3 view (0x276368)
            // (with the MRP world the 3D is drawn wider than the game projected its own 3D during the update - the 4:3
            // projection the game sets back each update -: the GS's projected 3D narrowed by that widening to sit in it)
            fd.gv.xscale_3d = menu_mode ? fd.gv.xscale : 1.0f / mrp_lod_frame_widen();
            if (snapshot) {  // (a pipelined frame: what its drawing reads of the game's state, captured now; the mobys' lists built now)
                render_view_snapshot();
                draw_memory_take();  // (the game's memory its drawing reads: the lighting's, the particles')
                mrp_lod_snapshot();
                mrp_shadows_snapshot();
                RAC1_PROF_ZONE(kMobyLists);
                if (mrp_moby_active()) mrp_moby_prepare(&fd.gv);
            }
            // (the scene's inputs captured on the worker meanwhile: the game's state is written again only after this)
            mrp_world_wait_captured();
            static const uint64_t give_frame = std::getenv("RAC1_GIVE_FRAME") != nullptr ? std::strtoull(std::getenv("RAC1_GIVE_FRAME"), nullptr, 10) : 60;  // testing: after a RAC1_GOTO level change
            if (std::getenv("RAC1_TEXT_AUDIT") != nullptr && frame == give_frame) {  // (testing) the item messages 0x1b0340 the level's text table lacks
                for (int32_t id = 0; id < 0x4a; ++id) {
                    const int32_t m = lw(OV(0x001b0340) + static_cast<uint32_t>(id) * 4);
                    if (m != -1) std::fprintf(stderr, "[textaudit] level %u item 0x%x%s msg 0x%x %s\n", lwu(0x0015ed84), id % 0x25, id >= 0x25 ? " (again)" : "", static_cast<uint32_t>(m), sub_00203a68(m) >= 0 ? "ok" : "MISSING");
                }
                for (uint32_t id = 0; id < 0x25; ++id)
                    for (uint32_t o : {0x46u, 0x48u}) {
                        const int32_t m = static_cast<int16_t>(lhu(OV(0x00179ac0) + id * 0x4c + o));
                        if (m > 0) std::fprintf(stderr, "[textaudit] level %u item 0x%x name+0x%x 0x%x %s\n", lwu(0x0015ed84), id, o, static_cast<uint32_t>(m), sub_00203a68(m) >= 0 ? "ok" : "MISSING");
                    }
            }
            if (const char* gi = std::getenv("RAC1_GIVE_ITEMS"); gi != nullptr && frame == give_frame) {  // testing: items given (ids, comma separated)
                for (const char* q = gi; *q != 0;) {
                    char* e = nullptr;
                    const long id = std::strtol(q, &e, 0);
                    if (e == q) break;
                    devtools::item_give_quiet(static_cast<int32_t>(id), 0);  // (no placeholder message for another level's item)
                    q = *e == ',' ? e + 1 : e;
                }
            }
#ifdef RAC1_DEVTOOLS
            devtools::unlocks_testing(frame);  // testing: RAC1_UNLOCKS=<frame>:<op>,.. (the debug menu's Unlocks tab)
#endif
            if (const char* bo = std::getenv("RAC1_BOLTS"); bo != nullptr && frame == give_frame)  // testing: the bolt count 0x15ed98 set
                sw(0x0015ed98, static_cast<uint32_t>(std::strtol(bo, nullptr, 0)));
            if (const char* pk = std::getenv("RAC1_POKE8"); pk != nullptr) for (const char* q = pk; *q != 0;) { char* e = nullptr; const uint64_t pf = std::strtoull(q, &e, 0); if (*e != ':') break; const uint32_t pa = static_cast<uint32_t>(std::strtoul(e + 1, &e, 0)); if (*e != '=') break; const uint32_t pv = static_cast<uint32_t>(std::strtoul(e + 1, &e, 0)); if (frame == pf) sb(pa, pv); q = *e == ',' ? e + 1 : e; }  // testing: RAC1_POKE8=<frame>:<addr>=<byte>,.. (e.g. the gold bolts found 0x14bec0[level * 4 + k])
            if (const char* eq = std::getenv("RAC1_EQUIP"); eq != nullptr && frame == give_frame + 10)  // testing: an item asked in hand (as the quick select does)
                sw(0x0013f350 + 0x20b8, static_cast<uint32_t>(std::strtol(eq, nullptr, 0)));
            // testing: RAC1_EQUIP_CYCLE=<item>,<item>,...@<period>: from give_frame + 10, the next item asked in hand every
            // <period> frames, round and round
            if (const char* ec = std::getenv("RAC1_EQUIP_CYCLE"); ec != nullptr && frame >= give_frame + 10) {
                static std::vector<uint32_t> items;
                static uint64_t period = 0;
                if (period == 0) {
                    const char* at = std::strchr(ec, '@');
                    period = at != nullptr ? std::strtoull(at + 1, nullptr, 10) : 60;
                    for (const char* q = ec; *q != 0 && *q != '@';) {
                        char* e;
                        items.push_back(static_cast<uint32_t>(std::strtoul(q, &e, 0)));
                        if (e == q) break;
                        q = *e == ',' ? e + 1 : e;
                    }
                }
                const uint64_t k = frame - (give_frame + 10);
                if (!items.empty() && period != 0 && k % period == 0) {
                    const uint32_t it = items[(k / period) % items.size()];
                    sw(0x0013f350 + 0x20b8, it);
                    std::printf("[equip] frame %llu item %u\n", static_cast<unsigned long long>(frame), it);
                }
            }
            // testing: RAC1_HERO_AT=<class>@<frame>[,dx,dy,dz]: at that game frame, in game mode 0,
            // the hero put by the game's own placement (0x216f90, as a respawn does) next to the first
            // moby of that class (its position + the offset, default 3 0 1), facing it. Only a place:
            // no flag of the story is touched (e.g. 834 = Veldin's ship on its pad, 531 = Ratchet's ship).
            const char* ha = std::getenv("RAC1_HERO_AT");
            for (int k = 0; ha != nullptr && k < hero_at_done; ++k) ha = std::strchr(ha, '/') != nullptr ? std::strchr(ha, '/') + 1 : nullptr;
            if (ha != nullptr && !exiting && !ee_thread_suspended() && lw(OV(0x0015f5c4)) == 0) {
                int cls = 0;
                unsigned long long hf = 0;
                float off[3] = {3.0f, 0.0f, 1.0f};
                float yaw = 1e9f;  // (an optional 6th value, the hero's yaw itself)
                if (const int nf = std::sscanf(ha, "%d@%llu,%f,%f,%f,%f", &cls, &hf, &off[0], &off[1], &off[2], &yaw); nf >= 2 && frame >= hf) {
                    // (class -1 = the offset is the position itself)
                    // (class -2 = the level's ship *(0x13e030), spawned by the level's load past the placed mobys; without
                    // an offset the hero is put at its hatch)
                    for (uint32_t m = cls == -2 ? lwu(0x0013e030) : cls < 0 ? 0u : lwu(OV(0x0015ffd8)); cls < 0 || (m <= lwu(OV(0x0015ffe0)) && lbu(m + 0x20) != 0xff); m += 0x100) {  // (the spawned mobys too)
                        if (cls == -2) {
                            if (m == 0) break;
                            cls = lh(m + 0xa6);
                            if (nf == 2) {  // (no offset: the ship's hatch, the point its boarding prompt measures, 0x1bd880[variant] through its rotation)
                                Vec4 h;
                                mat3_transform(h, V(OV(0x001bd880) + static_cast<uint32_t>(lh(0x0013e056)) * 0x10), M(m + 0xc0));
                                off[0] = h.x;
                                off[1] = h.y;
                                off[2] = h.z;
                            }
                        }
                        else if (cls >= 0 && (lh(m + 0xa6) != cls || static_cast<int8_t>(lbu(m + 0x20)) < 0)) continue;
                        const uint32_t buf = ee_stack_alloc(0x20);
                        swc1(buf, (cls < 0 ? 0.0f : lwc1(m + 0x10)) + off[0]);
                        swc1(buf + 4, (cls < 0 ? 0.0f : lwc1(m + 0x14)) + off[1]);
                        swc1(buf + 8, (cls < 0 ? 0.0f : lwc1(m + 0x18)) + off[2]);
                        swc1(buf + 0xc, 0.0f);
                        sw(buf + 0x10, 0);
                        sw(buf + 0x14, 0);
                        swc1(buf + 0x18, yaw != 1e9f ? yaw : std::atan2(-off[1], -off[0]) + 1.5707963f);
                        sw(buf + 0x1c, 0);
                        rac1::hero_place(buf, buf + 0x10, -1, 1);
                        ee_stack_free(0x20);
                        std::printf("[orig] RAC1_HERO_AT: the hero placed at %.1f %.1f %.1f next to moby %08x of class %d at frame %llu\n", lwc1(0x0013f3d0), lwc1(0x0013f3d4), lwc1(0x0013f3d8), m, cls,
                                    static_cast<unsigned long long>(frame));
                        ++hero_at_done;
                        break;
                    }
                }
            }
            // testing: RAC1_UNLOCK_PLANETS=<l>,<l>..: at the first gameplay frame those planets made reachable by
            // the ship (the game's own 0x2607d0, as a level's infobot does: 0x13dd40[l], the list 0x13d510 the galactic map shows)
            if (const char* up = std::getenv("RAC1_UNLOCK_PLANETS"); up != nullptr && !unlock_done && !exiting && !ee_thread_suspended() && lw(OV(0x0015f5c4)) == 0) {
                for (const char* q = up; *q != 0;) {
                    char* e = nullptr;
                    const long l = std::strtol(q, &e, 0);
                    if (e == q) break;
                    if (l >= 0 && l < 0x13) rac1::level_mark_visited(static_cast<int32_t>(l));
                    q = *e == ',' ? e + 1 : e;
                }
                unlock_done = true;
            }
            // testing: RAC1_KILL_UIDS=<frame>:<uid>,..: at that game frame those mobys' state set to 0xfe (the
            // state a dead moby's watchers test, e.g. Novalis's Blarg dropship 790 for its troopers 325..327); no other effect
            if (const char* ku = std::getenv("RAC1_KILL_UIDS"); ku != nullptr && !exiting) {
                char* e = nullptr;
                const uint64_t kat = std::strtoull(ku, &e, 10);
                if (frame == kat && e != nullptr && *e == ':')
                    for (const char* q = e + 1; *q != 0;) {
                        char* n = nullptr;
                        const long uid = std::strtol(q, &n, 0);
                        if (n == q) break;
                        if (uid >= 0 && uid < 0x800) sb(lwu(OV(0x0015ffd8)) + (static_cast<uint32_t>(uid) << 8) + 0x20, 0xfe);
                        q = *n == ',' ? n + 1 : n;
                    }
            }
            if (const char* k = std::getenv("RAC1_KILL_AT"); k != nullptr && frame == std::strtoull(k, nullptr, 0))  // testing: the hero's health to 0
                sw(0x001415f8, 0);
            // testing: RAC1_AMMO=<item>:<n>,...: those weapons' ammo 0x13d428[item] set at RAC1_GIVE_FRAME + 5
            if (const char* am = std::getenv("RAC1_AMMO"); am != nullptr && frame == give_frame + 5) {
                for (const char* q = am; *q != 0;) {
                    char* e = nullptr;
                    const long it = std::strtol(q, &e, 0);
                    if (e == q || *e != ':') break;
                    const long n = std::strtol(e + 1, &e, 0);
                    sw(0x0013d428 + 4u * static_cast<uint32_t>(it), static_cast<uint32_t>(n));
                    q = *e == ',' ? e + 1 : e;
                }
            }
            // testing: RAC1_PICKUP_LOG=<period>: every <period> frames the live item pickups (update 0x2c53c0) and the
            // crates (0x2cf218) within 15 of the hero (all at the first log): class, state, item / amount / timer / velocity, the item's ammo and maximum
            if (const char* pl = std::getenv("RAC1_PICKUP_LOG"); pl != nullptr && frame % std::max(1ul, std::strtoul(pl, nullptr, 0)) == 0) {
                for (uint32_t m = lwu(OV(0x0015ffd8)); m < lwu(OV(0x0015ffe0)); m += 0x100) {
                    const uint8_t st = lbu(m + 0x20);
                    if (st >= 0xfe) continue;
                    const uint32_t fn = lwu(m + 0x74), p = lwu(m + 0x78);
                    const float dx = lwc1(m + 0x10) - lwc1(0x0013f3d0), dy = lwc1(m + 0x14) - lwc1(0x0013f3d4);
                    if (fn == OVF(0x002c53c0u)) {
                        const uint32_t it = lwu(p + 8);
                        std::printf("[pickup] frame %d %08x class %d state %u item 0x%x amount %d timer %d pos %.2f %.2f %.2f vel %.3f %.3f %.3f scale %.3f ammo %d/%u hero %.2f %.2f %.2f\n", static_cast<int>(frame), m,
                                    lh(m + 0xa6), st, it, lw(p), lw(p + 0x20), lwc1(m + 0x10), lwc1(m + 0x14), lwc1(m + 0x18), lwc1(p + 0x10), lwc1(p + 0x14), lwc1(p + 0x18), lwc1(m + 0x2c),
                                    it < 0x40 ? lw(0x0013d428 + it * 4) : -1, it < 0x40 ? lhu(OV(0x001c40b0) + it * 0x18 + 0xe) : 0u, lwc1(0x0013f3d0), lwc1(0x0013f3d4), lwc1(0x0013f3d8));
                    } else if (fn == OVF(0x002cf218u) && (dx * dx + dy * dy < 225.0f || frame == std::strtoul(pl, nullptr, 0))) {  // (all of them the first time)
                        std::printf("[crate] frame %d %08x class %d state %u kind +0xc6 %d +0xcb %u pos %.2f %.2f %.2f\n", static_cast<int>(frame), m, lh(m + 0xa6), st, lh(p + 0xc6), lbu(p + 0xcb),
                                    lwc1(m + 0x10), lwc1(m + 0x14), lwc1(m + 0x18));
                    }
                }
            }
            if (std::getenv("RAC1_PART_TRACE") != nullptr && frame % 30 == 0) {  // the live particles by type
                const uint32_t base = lwu(OV(0x0016016c));
                const int32_t n = lw(OV(0x00160174));
                int hist[256] = {};
                for (int32_t i = 0; i <= n && i < 0x800; ++i) {
                    const uint32_t r = base + static_cast<uint32_t>(i) * 0x40;
                    if (static_cast<int8_t>(ee_read<uint8_t>(r + 1)) >= 0) ++hist[ee_read<uint8_t>(r)];
                }
                std::printf("[part] frame %llu:", static_cast<unsigned long long>(frame));
                for (int t = 0; t < 256; ++t) if (hist[t] != 0) std::printf(" %d:%d(fn %08x)", t, hist[t], lwu(OV(0x001b1e80u) + 4u * static_cast<uint32_t>(t)));
                std::printf("\n");
            }
            if (std::getenv("RAC1_GS_TRACE") != nullptr && lwu(0x0013c940 + 0x1a4) != 0) std::printf("[gs] pad pressed %x frame %llu\n", lwu(0x0013c940 + 0x1a4), static_cast<unsigned long long>(frame));
            if (const char* zt = std::getenv("RAC1_GS_ZTRACE"); zt != nullptr && frame == std::strtoull(zt, nullptr, 0))  // testing: the primitives' depth state
                for (const auto& pr : gs.primitives())
                    std::printf("[gsz] prim %d vu %d frame %llx scissor %llx test %llx zbuf %llx alpha %llx z %u %u %u xy %.1f %.1f rgba %02x%02x%02x%02x tex0 %llx tex1 %llx miptbp1 %llx q %f %f %f\n", pr.type, pr.vu ? 1 : 0, (unsigned long long)pr.st.frame, (unsigned long long)pr.st.scissor, (unsigned long long)pr.st.test, (unsigned long long)pr.st.zbuf,
                                (unsigned long long)pr.st.alpha, pr.v[0].z, pr.v.size() > 1 ? pr.v[1].z : 0u, pr.v.size() > 2 ? pr.v[2].z : 0u, pr.v[0].x, pr.v[0].y,
                                pr.v[0].r, pr.v[0].g, pr.v[0].b, pr.v[0].a, (unsigned long long)pr.st.tex0, (unsigned long long)pr.st.tex1, (unsigned long long)pr.st.miptbp1, pr.v[0].q, pr.v.size() > 1 ? pr.v[1].q : 0.0f, pr.v.size() > 2 ? pr.v[2].q : 0.0f);
            if (std::getenv("RAC1_GS_TRACE") != nullptr && frame % 60 == 1)
            {
                std::printf("[gs] chain %08x..%08x: %zu primitives (origin %x %x, display %d x %d, xyoffset %llx)\n", start, end, gs.primitives().size(), lwu(0x0013e510), lwu(0x0013e514), lhu(0x001518d0), lhu(0x001518d2), (unsigned long long)gs.reg(0x18));
                if (const char* dd = std::getenv("RAC1_GS_DUMP"); dd != nullptr && !gs.primitives().empty()) {  // the primitives' textures
                    for (size_t k = 0; k < gs.primitives().size() && k < 12; ++k) {
                        const auto& pr = gs.primitives()[k];
                        const int tw = 1 << ((pr.st.tex0 >> 26) & 15), th = 1 << ((pr.st.tex0 >> 30) & 15);
                        char fn[256];
                        std::snprintf(fn, sizeof fn, "%s_%02zu.ppm", dd, k);
                        if (FILE* f = std::fopen(fn, "wb")) {
                            std::fprintf(f, "P6 %d %d 255\n", tw, th);
                            for (int yy = 0; yy < th; ++yy) for (int xx = 0; xx < tw; ++xx) {
                                const uint32_t c = gs.texel(pr.st.tex0, pr.st.texa, xx, yy);
                                const uint8_t rgb[3] = {static_cast<uint8_t>(c), static_cast<uint8_t>(c >> 8), static_cast<uint8_t>(c >> 16)};
                                std::fwrite(rgb, 1, 3, f);
                            }
                            std::fclose(f);
                        }
                    }
                }
                for (size_t k = 0; k < gs.primitives().size() && k < 6; ++k) {
                    const auto& pr = gs.primitives()[k];
                    std::printf("[gs]  prim %d tex0 %016llx alpha %llx test %llx scissor %llx:", pr.type, (unsigned long long)pr.st.tex0, (unsigned long long)pr.st.alpha, (unsigned long long)pr.st.test, (unsigned long long)pr.st.scissor);
                    for (const auto& v : pr.v) std::printf(" (%.1f %.1f z%x rgba %02x%02x%02x%02x uv %.1f %.1f)", v.x, v.y, v.z, v.r, v.g, v.b, v.a, v.u, v.v);
                    std::printf("\n");
                }
                for (uint32_t i = 0; i < 13; ++i) {  // the HUD slots (request, fade, update / draw)
                    const uint32_t e = OV(0x0017dc50) + i * 0x90;
                    if (lwu(e + 0x18) != 0 || lw(e + 0x68) != 0)
                        std::printf("[gs]  hud %u: key %x pending %d fade %d timer %d upd %08x draw %08x\n", i, lwu(e + 0x20), lw(e + 0x68), lw(e + 0x6c), lw(e + 0x7c), lwu(e + 0x14), lwu(e + 0x18));
                }
            }
        };
        auto draw = [&](FrameDraw& fd) {
            RAC1_PROF_ZONE(kDraw);
            w = fd.w;
            h = fd.h;
            const int vx = fd.vx, vy = fd.vy, vw = fd.vw, vh = fd.vh;
            const bool menu_mode = fd.menu_mode, title = fd.title, story = fd.story;
            const std::vector<uint8_t>* movie_pic = fd.movie_pic;
            const int mw = fd.mw, mh = fd.mh;
            if (menu_mode || fd.vendor_pic || title || movie_pic != nullptr || fd.exiting) rac1::gs::memory_join();  // (those draws may use the GS's local memory)
            if (fd.prev_chain && !fd.fade && mrp_chain_used()) {
                // the frame before's chain walked again (a frame that built none of its own: the new level's first): its world and
                // mobys are not the MRP's scene (it has none of that chain) - the GS replica drew them as the VU1 programs kicked them;
                // the picture shown is kept instead, as on a held fade's frame (one frame, the loading screen's last)
                rac1::gs::memory_join();
                if (mrp_chain_frame().dropped_3d > 0) {
                    fd.fade = fd.fade_held = true;
                    mrp_moby_begin_frame(true, false, false);
                }
            }
            // (a frame that is not the game's own may show the last one: kept in the 3D's colour image until now, copied before the clear)
            if (menu_mode || fd.fade || fd.vendor_pic || title || movie_pic != nullptr || fd.exiting) g_lazy_shown.resolve();
            else g_lazy_shown.pending = false;  // (a frame of the game: it will be the last shown)
            if (rac1::vk::is_active()) {
                rac1::vk::begin_frame(w, h, 0.02f, 0.02f, 0.04f, 1.0f);  // (colour, depth and stencil cleared)
                rac1::vk::set_viewport(vx, vy, vw, vh);
            } else {
                glViewport(0, 0, w, h);
                glClearColor(0.02f, 0.02f, 0.04f, 1.0f);
                glClearStencil(0);
                glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);  // (the stencil with the depth: free; the ambient occlusion's moby mark starts at 0)
                glViewport(vx, vy, vw, vh);
                if (g_render_wireframe) glPolygonMode(GL_FRONT_AND_BACK, GL_LINE);
            }
            mrp_world_finish_frame();  // (the scene: waited for, its triangles and draw lists)
            mrp_lighting_frame_begin();  // (the modern lighting: off until this frame's shadow pass)
            bool world_3d = false;       // (the MRP drew the level's world this frame: the ambient particles over it)
            if (movie_pic != nullptr) {  // a movie plays: its picture is the whole frame
                movie_picture.draw(*movie_pic, mw, mh, vx, vy, vw, vh);
            } else if (fd.fade) {  // a frame of the screen fade (ADR 0012): the display buffer, the picture shown, its packets drawn over it
                const FrozenFrame& disp = displayed_is_last_shown ? last_shown : displayed;
                if (disp.valid() && disp.w == w && disp.h == h) disp.blit_to_screen();
            } else if (title && !fd.flight) {  // the boot's title level (its menu over it): its sky, ties and shrubs (its mobys: the original pass)
                if (mrp_world_active()) {  // its own passes' chain, drawn by the MRP world
                    rac1::gs::Viewport wv;
                    wv.x = vx; wv.y = vy; wv.w = vw; wv.h = vh;
                    wv.xscale = 1.0f / g_view_widen;
                    wv.xscale_3d = 1.0f;
                    RAC1_PROF_ZONE(kWorld);
                    mrp_world_draw(w, h, &wv);
                } else if (g_title_scene_host && !rac1::vk::is_active()) {  // (RAC1_TITLE_HOST=1) the old host renderers
                    title_scene_host_draw();
                }  // (else the chain's: the ported VU1 programs and the GS replica)
            } else if (menu_mode && g_scene_pic.menu_pic && g_scene_pic.pic.w == w && g_scene_pic.pic.h == h) {
                // the frame the menu's start stored (0x2a0f18), put back each frame by 0x2a0fc8: the picture shown then
                g_scene_pic.pic.blit_to_screen();
            } else if (menu_mode && last_shown.valid() && last_shown.w == w && last_shown.h == h) {
                last_shown.blit_to_screen();
            } else if (!menu_mode && !fd.vendor_pic && (!fd.exiting || (fd.flight && g_world_mrp))) {  // (the level exit: the level left is not drawn; its loading screen's flight is)
                if (g_render_sky && !g_world_faithful && !g_world_mrp && !rac1::vk::is_active()) sky_draw();  // (g_world_faithful: the sky comes from the frame's chain too, 0x23d968, g_world_mrp: the MRP draws it from that chain)
                if (g_world_mrp) {  // the MRP world: the frame's chain decoded (above), the world drawn from its assets
                    rac1::gs::Viewport wv;
                    wv.x = vx; wv.y = vy; wv.w = vw; wv.h = vh;
                    wv.xscale = 1.0f / g_view_widen;
                    wv.xscale_3d = 1.0f;
                    // the render scale: the MRP's 3D (world, sky, mobys) into its own buffer, put on the window before the effects
                    if (const float rs = rac1::graphics_render_scale(); rs != 1.0f && !g_render_wireframe) {
                        g_scaled_scene.begin(w, h, rs);
                        wv = g_scaled_scene.scaled(wv);
                        if (rac1::vk::is_active()) rac1::vk::set_viewport(wv.x, wv.y, wv.w, wv.h);
                        else glViewport(wv.x, wv.y, wv.w, wv.h);
                    }
                    {
                        RAC1_PROF_ZONE(kMobyLists);
                        if (mrp_moby_active()) mrp_moby_prepare(&fd.gv);  // (the modern lighting: the mobys cast too)
                    }
                    mrp_lighting_shadow_pass();
                    RAC1_PROF_ZONE(kWorld);
                    mrp_world_draw(w, h, &wv);
                    world_3d = !fd.exiting && !story;
                } else if (!g_world_faithful && !rac1::vk::is_active()) {
                    tfrag_render_update();  // (g_world_faithful: the world comes from the frame's VU1 chain)
                }
            }
            fprof.mark(1);
            // the menu's mobys are a part of its 4:3 layout: cut at the frame's edges in widescreen
            // (the original's frame ends there; its animations slide them out of it)
            const bool menu_cut = menu_mode && g_view_widen > 1.0f;
            if (menu_cut) {
                const int cw = static_cast<int>(static_cast<float>(vw) / g_view_widen);
                if (rac1::vk::is_active()) {
                    rac1::vk::set_scissor(true, vx + (vw - cw) / 2, vy, cw, vh);
                } else {
                    glEnable(GL_SCISSOR_TEST);
                    glScissor(vx + (vw - cw) / 2, vy, cw, vh);
                }
            }
            // the mobys: the host's model drawer, or the original pass (g_moby_vu1_pass: debug menu,
            // RAC1_MOBY_VU1; ADR 0007) drawing through the GS; the exit's cards and movies draw no 3D
            // (the moby list left is not drawn)
            if (g_render_moby && !g_moby_vu1_pass && !rac1::vk::is_active() && movie_pic == nullptr && !story && !title && !fd.fade && !fd.vendor_pic) {
                RAC1_PROF_ZONE(kMobysLegacy);
                orig_moby_draw();
            }
            fprof.mark(2);
            if (menu_cut) {
                if (rac1::vk::is_active()) rac1::vk::set_scissor(false);
                else glDisable(GL_SCISSOR_TEST);
            }
            fprof.mark(3);
            if (g_render_wireframe && !rac1::vk::is_active()) glPolygonMode(GL_FRONT_AND_BACK, GL_FILL);
            {
                rac1::gs::memory_join();  // (the frame's chain run on the worker thread)
                rac1::gs::Gs& gs = rac1::gs::gs_global();
                gs_prims_dump(gs, game_frame);  // (testing)
                fprof.mark(4);
                rac1::gs::Viewport& gv = fd.gv;
                const bool has_dm = fd.has_dm;
                rac1::gs::DepthMap& dm = fd.dm;
                if (mrp_shadows_check_enabled() && mrp_moby_active()) {  // (RAC1_MRP_SHADOW_CHECK: the captured shadow faces against the GS path's primitives)
                    float vpm[16];
                    mrp_world_view_proj(vpm);
                    mrp_shadows_check_gs(&gs.primitives(), w, h, fd.fb_w, fd.fb_h, vx, vy, vw, vh, has_dm ? dm.a : 0.0, has_dm ? dm.b : 0.0, vpm);
                }
                if (mrp_world_testing() && !rac1::vk::is_active())  // (testing: the in-game check, the offscreen image diff of the faithful world against the MRP's)
                    mrp_world_after_chain(w, h, &gv, gs, fd.fb_w, fd.fb_h, fd.frame, has_dm ? &dm : nullptr);
                if (s_moby_diff && !rac1::vk::is_active()) {
                    moby_gpu_diff_compare_drawn(w, h, &gv, gs, fd.fb_w, fd.fb_h, fd.frame, has_dm ? &dm : nullptr);
                }
                if (g_render_moby && !rac1::vk::is_active() && (g_moby_renderer == MobyRenderer::kGpu || s_moby_diff) && movie_pic == nullptr && !story && !fd.fade && !(title && lw(OV(0x0015f5c4)) == 0)) {
                    moby_gpu_draw(w, h, &gv);
                }
                static const bool s_mrp_diff = std::getenv("RAC1_MRP_DIFF") != nullptr;
                const int prim_filt = (g_moby_renderer == MobyRenderer::kGpu || s_moby_diff || mrp_moby_active() || (mrp_world_testing() && mrp_world_active())) ? 2 : 0;
                // the GS replica's primitives the chain made before the moby pass (the draw lists 0x1f85b8 runs before it: Hoven's water,
                // translucent and without Z): drawn under the MRP's mobys, in the original's order (drawn after them, they covered what a
                // moby draws without Z - the faces of Ratchet's ice cube, which fail its alpha test and write their colour only, #48)
                // the frame's primitives in their order: the reader's (mrp_chain.h; its 2D drawn by mrp_2d.h), or the GS replica's (the
                // faithful reference; with the MRP's effects, testing: RAC1_MRP_CHAIN=0 / _CHECK, a copy of its primitives for mrp_fx)
                const bool reader = mrp_chain_used();
                static MrpChainFrame s_replica_frame;
                if (!reader && gs.fx_external()) {
                    s_replica_frame.prims = gs.primitives();
                    s_replica_frame.vu7_batches = gs.vu7_batches();
                    s_replica_frame.vu7_verts = gs.vu7_vertices();
                    s_replica_frame.fx_mark = gs.fx_mark();
                    s_replica_frame.moby_mark = gs.moby_mark();
                    s_replica_frame.fx_external = true;
                }
                const MrpChainFrame& cf = reader ? mrp_chain_frame() : s_replica_frame;
                const bool fx_external = reader ? cf.fx_external : gs.fx_external();
                const size_t moby_mark = reader ? cf.moby_mark : gs.moby_mark();
                // the 2D of [from, to) in viewport `v`: the MRP's 2D drawer, or the replica's
                auto draw_2d = [&](const rac1::gs::Viewport& v, size_t from, size_t to) {
                    RAC1_PROF_ZONE(kGs2d);
                    if (reader) mrp_2d_draw(cf, w, h, fd.fb_w, fd.fb_h, has_dm ? &dm : nullptr, &v, from, to);
                    else rac1::gs::draw(gs, w, h, fd.fb_w, fd.fb_h, has_dm ? &dm : nullptr, &v, prim_filt, from, to);
                };
                size_t pre_moby = 0;
                if (mrp_moby_active() && g_render_moby && g_render_gs2d && movie_pic == nullptr && !fd.fade_held && !menu_mode && !title) {
                    const size_t mark = moby_mark;
                    if (mark != SIZE_MAX && mark > 0) {
                        const rac1::gs::Viewport pv = g_scaled_scene.on ? g_scaled_scene.scaled(gv) : gv;
                        if (fx_external) {
                            RAC1_PROF_ZONE(kEffects);
                            mrp_fx_draw(cf, w, h, &pv, [&](size_t from, size_t to) { draw_2d(pv, from, to); }, 0, mark);
                        } else {
                            draw_2d(pv, 0, mark);
                        }
                        pre_moby = mark;
                    }
                }
                if (mrp_moby_active()) {  // the MRP's mobys (with RAC1_MRP_DIFF: first both renderers' offscreen, compared)
                    if (s_mrp_diff && !rac1::vk::is_active()) mrp_moby_diff(w, h, &gv, gs, fd.fb_w, fd.fb_h, fd.frame, has_dm ? &dm : nullptr);
                    if (g_render_moby) {
                        RAC1_PROF_ZONE(kMobys);
                        mrp_lighting_ao_moby_mark(true);  // (their pixels marked for the ambient occlusion: gentler on them; nothing unless it is on)
                        if (g_scaled_scene.on) {  // (the render scale's buffer, its viewport)
                            const rac1::gs::Viewport sgv = g_scaled_scene.scaled(gv);
                            mrp_moby_draw(w, h, &sgv);
                        } else {
                            // the full-screen menu's mobys cut at its 4:3 frame (the original's frame ends there)
                            const bool mrp_cut = menu_mode && g_view_widen > 1.0f;
                            if (mrp_cut) {
                                const int cw = static_cast<int>(static_cast<float>(vw) / g_view_widen);
                                if (rac1::vk::is_active()) {
                                    rac1::vk::set_scissor(true, vx + (vw - cw) / 2, vy, cw, vh);
                                } else {
                                    glEnable(GL_SCISSOR_TEST);
                                    glScissor(vx + (vw - cw) / 2, vy, cw, vh);
                                }
                            }
                            g_mrp_moby_keep_scissor = mrp_cut;
                            mrp_moby_draw(w, h, &gv);
                            g_mrp_moby_keep_scissor = false;
                            if (mrp_cut) {
                                if (rac1::vk::is_active()) rac1::vk::set_scissor(false);
                                else glDisable(GL_SCISSOR_TEST);
                            }
                        }
                        mrp_lighting_ao_moby_mark(false);
                    }
                }
                mrp_lighting_ao_pass();  // (the ambient occlusion over the 3D, before the effects and the 2D; nothing unless asked)
                // program 7's surfaces that write Z (the seas, lava, sand) before our ambient particles, which write none: drawn after them,
                // the seas covered the rain, snow and embers in front of them (Batalia's rain). Only with the particles on (the order
                // otherwise the chain's, the translucent effects and the 2D in it still)
                bool surfaces_early = false;
                if (world_3d && g_ambient_particles > 0 && reader && fx_external && movie_pic == nullptr && !fd.fade_held) {
                    RAC1_PROF_ZONE(kEffects);
                    const rac1::gs::Viewport sv = g_scaled_scene.on ? g_scaled_scene.scaled(gv) : gv;
                    mrp_fx_draw_surfaces(cf, w, h, &sv, pre_moby);
                    surfaces_early = true;
                }
                if (world_3d && g_ambient_particles > 0) mrp_particles_draw(fd.frame);  // (our ambient particles: nothing unless asked)
                if (g_scaled_scene.on) {  // the scaled 3D on the window; the effects and the 2D drawn over it at the window's resolution
                    RAC1_PROF_ZONE(kScaleResolve);
                    g_scaled_scene.end(w, h);
                    if (rac1::vk::is_active()) rac1::vk::set_viewport(vx, vy, vw, vh);
                    else glViewport(vx, vy, vw, vh);
                }
                static const bool s_moby_new = std::getenv("RAC1_MOBY_NEW_TRACE") != nullptr;  // (testing: the mobys that appear, and how they draw)
                if (s_moby_new) {
                    static std::set<uint32_t> seen;
                    std::set<uint32_t> now;
                    uint32_t m = lwu(OV(0x0015ffd8u));
                    for (int guard = 0; guard < 4096 && m != 0 && m < 0x02000000u && lbu(m + 0x20) != 0xff; ++guard, m += 0x100) {
                        if (static_cast<int8_t>(lbu(m + 0x20)) < 0) continue;
                        const uint32_t key = m ^ (static_cast<uint32_t>(lhu(m + 0xa6)) << 25);
                        now.insert(key);
                        if (seen.count(key) == 0) {
                            bool in_scene = false;
                            if (mrp_moby_active())
                                for (const MrpMoby& sm : mrp_scene_draw().mobys) in_scene = in_scene || sm.moby == m;
                            std::printf("[moby-new] frame %llu moby %08x class %d state %d +0x34 %04x +0x31 %u +0x74 %08x +0x24 %08x in MRP scene %d\n", static_cast<unsigned long long>(fd.frame), m,
                                        lh(m + 0xa6), static_cast<int8_t>(lbu(m + 0x20)), lhu(m + 0x34), lbu(m + 0x31), lwu(m + 0x74), lwu(m + 0x24), in_scene ? 1 : 0);
                        }
                    }
                    seen.swap(now);
                }
                static const bool s_hero_trace = std::getenv("RAC1_HERO_TRACE") != nullptr;  // (testing: the hero in the drawn scene)
                if (s_hero_trace) {
                    const uint32_t hm = lwu(0x001413d0);
                    int in_scene = -1;
                    const MrpMoby* sm = nullptr;
                    if (mrp_moby_active())
                        for (const MrpMoby& m : mrp_scene_draw().mobys)
                            if (m.moby == hm) { in_scene = 1; sm = &m; break; }
                    if (mrp_moby_active() && sm == nullptr) in_scene = 0;
                    std::printf("[hero] frame %llu moby %08x state %d +0x31 %u +0x34 %04x +0x23 %u +0x3c %08x +0x32 %d in MRP scene %d tint a %u far %d deferred %d\n",
                                static_cast<unsigned long long>(fd.frame), hm, hm ? static_cast<int8_t>(lbu(hm + 0x20)) : 0, hm ? lbu(hm + 0x31) : 0, hm ? lhu(hm + 0x34) : 0,
                                hm ? lbu(hm + 0x23) : 0, hm ? lwu(hm + 0x3c) : 0, hm ? lh(hm + 0x32) : 0, in_scene, sm ? sm->tint[3] : 0, sm ? sm->far_lod : 0, sm ? sm->deferred : 0);
                }
                fprof.mark(5);
                // (the MRP's 3D effects in the primitives' order, the GS replica's runs between them; those before the moby pass drawn above)
                if (fx_external && movie_pic == nullptr && !fd.fade_held) {
                    RAC1_PROF_ZONE(kEffects);
                    mrp_fx_draw(cf, w, h, &gv, [&](size_t from, size_t to) {
                        if (g_render_gs2d) draw_2d(gv, from, to);
                    }, pre_moby, SIZE_MAX, surfaces_early);
                } else if (g_render_gs2d && movie_pic == nullptr && !fd.fade_held) {
                    draw_2d(gv, pre_moby, SIZE_MAX);
                }
                if (rac1::vk::is_active()) rac1::vk::set_viewport(0, 0, w, h);
                else glViewport(0, 0, w, h);
                fprof.mark(6);
            }
            const int prof_capture = rac1::prof::begin(rac1::prof::kFrameCapture);
            if (!menu_mode && !fd.fade && !fd.vendor_pic && !title && movie_pic == nullptr && !fd.exiting) {
                // the last frame of the game shown: what the frame buffer holds when the pause menu's start stores it (the
                // frame drawn when the menu opens is the gameplay chain drawn as a menu frame here: not that one)
                static const bool lazy_on = std::getenv("RAC1_LAZY_CAPTURE") == nullptr || std::atoi(std::getenv("RAC1_LAZY_CAPTURE")) != 0;
                bool lazy = lazy_on && rac1::vk::is_active() && last_shown.valid() && last_shown.w == w && last_shown.h == h;
#ifdef RAC1_DEVTOOLS
                lazy = lazy && !((!headless || menu_shot) && dt::dev_menu_drawing());  // (drawn over the frame after this point)
#endif
                if (lazy) {
                    g_lazy_shown.pending = true;
                    g_lazy_shown.w = w;
                    g_lazy_shown.h = h;
                    g_lazy_shown.dst = &last_shown;
                } else {
                    last_shown.capture(w, h);
                }
                displayed_is_last_shown = true;
                g_scene_pic.pf_valid = false;
                g_scene_pic.shown = &last_shown;
                g_scene_pic.shown_view[0] = vx;
                g_scene_pic.shown_view[1] = vy;
                g_scene_pic.shown_view[2] = vw;
                g_scene_pic.shown_view[3] = vh;
            } else {
                g_lazy_shown.resolve();  // (not expected: resolved at the frame's start)
                displayed.capture(w, h);  // (the picture shown: what a screen fade darkens)
                displayed_is_last_shown = false;
            }
            if (fd.scene) {
                g_scene_pic.keep(w, h, vx, vy, vw, vh);  // (the picture the game stores after this frame)
                g_scene_pic.prefetch(g_scene_pic.pic, vx, vy, vw, vh);
            }
            rac1::prof::end(prof_capture);
#ifdef RAC1_DEVTOOLS
            if (!headless || menu_shot) {
                RAC1_PROF_ZONE(kDebugMenu);
                dt::dev_menu_draw(update_shown_draw, presents.ema_ms);
            }
#endif
            const uint64_t shown = fd.frame;  // (the game frame this picture is)
        if (const char* shots = std::getenv("RAC1_SHOTS")) {  // testing: screenshots at the frames listed
                RAC1_PROF_ZONE(kScreenshots);
                for (const char* q = shots; *q != 0;) {
                    char* e;
                    const unsigned long f = std::strtoul(q, &e, 10);
                    if (e == q) break;
                    if (f == shown) {
                        char fn[64];
                        std::snprintf(fn, sizeof fn, "shot_%lu.ppm", f);
                        save_ppm(fn, w, h);
                        std::printf("[orig] %s: game mode %d step %d scene frame %d music slot +0xc %d dialog state %d\n", fn, lw(OV(0x0015f5c4)), lw(0x0013e050), lw(OV(0x0016c894)),
                                    lh(0x001516d0 + 0x34 + 0xc), lh(0x001516d0 + 0x5a));
                    }
                    q = *e == ',' ? e + 1 : e;
                }
            }
        };
        // a minimised window (Windows: a fullscreen window alt-tabbed out of is minimised, GLFW_AUTO_ICONIFY) has a framebuffer
        // of 0 x 0: nothing is prepared, drawn nor presented (the renderers' buffers, captures and views would be sized from it,
        // their scales divided by its 0; the swap of a minimised window waits for no vsync). The game goes on at 60 game frames
        // a second, as in a window not shown (a host frame of several game frames does not draw the earlier ones either), the
        // host sleeping until the next game frame is due; a pipelined frame waiting is dropped
        {
            int fb_w = 0, fb_h = 0;
            if (!headless) glfwGetFramebufferSize(window, &fb_w, &fb_h);
            const bool zero = (!headless && (fb_w <= 0 || fb_h <= 0 || glfwGetWindowAttrib(window, GLFW_ICONIFIED) != 0)) ||
                              (test_zero_fb(frame) && !g_test_zero_fb_raw);
            if (zero && !(max_frames != 0 && frame >= max_frames)) {
                if (overlap) {  // (minimised meanwhile: the game frames on this thread, the pipelined frame dropped)
                    overlap = false;
                    game_frames();
                }
                if (have_pend) {
                    have_pend = false;
                    mrp_world_drop_frame();
                }
                fprof.end_frame();
                // (the next game frame is due when game_time reaches 0.5; the debug menu holding the game: a frame's time)
                const double wait_s = host_paused ? 1.0 / 60.0 : std::max(0.0, 0.5 - game_time) / 60.0;
                std::this_thread::sleep_for(std::chrono::duration<double>(std::max(wait_s, 0.001)));
                continue;
            }
        }
        // the pipelined frame of the last host frame: drawn now, after this host frame's game frames (its scene built meanwhile)
        bool presented = false;
        update_shown_draw = update_shown;  // (the debug menu's figure: the game thread may write update_shown meanwhile)
        if (overlap) {  // the game frames on the game thread, the pipelined frame drawn meanwhile (from what its prepare took)
            ++g_overlapped;
            have_pend = false;
            rac1::game_thread::run_async(game_frames);
            static const bool gt_serial = std::getenv("RAC1_GT_SERIAL") != nullptr;  // (debug: the game thread joined before the draw)
            if (gt_serial) rac1::game_thread::finish();
            g_draw_from_snapshot = true;
            rac1::ee_copy_use(true);
            draw(pend);
            rac1::ee_copy_use(false);
            g_draw_from_snapshot = false;
            presented = true;
            {
                RAC1_PROF_ZONE(kGameThreadWait);
                rac1::game_thread::finish();  // (the game thread's GL / GS requests run here, after the drawing)
            }
        } else if (have_pend) {
            have_pend = false;
            if (pend.level != lw(0x0015ed84) || (exiting && !pend.exiting)) {  // (the level changed or left meanwhile: its picture dropped)
                mrp_world_drop_frame();
            } else {
                g_draw_from_snapshot = true;
                rac1::ee_copy_use(true);
                draw(pend);
                rac1::ee_copy_use(false);
                g_draw_from_snapshot = false;
                presented = true;
            }
        }
        static FrameDraw cur;
        const bool pipelined_ok = pipeline && !(max_frames != 0 && frame >= max_frames);  // (a run's last frame drawn at once)
        // the boot's title: its world passes' view in EE memory while the host prepares and draws the frame (the menu's
        // draw set its own after them), the game's bytes back after (mrp_title_view.h); a title frame is never pipelined
        const bool title_view = boot_active() && mrp_title_view_swap_in();
        prepare(cur, pipelined_ok);
        const bool pipe_this = pipelined_ok && mrp_world_pending() && !cur.menu_mode && cur.movie_pic == nullptr && !cur.title && !cur.exiting && !cur.fade && !cur.scene;
        if (pipe_this) {
            pend = cur;
            have_pend = true;
        } else {
            if (presented) {  // (two pictures this host frame: the pipelined one shown first)
                fprof.mark(7);
                RAC1_PROF_ZONE(kPresent);
                presents.swap(window);
            }
            draw(cur);
            presented = true;
        }
        if (title_view) mrp_world_drop_frame();  // (a pending scene's capture done before the bytes go back: none when drawn)
        mrp_title_view_swap_out();
        if (max_frames != 0 && frame >= max_frames) {
            if (g_overlapped != 0) std::printf("[game-thread] %llu host frames ran their game frames on the game thread beside the drawing\n", static_cast<unsigned long long>(g_overlapped));
            rac1::game_thread::report();
            pace_report();
            sample_prof_stop();
            mrp_scene_report();
            mrp_moby_report();
            mrp_shadows_report();
            mrp_world_report();
            mrp_lod_report();
            mrp_fx_report();
            if (s_mrp_skip_mode != 0)
                std::printf("[mrp-skip] mode %d: chain tags walked dry %llu; verify: skipped skinning's next record equal %llu, different %llu\n", s_mrp_skip_mode,
                            static_cast<unsigned long long>(rac1::gs::gs_global().skipped_tags()), static_cast<unsigned long long>(g_moby_mrp_skip_next_ok),
                            static_cast<unsigned long long>(g_moby_mrp_skip_next_bad));
            save_ppm("screenshot.ppm", w, h);
            if (rac1::vk::is_active()) rac1::vk::end_frame();
            if (std::getenv("RAC1_MOBY_LIST") != nullptr)  // the live mobys near the hero at the end
                for (uint32_t m = lwu(OV(0x0015ffd8)); lbu(m + 0x20) != 0xff && m < 0x01fe0000u; m += 0x100) {
                    const float dx = lwc1(m + 0x10) - lwc1(0x0013f3d0), dy = lwc1(m + 0x14) - lwc1(0x0013f3d4);
                    if (static_cast<int8_t>(lbu(m + 0x20)) >= 0 && dx * dx + dy * dy < 400.0f)
                        std::printf("[moby-end] %08x class %d state %d fn %08x pos %.1f %.1f %.1f flags %04x\n", m, lh(m + 0xa6), lbu(m + 0x20), lwu(m + 0x74), lwc1(m + 0x10), lwc1(m + 0x14), lwc1(m + 0x18), lhu(m + 0x34));
                }
            break;
        }
        if (!presented) {  // (nothing drawn this host frame: the first pipelined frame)
            fprof.mark(8);
            fprof.end_frame();
            continue;
        }
        fprof.mark(7);
        fprof.work_done();
        int cap = env_fps_cap >= 0 ? env_fps_cap : 60;
        double pace_hz = 0.0;  // (the present's slot, see HostPacer)
        {
            bool game_held = false;
            double speed = 1.0;
            bool vsync = env_vsync != 0;
            if (!headless) {  // (Options > Graphics; RAC1_FPS_CAP / RAC1_VSYNC over it)
                cap = env_fps_cap >= 0 ? env_fps_cap : rac1::graphics_display().fps_cap;
                vsync = env_vsync >= 0 ? env_vsync != 0 : rac1::graphics_display().vsync;
            }
#ifdef RAC1_DEVTOOLS
            if (!headless) {
                const dt::DevSettings& ds = dt::dev_settings();
                game_held = ds.paused || (ds.open && ds.pause_when_open);
                speed = static_cast<double>(ds.speed);
            }
#endif
            pace_hz = static_cast<double>(cap);
            // the game running: its 60 frames a second (times the debug menu's speed) are the most there is to show, whatever
            // the cap above it - Unlimited too: the host frames past them show no new picture (not presented, see "a host frame
            // that ran no game frame"), and without a slot the presents came at the end of each frame's variable work (the
            // jitter HostPacer removes: 3 or 5 refreshes of a 240 Hz display for one game frame)
            static const bool pace_game = std::getenv("RAC1_PACE_GAME") == nullptr || std::atoi(std::getenv("RAC1_PACE_GAME")) != 0;
            if (pace_game && !game_held && speed >= 1.0) pace_hz = cap > 0 ? std::min(pace_hz, 60.0 * speed) : 60.0 * speed;
            static const bool wayland = glfwGetPlatform() == GLFW_PLATFORM_WAYLAND;
            if (pace_hz <= 0.0 && vsync && wayland) pace_hz = host_refresh_hz(window);  // (no swap interval there: the display's rate; the game held, cap Unlimited)
        }
        {
            RAC1_PROF_ZONE(kPacing);
            // (a headless run paced only when RAC1_FPS_CAP asks: testing the audio at 60 fps; RAC1_REALTIME with RAC1_FPS_CAP=0: as a window with the cap Unlimited)
            if (!headless || env_fps_cap > 0 || (realtime_test && env_fps_cap == 0)) pacer.wait(pace_hz);
        }
        {
            RAC1_PROF_ZONE(kPresent);
            presents.swap(window);
        }
        fprof.mark(8);
        fprof.end_frame();
        if (!headless && pacer.legacy) {
            RAC1_PROF_ZONE(kPacing);
            if (cap > 0) {
                const auto period = std::chrono::nanoseconds(1000000000LL / cap);
                next += period;
                const auto now = std::chrono::steady_clock::now();
                if (now > next + period * 4) next = now;
                std::this_thread::sleep_until(next);
            } else {
                next = std::chrono::steady_clock::now();
            }
        }
    }
    pace_report();
    sample_prof_stop();
    rac1::prof::shutdown();
    fprof.report();
    if (fprof.on && !pacer.legacy)
        std::printf("[prof] pacing: %llu frames late for their slot, %llu re-anchored\n", static_cast<unsigned long long>(pacer.late),
                    static_cast<unsigned long long>(pacer.resync));
#ifdef RAC1_DEVTOOLS
    if (!headless || menu_shot) dt::dev_menu_shutdown();
#endif
    if (rac1::vk::is_active()) {
        g_lazy_shown.pending = false;
        displayed.vk_reset();
        last_shown.vk_reset();
        g_scene_pic.vk_reset();
        movie_picture.vk_reset();
        rac1::vk::shutdown();
    }
    if (window != nullptr) glfwDestroyWindow(window);
    if (g_audio_dump != nullptr) std::fclose(g_audio_dump);
    if (g_mirror_test) {  // (testing)
        snd989_mirror_report();
        if (g_mirror_dump != nullptr) std::fclose(g_mirror_dump);
    }
    audio_out_stop();
    snd989_iop_stop();
    if (!no_graphics) glfwTerminate();
    std::fflush(stdout);
    if (rac1::vuprof::enabled()) rac1::vuprof::report();  // (testing: RAC1_VU_PROF)
    rac1::vu0::fallback_report();  // (testing: RAC1_VU_FALLBACK)
    rac1::vu1::fallback_report();
    if (no_graphics_skip && s_mrp_skip_mode == 2)  // (the graphics path's own report is at its last frame)
        std::printf("[mrp-skip] verify: skipped skinning's next record equal %llu, different %llu\n", static_cast<unsigned long long>(g_moby_mrp_skip_next_ok),
                    static_cast<unsigned long long>(g_moby_mrp_skip_next_bad));
    if (std::getenv("RAC1_VU0_CHAIN") != nullptr) std::fprintf(stderr, "[vu0-chain] %016llx\n", static_cast<unsigned long long>(rac1::vu0::chain_hash()));
    rac1::progress::finish();  // (the launcher's progress.json: the time played since its last write)
    rac1::platform::process_end();  // (std::_Exit runs no atexit handler)
    rac1::update_check::run_update();
#if defined(__WIIU__)
    if (ramhash != nullptr) std::fclose(ramhash);  // (no _Exit here: the file's last frame written)
    return 0;  // (the Wii U: back to ports/wiiu/game's main, which goes back to the menu; no GL objects there)
#else
#if defined(__SANITIZE_ADDRESS__)
    __lsan_do_leak_check();  // (an AddressSanitizer build: std::_Exit skips LeakSanitizer's own check at exit)
#endif
    std::_Exit(0);  // the renderers' static GL objects would be freed after the context is gone
#endif
}
