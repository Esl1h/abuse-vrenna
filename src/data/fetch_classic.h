/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Running the original data fetcher from inside the game.
 *
 *  The original sound and music are not redistributable and are in no
 *  package: the player downloads them. That has always worked, through
 *  scripts/fetch-classic-data.sh, and it has always meant opening a
 *  terminal, which for most people means not doing it at all.
 *
 *  So the game runs the same script. Not a second implementation of the
 *  download: the script checks SHA-256 before it writes anything, writes
 *  atomically, and is what the packages already carry. This only starts it
 *  and reads what it prints.
 *
 *  No new dependency. SDL3 can start a process and hand back its output,
 *  which is the whole reason the libcurl plan of 2026-09-18 was dropped.
 *
 *  Rule 8 of AGENTS.md stands: this is the explicit, confirmed download of
 *  the original data, started by the player on a screen that exists to ask.
 *  Nothing here runs on its own.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_DATA_FETCH_CLASSIC_H_
#define ABUSE_DATA_FETCH_CLASSIC_H_

#include <string>

namespace abuse::data {

enum class FetchState
{
    Idle,
    Running,
    Done,
    Failed,
};

// Where the fetcher is, or empty when there is none to run: a checkout has
// it in scripts/, a package installs it beside the binary, and the Flatpak
// calls it fetch-classic-data. Checked before the screen offers the choice,
// because an option that cannot work should not be on screen.
std::string fetch_script();

// Where the fetcher is told to write: classic_data_dir() without its
// trailing slash. That slash belongs where the path is used as a prefix and
// is poison as an argument, because the script's atomic rename then names a
// directory inside the one it is moving.
std::string fetch_destination();

// Starts it, writing into fetch_destination(). False when it could not be
// started at all, which leaves the state Idle.
bool fetch_start();

// Reads whatever the script has printed since the last call and returns
// where things stand. Called once a frame; it never blocks.
FetchState fetch_poll();

// The last line the script printed, which is what the screen shows. Empty
// before the first line arrives.
char const *fetch_message();

// Back to Idle, so the screen can offer the choice again after a failure.
void fetch_reset();

}

#endif
