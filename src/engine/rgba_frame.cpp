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

#include "rgba_frame.h"

#include <algorithm>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <vector>

#include "math_base.h"

namespace
{
    int64_t scaledEdge( const int64_t edge, const int32_t physical, const int32_t logical )
    {
        const int64_t numerator = edge * physical;
        // C++ division truncates toward zero, while negative edges must round toward minus infinity.
        return numerator / logical - ( numerator % logical < 0 ? 1 : 0 );
    }

    fheroes2::RgbaPixel sourceOver( const fheroes2::RgbaPixel source, const fheroes2::RgbaPixel destination )
    {
        if ( source.alpha == 0 ) {
            return destination;
        }
        if ( source.alpha == 255 ) {
            return source;
        }
        const uint32_t inverseAlpha = 255U - source.alpha;
        const uint32_t alpha = source.alpha * 255U + destination.alpha * inverseAlpha;
        const auto channel = [&]( const uint8_t src, const uint8_t dst ) {
            return static_cast<uint8_t>( ( src * source.alpha * 255U + dst * destination.alpha * inverseAlpha + alpha / 2 ) / alpha );
        };
        return { channel( source.red, destination.red ), channel( source.green, destination.green ), channel( source.blue, destination.blue ),
                 static_cast<uint8_t>( ( alpha + 127 ) / 255 ) };
    }
}

bool fheroes2::RgbaFrame::resize( const Size logicalSize, const Size physicalSize )
{
    if ( logicalSize.width <= 0 || logicalSize.height <= 0 || logicalSize.width > 16384 || logicalSize.height > 16384 || physicalSize.width <= 0
         || physicalSize.height <= 0 || physicalSize.width > 16384 || physicalSize.height > 16384
         || static_cast<int64_t>( physicalSize.width ) * physicalSize.height > 64000000 ) {
        return false;
    }
    if ( _physicalSize != physicalSize ) {
        std::vector<RgbaPixel> pixels( static_cast<size_t>( physicalSize.width ) * physicalSize.height );
        _pixels.swap( pixels );
    }
    _logicalSize = logicalSize;
    _physicalSize = physicalSize;
    return true;
}

void fheroes2::RgbaFrame::clear( const RgbaPixel color )
{
    std::fill( _pixels.begin(), _pixels.end(), color );
}

bool fheroes2::RgbaFrame::blit( const RgbaView & source, const Rect & sourceRect, const Rect & logicalDestination )
{
    if ( _pixels.empty() || source.pixels == nullptr || source.width <= 0 || source.height <= 0 || source.width > 16384 || source.height > 16384
         || source.stride < source.width || static_cast<uint64_t>( source.stride ) * source.height > std::numeric_limits<size_t>::max() / sizeof( RgbaPixel )
         || sourceRect.x < 0 || sourceRect.y < 0 || sourceRect.width <= 0 || sourceRect.height <= 0
         || static_cast<int64_t>( sourceRect.x ) + sourceRect.width > source.width || static_cast<int64_t>( sourceRect.y ) + sourceRect.height > source.height
         || logicalDestination.width <= 0 || logicalDestination.height <= 0 ) {
        return false;
    }

    const int64_t left = scaledEdge( logicalDestination.x, width(), _logicalSize.width );
    const int64_t top = scaledEdge( logicalDestination.y, height(), _logicalSize.height );
    const int64_t right = scaledEdge( static_cast<int64_t>( logicalDestination.x ) + logicalDestination.width, width(), _logicalSize.width );
    const int64_t bottom = scaledEdge( static_cast<int64_t>( logicalDestination.y ) + logicalDestination.height, height(), _logicalSize.height );
    const int64_t clippedLeft = std::max<int64_t>( left, 0 );
    const int64_t clippedTop = std::max<int64_t>( top, 0 );
    const int64_t clippedRight = std::min<int64_t>( right, width() );
    const int64_t clippedBottom = std::min<int64_t>( bottom, height() );
    if ( clippedLeft >= clippedRight || clippedTop >= clippedBottom ) {
        return true;
    }

    for ( int64_t y = clippedTop; y < clippedBottom; ++y ) {
        const int64_t sourceY = sourceRect.y + ( ( 2 * ( y - top ) + 1 ) * sourceRect.height ) / ( 2 * ( bottom - top ) );
        for ( int64_t x = clippedLeft; x < clippedRight; ++x ) {
            const int64_t sourceX = sourceRect.x + ( ( 2 * ( x - left ) + 1 ) * sourceRect.width ) / ( 2 * ( right - left ) );
            RgbaPixel & output = _pixels[static_cast<size_t>( y ) * width() + static_cast<size_t>( x )];
            output = sourceOver( source.pixels[static_cast<size_t>( sourceY ) * source.stride + static_cast<size_t>( sourceX )], output );
        }
    }
    return true;
}
