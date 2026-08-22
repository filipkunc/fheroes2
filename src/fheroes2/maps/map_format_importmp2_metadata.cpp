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

#include "map_format_importmp2_metadata.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <map>
#include <string>
#include <utility>

#include "artifact.h"
#include "castle.h"
#include "color.h"
#include "heroes.h"
#include "map_format_info.h"
#include "monster.h"
#include "mp2.h"
#include "resource.h"
#include "skill.h"

namespace
{
    uint16_t getLE16( const std::vector<uint8_t> & data, const size_t offset )
    {
        return static_cast<uint16_t>( data[offset] | ( static_cast<uint16_t>( data[offset + 1] ) << 8 ) );
    }

    uint32_t getLE32( const std::vector<uint8_t> & data, const size_t offset )
    {
        return static_cast<uint32_t>( data[offset] ) | ( static_cast<uint32_t>( data[offset + 1] ) << 8 ) | ( static_cast<uint32_t>( data[offset + 2] ) << 16 )
               | ( static_cast<uint32_t>( data[offset + 3] ) << 24 );
    }

    std::string getString( const std::vector<uint8_t> & data, const size_t offset, const size_t maximumLength )
    {
        if ( offset >= data.size() ) {
            return {};
        }

        const size_t availableLength = std::min( maximumLength, data.size() - offset );
        const auto stringEnd
            = std::find( data.cbegin() + static_cast<ptrdiff_t>( offset ), data.cbegin() + static_cast<ptrdiff_t>( offset + availableLength ), uint8_t{ 0 } );

        return { data.cbegin() + static_cast<ptrdiff_t>( offset ), stringEnd };
    }

    int32_t getArtifactId( const uint16_t mp2ArtifactId )
    {
        return Artifact{ static_cast<int32_t>( mp2ArtifactId ) + 1 }.GetID();
    }

    int32_t getMonsterId( const uint8_t mp2MonsterId )
    {
        const int32_t monsterId = static_cast<int32_t>( mp2MonsterId ) + 1;
        return monsterId > Monster::UNKNOWN && monsterId < Monster::MONSTER_COUNT ? monsterId : Monster::UNKNOWN;
    }

    void addBuilding( const uint16_t value, const uint16_t mask, const uint32_t building, std::vector<uint32_t> & buildings )
    {
        if ( ( value & mask ) != 0 ) {
            buildings.push_back( building );
        }
    }
}

