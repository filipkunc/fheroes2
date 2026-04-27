/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2021 - 2024                                             *
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

#include "core.h"

#include <cassert>
#include <cstdint>
#include <stdexcept>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL3/SDL.h>
#include <SDL3/SDL_error.h>
#include <SDL3/SDL_init.h>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#include "audio.h"
#include "localevent.h"
#include "logging.h"

namespace
{
    void initHardwareInternally()
    {
        // Do nothing.
    }

    void freeHardwareInternally()
    {
        // Do nothing.
    }

    SDL_InitFlags convertToSDLFlag( const fheroes2::SystemInitializationComponent component )
    {
        switch ( component ) {
        case fheroes2::SystemInitializationComponent::Audio:
            return SDL_INIT_AUDIO;
        case fheroes2::SystemInitializationComponent::Video:
            return SDL_INIT_VIDEO;
        case fheroes2::SystemInitializationComponent::GameController:
            return SDL_INIT_GAMEPAD;
        default:
            // Did you add a new component?
            assert( 0 );
            break;
        }

        return 0;
    }

    SDL_InitFlags getSDLInitFlags( const std::set<fheroes2::SystemInitializationComponent> & components )
    {
        SDL_InitFlags flags = 0;
        for ( const fheroes2::SystemInitializationComponent component : components ) {
            flags |= convertToSDLFlag( component );
        }
        return flags;
    }

    // For now only SDL library is supported.
    bool initCoreInternally( const std::set<fheroes2::SystemInitializationComponent> & components )
    {
        const SDL_InitFlags sdlFlags = getSDLInitFlags( components );

        if ( !SDL_Init( sdlFlags ) ) {
            ERROR_LOG( SDL_GetError() )
            return false;
        }

        if ( components.count( fheroes2::SystemInitializationComponent::Audio ) > 0 ) {
            Audio::Init();
        }

        if ( components.count( fheroes2::SystemInitializationComponent::GameController ) > 0 ) {
            LocalEvent::Get().initController();
        }

        LocalEvent::initEventEngine();

        return true;
    }

    void freeCoreInternally()
    {
        if ( fheroes2::isComponentInitialized( fheroes2::SystemInitializationComponent::GameController ) ) {
            LocalEvent::Get().CloseController();
        }

        if ( fheroes2::isComponentInitialized( fheroes2::SystemInitializationComponent::Audio ) ) {
            Audio::Quit();
        }

        SDL_Quit();
    }

    bool isComponentInitializedInternally( const fheroes2::SystemInitializationComponent component )
    {
        const SDL_InitFlags sdlFlag = convertToSDLFlag( component );
        assert( sdlFlag != 0 );

        return SDL_WasInit( sdlFlag ) != 0;
    }
}

namespace fheroes2
{
    HardwareInitializer::HardwareInitializer()
    {
        initHardwareInternally();
    }

    HardwareInitializer::~HardwareInitializer()
    {
        freeHardwareInternally();
    }

    CoreInitializer::CoreInitializer( const std::set<SystemInitializationComponent> & components )
    {
        if ( !initCoreInternally( components ) ) {
            throw std::logic_error( "Core module initialization failed." );
        }
    }

    CoreInitializer::~CoreInitializer()
    {
        freeCoreInternally();
    }

    bool isComponentInitialized( const SystemInitializationComponent component )
    {
        return isComponentInitializedInternally( component );
    }
}
