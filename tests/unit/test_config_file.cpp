#include <doctest/doctest.h>

#include <stdio.h>

#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>

#include "data/config_file.h"

using abuse::data::save_config_key;
using abuse::data::set_config_key;

TEST_CASE("an existing assignment is replaced where it stands") {
    std::string in =
        "; comentario\n"
        "deadzone=8192\n"
        "language=en\n"
        "rumble=100\n";

    std::string out = set_config_key(in, "language", "pt_BR");

    CHECK(out ==
        "; comentario\n"
        "deadzone=8192\n"
        "language=pt_BR\n"
        "rumble=100\n");
}

TEST_CASE("a key that is not there is appended") {
    std::string out = set_config_key("deadzone=8192\n", "language", "fr");
    CHECK(out == "deadzone=8192\nlanguage=fr\n");
}

TEST_CASE("an empty file gets the single line") {
    CHECK(set_config_key("", "rumble", "50") == "rumble=50\n");
}

TEST_CASE("a file with no trailing newline still ends with one") {
    CHECK(set_config_key("deadzone=8192", "rumble", "50")
          == "deadzone=8192\nrumble=50\n");
}

// createRCFile writes the keys it documents as commented examples. Taking one
// over is what keeps the file from growing a second copy below the comment
// that explains it.
TEST_CASE("a commented example is taken over instead of duplicated") {
    std::string in =
        "; Language of the in-game text\n"
        ";language=en\n"
        "\n"
        "deadzone=8192\n";

    std::string out = set_config_key(in, "language", "de");

    CHECK(out ==
        "; Language of the in-game text\n"
        "language=de\n"
        "\n"
        "deadzone=8192\n");
}

// A real assignment further down is the one the game reads, so that is the one
// that has to change.
TEST_CASE("a real assignment wins over a commented example") {
    std::string in =
        ";language=en\n"
        "language=fr\n";

    std::string out = set_config_key(in, "language", "de");
    CHECK(out == "language=de\n");
}

// The claimed comment is dropped by where it was written. Looking for its text
// instead found the last line that merely contained it, which here is
// foo_mode=remaster, and glued "foo_" onto the next line.
TEST_CASE("taking over a comment leaves the lines around it alone") {
    std::string in =
        "; mode=remaster\n"
        "foo_mode=remaster\n"
        "bar=1\n"
        "mode=original\n";

    std::string out = set_config_key(in, "mode", "remaster");
    CHECK(out ==
        "foo_mode=remaster\n"
        "bar=1\n"
        "mode=remaster\n");
}

TEST_CASE("only the first of several assignments is rewritten") {
    std::string in = "rumble=100\nrumble=50\n";
    CHECK(set_config_key(in, "rumble", "0") == "rumble=0\nrumble=50\n");
}

// The file is hand edited, and a setting changed in a menu must not cost the
// player their comments or their spacing.
TEST_CASE("everything else is preserved byte for byte") {
    std::string in =
        "; primeiro\n"
        "\n"
        "   ; indentado\n"
        "bind=left,key,A\n"
        "bind=right,key,D\n"
        "rumble=100\n"
        "\n"
        "; ultimo\n";

    std::string out = set_config_key(in, "rumble", "25");
    CHECK(out ==
        "; primeiro\n"
        "\n"
        "   ; indentado\n"
        "bind=left,key,A\n"
        "bind=right,key,D\n"
        "rumble=25\n"
        "\n"
        "; ultimo\n");
}

TEST_CASE("the key is matched whole, not as a prefix") {
    std::string in = "deadzoneouter=30000\ndeadzone=8192\n";
    CHECK(set_config_key(in, "deadzone", "4096")
          == "deadzoneouter=30000\ndeadzone=4096\n");
}

TEST_CASE("the key is matched without regard to case") {
    CHECK(set_config_key("RUMBLE=100\n", "rumble", "0") == "rumble=0\n");
}

TEST_CASE("leading spaces before a key are tolerated") {
    CHECK(set_config_key("  rumble=100\n", "rumble", "0") == "rumble=0\n");
}

TEST_CASE("an empty key changes nothing") {
    CHECK(set_config_key("rumble=100\n", "", "0") == "rumble=100\n");
}

