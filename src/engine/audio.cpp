/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2008 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include "audio.h"

#include <atomic>
#include <cassert>
#include <cmath>
#include <cstddef>
#include <map>
#include <memory>
#include <mutex>
#include <ostream>
#include <string>
#include <utility>
#include <variant>
#include <vector>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL3/SDL_audio.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_iostream.h>
#include <SDL3/SDL_properties.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3_mixer/SDL_mixer.h>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#include "core.h"
#include "dir.h"
#include "logging.h"
#include "system.h"

namespace
{
    // SDL3 picks the device's preferred sample rate / format / channels and auto-resamples
    // whatever the app produces. We just hint stereo s16 at 44100 Hz; SDL3 negotiates from there.
    constexpr int audioFrequency = 44100;
    constexpr int audioChannels = 2;
    constexpr SDL_AudioFormat audioFormat = SDL_AUDIO_S16;

    std::atomic<bool> isInitialized{ false };

    // Single mixer bound to the default playback device.
    MIX_Mixer * gMixer{ nullptr };

    // Sound-effect channels: each slot owns one MIX_Track. A track is created lazily on first
    // Play() into that slot and then reused for subsequent sounds.
    std::vector<MIX_Track *> gChannelTracks;

    // Music: one dedicated track + a UID-keyed cache of MIX_Audio.
    MIX_Track * gMusicTrack{ nullptr };

    bool isMuted{ false };
    // SDL3 uses linear gain (float). 1.0 = unity, 0.0 = silent. We cap at 50/53 ≈ 0.943 to leave
    // ~0.5 dB of headroom for clipping, matching the SDL2 behaviour.
    float currentSfxGain{ 50.0f / 53.0f };
    float currentMusicGain{ 50.0f / 53.0f };
    float savedSfxGain{ 50.0f / 53.0f };
    float savedMusicGain{ 50.0f / 53.0f };

    int musicFadeInMs{ 0 };

    // Protects all global state above. Recursive because some public methods call each other
    // (e.g. Music::Play -> Music::Stop).
    std::recursive_mutex audioMutex;

    // By the Weber-Fechner law, humans subjective sound sensation is proportional logarithm of sound intensity.
    // So for linear changing sound intensity we have to change the volume exponential.
    // There is a good explanation at https://www.dr-lex.be/info-stuff/volumecontrols.html.
    // Maps 0..100 percent to a linear gain in [0, 50/53]. The ceiling matches the old SDL2 mapping
    // (MIX_MAX_VOLUME * 50 / 53), which reserved ~0.5 dB of headroom against clipping.
    float normalizeToMixerGain( const int volumePercentage )
    {
        if ( volumePercentage < 0 ) {
            // Why are you passing a negative volume value?
            assert( 0 );
            return 0.0f;
        }

        if ( volumePercentage >= 100 ) {
            return 50.0f / 53.0f;
        }

        return static_cast<float>( ( std::exp( std::log( 11.0 ) * volumePercentage / 100.0 ) - 1.0 ) / 10.6 );
    }

    void applySfxGainToAllTracks( const float gain )
    {
        for ( MIX_Track * track : gChannelTracks ) {
            if ( track != nullptr ) {
                MIX_SetTrackGain( track, gain );
            }
        }
    }

    // SDL3_mixer 3D coordinates: +x right, +y up, -z forward (listener facing -z).
    // The fheroes2 API takes (angle_deg, distance_0_255) where 0 deg = forward, 90 deg = right.
    MIX_Point3D anglePolarTo3D( const int16_t angleDeg, const uint8_t distance )
    {
        constexpr double pi = 3.14159265358979323846;
        const double angleRad = static_cast<double>( angleDeg ) * pi / 180.0;
        const float dist = static_cast<float>( distance ) / 255.0f;

        MIX_Point3D pos{};
        pos.x = static_cast<float>( std::sin( angleRad ) ) * dist;
        pos.y = 0.0f;
        pos.z = -static_cast<float>( std::cos( angleRad ) ) * dist;
        return pos;
    }

    class MusicInfo
    {
    public:
        explicit MusicInfo( std::vector<uint8_t> v )
            : _source( std::move( v ) )
        {
            // Do nothing
        }

        explicit MusicInfo( std::string file )
            : _source( std::move( file ) )
        {
            // Do nothing
        }

        MusicInfo( const MusicInfo & ) = delete;
        MusicInfo & operator=( const MusicInfo & ) = delete;

