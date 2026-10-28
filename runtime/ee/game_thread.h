#ifndef RAC1_RUNTIME_EE_GAME_THREAD_H
#define RAC1_RUNTIME_EE_GAME_THREAD_H
// The game's update on a thread of its own, beside the host's drawing (docs/architecture.md, Threads and the cores).
//
// A host frame whose picture is a pipelined one (the frame prepared at the end of the host frame before, drawn now) can run
// its game frames - the EE coroutine (ee_thread.h) and the host's work around each - on the game thread while the main
// thread draws that picture: the drawing reads only what was taken from the game at that frame's preparation, the update
// writes only the game's memory and the host's captures the next preparation reads. The main thread joins the game thread
// before it prepares the next frame, so the game's state is the same, word for word, as with the update on the main
// thread (the same code, in the same order, with the same float environment).
//
// What the update may not do beside the drawing is asked for explicitly: wait_draw() (the GS replica's memory, which the
// drawing reads) and on_main() (a GL / Vulkan call: the context is the main thread's). Both are no-ops off the game thread
// or once the drawing is over.
//
// Host only: nothing of the game decides here.
#include <functional>

namespace rac1::game_thread {

// The main thread: `job` started on the game thread with the caller's float environment (the EE's rounding), returning at once.
// One job at a time. `draw_reads_gs` false: the drawing beside it reads nothing of the GS's local memory (the Wii U's: the 2D
// decoded by the chain's reader, joined before the job), so wait_draw returns at once for this job.
void run_async(std::function<void()> job, bool draw_reads_gs = true);
// The main thread, after its drawing: the job's requests (on_main) run here until the job ends; its exception rethrown.
void finish();
// The jobs' timings (stdout, at the end of a run): the start's latency, the jobs' own time, the join's latency.
void report();
// A job is running on the game thread (asked from any thread).
bool running();
// The calling thread is the game thread.
bool on_game_thread();
// The game thread: waits until the main thread's drawing is over (finish called); elsewhere, or after it, returns at once.
void wait_draw();
// The game thread: `fn` run by the main thread once its drawing is over, this thread waiting for it; elsewhere `fn()` here.
void on_main(const std::function<void()>& fn);
// The main thread, at the end (the Wii U: the way back to the menu): the game thread ended and joined, no job running.
// Elsewhere (the PC's thread is never joined: the process ends with std::_Exit) nothing.
void stop();

}  // namespace rac1::game_thread

#endif
