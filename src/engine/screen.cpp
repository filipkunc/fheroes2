/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2025                                             *
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

#include <algorithm>
#include <cassert>
#include <cmath>
#include <cstdint>
#include <cstring>
#include <iterator>
#include <optional>
#include <vector>
#include <ostream>
#include <set>
#include <utility>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_events.h>
#include <SDL3/SDL_hints.h>
#include <SDL3/SDL_mouse.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_rect.h>
#include <SDL3/SDL_render.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>
#include <SDL3/SDL_video.h>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#include "image_palette.h"
#include "logging.h"
#include "math_tools.h"
#include "screen.h"
#include "system.h"

namespace
{
    // Returns nearest screen supported resolution
    fheroes2::ResolutionInfo GetNearestResolution( fheroes2::ResolutionInfo resolutionInfo, const std::vector<fheroes2::ResolutionInfo> & resolutions )
    {
        if ( resolutions.empty() ) {
            return resolutionInfo;
        }

        if ( resolutionInfo.gameWidth < 1 ) {
            resolutionInfo.gameWidth = 1;
        }

        if ( resolutionInfo.gameHeight < 1 ) {
            resolutionInfo.gameHeight = 1;
        }

        if ( resolutionInfo.screenWidth < resolutionInfo.gameWidth ) {
            resolutionInfo.screenWidth = resolutionInfo.gameWidth;
        }

        if ( resolutionInfo.screenHeight < resolutionInfo.gameHeight ) {
            resolutionInfo.screenHeight = resolutionInfo.gameHeight;
        }

        const double gameX = resolutionInfo.gameWidth;
        const double gameY = resolutionInfo.gameHeight;
        const double screenX = resolutionInfo.screenWidth;
        const double screenY = resolutionInfo.screenHeight;

        std::vector<double> similarity( resolutions.size(), 0 );
        for ( size_t i = 0; i < resolutions.size(); ++i ) {
            similarity[i] = std::fabs( resolutions[i].gameWidth - gameX ) / gameX + std::fabs( resolutions[i].gameHeight - gameY ) / gameY
                            + std::fabs( resolutions[i].screenWidth - screenX ) / screenX + std::fabs( resolutions[i].screenHeight - screenY ) / screenY;
        }

        const std::vector<double>::difference_type id = std::distance( similarity.begin(), std::min_element( similarity.begin(), similarity.end() ) );

        return resolutions[id];
    }

    bool IsLowerThanDefaultRes( const fheroes2::ResolutionInfo & value )
    {
        return value.gameWidth < fheroes2::Display::DEFAULT_WIDTH || value.gameHeight < fheroes2::Display::DEFAULT_HEIGHT;
    }

    std::set<fheroes2::ResolutionInfo> FilterResolutions( const std::set<fheroes2::ResolutionInfo> & resolutionSet )
    {
        static_assert( fheroes2::Display::DEFAULT_WIDTH == 640 && fheroes2::Display::DEFAULT_HEIGHT == 480, "Default resolution must be 640 x 480" );

        if ( resolutionSet.empty() ) {
            return { { fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT } };
        }

        std::set<fheroes2::ResolutionInfo> resolutions;

        for ( const fheroes2::ResolutionInfo & resolution : resolutionSet ) {
            if ( IsLowerThanDefaultRes( resolution ) ) {
                continue;
            }

            resolutions.emplace( resolution );
        }

        if ( resolutions.empty() ) {
            return { { fheroes2::Display::DEFAULT_WIDTH, fheroes2::Display::DEFAULT_HEIGHT } };
        }

        {
            // TODO: add resolutions which are close to the current screen aspect ratio.
            // Some operating systems do not work well with SDL so they return very limited number of high resolutions.
            // Populate missing resolutions into the list.
            const std::set<fheroes2::ResolutionInfo> possibleResolutions
                = { { 640, 480 },   { 800, 600 },  { 1024, 768 },  { 1152, 864 }, { 1280, 600 }, { 1280, 720 },  { 1280, 768 }, { 1280, 960 },
                    { 1280, 1024 }, { 1360, 768 }, { 1400, 1050 }, { 1440, 900 }, { 1600, 900 }, { 1680, 1050 }, { 1920, 1080 } };

            assert( !resolutions.empty() );
            const fheroes2::ResolutionInfo lowestResolution = *( resolutions.begin() );

            for ( const fheroes2::ResolutionInfo & resolution : possibleResolutions ) {
                if ( lowestResolution.gameWidth < resolution.gameWidth || lowestResolution.gameHeight < resolution.gameHeight || lowestResolution == resolution ) {
                    continue;
                }

                resolutions.emplace( resolution );
            }
        }

        std::set<fheroes2::ResolutionInfo> scaledResolutions;

        {
            // Widescreen devices support much higher resolutions but items on such resolutions are too tiny. In order to improve user
            // experience on these devices we are adding a special non-standard resolution with (potentially) non-integer scale.
            assert( !resolutions.empty() );
            const fheroes2::ResolutionInfo biggestResolution = *( std::prev( resolutions.end() ) );
            assert( biggestResolution.gameWidth == biggestResolution.screenWidth && biggestResolution.gameHeight == biggestResolution.screenHeight );

            // This resolution should be really "widescreen" (or at least square), i.e. the width should be not less than the height, otherwise
            // the scaled resolution's in-game width will become less than DEFAULT_WIDTH
            if ( biggestResolution.gameWidth > fheroes2::Display::DEFAULT_WIDTH && biggestResolution.gameWidth >= biggestResolution.gameHeight ) {
                assert( biggestResolution.gameHeight >= fheroes2::Display::DEFAULT_HEIGHT );

                scaledResolutions.emplace( biggestResolution.gameWidth * fheroes2::Display::DEFAULT_HEIGHT / biggestResolution.gameHeight,
                                           fheroes2::Display::DEFAULT_HEIGHT, biggestResolution.gameWidth, biggestResolution.gameHeight );
            }
        }

        // Try to find and add scaled resolutions with integer scale (x2, x3, x4, etc) which correspond to existing "hardware" resolutions
        for ( const fheroes2::ResolutionInfo & resolution : resolutions ) {
            assert( resolution.gameWidth == resolution.screenWidth && resolution.gameHeight == resolution.screenHeight );

            const int32_t maxScale = std::min( resolution.gameWidth / fheroes2::Display::DEFAULT_WIDTH, resolution.gameHeight / fheroes2::Display::DEFAULT_HEIGHT );

            for ( int32_t scale = 2; scale <= maxScale; ++scale ) {
                if ( resolution.gameWidth % scale != 0 || resolution.gameHeight % scale != 0 ) {
                    continue;
                }

                scaledResolutions.emplace( resolution.gameWidth / scale, resolution.gameHeight / scale, resolution.gameWidth, resolution.gameHeight );
            }
        }

        resolutions.merge( scaledResolutions );

        return resolutions;
    }

    std::vector<uint8_t> StandardPaletteIndexes()
    {
        std::vector<uint8_t> indexes( 256 );
        for ( uint32_t i = 0; i < 256; ++i ) {
            indexes[i] = static_cast<uint8_t>( i );
        }
        return indexes;
    }

