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
#include <cstdint>
#include <cstdio>
#include <cstring>
#include <memory>
#include <ostream>
#include <string_view>
#include <vector>

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic push

#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wswitch-default"
#endif

#include <SDL3/SDL_error.h>
#include <SDL3/SDL_pixels.h>
#include <SDL3/SDL_stdinc.h>
#include <SDL3/SDL_surface.h>

#if defined( WITH_IMAGE )
#include <SDL3_image/SDL_image.h>
#endif

// Managing compiler warnings for SDL headers
#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif

#if !defined( WITH_IMAGE )
// Use stb_image as a fallback PNG loader when SDL_image is not available (e.g. Android).
#if defined( __GNUC__ )
#pragma GCC diagnostic push
#pragma GCC diagnostic ignored "-Wcast-align"
#pragma GCC diagnostic ignored "-Wcast-qual"
#pragma GCC diagnostic ignored "-Wdouble-promotion"
#pragma GCC diagnostic ignored "-Wfloat-conversion"
#pragma GCC diagnostic ignored "-Wold-style-cast"
#pragma GCC diagnostic ignored "-Wsign-conversion"
#pragma GCC diagnostic ignored "-Wswitch-default"
#pragma GCC diagnostic ignored "-Wunused-function"
#endif

#if defined( __clang__ )
#pragma clang diagnostic push
#pragma clang diagnostic ignored "-Wdisabled-macro-expansion"
#pragma clang diagnostic ignored "-Wcomma"
#endif

#if defined( _MSC_VER )
#pragma warning( push )
// stb_image v2.30 defines several detection helpers (stbi__cpuid3, stbi__addints_valid,
// stbi__mul2shorts_valid) that are conditionally unused depending on the targeted format set.
// MSVC strips them and emits C4505; the equivalent GCC/Clang -Wunused-function suppression is
// already in place above, this is the MSVC-side complement.
#pragma warning( disable : 4505 )
#endif

#define STB_IMAGE_IMPLEMENTATION
#define STBI_ONLY_PNG
#define STBI_NO_STDIO
#include "stb_image.h"

#if defined( _MSC_VER )
#pragma warning( pop )
#endif

#if defined( __clang__ )
#pragma clang diagnostic pop
#endif

#if defined( __GNUC__ )
#pragma GCC diagnostic pop
#endif
#endif

#include "agg_file.h"
#include "image_palette.h"
#include "image_tool.h"
#include "logging.h"
#include "serialize.h"
#include "system.h"

namespace
{
    bool isPNGFilePath( const std::string_view path )
    {
        const std::string pngExtension( ".png" );
        return path.size() >= pngExtension.size() && ( path.compare( path.size() - pngExtension.size(), pngExtension.size(), pngExtension ) == 0 );
    }

