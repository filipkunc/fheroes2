/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2025                                             *
 *                                                                         *
 *   Free Heroes2 Engine: http://sourceforge.net/projects/fheroes2         *
 *   Copyright (C) 2009 by Andrey Afletdinov <fheroes2@gmail.com>          *
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

#include "army_troop.h"

#include <cassert>

#include "army.h"
#include "color.h"
#include "heroes_base.h"
#include "heroes_specialty_runtime.h"
#include "resource.h"
#include "serialize.h"
#include "speed.h"

bool Troop::isMonster( const int mons ) const
{
    return GetID() == mons;
}

void Troop::Set( const Troop & troop )
{
    SetMonster( troop.GetMonster() );
    SetCount( troop.GetCount() );
}

void Troop::Set( const Monster & mons, const uint32_t count )
{
    Set( Troop( mons, count ) );
}

void Troop::SetMonster( const Monster & mons )
{
    id = mons.GetID();
}

const char * Troop::GetName() const
{
    return Monster::GetPluralName( _count );
}

uint32_t Troop::GetHitPoints() const
{
    return Monster::GetHitPoints() * _count;
}

uint32_t Troop::GetDamageMin() const
{
    return Monster::GetDamageMin() * _count;
}

uint32_t Troop::GetDamageMax() const
{
    return Monster::GetDamageMax() * _count;
}

double Troop::GetStrength() const
{
    return Monster::GetMonsterStrength() * _count;
}

double Troop::GetStrengthWithBonus( const int bonusAttack, const int bonusDefense ) const
{
    assert( bonusAttack >= 0 && bonusDefense >= 0 );

    return Monster::GetMonsterStrength( static_cast<int>( Monster::GetAttack() ) + bonusAttack, static_cast<int>( Monster::GetDefense() ) + bonusDefense ) * _count;
}

bool Troop::isValid() const
{
    return Monster::isValid() && _count;
}

Funds Troop::GetTotalCost() const
{
    return GetCost() * _count;
}

Funds Troop::GetTotalUpgradeCost() const
{
    return GetUpgradeCost() * _count;
}

bool Troop::isBattle() const
{
    return false;
}

bool Troop::isModes( const uint32_t /* unused */ ) const
{
    return false;
}

std::string Troop::GetAttackString() const
{
    return std::to_string( GetAttack() );
}

std::string Troop::GetDefenseString() const
{
    return std::to_string( GetDefense() );
}

std::string Troop::GetShotString() const
{
    return std::to_string( GetShots() );
}

std::string Troop::GetSpeedString() const
{
    return GetSpeedString( GetSpeed() );
}

std::string Troop::GetSpeedString( const uint32_t speed )
{
    std::string output( Speed::String( speed ) );
    output += " (";
    output += std::to_string( speed );
    output += ')';

    return output;
}

uint32_t Troop::GetHitPointsLeft() const
{
    return 0;
}

uint32_t Troop::GetSpeed() const
{
    return Monster::GetSpeed();
}

uint32_t Troop::GetAffectedDuration( uint32_t /* unused */ ) const
{
    return 0;
}

uint32_t ArmyTroop::GetAttack() const
{
    int value = static_cast<int>( Troop::GetAttack() );
    if ( const HeroBase * commander = _army ? _army->GetCommander() : nullptr ) {
        value += static_cast<int>( commander->GetAttack() );
        // Hero specialty: per-unit attack bonus stacks on top of the commander's base attack.
        value += getSpecialtyAtkBonus( commander, GetID() );
    }
    if ( value < 0 ) {
        value = 0;
    }
    return static_cast<uint32_t>( value );
}

uint32_t ArmyTroop::GetDefense() const
{
    int value = static_cast<int>( Troop::GetDefense() );
    if ( const HeroBase * commander = _army ? _army->GetCommander() : nullptr ) {
        value += static_cast<int>( commander->GetDefense() );
        value += getSpecialtyDefBonus( commander, GetID() );
    }
    if ( value < 0 ) {
        value = 0;
    }
    return static_cast<uint32_t>( value );
}

uint32_t ArmyTroop::GetSpeed() const
{
    // Hero specialty speed bonus is reflected in the army info dialog (and any
    // other non-battle context that calls Troop::GetSpeed via dynamic dispatch).
    // Battle::Unit::GetSpeed has its own override that bypasses this path and
    // applies the bonus directly on Monster::GetSpeed, so there's no double-count.
    int value = static_cast<int>( Troop::GetSpeed() );
    if ( const HeroBase * commander = _army ? _army->GetCommander() : nullptr ) {
        value += getSpecialtySpeedBonus( commander, GetID() );
    }
    if ( value < 0 ) {
        value = 0;
    }
    return static_cast<uint32_t>( value );
}

PlayerColor ArmyTroop::GetColor() const
{
    return _army ? _army->GetColor() : PlayerColor::NONE;
}

int ArmyTroop::GetMorale() const
{
    return _army && isAffectedByMorale() ? _army->GetMorale() : Troop::GetMorale();
}

int ArmyTroop::GetLuck() const
{
    return _army ? _army->GetLuck() : Troop::GetLuck();
}

std::string ArmyTroop::GetAttackString() const
{
    if ( Troop::GetAttack() == GetAttack() ) {
        return std::to_string( Troop::GetAttack() );
    }

    std::string output( std::to_string( Troop::GetAttack() ) );
    output += " (";
    output += std::to_string( GetAttack() );
    output += ')';

    return output;
}

std::string ArmyTroop::GetDefenseString() const
{
    if ( Troop::GetDefense() == GetDefense() ) {
        return std::to_string( Troop::GetDefense() );
    }

    std::string output( std::to_string( Troop::GetDefense() ) );
    output += " (";
    output += std::to_string( GetDefense() );
    output += ')';

    return output;
}

OStreamBase & operator<<( OStreamBase & stream, const Troop & troop )
{
    return stream << troop.id << troop._count;
}

IStreamBase & operator>>( IStreamBase & stream, Troop & troop )
{
    return stream >> troop.id >> troop._count;
}
