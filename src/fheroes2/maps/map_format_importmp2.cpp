/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2025 - 2026                                             *
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

#include "map_format_importmp2.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <iterator>
#include <list>
#include <map>
#include <ostream>
#include <string>
#include <tuple>
#include <utility>
#include <vector>

#include "artifact.h"
#include "castle.h"
#include "color.h"
#include "game_language.h"
#include "heroes.h"
#include "logging.h"
#include "map_format_helper.h"
#include "map_format_importmp2_metadata.h"
#include "map_format_importmp2_validation.h"
#include "map_format_info.h"
#include "map_object_info.h"
#include "maps.h"
#include "maps_fileinfo.h"
#include "maps_tiles.h"
#include "maps_tiles_helper.h"
#include "mp2.h"
#include "players.h"
#include "race.h"
#include "resource.h"
#include "settings.h"
#include "tools.h"
#include "world.h"
#include "world_object_uid.h"

namespace
{
    Maps::Map_Format::DailyEvent convertDailyEvent( const EventDate & event )
    {
        Maps::Map_Format::DailyEvent result;
        result.message = event.message;
        result.humanPlayerColors = event.colors;
        result.computerPlayerColors = event.isApplicableForAIPlayers ? event.colors : 0;
        result.firstOccurrenceDay = event.firstOccurrenceDay;
        result.repeatPeriodInDays = event.repeatPeriodInDays;
        result.resources = event.resource;

        return result;
    }

    void preserveDerivedHeroMetadata( const Heroes & hero, const bool hasCustomArmy, Maps::Map_Format::HeroMetadata & metadata )
    {
        const bool hasEmptyCustomArmy = hasCustomArmy && std::none_of( metadata.armyMonsterType.cbegin(), metadata.armyMonsterType.cend(), []( const int32_t monsterId ) {
                                            return monsterId > 0;
                                        } );
        const bool needsArtifactMetadata = std::find( metadata.artifact.cbegin(), metadata.artifact.cend(), Artifact::SPELL_SCROLL ) != metadata.artifact.cend();
        const bool needsSpells = std::find( metadata.artifact.cbegin(), metadata.artifact.cend(), Artifact::MAGIC_BOOK ) != metadata.artifact.cend();
        if ( !hasEmptyCustomArmy && !needsArtifactMetadata && !needsSpells ) {
            return;
        }

        const Maps::Map_Format::HeroMetadata runtimeMetadata = hero.getHeroMetadata();
        if ( hasEmptyCustomArmy ) {
            metadata.armyMonsterType = runtimeMetadata.armyMonsterType;
            metadata.armyMonsterCount = runtimeMetadata.armyMonsterCount;
        }
        if ( needsArtifactMetadata ) {
            auto runtimeArtifact = runtimeMetadata.artifact.cbegin();
            for ( size_t i = 0; i < metadata.artifact.size(); ++i ) {
                if ( metadata.artifact[i] != Artifact::SPELL_SCROLL ) {
                    continue;
                }

                runtimeArtifact = std::find( runtimeArtifact, runtimeMetadata.artifact.cend(), Artifact::SPELL_SCROLL );
                if ( runtimeArtifact == runtimeMetadata.artifact.cend() ) {
                    break;
                }

                const size_t runtimeIndex = static_cast<size_t>( std::distance( runtimeMetadata.artifact.cbegin(), runtimeArtifact ) );
                metadata.artifactMetadata[i] = runtimeMetadata.artifactMetadata[runtimeIndex];
                ++runtimeArtifact;
            }
        }
        if ( needsSpells ) {
            metadata.availableSpells = runtimeMetadata.availableSpells;
        }
    }

    bool getUniqueEditorObjectByType( const MP2::MapObjectType objectType, Maps::ObjectGroup & group, uint32_t & objectIndex )
    {
        bool isFound = false;
        for ( uint32_t groupIndex = 0; groupIndex < static_cast<uint32_t>( Maps::ObjectGroup::GROUP_COUNT ); ++groupIndex ) {
            const auto currentGroup = static_cast<Maps::ObjectGroup>( groupIndex );
            const auto & objects = Maps::getObjectsByGroup( currentGroup );
            for ( size_t index = 0; index < objects.size(); ++index ) {
                if ( objects[index].objectType != objectType ) {
                    continue;
                }

                if ( isFound ) {
                    return false;
                }

                group = currentGroup;
                objectIndex = static_cast<uint32_t>( index );
                isFound = true;
            }
        }

        return isFound;
    }
}

