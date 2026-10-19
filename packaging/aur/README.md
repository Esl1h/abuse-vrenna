# AUR

Three packages are planned, and none is submitted yet.

- `abuse-vrenna`: the tagged release, built from the GitHub tarball
- `abuse-vrenna-git`: the `main` branch, for people who want it early
- `abuse-vrenna-classic-data`: **not planned.** The original sound and music
  are not redistributable, so there is nothing to package. The fetch script
  installed as `abuse-vrenna-fetch-classic-data` is what the player runs.

`abuse` and `abuse-git` already exist in the AUR and are the SDL 1.2 port.
This package does not conflict with them: different binary, different data
directory, different name.

Before submitting:

```sh
namcap PKGBUILD
makepkg --printsrcinfo > .SRCINFO
```
