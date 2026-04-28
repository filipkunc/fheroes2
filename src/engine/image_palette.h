/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2021 - 2022                                             *
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

namespace fheroes2
{
    const uint8_t * getGamePalette();

    // This function must be called only at the start of the application after loading AGG file content.
    void setGamePalette( const std::vector<uint8_t> & palette );

    // Returns the 8-bit RGB palette currently used by paletteIdxToRGBA when converting indexed
    // sprite pixels to RGBA framebuffer pixels. Defaults to the static gamePalette (KB.PAL)
    // expanded from 6-bit to 8-bit; while a video plays, Display::changePalette points this at
    // the video's own 8-bit palette so sprite blits that happen during playback (the X_IVY
    // background of the POL campaign-selection screen, etc.) render with the colours the
    // artwork was authored against — matching the indexed-Display behaviour pre-RGBA refactor.
    const uint8_t * getRenderPalette8Bit();

    // Set the 8-bit RGB palette used for indexed→RGBA blits. Pass nullptr to revert to the
    // default (gamePalette expanded to 8-bit). Caller owns the storage; the engine just keeps a
    // pointer to it. Cleared by ScreenPaletteRestorer's destructor.
    void setRenderPalette8Bit( const uint8_t * palette );
}
