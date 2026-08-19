/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#if defined( WITH_SDL3 )

#include <algorithm>
#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <map>
#include <mutex>
#include <ostream>
#include <string>
#include <utility>
#include <variant>

#include "audio.h"

// Managing compiler warnings for SDL headers.
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL3/SDL.h>
#include <SDL3_mixer/SDL_mixer.h>

// Managing compiler warnings for SDL headers.
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#include "core.h"
#include "logging.h"
#include "system.h"

namespace
{
    constexpr int defaultChannelCount = 8;

    std::recursive_mutex audioMutex;

    MIX_Mixer * mixer = nullptr;
    MIX_Track * musicTrack = nullptr;
    std::vector<MIX_Track *> mixerTracks;

    std::atomic<bool> isInitialized{ false };
    bool isMuted = false;
    float mixerGain = 1.0F;
    float musicGain = 1.0F;
    int musicFadeInMs = 0;

    struct MusicInfo
    {
        explicit MusicInfo( std::vector<uint8_t> data )
            : source( std::move( data ) )
        {}

        explicit MusicInfo( std::string file )
            : source( std::move( file ) )
        {}

        std::variant<std::vector<uint8_t>, std::string> source;
        Sint64 positionMs = 0;
    };

    std::map<uint64_t, MusicInfo> musicDatabase;
    std::map<uint64_t, MusicInfo>::iterator currentMusic = musicDatabase.end();
    Music::PlaybackMode currentPlaybackMode = Music::PlaybackMode::PLAY_ONCE;

    float normalizeToSDLGain( const int volumePercentage )
    {
        if ( volumePercentage < 0 ) {
            assert( 0 );
            return 0.0F;
        }

        if ( volumePercentage >= 100 ) {
            return 50.0F / 53.0F;
        }

        return static_cast<float>( ( std::exp( std::log( 11.0 ) * volumePercentage / 100.0 ) - 1.0 ) / 10.6 );
    }

    void applyMixerGain()
    {
        const float gain = isMuted ? 0.0F : mixerGain;
        for ( MIX_Track * track : mixerTracks ) {
            if ( !MIX_SetTrackGain( track, gain ) ) {
                ERROR_LOG( "Failed to set an SDL3_mixer channel gain. The error: " << SDL_GetError() )
            }
        }
    }

    void applyMusicGain()
    {
        if ( musicTrack != nullptr && !MIX_SetTrackGain( musicTrack, isMuted ? 0.0F : musicGain ) ) {
            ERROR_LOG( "Failed to set the SDL3_mixer music gain. The error: " << SDL_GetError() )
        }
    }

    void destroyTracks()
    {
        for ( MIX_Track * track : mixerTracks ) {
            MIX_DestroyTrack( track );
        }
        mixerTracks.clear();

        if ( musicTrack != nullptr ) {
            MIX_DestroyTrack( musicTrack );
            musicTrack = nullptr;
        }
    }

    void setChannelCount( const int count )
    {
        while ( static_cast<int>( mixerTracks.size() ) > count ) {
            MIX_DestroyTrack( mixerTracks.back() );
            mixerTracks.pop_back();
        }

        while ( static_cast<int>( mixerTracks.size() ) < count ) {
            MIX_Track * track = MIX_CreateTrack( mixer );
            if ( track == nullptr ) {
                ERROR_LOG( "Failed to create an SDL3_mixer channel. The error: " << SDL_GetError() )
                break;
            }

            mixerTracks.push_back( track );
        }

        applyMixerGain();
    }

    MIX_Audio * loadMusic( const MusicInfo & info )
    {
        if ( std::holds_alternative<std::vector<uint8_t>>( info.source ) ) {
            const std::vector<uint8_t> & data = std::get<std::vector<uint8_t>>( info.source );
            SDL_IOStream * stream = SDL_IOFromConstMem( data.data(), data.size() );
            if ( stream == nullptr ) {
                ERROR_LOG( "Failed to create a music stream from memory. The error: " << SDL_GetError() )
                return nullptr;
            }

            return MIX_LoadAudio_IO( mixer, stream, false, true );
        }

        const std::string & file = std::get<std::string>( info.source );
        return MIX_LoadAudio( mixer, System::encLocalToUTF8( file ).c_str(), false );
    }