    std::vector<uint8_t> PALPalette()
    {
        const uint8_t * gamePalette = fheroes2::getGamePalette();

        std::vector<uint8_t> palette( 256 * 3 );
        for ( size_t i = 0; i < palette.size(); ++i ) {
            palette[i] = gamePalette[i] << 2;
        }

        return palette;
    }

#if defined( WITH_IMAGE )
    bool SaveImage( const fheroes2::Image & image, const std::string & path )
#else
    bool SaveImage( const fheroes2::Image & image, std::string path )
#endif
    {
        const std::vector<uint8_t> & palette = PALPalette();
        const uint8_t * currentPalette = palette.data();

        const int32_t width = image.width();
        const int32_t height = image.height();

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> surface( SDL_CreateSurface( width, height, SDL_PIXELFORMAT_INDEX8 ), SDL_DestroySurface );
        if ( !surface ) {
            ERROR_LOG( "Error while creating a SDL surface for an image to be saved under " << path << ". Error " << SDL_GetError() )
            return false;
        }

        assert( SDL_BYTESPERPIXEL( surface->format ) == 1 );

        std::vector<SDL_Color> paletteSDL;
        paletteSDL.resize( 256 );
        for ( int32_t i = 0; i < 256; ++i ) {
            const uint8_t * value = currentPalette + i * 3;
            SDL_Color & col = paletteSDL[i];

            col.r = *value;
            col.g = *( value + 1 );
            col.b = *( value + 2 );
            col.a = 255;
        }

        SDL_Palette * surfacePalette = SDL_CreateSurfacePalette( surface.get() );
        if ( surfacePalette != nullptr ) {
            SDL_SetPaletteColors( surfacePalette, paletteSDL.data(), 0, 256 );
        }

        if ( surface->pitch != width ) {
            const uint8_t * imageIn = image.image();

            for ( int32_t i = 0; i < height; ++i ) {
                memcpy( static_cast<uint8_t *>( surface->pixels ) + surface->pitch * i, imageIn + width * i, static_cast<size_t>( width ) );
            }
        }
        else {
            memcpy( surface->pixels, image.image(), static_cast<size_t>( width * height ) );
        }

#if defined( WITH_IMAGE )
        bool res = false;

        if ( isPNGFilePath( path ) ) {
            res = IMG_SavePNG( surface.get(), System::encLocalToUTF8( path ).c_str() );
        }
        else {
            res = SDL_SaveBMP( surface.get(), System::encLocalToUTF8( path ).c_str() );
        }
#else
        if ( isPNGFilePath( path ) ) {
            memcpy( path.data() + path.size() - 3, "bmp", 3 );
        }

        const bool res = SDL_SaveBMP( surface.get(), System::encLocalToUTF8( path ).c_str() );
#endif

        return res;
    }
}

namespace fheroes2
{
    bool Save( const Image & image, const std::string & path, const uint8_t background )
    {
        if ( image.empty() || path.empty() )
            return false;

        Image temp( image.width(), image.height() );
        temp.fill( background );

        Blit( image, temp );

        return SaveImage( temp, path );
    }

    bool Save( const Image & image, const std::string & path )
    {
        if ( image.empty() || path.empty() )
            return false;

        return SaveImage( image, path );
    }

    bool Load( const std::string & path, Image & image )
    {
        if ( image.singleLayer() ) {
            // Output image should be double-layer!
            assert( 0 );
            return false;
        }

        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> surface( nullptr, SDL_DestroySurface );
        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> loadedSurface( nullptr, SDL_DestroySurface );

#if defined( WITH_IMAGE )
        loadedSurface.reset( IMG_Load( System::encLocalToUTF8( path ).c_str() ) );
#else
        // Try stb_image for PNG files, fall back to SDL_LoadBMP for other formats.
        if ( isPNGFilePath( path ) ) {
            FILE * f = fopen( System::encLocalToUTF8( path ).c_str(), "rb" );
            if ( f ) {
                fseek( f, 0, SEEK_END );
                const long fileSize = ftell( f );
                fseek( f, 0, SEEK_SET );

                if ( fileSize > 0 ) {
                    std::vector<uint8_t> fileData( static_cast<size_t>( fileSize ) );
                    if ( fread( fileData.data(), 1, fileData.size(), f ) == fileData.size() ) {
                        int w = 0;
                        int h = 0;
                        int channels = 0;
                        uint8_t * pixels = stbi_load_from_memory( fileData.data(), static_cast<int>( fileData.size() ), &w, &h, &channels, 4 );
                        if ( pixels && w > 0 && h > 0 ) {
                            // Create an SDL_Surface and copy the pixel data into it.
                            loadedSurface.reset( SDL_CreateSurface( w, h, SDL_PIXELFORMAT_RGBA32 ) );
                            if ( loadedSurface ) {
                                memcpy( loadedSurface->pixels, pixels, static_cast<size_t>( w ) * h * 4 );
                            }
                            stbi_image_free( pixels );
                        }
                    }
                }
                fclose( f );
            }
        }
        else {
            loadedSurface.reset( SDL_LoadBMP( System::encLocalToUTF8( path ).c_str() ) );
        }
#endif
        if ( !loadedSurface ) {
            return false;
        }

        // Image loading functions can theoretically return SDL_Surface in any supported color format, so we will convert it to a specific format for subsequent
        // processing
        surface.reset( SDL_ConvertSurface( loadedSurface.get(), SDL_PIXELFORMAT_BGRA32 ) );
        if ( !surface ) {
            return false;
        }

        assert( !SDL_MUSTLOCK( surface.get() ) && SDL_BYTESPERPIXEL( surface->format ) == 4 );

        image.resize( surface->w, surface->h );
        image.reset();

        const uint8_t * inY = static_cast<uint8_t *>( surface->pixels );
        uint8_t * outY = image.image();
        uint8_t * transformY = image.transform();

        const uint8_t * inYEnd = inY + surface->h * surface->pitch;

        for ( ; inY != inYEnd; inY += surface->pitch, outY += surface->w, transformY += surface->w ) {
            const uint8_t * inX = inY;
            uint8_t * outX = outY;
            uint8_t * transformX = transformY;
            const uint8_t * inXEnd = inX + surface->w * 4;

            for ( ; inX != inXEnd; inX += 4, ++outX, ++transformX ) {
                const uint8_t alpha = *( inX + 3 );
                if ( alpha < 255 ) {
                    if ( alpha == 0 ) {
                        *outX = 0;
                        *transformX = 1;
                    }
                    else if ( *inX == 0 && *( inX + 1 ) == 0 && *( inX + 2 ) == 0 ) {
                        *outX = 0;
                        *transformX = 2;
                    }
                    else {
                        *outX = GetColorId( *( inX + 2 ), *( inX + 1 ), *inX );
                        *transformX = 0;
                    }
                }
                else {
                    *outX = GetColorId( *( inX + 2 ), *( inX + 1 ), *inX );
                    *transformX = 0;
                }
            }
        }

        return true;
    }

