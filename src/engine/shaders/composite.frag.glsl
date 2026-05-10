// composite.frag.glsl
//
// SPIR-V counterpart of composite.frag.hlsl. Phase 3 dual-channel composite
// (SDL_GPU). Each game pixel resolves through one of two paths:
//
//   mask == 255 -> "indexed pixel". idx is a palette index (already remapped
//                  through the engine's transformTable on the CPU if a shadow
//                  was applied). Output palette[idx].
//
//   mask == 0   -> "RGBA pixel". The CPU stores the shadow state in idx, which
//                  the GPU uses to dim the freshly painted RGBA without any
//                  CPU multiply:
//                    idx == 0          -> no shadow, output RGBA as-is.
//                    idx in [2..5]     -> shadow types matching the engine's
//                                         transform 2-5 with factors
//                                         {0.25, 0.40, 0.55, 0.70}.
//                    any other value   -> treated as "no shadow" (defensive).
//
// SDL_GPU SPIR-V binding convention:
//   Fragment shader sampled images : set = 2, binding = N
//
// Compile: glslang -V -S frag -o composite.frag.spv composite.frag.glsl

#version 450

// Painter's-algorithm shader (2026-05-10): the engine writes everything as RGBA
// at physical pixel scale, so the GPU just samples + outputs. Mask + palette LUT
// + shadow markers are gone — the engine applies palette resolution and shadow
// dimming on the CPU side via paletteIdxToRGBA / shadeRGBABlock as it writes.
//
// Slots 0 (gameSampler) and 2 (paletteSampler) are still bound from the engine
// for layout compatibility but ignored. They can be removed in a future cleanup
// pass once we're confident no other code path needs them.
layout( set = 2, binding = 0 ) uniform sampler2D gameSampler;
layout( set = 2, binding = 1 ) uniform sampler2D rgbaSampler;
layout( set = 2, binding = 2 ) uniform sampler2D paletteSampler;

layout( location = 0 ) in vec2 v_uv;
layout( location = 0 ) out vec4 outColor;

void main()
{
    outColor = texture( rgbaSampler, v_uv );
}
