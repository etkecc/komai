// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components" as Components
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai 1.0

Components.OverlayDialog {
    id: root

    required property string roomId
    readonly property var roomPreview: Rooms.getRoomPreviewById(roomId)
    readonly property string roomName: roomPreview ? roomPreview.roomName : ""
    readonly property string inviterDisplayName: roomPreview ? roomPreview.inviterDisplayName : ""
    readonly property string inviterUserId: roomPreview ? roomPreview.inviterUserId : ""
    readonly property string inviterAvatarUrl: roomPreview ? roomPreview.inviterAvatarUrl : ""
    readonly property string reason: roomPreview ? roomPreview.reason : ""

    title: roomName
        ? qsTr("Join %1?").arg(roomName)
        : qsTr("Accept room invitation?")
    titleIcon: ":/icons/icons/ui/state-member-change.svg"

    // Inviter info
    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium
        visible: root.inviterUserId !== ""

        Avatar {
            id: inviterAvatar

            displayName: root.inviterDisplayName
            enabled: true
            implicitHeight: Komai.listIconSize
            roomid: root.roomId
            url: root.inviterAvatarUrl.replace("mxc://", "image://MxcImage/")
            userid: root.inviterUserId
            implicitWidth: Komai.listIconSize

            onClicked: {
                root.close();
                TimelineManager.openGlobalUserProfile(root.inviterUserId);
            }
        }
        ColumnLayout {
            Layout.fillWidth: true
            spacing: Komai.paddingSmall

            Label {
                Layout.fillWidth: true
                color: palette.text
                font.pointSize: Settings.uiFontSizePt
                text: qsTr("Invited by %1").arg(TimelineManager.escapeEmoji(root.inviterDisplayName || root.inviterUserId))
                wrapMode: Text.WordWrap
                textFormat: Text.RichText
            }
            Label {
                Layout.fillWidth: true
                color: palette.buttonText
                font.pointSize: Settings.uiFontSizePt * 0.9
                text: root.inviterUserId
                elide: Text.ElideRight
                visible: root.inviterDisplayName !== ""
            }
        }
    }

    // Reason
    Label {
        Layout.fillWidth: true
        color: palette.text
        font.pointSize: Settings.uiFontSizePt
        text: root.reason
        visible: root.reason !== ""
        wrapMode: Text.WordWrap
    }

    // Action buttons
    RowLayout {
        Layout.fillWidth: true
        spacing: Komai.paddingMedium

        Components.KomaiButton {
            text: qsTr("Decline")
            onClicked: {
                Rooms.declineInvite(root.roomId);
                root.close();
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Label {
            color: palette.buttonText
            font.pointSize: Settings.uiFontSizePt * 0.9
            text: qsTr("Decline and ignore user")
            visible: root.inviterUserId !== ""

            MouseArea {
                anchors.fill: parent
                cursorShape: Qt.PointingHandCursor
                hoverEnabled: true

                onClicked: {
                    Rooms.declineInvite(root.roomId);
                    var inviter = TimelineManager.getGlobalUserProfile(root.inviterUserId);
                    inviter.ignored = true;
                    root.close();
                }
                onEntered: parent.font.underline = true
                onExited: parent.font.underline = false
            }
        }

        Item {
            Layout.fillWidth: true
        }

        Components.KomaiButton {
            id: acceptButton
            text: qsTr("Accept")
            highlighted: true
            onClicked: {
                Rooms.acceptInvite(root.roomId);
                root.close();
            }
        }
    }

    initialFocusItem: acceptButton
}
