// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import im.nheko

import "../bubble"

TimelineBubbleMessageStyle {
    // Plain mode reuses bubble positioning/layout but disables bubble chrome.
    styleFileMessagePadding: 12
    styleShowFileMessageBackground: true
    styleShowEncryptedMessageBackground: true

    messageBubblePadding: 0
    messageBubbleBackgroundEnabled: false
    alignMessageTextToSide: true
    reserveAvatarRowHeight: true
}
