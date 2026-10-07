/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See options_screen.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "options_screen.h"

#include <stdio.h>
#include <string.h>

#include "data/config_file.h"
#include "data/paths.h"
#include "sdlport/setup.h"
#include "hexfont.h"
#include "i18n/language.h"
#include "i18n/uitext.h"
#include "hud.h"
#include "language_screen.h"
#include "menu_list.h"
#include "input/gamepad.h"
#include "input/aim.h"
#include "input/rumble.h"
#include "input/actions.h"
#include "configuration.h"
#include "jwindow.h"
#include "sbar.h"
#include "keys.h"
#include "overlay.h"
#include "render/lightmap.h"
#include "render/options.h"
#include "render/particles.h"
#include "render/dynlight.h"
#include "gamma.h"
#include "video.h"

extern WindowManager *wm;

// Declared here and not in a header because that is how every other file in
// this engine reaches it: setup.cpp defines it, and video, event and sound
// each say so for themselves.
extern flags_struct flags;

namespace abuse::ui {

namespace {

// One row of the screen. `step` moves the value by one notch in `dir`, and
// `show` writes what to print for it.
struct Item
{
    i18n::Phrase label;
    char const *key;            // the abuserc key it writes
    bool restart;               // the running session keeps the old value
    void (*step)(int dir);
    void (*show)(char *buf, size_t n);
    void (*value)(char *buf, size_t n);   // what goes in the config file
};

int clamp_step(int v, int dir, int step, int lo, int hi)
{
    v += dir * step;
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return v;
}

// ---- mode -----------------------------------------------------------------

// Which set of data and rules the game runs on. It only takes effect on the
// next run: the data prefix, the sound and the save directory are all chosen
// during startup, and re-pointing them with a level loaded is a good deal
// more than a settings row should attempt.
void mode_step(int)
{
    data::set_mode(data::mode() == data::Mode::Original
                       ? data::Mode::Remaster : data::Mode::Original);
}

void mode_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(data::mode() == data::Mode::Original
                                   ? i18n::kModeOriginal : i18n::kModeRemaster));
}

void mode_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", data::mode_name(data::mode()));
}

// ---- picture shape --------------------------------------------------------

// 4:3 through 21:9. Only on the next run: the buffer, the views and the
// light table are all sized from it when the game starts.
//
// What wakes up in a level does not follow this, which is what makes it
// safe to offer: see view::classic_xoff.
void aspect_step(int dir)
{
    static render::Aspect const order[] = {
        render::Aspect::Classic, render::Aspect::Wide16x10,
        render::Aspect::Wide16x9, render::Aspect::Ultra21x9
    };
    int const count = (int)(sizeof(order) / sizeof(order[0]));

    int at = 0;
    for (int i = 0; i < count; i++)
        if (order[i] == render::options().aspect)
            at = i;

    render::options().aspect = order[list_wrap(at, dir, count)];
}

void aspect_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::aspect_name(render::options().aspect));
}

void aspect_value(char *buf, size_t n)
{
    aspect_show(buf, n);
}

// ---- HUD ------------------------------------------------------------------

// Takes effect at once: the strip asks classic_hud() before it draws, and the
// new HUD asks it before it draws. Only the Original mode overrides it, and
// it does so for the whole run.
void hud_step(int)
{
    set_classic_hud(!classic_hud_configured());
    sbar.need_refresh();
}

void hud_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(classic_hud_configured()
                                   ? i18n::kHudClassic : i18n::kHudModern));
}

void hud_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", classic_hud_configured() ? "classic" : "modern");
}

// ---- smooth movement ------------------------------------------------------

// Positions blended between the last two logical ticks. Takes effect at once:
// the draw asks for it every frame. The Original mode ignores it, like every
// other visual addition.
void smooth_step(int)
{
    render::options().interpolate = !render::options().interpolate;
}

void smooth_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(render::options().interpolate
                                   ? i18n::kOn : i18n::kOff));
}

void smooth_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::options().interpolate ? "on" : "off");
}

// ---- preset ---------------------------------------------------------------

// The named looks, in the order they add to each other: the 1995 picture,
// hard pixels, the card doing the work, and the card with scanlines.
render::Preset const kPresets[] = {
    render::Preset::Classic, render::Preset::Sharp,
    render::Preset::Enhanced, render::Preset::Crt
};

