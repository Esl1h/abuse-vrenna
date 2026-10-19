#include <doctest/doctest.h>

#include <string>
#include <vector>

#include "data/relaunch.h"

using Args = std::vector<std::string>;

namespace {

Args relaunched(std::vector<char const *> const &argv)
{
    return abuse::data::relaunch_args((int)argv.size(), argv.data());
}

}

TEST_CASE("a plain launch is started again as it was") {
    CHECK(relaunched({"abuse", "-datadir", "./data", "-window"})
          == Args{"abuse", "-datadir", "./data", "-window"});
}

TEST_CASE("the mode on the command line is dropped with its value") {
    // It would win over the one the menu just saved, and the game would come
    // back in the mode the player had left.
    CHECK(relaunched({"abuse", "--mode", "original", "-window"})
          == Args{"abuse", "-window"});
    CHECK(relaunched({"abuse", "-window", "--mode", "remaster"})
          == Args{"abuse", "-window"});
}

TEST_CASE("the program name is never taken for a flag") {
    CHECK(relaunched({"--mode", "x"}) == Args{"--mode", "x"});
}

TEST_CASE("a trailing --mode with no value is dropped, not read past the end") {
    CHECK(relaunched({"abuse", "--mode"}) == Args{"abuse"});
}

TEST_CASE("no arguments at all is no arguments") {
    CHECK(relaunched({}).empty());
}
