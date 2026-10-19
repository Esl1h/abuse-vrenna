/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See relaunch.h.
 *
 *  This software was released into the Public Domain.
 */

#include "sdlport/relaunch.h"

#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <string>
#include <vector>

#ifdef _WIN32
#include <process.h>
#else
#include <limits.h>
#include <unistd.h>
#endif

#include <SDL3/SDL.h>

#include "data/relaunch.h"

namespace abuse::sdlport {

namespace {

bool g_requested = false;

// What the replaced process leaves for the new one. An environment variable
// because it is the one thing exec carries across on every platform, and a
// flag on the command line would be one more thing for relaunch_args to strip.
char const *const kMarker = "ABUSE_RELAUNCHED";

}

bool was_relaunched()
{
    char const *v = getenv(kMarker);
    return v && v[0] == '1';
}

void request_relaunch()
{
    g_requested = true;
}

bool relaunch_requested()
{
    return g_requested;
}

void relaunch(int argc, char **argv)
{
    std::vector<std::string> const args = data::relaunch_args(argc, argv);
    if (args.empty())
        return;

    std::vector<char *> list;
    for (std::string const &a : args)
        list.push_back(const_cast<char *>(a.c_str()));
    list.push_back(nullptr);

#ifdef _WIN32
    _putenv_s(kMarker, "1");
#else
    setenv(kMarker, "1", 1);
#endif

    // The graphics are already closed by the caller; this makes sure nothing
    // of SDL is held across the replacement.
    SDL_Quit();
    fflush(stdout);
    fflush(stderr);

#ifdef _WIN32
    _execv(args[0].c_str(), list.data());
#else
    // The path the kernel started this from, not argv[0], which can be a bare
    // name looked up on PATH or relative to a directory the game has left. An
    // AppImage or a Flatpak is the same binary at the same place either way.
    char self[PATH_MAX];
    ssize_t const n = readlink("/proc/self/exe", self, sizeof(self) - 1);
    if (n > 0)
    {
        self[n] = 0;
        execv(self, list.data());
    }
    execvp(args[0].c_str(), list.data());
#endif

    fprintf(stderr, "Mode: could not start the game again (%s), "
            "the new mode applies the next time it is started\n",
            strerror(errno));
}

}
