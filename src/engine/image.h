/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2020 - 2026                                             *
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

#include <cassert>
#include <cstdint>
#include <memory>
#include <utility>
#include <vector>

#include "math_base.h"

namespace fheroes2
{
    enum class ImageFormat : uint8_t
    {
        INDEXED_8BIT = 0,
        RGBA_32BIT = 1
    };

    // Image always contains an image layer and if image is not a single-layer then also a transform layer.
    // - image layer contains visible pixels which are copy to a destination image
    // - transform layer is used to apply some transformation to an image on which we draw the current one. For example, shadowing
    // - for RGBA_32BIT format: data is 4 bytes per pixel (R,G,B,A), no transform layer; image() returns the RGBA bytes.
    class Image
    {
    public:
        Image() = default;

        Image( const int32_t width, const int32_t height )
        {
            Image::resize( width, height );
        }

        Image( const int32_t width, const int32_t height, const ImageFormat format )
            : _format( format )
        {
            if ( format == ImageFormat::RGBA_32BIT ) {
                _singleLayer = true;
            }
            Image::resize( width, height );
        }

        Image( const Image & image )
        {
            copy( image );
        }

        Image( Image && image ) noexcept;

        virtual ~Image() = default;

        Image & operator=( const Image & image );
        Image & operator=( Image && image ) noexcept;

        virtual void resize( const int32_t width_, const int32_t height_ );

        // It's safe to cast to uint32_t as width and height are always >= 0
        int32_t width() const
        {
            return _width;
        }

        int32_t height() const
        {
            return _height;
        }

        // Physical-pixel byte stride of the backing buffer (in pixels). Default == width().
        // Display overrides this to return its physical-resolution width so primitives that
        // write to Display step rows by physical-pixel stride while still reasoning in game
        // coords for clipping / hit testing.
        virtual int32_t bufferStride() const
        {
            return _width;
        }

        // Physical-pixel row count of the backing buffer. Default == height(); Display overrides
        // it to return its physical-resolution height.
        virtual int32_t bufferHeight() const
        {
            return _height;
        }

        // Ratio of physical pixels to logical (game) pixels for this image. Default == 1.0;
        // Display overrides it to return its getPhysicalScale() so RGBA-out primitives expand
        // each game pixel into a scale x scale physical block when writing to Display.
        virtual float physicalScale() const
        {
            return 1.0f;
        }

        // Phase 3 (SDL_GPU): Display owns a separate game-resolution indexed buffer that
        // indexed-source primitives can target directly (1 byte per game pixel, no scale²
        // block expansion). Returns nullptr on every Image other than Display, which lets
        // the indexed-write fast path detect "this is the framebuffer" without dynamic_cast.
        //
        // The validity bit lives in a separate parallel mask buffer (same dims) so any
        // indexed value 0..255 is meaningful — the GPU composite shader treats mask==0 as
        // "use the RGBA channel here" and any non-zero mask as "use palette[indexed]".
        // Indexed primitives write mask=255 alongside the index byte; RGBA-output primitives
        // clear mask (and indexed) for the bbox they paint over so the sentinel rule resolves
        // through the freshly painted RGBA pixels.
        virtual uint8_t * indexedBuffer()
        {
            return nullptr;
        }
        virtual const uint8_t * indexedBuffer() const
        {
            return nullptr;
        }
        virtual uint8_t * maskBuffer()
        {
            return nullptr;
        }
        virtual const uint8_t * maskBuffer() const
        {
            return nullptr;
        }
        virtual int32_t indexedStride() const
        {
            return 0;
        }
        virtual int32_t indexedHeight() const
        {
            return 0;
        }
        // Notify the engine that the indexed/mask buffers were modified within the given
        // (game-coord) ROI so the next frame's GPU upload can be dirty-rect bounded. The
        // mask is uploaded in lock-step with indexed; one dirty rect tracks both. No-op on
        // non-Display.
        virtual void markIndexedDirty( const Rect & /* roi */ ) {}

        virtual uint8_t * image();

        virtual const uint8_t * image() const;

        uint8_t * transform()
        {
            assert( !_singleLayer && _format == ImageFormat::INDEXED_8BIT );

            return _singleLayer ? nullptr : _data.get() + width() * height();
        }

        const uint8_t * transform() const
        {
            assert( !_singleLayer && _format == ImageFormat::INDEXED_8BIT );

            return _singleLayer ? nullptr : _data.get() + width() * height();
        }