    const uint8_t * PALPalette( const bool forceDefaultPaletteUpdate = false )
    {
        static std::vector<uint8_t> palette;
        if ( palette.empty() || forceDefaultPaletteUpdate ) {
            const uint8_t * gamePalette = fheroes2::getGamePalette();

            palette.resize( 256 * 3 );
            for ( size_t i = 0; i < palette.size(); ++i ) {
                palette[i] = gamePalette[i] << 2;
            }
        }

        return palette.data();
    }

    // Updates `roi` to be only inside {0, 0, width, height} rectangle.
    // Returns false if `roi` is out of such rectangle.
    bool getActiveArea( fheroes2::Rect & roi, const int32_t width, const int32_t height )
    {
        if ( roi.width <= 0 || roi.height <= 0 || roi.x >= width || roi.y >= height ) {
            return false;
        }

        if ( roi.x < 0 ) {
            if ( -roi.x >= roi.width ) {
                return false;
            }

            roi.width += roi.x;
            roi.x = 0;
        }

        if ( roi.y < 0 ) {
            if ( -roi.y >= roi.height ) {
                return false;
            }

            roi.height += roi.y;
            roi.y = 0;
        }

        if ( const int32_t offsetX = roi.x + roi.width - width; offsetX > 0 ) {
            if ( offsetX >= roi.width ) {
                return false;
            }

            roi.width -= offsetX;
        }

        if ( const int32_t offsetY = roi.y + roi.height - height; offsetY > 0 ) {
            if ( offsetY >= roi.height ) {
                return false;
            }

            roi.height -= offsetY;
        }

        return true;
    }

    const uint8_t * currentPalette = PALPalette();

    class BaseSDLRenderer
    {
    protected:
        std::vector<uint32_t> _palette32Bit;
        std::vector<SDL_Color> _palette8Bit;

        void copyImageToSurface( const fheroes2::Image & image, SDL_Surface * surface, const fheroes2::Rect & roi )
        {
            assert( surface != nullptr && !image.empty() );

            if ( SDL_MUSTLOCK( surface ) ) {
                if ( !SDL_LockSurface( surface ) ) {
                    ERROR_LOG( "Failed to lock surface. The error: " << SDL_GetError() )
                }
            }

            const int32_t imageWidth = image.width();
            const int32_t imageHeight = image.height();

            const bool fullFrame = ( roi.width == imageWidth ) && ( roi.height == imageHeight );

            const uint8_t * imageIn = image.image();
            // SDL3 reports SDL_BITSPERPIXEL of XRGB8888 as 24 (only color bits) — switch the
            // dispatch to bytes-per-pixel so 32-bit storage formats hit the 32-bit branch.
            const int bytesPerPixel = SDL_BYTESPERPIXEL( surface->format );

            if ( fullFrame ) {
                if ( bytesPerPixel == 4 ) {
                    uint32_t * out = static_cast<uint32_t *>( surface->pixels );
                    const uint32_t * outEnd = out + imageWidth * imageHeight;
                    const uint8_t * in = imageIn;
                    const uint32_t * transform = _palette32Bit.data();

                    for ( ; out != outEnd; ++out, ++in )
                        *out = *( transform + *in );
                }
                else if ( ( bytesPerPixel == 1 ) && ( surface->pixels != imageIn ) ) {
                    if ( imageWidth % 4 != 0 ) {
                        const int32_t screenWidth = ( imageWidth / 4 ) * 4 + 4;
                        for ( int32_t i = 0; i < imageHeight; ++i ) {
                            memcpy( static_cast<uint8_t *>( surface->pixels ) + screenWidth * i, imageIn + imageWidth * i, static_cast<size_t>( imageWidth ) );
                        }
                    }
                    else {
                        memcpy( surface->pixels, imageIn, static_cast<size_t>( imageWidth ) * imageHeight );
                    }
                }
            }
            else {
                if ( bytesPerPixel == 4 ) {
                    uint32_t * outY = static_cast<uint32_t *>( surface->pixels );
                    const uint32_t * outYEnd = outY + imageWidth * roi.height;
                    const uint8_t * inY = imageIn + roi.x + roi.y * imageWidth;
                    const uint32_t * transform = _palette32Bit.data();

                    for ( ; outY != outYEnd; outY += imageWidth, inY += imageWidth ) {
                        uint32_t * outX = outY;
                        const uint32_t * outXEnd = outX + roi.width;
                        const uint8_t * inX = inY;

                        for ( ; outX != outXEnd; ++outX, ++inX )
                            *outX = *( transform + *inX );
                    }
                }
                else if ( ( bytesPerPixel == 1 ) && ( surface->pixels != imageIn ) ) {
                    const int32_t screenWidth = ( imageWidth / 4 ) * 4 + 4;
                    const int32_t screenOffset = roi.x + roi.y * screenWidth;
                    const int32_t imageOffset = roi.x + roi.y * imageWidth;
                    for ( int32_t i = 0; i < roi.height; ++i ) {
                        memcpy( static_cast<uint8_t *>( surface->pixels ) + screenWidth * i + screenOffset, imageIn + imageOffset + imageWidth * i,
                                static_cast<size_t>( roi.width ) );
                    }
                }
            }

            if ( SDL_MUSTLOCK( surface ) )
                SDL_UnlockSurface( surface );
        }

        void generatePalette( const std::vector<uint8_t> & colorIds, const SDL_Surface * surface )
        {
            assert( surface != nullptr );

            const int bytesPerPixel = SDL_BYTESPERPIXEL( surface->format );
            const SDL_PixelFormatDetails * formatDetails = SDL_GetPixelFormatDetails( surface->format );

            if ( bytesPerPixel == 4 ) {
                _palette32Bit.resize( 256u );

                if ( formatDetails != nullptr && formatDetails->Amask > 0 ) {
                    for ( size_t i = 0; i < 256u; ++i ) {
                        const uint8_t * value = currentPalette + colorIds[i] * 3;
                        _palette32Bit[i] = SDL_MapRGBA( formatDetails, nullptr, *value, *( value + 1 ), *( value + 2 ), 255 );
                    }
                }
                else {
                    for ( size_t i = 0; i < 256u; ++i ) {
                        const uint8_t * value = currentPalette + colorIds[i] * 3;
                        _palette32Bit[i] = SDL_MapRGB( formatDetails, nullptr, *value, *( value + 1 ), *( value + 2 ) );
                    }
                }
            }
            else if ( bytesPerPixel == 1 ) {
                _palette8Bit.resize( 256 );
                for ( uint32_t i = 0; i < 256; ++i ) {
                    const uint8_t * value = currentPalette + colorIds[i] * 3;
                    SDL_Color & col = _palette8Bit[i];

                    col.r = *value;
                    col.g = *( value + 1 );
                    col.b = *( value + 2 );
                }
            }
            else {
                // This is unsupported format. Please implement it.
                assert( 0 );
            }
        }

