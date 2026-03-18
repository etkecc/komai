// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Rectangle {
    id: root

    readonly property color badgeColor: palette.buttonText
    readonly property real badgeFontSize: Math.floor(Settings.uiFontSizePt * 0.85)
    property point hoverPoint: Qt.point(badgeRow.width / 2, badgeRow.height)

    implicitWidth: badgeRow.implicitWidth + Komai.paddingSmall * 2
    implicitHeight: badgeRow.implicitHeight + Komai.paddingSmall
    radius: Komai.paddingSmall
    color: palette.window
    border.color: palette.mid
    border.width: 1

    KomaiToolTip {
        anchorItem: badgeRow
        anchorX: root.hoverPoint.x
        anchorY: root.hoverPoint.y
        gapX: Komai.paddingSmall
        gapY: Komai.paddingSmall
        text: qsTr("This setting is stored on your Matrix account and applies across all your devices which support it.")
        delay: 500
        requestedVisible: badgeHover.hovered
    }

    RowLayout {
        id: badgeRow
        anchors.centerIn: parent
        spacing: Komai.paddingSmall

        HoverHandler {
            id: badgeHover

            onPointChanged: if (hovered)
                root.hoverPoint = Qt.point(point.position.x, point.position.y)
        }

        Image {
            readonly property int badgeIconSize: Math.max(12, Math.round(Settings.uiFontSizePt * 1.6))
            Layout.preferredHeight: badgeIconSize
            Layout.preferredWidth: badgeIconSize
            sourceSize.height: height
            sourceSize.width: width
            source: "image://colorimage/:/icons/icons/ui/cloud-arrow-up.svg?" + root.badgeColor
        }

        Label {
            text: qsTr("Synced to Matrix")
            color: root.badgeColor
            font.pointSize: root.badgeFontSize
        }
    }
}
