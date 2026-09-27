#!/usr/bin/env python3
"""Builds the free sound pack for the Remastered mode.

The Lisp asks for `sfx/<name>.wav` and those names are Bobby Prince's files,
which are not redistributable and are not in this repository. The engine
widens the lookup: ask for foo.wav, take foo.ogg if it is there, and never
in the Original mode (see src/audio/formats.h). So a free pack is a set of
.ogg files under data/sfx/ named after the originals, and no Lisp changes.

What this script does:

  1. downloads the source packs into a cache, if they are not there yet
  2. converts one chosen file per event to mono 22 kHz Vorbis
  3. writes data/sfx/<name>.ogg
  4. prints the MANIFEST.toml block for those files

**Nothing here was chosen by ear.** Each line of MAPPING is a candidate
picked by function and duration from the description in docs/audio-map.md,
which is the rule the project set: the reference for a substitute is the
*description* of the event, never the original file. Listening, and vetoing,
is a person's job.

Licence policy, in one line: CC0 and public domain only in this pack, so
nothing here needs a credit line to be redistributable. Sources that permit
use but forbid redistributing the files (Pixabay, Sonniss) are out; the
reasoning is in docs/plan/fase-05-audio.md.

    ./tools/free-sfx/build-pack.py            # build the pack
    ./tools/free-sfx/build-pack.py --manifest # print the manifest block only
"""

import argparse
import pathlib
import shutil
import subprocess
import sys
import tarfile
import zipfile

ROOT = pathlib.Path(__file__).resolve().parents[2]
CACHE = ROOT / "build" / "free-sfx-cache"
OUT = ROOT / "data" / "sfx"

# What the engine was written for; see the treatment in main().
RATE = 11025

# How loud each family ends up, as mean level in dBFS, with a ceiling of
# -1 dBFS on the peak.
#
# One number for everything is what the first pass did, through
# dynaudnorm, and it normalises inside a file rather than between files:
# every sound came out at its own level and the set was uneven. These are
# measured and applied per file, and they are not equal on purpose, because
# an ambience loop that sits at the level of a gunshot is not ambience.
LEVELS = {
    "voice":     -16.0,   # the player and the aliens: the loudest thing here
    "combat":    -17.0,   # weapons, explosions, impacts
    "mechanics": -19.0,   # doors, platforms, switches, the teleporter
    "interface": -19.0,
    "ambience":  -27.0,   # under everything, always
}

FAMILY = {
    "voice": {"plpain01", "plpain02", "plpain04", "plpain10",
              "pldeth02", "pldeth04", "pldeth05", "pldeth07",
              "alien01", "ahit01", "adie05", "adie02", "adie03", "poof05",
              "aslash01", "scream02", "scream03", "scream08", "amb07",
              "amb16"},
    "combat": {"zap2", "zap3", "plasma02", "plasma03", "lasrmis2", "ammo02",
               "rocket02", "firebmb1", "grenad01", "throw01", "elect02",
               "poof06", "mghit01", "mghit02", "crmble01", "blkfoot4",
               "ball01", "aland01"},
    "mechanics": {"doorup01", "doorup02", "swish01", "switch01", "spring03",
                  "eleacc01", "eledec01", "telept01", "timerfst", "fadeon01",
                  "cleaner", "robot02", "speed02", "fly03", "force01",
                  "lava01", "amb10"},
    "interface": {"button02", "save05", "endlvl02", "health01", "ammo01",
                  "logo09", "delobj01", "link01"},
}


def target_level(stem: str) -> float:
    for family, members in FAMILY.items():
        if stem in members:
            return LEVELS[family]
    return LEVELS["ambience"]


def probe_duration(path: pathlib.Path):
    out = subprocess.run(
        ["ffprobe", "-v", "error", "-show_entries", "format=duration",
         "-of", "csv=p=0", str(path)],
        capture_output=True, text=True).stdout.strip()
    try:
        return float(out)
    except ValueError:
        return None


