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

* Added an opt-in Linux CMake path with `-DUSE_SDL_VERSION=SDL3`.
* Pinned SDL 3.4.14, SDL2-compat 2.32.70 and SDL2_mixer 2.8.2 through CMake FetchContent while leaving SDL2 as the unchanged default.
* Kept the current SDL2 renderer and SDL2_mixer behavior behind SDL's official compatibility layer instead of importing the reference branch's RGBA,
  physical-resolution or platform-removal changes.
* Added a Linux CI job that compiles the game and synthetic tests on the SDL3 path.
* Added a data-free smoke test that initializes the compatibility runtime and rejects accidental linkage to the runner's system SDL2.

Validation:

* The default SDL2 configuration remains the existing CMake default and the existing platform workflows are unchanged.
* A local default SDL2 configuration built and passed the synthetic renderer test.
* A local SDL3 configuration built `fheroes2` and passed both the synthetic renderer and SDL3 runtime smoke tests.
* The tracked-file guard passes and the workflow YAML parses successfully.
* The change contains only CMake, CI, C++ test and documentation files; it adds no image, audio, map or original game data.

Findings:

* The preserved branch's native SDL3 port is mixed with renderer, physical-resolution and platform-scope changes, so it is not safe to transplant as a
  foundation commit.
* SDL2-compat provides a narrow runtime baseline for reviewing build and dependency behavior, but it is a bridge rather than the final native SDL3 API
  port.

Best next step:

* Replace the compatibility bridge with conditional native SDL3 platform code for Linux while preserving the current indexed renderer behavior and
  keeping SDL2 as the default until input, audio, windowing and lifecycle tests pass.
