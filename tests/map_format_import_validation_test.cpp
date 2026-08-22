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

#include <cstdint>
#include <iostream>
#include <string>
#include <vector>

#include "map_format_importmp2_validation.h"
#include "map_format_info.h"

namespace
{
    constexpr int32_t mapWidth = 5;
    constexpr int32_t castlePosition = 12;
    constexpr int32_t heroPosition = castlePosition;
    constexpr uint32_t castleUid = 100;
    constexpr uint32_t heroUid = 200;

    Maps::Map_Format::MapFormat makeValidMap()
    {
        Maps::Map_Format::MapFormat map;
        map.width = mapWidth;
        map.tiles.resize( mapWidth * mapWidth );

        map.tiles[castlePosition].objects.push_back( { castleUid, Maps::ObjectGroup::KINGDOM_TOWNS, 10 } );
        map.tiles[heroPosition].objects.push_back( { heroUid, Maps::ObjectGroup::KINGDOM_HEROES, 35 } );
        map.tiles[castlePosition - 1].objects.push_back( { castleUid, Maps::ObjectGroup::LANDSCAPE_FLAGS, 10 } );
        map.tiles[castlePosition + 1].objects.push_back( { castleUid, Maps::ObjectGroup::LANDSCAPE_FLAGS, 11 } );

        return map;
    }

    bool expectValidation( const Maps::Map_Format::MapFormat & map, const Maps::Map_Format::MP2MapValidationInfo & validationInfo, const bool expected,
                           const std::string & context )
    {
        const bool result = Maps::Map_Format::validateImportedMapObjectPlacement( map, validationInfo );
        if ( result == expected ) {
            return true;
        }

        std::cerr << context << ": expected " << expected << ", got " << result << ".\n";
        return false;
    }
}

int main()
{
    Maps::Map_Format::MP2MapValidationInfo validationInfo;
    validationInfo.heroPositions = { heroPosition };
    validationInfo.castlePositions = { castlePosition };

    if ( !expectValidation( makeValidMap(), validationInfo, true, "valid imported objects" ) ) {
        return 1;
    }

    {
        auto map = makeValidMap();
        map.tiles[castlePosition - 1].objects.front().id = heroUid;
        map.tiles[castlePosition + 1].objects.front().id = heroUid;
        if ( !expectValidation( map, validationInfo, false, "orphan hero flags" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[castlePosition + 1].objects.clear();
        if ( !expectValidation( map, validationInfo, false, "missing castle flag" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[castlePosition + 1].objects.front().index = 9;
        if ( !expectValidation( map, validationInfo, false, "mismatched castle flag color" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[castlePosition].objects.front().index = 13;
        map.tiles[castlePosition - 1].objects.front().index = 12;
        map.tiles[castlePosition + 1].objects.front().index = 13;

        auto randomTownValidationInfo = validationInfo;
        randomTownValidationInfo.placeholderObjects.emplace_back( castlePosition, Maps::ObjectGroup::KINGDOM_TOWNS, 13 );
        if ( !expectValidation( map, randomTownValidationInfo, true, "preserved random town" ) ) {
            return 1;
        }

        map.tiles[castlePosition].objects.front().index = 12;
        if ( !expectValidation( map, randomTownValidationInfo, false, "evaluated random town" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[1].objects.push_back( { 300, Maps::ObjectGroup::MONSTERS, 68 } );

        auto randomMonsterValidationInfo = validationInfo;
        randomMonsterValidationInfo.placeholderObjects.emplace_back( 1, Maps::ObjectGroup::MONSTERS, 68 );
        if ( !expectValidation( map, randomMonsterValidationInfo, true, "preserved declared random monster tier" ) ) {
            return 1;
        }

        map.tiles[1].objects.front().index = 67;
        if ( !expectValidation( map, randomMonsterValidationInfo, false, "evaluated or mismatched random monster tier" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[0].objects.push_back( { heroUid, Maps::ObjectGroup::LANDSCAPE_MOUNTAINS, 0 } );
        if ( !expectValidation( map, validationInfo, false, "secondary sprite emitted as another object root" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[1].objects.push_back( { 300, Maps::ObjectGroup::ROADS, 2 } );
        auto roadValidationInfo = validationInfo;
        roadValidationInfo.roadPositions = { 1 };
        if ( !Maps::Map_Format::validateImportedMapObjectPlacement( map, roadValidationInfo )
             || Maps::Map_Format::validateImportedMapObjectPlacement( map, validationInfo ) ) {
            std::cerr << "road tile placement validation failed.\n";
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[1].objects.push_back( { 300, Maps::ObjectGroup::ADVENTURE_POWER_UPS, 0 } );
        map.selectionObjectMetadata[300].selectedItems = { 1 };
        map.dailyEvents.emplace_back();
        map.rumors.emplace_back( "Rumor" );

        auto metadataValidationInfo = validationInfo;
        metadataValidationInfo.selectionObjectPositions = { 1 };
        metadataValidationInfo.dailyEventCount = 1;
        metadataValidationInfo.rumorCount = 1;
        if ( !expectValidation( map, metadataValidationInfo, true, "supported map metadata" )
             || !expectValidation( map, validationInfo, false, "unexpected map metadata" ) ) {
            return 1;
        }
    }

    {
        auto map = makeValidMap();
        map.tiles[2].objects.push_back( { 400, Maps::ObjectGroup::ADVENTURE_ARTIFACTS, 0 } );
        map.artifactMetadata[400].radius = 3;

        auto artifactValidationInfo = validationInfo;
        artifactValidationInfo.artifactPositions = { 2 };
        artifactValidationInfo.ultimateArtifactPosition = 2;
        artifactValidationInfo.ultimateArtifactObjectUid = 400;
        if ( !expectValidation( map, artifactValidationInfo, true, "Ultimate Artifact marker" ) ) {
            return 1;
        }
    }

    auto missingHeroInfo = validationInfo;
    missingHeroInfo.heroPositions.clear();
    auto missingCastleInfo = validationInfo;
    missingCastleInfo.castlePositions.clear();
    if ( !expectValidation( makeValidMap(), missingHeroInfo, false, "missing expected hero" )
         || !expectValidation( makeValidMap(), missingCastleInfo, false, "missing expected castle" ) ) {
        return 1;
    }

    std::cout << "Imported map object placement validation tests passed.\n";
    return 0;
}
