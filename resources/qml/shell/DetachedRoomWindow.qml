// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import QtQuick.Controls
import QtQuick.Window
import cc.etke.komai

ApplicationWindow {
    id: roomWindowW

    property var room: null
    property var roomPreview: null
    readonly property string effectiveRoomId: room && room.roomId ? room.roomId : (roomPreview && roomPreview.roomid ? roomPreview.roomid : "")
    readonly property string effectiveRoomName: room && room.plainRoomName
        ? room.plainRoomName
        : (roomPreview && roomPreview.roomName ? roomPreview.roomName : "")

    color: palette.window
    height: 650
    minimumHeight: 150
    minimumWidth: 150
    title: effectiveRoomName
    width: 420

    Component.onCompleted: {
        MainWindow.addPerRoomWindow(effectiveRoomId, roomWindowW);
        Komai.setTransientParent(roomWindowW, null);
    }
    Component.onDestruction: MainWindow.removePerRoomWindow(effectiveRoomId, roomWindowW)
    onActiveChanged: {
        if (room && typeof room.lastReadIdOnWindowFocus === "function")
            room.lastReadIdOnWindowFocus();
    }

    Shortcut {
        sequence: StandardKey.Cancel

        onActivated: roomWindowW.close()
    }
    TimelineView {
        id: timeline

        anchors.fill: parent
        dialogHost: roomWindowW
        windowFocusBlurOverlay: windowFocusBlurOverlay
        room: roomWindowW.room
        roomPreview: roomWindowW.roomPreview && roomWindowW.roomPreview.roomid ? roomWindowW.roomPreview : null
    }
    PrivacyScreen {
        id: windowFocusBlurOverlay

        anchors.fill: parent
        screenTimeout: Settings.privacyWindowFocusBlurDelaySeconds
        timelineRoot: timeline
        visible: Settings.privacyWindowFocusBlurEnabled
        windowTarget: roomWindowW
    }
}
