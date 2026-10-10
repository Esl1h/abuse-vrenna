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
# include <Windows.h>
#endif
#include <cstring>

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

#ifdef MUSIC_NATIVE_MIDI
# include <SDL3_native_midi/SDL_native_midi.h>
#endif

#include "sound.h"

#include <string>

#include "audio/buses.h"
#include "audio/formats.h"
#include "audio/limiter.h"
#include "audio/voices.h"
#include "data/paths.h"
#include "hmi.h"
#include "specs.h"
#include "setup.h"
#include "util.h"

extern flags_struct flags;
static int sound_enabled = 0;
static SDL_AudioSpec audioObtained;

static MIX_Mixer* mixer = NULL;
// Very tempted to make this a std::vector and dynamically grow as needed
static MIX_Track** tracks = NULL;
static size_t numberTracks = 0;

// Phase 5, task 5.1. Which track a sound gets, and what happens when they are
// all busy: see audio/voices.h. The policy is there, free of SDL, so it can
// be tested without a device; this file only tells it what the hardware did.
static abuse::audio::VoicePool voices;

#ifdef MUSIC_NATIVE_MIDI
static bool haveNativeMidi = 0;
#endif

void allocate_tracks(size_t count)
{
    if (numberTracks == count)
    {
        return;
    }
    if (count == 0)
    {
        if (tracks != NULL)
        {
            for (size_t i = 0; i < numberTracks; i++)
            {
                MIX_DestroyTrack(tracks[i]);
            }
            SDL_free(tracks);
            tracks = NULL;
        }
        numberTracks = 0;
        return;
    }
    // Currently this will only ever allocate tracks up but in the future it
    // may make sense to enable some form of garbage collection
    // Destroy tracks past the new count
    for (size_t i = count; i < numberTracks; i++)
    {
        MIX_DestroyTrack(tracks[i]);
    }
    // Attempt to reallocate (note that when tracks is NULL this acts like
    // SDL_malloc so this is safe even on the first try)
    MIX_Track** newTracks = (MIX_Track**) SDL_realloc(tracks, sizeof(MIX_Track*) * count);
    if (newTracks == NULL)
    {
        printf("Audio: Unable to allocate tracks (out of memory)\n");
        return;
    }
    // Allocate any new tracks
    for (size_t i = numberTracks; i < count; i++)
    {
        newTracks[i] = MIX_CreateTrack(mixer);
        if (newTracks[i] == NULL)
        {
            printf("Error: Unable to allocate audio track: %s\n", SDL_GetError());
            // In this case set the number of tracks created to the current index
            count = i;
            break;
        }
    }
    numberTracks = count;
    tracks = newTracks;
    voices.reset((int)count);
}

// `priority` decides who gets a voice when they are all busy. Louder means
// more important, which is a decent proxy in a game that already attenuates
// by distance: a shot across the level does not silence one at the player's
// feet. A caller that knows better passes one of the named priorities.
MIX_Track* find_available_track(int priority)
{
    if (tracks == NULL)
    {
        return NULL;
    }

    // Only the mixer knows what finished on its own, so tell the policy
    // before asking it anything.
    for (size_t i = 0; i < numberTracks; i++)
        if (!MIX_TrackPlaying(tracks[i]))
            voices.release((int)i);

    int slot = voices.acquire(priority, SDL_GetTicks());
    if (slot == abuse::audio::VoicePool::kNone)
        return NULL;

    // Taken from something still sounding: stop it first, or the two overlap
    // on one track.
    if (MIX_TrackPlaying(tracks[slot]))
        MIX_StopTrack(tracks[slot], 0);

    return tracks[slot];
}

// The last thing the mixer does before handing the buffer to the device.
// Runs on the audio thread: it touches nothing but the limiter, whose own
// state is only written here and at startup.
static void SDLCALL post_mix( void *userdata, MIX_Mixer *m,
                              const SDL_AudioSpec *spec, float *pcm,
                              int samples )
{
    (void)userdata;
    (void)m;
    (void)spec;
    abuse::audio::limiter().process( pcm, samples );
}

