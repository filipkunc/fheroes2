/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2023 - 2026                                             *
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

#include "map_format_info.h"

#include <algorithm>
#include <array>
#include <cassert>
#include <cstddef>
#include <initializer_list>
#include <set>
#include <utility>

#include "artifact.h"
#include "direction.h"
#include "monster.h"
#include "monster_info.h"
#include "mp2.h"
#include "rand.h"
#include "serialize.h"
#include "zzlib.h"

namespace Maps::Map_Format
{
    // This structure is deprecated. We are keeping it for backward compatibility.
    struct StandardObjectMetadata
    {
        std::array<int32_t, 3> metadata{ 0 };
    };

    // The following operators are used only inside this module, but they cannot be declared in an anonymous namespace due to the way ADL works

    OStreamBase & operator<<( OStreamBase & stream, const TileObjectInfo & object );
    IStreamBase & operator>>( IStreamBase & stream, TileObjectInfo & object );

    OStreamBase & operator<<( OStreamBase & stream, const TileInfo & tile );
    IStreamBase & operator>>( IStreamBase & stream, TileInfo & tile );

    OStreamBase & operator<<( OStreamBase & stream, const DailyEvent & eventInfo );
    IStreamBase & operator>>( IStreamBase & stream, DailyEvent & eventInfo );

    IStreamBase & operator>>( IStreamBase & stream, StandardObjectMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const CastleMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, CastleMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const HeroMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, HeroMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const SphinxMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, SphinxMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const SignMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, SignMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const AdventureMapEventMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, AdventureMapEventMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const SelectionObjectMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, SelectionObjectMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const CapturableObjectMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, CapturableObjectMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const MonsterMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, MonsterMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const ArtifactMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, ArtifactMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const ResourceMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, ResourceMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const TranslationSphinxMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, TranslationSphinxMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const TranslationBaseMapMetadata & metadata );
    IStreamBase & operator>>( IStreamBase & stream, TranslationBaseMapMetadata & metadata );

    OStreamBase & operator<<( OStreamBase & stream, const TranslationFormat & metadata );
    IStreamBase & operator>>( IStreamBase & stream, TranslationFormat & metadata );
}

namespace
{
    const std::array<uint8_t, 6> magicWord{ 'h', '2', 'm', 'a', 'p', '\0' };

    // This value is set to avoid any corrupted files to be processed.
    // It is impossible to have a map with smaller than this size.
    const size_t minFileSize{ 512 };

    constexpr uint16_t minimumSupportedVersion{ 2 };

    // Change the version when there is a need to expand map format functionality.
    constexpr uint16_t currentSupportedVersion{ 14 };

    void loadCastleMetadata( IStreamBase & stream, std::map<uint32_t, Maps::Map_Format::CastleMetadata> & castleMetadata, const uint16_t mapVersion,
                             const bool use64BitBuildingIds )
    {
        if ( mapVersion >= 14 || use64BitBuildingIds ) {
            stream >> castleMetadata;
            return;
        }

        const uint32_t size = stream.get32();
        castleMetadata.clear();

        for ( uint32_t i = 0; i < size; ++i ) {
            uint32_t objectId = 0;
            Maps::Map_Format::CastleMetadata metadata;
            std::vector<uint32_t> builtBuildings;
            std::vector<uint32_t> bannedBuildings;

            stream >> objectId >> metadata.customName >> metadata.defenderMonsterType >> metadata.defenderMonsterCount >> metadata.customBuildings >> builtBuildings
                >> bannedBuildings >> metadata.mustHaveSpells >> metadata.bannedSpells >> metadata.availableToHireMonsterCount;

            metadata.builtBuildings.assign( builtBuildings.cbegin(), builtBuildings.cend() );
            metadata.bannedBuildings.assign( bannedBuildings.cbegin(), bannedBuildings.cend() );
            castleMetadata.emplace( objectId, std::move( metadata ) );
        }
    }

    bool hasCompleteRequiredObjectMetadata( const Maps::Map_Format::MapFormat & map )
    {
        for ( const Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( const Maps::Map_Format::TileObjectInfo & objectInfo : tileInfo.objects ) {
                if ( objectInfo.group == Maps::ObjectGroup::MONSTERS && map.monsterMetadata.find( objectInfo.id ) == map.monsterMetadata.end() ) {
                    return false;
                }

                if ( objectInfo.group == Maps::ObjectGroup::ADVENTURE_ARTIFACTS && map.artifactMetadata.find( objectInfo.id ) == map.artifactMetadata.end() ) {
                    return false;
                }
            }
        }

        return true;
    }

