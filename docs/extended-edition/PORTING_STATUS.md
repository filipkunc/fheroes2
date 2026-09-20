# Extended Edition porting status

This is the execution checklist and handoff log. Keep it short enough to read at the start of every development session.

Status markers:

* `[ ]` not started
* `[~]` in progress
* `[x]` complete and validated
* `[!]` blocked or requires a decision

## Current state

* [x] Inspect the preserved `FK/Azure-Dragon` implementation.
* [x] Agree on product direction, primary platforms and upstream strategy.
* [x] Separate the art application from the game architecture.
* [x] Define the proprietary-data boundary and synthetic CI approach.
* [x] Create the clean rebuild from upstream commit `495c790e`.
* [x] Define `master` as the exact upstream mirror and `extended-edition` as the long-lived integration branch.
* [x] Establish the initial Linux and Android CI baseline.
* [x] Add repository safeguards against accidental proprietary asset commits.
* [x] Add the first synthetic renderer test executable and fixtures.

The Linux SDL3 platform slice and original-map editor importer are now implemented; SDL2 remains the default runtime.

## Reference implementation findings

The inspection snapshot found 104 branch-only commits and 272 newer upstream commits. A direct merge was projected to conflict in 27 files, including
central image, screen, input, audio, battle, UI and gameplay paths.

Important findings to preserve or resolve:

* Seven custom creatures exist: Azure Dragon, Blood Dragon, Thor, Avenger, Succubus, Dachshund and Maid. Five have dedicated high-resolution RGBA artwork
  and two use palette-remap fallbacks.
* Creature data is repeated across arrays, switches and generated tables.
* The renderer writes a physical-resolution RGBA display with painter ordering.
* Indexed and mask storage remains partly allocated even though the painter path bypasses much of it.
* Vulkan and Windows shader paths do not describe the same compositor behavior.
* Android packages vendored SDL3 libraries but does not reproducibly package all high-resolution sprite assets.
* The art editor is substantial enough to be its own project and currently lacks a complete reproducible Python environment.
* Background removal can erase legitimate sprite colors and the native high-resolution workflow can lose detail.

These are migration inputs, not requirements to reproduce the old implementation exactly.

## Workstreams

### Foundation

* [x] Confirm the clean upstream Linux build.
* [x] Confirm the clean upstream Android build.
* [x] Add or adjust CI without requiring original game data.
* [x] Add asset-path ignore rules and a tracked-file guard.
* [x] Introduce synthetic renderer fixtures described in `TESTING.md`.

### Custom game model

* [x] Design stable custom creature identifiers and save compatibility.
* [x] Define one source of truth for creature metadata.
* [x] Avoid parallel custom-creature tables by constructing runtime metadata directly from the registry.
* [x] Port the seven custom creatures with indexed fallbacks.
* [x] Port hero specialties as a separate gameplay slice.
* [x] Port the MP2/MX2 importer as a separate slice.

### SDL3 platform port

* [~] Port upstream behavior to SDL3 with minimal renderer changes.
* [x] Validate Linux input, audio, windowing and lifecycle behavior.
* [~] Validate Android input, audio, packaging and lifecycle behavior.
* [x] Remove source-tree writes from Android asset generation; no shader generation is present in this rebuild.

### RGBA renderer

* [x] Specify logical and physical coordinate contracts.
* [x] Add opt-in physical-resolution RGBA output behind tested interfaces.
* [~] Implement painter ordering and alpha behavior: the RGBA compositor is tested; world draw-call migration remains.
* [x] Validate deterministic synthetic RGBA output at 1x, 2x and 3x.
* [ ] Remove obsolete indexed-mask GPU paths after compatibility tests pass.

### High-resolution runtime assets

* [x] Define the exported asset manifest shared with the art project.
* [ ] Validate manifests and custom PNG files during the build or packaging step.
* [ ] Implement consistent runtime lookup and indexed fallback behavior.
* [ ] Package the accepted assets reproducibly on Linux and Android.