        ImageFormat format() const
        {
            return _format;
        }

        bool empty() const
        {
            return !_data;
        }

        void reset(); // makes image fully transparent (transform layer is set to 1)

        void clear(); // makes the image empty

        // Fill 'image' layer with given value, setting 'transform' layer to 0.
        void fill( const uint8_t value );

        // This is an optional indicator for image processing functions.
        // The whole image still consists of 2 layers but transform layer might be ignored in computations
        bool singleLayer() const
        {
            return _singleLayer;
        }

        // BE CAREFUL! This method disables transform layer usage. Use only for display / video related images which are for end rendering purposes!
        // The name of this method starts from _ on purpose to do not mix with other public methods.
        void _disableTransformLayer()
        {
            _singleLayer = true;
        }

    protected:
        // Display uses this to publish game dimensions via width()/height() while keeping
        // a physical-resolution backing buffer. Does NOT touch _data; only call after the
        // buffer has been allocated at the desired physical size.
        void _setLogicalDimensions( const int32_t logicalWidth, const int32_t logicalHeight )
        {
            _width = logicalWidth;
            _height = logicalHeight;
        }

    private:
        void copy( const Image & image );

        int32_t _width{ 0 };
        int32_t _height{ 0 };
        std::unique_ptr<uint8_t[]> _data; // holds 2 image layers (8-bit) or RGBA data (32-bit)

        // Only for images which are not used for any other operations except displaying on screen.
        bool _singleLayer{ false };

        ImageFormat _format{ ImageFormat::INDEXED_8BIT };
    };

    class Sprite : public Image
    {
    public:
        Sprite() = default;
        Sprite( const int32_t width, const int32_t height, const int32_t x = 0, const int32_t y = 0 )
            : Image( width, height )
            , _x( x )
            , _y( y )
        {
            // Do nothing.
        }

        explicit Sprite( const Image & image, const int32_t x = 0, const int32_t y = 0 )
            : Image( image )
            , _x( x )
            , _y( y )
        {
            // Do nothing.
        }

        explicit Sprite( Image && image, const int32_t x = 0, const int32_t y = 0 )
            : Image( std::move( image ) )
            , _x( x )
            , _y( y )
        {
            // Do nothing.
        }

        Sprite( const Sprite & sprite ) = default;
        Sprite( Sprite && sprite ) noexcept;

        ~Sprite() override = default;

        Sprite & operator=( const Sprite & sprite );
        Sprite & operator=( Sprite && sprite ) noexcept;

        Sprite & operator=( Image && image ) noexcept;

        int32_t x() const
        {
            return _x;
        }

        int32_t y() const
        {
            return _y;
        }

        virtual void setPosition( const int32_t x_, const int32_t y_ );

    private:
        int32_t _x{ 0 };
        int32_t _y{ 0 };
    };

    // Save / restore a region of an image. With the pure-RGBA Display, _copy is RGBA when
    // _image is the RGBA Display (or any RGBA-format target); a single Copy() call restores
    // both palette and hi-res content because they live in the same buffer.
    class ImageRestorer final
    {
    public:
        explicit ImageRestorer( Image & image );
        ImageRestorer( Image & image, const int32_t x_, const int32_t y_, const int32_t width, const int32_t height );

        ImageRestorer( const ImageRestorer & ) = delete;
        ImageRestorer & operator=( const ImageRestorer & ) = delete;

        ~ImageRestorer()
        {
            if ( !_isRestored ) {
                restore();
            }
        }

        void update( const int32_t x_, const int32_t y_, const int32_t width, const int32_t height );

        int32_t x() const
        {
            return _x;
        }

        int32_t y() const
        {
            return _y;
        }

        int32_t width() const
        {
            return _width;
        }

        int32_t height() const
        {
            return _height;
        }

        Rect rect() const
        {
            return { _x, _y, _width, _height };
        }

        void restore();

        void reset()
        {
            _isRestored = true;
        }

    private:
        Image & _image;
        Image _copy;
        // Phase 3 (SDL_GPU): when _image is the Display, the indexed + mask channels hold
        // most of the visible adventure-map content (RGBA may be stale where indexed
        // primitives painted). Save/restore them alongside the RGBA copy so a dialog
        // open/close cycle truly reverts the area. Empty when _image has no indexed
        // buffer (every Image except Display).
        std::vector<uint8_t> _indexedCopy;
        std::vector<uint8_t> _maskCopy;

