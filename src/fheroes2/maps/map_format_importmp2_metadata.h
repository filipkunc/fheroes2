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

#pragma once

#include <cstdint>
#include <vector>

namespace Maps::Map_Format
{
    struct AdventureMapEventMetadata;
    struct CastleMetadata;
    struct HeroMetadata;
    struct SignMetadata;
    struct SphinxMetadata;

    // Convert original MP2 object blocks without passing them through runtime randomization or default-value expansion.
    bool readMP2CastleMetadata( const std::vector<uint8_t> & data, CastleMetadata & metadata );
    bool readMP2HeroMetadata( const std::vector<uint8_t> & data, uint8_t race, HeroMetadata & metadata );
    bool readMP2SignMetadata( const std::vector<uint8_t> & data, SignMetadata & metadata );
    bool readMP2AdventureMapEventMetadata( const std::vector<uint8_t> & data, AdventureMapEventMetadata & metadata );
    bool readMP2SphinxMetadata( const std::vector<uint8_t> & data, SphinxMetadata & metadata );
}
