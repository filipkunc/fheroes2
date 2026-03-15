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
This branch adds three custom monsters: Azure Dragon (Warlock), Blood Dragon (Necromancer), Thor (Wizard).
- Monster enum defined in `src/fheroes2/monster/monster.h`
- Stats and abilities in `src/fheroes2/monster/monster_info.cpp`
- Battle sprites generated via palette transforms from base monsters in `src/fheroes2/agg/agg_image.cpp`
- When adding new monsters, update the loop in `populateMonsters()` in `src/fheroes2/maps/map_object_info.cpp` to include them

## Build
- Build directory: `/home/fkunc/Projects/fheroes2/build/`
- Tools (extractor, icn2img, pal2img) are in the build directory
