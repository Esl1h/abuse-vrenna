# Architecture

A map of the fork's code, written by reading `src/` and `data/lisp/`. Base:
`upstream/sdl3` (tag `upstream-sdl3-base`, `dbe3e53`). No code was changed in order to
write this document.

References in the form `file:line` point at that base.

## 1. Overview

The game is a single loop with no logic threads. The world state lives in one global
`level`, the objects are a plain linked list, and the behaviour of each object is
written in Lisp and run by an interpreter of its own in `src/lisp/`.

The C++ layer and the Lisp talk to each other by **number**, not by name: every native
function is registered with an integer identifier (`src/lisp/lisp.cpp:954`) and
dispatched through a huge `switch` in `src/clisp.cpp`. Renumbering breaks everything
silently. That is the reason for rule 6 in `AGENTS.md`.

Rendering is indirect: the game draws into an 8-bit indexed framebuffer, which only
becomes an RGBA texture at the end of the frame.

```
main()                          src/game.cpp:2284
├── setup()                     src/sdlport/setup.cpp:317   window, abuserc, command line
├── start_sound()               src/sdlport/sound.cpp:127
├── jrand_init()                src/game.cpp:2350           RNG SEED
├── Lisp::Init()                src/game.cpp:2365
├── new Game()                  src/game.cpp:1268
└── while (!g->done())          src/game.cpp:2400
    ├── music_check()
    ├── net_receive()
    ├── g->get_input()          src/game.cpp:1565
    ├── net_send() | demo_man.do_inputs()
    ├── g->step()               src/game.cpp:1890   -> level::tick()  src/level.cpp:616
    ├── g->calc_speed()         src/game.cpp:1511   WALL CLOCK
    └── g->update_screen()      src/game.cpp:1454
```

## 2. Modules

| Directory | Role | Largest files |
| --- | --- | --- |
| `src/` | The game: loop, level, objects, AI, HUD, menu, editor | `dev.cpp` 3596, `level.cpp` 3272, `game.cpp` 2501, `clisp.cpp` 2336, `objects.cpp` 1640 |
| `src/lisp/` | Lisp interpreter: reader, evaluator, GC, symbol table | `lisp.cpp` 3232 |
| `src/imlib/` | Abuse's own graphics and window library: images, fonts, widgets, `.spe`, RNG | `image.cpp` 999, `specs.cpp` 967, `jwindow.cpp` 743 |
| `src/sdlport/` | The bridge to SDL3: video, events, sound, time, configuration | `sound.cpp`, `video.cpp`, `event.cpp`, `setup.cpp` |
| `src/lol/` | Leftovers of the Lol Engine: vectors, matrices, `Timer` | `matrix.h`, `timer.cpp` |
| `src/net/` | Networking: TCP/IP, game server, file server | `tcpip.cpp` 700, `fileman.cpp` 564 |
| `src/ui/` | Native resolution overlay, the new screens, and two inherited widgets | `options_screen.cpp`, `start_menu.cpp`, `overlay.cpp`, `menu_list.cpp`, `hexfont.cpp` |
| `src/tool/` | `abuse-tool`, a `.spe` utility that runs outside the game | |

Modules added by this fork, all in `namespace abuse::<module>`:

| Directory | Role |
| --- | --- |
| `src/data/` | Per-mode paths, detecting the classic data, writing back to `abuserc` |
| `src/render/` | Presentation options: scale, filter, vsync, FPS limit |
| `src/timing/` | Fixed 15 Hz timestep, decoupled from the frame |
| `src/input/` | Action map, gamepad, aim, rumble |
| `src/i18n/` | Language selection and the catalogue for text written in C++ |
| `src/ui/` | The native resolution overlay and the screens it draws |

Dependencies: `src/` uses everything; `src/imlib/` and `src/lisp/` do not depend on SDL;
`src/sdlport/` is the only one that includes `SDL3/`. That separation is what made the
SDL3 port possible without rewriting the game. The new modules follow the same rule:
`input/`, `i18n/` and `render/` are pure, testable logic, and whatever talks to SDL
lives in `sdlport/`. The one deliberate exception is `ui/classic_data_screen.cpp`,
which calls `SDL_OpenURL` because that call is the whole feature.

### Two text layers

The engine draws text in two ways, and they do not mix:

