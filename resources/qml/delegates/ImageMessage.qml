// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

Item {
    id: root

    property var roomAdapter: null
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
    // Bubble's content max width. Set externally by the bubble style (see TimelineBubbleBody.qml,
    // which reparents the delegate after construction; we can't read it from `parent` because the
    // chooser is no longer the visual parent once the bubble takes over). Caps the caption pill so
    // it never spills past the bubble.
    property int bubbleMaxWidth: 0
    // Width at which the image itself is rendered (image-native size, height-fit scaled).
    readonly property int imageDisplayWidth: Math.max(1, Math.round(tempWidth * Math.min((containerHeight / divisor) / (tempWidth * safeProportionalHeight), 1)))
    // Width the caption pill needs to render the text on a single line (paddings included).
    // Driven by a hidden, no-wrap TextEdit so we can measure unwrapped natural width even when the
    // visible TextEdit has wrapMode set. Math.ceil is critical: implicitWidth is a real (e.g.
    // 505.25), and truncating to int loses the fractional part, leaving the TextEdit 0.25px too
    // narrow and causing it to wrap by one character.
    readonly property int captionDesiredPillWidth: showPersistentCaption
        ? Math.ceil(captionWidthMeasurer.implicitWidth) + Komai.paddingMedium * 2
        : 0
    // The pill hugs the caption's natural width, but is bounded from below by the image width
    // (no L-shape reversal where the pill is narrower than the image) and from above by
    // bubbleMaxWidth (so a very long caption wraps inside the bubble instead of overflowing).
    readonly property int captionPillWidth: showPersistentCaption && bubbleMaxWidth > 0
        ? Math.max(imageDisplayWidth, Math.min(captionDesiredPillWidth, bubbleMaxWidth))
        : imageDisplayWidth
    // Layout box: widen only as much as the caption pill actually needs (capped at bubble width).
    // Normal images with normal-length captions stay at image width — same as before.
    implicitWidth: Math.max(imageDisplayWidth, captionPillWidth)
    width: Math.min(parent?.width ?? implicitWidth, implicitWidth)
    height: implicitHeight

    readonly property var roomContext: roomAdapter
        ? roomAdapter
        : (typeof effectiveRoomContext !== "undefined" && effectiveRoomContext)
        ? effectiveRoomContext
        : ((typeof room !== "undefined" && room) ? room : null)
    readonly property string hoverOverlayText: hasCaption ? body : (filename.length > 0 ? filename : body)

    // Allow expansion past the image's native width when a long caption needs more room.
    // Without this the chooser would clip the layout back to originalWidth.
    EventDelegateChooser.maxWidth: Math.max(originalWidth, captionPillWidth)

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
    readonly property bool perfDisableTimelineInteraction: TimelineManager.perfUiFlagEnabled("disable_timeline_interaction")

    implicitHeight: contentColumn.implicitHeight

    // Off-layout measurer used purely to compute the unwrapped natural width of the caption text.
    // The visible `persistentCaptionText` has wrapMode set, so its own implicitWidth is constrained
    // by current width and can't tell us how wide the text *would* like to be.
    // Using TextEdit (not Text) here so the measurement includes TextEdit's internal document
    // margins — otherwise the visible TextEdit would still wrap by a few pixels.
    TextEdit {
        id: captionWidthMeasurer
        visible: false
        text: root.body
        font: persistentCaptionText.font
        textFormat: TextEdit.PlainText
        wrapMode: TextEdit.NoWrap
        readOnly: true
    }

    Column {
        id: contentColumn

        width: root.width
        spacing: root.showPersistentCaption ? Komai.paddingSmall : 0

        Item {
            id: mediaFrame

            // Image-native size; do not stretch when root is widened to fit a long caption.
            width: root.imageDisplayWidth
            height: Math.max(1, Math.round(root.imageDisplayWidth * root.safeProportionalHeight))

            HoverHandler {
                id: mediaHover
                enabled: !root.perfDisableTimelineInteraction
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
                interactive: !EventDelegateChooser.isReply && !root.perfDisableTimelineInteraction
                revealEnabled: !EventDelegateChooser.isReply && !root.perfDisableTimelineInteraction
                onActivated: {
                    if (!root.roomContext)
                        return;

                    if (Settings.timelineMediaOpenImagesExternal) {
                        root.roomContext.openMedia(root.eventId);
                    } else if (typeof root.roomContext.openMediaOverlay === "function"
                            ) {
                        const handled = root.roomContext.openMediaOverlay(root.eventId);
                        if (handled)
                            return;
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
