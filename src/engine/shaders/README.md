# SDL_GPU composite shaders

Phase 2 of the SDL3 + SDL_GPU migration ships precompiled DXIL bytecode for the
D3D12 backend on Windows. The HLSL sources live next to their compiled `.dxil`
output, both checked into git so a clean build does not require dxc.exe to be
installed.

CMake reads the `.dxil` files at configure time and embeds them into a
generated C++ header (`composite_dxil_embedded.h`) inside the build directory.
The engine `#include`s that generated header and feeds the bytes to
`SDL_CreateGPUShader`.

## Regenerating the DXIL bytecode

If you change the HLSL sources you must regenerate the bytecode. Run from the
repo root (substitute the actual Windows SDK version on your machine):

```bash
DXC="/c/Program Files (x86)/Windows Kits/10/bin/10.0.26100.0/x64/dxc.exe"
"$DXC" -nologo -T vs_6_0 -E main src/engine/shaders/composite.vert.hlsl -Fo src/engine/shaders/composite.vert.dxil
"$DXC" -nologo -T ps_6_0 -E main src/engine/shaders/composite.frag.hlsl -Fo src/engine/shaders/composite.frag.dxil
```

Then re-run CMake configure (`cmake --preset default`) so the embedded header
picks up the new bytecode. CMake `configure_file` runs at configure time, not
build time — a plain incremental rebuild won't notice the `.dxil` change.

## Cross-platform formats (Phase 6 work)

Vulkan (SPIR-V) and Metal (MSL) bytecode is not produced yet. When Phase 6
expands cross-platform support, additional `.spv` and `.msl` artifacts will sit
alongside the `.dxil` files and `screen_gpu.cpp` will pick whichever format
`SDL_GetGPUShaderFormats` reports as supported.