### Separate art project

* [ ] Choose the new repository name and license metadata.
* [ ] Extract the PySide tool with useful history where practical.
* [ ] Add a reproducible Python dependency definition.
* [ ] Add provider mocks and headless export tests.
* [ ] Improve alpha cleanup and native-resolution editing after extraction.
* [ ] Keep prompts, working images and rejected variants outside the game repository.

## Session handoff

Last updated: 2026-09-20

Current session sequence:

1. Merged hero specialties through PR #12 after verifying all completed CI checks passed.
2. Implement and validate the opt-in Android SDL3 build and remove Android asset generation writes from the source tree.
3. Define the exported asset manifest and logical/physical coordinate contracts.
4. Implement physical-resolution RGBA output with painter-order, alpha, clipping and 1x/2x/3x synthetic tests in separate changes.

Android implementation and automated validation precede the contract/renderer work. Hardware-only validation is deferred at the user's request and does
not block build checks. The RGBA branch is stacked on the Android branch to keep the changes separately reviewable without claiming device validation.

This explicitly prioritizes completing the SDL3 platform slice over the previous manifest-first handoff. Linux and Android are the primary platforms.

Completed in the current session:

* Merged hero specialties through PR #12 after checking all CI results.
* Defined version-1 asset export/validation requirements and the logical/physical coordinate and alpha contracts.
* Added opt-in physical RGBA presentation, nearest resampling, source-over composition, padded-source views and clipped ordered drawing.
* Verified native-detail texture readback at 1x/2x/3x and game viewport resizing using synthetic SDL output.
* Built and verified the RGBA Android APK/AAB for ARM64, ARMv7, x86 and x86_64; app lint and all 121 packaged asset digests passed.
* The compositor currently receives the indexed compatibility frame first; runtime manifest loading and individual world-sprite migration are not implemented.
* Added the opt-in Android SDL3 CMake/Gradle build and shared immutable SDL/SDL_mixer revisions with Linux.
* Built and checked ARM64/x86_64 APK and AAB output, Java/native version consistency and native SDL_main export.
* Moved Android asset staging and deterministic digests into build directories; added APK architecture/library/digest checks and CI.
* Adapted Android storage/orientation APIs, corrected letterboxed touch mapping and handled canceled gestures without creating clicks.
* Passed fresh warning-as-error Linux builds, seven SDL2 tests, nine SDL3 compatibility tests and ten physical RGBA tests, including touch cancellation/recovery.
* SDL3 and SDL2 Android app lint passed. The RGBA compositor passed AddressSanitizer/UndefinedBehaviorSanitizer checks (leak detection unavailable in the sandbox).
* Device-only input, audio, suspend/resume and activity recreation checks are explicitly deferred until a later device playtest.
* Review is split into draft PR #13 (Android) and dependent draft PR #14 (contracts/RGBA). Linux and Android SDL3 CI passed with RGBA both enabled and disabled.
* Guarded the SDL3/RGBA-only test consistently with the existing SDL3 tests so SDL2 static analysis does not require SDL3 headers.
* The existing macOS Intel Make CI job fails during Homebrew installation because the current installer rejects Intel macOS; no game compilation occurs.

Completed in previous sessions:

* Ported all 19 non-empty hero specialty definitions into game-owned source without the preserved generator, RGBA headers, renderer changes or artwork.
* Applied unit attack, defense and speed bonuses through the current adventure and battle stat paths, and included attack/defense bonuses in strategic
  army strength so AI evaluation observes them without double-counting.
* Applied specialty spell effectiveness to battle damage, spell descriptions, AI estimates, healing and resurrection; applied final spell-point reductions
  with a one-point minimum and guaranteed each spell-specialist's spell on hero creation and save loading.
