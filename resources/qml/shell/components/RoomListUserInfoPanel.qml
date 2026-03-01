// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko

Pane {
    id: userInfoPanel

    required property bool collapsed
    required property var profileContextMenu
    readonly property real baseFontPixelSize: Qt.application.font.pixelSize > 0 ? Qt.application.font.pixelSize : 14
    readonly property real lineSpacing: Math.max(1, Math.round(baseFontPixelSize * 1.2))

    function closeMenu() {
        userInfoMenuController.close();
    }

    function openUserProfile() {
        profileContextMenu.openCurrentUserProfile();
    }

    Layout.maximumHeight: Settings.sidebarsCommunitiesVisible ? 0 : -1
    clip: true
    Layout.alignment: Qt.AlignBottom
    Layout.fillWidth: true
    Layout.minimumHeight: 40
    padding: Nheko.paddingMedium

    background: Rectangle {
        color: palette.window
    }
    contentItem: RowLayout {
        id: userInfoGrid

        property var profile: Nheko.currentUser

        spacing: Nheko.paddingMedium

        Avatar {
            id: headerAvatar

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: userInfoPanel.lineSpacing * 2
            Layout.preferredWidth: userInfoPanel.lineSpacing * 2
            displayName: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
            enabled: false
            url: (userInfoGrid.profile ? userInfoGrid.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
            userid: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
        }
        ColumnLayout {
            id: col

            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            Layout.preferredWidth: parent.width - headerAvatar.width - logoutButton.width - Nheko.paddingMedium * 2
            spacing: 0
            visible: !userInfoPanel.collapsed

            ElidedLabel {
                Layout.alignment: Qt.AlignBottom
                elideWidth: col.width
                font.pointSize: Settings.uiFontSizePt * 1.1
                font.weight: Font.DemiBold
                fullText: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
            }
            ElidedLabel {
                Layout.alignment: Qt.AlignTop
                color: palette.buttonText
                elideWidth: col.width
                font.pointSize: Settings.uiFontSizePt * 0.9
                fullText: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
            }
        }
        Item {
        }
        ImageButton {
            id: logoutButton

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: userInfoPanel.lineSpacing * 2
            Layout.preferredWidth: userInfoPanel.lineSpacing * 2
            ToolTip.delay: Nheko.tooltipDelay
            ToolTip.text: qsTr("Logout")
            ToolTip.visible: hovered
            image: ":/icons/icons/ui/power-off.svg"
            visible: !userInfoPanel.collapsed

            onClicked: Nheko.openLogoutDialog()
        }
    }

    RoomListUserInfoMenu {
        id: userInfoMenuController

        profileContextMenu: userInfoPanel.profileContextMenu
        profile: userInfoGrid.profile
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        margin: -Nheko.paddingSmall

        onLongPressed: userInfoMenuController.popup(userInfoPanel)
        onSingleTapped: userInfoPanel.openUserProfile()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        margin: -Nheko.paddingSmall

        onSingleTapped: userInfoMenuController.popup(userInfoPanel)
    }
}
