// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

AbstractButton {
    id: linkBtn

    hoverEnabled: true
    leftPadding: Komai.paddingSmall
    rightPadding: Komai.paddingSmall
    topPadding: Komai.paddingSmall / 2
    bottomPadding: Komai.paddingSmall / 2
    implicitHeight: linkRow.implicitHeight + topPadding + bottomPadding
    implicitWidth: linkRow.implicitWidth + leftPadding + rightPadding

    contentItem: Row {
        id: linkRow
        spacing: Komai.paddingSmall

        Image {
            anchors.verticalCenter: parent.verticalCenter
            visible: linkBtn.icon.source.toString() !== ""
            width: linkIconSize
            height: linkIconSize
            source: linkBtn.icon.source
            sourceSize.width: linkIconSize
            sourceSize.height: linkIconSize

            readonly property int linkIconSize: Math.max(12, Math.round(Settings.uiFontSizePt * 1.2))
        }

        Label {
            anchors.verticalCenter: parent.verticalCenter
            text: linkBtn.text
            color: linkBtn.hovered ? palette.highlight : palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
            font.underline: linkBtn.hovered
        }
    }

    background: null

    KomaiCursorShape {
        anchors.fill: parent
        cursorShape: Qt.PointingHandCursor
    }
}
