#include <doctest/doctest.h>

#include <stdint.h>

#include "timing/filetime.h"

using abuse::timing::filetime_diff_seconds;
using abuse::timing::filetime_ticks;

TEST_CASE("the two words of a FILETIME are one 64-bit count") {
    CHECK(filetime_ticks(0, 5) == 5u);
    CHECK(filetime_ticks(1, 0) == 4294967296ull);
    CHECK(filetime_ticks(0x01DC, 5) == 0x01DC00000005ull);
}

TEST_CASE("ten million ticks are a second") {
    CHECK(filetime_diff_seconds(10000000, 0) == doctest::Approx(1.0));
    CHECK(filetime_diff_seconds(0, 10000000) == doctest::Approx(-1.0));
}

TEST_CASE("the low word wrapping is not a jump in time") {
    // Thirty-two ticks apart, across the wrap of the low word.
    uint64_t const before = filetime_ticks(5, 0xFFFFFFF0u);
    uint64_t const after = filetime_ticks(6, 0x10u);

    CHECK(filetime_diff_seconds(after, before) == doctest::Approx(3.2e-6));
}

TEST_CASE("a ten second wait that straddles the wrap measures ten seconds") {
    // What the start menu does before it plays a demo, and what stalled when
    // the high word was never shifted.
    uint64_t const then = filetime_ticks(0x01DC, 0xFFFFFFFFu - 50000000u);
    uint64_t const now = then + 100000000ull;

    // Round-tripped through the two words, as GetSystemTimeAsFileTime hands
    // them over.
    uint64_t const again = filetime_ticks((uint32_t)(now >> 32), (uint32_t)now);
    REQUIRE(again == now);

    CHECK(filetime_diff_seconds(again, then) == doctest::Approx(10.0));
    CHECK(filetime_diff_seconds(again, then) > 10.0 - 1e-6);
}
