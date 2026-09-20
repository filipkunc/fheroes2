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
#include <cstdint>
#include <iostream>

#include "heroes.h"
#include "heroes_specialty.h"
#include "monster.h"
#include "resource.h"
#include "spell.h"

namespace
{
    struct UnitSpecialty
    {
        int32_t heroId;
        std::array<int32_t, 6> monsterIds;
        size_t monsterCount;
        int32_t attackBonus;
        int32_t defenseBonus;
        int32_t speedBonus;
    };

    constexpr std::array<UnitSpecialty, 8> unitSpecialties{ {
        { Heroes::LORDKILBURN, { Monster::PEASANT, Monster::MAID }, 2, 4, 4, 1 },
        { Heroes::GVENNETH, { Monster::CAVALRY, Monster::CHAMPION, Monster::PALADIN, Monster::CRUSADER, Monster::AVENGER }, 5, 5, 5, 1 },
        { Heroes::THUNDAX, { Monster::OGRE, Monster::OGRE_LORD, Monster::TROLL, Monster::WAR_TROLL }, 4, 3, 4, 1 },
        { Heroes::ERGON, { Monster::WOLF, Monster::DACHSHUND }, 2, 5, 2, 1 },
        { Heroes::KASTORE,
          { Monster::GREEN_DRAGON, Monster::RED_DRAGON, Monster::BLACK_DRAGON, Monster::AZURE_DRAGON, Monster::BONE_DRAGON, Monster::BLOOD_DRAGON },
          6,
          5,
          5,
          1 },
        { Heroes::MYRA, { Monster::BOAR, Monster::ROC }, 2, 3, 3, 1 },
        { Heroes::SARAKIN, { Monster::THOR, Monster::MAGE, Monster::ARCHMAGE, Monster::GIANT, Monster::TITAN }, 5, 2, 2, 1 },
        { Heroes::RANLOO, { Monster::VAMPIRE, Monster::VAMPIRE_LORD, Monster::LICH, Monster::POWER_LICH }, 4, 4, 4, 1 },
    } };

    struct UnitChain
    {
        int32_t heroId;
        int32_t baseMonsterId;
        int32_t customMonsterId;
        int32_t legacyFkMonsterId;
        int32_t attackBonus;
        int32_t defenseBonus;
    };

    constexpr std::array<UnitChain, 5> customUnitChains{ {
        { Heroes::LORDKILBURN, Monster::PEASANT, Monster::MAID, 73, 4, 4 },
        { Heroes::GVENNETH, Monster::CRUSADER, Monster::AVENGER, 70, 5, 5 },
        { Heroes::ERGON, Monster::WOLF, Monster::DACHSHUND, 72, 5, 2 },
        { Heroes::KASTORE, Monster::BLACK_DRAGON, Monster::AZURE_DRAGON, 67, 5, 5 },
        { Heroes::SARAKIN, Monster::TITAN, Monster::THOR, 69, 2, 2 },
    } };

    struct SpellSpecialty
    {
        int32_t heroId;
        int32_t spellId;
        int32_t effectivenessPercent;
        int32_t spellPointCostReduction;
    };

    constexpr std::array<SpellSpecialty, 8> spellSpecialties{ {
        { Heroes::TYRO, Spell::RESURRECTTRUE, 50, 10 },
        { Heroes::CARLAWN, Spell::LIGHTNINGBOLT, 33, 1 },
        { Heroes::BAROK, Spell::ARROW, 50, 0 },
        { Heroes::FLINT, Spell::COLDRAY, 20, 1 },
        { Heroes::HALON, Spell::PARALYZE, 0, 2 },
        { Heroes::KALINDRA, Spell::METEORSHOWER, 15, 4 },
        { Heroes::ROXANA, Spell::ANIMATEDEAD, 20, 2 },
        { Heroes::SANDRO, Spell::DEATHWAVE, 25, 5 },
    } };
}