* Added daily wood, crystal and gem specialties to the existing hero-income pass exactly once and exposed localized, deterministic specialty descriptions
  by clicking the existing indexed hero portrait.
* Kept specialty state out of serialization, used only stable registry IDs for all custom-creature chains and added exact data-free coverage for every
  preserved unit, spell and resource definition plus invalid IDs and legacy-ID isolation.
* Merged the custom-creature gameplay slice through pull request #11 and corrected the playtest regressions found in Dragon Rider and Dragon's Eye:
  random-monster fallback IDs, omitted legacy artifact/monster metadata and high-bit building-mask handling.
* Removed legacy-ID substitutions from normal runtime lookup so Wolf and the other upstream creatures cannot be replaced by custom creatures.
* Connected all seven registry creatures to their preserved upgrade predecessors, race-specific castle dwellings, weekly growth, recruitment,
  construction requirements and prices, AI construction and reinforcement priorities, and castle/editor building progression.
* Added the custom creatures to direct monster selection while keeping them out of generic random-monster pools and preserving existing
  random-placeholder IDs and editor object indices.
* Extended building masks and serialization to 64 bits, bumped the FH2M format to version 14, retained loading for both upstream 32-bit and FK
  Extended Edition 64-bit version 13 castle metadata and older save files, and round-tripped high-bit custom dwelling IDs.
* Reused existing indexed creature, portrait and castle art as deterministic fallbacks; dedicated RGBA artwork remains a separate runtime-assets slice.
* Reserved stable Extended Edition creature IDs at `0x00010000` through `0x00010006` without changing upstream creature or random-placeholder IDs.
* Added one declarative registry for Azure Dragon, Blood Dragon, Thor, Avenger, Succubus, Dachshund and Maid, including original stats, abilities,
  costs, sounds and indexed fallback resources.
* Kept the ambiguous `FK/Azure-Dragon` IDs available only through an explicit legacy lookup so normal random placeholders are never reinterpreted.
* Added a data-free registry and signed-32-bit serialization round-trip test.
* Replaced the Linux SDL2-compat bridge with direct SDL3 API use while leaving SDL2 as the default build.
* Ported native SDL3 lifecycle, event, keyboard, mouse, gamepad, touch, cursor, display, fullscreen and window-capture behavior.
* Preserved the indexed game image and existing palette-to-32-bit screen upload instead of introducing RGBA or physical-resolution rendering.
* Replaced the temporary SDL3 audio stub with native SDL3_mixer tracks for sound channels and music while leaving the default SDL2/SDL2_mixer path unchanged.
* Added a data-free initialization test that uses SDL's dummy audio driver and verifies that the engine creates native SDL3_mixer channels.
* Completed a local Linux playthrough setup with legally installed HoMM II Gold data and native Wayland/PipeWire backends.
* Fixed SDL3 mouse motion and button coordinates when logical rendering is scaled, including the 640x480-at-3x mode.
* Ported the MP2/MX2-to-FH2M editor importer against the current map format, including original maps in the editor selection dialog.
* Added reverse lookup from original ICN main sprites to editor object groups and a data-free registry regression test.
* Initialized importer player state before loading original maps and preserved legacy objects whose non-action parts are clipped at map boundaries.
* Preserved castles whose entrance tile is occupied by a hero, and reconstruct only castle-owned flags so removed hero sprites cannot leave standalone flags.
* Added semantic import validation for source hero/castle positions, castle flag ownership, adjacency and color pairing, including the occupied-castle regression.
* Reconstructed roads from source road-tile connectivity because road variants intentionally share main sprites while containing different neighboring parts.
* Strengthened registry validation to compare complete object definitions, including ground/top parts and layers,
  while explicitly covering the special road and mine variants.
