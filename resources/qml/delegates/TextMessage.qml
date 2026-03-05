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
    // Cap enlarged emoji-only messages against the default timeline avatar size.
    // We intentionally ignore the "small avatars" toggle here: otherwise the cap gets too low and
    // "enlarged" emojis can end up near regular text size, which defeats the feature.
    readonly property real timelineAvatarSize: Komai.avatarSize
    readonly property real pixelsPerPoint: Math.max(0.01, Screen.pixelDensity * 25.4 / 72)
    readonly property int enlargedEmojiCapPixelSize: Math.max(1, Math.round(timelineAvatarSize * 0.9))
    readonly property real enlargedEmojiCapPointSize: enlargedEmojiCapPixelSize / pixelsPerPoint
    readonly property real enlargedEmojiPointSize: Math.min(Settings.uiFontSizePt * 3, enlargedEmojiCapPointSize)

    property string copyText: selectedText.length > 0 ? selectedText : body

    html: formatted
    color: palette.text
    font.pointSize: enlargedEmojiOnly ? enlargedEmojiPointSize : Settings.uiFontSizePt
    compact: Komai.uiLayoutCompactMode

    enabled: !isReply

    onLinkActivated: (link) => {
        if (link && link.startsWith("mxc://")) {
            const roomAvatarPreviewSuffix = "#room-avatar";
            const isRoomAvatarPreview = link.endsWith(roomAvatarPreviewSuffix);
            const cleanLink = isRoomAvatarPreview ? link.slice(0, -roomAvatarPreviewSuffix.length) : link;
            TimelineManager.openImageOverlay(null, cleanLink, "", isRoomAvatarPreview ? 512 : 0, isRoomAvatarPreview ? 1.0 : 0);
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

}