int const kPresetCount = (int)(sizeof(kPresets) / sizeof(kPresets[0]));

// Which one the current settings amount to, or -1 when the player has
// since changed something by hand. There is no stored "current preset":
// a preset is something applied, not something held, and pretending
// otherwise would have the screen lie after one change to a single row.
int preset_now()
{
    for (int i = 0; i < kPresetCount; i++)
    {
        render::Options probe = render::options();
        render::apply_preset(kPresets[i], probe);

        if (probe.scale == render::options().scale
            && probe.filter == render::options().filter
            && probe.backend == render::options().backend
            && probe.scanlines == render::options().scanlines
            && render::preset_wants_rgb_light(kPresets[i])
                   == render::rgb_lighting())
            return i;
    }
    return -1;
}

void preset_step(int dir)
{
    int at = preset_now();
    // From "none of them", stepping forwards lands on the first.
    int next = at < 0 ? (dir > 0 ? 0 : kPresetCount - 1)
                      : (at + dir + kPresetCount) % kPresetCount;

    render::apply_preset(kPresets[next], render::options());
    render::set_rgb_lighting(render::preset_wants_rgb_light(kPresets[next]));
    sbar.need_refresh();
}

void preset_show(char *buf, size_t n)
{
    int at = preset_now();
    snprintf(buf, n, "%s", at < 0 ? "-" : render::preset_name(kPresets[at]));
}

void preset_value(char *buf, size_t n)
{
    int at = preset_now();
    if (at < 0)
        buf[0] = 0;     // nothing to write: the rows below say it all
    else
        snprintf(buf, n, "%s", render::preset_name(kPresets[at]));
}

// ---- particles ------------------------------------------------------------

// Sparks off a hit and a casing off a shot. Takes effect at once: turning it
// off clears what is in the air as well, so the picture settles rather than
// finishing the burst that was already flying.
void particles_step(int)
{
    render::set_particles_enabled(!render::particles_enabled());
}

void particles_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(render::particles_enabled()
                                   ? i18n::kOn : i18n::kOff));
}

void particles_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::particles_enabled() ? "on" : "off");
}

// ---- brightness -----------------------------------------------------------

// The gamma calibration, which until now could only be set through the
// grey-scale picker the game has shown since 1995. That picker is driven
// by the pointer and nothing else, and a player with a controller could
// not work it at all.
//
// Shown as steps from the reference rather than as the stored number: 16
// is gamma exactly 1, the palette untouched, and it is the value every
// reference frame in this repository was recorded with. So 16 shows as 0,
// darker is negative and brighter is positive, which is the only part of
// this a player has any reason to care about.
int const kGammaSteps[] = { 1, 4, 8, 12, 16, 20, 24, 32 };
int const kGammaCount = (int)(sizeof(kGammaSteps) / sizeof(kGammaSteps[0]));
int const kGammaReference = 4;      // the index of 16 above

int gamma_index()
{
    int const dg = gamma_value();
    int best = kGammaReference;
    for (int i = 0; i < kGammaCount; i++)
        if (kGammaSteps[i] == dg)
            best = i;
    return best;
}

void brightness_step(int dir)
{
    int const at = list_wrap(gamma_index(), dir, kGammaCount);
    set_gamma_value(kGammaSteps[at]);
    sbar.need_refresh();
}

void brightness_show(char *buf, size_t n)
{
    int const step = gamma_index() - kGammaReference;
    if (step == 0)
        snprintf(buf, n, "0");
    else
        snprintf(buf, n, "%+d", step);
}

// ---- dynamic light --------------------------------------------------------

// Shots and explosions lighting the room, from the table in
// data/dynlight.txt. Shown as off, whatever the setting says, while the
// lighting is on the 1995 path: that path remaps palette indices through a
// table that only darkens, so there is nothing for this to add light to.
void dynlight_step(int)
{
    render::set_dynlight_enabled(!render::dynlight_enabled());
}

void dynlight_show(char *buf, size_t n)
{
    bool const on = render::dynlight_enabled() && render::rgb_lighting();
    snprintf(buf, n, "%s", say(on ? i18n::kOn : i18n::kOff));
}