//
// sound_init()
// Initialise audio
//
int sound_init( int argc, char **argv )
{
    char *sfxdir, *datadir;
    SDL_AudioSpec audiospec;

    // Disable sound if requested.
    if( flags.nosound )
    {
        // User requested that sound be disabled
        printf( "Sound: Disabled (-nosound)\n" );
        return 0;
    }

    if (!MIX_Init())
    {
        printf( "Sound: Failed to initialize: %s\n", SDL_GetError() );
        return 0;
    }
#ifdef MUSIC_NATIVE_MIDI
    if (NativeMidi_Init())
    {
        haveNativeMidi = 1;
    }
    else
    {
        printf("Sound: Failed to initialize MIDI. Music will not play.\n");
    }
#endif

    // Check for the sfx directory, disable sound if we can't find it.
    // In Original mode the sounds live in the overlay, not in the data
    // directory, so look there first when one is set.
    datadir = get_fallback_filename_prefix();
    if( datadir == NULL )
        datadir = get_filename_prefix();
    size_t len = SDL_strlen( datadir ) + 4;
    sfxdir = (char *)SDL_malloc( len );
    if (sfxdir == NULL)
    {
        printf( "Sound: out of memory\n" );
        return 0;
    }
    SDL_strlcpy( sfxdir, datadir, len );
    SDL_strlcat( sfxdir, "sfx", len );
#ifdef WIN32
    // Attempting to fopen a directory under Windows will fail, and
    // opendir does not exist. Use GetFileAttributes instead.
    if( GetFileAttributes( sfxdir ) == INVALID_FILE_ATTRIBUTES )
#else
    FILE *fd = NULL;
    if( (fd = fopen( sfxdir,"r" )) == NULL )
#endif
    {
        // Didn't find the directory, so disable sound. Not an error for the
        // Original mode, which has sound the moment its data is installed.
        // The Remastered mode ships its own, so there the path is the
        // useful part: a build whose data prefix is wrong says exactly
        // that and nothing else.
        if( abuse::data::mode() == abuse::data::Mode::Original )
            printf( "Sound: none yet. Original mode looks in %s;\n"
                    "       run scripts/fetch-classic-data.sh to install it.\n",
                    sfxdir );
        else
            printf( "Sound: no sfx directory at %s\n"
                    "       (the data directory is wrong, or the free pack is missing)\n",
                    sfxdir );
        SDL_free( sfxdir );
        return 0;
    }
#ifndef WIN32
    // The probe is an open handle on the directory and this is the branch
    // where it succeeded.
    fclose( fd );
#endif
    // SDL_free and not free: SDL_malloc allocated it, and a program that
    // hands SDL its own allocator with SDL_SetMemoryFunctions would be
    // returning this block to the wrong heap.
    SDL_free( sfxdir );

    audiospec.format = SDL_AUDIO_S16;
    audiospec.channels = 2;
    audiospec.freq = 44100;
    // The default device and never a named one. SDL3 opens a logical
    // device on top of the physical one and, for the default, moves it by
    // itself when the system default changes or headphones are plugged in
    // (SDL_audio.h, "One other benefit of logical devices"). Asking for a
    // particular device would buy a settings row and lose that, and the
    // game would go silent the moment someone plugged anything in.
    mixer = MIX_CreateMixerDevice(SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &audiospec);
    if (mixer == NULL)
    {
        printf( "Sound: Unable to open audio - %s\nSound: Disabled (error)\n", SDL_GetError() );
        return 0;
    }

    // Allocate 50 tracks
    allocate_tracks(50);

    // Phase 5, task 5.1: the master limiter, last in the chain. Abuse fires
    // a lot of short loud sounds at once and their sum clips; this rides the
    // whole mix down when it would go over and lets it back up gently.
    //
    // Never in the Original mode: that mode's sound is the reference, and
    // the reference includes what it does when it clips.
    abuse::audio::limiter().configure(audiospec.freq, audiospec.channels);
    if( abuse::data::mode() == abuse::data::Mode::Original )
        abuse::audio::limiter().set_enabled( false );
    if( !MIX_SetPostMixCallback( mixer, post_mix, NULL ) )
        printf( "Sound: no master limiter (%s)\n", SDL_GetError() );

    // FIXME
    //MIX_GetMixerFormat(&audioObtained.freq, &audioObtained.format, &tempChannels);
    audioObtained.channels = audiospec.channels;

    sound_enabled = SFX_INITIALIZED | MUSIC_INITIALIZED;

    printf( "Sound: Enabled\n" );

    // It's all good
    return sound_enabled;
}

