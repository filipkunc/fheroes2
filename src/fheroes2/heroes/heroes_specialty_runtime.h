/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 ***************************************************************************/

#pragma once

// Runtime helpers around the codegen'd HeroSpecialty table. Keeps the
// generated heroes_specialty.{h,cpp} pure data — anything hand-written goes
// here so the next codegen export doesn't clobber it.

class HeroBase;
struct HeroSpecialty;
struct Funds;

// Specialty lookup keyed on a HeroBase. Returns the empty specialty (kind=NONE)
// for null pointers or for non-Heroes hero types (Captain).
const HeroSpecialty & getHeroSpecialty( const HeroBase * hero );

// Per-monster bonus accessors. Each returns 0 if the hero has no UNIT
// specialty, or if the specialty doesn't list this monster. Negative bonuses
// are allowed (the UI permits them) and propagate through.
int getSpecialtyAtkBonus( const HeroBase * hero, int monsterId );
int getSpecialtyDefBonus( const HeroBase * hero, int monsterId );
int getSpecialtySpeedBonus( const HeroBase * hero, int monsterId );

// Per-spell modifiers. Each returns 0 if the hero has no SPELL specialty for
// the given spellId.
int getSpecialtySpellDamageBonusPercent( const HeroBase * hero, int spellId );
int getSpecialtySpellSpCostReduction( const HeroBase * hero, int spellId );

// The spell ID this hero is guaranteed to have in his spell book (Spell::NONE
// if no SPELL specialty). Drives the auto-add-to-spell-book contract documented
// in heroes_specialty.h.
int getSpecialtySpellBookInclusion( const HeroBase * hero );

// Per-day resource bonus. Returns an empty Funds if the hero has no RESOURCE
// specialty.
Funds getSpecialtyResourceBonus( const HeroBase * hero );

// Human-readable one-line description of the hero's specialty, e.g.
//   "Lightning Bolt: +33% damage, -1 SP"
//   "Dragons: +5/+5/+1"
//   "+1 wood / day"
// Returns an empty string if the hero has no specialty.
#include <string>
std::string getSpecialtyDescription( const HeroBase * hero );
