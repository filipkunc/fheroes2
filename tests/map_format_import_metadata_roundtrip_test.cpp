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

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "artifact.h"
#include "castle.h"
#include "color.h"
#include "map_format_importmp2_metadata.h"
#include "map_format_info.h"
#include "map_object_info.h"
#include "monster.h"
#include "mp2.h"
#include "race.h"
#include "serialize.h"
#include "skill.h"

bool Funds::operator==( const Funds & other ) const
{
    return std::tie( wood, mercury, ore, sulfur, crystal, gems, gold )
           == std::tie( other.wood, other.mercury, other.ore, other.sulfur, other.crystal, other.gems, other.gold );
}

OStreamBase & operator<<( OStreamBase & stream, const Funds & funds )
{
    return stream << funds.wood << funds.mercury << funds.ore << funds.sulfur << funds.crystal << funds.gems << funds.gold;
}

IStreamBase & operator>>( IStreamBase & stream, Funds & funds )
{
    return stream >> funds.wood >> funds.mercury >> funds.ore >> funds.sulfur >> funds.crystal >> funds.gems >> funds.gold;
}

namespace
{
    void putLE16( std::vector<uint8_t> & data, const size_t offset, const uint16_t value )
    {
        data[offset] = static_cast<uint8_t>( value );
        data[offset + 1] = static_cast<uint8_t>( value >> 8 );
    }

    void putLE32( std::vector<uint8_t> & data, const size_t offset, const uint32_t value )
    {
        data[offset] = static_cast<uint8_t>( value );
        data[offset + 1] = static_cast<uint8_t>( value >> 8 );
        data[offset + 2] = static_cast<uint8_t>( value >> 16 );
        data[offset + 3] = static_cast<uint8_t>( value >> 24 );
    }

    void putString( std::vector<uint8_t> & data, const size_t offset, const size_t fieldSize, const std::string & value )
    {
        const size_t length = std::min( value.size(), fieldSize - 1 );
        std::copy_n( value.cbegin(), length, data.begin() + static_cast<ptrdiff_t>( offset ) );
        data[offset + length] = 0;
    }

    std::pair<Maps::ObjectGroup, uint32_t> findObject( const MP2::MapObjectType objectType )
    {
        for ( uint32_t groupIndex = 0; groupIndex < static_cast<uint32_t>( Maps::ObjectGroup::GROUP_COUNT ); ++groupIndex ) {
            const auto group = static_cast<Maps::ObjectGroup>( groupIndex );
            const auto & objects = Maps::getObjectsByGroup( group );
            for ( size_t index = 0; index < objects.size(); ++index ) {
                if ( objects[index].objectType == objectType ) {
                    return { group, static_cast<uint32_t>( index ) };
                }
            }
        }

        return { Maps::ObjectGroup::NONE, 0 };
    }

    bool equalObjects( const std::vector<Maps::Map_Format::TileObjectInfo> & lhs, const std::vector<Maps::Map_Format::TileObjectInfo> & rhs )
    {
        if ( lhs.size() != rhs.size() ) {
            return false;
        }

        for ( size_t i = 0; i < lhs.size(); ++i ) {
            if ( lhs[i].id != rhs[i].id || lhs[i].group != rhs[i].group || lhs[i].index != rhs[i].index ) {
                return false;
            }
        }
        return true;
    }

    bool fail( const std::string & message )
    {
        std::cerr << message << '\n';
        return false;
    }
}