void dynlight_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::dynlight_enabled() ? "on" : "off");
}

// ---- reduce motion --------------------------------------------------------

// The veto over every effect that moves the picture by itself. It does not
// clear the individual settings, so turning it back off restores what the
// player had chosen. Takes effect at once, like the settings it covers.
void reduce_motion_step(int)
{
    render::options().reduce_motion = !render::options().reduce_motion;
}

void reduce_motion_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(render::options().reduce_motion
                                   ? i18n::kOn : i18n::kOff));
}

void reduce_motion_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::options().reduce_motion ? "on" : "off");
}

// ---- lighting -------------------------------------------------------------

// Whether the frame is lit by remapping palette indices, as it has been since
// 1995, or in RGB on the way to the window. Same curve either way; what the
// second one drops is the snap to the nearest of 256 colours, which is what
// bands every dark corner. Takes effect on the next frame drawn.
void light_step(int)
{
    render::set_rgb_lighting(!render::rgb_lighting());
}

void light_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(render::rgb_lighting()
                                   ? i18n::kLightRgb : i18n::kLightClassic));
}

void light_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::rgb_lighting() ? "on" : "off");
}

// ---- language -------------------------------------------------------------

i18n::Language const kLangs[] = {
    i18n::Language::English, i18n::Language::French, i18n::Language::German,
    i18n::Language::Portuguese, i18n::Language::Pseudo
};

void lang_step(int dir)
{
    int n = (int)(sizeof(kLangs) / sizeof(kLangs[0]));
    int at = 0;
    for (int i = 0; i < n; i++)
        if (kLangs[i] == i18n::language())
            at = i;
    i18n::language() = kLangs[list_wrap(at, dir, n)];

    // Reloads the symbol table and the font, so the screen itself changes
    // language under the cursor instead of promising to next time.
    apply_language_change();
}

void lang_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", i18n::language_name(i18n::language()));
}

// ---- font -----------------------------------------------------------------

void font_step(int) { set_extended_font(!extended_font_wanted()); }
void font_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", extended_font_wanted() ? "extended" : "classic");
}

// ---- video ----------------------------------------------------------------

void scale_step(int dir)
{
    render::ScaleMode const modes[] = { render::ScaleMode::Integer,
                                        render::ScaleMode::Fit,
                                        render::ScaleMode::Stretch };
    int at = 0;
    for (int i = 0; i < 3; i++)
        if (modes[i] == render::options().scale)
            at = i;
    render::options().scale = modes[list_wrap(at, dir, 3)];
    apply_presentation();
}

void scale_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::scale_mode_name(render::options().scale));
}

void filter_step(int dir)
{
    render::Filter const filters[] = { render::Filter::Nearest,
                                       render::Filter::Linear,
                                       render::Filter::PixelArt,
                                       render::Filter::Scale2x };
    // Scale2x is a shader, so it is only on the list when the GPU path
    // is the one selected. Offering it otherwise would show a name and
    // draw nearest.
    int const count = render::options().backend == render::Backend::Gpu ? 4 : 3;
    int at = 0;
    for (int i = 0; i < count; i++)
        if (filters[i] == render::options().filter)
            at = i;
    render::options().filter = filters[list_wrap(at, dir, count)];
    apply_filter();
}

void filter_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::filter_name(render::options().filter));
}

void vsync_step(int) { render::options().vsync = !render::options().vsync;
                       apply_presentation(); }
void vsync_show(char *buf, size_t n)
{
    snprintf(buf, n, "%s", say(render::options().vsync ? i18n::kOn : i18n::kOff));
}
void vsync_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", render::options().vsync ? "true" : "false");
}

void fps_step(int dir)
{
    int const steps[] = { 0, 30, 60, 75, 120, 144, 240 };
    int const count = (int)(sizeof(steps) / sizeof(steps[0]));
    int at = 0;
    for (int i = 0; i < count; i++)
        if (steps[i] == render::options().fps_limit)
            at = i;
    render::options().fps_limit = steps[list_wrap(at, dir, count)];
}

