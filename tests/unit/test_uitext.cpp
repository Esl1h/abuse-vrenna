#include <doctest/doctest.h>

#include <initializer_list>
#include <string.h>

#include "i18n/language.h"
#include "i18n/uitext.h"

using namespace abuse::i18n;

namespace {

// Every test here changes the global language, so each one puts it back.
struct LanguageGuard
{
    Language saved = language();
    ~LanguageGuard() { language() = saved; }
};

}

TEST_CASE("English is what a language with no column of its own gets") {
    LanguageGuard guard;
    for (Language l : { Language::English, Language::French, Language::German })
    {
        language() = l;
        CHECK(strcmp(say(kOptionsTitle), "Options") == 0);
    }
}

TEST_CASE("Portuguese gets its own column") {
    LanguageGuard guard;
    language() = Language::Portuguese;
    CHECK(strcmp(say(kOptionsTitle), "Op\xe7\xf5" "es") == 0);
    CHECK(strcmp(say(kControlsTitle), "Controles") == 0);
}

// The phrases are Latin-1, the encoding the overlay draws in. A UTF-8 accent
// would be two bytes and would come out as two wrong glyphs.
TEST_CASE("the Portuguese column is Latin-1, not UTF-8") {
    LanguageGuard guard;
    language() = Language::Portuguese;

    char const *s = say(kOptRumble);          // "Vibracao" with a tilde and a cedilla
    CHECK(strchr(s, '\xe7') != nullptr);
    CHECK(strchr(s, '\xe3') != nullptr);
    // No UTF-8 lead byte anywhere in it.
    CHECK(strchr(s, '\xc3') == nullptr);
}

TEST_CASE("a phrase with no translation falls back rather than showing nothing") {
    LanguageGuard guard;
    language() = Language::Portuguese;

    Phrase untranslated = { "Only English", nullptr };
    CHECK(strcmp(say(untranslated), "Only English") == 0);
}

// The pseudo language is there to find text that does not fit. Text written in
// C++ has to go through it too, or half the UI escapes the check.
TEST_CASE("the pseudo language lengthens the catalogue too") {
    LanguageGuard guard;

    language() = Language::English;
    size_t plain = strlen(say(kOptionsTitle));

    language() = Language::Pseudo;
    size_t pseudo = strlen(say(kOptionsTitle));

    CHECK(pseudo > plain);
}

TEST_CASE("every phrase has both columns filled") {
    Phrase const all[] = {
        kOptionsTitle, kOptionsHelp, kRestartNote, kSaved, kNotSaved, kOn, kOff,
        kOptLanguage, kOptFont, kOptScaleMode, kOptFilter, kOptVsync,
        kOptFpsLimit, kOptDeadzone, kOptAimRadius, kOptAimAssist, kOptRumble,
        kOptCursorSpeed, kControlsTitle, kControlsHelp, kPressAny, kUnbound,
        kNoRoomForMore, kActMoveLeft, kActMoveRight, kActUp, kActDown, kActFire,
        kActSpecial, kActWeaponPrev, kActWeaponNext, kPadLost, kPadLostHelp,
        kMenuHint, kStartOriginalSound, kClassicTitleOptional,
    };

    for (Phrase const &p : all)
    {
        REQUIRE(p.en != nullptr);
        REQUIRE(p.pt != nullptr);
        CHECK(p.en[0] != 0);
        CHECK(p.pt[0] != 0);
    }
}

// The whole file has to stay pure ASCII source: the accents are escapes so no
// editor can re-encode them. This checks the result, which is what matters.
TEST_CASE("no phrase carries a byte the Latin-1 font cannot draw") {
    Phrase const all[] = { kOptionsTitle, kOptRumble, kOptAimAssist,
                           kPadLostHelp, kMenuHint, kActWeaponNext };

    for (Phrase const &p : all)
        for (char const *c = p.pt; *c; c++)
            REQUIRE((unsigned char)*c != 0xc3);
}
