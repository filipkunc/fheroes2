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

No gameplay or renderer behavior has been ported on this branch yet.

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

* [ ] Design stable custom creature identifiers and save compatibility.
* [ ] Define one source of truth for creature metadata.
* [ ] Generate repetitive C++ tables where this reduces missed integration points.
* [ ] Port the seven custom creatures with indexed fallbacks.
* [ ] Port hero specialties as a separate gameplay slice.
* [ ] Port the MP2/MX2 importer as a separate slice.

### SDL3 platform port

* [~] Port upstream behavior to SDL3 with minimal renderer changes.
* [ ] Validate Linux input, audio, windowing and lifecycle behavior.
* [ ] Validate Android input, audio, packaging and lifecycle behavior.
* [ ] Remove source-tree writes from shader or Android generation steps.

### RGBA renderer

* [ ] Specify logical and physical coordinate contracts.
* [ ] Add physical-resolution RGBA output behind tested interfaces.
* [ ] Implement painter ordering and alpha behavior.
* [ ] Validate deterministic output at 1x, 2x and 3x.
* [ ] Remove obsolete indexed-mask GPU paths after compatibility tests pass.

### High-resolution runtime assets

* [ ] Define the exported asset manifest shared with the art project.
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

Last updated: 2026-08-19

Completed in the latest session:

* Replaced the Linux SDL2-compat bridge with direct SDL3 API use while leaving SDL2 as the default build.
* Ported native SDL3 lifecycle, event, keyboard, mouse, gamepad, touch, cursor, display, fullscreen and window-capture behavior.
* Preserved the indexed game image and existing palette-to-32-bit screen upload instead of introducing RGBA or physical-resolution rendering.
* Replaced the temporary SDL3 audio stub with native SDL3_mixer tracks for sound channels and music while leaving the default SDL2/SDL2_mixer path unchanged.
* Added a data-free initialization test that uses SDL's dummy audio driver and verifies that the engine creates native SDL3_mixer channels.

Validation:

* A local native SDL3 configuration compiled the complete `fheroes2` executable with warnings treated as errors.
* The local native SDL3 build passed the synthetic renderer, SDL3 runtime and SDL3_mixer initialization tests.
* The SDL3 dependency graph contains native SDL3 and SDL3_mixer without SDL2-compat or SDL2_mixer.
* The existing CI job still builds the full game and data-free tests; the normal pull-request matrix verifies the unchanged SDL2 platforms.
* No image, audio, map or original game-data file is part of this change or its validation.

Findings:

* SDL3 can retain the current indexed renderer contract by converting palette indexes into an RGBA32 SDL surface before texture upload.
* SDL3 display IDs, event types, gamepads, cursor visibility and surface metadata need explicit adaptation; SDL's old-name diagnostics are not a
  compatibility API.
* SDL3_mixer 3 uses persistent tracks instead of SDL2_mixer's numbered-channel and global-music APIs; the engine now owns that mapping explicitly.

Best next step:

* Add synthetic in-memory sound playback coverage, then validate Linux input, audio, windowing and lifecycle behavior on real hardware before considering
  any change to the default SDL runtime.
