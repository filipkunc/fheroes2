/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2019 - 2026                                             *
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

#pragma once

#include <array>
#include <cstddef>
#include <cstdint>
#include <functional>
#include <map>
#include <memory>
#include <set>
#include <string>
#include <utility>
#include <vector>

#include "army.h"
#include "bitmodes.h"
#include "captain.h"
#include "color.h"
#include "mageguild.h"
#include "math_base.h"
#include "monster.h"
#include "players.h"
#include "position.h"
#include "race.h"

class IStreamBase;
class OStreamBase;

class HeroBase;
class Heroes;
class Troop;

struct Funds;

namespace fheroes2
{
    class RandomMonsterAnimation;
    class Image;
}

namespace Maps::Map_Format
{
    struct CastleMetadata;
}

enum BuildingType : uint64_t
{
    BUILD_NOTHING = 0x0000000000000000ULL,
    BUILD_THIEVESGUILD = 0x0000000000000001ULL,
    BUILD_TAVERN = 0x0000000000000002ULL,
    BUILD_SHIPYARD = 0x0000000000000004ULL,
    BUILD_WELL = 0x0000000000000008ULL,
    BUILD_STATUE = 0x0000000000000010ULL,
    BUILD_LEFTTURRET = 0x0000000000000020ULL,
    BUILD_RIGHTTURRET = 0x0000000000000040ULL,
    BUILD_MARKETPLACE = 0x0000000000000080ULL,
    // Farm, Garbage Heap, Crystal Garden, Waterfall, Orchard, Skull Pile
    BUILD_WEL2 = 0x0000000000000100ULL,
    BUILD_MOAT = 0x0000000000000200ULL,
    // Fortifications, Coliseum, Rainbow, Dungeon, Library, Storm
    BUILD_SPEC = 0x0000000000000400ULL,
    BUILD_CASTLE = 0x0000000000000800ULL,
    BUILD_CAPTAIN = 0x0000000000001000ULL,
    BUILD_SHRINE = 0x0000000000002000ULL,
    BUILD_MAGEGUILD1 = 0x0000000000004000ULL,
    BUILD_MAGEGUILD2 = 0x0000000000008000ULL,
    BUILD_MAGEGUILD3 = 0x0000000000010000ULL,
    BUILD_MAGEGUILD4 = 0x0000000000020000ULL,
    BUILD_MAGEGUILD5 = 0x0000000000040000ULL,
    BUILD_MAGEGUILD = BUILD_MAGEGUILD1 | BUILD_MAGEGUILD2 | BUILD_MAGEGUILD3 | BUILD_MAGEGUILD4 | BUILD_MAGEGUILD5,
    BUILD_TENT = 0x0000000000080000ULL,
    DWELLING_MONSTER1 = 0x0000000000100000ULL,
    DWELLING_MONSTER2 = 0x0000000000200000ULL,
    DWELLING_MONSTER3 = 0x0000000000400000ULL,
    DWELLING_MONSTER4 = 0x0000000000800000ULL,
    DWELLING_MONSTER5 = 0x0000000001000000ULL,
    DWELLING_MONSTER6 = 0x0000000002000000ULL,
    DWELLING_MONSTERS = DWELLING_MONSTER1 | DWELLING_MONSTER2 | DWELLING_MONSTER3 | DWELLING_MONSTER4 | DWELLING_MONSTER5 | DWELLING_MONSTER6,
    DWELLING_UPGRADE2 = 0x0000000004000000ULL,
    DWELLING_UPGRADE3 = 0x0000000008000000ULL,
    DWELLING_UPGRADE4 = 0x0000000010000000ULL,
    DWELLING_UPGRADE5 = 0x0000000020000000ULL,
    DWELLING_UPGRADE6 = 0x0000000040000000ULL,
    // Black Dragons
    DWELLING_UPGRADE7 = 0x0000000080000000ULL,
    // Azure Dragons
    DWELLING_UPGRADE8 = 0x0000000100000000ULL,
    // Blood Dragons
    DWELLING_UPGRADE9 = 0x0000000200000000ULL,
    // Thor
    DWELLING_UPGRADE10 = 0x0000000400000000ULL,
    // Avenger
    DWELLING_UPGRADE11 = 0x0000000800000000ULL,
    // Succubus
    DWELLING_UPGRADE12 = 0x0000001000000000ULL,
    // Dachshund
    DWELLING_UPGRADE13 = 0x0000002000000000ULL,
    // Maid
    DWELLING_UPGRADE14 = 0x0000004000000000ULL,
    DWELLING_UPGRADES = DWELLING_UPGRADE2 | DWELLING_UPGRADE3 | DWELLING_UPGRADE4 | DWELLING_UPGRADE5 | DWELLING_UPGRADE6 | DWELLING_UPGRADE7 | DWELLING_UPGRADE8 | DWELLING_UPGRADE9
                        | DWELLING_UPGRADE10 | DWELLING_UPGRADE11 | DWELLING_UPGRADE12 | DWELLING_UPGRADE13 | DWELLING_UPGRADE14
};

