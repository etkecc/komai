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

    color: palette.window
    height: 650
    minimumHeight: 150
    minimumWidth: 150
    title: room.plainRoomName
    width: 420

    Component.onCompleted: {
        MainWindow.addPerRoomWindow(room.roomId || roomPreview.roomid, roomWindowW);
        Komai.setTransientParent(roomWindowW, null);
    }
    Component.onDestruction: MainWindow.removePerRoomWindow(room.roomId || roomPreview.roomid, roomWindowW)
    onActiveChanged: {
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
        roomPreview: roomWindowW.roomPreview.roomid ? roomWindowW.roomPreview : null
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
