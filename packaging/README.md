# Packaging

Skeletons for the four channels of phase 8. **None of these has been built
yet**: they carry the identity (name, App ID, licence, description) so that
everything downstream agrees on it, and the build recipes still need to be
run.

The Flatpak manifest is now pinned to what `CMakeLists.txt` actually
fetches, with a commit for every dependency, and `SDL3_native_midi` and
doctest dealt with: a Flathub build has no network, so anything CPM would
fetch at configure time has to be supplied as a source or switched off.
`scripts/check-flatpak-pins.sh`, which runs in CTest, fails when the two
drift apart. They already had: the manifest asked for SDL3 3.2.0 while the
build used 3.4.14, and pinned SDL_mixer to a branch, which Flathub does not
accept.

| Channel | Built by | Package name |
| --- | --- | --- |
| Flathub | `flatpak/` | `io.github.Esl1h.AbuseVrenna` |
| AppImage | `appimage/build-appimage.sh` | `Abuse_Vrenna-<version>-x86_64.AppImage` |
| Tarball | `scripts/package-linux.sh tarball` | `Abuse_Vrenna-<version>-linux-x86_64.tar.gz` |
| Debian | `scripts/package-linux.sh deb` | `abuse-vrenna_<version>_amd64.deb` |
| Fedora | `scripts/package-linux.sh rpm` | `abuse-vrenna-<version>-1.x86_64.rpm` |
| AUR | `aur/PKGBUILD` | `abuse-vrenna`, `abuse-vrenna-git` |
| Windows | (CI, on a tag) | `Abuse_Vrenna-<tag>-windows-x64.zip` |

## What the first real build taught

None of this was built until 2026-09-25, and building it found five things
a manifest nobody runs cannot show:

- SDL3_mixer does not compile in tree: its example data copies onto itself
  and ninja calls that a dependency cycle. Every Flatpak module builds out
  of tree now.
- `get_app_name()` in SDL_native_midi reads address one when the command
  line holds no `/`, which is exactly how a Flatpak launches a game. See
  `third_party/patches/`.
- The AppImage could not find its data: the path is baked in at configure
  time and lives under the mount point there. Its AppRun sets `ABUSE_PATH`.
- A package must not write `libSDL3.so.0` into the system library
  directory: dpkg refuses to install over Debian's `libsdl3-0`. Both
  bundled libraries go to `<libdir>/abuse-vrenna/` and the binary finds
  them through its RUNPATH.
- What is bundled must be filtered out of the package's own dependencies,
  or dnf asks the system for a library only this package has.

A sixth turned up only when a person played the AppImage, on 2026-10-09: it
found its levels and art but had no sound. `ABUSE_PATH` was read after
`sound_init`, which looks for `sfx/` under the baked-in prefix and gives up
with no device when it is not there. The Flatpak never showed it, because its
prefix is the real one. `scripts/check-install.sh` now starts the installed
game with a wrong prefix and `ABUSE_PATH` set and expects `Sound: Enabled`;
`--headless` cannot see this, because it switches sound off.

Debian 13 ships SDL3 3.2, older than the 3.4 the pinned SDL3_mixer needs, so
there CPM builds SDL3 as well and both libraries travel inside the package.
On Fedora and Arch the system SDL3 is used and only the mixer travels.

### Not yet, and why

- **Snap**: the manifest is the easy part. It needs snapcraft plus LXD or
  Multipass to build, and strict confinement moves the save directory out of
  `~/.abuse`, which this project has decided not to rename. That means
  classic confinement and a manual review, or a decision about saves.
- **Nix**: cheap to write and impossible to verify here, because no machine
  in this project has Nix. The work is teaching it to build offline: SDL3
  and SDL3_mixer from nixpkgs, SDL_native_midi as a fixed-output fetch with
  our patch in `patches`, and the tests off so doctest is not fetched.

## Identity

- **Title**: Abuse: Vrenna
- **Summary**: Modern port of Abuse (1995)
- **App ID**: `io.github.Esl1h.AbuseVrenna`
- **Binary**: `abuse-vrenna`
- **Licence**: GPL-2.0-or-later for the code; see `data/MANIFEST.toml` for data

The repository is `Esl1h/abuse-vrenna`, renamed from `Esl1h/abuse` on
2026-09-22 to match. GitHub redirects the old URL, so a clone made before the
rename keeps working.

The App ID keeps its capitals, `io.github.Esl1h.AbuseVrenna`, and does not
have to follow the repository name. AppStream accepts it: `appstreamcli
validate` passes, with one pedantic note that a component ID should be all
lowercase. A hyphen in an ID is fine, measured the same way, so
`io.github.esl1h.abuse-vrenna` would validate with nothing at all. Changing
it renames four files and the manifest, and is a decision of its own.

The App ID and the package names carry `vrenna` because `abuse` is already
taken on both Flathub and the AUR: `com.github.Xenoveritas.abuse` is the
upstream of this fork, and `abuse` and `abuse-git` are the SDL 1.2 port.

**The user directory is not renamed.** Config and saves stay under
`$XDG_CONFIG_HOME/abuse/` and `$XDG_DATA_HOME/abuse/`, where they have always
been. That path is not the product name, and moving it would orphan every
existing save for a cosmetic reason.

## Original data

The original sound and music are not redistributable and are in no package
here. Both the Flatpak and the native packages ship
`scripts/fetch-classic-data.sh`, which the player runs once; the Original mode
finds the result on its own.