1. **`JCFont`**, the original: a 32 by 8 glyph atlas cut out of an image, indexed by
   byte, drawn into the 320x200 buffer and scaled along with it. All the inherited
   interface uses this one. The extended font enters here as *another atlas*, not as
   another drawing path.
2. **`abuse::ui::Overlay`**: an ARGB buffer the size of the window, composited after
   the scaling, with glyphs read from a `.hex` file. Only the new screens use it.

In both, **one byte is one glyph**: the engine measures text with `strlen` in dozens of
places, and moving to UTF-8 would break that arithmetic in all of them. That is why
each language has a single-byte encoding (CP437 for English, French and German;
Latin-1 for Portuguese) instead of one universal encoding.

## 3. Time and ticks

**There is no separation between a logic tick and a rendered frame** in the original
code. One turn of the loop is one tick and one frame, always. This fork adds
`src/timing/pacer`, which runs the logic at a fixed 15 Hz and lets the frame rate move
independently.

The original cadence comes from `Game::calc_speed()` (`src/game.cpp:1511`), which
measures elapsed real time with `Timer::PollMs()` and tries to hold the loop at 15 Hz
(`frame_timer.WaitMs(1000.0f / 15)`, `src/game.cpp:1544`). `Timer`
(`src/lol/timer.cpp:48`) uses `gettimeofday` on Linux.

When the target is missed, two counters go up:

| Variable | Defined at | Effect |
| --- | --- | --- |
| `frame_panic` | `src/game.cpp:81` | **Visible to the Lisp** as `(frame_panic)`, `src/clisp.cpp:446` |
| `massive_frame_panic` | `src/game.cpp:81` | Progressively turns the lighting off, `src/level.cpp:683` |

The level's tick counter is `level::tick_counter()`, incremented at
`src/level.cpp:766`, persisted in the savegame (`src/level.cpp:2091`) and truncated to
8 bits in `base->current_tick` for the network (`src/game.cpp:537`).

## 4. World state and iteration order

`current_level` is global. The objects form two linked lists:

- `first` / `next`: every object in the level, in load order
- `first_active` / `next_active`: the ones active this tick

`level::add_actives()` (`src/level.cpp:220`) walks `first` and activates whatever falls
inside a rectangle, preserving the order of the full list. The rectangle comes from the
view, in `src/game.cpp:1908`, with a margin of a quarter of the width and height.

A direct consequence for the widescreen phase: **the activation area is the visible
area**. Widening the viewport to 16:9 changes which objects wake up and when, so it
changes the logic. The decoupling that phase needs happens exactly here.

`level::tick()` (`src/level.cpp:616`) walks `first_active` and, for each object, stores
`last_x`/`last_y` before moving it (`src/level.cpp:656`). Those fields already exist
and are what frame interpolation would need.

## 5. RNG

A fixed table of 1024 values, generated by a linear congruential generator with a
constant seed, plus a global cursor:

```c
extern unsigned short rtable[RAND_TABLE_SIZE];   src/imlib/jrand.h:15
extern unsigned short rand_on;                   src/imlib/jrand.h:16
inline unsigned short jrand() { return rtable[(rand_on++) & 1023]; }
```

The table is always the same. **The cursor is not**: `src/imlib/jrand.cpp:35-36` does
`rand_on = time(NULL) % RAND_TABLE_SIZE`. That is the main source of non-determinism in
the game, and the only one at startup.

Places where `rand_on` is touched outside `jrand()`:

| Where | What it does |
| --- | --- |
| `src/demo.cpp:143` | `rand_on = 0` when demo recording or playback starts |
| `src/game.cpp:903` and `:1042` | Saves and restores it around drawing, because render functions consume the RNG |
| `src/cop.cpp:189` | `rand_on += o->lvars[point_angle]`: the logic advances the cursor on purpose |
| `src/level.cpp:1683` and `:1777` | Written to and read from the savegame |

The `src/game.cpp:903`/`:1042` pair deserves attention: the renderer consuming the RNG
and the logic compensating afterwards is fragile. Any change to the drawing path can
shift the cursor without anyone noticing.

## 6. Replay: what was already there

Upstream **already had** input recording and playback, in `src/demo.cpp` and
`src/demo.h`.

