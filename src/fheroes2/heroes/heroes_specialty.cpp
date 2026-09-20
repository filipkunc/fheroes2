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

#include "heroes_specialty.h"

#include <algorithm>

#include "heroes.h"
#include "monster.h"
#include "resource.h"
#include "spell.h"

namespace
{
    constexpr HeroSpecialty makeSpellSpecialty( const int32_t spellId, const int32_t effectivenessPercent, const int32_t spellPointCostReduction )
    {
        HeroSpecialty specialty;
        specialty.type = HeroSpecialty::Type::SPELL;
        specialty.spellId = spellId;
        specialty.spellEffectivenessPercent = effectivenessPercent;
        specialty.spellPointCostReduction = spellPointCostReduction;
        return specialty;
    }

    constexpr HeroSpecialty makeUnitSpecialty( const std::array<int32_t, 6> & monsterIds, const size_t monsterCount, const int32_t attackBonus,
                                               const int32_t defenseBonus, const int32_t speedBonus )
    {
        HeroSpecialty specialty;
        specialty.type = HeroSpecialty::Type::UNIT;
        specialty.monsterIds = monsterIds;
        specialty.monsterCount = monsterCount;
        specialty.attackBonus = attackBonus;
        specialty.defenseBonus = defenseBonus;
        specialty.speedBonus = speedBonus;
        return specialty;
    }

    constexpr HeroSpecialty makeResourceSpecialty( const int32_t resourceId, const int32_t amountPerDay )
    {
        HeroSpecialty specialty;
        specialty.type = HeroSpecialty::Type::RESOURCE;
        specialty.resourceId = resourceId;
        specialty.resourceAmountPerDay = amountPerDay;
        return specialty;
    }

    const std::array<HeroSpecialty, Heroes::HEROES_COUNT> heroSpecialties = []() {
        std::array<HeroSpecialty, Heroes::HEROES_COUNT> specialties{};

        specialties[Heroes::LORDKILBURN] = makeUnitSpecialty( { Monster::PEASANT, Monster::MAID }, 2, 4, 4, 1 );
        specialties[Heroes::ECTOR] = makeResourceSpecialty( Resource::WOOD, 2 );
        specialties[Heroes::GVENNETH] = makeUnitSpecialty( { Monster::CAVALRY, Monster::CHAMPION, Monster::PALADIN, Monster::CRUSADER, Monster::AVENGER }, 5, 5, 5, 1 );
        specialties[Heroes::TYRO] = makeSpellSpecialty( Spell::RESURRECTTRUE, 50, 10 );
        specialties[Heroes::RUBY] = makeResourceSpecialty( Resource::CRYSTAL, 1 );
        specialties[Heroes::THUNDAX] = makeUnitSpecialty( { Monster::OGRE, Monster::OGRE_LORD, Monster::TROLL, Monster::WAR_TROLL }, 4, 3, 4, 1 );
        specialties[Heroes::ERGON] = makeUnitSpecialty( { Monster::WOLF, Monster::DACHSHUND }, 2, 5, 2, 1 );
        specialties[Heroes::CARLAWN] = makeSpellSpecialty( Spell::LIGHTNINGBOLT, 33, 1 );
        specialties[Heroes::BAROK] = makeSpellSpecialty( Spell::ARROW, 50, 0 );
        specialties[Heroes::KASTORE] = makeUnitSpecialty( { Monster::GREEN_DRAGON, Monster::RED_DRAGON, Monster::BLACK_DRAGON, Monster::AZURE_DRAGON,
                                                            Monster::BONE_DRAGON, Monster::BLOOD_DRAGON },
                                                          6, 5, 5, 1 );
        specialties[Heroes::MYRA] = makeUnitSpecialty( { Monster::BOAR, Monster::ROC }, 2, 3, 3, 1 );
        specialties[Heroes::FLINT] = makeSpellSpecialty( Spell::COLDRAY, 20, 1 );
        specialties[Heroes::HALON] = makeSpellSpecialty( Spell::PARALYZE, 0, 2 );
        specialties[Heroes::SARAKIN] = makeUnitSpecialty( { Monster::THOR, Monster::MAGE, Monster::ARCHMAGE, Monster::GIANT, Monster::TITAN }, 5, 2, 2, 1 );
        specialties[Heroes::KALINDRA] = makeSpellSpecialty( Spell::METEORSHOWER, 15, 4 );
        specialties[Heroes::MANDIGAL] = makeResourceSpecialty( Resource::GEMS, 1 );
        specialties[Heroes::RANLOO] = makeUnitSpecialty( { Monster::VAMPIRE, Monster::VAMPIRE_LORD, Monster::LICH, Monster::POWER_LICH }, 4, 4, 4, 1 );
        specialties[Heroes::ROXANA] = makeSpellSpecialty( Spell::ANIMATEDEAD, 20, 2 );
        specialties[Heroes::SANDRO] = makeSpellSpecialty( Spell::DEATHWAVE, 25, 5 );

        return specialties;
    }();

    const HeroSpecialty emptySpecialty;
}

bool HeroSpecialty::affectsMonster( const int32_t monsterId ) const
{
    return type == Type::UNIT && std::find( monsterIds.cbegin(), monsterIds.cbegin() + monsterCount, monsterId ) != monsterIds.cbegin() + monsterCount;
}

const HeroSpecialty & getHeroSpecialty( const int32_t heroId )
{
    if ( heroId < Heroes::UNKNOWN || heroId >= Heroes::HEROES_COUNT ) {
        return emptySpecialty;
    }

    return heroSpecialties[heroId];
}

int32_t getSpecialtyAttackBonus( const HeroSpecialty & specialty, const int32_t monsterId )
{
    return specialty.affectsMonster( monsterId ) ? specialty.attackBonus : 0;
}

int32_t getSpecialtyDefenseBonus( const HeroSpecialty & specialty, const int32_t monsterId )
{
    return specialty.affectsMonster( monsterId ) ? specialty.defenseBonus : 0;
}

int32_t getSpecialtySpeedBonus( const HeroSpecialty & specialty, const int32_t monsterId )
{
    return specialty.affectsMonster( monsterId ) ? specialty.speedBonus : 0;
}

int32_t getSpecialtySpellEffectivenessPercent( const HeroSpecialty & specialty, const int32_t spellId )
{
    return specialty.type == HeroSpecialty::Type::SPELL && specialty.spellId == spellId ? specialty.spellEffectivenessPercent : 0;
}

int32_t getSpecialtySpellPointCostReduction( const HeroSpecialty & specialty, const int32_t spellId )
{
    return specialty.type == HeroSpecialty::Type::SPELL && specialty.spellId == spellId ? specialty.spellPointCostReduction : 0;
}
