// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    required property var chatList
    required property var chatRoot
    required property var roomModel

    Window.onActiveChanged: readTimer.running = Window.active

    Shortcut {
        sequences: [StandardKey.MoveToPreviousPage]

        onActivated: {
            chatList.keepPinnedToBottom = false;
            chatList.contentY = chatList.contentY - chatList.height * 0.9;
            chatList.returnToBounds();
        }
    }
    Shortcut {
        sequences: [StandardKey.MoveToNextPage]

        onActivated: {
            chatList.keepPinnedToBottom = false;
            chatList.contentY = chatList.contentY + chatList.height * 0.9;
            chatList.returnToBounds();
        }
    }
    Shortcut {
        sequences: [StandardKey.Cancel]

        onActivated: chatRoot.handleEscape()
    }
    Shortcut {
        sequence: "Alt+Up"

        onActivated: chatRoot.handleAltWalkModeMoveTowardOlderEvents()
    }
    Shortcut {
        sequence: "Alt+Down"

        onActivated: chatRoot.handleAltWalkModeMoveTowardNewerEvents()
    }
    Timer {
        id: readTimer

        interval: 1000

        // force current read index to update
        onTriggered: {
            if (roomModel)
                roomModel.setCurrentIndex(roomModel.currentIndex);
        }
    }
}