        ~MusicInfo()
        {
            if ( _audio != nullptr ) {
                MIX_DestroyAudio( _audio );
                _audio = nullptr;
            }
        }

        // Loads (and caches) the MIX_Audio for this music source. Returns nullptr on failure.
        // The MIX_Audio is bound to the mixer that loaded it; if the mixer changes, the cache
        // must be cleared first. Streaming (predecode=false) keeps memory low for long MIDI/OGG
        // tracks at the cost of read-on-demand seeks; for in-memory sources the underlying buffer
        // (held by this MusicInfo) outlives the IOStream handle, so reads remain valid.
        MIX_Audio * getOrLoad( MIX_Mixer * mixer )
        {
            if ( _audio != nullptr ) {
                return _audio;
            }
            if ( mixer == nullptr ) {
                return nullptr;
            }

            if ( const auto * vec = std::get_if<std::vector<uint8_t>>( &_source ); vec != nullptr ) {
                SDL_IOStream * io = SDL_IOFromConstMem( vec->data(), vec->size() );
                if ( io == nullptr ) {
                    ERROR_LOG( "Failed to create an IOStream for music data. The error: " << SDL_GetError() )
                    return nullptr;
                }

                _audio = MIX_LoadAudio_IO( mixer, io, false, true );
                if ( _audio == nullptr ) {
                    ERROR_LOG( "Failed to load the music track from memory. The error: " << SDL_GetError() )
                }
                return _audio;
            }

            if ( const auto * file = std::get_if<std::string>( &_source ); file != nullptr ) {
                _audio = MIX_LoadAudio( mixer, System::encLocalToUTF8( *file ).c_str(), false );
                if ( _audio == nullptr ) {
                    ERROR_LOG( "Failed to load the music track from file " << *file << ". The error: " << SDL_GetError() )
                }
                return _audio;
            }

            assert( 0 );
            return nullptr;
        }

        double getPosition() const
        {
            return _position;
        }

        void setPosition( const double pos )
        {
            _position = pos;
        }

    private:
        const std::variant<std::vector<uint8_t>, std::string> _source;
        MIX_Audio * _audio{ nullptr };
        double _position{ 0 };
    };

    class MusicTrackManager
    {
    public:
        MusicTrackManager() = default;
        MusicTrackManager( const MusicTrackManager & ) = delete;
        MusicTrackManager & operator=( const MusicTrackManager & ) = delete;
        ~MusicTrackManager() = default;

        bool isTrackInMusicDB( const uint64_t musicUID ) const
        {
            return _musicDB.find( musicUID ) != _musicDB.end();
        }

        std::shared_ptr<MusicInfo> getTrackFromMusicDB( const uint64_t musicUID ) const
        {
            const auto iter = _musicDB.find( musicUID );
            assert( iter != _musicDB.end() );
            return iter->second;
        }

        void addTrackToMusicDB( const uint64_t musicUID, std::shared_ptr<MusicInfo> track )
        {
            const auto res = _musicDB.try_emplace( musicUID, std::move( track ) );
            if ( !res.second ) {
                assert( 0 );
            }
        }

        void clearMusicDB()
        {
            // Audio handles are released by ~MusicInfo as the shared_ptrs drop.
            _musicDB.clear();
        }

        std::weak_ptr<MusicInfo> getCurrentTrack() const
        {
            return _currentTrack;
        }

        uint64_t getCurrentTrackUID() const
        {
            return _currentTrackUID;
        }

        Music::PlaybackMode getCurrentTrackPlaybackMode() const
        {
            return _currentTrackPlaybackMode;
        }

        void updateCurrentTrack( const uint64_t musicUID, const Music::PlaybackMode trackPlaybackMode )
        {
            _currentTrack = getTrackFromMusicDB( musicUID );
            _currentTrackUID = musicUID;
            _currentTrackPlaybackMode = trackPlaybackMode;
        }

        void resetCurrentTrack()
        {
            _currentTrack = {};
            _currentTrackUID = 0;
            _currentTrackPlaybackMode = Music::PlaybackMode::PLAY_ONCE;
        }

    private:
        std::map<uint64_t, std::shared_ptr<MusicInfo>> _musicDB;

        std::weak_ptr<MusicInfo> _currentTrack;

        uint64_t _currentTrackUID{ 0 };
        Music::PlaybackMode _currentTrackPlaybackMode{ Music::PlaybackMode::PLAY_ONCE };
    };

    MusicTrackManager musicTrackManager;

