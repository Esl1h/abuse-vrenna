/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  What the Original mode says when the classic sound is not installed.
 *  Phase 4, task 4.4.
 *
 *  The decision of 2026-09-18 was that the package does not carry the original
 *  sound and music: the music's licence is not clear, and Flathub and the AUR
 *  fetch it at install time through their own mechanisms. What is left is the
 *  tarball, the AppImage and Windows, where there is no package manager, and
 *  for those this screen points at the page and at the script. No new HTTP
 *  dependency, which is what SDL_OpenURL buys.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_CLASSIC_DATA_SCREEN_H_
#define ABUSE_UI_CLASSIC_DATA_SCREEN_H_

namespace abuse::ui {

// Shows it and runs until the player leaves. Called at startup when the mode is
// Original and the sound is missing, and from the start menu in any mode while
// the original sound is not installed.
void run_classic_data_screen();

// Draws one frame of it, for a scripted capture.
void draw_classic_data_screen(int selected);

// True when the running mode wants the classic sound and cannot find it.
bool classic_data_missing();

}

#endif