def measure(path: pathlib.Path):
    """Mean and peak level of a file, in dBFS.

    volumedetect and not ebur128: half of these sounds are under half a
    second, which is shorter than the window R128 integrates over, and the
    loudness it reports for them is not a number to trust.
    """
    out = subprocess.run(
        ["ffmpeg", "-hide_banner", "-nostats", "-i", str(path),
         "-af", "volumedetect", "-f", "null", "-"],
        capture_output=True, text=True).stderr

    mean = peak = None
    for line in out.splitlines():
        if "mean_volume:" in line:
            mean = float(line.split("mean_volume:")[1].split("dB")[0])
        elif "max_volume:" in line:
            peak = float(line.split("max_volume:")[1].split("dB")[0])
    return mean, peak

# The packs. Every one of them is CC0 or public domain; the licence was read
# on the page, not deduced from the site.
PACKS = {
    "kenney-scifi": dict(
        url="https://kenney.nl/media/pages/assets/sci-fi-sounds/6b296f9ecf-1677589334/kenney_sci-fi-sounds.zip",
        author="Kenney",
        licence="CC0",
        page="https://kenney.nl/assets/sci-fi-sounds",
    ),
    "kenney-impact": dict(
        url="https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip",
        author="Kenney",
        licence="CC0",
        page="https://kenney.nl/assets/impact-sounds",
    ),
    "kenney-interface": dict(
        url="https://kenney.nl/media/pages/assets/interface-sounds/fa43c1dd4d-1677589452/kenney_interface-sounds.zip",
        author="Kenney",
        licence="CC0",
        page="https://kenney.nl/assets/interface-sounds",
    ),
    "creature": dict(
        url="https://opengameart.org/sites/default/files/80-CC0-creature-SFX_0.zip",
        author="rubberduck",
        licence="CC0",
        page="https://opengameart.org/content/80-cc0-creature-sfx",
    ),
    "sfx100": dict(
        url="https://opengameart.org/sites/default/files/100-CC0-SFX_0.zip",
        author="rubberduck",
        licence="CC0",
        page="https://opengameart.org/content/100-cc0-sfx",
    ),
    "metalwood": dict(
        url="https://opengameart.org/sites/default/files/100-CC0-wood-metal-SFX.zip",
        author="rubberduck",
        licence="CC0",
        page="https://opengameart.org/content/100-cc0-metal-and-wood-sfx",
    ),
    "scifi50": dict(
        url="https://opengameart.org/sites/default/files/sci-fi-sfx.zip",
        author="rubberduck",
        licence="CC0",
        page="https://opengameart.org/content/50-cc0-sci-fi-sfx",
    ),
    # Dual licensed OGA-BY 3.0 and CC0 on the page. Taken under CC0, which
    # is why the pack needs no credit line; crediting anyway costs nothing
    # and data/sfx/CREDITS.md does.
    "yelling": dict(
        url="https://opengameart.org/sites/default/files/yelling%20sounds.zip",
        author="HaelDB",
        licence="CC0",
        page="https://opengameart.org/content/male-gruntyelling-sounds",
    ),
    # The horror library. CC-BY 3.0, so this one is credited, and it is the
    # reason the pack stopped sounding like a mobile game: male screams,
    # monsters, zombies, bone cracks, a knife, a heavy gate and a machine
    # loop from hell.
    "horror": dict(
        url="https://opengameart.org/sites/default/files/Horror%20Sound%20Library.zip",
        author="Little Robot Sound Factory",
        licence="CC-BY-4.0",
        page="https://opengameart.org/content/horror-sound-effects-library",
    ),
    "joth": dict(
        url="https://opengameart.org/sites/default/files/The%20Depths%20of%20Hell.mp3",
        author="Joth",
        licence="CC0",
        page="https://opengameart.org/content/ambience-pack-1-sci-fi-horror",
    ),
    "caverns": dict(
        url="https://opengameart.org/sites/default/files/caverns_0.ogg",
        author="congusbongus",
        licence="CC0",
        page="https://opengameart.org/content/ancient-caverns-horror-ambient-loop",
    ),
    # Crack dot Com released Golgotha into the public domain. It is the only
    # source here with the ambient loops this game needs, and the mapping
    # below is the one docs/audio-map.md proposed in 2026-09-17.
    "golgotha": dict(
        url="http://abuse.zoy.org/raw-attachment/wiki/download/golgotha-sfx.tar.gz",
        author="Crack dot Com",
        licence="public-domain",
        page="http://abuse.zoy.org/wiki/download",
    ),
}

