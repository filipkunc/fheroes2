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
This branch adds custom monsters: Azure Dragon (Warlock), Blood Dragon (Necromancer), Thor (Wizard), Avenger (Knight), Succubus (Barbarian).
- Monster enum defined in `src/fheroes2/monster/monster.h`
- Stats and abilities in `src/fheroes2/monster/monster_info.cpp`
- Battle sprites generated via palette transforms from base monsters in `src/fheroes2/agg/agg_image.cpp`

### Checklist when adding a new custom monster:
1. Add to `MonsterType` enum in `monster.h`
2. Add `DWELLING_UPGRADEn` in `castle.h` and update `DWELLING_UPGRADES` bitmask
3. Add stats (4 parallel arrays: ICN, BIN, sounds, battle stats) + general stats + abilities in `monster_info.cpp`
4. Wire upgrade/downgrade, FromDwelling, GetDwelling, ICNMonh, GetUpgradeCost in `monster.cpp`
5. Add ICN entries in `icn.h`
6. Add palette type in `pal.h` and palette table in `pal.cpp` (reuse BLOOD_DRAGON / BLOOD_CRYPT if the target is red — no need to duplicate the tables)
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

### Adding a new spell-caster creature (e.g. HYPNOTIZE):
If a monster casts a spell that isn't yet used by any creature, three switch statements have `assert(0)` defaults that crash in debug. Add the spell case to each:
- `getMonsterBaseStrength` in `monster_info.cpp` — power-rating of the spell
- `evaluateThreatForUnit` in `battle_troop.cpp` — AI damage threat contribution
- `RedrawActionMonsterSpellCastStatus` in `battle_interface.cpp` — status bar message

Creature-cast spells that depend on hero spell power (`HYPNOTIZE`, damage spells) also need a null-`applyingHero` fallback in `Unit::GetMagicResist` (`spellPowerForBuiltinMonsterSpells` / a per-spell constant like `spellPowerForMonsterHypnotize`).

## Hi-res RGBA sprite pipeline (Thor, Succubus)

Monster battle sprites generated from the sprite editor tool live as large PNGs under `files/data/sprites/{prefix}_NNN.png` and a sidecar `{prefix}_offsets.jsonl` with per-frame `offset_x/offset_y/display_width/display_height`. Registration lives in `AGG::GetRGBACustomFrames` in `agg_image.cpp`:
```cpp
static const RGBACustomEntry registry[] = {
    { Monster::THOR, "thor", 56 },
    { Monster::SUCCUBUS, "succubus", 32 },
};
```
Add a line per new monster with hi-res PNGs. Frame 0 must exist (a 1×1 transparent placeholder is fine) — the loader probes it to detect "are these sprites available at all".

### Painter compositor (single Display-owned RGBA surface)

`Display` owns `_screenRGBA` at physical resolution. Every drawing primitive on `Display` (`Blit`, `Fill`, `AlphaBlit`, `ApplyPalette`, `ApplyAlpha`, `Copy`, `Flip`, `DrawLine`, `DrawRect`, `Resize`, `SubpixelResize`, `SetPixel`, `Transpose`, `ReplaceColorId{,ByTransformId}`, `addGradientShadow`, `CreateDitheringTransition`, …) ends with `out._notifyWrite(roi)`. The hook installed by `Display::setResolution` mirrors the just-written rect from indexed → `_screenRGBA` at physical scale. Sprites and intermediate buffers leave the hook null (one branch per write).

Hi-res RGBA paints (`renderHiResMonsterPortrait`, battle scene, spell effects) bypass the indexed buffer and write directly to `Display::screenRGBA()` at absolute physical-pixel coords, *after* the palette draws above them — order of execution is order of pixel writes.

`Display::render` is just: cursor blit + one `SDL_UpdateTexture` + `SDL_RenderCopy`.

`Display::changePalette(p)` swaps the active 8-bit render palette via `setRenderPalette8Bit(p)` (in `image_palette.{h,cpp}`). `paletteIdxToRGBA` reads from this palette, so any sprite blit *after* the call resolves indices against `p` instead of the static KB.PAL — the original Succession Wars and POL campaign-selection screens rely on this so X_IVY etc. render through the SMK video palette they were authored against. Already-drawn framebuffer pixels stay at the old colours until the affected widgets repaint themselves; full re-mirror and color cycling animations (gold/water/lava) remain disabled until a shader-LUT path lands.

### `ArmyBar::RedrawItem` overrides and hi-res portraits

Any subclass that overrides `RedrawItem` must handle the custom-portrait path itself — the base class's mini-sprite branch direct-paints `renderHiResMonsterPortrait` for hi-res monsters (Dachshund, Thor, …), and an override that only blits `MONS32` ends up showing the palette-quantised baked downscale instead of the hi-res RGBA art. `MeetingArmyBar::RedrawItem` in `heroes_meeting.cpp` is the reference pattern.

### Hi-res portrait callers

