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

        onActivated: {
            if (roomModel.input.uploads.length > 0)
                roomModel.input.declineUploads();
            else if (roomModel.reply)
                roomModel.reply = undefined;
            else if (roomModel.edit)
                roomModel.edit = undefined;
            else
                roomModel.thread = undefined;
            TimelineManager.focusMessageInput();
        }
    }

    // These shortcuts use the room timeline because switching to threads and out is annoying otherwise.
    // Better solution welcome.
    Shortcut {
        sequence: "Alt+Up"

        onActivated: roomModel.reply = roomModel.indexToId(roomModel.reply ? roomModel.idToIndex(roomModel.reply) + 1 : 0)
    }
    Shortcut {
        sequence: "Alt+Down"

        onActivated: {
            var idx = roomModel.reply ? roomModel.idToIndex(roomModel.reply) - 1 : -1;
            roomModel.reply = idx >= 0 ? roomModel.indexToId(idx) : null;
        }
    }
    Shortcut {
        sequence: "Alt+F"

        onActivated: {
            if (roomModel.reply) {
                chatRoot.openForwardDialog(roomModel.reply);
                roomModel.reply = null;
            }
        }
    }
    Shortcut {
        sequence: "Ctrl+E"

        onActivated: {
            roomModel.edit = roomModel.reply;
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
