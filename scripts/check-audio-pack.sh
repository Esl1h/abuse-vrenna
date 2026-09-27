#!/usr/bin/env bash
# Checks that the free pack answers every sound and every track the game
# asks for.
#
# A missing sound is the quietest possible failure: the game starts, plays
# on, and one event is silent for the rest of the project's life. The names
# live in data/lisp/sfx.lsp and in game.cpp, the files live in data/sfx and
# data/music, and nothing until now compared the two lists.
#
# Only the Remastered pack is checked. The Original mode plays Bobby
# Prince's files, which are not here and are not supposed to be.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

missing=0

# Sound effects: every sfxdir("...") the Lisp asks for, minus the ones it
# has commented out.
while read -r name; do
    [ -n "$name" ] || continue
    if [ ! -f "data/sfx/${name%.wav}.ogg" ]; then
        echo "missing: data/sfx/${name%.wav}.ogg" >&2
        missing=$((missing + 1))
    fi
done < <(grep -v '^[[:space:]]*;' data/lisp/sfx.lsp \
         | grep -o 'sfxdir "[^"]*"' \
         | sed 's/sfxdir "//; s/"//' \
         | sort -u)

# Music: what the Lisp plays, plus the title track, which game.cpp asks for
# by name.
while read -r name; do
    [ -n "$name" ] || continue
    if [ ! -f "data/${name%.hmi}.ogg" ]; then
        echo "missing: data/${name%.hmi}.ogg" >&2
        missing=$((missing + 1))
    fi
done < <({ grep -v '^[[:space:]]*;' data/lisp/sfx.lsp | grep -o 'music/[a-z0-9_]*\.hmi'
           grep -ho 'music/[a-z0-9_]*\.hmi' src/game.cpp; } | sort -u)

if [ "$missing" -gt 0 ]; then
    echo "$missing file(s) the game asks for are not in the pack" >&2
    echo "rebuild it: ./tools/free-sfx/build-pack.py and build-music.py" >&2
    exit 1
fi

sfx=$(find data/sfx -name '*.ogg' | wc -l)
music=$(find data/music -name '*.ogg' | wc -l)
echo "audio pack ok: $sfx sounds, $music tracks, nothing the game asks for is missing"
