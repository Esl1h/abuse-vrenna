/*
 *  Abuse - dark 2D side-scrolling platform game
 *
 *  See classic_data_screen.h.
 *
 *  This software was released into the Public Domain.
 */

#if defined HAVE_CONFIG_H
#   include "config.h"
#endif

#include "common.h"

#include "classic_data_screen.h"

#include <stdio.h>
#include <string.h>

#include <string>

#include <SDL3/SDL.h>

#include "data/fetch_classic.h"
#include "data/paths.h"
#include "i18n/uitext.h"
#include "jwindow.h"
#include "keys.h"
#include "loader2.h"
#include "menu_list.h"
#include "overlay.h"
#include "sdlport/setup.h"
#include "sdlport/sound.h"
#include "specs.h"

extern WindowManager *wm;
extern flags_struct flags;

namespace abuse::ui {

namespace {

using i18n::say;

enum Choice
{
    Fetch,
    OpenPage,
    PlayAnyway,
    ChoiceCount
};

// Set once the player has asked for something, so the screen can say what
// happened instead of looking like nothing did.
i18n::Phrase const *g_result = nullptr;

// Whether there is a fetcher to run at all. Asked once: a screen that
// offers a choice which cannot work is worse than one that does not offer
// it, and in a build with no script the player still has the page and the
// address.
bool fetcher_here()
{
    static bool const yes = !data::fetch_script().empty();
    return yes;
}

// Puts the data that just arrived to use, without a restart.
//
// sound_init ran at startup, found no sfx directory and gave up with no
// device at all, so a download that works still leaves the game mute: the
// files are there and the mixer is not. Two things fix that, and both have
// to happen here because setup() is long past.
void use_what_arrived()
{
    // Where the Original mode's sound lives. open_file consults this
    // overlay, and it is what setup() sets when the data was already
    // installed at startup.
    //
    // Only when that sound is what the player asked for. The overlay wins
    // over the free pack for every file the original has, so setting it
    // from the Remastered mode would swap the sound under a player who
    // chose the free one, and the next launch would swap it back.
    if (data::mode() == data::Mode::Original || flags.classic_sfx)
    {
        char *classic = SDL_strdup(data::classic_data_dir().c_str());
        set_fallback_filename_prefix(classic);
        SDL_free(classic);
    }

    sound_avail = sound_retry();
    if (!(sound_avail & SFX_INITIALIZED))
        printf("Classic data: downloaded, and sound did not come up; "
               "restarting the game will use it\n");
}

// Runs the download and keeps drawing while it runs.
//
// Its own loop, because the one below waits for an event and a download
// that prints a line every few seconds would freeze the screen between
// them. Events are taken only when there are any, so Esc still answers.
void run_fetch(int selected)
{
    if (!data::fetch_start())
    {
        g_result = &i18n::kClassicFailed;
        return;
    }

    for (;;)
    {
        data::FetchState const state = data::fetch_poll();
        if (state != data::FetchState::Running)
        {
            if (state == data::FetchState::Done)
            {
                g_result = &i18n::kClassicGot;
                use_what_arrived();
            }
            else
                g_result = &i18n::kClassicFailed;
            return;
        }

        draw_classic_data_screen(selected);
        wm->flush_screen();

        while (wm->IsPending())
        {
            Event ev;
            wm->get_event(ev);
            if (ev.type == EV_KEY && ev.key == JK_ESC)
            {
                data::fetch_reset();
                g_result = &i18n::kClassicFailed;
                return;
            }
        }

        SDL_Delay(30);
    }
}

}

bool classic_data_missing()
{
    return data::mode() == data::Mode::Original && !data::classic_data_present();
}

void draw_classic_data_screen(int selected)
{
    // The two choices carry no value column: the address and the path are long
    // enough to dominate the panel width, and a wide panel is a small one,
    // because the text shrinks until it fits.
    Row rows[ChoiceCount];
    rows[Fetch].label = say(i18n::kClassicFetch);
    rows[OpenPage].label = say(i18n::kClassicOpen);
    rows[PlayAnyway].label = say(i18n::kClassicGoOn);

    // Held in a local: classic_data_dir returns by value, and the pointer into
    // a temporary would be dangling by the time the list is drawn.
    std::string where = data::classic_data_dir();

    char const *footers[8];
    int n = 0;
    footers[n++] = say(i18n::kClassicWhy1);
    footers[n++] = say(i18n::kClassicWhy2);
    footers[n++] = data::classic_data_url();
    footers[n++] = say(i18n::kClassicScript);
    footers[n++] = where.c_str();

    // While it runs, what the script is saying, which is the only honest
    // progress report available: it prints a line per file.
    if (data::fetch_poll() == data::FetchState::Running)
    {
        footers[n++] = say(i18n::kClassicFetching);
        footers[n++] = say(i18n::kClassicCancel);
        char const *line = data::fetch_message();
        if (line[0])
            footers[n - 1] = line;
    }
    else if (g_result)
        footers[n++] = say(*g_result);

    draw_list(say(data::mode() == data::Mode::Original
                      ? i18n::kClassicTitle : i18n::kClassicTitleOptional),
              rows, ChoiceCount, selected, footers, n);
}

void run_classic_data_screen()
{
    int selected = 0;
    bool quit = false;
    g_result = nullptr;

    while (!quit)
    {
        draw_classic_data_screen(selected);
        wm->flush_screen();

        Event ev;
        wm->get_event(ev);
        if (ev.type != EV_KEY)
            continue;

        switch (ev.key)
        {
        case JK_UP:
            selected = list_wrap(selected, -1, ChoiceCount);
            break;
        case JK_DOWN:
            selected = list_wrap(selected, 1, ChoiceCount);
            break;
        case JK_ENTER:
        case JK_SPACE:
            if (selected == Fetch)
            {
                if (fetcher_here())
                {
                    run_fetch(selected);
                    // The data is here, and nothing else on this screen
                    // matters once it is. Asked of the disk and not of
                    // classic_data_missing(), which is only ever true in
                    // the Original mode: from any other mode it answered
                    // "not missing" before the download had even run, and
                    // the screen closed on a failure without saying so.
                    if (data::classic_data_present())
                        quit = true;
                }
                else
                    g_result = &i18n::kClassicFailed;
            }
            else if (selected == OpenPage)
            {
                // The whole of the download feature. A browser is a thing every
                // desktop has, and this needs no library that the game would
                // then have to ship on three systems.
                if (SDL_OpenURL(data::classic_data_url()))
                    g_result = &i18n::kClassicOpened;
                else
                {
                    g_result = &i18n::kClassicNoOpen;
                    printf("Classic data: %s\n", SDL_GetError());
                }
            }
            else
                quit = true;
            break;
        case JK_ESC:
            quit = true;
            break;
        default:
            break;
        }
    }

    overlay().Clear();
    wm->flush_screen();
}

}