enum class BuildingStatus : int32_t
{
    UNKNOWN_COND,
    ALLOW_BUILD,
    NOT_TODAY,
    ALREADY_BUILT,
    NEED_CASTLE,
    BUILD_DISABLE,
    SHIPYARD_NOT_ALLOWED,
    UNKNOWN_UPGRADE,
    REQUIRES_BUILD,
    LACK_RESOURCES
};

class Castle final : public MapPosition, public BitModes, public ColorBase, public Control
{
public:
    // Maximum number of creature dwellings that can be built in a castle
    static constexpr int maxNumOfDwellings{ 6 };

    enum : uint32_t
    {
        UNUSED_ALLOW_CASTLE_CONSTRUCTION = ( 1 << 1 ),
        CUSTOM_ARMY = ( 1 << 2 ),
        ALLOW_TO_BUILD_TODAY = ( 1 << 3 )
    };

    enum class CastleDialogReturnValue : int
    {
        DoNothing,
        Close, // Close the dialog.
        NextCastle, // Open main dialog of the next castle.
        PreviousCastle, // Open main dialog of the previous castle.
        NextConstructionWindow, // Open construction dialog of the next castle.
        PreviousConstructionWindow, // Open construction dialog of the previous castle.
        NextMageGuildWindow, // Open Mage Guild dialog of the next castle.
        PreviousMageGuildWindow, // Open Mage Guild dialog of the previous castle.
    };

    Castle() = default;
    Castle( const int32_t posX, const int32_t posY, const int race )
        : MapPosition( { posX, posY } )
        , _race( race )
    {
        // Do nothing.
    }

    Castle( const Castle & ) = delete;

    ~Castle() override = default;

    Castle & operator=( const Castle & ) = delete;

    void LoadFromMP2( const std::vector<uint8_t> & data );

    void loadFromResurrectionMap( const Maps::Map_Format::CastleMetadata & metadata );

    Captain & GetCaptain()
    {
        return _captain;
    }

    const Captain & GetCaptain() const
    {
        return _captain;
    }

    bool isCastle() const
    {
        return ( _constructedBuildings & BUILD_CASTLE ) != 0;
    }

    bool HasSeaAccess() const;
    bool HasBoatNearby() const;

    // Returns a tile ID where it is possible to place a boat or -1 if it is not.
    int32_t getTileIndexToPlaceBoat() const;

    bool AllowBuyHero( std::string * msg = nullptr ) const;
    bool isPosition( const fheroes2::Point & pt ) const override;
    bool isNecromancyShrineBuild() const
    {
        return _race == Race::NECR && ( BUILD_SHRINE & _constructedBuildings );
    }

    uint32_t CountBuildings() const;