* Preserved timed daily events, fixed shrine/witch-hut/pyramid selections and the original Ultimate Artifact editor marker, including its radius and artifact choice.
* Preserved source UIDs for generic objects and castles so equal-layer reconstruction retains the authored placement order against nearby scenery.
* Preserved editor placeholders without evaluating them during import: random heroes, towns/castles, monster tiers, resources and artifact tiers.
* Made placeholder import follow the declared MP2 object category even when a legacy map stores a mismatched placeholder sprite.
* Avoided a false SDL hint error when the environment already disables fullscreen minimize-on-focus-loss behavior.

Validation:

* Warning-as-error SDL2 and native SDL3 builds completed with the full `fheroes2` executable.
* All six SDL2 data-free tests and all eight SDL3 data-free tests passed, including hero specialties, custom registry, editor-object lookup and FH2M
  metadata round trips.
* The registry test now covers unique upgrade predecessors and dwellings, fixed upgrade prices and exact 64-bit dwelling serialization.
* The editor-object test verifies that all custom creatures are selectable and that the five existing random-monster object indices did not move.
* Code-format, copyright-header, whitespace and tracked-asset checks passed; no proprietary or generated game-data asset was added.
* The custom-creature registry test validates stable and legacy IDs, unique keys, representative source metadata, fallback mappings, placeholder
  isolation and exact serialization round trips.
* A local native SDL3 configuration compiled the complete `fheroes2` executable with warnings treated as errors.
* The local native SDL3 build passed the synthetic renderer, SDL3 runtime and SDL3_mixer initialization tests.
* The SDL3 dependency graph contains native SDL3 and SDL3_mixer without SDL2-compat or SDL2_mixer.
* A local full-data launch verified window creation, input and external music playback; 3x logical scaling was confirmed interactively.
* All 97 MP2/MX2 maps in the local GOG HoMM II Gold installation completed importer conversion,
  semantic object/road/artifact/placeholder/metadata validation and editor reconstruction in isolated processes.
* Representative MP2 and MX2 maps completed import, FH2M serialization, reload and editor reconstruction roundtrips.
* Warning-as-error SDL2 and native SDL3 builds completed; all data-free tests and the tracked-asset guard passed.
* The existing CI job still builds the full game and data-free tests; the normal pull-request matrix verifies the unchanged SDL2 platforms.
* No image, audio, map or original game-data file is part of this change or its validation.

Findings:

* Seven additional dwelling variants exhaust the 32-bit building mask, so construction, payment, editor metadata and save paths must consistently use
  64-bit masks; implicit narrowing can otherwise make custom buildings free or remove their prerequisites.
* Custom editor creatures intentionally share upstream MONS32 sprites as indexed fallbacks. They therefore need stable metadata IDs, while sprite-only
  reverse lookup continues to resolve the original upstream object.
* Appending custom editor objects after the existing random placeholders preserves FH2M group/index compatibility without a map conversion.
* SDL3 can retain the current indexed renderer contract by converting palette indexes into an RGBA32 SDL surface before texture upload.
* SDL3 display IDs, event types, gamepads, cursor visibility and surface metadata need explicit adaptation; SDL's old-name diagnostics are not a
  compatibility API.
* SDL3_mixer 3 uses persistent tracks instead of SDL2_mixer's numbered-channel and global-music APIs; the engine now owns that mapping explicitly.
* SDL3 no longer transforms pointer events to renderer-logical coordinates automatically; the event loop must explicitly call
  `SDL_ConvertEventToRenderCoordinates()`.
* The original map loader expects `Settings` and `Players` to describe the selected map before world initialization; an importer cannot call it in isolation.
* Original Editor maps may legally place an action object at the map edge with decorative or non-action constituent sprites outside the map boundary.

Best next step:

* Finish automated build/static-analysis checks; device availability is not a blocker for this stage.
* Next implementation slice: manifest validator and a shared ordered world-sprite submission path, preserving cursor/UI ordering over native artwork.
* Later device playtest: use the prepared Android SDL3 and RGBA APKs to validate touch, audio, background/resume and activity recreation.
