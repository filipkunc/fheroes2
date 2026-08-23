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

#include <array>
#include <cstddef>
#include <cstdint>
#include <iostream>
#include <set>
#include <string>
#include <vector>

#include "castle.h"
#include "monster.h"
#include "monster_info.h"
#include "serialize.h"

namespace
{
    struct ExpectedDefinition
    {
        int32_t id;
        int32_t legacyFkId;
        const char * stableKey;
        int32_t fallbackMonsterId;
        int32_t upgradeFromMonsterId;
        uint64_t dwellingId;
        Cost upgradeCost;
        uint32_t attack;
        uint32_t defense;
        uint32_t hitPoints;
    };

    constexpr std::array<ExpectedDefinition, 7> expectedDefinitions{ {
        { Monster::AZURE_DRAGON, 67, "azure_dragon", Monster::BLACK_DRAGON, Monster::BLACK_DRAGON, DWELLING_UPGRADE8, {}, 16, 16, 400 },
        { Monster::BLOOD_DRAGON, 68, "blood_dragon", Monster::BONE_DRAGON, Monster::BONE_DRAGON, DWELLING_UPGRADE9, { 2000, 0, 1, 0, 0, 0, 0 }, 13, 11, 200 },
        { Monster::THOR, 69, "thor", Monster::TITAN, Monster::TITAN, DWELLING_UPGRADE10, { 2000, 0, 0, 0, 0, 0, 1 }, 16, 16, 300 },
        { Monster::AVENGER, 70, "avenger", Monster::CRUSADER, Monster::CRUSADER, DWELLING_UPGRADE11, { 500, 0, 0, 0, 0, 0, 0 }, 15, 15, 100 },
        { Monster::SUCCUBUS, 71, "succubus", Monster::GARGOYLE, Monster::CYCLOPS, DWELLING_UPGRADE12, { 1750, 0, 0, 0, 0, 0, 0 }, 13, 12, 250 },
        { Monster::DACHSHUND, 72, "dachshund", Monster::WOLF, Monster::WOLF, DWELLING_UPGRADE13, {}, 6, 6, 50 },
        { Monster::MAID, 73, "maid", Monster::PEASANT, Monster::PEASANT, DWELLING_UPGRADE14, {}, 2, 2, 5 },
    } };

    bool areSameCosts( const Cost & left, const Cost & right )
    {
        return left.gold == right.gold && left.wood == right.wood && left.mercury == right.mercury && left.ore == right.ore && left.sulfur == right.sulfur
               && left.crystal == right.crystal && left.gems == right.gems;
    }
}

int main()
{
    if ( Monster::RANDOM_MONSTER != 67 || Monster::RANDOM_MONSTER_LEVEL_4 != 71 || Monster::MONSTER_COUNT >= Monster::CUSTOM_MONSTER_ID_BEGIN ) {
        std::cerr << "The upstream monster ID namespace changed.\n";
        return 1;
    }

    const std::vector<fheroes2::CustomMonsterDefinition> & definitions = fheroes2::getCustomMonsterDefinitions();
    if ( definitions.size() != expectedDefinitions.size() ) {
        std::cerr << "Unexpected custom creature registry size.\n";
        return 1;
    }

    std::set<int32_t> ids;
    std::set<int32_t> legacyIds;
    std::set<int32_t> upgradeBaseIds;
    std::set<uint64_t> dwellingIds;
    std::set<std::string> stableKeys;
    std::vector<int32_t> serializedIds;

    for ( size_t i = 0; i < definitions.size(); ++i ) {
        const fheroes2::CustomMonsterDefinition & definition = definitions[i];
        const ExpectedDefinition & expected = expectedDefinitions[i];

        if ( definition.id != expected.id || definition.legacyFkId != expected.legacyFkId || definition.stableKey != std::string( expected.stableKey )
             || definition.fallbackMonsterId != expected.fallbackMonsterId || definition.upgradeFromMonsterId != expected.upgradeFromMonsterId
             || definition.dwellingId != expected.dwellingId || !areSameCosts( definition.upgradeCost, expected.upgradeCost )
             || definition.data.battleStats.attack != expected.attack || definition.data.battleStats.defense != expected.defense
             || definition.data.battleStats.hp != expected.hitPoints ) {
            std::cerr << "Custom creature metadata mismatch at registry index " << i << ".\n";
            return 1;
        }

        if ( !ids.emplace( definition.id ).second || !legacyIds.emplace( definition.legacyFkId ).second
             || !upgradeBaseIds.emplace( definition.upgradeFromMonsterId ).second || !dwellingIds.emplace( definition.dwellingId ).second
             || !stableKeys.emplace( definition.stableKey ).second ) {
            std::cerr << "Custom creature identifiers, upgrade bases, dwellings and keys must be unique.\n";
            return 1;
        }

        if ( !fheroes2::isCustomMonsterId( definition.id ) || fheroes2::getCustomMonsterFallbackId( definition.id ) != definition.fallbackMonsterId
             || fheroes2::findCustomMonsterDefinition( definition.id ) != &definition
             || fheroes2::findCustomMonsterDefinitionByUpgradeBase( definition.upgradeFromMonsterId ) != &definition
             || fheroes2::findCustomMonsterDefinitionByLegacyFkId( definition.legacyFkId ) != &definition ) {
            std::cerr << "Custom creature lookup mismatch.\n";
            return 1;
        }

        if ( definition.dwellingId <= UINT32_MAX ) {
            std::cerr << "Custom creature gameplay metadata is outside its supported range.\n";
            return 1;
        }

        serializedIds.emplace_back( definition.id );
    }

    // The old FK IDs overlap the upstream random-placeholder namespace. Normal lookup must never reinterpret them.
    for ( int32_t placeholderId = Monster::RANDOM_MONSTER; placeholderId <= Monster::RANDOM_MONSTER_LEVEL_4; ++placeholderId ) {
        if ( fheroes2::isCustomMonsterId( placeholderId ) || fheroes2::findCustomMonsterDefinition( placeholderId ) != nullptr
             || fheroes2::getCustomMonsterFallbackId( placeholderId ) != placeholderId ) {
            std::cerr << "A random monster placeholder was reinterpreted as a custom creature.\n";
            return 1;
        }
    }

    // Monster IDs are stored as signed 32-bit values in saves and FH2M metadata. Verify the reserved IDs survive that wire representation exactly.
    RWStreamBuf stream;
    stream << serializedIds;
    stream.seek( 0 );

    std::vector<int32_t> restoredIds;
    stream >> restoredIds;
    if ( stream.fail() || restoredIds != serializedIds ) {
        std::cerr << "Custom creature IDs failed their serialization round trip.\n";
        return 1;
    }

    const std::vector<uint64_t> serializedDwellings( dwellingIds.cbegin(), dwellingIds.cend() );
    RWStreamBuf dwellingStream;
    dwellingStream << serializedDwellings;
    dwellingStream.seek( 0 );

    std::vector<uint64_t> restoredDwellings;
    dwellingStream >> restoredDwellings;
    if ( dwellingStream.fail() || restoredDwellings != serializedDwellings ) {
        std::cerr << "64-bit custom dwelling IDs failed their serialization round trip.\n";
        return 1;
    }

    return 0;
}