    Heroes * RecruitHero( Heroes * hero );
    Heroes * GetHero() const;

    int GetRace() const
    {
        return _race;
    }

    const std::string & GetName() const
    {
        return _name;
    }

    // This method must be called only at the time of map loading and only for castles with empty names.
    void setName( const std::set<std::string, std::less<>> & usedNames );

    int GetControl() const override;

    int GetLevelMageGuild() const;

    const MageGuild & GetMageGuild() const
    {
        return _mageGuild;
    }

    bool HaveLibraryCapability() const
    {
        return _race == Race::WZRD;
    }

    bool isLibraryBuilt() const
    {
        return _race == Race::WZRD && isBuild( BUILD_SPEC );
    }

    void trainHeroInMageGuild( HeroBase & hero ) const
    {
        _mageGuild.trainHero( hero, GetLevelMageGuild(), isLibraryBuilt() );
    }

    bool isFortificationBuilt() const
    {
        return _race == Race::KNGT && isBuild( BUILD_SPEC );
    }

    const Army & GetArmy() const
    {
        return _army;
    }

    Army & GetArmy()
    {
        return _army;
    }

    const Army & GetActualArmy() const;
    Army & GetActualArmy();

    // Returns current monsters count in dwelling.
    uint32_t getMonstersInDwelling( const uint64_t buildingType ) const;

    // Returns the garrison strength estimation calculated as if this castle had really been attacked, including
    // an estimate of the strength of the combined army consisting of the garrison and the guest hero's troops
    // (if present), castle-specific bonuses from moat, towers and so on, relative to the attacking hero's abilities
    // (if the 'attackingHero' is not 'nullptr'). See the implementation for details.
    double GetGarrisonStrength( const Heroes * attackingHero ) const;
    double GetGarrisonStrength( const Heroes & attackingHero ) const
    {
        return GetGarrisonStrength( &attackingHero );
    }

    // Returns the correct dwelling type available in the castle. BUILD_NOTHING is returned if this is not a dwelling.
    uint64_t GetActualDwelling( const uint64_t buildId ) const;

    // Returns true in case of successful recruitment.
    bool RecruitMonster( const Troop & troop, bool showDialog = true );

    uint32_t getRecruitLimit( const Monster & monster, const Funds & budget ) const;

    int getBuildingValue() const;

    // Used only for AI.
    double getArmyRecruitmentValue() const;
    double getVisitValue( const Heroes & hero ) const;

    void ChangeColor( const PlayerColor newColor );

    void ActionNewDay();
    void ActionNewWeek();

    void ActionPreBattle();
    void ActionAfterBattle( const bool attackerWins );

    void DrawImageCastle( const fheroes2::Point & pt ) const;

    CastleDialogReturnValue OpenDialog( const bool openConstructionWindow, const bool openMageGuildWindow, const bool fade, const bool renderBackgroundDialog );

    int GetAttackModificator( const std::string * /* unused */ ) const
    {
        return 0;
    }

    int GetDefenseModificator( const std::string * /* unused */ ) const
    {
        return 0;
    }

    int GetPowerModificator( std::string * strs ) const;

    int GetKnowledgeModificator( const std::string * /* unused */ ) const
    {
        return 0;
    }

    int GetMoraleModificator( std::string * strs ) const;
    int GetLuckModificator( std::string * strs ) const;

    bool AllowBuyBuilding( const uint64_t buildingType ) const
    {
        return BuildingStatus::ALLOW_BUILD == CheckBuyBuilding( buildingType );
    }

    bool isBuild( const uint64_t buildingType ) const
    {
        return ( _constructedBuildings & buildingType ) != 0;
    }

    bool BuyBuilding( const uint64_t buildingType );

    BuildingStatus CheckBuyBuilding( const uint64_t build ) const;
    static BuildingStatus GetAllBuildingStatus( const Castle & castle );

    bool AllowBuyBoat( const bool checkPayment ) const;
    bool BuyBoat() const;

