/*
 *  Abuse - dark 2D side-scrolling platform game
 *  Copyright (c) 1995 Crack dot Com
 *  Copyright (c) 2005-2011 Sam Hocevar <sam@hocevar.net>
 *
 *  This software was released into the Public Domain. As with most public
 *  domain software, no warranty is made or implied by Crack dot Com, by
 *  Jonathan Clark, or by Sam Hocevar.
 */

#ifndef __SOUND_H__
#define __SOUND_H__

#include <SDL3_mixer/SDL_mixer.h>
#ifdef MUSIC_NATIVE_MIDI
#include <SDL3_native_midi/SDL_native_midi.h>
#endif

/* options are passed via command line */

#define SFX_INITIALIZED    1
#define MUSIC_INITIALIZED  2

int sound_init(int argc, char **argv);
void sound_uninit();
void print_sound_options(); // print the options avaible for sound

class sound_effect
{
public:
    sound_effect(char const *filename);
    ~sound_effect();

    void play(int volume = 127, int pitch = 128, int panpot = 128);

private:
    // Must start null: the constructor returns early when sound is off or the
    // file is missing, and an uninitialised pointer reaching MIX_SetTrackAudio
    // crashes inside SDL_mixer.
    MIX_Audio* m_chunk = NULL;
};

class song
{
public:
    char const *name() { return Name; }
    song(char const *filename);
    void play(unsigned char volume=127);
    void stop(long fadeout_time=0); // time in ms
    int playing();
    void set_volume(int volume);
    ~song();

private:
    char *Name;
    unsigned char *data;
    unsigned long song_id;
#ifdef MUSIC_NATIVE_MIDI
    NativeMidi_Song* music;
#else
    MIX_Audio* music;
    MIX_Track* activeTrack;
#endif
    SDL_IOStream* rw;

    // A free track standing in for the MIDI one.
    //
    // The Lisp asks for music/abuse01.hmi and always will, because the
    // engine and the Lisp are coupled by those names; the same widening the
    // sound effects get applies here, so music/abuse01.ogg is taken when it
    // is there. These two are set only then, and every method below asks
    // them first, which is also what makes this work on a build with native
    // MIDI compiled in: that path cannot play Vorbis at all.
    MIX_Audio* m_free_music = NULL;
    MIX_Track* m_free_track = NULL;
};

#endif

