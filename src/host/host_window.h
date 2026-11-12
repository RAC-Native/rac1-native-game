// The game's window as a desktop's window, and the process's end on a fatal error. Host code.
//
// - Never "always on top". GLFW makes a fullscreen window HWND_TOPMOST on Windows (at its creation, at
//   glfwSetWindowMonitor, and again each time it is restored from the taskbar): a topmost window stays over every
//   other one, the Task Manager included, and while the game does not answer (a long load, a hang) it cannot even
//   be minimised: the other windows could not come to the front. host_window_frame takes the flag off as soon as it
//   is seen (once a host frame: GLFW sets it back on its own), so the window is an ordinary one; covering its monitor it
//   is still presented as a fullscreen one (the drivers' "independent flip" needs no topmost).
// - A fatal error ends the process at once. An uncaught C++ exception (std::terminate), an abort() or a crash went to
//   Windows' error reporting by default, which kept the process alive, frozen, while it wrote its report (and asked,
//   in a dialog behind the game's window): the fullscreen window stayed on the screen, "not responding", until the
//   process was killed. host_fatal_handlers_install's handlers write what happened to the log (stderr: game.log
//   when the launcher started the game), give the monitor its desktop mode back and end the process with exit
//   status 3. The same on Linux for std::terminate (the message, then abort as before).
#ifndef RAC1_HOST_HOST_WINDOW_H
#define RAC1_HOST_HOST_WINDOW_H

struct GLFWwindow;

namespace rac1 {

// orig: (host) once at the start, before the window: the fatal errors' handlers
void host_fatal_handlers_install();
// orig: (host) the window made (or made again): it is the one the handlers restore; not topmost
void host_window_created(GLFWwindow* w);
// orig: (host) once a host frame, after the window's events: not topmost (GLFW's fullscreen sets it again)
void host_window_frame(GLFWwindow* w);

}  // namespace rac1

#endif
