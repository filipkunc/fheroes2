// composite.frag.hlsl
//
// Phase 3 dual-channel composite (SDL_GPU). Each game pixel resolves through
// one of two paths:
//
//   mask == 255 → "indexed pixel". idx is a palette index (already remapped
//                 through the original engine's transformTable on the CPU
//                 if a shadow was applied). Output palette[idx].
//
//   mask == 0   → "RGBA pixel". The CPU stores the shadow state in idx, which
//                 the GPU uses to dim the freshly painted RGBA without any
//                 CPU multiply:
//                   idx == 0          → no shadow, output RGBA as-is.
//                   idx in [2..5]     → shadow types matching the engine's
//                                       transform 2-5 with factors
//                                       {0.25, 0.40, 0.55, 0.70}.
//                   any other value   → treated as "no shadow" (defensive).
//
// Doing the dimming in-shader makes shadows idempotent: a primitive can
// re-write the same byte each frame without compounding multiplication.

Texture2D<float4> indexedTex     : register( t0, space2 );
SamplerState      indexedSampler : register( s0, space2 );
Texture2D<float4> rgbaTex        : register( t1, space2 );
SamplerState      rgbaSampler    : register( s1, space2 );
Texture2D<float4> paletteTex     : register( t2, space2 );
SamplerState      paletteSampler : register( s2, space2 );
Texture2D<float4> maskTex        : register( t3, space2 );
SamplerState      maskSampler    : register( s3, space2 );

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main( PSInput input ) : SV_TARGET
{
    const float maskValid = maskTex.Sample( maskSampler, input.uv ).r;
    const float idxNorm = indexedTex.Sample( indexedSampler, input.uv ).r;
    const float idxScaled = idxNorm * 255.0;

    if ( maskValid > 0.5 ) {
        // Indexed pixel — palette LUT lookup.
        const float u = ( idxScaled + 0.5 ) / 256.0;
        return paletteTex.Sample( paletteSampler, float2( u, 0.5 ) );
    }

    // RGBA pixel, possibly with shadow flag in idx.
    float4 rgba = rgbaTex.Sample( rgbaSampler, input.uv );
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
    return float4( rgba.rgb * factor, rgba.a );
}
