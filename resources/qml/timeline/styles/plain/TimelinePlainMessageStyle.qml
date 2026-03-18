// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

import "../bubble"

TimelineBubbleMessageStyle {
    // Plain mode reuses bubble positioning/layout but disables bubble chrome.
    // Plain style has no bubble chrome, so add small horizontal content padding to keep text from
    // touching the hover-highlighted container. Bubble style already has bubble padding/chrome.
    styleFileMessagePadding: 12
    styleShowFileMessageBackground: true
    styleShowEncryptedMessageBackground: true

    messageBubblePadding: 0
    messageBubbleBackgroundEnabled: false
    messageBubbleHorizontalPadding: Komai.paddingSmall
    messageBubbleVerticalPadding: 0
    alignMessageTextToSide: true
    reserveAvatarRowHeight: startsNewMessageGroup
    pushMetadataToEdge: true
    alignBubbleToTop: true
}
