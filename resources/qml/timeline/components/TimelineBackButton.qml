// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import cc.etke.komai

ImageButton {
    id: root

    required property var roomModel
    required property bool showBackButton

    ToolTip.text: qsTr("Back to room list")
    ToolTip.visible: hovered
    anchors.left: parent.left
    anchors.margins: Komai.paddingMedium
    anchors.top: parent.top
    enabled: visible
    height: Komai.listIconSize
    image: ":/icons/icons/ui/angle-arrow-left.svg"
    visible: (roomModel == null || roomModel.isSpace) && showBackButton
    width: Komai.listIconSize

    onClicked: Rooms.resetCurrentRoom()
}
