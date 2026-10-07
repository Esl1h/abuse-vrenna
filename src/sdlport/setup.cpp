/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 2001 Anthony Kruize <trandor@labyrinth.net.au>
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software Foundation,
 *  Inc., 51 Franklin Street, Fifth Floor, Boston MA 02110-1301, USA.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#ifdef WIN32
// Windows.h for GetModuleFileName, which is how the data directory is found
// next to the executable. ShlObj.h and direct.h went with the hand-rolled
// save path that SDL_GetPrefPath replaced.
# include <Windows.h>
# define strcasecmp _stricmp
#endif
#ifdef __APPLE__
# include <CoreFoundation/CoreFoundation.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <string>
#include <time.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <signal.h>
#include <SDL3/SDL.h>

#include "compat.h"
#include "data/paths.h"
#include "render/options.h"
#include "render/lightmap.h"
#include "render/shake.h"
#include "render/particles.h"
#include "render/dynlight.h"
#include "imlib/hd.h"
#include "input/gamepad.h"
#include "input/rumble.h"
#include "input/aim.h"
#include "harness.h"
#include "i18n/language.h"
#include "audio/buses.h"
#include "audio/limiter.h"
#include "ui/hexfont.h"
#include "ui/start_menu.h"
#include "ui/hud.h"
#include "configuration.h"
#include "specs.h"
#include "keys.h"
#include <filesystem>

#include "setup.h"
#include "errorui.h"
#include "util.h"

flags_struct flags;

namespace {

// Whether the original sound is actually on this disk.
//
// Not abuse::data::has_classic_data(), which only says whether a path was
// named on the command line. The question here is whether there is
// anything to play, and only the filesystem answers that.
bool classic_sound_installed()
{
    std::error_code ec;
    std::filesystem::path const sfx =
        std::filesystem::path(abuse::data::classic_data_dir()) / "sfx";
    return std::filesystem::is_directory(sfx, ec);
}

}

// Only when the player has not chosen one does the system locale get a say.
static bool g_language_from_config = false;

keys_struct keys;

extern int xres, yres;
static unsigned int scale;

// Per-mode location of abuserc, resolved in setup() before readRCFile().
// Empty keeps the historical save-prefix lookup (Windows, no $HOME).
static std::string g_rc_path;

// mkdir -p, component by component. The engine has no directory API, and
// the XDG paths (~/.config/abuse/<mode>/) arrive unwritten on first run.
//
// Both separators, because on Windows the base comes from SDL with
// backslashes and the per-mode part is appended with forward ones. Missing
// that is how a path stops being created halfway.
static void make_dirs(char const *path)
{
    char buffer[4096];
    snprintf(buffer, sizeof(buffer), "%s", path);
    for (char *p = buffer + 1; *p; p++)
    {
        if ((*p == '/' || *p == '\\') && p[1] != '\0')
        {
            char const sep = *p;
            *p = '\0';
            abuse::make_directory(buffer);
            *p = sep;
        }
    }
    abuse::make_directory(buffer);
}

//
// Display help
//
void showHelp(const char* executableName)
{
    printf( "\n" );
    printf( "Usage: %s [options]\n", executableName );
    printf( "Options:\n\n" );
    printf( "** Abuse Options **\n" );
    printf( "  -size <arg>       Set the size of the screen\n" );
    printf( "  -edit             Startup in editor mode\n" );
    printf( "  -a <arg>          Use addon named <arg>\n" );
    printf( "  -f <arg>          Load map file named <arg>\n" );
    printf( "  -lisp             Startup in lisp interpreter mode\n" );
    printf( "  -nodelay          Run at maximum speed\n" );
    printf( "\n" );
    printf( "** Abuse-SDL Options **\n" );
    printf( "  -datadir <arg>    Set the location of the game data to <arg>\n" );
    printf( "  -fullscreen       Enable fullscreen mode\n" );
    printf( "  -window           Enable windowed mode\n" );
    printf( "  -h, --help        Display this text\n" );
    printf( "  -nosound          Disable sound\n" );
    printf( "  -scale <arg>      Scale to <arg>\n" );
    printf( "  -preset <arg>     classic or sharp\n" );
    printf( "  -scalemode <arg>  integer, fit or stretch\n" );
    printf( "  -filter <arg>     nearest, linear, pixelart or scale2x\n" );
    printf( "  -novsync          Do not wait for the display refresh\n" );
    printf( "  -language <arg>   en, fr, de, pt_BR, or xx_XX for pseudo\n" );
    printf( "  -font <arg>       classic art font, or extended for accents\n" );
//    printf( "  -x <arg>          Set the width to <arg>\n" );
//    printf( "  -y <arg>          Set the height to <arg>\n" );
    printf( "\n" );
    printf( "Anthony Kruize <trandor@labyrinth.net.au>\n" );
    printf( "\n" );
}