    void rememberMusicPosition()
    {
        if ( currentMusic == musicDatabase.end() ) {
            return;
        }

        if ( currentPlaybackMode != Music::PlaybackMode::RESUME_AND_PLAY_INFINITE ) {
            currentMusic->second.positionMs = 0;
            return;
        }

        const Sint64 positionFrames = MIX_GetTrackPlaybackPosition( musicTrack );
        SDL_AudioSpec spec{};
        if ( positionFrames >= 0 && MIX_GetMixerFormat( mixer, &spec ) ) {
            currentMusic->second.positionMs = MIX_FramesToMS( spec.freq, positionFrames );
        }
    }

    void playMusic( std::map<uint64_t, MusicInfo>::iterator music, const Music::PlaybackMode playbackMode )
    {
        MIX_Audio * audio = loadMusic( music->second );
        if ( audio == nullptr ) {
            ERROR_LOG( "Failed to load an SDL3_mixer music track. The error: " << SDL_GetError() )
            return;
        }

        const bool inputWasSet = MIX_SetTrackAudio( musicTrack, audio );
        MIX_DestroyAudio( audio );
        if ( !inputWasSet ) {
            ERROR_LOG( "Failed to assign an SDL3_mixer music track. The error: " << SDL_GetError() )
            return;
        }

        SDL_PropertiesID options = SDL_CreateProperties();
        if ( options == 0 ) {
            ERROR_LOG( "Failed to create SDL3_mixer playback options. The error: " << SDL_GetError() )
            return;
        }

        const bool shouldResume = playbackMode == Music::PlaybackMode::RESUME_AND_PLAY_INFINITE && music->second.positionMs > 1000;
        const Sint64 startPosition = shouldResume ? music->second.positionMs : 0;
        if ( !shouldResume ) {
            music->second.positionMs = 0;
        }

        bool optionsAreValid = SDL_SetNumberProperty( options, MIX_PROP_PLAY_LOOPS_NUMBER, playbackMode == Music::PlaybackMode::PLAY_ONCE ? 0 : -1 );
        optionsAreValid = SDL_SetNumberProperty( options, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, startPosition ) && optionsAreValid;
        optionsAreValid = SDL_SetNumberProperty( options, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, musicFadeInMs ) && optionsAreValid;

        applyMusicGain();
        const bool started = optionsAreValid && MIX_PlayTrack( musicTrack, options );
        SDL_DestroyProperties( options );

        if ( !started ) {
            ERROR_LOG( "Failed to play an SDL3_mixer music track. The error: " << SDL_GetError() )
            return;
        }

        currentMusic = music;
        currentPlaybackMode = playbackMode;
    }
}

void Audio::Init()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( isInitialized ) {
        assert( 0 );
        return;
    }

    if ( !System::isComponentInitialized( System::SystemInitializationComponent::Audio ) ) {
        ERROR_LOG( "The audio subsystem was not initialized." )
        return;
    }

    if ( !MIX_Init() ) {
        ERROR_LOG( "Failed to initialize SDL3_mixer. The error: " << SDL_GetError() )
        return;
    }

    SDL_AudioSpec requestedSpec{};
    requestedSpec.freq = 44100;
    requestedSpec.format = SDL_AUDIO_S16;
    requestedSpec.channels = 2;

#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    mixer = MIX_CreateMixerDevice( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &requestedSpec );
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
    if ( mixer == nullptr ) {
        ERROR_LOG( "Failed to initialize an SDL3_mixer audio device. The error: " << SDL_GetError() )
        MIX_Quit();
        return;
    }

    SDL_AudioSpec actualSpec{};
    if ( !MIX_GetMixerFormat( mixer, &actualSpec ) ) {
        ERROR_LOG( "Failed to query the SDL3_mixer audio device. The error: " << SDL_GetError() )
    }

    musicTrack = MIX_CreateTrack( mixer );
    if ( musicTrack == nullptr ) {
        ERROR_LOG( "Failed to create the SDL3_mixer music track. The error: " << SDL_GetError() )
        MIX_DestroyMixer( mixer );
        mixer = nullptr;
        MIX_Quit();
        return;
    }

    isMuted = false;
    setChannelCount( defaultChannelCount );
    if ( mixerTracks.empty() ) {
        destroyTracks();
        MIX_DestroyMixer( mixer );
        mixer = nullptr;
        MIX_Quit();
        return;
    }

    applyMusicGain();
    isInitialized = true;
}

