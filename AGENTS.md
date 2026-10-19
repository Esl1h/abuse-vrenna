# AGENTS.md

Instructions for AI agents (Claude Code, Codex, and so on) working in this repository.
`CLAUDE.md` is a symlink to this file: `ln -s AGENTS.md CLAUDE.md`.

Everything public in this repository is written in English: source, comments, console
output, scripts, documentation and commit messages. Portuguese appears only where it is
the content itself, which is the translation files.

## 1. Project

**Abuse: Vrenna**, a modern port of **Abuse** (Crack dot Com, 1995/1996), a 2D action
game with free aiming, starting from Xenoveritas's SDL2 + CMake fork.

The name was decided on 2026-09-20. "Abuse" stays in the title because it is what the
game's own art says; "Vrenna", the protagonist's surname, is what distinguishes it from
the `abuse` that already exists on Flathub (`com.github.Xenoveritas.abuse`, the upstream
of this fork) and in the AUR (`abuse`, the SDL 1.2 port). App ID
`io.github.Esl1h.AbuseVrenna`, package and binary `abuse-vrenna`. **The user directory
is not renamed**: config and saves stay under `abuse/`, where they have always been.

Goals:

- Run well on current hardware and systems (Linux, Windows, macOS; Wayland and X11).
- Full gamepad support, modern UI and HUD, translation (en, pt-BR), optional lighting
  and shaders.
- Two modes in the start menu: **Original** (original data and audio, untouched) and
  **Remastered**.
- Ship with data that is 100% redistributable (Flathub, AUR, AppImage, GitHub Releases).

**`docs/` is local and stays out of the repository** (it is in `.gitignore`). That is
where the planning lives: `docs/plan/README.md` with the state of each phase, one file
per phase, `docs/plan/proximos-passos.md`, `docs/plan/validacao-humana.md` and the rest.
A fresh clone has none of it; what is public about the direction of the project is in
the [`README.md`](README.md).

An agent working on a machine that has `docs/` should read it as before. In a clone
without it, the README and this file are the whole contract.

### References

| Repository | Role |
| --- | --- |
| https://github.com/Xenoveritas/abuse | **Upstream of this fork** (SDL2, CMake, vcpkg) |
| https://github.com/Xenoveritas/abuse/blob/master/TODO.md | Upstream's own open items (gamepad, input, config) |
| https://github.com/darealshinji/abuse-game | SDL 1.2 / autotools port (historical base, behaviour reference) |
| http://abuse.zoy.org/ | Sam Hocevar's Abuse-SDL project: history, data, licences |
| http://abuse.zoy.org/wiki/download | Data tarballs, `abuse-free`, Golgotha sound and music |

## 2. Rules that are never broken

1. **Do not modify original data.** Nothing under `classic/`, and none of Bobby Prince's
   sound or music, may be converted, resampled, edited, or fed to an AI tool.
2. **Do not commit non-free data.** Original sound and music never enter the repository
   or the official package. The user downloads them.
3. **Every new asset is registered** in `data/MANIFEST.toml` with origin, author,
   licence and, when AI-generated, the tool, model and date.
4. **Game logic does not change without the replays passing.** The state hash of the
   replays in `tests/replays/` must stay identical, unless the change is intentional and
   documented in the PR.
5. **The Original mode is the reference.** Its snapshots and hashes must not regress.
6. **Do not touch `data/lisp/` or object logic** without the task asking for it. The C++
   engine and the Lisp are coupled by symbol names and order.
7. **Every visual feature is optional** and the "classic" preset stays available.
8. **No network dependency at runtime**, other than the explicit, confirmed download of
   the original data.
9. **Code licence: GPL-2.0** (inherited). Do not add code under an incompatible licence.
10. Do not run `sudo`, do not change the host system, do not `git push` unless told to.

## 3. Repository layout

