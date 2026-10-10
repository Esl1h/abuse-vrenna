#!/usr/bin/env bash
#
# Builds an AppImage of Abuse: Vrenna with linuxdeploy.
#
# linuxdeploy and its appimage plugin are downloaded, which means this script
# needs the network and does not belong in the offline build.
#
#   ./packaging/appimage/build-appimage.sh
#
# Output: Abuse_Vrenna-<version>-x86_64.AppImage in the current directory.
#
# Two things about linuxdeploy bit on 2026-10-10, once current distributions
# started linking their libraries with DT_RELR (a .relr.dyn section):
#
# - Its strip is too old for it and fails the whole run, so NO_STRIP is set
#   below. The bundled libraries are a little larger and nothing else.
# - Its patchelf, 0.15, rewrites those libraries into ones that segfault in
#   their constructors, before main, so the AppImage builds, looks fine and
#   does not start. Extract linuxdeploy (--appimage-extract) and replace
#   usr/bin/patchelf with a current one; 0.19.2 is the one that was tried.
#   The check at the end of this script is what notices.

set -euo pipefail

here=$(cd -- "$(dirname -- "$0")" && pwd)
root=$(cd -- "$here/../.." && pwd)

app_id=io.github.Esl1h.AbuseVrenna
build=$root/build/appimage
appdir=$build/AppDir

# Named after the version the game prints, and not after the desktop entry:
# linuxdeploy would call it "Abuse:_Vrenna-x86_64.AppImage", and a colon in a
# file name is trouble in a URL, on Windows and in a shell.
version=$(sed -n 's/^set(abuse_VERSION \(.*\))$/\1/p' "$root/CMakeLists.txt")
export OUTPUT="Abuse_Vrenna-${version}-x86_64.AppImage"

command -v linuxdeploy-x86_64.AppImage >/dev/null 2>&1 || {
    echo "linuxdeploy-x86_64.AppImage is not on PATH." >&2
    echo "Get it from https://github.com/linuxdeploy/linuxdeploy/releases" >&2
    exit 1
}

# See the header: linuxdeploy's strip cannot read current libraries.
export NO_STRIP=${NO_STRIP:-1}

rm -rf "$appdir"

cmake -S "$root" -B "$build" -G Ninja \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=/usr \
    -DCPM_USE_LOCAL_PACKAGES=ON
cmake --build "$build"
DESTDIR="$appdir" cmake --install "$build"

install -Dm755 "$root/scripts/fetch-classic-data.sh" \
    "$appdir/usr/bin/abuse-vrenna-fetch-classic-data"

# --icon-filename, because the name of the file on disk has to match the
# Icon= key of the desktop entry and the source is not called that.
linuxdeploy-x86_64.AppImage \
    --appdir "$appdir" \
    --executable "$appdir/usr/bin/abuse-vrenna" \
    --custom-apprun "$here/AppRun" \
    --desktop-file "$root/packaging/flatpak/$app_id.desktop" \
    --icon-file "$root/data/freedesktop/icons/hicolor/256x256/apps/$app_id.png" \
    --icon-filename "$app_id" \
    --output appimage

# Start what was just written, the way a person would, on SDL's dummy drivers
# and a prefix that does not exist, which is what AppRun has to put right.
# Nothing else here would notice an AppImage that does not start, or one that
# starts and plays no sound: both have shipped from this script. The game
# never exits by itself, so the log is polled for the line and it is killed.
check=$(mktemp -d)
trap 'rm -rf "$check"' EXIT
(cd "$check" && "$OLDPWD/$OUTPUT" --appimage-extract > /dev/null)
mkdir "$check/home"
HOME="$check/home" XDG_CONFIG_HOME="$check/home/config" \
    XDG_DATA_HOME="$check/home/data" SDL_VIDEODRIVER=dummy \
    SDL_AUDIODRIVER=dummy "$check/squashfs-root/AppRun" -window \
    -language en -datadir "$check/no-such-directory" \
    > "$check/run.log" 2>&1 &
pid=$!
for _ in $(seq 1 150); do
    grep -aq 'Sound: ' "$check/run.log" && break
    kill -0 "$pid" 2>/dev/null || break
    sleep 0.2
done
kill "$pid" 2>/dev/null || true
wait "$pid" 2>/dev/null || true
if ! grep -aq 'Sound: Enabled' "$check/run.log"; then
    echo "the AppImage did not start with sound:" >&2
    tr '\r' '\n' < "$check/run.log" | tail -n 5 >&2
    exit 1
fi

echo "AppImage written to $root, and it starts with sound"
