// SPDX-FileCopyrightText: Komai Contributors
//
// SPDX-License-Identifier: GPL-3.0-or-later

import QtQuick
import cc.etke.komai

QtObject {
    id: controller

    signal aboutToSwitchRoom()
    signal roomSwitched(string newRoomId)

    property ListModel tabs: ListModel {}

    // Suppresses handling of Rooms.currentRoomIdChanged when we initiated the change.
    property bool _internalNavigation: false

    // Tracks the previous room so we know which tab to update on external navigation.
    property string _previousRoomId: ""

    // Bumped on Rooms model data changes; forces attention-state bindings to re-evaluate.
    property int attentionRevision: 0

    // Bumped to trigger a shake animation on the empty tab delegate.
    property int shakeEmptyTabRevision: 0

    // Role constants matching RoomlistModel::Roles enum (Qt::UserRole = 256).
    readonly property int roleAvatarUrl: 256
    readonly property int roleRoomName: 257
    readonly property int roleHasUnreadMessages: 262
    readonly property int roleHasLoudNotification: 263
    readonly property int roleHasDraft: 265
    readonly property int roleTags: 271

    // Persist tab list to Settings after any mutation.
    // Empty tabs (roomId="") are ephemeral and not persisted.
    function _saveTabs() {
        var roomIds = [];
        for (var i = 0; i < tabs.count; i++) {
            var rid = tabs.get(i).roomId;
            if (rid)
                roomIds.push(rid);
        }
        Settings.openTabs = roomIds;
    }

    // Persist pinned tab list to Settings.
    function _savePinnedTabs() {
        var pinnedIds = [];
        for (var i = 0; i < tabs.count; i++) {
            if (tabs.get(i).pinned)
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
                tabs.append({ "roomId": roomId, "roomName": _getRoomName(roomId), "pinned": !!pinnedSet[roomId] });
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
        // Don't persist empty tabs.
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
            tabs.set(activeIndex, {
                "roomId": roomId,
                "roomName": _getRoomName(roomId),
                "pinned": false
            });
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
        tabs.remove(index);
        _saveTabs();
        _savePinnedTabs();
        if (tabs.count === 0) {
            _internalNavigation = true;
            _previousRoomId = "";
            aboutToSwitchRoom();
            Rooms.resetCurrentRoom();
            roomSwitched("");
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

    function closeOtherTabs(roomId) {
        var keepIndex = findTab(roomId);
        if (keepIndex === -1)
            return;
        // Remove from the end to avoid index shifting issues.
        for (var i = tabs.count - 1; i >= 0; i--) {
            if (i !== keepIndex && !tabs.get(i).pinned)
                tabs.remove(i);
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
            if (!tabs.get(i).pinned)
                tabs.remove(i);
        }
        _saveTabs();
        _savePinnedTabs();
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
            tabs.set(prevTabIndex, {
                "roomId": newRoomId,
                "roomName": _getRoomName(newRoomId),
                "pinned": false
            });
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
        // Clicking active room: close its tab (or deselect if no tabs).
        // But never close a pinned tab via click.
        if (roomId === Rooms.currentRoomId) {
            if (existingIndex !== -1 && !tabs.get(existingIndex).pinned)
                closeTab(roomId);
            else if (existingIndex === -1)
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
