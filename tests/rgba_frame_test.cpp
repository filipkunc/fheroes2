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

#include <array>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <limits>

#include "math_base.h"
#include "rgba_frame.h"

namespace
{
    constexpr fheroes2::RgbaPixel red{ 255, 0, 0, 255 };
    constexpr fheroes2::RgbaPixel green{ 0, 255, 0, 255 };
    constexpr fheroes2::RgbaPixel blue{ 0, 0, 255, 255 };
    constexpr fheroes2::RgbaPixel white{ 255, 255, 255, 255 };

    bool pixel( const fheroes2::RgbaFrame & frame, const int x, const int y, const fheroes2::RgbaPixel expected )
    {
        const auto actual = frame.data()[y * frame.width() + x];
        if ( actual.red == expected.red && actual.green == expected.green && actual.blue == expected.blue && actual.alpha == expected.alpha ) {
            return true;
        }
        std::cerr << "Unexpected RGBA pixel at " << x << ',' << y << '\n';
        return false;
    }

    bool testScaleAndPainterOrder()
    {
        const std::array<fheroes2::RgbaPixel, 4> source{ red, green, blue, white };
        const fheroes2::RgbaPixel translucent{ 0, 0, 255, 128 };
        for ( int scale = 1; scale <= 3; ++scale ) {
            fheroes2::RgbaFrame frame;
            if ( !frame.resize( { 2, 2 }, { 2 * scale, 2 * scale } ) || !frame.blit( { source.data(), 2, 2, 2 }, { 0, 0, 2, 2 }, { 0, 0, 2, 2 } ) ) {
                return false;
            }
            for ( int y = 0; y < frame.height(); ++y ) {
                for ( int x = 0; x < frame.width(); ++x ) {
                    if ( !pixel( frame, x, y, source[( y / scale ) * 2 + x / scale] ) ) {
                        return false;
                    }
                }
            }
            if ( !frame.blit( { &translucent, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) || !pixel( frame, 0, 0, { 127, 0, 128, 255 } )
                 || !pixel( frame, scale, 0, green ) || !frame.blit( { &red, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) || !pixel( frame, 0, 0, red ) ) {
                return false;
            }
        }
        return true;
    }

    bool testAlpha()
    {
        fheroes2::RgbaFrame frame;
        if ( !frame.resize( { 1, 1 }, { 1, 1 } ) ) {
            return false;
        }
        frame.clear( { 0, 0, 0, 0 } );
        const fheroes2::RgbaPixel halfRed{ 255, 0, 0, 128 };
        const fheroes2::RgbaPixel halfBlue{ 0, 0, 255, 128 };
        const fheroes2::RgbaPixel transparent{ 255, 255, 255, 0 };
        return frame.blit( { &halfRed, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) && pixel( frame, 0, 0, halfRed )
               && frame.blit( { &halfBlue, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) && pixel( frame, 0, 0, { 85, 0, 170, 192 } )
               && frame.blit( { &transparent, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) && pixel( frame, 0, 0, { 85, 0, 170, 192 } );
    }

    bool testClippingStrideAndNativeDetail()
    {
        const std::array<fheroes2::RgbaPixel, 6> padded{ red, green, white, blue, white, red };
        fheroes2::RgbaFrame frame;
        if ( !frame.resize( { 3, 3 }, { 3, 3 } ) || !frame.blit( { padded.data(), 2, 2, 3 }, { 0, 0, 2, 2 }, { -1, -1, 4, 4 } ) || !pixel( frame, 0, 0, red )
             || !pixel( frame, 2, 0, green ) || !pixel( frame, 0, 2, blue ) || !pixel( frame, 2, 2, white ) ) {
            return false;
        }
        frame.clear();
        if ( !frame.blit( { padded.data(), 2, 2, 3 }, { 1, 0, 1, 2 }, { 0, 0, 1, 2 } ) || !pixel( frame, 0, 0, green ) || !pixel( frame, 0, 1, white )
             || !pixel( frame, 1, 0, {} ) ) {
            return false;
        }
        // All four source pixels fit inside one logical pixel at native 2x resolution.
        return frame.resize( { 1, 1 }, { 2, 2 } ) && frame.blit( { padded.data(), 2, 2, 3 }, { 0, 0, 2, 2 }, { 0, 0, 1, 1 } ) && pixel( frame, 0, 0, red )
               && pixel( frame, 1, 0, green ) && pixel( frame, 0, 1, blue ) && pixel( frame, 1, 1, white );
    }

    bool testEdgesAndInvalidInput()
    {
        fheroes2::RgbaFrame frame;
        if ( !frame.resize( { 3, 1 }, { 5, 1 } ) || !frame.blit( { &red, 1, 1, 1 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } )
             || !frame.blit( { &green, 1, 1, 1 }, { 0, 0, 1, 1 }, { 1, 0, 1, 1 } ) || !frame.blit( { &blue, 1, 1, 1 }, { 0, 0, 1, 1 }, { 2, 0, 1, 1 } )
             || !pixel( frame, 0, 0, red ) || !pixel( frame, 1, 0, green ) || !pixel( frame, 2, 0, green ) || !pixel( frame, 3, 0, blue )
             || !pixel( frame, 4, 0, blue ) ) {
            return false;
        }
        const std::array<fheroes2::RgbaPixel, 2> pair{ red, green };
        return frame.blit( { pair.data(), 2, 1, 2 }, { 0, 0, 2, 1 }, { -1, 0, 2, 1 } ) && pixel( frame, 0, 0, green ) && !frame.resize( { 0, 1 }, { 5, 1 } )
               && !frame.resize( { 3, 1 }, { 16384, 16384 } ) && !frame.blit( {}, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } )
               && !frame.blit( { &red, 1, 1, 0 }, { 0, 0, 1, 1 }, { 0, 0, 1, 1 } ) && !frame.blit( { &red, 1, 1, 1 }, { 0, 0, 2, 1 }, { 0, 0, 1, 1 } )
               && frame.blit( { &red, 1, 1, 1 }, { 0, 0, 1, 1 }, { std::numeric_limits<int32_t>::max(), 0, 1, 1 } ) && pixel( frame, 0, 0, green );
    }
}

int main()
{
    if ( !testScaleAndPainterOrder() || !testAlpha() || !testClippingStrideAndNativeDetail() || !testEdgesAndInvalidInput() ) {
        return EXIT_FAILURE;
    }
    std::cout << "Physical RGBA composition passed at 1x, 2x and 3x\n";
    return EXIT_SUCCESS;
}