int sound_retry()
{
    if (sound_enabled)
        return sound_enabled;

    // No argv: the only thing sound_init reads from it is the volume
    // switches, and those were parsed at startup and still hold.
    return sound_init(0, NULL);
}

//
// sound_uninit
//
// Shutdown audio and release any memory left over.
//
void sound_uninit()
{
    if (!sound_enabled)
        return;

    // Destroy all tracks
    allocate_tracks(0);
    MIX_DestroyMixer(mixer);
    MIX_Quit();
}

//
// sound_effect constructor
//
// Read in the requested .wav file.
//
sound_effect::sound_effect(char const *filename)
{
    if (!sound_enabled)
        return;

    // open_file, not jFILE: the Original mode serves its sounds from the
    // overlay, and only open_file consults it. Going straight to jFILE looked
    // in the data directory alone, found nothing, and left every sound without
    // a chunk, which is why the Original mode was silent and then crashed on
    // the first shot.
    //
    // The name the Lisp asks for always ends in .wav. In the Remastered mode
    // a Vorbis or FLAC file of the same name counts, because a free pack has
    // to fit in plain git; see audio/formats.h. The Original mode gets no
    // substitution at all.
    bool const substitutes =
        abuse::data::mode() != abuse::data::Mode::Original;

    bFILE *file = NULL;
    for (std::string const &name :
         abuse::audio::sound_candidates(filename, substitutes))
    {
        bFILE *candidate = open_file(name.c_str(), "rb");
        if (!candidate->open_failure())
        {
            // Said out loud, because a sound arriving from a file nobody
            // asked for is exactly the kind of thing that is impossible to
            // work out later from a silent game.
            if (name != filename)
                printf("Sound: %s served by %s\n", filename, name.c_str());
            file = candidate;
            break;
        }
        delete candidate;
    }

    if (!file)
    {
        // Once, for the same reason the registration says it once: with no
        // sound pack installed this is every sound in the game, and the
        // registration has already said so.
        static bool said = false;
        if (!said)
        {
            said = true;
            printf("Sound: no file for %s, and nothing else will be "
                   "reported\n", filename);
        }
        return;
    }

    bFILE &fp = *file;

    void *temp_data = SDL_malloc(fp.file_size());
    if (!temp_data)
    {
        delete file;
        return;
    }
    fp.read(temp_data, fp.file_size());
    SDL_IOStream *ios = SDL_IOFromMem(temp_data, fp.file_size());
    if (!ios)
    {
        SDL_free(temp_data);
        delete file;
        return;
    }

    // predecode = true, so the samples are copied out and temp_data can go.
    m_chunk = MIX_LoadAudio_IO(mixer, ios, true, true);
    if (!m_chunk)
        printf("Sound: could not decode %s: %s\n", filename, SDL_GetError());
    SDL_free(temp_data);
    delete file;
}