void fps_show(char *buf, size_t n)
{
    if (render::options().fps_limit <= 0)
        snprintf(buf, n, "%s", say(i18n::kOff));
    else
        snprintf(buf, n, "%d", render::options().fps_limit);
}

// ---- input ----------------------------------------------------------------

void deadzone_step(int dir)
{
    input::deadzone().inner =
        clamp_step(input::deadzone().inner, dir, 1024, 0, 24576);
}
void deadzone_show(char *buf, size_t n)
{
    snprintf(buf, n, "%d", input::deadzone().inner);
}

void aim_step(int dir)
{
    input::aim_settings().radius =
        clamp_step(input::aim_settings().radius, dir, 8, 16, 240);
}
void aim_show(char *buf, size_t n)
{
    snprintf(buf, n, "%d", input::aim_settings().radius);
}

void assist_step(int dir)
{
    input::assist_settings().strength =
        clamp_step(input::assist_settings().strength, dir, 5, 0, 100);
}
void assist_show(char *buf, size_t n)
{
    snprintf(buf, n, "%d%%", input::assist_settings().strength);
}

void rumble_step(int dir)
{
    input::rumble_settings().strength =
        clamp_step(input::rumble_settings().strength, dir, 10, 0, 100);
}
void rumble_show(char *buf, size_t n)
{
    snprintf(buf, n, "%d%%", input::rumble_settings().strength);
}

void cursor_step(int dir)
{
    input::cursor_settings().speed =
        clamp_step(input::cursor_settings().speed, dir, 2, 4, 64);
}
void cursor_show(char *buf, size_t n)
{
    snprintf(buf, n, "%d", input::cursor_settings().speed);
}

// ---- which sound set ------------------------------------------------------

// Free pack or the 1995 one, where the player has installed the 1995 one.
//
// Next launch, like the mode: the overlay that serves the original sound is
// set once, before the data directory is read, and sounds already loaded
// would keep playing from wherever they came from.
void sound_set_step(int)
{
    flags.classic_sfx = !flags.classic_sfx;
}

void sound_set_show(char *buf, size_t n)
{
    // Said plainly when the choice has no teeth: the row stays visible so
    // the player learns the original can be had, and the classic data
    // screen is where it is fetched.
    if (!data::classic_data_present())
        snprintf(buf, n, "%s", say(i18n::kSoundNotHere));
    else
        snprintf(buf, n, "%s", say(flags.classic_sfx ? i18n::kSoundClassic
                                                     : i18n::kSoundFree));
}

void sound_set_value(char *buf, size_t n)
{
    snprintf(buf, n, "%s", flags.classic_sfx ? "on" : "off");
}

// The value written to the config is the shown one for everything whose
// display is already the config syntax.
void same_as_shown(char *buf, size_t n) { (void)buf; (void)n; }

Item const kItems[] = {
    { i18n::kOptMode,        "mode",        true,  mode_step,     mode_show,     mode_value },
    { i18n::kOptAspect,      "aspect",      true,  aspect_step,   aspect_show,   aspect_value },
    { i18n::kOptSound,       "classicsfx",  true,  sound_set_step, sound_set_show, sound_set_value },
    { i18n::kOptHud,         "hud",         false, hud_step,      hud_show,      hud_value },
    { i18n::kOptSmooth,      "interpolate", false, smooth_step,   smooth_show,   smooth_value },
    { i18n::kOptPreset,      "preset",      true,  preset_step,    preset_show,    preset_value },
    { i18n::kOptParticles,   "particles",   false, particles_step, particles_show, particles_value },
    { i18n::kOptBrightness,  "gamma",       false, brightness_step, brightness_show, NULL },
    { i18n::kOptDynLight,    "dynlight",    false, dynlight_step, dynlight_show, dynlight_value },
    { i18n::kOptReduceMotion, "reducemotion", false, reduce_motion_step, reduce_motion_show, reduce_motion_value },
    { i18n::kOptLighting,    "rgblight",    false, light_step,    light_show,    light_value },
    { i18n::kOptLanguage,    "language",    false, lang_step,     lang_show,     NULL },
    { i18n::kOptFont,        "font",        true,  font_step,     font_show,     NULL },
    { i18n::kOptScaleMode,   "scalemode",   false, scale_step,    scale_show,    NULL },
    { i18n::kOptFilter,      "filter",      false, filter_step,   filter_show,   NULL },
    { i18n::kOptVsync,       "vsync",       false, vsync_step,    vsync_show,    vsync_value },
    { i18n::kOptFpsLimit,    "fpslimit",    false, fps_step,      fps_show,      NULL },
    { i18n::kOptDeadzone,    "deadzone",    false, deadzone_step, deadzone_show, NULL },
    { i18n::kOptAimRadius,   "aimradius",   false, aim_step,      aim_show,      NULL },
    { i18n::kOptAimAssist,   "aimassist",   false, assist_step,   assist_show,   NULL },
    { i18n::kOptRumble,      "rumble",      false, rumble_step,   rumble_show,   NULL },
    { i18n::kOptCursorSpeed, "cursorspeed", false, cursor_step,   cursor_show,   NULL },
};

