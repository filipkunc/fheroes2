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

#include "heroes_specialty_runtime.h"

#include <array>
#include <cassert>
#include <cstddef>
#include <string>

#include "heroes.h"
#include "heroes_base.h"
#include "heroes_specialty.h"
#include "monster.h"
#include "resource.h"
#include "spell.h"
#include "tools.h"
#include "translations.h"

const HeroSpecialty & getHeroSpecialty( const HeroBase * hero )
{
    if ( hero == nullptr || !hero->isHeroes() ) {
        return getHeroSpecialty( -1 );
    }

    return getHeroSpecialty( static_cast<const Heroes *>( hero )->GetID() );
}

int32_t getSpecialtyAttackBonus( const HeroBase * hero, const int32_t monsterId )
{
    return getSpecialtyAttackBonus( getHeroSpecialty( hero ), monsterId );
}

int32_t getSpecialtyDefenseBonus( const HeroBase * hero, const int32_t monsterId )
{
    return getSpecialtyDefenseBonus( getHeroSpecialty( hero ), monsterId );
}

int32_t getSpecialtySpeedBonus( const HeroBase * hero, const int32_t monsterId )
{
    return getSpecialtySpeedBonus( getHeroSpecialty( hero ), monsterId );
}

int32_t getSpecialtySpellEffectivenessPercent( const HeroBase * hero, const int32_t spellId )
{
    return getSpecialtySpellEffectivenessPercent( getHeroSpecialty( hero ), spellId );
}

int32_t getSpecialtySpellPointCostReduction( const HeroBase * hero, const int32_t spellId )
{
    return getSpecialtySpellPointCostReduction( getHeroSpecialty( hero ), spellId );
}

int32_t getSpecialtySpellBookInclusion( const HeroBase * hero )
{
    const HeroSpecialty & specialty = getHeroSpecialty( hero );
    return specialty.type == HeroSpecialty::Type::SPELL ? specialty.spellId : Spell::NONE;
}

Funds getSpecialtyResourceBonus( const HeroBase * hero )
{
    const HeroSpecialty & specialty = getHeroSpecialty( hero );
    if ( specialty.type != HeroSpecialty::Type::RESOURCE || specialty.resourceAmountPerDay <= 0 ) {
        return {};
    }

    return Funds( specialty.resourceId, static_cast<uint32_t>( specialty.resourceAmountPerDay ) );
}

std::string getSpecialtyDescription( const HeroBase * hero )
{
    const HeroSpecialty & specialty = getHeroSpecialty( hero );

    switch ( specialty.type ) {
    case HeroSpecialty::Type::SPELL: {
        std::string description = _( "Specializes in %{spell}." );
        StringReplace( description, "%{spell}", Spell( specialty.spellId ).GetName() );

        if ( specialty.spellEffectivenessPercent != 0 ) {
            std::string effect = _( "Spell effectiveness: %{value}%." );
            StringReplace( effect, "%{value}",
                           specialty.spellEffectivenessPercent > 0 ? "+" + std::to_string( specialty.spellEffectivenessPercent )
                                                                   : std::to_string( specialty.spellEffectivenessPercent ) );
            description += '\n';
            description += effect;
        }
        if ( specialty.spellPointCostReduction != 0 ) {
            std::string cost = _( "Spell point cost: -%{value}." );
            StringReplace( cost, "%{value}", specialty.spellPointCostReduction );
            description += '\n';
            description += cost;
        }

        return description;
    }
    case HeroSpecialty::Type::UNIT: {
        assert( specialty.monsterCount > 0 );

        std::string monsterNames;
        for ( std::size_t i = 0; i < specialty.monsterCount; ++i ) {
            if ( !monsterNames.empty() ) {
                monsterNames += ", ";
            }
            monsterNames += Monster( specialty.monsterIds[i] ).GetName();
        }

        std::string description = _( "Specializes in %{monsters}.\nAttack: %{attack}, Defense: %{defense}, Speed: %{speed}." );
        StringReplace( description, "%{monsters}", monsterNames );
        StringReplace( description, "%{attack}", specialty.attackBonus >= 0 ? "+" + std::to_string( specialty.attackBonus ) : std::to_string( specialty.attackBonus ) );
        StringReplace( description, "%{defense}",
                       specialty.defenseBonus >= 0 ? "+" + std::to_string( specialty.defenseBonus ) : std::to_string( specialty.defenseBonus ) );
        StringReplace( description, "%{speed}", specialty.speedBonus >= 0 ? "+" + std::to_string( specialty.speedBonus ) : std::to_string( specialty.speedBonus ) );
        return description;
    }
    case HeroSpecialty::Type::RESOURCE: {
        std::string description = _( "Produces %{count} %{resource} per day." );
        StringReplace( description, "%{count}", specialty.resourceAmountPerDay );
        StringReplace( description, "%{resource}", Resource::String( specialty.resourceId ) );
        return description;
    }
    case HeroSpecialty::Type::NONE:
    default:
        return {};
    }
}
