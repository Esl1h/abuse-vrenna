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

# Linux only, and it exits 77, CTest's skip code, anywhere else.
#
# What it checks is the Linux install layout: usr/bin and usr/share/games.
# Windows installs everything into one directory and macOS builds a bundle,
# so the same assertions are false there for reasons that have nothing to do
# with what this is watching for. The first version of this file did not say
# so and turned both of those jobs red.
case "$(uname -s)" in
    Linux) ;;
    *)
        echo "not Linux: the install layout this checks is the one the"
        echo "AppImage, the tarball, the .deb and the .rpm are built from"
        exit 77
        ;;
esac

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

# And it brings its sound up from there. --headless above turns the sound
# off, so this is the only place anything looks. The prefix baked into the
# binary is the one it was configured with, which is not where an AppImage
# keeps its data: ABUSE_PATH is all that says where sfx/ is, and it once
# arrived after sound_init had already looked, so the AppImage shipped with
# a free pack it never played.
#
# Not headless, then, with SDL's dummy drivers standing in for a screen and
# a sound card. The game sits at its first screen and never exits, so the
# log is polled for the line and the process is killed.
#
# -datadir names a directory that does not exist, so the prefix is wrong on
# every machine and not only on the ones where the baked-in one happens to
# be empty. ABUSE_PATH has to win over it.
home="$stage/home"
mkdir -p "$home"
HOME="$home" XDG_CONFIG_HOME="$home/config" XDG_DATA_HOME="$home/data" \
    SDL_VIDEODRIVER=dummy SDL_AUDIODRIVER=dummy ABUSE_PATH="$data" \
    "$stage/usr/bin/abuse-vrenna" -window -language en \
    -datadir "$stage/no-such-directory" \
    > "$stage/sound.log" 2>&1 &
pid=$!
for _ in $(seq 1 150); do
    grep -aq 'Sound: ' "$stage/sound.log" && break
    sleep 0.2
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true

if ! grep -aq 'Sound: Enabled' "$stage/sound.log"; then
    echo "the installed game did not bring its sound up:" >&2
    grep -a 'Sound' "$stage/sound.log" | head -5 >&2
    exit 1
fi

echo "install ok: $sfx sounds, $music tracks, no non-free audio, it starts and plays"
