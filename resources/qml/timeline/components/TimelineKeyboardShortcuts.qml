// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    required property var chatList
    required property var chatRoot
    required property var roomModel
    property bool allowEscape: true

    Window.onActiveChanged: readTimer.running = Window.active

    Shortcut {
        sequences: [StandardKey.MoveToPreviousPage]
        enabled: !!chatList

        onActivated: {
            if (!chatList)
                return;
            chatList.keepPinnedToBottom = false;
            chatList.contentY = chatList.contentY - chatList.height * 0.9;
            chatList.returnToBounds();
        }
    }
    Shortcut {
        sequences: [StandardKey.MoveToNextPage]
        enabled: !!chatList

        onActivated: {
            if (!chatList)
                return;
            chatList.keepPinnedToBottom = false;
            chatList.contentY = chatList.contentY + chatList.height * 0.9;
            chatList.returnToBounds();
        }
    }
    Shortcut {
        sequences: [StandardKey.Cancel, "Escape"]
        enabled: allowEscape && !!chatRoot

        onActivated: {
            if (chatRoot)
                chatRoot.handleEscape();
        }
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