TEST_CASE("an empty value is written as one") {
    CHECK(set_config_key("rumble=100\n", "rumble", "") == "rumble=\n");
}

using abuse::data::set_config_lines;

TEST_CASE("a repeating key is replaced as a whole block") {
    std::string in =
        "deadzone=8192\n"
        "bind=left,key,A\n"
        "bind=right,key,D\n"
        "rumble=100\n";

    std::string out = set_config_lines(in, "bind",
        { "left,key,LEFT", "right,key,RIGHT", "fire,pad,righttrigger+" });

    CHECK(out ==
        "deadzone=8192\n"
        "bind=left,key,LEFT\n"
        "bind=right,key,RIGHT\n"
        "bind=fire,pad,righttrigger+\n"
        "rumble=100\n");
}

// A binding the player removed has to disappear from the file, not linger
// below the new block.
TEST_CASE("fewer lines than before leaves no leftovers") {
    std::string in =
        "bind=left,key,A\n"
        "bind=left,key,B\n"
        "bind=left,key,C\n";

    CHECK(set_config_lines(in, "bind", { "left,key,A" }) == "bind=left,key,A\n");
}

TEST_CASE("an empty set removes the key entirely") {
    std::string in = "rumble=100\nbind=left,key,A\n";
    CHECK(set_config_lines(in, "bind", {}) == "rumble=100\n");
}

TEST_CASE("a key that was not there is appended") {
    CHECK(set_config_lines("rumble=100\n", "bind", { "left,key,A" })
          == "rumble=100\nbind=left,key,A\n");
}

// For a repeating key a commented line is the syntax documentation that
// createRCFile writes, not a disabled setting, so it stays.
TEST_CASE("commented lines are left alone") {
    std::string in =
        "; bind=<action>,key,<key name>\n"
        ";bind=fire,key,Space\n"
        "bind=fire,key,Space\n"
        "rumble=100\n";

    CHECK(set_config_lines(in, "bind", { "fire,key,X" }) ==
        "; bind=<action>,key,<key name>\n"
        ";bind=fire,key,Space\n"
        "bind=fire,key,X\n"
        "rumble=100\n");
}

TEST_CASE("the rest of the file is preserved") {
    std::string in =
        "; comentario\n"
        "\n"
        "bind=left,key,A\n"
        "\n"
        "; fim\n";

    CHECK(set_config_lines(in, "bind", { "left,key,B" }) ==
        "; comentario\n"
        "\n"
        "bind=left,key,B\n"
        "\n"
        "; fim\n");
}

namespace {

std::filesystem::path scratch(char const *name)
{
    std::filesystem::path dir = std::filesystem::temp_directory_path() / "abuse-config-test";
    std::filesystem::create_directories(dir);
    return dir / name;
}

std::string slurp(std::filesystem::path const &p)
{
    std::ifstream in(p, std::ios::binary);
    std::stringstream ss;
    ss << in.rdbuf();
    return ss.str();
}

}

TEST_CASE("saving writes the file and leaves no temporary behind") {
    auto path = scratch("abuserc");
    std::filesystem::remove(path);

    CHECK(save_config_key(path.string().c_str(), "mode", "original"));
    CHECK(slurp(path) == "mode=original\n");
    CHECK(!std::filesystem::exists(path.string() + ".tmp"));
}

TEST_CASE("a failed save keeps the old file whole") {
    auto path = scratch("kept");
    {
        std::ofstream out(path, std::ios::binary);
        out << "left=a\nright=d\n";
    }
    // The temporary's name is taken by a directory, so the write cannot start.
    std::filesystem::create_directories(path.string() + ".tmp");

    CHECK(!save_config_key(path.string().c_str(), "left", "b"));
    CHECK(slurp(path) == "left=a\nright=d\n");

    std::filesystem::remove_all(path.string() + ".tmp");
}

TEST_CASE("a file that is there and cannot be read is not overwritten") {
    // A directory in the file's place: it exists, and reading it fails.
    auto path = scratch("unreadable");
    std::filesystem::remove_all(path);
    std::filesystem::create_directories(path);

    CHECK(!save_config_key(path.string().c_str(), "mode", "original"));
    CHECK(std::filesystem::is_directory(path));

    std::filesystem::remove_all(path);
}
