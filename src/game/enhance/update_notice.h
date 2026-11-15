// The update notice (GitHub issue #14): when the update check (src/host/update_check.cpp, at start, in
// the background) found a newer version, a panel at the main menu says so, with the release notes in
// short and how to update, and offers OK / Skip Version. Not a port: a page built from the
// game's own menu widgets, as the New Game mode popup (graphics.h). Shown at most once a run, at the
// main menu's first page only, never during play; nothing at all when there is nothing new (headless
// runs never check).
#ifndef RAC1_GAME_ENHANCE_UPDATE_NOTICE_H
#define RAC1_GAME_ENHANCE_UPDATE_NOTICE_H

#include <cstdint>
#include <string>

namespace rac1 {

// What the update check found newer than the installed build
struct UpdateOffer {
    bool development = false;  // the development channel: new commits on the dev branch (else a release)
    std::string id;            // what "Skip Version" keeps: the release's tag / the dev branch's commit
    std::string version;       // (stable) the release's version, x.y.z
    int commits = 0;           // (development) the dev branch's commits the installed build lacks
    std::string notes;         // (stable) the release notes in short, plain ASCII
    std::string installed;     // the installed version (stable) / commit, short (development)
    bool git = false;          // built from a git clone (to update: git pull), else from a download
    bool windows = false;      // the update's command: setup.cmd, else ./setup.sh
};

// orig: (none) — enhancement: the host's update check found `offer` (the game's thread, once): the
// notice shown at the main menu's first page when it is next idle there
void update_notice_offer(const UpdateOffer& offer);

// orig: (none) — enhancement: the main menu's frame (title_menu_update, before the widgets): when an
// offer waits, the menu `ms` (its state block) idle on its first page `root` and nothing else under way,
// the notice's page built and opened, as an entry with a page (action 3) opens it
void update_notice_menu_frame(uint32_t ms, uint32_t root);

// orig: (none) — enhancement: the list `w` runs its entry's action 3 (a page): when it is the notice's,
// "Skip Version" kept (OK: nothing; the notice comes back at the next start)
void update_notice_choice(uint32_t w);

// orig: (none) — enhancement: the notice's page (graphics_menu_back: leaving it, its parent's boxes)
uint32_t update_notice_page();

// orig: (none) — enhancement: a text of the notice (its ids 0x7e00..), 0 for others
uint32_t update_notice_text(int32_t id);

}  // namespace rac1

#endif  // RAC1_GAME_ENHANCE_UPDATE_NOTICE_H
