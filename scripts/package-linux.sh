#!/usr/bin/env bash
# Builds the Linux packages that are not the Flatpak or the AppImage: a
# portable tarball, a .deb and an .rpm.
#
#   ./scripts/package-linux.sh tarball
#   ./scripts/package-linux.sh deb
#   ./scripts/package-linux.sh rpm
#   ./scripts/package-linux.sh all
#
# The tarball is built here. The .deb and the .rpm are built inside a
# container of the distribution they are for, because dpkg-deb and rpmbuild
# are what CPack shells out to and neither is installed on the reference
# host; building a Debian package on Fedora would also record this machine's
# library names. podman is used when it is there, docker otherwise.
#
# Everything lands in dist/.
set -euo pipefail

root=$(cd -- "$(dirname -- "$0")/.." && pwd)
cd "$root"

version=$(sed -n 's/^set(abuse_VERSION \(.*\))$/\1/p' CMakeLists.txt)
dist="$root/dist"
mkdir -p "$dist"

engine=""
if command -v podman >/dev/null 2>&1; then
    engine=podman
elif command -v docker >/dev/null 2>&1; then
    engine=docker
fi

# The build dependencies, by distribution. SDL3 comes from the distribution
# where it has one; SDL3_mixer never does, so it is built from source and
# installed beside the game, and these are the codec libraries it looks for.
# Debian 13 ships SDL3 3.2, older than the 3.4 the pinned SDL3_mixer needs,
# so CPM builds SDL3 from source there and every one of SDL3's own build
# dependencies has to be here. Leaving one out does not disable a feature:
# SDL treats a missing dependency for an enabled subsystem as an error.
# file and dpkg-dev are for CPack itself: it shells out to dpkg-shlibdeps
# to work out the dependencies, and refuses to start without `file`.
debian_deps="build-essential cmake ninja-build git ca-certificates file dpkg-dev
    libogg-dev libvorbis-dev libopus-dev libopusfile-dev
    libflac-dev libmpg123-dev libxmp-dev libwavpack-dev
    libasound2-dev libpulse-dev libjack-dev libsndio-dev
    libx11-dev libxext-dev libxrandr-dev libxcursor-dev
    libxfixes-dev libxi-dev libxss-dev libxtst-dev libxkbcommon-dev
    libdrm-dev libgbm-dev libgl1-mesa-dev libgles2-mesa-dev libegl1-mesa-dev
    libdbus-1-dev libibus-1.0-dev libudev-dev
    libfribidi-dev libthai-dev libusb-1.0-0-dev
    libwayland-dev wayland-protocols libdecor-0-dev"
fedora_deps="gcc gcc-c++ cmake ninja-build git rpm-build
    SDL3-devel libogg-devel libvorbis-devel opus-devel opusfile-devel
    flac-devel libmpg123-devel libxmp-devel wavpack-devel alsa-lib-devel"

build_tarball() {
    local stage="$root/build/tarball"
    local name="Abuse_Vrenna-${version}-linux-x86_64"
    local out="$stage/$name"

    rm -rf "$stage"
    cmake -S "$root" -B "$stage/build" -G Ninja \
        -DCMAKE_BUILD_TYPE=Release \
        -DCMAKE_INSTALL_PREFIX=/usr \
        -DABUSE_BUILD_TESTS=OFF \
        -DCPM_USE_LOCAL_PACKAGES=ON
    cmake --build "$stage/build"
    DESTDIR="$out" cmake --install "$stage/build"

    install -Dm644 "$root/README.md" "$out/README.md"
    install -Dm644 "$root/COPYING" "$out/COPYING"

    # Same reason as the AppImage's AppRun: the data path is baked in at
    # configure time and a tarball is unpacked wherever its owner likes.
    cat > "$out/abuse-vrenna" <<'LAUNCH'
#!/bin/sh
# Runs the game from wherever this tarball was unpacked.
set -e
HERE=$(dirname "$(readlink -f "$0")")
export ABUSE_PATH="${ABUSE_PATH:-$HERE/usr/share/games/abuse}"
export LD_LIBRARY_PATH="$HERE/usr/lib64/abuse-vrenna:$HERE/usr/lib/abuse-vrenna:$HERE/usr/lib64:$HERE/usr/lib:${LD_LIBRARY_PATH}"
exec "$HERE/usr/bin/abuse-vrenna" "$@"
LAUNCH
    chmod +x "$out/abuse-vrenna"

    tar -C "$stage" -czf "$dist/${name}.tar.gz" "$name"
    echo "tarball: $dist/${name}.tar.gz"
}

# Builds one native package inside a container. $1 is the image, $2 the
# install command for the build dependencies, $3 the CPack generator.
build_in_container() {
    local image=$1 deps=$2 generator=$3

    # The lists below are written over several lines to be read; the shell
    # inside the container would end the install command at the first
    # newline, so they are folded back into one.
    local deps_one_line
    # shellcheck disable=SC2086,SC2116 # the word splitting is the folding
    deps_one_line=$(echo $deps)

    if [ -z "$engine" ]; then
        echo "neither podman nor docker is installed; cannot build $generator" >&2
        return 1
    fi

    # As root inside the container, which is what installing build
    # dependencies needs. Under rootless podman that root is this user on the
    # outside, so what lands in dist/ is owned by whoever ran this.
    "$engine" run --rm \
        `# :z and not :Z. The exclusive label takes the directory away` \
        `# from any other container using it, and a package test running` \
        `# beside a build then fails with a permission denied nobody expects.` \
        -v "$root:/src:z" -w /src \
        -e "GENERATOR=$generator" \
        "$image" bash -lc "
            set -euo pipefail
            $deps_one_line
            rm -rf /tmp/build
            cmake -S /src -B /tmp/build -G Ninja \
                -DCMAKE_BUILD_TYPE=Release \
                -DCMAKE_INSTALL_PREFIX=/usr \
                -DABUSE_BUILD_TESTS=OFF \
                -DCPM_USE_LOCAL_PACKAGES=ON
            cmake --build /tmp/build
            cd /tmp/build && cpack -G \$GENERATOR
            mkdir -p /src/dist
            shopt -s nullglob
            for f in /tmp/build/*.deb /tmp/build/*.rpm; do
                cp -v \"\$f\" /src/dist/
            done
        "
}

what=${1:-all}
case "$what" in
    tarball) build_tarball ;;
    deb)     build_in_container debian:13 "apt-get update && apt-get install -y $debian_deps" DEB ;;
    rpm)     build_in_container fedora:44 "dnf install -y $fedora_deps" RPM ;;
    all)
        build_tarball
        build_in_container debian:13 "apt-get update && apt-get install -y $debian_deps" DEB
        build_in_container fedora:44 "dnf install -y $fedora_deps" RPM
        ;;
    *)
        echo "usage: $0 [tarball|deb|rpm|all]" >&2
        exit 2
        ;;
esac

ls -lh "$dist"
