/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See hd.h.
 *
 *  This software was released into the Public Domain.
 */

#include "common.h"

#include "hd.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <filesystem>
#include <map>
#include <string>

// Only the PNG decoder: it is the one format the pack uses, and the rest
// of stb_image is a lot of code to carry for nothing. See
// src/thirdparty/README.md for where this came from.
#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "thirdparty/stb_image.h"

#include "image.h"
#include "palette.h"
#include "specs.h"

extern palette *pal;

// The palette as it was before the brightness calibration, which gamma.cpp
// keeps while `pal` holds the corrected copy. Null until the calibration has
// run.
extern palette *old_pal;

namespace abuse::hd {

namespace {

bool g_enabled = true;
unsigned long long g_variant_seed = 0;

// Where the pack lives, worked out once, as a path open_file understands.
//
// Two notions of path meet here and the first version got them crossed.
// open_file takes a name relative to the data directory and prefixes it
// itself; the filesystem check has to be done on the full path. Looking
// for "data/hd" and then handing that to open_file made it ask for
// "./data/data/hd/...", which simply was not there, and the override was
// refused with no message at all.
std::string const &root()
{
    static bool looked = false;
    static std::string path;

    if (!looked)
    {
        looked = true;
        char const *prefix = get_filename_prefix();
        std::string const full = std::string(prefix ? prefix : "") + "hd";

        std::error_code ec;
        if (std::filesystem::is_directory(full, ec))
        {
            path = "hd";
            printf("HD: override pack at %s\n", full.c_str());
        }
    }
    return path;
}

// "art/fore/techno.spe" -> "techno", which is how spe-export names the
// directory it writes the entries of that file into.
std::string stem_of(char const *spe_path)
{
    std::string s(spe_path ? spe_path : "");
    size_t const slash = s.find_last_of("/\\");
    if (slash != std::string::npos)
        s = s.substr(slash + 1);
    size_t const dot = s.find_last_of('.');
    if (dot != std::string::npos)
        s = s.substr(0, dot);
    return s;
}

// The same scrubbing spe-export does, so the two agree on the filename.
std::string safe_name(char const *name)
{
    std::string s(name ? name : "");
    if (s.size() > 4)
    {
        std::string const tail = s.substr(s.size() - 4);
        if (tail == ".pcx" || tail == ".PCX")
            s = s.substr(0, s.size() - 4);
    }
    for (char &c : s)
    {
        bool const ok = (c >= 'A' && c <= 'Z') || (c >= 'a' && c <= 'z')
                        || (c >= '0' && c <= '9') || c == '_' || c == '-'
                        || c == '.';
        if (!ok)
            c = '_';
    }
    return s;
}

// Which of an entry's variants this run uses.
//
// The choice is made once per entry and remembered, because find() is asked
// again every time the cache reloads the object and a title screen that
// changed while the menu was open would look like a glitch rather than a
// feature.
//
// Not the game's RNG: that one is part of the simulation and drawing must
// not disturb it. splitmix64, the same mixer the dynamic light uses.
int variant_of(std::string const &base, int count)
{
    static std::map<std::string, int> chosen;

    if (count <= 1 || g_variant_seed == 0)
        return 1;

    auto const it = chosen.find(base);
    if (it != chosen.end())
        return it->second;

    unsigned long long x = g_variant_seed;
    for (char const c : base)
        x += (unsigned long long)(unsigned char)c * 0x9E3779B97F4A7C15ull;
    x += 0x9E3779B97F4A7C15ull;
    x = (x ^ (x >> 30)) * 0xBF58476D1CE4E5B9ull;
    x = (x ^ (x >> 27)) * 0x94D049BB133111EBull;
    x ^= x >> 31;

    int const pick = (int)(x % (unsigned long long)count) + 1;
    chosen[base] = pick;
    return pick;
}

// name.png, name.2.png, ... name.9.png, counted from the first gap.
int variant_count(std::string const &base)
{
    int n = 1;
    while (n < 9)
    {
        std::error_code ec;
        std::string const next = base + "." + (char)('1' + n) + ".png";
        if (!std::filesystem::is_regular_file(next, ec))
            break;
        n++;
    }
    return n;
}

}

image *load(char const *path, int want_w, int want_h);

bool enabled()
{
    return g_enabled;
}

void set_enabled(bool on)
{
    g_enabled = on;
}

void set_variant_seed(unsigned long long seed)
{
    g_variant_seed = seed;
}

bool available()
{
    return g_enabled && !root().empty();
}

char *find(char const *spe_path, char const *name)
{
    if (!available() || !name || !name[0])
        return NULL;

    // Relative for open_file, absolute for the check. See root().
    std::string const stem =
        root() + "/" + stem_of(spe_path) + "/" + safe_name(name);

    char const *prefix = get_filename_prefix();
    std::string const full_stem = std::string(prefix ? prefix : "") + stem;

    std::error_code ec;
    if (!std::filesystem::is_regular_file(full_stem + ".png", ec))
        return NULL;

    int const pick = variant_of(stem, variant_count(full_stem));
    std::string const path =
        pick == 1 ? stem + ".png"
                  : stem + "." + (char)('0' + pick) + ".png";

    return strdup(path.c_str());
}

image *swap(char *&path, image *original)
{
    if (!path || !original)
        return original;

    image *replacement = load(path, original->Size().x, original->Size().y);
    if (replacement)
    {
        delete original;
        return replacement;
    }

    // Refused, and load() has said why. Stop asking about this one.
    free(path);
    path = NULL;
    return original;
}

image *load(char const *path, int want_w, int want_h)
{
    if (!path || !pal)
        return NULL;

    // Read it whole rather than let stb open it: the engine's files can
    // come from an overlay directory, and keeping one way in means one
    // place to change when they can come from an archive too.
    bFILE *file = open_file(path, "rb");
    if (file->open_failure())
    {
        delete file;
        return NULL;
    }

    size_t const size = (size_t)file->file_size();
    uint8_t *raw = (uint8_t *)malloc(size);
    if (!raw)
    {
        delete file;
        return NULL;
    }
    file->read(raw, (int32_t)size);
    delete file;

    int w = 0, h = 0, channels = 0;
    stbi_uc *pixels = stbi_load_from_memory(raw, (int)size, &w, &h,
                                            &channels, 4);
    free(raw);

    if (!pixels)
    {
        printf("HD: %s will not decode (%s)\n", path, stbi_failure_reason());
        return NULL;
    }

    if (w != want_w || h != want_h)
    {
        // Said out loud, because the file is obviously meant to be used
        // and silently ignoring it would look like the pack not working.
        printf("HD: %s is %dx%d and the game wants %dx%d; ignored\n",
               path, w, h, want_w, want_h);
        stbi_image_free(pixels);
        return NULL;
    }

    image *out = new image(ivec2(w, h));

    // Matched against the palette before the calibration, not the corrected
    // one. The screen shows these indices through the corrected palette, so
    // a match made against that same palette is undone by it: the picture
    // comes out as the artist painted it whatever the player calibrated, and
    // the Remastered mode's title and menu stayed bright while every
    // original sprite obeyed the setting.
    palette *const source = old_pal ? old_pal : pal;

    // Back to palette indices. Colour 0 is the transparent one throughout
    // the engine, so anything mostly see-through becomes that rather than
    // the nearest colour to whatever was underneath it.
    for (int y = 0; y < h; y++)
    {
        uint8_t *dst = out->scan_line((int16_t)y);
        stbi_uc const *src = pixels + (size_t)y * w * 4;

        for (int x = 0; x < w; x++, src += 4)
            dst[x] = src[3] < 128
                         ? 0
                         : (uint8_t)source->find_closest(src[0], src[1], src[2]);
    }

    stbi_image_free(pixels);
    return out;
}

}
