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

#include "localevent.h"
#include "sdl3_input.h"

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

    if ( !fheroes2::convertSDL3PointerEvent( renderer, event, 640, 480 ) || std::abs( event.motion.x - 320.0F ) > 0.001F || std::abs( event.motion.y - 240.0F ) > 0.001F
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

    if ( !fheroes2::convertSDL3PointerEvent( renderer, event, 640, 480 ) || std::abs( event.button.x - 500.0F ) > 0.001F
         || std::abs( event.button.y - 400.0F ) > 0.001F ) {
        std::cerr << "SDL3 did not convert 3x mouse button coordinates to the logical presentation: " << SDL_GetError() << '\n';
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        SDL_Quit();
        return EXIT_FAILURE;
    }

    // A wide Android window letterboxes a 4:3 game. Touch must follow the visible image,
    // not stretch over the bars; canceled contacts must follow the same conversion.
    if ( !SDL_SetWindowSize( window, 1920, 1080 ) ) {
        return EXIT_FAILURE;
    }
    SDL_PumpEvents();
    for ( const Uint32 type : { SDL_EVENT_FINGER_DOWN, SDL_EVENT_FINGER_MOTION, SDL_EVENT_FINGER_UP, SDL_EVENT_FINGER_CANCELED } ) {
        event = {};
        event.type = type;
        event.tfinger.windowID = SDL_GetWindowID( window );
        event.tfinger.x = 0.3125F; // x=600 in the window, x=160 in the logical image.
        event.tfinger.y = 0.25F;
        event.tfinger.dx = 0.075F;
        event.tfinger.dy = 0.1F;
        if ( !fheroes2::convertSDL3PointerEvent( renderer, event, 640, 480 ) || std::abs( event.tfinger.x - 0.25F ) > 0.001F
             || std::abs( event.tfinger.y - 0.25F ) > 0.001F || std::abs( event.tfinger.dx - 0.1F ) > 0.001F || std::abs( event.tfinger.dy - 0.1F ) > 0.001F ) {
            std::cerr << "SDL3 touch coordinates do not follow the letterboxed image\n";
            SDL_DestroyRenderer( renderer );
            SDL_DestroyWindow( window );
            SDL_Quit();
            return EXIT_FAILURE;
        }
    }

    LocalEvent::initEventEngine();
    LocalEvent & events = LocalEvent::Get();
    SDL_FlushEvents( SDL_EVENT_FIRST, SDL_EVENT_LAST );
    const auto sendTouch = [&events]( const Uint32 type ) {
        SDL_Event touch{};
        touch.type = type;
        touch.tfinger.touchID = 1;
        touch.tfinger.fingerID = 1;
        // No window is needed to exercise gesture state independently of coordinate mapping.
        if ( !SDL_PushEvent( &touch ) ) {
            return false;
        }
        // SDL may leave a poll sentinel after the previous event; process through that boundary.
        for ( int attempt = 0; attempt < 4 && SDL_HasEvent( type ); ++attempt ) {
            if ( !events.HandleEvents( false ) ) {
                return false;
            }
        }
        return !SDL_HasEvent( type );
    };
    if ( !sendTouch( SDL_EVENT_FINGER_DOWN ) || !events.isMouseLeftButtonPressed() || !sendTouch( SDL_EVENT_FINGER_CANCELED ) || events.isMouseLeftButtonPressed()
         || events.MouseClickLeft() || !sendTouch( SDL_EVENT_FINGER_DOWN ) || !events.isMouseLeftButtonPressed() || !sendTouch( SDL_EVENT_FINGER_UP )
         || events.isMouseLeftButtonPressed() || !events.MouseClickLeft() ) {
        std::cerr << "Canceling a touch must release the gesture without clicking and allow a new gesture\n";
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