# target .wav name -> (pack, file inside the pack, why)
#
# Second attempt. The first one drew on tidy CC0 game-asset packs and the
# verdict, from the one person who has played this game, was that it sounded
# cheerful and none of it was usable: Abuse is dark, industrial and grim, and
# Kenney's clean interface beeps belong to a different kind of game.
#
# So the sources changed and so did the treatment. Everything below is
# horror, industrial or from Golgotha, which is another Crack dot Com game
# of the same era and carries the same grit, and everything is put through
# the 1995 palette on the way in: mono, 11025 Hz, eight bits, no treble.
# That format is not nostalgia, it is what data/lisp/sfx.lsp documents at
# the top of the file.
MAPPING = {
    # The player. Five male screams become eight files; the ones marked with
    # a pitch are the same scream moved, which is how a set of four reads as
    # four voices and not as one sound repeating.
    "plpain01.wav": ("horror", "Scream_Male_02.wav", "dor do jogador", dict(trim=0.9)),
    "plpain02.wav": ("horror", "Scream_Male_03.wav", "dor do jogador", dict(trim=0.9)),
    "plpain04.wav": ("horror", "Scream_Male_04.wav", "dor do jogador", dict(trim=0.9)),
    "plpain10.wav": ("horror", "Scream_Male_02.wav", "dor do jogador", dict(trim=0.9, pitch=0.92)),
    "pldeth02.wav": ("horror", "Scream_Male_00.wav", "morte do jogador", dict()),
    "pldeth04.wav": ("horror", "Scream_Male_01.wav", "morte do jogador", dict()),
    "pldeth05.wav": ("horror", "Scream_Male_00.wav", "morte do jogador", dict(pitch=0.88)),
    "pldeth07.wav": ("horror", "Scream_Male_01.wav", "morte do jogador", dict(pitch=1.08)),

    # Aliens: monsters and zombies, not cartoon creatures.
    "alien01.wav": ("horror", "Monster_03.wav", "alienígena gritando", dict()),
    "ahit01.wav": ("horror", "Monster_05.wav", "dor de alienígena", dict(trim=0.8)),
    "adie05.wav": ("horror", "Zombie_00.wav", "morte de alienígena pequeno", dict()),
    "adie02.wav": ("horror", "Zombie_02.wav", "morte de alienígena grande", dict()),
    "adie03.wav": ("horror", "Monster_01.wav", "morte grande, também ambiente", dict()),
    "poof05.wav": ("horror", "Zombie_04.wav", "sumiço na morte", dict(trim=1.0)),
    "aslash01.wav": ("horror", "Stab_Knife_01.wav", "corte ou mordida", dict()),
    "aland01.wav": ("golgotha", "misc/vehicle_landing_clank.mp3", "alienígena pousando", dict(trim=1.0)),
    "scream02.wav": ("horror", "Monster_00.wav", "grito ao longe", dict()),
    "scream03.wav": ("horror", "Monster_06.wav", "grito ao longe", dict()),
    "scream08.wav": ("horror", "Zombie_03.wav", "grito ao longe", dict()),
    "amb07.wav": ("horror", "Laugh_Evil_01.wav", "provocação", dict()),
    "amb16.wav": ("horror", "Monster_04.wav", "aparição", dict()),
    "amb10.wav": ("golgotha", "misc/warning_siren_22khz_lp.wav", "alerta, susto", dict()),

    # Weapons, from Golgotha: a 1995 game's artillery, public domain.
    "zap2.wav": ("golgotha", "fire/fire_one_22khz.mp3", "tiro de laser", dict(trim=0.7)),
    "zap3.wav": ("golgotha", "rumble/jet_lp.wav", "nave passando", dict(trim=2.0)),
    "plasma02.wav": ("golgotha", "fire/supertank_plazma_fireball_22khz.mp3", "sabre curto", dict(trim=0.8)),
    "plasma03.wav": ("golgotha", "fire/supergun.mp3", "arma de plasma", dict(trim=1.0)),
    "lasrmis2.wav": ("golgotha", "fire/acid2.mp3", "laser em algo que não morre", dict(trim=0.6)),
    "ammo02.wav": ("golgotha", "fire/machine_gun_fire_22khz_lp.wav", "arma montada inimiga", dict(trim=0.5)),
    "rocket02.wav": ("golgotha", "fire/supertank_fires_rocket_22khz.mp3", "foguete disparado", dict(trim=1.2)),
    "firebmb1.wav": ("golgotha", "explosion/acid.mp3", "bomba incendiária", dict(trim=1.5)),
    "grenad01.wav": ("golgotha", "explosion/generic.mp3", "explodir algo", dict(trim=1.5)),
    "throw01.wav": ("golgotha", "misc/rising_missile_bay.mp3", "granada lançada", dict(trim=0.8)),
    "elect02.wav": ("golgotha", "fire/electric_tower_firing_three_22khz_lp.wav", "eletricidade do chão", dict()),
    "force01.wav": ("golgotha", "misc/electric_car_charged_lp.wav", "campo de força", dict()),
    "lava01.wav": ("golgotha", "ambient/lava_bubbles_22khz_lp.wav", "lava jorrando", dict()),

    # Explosions and what breaks.
    "poof06.wav": ("golgotha", "explosion/shockwave.mp3", "explosão de planeta", dict(trim=2.0)),
    "mghit01.wav": ("horror", "Bonecrack_00.wav", "metralhadora no chão", dict(trim=0.5)),
    "mghit02.wav": ("horror", "Bonecrack_01.wav", "metralhadora no chão", dict(trim=0.5)),
    "crmble01.wav": ("horror", "Bonecrack_02.wav", "bloco desmoronando", dict()),
    "blkfoot4.wav": ("golgotha", "explosion/old_generic.mp3", "parede oculta sumindo", dict(trim=1.2)),
    "ball01.wav": ("metalwood", "metal_hit_01.ogg", "quique de bola de aço", dict()),

    # The level's machinery: heavy, mechanical, and none of it bright.
    "doorup01.wav": ("horror", "Gate_Open_00.wav", "porta subindo", dict(trim=1.6)),
    "doorup02.wav": ("golgotha", "misc/missile_truck_lower.mp3", "porta descendo", dict(trim=1.6)),
    "swish01.wav": ("golgotha", "misc/missile_truck_raise.mp3", "porta abrindo", dict(trim=1.4)),
    "switch01.wav": ("golgotha", "misc/click_one_22khz.mp3", "interruptor", dict()),
    "spring03.wav": ("golgotha", "misc/rotating_turret.mp3", "mola", dict(trim=0.7)),
    "eleacc01.wav": ("golgotha", "rumble/engineering_vehicle_lp.wav", "plataforma acelerando", dict(trim=2.0)),
    "eledec01.wav": ("golgotha", "misc/supertank_refuel_lp.wav", "plataforma desacelerando", dict(trim=2.0)),
    "telept01.wav": ("golgotha", "fire/electric_tower_charge_up_22khz.mp3", "teletransporte", dict(trim=1.5)),
    "timerfst.wav": ("golgotha", "misc/click_two_22khz.mp3", "tique da bomba", dict(trim=0.3)),
    "fadeon01.wav": ("golgotha", "fire/electric_tower_power_down_22khz.mp3", "luz esmaecendo", dict(trim=1.5)),
    "cleaner.wav": ("horror", "Evil_Machine_Loop_00.wav", "o limpador", dict(trim=4.0)),
    "robot02.wav": ("golgotha", "misc/turbine1_lp.wav", "robô voador", dict(trim=3.0)),
    "speed02.wav": ("golgotha", "misc/missle_in_flight_22khz.mp3", "velocidade", dict(trim=1.5)),
    "fly03.wav": ("golgotha", "rumble/helicopter_lp.wav", "voo", dict(trim=3.0)),

    # Interface. Mechanical clicks and machine noise, because a chime in this
    # game sounds like a different game.
    "button02.wav": ("golgotha", "misc/click_one_22khz.mp3", "botão do menu", dict(trim=0.3)),
    "save05.wav": ("golgotha", "misc/main_barrel_refuel.mp3", "salvar no console", dict(trim=1.2)),
    "endlvl02.wav": ("golgotha", "misc/rising_missile_bay.mp3", "fim do nível", dict(trim=2.0)),
    "health01.wav": ("golgotha", "misc/main_missile_refuel.mp3", "ganhar vida", dict(trim=1.0)),
    "ammo01.wav": ("golgotha", "misc/supertank_missile_refuel.mp3", "pegar munição", dict(trim=1.0)),
    "logo09.wav": ("horror", "Ambience_Hell_00.wav", "abertura", dict(trim=3.0)),
    "delobj01.wav": ("golgotha", "misc/click_two_22khz.mp3", "editor: apagar objeto", dict(trim=0.3)),
    "link01.wav": ("golgotha", "misc/click_one_22khz.mp3", "editor: ligar objetos", dict(trim=0.3, pitch=1.2)),

    # Ambience. The horror library's three hells, the caverns loop the plan
    # had already cleared, and Golgotha's wind and water for the rest.
    "ambtech1.wav": ("horror", "Evil_Machine_Loop_00.wav", "ambiente industrial", dict(trim=8.0)),
    "ambtech2.wav": ("golgotha", "rumble/engineering_vehicle_lp.wav", "ambiente industrial", dict(trim=6.0)),
    "ambtech3.wav": ("golgotha", "rumble/electric_car_lp.wav", "ambiente industrial", dict(trim=6.0)),
    "ambcave1.wav": ("caverns", "caverns_0.ogg", "caverna", dict(trim=10.0)),
    "ambcave2.wav": ("horror", "Ambience_Hell_01.wav", "caverna, inferno", dict(trim=8.0)),
    "ambcave3.wav": ("golgotha", "ambient/fast_dripping_water_22khz_lp.wav", "caverna, água", dict(trim=6.0)),
    "ambcave4.wav": ("golgotha", "ambient/wind_and_water_tunnel_mix_22khz_lp.wav", "caverna, túnel", dict(trim=6.0)),
    "ambfrst2.wav": ("horror", "Ambience_MurderofCrows_00.wav", "fora, corvos", dict(trim=8.0)),
    "amb11.wav": ("horror", "Ambience_Hell_02.wav", "aberto, inferno", dict(trim=8.0)),
    "amb13.wav": ("golgotha", "ambient/gushing_wind_22khz_lp.wav", "aberto, vento", dict(trim=6.0)),
}


