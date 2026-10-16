/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "compat.h"
#include <vector>

#include "harness.h"

#include "data/paths.h"

#include "demo.h"
#include "game.h"
#include "level.h"
#include "loadgame.h"
#include "clisp.h"
#include "lisp.h"
#include "dev.h"
#include "jwindow.h"
#include "keys.h"
#include "objects.h"
#include "chars.h"
#include "items.h"
#include "loader2.h"
#include "jrand.h"
#include "video.h"
#include "ui/options_screen.h"
#include "ui/classic_data_screen.h"
#include "ui/language_screen.h"
#include "render/lightmap.h"
#include "render/dynlight.h"
#include "render/options.h"
#include "ui/start_menu.h"
#include "ui/hud.h"
#include "configuration.h"
#include "lisp.h"
#include "dev.h"
#include "lisp_gc.h"

namespace abuse::harness {

namespace {

struct Options {
    bool headless = false;
    bool want_hash = false;
    bool dump_bindings = false;
    bool dump_window = false;
    bool mode_given = false;
    bool level_info = false;
    bool tile_dump = false;
    bool dynlight_dump = false;
    bool player_dump = false;
    bool rgb_light = false;
    bool scanlines = false;
    float bloom = 0.0f;
    abuse::render::Backend backend = abuse::render::Backend::Classic;
    bool backend_given = false;
    float bloom_threshold = 0.0f;
    bool bloom_threshold_given = false;
    bool save_test = false;
    bool particle_demo = false;
    bool dynlight_demo = false;
    bool save_dialog = false;
    int viewport_w = 0;
    int viewport_h = 0;
    // Interpolation is off under the harness, because one frame is one tick
    // there. This forces a blend anyway, at a fixed point, so a scripted
    // frame can show what the player would see between two ticks.
    float frame_alpha = 0.0f;
    bool frame_alpha_given = false;
    enum class Screen { None, Options, Rebind, ClassicData, MenuHint, Language,
                        StartMenu, Hud };
    Screen dump_screen = Screen::None;
    int window_w = 0;
    int window_h = 0;
    long hash_every = 0;        // 0 = only when the run ends
    // Capture every Nth frame. Nothing on this desktop can film the game:
    // the session is Wayland, ffmpeg here has no pipewire input, and
    // x11grab sees the XWayland root, which is black. The game filming
    // itself is the way in, and the frames come out already cropped to the
    // window with no desktop around them.
    long dump_every = 0;
    long seed = -1;             // < 0 = leave jrand_init's value alone
    long max_ticks = -1;        // < 0 = no limit
    char *record = nullptr;
    char *playback = nullptr;
    char *level = nullptr;
    std::vector<long> dump_ticks;
    // Frames are counted by the loop, which in a windowed run means once
    // per drawn frame and so depends on how fast the machine is. These are
    // level ticks instead: with interpolation off, which is the default,
    // every frame drawn inside one tick is the same picture, so the first
    // frame at or after a given tick is the same picture everywhere.
    std::vector<long> dump_level_ticks;
    std::vector<long> dumped_level_ticks;
    char const *out_dir = ".";
};

Options opt;
bool demo_running = false;

// Counted here rather than read from the level: at the title screen there is
// no level, and its tick counter would sit at zero forever.
uint64_t loop_ticks = 0;

// Playback ends by tearing the level down, so the hash has to be captured
// while it still exists rather than recomputed at the end of the run.
bool have_last = false;
uint64_t last_hash = 0;
uint64_t last_loop = 0;
uint32_t last_level_tick = 0;

// Consumes the value that follows a switch, or dies: a scripted run that
// silently ignores a malformed argument is worse than one that stops.
char *take_value(int argc, char **argv, int &i, char const *what)
{
    if (i + 1 >= argc)
    {
        fprintf(stderr, "%s requires an argument\n", what);
        exit(2);
    }
    return argv[++i];
}

// A fraction, for the one flag that takes one. Same refusal as take_number:
// a typo stops the run rather than being read as zero.
double take_fraction(int argc, char **argv, int &i, char const *what)
{
    char *end;
    char *raw = take_value(argc, argv, i, what);
    double value = strtod(raw, &end);
    if (*end || end == raw)
    {
        fprintf(stderr, "%s expects a number, got '%s'\n", what, raw);
        exit(2);
    }
    return value;
}

long take_number(int argc, char **argv, int &i, char const *what)
{
    char *end;
    char *raw = take_value(argc, argv, i, what);
    long value = strtol(raw, &end, 10);
    if (*end || end == raw)
    {
        fprintf(stderr, "%s expects a number, got '%s'\n", what, raw);
        exit(2);
    }
    return value;
}

// The engine resolves relative names against the data directory prefix, which
// is right for game assets and wrong for a path the user typed. Make replay
// paths absolute so they mean what the shell means.
namespace {

// One entry per line of the script: what is held down, and for how long.
struct ScriptStep
{
    uint8_t flags = 0;
    int ticks = 0;
};

std::vector<ScriptStep> g_script;
size_t g_script_at = 0;
int g_script_left = 0;

// The packet layout, which is also what to_flags produces: see
// abuse::input::to_flags and view::get_input.
bool name_to_flag(char const *name, uint8_t &bit)
{
    struct { char const *name; uint8_t bit; } const table[] = {
        { "right",   1 },
        { "left",    2 },
        { "down",    4 },
        { "up",      8 },
        { "jump",    8 },
        { "fire",    16 },
        { "special", 32 },
        { "none",    0 },
    };

    for (size_t i = 0; i < sizeof(table) / sizeof(table[0]); i++)
        if (!strcasecmp(name, table[i].name))
        {
            bit = table[i].bit;
            return true;
        }
    return false;
}

void load_input_script(char const *path)
{
    FILE *f = fopen(path, "r");
    if (!f)
    {
        fprintf(stderr, "unable to read the input script '%s'\n", path);
        exit(2);
    }

    char line[512];
    int number = 0;
    while (fgets(line, sizeof(line), f))
    {
        number++;

        char *hash = strchr(line, '#');
        if (hash)
            *hash = 0;

        char *word = strtok(line, " \t\r\n");
        if (!word)
            continue;

        ScriptStep step;
        step.ticks = atoi(word);
        if (step.ticks < 1)
        {
            fprintf(stderr, "%s:%d: expected a tick count, got '%s'\n",
                    path, number, word);
            exit(2);
        }

        while ((word = strtok(NULL, " \t\r\n")) != NULL)
        {
            uint8_t bit = 0;
            if (!name_to_flag(word, bit))
            {
                fprintf(stderr, "%s:%d: unknown action '%s'\n",
                        path, number, word);
                exit(2);
            }
            step.flags |= bit;
        }

        g_script.push_back(step);
    }
    fclose(f);

    if (g_script.empty())
    {
        fprintf(stderr, "the input script '%s' has no steps\n", path);
        exit(2);
    }
}

}

char *absolute(char *path)
{
    if (!path || path[0] == '/')
        return path;

    char cwd[4096];
    if (!getcwd(cwd, sizeof(cwd)))
        return path;

    size_t len = strlen(cwd) + 1 + strlen(path) + 1;
    char *full = (char *)malloc(len);
    if (!full)
        return path;
    snprintf(full, len, "%s/%s", cwd, path);
    return full;
}

void parse_into(char const *raw, char const *flag, std::vector<long> &out)
{
    char const *p = raw;
    while (*p)
    {
        char *end;
        long tick = strtol(p, &end, 10);
        if (end == p)
        {
            fprintf(stderr, "%s expects a comma separated list, got '%s'\n",
                    flag, raw);
            exit(2);
        }
        out.push_back(tick);
        p = end;
        while (*p == ',')
            p++;
    }
}

void parse_tick_list(char const *raw)
{
    parse_into(raw, "--dump-frames", opt.dump_ticks);
}

void parse_level_tick_list(char const *raw)
{
    parse_into(raw, "--dump-tick", opt.dump_level_ticks);
}

// FNV-1a. The state is integer only, so no float formatting ambiguity.
inline void mix(uint64_t &h, uint64_t value)
{
    for (int i = 0; i < 8; i++)
    {
        h ^= (value >> (i * 8)) & 0xff;
        h *= 0x100000001b3ULL;
    }
}

}

void parse_args(int argc, char **argv)
{
    for (int i = 1; i < argc; i++)
    {
        if (!strcmp(argv[i], "--headless"))
            opt.headless = true;
        else if (!strcmp(argv[i], "--state-hash"))
            opt.want_hash = true;
        else if (!strcmp(argv[i], "--dump-bindings"))
            opt.dump_bindings = true;
        else if (!strcmp(argv[i], "--dump-window"))
            opt.dump_window = true;
        else if (!strcmp(argv[i], "--dump-options"))
            opt.dump_screen = Options::Screen::Options;
        else if (!strcmp(argv[i], "--dump-controls"))
            opt.dump_screen = Options::Screen::Rebind;
        else if (!strcmp(argv[i], "--dump-classic-data"))
            opt.dump_screen = Options::Screen::ClassicData;
        else if (!strcmp(argv[i], "--dump-menu-hint"))
            opt.dump_screen = Options::Screen::MenuHint;
        else if (!strcmp(argv[i], "--dump-language"))
            opt.dump_screen = Options::Screen::Language;
        else if (!strcmp(argv[i], "--dump-start-menu"))
            opt.dump_screen = Options::Screen::StartMenu;
        else if (!strcmp(argv[i], "--dump-hud"))
        {
            opt.dump_screen = Options::Screen::Hud;
            // Before anything draws, not at capture time: the classic strip
            // goes into the 320x200 buffer, and by then it is already there.
            abuse::ui::set_classic_hud(false);
        }
        else if (!strcmp(argv[i], "--particle-demo"))
            opt.particle_demo = true;
        else if (!strcmp(argv[i], "--dynlight-demo"))
            opt.dynlight_demo = true;
        else if (!strcmp(argv[i], "--renderer"))
        {
            char const *name = take_value(argc, argv, i, "--renderer");
            abuse::render::Backend b;
            if (!abuse::render::parse_backend(name, b))
            {
                fprintf(stderr, "unknown renderer '%s', expected classic or "
                                "gpu\n", name);
                exit(2);
            }
            opt.backend = b;
            opt.backend_given = true;
        }
        else if (!strcmp(argv[i], "--bloom"))
            opt.bloom = (float)take_fraction(argc, argv, i, "--bloom");
        else if (!strcmp(argv[i], "--bloom-threshold"))
        {
            opt.bloom_threshold =
                (float)take_fraction(argc, argv, i, "--bloom-threshold");
            opt.bloom_threshold_given = true;
        }
        else if (!strcmp(argv[i], "--scanlines"))
            opt.scanlines = true;
        else if (!strcmp(argv[i], "--save-test"))
            opt.save_test = true;
        else if (!strcmp(argv[i], "--save-dialog"))
            opt.save_dialog = true;
        else if (!strcmp(argv[i], "--viewport"))
        {
            opt.viewport_w = (int)take_number(argc, argv, i, "--viewport");
            opt.viewport_h = (int)take_number(argc, argv, i, "--viewport");
        }
        else if (!strcmp(argv[i], "--dump-player"))
            opt.player_dump = true;
        else if (!strcmp(argv[i], "--dump-tiles"))
            opt.tile_dump = true;
        else if (!strcmp(argv[i], "--dump-dynlight"))
            opt.dynlight_dump = true;
        else if (!strcmp(argv[i], "--level-info"))
            opt.level_info = true;
        else if (!strcmp(argv[i], "--input-script"))
            load_input_script(take_value(argc, argv, i, "--input-script"));
        else if (!strcmp(argv[i], "--rgb-light"))
            opt.rgb_light = true;
        else if (!strcmp(argv[i], "--frame-alpha"))
        {
            opt.frame_alpha = (float)take_fraction(argc, argv, i, "--frame-alpha");
            opt.frame_alpha_given = true;
        }
        else if (!strcmp(argv[i], "--window-size"))
        {
            opt.window_w = (int)take_number(argc, argv, i, "--window-size");
            opt.window_h = (int)take_number(argc, argv, i, "--window-size");
        }
        else if (!strcmp(argv[i], "--dump-every"))
            opt.dump_every = take_number(argc, argv, i, "--dump-every");
        else if (!strcmp(argv[i], "--hash-every"))
            opt.hash_every = take_number(argc, argv, i, "--hash-every");
        else if (!strcmp(argv[i], "--seed"))
            opt.seed = take_number(argc, argv, i, "--seed");
        else if (!strcmp(argv[i], "--max-ticks"))
            opt.max_ticks = take_number(argc, argv, i, "--max-ticks");
        else if (!strcmp(argv[i], "--record"))
            opt.record = take_value(argc, argv, i, "--record");
        else if (!strcmp(argv[i], "--playback"))
            opt.playback = take_value(argc, argv, i, "--playback");
        else if (!strcmp(argv[i], "--level"))
            opt.level = take_value(argc, argv, i, "--level");
        else if (!strcmp(argv[i], "--dump-tick"))
            parse_level_tick_list(take_value(argc, argv, i, "--dump-tick"));
        else if (!strcmp(argv[i], "--dump-frames"))
            parse_tick_list(take_value(argc, argv, i, "--dump-frames"));
        else if (!strcmp(argv[i], "--out"))
            opt.out_dir = take_value(argc, argv, i, "--out");
        else if (!strcmp(argv[i], "--mode"))
        {
            char *value = take_value(argc, argv, i, "--mode");
            abuse::data::Mode m;
            if (abuse::data::parse_mode(value, m))
            {
                abuse::data::set_mode(m);
                opt.mode_given = true;
            }
            else
            {
                fprintf(stderr, "--mode expects original or remaster, got '%s'\n",
                        value);
                exit(2);
            }
        }
        else if (!strcmp(argv[i], "--classic-data"))
            abuse::data::set_classic_data(
                take_value(argc, argv, i, "--classic-data"));
    }

    if (opt.record && opt.playback)
    {
        fprintf(stderr, "--record and --playback are mutually exclusive\n");
        exit(2);
    }

    opt.record = absolute(opt.record);
    opt.playback = absolute(opt.playback);

    if (opt.record && !opt.level)
    {
        // start_recording() needs a level; without one it would fail at a
        // point where the only recourse is to read the source.
        fprintf(stderr, "--record also needs --level <file>\n");
        exit(2);
    }

    if (opt.headless && !opt.level && !opt.playback)
    {
        // main_menu() is a modal loop of its own, so tick() never runs and
        // --max-ticks cannot fire. Say so rather than hang silently.
        fprintf(stderr, "warning: --headless without --level or --playback "
                        "stops at the main menu and will not exit\n");
    }

    if (opt.headless)
    {
        // Set before SDL_Init, which setup() calls.
        abuse::set_env("SDL_VIDEO_DRIVER", "dummy");
        abuse::set_env("SDL_VIDEODRIVER", "dummy");
        abuse::set_env("SDL_AUDIO_DRIVER", "dummy");
        abuse::set_env("SDL_AUDIODRIVER", "dummy");
    }
}

bool window_size(int &w, int &h)
{
    if (opt.window_w < 1 || opt.window_h < 1)
        return false;
    w = opt.window_w;
    h = opt.window_h;
    return true;
}

bool viewport_size(int &w, int &h)
{
    // Headless only. A player asking for this would get a game whose status
    // bar is in the wrong place; a measurement wants exactly that picture.
    if (!opt.headless || opt.viewport_w < 64 || opt.viewport_h < 64)
        return false;
    w = opt.viewport_w;
    h = opt.viewport_h;
    return true;
}

bool want_tile_dump()
{
    return opt.tile_dump;
}

bool want_dynlight_dump()
{
    return opt.dynlight_dump;
}

void print_dynlight_dump()
{
    if (!opt.dynlight_dump)
        return;

    std::vector<abuse::render::Emitter> const &table
        = abuse::render::emitters();

    printf("dynlight-dump entries=%d types=%d\n", (int)table.size(),
           total_objects);
    printf("dynlight-dump name radius strength r g b flicker otype\n");

    for (size_t i = 0; i < table.size(); i++)
    {
        int otype = -1;
        for (int t = 0; t < total_objects; t++)
            if (object_names[t] && !strcmp(object_names[t], table[i].name.c_str()))
            {
                otype = t;
                break;
            }

        // An entry naming a type this data set does not define is not an
        // error: the shareware data aliases some bullets onto others, and
        // the table is written once for every data set there is.
        printf("dynlight %s %d %d %d %d %d %d %d\n", table[i].name.c_str(),
               table[i].radius, table[i].strength, table[i].r, table[i].g,
               table[i].b, table[i].flicker, otype);
    }
}

bool want_player_dump()
{
    return opt.player_dump;
}

void print_player_dump(int tick, int x, int y, int xvel, int yvel)
{
    if (!opt.player_dump)
        return;
    printf("player %d x=%d y=%d xvel=%d yvel=%d\n", tick, x, y, xvel, yvel);
}

void print_tile_dump()
{
    if (!opt.tile_dump)
        return;

    printf("tile-dump count=%d size=%dx%d\n", nforetiles, f_wid, f_hi);
    printf("tile-dump id points x0 y0 x1 y1 damage\n");

    // foretile::ylevel is documented as the ground offset and is never
    // written by anything, so it is not reported here. The collision that
    // does exist is the boundary polygon, and its bounding box is what a
    // map compiler needs: a full block covers the tile, a ledge is a few
    // rows at the top, and a tile with no points at all is walked through.
    for (int i = 0; i < nforetiles; i++)
    {
        foretile *f = the_game->get_fg(i);
        if (!f)
            continue;

        int points = (f->points && f->points->data) ? f->points->tot : 0;
        int x0 = 0, y0 = 0, x1 = -1, y1 = -1;

        for (int p = 0; p < points; p++)
        {
            int px = f->points->data[p * 2];
            int py = f->points->data[p * 2 + 1];
            if (x1 < 0) { x0 = x1 = px; y0 = y1 = py; }
            if (px < x0) x0 = px;
            if (px > x1) x1 = px;
            if (py < y0) y0 = py;
            if (py > y1) y1 = py;
        }

        printf("tile-dump %4d %6d %3d %3d %3d %3d %6d\n",
               i, points, x0, y0, x1, y1, (int)f->damage);
    }
}

bool want_level_info()
{
    return opt.level_info;
}

void print_level_info(int fg_tiles_x, int fg_tiles_y, int tile_w, int tile_h,
                      int bg_tiles_x, int bg_tiles_y, int bg_tile_w,
                      int bg_tile_h, int bg_empty)
{
    if (!opt.level_info)
        return;

    int const px_w = fg_tiles_x * tile_w;
    int const px_h = fg_tiles_y * tile_h;

    // The game draws 320 by 200 and the logical presentation stretches that
    // to 320 by 240, so the aspect the player sees is 4:3. A wider viewport
    // shows more level, and the number that matters is how much more.
    struct { char const *name; int w; } const views[] = {
        { "4:3",  320 },
        { "16:10", 400 },
        { "16:9",  427 },
        { "21:9",  560 },
    };

    printf("level-info tiles=%dx%d tile=%dx%d pixels=%dx%d\n",
           fg_tiles_x, fg_tiles_y, tile_w, tile_h, px_w, px_h);

    for (size_t i = 0; i < sizeof(views) / sizeof(views[0]); i++)
    {
        // A camera centred on the player cannot show more than the level
        // has; at the edges it clamps, and what it would have shown beyond
        // them is whatever the level does not draw.
        int const margin = px_w - views[i].w;
        printf("level-info view=%-5s width=%3d fits=%s margin=%d\n",
               views[i].name, views[i].w,
               margin >= 0 ? "yes" : "NO", margin);
    }

    printf("level-info height=%d view=200 fits=%s margin=%d\n",
           px_h, px_h >= 200 ? "yes" : "NO", px_h - 200);

    // The background is the layer a wider view exposes. Where it has no tile
    // the screen shows nothing at all, and in 4:3 those places may simply
    // never have been on camera.
    int const bg_total = bg_tiles_x * bg_tiles_y;
    printf("level-info background tiles=%dx%d tile=%dx%d pixels=%dx%d"
           " empty=%d of %d (%d%%)\n",
           bg_tiles_x, bg_tiles_y, bg_tile_w, bg_tile_h,
           bg_tiles_x * bg_tile_w, bg_tiles_y * bg_tile_h,
           bg_empty, bg_total,
           bg_total > 0 ? bg_empty * 100 / bg_total : 0);
}

bool scripted_input(uint8_t &flags)
{
    if (g_script.empty())
        return false;

    // Past the end the player simply stands still, so a run can outlive its
    // script without the last key staying held down.
    if (g_script_at >= g_script.size())
    {
        flags = 0;
        return true;
    }

    if (g_script_left <= 0)
        g_script_left = g_script[g_script_at].ticks;

    flags = g_script[g_script_at].flags;

    if (--g_script_left <= 0)
        g_script_at++;

    return true;
}

bool frame_alpha(float &out)
{
    if (!opt.frame_alpha_given)
        return false;
    out = opt.frame_alpha;
    return true;
}

bool mode_from_command_line()
{
    return opt.mode_given;
}

bool headless()
{
    return opt.headless;
}

bool scripted_run()
{
    return opt.headless || opt.playback || opt.record || !g_script.empty();
}

bool input_is_scripted()
{
    return opt.headless || opt.playback || !g_script.empty();
}

bool save_dialog_wanted()
{
    return opt.save_dialog;
}

void apply_seed()
{
    if (opt.seed >= 0)
        rand_on = (unsigned short)(opt.seed & (RAND_TABLE_SIZE - 1));
}

void before_game()
{
    // These two are applied here and not where they are parsed.
    //
    // parse_args runs before setup(), because --headless has to pick the
    // dummy drivers before SDL_Init. setup() then reads the command line
    // and abuserc, and a preset there resets the look: with the old order
    // a -preset on the same line quietly undid --rgb-light, which is how
    // two snapshots lost their lighting the day presets grew teeth.
    if (opt.rgb_light)
        abuse::render::set_rgb_lighting(true);
    if (opt.scanlines)
        abuse::render::options().scanlines = true;
    if (opt.bloom > 0.0f)
        abuse::render::options().bloom = opt.bloom;
    if (opt.bloom_threshold_given)
        abuse::render::options().bloom_threshold = opt.bloom_threshold;
    if (opt.backend_given)
        abuse::render::options().backend = opt.backend;

    if (!opt.headless)
        return;

    // gamma_correct() shows a modal calibration screen unless darkest_gray is
    // already defined. Define it so a scripted run does not stop there. The
    // value only affects the palette ramp, never game state. gamma_correct()
    // pins the value it actually uses, because startup.lsp loads gamma.lsp
    // after this point.
    LSymbol *gs = LSymbol::Find("darkest_gray");
    if (!gs || !DEFINEDP(gs->GetValue()))
    {
        LSpace *sp = LSpace::Current;
        LSpace::Current = &LSpace::Perm;
        LSymbol::FindOrCreate("darkest_gray")->SetNumber(16);
        LSpace::Current = sp;
    }
}

bool particle_demo()
{
    return opt.particle_demo;
}

bool dynlight_demo()
{
    return opt.dynlight_demo;
}

bool start_demo()
{
    if (opt.dump_bindings)
        print_action_map();

    if (opt.level)
    {
        the_game->load_level(opt.level);
        if (!current_level)
        {
            fprintf(stderr, "unable to load level '%s'\n", opt.level);
            return false;
        }
        the_game->set_state(RUN_STATE);
    }

    if (opt.playback)
    {
        if (!demo_man.set_state(demo_manager::PLAYING, opt.playback))
        {
            fprintf(stderr, "unable to play back '%s'\n", opt.playback);
            return false;
        }
        demo_running = true;
    }
    else if (opt.record)
    {
        if (!demo_man.set_state(demo_manager::RECORDING, opt.record))
        {
            fprintf(stderr, "unable to record to '%s'\n", opt.record);
            return false;
        }
        demo_running = true;
    }
    return true;
}

uint64_t state_hash()
{
    uint64_t h = 0xcbf29ce484222325ULL;

    if (!current_level)
    {
        mix(h, 0);
        return h;
    }

    mix(h, current_level->tick_counter());
    mix(h, rand_on);

    uint64_t count = 0;
    // first_object() walks the level list in load order, which does not depend
    // on what happens to be on screen. See ARCHITECTURE.md section 4.
    for (game_object *o = current_level->first_object(); o; o = o->next)
    {
        count++;
        mix(h, (uint64_t)(uint32_t)o->x);
        mix(h, (uint64_t)(uint32_t)o->y);
        mix(h, (uint64_t)(uint32_t)o->Xvel);
        mix(h, (uint64_t)(uint32_t)o->Yvel);
        mix(h, o->otype);
        mix(h, (uint64_t)o->state);
        mix(h, o->Hp);
        mix(h, o->Mp);
        mix(h, o->Aitype);
        mix(h, o->Aistate);
        mix(h, o->Aistate_time);
        mix(h, (uint64_t)(uint8_t)o->direction);
        mix(h, (uint64_t)(uint16_t)o->current_frame);

        // Local variables carry ammo, timers and everything the Lisp side
        // keeps per object.
        if (o->lvars)
        {
            int total = figures[o->otype]->tv;
            for (int i = 0; i < total; i++)
                mix(h, (uint64_t)(uint32_t)o->lvars[i]);
        }
    }
    mix(h, count);

    return h;
}

namespace {

void report(char const *why, uint64_t loop, uint32_t level_tick, uint64_t hash)
{
    printf("%s loop=%llu level_tick=%u hash=%016llx\n", why,
           (unsigned long long)loop, level_tick, (unsigned long long)hash);
    fflush(stdout);
}

}

bool tick()
{
    if (opt.want_hash && current_level)
    {
        last_hash = state_hash();
        last_loop = loop_ticks;
        last_level_tick = current_level->tick_counter();
        have_last = true;

        if (opt.hash_every > 0 && loop_ticks % (uint64_t)opt.hash_every == 0)
            report("tick", last_loop, last_level_tick, last_hash);
    }

    // A playback that runs out of packets drops back to NORMAL on its own.
    if (demo_running && opt.playback && demo_man.current_state() == demo_manager::NORMAL)
        return false;

    // Asked for frames at given level ticks and got them all: there is
    // nothing left for this run to do. Worth ending on, because a windowed
    // run waits on the display for every frame it draws, and playing out
    // the rest of a replay for nothing costs most of a minute.
    if (!opt.dump_level_ticks.empty()
        && opt.dumped_level_ticks.size() == opt.dump_level_ticks.size())
        return false;

    if (opt.max_ticks >= 0 && loop_ticks >= (uint64_t)opt.max_ticks)
        return false;

    loop_ticks++;
    return true;
}

namespace {

bool tick_is_wanted()
{
    if (opt.dump_every > 0 && loop_ticks % (uint64_t)opt.dump_every == 0)
        return true;

    for (long t : opt.dump_ticks)
        if ((uint64_t)t == loop_ticks)
            return true;

    // The level-tick form fires once, on the first frame at or after the
    // tick asked for, and remembers that it has.
    if (current_level)
    {
        long const now = (long)current_level->tick_counter();
        for (long t : opt.dump_level_ticks)
        {
            if (now < t)
                continue;
            bool done = false;
            for (long d : opt.dumped_level_ticks)
                if (d == t)
                    done = true;
            if (!done)
                return true;
        }
    }
    return false;
}

// Marks the level ticks this frame satisfies, so the next frame does not
// capture again.
void note_level_tick_dumped()
{
    if (!current_level)
        return;
    long const now = (long)current_level->tick_counter();
    for (long t : opt.dump_level_ticks)
        if (now >= t)
        {
            bool done = false;
            for (long d : opt.dumped_level_ticks)
                if (d == t)
                    done = true;
            if (!done)
                opt.dumped_level_ticks.push_back(t);
        }
}

}

void before_frame()
{
    if ((opt.dump_ticks.empty() && opt.dump_level_ticks.empty()
         && opt.dump_every <= 0)
        || !tick_is_wanted())
        return;

    note_level_tick_dumped();

    if (!opt.dump_window)
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s/%06llu-window.bmp", opt.out_dir,
             (unsigned long long)loop_ticks);
    request_window_capture(path);
}

