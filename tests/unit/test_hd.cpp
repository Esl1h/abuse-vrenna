#include <doctest/doctest.h>

#include <filesystem>
#include <fstream>
#include <set>
#include <string>

#include "common.h"

#include "image.h"
#include "imlib/hd.h"
#include "palette.h"
#include "specs.h"

// The loader decodes against the game palette; loader2.cpp owns this in the
// game and nothing in this suite draws, so a null one is the whole of it.
palette *pal = NULL;
palette *old_pal = NULL;

// A pack with more than one picture for the same entry. The files are never
// decoded here: find() only looks at what is on disk, which is the part that
// decides which picture a run shows.
namespace {

std::string pack_root()
{
    static std::string root;
    if (!root.empty())
        return root;

    root = std::string(ABUSE_TEST_BUILD_DIR) + "/hdpack/";
    std::filesystem::create_directories(root + "hd/title");
    std::filesystem::create_directories(root + "hd/frame");

    auto touch = [](std::string const &path) {
        std::ofstream(path, std::ios::binary) << "not a png";
    };

    // Three of the title, one of the other, which is the shape of the pack
    // the repository ships.
    touch(root + "hd/title/title_screen.png");
    touch(root + "hd/title/title_screen.2.png");
    touch(root + "hd/title/title_screen.3.png");
    touch(root + "hd/frame/end_level_screen.png");

    set_filename_prefix(root.c_str());
    return root;
}

std::string found(char const *spe, char const *name)
{
    pack_root();
    char *p = abuse::hd::find(spe, name);
    std::string s(p ? p : "");
    free(p);
    return s;
}

}

TEST_CASE("without a seed the first picture is the one used") {
    abuse::hd::set_variant_seed(0);
    CHECK(found("art/title.spe", "title_screen") == "hd/title/title_screen.png");
}

TEST_CASE("an entry with one picture ignores the seed") {
    abuse::hd::set_variant_seed(12345);
    CHECK(found("art/frame.spe", "end_level_screen")
          == "hd/frame/end_level_screen.png");
}

TEST_CASE("a seeded run picks one of the three and keeps it") {
    abuse::hd::set_variant_seed(98765);

    std::string const first = found("art/title.spe", "title_screen");
    std::set<std::string> const allowed = {
        "hd/title/title_screen.png",
        "hd/title/title_screen.2.png",
        "hd/title/title_screen.3.png",
    };
    CHECK(allowed.count(first) == 1);

    // Asked again, as the cache does when it reloads the object. A title
    // screen that changed while the menu was open would read as a glitch.
    CHECK(found("art/title.spe", "title_screen") == first);
    CHECK(found("art/title.spe", "title_screen") == first);
}

TEST_CASE("a missing entry is still a miss") {
    abuse::hd::set_variant_seed(98765);
    CHECK(found("art/title.spe", "no_such_entry").empty());
}

// The brightness calibration keeps the palette it started from in old_pal and
// puts a corrected copy in pal. The screen shows indices through the corrected
// one, so a picture matched against that same palette is undone by it and
// comes out as painted, whatever the player calibrated.
TEST_CASE("art is matched against the palette from before the calibration") {
    std::string const data = std::string(ABUSE_TEST_DATA_DIR) + "/";
    set_filename_prefix(data.c_str());

    // A grey ramp, and the same ramp at a quarter of the brightness, which
    // stands for a calibration that darkened the screen.
    palette ramp, dark;
    for (int i = 0; i < 256; i++)
    {
        ramp.set(i, i, i, i);
        dark.set(i, i / 4, i / 4, i / 4);
    }

    char const *const path = "hd/frame/end_level_screen.png";
    int const w = 160, h = 198;

    auto indices = [&](palette *corrected, palette *original) {
        pal = corrected;
        old_pal = original;
        image *im = abuse::hd::load(path, w, h);
        REQUIRE(im != nullptr);
        std::string out;
        for (int y = 0; y < h; y++)
            out.append((char const *)im->scan_line((int16_t)y), (size_t)w);
        delete im;
        return out;
    };

    std::string const before_calibration = indices(&ramp, nullptr);
    std::string const after_calibration = indices(&dark, &ramp);
    std::string const matched_wrongly = indices(&dark, nullptr);

    pal = NULL;
    old_pal = NULL;

    // REQUIRE and not CHECK: common.h, which image.h needs ahead of it, takes
    // that name for the engine's own assertion. Compared as booleans so a
    // failure does not print the two pictures.
    bool const matched_as_before = after_calibration == before_calibration;
    bool const wrong_one_differs = matched_wrongly != before_calibration;
    REQUIRE(matched_as_before);
    REQUIRE(wrong_one_differs);
}