def fetch(name: str) -> pathlib.Path:
    """Downloads and unpacks one source, once. Some are a single file."""
    spec = PACKS[name]
    CACHE.mkdir(parents=True, exist_ok=True)
    target = CACHE / name
    if target.is_dir():
        return target

    suffix = pathlib.Path(spec["url"].split("?")[0]).suffix
    archive = CACHE / (name + suffix)
    if not archive.exists():
        print(f"fetching {name}")
        subprocess.run(["curl", "-sSL", "-o", str(archive), spec["url"]], check=True)

    target.mkdir(parents=True)
    if suffix == ".zip":
        with zipfile.ZipFile(archive) as z:
            z.extractall(target)
    elif suffix in (".gz", ".tgz", ".xz", ".bz2"):
        with tarfile.open(archive) as t:
            t.extractall(target)
    else:
        shutil.copy(archive, target / pathlib.Path(spec["url"]).name.replace("%20", "_"))
    return target


def find(root: pathlib.Path, wanted: str) -> pathlib.Path:
    """The file inside a source, by name or by the tail of its path.

    Anything starting with "._" is skipped: the horror library was zipped on
    a Mac and carries a resource fork beside every file, which matches the
    name and is not audio.
    """
    for p in sorted(root.rglob("*")):
        if not p.is_file() or p.name.startswith("._"):
            continue
        if p.name == wanted or p.as_posix().endswith("/" + wanted):
            return p
    raise SystemExit(f"not in the pack: {wanted} under {root}")


