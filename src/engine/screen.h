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

#pragma once

#include <algorithm>
#include <cstdint>
#include <functional>
#include <memory>
#include <string>
#include <tuple>
#include <vector>

#include "image.h"
#include "math_base.h"

namespace fheroes2
{
    class Cursor;
    class Display;

    struct ResolutionInfo
    {
        ResolutionInfo() = default;

        ResolutionInfo( const int32_t gameWidth_, const int32_t gameHeight_ )
            : gameWidth( gameWidth_ )
            , gameHeight( gameHeight_ )
            , screenWidth( gameWidth_ )
            , screenHeight( gameHeight_ )
        {
            // Do nothing.
        }

        ResolutionInfo( const int32_t gameWidth_, const int32_t gameHeight_, const int32_t screenWidth_, const int32_t screenHeight_ )
            : gameWidth( gameWidth_ )
            , gameHeight( gameHeight_ )
            , screenWidth( screenWidth_ )
            , screenHeight( screenHeight_ )
        {
            // Do nothing.
        }

        bool operator==( const ResolutionInfo & info ) const
        {
            return gameWidth == info.gameWidth && gameHeight == info.gameHeight && screenWidth == info.screenWidth && screenHeight == info.screenHeight;
        }

        bool operator!=( const ResolutionInfo & info ) const
        {
            return !operator==( info );
        }

        bool operator<( const ResolutionInfo & info ) const
        {
            return std::tie( gameWidth, gameHeight, screenWidth, screenHeight ) < std::tie( info.gameWidth, info.gameHeight, info.screenWidth, info.screenHeight );
        }

        int32_t gameWidth{ 0 };

        int32_t gameHeight{ 0 };

        int32_t screenWidth{ 0 };

        int32_t screenHeight{ 0 };
    };

    class BaseRenderEngine
    {
    public:
        friend class Cursor;
        friend class Display;

        virtual ~BaseRenderEngine() = default;

        virtual void toggleFullScreen()
        {
            _isFullScreen = !_isFullScreen;
        }

        virtual bool isFullScreen() const
        {
            return _isFullScreen;
        }

        virtual std::vector<ResolutionInfo> getAvailableResolutions() const
        {
            return {};
        }

        virtual void setTitle( const std::string & )
        {
            // Do nothing.
        }

        virtual void setIcon( const Image & )
        {
            // Do nothing.
        }

        virtual Rect getActiveWindowROI() const
        {
            return {};
        }

        virtual Size getCurrentScreenResolution() const
        {
            return {};
        }

        virtual void setVSync( const bool )
        {
            // Do nothing.
        }

        void setNearestScaling( const bool enable )
        {
            _nearestScaling = enable;
        }

        bool isNearestScaling() const
        {
            return _nearestScaling;
        }

        virtual Point getWindowPos() const
        {
            return { -1, -1 };
        }

        virtual void setWindowPos( const Point /* pos */ )
        {
            // Do nothing
        }

    protected:
        BaseRenderEngine()
            : _isFullScreen( false )
            , _nearestScaling( false )
        {
            // Do nothing.
        }

        virtual void clear()
        {
            // Do nothing.
        }

        virtual void render( const Display &, const Rect & )
        {
            // Do nothing.
        }

        virtual bool allocate( ResolutionInfo & /*unused*/, bool /*unused*/ )
        {
            return false;
        }

        virtual bool isMouseCursorActive() const
        {
            return false;
        }

        // To support color cycling we need to update palette.
        virtual void updatePalette( const std::vector<uint8_t> & )
        {
            // Do nothing.
        }

        void linkRenderSurface( uint8_t * surface ) const; // declaration of this method is in source file

    private:
        bool _isFullScreen;

        bool _nearestScaling;
    };

    class Display final : public Image
    {
    public:
        friend class BaseRenderEngine;

        enum : int32_t
        {
            DEFAULT_WIDTH = 640,
            DEFAULT_HEIGHT = 480
        };

        static Display & instance();

        ~Display() override = default;

