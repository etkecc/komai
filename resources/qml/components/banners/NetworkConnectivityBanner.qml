// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

ColumnLayout {
    id: root

    readonly property bool shouldShow: !TimelineManager.isConnected
    readonly property color accentColor: Komai.theme.error
    readonly property int iconSize: Komai.iconSize
    readonly property int targetHeight: Komai.navigationRowHeight + 1

    spacing: 0
    visible: shouldShow

    Rectangle {
        id: banner

        Layout.fillWidth: true
        color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, bannerHover.hovered ? 0.16 : 0.10)
        implicitHeight: Math.max(contentRow.implicitHeight + 2 * Komai.paddingSmall, root.targetHeight)
        Layout.minimumHeight: root.targetHeight

        RowLayout {
            id: contentRow

            anchors.fill: parent
            anchors.leftMargin: Komai.paddingMedium
            anchors.rightMargin: Komai.paddingMedium
            anchors.topMargin: Komai.paddingSmall
            anchors.bottomMargin: Komai.paddingSmall
            spacing: Komai.paddingMedium

            Image {
                Layout.alignment: Qt.AlignVCenter
                Layout.preferredHeight: root.iconSize
                Layout.preferredWidth: root.iconSize
                source: "image://colorimage/:/icons/icons/ui/network-disconnected.svg?" + root.accentColor
                sourceSize.height: root.iconSize
                sourceSize.width: root.iconSize
                fillMode: Image.PreserveAspectFit
            }
            Label {
                Layout.fillWidth: true
                color: palette.text
                text: qsTr("Network connectivity trouble. Trying to reconnect…")
                textFormat: Text.PlainText
                wrapMode: Text.Wrap
                verticalAlignment: Text.AlignVCenter
            }
        }
        Rectangle {
            anchors.bottom: parent.bottom
            anchors.left: parent.left
            anchors.right: parent.right
            color: Qt.rgba(root.accentColor.r, root.accentColor.g, root.accentColor.b, 0.70)
            height: 1
        }
        HoverHandler {
            id: bannerHover

            acceptedDevices: PointerDevice.Mouse | PointerDevice.Stylus | PointerDevice.TouchPad
        }
    }
}
