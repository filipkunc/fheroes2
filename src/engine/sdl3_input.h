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

#pragma once

#if defined( WITH_SDL3 )

#include <SDL3/SDL.h>

namespace fheroes2
{
    // Preserve the engine's normalized touch contract after mapping out window letterboxing.
    inline bool convertSDL3PointerEvent( SDL_Renderer * renderer, SDL_Event & event, const int logicalWidth, const int logicalHeight )
    {
        const bool isTouch = event.type == SDL_EVENT_FINGER_DOWN || event.type == SDL_EVENT_FINGER_UP || event.type == SDL_EVENT_FINGER_MOTION
                             || event.type == SDL_EVENT_FINGER_CANCELED;
        if ( !isTouch && event.type != SDL_EVENT_MOUSE_MOTION && event.type != SDL_EVENT_MOUSE_BUTTON_DOWN && event.type != SDL_EVENT_MOUSE_BUTTON_UP ) {
            return true;
        }
        if ( renderer == nullptr || logicalWidth <= 0 || logicalHeight <= 0 || !SDL_ConvertEventToRenderCoordinates( renderer, &event ) ) {
            return false;
        }
        if ( isTouch ) {
            event.tfinger.x /= static_cast<float>( logicalWidth );
            event.tfinger.y /= static_cast<float>( logicalHeight );
            event.tfinger.dx /= static_cast<float>( logicalWidth );
            event.tfinger.dy /= static_cast<float>( logicalHeight );
        }
        return true;
    }
}

#endif