        int32_t _x{ 0 };
        int32_t _y{ 0 };
        int32_t _width{ 0 };
        int32_t _height{ 0 };

        void _updateRoi();
        // Phase 3: capture / restore the game-resolution indexed bytes for the saved ROI.
        void _captureIndexed();
        void _restoreIndexed();

        bool _isRestored{ false };
    };

    // Apply shadow that gradually reduces strength using 'in' image shape. Shadow is applied to the 'out' image.
    void addGradientShadow( const Sprite & in, Image & out, const Point & outPos, const Point & shadowOffset );
    void addGradientShadowForArea( Image & out, const Point & outPos, const int32_t areaWidth, const int32_t areaHeight, const int32_t shadowOffset );

    // Generates a new image with a shadow of the shape of existing image. Shadow must have only (-x, +y) offset.
    Sprite addShadow( const Sprite & in, const Point & shadowOffset, const uint8_t transformId );

    // make sure that output image's transform layer doesn't have skipping values (transform == 1)
    void AlphaBlit( const Image & in, Image & out, const uint8_t alphaValue, const bool flip = false );
    void AlphaBlit( const Image & in, Image & out, int32_t outX, int32_t outY, const uint8_t alphaValue, const bool flip = false );
    void AlphaBlit( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const uint8_t alphaValue,
                    const bool flip = false );

    // apply palette only for image layer, it doesn't affect transform part
    void ApplyPalette( Image & image, const std::vector<uint8_t> & palette );
    void ApplyPalette( const Image & in, Image & out, const std::vector<uint8_t> & palette );
    void ApplyPalette( Image & image, const uint8_t paletteId );
    void ApplyPalette( const Image & in, Image & out, const uint8_t paletteId );
    void ApplyPalette( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, uint8_t paletteId );
    void ApplyPalette( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                       const std::vector<uint8_t> & palette );

    void ApplyAlpha( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const uint8_t alpha );

    void ApplyTransform( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t transformId );

    // Draw one image on another taking into account the transparency and shadows data in the transform layer.
    void Blit( const Image & in, Image & out, const bool flip = false );
    void Blit( const Image & in, Image & out, const Rect & outRoi, const bool flip = false );
    void Blit( const Image & in, Image & out, const int32_t outX, const int32_t outY, const bool flip = false );
    void Blit( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const bool flip = false );

    // inPos must contain non-negative values
    void Blit( const Image & in, const Point & inPos, Image & out, const Point & outPos, const Size & size, bool flip = false );

    void Copy( const Image & in, Image & out );
    void Copy( const Image & in, const int32_t inX, const int32_t inY, Image & out, const Rect & outRoi );
    void Copy( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height );

    // Copies transform the layer from in to out. Both images must be of the same size.
    void copyTransformLayer( const Image & in, Image & out );
    // Copies transform the layer from in to out.
    void copyTransformLayer( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height );

    Sprite CreateContour( const Image & image, const uint8_t value );

    // Make a transition to "in" image from left to right or vertically - from top to bottom using dithering (https://en.wikipedia.org/wiki/Dither).
    // The direction of transition can be reversed.
    void CreateDitheringTransition( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height,
                                    const bool isVertical, const bool isReverse );

    Sprite Crop( const Image & image, int32_t x, int32_t y, int32_t width, int32_t height );

    // skipFactor is responsible for non-solid line. You can interpret it as skip every N pixel
    void DrawBorder( Image & image, const uint8_t value, const uint32_t skipFactor = 0 );

    // roi is an optional parameter when you need to draw in a small than image area
    void DrawLine( Image & image, const Point & start, const Point & end, const uint8_t value, const Rect & roi = Rect() );

    void DrawRect( Image & image, const Rect & roi, const uint8_t value );

    void DivideImageBySquares( const Point & spriteOffset, const Image & original, const int32_t squareSize, std::vector<Point> & outputSquareId,
                               std::vector<std::pair<Point, Rect>> & outputImageInfo );

    // Every image in the array must be the same size. Make sure that pointers aren't nullptr!
    Image ExtractCommonPattern( const std::vector<const Image *> & input );

    // Please use GetColorId function if you want to use an RGB value
    void Fill( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t colorId );

    void FillTransform( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, const uint8_t transformId );

