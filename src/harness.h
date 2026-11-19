/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Test harness: command line switches that make a run reproducible and
 *  observable, so replays can be compared byte for byte in CI.
 *
 *  This software was released into the Public Domain.
 */

#ifndef __HARNESS_H_
#define __HARNESS_H_

#include <stdint.h>

namespace abuse::harness {

// Parsed once, before SDL is touched, so that --headless can pick the dummy
// drivers. Unknown arguments are left alone for the existing parsers.
void parse_args(int argc, char **argv);

bool headless();

// True when something other than a person is driving: --headless, a
// playback, a recording, or an input script.
//
// Not the same question as headless(). A scripted run can have a real
// window, which is the only way to exercise the GPU path, and it still must
// not read the machine it runs on: the gamepad is the one that bit.
// SDL_EVENT_GAMEPAD_BUTTON_DOWN arrives as a key press, any key press stops
// a demo playback (src/demo.cpp), and a pad on the desk therefore ended runs
// at a different tick every time. Five of eight, measured.
bool scripted_run();

// Narrower: the input itself comes from somewhere that is not a person.
//
// Recording is the difference. A recording run *is* a person playing, and
// the whole point of it is to capture what they do, gamepad included; it
// is only a scripted run in the sense that the harness is watching. Every
// other use of scripted_run() wants both.
bool input_is_scripted();

// True under --save-dialog, which opens the save picker on purpose to draw
// it. It is the one caller allowed past the guard in load_game, and it
// queues an Esc first so the picker has something to read.
bool save_dialog_wanted();

// --frame-alpha F. Forces the interpolated draw at a fixed point between two
// ticks, which is the only way a scripted capture can show it: the harness
// runs one frame per tick, where there is nothing to blend.
bool frame_alpha(float &out);

// --save-dialog: opens the save-slot picker, the thing that stands between
// pressing down at a save console and a file being written, and closes it
// with Esc. Prints what it returned.
//
// The picker is where the Windows crash has to be: writing the file itself
// is covered by --save-test and passes there. It builds thumbnails from
// whatever saves exist, makes a window out of icon art, and runs its own
// event loop, and none of that had ever been exercised by a test.

// --save-test: saves the game at the end of the run, the way the save
// console does, and says on stdout whether the file was written and how big
// it is. The point is the path and the writing, not the contents.
//
// Saving crashed on Windows the first time a person tried it, on a machine
// none of us can debug, and nothing in the suite had ever written a save
// file. This is how that gets exercised on every platform CI builds for.

// --viewport W H: the size of the buffer the game draws into, which is
// 320x200 everywhere else. Phase 6.5 needs to see what a wider one looks
// like before deciding anything, and the engine has refused -size outside
// the editor since 1995.
//
// A measuring tool, not a feature: it is honoured only under --headless, and
// the HUD is still drawn as if the buffer were 320 wide, which is one of the
// things the measurement is meant to show.
bool viewport_size(int &w, int &h);

// --level-info: prints the size of the level that was loaded, in tiles and
// in pixels, and how much of it a viewport of each aspect ratio would need.
// Read-only, and the run ends right after.
//
// This is the measurement phase 6.5 waits on: the levels were drawn for
// 4:3, and the question that decides whether widescreen is possible at all
// is how much room there is at the edges.
bool want_level_info();
void print_level_info(int fg_tiles_x, int fg_tiles_y, int tile_w, int tile_h,
                      int bg_tiles_x, int bg_tiles_y, int bg_tile_w,
                      int bg_tile_h, int bg_empty);

// --input-script <file>: a player driven from a text file instead of from a
// keyboard, one line per stretch of ticks:
//
//     # comment
//     30 right
//     10 right fire
//     15 none
//
// The three replays this project started with are 400 ticks of nobody
// touching anything, which is how an interpolation bug reached a human
// before it reached a test. This is how a replay with movement gets made:
// run with --level and --record, and the game writes a real recording of
// what the script did.
//
// Consumed one tick per call, which is how often the engine asks for input.
// False when no script is loaded, and the keyboard answers as usual.
bool scripted_input(uint8_t &flags);

// True when --mode was given, so the remembered mode knows to stand aside:
// a flag the player typed beats a file they forgot about.
bool mode_from_command_line();

// --window-size W H. What the window snapshots compare is the presented frame,
// and scale mode, filter and letterbox all depend on the shape of the window
// it was presented into, so a scripted run has to pin it. False when the run
// did not ask for one.
bool window_size(int &w, int &h);

// Applied right after jrand_init(), which would otherwise seed the random
// cursor from the wall clock. See ARCHITECTURE.md section 5.
void apply_seed();

// Called after Lisp::Init() and before the Game is built. In headless mode it
// answers the gamma calibration prompt, which would otherwise block forever
// waiting for a keypress that cannot arrive.
void before_game();

// Starts recording or playback once a Game exists. Returns false when the
// requested demo could not be started, which is a fatal condition for a
// scripted run.
bool start_demo();

// --dump-player: prints the player's position and velocity every tick.
// Written for one question a map compiler cannot answer by reading the
// Lisp: how high the player actually jumps, which decides whether a ledge
// is a route or a wall. Assuming three tiles produced a map with a
// platform nobody could reach.
bool want_player_dump();
void print_player_dump(int tick, int x, int y, int xvel, int yvel);

// --dump-tiles: prints one line per foreground tile, with the collision the
// tile carries. There is no hardness table in this engine: a tile blocks
// because its own art carries a boundary, so the only way to know what a
// tile does is to ask the loaded tile. A map compiler that guesses instead
// builds levels you fall through.
bool want_tile_dump();
void print_tile_dump();

// --dump-dynlight: prints the light table and, for each entry, the object
// type it resolved to in the loaded data, or -1. The table is written by
// name and the names come from the Lisp, which differs between the free
// data and the original: without this there is no way to tell a light that
// is switched off from one whose name was never defined.
bool want_dynlight_dump();
void print_dynlight_dump();

// True under --particle-demo: a burst of debris at the player every few
// ticks, so the particle drawer gets a golden frame of its own. The two
// spawns the game itself has need a hit and a round of ammunition, neither
// of which happens in a scripted run down an empty corridor.
bool particle_demo();

// True under --dynlight-demo: the player carries a light, so that the
// light-emitting objects of block 6.4 get a golden frame. The real sources
// are shots and explosions, and a scripted run produces neither: the
// player's first weapon needs ammunition it has not picked up yet, and the
// shot itself is aimed at wherever the mouse is, which in a headless run is
// the corner. Same reason --particle-demo exists.
bool dynlight_demo();

// True once a --playback has run out of packets. The state change that ends
// it happens inside Game::step, which would go on to the main menu and wait
// there for its attract timeout; a scripted run has nothing to do in it.
bool playback_ended();

// Called once per iteration of the main loop, after the world has stepped.
// Returns false when the run should end.
bool tick();

// Called right before the frame is drawn, so --dump-window can arm the capture
// that has to happen inside the present.
void before_frame();

// Called while the frame is being drawn, from the one place that decides what
// goes on the overlay. Returns true when --dump-options has put the options
// screen there, so the caller leaves it alone: the overlay is cleared every
// frame, and drawing it any earlier would only have it wiped.
bool draw_overlay_for_capture();

// Called right after the frame has been presented, so that --dump-frames
// captures what was actually drawn for this tick.
void after_frame();

// Called after the run, before teardown.
void finish();

// Hash of the integer game state: every object's position, velocity, health,
// AI state and local variables, plus the random cursor and the tick counter.
uint64_t state_hash();

}

#endif