        // Render an entire frame on screen.
        void render()
        {
            render( { 0, 0, width(), height() } );
        }

        // Render a part of frame on screen.
        void render( const Rect & roi );

        // Update the area which will be rendered on the next render() call.
        void updateNextRenderRoi( const Rect & roi );

        // Do not call this method. It serves as a patch over the basic class.
        void resize( int32_t width_, int32_t height_ ) override;

        void setResolution( ResolutionInfo info );

        // Call this method only if you need to reset renderer to update its parameters (e.g. screen scaling).
        void resetRenderer();

        bool isDefaultSize() const
        {
            return width() == DEFAULT_WIDTH && height() == DEFAULT_HEIGHT;
        }

        Point getWindowPos() const
        {
            return _engine->getWindowPos();
        }

        void setWindowPos( const Point point );

        // this function must return true if new palette has been generated
        using PreRenderProcessing = std::function<bool( std::vector<uint8_t> & )>;
        using PostRenderProcessing = std::function<void()>;

        void subscribe( const PreRenderProcessing & preprocessing, const PostRenderProcessing & postprocessing )
        {
            _preprocessing = preprocessing;
            _postprocessing = postprocessing;
        }

        // For 8-bit mode we return a pointer to direct surface which we draw on screen
        uint8_t * image() override;
        const uint8_t * image() const override;

        void release(); // to release all allocated resources. Should be used at the end of the application

        // Change the whole color representation on the screen. Make sure that palette exists all the time!!!
        // nullptr input parameter is used to reset palette to default one.
        void changePalette( const uint8_t * palette = nullptr, const bool forceDefaultPaletteUpdate = false ) const;

        Size screenSize() const
        {
            return _screenSize;
        }

        // RGBA overlay support for true-color rendering.
        //
        // Each overlay is tagged with the current forwarding-stack depth so that when a deeper
        // scope (e.g. a battle nested inside the adventure map) registers its own overlays, the
        // shallower scope's overlays automatically stop compositing. See _renderRGBAOverlays.
        void addRGBAOverlay( const RGBAImage & overlay, const int32_t x, const int32_t y, const int32_t gameWidth = 0, const bool flip = false,
                             const uint8_t alpha = 255 )
        {
            _rgbaOverlays.push_back( { &overlay, x, y, gameWidth, flip, alpha, Image::getDialogFwdDepth() } );
        }

        void clearRGBAOverlays()
        {
            _rgbaOverlays.clear();
        }

        // Remove any overlay entries whose image pointer matches the given image.
        // Use this when you need to drop a specific overlay without clearing the ones
        // other subsystems (battle, dialogs, etc.) have registered.
        void removeRGBAOverlay( const RGBAImage * image )
        {
            if ( image == nullptr ) {
                return;
            }
            _rgbaOverlays.erase( std::remove_if( _rgbaOverlays.begin(), _rgbaOverlays.end(),
                                                 [image]( const RGBAOverlay & o ) { return o.image == image; } ),
                                 _rgbaOverlays.end() );
        }

        // Remove the overlay entry for a specific image at a specific position. Use this
        // when the same image is registered in multiple places (e.g. an army with several
        // slots of the same monster type, or two ArmyBars sharing a portrait) and you only
        // want to drop the one you own. Matches on (image, x, y).
        void removeRGBAOverlayAt( const RGBAImage * image, const int32_t x, const int32_t y )
        {
            if ( image == nullptr ) {
                return;
            }
            _rgbaOverlays.erase( std::remove_if( _rgbaOverlays.begin(), _rgbaOverlays.end(),
                                                 [image, x, y]( const RGBAOverlay & o ) { return o.image == image && o.x == x && o.y == y; } ),
                                 _rgbaOverlays.end() );
        }

