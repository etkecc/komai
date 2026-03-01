// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

pragma ComponentBehavior: Bound
import QtQuick
import QtQuick.Layouts
import QtQuick.Controls
import cc.etke.komai

Item {
    id: root

    required property string label
    implicitWidth: sectionLayout.implicitWidth
    implicitHeight: sectionLayout.implicitHeight

    ColumnLayout {
        id: sectionLayout
        anchors.left: parent.left
        anchors.right: parent.right
        spacing: Komai.paddingSmall

        Label {
            Layout.fillWidth: true
            color: palette.text
            text: root.label
            textFormat: Text.AutoText
            font.pointSize: 1.1 * Settings.uiFontSizePt
            font.capitalization: Font.AllUppercase
            wrapMode: Text.Wrap
        }

        Item {
            Layout.fillWidth: true
            Layout.preferredHeight: Komai.paddingLarge

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