    void Scout() const;

    std::string GetStringBuilding( const uint64_t buildingType ) const
    {
        return GetStringBuilding( buildingType, _race );
    }

    std::string GetDescriptionBuilding( const uint64_t buildingType ) const;

    static const char * GetStringBuilding( const uint64_t buildingType, const int race );

    // Get building ICN ID for given race and building type.
    static int GetICNBuilding( const uint64_t buildingType, const int race );
    static int GetICNBoat( const int race );
    uint64_t GetUpgradeBuilding( const uint64_t buildingId ) const;

    static bool PredicateIsCastle( const Castle * castle )
    {
        return castle && castle->isCastle();
    }

    static bool PredicateIsTown( const Castle * castle )
    {
        return castle && !castle->isCastle();
    }

    static bool PredicateIsBuildBuilding( const Castle * castle, const uint64_t buildingType )
    {
        return castle && castle->isBuild( buildingType );
    }

    static uint32_t GetGrownWell();
    static uint32_t GetGrownWel2();
    static uint32_t GetGrownWeekOf();
    static uint32_t GetGrownMonthOf();

    std::string String() const;

    int DialogBuyHero( const Heroes * hero ) const;
    int DialogBuyCastle( const bool hasButtons = true ) const;

    Troops getAvailableArmy( Funds potentialBudget ) const;

    bool isBuildingDisabled( const uint64_t buildingType ) const
    {
        return ( _disabledBuildings & buildingType ) != 0;
    }

    // Update French language-specific characters to match CP1252.
    // Call this method only when loading maps made with original French editor.
    void fixFrenchCharactersInName();

private:
    enum class ConstructionDialogResult : int
    {
        DoNothing,
        NextConstructionWindow, // Open construction dialog for the next castle.
        PrevConstructionWindow, // Open construction dialog for the previous castle.
        Build, // Build something.
        RecruitHero // Recruit a hero.
    };

    enum class MageGuildDialogResult : uint8_t
    {
        DoNothing,
        NextMageGuildWindow, // Open Mage Guild dialog for the next castle.
        PrevMageGuildWindow, // Open Mage Guild dialog for the previous castle.
    };

    // Checks whether this particular building is currently built in the castle (unlike
    // the isBuild(), upgraded versions of the same building are not taken into account)
    bool _isExactBuildingBuilt( const uint64_t buildingToCheck ) const;

    uint32_t * _getDwelling( const uint64_t buildingType );
    void _trainGuestHeroAndCaptainInMageGuild();

    ConstructionDialogResult _openConstructionDialog( uint64_t & dwellingTobuild );

    void _openTavern() const;
    void _openWell();
    MageGuildDialogResult _openMageGuild( const Heroes * hero ) const;
    void _joinRNDArmy();
    void _postLoad();

    void _wellRedrawAvailableMonsters( const uint64_t dwellingType, const bool restoreBackground, fheroes2::Image & background ) const;
    void _wellRedrawBackground( fheroes2::Image & background ) const;
    void _wellRedrawMonsterAnimation( const fheroes2::Rect & roi, std::array<fheroes2::RandomMonsterAnimation, maxNumOfDwellings> & monsterAnimInfo ) const;

    void _setDefaultBuildings();

    // Recruit maximum monsters from the castle. Returns 'true' if the recruit was made.
    bool _recruitCastleMax( const Troops & currentCastleArmy );

    friend OStreamBase & operator<<( OStreamBase & stream, const Castle & castle );
    friend IStreamBase & operator>>( IStreamBase & stream, Castle & castle );

    int32_t _race{ Race::NONE };
    uint64_t _constructedBuildings{ 0 };
    uint64_t _disabledBuildings{ 0 };

    Captain _captain{ *this };

    std::string _name;

    MageGuild _mageGuild;
    std::array<uint32_t, maxNumOfDwellings> _dwelling{ 0 };
    Army _army{ &_captain };
};