    int32_t convertLegacyFkMonsterId( const int32_t monsterId )
    {
        const fheroes2::CustomMonsterDefinition * definition = fheroes2::findCustomMonsterDefinitionByLegacyFkId( monsterId );
        return definition != nullptr ? definition->id : monsterId;
    }

    void convertLegacyFkV13MonsterData( Maps::Map_Format::MapFormat & map )
    {
        static_assert( Monster::WATER_ELEMENT == 66 && Monster::RANDOM_MONSTER == 67 && Monster::RANDOM_MONSTER_LEVEL_4 == 71,
                       "Monster object indices are part of the legacy FK map compatibility contract." );

        const uint32_t standardMonsterCount = Monster::WATER_ELEMENT;
        const uint32_t customMonsterCount = static_cast<uint32_t>( fheroes2::getCustomMonsterDefinitions().size() );
        const uint32_t randomMonsterCount = Monster::RANDOM_MONSTER_LEVEL_4 - Monster::RANDOM_MONSTER + 1;

        // The FK v13 editor placed custom creatures before random monster placeholders. The current registry keeps
        // upstream random placeholders at their original indices and appends custom creatures after them.
        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objectInfo : tileInfo.objects ) {
                if ( objectInfo.group != Maps::ObjectGroup::MONSTERS ) {
                    continue;
                }

                if ( objectInfo.index >= standardMonsterCount && objectInfo.index < standardMonsterCount + customMonsterCount ) {
                    objectInfo.index += randomMonsterCount;
                }
                else if ( objectInfo.index >= standardMonsterCount + customMonsterCount
                          && objectInfo.index < standardMonsterCount + customMonsterCount + randomMonsterCount ) {
                    objectInfo.index -= customMonsterCount;
                }
            }
        }

        for ( auto & [objectId, metadata] : map.castleMetadata ) {
            (void)objectId;
            std::transform( metadata.defenderMonsterType.begin(), metadata.defenderMonsterType.end(), metadata.defenderMonsterType.begin(), convertLegacyFkMonsterId );
        }

        for ( auto & [objectId, metadata] : map.heroMetadata ) {
            (void)objectId;
            std::transform( metadata.armyMonsterType.begin(), metadata.armyMonsterType.end(), metadata.armyMonsterType.begin(), convertLegacyFkMonsterId );
        }

        for ( auto & [objectId, metadata] : map.adventureMapEventMetadata ) {
            (void)objectId;
            metadata.monsterType = convertLegacyFkMonsterId( metadata.monsterType );
        }

        for ( auto & [objectId, metadata] : map.monsterMetadata ) {
            (void)objectId;
            std::transform( metadata.selected.begin(), metadata.selected.end(), metadata.selected.begin(), convertLegacyFkMonsterId );
        }
    }

    void convertFromV2ToV3( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 2, "Remove this function." );

        if ( map.version > 2 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_DWELLINGS ) {
                    switch ( objInfo.index ) {
                    case 17: // Graveyard, grass terrain, ugly version
                        objInfo.group = Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS;
                        objInfo.index = 62;
                        break;
                    case 18: // Graveyard, snow terrain, ugly version
                        objInfo.group = Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS;
                        objInfo.index = 63;
                        break;
                    case 19: // Graveyard, desert terrain(?), ugly version
                        objInfo.group = Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS;
                        objInfo.index = 64;
                        break;
                    case 20: // Graveyard, generic terrain
                        objInfo.group = Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS;
                        objInfo.index = 0;
                        break;
                    case 21: // Graveyard, snow terrain
                        objInfo.group = Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS;
                        objInfo.index = 1;
                        break;
                    default: // Shift the rest of the objects in the Dwellings group by 5 positions "up"
                        if ( objInfo.index > 21 ) {
                            objInfo.index -= 5;
                        }
                        break;
                    }

                    continue;
                }

                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS ) {
                    // Shift the objects in the Miscellaneous group by 2 positions "down", since non-ugly Graveyard versions were added to the beginning of this group
                    objInfo.index += 2;
                }
            }
        }
    }

    void convertFromV3ToV4( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 3, "Remove this function." );

        if ( map.version > 3 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_DWELLINGS && objInfo.index >= 18 ) {
                    // Shift the objects in the Dwellings group by 1 position "down" to add a new Cave object.
                    objInfo.index += 1;
                }
            }
        }
    }

    void convertFromV4ToV5( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 4, "Remove this function." );

        if ( map.version > 4 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::LANDSCAPE_MISCELLANEOUS && objInfo.index >= 128 ) {
                    // Shift the objects in the Landscape Miscellaneous group by 1 position "down" to add missed Small cliff, dirt terrain.
                    objInfo.index += 1;
                }
            }
        }
    }

    void convertFromV5ToV6( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 5, "Remove this function." );

        if ( map.version > 5 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS && objInfo.index >= 17 ) {
                    // Shift the objects in the Adventure Miscellaneous group by 1 position "down" to add new Lean-To object.
                    objInfo.index += 1;
                }
            }
        }
    }

    void convertFromV6ToV7( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 6, "Remove this function." );

        if ( map.version > 6 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS && objInfo.index >= 38 ) {
                    // Shift the objects in the Adventure Miscellaneous group by 1 position "down" to add new Stone Liths object.
                    objInfo.index += 1;
                }
            }
        }
    }

    void convertFromV7ToV8( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 7, "Remove this function." );

        if ( map.version > 7 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_MISCELLANEOUS && objInfo.index >= 43 ) {
                    // Shift the objects in the Adventure Miscellaneous group by 3 position "down" to add new Observation Tower variants.
                    objInfo.index += 3;
                }
            }
        }
    }

    void convertFromV9ToV10( Maps::Map_Format::MapFormat & map, std::map<uint32_t, Maps::Map_Format::StandardObjectMetadata> standardMetadata )
    {
        static_assert( minimumSupportedVersion <= 9, "Remove this function." );

        if ( map.version > 9 ) {
            return;
        }

        map.monsterMetadata = {};
        map.artifactMetadata = {};
        map.resourceMetadata = {};

        const auto & artifactObjects = Maps::getObjectsByGroup( Maps::ObjectGroup::ADVENTURE_ARTIFACTS );
        const auto & treasuresObjects = Maps::getObjectsByGroup( Maps::ObjectGroup::ADVENTURE_TREASURES );

        // In version 10 we updated metadata for monsters and artifacts. We have to move the original metadata for them from one structure to another.
        for ( size_t tileId = 0; tileId < map.tiles.size(); ++tileId ) {
            const auto & tile = map.tiles[tileId];

            for ( const auto & object : tile.objects ) {
                if ( object.group == Maps::ObjectGroup::MONSTERS ) {
                    assert( standardMetadata.find( object.id ) != standardMetadata.end() );

                    map.monsterMetadata[object.id].count = standardMetadata[object.id].metadata[0];
                }
                else if ( object.group == Maps::ObjectGroup::ADVENTURE_ARTIFACTS ) {
                    const auto & objectInfo = artifactObjects[object.index];

                    assert( standardMetadata.find( object.id ) != standardMetadata.end() );

                    if ( objectInfo.objectType == MP2::OBJ_RANDOM_ULTIMATE_ARTIFACT ) {
                        map.artifactMetadata[object.id].radius = standardMetadata[object.id].metadata[0];
                    }
                    else if ( objectInfo.objectType == MP2::OBJ_ARTIFACT && objectInfo.metadata[0] == Artifact::SPELL_SCROLL ) {
                        map.artifactMetadata[object.id].selected.push_back( standardMetadata[object.id].metadata[0] );
                    }
                    else {
                        map.artifactMetadata[object.id] = {};
                    }
                }
                else if ( object.group == Maps::ObjectGroup::ADVENTURE_TREASURES ) {
                    assert( object.index < treasuresObjects.size() );
                    const auto & objectInfo = treasuresObjects[object.index];

                    if ( objectInfo.objectType == MP2::OBJ_RESOURCE ) {
                        map.resourceMetadata[object.id].count = standardMetadata[object.id].metadata[0];
                    }
                }
            }
        }
    }

    void convertFromV11ToV12( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 11, "Remove this function." );

        if ( map.version > 11 ) {
            return;
        }

        const std::set<uint32_t> consideredAsRoadIndecies{ 0, 2, 3, 4, 5, 6, 7, 9, 12, 13, 14, 16, 17, 18, 19, 20, 21, 26, 28, 29, 30, 31 };

        // Remove road objects that are not considered as roads on the Adventure Map.
        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( auto iter = tileInfo.objects.begin(); iter != tileInfo.objects.end(); ) {
                if ( iter->group == Maps::ObjectGroup::ROADS && consideredAsRoadIndecies.count( iter->index ) == 0 ) {
                    // This is a part of the road just as a decoration. Remove it.
                    iter = tileInfo.objects.erase( iter );
                    continue;
                }

                ++iter;
            }
        }

        // Update objects that are marked as road corresponding to their relative positions to other tiles with roads.
        const int32_t size = map.width * map.width;
        for ( int32_t tileIndex = 0; tileIndex < size; ++tileIndex ) {
            auto & tileObjects = map.tiles[tileIndex].objects;
            auto roadObjectIter = std::find_if( tileObjects.begin(), tileObjects.end(), []( const auto & object ) { return object.group == Maps::ObjectGroup::ROADS; } );

            if ( roadObjectIter == tileObjects.end() ) {
                continue;
            }

            auto & roadObjectIndex = roadObjectIter->index;

            if ( tileIndex > map.width ) {
                const auto & aboveObjects = map.tiles[tileIndex - map.width].objects;

                if ( std::any_of( aboveObjects.cbegin(), aboveObjects.cend(), []( const auto & object ) { return object.group == Maps::ObjectGroup::KINGDOM_TOWNS; } ) ) {
                    // 512 is the index of castle entrance road object.
                    roadObjectIndex = 512;

                    continue;
                }
            }

            roadObjectIndex = 0;

            const int32_t centerX = tileIndex % map.width;
            const int32_t centerY = tileIndex / map.width;

            // Avoid getting out of map boundaries.
            const int32_t minTileX = std::max<int32_t>( centerX - 1, 0 );
            const int32_t minTileY = std::max<int32_t>( centerY - 1, 0 );
            const int32_t maxTileX = std::min<int32_t>( centerX + 1 + 1, map.width );
            const int32_t maxTileY = std::min<int32_t>( centerY + 1 + 1, map.width );

            for ( int32_t tileY = minTileY; tileY < maxTileY; ++tileY ) {
                const int32_t indexOffsetY = tileY * map.width;

                for ( int32_t tileX = minTileX; tileX < maxTileX; ++tileX ) {
                    const int32_t otherTileIndex = indexOffsetY + tileX;

                    if ( otherTileIndex == tileIndex ) {
                        // Skip the center tile.
                        continue;
                    }

                    const auto & objects = map.tiles[otherTileIndex].objects;

                    if ( std::any_of( objects.cbegin(), objects.cend(), []( const auto & object ) { return object.group == Maps::ObjectGroup::ROADS; } ) ) {
                        const int32_t diff = otherTileIndex - tileIndex;

                        if ( diff == ( -map.width - 1 ) ) {
                            roadObjectIndex |= Direction::TOP_LEFT;
                        }
                        else if ( diff == -map.width ) {
                            roadObjectIndex |= Direction::TOP;
                        }
                        else if ( diff == ( -map.width + 1 ) ) {
                            roadObjectIndex |= Direction::TOP_RIGHT;
                        }
                        else if ( diff == -1 ) {
                            roadObjectIndex |= Direction::LEFT;
                        }
                        else if ( diff == 1 ) {
                            roadObjectIndex |= Direction::RIGHT;
                        }
                        else if ( diff == map.width - 1 ) {
                            roadObjectIndex |= Direction::BOTTOM_LEFT;
                        }
                        else if ( diff == map.width ) {
                            roadObjectIndex |= Direction::BOTTOM;
                        }
                        else if ( diff == map.width + 1 ) {
                            roadObjectIndex |= Direction::BOTTOM_RIGHT;
                        }
                    }
                }
            }

            // Road sprites have 2 variants in assets.
            roadObjectIndex += Rand::Get( 1 ) * 256;
        }
    }

    void convertFromV12ToV13( Maps::Map_Format::MapFormat & map )
    {
        static_assert( minimumSupportedVersion <= 12, "Remove this function." );

        if ( map.version > 12 ) {
            return;
        }

        for ( Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( Maps::Map_Format::TileObjectInfo & objInfo : tileInfo.objects ) {
                if ( objInfo.group == Maps::ObjectGroup::ADVENTURE_MINES ) {
                    if ( objInfo.index >= 42 ) {
                        // Shift the objects in the Mines group by 6 position "down" to add 6 new Abandoned Mines.
                        objInfo.index += 6;
                    }
                    else if ( objInfo.index == 40 ) {
                        // Shift Grass Abandoned Mine to a new position.
                        objInfo.index = 41;
                    }
                    else if ( objInfo.index == 41 ) {
                        // Shift Dirt Abandoned Mine to a new position.
                        objInfo.index = 46;
                    }
                }
            }
        }
    }

    void addMissingDefaultObjectMetadata( Maps::Map_Format::MapFormat & map )
    {
        const std::vector<Maps::ObjectInfo> & artifactObjects = Maps::getObjectsByGroup( Maps::ObjectGroup::ADVENTURE_ARTIFACTS );

        for ( const Maps::Map_Format::TileInfo & tileInfo : map.tiles ) {
            for ( const Maps::Map_Format::TileObjectInfo & objectInfo : tileInfo.objects ) {
                if ( objectInfo.group == Maps::ObjectGroup::MONSTERS ) {
                    map.monsterMetadata.try_emplace( objectInfo.id );
                }
                else if ( objectInfo.group == Maps::ObjectGroup::ADVENTURE_ARTIFACTS && objectInfo.index < artifactObjects.size() ) {
                    const Maps::ObjectInfo & artifactObject = artifactObjects[objectInfo.index];
                    if ( artifactObject.objectType != MP2::OBJ_ARTIFACT || artifactObject.metadata[0] != Artifact::SPELL_SCROLL ) {
                        map.artifactMetadata.try_emplace( objectInfo.id );
                    }
                }
            }
        }
    }

    bool saveToStream( OStreamBase & stream, const Maps::Map_Format::BaseMapFormat & map )
    {
        stream << currentSupportedVersion << map.isCampaign << map.difficulty << map.availablePlayerColors << map.humanPlayerColors << map.computerPlayerColors
               << map.alliances << map.playerRace << map.victoryConditionType << map.isVictoryConditionApplicableForAI << map.allowNormalVictory
               << map.victoryConditionMetadata << map.lossConditionType << map.lossConditionMetadata << map.width << map.mainLanguage << map.name << map.description
               << map.creatorNotes << map.translations;

        return !stream.fail();
    }

    bool loadFromStream( IStreamBase & stream, Maps::Map_Format::BaseMapFormat & map )
    {
        stream >> map.version;
        if ( map.version < minimumSupportedVersion || map.version > currentSupportedVersion ) {
            return false;
        }

        stream >> map.isCampaign >> map.difficulty >> map.availablePlayerColors >> map.humanPlayerColors >> map.computerPlayerColors >> map.alliances >> map.playerRace
            >> map.victoryConditionType >> map.isVictoryConditionApplicableForAI >> map.allowNormalVictory >> map.victoryConditionMetadata >> map.lossConditionType
            >> map.lossConditionMetadata >> map.width;

        if ( map.width <= 0 ) {
            // This is not a correct map size.
            return false;
        }

        stream >> map.mainLanguage >> map.name >> map.description;

        if ( map.version < 9 ) {
            map.creatorNotes = {};
        }
        else {
            stream >> map.creatorNotes;
        }

        if ( map.version < 11 ) {
            // True translation support was added in version 11.
            map.translations = {};
        }
        else {
            stream >> map.translations;
        }

        return !stream.fail();
    }

    bool saveToStream( OStreamBase & stream, const Maps::Map_Format::MapFormat & map )
    {
        // Only the base map information is not encoded.
        // The rest of data must be compressed to prevent manual corruption of the file.
        if ( !saveToStream( stream, static_cast<const Maps::Map_Format::BaseMapFormat &>( map ) ) ) {
            return false;
        }

        RWStreamBuf compressed;
        compressed.setBigendian( true );

        compressed << map.additionalInfo << map.tiles << map.dailyEvents << map.rumors << map.castleMetadata << map.heroMetadata << map.sphinxMetadata << map.signMetadata
                   << map.adventureMapEventMetadata << map.selectionObjectMetadata << map.capturableObjectsMetadata << map.monsterMetadata << map.artifactMetadata
                   << map.resourceMetadata << map.translationInfo;

        const std::vector<uint8_t> temp = Compression::zipData( compressed.data(), compressed.size(), false );

        stream.putRaw( temp.data(), temp.size() );

        return !stream.fail();
    }

    bool loadFromStream( IStreamBase & stream, Maps::Map_Format::MapFormat & map )
    {
        // TODO: verify the correctness of metadata.
        if ( !loadFromStream( stream, static_cast<Maps::Map_Format::BaseMapFormat &>( map ) ) ) {
            map = {};
            return false;
        }

        std::vector<uint8_t> compressedData = stream.getRaw( 0 );
        if ( compressedData.empty() ) {
            // This is a corrupted file.
            map = {};
            return false;
        }

        const std::vector<uint8_t> decompressedData = Compression::unzipData( compressedData.data(), compressedData.size() );
        if ( decompressedData.empty() ) {
            // This is a corrupted file.
            map = {};
            return false;
        }

        const Maps::Map_Format::BaseMapFormat baseMap = static_cast<const Maps::Map_Format::BaseMapFormat &>( map );
        const auto parseMapData = [&baseMap, &decompressedData]( Maps::Map_Format::MapFormat & parsedMap, const bool use64BitBuildingIds ) {
            static_cast<Maps::Map_Format::BaseMapFormat &>( parsedMap ) = baseMap;

            ROStreamBuf decompressed( decompressedData );
            decompressed.setBigendian( true );
            decompressed >> parsedMap.additionalInfo >> parsedMap.tiles;

            if ( parsedMap.tiles.size() != static_cast<size_t>( parsedMap.width ) * parsedMap.width ) {
                return false;
            }

            decompressed >> parsedMap.dailyEvents >> parsedMap.rumors;

            std::map<uint32_t, Maps::Map_Format::StandardObjectMetadata> standardMetadata;
            if ( parsedMap.version < 10 ) {
                decompressed >> standardMetadata;
            }

            loadCastleMetadata( decompressed, parsedMap.castleMetadata, parsedMap.version, use64BitBuildingIds );
            if ( decompressed.fail() ) {
                return false;
            }

            decompressed >> parsedMap.heroMetadata >> parsedMap.sphinxMetadata >> parsedMap.signMetadata >> parsedMap.adventureMapEventMetadata
                >> parsedMap.selectionObjectMetadata;
            if ( decompressed.fail() ) {
                return false;
            }

            static_assert( minimumSupportedVersion <= 8, "Remove this check." );
            if ( parsedMap.version > 8 ) {
                decompressed >> parsedMap.capturableObjectsMetadata;
            }
            if ( decompressed.fail() ) {
                return false;
            }

            convertFromV2ToV3( parsedMap );
            convertFromV3ToV4( parsedMap );
            convertFromV4ToV5( parsedMap );
            convertFromV5ToV6( parsedMap );
            convertFromV6ToV7( parsedMap );
            convertFromV7ToV8( parsedMap );

            if ( parsedMap.version > 9 ) {
                decompressed >> parsedMap.monsterMetadata >> parsedMap.artifactMetadata >> parsedMap.resourceMetadata;
            }
            else {
                convertFromV9ToV10( parsedMap, std::move( standardMetadata ) );
            }
            if ( decompressed.fail() ) {
                return false;
            }

            if ( parsedMap.version < 11 ) {
                parsedMap.translationInfo = {};
            }
            else {
                decompressed >> parsedMap.translationInfo;
            }

            convertFromV11ToV12( parsedMap );
            convertFromV12ToV13( parsedMap );

            return !decompressed.fail() && decompressed.size() == 0;
        };

        Maps::Map_Format::MapFormat standardMap;
        const bool isStandardMapParsed = parseMapData( standardMap, false );

        // The FK Extended Edition used version 13 while already storing 64-bit castle building IDs. Retry this exact
        // legacy layout when the upstream v13 interpretation is invalid or loses required object metadata.
        if ( map.version == 13 && ( !isStandardMapParsed || !hasCompleteRequiredObjectMetadata( standardMap ) ) ) {
            Maps::Map_Format::MapFormat legacyFkMap;
            if ( parseMapData( legacyFkMap, true ) && hasCompleteRequiredObjectMetadata( legacyFkMap ) ) {
                convertLegacyFkV13MonsterData( legacyFkMap );
                map = std::move( legacyFkMap );
                return true;
            }
        }

        if ( !isStandardMapParsed ) {
            map = {};
            return false;
        }

        map = std::move( standardMap );

        // Some maps made by older editors omitted default monster and artifact metadata entirely. Missing metadata has
        // the same meaning as a default-constructed entry, so restore it before runtime and editor code validates the map.
        addMissingDefaultObjectMetadata( map );

        return true;
    }
}

