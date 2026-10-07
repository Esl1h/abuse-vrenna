#!/usr/bin/env bash
# Downloads the original Abuse data and installs it where the Original mode
# looks for it.
#
# Sound effects and music only: levels, art and Lisp are already in the
# repository and are public domain.
#
# Nothing is written to the destination before the SHA-256 checks out, and the
# write is atomic: it extracts into a temporary directory and renames at the
# end.
set -euo pipefail

here=$(cd -- "$(dirname -- "$0")" && pwd)
base=${ABUSE_CLASSIC_URL:-http://abuse.zoy.org/raw-attachment/wiki/download}

dest=${1:-${XDG_DATA_HOME:-$HOME/.local/share}/abuse/classic}

# Without a trailing slash, whatever was passed.
#
# The atomic write at the end moves "$dest" to "$dest.previous.$$", and with
# a slash on the end that name lands *inside* the directory being moved:
# mv refuses, "cannot move to a subdirectory of itself", after everything
# has already been downloaded and checked. The game passes the path it uses
# as a prefix elsewhere, where the slash belongs, and that is how this was
# found.
while [ "$dest" != "/" ] && [ "${dest%/}" != "$dest" ]; do
    dest=${dest%/}
done

# The checksums are looked for in the three places this script is ever run
# from, in order: a checkout, an installed tree, and a directory where the
# game and its data sit together.
#
# It used to be only the first, "$here/../data/classic.sha256", which works
# in a clone and in nothing else. Installed as /usr/bin/abuse-vrenna-fetch-
# classic-data the script asked for /usr/data/classic.sha256 and refused to
# do anything, which made the Original mode unreachable from every package.
#
# ABUSE_CLASSIC_SUMS overrides the search outright.
sums=${ABUSE_CLASSIC_SUMS:-}
if [ -z "$sums" ]; then
    for candidate in \
        "$here/../data/classic.sha256" \
        "$here/../share/games/abuse/classic.sha256" \
        "$here/classic.sha256"
    do
        if [ -f "$candidate" ]; then
            sums=$candidate
            break
        fi
    done
fi

if [ -z "$sums" ] || [ ! -f "$sums" ]; then
    echo "cannot find classic.sha256 next to $here" >&2
    echo "point ABUSE_CLASSIC_SUMS at it" >&2
    exit 1
fi
command -v curl > /dev/null || { echo "curl not found" >&2; exit 1; }
command -v sha256sum > /dev/null || { echo "sha256sum not found" >&2; exit 1; }

tmp=$(mktemp -d)
trap 'rm -rf "$tmp"' EXIT

echo "destination: $dest"

while read -r hash file; do
    [ -n "$file" ] || continue
    echo "downloading $file"
    if ! curl -fsSL -o "$tmp/$file" "$base/$file"; then
        echo "FAIL: could not download $file from $base" >&2
        exit 1
    fi
    if ! printf '%s  %s\n' "$hash" "$tmp/$file" | sha256sum -c --status -; then
        echo "FAIL: SHA-256 of $file does not match; nothing was written" >&2
        echo "  expected: $hash" >&2
        echo "  got:      $(sha256sum "$tmp/$file" | cut -d' ' -f1)" >&2
        exit 1
    fi
    echo "  hash ok"
done < <(grep -v '^[[:space:]]*#' "$sums" | grep -v '^[[:space:]]*$')

# Extracted with no transformation at all: the files go in as the tarball has them.
stage="$tmp/stage"
mkdir -p "$stage/sfx" "$stage/music"

tar xzf "$tmp/abuse-sfx-2.00.tar.gz" -C "$tmp"
find "$tmp" -path "$stage" -prune -o -name '*.wav' -print0 |
    xargs -0 -r -I{} cp -n {} "$stage/sfx/"

tar xzf "$tmp/abuse-data-2.00.tar.gz" -C "$tmp"
find "$tmp" -path "$stage" -prune -o -name '*.hmi' -print0 |
    xargs -0 -r -I{} cp -n {} "$stage/music/"

wavs=$(find "$stage/sfx" -name '*.wav' | wc -l)
hmis=$(find "$stage/music" -name '*.hmi' | wc -l)
echo "extracted: $wavs effects, $hmis music tracks"

[ "$wavs" -gt 0 ] || { echo "FAIL: no sound effect extracted" >&2; exit 1; }

# Atomic write: the destination only ever exists complete.
mkdir -p "$(dirname "$dest")"
if [ -e "$dest" ]; then
    previous="$dest.previous.$$"
    mv "$dest" "$previous"
    mv "$stage" "$dest"
    rm -rf "$previous"
else
    mv "$stage" "$dest"
fi

echo "done: $dest"
echo "run with: --mode original --classic-data $dest"