```text
.
├── AGENTS.md            # this file (CLAUDE.md -> AGENTS.md)
├── ARCHITECTURE.md      # map of the code
├── BUILDING.md          # build instructions, inherited from upstream
├── CMakeLists.txt       # minimum 3.14; dependencies through CPM
├── CMakePresets.json    # upstream's Windows/macOS presets + dev/release/asan/headless
├── cmake/CPM.cmake      # upstream's dependency manager
├── src/                 # C++ engine
│   ├── (root)           # loop, level, objects, AI, HUD, menu, editor
│   ├── data/            # per-mode paths, classic data, writing back to abuserc
│   ├── i18n/            # language selection and the catalogue for C++ text
│   ├── imlib/           # graphics, windows, fonts, .spe, RNG
│   ├── input/           # action map, gamepad, aim, rumble
│   ├── lisp/            # Lisp interpreter
│   ├── lol/             # leftovers of the Lol Engine: vectors, Timer
│   ├── net/             # networking
│   ├── render/          # presentation options: scale, filter, vsync, FPS limit
│   ├── sdlport/         # the SDL3 bridge: video, events, sound, config
│   ├── timing/          # fixed 15 Hz timestep
│   ├── tool/            # abuse-tool
│   └── ui/              # native resolution overlay and the screens it draws
├── data/                # game data
│   ├── lisp/            # object behaviour, and the translation tables
│   ├── abuse.lsp        # Lisp entry point
│   ├── dynlight.txt     # which object types light the room, and by how much
│   └── MANIFEST.toml    # licence per file; checked by scripts/check-licenses.sh
├── doc/                 # upstream documentation and icons
├── docs/                # planning (local, not in the repository)
├── packaging/           # Flatpak, AUR and AppImage skeletons; none built yet
└── tests/
    ├── unit/
    ├── replays/         # *.rec
    └── golden/          # hash/, frames/ and window/
```

`src/imlib/` and `src/lisp/` do not depend on SDL; `src/sdlport/` is the only directory
that includes `SDL3/`. The modules added by this fork follow the same rule: `input/`,
`i18n/`, `render/`, `timing/` and `data/` are pure logic and unit tested, and whatever
talks to SDL lives in `sdlport/`.

## 4. Build

Reference host: Fedora 44 desktop with AMD. Test host: an EndeavourOS laptop with Intel
and NVIDIA.

Presets are defined in `CMakePresets.json`:

| Preset | Type | For |
| --- | --- | --- |
| `dev` | RelWithDebInfo | Development: `-Wall -Wextra`, tests, ccache, mold |
| `release` | Release | Playing and packaging |
| `asan` | Debug | ASan and UBSan |
| `headless` | RelWithDebInfo | CI: no MIDI, no ccache or mold |

```bash
cmake --preset dev && cmake --build --preset dev
cmake --preset asan && cmake --build --preset asan
```

They all inherit `linux-base`, which sets `CPM_USE_LOCAL_PACKAGES=ON`. **Without it CPM
builds the whole of SDL3 from source** and needs every X11 build dependency.
`SDL3_mixer` and `SDL3_native_midi` come from CPM either way: no distribution packages
them yet.

There is no `vcpkg` here: the SDL2 path used it, and every dependency now comes through CPM.

Running:

```bash
./build/release/src/abuse -datadir ./data -window
SDL_VIDEODRIVER=x11 ./build/release/src/abuse -datadir ./data
```

`-datadir ./data` is needed while the `datadir` in `~/.abuse/abuserc` points at an
installed path. The default is fullscreen; use `-window`. On the first run the game asks
for the language and then for the gamma calibration, once each.

**The Remastered mode has no sound yet.** The free data carries no effects and no music;
that is phase 5. The Original mode plays as soon as the original data is installed with
`scripts/fetch-classic-data.sh`, or `scripts/fetch-classic-data.ps1` on Windows.

## 5. Tests

Test harness flags (`src/harness.cpp`):