namespace Maps::Map_Format
{
    bool readMP2CastleMetadata( const std::vector<uint8_t> & data, CastleMetadata & metadata )
    {
        if ( data.size() != MP2::MP2_CASTLE_STRUCTURE_SIZE ) {
            return false;
        }

        metadata = {};

        const bool hasCustomBuildings = data[1] != 0;
        metadata.customBuildings = hasCustomBuildings;
        if ( hasCustomBuildings ) {
            const uint16_t commonBuildings = getLE16( data, 2 );
            addBuilding( commonBuildings, 0x0002, BUILD_THIEVESGUILD, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0004, BUILD_TAVERN, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0008, BUILD_SHIPYARD, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0010, BUILD_WELL, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0080, BUILD_STATUE, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0100, BUILD_LEFTTURRET, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0200, BUILD_RIGHTTURRET, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0400, BUILD_MARKETPLACE, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x0800, BUILD_WEL2, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x1000, BUILD_MOAT, metadata.builtBuildings );
            addBuilding( commonBuildings, 0x2000, BUILD_SPEC, metadata.builtBuildings );

            const uint16_t dwellings = getLE16( data, 4 );
            addBuilding( dwellings, 0x0008, DWELLING_MONSTER1, metadata.builtBuildings );
            addBuilding( dwellings, 0x0010, DWELLING_MONSTER2, metadata.builtBuildings );
            addBuilding( dwellings, 0x0020, DWELLING_MONSTER3, metadata.builtBuildings );
            addBuilding( dwellings, 0x0040, DWELLING_MONSTER4, metadata.builtBuildings );
            addBuilding( dwellings, 0x0080, DWELLING_MONSTER5, metadata.builtBuildings );
            addBuilding( dwellings, 0x0100, DWELLING_MONSTER6, metadata.builtBuildings );

            const std::array<std::pair<uint16_t, uint32_t>, 5> upgrades{ { { static_cast<uint16_t>( 0x0200 ), DWELLING_UPGRADE2 },
                                                                           { static_cast<uint16_t>( 0x0400 ), DWELLING_UPGRADE3 },
                                                                           { static_cast<uint16_t>( 0x0800 ), DWELLING_UPGRADE4 },
                                                                           { static_cast<uint16_t>( 0x1000 ), DWELLING_UPGRADE5 },
                                                                           { static_cast<uint16_t>( 0x2000 ), DWELLING_UPGRADE6 } } };
            for ( size_t i = 0; i < upgrades.size(); ++i ) {
                if ( ( dwellings & upgrades[i].first ) != 0 ) {
                    const uint32_t baseDwelling = DWELLING_MONSTER2 << i;
                    if ( std::find( metadata.builtBuildings.cbegin(), metadata.builtBuildings.cend(), baseDwelling ) == metadata.builtBuildings.cend() ) {
                        metadata.builtBuildings.push_back( baseDwelling );
                    }
                    metadata.builtBuildings.push_back( upgrades[i].second );
                }
            }

            const uint8_t mageGuildLevel = std::min<uint8_t>( data[6], 5 );
            const std::array<uint32_t, 5> mageGuilds{ BUILD_MAGEGUILD1, BUILD_MAGEGUILD2, BUILD_MAGEGUILD3, BUILD_MAGEGUILD4, BUILD_MAGEGUILD5 };
            metadata.builtBuildings.insert( metadata.builtBuildings.end(), mageGuilds.cbegin(), mageGuilds.cbegin() + mageGuildLevel );
        }

        if ( data[7] != 0 ) {
            for ( size_t i = 0; i < metadata.defenderMonsterType.size(); ++i ) {
                metadata.defenderMonsterType[i] = getMonsterId( data[8 + i] );
                metadata.defenderMonsterCount[i] = static_cast<int32_t>( getLE16( data, 13 + i * 2 ) );
            }
        }
        else {
            metadata.defenderMonsterType.fill( -1 );
        }

        if ( data[23] != 0 ) {
            metadata.builtBuildings.push_back( BUILD_CAPTAIN );
        }
        if ( data[24] != 0 ) {
            metadata.customName = getString( data, 25, 13 );
        }

        metadata.builtBuildings.push_back( data[39] != 0 ? BUILD_CASTLE : BUILD_TENT );
        if ( data[40] != 0 ) {
            metadata.bannedBuildings.push_back( BUILD_CASTLE );
        }

        return true;
    }

    bool readMP2HeroMetadata( const std::vector<uint8_t> & data, const uint8_t race, HeroMetadata & metadata )
    {
        if ( data.size() != MP2::MP2_HEROES_STRUCTURE_SIZE ) {
            return false;
        }

        metadata = {};
        metadata.race = race;

        if ( data[1] != 0 ) {
            for ( size_t i = 0; i < metadata.armyMonsterType.size(); ++i ) {
                metadata.armyMonsterType[i] = getMonsterId( data[2 + i] );
                metadata.armyMonsterCount[i] = static_cast<int32_t>( getLE16( data, 7 + i * 2 ) );
            }
        }

        if ( data[17] != 0 ) {
            const int32_t portrait = static_cast<int32_t>( data[18] ) + 1;
            if ( !Heroes::isValidId( portrait ) ) {
                return false;
            }
            metadata.customPortrait = portrait;
        }

        for ( size_t i = 0; i < 3; ++i ) {
            metadata.artifact[i] = getArtifactId( data[19 + i] );
        }

        metadata.customExperience = static_cast<int32_t>( getLE32( data, 23 ) );

        if ( data[27] != 0 ) {
            for ( size_t i = 0; i < metadata.secondarySkill.size(); ++i ) {
                const int32_t skill = static_cast<int32_t>( data[28 + i] ) + 1;
                const uint8_t level = data[36 + i];
                if ( skill <= Skill::Secondary::ESTATES && level >= Skill::Level::BASIC && level <= Skill::Level::EXPERT ) {
                    metadata.secondarySkill[i] = static_cast<int8_t>( skill );
                    metadata.secondarySkillLevel[i] = level;
                }
            }
        }

        if ( data[45] != 0 ) {
            metadata.customName = getString( data, 46, 13 );
        }

        metadata.isOnPatrol = data[59] != 0;
        if ( metadata.isOnPatrol ) {
            metadata.patrolRadius = data[60];
        }

        return true;
    }

