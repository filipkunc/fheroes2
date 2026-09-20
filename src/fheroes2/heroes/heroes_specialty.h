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

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

struct HeroSpecialty
{
    enum class Type : uint8_t
    {
        NONE,
        SPELL,
        UNIT,
        RESOURCE
    };

    Type type{ Type::NONE };

    int32_t spellId{ 0 };
    int32_t spellEffectivenessPercent{ 0 };
    int32_t spellPointCostReduction{ 0 };

    std::array<int32_t, 6> monsterIds{};
    size_t monsterCount{ 0 };
    int32_t attackBonus{ 0 };
    int32_t defenseBonus{ 0 };
    int32_t speedBonus{ 0 };

    int32_t resourceId{ 0 };
    int32_t resourceAmountPerDay{ 0 };

    bool affectsMonster( int32_t monsterId ) const;
};

// Returns an empty specialty for invalid hero IDs.
const HeroSpecialty & getHeroSpecialty( int32_t heroId );

int32_t getSpecialtyAttackBonus( const HeroSpecialty & specialty, int32_t monsterId );
int32_t getSpecialtyDefenseBonus( const HeroSpecialty & specialty, int32_t monsterId );
int32_t getSpecialtySpeedBonus( const HeroSpecialty & specialty, int32_t monsterId );
int32_t getSpecialtySpellEffectivenessPercent( const HeroSpecialty & specialty, int32_t spellId );
int32_t getSpecialtySpellPointCostReduction( const HeroSpecialty & specialty, int32_t spellId );