void Audio::Quit()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    Music::Stop();
    Mixer::Stop();

    musicDatabase.clear();
    currentMusic = musicDatabase.end();
    destroyTracks();

    MIX_DestroyMixer( mixer );
    mixer = nullptr;
    MIX_Quit();

    isInitialized = false;
}

void Audio::Mute()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( isMuted || !isInitialized ) {
        return;
    }

    isMuted = true;
    applyMixerGain();
    applyMusicGain();
}

void Audio::Unmute()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isMuted || !isInitialized ) {
        return;
    }

    isMuted = false;
    applyMixerGain();
    applyMusicGain();
}

bool Audio::isValid()
{
    return isInitialized;
}

void Mixer::SetChannels( const int num )
{
    if ( num <= 0 ) {
        assert( 0 );
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( isInitialized ) {
        setChannelCount( num );
    }
}

int Mixer::getChannelCount()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    return static_cast<int>( mixerTracks.size() );
}

int Mixer::Play( const uint8_t * ptr, const uint32_t size, const bool loop, const std::optional<std::pair<int16_t, uint8_t>> position /* = {} */ )
{
    if ( ptr == nullptr || size == 0 ) {
        assert( 0 );
        return -1;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return -1;
    }

    const auto availableTrack
        = std::find_if( mixerTracks.begin(), mixerTracks.end(), []( MIX_Track * track ) { return !MIX_TrackPlaying( track ) && !MIX_TrackPaused( track ); } );
    if ( availableTrack == mixerTracks.end() ) {
        return -1;
    }

    SDL_IOStream * stream = SDL_IOFromConstMem( ptr, size );
    if ( stream == nullptr ) {
        ERROR_LOG( "Failed to create an SDL3_mixer sound stream. The error: " << SDL_GetError() )
        return -1;
    }

    MIX_Audio * audio = MIX_LoadAudio_IO( mixer, stream, true, true );
    if ( audio == nullptr ) {
        ERROR_LOG( "Failed to load an SDL3_mixer sound. The error: " << SDL_GetError() )
        return -1;
    }

    MIX_Track * track = *availableTrack;
    const bool inputWasSet = MIX_SetTrackAudio( track, audio );
    MIX_DestroyAudio( audio );
    if ( !inputWasSet ) {
        ERROR_LOG( "Failed to assign an SDL3_mixer sound. The error: " << SDL_GetError() )
        return -1;
    }

    if ( position ) {
        const float radians = static_cast<float>( position->first ) * ( SDL_PI_F / 180.0F );
        const float distance = static_cast<float>( position->second );
        const MIX_Point3D point{ SDL_cosf( radians ) * distance, 0.0F, SDL_sinf( radians ) * distance };
        if ( !MIX_SetTrack3DPosition( track, &point ) ) {
            ERROR_LOG( "Failed to position an SDL3_mixer sound. The error: " << SDL_GetError() )
        }
    }
    else if ( !MIX_SetTrack3DPosition( track, nullptr ) ) {
        ERROR_LOG( "Failed to reset an SDL3_mixer sound position. The error: " << SDL_GetError() )
    }

    SDL_PropertiesID options = SDL_CreateProperties();
    if ( options == 0 ) {
        ERROR_LOG( "Failed to create SDL3_mixer playback options. The error: " << SDL_GetError() )
        return -1;
    }

    const bool optionsAreValid = SDL_SetNumberProperty( options, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0 );
    const bool started = optionsAreValid && MIX_PlayTrack( track, options );
    SDL_DestroyProperties( options );
    if ( !started ) {
        ERROR_LOG( "Failed to play an SDL3_mixer sound. The error: " << SDL_GetError() )
        return -1;
    }

    return static_cast<int>( std::distance( mixerTracks.begin(), availableTrack ) );
}

void Mixer::setVolume( const int volumePercentage )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    mixerGain = normalizeToSDLGain( volumePercentage );
    if ( isInitialized && !isMuted ) {
        applyMixerGain();
    }
}

