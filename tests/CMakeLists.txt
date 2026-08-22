###########################################################################
#   fheroes2: https://github.com/ihhub/fheroes2                           #
#   Copyright (C) 2026                                                    #
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

add_executable(synthetic_renderer_test synthetic_renderer_test.cpp)
target_link_libraries(synthetic_renderer_test PRIVATE engine)

add_test(NAME synthetic_renderer COMMAND synthetic_renderer_test)

set(
	FHEROES2_TEST_INCLUDE_DIRS
	${PROJECT_SOURCE_DIR}/src/engine
	${PROJECT_SOURCE_DIR}/src/fheroes2/agg
	${PROJECT_SOURCE_DIR}/src/fheroes2/ai
	${PROJECT_SOURCE_DIR}/src/fheroes2/army
	${PROJECT_SOURCE_DIR}/src/fheroes2/audio
	${PROJECT_SOURCE_DIR}/src/fheroes2/battle
	${PROJECT_SOURCE_DIR}/src/fheroes2/campaign
	${PROJECT_SOURCE_DIR}/src/fheroes2/castle
	${PROJECT_SOURCE_DIR}/src/fheroes2/dialog
	${PROJECT_SOURCE_DIR}/src/fheroes2/editor
	${PROJECT_SOURCE_DIR}/src/fheroes2/game
	${PROJECT_SOURCE_DIR}/src/fheroes2/gui
	${PROJECT_SOURCE_DIR}/src/fheroes2/h2d
	${PROJECT_SOURCE_DIR}/src/fheroes2/heroes
	${PROJECT_SOURCE_DIR}/src/fheroes2/image
	${PROJECT_SOURCE_DIR}/src/fheroes2/kingdom
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps
	${PROJECT_SOURCE_DIR}/src/fheroes2/monster
	${PROJECT_SOURCE_DIR}/src/fheroes2/resource
	${PROJECT_SOURCE_DIR}/src/fheroes2/spell
	${PROJECT_SOURCE_DIR}/src/fheroes2/system
	${PROJECT_SOURCE_DIR}/src/fheroes2/world
	)

add_executable(
	map_object_import_lookup_test
	map_object_import_lookup_test.cpp
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps/map_object_info.cpp
	)
# Keep this standalone registry test independent of the full game runtime. Its checks do not rely on assert().
target_compile_definitions(map_object_import_lookup_test PRIVATE NDEBUG)
target_include_directories(
	map_object_import_lookup_test
	PRIVATE
	${FHEROES2_TEST_INCLUDE_DIRS}
	)

add_test(NAME map_object_import_lookup COMMAND map_object_import_lookup_test)

add_executable(
	map_format_import_validation_test
	map_format_import_validation_test.cpp
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps/map_format_importmp2_validation.cpp
	)
target_include_directories(map_format_import_validation_test PRIVATE ${FHEROES2_TEST_INCLUDE_DIRS})

add_test(NAME map_format_import_validation COMMAND map_format_import_validation_test)

add_executable(
	map_format_import_metadata_roundtrip_test
	map_format_import_metadata_roundtrip_test.cpp
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps/map_format_importmp2_metadata.cpp
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps/map_format_info.cpp
	${PROJECT_SOURCE_DIR}/src/fheroes2/maps/map_object_info.cpp
	)
# Keep this standalone registry test independent of the full game runtime. Its checks do not rely on assert().
target_compile_definitions(map_format_import_metadata_roundtrip_test PRIVATE NDEBUG)
target_include_directories(map_format_import_metadata_roundtrip_test PRIVATE ${FHEROES2_TEST_INCLUDE_DIRS})
target_link_libraries(map_format_import_metadata_roundtrip_test PRIVATE engine)

add_test(NAME map_format_import_metadata_roundtrip COMMAND map_format_import_metadata_roundtrip_test)

if(USE_SDL_VERSION STREQUAL "SDL3")
	add_executable(sdl3_foundation_test sdl3_foundation_test.cpp)
	target_compile_definitions(sdl3_foundation_test PRIVATE WITH_SDL3)
	target_link_libraries(sdl3_foundation_test PRIVATE ${FHEROES2_SDL_TARGET})
	add_executable(sdl3_audio_test sdl3_audio_test.cpp)
	target_compile_definitions(sdl3_audio_test PRIVATE WITH_SDL3)
	target_link_libraries(sdl3_audio_test PRIVATE engine)

	add_test(NAME sdl3_foundation COMMAND sdl3_foundation_test)
	add_test(NAME sdl3_audio COMMAND sdl3_audio_test)
endif()
