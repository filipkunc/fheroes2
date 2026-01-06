# Build fheroes2 using Visual Studio 2022
& "${env:ProgramFiles}\Microsoft Visual Studio\2022\Community\MSBuild\Current\Bin\MSBuild.exe" `
    "$PSScriptRoot\fheroes2-vs2019.vcxproj" `
    /p:Configuration=Release-SDL2 `
    /p:Platform=x64 `
    /p:PlatformToolset=v143 `
    /m
