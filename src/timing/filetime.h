/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  A Windows FILETIME as a number. Kept apart from the one place that uses it
 *  so the arithmetic can be tested on every platform, because it is the kind
 *  of line that is wrong in a way no machine without Windows will show.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_TIMING_FILETIME_H_
#define ABUSE_TIMING_FILETIME_H_

#include <stdint.h>

namespace abuse::timing {

// A FILETIME is 100 ns ticks since 1601 in two 32-bit words. The shift has to
// happen on a 64-bit value: `dwHighDateTime << 32` on the DWORD itself shifts
// a 32-bit number by 32, which is undefined, and left the high word where it
// was. Every difference taken from it was then wrong by up to the whole of the
// low word, which wraps every 429.5 seconds, and a wait for ten seconds that
// straddled a wrap measured minus four hundred and went on for minutes. That
// is what stalled the Windows CI runner a third of the time.
inline uint64_t filetime_ticks(uint32_t high, uint32_t low)
{
    return ((uint64_t)high << 32) | low;
}

// Seconds from `then` to `now`. Signed, because a clock that is set back is
// still a clock.
inline double filetime_diff_seconds(uint64_t now, uint64_t then)
{
    return (double)((int64_t)now - (int64_t)then) / 10000000.0;
}

}

#endif