namespace CastleDialog
{
    // Class used for fading animation
    class FadeBuilding
    {
    public:
        FadeBuilding() = default;

        void startFadeBuilding( const uint64_t building )
        {
            _alpha = 0;
            _building = building;
            _isOnlyBoat = false;
        }

        void startFadeBoat()
        {
            _alpha = 0;
            _building = BUILD_SHIPYARD;
            _isOnlyBoat = true;
        }

        bool updateFadeAlpha();

        bool isFadeDone() const
        {
            return _alpha == 255;
        }

        void stopFade()
        {
            _alpha = 255;
            _building = BUILD_NOTHING;
            _isOnlyBoat = false;
        }

        uint8_t getAlpha() const
        {
            return _alpha;
        }

        uint64_t getBuilding() const
        {
            return _building;
        }

        bool isOnlyBoat() const
        {
            return _isOnlyBoat;
        }

    private:
        uint64_t _building{ BUILD_NOTHING };
        uint8_t _alpha{ 255 };
        bool _isOnlyBoat{ false };
    };

    struct BuildingRenderInfo final
    {
        BuildingRenderInfo( const BuildingType buildingType, std::vector<fheroes2::Rect> buildingAreas )
            : id( buildingType )
            , areas( std::move( buildingAreas ) )
        {
            // Do nothing.
        }

        bool operator==( const uint64_t buildingType ) const
        {
            return buildingType == static_cast<uint64_t>( id );
        }

        BuildingType id{ BUILD_NOTHING };
        std::vector<fheroes2::Rect> areas;
    };

    struct BuildingsRenderQueue : std::vector<BuildingRenderInfo>
    {
        BuildingsRenderQueue( const Castle & castle, const fheroes2::Point & top );
    };

    void redrawAllBuildings( const Castle & castle, const fheroes2::Point & offset, const BuildingsRenderQueue & buildings,
                             const CastleDialog::FadeBuilding & alphaBuilding, const uint32_t animationIndex );
}

struct VecCastles : public std::vector<Castle *>
{
    VecCastles() = default;

    Castle * GetFirstCastle() const;
};

class AllCastles
{
public:
    AllCastles();
    AllCastles( const AllCastles & ) = delete;

    ~AllCastles() = default;

    AllCastles & operator=( const AllCastles & ) = delete;

    auto begin() const noexcept
    {
        return Iterator( _castles.begin() );
    }

    auto end() const noexcept
    {
        return Iterator( _castles.end() );
    }

    void Init()
    {
        Clear();
    }

    void Clear()
    {
        _castles.clear();
        _castleTiles.clear();
    }

    size_t Size() const
    {
        return _castles.size();
    }

    // Return the maximum allowed castles and towns on map limited by the count of castle default names.
    static size_t getMaximumAllowedCastles();

    void AddCastle( std::unique_ptr<Castle> && castle );

    Castle * Get( const fheroes2::Point & position ) const;

    void removeCastle( const fheroes2::Point & position );

    void Scout( const PlayerColorsSet colors ) const;

    void NewDay() const;
    void NewWeek() const;

    template <typename BaseIterator>
    struct Iterator : public BaseIterator
    {
        explicit Iterator( BaseIterator && other ) noexcept
            : BaseIterator( std::move( other ) )
        {}

        auto * operator*() const noexcept
        {
            return BaseIterator::operator*().get();
        }
    };

private:
    std::vector<std::unique_ptr<Castle>> _castles;
    std::map<fheroes2::Point, size_t> _castleTiles;
};

OStreamBase & operator<<( OStreamBase & stream, const VecCastles & castles );
IStreamBase & operator>>( IStreamBase & stream, VecCastles & castles );

OStreamBase & operator<<( OStreamBase & stream, const AllCastles & castles );
IStreamBase & operator>>( IStreamBase & stream, AllCastles & castles );