| Piece | Where | State |
| --- | --- | --- |
| `demo_manager` with `NORMAL`/`RECORDING`/`PLAYING` | `src/demo.h:17` | works |
| `DEMO,VERSION:2` format: level name + difficulty + packets | `src/demo.cpp:77` | works |
| Deterministic reset before recording | `src/demo.cpp:143` | zeroes `rand_on` |
| Per-tick sync hash | `src/view.cpp:296` | **weak** |
| `frame_panic` neutralised during a demo | `src/clisp.cpp:2002` | already done |

The hash is 16 bits and covers very little:

```c
uint16_t make_sync()        // src/view.cpp:296
{
  // XOR of x and y of each player focus ...
  x ^= rand_on;             // src/view.cpp:312
  return x;
}
```

It does not include health, ammunition, AI state or the number of live objects. The
recording machinery could be reused almost whole; what had to be built was a real state
hash and the command line flags.

There is also a commented-out per-tick RNG checker in `src/level.cpp:626-651`, which
wrote `rand_on` every tick to an `rcheck` file. The right idea, abandoned.

## 7. Rendering

The path of a pixel, all in `src/sdlport/video.cpp`:

1. The game draws into an 8-bit indexed `SDL_Surface`, `SDL_PIXELFORMAT_INDEX8`
   (`:102`). That is the surface `src/imlib/image.cpp` manipulates.
2. The palette is a 256-entry `SDL_Palette` attached to that surface (`:261`).
3. At the end of the frame, `update_window_done()` (`:289`) locks a streaming
   `ARGB8888` texture (`:110`) and does an `SDL_BlitSurface`, which converts index to
   RGBA **on the CPU**.
4. `SDL_RenderTexture` + `SDL_RenderPresent` (`:300`).

Scaling uses `SDL_SetRenderLogicalPresentation(..., LETTERBOX)` (`:84`) and
`SDL_SetTextureScaleMode(texture, SDL_SCALEMODE_PIXELART)` (`:119`).

So most of the presentation work was already done upstream. What the shader phase would
replace is step 3: send the index and the palette as two textures and convert in the
shader, instead of blitting on the CPU.

One trap in this area, found the hard way: `SDL_GetCurrentRenderOutputSize` answers
with the **logical** size whenever a logical presentation is set, not with the window
size. The overlay is composited with that presentation turned off, so a buffer built to
the logical size gets stretched across the window. `window_pixel_size()` uses
`SDL_GetRenderOutputSize` for that reason, and `game_rect_to_window()` is what converts
between the two coordinate spaces.

### Lighting

Precomputed tables, not per-pixel work at runtime.

`calc_light_table()` (`src/light.cpp:210`) builds `white_light`, a map of 256 colours by
64 brightness levels, plus `TTINTS` tint tables. The result is written to `light.tbl` in
the save directory and revalidated by a CRC of the palette (`src/light.cpp:232`); if the
palette changes, it is recomputed.

At runtime, `light_patch` (`src/light.h:44`) describes regions and `calc_light_value()`
resolves the level for a point. The lighting depends on the indexed palette, which is
why reproducing it in a shader with byte-identical results is not trivial, and why the
CPU path has to stay available for the classic preset.

The table is **subtractive**: level 63 is the colour itself and each level below takes
one off every channel, the result snapped to the nearest of the 256 palette entries.
That snap is the banding, and it is why there is no room for a coloured light.

`src/render/lightmap.cpp` is the same curve without the snap. With `rgblight=on` the
lighting pass writes its levels into a map instead of remapping indices, and
`update_window_done` applies them to the real colour during the conversion to ARGB:
64000 pixels a frame, on the CPU, no GPU needed. Dark areas come out brighter than the
classic path, because the snap was landing below what the curve asked for. Off by
default, and never in the Original mode.

`src/render/dynlight.cpp` adds light back on top of that map. After the lighting pass
has run, every active object whose type is listed in `data/dynlight.txt` brightens the
map around itself, which is how a shot lights the corridor it crosses and an explosion
lights the room. It writes nothing but the map: no light source is created, and the
replay hash is the same with it on or off. It needs `rgblight=on`, because the 1995
path has no way to make a pixel brighter than the palette entry it already is.