        SDL_Surface * generateIconSurface( const fheroes2::Image & icon )
        {
            if ( icon.empty() || icon.singleLayer() ) {
                // What are you trying to do? Icon should have not empty both image and transform layers.
                assert( 0 );
                return nullptr;
            }

            SDL_Surface * surface = SDL_CreateSurface( icon.width(), icon.height(), SDL_PIXELFORMAT_RGBA32 );
            if ( surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << icon.width() << " x " << icon.height() << " size for cursor. The error: " << SDL_GetError() )
                return nullptr;
            }

            const uint32_t width = icon.width();
            const uint32_t height = icon.height();

            uint32_t * out = static_cast<uint32_t *>( surface->pixels );
            const uint32_t * outEnd = out + width * height;
            const uint8_t * in = icon.image();
            const uint8_t * transform = icon.transform();

            const SDL_PixelFormatDetails * formatDetails = SDL_GetPixelFormatDetails( surface->format );

            for ( ; out != outEnd; ++out, ++in, ++transform ) {
                if ( *transform == 0 ) {
                    const uint8_t * value = currentPalette + *in * 3;
                    *out = SDL_MapRGBA( formatDetails, nullptr, *value, *( value + 1 ), *( value + 2 ), 255 );
                }
                else {
                    *out = SDL_MapRGBA( formatDetails, nullptr, 0, 0, 0, 0 );
                }
            }

