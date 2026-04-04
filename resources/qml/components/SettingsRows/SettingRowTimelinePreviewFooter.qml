// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

Rectangle {
    id: root

    required property real frameRadius
    required property string text

    color: palette.alternateBase
    implicitHeight: footerLabel.implicitHeight + 2 * Komai.paddingSmall
    radius: frameRadius

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.top: parent.top
        color: parent.color
        height: parent.radius
    }

    Label {
        id: footerLabel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        color: palette.text
        font.pointSize: Settings.uiFontSizePt
        text: root.text
        textFormat: Text.RichText
        wrapMode: Text.Wrap
    }
}
