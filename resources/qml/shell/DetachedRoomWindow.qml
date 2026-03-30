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

    property var roomPreview: null
    readonly property string effectiveRoomId: roomPreview && roomPreview.roomid ? roomPreview.roomid : ""
    readonly property string effectiveRoomName: roomPreview && roomPreview.roomName ? roomPreview.roomName : ""

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

    Shortcut {
        sequence: StandardKey.Cancel

        onActivated: roomWindowW.close()
    }
    TimelineView {
        id: timeline

        anchors.fill: parent
        dialogHost: roomWindowW
        windowFocusBlurOverlay: windowFocusBlurOverlay
        room: null
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
