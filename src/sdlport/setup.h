/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef _SETUP_H_
#define _SETUP_H_

struct flags_struct
{
    short fullscreen;
    short nosound;
    short grabmouse;
    short xres;
    short yres;

    // Whether the Remastered mode plays the original sound where the player
    // has installed it, instead of the free pack that ships with the game.
    //
    // Off, which is the opposite of what it was: it defaulted to on when the
    // mode had no sound of its own and a mute game was worse than a borrowed
    // one. The mode has 73 effects and 12 tracks now, and borrowing over them
    // would hide what the game ships with. The player chooses, in the options
    // or with classicsfx=on in abuserc.
    //
    // Never applies to what is distributed: this is the player's own copy of
    // data they downloaded themselves.
    bool classic_sfx = false;
};

struct keys_struct
{
    int left;
    int left_2;
    int right;
    int right_2;
    int up;
    int up_2;
    int down;
    int down_2;
    int b1;
    int b2;
    int b3;
    int b4;
};

// Where abuserc was actually read from, resolved once in setup(). The options
// screen writes back to the same file rather than working it out again, so the
// two can never disagree.
char const *config_file_path();

// True when abuserc or the command line named a language. False means nobody
// has chosen: the language came from the system locale, or from the default.
bool language_was_configured();

#endif // _SETUP_H_
