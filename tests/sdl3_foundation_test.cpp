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

#if defined( WITH_SDL3 )

#include <cstdlib>
#include <iostream>

#include <SDL3/SDL.h>

int main()
{
    if ( !SDL_Init( 0 ) ) {
        std::cerr << "Native SDL3 initialization failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    const int linkedVersion = SDL_GetVersion();
    SDL_Quit();

    if ( SDL_VERSIONNUM_MAJOR( linkedVersion ) != 3 ) {
        std::cerr << "Expected a native SDL3 runtime, got version " << SDL_VERSIONNUM_MAJOR( linkedVersion ) << '.' << SDL_VERSIONNUM_MINOR( linkedVersion ) << '.'
                  << SDL_VERSIONNUM_MICRO( linkedVersion ) << '\n';
        return EXIT_FAILURE;
    }

    std::cout << "Native SDL3 runtime initialized: " << SDL_VERSIONNUM_MAJOR( linkedVersion ) << '.' << SDL_VERSIONNUM_MINOR( linkedVersion ) << '.'
              << SDL_VERSIONNUM_MICRO( linkedVersion ) << '\n';
    return EXIT_SUCCESS;
}

#endif
