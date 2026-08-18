# Extended Edition testing strategy

## Core rule

Every public CI test must run from a clean checkout without Ubisoft assets. Rendering fixtures must use procedurally generated synthetic data or approved custom artwork.

Public CI must not:

* download the original game or demo data
* commit extracted original assets
* include original artwork in golden images or test screenshots
* upload logs, screenshots, archives or other artifacts containing original data

## Testing layers

| Layer | Purpose | Data used in public CI |
| --- | --- | --- |
| Unit tests | Algorithms, tables, serialization and manifest parsing | Programmatic values and small text fixtures |
| Renderer tests | Blending, clipping, scaling, painter order and coordinate conversion | Procedurally generated RGBA images |
| Asset validation | Dimensions, alpha, frames, offsets and manifest consistency | Accepted custom artwork only |
| Visual regression | Stable output at 1x, 2x and 3x | Synthetic scenes and custom art on synthetic backgrounds |
| Full-data smoke test | Compatibility with an installed game | Local developer data only |

Artistic quality is not an automated pass or fail condition. CI can generate contact sheets containing only distributable custom artwork for human review.

## Synthetic renderer fixtures

Renderer fixtures should be generated in test code so their expected behavior is obvious. The initial set should cover:

* opaque overlapping rectangles that prove painter order
* transparent gradients and hard alpha edges
* sprites clipped on every display edge
* source and destination rectangles with non-default stride
* logical-to-physical coordinate conversion at 1x, 2x and 3x
* nearest-neighbor scaling for pixel-oriented content
* full-color scaling for high-resolution content
* palette cycling represented by synthetic tagged pixels, while that compatibility path exists
* cursor and UI overlays on a synthetic background

Golden images must be deterministic. Tests should fix dimensions, scale, timing, animation frame, random seed and color-space assumptions. A golden update
should be a deliberate review action, not an automatic response to failure.

## Gameplay and asset checks

Custom creature tests should verify:

* stable IDs and save/load behavior
* upgrade relationships and dwelling mappings
* costs, statistics and abilities
* animation names, frame counts and offsets
* AI-facing values and lookup completeness
* indexed fallback behavior when high-resolution artwork is unavailable

The asset importer should reject incomplete manifests, unsafe paths, duplicate IDs, invalid dimensions and missing frames before packaging.

## Local full-data tests

Tests that need the original game may accept an explicit local path such as `FHEROES2_DATA_PATH`. They must skip cleanly when the path is absent.

These tests should default to local execution only. They must not upload screenshots or captured data. If private CI is added later, it needs an explicit
review of storage, logs, retention and artifact settings before original data is supplied.

## Initial public CI target

| Trigger | Linux | Android | Synthetic render tests |
| --- | ---: | ---: | ---: |
| Every pull request | Build and unit tests | Compile representative configuration | 1x, 2x and 3x |
| Significant renderer change | Build and unit tests | Compile representative configuration | Full fixture set |
| Art import change | Build and asset validation | Package validation | Custom-only contact sheet |

Paid or network-backed art generation is never run in CI. The separate art project should use provider mocks for automated tests.