//
// sound_effect destructor
//
// Release the audio data.
//
sound_effect::~sound_effect()
{
    if(!sound_enabled)
        return;

    // Sound effect deletion only happens on level load, so there
    // is no problem in stopping everything. But the original playing
    // code handles the sound effects and the "playlist" differently.
    // Therefore with SDL_mixer, a sound that has not finished playing
    // on a level load will cut off in the middle. This is most noticable
    // for the button sound of the load savegame dialog.
    // FIXME: Original SDL did this
    // MIX_StopTag(-1, 100);
    // while (MIX_TrackPlaying(-1))
    //     SDL_Delay(10);
    // SDL3_mixer possibly fixes this - tracks with the audio will continue
    // playing after the audio is destroyed and SDL uses reference counting to
    // know when to free the audio.
    MIX_DestroyAudio(m_chunk);
}

//
// sound_effect::play
//
// Add a new sample for playing.
// panpot defines the pan position for the sound effect.
//   0   - Completely to the right.
//   128 - Centered.
//   255 - Completely to the left.
//
void sound_effect::play(int volume, int pitch, int panpot)
{
    if (!sound_enabled)
        return;
    // A sound whose file was missing or failed to decode has no chunk; playing
    // it must be a no-op, not a crash inside SDL_mixer.
    if (m_chunk == NULL)
        return;

    MIX_Track* track = find_available_track(volume);
    if (track == NULL)
        return;
    if (!MIX_SetTrackAudio(track, m_chunk))
        return;
    MIX_SetTrackGain(track, abuse::audio::voice_gain(abuse::audio::Bus::Sfx,
                                                    volume));
    MIX_StereoGains stereo;
    stereo.left = panpot / 255.0f;
    stereo.right = 1.0f - stereo.left;
    MIX_SetTrackStereo(track, &stereo);
    MIX_PlayTrack(track, 0);
}


// Play music using SDL_Mixer

song::song(char const * filename)
{
#ifndef MUSIC_NATIVE_MIDI
    activeTrack = NULL;
#endif
    data = NULL;
    Name = strdup(filename);
    song_id = 0;

    rw = NULL;
    music = NULL;

    // A free track first, where one exists. Same rule as the sound effects:
    // the name the Lisp asks for ends in .hmi, and outside the Original mode
    // a Vorbis or FLAC file of the same name counts. The Original mode gets
    // no substitution, because it plays what it was given.
    if (abuse::data::mode() != abuse::data::Mode::Original)
    {
        for (std::string const &candidate :
             abuse::audio::sound_candidates(filename, true))
        {
            if (candidate == filename)
                continue;

            std::string const path =
                std::string(get_filename_prefix() ? get_filename_prefix() : "")
                + candidate;

            m_free_music = MIX_LoadAudio(mixer, path.c_str(), false);
            if (m_free_music)
            {
                printf("Music: %s served by %s\n", filename, candidate.c_str());
                return;
            }
        }
    }

    // Built by hand instead of going through open_file, so the overlay has to
    // be applied here too: in Original mode the music lives with the sounds.
    char* realname = join_strings(get_filename_prefix(), filename);

    uint32_t data_size;
    data = load_hmi(realname, data_size);

    if (!data && get_fallback_filename_prefix())
    {
        char* alt = join_strings(get_fallback_filename_prefix(), filename);
        data = load_hmi(alt, data_size);
        if (data)
        {
            SDL_free(realname);
            realname = alt;
        }
        else
            SDL_free(alt);
    }

    if (!data)
    {
        printf("Sound: ERROR - could not load %s\n", realname);
        return;
    }

    rw = SDL_IOFromMem(data, data_size);
#ifdef MUSIC_NATIVE_MIDI
    music = NativeMidi_LoadSong_IO(rw, 0);
    if (!music)
    {
        printf("Sound: ERROR - could not load %s\n", realname);
        SDL_free(realname);
        return;
    }
#else
    SDL_PropertiesID props = SDL_CreateProperties();
    if (props == 0)
    {
        printf("Sound: ERROR - could not create properties: %s\n", SDL_GetError());
    }
    SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_IOSTREAM_POINTER, rw);
    SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_CLOSEIO_BOOLEAN, 0);
    SDL_SetBooleanProperty(props, MIX_PROP_AUDIO_LOAD_PREDECODE_BOOLEAN, 0);
    SDL_SetPointerProperty(props, MIX_PROP_AUDIO_LOAD_PREFERRED_MIXER_POINTER, mixer);
    // This is documented nowhere except the code, but the path to the sound font to use
    char* soundfont = join_strings(get_filename_prefix(), "VintageDreamsWaves-v2.sf2");
    SDL_SetStringProperty(props, "SDL_mixer.decoder.fluidsynth.soundfont_path", soundfont);
    music = MIX_LoadAudioWithProperties(props);

    if (!music)
    {
        printf("Sound: ERROR - %s while loading %s\n", SDL_GetError(), realname);
        SDL_free(realname);
        return;
    }
