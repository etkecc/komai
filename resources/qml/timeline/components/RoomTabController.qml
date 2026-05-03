// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: controller

    signal aboutToSwitchRoom()
    signal roomSwitched(string newRoomId)
    signal tabClosed(string roomId)

    property ListModel tabs: ListModel {}

    // Suppresses handling of Rooms.currentRoomIdChanged when we initiated the change.
    property bool _internalNavigation: false

    // Tracks the previous room so we know which tab to update on external navigation.
    property string _previousRoomId: ""

    // Bumped on Rooms model data changes; forces attention-state bindings to re-evaluate.
    property int attentionRevision: 0

    // Bumped to trigger a shake animation on the empty tab delegate.
    property int shakeEmptyTabRevision: 0

    // Stack of recently closed room IDs for Ctrl+Shift+T restore.
    // Must use _pushClosedTab/_popClosedTab to trigger QML change notifications.
    property var _closedTabsStack: []

    // History of visited tab roomIds for "go back" on close (most recent last).
    // Each roomId appears at most once.
    property var _tabHistoryStack: []

    function _pushClosedTab(roomId, pinned) {
        var stack = _closedTabsStack.slice();
        stack.push({ "roomId": roomId, "pinned": !!pinned });
        _closedTabsStack = stack;
    }

    function _popClosedTab() {
        var stack = _closedTabsStack.slice();
        var entry = stack.pop();
        _closedTabsStack = stack;
        return entry;
    }

    readonly property int roleAvatarUrl: Rooms.roleId("avatarUrl")
    readonly property int roleRoomName: Rooms.roleId("roomName")
    readonly property int roleHasUnreadMessages: Rooms.roleId("hasUnreadMessages")
    readonly property int roleHasLoudNotification: Rooms.roleId("hasLoudNotification")
    readonly property int roleHasDraft: Rooms.roleId("hasDraft")
    readonly property int roleTags: Rooms.roleId("tags")
    readonly property int roleIsDirect: Rooms.roleId("isDirect")
    readonly property int roleDirectChatOtherUserId: Rooms.roleId("directChatOtherUserId")

    // Persist tab list to Settings after any mutation.
    // Empty tabs (roomId="") are persisted as startup-restorable "new tab" entries.
    function _saveTabs() {
        var roomIds = [];
        for (var i = 0; i < tabs.count; i++)
            roomIds.push(tabs.get(i).roomId);
        Settings.openTabs = roomIds;
    }

    // Persist pinned tab list to Settings.
    function _savePinnedTabs() {
        var pinnedIds = [];
        for (var i = 0; i < tabs.count; i++) {
            if (tabs.get(i).pinned && tabs.get(i).roomId)
                pinnedIds.push(tabs.get(i).roomId);
        }
        Settings.pinnedTabs = pinnedIds;
    }

    // Restore tabs from Settings on startup.
    function restoreTabs() {
        var saved = Settings.openTabs;
        if (!saved || saved.length === 0)
            return;
        var pinnedSet = {};
        var pinnedList = Settings.pinnedTabs;
        if (pinnedList) {
            for (var p = 0; p < pinnedList.length; p++)
                pinnedSet[pinnedList[p]] = true;
        }
        for (var i = 0; i < saved.length; i++) {
            var roomId = saved[i];
            if (findTab(roomId) === -1)
                tabs.append({
                    "roomId": roomId,
                    "roomName": roomId ? _getRoomName(roomId) : "",
                    "pinned": roomId ? !!pinnedSet[roomId] : false
                });
        }
        // Ensure pinned tabs are sorted to the left.
        _sortPinnedToLeft();
    }

    function _getRoomName(roomId) {
        var name = Rooms.unfilteredRoomData(roomId, roleRoomName);
        return name || roomId;
    }

    function openNewTab() {
        // Only one empty tab allowed. If one exists, focus it and shake.
        var existing = findTab("");
        if (existing !== -1) {
            switchToTab(existing);
            shakeEmptyTabRevision++;
            return;
        }
        tabs.append({ "roomId": "", "roomName": "", "pinned": false });
        _saveTabs();
        _setCurrentRoom("");
    }

    function openTab(roomId) {
        var existing = findTab(roomId);
        if (existing !== -1) {
            _setCurrentRoom(roomId);
            return;
        }
        tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId), "pinned": false });
        _saveTabs();
        _setCurrentRoom(roomId);
    }

    // Navigate from the new tab page: if the room already has a tab, focus it
    // and close the empty tab; otherwise navigate the empty tab to the room.
    function navigateFromNewTab(roomId) {
        var existingIndex = findTab(roomId);
        if (existingIndex !== -1) {
            switchToTab(existingIndex);
            closeTab("");
        } else {
            navigateCurrentTab(roomId);
        }
    }

    function navigateCurrentTab(roomId) {
        if (tabs.count === 0) {
            openTab(roomId);
            return;
        }
        // Find the active unpinned tab to navigate.
        var activeIndex = _previousRoomId ? findTab(_previousRoomId) : -1;
        if (activeIndex === -1)
            activeIndex = findTab(Rooms.currentRoomId);
        // Skip pinned tabs — they are locked.
        if (activeIndex !== -1 && tabs.get(activeIndex).pinned)
            activeIndex = -1;
        if (activeIndex !== -1) {
            var previousRoomId = tabs.get(activeIndex).roomId;
            tabs.set(activeIndex, {
                "roomId": roomId,
                "roomName": _getRoomName(roomId),
                "pinned": false
            });
            if (previousRoomId && previousRoomId !== roomId)
                tabClosed(previousRoomId);
        } else {
            // No active unpinned tab — open a new one at the end.
            tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId), "pinned": false });
        }
        _saveTabs();
        _setCurrentRoom(roomId);
    }

    function closeTab(roomId) {
        var index = findTab(roomId);
        if (index === -1)
            return;
        var wasActive = (roomId === Rooms.currentRoomId);
        var wasPinned = tabs.get(index).pinned;
        tabs.remove(index);
        _saveTabs();
        _savePinnedTabs();
        if (roomId) {
            _pushClosedTab(roomId, wasPinned);
            tabClosed(roomId);
        }
        if (tabs.count === 0) {
            _internalNavigation = true;
            _previousRoomId = "";
            aboutToSwitchRoom();
            Rooms.resetCurrentRoom();
            roomSwitched("");
            _internalNavigation = false;
            return;
        }
        // Remove the closed tab from the visit history.
        _tabHistoryStack = _tabHistoryStack.filter(function(id) { return id !== roomId; });
        if (wasActive) {
            // Try to go back to the most recently visited tab that still exists.
            var switched = false;
            while (_tabHistoryStack.length > 0) {
                var hist = _tabHistoryStack.slice();
                var candidate = hist.pop();
                _tabHistoryStack = hist;
                var candidateIndex = findTab(candidate);
                if (candidateIndex !== -1) {
                    switchToTab(candidateIndex);
                    switched = true;
                    break;
                }
            }
            if (!switched) {
                var newIndex = Math.min(index, tabs.count - 1);
                switchToTab(newIndex);
            }
        }
    }

    function closeCurrentTab() {
        if (tabs.count === 0)
            return;
        var rid = Rooms.currentRoomId;
        if (rid) {
            if (findTab(rid) !== -1)
                closeTab(rid);
        } else {
            // Empty room ID means "New Tab" page — close it.
            var emptyIndex = findTab("");
            if (emptyIndex !== -1)
                closeTab("");
        }
    }

    function reopenClosedTab() {
        while (_closedTabsStack.length > 0) {
            var entry = _popClosedTab();
            // Skip if already open.
            if (findTab(entry.roomId) !== -1)
                continue;
            openTab(entry.roomId);
            if (entry.pinned)
                pinTab(entry.roomId);
            return;
        }
    }

    function closeOtherTabs(roomId) {
        var keepIndex = findTab(roomId);
        if (keepIndex === -1)
            return;
        // Remove from the end to avoid index shifting issues.
        for (var i = tabs.count - 1; i >= 0; i--) {
            if (i !== keepIndex && !tabs.get(i).pinned) {
                var closedId = tabs.get(i).roomId;
                tabs.remove(i);
                if (closedId) {
                    _pushClosedTab(closedId);
                    tabClosed(closedId);
                }
            }
        }
        _saveTabs();
        _savePinnedTabs();
        // Ensure the kept room is active.
        if (Rooms.currentRoomId !== roomId)
            _setCurrentRoom(roomId);
    }

    function closeTabsToTheRight(roomId) {
        var fromIndex = findTab(roomId);
        if (fromIndex === -1)
            return;
        for (var i = tabs.count - 1; i > fromIndex; i--) {
            if (!tabs.get(i).pinned) {
                var closedId = tabs.get(i).roomId;
                tabs.remove(i);
                if (closedId) {
                    _pushClosedTab(closedId);
                    tabClosed(closedId);
                }
            }
        }
        _saveTabs();
        _savePinnedTabs();
    }

    function closeUnpinnedTabs() {
        for (var i = tabs.count - 1; i >= 0; i--) {
            if (!tabs.get(i).pinned) {
                var closedId = tabs.get(i).roomId;
                tabs.remove(i);
                if (closedId) {
                    _pushClosedTab(closedId);
                    tabClosed(closedId);
                }
            }
        }
        _saveTabs();
        _savePinnedTabs();
        // Switch to the last pinned tab if current room was closed.
        if (tabs.count > 0 && findTab(Rooms.currentRoomId) === -1)
            _setCurrentRoom(tabs.get(tabs.count - 1).roomId);
        else if (tabs.count === 0)
            Rooms.resetCurrentRoom();
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

    function pinTab(roomId) {
        var index = findTab(roomId);
        if (index === -1 || tabs.get(index).pinned)
            return;
        tabs.setProperty(index, "pinned", true);
        _sortPinnedToLeft();
        _saveTabs();
        _savePinnedTabs();
    }

    function unpinTab(roomId) {
        var index = findTab(roomId);
        if (index === -1 || !tabs.get(index).pinned)
            return;
        tabs.setProperty(index, "pinned", false);
        // Move right after the last remaining pinned tab.
        var lastPinnedIndex = -1;
        for (var i = 0; i < tabs.count; i++) {
            if (tabs.get(i).pinned)
                lastPinnedIndex = i;
        }
        if (index <= lastPinnedIndex)
            tabs.move(index, lastPinnedIndex, 1);
        _saveTabs();
        _savePinnedTabs();
    }

    function isTabPinned(roomId) {
        var index = findTab(roomId);
        return index !== -1 && tabs.get(index).pinned;
    }

    // Returns the index of the first unpinned tab (= insertion point for new tabs).
    function _firstUnpinnedIndex() {
        for (var i = 0; i < tabs.count; i++) {
            if (!tabs.get(i).pinned)
                return i;
        }
        return tabs.count;
    }

    // Move all pinned tabs to the left, preserving relative order within each group.
    function _sortPinnedToLeft() {
        var insertPos = 0;
        for (var i = 0; i < tabs.count; i++) {
            if (tabs.get(i).pinned && i !== insertPos) {
                tabs.move(i, insertPos, 1);
            }
            if (tabs.get(insertPos).pinned)
                insertPos++;
        }
    }

    // --- Drag-and-drop reordering ---

    property bool isDragging: false
    property string _dragRoomId: ""
    property int _dragOriginalIndex: -1
    property bool _dragOriginalPinned: false

    function beginDrag(roomId) {
        var index = findTab(roomId);
        if (index === -1)
            return;
        _dragRoomId = roomId;
        _dragOriginalIndex = index;
        _dragOriginalPinned = tabs.get(index).pinned;
        isDragging = true;
    }

    function updateDragPosition(targetIndex) {
        if (!isDragging)
            return;
        targetIndex = Math.max(0, Math.min(targetIndex, tabs.count - 1));
        var currentIndex = findTab(_dragRoomId);
        if (currentIndex === -1 || currentIndex === targetIndex)
            return;
        tabs.move(currentIndex, targetIndex, 1);
        // Update pinned state based on new position.
        var shouldPin = _shouldBePinnedAtIndex(targetIndex);
        if (tabs.get(targetIndex).pinned !== shouldPin)
            tabs.setProperty(targetIndex, "pinned", shouldPin);
    }

    function commitDrag() {
        if (!isDragging)
            return;
        isDragging = false;
        _dragRoomId = "";
        _dragOriginalIndex = -1;
        _saveTabs();
        _savePinnedTabs();
    }

    function cancelDrag() {
        if (!isDragging)
            return;
        // Restore original position and pinned state.
        var currentIndex = findTab(_dragRoomId);
        if (currentIndex !== -1 && currentIndex !== _dragOriginalIndex)
            tabs.move(currentIndex, _dragOriginalIndex, 1);
        if (_dragOriginalIndex >= 0 && _dragOriginalIndex < tabs.count)
            tabs.setProperty(_dragOriginalIndex, "pinned", _dragOriginalPinned);
        isDragging = false;
        _dragRoomId = "";
        _dragOriginalIndex = -1;
    }

    // Determine if a tab at the given index should be pinned based on its neighbors.
    // Rule: pinned if all tabs before it are pinned (contiguous with pinned group).
    function _shouldBePinnedAtIndex(index) {
        for (var i = 0; i < index; i++) {
            if (!tabs.get(i).pinned)
                return false;
        }
        // At index 0: only pin if there's a pinned tab after it.
        if (index === 0)
            return tabs.count > 1 && tabs.get(1).pinned;
        return true;
    }

    function _setCurrentRoom(roomId) {
        _internalNavigation = true;
        // Push the outgoing room onto the history stack (for "go back" on close).
        var outgoing = Rooms.currentRoomId;
        if (outgoing !== undefined && outgoing !== roomId && findTab(outgoing) !== -1) {
            var hist = _tabHistoryStack.filter(function(id) { return id !== outgoing; });
            hist.push(outgoing);
            _tabHistoryStack = hist;
        }
        _previousRoomId = roomId;
        aboutToSwitchRoom();
        Rooms.setCurrentRoom(roomId);
        roomSwitched(roomId);
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
        // Skip pinned tabs — they are locked.
        if (prevTabIndex !== -1 && tabs.get(prevTabIndex).pinned)
            prevTabIndex = -1;
        if (prevTabIndex !== -1) {
            var previousRoomId = tabs.get(prevTabIndex).roomId;
            tabs.set(prevTabIndex, {
                "roomId": newRoomId,
                "roomName": _getRoomName(newRoomId),
                "pinned": false
            });
            if (previousRoomId && previousRoomId !== newRoomId)
                tabClosed(previousRoomId);
        } else {
            // No navigable tab (all pinned or no previous tab) — open a new one.
            tabs.append({ "roomId": newRoomId, "roomName": _getRoomName(newRoomId), "pinned": false });
        }
        _saveTabs();
        _previousRoomId = newRoomId;
    }

    function handleRoomClick(roomId, isInvite, ctrlHeld) {
        if (isInvite) {
            TimelineManager.openInviteResponseDialog(roomId);
            return;
        }
        var existingIndex = findTab(roomId);
        // Room already has a tab: focus it.
        if (existingIndex !== -1) {
            switchToTab(existingIndex);
            return;
        }
        // Ctrl+Click, no tabs yet, or "open new tab" policy: open new tab.
        if (ctrlHeld || tabs.count === 0
            || Settings.navigationRoomListOpeningPolicy === Settings.RoomListOpeningPolicy.OpenNewTab)
            openTab(roomId);
        else
            navigateCurrentTab(roomId);
    }
}
