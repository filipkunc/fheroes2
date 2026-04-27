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

// Audio is stubbed out for the SDL3 migration (Phase 1). The game runs silently.
// SDL3_mixer integration lands in Session B.

#include "audio.h"

void Audio::Init()
{
    // No-op stub.
}

void Audio::Quit()
{
    // No-op stub.
}

void Audio::Mute()
{
    // No-op stub.
}

void Audio::Unmute()
{
    // No-op stub.
}

bool Audio::isValid()
{
    return false;
}

void Mixer::SetChannels( const int /* num */ )
{
    // No-op stub.
}

int Mixer::getChannelCount()
{
    return 0;
}

int Mixer::Play( const uint8_t * /* ptr */, const uint32_t /* size */, const bool /* loop */, const std::optional<std::pair<int16_t, uint8_t>> /* position */ )
{
    return -1;
}

void Mixer::setPosition( const int /* channelId */, const int16_t /* angle */, const uint8_t /* distance */ )
{
    // No-op stub.
}

void Mixer::setVolume( const int /* volumePercentage */ )
{
    // No-op stub.
}

void Mixer::Stop( const int /* channelId */ )
{
    // No-op stub.
}

bool Mixer::isPlaying( const int /* channelId */ )
{
    return false;
}

bool Music::Play( const uint64_t /* musicUID */, const PlaybackMode /* playbackMode */ )
{
    return false;
}

void Music::Play( const uint64_t /* musicUID */, const std::vector<uint8_t> & /* v */, const PlaybackMode /* playbackMode */ )
{
    // No-op stub.
}

void Music::Play( const uint64_t /* musicUID */, const std::string & /* file */, const PlaybackMode /* playbackMode */ )
{
    // No-op stub.
}

void Music::SetFadeInMs( const int /* timeMs */ )
{
    // No-op stub.
}

void Music::setVolume( const int /* volumePercentage */ )
{
    // No-op stub.
}

void Music::Stop()
{
    // No-op stub.
}

bool Music::isPlaying()
{
    return false;
}

void Music::setMidiSoundFonts( const ListFiles & /* files */ )
{
    // No-op stub.
}

void Music::setMidiTimidityCfg( const std::string & /* path */ )
{
    // No-op stub.
}