    // Caller must hold audioMutex. Begins playback of the music track previously registered as
    // musicUID. If the track is in RESUME_AND_PLAY_INFINITE mode and a saved position exists,
    // playback starts from that offset; otherwise it starts at 0.
    void playMusic( const uint64_t musicUID, const Music::PlaybackMode playbackMode )
    {
        // This function should never be called if a music track is currently playing.
        assert( !Music::isPlaying() );

        if ( gMusicTrack == nullptr || gMixer == nullptr ) {
            return;
        }

        const std::shared_ptr<MusicInfo> track = musicTrackManager.getTrackFromMusicDB( musicUID );
        assert( track );

        MIX_Audio * audio = track->getOrLoad( gMixer );
        if ( audio == nullptr ) {
            musicTrackManager.resetCurrentTrack();
            return;
        }

        if ( !MIX_SetTrackAudio( gMusicTrack, audio ) ) {
            ERROR_LOG( "Failed to assign music to the music track. The error: " << SDL_GetError() )
            musicTrackManager.resetCurrentTrack();
            return;
        }

        // Apply current music gain.
        MIX_SetTrackGain( gMusicTrack, isMuted ? 0.0f : currentMusicGain );

        // Build play options. SDL_mixer 3.x handles looping natively so we don't need a finished
        // hook to restart the track ourselves; -1 = infinite, 0 = play once.
        SDL_PropertiesID props = SDL_CreateProperties();
        if ( props == 0 ) {
            ERROR_LOG( "Failed to create properties for music playback. The error: " << SDL_GetError() )
            musicTrackManager.resetCurrentTrack();
            return;
        }

        const bool loopForever = ( playbackMode != Music::PlaybackMode::PLAY_ONCE );
        SDL_SetNumberProperty( props, MIX_PROP_PLAY_LOOPS_NUMBER, loopForever ? -1 : 0 );

        if ( musicFadeInMs > 0 ) {
            SDL_SetNumberProperty( props, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, musicFadeInMs );
        }

        // Resume from a saved position only for RESUME_AND_PLAY_INFINITE, mirroring the SDL2
        // behaviour. The 1-second guard skips trivial near-start positions.
        const double savedPosSec = track->getPosition();
        if ( playbackMode == Music::PlaybackMode::RESUME_AND_PLAY_INFINITE && savedPosSec > 1.0 ) {
            const Sint64 startMs = static_cast<Sint64>( savedPosSec * 1000.0 );
            SDL_SetNumberProperty( props, MIX_PROP_PLAY_START_MILLISECOND_NUMBER, startMs );
        }

        // Update the bookkeeping before starting playback so any future stop/query sees the
        // correct mode/UID. (No music-finished callback exists in this implementation, so
        // there is no thread that races against this update.)
        musicTrackManager.updateCurrentTrack( musicUID, playbackMode );

        const bool started = MIX_PlayTrack( gMusicTrack, props );
        SDL_DestroyProperties( props );

        if ( !started ) {
            ERROR_LOG( "Failed to play the music track. The error: " << SDL_GetError() )

            // If the resume seek failed, retry from the beginning.
            if ( playbackMode == Music::PlaybackMode::RESUME_AND_PLAY_INFINITE && savedPosSec > 1.0 ) {
                track->setPosition( 0 );

                SDL_PropertiesID retryProps = SDL_CreateProperties();
                if ( retryProps != 0 ) {
                    SDL_SetNumberProperty( retryProps, MIX_PROP_PLAY_LOOPS_NUMBER, -1 );
                    if ( musicFadeInMs > 0 ) {
                        SDL_SetNumberProperty( retryProps, MIX_PROP_PLAY_FADE_IN_MILLISECONDS_NUMBER, musicFadeInMs );
                    }
                    const bool retryOk = MIX_PlayTrack( gMusicTrack, retryProps );
                    SDL_DestroyProperties( retryProps );
                    if ( retryOk ) {
                        return;
                    }
                    ERROR_LOG( "Music retry from start also failed. The error: " << SDL_GetError() )
                }
            }

            musicTrackManager.resetCurrentTrack();
        }
    }
}

void Audio::Init()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( isInitialized ) {
        // If this assertion blows up you are trying to initialize an already initialized system.
        assert( 0 );
        return;
    }

    if ( !fheroes2::isComponentInitialized( fheroes2::SystemInitializationComponent::Audio ) ) {
        ERROR_LOG( "The audio subsystem was not initialized." )
        return;
    }

    if ( !MIX_Init() ) {
        ERROR_LOG( "Failed to initialize SDL_mixer. The error: " << SDL_GetError() )
        return;
    }

    SDL_AudioSpec spec{};
    spec.freq = audioFrequency;
    spec.format = audioFormat;
    spec.channels = audioChannels;

    // SDL3's SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK macro expands to a C-style cast,
    // which trips -Wold-style-cast in the strict warning configuration the Android
    // NDK build uses. Suppress locally; the macro itself is the right value.
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wold-style-cast"
#endif
    gMixer = MIX_CreateMixerDevice( SDL_AUDIO_DEVICE_DEFAULT_PLAYBACK, &spec );