        // Direct-paint-into-RGBA-buffer registrations. These are the RGBA-space analogue of
        // addRGBAOverlay: persistent entries (not cleared per frame) that Display::render()
        // applies AFTER the palette→RGBA forwarding loop. Widgets register once, keep the
        // paint alive across renders, and remove explicitly when the source slot empties or
        // the widget is destroyed — exactly like overlays. The "after forwarding" ordering
        // preserves hi-res portraits that would otherwise be overwritten by the palette
        // conversion when the host screen owns a forwarded RGBA surface.
        //
        // Identity is keyed on (src, gameX, gameY) — the caller's game-space coordinates.
        // The surface-space blit coordinates (dst*) are used only by Display::render() to
        // perform the actual BlitRGBAScaled; widgets don't have to know about them.
        struct RGBABufferPaint
        {
            const RGBAImage * src;
            RGBAImage * dst;
            // Identity: caller's game-space coordinates (used by registerRGBABufferPaint
            // dedup and removeRGBABufferPaintAt). Widgets track and remove in this space.
            int32_t gameX;
            int32_t gameY;
            // Blit parameters in destination surface coordinates (used at render time).
            int32_t dstX;
            int32_t dstY;
            int32_t dstW;
            int32_t dstH;
            bool flip;
            uint8_t alpha;
        };

        void registerRGBABufferPaint( const RGBAImage & src, RGBAImage & dst, const int32_t gameX, const int32_t gameY, const int32_t dstX, const int32_t dstY,
                                      const int32_t dstW, const int32_t dstH, const bool flip = false, const uint8_t alpha = 255 )
        {
            // Replace any existing registration matching this (src, dst, gameX, gameY) tuple so
            // repeated widget redraws don't accumulate duplicates.
            for ( RGBABufferPaint & p : _rgbaBufferPaints ) {
                if ( p.src == &src && p.dst == &dst && p.gameX == gameX && p.gameY == gameY ) {
                    p.dstX = dstX;
                    p.dstY = dstY;
                    p.dstW = dstW;
                    p.dstH = dstH;
                    p.flip = flip;
                    p.alpha = alpha;
                    return;
                }
            }
            _rgbaBufferPaints.push_back( { &src, &dst, gameX, gameY, dstX, dstY, dstW, dstH, flip, alpha } );
        }

        // Remove registration(s) matching (src, gameX, gameY), regardless of destination
        // buffer. Call this when a slot dismisses / moves / the owning widget is destroyed.
        void removeRGBABufferPaintAt( const RGBAImage * src, const int32_t gameX, const int32_t gameY )
        {
            if ( src == nullptr ) {
                return;
            }
            _rgbaBufferPaints.erase( std::remove_if( _rgbaBufferPaints.begin(), _rgbaBufferPaints.end(),
                                                    [src, gameX, gameY]( const RGBABufferPaint & p ) {
                                                        return p.src == src && p.gameX == gameX && p.gameY == gameY;
                                                    } ),
                                    _rgbaBufferPaints.end() );
        }

        // Remove every registration that targets the given destination buffer. Call this when
        // the destination surface is about to go out of scope (e.g. a screen's RGBA is being
        // destroyed on dialog close) to avoid dangling dst pointers.
        void removeRGBABufferPaintsForTarget( const RGBAImage * dst )
        {
            if ( dst == nullptr ) {
                return;
            }
            _rgbaBufferPaints.erase( std::remove_if( _rgbaBufferPaints.begin(), _rgbaBufferPaints.end(),
                                                    [dst]( const RGBABufferPaint & p ) { return p.dst == dst; } ),
                                    _rgbaBufferPaints.end() );
        }

        // Remove paints targeting `dst` whose game-space coordinate falls within the given rect.
        // Use this from a widget that redraws periodically with a variable troop list (status
        // panel on focus change, casualty dialog, quickinfo popup): before registering this
        // frame's portraits, wipe any leftovers from the previous frame's troops so a dismissed
        // custom monster doesn't keep ghosting over whatever now occupies its slot.
        void removeRGBABufferPaintsInRect( const RGBAImage * dst, const int32_t gameX, const int32_t gameY, const int32_t gameW, const int32_t gameH )
        {
            if ( dst == nullptr || gameW <= 0 || gameH <= 0 ) {
                return;
            }
            const int32_t x1 = gameX + gameW;
            const int32_t y1 = gameY + gameH;
            _rgbaBufferPaints.erase( std::remove_if( _rgbaBufferPaints.begin(), _rgbaBufferPaints.end(),
                                                    [dst, gameX, gameY, x1, y1]( const RGBABufferPaint & p ) {
                                                        return p.dst == dst && p.gameX >= gameX && p.gameX < x1 && p.gameY >= gameY && p.gameY < y1;
                                                    } ),
                                    _rgbaBufferPaints.end() );
        }

