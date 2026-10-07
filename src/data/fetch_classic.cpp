/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See fetch_classic.h.
 *
 *  This software was released into the Public Domain.
 */

#include "fetch_classic.h"

#include <stdio.h>
#include <string.h>

#include <filesystem>

#include <SDL3/SDL.h>

#include "paths.h"

namespace abuse::data {

namespace {

SDL_Process *g_process = nullptr;
SDL_IOStream *g_output = nullptr;
FetchState g_state = FetchState::Idle;

// The last whole line the script printed, and whatever has arrived since
// the newline after it.
std::string g_line;
std::string g_partial;

bool is_file(std::string const &path)
{
    std::error_code ec;
    return !path.empty() && std::filesystem::is_regular_file(path, ec);
}

// Keeps the last line and throws the rest away. The script prints one line
// per file it downloads and one per check; the screen has room for one.
void take(char const *data, size_t len)
{
    for (size_t i = 0; i < len; i++)
    {
        char const c = data[i];
        if (c == '\n' || c == '\r')
        {
            if (!g_partial.empty())
            {
                g_line = g_partial;
                g_partial.clear();
            }
        }
        else if (g_partial.size() < 200)
            g_partial.push_back(c);
    }
}

}

std::string fetch_script()
{
    // An override first, which is how a test points at a script of its own
    // without a network in sight.
    if (char const *env = SDL_getenv("ABUSE_FETCH_SCRIPT"))
    {
        if (is_file(env))
            return env;
    }

    // Beside the binary, which covers every package: the tarball and the
    // AppImage install it as abuse-vrenna-fetch-classic-data, the Flatpak
    // as fetch-classic-data.
    if (char const *base = SDL_GetBasePath())
    {
        for (char const *name : { "abuse-vrenna-fetch-classic-data",
                                  "fetch-classic-data" })
        {
            std::string const path = std::string(base) + name;
            if (is_file(path))
                return path;
        }
    }

    // And a checkout, where the game is run from the top of the tree.
    if (is_file("scripts/fetch-classic-data.sh"))
        return "scripts/fetch-classic-data.sh";

    return std::string();
}

std::string fetch_destination()
{
    std::string dest = classic_data_dir();
    while (dest.size() > 1 && dest.back() == '/')
        dest.pop_back();
    return dest;
}

bool fetch_start()
{
    if (g_state == FetchState::Running)
        return true;

    std::string const script = fetch_script();
    if (script.empty())
        return false;

    std::string const dest = fetch_destination();
    char const *args[] = { script.c_str(), dest.c_str(), nullptr };

    fetch_reset();

    // Piped, not inherited: the output belongs on the screen the player is
    // looking at, and a game with no terminal attached has nowhere else to
    // put it.
    g_process = SDL_CreateProcess(args, true);
    if (!g_process)
    {
        printf("Classic data: could not start %s: %s\n",
               script.c_str(), SDL_GetError());
        return false;
    }

    g_output = SDL_GetProcessOutput(g_process);
    g_state = FetchState::Running;
    printf("Classic data: running %s %s\n", script.c_str(), dest.c_str());
    return true;
}

FetchState fetch_poll()
{
    if (g_state != FetchState::Running)
        return g_state;

    // Read what is there and no more. SDL leaves a process pipe
    // non-blocking, so an empty read means "nothing yet", not "finished":
    // only SDL_WaitProcess says that.
    if (g_output)
    {
        char buffer[512];
        for (;;)
        {
            size_t const got = SDL_ReadIO(g_output, buffer, sizeof(buffer));
            if (got == 0)
                break;
            take(buffer, got);
        }
    }

    int status = 0;
    if (SDL_WaitProcess(g_process, false, &status))
    {
        // Whatever was in the pipe when it ended.
        if (g_output)
        {
            char buffer[512];
            for (;;)
            {
                size_t const got = SDL_ReadIO(g_output, buffer, sizeof(buffer));
                if (got == 0)
                    break;
                take(buffer, got);
            }
        }
        if (!g_partial.empty())
        {
            g_line = g_partial;
            g_partial.clear();
        }

        SDL_DestroyProcess(g_process);
        g_process = nullptr;
        g_output = nullptr;

        // The script's own exit code decides, and then the filesystem
        // confirms: it refuses to write anything that fails its checksum,
        // so a zero exit with nothing on disk would be a bug worth seeing.
        g_state = (status == 0 && classic_data_present())
                      ? FetchState::Done
                      : FetchState::Failed;
        printf("Classic data: %s (exit %d)\n",
               g_state == FetchState::Done ? "done" : "failed", status);
    }

    return g_state;
}

char const *fetch_message()
{
    return g_line.c_str();
}

void fetch_reset()
{
    if (g_process)
    {
        SDL_KillProcess(g_process, true);
        SDL_DestroyProcess(g_process);
        g_process = nullptr;
    }
    g_output = nullptr;
    g_state = FetchState::Idle;
    g_line.clear();
    g_partial.clear();
}

}
