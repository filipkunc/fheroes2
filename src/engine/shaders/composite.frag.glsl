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

layout( set = 2, binding = 0 ) uniform sampler2D indexedSampler;
layout( set = 2, binding = 1 ) uniform sampler2D rgbaSampler;
layout( set = 2, binding = 2 ) uniform sampler2D paletteSampler;
layout( set = 2, binding = 3 ) uniform sampler2D maskSampler;

layout( location = 0 ) in vec2 v_uv;
layout( location = 0 ) out vec4 outColor;

void main()
{
    float maskValid = texture( maskSampler, v_uv ).r;
    float idxNorm = texture( indexedSampler, v_uv ).r;
    float idxScaled = idxNorm * 255.0;

    if ( maskValid > 0.5 ) {
        // Indexed pixel - palette LUT lookup.
        float u = ( idxScaled + 0.5 ) / 256.0;
        outColor = texture( paletteSampler, vec2( u, 0.5 ) );
        return;
    }

    // RGBA pixel, possibly with shadow flag in idx.
    vec4 rgba = texture( rgbaSampler, v_uv );
    float factor = 1.0;
    if ( idxScaled > 1.5 && idxScaled < 5.5 ) {
        if ( idxScaled < 2.5 ) {
            factor = 0.25;
        }
        else if ( idxScaled < 3.5 ) {
            factor = 0.40;
        }
        else if ( idxScaled < 4.5 ) {
            factor = 0.55;
        }
        else {
            factor = 0.70;
        }
    }
    outColor = vec4( rgba.rgb * factor, rgba.a );
}