    bool readMP2SignMetadata( const std::vector<uint8_t> & data, SignMetadata & metadata )
    {
        if ( data.size() < MP2::MP2_SIGN_STRUCTURE_MIN_SIZE || data[0] != 1 ) {
            return false;
        }

        metadata = {};
        metadata.message = getString( data, 9, data.size() - 9 );
        return true;
    }

    bool readMP2AdventureMapEventMetadata( const std::vector<uint8_t> & data, AdventureMapEventMetadata & metadata )
    {
        if ( data.size() < MP2::MP2_EVENT_STRUCTURE_MIN_SIZE || data[0] != 1 ) {
            return false;
        }

        metadata = {};
        metadata.resources.wood = static_cast<int32_t>( getLE32( data, 1 ) );
        metadata.resources.mercury = static_cast<int32_t>( getLE32( data, 5 ) );
        metadata.resources.ore = static_cast<int32_t>( getLE32( data, 9 ) );
        metadata.resources.sulfur = static_cast<int32_t>( getLE32( data, 13 ) );
        metadata.resources.crystal = static_cast<int32_t>( getLE32( data, 17 ) );
        metadata.resources.gems = static_cast<int32_t>( getLE32( data, 21 ) );
        metadata.resources.gold = static_cast<int32_t>( getLE32( data, 25 ) );
        metadata.artifact = getArtifactId( getLE16( data, 29 ) );
        metadata.isRecurringEvent = data[32] == 0;

        const std::array<PlayerColor, 6> colors{ PlayerColor::BLUE, PlayerColor::GREEN, PlayerColor::RED, PlayerColor::YELLOW, PlayerColor::ORANGE, PlayerColor::PURPLE };
        for ( size_t i = 0; i < colors.size(); ++i ) {
            if ( data[43 + i] != 0 ) {
                metadata.humanPlayerColors |= colors[i];
            }
        }
        if ( data[31] != 0 ) {
            metadata.computerPlayerColors = metadata.humanPlayerColors;
        }

        metadata.message = getString( data, 49, data.size() - 49 );
        return true;
    }

    bool readMP2SphinxMetadata( const std::vector<uint8_t> & data, SphinxMetadata & metadata )
    {
        if ( data.size() < MP2::MP2_RIDDLE_STRUCTURE_MIN_SIZE || data[0] != 0 ) {
            return false;
        }

        metadata = {};
        metadata.resources.wood = static_cast<int32_t>( getLE32( data, 1 ) );
        metadata.resources.mercury = static_cast<int32_t>( getLE32( data, 5 ) );
        metadata.resources.ore = static_cast<int32_t>( getLE32( data, 9 ) );
        metadata.resources.sulfur = static_cast<int32_t>( getLE32( data, 13 ) );
        metadata.resources.crystal = static_cast<int32_t>( getLE32( data, 17 ) );
        metadata.resources.gems = static_cast<int32_t>( getLE32( data, 21 ) );
        metadata.resources.gold = static_cast<int32_t>( getLE32( data, 25 ) );
        metadata.artifact = getArtifactId( getLE16( data, 29 ) );

        const size_t answerCount = std::min<size_t>( data[31], 8 );
        metadata.answers.reserve( answerCount );
        for ( size_t i = 0; i < answerCount; ++i ) {
            const std::string answer = getString( data, 32 + i * 13, 13 );
            if ( !answer.empty() ) {
                metadata.answers.push_back( answer );
            }
        }
        metadata.riddle = getString( data, 136, data.size() - 136 );

        return true;
    }
}
