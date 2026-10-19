/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See relaunch.h.
 *
 *  This software was released into the Public Domain.
 */

#include "data/relaunch.h"

#include <string.h>

namespace abuse::data {

std::vector<std::string> relaunch_args(int argc, char const *const *argv)
{
    std::vector<std::string> out;
    for (int i = 0; i < argc; i++)
    {
        // Never the program name: it has to survive, and a player cannot have
        // passed --mode as the name of the executable.
        if (i > 0 && !strcmp(argv[i], "--mode"))
        {
            i++;    // and its value
            continue;
        }
        out.push_back(argv[i]);
    }
    return out;
}

}