    bool LoadRGBA( const std::string & path, Image & image )
    {
#if !defined( WITH_IMAGE )
        // Without SDL_image, use stb_image for PNG files.
        if ( isPNGFilePath( path ) ) {
            FILE * f = fopen( System::encLocalToUTF8( path ).c_str(), "rb" );
            if ( !f ) {
                return false;
            }

            fseek( f, 0, SEEK_END );
            const long fileSize = ftell( f );
            fseek( f, 0, SEEK_SET );

            if ( fileSize <= 0 ) {
                fclose( f );
                return false;
            }

            std::vector<uint8_t> fileData( static_cast<size_t>( fileSize ) );
            const size_t bytesRead = fread( fileData.data(), 1, fileData.size(), f );
            fclose( f );

            if ( bytesRead != fileData.size() ) {
                return false;
            }

            int w = 0;
            int h = 0;
            int channels = 0;
            uint8_t * pixels = stbi_load_from_memory( fileData.data(), static_cast<int>( fileData.size() ), &w, &h, &channels, 4 );
            if ( !pixels || w <= 0 || h <= 0 ) {
                if ( pixels ) {
                    stbi_image_free( pixels );
                }
                return false;
            }

            image = Image( w, h, ImageFormat::RGBA_32BIT );
            memcpy( image.image(), pixels, static_cast<size_t>( w ) * h * 4 );
            stbi_image_free( pixels );
            return true;
        }
#endif

        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> loadedSurface( nullptr, SDL_DestroySurface );

#if defined( WITH_IMAGE )
        loadedSurface.reset( IMG_Load( System::encLocalToUTF8( path ).c_str() ) );
#else
        loadedSurface.reset( SDL_LoadBMP( System::encLocalToUTF8( path ).c_str() ) );
#endif
        if ( !loadedSurface ) {
            return false;
        }

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> surface( SDL_ConvertSurface( loadedSurface.get(), SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !surface ) {
            return false;
        }

        assert( SDL_BYTESPERPIXEL( surface->format ) == 4 );

        image = Image( surface->w, surface->h, ImageFormat::RGBA_32BIT );

        const uint8_t * srcRow = static_cast<const uint8_t *>( surface->pixels );
        uint8_t * dstRow = image.image();

        for ( int32_t y = 0; y < surface->h; ++y ) {
            memcpy( dstRow, srcRow, static_cast<size_t>( surface->w ) * 4 );
            srcRow += surface->pitch;
            dstRow += static_cast<ptrdiff_t>( surface->w ) * 4;
        }

        return true;
    }

    bool LoadRGBA( const std::string & path, Image & image, const int32_t targetWidth, const int32_t targetHeight )
    {
        if ( targetWidth <= 0 || targetHeight <= 0 ) {
            return false;
        }

        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> loadedSurface( nullptr, SDL_DestroySurface );

#if defined( WITH_IMAGE )
        loadedSurface.reset( IMG_Load( System::encLocalToUTF8( path ).c_str() ) );
#else
        loadedSurface.reset( SDL_LoadBMP( System::encLocalToUTF8( path ).c_str() ) );
#endif
        if ( !loadedSurface ) {
            return false;
        }

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> rgbaSurface( SDL_ConvertSurface( loadedSurface.get(), SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !rgbaSurface ) {
            return false;
        }

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> scaledSurface(
            SDL_CreateSurface( targetWidth, targetHeight, SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !scaledSurface ) {
            return false;
        }

        if ( !SDL_BlitSurfaceScaled( rgbaSurface.get(), nullptr, scaledSurface.get(), nullptr, SDL_SCALEMODE_LINEAR ) ) {
            return false;
        }

        image = Image( targetWidth, targetHeight, ImageFormat::RGBA_32BIT );

        const uint8_t * srcRow = static_cast<const uint8_t *>( scaledSurface->pixels );
        uint8_t * dstRow = image.image();

        for ( int32_t y = 0; y < targetHeight; ++y ) {
            memcpy( dstRow, srcRow, static_cast<size_t>( targetWidth ) * 4 );
            srcRow += scaledSurface->pitch;
            dstRow += static_cast<ptrdiff_t>( targetWidth ) * 4;
        }

        return true;
    }

    bool LoadAsRGBA( const std::string & path, Image & image )
    {
        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> loadedSurface( nullptr, SDL_DestroySurface );

#if defined( WITH_IMAGE )
        loadedSurface.reset( IMG_Load( System::encLocalToUTF8( path ).c_str() ) );
#else
        loadedSurface.reset( SDL_LoadBMP( System::encLocalToUTF8( path ).c_str() ) );
#endif
        if ( !loadedSurface ) {
            return false;
        }

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> surface( SDL_ConvertSurface( loadedSurface.get(), SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !surface ) {
            return false;
        }

        assert( SDL_BYTESPERPIXEL( surface->format ) == 4 );

        image = Image( surface->w, surface->h, ImageFormat::RGBA_32BIT );

        const uint8_t * srcRow = static_cast<const uint8_t *>( surface->pixels );
        uint8_t * dstRow = image.image();

        for ( int32_t y = 0; y < surface->h; ++y ) {
            memcpy( dstRow, srcRow, static_cast<size_t>( surface->w ) * 4 );
            srcRow += surface->pitch;
            dstRow += static_cast<ptrdiff_t>( surface->w ) * 4;
        }

        return true;
    }

    bool LoadAsRGBA( const std::string & path, Image & image, const int32_t targetWidth, const int32_t targetHeight )
    {
        if ( targetWidth <= 0 || targetHeight <= 0 ) {
            return false;
        }

        std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> loadedSurface( nullptr, SDL_DestroySurface );

#if defined( WITH_IMAGE )
        loadedSurface.reset( IMG_Load( System::encLocalToUTF8( path ).c_str() ) );
#else
        loadedSurface.reset( SDL_LoadBMP( System::encLocalToUTF8( path ).c_str() ) );
#endif
        if ( !loadedSurface ) {
            return false;
        }

        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> rgbaSurface( SDL_ConvertSurface( loadedSurface.get(), SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !rgbaSurface ) {
            return false;
        }

        // Create a target surface and scale into it.
        const std::unique_ptr<SDL_Surface, void ( * )( SDL_Surface * )> scaledSurface(
            SDL_CreateSurface( targetWidth, targetHeight, SDL_PIXELFORMAT_RGBA32 ), SDL_DestroySurface );
        if ( !scaledSurface ) {
            return false;
        }

        if ( !SDL_BlitSurfaceScaled( rgbaSurface.get(), nullptr, scaledSurface.get(), nullptr, SDL_SCALEMODE_LINEAR ) ) {
            return false;
        }

        image = Image( targetWidth, targetHeight, ImageFormat::RGBA_32BIT );

        const uint8_t * srcRow = static_cast<const uint8_t *>( scaledSurface->pixels );
        uint8_t * dstRow = image.image();

        for ( int32_t y = 0; y < targetHeight; ++y ) {
            memcpy( dstRow, srcRow, static_cast<size_t>( targetWidth ) * 4 );
            srcRow += scaledSurface->pitch;
            dstRow += static_cast<ptrdiff_t>( targetWidth ) * 4;
        }

        return true;
    }

    Sprite decodeICNSprite( const uint8_t * data, const uint8_t * dataEnd, const ICNHeader & icnHeader )
    {
        Sprite sprite( icnHeader.width, icnHeader.height, icnHeader.offsetX, icnHeader.offsetY );
        sprite.reset();

        uint8_t * imageTransform = sprite.transform();

        uint32_t posX = 0;

        // The need for a transform layer can only be determined during ICN decoding.
        bool noTransformLayer = true;

        // When the 6th bit in animationFrames is set then it is Monochromatic ICN image.
        const bool isMonochromatic = ( icnHeader.animationFrames & 0x20 );

        if ( isMonochromatic ) {
            while ( data < dataEnd ) {
                if ( *data == 0 ) {
                    // 0x00 - end of row reached, go to the first pixel of next row.

                    noTransformLayer = noTransformLayer && ( static_cast<int32_t>( posX ) >= icnHeader.width );

                    imageTransform += icnHeader.width;
                    posX = 0;
                    ++data;
                }
                else if ( *data < 0x80 ) {
                    // 0x01-0x7F - number of black pixels.
                    // Image data is all already set to 0. Just set transform layer to 0.

                    const uint8_t pixelCount = *data;

                    memset( imageTransform + posX, static_cast<uint8_t>( 0 ), pixelCount );

                    ++data;
                    posX += pixelCount;
                }
                else if ( *data == 0x80 ) {
                    // 0x80 - end of image.

                    break;
                }
                else {
                    // 0x81 to 0xFF - number of empty (transparent) pixels + 0x80.
                    // The (n - 128) pixels are transparent.

                    noTransformLayer = false;

                    posX += *data - 0x80;
                    ++data;
                }
            }
        }
        else {
            uint8_t * imageData = sprite.image();

            while ( data < dataEnd ) {
                if ( *data == 0 ) {
                    // 0x00 - end of row reached, go to the first pixel of next row.
                    // All of remaining pixels of current line are transparent.

                    noTransformLayer = noTransformLayer && ( static_cast<int32_t>( posX ) >= icnHeader.width );

                    imageData += icnHeader.width;
                    imageTransform += icnHeader.width;
                    posX = 0;
                    ++data;
                }
                else if ( *data < 0x80 ) {
                    // 0x01-0x7F - number N of sprite pixels.
                    // The next N bytes are the colors of the next N pixels.

                    const uint8_t pixelCount = *data;
                    ++data;

                    if ( data + pixelCount > dataEnd ) {
                        // Image data is corrupted - we can not read data beyond dataEnd.
                        break;
                    }

                    memcpy( imageData + posX, data, pixelCount );
                    memset( imageTransform + posX, static_cast<uint8_t>( 0 ), pixelCount );

                    data += pixelCount;
                    posX += pixelCount;
                }
                else if ( *data == 0x80 ) {
                    // 0x80 - end of image

                    noTransformLayer = noTransformLayer && ( static_cast<int32_t>( posX ) >= icnHeader.width );

                    break;
                }
                else if ( *data < 0xC0 ) {
                    // 0x81 to 0xBF - number of empty (transparent) pixels + 0x80. The (n - 128) pixels are transparent.

                    noTransformLayer = false;

                    posX += *data - 0x80;
                    ++data;
                }
                else if ( *data == 0xC0 ) {
                    // 0xC0 - put here N transform layer pixels.
                    // If the next byte modulo 4 is not null, N equals the next byte modulo 4,
                    // otherwise N equals the second next byte.

                    noTransformLayer = false;

                    ++data;

                    const uint8_t transformValue = *data;

                    const uint32_t countValue = transformValue & 0x03;
                    const uint32_t pixelCount = ( countValue != 0 ) ? countValue : *( ++data );

                    if ( transformValue & 0x40 ) {
                        // Transform layer data types:
                        // 0 - no transparency,
                        // 1 - full transparency (to skip image data),
                        // from 5 (light) to 2 (strong) - for darkening,
                        // from 10 (light) to 6 (strong) - for lightening
                        const uint8_t transformType = static_cast<uint8_t>( ( ( transformValue & 0x3C ) >> 2 ) + 2 );

                        if ( transformType < 16 ) {
                            memset( imageTransform + posX, transformType, pixelCount );
                        }
                    }

                    // TODO: Use ( transformValue & 0x80 ) to detect and store shining contour data bit.
                    // It is used for units on the Battlefield and for icons in the View World.

                    posX += pixelCount;

                    ++data;
                }
                else {
                    // 0xC1 - next byte stores the number of next pixels of same color, or
                    // 0xC2 to 0xFF - the number of pixels of same color plus 0xC0.
                    // Next byte is the color of these pixels.

                    const uint32_t pixelCount = ( *data == 0xC1 ) ? *( ++data ) : *data - 0xC0;
                    ++data;

                    memset( imageData + posX, *data, pixelCount );
                    memset( imageTransform + posX, static_cast<uint8_t>( 0 ), pixelCount );

                    posX += pixelCount;

                    ++data;
                }
            }
        }

        if ( noTransformLayer ) {
            sprite._disableTransformLayer();
        }

        return sprite;
    }

    void decodeTILImages( const uint8_t * data, const size_t imageCount, const int32_t width, const int32_t height, std::vector<Image> & output )
    {
        assert( data != nullptr && imageCount > 0 && width > 0 && height > 0 );

        output.resize( imageCount );

        const size_t imageSize = static_cast<size_t>( width ) * height;

        for ( size_t i = 0; i < imageCount; ++i ) {
            Image & tilImage = output[i];
            tilImage._disableTransformLayer();
            tilImage.resize( width, height );
            memcpy( tilImage.image(), data + i * imageSize, imageSize );
        }
    }

    Sprite decodeBMPFile( const std::vector<uint8_t> & data )
    {
        if ( data.size() < 6 ) {
            // It is an invalid BMP file.
            return {};
        }

        ROStreamBuf imageStream( data );

        const uint8_t blackColor = imageStream.get();
        const uint8_t whiteColor = 11;

        // Skip the second byte
        imageStream.get();

        const int32_t width = imageStream.getLE16();
        const int32_t height = imageStream.getLE16();

        if ( static_cast<int32_t>( data.size() ) != 6 + width * height ) {
            // It is an invalid BMP file.
            return {};
        }

        fheroes2::Sprite output( width, height );

        const uint8_t * input = data.data() + 6;
        uint8_t * image = output.image();
        const uint8_t * imageEnd = image + static_cast<ptrdiff_t>( width ) * height;
        uint8_t * transform = output.transform();

        for ( ; image != imageEnd; ++image, ++transform, ++input ) {
            if ( *input == 1 ) {
                *image = whiteColor;
                *transform = 0;
            }
            else if ( *input == 2 ) {
                *image = blackColor;
                *transform = 0;
            }
            else {
                *transform = 1;
            }
        }

        return output;
    }

    bool isPNGFormatSupported()
    {
#if defined( WITH_IMAGE )
        return true;
#else
        return false;
#endif
    }
}
