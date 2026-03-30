// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    id: root

    property var roomAdapter: null
    required property int originalWidth
    required property int originalHeight
    required property double proportionalHeight
    required property string url
    required property string blurhash
    required property string eventId
    required property string mimetype
    required property int containerHeight

    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1 ? 400 : originalWidth
    readonly property double safeProportionalHeight: proportionalHeight > 0
                                                   ? proportionalHeight
                                                   : ((originalWidth > 0 && originalHeight > 0) ? (originalHeight / originalWidth) : 1.0)
    readonly property var roomContext: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)

    implicitWidth: Math.max(1, Math.round(tempWidth * Math.min((containerHeight / divisor) / (tempWidth * safeProportionalHeight), 1)))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: Math.max(1, Math.round(width * safeProportionalHeight))
    implicitHeight: height

    EventDelegateChooser.keepAspectRatio: true
    EventDelegateChooser.maxWidth: originalWidth
    EventDelegateChooser.maxHeight: containerHeight / divisor
    EventDelegateChooser.aspectRatio: safeProportionalHeight

    property int metadataWidth
    property bool fitsMetadata: parent != null ? (parent.width - width) > metadataWidth + 4 : false

    MediaImageSurface {
        anchors.fill: parent
        originalWidth: root.originalWidth
        safeProportionalHeight: root.safeProportionalHeight
        url: root.url
        blurhash: root.blurhash
        eventId: root.eventId
        mimeType: root.mimetype
        roomContext: root.roomContext
        revealEnabled: !EventDelegateChooser.isReply
    }
}
