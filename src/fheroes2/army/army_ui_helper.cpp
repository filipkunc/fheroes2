/***************************************************************************
 *   fheroes2: https://github.com/ihhub/fheroes2                           *
 *   Copyright (C) 2021 - 2023                                             *
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

#include "army_ui_helper.h"

#include <cassert>
#include <cstddef>
#include <string>
#include <utility>

#include "agg_image.h"
#include "army.h"
#include "army_troop.h"
#include "game.h"
#include "icn.h"
#include "image.h"
#include "screen.h"
#include "ui_text.h"

void fheroes2::drawMiniMonsters( const Troops & troops, int32_t cx, const int32_t cy, const int32_t width, uint32_t first, uint32_t count, const bool isCompact,
                                 const bool isDetailedView, const bool isGarrisonView, const uint32_t thievesGuildsCount, Image & output )
{
    if ( !troops.isValid() ) {
        return;
    }

    bool isRightToLeftRender = false;

    if ( count == 0 ) {
        // If this function was called with 'count == 0' than the count is determined by the number of troop slots.
        count = troops.GetOccupiedSlotCount();

        if ( first == 0 ) {
            // This is the case when we render all the troops and do it from left to right.
            // It is done to make troop sprites overlapping more appealing when troops are close to each other.
            // This case may occur if many troops were killed during the battle (lots of summoned elementals and/or mirror image troops).
            isRightToLeftRender = true;
        }
    }

    const double chunk = width / static_cast<double>( count );

    if ( !isCompact ) {
        cx += static_cast<int32_t>( chunk / 2 );
    }
    if ( !isRightToLeftRender ) {
        cx += width;
    }

    // No stale-paint cleanup needed — hi-res portraits are direct-blitted into _screenRGBA each
    // call. The palette row redraw runs first (WriteHook mirrors it), then this loop direct-blits
    // the new portraits on top via renderHiResMonsterPortrait.

    const size_t slots = troops.Size();
    for ( size_t slot = 0; slot < slots; ++slot ) {
        if ( count == 0 ) {
            // We have rendered the given count of troops. There is nothing more to do.
            break;
        }

        const Troop * troop = troops.GetTroop( isRightToLeftRender ? ( slots - slot - 1 ) : slot );

        if ( troop == nullptr || !troop->isValid() ) {
            continue;
        }

        if ( first != 0 ) {
            --first;
            continue;
        }

        std::string monstersCountRepresentation;

        if ( isDetailedView || !isGarrisonView ) {
            monstersCountRepresentation = Game::formatMonsterCount( troop->GetCount(), isDetailedView, isCompact );
        }
        else {
            assert( thievesGuildsCount > 0 );

            if ( thievesGuildsCount == 1 ) {
                monstersCountRepresentation = "???";
            }
            else {
                monstersCountRepresentation = Army::SizeString( troop->GetCount() );
            }
        }

        const fheroes2::Sprite & monster = fheroes2::AGG::GetICN( ICN::MONS32, troop->GetSpriteIndex() );
        const fheroes2::Text text( std::move( monstersCountRepresentation ), fheroes2::FontType::smallWhite() );

        const int32_t posX = isRightToLeftRender ? ( cx + static_cast<int32_t>( chunk * ( count - 1 ) ) ) : ( cx - static_cast<int32_t>( chunk * count ) );

        // For custom monsters with hi-res PNGs (Thor, Succubus, ...), route the portrait through
        // renderHiResMonsterPortrait so it direct-paints into the active forwarding RGBA surface.
        // The painter compositor alpha-composites that surface onto the screen framebuffer.
        const fheroes2::RGBAImage * portrait = fheroes2::AGG::GetRGBACustomPortrait( troop->GetID() );
        const bool useCustomPortrait = ( portrait != nullptr && !portrait->empty() );

        // This is the drawing of army troops in compact form in the small info window beneath resources
        if ( isCompact ) {
            const int32_t offsetY = ( monster.height() < 37 ) ? 37 - monster.height() : 0;
            int32_t offset = ( static_cast<int32_t>( chunk ) - monster.width() - text.width() ) / 2;
            if ( offset < 0 ) {
                offset = 0;
            }
            const int32_t blitX = posX + offset;
            const int32_t blitY = cy + offsetY + monster.y();
            if ( useCustomPortrait ) {
                const int32_t srcW = portrait->width();
                const int32_t srcH = portrait->height();
                const int32_t boxW = monster.width();
                const int32_t boxH = monster.height();
                int32_t overlayW = boxW;
                int32_t overlayH = ( static_cast<int64_t>( srcH ) * boxW ) / srcW;
                if ( overlayH > boxH ) {
                    overlayH = boxH;
                    overlayW = ( static_cast<int64_t>( srcW ) * boxH ) / srcH;
                }
                const int32_t overlayX = blitX + ( boxW - overlayW ) / 2;
                const int32_t overlayY = blitY + ( boxH - overlayH );
                fheroes2::AGG::renderHiResMonsterPortrait( *portrait, overlayX, overlayY, overlayW );
            }
            else {
                fheroes2::Blit( monster, output, blitX, blitY );
            }
            text.draw( posX - text.width() - offset + static_cast<int32_t>( chunk ), cy + 23, output );
        }
        else {
            // Non-compact path (quickinfo popups, kingdom overview lists, battle casualty
            // dialog). The active forwarding target is always the topmost dialog or the
            // adventure-map screenRGBA, so renderHiResMonsterPortrait direct-paints into it.
            const int32_t offsetY = 28 - monster.height() + monster.y();
            const int32_t blitX = posX - monster.width() / 2 + monster.x() + 2;
            const int32_t blitY = cy + offsetY;
            if ( useCustomPortrait ) {
                const int32_t srcW = portrait->width();
                const int32_t srcH = portrait->height();
                const int32_t boxW = monster.width();
                const int32_t boxH = monster.height();
                int32_t overlayW = boxW;
                int32_t overlayH = ( static_cast<int64_t>( srcH ) * boxW ) / srcW;
                if ( overlayH > boxH ) {
                    overlayH = boxH;
                    overlayW = ( static_cast<int64_t>( srcW ) * boxH ) / srcH;
                }
                const int32_t overlayX = blitX + ( boxW - overlayW ) / 2;
                const int32_t overlayY = blitY + ( boxH - overlayH );
                fheroes2::AGG::renderHiResMonsterPortrait( *portrait, overlayX, overlayY, overlayW );
            }
            else {
                fheroes2::Blit( monster, output, blitX, blitY );
            }
            text.draw( posX - text.width() / 2, cy + 29, output );
        }

        --count;
    }
}
