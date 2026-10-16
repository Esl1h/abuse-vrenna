/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Options screen. Phase 4, task 4.5.
 *
 *  Drawn through the native resolution overlay, so it is readable at any
 *  window size, and driven by the same event queue as every other window, so
 *  keyboard, mouse and gamepad all reach it.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_UI_OPTIONS_SCREEN_H_
#define ABUSE_UI_OPTIONS_SCREEN_H_

class Event;

namespace abuse::ui {

// Runs until the player leaves, then writes back whatever they changed.
void run_options_screen();

// The eight packet actions and what triggers them. Runs until the player
// leaves, then writes the whole set back as bind= lines.
void run_rebind_screen();

// Draws one frame of it, for a scripted capture. `capturing` shows the row
// that is waiting for a key.
void draw_rebind_screen(int selected, bool capturing);

// Draws one frame of it into the overlay, without touching the event queue.
// Separate so a scripted run can capture it.
void draw_options_screen(int selected);

// The same, with whether the original data is on this host's disk pinned to
// "no". A golden frame has to look the same on every machine that records it.
void draw_options_screen_pinned(int selected);

// How many rows the screen has, for tests and for the scripted capture.
int options_item_count();

// One line saying the controller is gone, drawn while the game is paused
// because of it. Returns false when there is nothing to say.
bool draw_pad_lost_notice();

// F2 and F3, wherever the player presses them. True when the event was one of
// them and the screen has already run, so the caller drops it.
//
// The game has several event loops that never meet: the tick loop in
// Game::get_input, the title menu's own loop in menu.cpp, and the modal
// windows. A key handled in only one of them works in only one of them, which
// is exactly how F2 ended up reachable inside a level and nowhere else.
bool handle_global_key(Event &ev);

// One line on the menu saying how to reach the options. Drawn there because
// the menu is where a player looks for a way in, and the menu itself is built
// from art this project does not edit.
void draw_options_hint();

}

#endif
