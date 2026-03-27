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
    required property string mimetype
    required property int containerHeight

    property double divisor: EventDelegateChooser.isReply ? 10 : 4
    // When the server omits image dimensions use a modest fallback so
    // the bubble stays compact until the actual image loads and the
    // cache picks up the true height.
    property int tempWidth: originalWidth < 1 ? 240 : originalWidth
    readonly property double safeProportionalHeight: proportionalHeight > 0
                                                   ? proportionalHeight
                                                   : ((originalWidth > 0 && originalHeight > 0) ? (originalHeight / originalWidth) : 0.75)
    // Bubble layout resolves width from delegates' implicitWidth. Provide explicit media sizing here
    // so image messages don't collapse to near-zero width in bubble style.
    implicitWidth: Math.max(1, Math.round(tempWidth * Math.min((containerHeight / divisor) / (tempWidth * safeProportionalHeight), 1)))
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: implicitHeight

    readonly property var roomContext: (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)
    readonly property string hoverOverlayText: hasCaption ? body : (filename.length > 0 ? filename : body)

    EventDelegateChooser.maxWidth: originalWidth

    // A non-empty body that doesn't look like a filename is treated as a real caption
    readonly property bool hasCaption: body.length > 0 && !body.match(/\.\w{2,5}$/)
    readonly property bool showPersistentCaption: hasCaption && !EventDelegateChooser.isReply
    readonly property bool showHoverOverlay: mediaHover.hovered
                                             && hoverOverlayText.length > 0
                                             && !showPersistentCaption
    property string copyText: persistentCaptionText.selectedText.length > 0
        ? persistentCaptionText.selectedText
        : body

    property int metadataWidth
    property bool fitsMetadata: parent != null ? (parent.width - width) > metadataWidth + 4 : false

    implicitHeight: contentColumn.implicitHeight

    Column {
        id: contentColumn

        width: root.width
        spacing: root.showPersistentCaption ? Komai.paddingSmall : 0

        Item {
            id: mediaFrame

            width: parent.width
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
                mimeType: root.mimetype
                roomContext: root.roomContext
                hovered: mediaHover.hovered
                interactive: !EventDelegateChooser.isReply
                revealEnabled: !EventDelegateChooser.isReply
                onActivated: {
                    if (!root.roomContext)
                        return;

                    if (Settings.timelineMediaOpenImagesExternal) {
                        root.roomContext.openMedia(root.eventId);
                    } else if (root.roomContext.isActiveMatrixTimelineRoom === true) {
                        TimelineManager.openMediaOverlay(null,
                                                         root.url,
                                                         root.eventId,
                                                         root.originalWidth,
                                                         root.proportionalHeight);
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

                visible: root.showHoverOverlay

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

        Rectangle {
            id: persistentCaptionContainer

            visible: root.showPersistentCaption
            width: parent.width
            implicitHeight: persistentCaptionText.implicitHeight + Komai.paddingSmall * 2
            radius: 8
            color: Qt.rgba(palette.window.r, palette.window.g, palette.window.b, 0.92)

            TextEdit {
                id: persistentCaptionText

                width: Math.max(1, parent.width - Komai.paddingMedium * 2)
                x: Komai.paddingMedium
                y: Komai.paddingSmall
                readOnly: true
                selectByMouse: true
                selectionColor: palette.highlight
                selectedTextColor: palette.highlightedText
                wrapMode: TextEdit.Wrap
                textFormat: TextEdit.PlainText
                text: root.body
                color: palette.text
            }
        }
    }
}