        const std::vector<RGBABufferPaint> & getRGBABufferPaints() const
        {
            return _rgbaBufferPaints;
        }

        const std::vector<RGBAOverlay> & getRGBAOverlays() const
        {
            return _rgbaOverlays;
        }


        // Set a physical-resolution RGBA surface to be copied directly to the SDL surface during rendering.
        // The surface replaces the palette-converted pixels in the specified area.
        void setRGBACompositLayer( const RGBAImage * layer, const int32_t offsetX, const int32_t offsetY )
        {
            _rgbaCompositLayer = layer;
            _rgbaCompositOffsetX = offsetX;
            _rgbaCompositOffsetY = offsetY;
        }

        const RGBAImage * getRGBACompositLayer() const
        {
            return _rgbaCompositLayer;
        }

        int32_t getRGBACompositOffsetX() const
        {
            return _rgbaCompositOffsetX;
        }

        int32_t getRGBACompositOffsetY() const
        {
            return _rgbaCompositOffsetY;
        }

        friend BaseRenderEngine & engine();
        friend Cursor & cursor();

    private:
        std::unique_ptr<BaseRenderEngine> _engine;
        std::unique_ptr<Cursor> _cursor;
        PreRenderProcessing _preprocessing{ nullptr };
        PostRenderProcessing _postprocessing{ nullptr };

        uint8_t * _renderSurface{ nullptr };

        // Previous area drawn on the screen.
        Rect _prevRoi;

        Size _screenSize;

        std::vector<RGBAOverlay> _rgbaOverlays;
        std::vector<RGBABufferPaint> _rgbaBufferPaints;

        const RGBAImage * _rgbaCompositLayer{ nullptr };
        int32_t _rgbaCompositOffsetX{ 0 };
        int32_t _rgbaCompositOffsetY{ 0 };

        // Only for cases of direct drawing on rendered 8-bit image.
        void linkRenderSurface( uint8_t * surface )
        {
            _renderSurface = surface;
        }

        Display();

        void _renderFrame( const Rect & roi ) const; // prepare and render a frame
    };

    class Cursor
    {
    public:
        friend Display;
        virtual ~Cursor() = default;

        virtual void show( const bool enable )
        {
            _show = enable;
        }

        virtual bool isVisible() const
        {
            return _show;
        }

        bool isFocusActive() const;

        virtual void update( const Image & image, int32_t offsetX, int32_t offsetY )
        {
            _image = Sprite( image, offsetX, offsetY );
        }

        void setPosition( const int32_t x, const int32_t y )
        {
            _image.setPosition( x, y );
        }

        Rect getArea() const
        {
            return { _image.x(), _image.y(), _image.width(), _image.height() };
        }

        const Sprite & getImage() const
        {
            return _image;
        }

        // Default implementation of Cursor uses software emulation.
        virtual void enableSoftwareEmulation( const bool )
        {
            // Do nothing.
        }

        bool isSoftwareEmulation() const
        {
            return _emulation;
        }

        void registerUpdater( void ( *cursorUpdater )() )
        {
            _cursorUpdater = cursorUpdater;
        }

        void keepInScreenArea( const bool value )
        {
            _keepInScreenArea = value;
        }

    protected:
        Sprite _image;
        void ( *_cursorUpdater )(){ nullptr };
        bool _emulation{ false };
        bool _show{ false };
        bool _keepInScreenArea{ false };

        Cursor() = default;
    };

    BaseRenderEngine & engine();
    Cursor & cursor();
}
