/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See start_menu.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "start_menu.h"

#include <stdio.h>
#include <string.h>

#include "cache.h"
#include "classic_data_screen.h"
#include "compat.h"
#include "clisp.h"
#include "data/paths.h"
#include "dev.h"
#include "game.h"
#include "gamma.h"
#include "i18n/uitext.h"
#include "jwindow.h"
#include "keys.h"
#include "level.h"
#include "loader2.h"
#include "loadgame.h"
#include "menu.h"
#include "menu_list.h"
#include "options_screen.h"
#include "overlay.h"
#include "timing.h"
#include "video.h"

extern WindowManager *wm;
extern palette *pal;

namespace abuse::ui {

namespace {

using i18n::say;

bool g_classic = false;

// See draw_start_menu_pinned.
bool g_pinned = false;

// Every row the menu can show. Which of them it does show depends on whether
// a level is loaded and whether there is a save on disk.
enum Action
{
    ActResume,
    ActPlay,
    ActContinue,
    ActDifficulty,
    ActMode,
    ActBrightness,
    ActVolume,
    ActOriginalSound,
    ActOptions,
    ActControls,
    ActCredits,
    ActQuit,
    ActCount
};

// ---- difficulty -----------------------------------------------------------

// In the order the classic menu offered them, which is also the order they
// read in: easiest first.
LSymbol *const *difficulty_levels()
{
    static LSymbol *const levels[4] = { l_easy, l_medium, l_hard, l_extreme };
    return levels;
}

int difficulty_index()
{
    if (g_pinned)
        return 1;

    LSymbol *const *levels = difficulty_levels();
    if (DEFINEDP(symbol_value(l_difficulty)))
        for (int i = 0; i < 4; i++)
            if (symbol_value(l_difficulty) == levels[i])
                return i;
    return 1;   // what clisp.cpp starts from, and what save_difficulty writes
}

char const *difficulty_name(int i)
{
    switch (i)
    {
    case 0:  return say(i18n::kDiffEasy);
    case 2:  return say(i18n::kDiffHard);
    case 3:  return say(i18n::kDiffExtreme);
    default: return say(i18n::kDiffMedium);
    }
}

void step_difficulty(int dir)
{
    int i = list_wrap(difficulty_index(), dir, 4);
    l_difficulty->SetValue(difficulty_levels()[i]);
    save_difficulty();
}

// ---- the rows -------------------------------------------------------------

// The row writes the choice down as it is made: the menu has no OK button,
// and a mode that only applies next time has to survive being chosen.
void toggle_mode()
{
    data::Env env = data::system_env();
    data::set_mode(data::mode() == data::Mode::Original
                       ? data::Mode::Remaster : data::Mode::Original);
    if (!data::save_mode(env, data::mode()))
        printf("Mode: could not write %s\n", data::mode_file(env).c_str());
}

char const *mode_label()
{
    return say(data::mode() == data::Mode::Original
                   ? i18n::kModeOriginal : i18n::kModeRemaster);
}

// Fills `rows` with what the menu looks like right now and `acts` with what
// each of them does. Returns how many there are.
int build_rows(Row *rows, Action *acts)
{
    int n = 0;

    if (current_level)
    {
        rows[n].label = say(i18n::kStartResume);
        acts[n++] = ActResume;
    }

    rows[n].label = say(i18n::kStartPlay);
    acts[n++] = ActPlay;

    if (!g_pinned && show_load_icon())
    {
        rows[n].label = say(i18n::kStartContinue);
        acts[n++] = ActContinue;
    }

    rows[n].label = say(i18n::kStartDifficulty);
    rows[n].value = difficulty_name(difficulty_index());
    acts[n++] = ActDifficulty;

    // Marked: the data prefix, the sound and the save directory are all
    // chosen during startup, so a mode picked here is the one the next run
    // will play. See data::save_mode.
    rows[n].label = say(i18n::kOptMode);
    rows[n].value = mode_label();
    rows[n].marked = true;
    acts[n++] = ActMode;

    rows[n].label = say(i18n::kStartBrightness);
    acts[n++] = ActBrightness;

    rows[n].label = say(i18n::kStartVolume);
    acts[n++] = ActVolume;

    // Until the original sound is on the disk, in whichever mode. The
    // Remastered mode plays its free pack without it, so nothing else would
    // ever tell the player the 1995 one can be had. A pinned frame counts it
    // as missing: it cannot ask the disk, and the frame has to come out the
    // same on every machine.
    if (g_pinned || !data::classic_data_present())
    {
        rows[n].label = say(i18n::kStartOriginalSound);
        acts[n++] = ActOriginalSound;
    }

    rows[n].label = say(i18n::kOptionsTitle);
    acts[n++] = ActOptions;

    rows[n].label = say(i18n::kControlsTitle);
    acts[n++] = ActControls;

    rows[n].label = say(i18n::kStartCredits);
    acts[n++] = ActCredits;

    rows[n].label = say(i18n::kStartQuit);
    acts[n++] = ActQuit;

    return n;
}

// The title screen, redrawn under the panel. Only when there is no level: the
// menu reached with Esc from inside one sits over the frozen game, the way
// the icons used to.
void draw_backdrop()
{
    if (current_level || title_screen < 0)
        return;

    main_screen->clear();
    image *im = cache.img(title_screen);
    main_screen->PutImage(im, main_screen->Size() / 2 - im->Size() / 2);
    main_screen->AddDirty(ivec2(0), main_screen->Size());
}

// ---- credits --------------------------------------------------------------

void run_credits_screen()
{
    Row rows[4];
    rows[0].label = say(i18n::kCreditGame);
    rows[0].value = "Crack dot Com";
    rows[1].label = say(i18n::kCreditPort);
    rows[1].value = "Sam Hocevar";
    rows[2].label = say(i18n::kCreditFork);
    rows[2].value = "Xenoveritas";
    rows[3].label = say(i18n::kCreditThis);
    rows[3].value = "abuse.zoy.org";

    char const *footers[2] = { say(i18n::kCreditLicence), say(i18n::kCreditBack) };

    bool quit = false;
    while (!quit)
    {
        draw_backdrop();
        draw_list(say(i18n::kStartCredits), rows, 4, -1, footers, 2);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);
        if (ev.type != EV_KEY)
            continue;
        if (ev.key == JK_ESC || ev.key == JK_ENTER || ev.key == JK_SPACE)
            quit = true;
    }

