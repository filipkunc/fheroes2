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

if(USE_SDL_VERSION STREQUAL "SDL3")
	add_executable(sdl3_foundation_test sdl3_foundation_test.cpp)
	target_compile_definitions(sdl3_foundation_test PRIVATE WITH_SDL3)
	target_link_libraries(sdl3_foundation_test PRIVATE ${FHEROES2_SDL_TARGET})

	add_test(NAME sdl3_foundation COMMAND sdl3_foundation_test)
endif()