namespace Maps::Map_Format
{
    bool importMP2Map( const std::string & filePath, MapFormat & mapFormat )
    {
        // Step 1: read the map information and initialize the player configuration expected by the original map loader.
        Maps::FileInfo fi;
        if ( !fi.readMP2Map( filePath, true ) ) {
            return false;
        }

        Settings & settings = Settings::Get();
        settings.setCurrentMapInfo( fi );
        settings.GetPlayers().SetStartGame();

        const std::string lowerPath = StringLower( filePath );
        const bool isOriginalMp2 = lowerPath.size() >= 4 && lowerPath.substr( lowerPath.size() - 4 ) == ".mp2";

        // Step 2: load the MP2/MX2 file into the World.
        World & mapWorld = World::Get();
        MP2MapImportInfo importInfo;
        if ( !mapWorld.LoadMapMP2( filePath, isOriginalMp2, &importInfo ) ) {
            return false;
        }

        mapFormat = MapFormat{};

        mapFormat.name = fi.name;
        mapFormat.description = fi.description;
        mapFormat.difficulty = fi.difficulty;
        mapFormat.mainLanguage = fheroes2::SupportedLanguage::English;

        mapFormat.victoryConditionType = fi.victoryConditionType;
        mapFormat.isVictoryConditionApplicableForAI = fi.compAlsoWins;
        mapFormat.allowNormalVictory = fi.allowNormalVictory;

        // Populate victory condition metadata in the format that loadResurrectionMap expects.
        // The MP2 params[0] is x (or artifact ID / gold/1000), params[1] is y.
        switch ( fi.victoryConditionType ) {
        case Maps::FileInfo::VICTORY_DEFEAT_EVERYONE:
            // No metadata.
            break;
        case Maps::FileInfo::VICTORY_CAPTURE_TOWN: {
            const uint32_t tileIndex = static_cast<uint32_t>( fi.victoryConditionParams[0] ) + static_cast<uint32_t>( fi.victoryConditionParams[1] ) * fi.width;
            mapFormat.victoryConditionMetadata.push_back( tileIndex );

            const Castle * castle = mapWorld.getCastle( Maps::GetPoint( static_cast<int32_t>( tileIndex ) ) );
            mapFormat.victoryConditionMetadata.push_back( castle ? static_cast<uint32_t>( castle->GetColor() ) : 0 );
            break;
        }
        case Maps::FileInfo::VICTORY_KILL_HERO: {
            const uint32_t tileIndex = static_cast<uint32_t>( fi.victoryConditionParams[0] ) + static_cast<uint32_t>( fi.victoryConditionParams[1] ) * fi.width;
            mapFormat.victoryConditionMetadata.push_back( tileIndex );

            const Heroes * hero = mapWorld.GetHeroes( Maps::GetPoint( static_cast<int32_t>( tileIndex ) ) );
            mapFormat.victoryConditionMetadata.push_back( hero ? static_cast<uint32_t>( hero->GetColor() ) : 0 );
            break;
        }
        case Maps::FileInfo::VICTORY_OBTAIN_ARTIFACT:
            // metadata[0] = artifact ID
            mapFormat.victoryConditionMetadata.push_back( fi.victoryConditionParams[0] );
            break;
        case Maps::FileInfo::VICTORY_COLLECT_ENOUGH_GOLD:
            // metadata[0] = gold amount (params[0] is gold/1000 in MP2 format)
            mapFormat.victoryConditionMetadata.push_back( static_cast<uint32_t>( fi.victoryConditionParams[0] ) * 1000 );
            break;
        case Maps::FileInfo::VICTORY_DEFEAT_OTHER_SIDE:
            // Alliances are stored in fi.unions — extract the two distinct sides.
            {
                PlayerColorsSet side1 = 0;
                PlayerColorsSet side2 = 0;
                for ( int i = 0; i < maxNumOfPlayers; ++i ) {
                    const PlayerColor color = Color::IndexToColor( i );
                    if ( !( fi.kingdomColors & color ) ) {
                        continue;
                    }
                    if ( fi.unions[i] & fi.colorsAvailableForHumans ) {
                        side1 |= static_cast<PlayerColorsSet>( color );
                    }
                    else {
                        side2 |= static_cast<PlayerColorsSet>( color );
                    }
                }
                if ( side1 != 0 && side2 != 0 ) {
                    mapFormat.alliances.push_back( side1 );
                    mapFormat.alliances.push_back( side2 );
                }
                else {
                    // Fallback: defeat everyone.
                    mapFormat.victoryConditionType = Maps::FileInfo::VICTORY_DEFEAT_EVERYONE;
                }
            }
            break;
        default:
            break;
        }

        mapFormat.lossConditionType = fi.lossConditionType;

        // Populate loss condition metadata.
        switch ( fi.lossConditionType ) {
        case Maps::FileInfo::LOSS_EVERYTHING:
            // No metadata.
            break;
        case Maps::FileInfo::LOSS_TOWN: {
            const uint32_t tileIndex = static_cast<uint32_t>( fi.lossConditionParams[0] ) + static_cast<uint32_t>( fi.lossConditionParams[1] ) * fi.width;
            mapFormat.lossConditionMetadata.push_back( tileIndex );

            const Castle * castle = mapWorld.getCastle( Maps::GetPoint( static_cast<int32_t>( tileIndex ) ) );
            mapFormat.lossConditionMetadata.push_back( castle ? static_cast<uint32_t>( castle->GetColor() ) : 0 );
            break;
        }
        case Maps::FileInfo::LOSS_HERO: {
            const uint32_t tileIndex = static_cast<uint32_t>( fi.lossConditionParams[0] ) + static_cast<uint32_t>( fi.lossConditionParams[1] ) * fi.width;
            mapFormat.lossConditionMetadata.push_back( tileIndex );

            const Heroes * hero = mapWorld.GetHeroes( Maps::GetPoint( static_cast<int32_t>( tileIndex ) ) );
            mapFormat.lossConditionMetadata.push_back( hero ? static_cast<uint32_t>( hero->GetColor() ) : 0 );
            break;
        }
        case Maps::FileInfo::LOSS_OUT_OF_TIME:
            // metadata[0] = number of days
            mapFormat.lossConditionMetadata.push_back( fi.lossConditionParams[0] );
            break;
        default:
            break;
        }

        // Copy player colors.
        mapFormat.availablePlayerColors = fi.kingdomColors;
        mapFormat.humanPlayerColors = fi.colorsAvailableForHumans;
        mapFormat.computerPlayerColors = fi.colorsAvailableForComp;

        // Copy player races (one per player slot: Blue, Green, Red, Yellow, Orange, Purple).
        mapFormat.playerRace = fi.races;

        // Step 3: terrain + object tiles.
        const int32_t mapWidth = mapWorld.w();
        mapFormat.width = mapWidth;

        const int32_t totalTiles = mapWidth * mapWidth;
        mapFormat.tiles.resize( static_cast<size_t>( totalTiles ) );

        // Maps hero/castle tile IDs to the UIDs assigned in Step 3.
        // Heroes: LoadMapMP2() strips the MINIHERO sprite so _mainObjectPart is blank.
        // Castles: basement (LANDSCAPE_TOWN_BASEMENTS) and castle (KINGDOM_TOWNS) must share
        //          one UID, with the basement placed first, so the castle sprite wins the main
        //          object part slot after sorting (mirrors _placeCastle() in editor_interface.cpp).
        // Flags: the two LANDSCAPE_FLAGS tiles adjacent to each castle entrance (tileId±1) must
        //        also share the castle's UID so that getTownColorIndex() can match them.  They
        //        are emitted by the generic tryAddObject path with the raw MP2 UID, so we fix
        //        them up after the main loop.
        std::map<int32_t, uint32_t> heroUidByTileId;
        std::map<int32_t, uint32_t> castleUidByTileId;
        std::map<int32_t, std::pair<uint32_t, uint32_t>> castleFlagByTileId; // tileId → shared UID and flag object index
        struct ImportedPlaceholder
        {
            MP2PlaceholderObjectInfo source;
            ObjectGroup group{ ObjectGroup::NONE };
            uint32_t objectIndex{ 0 };
        };
        std::map<int32_t, ImportedPlaceholder> placeholderByTileId;
        MP2MapValidationInfo validationInfo;

        for ( const MP2PlaceholderObjectInfo & placeholder : importInfo.placeholderObjects ) {
            ObjectGroup group;
            uint32_t objectIndex = 0;
            if ( placeholder.position < 0 || placeholder.position >= totalTiles || placeholder.objectUid == 0 ) {
                ERROR_LOG( "Invalid MP2 placeholder at tile " << placeholder.position << " with UID " << placeholder.objectUid << '.' )
                return false;
            }
            const bool isResolved = placeholder.objectType == MP2::OBJ_HERO
                                        ? Maps::getObjectGroupAndIndexByMainIcn( static_cast<MP2::ObjectIcnType>( placeholder.objectIcnType ), placeholder.objectIcnIndex,
                                                                                 group, objectIndex )
                                        : getUniqueEditorObjectByType( placeholder.objectType, group, objectIndex );
            if ( !isResolved ) {
                ERROR_LOG( "Failed to resolve MP2 placeholder type " << static_cast<uint32_t>( placeholder.objectType ) << " at tile " << placeholder.position << '.' )
                return false;
            }
            if ( !placeholderByTileId.emplace( placeholder.position, ImportedPlaceholder{ placeholder, group, objectIndex } ).second ) {
                ERROR_LOG( "Multiple MP2 placeholders occupy tile " << placeholder.position << '.' )
                return false;
            }

            validationInfo.placeholderObjects.emplace_back( placeholder.position, group, objectIndex );
        }

        for ( int32_t tileId = 0; tileId < totalTiles; ++tileId ) {
            const Maps::Tile & worldTile = mapWorld.getTile( tileId );
            TileInfo & mapTile = mapFormat.tiles[static_cast<size_t>( tileId )];
            const Castle * castleOnTile = mapWorld.getCastle( Maps::GetPoint( tileId ) );
            const Castle * castleAtEntrance = ( castleOnTile != nullptr && castleOnTile->GetIndex() == tileId ) ? castleOnTile : nullptr;

            if ( castleAtEntrance != nullptr ) {
                validationInfo.castlePositions.push_back( tileId );
            }
            if ( worldTile.isRoad() ) {
                validationInfo.roadPositions.push_back( tileId );
            }
            const auto placeholderOnTile = placeholderByTileId.find( tileId );
            if ( worldTile.getMainObjectType( false ) == MP2::OBJ_ARTIFACT
                 || ( placeholderOnTile != placeholderByTileId.end() && placeholderOnTile->second.group == ObjectGroup::ADVENTURE_ARTIFACTS ) ) {
                validationInfo.artifactPositions.push_back( tileId );
            }

            mapTile.terrainIndex = worldTile.getTerrainImageIndex();
            mapTile.terrainFlags = worldTile.getTerrainFlags();

            // tryAddObject: emit a TileObjectInfo only if the part is the *main* part (first
            // groundLevelPart, at tileOffset {0,0}) of a known object in the registry.
            // Multi-tile objects store a single TileObjectInfo at their root tile;
            // readTileObject/setObjectOnTile reconstructs all other parts when the map loads.
            const auto tryAddObject = [&]( const Maps::ObjectPart & part ) {
                if ( part.icnType == MP2::OBJ_ICN_TYPE_UNKNOWN ) {
                    return;
                }
                if ( placeholderOnTile != placeholderByTileId.end() && placeholderOnTile->second.source.objectType == MP2::OBJ_HERO
                     && part._uid == placeholderOnTile->second.source.objectUid ) {
                    return;
                }

                ObjectGroup group;
                uint32_t objIndex;
                if ( Maps::getObjectGroupAndIndexByMainIcn( part.icnType, part.icnIndex, group, objIndex ) ) {
                    if ( group == ObjectGroup::ROADS || group == ObjectGroup::LANDSCAPE_FLAGS
                         || ( castleAtEntrance != nullptr && ( group == ObjectGroup::KINGDOM_TOWNS || group == ObjectGroup::LANDSCAPE_TOWN_BASEMENTS ) ) ) {
                        // Original hero sprites can leave FLAG32 parts behind after the MP2 loader removes MINIHERO.
                        // Castle parts and flags are reconstructed explicitly after every castle receives its new shared UID.
                        return;
                    }
                    mapTile.objects.push_back( { part._uid, group, objIndex } );
                }
            };

            // --- Castle entrance special case ---
            // The editor (_placeCastle) places the LANDSCAPE_TOWN_BASEMENTS object first,
            // then resets the UID counter so the KINGDOM_TOWNS castle gets the SAME UID.
            // Both objects share one UID; the basement must appear first in mapTile.objects
            // so that during FH2M loading the castle is placed last and wins _mainObjectPart.
            // A hero may occupy the castle entrance. In that case the castle's root sprite is
            // stored among the ground parts rather than in getMainObjectPart().
            if ( castleAtEntrance != nullptr ) {
                ObjectGroup castleGroup;
                uint32_t castleIndex = 0;
                uint32_t castleSourceUid = 0;
                bool hasCastleSprite = false;

                const auto placeholderIter = placeholderByTileId.find( tileId );
                if ( placeholderIter != placeholderByTileId.end()
                     && ( placeholderIter->second.source.objectType == MP2::OBJ_RANDOM_TOWN || placeholderIter->second.source.objectType == MP2::OBJ_RANDOM_CASTLE ) ) {
                    castleGroup = placeholderIter->second.group;
                    castleIndex = placeholderIter->second.objectIndex;
                    castleSourceUid = placeholderIter->second.source.objectUid;
                    hasCastleSprite = castleGroup == ObjectGroup::KINGDOM_TOWNS;
                }
                else {
                    hasCastleSprite
                        = Maps::getObjectGroupAndIndexByMainIcn( worldTile.getMainObjectPart().icnType, worldTile.getMainObjectPart().icnIndex, castleGroup, castleIndex )
                          && castleGroup == ObjectGroup::KINGDOM_TOWNS;
                    if ( hasCastleSprite ) {
                        castleSourceUid = worldTile.getMainObjectPart()._uid;
                    }
                }

                if ( !hasCastleSprite ) {
                    for ( const auto & part : worldTile.getGroundObjectParts() ) {
                        hasCastleSprite
                            = Maps::getObjectGroupAndIndexByMainIcn( part.icnType, part.icnIndex, castleGroup, castleIndex ) && castleGroup == ObjectGroup::KINGDOM_TOWNS;
                        if ( hasCastleSprite ) {
                            castleSourceUid = part._uid;
                            break;
                        }
                    }
                }

                if ( !hasCastleSprite || castleSourceUid == 0 ) {
                    return false;
                }

                const int castleColorIndex = Color::GetIndex( castleAtEntrance->GetColor() );
                if ( castleColorIndex < 0 || castleColorIndex > 6 ) {
                    return false;
                }

                const uint32_t sharedUid = castleSourceUid;

                // Basement first — find the OBJNTWBA sprite (icnOffset+2) in ground parts.
                for ( const auto & part : worldTile.getGroundObjectParts() ) {
                    if ( part.icnType != MP2::OBJ_ICN_TYPE_OBJNTWBA ) {
                        continue;
                    }
                    ObjectGroup basementGroup;
                    uint32_t basementIndex = 0;
                    if ( Maps::getObjectGroupAndIndexByMainIcn( part.icnType, part.icnIndex, basementGroup, basementIndex )
                         && basementGroup == ObjectGroup::LANDSCAPE_TOWN_BASEMENTS ) {
                        mapTile.objects.push_back( { sharedUid, basementGroup, basementIndex } );
                        break; // one basement per castle
                    }
                }

                // Castle second — shares the same UID so it overwrites the basement in
                // _mainObjectPart after sortObjectParts() (pushed last = wins OBJECT_LAYER slot).
                mapTile.objects.push_back( { sharedUid, castleGroup, castleIndex } );
                castleUidByTileId[tileId] = sharedUid;

                if ( tileId > 0 ) {
                    castleFlagByTileId[tileId - 1] = { sharedUid, static_cast<uint32_t>( castleColorIndex * 2 ) };
                }
                if ( tileId + 1 < totalTiles ) {
                    castleFlagByTileId[tileId + 1] = { sharedUid, static_cast<uint32_t>( castleColorIndex * 2 + 1 ) };
                }
            }

            // --- Hero special case ---
            // LoadMapMP2() removes the OBJ_ICN_TYPE_MINIHERO sprite via removeObjectPartsByUID(),
            // leaving no sprite to reverse-map. Reconstruct the TileObjectInfo from color/race.
            const auto placeholderIter = placeholderByTileId.find( tileId );
            const bool hasRandomHeroPlaceholder = placeholderIter != placeholderByTileId.end() && placeholderIter->second.source.objectType == MP2::OBJ_HERO;
            if ( hasRandomHeroPlaceholder ) {
                validationInfo.heroPositions.push_back( tileId );
                mapTile.objects.push_back( { placeholderIter->second.source.objectUid, placeholderIter->second.group, placeholderIter->second.objectIndex } );
                heroUidByTileId[tileId] = placeholderIter->second.source.objectUid;

                const Heroes * hero = mapWorld.GetHeroes( placeholderIter->second.source.heroId );
                if ( hero == nullptr ) {
                    return false;
                }
            }
            else if ( worldTile.getMainObjectType() == MP2::OBJ_HERO ) {
                validationInfo.heroPositions.push_back( tileId );

                const Heroes * hero = mapWorld.GetHeroes( Maps::GetPoint( tileId ) );
                if ( hero != nullptr ) {
                    const int colorIdx = Color::GetIndex( hero->GetColor() );
                    int raceIdx = 6; // default: RAND/unknown
                    switch ( hero->GetRace() ) {
                    case Race::KNGT:
                        raceIdx = 0;
                        break;
                    case Race::BARB:
                        raceIdx = 1;
                        break;
                    case Race::SORC:
                        raceIdx = 2;
                        break;
                    case Race::WRLK:
                        raceIdx = 3;
                        break;
                    case Race::WZRD:
                        raceIdx = 4;
                        break;
                    case Race::NECR:
                        raceIdx = 5;
                        break;
                    default:
                        break;
                    }
                    // Color::GetIndex returns 6 for NONE/invalid; only 0-5 have hero objects.
                    if ( colorIdx < 6 ) {
                        const uint32_t heroObjectIndex = static_cast<uint32_t>( colorIdx * 7 + raceIdx );
                        const uint32_t heroUid = Maps::getNewObjectUID();
                        mapTile.objects.push_back( { heroUid, ObjectGroup::KINGDOM_HEROES, heroObjectIndex } );
                        heroUidByTileId[tileId] = heroUid;
                    }
                }
            }

            // --- Generic path ---
            tryAddObject( worldTile.getMainObjectPart() );
            for ( const auto & part : worldTile.getGroundObjectParts() ) {
                tryAddObject( part );
            }
            for ( const auto & part : worldTile.getTopObjectParts() ) {
                tryAddObject( part );
            }

            if ( placeholderIter != placeholderByTileId.end() && placeholderIter->second.source.objectType != MP2::OBJ_RANDOM_TOWN
                 && placeholderIter->second.source.objectType != MP2::OBJ_RANDOM_CASTLE && placeholderIter->second.source.objectType != MP2::OBJ_HERO ) {
                const uint32_t placeholderUid = placeholderIter->second.source.objectUid;
                mapTile.objects.erase( std::remove_if( mapTile.objects.begin(), mapTile.objects.end(),
                                                       [placeholderUid]( const TileObjectInfo & object ) { return object.id == placeholderUid; } ),
                                       mapTile.objects.end() );
                mapTile.objects.push_back( { placeholderUid, placeholderIter->second.group, placeholderIter->second.objectIndex } );
            }

            // Fix-up: correct mine resource type.
            // tryAddObject finds the terrain ICN of the mine, which is registered as the Ore mine
            // variant (index +0) for each terrain type. The actual resource is encoded in the EXTRAOVR
            // main part's icnIndex (0=Ore, 1=Sulfur, 2=Crystal, 3=Gems, 4=Gold). Add that offset to
            // the emitted ADVENTURE_MINES TileObjectInfo index to select the right resource variant.
            if ( worldTile.getMainObjectType() == MP2::OBJ_MINE && worldTile.getMainObjectPart().icnType == MP2::OBJ_ICN_TYPE_EXTRAOVR ) {
                const uint8_t extraovrIdx = worldTile.getMainObjectPart().icnIndex;
                if ( extraovrIdx > 0 && extraovrIdx < 5 ) {
                    for ( auto & obj : mapTile.objects ) {
                        if ( obj.group == ObjectGroup::ADVENTURE_MINES ) {
                            obj.index += extraovrIdx;
                            break;
                        }
                    }
                }
            }
        }

        const MP2UltimateArtifactInfo & ultimateArtifactInfo = importInfo.ultimateArtifact;
        if ( ultimateArtifactInfo.position >= 0 ) {
            const auto placeholderIter = placeholderByTileId.find( ultimateArtifactInfo.position );
            if ( placeholderIter == placeholderByTileId.end() || placeholderIter->second.source.objectType != MP2::OBJ_RANDOM_ULTIMATE_ARTIFACT
                 || placeholderIter->second.source.objectUid != ultimateArtifactInfo.objectUid || placeholderIter->second.group != ObjectGroup::ADVENTURE_ARTIFACTS ) {
                return false;
            }

            ArtifactMetadata & metadata = mapFormat.artifactMetadata[ultimateArtifactInfo.objectUid];
            metadata.radius = ultimateArtifactInfo.radius;
            if ( Artifact{ ultimateArtifactInfo.artifactId }.isUltimate() ) {
                metadata.selected = { ultimateArtifactInfo.artifactId };
            }

            validationInfo.ultimateArtifactPosition = ultimateArtifactInfo.position;
            validationInfo.ultimateArtifactObjectUid = ultimateArtifactInfo.objectUid;
        }

        // Road main sprites are not unique: their editor objects also encode connections to neighboring road tiles.
        // Reconstruct one road root per source road tile from connectivity instead of reverse-mapping an ambiguous sprite.
        for ( const int32_t roadTileId : validationInfo.roadPositions ) {
            uint32_t roadObjectIndex = 0;

            if ( roadTileId >= mapWidth ) {
                const auto & aboveObjects = mapFormat.tiles[static_cast<size_t>( roadTileId - mapWidth )].objects;
                if ( std::any_of( aboveObjects.cbegin(), aboveObjects.cend(),
                                  []( const TileObjectInfo & object ) { return object.group == ObjectGroup::KINGDOM_TOWNS; } ) ) {
                    roadObjectIndex = 512;
                }
            }

            if ( roadObjectIndex != 512 ) {
                for ( const int32_t nearbyTileId : Maps::getAroundIndexes( roadTileId, mapWidth, mapWidth, 1 ) ) {
                    if ( mapWorld.getTile( nearbyTileId ).isRoad() ) {
                        roadObjectIndex |= static_cast<uint32_t>( Maps::GetDirection( roadTileId, nearbyTileId ) );
                    }
                }
            }

            mapFormat.tiles[static_cast<size_t>( roadTileId )].objects.push_back( { Maps::getNewObjectUID(), ObjectGroup::ROADS, roadObjectIndex } );
        }

        // Reconstruct only flags belonging to actual castles. The legacy loader can leave orphan hero FLAG32 parts in neighboring tiles.
        for ( const auto & [flagTileId, flagInfo] : castleFlagByTileId ) {
            mapFormat.tiles[static_cast<size_t>( flagTileId )].objects.push_back( { flagInfo.first, ObjectGroup::LANDSCAPE_FLAGS, flagInfo.second } );
        }

        // Step 4: castle metadata — iterate tiles to find castle entrances.
        for ( int32_t tileId = 0; tileId < totalTiles; ++tileId ) {
            // Use the UID we assigned in Step 3 (the shared UID for basement + castle).
            const auto castleUidIt = castleUidByTileId.find( tileId );
            if ( castleUidIt == castleUidByTileId.end() ) {
                continue;
            }
            const uint32_t uid = castleUidIt->second;

            const Castle * castle = mapWorld.getCastle( Maps::GetPoint( tileId ) );
            if ( castle == nullptr || castle->GetIndex() != tileId ) {
                return false;
            }

            const auto metadataIter = importInfo.objectMetadata.find( tileId );
            if ( metadataIter == importInfo.objectMetadata.end() ) {
                return false;
            }

            CastleMetadata metadata;
            if ( !readMP2CastleMetadata( metadataIter->second, metadata ) ) {
                return false;
            }

            mapFormat.castleMetadata[uid] = std::move( metadata );
        }

        // Step 5: per-tile metadata (heroes, signs, events, sphinxes, capturable objects).
        const auto getImportedObjectUid = [&mapFormat]( const int32_t tileId, const MP2::MapObjectType objectType ) {
            for ( const TileObjectInfo & object : mapFormat.tiles[static_cast<size_t>( tileId )].objects ) {
                if ( Maps::getObjectInfo( object.group, static_cast<int32_t>( object.index ) ).objectType == objectType ) {
                    return object.id;
                }
            }

            return uint32_t{ 0 };
        };

        for ( int32_t tileId = 0; tileId < totalTiles; ++tileId ) {
            const Maps::Tile & tile = mapWorld.getTile( tileId );
            const MP2::MapObjectType objType = tile.getMainObjectType();
            const auto placeholderIter = placeholderByTileId.find( tileId );

            if ( objType == MP2::OBJ_NONE ) {
                continue;
            }

            const uint32_t uid = tile.getMainObjectPart()._uid;

            const auto saveSelectionMetadata = [&mapFormat, tileId, objType, &tile]() {
                const TileInfo & mapTile = mapFormat.tiles[static_cast<size_t>( tileId )];
                for ( const TileObjectInfo & object : mapTile.objects ) {
                    if ( Maps::getObjectInfo( object.group, static_cast<int32_t>( object.index ) ).objectType == objType ) {
                        mapFormat.selectionObjectMetadata[object.id].selectedItems = { static_cast<int32_t>( tile.metadata()[0] ) };
                        return;
                    }
                }
            };

            switch ( objType ) {
            case MP2::OBJ_HERO: {
                const auto it = heroUidByTileId.find( tileId );
                if ( it != heroUidByTileId.end() ) {
                    const Heroes * hero = mapWorld.GetHeroes( Maps::GetPoint( tileId ) );
                    const auto metadataIter = importInfo.objectMetadata.find( tileId );
                    if ( hero == nullptr || metadataIter == importInfo.objectMetadata.end() ) {
                        return false;
                    }

                    const uint8_t race = static_cast<uint8_t>(
                        placeholderIter != placeholderByTileId.end() && placeholderIter->second.source.objectType == MP2::OBJ_HERO ? Race::RAND : hero->GetRace() );
                    HeroMetadata & metadata = mapFormat.heroMetadata[it->second];
                    if ( !readMP2HeroMetadata( metadataIter->second, race, metadata ) ) {
                        return false;
                    }
                    preserveDerivedHeroMetadata( *hero, metadataIter->second[1] != 0, metadata );
                }
                break;
            }
            case MP2::OBJ_JAIL: {
                const uint32_t objectUid = getImportedObjectUid( tileId, MP2::OBJ_JAIL );
                const auto metadataIter = importInfo.objectMetadata.find( tileId );
                const Heroes * hero = mapWorld.GetHeroes( Maps::GetPoint( tileId ) );
                if ( objectUid == 0 || hero == nullptr || metadataIter == importInfo.objectMetadata.end()
                     || metadataIter->second.size() != MP2::MP2_HEROES_STRUCTURE_SIZE ) {
                    return false;
                }

                const uint8_t race
                    = metadataIter->second[60] <= 5 ? static_cast<uint8_t>( Race::IndexToRace( metadataIter->second[60] ) ) : static_cast<uint8_t>( Race::KNGT );
                HeroMetadata & metadata = mapFormat.heroMetadata[objectUid];
                if ( !readMP2HeroMetadata( metadataIter->second, race, metadata ) ) {
                    return false;
                }
                preserveDerivedHeroMetadata( *hero, metadataIter->second[1] != 0, metadata );
                break;
            }

            case MP2::OBJ_MONSTER:
            case MP2::OBJ_RANDOM_MONSTER:
            case MP2::OBJ_RANDOM_MONSTER_WEAK:
            case MP2::OBJ_RANDOM_MONSTER_MEDIUM:
            case MP2::OBJ_RANDOM_MONSTER_STRONG:
            case MP2::OBJ_RANDOM_MONSTER_VERY_STRONG: {
                // Every MONSTERS-group TileObjectInfo requires a monsterMetadata entry (assertion
                // in world_loadmap.cpp:884).  Conversely, creating metadata without a matching
                // TileObjectInfo triggers the reverse assertion at line 1232.  So we only
                // create metadata when Step 3 actually emitted a TileObjectInfo for this tile,
                // which we detect by scanning the tile's objects for a MONSTERS-group entry.
                // Monsters whose sprites are absent from mainObjectByIcn (e.g. Azure Dragon)
                // produce no TileObjectInfo and must produce no metadata either.
                const TileInfo & mapTile = mapFormat.tiles[static_cast<size_t>( tileId )];
                for ( const TileObjectInfo & obj : mapTile.objects ) {
                    if ( obj.group == ObjectGroup::MONSTERS ) {
                        MonsterMetadata & meta = mapFormat.monsterMetadata[obj.id];
                        if ( placeholderIter == placeholderByTileId.end() ) {
                            meta.count = static_cast<int32_t>( tile.metadata()[0] );
                        }
                        break;
                    }
                }
                break;
            }

            case MP2::OBJ_ARTIFACT:
            case MP2::OBJ_RANDOM_ARTIFACT:
            case MP2::OBJ_RANDOM_ARTIFACT_TREASURE:
            case MP2::OBJ_RANDOM_ARTIFACT_MINOR:
            case MP2::OBJ_RANDOM_ARTIFACT_MAJOR:
            case MP2::OBJ_RANDOM_ULTIMATE_ARTIFACT: {
                // Every ADVENTURE_ARTIFACTS-group TileObjectInfo requires an artifactMetadata entry.
                // Same bidirectional constraint as monsters (world_loadmap.cpp:1146 and :1243).
                // Only create metadata when Step 3 actually emitted an ADVENTURE_ARTIFACTS entry.
                // After LoadMapMP2() random artifacts are already resolved to OBJ_ARTIFACT, so
                // only OBJ_RANDOM_ULTIMATE_ARTIFACT keeps its own type.
                const TileInfo & mapTile = mapFormat.tiles[static_cast<size_t>( tileId )];
                for ( const TileObjectInfo & obj : mapTile.objects ) {
                    if ( obj.group == ObjectGroup::ADVENTURE_ARTIFACTS ) {
                        ArtifactMetadata & meta = mapFormat.artifactMetadata[obj.id];
                        if ( placeholderIter == placeholderByTileId.end() ) {
                            if ( objType == MP2::OBJ_RANDOM_ULTIMATE_ARTIFACT ) {
                                meta.radius = static_cast<int32_t>( tile.metadata()[0] );
                            }
                            else {
                                const Artifact art = Maps::getArtifactFromTile( tile );
                                if ( art.GetID() == Artifact::SPELL_SCROLL ) {
                                    // selected[0] = spell ID; loadResurrectionMap stores it as
                                    // tileData[0] = selected[0] - 1, then updateObjectInfoTile
                                    // adds 1 back and sets metadata()[1] = spell ID.
                                    meta.selected = { art.getSpellId() };
                                }
                                // Regular artifacts: default empty metadata is fine.
                            }
                        }
                        break;
                    }
                }
                break;
            }

            case MP2::OBJ_RESOURCE:
            case MP2::OBJ_RANDOM_RESOURCE: {
                // Every ADVENTURE_TREASURES OBJ_RESOURCE TileObjectInfo needs a resourceMetadata
                // entry so that readAllTiles sets the resource count on the tile. Without it the
                // count stays 0 and getFundsFromTile returns all-zero Funds, crashing the AI.
                const TileInfo & mapTile = mapFormat.tiles[static_cast<size_t>( tileId )];
                for ( const TileObjectInfo & obj : mapTile.objects ) {
                    if ( obj.group == ObjectGroup::ADVENTURE_TREASURES ) {
                        if ( placeholderIter == placeholderByTileId.end() ) {
                            mapFormat.resourceMetadata[obj.id].count = static_cast<int32_t>( tile.metadata()[1] );
                        }
                        break;
                    }
                }
                break;
            }

            case MP2::OBJ_SIGN:
            case MP2::OBJ_BOTTLE: {
                const uint32_t objectUid = getImportedObjectUid( tileId, objType );
                const auto metadataIter = importInfo.objectMetadata.find( tileId );
                if ( objectUid == 0 || metadataIter == importInfo.objectMetadata.end()
                     || !readMP2SignMetadata( metadataIter->second, mapFormat.signMetadata[objectUid] ) ) {
                    return false;
                }
                break;
            }

            case MP2::OBJ_EVENT: {
                const uint32_t objectUid = getImportedObjectUid( tileId, objType );
                const auto metadataIter = importInfo.objectMetadata.find( tileId );
                if ( objectUid == 0 || metadataIter == importInfo.objectMetadata.end()
                     || !readMP2AdventureMapEventMetadata( metadataIter->second, mapFormat.adventureMapEventMetadata[objectUid] ) ) {
                    return false;
                }
                break;
            }

            case MP2::OBJ_SPHINX: {
                const uint32_t objectUid = getImportedObjectUid( tileId, objType );
                const auto metadataIter = importInfo.objectMetadata.find( tileId );
                if ( objectUid == 0 || metadataIter == importInfo.objectMetadata.end()
                     || !readMP2SphinxMetadata( metadataIter->second, mapFormat.sphinxMetadata[objectUid] ) ) {
                    return false;
                }
                break;
            }

            case MP2::OBJ_PYRAMID:
            case MP2::OBJ_SHRINE_FIRST_CIRCLE:
            case MP2::OBJ_SHRINE_SECOND_CIRCLE:
            case MP2::OBJ_SHRINE_THIRD_CIRCLE:
            case MP2::OBJ_WITCHS_HUT:
                validationInfo.selectionObjectPositions.push_back( tileId );
                saveSelectionMetadata();
                break;

            default:
                // Capturable objects (mines, lighthouses, sawmills, etc.) – preserve owner color if non-neutral.
                if ( Maps::isCapturableObject( objType ) ) {
                    const PlayerColor ownerColor = mapWorld.GetCapturedObject( tileId ).GetColor();
                    if ( ownerColor != PlayerColor::NONE ) {
                        mapFormat.capturableObjectsMetadata[uid].ownerColor = ownerColor;
                    }
                }
                break;
            }
        }

        // Step 6: daily events and rumors.
        for ( const EventDate & event : mapWorld.getDailyEvents() ) {
            mapFormat.dailyEvents.emplace_back( convertDailyEvent( event ) );
        }
        validationInfo.dailyEventCount = mapWorld.getDailyEvents().size();

        for ( const std::string & rumor : mapWorld.getCustomRumors() ) {
            mapFormat.rumors.push_back( rumor );
        }
        validationInfo.rumorCount = mapWorld.getCustomRumors().size();

        std::string validationError;
        if ( !validateImportedMapObjectPlacement( mapFormat, validationInfo, &validationError ) ) {
            ERROR_LOG( "Failed to validate imported MP2 map object placement: " << validationError )
            return false;
        }

        return true;
    }
}
