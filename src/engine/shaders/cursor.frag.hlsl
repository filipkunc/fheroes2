// cursor.frag.hlsl
//
// Plain RGBA pass-through used for the software cursor overlay (alpha-blended
// over the framebuffer composite pass). Same trivial path the Phase 2
// framebuffer shader used; kept as a dedicated shader so the framebuffer
// composite can hold the 3-sampler dual-channel layout from Phase 3 onward.
//
// SDL_GPU bindings:
//   t0/s0, space2 : RGBA cursor texture, sampled with the framebuffer's sampler.

Texture2D<float4> rgba_tex     : register( t0, space2 );
SamplerState      rgba_sampler : register( s0, space2 );

struct PSInput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

float4 main( PSInput input ) : SV_TARGET
{
    return rgba_tex.Sample( rgba_sampler, input.uv );
}