int const kItemCount = (int)(sizeof(kItems) / sizeof(kItems[0]));

// Colours for the notice, which is not a list and draws itself.
Colour const kInk     = rgba(210, 210, 220);
Colour const kPick    = rgba(255, 220, 120);

using i18n::say;

char const *changed_keys[kItemCount] = {};

void value_string(Item const &it, char *buf, size_t n)
{
    if (it.value)
        it.value(buf, n);
    else
        it.show(buf, n);
}

}

int options_item_count()
{
    return kItemCount;
}

bool handle_global_key(Event &ev)
{
    if (ev.type != EV_KEY)
        return false;

    if (ev.key == JK_F2)
    {
        run_options_screen();
        return true;
    }
    if (ev.key == JK_F3)
    {
        run_rebind_screen();
        return true;
    }
    return false;
}

bool draw_pad_lost_notice()
{
    if (!input::pad_lost())
        return false;

    int w = 0, h = 0;
    if (!window_pixel_size(w, h))
        return false;

    Overlay &ov = overlay();
    if (!ov.Begin(w, h))
        return false;

    HexFont const &font = overlay_font();
    if (font.Empty())
        return false;

    int s = list_scale_for(h);
    int cell = 8 * s;
    int row = cell + s * 3;

    char const *lines[2] = { say(i18n::kPadLost), say(i18n::kPadLostHelp) };
    int widest = 0;
    for (char const *l : lines)
    {
        int n = Overlay::TextWidth(l, s);
        if (n > widest)
            widest = n;
    }

    int box_w = widest + cell * 2;
    int box_h = row * 2 + cell;
    int bx = (w - box_w) / 2;
    int by = h / 3;

    ov.FillRect(bx, by, box_w, box_h, rgba(20, 8, 8, 225));
    ov.FrameRect(bx, by, box_w, box_h, rgba(200, 120, 120));

    int y = by + cell / 2;
    ov.Text(font, (w - Overlay::TextWidth(lines[0], s)) / 2, y, lines[0], s, kPick);
    y += row;
    ov.Text(font, (w - Overlay::TextWidth(lines[1], s)) / 2, y, lines[1], s, kInk);

    return true;
}

void draw_options_hint()
{
    int w = 0, h = 0;
    if (!window_pixel_size(w, h))
        return;

    Overlay &ov = overlay();
    if (!ov.Begin(w, h))
        return;

    HexFont const &font = overlay_font();
    if (font.Empty())
        return;

    int s = list_scale_for(h);
    int cell = 8 * s;

    char const *hint = say(i18n::kMenuHint);
    int tw = Overlay::TextWidth(hint, s);
    int x = (w - tw) / 2;
    // At the top: the menu's own icons run down the right edge and the eye
    // starts there, and a line at the very bottom reads as a status bar.
    int y = cell;

    ov.FillRect(x - cell / 2, y - s * 2, tw + cell, cell + s * 4,
                rgba(0, 0, 0, 170));
    ov.Text(font, x, y, hint, s, kInk);
}