| Flag | What it does |
| --- | --- |
| `--seed N` | Pins the RNG cursor, which otherwise comes from the clock |
| `--headless` | SDL dummy drivers; skips gamma, the title and the language screen |
| `--level <file>` | Loads a level without going through the menu |
| `--record <file>` | Records input per tick; needs `--level` |
| `--playback <file>` | Replays recorded input |
| `--state-hash` | Prints the state hash at the end |
| `--hash-every N` | Also prints it every N ticks |
| `--max-ticks N` | Ends the run |
| `--dump-frames <t1,t2,...>` | Writes BMP frames at the given loop counts. One loop is one tick only when the run is headless; with a window it is one drawn frame |
| `--dump-tick <t1,t2,...>` | Writes a frame at the given **level** ticks instead, and ends the run once it has them all. The same moment whatever the frame rate, which is what makes a windowed capture comparable |
| `--dump-window` | Also writes the frame as presented, after scaling and letterboxing |
| `--window-size W H` | Pins the window, which the presented frame depends on |
| `--out <dir>` | Output directory for the dumps |
| `--dump-bindings` | Prints the resolved action map |
| `--dump-options`, `--dump-controls`, `--dump-language`, `--dump-classic-data`, `--dump-menu-hint`, `--dump-start-menu`, `--dump-hud` | Draws one of the new screens into a scripted frame |
| `--particle-demo` | Throws debris at the player every few ticks, which nothing in a scripted run does on its own |
| `--dynlight-demo` | Gives the player a warm, wavering light, for the same reason: a scripted run fires no shot, because the first weapon has no ammunition yet and the shot is aimed at wherever the mouse is |
| `--dump-dynlight` | Prints `data/dynlight.txt` and the object type each entry resolved to, or -1. The free data and the original define different types |
| `--dump-tiles` | Prints the collision each foreground tile carries. There is no hardness table in this engine: a tile blocks because its own art says so |
| `--dump-player` | Prints the player's position and velocity every tick. How the jump height was measured rather than assumed |
| `--renderer <classic\|gpu>` | Picks the presentation path without touching abuserc. The tests always use classic, which is what the golden frames were taken through |
| `--bloom F`, `--bloom-threshold F` | The glow and where it starts, on the gpu path |
| `--frame-alpha F` | Forces the interpolated draw at a fixed point between two ticks |
| `--input-script <file>` | Drives the player from a text file, one line of `<ticks> <actions>` per stretch. With `--record` it writes a real recording of what the script did |
| `--level-info` | Prints the geometry of the level that loaded, and what each aspect ratio would need of it |
| `--mode <original\|remaster>` | Selects the mode without going through the menu |
| `--classic-data <dir>` | Path to the original data |

The binary lands in `build/<preset>/src/abuse`.

Commands:

```bash
ctest --preset dev                                     # unit + replays + snapshots + window
./scripts/test-replays.sh build/dev/src/abuse          # logic hashes only
./scripts/test-snapshots.sh build/dev/src/abuse        # the 320x200 frames
WINDOW=1 ./scripts/test-snapshots.sh build/dev/src/abuse   # the presented frames
./scripts/update-golden.sh build/dev/src/abuse         # rewrite the references
./scripts/record-replay.sh build/dev/src/abuse levels/level00.spe tests/replays/x.rec  # synthetic
./scripts/record-played.sh build/release/src/abuse levels/level00.spe tests/replays/x.rec  # played
clang-tidy -p build/dev $(git diff --name-only -- '*.cpp')
```

`./scripts/test-gpu.sh build/dev/src/abuse` draws the same screens through both
presentation paths and compares them with each other, which is the only automatic
check the GPU path has. Not against the stored frames: a window read back from a
real driver does not match one read back from SDL's dummy driver, and every stored
frame is headless. It skips itself, with CTest's 77, wherever the GPU path cannot
start, which includes every CI runner.

`./scripts/check-licenses.sh` checks the data manifest and runs in CI.

`./scripts/play-diagnostics.sh [asan|release]` plays with the sanitisers on and
everything written to a log, for a crash that only happens with a person at the
controls.

`./tools/spe-export/spe-export.py` exports the art as PNG with an index and a
licence check, for the HD pack. `./tools/asciimap/` compiles a text grid into a
playable level, tiling it the way the shipped levels are tiled.

`./scripts/scan-levels.sh build/dev/src/abuse` walks every level at each wider
aspect ratio and reports how much light there is down each edge of the frame, which
is how a level that was never drawn that far out gets found.

