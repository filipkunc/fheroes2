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

#include <cstddef>
#include <cstdlib>
#include <iostream>
#include <vector>

#include <SDL3/SDL.h>

#include "rgba_frame.h"
#include "screen.h"

namespace
{
    bool testPhysicalTexture( const int scale )
    {
        const int size = 2 * scale;
        SDL_Window * window = SDL_CreateWindow( "Synthetic RGBA readback", size + 2, size, 0 );
        SDL_Renderer * renderer = window != nullptr ? SDL_CreateRenderer( window, "software" ) : nullptr;
        if ( renderer == nullptr ) {
            SDL_DestroyWindow( window );
            return false;
        }
        std::vector<fheroes2::RgbaPixel> nativePixels( static_cast<size_t>( size * size ) );
        for ( int y = 0; y < size; ++y ) {
            for ( int x = 0; x < size; ++x ) {
                nativePixels[y * size + x] = { static_cast<Uint8>( 17 + x * 23 ), static_cast<Uint8>( 13 + y * 19 ), 137, 255 };
            }
        }
        fheroes2::RgbaFrame frame;
        bool success = frame.resize( { 2, 2 }, { size, size } ) && frame.blit( { nativePixels.data(), size, size, size }, { 0, 0, size, size }, { 0, 0, 2, 2 } );
        SDL_Texture * texture = SDL_CreateTexture( renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, size, size );
        success = success && texture != nullptr && SDL_SetRenderLogicalPresentation( renderer, 2, 2, SDL_LOGICAL_PRESENTATION_LETTERBOX )
                  && SDL_SetTextureScaleMode( texture, SDL_SCALEMODE_NEAREST ) && SDL_SetTextureBlendMode( texture, SDL_BLENDMODE_NONE )
                  && SDL_UpdateTexture( texture, nullptr, frame.data(), frame.pitch() ) && SDL_SetRenderDrawColor( renderer, 0, 0, 0, 255 ) && SDL_RenderClear( renderer )
                  && SDL_RenderTexture( renderer, texture, nullptr, nullptr );
        // Readback covers the current game viewport; letterbox bars are outside it.
        SDL_Surface * readback = success ? SDL_RenderReadPixels( renderer, nullptr ) : nullptr;
        SDL_Surface * rgba = readback != nullptr ? SDL_ConvertSurface( readback, SDL_PIXELFORMAT_RGBA32 ) : nullptr;
        success = success && rgba != nullptr && rgba->w == size && rgba->h == size;
        if ( success ) {
            for ( int y = 0; y < size; ++y ) {
                const auto * row = reinterpret_cast<const fheroes2::RgbaPixel *>( static_cast<const Uint8 *>( rgba->pixels ) + y * rgba->pitch );
                for ( int x = 0; x < size; ++x ) {
                    const auto expected = nativePixels[y * size + x];
                    success = success && row[x].red == expected.red && row[x].green == expected.green && row[x].blue == expected.blue && row[x].alpha == expected.alpha;
                }
            }
        }
        if ( !success ) {
            std::cerr << "Texture readback mismatch at scale " << scale << "; output " << ( rgba != nullptr ? rgba->w : 0 ) << 'x' << ( rgba != nullptr ? rgba->h : 0 )
                      << '\n';
        }
        SDL_DestroySurface( rgba );
        SDL_DestroySurface( readback );
        SDL_DestroyTexture( texture );
        SDL_DestroyRenderer( renderer );
        SDL_DestroyWindow( window );
        return success;
    }

    bool testGamePresentation()
    {
        fheroes2::Display & display = fheroes2::Display::instance();
        display.setResolution( { 640, 480, 640, 480 } );
        if ( display.empty() ) {
            return false;
        }
        display.fill( 1 );
        int count = 0;
        SDL_Window ** windows = SDL_GetWindows( &count );
        SDL_Window * window = count == 1 ? windows[0] : nullptr;
        SDL_free( windows );
        if ( window == nullptr ) {
            display.release();
            return false;
        }
        bool success = true;
        for ( const int scale : { 1, 2, 3, 1 } ) {
            bool callbackRan = false;
            fheroes2::engine().setRgbaRenderCallback( [&]( fheroes2::RgbaFrame & frame ) {
                callbackRan = true;
                success = success && frame.width() == display.width() * scale && frame.height() == display.height() * scale;
                const fheroes2::RgbaPixel overlay{ 19, 137, 231, 255 };
                success = success && frame.blit( { &overlay, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) && frame.data()[0].red == 19 && frame.data()[0].green == 137
                          && frame.data()[0].blue == 231;
            } );
            success = success && SDL_SetWindowSize( window, display.width() * scale, display.height() * scale );
            SDL_PumpEvents();
            display.render();
            success = success && callbackRan;
            fheroes2::engine().setRgbaRenderCallback( {} );
        }
        if ( !success ) {
            std::cerr << "Game presentation callback/resize mismatch\n";
        }
        display.release();
        return success;
    }
}

int main()
{
    if ( !SDL_SetEnvironmentVariable( SDL_GetEnvironment(), "SDL_VIDEODRIVER", "dummy", true ) || !SDL_Init( SDL_INIT_VIDEO ) ) {
        std::cerr << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }
    const bool success = testPhysicalTexture( 1 ) && testPhysicalTexture( 2 ) && testPhysicalTexture( 3 ) && testGamePresentation();
    SDL_Quit();
    if ( !success ) {
        std::cerr << "Physical RGBA presentation/readback failed: " << SDL_GetError() << '\n';
        return EXIT_FAILURE;
    }
    std::cout << "Native detail survived 1x/2x/3x SDL presentation; game viewport resized correctly\n";
    return EXIT_SUCCESS;
}

#endif
