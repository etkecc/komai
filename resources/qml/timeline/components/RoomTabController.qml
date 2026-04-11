// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: controller

    property ListModel tabs: ListModel {}

    // Suppresses handling of Rooms.currentRoomIdChanged when we initiated the change.
    property bool _internalNavigation: false

    // Tracks the previous room so we know which tab to update on external navigation.
    property string _previousRoomId: ""

    // Bumped on Rooms model data changes; forces attention-state bindings to re-evaluate.
    property int attentionRevision: 0

    // Role constants matching RoomlistModel::Roles enum (Qt::UserRole = 256).
    readonly property int roleAvatarUrl: 256
    readonly property int roleRoomName: 257
    readonly property int roleHasUnreadMessages: 262
    readonly property int roleHasLoudNotification: 263
    readonly property int roleHasDraft: 265
    readonly property int roleTags: 271

    // Persist tab list to Settings after any mutation.
    function _saveTabs() {
        var roomIds = [];
        for (var i = 0; i < tabs.count; i++)
            roomIds.push(tabs.get(i).roomId);
        Settings.openTabs = roomIds;
    }

    // Restore tabs from Settings on startup.
    function restoreTabs() {
        var saved = Settings.openTabs;
        if (!saved || saved.length === 0)
            return;
        for (var i = 0; i < saved.length; i++) {
            var roomId = saved[i];
            if (findTab(roomId) === -1)
                tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId) });
        }
    }

    function _getRoomName(roomId) {
        var row = Rooms.roomidToIndex(roomId);
        if (row < 0)
            return roomId;
        var name = Rooms.data(Rooms.index(row, 0), roleRoomName);
        return name || roomId;
    }

    function openTab(roomId) {
        var existing = findTab(roomId);
        if (existing !== -1) {
            _setCurrentRoom(roomId);
            return;
        }
        tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId) });
        _saveTabs();
        _setCurrentRoom(roomId);
    }

    function navigateCurrentTab(roomId) {
        if (tabs.count === 0) {
            openTab(roomId);
            return;
        }
        var activeIndex = _previousRoomId ? findTab(_previousRoomId) : -1;
        if (activeIndex === -1)
            activeIndex = findTab(Rooms.currentRoomId);
        if (activeIndex !== -1) {
            tabs.set(activeIndex, {
                "roomId": roomId,
                "roomName": _getRoomName(roomId)
            });
        } else {
            tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId) });
        }
        _saveTabs();
        _setCurrentRoom(roomId);
    }

    function closeTab(roomId) {
        var index = findTab(roomId);
        if (index === -1)
            return;
        var wasActive = (roomId === Rooms.currentRoomId);
        tabs.remove(index);
        _saveTabs();
        if (tabs.count === 0) {
            _internalNavigation = true;
            _previousRoomId = "";
            Rooms.resetCurrentRoom();
            _internalNavigation = false;
            return;
        }
        if (wasActive) {
            var newIndex = Math.min(index, tabs.count - 1);
            switchToTab(newIndex);
        }
    }

    function closeCurrentTab() {
        if (tabs.count === 0)
            return;
        var rid = Rooms.currentRoomId;
        if (rid && findTab(rid) !== -1)
            closeTab(rid);
    }

    function switchToTab(index) {
        if (index < 0 || index >= tabs.count)
            return;
        _setCurrentRoom(tabs.get(index).roomId);
    }

    function findTab(roomId) {
        for (var i = 0; i < tabs.count; i++) {
            if (tabs.get(i).roomId === roomId)
                return i;
        }
        return -1;
    }

    function nextTab() {
        if (tabs.count <= 1)
            return;
        var current = findTab(Rooms.currentRoomId);
        if (current === -1)
            return;
        switchToTab((current + 1) % tabs.count);
    }

    function previousTab() {
        if (tabs.count <= 1)
            return;
        var current = findTab(Rooms.currentRoomId);
        if (current === -1)
            return;
        switchToTab((current - 1 + tabs.count) % tabs.count);
    }

    function _setCurrentRoom(roomId) {
        _internalNavigation = true;
        _previousRoomId = roomId;
        Rooms.setCurrentRoom(roomId);
        _internalNavigation = false;
    }

    function handleExternalRoomChange(newRoomId) {
        if (_internalNavigation)
            return;
        if (!newRoomId || tabs.count === 0) {
            _previousRoomId = newRoomId || "";
            return;
        }
        var existingTab = findTab(newRoomId);
        if (existingTab !== -1) {
            _previousRoomId = newRoomId;
            return;
        }
        // External navigation to a room not in any tab: navigate the current tab.
        var prevTabIndex = _previousRoomId ? findTab(_previousRoomId) : -1;
        if (prevTabIndex !== -1) {
            tabs.set(prevTabIndex, {
                "roomId": newRoomId,
                "roomName": _getRoomName(newRoomId)
            });
            _saveTabs();
        }
        _previousRoomId = newRoomId;
    }

    function handleRoomClick(roomId, isInvite, ctrlHeld) {
        if (isInvite) {
            TimelineManager.openInviteResponseDialog(roomId);
            return;
        }
        var existingIndex = findTab(roomId);
        // Clicking active room: close its tab (or deselect if no tabs).
        if (roomId === Rooms.currentRoomId) {
            if (existingIndex !== -1)
                closeTab(roomId);
            else
                Rooms.resetCurrentRoom();
            return;
        }
        // Room already has a tab: focus it.
        if (existingIndex !== -1) {
            switchToTab(existingIndex);
            return;
        }
        // Ctrl+Click or no tabs yet: open new tab.
        if (ctrlHeld || tabs.count === 0)
            openTab(roomId);
        else
            navigateCurrentTab(roomId);
    }
}
