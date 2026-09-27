#!/usr/bin/env bash
# Installs the build into a staging directory and checks what landed.
#
# Every other suite runs from the source tree, where everything is always
# where the game left it. Installing is the step the packages take and the
# one nothing looked at, and it broke silently: data/CMakeLists.txt decided
# the original sound was present because the directory holding the *free*
# pack existed, went looking for music/intro.hmi, and failed. AppImage,
# tarball, .deb and .rpm would have failed with it.
#
#   ./scripts/check-install.sh <build dir>
set -euo pipefail

build=${1:?usage: check-install.sh <build dir>}
root=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

stage=$(mktemp -d)
trap 'rm -rf "$stage"' EXIT

DESTDIR="$stage" cmake --install "$build" --prefix /usr > "$stage/install.log" 2>&1 || {
    echo "install failed:" >&2
    tail -n 20 "$stage/install.log" >&2
    exit 1
}

data="$stage/usr/share/games/abuse"
fail=0

need() {
    if [ ! -e "$1" ]; then
        echo "missing after install: ${1#"$stage"}" >&2
        fail=1
    fi
}

need "$stage/usr/bin/abuse-vrenna"
need "$data/abuse.lsp"
need "$data/art"
need "$data/levels"

# The free pack. Counted rather than listed: the point is that the install
# carries it at all, and how many there are is check-audio-pack.sh's job.
sfx=$(find "$data/sfx" -name '*.ogg' 2>/dev/null | wc -l)
music=$(find "$data/music" -name '*.ogg' 2>/dev/null | wc -l)
[ "$sfx" -gt 0 ] || { echo "no sounds installed" >&2; fail=1; }
[ "$music" -gt 0 ] || { echo "no music installed" >&2; fail=1; }

# Nothing of Bobby Prince's, ever. The packages are redistributable and
# this is the line that says so.
if find "$data" \( -name '*.hmi' -o -name '*.wav' \) -print -quit | grep -q .; then
    echo "non-free audio in the install tree:" >&2
    find "$data" \( -name '*.hmi' -o -name '*.wav' \) | head -5 >&2
    fail=1
fi

if [ "$fail" -ne 0 ]; then
    exit 1
fi

# And it runs from there, which is what the packages actually do.
if ! ABUSE_PATH="$data" "$stage/usr/bin/abuse-vrenna" \
        --headless -nodelay --state-hash --max-ticks 5 \
        > "$stage/run.log" 2>&1; then
    echo "the installed game did not start:" >&2
    tail -n 10 "$stage/run.log" >&2
    exit 1
fi

echo "install ok: $sfx sounds, $music tracks, no non-free audio, and it starts"
