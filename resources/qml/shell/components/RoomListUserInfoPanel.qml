// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

Pane {
    id: userInfoPanel

    required property bool collapsed
    readonly property real baseFontPixelSize: Komai.fontPixelSize
    readonly property real lineSpacing: Math.max(1, Math.round(baseFontPixelSize * 1.2))

    function closeMenu() {
        userInfoMenuController.close();
    }

    function openUserProfile() {
        MainWindow.showUserSettingsPage(UserSettingsModel.TabAccount);
    }

    Layout.maximumHeight: 0
    clip: true
    Layout.alignment: Qt.AlignBottom
    Layout.fillWidth: true
    Layout.minimumHeight: Komai.listIconSize + padding * 2
    padding: Komai.paddingMedium

    background: Rectangle {
        color: palette.window
    }
    contentItem: RowLayout {
        id: userInfoGrid

        property var profile: Komai.currentUser

        spacing: Komai.paddingMedium

        Avatar {
            id: headerAvatar

            Layout.alignment: Qt.AlignVCenter
            Layout.preferredHeight: Komai.listIconSize
            Layout.preferredWidth: Komai.listIconSize
            displayName: userInfoGrid.profile ? userInfoGrid.profile.displayName : ""
            enabled: false
            url: (userInfoGrid.profile ? userInfoGrid.profile.avatarUrl : "").replace("mxc://", "image://MxcImage/")
            userid: userInfoGrid.profile ? userInfoGrid.profile.userid : ""
        }
        ColumnLayout {
            id: col

            Layout.alignment: Qt.AlignLeft
            Layout.fillWidth: true
            Layout.preferredWidth: parent.width - headerAvatar.width - logoutButton.width - Komai.paddingMedium * 2
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
            toolTipText: qsTr("Sign out")
            toolTipVisible: hovered
            image: ":/icons/icons/ui/power-off.svg"
            visible: !userInfoPanel.collapsed

            onClicked: Komai.openLogoutDialog()
        }
    }

    RoomListUserInfoMenu {
        id: userInfoMenuController

        profile: userInfoGrid.profile
    }
    TapHandler {
        acceptedButtons: Qt.LeftButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        margin: -Komai.paddingSmall

        onLongPressed: userInfoMenuController.popup(userInfoPanel)
        onSingleTapped: userInfoPanel.openUserProfile()
    }
    TapHandler {
        acceptedButtons: Qt.RightButton
        gesturePolicy: TapHandler.ReleaseWithinBounds
        margin: -Komai.paddingSmall

        onSingleTapped: userInfoMenuController.popup(userInfoPanel)
    }
}
