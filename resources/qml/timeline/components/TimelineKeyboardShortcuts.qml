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
            let handledComposerState = false;

            if (chatRoot.keyboardActionsOpen) {
                chatRoot.closeKeyboardActions();
            } else if (roomModel.input.uploads.length > 0) {
                roomModel.input.declineUploads();
                handledComposerState = true;
            } else if (roomModel.reply) {
                roomModel.reply = undefined;
                handledComposerState = true;
            } else if (roomModel.edit) {
                roomModel.edit = undefined;
                handledComposerState = true;
            } else if (roomModel.thread) {
                roomModel.thread = undefined;
                handledComposerState = true;
            } else if (chatRoot.hasSelectedEvent) {
                chatRoot.clearSelectedEvent();
            }

            if (handledComposerState) {
                if (chatRoot.hasSelectedEvent)
                    chatRoot.focusTimelineSelection();
                else
                    TimelineManager.focusMessageInput();
            }
        }
    }

    Shortcut {
        sequence: "Alt+Up"

        onActivated: {
            if (chatRoot.hasSelectedEvent)
                chatRoot.moveSelection(1);
            else
                chatRoot.selectBottomMostVisibleEvent();
        }
    }
    Shortcut {
        sequence: "Alt+Down"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.moveSelection(-1)
    }
    Shortcut {
        sequence: "Alt+R"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("reply")
    }
    Shortcut {
        sequence: "Alt+Shift+T"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("thread")
    }
    Shortcut {
        sequence: "Alt+E"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("edit")
    }
    Shortcut {
        sequence: "Alt+F"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("forward")
    }
    Shortcut {
        sequence: "Alt+D"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("remove")
    }
    Shortcut {
        sequence: "Alt+U"
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.performSelectedMessageAction("raw")
    }
    Shortcut {
        sequences: ["Menu", "Shift+F10"]
        enabled: chatRoot.hasSelectedEvent

        onActivated: chatRoot.openSelectedMessageActionsDialog()
    }
    Shortcut {
        sequences: ["Return", "Enter"]
        enabled: chatRoot.hasSelectedEvent && !chatRoot.keyboardActionsOpen

        onActivated: chatRoot.openKeyboardActionsForSelection()
    }
    Shortcut {
        sequence: "Left"
        enabled: chatRoot.keyboardActionsOpen

        onActivated: chatRoot.moveKeyboardActionsFocus(-1)
    }
    Shortcut {
        sequence: "Right"
        enabled: chatRoot.keyboardActionsOpen

        onActivated: chatRoot.moveKeyboardActionsFocus(1)
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
