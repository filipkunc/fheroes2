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

#include <cstddef>
#include <cstdint>
#include <string>
#include <tuple>
#include <vector>

namespace Maps
{
    enum class ObjectGroup : uint8_t;
}

namespace Maps::Map_Format
{
    struct MapFormat;

    struct MP2MapValidationInfo
    {
        std::vector<int32_t> heroPositions;
        std::vector<int32_t> castlePositions;
        std::vector<int32_t> roadPositions;
        std::vector<int32_t> artifactPositions;
        std::vector<int32_t> selectionObjectPositions;
        int32_t ultimateArtifactPosition{ -1 };
        uint32_t ultimateArtifactObjectUid{ 0 };
        size_t dailyEventCount{ 0 };
        size_t rumorCount{ 0 };
        std::vector<std::tuple<int32_t, ObjectGroup, uint32_t>> placeholderObjects;
    };

    // Verify that rendering- and gameplay-critical imported objects remain on their original tiles, object roots have unique UIDs, every castle has one correctly
    // linked flag on each side, and supported map-level metadata was preserved.
    bool validateImportedMapObjectPlacement( const MapFormat & map, const MP2MapValidationInfo & expected, std::string * errorMessage = nullptr );
}