    Image FilterOnePixelNoise( const Image & input );

    bool FitToRoi( const Image & in, Point & inPos, const Image & out, Point & outPos, Size & outputSize, const Rect & outputRoi );

    Image Flip( const Image & in, const bool horizontally, const bool vertically );
    void Flip( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t width, int32_t height, const bool horizontally,
               const bool vertically );

    // Return ROI with pixels which are not skipped and not used for shadow creation. 1 is to skip, 2 - 5 types of shadows
    Rect GetActiveROI( const Image & image, const uint8_t minTransformValue = 6 );

    // Returns a closest color ID from the original game's palette
    uint8_t GetColorId( const uint8_t red, const uint8_t green, const uint8_t blue );

    Sprite makeShadow( const Sprite & in, const Point & shadowOffset, const uint8_t transformId );

    // This function does NOT check transform layer. If you intent to replace few colors at the same image please use ApplyPalette to be more efficient.
    void ReplaceColorId( Image & image, const uint8_t oldColorId, const uint8_t newColorId );

    // Use this function only when you need to convert pixel value into transform layer
    void ReplaceColorIdByTransformId( Image & image, const uint8_t colorId, const uint8_t transformId );

    // Use this function only when you need to convert transform value into non-transparent pixel with the given color.
    void ReplaceTransformIdByColorId( Image & image, const uint8_t transformId, const uint8_t colorId );

    void Resize( const Image & in, Image & out );

    void Resize( const Image & in, const int32_t inX, const int32_t inY, const int32_t widthRoiIn, const int32_t heightRoiIn, Image & out, const int32_t outX,
                 const int32_t outY, const int32_t widthRoiOut, const int32_t heightRoiOut );

    // Please use value from the main palette only
    void SetPixel( Image & image, const int32_t x, const int32_t y, const uint8_t value );

    void SetPixel( Image & image, const std::vector<Point> & points, const uint8_t value );

    // Please set value not bigger than 13!
    void SetTransformPixel( Image & image, const int32_t x, const int32_t y, const uint8_t value );

    Image Stretch( const Image & in, int32_t inX, int32_t inY, int32_t widthIn, int32_t heightIn, const int32_t widthOut, const int32_t heightOut );

    void SubpixelResize( const Image & in, Image & out );

    void SubpixelResize( const Image & in, const int32_t inX, const int32_t inY, const int32_t widthRoiIn, const int32_t heightRoiIn, Image & out, const int32_t outX,
                         const int32_t outY, const int32_t widthRoiOut, const int32_t heightRoiOut );

    void Transpose( const Image & in, Image & out );

    void updateShadow( Image & image, const Point & shadowOffset, const uint8_t transformId, const bool connectCorners );

    // ----- RGBA-only helpers (Image must be in RGBA_32BIT format) -----
    // These operate on the raw RGBA byte buffer of an Image. Used for hi-res monster portraits
    // (BlitRGBAScaled[Alpha], DrawLineRGBA) and battle spell effects (CopyRGBA, DimRGBA, BlitRGBAAlpha).

    // Blit a sub-region of an RGBA Image onto another RGBA Image, scaled to dstW x dstH, with optional flip.
    void BlitRGBAScaled( const Image & in, Image & out, int32_t outX, int32_t outY, int32_t dstW, int32_t dstH, bool flip = false );
    // Same with global alpha multiplier (0..255).
    void BlitRGBAScaledAlpha( const Image & in, Image & out, int32_t outX, int32_t outY, int32_t dstW, int32_t dstH, uint8_t alpha, bool flip = false );
    // src_over alpha blend of two same-sized RGBA Images at (outX, outY) with global alpha multiplier.
    void BlitRGBAAlpha( const Image & in, Image & out, int32_t outX, int32_t outY, uint8_t alpha, bool flip = false );

    // Bresenham line on an RGBA Image.
    void DrawLineRGBA( Image & image, const Point & start, const Point & end, uint8_t r, uint8_t g, uint8_t b, uint8_t a = 255 );

    // Memcpy a rectangular RGBA region from one Image to another.
    void CopyRGBA( const Image & in, int32_t inX, int32_t inY, Image & out, int32_t outX, int32_t outY, int32_t w, int32_t h );

    // Multiply RGB by factor (0.0..1.0), preserve alpha.
    void DimRGBA( Image & image, int32_t x, int32_t y, int32_t width, int32_t height, float factor );
}
