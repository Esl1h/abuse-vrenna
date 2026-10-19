#!/usr/bin/env bash
# The simulation must not depend on how wide the window is.
#
# The levels were drawn for a 4:3 view, and what wakes up in one is decided
# by the region around the player, not by the window: a wider view shows more
# of the room and has to fight exactly the same fight. If that ever stops
# being true, widescreen becomes a different game and every reference hash
# recorded at 4:3 becomes a lie.
#
# Exits 77 (CTest's skip code) when there is no replay to run.
set -uo pipefail

bin=${1:?usage: test-aspect.sh <binary>}

shopt -s nullglob

# One replay by default, and the one with movement in it: four widths times
# every recording is four minutes of suite for coverage that barely differs,
# since a player standing still activates the same things wherever the edges
# of the screen are. ASPECT_ALL=1 runs the lot.
if [ -n "${ASPECT_ALL:-}" ]; then
    recs=(tests/replays/*.rec)
else
    recs=(tests/replays/level00-run.rec)
    [ -f "${recs[0]}" ] || recs=(tests/replays/*.rec)
fi

if [ ${#recs[@]} -eq 0 ]; then
    echo "no replays in tests/replays/, nothing to check"
    exit 77
fi

# 4:3, 16:10, 16:9 and 21:9 of a 200 pixel tall buffer.
widths=(320 400 427 560)

# One run of the game, bounded in time.
#
# Windows CI has twice killed this suite at its 300 second budget and
# reported one line, "Timeout", which says neither which replay was running
# nor whether the process hung or died behind a crash dialog. The healthy
# run takes about fifty seconds in total there, so a single invocation that
# passes two minutes is stuck, not slow.
#
# `timeout` comes with Git for Windows and with coreutils on Linux. Where it
# is missing, which is macOS without coreutils, the run is unbounded exactly
# as before: the bound is a diagnostic, not a requirement.
bound=""
if command -v timeout >/dev/null 2>&1; then
    bound=${ABUSE_RUN_TIMEOUT:-120}
fi

# A run that is killed is tried once more, and said so out loud.
#
# The stall that used to kill one run in a third of the Windows CI jobs was a
# wrong shift in time_marker::diff_time (see timing/filetime.h), fixed. This is
# kept for whatever else a shared runner can do to a process, and it prints
# every retry, on the stream the caller left alone, so one that keeps
# happening is seen and not absorbed.
exec 3>&2

run_game() {
    local rc
    if [ -z "$bound" ]; then
        "$@"
        return
    fi
    timeout "$bound" "$@"
    rc=$?
    if [ "$rc" -eq 124 ]; then
        echo "RETRY: killed after ${bound}s, trying once more: $*" >&3
        timeout "$bound" "$@"
        rc=$?
    fi
    return "$rc"
}

errors=$(mktemp)
trap 'rm -f "$errors"' EXIT

rc=0
for rec in "${recs[@]}"; do
    name=$(basename "$rec" .rec)
    seen=""
    for w in "${widths[@]}"; do
        # Timed and announced, one width at a time.
        #
        # This test has twice been killed by the CI timeout with nothing
        # to show for it: a single line saying the whole thing took too
        # long says neither which width was running nor whether the others
        # were already slow. On Windows it passes in 47 seconds and then
        # occasionally does not finish in 300.
        started=$(date +%s)
        echo "  ${w}x200..."

        # No set +e/-e around this: this script runs without -e (see the
        # line at the top), and turning it on here would make the empty
        # grep below end the run instead of reporting the width.
        # Kept rather than discarded: when one of these hangs on Windows,
        # what it managed to print before being killed is the only clue
        # anyone here will ever get about where it stopped.
        raw=$(run_game "$bin" --headless -nodelay --playback "$rec" \
              --state-hash --viewport "$w" 200 -datadir ./data 2>"$errors")
        status=$?
        hash=$(printf '%s\n' "$raw" | grep '^final' | grep -o 'hash=[0-9a-f]*')

        echo "  ${w}x200 took $(( $(date +%s) - started ))s"
        if [ "$status" -eq 124 ]; then
            echo "FAIL: $name at ${w}x200 was still running after ${bound}s"
            echo "  $bin --headless -nodelay --playback $rec --state-hash --viewport $w 200 -datadir ./data"
            printf '%s\n' "$raw" | tail -n 5 | sed 's/^/  out| /'
            tail -n 15 "$errors" | sed 's/^/  err| /'
            rc=1
            continue
        fi
        if [ -z "$hash" ]; then
            echo "FAIL: $name at ${w}x200 produced no hash"
            rc=1
            continue
        fi
        seen="$seen$w:$hash"$'\n'
    done

    different=$(printf '%s' "$seen" | cut -d: -f2- | sort -u | wc -l)
    if [ "$different" -eq 1 ]; then
        echo "ok: $name, same state at every width"
    else
        echo "FAIL: $name plays differently depending on the window"
        printf '%s' "$seen" | sed 's/^/  /'
        rc=1
    fi
done

exit $rc
