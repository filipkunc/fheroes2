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

    // Forward declaration so BaseRenderEngine can refer to Display.
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

        // SDL3 no longer auto-remaps mouse event coords through the renderer's logical
        // presentation (unlike SDL2's SDL_RenderSetLogicalSize). LocalEvent calls this
        // for every mouse/touch event so coords reach the game in logical (game) space.
        virtual void convertWindowToRenderCoordinates( float & /* x */, float & /* y */ ) const
        {
            // Default: identity.
        }

        // True when the engine uses SDL_SetRenderLogicalPresentation (legacy SDL_Renderer
        // path). LocalEvent's touch handler uses this to decide whether normalised touch
        // coords map directly to game coords (logical presentation) or need to be scaled
        // through window pixel size + letterbox math (GPU path with no logical wrapper).
        virtual bool usesLogicalPresentation() const
        {
            return false;
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

        // Pure-RGBA Display path: upload display.image() (game-resolution RGBA bytes) to the engine's
        // screen texture and present the frame in a single SDL_RenderCopy. The engine handles the
        // upscale-to-window via SDL_RenderSetLogicalSize / SDL filtering.
        virtual void renderScreenRGBA( const Display & )
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

    private:
        bool _isFullScreen;

        bool _nearestScaling;
    };

    // Pure-RGBA Display: a single Image with format=RGBA_32BIT at game resolution. No separate
    // _screenRGBA, no WriteHook, no mirroring. All drawing primitives target this RGBA buffer
    // directly via their RGBA-output paths in image.cpp. The engine reads display.image() and
    // uploads RGBA bytes to a streaming texture; SDL_RenderSetLogicalSize handles upscale to
    // the physical window.
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

        void release(); // to release all allocated resources. Should be used at the end of the application

        // Change the whole color representation on the screen. Make sure that palette exists all the time!!!
        // nullptr input parameter is used to reset palette to default one.
        // NOTE: with the pure-RGBA Display, this only updates the palette table. Color cycling
        // animations (gold/water/lava) are postponed until a shader-LUT path lands.
        void changePalette( const uint8_t * palette = nullptr, const bool forceDefaultPaletteUpdate = false );

        Size screenSize() const
        {
            return _screenSize;
        }

        // Ratio of physical screen pixels to game pixels. With the physical-resolution Display
        // this is the scale every RGBA-out primitive uses to expand a single game-pixel write
        // into a scale x scale block in the physical-pixel backing buffer.
        float getPhysicalScale() const
        {
            const int32_t gameW = width();
            const int32_t gameH = height();
            if ( gameW <= 0 || gameH <= 0 ) {
                return 1.0f;
            }
            const float scaleX = static_cast<float>( _screenSize.width ) / static_cast<float>( gameW );
            const float scaleY = static_cast<float>( _screenSize.height ) / static_cast<float>( gameH );
            const float scale = std::min( scaleX, scaleY );
            return ( scale < 1.0f ) ? 1.0f : scale;
        }

        // Physical-resolution Display overrides: width()/height() report game dims (logical
        // coords used by widget code), bufferStride()/bufferHeight() report the physical-pixel
        // backing-buffer dimensions, physicalScale() drives the per-pixel block expansion in
        // every RGBA-out primitive.
        int32_t bufferStride() const override
        {
            return _physWidth;
        }

        int32_t bufferHeight() const override
        {
            return _physHeight;
        }

        float physicalScale() const override
        {
            return getPhysicalScale();
        }

        // Phase 3 (SDL_GPU): a second game-resolution buffer carrying palette indices, plus
        // a parallel mask buffer holding the validity bit (0 = use RGBA, 255 = use indexed).
        // Indexed-source primitives skip scale² block expansion and write one byte per game
        // pixel to the indexed buffer (and 255 to the mask). RGBA-source primitives clear the
        // mask for their bbox so the GPU composite shader resolves through their RGBA result.
        // Both buffers are sized at width()×height() (game dims).
        uint8_t * indexedBuffer() override
        {
            return _indexedBuffer.get();
        }
        const uint8_t * indexedBuffer() const override
        {
            return _indexedBuffer.get();
        }
        uint8_t * maskBuffer() override
        {
            return _maskBuffer.get();
        }
        const uint8_t * maskBuffer() const override
        {
            return _maskBuffer.get();
        }
        int32_t indexedStride() const override
        {
            return width();
        }
        int32_t indexedHeight() const override
        {
            return height();
        }
        void markIndexedDirty( const Rect & roi ) override;

        // Returns the game-coord ROI that has been written to the indexed buffer since the
        // last consumeIndexedDirtyRoi() call, intersected with the framebuffer. Empty Rect
        // means nothing dirty. Used by the GPU engine to bound per-frame uploads.
        Rect consumeIndexedDirtyRoi();

        friend BaseRenderEngine & engine();
        friend Cursor & cursor();

    private:
        std::unique_ptr<BaseRenderEngine> _engine;
        std::unique_ptr<Cursor> _cursor;
        PreRenderProcessing _preprocessing{ nullptr };
        PostRenderProcessing _postprocessing{ nullptr };

        // Previous area drawn on the screen.
        Rect _prevRoi;

        Size _screenSize;

        // Physical-pixel dimensions of the backing buffer. width()/height() report the GAME
        // dimensions (logical coords), but the actual RGBA buffer is sized at these physical
        // dimensions so primitives can write final-pixel-resolution content directly.
        int32_t _physWidth{ 0 };
        int32_t _physHeight{ 0 };

        // Phase 3: game-resolution indexed buffer + parallel mask buffer, populated by
        // indexed-source primitives. The mask carries the validity bit (0 = use RGBA at this
        // game pixel, 255 = use indexed). Both sized at width()×height() bytes; reallocated
        // on setResolution(). Single dirty rect tracks both.
        std::unique_ptr<uint8_t[]> _indexedBuffer;
        std::unique_ptr<uint8_t[]> _maskBuffer;
        Rect _indexedDirtyRoi;

        Display();
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