#if defined( __GNUC__ ) || defined( __clang__ )
#pragma GCC diagnostic pop
#endif
    if ( gMixer == nullptr ) {
        ERROR_LOG( "Failed to open an audio device. The error: " << SDL_GetError() )
        MIX_Quit();
        return;
    }

    SDL_AudioSpec actual{};
    if ( MIX_GetMixerFormat( gMixer, &actual ) ) {
        if ( actual.freq != audioFrequency ) {
            // SDL3 will resample for us if the device prefers something else; this is informational.
            DEBUG_LOG( DBG_ENGINE, DBG_WARN, "Audio frequency negotiated as " << actual.freq << " Hz instead of " << audioFrequency )
        }
    }

    gMusicTrack = MIX_CreateTrack( gMixer );
    if ( gMusicTrack == nullptr ) {
        ERROR_LOG( "Failed to create the music track. The error: " << SDL_GetError() )
        MIX_DestroyMixer( gMixer );
        gMixer = nullptr;
        MIX_Quit();
        return;
    }

    isMuted = false;
    currentSfxGain = 50.0f / 53.0f;
    currentMusicGain = 50.0f / 53.0f;
    savedSfxGain = currentSfxGain;
    savedMusicGain = currentMusicGain;

    MIX_SetTrackGain( gMusicTrack, currentMusicGain );

    isInitialized = true;
}

void Audio::Quit()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    if ( !fheroes2::isComponentInitialized( fheroes2::SystemInitializationComponent::Audio ) ) {
        // Something wrong with the logic! The component must be initialized.
        assert( 0 );
        return;
    }

    Music::Stop();
    Mixer::Stop();

    if ( gMusicTrack != nullptr ) {
        MIX_DestroyTrack( gMusicTrack );
        gMusicTrack = nullptr;
    }

    for ( MIX_Track * track : gChannelTracks ) {
        if ( track != nullptr ) {
            MIX_DestroyTrack( track );
        }
    }
    gChannelTracks.clear();

    musicTrackManager.clearMusicDB();
    musicTrackManager.resetCurrentTrack();

    if ( gMixer != nullptr ) {
        MIX_DestroyMixer( gMixer );
        gMixer = nullptr;
    }
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

    savedSfxGain = currentSfxGain;
    savedMusicGain = currentMusicGain;

    applySfxGainToAllTracks( 0.0f );
    if ( gMusicTrack != nullptr ) {
        MIX_SetTrackGain( gMusicTrack, 0.0f );
    }
}

void Audio::Unmute()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isMuted || !isInitialized ) {
        return;
    }

    isMuted = false;

    currentSfxGain = savedSfxGain;
    currentMusicGain = savedMusicGain;

    applySfxGainToAllTracks( currentSfxGain );
    if ( gMusicTrack != nullptr ) {
        MIX_SetTrackGain( gMusicTrack, currentMusicGain );
    }
}

bool Audio::isValid()
{
    return isInitialized;
}

