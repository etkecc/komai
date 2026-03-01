// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import "../../components"
import QtQuick
import QtQuick.Controls
import QtQuick.Layouts
import cc.etke.komai

RowLayout {
    id: root

    required property var room
    required property real lineSpacing

    readonly property string communityAvatarUrl: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomAvatarUrl) || ""
    readonly property string communityId: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomid) || ""
    readonly property string communityName: (Settings.sidebarsCommunitiesVisible && room && room.parentSpace && room.parentSpace.roomName) || ""
    readonly property bool communityVisible: !Komai.uiLayoutCompactMode && communityId && room.parentSpace.isLoaded && ("space:" + room.parentSpace.roomid != Communities.currentTagId)

    Layout.column: 1
    Layout.columnSpan: 2
    Layout.fillWidth: true
    Layout.row: 0
    spacing: Komai.paddingSmall

    Avatar {
        Layout.alignment: Qt.AlignHCenter
        displayName: root.communityName
        enabled: false
        implicitHeight: root.lineSpacing
        implicitWidth: root.lineSpacing
        roomid: root.communityId
        url: root.communityAvatarUrl.replace("mxc://", "image://MxcImage/")
        visible: root.communityVisible
    }
    Label {
        Layout.fillWidth: true
        color: palette.text
        elide: Text.ElideRight
        maximumLineCount: 1
        text: qsTr("In %1").arg(root.communityName)
        textFormat: Text.RichText
        visible: root.communityVisible

        MouseArea {
            anchors.fill: parent
            cursorShape: Qt.PointingHandCursor

            onClicked: {
                if (!Communities.trySwitchToSpace(root.room.parentSpace.roomid))
                    root.room.parentSpace.promptJoin();
            }
        }
    }
}
