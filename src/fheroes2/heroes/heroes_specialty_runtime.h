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

#include <cstdint>
#include <string>

class HeroBase;
struct Funds;
struct HeroSpecialty;

const HeroSpecialty & getHeroSpecialty( const HeroBase * hero );

int32_t getSpecialtyAttackBonus( const HeroBase * hero, int32_t monsterId );
int32_t getSpecialtyDefenseBonus( const HeroBase * hero, int32_t monsterId );
int32_t getSpecialtySpeedBonus( const HeroBase * hero, int32_t monsterId );

int32_t getSpecialtySpellEffectivenessPercent( const HeroBase * hero, int32_t spellId );
int32_t getSpecialtySpellPointCostReduction( const HeroBase * hero, int32_t spellId );
int32_t getSpecialtySpellBookInclusion( const HeroBase * hero );

Funds getSpecialtyResourceBonus( const HeroBase * hero );

// Returns a localized description, or an empty string when this hero has no specialty.
std::string getSpecialtyDescription( const HeroBase * hero );
