/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
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
#include <cstdint>
#include <vector>

#include "icn.h"
#include "m82.h"
#include "monster.h"
#include "monster_info.h"
#include "race.h"
#include "speed.h"
#include "spell.h"
#include "translations.h"

namespace
{
    static_assert( Monster::RANDOM_MONSTER == 67 && Monster::RANDOM_MONSTER_LEVEL_4 == 71 && Monster::MONSTER_COUNT < Monster::CUSTOM_MONSTER_ID_BEGIN,
                   "Upstream monster IDs are part of the map and save compatibility contract." );
    static_assert( Monster::CUSTOM_MONSTER_ID_BEGIN == 0x00010000 && Monster::CUSTOM_MONSTER_ID_END == 0x00010006,
                   "Extended Edition monster IDs are persistent and must never be renumbered." );

    const std::vector<fheroes2::CustomMonsterDefinition> customMonsterDefinitions{
        { Monster::AZURE_DRAGON,
          67,
          "azure_dragon",
          Monster::BLACK_DRAGON,
          { ICN::DRAGBLAK,
            "DRAGBFRM.BIN",
            { M82::DRGNATTK, M82::DRGNKILL, M82::DRGNMOVE, M82::DRGNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 16,
              16,
              40,
              60,
              400,
              Speed::ULTRAFAST,
              0,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DRAGON ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_HEX_SIZE ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::FLYING ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::MAGIC_RESISTANCE, 100, 0 ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::TWO_CELL_MELEE_ATTACK ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::MORAL_DECREMENT, 100, 1 ) },
              {} },
            { gettext_noop( "Azure Dragon" ), gettext_noop( "Azure Dragons" ), 1, Race::WRLK, 6, { 6000, 0, 0, 0, 3, 0, 0 } } } },
        { Monster::BLOOD_DRAGON,
          68,
          "blood_dragon",
          Monster::BONE_DRAGON,
          { ICN::DRAGBONE,
            "DRABNFRM.BIN",
            { M82::BONEATTK, M82::BONEKILL, M82::BONEMOVE, M82::BONEWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 13,
              11,
              30,
              50,
              200,
              Speed::FAST,
              0,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DRAGON ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_HEX_SIZE ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::FLYING ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::UNDEAD ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::MORAL_DECREMENT, 100, 1 ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::HP_DRAIN ) },
              {} },
            { gettext_noop( "Blood Dragon" ), gettext_noop( "Blood Dragons" ), 1, Race::NECR, 6, { 3000, 0, 1, 0, 0, 0, 0 } } } },
        { Monster::THOR,
          69,
          "thor",
          Monster::TITAN,
          { ICN::TITANBLA,
            "TITA2FRM.BIN",
            { M82::TITNATTK, M82::TITNKILL, M82::TITNMOVE, M82::TITNWNCE, M82::TITNSHOT, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 16,
              16,
              30,
              40,
              300,
              Speed::ULTRAFAST,
              24,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::NO_MELEE_PENALTY ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::MIND_SPELL_IMMUNITY ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::HP_REGENERATION, 0, 40 ) },
              {} },
            { gettext_noop( "Thor" ), gettext_noop( "Thors" ), 1, Race::WZRD, 6, { 7000, 0, 0, 0, 0, 0, 3 } } } },
        { Monster::AVENGER,
          70,
          "avenger",
          Monster::CRUSADER,
          { ICN::PALADIN2,
            "PALADFRM.BIN",
            { M82::PLDNATTK, M82::PLDNKILL, M82::PLDNMOVE, M82::PLDNWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 15,
              15,
              20,
              35,
              100,
              Speed::ULTRAFAST,
              0,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_MELEE_ATTACK ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_DAMAGE_TO_UNDEAD ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_DAMAGE_TO_DRAGONS ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::IMMUNE_TO_CERTAIN_SPELL, 100, Spell::CURSE ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::IMMUNE_TO_CERTAIN_SPELL, 100, Spell::MASSCURSE ) },
              {} },
            { gettext_noop( "Avenger" ), gettext_noop( "Avengers" ), 2, Race::KNGT, 6, { 1500, 0, 0, 0, 0, 0, 0 } } } },
        { Monster::SUCCUBUS,
          71,
          "succubus",
          Monster::GARGOYLE,
          { ICN::GARGOYLE,
            "GARGLFRM.BIN",
            { M82::GARGATTK, M82::GARGKILL, M82::GARGMOVE, M82::GARGWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 13,
              12,
              20,
              30,
              250,
              Speed::ULTRAFAST,
              0,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::FLYING ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::SPELL_CASTER, 50, Spell::HYPNOTIZE ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::MAGIC_RESISTANCE, 25, 0 ) },
              {} },
            { gettext_noop( "Succubus" ), gettext_noop( "Succubi" ), 2, Race::BARB, 6, { 2500, 0, 0, 0, 0, 1, 0 } } } },
        { Monster::DACHSHUND,
          72,
          "dachshund",
          Monster::WOLF,
          { ICN::WOLF,
            "WOLF_FRM.BIN",
            { M82::WOLFATTK, M82::WOLFKILL, M82::WOLFMOVE, M82::WOLFWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 6,
              6,
              3,
              5,
              50,
              Speed::VERYFAST,
              0,
              0,
              { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_HEX_SIZE ), fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_MELEE_ATTACK ),
                fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::NO_ENEMY_RETALIATION ) },
              {} },
            { gettext_noop( "Dachshund" ), gettext_noop( "Dachshunds" ), 5, Race::BARB, 3, { 400, 0, 0, 0, 0, 0, 0 } } } },
        { Monster::MAID,
          73,
          "maid",
          Monster::PEASANT,
          { ICN::PEASANT,
            "PEAS_FRM.BIN",
            { M82::PSNTATTK, M82::PSNTKILL, M82::PSNTMOVE, M82::PSNTWNCE, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN, M82::UNKNOWN },
            { 2, 2, 2, 3, 5, Speed::AVERAGE, 0, 0, { fheroes2::MonsterAbility( fheroes2::MonsterAbilityType::DOUBLE_MELEE_ATTACK ) }, {} },
            { gettext_noop( "Maid" ), gettext_noop( "Maids" ), 10, Race::KNGT, 1, { 35, 0, 0, 0, 0, 0, 0 } } } },
    };
}

