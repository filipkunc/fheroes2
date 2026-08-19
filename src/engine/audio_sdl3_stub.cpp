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

#include "audio.h"

void Audio::Init() {}
void Audio::Quit() {}
void Audio::Mute() {}
void Audio::Unmute() {}
bool Audio::isValid()
{
    return false;
}

void Mixer::SetChannels( const int ) {}
int Mixer::getChannelCount()
{
    return 0;
}
int Mixer::Play( const uint8_t *, const uint32_t, const bool, const std::optional<std::pair<int16_t, uint8_t>> )
{
    return -1;
}
void Mixer::setVolume( const int ) {}
void Mixer::setPosition( const int, const int16_t, const uint8_t ) {}
void Mixer::Stop( const int ) {}
bool Mixer::isPlaying( const int )
{
    return false;
}

bool Music::Play( const uint64_t, const PlaybackMode )
{
    return false;
}
void Music::Play( const uint64_t, const std::vector<uint8_t> &, const PlaybackMode ) {}
void Music::Play( const uint64_t, const std::string &, const PlaybackMode ) {}
void Music::setVolume( const int ) {}
void Music::SetFadeInMs( const int ) {}
void Music::Stop() {}
bool Music::isPlaying()
{
    return false;
}
void Music::setMidiSoundFonts( const ListFiles & ) {}
void Music::setMidiTimidityCfg( const std::string & ) {}

#endif
