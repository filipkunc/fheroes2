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
#include <limits>

#include "map_object_info.h"

namespace
{
    bool areSameObjectParts( const Maps::ObjectPartInfo & left, const Maps::ObjectPartInfo & right )
    {
        return left.tileOffset == right.tileOffset && left.icnIndex == right.icnIndex && left.objectType == right.objectType && left.icnType == right.icnType
               && left.animationFrames == right.animationFrames;
    }

    bool areSameObjects( const Maps::ObjectInfo & left, const Maps::ObjectInfo & right )
    {
        if ( left.objectType != right.objectType || left.metadata != right.metadata || left.groundLevelParts.size() != right.groundLevelParts.size()
             || left.topLevelParts.size() != right.topLevelParts.size() ) {
            return false;
        }

        for ( size_t i = 0; i < left.groundLevelParts.size(); ++i ) {
            if ( left.groundLevelParts[i].layerType != right.groundLevelParts[i].layerType
                 || !areSameObjectParts( left.groundLevelParts[i], right.groundLevelParts[i] ) ) {
                return false;
            }
        }

        for ( size_t i = 0; i < left.topLevelParts.size(); ++i ) {
            if ( !areSameObjectParts( left.topLevelParts[i], right.topLevelParts[i] ) ) {
                return false;
            }
        }

        return true;
    }
}

int main()
{
    size_t checkedObjectCount = 0;

    for ( uint32_t groupIndex = 0; groupIndex < static_cast<uint32_t>( Maps::ObjectGroup::GROUP_COUNT ); ++groupIndex ) {
        const auto group = static_cast<Maps::ObjectGroup>( groupIndex );
        const auto & objects = Maps::getObjectsByGroup( group );

        for ( size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex ) {
            const Maps::ObjectInfo & object = objects[objectIndex];
            if ( object.groundLevelParts.empty() ) {
                std::cerr << "Object group " << groupIndex << " contains an object without a main part.\n";
                return 1;
            }

            const Maps::ObjectPartInfo & mainPart = object.groundLevelParts.front();
            Maps::ObjectGroup resolvedGroup = Maps::ObjectGroup::GROUP_COUNT;
            uint32_t resolvedIndex = std::numeric_limits<uint32_t>::max();

            if ( !Maps::getObjectGroupAndIndexByMainIcn( mainPart.icnType, mainPart.icnIndex, resolvedGroup, resolvedIndex ) ) {
                std::cerr << "Failed to resolve main ICN sprite " << static_cast<uint32_t>( mainPart.icnType ) << ':' << mainPart.icnIndex << ".\n";
                return 1;
            }

            const Maps::ObjectInfo & resolvedObject = Maps::getObjectInfo( resolvedGroup, static_cast<int32_t>( resolvedIndex ) );
            if ( resolvedObject.groundLevelParts.empty() ) {
                std::cerr << "Resolved an empty editor object.\n";
                return 1;
            }

            const Maps::ObjectPartInfo & resolvedMainPart = resolvedObject.groundLevelParts.front();
            if ( resolvedMainPart.icnType != mainPart.icnType || resolvedMainPart.icnIndex != mainPart.icnIndex ) {
                std::cerr << "Reverse lookup resolved a different main ICN sprite.\n";
                return 1;
            }

            if ( !areSameObjects( object, resolvedObject ) ) {
                if ( resolvedGroup != group || ( group != Maps::ObjectGroup::ROADS && group != Maps::ObjectGroup::ADVENTURE_MINES ) ) {
                    std::cerr << "Main ICN sprite " << static_cast<uint32_t>( mainPart.icnType ) << ':' << mainPart.icnIndex << " identifies object " << groupIndex << ':'
                              << objectIndex << " and resolved object " << static_cast<uint32_t>( resolvedGroup ) << ':' << resolvedIndex
                              << " with different parts or layers.\n";
                    return 1;
                }
            }

            ++checkedObjectCount;
        }
    }

    Maps::ObjectGroup invalidGroup = Maps::ObjectGroup::NONE;
    uint32_t invalidIndex = 0;
    if ( Maps::getObjectGroupAndIndexByMainIcn( MP2::OBJ_ICN_TYPE_UNKNOWN, std::numeric_limits<uint32_t>::max(), invalidGroup, invalidIndex ) ) {
        std::cerr << "An invalid main ICN sprite unexpectedly resolved to an editor object.\n";
        return 1;
    }

    if ( checkedObjectCount == 0 ) {
        std::cerr << "No editor objects were checked.\n";
        return 1;
    }

    std::cout << "Validated main ICN reverse lookup for " << checkedObjectCount << " editor objects.\n";
    return 0;
}