            return surface;
        }
    };

    class RenderCursor final : public fheroes2::Cursor
    {
    public:
        RenderCursor( const RenderCursor & ) = delete;

        ~RenderCursor() override
        {
            clear();
        }

        RenderCursor & operator=( const RenderCursor & ) = delete;

        void show( const bool enable ) override
        {
            fheroes2::Cursor::show( enable );

            if ( !_emulation ) {
                const bool success = _show ? SDL_ShowCursor() : SDL_HideCursor();
                if ( !success ) {
                    ERROR_LOG( "Failed to set cursor. The error: " << SDL_GetError() )
                }
            }
        }

        bool isVisible() const override
        {
            if ( _emulation ) {
                return fheroes2::Cursor::isVisible();
            }

            return fheroes2::Cursor::isVisible() && SDL_CursorVisible();
        }

        void update( const fheroes2::Image & image, int32_t offsetX, int32_t offsetY ) override
        {
            if ( image.empty() || image.singleLayer() ) {
                // What are you trying to do? Set an invisible cursor? Use hide() method!
                assert( 0 );
                return;
            }

            if ( _emulation ) {
                fheroes2::Cursor::update( image, offsetX, offsetY );
                return;
            }

            SDL_Surface * surface = SDL_CreateSurface( image.width(), image.height(), SDL_PIXELFORMAT_RGBA32 );
            if ( surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << image.width() << " x " << image.height() << " size for cursor. The error: " << SDL_GetError() )
                return;
            }

            const uint32_t width = image.width();
            const uint32_t height = image.height();

            uint32_t * out = static_cast<uint32_t *>( surface->pixels );
            const uint32_t * outEnd = out + width * height;
            const uint8_t * in = image.image();
            const uint8_t * transform = image.transform();

            const SDL_PixelFormatDetails * formatDetails = SDL_GetPixelFormatDetails( surface->format );

            for ( ; out != outEnd; ++out, ++in, ++transform ) {
                if ( *transform == 0 ) {
                    const uint8_t * value = currentPalette + *in * 3;
                    *out = SDL_MapRGBA( formatDetails, nullptr, *value, *( value + 1 ), *( value + 2 ), 255 );
                }
                else if ( *transform > 1 ) {
                    // The OS draws the hardware cursor as RGBA. Approximate the original
                    // game's cursor shadow by mapping the shadow transform values to a
                    // semi-transparent black.
                    *out = SDL_MapRGBA( formatDetails, nullptr, 0, 0, 0, 64 );
                }
                else {
                    *out = SDL_MapRGBA( formatDetails, nullptr, 0, 0, 0, 0 );
                }
            }

            SDL_Cursor * tempCursor = SDL_CreateColorCursor( surface, offsetX, offsetY );
            if ( tempCursor == nullptr ) {
                ERROR_LOG( "Failed to create a cursor. The error description: " << SDL_GetError() )
            }
            else {
                SDL_SetCursor( tempCursor );
            }

            const bool success = _show ? SDL_ShowCursor() : SDL_HideCursor();
            if ( !success ) {
                ERROR_LOG( "Failed to set cursor state. The error: " << SDL_GetError() )
            }
            SDL_DestroySurface( surface );

            if ( tempCursor != nullptr ) {
                clear();
                std::swap( _cursor, tempCursor );
            }
        }

        void enableSoftwareEmulation( const bool enable ) override
        {
            if ( enable == _emulation ) {
                return;
            }

            if ( enable ) {
                clear();

                if ( !SDL_HideCursor() ) {
                    ERROR_LOG( "Failed to disable cursor. The error: " << SDL_GetError() )
                }

                _emulation = true;
            }
            else {
                const bool success = _show ? SDL_ShowCursor() : SDL_HideCursor();
                if ( !success ) {
                    ERROR_LOG( "Failed to set cursor state. The error: " << SDL_GetError() )
                }

                _emulation = false;
            }

            if ( _cursorUpdater != nullptr ) {
                _cursorUpdater();
            }
        }

        static RenderCursor * create()
        {
            return new RenderCursor;
        }

    private:
        SDL_Cursor * _cursor{ nullptr };

        RenderCursor()
        {
            _emulation = false;

            const bool success = _show ? SDL_ShowCursor() : SDL_HideCursor();
            if ( !success ) {
                ERROR_LOG( "Failed to set cursor state. The error: " << SDL_GetError() )
            }
        }

        void clear()
        {
            if ( _cursor != nullptr ) {
                SDL_DestroyCursor( _cursor );
                _cursor = nullptr;
            }
        }
    };

    bool shouldUseFullscreenDesktopMode()
    {
        return fheroes2::cursor().isSoftwareEmulation();
    }

    std::optional<bool> isWindowInAnyDisplay( const fheroes2::ResolutionInfo & resolutionInfo, const fheroes2::Point & windowPos )
    {
        int numDisplays = 0;
        SDL_DisplayID * displays = SDL_GetDisplays( &numDisplays );
        if ( displays == nullptr || numDisplays < 1 ) {
            ERROR_LOG( "Failed to get the number of video displays. The error: " << SDL_GetError() )
            SDL_free( displays );
            return std::nullopt;
        }

        bool result = false;
        for ( int displayIndex = 0; displayIndex < numDisplays; ++displayIndex ) {
            SDL_Rect displayBounds;
            if ( !SDL_GetDisplayBounds( displays[displayIndex], &displayBounds ) ) {
                ERROR_LOG( "Failed to get display bounds for display #" << displayIndex << ". The error: " << SDL_GetError() )
                continue;
            }
            const int displayMaxX = displayBounds.x + displayBounds.w;
            const int displayMaxY = displayBounds.y + displayBounds.h;
            const int32_t windowMaxX = windowPos.x + resolutionInfo.screenWidth;
            const int32_t windowMaxY = windowPos.y + resolutionInfo.screenHeight;
            if ( windowPos.x >= displayBounds.x && windowMaxX < displayMaxX && windowPos.y >= displayBounds.y && windowMaxY < displayMaxY ) {
                result = true;
                break;
            }
        }

        SDL_free( displays );
        return result;
    }

    class RenderEngine final : public fheroes2::BaseRenderEngine, public BaseSDLRenderer
    {
    public:
        RenderEngine( const RenderEngine & ) = delete;

        ~RenderEngine() override
        {
            clear();
        }

        RenderEngine & operator=( const RenderEngine & ) = delete;

        void toggleFullScreen() override
        {
            if ( _window == nullptr ) {
                BaseRenderEngine::toggleFullScreen();
                return;
            }

            const SDL_WindowFlags flags = SDL_GetWindowFlags( _window );
            const bool wasFullScreen = ( flags & SDL_WINDOW_FULLSCREEN ) != 0;

            bool useExclusiveFullscreen = false;
            if ( !wasFullScreen ) {
#if defined( _WIN32 )
                // Force borderless desktop fullscreen for nearest scaling to disable hardware
                // scaling of game resolution by the monitor.
                useExclusiveFullscreen = !( isNearestScaling() || fheroes2::cursor().isSoftwareEmulation() );
#endif

                // Update the window position, in case we get queried for it
                SDL_GetWindowPosition( _window, &_prevWindowPos.x, &_prevWindowPos.y );

                // If the window size has been manually changed to one that does not match any of the resolutions supported by the display, then after switching
                // to full-screen mode, the in-game display area may not occupy the entire screen, and black bars will remain on the sides of the screen. To
                // avoid this, we remember the window size (to restore it later when turning off full-screen mode) and then set the window size to the size of
                // the in-game display area before switching to the full-screen mode.
                SDL_GetWindowSize( _window, &_windowedSize.width, &_windowedSize.height );

                if ( const fheroes2::Display & display = fheroes2::Display::instance(); display.width() != 0 && display.height() != 0 ) {
                    assert( display.screenSize().width >= display.width() && display.screenSize().height >= display.height() );

                    SDL_SetWindowSize( _window, display.screenSize().width, display.screenSize().height );
                }
            }

            // SDL3 splits fullscreen into a "fullscreen mode" (nullptr = borderless desktop, otherwise exclusive at given mode) and a fullscreen on/off toggle.
            if ( !wasFullScreen ) {
                const SDL_DisplayMode * targetMode = nullptr;
                SDL_DisplayMode closestMode{};
                if ( useExclusiveFullscreen ) {
                    const SDL_DisplayID displayID = SDL_GetDisplayForWindow( _window );
                    if ( displayID != 0 ) {
                        const fheroes2::Display & display = fheroes2::Display::instance();
                        if ( SDL_GetClosestFullscreenDisplayMode( displayID, display.screenSize().width, display.screenSize().height, 0.0f, false, &closestMode ) ) {
                            targetMode = &closestMode;
                        }
                    }
                }
                SDL_SetWindowFullscreenMode( _window, targetMode );
            }

            if ( !SDL_SetWindowFullscreen( _window, !wasFullScreen ) ) {
                ERROR_LOG( "Failed to set fullscreen mode. The error: " << SDL_GetError() )
            }

            _syncFullScreen();

            // If the full-screen mode has been turned off, then restore the remembered window size.
            if ( !isFullScreen() && _windowedSize.width != 0 && _windowedSize.height != 0 ) {
                SDL_SetWindowSize( _window, _windowedSize.width, _windowedSize.height );
            }

            _retrieveWindowInfo();
            _toggleMouseCaptureMode();
        }

        bool isFullScreen() const override
        {
            if ( _window == nullptr ) {
                return BaseRenderEngine::isFullScreen();
            }

            const SDL_WindowFlags flags = SDL_GetWindowFlags( _window );
            return ( flags & SDL_WINDOW_FULLSCREEN ) != 0;
        }

        std::vector<fheroes2::ResolutionInfo> getAvailableResolutions() const override
        {
            std::set<fheroes2::ResolutionInfo> resolutionSet;

            int displayCount = 0;
            SDL_DisplayID * displays = SDL_GetDisplays( &displayCount );
            if ( displays != nullptr && displayCount >= 1 ) {
                for ( int displayId = 0; displayId < displayCount; ++displayId ) {
                    int displayModeCount = 0;
                    SDL_DisplayMode ** modes = SDL_GetFullscreenDisplayModes( displays[displayId], &displayModeCount );
                    if ( modes != nullptr && displayModeCount >= 1 ) {
                        for ( int i = 0; i < displayModeCount; ++i ) {
                            resolutionSet.emplace( modes[i]->w, modes[i]->h );
                        }
                    }
                    else {
                        ERROR_LOG( "Failed to get the number of display modes for display " << displayId << ". The error: " << SDL_GetError() )
                    }
                    SDL_free( modes );
                }
            }
            else {
                ERROR_LOG( "Failed to get the number of displays. The error: " << SDL_GetError() )
            }

#if defined( TARGET_NINTENDO_SWITCH )
            // Nintendo Switch supports arbitrary resolutions via the HW scaler
            // 848x480 is the smallest resolution supported by fheroes2
            resolutionSet.emplace( 848, 480 );
#endif
            resolutionSet = FilterResolutions( resolutionSet );

            if ( displays == nullptr || displayCount < 1 ) {
                SDL_free( displays );
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            if ( !shouldUseFullscreenDesktopMode() ) {
                // If software emulation is not enabled it means that we need to use resolutions supported
                // by the hardware.
                SDL_free( displays );
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            // We should limit all available resolutions to the one which is currently chosen on the system
            // to avoid ending up having application window which is bigger than the screen resolution.
            SDL_DisplayMode maxDisplayMode{};

            for ( int displayId = 0; displayId < displayCount; ++displayId ) {
                const SDL_DisplayMode * displayMode = SDL_GetCurrentDisplayMode( displays[displayId] );
                if ( displayMode == nullptr ) {
                    ERROR_LOG( "Failed to retrieve the current display mode for display " << displayId << ". The error: " << SDL_GetError() )
                    continue;
                }

                // There is no an ideal formula of how to choose the biggest resolution among multiple displays
                // so let's use a simple and well-known approach.
                maxDisplayMode.w = std::max( maxDisplayMode.w, displayMode->w );
                maxDisplayMode.h = std::max( maxDisplayMode.h, displayMode->h );
            }

            SDL_free( displays );

            if ( maxDisplayMode.w == 0 || maxDisplayMode.h == 0 ) {
                // None of displays returned a value display mode.
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            // If the current display resolution is somehow very small, ignore it.
            // It could happen when a device has smaller than the required by the engine resolution.
            if ( maxDisplayMode.w < fheroes2::Display::DEFAULT_WIDTH || maxDisplayMode.h < fheroes2::Display::DEFAULT_HEIGHT ) {
                return std::vector<fheroes2::ResolutionInfo>{ resolutionSet.rbegin(), resolutionSet.rend() };
            }

            std::vector<fheroes2::ResolutionInfo> temp;
            for ( auto iter = resolutionSet.rbegin(); iter != resolutionSet.rend(); ++iter ) {
                if ( iter->screenWidth <= maxDisplayMode.w && iter->screenHeight <= maxDisplayMode.h ) {
                    temp.emplace_back( *iter );
                }
            }

            return temp;
        }

        void setTitle( const std::string & title ) override
        {
            if ( _window == nullptr ) {
                return;
            }

            SDL_SetWindowTitle( _window, System::encLocalToUTF8( title ).c_str() );
        }

        void setIcon( const fheroes2::Image & icon ) override
        {
            if ( _window == nullptr )
                return;

            SDL_Surface * surface = generateIconSurface( icon );
            if ( surface == nullptr )
                return;

            SDL_SetWindowIcon( _window, surface );

            SDL_DestroySurface( surface );
        }

        fheroes2::Rect getActiveWindowROI() const override
        {
            return _activeWindowROI;
        }

        fheroes2::Size getCurrentScreenResolution() const override
        {
            return _currentScreenResolution;
        }

        static RenderEngine * create()
        {
            return new RenderEngine;
        }

        void setVSync( const bool enable ) override
        {
            _isVSyncEnabled = enable;

            if ( _renderer == nullptr ) {
                return;
            }

            _toggleVSync();
        }

        fheroes2::Point getWindowPos() const override
        {
            if ( isFullScreen() ) {
                return _prevWindowPos;
            }
            if ( _window == nullptr ) {
                return { -1, -1 };
            }
            fheroes2::Point result;
            SDL_GetWindowPosition( _window, &result.x, &result.y );
            return result;
        }

        void convertWindowToRenderCoordinates( float & x, float & y ) const override
        {
            if ( _renderer != nullptr ) {
                SDL_RenderCoordinatesFromWindow( _renderer, x, y, &x, &y );
            }
        }

        void setWindowPos( const fheroes2::Point pos ) override
        {
            if ( pos == fheroes2::Point{ -1, -1 } ) {
                _prevWindowPos = { SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
            }
            else {
                _prevWindowPos = pos;
            }
        }

    private:
        SDL_Window * _window{ nullptr };
        SDL_Surface * _surface{ nullptr };
        SDL_Renderer * _renderer{ nullptr };
        SDL_Texture * _texture{ nullptr };
        int _driverIndex{ -1 };

        std::string _previousWindowTitle;
        fheroes2::Point _prevWindowPos{ SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
        fheroes2::Size _currentScreenResolution;
        fheroes2::Rect _activeWindowROI;

        fheroes2::Size _windowedSize;

        bool _isVSyncEnabled{ false };

        // Streaming RGBA32 texture sized to Display dimensions. Pure-RGBA Display upload target —
        // one SDL_UpdateTexture + SDL_RenderTexture per frame, with SDL_SetRenderLogicalPresentation
        // handling the upscale to the physical window.
        SDL_Texture * _screenTexture{ nullptr };
        int32_t _screenTextureW{ 0 };
        int32_t _screenTextureH{ 0 };

        // Composite-on-top cursor overlay: the software cursor is rendered as a separate texture
        // after the main screen texture is presented, so the framebuffer never accumulates cursor
        // pixels. Re-uploaded each frame the cursor is shown (cursor sprites are tiny — 15x21).
        SDL_Texture * _cursorTexture{ nullptr };
        int32_t _cursorTextureW{ 0 };
        int32_t _cursorTextureH{ 0 };
        std::vector<uint8_t> _cursorRgbaScratch;

        RenderEngine() = default;

        void clear() override
        {
            if ( _cursorTexture != nullptr ) {
                SDL_DestroyTexture( _cursorTexture );
                _cursorTexture = nullptr;
                _cursorTextureW = 0;
                _cursorTextureH = 0;
            }

            if ( _screenTexture != nullptr ) {
                SDL_DestroyTexture( _screenTexture );
                _screenTexture = nullptr;
                _screenTextureW = 0;
                _screenTextureH = 0;
            }

            if ( _texture != nullptr ) {
                SDL_DestroyTexture( _texture );
                _texture = nullptr;
            }

            if ( _renderer != nullptr ) {
                SDL_DestroyRenderer( _renderer );
                _renderer = nullptr;
            }

            if ( _window != nullptr ) {
                // Let's collect needed info about previous setup
                if ( !isFullScreen() ) {
                    SDL_GetWindowPosition( _window, &_prevWindowPos.x, &_prevWindowPos.y );
                }
                _previousWindowTitle = System::encUTF8ToLocal( SDL_GetWindowTitle( _window ) );

                SDL_DestroyWindow( _window );
                _window = nullptr;
            }

            if ( _surface != nullptr ) {
                SDL_DestroySurface( _surface );
                _surface = nullptr;
            }

            _driverIndex = -1;
            _windowedSize = {};
        }

        void render( const fheroes2::Display & display, const fheroes2::Rect & roi ) override
        {
            if ( _surface == nullptr ) {
                return;
            }

            assert( _renderer != nullptr && _texture != nullptr );

            copyImageToSurface( display, _surface, roi );

            const bool fullFrame = ( roi.width == display.width() ) && ( roi.height == display.height() );
            if ( fullFrame ) {
                if ( !SDL_UpdateTexture( _texture, nullptr, _surface->pixels, _surface->pitch ) ) {
                    ERROR_LOG( "Failed to update texture. The error: " << SDL_GetError() )
                }
            }
            else {
                SDL_Rect area;
                area.x = roi.x;
                area.y = roi.y;
                area.w = roi.width;
                area.h = roi.height;

                if ( !SDL_UpdateTexture( _texture, &area, _surface->pixels, _surface->pitch ) ) {
                    ERROR_LOG( "Failed to update texture. The error: " << SDL_GetError() )
                }
            }

            if ( !SDL_RenderClear( _renderer ) ) {
                ERROR_LOG( "Failed to clear renderer. The error: " << SDL_GetError() )
                return;
            }

            if ( !SDL_RenderTexture( _renderer, _texture, nullptr, nullptr ) ) {
                ERROR_LOG( "Failed to copy render. The error: " << SDL_GetError() )
                return;
            }

            SDL_RenderPresent( _renderer );
        }

        // Physical-resolution Display path: upload display.image() (RGBA bytes at the
        // physical-pixel buffer dims reported by display.bufferStride()/bufferHeight()) to
        // the streaming texture, then RenderCopy under SDL_RenderSetLogicalSize so SDL maps
        // the texture 1:1 onto the physical window (no further filtering, since the texture
        // already matches physical dims). When the screen happens to equal the game res the
        // physical buffer == game res, and the path collapses to the previous behaviour.
        void renderScreenRGBA( const fheroes2::Display & display ) override
        {
            if ( _renderer == nullptr || _screenTexture == nullptr ) {
                return;
            }

            if ( display.empty() ) {
                return;
            }

            const int32_t w = display.bufferStride();
            const int32_t h = display.bufferHeight();
            if ( w != _screenTextureW || h != _screenTextureH ) {
                // Reallocate the streaming texture to match the physical-pixel framebuffer
                // (e.g. resolution change).
                SDL_DestroyTexture( _screenTexture );
                _screenTexture = SDL_CreateTexture( _renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, w, h );
                if ( _screenTexture == nullptr ) {
                    _screenTextureW = 0;
                    _screenTextureH = 0;
                    ERROR_LOG( "Failed to reallocate the screen RGBA texture: " << SDL_GetError() )
                    return;
                }
                SDL_SetTextureBlendMode( _screenTexture, SDL_BLENDMODE_NONE );
                _screenTextureW = w;
                _screenTextureH = h;
            }

            if ( !SDL_UpdateTexture( _screenTexture, nullptr, display.image(), w * 4 ) ) {
                ERROR_LOG( "Failed to update screen RGBA texture. Error: " << SDL_GetError() )
                return;
            }

            SDL_SetRenderDrawColor( _renderer, 0, 0, 0, 255 );
            if ( !SDL_RenderClear( _renderer ) ) {
                ERROR_LOG( "Failed to clear renderer. Error: " << SDL_GetError() )
            }

            if ( !SDL_RenderTexture( _renderer, _screenTexture, nullptr, nullptr ) ) {
                ERROR_LOG( "Failed to copy screen RGBA texture. Error: " << SDL_GetError() )
            }

            // Composite the software cursor on top of the framebuffer (never blitted into
            // display._data, so the buffer stays clean across frames).
            const fheroes2::Cursor & cursorRef = fheroes2::cursor();
            if ( cursorRef.isVisible() && cursorRef.isSoftwareEmulation() && !cursorRef.getImage().empty() ) {
                renderCursorOverlay( cursorRef );
            }

            SDL_RenderPresent( _renderer );
        }

        void renderCursorOverlay( const fheroes2::Cursor & cursorRef )
        {
            const fheroes2::Sprite & cursorSprite = cursorRef.getImage();
            const int32_t cw = cursorSprite.width();
            const int32_t ch = cursorSprite.height();
            if ( cw <= 0 || ch <= 0 ) {
                return;
            }

            // Allocate / resize the cursor texture if needed.
            if ( _cursorTexture == nullptr || _cursorTextureW != cw || _cursorTextureH != ch ) {
                if ( _cursorTexture != nullptr ) {
                    SDL_DestroyTexture( _cursorTexture );
                    _cursorTexture = nullptr;
                }
                _cursorTexture = SDL_CreateTexture( _renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, cw, ch );
                if ( _cursorTexture == nullptr ) {
                    _cursorTextureW = 0;
                    _cursorTextureH = 0;
                    ERROR_LOG( "Failed to create cursor overlay texture: " << SDL_GetError() )
                    return;
                }
                SDL_SetTextureBlendMode( _cursorTexture, SDL_BLENDMODE_BLEND );
                SDL_SetTextureScaleMode( _cursorTexture, isNearestScaling() ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR );
                _cursorTextureW = cw;
                _cursorTextureH = ch;
            }

            // Convert indexed cursor sprite to RGBA32 via the current palette.
            _cursorRgbaScratch.resize( static_cast<size_t>( cw ) * ch * 4 );
            const uint8_t * idxData = cursorSprite.image();
            const uint8_t * transformData = cursorSprite.transform();
            uint8_t * outRgba = _cursorRgbaScratch.data();
            const int32_t totalPixels = cw * ch;
            for ( int32_t i = 0; i < totalPixels; ++i ) {
                uint8_t * out = outRgba + i * 4;
                if ( transformData[i] == 0 ) {
                    const uint8_t * c = currentPalette + idxData[i] * 3;
                    out[0] = c[0];
                    out[1] = c[1];
                    out[2] = c[2];
                    out[3] = 255;
                }
                else {
                    out[0] = 0;
                    out[1] = 0;
                    out[2] = 0;
                    out[3] = 0;
                }
            }

            if ( !SDL_UpdateTexture( _cursorTexture, nullptr, _cursorRgbaScratch.data(), cw * 4 ) ) {
                ERROR_LOG( "Failed to update cursor overlay texture: " << SDL_GetError() )
                return;
            }

            // Position is the cursor sprite's logical (game) position; logical presentation maps
            // it to physical pixels.
            SDL_FRect dstRect{ static_cast<float>( cursorSprite.x() ), static_cast<float>( cursorSprite.y() ), static_cast<float>( cw ), static_cast<float>( ch ) };
            if ( !SDL_RenderTexture( _renderer, _cursorTexture, nullptr, &dstRect ) ) {
                ERROR_LOG( "Failed to render cursor overlay: " << SDL_GetError() )
            }
        }

        bool allocate( fheroes2::ResolutionInfo & resolutionInfo, bool isFullScreen ) override
        {
            clear();

            const std::vector<fheroes2::ResolutionInfo> resolutions = getAvailableResolutions();
            assert( !resolutions.empty() );
            if ( !resolutions.empty() ) {
                resolutionInfo = GetNearestResolution( resolutionInfo, resolutions );
            }

#if defined( ANDROID ) || defined( __IPHONEOS__ )
            // Same as ActivityInfo.SCREEN_ORIENTATION_SENSOR_LANDSCAPE
            if ( !SDL_SetHint( SDL_HINT_ORIENTATIONS, "LandscapeLeft LandscapeRight" ) ) {
                ERROR_LOG( "Failed to set the " SDL_HINT_ORIENTATIONS " hint." )
            }
#endif

            // SDL3 windows are visible by default; SDL_WINDOW_SHOWN is gone.
            // SDL_WINDOW_FULLSCREEN_DESKTOP is gone — set the fullscreen mode separately via
            // SDL_SetWindowFullscreenMode (nullptr = borderless desktop).
            SDL_WindowFlags flags = SDL_WINDOW_RESIZABLE;
            if ( isFullScreen ) {
                flags |= SDL_WINDOW_FULLSCREEN;
            }

            std::optional<bool> isInDisplay = isWindowInAnyDisplay( resolutionInfo, _prevWindowPos );

            // check if the previous window position is within display or error occured
            if ( !isInDisplay || !*isInDisplay ) {
                // reset position if it is not within the bounds of any display
                _prevWindowPos = { SDL_WINDOWPOS_CENTERED, SDL_WINDOWPOS_CENTERED };
            }

            _window = SDL_CreateWindow( System::encLocalToUTF8( _previousWindowTitle ).c_str(), resolutionInfo.screenWidth, resolutionInfo.screenHeight, flags );
            if ( _window == nullptr ) {
                ERROR_LOG( "Failed to create an application window of " << resolutionInfo.screenWidth << " x " << resolutionInfo.screenHeight
                                                                        << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            // Restore previous window position (SDL3 splits this from SDL_CreateWindow).
            SDL_SetWindowPosition( _window, _prevWindowPos.x, _prevWindowPos.y );

            if ( isFullScreen ) {
                bool useExclusiveFullscreen = false;
#if defined( _WIN32 )
                useExclusiveFullscreen = !( isNearestScaling() || fheroes2::cursor().isSoftwareEmulation() );
#endif
                const SDL_DisplayMode * targetMode = nullptr;
                SDL_DisplayMode closestMode{};
                if ( useExclusiveFullscreen ) {
                    const SDL_DisplayID displayID = SDL_GetDisplayForWindow( _window );
                    if ( displayID != 0 ) {
                        if ( SDL_GetClosestFullscreenDisplayMode( displayID, resolutionInfo.screenWidth, resolutionInfo.screenHeight, 0.0f, false, &closestMode ) ) {
                            targetMode = &closestMode;
                        }
                    }
                }
                SDL_SetWindowFullscreenMode( _window, targetMode );
            }

            _syncFullScreen();

            // SDL3 has dropped SDL_RendererInfo / SDL_GetRenderDriverInfo / SDL_RENDERER_ACCELERATED.
            // The pure-RGBA Display already always uses an RGBA8888 streaming texture, so we don't
            // need to detect indexed-format support at the driver level. Always go RGBA.
            _driverIndex = -1;

            // Setting this hint prevents the window to regain focus after losing it in fullscreen mode.
            // It also fixes issues when SDL_UpdateTexture() calls fail because of refocusing.
            if ( !SDL_SetHint( SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS, "0" ) ) {
                ERROR_LOG( "Failed to set the " SDL_HINT_VIDEO_MINIMIZE_ON_FOCUS_LOSS " hint." )
            }

            _surface = SDL_CreateSurface( resolutionInfo.gameWidth, resolutionInfo.gameHeight, SDL_PIXELFORMAT_XRGB8888 );
            if ( _surface == nullptr ) {
                ERROR_LOG( "Failed to create a surface of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            if ( _surface->w <= 0 || _surface->h <= 0 || _surface->w != resolutionInfo.gameWidth || _surface->h != resolutionInfo.gameHeight ) {
                clear();
                return false;
            }

            _createPalette();

            // SDL3: SDL_CreateRenderer takes (window, name) — pass nullptr to pick the default.
            _renderer = SDL_CreateRenderer( _window, nullptr );
            if ( _renderer == nullptr ) {
                ERROR_LOG( "Failed to create a window renderer of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                    << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }

            if ( !SDL_SetRenderDrawColor( _renderer, 0, 0, 0, SDL_ALPHA_OPAQUE ) ) {
                ERROR_LOG( "Failed to set default color for renderer. The error: " << SDL_GetError() )
            }

            if ( !SDL_SetRenderTarget( _renderer, nullptr ) ) {
                ERROR_LOG( "Failed to set render target to window. The error: " << SDL_GetError() )
            }

            // SDL3: SDL_HINT_RENDER_SCALE_QUALITY is gone — scale mode is set per texture below.
            // Always use LETTERBOX presentation: the Display buffer is already at physical
            // resolution (matching screenSize), so 1:1 mapping happens naturally and there's no
            // extra downsample. The "screen scaling type = nearest" preference is honoured by
            // the texture's scale mode, not by forcing INTEGER_SCALE here (which would constrain
            // the on-screen size to the largest integer multiple of the game res — e.g. 2× on a
            // 1920×1080 monitor — and add a lossy 1440→1280 downsample of the physical buffer).
            if ( !SDL_SetRenderLogicalPresentation( _renderer, resolutionInfo.gameWidth, resolutionInfo.gameHeight, SDL_LOGICAL_PRESENTATION_LETTERBOX ) ) {
                ERROR_LOG( "Failed to set logical presentation of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                    << ". The error: " << SDL_GetError() )
                clear();
                return false;
            }

            _texture = SDL_CreateTextureFromSurface( _renderer, _surface );
            if ( _texture == nullptr ) {
                ERROR_LOG( "Failed to create a texture from a surface of " << resolutionInfo.gameWidth << " x " << resolutionInfo.gameHeight
                                                                           << " size. The error: " << SDL_GetError() )
                clear();
                return false;
            }
            SDL_SetTextureScaleMode( _texture, isNearestScaling() ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR );

            // Physical-resolution Display upload texture: lazily (re)allocated in
            // renderScreenRGBA() once Display has computed its physical buffer dims. The
            // initial 1x1 placeholder keeps the texture pointer non-null so the render path
            // doesn't bail early on the first frame.
            _screenTexture = SDL_CreateTexture( _renderer, SDL_PIXELFORMAT_RGBA32, SDL_TEXTUREACCESS_STREAMING, 1, 1 );
            if ( _screenTexture == nullptr ) {
                ERROR_LOG( "Failed to create the screen RGBA placeholder texture. The error: " << SDL_GetError() )
                clear();
                return false;
            }
            SDL_SetTextureBlendMode( _screenTexture, SDL_BLENDMODE_NONE );
            SDL_SetTextureScaleMode( _screenTexture, isNearestScaling() ? SDL_SCALEMODE_NEAREST : SDL_SCALEMODE_LINEAR );
            _screenTextureW = 1;
            _screenTextureH = 1;

            if ( !_retrieveWindowInfo() ) {
                clear();
                return false;
            }

            _toggleMouseCaptureMode();
            _toggleVSync();

            return true;
        }

        void updatePalette( const std::vector<uint8_t> & colorIds ) override
        {
            if ( _surface == nullptr || colorIds.size() != 256 )
                return;

            generatePalette( colorIds, _surface );
            if ( SDL_BYTESPERPIXEL( _surface->format ) == 1 ) {
                SDL_Palette * surfacePalette = SDL_GetSurfacePalette( _surface );
                if ( surfacePalette != nullptr ) {
                    if ( !SDL_SetPaletteColors( surfacePalette, _palette8Bit.data(), 0, 256 ) ) {
                        ERROR_LOG( "Failed to set palette color. The error: " << SDL_GetError() )
                    }
                }
            }
        }

        bool isMouseCursorActive() const override
        {
            return ( _window != nullptr ) && ( ( SDL_GetWindowFlags( _window ) & SDL_WINDOW_MOUSE_FOCUS ) == SDL_WINDOW_MOUSE_FOCUS );
        }

        void _createPalette()
        {
            if ( _surface == nullptr )
                return;

            updatePalette( StandardPaletteIndexes() );
            // Pure-RGBA Display: no longer link the indexed render surface to the SDL palette
            // surface. The legacy 8-bit palette path is dead.
        }

        bool _retrieveWindowInfo()
        {
            assert( _window != nullptr );

            const SDL_DisplayID displayID = SDL_GetDisplayForWindow( _window );
            if ( displayID == 0 ) {
                ERROR_LOG( "Failed to get window display ID. The error: " << SDL_GetError() )
                return false;
            }

            const SDL_DisplayMode * displayMode = SDL_GetCurrentDisplayMode( displayID );
            if ( displayMode == nullptr ) {
                ERROR_LOG( "Failed to retrieve current display mode. The error: " << SDL_GetError() )
                return false;
            }

            _currentScreenResolution.width = displayMode->w;
            _currentScreenResolution.height = displayMode->h;

#if defined( TARGET_NINTENDO_SWITCH )
            // On a Nintendo Switch the game is always fullscreen
            _activeWindowROI = { 0, 0, _currentScreenResolution.width, _currentScreenResolution.height };
#else
            SDL_GetWindowPosition( _window, &_activeWindowROI.x, &_activeWindowROI.y );
            SDL_GetWindowSize( _window, &_activeWindowROI.width, &_activeWindowROI.height );
#endif

            return true;
        }

        void _toggleVSync()
        {
            assert( _renderer != nullptr );

            if ( !SDL_SetRenderVSync( _renderer, _isVSyncEnabled ? 1 : 0 ) ) {
                ERROR_LOG( "Failed to " << ( _isVSyncEnabled ? "enable" : "disable" ) << " vsync mode for renderer. The error: " << SDL_GetError() )
            }
        }

        void _toggleMouseCaptureMode()
        {
            assert( _window != nullptr );

            // To properly support fullscreen mode on devices with multiple displays or devices with notch,
            // it is important to lock the mouse in the application window area.
            SDL_SetWindowMouseGrab( _window, isFullScreen() );
        }

        void _syncFullScreen()
        {
            if ( isFullScreen() == BaseRenderEngine::isFullScreen() ) {
                return;
            }

            BaseRenderEngine::toggleFullScreen();

            assert( isFullScreen() == BaseRenderEngine::isFullScreen() );
        }
    };
}

namespace fheroes2
{
    Display::Display()
        : _engine( RenderEngine::create() )
        , _cursor( RenderCursor::create() )
    {
        // Display is the single RGBA framebuffer. No transform layer, no separate _screenRGBA.
        _disableTransformLayer();
        // The Image::format() field is private; we set it via constructor in resize() below
        // by reconfiguring through a typed helper.
    }

    void Display::resize( int32_t width_, int32_t height_ )
    {
        assert( width_ == width() && height_ == height() );

#ifdef NDEBUG
        (void)width_;
        (void)height_;
#endif
    }

    void Display::setResolution( ResolutionInfo info )
    {
        if ( width() > 0 && height() > 0 && info.gameWidth == width() && info.gameHeight == height() && info.screenWidth == _screenSize.width
             && info.screenHeight == _screenSize.height ) // nothing to resize
            return;

        const bool isFullScreen = _engine->isFullScreen();

        // deallocate engine resources
        _engine->clear();

        _prevRoi = {};

        // allocate engine resources
        if ( !_engine->allocate( info, isFullScreen ) ) {
            clear();
        }

        _screenSize = { info.screenWidth, info.screenHeight };

        // Compute the physical-pixel dimensions of the backing buffer. We size the buffer at
        // the screen resolution (or the largest integer-aspect fit) and clamp scale >= 1, so
        // when the screen happens to match the game dims the buffer is just game-sized.
        const float scaleX = static_cast<float>( info.screenWidth ) / static_cast<float>( info.gameWidth );
        const float scaleY = static_cast<float>( info.screenHeight ) / static_cast<float>( info.gameHeight );
        const float scale = std::max( 1.0f, std::min( scaleX, scaleY ) );
        _physWidth = static_cast<int32_t>( info.gameWidth * scale );
        _physHeight = static_cast<int32_t>( info.gameHeight * scale );

        // Allocate an RGBA Image at physical resolution, then patch _width/_height down to
        // game dimensions so widget code sees game coords. _data stays physical-sized; the
        // bufferStride()/bufferHeight() virtuals expose the actual buffer dims to primitives.
        Image fresh( _physWidth, _physHeight, ImageFormat::RGBA_32BIT );
        fresh.reset(); // Zero out so the framebuffer starts fully transparent black.
        Image::operator=( std::move( fresh ) );
        _setLogicalDimensions( info.gameWidth, info.gameHeight );
    }

    void Display::resetRenderer()
    {
        const bool isFullScreen = _engine->isFullScreen();

        _engine->clear();
        _prevRoi = {};

        ResolutionInfo res( width(), height(), _screenSize.width, _screenSize.height );

        if ( !_engine->allocate( res, isFullScreen ) ) {
            clear();
        }
    }

    void Display::setWindowPos( const Point point )
    {
        _engine->setWindowPos( point );
    }

    Display & Display::instance()
    {
        static Display display;
        return display;
    }

    void Display::render( const Rect & roi )
    {
        Rect temp( roi );
        if ( !getActiveArea( temp, width(), height() ) ) {
            return;
        }

        if ( empty() ) {
            // No framebuffer yet (pre-resolution-set).
            if ( _postprocessing ) {
                _postprocessing();
            }
            _prevRoi = temp;
            return;
        }

        if ( _preprocessing ) {
            std::vector<uint8_t> palette;
            if ( _preprocessing( palette ) ) {
                _engine->updatePalette( palette );
            }
        }

        // Pure-RGBA Display: don't blit the cursor into the framebuffer (it would accumulate
        // since there's no indexed→RGBA mirror to regenerate the buffer). The engine composites
        // the cursor as a separate overlay layer on top of the presented framebuffer.
        _engine->renderScreenRGBA( *this );

        if ( _postprocessing ) {
            _postprocessing();
        }

        _prevRoi = temp;
    }

    void Display::updateNextRenderRoi( const Rect & roi )
    {
        _prevRoi = getBoundaryRect( _prevRoi, roi );
        getActiveArea( _prevRoi, width(), height() );
    }

    void Display::release()
    {
        _engine->clear();
        _cursor.reset();
        clear();

        _prevRoi = {};
    }

    void Display::changePalette( const uint8_t * palette, const bool forceDefaultPaletteUpdate )
    {
        if ( currentPalette == palette || ( palette == nullptr && currentPalette == PALPalette() && !forceDefaultPaletteUpdate ) )
            return;

        currentPalette = ( palette == nullptr ) ? PALPalette( forceDefaultPaletteUpdate ) : palette;

        _engine->updatePalette( StandardPaletteIndexes() );

        // POSTPONED: with the pure-RGBA Display we have no indexed buffer to re-mirror under
        // the new palette. Color cycling animations (gold/water/lava) won't update on screen
        // until a shader-LUT path lands. The currentPalette is kept up to date so newly drawn
        // primitives use the latest colours, but already-drawn pixels stay at the old colours
        // until the affected widgets redraw themselves.
    }

    bool Cursor::isFocusActive() const
    {
        return engine().isMouseCursorActive();
    }

    BaseRenderEngine & engine()
    {
        const fheroes2::Display & display = Display::instance();

        assert( display._engine );

        return *( display._engine );
    }

    Cursor & cursor()
    {
        const fheroes2::Display & display = Display::instance();

        assert( display._cursor );

        return *( display._cursor );
    }
}