All call `renderHiResMonsterPortrait` (in `agg_image.cpp`), which direct-blits to `Display::screenRGBA()`:
- Battle main sprite (`RedrawTroopSprite`)
- Battle turn order (`TurnOrder::addCustomMonsterOverlays`)
- Army info dialog (`DrawMonster` in `dialog_armyinfo.cpp`)
- Set Count / Monster Selector dialogs via `MonsterDialogElement::draw` (uses `AGG::GetRGBACustomPortrait` — bbox-cropped frame 1, not the full PNG, so the figure fills the portrait rect)
- Select Monster list (`SelectEnumMonster::RedrawItem`)
- Army bar in both mini-sprite and full-portrait branches
- Adventure-map status panel (compact `drawMiniMonsters` path)

The widget code MUST draw the palette art *first* (so the WriteHook mirrors it into `_screenRGBA`) and call `renderHiResMonsterPortrait` *after* — Z-order is correct by construction. No buffer-paint pool, no forwarding stack, no scope keys, no per-frame composition step.

### Battle's RGBA writes

`Battle::Interface` writes the battle scene directly to `Display::screenRGBA()` via `_blitOnSurface` / `_alphaBlitOnSurface` / `_copyOnSurface` / `_copyFullSurface` (offset by `_interfacePosition.{x,y}`). `_battleAreaWidthPx` / `_battleAreaHeightPx` cache the battlefield rect in physical pixels for spell effects (DimRGBA, DeathWave, HolyShout, Armageddon, Earthquake, …) that operate on a sub-rect of `_screenRGBA`. There is no `_mainSurfaceRGBA` member or `_rgbaScale` — battle uses `Display::instance().getPhysicalScale()` directly.

Indexed-fallback path (`agg_image.cpp::writeIndexedSpriteFromRGBA`) uses **alpha-weighted box filter** averaging before `GetColorId` palette quantisation — preserves dramatically more detail than the engine's default nearest-neighbour `Resize` at large downscale ratios.

## Build
- Build directory: `/home/fkunc/Projects/fheroes2/build/`
- Tools (extractor, icn2img, pal2img) are in the build directory

## Android Build
- Requires Java 17 (system Java 25 is too new for Gradle 8.13). Installed via SDKMAN: `~/.sdkman/candidates/java/17.0.13-tem`
- Android SDK location: `~/Android/Sdk`. NDK 28.0.13004108 used for SDL3 prebuilts.
- **SDL3 + SDL3_mixer prebuilts live under `android/app/jni/SDL3*/`** (replaced the old SDL2 ones). Currently only `arm64-v8a` is shipped — the `app/build.gradle` `abiFilters` is set to `'arm64-v8a'` only. Other ABIs were removed during the SDL3 migration to keep the build tree small. To add other ABIs back: build SDL3 + SDL3_mixer for each, drop `.so` under `jni/SDL3*/lib/<abi>/`, reinstate the ABI in `abiFilters`.
- **Java glue is in `android/sdl3/`** (was `android/sdl2/`). Sources copied verbatim from upstream SDL3 release-3.4.8 `android-project/app/src/main/java/org/libsdl/app/`. `GameActivity.getLibraries()` is overridden to load `{"SDL3", "SDL3_mixer", "main"}` in order.
- **Manifest orientation: `landscape` (forced) + `resizeableActivity="false"`.** Samsung One UI on Galaxy A56 (and probably similar) ignored the weaker `sensorLandscape` even with the SDL_HINT_ORIENTATIONS hint set; the activity launched at the device's current physical orientation and rendered nothing visible. With the stricter manifest the launching device must already be physically held in landscape — phones held in portrait at launch may not auto-rotate. Stock-Android phones don't seem to need this.
- Build command (no change):
  ```
  cd android
  JAVA_HOME=~/.sdkman/candidates/java/17.0.13-tem ANDROID_HOME=~/Android/Sdk ./gradlew assembleDebug
  ```
- APK output: `android/app/build/outputs/apk/debug/app-debug.apk`
- Install: `adb install -r <apk>` (may need `adb uninstall org.fheroes2` first if signing key differs)
- Phone device ID: RZCX50PCXYA (Samsung Galaxy A56)
- **Known issue (not fixed yet):** dialog backgrounds leak through to show what was rendered behind the dialog (most reliably reproduced: editor → "Battle Only" → "Select Monster" — the wood-textured interior shows through to the previous main-menu sprites). Linux Vulkan does not exhibit this. See `SDL3_GPU_SHADER_PLAN.md` Phase 6 for the suspected Samsung Xclipse 540 driver quirk.

### Regenerating SDL3 / SDL3_mixer Android prebuilts

If SDL3 needs upgrading (or another ABI added), the build process is:

