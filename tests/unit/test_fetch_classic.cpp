#include <doctest/doctest.h>

#include <chrono>
#include <fstream>
#include <string>
#include <thread>

#include <SDL3/SDL.h>

#include "data/fetch_classic.h"

// Starting the fetcher and reading what it says, with a script of our own:
// the real one downloads ten megabytes from a server, which is not something
// a test suite should do, and what is worth testing here is the plumbing
// around it rather than the download.
namespace {

using abuse::data::FetchState;

std::string write_script(char const *name, char const *body)
{
    std::string const path = std::string(ABUSE_TEST_BUILD_DIR) + "/" + name;
    {
        std::ofstream out(path);
        out << "#!/bin/sh\n" << body;
    }
    // 0755, because the point is that it runs.
    SDL_SetEnvironmentVariable(SDL_GetEnvironment(), "ABUSE_FETCH_SCRIPT",
                               path.c_str(), true);
    std::string const chmod = "chmod 755 '" + path + "'";
    (void)!system(chmod.c_str());
    return path;
}

// Runs it to the end, or gives up. The loop is the one the screen runs,
// minus the drawing.
FetchState run_to_end()
{
    for (int i = 0; i < 400; i++)
    {
        FetchState const state = abuse::data::fetch_poll();
        if (state != FetchState::Running)
            return state;
        std::this_thread::sleep_for(std::chrono::milliseconds(10));
    }
    return FetchState::Running;
}

}

TEST_CASE("the destination carries no trailing slash") {
    // classic_data_dir() ends in one, because elsewhere it is a prefix and
    // the slash belongs. As an argument it is poison: the script renames
    // "$dest" to "$dest.previous.$$" for its atomic write, and with the
    // slash that name is inside the directory being moved. mv refuses,
    // after the download and the checksums have already passed.
    std::string const dest = abuse::data::fetch_destination();
    REQUIRE(!dest.empty());
    CHECK(dest.back() != '/');
}

TEST_CASE("the script is found through the override") {
    std::string const path = write_script("fake-fetch.sh", "exit 0\n");
    CHECK(abuse::data::fetch_script() == path);
    abuse::data::fetch_reset();
}

TEST_CASE("what the script prints comes back, and it ends") {
    write_script("fake-fetch-talks.sh",
                 "echo 'downloading abuse-sfx-2.00.tar.gz'\n"
                 "echo '  hash ok'\n"
                 "exit 0\n");

    REQUIRE(abuse::data::fetch_start());
    FetchState const state = run_to_end();

    // Failed and not Done: the script wrote no data, and fetch_poll asks the
    // filesystem as well as the exit code. What matters here is that it
    // finished at all and that its last line arrived.
    CHECK(state != FetchState::Running);
    CHECK(std::string(abuse::data::fetch_message()) == "  hash ok");
    abuse::data::fetch_reset();
}

TEST_CASE("a script that fails is a failure") {
    write_script("fake-fetch-fails.sh",
                 "echo 'FAIL: could not download' >&2\n"
                 "exit 1\n");

    REQUIRE(abuse::data::fetch_start());
    CHECK(run_to_end() == FetchState::Failed);
    abuse::data::fetch_reset();
}

TEST_CASE("reset leaves nothing behind") {
    write_script("fake-fetch-slow.sh", "sleep 5\n");

    REQUIRE(abuse::data::fetch_start());
    CHECK(abuse::data::fetch_poll() == FetchState::Running);

    // What Esc does on the screen: the process is killed and the state is
    // ready to offer the choice again.
    abuse::data::fetch_reset();
    CHECK(abuse::data::fetch_poll() == FetchState::Idle);
    CHECK(std::string(abuse::data::fetch_message()).empty());
}
