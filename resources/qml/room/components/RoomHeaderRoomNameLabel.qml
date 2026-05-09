// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Item {
    id: root

    required property string roomName
    property var room: null
    property bool showVisibilityLabel: false
    readonly property bool isPublic: room ? room.isPublic : true
    readonly property bool isSpace: room ? !!room.isSpace : false
    readonly property bool mirrored: LayoutMirroring.enabled || Qt.application.layoutDirection === Qt.RightToLeft
    readonly property real nameImplicitWidth: nameWidthHelper.implicitWidth
    readonly property real visibilityFullWidth: row.spacing + visibilityIcon.width + visibilityRow.spacing + visibilityLabel.implicitWidth

    Layout.column: 2
    Layout.fillWidth: true
    Layout.minimumWidth: 0
    Layout.preferredWidth: 0
    Layout.row: 1
    implicitHeight: row.implicitHeight
    clip: true

    // Hidden helper to measure true single-line width of the room name.
    // The visible label's implicitWidth is unreliable with RichText + wrapMode:
    // once text wraps, Qt reports the longest wrapped line width instead of
    // the natural unwrapped width, creating a feedback loop that keeps the
    // label narrow even when space is available.
    Text {
        id: nameWidthHelper

        visible: false
        font: nameLabel.font
        text: nameLabel.text
        textFormat: Text.RichText
    }

    Row {
        id: row

        x: root.mirrored ? Math.max(0, root.width - width) : 0
        width: Math.min(implicitWidth, root.width)
        LayoutMirroring.enabled: root.mirrored
        LayoutMirroring.childrenInherit: true
        spacing: Komai.paddingMedium

        Label {
            id: nameLabel

            width: Math.min(nameWidthHelper.implicitWidth, Math.max(0, root.width
                - (spaceBadge.visible ? (row.spacing + spaceBadge.implicitWidth) : 0)
                - (visibilityItem.visible ? (row.spacing + visibilityItem.implicitWidth) : 0)))
            color: palette.text
            elide: Text.ElideRight
            font.bold: true
            font.pointSize: Settings.uiFontSizePt * 1.1
            horizontalAlignment: Text.AlignLeft
            maximumLineCount: 2
            text: root.roomName
            textFormat: Text.RichText
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
            // RichText so the visible string may contain markup; Accessible.name
            // should be the bare room name.
            Accessible.role: Accessible.Heading
            Accessible.name: root.roomName
        }

        Rectangle {
            id: spaceBadge

            readonly property color badgeColor: palette.buttonText

            visible: root.isSpace
            anchors.verticalCenter: nameLabel.verticalCenter
            implicitWidth: spaceBadgeLabel.implicitWidth + Komai.paddingSmall * 2
            implicitHeight: spaceBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
            radius: Komai.paddingSmall
            color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.15)
            border.color: Qt.rgba(badgeColor.r, badgeColor.g, badgeColor.b, 0.4)
            border.width: 1

            Label {
                id: spaceBadgeLabel

                anchors.centerIn: parent
                text: qsTr("Space")
                color: spaceBadge.badgeColor
                font.pointSize: Settings.uiFontSizePt * 0.8
            }
        }

        Item {
            id: visibilityItem

            anchors.verticalCenter: nameLabel.verticalCenter
            implicitHeight: visibilityRow.implicitHeight
            implicitWidth: visibilityRow.implicitWidth
            height: implicitHeight
            width: implicitWidth
            visible: !!root.room

            KomaiToolTip {
                anchorItem: visibilityItem
                anchorX: visibilityItem.width / 2
                anchorY: visibilityItem.height
                gapX: Komai.paddingMedium
                gapY: Komai.paddingMedium
                text: root.isPublic ? qsTr("This room is public. Anyone can join.") : qsTr("This room is private. Invitation required.")
                delay: Komai.tooltipDelay
                requestedVisible: visibilityMouse.containsMouse
            }

            Row {
                id: visibilityRow

                spacing: Komai.paddingSmall

                Image {
                    id: visibilityIcon

                    anchors.verticalCenter: parent.verticalCenter
                    height: visibilityFontMetrics.height
                    width: visibilityFontMetrics.height
                    source: (root.isPublic
                            ? "image://colorimage/:/icons/icons/ui/people-community.svg?"
                            : "image://colorimage/:/icons/icons/ui/lock-closed.svg?")
                        + palette.buttonText
                    sourceSize.height: visibilityFontMetrics.height
                    sourceSize.width: visibilityFontMetrics.height

                    FontMetrics {
                        id: visibilityFontMetrics

                        font.pointSize: Settings.uiFontSizePt
                    }
                }
                Label {
                    id: visibilityLabel

                    anchors.verticalCenter: parent.verticalCenter
                    color: palette.buttonText
                    font.pointSize: Settings.uiFontSizePt
                    horizontalAlignment: Text.AlignLeft
                    text: root.isPublic ? qsTr("Public") : qsTr("Private")
                    visible: root.showVisibilityLabel
                }
            }

            MouseArea {
                id: visibilityMouse

                anchors.fill: parent
                acceptedButtons: Qt.NoButton
                hoverEnabled: true
            }
        }
    }
}