```bash
# SDL3 — clone matching version and build via NDK CMake toolchain
cd ~/Projects/sdl3-android/SDL3
git checkout release-3.4.8  # match the version SDL3_mixer expects
cmake -B build-android-arm64 -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_HOME/ndk/28.0.13004108/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release \
    -DCMAKE_INSTALL_PREFIX=~/Projects/sdl3-android/install-arm64 \
    -DSDL_STATIC=OFF -DSDL_SHARED=ON -DSDL_TEST_LIBRARY=OFF
cmake --build build-android-arm64 -j$(nproc)
cmake --install build-android-arm64

# SDL3_mixer — needs SDL3_DIR explicitly because find_package doesn't honour CMAKE_PREFIX_PATH
# under the NDK toolchain the same way native does. Sources need ./external/download.sh first.
cd ~/Projects/sdl3-android/SDL3_mixer
./external/download.sh   # only first time; clones libogg/vorbis/mpg123/etc.
cmake -B build-android-arm64 -G Ninja \
    -DCMAKE_TOOLCHAIN_FILE=$ANDROID_HOME/ndk/28.0.13004108/build/cmake/android.toolchain.cmake \
    -DANDROID_ABI=arm64-v8a -DANDROID_PLATFORM=android-21 \
    -DCMAKE_BUILD_TYPE=Release \
    -DSDL3_DIR=~/Projects/sdl3-android/install-arm64/lib/cmake/SDL3 \
    -DSDLMIXER_VENDORED=ON \
    -DSDLMIXER_GME=OFF -DSDLMIXER_OPUS=OFF -DSDLMIXER_WAVPACK=OFF \
    -DSDLMIXER_VORBIS=STB -DSDLMIXER_FLAC=OFF -DSDLMIXER_MOD=OFF \
    -DSDLMIXER_MP3=MPG123 -DSDLMIXER_MIDI=OFF \
    -DSDLMIXER_SAMPLES=OFF -DBUILD_SHARED_LIBS=ON
cmake --build build-android-arm64 -j$(nproc)
# Tests will fail to link (no main()) — that's fine, libSDL3_mixer.so still built.

# Drop into the Android tree
cp ~/Projects/sdl3-android/install-arm64/libSDL3.so   android/app/jni/SDL3/lib/arm64-v8a/
cp ~/Projects/sdl3-android/SDL3_mixer/build-android-arm64/libSDL3_mixer.so   android/app/jni/SDL3_mixer/lib/arm64-v8a/
# Headers update (only when SDL3 version changes):
rm -rf android/app/jni/SDL3/include/SDL3 && cp -r ~/Projects/sdl3-android/install-arm64/include/SDL3 android/app/jni/SDL3/include/
```

If the Java glue changed in upstream SDL3, copy `org.libsdl.app.*.java` into `android/sdl3/src/main/java/org/libsdl/app/` too.

## Linux Build (SDL3)

System packages required: `dnf install SDL3-devel glslang vulkan-tools`. **SDL3_mixer is not packaged on Fedora** — but the project's root `CMakeLists.txt` handles this: `find_package(SDL3_mixer CONFIG QUIET)` is tried first, and if it fails CMake's `FetchContent` clones `libsdl-org/SDL_mixer` (release-3.2.0) and builds it in-tree with only bundled codecs (dr_mp3 for MP3, stb_vorbis for Ogg — no external codec libs needed). The `SDL3_mixer::SDL3_mixer` ALIAS the engine links against is produced either way.

Build (fresh clone):

```bash
cmake -B build -G Ninja -DCMAKE_BUILD_TYPE=Debug -DENABLE_TOOLS=ON
cmake --build build -j$(nproc)
./build/fheroes2
```

The first configure takes an extra ~15s + a one-time ~30s SDL_mixer compile when fetched. Subsequent configures skip both. RPATH is baked into the binary, so no `LD_LIBRARY_PATH` is needed at runtime.

A `CMakeUserPresets.json` exists in the repo root (gitignored) with `linux-debug` / `linux-release` presets that CMake Tools picks up automatically. Use `cmake --preset linux-debug` from the CLI.

Game data + music expected at `build/data/`, `build/files/`, `build/music/` — symlink them: `ln -s "/path/to/HoMM 2 Gold/DATA" build/data` etc.

### SDL_GPU shader regeneration

The shader binaries (`src/engine/shaders/composite.*.dxil` and `composite.*.spv`) are checked into git. If you modify the HLSL or GLSL sources you must regenerate (CMake reads the binaries at configure time and embeds them into `composite_shaders_embedded.h`):

```bash
# SPIR-V via system glslang
glslang -V -S vert -o src/engine/shaders/composite.vert.spv src/engine/shaders/composite.vert.glsl
glslang -V -S frag -o src/engine/shaders/composite.frag.spv src/engine/shaders/composite.frag.glsl
glslang -V -S frag -o src/engine/shaders/cursor.frag.spv    src/engine/shaders/cursor.frag.glsl

# DXIL needs Windows + dxc.exe — see src/engine/shaders/README.md
```

After regenerating, re-run `cmake -B build` (the embedded header is generated by `configure_file`, not regular build steps).

### Android asset sync

`android/app/src/main/assets/files/` is a real copy, not a symlink. When you change anything under `files/data/sprites/` on the desktop (new monster PNGs, updated offsets JSONL, etc.), sync it before building the APK:

```
rsync -av --delete files/data/sprites/ android/app/src/main/assets/files/data/sprites/
rm -rf android/app/src/main/assets/files/data/sprites/sheets  # editor working files
```

Gradle's incremental detector doesn't reliably pick up asset changes from an rsync, so run `./gradlew clean assembleDebug` (not just `assembleDebug`) after syncing or the APK silently ships the old assets.
