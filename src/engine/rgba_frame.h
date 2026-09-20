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

#pragma once

#include <cstdint>
#include <vector>

#include "math_base.h"

namespace fheroes2
{
    struct RgbaPixel
    {
        uint8_t red{ 0 };
        uint8_t green{ 0 };
        uint8_t blue{ 0 };
        uint8_t alpha{ 255 };
    };

    static_assert( sizeof( RgbaPixel ) == 4, "RGBA pixels must have no padding" );

    struct RgbaView
    {
        const RgbaPixel * pixels{ nullptr };
        int32_t width{ 0 };
        int32_t height{ 0 };
        // Stride in pixels. The caller owns at least stride * height pixels, without aliasing the destination.
        int32_t stride{ 0 };
    };

    // A physical game-viewport canvas. Drawing destinations remain in logical coordinates.
    class RgbaFrame
    {
    public:
        // Invalid dimensions leave the previous canvas intact.
        bool resize( Size logicalSize, Size physicalSize );
        void clear( RgbaPixel color = {} );
        // Validates the complete source before modifying the frame. Destination clipping preserves sampling.
        bool blit( const RgbaView & source, const Rect & sourceRect, const Rect & logicalDestination );

        int32_t width() const
        {
            return _physicalSize.width;
        }
        int32_t height() const
        {
            return _physicalSize.height;
        }
        int32_t pitch() const
        {
            return width() * 4;
        }
        const RgbaPixel * data() const
        {
            return _pixels.data();
        }

    private:
        Size _logicalSize;
        Size _physicalSize;
        std::vector<RgbaPixel> _pixels;
    };
}
