/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  The HD override pack. Phase 6, block 6.6.
 *
 *  An image in data/hd/ replaces the one the game would have loaded from
 *  its .spe. The path is the one tools/spe-export writes, so an artist
 *  exports the art, works on a file, drops it back under the same name and
 *  the game picks it up.
 *
 *  **Same size as the original, and that is not a temporary limit.** The
 *  engine draws into an 8-bit buffer 320 pixels wide and every sprite is
 *  placed and clipped in those pixels: a sprite twice the size draws twice
 *  as big, not twice as sharp. Actual high-resolution art needs the
 *  renderer to composite sprites itself rather than blit them into that
 *  buffer, which is a larger job than this one and is written up in
 *  docs/plan/fase-06-visual.md.
 *
 *  What this is good for today is retouching: fixing a sprite, recolouring
 *  it, cleaning up a scan, and seeing it in the game without an editor.
 *
 *  Covers images, foreground and background tiles, and characters, which
 *  is every picture the cache hands out. Characters come through last
 *  because they are cut into a forward and a backward copy and the swap
 *  has to happen before either.
 *
 *  **Never in the Original mode.** That mode is what the snapshots and the
 *  replay hashes are compared against, so an installed pack must not reach
 *  it; setup() turns this off there. Without that, a reference frame would
 *  depend on whether the machine that recorded it happened to have a pack.
 *
 *  The repository ships files of its own here, the Remastered mode's title
 *  screens and the picture between levels. Everything else under data/hd/ belongs to whoever put it
 *  there: .gitignore keeps it out of the repository and
 *  scripts/check-licenses.sh fails on it, because art nobody has accounted
 *  for is exactly what that check is for.
 *
 *  This software was released into the Public Domain.
 */

#ifndef ABUSE_IMLIB_HD_H_
#define ABUSE_IMLIB_HD_H_

class image;

namespace abuse::hd {

// Whether there is a data/hd/ at all. Checked once: without it nothing
// below costs anything, which is the usual case.
bool available();

// The file that would replace entry `name` of `spe_path`, if it exists.
// Returns an allocated string the caller owns, or null.
char *find(char const *spe_path, char const *name);

// Decodes it. Null when the file will not read, when it is not the size
// the game expects, or when the palette is not loaded yet; in every case
// the caller falls back to the original and says so once.
image *load(char const *path, int want_w, int want_h);

// Swaps `original` for the file at `path` when it reads and is the same
// size, and takes ownership of whichever it does not return. Null path or
// a refusal both give the original back, so a caller can write
//
//     im = abuse::hd::swap(path, im);
//
// and be done. The path is cleared when it is refused, so the refusal is
// reported once and not every time the entry is reloaded.
image *swap(char *&path, image *original);

// Off turns the whole thing off regardless of what is on disk.
bool enabled();
void set_enabled(bool on);

// Variants. A pack may offer more than one picture for the same entry, as
// `name.png`, `name.2.png`, `name.3.png` and so on up to nine; the game
// picks one when the entry is first asked for and keeps it, so the title
// screen does not change while it is on screen. next_variant is how a caller
// asks for another one, at a moment of its own choosing.
//
// The pick comes from this seed. **Zero means always the first**, which is
// what a scripted run sets: a test that picked at random would be a test
// whose reference frame depends on the day it ran. setup() seeds it from
// the clock otherwise.
void set_variant_seed(unsigned long long seed);

// Another picture for the entry whose current path is `current`: an allocated
// path the caller owns, always a different variant from the one it has. Null
// when the entry has only one picture, when the seed is zero, or when
// `current` is not a path find() gave. The caller reloads the entry from it.
char *next_variant(char const *current);

}

#endif
