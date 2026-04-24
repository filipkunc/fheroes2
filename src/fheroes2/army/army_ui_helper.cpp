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

    // Wipe any hi-res portrait paints left in this row's rect by a previous call — the troop
    // list may have changed (focus switched to a hero without the custom monster, the unit was
    // dismissed, etc.) and persistent buffer paints would otherwise ghost the stale portrait on
    // top of whatever now occupies the slot. We clear against the currently active forwarding
    // target, which is the surface renderHiResMonsterPortrait will register into below.
    const fheroes2::Image::DialogForwardingFrame * activeForFrame = fheroes2::Image::getActiveDialogForwarding();
    if ( activeForFrame != nullptr && activeForFrame->target != nullptr ) {
        // Height is generous — covers the vertical range portraits can land at in either layout.
        const int32_t clearH = isCompact ? 45 : 60;
        const int32_t clearY = isCompact ? cy : ( cy - 30 );
        fheroes2::Display::instance().removeRGBABufferPaintsInRect( activeForFrame->target, cx - width, clearY, width * 2, clearH );
    }

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
        // renderHiResMonsterPortrait so that when a host screen later installs RGBA forwarding the
        // portrait direct-paints into the surface instead of registering a separate SDL overlay.
        // Today on the adventure map no forwarding is active and the helper falls back to
        // Display::addRGBAOverlay — same behaviour as before, new plumbing.
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
            // dialog). Post-Phase 3b there is always an outer forwarding target on the stack —
            // the adventure-map screenRGBA at minimum, or a dialog / battle surface nested on
            // top — so renderHiResMonsterPortrait takes the direct-paint path, never the
            // addRGBAOverlay fallback that was the leak concern before Phase 3b.
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