namespace Maps::Map_Format
{
    OStreamBase & operator<<( OStreamBase & stream, const TileObjectInfo & object )
    {
        return stream << object.id << object.group << object.index;
    }

    IStreamBase & operator>>( IStreamBase & stream, TileObjectInfo & object )
    {
        return stream >> object.id >> object.group >> object.index;
    }

    OStreamBase & operator<<( OStreamBase & stream, const TileInfo & tile )
    {
        return stream << tile.terrainIndex << tile.terrainFlags << tile.objects;
    }

    IStreamBase & operator>>( IStreamBase & stream, TileInfo & tile )
    {
        return stream >> tile.terrainIndex >> tile.terrainFlags >> tile.objects;
    }

    OStreamBase & operator<<( OStreamBase & stream, const DailyEvent & eventInfo )
    {
        return stream << eventInfo.message << eventInfo.humanPlayerColors << eventInfo.computerPlayerColors << eventInfo.firstOccurrenceDay
                      << eventInfo.repeatPeriodInDays << eventInfo.resources;
    }

    IStreamBase & operator>>( IStreamBase & stream, DailyEvent & eventInfo )
    {
        return stream >> eventInfo.message >> eventInfo.humanPlayerColors >> eventInfo.computerPlayerColors >> eventInfo.firstOccurrenceDay
               >> eventInfo.repeatPeriodInDays >> eventInfo.resources;
    }

