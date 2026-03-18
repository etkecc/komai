// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick 2.15
import QtQuick.Controls 2.15
import QtQuick.Layouts 1.2
import cc.etke.komai 1.0

Item {
    id: root

    required property string roomName
    property var room: null
    property bool showVisibilityLabel: false
    readonly property bool isPublic: room ? room.isPublic : true
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

        spacing: Komai.paddingMedium

        Label {
            id: nameLabel

            width: Math.min(nameWidthHelper.implicitWidth, Math.max(0, root.width - (visibilityItem.visible ? (row.spacing + visibilityItem.implicitWidth) : 0)))
            color: palette.text
            elide: Text.ElideRight
            font.bold: true
            font.pointSize: Settings.uiFontSizePt * 1.1
            maximumLineCount: 2
            text: root.roomName
            textFormat: Text.RichText
            wrapMode: Text.WrapAtWordBoundaryOrAnywhere
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