The map holds a level **per channel**, which is what lets those lights have a colour:
a red flash takes less off the red than off the other two, and on a grey wall that
reads as red. It never adds colour, because the colour is the palette's. With the
three channels equal, which is what the lighting pass writes, the result is bit for
bit the single-level one.

## 8. Input

Flow: SDL → the engine's own `Event` → `view::get_input()` → packet →
`process_packet_commands`.

- `src/sdlport/event.cpp` translates `SDL_Event` into `imlib`'s `Event`.
  `SDL_PollEvent` at `:93`, keyboard at `:224`, mouse at `:99`.
- Upstream bindings: `get_key_binding("left", 1)` (`src/sdlport/setup.cpp:451`) returns
  a field of a fixed `keys` struct. **One binding per action, with no alternative.**
- That struct is filled by `readRCFile()` (`src/sdlport/setup.cpp:118`) reading
  `abuserc`, a plain `key=value` file.

This fork adds `src/input/actions`, an action map that allows several bindings per
action, from the keyboard, the mouse or a gamepad, expressed in `abuserc` as `bind=`
lines. The eight packet actions are the contract: `to_flags()` is the only place that
knows the wire layout.

There is a parallel dead path: `data/lisp/input.lsp` and the commented-out block in
`src/view.cpp:324` allowed input to be defined from the Lisp side.

### Gamepad

The upstream `TODO.md` understates what was already there: d-pad buttons mapped to
actions (`src/sdlport/event.cpp:368`), analogue axes (`:392`), and right-stick aiming
with a centre and a scale (`src/imlib/event.h:99-124`).

What this fork added: `SDL_Gamepad` instead of `SDL_Joystick`, hot-plug that pauses the
game, configurable deadzones, rumble, button labels per controller family, aim as a
vector from the player with a circular clamp, and optional aim assist.

Two things about weapon switching are worth knowing before touching it. It is **not**
one of the eight packet bits: nothing consumes those bits, and the engine has always
driven it from key events, as a comment in `data/lisp/input.lsp` explains. This fork
routes it through the action map instead, detecting a rising edge in
`consume_weapon_change()`, which is sampled after every event rather than once a tick,
because a tap shorter than 66 ms would fit inside one tick.

There are also several event loops that never meet: the tick loop in
`Game::get_input`, the title menu's own loop in `src/menu.cpp`, and the modal windows.
A key handled in only one of them works in only one of them, which is why
`abuse::ui::handle_global_key` exists.

## 9. The start menu

`src/ui/start_menu.cpp` is the list the game opens on, and the one Esc reaches from
inside a level. It replaces the strip of icons that `make_default_buttons` builds in
`src/menu.cpp`, which is still there and still reachable with `startmenu=classic`.

`run_start_menu` handles everything that stays on the menu, and returns a
`StartAction` for the four things that do not: resuming, starting, loading, quitting,
plus `Idle` for the attract loop. `main_menu` acts on it, because starting a level is
the game's business. The mode row is marked: see `data::save_mode`.

### The HUD

`src/ui/hud.cpp` draws the Remastered HUD into the same overlay. The classic strip is
`status_bar` in `src/statbar.cpp`, which asks `abuse::ui::classic_hud()` before it
draws anything, in `redraw`, `draw_health`, `draw_ammo` and `draw_update`: the strip
paints from four places, and gating only the first would leave the numbers behind.

The new HUD is the default in the Remastered mode. `hud=classic` brings back the strip,
and the Original mode keeps the strip whatever the config says.

## 10. Audio

`src/sdlport/sound.cpp`, already on the new SDL3_mixer API:

- A single `MIX_Mixer`, with 50 `MIX_Track` voices
- Volume through `MIX_SetTrackGain` and position through `MIX_StereoGains`

Three decisions moved out of it into `src/audio/`, which is free of SDL and unit
tested:

| Module | What it decides |
| --- | --- |
| `audio/buses.cpp` | Gain per bus (sfx, music, UI) and a master over them, as percentages in abuserc |
| `audio/voices.cpp` | Which track a sound gets, and who loses one when they are all busy: free voice first, otherwise the weakest playing, and only if the newcomer outranks it |
| `audio/limiter.cpp` | The master limiter, installed as the mixer's post-mix callback. Both channels move together; never applied in the Original mode |