    IStreamBase & operator>>( IStreamBase & stream, StandardObjectMetadata & metadata )
    {
        return stream >> metadata.metadata;
    }

    OStreamBase & operator<<( OStreamBase & stream, const CastleMetadata & metadata )
    {
        return stream << metadata.customName << metadata.defenderMonsterType << metadata.defenderMonsterCount << metadata.customBuildings << metadata.builtBuildings
                      << metadata.bannedBuildings << metadata.mustHaveSpells << metadata.bannedSpells << metadata.availableToHireMonsterCount;
    }

    IStreamBase & operator>>( IStreamBase & stream, CastleMetadata & metadata )
    {
        return stream >> metadata.customName >> metadata.defenderMonsterType >> metadata.defenderMonsterCount >> metadata.customBuildings >> metadata.builtBuildings
               >> metadata.bannedBuildings >> metadata.mustHaveSpells >> metadata.bannedSpells >> metadata.availableToHireMonsterCount;
    }

    OStreamBase & operator<<( OStreamBase & stream, const HeroMetadata & metadata )
    {
        return stream << metadata.customName << metadata.customPortrait << metadata.armyMonsterType << metadata.armyMonsterCount << metadata.artifact
                      << metadata.artifactMetadata << metadata.availableSpells << metadata.isOnPatrol << metadata.patrolRadius << metadata.secondarySkill
                      << metadata.secondarySkillLevel << metadata.customLevel << metadata.customExperience << metadata.customAttack << metadata.customDefense
                      << metadata.customKnowledge << metadata.customSpellPower << metadata.magicPoints << metadata.race;
    }

