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

#include <cmath>
#include <cstdlib>
#include <iostream>

#include <SDL3/SDL.h>

int main()
{
    if ( !SDL_SetEnvironmentVariable( SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true ) ) {
        std::cerr << "Failed to select the dummy SDL3 video driver: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    if ( !SDL_Init( SDL_INIT_VIDEO ) ) {
        std::cerr << "Native SDL3 initialization failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }

    const int linkedVersion = SDL_GetVersion();
    if ( SDL_VERSIONNUM_MAJOR( linkedVersion ) != 3 ) {
        std::cerr << "Expected a native SDL3 runtime, got version " << SDL_VERSIONNUM_MAJOR( linkedVersion ) << '.' << SDL_VERSIONNUM_MINOR( linkedVersion ) << '.'
                  << SDL_VERSIONNUM_MICRO( linkedVersion ) << '\n';
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Window * window = SDL_CreateWindow( "SDL3 coordinate conversion test", 1920, 1440, 0 );
    SDL_Renderer * renderer = window != nullptr ? SDL_CreateRenderer( window, nullptr ) : nullptr;
    if ( renderer == nullptr ) {
        std::cerr << "Failed to create the SDL3 coordinate conversion test renderer: " << SDL_GetError() << '\n';
        SDL_DestroyWindow( window );
        SDL_Quit();
        return EXIT_FAILURE;
    }

    if ( !SDL_SetRenderLogicalPresentation( renderer, 640, 480, SDL_LOGICAL_PRESENTATION_LETTERBOX ) ) {
        std::cerr << "Failed to set the SDL3 logical presentation: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_Event event{};
    event.type = SDL_EVENT_MOUSE_MOTION;
    event.motion.windowID = SDL_GetWindowID( window );
    event.motion.x = 960.0F;
    event.motion.y = 720.0F;
    event.motion.xrel = 30.0F;
    event.motion.yrel = 30.0F;

    if ( !SDL_ConvertEventToRenderCoordinates( renderer, &event ) || std::abs( event.motion.x - 320.0F ) > 0.001F || std::abs( event.motion.y - 240.0F ) > 0.001F
         || std::abs( event.motion.xrel - 10.0F ) > 0.001F || std::abs( event.motion.yrel - 10.0F ) > 0.001F ) {
        std::cerr << "SDL3 did not convert 3x mouse motion coordinates to the logical presentation: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return EXIT_FAILURE;
    }

    event = {};
    event.type = SDL_EVENT_MOUSE_BUTTON_DOWN;
    event.button.windowID = SDL_GetWindowID( window );
    event.button.x = 1500.0F;
    event.button.y = 1200.0F;

    if ( !SDL_ConvertEventToRenderCoordinates( renderer, &event ) || std::abs( event.button.x - 500.0F ) > 0.001F || std::abs( event.button.y - 400.0F ) > 0.001F ) {
        std::cerr << "SDL3 did not convert 3x mouse button coordinates to the logical presentation: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return EXIT_FAILURE;
    }

    SDL_DestroyRenderer( renderer );
    SDL_DestroyWindow( window );
    SDL_Quit();

    std::cout << "Native SDL3 runtime initialized: " << SDL_VERSIONNUM_MAJOR( linkedVersion ) << '.' << SDL_VERSIONNUM_MINOR( linkedVersion ) << '.'
              << SDL_VERSIONNUM_MICRO( linkedVersion ) << '\n';
    return EXIT_SUCCESS;
}

#endif
