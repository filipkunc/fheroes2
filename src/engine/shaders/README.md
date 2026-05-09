# SDL_GPU composite shaders

The engine ships precompiled shader bytecode for both backends:

- **DXIL** (D3D12 / Windows). Source: `*.hlsl`. Compiled with Microsoft's `dxc.exe`.
- **SPIR-V** (Vulkan / Linux + Android). Source: `*.glsl`. Compiled with Khronos's `glslang`.

CMake reads whichever of the precompiled binaries (`.dxil` and `.spv`) exist
at configure time and embeds them as C arrays into a generated header
(`composite_shaders_embedded.h`) inside the build directory. `screen.cpp`
queries `SDL_GetGPUShaderFormats()` at runtime and feeds the matching blob to
`SDL_CreateGPUShader`.

A clean build does not require `dxc` or `glslang` to be installed — the
compiled binaries are checked into git. Install the toolchain only if you want
to modify the shader sources.

## Regenerating DXIL (Windows / D3D12)

If you change the HLSL sources you must regenerate the DXIL bytecode. Run from
the repo root (substitute the actual Windows SDK version on your machine):

```bash
DXC="/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
"$DXC" -nologo -T vs_6_0 -E main src/engine/shaders/composite.vert.hlsl -Fo src/engine/shaders/composite.vert.dxil
"$DXC" -nologo -T ps_6_0 -E main src/engine/shaders/composite.frag.hlsl -Fo src/engine/shaders/composite.frag.dxil
"$DXC" -nologo -T ps_6_0 -E main src/engine/shaders/cursor.frag.hlsl    -Fo src/engine/shaders/cursor.frag.dxil
```

## Regenerating SPIR-V (Linux + Android / Vulkan)

If you change the GLSL sources you must regenerate the SPIR-V bytecode. From
the repo root:

```bash
glslang -V -S vert -o src/engine/shaders/composite.vert.spv src/engine/shaders/composite.vert.glsl
glslang -V -S frag -o src/engine/shaders/composite.frag.spv src/engine/shaders/composite.frag.glsl
glslang -V -S frag -o src/engine/shaders/cursor.frag.spv    src/engine/shaders/cursor.frag.glsl
```

(Or `glslangValidator`, depending on your distro's package layout.)

The HLSL and GLSL sources are kept in sync by hand. They produce visually
identical output via the SDL_GPU coordinate-system normalisation (see comments
inside the .vert sources for details about Y-up NDC).

## After regenerating

Re-run CMake configure (`cmake -B build`) so the embedded header picks up the
new bytecode. CMake `configure_file` runs at configure time, not build time —
a plain incremental rebuild won't notice the binary change. (CMake's
`CMAKE_CONFIGURE_DEPENDS` does set up auto-reconfigure on these files; this is
just for the case where it doesn't trigger.)