    IStreamBase & operator>>( IStreamBase & stream, HeroMetadata & metadata )
    {
        return stream >> metadata.customName >> metadata.customPortrait >> metadata.armyMonsterType >> metadata.armyMonsterCount >> metadata.artifact
               >> metadata.artifactMetadata >> metadata.availableSpells >> metadata.isOnPatrol >> metadata.patrolRadius >> metadata.secondarySkill
               >> metadata.secondarySkillLevel >> metadata.customLevel >> metadata.customExperience >> metadata.customAttack >> metadata.customDefense
               >> metadata.customKnowledge >> metadata.customSpellPower >> metadata.magicPoints >> metadata.race;
    }

    OStreamBase & operator<<( OStreamBase & stream, const SphinxMetadata & metadata )
    {
        return stream << metadata.riddle << metadata.answers << metadata.artifact << metadata.artifactMetadata << metadata.resources;
    }

    IStreamBase & operator>>( IStreamBase & stream, SphinxMetadata & metadata )
    {
        return stream >> metadata.riddle >> metadata.answers >> metadata.artifact >> metadata.artifactMetadata >> metadata.resources;
    }

    OStreamBase & operator<<( OStreamBase & stream, const SignMetadata & metadata )
    {
        return stream << metadata.message;
    }

    IStreamBase & operator>>( IStreamBase & stream, SignMetadata & metadata )
    {
        return stream >> metadata.message;
    }

