// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

TextArea {
    id: r

    property int cursorShape: Qt.ArrowCursor

    ToolTip.text: Komai.punyLink(hoveredLink)
    ToolTip.visible: hoveredLink || false
    background: null
    font.pointSize: Settings.uiFontSizePt
    bottomInset: 0
    bottomPadding: 0
    // this always has to be enabled, otherwise you can't click links anymore!
    //enabled: selectByMouse
    color: palette.text
    focus: false
    leftInset: 0
    leftPadding: 0
    readOnly: true
    rightInset: 0
    rightPadding: 0
    textFormat: TextEdit.RichText
    topInset: 0
    topPadding: 0
    wrapMode: Text.Wrap

    // Setting a tooltip delay makes the hover text empty .-.
    //ToolTip.delay: Komai.tooltipDelay
    Component.onCompleted: {
        TimelineManager.fixImageRendering(r.textDocument, r);
    }
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

    // propagate events up
    onPressAndHold: event => event.accepted = false
    onPressed: event => event.accepted = (event.button == Qt.LeftButton)

    KomaiCursorShape {
        id: cs

        anchors.fill: parent
        cursorShape: hoveredLink ? Qt.PointingHandCursor : r.cursorShape
    }
}