Priority is the sound's own volume, which the engine has already attenuated by
distance, so a shot across the level does not silence one at the player's feet. The
named priorities in `voices.h` are for callers that know more than the volume says;
the music track uses the highest, since the score should not be what gets stolen.

`limiter().process()` runs on the audio thread. Nothing else in that module is
synchronised, because it is all called at startup before the callback is installed.

Still open in phase 5: runtime device switching, and the free sound set itself.

The sound directory is looked up through the classic-data overlay when one is set, so
the Original mode plays as soon as that data is installed. The Remastered mode has no
free sound set yet, which is why it is silent.

## 11. Lisp and the binding to C++

`src/lisp/lisp.cpp` is a complete interpreter: reader, evaluator, garbage collector,
symbol table. Object behaviour lives in `data/lisp/`.

The binding is by number:

```c
add_c_function("distx", 0, 0, 1);            src/clisp.cpp:208
add_c_bool_fun("frame_panic", 0, 0, 243);    src/clisp.cpp:446
```

There are 257 registrations in `src/clisp.cpp`, dispatched by a `switch` on the number.
The name is only the lookup key on the Lisp side; **the number is the contract**.
Inserting a function in the middle of the list, or reordering, changes the meaning of
every one after it with no compile error.

The same holds for object variables, through `add_c_object(symbol, index)`
(`src/lisp/lisp.cpp:941`), and for `o->lvars[...]`, indexed by position.

### Strings and language

Text was already separated by language inside the Lisp: `data/lisp/english.lsp`,
`french.lsp` and `german.lsp`, selected by a `section` symbol. But `data/abuse.lsp:9`
loaded `english.lsp` and nothing else, with no choice.

Those symbols are read **only** by C++, through `symbol_str()` in `src/dev.cpp`, and by
no other Lisp code. That is what makes it safe to load a translation *on top of*
English: a symbol the translation does not cover keeps its English text instead of
reaching `"Missing language symbol!"`. `load_language_table()` in `src/loader2.cpp`
does exactly that, into the permanent Lisp space so the strings survive the `tmp` space
being cleared.

Text the new UI adds cannot live there, because `english.lsp` is original data. It goes
in `src/i18n/uitext.h` instead, one phrase per entry with an English and a Portuguese
column.

## 12. Loaders

| Format | Where | Note |
| --- | --- | --- |
| `.spe` | `src/imlib/specs.cpp`, types in `src/imlib/specs.h:26` | `SPEC1.0` signature, a directory of named entries |
| `.lsp` | `src/lisp/lisp.cpp` | Read and evaluated at load time |
| `abuserc` | `src/sdlport/setup.cpp:118` | `key=value`, no sections |
| Savegame | `src/level.cpp:2091` onwards | Includes `tick_counter` and `rand_on` |
| `light.tbl` | `src/light.cpp:224` | A cache, revalidated by a CRC of the palette |
| `.hex` | `src/ui/hexfont.cpp` | The unscii/Unifont glyph format, one line per codepoint |

Paths: data through `-datadir` or `ABUSE_PATH`. Saves, config and caches come from the
per-mode resolver (`src/data/paths.cpp`): new installs use
`$XDG_CONFIG_HOME/abuse/<mode>/` and `$XDG_DATA_HOME/abuse/<mode>/`; the legacy
`~/.abuse/` still wins while it exists, so current installs and the replay baseline are
not invalidated. `--mode original` overlays the classic set (`--classic-data`, or the
XDG default) on top of the free data rather than replacing it, because the classic
tarballs carry no levels. Windows and macOS keep their historical paths.

## 13. Editor and networking

The editor is not a separate mode: it is the same loop with `dev & EDIT_MODE` on
(`src/game.cpp:1925`), which diverts from `current_level->tick()` (`:1938`) to
`dev_scroll()` (`:1941`). `src/dev.cpp` has 3596 lines and 30 file-scope variables.
Entered with `-edit`.

Networking is peer-to-peer with input lockstep: each tick packs commands into
`base->packet` (`src/netface.h:152`), sends, receives, and only then executes. The same
path demo recording uses. `main_net_cfg` controls client and server.

This has a useful implication: **the game already has to be deterministic for
multiplayer to work.** `make_sync()` exists precisely to detect divergence between
peers. What was missing was not determinism, it was measurement.

## 14. Global state

