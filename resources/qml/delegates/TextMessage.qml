// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../ui"
import QtQuick.Window
import cc.etke.komai

MatrixText {
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
    readonly property int emojiBottomTrim: emojiOnlyMessage ? Math.min(18, Math.round(font.pixelSize * 0.32)) : 0

    property string copyText: selectedText ? getText(selectionStart, selectionEnd) : body
    property int metadataWidth: 100
    property bool fitsMetadata: false //positionAt(width,height-4) == positionAt(width-metadataWidth-10, height-4)

    // table border-collapse doesn't seem to work
    text: `
    <style type="text/css">
    code { background-color: ` + palette.alternateBase + `; white-space: pre-wrap; }
    /* Keep block code readable even when plain style right-aligns own messages. */
    pre {
        background-color: ` + palette.alternateBase + `;
        white-space: pre-wrap;
        text-align: left;
    }
    pre code { text-align: left; }
    table {
        border-width: 1px;
        border-collapse: collapse;
        border-style: solid;
        border-color: ` + palette.text + `;
        background-color: ` + palette.alternateBase + `;
    }
    table th,
    table td {
        padding: ` + Math.ceil(fontMetrics.lineSpacing/2) + `px;
    }
    blockquote { margin-left: 1em; }
    ` + (Settings.uiInputMode ? `span[data-mx-spoiler] {
        color: transparent;
        background-color: ` + palette.text + `;
    }` : "") +  // TODO(Nico): Figure out how to support mobile
    `</style>
    ` + formatted.replace(/<del>/g, "<s>").replace(/<\/del>/g, "</s>").replace(/<strike>/g, "<s>").replace(/<\/strike>/g, "</s>")

    enabled: !isReply
    font.pointSize: enlargedEmojiOnly ? enlargedEmojiPointSize : Settings.uiFontSizePt
    bottomPadding: -emojiBottomTrim

    KomaiCursorShape {
        enabled: isReply
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }

}
