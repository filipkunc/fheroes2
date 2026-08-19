/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2026                                                    *
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************/

#include <cstdlib>
#include <iostream>

#include <SDL.h>

int main()
{
    if ( SDL_Init( 0 ) != 0 ) {
        std::cerr << "SDL3 compatibility initialization failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    SDL_version linkedVersion{};
    SDL_GetVersion( &linkedVersion );
    SDL_Quit();

    // Official SDL2-compat releases use the 2.32 version line. This prevents the
    // opt-in job from silently resolving the runner's system SDL2 library instead.
    if ( linkedVersion.major != 2 || linkedVersion.minor < 32 ) {
        std::cerr << "Expected SDL2-compat 2.32 or newer, got " << static_cast<int>( linkedVersion.major ) << '.'
                  << static_cast<int>( linkedVersion.minor ) << '.' << static_cast<int>( linkedVersion.patch ) << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "SDL3 foundation initialized through SDL2-compat " << static_cast<int>( linkedVersion.major ) << '.'
              << static_cast<int>( linkedVersion.minor ) << '.' << static_cast<int>( linkedVersion.patch ) << '\n';
    return EXIT_SUCCESS;
}
