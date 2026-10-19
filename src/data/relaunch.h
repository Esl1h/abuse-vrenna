/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  What the game starts again with when the player switches mode.
 *
 *  The mode fixes the width of the picture, the folder abuserc is read from,
 *  the folder saves go to, and which art and sound every cache entry points at,
 *  all of it once at startup. Switching without a restart would mean running
 *  that startup a second time in the same process, and the Lisp load cannot be
 *  run twice. So the game shuts down normally and starts itself again, and the
 *  new mode comes from the file the menu wrote.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_DATA_RELAUNCH_H_
#define ABUSE_DATA_RELAUNCH_H_

#include <string>
#include <vector>

namespace abuse::data {

// The arguments to start again with: the ones the game was given, minus
// --mode and its value. A mode on the command line wins over the saved one, so
// leaving it in would start the game in the mode the player just left.
std::vector<std::string> relaunch_args(int argc, char const *const *argv);

}

#endif
