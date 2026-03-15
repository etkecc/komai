// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../components"
import "../ui"
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

LitehtmlItem {
    id: litehtmlRoot
    required property string body
    required property int isOnlyEmoji
    property bool isReply: EventDelegateChooser.isReply
    required property bool keepFullText
    required property string formatted
    readonly property bool emojiOnlyMessage: isOnlyEmoji > 0 && isOnlyEmoji < 4
    readonly property bool enlargedEmojiOnly: Settings.timelineMessagesEmojiOnlyEnlarge && emojiOnlyMessage

    // Collapsible large messages
    property real timelineViewportHeight: 0
    readonly property int maxCollapsedHeight: timelineViewportHeight > 0 ? Math.max(150, Math.round(timelineViewportHeight * 0.5)) : 300
    readonly property bool collapsible: !isReply && implicitHeight > maxCollapsedHeight
    property bool collapsed: true
    // Overflow to extend "Show more" bar to bubble container edges.
    // Parent chain: TextMessage → Column → contentItem → messageBubble.
    readonly property real collapseOverflowH: {
        let bubble = parent?.parent?.parent;
        return (bubble && typeof bubble.leftPadding === "number") ? bubble.leftPadding : 0;
    }
    readonly property real collapseOverflowV: {
        let bubble = parent?.parent?.parent;
        return (bubble && typeof bubble.bottomPadding === "number") ? bubble.bottomPadding : 0;
    }

    height: collapsible ? (collapsed ? maxCollapsedHeight : implicitHeight + showMoreBar.implicitHeight) : implicitHeight
    // No QML clip — QQuickPaintedItem naturally clips paint output to its
    // bounds; omitting clip lets child overlays (gradient, "Show more" bar)
    // extend beyond with negative margins.
    // Cap enlarged emoji-only messages against the default timeline avatar size.
    // We intentionally ignore the "small avatars" toggle here: otherwise the cap gets too low and
    // "enlarged" emojis can end up near regular text size, which defeats the feature.
    readonly property real timelineAvatarSize: Komai.avatarSize
    readonly property real pixelsPerPoint: Math.max(0.01, Screen.pixelDensity * 25.4 / 72)
    readonly property int enlargedEmojiCapPixelSize: Math.max(1, Math.round(timelineAvatarSize * 0.9))
    readonly property real enlargedEmojiCapPointSize: enlargedEmojiCapPixelSize / pixelsPerPoint
    readonly property real enlargedEmojiPointSize: Math.min(Settings.uiFontSizePt * 3, enlargedEmojiCapPointSize)

    property string copyText: selectedText.length > 0 ? selectedText : body

    // backgroundColor is bound by the message style (bubble/plain) to the resolved
    // bubble background color. This enables the QImage paint cache to use an opaque
    // fill, which allows sub-pixel text antialiasing instead of grayscale fallback.
    property color bubbleBackgroundColor: palette.window

    html: formatted
    color: palette.text
    backgroundColor: bubbleBackgroundColor
    font.pointSize: enlargedEmojiOnly ? enlargedEmojiPointSize : Settings.uiFontSizePt
    font.family: Komai.fontFamily
    compact: Komai.uiLayoutCompactMode

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

    TextMetrics {
        id: linkMetrics
        text: Komai.punyLink(hoveredLink)
    }

    KomaiToolTip {
        text: linkMetrics.text
        visible: hoveredLink.length > 0
        textColor: palette.text
        backgroundColor: palette.alternateBase
        width: Math.min(linkMetrics.advanceWidth + leftPadding + rightPadding,
                        (litehtmlRoot.Window.window ? litehtmlRoot.Window.window.width : 500) * 0.5)
    }

    HoverHandler {
        cursorShape: hoveredLink.length > 0 ? Qt.PointingHandCursor
                   : (isReply ? Qt.PointingHandCursor : Qt.IBeamCursor)
        onPointChanged: if (hovered) litehtmlRoot.handleHoverMove(point.position.x, point.position.y)
        onHoveredChanged: if (!hovered) litehtmlRoot.handleHoverLeave()
    }

    // Gradient fade overlay when collapsed
    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -litehtmlRoot.collapseOverflowH
        anchors.rightMargin: -litehtmlRoot.collapseOverflowH
        anchors.bottomMargin: -litehtmlRoot.collapseOverflowV
        height: 60
        visible: litehtmlRoot.collapsible && litehtmlRoot.collapsed
        gradient: Gradient {
            GradientStop { position: 0.0; color: "transparent" }
            GradientStop { position: 1.0; color: palette.base }
        }
    }

    // "Show more" action bar (styled like room header toolbar buttons)
    AbstractButton {
        id: showMoreBar

        readonly property bool active: hovered || pressed
        readonly property color foreground: active ? palette.brightText : palette.buttonText
        readonly property int iconSize: 12

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        anchors.leftMargin: -litehtmlRoot.collapseOverflowH
        anchors.rightMargin: -litehtmlRoot.collapseOverflowH
        anchors.bottomMargin: -litehtmlRoot.collapseOverflowV
        visible: litehtmlRoot.collapsible
        hoverEnabled: true
        leftPadding: Komai.paddingSmall
        rightPadding: Komai.paddingSmall
        topPadding: Komai.paddingMedium
        bottomPadding: Komai.paddingMedium
        z: 1

        background: Rectangle {
            radius: Komai.paddingSmall
            color: showMoreBar.active ? palette.dark : palette.alternateBase
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
