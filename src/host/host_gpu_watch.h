// The GPU's clocks as the system holds them, in the log. Host code.
//
// A laptop's GPU is held below its speed by the machine, not by the game: its power limit lowered (a power mode, the
// charger's wattage, the temperature), a performance state kept low (P5: the video memory at a seventh of its clock).
// The game is then slower than the same machine gives at other times, the GPU busy 100 % - "54 fps" that no setting
// of the game explains. On an NVIDIA GPU (NVML, the driver's own library: nvml.dll on Windows, libnvidia-ml.so.1 on
// Linux, loaded if there) a thread of its own reads the GPU's state twice a second and writes, every 10 s that it was
// held while the game kept it busy, one `[gpu]` line: the performance state, the clocks against their maximum, the
// power drawn against the limit in force and the driver's default, and the driver's reasons (power cap, thermal).
// At the start one line with the limits. Nothing on another GPU or without the library.
#ifndef RAC1_HOST_HOST_GPU_WATCH_H
#define RAC1_HOST_HOST_GPU_WATCH_H

namespace rac1 {

// orig: (host) a window's run, once: the watch's thread started (RAC1_GPU_WATCH=0: not; 2: the line every 10 s, held or not)
void host_gpu_watch_start();
// orig: (host) at the end: the thread stopped
void host_gpu_watch_stop();

}  // namespace rac1

#endif
