// composite.vert.hlsl
//
// Phase 2 (SDL_GPU): vertex shader emitting a parametric quad as a 4-vertex
// triangle strip using SV_VertexID, no vertex buffer needed. dst_rect_ndc gives
// the destination rectangle in NDC space (NDC ranges from (-1,-1) bottom-left
// to (+1,+1) top-right under D3D conventions, which SDL_GPU normalizes across
// backends). Texture coordinates assume (0,0) at the top-left of the source
// texture so v is flipped relative to clip-space y.
//
// SDL_GPU resource binding conventions (from SDL_gpu.h docs):
//   For vertex shaders, uniform buffers go at (b[n], space1).

cbuffer Xform : register( b0, space1 )
{
    float4 dst_rect_ndc; // x, y, w, h in NDC
};

struct VSOutput
{
    float4 pos : SV_POSITION;
    float2 uv  : TEXCOORD0;
};

VSOutput main( uint vid : SV_VertexID )
{
    // Triangle strip, 4 vertices: BL, BR, TL, TR.
    float2 corners[4] = {
        float2( 0, 0 ),
        float2( 1, 0 ),
        float2( 0, 1 ),
        float2( 1, 1 )
    };
    float2 c = corners[vid];

    VSOutput o;
    // (0,0) of the texture is the top row; map corner.y=1 (top of quad in NDC)
    // to v=0 so row 0 ends up at the top of the displayed quad.
    o.uv = float2( c.x, 1.0 - c.y );
    o.pos = float4( dst_rect_ndc.x + c.x * dst_rect_ndc.z,
                    dst_rect_ndc.y + c.y * dst_rect_ndc.w,
                    0.0, 1.0 );
    return o;
}