void draw_options_screen(int selected)
{
    Row rows[kItemCount];
    char values[kItemCount][80];

    for (int i = 0; i < kItemCount; i++)
    {
        char value[64];
        kItems[i].show(value, sizeof(value));
        // The arrows say the row is changeable, which nothing else on it does.
        snprintf(values[i], sizeof(values[i]), "< %s >", value);

        rows[i].label = say(kItems[i].label);
        rows[i].value = values[i];
        rows[i].marked = kItems[i].restart;
    }

    char const *footers[2] = { say(i18n::kOptionsHelp), say(i18n::kRestartNote) };
    draw_list(say(i18n::kOptionsTitle), rows, kItemCount, selected, footers, 2);
}


namespace {

// Labels for the eight actions, in the player's words rather than the wire
// names that go into abuserc.
i18n::Phrase const kActionLabels[] = {
    i18n::kActMoveLeft, i18n::kActMoveRight, i18n::kActUp, i18n::kActDown,
    i18n::kActFire, i18n::kActSpecial, i18n::kActWeaponPrev,
    i18n::kActWeaponNext,
};

// A row's bindings joined into one line, or the "not bound" note.
void bindings_line(input::Action a, char *out, size_t n)
{
    input::ActionMap const &map = mutable_action_map();
    out[0] = 0;
    size_t used = 0;

    for (int j = 0; j < input::kMaxBindings; j++)
    {
        input::Binding const &b = map.binding(a, j);
        if (b.empty())
            continue;

        char one[64];
        describe_binding(b, one, sizeof(one));
        if (!one[0])
            continue;

        int wrote = snprintf(out + used, n - used, "%s%s",
                             used ? ", " : "", one);
        if (wrote < 0 || (size_t)wrote >= n - used)
            break;
        used += (size_t)wrote;
    }

    if (!used)
        snprintf(out, n, "%s", say(i18n::kUnbound));
}

}

void draw_rebind_screen(int selected, bool capturing)
{
    int const count = (int)input::Action::Count;

    Row rows[(int)input::Action::Count];
    char values[(int)input::Action::Count][256];

    for (int i = 0; i < count; i++)
    {
        bindings_line((input::Action)i, values[i], sizeof(values[i]));
        if (i == selected && capturing)
            snprintf(values[i], sizeof(values[i]), "%s", "...");

        rows[i].label = say(kActionLabels[i]);
        rows[i].value = values[i];
        rows[i].marked = false;
    }

    char const *footer = say(capturing ? i18n::kPressAny : i18n::kControlsHelp);
    draw_list(say(i18n::kControlsTitle), rows, count, selected, &footer, 1);
}


namespace {

// Is anything on the pad still held?
bool pad_anything_down()
{
    for (int i = 0; i < input::PadState::kMaxButtons; i++)
        if (input::pad_state().button(i))
            return true;

    for (int i = 0; i < input::PadState::kMaxAxes; i++)
    {
        input::Deadzone const &dz = input::deadzone_for_axis(i);
        if (input::pad_state().axis_active(i, -1, dz)
            || input::pad_state().axis_active(i, 1, dz))
            return true;
    }
    return false;
}

// The button that opened the capture is still held when the capture starts,
// and the stick that walked down the list is often still deflected. Without
// this the confirm button binds itself to the row it was used to pick.
void wait_for_release(int selected)
{
    while (pad_anything_down())
    {
        draw_rebind_screen(selected, true);
        wm->flush_screen();

        // Drain what is queued, so a key release is seen and the loop is not
        // spinning on a stale event.
        while (wm->IsPending())
        {
            Event ev;
            wm->get_event(ev);
        }
    }

    while (wm->IsPending())
    {
        Event ev;
        wm->get_event(ev);
    }
}

// Waits for one key, pad button or stick push and turns it into a binding.
// Reads the pad from its state rather than from events: a button arrives as a
// mapped key code, which is the binding we already have, not the one being
// made.
bool capture_binding(int selected, input::Binding &out)
{
    wait_for_release(selected);

    while (true)
    {
        draw_rebind_screen(selected, true);
        wm->flush_screen();

        for (int i = 0; i < input::PadState::kMaxButtons; i++)
            if (input::pad_state().button(i))
            {
                out.source = input::Source::PadButton;
                out.code = i;
                out.sign = 0;
                return true;
            }

        for (int i = 0; i < input::PadState::kMaxAxes; i++)
        {
            input::Deadzone const &dz = input::deadzone_for_axis(i);
            for (int sign : { -1, 1 })
                if (input::pad_state().axis_active(i, sign, dz))
                {
                    out.source = input::Source::PadAxis;
                    out.code = i;
                    out.sign = sign;
                    return true;
                }
        }
        if (!wm->IsPending())
            continue;

        Event ev;
        wm->get_event(ev);
        if (ev.type != EV_KEY)
            continue;
        if (ev.key == JK_ESC)
            return false;

        out.source = input::Source::Key;
        out.code = ev.key;
        out.sign = 0;
        return true;
    }
}

}

