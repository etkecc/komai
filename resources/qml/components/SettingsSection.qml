// SPDX-FileCopyrightText: Nheko Contributors
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
    implicitWidth: sectionLayout.implicitWidth
    implicitHeight: sectionLayout.implicitHeight

    ColumnLayout {
        id: sectionLayout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Nheko.paddingSmall

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: root.label
            textFormat: Text.AutoText
            font.pointSize: 1.1 * fontMetrics.font.pointSize
            font.capitalization: Font.AllUppercase
            wrapMode: Text.Wrap
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Nheko.paddingLarge

            Rectangle {
                anchors.verticalCenter: parent.verticalCenter
                anchors.left: parent.left
                anchors.right: parent.right
                color: palette.buttonText
                height: 1
            }
        }
    }
}