//
// Create a default 'abuserc' file
//
void createRCFile( char *rcfile )
{
    FILE *fd = NULL;

    if( (fd = fopen( rcfile, "w" )) != NULL )
    {
        fputs( "; Abuse-SDL Configuration file\n\n", fd );
        fputs( "; Startup fullscreen\nfullscreen=1\n\n", fd );
#if !((defined SDL_PLATFORM_APPLE) || (defined SDL_PLATFORM_WINDOWS))
        fputs( "; Location of the datafiles\ndatadir=", fd );
        fputs( ASSETDIR "\n\n", fd );
#endif
        fputs( "; Grab the mouse to the window\ngrabmouse=0\n\n", fd );
        fputs( "; Set the scale factor\nscale=2\n\n", fd );
        fputs( "; Look preset: classic or sharp. Sets scalemode and filter together;\n", fd );
        fputs( "; anything set after it wins.\npreset=classic\n\n", fd );
        fputs( "; How the frame is scaled to the window: integer, fit, stretch\n", fd );
        fputs( "scalemode=fit\n\n", fd );
        fputs( "; Texture filter: nearest, linear, pixelart, scale2x (gpu only)\n"
               "filter=pixelart\n\n", fd );
        fputs( "; Wait for the display refresh\nvsync=1\n\n", fd );
        fputs( "; Frame cap, 0 for none\nfpslimit=0\n\n", fd );
        fputs( "; Colour of the bars around the image, rrggbb\nletterbox=000000\n\n", fd );
        fputs( "; Gamepad stick deadzone, 0 to 32767. Raise it if the character\n", fd );
        fputs( "; drifts with the stick centred.\ndeadzone=8192\n", fd );
        fputs( "deadzoneouter=30000\n\n", fd );
        fputs( "; Pixels the menu cursor travels per tick with the stick fully\n", fd );
        fputs( "; pushed. The screen is 320 wide and there are 15 ticks a second.\n", fd );
        fputs( "cursorspeed=16\n\n", fd );
        fputs( "; Trigger deadzone. Lower than the stick because a trigger rests\n", fd );
        fputs( "; at zero reliably.\ntriggerdeadzone=2000\n\n", fd );
        fputs( "; Force feedback strength in percent, 0 turns it off\n", fd );
        fputs( "rumble=100\n\n", fd );
        fputs( "; How far the gamepad crosshair can sit from the character, in\n", fd );
        fputs( "; game pixels. The screen is 320 wide.\naimradius=80\n\n", fd );
        fputs( "; Aim assistance for the gamepad, in percent. 0 is off, and off is\n", fd );
        fputs( "; the default: it changes how the game plays. Never applied in\n", fd );
        fputs( "; Original mode.\naimassist=0\n", fd );
        fputs( "; How far off the current aim a target may be to be helped, in\n", fd );
        fputs( "; degrees.\naimassistcone=25\n\n", fd );
//        fputs( "; Set the width of the window\nx=320\n\n", fd );
//        fputs( "; Set the height of the window\ny=200\n\n", fd );
        fputs( "; Master limiter: holds the mix under full scale when a lot\n", fd );
        fputs( "; happens at once. Never applied in the Original mode.\n", fd );
        fputs( ";limiter=off\n\n", fd );
        fputs( "; The mix, as percentages. These survive a restart; the volume\n", fd );
        fputs( "; window in the menu is the slider for the session.\n", fd );
        fputs( ";volume_master=100\n;volume_sfx=100\n;volume_music=100\n;volume_ui=100\n\n", fd );
        fputs( "; Shape of the picture: 4:3, 16:10, 16:9 or 21:9. A wider one\n", fd );
        fputs( "; shows more of the room; what wakes up in a level does not\n", fd );
        fputs( "; change with it. Takes effect when the game starts.\n", fd );
        fputs( ";aspect=16:9\n\n", fd );
        fputs( "; A knock to the camera when the player is hit.\n", fd );
        fputs( ";shake=off\n\n", fd );
        fputs( "; A glow around what is already bright, on the gpu renderer.\n", fd );
        fputs( "; 0 is off, 0.6 is what the enhanced preset uses. The\n", fd );
        fputs( "; threshold is where it starts, in brightness from 0 to 1.\n", fd );
        fputs( ";bloom=0.6\n;bloomthreshold=0.25\n\n", fd );
        fputs( "; A dark line under each pixel row, the way a CRT left one.\n", fd );
        fputs( ";scanlines=on\n\n", fd );
        fputs( "; Lighting in RGB instead of by palette lookup: the same curve\n", fd );
        fputs( "; without the banding. Experimental, and never in Original mode.\n", fd );
        fputs( ";rgblight=on\n\n", fd );
        fputs( "; Named combinations, so the common cases are one line.\n", fd );
        fputs( "; classic is the 1995 look, sharp is hard pixels at whole\n", fd );
        fputs( "; multiples, enhanced adds the card and the RGB lighting,\n", fd );
        fputs( "; and crt is enhanced with scanlines.\n", fd );
        fputs( ";preset=enhanced\n\n", fd );
        fputs( "; How the frame reaches the window. classic is SDL_Renderer,\n", fd );
        fputs( "; which is what the tests compare against; gpu is SDL_GPU,\n", fd );
        fputs( "; which does the palette conversion on the card. Vulkan only\n", fd );
        fputs( "; so far, and it falls back to classic when it cannot start.\n", fd );
        fputs( ";renderer=gpu\n\n", fd );
        fputs( "; Art from data/hd/ replaces the art in the .spe files when\n", fd );
        fputs( "; it is there. Same size as the original; see the README in\n", fd );
        fputs( "; the pack. Off ignores the pack entirely.\n", fd );
        fputs( ";hd=off\n\n", fd );
        fputs( "; The Remastered mode has no sound of its own yet. When the\n", fd );
        fputs( "; original data is installed it borrows the sound and music\n", fd );
        fputs( "; from it, played exactly as they are. Off leaves it silent.\n", fd );
        fputs( ";classicsfx=on\n\n", fd );
        fputs( "; Sparks off a hit and an ejected casing off a shot.\n", fd );
        fputs( ";particles=off\n\n", fd );
        fputs( "; Shots and explosions light the room around them, in the\n", fd );
        fputs( "; colour and with the waver that data/dynlight.txt gives\n", fd );
        fputs( "; them. Needs rgblight=on: the 1995 light path can only\n", fd );
        fputs( "; darken.\n", fd );
        fputs( ";dynlight=off\n\n", fd );
        fputs( "; Turns off every effect that moves the picture by itself,\n", fd );
        fputs( "; without clearing the settings below.\n", fd );
        fputs( ";reducemotion=on\n\n", fd );
        fputs( "; Smooth movement: positions blended between logical ticks.\n", fd );
        fputs( "; The world still advances 15 times a second either way.\n", fd );
        fputs( ";interpolate=off\n\n", fd );
        fputs( "; HUD: classic is the 1995 status bar, modern the overlay one.\n", fd );
        fputs( ";hud=classic\n\n", fd );
        fputs( "; Start menu: modern is the list, classic the strip of icons.\n", fd );
        fputs( ";startmenu=classic\n\n", fd );
        fputs( "; Language of the in-game text: en, fr, de, pt_BR.\n", fd );
        fputs( "; Left out, the system locale decides. xx_XX is the pseudo\n", fd );
        fputs( "; language, for spotting text that does not fit.\n", fd );
        fputs( ";language=en\n\n", fd );
        fputs( "; Font: classic is the art the game shipped with, CP437.\n", fd );
        fputs( "; extended covers Latin-1, which pt_BR needs and picks by itself.\n", fd );
        fputs( ";font=classic\n\n", fd );
        fputs( "; Key mappings\n", fd );
        fputs( "left=LEFT\nright=RIGHT\nup=UP\ndown=DOWN\n", fd );
        fputs( "fire=SPACE\nweapprev=CTRL_R\nweapnext=INSERT\n", fd );
        fputs( "; Alternative key bindings\n; Note: only the following keys can have two bindings\n", fd );
        fputs( "left2=a\nright2=d\nup2=w\ndown2=s\n\n", fd );
        fputs( "; Newer form, one line per binding, any number per action:\n", fd );
        fputs( ";   bind=<action>,key,<key name>\n", fd );
        fputs( ";   bind=<action>,pad,<button>      e.g. a, dpleft, leftshoulder\n", fd );
        fputs( ";   bind=<action>,pad,<axis>+       e.g. righttrigger+, leftx-\n", fd );
        fputs( ";   bind=<action>,mouse,<1-8>\n", fd );
        fputs( "; Actions: left right up down fire special weapprev weapnext\n", fd );
        fputs( ";\n; Or pick a ready-made keyboard layout instead:\n", fd );
        fputs( ";   keypreset=classic   as the game always shipped (default)\n", fd );
        fputs( ";   keypreset=modern    Q/E change weapon, F is the special\n", fd );
        fputs( "; The first bind= line replaces every default, so list them all\n", fd );
        fputs( "; or none. The keys above keep working when no bind= is present.\n", fd );
        fclose( fd );
    }
    else
    {
        printf( "Unable to create 'abuserc' file.\n" );
    }
}

