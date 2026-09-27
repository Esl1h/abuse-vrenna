#!/usr/bin/env python3
"""Builds the free soundtrack for the Remastered mode.

The game asks for twelve `music/*.hmi` files, which are the 1995 MIDI
tracks: not redistributable, not in this repository. song::song widens the
lookup the same way the sound effects are widened, so music/abuse01.ogg is
taken when it is there and never in the Original mode.

Twelve names, seven tracks: the sources that fit this game are fewer than
the levels, so some repeat. Which name gets which track is below, and it is
the only part of this worth arguing about.

Unlike the sound effects, music is **not** put through the 1995 palette.
Eight bits and 11 kHz is what a sampled gunshot sounded like on that
hardware; the music was MIDI, synthesised by whatever card the player had,
so there is no grit to match and crushing it would only make it worse.
Mono at 22 kHz, which halves the size and loses nothing a player will
notice under gunfire.

**Nobody has heard any of this in the game.**

    ./tools/free-sfx/build-music.py
    ./tools/free-sfx/build-music.py --manifest
"""

import argparse
import pathlib
import shutil
import subprocess
import sys

ROOT = pathlib.Path(__file__).resolve().parents[2]
CACHE = ROOT / "build" / "free-sfx-cache" / "music"
OUT = ROOT / "data" / "music"

RATE = 22050

# name -> (url, author, licence, page, what it is)
TRACKS = {
    "depths": (
        "https://opengameart.org/sites/default/files/The%20Depths%20of%20Hell.mp3",
        "Joth", "CC0", "https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "ambiência pesada, um minuto em loop"),
    "infestation": (
        "https://opengameart.org/sites/default/files/Infestation%20in%20the%20Control%20Room.mp3",
        "Joth", "CC0", "https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "sala de controle tomada"),
    "cryptid": (
        "https://opengameart.org/sites/default/files/Cage%20of%20the%20Cryptid.mp3",
        "Joth", "CC0", "https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "algo preso e vivo"),
    "captainslog": (
        "https://opengameart.org/sites/default/files/Final%20Captain%27s%20Log.mp3",
        "Joth", "CC0", "https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "o último registro"),
    "surreal": (
        "https://opengameart.org/sites/default/files/The%20Surreal%20Truth.mp3",
        "Joth", "CC0", "https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
        "estranheza"),
    "wastelands": (
        "https://opengameart.org/sites/default/files/Juhani%20Junkala%20-%20Post%20Apocalyptic%20Wastelands%20%5BLoop%20Ready%5D.ogg",
        "Juhani Junkala", "CC0", "https://opengameart.org/content/horror-atmosphere",
        "cinco minutos, feita para repetir"),
    "darkambience": (
        "https://opengameart.org/sites/default/files/Iwan%20Gabovitch%20-%20Dark%20Ambience%20Loop.ogg",
        "Iwan Gabovitch (qubodup)", "GPL-2.0",
        "https://opengameart.org/content/dark-ambience-loop",
        "loop curto e escuro; dupla licença CC-BY e GPL, tomada sob GPL-2, a do projeto"),
}

# The twelve names the game plays, and what stands in for each.
MAPPING = {
    # Asked for by game.cpp and not by the Lisp: the title screen's music,
    # which is the first thing anyone hears.
    "intro.hmi": "surreal",

    "abuse01.hmi": "depths",
    "abuse02.hmi": "infestation",
    "abuse04.hmi": "cryptid",
    "abuse07.hmi": "captainslog",
    "abuse08.hmi": "surreal",
    "abuse10.hmi": "wastelands",
    "indst1.hmi": "infestation",
    "indst2.hmi": "darkambience",
    "indst3.hmi": "depths",
    "indst4.hmi": "wastelands",
    "indst5.hmi": "cryptid",
}


def fetch(name: str) -> pathlib.Path:
    url = TRACKS[name][0]
    CACHE.mkdir(parents=True, exist_ok=True)
    dst = CACHE / (name + pathlib.Path(url.split("?")[0]).suffix)
    if not dst.exists():
        print(f"fetching {name}")
        subprocess.run(["curl", "-sSL", "-o", str(dst), url], check=True)
    return dst


def manifest_block() -> str:
    lines = [
        "# ============================================================",
        "# Free soundtrack (Remastered mode)",
        "# ============================================================",
        "#",
        "# Built by tools/free-sfx/build-music.py, which holds the mapping.",
        "# Twelve names, seven tracks: the free music that suits this game is",
        "# scarcer than the levels are numerous, so some of them repeat.",
        "#",
        "# The names are the 1995 MIDI names, because that is what the Lisp",
        "# asks for. The music is not the 1995 music, which belongs to Bobby",
        "# Prince and is in no package here.",
        "#",
        "# **Nobody has heard this in the game yet.**",
        "",
    ]
    for target in sorted(MAPPING):
        track = MAPPING[target]
        _, author, licence, page, what = TRACKS[track]
        lines += [
            "[[asset]]",
            f'path = "music/{target.replace(".hmi", ".ogg")}"',
            f'origin = "{page}"',
            f'author = "{author}"',
            f'license = "{licence}"',
            "modified = true",
            f'note = "{what}; mono 22 kHz"',
            "",
        ]
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", action="store_true")
    args = ap.parse_args()

    if args.manifest:
        print(manifest_block())
        return 0

    if not shutil.which("ffmpeg"):
        print("ffmpeg is not installed", file=sys.stderr)
        return 1

    sources = {name: fetch(name) for name in sorted({t for t in MAPPING.values()})}
    OUT.mkdir(parents=True, exist_ok=True)

    for target in sorted(MAPPING):
        track = MAPPING[target]
        dst = OUT / target.replace(".hmi", ".ogg")
        subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
             "-i", str(sources[track]),
             "-ac", "1", "-ar", str(RATE),
             # Level is set on the music bus at runtime; this only evens the
             # tracks out against each other, so one level is not twice as
             # loud as the next.
             "-af", "loudnorm=I=-23:TP=-2:LRA=11",
             "-c:a", "libvorbis", "-q:a", "1", str(dst)],
            check=True)
        print(f"{dst.name:<14} <- {track}")

    total = sum(p.stat().st_size for p in OUT.glob("*.ogg"))
    print(f"\n{len(MAPPING)} tracks, {total / 1024 / 1024:.1f} MiB in data/music/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