namespace fheroes2
{
    const std::vector<CustomMonsterDefinition> & getCustomMonsterDefinitions()
    {
        return customMonsterDefinitions;
    }

    const CustomMonsterDefinition * findCustomMonsterDefinition( const int32_t monsterId )
    {
        const auto iter = std::find_if( customMonsterDefinitions.cbegin(), customMonsterDefinitions.cend(),
                                        [monsterId]( const CustomMonsterDefinition & definition ) { return definition.id == monsterId; } );
        return iter != customMonsterDefinitions.cend() ? &( *iter ) : nullptr;
    }

    const CustomMonsterDefinition * findCustomMonsterDefinitionByLegacyFkId( const int32_t legacyMonsterId )
    {
        // FK/Azure-Dragon reused IDs now occupied by upstream random placeholders. Callers must opt into
        // legacy-FK interpretation; automatically remapping these values would corrupt normal FH2M maps.
        const auto iter = std::find_if( customMonsterDefinitions.cbegin(), customMonsterDefinitions.cend(),
                                        [legacyMonsterId]( const CustomMonsterDefinition & definition ) { return definition.legacyFkId == legacyMonsterId; } );
        return iter != customMonsterDefinitions.cend() ? &( *iter ) : nullptr;
    }

    bool isCustomMonsterId( const int32_t monsterId )
    {
        return findCustomMonsterDefinition( monsterId ) != nullptr;
    }

    int32_t getCustomMonsterFallbackId( const int32_t monsterId )
    {
        const CustomMonsterDefinition * definition = findCustomMonsterDefinition( monsterId );
        return definition != nullptr ? definition->fallbackMonsterId : monsterId;
    }
}