bool language_was_configured()
{
    return g_language_from_config;
}

char const *config_file_path()
{
    static std::string resolved;
    if (!g_rc_path.empty())
        return g_rc_path.c_str();

    if (resolved.empty())
    {
        char *fallback = join_strings(get_save_filename_prefix(), "/abuserc");
        resolved = fallback;
        free(fallback);
    }
    return resolved.c_str();
}

//
// Read in the 'abuserc' file
//
void readRCFile()
{
    FILE *fd = NULL;
    char *rcfile;
    char buf[255];
    char *result;

    // Phase 1, task 1.3: the rc file moved to the per-mode config directory.
    // Empty (no $HOME, Windows path) keeps the historical save-prefix lookup.
    if (!g_rc_path.empty())
        rcfile = SDL_strdup(g_rc_path.c_str());
    else
        rcfile = join_strings(get_save_filename_prefix(), "/abuserc");
    if( (fd = fopen( rcfile, "r" )) != NULL )
    {
        while( fgets( buf, sizeof( buf ), fd ) != NULL )
        {
            result = strtok( buf, "=" );
            if( strcasecmp( result, "fullscreen" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                flags.fullscreen = atoi( result );
            }
            else if( strcasecmp( result, "grabmouse" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                flags.grabmouse = atoi( result );
            }
            else if( strcasecmp( result, "preset" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::Preset preset;
                if( abuse::render::parse_preset( result, preset ) )
                {
                    abuse::render::apply_preset( preset,
                                                 abuse::render::options() );
                    abuse::render::set_rgb_lighting(
                        abuse::render::preset_wants_rgb_light( preset ) );
                }
            }
            else if( strcasecmp( result, "scalemode" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::parse_scale_mode( result, abuse::render::options().scale );
            }
            else if( strcasecmp( result, "filter" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::parse_filter( result, abuse::render::options().filter );
            }
            else if( strcasecmp( result, "vsync" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::options().vsync = atoi( result ) != 0;
            }
            else if( strcasecmp( result, "fpslimit" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::options().fps_limit = atoi( result );
            }
            else if( strcasecmp( result, "language" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                if( result && !abuse::i18n::parse_language(
                        result, abuse::i18n::language() ) )
                    printf( "Config: unknown language '%s'\n", result );
                else
                    g_language_from_config = true;
            }
            else if( strcasecmp( result, "cursorspeed" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                int speed = 0;
                if( result && abuse::input::parse_deadzone_value( result, speed )
                    && speed > 0 )
                    abuse::input::cursor_settings().speed = speed;
                else
                    printf( "Config: bad cursorspeed '%s'\n", result );
            }
            else if( strcasecmp( result, "font" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool extended = false;
                if( result && abuse::ui::parse_font_choice( result, extended ) )
                    abuse::ui::set_extended_font( extended );
                else
                    printf( "Config: unknown font '%s', expected classic or extended\n",
                            result );
            }
            else if( strcasecmp( result, "startmenu" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool classic = false;
                if( result && abuse::ui::parse_start_menu_choice( result, classic ) )
                    abuse::ui::set_classic_start_menu( classic );
                else
                    printf( "Config: unknown startmenu '%s', expected modern or classic\n",
                            result );
            }
            else if( strcasecmp( result, "limiter" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::audio::limiter().set_enabled( on );
                else
                    printf( "Config: unknown limiter '%s', expected on or off\n",
                            result );
            }
            else if( strncasecmp( result, "volume_", 7 ) == 0 )
            {
                // volume_master, volume_sfx, volume_music, volume_ui, each a
                // percentage. The mix that survives a restart; the volume
                // window in the menu is still the slider for the session.
                char const *which = result + 7;
                result = strtok( NULL, "\n" );
                float gain = 1.0f;
                abuse::audio::Bus bus;
                if( !result || !abuse::audio::parse_percent( result, gain ) )
                    printf( "Config: volume_%s wants a percentage, got '%s'\n",
                            which, result ? result : "" );
                else if( strcasecmp( which, "master" ) == 0 )
                    abuse::audio::set_master( gain );
                else if( abuse::audio::parse_bus( which, bus ) )
                    abuse::audio::set_gain( bus, gain );
                else
                    printf( "Config: unknown volume '%s', expected master, sfx,"
                            " music or ui\n", which );
            }
            else if( strcasecmp( result, "shake" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::set_shake_enabled( on );
                else
                    printf( "Config: unknown shake '%s', expected on or off\n",
                            result );
            }
            else if( strcasecmp( result, "aspect" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::Aspect a;
                if( result && abuse::render::parse_aspect( result, a ) )
                    abuse::render::options().aspect = a;
                else
                    printf( "Config: unknown aspect '%s', expected 4:3, 16:10,"
                            " 16:9 or 21:9\n", result );
            }
            else if( strcasecmp( result, "bloom" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                if( result )
                {
                    double v = atof( result );
                    if( v < 0 ) v = 0;
                    if( v > 2 ) v = 2;
                    abuse::render::options().bloom = (float)v;
                }
            }
            else if( strcasecmp( result, "bloomthreshold" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                if( result )
                {
                    double v = atof( result );
                    if( v < 0 ) v = 0;
                    if( v > 1 ) v = 1;
                    abuse::render::options().bloom_threshold = (float)v;
                }
            }
            else if( strcasecmp( result, "scanlines" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = false;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::options().scanlines = on;
                else
                    printf( "Config: unknown scanlines '%s', expected on or off\n",
                            result );
            }
            else if( strcasecmp( result, "rgblight" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = false;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::set_rgb_lighting( on );
                else
                    printf( "Config: unknown rgblight '%s', expected on or off\n",
                            result );
            }
            else if( strcasecmp( result, "renderer" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::Backend b;
                if( result && abuse::render::parse_backend( result, b ) )
                    abuse::render::options().backend = b;
                else
                    printf( "Config: unknown renderer '%s', expected classic"
                            " or gpu\n", result );
            }
            else if( strcasecmp( result, "hd" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::hd::set_enabled( on );
                else
                    printf( "Config: unknown hd '%s', expected on or off\n",
                            result );
            }
            else if( strcasecmp( result, "classicsfx" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    flags.classic_sfx = on;
                else
                    printf( "Config: unknown classicsfx '%s', expected on or"
                            " off\n", result );
            }
            else if( strcasecmp( result, "particles" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::set_particles_enabled( on );
                else
                    printf( "Config: unknown particles '%s', expected on or"
                            " off\n", result );
            }
            else if( strcasecmp( result, "dynlight" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::set_dynlight_enabled( on );
                else
                    printf( "Config: unknown dynlight '%s', expected on or"
                            " off\n",
                            result );
            }
            else if( strcasecmp( result, "reducemotion" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = false;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::options().reduce_motion = on;
                else
                    printf( "Config: unknown reducemotion '%s', expected on or"
                            " off\n", result );
            }
            else if( strcasecmp( result, "interpolate" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool on = true;
                if( result && abuse::render::parse_switch( result, on ) )
                    abuse::render::options().interpolate = on;
                else
                    printf( "Config: unknown interpolate '%s', expected on or off\n",
                            result );
            }
            else if( strcasecmp( result, "hud" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                bool classic = true;
                if( result && abuse::ui::parse_hud_choice( result, classic ) )
                    abuse::ui::set_classic_hud( classic );
                else
                    printf( "Config: unknown hud '%s', expected classic or modern\n",
                            result );
            }
            else if( strcasecmp( result, "keypreset" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                if( result && !apply_config_key_preset( result ) )
                    printf( "Config: unknown keypreset '%s', expected classic or modern\n",
                            result );
            }
            else if( strcasecmp( result, "bind" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                // A bad line drops one binding and says so, instead of taking
                // the whole config down.
                if( result && !add_config_binding( result ) )
                    printf( "Config: ignoring malformed bind '%s'\n", result );
            }
            else if( strcasecmp( result, "aimradius" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                int radius = 0;
                // Reuses the deadzone parser: same shape, a plain number with
                // no suffix, rejected rather than clamped when out of range.
                if( abuse::input::parse_deadzone_value( result, radius ) && radius > 0 )
                    abuse::input::aim_settings().radius = radius;
            }
            else if( strcasecmp( result, "aimassist" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                int percent = 0;
                if( abuse::input::parse_rumble_strength( result, percent ) )
                    abuse::input::assist_settings().strength = percent;
            }
            else if( strcasecmp( result, "aimassistcone" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                int degrees = 0;
                if( abuse::input::parse_rumble_strength( result, degrees )
                    && degrees > 0 && degrees <= 90 )
                    abuse::input::assist_settings().cone_degrees = degrees;
            }
            else if( strcasecmp( result, "rumble" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::input::parse_rumble_strength(
                    result, abuse::input::rumble_settings().strength );
            }
            else if( strcasecmp( result, "deadzone" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::input::parse_deadzone_value(
                    result, abuse::input::deadzone().inner );
            }
            else if( strcasecmp( result, "deadzoneouter" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::input::parse_deadzone_value(
                    result, abuse::input::deadzone().outer );
            }
            else if( strcasecmp( result, "triggerdeadzone" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::input::parse_deadzone_value(
                    result, abuse::input::trigger_deadzone().inner );
            }
            else if( strcasecmp( result, "letterbox" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                abuse::render::parse_letterbox( result, abuse::render::options().letterbox );
            }
            else if( strcasecmp( result, "scale" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                scale = atoi( result );
//                flags.xres = xres * atoi( result );
//                flags.yres = yres * atoi( result );
            }
/*            else if( strcasecmp( result, "x" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                flags.xres = atoi( result );
            }
            else if( strcasecmp( result, "y" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                flags.yres = atoi( result );
            }*/
            else if( strcasecmp( result, "datadir" ) == 0 )
            {
                result = strtok( NULL, "\n" );
                set_filename_prefix( result );
            }
            else if( strcasecmp( result, "left" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.left = key_value( result );
            }
            else if( strcasecmp( result, "right" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.right = key_value( result );
            }
            else if( strcasecmp( result, "up" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.up = key_value( result );
            }
            else if( strcasecmp( result, "down" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.down = key_value( result );
            }
            else if( strcasecmp( result, "left2" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.left_2 = key_value( result );
            }
            else if( strcasecmp( result, "right2" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.right_2 = key_value( result );
            }
            else if( strcasecmp( result, "up2" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.up_2 = key_value( result );
            }
            else if( strcasecmp( result, "down2" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.down_2 = key_value( result );
            }
            else if( strcasecmp( result, "fire" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.b2 = key_value( result );
            }
            else if( strcasecmp( result, "special" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.b1 = key_value( result );
            }
            else if( strcasecmp( result, "weapprev" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.b3 = key_value( result );
            }
            else if( strcasecmp( result, "weapnext" ) == 0 )
            {
                result = strtok( NULL,"\n" );
                keys.b4 = key_value( result );
            }
        }
        fclose( fd );
    }
    else
    {
        // Couldn't open the abuserc file so let's create a default one
        createRCFile( rcfile );
    }
    SDL_free( rcfile );
}

//
// Parse the command-line parameters
//
void parseCommandLine( int argc, char **argv )
{
    for( int ii = 1; ii < argc; ii++ )
    {
        if( !strcasecmp( argv[ii], "-fullscreen" ) )
        {
            flags.fullscreen = 1;
        }
        else if( !strcasecmp( argv[ii], "-size" ) )
        {
            if( ii + 1 < argc && !sscanf( argv[++ii], "%d", &xres ) )
            {
                xres = 320;
            }
            if( ii + 1 < argc && !sscanf( argv[++ii], "%d", &yres ) )
            {
                yres = 200;
            }
        }
        else if( !strcasecmp( argv[ii], "-language" ) )
        {
            if( ii + 1 < argc && abuse::i18n::parse_language(
                    argv[++ii], abuse::i18n::language() ) )
                g_language_from_config = true;
            else
                printf( "Unknown language '%s'\n", argv[ii] );
        }
        else if( !strcasecmp( argv[ii], "-font" ) )
        {
            bool extended = false;
            if( ii + 1 < argc && abuse::ui::parse_font_choice( argv[++ii], extended ) )
                abuse::ui::set_extended_font( extended );
            else
                printf( "Unknown font '%s', expected classic or extended\n", argv[ii] );
        }
        else if( !strcasecmp( argv[ii], "-preset" ) )
        {
            abuse::render::Preset preset;
            if( ii + 1 < argc && abuse::render::parse_preset( argv[++ii], preset ) )
            {
                abuse::render::apply_preset( preset,
                                             abuse::render::options() );
                abuse::render::set_rgb_lighting(
                    abuse::render::preset_wants_rgb_light( preset ) );
            }
            else
                printf( "Unknown preset '%s', expected classic, sharp,"
                        " enhanced or crt\n", argv[ii] );
        }
        else if( !strcasecmp( argv[ii], "-scalemode" ) )
        {
            if( ii + 1 < argc && !abuse::render::parse_scale_mode( argv[++ii],
                                        abuse::render::options().scale ) )
                printf( "Unknown scale mode '%s', keeping %s\n", argv[ii],
                        abuse::render::scale_mode_name( abuse::render::options().scale ) );
        }
        else if( !strcasecmp( argv[ii], "-filter" ) )
        {
            if( ii + 1 < argc && !abuse::render::parse_filter( argv[++ii],
                                        abuse::render::options().filter ) )
                printf( "Unknown filter '%s', keeping %s\n", argv[ii],
                        abuse::render::filter_name( abuse::render::options().filter ) );
        }
        else if( !strcasecmp( argv[ii], "-novsync" ) )
        {
            abuse::render::options().vsync = 0;
        }
        else if( !strcasecmp( argv[ii], "-scale" ) )
        {
            // FIXME: Pretty sure scale does nothing now
            int result;
            if( sscanf( argv[++ii], "%d", &result ) )
            {
                scale = result;
/*                flags.xres = xres * scale;
                flags.yres = yres * scale; */
            }
        }
/*        else if( !strcasecmp( argv[ii], "-x" ) )
        {
            int x;
            if( sscanf( argv[++ii], "%d", &x ) )
            {
                flags.xres = x;
            }
        }
        else if( !strcasecmp( argv[ii], "-y" ) )
        {
            int y;
            if( sscanf( argv[++ii], "%d", &y ) )
            {
                flags.yres = y;
            }
        }*/
        else if( !strcasecmp( argv[ii], "-window" ) )
        {
            flags.fullscreen = 0;
        }
        else if( !strcasecmp( argv[ii], "-nosound" ) )
        {
            flags.nosound = 1;
        }
        else if( !strcasecmp( argv[ii], "-datadir" ) )
        {
            char datadir[255];
            if( ii + 1 < argc && sscanf( argv[++ii], "%s", datadir ) )
            {
                set_filename_prefix( datadir );
            }
        }
        else if( !strcasecmp( argv[ii], "-h" ) || !strcasecmp( argv[ii], "--help" ) )
        {
            showHelp(argv[0]);
            exit( 0 );
        }
        else if ( !strcasecmp( argv[ii], "-pause" ) )
        {
            // Debug command to force a pause here
            printf("Pausing, press any key to resume (attach debugger now!) . . .");
            getc(stdin);
            printf("\n");
        }
    }
}

//
// Setup SDL and configuration
//
void setup( int argc, char **argv )
{
    // Initialize default settings
    flags.fullscreen         = 1;    // Start fullscreen (actually windowed-fullscreen now)
    flags.nosound            = 0;    // Enable sound
    flags.grabmouse          = 0;    // Don't grab the mouse
    flags.xres = xres        = 320;  // Default window width
    flags.yres = yres        = 200;  // Default window height

    // A wider buffer, for the phase 6.5 measurements: see
    // abuse::harness::viewport_size. The engine has refused -size outside
    // the editor since 1995, and that refusal stays where it is.
    {
        int vw = 0, vh = 0;
        if( abuse::harness::viewport_size( vw, vh ) )
        {
            flags.xres = xres = vw;
            flags.yres = yres = vh;
        }
    }

    // The aspect from abuserc, read further down: applied after it, in
    // apply_aspect() below, because this runs before the file is read.
    keys.up                  = key_value( "UP" );
    keys.down                = key_value( "DOWN" );
    keys.left                = key_value( "LEFT" );
    keys.right               = key_value( "RIGHT" );
    keys.up_2                = key_value( "w" );
    keys.down_2              = key_value( "s" );
    keys.left_2              = key_value( "a" );
    keys.right_2             = key_value( "d" );
    keys.b3                  = key_value( "CTRL_R" );
    keys.b4                  = key_value( "INSERT" );
    scale                    = 2;    // Default scale amount

    // Display our name and version
    printf( "%s %s\n", PACKAGE_NAME, PACKAGE_VERSION );

    // SDL_native_midi ships with its debug logging forced on: its guard reads
    // "#if 1 //ndef NDEBUG", so a release build still logs every MIDI poll and
    // event. That is tens of thousands of lines per session, drowning the
    // messages that matter. The game itself never calls SDL_Log, so raising
    // the floor for that category costs nothing and keeps warnings and errors.
    SDL_SetLogPriority( SDL_LOG_CATEGORY_APPLICATION, SDL_LOG_PRIORITY_WARN );

    // Initialize SDL with video and audio support.
    //
    // No gamepad in a scripted run. Such a run must not read a physical
    // device: a pad plugged into the machine changes the button labels the
    // controls screen prints, and a held button reaches the action map and
    // changes what a replay does. Both are the host leaking into a result
    // that is compared byte for byte.
    //
    // This used to ask headless() and so covered only the runs with no
    // window. It asks input_is_scripted() and not scripted_run(), because
    // a recording is a person playing and the gamepad is exactly what it
    // is there to capture. A windowed playback still opened the pad, and since a gamepad
    // button arrives as a key press and any key press stops a demo
    // (src/demo.cpp), a pad sitting on the desk ended those runs at a
    // different tick every time: five of eight, with the start button as
    // Esc and the back button as 'p' among the culprits. That is also the
    // only way to exercise the GPU path, which needs a real window.
    SDL_InitFlags init = SDL_INIT_VIDEO | SDL_INIT_AUDIO;
    if( !abuse::harness::input_is_scripted() )
        init |= SDL_INIT_JOYSTICK | SDL_INIT_GAMEPAD;

    if( !SDL_Init( init ) )
    {
        show_startup_error( "Unable to initialize SDL : %s\n", SDL_GetError() );
        exit( 1 );
    }
    atexit( SDL_Quit );

    // Set the savegame directory
#ifdef WIN32
    // Windows has no $HOME and no XDG. SDL picks the per-user directory,
    // creates it, and returns UTF-8 ending in a separator: exactly
    // %APPDATA%\Abuse\, which is where this used to point by hand.
    //
    // What it replaces sized a buffer from the wide-character length, left
    // room for "\Abuse\" but not for the terminator, and converted with
    // wcstombs in the current locale, which mangles any user name that is
    // not ASCII. Both faults are gone with the call.
    if( char *pref = SDL_GetPrefPath( NULL, "Abuse" ) )
    {
        abuse::data::set_user_dir( pref );
        SDL_free( pref );
    }
#endif

    // Phase 1, task 1.3: XDG directories per mode, with the legacy ~/.abuse/
    // still winning while it exists, so current installs keep their saves,
    // light.tbl and abuserc, and replays keep their baseline. On Windows the
    // base is the directory SDL just named, and the per-mode layout under it
    // is the same.
    {
        abuse::data::Env env = abuse::data::system_env();

        // Before anything else here: every path below is named after the
        // mode, so the mode has to be settled first. --mode still wins, which
        // is why the saved value is only consulted when the flag was absent.
        // The harness ignores the file entirely, for the same reason it
        // ignores the locale: a snapshot must not depend on what the host
        // last played.
        abuse::data::Mode saved;
        if (!abuse::harness::mode_from_command_line()
            && !abuse::harness::headless()
            && abuse::data::load_saved_mode(env, saved))
            abuse::data::set_mode(saved);

        if (env.home.empty() && abuse::data::user_dir().empty())
        {
            printf( "WARNING: Unable to get $HOME environment variable.\n" );
            printf( "         Savegames will probably fail.\n" );
            set_save_filename_prefix( "" );
        }
        else
        {
            std::string prefix = abuse::data::save_prefix(env);
            std::string rcfile = abuse::data::rc_path(env);

            // The legacy directory already exists; the XDG ones may not.
            if (!abuse::data::legacy_dir_exists(env))
                make_dirs(prefix.substr(0, prefix.size() - 1).c_str());

            char *dir = SDL_strdup(prefix.c_str());
            set_save_filename_prefix(dir);
            SDL_free(dir);

            // abuserc lives in the config directory, which may not exist
            // either when the write below creates it.
            std::string parent = rcfile.substr(0, rcfile.find_last_of("/\\"));
            make_dirs(parent.c_str());
            g_rc_path = rcfile;
        }
    }

    // Set the datadir to a default value
    // (The current directory)
#ifdef SDL_PLATFORM_APPLE
    Uint8 buffer[255];
    CFURLRef bundleurl = CFBundleCopyBundleURL(CFBundleGetMainBundle());
    CFURLRef url = CFURLCreateCopyAppendingPathComponent(kCFAllocatorDefault, bundleurl, CFSTR("Contents/Resources/data"), true);

    if (!CFURLGetFileSystemRepresentation(url, true, buffer, 255))
    {
        exit(1);
    }
    else
    {
        printf("Setting prefix to [%s]\n", buffer);
        set_filename_prefix( (const char*)buffer );
    }
#elif defined WIN32
    // Under Windows, it makes far more sense to assume the data is stored
    // relative to our executable than anywhere else.
    char assetDirName[MAX_PATH];
    GetModuleFileName(NULL, assetDirName, MAX_PATH);
    // Find the first \ or / and cut the path there
    size_t cut_at = -1;
    for (size_t i = 0; assetDirName[i] != '\0'; i++) {
        if (assetDirName[i] == '\\' || assetDirName[i] == '/') {
            cut_at = i;
        }
    }
    if (cut_at >= 0)
        assetDirName[cut_at] = '\0';
    printf("Setting data dir to %s\n", assetDirName);
    set_filename_prefix( assetDirName );
#else
    set_filename_prefix( ASSETDIR );
#endif

    // Load the users configuration. Not under the test harness: a run that
    // reads the developer's abuserc is a run whose result depends on the
    // machine it happened on, and the window snapshots compare the presented
    // frame, which scale= alone would change. It also keeps a test run from
    // writing an abuserc into a CI home directory.
    if( abuse::harness::headless() )
        printf( "Config: abuserc skipped under --headless\n" );
    else
        readRCFile();

    // No audio device in a scripted run, for the same reason it reads no
    // abuserc: a test that touches the machine's audio stack is a test whose
    // result depends on that machine. Nothing the harness compares has a
    // sound in it, neither a state hash nor a stored frame.
    //
    // This was free until the free pack existed. Without data/sfx, sound_init
    // gave up at its first check and no device was ever opened; with 73 Vorbis
    // files there, every replay began opening audio and decoding the pack, and
    // the Windows runner went past the two minute bound the replay suite
    // allows and was killed.
    if( abuse::harness::headless() )
        flags.nosound = 1;

    // Handle command-line parameters
    parseCommandLine( argc, argv );

    // Which picture a pack with several of them shows this time. Left at
    // zero for a scripted run, which always takes the first: the same rule
    // as the locale and the gamepad, because a reference frame must not
    // depend on the day it was recorded.
    if( !abuse::harness::scripted_run() )
        abuse::hd::set_variant_seed( (unsigned long long)time( NULL ) );

    // Neither the config nor the command line named one, so ask the system.
    // Except under the test harness: a snapshot taken on a host set to
    // Portuguese would not match one taken on a host set to English, and the
    // font follows the language, so the whole frame would differ. An explicit
    // -language still wins, which is how the translated frames get tested.
    if( !g_language_from_config && !abuse::harness::headless() )
        abuse::i18n::language() = abuse::i18n::detect_language();

    // Phase 1, tasks 1.3 and 1.4: in Original mode the user-supplied data is an
    // OVERLAY, not a replacement. Levels, art and lisp stay in the normal data
    // directory, where they are public domain; only the sound and music that
    // are missing from it come from the classic set. Replacing the prefix
    // outright would lose the art, since the classic tarballs carry no levels.
    if (abuse::data::mode() == abuse::data::Mode::Original)
    {
        char *classic = SDL_strdup(abuse::data::classic_data_dir().c_str());
        set_fallback_filename_prefix( classic );
        SDL_free( classic );

        // The Original mode is the reference the tests compare against, so it
        // always presents the classic way regardless of what the config says.
        abuse::render::apply_preset( abuse::render::Preset::Classic,
                                     abuse::render::options() );

        // And the override pack is an addition like any other, so it is not
        // available here either. Left on, an installed pack would replace
        // pictures in the one mode whose pictures are the reference, and
        // every frame recorded from it would depend on whether the machine
        // that recorded it happened to have a pack.
        abuse::hd::set_enabled( false );
    }
    else if( flags.classic_sfx && !abuse::harness::headless()
             && classic_sound_installed() )
    {
        // Asked for, and not guessed at. The overlay is consulted first, so
        // every sound the original has wins and the free pack fills in what
        // it does not: that is the whole of "original sound" as a choice.
        //
        // It is the player's own copy of data they downloaded themselves,
        // which is the only reason this is allowed to exist at all.
        //
        // Nothing is converted, resampled or written; the files are played
        // exactly as they are, which is the rule that protects them.
        //
        // Never under the harness. A test that reads the user's data
        // directory is a test whose result depends on the machine it ran
        // on, which is the one property the harness exists to keep.
        char *classic = SDL_strdup(abuse::data::classic_data_dir().c_str());
        set_fallback_filename_prefix( classic );
        SDL_free( classic );
        printf( "Sound: the original set, from %s\n"
                "       (classicsfx=off goes back to the free pack)\n",
                abuse::data::classic_data_dir().c_str() );
    }

    // The shape of the picture, from abuserc, applied here because the file
    // is read above and everything sized from the buffer comes after.
    //
    // Never in the Original mode: that mode is what the snapshots compare
    // against, and a wider buffer is a different picture. Never under the
    // harness either, which has --viewport for this and pins it there.
    if( abuse::data::mode() != abuse::data::Mode::Original
        && !abuse::harness::headless() )
    {
        int const want = abuse::render::aspect_width(
            abuse::render::options().aspect );
        if( want > xres )
            flags.xres = xres = want;
    }

    // Calculate the scaled window size.
    flags.xres = xres * scale;
    flags.yres = yres * scale;
}

//
// Get the key binding for the requested function
//
int get_key_binding(char const *dir, int i)
{
    if( strcasecmp( dir, "left" ) == 0 )
        return keys.left;
    else if( strcasecmp( dir, "right" ) == 0 )
        return keys.right;
    else if( strcasecmp( dir, "up" ) == 0 )
        return keys.up;
    else if( strcasecmp( dir, "down" ) == 0 )
        return keys.down;
    else if( strcasecmp( dir, "left2" ) == 0 )
        return keys.left_2;
    else if( strcasecmp( dir, "right2" ) == 0 )
        return keys.right_2;
    else if( strcasecmp( dir, "up2" ) == 0 )
        return keys.up_2;
    else if( strcasecmp( dir, "down2" ) == 0 )
        return keys.down_2;
    else if( strcasecmp( dir, "b1" ) == 0 )
        return keys.b1;
    else if( strcasecmp( dir, "b2" ) == 0 )
        return keys.b2;
    else if( strcasecmp( dir, "b3" ) == 0 )
        return keys.b3;
    else if( strcasecmp( dir, "b4" ) == 0 )
        return keys.b4;

    return 0;
}
