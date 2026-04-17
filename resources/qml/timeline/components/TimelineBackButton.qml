// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

ImageButton {
    id: root

    property bool showBackButton: false

    toolTipText: qsTr("Back to room list")
    toolTipVisible: hovered
    anchors.left: parent.left
    anchors.margins: Komai.paddingMedium
    anchors.top: parent.top
    enabled: visible
    height: Komai.iconSize
    image: ":/icons/icons/ui/angle-arrow-left.svg"
    visible: !!showBackButton
    width: Komai.iconSize

    onClicked: Rooms.resetCurrentRoom()
}