void Mixer::SetChannels( const int num )
{
    if ( num <= 0 ) {
        // Zero channels would leave us with nothing to play onto.
        assert( 0 );
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    const size_t newSize = static_cast<size_t>( num );

    // Shrink: stop and free tracks beyond the new size.
    for ( size_t i = newSize; i < gChannelTracks.size(); ++i ) {
        if ( gChannelTracks[i] != nullptr ) {
            MIX_DestroyTrack( gChannelTracks[i] );
            gChannelTracks[i] = nullptr;
        }
    }
    gChannelTracks.resize( newSize, nullptr );

    // New tracks are created lazily on first Play() into the slot. Apply the current gain to any
    // tracks that were already created before the resize.
    applySfxGainToAllTracks( isMuted ? 0.0f : currentSfxGain );
}

int Mixer::getChannelCount()
{
    return static_cast<int>( gChannelTracks.size() );
}

int Mixer::Play( const uint8_t * ptr, const uint32_t size, const bool loop, const std::optional<std::pair<int16_t, uint8_t>> position /* = {} */ )
{
    if ( ptr == nullptr || size == 0 ) {
        // You are trying to play an empty sound. Check your logic!
        assert( 0 );
        return -1;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized || gMixer == nullptr ) {
        return -1;
    }

    // Find a free slot: one whose track is null or has stopped playing.
    int slot = -1;
    for ( size_t i = 0; i < gChannelTracks.size(); ++i ) {
        MIX_Track * existing = gChannelTracks[i];
        if ( existing == nullptr || !MIX_TrackPlaying( existing ) ) {
            slot = static_cast<int>( i );
            break;
        }
    }
    if ( slot < 0 ) {
        // All channels busy. Drop this sound, matching the SDL2 fallback behaviour.
        return -1;
    }

    SDL_IOStream * io = SDL_IOFromConstMem( ptr, static_cast<size_t>( size ) );
    if ( io == nullptr ) {
        ERROR_LOG( "Failed to create an IOStream for an audio chunk. The error: " << SDL_GetError() )
        return -1;
    }

    // predecode=true: load the entire WAV/OGG/etc into memory now. closeio=true: SDL closes the
    // stream once decoding finishes. After this call the source bytes pointed to by `ptr` are no
    // longer needed by SDL_mixer.
    MIX_Audio * audio = MIX_LoadAudio_IO( gMixer, io, true, true );
    if ( audio == nullptr ) {
        ERROR_LOG( "Failed to load an audio chunk. The error: " << SDL_GetError() )
        return -1;
    }

    MIX_Track * track = gChannelTracks[slot];
    if ( track == nullptr ) {
        track = MIX_CreateTrack( gMixer );
        if ( track == nullptr ) {
            ERROR_LOG( "Failed to create an audio track. The error: " << SDL_GetError() )
            MIX_DestroyAudio( audio );
            return -1;
        }
        gChannelTracks[slot] = track;
    }

    if ( !MIX_SetTrackAudio( track, audio ) ) {
        ERROR_LOG( "Failed to assign audio to track " << slot << ". The error: " << SDL_GetError() )
        MIX_DestroyAudio( audio );
        return -1;
    }
    // The track now holds a reference to the audio. Drop ours so SDL frees the audio once the
    // track is done with it (either when we replace its input or when the track is destroyed).
    MIX_DestroyAudio( audio );

    MIX_SetTrackGain( track, isMuted ? 0.0f : currentSfxGain );

    if ( position ) {
        const MIX_Point3D pos = anglePolarTo3D( position->first, position->second );
        MIX_SetTrack3DPosition( track, &pos );
    }
    else {
        // Reset 3D position so a previous spatialised sound on this slot doesn't leak its position.
        const MIX_Point3D pos{ 0.0f, 0.0f, 0.0f };
        MIX_SetTrack3DPosition( track, &pos );
    }

    SDL_PropertiesID props = SDL_CreateProperties();
    if ( props == 0 ) {
        ERROR_LOG( "Failed to create properties for sound playback. The error: " << SDL_GetError() )
        return -1;
    }
    SDL_SetNumberProperty( props, MIX_PROP_PLAY_LOOPS_NUMBER, loop ? -1 : 0 );

    const bool started = MIX_PlayTrack( track, props );
    SDL_DestroyProperties( props );

    if ( !started ) {
        ERROR_LOG( "Failed to play audio on channel " << slot << ". The error: " << SDL_GetError() )
        return -1;
    }

    return slot;
}

void Mixer::setPosition( const int channelId, const int16_t angle, const uint8_t distance )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    if ( channelId < 0 || static_cast<size_t>( channelId ) >= gChannelTracks.size() ) {
        return;
    }

    MIX_Track * track = gChannelTracks[channelId];
    if ( track == nullptr ) {
        return;
    }

    const MIX_Point3D pos = anglePolarTo3D( angle, distance );
    if ( !MIX_SetTrack3DPosition( track, &pos ) ) {
        ERROR_LOG( "Failed to set the position of channel " << channelId << ". The error: " << SDL_GetError() )
    }
}

void Mixer::setVolume( const int volumePercentage )
{
    const float gain = normalizeToMixerGain( volumePercentage );

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    if ( isMuted ) {
        // Save for restoration, but keep the audible level at 0 until unmuted.
        savedSfxGain = gain;
        return;
    }

    currentSfxGain = gain;
    applySfxGainToAllTracks( gain );
}