#endif
    SDL_free(realname);
}

song::~song()
{
    if(playing())
        stop();

    if (m_free_music)
    {
        MIX_DestroyAudio(m_free_music);
        m_free_music = NULL;
        free(Name);
        return;
    }
#ifndef MUSIC_NATIVE_MIDI
    // NULL out the active track - it may still exist if music was playing
    activeTrack = NULL;
#endif
    free(data);
    free(Name);

#ifdef MUSIC_NATIVE_MIDI
    NativeMidi_DestroySong(music);
#else
    MIX_DestroyAudio(music);
#endif
    SDL_free(rw);
}

void song::play( unsigned char volume )
{
    song_id = 1;

    if (m_free_music)
    {
        if (m_free_track == NULL)
        {
            m_free_track = find_available_track(abuse::audio::kUi);
            if (m_free_track == NULL)
                return;
        }
        MIX_SetTrackAudio(m_free_track, m_free_music);
        MIX_SetTrackGain(m_free_track,
                         abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                                  volume));
        // Looped, because a free track is a minute long and the MIDI it
        // stands in for played until the level ended.
        MIX_PlayTrack(m_free_track, 0);
        return;
    }

#ifdef MUSIC_NATIVE_MIDI
    NativeMidi_SetVolume(abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                                  volume));
    NativeMidi_Start(music, 0);
#else
    if (activeTrack == NULL)
    {
        activeTrack = find_available_track(abuse::audio::kUi);
        if (activeTrack == NULL)
            return;
    }
    MIX_SetTrackAudio(activeTrack, music);
    MIX_SetTrackGain(activeTrack,
                     abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                              volume));
    MIX_PlayTrack(activeTrack, 0);
#endif
}

void song::stop( long fadeout_time )
{
    song_id = 0;

    if (m_free_music)
    {
        if (m_free_track != NULL)
        {
            MIX_StopTrack(m_free_track,
                          MIX_TrackMSToFrames(m_free_track, fadeout_time));
            m_free_track = NULL;
        }
        return;
    }

#ifdef MUSIC_NATIVE_MIDI
    if (NativeMidi_Active())
    {
        NativeMidi_Stop();
    }
#else
    if (activeTrack != NULL)
    {
        MIX_StopTrack(activeTrack, MIX_TrackMSToFrames(activeTrack, fadeout_time));
        activeTrack = NULL;
    }
#endif
}

int song::playing()
{
    if (m_free_music)
        return m_free_track != NULL && MIX_TrackPlaying(m_free_track);

#ifdef MUSIC_NATIVE_MIDI
    return NativeMidi_Active();
#else
    return activeTrack != NULL && MIX_TrackPlaying(activeTrack);
#endif
}

void song::set_volume( int volume )
{
    if (m_free_music)
    {
        if (m_free_track != NULL)
            MIX_SetTrackGain(m_free_track,
                             abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                                      volume));
        return;
    }

#ifdef MUSIC_NATIVE_MIDI
    NativeMidi_SetVolume(abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                                  volume));
#else
    // TODO: Probably should persist this
    if (activeTrack != NULL)
    {
        MIX_SetTrackGain(activeTrack,
                         abuse::audio::voice_gain(abuse::audio::Bus::Music,
                                                  volume));
    }
#endif
}
