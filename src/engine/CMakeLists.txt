###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2022 - 2025                                             #
#                                                                         #
#   This program is free software; you can redistribute it and/or modify  #
#   it under the terms of the GNU General Public License as published by  #
#   the Free Software Foundation; either version 2 of the License, or     #
#   (at your option) any later version.                                   #
#                                                                         #
#   This program is distributed in the hope that it will be useful,       #
#   but WITHOUT ANY WARRANTY; without even the implied warranty of        #
#   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         #
#   GNU General Public License for more details.                          #
#                                                                         #
#   You should have received a copy of the GNU General Public License     #
#   along with this program; if not, write to the                         #
#   Free Software Foundation, Inc.,                                       #
#   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             #
###########################################################################

file(GLOB_RECURSE LIBENGINE_SOURCES CONFIGURE_DEPENDS *.cpp)

#
# SDL_GPU shader embedding (Phase 2 of the SDL3 + SDL_GPU migration).
#
# The HLSL sources in shaders/ are precompiled to DXIL via dxc.exe and the
# resulting .dxil binaries are checked into the source tree. At configure time
# we read those binaries and embed them as C arrays into a generated header
# (composite_dxil_embedded.h) under the build directory. screen.cpp (the GPU
# render path) #includes that header and feeds the bytes to SDL_CreateGPUShader.
#
# If you regenerate the .dxil files (see shaders/README.md), re-run CMake to
# refresh the embedded header — a plain incremental rebuild won't notice.
#
set(SHADER_SRC_DIR ${CMAKE_CURRENT_SOURCE_DIR}/shaders)
set(SHADER_GEN_HEADER ${CMAKE_CURRENT_BINARY_DIR}/composite_dxil_embedded.h)

# Force CMake to re-run when the .dxil files change so the embedded header
# stays in sync without a manual reconfigure.
set_property(
    DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS
    ${SHADER_SRC_DIR}/composite.vert.dxil
    ${SHADER_SRC_DIR}/composite.frag.dxil
    ${SHADER_SRC_DIR}/cursor.frag.dxil
    )

file(READ ${SHADER_SRC_DIR}/composite.vert.dxil COMPOSITE_VERT_HEX HEX)
file(READ ${SHADER_SRC_DIR}/composite.frag.dxil COMPOSITE_FRAG_HEX HEX)
file(READ ${SHADER_SRC_DIR}/cursor.frag.dxil CURSOR_FRAG_HEX HEX)
string(REGEX REPLACE "(..)" "0x\\1, " COMPOSITE_VERT_C_ARRAY "${COMPOSITE_VERT_HEX}")
string(REGEX REPLACE "(..)" "0x\\1, " COMPOSITE_FRAG_C_ARRAY "${COMPOSITE_FRAG_HEX}")
string(REGEX REPLACE "(..)" "0x\\1, " CURSOR_FRAG_C_ARRAY "${CURSOR_FRAG_HEX}")
configure_file(
    ${SHADER_SRC_DIR}/composite_dxil_embedded.h.in
    ${SHADER_GEN_HEADER}
    @ONLY
    )

add_compile_options("$<$<COMPILE_LANG_AND_ID:C,AppleClang,Clang,GNU>:${GNU_CC_WARN_OPTS}>")
add_compile_options("$<$<COMPILE_LANG_AND_ID:CXX,AppleClang,Clang,GNU>:${GNU_CXX_WARN_OPTS}>")
add_compile_options("$<$<OR:$<COMPILE_LANG_AND_ID:C,MSVC>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:${MSVC_CC_WARN_OPTS}>")

add_library(engine STATIC ${LIBENGINE_SOURCES})

target_include_directories(
    engine
    PRIVATE
    ${CMAKE_CURRENT_BINARY_DIR}
    )

target_compile_definitions(
	engine
	PRIVATE
	# MSVC: suppress deprecation warnings
	$<$<OR:$<COMPILE_LANG_AND_ID:C,MSVC>,$<COMPILE_LANG_AND_ID:CXX,MSVC>>:_CRT_SECURE_NO_WARNINGS>
	$<$<CONFIG:Debug>:WITH_DEBUG>
	$<$<BOOL:${ENABLE_IMAGE}>:WITH_IMAGE>
	$<$<BOOL:${MACOS_APP_BUNDLE}>:MACOS_APP_BUNDLE>
	)

target_include_directories(
	engine
	PUBLIC
	$<BUILD_INTERFACE:${CMAKE_CURRENT_SOURCE_DIR}>
	)

target_link_libraries(
	engine
	smacker
	SDL3::SDL3
	SDL3_mixer::SDL3_mixer
	$<$<BOOL:${ENABLE_IMAGE}>:SDL3_image::SDL3_image>
	$<$<BOOL:${ENABLE_IMAGE}>:PNG::PNG>
	Threads::Threads
	ZLIB::ZLIB
	)
