# Runtime asset export contract, version 1

The art project exports `manifest.json` and accepted PNG sprite sheets. The game consumes this directory without running an art tool or contacting a service.
This contract precedes the runtime loader; the initial RGBA renderer continues to use indexed fallback artwork.

## Manifest

The root object contains `version: 1` and an `assets` array. Each asset has a unique stable string `id`, an optional signed-32-bit `creature_id`,
a logical frame size and anchor, named animation sequences, and one or more scale variants. Creature IDs must resolve through the custom-creature registry;
legacy FK IDs, upstream IDs inferred from sprite filenames, and enum positions are never accepted as custom IDs.

Example structure (the filename and checksum are illustrative, not a packaged asset):

```json
{
  "version": 1,
  "assets": [{
    "id": "creature.azure_dragon",
    "creature_id": 65536,
    "logical_size": [64, 48],
    "anchor": [32, 40],
    "animations": {"idle": [0, 1]},
    "variants": [{
      "scale": 2,
      "path": "creatures/azure_dragon/2x.png",
      "sha256": "0000000000000000000000000000000000000000000000000000000000000000",
      "size": [256, 96],
      "frames": [
        {"id": 0, "rect": [0, 0, 128, 96]},
        {"id": 1, "rect": [128, 0, 128, 96]}
      ]
    }]
  }]
}
```

All frame rectangles use PNG pixel coordinates. Logical frame size and anchor are integers shared by all variants. Drawing at logical position `(x, y)`
places the top-left of the frame at `(x - anchor.x, y - anchor.y)`. Transparent padding is retained; trimming cannot silently change placement.
Frame IDs are stable within an asset, and every variant must supply the same frame IDs. Animation lists refer to these IDs in playback order.
Timing remains game-owned in version 1. Scale is one of 1, 2 or 3 and is unique within an asset.

## Validation before packaging

The validator must reject the whole export on any failure and report the asset, field and reason:

* Reject unsupported versions, duplicate JSON keys, unknown fields, duplicate asset/frame IDs or scales, wrong types and missing required fields.
* Require nonempty asset IDs of lowercase ASCII letters, digits, underscores and dots; require all animation references to resolve.
* Require relative POSIX paths without `..`, empty components, backslashes, drive prefixes, absolute paths or symlinks. Resolve paths beneath the export root.
* Require each referenced file to exist and match its lowercase 64-digit SHA-256 digest. Reject unreferenced PNGs in the export.
* Decode PNG headers and pixels with bounded memory: dimensions must be positive and at most 16384 per side, with at most 64 million pixels per sheet.
* Require 8-bit RGBA, straight alpha and sRGB color values. Reject indexed PNGs, unsupported color profiles and animated PNGs.
* Check declared sheet dimensions against decoded dimensions. Frame rectangles must be positive, inside the sheet and equal to logical size times variant scale.
* Require logical sizes to be positive and at most 4096 per side; anchors may lie outside the frame but must be within signed 16-bit bounds.
* Validate creature references against the registry and reject multiple assets claiming the same creature's base sprite role.

A missing optional export causes indexed fallback. A malformed installed export is logged once and disabled as a unit; it must never produce partially
replaced creatures or change save IDs. Packaging fails on malformed exports instead of shipping that fallback accidentally.

## Variant choice and ownership

Choose the smallest available scale at least as large as the effective physical/logical scale; otherwise use the largest available variant.
At exact 2x or 3x, a matching variant must retain every source pixel. Filtering and alpha follow `RENDERING_CONTRACT.md`.
No accepted custom variant means the existing indexed sprite remains the deterministic fallback.

Both Linux and Android consume the same validated export. Sort manifest assets by ID and variants by scale when exporting for reproducible diffs.
Original game data, extracted original artwork, prompts, rejected generations and working images are outside this export.
