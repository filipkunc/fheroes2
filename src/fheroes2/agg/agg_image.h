/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2021 - 2025                                             *
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
    class Image;
    class Sprite;

    enum class SupportedLanguage : uint8_t;

    namespace AGG
    {
        const Sprite & GetICN( int icnId, uint32_t index );
        uint32_t GetICNCount( int icnId );

        // shapeId could be 0, 1, 2 or 3 only
        const Image & GetTIL( int tilId, uint32_t index, uint32_t shapeId );

        // This function must be called only at the time of setting up a new language.
        void updateLanguageDependentResources( const SupportedLanguage language, const bool loadOriginalAlphabet );

        // High-resolution RGBA animation frames for custom monsters (Thor, Succubus, ...).
        // Returns nullptr if no RGBA PNGs were found. Each Image is RGBA_32BIT format.
        const std::vector<Image> * GetRGBACustomFrames( const int monsterId );

        // Portrait-ready single image cropped to the opaque bounding box of frame 1.
        // Returns nullptr if no RGBA PNGs are available. The Image is RGBA_32BIT format.
        const Image * GetRGBACustomPortrait( const int monsterId );

        // Paint a hi-res monster portrait / frame at (gameX, gameY) with logical width gameWidth
        // in game-space pixels. Writes directly to Display (RGBA at game res). Z-order is the
        // order in which the caller invokes drawing primitives.
        void renderHiResMonsterPortrait( const Image & portrait, int32_t gameX, int32_t gameY, int32_t gameWidth, bool flip = false, uint8_t alpha = 255 );
    }
}