    OStreamBase & operator<<( OStreamBase & stream, const AdventureMapEventMetadata & metadata )
    {
        return stream << metadata.message << metadata.humanPlayerColors << metadata.computerPlayerColors << metadata.isRecurringEvent << metadata.artifact
                      << metadata.artifactMetadata << metadata.resources << metadata.attack << metadata.defense << metadata.knowledge << metadata.spellPower
                      << metadata.experience << metadata.secondarySkill << metadata.secondarySkillLevel << metadata.monsterType << metadata.monsterCount;
    }

    IStreamBase & operator>>( IStreamBase & stream, AdventureMapEventMetadata & metadata )
    {
        return stream >> metadata.message >> metadata.humanPlayerColors >> metadata.computerPlayerColors >> metadata.isRecurringEvent >> metadata.artifact
               >> metadata.artifactMetadata >> metadata.resources >> metadata.attack >> metadata.defense >> metadata.knowledge >> metadata.spellPower
               >> metadata.experience >> metadata.secondarySkill >> metadata.secondarySkillLevel >> metadata.monsterType >> metadata.monsterCount;
    }

    OStreamBase & operator<<( OStreamBase & stream, const SelectionObjectMetadata & metadata )
    {
        return stream << metadata.selectedItems;
    }

    IStreamBase & operator>>( IStreamBase & stream, SelectionObjectMetadata & metadata )
    {
        return stream >> metadata.selectedItems;
    }

