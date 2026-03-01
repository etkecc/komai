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
    implicitHeight: titleLabel.implicitHeight + 2 * Komai.paddingSmall
    radius: frameRadius

    Rectangle {
        anchors.left: parent.left
        anchors.right: parent.right
        anchors.bottom: parent.bottom
        color: parent.color
        height: parent.radius
    }

    Label {
        id: titleLabel

        anchors.left: parent.left
        anchors.right: parent.right
        anchors.verticalCenter: parent.verticalCenter
        anchors.leftMargin: Komai.paddingMedium
        anchors.rightMargin: Komai.paddingMedium
        color: palette.text
        font.bold: true
        text: root.text
    }
}