void Mixer::Stop( const int channelId /* = -1 */ )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    if ( channelId < 0 ) {
        // Stop all SFX channels.
        for ( MIX_Track * track : gChannelTracks ) {
            if ( track != nullptr && MIX_TrackPlaying( track ) ) {
                MIX_StopTrack( track, 0 );
            }
        }
        return;
    }

    if ( static_cast<size_t>( channelId ) >= gChannelTracks.size() ) {
        return;
    }

    MIX_Track * track = gChannelTracks[channelId];
    if ( track != nullptr && MIX_TrackPlaying( track ) ) {
        MIX_StopTrack( track, 0 );
    }
}

bool Mixer::isPlaying( const int channelId )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return false;
    }

    if ( channelId < 0 ) {
        // Old API: -1 means "any channel playing". Iterate.
        for ( MIX_Track * track : gChannelTracks ) {
            if ( track != nullptr && MIX_TrackPlaying( track ) ) {
                return true;
            }
        }
        return false;
    }

    if ( static_cast<size_t>( channelId ) >= gChannelTracks.size() ) {
        return false;
    }

    MIX_Track * track = gChannelTracks[channelId];
    return track != nullptr && MIX_TrackPlaying( track );
}

bool Music::Play( const uint64_t musicUID, const PlaybackMode playbackMode )
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return false;
    }

    if ( !musicTrackManager.isTrackInMusicDB( musicUID ) ) {
        return false;
    }

    Stop();
    playMusic( musicUID, playbackMode );
    return true;
}

void Music::Play( const uint64_t musicUID, const std::vector<uint8_t> & v, const PlaybackMode playbackMode )
{
    if ( v.empty() ) {
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    musicTrackManager.addTrackToMusicDB( musicUID, std::make_shared<MusicInfo>( v ) );

    Stop();
    playMusic( musicUID, playbackMode );
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

    musicTrackManager.addTrackToMusicDB( musicUID, std::make_shared<MusicInfo>( file ) );

    Stop();
    playMusic( musicUID, playbackMode );
}

void Music::SetFadeInMs( const int timeMs )
{
    if ( timeMs < 0 ) {
        // Why are you even setting a negative value?
        assert( 0 );
        return;
    }

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    musicFadeInMs = timeMs;
}

void Music::setVolume( const int volumePercentage )
{
    const float gain = normalizeToMixerGain( volumePercentage );

    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized ) {
        return;
    }

    if ( isMuted ) {
        savedMusicGain = gain;
        return;
    }

    currentMusicGain = gain;
    if ( gMusicTrack != nullptr ) {
        MIX_SetTrackGain( gMusicTrack, gain );
    }
}

void Music::Stop()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    if ( !isInitialized || gMusicTrack == nullptr ) {
        return;
    }

    const std::shared_ptr<MusicInfo> currentTrack = musicTrackManager.getCurrentTrack().lock();
    if ( !currentTrack ) {
        // Nothing to do.
        return;
    }

    // For RESUME_AND_PLAY_INFINITE, capture the current source-data position before halting so we
    // can resume there next time. SDL_mixer reports playback position as the offset within the
    // source data (i.e. modulo the loop), which is exactly what we want.
    if ( musicTrackManager.getCurrentTrackPlaybackMode() == PlaybackMode::RESUME_AND_PLAY_INFINITE ) {
        const Sint64 frames = MIX_GetTrackPlaybackPosition( gMusicTrack );
        if ( frames > 0 ) {
            const Sint64 ms = MIX_TrackFramesToMS( gMusicTrack, frames );
            if ( ms > 0 ) {
                currentTrack->setPosition( static_cast<double>( ms ) / 1000.0 );
            }
        }
    }
    else {
        currentTrack->setPosition( 0 );
    }

    MIX_StopTrack( gMusicTrack, 0 );

    musicTrackManager.resetCurrentTrack();
}

bool Music::isPlaying()
{
    const std::scoped_lock<std::recursive_mutex> lock( audioMutex );

    return isInitialized && gMusicTrack != nullptr && MIX_TrackPlaying( gMusicTrack );
}

void Music::setMidiSoundFonts( const ListFiles & /* files */ )
{
    // SDL_mixer 3.x configures MIDI rendering via mixer/audio properties rather than the old
    // global Mix_SetSoundFonts() helper. Windows uses the OS MIDI synth out of the box, so this
    // is a no-op until cross-platform MIDI requires explicit SoundFont wiring.
}

void Music::setMidiTimidityCfg( const std::string & /* path */ )
{
    // See setMidiSoundFonts(): SDL_mixer 3.x doesn't expose Mix_SetTimidityCfg().
}
