# Logical coordinates and physical RGBA rendering

## Coordinates

Game logic, UI layout, hit testing and sprite anchors use logical pixels. Rectangles are half-open: `[x, x + width)` and `[y, y + height)`.
SDL window coordinates are a separate domain, particularly on high-density displays. Input conversion uses SDL's renderer presentation transform once.
Touch is converted back to normalized logical coordinates for the existing gesture handler; coordinates in letterbox bars remain outside the game area.

The RGBA backing canvas covers the physical game viewport, excluding SDL-owned letterbox bars. Its dimensions come from the renderer's logical-presentation
rectangle in output pixels, never from the OS window size. A 640x480 game at exact 3x therefore has a 1920x1440 RGBA canvas. SDL handles the viewport's offset
within the window. Resizing, fullscreen changes and output-density changes recalculate the canvas dimensions on the next frame.

For a logical axis of length L and physical viewport axis of length P, an integer logical edge e maps to `floor(e * P / L)`.
Use the same formula for both edges, including negative positions, so adjacent rectangles share a boundary. Clip only after this mapping.
Nearest sampling selects source pixels at the centers of the unclipped physical destination pixels; clipping never stretches the remaining sprite.
Integer 1x, 2x and 3x output must be exact and deterministic. Noninteger scales use the same edge and sampling rules.

## Pixel and composition contract

Pixels are tightly represented as four bytes in R, G, B, A order, independent of host byte order. Row stride is explicit and can exceed width times four.
PNG input is straight-alpha sRGB. Initial composition uses integer source-over in that encoded color space to preserve predictable game-art behavior.
No implicit gamma conversion is applied. Fully transparent pixels do not change the destination; opaque pixels replace it exactly.

Draw calls execute in submission order. Later sprites cover earlier sprites according to alpha; no depth buffer, material sorting or indexed mask plane
changes that order. The frame begins opaque black. Indexed compatibility images expand through the current palette with opaque alpha before composition.
Palette cycling therefore updates the compatibility base each frame. RGBA overlays keep their own colors and alpha.

The initial implementation uses nearest sampling for deterministic pixel retention, including native-resolution 2x/3x sheets. A future smooth filter must
sample premultiplied colors to prevent transparent-edge halos and have its own golden tests; it must not silently alter this contract.

## Migration boundary

Physical RGBA presentation is opt-in and requires SDL3. The first implementation expands the existing indexed game frame into the physical canvas and
provides an ordered RGBA composition callback before upload. That callback is for renderer integration and synthetic verification, not an AI or asset service.
It allows full-color pixels to reach the physical output without being reduced to the logical resolution or original palette.

Existing game drawing remains indexed during this step. Replacing individual world sprites must subsequently move those draw operations into a shared ordered
composition path. Appending every high-resolution sprite as a final overlay would violate world painter order and is not the final asset integration design.
The indexed fallback, software cursor and palette cycling remain compatibility requirements. The renderer must be tested with original data locally before
becoming the default.

## Validation

Use synthetic, data-free tests for opaque overlap, partial alpha, transparent no-op, all four clipping edges, padded row stride, source rectangles,
negative origins, adjacent rectangles, invalid geometry and exact 1x/2x/3x output. Test that two distinct physical pixels inside one logical pixel survive
SDL texture upload and readback. Retain the SDL2 suite and mouse/touch coordinate tests. Device-only Android lifecycle validation is tracked separately in
`PORTING_STATUS.md`.
