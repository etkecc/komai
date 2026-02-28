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
    id: roomLeaveButton

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
    ToolTip.text: qsTr("Leave room")
    ToolTip.visible: hovered
    image: ":/icons/icons/ui/power-off.svg"
    visible: roomAvailable

    onClicked: TimelineManager.openLeaveRoomDialog(roomId)
}
