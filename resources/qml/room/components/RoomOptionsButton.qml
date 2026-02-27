// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import im.nheko
import "../../ui"

ImageButton {
    id: roomOptionsButton

    property bool roomAvailable: false
    property string roomId: ""
    property int topBarAvatarSize: Nheko.barIconSize
    property int buttonPaddingH: Nheko.paddingMedium
    property int buttonPaddingV: 0

    Layout.alignment: Qt.AlignVCenter
    Layout.column: 9
    Layout.preferredHeight: topBarAvatarSize
    Layout.preferredWidth: topBarAvatarSize
    Layout.row: 1
    leftPadding: buttonPaddingH
    rightPadding: buttonPaddingH
    topPadding: buttonPaddingV
    bottomPadding: buttonPaddingV
    ToolTip.text: qsTr("Room options")
    ToolTip.visible: hovered
    image: ":/icons/icons/ui/options-circle.svg"
    visible: roomAvailable

    onClicked: roomOptionsMenu.popup(roomOptionsButton)

    Menu {
        id: roomOptionsMenu

        Component.onCompleted: {
            if (roomOptionsMenu.popupType != undefined) {
                roomOptionsMenu.popupType = 2; // Popup.Native with fallback on older Qt (<6.8.0)
            }
        }

        MenuItem {
            text: qsTr("Leave room")
            icon.source: "qrc:/icons/icons/ui/power-off.svg"

            onTriggered: TimelineManager.openLeaveRoomDialog(roomId)
        }
    }

    // HACK: https://bugreports.qt.io/browse/QTBUG-83972, qtwayland cannot auto hide menu
    Connections {
        function onHideMenu() {
            roomOptionsMenu.close();
        }

        target: MainWindow
    }
}
