/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  Unit tests for the sound file lookup (phase 5, task 5.1).
 *
 *  This software was released into the Public Domain.
 */

#include <doctest/doctest.h>

#include "audio/formats.h"

using abuse::audio::sound_candidates;
using abuse::audio::sound_extensions;

TEST_CASE("the requested name is always tried first") {
    auto c = sound_candidates("sfx/laser.wav", true);
    REQUIRE(c.size() > 1);
    CHECK(c[0] == "sfx/laser.wav");
}

TEST_CASE("the Original mode gets no substitution") {
    auto c = sound_candidates("sfx/laser.wav", false);
    REQUIRE(c.size() == 1);
    CHECK(c[0] == "sfx/laser.wav");
}

TEST_CASE("a wav request offers the compressed formats") {
    auto c = sound_candidates("sfx/laser.wav", true);
    CHECK(c.size() == 3);
    CHECK(c[1] == "sfx/laser.ogg");
    CHECK(c[2] == "sfx/laser.flac");

    // And never the extension that was asked for, which would be the same
    // miss twice.
    for (size_t i = 1; i < c.size(); i++)
        CHECK(c[i] != "sfx/laser.wav");
}

TEST_CASE("an hmi request offers the free soundtrack") {
    // The music goes through the same rule as the effects, and the name it
    // asks for is the 1995 MIDI one: song::song takes music/abuse01.ogg when
    // it is there, because the MIDI itself is not redistributable and is in
    // no package here.
    auto c = sound_candidates("music/abuse01.hmi", true);
    CHECK(c.size() == 4);
    CHECK(c[0] == "music/abuse01.hmi");
    CHECK(c[1] == "music/abuse01.ogg");
    CHECK(c[2] == "music/abuse01.flac");
    CHECK(c[3] == "music/abuse01.wav");
}

TEST_CASE("the Original mode gets no soundtrack substitution either") {
    auto c = sound_candidates("music/abuse01.hmi", false);
    CHECK(c.size() == 1);
    CHECK(c[0] == "music/abuse01.hmi");
}

TEST_CASE("the extension is matched whatever its case") {
    auto c = sound_candidates("SFX/LASER.WAV", true);
    CHECK(c.size() == 3);
    for (auto const &name : c)
        CHECK(name != "SFX/LASER.wav");
}

TEST_CASE("a request for one of the others works the same way") {
    auto c = sound_candidates("pack/hit.ogg", true);
    REQUIRE(c.size() == 3);
    CHECK(c[0] == "pack/hit.ogg");
    CHECK(c[1] == "pack/hit.flac");
    CHECK(c[2] == "pack/hit.wav");
}

TEST_CASE("a name with no extension is left alone") {
    auto c = sound_candidates("sfx/laser", true);
    REQUIRE(c.size() == 1);
    CHECK(c[0] == "sfx/laser");
}

TEST_CASE("a dot in a directory is not an extension") {
    auto c = sound_candidates("a.d/laser", true);
    REQUIRE(c.size() == 1);
    CHECK(c[0] == "a.d/laser");

    // But one after it still is.
    auto d = sound_candidates("a.d/laser.wav", true);
    CHECK(d.size() == 3);
    CHECK(d[1] == "a.d/laser.ogg");
}

TEST_CASE("an empty name asks for nothing") {
    CHECK(sound_candidates("", true).empty());
    CHECK(sound_candidates("", false).empty());
}

TEST_CASE("every extension offered is one the mixer decodes") {
    // Not a test of the mixer, a test of the list: SDL3_mixer is built here
    // with WAV, VORBIS, STBVORBIS, FLAC, DRFLAC, VOC, AIFF, AU and DRMP3, so
    // anything added to this list has to stay inside that set.
    for (char const *e : sound_extensions())
    {
        std::string const ext(e);
        CHECK((ext == ".ogg" || ext == ".flac" || ext == ".wav"));
    }
}