    overlay().Clear();
}

// Every way out of the menu goes through here: the overlay belongs to the
// screen that drew it, and whatever comes next paints the whole window.
StartAction leave(StartAction a)
{
    overlay().Clear();
    wm->flush_screen();
    return a;
}

}

bool classic_start_menu()
{
    return g_classic;
}

void set_classic_start_menu(bool on)
{
    g_classic = on;
}

bool parse_start_menu_choice(char const *name, bool &classic)
{
    if (!name)
        return false;
    if (strcasecmp(name, "classic") == 0)
    {
        classic = true;
        return true;
    }
    if (strcasecmp(name, "modern") == 0)
    {
        classic = false;
        return true;
    }
    return false;
}

int start_menu_item_count()
{
    Row rows[ActCount];
    Action acts[ActCount];
    return build_rows(rows, acts);
}

void draw_start_menu(int selected)
{
    Row rows[ActCount];
    Action acts[ActCount];
    int count = build_rows(rows, acts);

    char const *footers[2] = { say(i18n::kStartHelp), say(i18n::kRestartNote) };

    // "Abuse" is the name of the game in every language, so the title is the
    // one string on this screen that is not translated.
    draw_list("Abuse", rows, count, selected, footers, 2);
}

void draw_start_menu_pinned(int selected)
{
    g_pinned = true;
    draw_start_menu(selected);
    g_pinned = false;
}

StartAction run_start_menu()
{
    Row rows[ActCount];
    Action acts[ActCount];
    int count = build_rows(rows, acts);

    int selected = 0;
    bool redraw = true;

    // The attract loop the game has always had: ten seconds without input and
    // the caller gets to play a demo.
    time_marker idle_since;

    for (;;)
    {
        if (redraw)
        {
            count = build_rows(rows, acts);
            if (selected >= count)
                selected = count - 1;
            draw_backdrop();
            draw_start_menu(selected);
            wm->flush_screen();
            redraw = false;
        }

        if (!wm->IsPending())
        {
            time_marker now;
            if (now.diff_time(&idle_since) > 10)
                return leave(StartAction::Idle);

            // Without this the menu spins on a core for nothing.
            Timer t;
            t.WaitMs(30);
            continue;
        }

        Event ev;
        wm->get_event(ev);
        idle_since.get_time();

        if (handle_global_key(ev))
        {
            the_game->reset_keymap();
            redraw = true;
            continue;
        }

        if (ev.type != EV_KEY)
            continue;

        redraw = true;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, count);
            continue;
        case JK_DOWN:
            selected = list_wrap(selected, 1, count);
            continue;
        case JK_LEFT:
        case JK_RIGHT:
        {
            int dir = ev.key == JK_RIGHT ? 1 : -1;
            if (acts[selected] == ActDifficulty)
                step_difficulty(dir);
            else if (acts[selected] == ActMode)
            {
                toggle_mode();
            }
            continue;
        }
        case JK_ESC:
            // Esc is how the player got here from a level, so it is also how
            // they go back. With no level there is nothing behind the menu.
            if (current_level)
                return leave(StartAction::Resume);
            continue;
        case JK_ENTER:
        case JK_SPACE:
            break;
        default:
            continue;
        }

        switch (acts[selected])
        {
        case ActResume:
            return leave(StartAction::Resume);
        case ActPlay:
            return leave(StartAction::Play);
        case ActContinue:
            return leave(StartAction::Continue);
        case ActQuit:
            if (confirm_quit())
                return leave(StartAction::Quit);
            break;
        case ActDifficulty:
            step_difficulty(1);
            break;
        case ActMode:
            toggle_mode();
            break;
        case ActBrightness:
            overlay().Clear();
            gamma_correct(pal, 1);
            break;
        case ActVolume:
            overlay().Clear();
            show_volume_window();
            break;
        case ActOriginalSound:
            overlay().Clear();
            run_classic_data_screen();
            break;
        case ActOptions:
            run_options_screen();
            break;
        case ActControls:
            run_rebind_screen();
            break;
        case ActCredits:
            run_credits_screen();
            break;
        case ActCount:
            break;
        }

        the_game->reset_keymap();
        idle_since.get_time();
    }
}

}