void Mixer::setPosition( const int channelId, const int16_t angle, const uint8_t distance )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized || channelId < 0 || channelId >= static_cast<int>( mixerTracks.size() ) ) {
        return;
    }

    const float radians = static_cast<float>( angle ) * ( SDL_PI_F / 180.0F );
    const float floatDistance = static_cast<float>( distance );
    const MIX_Point3D point{ SDL_cosf( radians ) * floatDistance, 0.0F, SDL_sinf( radians ) * floatDistance };
    if ( !MIX_SetTrack3DPosition( mixerTracks[channelId], &point ) ) {
        ERROR_LOG( "Failed to position SDL3_mixer channel " << channelId << ". The error: " << SDL_GetError() )
    }
}

void Mixer::Stop( const int channelId /* = -1 */ )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return;
    }

    if ( channelId < 0 ) {
        for ( MIX_Track * track : mixerTracks ) {
            if ( !MIX_StopTrack( track, 0 ) ) {
                ERROR_LOG( "Failed to stop an SDL3_mixer channel. The error: " << SDL_GetError() )
            }
        }
    }
    else if ( channelId < static_cast<int>( mixerTracks.size() ) && !MIX_StopTrack( mixerTracks[channelId], 0 ) ) {
        ERROR_LOG( "Failed to stop SDL3_mixer channel " << channelId << ". The error: " << SDL_GetError() )
    }
}

bool Mixer::isPlaying( const int channelId )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return false;
    }

    if ( channelId < 0 ) {
        return std::any_of( mixerTracks.begin(), mixerTracks.end(), []( MIX_Track * track ) { return MIX_TrackPlaying( track ); } );
    }

    return channelId < static_cast<int>( mixerTracks.size() ) && MIX_TrackPlaying( mixerTracks[channelId] );
}

bool Music::Play( const uint64_t musicUID, const PlaybackMode playbackMode )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return false;
    }

    const auto music = musicDatabase.find( musicUID );
    if ( music == musicDatabase.end() ) {
        return false;
    }

    Stop();
    playMusic( music, playbackMode );
    return true;
}

void Music::Play( const uint64_t musicUID, const std::vector<uint8_t> & data, const PlaybackMode playbackMode )
{
    if ( data.empty() ) {
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return;
    }

    const auto result = musicDatabase.try_emplace( musicUID, data );
    if ( !result.second ) {
        assert( 0 );
        return;
    }

    Stop();
    playMusic( result.first, playbackMode );
}

void Music::Play( const uint64_t musicUID, const std::string & file, const PlaybackMode playbackMode )
{
    if ( file.empty() ) {
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized ) {
        return;
    }

    const auto result = musicDatabase.try_emplace( musicUID, file );
    if ( !result.second ) {
        assert( 0 );
        return;
    }

    Stop();
    playMusic( result.first, playbackMode );
}

void Music::setVolume( const int volumePercentage )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    musicGain = normalizeToSDLGain( volumePercentage );
    if ( isInitialized && !isMuted ) {
        applyMusicGain();
    }
}

void Music::SetFadeInMs( const int timeMs )
{
    if ( timeMs < 0 ) {
        assert( 0 );
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    musicFadeInMs = timeMs;
}

void Music::Stop()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !isInitialized || currentMusic == musicDatabase.end() ) {
        return;
    }

    rememberMusicPosition();
    if ( !MIX_StopTrack( musicTrack, 0 ) ) {
        ERROR_LOG( "Failed to stop the SDL3_mixer music track. The error: " << SDL_GetError() )
    }
    currentMusic = musicDatabase.end();
}

bool Music::isPlaying()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    return isInitialized && MIX_TrackPlaying( musicTrack );
}

void Music::setMidiSoundFonts( const ListFiles & )
{
    // The bundled SDL3_mixer configuration uses the Timidity backend, which consumes timidity.cfg instead of a SoundFont list.
}

void Music::setMidiTimidityCfg( const std::string & path )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );
    if ( !SDL_SetEnvironmentVariable( SDL_GetEnvironment(), "TIMIDITY_CFG", System::encLocalToUTF8( path ).c_str(), true ) ) {
        ERROR_LOG( "Failed to set the path to the timidity.cfg file to " << path << ". The error: " << SDL_GetError() )
    }
}

#endif