    OStreamBase & operator<<( OStreamBase & stream, const CapturableObjectMetadata & metadata )
    {
        return stream << metadata.ownerColor;
    }

    IStreamBase & operator>>( IStreamBase & stream, CapturableObjectMetadata & metadata )
    {
        return stream >> metadata.ownerColor;
    }

    OStreamBase & operator<<( OStreamBase & stream, const MonsterMetadata & metadata )
    {
        return stream << metadata.count << metadata.joinCondition << metadata.isWeeklyGrowthDisabled << metadata.selected;
    }

    IStreamBase & operator>>( IStreamBase & stream, MonsterMetadata & metadata )
    {
        return stream >> metadata.count >> metadata.joinCondition >> metadata.isWeeklyGrowthDisabled >> metadata.selected;
    }

    OStreamBase & operator<<( OStreamBase & stream, const ArtifactMetadata & metadata )
    {
        return stream << metadata.radius << metadata.captureCondition << metadata.selected;
    }

    IStreamBase & operator>>( IStreamBase & stream, ArtifactMetadata & metadata )
    {
        return stream >> metadata.radius >> metadata.captureCondition >> metadata.selected;
    }

    OStreamBase & operator<<( OStreamBase & stream, const ResourceMetadata & metadata )
    {
        return stream << metadata.count;
    }

