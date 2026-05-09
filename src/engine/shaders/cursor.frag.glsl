// cursor.frag.glsl
//
// SPIR-V counterpart of cursor.frag.hlsl. Plain RGBA pass-through used for the
// software cursor overlay (alpha-blended over the framebuffer composite pass).
//
// SDL_GPU SPIR-V binding convention:
//   Fragment shader sampled images : set = 2, binding = N
//
// Compile: glslang -V -S frag -o cursor.frag.spv cursor.frag.glsl

#version 450

layout( set = 2, binding = 0 ) uniform sampler2D rgba_sampler;

layout( location = 0 ) in vec2 v_uv;
layout( location = 0 ) out vec4 outColor;

void main()
{
    outColor = texture( rgba_sampler, v_uv );
}