void run_rebind_screen()
{
    int const count = (int)input::Action::Count;
    int selected = 0;
    bool quit = false;
    bool touched = false;

    while (!quit)
    {
        draw_rebind_screen(selected, false);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);
        if (ev.type != EV_KEY)
            continue;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, count);
            break;
        case JK_DOWN:
            selected = list_wrap(selected, 1, count);
            break;
        case JK_ENTER:
        {
            input::Binding b;
            if (capture_binding(selected, b))
            {
                mark_explicit_binds();
                if (mutable_action_map().add((input::Action)selected, b))
                    touched = true;
                else
                    printf("Controls: %s\n", say(i18n::kNoRoomForMore));
            }
            break;
        }
        case JK_BACKSPACE:
            mark_explicit_binds();
            mutable_action_map().clear((input::Action)selected);
            touched = true;
            break;
        case JK_ESC:
        case JK_F3:
            quit = true;
            break;
        default:
            break;
        }
    }

    if (touched)
    {
        char const *path = config_file_path();
        if (data::save_config_lines(path, "bind", format_all_bindings()))
            printf("Controls: %s (%s)\n", say(i18n::kSaved), path);
        else
            printf("Controls: %s (%s)\n", say(i18n::kNotSaved), path);
    }

    overlay().Clear();
    wm->flush_screen();
}

void run_options_screen()
{
    int selected = 0;
    bool quit = false;

    for (int i = 0; i < kItemCount; i++)
        changed_keys[i] = NULL;

    while (!quit)
    {
        draw_options_screen(selected);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);

        if (ev.type != EV_KEY)
            continue;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, kItemCount);
            break;
        case JK_DOWN:
            selected = list_wrap(selected, 1, kItemCount);
            break;
        case JK_LEFT:
        case JK_RIGHT:
            kItems[selected].step(ev.key == JK_RIGHT ? 1 : -1);
            changed_keys[selected] = kItems[selected].key;
            break;
        case JK_ESC:
        case JK_ENTER:
        case JK_F2:
            quit = true;
            break;
        default:
            break;
        }
    }

    // Write back only what was touched, so the screen never rewrites a key the
    // player did not ask it to.
    bool wrote = false, failed = false;
    char const *path = config_file_path();
    for (int i = 0; i < kItemCount; i++)
    {
        if (!changed_keys[i])
            continue;

        // The brightness is written to gamma.lsp the moment it changes,
        // by set_gamma_value, because that file is where this has been
        // kept since 1995 and the Lisp reads it at startup. Writing it to
        // abuserc as well would give one setting two homes.
        if (strcmp(changed_keys[i], "gamma") == 0)
        {
            wrote = true;
            continue;
        }

        // The mode is the one setting that cannot live in abuserc: that file
        // is inside a directory named after the mode. It has a file of its
        // own, a level above them.
        if (strcmp(changed_keys[i], "mode") == 0)
        {
            data::Env env = data::system_env();
            if (data::save_mode(env, data::mode()))
                wrote = true;
            else
                // Reported here rather than through `failed`, which would
                // name abuserc: this is a different file.
                printf("Mode: could not write %s\n",
                       data::mode_file(env).c_str());
            continue;
        }

        char value[64];
        value_string(kItems[i], value, sizeof(value));
        if (data::save_config_key(path, changed_keys[i], value))
            wrote = true;
        else
            failed = true;
    }

    overlay().Clear();
    wm->flush_screen();

    if (failed)
        printf("Options: %s (%s)\n", say(i18n::kNotSaved), path);
    else if (wrote)
        printf("Options: %s (%s)\n", say(i18n::kSaved), path);
}

}