int main()
{
    if ( getHeroSpecialty( -1 ).type != HeroSpecialty::Type::NONE || getHeroSpecialty( Heroes::HEROES_COUNT ).type != HeroSpecialty::Type::NONE ) {
        std::cerr << "Invalid hero IDs must return an empty specialty.\n";
        return 1;
    }

    size_t specialtyCount = 0;
    for ( int32_t heroId = Heroes::UNKNOWN; heroId < Heroes::HEROES_COUNT; ++heroId ) {
        if ( getHeroSpecialty( heroId ).type != HeroSpecialty::Type::NONE ) {
            ++specialtyCount;
        }
    }
    if ( specialtyCount != 19 ) {
        std::cerr << "The preserved specialty table must contain exactly 19 non-empty entries.\n";
        return 1;
    }

    for ( const UnitSpecialty & expected : unitSpecialties ) {
        const HeroSpecialty & specialty = getHeroSpecialty( expected.heroId );
        if ( specialty.type != HeroSpecialty::Type::UNIT || specialty.monsterIds != expected.monsterIds || specialty.monsterCount != expected.monsterCount
             || specialty.attackBonus != expected.attackBonus || specialty.defenseBonus != expected.defenseBonus || specialty.speedBonus != expected.speedBonus ) {
            std::cerr << "A unit specialty does not match the preserved definition.\n";
            return 1;
        }
    }

    for ( const UnitChain & chain : customUnitChains ) {
        const HeroSpecialty & specialty = getHeroSpecialty( chain.heroId );
        if ( specialty.type != HeroSpecialty::Type::UNIT || !specialty.affectsMonster( chain.baseMonsterId ) || !specialty.affectsMonster( chain.customMonsterId )
             || specialty.affectsMonster( chain.legacyFkMonsterId ) || getSpecialtyAttackBonus( specialty, chain.customMonsterId ) != chain.attackBonus
             || getSpecialtyDefenseBonus( specialty, chain.customMonsterId ) != chain.defenseBonus || getSpecialtySpeedBonus( specialty, chain.customMonsterId ) != 1 ) {
            std::cerr << "A custom-creature specialty chain or bonus does not match the preserved definition.\n";
            return 1;
        }
    }

    // Kastore's specialty includes both stable custom dragon IDs.
    if ( !getHeroSpecialty( Heroes::KASTORE ).affectsMonster( Monster::BLOOD_DRAGON ) ) {
        std::cerr << "The Blood Dragon is missing from Kastore's specialty.\n";
        return 1;
    }

    for ( const SpellSpecialty & expected : spellSpecialties ) {
        const HeroSpecialty & specialty = getHeroSpecialty( expected.heroId );
        if ( specialty.type != HeroSpecialty::Type::SPELL || specialty.spellId != expected.spellId
             || getSpecialtySpellEffectivenessPercent( specialty, expected.spellId ) != expected.effectivenessPercent
             || getSpecialtySpellPointCostReduction( specialty, expected.spellId ) != expected.spellPointCostReduction
             || getSpecialtySpellEffectivenessPercent( specialty, Spell::NONE ) != 0 || getSpecialtySpellPointCostReduction( specialty, Spell::NONE ) != 0 ) {
            std::cerr << "A spell specialty effect does not match the preserved definition.\n";
            return 1;
        }
    }

    const HeroSpecialty & ector = getHeroSpecialty( Heroes::ECTOR );
    const HeroSpecialty & ruby = getHeroSpecialty( Heroes::RUBY );
    const HeroSpecialty & mandigal = getHeroSpecialty( Heroes::MANDIGAL );
    if ( ector.type != HeroSpecialty::Type::RESOURCE || ector.resourceId != Resource::WOOD || ector.resourceAmountPerDay != 2
         || ruby.type != HeroSpecialty::Type::RESOURCE || ruby.resourceId != Resource::CRYSTAL || ruby.resourceAmountPerDay != 1
         || mandigal.type != HeroSpecialty::Type::RESOURCE || mandigal.resourceId != Resource::GEMS || mandigal.resourceAmountPerDay != 1 ) {
        std::cerr << "A daily-resource specialty does not match the preserved definition.\n";
        return 1;
    }

    return 0;
}
