# Extended Edition architecture

## Product definition

This is a personal extended edition of fheroes2, not a short-lived patch stack intended for upstream submission. Linux and Android are the primary
supported platforms. Upstream bug fixes remain important, so custom work should stay in understandable modules and be merged with upstream regularly.

The original implementation on `FK/Azure-Dragon` proves that the main ideas are viable. The rebuild should reuse its behavior and lessons without
carrying forward every experiment or accidental coupling.

## Repository boundary

The project is split into two repositories with a deterministic asset handoff.

| Game repository | Art tool repository |
| --- | --- |
| Runtime renderer and platform code | PySide editing application |
| Custom creature and gameplay definitions | AI provider integrations |
| Accepted runtime artwork | Prompts, masks and working images |
| Asset manifest reader and validator | Background removal and alpha cleanup |
| Linux and Android builds | Sprite-sheet assembly and previews |
| Synthetic rendering tests | Headless export tests and provider mocks |

The art tool exports accepted PNG files and a small manifest. The game validates and consumes that output. The game never invokes Gemini, FLUX,
ComfyUI, or another generator during configuration, building, testing, or normal use.

A Git submodule is not planned initially. It would unnecessarily couple game builds and Android checkouts to the tool history. The export format is the integration boundary.

## Runtime rendering target

The intended renderer has these properties:

* SDL3 provides platform integration.
* The final frame is full-color RGBA.
* Drawing follows painter order.
* The backing buffer uses the physical output resolution.
* Logical coordinates remain available for upstream game and UI code.
* Artwork can provide native 2x or 3x detail instead of being reduced to the original palette and resolution.

The rebuild should begin with the smallest SDL3 port that preserves upstream rendering behavior. RGBA and physical-resolution work should follow as
separate reviewable steps. Obsolete indexed-mask GPU experiments and inconsistent shader paths should not be carried forward unless a test demonstrates
that they remain necessary.

## Custom game data

Custom creature integration currently touches enums, parallel arrays, switches, dwelling flags, animation tables and resource lookup code. The rebuild
should define each custom creature once in a declarative manifest or table and generate repetitive C++ data where practical.

Stable identifiers and save compatibility require an explicit design before creature IDs are added. Indexed fallbacks may remain useful even when
high-resolution RGBA art is preferred.

Hero specialties and the MP2/MX2 importer are valuable but separate gameplay slices. They should not be bundled into renderer commits.

## Asset ownership boundary

The game repository may contain only assets that the project has chosen and is able to distribute, such as accepted custom artwork and synthetic test
fixtures. Original Ubisoft artwork and extracted game data are external user inputs.

The public availability of the original demo does not make its assets repository fixtures. Runtime compatibility with original data is tested locally
by developers who provide their own data path.

Working images, rejected generations, prompts, masks and generation metadata belong in the separate art project. The game repository should contain
only the smallest accepted runtime representation and any provenance record chosen for distribution.

## Branch strategy

* Preserve `FK/Azure-Dragon` as a read-only reference.
* Build the new edition from a current upstream commit.
* Use focused topic branches for gameplay, SDL3, RGBA rendering, asset loading and import tools.
* Merge upstream regularly instead of rebasing the long-lived edition history.
* Keep commits small enough that a regression can be isolated with `git bisect`.
* Update the migration documents whenever a technical decision changes.
