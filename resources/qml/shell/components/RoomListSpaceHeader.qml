// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai
import "../../components"

Rectangle {
    id: root

    required property bool collapsed
    required property int avatarSize

    readonly property bool active: Communities.currentFilterId.startsWith("space:")
    readonly property string spaceId: active ? Communities.currentFilterId.substring(6) : ""
    readonly property var spacePreview: active ? Rooms.getRoomPreviewById(spaceId) : null

    visible: active
    height: active ? Komai.navigationRowHeight : 0
    color: palette.alternateBase

    RowLayout {
        id: mainRow

        anchors.fill: parent
        anchors.margins: Komai.paddingMedium
        spacing: Komai.paddingMedium

        AvatarSettingsFlipButton {
            Layout.alignment: Qt.AlignVCenter
            Layout.preferredWidth: root.avatarSize
            Layout.preferredHeight: root.avatarSize
            avatarButtonSize: root.avatarSize
            avatarDisplayName: root.spacePreview ? root.spacePreview.roomName : ""
            avatarRoomId: root.spaceId
            avatarUrl: root.spacePreview ? root.spacePreview.roomAvatarUrl.replace("mxc://", "image://MxcImage/") : ""
            toolTipText: qsTr("Space settings")

            onLeftClicked: TimelineManager.openRoomInfo(root.spaceId, "settings")
            onRightClicked: TimelineManager.openRoomInfo(root.spaceId, "settings")
        }

        Item {
            Layout.fillWidth: true
            implicitHeight: nameRow.implicitHeight
            visible: !root.collapsed

            Row {
                id: nameRow
                width: parent.width
                spacing: Komai.paddingMedium
                clip: true

                Label {
                    width: Math.min(implicitWidth, nameRow.width - (spaceBadgeRect.visible ? spaceBadgeRect.width + nameRow.spacing : 0))
                    anchors.verticalCenter: parent.verticalCenter
                    text: root.spacePreview ? root.spacePreview.roomName : ""
                    font.bold: true
                    font.pixelSize: Komai.fontPixelSize
                    elide: Text.ElideRight
                    color: palette.text
                }

                Rectangle {
                    id: spaceBadgeRect
                    anchors.verticalCenter: parent.verticalCenter
                    implicitWidth: spaceBadgeLabel.implicitWidth + Komai.paddingSmall * 2
                    implicitHeight: spaceBadgeLabel.implicitHeight + Komai.paddingSmall * 0.5
                    radius: Komai.paddingSmall
                    color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.15)
                    border.color: Qt.rgba(palette.text.r, palette.text.g, palette.text.b, 0.4)
                    border.width: 1

                    Label {
                        id: spaceBadgeLabel
                        anchors.centerIn: parent
                        text: qsTr("Space")
                        color: palette.text
                        font.pointSize: Settings.uiFontSizePt * 0.8
                    }
                }
            }
        }

        AbstractButton {
            id: leaveButton

            readonly property int iconSize: Math.max(14, root.avatarSize / 2)
            readonly property bool activeState: hovered || pressed
            readonly property color actionTextColor: activeState ? palette.brightText : palette.buttonText
            readonly property color actionLabelColor: activeState ? palette.brightText : palette.text
            readonly property bool hasRoom: leaveLabel.implicitWidth + iconSize + Komai.paddingSmall + Komai.paddingSmall * 2 < root.width * 0.4

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: root.avatarSize
            implicitWidth: root.avatarSize + (hasRoom ? (Komai.paddingSmall + leaveLabel.implicitWidth) : 0)
            hoverEnabled: true
            leftPadding: Komai.paddingSmall
            rightPadding: Komai.paddingSmall
            visible: !root.collapsed

            KomaiToolTip {
                anchorItem: leaveButton
                anchorX: leaveButton.width / 2
                anchorY: leaveButton.height
                gapX: Komai.paddingMedium
                gapY: Komai.paddingMedium
                text: qsTr("Leave space")
                delay: Komai.tooltipDelay
                requestedVisible: leaveButton.hovered && !leaveButton.hasRoom
            }

            background: Rectangle {
                radius: Komai.paddingSmall
                color: leaveButton.activeState ? palette.dark : "transparent"
            }

            contentItem: RowLayout {
                spacing: Komai.paddingSmall

                Image {
                    Layout.alignment: Qt.AlignVCenter
                    Layout.preferredHeight: leaveButton.iconSize
                    Layout.preferredWidth: leaveButton.iconSize
                    source: "image://colorimage/:/icons/icons/ui/power-off.svg?" + leaveButton.actionTextColor
                    sourceSize.height: leaveButton.iconSize
                    sourceSize.width: leaveButton.iconSize
                }
                Label {
                    id: leaveLabel

                    Layout.alignment: Qt.AlignVCenter
                    color: leaveButton.actionLabelColor
                    font.bold: true
                    text: qsTr("Leave")
                    visible: leaveButton.hasRoom
                }
            }

            KomaiCursorShape {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
            }

            onClicked: TimelineManager.openLeaveRoomDialog(root.spaceId)
        }
    }

    Rectangle {
        anchors.bottom: parent.bottom
        anchors.left: parent.left
        anchors.right: parent.right
        color: Komai.theme.separator
        height: 1
    }
}