bool draw_overlay_for_capture()
{
    if (opt.dump_screen == Options::Screen::None || opt.dump_ticks.empty()
        || !tick_is_wanted())
        return false;

    // The only way a scripted run can see the options screen, which otherwise
    // exists only inside its own event loop.
    if (opt.dump_screen == Options::Screen::Rebind)
        abuse::ui::draw_rebind_screen(0, false);
    else if (opt.dump_screen == Options::Screen::ClassicData)
        abuse::ui::draw_classic_data_screen(0);
    else if (opt.dump_screen == Options::Screen::MenuHint)
        abuse::ui::draw_options_hint();
    else if (opt.dump_screen == Options::Screen::Hud)
        abuse::ui::draw_hud_pinned();
    else if (opt.dump_screen == Options::Screen::StartMenu)
        abuse::ui::draw_start_menu_pinned(0);
    else if (opt.dump_screen == Options::Screen::Language)
    {
        // The whole screen, art included, which is what the player sees.
        abuse::ui::draw_language_screen(0, abuse::ui::draw_language_backdrop());
    }
    else
        abuse::ui::draw_options_screen_pinned(0);
    return true;
}

void after_frame()
{
    if (opt.dump_ticks.empty() || !tick_is_wanted())
        return;

    char path[512];
    snprintf(path, sizeof(path), "%s/%06llu.bmp", opt.out_dir,
             (unsigned long long)loop_ticks);
    if (!save_frame_bmp(path))
        fprintf(stderr, "unable to write '%s'\n", path);
}