def readable(src: pathlib.Path, tmp: pathlib.Path) -> pathlib.Path:
    """Works around Golgotha's WAV headers.

    Those files declare format tag 3, IEEE float, and then carry 16 bit PCM:
    blockAlign is 2 and bitsPerSample is 16, which no float format can be.
    ffmpeg reads the tag, finds no decoder for it and refuses the file. The
    samples are fine; only the tag is wrong, so the fix is to write a copy
    with the tag set to 1 and convert that.
    """
    if src.suffix.lower() != ".wav":
        return src

    raw = bytearray(src.read_bytes())
    if len(raw) < 24 or raw[0:4] != b"RIFF" or raw[8:12] != b"WAVE":
        return src

    fmt = int.from_bytes(raw[20:22], "little")
    bits = int.from_bytes(raw[34:36], "little")
    if fmt != 3 or bits != 16:
        return src

    raw[20:22] = (1).to_bytes(2, "little")
    tmp.write_bytes(raw)
    return tmp


def manifest_block() -> str:
    lines = [
        "# ============================================================",
        "# Free sound pack (Remastered mode)",
        "# ============================================================",
        "#",
        "# Built by tools/free-sfx/build-pack.py, which holds the mapping and",
        "# the reason for each choice. Every file is CC0 or public domain, so",
        "# the pack needs no credit line to be redistributable; the credits",
        "# are in data/sfx/CREDITS.md anyway.",
        "#",
        "# The names are Bobby Prince's, because that is what the Lisp asks",
        "# for; the contents are not his and never were. Nothing under",
        "# classic/ was read, converted or used as a reference: each choice",
        "# comes from the written description of the event in",
        "# docs/audio-map.md.",
        "#",
        "# **Nobody has heard these yet.**",
        "",
    ]
    for target in sorted(MAPPING):
        pack, source, why, _ = MAPPING[target]
        spec = PACKS[pack]
        out = target.replace(".wav", ".ogg")
        lines += [
            "[[asset]]",
            f'path = "sfx/{out}"',
            f'origin = "{spec["page"]}"',
            f'author = "{spec["author"]}"',
            f'license = "{spec["licence"]}"',
            "modified = true",
            f'note = "{why}; 11 kHz mono 8 bits, nível normalizado por família"',
            "",
        ]
    return "\n".join(lines)


