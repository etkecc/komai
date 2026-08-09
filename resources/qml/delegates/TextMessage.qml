// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

LitehtmlItem {
    id: litehtmlRoot
    required property string body
    property int eventType: MtxEvent.UnknownMessage
    property int isOnlyEmoji: eventType === MtxEvent.TextMessage
        ? TimelineManager.emojiOnlyCodepointCount(body)
        : 0
    property bool isReply: EventDelegateChooser.isReply
    required property bool keepFullText
    required property string formatted
    // In-room search query, propagated from MatrixRoomView. When non-empty,
    // matches inside `formatted` are wrapped in <mark> before rendering.
    property string searchQuery: ""
    property point hoverPoint: Qt.point(0, 0)
    readonly property bool emojiOnlyMessage: isOnlyEmoji > 0 && isOnlyEmoji < 4
    readonly property bool enlargedEmojiOnly: Settings.timelineMessagesEmojiOnlyEnlarge && emojiOnlyMessage
    readonly property bool perfDisableTimelineInteraction: TimelineManager.perfUiFlagEnabled("disable_timeline_interaction")

    // Collapsible large messages
    property real timelineViewportHeight: 0
    readonly property int maxCollapsedHeight: timelineViewportHeight > 0 ? Math.max(150, Math.round(timelineViewportHeight * 0.5)) : 300
    readonly property bool collapsible: !isReply && implicitHeight > maxCollapsedHeight
    property bool collapsed: true
    readonly property real collapseControlsHeight: showMoreBar.visible ? showMoreBar.implicitHeight : 0
    // Overflow to extend "Show more" bar to bubble container edges.
    // Parent chain: TextMessage → Column → contentItem → messageBubble.
    // Column children are left-aligned by default and don't stretch to
    // fill the Column width, so the right overflow must also cover the
    // gap between litehtmlRoot's right edge and the Column's right edge.
    readonly property real collapseOverflowLeft: {
        let bubble = parent?.parent?.parent;
        return (bubble && typeof bubble.leftPadding === "number") ? bubble.leftPadding : 0;
    }
    readonly property real collapseOverflowRight: {
        let col = parent;
        let bubble = col?.parent?.parent;
        if (!bubble || typeof bubble.rightPadding !== "number") return 0;
        let columnGap = col ? Math.max(0, col.width - width) : 0;
        return bubble.rightPadding + columnGap;
    }
    readonly property real collapseOverflowV: {
        let bubble = parent?.parent?.parent;
        return (bubble && typeof bubble.bottomPadding === "number") ? bubble.bottomPadding : 0;
    }

    height: collapsible ? (collapsed ? maxCollapsedHeight : implicitHeight + collapseControlsHeight) : implicitHeight
    // No QML clip — QQuickPaintedItem naturally clips paint output to its
    // bounds; omitting clip lets child overlays (gradient, "Show more" bar)
    // extend beyond with negative margins.
    // Cap enlarged emoji-only messages against the default timeline avatar size.
    // We intentionally ignore the "small avatars" toggle here: otherwise the cap gets too low and
    // "enlarged" emojis can end up near regular text size, which defeats the feature.
    readonly property real timelineAvatarSize: Komai.iconSize
    readonly property real pixelsPerPoint: Math.max(0.01, Screen.pixelDensity * 25.4 / 72)
    readonly property int enlargedEmojiCapPixelSize: Math.max(1, Math.round(timelineAvatarSize * 0.9))
    readonly property real enlargedEmojiCapPointSize: enlargedEmojiCapPixelSize / pixelsPerPoint
    readonly property real enlargedEmojiPointSize: Math.min(Settings.uiFontSizePt * 3, enlargedEmojiCapPointSize)

    property string copyText: selectedText.length > 0 ? selectedText : body

    html: searchQuery.length > 0 ? Komai.markSearchMatchesInHtml(formatted, searchQuery) : formatted
    perfRoomId: EventDelegateChooser.room ? String(EventDelegateChooser.room.roomId || "") : ""
    perfEventId: String(EventDelegateChooser.eventId || "")
    color: palette.text
    linkColor: palette.link
    surfaceColor: palette.alternateBase
    font.pointSize: enlargedEmojiOnly ? enlargedEmojiPointSize : Settings.uiFontSizePt
    font.family: Komai.fontFamily
    compact: (Komai.density !== Settings.Density.Spacious)

    enabled: !isReply

    onLinkActivated: (link) => {
        if (link && link.startsWith("mxc://")) {
            const roomAvatarPreviewSuffix = "#room-avatar";
            const isRoomAvatarPreview = link.endsWith(roomAvatarPreviewSuffix);
            const cleanLink = isRoomAvatarPreview ? link.slice(0, -roomAvatarPreviewSuffix.length) : link;
            TimelineManager.openMediaOverlay(null, cleanLink, "", isRoomAvatarPreview ? 512 : 0, isRoomAvatarPreview ? 1.0 : 0);
            return;
        }
        Komai.openLink(link);
    }

    // Selection drags grab focus on this paint item; Escape and Tab/Backtab
    // route back through the same path the sidebars use, so the user always
    // has a way out.
    onFocusReleaseRequested: TimelineManager.requestEscape()

    Loader {
        active: !perfDisableTimelineInteraction && hoveredLink.length > 0
        sourceComponent: Component {
            Item {
                TextMetrics {
                    id: linkMetrics
                    text: Komai.punyLink(litehtmlRoot.hoveredLink)
                }

                KomaiToolTip {
                    anchorItem: litehtmlRoot
                    anchorX: litehtmlRoot.hoverPoint.x
                    anchorY: litehtmlRoot.hoverPoint.y
                    gapX: Komai.paddingMedium
                    gapY: Komai.paddingMedium
                    text: linkMetrics.text
                    requestedVisible: litehtmlRoot.hoveredLink.length > 0
                    width: Math.min(linkMetrics.advanceWidth + leftPadding + rightPadding,
                                    (litehtmlRoot.Window.window ? litehtmlRoot.Window.window.width : 500) * 0.5)
                }
            }
        }
    }

    HoverHandler {
        enabled: !perfDisableTimelineInteraction
        cursorShape: hoveredLink.length > 0 ? Qt.PointingHandCursor
                   : (isReply ? Qt.PointingHandCursor : Qt.IBeamCursor)
        onPointChanged: if (hovered) {
            hoverPoint = Qt.point(point.position.x, point.position.y);
            litehtmlRoot.handleHoverMove(point.position.x, point.position.y)
        }
        onHoveredChanged: if (!hovered) litehtmlRoot.handleHoverLeave()
    }

    // Copy button for the hovered <pre> code block, anchored to the block's
    // top-right corner. A real control on top of the paint item, so tooltip,
    // cursor, hover state and Accessible.name come from ImageButton.
    ImageButton {
        id: codeCopyButton

        property bool copied: false

        readonly property rect blockRect: litehtmlRoot.codeBlockRect
        // The C++ side keeps reporting the last block after the pointer moves
        // onto this button (or just past a short block's bottom edge), so our
        // own `hovered` has to participate in visibility to avoid flicker.
        readonly property bool shown: !litehtmlRoot.perfDisableTimelineInteraction
                                      && blockRect.width > 0
                                      && (litehtmlRoot.codeBlockHovered || hovered)
        x: blockRect.x + blockRect.width - width - Komai.paddingSmall
        y: blockRect.y + Komai.paddingSmall
        width: 32
        height: 32
        padding: 6
        hoverEnabled: true
        image: copied ? ":/icons/icons/ui/checkmark.svg" : ":/icons/icons/ui/copy.svg"
        toolTipText: copied ? qsTr("Copied") : qsTr("Copy code")
        opacity: shown ? 1 : 0
        visible: opacity > 0

        Behavior on opacity {
            NumberAnimation { duration: 100 }
        }

        onBlockRectChanged: copied = false
        onClicked: {
            if (litehtmlRoot.copyCodeBlockText()) {
                copied = true;
                copiedRevertTimer.restart();
            }
        }

        background: Rectangle {
            radius: Komai.paddingSmall
            color: codeCopyButton.hovered ? palette.dark : palette.alternateBase
            border.color: Komai.theme.separator
            border.width: 1
        }

        Timer {
            id: copiedRevertTimer
            interval: 2000
            onTriggered: codeCopyButton.copied = false
        }
    }

    // Gradient fade overlay when collapsed
    Rectangle {
        visible: litehtmlRoot.collapsible && litehtmlRoot.collapsed
        anchors.left: litehtmlRoot.left
        anchors.right: litehtmlRoot.right
        anchors.bottom: litehtmlRoot.bottom
        anchors.leftMargin: -litehtmlRoot.collapseOverflowLeft
        anchors.rightMargin: -litehtmlRoot.collapseOverflowRight
        anchors.bottomMargin: -litehtmlRoot.collapseOverflowV
        height: 60
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: palette.base }
        }
    }

    // "Show more" action bar (styled like room header toolbar buttons)
    AbstractButton {
        id: showMoreBar
        visible: litehtmlRoot.collapsible
        readonly property bool active: hovered || pressed
        readonly property color foreground: active ? palette.brightText : palette.buttonText
        readonly property int iconSize: 12

        anchors.left: litehtmlRoot.left
        anchors.right: litehtmlRoot.right
        anchors.bottom: litehtmlRoot.bottom
        anchors.leftMargin: -litehtmlRoot.collapseOverflowLeft
        anchors.rightMargin: -litehtmlRoot.collapseOverflowRight
        anchors.bottomMargin: -litehtmlRoot.collapseOverflowV
        hoverEnabled: true
        leftPadding: Komai.paddingSmall
        rightPadding: Komai.paddingSmall
        topPadding: Komai.paddingMedium
        bottomPadding: Komai.paddingMedium
        z: 1

        background: Rectangle {
            radius: Komai.paddingSmall
            color: parent.active ? palette.dark : palette.alternateBase
            border.color: Komai.theme.separator
            border.width: 1
        }

        contentItem: Item {
            implicitHeight: showMoreRow.implicitHeight

            Row {
                id: showMoreRow

                anchors.centerIn: parent
                spacing: Komai.paddingSmall

                Image {
                    source: "image://colorimage/:/icons/icons/ui/" + (litehtmlRoot.collapsed ? "chevron-circle-down.svg" : "chevron-circle-up.svg") + "?" + showMoreBar.foreground
                    sourceSize.height: showMoreBar.iconSize
                    sourceSize.width: showMoreBar.iconSize
                    anchors.verticalCenter: parent.verticalCenter
                }

                Label {
                    text: litehtmlRoot.collapsed ? qsTr("Show more") : qsTr("Show less")
                    color: showMoreBar.foreground
                    font.bold: true
                    anchors.verticalCenter: parent.verticalCenter
                }
            }
        }

        onClicked: litehtmlRoot.collapsed = !litehtmlRoot.collapsed

        KomaiCursorShape {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor
        }
    }

}