void finish()
{
    // Called twice: once when the main loop ends and once when main does.
    // Harmless for a hash, not for anything that acts on the world. The
    // save dialog opened a second time with nothing queued to close it and
    // sat in its event loop for as long as it was given.
    static bool already_finished = false;
    bool const first_time = !already_finished;
    already_finished = true;

    if (first_time && opt.save_dialog)
    {
        // Esc first: the picker reads events until it has a slot or a
        // cancel, and there is nobody here to press anything.
        Event *escape = new Event();
        escape->type = EV_KEY;
        escape->key = JK_ESC;
        wm->Push(escape);

        printf("save-dialog: opening\n");
        int slot = load_game(1, symbol_str("SAVE"));
        printf("save-dialog: returned %d\n", slot);
    }

    if (first_time && opt.save_test)
    {
        if (!current_level)
            printf("save-test: no level loaded\n");
        else
        {
            char const *prefix = get_save_filename_prefix();
            printf("save-test: prefix '%s'\n", prefix ? prefix : "(none)");

            // The same call the save console makes, through the Lisp
            // binding: save_all, which is what writes a savegame rather
            // than a level.
            //
            // Not save0001.spe: the save directory belongs to whoever is
            // running this, and the game only ever lists save%04d.spe, so
            // this name is invisible to it and cannot overwrite a slot.
            current_level->save("savetest.spe", 1);

            char path[512];
            snprintf(path, sizeof(path), "%ssavetest.spe",
                     prefix ? prefix : "");

            FILE *f = fopen(path, "rb");
            if (!f)
                printf("save-test: FAILED, no file at %s\n", path);
            else
            {
                fseek(f, 0, SEEK_END);
                long size = ftell(f);
                fclose(f);
                printf("save-test: wrote %ld bytes to %s\n", size, path);
                if (size < 1024)
                    printf("save-test: FAILED, that file is too small\n");

                // Nobody's save directory needs this lying around.
                remove(path);
            }
        }
    }

    if (opt.want_hash)
    {
        if (have_last)
            report("final", last_loop, last_level_tick, last_hash);
        else
            report("final", loop_ticks, 0, state_hash());
    }

    if (demo_running && demo_man.current_state() == demo_manager::RECORDING)
        demo_man.set_state(demo_manager::NORMAL);
}

}
