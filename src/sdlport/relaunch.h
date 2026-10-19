/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Starting the game again, for the switch of mode. See data/relaunch.h for
 *  why it is a restart and not a switch.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_SDLPORT_RELAUNCH_H_
#define ABUSE_SDLPORT_RELAUNCH_H_

namespace abuse::sdlport {

// Asks for the game to start again once it has shut down. Only a flag: the
// caller still leaves the way it always does, so saves and the config are
// written and the window is closed before anything is replaced.
void request_relaunch();
bool relaunch_requested();

// True in a process that relaunch() started. The player was at the title menu
// when they switched, so playing the intro again would be a thirty second
// toll on every switch; the new process goes straight to the menu.
bool was_relaunched();

// Replaces this process with a new one running the same program and
// arguments, without --mode. Called last, after everything is torn down.
// Returns only when it could not, and then the game simply ends: the mode is
// already written down, so the next launch has it anyway.
void relaunch(int argc, char **argv);

}

#endif
