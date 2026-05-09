// composite.vert.glsl
//
// SPIR-V counterpart of composite.vert.hlsl. Same parametric quad emitted from
// gl_VertexIndex, no vertex buffer needed. dst_rect_ndc gives the destination
// rectangle in NDC space.
//
// SDL_GPU coordinate convention (matches D3D12 / OpenGL): NDC Y is up, range
// [-1, +1]. The Vulkan backend internally flips Y so this shader produces
// identical output to composite.vert.hlsl regardless of platform.
//
// SDL_GPU SPIR-V binding convention:
//   Vertex shader uniform buffers : set = 1, binding = N
//
// Compile: glslang -V -S vert -o composite.vert.spv composite.vert.glsl

#version 450

layout( set = 1, binding = 0 ) uniform Xform
{
    vec4 dst_rect_ndc; // x, y, w, h in NDC
};

layout( location = 0 ) out vec2 v_uv;

void main()
{
    // Triangle strip, 4 vertices: BL, BR, TL, TR.
    vec2 corners[4] = vec2[4](
        vec2( 0.0, 0.0 ),
        vec2( 1.0, 0.0 ),
        vec2( 0.0, 1.0 ),
        vec2( 1.0, 1.0 )
    );
    vec2 c = corners[gl_VertexIndex];

    // (0,0) of the texture is the top row; map corner.y=1 (top of quad in NDC)
    // to v=0 so row 0 ends up at the top of the displayed quad.
    v_uv = vec2( c.x, 1.0 - c.y );
    gl_Position = vec4(
        dst_rect_ndc.x + c.x * dst_rect_ndc.z,
        dst_rect_ndc.y + c.y * dst_rect_ndc.w,
        0.0, 1.0
    );
}