There is no easy exact count, but the order of magnitude is: ~105 `extern` symbols in
headers, 27 file-scope definitions in `game.cpp` and 30 in `dev.cpp`.

The structural ones:

| Symbol | Defined at | Role |
| --- | --- | --- |
| `the_game` | `src/game.cpp:74` | The `Game` instance |
| `current_level` | `src/level.cpp` | The whole level |
| `wm` | `src/game.cpp:75` | Window manager, and owner of the right-stick state |
| `rand_on`, `rtable` | `src/imlib/jrand.cpp:20-21` | RNG |
| `dev` | `src/game.cpp:76` | Mode mask, including `EDIT_MODE` |
| `base` | `src/netface.h:152` | The tick's input packet |
| `demo_man` | `src/demo.cpp:30` | Recording and playback |
| `frame_panic`, `massive_frame_panic` | `src/game.cpp:81` | Cadence, with an effect on the logic |
| `player_list`, `first_view` | `src/view.cpp` | Views and players |
| `white_light`, `tints` | `src/light.cpp:33`, `:187` | Lighting tables |
| `cache` | `src/cache.cpp` | Sprite and sound cache |

There is also `static` state inside functions, such as `frame_timer` and `first` in
`calc_speed()` (`src/game.cpp:1513-1514`), which do not reset between games in the same
process.

## 15. Sources of non-determinism

In order of importance for the replay work.

| # | Source | Where | Severity | Note |
| --- | --- | --- | --- | --- |
| 1 | RNG cursor seeded from the clock | `src/imlib/jrand.cpp:35-36` | High | `demo.cpp:143` already zeroes it for demos; `--seed` covers the rest |
| 2 | `frame_panic` exposed to the Lisp logic | `src/clisp.cpp:446`, `:2005` | High | Neutralised during a demo (`:2002`); depends on that staying true |
| 3 | Wall clock in the loop cadence | `src/game.cpp:1511` | High | Fixed by `src/timing/pacer` |
| 4 | `massive_frame_panic` changes the lighting | `src/level.cpp:683` | ~~Medium~~ | **Fixed**: neutralised during a demo, like `frame_panic` |
| 5 | The renderer consumes the RNG | `src/game.cpp:903`, `:1042` | Medium | Saving and restoring is compensation, not isolation |
| 6 | Object activation depends on the view size | `src/level.cpp:220`, `src/game.cpp:1908` | Medium | Becomes a real problem in the widescreen phase |
| 7 | Function-`static` state survives between games | `src/game.cpp:1513` | Low | Only affects a second game in the same process |
| 8 | `time()` in the savegame | `src/level.cpp:1646` | None | Only the thumbnail caption |
| 9 | The host environment | `abuserc`, system locale, a plugged-in gamepad | High | The harness ignores all three; see AGENTS.md section 5 |

Verified afterwards: a replay under ASan completes with no diagnostic and with the same
hash as an `-O2` binary. That cost one fix in the Lisp collector, where
`PtrRef ref1(this)` in `LObject::Eval()` registered the address of a dead temporary,
leading the copying collector to write into dead stack.

What only MSan would answer is still unanswered: reads of uninitialised memory.

Floating point: the logic works in integers (`int32_t` for positions and `lvars`).
Floats appear in the cadence (`avg_ms`) and in `src/lol/`. That is what makes hashing
only integer state sound.

## 16. What this changed in the plan

| Area | Finding | Effect |
| --- | --- | --- |
| Replay | `demo_manager` already records and plays input | Reused rather than rebuilt; the work became the state hash and the flags |
| Replay | `rand_on` seeded by `time()` | `--seed` is one line, and the highest-return item |
| Render | The indexed framebuffer already becomes a texture with a scaler | Most of the presentation work was done |
| Input | Right-stick aiming already existed in `imlib/event.h` | Smaller scope, but the action map was still needed whole |
| i18n | Game strings already separated by language in the Lisp | A ready starting point, in the wrong format |
| Audio | The backend is already SDL3_mixer with voices | What is left is buses, a limiter and the free pack |
| Lighting | Indexed tables validated by a palette CRC | The CPU path has to stay for the classic preset |
| Widescreen | The activation area is the visible area | Risk confirmed, with an exact location |
| — | The game already has to be deterministic because of multiplayer | Measurement was missing, not determinism |
