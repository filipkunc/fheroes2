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

#include <cstdint>
#include <iostream>
#include <string>

#include "image.h"

namespace
{
    bool expectPixel( const fheroes2::Image & image, const int32_t x, const int32_t y, const uint8_t expected, const std::string & context )
    {
        const uint8_t actual = image.image()[y * image.width() + x];
        if ( actual == expected ) {
            return true;
        }

        std::cerr << context << ": expected pixel (" << x << ", " << y << ") to be " << static_cast<int>( expected ) << ", got " << static_cast<int>( actual ) << '\n';
        return false;
    }

    bool testPainterOrder()
    {
        fheroes2::Image canvas( 5, 5 );
        canvas.fill( 3 );

        fheroes2::Image firstLayer( 3, 3 );
        firstLayer.fill( 17 );

        fheroes2::Image secondLayer( 2, 2 );
        secondLayer.fill( 42 );

        fheroes2::Blit( firstLayer, canvas, 1, 1 );
        fheroes2::Blit( secondLayer, canvas, 2, 2 );

        return expectPixel( canvas, 0, 0, 3, "painter order background" ) && expectPixel( canvas, 1, 1, 17, "painter order first layer" )
               && expectPixel( canvas, 2, 2, 42, "painter order overlap" ) && expectPixel( canvas, 3, 3, 42, "painter order second layer" );
    }

    bool testEdgeClipping()
    {
        fheroes2::Image canvas( 4, 4 );
        canvas.fill( 5 );

        fheroes2::Image source( 3, 3 );
        source.fill( 31 );

        fheroes2::Blit( source, canvas, -1, -1 );

        return expectPixel( canvas, 0, 0, 31, "clipped top-left" ) && expectPixel( canvas, 1, 1, 31, "clipped source extent" )
               && expectPixel( canvas, 2, 0, 5, "clipped right boundary" ) && expectPixel( canvas, 0, 2, 5, "clipped bottom boundary" );
    }

    bool testNearestNeighborScaling()
    {
        fheroes2::Image source( 2, 2 );
        source.fill( 0 );

        uint8_t * sourcePixels = source.image();
        sourcePixels[0] = 11;
        sourcePixels[1] = 22;
        sourcePixels[2] = 33;
        sourcePixels[3] = 44;

        for ( int32_t scale = 1; scale <= 3; ++scale ) {
            fheroes2::Image output( source.width() * scale, source.height() * scale );
            output.reset();
            fheroes2::Resize( source, output );

            for ( int32_t y = 0; y < output.height(); ++y ) {
                for ( int32_t x = 0; x < output.width(); ++x ) {
                    const uint8_t expected = sourcePixels[( y / scale ) * source.width() + x / scale];
                    if ( !expectPixel( output, x, y, expected, "nearest-neighbor " + std::to_string( scale ) + "x" ) ) {
                        return false;
                    }
                }
            }
        }

        return true;
    }
}

int main()
{
    if ( !testPainterOrder() || !testEdgeClipping() || !testNearestNeighborScaling() ) {
        return 1;
    }

    std::cout << "Synthetic renderer tests passed.\n";
    return 0;
}