def credits_file() -> str:
    used = sorted({pack for pack, _, _, _ in MAPPING.values()})
    lines = [
        "# Free sound pack",
        "",
        "Every file here is CC0 or public domain. Nothing in it requires a",
        "credit; this file exists because saying where a thing came from is",
        "worth more than the obligation to say it.",
        "",
        "| Pack | Author | Licence |",
        "| --- | --- | --- |",
    ]
    for pack in used:
        spec = PACKS[pack]
        lines.append(f'| [{pack}]({spec["page"]}) | {spec["author"]} | {spec["licence"]} |')
    lines += [
        "",
        "The file names are the ones the 1995 Lisp asks for. The sounds are",
        "not the 1995 sounds: those belong to Bobby Prince, are not",
        "redistributable, and are in no package here. The Original mode plays",
        "his, from the data the player installs; this pack is what the",
        "Remastered mode plays instead.",
        "",
        "Built by `tools/free-sfx/build-pack.py`, which holds the mapping.",
        "",
    ]
    return "\n".join(lines)


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("--manifest", action="store_true",
                    help="print the manifest block and do nothing else")
    args = ap.parse_args()

    if args.manifest:
        print(manifest_block())
        return 0

    if not shutil.which("ffmpeg"):
        print("ffmpeg is not installed", file=sys.stderr)
        return 1

    roots = {name: fetch(name) for name in sorted({p for p, _, _, _ in MAPPING.values()})}
    OUT.mkdir(parents=True, exist_ok=True)

    for target in sorted(MAPPING):
        pack, source, _, opts = MAPPING[target]
        src = readable(find(roots[pack], source), CACHE / "fixed.wav")
        dst = OUT / target.replace(".wav", ".ogg")

        # The 1995 palette, and the reason it is here: the first pack was
        # made of clean 22 kHz studio recordings and sounded like a
        # different, brighter game. sfx.lsp says it in its fourth line,
        # "samples should be 8 bit mono playing 11025 Hz", so that is what
        # everything is put through on the way in.
        chain = ["lowpass=f=4800", "acrusher=bits=8:mode=lin:aa=1"]
        pitch = opts.get("pitch")
        if pitch:
            # asetrate moves pitch and speed together, which is what a tape
            # does and what makes a second scream read as a second person;
            # aresample puts the rate back.
            chain.insert(0, f"asetrate={int(RATE * pitch)},aresample={RATE}")

        # Treated first, into a plain WAV, because the level has to be
        # measured after the treatment and before the encoder.
        stage = CACHE / "stage.wav"
        args = ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
                "-i", str(src)]
        if opts.get("trim"):
            args += ["-t", str(opts["trim"])]
        args += ["-ac", "1", "-ar", str(RATE), "-af", ",".join(chain),
                 str(stage)]
        subprocess.run(args, check=True)

        stem = target.replace(".wav", "")
        mean, peak = measure(stage)
        want = target_level(stem)
        # A short percussive sound is heard as its transient, not as its
        # average: a bone crack is a spike with silence around it, and the
        # mean over the file says almost nothing about how loud it lands.
        # Under six tenths of a second the peak decides instead, which is
        # also what keeps a quiet recording from being lifted twenty-eight
        # decibels and bringing its own hiss with it.
        duration = probe_duration(stage)
        gain = 0.0
        if duration is not None and duration < 0.6 and peak is not None:
            gain = -3.0 - peak
        elif mean is not None:
            # The mean decides, and the limiter after it holds the ceiling.
            #
            # Letting the peak decide instead, which is what the first
            # attempt did, leaves a short percussive sound far below its
            # family: a gunshot is mostly silence around one spike, so its
            # peak hits the ceiling while its level is still ten decibels
            # too low. Voice and combat came out with a spread of 2 and 5
            # dB that way, against half a decibel for the steady sounds.
            gain = max(-24.0, min(24.0, want - mean))

        subprocess.run(
            ["ffmpeg", "-hide_banner", "-loglevel", "error", "-y",
             "-i", str(stage),
             "-af", f"volume={gain:.2f}dB,alimiter=limit=0.85",
             "-c:a", "libvorbis", "-q:a", "2", str(dst)],
            check=True)
        print(f"{dst.name:<16} <- {pack}/{source}"
              f"   ({mean:+.1f} dB -> {want:+.1f}, ganho {gain:+.1f})")

    (OUT / "CREDITS.md").write_text(credits_file(), encoding="utf-8")
    total = sum(p.stat().st_size for p in OUT.glob("*.ogg"))
    print(f"\n{len(MAPPING)} sounds, {total / 1024:.0f} KiB in data/sfx/")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
