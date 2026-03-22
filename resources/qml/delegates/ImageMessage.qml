// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    required property int originalWidth
    required property int originalHeight
    required property double proportionalHeight
    required property string url
    required property string blurhash
    required property string body
    required property string filename
    required property string eventId
    required property int containerHeight

    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    property int tempWidth: originalWidth < 1 ? 400 : originalWidth
    readonly property double safeProportionalHeight: proportionalHeight > 0
                                                   ? proportionalHeight
                                                   : ((originalWidth > 0 && originalHeight > 0) ? (originalHeight / originalWidth) : 1.0)
    // Bubble layout resolves width from delegates' implicitWidth. Provide explicit media sizing here
    // so image messages don't collapse to near-zero width in bubble style.
    implicitWidth: Math.max(1, Math.round(tempWidth * Math.min((containerHeight / divisor) / (tempWidth * safeProportionalHeight), 1)))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: mediaFrame.height
    implicitHeight: height

    readonly property var roomContext: (typeof room !== "undefined") ? room : null
    readonly property string hoverOverlayText: hasCaption ? body : filename

    EventDelegateChooser.keepAspectRatio: true
    EventDelegateChooser.maxWidth: originalWidth
    EventDelegateChooser.maxHeight: containerHeight / divisor
    EventDelegateChooser.aspectRatio: safeProportionalHeight

    // A non-empty body that doesn't look like a filename is treated as a real caption
    readonly property bool hasCaption: body.length > 0 && !body.match(/\.\w{2,5}$/)

    property int metadataWidth
    property bool fitsMetadata: parent != null ? (parent.width - width) > metadataWidth+4 : false

    Item {
        id: mediaFrame

        width: root.width
        height: Math.max(1, Math.round(root.width * root.safeProportionalHeight))

        HoverHandler {
            id: mediaHover
        }

        MediaImageSurface {
            anchors.fill: parent
            originalWidth: root.originalWidth
            safeProportionalHeight: root.safeProportionalHeight
            url: root.url
            blurhash: root.blurhash
            eventId: root.eventId
            roomContext: root.roomContext
            hovered: mediaHover.hovered
            interactive: !EventDelegateChooser.isReply
            revealEnabled: !EventDelegateChooser.isReply
            onActivated: {
                if (!root.roomContext)
                    return;

                if (Settings.timelineMediaOpenImagesExternal) {
                    root.roomContext.openMedia(root.eventId);
                } else {
                    TimelineManager.openMediaOverlayWithContext(root.roomContext,
                                                                root.url,
                                                                root.eventId,
                                                                root.originalWidth,
                                                                root.proportionalHeight,
                                                                timeline,
                                                                timelineView);
                }
            }
        }

        Item {
            id: overlay

            anchors.fill: parent

            visible: root.hasCaption || mediaHover.hovered

            Rectangle {
                id: container

                width: parent.width
                implicitHeight: imgcaption.implicitHeight
                anchors.bottom: overlay.bottom
                color: palette.window
                opacity: 0.75
            }

            Text {
                id: imgcaption

                anchors.fill: container
                elide: Text.ElideMiddle
                horizontalAlignment: Text.AlignHCenter
                verticalAlignment: Text.AlignVCenter
                // See this MSC: https://github.com/matrix-org/matrix-doc/pull/2530
                text: root.hoverOverlayText
                color: palette.text
            }

        }
    }
}
