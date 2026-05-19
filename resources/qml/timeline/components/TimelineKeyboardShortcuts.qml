// SPDX-FileCopyrightText: Nheko Contributors
// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick

Item {
    required property var chatList
    required property var chatRoot
    property bool allowEscape: true

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
    Shortcut {
        sequences: [StandardKey.Copy]
        context: Qt.ApplicationShortcut
        enabled: !!chatRoot
            && chatRoot.selectionModeCopyShortcutEnabled !== undefined
            && !!chatRoot.selectionModeCopyShortcutEnabled

        onActivated: {
            if (chatRoot && typeof chatRoot.copySelectionModeText === "function")
                chatRoot.copySelectionModeText(false);
        }
        onActivatedAmbiguously: {
            if (chatRoot && typeof chatRoot.copySelectionModeText === "function")
                chatRoot.copySelectionModeText(false);
        }
    }
    Shortcut {
        sequences: ["Ctrl+Shift+C"]
        context: Qt.ApplicationShortcut
        enabled: !!chatRoot
            && chatRoot.selectionModeCopyShortcutEnabled !== undefined
            && !!chatRoot.selectionModeCopyShortcutEnabled

        onActivated: {
            if (chatRoot && typeof chatRoot.copySelectionModeText === "function")
                chatRoot.copySelectionModeText(true);
        }
        onActivatedAmbiguously: {
            if (chatRoot && typeof chatRoot.copySelectionModeText === "function")
                chatRoot.copySelectionModeText(true);
        }
    }
}
