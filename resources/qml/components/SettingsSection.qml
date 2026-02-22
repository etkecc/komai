// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import im.nheko

Item {
    id: root

    required property string label

    ColumnLayout {
        anchors.fill: parent
        spacing: 0

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: root.label
            textFormat: Text.AutoText
            font.pointSize: 1.1 * fontMetrics.font.pointSize
            wrapMode: Text.Wrap
        }

        Item {
            Layout.fillWidth: true
            height: fontMetrics.lineSpacing

            Rectangle {
                anchors.topMargin: Nheko.paddingSmall
                anchors.top: parent.top
                anchors.left: parent.left
                anchors.right: parent.right
                color: palette.buttonText
                height: 1
            }
        }
    }
}
