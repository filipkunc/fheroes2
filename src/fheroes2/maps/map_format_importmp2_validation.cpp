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

#include "map_format_importmp2_validation.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <map>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "map_format_info.h"

namespace Maps::Map_Format
{
    bool validateImportedMapObjectPlacement( const MapFormat & map, const MP2MapValidationInfo & expected, std::string * errorMessage )
    {
        const auto fail = [errorMessage]( std::string message ) {
            if ( errorMessage != nullptr ) {
                *errorMessage = std::move( message );
            }
            return false;
        };

        if ( map.width <= 0 || map.tiles.size() != static_cast<size_t>( map.width ) * static_cast<size_t>( map.width ) ) {
            return fail( "invalid map dimensions" );
        }

        std::set<int32_t> heroPositions;
        std::set<int32_t> castlePositions;
        std::set<int32_t> roadPositions;
        std::set<int32_t> artifactPositions;
        std::set<int32_t> selectionObjectPositions;
        std::map<uint32_t, std::pair<int32_t, ObjectGroup>> primaryObjectByUid;
        std::map<uint32_t, int32_t> castlePositionByUid;
        std::map<uint32_t, std::vector<std::pair<int32_t, uint32_t>>> castleFlagsByUid;
        int32_t ultimateArtifactPosition = -1;

        for ( size_t tileIndex = 0; tileIndex < map.tiles.size(); ++tileIndex ) {
            for ( const TileObjectInfo & object : map.tiles[tileIndex].objects ) {
                if ( object.group != ObjectGroup::LANDSCAPE_FLAGS && object.group != ObjectGroup::LANDSCAPE_TOWN_BASEMENTS ) {
                    if ( object.id == 0 || !primaryObjectByUid.emplace( object.id, std::make_pair( static_cast<int32_t>( tileIndex ), object.group ) ).second ) {
                        return fail( "object UID " + std::to_string( object.id ) + " identifies multiple object roots" );
                    }
                }

                if ( map.selectionObjectMetadata.find( object.id ) != map.selectionObjectMetadata.end() ) {
                    selectionObjectPositions.emplace( static_cast<int32_t>( tileIndex ) );
                }

                if ( expected.ultimateArtifactObjectUid != 0 && object.id == expected.ultimateArtifactObjectUid ) {
                    if ( ultimateArtifactPosition >= 0 || object.group != ObjectGroup::ADVENTURE_ARTIFACTS
                         || map.artifactMetadata.find( object.id ) == map.artifactMetadata.end() ) {
                        return fail( "invalid Ultimate Artifact marker" );
                    }
                    ultimateArtifactPosition = static_cast<int32_t>( tileIndex );
                }

                switch ( object.group ) {
                case ObjectGroup::ROADS:
                    if ( !roadPositions.emplace( static_cast<int32_t>( tileIndex ) ).second ) {
                        return fail( "multiple roads occupy tile " + std::to_string( tileIndex ) );
                    }
                    break;
                case ObjectGroup::ADVENTURE_ARTIFACTS:
                    if ( !artifactPositions.emplace( static_cast<int32_t>( tileIndex ) ).second ) {
                        return fail( "multiple artifacts occupy tile " + std::to_string( tileIndex ) );
                    }
                    break;
                case ObjectGroup::KINGDOM_HEROES:
                    if ( !heroPositions.emplace( static_cast<int32_t>( tileIndex ) ).second ) {
                        return fail( "multiple heroes occupy tile " + std::to_string( tileIndex ) );
                    }
                    break;
                case ObjectGroup::KINGDOM_TOWNS:
                    if ( object.id == 0 || !castlePositions.emplace( static_cast<int32_t>( tileIndex ) ).second
                         || !castlePositionByUid.emplace( object.id, static_cast<int32_t>( tileIndex ) ).second ) {
                        return fail( "invalid or duplicate castle at tile " + std::to_string( tileIndex ) );
                    }
                    break;
                case ObjectGroup::LANDSCAPE_FLAGS:
                    if ( object.id == 0 ) {
                        return fail( "flag at tile " + std::to_string( tileIndex ) + " has no object UID" );
                    }
                    castleFlagsByUid[object.id].emplace_back( static_cast<int32_t>( tileIndex ), object.index );
                    break;
                default:
                    break;
                }
            }
        }

        if ( heroPositions != std::set<int32_t>( expected.heroPositions.begin(), expected.heroPositions.end() ) ) {
            return fail( "imported " + std::to_string( heroPositions.size() ) + " of " + std::to_string( expected.heroPositions.size() ) + " source heroes" );
        }

        if ( castlePositions != std::set<int32_t>( expected.castlePositions.begin(), expected.castlePositions.end() ) ) {
            return fail( "imported " + std::to_string( castlePositions.size() ) + " of " + std::to_string( expected.castlePositions.size() ) + " source castles" );
        }

        for ( const auto & [position, group, objectIndex] : expected.placeholderObjects ) {
            if ( position < 0 || static_cast<size_t>( position ) >= map.tiles.size() ) {
                return fail( "placeholder position is outside the map" );
            }

            const auto & objects = map.tiles[static_cast<size_t>( position )].objects;
            if ( std::none_of( objects.cbegin(), objects.cend(),
                               [group, objectIndex]( const TileObjectInfo & object ) { return object.group == group && object.index == objectIndex; } ) ) {
                std::string actualObjects;
                for ( const TileObjectInfo & object : objects ) {
                    actualObjects += " " + std::to_string( static_cast<uint32_t>( object.group ) ) + ':' + std::to_string( object.index );
                }
                return fail( "placeholder at tile " + std::to_string( position ) + " was evaluated during import (expected group "
                             + std::to_string( static_cast<uint32_t>( group ) ) + ", index " + std::to_string( objectIndex ) + "; actual" + actualObjects + ')' );
            }
        }

        if ( roadPositions != std::set<int32_t>( expected.roadPositions.begin(), expected.roadPositions.end() ) ) {
            return fail( "imported " + std::to_string( roadPositions.size() ) + " of " + std::to_string( expected.roadPositions.size() ) + " source road tiles" );
        }

        if ( artifactPositions != std::set<int32_t>( expected.artifactPositions.begin(), expected.artifactPositions.end() ) ) {
            return fail( "imported " + std::to_string( artifactPositions.size() ) + " of " + std::to_string( expected.artifactPositions.size() ) + " source artifacts" );
        }

        if ( selectionObjectPositions != std::set<int32_t>( expected.selectionObjectPositions.begin(), expected.selectionObjectPositions.end() )
             || selectionObjectPositions.size() != map.selectionObjectMetadata.size() ) {
            return fail( "selection metadata does not match the source objects" );
        }

        if ( map.dailyEvents.size() != expected.dailyEventCount ) {
            return fail( "imported " + std::to_string( map.dailyEvents.size() ) + " of " + std::to_string( expected.dailyEventCount ) + " source daily events" );
        }

        if ( map.rumors.size() != expected.rumorCount ) {
            return fail( "imported " + std::to_string( map.rumors.size() ) + " of " + std::to_string( expected.rumorCount ) + " source rumors" );
        }

        if ( ultimateArtifactPosition != expected.ultimateArtifactPosition ) {
            return fail( "Ultimate Artifact marker position does not match the source map" );
        }

        if ( castleFlagsByUid.size() != castlePositionByUid.size() ) {
            return fail( "flag ownership does not match the imported castles" );
        }

        for ( const auto & [castleUid, castlePosition] : castlePositionByUid ) {
            const auto flagIter = castleFlagsByUid.find( castleUid );
            if ( flagIter == castleFlagsByUid.end() || flagIter->second.size() != 2 ) {
                return fail( "castle at tile " + std::to_string( castlePosition ) + " does not have exactly two flags" );
            }

            bool hasLeftFlag = false;
            bool hasRightFlag = false;

            for ( const auto & [flagPosition, flagObjectIndex] : flagIter->second ) {
                if ( flagObjectIndex >= 14 ) {
                    return fail( "castle at tile " + std::to_string( castlePosition ) + " has an invalid flag color" );
                }

                if ( flagPosition == castlePosition - 1 && flagObjectIndex % 2 == 0 ) {
                    hasLeftFlag = true;
                }
                else if ( flagPosition != castlePosition + 1 || flagObjectIndex % 2 == 0 ) {
                    return fail( "castle at tile " + std::to_string( castlePosition ) + " has a flag in the wrong position" );
                }
                else {
                    hasRightFlag = true;
                }
            }

            if ( !hasLeftFlag || !hasRightFlag ) {
                return fail( "castle at tile " + std::to_string( castlePosition ) + " is missing a left or right flag" );
            }

            const auto & firstFlag = flagIter->second.front();
            const auto & secondFlag = flagIter->second.back();
            if ( firstFlag.second / 2 != secondFlag.second / 2 ) {
                return fail( "castle at tile " + std::to_string( castlePosition ) + " has mismatched flag colors" );
            }
        }

        return true;
    }
}
