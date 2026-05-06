/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#include "heroes_specialty_runtime.h"

#include "heroes.h"
#include "heroes_base.h"
#include "heroes_specialty.h"
#include "monster.h"
#include "resource.h"
#include "spell.h"

#include <algorithm>
#include <sstream>
#include <string>

namespace
{
    bool unitMatches( const HeroSpecialty & spec, const int monsterId )
    {
        return std::find( spec.monsterIds.begin(), spec.monsterIds.end(), monsterId ) != spec.monsterIds.end();
    }
}

const HeroSpecialty & getHeroSpecialty( const HeroBase * hero )
{
    if ( hero == nullptr || !hero->isHeroes() ) {
        // Pass an out-of-range id so the table getter returns the empty sentinel.
        return getHeroSpecialty( -1 );
    }
    const Heroes * heroes = static_cast<const Heroes *>( hero );
    return getHeroSpecialty( heroes->GetID() );
}

int getSpecialtyAtkBonus( const HeroBase * hero, const int monsterId )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::UNIT && unitMatches( s, monsterId ) ) ? s.atkBonus : 0;
}

int getSpecialtyDefBonus( const HeroBase * hero, const int monsterId )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::UNIT && unitMatches( s, monsterId ) ) ? s.defBonus : 0;
}

int getSpecialtySpeedBonus( const HeroBase * hero, const int monsterId )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::UNIT && unitMatches( s, monsterId ) ) ? s.speedBonus : 0;
}

int getSpecialtySpellDamageBonusPercent( const HeroBase * hero, const int spellId )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::SPELL && s.spellId == spellId ) ? s.damageBonusPercent : 0;
}

int getSpecialtySpellSpCostReduction( const HeroBase * hero, const int spellId )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::SPELL && s.spellId == spellId ) ? s.spCostReduction : 0;
}

int getSpecialtySpellBookInclusion( const HeroBase * hero )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    return ( s.kind == HeroSpecialty::SPELL ) ? s.spellId : 0;
}

Funds getSpecialtyResourceBonus( const HeroBase * hero )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    if ( s.kind != HeroSpecialty::RESOURCE || s.amountPerDay <= 0 ) {
        return Funds();
    }
    return Funds( s.resourceId, static_cast<uint32_t>( s.amountPerDay ) );
}

namespace
{
    const char * resourceName( const int resourceId )
    {
        switch ( resourceId ) {
        case Resource::WOOD: return "wood";
        case Resource::ORE: return "ore";
        case Resource::MERCURY: return "mercury";
        case Resource::SULFUR: return "sulfur";
        case Resource::CRYSTAL: return "crystal";
        case Resource::GEMS: return "gems";
        case Resource::GOLD: return "gold";
        default: return "?";
        }
    }

    void appendBonus( std::ostringstream & out, const int atk, const int def, const int speed )
    {
        out << " (";
        bool first = true;
        const auto put = [&out, &first]( const char * label, int value ) {
            if ( value == 0 ) return;
            if ( !first ) out << ", ";
            out << ( value > 0 ? "+" : "" ) << value << " " << label;
            first = false;
        };
        put( "atk", atk );
        put( "def", def );
        put( "spd", speed );
        if ( first ) {
            out << "no stat bonus";
        }
        out << ")";
    }
}

std::string getSpecialtyDescription( const HeroBase * hero )
{
    const HeroSpecialty & s = getHeroSpecialty( hero );
    std::ostringstream out;

    switch ( s.kind ) {
    case HeroSpecialty::SPELL: {
        out << Spell( s.spellId ).GetName();
        if ( s.damageBonusPercent != 0 || s.spCostReduction != 0 ) {
            out << ":";
            if ( s.damageBonusPercent != 0 ) {
                out << " " << ( s.damageBonusPercent > 0 ? "+" : "" ) << s.damageBonusPercent << "% dmg";
            }
            if ( s.spCostReduction != 0 ) {
                if ( s.damageBonusPercent != 0 ) out << ",";
                out << " -" << s.spCostReduction << " SP";
            }
        }
        break;
    }
    case HeroSpecialty::UNIT: {
        const size_t total = s.monsterIds.size();
        if ( total == 0 ) {
            return {};
        }
        // Up to 3 names inline; "and N more" beyond.
        const size_t inlineCount = std::min<size_t>( total, 3 );
        for ( size_t i = 0; i < inlineCount; ++i ) {
            if ( i > 0 ) out << ", ";
            out << Monster( s.monsterIds[i] ).GetName();
        }
        if ( total > inlineCount ) {
            out << " +" << ( total - inlineCount ) << " more";
        }
        appendBonus( out, s.atkBonus, s.defBonus, s.speedBonus );
        break;
    }
    case HeroSpecialty::RESOURCE: {
        if ( s.amountPerDay <= 0 ) return {};
        out << "+" << s.amountPerDay << " " << resourceName( s.resourceId ) << " / day";
        break;
    }
    case HeroSpecialty::NONE:
    default:
        return {};
    }

    return out.str();
}