int main()
{
    using namespace Maps::Map_Format;

    MapFormat imported;
    imported.width = 36;
    imported.name = "Synthetic MP2 metadata";
    imported.description = "Round-trip fixture";
    imported.availablePlayerColors = PlayerColor::BLUE | PlayerColor::GREEN | PlayerColor::RED;
    imported.humanPlayerColors = PlayerColor::BLUE | PlayerColor::GREEN;
    imported.computerPlayerColors = PlayerColor::GREEN | PlayerColor::RED;
    imported.tiles.resize( static_cast<size_t>( imported.width ) * imported.width );
    for ( size_t i = 0; i < imported.tiles.size(); ++i ) {
        imported.tiles[i].terrainIndex = static_cast<uint16_t>( ( i * 37 ) % 432 );
        imported.tiles[i].terrainFlags = static_cast<uint8_t>( i % 4 );
    }

    std::vector<uint8_t> castleBlock( MP2::MP2_CASTLE_STRUCTURE_SIZE );
    castleBlock[1] = 1;
    putLE16( castleBlock, 2, 0x0406 ); // Thieves' Guild, Tavern and Marketplace.
    putLE16( castleBlock, 4, 0x0238 ); // Level 1-3 dwellings and upgraded level 2.
    castleBlock[6] = 3;
    castleBlock[7] = 1;
    castleBlock[8] = static_cast<uint8_t>( Monster::ARCHER - 1 );
    castleBlock[9] = static_cast<uint8_t>( Monster::DWARF - 1 );
    putLE16( castleBlock, 13, 23 );
    putLE16( castleBlock, 15, 11 );
    castleBlock[23] = 1;
    castleBlock[24] = 1;
    putString( castleBlock, 25, 13, "Roundtrip" );
    castleBlock[39] = 0;
    castleBlock[40] = 1;

    CastleMetadata castleMetadata;
    if ( !readMP2CastleMetadata( castleBlock, castleMetadata ) ) {
        return fail( "Failed to read the synthetic town block." );
    }
    if ( castleMetadata.customName != "Roundtrip" || !castleMetadata.customBuildings || castleMetadata.defenderMonsterType[0] != Monster::ARCHER
         || castleMetadata.defenderMonsterCount[0] != 23
         || std::find( castleMetadata.builtBuildings.cbegin(), castleMetadata.builtBuildings.cend(), BUILD_TENT ) == castleMetadata.builtBuildings.cend()
         || castleMetadata.bannedBuildings != std::vector<uint32_t>{ BUILD_CASTLE } ) {
        return fail( "The synthetic town metadata was not imported exactly." );
    }

    std::vector<uint8_t> defaultCastleBlock( MP2::MP2_CASTLE_STRUCTURE_SIZE );
    defaultCastleBlock[39] = 1;
    CastleMetadata defaultCastleMetadata;
    if ( !readMP2CastleMetadata( defaultCastleBlock, defaultCastleMetadata ) || !defaultCastleMetadata.customName.empty() || defaultCastleMetadata.customBuildings
         || defaultCastleMetadata.builtBuildings != std::vector<uint32_t>{ BUILD_CASTLE }
         || defaultCastleMetadata.defenderMonsterType != std::array<int32_t, 5>{ -1, -1, -1, -1, -1 } ) {
        return fail( "Default castle metadata was resolved instead of being preserved." );
    }

    std::vector<uint8_t> heroBlock( MP2::MP2_HEROES_STRUCTURE_SIZE, 0xFF );
    heroBlock[0] = 0;
    heroBlock[1] = 0; // Keep the random hero's army unresolved.
    std::fill( heroBlock.begin() + 2, heroBlock.begin() + 17, 0 );
    heroBlock[17] = 0; // Keep the random hero's identity unresolved.
    heroBlock[18] = 0;
    heroBlock[19] = static_cast<uint8_t>( Artifact::GOLDEN_GOOSE - 1 );
    heroBlock[20] = 0xFF;
    heroBlock[21] = 0xFF;
    heroBlock[22] = 0;
    putLE32( heroBlock, 23, 54321 );
    heroBlock[27] = 1;
    std::fill( heroBlock.begin() + 28, heroBlock.begin() + 44, 0xFF );
    heroBlock[28] = static_cast<uint8_t>( Skill::Secondary::LOGISTICS - 1 );
    heroBlock[36] = Skill::Level::EXPERT;
    heroBlock[44] = 0;
    heroBlock[45] = 1;
    std::fill( heroBlock.begin() + 46, heroBlock.end(), 0 );
    putString( heroBlock, 46, 13, "Wanderer" );
    heroBlock[59] = 1;
    heroBlock[60] = 7;

    HeroMetadata heroMetadata;
    if ( !readMP2HeroMetadata( heroBlock, Race::RAND, heroMetadata ) ) {
        return fail( "Failed to read the synthetic random hero block." );
    }
    if ( heroMetadata.race != Race::RAND || heroMetadata.customPortrait != 0 || heroMetadata.armyMonsterType != std::array<int32_t, 5>{ 0 }
         || heroMetadata.artifact[0] != Artifact::GOLDEN_GOOSE || heroMetadata.customExperience != 54321 || heroMetadata.secondarySkill[0] != Skill::Secondary::LOGISTICS
         || heroMetadata.secondarySkillLevel[0] != Skill::Level::EXPERT || heroMetadata.customName != "Wanderer" || !heroMetadata.isOnPatrol
         || heroMetadata.patrolRadius != 7 ) {
        return fail( "The synthetic random hero metadata was not imported exactly." );
    }

    std::vector<uint8_t> signBlock( MP2::MP2_SIGN_STRUCTURE_MIN_SIZE );
    signBlock[0] = 1;
    SignMetadata emptySign;
    if ( !readMP2SignMetadata( signBlock, emptySign ) || !emptySign.message.empty() ) {
        return fail( "An empty sign was resolved instead of being preserved." );
    }

    std::vector<uint8_t> bottleBlock( 32 );
    bottleBlock[0] = 1;
    putString( bottleBlock, 9, bottleBlock.size() - 9, "Message in a bottle" );
    SignMetadata bottle;
    if ( !readMP2SignMetadata( bottleBlock, bottle ) || bottle.message != "Message in a bottle" ) {
        return fail( "Failed to read the synthetic bottle block." );
    }

    std::vector<uint8_t> eventBlock( 80 );
    eventBlock[0] = 1;
    putLE32( eventBlock, 1, static_cast<uint32_t>( -5 ) );
    putLE32( eventBlock, 25, 12345 );
    putLE16( eventBlock, 29, static_cast<uint16_t>( Artifact::MEDAL_VALOR - 1 ) );
    eventBlock[31] = 1;
    eventBlock[32] = 0;
    eventBlock[43] = 1; // Blue.
    eventBlock[45] = 1; // Red.
    putString( eventBlock, 49, eventBlock.size() - 49, "Masked event" );

    AdventureMapEventMetadata eventMetadata;
    if ( !readMP2AdventureMapEventMetadata( eventBlock, eventMetadata ) || eventMetadata.message != "Masked event" || eventMetadata.resources.wood != -5
         || eventMetadata.resources.gold != 12345 || eventMetadata.artifact != Artifact::MEDAL_VALOR || !eventMetadata.isRecurringEvent
         || eventMetadata.humanPlayerColors != ( PlayerColor::BLUE | PlayerColor::RED ) || eventMetadata.computerPlayerColors != eventMetadata.humanPlayerColors ) {
        return fail( "The event player mask or rewards were not imported exactly." );
    }

    std::vector<uint8_t> sphinxBlock( 180 );
    putLE32( sphinxBlock, 21, 17 );
    putLE16( sphinxBlock, 29, static_cast<uint16_t>( Artifact::TRUE_COMPASS_MOBILITY - 1 ) );
    sphinxBlock[31] = 2;
    putString( sphinxBlock, 32, 13, "CamelCase" );
    putString( sphinxBlock, 45, 13, "Second" );
    putString( sphinxBlock, 136, sphinxBlock.size() - 136, "Keep answer case?" );

    SphinxMetadata sphinxMetadata;
    if ( !readMP2SphinxMetadata( sphinxBlock, sphinxMetadata ) || sphinxMetadata.answers != std::vector<std::string>{ "CamelCase", "Second" }
         || sphinxMetadata.riddle != "Keep answer case?" || sphinxMetadata.resources.gems != 17 || sphinxMetadata.artifact != Artifact::TRUE_COMPASS_MOBILITY ) {
        return fail( "The synthetic sphinx metadata was not imported exactly." );
    }

    std::vector<uint8_t> emptySphinxBlock( MP2::MP2_RIDDLE_STRUCTURE_MIN_SIZE );
    putLE16( emptySphinxBlock, 29, 0xFFFF );
    SphinxMetadata emptySphinxMetadata;
    if ( !readMP2SphinxMetadata( emptySphinxBlock, emptySphinxMetadata ) || !emptySphinxMetadata.riddle.empty() || !emptySphinxMetadata.answers.empty() ) {
        return fail( "An invalid-but-present sphinx block was discarded." );
    }

    const auto randomTown = findObject( MP2::OBJ_RANDOM_TOWN );
    const auto randomCastle = findObject( MP2::OBJ_RANDOM_CASTLE );
    const auto randomArtifact = findObject( MP2::OBJ_RANDOM_ARTIFACT_MAJOR );
    const auto randomMonster = findObject( MP2::OBJ_RANDOM_MONSTER_STRONG );
    const auto randomResource = findObject( MP2::OBJ_RANDOM_RESOURCE );
    const auto sign = findObject( MP2::OBJ_SIGN );
    const auto bottleObject = findObject( MP2::OBJ_BOTTLE );
    const auto event = findObject( MP2::OBJ_EVENT );
    const auto sphinx = findObject( MP2::OBJ_SPHINX );
    if ( randomTown.first == Maps::ObjectGroup::NONE || randomCastle.first == Maps::ObjectGroup::NONE || randomArtifact.first == Maps::ObjectGroup::NONE
         || randomMonster.first == Maps::ObjectGroup::NONE || randomResource.first == Maps::ObjectGroup::NONE || sign.first == Maps::ObjectGroup::NONE
         || bottleObject.first == Maps::ObjectGroup::NONE || event.first == Maps::ObjectGroup::NONE || sphinx.first == Maps::ObjectGroup::NONE ) {
        return fail( "A synthetic fixture object could not be resolved in the editor registry." );
    }

    const std::vector<TileObjectInfo> placeholders{ { 1001, Maps::ObjectGroup::KINGDOM_HEROES, 6 },      { 1002, randomTown.first, randomTown.second },
                                                    { 1003, randomCastle.first, randomCastle.second },   { 1004, randomArtifact.first, randomArtifact.second },
                                                    { 1005, randomMonster.first, randomMonster.second }, { 1006, randomResource.first, randomResource.second } };
    imported.tiles[0].objects = placeholders;
    imported.tiles[1].objects.push_back( { 2001, sign.first, sign.second } );
    imported.tiles[2].objects.push_back( { 2002, bottleObject.first, bottleObject.second } );
    imported.tiles[3].objects.push_back( { 3001, event.first, event.second } );
    imported.tiles[4].objects.push_back( { 4001, sphinx.first, sphinx.second } );
    imported.tiles[5].objects.push_back( { 4002, sphinx.first, sphinx.second } );
    imported.castleMetadata.emplace( 1002, castleMetadata );
    imported.castleMetadata.emplace( 1003, defaultCastleMetadata );
    imported.heroMetadata.emplace( 1001, heroMetadata );
    imported.signMetadata.emplace( 2001, emptySign );
    imported.signMetadata.emplace( 2002, bottle );
    imported.adventureMapEventMetadata.emplace( 3001, eventMetadata );
    imported.sphinxMetadata.emplace( 4001, sphinxMetadata );
    imported.sphinxMetadata.emplace( 4002, emptySphinxMetadata );

    const std::string outputPath = "synthetic_mp2_metadata_roundtrip.fh2m";
    if ( !saveMap( outputPath, imported ) ) {
        return fail( "Failed to save the synthetic FH2M fixture." );
    }

    MapFormat reopened;
    const bool isLoaded = loadMap( outputPath, reopened );
    std::remove( outputPath.c_str() );
    if ( !isLoaded ) {
        return fail( "Failed to reopen the synthetic FH2M fixture." );
    }

    for ( size_t i = 0; i < imported.tiles.size(); ++i ) {
        if ( reopened.tiles[i].terrainIndex != imported.tiles[i].terrainIndex || reopened.tiles[i].terrainFlags != imported.tiles[i].terrainFlags
             || !equalObjects( reopened.tiles[i].objects, imported.tiles[i].objects ) ) {
            return fail( "An unresolved random placeholder or map tile changed during FH2M round-trip." );
        }
    }
    if ( reopened.castleMetadata != imported.castleMetadata || reopened.heroMetadata != imported.heroMetadata || reopened.sphinxMetadata != imported.sphinxMetadata
         || reopened.adventureMapEventMetadata != imported.adventureMapEventMetadata || reopened.signMetadata.size() != imported.signMetadata.size()
         || reopened.signMetadata.at( 2001 ).message != imported.signMetadata.at( 2001 ).message
         || reopened.signMetadata.at( 2002 ).message != imported.signMetadata.at( 2002 ).message ) {
        return fail( "Imported MP2 object metadata changed during FH2M round-trip." );
    }

    return EXIT_SUCCESS;
}
