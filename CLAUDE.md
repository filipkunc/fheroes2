# fheroes2 Project Guidelines

## Palette System

The game uses an indexed 256-color palette loaded from `KB.PAL` (extracted from `HEROES2.AGG`).

### Key palette ranges and their colors (bright to dark within each range):
- **0-9**: System/special colors
- **10-36**: Grays — bright white (252,252,252) at index 10 → black (0,0,0) at index 36
- **37-64**: Browns
- **65-84**: Blues
- **85-107**: Greens
- **108-130**: Yellows/golds — bright yellow at 108 → dark gold at 130
- **131-151**: Purples — bright lavender (228,204,248) at 131 → dark purple (16,0,44) at 151
- **152-174**: Cyans/teals
- **175-197**: Reds — bright pink (244,208,208) at 176 → dark red (72,0,0) at 197
- **198-213**: Dark oranges/browns
- **214-217**: Cycling animation (gold group 1) — DO NOT use unless animation is needed
- **218-221**: Cycling animation (gold group 2) — DO NOT use unless animation is needed
- **222-230**: Grays
- **231-235**: Cycling animation (blue)
- **238-241**: Cycling animation
- **242-255**: Blues

### Palette remapping gotchas:
1. **Direction matters**: All color ranges go bright→dark as index increases. When remapping, preserve this direction (e.g. gray 10→red 176, gray 36→red 197).
2. **Always analyze the source sprite first**: Use `icn2img` to extract the sprite, then analyze pixel palette indices with PIL/Pillow to see which indices are actually used and how frequently. Don't guess.
3. **Use `pal2img`** to generate a visual palette reference: `pal2img kb.pal palette.png`
4. **Use `icn2img`** to extract sprites: `icn2img dst_dir kb.pal input.icn`
5. **Use `extractor`** to get AGG contents: `extractor dst_dir HEROES2.AGG`
6. **Game data location**: `/home/fkunc/Games/Heroic/HoMM 2 Gold/DATA/HEROES2.AGG`

### Palette remap tables (in `src/engine/pal.cpp`):
- Tables are 256-entry vectors mapping old index → new index
- Identity mapping (no change) = same value as the index position
- Palettes are defined as `PaletteType` enum in `src/engine/pal.h`

## Custom Monsters
This branch adds custom monsters: Azure Dragon (Warlock), Blood Dragon (Necromancer), Thor (Wizard), Avenger (Knight).
- Monster enum defined in `src/fheroes2/monster/monster.h`
- Stats and abilities in `src/fheroes2/monster/monster_info.cpp`
- Battle sprites generated via palette transforms from base monsters in `src/fheroes2/agg/agg_image.cpp`

### Checklist when adding a new custom monster:
1. Add to `MonsterType` enum in `monster.h`
2. Add `DWELLING_UPGRADEn` in `castle.h` and update `DWELLING_UPGRADES` bitmask
3. Add stats (4 parallel arrays: ICN, BIN, sounds, battle stats) + general stats + abilities in `monster_info.cpp`
4. Wire upgrade/downgrade, FromDwelling, GetDwelling, ICNMonh, GetUpgradeCost in `monster.cpp`
5. Add ICN entries in `icn.h`
6. Add palette type in `pal.h` and palette table in `pal.cpp`
7. Add sprite generation in `agg_image.cpp`: battle sprite (ICN case), portrait (MONH case), building sprite, MONS32 entry, MINIMON entry
8. Add to `AnimationReference` whitelist in `battle_animation.cpp` (line ~97)
9. Castle integration in `castle.cpp`:
   - `_postLoad()`: clear new dwelling for non-applicable races, keep for applicable race
   - Dwelling growth init (add before existing checks)
   - Castle value scoring
   - `getUpgradedDwellingID` / `checkBuilding`: add to DWELLING_MONSTER6 and DWELLING_UPGRADE6 bitmasks
   - `_getDwelling`: add case returning `&_dwelling[5]`
   - `GetActualDwelling`: add to early-return pass-through list AND to DWELLING_MONSTER6/UPGRADE6 resolution
   - `allDwellings` array: increase size and add entry
   - `getDwellingIndexFromId`: add case with `dwellingIndex = 5`
   - `getMonstersCountByDwelling`: add case returning `_dwelling[5]`
   - `getTownSpecificImageId`: add case for race returning building ICN
   - `GetUpgradeBuilding`: add race-specific multi-tier upgrade handling
   - `CheckBuyBuilding`: add race restriction (`UNKNOWN_UPGRADE` for wrong races)
10. Building info in `castle_building_info.cpp`: building area, name, description, upgrade chain, requirements, sprite index, draw priorities
11. Building cost in `buildinginfo.cpp`: add to `buildingStats` array (increase array size), add to `isDwelling` switch
12. AI logic in `ai_planner_castle.cpp`: add to race build order and `castleDwellings` array (increase size)
13. Update `populateMonsters()` loop bound in `map_object_info.cpp`
14. Add battle damage handling if new ability type (in `battle_troop.cpp`)
15. Add ability description text in `monster_info.cpp` `getMonsterAbilityText()`

## Build
- Build directory: `/home/fkunc/Projects/fheroes2/build/`
- Tools (extractor, icn2img, pal2img) are in the build directory

## Android Build
- Requires Java 17 (system Java 25 is too new for Gradle 8.13)
- Java 17 installed via SDKMAN: `~/.sdkman/candidates/java/17.0.13-tem`
- Android SDK location: `~/Android/Sdk`
- Android deps script (`script/android/install_packages.sh`) may have stale checksum — if it fails, download with `curl -L` and extract manually with `unzip -d android`
- Build command:
  ```
  cd android
  JAVA_HOME=~/.sdkman/candidates/java/17.0.13-tem ANDROID_HOME=~/Android/Sdk ./gradlew assembleDebug
  ```
- APK output: `android/app/build/outputs/apk/debug/app-debug.apk`
- Install: `adb install <apk>` (may need `adb uninstall org.fheroes2` first if signing key differs)
- Phone device ID: RZCX50PCXYA (Samsung)
