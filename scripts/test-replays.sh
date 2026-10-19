#!/usr/bin/env bash
# Replays every tests/replays/*.rec and compares the state hash with the
# reference in tests/golden/hash/. Exits 77 (CTest's skip code) when there is
# nothing to replay.
set -euo pipefail

bin=${1:?usage: test-replays.sh <binary>}
mode=${2:-original}

shopt -s nullglob
recs=(tests/replays/*.rec)
if [ ${#recs[@]} -eq 0 ]; then
    echo "no replays in tests/replays/, nothing to check"
    exit 77
fi

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
    golden="tests/golden/hash/$name.$mode.hash"

    set +e
    raw=$(run_game "$bin" --headless -nodelay --playback "$rec" --state-hash -datadir ./data 2>"$errors")
    status=$?
    set -e
    out=$(printf '%s\n' "$raw" | grep '^final' || true)

    if [ "$status" -eq 124 ]; then
        echo "FAIL: $name ($mode) was still running after ${bound}s and was killed"
        echo "  $bin --headless -nodelay --playback $rec --state-hash -datadir ./data"
        tail -n 10 "$errors" | sed 's/^/  err| /'
        rc=1
        continue
    fi

    if [ -z "$out" ]; then
        # Say why. A suite that hides the reason for failing costs more time
        # than it saves, and this one first failed on a platform none of us
        # can run it on.
        echo "FAIL: $name ($mode) produced no hash (exit $status)"
        echo "  $bin --headless -nodelay --playback $rec --state-hash -datadir ./data"
        printf '%s\n' "$raw" | tail -n 5 | sed 's/^/  out| /'
        tail -n 10 "$errors" | sed 's/^/  err| /'
        rc=1
        continue
    fi

    if [ ! -f "$golden" ]; then
        echo "FAIL: $name ($mode) has no reference in $golden"
        echo "      run scripts/update-golden.sh to create it"
        rc=1
        continue
    fi

    if [ "$out" != "$(cat "$golden")" ]; then
        echo "FAIL: $name ($mode)"
        echo "  expected: $(cat "$golden")"
        echo "  got:      $out"
        rc=1
    else
        echo "ok: $name ($mode)"
    fi
done

exit "$rc"