    IStreamBase & operator>>( IStreamBase & stream, ResourceMetadata & metadata )
    {
        return stream >> metadata.count;
    }

    OStreamBase & operator<<( OStreamBase & stream, const TranslationSphinxMetadata & metadata )
    {
        return stream << metadata.riddle << metadata.answers;
    }

    IStreamBase & operator>>( IStreamBase & stream, TranslationSphinxMetadata & metadata )
    {
        return stream >> metadata.riddle >> metadata.answers;
    }

    OStreamBase & operator<<( OStreamBase & stream, const TranslationBaseMapMetadata & metadata )
    {
        return stream << metadata.name << metadata.description << metadata.creatorNotes;
    }

    IStreamBase & operator>>( IStreamBase & stream, TranslationBaseMapMetadata & metadata )
    {
        return stream >> metadata.name >> metadata.description >> metadata.creatorNotes;
    }

    OStreamBase & operator<<( OStreamBase & stream, const TranslationFormat & metadata )
    {
        return stream << metadata.dailyEvents << metadata.rumors << metadata.castleMetadata << metadata.heroMetadata << metadata.sphinxMetadata << metadata.signMetadata
                      << metadata.adventureMapEventMetadata;
    }

    IStreamBase & operator>>( IStreamBase & stream, TranslationFormat & metadata )
    {
        return stream >> metadata.dailyEvents >> metadata.rumors >> metadata.castleMetadata >> metadata.heroMetadata >> metadata.sphinxMetadata >> metadata.signMetadata
               >> metadata.adventureMapEventMetadata;
    }

    bool loadBaseMap( const std::string & path, BaseMapFormat & map )
    {
        if ( path.empty() ) {
            return false;
        }

        StreamFile fileStream;
        fileStream.setBigendian( true );

        if ( !fileStream.open( path, "rb" ) ) {
            return false;
        }

        const size_t fileSize = fileStream.size();
        if ( fileSize < minFileSize ) {
            return false;
        }

        for ( const uint8_t value : magicWord ) {
            if ( fileStream.get() != value ) {
                return false;
            }
        }

        return loadFromStream( fileStream, map );
    }

    bool loadMap( const std::string & path, MapFormat & map )
    {
        if ( path.empty() ) {
            return false;
        }

        StreamFile fileStream;
        fileStream.setBigendian( true );

        if ( !fileStream.open( path, "rb" ) ) {
            return false;
        }

        const size_t fileSize = fileStream.size();
        if ( fileSize < minFileSize ) {
            return false;
        }

        for ( const uint8_t value : magicWord ) {
            if ( fileStream.get() != value ) {
                return false;
            }
        }

        return loadFromStream( fileStream, map );
    }

    bool saveMap( const std::string & path, const MapFormat & map )
    {
        if ( path.empty() ) {
            return false;
        }

        StreamFile fileStream;
        fileStream.setBigendian( true );

        if ( !fileStream.open( path, "wb" ) ) {
            return false;
        }

        for ( const uint8_t value : magicWord ) {
            fileStream.put( value );
        }

        return saveToStream( fileStream, map );
    }

    bool saveMap( OStreamBase & stream, const MapFormat & map )
    {
        return saveToStream( stream, map );
    }

    bool loadMap( IStreamBase & stream, MapFormat & map )
    {
        return loadFromStream( stream, map );
    }
}