`./scripts/test-aspect.sh build/dev/src/abuse` checks that the simulation reaches the
same state whatever the window width, which is what keeps widescreen from being a
different game. `ASPECT_ALL=1` runs it on every replay instead of the moving one.

To make a replay that does something:

```sh
./build/dev/src/abuse --headless -nodelay --level levels/level00.spe \
    --input-script tests/inputs/level00-run.txt \
    --record tests/replays/level00-run.rec --max-ticks 200 -datadir ./data
```

`test-replays.sh` and `test-snapshots.sh` exit with 77, CTest's skip code, when there is
no replay at all.

**Only update the references for an intentional change, and in a commit of its own.**
Never run `update-golden.sh` on a loaded machine.

Three properties of the harness are load-bearing and easy to undo by accident: it does
**not** read `abuserc`, does **not** consult the system locale, and does **not** open a
gamepad. All three are the same rule, which is that a test reading the environment is a
test whose result depends on the machine it ran on.

## 6. Code conventions

- C++17 (moving to C++20 only with the decision written down). No new exceptions on a
  hot path.
- Do not reformat whole files. Run `clang-format` on the changed lines only
  (`git clang-format`). Style for new code: 4 spaces, brace on the same line.
- New modules go in `namespace abuse::<module>`.
- No raw `new`/`delete` in new code; use `std::unique_ptr` and containers.
- No new global state; where it is unavoidable, isolate it and say why.
- **English everywhere**: comments, identifiers, console output, scripts and commit
  messages. The exception is translated content, which is Portuguese by definition:
  `data/lisp/portuguese.lsp` and the `pt` column of `src/i18n/uitext.h`.
- Player-visible text goes through the Lisp symbol table or `src/i18n/uitext.h`, never
  as a bare literal.
- One byte is one glyph throughout the engine: it measures text with `strlen` in dozens
  of places, so each language has a single-byte encoding rather than UTF-8.
- Build with no new warnings under `-Wall -Wextra`.

## 7. Commits and PRs

Conventional Commits, scope = module, subject in English, imperative, 50 characters at
most:

```text
feat(render): upload the 8-bit framebuffer as a texture
feat(input): action map with several bindings per action
fix(audio): clipping when several explosions mix
test(replay): deterministic input recording
chore(ci): Linux/Windows/macOS matrix
```

- One PR per deliverable. A small PR beats a large one.
- Branch: `phase-N/<short-description>`.
- PR description: what changed, how it was tested, the effect on replays and snapshots,
  and the risks.
- Tags: `v0.9.0-alpha.N` when a phase closes; `v1.0.0` at the end of phase 8.

## 8. Agent workflow

For each task:

1. Read `docs/plan/README.md` and the file for the phase, when `docs/` exists on this
   machine; otherwise the README.
2. Read `ARCHITECTURE.md` and the source files involved before proposing a change.
3. Plan in a few steps and list the files that will be touched.
4. Implement the smallest change that meets the acceptance criterion.
5. Build and run the tests (section 5). Do not report done with a failing test.
6. Update the documentation that the change affects (`ARCHITECTURE.md`, the phase file,
   `MANIFEST.toml`, `README.md`).
7. Deliver: a summary, the relevant diff, the test results, the risks, and what needs a
   human to judge it.

Stop and ask when:

- The task would need Lisp, the `.spe` format, or object logic changed.
- A replay hash moves and the reason is not obvious.
- There is any doubt about the licence of a file.
- A new dependency would have to be added.

## 9. Definition of done

- [ ] Clean build (Release and Debug+ASan/UBSan) with no new warnings
- [ ] `ctest` green
- [ ] Replay hashes identical, or the change justified and the reference updated
- [ ] Snapshots of the classic preset and the Original mode unchanged
- [ ] `check-licenses.sh` green
- [ ] Documentation and the phase checklist updated
- [ ] Anything needing human judgement listed in the PR rather than declared validated

## 10. What needs a human to judge it

Feel of the controls and latency, difficulty, audio mix and timbre, art and fonts,
translation in context, physical gamepads, licence and naming decisions, publishing.
An agent lists these in the PR instead of calling them validated.
